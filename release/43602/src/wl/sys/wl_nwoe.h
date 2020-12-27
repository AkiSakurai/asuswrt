/*
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */


#ifndef _wl_nwoe_h_
#define _wl_nwoe_h_

typedef struct wl_nwoe_info wl_nwoe_info_t;

#define NWOE_PKT_CONSUMED    0
#define NWOE_PKT_RETURNED    2
#define NWOE_PKT_ERROR      -1

#ifdef NWOE

#include <lwip/netif.h>

/* Process frames in receive direction */
extern int wl_nwoe_recv_proc(wl_nwoe_info_t *lwipi, osl_t *osh, void *pkt);

/*
 * Initialize Netwok Offload Engine private context.
 * Returns a pointer to the NWOE private context, NULL on failure.
 */
extern wl_nwoe_info_t *wl_nwoe_attach(wlc_info_t *wlc);

/* Cleanup NWOE private context */
extern void wl_nwoe_detach(wl_nwoe_info_t *nwoei);

/* Random number generator required for LWIP port */
extern uint32 wl_lwip_rand(struct netif *n);

#else

#define wl_nwoe_recv_proc(a, b, c)      (-1)
#define wl_nwoe_attach(a)		(wl_nwoe_info_t *)0x0dadbeef
#define	wl_nwoe_detach(a)		do {} while (0)

#endif /* NWOE */
#endif /* _wl_nwoe_h_ */
