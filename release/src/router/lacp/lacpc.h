/*
 * Broadcom LACP driver - common code routines
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

#ifndef _lacpc_h_
#define _lacpc_h_

#define LACPC_MAX_CB 1

#define LACPC_OFF	0
#define LACPC_ON	1
#define LACPC_AUTO	2

/* function type registered by fsm */
typedef int32 (*lacpc_rcv_fn_t)(void *hdl, int8 portid, void *pkt, int32 pkt_len);
typedef int32 (*lacpc_portchg_fn_t)(void *hdl, int8 portid, uint32 linkup,
	uint32 speed, uint32 fulldplx);

/* rx callback */
typedef struct lacpc_rcv_cb {
	bool inuse;
	void *hdl;
	lacpc_rcv_fn_t rcv_fn;
} lacpc_rcv_cb_t;

/* port change callback */
typedef struct lacpc_portchg_cb {
	bool inuse;
	void *hdl;
	lacpc_portchg_fn_t portchg_fn;
} lacpc_portchg_cb_t;

typedef struct lacpc_info {
	void *osh;					/* OS layer handle */
	void *lacpi;					/* OS layer LACP info */
	void *fsmi;					/* FSM info */
	lacpc_rcv_cb_t rcv_cb[LACPC_MAX_CB];		/* rcv callbacks */
	lacpc_portchg_cb_t portchg_cb[LACPC_MAX_CB];	/* portchange callbacks */
	lacpc_osl_fn_t osl_func;			/* OS layer functions */
	uint32 last_linksts;				/* keep the last link status */
	int8 on;					/* lacpc off/on/auto */
} lacpc_info_t;

/* function called by fsm */
int32 lacpc_register_rcv_handler(void *lacpc_hdl, void *hdl, lacpc_rcv_fn_t rcv_fn);
int32 lacpc_unregister_rcv_handler(void *lacpc_hdl, void *hdl, lacpc_rcv_fn_t rcv_fn);
int32 lacpc_register_portchg_handler(void *lacpc_hdl, void *hdl,
	lacpc_portchg_fn_t portchg_fn);
int32 lacpc_unregister_portchg_handler(void *lacpc_hdl, void *hdl,
	lacpc_portchg_fn_t portchg_fn);
int32 lacpc_send(void *lacpc_hdl, int8 portid, void *pkt, int32 pkt_len);
int32 lacpc_update_agg(void *lacpc_hdl, int8 group, uint16 portmap);
int32 lacpc_get_hostmac(void *lacpc_hdl, uint8 *hostmac);
#endif /* !_lacpc_h_ */
