/*
 * Neighbor Advertisement Offload interface
 *
 *   Copyright 2020 Broadcom
 *
 *   This program is the proprietary software of Broadcom and/or
 *   its licensors, and may only be used, duplicated, modified or distributed
 *   pursuant to the terms and conditions of a separate, written license
 *   agreement executed between you and Broadcom (an "Authorized License").
 *   Except as set forth in an Authorized License, Broadcom grants no license
 *   (express or implied), right to use, or waiver of any kind with respect to
 *   the Software, and Broadcom expressly reserves all rights in and to the
 *   Software and all intellectual property rights therein.  IF YOU HAVE NO
 *   AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *   WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *   THE SOFTWARE.
 *
 *   Except as expressly set forth in the Authorized License,
 *
 *   1. This program, including its structure, sequence and organization,
 *   constitutes the valuable trade secrets of Broadcom, and you shall use
 *   all reasonable efforts to protect the confidentiality thereof, and to
 *   use this information only in connection with your use of Broadcom
 *   integrated circuit products.
 *
 *   2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *   "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *   REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *   OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *   DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *   NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *   ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *   CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *   OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *   3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *   BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *   SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *   IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *   IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *   ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *   OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *   NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
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
#endif // endif
#endif /* WLNDOE */

#endif /* _WL_NDOE_H_ */
