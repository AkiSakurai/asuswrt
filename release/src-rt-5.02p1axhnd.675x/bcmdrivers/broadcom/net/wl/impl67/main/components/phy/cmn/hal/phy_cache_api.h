/*
 * Cache module public interface (to MAC driver).
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
 * $Id: phy_cache_api.h 780794 2019-11-04 09:44:00Z $
 */

#ifndef _phy_cache_api_h_
#define _phy_cache_api_h_

#include <typedefs.h>
#include <phy_api.h>

/* default cache capacity : This is the max number of cache entries */
#ifndef PHY_CACHE_SZ
#define PHY_CACHE_SZ 4
#endif // endif

/* API for cache validation */
typedef int (*phy_cache_validate_fn_t)(void *context);

#if defined(PHYCAL_CACHING)
void phy_cache_cal(phy_info_t *pi);
int phy_cache_restore_cal(phy_info_t *pi);
int wlc_phy_reuse_chanctx(phy_info_t *pi, chanspec_t chanspec);
int wlc_phy_invalidate_chanctx(phy_info_t *pi, chanspec_t chanspec);
void wlc_phy_update_chctx_glacial_time(phy_info_t *pi, chanspec_t chanspec);
uint32 wlc_phy_get_current_cachedchans(phy_info_t *pi);
void wlc_phy_get_all_cached_ctx(phy_info_t *pi, chanspec_t *chanlist);
extern int	wlc_phy_cal_cache_init(phy_info_t *pi);
extern void wlc_phy_cal_cache_deinit(phy_info_t *pi);
extern void wlc_phy_cal_cache_set(phy_info_t *pi, bool state);
extern bool wlc_phy_cal_cache_get(phy_info_t *pi);
int phy_cache_register_cb(phy_info_t *pi, phy_cache_validate_fn_t fn, void *context);
#endif /* PHYCAL_CACHING */

#endif /* _phy_cache_api_h_ */
