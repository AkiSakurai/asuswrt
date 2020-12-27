/*
 * ACPHY Gain Table loading specific portion of Broadcom BCM43XX 802.11abgn
 * Networking Device Driver.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_ac_gains.c 462924 2014-03-19 06:28:19Z $
 */

#include <wlc_cfg.h>

#if ACCONF != 0
#include <typedefs.h>
#include <osl.h>
#include <bcmwifi_channels.h>

#include "wlc_phy_types.h"
#include "wlc_phy_int.h"
#include "wlc_phyreg_ac.h"
#include "wlc_phytbl_ac.h"
#include "wlc_phytbl_20691.h"
#include "wlc_phytbl_ac_gains.h"
#include "wlc_phy_ac_gains.h"

static void
wlc_phy_gaintbl_blanking(phy_info_t *pi, uint16 *tx_pwrctrl_tbl, uint8 txidxcap)
{
	uint16 k, m, K;
	/* ACPHY has 48bit gaincode = 3 16-bit word */
	uint16 nword_per_gaincode = 3;

	K = txidxcap & 0xFF;

	for (k = 0; k < K; k++) {
		for (m = 0; m < nword_per_gaincode; m++) {
			tx_pwrctrl_tbl[k*nword_per_gaincode + m] =
			        tx_pwrctrl_tbl[K*nword_per_gaincode + m];
		}
	}
}

static uint16 *
wlc_phy_get_tx_pwrctrl_tbl_2069(phy_info_t *pi)
{
	uint16 *tx_pwrctrl_tbl = NULL;

	if (CHSPEC_IS2G(pi->radio_chanspec)) {

		tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev0;

		if (PHY_IPA(pi)) {

			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev16;
				break;
			case 17:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev17;
				break;
			case 23: /* iPa  case TXGain tables */
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev17;
				break;
			case 25: /* iPa  case TXGain tables */
				if (pi->xtalfreq == 40000000) /* Special table for 43162zp */
					tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev25_bcm43162;
				else
					tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev25;
				break;
			case 18:
			case 24:
			case 26:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev18;
				break;
			case 32:
			case 33:
			case 34:
			case 35:
			case 37:
			case 38:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev33_37;
				break;
			case 39:
			case 40:
			case 41:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev39;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev0;
				break;

			}
		} else {

			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 17:
			case 23:
			case 25:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev17;
				if (pi->u.pi_acphy->srom.txgaintbl2g_blank) {
					wlc_phy_gaintbl_blanking(pi, tx_pwrctrl_tbl,
					                         pi->sh->txidxcap2g);
				}
				break;
			case 18:
			case 24:
			case 26:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev18;
				break;
			case 4:
			case 8:
			case 7: /* 43602a0 uses radio rev4 tx pwr ctrl tables */
				switch (pi->u.pi_acphy->srom.txgaintbl_id) {
				case 0:
					tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev4;
					break;
				case 1:
					tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev4_id1;
					break;
				default:
					tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev4;
					break;
				}
				break;
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev16;
				break;
			case 32:
			case 33:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev33_35_36_37;
				break;
			case 34:
			case 36:
			case 42:
			case 43:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev34;
				break;
			case 35:
			case 37:
			case 38:
			case 39:
			case 40:
			case 41:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev33_35_36_37;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev0;
				break;
			}
		}
	} else {
		if (PHY_IPA(pi)) {
			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 17:
			case 23:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev17;
				break;
			case 25:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev25;
				break;
			case 18:
			case 24:
			case 26:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev18;
				break;
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev16;
				break;
			case 32:
			case 33:
			case 34:
			case 35:
			case 37:
			case 38:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev33_37;
				break;
			case 39:
			case 40:
			case 41:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev39;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev0;
				break;
			}
		} else {

			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 17:
			case 23:
			case 25:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev17;
				if (pi->u.pi_acphy->srom.txgaintbl5g_blank) {
					wlc_phy_gaintbl_blanking(pi, tx_pwrctrl_tbl,
					                         pi->sh->txidxcap5g);
				}
				break;
			case 18:
			case 24:
			case 26:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev18;
				break;
			case 4:
			case 8:
			case 7: /* 43602a0 uses radio rev4 tx pwr ctrl tables */
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev4;
				break;
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev16;
				break;
			case 32:
			case 33:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev33_35_36;
				break;
			case 34:
			case 36:
			case 42:
			case 43:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev34;
				break;
			case 35:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev33_35_36;
				break;
			case 37:
			case 38:
			case 39:
			case 40:
			case 41:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev37_38;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev0;
				break;

			}
		}
	}

	return tx_pwrctrl_tbl;
}

void
BCMATTACHFN(wlc_phy_set_txgain_tbls)(phy_info_t *pi)
{
	const uint16 *tx_pwrctrl_tbl = NULL;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	ASSERT(pi_ac->gaintbl_2g == NULL);
	if ((pi_ac->gaintbl_2g = MALLOC(pi->sh->osh,
		sizeof(uint16) * TXGAIN_TABLES_LEN)) == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	if (PHY_IPA(pi)) {
		tx_pwrctrl_tbl = (pi->sh->extpagain2g != 0) ?
			txgain_tiny_phyrev7_2g_ipa : txgain_tiny_phyrev7_ipa_2g_for_epa;
	} else {
		/* Same for now. 4345A0_YA1 epa */
		tx_pwrctrl_tbl = txgain_tiny_phyrev7_2g_epa;
	}

	/* Copy the table to the new location. */
	bcopy(tx_pwrctrl_tbl, pi_ac->gaintbl_2g, sizeof(uint16) * TXGAIN_TABLES_LEN);

#ifdef BAND5G
	ASSERT(pi_ac->gaintbl_5g == NULL);
	if ((pi_ac->gaintbl_5g = MALLOC(pi->sh->osh,
		sizeof(uint16) * TXGAIN_TABLES_LEN)) == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	if (ACREV_IS(pi->pubpi.phy_rev, 4)) {
		if (PHY_IPA(pi)) {
			tx_pwrctrl_tbl = txgain_tiny_phyrev4_5g_ipa;
		} else {
			tx_pwrctrl_tbl = txgain_tiny_phyrev4_5g_epa;
		}
	} else {
		if (PHY_IPA(pi)) {
			tx_pwrctrl_tbl = txgain_tiny_phyrev7_5g_ipa;
		} else {
			tx_pwrctrl_tbl = txgain_tiny_phyrev7_5g_epa;
			if (pi->txgaintbl5g == 1) {
				tx_pwrctrl_tbl = txgain_tiny_phyrev7_5g_1_epa;
			}
		}
	}

	/* Copy the table to the new location. */
	bcopy(tx_pwrctrl_tbl, pi_ac->gaintbl_5g, sizeof(uint16) * TXGAIN_TABLES_LEN);
#else
	/* Set the pointer to null if 5G is not defined. */
	pi_ac->gaintbl_5g = NULL;
#endif /* BAND5G */
}

static const uint16 *
wlc_phy_get_tiny_txgain_tbl(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	ASSERT(RADIOID(pi->pubpi.radioid) == BCM20691_ID);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		return pi_ac->gaintbl_2g;
	}
#ifdef BAND5G
	else if (CHSPEC_IS5G(pi->radio_chanspec)) {
		return pi_ac->gaintbl_5g;
	}
#endif
	return NULL;
}

void
wlc_phy_ac_gains_load(phy_info_t *pi)
{
	uint idx;
	const uint16 *tx_pwrctrl_tbl;
	uint16 GainWord_Tbl[3];
	uint8 Gainoverwrite = 0;
	uint8 PAgain = 0xff;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* Load tx gain table */
	tx_pwrctrl_tbl = (RADIOID(pi->pubpi.radioid) == BCM2069_ID)
				? wlc_phy_get_tx_pwrctrl_tbl_2069(pi)
				: wlc_phy_get_tiny_txgain_tbl(pi);

	ASSERT(tx_pwrctrl_tbl != NULL);


	if (PHY_IPA(pi)) {
		Gainoverwrite = (CHSPEC_IS2G(pi->radio_chanspec)) ?
			pi->u.pi_acphy->srom_pagc2g_ovr :
			pi->u.pi_acphy->srom_pagc5g_ovr;
		PAgain = (CHSPEC_IS2G(pi->radio_chanspec)) ?
			pi->u.pi_acphy->srom_pagc2g :
			pi->u.pi_acphy->srom_pagc5g;
	}

	if (Gainoverwrite > 0) {
		for (idx = 0; idx < 128; idx++) {
			GainWord_Tbl[0] = tx_pwrctrl_tbl[3*idx];
			GainWord_Tbl[1] = (tx_pwrctrl_tbl[3*idx+1] & 0xff00) + (0xff & PAgain);

			GainWord_Tbl[2] = tx_pwrctrl_tbl[3*idx+2];
			wlc_phy_table_write_acphy(pi,
				ACPHY_TBL_ID_GAINCTRLBBMULTLUTS, 1, idx, 48, GainWord_Tbl);
		}
	} else {
		wlc_phy_table_write_acphy(pi,
			ACPHY_TBL_ID_GAINCTRLBBMULTLUTS, 128, 0, 48, tx_pwrctrl_tbl);
	}

	if (TINY_RADIO(pi)) {
		uint16 k, txgaintemp[3];
		uint16 txidxcap = 0;

		if (CHSPEC_IS2G(pi->radio_chanspec) && pi->u.pi_acphy->srom.txgaintbl2g_blank) {
				txidxcap = pi->sh->txidxcap2g & 0xFF;
		}
		if (CHSPEC_IS5G(pi->radio_chanspec) && pi->u.pi_acphy->srom.txgaintbl5g_blank) {
				txidxcap = pi->sh->txidxcap5g & 0xFF;
		}
		if (txidxcap != 0) {
			txgaintemp[0] = tx_pwrctrl_tbl[3*txidxcap];
			txgaintemp[1] = tx_pwrctrl_tbl[3*txidxcap+1];
			txgaintemp[2] = tx_pwrctrl_tbl[3*txidxcap+2];
			for (k = 0; k < txidxcap; k++) {
				wlc_phy_table_write_acphy(pi,
				ACPHY_TBL_ID_GAINCTRLBBMULTLUTS, 1, k, 48, txgaintemp);
			}
		}
	}
}

void
wlc_phy_ac_delete_gain_tbl(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	if (pi_ac->gaintbl_2g) {
		MFREE(pi->sh->osh, (void *)pi_ac->gaintbl_2g,
			sizeof(uint16)*TXGAIN_TABLES_LEN);
		pi_ac->gaintbl_2g = NULL;
	}

#ifdef BAND5G
	if (pi_ac->gaintbl_5g) {
		MFREE(pi->sh->osh, (void *)pi_ac->gaintbl_5g,
			sizeof(uint16)*TXGAIN_TABLES_LEN);
		pi_ac->gaintbl_5g = NULL;
	}
#endif

}

#endif /* ACCONF != 0 */
