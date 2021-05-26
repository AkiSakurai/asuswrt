/*
 * Common (OS-independent) portion of
 * Broadcom Station Prioritization Module
 *
 * This module is used to manage scb memory allocation.
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
 * $Id: wlc_scb_alloc.h $
 */

#ifndef _wlc_scb_alloc_h_
#define _wlc_scb_alloc_h_

enum {
	SCB_CUBBY_ID_SCBINFO = 0,
	SCB_CUBBY_ID_AMPDU = 1,
	SCB_CUBBY_ID_AMPDU_RX = 2,
	SCB_CUBBY_ID_RATESEL = 3,
	SCB_CUBBY_ID_RRM = 4,
	SCB_CUBBY_ID_TAF = 5,
	SCB_CUBBY_ID_TXBF = 6,
	SCB_CUBBY_ID_TXC = 7,
	SCB_CUBBY_ID_KEY = 8,
	SCB_CUBBY_ID_RXIV = 9,
	SCB_CUBBY_ID_POWERSEL = 10,
	SCB_CUBBY_ID_NAR = 11,
	SCB_CUBBY_ID_LAST = 12
};

#define HOST_SCB_CNT	80

#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
#define HOST_MEM_ADDR_RANGE 0x08000000
#endif // endif

extern wlc_scb_alloc_info_t *wlc_scb_alloc_attach(wlc_info_t *wlc);
extern void wlc_scb_alloc_detach(wlc_scb_alloc_info_t *scb_alloc_info);

extern void *wlc_scb_alloc_mem_get(wlc_info_t *wlc, int cubby_id, int size, int num);
extern void wlc_scb_alloc_mem_free(wlc_info_t *wlc, int cubby_id, void *cubby);

extern bool wlc_scb_alloc_ishost(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

#endif /* _wlc_scb_alloc_h_ */
