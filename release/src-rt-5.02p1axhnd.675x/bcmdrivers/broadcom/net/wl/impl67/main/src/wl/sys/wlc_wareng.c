/*
 * WAR-engine related declarations
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_wareng.c 2020-01-31 11:51:41 ql888745 $
 */

#include <wlc_cfg.h>

#if defined(WLWARENG)
#include <typedefs.h>
#include <wlc.h>
#include <wlc_hw_priv.h>
#include <d11wareng_code.h>
#include <wlc_wareng.h>
#include <phy_wareng_api.h>

bool
wlc_wareng_hw_supported(uint corerev)
{
	return (D11REV_IS(corerev, 132));
}

/* Download wareng code and bring it up to run */
void
BCMINITFN(wlc_wareng_download)(wlc_hw_info_t *wlc_hw)
{
	wlc_info_t *wlc;
	wlc = wlc_hw->wlc;
	if (D11REV_IS(wlc_hw->corerev, 132)) {
		WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

		// phy cpu reset top
		si_wrapperreg(wlc_hw->sih, AI_IOCTRL, 1 << 21, 0 << 21);

		// phy cpu reset core
		si_wrapperreg(wlc_hw->sih, AI_IOCTRL, 1 << 20, 0 << 20);

		OSL_DELAY(2);

		// Release phy cpu reset top
		si_wrapperreg(wlc->pub->sih, AI_IOCTRL, 1 << 21, 1 << 21);

		if (wlc_hw->wareng_loaded == FALSE) {
			phy_wareng_download(wlc_hw->band->pi, d11wareng0sz, d11wareng0);
		}

		// Release phy cpu reset core
		si_wrapperreg(wlc->pub->sih, AI_IOCTRL, 1 << 20, 1 << 20);

		wlc_hw->wareng_loaded = TRUE;
	}
}

#endif /* WLWARENG */
