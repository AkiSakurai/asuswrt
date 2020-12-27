/*
 * Neighbor Advertisement Offload interface
 *
 *   Copyright (C) 2014, Broadcom Corporation
 *   All Rights Reserved.
 *   
 *   This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *   the contents of this file may not be disclosed to third parties, copied
 *   or duplicated in any form, in whole or in part, without the prior
 *   written permission of Broadcom Corporation.
 *
 * $Id: wl_ndoe.h 472326 2014-04-23 17:42:11Z $
 */


#ifndef _wl_ndoe_h_
#define _wl_ndoe_h_

#include <proto/bcmipv6.h>

/* Forward declaration */
typedef struct wl_nd_info wl_nd_info_t;

/* Return values */
#define ND_REPLY_PEER 		0x1	/* Reply was sent to service NS request from peer */
#define ND_REQ_SINK			0x2	/* Input packet should be discarded */
#define ND_FORCE_FORWARD    0X3	/* For the dongle to forward req to HOST */

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* Neighbor Solicitation Response Offload IOVAR param */
typedef BWL_PRE_PACKED_STRUCT struct nd_param {
	struct ipv6_addr	host_ip[2];
	struct ipv6_addr	solicit_ip;
	struct ipv6_addr	remote_ip;
	uint8	host_mac[ETHER_ADDR_LEN];
	uint32	offload_id;
} BWL_POST_PACKED_STRUCT nd_param_t;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#ifdef WLNDOE
extern wl_nd_info_t * wl_nd_attach(wlc_info_t *wlc);
extern void wl_nd_detach(wl_nd_info_t *ndi);
extern int wl_nd_recv_proc(wl_nd_info_t *ndi, void *sdu);
extern int wl_nd_intercept_ra_packets(wlc_info_t *wlc, void *sdu);
extern wl_nd_info_t * wl_nd_alloc_ifndi(wl_nd_info_t *ndi_p, wlc_if_t *wlcif);
extern int wl_nd_send_proc(wl_nd_info_t *ndi, void *sdu);
extern wl_nd_info_t * wl_nd_alloc_ifndi(wl_nd_info_t *ndi_p, wlc_if_t *wlcif);
extern void wl_nd_free_ifndi(wl_nd_info_t *ndi);
extern int wl_nd_ra_filter_clear_cache(wl_nd_info_t *ndi);
#ifdef BCM_OL_DEV
extern void wl_nd_proc_msg(wlc_dngl_ol_info_t *wlc_dngl_ol, wl_nd_info_t *ndi, void *buf);
extern void wl_nd_update_stats(wl_nd_info_t *ndi, bool suppressed);
#endif
#endif /* WLNDOE */

#endif /* _WL_NDOE_H_ */
