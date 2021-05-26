/*
 * CHAN CONTEXT Module Public Interface
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
 * $Id: wlc_chanctxt.h 784933 2020-03-09 17:13:55Z $
 */
#ifndef _WLC_CHANCTXT_H_
#define _WLC_CHANCTXT_H_

/* multi-channel scheduler module interface */
extern wlc_chanctxt_info_t* wlc_chanctxt_attach(wlc_info_t* wlc);
extern void wlc_chanctxt_detach(wlc_chanctxt_info_t *chanctxt_info);

extern void wlc_chanctxt_set_passive_use(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool value);
extern void wlc_txqueue_start(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chanspec);
extern void wlc_txqueue_end(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool wlc_txqueue_active(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool wlc_has_chanctxt(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern chanspec_t wlc_get_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool wlc_shared_chanctxt(wlc_info_t *wlc, wlc_bsscfg_t *cfg1, wlc_bsscfg_t *cfg2);
/* Check if the channel is used by the given bsscfg */
extern bool wlc_shared_chanctxt_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chan);
extern bool wlc_shared_current_chanctxt(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool _wlc_ovlp_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chspec, uint chbw);
extern bool wlc_ovlp_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg1, wlc_bsscfg_t *cfg2, uint chbw);

#endif /* _WLC_CHANCTXT_H_ */
