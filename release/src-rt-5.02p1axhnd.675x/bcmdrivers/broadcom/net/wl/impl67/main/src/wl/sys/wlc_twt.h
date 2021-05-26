/*
 * 802.11ah/11ax Target Wake Time protocol and d11 h/w manipulation.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_twt.h 789164 2020-07-21 15:14:52Z $
 */

#ifndef _wlc_twt_h_
#define _wlc_twt_h_

#define WLC_TWT_CAP_BCAST_TWT_SUPPORT		0
#define WLC_TWT_CAP_TWT_RESP_SUPPORT		1
#define WLC_TWT_CAP_TWT_REQ_SUPPORT		2

#ifdef WLTWT

/* attach/detach */
extern wlc_twt_info_t *wlc_twt_attach(wlc_info_t *wlc);
extern void wlc_twt_detach(wlc_twt_info_t *twti);
extern void wlc_twt_disable(wlc_info_t *wlc);
extern void wlc_twt_dump_schedblk(wlc_info_t *wlc);
extern bool wlc_twt_req_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool wlc_twt_resp_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool wlc_twt_bcast_cap(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_twt_scb_set_cap(wlc_twt_info_t *twti, scb_t *scb, uint8 cap_idx, bool set);
extern void wlc_twt_tbtt(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern int wlc_twt_actframe_proc(wlc_twt_info_t *twti, uint action_id,
	struct dot11_management_header *hdr, scb_t *scb, uint8 *body, uint body_len);
extern bool wlc_twt_scb_active(wlc_twt_info_t *twti, scb_t *scb);
extern uint8 wlc_twt_scb_info(wlc_twt_info_t *twti, scb_t *scb);
extern void wlc_twt_rx_pkt_trigger(wlc_twt_info_t *twti, scb_t *scb);
extern void wlc_twt_intr(wlc_twt_info_t *twti);
extern bool wlc_twt_scb_get_schedid(wlc_twt_info_t *twti, scb_t *scb, uint16 *schedid);
extern void wlc_twt_fill_link_entry(wlc_twt_info_t *twti, scb_t *scb,
	d11linkmem_entry_t *link_entry);
extern void wlc_twt_ps_suppress_done(wlc_twt_info_t *twti, scb_t *scb);
extern void wlc_twt_apps_ready_for_twt(wlc_twt_info_t *twti, scb_t *scb);
extern bool wlc_twt_scb_is_trig_enab(wlc_twt_info_t *twti, scb_t *scb);

#else

#define wlc_twt_disable(a)
#define wlc_twt_dump_schedblk(a)
#define wlc_twt_req_cap(a, b)			FALSE
#define wlc_twt_resp_cap(a, b)			FALSE
#define wlc_twt_bcast_cap(a, b)			FALSE
#define wlc_twt_scb_set_cap(a, b, c, d)
#define wlc_twt_tbtt(a, b)
#define wlc_twt_actframe_proc(a, b, c, d, e, f)
#define wlc_twt_scb_active(a, b)		FALSE
#define wlc_twt_rx_pkt_trigger(a, b)
#define wlc_twt_intr(a)
#define wlc_twt_scb_get_schedid(a, b, c)	FALSE
#define wlc_twt_fill_link_entry(a, b, c)
#define wlc_twt_ps_suppress_done(a, b)
#define wlc_twt_scb_is_trig_enab(a, b)		FALSE

#endif /* WLTWT */

#define wlc_twt_set_twt_required(a, b, c)

#endif /* _wlc_twt_h_ */
