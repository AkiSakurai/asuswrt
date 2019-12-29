/*
 * Functions for supporting HW save-restore functionality
 *
 * Copyright 2019 Broadcom
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
 * $Id: saverestore.c 774680 2019-05-02 12:46:25Z $
 */

#include <typedefs.h>
#include <osl.h>
#include <sbchipc.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <sbsocram.h>
#include <hndpmu.h>

#include <saverestore.h>
#include <sbgci.h>

#if defined(BCMDBG_ERR)
#define	SR_ERROR(args)	printf args
#else
#define	SR_ERROR(args)
#endif	/* BCMDBG_ERR */

static uint32 sr_control(si_t *sih, int sr_core, uint32 n, uint32 mask, uint32 val);

/* minimal SR functionality to support srvsdb */
#ifndef SR_ESSENTIALS_DISABLED
bool _sr_essentials = TRUE;
#else
bool _sr_essentials = FALSE;
#endif // endif

/* Power save related save/restore support */
#ifndef SAVERESTORE_DISABLED
bool _sr = TRUE;
#else
bool _sr = FALSE;
#endif // endif

#ifdef SRFAST
/* Save/restore fast mode support */
#ifndef SR_FAST_DISABLED
bool _sr_fast = TRUE;
#else
bool _sr_fast = FALSE;
#endif // endif
#endif /* SRFAST */

#define SR_MAX_CLIENT		4
typedef struct {
	sr_save_callback_t cb;
	void *arg;
} sr_s_client_info_t;

typedef struct {
	sr_restore_callback_t cb;
	void *arg;
} sr_r_client_info_t;

#if defined(SAVERESTORE)
static bool g_wakeup_from_deep_sleep = FALSE;
static uint32 g_num_wakeup_from_deep_sleep = 0;
static void sr_update_deep_sleep(si_t *sih);
static sr_r_client_info_t sr_restore_list[SR_MAX_CLIENT];
static sr_s_client_info_t sr_save_list[SR_MAX_CLIENT];
static void sr_wokeup_from_deep_sleep(bool state);
static bool sr_is_wakeup_from_deep_sleep(void);

/* Accessor Functions */
static sr_r_client_info_t* BCMRAMFN(sr_get_restore_info)(uint8 idx)
{
	return &sr_restore_list[idx];
}

static sr_s_client_info_t* BCMRAMFN(sr_get_save_info)(uint8 idx)
{
	return &sr_save_list[idx];
}
#endif /* SAVERESTORE */

/* Access the save-restore portion of the PMU chipcontrol reg */
uint32
sr_chipcontrol(si_t *sih, uint32 mask, uint32 val)
{
	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		/* All 32 bits of pmu chipctl3 are for save-restore.
		 * Hence not needed of masking/shifting the input arguments
		 */
		val = si_pmu_chipcontrol(sih, 3, mask, val);
		break;
	CASE_BCM43602_CHIP: /* uses a newer access mechanism */
	/* intentional fall through */
	default:
		val = BCME_NOTFOUND;
		break;
	}
	return val;
}

/* Access the chipcontrol reg 4 */
uint32
sr_chipcontrol4(si_t *sih, uint32 mask, uint32 val)
{
	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		val = si_pmu_chipcontrol(sih, 4, mask, val);
		break;
	CASE_BCM43602_CHIP: /* uses a newer access mechanism */
	/* intentional fall through */
	default:
		val = BCME_NOTFOUND;
		break;
	}
	return val;
}

/* Access the chipcontrol reg 2 */
uint32
sr_chipcontrol2(si_t *sih, uint32 mask, uint32 val)
{
	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		val = si_pmu_chipcontrol(sih, 2, mask, val);
		break;
	CASE_BCM43602_CHIP: /* uses a newer access mechanism */
	/* intentional fall through */
	default:
		val = BCME_NOTFOUND;
		break;
	}
	return val;
}

/* Access the chipcontrol reg 5 */
uint32
sr_chipcontrol5(si_t *sih, uint32 mask, uint32 val)
{
	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		val = si_pmu_chipcontrol(sih, 5, mask, val);
		break;
	CASE_BCM43602_CHIP: /* uses a newer access mechanism */
	/* intentional fall through */
	default:
		val = BCME_NOTFOUND;
		break;
	}
	return val;
}

/* Access the chipcontrol reg 6 */
uint32
sr_chipcontrol6(si_t *sih, uint32 mask, uint32 val)
{
	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		val = si_pmu_chipcontrol(sih, 6, mask, val);
		break;
	CASE_BCM43602_CHIP: /* uses a newer access mechanism */
	/* intentional fall through */
	default:
		val = BCME_NOTFOUND;
		break;
	}
	return val;
}

/** Access regulator control register 4 */
uint32
sr_regcontrol4(si_t *sih, uint32 mask, uint32 val)
{
	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		val = si_pmu_vreg_control(sih, 4, mask, val);
		break;
	CASE_BCM43602_CHIP: /* uses a newer access mechanism */
	/* intentional fall through */
	default:
		val = BCME_NOTFOUND;
		break;
	}
	return val;
}

static uint32
sr_control(si_t *sih, int sr_core, uint32 n, uint32 mask, uint32 val)
{
	uint32 retVal;
	chipcregs_t *cc;
	srregs_t *srr;
	uint origidx = si_coreidx(sih);
	uint intr_val = 0;
	volatile unsigned int *p;
	osl_t *osh = si_osh(sih);

	/* Block ints and save current core */
	intr_val = si_introff(sih);
	switch (CHIPID(sih->chip)) {
		case BCM4347_CHIP_GRPID:
		case BCM4369_CHIP_GRPID:
			if (!AOB_ENAB(sih)) {
				ASSERT(0);
				return BCME_NOTFOUND;
			}
			srr = si_setcore(sih, SR_CORE_ID, sr_core);

			if (n == 0) {
				p = &srr->sr_control0;
			} else {
				p = &srr->sr_control1;
			}

			break;

		default:
			cc = si_setcoreidx(sih, SI_CC_IDX);

			if (sr_core == 0) {
				if (n == 0)
					p = &cc->sr_control0;
				else
					p = &cc->sr_control1;
			} else {
				if (n == 0)
					p = &cc->sr1_control0;
				else
					p = &cc->sr1_control1;
			}

			break;
	}

	retVal = R_REG(osh, p);

	if (mask != 0) {
		AND_REG(osh, p, ~mask);
		OR_REG(osh, p, val);
	}

	si_setcoreidx(sih, origidx);
	si_intrrestore(sih, intr_val);

	return retVal;
}

/* Function to change the sr memory control to sr engine or driver access */
static int
sr_enable_sr_mem_for(si_t *sih, int opt)
{
	int retVal = 0;
	uint origidx;
	sbsocramregs_t *socramregs;

	origidx = si_coreidx(sih);

	switch (CHIPID(sih->chip)) {
	default:
		retVal = -1;
	}

	/* Return to previous core */
	si_setcoreidx(sih, origidx);

	return retVal;
}

#if defined(SAVERESTORE)
/** Interface function used to read/write value from/to sr memory */
uint32
sr_mem_access(si_t *sih, int op, uint32 addr, uint32 data)
{
	uint origidx;
	sbsocramregs_t *socramregs;
	uint32 sr_ctrl;
	osl_t *osh = si_osh(sih);

	BCM_REFERENCE(sr_ctrl);
	origidx = si_coreidx(sih);

	sr_enable_sr_mem_for(sih, SR_HOST);

	switch (CHIPID(sih->chip)) {
	default:
		data = 0xFFFFFFFF;
	}

	/* Return to previous core */
	si_setcoreidx(sih, origidx);

	sr_enable_sr_mem_for(sih, SR_ENGINE);

	return data;
}
#endif /* SAVERESTORE */

/** This address is offset to the start of the txfifo bank containing the SR assembly code */
static uint32
BCMATTACHFN(sr_asm_addr)(si_t *sih, int sr_core)
{
	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		return CC4_4350_SR_ASM_ADDR;
	CASE_BCM43602_CHIP:
		return CC_SR1_43602_SR_ASM_ADDR;
	case BCM4347_CHIP_GRPID:
		if (sr_core == SRENG0) {
			return SR_ASM_ADDR_AUX_4347;
		} else if (sr_core == SRENG1) {
			return SR_ASM_ADDR_MAIN_4347;
		} else if (sr_core == SRENG2) {
			return SR_ASM_ADDR_DIG_4347;
		}
		ASSERT(0);
		break;

	case BCM4369_CHIP_GRPID:
		if (sr_core == SRENG0) {
			return SR_ASM_ADDR_AUX_4369;
		} else if (sr_core == SRENG1) {
			return SR_ASM_ADDR_MAIN_4369;
		} else if (sr_core == SRENG2) {
			return SR_ASM_ADDR_DIG_4369;
		}
		ASSERT(0);
		break;

	default:
		ASSERT(0);
		break;
	}

	return 0;
}

/** Initialize the save-restore engine during chip attach */
void
BCMATTACHFN(sr_save_restore_init)(si_t *sih)
{
	chipcregs_t *cc;
	uint origidx;
	uint intr_val = 0;
	osl_t *osh = si_osh(sih);
	int i;

	if (sr_cap(sih) == FALSE)
		return;

	/* Block ints and save current core */
	intr_val = si_introff(sih);
	origidx = si_coreidx(sih);

	cc = si_setcoreidx(sih, SI_CC_IDX);
	UNUSED_PARAMETER(cc);
	ASSERT(cc != NULL);
	BCM_REFERENCE(cc);

	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
	{
		/* Add ASM Init address */
		sr_chipcontrol4(sih, CC4_SR_INIT_ADDR_MASK,
			sr_asm_addr(sih, SRENG0) << CC4_SR_INIT_ADDR_SHIFT);

		/* Turn on MEMLPDO before MEMLPLDO power switch .(bit 159) */
		sr_regcontrol4(sih, VREG4_4350_MEMLPDO_PU_MASK,
			(1 << VREG4_4350_MEMLPDO_PU_SHIFT));

		/* Switch to ALP/HT clk as SR clk is too long */
		sr_chipcontrol4(sih, CC4_4350_EN_SR_CLK_ALP_MASK | CC4_4350_EN_SR_CLK_HT_MASK,
			(1 << CC4_4350_EN_SR_CLK_ALP_SHIFT) | (1 << CC4_4350_EN_SR_CLK_HT_SHIFT));

		/* MEMLPLDO power switch enable */
		sr_chipcontrol2(sih, CC2_4350_MEMLPLDO_PWRSW_EN_MASK,
			(1 << CC2_4350_MEMLPLDO_PWRSW_EN_SHIFT));

		/* VDDM power switch enable */
		sr_chipcontrol2(sih, CC2_4350_VDDM_PWRSW_EN_MASK,
			(1 << CC2_4350_VDDM_PWRSW_EN_SHIFT));
#ifdef SRFAST
		if (SR_FAST_ENAB()) {
			sr_chipcontrol2(sih, CC2_4350_PHY_PWRSW_UPTIME_MASK,
				0x4 << CC2_4350_PHY_PWRSW_UPTIME_SHIFT);
			sr_chipcontrol2(sih, CC2_4350_VDDM_PWRSW_UPDELAY_MASK,
				0x4 << CC2_4350_VDDM_PWRSW_UPDELAY_SHIFT);
		}
#endif /* SRFAST */
		if ((BCM4350_CHIP(sih->chip) &&
			CST4350_CHIPMODE_SDIOD(sih->chipst))) {
			sr_chipcontrol2(sih, CC2_4350_SDIO_AOS_WAKEUP_MASK,
				(1 << CC2_4350_SDIO_AOS_WAKEUP_SHIFT));
		} else if (BCM4350_CHIP(sih->chip) && CST4350_CHIPMODE_PCIE(sih->chipst)) {
			si_pmu_chipcontrol(sih, 6,
				(CC6_4350_PCIE_CLKREQ_WAKEUP_MASK |
				CC6_4350_PMU_WAKEUP_ALPAVAIL_MASK),
				((1 << CC6_4350_PCIE_CLKREQ_WAKEUP_SHIFT) |
				(1 << CC6_4350_PMU_WAKEUP_ALPAVAIL_SHIFT)));
		}

		/* Magic number for chip control 3: 0xF012C33 */
		sr_chipcontrol(sih, ~0, (0x3 << CC3_SR_CLK_SR_MEM_SHIFT |
			0x3 << CC3_SR_MINDIV_FAST_CLK_SHIFT |
			1 << CC3_SR_R23_SR_RISE_EDGE_TRIG_SHIFT |
			1 << CC3_SR_R23_SR_FALL_EDGE_TRIG_SHIFT |
			2 << CC3_SR_NUM_CLK_HIGH_SHIFT |
			1 << CC3_SR_PHY_FUNC_PIC_SHIFT |
			0x7 << CC3_SR_ALLOW_SBC_FUNC_PIC_SHIFT |
			1 << CC3_SR_ALLOW_SBC_STBY_SHIFT));
	} break;

	CASE_BCM43602_CHIP:
		/*
		 * In contrast to 4345/4350/4335 chips, there is no need to touch PMU chip control
		 * 3 and 4 for SR setup.
		 */
		sr_control(sih, SRENG0, 0, 0xFFFFFFFF, (1 << CC_SR_CTL0_EN_SR_ENG_CLK_SHIFT)     |
			(0xc << CC_SR_CTL0_RSRC_TRIGGER_SHIFT)    |
			(3 << CC_SR_CTL0_MIN_DIV_SHIFT)           |
			(1 << CC_SR_CTL0_EN_SBC_STBY_SHIFT)       |
			(1 << CC_SR_CTL0_EN_SR_HT_CLK_SHIFT)      |
			(3 << CC_SR_CTL0_ALLOW_PIC_SHIFT)         |
			(0x10 << CC_SR_CTL0_MAX_SR_LQ_CLK_CNT_SHIFT) |
			(1 << CC_SR_CTL0_EN_MEM_DISABLE_FOR_SLEEP));

		W_REG(osh, &cc->sr_control1, sr_asm_addr(sih, SRENG0)); /* TxFIFO offset */
		break;

	case BCM4347_CHIP_GRPID:
		for (i = 0; i < SRENGMAX; i ++)	{
			uint32 val;

			val = SR0_SR_ENG_CLK_EN |
				SR0_RSRC_TRIGGER |
				SR0_WD_MEM_MIN_DIV |
				SR0_MEM_STBY_ALLOW |
				SR0_ENABLE_SR_HT |
				SR0_ALLOW_PIC |
				SR0_ENB_PMU_MEM_DISABLE;

			if (i == 0 || i == 2) {
				val |= SR0_INVERT_SR_CLK;
			}

			sr_control(sih, i, 0, ~0, val);
			sr_control(sih, i, 1, ~0, sr_asm_addr(sih, i));
		}

		/* retain voltage on the SR memories */
		si_pmu_chipcontrol(sih, 2,
			CC2_4347_VASIP_MEMLPLDO_VDDB_OFF_MASK |
			CC2_4347_MAIN_MEMLPLDO_VDDB_OFF_MASK |
			CC2_4347_AUX_MEMLPLDO_VDDB_OFF_MASK |
			CC2_4347_VASIP_VDDRET_ON_MASK |
			CC2_4347_MAIN_VDDRET_ON_MASK |
			CC2_4347_AUX_VDDRET_ON_MASK,
			(0 << CC2_4347_VASIP_MEMLPLDO_VDDB_OFF_SHIFT) |
			(0 << CC2_4347_MAIN_MEMLPLDO_VDDB_OFF_SHIFT) |
			(0 << CC2_4347_AUX_MEMLPLDO_VDDB_OFF_SHIFT) |
			(1 << CC2_4347_VASIP_VDDRET_ON_SHIFT) |
			(1 << CC2_4347_MAIN_VDDRET_ON_SHIFT) |
			(1 << CC2_4347_AUX_VDDRET_ON_SHIFT));

		if (CST4347_CHIPMODE_SDIOD(sih->chipst)) {
			/* setup wakeup for SDIO save/restore */
			si_pmu_chipcontrol(sih, 2, CC2_4347_SDIO_AOS_WAKEUP_MASK,
				(1 << CC2_4347_SDIO_AOS_WAKEUP_SHIFT));

			if (CHIPREV(sih->chiprev) == 1) {
				/* WAR for A0 */
				si_pmu_chipcontrol(sih, 6, CC6_4347_PWROK_WDT_EN_IN_MASK,
					(1 << CC6_4347_PWROK_WDT_EN_IN_SHIFT));
			}
		}
		break;

	case BCM4369_CHIP_GRPID:
		for (i = 0; i < SRENGMAX; i ++)	{
			uint32 val;

			val = SR0_4369_SR_ENG_CLK_EN |
				SR0_4369_RSRC_TRIGGER |
				SR0_4369_WD_MEM_MIN_DIV |
				SR0_4369_MEM_STBY_ALLOW |
				SR0_4369_ENABLE_SR_HT |
				SR0_4369_ALLOW_PIC |
				SR0_4369_ENB_PMU_MEM_DISABLE;

			if (i == 0 || i == 2) {
				val |= SR0_4369_INVERT_SR_CLK;
			}

			sr_control(sih, i, 0, ~0, val);
			sr_control(sih, i, 1, ~0, sr_asm_addr(sih, i));
		}
		break;

	default:
		break;
	}

	/* Return to previous core */
	si_setcoreidx(sih, origidx);

	si_intrrestore(sih, intr_val);
}

#ifdef SR_ESSENTIALS
/**
 * Load the save-restore firmware into the memory for some chips, other chips use a different
 * function.
 */
void
BCMATTACHFN(sr_download_firmware)(si_t *sih)
{
	uint origidx;
	uint intr_val = 0;

	/* Block ints and save current core */
	intr_val = si_introff(sih);
	origidx = si_coreidx(sih);

	switch (CHIPID(sih->chip)) {
	CASE_BCM43602_CHIP:
	case BCM43570_CHIP_ID:
	case BCM4347_CHIP_GRPID:
	case BCM4369_CHIP_GRPID:
	{
		/* Firmware is downloaded in tx-fifo from wlc.c */
	}
	default:
		break;
	}

	/* Return to previous core */
	si_setcoreidx(sih, origidx);

	si_intrrestore(sih, intr_val);
}

/**
 * Either query or enable/disable the save restore engine. Returns a negative value on error.
 * For a query, returns 0 when engine is disabled, positive value when engine is enabled.
 */
int
sr_engine_enable(si_t *sih, int sr_core, bool oper, bool enable)
{
	uint origidx;
	uint intr_val = 0;
	int ret_val = 0;

	/* Block ints and save current core */
	intr_val = si_introff(sih);
	origidx = si_coreidx(sih);

	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
	{
		uint8 on;

		/* Use sr_isenab() for IOV_GET */
		ASSERT(oper == IOV_SET);

		on = enable ? 1 : 0;
		sr_chipcontrol(sih, CC3_SR_ENGINE_ENABLE_MASK, (on << CC3_SR_ENGINE_ENABLE_SHIFT));
	} break;
	CASE_BCM43602_CHIP:
		if (oper == IOV_GET) {
			ret_val = (sr_control(sih, sr_core, 0, 0, 0) & CC_SR0_4349_SR_ENG_EN_MASK);
			ret_val >>= CC_SR0_4349_SR_ENG_EN_SHIFT;
		} else {
			if (!BCM43602_CHIP(sih->chip)) {
				si_force_islanding(sih, enable);
			}
			sr_control(sih, sr_core, 0, CC_SR0_4349_SR_ENG_EN_MASK,
				(enable ? 1: 0) << CC_SR0_4349_SR_ENG_EN_SHIFT);
			ret_val = enable;
		}
		break;

	case BCM4347_CHIP_GRPID:
	case BCM4369_CHIP_GRPID:
		if (oper == IOV_GET) {
			ret_val = (sr_control(sih, sr_core, 0, 0, 0) & SR0_SR_ENG_EN_MASK);
			ret_val >>= SR0_SR_ENG_EN_SHIFT;
		} else {
			sr_control(sih, sr_core, 0, SR0_SR_ENG_EN_MASK,
				(enable ? 1: 0) << SR0_SR_ENG_EN_SHIFT);
			ret_val = enable;
		}
		break;

	default:
		ret_val = BCME_UNSUPPORTED;
		break;
	}

	si_setcoreidx(sih, origidx);

	si_intrrestore(sih, intr_val);
	return ret_val;
} /* sr_engine_enable */
#endif /* SR_ESSENTIALS */

#if defined(SAVERESTORE)
CONST uint32*
BCMPREATTACHFNSR(sr_get_sr_params)(si_t *sih, int sr_core, uint32 *arrsz, uint32 *offset)
{
	uint32 addr;
	const uint32 *sr_source_code = NULL;

	if ((CCREV(sih->ccrev) >= 48) && (CCREV(sih->ccrev) != 51)) {
		addr = sr_control(sih, sr_core, 1, 0, 0);
		addr = (addr & CC_SR_CTL1_SR_INIT_MASK) >> CC_SR_CTL1_SR_INIT_SHIFT;
	} else {
		addr = sr_chipcontrol4(sih, 0, 0);
		addr = (addr & CC4_SR_INIT_ADDR_MASK) >> CC4_SR_INIT_ADDR_SHIFT;
	}

	/* convert to txfifo byte offset */
	*offset = addr << 8;

	sr_get_source_code_array(sih, sr_core, &sr_source_code, arrsz);
	return sr_source_code;
}
#endif /* SAVERESTORE */

#if defined(SAVERESTORE)
void
sr_engine_enable_post_dnld(si_t *sih, int sr_core, bool enable)
{
	switch (CHIPID(sih->chip)) {
	CASE_BCM43602_CHIP:
	case BCM43570_CHIP_ID:
	case BCM4347_CHIP_GRPID:
	case BCM4369_CHIP_GRPID:
	{
		sr_engine_enable(sih, sr_core, IOV_SET, enable);
	} break;
	default:
		break;
	}
}
#endif /* SAVERESTORE */

#ifdef SAVERESTORE
/** return TRUE if the power save variant of save/restore is enabled */
bool
sr_isenab(si_t *sih)
{
	bool enab = FALSE;

	switch (CHIPID(sih->chip)) {
	case BCM43570_CHIP_ID:
		enab = (sr_chipcontrol(sih, 0, 0) & CC3_SR_ENGINE_ENABLE_MASK) ? TRUE : FALSE;
		break;

	CASE_BCM43602_CHIP: /* add chips here when CCREV >= 48 */
	enab = ((sr_control(sih, SRENG0, 0, 0, 0) & CC_SR0_4349_SR_ENG_EN_MASK) >>
			CC_SR0_4349_SR_ENG_EN_SHIFT);
		enab = (enab ? TRUE: FALSE);
		break;

	case BCM4347_CHIP_GRPID:
		enab = (((sr_control(sih, SRENG0, 0, 0, 0) & SR0_SR_ENG_EN_MASK)) &&
			((sr_control(sih, SRENG1, 0, 0, 0) & SR0_SR_ENG_EN_MASK)) &&
			((sr_control(sih, SRENG2, 0, 0, 0) & SR0_SR_ENG_EN_MASK))) ?
			TRUE : FALSE;
		break;

	case BCM4369_CHIP_GRPID:
		enab = (((sr_control(sih, SRENG0, 0, 0, 0) & SR0_4369_SR_ENG_EN_MASK)) &&
			((sr_control(sih, SRENG1, 0, 0, 0) & SR0_4369_SR_ENG_EN_MASK)) &&
			((sr_control(sih, SRENG2, 0, 0, 0) & SR0_4369_SR_ENG_EN_MASK))) ?
			TRUE : FALSE;
		break;

	default:
		ASSERT(0);
		break;
	}

	return enab;
}
#endif /* SAVERESTORE */

#ifdef SR_ESSENTIALS
/**
 * return TRUE for chips that support the power save variant of save/restore
 */
bool
sr_cap(si_t *sih)
{
	bool cap = FALSE;

	switch (CHIPID(sih->chip)) {
	case BCM4347_CHIP_GRPID:
	case BCM4369_CHIP_GRPID:
	CASE_BCM43602_CHIP:
		cap = TRUE;
		break;
	case BCM43570_CHIP_ID:		cap = TRUE;
		break;
	default:
		break;
	}

	return cap;
}
#endif /* SR_ESSENTIALS */

#if defined(SAVERESTORE)
uint32
BCMATTACHFN(sr_register_save)(si_t *sih, sr_save_callback_t cb, void *arg)
{
	uint8 i;
	sr_s_client_info_t *sr_save_info;

	if (sr_cap(sih) == FALSE)
		return BCME_ERROR;

	for (i = 0; i < SR_MAX_CLIENT; i++) {
		sr_save_info = sr_get_save_info(i);
		if (sr_save_info->cb == NULL) {
			sr_save_info->cb = cb;
			sr_save_info->arg = arg;
			return 0;
		}
	}

	return BCME_ERROR;
}

uint32
BCMATTACHFN(sr_register_restore)(si_t *sih, sr_restore_callback_t cb, void *arg)
{
	uint8 i;
	sr_r_client_info_t *sr_restore_info;

	if (sr_cap(sih) == FALSE)
		return BCME_ERROR;

	for (i = 0; i < SR_MAX_CLIENT; i++) {
		sr_restore_info = sr_get_restore_info(i);
		if (sr_restore_info->cb == NULL) {
			sr_restore_info->cb = cb;
			sr_restore_info->arg = arg;
			return 0;
		}
	}

	return BCME_ERROR;
}

void
sr_process_save(si_t *sih)
{
	bool sr_enable_ok = TRUE;
	uint8 i;
	sr_s_client_info_t *sr_save_info;

	if (sr_cap(sih) == FALSE) {
		return;
	}

	for (i = 0; i < SR_MAX_CLIENT; i++) {
		sr_save_info = sr_get_save_info(i);
		if (sr_save_info->cb != NULL && !sr_save_info->cb(sr_save_info->arg)) {
			sr_enable_ok = FALSE;
			break;
		}
	}

	if (sr_isenab(sih) != sr_enable_ok) {
		sr_engine_enable(sih, SRENG0, IOV_SET, sr_enable_ok);
	}
}

void
sr_process_restore(si_t *sih)
{
	uint origidx;
	uint intr_val = 0;
	uint8 i;
	sr_r_client_info_t *sr_restore_info;

	if (sr_cap(sih) == FALSE)
		return;

	sr_update_deep_sleep(sih);

	/* Block ints, save current core and switch to common core */
	intr_val = si_introff(sih);
	origidx = si_coreidx(sih);

	/* If it is a 4334 or a variant we can test if the hardware actually
	   went into deep sleep.  If so clear indication/log bit for next time
	   before letting any registered modules do their restore processing.
	 */
	if (!SRPWR_ENAB() && !sr_is_wakeup_from_deep_sleep()) goto exit;

	/* Note that PSM/ucode will resume execution at the same time as
	 * the ARM and you need to consider any possible race conditions.
	 *
	 * Any wl related registered, restore function should consider
	 * whether the PSM might use resources it is restoring before we
	 * are done restoring, e.g.. RCMTA table.
	 *
	 * Similarly, you also have to consider whether we update, use
	 * or otherwise depend on resources which PSM might be restoring.
	 * If so it might make sense to first synchronize with the PSM by
	 * "suspending the MAC".
	 */
	for (i = 0; i < SR_MAX_CLIENT; i++) {
		sr_restore_info = sr_get_restore_info(i);
		if (sr_restore_info->cb != NULL) {
			sr_restore_info->cb(sr_restore_info->arg,
				sr_is_wakeup_from_deep_sleep());
		}
	}

exit:
	si_setcoreidx(sih, origidx);
	si_intrrestore(sih, intr_val);
}

static void sr_update_deep_sleep(si_t *sih)
{
	osl_t *osh = si_osh(sih);

	sr_wokeup_from_deep_sleep(FALSE);

	if (si_pmu_reset_ret_sleep_log(sih, osh))
		sr_wokeup_from_deep_sleep(TRUE);
}

static void sr_wokeup_from_deep_sleep(bool state)
{
	g_wakeup_from_deep_sleep = state;
	if (state) {
		g_num_wakeup_from_deep_sleep ++;
#ifdef SR_DEBUG
		if (!(g_num_wakeup_from_deep_sleep % 10))
			printf("g_num_wakeup_from_deep_sleep = %d\n", g_num_wakeup_from_deep_sleep);
#endif // endif
	}
}

static bool sr_is_wakeup_from_deep_sleep()
{
	return g_wakeup_from_deep_sleep;
}

#endif /* SAVERESTORE */
