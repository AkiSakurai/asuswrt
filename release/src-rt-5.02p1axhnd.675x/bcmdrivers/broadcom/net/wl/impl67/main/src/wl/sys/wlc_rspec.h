/*
 * ratespec related routines
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
 * $Id: wlc_rspec.h 780792 2019-11-04 09:27:32Z $
 */

#ifndef __wlc_rspec_h__
#define __wlc_rspec_h__

#include <typedefs.h>
#include <wlc_types.h>
#include <wlc_rate.h>

#include <bcmwifi_rspec.h>

extern int wlc_set_iovar_ratespec_override(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
		enum wlc_bandunit bandunit, uint32 wl_rspec, bool mcast);
extern ratespec_t wlc_rspec_to_rts_rspec(wlc_bsscfg_t *cfg, ratespec_t rspec, bool use_rspec);
extern ratespec_t wlc_lowest_basic_rspec(wlc_info_t *wlc, wlc_rateset_t *rs);
extern ratespec_t wlc_get_compat_basic_rspec(wlc_info_t *wlc, wlc_rateset_t *rs,
                                             uint8 rate_thold);

#if defined(MBSS) || defined(WLTXMONITOR)
extern ratespec_t ofdm_plcp_to_rspec(uint8 rate);
#endif /* MBSS || WLTXMONITOR */

extern ratespec_t wlc_get_rspec_history(wlc_bsscfg_t *cfg);
extern ratespec_t wlc_get_current_highest_rate(wlc_bsscfg_t *cfg);
void wlc_bsscfg_reprate_init(wlc_bsscfg_t *bsscfg);
void wlc_rspec_txexp_upd(wlc_info_t *wlc, ratespec_t *rspec);

/* attach/detach */
wlc_rspec_info_t *wlc_rspec_attach(wlc_info_t *wlc);
void wlc_rspec_detach(wlc_rspec_info_t *rsi);

/**
 * Lookup table for Frametypes [LEGACY (CCK / OFDM) /HT/VHT/HE]
 */
extern const uint8 wlc_rspec_enc2ft_tbl[];

/**
 * Extract Encoding field from RSPEC and use the result as Frametype TBL index
 */
/* Finding FT from rspec */
#define WLC_RSPEC_ENC2FT(rspec)	((RSPEC_ISLEGACY(rspec)) ? \
				 (RSPEC_ISOFDM(rspec) ? FT_OFDM : FT_CCK) : \
				 wlc_rspec_enc2ft_tbl[RSPEC_ENCODE(rspec)])

#endif /* __wlc_rspec_h__ */
