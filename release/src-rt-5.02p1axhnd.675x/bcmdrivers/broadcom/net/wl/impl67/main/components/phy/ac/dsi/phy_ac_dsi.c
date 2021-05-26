/*
 * ACPHY DeepSleepInit module
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
 * $Id: phy_ac_dsi.c 783420 2020-01-28 13:00:22Z $
 */

#include <phy_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>

/* PHY common dependencies */
#include <phy_dbg.h>
#include <phy_mem.h>
#include <phy_utils_var.h>
#include <phy_utils_reg.h>
#include <phy_utils_radio.h>

/* PHY type dependencies */
#include <phy_ac.h>
#include <phy_ac_info.h>
#include <wlc_phyreg_ac.h>
#include <wlc_phy_radio.h>
#include <wlc_phytbl_ac.h>

/* DSI module dependencies */
#include <phy_ac_dsi.h>
#include "phy_ac_dsi_data.h"
#include "phy_type_dsi.h"

/* Inter-module dependencies */
#include "phy_ac_radio.h"

#include "fcbs.h"

typedef struct {
	uint8 ds1_napping_enable;
} phy_ac_dsi_params_t;

/* module private states */
struct phy_ac_dsi_info {
	phy_info_t *pi;
	phy_ac_info_t *aci;
	phy_dsi_info_t *di;
	phy_ac_dsi_params_t *dp;
};

typedef struct {
	int num;
	fcbs_input_data_t *data;
} dsi_fcbs_t;

typedef struct {
	int8 blk_num;
	int8 exec_seq_num;
	fcbs_input_data_t *data;
} dsi_radio_fcbs_t;

static const char BCMATTACHDATA(rstr_ds1nap)[] = "ds1_nap";

#define DSI_DBG_PRINTS 0

#define FCBS_DS0_RADIO_PD_BLOCK_AUX (2)
#define FCBS_DS0_RADIO_PU_BLOCK_AUX (3)

/* debug prints */
#if defined(DSI_DBG_PRINTS) && DSI_DBG_PRINTS
#define DSI_DBG(args)	printf args; OSL_DELAY(500);
#else
#define DSI_DBG(args)
#endif /* DSI_DBG_PRINTS */

/* accessor functions */
dsi_fcbs_t * BCMRAMFN(dsi_get_ram_seq)(phy_info_t *pi);
dsi_radio_fcbs_t *BCMRAMFN(dsi_get_radio_pu_dyn_seq)(phy_info_t *pi, int8 ds_idx);

fcbs_input_data_t *BCMRAMFN(dsi_get_radio_pd_seq)(phy_info_t *pi);

/* top level wrappers */

/* Generic Utils */
static void dsi_save_phyregs(phy_info_t *pi, adp_t *input, uint16 len);
static void dsi_save_radioregs(phy_info_t *pi, adp_t *input, uint16 len);
static void dsi_save_phytbls(phy_info_t *pi, phytbl_t *phytbl, uint16 len);
static void dsi_update_radio_dyn_data(phy_info_t *pi, phy_rad_dyn_adp_t *radioreg, uint16 len);
static void dsi_update_phy_dyn_data(phy_info_t *pi, phy_rad_dyn_adp_t *phyreg, uint16 len);
static void dsi_update_radio_seq(phy_info_t *pi, int8 ds_idx, bool *seq_en_flags);

/* Takes a selective snapshot of current PHY and RADIO register space */
static void dsi_update_snapshot(phy_info_t *pi, fcbs_input_data_t *save_seq);

/* PhySpecific save routine */
static void dsi_update_radio_seq_ACMAJORREV_36(phy_info_t *pi, int8 ds_idx, bool *seq_en_flags);

static void update_vco_cal_codes(phy_info_t *pi, fcbs_input_data_t *data);

/* FCBS Dynamic sequence numbers and pointer for maj36_min0 (43012A0) */
dsi_fcbs_t ram_maj36_min0_seq[] = {
	{CHANSPEC_PHY_RADIO, ram_maj36_min0_chanspec_phy_radio},
	{CALCACHE_PHY_RADIO, ram_maj36_min0_calcache_phy_radio},
	{0xff, NULL}
};

/* Radio sequence for DS0 (20695_maj1_min0 - 43012A0) */
dsi_radio_fcbs_t ds0_radio_maj36_min0_seq[] = {
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_MINIPMU_PU,
	dyn_ram_20695_maj1_min0_radio_minipmu_pwr_up},
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_PLL_PU,
	dyn_ram_20695_maj1_min0_radio_2g_pll_pwr_up},
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_PLL_PU,
	dyn_ram_20695_maj1_min0_radio_5g_pll_to_2g_pwr_up},
#ifdef MULTIBAND
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_PLL_PU,
	dyn_ram_20695_maj1_min0_radio_5g_pll_pwr_up},
#endif /* MULTIBAND */
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_CHAN_TUNE,
	ram_20695_maj1_min0_chan_tune}
};

/* FCBS Dynamic sequence numbers and pointer for maj36_min1 (43012B0) */
dsi_fcbs_t ram_maj36_min1_seq[] = {
	{CHANSPEC_PHY_RADIO, ram_maj36_min1_chanspec_phy_radio},
	{CALCACHE_PHY_RADIO, ram_maj36_min1_calcache_phy_radio},
	{0xff, NULL}
};

/* Radio sequence for DS0 (20695_maj2_min0 - 43012B0) */
dsi_radio_fcbs_t ds0_radio_maj36_min1_seq[] = {
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_MINIPMU_PU,
	dyn_ram_20695_maj2_min0_radio_minipmu_pwr_up},
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_PLL_PU,
	dyn_ram_20695_maj2_min0_radio_2g_pll_pwr_up},
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_PLL_PU,
	dyn_ram_20695_maj2_min0_radio_5g_pll_to_2g_pwr_up},
#ifdef MULTIBAND
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_PLL_PU,
	dyn_ram_20695_maj2_min0_radio_5g_pll_pwr_up},
#endif /* MULTIBAND */
	{FCBS_DS0_RADIO_PU_BLOCK, DS0_EXEC_CHAN_TUNE,
	ram_20695_maj2_min0_chan_tune}
};

typedef enum {
	MINI_PMU_PU_OFF = 0,
	PLL_2G_OFF = 1,
	PLL_5G_TO_2G_OFF = 2,
	PLL_5G_OFF = 3,
#ifdef MULTIBAND
	CHAN_TUNE_OFF = 4,
#else
	CHAN_TUNE_OFF = 3
#endif /* MULTIBAND */
} radio_pu_seq_off_t;

typedef enum {
	RADIO_MINI_PMU_PU_OFF = 0,
	RADIO_PLL_PU_VCOCAL_OFF = 1,
	RADIO_CHAN_TUNE_OFF = 2,
	NUM_RADIO_PU_SEQ = 3,
	RADIO_PD_OFF = 3,
	NUM_RADIO_SEQ = 4
} radio_seq_enbl_off_t;

dsi_fcbs_t *
BCMRAMFN(dsi_get_ram_seq)(phy_info_t *pi)
{
	PHY_ERROR(("wl%d %s: Invalid ACMAJORREV!\n", PI_INSTANCE(pi), __FUNCTION__));
	ASSERT(0);

	return NULL;
}

dsi_radio_fcbs_t *
BCMRAMFN(dsi_get_radio_pu_dyn_seq)(phy_info_t *pi, int8 ds_idx)
{
	dsi_radio_fcbs_t *ret_ptr = NULL;

	PHY_ERROR(("wl%d %s: Invalid ACMAJORREV!\n", PI_INSTANCE(pi), __FUNCTION__));
	ASSERT(0);

	return ret_ptr;
}

fcbs_input_data_t *
BCMRAMFN(dsi_get_radio_pd_seq)(phy_info_t *pi)
{
	PHY_ERROR(("wl%d %s: Invalid ACMAJORREV!\n", PI_INSTANCE(pi), __FUNCTION__));
	ASSERT(0);

	return NULL;
}

/* register phy type specific implementation */
phy_ac_dsi_info_t *
BCMATTACHFN(phy_ac_dsi_register_impl)(phy_info_t *pi, phy_ac_info_t *aci, phy_dsi_info_t *di)
{
	phy_ac_dsi_info_t *info;
	phy_type_dsi_fns_t fns;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* allocate all storage together */
	if ((info = phy_malloc(pi, sizeof(phy_ac_dsi_info_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}

	if ((info->dp = phy_malloc(pi, sizeof(phy_ac_dsi_params_t))) == NULL) {
		PHY_ERROR(("%s: phy_malloc failed\n", __FUNCTION__));
		goto fail;
	}

	info->pi = pi;
	info->aci = aci;
	info->di = di;

	/* register PHY type specific implementation */
	bzero(&fns, sizeof(fns));

	fns.ctx = info;

	/* 4339 Prototype Work */
	if (ACMAJORREV_1(pi->pubpi->phy_rev))
		dsi_populate_addr_ACMAJORREV_1(pi);

	phy_dsi_register_impl(di, &fns);

	/* Register DS1 entry call back */

	/* By default, DS1 napping will be disabled */
	info->dp->ds1_napping_enable = (uint8)PHY_GETINTVAR_DEFAULT(pi, rstr_ds1nap, 0);

	return info;

	/* error handling */
fail:
	if (info) {

		if (info->dp)
			phy_mfree(pi, info->dp, sizeof(phy_ac_dsi_params_t));

		phy_mfree(pi, info, sizeof(phy_ac_dsi_info_t));
	}

	return NULL;
}

void
BCMATTACHFN(phy_ac_dsi_unregister_impl)(phy_ac_dsi_info_t *info)
{
	phy_info_t *pi = info->pi;
	phy_dsi_info_t *di = info->di;

	PHY_TRACE(("%s\n", __FUNCTION__));

	/* unregister from common */
	phy_dsi_unregister_impl(di);

	if (info->dp)
		phy_mfree(pi, info->dp, sizeof(phy_ac_dsi_params_t));

	phy_mfree(pi, info, sizeof(phy_ac_dsi_info_t));
}

static void
dsi_update_snapshot(phy_info_t *pi, fcbs_input_data_t *save_seq)
{
	while (save_seq->type != FCBS_TYPE_MAX) {
		switch (save_seq->type) {
			case FCBS_PHY_REG:
				if (save_seq->flags & FCBS_PHY_RADIO_DYNAMIC) {
					dsi_update_phy_dyn_data(pi,
							(phy_rad_dyn_adp_t *)save_seq->data,
							save_seq->data_size);
				} else {
					dsi_save_phyregs(pi, (adp_t *)save_seq->data,
							save_seq->data_size);
				}
				break;

			case FCBS_RADIO_REG:
				if (save_seq->flags & FCBS_PHY_RADIO_DYNAMIC) {
					dsi_update_radio_dyn_data(pi,
							(phy_rad_dyn_adp_t *)save_seq->data,
							save_seq->data_size);
				} else {
					dsi_save_radioregs(pi, (adp_t *)save_seq->data,
							save_seq->data_size);
				}
				break;

			case FCBS_PHY_TBL:
				dsi_save_phytbls(pi, (phytbl_t *)save_seq->data,
						save_seq->data_size);
				break;

			case FCBS_DELAY:
				DSI_DBG(("Skipping delay\n"));
				break;

			default:
				DSI_DBG(("Error! Invalid Type!\n"));
				break;
		}
		/* Move to next element */
		save_seq++;
	}
}

static void
dsi_save_phytbls(phy_info_t *pi, phytbl_t *phytbl, uint16 len)
{
	uint16 i;
	uint8 core = 0;

	/* Read and save phy table */
	for (i = 0; i < len; i++) {
		DSI_DBG(("PHY_TABLE : Tbl_id = 0x%x, Len = 0x%x, Offset = 0x%x, Width = %d\n",
				phytbl[i].id, phytbl[i].len, phytbl[i].offset, phytbl[i].width));

		/* Save table */
		if (phytbl[i].id == ACPHY_TBL_ID_EPSILONTABLE0 ||
				phytbl[i].id == ACPHY_TBL_ID_WBCAL_PTBL0 ||
				phytbl[i].id == ACPHY_TBL_ID_WBCAL_TXDELAY_BUF0 ||
				phytbl[i].id == ACPHY_TBL_ID_ETMFTABLE0) {
			wlc_phy_table_read_acphy_dac_war(pi, phytbl[i].id, phytbl[i].len,
					phytbl[i].offset, phytbl[i].width, phytbl[i].data, core);
		} else {
			wlc_phy_table_read_acphy(pi, phytbl[i].id, phytbl[i].len, phytbl[i].offset,
					phytbl[i].width, phytbl[i].data);
		}
	}
}

static void
dsi_save_phyregs(phy_info_t *pi, adp_t *phyreg, uint16 len)
{
	uint16 i;

	/* Read and save phy reg */
	for (i = 0; i < len; i++) {
		phyreg[i].data = phy_utils_read_phyreg(pi, (uint16)phyreg[i].addr);
		DSI_DBG(("PHY_REG : Addr = 0x%x, Data = 0x%x\n",
				phyreg[i].addr, phyreg[i].data));
	}
}

static void
dsi_update_phy_dyn_data(phy_info_t *pi, phy_rad_dyn_adp_t *phyreg, uint16 len)
{
	uint16 i;
	uint16 read_val;

	/* Read and save phy reg  and use static value as well */
	for (i = 0; i < len; i++) {
		read_val = phy_utils_read_phyreg(pi, (uint16)phyreg[i].addr);
		phyreg[i].data = (read_val & ~phyreg[i].mask) | phyreg[i].static_data;
		DSI_DBG(("PHY_REG : Addr = 0x%x, Mask = 0x%x, Read_val=0x%x, Static_data = 0x%x, "
				"Data = 0x%x\n", phyreg[i].addr, phyreg[i].mask, read_val,
				phyreg[i].static_data, phyreg[i].data));
	}
}

static void
dsi_save_radioregs(phy_info_t *pi, adp_t *radioreg, uint16 len)
{
	uint16 i;

	/* Read and save radio reg */
	for (i = 0; i < len; i++) {
		radioreg[i].data = phy_utils_read_radioreg(pi, (uint16)radioreg[i].addr);
		DSI_DBG(("RADIO_REG : Addr = 0x%x, Data = 0x%x\n",
				radioreg[i].addr, radioreg[i].data));
	}
}

static void
dsi_update_radio_dyn_data(phy_info_t *pi, phy_rad_dyn_adp_t *radioreg, uint16 len)
{
	uint16 i;
	uint16 read_val;

	/* Read and save radio reg  and use static value as well */
	for (i = 0; i < len; i++) {
		read_val = phy_utils_read_radioreg(pi, (uint16)radioreg[i].addr);
		radioreg[i].data = (read_val & ~radioreg[i].mask) | radioreg[i].static_data;
		DSI_DBG(("RADIO_REG : Addr = 0x%x, Mask = 0x%x, Read_val=0x%x, Static_data = 0x%x, "
				"Data = 0x%x\n", radioreg[i].addr, radioreg[i].mask, read_val,
				radioreg[i].static_data, radioreg[i].data));
	}
}

void
dsi_update_radio_seq_ACMAJORREV_36(phy_info_t *pi, int8 ds_idx, bool *seq_en_flags)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	phy_ac_dsi_params_t *param = pi_ac->dsii->dp;

	uint16 stall_val;
	dsi_radio_fcbs_t *radio_pu_seq;
	bool suspend = FALSE;
	fcbs_input_data_t *radio_pd_seq;
	dsi_radio_fcbs_t update_seq[NUM_RADIO_PU_SEQ];
	fcbs_input_data_t *seq_ptr;
	uint8 i;

	BCM_REFERENCE(pi_ac);
	BCM_REFERENCE(param);
	BCM_REFERENCE(radio_pd_seq);

	radio_pu_seq = dsi_get_radio_pu_dyn_seq(pi, ds_idx);

	if (radio_pu_seq == NULL) {
		return;
	}

	radio_pd_seq = dsi_get_radio_pd_seq(pi);

	suspend = !(R_REG(pi->sh->osh, D11_MACCONTROL(pi)) & MCTL_EN_MAC);

	if (!suspend) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		phy_utils_phyreg_enter(pi);
	}

	stall_val = READ_PHYREGFLD(pi, RxFeCtrl1, disable_stalls);

	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, TRUE);
	ACPHY_DISABLE_STALL(pi);

	/* Mini PMU PU sequenc */
	update_seq[RADIO_MINI_PMU_PU_OFF] = radio_pu_seq[MINI_PMU_PU_OFF];

	/* PLL, LOGEN PU and VCO Cal sequence */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		if (phy_ac_radio_get_data(pi_ac->radioi)->use_5g_pll_for_2g) {
			update_seq[RADIO_PLL_PU_VCOCAL_OFF] = radio_pu_seq[PLL_5G_TO_2G_OFF];
		} else {
			update_seq[RADIO_PLL_PU_VCOCAL_OFF] = radio_pu_seq[PLL_2G_OFF];
		}
	} else {
		if (PHY_BAND5G_ENAB(pi)) {
			update_seq[RADIO_PLL_PU_VCOCAL_OFF] = radio_pu_seq[PLL_5G_OFF];
		} else {
			ROMMABLE_ASSERT(FALSE);
		}
	}

	/* Chan specific tune sequence */
	update_seq[RADIO_CHAN_TUNE_OFF] = radio_pu_seq[CHAN_TUNE_OFF];

	/* Update the sequence based on enable flag */
	for (i = 0; i < NUM_RADIO_PU_SEQ; i++) {
		if (seq_en_flags[i] == TRUE) {
			/* Sequence enabled, update */
			dsi_update_snapshot(pi, update_seq[i].data);
			seq_ptr = update_seq[i].data;
		} else {
			/* Sequence not enabled, skip it */
			seq_ptr = (fcbs_input_data_t *)FCBS_DYN_SEQ_SKIP;
		}

#ifdef WL_DSI
		fcbs_add_dynamic_seq(seq_ptr,
			update_seq[i].blk_num, update_seq[i].exec_seq_num);
#endif /* WL_DSI */
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
	phy_rxgcrs_stay_in_carriersearch(pi->rxgcrsi, FALSE);
	if (!suspend) {
		phy_utils_phyreg_exit(pi);
		wlapi_enable_mac(pi->sh->physhim);
	}
}

static void
dsi_update_radio_seq(phy_info_t *pi, int8 ds_idx, bool *seq_en_flags)
{
	/* Call phy specific update routines  */
	if (ACMAJORREV_1(pi->pubpi->phy_rev)) {
		PHY_INFORM(("wl%d %s : There is no dynamic seq in 4335c0\n",
				PI_INSTANCE(pi), __FUNCTION__));
	} else {
		PHY_ERROR(("wl%d %s: Invalid ACMAJORREV!\n", PI_INSTANCE(pi), __FUNCTION__));
		ASSERT(0);
	}
}

#ifdef WL_DSI
void
ds0_radio_seq_update(phy_info_t *pi)
{
	/* Seq order : Minpmu_PU, PLL_PU, Chan_tune, Radio_PD */
	bool init_seq_en[NUM_RADIO_SEQ] = {TRUE, TRUE, TRUE, TRUE};
	bool band_change_seq_en[NUM_RADIO_SEQ] = {FALSE, TRUE, TRUE, FALSE};
	bool chan_change_seq_en[NUM_RADIO_SEQ] = {FALSE, FALSE, TRUE, FALSE};

	bool *seq_en_ptr;
	uint err = 0;
	uint blk_shm_addr, cmd_ptr_shm_addr;
	uint32 cmd_ptr = FCBS_DS0_BM_CMDPTR_BASE_CORE0;
	uint32 data_ptr = FCBS_DS0_BM_DATPTR_BASE_CORE0;

	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	uint16 drv_ucode_if_blk_addr;

	BCM_REFERENCE(blk_shm_addr);
	BCM_REFERENCE(cmd_ptr_shm_addr);

	if (pi->pubpi->slice == DUALMAC_AUX) {
		cmd_ptr = FCBS_DS0_BM_CMDPTR_BASE_CORE1;
		data_ptr = FCBS_DS0_BM_DATPTR_BASE_CORE1;
	}
	/* Driver ucode interface SHM block address */
	drv_ucode_if_blk_addr = wlapi_bmac_read_shm(pi->sh->physhim, M_DRVR_UCODE_IF_PTR(pi));

	/* Radio PU block SHM address */
	blk_shm_addr = (drv_ucode_if_blk_addr * SHM_ENTRY_SIZE) +
			M_FCBS_DS0_RADIO_PU_BLOCK_OFFSET(pi);

	if (CCT_INIT(pi_ac)) {
		seq_en_ptr = init_seq_en;
	} else if (CCT_BAND_CHG(pi_ac)) {
		seq_en_ptr = band_change_seq_en;

		/* Get PLL PU FCBS sequence cmd pointer SHM address */
		cmd_ptr_shm_addr = blk_shm_addr + SHM_ENTRY_SIZE +
				(RADIO_PLL_PU_VCOCAL_OFF * FCBS_SHM_SEQ_SZ);

		/* Read the command and data pointer address and convert it to byte address */
		cmd_ptr = wlapi_bmac_read_shm(pi->sh->physhim, cmd_ptr_shm_addr) << 2;
		data_ptr = wlapi_bmac_read_shm(pi->sh->physhim,
				(cmd_ptr_shm_addr + SHM_ENTRY_SIZE)) << 2;
	} else {
		seq_en_ptr = chan_change_seq_en;

		/* Get Chan tune FCBS sequence cmd pointer SHM address */
		cmd_ptr_shm_addr = blk_shm_addr + SHM_ENTRY_SIZE +
				(RADIO_CHAN_TUNE_OFF * FCBS_SHM_SEQ_SZ);

		/* Read the command and data pointer address and convert it to byte address */
		cmd_ptr = wlapi_bmac_read_shm(pi->sh->physhim, cmd_ptr_shm_addr) << 2;
		data_ptr = wlapi_bmac_read_shm(pi->sh->physhim,
				(cmd_ptr_shm_addr + SHM_ENTRY_SIZE)) << 2;
	}

	/* Reset FCBS cmd and data pointers */
	err = fcbs_reset_cmd_dat_ptrs(wlapi_bmac_get_fcbs_info(pi->sh->physhim),
		FCBS_DS0, cmd_ptr, data_ptr);

	if (err != BCME_OK) {
		PHY_ERROR(("wl%d %s: Failed to reset FCBS cmd/data pts.", PI_INSTANCE(pi),
				__FUNCTION__));
		ASSERT(0);
	}

	/* Update Radio sequences */
	dsi_update_radio_seq(pi, FCBS_DS0, seq_en_ptr);
}
#endif /* WL_DSI */
