/*
 * SMC related declarations
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
 * $Id: d11smc_code.c 2017-09-25 02:27:41Z srana $
 */

#include <wlc_cfg.h>

#if defined(WLSMC)
#include <typedefs.h>
#include <wlc.h>
#include <wlc_hw_priv.h>
#include <d11smc_code.h>
#include <wlc_smc.h>
#include <phy_smc_api.h>

bool wlc_smc_hw_supported(uint corerev)
{
	return (D11REV_GE(corerev, 128) && D11REV_LE(corerev, 132));
}

/* Download smc code and bring it up to run */
void
BCMINITFN(wlc_smc_download)(wlc_hw_info_t *wlc_hw)
{
	uint16 val;
	osl_t *osh;
	wlc_info_t *wlc;
	wlc =  wlc_hw->wlc;
	osh = wlc_hw->osh;
	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	if (wlc_hw->smc_loaded == FALSE) {
		phy_smc_reset(wlc_hw->band->pi, FALSE);

		val = R_REG(osh, D11_PSM_PHY_CTL(wlc));
		W_REG(osh, D11_PSM_PHY_CTL(wlc), (uint16)(val | PHYCTL_PHYRSTSMC_GE128));
		W_REG(osh, D11_PSM_PHY_CTL(wlc), (uint16)(val & ~(PHYCTL_PHYRSTSMC_GE128)));

		if (D11REV_IS(wlc_hw->corerev, 129) || D11REV_IS(wlc_hw->corerev, 130))
			phy_smc_download(wlc_hw->band->pi, d11smc1sz/6, d11smc1);
		else if (D11REV_IS(wlc_hw->corerev, 131))
			phy_smc_download(wlc_hw->band->pi, d11smc2sz/6, d11smc2);
		else if (D11REV_IS(wlc_hw->corerev, 132))
			phy_smc_download(wlc_hw->band->pi, d11smc_rev132sz/6,
				d11smc_rev132);
	}
	phy_smc_reset(wlc_hw->band->pi, TRUE);

	wlc_hw->smc_loaded = TRUE;
}

#endif /* WLSMC */
