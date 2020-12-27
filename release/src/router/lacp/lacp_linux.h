/*
 * Broadcom LACP driver for linux
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

#ifndef	_lacp_linux_h_
#define	_lacp_linux_h_
#include "lacpc_export.h"

typedef int (*looper_t)(void *);

/* LACP task */
typedef struct {
	osl_t *osh;
	bool terminated;
	struct completion completed;
	struct mutex lock;
	struct task_struct *lacp_kthread_tsk;
	void *data;
	looper_t looper;
} lacp_tsk_ctl_t;

/* Start/stop LACP task */
#define LACP_PROC_START(osh, looper_func, param, name) \
	lacp_tsk_start(osh, looper_func, param, name)
#define LACP_PROC_STOP(tsk_ctl) \
	lacp_tsk_stop(tsk_ctl)

lacp_tsk_ctl_t*
lacp_tsk_start(osl_t *osh, looper_t looper_func, void *param, char *name);
void lacp_tsk_stop(lacp_tsk_ctl_t *tsk_ctl);
void lacp_tsk_lock(lacp_tsk_ctl_t *tsk);
void lacp_tsk_unlock(lacp_tsk_ctl_t *tsk);
#endif	/* _lacp_linux_h_ */
