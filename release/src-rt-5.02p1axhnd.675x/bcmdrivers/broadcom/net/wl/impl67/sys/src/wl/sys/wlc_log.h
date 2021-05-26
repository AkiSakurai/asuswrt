/*
 * Event mechanism
 *
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
 * $Id:  $
 */

/**
 * @file
 * @brief
 * The WLAN driver currently has tight coupling between different components. In particular,
 * components know about each other, and call each others functions, access data, and invoke
 * callbacks. This means that maintenance and new features require changing these
 * relationships. This is fundamentally a tightly coupled system where everything touches
 * many other things.
 *
 * @brief
 * We can reduce the coupling between our features by reducing their need to directly call
 * each others functions, and access each others data. An mechanism for accomplishing this is
 * a generic event signaling mechanism. The event infrastructure enables modules to communicate
 * indirectly through events, rather than directly by calling each others routines and
 * callbacks.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlanSwArchitectureEventNotification]
 */

#ifndef _wlc_log_h_
#define _wlc_log_h_
#include  <wlc_types.h>
#include  <wl_dbg.h>
#include  <802.11.h>

/* Debug error log helpers */
#ifndef WL_PKTDROP_STATS
extern void wlc_log_unexpected_rx_frame_log_80211hdr(wlc_info_t *wlc,
	const struct dot11_header *h, wlc_rx_pktdrop_reason_t toss_reason);
extern void wlc_log_unexpected_tx_frame_log_8023hdr(wlc_info_t *wlc,
	const struct ether_header *eh, wlc_tx_pktdrop_reason_t toss_reason);
#endif // endif
extern void wlc_log_unexpected_tx_frame_log_80211hdr(wlc_info_t *wlc, const struct dot11_header *h);

#endif	/* _wlc_log_h_ */
