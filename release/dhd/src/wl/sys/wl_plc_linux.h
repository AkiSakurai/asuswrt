/*
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: wl_plc_linux.h 467328 2014-04-03 01:23:40Z $
 */

/**
 * PLC = Power Line Communication
 */


#ifndef _WL_PLC_LINUX_H_
#define _WL_PLC_LINUX_H_

#define PATH_COST_WIFI		0
#define PATH_COST_PLC		100

#define PLC_NODE_AGING_TIME	180	/* in unit of second */

/* Node struct representing wifi plc node. Created when a sta capable of plc
 * associates with us or when we associate to a plc capable ap or when a sta
 * behind wet tunnel is created.
 */
typedef struct wl_plc_node {
	struct	wl_plc_node *next; 	/* Pointer to next node */
	uint8	node_type;		/* Node type */
	uint8	path_cost;		/* Preferred link, WiFi or PLC */
	struct	ether_addr ea;		/* MAC address of node */
	struct	ether_addr link_ea;	/* MAC address of WET device */
	wlc_bsscfg_t *cfg;		/* AP BSS that the node belongs to */
	struct scb *scb;		/* SCB of wifi sta or node behind wet */
	uint32	used;			/* Last usage time */
	uint32	aging_time;		/* Node aging time */
} wl_plc_node_t;

extern struct sk_buff *wl_plc_tx_prep(wl_if_t *wlif, struct sk_buff *skb);
extern void wl_plc_sendpkt(wl_if_t *wlif, struct sk_buff *skb, struct net_device *dev);
extern int32 wl_plc_recv(struct sk_buff *skb, struct net_device *dev, wl_plc_t *plc, uint16 if_in);
extern int32 wl_plc_init(wl_if_t *wlif);
extern void wl_plc_cleanup(wl_if_t *wlif);
extern int32 wl_plc_send_proc(struct wl_info *wl, void *p, struct wl_if *wlif);
extern int32 wl_plc_forward(void *p, struct net_device *dev, wl_plc_t *plc, uint16 if_in);
extern void wl_plc_power_off(si_t *sih);
#endif /* _WL_PLC_LINUX_H_ */
