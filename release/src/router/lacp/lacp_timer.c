/*
 * Timer management module for Broadcom LACP driver
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include "lacp_fsm.h"
#include "lacp_linux.h"
#include "lacp_timer.h"
#include "lacp_debug.h"

#define LACP_INVALID_PORT		-1

typedef struct lacp_timer
{
	int8 port;
	lacp_timer_id_t timer_id;
	lacp_timer_handler func;
	void *context;
	uint32 interval;
	uint32 expired_time;	/* time to fire the timer handler */
	bool repeat;
	bool activated;
} lacp_timer_t;

typedef struct {
	osl_t *osh;
	lacp_tsk_ctl_t *tsk;
	lacp_timer_t list[MAX_LAG_PORTS * LACP_TIMER_MAX];
} lacp_timer_info_t;

static bool initialized = FALSE;
static lacp_timer_info_t *lacp_timer_info = NULL;
#define LACP_TIMER_INFO()	(lacp_timer_info)

#define LACP_TIMER_LOCK(info) \
	do {if (info) lacp_tsk_lock((info)->tsk);} while (0)
#define LACP_TIMER_UNLOCK(info) \
	do {if (info) lacp_tsk_unlock((info)->tsk);} while (0)

static void
lacp_timer_clear(lacp_timer_t *t)
{
	if (t) {
		t->activated = FALSE;
		t->port = LACP_INVALID_PORT;
		t->func = NULL;
	}
}

int32 lacp_get_timer_info(char *buf)
{
	int i, j;
	int32 len = 0;
	lacp_timer_info_t *info = LACP_TIMER_INFO();
	lacp_timer_t *t;

	if (!info) {
		len += sprintf(buf + len, "\nERROR!! lacp_timer_info_t NULL");
		return len;
	}
	len += sprintf(buf + len, "\n========================================================"
		"============");
	for (i = 0; i < MAX_LAG_PORTS; i++) {
		len += sprintf(buf + len, "\nPort %d: ", i);
		len += sprintf(buf + len, "\n     tid    registered    start    interval    "
			"CutTime    ExpireTime");
		for (j = 0; j < LACP_TIMER_MAX; j++) {
			t = &(info->list[(i * (int)LACP_TIMER_MAX) + j]);
			if (t->func) {
				if (t->activated) {
					len += sprintf(buf + len, "\n      %d         Yes        "
						"Yes       %d      %d     %d",
						j, t->interval, OSL_SYSUPTIME(), t->expired_time);
				} else {
					len += sprintf(buf + len, "\n      %d         Yes        "
						"No         --          --          --", j);
				}
			} else {
				len += sprintf(buf + len, "\n      %d         No         "
					"--         --          --          --", j);
			}
		}
	}
	return len;
}

static lacp_timer_t *
lacp_timer_get(lacp_timer_info_t *info, int8 port, lacp_timer_id_t timer_id)
{
	lacp_timer_t *t;

	t = &(info->list[(port * (int)LACP_TIMER_MAX) + (int)timer_id]);
	return t;
}

static int
lacp_timer_looper(void *data)
{
	lacp_timer_info_t *info = (lacp_timer_info_t *) data;
	lacp_timer_t *t;
	int8 port_idx, timer_idx;

	ASSERT(info);

	for (timer_idx = 0; timer_idx < (int)LACP_TIMER_MAX; timer_idx++) {
		for (port_idx = 0; port_idx < MAX_LAG_PORTS; port_idx++) {
			t = &(info->list[(port_idx * (int)LACP_TIMER_MAX) + (int)timer_idx]);
			if (t->activated == TRUE && t->expired_time <= OSL_SYSUPTIME()) {
				if (lacp_msg_level & LACP_TIMER_VAL) {
					if (t->timer_id == 0)
						printf("\n");
				}
				LACP_TIMER(("call timer handler, port=%d, timer_id=%d, "
					"%s timer, time %d\n",
					t->port, (int)t->timer_id,
					t->repeat? "repeat": "one shot", OSL_SYSUPTIME()));

				LACP_TIMER_LOCK(info);
				if (t->func)
					t->func(t->context, t->port);

				if (t->repeat) {
					/* update next expired time for repeat timer */
					t->expired_time = OSL_SYSUPTIME() + t->interval;
				} else {
					/* deactivate one shot timer */
					t->activated = FALSE;
				}
				LACP_TIMER_UNLOCK(info);
			}
		}
	}
	return BCME_OK;
}

int
lacp_timer_init(osl_t *osh)
{
	lacp_timer_info_t *info;
	int i;

	if (initialized) {
		LACP_TIMER(("initialized\n"));
		return BCME_ERROR;
	}

	info = (lacp_timer_info_t *)MALLOCZ(osh, sizeof(lacp_timer_info_t));
	if (info == NULL) {
		LACP_ERROR("out of memory, malloced %d bytes\n", MALLOCED(osh));
		return BCME_NORESOURCE;
	}

	/* set global variable */
	LACP_TIMER_INFO() = info;

	/* initial timer list */
	for (i = 0; i < MAX_LAG_PORTS * LACP_TIMER_MAX; i++)
		lacp_timer_clear(&info->list[i]);

	info->osh = osh;

	/* create task for timer */
	info->tsk = LACP_PROC_START(osh, lacp_timer_looper, info, "lacp_timer");
	if (info->tsk == NULL) {
		LACP_ERROR("failed to create timer task\n");
		MFREE(info->osh, info, sizeof(lacp_timer_info_t));
		return BCME_NORESOURCE;
	}

	initialized = TRUE;
	return BCME_OK;
}

void
lacp_timer_deinit(void)
{
	lacp_timer_info_t *info = LACP_TIMER_INFO();

	if (!initialized) {
		LACP_TIMER(("NOT initialized\n"));
		return;
	}

	ASSERT(info);

	LACP_PROC_STOP(info->tsk);
	MFREE(info->osh, info, sizeof(lacp_timer_info_t));
	initialized = FALSE;
}

int32
lacp_timer_register(int8 port, lacp_timer_id_t timer_id,
	lacp_timer_handler handler_function, void *context)
{
	lacp_timer_info_t *info = LACP_TIMER_INFO();
	lacp_timer_t *t;

	ASSERT(port >= 0 && port < MAX_LAG_PORTS);
	ASSERT((int)timer_id < (int)LACP_TIMER_MAX);
	ASSERT(handler_function);
	ASSERT(info);

	t = lacp_timer_get(info, port, timer_id);
	if (t == NULL) {
		LACP_ERROR("no availabe timer, port=%d, time_id=%d\n", port, (int)timer_id);
		return BCME_NORESOURCE;
	}

	LACP_TIMER_LOCK(info);
	t->activated = FALSE;
	t->port = port;
	t->timer_id = timer_id;
	t->context = context;
	t->func = handler_function;
	LACP_TIMER_UNLOCK(info);

	LACP_TIMER(("timer registered, port=%d, time_id=%d\n",
		(int)port, (int)timer_id));
	return BCME_OK;
}

void
lacp_timer_unregister(int8 port, lacp_timer_id_t timer_id)
{
	lacp_timer_info_t *info = LACP_TIMER_INFO();
	lacp_timer_t *t;

	ASSERT(info);

	t = lacp_timer_get(info, port, timer_id);
	if (t) {
		LACP_TIMER_LOCK(info);
		lacp_timer_clear(t);
		LACP_TIMER_UNLOCK(info);

		LACP_TIMER(("timer unregistered, port=%d, time_id=%d\n",
			port, (int)timer_id));
	} else {
		LACP_TIMER(("cannot find timer, port=%d, time_id=%d\n",
			port, (int)timer_id));
	}
}

void
lacp_timer_start(int8 port, lacp_timer_id_t timer_id, uint32 elapse, bool repeat)
{
	lacp_timer_info_t *info = LACP_TIMER_INFO();
	lacp_timer_t *t;

	ASSERT(info);

	t = lacp_timer_get(info, port, timer_id);
	if (t) {
		/* LACP_TIMER_LOCK(info); */
		t->interval = elapse * 1000;	/* means ms */
		t->expired_time = OSL_SYSUPTIME() + t->interval;
		t->repeat = repeat;
		t->activated = TRUE;
		/* LACP_TIMER_UNLOCK(info); */

		LACP_TIMER(("timer started, port=%d, timer_id=%d, timeout  %d, %s timer,"
			" t->expired_time %d\n", port, (int)timer_id, elapse,
			repeat? "repeat": "one shot", t->expired_time));
	} else {
		LACP_TIMER(("cannot find timer, port=%d, time_id=%d\n",
			port, (int)timer_id));
	}
}

void
lacp_timer_stop(int8 port, lacp_timer_id_t timer_id)
{
	lacp_timer_info_t *info = LACP_TIMER_INFO();
	lacp_timer_t *t;

	if (!info)
		return;

	t = lacp_timer_get(info, port, timer_id);
	if (t) {
		t->activated = FALSE;
		LACP_TIMER(("timer stopped, port=%d, time_id=%d\n",
			port, (int)timer_id));
	} else {
		LACP_TIMER(("cannot find timer, port=%d, time_id=%d\n",
			port, (int)timer_id));
	}
}
