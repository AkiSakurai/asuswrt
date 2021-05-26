/*
 * ACPHY 20708 Radio PLL configuration
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <phy_dbg.h>
#include <phy_mem.h>
#include <bcm_math.h>
#include <phy_utils_reg.h>
#include <phy_ac_info.h>
#include <wlc_radioreg_20708.h>
#include <phy_ac_pllconfig_20708.h>

#define DBG_PLL 0

/* Fixed point constant defines */
/* 2.0 in 1.15.16 */
#define RFPLL_VCOCAL_DELAYEND_FX        0x20000
/* 10.0 in 1.15.16 */
#define RFPLL_VCOCAL_DELAYSTARTCOLD_FX  0xA0000
/* 1.0 in 1.15.16 */
#define RFPLL_VCOCAL_DELAYSTARTWARM_FX  0x10000
/* 1.0 in 1.15.16 */
#define RFPLL_VCOCAL_PAUSECNT_FX        0x10000
/* 2.0 in 1.15.16 */
#define RFPLL_VCOCAL_XTALCOUNT_FX       0x20000

/* 250.00 in 0.16.16 */
#define CONST_250_00_FX                 0xfa0000
/* 1/250.00 in 0.8.24 */
#define CONST_250_00_INV_FX             0x10625
/* 200.00 in 0.16.16 */
#define CONST_200_00_FX                 0xc80000
/* 1/200.00 in 0.8.24 */
#define CONST_200_00_INV_FX             0x147ae
/* 254.296 in 0.16.16 */
#define CONST_254_296_FX                0xfe4bc7
/* 1/253.775 in 0.8.24 */
#define CONST_253_775_INV_FX            0x1023f
/* 203.0974 in 0.16.16 */
#define CONST_203_0974_FX               0xcb18ef
/* 1/203.0098 in 0.8.24 */
#define CONST_203_0098_INV_FX           0x142d2
/* 203.4368 in 0.16.16 */
#define CONST_203_4368_FX               0xcb6fd2
/* 1/203.0200 in 0.8.24 */
#define CONST_203_0200_INV_FX           0x142ce
/* 10.00 in 0.16.16 */
#define CONST_10_00_FX                  0xa0000
/* 15.048 in 0.16.16 */
#define CONST_15_048_FX                 0xf0c4a
/* 20.00 in 0.16.16 */
#define CONST_20_00_FX                  0x140000
/* 29.956 in 0.16.16 */
#define CONST_29_956_FX                 0x1df4bc
/* 47.636 in 0.16.16 */
#define CONST_47_636_FX                 0x2fa2d1
/* 48.392 in 0.16.16 */
#define CONST_48_392_FX                 0x30645a
/* 0.10 in 0.8.24 */
#define CONST_0_10_FX                   0x19999a
/* 0.0998 in 0.8.24 */
#define CONST_0_0998_FX                 0x198b37
/* 0.05 in 0.8.24 */
#define CONST_0_05_FX                   0xccccd
/* 0.0489 in 0.8.24 */
#define CONST_0_0489_FX                 0xc8462
/* 0.0508 in 0.8.24 */
#define CONST_0_0508_FX                 0xd02c2
/* 0.0509 in 0.8.24 */
#define CONST_0_0509_FX                 0xd04ca
/* 3e9 in (0.32.0) */
#define CONST_3E9_FX                    0xb2d05e00
/* 0.5 in (0.32.0) */
#define CONST_0_5_FX                    0x80000000
/* 0.75 in (0.32.0) */
#define CONST_0_75_FX                   0xc0000000
/* 0.525 in (0.32.0) */
#define CONST_0_525_FX                  0x86666666
/* 0.45 in (0.32.0) */
#define CONST_0_45_FX                   0x73333333
/* 2/3 in (0.2.30) */
#define CONST_2_OVER_3_FX               0x2aaaaaab
/* 3/2 in (0.2.30) */
#define CONST_3_OVER_2_FX               0x60000000
/* 4/3 in (0.2.30) */
#define CONST_4_OVER_3_FX               0x55555555
/* 10/3 in (0.2.30) */
#define CONST_10_OVER_3_FX              0xd5555555

/* Band dependent constants */
#define VCO_CAL_CAP_BITS 11

/* const for wn calculation
 * damp = 1
 * f3db_ovr_fn = sqrt(1+2* damp**2+sqrt(2+4*damp**2+4*damp**4))
 * wn = 2*pi * loop_bw / f3db_ovr_fn
 * wn_const = 2*pi / f3db_ovr_fn
 * wn = wn_const * loop_bw
 */
#define WN_CONST_FX  0xa1fd8938  /* 2.5310996100480279 in (0.2.30) */

/* const for Kvco calculation
 * Kvco = 2 * pi**2 * 1.58 * 1e-10 * 0.00338811 * Fvco**3 * $rfpll_vco_cvar
 *      = ((2 * pi**2 * 1.58 * 1e-10 * 0.00338811)**(1/3) * Fvco)**3 * rfpll_vco_cvar
 * Kvco_const = (2 * pi**2 * 1.58 * 1e-10 * 0.00338811)**(1/3);
 * Kvco = (Kvco_const * VCO_frequency)**3 * $rfpll_vco_cvar
 */
#define KVCO_CONST_FX 0xe6195   /* 2.1943945e-04 in (0.0.32) */

/* rfpll_vco_ctail_top = round((pow((2 * $VCO_frequency),(-2)) * 11.2205e9) - 31.46667
 * rfpll_vco_ctail_top = (sqrt(0.25*11.2205e9)/$VCO_frequency)^2 - 31.46667
 * rfpll_vco_ctail_top = (ctail_const1/$VCO_frequency)^2 - ctail_const2
 *   with ctail_const1 = 52963.4308,  ctail_const2 = 31.46667;
 */
#define CTAIL_CONST1_FX 0xcee36e48  /* 52963.4308 (0.16.16) */
#define CTAIL_CONST2_FX 0x7ddddebe  /* 31.46667 (0.6.26) */

/* Constants */
#define RFPLL_VCOCAL_TARGETCOUNT                0

#define RFPLL_VCOCAL_UPDATESEL_DEC              1

/* PLL loop bw in kHz */
#define LOOP_BW     1250
#define USE_DOUBLER 3
#define DS_SELECT   0 /* DS_SELECT: 0->MASH; 1->SSMF; */

/* No of fraction bits */
#define NF0      0
#define NF8      8
#define NF16    16
#define NF18    18
#define NF20    20
#define NF21    21
#define NF24    24
#define NF25    25
#define NF26    26
#define NF28    28
#define NF30    30
#define NF32    32

#define PRINT_PLL_CONFIG_20708(pi, pll_struct, offset) \
	printf("%s = %u\n", #offset, \
	((phy_utils_read_radioreg(pi, pll_struct->reg_addr[IDX_20708_##offset]) & \
	pll_struct->reg_field_mask[IDX_20708_##offset]) >> \
	pll_struct->reg_field_shift[IDX_20708_##offset]))

#define PLL_CONFIG_20708_VAL_ENTRY(pll_struct, offset, val) \
	pll_struct->reg_field_val[IDX_20708_##offset] = val

#define PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll_struct, offset, reg, fld) \
	pll_struct->pll0_reg_addr[IDX_20708_##offset] = RADIO_PLLREG_20708(pi, reg, 0); \
	pll_struct->pll1_reg_addr[IDX_20708_##offset] = RADIO_PLLREG_20708(pi, reg, 1); \
	pll_struct->reg_field_mask[IDX_20708_##offset] = \
			RF_##20708##_##reg##_##fld##_MASK(pi->pubpi->radiorev); \
	pll_struct->reg_field_shift[IDX_20708_##offset] = \
			RF_##20708##_##reg##_##fld##_SHIFT(pi->pubpi->radiorev);

typedef enum {
	IDX_20708_RFPLL_CP_IOFF_EXTRA,
	IDX_20708_RFPLL_FRCT_DITH_SEL,
	IDX_20708_RFPLL_FRCT_LSB_SEL,
	IDX_20708_RFPLL_FRCT_MASH_SEL,
	IDX_20708_RFPLL_FRCT_STOP_MOD,
	IDX_20708_RFPLL_FRCT_PU,
	IDX_20708_OVR_RFPLL_FRCT_PU,
	IDX_20708_RFPLL_RST_N_FROM_PAD,
	IDX_20708_RFPLL_RST_N_XTL,
	IDX_20708_OVR_RFPLL_VCOCAL_XTALCOUNT,
	IDX_20708_OVR_RFPLL_VCOCAL_RST_N,
	IDX_20708_SEL_BAND,
	IDX_20708_RFPLL_VCOCAL_XTALCOUNT,
	IDX_20708_RFPLL_VCOCAL_DELAYEND,
	IDX_20708_RFPLL_VCOCAL_DELAYSTARTCOLD,
	IDX_20708_RFPLL_VCOCAL_DELAYSTARTWARM,
	IDX_20708_RFPLL_VCOCAL_MSBMAX,
	IDX_20708_RFPLL_VCOCAL_MSBSEL,
	IDX_20708_RFPLL_VCOCAL_MSBBACKOFF,
	IDX_20708_RFPLL_VCOCAL_MAXITERATION,
	IDX_20708_RFPLL_VCOCAL_OVERLAP,
	IDX_20708_RFPLL_VCOCAL_INITCAP,
	IDX_20708_RFPLL_VCOCAL_PAUSECNT,
	IDX_20708_RFPLL_VCOCAL_ROUNDLSB,
	IDX_20708_RFPLL_VCOCAL_TARGETCOUNT,
	IDX_20708_RFPLL_VCOCAL_SLOPE,
	IDX_20708_RFPLL_VCOCAL_CALCAPRBMODE,
	IDX_20708_RFPLL_VCOCAL_TESTVCOCNT,
	IDX_20708_RFPLL_VCOCAL_FORCE_CAPS_OVR,
	IDX_20708_RFPLL_VCOCAL_FORCE_CAPS_OVRVAL,
	IDX_20708_RFPLL_VCOCAL_FAST_SETTLE_OVR,
	IDX_20708_RFPLL_VCOCAL_FAST_SETTLE_OVRVAL,
	IDX_20708_RFPLL_VCOCAL_FORCE_VCTRL_OVR,
	IDX_20708_RFPLL_VCOCAL_FORCE_VCTRL_OVRVAL,
	IDX_20708_RFPLL_VCO_AUX_CAP,
	IDX_20708_RFPLL_VCO_CORE1_EN,
	IDX_20708_RFPLL_VCO_CVAR,
	IDX_20708_RFPLL_VCO_ALC_REF_CTRL,
	IDX_20708_RFPLL_VCO_TEMPCO_DCADJ,
	IDX_20708_RFPLL_VCO_TEMPCO,
	IDX_20708_OVR_RFPLL_FRCT_WILD_BASE,
	IDX_20708_RFPLL_FRCT_WILD_BASE_HIGH,
	IDX_20708_RFPLL_FRCT_WILD_BASE_LOW,
	IDX_20708_RFPLL_FRCTLSB,
	IDX_20708_RFPLL_PFD_EN,
	IDX_20708_RFPLL_LF_EXTVCBIN,
	IDX_20708_RFPLL_CP_IDUMP_OFFSET_UP,
	IDX_20708_RFPLL_LF_LF_RS_CM,
	IDX_20708_RFPLL_LF_LF_RF_CM,
	IDX_20708_RFPLL_LF_LF_R2,
	IDX_20708_RFPLL_LF_LF_R3,
	IDX_20708_RFPLL_LF_LF_C1,
	IDX_20708_RFPLL_LF_LF_C2,
	IDX_20708_RFPLL_LF_LF_C3,
	IDX_20708_RFPLL_LF_LF_C4,
	IDX_20708_RFPLL_CP_IOUT_BIAS,
	IDX_20708_RFPLL_CP_IOFF_BIAS,
	IDX_20708_RFPLL_CP_KPD_SCALE,
	IDX_20708_RFPLL_CP_IOFF,
	IDX_20708_RFPLL_VCO_CTAIL_TOP,
	IDX_20708_RFPLL_OVR_MOD_SYNC,
	PLL_CONFIG_20708_ARRAY_SIZE
} pll_config_20708_offset_t;

static void phy_ac_radio20708_write_pll_config(phy_info_t *pi, pll_config_20708_tbl_t *pll,
		uint8 pll_num);
static void BCMATTACHFN(phy_ac_radio20708_pll_config_const_calc)(phy_info_t *pi,
		pll_config_20708_tbl_t *pll);

#if DBG_PLL != 0
static void print_pll_config_20708(phy_info_t *pi, pll_config_20708_tbl_t *pll, uint32 xtal_freq,
		uint32 lo_freq, uint32 loop_bw, uint32 vco_freq_fx, uint32 kvco_fx);
#endif // endif

void
BCMATTACHFN(phy_ac_radio20708_populate_pll_config_mfree)(phy_info_t *pi,
		pll_config_20708_tbl_t *pll)
{
	if (pll == NULL) {
		return;
	}

	if (pll->pll0_reg_addr != NULL) {
		phy_mfree(pi, pll->pll0_reg_addr, (sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE));
	}

	if (pll->pll1_reg_addr != NULL) {
		phy_mfree(pi, pll->pll1_reg_addr, (sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE));
	}

	if (pll->reg_field_mask != NULL) {
		phy_mfree(pi, pll->reg_field_mask,
				(sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE));
	}

	if (pll->reg_field_shift != NULL) {
		phy_mfree(pi, pll->reg_field_shift,
				(sizeof(uint8) * PLL_CONFIG_20708_ARRAY_SIZE));
	}

	if (pll->reg_field_val != NULL) {
		phy_mfree(pi, pll->reg_field_val, (sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE));
	}

	phy_mfree(pi, pll, sizeof(pll_config_20708_tbl_t));
}

pll_config_20708_tbl_t *
BCMATTACHFN(phy_ac_radio20708_populate_pll_config_tbl)(phy_info_t *pi)
{
	pll_config_20708_tbl_t *pll;

	if ((pll = phy_malloc(pi, sizeof(pll_config_20708_tbl_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc pll_conf failed\n", __FUNCTION__));
		goto fail;
	}

	if ((pll->pll0_reg_addr =
		phy_malloc(pi, (sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE))) == NULL) {
		PHY_ERROR(("%s: phy_malloc pll0_reg_addr failed\n", __FUNCTION__));
		goto fail;
	}

	if ((pll->pll1_reg_addr =
		phy_malloc(pi, (sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE))) == NULL) {
		PHY_ERROR(("%s: phy_malloc pll1_reg_addr failed\n", __FUNCTION__));
		goto fail;
	}

	if ((pll->reg_field_mask =
		phy_malloc(pi, (sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE))) == NULL) {
		PHY_ERROR(("%s: phy_malloc reg_field_mask failed\n", __FUNCTION__));
		goto fail;
	}

	if ((pll->reg_field_shift =
		phy_malloc(pi, (sizeof(uint8) * PLL_CONFIG_20708_ARRAY_SIZE))) == NULL) {
		PHY_ERROR(("%s: phy_malloc reg_field_shift failed\n", __FUNCTION__));
		goto fail;
	}

	if ((pll->reg_field_val =
		phy_malloc(pi, (sizeof(uint16) * PLL_CONFIG_20708_ARRAY_SIZE))) == NULL) {
		PHY_ERROR(("%s: phy_malloc reg_field_val failed\n", __FUNCTION__));
		goto fail;
	}

	/* Register addresses */

	/* ------------------------------------------------------------------- */
	/* This section is the same as 20707 */
	/* ------------------------------------------------------------------- */
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, OVR_RFPLL_VCOCAL_XTALCOUNT,
			PLL_VCOCAL_OVR1, ovr_rfpll_vcocal_XtalCount);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, OVR_RFPLL_VCOCAL_RST_N,
			PLL_VCOCAL_OVR1, ovr_rfpll_vcocal_rst_n);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, SEL_BAND, PLL_MUXSELECT_LINE,
			sel_band);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_XTALCOUNT, PLL_VCOCAL7,
			rfpll_vcocal_XtalCount);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_DELAYEND, PLL_VCOCAL18,
			rfpll_vcocal_DelayEnd);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_DELAYSTARTCOLD, PLL_VCOCAL3,
			rfpll_vcocal_DelayStartCold);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_DELAYSTARTWARM, PLL_VCOCAL4,
			rfpll_vcocal_DelayStartWarm);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_PAUSECNT, PLL_VCOCAL19,
			rfpll_vcocal_PauseCnt);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_ROUNDLSB, PLL_VCOCAL1,
			rfpll_vcocal_RoundLSB);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_CALCAPRBMODE, PLL_VCOCAL7,
			rfpll_vcocal_CalCapRBMode);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_TESTVCOCNT, PLL_VCOCAL1,
			rfpll_vcocal_testVcoCnt);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_FORCE_CAPS_OVR, PLL_VCOCAL2,
			rfpll_vcocal_force_caps_ovr);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_FORCE_CAPS_OVRVAL,
			PLL_VCOCAL2, rfpll_vcocal_force_caps_ovrVal);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_FAST_SETTLE_OVR, PLL_VCOCAL1,
			rfpll_vcocal_fast_settle_ovr);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_FAST_SETTLE_OVRVAL,
			PLL_VCOCAL1, rfpll_vcocal_fast_settle_ovrVal);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_FORCE_VCTRL_OVR, PLL_VCOCAL1,
			rfpll_vcocal_force_vctrl_ovr);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_FORCE_VCTRL_OVRVAL,
			PLL_VCOCAL1, rfpll_vcocal_force_vctrl_ovrVal);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCO_CORE1_EN, PLL_VCO7,
			rfpll_vco_core1_en);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCO_CVAR, PLL_VCO2,
			rfpll_vco_cvar);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCO_ALC_REF_CTRL, PLL_VCO6,
			rfpll_vco_ALC_ref_ctrl);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCO_TEMPCO_DCADJ, PLL_VCO5,
			rfpll_vco_tempco_dcadj);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCO_TEMPCO, PLL_VCO4,
			rfpll_vco_tempco);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, OVR_RFPLL_FRCT_WILD_BASE, PLL_OVR1,
			ovr_rfpll_frct_wild_base);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCT_WILD_BASE_HIGH, PLL_FRCT2,
			rfpll_frct_wild_base_high);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCT_WILD_BASE_LOW, PLL_FRCT3,
			rfpll_frct_wild_base_low);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCTLSB, PLL_FRCT1,
			rfpll_frctlsb);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_PFD_EN, PLL_CFG1,
			rfpll_pfd_en);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_RS_CM, PLL_LF7,
			rfpll_lf_lf_rs_cm);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_RF_CM, PLL_LF7,
			rfpll_lf_lf_rf_cm);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_R2, PLL_LF4,
			rfpll_lf_lf_r2);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_R3, PLL_LF5,
			rfpll_lf_lf_r3);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_C1, PLL_LF2,
			rfpll_lf_lf_c1);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_C2, PLL_LF2,
			rfpll_lf_lf_c2);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_C3, PLL_LF3,
			rfpll_lf_lf_c3);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_LF_C4, PLL_LF3,
			rfpll_lf_lf_c4);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_CP_KPD_SCALE, PLL_CP4,
			rfpll_cp_kpd_scale);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_CP_IOFF, PLL_CP4,
			rfpll_cp_ioff);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_OVR_MOD_SYNC, PLL_CFG1,
			rfpll_ovr_mod_sync);

	/* ------------------------------------------------------------------- */
	/* This section is new for 20708 */
	/* ------------------------------------------------------------------- */
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_MSBMAX, PLL_VCOCAL28,
			rfpll_vcocal_MsbMax);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_MSBSEL, PLL_VCOCAL12,
			rfpll_vcocal_MsbSel);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_MSBBACKOFF, PLL_VCOCAL14,
			rfpll_vcocal_MsbBackoff);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_MAXITERATION,
			PLL_VCOCAL13, rfpll_vcocal_Maxiteration);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_OVERLAP,
			PLL_VCOCAL11, rfpll_vcocal_Overlap);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_INITCAP,
			PLL_VCOCAL12, rfpll_vcocal_InitCap);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_TARGETCOUNT, PLL_VCOCAL6,
			rfpll_vcocal_TargetCount);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCOCAL_SLOPE, PLL_VCOCAL14,
			rfpll_vcocal_Slope);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCO_AUX_CAP, PLL_VCO4,
			rfpll_vco_aux_cap);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_LF_EXTVCBIN, PLL_LF1,
			rfpll_lf_extvcbin);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_CP_IDUMP_OFFSET_UP, PLL_CP3,
			rfpll_cp_idump_offset_up);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_CP_IOUT_BIAS, PLL_CP1,
			rfpll_cp_iout_bias);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_CP_IOFF_BIAS, PLL_CP5,
			rfpll_cp_ioff_bias);

	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_CP_IOFF_EXTRA, PLL_CP5,
			rfpll_cp_ioff_extra);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_VCO_CTAIL_TOP, PLL_VCO6,
			rfpll_vco_ctail_top);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCT_DITH_SEL, PLL_FRCT1,
			rfpll_frct_dith_sel);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCT_LSB_SEL, PLL_FRCT1,
			rfpll_frct_lsb_sel);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCT_MASH_SEL, PLL_FRCT1,
			rfpll_frct_mash_sel);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCT_STOP_MOD, PLL_FRCT1,
			rfpll_frct_stop_mod);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_FRCT_PU, PLL_FRCT1,
			rfpll_frct_pu);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, OVR_RFPLL_FRCT_PU, PLL_OVR1,
			ovr_rfpll_frct_pu);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_RST_N_FROM_PAD, PLL_CFG2,
			rfpll_rst_n_from_pad);
	PLL_CONFIG_20708_REG_INFO_ENTRY(pi, pll, RFPLL_RST_N_XTL, PLL_CFG2,
			rfpll_rst_n_xtl);

	/* Add frequency independent data */
	phy_ac_radio20708_pll_config_const_calc(pi, pll);

	return pll;

fail:
	phy_ac_radio20708_populate_pll_config_mfree(pi, pll);

	return NULL;
}

static void
phy_ac_radio20708_write_pll_config(phy_info_t *pi, pll_config_20708_tbl_t *pll, uint8 pll_num)
{
	uint16 *reg_val;
	uint16 *pll0_reg_addr;
	uint16 *pll1_reg_addr;
	uint16 *reg_mask;
	uint8 *reg_shift;
	uint8 i;

	reg_val = pll->reg_field_val;
	pll0_reg_addr = pll->pll0_reg_addr;
	pll1_reg_addr = pll->pll1_reg_addr;
	reg_mask = pll->reg_field_mask;
	reg_shift = pll->reg_field_shift;

	/* Write to register fields */
	for (i = 0; i < PLL_CONFIG_20708_ARRAY_SIZE; i++) {
		if (pll_num == 1) {
			phy_utils_mod_radioreg(pi, pll1_reg_addr[i], reg_mask[i],
				(reg_val[i] << reg_shift[i]));
		} else { /* pll_num == 0 */
			phy_utils_mod_radioreg(pi, pll0_reg_addr[i], reg_mask[i],
				(reg_val[i] << reg_shift[i]));
		}
	}
}

static void
BCMATTACHFN(phy_ac_radio20708_pll_config_const_calc)(phy_info_t *pi, pll_config_20708_tbl_t *pll)
{
	/* 20708_procs.tcl r774108: 20708_pll_config */
	uint32 xtal_freq;
	uint32 xtal_fx;
	uint64 temp_64;
	uint8  nf;

	/* Default parameters to be used in chan dependent computation */
	pll->use_doubler = USE_DOUBLER;
	pll->loop_bw = LOOP_BW;
	pll->ds_select = DS_SELECT;

	/* ------------------------------------------------------------------- */
	/* XTAL frequency in 0.8.24 */
	xtal_freq = pi->xtalfreq;
	/* <0.32.0> / <0.32.0> -> <0.(32-nf).nf> */
	nf = math_fp_div_64(xtal_freq, 1000000, 0, 0, &xtal_fx);
	/* round(<0.(32-nf).nf>, (nf-24)) -> <0.8.24> */
	pll->xtal_fx = math_fp_round_32(xtal_fx, (nf - NF24));
	xtal_fx = pll->xtal_fx;
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* XTAL dependent */
	/* <0.8.24> * <0.16.16> -> <0.16.16> */
	temp_64 = math_fp_mult_64(xtal_fx, RFPLL_VCOCAL_DELAYEND_FX, NF24, NF16, NF16);
	/* round(0.16.16, 16) -> <0.32.0> */
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_DELAYEND,
			(uint16)math_fp_round_64(temp_64, NF16));

	/* <0.8.24> * <0.16.16> -> <0.16.16> */
	temp_64 = math_fp_mult_64(xtal_fx, RFPLL_VCOCAL_DELAYSTARTCOLD_FX, NF24, NF16, NF16);
	/* round(0.16.16, 16) -> <0.32.0> */
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_DELAYSTARTCOLD,
			(uint16)math_fp_round_64(temp_64, NF16));

	/* <0.8.24> * <0.16.16> -> <0.16.16> */
	temp_64 = math_fp_mult_64(xtal_fx, RFPLL_VCOCAL_DELAYSTARTWARM_FX, NF24, NF16, NF16);
	/* round(0.16.16, 16) -> <0.32.0> */
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_DELAYSTARTWARM,
			(uint16)math_fp_round_64(temp_64, NF16));

	/* <0.8.24> * <0.16.16> -> <0.16.16> */
	temp_64 = math_fp_mult_64(xtal_fx, RFPLL_VCOCAL_PAUSECNT_FX, NF24, NF16, NF16);
	/* round(0.16.16, 16) -> <0.32.0> */
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_PAUSECNT,
			(uint16)math_fp_round_64(temp_64, NF16));

	/* <0.8.24> * <0.16.16> -> <0.16.16> */
	temp_64 = math_fp_mult_64(xtal_fx, RFPLL_VCOCAL_XTALCOUNT_FX, NF24, NF16, NF16);
	/* round(0.16.16, 16) -> <0.32.0> */
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_XTALCOUNT,
			(uint16)math_fp_ceil_64(temp_64, NF16));

	/* ------------------------------------------------------------------- */
	/* Put other register values to structure */
	PLL_CONFIG_20708_VAL_ENTRY(pll, OVR_RFPLL_VCOCAL_XTALCOUNT,        1);
	PLL_CONFIG_20708_VAL_ENTRY(pll, OVR_RFPLL_VCOCAL_RST_N,            1);
	PLL_CONFIG_20708_VAL_ENTRY(pll, SEL_BAND,                          3);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_CALCAPRBMODE,         3);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_TESTVCOCNT,           0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_FORCE_CAPS_OVR,       0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_FORCE_CAPS_OVRVAL,    0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_FAST_SETTLE_OVR,      0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_FAST_SETTLE_OVRVAL,   0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_FORCE_VCTRL_OVR,      0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_FORCE_VCTRL_OVRVAL,   0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_OVERLAP,              0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_MSBMAX,             127);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_MSBSEL,               3);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_MSBBACKOFF,           0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_MAXITERATION,        15);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCO_AUX_CAP,                 0);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCO_CORE1_EN,                1);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_EXTVCBIN,                 4);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_CP_IDUMP_OFFSET_UP,          7);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_CP_IOUT_BIAS,                1);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_CP_IOFF_BIAS,                1);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_OVR_MOD_SYNC,                0);

	if (pll->ds_select == 0) {
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_CP_IOFF_EXTRA,       0);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_DITH_SEL,       0);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_LSB_SEL,        0);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_MASH_SEL,       1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_STOP_MOD,       0);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_PU,             1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, OVR_RFPLL_FRCT_PU,         1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_RST_N_FROM_PAD,      1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_RST_N_XTL,           1);
	} else {
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_CP_IOFF_EXTRA,       1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_DITH_SEL,       0);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_LSB_SEL,        1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_MASH_SEL,       0);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_STOP_MOD,       1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_PU,             0);
		PLL_CONFIG_20708_VAL_ENTRY(pll, OVR_RFPLL_FRCT_PU,         1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_RST_N_FROM_PAD,      1);
		PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_RST_N_XTL,           1);
	}
}

void
wlc_phy_radio20708_pll_tune(phy_info_t *pi, pll_config_20708_tbl_t *pll,
		uint32 lo_freq, uint8 pll_num)
{
	/* 20708_procs.tcl r804993: 20708_pll_config */
	uint32 kvco_fx;
	uint32 pfd_ref_freq_fx;
	uint8  nf, loop1;
	uint32 temp_32;
	uint32 temp_32_1;
	uint32 vco_freq_fx;
	uint32 divide_ratio_fx;
	uint32 ndiv_over_kvco_fx;
	uint16 wildbase_lsb_fx;
	uint16 wildbase_low_fx;
	uint16 wildbase_high_fx;
	uint16 rfpll_pfd_en;
	uint16 refDoubler_pu;
	uint16 ref_freq_exp;
	uint32 wn_fx;
	uint32 loop_band;
	uint32 r1_passive_lf_fx;
	uint32 c1_passive_lf_fx;
	uint16 rfpll_vco_cvar_dec = 0;
	uint16 rfpll_vco_ALC_ref_ctrl_dec = 0;
	uint16 rfpll_vco_tempco_dcadj_dec = 0;
	uint16 rfpll_vco_tempco_dec = 0;
	uint16 rfpll_vcocal_InitCap = 0;
	uint16 rfpll_vcocal_Slope = 0;
	uint16 rfpll_vcocal_TargetCount = 0;
	uint16 rfpll_cp_kpd_scale_ideal = 0;
	uint16 rfpll_cp_ioff_ideal = 0;
	uint16 rfpll_ctail_top_ideal = 0;
	uint32 rX[4] = {0, 0, 0, 0};
	uint32 cX[4] = {0, 0, 0, 0};
	uint32 const_rX_A[4] = {CONST_254_296_FX, CONST_254_296_FX,
			CONST_203_0974_FX, CONST_203_4368_FX};
	uint32 const_rX_B[4] = {CONST_253_775_INV_FX, CONST_253_775_INV_FX,
			CONST_203_0098_INV_FX, CONST_203_0200_INV_FX};
	uint32 const_cX_A[4] = {CONST_15_048_FX, CONST_29_956_FX,
			CONST_47_636_FX, CONST_48_392_FX};
	uint32 const_cX_B[4] = {CONST_0_0998_FX, CONST_0_0489_FX,
			CONST_0_0508_FX, CONST_0_0509_FX};

	/* Refdoubler delay calibration */
	MOD_RADIO_PLLREG_20708(pi, PLL_REFDOUBLER6, 0, RefDoubler_clkdiv, 96);
	MOD_RADIO_PLLREG_20708(pi, PLL_REFDOUBLER5, 0, RefDoubler_cal_pu, 1);
	MOD_RADIO_PLLREG_20708(pi, PLL_REFDOUBLER5, 0, RefDoubler_cal_init, 1);
	MOD_RADIO_PLLREG_20708(pi, PLL_REFDOUBLER5, 0, RefDoubler_cal_init, 0);
	MOD_RADIO_PLLREG_20708(pi, PLL_REFDOUBLER5, 0, RefDoubler_cal_mask, 0);

	/* ------------------------------------------------------------------- */
	/* PFD Reference Frequency */
	/* <0.8.24> * <0.1.0> --> <0.8.24> */
	if (pll->use_doubler == 1) {
		rfpll_pfd_en  = 1;
		refDoubler_pu = 1;
		ref_freq_exp  = 1;
	} else if (pll->use_doubler == 2) {
		rfpll_pfd_en  = 3;
		refDoubler_pu = 0;
		ref_freq_exp  = 1;
	} else if (pll->use_doubler == 3) {
		rfpll_pfd_en  = 3;
		refDoubler_pu = 1;
		ref_freq_exp  = 2;
	} else {
		rfpll_pfd_en  = 1;
		refDoubler_pu = 0;
		ref_freq_exp  = 0;
	}
	pfd_ref_freq_fx = pll->xtal_fx << ref_freq_exp;

	MOD_RADIO_PLLREG_20708(pi, PLL_OVR2, 0, ovr_RefDoubler_pu, 0x1);
	MOD_RADIO_PLLREG_20708(pi, PLL_REFDOUBLER1, 0, RefDoubler_pu, refDoubler_pu);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_PFD_EN, rfpll_pfd_en);

	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* VCO Frequency: 7770 < f < 9450 in (0.14.18) */
	/* In 2G: VCO = LO x 10/3 */
	/* In 5G: VCO = LO x 3/2  */
	/* In 6G: VCO = LO x 4/3  */
	/* <0.13.0> * <0.1.31> --> <0.14.18> */
	vco_freq_fx = (uint32)math_fp_mult_64(
		lo_freq,
		(lo_freq <= 3000)? CONST_10_OVER_3_FX :
		(lo_freq <= 5905)? CONST_3_OVER_2_FX : CONST_4_OVER_3_FX,
		NF0, NF30, NF18);
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* VCO Frequency dependent calculations */
	/* <0.14.18> / <0.8.24> --> <0.(32 - nf).nf> */
	nf = math_fp_div_64(vco_freq_fx, pfd_ref_freq_fx, NF18, NF24, &divide_ratio_fx);
	if (pll->ds_select) {
		/* floor(<0.(32-nf).nf>, (nf-25)) -> 0.7.25 */
		/* please note: if doubler == 0, 7 integer bit is NOT sufficient */
		/* and we need 8 integer bit. However, for all existing 11ax chips, */
		/* we always use doubler >= 1. Therefore, 0.7.25 representation is sufficient */
		divide_ratio_fx = math_fp_round_32(divide_ratio_fx, (nf - NF25));
	} else {
		/* floor(<0.(32-nf).nf>, (nf-21)) -> 0.11.21 */
		divide_ratio_fx = math_fp_round_32(divide_ratio_fx, (nf - NF21));
	}
	/* ------------------------------------------------------------------- */

	/* Frequency range dependent constants */
	if (vco_freq_fx < (7500 << NF18)) {
		rfpll_vcocal_InitCap = 2048;
		rfpll_vcocal_Slope   = 12;
	} else if (vco_freq_fx < (8000 << NF18)) {
		rfpll_vcocal_InitCap = 1536;
		rfpll_vcocal_Slope   = 11;
	} else if (vco_freq_fx < (8400 << NF18)) {
		rfpll_vcocal_InitCap = 1024;
		rfpll_vcocal_Slope   = 9;
	} else if (vco_freq_fx < (8800 << NF18)) {
		rfpll_vcocal_InitCap = 512;
		rfpll_vcocal_Slope   = 8;
	} else if (vco_freq_fx < (9200 << NF18)) {
		rfpll_vcocal_InitCap = 256;
		rfpll_vcocal_Slope   = 7;
	} else {
		rfpll_vcocal_InitCap = 0;
		rfpll_vcocal_Slope   = 7;
	}
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_INITCAP, rfpll_vcocal_InitCap);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_SLOPE, rfpll_vcocal_Slope);

	/* <0.32.0> - <0.32.0> -> <0.32.0> */
	// pll->rfpll_vcocal_RoundLSB = 12 - VCO_CAL_CAP_BITS;
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_ROUNDLSB, (12 - VCO_CAL_CAP_BITS));
	rfpll_vcocal_TargetCount = math_fp_round_32(vco_freq_fx, NF18);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCOCAL_TARGETCOUNT, rfpll_vcocal_TargetCount);

	/* ------------------------------------------------------------------- */
	/* VCO register */
	/* ------------------------------------------------------------------- */

	/* Fract-N (Sigma-Delta) register */
	if (pll->ds_select) {
		temp_32 = divide_ratio_fx >> 25;               // integer part
		temp_32_1 = divide_ratio_fx & 0x1FFFFFF;       // fractional part
		// equivalent to fractional part < 0.5)
		if (temp_32_1 < 0x1000000) {
			temp_32 = temp_32 - 1;
			temp_32_1 = temp_32_1 + 0x2000000;
		}
		temp_32 = (temp_32 << 21) + (temp_32_1 >> 5);

		wildbase_lsb_fx  = temp_32_1       & 0x1F;
		wildbase_low_fx  = temp_32         & 0xFFFF;
		wildbase_high_fx = (temp_32 >> 16) & 0xFFFF;
		divide_ratio_fx  = divide_ratio_fx >> 4; // convert back to 0.11.21 format
	} else {
		wildbase_lsb_fx  = 0;
		wildbase_low_fx  = divide_ratio_fx         & 0xFFFF;
		wildbase_high_fx = (divide_ratio_fx >> 16) & 0xFFFF;
	}
	PLL_CONFIG_20708_VAL_ENTRY(pll, OVR_RFPLL_FRCT_WILD_BASE, 1);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_WILD_BASE_HIGH, wildbase_high_fx);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCT_WILD_BASE_LOW, wildbase_low_fx);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_FRCTLSB, wildbase_lsb_fx);

	loop_band = LOOP_BW;
	rfpll_vco_cvar_dec         = 6;
	rfpll_vco_ALC_ref_ctrl_dec = 13;
	rfpll_vco_tempco_dcadj_dec = 6;
	rfpll_vco_tempco_dec       = 5;
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCO_CVAR, rfpll_vco_cvar_dec);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCO_ALC_REF_CTRL, rfpll_vco_ALC_ref_ctrl_dec);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCO_TEMPCO_DCADJ, rfpll_vco_tempco_dcadj_dec);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCO_TEMPCO, rfpll_vco_tempco_dec);

	/* ------------------------------------------------------------------- */
	/* CP and Loop Filter Registers  */
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* wn = wn_const * loop_bw
	 * wn in <0.12.20> (=2151.435 for loop_band=850)
	 */
	wn_fx = (uint32)math_fp_mult_64(WN_CONST_FX, loop_band, NF30, NF0, NF20);
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* VCO Frequency:  7770 < f < 9450 in (0.14.18)
	 * Kvco_const = (2 * pi**2 * 1.58 * 1e-10 * 0.00338811)**(1/3);
	 * Kvco = (Kvco_const * VCO_frequency)**3 * $rfpll_vco_cvar_dec
	 * Kvco in <0.7.25> ( 46.91 < Kvco < 84.39 )
	 */
	temp_32   = (uint32)math_fp_mult_64(KVCO_CONST_FX, vco_freq_fx, NF32, NF18, NF28);
	temp_32_1 = (uint32)math_fp_mult_64(temp_32, temp_32, NF28, NF28, NF28);
	temp_32   = (uint32)math_fp_mult_64(temp_32, temp_32_1, NF28, NF28, NF28);
	kvco_fx   = (uint32)math_fp_mult_64(temp_32, rfpll_vco_cvar_dec, NF28, NF0, NF25);
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* ndiv_over_kvco = divide_ratio / kvco
	 * ndiv_over_kvco in <0.16.16>  ( 0.56 < ndiv_over_kvco < 0.83 )
	 */
	nf = math_fp_div_64(divide_ratio_fx, kvco_fx, 21, 25, &ndiv_over_kvco_fx);
	ndiv_over_kvco_fx = math_fp_round_32(ndiv_over_kvco_fx, (nf - NF16));
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* r1_passive_lf = 2/3 * wn * ndiv_over_kvco
	 * r1_passive_lf in <0.16.16>
	 */
	temp_32 = (uint32)math_fp_mult_64(wn_fx, CONST_2_OVER_3_FX, NF20, NF30, NF20);
	r1_passive_lf_fx = (uint32)math_fp_mult_64(temp_32, ndiv_over_kvco_fx, NF20, NF16, NF16);

	/* c1_passive_lf = icp / (wn**2 * ndiv_over_kvco) * 1e9
	 * c1_passive_lf in <0.16.16>
	 */
	temp_32 = (uint32)math_fp_mult_64(wn_fx, wn_fx, NF20, NF20, NF8);
	temp_32 = (uint32)math_fp_mult_64(ndiv_over_kvco_fx, temp_32, NF16, NF8, NF8);
	nf = math_fp_div_64(CONST_3E9_FX, temp_32, NF0, NF8, &c1_passive_lf_fx);
	c1_passive_lf_fx = math_fp_round_32(c1_passive_lf_fx, (nf - NF16));
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* settings for rfpll_lf_lf_r2, rfpll_lf_lf_r3, rfpll_lf_lf_rs_cm, rfpll_lf_lf_rf_cm */
	for (loop1 = 0; loop1 < 4; loop1++) {
		if (r1_passive_lf_fx > const_rX_A[loop1]) {
			/* <0.16.16> - <0.16.16> -> <0.16.16> */
			temp_32 = r1_passive_lf_fx - const_rX_A[loop1];
			temp_32 = (uint32)math_fp_mult_64(temp_32, const_rX_B[loop1],
					NF16, NF24, NF0);
		} else {
			temp_32 = 0;
		}
		rX[loop1] = (uint16)LIMIT((int32)temp_32, 0, 31);
	}
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_R2,    rX[0]);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_R3,    rX[1]);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_RS_CM, rX[2]);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_RF_CM, rX[3]);
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* settings for  */
	for (loop1 = 0; loop1 < 4; loop1++) {
		if (c1_passive_lf_fx > const_cX_A[loop1]) {
			/* <0.0.32> - <0.0.32> -> <0.16.16> */
			temp_32 = c1_passive_lf_fx - const_cX_A[loop1];
			temp_32 = (uint32)math_fp_mult_64(temp_32, const_cX_B[loop1],
					NF16, NF24, NF0);
		} else {
			temp_32 = 0;
		}
		cX[loop1] = (uint16)LIMIT((int32)temp_32, 0, 63);
	}
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_C1, cX[0]);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_C2, cX[1]);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_C3, cX[2]);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_LF_LF_C4, cX[3]);
	/* ------------------------------------------------------------------- */

	/* ------------------------------------------------------------------- */
	/* cp_kpd_scale_ideal = round(3*1000/((3+1)*8))
	 * cp_ioff_ideal = round(3*0.6/4*pfd_ref_freq_fx)
	 */
	temp_32 = (uint32)math_fp_mult_64(CONST_0_45_FX, pfd_ref_freq_fx, NF32, NF24, NF0);
	rfpll_cp_ioff_ideal = (uint16)(LIMIT((int32)temp_32, 0, 255));
	rfpll_cp_kpd_scale_ideal = 94;
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_CP_KPD_SCALE, rfpll_cp_kpd_scale_ideal);
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_CP_IOFF, rfpll_cp_ioff_ideal);
	/* ------------------------------------------------------------------- */

	/* rfpll_vco_ctail_top = round((2*$VCO_frequency)^-2*11.2205e9) - 31.46667
	 * rfpll_vco_ctail_top = (sqrt(0.25*11.2205e9)/$VCO_frequency)^2 - 31.46667
	 * rfpll_vco_ctail_top = (ctail_const1/$VCO_frequency)^2 - ctail_const2
	 *   with ctail_const1 = 52963.4308,  ctail_const2 = 31.46667;
	 */
	nf = math_fp_div_64(CTAIL_CONST1_FX, vco_freq_fx, NF16, NF18, &temp_32);
	temp_32 = math_fp_round_32(temp_32, (nf - NF26));
	temp_32 = (uint32)math_fp_mult_64(temp_32, temp_32, NF26, NF26, NF26);
	temp_32 = math_fp_round_32(temp_32 - CTAIL_CONST2_FX, NF26);
	rfpll_ctail_top_ideal = (uint16)(LIMIT((int32)temp_32, 0, 15));
	PLL_CONFIG_20708_VAL_ENTRY(pll, RFPLL_VCO_CTAIL_TOP, rfpll_ctail_top_ideal);

	/* Write computed values to PLL registers */
	phy_ac_radio20708_write_pll_config(pi, pll, pll_num);

#if DBG_PLL != 0
	print_pll_config_20708(pi, pll, pll->xtal_fx, lo_freq, loop_band, vco_freq_fx, kvco_fx);
#endif /* DBG_PLL */

	/* Turn on logen_top and logen_vcobuf (tcl proc does not have them) */
	//MOD_RADIO_PLLREG_20708(pi, LOGEN_REG0, 0, logen_buff_pu, 1);
	//MOD_RADIO_PLLREG_20708(pi, LOGEN_REG0, 0, logen_vcobuf_pu, 1);
}

#if DBG_PLL != 0
static void
print_pll_config_20708(phy_info_t *pi, pll_config_20708_tbl_t *pll, uint32 xtal_freq,
		uint32 lo_freq, uint32 loop_bw, uint32 vco_freq_fx, uint32 kvco_fx)
{
	printf("------------------------------------------------------------\n");
	printf("F_xtal = %d MHz (0.8.24 format)\n", xtal_freq);
	printf("F_LO = %d MHz\n", lo_freq);
	printf("F_vco = %u MHz (0.14.18 format)\n", vco_freq_fx);
	printf("BW = %d kHz\n", loop_bw);
	printf("Kvco = %u MHz/V (0.7.25 format)\n", kvco_fx);
	printf("------------------------------------------------------------\n");

	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_DELAYEND);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_DELAYSTARTCOLD);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_DELAYSTARTWARM);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_PAUSECNT);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_XTALCOUNT);
	PRINT_PLL_CONFIG_20708(pi, pll, OVR_RFPLL_VCOCAL_XTALCOUNT);
	PRINT_PLL_CONFIG_20708(pi, pll, OVR_RFPLL_VCOCAL_RST_N);
	PRINT_PLL_CONFIG_20708(pi, pll, SEL_BAND);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_CALCAPRBMODE);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_TESTVCOCNT);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_FORCE_CAPS_OVR);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_FORCE_CAPS_OVRVAL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_FAST_SETTLE_OVR);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_FAST_SETTLE_OVRVAL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_FORCE_VCTRL_OVR);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_FORCE_VCTRL_OVRVAL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_OVERLAP);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_MSBMAX);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_MSBSEL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_MSBBACKOFF);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_MAXITERATION);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCO_AUX_CAP);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCO_CORE1_EN);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_EXTVCBIN);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_CP_IDUMP_OFFSET_UP);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_CP_IOUT_BIAS);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_CP_IOFF_BIAS);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_OVR_MOD_SYNC);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_INITCAP);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_SLOPE);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_ROUNDLSB);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCOCAL_TARGETCOUNT);
	PRINT_PLL_CONFIG_20708(pi, pll, OVR_RFPLL_FRCT_WILD_BASE);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCT_WILD_BASE_HIGH);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCT_WILD_BASE_LOW);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCTLSB);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_PFD_EN);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCO_CVAR);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCO_ALC_REF_CTRL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCO_TEMPCO_DCADJ);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCO_TEMPCO);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_R2);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_R3);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_RS_CM);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_RF_CM);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_C1);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_C2);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_C3);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_LF_LF_C4);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_CP_KPD_SCALE);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_CP_IOFF);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_CP_IOFF_EXTRA);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_VCO_CTAIL_TOP);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCT_DITH_SEL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCT_LSB_SEL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCT_MASH_SEL);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCT_STOP_MOD);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_FRCT_PU);
	PRINT_PLL_CONFIG_20708(pi, pll, OVR_RFPLL_FRCT_PU);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_RST_N_FROM_PAD);
	PRINT_PLL_CONFIG_20708(pi, pll, RFPLL_RST_N_XTL);
}
#endif /* DBG_PLL */
