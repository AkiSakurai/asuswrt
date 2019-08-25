/*
 * LCNPHY NOISEmeasure module implementation
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

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_noise.h>
#include <phy_type_noise.h>
#include <phy_lcn.h>
#include <phy_lcn_noise.h>

#ifndef ALL_NEW_PHY_MOD
/* < TODO: all these are going away... */
#include <wlc_phy_int.h>
#include <wlc_phy_lcn.h>
/* TODO: all these are going away... > */
#endif

/* module private states */
struct phy_lcn_noise_info {
	phy_info_t *pi;
	phy_lcn_info_t *lcni;
	phy_noise_info_t *nxi;
};

/* local functions */
static void phy_lcn_noise_set_mode(phy_type_noise_ctx_t *ctx, int wanted_mode, bool init);
static bool phy_lcn_noise_aci_wd(phy_wd_ctx_t *ctx);
static bool phy_lcn_noise_start(phy_type_noise_ctx_t *ctx, uint8 reason);
static void phy_lcn_noise_stop(phy_type_noise_ctx_t *ctx);

/* Register/unregister LCNPHY specific implementation to common layer. */
phy_lcn_noise_info_t *
BCMATTACHFN(phy_lcn_noise_register_impl)(phy_info_t *pi, phy_lcn_info_t *lcni,
	phy_noise_info_t *nxi)
{
	phy_lcn_noise_info_t *info;
	phy_type_noise_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage in once */
	if ((info = phy_malloc(pi, sizeof(phy_lcn_noise_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	bzero(info, sizeof(phy_lcn_noise_info_t));
	info->pi = pi;
	info->lcni = lcni;
	info->nxi = nxi;

	if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
		pi->sh->interference_mode_2G = WLAN_AUTO;
		pi->sh->interference_mode = WLAN_AUTO;
	}

	/* register watchdog fn */
	if (phy_wd_add_fn(pi->wdi, phy_lcn_noise_aci_wd, info,
	                  PHY_WD_PRD_1TICK, PHY_WD_1TICK_NOISE_ACI,
	                  PHY_WD_FLAG_NONE) != BCME_OK) {
		PHY_ERROR(("%s: phy_wd_add_fn failed\n", __FUNCTION__));
		goto fail;
	}

	/* Register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.mode = phy_lcn_noise_set_mode;
	fns.start = phy_lcn_noise_start;
	fns.stop = phy_lcn_noise_stop;
	fns.ctx = info;

	phy_noise_register_impl(nxi, &fns);

	return info;

	/* error handling */
fail:
	if (info != NULL)
		phy_mfree(pi, info, sizeof(phy_lcn_noise_info_t));
	return NULL;
}

void
BCMATTACHFN(phy_lcn_noise_unregister_impl)(phy_lcn_noise_info_t *info)
{
	phy_info_t *pi = info->pi;
	phy_noise_info_t *nxi = info->nxi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* unregister from common */
	phy_noise_unregister_impl(nxi);

	phy_mfree(pi, info, sizeof(phy_lcn_noise_info_t));
}

/* start/stop noise measure */
static void
phy_lcn_noise_set_mode(phy_type_noise_ctx_t *ctx, int wanted_mode, bool init)
{
	phy_lcn_noise_info_t *info = (phy_lcn_noise_info_t *)ctx;
	phy_info_t *pi = info->pi;

	PHY_TRACE(("%s: mode %d init %d\n", __FUNCTION__, wanted_mode, init));

	if (init) {
		pi->interference_mode_crs_time = 0;
		pi->crsglitch_prev = 0;

		if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
			wlc_lcnphy_aci_init(pi);
		}
	}

	/* NPHY 5G, supported for NON_WLAN and INTERFERE_NONE only */
	if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
		if (wanted_mode == INTERFERE_NONE) {	/* disable */
			switch (pi->cur_interference_mode) {
			case WLAN_AUTO:
			case WLAN_AUTO_W_NOISE:
			case WLAN_MANUAL:
				if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
					wlc_lcnphy_force_adj_gain(pi, FALSE, wanted_mode);
				}
				pi->aci_state &= ~ACI_ACTIVE;
				break;
			case NON_WLAN:
				if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
					pi->aci_state &= ~ACI_ACTIVE;
					wlc_lcnphy_force_adj_gain(pi, FALSE, wanted_mode);
				} else {
					pi->interference_mode_crs = 0;

				}
				break;
			}
		} else {	/* Enable */
			switch (wanted_mode) {
			case NON_WLAN:
				if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
					wlc_lcnphy_force_adj_gain(pi, TRUE, wanted_mode);
				} else {
					pi->interference_mode_crs = 1;
				}
				break;
			case WLAN_AUTO:
			case WLAN_AUTO_W_NOISE:
				/* fall through */
				if (((pi->aci_state & ACI_ACTIVE) != 0))
					break;
				if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
					wlc_lcnphy_aci(pi, FALSE);
					break;
				}
				/* FALLTHRU */
			case WLAN_MANUAL:
				if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
					wlc_lcnphy_force_adj_gain(pi, TRUE, wanted_mode);
				}
			}
		}
	}
}

static bool
phy_lcn_noise_aci_wd(phy_wd_ctx_t *ctx)
{
	phy_lcn_noise_info_t *ii = (phy_lcn_noise_info_t *)ctx;
	phy_info_t *pi = ii->pi;

	/* defer interference checking, scan and update if RM is progress */
	if (!SCAN_RM_IN_PROGRESS(pi) &&
	    (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		wlc_phy_aci_upd(pi);
	}

	return TRUE;
}

static bool
phy_lcn_noise_start(phy_type_noise_ctx_t *ctx, uint8 reason)
{
	phy_lcn_noise_info_t *nxi = (phy_lcn_noise_info_t *)ctx;
	phy_info_t *pi = nxi->pi;

	PHY_TRACE(("%s: state %d reason %d\n",
	           __FUNCTION__, pi->phynoise_state, reason));

	/* Trigger noise cal but don't adjust anything */
	wlc_lcnphy_noise_measure_start(pi, FALSE);

	/* interrupt scheduled */
	return TRUE;
}

static void
phy_lcn_noise_stop(phy_type_noise_ctx_t *ctx)
{
	phy_lcn_noise_info_t *nxi = (phy_lcn_noise_info_t *)ctx;
	phy_info_t *pi = nxi->pi;

	PHY_TRACE(("%s\n", __FUNCTION__));

	wlc_lcnphy_noise_measure_stop(pi);
}
