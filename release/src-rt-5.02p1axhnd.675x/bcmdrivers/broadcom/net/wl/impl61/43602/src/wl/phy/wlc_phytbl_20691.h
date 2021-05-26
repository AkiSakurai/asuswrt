/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THE CONTENTS OF THIS FILE IS TEMPORARY.
 * Eventually it'll be auto-generated.
 *
 * Copyright(c) 2012 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id$
 */

#ifndef _WLC_PHYTBL_20691_H_
#define _WLC_PHYTBL_20691_H_

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>

#include "wlc_phy_int.h"

/*
 * Channel Info table for the 2069 rev 1 (4345 A0).
 */

typedef struct _chan_info_radio20691 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RF_pll_vcocal4;
	uint16 RF_pll_vcocal6;
	uint16 RF_pll_vcocal2;
	uint16 RF_pll_vcocal1;
	uint16 RF_pll_vcocal11;
	uint16 RF_pll_vcocal12;
	uint16 RF_pll_frct2;
	uint16 RF_pll_frct3;
	uint16 RF_pll_vcocal10;
	uint16 RF_pll_xtal3;
	uint16 RF_pll_vco2;
	uint16 RF_logen_cfg2;
	uint16 RF_pll_vco3;
	uint16 RF_pll_xtalldo1;
	uint16 RF_pll_hvldo1;
	uint16 RF_pll_hvldo2;
	uint16 RF_pll_vco6;
	uint16 RF_pll_vco5;
	uint16 RF_pll_vco4;
	uint16 RF_pll_lf4;
	uint16 RF_pll_lf5;
	uint16 RF_pll_lf7;
	uint16 RF_pll_lf2;
	uint16 RF_pll_lf3;
	uint16 RF_pll_cp4;
	uint16 RF_pll_lf6;
	uint16 RF_logen_cfg1;
	uint16 RF_lna2g_tune;
	uint16 RF_txmix2g_cfg5;
	uint16 RF_pa2g_cfg2;
	uint16 RF_lna5g_tune;
	uint16 RF_txmix5g_cfg6;
	uint16 RF_pa5g_cfg4;
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio20691_t;

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
#if defined(DBG_PHY_IOV) || defined(BCMDBG_PHYDUMP)
extern radio_20xx_dumpregs_t dumpregs_20691_rev18[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev27[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev30[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev31[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev32[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev48[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev50[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev68[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev75[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev79[];
extern radio_20xx_dumpregs_t dumpregs_20691_rev82[];
#endif // endif
#endif	/* BCMDBG || BCMDBG_DUMP */

/* Radio referred values tables */
extern radio_20xx_prefregs_t prefregs_20691_rev18[];
extern radio_20xx_prefregs_t prefregs_20691_rev27[];
extern radio_20xx_prefregs_t prefregs_20691_rev30[];
extern radio_20xx_prefregs_t prefregs_20691_rev31[];
extern radio_20xx_prefregs_t prefregs_20691_rev32[];
extern radio_20xx_prefregs_t prefregs_20691_rev48[];
extern radio_20xx_prefregs_t prefregs_20691_rev50[];
extern radio_20xx_prefregs_t prefregs_20691_rev68[];
extern radio_20xx_prefregs_t prefregs_20691_rev75[];
extern radio_20xx_prefregs_t prefregs_20691_rev79[];
extern radio_20xx_prefregs_t prefregs_20691_rev82[];

/* For 2g ipa only, to be removed after code addition */
extern uint16 acphy_radiogainqdb_20691_majrev0[128];

/* Radio tuning values tables */
extern chan_info_radio20691_t chan_tuning_20691_rev18[77];
extern chan_info_radio20691_t chan_tuning_20691_rev27[77];
extern chan_info_radio20691_t chan_tuning_20691_rev30[77];
extern chan_info_radio20691_t chan_tuning_20691_rev31[77];
extern chan_info_radio20691_t chan_tuning_20691_rev32[77];
extern chan_info_radio20691_t chan_tuning_20691_rev48[77];
extern chan_info_radio20691_t chan_tuning_20691_rev50[77];
extern chan_info_radio20691_t chan_tuning_20691_rev68[77];
extern chan_info_radio20691_t chan_tuning_20691_rev75[77];
extern chan_info_radio20691_t chan_tuning_20691_rev79[77];
extern chan_info_radio20691_t chan_tuning_20691_rev82[77];

#endif	/* _WLC_PHYTBL_20691_H_ */
