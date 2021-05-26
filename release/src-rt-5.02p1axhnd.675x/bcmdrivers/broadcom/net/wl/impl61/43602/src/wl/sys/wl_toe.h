/*
 * TCP Offload Engine (TOE) components interface.
 *
 * This offload is not related to the hardware offload engines in AC chips.
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
 *   $Id: wl_toe.h 453919 2014-02-06 23:10:30Z $
 */

#ifndef _wl_toe_h_
#define _wl_toe_h_

/* Forward declaration */
typedef struct wl_toe_info wl_toe_info_t;

#ifdef TOE

/*
 * Initialize toe private context.It returns a pointer to the
 * toe private context if succeeded. Otherwise it returns NULL.
 */
extern wl_toe_info_t *wl_toe_attach(wlc_info_t *wlc);

/* Cleanup toe private context */
extern void wl_toe_detach(wl_toe_info_t *toei);

/* Process frames in transmit direction */
extern void wl_toe_send_proc(wl_toe_info_t *toei, void *sdu);

/* Process frames in receive direction */
extern int wl_toe_recv_proc(wl_toe_info_t *toei, void *sdu);

extern void wl_toe_set_olcmpnts(wl_toe_info_t *toei, int ol_cmpnts);

#else	/* stubs */

#define wl_toe_attach(a)		(wl_toe_info_t *)0x0dadbeef
#define	wl_toe_detach(a)		do {} while (0)
#define wl_toe_send_proc(a, b)		do {} while (0)
#define wl_toe_recv_proc(a, b)		(-1)
#define wl_toe_set_olcmpnts(a, b)	do {} while (0)

#endif /* TOE */

#endif	/* _wl_toe_h_ */
