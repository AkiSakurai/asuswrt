/**
 * @file
 * @brief
 * ACTION FRAME Module Public Interface
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
 * $Id: wlc_act_frame.h 784444 2020-02-27 10:16:53Z $
 */

#ifndef _WLC_ACT_FRAME_H_
#define _WLC_ACT_FRAME_H_

wlc_act_frame_info_t* wlc_act_frame_attach(wlc_info_t* wlc);
void wlc_act_frame_detach(wlc_act_frame_info_t *mctxt);

extern int wlc_send_action_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	const struct ether_addr *bssid, void *action_frame);
extern int wlc_tx_action_frame_now(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, struct scb *scb);
extern int wlc_is_publicaction(wlc_info_t * wlc, struct dot11_header *hdr,
                               int len, struct scb *scb, wlc_bsscfg_t *bsscfg);

typedef int (*wlc_act_frame_tx_cb_fn_t)(wlc_info_t *wlc, void *arg, void *pkt);

extern int wlc_send_action_frame_off_channel(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	chanspec_t chanspec, int reason, int32 dwell_time, struct ether_addr *bssid,
	wl_action_frame_t *action_frame, wlc_act_frame_tx_cb_fn_t cb, void *arg);

/*
 * Action frame call back interface declaration
 */
typedef void (*wlc_actframe_callback)(wlc_info_t *wlc, void* handler_ctxt, uint *arg);

extern int wlc_msch_actionframe(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, chanspec_t chspec20,
	int reason, uint32 dwell_time, struct ether_addr *bssid,
	wlc_actframe_callback cb_func, void *arg);

extern int wlc_actframe_abort_action_frame(wlc_bsscfg_t *cfg);
bool wlc_af_dwelltime_inprogress(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#define ACT_FRAME_IN_PROGRESS(wlc, cfg) wlc_af_inprogress(wlc, cfg)
bool wlc_af_inprogress(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

extern void wlc_set_protected_dual_publicaction(uint8 *action_frame,
	uint8 mfp, wlc_bsscfg_t *bsscfg);

bool wlc_is_protected_dual_publicaction(uint8 act, wlc_bsscfg_t *bsscfg);
#endif /* _WLC_ACT_FRAME_H_ */
