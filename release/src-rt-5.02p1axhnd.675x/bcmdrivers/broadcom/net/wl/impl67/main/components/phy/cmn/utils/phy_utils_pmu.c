/*
 * PMU control module implementation - shared by PHY type specific implementations.
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
 * $Id: phy_utils_pmu.c 781580 2019-11-25 10:17:12Z $
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <phy_api.h>
#include <phy_utils_pmu.h>
#include <siutils.h>
#include <sbchipc.h>
#include <hndpmu.h>

#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>

void
phy_utils_pmu_regcontrol_access(phy_info_t *pi, uint8 addr, uint32* val, bool write)
{
	si_t *sih;
	chipcregs_t *cc;
	uint origidx, intr_val;

	/* shared pi handler */
	sih = (si_t*)pi->sh->sih;

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);

	if (write) {
		W_REG(si_osh(sih), &cc->regcontrol_addr, (uint32)addr);
		W_REG(si_osh(sih), &cc->regcontrol_data, *val);
		/* read back to confirm */
		*val = R_REG(si_osh(sih), &cc->regcontrol_data);
	} else {
		W_REG(si_osh(sih), &cc->regcontrol_addr, (uint32)addr);
		*val = R_REG(si_osh(sih), &cc->regcontrol_data);
	}

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
}

void
phy_utils_pmu_chipcontrol_access(phy_info_t *pi, uint8 addr, uint32* val, bool write)
{
	si_t *sih;
	chipcregs_t *cc;
	uint origidx, intr_val;

	/* shared pi handler */
	sih = (si_t*)pi->sh->sih;

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);

	if (write) {
		W_REG(si_osh(sih), &cc->chipcontrol_addr, (uint32)addr);
		W_REG(si_osh(sih), &cc->chipcontrol_data, *val);
		/* read back to confirm */
		*val = R_REG(si_osh(sih), &cc->chipcontrol_data);
	} else {
		W_REG(si_osh(sih), &cc->chipcontrol_addr, (uint32)addr);
		*val = R_REG(si_osh(sih), &cc->chipcontrol_data);
	}

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
}

/*
 * APIs for 6710 to switch and restore setting of BBPLL post-divider
 * Switch BBPLL frequency before calling resetcca of phy calibration
 * Restore BBPLL frequency after finishing resetcca
 */
#define BBPLL_DIV_2_6710		0x18181812
#define BBPLL_DIV_4_6710		0x30303024
#define BBPLL_DIV_8_6710		0x60606048
#define BBPLL_DIV_16_6710		0xc0c0c090
#define BBPLL_SW_DELAY			1

void
phy_utils_pmu_bbpll_freq_switch(phy_info_t *pi, uint *pll_val, uint restore)
{
	si_t *sih;
	uint div_ratio = BBPLL_DIV_16_6710;
	uint dly = BBPLL_SW_DELAY;

	ASSERT(pll_val != NULL);

	/* shared pi handler */
	sih = (si_t*)pi->sh->sih;

	if (BCM6710_CHIP(sih->chip)) {
		if (restore) {
			si_pmu_pllcontrol(sih, PMU_CHIPCTL4, ~0, *pll_val);
		} else {
			*pll_val = si_pmu_pllcontrol(sih, PMU_CHIPCTL4, 0, 0);
			si_pmu_pllcontrol(sih, PMU_CHIPCTL4, ~0, div_ratio);
		}
		si_pmu_pllupd(sih);
		OSL_DELAY(dly);
	}
}
