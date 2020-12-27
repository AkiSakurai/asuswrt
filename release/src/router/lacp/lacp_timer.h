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

#ifndef	_lacp_timer_h_
#define	_lacp_timer_h_

/* timer id */
typedef enum {
	LACP_TIMER_LINK_STATUS,		/* for Port Link status */
	LACP_TIMER_RX_MACHINE,		/* 43.4.10 cruuent_while_timer */
	LACP_TIMER_PERIODIC_TX,		/* 43.4.10 periodic_timer */
	LACP_TIMER_SELECT_LOGIC,
	LACP_TIMER_MUX_MACHINE,		/* 43.4.10 wait_while_timer */
	LACP_TIMER_TX_MACHINE,
	LACP_TIMER_CHUNK_DECTION,	/* 43.4.10 actor_churn_timer and partner_churn_timer */
	LACP_TIMER_MAX
} lacp_timer_id_t;

typedef int (*lacp_timer_handler)(void *, int8);

int lacp_timer_init(osl_t *);
void lacp_timer_deinit(void);
int32 lacp_timer_register(int8 port, lacp_timer_id_t timer_id,
	lacp_timer_handler handler_function, void *context);
void lacp_timer_unregister(int8 port, lacp_timer_id_t timer_id);
void lacp_timer_start(int8 port, lacp_timer_id_t timer_id,
	uint32 elapse, bool repeat);
void lacp_timer_stop(int8 port, lacp_timer_id_t timer_id);
int32 lacp_get_timer_info(char *buf);

#endif	/* _lacp_timer_h_ */
