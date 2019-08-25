/*
 * LCN40PHY Noise module implementation
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id$
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_wd.h>
#include "phy_type_noise.h"
#include <phy_lcn40.h>
#include <phy_lcn40_noise.h>

#ifndef ALL_NEW_PHY_MOD
/* < TODO: all these are going away... */
#include <wlc_phy_int.h>
#include <wlc_phy_lcn40.h>
/* TODO: all these are going away... > */
#endif

/* module private states */
struct phy_lcn40_noise_info {
	phy_info_t *pi;
	phy_lcn40_info_t *lcn40i;
	phy_noise_info_t *ii;
};

/* local functions */
static void phy_lcn40_noise_set_mode(phy_type_noise_ctx_t *ctx, int wanted_mode, bool init);
static bool phy_lcn40_noise_aci_wd(phy_wd_ctx_t *ctx);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int phy_lcn40_noise_dump(phy_type_noise_ctx_t *ctx, struct bcmstrbuf *b);
#else
#define phy_lcn40_noise_dump NULL
#endif

/* register phy type specific implementation */
phy_lcn40_noise_info_t *
BCMATTACHFN(phy_lcn40_noise_register_impl)(phy_info_t *pi, phy_lcn40_info_t *lcn40i,
	phy_noise_info_t *ii)
{
	phy_lcn40_noise_info_t *info;
	phy_type_noise_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage together */
	if ((info = phy_malloc(pi, sizeof(phy_lcn40_noise_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	bzero(info, sizeof(phy_lcn40_noise_info_t));
	info->pi = pi;
	info->lcn40i = lcn40i;
	info->ii = ii;

	if (CHIPID(pi->sh->chip) == BCM43142_CHIP_ID) {
		pi->sh->interference_mode = pi->sh->interference_mode_2G = WLAN_AUTO_W_NOISE;
	}

	/* register watchdog fn */
	if (phy_wd_add_fn(pi->wdi, phy_lcn40_noise_aci_wd, info,
	                  PHY_WD_PRD_1TICK, PHY_WD_1TICK_NOISE_ACI,
	                  PHY_WD_FLAG_NONE) != BCME_OK) {
		PHY_ERROR(("%s: phy_wd_add_fn failed\n", __FUNCTION__));
		goto fail;
	}

	/* register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.mode = phy_lcn40_noise_set_mode;
	fns.dump = phy_lcn40_noise_dump;
	fns.ctx = info;

	phy_noise_register_impl(ii, &fns);

	return info;

	/* error handling */
fail:
	if (info != NULL)
		phy_mfree(pi, info, sizeof(phy_lcn40_noise_info_t));
	return NULL;
}

void
BCMATTACHFN(phy_lcn40_noise_unregister_impl)(phy_lcn40_noise_info_t *info)
{
	phy_info_t *pi = info->pi;
	phy_noise_info_t *ii = info->ii;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* unregister from common */
	phy_noise_unregister_impl(ii);

	phy_mfree(pi, info, sizeof(phy_lcn40_noise_info_t));
}

/* set mode */
static void
phy_lcn40_noise_set_mode(phy_type_noise_ctx_t *ctx, int wanted_mode, bool init)
{
	phy_lcn40_noise_info_t *info = (phy_lcn40_noise_info_t *)ctx;
	phy_info_t *pi = info->pi;

	PHY_TRACE(("%s: mode %d init %d\n", __FUNCTION__, wanted_mode, init));

	if (init) {
		pi->interference_mode_crs_time = 0;
		pi->crsglitch_prev = 0;
		if (CHIPID(pi->sh->chip) == BCM43142_CHIP_ID) {
			wlc_lcn40phy_aci_init(pi);
		}
	}
	if (CHIPID(pi->sh->chip) == BCM43143_CHIP_ID) {
		wlc_lcn40phy_rev6_aci(pi, wanted_mode);
	}
}

/* watchdog callback */
static bool
phy_lcn40_noise_aci_wd(phy_wd_ctx_t *ctx)
{
	phy_lcn40_noise_info_t *ii = (phy_lcn40_noise_info_t *)ctx;
	phy_info_t *pi = ii->pi;

	/* defer interference checking, scan and update if RM is progress */
	if (!SCAN_RM_IN_PROGRESS(pi) &&
	    (CHIPID(pi->sh->chip) == BCM43142_CHIP_ID ||
	     CHIPID(pi->sh->chip) == BCM43341_CHIP_ID)) {
		wlc_phy_aci_upd(pi);
	}

	return TRUE;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
phy_lcn40_noise_dump(phy_type_noise_ctx_t *ctx, struct bcmstrbuf *b)
{
	phy_lcn40_noise_info_t *info = (phy_lcn40_noise_info_t *)ctx;
	phy_info_t *pi = info->pi;

	return phy_noise_dump_common(pi, b);
}
#endif
