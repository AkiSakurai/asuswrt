/**
 * @file
 * @brief
 * Common (OS-independent) portion of Broadcom debug crash validation
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
 * $Id: wlc_debug_crash.c 614820 2016-01-23 17:16:17Z $
 */

#if defined(WLC_DEBUG_CRASH)

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wl_export.h>
#include <hndsoc.h>
#include <wlc_pio.h>
#include <wlc_hw_priv.h>
#include <wlc_debug_crash.h>
#include <osl.h>
#include <d11ucode.h>
#include <wlc_bmac.h>
#include <bcmutils.h>
#include <phy_dbg_api.h>
#include <phy_hc_api.h>

struct wlc_debug_crash_info {
	wlc_info_t *wlc;
	uint32 crash_type;
	uint32 delay;
	bool timer_active;
	struct wl_timer * debug_crash_timer;
};

typedef struct crash_info {
	uint32 reg;
	uint8 write;
	uint8 clk;
	uint8 pwr;
	uint8 trap;
} crash_info_t;

enum
{
	E_UCRASH_CMD_DONE	= 0,	/* command processed from ucode */
	E_UCRASH_CMD_READ	= 1,	/* read invalid addr */
	E_UCRASH_CMD_WRITE	= 2,	/* write invalid addr */
	E_UCRASH_CMD_PHYREAD	= 3,	/* read valid/invalid phy reg */
	E_UCRASH_CMD_PHYWRITE	= 4,	/* write valid/invalid phy reg */
	E_UCRASH_CMD_HANG	= 5,	/* ucode hang/trap */
};

#ifndef M_PSM_SOFT_REGS
#define M_PSM_SOFT_REGS 0x0
#endif /* M_PSM_SOFT_REGS */

#ifndef M_PSM_SOFT_REGS_EXT
#define M_PSM_SOFT_REGS_EXT     (0xc0*2)  /* corerev >= 40 only */
#endif /* M_PSM_SOFT_REGS_EXT */

static crash_info_t crash_action[] =
	/* REG			WRITE	CLK	PWR	TRAP/HANG			  */
	{{0x12345678,		FALSE, FALSE, FALSE, FALSE},  /* 0 - Read random	  */
	{0xF000, FALSE, FALSE, FALSE, FALSE},  /* 1 - RD INV CORE	  */
	{0xF000, TRUE,  FALSE, FALSE, FALSE},  /* 2 - WR INV CORE	  */
	{0xF000, FALSE, FALSE, FALSE, FALSE},  /* 3 - RD INV WRAP	  */
	{0xF000, TRUE,  FALSE, FALSE, FALSE},  /* 6 - WR INV WRAP	  */
	{0x1000, FALSE, FALSE, FALSE, FALSE},  /* 5 - RD RES CORE	  */
	{0x1000, TRUE,  FALSE, FALSE, FALSE},  /* 6 - WR RES CORE	  */
	{0x0008, FALSE, FALSE, FALSE, FALSE},  /* 7 - RD RES WRAP	  */
	{0x0008, TRUE,  FALSE, FALSE, FALSE},  /* 8 - WR RES WRAP	  */
	{0x1120, FALSE, TRUE,  FALSE, FALSE},  /* 9 - RD CORE - NO CLK     */
	{0x1120, TRUE,  TRUE,  FALSE, FALSE},  /* A - WR CORE - NO CLK     */
	{0x1120, FALSE, FALSE, TRUE,  FALSE},  /* B - RD CORE - NO PWR     */
	{0x1120, TRUE,  FALSE, TRUE,  FALSE},  /* C - WR CORE - NO PWR     */
	/* D - Generate PCIe   AER  */
	{0x12345678,			FALSE, FALSE, FALSE, FALSE},
	/* E - Trap  */
	{0,						FALSE, FALSE, FALSE, FALSE},
	/* F - Hang  */
	{0,						FALSE, FALSE, FALSE, FALSE},
	/* 0x10 Valid Phy reg rd - NO CLK */
	{0x0001,				FALSE, TRUE,  FALSE, FALSE},
	/* 0x11 Valid Phy reg wr - NO CLK */
	{0x0001,				TRUE,  TRUE,  FALSE, FALSE},
	/* 0x12 - Invalid Phy reg read */
	{0x0002,				FALSE, FALSE, FALSE, FALSE},
	/* 0x13 - Invalid Phy reg write */
	{0x0002,				TRUE,  FALSE, FALSE, FALSE}};

/**
 * New(er) chips started using other backplane addresses than 0x18000000 for the chipcommon core.
 * The same NIC or DHD driver binary is required to support both old(er) and new(er) chips.
 * Table crash_action[] contains adresses in different domains. This functions returns, for
 * addresses in the backplane domain, the appropriate chip specific backplane address offset.
 */
static uint32 wlc_debug_crash_reg_offset(si_t *sih, uint32 type)
{
	switch (type) {
		case DBG_CRSH_TYPE_RD_INV_CORE:
		case DBG_CRSH_TYPE_WR_INV_CORE:
		case DBG_CRSH_TYPE_RD_RES_CORE:
		case DBG_CRSH_TYPE_WR_RES_CORE:
		case DBG_CRSH_TYPE_RD_CORE_NO_CLK:
		case DBG_CRSH_TYPE_WR_CORE_NO_CLK:
		case DBG_CRSH_TYPE_RD_CORE_NO_PWR:
		case DBG_CRSH_TYPE_WR_CORE_NO_PWR:
			return SI_ENUM_BASE(sih);
		case DBG_CRSH_TYPE_RD_INV_WRAP:
		case DBG_CRSH_TYPE_WR_INV_WRAP:
		case DBG_CRSH_TYPE_RD_RES_WRAP:
		case DBG_CRSH_TYPE_WR_RES_WRAP:
			return SI_WRAP_BASE(sih);
	}

	return 0;
}

/* **** Private Functions Prototypes *** */
#define DBG_CRASH_SRC(x) ((x>>16)&0xFFFF)
#define DBG_CRASH_TYPE(x) ((x)&0xFFFF)

uint32 wlc_debug_crash_execute_crash(wlc_info_t* wlc, uint32 type, uint32 delay, int * err)
{
	wlc_debug_crash_info_t * ctxt = wlc->debug_crash_info;

	if (ctxt->timer_active) {
		return (*err = BCME_BUSY);
	}

	if (delay) {
		ctxt->crash_type = type;
		ctxt->delay = delay;
		wl_add_timer(ctxt->wlc->wl, ctxt->debug_crash_timer, delay*1000, 0);
		ctxt->timer_active = TRUE;
		return BCME_OK;
	}

	if (type == DBG_CRSH_TYPE_POWERCYCLE) {
#if defined(BCM_OL_DEV) || defined(DONGLEBUILD)
		/* DBG_CRSH_TYPE_POWERCYCLE shares same code as that of
		 * DBG_CRSH_TYPE_TRAP for FW
		 */
		((void (*)())(0))();
#else
#ifdef NEED_HARD_RESET
		/* Powercycle WiFi module in case of OSX */
		wl_log_system_state(wlc->wl, "TestPowerCycle", FALSE);
		wl_powercycle(wlc->wl);
#else
		return (*err = BCME_UNSUPPORTED);
#endif /* NEED_HARD_RESET */
#endif /* if defined(BCM_OL_DEV) || defined(DONGLEBUILD) */
	} else if (type == DBG_CRSH_TYPE_HANG) {
		while (1);
	} else if ((type >= DBG_CRSH_TYPE_RADIO_HEALTHCHECK_START) &&
			(type <= DBG_CRSH_TYPE_RADIO_HEALTHCHECK_LAST)) {
#ifdef RADIO_HEALTH_CHECK
		phy_crash_reason_t dc;
		phy_healthcheck_type_t hc;
		switch (type) {
			case DBG_CRSH_TYPE_DESENSE_LIMITS:
				dc = PHY_RC_DESENSE_LIMITS;
				hc = PHY_HC_RX;
				break;
			case DBG_CRSH_TYPE_BASEINDEX_LIMITS:
				dc = PHY_RC_BASEINDEX_LIMITS;
				hc = PHY_HC_TX;
				break;
			case DBG_CRSH_TYPE_TXCHAIN_INVALID:
				dc = PHY_RC_TXCHAIN_INVALID;
				hc = PHY_HC_TX;
				break;
			default:
				return (*err = BCME_BADARG);
		}
		if ((*err = phy_hc_debugcrash_forcefail(wlc->hw->band->pi, dc))
					!= BCME_OK) {
			return (*err);
		}
		*err = phy_hc_debugcrash_health_check(wlc->hw->band->pi, hc);
		return (*err);
#else
		return (*err = BCME_UNSUPPORTED);
#endif /* DISABLE_RADIO_HEALTH_CHECK */
	} else if (type == DBG_CRSH_TYPE_DUMP_STATE) {
		/* Debuggability :
		 * Take a core capture and dump only the driver state.
		 */
		wl_log_system_state(wlc->wl, "DumpDriverState", TRUE);
	} else if (type < (sizeof(crash_action)/sizeof(crash_info_t))) {
		uint32 reg;
		if (crash_action[type].clk) {
			/* Turn off the core clock */
			si_core_cflags(wlc->hw->sih, SICF_CLOCK_EN, 0);
		}
		reg = crash_action[type].reg + wlc_debug_crash_reg_offset(wlc->hw->sih, type);
		return si_raw_reg(wlc->hw->sih, reg, 0, crash_action[type].write);
	} else {
		return (*err = BCME_BADARG);
	}

	return 0;
}

static void
wlc_debug_crash_timer_cb(void *arg)
{
	wlc_info_t * wlc = (wlc_info_t *)arg;
	wlc_debug_crash_info_t * ctxt = wlc->debug_crash_info;
	int err = 0;

	if (ctxt->timer_active) {
		ctxt->timer_active = FALSE;
		wlc_debug_crash_execute_crash(wlc, DBG_CRASH_TYPE(ctxt->crash_type), 0, &err);
	}

}

static const bcm_iovar_t debug_crash_iovars[] = {
	{ "debug_crash", IOV_DEBUG_CRASH,
	(IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, 0
	},
	{NULL, 0, 0, 0, 0, 0 }
};

static uint32
wlc_debug_crash_ucode(wlc_info_t* wlc, uint32 type, uint32 delay, int * err)
{
	uint16 shm_adr, shm_data_lo, shm_data_hi, shm_cmd;
	uint cmd, data = 0;
	uint addr = 0;
	uint16 ucode_dbg_crash_blk;

	if (delay) {
		*err = BCME_UNSUPPORTED;
		return 0;
	}

	if (!wlc->hw->clk) {
		*err = BCME_NOCLK;
		return 0;
	}

	if ((type >= DBG_CRSH_TYPE_PHYREAD) &&
		(type <= DBG_CRSH_TYPE_INV_PHYWRITE)) {
		cmd = crash_action[type].write ?
			E_UCRASH_CMD_PHYWRITE : E_UCRASH_CMD_PHYREAD;
		addr = crash_action[type].reg;
		data = 1;
	} else if ((type == DBG_CRSH_TYPE_TRAP) ||
		(type == DBG_CRSH_TYPE_HANG)) {
		cmd = E_UCRASH_CMD_HANG;
	} else if (type >= DBG_CRSH_TYPE_RD_CORE_NO_PWR) {
		*err = BCME_UNSUPPORTED;
		return 0;
	} else {
		cmd = crash_action[type].write ?
			E_UCRASH_CMD_WRITE : E_UCRASH_CMD_READ;
		addr = crash_action[type].reg + wlc_debug_crash_reg_offset(wlc->hw->sih, type);
		data = 1;
	}

	/* shm register locations based on core revision */
	if (D11REV_GE(wlc->hw->corerev, 40)) {
		ucode_dbg_crash_blk = 2 * wlc_bmac_read_shm(wlc->hw,
			M_UDBG_CRASH_BLK_PTR_AC);
	}
	else {
		ucode_dbg_crash_blk = 2 * wlc_bmac_read_shm(wlc->hw,
			M_UDBG_CRASH_BLK_PTR_LE30);
	}

	shm_adr = ucode_dbg_crash_blk;
	shm_data_lo = ucode_dbg_crash_blk + 2;
	shm_data_hi = ucode_dbg_crash_blk + 4;
	shm_cmd	= ucode_dbg_crash_blk + 6;

	if (type < DBG_CRSH_TYPE_PHYREAD) {
		/* Write upper 24 address bits to IHR registers */
		W_REG(wlc->osh, D11_SbAddrL(wlc), ((addr>>16)&0xFFFF));
		W_REG(wlc->osh, D11_SbAddrLL(wlc), (addr&0xFF00));

		/* lower 8 bits of address to SHM */
		wlc_bmac_write_shm(wlc->hw, shm_adr, (uint16)(addr&0xFF));

		/* If command is write, then write the data to IHR registers */
		if (cmd == E_UCRASH_CMD_WRITE) {
			W_REG(wlc->osh, D11_SBRegDataL(wlc), (data&0xFFFF));
			W_REG(wlc->osh, D11_SBRegDataH(wlc), ((data>>16)&0xFFFF));
		}
	} else {
		/* write phy address to SHM */
		wlc_bmac_write_shm(wlc->hw, shm_adr, (uint16)addr);
		/* Write data to SHM */
		wlc_bmac_write_shm(wlc->hw, shm_data_lo, data);
	}

	wlc_bmac_suspend_mac_and_wait(wlc->hw);

	/* turn off phy clock */
	if ((type >= DBG_CRSH_TYPE_PHYREAD) && (crash_action[type].clk)) {
		wlc_bmac_core_phy_clk(wlc->hw, OFF);
	}

	/* Write the command to SHM */
	wlc_bmac_write_shm(wlc->hw, shm_cmd, cmd);

	/* Poll for shm_cmd to rurn to zero indicating command completion */
#define WLC_DBG_CRASH_MAX_UCODE_TO	(100*1000)
	SPINWAIT((wlc_bmac_read_shm(wlc->hw, shm_cmd) !=
		E_UCRASH_CMD_DONE), WLC_DBG_CRASH_MAX_UCODE_TO);

	if (wlc_bmac_read_shm(wlc->hw, shm_cmd) != E_UCRASH_CMD_DONE) {
		WL_ERROR(("UCODE DID NOT RETURN E_UCRASH_CMD_DONE\n"));
		goto end;
	}

	/* If command is read, read back the data from SHM */
	if ((cmd == E_UCRASH_CMD_READ) ||
			(cmd == E_UCRASH_CMD_PHYREAD)) {
		data = wlc_bmac_read_shm(wlc->hw, shm_data_lo);
		data |= wlc_bmac_read_shm(wlc->hw, shm_data_hi) << 16;
	}

end:
	/* Check for back plane error recorded */
	cmd = R_REG(wlc->osh, D11_SBRegAddr(wlc));
	if (cmd & (1<<14)) {
		WL_ERROR(("BACKPLANE ACCESS BUSY : %x\n", cmd));
	}

	if (cmd & (1<<15)) {
		WL_ERROR(("BACKPLANE ACCESS ERROR : %x\n", cmd));
	}

	/* turn on phy clock */
	if ((type >= DBG_CRSH_TYPE_PHYREAD) && (crash_action[type].clk)) {
		wlc_bmac_core_phy_clk(wlc->hw, ON);
	}
	wlc_bmac_enable_mac(wlc->hw);

	return data;
}

static uint32
wlc_debug_crash_btcoex(wlc_info_t* wlc, uint32 type, uint32 delay, int * err)
{
	*err = BCME_OK;
	uint16 shmem_data;
	if (delay) {
		return (*err = BCME_UNSUPPORTED);
	}
	if (!wlc->hw->clk) {
		return (*err = BCME_NOCLK);
	}
	shmem_data = (uint16)wlc_bmac_btc_params_get(wlc->hw, M_BTCX_HOST_FLAGS2_OFFSET(wlc)/2);
	switch (type) {
		case DBG_CRSH_TYPE_BTCOEX_RFACTIVE:
			WL_PRINT(("Force TRAP on RF_ACTIVE\n"));
			*err = wlc_bmac_btc_params_set(wlc->hw, M_BTCX_HOST_FLAGS2_OFFSET(wlc)/2,
					shmem_data | BTCX_HFLG2_TRAP_RFACTIVE);
		break;
		case DBG_CRSH_TYPE_BTCOEX_TXCONF_DELAY:
			WL_PRINT(("Force TRAP on TXCONF\n"));
			*err = wlc_bmac_btc_params_set(wlc->hw, M_BTCX_HOST_FLAGS2_OFFSET(wlc)/2,
					shmem_data | BTCX_HFLG2_TRAP_TXCONF);
		break;
		case DBG_CRSH_TYPE_BTCOEX_ANT_DELAY:
			WL_PRINT(("Force TRAP on ANTDLY\n"));
			*err = wlc_bmac_btc_params_set(wlc->hw, M_BTCX_HOST_FLAGS2_OFFSET(wlc)/2,
					shmem_data | BTCX_HFLG2_TRAP_ANTDLY);
		break;
		case DBG_CRSH_TYPE_BTCOEX_INVLD_TASKID:
			WL_PRINT(("Force TRAP on BTTYPE\n"));
			*err = wlc_bmac_btc_params_set(wlc->hw, M_BTCX_HOST_FLAGS2_OFFSET(wlc)/2,
					shmem_data | BTCX_HFLG2_TRAP_BTTYPE);
		break;
		default:
			return (*err = BCME_UNSUPPORTED);
	}
	/* Force the trap specified above */
	shmem_data = (uint16) wlc_bmac_btc_params_get(wlc->hw, M_BTCX_CONFIG_OFFSET(wlc)/2);
	*err = wlc_bmac_btc_params_set(wlc->hw, M_BTCX_CONFIG_OFFSET(wlc)/2,
			shmem_data | BTCX_CONFIG_FORCE_TRAP);
	return *err;
}

static int
wlc_debug_crash_doiovar(void *hdl, uint32 actionid,
	void * p, uint plen, void * a, uint alen,
	uint vsize, struct wlc_if * wlcif)
{
	wlc_debug_crash_info_t *ctxt = hdl;
	int err = 0;
	uint32 int_val = 0;
	uint32 int_val2 = 0;
	int32 *ret_int_ptr = (int32*)a;

	BCM_REFERENCE(alen);
	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(vsize);

	if (plen >= (int)sizeof(int_val)) {
		bcopy(p, &int_val, sizeof(int_val));
	}

	switch (actionid) {
		case IOV_SVAL(IOV_DEBUG_CRASH):
			/* Delayed execution will have second parameter */
			if (plen < (int)sizeof(int_val) * 2) {
				err = BCME_BUFTOOSHORT;
				break;
			}

			if (plen >= (int)sizeof(int_val) * 2) {
				bcopy((void*)((uintptr)p + sizeof(int_val)),
					&int_val2, sizeof(int_val));
			}

			/* Fall through */
		case IOV_GVAL(IOV_DEBUG_CRASH):
			/* Immediate execution will return the register value after read */
			if (plen < (int)sizeof(int_val)) {
				err = BCME_BUFTOOSHORT;
				break;
			}

			if (int_val2 > 60) {
				err = BCME_BADARG;
				break;
			}

			if ((DBG_CRASH_TYPE(int_val) == DBG_CRSH_TYPE_RD_CORE_NO_PWR) ||
				(DBG_CRASH_TYPE(int_val) == DBG_CRSH_TYPE_WR_CORE_NO_PWR)) {
				err = BCME_UNSUPPORTED;
				break;
			}

			switch (DBG_CRASH_SRC(int_val)) {
				case DBG_CRASH_SRC_DRV:
#ifdef DONGLEBUILD
					err = BCME_BADARG;
#else
					if (((DBG_CRASH_TYPE(int_val) > DBG_CRSH_TYPE_POWERCYCLE) &&
							(DBG_CRASH_TYPE(int_val) <
							DBG_CRSH_TYPE_RADIO_HEALTHCHECK_START)) ||
							(DBG_CRASH_TYPE(int_val) >=
							DBG_CRSH_TYPE_LAST)) {
						err = BCME_BADARG;
						break;
					}
					*ret_int_ptr =
						wlc_debug_crash_execute_crash(ctxt->wlc,
							DBG_CRASH_TYPE(int_val), int_val2, &err);
#endif /* DONGLEBUILD */
					break;

				case DBG_CRASH_SRC_FW:
					if (((DBG_CRASH_TYPE(int_val) > DBG_CRSH_TYPE_HANG) &&
							(DBG_CRASH_TYPE(int_val) <
							DBG_CRSH_TYPE_RADIO_HEALTHCHECK_START)) ||
							(DBG_CRASH_TYPE(int_val) >=
							DBG_CRSH_TYPE_LAST)) {
						err = BCME_BADARG;
						break;
					}
#ifdef DONGLEBUILD

					*ret_int_ptr =
						wlc_debug_crash_execute_crash(ctxt->wlc,
							DBG_CRASH_TYPE(int_val), int_val2, &err);
#endif /* DONGLEBUILD */
					break;

				case DBG_CRASH_SRC_UCODE:
					if ((DBG_CRASH_TYPE(int_val) >=
						DBG_CRSH_TYPE_RD_CORE_NO_CLK) &&
						(DBG_CRASH_TYPE(int_val) <=
						DBG_CRSH_TYPE_PCIe_AER)) {
						err = BCME_UNSUPPORTED;
						break;
					}

					if ((DBG_CRASH_TYPE(int_val) >=
						DBG_CRSH_TYPE_BTCOEX_RFACTIVE) &&
						(DBG_CRASH_TYPE(int_val) <=
						DBG_CRSH_TYPE_BTCOEX_INVLD_TASKID)) {
						/* Force the requested BTCOEX TRAP */
						*ret_int_ptr = wlc_debug_crash_btcoex(ctxt->wlc,
							DBG_CRASH_TYPE(int_val), int_val2, &err);
					} else {
						/* Execute ucode crash immediatly */
						*ret_int_ptr = wlc_debug_crash_ucode(ctxt->wlc,
							DBG_CRASH_TYPE(int_val), int_val2, &err);
					}
					break;

				default:
					err = BCME_BADARG;
					break;
			}
			break;

		default:
			WL_ERROR(("%s - Unsupported debug IOVAR\n", __FUNCTION__));
			err = BCME_NOTFOUND;
	}
	return err;
}

static int
wlc_debug_crash_down(void *context)
{
	/* wlc_debug_crash_info_t * debug_crash_info = (wlc_debug_crash_info_t *)context; */

	BCM_REFERENCE(context);

	return 0;
}

/* **** Public Functions *** */
void
BCMATTACHFN(wlc_debug_crash_detach)(wlc_debug_crash_info_t *ctxt)
{
	if (ctxt != NULL) {

		if (ctxt->debug_crash_timer) {
			wl_del_timer(ctxt->wlc->wl, ctxt->debug_crash_timer);
			wl_free_timer(ctxt->wlc->wl, ctxt->debug_crash_timer);
			ctxt->debug_crash_timer = NULL;
			ctxt->timer_active = FALSE;
		}

		/* Unregister the module */
		wlc_module_unregister(ctxt->wlc->pub, "debug_crash", ctxt);
		/* Free the all context memory */
		MFREE(ctxt->wlc->osh, ctxt, sizeof(wlc_debug_crash_info_t));
	}
}

wlc_debug_crash_info_t *
BCMATTACHFN(wlc_debug_crash_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	wlc_debug_crash_info_t *ctxt;

	ctxt = (wlc_debug_crash_info_t*)MALLOC(pub->osh, sizeof(wlc_debug_crash_info_t));
	if (ctxt == NULL) {
		WL_ERROR(("wl%d: %s: ctxt MALLOC failed; total mallocs %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	ctxt->wlc = wlc;

	ctxt->debug_crash_timer = wl_init_timer(wlc->wl,
		wlc_debug_crash_timer_cb, wlc, "debug_crash_timer");
	ctxt->timer_active = FALSE;

	/* register module */
	if (wlc_module_register(
		wlc->pub, debug_crash_iovars, "debug_crash", ctxt, wlc_debug_crash_doiovar,
		/* wlc_debug_crash_watchdog */ NULL, NULL, wlc_debug_crash_down)) {
		WL_ERROR(("wlc_module_register() failed - %s\n", __FUNCTION__));
		goto fail;
	}

	return ctxt;

fail:
	MFREE(pub->osh, ctxt, sizeof(wlc_debug_crash_info_t));

	return NULL;
}
#endif /* WLC_DEBUG_CRASH */
