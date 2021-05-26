/*
 * STA Module Public Interface
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
 * $Id: wlc_sta.h 787288 2020-05-25 16:56:02Z $
 */
#ifndef _WLC_STA_H_
#define _WLC_STA_H_

/* specific bsscfg that is not supported by this sta module handling */
#define BSSCFG_SPECIAL(wlc, cfg) \
	(BSS_TDLS_ENAB(wlc, cfg) || BSS_PROXD_ENAB(wlc, cfg))

/* multi-channel scheduler module interface */
wlc_sta_info_t* wlc_sta_attach(wlc_info_t* wlc);
void wlc_sta_detach(wlc_sta_info_t *sta_info);

#ifdef STA
extern int wlc_sta_timeslot_register(wlc_bsscfg_t *cfg);
extern void wlc_sta_timeslot_unregister(wlc_bsscfg_t *cfg);
extern bool wlc_sta_timeslot_registed(wlc_bsscfg_t *cfg);
extern bool wlc_sta_timeslot_onchannel(wlc_bsscfg_t *cfg);
extern void wlc_sta_timeslot_update(wlc_bsscfg_t *cfg, uint32 start_tsf, uint32 interval);

extern void wlc_sta_pm_pending_complete(wlc_info_t *wlc);
extern void wlc_sta_set_pmpending(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool pmpending);

extern int wlc_sta_get_infra_bcn(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *buf, uint *len);
extern void wlc_sta_update_bw(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	chanspec_t old_chanspec, chanspec_t chanspec);
extern bool wlc_sta_msch_check_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#else

#define wlc_sta_timeslot_register(cfg)	(BCME_ERROR)
#define wlc_sta_timeslot_unregister(cfg)
#define wlc_sta_timeslot_registed(cfg)
#define wlc_sta_timeslot_update(cfg, start_tsf, interval)

#define wlc_sta_pm_pending_complete(wlc)
#define wlc_sta_set_pmpending(wlc, cfg, pmpending)
#define wlc_sta_fifo_suspend_complete(wlc, cfg)
#define wlc_sta_update_bw(wlc, cfg, old_chanspec, chanspec)
#define wlc_sta_msch_check_on_chan(wlc, cfg)
#endif /* STA */

#endif /* _WLC_STA_H_ */
