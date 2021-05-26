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

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */

#include <wlc_cfg.h>
#include <wlc_types.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlc_log.h>
#include <wl_dbg.h>
#include <ethernet.h>
#include <802.11.h>
#include <wlioctl.h>
#include <wlc.h>
#include <wlc_rx.h>

#ifndef WL_PKTDROP_STATS
void wlc_log_unexpected_rx_frame_log_80211hdr(wlc_info_t *wlc,
	const struct dot11_header *h, wlc_rx_pktdrop_reason_t toss_reason)
{
	WL_ERROR(("Unexpected RX toss reason %d {if=wl%d fc=%04x seq=%04x "
		"A1=%02x:%02x:%02x:%02x:%02x:%02x A2=%02x:%02x:%02x:%02x:%02x:%02x}\n",
		toss_reason,
		WLCWLUNIT(wlc), ltoh16(h->fc), ltoh16(h->seq),
		h->a1.octet[0], h->a1.octet[1], h->a1.octet[2],
		h->a1.octet[3], h->a1.octet[4], h->a1.octet[5],
		h->a2.octet[0], h->a2.octet[1], h->a2.octet[2],
		h->a2.octet[3], h->a2.octet[4], h->a2.octet[5]));
}

void wlc_log_unexpected_tx_frame_log_8023hdr(wlc_info_t *wlc,
	const struct ether_header *eh, wlc_tx_pktdrop_reason_t toss_reason)
{
	WL_INFORM(("Unexpected TX toss reason %d {if=wl%d "
		"DST=%02x:%02x:%02x:%02x:%02x:%02x SRC=%02x:%02x:%02x:%02x:%02x:%02x}\n",
		toss_reason, WLCWLUNIT(wlc),
		eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
		eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5],
		eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2],
		eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5]));
}
#endif /* WL_PKTDROP_STATS */

void wlc_log_unexpected_tx_frame_log_80211hdr(wlc_info_t *wlc, const struct dot11_header *h)
{
	WL_PRINT(("Unexpected TX {if=wl%d fc=%04x seq=%04x "
		"A1=%02x:%02x:%02x:%02x:%02x:%02x A2=%02x:%02x:%02x:%02x:%02x:%02x}\n",
		WLCWLUNIT(wlc), ltoh16(h->fc), ltoh16(h->seq),
		h->a1.octet[0], h->a1.octet[1], h->a1.octet[2],
		h->a1.octet[3], h->a1.octet[4], h->a1.octet[5],
		h->a2.octet[0], h->a2.octet[1], h->a2.octet[2],
		h->a2.octet[3], h->a2.octet[4], h->a2.octet[5]));
}
