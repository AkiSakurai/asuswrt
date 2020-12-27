/*
 * Broadcom LACP driver header file
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

#ifndef _lacpc_export_h_
#define _lacpc_export_h_

#define LACPC_PORT_LINK_DOWN	0
#define LACPC_PORT_LINK_UP	1

#define LACPC_PORT_HALF_DUPLEX  0
#define LACPC_PORT_FULL_DUPLEX  1

#define LACPC_PORT_SPEED_10	0
#define LACPC_PORT_SPEED_100    1
#define LACPC_PORT_SPEED_1000   2

/* OS layer wrapper */
typedef struct lacpc_osl_fn {
	int32 (*send_fn)(void *lacpi, void *pkt, int32 pkt_len);
	int32 (*update_agg_fn)(void *lacpi, int8 group, uint32 portmap);
	int32 (*get_portsts_fn)(void *lacpi, int8 portid, uint32 *link, uint32 *speed,
		uint32 *duplex);
	int32 (*get_linksts_fn)(void *lacpi, uint32 *linksts);
	int32 (*get_hostmac_fn)(void *lacpi, uint8 *hostmac);
} lacpc_osl_fn_t;

/* function called by OS layer */
extern void *lacpc_init(void *lacpi, osl_t *osh, lacpc_osl_fn_t *osl_func, int8 on);
extern void lacpc_deinit(void *lacpc_hdl);
extern int32 lacpc_rcv(void *lacpc_hdl, void *h, int len);
#endif /* !_lacpc_export_h_ */
