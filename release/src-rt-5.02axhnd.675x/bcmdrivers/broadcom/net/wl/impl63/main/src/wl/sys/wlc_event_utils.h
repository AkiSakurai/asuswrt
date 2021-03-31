/*
 * event wrapper related routines
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
 * $Id: wlc_event_utils.h 746424 2018-02-13 07:17:15Z $
 */

#ifndef _wlc_event_utils_h_
#define _wlc_event_utils_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <bcmevent.h>
#include <hndd11.h>

extern void wlc_event_if(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_event_t *e,
	const struct ether_addr *addr);

extern void wlc_mac_event(wlc_info_t* wlc, uint msg, const struct ether_addr* addr,
	uint result, uint status, uint auth_type, void *data, int datalen);
extern void wlc_bss_mac_event(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
	const struct ether_addr* addr, uint result, uint status, uint auth_type,
	void *data, int datalen);
extern void wlc_bss_mac_event_immediate(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
	const struct ether_addr* addr, uint result, uint status, uint auth_type,
	void *data, int datalen);
extern int wlc_bss_mac_rxframe_event(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
	const struct ether_addr* addr, uint result, uint status, uint auth_type,
	void *data, int datalen, wl_event_rx_frame_data_t *rxframe_data);
extern void wlc_recv_prep_event_rx_frame_data(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
                                         wl_event_rx_frame_data_t *rxframe_data);
#if !defined(WLNOEIND)
void wlc_bss_eapol_event(wlc_info_t *wlc, wlc_bsscfg_t * bsscfg,
                                const struct ether_addr *ea, uint8 *data, uint32 len);
void wlc_eapol_event_indicate(wlc_info_t *wlc, void *p, struct scb *scb, wlc_bsscfg_t *bsscfg,
                              struct ether_header *eh);
#else
#define wlc_bss_eapol_event(wlc, cfg, ea, data, len) do {} while (0)
#define wlc_eapol_event_indicate(wlc, p, scb, cfg, ea) do {} while (0)
#endif /* !defined(WLNOEIND) */

extern void wlc_if_event(wlc_info_t *wlc, uint8 what, wlc_if_t *wlcif);
extern void wlc_process_event(wlc_info_t *wlc, wlc_event_t *e);

#define SCB_EA(scb) ((scb) ? &((scb)->ea) : NULL)
#define SCB_OR_HDR_EA(scb, hdr) ((scb) ? &((scb)->ea) : ((hdr) ? &((hdr)->sa) : NULL))

#ifdef BCM_CEVENT
extern int wlc_send_cevent(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr* addr,
                          uint result, uint status, uint auth_type, void *data, int datalen,
                          uint32 ce_subtype, uint32 ce_flags);
#else /* BCM_CEVENT */
#define wlc_send_cevent(a, b, c, d, e, f, g, h, i, j) do {} while (0)
#endif /* BCM_CEVENT */

#endif /* _wlc_event_utils_h_ */
