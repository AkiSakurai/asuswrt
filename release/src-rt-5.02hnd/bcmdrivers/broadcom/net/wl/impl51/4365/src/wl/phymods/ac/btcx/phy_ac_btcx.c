/*
 * ACPHY BT Coex module implementation
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
#include <phy_dbg.h>
#include <phy_mem.h>
#include "phy_type_btcx.h"
#include <phy_ac.h>
#include <phy_ac_btcx.h>

/* ************************ */
/* Modules used by this module */
/* ************************ */
#include <wlc_phyreg_ac.h>
#include <wlc_phy_int.h>

#include <siutils.h>
#include <sbchipc.h>
#include <phy_utils_reg.h>
#include <phy_ac_info.h>


/* module private states */
struct phy_ac_btcx_info {
	phy_info_t			*pi;
	phy_ac_info_t		*aci;
	phy_btcx_info_t		*cmn_info;

/* add other variable size variables here at the end */
};

/* local functions */

/* register phy type specific implementation */
phy_ac_btcx_info_t *
BCMATTACHFN(phy_ac_btcx_register_impl)(phy_info_t *pi, phy_ac_info_t *aci,
	phy_btcx_info_t *cmn_info)
{
	phy_ac_btcx_info_t *ac_info;
	phy_type_btcx_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage together */
	if ((ac_info = phy_malloc(pi, sizeof(phy_ac_btcx_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}
	ac_info->pi = pi;
	ac_info->aci = aci;
	ac_info->cmn_info = cmn_info;

	/* register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.ctx = ac_info;

	if (phy_btcx_register_impl(cmn_info, &fns) != BCME_OK) {
		PHY_ERROR(("%s: phy_btcx_register_impl failed\n", __FUNCTION__));
		goto fail;
	}

	/* PHY-Feature specific parameter initialization */


	return ac_info;

	/* error handling */
fail:
	if (ac_info != NULL)
		phy_mfree(pi, ac_info, sizeof(phy_ac_btcx_info_t));
	return NULL;
}

void
BCMATTACHFN(phy_ac_btcx_unregister_impl)(phy_ac_btcx_info_t *ac_info)
{
	phy_info_t *pi;
	phy_btcx_info_t *cmn_info;

	ASSERT(ac_info);
	pi = ac_info->pi;
	cmn_info = ac_info->cmn_info;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* unregister from common */
	phy_btcx_unregister_impl(cmn_info);

	phy_mfree(pi, ac_info, sizeof(phy_ac_btcx_info_t));
}

/* ********************************************* */
/*				External Definitions					*/
/* ********************************************* */

void
wlc_phy_set_bt_on_core1_acphy(phy_info_t *pi, uint8 bt_fem_val, uint16 gpioen)
{
	/* *** NOTE : For boards with BT on sharead antenna, update code in
	   wlc_bmac_set_ctrl_bt_shd0() so that in down mode BT has control of fem
	   Also, BT needs control when insmod (but not up), in that case wlc_phy_ac.c
	   is not even called, and so need to have some code in wlc_bmac.
	*/

	/* chip_bandsel = bandsel */
	MOD_PHYREG(pi, BT_SwControl, bt_sharing_en, 1);
	/* Bring c1_2g ctrls on gpio/srmclk */
	WRITE_PHYREG(pi, shFemMuxCtrl, 0x555);

	/* Setup chipcontrol (chipc[3] for acphy_gpios) */
	si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
		CCTRL4360_BTSWCTRL_MODE, CCTRL4360_BTSWCTRL_MODE);

	/* point fem_bt to chip_bt control line */
	MOD_PHYREG(pi, BT_FemControl, bt_fem, bt_fem_val);

	if (ACREV_IS(pi->pubpi.phy_rev, 0)) {
		/* PHY controls bits 5,6,7 of gpio for BT boards (only needed for A0) */
		si_gpiocontrol(pi->sh->sih, 0xffff, 0x00e0, GPIO_DRV_PRIORITY);

		/* acphy_gpios = mux(bt_fem, femctrl[7:4]) */
		WRITE_PHYREG(pi, gpioSel, 0xb);
		/* bt control lines in gpio 5,6,7 */
		WRITE_PHYREG(pi, gpioLoOutEn, 0xe0);

		pi->u.pi_acphy->poll_adc_WAR = TRUE;

		/* 4360A0 : Force in WLAN mode, as A0 does not have inv_btcx_prisel bit,
		   and we have to change top level MAC definition of prisel (too complicated)
		   We are not supporting BT on 4360A0 anyway
		*/
		MOD_PHYREG(pi, BT_FemControl, bt_en, 0);
		MOD_PHYREG(pi, BT_FemControl, bt_en_ovrd, 1);
	} else {
		pi->u.pi_acphy->poll_adc_WAR = FALSE;

		/* bt_prisel is active low */
		MOD_PHYREG(pi, BT_SwControl, inv_btcx_prisel, 1);
	}

	/* In wlan Off/sleep mode, Make BT as input, and driver others as 0 */
	si_gpioout(pi->sh->sih, gpioen, 0, GPIO_DRV_PRIORITY);
	si_gpioouten(pi->sh->sih, gpioen, gpioen, GPIO_DRV_PRIORITY);
	si_gpiocontrol(pi->sh->sih, gpioen, 0, GPIO_DRV_PRIORITY);
}

void
wlc_phy_bt_on_gpio4_acphy(phy_info_t *pi)
{
	uint16 mask = 0x10;    /* gpio 4 = 0 */

	/* Force gpio4 to be 0 */
	si_gpioout(pi->sh->sih, (1 << 4), (0 << 4), GPIO_DRV_PRIORITY);
	si_gpioouten(pi->sh->sih, (1 << 4), (1 << 4), GPIO_DRV_PRIORITY);

	/* Take away gpio4 contorl from phy */
	si_gpiocontrol(pi->sh->sih, mask, 0, GPIO_DRV_PRIORITY);
}

void
wlc_phy_set_femctrl_bt_wlan_ovrd_acphy(phy_info_t *pi, int8 state)
{
	wlapi_suspend_mac_and_wait(pi->sh->physhim);

	if (state == ON) {
		MOD_PHYREG(pi, BT_FemControl, bt_en, 1);
		MOD_PHYREG(pi, BT_FemControl, bt_en_ovrd, 1);
	} else if (state == OFF) {
		MOD_PHYREG(pi, BT_FemControl, bt_en, 0);
		MOD_PHYREG(pi, BT_FemControl, bt_en_ovrd, 1);
	} else {
		MOD_PHYREG(pi, BT_FemControl, bt_en_ovrd, 0);
		MOD_PHYREG(pi, BT_FemControl, bt_en, 0);
	}

	wlapi_enable_mac(pi->sh->physhim);
}

int8
wlc_phy_get_femctrl_bt_wlan_ovrd_acphy(phy_info_t *pi)
{
	int8 state;
	uint8 bten, bten_ovrd;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);

	bten = READ_PHYREGFLD(pi, BT_FemControl, bt_en);
	bten_ovrd = READ_PHYREGFLD(pi, BT_FemControl, bt_en_ovrd);

	state = (bten_ovrd == 0) ? AUTO : (bten == 0) ? OFF : ON;

	wlapi_enable_mac(pi->sh->physhim);
	return state;
}
