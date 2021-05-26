/*
 * dccal module implementation
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
 * $Id: phy_ac_dccal.c 786644 2020-05-04 11:37:46Z $
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_btcx.h>
#include <phy_dccal.h>
#include <phy_rstr.h>
#include "phy_type_dccal.h"
#include <phy_ac_dccal.h>
#include <phy_ac_btcx.h>
#include <phy_ac_info.h>
#include <phy_utils_reg.h>
#include <phy_utils_var.h>
#include <phy_stf.h>
#include <phy_misc_api.h>

#ifndef NEW_PHY_CAL_ARCH
#include "wlc_radioreg_20691.h"
#include "wlc_radioreg_20693.h"
#include "wlc_radioreg_20694.h"
#include "wlc_radioreg_20698.h"
#include "wlc_radioreg_20704.h"
#include "wlc_radioreg_20707.h"
#include "wlc_radioreg_20708.h"
#include "wlc_radioreg_20709.h"
#include "wlc_radioreg_20710.h"

#include "wlc_phyreg_ac.h"
#endif /* NEW_PHY_CAL_ARCH */

typedef struct {
	int32 i_accum;
	int32 q_accum;
} phy_hpf_dc_est_t;

/* module private states */
struct phy_ac_dccal_info {
	phy_info_t *pi;
	phy_ac_info_t *aci;
	phy_dccal_info_t *dccali;
	int16 idac_min_i;  /* DCC sw-cal - i-rail minimum idac index */
	int16 idac_min_q;  /* DCC sw-cal - q-rail minimum idac index */
	uint32 dcoe_table[4][36];
	uint32 idacc_table[4][24];
};

#define DEFAULT_TIA_FOR_IDACC 6

/* local functions */
static void wlc_phy_get_rx_hpf_dc_est_acphy(phy_info_t *pi, phy_hpf_dc_est_t *hpf_iqdc,
	uint16 nump_samps);
static void wlc_rx_digi_dccomp_set(phy_info_t *pi, int16 i, int16 q, uint8 core);
static void phy_ac_dccal_3steps(phy_info_t *pi);

/* Register/unregister ACPHY specific implementation to common layer. */
phy_ac_dccal_info_t *
BCMATTACHFN(phy_ac_dccal_register_impl)(phy_info_t *pi, phy_ac_info_t *aci,
	phy_dccal_info_t *dccali)
{
	phy_ac_dccal_info_t *info;
	phy_type_dccal_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage in once */
	if ((info = phy_malloc(pi, sizeof(phy_ac_dccal_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}

	info->pi = pi;
	info->aci = aci;
	info->dccali = dccali;

	/* Register PHY type specific implementation */
	bzero(&fns, sizeof(fns));
	fns.ctx = info;

	phy_dccal_register_impl(dccali, &fns);

	return info;
fail:
	if (info != NULL)
		phy_mfree(pi, info, sizeof(phy_ac_dccal_info_t));
	return NULL;
}

void
BCMATTACHFN(phy_ac_dccal_unregister_impl)(phy_ac_dccal_info_t *info)
{
	phy_info_t *pi;
	phy_dccal_info_t *dccali;

	ASSERT(info);
	pi = info->pi;
	dccali = info->dccali;

	PHY_TRACE(("%s\n", __FUNCTION__));

	phy_dccal_unregister_impl(dccali);

	phy_mfree(pi, info, sizeof(phy_ac_dccal_info_t));
}

/* NB returns SUM not AVERAGE of hpf dc est */
/* Based on: proc acphy_get_hpf_dc_est { {averages 10} } */

static void
wlc_phy_get_rx_hpf_dc_est_acphy(phy_info_t *pi, phy_hpf_dc_est_t *hpf_iqdc, uint16 nump_samps)
{
	uint8 core;
	uint16 i;

	FOREACH_CORE(pi, core) {
		hpf_iqdc[core].i_accum = 0;
		hpf_iqdc[core].q_accum = 0;
	}
	for (i = 0; i < nump_samps; i++) {
		FOREACH_CORE(pi, core) {
			hpf_iqdc[core].i_accum += (int16)READ_PHYREGCE(pi, DCestimateI, core);
			hpf_iqdc[core].q_accum += (int16)READ_PHYREGCE(pi, DCestimateQ, core);
		}
	}

#if (!defined(WL_SISOCHIP) && defined(SWCTRL_TO_BT_IN_COEX))
	phy_ac_btcx_reset_swctrl(pi->u.pi_acphy->btcxi);
#endif // endif
}

int
acphy_analog_dc_cal_war(phy_info_t *pi)
{
	uint8  core;
	uint8  count;
	int16  inext[PHY_CORE_MAX];
	int16  qnext[PHY_CORE_MAX];
	/* -255 is the limit, since DC is low, limiting search to -120 */
	int16  DAC_MIN;
	int16  DAC_MAX;
	int8   DAC_STEP =  2;
	phy_hpf_dc_est_t hpf_iqdc[PHY_CORE_MAX];

	/* return BCME_OK; */

	if (ACMAJORREV_37(pi->pubpi->phy_rev) ||
		(ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
		!ACMAJORREV_128(pi->pubpi->phy_rev))) {
		DAC_MIN = -120;
		DAC_MAX = 120;
	} else {
		DAC_MIN = -255;
		DAC_MAX = 255; /* same reasoning as above */
	}

	/* Disbale the analofg DCC */
	MOD_PHYREG(pi, RfseqTrigger, en_pkt_proc_dcc_ctrl, 0);
	FOREACH_CORE(pi, core) {
		/* biasadj parameter - controls the LSB */
		if (CHSPEC_IS2G(pi->radio_chanspec))
			MOD_RADIO_REG_TINY(pi, TIA_CFG8, core, tia_offset_dac_biasadj, 2);
		else
			MOD_RADIO_REG_TINY(pi, TIA_CFG8, core, tia_offset_dac_biasadj, 4);
	}

	/* Analog IDAC correction part */
	FOREACH_CORE(pi, core) {
		/* find the idac settings that give minimum via rx_iq_est */
		acphy_search_idac_iq_est(pi, core, DAC_MIN, DAC_MAX, DAC_STEP);
		/* Update the idac registers with the settings found above */
		/* i-rail */
		acphy_dcc_idac_set(pi, pi->u.pi_acphy->dccali->idac_min_i, 0, core);
		/* q-rail */
		acphy_dcc_idac_set(pi, pi->u.pi_acphy->dccali->idac_min_q, 1, core);
	}

	/* Digital correction part - After NSF and before Farrow */

	FOREACH_CORE(pi, core) {
		/* begin with '0' for dig_i and digi_q */
		inext[core] = 0;
		qnext[core] = 0;
		wlc_rx_digi_dccomp_set(pi, inext[core], qnext[core], core);
	}

	for (count = 0; count < 3; count++) {

		wlc_phy_get_rx_hpf_dc_est_acphy(pi, hpf_iqdc, 17);

		FOREACH_CORE(pi, core) {
			hpf_iqdc[core].i_accum += (1 << 13);
			hpf_iqdc[core].q_accum += (1 << 13);
			inext[core] = inext[core] - (int16)(hpf_iqdc[core].i_accum >> 14);
			qnext[core] = qnext[core] - (int16)(hpf_iqdc[core].q_accum >> 14);
		}
	}

	FOREACH_CORE(pi, core) {
		if (READ_PHYREGFLDCXE(pi, RxFeCtrl1, swap_iq, core))
			wlc_rx_digi_dccomp_set(pi, inext[core], qnext[core], core);
		else
			wlc_rx_digi_dccomp_set(pi, qnext[core], inext[core], core);

#ifdef BCMDBG
		printf("core[%d]: static dc offset cal (%i, %i)\n",
		core, inext[core], qnext[core]);
#endif // endif
	}
	return BCME_OK;
}

void
acphy_dcc_idac_set(phy_info_t *pi, int16 dac, int ch, int core)
{
	/* override register */
	MOD_RADIO_REG_TINY(pi, RX_BB_2G_OVR_EAST, core, ovr_tia_offset_dac, 1);

	/* Override dcoc idac with desired values. */
	if (ch == 0) {
		MOD_RADIO_REG_TINY(pi, TIA_CFG8, core, tia_offset_dac_sign_i,
		                   (dac >= 0) ? 0 : 1);
		MOD_RADIO_REG_TINY(pi, TIA_CFG11, core, tia_offset_dac_d_i, ABS(dac));
	} else {
		MOD_RADIO_REG_TINY(pi, TIA_CFG8, core, tia_offset_dac_sign_q,
		                   (dac >= 0) ? 0 : 1);
		MOD_RADIO_REG_TINY(pi, TIA_CFG11, core, tia_offset_dac_d_q, ABS(dac));
	}
}

void
acphy_search_idac_iq_est(phy_info_t *pi, uint8 core,
                         int16 dac_min, int16 dac_max, int8 dac_step)
{
	int dac_settle = 10;
	phy_iq_est_t iq_ii_qq[PHY_CORE_MAX];
	int16 dac;
	uint32 min_power_i = 100000000U;
	uint32 min_power_q = 100000000U;
	uint32 power_i;
	uint32 power_q;

	pi->u.pi_acphy->dccali->idac_min_i = 0;
	pi->u.pi_acphy->dccali->idac_min_q = 0;

	MOD_PHYREG(pi, DcFiltAddress, dcBypass, 1);
	wlc_rx_digi_dccomp_set(pi, 0, 0, core);
	for (dac = dac_min; dac <= dac_max; dac += dac_step) {
		acphy_dcc_idac_set(pi, dac, 0, core);
		acphy_dcc_idac_set(pi, dac, 1, core);

		OSL_DELAY(dac_settle);

		wlc_phy_rx_iq_est_acphy(pi, iq_ii_qq, 0x4000, 32, 0, 0);

		if (READ_PHYREGFLDCXE(pi, RxFeCtrl1, swap_iq, core) == 1) {
			power_i =  iq_ii_qq[core].q_pwr;
			power_q =  iq_ii_qq[core].i_pwr;
		} else {
			power_i =  iq_ii_qq[core].i_pwr;
			power_q =  iq_ii_qq[core].q_pwr;
		}

		if (power_i < min_power_i) {
			pi->u.pi_acphy->dccali->idac_min_i = dac;
			min_power_i = power_i;
		}

		if (power_q < min_power_q) {
			pi->u.pi_acphy->dccali->idac_min_q = dac;
			min_power_q = power_q;
		}
	}
	MOD_PHYREG(pi, DcFiltAddress, dcBypass, 0);

#ifdef BCMDBG
	printf("Core%d - DAC_min_I %d\n", core, pi->u.pi_acphy->dccali->idac_min_i);
	printf("Core%d - DAC_min_Q %d\n", core, pi->u.pi_acphy->dccali->idac_min_q);
#endif // endif
}

/*
 * Based on:
 *  proc acphy_digi_dccomp_minsearch { {DDCC_MIN -32} {DDCC_MAX 31} {DDCC_STEP 1} {use_hpf_est 1} }
 *  proc acphy_tiny_static_dc_offset_cal { }
 */
int
wlc_phy_tiny_static_dc_offset_cal(phy_info_t *pi)
{
	/* Use HPF DC estimate to find optimum static DC offset setting */

	int16 inext[PHY_CORE_MAX];
	int16 qnext[PHY_CORE_MAX];
	uint16 sparereg = 0;

	phy_hpf_dc_est_t hpf_iqdc[PHY_CORE_MAX];

	uint8 core;
	uint8 count;
	uint8 swap_saved[PHY_CORE_MAX] = {0};
	uint8 ovr[PHY_CORE_MAX] = {0};
	uint8 pu[PHY_CORE_MAX] = {0};
	uint8 idacbias[PHY_CORE_MAX] = {0};
	uint8 force_rshift;
	uint8 rx_farrow_rshift;

#ifdef ATE_BUILD
		printf("===> Running static dc offset cal\n");
#endif /* ATE_BUILD */

	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, TRUE);

	FOREACH_CORE(pi, core) {
		/* set LNA off */
		/* save config */
		if (CHSPEC_ISPHY5G6G(pi->radio_chanspec)) {
			ovr[core] = READ_RADIO_REGFLD_TINY(pi, RX_TOP_5G_OVR, core,
				ovr_lna5g_lna1_pu);
			pu[core] = READ_RADIO_REGFLD_TINY(pi, LNA5G_CFG1, core, lna5g_lna1_pu);

			if ((!ACMAJORREV_4(pi->pubpi->phy_rev)) &&
			    (!ACMAJORREV_32(pi->pubpi->phy_rev)) &&
			    (!ACMAJORREV_33(pi->pubpi->phy_rev))) {
				MOD_RADIO_REG_TINY(pi, RX_TOP_5G_OVR, core,
					ovr_lna5g_lna1_out_short_pu, 1);
				MOD_RADIO_REG_TINY(pi, LNA5G_CFG1, core,
					lna5g_lna1_out_short_pu, 1);
			}
			if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
				ACMAJORREV_33(pi->pubpi->phy_rev)) {
				MOD_RADIO_REG_TINY(pi, LNA5G_CFG1, core, lna5g_lna1_pu, 0);
				MOD_RADIO_REG_TINY(pi, RX_TOP_5G_OVR, core, ovr_lna5g_lna1_pu, 1);
			} else {
				MOD_RADIO_REG_TINY(pi, RX_TOP_5G_OVR, core, ovr_lna5g_lna1_pu, 1);
				MOD_RADIO_REG_TINY(pi, LNA5G_CFG1, core, lna5g_lna1_pu, 0);
			}
		} else {
			pu[core] = READ_RADIO_REGFLD_TINY(pi, LNA2G_CFG1, core, lna2g_lna1_pu);
			if (ACMAJORREV_4(pi->pubpi->phy_rev)) {
				ovr[core] = READ_RADIO_REGFLD_20693(pi, RX_TOP_2G_OVR_EAST2, core,
					ovr_lna2g_lna1_pu);
				MOD_RADIO_REG_20693(pi, RX_TOP_2G_OVR_EAST2, core,
					ovr_lna2g_lna1_pu, 1);
			} else if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
				ACMAJORREV_33(pi->pubpi->phy_rev)) {
				MOD_RADIO_REG_20693(pi, RX_TOP_2G_OVR_EAST2, core,
					ovr_lna2g_lna1_pu, 1);
				ovr[core] = READ_RADIO_REGFLD_20693(pi, RX_TOP_2G_OVR_EAST2, core,
					ovr_lna2g_lna1_pu);
			} else {
				ovr[core] = READ_RADIO_REGFLD_TINY(pi, RX_TOP_2G_OVR_NORTH, core,
					ovr_lna2g_lna1_pu);

				MOD_RADIO_REG_TINY(pi, RX_TOP_2G_OVR_NORTH, core,
					ovr_lna2g_lna1_out_short_pu, 1);
				MOD_RADIO_REG_TINY(pi, LNA2G_CFG1, core,
					lna2g_lna1_out_short_pu, 1);
				MOD_RADIO_REG_TINY(pi, RX_TOP_2G_OVR_NORTH, core,
					ovr_lna2g_lna1_pu, 1);
			}
			MOD_RADIO_REG_TINY(pi, LNA2G_CFG1, core, lna2g_lna1_pu, 0);
		}

		idacbias[core] = READ_RADIO_REGFLD_TINY(pi, TIA_CFG8, core,
			tia_offset_dac_biasadj);

		/* Minimise idac step size so that it does not contribute to DC offsets */
		MOD_RADIO_REG_TINY(pi, TIA_CFG8, core, tia_offset_dac_biasadj, 1);
	}

	/* Saving Farrow settings */
	force_rshift = READ_PHYREGFLD(pi, RxSdFeConfig1, farrow_rshift_force);
	rx_farrow_rshift = READ_PHYREGFLD(pi, RxSdFeConfig6, rx_farrow_rshift_0);

	ACPHY_REG_LIST_START
		/* Changing Farrow settings */
		MOD_PHYREG_ENTRY(pi, RxSdFeConfig1, farrow_rshift_force, 1)
		MOD_PHYREG_ENTRY(pi, RxSdFeConfig6, rx_farrow_rshift_0, 0)

		MOD_PHYREG_ENTRY(pi, RfseqTrigger, en_pkt_proc_dcc_ctrl, 0)
	ACPHY_REG_LIST_EXECUTE(pi);

	OSL_DELAY(1);	/* allow radio to settle */

	wlc_dcc_fsm_reset(pi);	/* Redo Coarse cal */

	if (ACMAJORREV_32(pi->pubpi->phy_rev) || ACMAJORREV_33(pi->pubpi->phy_rev))
		OSL_DELAY(1);	/* allow radio to settle */

	if (ACMAJORREV_4(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, rx_tia_dc_loop_0, dc_loop_hold, 1);
		if (phy_get_phymode(pi) != PHYMODE_RSDB)
			MOD_PHYREG(pi, rx_tia_dc_loop_1, dc_loop_hold, 1);
	} else if (ACMAJORREV_32(pi->pubpi->phy_rev) || ACMAJORREV_33(pi->pubpi->phy_rev)) {
		/* Loop through cores */
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_hold, 1);
		}
	} else {
		sparereg = READ_PHYREG(pi, SpareReg);
		WRITE_PHYREG(pi, SpareReg,  sparereg | 0x4000);	/* assert dcc fsm hold */
	}

	FOREACH_CORE(pi, core) {
		if (ACMAJORREV_32(pi->pubpi->phy_rev) || ACMAJORREV_33(pi->pubpi->phy_rev)) {
			/* Read back iq_swap values for all the cores */
			if (core == 0)
				swap_saved[core] = READ_PHYREGFLD(pi, RxFeCtrl1,  swap_iq0);
			else if (core == 1)
				swap_saved[core] = READ_PHYREGFLD(pi, RxFeCtrl1,  swap_iq1);
			else if (core == 2)
				swap_saved[core] = READ_PHYREGFLD(pi, RxFeCtrl1,  swap_iq2);
			else
				swap_saved[core] = READ_PHYREGFLD(pi, RxFeCtrl1,  swap_iq3);

			/* Force iq_swap to be '0' for all cores */
			if (core == 0)
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq0, 0);
			else if (core == 1)
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq1, 0);
			else if (core == 2)
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq2, 0);
			else
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq3, 0);

			inext[core] = 0;
			qnext[core] = 0;
		} else {
			swap_saved[core] = READ_PHYREGFLDCXE(pi, RxFeCtrl1, swap_iq, core);
			MOD_PHYREGCXE(pi, RxFeCtrl1, swap_iq, core, 0);

			inext[core] = 0;
			qnext[core] = 0;
		}
	}

	for (count = 0; count < 3; count++) {

		FOREACH_CORE(pi, core) {
			uint8 core_i;
			if (ACMAJORREV_4(pi->pubpi->phy_rev) &&
				(phy_get_phymode(pi) != PHYMODE_RSDB)) {
			/* In MIMO mode, RxSdFeConfig7 is a common register instead of being a
			 * path register in 4349A0. There are however two copies due to the
			 * RSDB support, which can be exploited to write to the shadow copy (RSDB)
			 * of each of the cores. This is done by writing to the MAC
			 * PHYMODE register => Turning on a bit in PHYMODE results the
			 * register write writing only to the core0 shadow copy. The solution
			 * then is to write to core1 first (which writes both core1 and core0
			 * copies), and then write only to core0 using the hack.
			 * Hence the following code: The register RxSdFeConfig7 is written by the
			 * function wlc_rx_digi_dccomp_set.
			 * So first writing to core1, then to core0
			 * -> function wlc_rx_digi_dccomp_set then enables the HACK when
			 *  writing to core 0
			 */
				core_i = !core;
			} else {
				core_i = core;
			}
			wlc_rx_digi_dccomp_set(pi, inext[core_i], qnext[core_i], core_i);
		}

		wlc_phy_get_rx_hpf_dc_est_acphy(pi, hpf_iqdc,
			((ACMAJORREV_32(pi->pubpi->phy_rev) ||
			ACMAJORREV_33(pi->pubpi->phy_rev)) ? 50 : 117));

		FOREACH_CORE(pi, core) {
			hpf_iqdc[core].i_accum += (1 << 13);
			hpf_iqdc[core].q_accum += (1 << 13);
			inext[core] = inext[core] - (int16)(hpf_iqdc[core].i_accum >> 14);
			qnext[core] = qnext[core] - (int16)(hpf_iqdc[core].q_accum >> 14);
		}
	}

	FOREACH_CORE(pi, core) {
		uint8 core_i;
		if (ACMAJORREV_4(pi->pubpi->phy_rev) &&
			(phy_get_phymode(pi) != PHYMODE_RSDB)) {
			/* see comment above */
			core_i = !core;
		} else {
			core_i = core;
		}

		/*
		 * Limit adjustment to maximum expected. This includes net offset due to:
		 * TIA, comparator and ADC offsets;
		if (inext[core_i] < -15)  inext[core_i] = -15;
		if (qnext[core_i] < -15)  qnext[core_i] = -15;
		if (inext[core_i] > 15)  inext[core_i] = 15;
		if (qnext[core_i] > 15)  qnext[core_i] = 15;
		*/

		wlc_rx_digi_dccomp_set(pi, inext[core_i], qnext[core_i], core_i);

		if (ACMAJORREV_32(pi->pubpi->phy_rev) || ACMAJORREV_33(pi->pubpi->phy_rev)) {
			/* Restore iq_swap values for all the cores */
			if (core == 0)
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq0, swap_saved[core_i]);
			else if (core == 1)
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq1, swap_saved[core_i]);
			else if (core == 2)
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq2, swap_saved[core_i]);
			else if (core == 3)
				MOD_PHYREG(pi, RxFeCtrl1, swap_iq3, swap_saved[core_i]);
		} else {
			MOD_PHYREGCXE(pi, RxFeCtrl1, swap_iq, core_i, swap_saved[core_i]);
		}

#if defined(BCMDBG_RXCAL)
		printf("core[%d]: static dc offset cal (%i, %i)\n",
			core_i, inext[core_i], qnext[core_i]);
#endif // endif
	}

	/* Restore settings */
	FOREACH_CORE(pi, core) {
		MOD_RADIO_REG_TINY(pi, TIA_CFG8, core, tia_offset_dac_biasadj, idacbias[core]);
		if (CHSPEC_ISPHY5G6G(pi->radio_chanspec)) {
			MOD_RADIO_REG_TINY(pi, RX_TOP_5G_OVR, core, ovr_lna5g_lna1_pu, ovr[core]);
			if ((!ACMAJORREV_4(pi->pubpi->phy_rev)) &&
			    (!ACMAJORREV_32(pi->pubpi->phy_rev)) &&
			    (!ACMAJORREV_33(pi->pubpi->phy_rev))) {
				MOD_RADIO_REG_TINY(pi, LNA5G_CFG1, core,
					lna5g_lna1_out_short_pu, 0);
				MOD_RADIO_REG_TINY(pi, RX_TOP_5G_OVR, core,
					ovr_lna5g_lna1_out_short_pu, 0);
			}
			MOD_RADIO_REG_TINY(pi, LNA5G_CFG1, core, lna5g_lna1_pu, pu[core]);
		} else {
			if (ACMAJORREV_4(pi->pubpi->phy_rev) || ACMAJORREV_32(pi->pubpi->phy_rev) ||
				ACMAJORREV_33(pi->pubpi->phy_rev)) {
				MOD_RADIO_REG_20693(pi, RX_TOP_2G_OVR_EAST2, core,
					ovr_lna2g_lna1_pu, ovr[core]);
			} else {
				MOD_RADIO_REG_TINY(pi, RX_TOP_2G_OVR_NORTH, core,
					ovr_lna2g_lna1_pu, ovr[core]);
				MOD_RADIO_REG_TINY(pi, RX_TOP_2G_OVR_NORTH, core,
					ovr_lna2g_lna1_out_short_pu, 0);
				MOD_RADIO_REG_TINY(pi, LNA2G_CFG1, core,
					lna2g_lna1_out_short_pu, 0);
			}
			MOD_RADIO_REG_TINY(pi, LNA2G_CFG1, core, lna2g_lna1_pu, pu[core]);
		}
	}

	if (ACMAJORREV_4(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, rx_tia_dc_loop_0, dc_loop_hold, 0);
		if (phy_get_phymode(pi) != PHYMODE_RSDB)
			MOD_PHYREG(pi, rx_tia_dc_loop_1, dc_loop_hold, 0);
	} else if (ACMAJORREV_32(pi->pubpi->phy_rev) || ACMAJORREV_33(pi->pubpi->phy_rev)) {
		/* Loop through cores */
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_hold, 0);
		}
	} else {
		WRITE_PHYREG(pi, SpareReg, sparereg & 0xbfff);	/* Release dcc fsm hold */
	}
	MOD_PHYREG(pi, RfseqTrigger, en_pkt_proc_dcc_ctrl, 1);

	/* Restoring Farrow settings */
	MOD_PHYREG(pi, RxSdFeConfig1, farrow_rshift_force, force_rshift);
	MOD_PHYREG(pi, RxSdFeConfig6, rx_farrow_rshift_0, rx_farrow_rshift);

	wlc_dcc_fsm_reset(pi);	/* Redo Coarse cal */
	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, FALSE);

#ifdef ATE_BUILD
	printf("===> Finished static dc offset cal\n");
#endif /* ATE_BUILD */

	return BCME_OK;
}

/*
 * Based on: proc tiny_rx_digi_dccomp_set {{value 0} { channel "i" }}
 */
static void
wlc_rx_digi_dccomp_set(phy_info_t *pi, int16 i, int16 q, uint8 core)
{
	if (i < -32)  i = -32;
	if (q < -32)  q = -32;
	if (i > 31)  i = 31;
	if (q > 31)  q = 31;

	/* Convert to 2's complement 6 bits representation */
	if (i < 0)
		i += 64;

	if (q < 0)
		q += 64;

	if (ACMAJORREV_4(pi->pubpi->phy_rev)) {
		WRITE_PHYREGCE(pi, RxSdFeConfig7, core, ((q & 0x3f) << 6) | (i & 0x3f));
	} else if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
	           ACMAJORREV_33(pi->pubpi->phy_rev) ||
	           ACMAJORREV_37(pi->pubpi->phy_rev) ||
	           (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
	           !ACMAJORREV_128(pi->pubpi->phy_rev))) {
		WRITE_PHYREGCE(pi, RxSdFeConfig7, core,
		               ((q & 0x3f) << 6) | (i & 0x3f));
	} else {
		uint16 war = READ_PHYREG(pi, work_around_ctrl);
		WRITE_PHYREG(pi, work_around_ctrl,
			(war & 0x8181) | ((q & 0x3f) << 9) | ((i & 0x3f) << 1));
		PHY_NONE(("%d  %d  0x%x\n", i, q, (READ_PHYREG(pi, work_around_ctrl) & 0x7e7e)));
	}
}

/* Based on: proc 20691_dcc_reset {  } */
void
wlc_dcc_fsm_reset(phy_info_t *pi)
{
	uint16 bt_state;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);

	/* Check if Manual BT mode is enabled and BT priority is already forced to WLAN. */
	bt_state = phy_btcx_is_override_enabled(pi);

	/* BT priority to WLAN, if not already done. */
	if (!bt_state)
		wlc_phy_btcx_override_enable(pi);

	if (ACMAJORREV_4(pi->pubpi->phy_rev)) {
		ACPHY_REG_LIST_START
			MOD_PHYREG_ENTRY(pi, rx_tia_dc_loop_0, dc_loop_hold, 0)
			MOD_PHYREG_ENTRY(pi, rx_tia_dc_loop_0, dc_loop_reset, 1)
			MOD_PHYREG_ENTRY(pi, rx_tia_dc_loop_0, dc_loop_reset, 0)
		ACPHY_REG_LIST_EXECUTE(pi);
		if (phy_get_phymode(pi) != PHYMODE_RSDB) {
			ACPHY_REG_LIST_START
				MOD_PHYREG_ENTRY(pi, rx_tia_dc_loop_1, dc_loop_hold, 0)
				MOD_PHYREG_ENTRY(pi, rx_tia_dc_loop_1, dc_loop_reset, 1)
				MOD_PHYREG_ENTRY(pi, rx_tia_dc_loop_1, dc_loop_reset, 0)
			ACPHY_REG_LIST_EXECUTE(pi);
		}
	} else if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
	           ACMAJORREV_33(pi->pubpi->phy_rev) ||
	           ACMAJORREV_37(pi->pubpi->phy_rev) ||
	           (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
	           !ACMAJORREV_128(pi->pubpi->phy_rev))) {
		uint8 core;
		/* Loop through cores */
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_hold, 0);
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_reset, 1);
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_reset, 0);
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_hold, 0);
		}
	} else {
		uint16 sparereg;

		sparereg = READ_PHYREG(pi, SpareReg);
		ASSERT(ACREV_GE(pi->pubpi->phy_rev, 11));
		WRITE_PHYREG(pi, SpareReg,  sparereg | 0x8000);
		WRITE_PHYREG(pi, SpareReg,  sparereg & 0x7fff);
	}
	/* Wait for FSM to finish. NB:depends on total dcc_loop_count_XX */
	OSL_DELAY(10);

	/* Restore BT priority, if over-written in this function. */
	if (!bt_state)
		wlc_phy_btcx_override_disable(pi);

	wlapi_enable_mac(pi->sh->physhim);
}

/* Based on: proc 20691_dcc_restart {  } */
void
wlc_dcc_fsm_restart(phy_info_t *pi)
{
	if (ACMAJORREV_4(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, rx_tia_dc_loop_0, dc_loop_hold, 1);
		MOD_PHYREG(pi, rx_tia_dc_loop_0, dc_loop_hold, 0);
		if (phy_get_phymode(pi) != PHYMODE_RSDB) {
			MOD_PHYREG(pi, rx_tia_dc_loop_1, dc_loop_hold, 1);
			MOD_PHYREG(pi, rx_tia_dc_loop_1, dc_loop_hold, 0);
		}
	} else if (ACMAJORREV_32(pi->pubpi->phy_rev) ||
	           ACMAJORREV_33(pi->pubpi->phy_rev) ||
	           ACMAJORREV_37(pi->pubpi->phy_rev) ||
	           (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
	           !ACMAJORREV_128(pi->pubpi->phy_rev))) {
		uint8 core;
		/* Loop through cores */
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_hold, 1);
			MOD_PHYREGCE(pi, rx_tia_dc_loop_, core, dc_loop_hold, 0);
		}
	} else {
		uint16 sparereg;

		sparereg = READ_PHYREG(pi, SpareReg);
		ASSERT(ACREV_GE(pi->pubpi->phy_rev, 11));
		WRITE_PHYREG(pi, SpareReg,  sparereg | 0x4000);
		WRITE_PHYREG(pi, SpareReg,  sparereg & 0xbfff);
	}
}

bool
phy_ac_dccal_multiphase_isen(phy_info_t *pi)
{
	bool en = FALSE;

	if (ACMAJORREV_GE47(pi->pubpi->phy_rev) && !ACMAJORREV_128(pi->pubpi->phy_rev)) en = TRUE;

	return en;
}

void
phy_ac_dccal_multiphase(phy_info_t *pi, uint16 cts_time)
{
	wlc_phy_cts2self(pi, cts_time);
	phy_ac_dccal(pi);
	pi->cal_info->cal_phase_id++;
}

void
phy_ac_dccal(phy_info_t *pi)
{
	uint8 num_retry = 1, save_fifo_rst = 0;
	uint8 core, num_rst_retry = 3;
	bool idac_cal_status = FALSE;
	bool suspend = FALSE;
	uint16 bt_state;
	bool dac_clks_forced_on = FALSE;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	/* Suspend MAC if haven't done so */
	wlc_phy_conditional_suspend(pi, &suspend);
	save_fifo_rst = READ_PHYREGFLD(pi, RxFeCtrl1,
			soft_sdfeFifoReset);
	MOD_PHYREG(pi, RxFeCtrl1, soft_sdfeFifoReset, 0);
	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, TRUE);

	bt_state = phy_btcx_is_override_enabled(pi);

	if (!bt_state) {
		/* Grab LNA control from BT */
		wlc_phy_btcx_override_enable(pi);
	}

	if (ACMAJORREV_40(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, dccal_control_7, core, dccal_clkgate_en, 0);
			MOD_PHYREGCE(pi, dccal_control_13, core, ld_dcoe_done, 0);
			MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x1);
			MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x0);

			MOD_PHYREGCE(pi, dccal_control_23, core, ld_idac_cal_done_0, 0xffc0);
			MOD_PHYREGCE(pi, dccal_control_22, core, ld_idac_cal_done_1, 0xff);
			MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0x1);
			MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0x0);

			MOD_PHYREGCE(pi, dccal_control_16, core, idact_bypass, 1);
		}
	} else if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, dccal_control_7, core, dccal_clkgate_en, 0);
			MOD_PHYREGCE(pi, dccal_control_13, core, ld_dcoe_done, 0);
			MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x1);
			MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x0);

			MOD_PHYREGCE(pi, dccal_control_26, core, idacc_done_init_0, 0xff00);
			MOD_PHYREGCE(pi, dccal_control_23, core, ld_idac_cal_done_0, 0xff00);
			MOD_PHYREGCE(pi, dccal_control_22, core, ld_idac_cal_done_1, 0xff);
			MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0x1);
			MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0x0);

			MOD_PHYREGCE(pi, dccal_control_16, core, idact_bypass, 1);
		}
	} else if (ACMAJORREV_GE47(pi->pubpi->phy_rev)) {
		if (ACMAJORREV_51_131(pi->pubpi->phy_rev)) {
			/* No need force dac clks on as DC will be re-calibrated after cal cleanup
			 * with PHY in final Rx configuration with c2c_sync on.(BCAWLAN-214876)
			 */
			dac_clks_forced_on = FALSE;
		} else if (pi_ac->c2c_sync_en && (pi->u.pi_acphy->c2csync_dac_clks_on == FALSE)) {
			phy_ac_chanmgr_core2core_sync_dac_clks(pi->u.pi_acphy->chanmgri, TRUE);
			dac_clks_forced_on = TRUE;
		}

		//wlc_phy_pulse_adc_reset_acphy(pi);
		MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, dccal_control_7, core, dccal_clkgate_en, 0);
			MOD_PHYREGCE(pi, dccal_control_13, core, ld_dcoe_done, 0);
			if (ACMAJORREV_47(pi->pubpi->phy_rev) ||
			     ACMAJORREV_51_129_131(pi->pubpi->phy_rev)) {

				if (!ACMAJORREV_129(pi->pubpi->phy_rev)) {
					MOD_PHYREGCE(pi, dccal_control_49, core,
						ld_dcoe_done_0, 0xf);
				} else {
					MOD_PHYREGCE(pi, dccal_control_49, core,
						ld_dcoe_done_0, 0x0);
				}
				MOD_PHYREGCE(pi, dccal_control_48, core, ld_dcoe_done_1, 0x0);
				MOD_PHYREGCE(pi, dccal_control_47, core, ld_dcoe_done_0, 0x0);
				MOD_PHYREGCE(pi, dccal_control_26, core, idacc_done_init_0, 0xe000);
				MOD_PHYREGCE(pi, dccal_control_27, core, idacc_done_init_1, 0xff);
				MOD_PHYREGCE(pi, dccal_control_23, core,
					ld_idac_cal_done_0, 0xe000);
				MOD_PHYREGCE(pi, dccal_control_22, core,
					ld_idac_cal_done_1, 0xff);
				MOD_PHYREGCE(pi, dccal_control_16, core,
					override_idac_cal_done, 0x1);
				MOD_PHYREGCE(pi, dccal_control_16, core,
					override_idac_cal_done, 0x0);
				MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x1);
				MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x0);
			} else {
				MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x1);
				MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0x0);
				MOD_PHYREGCE(pi, dccal_control_23, core,
					ld_idac_cal_done_0, 0xffc0);
				MOD_PHYREGCE(pi, dccal_control_22, core,
					ld_idac_cal_done_1, 0xff);
				MOD_PHYREGCE(pi, dccal_control_16, core,
					override_idac_cal_done, 0x1);
				MOD_PHYREGCE(pi, dccal_control_16, core,
					override_idac_cal_done, 0x0);
			}
			MOD_PHYREGCE(pi, dccal_control_16, core, idact_bypass, 1);
			MOD_PHYREGCE(pi, dccal_control_16, core, idacc_ppkt_reinit, 0);
			MOD_PHYREGCE(pi, dccal_control_15, core, idacc_tx2rx_reinit, 0);
			MOD_PHYREGCE(pi, dccal_control_18, core, idacc_tx2rx_only, 0);
		}
	} else {
		MOD_PHYREG(pi, dccal_control_130, ld_dcoe_done, 0x0);
		MOD_PHYREG(pi, dccal_control_130, override_dcoe_done, 0x1);
		MOD_PHYREG(pi, dccal_control_130, override_dcoe_done, 0x0);

		MOD_PHYREG(pi, dccal_control_230, ld_idac_cal_done_0, 0xf000);
		MOD_PHYREG(pi, dccal_control_220, ld_idac_cal_done_1, 0xff);
		MOD_PHYREG(pi, dccal_control_160, override_idac_cal_done, 0x1);
		MOD_PHYREG(pi, dccal_control_160, override_idac_cal_done, 0x0);

		MOD_PHYREG(pi, dccal_control_160, idact_bypass, 0x1);
	}

	if (ACMAJORREV_51_129_130_131(pi->pubpi->phy_rev)) {
		/* FIXME6715 */
		phy_ac_dccal_3steps(pi);
	} else if (ACMAJORREV_GE40(pi->pubpi->phy_rev)) {
		phy_ac_dccal_2steps(pi);
	} else {

		while ((num_rst_retry > 0) && (idac_cal_status == FALSE)) {
			MOD_PHYREG(pi, dccal_control_150, dccal_sw_reset_h, 0x1);
			MOD_PHYREG(pi, dccal_control_150, dccal_sw_reset_h, 0x0);
			MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x1);
			MOD_PHYREG(pi, dccal_control_160, dcoe_bypass, 0x0);
			MOD_PHYREG(pi, dccal_control_140, idacc_bypass, 0x0);
			MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x0);
			num_retry = 1;
			do {
				/* reset pkt_proc to perform dcoe, idacc; assumes PHY is in RX */
				/* wait for DCOE+IDACC to finish, max time depends on settling
				 * and accumulation times
				 */
				OSL_DELAY(80);
				/* This delay is important for idac_cal_done to assert */

				if (num_retry > 0) {
					if ((READ_PHYREGFLD(pi, dccal_control_210,
							idac_cal_done_0) == 0xffff) &&
							(READ_PHYREGFLD(pi, dccal_control_220,
							idac_cal_done_1) == 0xff)) {
						num_retry = 0;
						idac_cal_status = TRUE;
					} else {
						num_retry--;
					}
				}
			} while (num_retry);
			num_rst_retry--;
		}
	}
	if (!bt_state) {
		/* Release the control of LNA to BT */
		wlc_phy_btcx_override_disable(pi);
	}
	if (ACMAJORREV_GE40(pi->pubpi->phy_rev)) {
		if (!ACMAJORREV_51_131(pi->pubpi->phy_rev)) {
			MOD_PHYREG(pi, dccal_common, dcc_method_select, 0);
		}
		FOREACH_CORE(pi, core) {
			MOD_PHYREGCE(pi, dccal_control_7, core, dccal_clkgate_en, 1);
			MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 1);
			MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);
		}
	} else {
		PHY_TRACE(("\n\n\n\nIdac cal done status: %s, Num retry :%d\n",
			(idac_cal_status) ? "true" : "false", num_rst_retry));

		MOD_PHYREG(pi, dccal_control_160, dcoe_bypass, 0x1);
		MOD_PHYREG(pi, dccal_control_140, idacc_bypass, 0x1);
	}

	if (dac_clks_forced_on) {
		phy_ac_chanmgr_core2core_sync_dac_clks(pi->u.pi_acphy->chanmgri, FALSE);
	}

	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, FALSE);
	MOD_PHYREG(pi, RxFeCtrl1, soft_sdfeFifoReset, save_fifo_rst);

	/* Resume MAC */
	wlc_phy_conditional_resume(pi, &suspend);

}

static void
phy_ac_dccal_init_tia(phy_info_t *pi)
{
	uint8 core;

	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_15, core, dccal_sw_reset_h, 0);
		if (ACMAJORREV_40(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20694(pi, RF, TIA_DCDAC_REG2, core, tia_dcdac_scale, 2);
			MOD_RADIO_REG_20694(pi, RF, TIA_REG5, core, tia_spare, 1);
		} else if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20698(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 0);
			MOD_RADIO_REG_20698(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
		} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 0);
			MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
		} else if (ACMAJORREV_51(pi->pubpi->phy_rev)) {
			//MOD_RADIO_REG_20704(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 0);
			MOD_RADIO_REG_20704(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
		} else if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20709(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_i, 0);
			MOD_RADIO_REG_20709(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_q, 0);
			MOD_RADIO_REG_20709(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 7);
		} else if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20708(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 0);
			MOD_RADIO_REG_20708(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
			MOD_RADIO_REG_20708(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_i, 0);
			MOD_RADIO_REG_20708(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_q, 0);
		} else if (ACMAJORREV_131(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20710(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
		} else {
			MOD_RADIO_REG_20694(pi, RF, TIA_DCDAC_REG2, core, tia_dcdac_scale, 2);
			MOD_RADIO_REG_20694(pi, RF, TIA_REG5, core, tia_spare, 1);
		}
	}
}

static void
phy_ac_dccal_init_tia_percore(phy_info_t *pi, uint8 core)
{
	MOD_PHYREGCE(pi, dccal_control_15, core, dccal_sw_reset_h, 0);
	if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
		MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 0);
		MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
	}
}

void
phy_ac_dccal_init(phy_info_t *pi)
{
	uint8 core;
	// new DCC with digcorr
	if (ACMAJORREV_47(pi->pubpi->phy_rev) || ACMAJORREV_129(pi->pubpi->phy_rev)) {
		phy_ax_dccal_digcorr_init(pi);
	} else {
	// old DCC without digcorr
		if (ACMAJORREV_GE40(pi->pubpi->phy_rev)) {
			uint32 zeros[36] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

			WRITE_PHYREG(pi, idacc_update_hw_reset_len, 4000);
			MOD_PHYREG(pi, dccal_common, dcc_method_select, 0);
			MOD_PHYREG(pi, radio_pu_seq, dcc_tia_dac_method_select, 1);
			FOREACH_CORE(pi, core) {
				//dcoe init
				MOD_PHYREGCE(pi, dccal_control_17, core, dcctrigger_wait_cinit, 64);
				MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_inpshort, 0);
				MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_outshort, 1);
				MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_init, 0);
				MOD_PHYREGCE(pi, dccal_control,    core, dcoe_wait_cinit, 24);
				MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_zero_idac, 1);
				MOD_PHYREGCE(pi, dccal_control_1,  core, dcoe_abort_threshold, 50);
				MOD_PHYREGCE(pi, dccal_control_8,  core, dcoe_acc_cexp, 5);

				MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_num_entries, 32);
				MOD_PHYREGCE(pi, dccal_control_33, core, dcc_tia_num_entries, 8);
				MOD_PHYREGCE(pi, dccal_control_33, core, dcc_lpf_num_entries, 4);
				MOD_PHYREGCE(pi, dccal_control_13, core, ld_dcoe_done,  0);
				if (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
					!ACMAJORREV_128(pi->pubpi->phy_rev)) {
					MOD_PHYREGCE(pi, dccal_control_49, core,
						ld_dcoe_done_0, 0xf);
					MOD_PHYREGCE(pi, dccal_control_48, core,
						ld_dcoe_done_1, 0x0);
					MOD_PHYREGCE(pi, dccal_control_47, core,
						ld_dcoe_done_0, 0x0);
				}
				//override_dcoe_done toggle move to dccal

				//idacc init
				MOD_PHYREGCE(pi, dccal_control_7,  core, idacc_wait_cinit, 32);
				MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_acc_cexp, 5);
				MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_mag_select, 2);
				MOD_PHYREGCE(pi, dccal_control_9,  core, idac_lna1_refidx, 0);
				MOD_PHYREGCE(pi, dccal_control_9,  core, idac_lna1_scaling, 0);
				MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_abort_threshold, 50);
				MOD_PHYREGCE(pi, dccal_control_26, core, idacc_done_init_0, 0xffc0);
				MOD_PHYREGCE(pi, dccal_control_27, core, idacc_done_init_1, 0xff);
				MOD_PHYREGCE(pi, dccal_control_1,  core, idacc_tia_init_00, 5);
				MOD_PHYREGCE(pi, dccal_control_1,  core, idacc_tia_init_01, 5);
				MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_02, 5);
				MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_03, 5);
				MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_04, 5);
				MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_05, 5);
				if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
					MOD_PHYREGCE(pi, dccal_control_3, core, idacc_tia_init_06,
						5);
					MOD_PHYREGCE(pi, dccal_control_3, core, idacc_tia_init_07,
						5);
					MOD_PHYREGCE(pi, dccal_control_3, core, idacc_tia_init_08,
						5);
				}
				MOD_PHYREGCE(pi, dccal_control_15, core, dccal_sw_reset_h, 1);
				MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 1);
				MOD_PHYREGCE(pi, dccal_control_16, core, idact_bypass, 1);
				MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);

				if (ACMAJORREV_GE47(pi->pubpi->phy_rev)) {
					MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_0, 0);
					MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_1, 1);
					MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_2, 2);
					MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_3, 3);
					MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_4, 0);
					MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_5, 0);
					MOD_PHYREGCE(pi, dccal_control_42, core,
						dcoe_lpf_inv_map_0, 0);
					MOD_PHYREGCE(pi, dccal_control_42, core,
						dcoe_lpf_inv_map_1, 1);
					MOD_PHYREGCE(pi, dccal_control_42, core,
						dcoe_lpf_inv_map_2, 2);
					MOD_PHYREGCE(pi, dccal_control_42, core,
						dcoe_lpf_inv_map_3, 3);
					MOD_PHYREGCE(pi, dccal_control_43, core,
						dcoe_lpf_inv_map_4, 3);
					MOD_PHYREGCE(pi, dccal_control_43, core,
						dcoe_lpf_inv_map_5, 3);
					MOD_PHYREGCE(pi, dccal_control_38, core,
						dcoe_tia_inv_map_0, 0);
					MOD_PHYREGCE(pi, dccal_control_38, core,
						dcoe_tia_inv_map_1, 1);
					MOD_PHYREGCE(pi, dccal_control_38, core,
						dcoe_tia_inv_map_2, 2);
					MOD_PHYREGCE(pi, dccal_control_38, core,
						dcoe_tia_inv_map_3, 3);
					MOD_PHYREGCE(pi, dccal_control_39, core,
						dcoe_tia_inv_map_4, 4);
					MOD_PHYREGCE(pi, dccal_control_39, core,
						dcoe_tia_inv_map_5, 5);
					MOD_PHYREGCE(pi, dccal_control_39, core,
						dcoe_tia_inv_map_6, 6);
					MOD_PHYREGCE(pi, dccal_control_39, core,
						dcoe_tia_inv_map_7, 7);
					MOD_PHYREGCE(pi, dccal_control_40, core,
						dcoe_tia_inv_map_8, 7);
					MOD_PHYREGCE(pi, dccal_control_40, core,
						dcoe_tia_inv_map_9, 7);
					MOD_PHYREGCE(pi, dccal_control_40, core,
						dcoe_tia_inv_map_10, 7);
					MOD_PHYREGCE(pi, dccal_control_40, core,
						dcoe_tia_inv_map_11, 7);
					MOD_PHYREGCE(pi, dccal_control_41, core,
						dcoe_tia_inv_map_12, 7);
					MOD_PHYREGCE(pi, dccal_control_41, core,
						dcoe_tia_inv_map_13, 7);
					MOD_PHYREGCE(pi, dccal_control_41, core,
						dcoe_tia_inv_map_14, 7);
					MOD_PHYREGCE(pi, dccal_control_35, core,
						dcoe_tia_map_0, 0);
					MOD_PHYREGCE(pi, dccal_control_35, core,
						dcoe_tia_map_1, 1);
					MOD_PHYREGCE(pi, dccal_control_35, core,
						dcoe_tia_map_2, 2);
					MOD_PHYREGCE(pi, dccal_control_35, core,
						dcoe_tia_map_3, 3);
					MOD_PHYREGCE(pi, dccal_control_36, core,
						dcoe_tia_map_4, 4);
					MOD_PHYREGCE(pi, dccal_control_36, core,
						dcoe_tia_map_5, 5);
					MOD_PHYREGCE(pi, dccal_control_36, core,
						dcoe_tia_map_6, 6);
					MOD_PHYREGCE(pi, dccal_control_36, core,
						dcoe_tia_map_7, 15);
					MOD_PHYREGCE(pi, dccal_control_37, core,
						dcoe_tia_map_8, 0);
					MOD_PHYREGCE(pi, dccal_control_37, core,
						dcoe_tia_map_9, 0);
					MOD_PHYREGCE(pi, dccal_control_37, core,
						dcoe_tia_map_10, 0);
					MOD_PHYREGCE(pi, dccal_control_37, core,
						dcoe_tia_map_11, 0);
					if (!ACMAJORREV_128(pi->pubpi->phy_rev)) {
						MOD_PHYREGCE(pi, dccal_control_8,  core,
							idacc_acc_cexp, 6);
						MOD_PHYREGCE(pi, dccal_control_8,  core,
							dcoe_acc_cexp, 6);
					}
				}
				//override_idac_cal_done toggle move to dccal
			}

			//reset table
			MOD_PHYREG(pi, dccal_control_150, tbl_accessClk_sel, 0);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_DCOE_TABLE0, 36, 0, 32, zeros);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_TABLE0, 24, 0, 32, zeros);
			MOD_PHYREG(pi, dccal_control_150, tbl_accessClk_sel, 1);

			MOD_PHYREG(pi, dccal_control_151, tbl_accessClk_sel, 0);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_DCOE_TABLE1, 36, 0, 32, zeros);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_TABLE1, 24, 0, 32, zeros);
			MOD_PHYREG(pi, dccal_control_151, tbl_accessClk_sel, 1);

			if (ACMAJORREV_47_129_130(pi->pubpi->phy_rev)) {
				MOD_PHYREG(pi, dccal_control_152, tbl_accessClk_sel, 0);
				wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_DCOE_TABLE2,
					36, 0, 32, zeros);
				wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_TABLE2,
					24, 0, 32, zeros);
				MOD_PHYREG(pi, dccal_control_152, tbl_accessClk_sel, 1);

				MOD_PHYREG(pi, dccal_control_153, tbl_accessClk_sel, 0);
				wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_DCOE_TABLE3,
					36, 0, 32, zeros);
				wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_TABLE3,
					24, 0, 32, zeros);
				MOD_PHYREG(pi, dccal_control_153, tbl_accessClk_sel, 1);
			}
			phy_ac_dccal_init_tia(pi);
		} else {

		ACPHY_REG_LIST_START
			WRITE_PHYREG_ENTRY(pi, dccal_control_70, 0xe022)
			WRITE_PHYREG_ENTRY(pi, dccal_control_170, 0x003c)
			WRITE_PHYREG_ENTRY(pi, dccal_control_160, 0x001a)
			WRITE_PHYREG_ENTRY(pi, dccal_control_140, 0x0005)
			WRITE_PHYREG_ENTRY(pi, dccal_control_330, 0x4447)
			WRITE_PHYREG_ENTRY(pi, dccal_control_340, 0x0003)
			WRITE_PHYREG_ENTRY(pi, dccal_control_420, 0x0688)
			WRITE_PHYREG_ENTRY(pi, dccal_common, 0x1)
			WRITE_PHYREG_ENTRY(pi, dccal_control_80, 0xa145)
			WRITE_PHYREG_ENTRY(pi, dccal_control_90, 0x3200)
			WRITE_PHYREG_ENTRY(pi, idacc_update_hw_reset_len, 4000)
			WRITE_PHYREG_ENTRY(pi, dccal_control0, 0x4010)
		ACPHY_REG_LIST_EXECUTE(pi);

			/* Make IDAC current resolution as 16nA */
			MOD_RADIO_REG_28NM(pi, RF, TIA_DCDAC_REG2, 0, tia_dcdac_scale, 2);
			MOD_RADIO_REG_28NM(pi, RF, TIA_REG5, 0, tia_spare, 1);
		}
	}
}

void
phy_ac_load_gmap_tbl(phy_info_t *pi)
{
	if (ACMAJORREV_40(pi->pubpi->phy_rev)) {
		uint16 gmap_tbl_r40_2g[] = {0x10f3, 0x1133, 0x1177, 0x116b, 0x115f, 0x1143, 0x1000};
		uint16 gmap_tbl_r40_5g[] = {0x10e7, 0x1127, 0x11a7, 0x11eb, 0x11df, 0x11c3, 0x1000};
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if ((ACMAJORREV_40(pi->pubpi->phy_rev) && ACMINORREV_1(pi)) ||
				ACMAJORREV_128(pi->pubpi->phy_rev)) {
				gmap_tbl_r40_2g[0] = 0x10f7;
				gmap_tbl_r40_2g[1] = 0x1137;
			}
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 7, 0, 16,
				&gmap_tbl_r40_2g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 7, 0, 16,
				&gmap_tbl_r40_2g);
		} else {
			if ((ACMAJORREV_40(pi->pubpi->phy_rev) && ACMINORREV_1(pi)) ||
				ACMAJORREV_128(pi->pubpi->phy_rev)) {
				gmap_tbl_r40_5g[0] = 0x10f3;
				gmap_tbl_r40_5g[2] = 0x11ab;
			}
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 7, 0, 16,
				&gmap_tbl_r40_5g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 7, 0, 16,
				&gmap_tbl_r40_5g);
		}
	} else if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
		/* read directly from AGC lna1 gain/rout tables */
		uint16 gmap_tbl[13];
		uint8 k, gm, rout;

		gmap_tbl[0] = 0x1000;
		for (k = 0; k < 6; k++) {
			gm =  wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_GAINCODE);
			rout = wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_ROUT);
			gmap_tbl[k+1] = (0x1000 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			gmap_tbl[k+7] = (((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
		}

		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE2, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE3, 13, 0, 16,
			&gmap_tbl);
	} else if (ACMAJORREV_51_131(pi->pubpi->phy_rev)) {
		bool elna_present = (CHSPEC_IS2G(pi->radio_chanspec)) ?
		    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA) != 0):
		    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA_5GHz) != 0);
		/* read directly from AGC lna1 gain/rout tables */
		/*  TR[12] eLNA[11] LNA1_Bypass[10] LNA1_low_ct[9] LNA1_gm[8:6]
			LNA1_Rout[5:2] LNA2_gm[1:0]
			*/
		uint16 gmap_tbl[13];
		uint8 k, gm, rout;
		if (elna_present) {
			gmap_tbl[0] = 0x1000;
			for (k = 0; k < 6; k++) {
			    gm =  wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_GAINCODE);
			    rout = wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_ROUT);
			    gmap_tbl[k+1] = (0x1000 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			    gmap_tbl[k+7] = (((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			}
		} else { /* iLNA */
			gmap_tbl[0] = 0x1000;
			for (k = 0; k < 6; k++) {
			    gm =  wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_GAINCODE);
			    rout = wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_ROUT);
			    gmap_tbl[k+1] = (0x1000 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			    gmap_tbl[k+7] = 0x0000;
			}
			/* lna1bypass entry */
			gm =  wlc_phy_get_lna_gain_rout(pi, 6, GET_LNA_GAINCODE);
			rout = wlc_phy_get_lna_gain_rout(pi, 6, GET_LNA_ROUT);
			gmap_tbl[7] = (0x1400 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
		}
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 13, 0, 16, &gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 13, 0, 16, &gmap_tbl);
	} else if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
		bool elna_present = (CHSPEC_IS2G(pi->radio_chanspec)) ?
		    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA) != 0):
		    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA_5GHz) != 0);
		/* read directly from AGC lna1 gain/rout tables */
		/*  TR[12] eLNA[11] LNA1_Bypass[10] LNA1_low_ct[9] LNA1_gm[8:6]
			LNA1_Rout[5:2] LNA2_gm[1:0]
			*/
		uint16 gmap_tbl[13];
		uint8 k, gm, rout;
		gmap_tbl[0] = 0x1000;
		for (k = 0; k < 6; k++) {
			gm =  wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_GAINCODE);
			rout = wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_ROUT);
			gmap_tbl[k+1] = (0x1000 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			if (elna_present) {
			    gmap_tbl[k+7] = (((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			} else { /* iLNA */
			    gmap_tbl[k+7] = (k == 0) ?
			    (0x1400 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3) : 0x0;
			}
		}
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 13, 0, 16, &gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 13, 0, 16, &gmap_tbl);
	} else if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
		uint16 gmap_tbl_r47_2g[] = {4267, 4331, 4391, 4459, 4447, 4419, 4096};
		uint16 gmap_tbl_r47_5g[] = {4399, 4459, 4523, 4591, 4575, 4547, 4096};
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 7, 0, 16,
				&gmap_tbl_r47_2g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 7, 0, 16,
				&gmap_tbl_r47_2g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE2, 7, 0, 16,
				&gmap_tbl_r47_2g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE3, 7, 0, 16,
				&gmap_tbl_r47_2g);
		} else {
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 7, 0, 16,
				&gmap_tbl_r47_5g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 7, 0, 16,
				&gmap_tbl_r47_5g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE2, 7, 0, 16,
				&gmap_tbl_r47_5g);
			wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE3, 7, 0, 16,
				&gmap_tbl_r47_5g);
		}
	} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
		/* read directly from AGC lna1 gain/rout tables */
		uint16 gmap_tbl[13];
		uint8 k, gm, rout;

		bool elna_present = (CHSPEC_IS2G(pi->radio_chanspec)) ?
		    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA) != 0):
		    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA_5GHz) != 0);

		gmap_tbl[0] = 0x1000;
		for (k = 0; k < 6; k++) {
			gm =  wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_GAINCODE);
			rout = wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_ROUT);
			gmap_tbl[k+1] = (0x1000 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			gmap_tbl[k+7] = elna_present ?
			    (((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3) : 0x0000;
		}

		if (!elna_present) {
			/* lna1bypass entry */
			gm =  wlc_phy_get_lna_gain_rout(pi, 6, GET_LNA_GAINCODE);
			rout = wlc_phy_get_lna_gain_rout(pi, 6, GET_LNA_ROUT);
			gmap_tbl[7] = (0x1400 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
		}

		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE2, 13, 0, 16,
			&gmap_tbl);
	} else if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
		/* read directly from AGC lna1 gain/rout tables */
		uint16 gmap_tbl[13];
		uint8 k, gm, rout;

		gmap_tbl[0] = 0x1000;
		for (k = 0; k < 6; k++) {
			gm =  wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_GAINCODE);
			rout = wlc_phy_get_lna_gain_rout(pi, k, GET_LNA_ROUT);
			gmap_tbl[k+1] = (0x1000 | ((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
			gmap_tbl[k+7] = (((gm & 0x7) << 6) | ((rout & 0xf) << 2) | 3);
		}

		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE0, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE1, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE2, 13, 0, 16,
			&gmap_tbl);
		wlc_phy_table_write_acphy(pi, AC2PHY_TBL_ID_IDAC_GMAP_TABLE3, 13, 0, 16,
			&gmap_tbl);

	} else {
		uint16 gmap_table_2g[] = {4261, 4325, 4389, 4453, 4441, 4417, 4262, 4326,
			4390, 4454, 4442, 4418, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		uint16 gmap_table_5g[] = {0x1126, 0x1166, 0x11A6, 0x11E6, 0x11DA, 0x11C2, 0x1127,
			0x1167, 0x11A7, 0x11E7, 0x11DB, 0x11C3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IDAC_GMAP_TABLE0, 12, 0, 16,
				&gmap_table_2g);
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IDAC_GMAP_TABLE0, 12, 0, 16,
				&gmap_table_5g);
		}
	}
}

void phy_ac_dccal_2steps(phy_info_t *pi)
{
	if (ACMAJORREV_47(pi->pubpi->phy_rev) || ACMAJORREV_129(pi->pubpi->phy_rev)) {
		phy_ax_dccal_digcorr_dcoe(pi);
		phy_ax_dccal_digcorr_idacc(pi);
	} else {
		phy_ac_dccal_dcoe_only(pi);
		phy_ac_dccal_idacc_only(pi);
	}
}

static void phy_ac_dccal_3steps(phy_info_t *pi)
{
	bool elna_present = (CHSPEC_IS2G(pi->radio_chanspec)) ?
	    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA) != 0):
	    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA_5GHz) != 0);
	uint16 mask;

	uint8 core;

	phy_ax_dccal_digcorr_init(pi);

	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_zero_idac, 0);
		WRITE_PHYREGCE(pi, dccal_control_26, core, 0xfffe);
		WRITE_PHYREGCE(pi, dccal_control_23, core, 0xfffe);
		MOD_PHYREGCE(pi, dccal_control_50, core, biq0_gidx_init, 1);
		if (ACMAJORREV_51(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20704(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 0);
		} else if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20708(pi, LOGEN_CORE_OVR0, core, ovr_logen_core_mux_pu, 1);
			MOD_RADIO_REG_20708(pi, LOGEN_CORE_REG5, core, logen_core_mux_pu, 0);
		} else if (ACMAJORREV_131(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20710(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 0);
		} else {
			MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 0);
		}
	}
	phy_ax_dccal_digcorr_idacc(pi);
	phy_ax_dccal_digcorr_dcoe(pi);

	mask = elna_present ? 0xe001 : 0xff01;
	FOREACH_CORE(pi, core) {
		WRITE_PHYREGCE(pi, dccal_control_26, core, mask);
		WRITE_PHYREGCE(pi, dccal_control_23, core, mask);
		MOD_PHYREGCE(pi, dccal_control_50, core, biq0_gidx_init, 1);
		if (ACMAJORREV_51(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20704(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		} else if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20708(pi, LOGEN_CORE_OVR0, core, ovr_logen_core_mux_pu, 1);
			MOD_RADIO_REG_20708(pi, LOGEN_CORE_REG5, core, logen_core_mux_pu, 1);
		} else if (ACMAJORREV_131(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20710(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		} else {
			MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		}
	}
	phy_ax_dccal_digcorr_idacc(pi);

}

void
phy_ac_dccal_papd_special(phy_info_t *pi, int8 tia_init, uint8 core)
{
	uint8 coree;
	uint8 save_fifo_rst = 0;

	bool suspend = FALSE;

	/* Suspend MAC if haven't done so */
	wlc_phy_conditional_suspend(pi, &suspend);
	save_fifo_rst = READ_PHYREGFLD(pi, RxFeCtrl1,
			soft_sdfeFifoReset);
	MOD_PHYREG(pi, RxFeCtrl1, soft_sdfeFifoReset, 0);
	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, TRUE);

	//wlc_phy_pulse_adc_reset_acphy(pi);
	MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
	FOREACH_CORE(pi, coree) {
		MOD_PHYREGCE(pi, dccal_control_7, coree, dccal_clkgate_en, 0);
		MOD_PHYREGCE(pi, dccal_control_13, coree, ld_dcoe_done, 0);

		MOD_PHYREGCE(pi, dccal_control_49, coree, ld_dcoe_done_0, 0x0);

		MOD_PHYREGCE(pi, dccal_control_48, coree, ld_dcoe_done_1, 0x0);
		MOD_PHYREGCE(pi, dccal_control_47, coree, ld_dcoe_done_0, 0x0);
		MOD_PHYREGCE(pi, dccal_control_26, coree, idacc_done_init_0, 0xe000);
		MOD_PHYREGCE(pi, dccal_control_27, coree, idacc_done_init_1, 0xff);
		MOD_PHYREGCE(pi, dccal_control_23, coree, ld_idac_cal_done_0, 0xe000);
		MOD_PHYREGCE(pi, dccal_control_22, coree, ld_idac_cal_done_1, 0xff);
		MOD_PHYREGCE(pi, dccal_control_16, coree, override_idac_cal_done, 0x1);
		MOD_PHYREGCE(pi, dccal_control_16, coree, override_idac_cal_done, 0x0);
		MOD_PHYREGCE(pi, dccal_control_13, coree, override_dcoe_done, 0x1);
		MOD_PHYREGCE(pi, dccal_control_13, coree, override_dcoe_done, 0x0);

		MOD_PHYREGCE(pi, dccal_control_16, coree, idact_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_16, coree, idacc_ppkt_reinit, 0);
		MOD_PHYREGCE(pi, dccal_control_15, coree, idacc_tx2rx_reinit, 0);
		MOD_PHYREGCE(pi, dccal_control_18, coree, idacc_tx2rx_only, 0);
	}

	ASSERT(ACMAJORREV_129(pi->pubpi->phy_rev));

	FOREACH_CORE(pi, coree) {
		phy_ax_dccal_digcorr_init_tiainit(pi, tia_init, coree);
	}
	FOREACH_CORE(pi, coree) {
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_zero_idac, 0);
		if (coree == core) {
			WRITE_PHYREGCE(pi, dccal_control_26, coree, 0xfffe);
			WRITE_PHYREGCE(pi, dccal_control_23, coree, 0xfffe);
		} else {
			WRITE_PHYREGCE(pi, dccal_control_26, coree, 0xffff);
			WRITE_PHYREGCE(pi, dccal_control_23, coree, 0xffff);
		}
		MOD_PHYREGCE(pi, dccal_control_50, coree, biq0_gidx_init, 1);
	}

	phy_ax_dccal_digcorr_idacc_override(pi, core);

	MOD_PHYREG(pi, dccal_common, dcc_method_select, 0);
	FOREACH_CORE(pi, coree) {
		MOD_PHYREGCE(pi, dccal_control_7, coree, dccal_clkgate_en, 1);
		MOD_PHYREGCE(pi, dccal_control_16, coree, dcoe_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_14, coree, idacc_bypass, 1);
	}

	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, FALSE);
	MOD_PHYREG(pi, RxFeCtrl1, soft_sdfeFifoReset, save_fifo_rst);

	/* Resume MAC */
	wlc_phy_conditional_resume(pi, &suspend);
}

void
phy_ac_dccal_save(phy_info_t *pi)
{
	uint8 core, idx;
	uint16 tbl_id;
	uint16 idacc_size = 24;
	uint32 read_val;
	uint16 dcoe_size;

	if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
		dcoe_size = 36;
	} else {
		dcoe_size = READ_PHYREGFLD(pi, dccal_control_430, dcoe_num_entries);
	}

	FOREACH_CORE(pi, core) {
		tbl_id = AC2PHY_TBL_ID_DCOE_TABLE0 + 32 * core;
		for (idx = 0; idx < dcoe_size; idx++) {
			wlc_phy_table_read_acphy(pi, tbl_id, 1, idx, 32, &read_val);
			pi->u.pi_acphy->dccali->dcoe_table[core][idx] = read_val;
		}

		tbl_id = AC2PHY_TBL_ID_IDAC_TABLE0 + 32 * core;
		for (idx = 0; idx < idacc_size; idx++) {
			wlc_phy_table_read_acphy(pi, tbl_id, 1, idx, 32, &read_val);
			pi->u.pi_acphy->dccali->idacc_table[core][idx] = read_val;
		}
	}
}

void
phy_ac_dccal_restore(phy_info_t *pi)
{
	uint8 core, idx;
	uint16 tbl_id;
	uint16 idacc_size = 24;
	uint32 write_val;
	uint16 dcoe_size;

	if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
		dcoe_size = 36;
	} else {
		dcoe_size = READ_PHYREGFLD(pi, dccal_control_430, dcoe_num_entries);
	}

	FOREACH_CORE(pi, core) {
		tbl_id = AC2PHY_TBL_ID_DCOE_TABLE0 + 32 * core;
		for (idx = 0; idx < dcoe_size; idx++) {
			write_val = pi->u.pi_acphy->dccali->dcoe_table[core][idx];
			wlc_phy_table_write_acphy(pi, tbl_id, 1, idx, 32, &write_val);
		}

		tbl_id = AC2PHY_TBL_ID_IDAC_TABLE0 + 32 * core;
		for (idx = 0; idx < idacc_size; idx++) {
			write_val = pi->u.pi_acphy->dccali->idacc_table[core][idx];
			wlc_phy_table_write_acphy(pi, tbl_id, 1, idx, 32, &write_val);
		}

		MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 0);
		MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_pu, 0);
		MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
		MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_i, 0);
		MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_q, 0);
		MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG1, core, tia_dcdac_i, 0);
		MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_q, 0);
	}
}

void phy_ac_dccal_dcoe_only(phy_info_t  *pi)
{
	uint8 num_retry = 1;
	bool idac_cal_status = FALSE;
	uint8 core;
	uint8 phyrxchain;
	BCM_REFERENCE(phyrxchain);
	phyrxchain = phy_stf_get_data(pi->stfi)->phyrxchain;

	//CAL dcoe first
	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 0);
		MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);
	}
	if (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
		!ACMAJORREV_128(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
	}
	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		if (ACMAJORREV_40(pi->pubpi->phy_rev)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REG_20694(pi, RF, RX2G_CFG1_OVR, core, ovr_rx2g_lo_en, 1);
				MOD_RADIO_REG_20694(pi, RF, RX2G_REG1, core, rx2g_lo_en, 0);
			} else {
				MOD_RADIO_REG_20694(pi, RF, RX5G_CFG2_OVR, core,
				ovr_rx5g_mix_pu, 1);
				MOD_RADIO_REG_20694(pi, RF, RX5G_REG5, core, rx5g_mix_pu, 0);
			}
		} else if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
			// Shared between 2G and 5G
			MOD_RADIO_REG_20709(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 0);
		} else if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REG_20698(pi, RX2G_REG1, core, rx2g_lo_en, 0);
				MOD_RADIO_REG_20698(pi, RX2G_CFG1_OVR, core, ovr_rx2g_lo_en, 1);
			} else {
				MOD_RADIO_REG_20698(pi, RX5G_REG5, core, rx5g_mix_pu, 0);
				MOD_RADIO_REG_20698(pi, RX5G_CFG1_OVR, core, ovr_rx5g_mix_pu, 1);
			}
		} else if (ACMAJORREV_51(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
			// Shared between 2G and 5G
			MOD_RADIO_REG_20704(pi, LOGEN_CORE_REG0, core, logen_rx_rccr_pu, 0);
			MOD_RADIO_REG_20704(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 1);
		} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				// use mux_pu to power down LO
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			} else {
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG0, core, logen_rx_rccr_pu, 0);
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_OVR0, core,
					ovr_logen_rx_rccr_pu, 1);
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			}
		} else if (ACMAJORREV_131(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
			// Shared between 2G and 5G
			MOD_RADIO_REG_20710(pi, LOGEN_CORE_REG0, core, logen_rx_rccr_pu, 0);
			MOD_RADIO_REG_20710(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 1);
		} else {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REG_20694(pi, RF, RX2G_CFG1_OVR, core, ovr_rx2g_lo_en, 1);
				MOD_RADIO_REG_20694(pi, RF, RX2G_REG1, core, rx2g_lo_en, 0);
			} else {
				MOD_RADIO_REG_20694(pi, RF, RX5G_CFG2_OVR, core,
						ovr_rx5g_mix_pu, 1);
				MOD_RADIO_REG_20694(pi, RF, RX5G_REG5, core, rx5g_mix_pu, 0);
			}

		}
	}

	MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x1);
	OSL_DELAY(10);
	MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x0);
	OSL_DELAY(100);

	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		num_retry = 3;
		idac_cal_status = FALSE;
		while (!idac_cal_status) {
			if (num_retry > 0) {
				OSL_DELAY(10);
				if (READ_PHYREGFLDCE(pi, dccal_control_44,
						core, dcoe_done_0) == 0xffff &&
					READ_PHYREGFLDCE(pi, dccal_control_45,
						core, dcoe_done_1) == 0xffff) {
					idac_cal_status = TRUE;
					PHY_CAL(("dccal: dcoe complete for core %d\n",
							core));
				} else {
					num_retry--;
				}
			} else {
				PHY_CAL(("dccal: dcoe timeout for core %d!!!\n", core));
				idac_cal_status = TRUE;
			}
		}
		if (ACMAJORREV_40(pi->pubpi->phy_rev)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REG_20694(pi, RF, RX2G_CFG1_OVR, core, ovr_rx2g_lo_en, 0);
			} else {
				MOD_RADIO_REG_20694(pi, RF, RX5G_CFG2_OVR, core,
					ovr_rx5g_mix_pu, 0);
			}
		} else if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x1);
			// restore overwrites
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REG_20698(pi, RX2G_CFG1_OVR, core, ovr_rx2g_lo_en, 0);
			} else {
				MOD_RADIO_REG_20698(pi, RX5G_CFG1_OVR, core, ovr_rx5g_mix_pu, 0);
			}
		} else if (ACMAJORREV_51(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x1);
			// Shared between 2G and 5G
			MOD_RADIO_REG_20704(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 0);
		} else if (ACMAJORREV_128(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x1);
			// Shared between 2G and 5G
			MOD_RADIO_REG_20709(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x1);
			MOD_RADIO_REG_20707(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 0);
			MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		} else if (ACMAJORREV_131(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x1);
			// Shared between 2G and 5G
			MOD_RADIO_REG_20710(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 0);
		} else {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
					MOD_RADIO_REG_20694(pi, RF, RX2G_CFG1_OVR, core,
							ovr_rx2g_lo_en, 0);
					} else {
						MOD_RADIO_REG_20694(pi, RF, RX5G_CFG2_OVR, core,
								ovr_rx5g_mix_pu, 0);
					}
		}
	}
	if (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
		!ACMAJORREV_128(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, dccal_common, dcc_method_select, 0);
	}
}

void phy_ac_dccal_idacc_only(phy_info_t  *pi)
{
	uint8 num_retry = 1;
	bool idac_cal_status = FALSE;
	uint8 core;
	uint8 phyrxchain;
	BCM_REFERENCE(phyrxchain);
	phyrxchain = phy_stf_get_data(pi->stfi)->phyrxchain;

	//CAL idacc only
	if (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
		!ACMAJORREV_128(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
	}
	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 0);
		if (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
			!ACMAJORREV_128(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
		}
	}
	MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x1);
	OSL_DELAY(10);
	MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x0);
	OSL_DELAY(100);

	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		num_retry = 3;
		idac_cal_status = FALSE;
		while (!idac_cal_status) {
			if (num_retry > 0) {
				OSL_DELAY(10);
				if (READ_PHYREGFLDCE(pi, dccal_control_21,
						core, idac_cal_done_0) == 0xffff &&
					READ_PHYREGFLDCE(pi, dccal_control_22,
						core, idac_cal_done_1) == 0xff) {
					idac_cal_status = TRUE;
					PHY_CAL(("dccal: idacc complete for core %d\n",
							core));
				} else {
					num_retry--;
				}
			} else {
				PHY_CAL(("dccal: idacc timeout for core %d!!!\n", core));
				idac_cal_status = TRUE;
			}
		}
	}

	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		if (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
			!ACMAJORREV_128(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x1);
		}
	}
	if (ACMAJORREV_GE47(pi->pubpi->phy_rev) &&
		!ACMAJORREV_128(pi->pubpi->phy_rev)) {
		MOD_PHYREG(pi, dccal_common, dcc_method_select, 0);
	}
}

void
phy_ax_dccal_digcorr_init(phy_info_t *pi)
{
	uint8 core;
	uint8 stall_val;
	uint32 zeros[36] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	stall_val = READ_PHYREGFLD(pi, RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	WRITE_PHYREG(pi, idacc_update_hw_reset_len, 64000);
	MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
	MOD_PHYREG(pi, radio_pu_seq, dcc_tia_dac_method_select, 1);
	MOD_PHYREG(pi, dccal_common, dcc_opt_1024qam, 1);
	// Bypass DC fixed correction after HT/VHT/HE STF, better 11ax performance
	// We enable the DC fixed corr back if either DCOE or IDAC cal fails
	MOD_PHYREG(pi, DcFiltAddress, bypass_HT_VHT_HE_STF, 1);

	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_en, 1);
		MOD_PHYREGCE(pi, dccal_control_9,  core, idac_lna1_refidx, 0);

		MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_15, core, scale_acc_wlen, 1);
		MOD_PHYREGCE(pi, dccal_control_15, core, bypass_idac_res_apply, 0);
		MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 1);

		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_16, core, idact_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_16, core, idact_enable, 0);
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_zero_idac, 1);
		MOD_PHYREGCE(pi, dccal_control_14, core, dcoe_force_zero_ovrride, 0);

		MOD_PHYREGCE(pi, dccal_control_17, core, dcctrigger_wait_cinit, 28);

		MOD_PHYREGCE(pi, dccal_control_18, core, farrow_blankctr_init, 40);
		MOD_PHYREGCE(pi, dccal_control_18, core, rssi_blank_enable_during_dcc, 1);
		MOD_PHYREGCE(pi, dccal_control_18, core, dccal_blank_enable_farrow_in, 1);

		// dcoe gain map
		if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_num_entries, 36);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcc_tia_num_entries, 9);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcc_lpf_num_entries, 4);

			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_4, 0);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_5, 0);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_4, 3);
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_5, 3);

			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_1, 0);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_2, 1);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_3, 2);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_4, 3);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_5, 4);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_6, 5);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_7, 6);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_8, 15);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_9, 0);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_10, 0);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_11, 0);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_0, 1);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_1, 2);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_2, 3);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_3, 4);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_4, 5);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_5, 6);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_6, 7);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_7, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_8, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_9, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_10, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_11, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_12, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_13, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_14, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_15, 8);
		} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_num_entries, 36);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcc_tia_num_entries, 9);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcc_lpf_num_entries, 4);

			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_4, 0);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_5, 0);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_4, 3);
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_5, 3);

			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_0, 1);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_1, 2);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_2, 3);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_3, 4);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_4, 5);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_5, 6);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_6, 7);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_7, 8);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_8, 15);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_9, 0);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_10, 0);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_11, 0);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_1, 0);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_2, 1);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_3, 2);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_4, 3);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_5, 4);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_6, 5);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_7, 6);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_8, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_9, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_10, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_11, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_12, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_13, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_14, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_15, 8);
		} else {
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_num_entries, 32);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcc_tia_num_entries, 8);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcc_lpf_num_entries, 4);

			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_4, 0);
			MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_5, 0);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_4, 3);
			MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_5, 3);

			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_4, 4);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_5, 5);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_6, 6);
			MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_7, 15);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_8, 0);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_9, 0);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_10, 0);
			MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_11, 0);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_0, 0);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_1, 1);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_2, 2);
			MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_3, 3);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_4, 4);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_5, 5);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_6, 6);
			MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_7, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_8, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_9, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_10, 7);
			MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_11, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_12, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_13, 7);
			MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_14, 7);
		}
		// dcoe init regs
		MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_inpshort, 0);
		MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_outshort, 1);
		MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_init, 0);
		MOD_PHYREGCE(pi, dccal_control,    core, dcoe_wait_cinit, 24);
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_zero_idac, 1);
		MOD_PHYREGCE(pi, dccal_control_8,  core, dcoe_acc_cexp, 4);

		// clear dcoe done
		if (!ACMAJORREV_129_130(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_49, core, ld_dcoe_done_0, 0xf);
		} else {
			MOD_PHYREGCE(pi, dccal_control_49, core, ld_dcoe_done_0, 0x0);
		}
		MOD_PHYREGCE(pi, dccal_control_48, core, ld_dcoe_done_1, 0x0);
		MOD_PHYREGCE(pi, dccal_control_47, core, ld_dcoe_done_0, 0x0);
		MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 1);
		MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0);

		// idacc init regs
		MOD_PHYREGCE(pi, dccal_control_7,  core, idacc_wait_cinit, 32);
		MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_mag_select, 2);
		MOD_PHYREGCE(pi, dccal_control_1,  core, idacc_tia_init_00, 6);
		MOD_PHYREGCE(pi, dccal_control_1,  core, idacc_tia_init_01, 6);
		MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_02, 6);
		MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_03, 6);
		MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_04, 6);
		MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_05, 6);
		MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_06, 6);
		MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_07, 6);
		MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_08, 6);
		MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_09, 6);
		MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_10, 6);
		MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_11, 6);
		MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_12, 6);
		MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_13, 6);

		MOD_PHYREGCE(pi, dccal_control_50, core, tia_casc_idx, 10);
		MOD_PHYREGCE(pi, dccal_control_50, core, tia_casc_map, 6);

		MOD_PHYREGCE(pi, dccal_control_50, core, biq0_gidx_init, 0);

		MOD_PHYREGCE(pi, dccal_control_14, core, multi_clip_dcc_tia_war, 0);

		// clear idacc done
		MOD_PHYREGCE(pi, dccal_control_26, core, idacc_done_init_0, 0xe000);
		MOD_PHYREGCE(pi, dccal_control_27, core, idacc_done_init_1, 0xff);
		MOD_PHYREGCE(pi, dccal_control_23, core, ld_idac_cal_done_0, 0xe000);
		MOD_PHYREGCE(pi, dccal_control_22, core, ld_idac_cal_done_1, 0xff);
		MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 1);
		MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0);

		MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 0);

		MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 0);
		wlc_phy_table_write_acphy(pi,
				AC2PHY_TBL_ID_DCOE_TABLE0 + (core *
				(AC2PHY_TBL_ID_DCOE_TABLE1 - AC2PHY_TBL_ID_DCOE_TABLE0)),
				36, 0, 32, zeros);
		wlc_phy_table_write_acphy(pi,
				AC2PHY_TBL_ID_IDAC_TABLE0 + (core *
				(AC2PHY_TBL_ID_IDAC_TABLE1 - AC2PHY_TBL_ID_IDAC_TABLE0)),
				24, 0, 32, zeros);
		wlc_phy_table_write_acphy(pi,
				AXPHY_TBL_ID_IDAC_RES_TABLE0 + (core *
				(AXPHY_TBL_ID_IDAC_RES_TABLE1 - AXPHY_TBL_ID_IDAC_RES_TABLE0)),
				24, 0, 32, zeros);
		MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 1);
	}

	phy_ax_dccal_digcorr_bwspecific(pi);
	phy_ac_dccal_init_tia(pi);
	ACPHY_ENABLE_STALL(pi, stall_val);
}

void
phy_ax_dccal_digcorr_init_tiainit(phy_info_t *pi, int8 tia_init, uint8 core)
{
	uint8 coree;
	uint8 stall_val;
	uint32 zeros[36] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	stall_val = READ_PHYREGFLD(pi, RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	WRITE_PHYREG(pi, idacc_update_hw_reset_len, 64000);
	MOD_PHYREG(pi, dccal_common, dcc_method_select, 1);
	MOD_PHYREG(pi, radio_pu_seq, dcc_tia_dac_method_select, 1);
	MOD_PHYREG(pi, dccal_common, dcc_opt_1024qam, 1);
	// Bypass DC fixed correction after HT/VHT/HE STF, better 11ax performance
	// We enable the DC fixed corr back if either DCOE or IDAC cal fails
	MOD_PHYREG(pi, DcFiltAddress, bypass_HT_VHT_HE_STF, 1);

	MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_en, 1);
	MOD_PHYREGCE(pi, dccal_control_9,  core, idac_lna1_refidx, 0);

	MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);
	MOD_PHYREGCE(pi, dccal_control_15, core, scale_acc_wlen, 1);
	MOD_PHYREGCE(pi, dccal_control_15, core, bypass_idac_res_apply, 0);
	MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 1);
	FOREACH_CORE(pi, coree) {
		MOD_PHYREGCE(pi, dccal_control_16, coree, dcoe_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_16, coree, idact_bypass, 1);
	}
	MOD_PHYREGCE(pi, dccal_control_16, core, idact_enable, 0);
	MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_zero_idac, 1);
	MOD_PHYREGCE(pi, dccal_control_14, core, dcoe_force_zero_ovrride, 0);

	MOD_PHYREGCE(pi, dccal_control_17, core, dcctrigger_wait_cinit, 28);

	MOD_PHYREGCE(pi, dccal_control_18, core, farrow_blankctr_init, 40);
	MOD_PHYREGCE(pi, dccal_control_18, core, rssi_blank_enable_during_dcc, 1);
	MOD_PHYREGCE(pi, dccal_control_18, core, dccal_blank_enable_farrow_in, 1);

	// set override registers
	WRITE_PHYREGCE(pi, dccal_control_29, core, 0);
	WRITE_PHYREGCE(pi, dccal_control_30, core, 0);
	WRITE_PHYREGCE(pi, dccal_control_31, core, 0);
	WRITE_PHYREGCE(pi, dccal_control_32, core, 0);

	// dcoe gain map
	if (!ACMAJORREV_129(pi->pubpi->phy_rev)) {
		MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_num_entries, 32);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcc_tia_num_entries, 8);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcc_lpf_num_entries, 4);

		MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_0, 0);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_1, 1);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_2, 2);
		MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_3, 3);
		MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_4, 0);
		MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_5, 0);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_0, 0);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_1, 1);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_2, 2);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_3, 3);
		MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_4, 3);
		MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_5, 3);

		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_0, 0);
		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_1, 1);
		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_2, 2);
		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_3, 3);
		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_4, 4);
		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_5, 5);

		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_6, 6);
		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_7, 15);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_8, 0);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_9, 0);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_10, 0);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_11, 0);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_0, 0);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_1, 1);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_2, 2);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_3, 3);
		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_4, 4);
		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_5, 5);

		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_6, 6);
		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_7, 7);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_8, 7);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_9, 7);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_10, 7);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_11, 7);
		MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_12, 7);
		MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_13, 7);
		MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_14, 7);
	} else {
		MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_num_entries, 36);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcc_tia_num_entries, 9);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcc_lpf_num_entries, 4);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_0, 0);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_1, 1);
		MOD_PHYREGCE(pi, dccal_control_33, core, dcoe_lpf_map_2, 2);
		MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_3, 3);
		MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_4, 0);
		MOD_PHYREGCE(pi, dccal_control_34, core, dcoe_lpf_map_5, 0);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_0, 0);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_1, 1);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_2, 2);
		MOD_PHYREGCE(pi, dccal_control_42, core, dcoe_lpf_inv_map_3, 3);
		MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_4, 3);
		MOD_PHYREGCE(pi, dccal_control_43, core, dcoe_lpf_inv_map_5, 3);

		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_0, 1);
		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_1, 2);
		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_2, 3);
		MOD_PHYREGCE(pi, dccal_control_35, core, dcoe_tia_map_3, 4);
		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_4, 5);
		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_5, 6);
		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_6, 7);
		MOD_PHYREGCE(pi, dccal_control_36, core, dcoe_tia_map_7, 8);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_8, 15);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_9, 0);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_10, 0);
		MOD_PHYREGCE(pi, dccal_control_37, core, dcoe_tia_map_11, 0);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_0, 0);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_1, 0);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_2, 1);
		MOD_PHYREGCE(pi, dccal_control_38, core, dcoe_tia_inv_map_3, 2);
		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_4, 3);
		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_5, 4);
		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_6, 5);
		MOD_PHYREGCE(pi, dccal_control_39, core, dcoe_tia_inv_map_7, 6);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_8, 7);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_9, 7);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_10, 7);
		MOD_PHYREGCE(pi, dccal_control_40, core, dcoe_tia_inv_map_11, 7);
		MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_12, 7);
		MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_13, 7);
		MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_14, 7);
		MOD_PHYREGCE(pi, dccal_control_41, core, dcoe_tia_inv_map_15, 8);
	}

	// dcoe init regs
	MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_inpshort, 0);
	MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_outshort, 1);
	MOD_PHYREGCE(pi, dccal_control,    core, dcoe_lna1_init, 0);
	MOD_PHYREGCE(pi, dccal_control,    core, dcoe_wait_cinit, 24);
	MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_zero_idac, 1);
	MOD_PHYREGCE(pi, dccal_control_8,  core, dcoe_acc_cexp, 4);

	// clear dcoe done
	if (!ACMAJORREV_129(pi->pubpi->phy_rev)) {
		MOD_PHYREGCE(pi, dccal_control_49, core, ld_dcoe_done_0, 0xf);
	} else {
		MOD_PHYREGCE(pi, dccal_control_49, core, ld_dcoe_done_0, 0x0);
	}
	MOD_PHYREGCE(pi, dccal_control_48, core, ld_dcoe_done_1, 0x0);
	MOD_PHYREGCE(pi, dccal_control_47, core, ld_dcoe_done_0, 0x0);
	MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 1);
	MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0);

	// idacc init regs
	MOD_PHYREGCE(pi, dccal_control_7,  core, idacc_wait_cinit, 32);
	MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_mag_select, 2);
	MOD_PHYREGCE(pi, dccal_control_1,  core, idacc_tia_init_00, tia_init);
	MOD_PHYREGCE(pi, dccal_control_1,  core, idacc_tia_init_01, tia_init);
	MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_02, tia_init);
	MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_03, tia_init);
	MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_04, tia_init);
	MOD_PHYREGCE(pi, dccal_control_2,  core, idacc_tia_init_05, tia_init);
	MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_06, tia_init);
	MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_07, tia_init);
	MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_08, tia_init);
	MOD_PHYREGCE(pi, dccal_control_3,  core, idacc_tia_init_09, tia_init);
	MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_10, tia_init);
	MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_11, tia_init);
	MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_12, tia_init);
	MOD_PHYREGCE(pi, dccal_control_4,  core, idacc_tia_init_13, tia_init);

	PHY_CAL(("PAPD special dccal: core =%d, tia_init=%d\n", core, tia_init));

	MOD_PHYREGCE(pi, dccal_control_50, core, tia_casc_idx, 10);
	MOD_PHYREGCE(pi, dccal_control_50, core, tia_casc_map, 6);

	MOD_PHYREGCE(pi, dccal_control_50, core, biq0_gidx_init, 0);

	MOD_PHYREGCE(pi, dccal_control_14, core, multi_clip_dcc_tia_war, 0);

	// clear idacc done
	MOD_PHYREGCE(pi, dccal_control_26, core, idacc_done_init_0, 0xe000);
	MOD_PHYREGCE(pi, dccal_control_27, core, idacc_done_init_1, 0xff);
	MOD_PHYREGCE(pi, dccal_control_23, core, ld_idac_cal_done_0, 0xe000);
	MOD_PHYREGCE(pi, dccal_control_22, core, ld_idac_cal_done_1, 0xff);
	MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 1);
	MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0);

	MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 0);

	MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 0);
	wlc_phy_table_write_acphy(pi,
			AC2PHY_TBL_ID_DCOE_TABLE0 + (core *
			(AC2PHY_TBL_ID_DCOE_TABLE1 - AC2PHY_TBL_ID_DCOE_TABLE0)),
			36, 0, 32, zeros);
	wlc_phy_table_write_acphy(pi,
			AC2PHY_TBL_ID_IDAC_TABLE0 + (core *
			(AC2PHY_TBL_ID_IDAC_TABLE1 - AC2PHY_TBL_ID_IDAC_TABLE0)),
			24, 0, 32, zeros);
	wlc_phy_table_write_acphy(pi,
			AXPHY_TBL_ID_IDAC_RES_TABLE0 + (core *
			(AXPHY_TBL_ID_IDAC_RES_TABLE1 - AXPHY_TBL_ID_IDAC_RES_TABLE0)),
			24, 0, 32, zeros);
	MOD_PHYREGCE(pi, dccal_control_15, core, tbl_accessClk_sel, 1);

	MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 0);
	MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_i, 0);
	MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_q, 0);

	phy_ax_dccal_digcorr_bwspecific_percore(pi, core);
	phy_ac_dccal_init_tia_percore(pi, core);
	ACPHY_ENABLE_STALL(pi, stall_val);
}

void phy_ax_dccal_digcorr_bwspecific(phy_info_t *pi)
{
	uint8 core;

	if (CHSPEC_IS80(pi->radio_chanspec) || CHSPEC_IS160(pi->radio_chanspec)) {
		FOREACH_CORE(pi, core) {
			if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
				MOD_PHYREGCE(pi, dccal_control_53,  core,
						dcoe_abort_threshold, 1023);
			} else if (ACMAJORREV_129(pi->pubpi->phy_rev) && PHY_IPA(pi) &&
				CHSPEC_IS5G(pi->radio_chanspec)) {
				MOD_PHYREGCE(pi, dccal_control_1,  core, dcoe_abort_threshold, 255);
			} else {
				MOD_PHYREGCE(pi, dccal_control_1,  core, dcoe_abort_threshold, 150);
			}
			if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
				MOD_PHYREGCE(pi, dccal_control_54,  core,
						idacc_abort_threshold, 1023);
			} else {
				MOD_PHYREGCE(pi, dccal_control_8,  core,
						idacc_abort_threshold, 255);
			}
			MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_acc_cexp, 4);
		}
	} else {
		FOREACH_CORE(pi, core) {
			if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
				MOD_PHYREGCE(pi, dccal_control_53,  core,
						dcoe_abort_threshold, 400);
			} else if (ACMAJORREV_129(pi->pubpi->phy_rev) && PHY_IPA(pi) &&
				CHSPEC_IS5G(pi->radio_chanspec)) {
				MOD_PHYREGCE(pi, dccal_control_1,  core, dcoe_abort_threshold, 100);
			} else {
				MOD_PHYREGCE(pi, dccal_control_1,  core, dcoe_abort_threshold, 50);
			}
			if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
				MOD_PHYREGCE(pi, dccal_control_54,  core,
						idacc_abort_threshold, 1023);
				MOD_PHYREGCE(pi, dccal_control_8,  core,
						idacc_acc_cexp, 4);
			} else if (ACMAJORREV_51_129_131(pi->pubpi->phy_rev)) {
				//increase init thresholds to avoid timeouts in 63178.
				//Adding it to 6710 too.
				MOD_PHYREGCE(pi, dccal_control_8,  core,
						idacc_abort_threshold, 255);
				MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_acc_cexp, 4);
			} else {
				MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_abort_threshold, 50);
				MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_acc_cexp, 5);
			}
		}
	}
}

void phy_ax_dccal_digcorr_bwspecific_percore(phy_info_t *pi, uint8 core)
{
	ASSERT(ACMAJORREV_129(pi->pubpi->phy_rev));

	if (CHSPEC_IS80(pi->radio_chanspec)) {
		MOD_PHYREGCE(pi, dccal_control_1,  core, dcoe_abort_threshold, 150);
	} else {
		MOD_PHYREGCE(pi, dccal_control_1,  core, dcoe_abort_threshold, 50);
	}

	MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_abort_threshold, 255);
	MOD_PHYREGCE(pi, dccal_control_8,  core, idacc_acc_cexp, 4);
}

void phy_ax_dccal_digcorr_dcoe(phy_info_t  *pi)
{
	uint8 num_rst_retry = 6;
	uint8 cal_done = 0;
	uint8 retry = 0;
	uint8 num_actv_cores = 0;
	uint8 core;
	uint8 phyrxchain;
	uint16 dly;
	BCM_REFERENCE(phyrxchain);
	phyrxchain = phy_stf_get_data(pi->stfi)->phyrxchain;

	//CAL dcoe first
	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 0);
		MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);
		num_actv_cores++;
	}

	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REG_20698(pi, RX2G_REG1, core, rx2g_lo_en, 0);
				MOD_RADIO_REG_20698(pi, RX2G_CFG1_OVR, core, ovr_rx2g_lo_en, 1);
			} else {
				//MOD_RADIO_REG_20698(pi, RX5G_REG5, core, rx5g_mix_pu, 0);
				//MOD_RADIO_REG_20698(pi, RX5G_CFG1_OVR, core, ovr_rx5g_mix_pu, 1);
				MOD_RADIO_REG_20698(pi, LOGEN_CORE_REG0, core, logen_rx_rccr_pu, 0);
				MOD_RADIO_REG_20698(pi, LOGEN_CORE_OVR0, core,
					ovr_logen_rx_rccr_pu, 1);
			}
		} else if (ACMAJORREV_51(pi->pubpi->phy_rev)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				// use mux_pu to power down LO
				MOD_RADIO_REG_20704(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			} else {
				MOD_RADIO_REG_20704(pi, LOGEN_CORE_REG0, core, logen_rx_rccr_pu, 0);
				MOD_RADIO_REG_20704(pi, LOGEN_CORE_OVR0, core,
					ovr_logen_rx_rccr_pu, 1);
				MOD_RADIO_REG_20704(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			}
		} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				// use mux_pu to power down LO
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			} else {
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG0, core, logen_rx_rccr_pu, 0);
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_OVR0, core,
					ovr_logen_rx_rccr_pu, 1);
				MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			}
		} else if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
				// use mux_pu to power down LO
				MOD_RADIO_REG_20708(pi, LOGEN_CORE_OVR0, core,
					ovr_logen_core_mux_pu, 1);
				MOD_RADIO_REG_20708(pi, LOGEN_CORE_REG5, core,
					logen_core_mux_pu, 0);
		} else if (ACMAJORREV_131(pi->pubpi->phy_rev)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				// use mux_pu to power down LO
				MOD_RADIO_REG_20710(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			} else {
				MOD_RADIO_REG_20710(pi, LOGEN_CORE_REG0, core, logen_rx_rccr_pu, 0);
				MOD_RADIO_REG_20710(pi, LOGEN_CORE_OVR0, core,
					ovr_logen_rx_rccr_pu, 1);
				MOD_RADIO_REG_20710(pi, LOGEN_CORE_REG1, core,
					logen_rx_db_mux_pu, 0);
			}
		} else {
			ASSERT(0);
		}
		MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSw5GVal, 1);
		MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSwOvr, 1);
		MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
	}

	// longer wait time for 80/160 to avoid time-outs
	if (ACMAJORREV_129(pi->pubpi->phy_rev) && PHY_IPA(pi) && CHSPEC_IS5G(pi->radio_chanspec)) {
		num_rst_retry = 12;
		if (CHSPEC_IS80(pi->radio_chanspec)) {
			dly = 1000;
		} else {
			dly = 500;
		}
	} else { /* For 63178 and others */
		if (CHSPEC_IS80(pi->radio_chanspec) || CHSPEC_IS160(pi->radio_chanspec)) {
			dly = 700;
		} else {
			dly = 300;
		}
	}

	while ((retry <= num_rst_retry) && (cal_done < num_actv_cores)) {
		cal_done = 0;

		FOREACH_ACTV_CORE(pi, phyrxchain, core) {
			// toggle the override_dcoe_done bit to reset dcoe_done
			MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 1);
			MOD_PHYREGCE(pi, dccal_control_13, core, override_dcoe_done, 0);
		}
		MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x1);
		OSL_DELAY(10);
		MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x0);
		OSL_DELAY(dly);

		FOREACH_ACTV_CORE(pi, phyrxchain, core) {
			if (READ_PHYREGFLDCE(pi, dccal_control_44, core, dcoe_done_0) == 0xffff &&
			READ_PHYREGFLDCE(pi, dccal_control_45, core, dcoe_done_1) == 0xffff &&
			READ_PHYREGFLDCE(pi, dccal_control_46, core, dcoe_done_2) == 0xf) {
				cal_done++;
				PHY_CAL(("dccal: dcoe complete for core%d, retried = %d times,"
					" cal_done = %d\n", core, retry, cal_done));
			} else {
				if (retry == num_rst_retry) {
					PHY_CAL(("dccal: dcoe timeout core%d at the final trial\n",
						core));
					// enable the DC fixed corr back since DCOE failed
					MOD_PHYREG(pi, DcFiltAddress, bypass_HT_VHT_HE_STF, 0);
				} else {
					PHY_CAL(("dccal: dcoe temporary timeout core%d."
						" Retrying...\n", core));
				}
			}
		}
		retry++;
	}

	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 1);
	}

	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		if (ACMAJORREV_47(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20698(pi, RX2G_CFG1_OVR, core, ovr_rx2g_lo_en, 0);
			//MOD_RADIO_REG_20698(pi, RX5G_CFG1_OVR, core, ovr_rx5g_mix_pu, 0);
			MOD_RADIO_REG_20698(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 0);
		} else if (ACMAJORREV_51(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20704(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 0);
			MOD_RADIO_REG_20704(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		} else if (ACMAJORREV_129(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20707(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 0);
			MOD_RADIO_REG_20707(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		} else if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20708(pi, LOGEN_CORE_OVR0, core, ovr_logen_core_mux_pu, 1);
			MOD_RADIO_REG_20708(pi, LOGEN_CORE_REG5, core, logen_core_mux_pu, 1);
			MOD_PHYREGCE(pi, dccal_control_7, core, dccal_clkgate_en, 1);
		} else if (ACMAJORREV_131(pi->pubpi->phy_rev)) {
			MOD_RADIO_REG_20710(pi, LOGEN_CORE_OVR0, core, ovr_logen_rx_rccr_pu, 0);
			MOD_RADIO_REG_20710(pi, LOGEN_CORE_REG1, core, logen_rx_db_mux_pu, 1);
		} else {
			ASSERT(0);
		}
		MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSwOvr, 0);
	}
}

void phy_ax_dccal_digcorr_idacc(phy_info_t  *pi)
{
	uint8 num_rst_retry = 6;
	uint8 cal_done = 0;
	uint8 retry = 0;
	uint8 num_actv_cores = 0;
	uint8 core;
	uint8 phyrxchain;
	uint16 dly;
	BCM_REFERENCE(phyrxchain);
	phyrxchain = phy_stf_get_data(pi->stfi)->phyrxchain;

	//CAL idac
	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_16, core, dcoe_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 0);
		num_actv_cores++;
	}

	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSw5GVal, 1);
		MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSwOvr, 1);
		MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);
	}

	// longer wait time for 80/160 to avoid time-outs
	if (ACMAJORREV_129(pi->pubpi->phy_rev) && PHY_IPA(pi) && CHSPEC_IS5G(pi->radio_chanspec)) {
		num_rst_retry = 12;
		if (CHSPEC_IS80(pi->radio_chanspec)) {
			dly = 1000;
		} else {
			dly = 500;
		}
	} else { /* For 63178 and others */
		if (CHSPEC_IS80(pi->radio_chanspec) || CHSPEC_IS160(pi->radio_chanspec)) {
			dly = 500;
		} else {
			dly = 300;
		}
	}

	while ((retry <= num_rst_retry) && (cal_done < num_actv_cores)) {
		cal_done = 0;

		FOREACH_ACTV_CORE(pi, phyrxchain, core) {
			// toggle the override_idac_cal_done bit to reset dcoe_done
			MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 1);
			MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0);
		}
		MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x1);
		OSL_DELAY(10);
		MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x0);
		OSL_DELAY(dly);

		FOREACH_ACTV_CORE(pi, phyrxchain, core) {
			if (READ_PHYREGFLDCE(pi, dccal_control_21, core,
					idac_cal_done_0) == 0xffff &&
				READ_PHYREGFLDCE(pi, dccal_control_22, core,
					idac_cal_done_1) == 0xff) {
				cal_done++;
				PHY_CAL(("dccal: idacc complete for core %d, retried = %d times,"
					" cal_done = %d\n",	core, retry, cal_done));
			} else {
				if (retry == num_rst_retry) {
					PHY_CAL(("dccal: idacc timeout core%d at the final trial\n",
						core));
					// enable the DC fixed corr back since IDAC cal failed
					MOD_PHYREG(pi, DcFiltAddress, bypass_HT_VHT_HE_STF, 0);
				} else {
					PHY_CAL(("dccal: idacc temporary timeout core%d."
						" Retrying...\n", core));
				}
			}
		}
		retry++;
	}

	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);
	}

	FOREACH_ACTV_CORE(pi, phyrxchain, core) {
		MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSwOvr, 0);
		if (ACMAJORREV_130(pi->pubpi->phy_rev)) {
			MOD_PHYREGCE(pi, dccal_control_7, core, dccal_clkgate_en, 1);
		}
	}
}

void phy_ax_dccal_digcorr_idacc_override(phy_info_t  *pi, uint8 core)
{
	uint8 num_rst_retry = 6;
	uint8 cal_done = 0;
	uint8 retry = 0;
	uint8 num_actv_cores = 0;
	uint16 dly;
	uint16 id;
	uint16 idac0_q, idac0_i;
	uint32 idac0;
	uint8 coree;

	//CAL idac
	FOREACH_CORE(pi, coree) {
		MOD_PHYREGCE(pi, dccal_control_16, coree, dcoe_bypass, 1);
		MOD_PHYREGCE(pi, dccal_control_14, coree, idacc_bypass, 0);
	}

	num_actv_cores++;

	MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSw5GVal, 1);
	MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSwOvr, 1);
	MOD_PHYREGCE(pi, dccal_control_7,  core, dccal_clkgate_en, 0x0);

	// longer wait time for 80/160 to avoid time-outs
	if (CHSPEC_IS80(pi->radio_chanspec) || CHSPEC_IS160(pi->radio_chanspec)) {
		dly = 500;
	} else {
		dly = 300;
	}

	while ((retry <= num_rst_retry) && (cal_done < num_actv_cores)) {
		cal_done = 0;

		// toggle the override_idac_cal_done bit to reset dcoe_done
		MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 1);
		MOD_PHYREGCE(pi, dccal_control_16, core, override_idac_cal_done, 0);

		MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x1);
		OSL_DELAY(10);
		MOD_PHYREG(pi, RxControl, dbgpktprocReset, 0x0);
		OSL_DELAY(dly);

		if (READ_PHYREGFLDCE(pi, dccal_control_21, core,
				idac_cal_done_0) == 0xffff &&
			READ_PHYREGFLDCE(pi, dccal_control_22, core,
				idac_cal_done_1) == 0xff) {
			cal_done++;
			PHY_CAL(("dccal: idacc complete for core %d, retried = %d times,"
				" cal_done = %d\n",	core, retry, cal_done));
		} else {
			if (retry == num_rst_retry) {
				PHY_CAL(("dccal: idacc timeout core%d at the final trial\n",
					core));
				// enable the DC fixed corr back since IDAC cal failed
				MOD_PHYREG(pi, DcFiltAddress, bypass_HT_VHT_HE_STF, 0);
			} else {
				PHY_CAL(("dccal: idacc temporary timeout core%d."
					" Retrying...\n", core));
			}
		}
		retry++;
	}

	MOD_PHYREGCE(pi, dccal_control_14, core, idacc_bypass, 1);
	MOD_PHYREGCE(pi, RfCtrlCoreITRCtrl, core, lnaKillSwOvr, 0);

	id = AC2PHY_TBL_ID_IDAC_TABLE0 + (core * (AC2PHY_TBL_ID_IDAC_TABLE1
		- AC2PHY_TBL_ID_IDAC_TABLE0));
	wlc_phy_table_read_acphy(pi, id, 1, 0, 32, &(idac0));
	idac0_i = ((idac0 >> 10) & 0x3ff);
	idac0_q = (idac0 & 0x3ff);
	if (idac0_i >= 512) {
		idac0_i = 1024 - (idac0_i - 512) - 1;
	}
	if (idac0_q >= 512) {
		idac0_q = 1024 - (idac0_q - 512) - 1;
	}
	PHY_CAL(("PAPD special dccal: core =%d, idac0_i=%d, idac0_q=%d\n", core, idac0_i, idac0_q));

	MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_pu, 1);
	MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_scale, 6);
	MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG1, core, tia_dcdac_i, idac0_i);
	MOD_RADIO_REG_20707(pi, TIA_DCDAC_REG2, core, tia_dcdac_q, idac0_q);
	MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_pu, 1);
	MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_i, 1);
	MOD_RADIO_REG_20707(pi, TIA_CFG1_OVR, core, ovr_tia_dcdac_q, 1);

}
