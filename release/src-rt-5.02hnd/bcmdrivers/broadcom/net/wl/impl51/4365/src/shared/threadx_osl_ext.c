/*
 * Threadx OS Support Extension Layer
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 * $Id:$
 */

/* ---- Include Files ---------------------------------------------------- */

#include "typedefs.h"
#include "bcmdefs.h"
#include "bcmendian.h"
#include "bcmutils.h"
#include "osl_ext.h"
#include "tx_api.h"
#include <threadx_low_power.h>
#include <stdlib.h>
#include <rte_trap.h>

/* ---- Public Variables ------------------------------------------------- */
/* ---- Private Constants and Types -------------------------------------- */

/* Threadx specific task priorities. */
#ifndef THREADX_TASK_IDLE_PRIORITY
#define THREADX_TASK_IDLE_PRIORITY		(TX_MAX_PRIORITIES - 1)
#endif

#ifndef THREADX_TASK_LOW_PRIORITY
#define THREADX_TASK_LOW_PRIORITY		25
#endif

#ifndef THREADX_TASK_LOW_NORMAL_PRIORITY
#define THREADX_TASK_LOW_NORMAL_PRIORITY	20
#endif

#ifndef THREADX_TASK_NORMAL_PRIORITY
#define THREADX_TASK_NORMAL_PRIORITY		16
#endif

#ifndef THREADX_TASK_HIGH_NORMAL_PRIORITY
#define THREADX_TASK_HIGH_NORMAL_PRIORITY	10
#endif

#ifndef THREADX_TASK_HIGHEST_PRIORITY
#define THREADX_TASK_HIGHEST_PRIORITY		5
#endif

#ifndef THREADX_TASK_TIME_CRITICAL_PRIORITY
#define THREADX_TASK_TIME_CRITICAL_PRIORITY	0
#endif


#define OSL_LOG(a)	printf a


typedef void (*threadx_func_t)(ULONG);

/* ---- Private Variables ------------------------------------------------ */
/* ---- Private Function Prototypes -------------------------------------- */

static osl_ext_status_t osl_ext_error_check_threadx(UINT threadx_status);


/* ---- Functions -------------------------------------------------------- */

/****************************************************************************
* Function:   osl_ext_error_check_threadx
*
* Purpose:    Maps from Threadx specific error code to OSL generic error code.
*
* Parameters: threadx_status (in) Threadx specific error code.
*
* Returns:    OSL_EXT_SUCCESS on success, else error code.
*****************************************************************************
*/
static osl_ext_status_t
osl_ext_error_check_threadx(UINT threadx_status)
{
	osl_ext_status_t osl_ext_status;

	if (threadx_status == TX_SUCCESS)
		osl_ext_status = OSL_EXT_SUCCESS;
	else if (threadx_status == TX_WAIT_ABORTED)
		osl_ext_status = OSL_EXT_TIMEOUT;
	else
		osl_ext_status = OSL_EXT_ERROR;

	return (osl_ext_status);
}


/* --------------------------------------------------------------------------
** Semaphores
*/

/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_sem_create(char *name, int init_cnt, osl_ext_sem_t *sem)
{
	UINT status;

	memset(sem, 0, sizeof(osl_ext_sem_t));
	status = tx_semaphore_create(sem, name, init_cnt);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_sem_delete(osl_ext_sem_t *sem)
{
	UINT status;

	status = tx_semaphore_delete(sem);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_sem_give(osl_ext_sem_t *sem)
{
	UINT status;

	status = tx_semaphore_put(sem);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_sem_take(osl_ext_sem_t *sem, osl_ext_time_ms_t timeout_msec)
{
	UINT status;
	ULONG wait_option;

	if (timeout_msec == 0)
		wait_option = TX_NO_WAIT;
	else if (timeout_msec == OSL_EXT_TIME_FOREVER)
		wait_option = TX_WAIT_FOREVER;
	else {
		/* not supported */
		return (OSL_EXT_ERROR);
	}


	status = tx_semaphore_get(sem, wait_option);

	return (osl_ext_error_check_threadx(status));
}


/* --------------------------------------------------------------------------
** Mutex
*/

/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_mutex_create(char *name, osl_ext_mutex_t *mutex)
{
	UINT status;

	memset(mutex, 0, sizeof(osl_ext_mutex_t));
	status = tx_mutex_create(mutex, name, TX_INHERIT);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_mutex_delete(osl_ext_mutex_t *mutex)
{
	UINT status;

	status = tx_mutex_delete(mutex);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_mutex_acquire(osl_ext_mutex_t *mutex, osl_ext_time_ms_t timeout_msec)
{
	UINT status;
	ULONG wait_option;

	if (timeout_msec == 0)
		wait_option = TX_NO_WAIT;
	else if (timeout_msec == OSL_EXT_TIME_FOREVER)
		wait_option = TX_WAIT_FOREVER;
	else {
		/* not supported */
		return (OSL_EXT_ERROR);
	}

	status = tx_mutex_get(mutex, wait_option);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_mutex_release(osl_ext_mutex_t *mutex)
{
	UINT status;

	status = tx_mutex_put(mutex);

	return (osl_ext_error_check_threadx(status));
}


/* --------------------------------------------------------------------------
** Timers
*/

/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_timer_create(char *name, osl_ext_time_ms_t timeout_msec, osl_ext_timer_mode_t mode,
                 osl_ext_timer_callback func, osl_ext_timer_arg_t arg, osl_ext_timer_t *timer)
{
	UINT status;

	status = tx_timer_create(timer, name,
		(void (*)(ULONG))func, (ULONG)arg,
		0xffffffff, 0, TX_NO_ACTIVATE);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_timer_delete(osl_ext_timer_t *timer)
{
	UINT	status;

	status = tx_timer_deactivate(timer);

	if (status == TX_SUCCESS)
		status = tx_timer_delete(timer);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_timer_start(osl_ext_timer_t *timer,
	osl_ext_time_ms_t timeout_msec, osl_ext_timer_mode_t mode)
{
	UINT status;
	osl_ext_timer_mode_t updated_msec;
	ULONG init_ticks;
	ULONG reschedule_ticks = 0;

	/* compensate for time last updated */
	updated_msec = timeout_msec + threadx_low_power_time_since_update();

	init_ticks = OSL_MSEC_TO_TICKS(updated_msec);
	if (mode == OSL_EXT_TIMER_MODE_REPEAT)
		reschedule_ticks = OSL_MSEC_TO_TICKS(timeout_msec);

	status = tx_timer_change(timer, init_ticks, reschedule_ticks);

	if (status == TX_SUCCESS) {
		status = tx_timer_activate(timer);

		/* force h/w timer to be updated */
		threadx_low_power_timer_update();
	}

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_timer_stop(osl_ext_timer_t *timer)
{
	UINT	status;

	status = tx_timer_deactivate(timer);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_time_ms_t
osl_ext_time_get(void)
{
	return OSL_TICKS_TO_MSEC(tx_time_get());
}


/* --------------------------------------------------------------------------
** Tasks
*/

/* Map generic task priorities to threadx specific priorities. */
static const UINT g_task_priority_table[OSL_EXT_TASK_NUM_PRIORITES] =
{
	THREADX_TASK_IDLE_PRIORITY,
	THREADX_TASK_LOW_PRIORITY,
	THREADX_TASK_LOW_NORMAL_PRIORITY,
	THREADX_TASK_NORMAL_PRIORITY,
	THREADX_TASK_HIGH_NORMAL_PRIORITY,
	THREADX_TASK_HIGHEST_PRIORITY,
	THREADX_TASK_TIME_CRITICAL_PRIORITY
};


/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_task_create_ex(char* name,
	void *stack, unsigned int stack_size,
	osl_ext_task_priority_t priority,
	osl_ext_time_ms_t timeslice_msec,
	osl_ext_task_entry func, osl_ext_task_arg_t arg,
	osl_ext_task_t *task)
{
	UINT status;
	UINT threadx_priority;
	ULONG timeslice_ticks;

	/* stack must be provided, dynamically allocated is not supported */
	if (stack == NULL || task == NULL)
		return (OSL_EXT_ERROR);

	if (timeslice_msec == 0)
		timeslice_ticks = TX_NO_TIME_SLICE;
	else {
		/* not supported */
		return (OSL_EXT_ERROR);
	}

	memset(task, 0, sizeof(osl_ext_task_t));

	/* Map from the generic OSL priority to a threadx specific priority. */
	threadx_priority = g_task_priority_table[priority];

	/* Create the task... */
	status = tx_thread_create(task, name,
		(threadx_func_t)func, (ULONG)arg, stack, stack_size,
		threadx_priority, threadx_priority,
		timeslice_ticks, TX_AUTO_START);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_task_delete(osl_ext_task_t *task)
{
	/* deleting a task is complicated as a task cannot delete itself */
	/* not supported */
	return (OSL_EXT_ERROR);
}


/* ----------------------------------------------------------------------- */
osl_ext_task_t *osl_ext_task_current(void)
{
	return tx_thread_identify();
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_task_yield(void)
{
	tx_thread_relinquish();

	return (OSL_EXT_SUCCESS);
}


/* ----------------------------------------------------------------------- */
#ifdef TX_ENABLE_STACK_CHECKING
static void stack_error_handler(TX_THREAD *thread_ptr)
{
	printf("!!! stack error !!!\n");
	printf("thread name: %s\n", thread_ptr->tx_thread_name);
	printf("stack pointer: %p\n", thread_ptr->tx_thread_stack_ptr);
	printf("stack start: %p\n", thread_ptr->tx_thread_stack_start);
	printf("stack end: %p\n", thread_ptr->tx_thread_stack_end);
	printf("stack size: %ld\n", thread_ptr->tx_thread_stack_size);

	hnd_die();
}
#endif	/* TX_ENABLE_STACK_CHECKING */

/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_task_enable_stack_check(void)
{
#ifdef TX_ENABLE_STACK_CHECKING
	UINT status;

	status = tx_thread_stack_error_notify(stack_error_handler);

	return (osl_ext_error_check_threadx(status));
#else
	return (OSL_EXT_ERROR);
#endif	/* TX_ENABLE_STACK_CHECKING */
}


/* --------------------------------------------------------------------------
** Queue
*/

/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_queue_create(char *name,
	void *queue_buffer, unsigned int size, osl_ext_queue_t *queue)
{
	UINT status;
	UINT msg_size = 1; /* Number of 32-bit words in message. */

	/* The OSL extension API uses 'void *' messages for queues; whereas,
	 * threadx uses 32-bit words. For simplification, the
	 * implementation relies upon the fact that these types are
	 * the same size.
	 */
	ASSERT(sizeof(void *) == 4);

	/* queue buffer must be provided, dynamically allocated is not supported */
	if (queue_buffer == NULL || queue == NULL)
		return (OSL_EXT_ERROR);

	memset(queue, 0, sizeof(osl_ext_queue_t));

	/* Create queue. */
	status = tx_queue_create(queue, name,
		msg_size, queue_buffer, sizeof(void *) * size);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_queue_delete(osl_ext_queue_t *queue)
{
	UINT status;

	status = tx_queue_delete(queue);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_queue_send(osl_ext_queue_t *queue, void *data)
{
	UINT status;

	status = tx_queue_send(queue, &data, TX_NO_WAIT);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_queue_send_synchronous(osl_ext_queue_t *queue, void *data)
{
	/* not supported */
	return OSL_EXT_ERROR;
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t
osl_ext_queue_receive(osl_ext_queue_t *queue,
	osl_ext_time_ms_t timeout_msec, void **data)
{
	UINT status;
	ULONG wait_option;

	if (timeout_msec == 0)
		wait_option = TX_NO_WAIT;
	else if (timeout_msec == OSL_EXT_TIME_FOREVER)
		wait_option = TX_WAIT_FOREVER;
	else {
		/* not supported */
		return (OSL_EXT_ERROR);
	}

	status = tx_queue_receive(queue, data, wait_option);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_queue_count(osl_ext_queue_t *queue, int *count)
{
	UINT status;
	ULONG enqueued;

	status = tx_queue_info_get(queue, TX_NULL, &enqueued,
		TX_NULL, TX_NULL, TX_NULL, TX_NULL);

	if (status == TX_SUCCESS)
		*count = enqueued;

	return (osl_ext_error_check_threadx(status));
}


/* --------------------------------------------------------------------------
** Event
*/

/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_event_create(char *name, osl_ext_event_t *event)
{
	UINT status;

	memset(event, 0, sizeof(osl_ext_event_t));
	status = tx_event_flags_create(event, name);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_event_delete(osl_ext_event_t *event)
{
	UINT status;

	status = tx_event_flags_delete(event);

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_event_get(osl_ext_event_t *event,
	osl_ext_event_bits_t requested,	osl_ext_time_ms_t timeout_msec,
	osl_ext_event_bits_t *event_bits)
{
	UINT status;
	ULONG wait_option;
	ULONG threadx_event_bits;

	if (timeout_msec == 0)
		wait_option = TX_NO_WAIT;
	else if (timeout_msec == OSL_EXT_TIME_FOREVER)
		wait_option = TX_WAIT_FOREVER;
	else {
		/* not supported */
		return (OSL_EXT_ERROR);
	}

	status = tx_event_flags_get(event, requested, TX_OR_CLEAR,
		&threadx_event_bits, wait_option);

	if (status == TX_SUCCESS)
		*event_bits = threadx_event_bits;

	return (osl_ext_error_check_threadx(status));
}


/* ----------------------------------------------------------------------- */
osl_ext_status_t osl_ext_event_set(osl_ext_event_t *event,
	osl_ext_event_bits_t event_bits)
{
	UINT status;

	status = tx_event_flags_set(event, event_bits, TX_OR);

	return (osl_ext_error_check_threadx(status));
}


/* --------------------------------------------------------------------------
** Interrupt
*/

/* ----------------------------------------------------------------------- */
osl_ext_interrupt_state_t osl_ext_interrupt_disable(void)
{
	return tx_interrupt_control(TX_INT_DISABLE);
}


/* ----------------------------------------------------------------------- */
void osl_ext_interrupt_restore(osl_ext_interrupt_state_t state)
{
	tx_interrupt_control(state);
}
