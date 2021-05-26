/*
 * Wake-on-Wireless related header file
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_wowl.h 783430 2020-01-28 14:19:14Z $
*/

#ifndef _wlc_wowl_h_
#define _wlc_wowl_h_

#ifdef WOWL

extern wowl_info_t *wlc_wowl_attach(wlc_info_t *wlc);
extern void wlc_wowl_detach(wowl_info_t *wowl);
extern bool wlc_wowl_cap(struct wlc_info *wlc);
extern bool wlc_wowl_enable(wowl_info_t *wowl);
extern uint32 wlc_wowl_clear(wowl_info_t *wowl);
extern uint32 wlc_wowl_clear_bmac(wowl_info_t *wowl);
extern int wlc_wowl_wake_reason_process(wowl_info_t *wowl);
extern int wlc_wowl_verify_d3_exit(wlc_info_t * wlc);
#else	/* stubs */
#define wlc_wowl_attach(a) (wowl_info_t *)0x0dadbeef
#define	wlc_wowl_detach(a) do {} while (0)
#define wlc_wowl_cap(a) FALSE
#define wlc_wowl_enable(a) FALSE
#define wlc_wowl_clear(b) (0)
#define wlc_wowl_clear_bmac(b) (0)
#define wlc_wowl_wake_reason_process(a) do {} while (0)
#define wlc_wowl_verify_d3_exit(BCME_OK)
#endif /* WOWL */

#define WOWL_IPV4_ARP_TYPE			0
#define WOWL_IPV6_NS_TYPE			1
#define WOWL_DOT11_RSN_REKEY_TYPE	2
#define WOWL_OFFLOAD_INVALID_TYPE	3

#define	WOWL_IPV4_ARP_IDX		0
#define	WOWL_IPV6_NS_0_IDX		1
#define	WOWL_IPV6_NS_1_IDX		2
#define	WOWL_DOT11_RSN_REKEY_IDX	3
#define	WOWL_OFFLOAD_INVALID_IDX	4

#define	MAX_WOWL_OFFLOAD_ROWS		4
#define	MAX_WOWL_IPV6_ARP_PATTERNS	1
#define	MAX_WOWL_IPV6_NS_PATTERNS	2	/* Number of NS offload address patterns allowed */
#define	MAX_WOWL_IPV6_NS_OFFLOADS	1	/* Number of NS offload requests supported */

/* Reserve most	significant bits for internal usage for patterns */
#define	WOWL_INT_RESERVED_MASK      0xFF000000  /* Reserved for internal use */
#define	WOWL_INT_DATA_MASK          0x00FFFFFF  /* Mask for user data  */
#define	WOWL_INT_PATTERN_FLAG       0x80000000  /* OS-generated pattern */
#define	WOWL_INT_NS_TA2_FLAG        0x40000000  /* Pattern from NS TA2 of offload pattern */
#define	WOWL_INT_PATTERN_IDX_MASK   0x0F000000  /* Mask for pattern index in offload table */
#define	WOWL_INT_PATTERN_IDX_SHIFT  24          /* Note: only used for NS patterns */

/* number of WOWL patterns supported */
#define MAXPATTERNS(wlc) \
	(wlc_wowl_cap(wlc) ?									\
	(D11REV_LT((wlc)->pub->corerev, 40) ? 12 : 4)	: 0)

#define WOWL_KEEPALIVE_FIXED_PARAM	11
#define WOWL_NLDT_DEFAULT_INTVL		240000 /* in units of ms (4 mins) */

#endif /* _wlc_wowl_h_ */
