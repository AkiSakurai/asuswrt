/*
 * Event loop
 * Copyright 2002-2003, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright 2004, Instant802 Networks, Inc.
 * All Rights Reserved.
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: eloop.c 664710 2016-10-13 14:21:15Z $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "eloop.h"

struct eloop_sock {
	int sock;
	void *eloop_data;
	void *user_data;
	void (*handler)(int sock, void *eloop_ctx, void *sock_ctx);
};

struct eloop_timeout {
	clock_t time;
	void *eloop_data;
	void *user_data;
	void (*handler)(void *eloop_ctx, void *sock_ctx);
	struct eloop_timeout *next;
};

struct eloop_signal {
	int sig;
	void *user_data;
	void (*handler)(int sig, void *eloop_ctx, void *signal_ctx);
	int signaled;
};

struct eloop_data {
	void *user_data;

	int max_sock, reader_count;
	struct eloop_sock *readers;

	struct eloop_timeout *timeout;
	long ticks_per_second;

	int signal_count;
	struct eloop_signal *signals;
	int signaled;

	int terminate;
	int sock_unregistered;
};

static struct eloop_data eloop;

#define ELOOP_BEFORE(x, y)\
	((y) - (x) > 0)

/* Initialize global event loop data - must be called before any other eloop_*
 * function. user_data is a pointer to global data structure and will be passed
 * as eloop_ctx to signal handlers.
 */
void eloop_init(void *user_data)
{
	memset(&eloop, 0, sizeof(eloop));
	eloop.user_data = user_data;
	eloop.ticks_per_second = sysconf(_SC_CLK_TCK);
}

/* Register handler for read event */
int eloop_register_read_sock(int sock,
	void (*handler)(int sock, void *eloop_ctx, void *sock_ctx),
	void *eloop_data, void *user_data)
{
	struct eloop_sock *tmp;

	tmp = (struct eloop_sock *)
		realloc(eloop.readers,
			(eloop.reader_count + 1) * sizeof(struct eloop_sock));
	if (tmp == NULL)
		return -1;

	tmp[eloop.reader_count].sock = sock;
	tmp[eloop.reader_count].eloop_data = eloop_data;
	tmp[eloop.reader_count].user_data = user_data;
	tmp[eloop.reader_count].handler = handler;
	eloop.reader_count++;
	eloop.readers = tmp;
	if (sock > eloop.max_sock)
		eloop.max_sock = sock;

	return 0;
}

/* Unregister handler for read event */
int eloop_unregister_read_sock(int sock)
{
	int i;

	for (i = 0; i < eloop.reader_count; i++) {
		if (eloop.readers[i].sock == sock)
			break;
	}

	if (i >= eloop.reader_count)
		return -1;

	if (i + 1 < eloop.reader_count)
		memmove(&eloop.readers[i], &eloop.readers[i + 1],
			(eloop.reader_count - i - 1) *
			sizeof(struct eloop_sock));
	eloop.reader_count--;

	eloop.sock_unregistered = 1;

	/* max_sock for select need not be exact, so no need to update it */
	/* don't bother reallocating block, since this area is quite small and
	 * next registration will realloc anyway
	 */

	return 0;
}

/* Register timeout */
int eloop_register_timeout(unsigned int secs, unsigned int usecs,
	void (*handler)(void *eloop_ctx, void *timeout_ctx),
	void *eloop_data, void *user_data)
{
	struct eloop_timeout *timeout, *tmp, *prev;
	struct tms unused;



	timeout = (struct eloop_timeout *) malloc(sizeof(*timeout));
	if (timeout == NULL)
		return -1;
	timeout->time = times(&unused);
	/* Need to make sure secs isn't more than half the clock tick space. */
	timeout->time += secs * eloop.ticks_per_second;
	timeout->time += usecs / (1000000 / eloop.ticks_per_second);
	timeout->eloop_data = eloop_data;
	timeout->user_data = user_data;
	timeout->handler = handler;
	timeout->next = NULL;

	if (eloop.timeout == NULL) {
		eloop.timeout = timeout;
		return 0;
	}

	prev = NULL;
	tmp = eloop.timeout;
	while (tmp != NULL) {
		if (ELOOP_BEFORE(timeout->time, tmp->time))
			break;
		prev = tmp;
		tmp = tmp->next;
	}

	if (prev == NULL) {
		timeout->next = eloop.timeout;
		eloop.timeout = timeout;
	} else {
		timeout->next = prev->next;
		prev->next = timeout;
	}

	return 0;
}

/* Cancel timeouts matching <handler,eloop_data,user_data>.
 * ELOOP_ALL_CTX can be used as a wildcard for cancelling all timeouts
 * regardless of eloop_data/user_data.
 */
int eloop_cancel_timeout(void (*handler)(void *eloop_ctx, void *sock_ctx),
	void *eloop_data, void *user_data)
{
	struct eloop_timeout *timeout, *prev, *next;
	int removed = 0;

	prev = NULL;
	timeout = eloop.timeout;
	while (timeout != NULL) {
		next = timeout->next;

		if (timeout->handler == handler &&
		    (timeout->eloop_data == eloop_data ||
		     eloop_data == ELOOP_ALL_CTX) &&
		    (timeout->user_data == user_data ||
		     user_data == ELOOP_ALL_CTX)) {
			if (prev == NULL)
				eloop.timeout = next;
			else
				prev->next = next;
			free(timeout);
			removed++;
		} else
			prev = timeout;

		timeout = next;
	}

	return removed;
}

/* Signal callback */
static void eloop_handle_signal(int sig)
{
	int i;

	eloop.signaled++;
	for (i = 0; i < eloop.signal_count; i++) {
		if (eloop.signals[i].sig == sig) {
			eloop.signals[i].signaled++;
			break;
		}
	}
}

/* Process pending signals */
static void eloop_process_pending_signals(void)
{
	int i;
	struct tms unused;
	clock_t now;
	static clock_t last_time = 0;
	int check_children = 0;

	if (last_time == 0)
	{
		last_time = times(&unused);
	}
	now = times(&unused);
	if ((now - last_time) >
		(60 * eloop.ticks_per_second))
	{
		check_children = 1;
		last_time = now;
	}

	if (!check_children && (eloop.signaled == 0))
		return;
	eloop.signaled = 0;

	for (i = 0; i < eloop.signal_count; i++) {
		if (eloop.signals[i].signaled ||
			(check_children && (eloop.signals[i].sig == SIGCHLD))) {
			eloop.signals[i].signaled = 0;
			eloop.signals[i].handler(eloop.signals[i].sig,
			eloop.user_data,
			eloop.signals[i].user_data);
		}
	}

}

/* Register handler for signal.
 * Note: signals are 'global' events and there is no local eloop_data pointer
 * like with other handlers. The (global) pointer given to eloop_init() will be
 * used as eloop_ctx for signal handlers.
 */
int eloop_register_signal(int sig,
	void (*handler)(int sig, void *eloop_ctx, void *signal_ctx),
	void *user_data)
{
	struct eloop_signal *tmp;

	tmp = (struct eloop_signal *)
		realloc(eloop.signals,
			(eloop.signal_count + 1) *
			sizeof(struct eloop_signal));
	if (tmp == NULL)
		return -1;

	tmp[eloop.signal_count].sig = sig;
	tmp[eloop.signal_count].user_data = user_data;
	tmp[eloop.signal_count].handler = handler;
	tmp[eloop.signal_count].signaled = 0;
	eloop.signal_count++;
	eloop.signals = tmp;
	signal(sig, eloop_handle_signal);

	return 0;
}

/* Some packet from l2 call the handler */
void eloop_read(void)
{
	int i;

	if (!eloop.terminate &&
		(eloop.timeout || eloop.reader_count > 0)) {

		eloop.sock_unregistered = 0;

		for (i = 0; i < eloop.reader_count; i++) {

			if (eloop.sock_unregistered)
				break;

			eloop.readers[i].handler(
					eloop.readers[i].sock,
					eloop.readers[i].eloop_data,
					eloop.readers[i].user_data);
		}
	}
}

/* Start event loop and continue running as long as there are any registered
 * event handlers.
 */
void eloop_run(void)
{
	fd_set rfds;
	int i, res;
	struct tms unused;
	clock_t now, next;
	struct timeval tv;

	while (!eloop.terminate &&
		(eloop.timeout || eloop.reader_count > 0)) {
		if (eloop.timeout) {
			now = times(&unused);
			if (ELOOP_BEFORE(now, eloop.timeout->time))
				next = eloop.timeout->time - now;
			else
				next = 0;

			tv.tv_sec = next / eloop.ticks_per_second;
			tv.tv_usec = (next % eloop.ticks_per_second)
				* (1000000 / eloop.ticks_per_second);
		}

		FD_ZERO(&rfds);
		for (i = 0; i < eloop.reader_count; i++)
			FD_SET(eloop.readers[i].sock, &rfds);
		res = select(eloop.max_sock + 1, &rfds, NULL, NULL,
			eloop.timeout ? &tv : NULL);
		if (res < 0 && errno != EINTR) {
			perror("select");
			return;
		}
		eloop_process_pending_signals();

		/* check if some registered timeouts have occurred */
		if (eloop.timeout) {
			struct eloop_timeout *tmp;

			now = times(&unused);
			if (! ELOOP_BEFORE(now, eloop.timeout->time)) {
				tmp = eloop.timeout;
				eloop.timeout = eloop.timeout->next;
				tmp->handler(tmp->eloop_data,
					tmp->user_data);
				free(tmp);
			}

		}

		if (res <= 0)
			continue;

		eloop.sock_unregistered = 0;

		for (i = 0; i < eloop.reader_count; i++) {

			if (eloop.sock_unregistered)
				break;

			if (FD_ISSET(eloop.readers[i].sock, &rfds)) {
				eloop.readers[i].handler(
					eloop.readers[i].sock,
					eloop.readers[i].eloop_data,
					eloop.readers[i].user_data);
			}
		}
	}
}

/* Terminate event loop even if there are registered events. */
void eloop_terminate(void)
{
	eloop.terminate = 1;
}

/* Free any reserved resources. After calling eloop_destoy(), other eloop_*
 * functions must not be called before re-running eloop_init().
 */
void eloop_destroy(void)
{
	struct eloop_timeout *timeout, *prev;

	timeout = eloop.timeout;
	while (timeout != NULL) {
		prev = timeout;
		timeout = timeout->next;
		free(prev);
	}
	free(eloop.readers);
	free(eloop.signals);
}

/* Check whether event loop has been terminated. */
int eloop_terminated(void)
{
	return eloop.terminate;
}

/* Return user_data pointer that was registered with eloop_init() */
void * eloop_get_user_data(void)
{
	return eloop.user_data;
}
