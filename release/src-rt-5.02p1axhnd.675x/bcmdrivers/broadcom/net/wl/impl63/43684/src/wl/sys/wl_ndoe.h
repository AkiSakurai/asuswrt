/*
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * Neighbor Advertisement Offload interface
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 * $Id: wl_ndoe.h 772878 2019-03-06 08:25:59Z $
 */

#ifndef _wl_ndoe_h_
#define _wl_ndoe_h_

#include <bcmipv6.h>

/* Forward declaration */
typedef struct wl_nd_info wl_nd_info_t;

#ifdef WLNDOE
extern wl_nd_info_t * wl_nd_attach(wlc_info_t *wlc);
extern void wl_nd_detach(wl_nd_info_t *ndi);
extern int wl_nd_recv_proc(wl_nd_info_t *ndi, void *sdu);
extern wl_nd_info_t * wl_nd_alloc_ifndi(wl_nd_info_t *ndi_p, wlc_if_t *wlcif);
extern void wl_nd_clone_ifndi(wl_nd_info_t *from_ndi, wl_nd_info_t *to_ndi);
extern int wl_nd_send_proc(wl_nd_info_t *ndi, void *sdu);
extern wl_nd_info_t * wl_nd_alloc_ifndi(wl_nd_info_t *ndi_p, wlc_if_t *wlcif);
extern void wl_nd_free_ifndi(wl_nd_info_t *ndi);
#ifdef WLNDOE_RA
extern int wl_nd_ra_filter_clear_cache(wl_nd_info_t *ndi);
#endif /* WLNDOE_RA */
#endif /* WLNDOE */

#endif /* _WL_NDOE_H_ */
