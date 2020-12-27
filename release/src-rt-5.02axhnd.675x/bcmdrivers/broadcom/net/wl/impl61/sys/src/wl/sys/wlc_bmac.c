/*
 * Common (OS-independent) portion of
 * Broadcom 802.11bang Networking Device Driver
 *
 * BMAC portion of common driver.
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
 * $Id: wlc_bmac.c 780059 2019-10-15 02:58:47Z $
 */

/**
 * @file
 * @brief
 * In contrast to the traditional NIC driver architecture, dongle devices are limited by a slow(er)
 * host-client BUS. To cope with this bus latency(significantly slower R_REG, W_REG), some host
 * driver blocks have to be moved to run on dongle on-chip memory with simple CPU(like ARM7,
 * cortexM3). Dongle driver normally requires less load on host CPU due to the offloading.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlBmacDesign]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

/* On a split driver, wlc_bmac_recv() runs in the low driver. When PKTC is defined,
 * wlc_bmac_recv() calls directly to wlc_rxframe_chainable() and wlc_sendup_chain(),
 * which run in the high driver.
 */
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <802.11.h>
#include <bcmwifi_channels.h>
#include <bcm_math.h>
#include <bcmutils.h>
#include <bcmtlv.h>
#include <d11_cfg.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <sbconfig.h>
#include <sbchipc.h>
#include <pcicfg.h>
#include <sbhndpio.h>
#include <hnddma.h>
#include <hndpmu.h>
#include <hnddma.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_pcb.h>
#include <wlc_rate.h>
#include <wlc_mbss.h>
#include <wlc_channel.h>
#include <wlc_pio.h>
#include <bcmsrom.h>
#include <wlc_rm.h>
#include <wlc_macdbg.h>
#include <sbgci.h>
#include <bcmnvram.h>
#ifdef WLSMC
#include <wlc_smc.h>
#include <d11smc_code.h>
#endif // endif
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#ifdef PROP_TXSTATUS
#include <wlc_wlfc.h>
#endif /* PROP_TAXSTATUS */
#include <wlc.h>
#include <wlc_txs.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_led.h>
#include <wl_export.h>
#include "d11ucode.h"
#include <bcmotp.h>
#include <wlc_stf.h>
#include <wlc_bsscfg.h>
#include <wlc_rsdb.h>
#include <wlc_antsel.h>
#ifdef WLDIAG
#include <wlc_diag.h>
#endif // endif
#include <pcie_core.h>
#if (defined BCMPCIEDEV && !defined(BCMPCIEDEV_DISABLED))
#include <pcieregsoffs.h>
#endif // endif
#ifdef ROUTER_COMA
#include <hndchipc.h>
#include <hndjtagdefs.h>
#endif // endif
#if defined(BCMDBG) || defined(WLTEST)
#include <bcmsrom.h>
#endif // endif
#ifdef AP
#include <wlc_apps.h>
#endif // endif
#include <wlc_alloc.h>
#include <wlc_ampdu.h>
#include "saverestore.h"
#include <wlc_iocv_low.h>
#include <wlc_iocv_fwd.h>
#include <wlc_bmac_iocv.h>
#include <wlc_dump.h>
#include <wlc_dump_reg.h>
#include <wlc_macdbg.h>
#include <phyioctl.h>
#if defined(WL_DATAPATH_LOG_DUMP)
#include <event_log.h>
#endif /* WL_DATAPATH_LOG_DUMP */
#include <hndlhl.h>
#ifdef WL_BEAMFORMING
#include <wlc_txbf.h>
#endif /* WL_BEAMFORMING */

/* ******************************************************** */
#include <phy_api.h>
#include <phy_ana_api.h>
#include <phy_btcx_api.h>
#include <phy_cache_api.h>
#include <phy_calmgr_api.h>
#include <phy_chanmgr_api.h>
#include <phy_misc_api.h>
#include <phy_radio_api.h>
#include <phy_rssi_api.h>
#include <phy_wd_api.h>
#include <phy_dbg_api.h>
#include <phy_utils_api.h>
#include <phy_tpc_api.h>
#include <phy_antdiv_api.h>
#ifdef WLC_TXPWRCAP
#include <phy_txpwrcap_api.h>
#endif // endif
#include <phy_prephy_api.h>
#include <phy_vasip_api.h>
#include <phy_tssical_api.h>
#include <d11_addr_space.h>

/* ******************************************************** */
#include <wlc_vasip.h>

#ifdef WLDURATION
#include <wlc_duration.h>
#endif // endif

#ifdef DONGLEBUILD
#include <hndcpu.h>
#endif // endif

#include <wlc_tx.h>

#if defined(WL_MONITOR) && defined(DONGLEBUILD)
#include <bcmmsgbuf.h>
#endif /* WL_MONITOR && DONGLEBUILD */

#include <wlc_btcx.h>
#ifdef WL_MU_RX
#include <wlc_murx.h>
#endif /* WL_MU_RX */

#ifdef BCMLTECOEX
#include <wlc_ltecx.h>
#include <wlc_scb.h>
#endif /* BCMLTECOEX */

#include <wlc_rx.h>

#ifdef WL_MU_TX
#include <wlc_mutx.h>
#endif // endif

#ifdef UCODE_IN_ROM_SUPPORT
#include <d11ucode_upatch.h>
/* for ucode init/download/patch routines */
#include <hndd11.h>
#endif /* UCODE_IN_ROM_SUPPORT */
#include <hndd11.h>
#include <wlc_ucinit.h>

#ifdef WLC_SW_DIVERSITY
#include <wlc_swdiv.h>
#endif /* WLC_SW_DIVERSITY */

#ifdef WLC_TSYNC
#include <wlc_tsync.h>
#endif // endif

#ifdef BCMULP
#include <wlc_ulp.h>
#include <ulp.h>
#endif /* BCMULP */

#ifdef BCMFCBS
#include <fcbs.h>
#endif /* BCMFCBS */

#include <wlc_addrmatch.h>
#include <wlc_perf_utils.h>
#include <wlc_srvsdb.h>

#ifdef DONGLEBUILD
#include <rte_trap.h>
#endif // endif

#ifdef HEALTH_CHECK
#include <hnd_hchk.h>
#endif // endif

#ifdef STB_SOC_WIFI
#include <wl_stbsoc.h>
#endif /* STB_SOC_WIFI */

#ifdef AWD_EXT_TRAP
#include <rte.h>
#include <awd.h>
#include <hnd_debug.h>
#endif /* AWD_EXT_TRAP */

#include <wlc_ratelinkmem.h>
#if defined(BCMHWA) && defined(HWA_RXFILL_BUILD)
#include <hwa_lib.h>
#include <hwa_export.h>
#endif // endif
#if defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)
#include <wlc_offld.h>
#endif /* WLC_OFFLOADS_TXSTS || WLC_OFFLOADS_RXSTS */
#ifdef WLCFP
#include <wlc_cfp.h>
#endif // endif
#if defined(BCMSPLITRX)
#include <wlc_pktfetch.h>
#endif // endif
#include <wlc_he.h>
#ifdef WL_AUXPMQ
#include <wlc_pmq.h>
#endif // endif
#ifdef BCM_BUZZZ
#include <bcm_buzzz.h>
#endif // endif
#if defined(BCM_PKTFWD)
#include <wl_pktfwd.h>
#endif /* BCM_PKTFWD */

#define WL_NATOE_NOTIFY_PKTC(wlc, action)

#ifndef EFI
#ifdef ENABLE_PANIC_CHECK_CLK
#define PANIC_CHECK_CLK(clk, format, ...)			\
	do {							\
		if (!clk) {					\
			osl_panic(format, __VA_ARGS__);		\
		}						\
	} while (0);
#else
#define PANIC_CHECK_CLK(clk, format, ...)
#endif // endif
#endif /* !EFI */

#define WL_PKTENG_LONGPKTSZ		20000	/* maximum packet length in packet engine mode */
#define	TIMER_INTERVAL_PKTENG		500	/* watchdog timer, in unit of ms */

/* QT PHY */
#define	SYNTHPU_DLY_PHY_US_QT		100	/* QT(no radio) synthpu_dly time in us */

/* real PHYs */
/*
 * XXX - In ucode BOM 820.1 and later SYNTHPU and PRETBTT are independant.
 */
#define	SYNTHPU_DLY_BPHY_US		800	/* b/g phy synthpu_dly time in us, def */
#define SYNTHPU_DLY_LCN20PHY_US		300	/* lcn20phy synthpu_dly time in us */

#define	SYNTHPU_DLY_NPHY_US		1536	/* n phy REV3 synthpu_dly time in us, def */

#ifndef PSM_INVALID_INSTR_JUMP_WAR
#define PSM_INVALID_INSTR_JUMP_WAR 0
#endif // endif

#if PSM_INVALID_INSTR_JUMP_WAR
#define	SYNTHPU_DLY_ACPHY_US		576	/* 512+64 = 576usec delay to account for the war. */
#else
#define	SYNTHPU_DLY_ACPHY_US		512
#endif // endif

#define	SYNTHPU_DLY_ACPHY2_US		1200	/* AC phy synthpu_dly time in us, def */
#define	SYNTHPU_DLY_ACPHY_4347_US	0x200	/* 4347 specific synthpu_dly time in us */
#define	SYNTHPU_DLY_ACPHY_4369_US	0x1000	/* 4369 specific synthpu_dly time in us */

/* chip specific */
#if defined(PMU_OPT_REV6)
#define SYNTHPU_DLY_ACPHY_4339_US	310 	/* acphy 4339 synthpu_dly time in us */
#else
#define SYNTHPU_DLY_ACPHY_4339_US	400 	/* acphy 4339 synthpu_dly time in us */
#endif // endif
#define SYNTHPU_DLY_ACPHY_4349_MIMO_ONECORE_US	900	/* acphy 4349 synthpu_dly onecore dly */
#define SYNTHPU_DLY_ACPHY_4349_RSDB_US	1200	/* acphy 4349 synthpu_dly RSDB mode time in us */
#define SYNTHPU_DLY_ACPHY_4349_MIMO_US	1200	/* acphy 4349 synthpu_dly MIMO mode time in us */
#define SYNTHPU_DLY_ACPHY_4364_CORE0	 0x190	/* acphy 4364 3x3 synthpu_dly time in us */
#define PKTENG_TIMEOUT			2000000 /* 2sec timeout rather indefinite spinwait */

#define SYNTHPU_DLY_ACPHY_4364_CORE1	0x1B0	/* acphy 4364 1x1 synthpu_dly time in us */
#define SYNTHPU_DLY_ACPHY_4365_US	1024	/* acphy 4365, 4366 synthpu_dly time in us */
#define PKTENG_IDLE 0
#define PKTENG_RX_BUSY 1
#define PKTENG_TX_BUSY 2

#define PHYREG_SMCBUFFERADDR	0x1409
#define PHYREG_SMCBUFFERADDR_M2SRST	(1 << 15)

typedef struct _btc_flags_ucode {
	uint8	idx;
	uint16	mask;
} btc_flags_ucode_t;

#define BTC_FLAGS_SIZE 9
#define BTC_FLAGS_MHF3_START 1
#define BTC_FLAGS_MHF3_END   6

const btc_flags_ucode_t btc_ucode_flags[BTC_FLAGS_SIZE] = {
	{MHF2, MHF2_BTCPREMPT},
	{MHF3, MHF3_BTCX_DEF_BT},
	{MHF3, MHF3_BTCX_ACTIVE_PROT},
	{MHF3, MHF3_BTCX_SIM_RSP},
	{MHF3, MHF3_BTCX_PS_PROTECT},
	{MHF3, MHF3_BTCX_SIM_TX_LP},
	{MHF3, MHF3_BTCX_ECI},
	{MHF5, MHF5_BTCX_LIGHT},
	{MHF5, MHF5_BTCX_PARALLEL}
};

#define WLC_WRITE_INITS(wlc_hw, iv_main, iv_aux)  \
	if (wlc_hw->macunit == 0) { \
		wlc_write_inits(wlc_hw, iv_main); \
	} else  if (wlc_hw->macunit == 1) {  \
		wlc_write_inits(wlc_hw, iv_aux); \
	} else { \
		ASSERT(0); \
	}

#define WLC_WRITE_UCODE(wlc_hw, ucode32, nbytes, ucode_main, ucode_main_size, \
			ucode_aux, ucode_aux_size) \
	if (wlc_hw->macunit == 0) { \
		ucode32 = ucode_main; \
		nbytes = ucode_main_size; \
	} else if (wlc_hw->macunit == 1) { \
		ucode32 = ucode_aux; \
		nbytes = ucode_aux_size; \
	} else { \
		ASSERT(0); \
	}

#ifndef BMAC_DUP_TO_REMOVE
#define WLC_RM_WAIT_TX_SUSPEND		4 /* Wait Tx Suspend */
#define	ANTCNT			10		/* vanilla M_MAX_ANTCNT value */
#endif	/* BMAC_DUP_TO_REMOVE */

#define DMAREG(wlc_hw, direction, fifonum)	(((direction == DMA_TX) ? \
		(BCM_DMA_CT_ENAB(wlc_hw->wlc) ? \
		(void*)(uintptr)(&(D11Reggrp_inddma(wlc_hw, 0)->dma)) : \
		(void*)(uintptr)&(D11Reggrp_f64regs(wlc_hw, fifonum)->dmaxmt)) : \
		(void*)(uintptr)&(D11Reggrp_f64regs(wlc_hw, fifonum)->dmarcv)))

/*
 * The following table lists the buffer memory allocated to xmt fifos in HW.
 * the size is in units of 256bytes(one block), total size is HW dependent
 * ucode has default fifo partition, sw can overwrite if necessary
 *
 * This is documented in twiki under the topic UcodeTxFifo. Please ensure
 * the twiki is updated before making changes.
 */

#define XMTFIFOTBL_STARTREV	4	/* Starting corerev for the fifo size table */

static uint16 xmtfifo_sz[][NFIFO_LEGACY] = {
	{ 14, 14, 14, 14, 14, 2 }, 	/* corerev 4: 3584, 3584, 3584, 3584, 3584, 512 */
	{ 9, 13, 10, 8, 13, 1 }, 	/* corerev 5: 2304, 3328, 2560, 2048, 3328, 256 */
	{ 9, 13, 10, 8, 13, 1 }, 	/* corerev 6: 2304, 3328, 2560, 2048, 3328, 256 */
	{ 9, 13, 10, 8, 13, 1 }, 	/* corerev 7: 2304, 3328, 2560, 2048, 3328, 256 */
	{ 9, 13, 10, 8, 13, 1 }, 	/* corerev 8: 2304, 3328, 2560, 2048, 3328, 256 */
#if (defined(MBSS) && !defined(MBSS_DISABLED))
	/* Fifo sizes are different for ucode with this support */
	{ 9, 14, 10, 9, 14, 6 }, 	/* corerev 9: 2304, 3584, 2560, 2304, 3584, 1536 */
#else
	{ 10, 14, 11, 9, 14, 2 }, 	/* corerev 9: 2560, 3584, 2816, 2304, 3584, 512 */
#endif // endif
	{ 10, 14, 11, 9, 14, 2 }, 	/* corerev 10: 2560, 3584, 2816, 2304, 3584, 512 */

	{ 9, 58, 22, 14, 14, 5 }, 	/* corerev 11: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 9, 58, 22, 14, 14, 5 }, 	/* corerev 12: 2304, 14848, 5632, 3584, 3584, 1280 */

	{ 10, 14, 11, 9, 14, 4 }, 	/* corerev 13: 2560, 3584, 2816, 2304, 3584, 1280 */
	{ 10, 14, 11, 9, 14, 2 }, 	/* corerev 14: 2560, 3584, 2816, 2304, 3584, 512 */
	{ 10, 14, 11, 9, 14, 2 }, 	/* corerev 15: 2560, 3584, 2816, 2304, 3584, 512 */

#ifdef WLLPRS
	{ 20, 176, 192, 21, 17, 5 },	/* corerev 16: 5120, 45056, 49152, 5376, 4352, 1280 */
#else /* WLLPRS */
	{ 20, 192, 192, 21, 17, 5 },	/* corerev 16: 5120, 49152, 49152, 5376, 4352, 1280 */
#endif /* WLLPRS */

#ifdef WLLPRS
	{ 20, 176, 192, 21, 17, 5 },	/* corerev 17: 5120, 45056, 49152, 5376, 4352, 1280 */
#else /* WLLPRS */
	{ 20, 192, 192, 21, 17, 5 },	/* corerev 17: 5120, 49152, 49152, 5376, 4352, 1280 */
#endif /* WLLPRS */
	{ 20, 192, 192, 21, 17, 5 },	/* corerev 18: 5120, 49152, 49152, 5376, 4352, 1280 */
	{ 20, 192, 192, 21, 17, 5 },	/* corerev 19: 5120, 49152, 49152, 5376, 4352, 1280 */
	{ 20, 192, 192, 21, 17, 5 },	/* corerev 20: 5120, 49152, 49152, 5376, 4352, 1280 */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 21: 2304, 14848, 5632, 3584, 3584, 1280 */
#ifdef WLLPRS
	{ 9, 42, 22, 14, 14, 5 }, 	/* corerev 22: 2304, 10752, 5632, 3584, 3584, 1280 */
#else /* WLLPRS */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 22: 2304, 14848, 5632, 3584, 3584, 1280 */
#endif /* WLLPRS */

	{ 20, 192, 192, 21, 17, 5 },    /* corerev 23: 5120, 49152, 49152, 5376, 4352, 1280 */

	{ 9, 58, 22, 14, 14, 5 },	/* corerev 24: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 25: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 150, 223, 223, 21, 17, 5 },	/* corerev 26: 38400, 57088, 57088, 5376, 4352, 1280 */
	{ 20, 192, 192, 21, 17, 5 },	/* corerev 27: 5120, 49152, 49152, 5376, 4352, 1280 */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 28: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 29: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 9, 98, 22, 14, 14, 5 },       /* corerev 30: 2304, 25088, 5632, 3584, 3584, 1280 */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 31: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 12, 183, 25, 17, 17, 8 },	/* corerev 32: 3072, 46848, 6400, 4352, 4352, 2048 */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 33: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 9, 183, 25, 17, 17, 8 },	/* corerev 34: 2304, 46848, 6400, 4352, 4352, 2048 */
	{ 9, 183, 25, 17, 17, 8 },	/* corerev 35: 2304, 46848, 6400, 4352, 4352, 2048 */
	{ 9, 183, 25, 17, 17, 8 },	/* corerev 36: 2304, 46848, 6400, 4352, 4352, 2048 */
	{ 9, 58, 22, 14, 14, 5 },	/* corerev 37: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 9, 183, 25, 17, 17, 8 },	/* corerev 38: 2304, 46848, 6400, 4352, 4352, 2048 */
	{ 9, 73, 14, 14, 9, 2 },	/* corerev 39: 2304, 14848, 5632, 3584, 3584, 1280 */
	{ 9, 183, 25, 17, 17, 8 },	/* corerev >=40: 2304, 46848, 6400, 4352, 4352, 2048 */
};

static uint16 _xmtfifo_sz_dummy[NFIFO_LEGACY] = { 9, 183, 25, 17, 17, 8 };

static void* BCMRAMFN(get_xmtfifo_sz)(uint *xmtsize)
{
	*xmtsize = ARRAYSIZE(xmtfifo_sz);
	return xmtfifo_sz;
}
static void* BCMRAMFN(get_xmtfifo_sz_dummy)(void)
{
	return _xmtfifo_sz_dummy;
}
/* WLP2P Support */
#ifdef WLP2P
#ifndef WLP2P_UCODE
#error "WLP2P_UCODE is not defined"
#endif // endif
#endif /* WLP2P */

/* P2P ucode Support */
#ifdef WLP2P_UCODE
	#if defined(ROM_ENAB_RUNTIME_CHECK)
		#define DL_P2P_UC(wlc_hw)	((wlc_hw)->_p2p)
	#elif defined(WLP2P_UCODE_ONLY)
		#define DL_P2P_UC(wlc_hw)	1
	#elif defined(WLMCNX_DISABLED)
		#define DL_P2P_UC(wlc_hw)	0
	#else
		#define DL_P2P_UC(wlc_hw)	((wlc_hw)->_p2p)
	#endif /* WLP2P_UCODE_ONLY */
#else /* !WLP2P_UCODE */
	#define DL_P2P_UC(wlc_hw)	0
#endif /* !WLP2P_UCODE */

typedef struct bmac_dmactl {
	uint16 txmr;		/* no. of outstanding reads */
	uint16 txpfc;		/* tx prefetch control */
	uint16 txpft;		/* tx prefetch threshold */
	uint16 txblen;		/* tx burst len */
	uint16 rxpfc;		/* rx prefetch threshold */
	uint16 rxpft;		/* rx prefetch threshold */
	uint16 rxblen;		/* rx burst len */
} bmac_dmactl_t;

#define D11MAC_BMC_STARTADDR	0	/* Specified in units of 256B */
#define D11MAC_BMC_MAXBUFS		1024
#define D11MAC_BMC_BUFSIZE_512BLOCK	1	/* 1 = 512B */
#define D11MAC_BMC_BUFSIZE_256BLOCK	0	/* 0 = 256B */
#define D11MAC_BMC_BUFS_512(sz)	((sz) / (1 << (8 + D11MAC_BMC_BUFSIZE_512BLOCK)))
#define D11MAC_BMC_BUFS_256(sz)	((sz) / (1 << (8 + D11MAC_BMC_BUFSIZE_256BLOCK)))

/* WAR REV80: CRWLDOT11M-2573 */
#define D11MAC_BMC_MAX_BUFS_REV80	2048

#define D11AC_MAX_RX_FIFO_NUM	2
#define D11AC_MAX_RXFIFO_DRAIN 32

/* Time to ensure rx dma status to become IDLE WAIT in us */
#define D11AC_IS_DMA_IDLE_TIME	1000

/* For corerev 64, 65 actually the HW can accept 4095 blocks [4095 * 512 byte = ~2M] maximum.
 * Here we limit it to 1M which we think it should be enough for NIC mode.
 */
#define D11MAC_BMC_SYSMEM_MAX_BYTES		(1024 * 1024)   /* 1M bytes maximum */

#define D11MAC_BMC_SYSMEM_MAX_BYTES_REV128	((1024 + 512) * 1024)   /* 1.5M bytes maximum */
/* rev 130 does not have a sysmem core, but a 640KB large memory in the d11 core */
#define D11MAC_BMC_MACMEM_MAX_BYTES_REV130	(640 * 1024)   /**< 640K bytes maximum */

/* D11MAC rev48, rev49 layout: BM uses 128KB sized banks
 *  bmc_startaddr = 80KB
 *  TPL FIFO#7
 *      TPL BUFs  = 24KB
 *      Deadspace = 24KB*  (deadspace to align SR ASM at start of bank1)
 *      SR ASM    = 4KB*   (allocated at start of bank1)
 * Total FIFO#7 sizing:
 *      SR disabled: 24KB
 *      SR enabled*: 52KB
 */
#define D11MAC_BMC_STARTADDR_SRASM	320 /* units of 256B => 80KB */

#define D11MAC_BMC_TPL_BUFS_BYTES	(24 * 1024)	/* Min TPL FIFO#7 size */

#define D11MAC_BMC_SRASM_OFFSET		(128 * 1024)	/* bank1, d11rev48,49 */
#define D11MAC_BMC_SRASM_BYTES		(28 * 1024)	/* deadspace + 4KB */
#define D11MAC_BMC_SRASM_NUMBUFS	D11MAC_BMC_BUFS_512(D11MAC_BMC_SRASM_BYTES)

#define D11MAC_BMC_STARTADDR            0     /* Specified in units of 256B */
#define D11MAC_BMC_MAXBUFS              1024
#define D11MAC_BMC_BUFSIZE_512BLOCK     1     /* 1 = 512B */
#define D11MAC_BMC_BUFSIZE_256BLOCK     0     /* 0 = 256B */
#define D11AC_MAX_FIFO_NUM              9
#define D11MAC_BMC_BUFS_512(sz) ((sz) / (1 << (8 + D11MAC_BMC_BUFSIZE_512BLOCK)))
#define D11MAC_BMC_BUFS_256(sz) ((sz) / (1 << (8 + D11MAC_BMC_BUFSIZE_256BLOCK)))

#define D11AC_MAX_RX_FIFO_NUM   2

#define D11MAC_BMC_TPL_BYTES	(21 * 1024)	/* 21K bytes default */

#define D11MAC_BMC_TPL_NUMBUFS	D11MAC_BMC_BUFS_512(D11MAC_BMC_TPL_BYTES) /* Note: only for 512 */

/* For 4349 core revision 50 */
#define D11MAC_BMC_TPL_NUMBUFS_PERCORE	\
	D11MAC_BMC_BUFS_256(D11MAC_BMC_TPL_BYTES_PERCORE)

#define D11CORE_TEMPLATE_REGION_START D11MAC_BMC_TPL_BYTES_PERCORE

#if (defined(MBSS) && !defined(MBSS_DISABLED))
#define D11MAC_BMC_TPL_BYTES_PERCORE   D11MAC_BMC_TPL_BYTES
#else
#define D11MAC_BMC_TPL_BYTES_PERCORE   4096        /* 4K Template bytes */
#endif /* MBSS && !MBSS_DISABLED */

#ifdef SAVERESTORE
#define D11MAC_BMC_SR_BYTES				6144	/* 6K SR bytes */
#else
#define D11MAC_BMC_SR_BYTES				0
#endif /* SAVERESTORE */
#define D11MAC_BMC_SR_NUMBUFS			\
	D11MAC_BMC_BUFS_256(D11MAC_BMC_SR_BYTES)

#define D11MAC_BMC_RXFIFO0_IDX_IS131		(NFIFO_EXT_REV131 + 1)
#define D11MAC_BMC_RXFIFO1_IDX_IS131		(NFIFO_EXT_REV131 + 2)
#define D11MAC_BMC_RXFIFO0_IDX_IS130		(38 + 1)
#define D11MAC_BMC_RXFIFO1_IDX_IS130		(38 + 2)
#define D11MAC_BMC_RXFIFO0_IDX_GE128		71
#define D11MAC_BMC_RXFIFO1_IDX_GE128		72
#define D11MAC_BMC_RXFIFO0_IDX_LT80		6
#define D11MAC_BMC_RXFIFO1_IDX_LT80		8

#define D11MAC_BMC_MAXFIFOS_IS131		25 /* tx/tmpl/rx: 22 + 1 + 2 */
#define D11MAC_BMC_MAXFIFOS_IS130		(38 + 1 + 2) /**< embedded 2x2 ax core */
#define D11MAC_BMC_MAXFIFOS_GE128		73
#define D11MAC_BMC_MAXFIFOS_LT80		9

#define D11MAC_BMC_NUM_STASEL_GE128		13           /**< BMC statistics */
#define D11MAC_BMC_NUM_STASEL_LT128		12

#define D11MAC_BMC_SELTYPE_NBIT				7

#define D11MAC_BMC_NBIT_STASEL_GE128		9
#define D11MAC_BMC_NBIT_STASEL_LT128		4

#define D11MAC_BMC_BMASK_FIFOSEL_GE128		0x7F         /**< BMC statistics */
#define D11MAC_BMC_BMASK_FIFOSEL_LT128		0xF

#define D11MAC_BMC_MAXFIFOS(rev)	(D11REV_IS(rev, 131) ? D11MAC_BMC_MAXFIFOS_IS131: \
					(D11REV_IS(rev, 130) ? D11MAC_BMC_MAXFIFOS_IS130: \
					 D11MAC_BMC_MAXFIFOS_GE128))

/* Iterative fifo ID which need buffer allocated in BM. RX-FIOF2 is excluded from list since
 * classified packets(SplitRx-4 mode) recieved in RX-FIFO2 is transferred from HW to driver buffer
 * directly(without need to go to BM)
 */
static int bmc_fifo_list_is131[D11MAC_BMC_MAXFIFOS_IS131] =
	{22, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 23, 24};

static int bmc_fifo_list_is130[D11MAC_BMC_MAXFIFOS_IS130] =
	{38, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 39, 40};

static int bmc_fifo_list_ge128[D11MAC_BMC_MAXFIFOS_GE128] =
	{70, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
	31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
	47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
	63, 64, 65, 66, 67, 68, 69, 71, 72};

static int bmc_fifo_list_lt80[D11MAC_BMC_MAXFIFOS_LT80] = {7, 0, 1, 2, 3, 4, 5, 6, 8};

static int *
BCMRAMFN(wlc_bmac_get_bmc_fifo_list)(uint rev)
{
	return D11REV_IS(rev, 131) ? bmc_fifo_list_is131 :
	        D11REV_GE(rev, 130) ? bmc_fifo_list_is130 :
	        D11REV_GE(rev, 128) ? bmc_fifo_list_ge128 :
	        bmc_fifo_list_lt80;
}

#define D11MAC_BMC_RXFIFO0_IDX(rev)	(D11REV_IS(rev, 131) ? D11MAC_BMC_RXFIFO0_IDX_IS131: \
					(D11REV_IS(rev, 130) ? D11MAC_BMC_RXFIFO0_IDX_IS130: \
					(D11REV_GE(rev, 128) ? D11MAC_BMC_RXFIFO0_IDX_GE128: \
					D11MAC_BMC_RXFIFO0_IDX_LT80)))
#define D11MAC_BMC_RXFIFO1_IDX(rev)	(D11REV_IS(rev, 131) ? D11MAC_BMC_RXFIFO1_IDX_IS131: \
					(D11REV_IS(rev, 130) ? D11MAC_BMC_RXFIFO1_IDX_IS130: \
					(D11REV_GE(rev, 128) ? D11MAC_BMC_RXFIFO1_IDX_GE128: \
					D11MAC_BMC_RXFIFO1_IDX_LT80)))
#define D11MAC_BMC_NUM_STASEL(rev)	((D11REV_GE(rev, 128) ? \
			D11MAC_BMC_NUM_STASEL_GE128: \
			D11MAC_BMC_NUM_STASEL_LT128))
#define D11MAC_BMC_NBIT_STASEL(rev)	((D11REV_GE(rev, 128) ? \
			D11MAC_BMC_NBIT_STASEL_GE128: \
			D11MAC_BMC_NBIT_STASEL_LT128))
#define D11MAC_BMC_BMASK_FIFOSEL(rev) ((D11REV_GE(rev, 128) ? \
			D11MAC_BMC_BMASK_FIFOSEL_GE128: \
			D11MAC_BMC_BMASK_FIFOSEL_LT128))
#define D11MAC_BMC_FIFO_LIST(rev)	wlc_bmac_get_bmc_fifo_list(rev)

#ifdef RAMSIZE /* RAMSIZE is not defined for NIC builds */
/**
 * Remaining SYSMEM used by the d11mac port access
 * For 4365  core revision 64: CA7 use first 1792KB sysmem space, 114688 units of 16B(128bits).
 */
#define D11MAC_SYSM_STARTADDR_H_REV64		(RAMSIZE / 16) >> 16
#define D11MAC_SYSM_STARTADDR_L_REV64		(RAMSIZE / 16) & 0xFFFF

/**
 * 43684 core revision 128: CA7 use first 5632KB sysmem space, reserved 1536KB for BMC.
 * MAC port access to sysm start from 0x30_0000 address. Need to add the offset.
 */
#define D11MAC_SYSM_STARTADDR_H_REV128		((RAMSIZE - 0x300000)/ 16) >> 16
#define D11MAC_SYSM_STARTADDR_L_REV128		((RAMSIZE - 0x300000)/ 16) & 0xFFFF
#else
#define D11MAC_SYSM_STARTADDR_H_REV64		0
#define D11MAC_SYSM_STARTADDR_L_REV64		0
#define D11MAC_SYSM_STARTADDR_H_REV128		0
#define D11MAC_SYSM_STARTADDR_L_REV128		0
#endif /* RAMSIZE */

#ifndef WLC_BMAC_DUMP_NUM_REGS
#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
#define WLC_BMAC_DUMP_NUM_REGS   14
#else
#define WLC_BMAC_DUMP_NUM_REGS   0
#endif // endif
#endif /* WLC_BMAC_DUMP_NUM_REGS */

/* low iovar table registry capacity */
#ifndef WLC_IOVT_LOW_REG_SZ
#define WLC_IOVT_LOW_REG_SZ 29
#endif // endif

/* low ioctl table registry capacity */
#ifndef WLC_IOCT_LOW_REG_SZ
#define WLC_IOCT_LOW_REG_SZ 4
#endif // endif

#define UCODE_NAME_LENGTH	60
#define UCODE_LINE_LENGTH	50 /* must be less then UCODE_NAME_LENGTH */

const uint16 btc_fw_params_init_vals[BTC_FW_NUM_INDICES] = {
	BTC_FW_RX_REAGG_AFTER_SCO_INIT_VAL,
	BTC_FW_RSSI_THRESH_SCO_INIT_VAL,
	BTC_FW_ENABLE_DYN_LESCAN_PRI_INIT_VAL,
	BTC_FW_LESCAN_LO_TPUT_THRESH_INIT_VAL,
	BTC_FW_LESCAN_HI_TPUT_THRESH_INIT_VAL,
	BTC_FW_LESCAN_GRANT_INT_INIT_VAL,
	0,
	BTC_FW_RSSI_THRESH_BLE_INIT_VAL,
	0,
	0,
	BTC_FW_HOLDSCO_LIMIT_INIT_VAL,
	BTC_FW_HOLDSCO_LIMIT_HI_INIT_VAL,
	BTC_FW_SCO_GRANT_HOLD_RATIO_INIT_VAL,
	BTC_FW_SCO_GRANT_HOLD_RATIO_HI_INIT_VAL,
	BTC_FW_HOLDSCO_HI_THRESH_INIT_VAL,
	BTC_FW_MOD_RXAGG_PKT_SZ_FOR_SCO_INIT_VAL,
	BTC_FW_AGG_SIZE_LOW_INIT_VAL,
	BTC_FW_AGG_SIZE_HIGH_INIT_VAL,
	BTC_FW_MOD_RXAGG_PKT_SZ_FOR_A2DP_INIT_VAL
};

#if defined(BCMDBG)
#define DUMP_BMC_ARGV_MAX		64
#endif // endif

static void
wlc_bmac_tx_fifo_suspend_wait(wlc_hw_info_t *wlc_hw, uint tx_fifo);

static void wlc_clkctl_clk(wlc_hw_info_t *wlc, uint mode);
static void wlc_coreinit(wlc_hw_info_t *wlc_hw);
static void wlc_dma_pio_init(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_reset_amt(wlc_hw_info_t *wlc_hw);

/* used by wlc_bmac_wakeucode_init() */
static void wlc_write_inits(wlc_hw_info_t *wlc_hw, const d11init_t *inits);

#ifdef WLRSDB
static void wlc_bmac_rsdb_write_inits(wlc_hw_info_t *wlc_hw, const d11init_t *common_inits,
	const d11init_t *core1_inits);
#endif /* WLRSDB */

#if !defined(UCODE_IN_ROM_SUPPORT) || defined(ULP_DS1ROM_DS0RAM)
static void wlc_ucode_write(wlc_hw_info_t *wlc_hw, const uint32 ucode[], const uint nbytes);
static void wlc_ucodex_write(wlc_hw_info_t *wlc_hw, const uint32 ucode[], const uint nbytes);
#if !defined(BCMUCDOWNLOAD) && defined(WLP2P_UCODE)
static void wlc_ucode_write_byte(wlc_hw_info_t *wlc_hw, const uint8 ucode[], const uint nbytes);
#endif /* !BCMUCDOWNLOAD && WLP2P_UCODE */
#endif /* !defined(UCODE_IN_ROM_SUPPORT) || defined(ULP_DS1ROM_DS0RAM) */

static void wlc_war_rxfifo_shm(wlc_hw_info_t *wlc_hw, uint fifo, uint fifosize);

#define FIFO_BLOCK_SIZE      256
#define HW_RXFIFO_0_WAR(rev) (D11REV_IS(rev, 61))

#ifndef BCMUCDOWNLOAD
static int wlc_ucode_download(wlc_hw_info_t *wlc_hw);
#else
#define wlc_ucode_download(wlc_hw) do {} while (0)
int wlc_process_ucodeparts(wlc_info_t *wlc, uint8 *buf_to_process);
int wlc_handle_ucodefw(wlc_info_t *wlc, wl_ucode_info_t *ucode_buf);
int wlc_handle_initvals(wlc_info_t *wlc, wl_ucode_info_t *ucode_buf);

int BCMINITDATA(cumulative_len) = 0;
#endif // endif

/* locals used for save/restore ilp perid from DS1 to DS0 */
#ifdef BCMULP
struct wlc_bmac_p2_cubby_info {
	uint16 ilp_per_h; /* ilp_per_h */
	uint16 ilp_per_l; /* ilp_per_l */
};
typedef struct wlc_bmac_p2_cubby_info wlc_bmac_p2_cubby_info_t;
static int32 wlc_bmac_get_ilp_period(uint8 *p2_cache_data);
static uint wlc_bmac_get_retention_size_cb(void *handle, ulp_ext_info_t *einfo);
static int wlc_bmac_exit_cb(void *handle, uint8 *cache_data, uint8 *p2_cache_data);
static void wlc_bmac_skip_cal(wlc_hw_info_t *wlc_hw, uint8 *p2_cache_data);

static const ulp_p2_module_pubctx_t wlc_bmac_p2_retrieve_reg_cb = {
	sizeof(wlc_bmac_p2_cubby_info_t),
	wlc_bmac_p2_retrieve_cb
};

static const ulp_p1_module_pubctx_t wlc_bmac_p1_ctx = {
	MODCBFL_CTYPE_DYNAMIC,
	NULL,
	wlc_bmac_exit_cb,
	wlc_bmac_get_retention_size_cb,
	NULL,
	NULL
};
#endif /* BCMULP */

static int wlc_reset_accum_pmdur(wlc_info_t *wlc);

d11init_t *BCMINITDATA(initvals_ptr) = NULL;
uint32 BCMINITDATA(initvals_len) = 0;

static void wlc_ucode_txant_set(wlc_hw_info_t *wlc_hw);

/**
 * The following variable used for dongle images which have ucode download feature. Since ucode is
 * downloaded in chunks & written to ucode memory it is necessary to identify the first chunk, hence
 * the variable which gets reclaimed in attach phase.
*/
uint32 ucode_chunk = 0;

#if defined(WL_TXS_LOG)
static wlc_txs_hist_t *wlc_bmac_txs_hist_attach(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_txs_hist_detach(wlc_hw_info_t *wlc_hw);
#endif // endif

#ifdef WLLED
static void wlc_bmac_led_hw_init(wlc_hw_info_t *wlc_hw);
#endif // endif

/* used by wlc_down() */
static void wlc_flushqueues(wlc_hw_info_t *wlc_hw);

static void wlc_write_mhf(wlc_hw_info_t *wlc_hw, uint16 *mhfs);
#if defined(WL_PSMX)
static void wlc_mctrlx_reset(wlc_hw_info_t *wlc_hw);
static int wlc_bmac_wowlucodex_start(wlc_hw_info_t *wlc_hw);
static bool wlc_psmx_hw_supported(uint corerev);
#else
#define wlc_mctrlx_reset(a) do {} while (0)
#define wlc_bmac_wowlucodex_start(a) 0
#endif /* WL_PSMX */

#if defined(WL_PSMR1)
static bool wlc_psmr1_hw_supported(uint corerev);
static void wlc_bmac_mctrlr1(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val);
static void wlc_write_mhfr1(wlc_hw_info_t *wlc_hw, uint16 *mhfs);
#else
#define wlc_bmac_mctrlr1(a, b, c) do {} while (0)
#define wlc_write_mhfr1(wlc_hw, mhfs) do {} while (0)
#endif /* WL_PSMR1 */

static void wlc_bmac_btc_btcflag2ucflag(wlc_hw_info_t *wlc_hw);
static bool wlc_bmac_btc_param_to_shmem(wlc_hw_info_t *wlc_hw, uint32 *pval);
static bool wlc_bmac_btc_flags_ucode(uint8 val, uint8 *idx, uint16 *mask);
static void wlc_bmac_btc_flags_upd(wlc_hw_info_t *wlc_hw, bool set_clear, uint16, uint8, uint16);
static void wlc_bmac_btc_gpio_disable(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_btc_gpio_configure(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_gpio_configure(wlc_hw_info_t *wlc_hw, bool is_uppath);
#if defined(BCMDBG)
static void wlc_bmac_btc_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b);
#endif // endif

typedef wlc_dump_reg_fn_t bmac_dump_fn_t;

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_PHYDUMP)
static int wlc_bmac_add_dump_fn(wlc_hw_info_t *wlc_hw, const char *name,
	bmac_dump_fn_t fn, const void *ctx);
static int wlc_bmac_dump_phy(wlc_hw_info_t *wlc_hw, const char *name, struct bcmstrbuf *b);
#endif // endif
#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
static void wlc_bmac_suspend_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b);
#endif // endif
static int wlc_bmac_register_dumps(wlc_hw_info_t *wlc_hw);

/* Low Level Prototypes */
static uint16 wlc_bmac_read_objmem16(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 sel);
static uint32 wlc_bmac_read_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 sel);
static void wlc_bmac_write_objmem16(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v, uint32 sel);
static void wlc_bmac_write_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 v, uint32 sel);
static void wlc_bmac_update_objmem16(wlc_hw_info_t *wlc_hw, uint byte_offset,
	uint16 v, uint16 mask, uint32 objsel);
static bool wlc_bmac_attach_dmapio(wlc_hw_info_t *wlc_hw, bool wme);
static void wlc_bmac_detach_dmapio(wlc_hw_info_t *wlc_hw);
static int wlc_ucode_bsinit(wlc_hw_info_t *wlc_hw);
static bool wlc_validboardtype(wlc_hw_info_t *wlc);
static bool wlc_isgoodchip(wlc_hw_info_t* wlc_hw);
static char* wlc_get_macaddr(wlc_hw_info_t *wlc_hw, uint unit);
static void wlc_mhfdef(wlc_hw_info_t *wlc_hw, uint16 *mhfs);
#if defined(WL_PSMX)
static void wlc_mxhfdef(wlc_hw_info_t *wlc_hw, uint16 *mhfs);
#endif /* WL_PSMX */
static void wlc_mctrl_write(wlc_hw_info_t *wlc_hw);
static void wlc_ucode_mute_override_set(wlc_hw_info_t *wlc_hw);
static void wlc_ucode_mute_override_clear(wlc_hw_info_t *wlc_hw);
static uint32 wlc_wlintrsoff(wlc_hw_info_t *wlc_hw);
static void wlc_wlintrsrestore(wlc_hw_info_t *wlc_hw, uint32 macintmask);
#ifdef BCMDBG
static bool wlc_intrs_enabled(wlc_hw_info_t *wlc_hw);
#endif /* BCMDBG */
static int wlc_bmac_btc_param_attach(wlc_info_t *wlc);
static void wlc_bmac_btc_param_init(wlc_hw_info_t *wlc_hw);
static void wlc_corerev_fifofixup(wlc_hw_info_t *wlc_hw);
static void wlc_gpio_init(wlc_hw_info_t *wlc_hw);
static int wlc_corerev_fifosz_validate(wlc_hw_info_t *wlc_hw, uint16 *buf);
static int wlc_bmac_bmc_init(wlc_hw_info_t *wlc_hw);
static bool wlc_bmac_txfifo_sz_chk(wlc_hw_info_t *wlc_hw);
static void wlc_write_hw_bcntemplate0(wlc_hw_info_t *wlc_hw, void *bcn, int len);
static void wlc_write_hw_bcntemplate1(wlc_hw_info_t *wlc_hw, void *bcn, int len);
static void wlc_bmac_bsinit(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool chanswitch_path);
static uint32 wlc_setband_inact(wlc_hw_info_t *wlc_hw, uint bandunit);
static void wlc_bmac_setband(wlc_hw_info_t *wlc_hw, uint bandunit, chanspec_t chanspec);
static void wlc_bmac_update_slot_timing(wlc_hw_info_t *wlc_hw, bool shortslot);
#ifdef WL11N
static void wlc_upd_ofdm_pctl1_table(wlc_hw_info_t *wlc_hw);
static uint16 wlc_bmac_ofdm_ratetable_offset(wlc_hw_info_t *wlc_hw, uint8 rate);
#endif // endif
#if defined(BCMDBG)
static int wlc_bmac_bmc_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b);
#endif // endif

#ifdef NOT_RIGGED_UP_YET
/* switch phymode supported on RSDB family of chips */
static int wlc_bmac_switch_phymode(wlc_hw_info_t *wlc_hw, uint16 requested_phymode);
#endif /* NOT_RIGGED_UP_YET */

static uint8 wlc_bmac_rxfifo_enab(wlc_hw_info_t *hw, uint fifo);

static void wlc_bmac_enable_ct_access(wlc_hw_info_t *wlc_hw, bool enabled);

static uint32 wlc_bmac_txfifo_flush_status(wlc_hw_info_t *wlc_hw, uint fifo);
static uint32 wlc_bmac_txfifo_suspend_status(wlc_hw_info_t *wlc_hw, uint fifo);

void wlc_bmac_init_core_reset_disable_fn(wlc_hw_info_t *wlc_hw);
void wlc_bmac_core_reset(wlc_hw_info_t *wlc_hw, uint32 flags, uint32 resetflags);
void wlc_bmac_core_disable(wlc_hw_info_t *wlc_hw, uint32 bits);
static uint wlc_numd11coreunits(si_t *sih);
bool wlc_bmac_islast_core(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_4349_btcx_prisel_war(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_bmc_template_allocstatus(wlc_hw_info_t *wlc_hw,
uint32 mac_core_unit, int tplbuf);
#ifdef DONGLEBUILD
extern void wlc_bmac_btc_params_save(wlc_hw_info_t *wlc_hw);
#endif /* DONGLEBUILD */

static void wlc_setxband(wlc_hw_info_t *wlc_hw, uint bandunit);

#if (defined(WLTEST) || defined(WLPKTENG))
static void wlc_bmac_long_pkt(wlc_hw_info_t *wlc_hw, bool set);
static void wlc_bmac_pkteng_timer_cb(void *arg);
static void wlc_bmac_pkteng_timer_async_rx_cb(void *arg);

#ifdef PKTENG_TXREQ_CACHE
static int wlc_bmac_pkteng_cache(wlc_hw_info_t *wlc_hw, uint32 flags,
	uint32 nframes, uint32 delay, void* p);
static bool wlc_bmac_pkteng_check_cache(wlc_hw_info_t *wlc_hw);
#endif /* PKTENG_TXREQ_CACHE */
#endif // endif
static void wlc_bmac_enable_mac_clkgating(wlc_info_t *wlc);

static void wlc_bmac_phy_reset_preattach(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_core_phy_clk_preattach(wlc_hw_info_t *wlc_hw);
static void wlc_vasip_preattach_init(wlc_hw_info_t *wlc_hw);

static uint wlc_bmac_phase1_phy_attach(wlc_hw_info_t *wlc_hw);
static int wlc_bmac_phase1_phy_detach(wlc_hw_info_t *wlc_hw);
static uint32 wlc_bmac_phase1_get_resetflags(wlc_hw_info_t *wlc_hw);

#if defined(STS_FIFO_RXEN)|| defined(WLC_OFFLOADS_RXSTS)
static bool wlc_bmac_phyrxsts_alloc(wlc_hw_info_t *wlc_hw);
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

#ifdef BCMDBG_SR
static void wlc_sr_timer(void *arg);
#endif // endif
#ifdef AWD_EXT_TRAP
static uint8 *wlc_bmac_trap_phy(void *arg, trap_t *tr, uint8 *dst, int *dst_maxlen);
static uint8 *wlc_bmac_trap_mac(void *arg, trap_t *tr, uint8 *dst, int *dst_maxlen);
#endif /* AWD_EXT_TRAP */

#if defined(WLVASIP) && !defined(WLVASIP_DISABLED)
static bool vasip_activate = TRUE;
#else
static bool vasip_activate = FALSE;
#endif // endif
#define VASIP_ACTIVATE() (vasip_activate)

#if defined(WL_RX_DMA_STALL_CHECK)
/*
 * RX DMA Stall detection support
 * WLCNT support is required for rx overflow tracking.
 */

#define WLC_RX_DMA_STALL_HWFIFO_NUM     2  /** num hw rx fifos */
#define WLC_RX_DMA_STALL_DMA_NUM        3  /** num DMAs serving rx */

#if !defined(WLC_RX_DMA_STALL_TIMEOUT)
#define WLC_RX_DMA_STALL_TIMEOUT        2  /** default rx stall detection */
#endif // endif

/** flag def for wlc_rx_dma_stall_t.flags */
#define WL_RX_DMA_STALL_F_ASSERT        1 /**< assert/trap on stall in addition to report */

/** Bit defs for wlc_bmac_rx_dma_stall_force() testword, iovar "rx_dma_stall_force" */
#define WL_RX_DMA_STALL_TEST_ENABLE     0x1000
#define WL_RX_DMA_STALL_TEST_FIFO_MASK  0x00FF

typedef struct wlc_rx_dma_stall {
	uint	flags;
	uint    timeout;                                  /** thresh in sec to consider RX stall */
	uint    count      [WLC_RX_DMA_STALL_HWFIFO_NUM]; /** count of seconds with no progress */
	uint32  prev_ovfl  [WLC_RX_DMA_STALL_HWFIFO_NUM]; /** overflow count at last check */
	uint32  prev_rxfill[WLC_RX_DMA_STALL_DMA_NUM];    /** rxfills count at last check */
	uint32	rxfill     [WLC_RX_DMA_STALL_DMA_NUM];    /** Count RX DMA refill calls */
} wlc_rx_dma_stall_t;

/** Rx Stall check information */
struct wlc_rx_stall_info {
	wlc_rx_dma_stall_t *dma_stall;
};

#if defined(HEALTH_CHECK) && !defined(HEALTH_CHECK_DISABLED)
static void wlc_bmac_rx_dma_stall_report(wlc_hw_info_t *wlc_hw, int rxfifo_idx, int dma_idx,
	uint32 overflows);

static int wlc_bmac_hchk_rx_dma_stall_check(uint8 *buffer, uint16 length, void *context,
	int16 *bytes_written);
#endif /* HEALTH_CHECK */

#endif /* WL_RX_DMA_STALL_CHECK */

static const bmc_params_t *wlc_bmac_bmc_params(wlc_hw_info_t *wlc_hw);

static const char BCMATTACHDATA(rstr_devid)[] = "devid";
static const char BCMINITDATA(rstr_wlD)[] = "wl%d:dma%d";
static const char BCMINITDATA(rstr_wl_aqmD)[] = "wl%d:aqm_dma%d";
#if defined(__mips__) || defined(BCM47XX_CA9)
static const char BCMATTACHDATA(rstr_wl_tlclk)[] = "wl_tlclk";
#endif // endif
static const char BCMATTACHDATA(rstr_vendid)[] = "vendid";
static const char BCMATTACHDATA(rstr_boardrev)[] = "boardrev";
static const char BCMATTACHDATA(rstr_sromrev)[] = "sromrev";
static const char BCMATTACHDATA(rstr_boardflags)[] = "boardflags";
static const char BCMATTACHDATA(rstr_boardflags2)[] = "boardflags2";
static const char BCMATTACHDATA(rstr_boardflags4)[] = "boardflags4";
static const char BCMATTACHDATA(rstr_antswctl2g)[] = "antswctl2g";
static const char BCMATTACHDATA(rstr_antswctl5g)[] = "antswctl5g";
#ifdef PKTC
static const char BCMATTACHDATA(rstr_pktc_disable)[] = "pktc_disable";
#endif // endif
static const char BCMATTACHDATA(rstr_aa2g)[] = "aa2g";
static const char BCMATTACHDATA(rstr_macaddr)[] = "macaddr";
static const char BCMATTACHDATA(rstr_il0macaddr)[] = "il0macaddr";
static const char BCMATTACHDATA(rstr_et1macaddr)[] = "et1macaddr";
#ifdef WLLED
static const char BCMATTACHDATA(rstr_ledbhD)[] = "ledbh%d";
static const char BCMATTACHDATA(rstr_wl0gpioD)[] = "wl0gpio%d";
static const char BCMATTACHDATA(rstr_bmac_led_attach_out_of_mem_malloced_D_bytes)[] =
		"wlc_bmac_led_attach: out of memory, malloced %d bytes";
static const char BCMATTACHDATA(rstr_wlD_led_attach_wl_init_timer_for_led_blink_timer_failed)[] =
		"wl%d: wlc_led_attach: wl_init_timer for led_blink_timer failed\n";
#endif /* WLLED */
static const char BCMATTACHDATA(rstr_btc_paramsD)[] = "btc_params%d";
static const char BCMATTACHDATA(rstr_btc_flags)[] = "btc_flags";
static const char BCMATTACHDATA(rstr_btc_mode)[] = "btc_mode";
static const char BCMATTACHDATA(rstr_wowl_gpio)[] = "wowl_gpio";
static const char BCMATTACHDATA(rstr_wowl_gpiopol)[] = "wowl_gpiopol";
static const char BCMATTACHDATA(tx_burstlen_d11dma)[] = "tx_burstlen_d11dma";
#ifdef GPIO_TXINHIBIT
static const char BCMATTACHDATA(rstr_gpio_pullup_en)[] = "gpio_pullup_en";
#endif // endif
static const char BCMATTACHDATA(rstr_rsdb_mode)[] = "rsdb_mode";
static void wlc_bmac_update_dma_rx_ctrlflags_bhalt(wlc_hw_info_t *wlc_hw);

#ifdef BCMULP
/* This function registers for phase2 data retrieval. */
int
wlc_bmac_ulp_preattach(void)
{
	int err = BCME_OK;
	ULP_DBG(("%s enter\n", __FUNCTION__));

	/* bmac stores ilp period and that needs to be used just after ucode dnld */
	err = ulp_p2_module_register(ULP_MODULE_ID_BMAC, &wlc_bmac_p2_retrieve_reg_cb);

	return err;
}

/* phase2 data retrieval callback */
int
wlc_bmac_p2_retrieve_cb(void *handle, osl_t *osh, uint8 *p2_cache_data)
{
	p2_handle_t *p2_handle = (p2_handle_t *)handle;
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)p2_handle->wlc_hw;
	wlc_bmac_p2_cubby_info_t *ilp_info;

	ilp_info = (wlc_bmac_p2_cubby_info_t*)p2_cache_data;

	/* slowcal related shm's */
	ilp_info->ilp_per_h = wlc_bmac_read_shm(wlc_hw, M_ILP_PER_H(p2_handle));
	ilp_info->ilp_per_l = wlc_bmac_read_shm(wlc_hw, M_ILP_PER_L(p2_handle));

	WL_TRACE(("%s: save_period ilp_info->ilp_per_h = %x ilp_info->ilp_per_l = %x\n",
		__FUNCTION__, ilp_info->ilp_per_h, ilp_info->ilp_per_l));

	return BCME_OK;
}

/* restore ilp period */
static int32
wlc_bmac_get_ilp_period(uint8 *p2_cache_data)
{
	wlc_bmac_p2_cubby_info_t *ilp_info;
	uint32 ilp_period = 0;

	ilp_info = (wlc_bmac_p2_cubby_info_t*)p2_cache_data;
	ilp_period = ((ilp_info->ilp_per_h << 16) | ilp_info->ilp_per_l);

	WL_TRACE(("%s: restore ilp_period ilp_info->ilp_per_h = %x ilp_info->ilp_per_l = %x\n",
		__FUNCTION__, ilp_info->ilp_per_h, ilp_info->ilp_per_l));

	return ilp_period;
}

/* used on phase1 store/retrieval to return size of cubby required by this module */
static uint
wlc_bmac_get_retention_size_cb(void *handle, ulp_ext_info_t *einfo)
{
	/* Return size as 1 as this module is not using phase 1 cubby */
	return 1;
}

/* ulp exit callbk called in the context i.e. after download p2p ucode, when
 * ulp_p1_module_register is called and condition is warmboot
 */
static int
wlc_bmac_exit_cb(void *handle, uint8 *cache_data, uint8 *p2_cache_data)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)handle;

	/* ILP slow cal skip during warm boot */
	if (si_is_warmboot()) {
		wlc_bmac_skip_cal(wlc_hw, p2_cache_data);
	}

	return BCME_OK;
}

/** Skip DS0 calibration function */
static void
wlc_bmac_skip_cal(wlc_hw_info_t *wlc_hw, uint8 *p2_cache_data)
{
	uint32 ilp_period = 0;
	uint32 ilp_per_h = 0;
	uint32 ilp_per_l = 0;

	OR_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw), MCTL_SHM_EN);

	ilp_period = wlc_bmac_get_ilp_period(p2_cache_data);
	ilp_per_h = (ilp_period & 0xFFFF0000) >> 16;
	ilp_per_l = (ilp_period & 0x0000FFFF);

	/* Write the calibration code (period) in SHMs M_ILP_PER_L and M_ILP_PER_H */
	wlc_bmac_write_shm(wlc_hw, M_ILP_PER_L(wlc_hw), ilp_per_l);
	wlc_bmac_write_shm(wlc_hw, M_ILP_PER_H(wlc_hw), ilp_per_h);

	/* Program the ILP period */
	si_pmu_ulp_ilp_config(wlc_hw->sih, wlc_hw->osh, ilp_period);

	/* Clear bit 2 and set bit 3 of S_STREG */
	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), OBJADDR_SCR_SEL | S_STREG);
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	uint16 val16 = R_REG(wlc_hw->osh, D11_objdata(wlc_hw));
	W_REG(wlc_hw->osh, D11_objdata(wlc_hw), (val16 & ~(C_RETX_FAILURE))
		| (C_HOST_WAKEUP));
}

#endif /* BCMULP */

bool
wlc_bmac_pio_enab_check(wlc_hw_info_t *wlc_hw)
{
	BCM_REFERENCE(wlc_hw);
	return PIO_ENAB_HW(wlc_hw);
}

/** 11b/g has longer slot duration than 11g */
void
wlc_bmac_set_shortslot(wlc_hw_info_t *wlc_hw, bool shortslot)
{
	wlc_hw->shortslot = shortslot;

	if (BAND_2G(wlc_hw->band->bandtype) && wlc_hw->up) {
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
		wlc_bmac_update_slot_timing(wlc_hw, shortslot);
		wlc_bmac_enable_mac(wlc_hw);
	}
}

/**
 * Update the slot timing for standard 11b/g (20us slots)
 * or shortslot 11g (9us slots)
 * The PSM needs to be suspended for this call.
 */
static void
wlc_bmac_update_slot_timing(wlc_hw_info_t *wlc_hw, bool shortslot)
{
	osl_t *osh = wlc_hw->osh;

#if defined(WL_PWRSTATS)
	if (PWRSTATS_ENAB(wlc_hw->wlc->pub)) {
		wlc_mimo_siso_metrics_snapshot(wlc_hw->wlc, FALSE,
		    WL_MIMOPS_METRICS_SNAPSHOT_SLOTUPD);
	}
#endif /* WL_PWRSTATS */
	if (shortslot) {
		/* 11g short slot: 11a timing */
		W_REG(osh, D11_IFS_SLOT(wlc_hw), 0x0207);	/* APHY_SLOT_TIME */
		wlc_bmac_write_shm(wlc_hw, M_DOT11_SLOT(wlc_hw), APHY_SLOT_TIME);
	} else {
		/* 11g long slot: 11b timing */
		W_REG(osh, D11_IFS_SLOT(wlc_hw), 0x0212);	/* BPHY_SLOT_TIME */
		wlc_bmac_write_shm(wlc_hw, M_DOT11_SLOT(wlc_hw), BPHY_SLOT_TIME);
	}
}

/**
 * Band can change as a result of a 'wl up', band request from a higher layer or VSDB related. In
 * such a case, the ucode has to be provided with new band initialization values.
 */
static int
WLBANDINITFN(wlc_ucode_bsinit)(wlc_hw_info_t *wlc_hw)
{
	int err = BCME_OK;
#if defined(MBSS)
	bool ucode9 = TRUE;
	(void)ucode9;
#endif // endif

	/* init microcode host flags */
	wlc_write_mhf(wlc_hw, wlc_hw->band->mhfs);
#if defined(WL_PSMR1)
	if (PSMR1_ENAB(wlc_hw)) {
		wlc_write_mhfr1(wlc_hw, wlc_hw->band->mhfs);
	}
#endif /* WL_PSMR1 */

	/* do band-specific ucode IHR, SHM, and SCR inits */
#ifdef WLRSDB
	if (wlc_bmac_rsdb_cap(wlc_hw))  {
		if (D11REV_IS(wlc_hw->corerev, 56)) {
			wlc_bmac_rsdb_write_inits(wlc_hw, d11ac24bsinitvals56,
				d11ac24bsinitvals56core1);
		} else
			WL_ERROR(("wl%d: %s: corerev %d is invalid", wlc_hw->unit,
				__FUNCTION__, wlc_hw->corerev));
	} else
#endif /* WLRSDB */

#if defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM)
	if (wlc_uci_check_cap_ucode_rom_axislave(wlc_hw)) {
		wlc_uci_write_inits_with_rom_support(wlc_hw, UCODE_BSINITVALS);
	} else {
		WL_ERROR(("%s: wl%d: ROM enabled but no axi/ucode-rom cap! %d\n",
			__FUNCTION__, wlc_hw->unit, wlc_hw->corerev));
		err = BCME_UNSUPPORTED;
		goto done;
	}
/* the "#else" below is intentional */
#else /* defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM) */

	if (D11REV_IS(wlc_hw->corerev, 131)) {
		wlc_write_inits(wlc_hw, d11ax129bsinitvals131);
		wlc_write_inits(wlc_hw, d11ax129bsinitvalsx131);
	} else if (D11REV_IS(wlc_hw->corerev, 130)) {
		wlc_write_inits(wlc_hw, d11ax51bsinitvals130);
		wlc_write_inits(wlc_hw, d11ax51bsinitvalsx130);
	} else if (D11REV_IS(wlc_hw->corerev, 129)) {
		wlc_write_inits(wlc_hw, d11ax47bsinitvals129);
#if defined(WL_PSMR1)
		if (PSMR1_ENAB(wlc_hw)) {
			wlc_write_inits(wlc_hw, d11ax47bsinitvalsr1_129);
		}
#endif // endif
		wlc_write_inits(wlc_hw, d11ax47bsinitvalsx129);
	} else if (D11REV_IS(wlc_hw->corerev, 128)) {
		wlc_write_inits(wlc_hw, d11ax47bsinitvals128);
#if defined(WL_PSMR1)
		if (PSMR1_ENAB(wlc_hw)) {
			wlc_write_inits(wlc_hw, d11ax47bsinitvalsr1_128);
		}
#endif // endif
		wlc_write_inits(wlc_hw, d11ax47bsinitvalsx128);
	} else if (D11REV_IS(wlc_hw->corerev, 61)) {
		if (wlc_hw->wlc->pub->corerev_minor == 5) {
		        wlc_write_inits(wlc_hw, d11ac128bsinitvals61_5);
		} else {
			WLC_WRITE_INITS(wlc_hw, d11ac40bsinitvals61_1,
				d11ac40bsinitvals61_1_D11a);
		}
	} else if (D11REV_IS(wlc_hw->corerev, 65)) {
		wlc_write_inits(wlc_hw, d11ac33bsinitvals65);
		wlc_write_inits(wlc_hw, d11ac33bsinitvalsx65);
	} else if (D11REV_IS(wlc_hw->corerev, 49)) {
		wlc_write_inits(wlc_hw, d11ac9bsinitvals49);
	} else if (D11REV_IS(wlc_hw->corerev, 48)) {
		wlc_write_inits(wlc_hw, d11ac8bsinitvals48);
	} else if (D11REV_IS(wlc_hw->corerev, 42)) {
		wlc_write_inits(wlc_hw, d11ac1bsinitvals42);
	} else if (D11REV_IS(wlc_hw->corerev, 30)) {
		if (WLCISNPHY(wlc_hw->band))
			wlc_write_inits(wlc_hw, d11n16bsinitvals30);
	} else {
		WL_ERROR(("wl%d: %s: corerev %d is invalid", wlc_hw->unit,
			__FUNCTION__, wlc_hw->corerev));
		goto done;
	}
#endif /* defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM) */

done:
	return err;
} /* wlc_ucode_bsinit */

/** switch to new band but leave it inactive */
static uint32
WLBANDINITFN(wlc_setband_inact)(wlc_hw_info_t *wlc_hw, uint bandunit)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint32 macintmask;

	WL_TRACE(("wl%d: wlc_setband_inact\n", wlc_hw->unit));

	ASSERT(bandunit != wlc_hw->band->bandunit);
	ASSERT(si_iscoreup(wlc_hw->sih));
	ASSERT((R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC) == 0);

	/* disable interrupts */
	macintmask = wl_intrsoff(wlc->wl);

	/* radio off -- NPHY radios don't require to be turned off and on on a band switch */
	phy_radio_xband((phy_info_t *)wlc_hw->band->pi);

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	if (!WLCISACPHY(wlc_hw->band))
		wlc_bmac_core_phy_clk(wlc_hw, OFF);

	wlc_setxband(wlc_hw, bandunit);

	return (macintmask);
} /* wlc_setband_inact */

#define	PREFSZ			160

#ifdef PKTC
#define WLPREFHDRS(h, sz)
#else
#define WLPREFHDRS(h, sz)	OSL_PREF_RANGE_ST((h), (sz))
#endif // endif

#if defined(WL_MU_RX) && defined(WLCNT) && (defined(BCMDBG) || defined(WLDUMP) || \
	defined(BCMDBG_MU))

/* hacking Signal Tail field in VHT-SIG-A2 to save nss and mcs,
 * nss = bit 18:19 (for 11ac 2 bits to indicate maximum 4 nss)
 * mcs = 20:23
 */
void BCMFASTPATH
wlc_bmac_upd_murate(wlc_info_t *wlc, d11rxhdr_t *rxhdr, uchar *plcp)
{
	uint32 plcp0;
	uint8 gid;
	uint8 murate;

	/**
	 * Check AC rate
	 */
	if (D11PPDU_FT(rxhdr, wlc->pub->corerev) != FT_VHT) {
		return;
	}

	if ((plcp[0] | plcp[1] | plcp[2])) {
		plcp0 = plcp[0] | (plcp[1] << 8);
		gid = (plcp0 & VHT_SIGA1_GID_MASK) >> VHT_SIGA1_GID_SHIFT;
		if ((gid > VHT_SIGA1_GID_TO_AP) && (gid < VHT_SIGA1_GID_NOT_TO_AP)) {
			murate = D11RXHDR_ACCESS_VAL(rxhdr, wlc->pub->corerev, MuRate);
			if (D11REV_LT(wlc->hw->corerev, 128)) {
				ASSERT(((murate >> 4) & 0x7) >= 1);
				ASSERT(((murate >> 4) & 0x7) <= 4);
				plcp[5] &= ~0xFC;
				plcp[5] |= (((murate >> 4) & 0x3) - 1) << 2;
				plcp[5] |= (murate & 0xf) << 4;
			}
		}
	}
}

void BCMFASTPATH
wlc_bmac_save_murate2plcp(wlc_info_t *wlc, d11rxhdr_t *rxhdr, void *p)
{
	uchar *plcp;
	bool is_amsdu;
	uint8 pad;

	/* Skip amsdu until deaggration completed */
	if (IS_D11RXHDRSHORT(rxhdr, wlc->pub->corerev) != 0) {
		return;
	}

	pad = RXHDR_GET_PAD_LEN(rxhdr, wlc);
	is_amsdu = RXHDR_GET_AMSDU(rxhdr, wlc);

	if (is_amsdu) {
		return;
	}

	/* Hack murx rate */
	plcp = (uchar *)(PKTDATA(wlc->osh, p) + wlc->hwrxoff + pad);

	wlc_bmac_upd_murate(wlc, rxhdr, plcp);
}
#endif /* WL_MU_RX */

#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(BCM_11AXHDR_DBG)
/* Print MAC H/W generated Rx Status */
static void
wlc_print_ge128_mac_rxhdr(wlc_info_t *wlc, d11rxhdr_ge128_t *ge128_rxh)
{
	ASSERT(D11REV_GE(wlc->pub->corerev, 128));

	printf("\nwl%d: MAC H/W RX Status\n", wlc->pub->unit);

	/* Printing individual fields of hw rx status */
	printf("\tRxFrmSz:0x%04x dmaflgs:0x%02x fifo:0x%02x\n", ge128_rxh->RxFrameSize,
		ge128_rxh->dma_flags, ge128_rxh->fifo);
	printf("\tmrxs:0x%04x RxFrmSz_0:0x%04x HdrConvSt:0x%04x\n", ge128_rxh->mrxs,
		ge128_rxh->RxFrameSize_0, ge128_rxh->HdrConvSt);
	printf("\tfiltermap:0x%08x pktclass:0x%04x flwid:%04x errflgs:%04x\n", ge128_rxh->filtermap,
		ge128_rxh->pktclass, ge128_rxh->flowid, ge128_rxh->errflags);
}

/* Print uCode generated Rx Status */
static void
wlc_print_ge128_ucode_rxhdr(wlc_info_t *wlc, d11rxhdr_ge128_t *ge128_rxh)
{
	ASSERT(D11REV_GE(wlc->pub->corerev, 128));

	printf("wl%d: uCode RX Status\n", wlc->pub->unit);

	/* Printing individual fields of ucode rx status */
	printf("\tRxSt1:0x%04x RxSt2:0x%04x RxChan:%04x\n", ge128_rxh->RxStatus1,
		ge128_rxh->RxStatus2, ge128_rxh->RxChan);
	printf("\tAvbRxTime H:0x%04x L:0x%04x\n", ge128_rxh->AvbRxTimeH, ge128_rxh->AvbRxTimeL);
	printf("\tRxTsfTime H:0x%04x L:0x%04x MuRt:0x%04x\n", ge128_rxh->RxTsfTimeH,
		ge128_rxh->RxTSFTime, ge128_rxh->MuRate);
}

/* Print PHY Rx Status */
static void
wlc_print_ge128_phy_rxhdr(wlc_info_t *wlc, d11rxhdr_ge128_t *ge128_rxh)
{
	uint16 *ptr;
	uint16 len;

	ASSERT(D11REV_GE(wlc->pub->corerev, 128));

	/* Printing Phy Rxstatus words (2 bytes each word) */
	if (D11REV_GE(wlc->pub->corerev, 129)) {
		ptr = D11PHYSTSBUF_GE129_ACCESS_REF((d11rxhdr_t *)ge128_rxh, PhyRxStatus_0);
		len = D11PHYSTSBUF_GE129_ACCESS_VAL((d11rxhdr_t *)ge128_rxh, PhyRxStatusLen);
	}
	else {
		ptr = &ge128_rxh->PhyRxStatus_0;
		len = ge128_rxh->PhyRxStatusLen;
	}

	prhex("PHY RX Status", (uchar *)ptr, len);
}

/* Print Rx Status for corerev >= 128 */
static void
wlc_print_ge128_rxhdr(wlc_info_t *wlc, d11rxhdr_ge128_t *ge128_rxh)
{
	ASSERT(ge128_rxh);
	ASSERT(D11REV_GE(wlc->pub->corerev, 128));

	if (ge128_rxh) {
		ASSERT(((ge128_rxh->dma_flags & RXS_DMAFLAGS_MASK) != RXS_INVALID));

		/* Print MAC RX status first */
		wlc_print_ge128_mac_rxhdr(wlc, ge128_rxh);

		if (((ge128_rxh->dma_flags & RXS_DMAFLAGS_MASK) == RXS_MAC_UCODE) ||
			((ge128_rxh->dma_flags & RXS_DMAFLAGS_MASK) == RXS_MAC_UCODE_PHY)) {
			/* Print uCode RX status */
			wlc_print_ge128_ucode_rxhdr(wlc, ge128_rxh);
		}

		if ((ge128_rxh->dma_flags & RXS_DMAFLAGS_MASK) == RXS_MAC_UCODE_PHY) {
			/* Print PHY RX status */
			wlc_print_ge128_phy_rxhdr(wlc, ge128_rxh);
		}
	}
}
#endif /* BCMDBG || WLMSG_PRHDRS || BCM_11AXHDR_DBG */

#if defined(WLC_OFFLOADS_RXSTS)
/** The M2M/BME core DMA's phyrxsts into an array in host memory */
static sts_buff_t* BCMFASTPATH
wlc_bmac_offload_rxsts_get(wlc_hw_info_t *wlc_hw)
{
	sts_buff_t *sts_buff;

	ASSERT(STS_RX_OFFLOAD_ENAB(wlc_hw->wlc->pub));

	if ((sts_buff = STSBUF_ALLOC(&wlc_hw->sts_mempool)) != NULL) {
		/* get PhyRxSts buffer */
		if ((STSBUF_DATA(sts_buff) = wlc_offload_get_rxstatus(wlc_hw->wlc->offl,
			&STSBUF_BMEBUFIDX(sts_buff))) != NULL) {
			/* clear link of latest sts_buff */
			STSBUF_SETLINK(sts_buff, NULL);
		} else {
			/* No valid phyrxsts. Free allocated one */
			STSBUF_FREE(sts_buff, &wlc_hw->sts_mempool);
			sts_buff = NULL;
		}
	}

	return sts_buff;
} /* wlc_bmac_offload_rxsts_get */
#endif /* WLC_OFFLOADS_RXSTS */

/* sts mpool base addr */
#define MAX_HW_INSTANCES 7
uintptr sts_mp_base_addr[MAX_HW_INSTANCES];

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
/***************** Free sts buffer *************************
 * called from PKTFREE CB of rx lbuf
 * STS_MP_TABLE_ELEM_GE129: computes the address of the sts buffer,
 * given index, base address, and type of buffer.
 * pkttag:phystsbuf_idx contains the index
 */

void BCMFASTPATH
wlc_bmac_stsbuff_free(wlc_hw_info_t *wlc_hw, void *p)
{
	sts_buff_t *sts_buff;
	wlc_info_t *wlc = wlc_hw->wlc;
	void *head, *pkt;

	if (D11REV_LT(wlc->pub->corerev, 129)) {
		return;
	}

	head = p;
	while (head != NULL) {
		 pkt = head;
		/* Loop through AMSDU sub frames if any */
		while (pkt != NULL) {
			if (WLPKTTAG(pkt)->phystsbuf_idx != 0) {
				ASSERT((WLPKTTAG(pkt)->phystsbuf_idx) < STSBUF_MP_N_OBJ);
				sts_buff = STS_MP_TABLE_ELEM_GE129(sts_buff_t,
					sts_mp_base_addr[wlc_hw->unit],
					WLPKTTAG(pkt)->phystsbuf_idx);
				WLPKTTAG(pkt)->phystsbuf_idx = 0;
				STSBUF_FREE(sts_buff, &wlc_hw->sts_mempool);
			}
			pkt = PKTNEXT(wlc->osh, pkt);
		}
#if defined(PKTC) || defined(PKTC_DONGLE)
		head = PKTCLINK(head);
#else
		head = PKTLINK(head);
#endif // endif
	}

}

/* append list2 to list1 */
static void BCMFASTPATH
wlc_bmac_recv_append_rx_list(rx_list_t *list1, rx_list_t *list2)
{
	if (list2->rx_head == NULL)
		return;

	if (list1->rx_tail) {
		PKTSETLINK(list1->rx_tail, list2->rx_head);
	} else {
		list1->rx_head = list2->rx_head;
	}
	list1->rx_tail = list2->rx_tail;
}

void BCMFASTPATH
wlc_bmac_recv_append_sts_list(wlc_info_t *wlc, rx_list_t *sts_list1, rx_list_t *sts_list2)
{
#if defined(WLC_OFFLOADS_RXSTS)
	if (STS_RX_OFFLOAD_ENAB(wlc->pub)) {
		sts_buff_t *rxsts;
		while (TRUE) {
			rxsts = wlc_bmac_offload_rxsts_get(wlc->hw);
			if (rxsts == NULL) {
				break;
			}
			if (sts_list2->rx_head == NULL) {
				sts_list2->rx_head = rxsts;
			} else {
				STSBUF_SETLINK((sts_buff_t *)(sts_list2->rx_tail), rxsts);
			}
			sts_list2->rx_tail = rxsts;
		}
	}
#endif /* WLC_OFFLOADS_RXSTS */

	if (sts_list2->rx_head == NULL)
		return;

	if (sts_list1->rx_tail) {
		STSBUF_SETLINK((sts_buff_t *)(sts_list1->rx_tail),
			sts_list2->rx_head);
	} else {
		sts_list1->rx_head = sts_list2->rx_head;
	}
	 sts_list1->rx_tail = sts_list2->rx_tail;
}

/* pop prxs a.k.a sts_buff matching rxh seq num */
static sts_buff_t *BCMFASTPATH
wlc_bmac_recv_pop_sts(rx_list_t *rx_sts_list, uint16 rxh_seq)
{
	sts_buff_t *curr, *prev = NULL, *next;
	curr = rx_sts_list->rx_head;
	/* advance to next sts (advances the sts head ptr by one status) */
	while (curr != NULL) {
		if (rxh_seq == STSBUF_SEQ(curr)) {
			if (rx_sts_list->rx_head == curr) {
				rx_sts_list->rx_head = STSBUF_LINK(curr);
			} else {
				next = STSBUF_LINK(curr);
				STSBUF_SETLINK(prev, next);
			}
			STSBUF_SETLINK(curr, NULL);
			ASSERT(STSBUF_SEQ(curr) < STS_SEQNUM_MAX);
			break;
		}
		prev = curr;
		curr = STSBUF_LINK(curr);
	}

	if (rx_sts_list->rx_head == NULL) {
		rx_sts_list->rx_tail = NULL;
	} else if (rx_sts_list->rx_tail == curr) {
		rx_sts_list->rx_tail = prev;
	}

	return curr;
}

/* Attach prxs a.k.a sts_buff to skb or lbuf */
static void BCMFASTPATH
wlc_bmac_recv_sts_cons(wlc_hw_info_t *wlc_hw, uint8 idx, void *p,
		sts_buff_t *sts_buff, uint8 rxh_offset)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	d11rxhdr_t *rxh;
	uint16 rxh_seq;
	prxs_stats_t *prxs_stats = &wlc_hw->prxs_stats;
	/* XXX:
	 *  sts seq  matches the current MPDU, do the following:
	 *********************Attach a phystatus to lbuf/skb,..****************
	 *  - "STS_MP_TABLE_INDX_GE129" computes the index of a sts buffer,
	 *     given a sts mem pool base addr and buffer type.
	 *  - store 16-bit index to upper 16 bits of "d11rxhdr:uint32 filtermap"
	 *     of lbuf/skb
	 *  - store 16-bit index to "wlpkttag:phystsbuf_idx". Used to free up
	 *    sts buffer while freeing lbuf/skb, ...
	*/

	rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, p) + rxh_offset);
	rxh_seq = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) & 0xfff;
	ASSERT(!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev));
	ASSERT(rxh_seq == STSBUF_SEQ(sts_buff));

	wlc_hw->cons_seq[idx].seq = rxh_seq;
	wlc_hw->cons_seq[idx].sssn = wlc_hw->sssn;
	PRXSSTATSINCR(prxs_stats, n_rcvd);

	if (wlc_hw->sts_fifo_intr) {
		wlc_hw->sts_fifo_intr = FALSE;
		PRXSSTATSINCR(prxs_stats, n_cons_intr);
	}

	if (ISSTS_INVALID(sts_buff)) {
		STSBUF_FREE(sts_buff, &wlc_hw->sts_mempool);
		PRXSSTATSINCR(prxs_stats, n_invalid);
		return;
	}

	rxh->ge129.dma_flags = RXS_MAC_UCODE_PHY;
	WLPKTTAG(p)->phystsbuf_idx = STS_MP_TABLE_INDX_GE129(sts_buff_t,
			sts_mp_base_addr[wlc_hw->unit],
			sts_buff);
	ASSERT((WLPKTTAG(p)->phystsbuf_idx) < STSBUF_MP_N_OBJ);
	rxh->ge129.sts_buf_idx = WLPKTTAG(p)->phystsbuf_idx;
	rxh->ge129.hw_if = wlc_hw->unit;

	PRXSSTATSINCR(prxs_stats, n_cons);
}

/* returns the next pkt or the last pkt among set of pkts with same sequence.
 * rxpen_list - list of pending pkt/s with same or different sequence
 * number/s in order.
 * head, tail - list of pkt/s ready for release to upper layer.
 * head0 - pkt ready to be linked with sts_buff if available or will be held
 * on the pending list.
 */
static void *BCMFASTPATH
wlc_bmac_recv_find_last(wlc_hw_info_t *wlc_hw, void **rxpen_list,
		void **head, void **tail,
		void **head0, void **tail0, uint8 rxh_offset, bool *seq_switch)
{
	d11rxhdr_t *rxh;
	wlc_info_t *wlc = wlc_hw->wlc;
	uint16 rxh_seq, rxh_prev_seq = 0;
	void *curr_p;
	while (*rxpen_list != NULL) {
		curr_p = *rxpen_list;
		*rxpen_list  = PKTLINK(*rxpen_list);
		PKTSETLINK(curr_p, NULL);

		rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, curr_p) + rxh_offset);
		rxh_seq = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) & 0xfff;

		if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev)) {
			ASSERT(rxh_seq < STS_SEQNUM_MAX);
			/* break out on sequence jump, prxs should be present */
			if (*head0 && (rxh_seq != rxh_prev_seq)) {
				PKTSETLINK(curr_p, *rxpen_list);
				*rxpen_list = curr_p;
				*seq_switch = TRUE;
				break;
			}
			rxh_prev_seq = rxh_seq;

			if (*head0) {
				if (!(*head)) {
					*head = *head0;
				} else {
					PKTSETLINK(*tail, *head0);
					PKTSETLINK(*tail0, NULL);
				}
				*tail = *tail0;
				*head0 = *tail0 = NULL;
			}
		}

		if (!(*head0)) {
			*head0 = *tail0 = curr_p;
		} else {
			PKTSETLINK(*tail0, curr_p);
			*tail0 = curr_p;
		}
	}

	return *head0;
}

static int
wlc_bmac_prxs_stats_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)ctx;
	wlc_info_t *wlc = wlc_hw->wlc;
	sts_buff_t *sts_curr;
	void *p;
	int cnt, start = -1, last = -1;
	d11rxhdr_t *rxh;
	uint8 rxh_offset = 0;
	prxs_stats_t *prxs_stats = &wlc_hw->prxs_stats;

	if (prxs_stats->n_rcvd) {
		bcm_bprintf(b, "n_rcvd:  %d", prxs_stats->n_rcvd);
		bcm_bprintf(b, "  n_cons:  %d(%d%%)", prxs_stats->n_cons,
				(prxs_stats->n_cons * 100) / prxs_stats->n_rcvd);
		bcm_bprintf(b, "  n_cons_intr:  %d(%d%%)", prxs_stats->n_cons_intr,
				(prxs_stats->n_cons_intr * 100) / prxs_stats->n_rcvd);
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "n_invalid:  %d(%d%%)", prxs_stats->n_invalid,
				(prxs_stats->n_invalid * 100) / prxs_stats->n_rcvd);
		bcm_bprintf(b, "  n_miss:  %d(%d%%)", prxs_stats->n_miss,
				(prxs_stats->n_miss * 100) / prxs_stats->n_rcvd);
				bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "curr_sssn:  %d sssn_seq_cnt:  %d\n", wlc_hw->sssn,
			wlc_hw->seqcnt);
	bcm_bprintf(b, "fifo-%d cons_sssn:  %d cons_seq:  %d\n", RXPEN_LIST_IDX0,
			wlc_hw->cons_seq[RXPEN_LIST_IDX0].sssn,
			wlc_hw->cons_seq[RXPEN_LIST_IDX0].seq);
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		bcm_bprintf(b, "fifo-%d cons_sssn:  %d cons_seq:  %d\n", RXPEN_LIST_IDX1,
			wlc_hw->cons_seq[RXPEN_LIST_IDX1].sssn,
			wlc_hw->cons_seq[RXPEN_LIST_IDX1].seq);
	}
#endif /* BCMPCIEDEV */

	if (wlc_hw->rx_sts_list.rx_head) {
		cnt = 0;
		sts_curr = wlc_hw->rx_sts_list.rx_head;
		if (sts_curr != NULL) {
			last = start = STSBUF_SEQ(sts_curr);
		}
		while (sts_curr != NULL) {
			cnt++;
			last = STSBUF_SEQ(sts_curr);
			sts_curr = STSBUF_LINK(sts_curr);
		}
		bcm_bprintf(b, "sts-fifo: pend_cnt %d start_seq %d last_seq %d\n",
			cnt, start, last);
	}

	if (wlc_hw->rxpen_list[RXPEN_LIST_IDX0].rx_head) {
		cnt = 0;
		p = wlc_hw->rxpen_list[RXPEN_LIST_IDX0].rx_head;
#if defined(BCMHWA) && defined(HWA_RXFILL_BUILD)
		rxh_offset =  WLC_RXHDR_LEN;
#endif // endif
		rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, p) + rxh_offset);
		if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev)) {
			last = start = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) & 0xfff;
		}
		while (p != NULL) {
			rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, p) + rxh_offset);
			if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev)) {
				last = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) & 0xfff;
			}
			p = PKTLINK(p);
			cnt++;
		}
		bcm_bprintf(b, "fifo-0: pend_cnt %d start_seq %d last_seq %d\n",
			cnt, start, last);
	}

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB() &&
		(wlc_hw->rxpen_list[RXPEN_LIST_IDX1].rx_head)) {
		cnt = 0;
		p = wlc_hw->rxpen_list[RXPEN_LIST_IDX1].rx_head;
		rxh_offset =  0;
		rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, p) + rxh_offset);
		if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev)) {
			last = start = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) & 0xfff;
		}
		while (p != NULL) {
			rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, p) + 0);
			if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev)) {
				last = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) & 0xfff;
			}
			p = PKTLINK(p);
			cnt++;
		}
		bcm_bprintf(b, "fifo-2: pend_cnt  %d start_seq %d last_seq %d\n",
			cnt, start, last);
	}
#endif /* BCMPCIEDEV */

	bcm_bprintf(b, "mempool:  #Curr=%d #Fail=%d HiWtr=%d\n\n",
	            wlc->hw->sts_mempool.n_items, wlc->hw->sts_mempool.n_fail,
	            wlc->hw->sts_mempool.high_watermark);
	return BCME_OK;
}

static int
wlc_bmac_prxs_stats_dump_clr(void *ctx)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)ctx;

	memset(&wlc_hw->prxs_stats, 0, sizeof(prxs_stats_t));
	return BCME_OK;
}

static void
wlc_bmac_sts_reset(wlc_hw_info_t * wlc_hw)
{
	sts_buff_t *sts_curr, *sts_free;
	void *curr, *free;
	int idx;

	for (idx = 0; idx < MAX_RXPEN_LIST; idx++) {
		curr = wlc_hw->rxpen_list[idx].rx_head;
		while (curr != NULL) {
			free = curr;
			curr = PKTLINK(curr);
			PKTSETLINK(free, NULL);
			PKTFREE(wlc_hw->osh, free, FALSE);
		}
		wlc_hw->rxpen_list[idx].rx_head = NULL;
		wlc_hw->rxpen_list[idx].rx_tail = NULL;
	}

	sts_curr = wlc_hw->rx_sts_list.rx_head;
	while (sts_curr != NULL) {
		sts_free = sts_curr;
		sts_curr = STSBUF_LINK(sts_curr);
		STSBUF_SETLINK(sts_free, NULL);
		STSBUF_FREE(sts_free, &wlc_hw->sts_mempool);
	}
	wlc_hw->rx_sts_list.rx_head = NULL;
	wlc_hw->rx_sts_list.rx_tail = NULL;

	memset(&wlc_hw->prxs_stats, 0, sizeof(prxs_stats_t));
	wlc_hw->seqcnt = wlc_hw->sssn = 0;
#if defined(BCMPCIEDEV)
	memset(&wlc_hw->curr_seq[RXPEN_LIST_IDX], 0,
		sizeof(curr_seq_t) * MAX_RXPEN_LIST);
	wlc_hw->curr_seq[RXPEN_LIST_IDX0].seq = STS_SEQNUM_INVALID;
	wlc_hw->curr_seq[RXPEN_LIST_IDX1].seq = STS_SEQNUM_INVALID;
	memset(&wlc_hw->cons_seq[RXPEN_LIST_IDX], 0,
		sizeof(cons_seq_t) * MAX_RXPEN_LIST);
	wlc_hw->cons_seq[RXPEN_LIST_IDX0].seq = STS_SEQNUM_INVALID;
	wlc_hw->cons_seq[RXPEN_LIST_IDX1].seq = STS_SEQNUM_INVALID;
#else /* ! BCMPCIEDEV */
	wlc_hw->curr_seq[RXPEN_LIST_IDX].seq = STS_SEQNUM_INVALID;
	wlc_hw->curr_seq[RXPEN_LIST_IDX].sssn = 0;
	wlc_hw->cons_seq[RXPEN_LIST_IDX].seq = STS_SEQNUM_INVALID;
	wlc_hw->cons_seq[RXPEN_LIST_IDX].sssn = 0;
#endif /* ! BCMPCIEDEV */
}

static bool BCMFASTPATH
wlc_bmac_recv_process_seq_switch(wlc_hw_info_t *wlc_hw, uint16 rxh_seq, uint8 idx)
{
#if defined(BCMPCIEDEV)
#ifdef PRXS_DBG
	uint16 rxswrst_cnt, prxsfull_cnt;
	struct bcmstrbuf b;
	const int DUMP_SIZE = 8192;
	char *buf;
#endif /* PRXS_DBG */
	uint8 other_idx = (idx == RXPEN_LIST_IDX1) ?
			RXPEN_LIST_IDX0 : RXPEN_LIST_IDX1;
#endif /* BCMPCIEDEV */
	sts_buff_t *curr, *prev = NULL;
	rx_list_t *rx_sts_list = &wlc_hw->rx_sts_list;

	curr = rx_sts_list->rx_head;

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB()) {
		if ((wlc_hw->rxpen_list[other_idx].rx_head != NULL) &&
			(rxh_seq == wlc_hw->curr_seq[other_idx].seq)) {
			return FALSE;
		}

		if (((wlc_hw->cons_seq[other_idx].sssn == wlc_hw->sssn) ||
			(wlc_hw->cons_seq[other_idx].sssn == STS_PREV_SSSN(wlc_hw->sssn))) &&
			((wlc_hw->cons_seq[other_idx].seq == rxh_seq) ||
			(!IS_STSSEQ_ADVANCED(rxh_seq, wlc_hw->cons_seq[other_idx].seq)))) {

			while (curr != NULL) {
				if (IS_STSSEQ_ADVANCED(STSBUF_SEQ(curr), rxh_seq)) {
					break;
				}
				prev = curr;
				curr = STSBUF_LINK(curr);
				STSBUF_SETLINK(prev, NULL);
				STSBUF_FREE(prev, &wlc_hw->sts_mempool);
				++wlc_hw->seqcnt;
			}
			rx_sts_list->rx_head = curr;
			if (rx_sts_list->rx_head == NULL) {
				rx_sts_list->rx_tail = NULL;
			}

			if ((curr == NULL) || (STSBUF_SEQ(curr) != rxh_seq)) {
#ifdef PRXS_DBG
				buf = MALLOC(wlc_hw->osh, DUMP_SIZE);
				rxswrst_cnt = wlc_read_shm(wlc_hw->wlc, M_RXSWRST_CNT(wlc_hw->wlc));
				prxsfull_cnt = wlc_read_shm(wlc_hw->wlc,
					M_PHYRXSFULL_CNT(wlc_hw->wlc));
				WL_ERROR(("%s: fifo-idx %d Duplicate seq %u curr_sssn %u\n"
				"stsfifofull_cnt %u rxswrst_cnt %u rxactive %d\n",
				__FUNCTION__, idx, rxh_seq, wlc_hw->sssn,
				prxsfull_cnt, rxswrst_cnt,
				dma_rxactive(wlc_hw->di[STS_FIFO])));
				if (buf != NULL) {
					bcm_binit((void *)&b, buf, DUMP_SIZE);
					wlc_bmac_prxs_stats_dump(wlc_hw, &b);
					WL_ERROR(("%s\n", b.origbuf));
#if defined(WLC_OFFLOADS_RXSTS) && defined(BCMDBG)
					if (STS_RX_OFFLOAD_ENAB(wlc->pub)) {
						bcm_binit(&b, buf, DUMP_SIZE);
						wlc_offload_wl_dump(wlc_hw->wlc->offl, &b);
						WL_PRINT(("%s", buf));
					}
#endif // endif
					MFREE(wlc_hw->osh, buf, DUMP_SIZE);
				}
#endif /* PRXS_DBG */
				return FALSE;
			}
		}
	} else
#endif /* BCMPCIEDEV */
	{
		while (curr != NULL) {
			if (IS_STSSEQ_ADVANCED(STSBUF_SEQ(curr), rxh_seq)) {
				break;
			}
			prev = curr;
			curr = STSBUF_LINK(curr);
			STSBUF_SETLINK(prev, NULL);
			STSBUF_FREE(prev, &wlc_hw->sts_mempool);
			++wlc_hw->seqcnt;
		}
		rx_sts_list->rx_head = curr;
		if (rx_sts_list->rx_head == NULL) {
			rx_sts_list->rx_tail = NULL;
		}
	}

	if (++wlc_hw->seqcnt > (STS_SEQNUM_MAX >> 2)) {
		wlc_hw->sssn = STS_NEXT_SSSN(wlc_hw->sssn);
		wlc_hw->seqcnt = wlc_hw->seqcnt - (STS_SEQNUM_MAX >> 2);
	}

	return TRUE;
}

/**
 * XXX Starting from 43684B0(corerev >= 129), PHY status goes out on a different channel (being
 * STS-RXFIFO or BME DMA), independent of HW and UCODE RxStatus.
 * A single PHYStatus is generated per PPDU (so e.g. per AMPDU). An 8-bit sequence number is
 * indicated in both phystatus and ucode status of RxStatus for the SW to link ucode status with a
 * corresponding PhyStatus.
 *
 * Rules/usage of this function:
 * 1. All MPDUs within a received aggregate bear the same sts_seq number
 * 2. 1 Rx status is at most linked to 1 MPDU. When that happens, the status has been 'consumed'.
 *   .. meaning that at most one MPDU within an aggregate will carry a rxstatus ..
 * 3. MPDU's don't 'wait' for their rxstatus: if at the moment of receiving the MPDU the
 *   corresponding rxstatus is not available, then the MPDU will continue. When the rxstatus should
 *   happen to arrive later, the rxstatus will be discarded.
 * 4. An sts buffer is located in an array, and can thus be addressed with an index.
 * 5. Sts buffers with indices from "1" through "STSBUF_MP_N_OBJ - 1" are used to receive physts
 *   over rxfifo3/bme.
 * 6. The sts buffer with index "0" is used as a dummy "phystshdr" and is never posted to dma/bme.
 *
 * WL BMAC layer invokes single downcall to hnddma upon RXFIFO Interrupt, returning:
 * - rx_list, set of packets ready for processing based on RxFIFO "cur_desc"
 * - sts_list of status buffers data filled with PhyRxStatus, based on StsFIFO "cur_desc"
 *
 * WL BMAC layer processing: "rx_list", set of packets carrying same or different sequence number.
 * - "pops head of "sts_buffer" if matching seq_num.
 */

void BCMFASTPATH
wlc_bmac_recv_process_sts(wlc_hw_info_t *wlc_hw, uint fifo,
		rx_list_t *rx_list, rx_list_t *rx_sts_list,
		uint8 rxh_offset)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	d11rxhdr_t *rxh;
	uint16 rxh_seq;	/**< this is *not* a dot11 sequence number */
	void *curr_p, *head = NULL, *tail = NULL; /**< pointer to a received MPDU */
	void *head0, *tail0;
	sts_buff_t *sts_buff;
	void *rxpen_list; /**< list of 'pending' rx packets */
	bool seq_switch = FALSE, new_seq = FALSE;
	uint16 rxswrst_cnt, prxsfull_cnt;
	prxs_stats_t *prxs_stats = &wlc_hw->prxs_stats;
#if defined(BCMPCIEDEV)
	uint8 idx = ((fifo == RX_FIFO2) ?
		RXPEN_LIST_IDX1 : RXPEN_LIST_IDX0);
	void *data, *prev, *free;
	int len, resid;
	uint16 rxoff, rxbufsz;
#else /* ! BCMPCIEDEV */
	uint8 idx = RXPEN_LIST_IDX;
#endif /* ! BCMPCIEDEV */

#if defined(BCMHWA) && defined(HWA_RXFILL_BUILD)
	rxh_offset = ((fifo == RX_FIFO2) ? 0 : WLC_RXHDR_LEN);
#endif // endif
	BCM_REFERENCE(rxswrst_cnt);
	BCM_REFERENCE(prxsfull_cnt);
	wlc_bmac_recv_append_rx_list(&wlc_hw->rxpen_list[idx],
		rx_list);
	rx_list->rx_head = rx_list->rx_tail = NULL;
	wlc_bmac_recv_append_sts_list(wlc, &wlc_hw->rx_sts_list, rx_sts_list);
	rx_sts_list->rx_head = rx_sts_list->rx_tail = NULL;

	/* Iterate over received MPDUs, for each MPDU check if it matches the head of the status
	 * list.
	 */
	rxpen_list = wlc_hw->rxpen_list[idx].rx_head;
	while (rxpen_list != NULL) {
		curr_p = rxpen_list;
		rxpen_list  = PKTLINK(rxpen_list);
		PKTSETLINK(curr_p, NULL);

		rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, curr_p) + rxh_offset);
		rxh_seq = D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) & 0xfff;

		if (!IS_D11RXHDRSHORT(rxh, wlc->pub->corerev) &&
			!((wlc_hw->cons_seq[idx].seq == rxh_seq) &&
			((wlc_hw->cons_seq[idx].sssn == wlc_hw->sssn) ||
			(wlc_hw->cons_seq[idx].sssn == STS_PREV_SSSN(wlc_hw->sssn))))) {

			ASSERT(rxh_seq < STS_SEQNUM_MAX);
			seq_switch = FALSE;
			head0 = tail0 = NULL;
			PKTSETLINK(curr_p, rxpen_list);
			rxpen_list = curr_p;
			curr_p = wlc_bmac_recv_find_last(wlc_hw, &rxpen_list,
				&head, &tail, &head0, &tail0, rxh_offset, &seq_switch);
			ASSERT(curr_p != NULL);

			/* process arrival of new seq */
			if (!((rxh_seq == wlc_hw->curr_seq[idx].seq) &&
				((wlc_hw->curr_seq[idx].sssn == wlc_hw->sssn) ||
				(wlc_hw->curr_seq[idx].sssn == STS_PREV_SSSN(wlc_hw->sssn))))) {
				new_seq = wlc_bmac_recv_process_seq_switch(wlc_hw, rxh_seq, idx);
				if (new_seq) {
					wlc_hw->curr_seq[idx].seq = rxh_seq;
					wlc_hw->curr_seq[idx].sssn = wlc_hw->sssn;
				} else {
					/* update release list */
					if (!head) {
						head = head0;
					} else {
						PKTSETLINK(tail, head0);
						PKTSETLINK(tail0, NULL);
					}
					tail = tail0;
					continue;
				}
			}

			/* pop prxs a.k.a sts_buff matching rxh seq */
			sts_buff = wlc_bmac_recv_pop_sts(&wlc_hw->rx_sts_list, rxh_seq);
			/* consume prxs a.k.a. sts_buff */
			if ((sts_buff != NULL) || seq_switch) {
				if (sts_buff != NULL) {
					wlc_bmac_recv_sts_cons(wlc_hw, idx, head0, sts_buff,
						rxh_offset);
				} else {
					struct bcmstrbuf b;
					const int DUMP_SIZE = 8192;
					char *buf = MALLOC(wlc_hw->osh, DUMP_SIZE);
					ASSERT(rxpen_list != NULL);
					PRXSSTATSINCR(prxs_stats, n_miss);
					rxswrst_cnt = wlc_read_shm(wlc, M_RXSWRST_CNT(wlc));
					prxsfull_cnt = wlc_read_shm(wlc, M_PHYRXSFULL_CNT(wlc));
					WL_ERROR(("%s: fifo-idx %d Missed seq %u curr_sssn %u\n"
						"stsfifofull_cnt %u rxswrst_cnt %u rxactive %d\n",
						__FUNCTION__, idx, rxh_seq, wlc_hw->sssn,
						prxsfull_cnt, rxswrst_cnt,
						dma_rxactive(wlc_hw->di[STS_FIFO])));
					if (buf != NULL) {
						bcm_binit((void *)&b, buf, DUMP_SIZE);
						wlc_bmac_prxs_stats_dump(wlc_hw, &b);
						WL_ERROR(("%s", b.origbuf));
#if defined(WLC_OFFLOADS_RXSTS) && defined(BCMDBG)
						if (STS_RX_OFFLOAD_ENAB(wlc->pub)) {
							bcm_binit(&b, buf, DUMP_SIZE);
							wlc_offload_wl_dump(wlc_hw->wlc->offl, &b);
							WL_PRINT(("%s", buf));
						}
#endif // endif
						MFREE(wlc_hw->osh, buf, DUMP_SIZE);
					}
					ASSERT(0);
				}
				/* update release list */
				if (!head) {
					head = head0;
				} else {
					PKTSETLINK(tail, head0);
					PKTSETLINK(tail0, NULL);
				}
				tail = tail0;
				continue;
			} else {
				/* XXX prxs is not yet available for current seq,
				 * hold pkt until sts-fifo intr
				 */
				PKTSETLINK(tail0, rxpen_list);
				rxpen_list = head0;
				break;
			}
		}
		/* update release list */
		if (!tail) {
			head = tail = curr_p;
		} else {
			PKTSETLINK(tail, curr_p);
			tail = curr_p;
		}
	}  /* while */

	wlc_hw->rxpen_list[idx].rx_head = rxpen_list;
	if (rxpen_list == NULL) {
		wlc_hw->rxpen_list[idx].rx_tail = NULL;
	}

#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB() && (fifo == RX_FIFO2)) {
		prev = NULL;
		head0 = head;
		dma_rxparam_get(wlc_hw->di[RX_FIFO2], &rxoff, &rxbufsz);
		while (head != NULL) {
			data = PKTDATA(wlc_hw->osh, head);
			len = ltoh16(*(uint16 *)(data));
			resid = len - (rxbufsz - rxoff);
			if (resid > 0) {
				WL_ERROR(("%s wl%d:dma2: bad frame length (%d)\n",
					__FUNCTION__, wlc_hw->unit, len));
				free = head;
				if (prev != NULL) {
					head = PKTLINK(free);
					PKTSETLINK(prev, head);
				} else {
					head0 = head = PKTLINK(free);
				}
				PKTSETLINK(free, NULL);
				PKTFREE(wlc_hw->osh, free, FALSE);
				continue;
			}
			prev = head;
			head = PKTLINK(head);
		}
		head = head0;
		tail = prev;
	}
#endif /* BCMPCIEDEV */

	rx_list->rx_head = head;
	rx_list->rx_tail = tail;
} /* wlc_bmac_recv_process_sts */
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

/**
 * Called as a result of a hardware event: when the D11 core signals one or more received frames
 * on its RX FIFO(s). The received frames are then processed by firmware/driver.
 *
 * Return TRUE if more frames need to be processed. FALSE otherwise.
 * Param 'bound' indicates max. # frames to process before break out.
 */
bool BCMFASTPATH
wlc_bmac_recv(wlc_hw_info_t *wlc_hw, uint fifo, bool bound, wlc_dpc_info_t *dpc)
{
	void *p;
	void *head = NULL;
	void *tail = NULL;
	uint n = 0;
	uint corerev;
	uint corerev_minor;
	uint32 tsf_l;
	d11rxhdr_t *rxh;
	wlc_d11rxhdr_t *wrxh;
	wlc_info_t *wlc = wlc_hw->wlc;
#if defined(PKTC) || defined(PKTC_DONGLE)
	uint16 index = 0;
	void *head0 = NULL;
	bool one_chain = PKTC_ENAB(wlc->pub);
	uint bound_limit = bound ? wlc->pub->tunables->pktcbnd : -1;
#else
	uint bound_limit = bound ? wlc->pub->tunables->rxbnd : -1;
#endif // endif
#ifdef WLC_RXFIFO_CNT_ENABLE
	struct dot11_header *h;
	uint16 fc, pad = 0;
	wl_rxfifo_cnt_t *rxcnt = wlc->pub->_rxfifo_cnt;
#endif /* WLC_RXFIFO_CNT_ENABLE */
#ifdef BULKRX_PKTLIST
	rx_list_t rx_list = {0};
#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	rx_list_t rx_sts_list = {0};
#endif // endif
#endif /* BULKRX_PKTLIST */

	/* split_fifo will always hold orginal fifo number.
	   Only in the case of mode4, it will hold FIFO-1
	*/
	uint split_fifo = fifo;
	ASSERT(bound_limit > 0);
	BCM_REFERENCE(n);

#if defined(WLCFP)
#if defined(DONGLEBUILD)

#if defined(HWA_RXDATA_BUILD) && defined(HW_HDR_CONVERSION)
	/* CFP Bypass RX path */
	if ((fifo != RX_FIFO2) && CFP_RCB_ENAB(wlc->cfp)) {
		return wlc_cfp_bmac_recv(wlc_hw, fifo, dpc);
	}
#endif /* HWA_RXDATA_BUILD && HW_HDR_CONVERSION */

#else /* ! DONGLEBUILD */
	/* CFP Bypass RX path */
	if (CFP_RCB_ENAB(wlc->cfp)) {
		return wlc_cfp_bmac_recv(wlc_hw, fifo, dpc);
	}
#endif /* ! DONGLEBUILD */
#endif /* WLCFP */

	WL_NATOE_NOTIFY_PKTC(wlc, TRUE);
	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	corerev = wlc->pub->corerev;
	corerev_minor = wlc->pub->corerev_minor;

	/* get the TSF REG reading */
	wlc_bmac_read_tsf(wlc_hw, &tsf_l, NULL);

#ifdef BULKRX_PKTLIST
#ifdef STS_FIFO_RXEN
	dma_rx(wlc_hw->di[fifo], &rx_list, &rx_sts_list, bound_limit);
#else
	dma_rx(wlc_hw->di[fifo], &rx_list, NULL, bound_limit);
#endif // endif
	ASSERT(rx_list.rxfifocnt <= bound_limit);
	if (rx_list.rx_tail != NULL) {
		ASSERT(PKTLINK(rx_list.rx_tail) == NULL);
	}
	if (rx_list.rx_head != NULL) {
		ASSERT(rx_list.rx_tail != NULL);
	}

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	if (STS_RX_ENAB(wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc->pub)) {
		if (rx_sts_list.rx_head != NULL) {
			wlc_bmac_dma_rxfill(wlc->hw, STS_FIFO);
		}
		wlc_bmac_recv_process_sts(wlc_hw, fifo,
			&rx_list, &rx_sts_list, 0);
	}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */
	while (rx_list.rx_head != NULL) {
		p = rx_list.rx_head;
		rx_list.rx_head = PKTLINK(p);
		PKTSETLINK(p, NULL);
#else /* BULKRX_PKTLIST */
	/* gather received frames */
	while (1) {
#ifdef	WL_RXEARLYRC
		if (wlc_hw->rc_pkt_head != NULL) {
			p = wlc_hw->rc_pkt_head;
			wlc_hw->rc_pkt_head = PKTLINK(p);
			PKTSETLINK(p, NULL);
		} else
#endif /* WL_RXEARLYRC */
		{
			if (PIO_ENAB_HW(wlc_hw)) {
				ASSERT(fifo < NFIFO_LEGACY);
				p = wlc_pio_rx(wlc_hw->pio[fifo]);
			} else {
				p = dma_rx(wlc_hw->di[fifo]);
			}

			if (p == NULL) {
				break;
			}
		}
#endif /* BULKRX_PKTLIST */

#if defined(BCMPCIEDEV)
		/* For fifo-split rx , fifo-0/1 has to be synced up */
		if (BCMPCIEDEV_ENAB() && RXFIFO_SPLIT() && PKTISRXFRAG(wlc_hw->osh, p)) {
#ifdef BULKRX_PKTLIST
			wlc_bmac_process_split_fifo_pkt(wlc_hw, p, 0);
#else
			if (!wlc_bmac_process_split_fifo_pkt(wlc_hw, fifo, p))
				continue;
#endif /* BULKRX_PKTLIST */
			/* In mode4 it's known that non-converted copy count data will
			 * arrive in fif0-1 so setting split_fifo to RX_FIFO1
			 */
			split_fifo = RX_FIFO1;
		}
#endif /* BCMPCIEDEV */

		/* MAC/uCode/PHY RXHDR */
		rxh = (d11rxhdr_t *)PKTDATA(wlc_hw->osh, p);
#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
		if ((STS_RX_ENAB(wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc->pub)) &&
		    (D11RXHDR_GE129_ACCESS_VAL(rxh, dma_flags) & RXS_DMAFLAGS_MASK) !=
		     RXS_MAC_UCODE_PHY) {
			rxh->ge129.hw_if = wlc_hw->unit;
		};
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */
		if (D11REV_IS(wlc_hw->corerev, 128) &&
		    D11PPDU_FT(rxh, wlc->pub->corerev) == FT_CCK) {
			char *p1 = (char*)&rxh->ge128.PhyRxStatus_0;
			int i;
			p1 += 4; /* points now at where the RSSI byte of core 0 should be placed */
			for (i = 0; i < 4; i++) {
				*p1 = *(p1 + 1);
				p1++;
			}
		}

#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(BCM_11AXHDR_DBG)
		if (D11REV_GE(wlc_hw->corerev, 128) && WL_PRHDRS_ON() && WL_APSTA_RX_ON()) {
			wlc_print_ge128_rxhdr(wlc, &rxh->ge128);
		}
#endif /* BCMDBG || WLMSG_PRHDRS || BCM_11AXHDR_DBG */

		ASSERT(PKTHEADROOM(wlc_hw->osh, p) >= WLC_RXHDR_LEN);
		/* reserve room for SW RXHDR */
		wrxh = (wlc_d11rxhdr_t *)PKTPUSH(wlc_hw->osh, p, WLC_RXHDR_LEN);

#ifdef CTFMAP
		/* sanity check */
		if (CTF_ENAB(kcih)) {
			ASSERT(((uintptr)wrxh & 31) == 0);
		}
#endif /* CTFMAP */

		/* fixup the "fifo" field in d11rxhdr_t */
		if (IS_D11RXHDRSHORT(rxh, corerev)) {
			/* short rx status received */
			uint8 *rxh_fifo =
				D11RXHDRSHORT_ACCESS_REF(rxh, corerev, corerev_minor, fifo);
			*rxh_fifo = (uint8)split_fifo;
		} else {
			uint8 *rxh_fifo =
				D11RXHDR_ACCESS_REF(rxh, corerev, fifo);
			*rxh_fifo = (uint8)split_fifo;
		}

#ifdef WLC_TSYNC
		if (TSYNC_ACTIVE(wlc->pub))
			wlc_tsync_update_ts(wlc->tsync, p, D11RXHDR_ACCESS_VAL(rxh,
				wlc->pub->corerev, RxTSFTime),
				(D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev,
				AvbRxTimeH) << 16) |
				D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev,
				AvbRxTimeL), 0);
#endif // endif

#if (defined(WL_MU_RX) && defined(WLCNT) && (defined(BCMDBG) || defined(WLDUMP) || \
	defined(BCMDBG_MU)))

		if (MU_RX_ENAB(wlc) && wlc_murx_active(wlc->murx))
			wlc_bmac_save_murate2plcp(wlc, rxh, p);
#endif /* WL_MU_RX */

		/* Convert the RxChan to a chanspec for pre-rev40 devices
		 * The chanspec will not have sideband info on this conversion.
		 */
		if (D11REV_LT(corerev, 40)) {
			/* receive channel in host byte order */
			uint16 *rxchan = D11RXHDR_ACCESS_REF(rxh, corerev, RxChan);

			*rxchan = (
				/* channel */
				((*rxchan & RXS_CHAN_ID_MASK) >> RXS_CHAN_ID_SHIFT) |
				/* band */
				((*rxchan & RXS_CHAN_5G) ? WL_CHANSPEC_BAND_5G :
				WL_CHANSPEC_BAND_2G) |
				/* bw */
				((*rxchan & RXS_CHAN_40) ? WL_CHANSPEC_BW_40 :
				WL_CHANSPEC_BW_20) |
				/* bogus sideband */
				WL_CHANSPEC_CTL_SB_L);
		}
#if defined(PKTC) || defined(PKTC_DONGLE)
		if (BCMSPLITRX_ENAB()) {
			/* if management pkt  or data in fifo-1 skip chainable checks */
			if (fifo == PKT_CLASSIFY_FIFO) {
				one_chain = FALSE;
			}
			/* Chaining is intentionally disabled in monitor mode */
			if (MONITOR_ENAB(wlc_hw->wlc)) {
				one_chain = FALSE;
			}
#ifdef WLC_RXFIFO_CNT_ENABLE
			/* Decode d11 header to extract frame type */
			pad = RXHDR_GET_PAD_LEN(rxh, wlc);
			h = (struct dot11_header *)(PKTDATA(wlc->osh, p) +
				wlc->hwrxoff + pad + D11_PHY_RXPLCP_LEN(wlc_hw->corerev));
			fc = ltoh16(h->fc);

			if ((FC_TYPE(fc) == FC_TYPE_DATA))
				WLCNTINCR(rxcnt->rxf_data[fifo]);
			else
				WLCNTINCR(rxcnt->rxf_mgmtctl[fifo]);
#endif /* WLC_RXFIFO_CNT_ENABLE */
		}

		ASSERT(PKTCLINK(p) == NULL);
		/* if current frame hits the hot bridge cache entry, and if it
		 * belongs to the burst received from same source and going to
		 * same destination then it is a candidate for chained sendup.
		 */
		if (one_chain && !wlc_rxframe_chainable(wlc, &p, index)) {
			one_chain = FALSE;
			/* breaking chain from here, first half of burst can
			 * be sent up as one. frames in the other half are
			 * sent up individually.
			 */
			if (tail != NULL) {
				head0 = head;
				tail = NULL;
			}
		}

		if (p != NULL) {
			index++;
			PKTCENQTAIL(head, tail, p);
		}
#else /* PKTC || PKTC_DONGLE */
		if (!tail)
			head = tail = p;
		else {
			PKTSETLINK(tail, p);
			tail = p;
		}
#endif /* PKTC || PKTC_DONGLE */

#ifdef BCMDBG_POOL
		PKTPOOLSETSTATE(p, POOL_RXD11);
#endif // endif

#ifndef BULKRX_PKTLIST
		/* !give others some time to run! */
		if (++n >= bound_limit) {
			break;
		}
#endif /* BULKRX_PKTLIST */
	}

#ifdef BULKRX_PKTLIST
	ASSERT(rx_list.rx_head == NULL);
	rx_list.rx_tail = NULL;
#endif // endif

	/* post more rbufs */
	if (!PIO_ENAB_HW(wlc_hw)) {
		wlc_bmac_dma_rxfill(wlc_hw, fifo);
	}
#if defined(PKTC) || defined(PKTC_DONGLE)
	/* see if the chain is broken */
	if (head0 != NULL) {
		WL_TRACE(("%s: partial chain %p\n",
				__FUNCTION__, OSL_OBFUSCATE_BUF(head0)));
		wlc_sendup_chain(wlc, head0);
	} else {
		if (one_chain && (head != NULL)) {
			/* send up burst in one shot */
			WL_TRACE(("%s: full chain %p sz %d\n", __FUNCTION__,
				OSL_OBFUSCATE_BUF(head), n));
			wlc_sendup_chain(wlc, head);
			WL_NATOE_NOTIFY_PKTC(wlc, FALSE);

#if defined(BCM_PKTFWD) && defined(WL_PKTQUEUE_RXCHAIN)
			/* Flush packets accumulated in pktqueues */
			wl_pktfwd_flush_pktqueues(wlc->wl);
#endif /* BCM_PKTFWD && WL_PKTQUEUE_RXCHAIN */

#ifdef BULKRX_PKTLIST
			dpc->processed += rx_list.rxfifocnt;
			return (rx_list.rxfifocnt >= bound_limit);
#else
			dpc->processed += n;
			return (n >= bound_limit);
#endif /* BULKRX_PKTLIST */
		}
	}
#endif /* PKTC || PKTC_DONGLE */

		/* prefetch the headers */
	if (head != NULL) {
		WLPREFHDRS(PKTDATA(wlc_hw->osh, head), PREFSZ);
	}

	/* process each frame */
	while ((p = head) != NULL) {
#if defined(PKTC) || defined(PKTC_DONGLE)
		head = PKTCLINK(head);
		PKTSETCLINK(p, NULL);
		WLCNTINCR(wlc->pub->_cnt->unchained);
#else
		head = PKTLINK(head);
		PKTSETLINK(p, NULL);
#endif // endif

		/* prefetch the headers */
		if (head != NULL) {
			WLPREFHDRS(PKTDATA(wlc_hw->osh, head), PREFSZ);
		}

		wrxh = (wlc_d11rxhdr_t *)PKTDATA(wlc_hw->osh, p);

		/* record the tsf_l in wlc_rxd11hdr */
		/* On monolithic driver, write tsf in host byte order. rx status already
		 * in host byte order.
		 */
		wrxh->tsf_l = tsf_l;

		if (!(IS_D11RXHDRSHORT(&wrxh->rxhdr, corerev))) {
			/* compute the RSSI from d11rxhdr and record it in wlc_rxd11hr */
			phy_rssi_compute_rssi((phy_info_t *)wlc_hw->band->pi, wrxh);
		}

		wlc_recv(wlc, p);
	}
	WL_NATOE_NOTIFY_PKTC(wlc, FALSE);

#if defined(BCM_PKTFWD) && defined(WL_PKTQUEUE_RXCHAIN)
	/* Flush packets accumulated in pktqueues */
	wl_pktfwd_flush_pktqueues(wlc->wl);
#endif /* BCM_PKTFWD && WL_PKTQUEUE_RXCHAIN */

#ifdef BULKRX_PKTLIST
	dpc->processed += rx_list.rxfifocnt;
	return (rx_list.rxfifocnt >= bound_limit);
#else
	dpc->processed += n;
	return (n >= bound_limit);
#endif /* BULKRX_PKTLIST */
} /* wlc_bmac_recv */

#ifdef WLP2P_UCODE
/** ucode generates p2p specific interrupts. Low level p2p interrupt processing */
void
wlc_p2p_bmac_int_proc(wlc_hw_info_t *wlc_hw)
{
	uint b, i;
	uint8 p2p_interrupts[M_P2P_BSS_MAX];
	uint32 tsf_l, tsf_h;

	ASSERT(DL_P2P_UC(wlc_hw));
	ASSERT(wlc_hw->p2p_shm_base != (uint)~0);

	memset(p2p_interrupts, 0, sizeof(uint8) * M_P2P_BSS_MAX);

	/* collect and clear p2p interrupts */
	for (b = 0; b < M_P2P_BSS_MAX; b ++) {

		for (i = 0; i < M_P2P_I_BLK_SZ; i ++) {
			uint loc = wlc_hw->p2p_shm_base + M_P2P_I(wlc_hw, b, i);

			/* any P2P event/interrupt? */
			if (wlc_bmac_read_shm(wlc_hw, loc) == 0)
				continue;

			/* ACK */
			wlc_bmac_write_shm(wlc_hw, loc, 0);

			/* store */
			p2p_interrupts[b] |= (1 << i);
#ifdef BCMDBG
			/* Update p2p Interrupt stats. */
			wlc_update_p2p_stats(wlc_hw->wlc, i);
#endif // endif
		}
	}

	wlc_bmac_read_tsf(wlc_hw, &tsf_l, &tsf_h);
#ifdef WLMCNX
	wlc_p2p_int_proc(wlc_hw->wlc, p2p_interrupts, tsf_l, tsf_h);
#endif // endif
} /* wlc_p2p_bmac_int_proc */
#endif /* WLP2P_UCODE */

/**
 * Used for test functionality (packet engine / diagnostics), or for BMAC and offload firmware
 * builds.
 */
void
wlc_bmac_txfifo(wlc_hw_info_t *wlc_hw, uint fifo, void *p,
	bool commit, uint16 frameid, uint8 txpktpend)
{
	wlc_info_t *wlc;
	struct scb *scb;

	ASSERT(p);
	wlc = wlc_hw->wlc;
	scb = WLPKTTAGSCBGET(p);

	if ((frameid == INVALIDFID) && !scb) {
		scb = wlc->band->hwrs_scb;
		WLPKTTAGSCBSET(p, scb);
	}

	ASSERT(scb != NULL);
	SCB_PKTS_INFLT_FIFOCNT_INCR(scb, PKTPRIO(p));

	/* bump up pending count */
	if (commit) {
		TXPKTPENDINC(wlc_hw->wlc, fifo, txpktpend);
		wlc_low_txq_buffered_inc(wlc_hw->wlc->active_queue,
			fifo, txpktpend);

		WL_TRACE(("wlc_bmac_txfifo, pktpend inc %d to %d\n", txpktpend,
			TXPKTPENDGET(wlc_hw->wlc, fifo)));
	}

	if (!PIO_ENAB_HW(wlc_hw)) {
		/* Commit BCMC sequence number in the SHM frame ID location */
		if (frameid != INVALIDFID) {
			wlc_bmac_write_shm(wlc_hw, M_BCMC_FID(wlc_hw), frameid);
		}

		if (wlc_bmac_dma_txfast(wlc_hw->wlc, fifo, p, commit) < 0) {
			PKTFREE(wlc_hw->osh, p, TRUE);
			WL_ERROR(("wlc_bmac_txfifo: fatal, toss frames !!!\n"));
			if (commit) {
				TXPKTPENDDEC(wlc_hw->wlc, fifo, txpktpend);
				wlc_low_txq_buffered_dec(wlc_hw->wlc->active_queue,
					fifo, txpktpend);
			}
		}
	} else {
		ASSERT(fifo < NFIFO_LEGACY);
		wlc_pio_tx(wlc_hw->pio[fifo], p);
	}
} /* wlc_bmac_txfifo */

#ifdef WL_RX_DMA_STALL_CHECK
/* define the rx_stall attach/detach if dma rx stall checkers are defined */

/** Allocate, init and return the wlc_rx_stall_info */
static wlc_rx_stall_info_t*
BCMATTACHFN(wlc_bmac_rx_stall_attach)(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh = wlc_hw->osh;
	wlc_rx_stall_info_t *rx_stall;
	wlc_rx_dma_stall_t *dma_stall = NULL;

	BCM_REFERENCE(dma_stall);

	/* alloc parent rx_stall structure */
	rx_stall = (wlc_rx_stall_info_t*)MALLOCZ(osh, sizeof(*rx_stall));
	if (rx_stall == NULL) {
		WL_ERROR(("%s: no mem for rx_stall, malloced %d bytes\n", __FUNCTION__,
			MALLOCED(osh)));
		return NULL;
	}

#if defined(WL_RX_DMA_STALL_CHECK)
	/* alloc dma stall sub-structure */
	dma_stall = (wlc_rx_dma_stall_t*)MALLOCZ(osh, sizeof(*dma_stall));
	if (dma_stall == NULL) {
		WL_ERROR(("%s: no mem for rx_dma_stall, malloced %d bytes\n", __FUNCTION__,
			MALLOCED(osh)));
		goto fail;
	}

	/* dma_stall initial values */

#if !defined(WL_DATAPATH_HC_NOASSERT)
	dma_stall->flags = WL_RX_DMA_STALL_F_ASSERT;
#endif // endif
	dma_stall->timeout = WLC_RX_DMA_STALL_TIMEOUT;

	rx_stall->dma_stall = dma_stall;
#endif /* WL_RX_DMA_STALL_CHECK */

#if defined(HEALTH_CHECK) && !defined(HEALTH_CHECK_DISABLED)
	if (WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
		/* register with health check module */
		if (!wl_health_check_module_register(wlc_hw->wlc->wl, "wl_dma_rx_stall_check",
				wlc_bmac_hchk_rx_dma_stall_check, wlc_hw, WL_HC_DD_RX_DMA_STALL)) {
			goto fail;
		}
	}
#endif // endif

	return rx_stall;

fail:
	if (dma_stall != NULL) {
		MFREE(osh, dma_stall, sizeof(*dma_stall));
	}
	if (rx_stall != NULL) {
		MFREE(osh, rx_stall, sizeof(*rx_stall));
	}

	return NULL;
}

/** Free up the wlc_rx_stall_info if present in wlc_info */
static void
BCMATTACHFN(wlc_bmac_rx_stall_detach)(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh = wlc_hw->osh;
	wlc_rx_stall_info_t *rx_stall;

	rx_stall = wlc_hw->rx_stall;

	if (rx_stall != NULL) {
		/*
		 * free rx_stall sub-structures
		 */

		if (rx_stall->dma_stall != NULL) {
			MFREE(osh, rx_stall->dma_stall, sizeof(*rx_stall->dma_stall));
		}

		MFREE(osh, rx_stall, sizeof(*rx_stall));
	}
}

#else /* WL_RX_DMA_STALL_CHECK */

/* Null definitions if rx_stall support not needed */
#define wlc_bmac_rx_stall_attach(wlc_hw)        (wlc_rx_stall_info_t*)0x0dadbeef
#define wlc_bmac_rx_stall_detach(wlc_hw)        do {} while (0)

#endif /* WL_RX_DMA_STALL_CHECK */

#if defined(WL_RX_DMA_STALL_CHECK)

/** Return the current rx_dma_stall timeout value (seconds) */
uint
wlc_bmac_rx_dma_stall_timeout_get(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->rx_stall->dma_stall->timeout;
}

/** Set the rx_dma_stall detection timeout in seconds.
 * This routine will set the new timeout and reset the state machine.
 */
int
wlc_bmac_rx_dma_stall_timeout_set(wlc_hw_info_t *wlc_hw, uint timeout)
{
	int i;
	wlc_rx_dma_stall_t *rx_dma_stall = wlc_hw->rx_stall->dma_stall;
	wlc_pub_t *pub = wlc_hw->wlc->pub;

	rx_dma_stall->timeout = timeout;

	/* reset the counters and state */

	wlc_ctrupd(wlc_hw->wlc, MCSTOFF_RXF0OVFL);
	rx_dma_stall->prev_ovfl[0] = MCSTVAR(pub, rxf0ovfl);

	if (D11REV_GE(pub->corerev, 40)) {
		/* rxflovl is only present in the rev40+ macstats */
		wlc_ctrupd(wlc_hw->wlc, MCSTOFF_RXF1OVFL);
		rx_dma_stall->prev_ovfl[1] = ((wl_cnt_ge40mcst_v1_t *)pub->_mcst_cnt)->rxf1ovfl;
	}

	for (i = 0; i < WLC_RX_DMA_STALL_HWFIFO_NUM; i++) {
		rx_dma_stall->count[i] = 0;
	}

	for (i = 0; i < WLC_RX_DMA_STALL_DMA_NUM; i++) {
		rx_dma_stall->prev_rxfill[i] = rx_dma_stall->rxfill[i];
	}

	return BCME_OK;
}

/** trigger a forced rx stall */
int
wlc_bmac_rx_dma_stall_force(wlc_hw_info_t *wlc_hw, uint32 testword)
{
	hnddma_t *di;
	uint fifo;
	bool stall;

	fifo = testword & WL_RX_DMA_STALL_TEST_FIFO_MASK;
	stall = (testword & WL_RX_DMA_STALL_TEST_ENABLE) != 0;

	if (fifo >= NFIFO_LEGACY) {
		return BCME_RANGE;
	}

	di = wlc_hw->di[fifo];
	if (di == NULL) {
		return BCME_RANGE;
	}

	dma_rxfill_suspend(di, stall);

	return BCME_OK;
}

#if defined(HEALTH_CHECK) && !defined(HEALTH_CHECK_DISABLED)
/* Check for a period of time that the Rx DMAs have not been serviced.
 * This check fails (returns BCME_ERROR) if there have rx fifo overflows,
 * but no buffers posted to the DMA.
 */
static int
wlc_bmac_rx_dma_stall_check(wlc_hw_info_t *wlc_hw, uint16 *stalled)
{
	int rxfifo_idx;
	int rxfifo_count;
	int dma_idx;
	int ret = BCME_OK;
	wlc_info_t *wlc = wlc_hw->wlc;
	wlc_pub_t *pub = wlc->pub;
	wlc_rx_dma_stall_t *rx_dma_stall = wlc_hw->rx_stall->dma_stall;

	/* only detecting for DMA configuration */
	if (PIO_ENAB_HW(wlc_hw)) {
		return BCME_OK;
	}

	/* zero means the check is disabled */
	if (rx_dma_stall->timeout == 0) {
		return BCME_OK;
	}

	/* the number of rx dma channels is only 1 before rev40.
	 * At rev40, a second hw rxfifo was created, and it is used in the
	 * PKT_CLASSIFY() configuration.
	 */
	if (D11REV_GE(pub->corerev, 40) && PKT_CLASSIFY()) {
		rxfifo_count = 2;
	} else {
		rxfifo_count = 1;
	}

	/* check each active rx dma */
	for (rxfifo_idx = 0; rxfifo_idx < rxfifo_count; rxfifo_idx++) {
		uint32 new_rxfill, delta_rxfill;
		uint32 new_overflow, delta_overflow;
		uint macstat_offset;

		/* Get the hw rx fifo overflow count.
		 * The rxf1ovfl count is only supported on rev40+, and 'rxfifo_idx' will
		 * only be non-zero for rev40+.
		 */
		if (rxfifo_idx == 0) {
			macstat_offset = MCSTOFF_RXF0OVFL;
			wlc_ctrupd(wlc, macstat_offset);
			new_overflow = MCSTVAR(pub, rxf0ovfl);
		} else {
			macstat_offset = MCSTOFF_RXF1OVFL;
			wlc_ctrupd(wlc, macstat_offset);
			/* rxflovl is only present in the rev40+ macstats */
			new_overflow = ((wl_cnt_ge40mcst_v1_t *)pub->_mcst_cnt)->rxf1ovfl;
		}

		/* determine what DMA serviced the current rxfifo */
		if (rxfifo_idx == 0) {
			/* Get the index for the DMA servicing rxfifo0.
			 * In the normal configuration, DMA 0 serves rxfifo0.
			 * With RXFIFO_SPLIT() configuration, both DMA 0/1 service rxfifo0,
			 * but only DMA 1 is used for the rxfill operation.
			 */
			if (RXFIFO_SPLIT()) {
				dma_idx = 1;
			} else {
				dma_idx = 0;
			}
		} else {
			/* Get the index for the DMA servicing rxfifo1.
			 * This will be the PKT_CLASSIFY_FIFO value.
			 */
			dma_idx = PKT_CLASSIFY_FIFO;
		}

		/* get the current rxfill counts matching the DMA channel */
		new_rxfill = rx_dma_stall->rxfill[dma_idx];

		/* Rather than try to integrate this code with the counter reset operation,
		 * just detect a counter reset and skip the stall check.
		 * A normal counter wrap around will trigger this 'reset' check, but if we
		 * are actually stalled, the next call will pick up the stall condition.
		 */
		if (new_overflow < rx_dma_stall->prev_ovfl[rxfifo_idx]) {
			/* update the counters */
			rx_dma_stall->prev_ovfl[rxfifo_idx] = new_overflow;
			rx_dma_stall->rxfill[dma_idx] = new_rxfill;
			continue;
		}

		/* compare to the previous */
		delta_overflow = new_overflow - rx_dma_stall->prev_ovfl[rxfifo_idx];
		delta_rxfill = new_rxfill - rx_dma_stall->prev_rxfill[dma_idx];

		WL_NONE(("DBG: RX %d new_ovfl %u prev_ovfl %u "
		         "DMA %d new_rxfill %u prev_rxfill %u\n",
		         rxfifo_idx, new_overflow, rx_dma_stall->prev_ovfl[rxfifo_idx],
		         dma_idx, new_rxfill, rx_dma_stall->prev_rxfill[dma_idx]));

		/* if there have been overflows and no attempts to fill, declare an RX stall */
		if (delta_overflow > 0 && delta_rxfill == 0) {
			/* if there have been more seconds without progress than the limit,
			 * declare an RX stall
			 */
			if (rx_dma_stall->count[rxfifo_idx] == rx_dma_stall->timeout) {

				/* make note of the stalled dma */
				if (stalled != NULL) {
					*stalled |= (1 << dma_idx);
				}

				/* Declare a stall for this fifo */
				wlc_bmac_rx_dma_stall_report(wlc_hw, rxfifo_idx, dma_idx,
				                             delta_overflow);

				/* clean up the overflow count */
				rx_dma_stall->count[rxfifo_idx] = 0;

				/* clean up any dbg forced stall */
				wlc_bmac_rx_dma_stall_force(wlc_hw, dma_idx);

				ret = BCME_ERROR;
			} else {
				/* not a stall yet, keep counting ... */
				rx_dma_stall->count[rxfifo_idx]++;
				WL_NONE(("DBG: stall_cnt %u\n", rx_dma_stall->count[rxfifo_idx]));
			}
		} else {
			/* progress was made, so reset the stall counter and update the stats */
			rx_dma_stall->count[rxfifo_idx] = 0;

			/* update the counts for next stall */
			rx_dma_stall->prev_ovfl[rxfifo_idx] = new_overflow;
			rx_dma_stall->prev_rxfill[dma_idx] = new_rxfill;
		}
	}

	return ret;
}

static void
wlc_bmac_rx_dma_stall_report(wlc_hw_info_t *wlc_hw, int rxfifo_idx, int dma_idx, uint32 overflows)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	wlc_rx_dma_stall_t *rx_dma_stall = wlc_hw->rx_stall->dma_stall;

	/* Declare a stall for this fifo */

	WL_ERROR(("wl%d: wlc_bmac_rx_dma_stall_check(): over %d seconds "
	          "with %d rx fifo %d overflows and no DMA %d rxfills\n",
	          wlc_hw->unit, rx_dma_stall->timeout, overflows, rxfifo_idx, dma_idx));

#if defined(WL_DATAPATH_LOG_DUMP)
	/* dump some datapath info */
	wlc_datapath_log_dump(wlc, EVENT_LOG_TAG_WL_ERROR);
#endif // endif

	if ((rx_dma_stall->flags & WL_RX_DMA_STALL_F_ASSERT) != 0) {
		/* XXX In addition to an error msg, need to integrate with health check.
		 * For now, directly trigger a fatal error.
		 */
		wlc_hw->need_reinit = WL_REINIT_RC_RX_DMA_STALL;
		WLC_FATAL_ERROR(wlc);
	}
}
#endif /* (HEALTH_CHECK) && !(HEALTH_CHECK_DISABLED) */

#if defined(HEALTH_CHECK) && !defined(HEALTH_CHECK_DISABLED)
static int
wlc_bmac_hchk_rx_dma_stall_check(uint8 *buffer, uint16 length, void *context,
	int16 *bytes_written)
{
	int rc;
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t*)context;
	wlc_rx_dma_stall_t *rx_dma_stall = wlc_hw->rx_stall->dma_stall;
	uint16 stalled = 0;
	wl_rx_dma_hc_info_t hc_info;

	rc = wlc_bmac_rx_dma_stall_check(wlc_hw, &stalled);

	if (rc == BCME_OK) {
		*bytes_written = 0;
		return HEALTH_CHECK_STATUS_OK;
	}

	/* If an error is detected and space is available, client must write
	 * a XTLV record to indicate what happened.
	 * The buffer provided is word aligned.
	 */
	if (length >= sizeof(hc_info)) {
		hc_info.type = WL_HC_DD_RX_DMA_STALL;
		hc_info.length = sizeof(hc_info) - BCM_XTLV_HDR_SIZE;
		hc_info.timeout = rx_dma_stall->timeout;
		hc_info.stalled_dma_bitmap = stalled;

		bcopy(&hc_info, buffer, sizeof(hc_info));
		*bytes_written = sizeof(hc_info);
	} else {
		/* hc buffer too short */
		*bytes_written = 0;
	}

	/* overwrite the rc to return a proper status back to framework */
	rc = HEALTH_CHECK_STATUS_INFO_LOG_BUF;
#if defined(WL_DATAPATH_HC_NOASSERT)
	rc |= HEALTH_CHECK_STATUS_ERROR;
#else
	wlc_hw->need_reinit = WL_REINIT_RC_RX_DMA_STALL;
	rc |= HEALTH_CHECK_STATUS_TRAP;
#endif // endif

	return rc;
}
#endif /* HEALTH_CHECK */

#else /* !WL_RX_DMA_STALL_CHECK */

/* Null definition if WL_RX_DMA_STALL_CHECK is not defined */
#define wlc_bmac_rx_stall_attach(wlc_hw)        (wlc_rx_stall_info_t*)0x0dadbeef
#define wlc_bmac_rx_stall_detach(wlc_hw)        do {} while (0)
#define wlc_bmac_rx_dma_stall_check(wlc_hw, stalled)	(BCME_OK)

#endif /* WL_RX_DMA_STALL_CHECK */

/** Periodic tasks are carried out by a watchdog timer that is called once every second */
void
wlc_bmac_watchdog(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint i = 0;
	WL_TRACE(("wl%d: wlc_bmac_watchdog\n", wlc_hw->unit));

	if (!wlc_hw->up)
		return;

	ASSERT(!wlc_hw->sbclk || !si_taclear(wlc_hw->sih, TRUE));

	/* increment second count */
	wlc_hw->now++;

	/* Check for FIFO error interrupts */
	wlc_bmac_fifoerrors(wlc_hw);

	/* Check whether PSMx watchdog triggered */
	if (PSMX_ENAB(wlc->pub)) {
		wlc_bmac_psmx_errors(wlc);
	}

	/* make sure RX dma has buffers */
	if (!PIO_ENAB_HW(wlc_hw)) {
		/* DMA RXFILL */
		for (i = 0; i < MAX_RX_FIFO; i++) {
			if ((wlc_hw->di[i] != NULL) && wlc_bmac_rxfifo_enab(wlc_hw, i)) {
				wlc_bmac_dma_rxfill(wlc_hw, i);
			}
		}
	}
#if defined(HEALTH_CHECK) && !defined(HEALTH_CHECK_DISABLED)
	if (!WL_HEALTH_CHECK_ENAB(wlc->pub->cmn)) {
	/* Moved check for rx stall (requires WLCNT) to health check infrastructure */
		(void) wlc_bmac_rx_dma_stall_check(wlc_hw, NULL);
	}
#endif /* HEALTH_CHECK */
#ifdef LPAS
	/* In LPAS mode phy_wdog is invoked from tbtt only. It can be upto
	 * 2 secs between two phy_wdog invokations.
	 */
	if (!wlc->lpas)
#endif /* LPAS */
	{
		phy_watchdog((phy_info_t *)wlc_hw->band->pi);
	}
	ASSERT(!wlc_hw->sbclk || !si_taclear(wlc_hw->sih, TRUE));
} /* wlc_bmac_watchdog */

#ifdef SRHWVSDB

/** Higher MAC requests to activate SRVSDB operation for the currently selected band */
int wlc_bmac_activate_srvsdb(wlc_hw_info_t *wlc_hw, chanspec_t chan0, chanspec_t chan1)
{
	int err = BCME_ERROR;

	BCM_REFERENCE(chan0);
	BCM_REFERENCE(chan1);

	wlc_hw->sr_vsdb_active = FALSE;

	if (SRHWVSDB_ENAB(wlc_hw->wlc->pub) &&
	    phy_chanmgr_vsdb_sr_attach_module(wlc_hw->band->pi, chan0, chan1)) {
		wlc_hw->sr_vsdb_active = TRUE;
		err = BCME_OK;
	}

	return err;
}

/** Higher MAC requests to deactivate SRVSDB operation for the currently selected band */
void wlc_bmac_deactivate_srvsdb(wlc_hw_info_t *wlc_hw)
{
	wlc_hw->sr_vsdb_active = FALSE;

	if (SRHWVSDB_ENAB(wlc_hw->wlc->pub)) {
		phy_chanmgr_vsdb_sr_detach_module(wlc_hw->band->pi);
	}

}

/** All SW inits needed for band change */
static void
wlc_bmac_srvsdb_set_band(wlc_hw_info_t *wlc_hw, uint bandunit, chanspec_t chanspec)
{
	uint32 macintmask;
	uint32 mc;
	wlc_info_t *wlc = wlc_hw->wlc;

	BCM_REFERENCE(chanspec);

	ASSERT(NBANDS_HW(wlc_hw) > 1);
	ASSERT(bandunit != wlc_hw->band->bandunit);
	ASSERT(si_iscoreup(wlc_hw->sih));
	ASSERT((R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC) == 0);

	/* disable interrupts */
	macintmask = wl_intrsoff(wlc->wl);

	wlc_setxband(wlc_hw, bandunit);

	/* bsinit */
	wlc_ucode_bsinit(wlc_hw);

	mc = R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw));
	if ((mc & MCTL_EN_MAC) != 0) {
		if (mc == 0xffffffff)
			WL_ERROR(("wl%d: wlc_phy_init: chip is dead !!!\n", wlc_hw->unit));
		else
			WL_ERROR(("wl%d: wlc_phy_init:MAC running! mc=0x%x\n",
				wlc_hw->unit, mc));

		ASSERT((const char*)"wlc_phy_init: Called with the MAC running!" == NULL);
	}

	/* check D11 is running on Fast Clock */
	ASSERT(si_core_sflags(wlc_hw->sih, 0, 0) & SISF_FCLKA);

	/* cwmin is band-specific, update hardware with value for current band */
	wlc_bmac_set_cwmin(wlc_hw, wlc_hw->band->CWmin);
	wlc_bmac_set_cwmax(wlc_hw, wlc_hw->band->CWmax);

	wlc_bmac_update_slot_timing(wlc_hw,
	        BAND_5G(wlc_hw->band->bandtype) ? TRUE : wlc_hw->shortslot);

#ifdef WL11N
	/* initialize the txphyctl1 rate table since shmem is shared between bands */
	wlc_upd_ofdm_pctl1_table(wlc_hw);
#endif // endif

	/* Configure BTC GPIOs as bands change */
	if (BAND_5G(wlc_hw->band->bandtype))
		wlc_bmac_mhf(wlc_hw, MHF5, MHF5_BTCX_DEFANT,
			MHF5_BTCX_DEFANT, WLC_BAND_ALL);
	else
		wlc_bmac_mhf(wlc_hw, MHF5, MHF5_BTCX_DEFANT, 0, WLC_BAND_ALL);

	/*
	 * If there are any pending software interrupt bits,
	 * then replace these with a harmless nonzero value
	 * so wlc_dpc() will re-enable interrupts when done.
	 */
	if (wlc_hw->macintstatus)
		wlc_hw->macintstatus = MI_DMAINT;

	/* restore macintmask */
	wl_intrsrestore(wlc->wl, macintmask);
} /* wlc_bmac_srvsdb_set_band */

/** optionally switches band */
static uint8
wlc_bmac_srvsdb_set_chanspec(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute,
	bool fastclk, uint bandunit, bool band_change)
{
	uint8 switched;
	uint8 last_chan_saved = FALSE;

	/* if excursion is active (scan), switch to normal switch */
	/* mode, else allow sr switching when scan periodically */
	/* returns to home channel */
	if (wlc_hw->wlc->excursion_active)
		return FALSE;

	/* calls the PHY to switch */
	switched = phy_chanmgr_vsdb_sr_set_chanspec(wlc_hw->band->pi, chanspec, &last_chan_saved);

	/*
	 * note: for bmac firmware this is an asynchronous call. Given caller supplied flags,
	 * optionally saves / restores PPR (power per rate, tx power control related) context.
	 */
	wlc_srvsdb_switch_ppr(wlc_hw->wlc, chanspec, last_chan_saved, switched);

	/* If phy context changed from SRVSDB, continue, otherwise return */
	if (!switched)
		return FALSE;

	/* SW init required after SRVSDB switch */
	if (band_change) {
		wlc_bmac_srvsdb_set_band(wlc_hw, bandunit, chanspec);

		if (CHSPEC_IS2G(chanspec)) {
			wlc_hw->band->bandtype = 2;
			wlc_hw->band->bandunit = 0;
		} else {
			wlc_hw->band->bandunit = 1;
			wlc_hw->band->bandtype = 1;
		}
	}

	/* come out of mute */
	wlc_bmac_mute(wlc_hw, mute, 0);

	if (!fastclk)
		wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);

	/* Successfull SRVSDB switch */
	return TRUE;
} /* wlc_bmac_srvsdb_set_chanspec */

#endif /* SRHWVSDB */

/**
 * higher MAC requested a channel change, optionally to a different band
 * Parameters:
 *    mute : 'TRUE' for a 'quiet' channel, a channel on which no transmission is (yet) permitted. In
 *           that case, all txfifo's are suspended.
 */
void
wlc_bmac_set_chanspec(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute, ppr_t *txpwr,
	wl_txpwrcap_tbl_t *txpwrcap_tbl, int* cellstatus)
{
	bool fastclk;
	uint bandunit = 0;
#ifdef SRHWVSDB
	uint8 vsdb_switch = 0;
	uint8 vsdb_status = 0;

	if (SRHWVSDB_ENAB(wlc_hw->pub)) {
		vsdb_switch =  wlc_hw->sr_vsdb_force;
#if defined(WLMCHAN) && defined(SR_ESSENTIALS)
		vsdb_switch |= (wlc_hw->sr_vsdb_active &&
		                sr_engine_enable(wlc_hw->sih, wlc_hw->macunit, IOV_GET, FALSE) > 0);
#endif /* WLMCHAN SR_ESSENTIALS */
	}
#endif /* SRHWVSDB */

	WL_TRACE(("wl%d: wlc_bmac_set_chanspec 0x%x\n", wlc_hw->unit, chanspec));

	/* request FAST clock if not on */
	if (!(fastclk = wlc_hw->forcefastclk))
		wlc_clkctl_clk(wlc_hw, CLK_FAST);

	wlc_hw->chanspec = chanspec;

	WL_TSLOG(wlc_hw->wlc, __FUNCTION__, TS_ENTER, 0);
	/* Switch bands if necessary */
	if (NBANDS_HW(wlc_hw) > 1) {
		bandunit = CHSPEC_WLCBANDUNIT(chanspec);
		if (wlc_hw->band->bandunit != bandunit) {
#ifdef SRHWVSDB
			/* wlc_bmac_setband disables other bandunit,
			 *  use light band switch if not up yet
			 */
			if (wlc_hw->up) {
				if (SRHWVSDB_ENAB(wlc_hw->pub) && vsdb_switch) {
					vsdb_status = wlc_bmac_srvsdb_set_chanspec(wlc_hw,
						chanspec, mute, fastclk, bandunit, TRUE);
					if (vsdb_status) {
						WL_INFORM(("SRVSDB Switch DONE successfully \n"));
						WL_TSLOG(wlc_hw->wlc, __FUNCTION__, TS_EXIT, 0);
						return;
					}
				}
			}
#endif /* SRHWVSDB */
			if (wlc_hw->up) {
				wlc_phy_chanspec_radio_set(wlc_hw->bandstate[bandunit]->pi,
					chanspec);
				wlc_bmac_setband(wlc_hw, bandunit, chanspec);
			} else {
				wlc_setxband(wlc_hw, bandunit);
			}
		}
	}

#ifdef WLC_TXPWRCAP
	if (WLTXPWRCAP_ENAB(wlc_hw->wlc)) {
		if (txpwrcap_tbl) {
			if (wlc_phy_txpwrcap_tbl_set(wlc_hw->band->pi,
					(wl_txpwrcap_tbl_t*)txpwrcap_tbl) != BCME_OK) {
				WL_ERROR(("Txpwrcap table set failed \n"));
				ASSERT(0);
			}
		}

		if (cellstatus) {
			wlc_phyhal_txpwrcap_set_cellstatus(wlc_hw->band->pi, *cellstatus);
		}
	}
#endif // endif

	phy_calmgr_enable_initcal(wlc_hw->band->pi, !mute);

	if (!wlc_hw->up) {
		if (wlc_hw->clk)
			wlc_phy_txpower_limit_set(wlc_hw->band->pi, txpwr, chanspec);
		wlc_phy_chanspec_radio_set(wlc_hw->band->pi, chanspec);
	} else {
		/* Update muting of the channel.
		 * wlc_phy_chanspec_set may start a wlc_phy_cal_perical, to prevent emitting energy
		 * on a muted channel, muting of the channel is updated before hand.
		 */
		wlc_bmac_mute(wlc_hw, mute, 0);

		if ((wlc_hw->deviceid == BCM4360_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM43452_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM43602_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM4352_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM53573_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM47189_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM4359_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM43596_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM43597_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM4365_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM4366_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM43684_D11AX_ID) ||
		    (wlc_hw->deviceid == EMBEDDED_2x2AX_ID) ||
		    (wlc_hw->deviceid == BCM6710_D11AX_ID) ||
		    (wlc_hw->deviceid == BCM4347_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM4361_D11AC_ID) ||
		    (wlc_hw->deviceid == BCM6878_D11AC_ID)) {
			/* phymode switch requires phyinit */
			if (phy_init_pending((phy_info_t *)wlc_hw->band->pi)) {
				phy_init((phy_info_t *)wlc_hw->band->pi, chanspec);
			} else {
				wlc_phy_chanspec_set(wlc_hw->band->pi, chanspec);
#ifdef PHYCAL_CACHING
				/* Set operating channel so PHY can perform related periodic cal */
				if (wlc_set_operchan(wlc_hw->wlc, chanspec) != BCME_OK) {
					WL_INFORM(("BMAC1: Set oper channel failed\n"));
				}
#endif /* PHYCAL_CACHING */
			}
		} else {
			/* Bandswitch above may end up changing the channel so avoid repetition */
			if (chanspec != phy_utils_get_chanspec((phy_info_t *)wlc_hw->band->pi)) {
#ifdef SRHWVSDB
				if (SRHWVSDB_ENAB(wlc_hw->pub) && vsdb_switch) {
					vsdb_status = wlc_bmac_srvsdb_set_chanspec(wlc_hw,
						chanspec, mute, fastclk, bandunit, FALSE);
					if (vsdb_status) {
						WL_INFORM(("SRVSDB Switch DONE successfully \n"));
						WL_TSLOG(wlc_hw->wlc, __FUNCTION__, TS_EXIT, 0);
						return;
					}
				}
#endif // endif
				wlc_phy_chanspec_set(wlc_hw->band->pi, chanspec);
#ifdef PHYCAL_CACHING
				/* Set operating channel so PHY can perform related periodic cal */
				if (wlc_set_operchan(wlc_hw->wlc, chanspec) != BCME_OK) {
					WL_INFORM(("BMAC2: Set oper channel failed\n"));
				}
#endif /* PHYCAL_CACHING */
			}
		}
		wlc_phy_txpower_limit_set(wlc_hw->band->pi, txpwr, chanspec);
	}
#ifdef BCMLTECOEX
	if (BCMLTECOEX_ENAB(wlc_hw->wlc->pub)) {
		wlc_ltecx_update_scanreq_bm_channel(wlc_hw->wlc->ltecx, chanspec);
	}
#endif // endif
	if (!fastclk)
		wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);

	WL_TSLOG(wlc_hw->wlc, __FUNCTION__, TS_EXIT, 0);
} /* wlc_bmac_set_chanspec */

/**
 * Higher MAC (e.g. as contained in the wl host driver) has to query hardware+firmware
 * attributes to use the same settings as bmac, and has to query capabilities before enabling them.
 */
int
wlc_bmac_revinfo_get(wlc_hw_info_t *wlc_hw, wlc_bmac_revinfo_t *revinfo)
{
	si_t *sih = wlc_hw->sih;
	uint idx;

	revinfo->vendorid = wlc_hw->vendorid;
	revinfo->deviceid = wlc_hw->deviceid;

	revinfo->boardrev = wlc_hw->boardrev;
	revinfo->corerev = wlc_hw->corerev;
	revinfo->corerev_minor = wlc_hw->corerev_minor;
	revinfo->sromrev = wlc_hw->sromrev;
	/* srom9 introduced ppr, which requires corerev >= 24 */
	if (wlc_hw->sromrev >= 9) {
		WL_ERROR(("wlc_bmac_attach: srom9 ppr requires corerev >=24"));
		ASSERT(D11REV_GE(wlc_hw->corerev, 30));
	}
	revinfo->chiprev = sih->chiprev;
	revinfo->chip = SI_CHIPID(sih);
	revinfo->chippkg = sih->chippkg;
	revinfo->boardtype = sih->boardtype;
	revinfo->boardvendor = sih->boardvendor;
	revinfo->bustype = sih->bustype;
	revinfo->buscoretype = sih->buscoretype;
	revinfo->buscorerev = sih->buscorerev;
	revinfo->issim = sih->issim;
	revinfo->boardflags = wlc_hw->boardflags;
	revinfo->boardflags2 = wlc_hw->boardflags2;
	if (wlc_hw->sromrev >= 12)
		revinfo->boardflags4 = wlc_hw->boardflags4;

	revinfo->nbands = NBANDS_HW(wlc_hw);

	for (idx = 0; idx < NBANDS_HW(wlc_hw); idx++) {
		wlc_hwband_t *band;

		/* So if this is a single band 11a card, use band 1 */
		if (IS_SINGLEBAND_5G(wlc_hw->deviceid, wlc_hw->phy_cap))
			idx = BAND_5G_INDEX;

		band = wlc_hw->bandstate[idx];
		revinfo->band[idx].bandunit = band->bandunit;
		revinfo->band[idx].bandtype = band->bandtype;
		revinfo->band[idx].phytype = band->phytype;
		revinfo->band[idx].phyrev = band->phyrev;
		revinfo->band[idx].phy_minor_rev = band->phy_minor_rev;
		revinfo->band[idx].radioid = band->radioid;
		revinfo->band[idx].radiorev = band->radiorev;
		revinfo->band[idx].anarev = 0;

	}

	revinfo->_wlsrvsdb = wlc_hw->wlc->pub->_wlsrvsdb;

	return BCME_OK;
} /* wlc_bmac_revinfo_get */

int
wlc_bmac_state_get(wlc_hw_info_t *wlc_hw, wlc_bmac_state_t *state)
{
	state->machwcap = wlc_hw->machwcap;
	state->machwcap1 = wlc_hw->machwcap1;
	state->preamble_ovr = (uint32)phy_preamble_override_get(wlc_hw->band->pi);

	return 0;
}

void * wlc_bmac_dmatx_peeknexttxp(wlc_info_t *wlc, int fifo)
{
#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
	return hwa_txfifo_peeknexttxp(hwa_dev, WLC_HW_MAP_TXFIFO(wlc, fifo));
#else
	return dma_peeknexttxp(WLC_HW_DI(wlc, fifo));
#endif // endif
}

void
wlc_dma_map_pkts(wlc_hw_info_t *wlc_hw, map_pkts_cb_fn cb, void *ctx)
{
	uint i;
	hnddma_t *di;

	for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc_hw->wlc); i++) {
		di = wlc_hw->di[i];
		if (di) {
#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
			hwa_txfifo_map_pkts(hwa_dev,
				WLC_HW_MAP_TXFIFO(wlc_hw->wlc, i), cb, ctx);
#else
			dmatx_map_pkts(di, cb, ctx);
#endif // endif
		}
	}
}

#ifdef BCMDBG_POOL
/**
 * Packet pool separates memory allocation for packets from other allocations, making the system
 * more robust to low memory conditions, and prevents DMA error conditions by reusing recently freed
 * packets in a faster manner.
 */
static void
wlc_pktpool_dbg_cb(pktpool_t *pool, void *arg)
{
	wlc_hw_info_t *wlc_hw = arg;
	pktpool_stats_t pstats;

	if (wlc_hw == NULL)
		return;

	if (wlc_hw->up == FALSE)
		return;

	WL_ERROR(("wl: post=%d rxactive=%d txactive=%d txpend=%d\n",
		NRXBUFPOST,
		dma_rxactive(wlc_hw->di[RX_FIFO]),
		dma_txactive(wlc_hw->di[1]),
		dma_txpending(wlc_hw->di[1])));

	pktpool_stats_dump(pool, &pstats);
	WL_ERROR(("pool len=%d\n", pktpool_tot_pkts(pool)));
	WL_ERROR(("txdh:%d txd11:%d enq:%d rxdh:%d rxd11:%d rxfill:%d idle:%d\n",
		pstats.txdh, pstats.txd11, pstats.enq,
		pstats.rxdh, pstats.rxd11, pstats.rxfill, pstats.idle));
}
#endif /* BCMDBG_POOL */

#ifdef BCMPKTPOOL
static void
wlc_pktpool_avail_cb(pktpool_t *pool, void *arg)
{
	wlc_hw_info_t *wlc_hw = arg;
	wlc_info_t *wlc;
	wlc_info_t *current_wlc;
	int idx;
	int i = 0;

	BCM_REFERENCE(pool);
	BCM_REFERENCE(arg);

	if (wlc_hw == NULL) {
		return;
	}

	wlc = wlc_hw->wlc;

	FOREACH_WLC(wlc->cmn, idx, current_wlc) {
		wlc_hw = current_wlc->hw;

		if (wlc_hw->up == FALSE || wlc_hw->reinit) {
			continue;
		}

		/* The callback can come from bus context or timer context.
		* Ensure the srpwr request is ON before any reg access.
		*/
		if (SRPWR_ENAB()) {
			wlc_srpwr_request_on(wlc_hw->wlc);
		}

#ifdef	WL_RXEARLYRC
		if (WL_RXEARLYRC_ENAB(wlc_hw->wlc->pub)) {
			if ((wlc_hw->rc_pkt_head == NULL)) {
				if ((dma_activerxbuf(wlc_hw->di[RX_FIFO]) < 4)) {
					void *prev = NULL;
					void *p;
					while ((p = dma_rx(wlc_hw->di[RX_FIFO])) != NULL) {
						if (wlc_hw->rc_pkt_head == NULL) {
							wlc_hw->rc_pkt_head = p;
						} else {
							PKTSETLINK(prev, p);
						}
						prev = p;
					}
				}
			}
		}
#endif /* WL_RXEARLYRC */
		if (!PIO_ENAB_HW(wlc_hw)) {
			if (BCMSPLITRX_ENAB()) {
				for (i = 0; i < MAX_RX_FIFO; i++) {
					if (((wlc_hw->di[i] != NULL) &&
						wlc_bmac_rxfifo_enab(wlc_hw, i))) {
#ifdef STS_FIFO_RXEN
						if (i == STS_FIFO) {
							continue;
						}
#endif // endif
						if (pool == dma_pktpool_get(wlc_hw->di[i]))
							wlc_bmac_dma_rxfill(wlc_hw, i);
					}
				}
			} else if (wlc_hw->di[RX_FIFO]) {
				wlc_bmac_dma_rxfill(wlc_hw, RX_FIFO);
			}
		}
	} /* FOREACH */
}
#endif /* BCMPKTPOOL */

#ifdef BCMFRWDPOOLREORG
static void
wlc_sharedpktpool_avail_cb(pktpool_t *pool, void *arg)
{
	wlc_hw_info_t *wlc_hw = arg;
	wlc_info_t *wlc, *current_wlc;
	int idx;

	if (!wlc_hw) {
		return;
	}

	wlc = wlc_hw->wlc;
	FOREACH_WLC(wlc->cmn, idx, current_wlc) {
		wlc_hw = current_wlc->hw;

		if (wlc_hw->up == FALSE || wlc_hw->reinit) {
			continue;
		}

		if (!PIO_ENAB_HW(wlc_hw) && wlc_hw->di[PKT_CLASSIFY_FIFO]) {
			dma_rxfill(wlc_hw->di[PKT_CLASSIFY_FIFO]);
		}
	}
}

void
wlc_register_dma_rxfill_cb(wlc_info_t *wlc, bool reg)
{
	uint8 idx;
	wlc_info_t *wlci;
	wlc_hw_info_t *wlc_hw;

	if (BCMSPLITRX_ENAB()) {
		FOREACH_WLC(wlc->cmn, idx, wlci) {
			wlc_hw = wlci->hw;
			if (POOL_ENAB(wlci->pub->pktpool_rxlfrag)) {
				if (wlc_hw->di[RX_FIFO]) {
					if (reg == TRUE) {
						pktpool_avail_register(wlci->pub->pktpool_rxlfrag,
								wlc_pktpool_avail_cb, wlc_hw);
					} else {
						pktpool_avail_deregister(wlci->pub->pktpool_rxlfrag,
								wlc_pktpool_avail_cb, wlc_hw);
					}
				}
			}

			if (POOL_ENAB(wlci->pub->pktpool)) {
				if (wlc_hw->di[PKT_CLASSIFY_FIFO]) {
					if (reg == TRUE) {
						pktpool_avail_register(wlci->pub->pktpool,
								wlc_sharedpktpool_avail_cb, wlc_hw);
					} else {
						pktpool_avail_deregister(wlci->pub->pktpool,
								wlc_sharedpktpool_avail_cb, wlc_hw);
					}
				}
			}
		}
	}
}
#endif /* BCMFRWDPOOLREORG */

#ifdef WLRXOV

/** Throttle tx on rx fifo overflow: flow control related */
void
wlc_rxov_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;

	ASSERT(wlc->rxov_active == TRUE);
	if (wlc->rxov_delay > RXOV_TIMEOUT_MIN) {
		/* Gradually back off rxfifo overflow */
		wlc->rxov_delay -= RXOV_TIMEOUT_BACKOFF;

		wl_add_timer(wlc->wl, wlc->rxov_timer, wlc->rxov_delay, FALSE);
		wlc->rxov_active = TRUE;
	} else {
		/* Restore tx params */
		if (N_ENAB(wlc->pub) && AMPDU_MAC_ENAB(wlc->pub))
			wlc_set_txmaxpkts(wlc, MAXTXPKTS_AMPDUMAC);

		wlc->rxov_delay = RXOV_TIMEOUT_MIN;
		wlc->rxov_active = FALSE;

#ifdef BCMPKTPOOL
		if (POOL_ENAB(wlc->pub->pktpool))
			pktpool_avail_notify_normal(wlc->osh, SHARED_POOL);
#endif // endif
	}
}

void
wlc_rxov_int(wlc_info_t *wlc)
{
	int ret;
	if (wlc->rxov_active == FALSE) {
		if (POOL_ENAB(wlc->pub->pktpool)) {
			ret = pktpool_avail_notify_exclusive(wlc->osh, SHARED_POOL,
				wlc_pktpool_avail_cb);
			if (ret != BCME_OK) {
				WL_ERROR(("wl%d: %s: notify excl fail: %d\n",
					WLCWLUNIT(wlc), __FUNCTION__, ret));
			}
		}
		/*
		 * Throttle tx when hitting rxfifo overflow
		 * Increase rx post??
		 */
		if (N_ENAB(wlc->pub) && AMPDU_MAC_ENAB(wlc->pub))
			wlc_set_txmaxpkts(wlc, wlc->rxov_txmaxpkts);

		wl_add_timer(wlc->wl, wlc->rxov_timer, wlc->rxov_delay, FALSE);
		wlc->rxov_active = TRUE;
	} else {
		/* Re-arm it */
		wlc->rxov_delay = MIN(wlc->rxov_delay*2, RXOV_TIMEOUT_MAX);
		wl_add_timer(wlc->wl, wlc->rxov_timer, wlc->rxov_delay, FALSE);
	}

	if (!PIO_ENAB_HW(wlc->hw) && wlc->hw->di[RX_FIFO]) {
		wlc_bmac_dma_rxfill(wlc->hw, RX_FIFO);
	}
}

#endif /* WLRXOV */

static void
BCMUCODEFN(wlc_bmac_set_dma_burstlen_pcie)(wlc_hw_info_t *wlc_hw, bmac_dmactl_t *dmactl)
{
	uint32 devctl;
	uint32 mrrs, mps, rxblen;
	si_t *sih = wlc_hw->sih;
#ifdef WLTEST
	uint32 burstlen;
#endif // endif

	pcie_getdevctl(wlc_hw->osh, sih, &devctl);
	mrrs = (devctl & PCIE_CAP_DEVCTRL_MRRS_MASK) >> PCIE_CAP_DEVCTRL_MRRS_SHIFT;
	switch (mrrs)
	{
		case PCIE_CAP_DEVCTRL_MRRS_128B:
			dmactl->txblen = DMA_BL_128;
			break;
		case PCIE_CAP_DEVCTRL_MRRS_256B:
			dmactl->txblen = DMA_BL_256;
			break;
		case PCIE_CAP_DEVCTRL_MRRS_512B:
			dmactl->txblen = DMA_BL_512;
			break;
		case PCIE_CAP_DEVCTRL_MRRS_1024B:
			dmactl->txblen = DMA_BL_1024;
			break;
		default:
			ASSERT(0);
			break;
	}

	WL_INFORM(("MRRS Read from config reg %x \n", mrrs));
#ifdef WLTEST
	if (getvar(NULL, tx_burstlen_d11dma) != NULL) {
		burstlen = getintvar(NULL, tx_burstlen_d11dma);
		WL_ERROR(("MRRS Read from config reg %x \n", mrrs));
		WL_ERROR(("Overriding txburst length to: %d to induce MRRS error\n", burstlen));
		dmactl->txblen = burstlen;
	}
#endif // endif

	if ((BUSCORETYPE(wlc_hw->sih->buscoretype) == PCIE2_CORE_ID) &&
		(wlc_hw->sih->buscorerev ==  5)) {
		dmactl->txblen = DMA_BL_128;
		return;
	}

	mps = (devctl & PCIE_CAP_DEVCTRL_MPS_MASK) >> PCIE_CAP_DEVCTRL_MPS_SHIFT;
	switch (mps)
	{
		case PCIE_CAP_DEVCTRL_MPS_128B:
			rxblen = DMA_BL_128;
			break;
		case PCIE_CAP_DEVCTRL_MPS_256B:
			rxblen = DMA_BL_256;
			break;
		case PCIE_CAP_DEVCTRL_MPS_512B:
			rxblen = DMA_BL_512;
			break;
		case PCIE_CAP_DEVCTRL_MPS_1024B:
			rxblen = DMA_BL_1024;
			break;
		default:
			rxblen = DMA_BL_128;
			break;
	}
	WL_INFORM(("MPS Read from config reg %x \n", mps));

	/* XXX: set rxburstlen to minimum of MPS configured during
	 * enumeration and the burst length supported by chip.
	 */
	dmactl->rxblen = MIN(dmactl->rxblen, rxblen);
} /* wlc_bmac_set_dma_burstlen_pcie */

#define	PCIECFGREG_LNKSTA		0x80000000 /* Link Status */
#define PCIECFGREG_NEG_LNKSTA_SPEED	0xF0000	 /* Negotiated Link Speed */
#define PCIECFGREG_NEG_LNKSTA_WIDTH	0x3F00000 /* Negotiated Link Width */
#define PCIECFGREG_NEG_LNKSTA_WIDTH_X2	2 /* Negotiated Link Width */
#define PCIECFGREG_NEG_LNKSTA_SPEED_SHIFT	16
#define PCIECFGREG_NEG_LNKSTA_WIDTH_SHIFT	20

#define PCIECFGREG_LNKCAP_MAXSPEED	0xF	 /* Max Link Speed */
#define PCIECFGREG_LNKCAP_MAXWIDTH	0x3F0 /* Max Link Width */
#define PCIECFGREG_LNKCAP_MAXWIDTH_SHIFT 4

#ifdef BCMDBG
static char *
wlc_bmac_pcie_link_speed(uint32 speed)
{
	switch (speed) {
		case 1:
			return "2.5 GT/s";
		case 2:
			return "5 GT/s";
		case 3:
			return "8 GT/s";
		default:
			return "invalid";
	}
}
#endif // endif

static void
wlc_bmac_read_pcie_link_bw(wlc_hw_info_t *wlc_hw)
{
	uint32 linkctrl, linkcap, width, speed, max_width, max_speed;
	si_t *sih = wlc_hw->sih;

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		linkctrl = si_pciereg(sih, PCIECFGREG_LINK_STATUS_CTRL, 0, 0, PCIE_CONFIGREGS);
		linkcap = si_pciereg(sih, 0XB8, 0, 0, PCIE_CONFIGREGS);
	}
#ifdef BCMPCIEDEV
	else if (BCMPCIEDEV_ENAB()) {
		si_corereg(sih, sih->buscoreidx, OFFSETOF(pcieregs_t, configindaddr_ID),
			~0, 0xB8);
		linkcap = si_corereg(sih, sih->buscoreidx, OFFSETOF(pcieregs_t,
			configinddata_ID), 0, 0);

		si_corereg(sih, sih->buscoreidx, OFFSETOF(pcieregs_t, configindaddr_ID),
			~0, PCIECFGREG_LINK_STATUS_CTRL);
		linkctrl = si_corereg(sih, sih->buscoreidx, OFFSETOF(pcieregs_t,
			configinddata_ID), 0, 0);

	}
#endif // endif
	else {
		return;
	}
	width = (linkctrl & PCIECFGREG_NEG_LNKSTA_WIDTH) >> PCIECFGREG_NEG_LNKSTA_WIDTH_SHIFT;
	speed = (linkctrl & PCIECFGREG_NEG_LNKSTA_SPEED) >> PCIECFGREG_NEG_LNKSTA_SPEED_SHIFT;
	max_width = (linkcap & PCIECFGREG_LNKCAP_MAXWIDTH) >> PCIECFGREG_LNKCAP_MAXWIDTH_SHIFT;
	max_speed = linkcap & PCIECFGREG_LNKCAP_MAXSPEED;
	wlc_hw->is_pcielink_slowspeed = ((width < max_width) || (speed < max_speed)) ? TRUE : FALSE;

#ifdef BCMDBG
	WL_ERROR(("Read from config regs: %x %x max_speed %s, max_width x%d, neg_speed %s"
		" neg_width x%d\n", linkctrl, linkcap, wlc_bmac_pcie_link_speed(max_speed),
		max_width,  wlc_bmac_pcie_link_speed(speed), width));
#endif // endif
}

bool
wlc_bmac_is_pcielink_slowspeed(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->is_pcielink_slowspeed;

}

static uint16
BCMUCODEFN(wlc_bmac_dma_max_outstread)(wlc_hw_info_t *wlc_hw)
{
	uint16 txmr = (TXMR == 2) ? DMA_MR_2 : DMA_MR_1;

	if (BCM53573_CHIP(wlc_hw->sih->chip)) {
		txmr = DMA_MR_12;
	} else if (BCM4347_CHIP(wlc_hw->sih->chip) ||
		BCM6878_CHIP(wlc_hw->sih->chip)) {
#ifdef BCMQT
		txmr = DMA_MR_1;
#else
		txmr = DMA_MR_12;
#endif	/* BCMQT */
	}
	else if (CHIPID(wlc_hw->sih->chip) == BCM43570_CHIP_ID)
		txmr = DMA_MR_12;
	else if (BCM6710_CHIP(wlc_hw->sih->chip) ||
		EMBEDDED_2x2AX_CORE(wlc_hw->sih->chip) ||
		BCM43684_CHIP(wlc_hw->sih->chip) ||
		BCM4365_CHIP(wlc_hw->sih->chip) ||
		BCM43602_CHIP(wlc_hw->sih->chip)) {
		txmr = DMA_MR_12;
	}
	return txmr;
}

static void
BCMUCODEFN(wlc_bmac_dma_param_set)(wlc_hw_info_t *wlc_hw, uint bustype, hnddma_t *di,
	bmac_dmactl_t *dmactl)
{
	uint8 burstlen_cap = 0, channel_switch = 0;

	if (BUSTYPE(bustype) == PCI_BUS) {
		if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
		    BCM4350_CHIP(wlc_hw->sih->chip) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
		    BCM43602_CHIP(wlc_hw->sih->chip) ||
		    BCM53573_CHIP(wlc_hw->sih->chip) ||
		    BCM4365_CHIP(wlc_hw->sih->chip) ||
		    BCM43684_CHIP(wlc_hw->sih->chip) ||
		    BCM6710_CHIP(wlc_hw->sih->chip)) {
			/* Do nothing. Fall thru to use setting in dmactl */
		} else {
			bzero((char *)dmactl, sizeof(bmac_dmactl_t));

			/* if not known chip, use minimal dma settings, tx/rx burstlen set to 128 */
			if (D11REV_GE(wlc_hw->corerev, 32)) {
				dmactl->txblen = dmactl->rxblen = DMA_BL_128;
			} else {
				/* For d11 rev < 32 chip, use minimal DMA setting,
				* i.e., don't set any of D11 DMA parameter
				*/
			}
		}
	} else if (BUSTYPE(bustype) == SI_BUS) {
		if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
		    BCM43602_CHIP(wlc_hw->sih->chip) ||
		    BCM4350_CHIP(wlc_hw->sih->chip) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
		    BCM53573_CHIP(wlc_hw->sih->chip)) {
			bzero((char *)dmactl, sizeof(bmac_dmactl_t));
			dmactl->txmr = wlc_bmac_dma_max_outstread(wlc_hw);
			dmactl->txpfc = DMA_PC_0;
			dmactl->txpft = DMA_PT_1;
#if defined(DONGLEBUILD) && defined(BCMSDIODEV_ENABLED)
			/* SWWLAN-27667 WAR */
			/* SWWLAN-38880 WAR for SDIO FIFO overrun or underrun issue.
			   MAC DMA burst length is reduced from 64 to 32 bytes
			   while maintaining SDIO DMA burst length to 64 bytes,
			   which effectively makes SDIO DMA faster than MAC DMA.
			*/
			dmactl->txblen = DMA_BL_32;
			dmactl->rxblen = DMA_BL_32;
#else
			dmactl->txblen = DMA_BL_64;
			dmactl->rxblen = DMA_BL_64;
#endif /* defined(DONGLEBUILD) && defined(BCMSDIODEV_ENABLED) */
			dmactl->rxpfc = DMA_PC_0;
			dmactl->rxpft = DMA_PT_1;
		} else if (IS_AC2_DEV(wlc_hw->deviceid) ||
		           BCM43684_CHIP(wlc_hw->sih->chip) ||
		           EMBEDDED_2x2AX_CORE(wlc_hw->sih->chip)) {
			/* Do nothing. Fall thru to use setting in dmactl */
		} else if (BCM4347_CHIP(wlc_hw->sih->chip) ||
			BCM6878_CHIP(wlc_hw->sih->chip)) {
			bzero((char *)dmactl, sizeof(bmac_dmactl_t));
			dmactl->txmr = wlc_bmac_dma_max_outstread(wlc_hw);
			dmactl->txpfc = DMA_PC_16;
			dmactl->txpft = DMA_PT_8;
			channel_switch = DMA_CS_ON;
#if defined(DONGLEBUILD) && defined(BCMSDIODEV_ENABLED)
			/* SWWLAN-27667 WAR */
			/* SWWLAN-38880 WAR for SDIO FIFO overrun or underrun issue.
			   MAC DMA burst length is reduced from 64 to 32 bytes
			   while maintaining SDIO DMA burst length to 64 bytes,
			   which effectively makes SDIO DMA faster than MAC DMA.
			*/
			dmactl->txblen = DMA_BL_32;
			dmactl->rxblen = DMA_BL_32;
#elif defined(BCMQT)
			/* put a large number at first then reduce it if there is
			 * dma error
			 */
			dmactl->txblen = DMA_BL_512;
			dmactl->rxblen = DMA_BL_512;
#elif defined(BCMPCIEDEV)
			/* rx burstlen has mps dependence */
			dmactl->rxblen = DMA_BL_128;
			dmactl->rxpfc = DMA_PC_16;
			dmactl->rxpft = DMA_PT_8;
#else
			dmactl->txblen = DMA_BL_64;
			dmactl->rxblen = DMA_BL_64;
#endif /* defined(DONGLEBUILD) && defined(BCMSDIODEV_ENABLED) */
		} else if (D11REV_LT(wlc_hw->corerev, 32)) {
			/* Don't set any of D11 DMA parameter */
			bzero((char *)dmactl, sizeof(bmac_dmactl_t));
		}
#ifdef BCMPCIEDEV
		if (BCMPCIEDEV_ENAB() && (wlc_hw->sih->buscoretype == PCIE2_CORE_ID)) {
			burstlen_cap = 1;
		}
#endif /* BCMPCIEDEV */
	}

	/* Finally, override DMA parameters if the chip is PCIE2 device */
	if (wlc_hw->sih->buscoretype == PCIE2_CORE_ID &&
	    (BUSTYPE(bustype) == PCI_BUS ||	/* NIC mode */
	     BCMPCIEDEV_ENAB())) {		/* FD mode */
		wlc_bmac_set_dma_burstlen_pcie(wlc_hw, dmactl);
	}
#ifdef BCMDBG
	{
		char *txmrstr[] = {"1", "2", "4", "8", "12", "16", "20", "32"};
		char *pfcstr[] = {"0", "4", "8", "16", "32"};
		char *pftstr[] = {"1", "2", "4", "8"};
		char *blenstr[] = {"16", "32", "64", "128", "256", "512", "1024", "2048"};

		WL_INFORM(("wl%d: DMA tx outstanding reads: %s(value: %d) burstlen_cap: %d,"
			"channel_switch: %d\n"
			"\ttx prefetch control: %s(value: %d) tx prefetch threshold: %s(value: %d)"
			"tx burstlen: %s(value %d)\n"
			"\trx prefetch control: %s(value: %d) rx prefetch threshold: %s(value: %d)"
			"rx burstlen: %s(value %d)\n",
			wlc_hw->unit, txmrstr[dmactl->txmr], dmactl->txmr, burstlen_cap,
			channel_switch, pfcstr[dmactl->txpfc], dmactl->txpfc, pftstr[dmactl->txpft],
			dmactl->txpft, blenstr[dmactl->txblen], dmactl->txblen,
			pfcstr[dmactl->rxpfc], dmactl->rxpfc, pftstr[dmactl->rxpft], dmactl->rxpft,
			blenstr[dmactl->rxblen], dmactl->rxblen));
	}
#endif /* BCMDBG */

	/* no. of tx outstanding reads */
	dma_param_set(di, HNDDMA_PID_TX_MULTI_OUTSTD_RD, dmactl->txmr);
	/* tx prefetch control */
	dma_param_set(di, HNDDMA_PID_TX_PREFETCH_CTL, dmactl->txpfc);
	/* tx prefetch threshold */
	dma_param_set(di, HNDDMA_PID_TX_PREFETCH_THRESH, dmactl->txpft);
	/* tx burst len */
	dma_param_set(di, HNDDMA_PID_TX_BURSTLEN, dmactl->txblen);
	/* rx prefetch control */
	dma_param_set(di, HNDDMA_PID_RX_PREFETCH_CTL, dmactl->rxpfc);
	/* rx prefetch threshold */
	dma_param_set(di, HNDDMA_PID_RX_PREFETCH_THRESH, dmactl->rxpft);
	/* rx burst len */
	dma_param_set(di, HNDDMA_PID_RX_BURSTLEN, dmactl->rxblen);
	/* tx burst size control */
	dma_param_set(di, HNDDMA_PID_BURSTLEN_CAP, burstlen_cap);
	/* tx channel switch enable */
	dma_param_set(di, HNDDMA_PID_TX_CHAN_SWITCH, channel_switch);
} /* wlc_bmac_dma_param_set */

static void
BCMINITFN(wlc_bmac_construct_dmactl)(wlc_tunables_t *tune, bmac_dmactl_t *dmactl)
{
	dmactl->txmr = (tune->txmr == 32 ? DMA_MR_32 :
			tune->txmr == 20 ? DMA_MR_20 :
			tune->txmr == 16 ? DMA_MR_16 :
			tune->txmr == 12 ? DMA_MR_12 :
			tune->txmr == 8 ? DMA_MR_8 :
			tune->txmr == 4 ? DMA_MR_4 :
			tune->txmr == 2 ? DMA_MR_2 : DMA_MR_1);
	dmactl->txpft = (tune->txpft == 8 ? DMA_PT_8 :
			tune->txpft == 4 ? DMA_PT_4 :
			tune->txpft == 2 ? DMA_PT_2 : DMA_PT_1);
	dmactl->txpfc = (tune->txpfc == 32 ? DMA_PC_32 :
			tune->txpfc == 16 ? DMA_PC_16 :
			tune->txpfc == 8 ? DMA_PC_8 :
			tune->txpfc == 4 ? DMA_PC_4 : DMA_PC_0);
	dmactl->txblen = (tune->txblen == 1024 ? DMA_BL_1024 :
			tune->txblen == 512 ? DMA_BL_512 :
			tune->txblen == 256 ? DMA_BL_256 :
			tune->txblen == 128 ? DMA_BL_128 :
			tune->txblen == 64 ? DMA_BL_64 :
			tune->txblen == 32 ? DMA_BL_32 : DMA_BL_16);

	dmactl->rxpft =  (tune->rxpft == 8 ? DMA_PT_8 :
			tune->rxpft == 4 ? DMA_PT_4 :
			tune->rxpft == 2 ? DMA_PT_2 : DMA_PT_1);
	dmactl->rxpfc = (tune->rxpfc == 16 ? DMA_PC_16 :
			tune->rxpfc == 8 ? DMA_PC_8 :
			tune->rxpfc == 4 ? DMA_PC_4 : DMA_PC_0);
	dmactl->rxblen = (tune->rxblen == 1024 ? DMA_BL_1024 :
			tune->rxblen == 512 ? DMA_BL_512 :
			tune->rxblen == 256 ? DMA_BL_256 :
			tune->rxblen == 128 ? DMA_BL_128 :
			tune->rxblen == 64 ? DMA_BL_64 :
			tune->rxblen == 32 ? DMA_BL_32 : DMA_BL_16);
}

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
/**
 * The d11 core posts phyrx status in a different area in memory wrt the received MPDUs. Phyrx
 * status'es are produced by the d11 core in a linear fashion (that is, their sts_seq number
 * increments lineair), but are consumed by the driver in a non-lineair fashion. A related data
 * structure (sts_buff_t) provides a way to create a linked list of phyrx statuses to cope with
 * this. The phyrx statuses and the sts_buff_t are allocated separately, as two arrays.
 * The sts_buff_t[0] array element points at the phyrx[0] array element, etc.
 * For rev129, the phyrx statuses are DMA'ed by the d11 core using a scatter/gather mechanism.
 * This rev129 DMA mechanism has a limitation: phyrx statuses can only be DMA'ed to an 8 byte
 * aligned address. This is achieved by aligning each phyrx struct (d11phystshdr_t) array element
 * on an 8 byte boundary.
 * For rev129, the (scatter/gather supporting) rx DMA subsystem in the driver uses a pool to 'post'
 * new descriptors. To support this mechanism, a container for free status buffers had to be
 * created, in the form of a stack. This stack was implemented as a linked list and contains
 * sts_buff_t's (so not the 'raw' phyrx status struct).
 */
static bool
wlc_bmac_phyrxsts_alloc(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh = wlc_hw->osh;
	uint rxphysts_objsz;
	uint rxphysts_size;       /**< used for short-hand notation */
	uint32 sts_mp_align = D11_GE129_STS_MP_ALIGN_BYTES;
	dmaaddr_t sts_mp_pa;      /* Aligned physical address of mpm */
	dmaaddr_t sts_mp_paorig;  /* Original physical address of mpm */
#ifdef BCM_SECURE_DMA
#ifdef BCMDMA64OSL
	dma_addr_t pa;
	dma_addr_t paddr;
#endif /* BCMDMA64OSL */
#endif /* BCM_SECURE_DMA */
	uint8 *sts_phyrx_array;      /**< base of phyrx array (aligned address) */
	int i;

	rxphysts_objsz = sizeof(d11phystshdr_t);
	if (STS_RX_ENAB(wlc_hw->wlc->pub)) {
		/* size calculation for d11phystshdr_t array (alignment requirement for eg 684b0) */
		rxphysts_objsz = ALIGN_SIZE(rxphysts_objsz, sts_mp_align);
	}

	wlc_hw->sts_phyrx_alloc_sz = rxphysts_size = (rxphysts_objsz * STSBUF_MP_N_OBJ);
	/* The phyrx array is write-only by DMA hardware and read-only by software */
	if ((wlc_hw->sts_phyrx_va_non_aligned = MALLOC(osh, wlc_hw->sts_phyrx_alloc_sz)) == NULL) {
		return FALSE;
	}

	wlc_hw->sts_buff_t_alloc_sz = sizeof(sts_buff_t) * STSBUF_MP_N_OBJ;
	/* The sts_buff_t array is not accessed by DMA hardware */
	if ((wlc_hw->sts_buff_t_array = MALLOCZ(osh, wlc_hw->sts_buff_t_alloc_sz)) == NULL) {
		MFREE(osh, wlc_hw->sts_phyrx_va_non_aligned, wlc_hw->sts_phyrx_alloc_sz);
		wlc_hw->sts_phyrx_va_non_aligned = NULL;
		return FALSE;
	}

#ifdef BCM_SECURE_DMA
#ifdef BCMDMA64OSL
		pa = SECURE_DMA_MAP(osh, wlc_hw->sts_phyrx_va_non_aligned, rxphysts_size,
			DMA_RX, NULL, NULL, NULL, 0, CMA_TXBUF_POST);
		ULONGTOPHYSADDR(pa, sts_mp_paorig);
		PHYSADDRTOULONG(sts_mp_paorig, paddr);
		SECURE_DMA_UNMAP(osh, paddr, rxphysts_size, DMA_RX, NULL, NULL, NULL, 0);
#else
	sts_mp_paorig = SECURE_DMA_MAP(osh, wlc_hw->sts_phyrx_va_non_aligned, rxphysts_size,
	                               DMA_RX, NULL, NULL, NULL, 0, CMA_TXBUF_POST);
	SECURE_DMA_UNMAP(osh, sts_mp_paorig, rxphysts_size, DMA_RX, NULL, NULL, NULL, 0);
#endif /* BCMDMA64OSL */
#else
	sts_mp_paorig = DMA_MAP(osh, wlc_hw->sts_phyrx_va_non_aligned, rxphysts_objsz,
	                        DMA_RX, NULL, NULL);
	DMA_UNMAP(osh, sts_mp_paorig, rxphysts_objsz, DMA_RX, NULL, NULL);
#endif /* BCM_SECURE_DMA */

	/* adjust the pa by rounding up to the alignment */
	PHYSADDRLOSET(sts_mp_pa, ROUNDUP(PHYSADDRLO(sts_mp_paorig), sts_mp_align));
	PHYSADDRHISET(sts_mp_pa, PHYSADDRHI(sts_mp_paorig));
	/* Make sure that alignment didn't overflow */
	ASSERT(PHYSADDRLO(sts_mp_pa) >= PHYSADDRLO(sts_mp_paorig));
	ASSERT(ISALIGNED(PHYSADDRLO(sts_mp_pa), sts_mp_align));
	/* find the alignment offset that was used */
	sts_mp_align = (uint)(PHYSADDRLO(sts_mp_pa) - PHYSADDRLO(sts_mp_paorig));
	/* adjust the va by the same offset */
	sts_phyrx_array = wlc_hw->sts_phyrx_va_non_aligned + sts_mp_align;

	/* sets a global variable used by e.g. the D11PHYSTSBUF_GE129_ACCESS_REF() macro */
	sts_mp_base_addr[wlc_hw->unit] = (uintptr)wlc_hw->sts_buff_t_array;

	/* Each sts_buff->phystshdr now has to point at the respective *aligned* phyrx array
	 * element.
	 */
	for (i = 0; i < STSBUF_MP_N_OBJ; i++) {
		wlc_hw->sts_buff_t_array[i].phystshdr =
		         (d11phystshdr_t*)(sts_phyrx_array + i * rxphysts_objsz);
#ifdef WLC_OFFLOADS_RXSTS
		wlc_hw->sts_buff_t_array[i].bmebuf_idx = STSBUF_BME_INVALID_IDX;
#endif /* WLC_OFFLOADS_RXSTS */
	}

	/* Load a 'memory pool' (implemented as a stack) with sts_buff_t elements. Element[0] is
	 * skipped because is has a special meaning: it is the 'default' status, which is read when
	 * no phyrxsts is available for a given MPDU.
	 */
	for (i = 1; i < STSBUF_MP_N_OBJ; i++) {
		STSBUF_FREE(&wlc_hw->sts_buff_t_array[i], &wlc_hw->sts_mempool);
	}

	/* set 'rx post' pool for sts dma */
	if (wlc_hw->di[STS_FIFO]) {
		dma_sts_mp_set(wlc_hw->di[STS_FIFO], (void*)&wlc_hw->sts_mempool);
	}

	wlc_hw->seqcnt = wlc_hw->sssn = 0;
#if defined(BCMPCIEDEV)
	memset(&wlc_hw->curr_seq[RXPEN_LIST_IDX], 0,
		sizeof(curr_seq_t) * MAX_RXPEN_LIST);
	wlc_hw->curr_seq[RXPEN_LIST_IDX0].seq = STS_SEQNUM_INVALID;
	wlc_hw->curr_seq[RXPEN_LIST_IDX1].seq = STS_SEQNUM_INVALID;
	memset(&wlc_hw->cons_seq[RXPEN_LIST_IDX], 0,
		sizeof(cons_seq_t) * MAX_RXPEN_LIST);
	wlc_hw->cons_seq[RXPEN_LIST_IDX0].seq = STS_SEQNUM_INVALID;
	wlc_hw->cons_seq[RXPEN_LIST_IDX1].seq = STS_SEQNUM_INVALID;
#else /* ! BCMPCIEDEV */
	wlc_hw->curr_seq[RXPEN_LIST_IDX].seq = STS_SEQNUM_INVALID;
	wlc_hw->curr_seq[RXPEN_LIST_IDX].sssn = 0;
	wlc_hw->cons_seq[RXPEN_LIST_IDX].seq = STS_SEQNUM_INVALID;
	wlc_hw->cons_seq[RXPEN_LIST_IDX].sssn = 0;
#endif /* ! BCMPCIEDEV */
	return TRUE;
} /* wlc_bmac_phyrxsts_alloc */
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

/**
 * D11 core contains several TX FIFO's and one or two RX FIFO's. These FIFO's are fed by either a
 * DMA engine or programmatically (PIO).
 */
static bool
BCMUCODEFN(wlc_bmac_attach_dmapio)(wlc_hw_info_t *wlc_hw, bool wme)
{
	uint i;
	char name[14];
	/* ucode host flag 2 needed for pio mode, independent of band and fifo */
	wlc_info_t *wlc = wlc_hw->wlc;
	uint unit = wlc_hw->unit;
	wlc_tunables_t *tune = wlc->pub->tunables;
	int extraheadroom;
	uint8 splittx_hdr = (BCMLFRAG_ENAB() ? 1 : 0);
	uint nrxd_fifo1 = 0;
	uint rxbufpost_fifo1 = 0;
	bool fifo1_rxen;
#ifdef WME
	bool fifo2_rxen;
#endif /* WME */
	dma_common_t *dmacommon = NULL;
	volatile uint32 *indqsel_reg = NULL;
	volatile uint32 *suspreq_reg[BCM_DMA_REQ_REG_COUNT] = { NULL };
	volatile uint32 *flushreq_reg[BCM_DMA_REQ_REG_COUNT] = { NULL };
	uint reg_count = 0;
	uint bustype = BUSTYPE(wlc_hw->sih->bustype);

	/* init core's pio or dma channels */
	if (PIO_ENAB_HW(wlc_hw)) {
		/* skip if already initialized, for dual band, but single core
		 * MHF2 update by wlc_pio_attach is for PR38778 WAR. The WAR is
		 * independent of band and fifo.
		 */
		if (wlc_hw->pio[0] == 0) {
			pio_t *pio;

			for (i = 0; i < NFIFO_LEGACY; i++) {
				pio = wlc_pio_attach(wlc->pub, wlc, i);
				if (pio == NULL) {
					WL_ERROR(("wlc_attach: pio_attach failed\n"));
					return FALSE;
				}
				wlc_hw_set_pio(wlc_hw, i, pio);
			}
		}
	} else if (wlc_hw->di[RX_FIFO] == NULL) {	/* Init FIFOs */

		uint addrwidth;
		osl_t *osh = wlc_hw->osh;
		hnddma_t *di;
		uint nrxd_multiplier = 1;
		bmac_dmactl_t dmactl = { DMA_MR_2, DMA_PC_16,
			DMA_PT_8, DMA_BL_1024, DMA_PC_16, DMA_PT_8, DMA_BL_128};

		/* Use the *_large tunable values for cores that support the larger DMA ring size,
		 * 4k descriptors.
		 */
		uint ntxd = (D11REV_GE(wlc_hw->corerev, 42)) ? tune->ntxd_large : tune->ntxd;
		uint nrxd = (D11REV_GE(wlc_hw->corerev, 42)) ? tune->nrxd_large : tune->nrxd;
		/* All data channels are indirect when CT is enabled. */
		uint32 dma_attach_flags = BCM_DMA_CT_ENAB(wlc) ? BCM_DMA_IND_INTF_FLAG : 0;

		/* TX FIFO layout must be known */
		ASSERT(WLC_HW_NFIFO_TOTAL(wlc) != 0);

#ifdef BULK_PKTLIST
		dma_attach_flags |= BCM_DMA_BULK_PROCESSING;
#endif // endif

#ifdef BULKRX_PKTLIST
		dma_attach_flags |= BCM_DMA_BULKRX_PROCESSING;
#endif // endif

		if (D11REV_GE(wlc_hw->corerev, 65)) {
			dma_attach_flags |= BCM_DMA_CHAN_SWITCH_EN;
		}

		if (D11REV_GE(wlc_hw->corerev, 128)) {
			dma_attach_flags |= BCM_DMA_ROEXT_SUPPORT;
			dma_attach_flags |= BCM_DMA_RX_ALIGN_8BYTE;
		}

#ifdef BCMHWA
		/* FIFO 0/1 are HWA RX FIFOFs */
		HWA_RXFILL_EXPR({
			if (hwa_dev) {
				dma_attach_flags |= BCM_DMA_HWA_MACRXFIFO;
			}
		});

		/* TX FIFO are all HWA TX FIFOFs */
		HWA_TXFIFO_EXPR({
			if (hwa_dev) {
				dma_attach_flags |= BCM_DMA_HWA_MACTXFIFO;
			}
		});
#endif /* BCMHWA */

		if (!wlc_hw->dmacommon) {
			/* init the dma_common instance */
			if (D11REV_GE(wlc_hw->corerev, 128)) {
				indqsel_reg     = DISCARD_QUAL(D11_IndAQMQSel(wlc_hw), uint32);
				suspreq_reg[0]  = DISCARD_QUAL(D11_SUSPREQ(wlc_hw), uint32);
				suspreq_reg[1]  = DISCARD_QUAL(D11_SUSPREQ1(wlc_hw), uint32);
				suspreq_reg[2]  = DISCARD_QUAL(D11_SUSPREQ2(wlc_hw), uint32);
				flushreq_reg[0] = DISCARD_QUAL(D11_FLUSHREQ(wlc_hw), uint32);
				flushreq_reg[1] = DISCARD_QUAL(D11_FLUSHREQ1(wlc_hw), uint32);
				flushreq_reg[2] = DISCARD_QUAL(D11_FLUSHREQ2(wlc_hw), uint32);
				reg_count = 3;
			} else if (D11REV_GE(wlc_hw->corerev, 65)) {
				indqsel_reg     = DISCARD_QUAL(D11_IndAQMQSel(wlc_hw), uint32);
				suspreq_reg[0]  = DISCARD_QUAL(D11_SUSPREQ(wlc_hw), uint32);
				flushreq_reg[0] = DISCARD_QUAL(D11_FLUSHREQ(wlc_hw), uint32);
				reg_count = 1;
			}
			dmacommon = dma_common_attach(wlc_hw->osh, indqsel_reg, suspreq_reg,
				flushreq_reg, reg_count);

			if (dmacommon == NULL) {
				WL_ERROR(("wl%d: wlc_attach: dma_common_attach failed\n", unit));
				return FALSE;
			}
			wlc_hw_set_dmacommon(wlc_hw, dmacommon);
		}

		/* Find out the DMA addressing capability and let OS know
		 * All the channels within one DMA core have 'common-minimum' same
		 * capability
		 */
		addrwidth = dma_addrwidth(wlc_hw->sih, DMAREG(wlc_hw, DMA_TX, 0));
		OSL_DMADDRWIDTH(osh, addrwidth);

		if (!wl_alloc_dma_resources(wlc->wl, addrwidth)) {
			WL_ERROR(("wl%d: wlc_attach: alloc_dma_resources failed\n", unit));
			return FALSE;
		}

#if !defined(BCM_GMAC3)
		STATIC_ASSERT(BCMEXTRAHDROOM >= TXOFF);
#endif // endif

		/*
		 * FIFO 0
		 * TX: TX_AC_BK_FIFO (TX AC Background data packets)
		 * RX: RX_FIFO (RX data packets). For split-Rx mode N:
		 *    0   : outputs complete packets to TCM
		 *    1,2 : outputs split packets, headers to TCM, remainder to host
		 *    3,4 : outputs payload remainder to host
		 */
		STATIC_ASSERT(TX_AC_BK_FIFO == 0);
		STATIC_ASSERT(RX_FIFO == 0);

		/* name and offsets for dma_attach */
		snprintf(name, sizeof(name), rstr_wlD, unit, 0);

		/* For split rx case, we dont want any extra head room */
		/* pkt coming from d11 dma will be used only in PKT RX path */
		/* For RX path, we dont need to grow the packet at head */
		/* Pkt loopback within a dongle case may require some changes
		 * with this logic
		 */
		extraheadroom = (BCMSPLITRX_ENAB()) ? 0 : WLRXEXTHDROOM;
		if (extraheadroom == -1) {
			extraheadroom = BCMEXTRAHDROOM;
		}
#if defined(BCMPCIEDEV) && defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
		/* for monitor mode in splitrx 1-2 we need extraheadroom for cmn_msg_hdr_t */
		if (!RXFIFO_SPLIT() && extraheadroom < sizeof(cmn_msg_hdr_t)) {
			extraheadroom = sizeof(cmn_msg_hdr_t);
		}
#endif /* BCMPCIEDEV &&  WL_MONITOR && !WL_MONITOR_DISABLED */
		/* NIC and traditional FD (SDIO, USB, etc) use RX FIFO 0 only */
		/* request SW rxhdr space through "extra head room" */
		/* TODO: add extra header room for SW rxhdr based on SplitRX mode... */
		extraheadroom += WLC_RXHDR_LEN;

		di = dma_attach_ext(wlc_hw->dmacommon, osh, name, wlc_hw->sih,
			(wme ? DMAREG(wlc_hw, DMA_TX, 0) : NULL), DMAREG(wlc_hw, DMA_RX, 0),
			dma_attach_flags,
			WLC_HW_MAP_TXFIFO(wlc, 0),
			(wme ? ntxd : 0), nrxd*nrxd_multiplier, tune->rxbufsz,
			extraheadroom, tune->nrxbufpost, wlc->datafiforxoff, &wl_msg_level);

		if (di == NULL) {
			goto dma_attach_fail;
		}

		/* Set separate rx hdr flag only for fifo-0 */
		if (SPLIT_RXMODE1() || SPLIT_RXMODE2()) {
			dma_param_set(di, HNDDMA_SEP_RX_HDR, 1);
		}

		if (RXFIFO_SPLIT()) {
			dma_param_set(di, HNDDMA_SPLIT_FIFO, SPLIT_FIFO_0);
		}

		/* Override dmactl values based on tunables, if it's pciedev. */
		if (bustype == PCI_BUS ||
		    IS_AC2_DEV(wlc_hw->deviceid) ||
		    BCM43684_CHIP(wlc_hw->sih->chip) ||
		    BCM6710_CHIP(wlc_hw->sih->chip)) {
			wlc_bmac_construct_dmactl(tune, &dmactl);
		}

		/* constructure dmactl value based on chipid */
		if (bustype == PCI_BUS || bustype == SI_BUS) {
			wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);
		}

		wlc_hw_set_di(wlc_hw, 0, di);

		/*
		 * FIFO 1
		 * TX: TX_AC_BE_FIFO (TX AC Best-Effort data packets)
		 *   (legacy) TX_DATA_FIFO (TX data packets)
		 * RX: For split-Rx mode N:
		 *    0,1 : unused
		 *    2   : packets to TCM
		 *    3,4 : headers to TCM
		 */
		STATIC_ASSERT(TX_AC_BE_FIFO == 1);
		STATIC_ASSERT(TX_DATA_FIFO == 1);
		ASSERT(wlc_hw->di[1] == 0);
		/* if fifo-1 is used for classification, use classiifcation specific tunables */
		fifo1_rxen = (PKT_CLASSIFY_EN(RX_FIFO1) || RXFIFO_SPLIT());
		nrxd_fifo1 = (fifo1_rxen ?
			(PKT_CLASSIFY_EN(RX_FIFO1) ? tune->nrxd_classified_fifo : nrxd) : 0);
		rxbufpost_fifo1 = (PKT_CLASSIFY_EN(RX_FIFO1) ? tune->bufpost_classified_fifo :
			tune->nrxbufpost);
#ifdef	FORCE_RX_FIFO1
		/* in 4349a0, fifo-2 classification will work only if fifo-1 is enabled */
		/* Enable fifo-1, but dont do any posting */
		fifo1_rxen = TRUE;
		nrxd_fifo1 = 1;
		rxbufpost_fifo1 = 0;
#endif /* FORCE_RX_FIFO1 */

		/* reserve extra header room */
		/* PCIe FD with Split Rx capability uses RX FIFO 1 */
		extraheadroom = (PKT_CLASSIFY_EN(RX_FIFO1) ? WLRXEXTHDROOM : 0);
		if (extraheadroom == -1) {
			extraheadroom = BCMEXTRAHDROOM;
		}
		/* request SW rxhdr space through "extra head room" */
		if (fifo1_rxen) {
			extraheadroom += WLC_RXHDR_LEN;
		}

		/* Since we are splitting up TCM buffers, increase no of descriptors */
		/* if splitrx is enabled, fifo-1 needs to be inited for rx too */
		snprintf(name, sizeof(name), rstr_wlD, unit, 1);
		di = dma_attach_ext(wlc_hw->dmacommon, osh, name, wlc_hw->sih,
			(wme ? DMAREG(wlc_hw, DMA_TX, 1) : NULL),
			(fifo1_rxen ? DMAREG(wlc_hw, DMA_RX, 1) : NULL),
			dma_attach_flags,
			WLC_HW_MAP_TXFIFO(wlc, 1),
			(splittx_hdr ? tune->ntxd_lfrag : ntxd),
			(fifo1_rxen ? nrxd_fifo1 : 0),
			tune->rxbufsz,
			extraheadroom, rxbufpost_fifo1, wlc->d11rxoff, &wl_msg_level);
		if (di == NULL) {
			goto dma_attach_fail;
		}

		wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);
		wlc_hw_set_di(wlc_hw, 1, di);

		if (RXFIFO_SPLIT()) {
			dma_param_set(di, HNDDMA_SPLIT_FIFO, SPLIT_FIFO_1);
		}

#ifdef BCMHWA
		/* Other FIFOs are not HWA RX FIFOF */
		HWA_RXFILL_EXPR({
		if (hwa_dev) {
			dma_attach_flags &= ~BCM_DMA_HWA_MACRXFIFO;
		}
		});
#endif // endif

#ifdef WME
		/*
		 * FIFO 2
		 * TX: TX_AC_VI_FIFO (TX AC Video data packets)
		 * RX: CTL and MGMT in RXSM4
		 */
		STATIC_ASSERT(TX_AC_VI_FIFO == 2);
		fifo2_rxen = (PKT_CLASSIFY_EN(RX_FIFO2));

		/* reserve extra header room */
		/* PCIe FD with Split Rx capability uses RX FIFO 2 */
		extraheadroom = WLRXEXTHDROOM;
		/* hnddma still has the default extra head room (-1) processing code,
		 * we could do the following logic to make sure we pass specific size
		 * to hnddma but it's not really necessary...therefore comment this
		 * code out until we need to add more extra headers...
		 */
		if (extraheadroom == -1) {
			extraheadroom = BCMEXTRAHDROOM;
		}
		/* request SW rxhdr space through "extra head room" */
		if (fifo2_rxen) {
			extraheadroom += WLC_RXHDR_LEN;
		}

		/* if splitrx mode 3 is enabled, fifo-2 needs to be inited for rx too */
		if (wme || fifo2_rxen) {
			snprintf(name, sizeof(name), rstr_wlD, unit, 2);
			di = dma_attach_ext(wlc_hw->dmacommon, osh, name, wlc_hw->sih,
				(wme ? DMAREG(wlc_hw, DMA_TX, 2):NULL),
				(fifo2_rxen ? DMAREG(wlc_hw, DMA_RX, 2) : NULL),
				dma_attach_flags,
				WLC_HW_MAP_TXFIFO(wlc, 2),
				ntxd,
				(fifo2_rxen ? tune->nrxd_classified_fifo : 0), tune->rxbufsz,
				extraheadroom, tune->bufpost_classified_fifo,
				wlc->d11rxoff, &wl_msg_level);

			if (di == NULL) {
				goto dma_attach_fail;
			}

			wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);

			wlc_hw_set_di(wlc_hw, 2, di);
		}
#endif /* WME */

		/*
		 * FIFO 3
		 * TX: TX_AC_VO_FIFO (TX AC Voice data packets)
		 *   (legacy) TX_CTL_FIFO (TX control & mgmt packets)
		 * RX: RX_TXSTATUS_FIFO (transmit-status packets)
		 *	for corerev < 5 only
		 */
		STATIC_ASSERT(TX_AC_VO_FIFO == 3);
		STATIC_ASSERT(TX_CTL_FIFO == 3);
		snprintf(name, sizeof(name), rstr_wlD, unit, 3);
		di = dma_attach_ext(wlc_hw->dmacommon, osh, name, wlc_hw->sih,
			DMAREG(wlc_hw, DMA_TX, 3),
			(STS_RX_ENAB(wlc->pub) ? DMAREG(wlc_hw, DMA_RX, 3) : NULL),
			/* Status buffers are already 8 bytes aligned */
			(STS_RX_ENAB(wlc->pub) ?
			(dma_attach_flags & ~BCM_DMA_RX_ALIGN_8BYTE) : dma_attach_flags),
			WLC_HW_MAP_TXFIFO(wlc, 3),
			ntxd,
			(STS_RX_ENAB(wlc->pub) ? tune->nrxd_sts : 0),
			(STS_RX_ENAB(wlc->pub) ? tune->rxbufsz_sts : 0),
			0, (STS_RX_ENAB(wlc->pub) ? tune->bufpost_sts : 0),
			(STS_RX_ENAB(wlc->pub) ? STSBUF_RECV_OFFSET : 0), &wl_msg_level);

		if (di == NULL) {
			goto dma_attach_fail;
		}

		wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);

		wlc_hw_set_di(wlc_hw, 3, di);

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
		if (STS_RX_ENAB(wlc_hw->wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc_hw->wlc->pub)) {
			if (wlc_bmac_phyrxsts_alloc(wlc_hw) == FALSE) {
				goto dma_attach_fail;
			}
		}
#else
		if (D11REV_GE(wlc_hw->corerev, 129) &&
			((void *)(sts_mp_base_addr[wlc_hw->unit] =
			(uintptr) MALLOC(osh, sizeof(sts_buff_t)))) == NULL) {
			goto dma_attach_fail;
		}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

#ifdef AP
		/*
		 * FIFO 4
		 * TX: TX_BCMC_FIFO (TX broadcast & multicast packets)
		 * RX: UNUSED
		 */

		STATIC_ASSERT(TX_BCMC_FIFO == 4);
		snprintf(name, sizeof(name), rstr_wlD, unit, 4);
		di = dma_attach_ext(wlc_hw->dmacommon, osh, name, wlc_hw->sih,
			DMAREG(wlc_hw, DMA_TX, 4), NULL, dma_attach_flags,
			WLC_HW_MAP_TXFIFO(wlc, 4),
			tune->ntxd_bcmc,
			0, 0, -1, 0, 0, &wl_msg_level);
		if (di == NULL)
			goto dma_attach_fail;

		wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);

		wlc_hw_set_di(wlc_hw, 4, di);
#endif /* AP */

#if defined(MBSS) || defined(WLWNM_AP)
		/*
		 * FIFO 5: TX_ATIM_FIFO
		 * TX: MBSS: but used for beacon/probe resp pkts
		 * TX: WNM_AP: used for TIM Broadcast frames
		 * RX: UNUSED
		 */
		snprintf(name, sizeof(name), rstr_wlD, unit, 5);
		di = dma_attach_ext(wlc_hw->dmacommon, osh, name, wlc_hw->sih,
			DMAREG(wlc_hw, DMA_TX, 5), NULL, dma_attach_flags,
			WLC_HW_MAP_TXFIFO(wlc, 5),
			ntxd, 0, 0, -1, 0, 0, &wl_msg_level);
		if (di == NULL)
			goto dma_attach_fail;

		wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);

		wlc_hw_set_di(wlc_hw, 5, di);
#endif /* MBSS || WLWNM_AP */

		if (D11REV_GE(wlc_hw->corerev, 128)) {
			if (BCM_DMA_CT_ENAB(wlc)) {
				uint ntxd_aqm = ntxd;

				/* attach all the remaining FIFOs */
				for (i = NFIFO_LEGACY; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
					snprintf(name, sizeof(name), rstr_wlD, unit, i);
					di = dma_attach_ext(wlc_hw->dmacommon, osh, name,
						wlc_hw->sih,
						(void*)(uintptr)(
						&(D11Reggrp_inddma(wlc_hw, 0)->dma)),
						NULL, dma_attach_flags,
						WLC_HW_MAP_TXFIFO(wlc, i),
						ntxd, 0, 0, -1, 0, 0, &wl_msg_level);
					if (di == NULL) {
						WL_ERROR(("wl%d: dma_attach inddma fifo %d failed"
							"\n", unit, i));
						goto dma_attach_fail;
					}
					wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);
					wlc_hw_set_di(wlc_hw, i, di);
				}

				/* for AQM DMAs set the BCM_DMA_DESC_ONLY_FLAG also */
				dma_attach_flags = BCM_DMA_IND_INTF_FLAG | BCM_DMA_DESC_ONLY_FLAG |
					BCM_DMA_CHAN_SWITCH_EN;

#ifdef BCMHWA
				/* AQM FIFO are all HWA AQM FIFOFs */
				HWA_TXFIFO_EXPR({
					if (hwa_dev) {
						dma_attach_flags |= BCM_DMA_HWA_MACTXFIFO;
					}
				});
#endif // endif

#ifdef DONGLEBUILD
				ntxd_aqm = MIN(256, ntxd);
#else /* !DONGLEBUILD */
				ntxd_aqm = MIN(1024, ntxd);
#endif /* DONGLEBUILD */

				/* as needed attach all the AQM DMAs */
				for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
					snprintf(name, sizeof(name), rstr_wl_aqmD, unit, i);
					di = dma_attach_ext(wlc_hw->dmacommon, osh, name,
						wlc_hw->sih,
						(void*)(uintptr)(
						&(D11Reggrp_indaqm(wlc_hw, 0)->dma)),
						NULL, dma_attach_flags,
						WLC_HW_MAP_TXFIFO(wlc, i),
						ntxd_aqm, 0, 0, -1, 0, 0, &wl_msg_level);
					if (di == NULL) {
						WL_ERROR(("wl%d: dma_attach indaqm fifo %d failed"
							"\n", unit, i));
						goto dma_attach_fail;
					}
					wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);
					wlc_hw_set_aqm_di(wlc_hw, i, di);
				} /* for */
			}
		} else if (D11REV_GE(wlc_hw->corerev, 65)) {
			if (BCM_DMA_CT_ENAB(wlc)) {
				uint ntxd_aqm = ntxd;
				bool vo_hfifo = FALSE;

#if defined(WL_MU_TX) && !defined(WL_MU_TX_DISABLED)
				/* Cannot use MU_TX_ENAB here as the MU_TX and Beamforming modules
				 * are attached after dma_attach
				 */
				/* attach all the remaining FIFOs needed for MU TX */
				for (i = TX_FIFO_MU_START; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
					if (bustype == SI_BUS &&
					    D11REV_IS(wlc_hw->corerev, 65) &&
					    (i > TX_FIFO_25)) {
						/* skip unused fifo for 4365C0 in dongle */
						continue;
					}

					/* Reduce half of the ntxd for ac-vo to save more memory */
					vo_hfifo = FALSE;
					if (D11REV_IS(wlc_hw->corerev, 65)) {
						vo_hfifo = wlc_mutx_txfifo_ac_matching(i, AC_VO);
					}

					snprintf(name, sizeof(name), rstr_wlD, unit, i);
					di = dma_attach_ext(wlc_hw->dmacommon, osh, name,
						wlc_hw->sih,
						(void*)(uintptr)(
						&(D11Reggrp_inddma(wlc_hw, 0)->dma)),
						NULL,  dma_attach_flags, (uint8)i,
						(vo_hfifo ? (ntxd >> 1) : ntxd),
						0, 0, -1, 0, 0, &wl_msg_level);
					if (di == NULL) {
						WL_ERROR(("wl%d: dma_attach inddma failed"
							"NFIFO_EXT=%d\n", unit, i));
						goto dma_attach_fail;
					}
					wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);
					wlc_hw_set_di(wlc_hw, i, di);
				}
#endif /* WL_MU_TX && !WL_MU_TX_DISABLED */

				/* for AQM DMAs set the BCM_DMA_DESC_ONLY_FLAG also */
				dma_attach_flags = BCM_DMA_IND_INTF_FLAG | BCM_DMA_DESC_ONLY_FLAG |
					BCM_DMA_CHAN_SWITCH_EN;

#ifdef DONGLEBUILD
				ntxd_aqm = MIN(256, ntxd);
#else /* !DONGLEBUILD */
				ntxd_aqm = MIN(1024, ntxd);
#endif /* DONGLEBUILD */

				/* as needed attach all the AQM DMAs */
				for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
					vo_hfifo = FALSE;

#if defined(WL_MU_TX) && !defined(WL_MU_TX_DISABLED)
					/* skip FIFO #6 and #7 as they are not used */
					if ((i == TX_FIFO_6)||(i == TX_FIFO_7)) {
						continue;
					}

					if (bustype == SI_BUS &&
					    (i >= TX_FIFO_MU_START) &&
					    (D11REV_IS(wlc_hw->corerev, 65) &&
					    (i > TX_FIFO_25))) {
						/* skip unused fifo for 4365C0 in dongle */
						continue;
					}

					/* Reduce half of the ntxd for ac-vo to save more memory */
					if (D11REV_IS(wlc_hw->corerev, 65) &&
						(i >= TX_FIFO_MU_START)) {
						vo_hfifo = wlc_mutx_txfifo_ac_matching(i, AC_VO);
					}
#endif /* WL_MU_TX && !WL_MU_TX_DISABLED */

					snprintf(name, sizeof(name), rstr_wl_aqmD, unit, i);
					di = dma_attach_ext(wlc_hw->dmacommon, osh, name,
						wlc_hw->sih,
						(void*)(uintptr)(
						&(D11Reggrp_indaqm(wlc_hw, 0)->dma)),
						NULL,  dma_attach_flags, (uint8)i,
						(vo_hfifo ? (ntxd_aqm >> 1) : ntxd_aqm),
						0, 0, -1, 0, 0, &wl_msg_level);
					if (di == NULL) {
						WL_ERROR(("wl%d: dma_attach indaqm failed "
							"NFIFO_EXT=%d\n", unit, i));
						goto dma_attach_fail;
					}
					wlc_bmac_dma_param_set(wlc_hw, bustype, di, &dmactl);
					wlc_hw_set_aqm_di(wlc_hw, i, di);

				} /* for */
			}
		} /* if (D11REV_GE(wlc_hw->corerev, 65)) */

		/* get pointer to dma engine tx flow control variable */
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
			if (wlc_hw->di[i]) {
				wlc_hw->txavail[i] =
					(uint*)dma_getvar(wlc_hw->di[i], "&txavail");
				if (BCM_DMA_CT_ENAB(wlc)) {
					if (wlc_hw->aqm_di[i]) {
						wlc_hw->txavail_aqm[i] =
							(uint*)dma_getvar(wlc_hw->aqm_di[i],
							"&txavail");
					}
				}
			}
		}

#ifdef BCMHWA
		/* Config RX/TX DMA address and depth */
		if (hwa_dev) {
			HWA_RXFILL_EXPR(dma64regs_t *d64regs);
#if defined(HWA_RXFILL_BUILD) || defined(HWA_TXFIFO_BUILD)
			int ret;
			uint32 fifo_depth;
			dmaaddr_t fifo_base;
			dma64addr_t fifo_addr;
#endif // endif
			HWA_TXFIFO_EXPR(uint32 aqm_fifo_depth);
			HWA_TXFIFO_EXPR(dmaaddr_t aqm_fifo_base);
			HWA_TXFIFO_EXPR(dma64addr_t aqm_fifo_addr);

			//RX
			HWA_RXFILL_EXPR({
			for (i = 0; i < RX_FIFO2; i++) {
				if (wlc_hw->di[i] == NULL)
					continue;

				fifo_depth = 0;
				ret = dma_get_fifo_info(wlc_hw->di[i], &fifo_base,
					&fifo_depth, FALSE);
				if (ret != BCME_OK || fifo_depth == 0) {
					WL_ERROR(("wl%d: RX DATA descriptor table %d is empty,"
						" ret(%d)\n", unit, i, ret));
					ASSERT(0);
					continue;
				}

				PHYSADDR64HISET(fifo_addr, PHYSADDRHI(fifo_base));
				PHYSADDR64LOSET(fifo_addr, PHYSADDRLO(fifo_base));
				d64regs = DMAREG(wlc_hw, DMA_RX, i);

				hwa_rxfill_fifo_attach(wlc, 0, i, fifo_depth, fifo_addr,
					(uint32)&d64regs->ptr);
			}
			});

			// TX
			HWA_TXFIFO_EXPR({
			for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
				//DATA
				if (wlc_hw->di[i] == NULL)
					continue;

				fifo_depth = 0;
				ret = dma_get_fifo_info(wlc_hw->di[i], &fifo_base,
					&fifo_depth, TRUE);
				if (ret != BCME_OK || fifo_depth == 0) {
					WL_ERROR(("wl%d: TX DATA descriptor table %d is empty,"
						" ret(%d)\n", unit, i, ret));
					ASSERT(0);
					continue;
				}
				PHYSADDR64HISET(fifo_addr, PHYSADDRHI(fifo_base));
				PHYSADDR64LOSET(fifo_addr, PHYSADDRLO(fifo_base));

				//AQM
				ASSERT(wlc_hw->aqm_di[i]);
				aqm_fifo_depth = 0;
				ret = dma_get_fifo_info(wlc_hw->aqm_di[i], &aqm_fifo_base,
					&aqm_fifo_depth, TRUE);
				if (ret != BCME_OK || aqm_fifo_depth == 0) {
					WL_ERROR(("wl%d: TX AQM descriptor table %d is empty,"
						" ret(%d)\n", unit, i, ret));
					ASSERT(0);
					continue;
				}
				PHYSADDR64HISET(aqm_fifo_addr, PHYSADDRHI(aqm_fifo_base));
				PHYSADDR64LOSET(aqm_fifo_addr, PHYSADDRLO(aqm_fifo_base));

				if (hwa_txfifo_config(hwa_dev, 0, WLC_HW_MAP_TXFIFO(wlc, i),
					fifo_addr, fifo_depth, aqm_fifo_addr, aqm_fifo_depth)
					!= HWA_SUCCESS) {
					WL_ERROR(("wl%d: HWA txfifo config failed for fifo %d\n",
						unit, i));
					goto dma_attach_fail;
				}
			}
			});
		}
#endif /* BCMHWA */
	}

	if (BCMSPLITRX_ENAB()) {
		WL_ERROR(("wlc_bmac_attach_dmapio: enable split RX\n"));
		/* enable host flags to do ucode frame classification */
		wlc_bmac_enable_rx_hostmem_access(wlc_hw, TRUE);
		wlc_mhf(wlc, MHF3, MHF3_SELECT_RXF1, MHF3_SELECT_RXF1, WLC_BAND_ALL);
	}

	if (PKT_CLASSIFY()) {
		WL_ERROR(("wlc_bmac_attach_dmapio: Enable PKT_CLASSIFY\n"));
		/* enable host flags to do ucode frame classification */
		wlc_bmac_enable_rx_hostmem_access(wlc_hw, TRUE);
	}

	if (RXFIFO_SPLIT()) {
		WL_ERROR(("wlc_bmac_attach_dmapio: link F0/F1 dma \n"));
		dma_link_handle(wlc_hw->di[RX_FIFO1], wlc_hw->di[RX_FIFO]);
	}

#ifdef STS_FIFO_RXEN
	if (STS_RX_ENAB(wlc_hw->wlc->pub)) {
		dma_link_sts_handle(wlc_hw->di[STS_FIFO], wlc_hw->di[RX_FIFO]);
		if (wlc_hw->di[RX_FIFO1]) {
			dma_link_sts_handle(wlc_hw->di[STS_FIFO], wlc_hw->di[RX_FIFO1]);
		}
		if (wlc_hw->di[RX_FIFO2]) {
			dma_link_sts_handle(wlc_hw->di[STS_FIFO], wlc_hw->di[RX_FIFO2]);
		}
	}
#endif /* STS_FIFO_RXEN */

	return TRUE;

dma_attach_fail:
	WL_ERROR(("wl%d: wlc_attach: dma_attach failed\n", unit));
	return FALSE;
} /* wlc_bmac_attach_dmapio */

static void
BCMUCODEFN(wlc_bmac_detach_dmapio)(wlc_hw_info_t *wlc_hw)
{
	uint j;

	for (j = 0; j < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); j++) {
		if (!PIO_ENAB_HW(wlc_hw)) {
			if (wlc_hw->di[j]) {
				dma_detach(wlc_hw->di[j]);
				wlc_hw_set_di(wlc_hw, j, NULL);
			}
		} else {
			if ((j < NFIFO_LEGACY) && wlc_hw->pio[j]) {
				wlc_pio_detach(wlc_hw->pio[j]);
				wlc_hw_set_pio(wlc_hw, j, NULL);
			}
		}
#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
		/* Detach all the AQM DMAs */
		if (wlc_hw->aqm_di[j]) {
			dma_detach(wlc_hw->aqm_di[j]);
			wlc_hw_set_aqm_di(wlc_hw, j, NULL);
		}
#endif /* defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED) */
	}

	if (wlc_hw->dmacommon) {
		/* detach dma_common */
		dma_common_detach(wlc_hw->dmacommon);
		wlc_hw_set_dmacommon(wlc_hw, NULL);
	}
}

#define GPIO_4_BTSWITCH          (1 << 4)
#define GPIO_4_GPIOOUT_DEFAULT    0
#define GPIO_4_GPIOOUTEN_DEFAULT  0

int
wlc_bmac_set_btswitch_ext(wlc_hw_info_t *wlc_hw, int8 state)
{
	if (WLCISACPHY(wlc_hw->band)) {
		if (wlc_hw->up) {
			wlc_phy_set_femctrl_bt_wlan_ovrd(wlc_hw->band->pi, state);
			/* Save switch state */
			wlc_hw->btswitch_ovrd_state = state;
			return BCME_OK;
		} else {
			return BCME_NOTUP;
		}
	}

	return BCME_UNSUPPORTED;
}

#if defined(SAVERESTORE) /* conserves power by powering off parts of the chip when idle \
	*/
static CONST uint32 *
BCMPREATTACHFNSR(wlc_bmac_sr_params_get)(wlc_hw_info_t *wlc_hw,
	int sr_core, uint32 *offset, uint32 *srfwsz)
{
	CONST uint32 *srfw = sr_get_sr_params(wlc_hw->sih, sr_core, srfwsz, offset);

	if (D11REV_IS(wlc_hw->corerev, 48) || D11REV_IS(wlc_hw->corerev, 49))
		*offset += D11MAC_BMC_SRASM_OFFSET - (D11MAC_BMC_STARTADDR_SRASM << 8);
	else if (D11REV_IS(wlc_hw->corerev, 56) || D11REV_IS(wlc_hw->corerev, 61))
			*offset <<= 1;

	if ((D11REV_IS(wlc_hw->corerev, 61)) &&	(sr_core == SRENG2)) {
		/* For dig sr engine, we only use second vasip memory bank for SR,
		 * calc start address of second bank + sr offset
		 */
		*offset += VASIP_MEM_BANK_SIZE;
	}
	return srfw;
}

#ifdef BCMDBG_SR
/**
 * SR sanity check
 * - ASM code is expected to be constant so compare original with txfifo
 */
static int
wlc_bmac_sr_verify_ex(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b, int sr_core)
{
	int i;
	uint32 offset = 0;
	uint32 srfwsz = 0;
	CONST uint32 *srfw;
	uint32 c1, buf[1];
	bool asm_pass = TRUE;

	bcm_bprintf(b, "SR ASM:\n");
	if (!wlc_hw->wlc->clk) {
		bcm_bprintf(b, "No clk\n");
		WL_ERROR(("No clk\n"));
		return BCME_NOCLK;
	}

	srfw = wlc_bmac_sr_params_get(wlc_hw, sr_core, &offset, &srfwsz);

	if (sr_core == SRENG2) {
		phy_vasip_set_clk((phy_info_t *)wlc_hw->band->pi, TRUE);
		for (i = 0; i < srfwsz/4; i ++) {
			c1 = *srfw++;
			wlc_vasip_read(wlc_hw, buf, 4, i * 4 + offset);

			if (c1 != buf[0]) {
				bcm_bprintf(b, "\ncmp failed: %d exp: 0x%x got: 0x%x\n",
					i, c1, buf[0]);
				asm_pass = FALSE;
				break;
			}
		}
		phy_vasip_set_clk((phy_info_t *)wlc_hw->band->pi, FALSE);
	} else {
		/* The template region starts where the BMC_STARTADDR starts.
		 * This shouldn't use a #defined value but some parameter in a
		 * global struct.
		 */
		if (D11REV_IS(wlc_hw->corerev, 48) || D11REV_IS(wlc_hw->corerev, 49))
			offset += (D11MAC_BMC_STARTADDR_SRASM << 8);
		wlc_bmac_templateptr_wreg(wlc_hw, offset);
		bcm_bprintf(b, "len: %d offset: 0x%x ", srfwsz, wlc_bmac_templateptr_rreg(wlc_hw));

		for (i = 0; i < (srfwsz/4); i++) {
			c1 = *srfw++;
			buf[0] = wlc_bmac_templatedata_rreg(wlc_hw);
			if (c1 != buf[0]) {
				bcm_bprintf(b, "\ncmp failed: %d - 0x%x exp: 0x%x got: 0x%x\n",
					i, wlc_bmac_templateptr_rreg(wlc_hw), c1, buf[0]);
				asm_pass = FALSE;
				break;
			}
		}
	}

	bcm_bprintf(b, "\ncmp: %s", asm_pass ? "PASS" : "FAIL");
	bcm_bprintf(b, "\n");
	WL_ERROR(("[%d] cmp: %s\n", sr_core, asm_pass ? "PASS" : "FAIL"));
	return 0;
}

int
wlc_bmac_sr_verify(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b)
{
	wlc_bmac_sr_verify_ex(wlc_hw, b, SRENG0);

	if (RSDB_ENAB(wlc_hw->wlc->pub) && (WLC_DUALMAC_RSDB(wlc_hw->wlc->cmn))) {
		wlc_info_t *wlc1 = wlc_rsdb_get_other_wlc(wlc_hw->wlc);
		wlc_bmac_sr_verify_ex(wlc1->hw, b, SRENG1);
	}

	if (wlc_vasip_present(wlc_hw))
		wlc_bmac_sr_verify_ex(wlc_hw, b, SRENG2);

	return 0;
}

int
wlc_bmac_sr_testmode(wlc_hw_info_t *wlc_hw, int mode)
{
	wlc_info_t *wlc = wlc_hw->wlc;

	WL_ERROR(("Testing mode:%d\n", mode));
	if (mode >= 50) {
		WL_ERROR(("schedule timer:%d\n", mode));
		wl_add_timer(wlc->wl, wlc_hw->srtimer, mode, TRUE);
	} else if (mode == 0) {
		wl_del_timer(wlc->wl, wlc_hw->srtimer);
	}

	return 0;
}
#endif /* BCMDBG_SR */

/** S/R binary code is written into the D11 TX FIFO */
static int
BCMPREATTACHFN(wlc_bmac_sr_asm_download)(wlc_hw_info_t *wlc_hw, int sr_core)
{
	uint32 offset = 0;
	uint32 srfwsz = 0;
	CONST uint32 *srfw = wlc_bmac_sr_params_get(wlc_hw, sr_core, &offset, &srfwsz);

	if (D11REV_IS(wlc_hw->corerev, 61)) {
		if (sr_core == SRENG2) {
			d11regs_t *regs = wlc_hw->regs;

			/* download S/R binary for VASIP here
			 * write binary to the vasip program memory
			 */
			phy_prephy_vasip_clk_set(wlc_hw->prepi, regs, TRUE);
			wlc_vasip_write(wlc_hw, srfw, srfwsz, offset, 0);
			phy_prephy_vasip_clk_set(wlc_hw->prepi, regs, FALSE);
			return BCME_OK;
		}
	}

	wlc_bmac_write_template_ram(wlc_hw, offset, srfwsz, (const void *)srfw);
	return BCME_OK;
}

static int
BCMPREATTACHFN(wlc_bmac_sr_enable)(wlc_hw_info_t *wlc_hw, int sr_core)
{
	sr_engine_enable_post_dnld(wlc_hw->sih, sr_core, TRUE);

	/*
	 * After enabling SR engine, update PMU min res mask
	 * This is done before si_clkctl_fast_pwrup_delay().
	 */
	if (sr_isenab(wlc_hw->sih)) {
		si_update_masks(wlc_hw->sih);
	}

	return BCME_OK;
}

static int
BCMPREATTACHFN(wlc_bmac_sr_init)(wlc_hw_info_t *wlc_hw, int sr_core)
{
	if (sr_cap(wlc_hw->sih) == FALSE) {
		WL_ERROR(("%s: sr not supported\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (wlc_bmac_sr_asm_download(wlc_hw, sr_core) == BCME_OK) {
		wlc_bmac_sr_enable(wlc_hw, sr_core);
	} else {
		WL_ERROR(("%s: sr download failed\n", __FUNCTION__));
	}

	return BCME_OK;
}

static bool
wlc_sr_save(void *arg)
{
	wlc_info_t *wlc = arg;

	ASSERT(wlc);
	if (SRPWR_ENAB()) {
		wlc_srpwr_request_off(wlc);
	}

	return TRUE;
}
#endif /* SAVERESTORE */

wlc_hw_info_t *
BCMATTACHFN(wlc_bmac_phase1_attach)(osl_t *osh, si_t *sih, char *vars, uint varsz,
	uint *perr, uint unit)
{
	int err = 0;
	wlc_hw_info_t *wlc_hw = NULL;
	uint32 resetflags = 0;

	if ((wlc_hw = (wlc_hw_info_t*) MALLOC_NOPERSIST(osh, sizeof(wlc_hw_info_t))) == NULL) {
		WL_ERROR(("%s: no mem for wlc_hw, malloced %d bytes\n", __FUNCTION__,
			MALLOCED(osh)));
		err = 800;
		goto fail;
	}

	bzero((char *)wlc_hw, sizeof(wlc_hw_info_t));
	wlc_hw->sih = sih;
	wlc_hw->vars = vars;
	wlc_hw->vars_size = varsz;
#if defined(DONGLEBUILD) && defined(WLRSDB)
	wlc_hw->regs = (d11regs_t *)si_setcore(wlc_hw->sih, D11_CORE_ID, unit);
#else
	wlc_hw->regs = (d11regs_t *)si_setcore(wlc_hw->sih, D11_CORE_ID, 0);
#endif /* (DONGLEBUILD) && (WLRSDB) */
	ASSERT(wlc_hw->regs != NULL);

	/* Do the reset of both cores together at this point for RSDB
	 * device
	 */
	wlc_hw->macunit = 0;
	wlc_hw->num_mac_chains = si_numd11coreunits(wlc_hw->sih);
	wlc_hw->corerev = si_corerev(wlc_hw->sih);
	wlc_hw->osh = osh;
	wlc_hw->bmac_phase = BMAC_PHASE_1;
	wlc_hw->boardflags = (uint32)getintvar(vars, rstr_boardflags);

	err = d11regs_select_offsets_tbl(&wlc_hw->regoffsets, wlc_hw->corerev);
	if (err) {
		WL_ERROR(("%s: failed, regs for rev not found\n", __FUNCTION__));
		err = 36;
		goto fail;
	}

#ifdef WLP2P_UCODE_ONLY
	/*
	 * DL_P2P_UC() evaluates to wlc_hw->_p2p in the ROM, so must set this field
	 * appropriately in order to decide which ucode to load.
	 */
	wlc_hw->_p2p = TRUE;	/* P2P ucode must be loaded in this case */
#endif // endif

	wlc_bmac_init_core_reset_disable_fn(wlc_hw);
	wlc_bmac_core_reset(wlc_hw, wlc_bmac_phase1_get_resetflags(wlc_hw), resetflags);

	err = wlc_bmac_phase1_phy_attach(wlc_hw);
	if (!err) {
		goto done;
	}
fail:
	wlc_bmac_phase1_detach(wlc_hw);
	wlc_hw = NULL;
done:
	if (perr)
		*perr = err;
	return wlc_hw;
} /* wlc_bmac_phase1_attach */

static uint32
BCMATTACHFN(wlc_bmac_phase1_get_resetflags)(wlc_hw_info_t *wlc_hw)
{
	return (SICF_PRST | SICF_PCLKE);
}

int
BCMATTACHFN(wlc_bmac_phase1_detach)(wlc_hw_info_t *wlc_hw)
{
	if (!wlc_hw)
		return BCME_OK;
	wlc_bmac_phase1_phy_detach(wlc_hw);

	MFREE(wlc_hw->osh, wlc_hw, sizeof(wlc_hw_info_t));

	return BCME_OK;
}

static uint
BCMATTACHFN(wlc_bmac_phase1_phy_attach)(wlc_hw_info_t *wlc_hw)
{
	int err = 0;
	shared_phy_params_t sha_params;

	wlc_bmac_phy_reset_preattach(wlc_hw);

	wlc_hw->physhim = wlc_phy_shim_attach(wlc_hw, NULL, NULL);

	if (wlc_hw->physhim == NULL) {
		WL_ERROR(("wl%d: %s: wlc_phy_shim_attach failed\n",
			wlc_hw->macunit, __FUNCTION__));
		err = 802;
		goto fail;
	}

	sha_params.osh = wlc_hw->osh;
	sha_params.sih = wlc_hw->sih;
	sha_params.unit = wlc_hw->macunit;
	sha_params.corerev = wlc_hw->corerev;
	sha_params.physhim = wlc_hw->physhim;

	if ((wlc_hw->phy_sh = wlc_prephy_shared_attach(&sha_params)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_prephy_shared_attach failed\n",
			wlc_hw->macunit, __FUNCTION__));
		err = 803;
		goto fail;
	}
	if ((wlc_hw->prepi = prephy_module_attach(wlc_hw->phy_sh,
		(void *)(uintptr)wlc_hw->regs)) == NULL) {
		WL_ERROR(("wl%d: %s: prephy_module_attach failed\n",
			wlc_hw->macunit, __FUNCTION__));
		err = 804;
		goto fail;
	}
	goto done;
fail:
	wlc_bmac_phase1_phy_detach(wlc_hw);
done:
	return err;
}

static int
BCMATTACHFN(wlc_bmac_phase1_phy_detach)(wlc_hw_info_t *wlc_hw)
{
	if (wlc_hw->physhim)
		wlc_phy_shim_detach(wlc_hw->physhim);
	if (wlc_hw->prepi)
		prephy_module_detach(wlc_hw->prepi);
	if (wlc_hw->phy_sh)
		wlc_prephy_shared_detach(wlc_hw->phy_sh);
	return BCME_OK;
}

/** Only called for firmware builds. Saves RAM by freeing ucode and SR arrays in an early stadium */
int
BCMPREATTACHFN(wlc_bmac_process_ucode_sr)(wlc_hw_info_t *wlc_hw)
{
	int err;
	uint32 num_d11_cores = 1;
	uint32 resetflags = 0;

	if (!wlc_hw)
	   return BCME_ERROR;

#if defined(BCMULP) && !defined(WLULP_DISABLED)
	p2_handle_t *p2_handle;

	/* allocate wlc_hw_info_t state structure */
	/* freed after ucode download for firmware builds */
	if ((p2_handle = (p2_handle_t*) MALLOC_NOPERSIST(wlc_hw->osh,
			sizeof(p2_handle_t))) == NULL) {
		WL_ERROR(("%s: no mem for wlc_hw, malloced %d bytes\n", __FUNCTION__,
			MALLOCED(wlc_hw->osh)));
		err = 0;
		return err;
	}
#endif // endif

	if (SRPWR_ENAB()) {
		/* Power request on Main domain */
		si_srpwr_request(wlc_hw->sih,
			SRPWR_DMN3_MACMAIN_MASK, SRPWR_DMN3_MACMAIN_MASK);
	}

#if defined(BCMULP) && !defined(WLULP_DISABLED)
	p2_handle->wlc_hw = wlc_hw;
	wlc_bmac_autod11_shm_upd(wlc_hw, D11_IF_SHM_ULP);
	ulp_p2_retrieve(p2_handle);
#endif // endif
	wlc_ucode_download(wlc_hw);

	/* vasip download as part of pre-attach. */
	if (VASIP_ACTIVATE() || SR_ENAB()) {
		wlc_vasip_preattach_init(wlc_hw);
	}
	num_d11_cores = wlc_hw->num_mac_chains;

	/* Check the mac capability */
	wlc_hw->num_mac_chains = 1 + ((wlc_hw->machwcap1 & MCAP1_NUMMACCHAINS)
	                               >> MCAP1_NUMMACCHAINS_SHIFT) > 1;

	if (num_d11_cores == 2 && wlc_hw->num_mac_chains == 1) {
		/* If it is dual mac instead of rsdb mac (like 4364), we have to download
		 * different ucode for core0 and core1
		 */
		wlc_hw->macunit = 1;
		wlc_hw->ucode_loaded = FALSE;

		if (SRPWR_ENAB()) {
			/* Power request on Aux domain */
			si_srpwr_request(wlc_hw->sih,
				SRPWR_DMN2_MACAUX_MASK, SRPWR_DMN2_MACAUX_MASK);
		}

		wlc_hw->regs = (d11regs_t *)si_setcore(wlc_hw->sih, D11_CORE_ID, 1);
		ASSERT(wlc_hw->regs != NULL);

		wlc_bmac_core_reset(wlc_hw, wlc_bmac_phase1_get_resetflags(wlc_hw), resetflags);
		wlc_ucode_download(wlc_hw);
#if defined(SAVERESTORE)
		if (SR_ENAB()) {
			/* SR ASM download and SR enable for D11 MAIN core */
			wlc_hw->macunit = 0;
			wlc_hw->regs = (d11regs_t *)si_setcore(wlc_hw->sih, D11_CORE_ID, 0);
			wlc_bmac_sr_init(wlc_hw, SRENG1); /* SRENG1 is for D11 MAIN core */

			/* SR ASM download and SR enable for D11 AUX core */
			wlc_hw->macunit = 1;
			wlc_hw->regs = (d11regs_t *)si_setcore(wlc_hw->sih, D11_CORE_ID, 1);
			wlc_bmac_sr_init(wlc_hw, SRENG0); /* SRENG0 is for D11 AUX core */

			if (D11REV_IS(wlc_hw->corerev, 61)) {
				/* sr engine for VASIP */
				wlc_bmac_sr_init(wlc_hw, SRENG2);
			}
		}
#endif /* SAVERESTORE */
#if !defined(WLRSDB) || defined(WLRSDB_DISABLED)
		wlc_bmac_core_disable(wlc_hw, 0);
#endif // endif
	}
	else {
#if defined(SAVERESTORE)
		/* Download SR code and reclaim: ~3.5K for 4350, ~2.2K for 4335 */
		if (SR_ENAB()) {
			wlc_hw->macunit = 0;
			wlc_hw->regs = (d11regs_t *)si_setcore(wlc_hw->sih, D11_CORE_ID, 0);
			wlc_bmac_sr_init(wlc_hw, wlc_hw->macunit);
		}
#endif // endif
	}
	err = 0;

#if defined(BCMULP) && !defined(WLULP_DISABLED)
	MFREE(wlc_hw->osh, p2_handle, sizeof(p2_handle_t));
#endif // endif
	return err;
} /* wlc_bmac_process_ucode_sr */

int wlc_bmac_4360_pcie2_war(wlc_hw_info_t* wlc_hw, uint32 vcofreq);

/**
 * BMAC level function to allocate si handle.
 *     @param device   pci device id (used to determine chip#)
 *     @param osh      opaque OS handle
 *     @param regs     virtual address of initial core registers
 *     @param bustype  pci/pcmcia/sb/sdio/etc
 *     @param btparam  opaque pointer pointing to e.g. a Linux 'struct pci_dev'
 *     @param vars     pointer to a pointer area for "environment" variables
 *     @param varsz    pointer to int to return the size of the vars
 */
si_t *
BCMATTACHFN(wlc_bmac_si_attach)(uint device, osl_t *osh, volatile void *regsva, uint bustype,
	void *btparam, char **vars, uint *varsz)
{
#ifdef NVSRCX
	srom_info_init(osh);
#endif // endif
	return si_attach(device, osh, regsva, bustype, btparam, vars, varsz);
}

/** may be called with core in reset */
void
BCMATTACHFN(wlc_bmac_si_detach)(osl_t *osh, si_t *sih)
{
	BCM_REFERENCE(osh);

	if (sih) {
		MODULE_DETACH(sih, si_detach);
	}
}

/** register iovar table/handlers to the system */
static int
BCMATTACHFN(wlc_bmac_register_iovt_all)(wlc_hw_info_t *hw, wlc_iocv_info_t *ii)
{
	phy_info_t *pi, *prev_pi = NULL;
	int err;
	uint i;

	if ((err = wlc_bmac_register_iovt(hw, ii)) != BCME_OK) {
		WL_ERROR(("%s: wlc_bmac_register_iovt failed\n", __FUNCTION__));
		goto fail;
	}

	for (i = 0; i < MAXBANDS; i++) {
		wlc_hwband_t *band = hw->bandstate[i];

		if (band == NULL)
			continue;

		pi = (phy_info_t *)band->pi;
		if (pi == NULL)
			continue;

		if (pi == prev_pi)
			continue;

		if ((err = phy_register_iovt_all(pi, ii)) != BCME_OK) {
			WL_ERROR(("%s: phy_register_iovt_all failed\n", __FUNCTION__));
			goto fail;
		}

		prev_pi = pi;
	}

	return BCME_OK;

fail:
	return err;
}

static int
BCMATTACHFN(wlc_bmac_register_ioct_all)(wlc_hw_info_t *hw, wlc_iocv_info_t *ii)
{
	phy_info_t *pi, *prev_pi = NULL;
	int err;
	uint i;

	for (i = 0; i < MAXBANDS; i++) {
		wlc_hwband_t *band = hw->bandstate[i];

		if (band == NULL)
			continue;

		pi = (phy_info_t *)band->pi;
		if (pi == NULL)
			continue;

		if (pi == prev_pi)
			continue;

		if ((err = phy_register_ioct_all(pi, ii)) != BCME_OK) {
			WL_ERROR(("%s: phy_register_ioct_all failed\n", __FUNCTION__));
			goto fail;
		}

		prev_pi = pi;
	}

	return BCME_OK;

fail:
	return err;
}

static uint
wlc_numd11coreunits(si_t *sih)
{
#if defined(WLRSDB) && defined(WLRSDB_DISABLED)
	/* If RSDB functionality is compiled out, then ignore any D11 cores
	 * beyond the first. Used in norsdb build variants for rsdb chip.
	 */
	return 1;
#else
	return si_numd11coreunits(sih);
#endif /* defined(WLRSDB) && defined(WLRSDB_DISABLED) */
}

#if defined(WL_EAP_BOARD_RF_5G_FILTER)
/*
 * WL_EAP - wlc_bmac_board_analog_rf_filters
 *  Set MAC GPIO for on-board 5G analog band-pass filtering.
 *  Also used in PHY for TSSI definitions.
 *  Currently used on the BCM949408EAP reference design.
 */
static void wlc_bmac_board_analog_rf_filters(wlc_hw_info_t *wlc_hw)
{
#if defined(BCA_HNDROUTER) /* BCM949408EAP */
	/* Configuration for the BCM949408EAP reference design.
	 * BCM4366_D11AC5G_ID port uses GPIO15 to set filter.
	 *	Filter disable (GPIO15=0), sets 5G full-band.
	 *	Filter enable (GPIO15=1), filters low 5G channels.
	 * BCM4366_D11AC_ID port has fixed 5G filtering of high
	 *	channels.
	 * Algorithm:
	 *	// check for 949408 board
	 *	if nvram get boardid == 94908REF_XPHY
	 *		// check for filter config
	 *		if nvram get 5gsplitband != 0
	 *			set filter on (split-band mode)
	 *		else
	 *			set filter off (full-band mode)
	 */
	#define RF_FILTER_BOARD_NAME "949408EAP_XPHY"
	#define GPIO_15 0x8000
	#define GPIO_15_BYPASS_FILTER 0x8000
	#define GPIO_15_THROUGH_FILTER 0x0000
	char *boardname;
	if ((boardname = getvar(wlc_hw->vars, "boardid")) != NULL) {
		printk("Filter check for board '%s'\n", boardname);
		if (!strncmp(boardname, RF_FILTER_BOARD_NAME, strlen(RF_FILTER_BOARD_NAME))) {
			/* Get desired filter configuration from NVRAM */
			int filter = getintvar(wlc_hw->vars, "5gsplitband");
			/* 5G-only port has filter options */
			if (IS_SINGLEBAND_5G(wlc_hw->deviceid, wlc_hw->phy_cap)) {
				printk("gpio in = %x\n", (unsigned int)si_gpioin(wlc_hw->sih));
				/* give the control to chip common */
				si_gpiocontrol(wlc_hw->sih, GPIO_15, GPIO_15_THROUGH_FILTER,
					GPIO_DRV_PRIORITY);
				// set gpios
				if (filter) {
					printk("Device is split mode: Enabling filter\n");
					/* drive the output to 0 */
					si_gpioout(wlc_hw->sih, GPIO_15, GPIO_15_THROUGH_FILTER,
						GPIO_DRV_PRIORITY);

					/* 94908REF_XPHY 5HL port's filter=on blocks UNII-1+2A */
					wlc_hw->board5gfilter = (BOARD_5G_FILTER_BLOCKS_UNII1 +
						BOARD_5G_FILTER_BLOCKS_UNII2A);
				} else {
					printk("Device is standard mode: Bypassing filter\n");
					/* drive the output to 1 */
					si_gpioout(wlc_hw->sih, GPIO_15, GPIO_15_BYPASS_FILTER,
						GPIO_DRV_PRIORITY);

					/* 94908REF_XPHY 5HL port's filter=off allows full-band */
					wlc_hw->board5gfilter = BOARD_5G_FILTER_BLOCKS_NONE;
				}
				/* set output enable */
				si_gpioouten(wlc_hw->sih, GPIO_15, GPIO_15_BYPASS_FILTER,
					GPIO_DRV_PRIORITY);
				/* read back the gpio */
				printk("gpio in = %x\n", (unsigned int)si_gpioin(wlc_hw->sih));
			} else {
				/* MC/dual-band port has a fixed lower-band 5G filter */
				printk("Device is NOT single band\n");

				/* 94908REF_XPHY MC port's permanent filter blocks UNII-2C+3 */
				wlc_hw->board5gfilter = (BOARD_5G_FILTER_BLOCKS_UNII2C +
					BOARD_5G_FILTER_BLOCKS_UNII3);
			}
		}
	}
#else /* !BCA_HNDROUTER */
	/* Currently, no other boards support split-band 5G filtering */
	BCM_REFERENCE(wlc_hw);
#endif /* BCA_HNDROUTER */
}
#endif /* WL_EAP_BOARD_RF_5G_FILTER */

/**
 * low level attach
 *    run backplane attach, init nvram
 *    run phy attach
 *    initialize software state for each core and band
 *    put the whole chip in reset(driver down state), no clock
 */
int
BCMATTACHFN(wlc_bmac_attach)(wlc_info_t *wlc, uint16 vendor, uint16 device, uint unit,
	bool piomode, osl_t *osh, volatile void *regsva, uint bustype, void *btparam, uint macunit)
{
	wlc_hw_info_t *wlc_hw;
	char *macaddr = NULL;
	const char *rsdb_mode;
	char *vars;
	uint16 devid;
	uint err = 0;
	uint j;
	bool wme = FALSE;
	shared_phy_params_t sha_params;
	uint xmtsize;
	uint16 *ptr_xmtfifo = NULL;
	uint16 *xmtfifo_sz_dummy = NULL;
#ifdef GPIO_TXINHIBIT
	uint32 gpio_pullup_en;
#endif // endif

	BCM_REFERENCE(regsva);
	BCM_REFERENCE(btparam);

	WL_TRACE(("wl%d: %s: vendor 0x%x device 0x%x\n", unit, __FUNCTION__, vendor, device));

	if ((wlc_hw = wlc_hw_attach(wlc, osh, unit, &err, macunit)) == NULL)
		goto fail;

	wlc->hw = wlc_hw;
	wlc_hw->phy_cap = wlc->phy_cap;
	wlc_hw->pkteng_status = PKTENG_IDLE;
	wlc_hw->pkteng_async->timer_cb_dur = TIMER_INTERVAL_PKTENG;
	/* initialize header conversion mode */
	wlc_hw->hdrconv_mode = HDR_CONV();

	wlc->cmn->num_d11_cores = wlc_numd11coreunits(wlc->pub->sih);
	wlc_hw->num_mac_chains = si_numcoreunits(wlc->pub->sih, D11_CORE_ID);
#ifdef WLRSDB
	/* Update the pub state saying we are an RSDB capable chip. */
#ifdef WL_DUALNIC_RSDB
	wlc->pub->cmn->_isrsdb = TRUE;
#else
	if (wlc->cmn->num_d11_cores > 1) {
		wlc->pub->cmn->_isrsdb = TRUE;
	}
#endif /* WL_DUALNIC_RSDB */
#endif /* WLRSDB */
#ifdef WME
	wme = TRUE;
#endif /* WME */

	wlc_hw->_piomode = piomode;

#if defined(SRHWVSDB) && !defined(SRHWVSDB_DISABLED)
	wlc->pub->_wlsrvsdb = TRUE; /* from this point on, macro SRHWVSDB_ENAB() may be used */
#endif /* SRHWVSDB SRHWVSDB_DISABLED */

	/* si_attach is done much more earlier in the attach path and we dont
	 * expect it to be null.
	 */
	wlc_hw->sih = wlc->pub->sih;
	wlc_hw->vars = wlc->pub->vars;
	wlc_hw->vars_size = wlc->pub->vars_size;
	ASSERT(wlc_hw->sih);
	vars = wlc_hw->vars;
#ifdef GPIO_TXINHIBIT
	/* This is for GPIO based tx_inhibit functionality, not coex. */
	/* Pullup tx_inhibit gpio(SWWLAN-109270) */
	gpio_pullup_en = (uint32)getvar(NULL, rstr_gpio_pullup_en);
	si_gpiopull(wlc_hw->sih, GPIO_PULLUP, gpio_pullup_en, gpio_pullup_en);
#endif // endif

	/* set bar0 window to point at D11 core */
	wlc_hw->regs = (d11regs_t *)si_setcore(wlc_hw->sih, D11_CORE_ID, wlc_hw->macunit);
	ASSERT(wlc_hw->regs != NULL);

	wlc->regs = wlc_hw->regs;

	/* Save the corerev */
	wlc_hw->corerev = si_corerev(wlc_hw->sih);
	wlc_hw->corerev_minor = (uint8)si_corerev_minor(wlc_hw->sih);

	wlc_bmac_autod11_shm_upd(wlc_hw, D11_IF_SHM_STD);

	err = d11regs_select_offsets_tbl(&wlc_hw->regoffsets, wlc_hw->corerev);
	if (err) {
		WL_ERROR(("wl%d: %s: failed, regs for rev not found\n", unit, __FUNCTION__));
		err = 35;
		goto fail;
	}
	wlc->regoffsets = wlc_hw->regoffsets;

	/*
	 * wlc_bmac_rsdb_cap() will return FALSE if dualmac_rsdb is set.
	 * use this because machwcap1 is not available at this point.
	 */
	if (BCM4347_CHIP(wlc_hw->sih->chip))
		wlc->cmn->dualmac_rsdb = TRUE;

#if defined(WL_TXS_LOG)
	wlc_hw->txs_hist = wlc_bmac_txs_hist_attach(wlc_hw);
	if (wlc_hw->txs_hist == NULL) {
		WL_ERROR(("wl%d: %s: wlc_bmac_txs_hist_attach failed\n",
			unit, __FUNCTION__));
		err = 34;
		goto fail;
	}
#endif /* WL_TXS_LOG */

	wlc_hw->rx_stall = wlc_bmac_rx_stall_attach(wlc_hw);
	if (wlc_hw->rx_stall == NULL) {
		WL_ERROR(("wl%d: %s: wlc_bmac_rx_stall_attach failed\n",
		          unit, __FUNCTION__));
		err = 33;
		goto fail;
	}

	/* populate wlc_hw_info_t with default values  */
	wlc_bmac_info_init(wlc_hw);

#if defined(__mips__) || defined(BCM47XX_CA9)
	if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
	    (CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
	    (CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) ||
	    (CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID)) {
		extern int do_4360_pcie2_war;
		char *var;
		uint8 tlclkwar = 0;
		/* changing the avb vcoFreq as 510M (from default: 500M) */
		/* Tl clk 127.5Mhz */
		if ((var = getvar(NULL, rstr_wl_tlclk)))
			tlclkwar = (uint8)bcm_strtoul(var, NULL, 16);

		if (tlclkwar == 1) {
			if (wlc_bmac_4360_pcie2_war(wlc_hw, 510) != BCME_OK) {
				err = 31;
				goto fail;
			}
		}
		else if (tlclkwar == 2)
			do_4360_pcie2_war = 1;
	}
#endif /* defined(__mips__) || defined(BCM47XX_CA9) */

	/*
	 * Get vendid/devid nvram overwrites, which could be different
	 * than those the BIOS recognizes for devices on PCMCIA_BUS,
	 * SDIO_BUS, and SROMless devices on PCI_BUS.
	 */
#ifdef BCMBUSTYPE
	bustype = BCMBUSTYPE;
#endif // endif
	if (bustype != SI_BUS)
	{
	char *var;

	/* set vars table accesser for multi-slice chips */
	if ((!wlc_bmac_rsdb_cap(wlc_hw)) &&
		(wlc->cmn->num_d11_cores > 1) &&
		(wlc_hw->macunit == 1)) {
		strncpy(wlc_hw->vars_table_accessor, "slice/1/",
			sizeof(wlc_hw->vars_table_accessor));
		wlc_hw->vars_table_accessor[sizeof(wlc_hw->vars_table_accessor)-1] = '\0';
		printf("DEVID Debug: macunit is: %x\n", wlc_hw->macunit);
	}

	if ((var = getvar(vars, rstr_vendid))) {
		vendor = (uint16)bcm_strtoul(var, NULL, 0);
		WL_ERROR(("Overriding vendor id = 0x%x\n", vendor));
	}

	devid = (uint16)getintvar_slicespecific(wlc_hw, vars, rstr_devid);
	if (devid) {
		device = devid;
		WL_ERROR(("Overriding device id = 0x%x\n", device));
	}

	if (BCM43602_CHIP(wlc_hw->sih->chip) && device == BCM43602_CHIP_ID) {
		device = BCM43602_D11AC_ID;
	}

	/* verify again the device is supported */
	if (!wlc_chipmatch(vendor, device)) {
		WL_ERROR(("wl%d: %s: Unsupported vendor/device (0x%x/0x%x)\n",
		          unit, __FUNCTION__, vendor, device));
		err = 12;
		goto fail;
	}
	}

	wlc_hw->vendorid = vendor;
	wlc_hw->deviceid = device;

	/* XXX 4360:11AC:WES unprogrammed BCM4360 has a dev id 0x4360 which is actually
	 * BCM4330_D11N_ID, a single band dev id
	 * Remove this when we get QT to have OTP to set proper DevID
	 */
	if ((ISSIM_ENAB(wlc_hw->sih)) &&
	    ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
	     (CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
	     (CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) ||
	     (CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID))) {
		wlc_hw->deviceid = (CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ?
			BCM4352_D11AC_ID : BCM4360_D11AC_ID;
	}

	wlc_hw->band = wlc_hw->bandstate[IS_SINGLEBAND_5G(wlc_hw->deviceid, wlc_hw->phy_cap) ?
		BAND_5G_INDEX : BAND_2G_INDEX];
	/* Monolithic driver gets wlc->band and band members initialized in wlc_bmac_attach() */
	wlc->band = wlc->bandstate[IS_SINGLEBAND_5G(wlc_hw->deviceid, wlc_hw->phy_cap) ?
		BAND_5G_INDEX : BAND_2G_INDEX];
#ifdef BCMPCIDEV
	wlc_hw->pcieregs = (sbpcieregs_t *)((volatile uchar *)regsva + PCI_16KB0_PCIREGS_OFFSET);
#endif // endif

	/* validate chip, chiprev and corerev */
	if (!wlc_isgoodchip(wlc_hw)) {
		err = 13;
		goto fail;
	}

#ifdef BCMHWA
	{
		uint coreidx;
		uint32 mac_axi_addr, mac_base_addr;
#ifdef DONGLEBUILD
		volatile void *hwa_regs;
#endif /* DONGLEBUILD */

		ASSERT(macunit == 0);
		/* Attach HWA device globally before wlc_attach */
		coreidx = si_coreidx(wlc_hw->sih);
#ifdef DONGLEBUILD
		hwa_regs = si_setcore(wlc->pub->sih, HWA_CORE_ID, macunit);
		if (hwa_regs == NULL)
			hwa_dev = (struct hwa_dev*)NULL;
		else
			hwa_dev = hwa_attach(wlc, wlc_hw->deviceid, osh, hwa_regs,
				BUSTYPE(wlc->pub->sih->bustype));
#else /* !DONGLEBUILD */
		hwa_dev = hwa_attach(wlc, wlc_hw->deviceid, osh, regsva,
			BUSTYPE(wlc->pub->sih->bustype));
#endif /* !DONGLEBUILD */
		si_setcoreidx(wlc_hw->sih, coreidx);

		if (hwa_dev == NULL) {
			WL_ERROR(("wl%d: wlc_bmac_attach: hwa_attach attach failed.\n", unit));
			err = 40;
			goto fail;
		}

#if defined(HWA_PKT_MACRO)
		/* Allocate hwa_txpkt dynamic variable */
		wlc->hwa_txpkt = (wlc_hwa_txpkt_t*)MALLOCZ(wlc->osh, sizeof(*wlc->hwa_txpkt));
		if (wlc->hwa_txpkt == NULL) {
			WL_ERROR(("wl%d: wlc_bmac_attach: hwa_txpkt alloc failed.\n", unit));
			err = 41;
			goto fail;
		}
#endif /* HWA_PKT_MACRO */

		/* Configure HWA with mac register and base addresses */
		hwa_mac_config(HWA_HC_HIN_REGS, 0, wlc_hw->regs, 0U);

		/* set the MAC_AXI_BASE */
		HWA_RXDATA_EXPR({
		   mac_axi_addr = si_addrspace(wlc_hw->sih, CORE_SLAVE_PORT_1, CORE_BASE_ADDR_0);
		   hwa_mac_config(HWA_MAC_AXI_BASE, 0, NULL, mac_axi_addr);
		});

		/* set the MAC_BASE_ADDR */
		mac_base_addr = si_addrspace(wlc_hw->sih, CORE_SLAVE_PORT_0, CORE_BASE_ADDR_0);
		hwa_mac_config(HWA_MAC_BASE_ADDR, 0, NULL, mac_base_addr);

		WL_TRACE(("TRDEBUG =====  %s: mac_axi_addr = 0x%x, mac_base_addr = 0x%x",
				__FUNCTION__, mac_axi_addr, mac_base_addr));

#ifdef DONGLEBUILD
		hwa_mac_config(HWA_MAC_FRMTXSTATUS, 0, NULL, D11_FrmTxStatus_OFFSET(wlc_hw));
		hwa_mac_config(HWA_MAC_IND_XMTPTR, 0, NULL, D11_inddma_OFFSET(wlc_hw) +
			OFFSETOF(dma64regs_t, ptr));
		hwa_mac_config(HWA_MAC_IND_QSEL, 0, NULL, D11_IndAQMQSel_OFFSET(wlc_hw));
#endif // endif
	}
#endif /* BCMHWA */

	wlc_hw->machwcap1 = R_REG(wlc_hw->osh, D11_MacHWCap1(wlc_hw));

	/* In case of RSDB chip do the reset of both the cores
	 * together at the beginning, before detecting whether it is a
	 * RSDB mac or dual MAC design. Read the capability register
	 * to check whether the MAC is rsdb capable or not and then
	 * onwards use core specific reset for non-rsdb mac device
	 */
	if (wlc_hw->macunit == 0) {
		wlc_bmac_init_core_reset_disable_fn(wlc_hw);
		/* make sure the core is up before accessing registers */
		if (!si_iscoreup(wlc_hw->sih)) {
			wlc_bmac_core_reset(wlc_hw, 0, 0);
		}

		/* Check the mac capability */
		wlc_hw->num_mac_chains = 1 + ((wlc_hw->machwcap1 & MCAP1_NUMMACCHAINS)
					       >> MCAP1_NUMMACCHAINS_SHIFT);
		wlc_bmac_init_core_reset_disable_fn(wlc_hw);
	}
#if defined(WLRSDB) && !defined(WLRSDB_DISABLED)
	else {
		wlc_info_t *other_wlc = wlc_rsdb_get_other_wlc(wlc_hw->wlc);
		wlc_hw->num_mac_chains = other_wlc->hw->num_mac_chains;
		wlc_bmac_init_core_reset_disable_fn(wlc_hw);
	}
#endif /* WLRSDB && WLRSDB_DISABLED */

	if ((wlc->cmn->num_d11_cores > 1) &&
		(wlc_hw->num_mac_chains == 1)) {
		wlc->cmn->dualmac_rsdb = TRUE;
#ifdef WL_OBJ_REGISTRY
		/*
		 * XXX: This is required as there are different phy's for
		 * 4364. Need to add checks for different phy type if there is
		 * a chip with same phy versions for both cores
		 *
		 * Bandstate sharing is handled here because bmac_attach happens after
		 * wlc_attach_alloc()
		 */
		if (!RSDB_CMN_BANDSTATE_ENAB(wlc->pub)) {
			obj_registry_disable(wlc->objr, OBJR_WLC_BANDSTATE);
		}
		obj_registry_disable(wlc->objr, OBJR_ACPHY_SROM_INFO);
#endif /* WL_OBJ_REGISTRY */
	}

	/* BMC params selection */
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		wlc_bmac_bmc_params(wlc_hw);
	}

	/* initialize power control registers */
	si_clkctl_init(wlc_hw->sih);

	si_pcie_ltr_war(wlc_hw->sih);

	/* 'ltr' advertizes to the PCIe host how long the device takes to power up or down */
	if ((BCM4350_CHIP(wlc_hw->sih->chip) &&
	     CST4350_IFC_MODE(wlc_hw->sih->chipst) == CST4350_IFC_MODE_PCIE) ||
	     BCM43602_CHIP(wlc_hw->sih->chip)) { /* 43602 is PCIe only */
		si_pcieltrenable(wlc_hw->sih, 1, 1);
	}

	if ((BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) &&
	    (BUSCORETYPE(wlc_hw->sih->buscoretype) == PCIE2_CORE_ID) &&
	    (wlc_hw->sih->buscorerev <= 4)) {
		si_pcieobffenable(wlc_hw->sih, 1, 0);
	}

	/* request fastclock and force fastclock for the rest of attach
	 * bring the d11 core out of reset.
	 *   For PMU chips, the first wlc_clkctl_clk is no-op since core-clk is still FALSE;
	 *   But it will be called again inside wlc_corereset, after d11 is out of reset.
	 */
	wlc_clkctl_clk(wlc_hw, CLK_FAST);

	/* Change the oob settings to route the d11 core1 interrupts through DDR interrupt line.
	 * Required because the original interrupt line for D11 core1 is not connected to the CA7
	 * GIC. So, disabling the DDR interrupts and using that line for D11_Core1 interrupts.
	 */
	if (BCM53573_CHIP(wlc_hw->sih->chip) && (wlc_hw->macunit == 1)) {
		si_config_53573_d11_oob(wlc_hw->sih, D11_CORE_ID);
	}

#if defined(WL_PSMX)
	wlc->pub->_psmx = wlc_psmx_hw_supported(wlc_hw->corerev);
#endif /* WL_PSMX */

#if defined(WL_PSMR1)
	wlc_hw->_psmr1 = wlc_psmr1_hw_supported(wlc_hw->corerev);
#endif /* WL_PSMR1 */

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	wlc->pub->_sts_rxen = D11REV_IS(wlc_hw->corerev, 129);
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

#if defined(WLC_OFFLOADS_RXSTS)
	/* rev130/131 has a separate channel for phyrxsts, just like rev129, but has no rxfifo-3 */
	wlc->pub->_sts_offld_rxen = D11REV_GE(wlc_hw->corerev, 130);
#endif /* WLC_OFFLOADS_RXSTS */

	wlc_bmac_corereset(wlc_hw, WLC_USE_COREFLAGS);

	if (!wlc_bmac_validate_chip_access(wlc_hw)) {
		WL_ERROR(("wl%d: %s: validate_chip_access failed\n", unit, __FUNCTION__));
		err = 14;
		goto fail;
	}

	/* get the board rev, used just below */
	j = getintvar(vars, rstr_boardrev);
	/* promote srom boardrev of 0xFF to 1 */
	if (j == BOARDREV_PROMOTABLE)
		j = BOARDREV_PROMOTED;
	wlc_hw->boardrev = (uint16)j;
	if (!wlc_validboardtype(wlc_hw)) {
		WL_ERROR(("wl%d: %s: Unsupported Broadcom board type (0x%x)"
			" or revision level (0x%x)\n",
			unit, __FUNCTION__, wlc_hw->sih->boardtype, wlc_hw->boardrev));
		err = 15;
		goto fail;
	}

	wlc_hw->sromrev = (uint8)getintvar(vars, rstr_sromrev);
	wlc_hw->boardflags = (uint32)getintvar_slicespecific(wlc_hw, vars, rstr_boardflags);
	wlc_hw->boardflags2 = (uint32)getintvar_slicespecific(wlc_hw, vars, rstr_boardflags2);
	if (wlc_hw->sromrev >= 12)
		wlc_hw->boardflags4 =
			(uint32)getintvar_slicespecific(wlc_hw, vars, rstr_boardflags4);
	wlc_hw->antswctl2g = (uint8)getintvar(vars, rstr_antswctl2g);
	wlc_hw->antswctl5g = (uint8)getintvar(vars, rstr_antswctl5g);

	/* some branded-boards boardflags srom programmed incorrectly */
	if (wlc_hw->sih->boardvendor == VENDOR_APPLE) {
		if ((wlc_hw->sih->boardtype == 0x4e) && (wlc_hw->boardrev >= 0x41)) {
			wlc_hw->boardflags |= BFL_PACTRL;
		}
	}

#if defined(WL_EAP_BOARD_RF_5G_FILTER)
	/* Initialize board filter configuration */
	wlc_hw->board5gfilter = BOARD_5G_FILTER_ABSENT;
	/* Set 5G analog filtering if required (e.g., BCM949408) */
	wlc_bmac_board_analog_rf_filters(wlc_hw);
#endif /* WL_EAP_BOARD_RF_5G_FILTER */

	if (wlc_hw->boardflags & BFL_NOPLLDOWN)
		wlc_bmac_pllreq(wlc_hw, TRUE, WLC_PLLREQ_SHARED);

#if defined(DBAND)
	/* check device id(srom, nvram etc.) to set bands */
	if ((wlc_hw->deviceid == BCM6362_D11N_ID) ||
	    (wlc_hw->deviceid == BCM4360_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM4352_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM43452_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM43602_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM4352_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM43569_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM4359_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM4369_D11AX_ID) ||
	    (wlc_hw->deviceid == BCM4377_D11AX_ID) ||
	    (wlc_hw->deviceid == BCM4347_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM6878_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM4361_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM4365_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM43684_D11AX_ID) ||
	    (wlc_hw->deviceid == EMBEDDED_2x2AX_ID) ||
	    (wlc_hw->deviceid == BCM4366_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM53573_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM47189_D11AC_ID) ||
	    (wlc_hw->deviceid == BCM6710_CHIP_ID) ||
	    (wlc_hw->deviceid == BCM6710_D11AX_ID)) {
		/* Dualband boards */
		wlc_hw->_nbands = 2;
	} else
#endif /* DBAND */
		wlc_hw->_nbands = 1;

#if NCONF
	if (CHIPID(wlc_hw->sih->chip) == BCM43217_CHIP_ID) {
		wlc_hw->_nbands = 1;
	}
#endif /* NCONF */

	/* BMAC_NOTE: remove init of pub values when wlc_attach() unconditionally does the
	 * init of these values
	 */
	wlc->vendorid = wlc_hw->vendorid;
	wlc->deviceid = wlc_hw->deviceid;
	wlc->pub->sih = wlc_hw->sih;
	wlc->pub->corerev = wlc_hw->corerev;
	wlc->pub->sromrev = wlc_hw->sromrev;
	wlc->pub->boardrev = wlc_hw->boardrev;
	wlc->pub->boardflags = wlc_hw->boardflags;
	wlc->pub->boardflags2 = wlc_hw->boardflags2;
	if (wlc_hw->sromrev >= 12)
		wlc->pub->boardflags4 = wlc_hw->boardflags4;
	wlc->pub->_nbands = wlc_hw->_nbands;
	wlc->pub->corerev_minor = wlc_hw->corerev_minor;
	if (D11REV_IS(wlc->pub->corerev, 129)) {
		wlc->pub->corerev_minor = wlc_hw->sih->otpflag;
	} else if (D11REV_IS(wlc->pub->corerev, 130)) {
		wlc->pub->corerev_minor =
			R_REG(wlc_hw->osh, D11_Workaround(wlc_hw)) >> WAR_COREVMINOR_SHIFT;
	}

	WL_ERROR(("wlc_bmac_attach, deviceid 0x%x nbands %d\n", wlc_hw->deviceid, wlc_hw->_nbands));

#ifdef PKTC
	/* pktc, pktx_tx are enabled by default. If there is no support to chain packets
	 * coming into wl driver it is recommended to turn off pktc_tx to help with amsdu
	 * aggregation
	 */
	wlc->pub->_pktc = wlc->pub->_pktc_tx = (getintvar(vars, "pktc_disable") == 0) &&
		(getintvar(vars, "ctf_disable") == 0);
#endif /* PKTC */

#if defined(PKTC_DONGLE)
	wlc->pub->_pktc = wlc->pub->_pktc_tx = TRUE;
#endif // endif

#if defined(PKTC) || defined(PKTC_DONGLE)
	WL_ERROR(("wlc_bmac_attach, pktc<%d> pktc_tx<%d> \n", wlc->pub->_pktc, wlc->pub->_pktc_tx));
#endif // endif

	wlc_hw->physhim = wlc_phy_shim_attach(wlc_hw, wlc->wl, wlc);

	if (wlc_hw->physhim == NULL) {
		WL_ERROR(("wl%d: %s: wlc_phy_shim_attach failed\n", unit, __FUNCTION__));
		err = 25;
		goto fail;
	}

	/* pass all the parameters to wlc_phy_shared_attach in one struct */
	sha_params.osh = osh;
	sha_params.sih = wlc_hw->sih;
	sha_params.physhim = wlc_hw->physhim;
	sha_params.unit = unit;
	sha_params.corerev = wlc_hw->corerev;
	sha_params.vars = vars;
	sha_params.vid = wlc_hw->vendorid;
	sha_params.did = wlc_hw->deviceid;
	sha_params.chip = wlc_hw->sih->chip;
	sha_params.chiprev = wlc_hw->sih->chiprev;
	sha_params.chippkg = wlc_hw->sih->chippkg;
	sha_params.sromrev = wlc_hw->sromrev;
	sha_params.boardtype = wlc_hw->sih->boardtype;
	sha_params.boardrev = wlc_hw->boardrev;
	sha_params.boardvendor = wlc_hw->sih->boardvendor;
	sha_params.boardflags = wlc_hw->boardflags;
	sha_params.boardflags2 = wlc_hw->boardflags2;
	sha_params.boardflags4 = wlc_hw->boardflags4;
	sha_params.bustype = wlc_hw->sih->bustype;
	sha_params.buscorerev = wlc_hw->sih->buscorerev;
#if defined(WL_EAP_BOARD_RF_5G_FILTER)
	/* Pass filter config to PHY for TSSI optimizations */
	sha_params.board5gfilter = wlc_hw->board5gfilter;
#endif /* WL_EAP_BOARD_RF_5G_FILTER */
	strncpy(sha_params.vars_table_accessor, wlc_hw->vars_table_accessor,
		sizeof(sha_params.vars_table_accessor));
	sha_params.vars_table_accessor[sizeof(sha_params.vars_table_accessor)-1] = '\0';
	/* alloc and save pointer to shared phy state area */
	wlc_hw->phy_sh = wlc_phy_shared_attach(&sha_params);
	if (!wlc_hw->phy_sh) {
		err = 16;
		goto fail;
	}

	/* use different hw rx offset for different core revids, must be done before dma_attach */
	/* the room from 0 to this offset is taken by various rx headers (phy+mac+psm) */
	wlc->d11rxoff = D11_RXHDR_LEN(wlc_hw->corerev);
#ifdef HW_HDR_CONVERSION
	if (SPLIT_RXMODE4())
		wlc->datafiforxoff = WL_DATA_FIFO_OFFSET;
	else
#endif // endif
		wlc->datafiforxoff = wlc->d11rxoff;
	ASSERT(wlc->d11rxoff % 2 == 0);
	/* the room from 0 to this offset is taken by various rx headers (above+SW) */
	wlc->hwrxoff = WL_RXHDR_LEN(wlc_hw->corerev);

	/* for corerev < 40 ucode expects a non 4 byte aligned hwrxoff */
	if (D11REV_LT(wlc_hw->corerev, 40)) {
		ASSERT(wlc->hwrxoff % 4 != 0);
	}

	ASSERT(wlc->hwrxoff % 2 == 0);

	WL_ERROR(("RXOFFSET Programming : datafiforxoff %d d11rxoff %d hwrxoff %d \n",
		wlc->datafiforxoff, wlc->d11rxoff, wlc->hwrxoff));

	wlc->hwrxoff_pktget = (wlc->hwrxoff % 4) ?  wlc->hwrxoff : (wlc->hwrxoff + 2);

#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
	if (WLC_CT_HW_SUPPORTED(wlc)) {
		if (D11REV_GE(wlc_hw->corerev, 128)) {
			/* CTDMA is mandatory after rev128 */
			wlc->_dma_ct = TRUE;
		} else if (!wlc->_dma_ct) {
			/* Check var if we need to enable CT */
			wlc->_dma_ct = (getintvar(vars, "ctdma") == 1);

			/* enable CTDMA in dongle mode if there is no ctdma specified in NVRAM */
			if ((BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) &&
			    D11REV_IS(wlc_hw->corerev, 65) && (getvar(NULL, "ctdma") == NULL)) {
				wlc->_dma_ct = TRUE;
			}
		}
	}

	/* preconfigured ctdma with NVRAM or param */
	wlc->_dma_ct_flags = wlc->_dma_ct? DMA_CT_PRECONFIGURED : 0;
#endif /* BCM_DMA_CT && !BCM_DMA_CT_DISABLED */

	wlc_hw_update_nfifo(wlc_hw);

	wlc_hw->vcoFreq_4360_pcie2_war = 510; /* Default Value */

	wlc_hw->num_mac_chains =
		1 + ((wlc_hw->machwcap1 & MCAP1_NUMMACCHAINS) >> MCAP1_NUMMACCHAINS_SHIFT);

	/* initialize software state for each core and band */
	for (j = 0; j < NBANDS_HW(wlc_hw); j++) {
		/*
		 * band0 is always 2.4Ghz
		 * band1, if present, is 5Ghz
		 */

		/* So if this is a single band 11a card, use band 1 */
		if (IS_SINGLEBAND_5G(wlc_hw->deviceid, wlc_hw->phy_cap))
			j = BAND_5G_INDEX;

		wlc_setxband(wlc_hw, j);

		wlc_hw->band->bandunit = j;
		wlc_hw->band->bandtype = j ? WLC_BAND_5G : WLC_BAND_2G;
		/* Monolithic driver gets wlc->band and band members
		 * initialized in wlc_bmac_attach()
		 */
		wlc->band->bandunit = j;
		wlc->band->bandtype = j ? WLC_BAND_5G : WLC_BAND_2G;
		wlc->core->coreidx = si_coreidx(wlc_hw->sih);

		wlc_hw->machwcap = R_REG(osh, D11_MacCapability(wlc));
		/* PR78990 WAR for broken HW TKIP in some revs
		 * XXX 4360A0: HW TKIP also broken, 4360B0 not sure
		 */
		if ((D11REV_IS(wlc_hw->corerev, 30)) ||
		    (D11REV_IS(wlc_hw->corerev, 42)) ||
		    (D11REV_IS(wlc_hw->corerev, 48)) ||
		    (D11REV_IS(wlc_hw->corerev, 49))) {
		     WL_ERROR(("%s: Disabling HW TKIP!\n", __FUNCTION__));
			 wlc_hw->machwcap &= ~MCAP_TKIPMIC;
		}

		ptr_xmtfifo = get_xmtfifo_sz(&xmtsize);
		xmtfifo_sz_dummy = get_xmtfifo_sz_dummy();
		/* init tx fifo size */

		if (D11REV_GE(wlc_hw->corerev, 48) && D11REV_LT(wlc_hw->corerev, 64)) {
			/* this is just a way to reduce memory footprint */
			wlc_hw->xmtfifo_sz = xmtfifo_sz_dummy;
		} else if (D11REV_LT(wlc_hw->corerev, 40)) {
			/* init tx fifo size */
			ASSERT((wlc_hw->corerev - XMTFIFOTBL_STARTREV) < xmtsize);
			wlc_hw->xmtfifo_sz = (ptr_xmtfifo +
				((wlc_hw->corerev - XMTFIFOTBL_STARTREV) * NFIFO_LEGACY));
		} else {
			wlc_hw->xmtfifo_sz =
				(ptr_xmtfifo + ((40 - XMTFIFOTBL_STARTREV) * NFIFO_LEGACY));
		}

		/* Get a phy for this band */
		WL_NONE(("wl%d: %s: bandunit %d bandtype %d coreidx %d\n", unit,
		         __FUNCTION__, wlc_hw->band->bandunit, wlc_hw->band->bandtype,
		         wlc->core->coreidx));
		if ((wlc_hw->band->pi = (wlc_phy_t *)
		     phy_module_attach(wlc_hw->phy_sh, (void *)(uintptr)wlc_hw->regs,
		                wlc_hw->band->bandtype, vars)) == NULL) {
			WL_ERROR(("wl%d: %s: phy_module_attach failed\n", unit, __FUNCTION__));
			err = 17;
			goto fail;
		}
		/* it's called again after phy module attach as bmac module attach don't have pi */
		wlc_phy_set_shmdefs(wlc_hw->band->pi, wlc_hw->shmdefs);

		phy_machwcap_set(wlc_hw->band->pi, wlc_hw->machwcap);

		phy_utils_get_phyversion((phy_info_t *)wlc_hw->band->pi, &wlc_hw->band->phytype,
			&wlc_hw->band->phyrev, &wlc_hw->band->radioid, &wlc_hw->band->radiorev,
			&wlc_hw->band->phy_minor_rev);

		wlc_hw->band->core_flags = phy_utils_get_coreflags((phy_info_t *)wlc_hw->band->pi);

		/* verify good phy_type & supported phy revision */
		if (WLCISNPHY(wlc_hw->band)) {
			if (NCONF_HAS(wlc_hw->band->phyrev))
				goto good_phy;
			else
				goto bad_phy;
		} else if (WLCISLCN20PHY(wlc_hw->band)) {
			if (LCN20CONF_HAS(wlc_hw->band->phyrev))
				goto good_phy;
			else
				goto bad_phy;
		} else if (WLCISACPHY(wlc_hw->band)) {
			if (ACCONF_HAS(wlc_hw->band->phyrev))
				goto good_phy;
			else
				goto bad_phy;
		} else {
bad_phy:
			WL_ERROR(("wl%d: %s: unsupported phy type/rev (%d/%d)\n",
			          unit, __FUNCTION__, wlc_hw->band->phytype,
			          wlc_hw->band->phyrev));
			err = 18;
			goto fail;
		}

good_phy:
		/* XXX Chiprev of 6878a1 is same as 6878a0, a hardware issue.
		 * Doing a software workaroud here based on radiorev.
		 */
		if (BCM6878_CHIP(wlc_hw->sih->chip) && ((wlc_hw->band->radiorev) == 1) &&
			(wlc_hw->sih->chiprev == 0)) {
			wlc_hw->sih->chiprev = 1;
		}

		WL_ERROR(("wl%d: %s: chiprev %d corerev %d "
		          "cccap 0x%x maccap 0x%x band %sG, phy_type %d phy_rev %d\n",
		          unit, __FUNCTION__, CHIPREV(wlc_hw->sih->chiprev),
		          wlc_hw->corerev, wlc_hw->sih->cccaps, wlc_hw->machwcap,
		          BAND_2G(wlc_hw->band->bandtype) ? "2.4" : "5",
		          wlc_hw->band->phytype, wlc_hw->band->phyrev));

		/* Monolithic driver gets wlc->band and band members
		 * initialized in wlc_bmac_attach()
		 */
		/* Initialize both wlc->pi and wlc->bandinst->pi */
		wlc->pi = wlc->bandinst[wlc->band->bandunit]->pi = wlc_hw->band->pi;
		wlc->band->phytype = wlc_hw->band->phytype;
		wlc->band->phyrev = wlc_hw->band->phyrev;
		wlc->band->radioid = wlc_hw->band->radioid;
		wlc->band->radiorev = wlc_hw->band->radiorev;
		wlc->band->phy_minor_rev = wlc_hw->band->phy_minor_rev;

		/* default contention windows size limits */
		wlc_hw->band->CWmin = APHY_CWMIN;
		wlc_hw->band->CWmax = PHY_CWMAX;

		/* initial ucode host flags */
		wlc_mhfdef(wlc_hw, wlc_hw->band->mhfs);

#if defined(WL_PSMX)
		if (PSMX_ENAB(wlc->pub)) {
			wlc_mxhfdef(wlc_hw, wlc_hw->band->mhfs);
		}
#endif /* WL_PSMX */

		if (!wlc_bmac_attach_dmapio(wlc_hw, wme)) {
			err = 19;
			goto fail;
		}
	}

#ifdef AWD_EXT_TRAP
	/* Attach extended trap handler for each phy module */
	awd_register_trap_ext_callback(wlc_bmac_trap_phy, (void *)wlc_hw->band->pi);
	awd_register_trap_ext_callback(wlc_bmac_trap_mac, (void *)wlc_hw);
#endif /* AWD_EXT_TRAP */

	/* set default 2-wire or 3-wire setting */
	wlc_bmac_btc_wire_set(wlc_hw, WL_BTC_DEFWIRE);

	wlc_hw->btc->btcx_aa = (uint8)getintvar(vars, rstr_aa2g);
	wlc_hw->btc->mode = (uint8)getintvar(vars, rstr_btc_mode);
	/* set BT Coexistence default mode */
	if (getvar(vars, rstr_btc_mode))
		wlc_bmac_btc_mode_set(wlc_hw, (uint8)getintvar(vars, rstr_btc_mode));
	else
		wlc_bmac_btc_mode_set(wlc_hw, WL_BTC_DEFAULT);

	/* attach/register iovar/ioctl handlers */
	if ((wlc_hw->iocvi =
	     wlc_iocv_low_attach(osh, WLC_IOVT_LOW_REG_SZ, WLC_IOCT_LOW_REG_SZ)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_iocv_low_attach failed\n", unit, __FUNCTION__));
		err = 11;
		goto fail;
	}
	if (wlc_bmac_register_iovt_all(wlc_hw, wlc_hw->iocvi) != BCME_OK) {
		err = 181;
		goto fail;
	}
	if (wlc_bmac_register_ioct_all(wlc_hw, wlc_hw->iocvi) != BCME_OK) {
		err = 182;
		goto fail;
	}

	if (D11REV_GE(wlc_hw->corerev, 61)) {
		uint coreunit;
		/**
		* MAC revision 61 had an EROM bug where Main and Aux slices
		* had their AXI slave base addresses swapped
		*/
		coreunit = wlc_hw->unit;
		if (D11REV_IS(wlc_hw->corerev, 61)) {
			if (coreunit == MAC_CORE_UNIT_0) {
				coreunit = MAC_CORE_UNIT_1;
			} else {
				coreunit = MAC_CORE_UNIT_0;
			}
		}

		wlc_hw->d11axi_slave_base_addr = si_get_d11_slaveport_addr(wlc_hw->sih,
		D11_AXI_SP_IDX, CORE_BASE_ADDR_0, coreunit);
	}

#ifdef PREATTACH_NORECLAIM
#if defined(DONGLEBUILD)
	/* 2 stage reclaim
	 *  download the ucode during attach, reclaim the ucode after attach
	 *    along with other rattach stuff, unconditionally on dongle
	 *  the second stage reclaim happens after up conditioning on reclaim flag
	 * for dual d11 core chips, ucode is downloaded only once
	 * and will be thru core-0
	 */
	if (wlc_hw->macunit == 0) {
		wlc_ucode_download(wlc_hw);
#if defined(WLVASIP)
		/* vasip intialization
		 * do vasip init() here during attach() because vasip ucode gets reclaimed.
		 */
		if (VASIP_ENAB(wlc->pub)) {
			uint32 vasipver;

			if (wlc_vasip_support(wlc_hw, &vasipver, FALSE)) {
				wlc_vasip_init(wlc_hw, vasipver, FALSE);
			}
		}
#endif /* WLVASIP */
#ifdef WLSMC
		if (wlc_smc_hw_supported(wlc_hw->corerev))
			wlc_smc_download(wlc_hw);
#endif /* WLSMC */

	}
#endif /* DONGLEBUILD */
#endif /* PREATTACH_NORECLAIM */

#ifdef SAVERESTORE
	if (SR_ENAB() && sr_cap(wlc_hw->sih)) {
#ifndef DONGLEBUILD
		/* Download SR code */
		wlc_bmac_sr_init(wlc_hw, wlc_hw->macunit);
#endif /* !DONGLEBUILD */
	}

#ifdef DONGLEBUILD
	if (SRPWR_ENAB()) {
		sr_register_save(wlc->pub->sih, wlc_sr_save, (void *)wlc);
	}
#endif // endif
#endif /* SAVERESTORE */

	/* disable core to match driver "down" state */
	wlc_coredisable(wlc_hw);

#ifdef ATE_BUILD
// FIXME : What is this supposed to do??
// 4347 ATE driver crashes if not commented out.
//	return err;
#endif // endif

	if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
		BCM43602_CHIP(wlc_hw->sih->chip) ||
		(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID))
		si_pmu_rfldo(wlc_hw->sih, 0);

	/* Match driver "down" state */
	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS)
		si_pci_down(wlc_hw->sih);

	/* register sb interrupt callback functions */
#ifdef BCMDBG
	si_register_intr_callback(wlc_hw->sih, (void *)wlc_wlintrsoff,
		(void *)wlc_wlintrsrestore, (void *)wlc_intrs_enabled, wlc_hw);
#else
	si_register_intr_callback(wlc_hw->sih, (void *)wlc_wlintrsoff,
		(void *)wlc_wlintrsrestore, NULL, wlc_hw);
#endif /* BCMDBG */

	ASSERT(!wlc_hw->sbclk || !si_taclear(wlc_hw->sih, TRUE));

	/* turn off pll and xtal to match driver "down" state */
	wlc_bmac_xtal(wlc_hw, OFF);

	/* *********************************************************************
	 * The hardware is in the DOWN state at this point. D11 core
	 * or cores are in reset with clocks off, and the board PLLs
	 * are off if possible.
	 *
	 * Beyond this point, wlc->sbclk == FALSE and chip registers
	 * should not be touched.
	 *********************************************************************
	 */

	/* init etheraddr state variables */
#ifdef STB_SOC_WIFI
	macaddr = wlc_stbsoc_get_macaddr((struct device *)btparam);
#endif /* STB_SOC_WIFI */

	if (macaddr == NULL) {
		if ((macaddr = wlc_get_macaddr(wlc_hw, wlc->pub->unit)) == NULL) {
			WL_ERROR(("wl%d: %s: macaddr not found\n", unit, __FUNCTION__));
			err = 21;
			goto fail;
		}
	}
#if defined(BCA_HNDROUTER) && defined(linux)
	osl_adjust_mac(wlc->pub->unit, macaddr);
#endif // endif

#ifdef WL_DUALNIC_RSDB
	if (wlc->pub->unit == 1) {
		bcopy(&wlc->cmn->wlc[0]->hw->etheraddr, &wlc_hw->etheraddr, ETHER_ADDR_LEN);
	} else
#endif // endif
	{
		bcm_ether_atoe(macaddr, &wlc_hw->etheraddr);
	}

	if (ETHER_ISBCAST((char*)&wlc_hw->etheraddr) ||
		ETHER_ISNULLADDR((char*)&wlc_hw->etheraddr)) {
		WL_ERROR(("wl%d: %s: bad macaddr %s\n", unit, __FUNCTION__, macaddr));
		err = 22;
		goto fail;
	}

	WL_INFORM(("wl%d: %s: board 0x%x macaddr: %s\n", unit, __FUNCTION__,
		wlc_hw->sih->boardtype, macaddr));

#ifdef WLLED
	if ((wlc_hw->ledh = wlc_bmac_led_attach(wlc_hw)) == NULL) {
		WL_ERROR(("wl%d: %s: wlc_bmac_led_attach() failed.\n", unit, __FUNCTION__));
		err = 23;
		goto fail;
	}
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
	wlc_hw->suspend_stats = (bmac_suspend_stats_t*) MALLOC(wlc_hw->osh,
	                                                       sizeof(*wlc_hw->suspend_stats));
	if (wlc_hw->suspend_stats == NULL) {
		WL_ERROR(("wl%d: wlc_bmac_attach: suspend_stats alloc failed.\n", unit));
		err = 26;
		goto fail;
	}
#endif // endif

#ifdef BCMPKTPOOL
	/* Register to be notified when pktpool is available which can
	 * happen outside this scope from bus side.
	 */
	if (BCMSPLITRX_ENAB()) {
		if (POOL_ENAB(wlc->pub->pktpool_rxlfrag)) {
			/* if rx frag pool is enabled, use fragmented rx pool for registration */
#ifdef BCMDBG_POOL
			pktpool_dbg_register(wlc->pub->pktpool_rxlfrag, wlc_pktpool_dbg_cb, wlc_hw);
#endif // endif
			/* register a callback to be invoked when hostaddress is posted */
			pkpool_haddr_avail_register_cb(wlc->pub->pktpool_rxlfrag,
				wlc_pktpool_avail_cb, wlc_hw);

			/* Pass down pool info to dma layer */
			if (wlc_hw->di[RX_FIFO]) {
				dma_pktpool_set(wlc_hw->di[RX_FIFO], wlc->pub->pktpool_rxlfrag);
			}

			/* if second fifo is set, set pktpool for that */
			if (wlc_hw->di[RX_FIFO1]) {
				if (RXFIFO_SPLIT()) {
					dma_pktpool_set(wlc_hw->di[RX_FIFO1],
						wlc->pub->pktpool_rxlfrag);
				} else {
					dma_pktpool_set(wlc_hw->di[RX_FIFO1],
						wlc->pub->pktpool);
					pktpool_avail_register(wlc->pub->pktpool,
						wlc_pktpool_avail_cb, wlc_hw);
				}
			}

			/* FIFO- 2 */
			if (wlc_hw->di[RX_FIFO2]) {
				dma_pktpool_set(wlc_hw->di[RX_FIFO2], wlc->pub->pktpool);
				pktpool_avail_register(wlc->pub->pktpool,
					wlc_pktpool_avail_cb, wlc_hw);
			}

		} else {
			WL_ERROR(("%s: RXLFRAG Pool not available split RX mode \n", __FUNCTION__));
		}
	} else if (POOL_ENAB(wlc->pub->pktpool)) {
#ifndef ATE_BUILD
		pktpool_avail_register(wlc->pub->pktpool,
			wlc_pktpool_avail_cb, wlc_hw);
#endif // endif
#ifdef BCMDBG_POOL
		pktpool_dbg_register(wlc->pub->pktpool, wlc_pktpool_dbg_cb, wlc_hw);
#endif // endif

		/* set pool for rx dma */
		if (wlc_hw->di[RX_FIFO])
			dma_pktpool_set(wlc_hw->di[RX_FIFO], wlc->pub->pktpool);
	}
#endif /* BCMPKTPOOL */

#ifdef ATE_BUILD
	return err;
#endif // endif

	/* Initialize btc param information from NVRAM */
	err = wlc_bmac_btc_param_attach(wlc);
	if (err != BCME_OK) {
		WL_ERROR(("%s: btc param attach FAILED (err %d)\n", __FUNCTION__, err));
		goto fail;
	}
#ifdef WOWL_GPIO
	wlc_hw->wowl_gpio = WOWL_GPIO;
#ifdef WOWL_GPIO_POLARITY
	wlc_hw->wowl_gpiopol = WOWL_GPIO_POLARITY;
#endif // endif
	{
		/* override wowl gpio if defined in nvram */
		char *var;
		if ((var = getvar(wlc_hw->vars, rstr_wowl_gpio)) != NULL)
			wlc_hw->wowl_gpio =  (uint8)bcm_strtoul(var, NULL, 0);
		if ((var = getvar(wlc_hw->vars, rstr_wowl_gpiopol)) != NULL)
			wlc_hw->wowl_gpiopol =  (bool)bcm_strtoul(var, NULL, 0);
	}
#endif /* WOWL_GPIO */
#if (defined(WLTEST) || defined(WLPKTENG))
	if ((wlc_hw->pkteng_timer =
			wl_init_timer(wlc->wl, wlc_bmac_pkteng_timer_cb, wlc_hw,
			"pkteng_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer() failed.\n", unit, __FUNCTION__));
		err = 30;
		goto fail;
	}

	if ((wlc_hw->pkteng_async->pkteng_async_timer =
			wl_init_timer(wlc->wl, wlc_bmac_pkteng_timer_async_rx_cb, wlc_hw,
			"pkteng_async_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer() failed.\n", unit, __FUNCTION__));
		err = 30;
		goto fail;
	}
#endif // endif

#ifdef BCMDBG_SR
	if (!(wlc_hw->srtimer = wl_init_timer(wlc->wl, wlc_sr_timer, wlc_hw, "srtimer"))) {
		WL_ERROR(("wl%d: srtimer failed\n", unit));
		err = 33;
		goto fail;
	}
#endif // endif
#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	if (STS_RX_ENAB(wlc_hw->wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc_hw->wlc->pub)) {
		wlc_dump_add_fns(wlc_hw->wlc->pub, "prxs_stats", wlc_bmac_prxs_stats_dump,
			wlc_bmac_prxs_stats_dump_clr, wlc_hw);
	}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */
#if WLC_BMAC_DUMP_NUM_REGS > 0
	wlc_hw->dump = wlc_dump_reg_create(wlc_hw->osh, WLC_BMAC_DUMP_NUM_REGS);
	if (wlc_hw->dump == NULL) {
		WL_ERROR(("wl%d: %s: wlc_dump_reg_create() failed.\n", unit, __FUNCTION__));
		err = 32;
		goto fail;
	}
#endif // endif
	wlc_bmac_register_dumps(wlc_hw);

	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		wlc_hw->templatebase =
			D11MAC_BMC_TPL_BYTES_PERCORE * wlc_hw->macunit;
	} else {
		wlc_hw->templatebase = 0;
	}
	if ((rsdb_mode = getvar(NULL, rstr_rsdb_mode))) {
		wlc->cmn->ap_rsdb_mode = (int8)bcm_atoi(rsdb_mode);
	}

	/* Read Link Cap && Link STA from config reg */
	wlc_bmac_read_pcie_link_bw(wlc_hw);
	return BCME_OK;

fail:
	WL_ERROR(("wl%d: %s: failed with err %d\n", unit, __FUNCTION__, err));
	return err;
} /* wlc_bmac_attach */

/**
 * Initialize wlc_info default values ... may get overrides later in this function.
 * BMAC_NOTES, move low out and resolve the dangling ones.
 */
void
BCMATTACHFN(wlc_bmac_info_init)(wlc_hw_info_t *wlc_hw)
{
	wlc_info_t *wlc = wlc_hw->wlc;

	(void)wlc;

	/* set default sw macintmask value */
	wlc_hw->defmacintmask = DEF_MACINTMASK;

	/* set default delayedintmask value */
	wlc_hw->delayedintmask = DELAYEDINTMASK;

	ASSERT(wlc_hw->corerev);

#ifdef WL_PRE_AC_DELAY_NOISE_INT
	/* delay noise interrupt for non ac chips */
	if (D11REV_LT(wlc_hw->corerev, 40)) {
		/* stop BG_NOISE from interrupting host */
		wlc_bmac_set_defmacintmask(wlc_hw, MI_BG_NOISE, ~MI_BG_NOISE);
		/* instead handle BG_NOISE when already interrupted */
		wlc_hw->delayedintmask |= MI_BG_NOISE;
	}
#endif /* WL_PRE_AC_DELAY_NOISE_INT */

	wlc_bmac_set_defmacintmask(wlc_hw, MI_HWACI_NOTIFY, MI_HWACI_NOTIFY);

#ifdef WLC_TSYNC
	if (D11REV_GE(wlc->pub->corerev, 40))
		wlc_hw->defmacintmask |= MI_DMATX;
#endif // endif

	/* inital value for receive interrupt lazy control */
	/* XXX The rcvlazy register value gave optimum util without tput dips
	   where the number of interrupts coalesced is 0x10 i.e. 16
	   and the timeout for interrupts is 0x20 i.e. 32 us
	   May need to experiment more
	 */
	wlc_hw->intrcvlazy = WLC_INTRCVLAZY_DEFAULT;

	/* various 802.11g modes */
	wlc_hw->shortslot = FALSE;

	wlc_hw->SFBL = RETRY_SHORT_FB;
	wlc_hw->LFBL = RETRY_LONG_FB;

	/* default mac retry limits */
	wlc_hw->SRL = RETRY_SHORT_DEF;
	wlc_hw->LRL = RETRY_LONG_DEF;
	wlc_hw->chanspec = CH20MHZ_CHSPEC(1);

#ifdef WLRXOV
	wlc->rxov_delay = RXOV_TIMEOUT_MIN;
	wlc->rxov_txmaxpkts = MAXTXPKTS;

	if (WLRXOV_ENAB(wlc->pub))
		wlc_hw->defmacintmask |= MI_RXOV;
#endif // endif

	wlc_hw->btswitch_ovrd_state = AUTO;

#ifdef WLP2P_UCODE
	/* default p2p to enabled */
#ifdef WLP2P_UCODE_ONLY
	wlc_hw->_p2p = TRUE;
	if (D11REV_IS(wlc_hw->corerev, 56) || D11REV_IS(wlc_hw->corerev, 61)) {
		wlc->cmn->ps_multista = TRUE;
	}
#endif /* WLP2P_UCODE_ONLY */
#endif /* WLP2P_UCODE */

#if defined(BCMHWA) && defined(HWA_TXSTAT_BUILD)
	/* stop MI_TFS from interrupting host */
	wlc_bmac_set_defmacintmask(wlc_hw, MI_TFS, ~MI_TFS);
#endif // endif
} /* wlc_bmac_info_init */

struct wlc_btc_param_vars_entry {
	uint16 index;
	uint16 value;
};

struct wlc_btc_param_vars_info {
	bool flags_present;
	uint16 flags;
	uint16 num_entries;
	struct wlc_btc_param_vars_entry param_list[0];
};

/** low level detach */
int
BCMATTACHFN(wlc_bmac_detach)(wlc_info_t *wlc)
{
	uint i;
	wlc_hwband_t *band;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	int callbacks;

	callbacks = 0;
	if (wlc_hw == NULL) {
		return callbacks;
	}
#if WLC_BMAC_DUMP_NUM_REGS > 0
	if (wlc_hw->dump != NULL) {
		wlc_dump_reg_destroy(wlc_hw->dump);
	}
#endif // endif
	if (wlc_hw->sih) {
		/* detach interrupt sync mechanism since interrupt is disabled and per-port
		 * interrupt object may has been freed. this must be done before sb core switch
		 */
		si_deregister_intr_callback(wlc_hw->sih);

		if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS)
			si_pci_sleep(wlc_hw->sih);
	}
	wlc_bmac_detach_dmapio(wlc_hw);

	band = wlc_hw->band;
	for (i = 0; i < NBANDS_HW(wlc_hw); i++) {
		/* So if this is a single band 11a card, use band 1 */
		if (IS_SINGLEBAND_5G(wlc_hw->deviceid, wlc_hw->phy_cap))
			i = BAND_5G_INDEX;

		if (band->pi) {
			/* Detach this band's phy */
			phy_module_detach((phy_info_t *)band->pi);
			band->pi = NULL;
		}
		band = wlc_hw->bandstate[OTHERBANDUNIT(wlc_hw)];
	}

	/* Free shared phy state */
	MODULE_DETACH(wlc_hw->phy_sh, wlc_phy_shared_detach);

	MODULE_DETACH(wlc_hw->physhim, wlc_phy_shim_detach);

	/* free vars */
	/*
	 * we are done with vars now, let wlc_detach take care of freeing it.
	 */
	wlc_hw->vars = NULL;

	/*
	 * we are done with sih now, let wlc_detach take care of freeing it.
	 */
	wlc_hw->sih = NULL;

#ifdef WLLED
	if (wlc_hw->ledh) {
		callbacks += wlc_bmac_led_detach(wlc_hw);
		wlc_hw->ledh = NULL;
	}
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
	if (wlc_hw->suspend_stats) {
		MFREE(wlc_hw->osh, wlc_hw->suspend_stats, sizeof(*wlc_hw->suspend_stats));
		wlc_hw->suspend_stats = NULL;
	}
#endif // endif

	if (wlc->btc_param_vars) {
		MFREE(wlc_hw->osh, wlc->btc_param_vars,
			sizeof(struct wlc_btc_param_vars_info) + wlc->btc_param_vars->num_entries
				* sizeof(struct wlc_btc_param_vars_entry));
		wlc->btc_param_vars = NULL;
	}

#ifdef DONGLEBUILD
	/* Free btc_params state */
	if (wlc_hw->btc->wlc_btc_params) {
		MFREE(wlc->osh, wlc_hw->btc->wlc_btc_params,
			M_BTCX_BACKUP_SIZE*sizeof(uint16));
		wlc_hw->btc->wlc_btc_params = NULL;
	}
#endif /* DONGLEBUILD */

	if (wlc_hw->btc->wlc_btc_params_fw) {
		MFREE(wlc->osh, wlc_hw->btc->wlc_btc_params_fw,
			BTC_FW_NUM_INDICES*sizeof(uint16));
		wlc_hw->btc->wlc_btc_params_fw = NULL;
	}

#if (defined(WLTEST) || defined(WLPKTENG))
	/* free pkteng timer */
	if (wlc_hw->pkteng_timer) {
		wl_free_timer(wlc->wl, wlc_hw->pkteng_timer);
		wlc_hw->pkteng_timer = NULL;
	}

	if (wlc_hw->pkteng_async->pkteng_async_timer) {
		wl_free_timer(wlc->wl, wlc_hw->pkteng_async->pkteng_async_timer);
		wlc_hw->pkteng_async->pkteng_async_timer = NULL;
	}

#endif // endif

#ifdef BCMDBG_SR
	if (wlc_hw->srtimer) {
		wl_free_timer(wlc->wl, wlc_hw->srtimer);
		wlc_hw->srtimer = NULL;
	}
#endif // endif

	if (wlc_hw->iocvi != NULL) {
		MODULE_DETACH(wlc_hw->iocvi, wlc_iocv_low_detach);
	}

#if defined(WL_TXS_LOG)
	wlc_bmac_txs_hist_detach(wlc_hw);
#endif // endif
	wlc_bmac_rx_stall_detach(wlc_hw);

#if defined(BCMHWA)

#ifdef HWA_PKT_MACRO
	if (wlc->hwa_txpkt) {
		MFREE(wlc_hw->osh, wlc->hwa_txpkt, sizeof(*wlc->hwa_txpkt));
		wlc->hwa_txpkt = NULL;
	}
#endif // endif
#endif /* BCMHWA */

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	if (STS_RX_ENAB(wlc_hw->wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc_hw->wlc->pub)) {
		MFREE(wlc->osh, wlc_hw->sts_phyrx_va_non_aligned, wlc_hw->sts_phyrx_alloc_sz);
		MFREE(wlc->osh, wlc_hw->sts_buff_t_array, wlc_hw->sts_buff_t_alloc_sz);
	}
#else
	/* Reserve a dummy sts buffer until "STS_FIFO_RXEN" is enabled for router NIC */
	if (D11REV_GE(wlc_hw->corerev, 129)) {
		MFREE(wlc_hw->osh, (void*) sts_mp_base_addr[wlc_hw->unit], sizeof(sts_buff_t));
	}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

	MODULE_DETACH(wlc_hw, wlc_hw_detach);
	wlc->hw = NULL;

	return callbacks;
} /* wlc_bmac_detach */

/** d11 core needs to be reset during a 'wl up' or 'wl down' */
void
BCMINITFN(wlc_bmac_reset)(wlc_hw_info_t *wlc_hw)
{
	bool dev_gone;
	uint32 tsf_l;
	uint32 tsf_h;

	WLCNTINCR(wlc_hw->wlc->pub->_cnt->reset);

	dev_gone = DEVICEREMOVED(wlc_hw->wlc);
	/* reset the core */
	if (!dev_gone) {
		/* When the D11 core rset is executeda as part of big hammer then the TSF should
		 * be preserved. since TSF gets initialized in a later stage, it is safe to
		 * resad TSF, reset D11 and restore TSF.
		 */
		tsf_l = R_REG(wlc_hw->osh, D11_TSFTimerLow(wlc_hw));
		tsf_h = R_REG(wlc_hw->osh, D11_TSFTimerHigh(wlc_hw));

		wlc_bmac_corereset(wlc_hw, WLC_USE_COREFLAGS);

		W_REG(wlc_hw->osh, D11_TSFTimerLow(wlc_hw), tsf_l);
		W_REG(wlc_hw->osh, D11_TSFTimerHigh(wlc_hw), tsf_h);
	}

	/* purge the pio queues or dma rings */
	wlc_hw->reinit = TRUE;
	wlc_flushqueues(wlc_hw);

	/* save a copy of the btc params before going down */
#ifdef DONGLEBUILD
	if (!dev_gone)
		wlc_bmac_btc_params_save(wlc_hw);
#endif // endif

	wlc_reset_bmac_done(wlc_hw->wlc);
}

/**
 * d11 core needs to be initialized during a 'wl up'.
 * At function exit, ucode is in suspended state.
 */
void
BCMINITFN(wlc_bmac_init)(wlc_hw_info_t *wlc_hw, chanspec_t chanspec)
{
	bool fastclk;
	wlc_info_t *wlc = wlc_hw->wlc;

	WL_TRACE(("wl%d: wlc_bmac_init\n", wlc_hw->unit));

	/* request FAST clock if not on */
	if (!(fastclk = wlc_hw->forcefastclk))
		wlc_clkctl_clk(wlc_hw, CLK_FAST);

	if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
		BCM43602_CHIP(wlc_hw->sih->chip) ||
		(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID))
		si_pmu_rfldo(wlc_hw->sih, 1);

	/* set up the specified band and chanspec */
	wlc_setxband(wlc_hw, CHSPEC_WLCBANDUNIT(chanspec));
	wlc_phy_chanspec_radio_set(wlc_hw->band->pi, chanspec);
	wlc_hw->chanspec = chanspec;

	/* do one-time phy inits and calibration */
	phy_calmgr_init(wlc_hw->band->pi);

	/* core-specific initialization. E.g. load and initialize ucode. */
#ifdef WL_MIMO_SISO_STATS
	/* take a snapshot and reset_last=TRUE
	 * since wlc_coreinit() initialize ucode
	 */
	if (wlc->core_boot_init) {
#if defined(WL_PWRSTATS)
		if (PWRSTATS_ENAB(wlc_hw->wlc->pub)) {
			wlc_mimo_siso_metrics_snapshot(wlc_hw->wlc, TRUE,
			        WL_MIMOPS_METRICS_SNAPSHOT_BMACINIT);
		}
#endif /* WL_PWRSTATS */
	} else {
		wlc->core_boot_init = TRUE;
	}
#endif /* WL_MIMO_SISO_STATS */
	wlc_hw->sw_macintstatus = 0;
	wlc_coreinit(wlc_hw); /* after this call, ucode is in 'suspended' state */

	/* CRWLDOT11M-2187 WAR: Always force backplane HT clock with CTDMA in 4365 */
	if (D11REV_IS(wlc_hw->corerev, 65)) {
		if (BCM_DMA_CT_ENAB(wlc_hw->wlc))
			fastclk = TRUE;
		else
			fastclk = FALSE;
	}

#ifndef DONGLEBUILD
#ifdef BCMHWA
	if (hwa_dev) {
		if (hwa_probe(hwa_dev, 0, HWA_CORE_ID, wlc_hw->unit) != BCME_OK) {
			WL_ERROR(("wl%d: hwa_probe for HWA%d failed\n",
				wlc_hw->unit, wlc_hw->unit));
			ASSERT(0);
		}
	}
#endif /* BCMHWA */
#endif /* DONGLEBUILD */

	/*
	 * initialize mac_suspend_depth to 1 to match ucode initial suspended state
	 */
	wlc_hw->mac_suspend_depth = 1;
#if defined(WL_PSMX)
	wlc_hw->macx_suspend_depth = 1;
#endif /* WL_PSMX */
#if defined(WL_PSMX) && defined(WL_AIR_IQ)
	if (D11REV_GE(wlc_hw->corerev, 65)) {
		/* setup PSMX interrupt mask */
		SET_MACINTMASK_X(wlc_hw->osh, wlc_hw, DEF_MACINTMASK_X);
	}
#endif /* WL_PSMX && WL_AIR_IQ */

	/* seed wake_override with WLC_WAKE_OVERRIDE_MACSUSPEND since the mac
	 * is suspended and wlc_bmac_enable_mac() will clear this override bit.
	 */
	mboolset(wlc_hw->wake_override, WLC_WAKE_OVERRIDE_MACSUSPEND);

	/* XXX:
	 * For HTPHY, must switch radio off to allow pi->radio_is_on to be cleared.
	 * HTPHY wlc_phy_switch_radio_htphy() checks for pi->radio_is_on.
	 * if pi->radio_is_on is set, wlc_phy_radio2059_init and wlc_phy_chanspec_set
	 * will not get called when switching radio back on.
	 * In cases when "wl reinit" or big hammer is triggered, not calling
	 * wlc_phy_radio2059_init and/or wlc_phy_chanspec_set in wlc_phy_switch_radio_htphy()
	 * when switching radio on in wlc_bmac_bsinit()
	 * will cause the driver to lose tx/rx capabilities and lose connection to AP.
	 */
	phy_radio_init((phy_info_t *)wlc_hw->band->pi);

	if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
	    (CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
	    (CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) ||
	    (CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
	    (EMBEDDED_2x2AX_CORE(wlc_hw->sih->chip)) ||
	    BCM6878_CHIP(wlc_hw->sih->chip) ||
	    BCM43602_CHIP(wlc_hw->sih->chip) ||
	    BCM4350_CHIP(wlc_hw->sih->chip)) {
		/**
		 * JIRA:SWWLAN-26291. Whenever driver changes BBPLL frequency it needs to adjust
		 * the TSF clock as well.
		 */
		wlc_bmac_switch_macfreq(wlc_hw, 0);
	}

	/* band-specific inits */
	wlc_bmac_bsinit(wlc_hw, chanspec, FALSE);

#if !defined(WL_PROXDETECT)
	/* Low power modes will switch off cores other than host bus */
	/* TOF AVB timer CLK won't work when si_lowpwr_opt is called */
	if (!PROXD_ENAB(wlc->pub))
		si_lowpwr_opt(wlc_hw->sih);
#endif // endif

	/* restore the clk */
	if (!fastclk)
		wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);

	if (MAC_CLKGATING_ENAB(wlc->pub)) {
		wlc_bmac_enable_mac_clkgating(wlc);
	}

	if (D11REV_IS(wlc_hw->corerev, 61)) {
		if (RSDB_ENAB(wlc_hw->wlc->pub) && WLC_DUALMAC_RSDB(wlc_hw->wlc->cmn)) {
			/* HW4347-701: For power consumption
			 * Remap MAC and AVB clock request from
			 * both main and aux mac
			 */
			if (wlc_bmac_coreunit(wlc_hw->wlc) == DUALMAC_MAIN) {
				si_wrapperreg(wlc_hw->sih, AI_OOBSELOUTB74, ~0, 0x84858484);
			} else if (wlc_bmac_coreunit(wlc_hw->wlc) == DUALMAC_AUX) {
				si_wrapperreg(wlc_hw->sih, AI_OOBSELOUTB74, ~0, 0x84868484);
			}
		}

#if !defined(BCMDONGLEHOST)
		si_pmu_set_mac_rsrc_req(wlc_hw->sih, wlc_hw->macunit);
#endif // endif
	}

	if (D11REV_GE(wlc_hw->corerev, 128)) {
#if !defined(BCMDONGLEHOST)
		si_pmu_set_mac_rsrc_req(wlc_hw->sih, wlc_hw->macunit);
#endif // endif
	}

	wlc_hw->reinit = FALSE;
} /* wlc_bmac_init */

/** called during 'wl up' (after wlc_bmac_init), or on a 'big hammer' event */
int
BCMINITFN(wlc_bmac_up_prep)(wlc_hw_info_t *wlc_hw)
{
	uint coremask;
	wlc_tunables_t *tune = wlc_hw->wlc->pub->tunables;

	WL_TRACE(("wl%d: %s:\n", wlc_hw->unit, __FUNCTION__));

	ASSERT(wlc_hw->wlc->pub->hw_up && wlc_hw->macintmask == 0);

	if (BCM4350_CHIP(wlc_hw->sih->chip) &&
	    (CHIPREV(wlc_hw->sih->chiprev) == 0)) {
		si_pmu_chipcontrol(wlc_hw->sih, PMU_CHIPCTL2,
			PMU_CC2_FORCE_PHY_PWR_SWITCH_ON,
			PMU_CC2_FORCE_PHY_PWR_SWITCH_ON);
	}

	/*
	 * Enable pll and xtal, initialize the power control registers,
	 * and force fastclock for the remainder of wlc_up().
	 */
	wlc_bmac_xtal(wlc_hw, ON);
	si_clkctl_init(wlc_hw->sih);
	wlc_clkctl_clk(wlc_hw, CLK_FAST);

	/*
	 * Configure pci/pcmcia here instead of in wlc_attach()
	 * to allow mfg hotswap:  down, hotswap (chip power cycle), up.
	 */
	coremask = (1 << wlc_hw->wlc->core->coreidx);

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS)
		si_pci_setup(wlc_hw->sih, coremask);
	else if (BUSTYPE(wlc_hw->sih->bustype) == PCMCIA_BUS) {
		wlc_hw->regs = (d11regs_t*)si_setcore(wlc_hw->sih, D11_CORE_ID, wlc_hw->macunit);
		ASSERT(wlc_hw->regs != NULL);
		wlc_hw->wlc->regs = wlc_hw->regs;
		si_pcmcia_init(wlc_hw->sih);
	}
	ASSERT(si_coreid(wlc_hw->sih) == D11_CORE_ID);

	/*
	 * Need to read the hwradio status here to cover the case where the system
	 * is loaded with the hw radio disabled. We do not want to bring the driver up in this case.
	 */
	if (wlc_bmac_radio_read_hwdisabled(wlc_hw)) {
		/* put SB PCI in down state again */
		if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS)
			si_pci_down(wlc_hw->sih);
		wlc_bmac_xtal(wlc_hw, OFF);
		return BCME_RADIOOFF;
	}

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		if (tune->mrrs != AUTO) {
			si_pcie_set_request_size(wlc_hw->sih, (uint16)tune->mrrs);
			si_pcie_set_maxpayload_size(wlc_hw->sih, (uint16)tune->mrrs);
		}

		si_pci_up(wlc_hw->sih);
	}

	/* Jira: SWWLAN-47716: In the down path, the FEM control has been overridden.
	 * Restore FEM control back to its default.
	 */
	if (BCM43602_CHIP(wlc_hw->sih->chip)) {
		si_pmu_chipcontrol(wlc_hw->sih, CHIPCTRLREG1, PMU43602_CC1_GPIO12_OVRD, 0);
	}

	/* reset the d11 core */
	wlc_bmac_corereset(wlc_hw, WLC_USE_COREFLAGS);

	return 0;
} /* wlc_bmac_up_prep */

/** called during 'wl up', after the chanspec has been set */
int
BCMINITFN(wlc_bmac_up_finish)(wlc_hw_info_t *wlc_hw)
{
	bool disable_dynamic_clock = FALSE;
	WL_TRACE(("wl%d: %s:\n", wlc_hw->unit, __FUNCTION__));

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
	bzero(wlc_hw->suspend_stats, sizeof(*wlc_hw->suspend_stats));
	wlc_hw->suspend_stats->suspend_start = (uint32)-1;
	wlc_hw->suspend_stats->suspend_end = (uint32)-1;
#endif // endif
	wlc_hw->up = TRUE;

	phy_hw_state_upd(wlc_hw->band->pi, TRUE);

	/* CRWLDOT11M-2187 WAR: Always force backplane HT clock in 4365 */
	if (BCM_DMA_CT_ENAB(wlc_hw->wlc) && D11REV_IS(wlc_hw->corerev, 65)) {
		disable_dynamic_clock = TRUE;
	}
	/* FULLY enable dynamic power control and d11 core interrupt */
	if (!disable_dynamic_clock)
		wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);
	ASSERT(!wlc_hw->sbclk || !si_taclear(wlc_hw->sih, TRUE));
	wl_intrson(wlc_hw->wlc->wl);

#if NCONF || ACCONF || ACCONF2
	wlc_bmac_ifsctl_edcrs_set(wlc_hw, FALSE);
#endif /* NCONF */

	return 0;
}

/** On some chips, pins are multiplexed and serve either an SROM or WLAN specific function */
int
BCMINITFN(wlc_bmac_set_ctrl_SROM)(wlc_hw_info_t *wlc_hw)
{
	WL_TRACE(("wl%d: %s:\n", wlc_hw->unit, __FUNCTION__));

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		if (BCM43602_CHIP(wlc_hw->sih->chip) ||
			(((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID)) &&
			(CHIPREV(wlc_hw->sih->chiprev) <= 2))) {
			si_chipcontrl_srom4360(wlc_hw->sih, TRUE);
		}
	}

	return 0;
}

/** tear down d11 interrupts, cancel BMAC software timers, tear down PHY operation */
int
BCMUNINITFN(wlc_bmac_down_prep)(wlc_hw_info_t *wlc_hw)
{
	bool dev_gone;
	uint callbacks = 0;

	WL_TRACE(("wl%d: %s:\n", wlc_hw->unit, __FUNCTION__));

	if (!wlc_hw->up)
		return callbacks;

	dev_gone = DEVICEREMOVED(wlc_hw->wlc);

#if defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)
	if (D11REV_GE(wlc_hw->corerev, 130)) {
		wlc_offload_deinit(wlc_hw->wlc->offl);
	}
#endif /* WLC_OFFLOADS_TXSTS || WLC_OFFLOADS_RXSTS */

	/* disable interrupts */
	if (dev_gone)
		wlc_hw->macintmask = 0;
	else {
		/* now disable interrupts */
		wl_intrsoff(wlc_hw->wlc->wl);

		/* ensure we're running on the pll clock again */
		wlc_clkctl_clk(wlc_hw, CLK_FAST);

		/* Disable GPIOs related to BTC returning the control to chipcommon */
		if (!wlc_hw->noreset)
			wlc_bmac_btc_gpio_disable(wlc_hw);
	}

	/* apply gpio WAR in the down path */
	wlc_bmac_gpio_configure(wlc_hw, FALSE);

	/* save a copy of the btc params before going down */
#ifdef DONGLEBUILD
	if (!dev_gone) {
		wlc_bmac_btc_params_save(wlc_hw);
	}
#endif /* DONGLEBUILD */

	/* down phy at the last of this stage */
	callbacks += phy_down((phy_info_t *)wlc_hw->band->pi);

	return callbacks;
} /* wlc_bmac_down_prep */

void
BCMUNINITFN(wlc_bmac_hw_down)(wlc_hw_info_t *wlc_hw)
{
	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		wlc_bmac_set_ctrl_SROM(wlc_hw);

		if ((wlc_hw->sih->boardvendor == VENDOR_APPLE) &&
		    ((wlc_hw->sih->boardtype == BCM94360X29C) ||
		     (wlc_hw->sih->boardtype == BCM94360X29CP2) ||
		     (wlc_hw->sih->boardtype == BCM94360X29CP3) ||
		     (wlc_hw->sih->boardtype == BCM94360X52C))) {
			/* Set GPIO7 as input */
			si_pmu_chipcontrol(wlc_hw->sih, CHIPCTRLREG1, PMU4360_CC1_GPIO7_OVRD, 0);
			/* Switch pin MUX from FEM to GPIO control */
			si_corereg(wlc_hw->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
				CCTRL4360_BTSWCTRL_MODE, 0);
		}
#ifdef BCMECICOEX
		/* seci down */
		if (BCMSECICOEX_ENAB_BMAC(wlc_hw))
			si_seci_down(wlc_hw->sih);
#endif /* BCMECICOEX */
		si_pci_down(wlc_hw->sih);
#if defined(SAVERESTORE) && !defined(DONGLEBUILD)
		/* for NIC mode, disable SR if we're down */
		if (SR_ENAB() && sr_cap(wlc_hw->sih)) {
			sr_engine_enable(wlc_hw->sih, wlc_hw->macunit, IOV_SET, FALSE);
		}
#endif /* SAVERESTORE && !DONGLEBUILD */
	}

	/* Jira: SWWLAN-47716: override the FEM control to GPIO (High-Z) so that in down state
	 * the pin is not driven low which causes excess current draw.
	 */
	if (BCM43602_CHIP(wlc_hw->sih->chip)) {
		si_pmu_chipcontrol(wlc_hw->sih, CHIPCTRLREG1,
			PMU43602_CC1_GPIO12_OVRD, PMU43602_CC1_GPIO12_OVRD);
	}

	if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
		BCM43602_CHIP(wlc_hw->sih->chip) ||
		(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID))
		si_pmu_rfldo(wlc_hw->sih, 0);

	wlc_bmac_xtal(wlc_hw, OFF);

	if (BCM4350_CHIP(wlc_hw->sih->chip) &&
	    (CHIPREV(wlc_hw->sih->chiprev) == 0)) {
		si_pmu_chipcontrol(wlc_hw->sih, PMU_CHIPCTL2, PMU_CC2_FORCE_PHY_PWR_SWITCH_ON, 0);
	}
}

int
BCMUNINITFN(wlc_bmac_down_finish)(wlc_hw_info_t *wlc_hw)
{
	uint callbacks = 0;
	bool dev_gone;

	WL_TRACE(("wl%d: %s:\n", wlc_hw->unit, __FUNCTION__));

	if (!wlc_hw->up)
		return callbacks;
	wlc_hw->up = FALSE;
	phy_hw_state_upd(wlc_hw->band->pi, FALSE);

	dev_gone = DEVICEREMOVED(wlc_hw->wlc);

	if (dev_gone) {
		wlc_hw->sbclk = FALSE;
		wlc_hw->clk = FALSE;
		phy_hw_clk_state_upd(wlc_hw->band->pi, FALSE);

		/* reclaim any posted packets */
		wlc_flushqueues(wlc_hw);
	} else if (!wlc_hw->wlc->psm_watchdog_debug) {

		/* Reset and disable the core */
		if (si_iscoreup(wlc_hw->sih)) {
			if (R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
#ifdef WL_PSMX
			if (PSMX_ENAB(wlc_hw->wlc->pub) &&
				(R_REG(wlc_hw->osh, D11_MACCONTROL_psmx(wlc_hw)) & MCTL_EN_MAC)) {
				wlc_bmac_suspend_macx_and_wait(wlc_hw);
			}
#endif // endif
			callbacks += wl_reset(wlc_hw->wlc->wl);
			wlc_coredisable(wlc_hw);
		}

		/* turn off primary xtal and pll */
		if (!wlc_hw->noreset)
			wlc_bmac_hw_down(wlc_hw);
	}

	ASSERT(!wlc_hw->sbclk || !si_taclear(wlc_hw->sih, TRUE));

	return callbacks;
}

/** 802.11 Power State (PS) related */
void
wlc_bmac_wait_for_wake(wlc_hw_info_t *wlc_hw)
{
	uint16 psmstate;

	/* delay before first read of ucode state */
	if (!(wlc_hw->band)) {
		WL_ERROR(("wl%d: %s:Active per-band state not set. \n",
			wlc_hw->unit, __FUNCTION__));
		return;
	}

	OSL_DELAY(40);

	/* wait until ucode is no longer asleep */
	SPINWAIT((wlc_bmac_read_shm(wlc_hw, M_UCODE_DBGST(wlc_hw)) != DBGST_ACTIVE),
		wlc_hw->fastpwrup_dly);

	psmstate = wlc_bmac_read_shm(wlc_hw, M_UCODE_DBGST(wlc_hw));
	if (psmstate != DBGST_ACTIVE) {
		WL_PRINT(("WAKE FAIL: dbgst 0x%04x psmdebug 0x%04x slowctl 0x%04x "
			"corectlsts 0x%04x\n", psmstate,
			R_REG(wlc_hw->wlc->osh, D11_PSM_DEBUG(wlc_hw->wlc)),
			R_REG(wlc_hw->wlc->osh, D11_SLOW_CTL(wlc_hw->wlc)),
			R_REG(wlc_hw->wlc->osh, D11_PSMCoreCtlStat(wlc_hw->wlc))));
		if (wlc_bmac_report_fatal_errors(wlc_hw, WL_REINIT_RC_MAC_WAKE)) {
			return;
		}
	}
#if !defined(DONGLEBUILD)
	 else {
		/* if HPS is 0, update SW awake_hps_0 status indicating ucode is awake.
		 * This will be cleared on re-enabling the HPS
		 */
		if (!(wlc_hw->maccontrol & MCTL_HPS) && !wlc_hw->awake_hps_0) {
			wlc_hw->awake_hps_0 = TRUE;
		}
	}
#endif /* !DONGLEBUILD */

	ASSERT(psmstate == DBGST_ACTIVE);

}

void
wlc_bmac_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea)
{
	bcopy(&wlc_hw->etheraddr, ea, ETHER_ADDR_LEN);
}

void
wlc_bmac_set_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea)
{
	bcopy(ea, &wlc_hw->etheraddr, ETHER_ADDR_LEN);
}

int
wlc_bmac_bandtype(wlc_hw_info_t *wlc_hw)
{
	return (wlc_hw->band->bandtype);
}

void *
wlc_cur_phy(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	return ((void *)wlc_hw->band->pi);
}

/** control chip clock to save power, enable dynamic clock or force fast clock */
static void
wlc_clkctl_clk(wlc_hw_info_t *wlc_hw, uint mode)
{
	if (PMUCTL_ENAB(wlc_hw->sih)) {
		/* new chips with PMU, CCS_FORCEHT will distribute the HT clock on backplane,
		 *  but mac core will still run on ALP(not HT) when it enters powersave mode,
		 *      which means the FCA bit may not be set.
		 *      should wakeup mac if driver wants it to run on HT.
		 */

		if (wlc_hw->clk) {
			if (mode == CLK_FAST) {
				OR_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw), CCS_FORCEHT);

				/* PR53224 PR53908: PMU could be in ILP clock while we reset
				 * d11 core. Thus, if we do forceht after a d11 reset, it
				 * might take 1 ILP clock cycle (32us) + 2 ALP clocks (~100ns)
				 * to get through all the transitions before having HT clock
				 * ready. Put 64us instead of the minimum 33us here for some
				 * margin.
				 */
				OSL_DELAY(64);

				SPINWAIT(((R_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw)) &
				           CCS_HTAVAIL) == 0), PMU_MAX_TRANSITION_DLY);
				ASSERT(R_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw)) &
					CCS_HTAVAIL);
			} else {
				if ((PMUREV(wlc_hw->sih->pmurev) == 0) &&
				    (R_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw)) &
				     (CCS_FORCEHT | CCS_HTAREQ)))
					SPINWAIT(((R_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw)) &
					           CCS_HTAVAIL) == 0), PMU_MAX_TRANSITION_DLY);
				AND_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw), ~CCS_FORCEHT);
			}
		}
		wlc_hw->forcefastclk = (mode == CLK_FAST);
	} else {
		wlc_hw->forcefastclk = si_clkctl_cc(wlc_hw->sih, mode);

		/* check fast clock is available (if core is not in reset) */
		if (wlc_hw->forcefastclk && wlc_hw->clk)
			ASSERT(si_core_sflags(wlc_hw->sih, 0, 0) & SISF_FCLKA);

		/* keep the ucode wake bit on if forcefastclk is on
		 * since we do not want ucode to put us back to slow clock
		 * when it dozes for PM mode.
		 * Code below matches the wake override bit with current forcefastclk state
		 * Only setting bit in wake_override instead of waking ucode immediately
		 * since old code (wlc.c 1.4499) had this behavior. Older code set
		 * wlc->forcefastclk but only had the wake happen if the wakup_ucode work
		 * (protected by an up check) was executed just below.
		 */
		if (wlc_hw->forcefastclk)
			mboolset(wlc_hw->wake_override, WLC_WAKE_OVERRIDE_FORCEFAST);
		else
			mboolclr(wlc_hw->wake_override, WLC_WAKE_OVERRIDE_FORCEFAST);

	}
} /* wlc_clkctl_clk */

/** Forcing Core1's HW request Off bit in PM Mode for MIMO and 80P80 */
void
wlc_bmac_4349_core1_hwreqoff(wlc_hw_info_t *wlc_hw, bool mode)
{
	ASSERT(wlc_hw != NULL);

	/* Apply the setting to D11 core unit one always */
	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		int sicoreunit = 0;

		sicoreunit = si_coreunit(wlc_hw->sih);

		/* Apply the setting to D11 core unit one always */
		if ((wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_MIMO) ||
		(wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_80P80)) {
			d11regs_t *regs;
			uint32 backup_mask;

			/* Turn off interrupts before switching core */
			backup_mask = wlc_intrsoff(wlc_hw->wlc);
			regs = si_d11_switch_addrbase(wlc_hw->sih, MAC_CORE_UNIT_1);

			if (mode == TRUE) {
				OR_REG(wlc_hw->osh,
					D11_ClockCtlStatus_ALTBASE(regs, wlc_hw->regoffsets),
					CCS_FORCEHWREQOFF);
			} else {
				AND_REG(wlc_hw->osh,
					D11_ClockCtlStatus_ALTBASE(regs, wlc_hw->regoffsets),
					~CCS_FORCEHWREQOFF);
			}

			si_d11_switch_addrbase(wlc_hw->sih, sicoreunit);
			wlc_intrsrestore(wlc_hw->wlc, backup_mask);
		}
	}
}

/**
 * Update the hardware for rcvlazy (interrupt mitigation) setting changes
 */
void
wlc_bmac_rcvlazy_update(wlc_hw_info_t *wlc_hw, uint32 intrcvlazy)
{
	W_REG(wlc_hw->osh, (D11Reggrp_intrcvlazy(wlc_hw, 0)), intrcvlazy);

	/* interrupt for second fifo - 1 */
	if (PKT_CLASSIFY_EN(RX_FIFO1) || RXFIFO_SPLIT()) {
		W_REG(wlc_hw->osh, (D11Reggrp_intrcvlazy(wlc_hw, RX_FIFO1)), intrcvlazy);
	}
	/* interrupt for second fifo - 2 */
	if (PKT_CLASSIFY_EN(RX_FIFO2)) {
		W_REG(wlc_hw->osh, (D11Reggrp_intrcvlazy(wlc_hw, RX_FIFO2)), intrcvlazy);
	}

#ifdef STS_FIFO_RXEN
	/* interrupt for fifo - 3 */
	if (STS_RX_ENAB(wlc_hw->wlc->pub)) {
		W_REG(wlc_hw->osh, (D11Reggrp_intrcvlazy(wlc_hw, STS_FIFO)), intrcvlazy);
	}
#endif // endif
}

/** set initial host flags value. Ucode interprets these host flags. */
static void
BCMUCODEFN(wlc_mhfdef)(wlc_hw_info_t *wlc_hw, uint16 *mhfs)
{
	bzero(mhfs, sizeof(uint16) * MHFMAX);

	/* prohibit use of slowclock on multifunction boards */
	if (wlc_hw->boardflags & BFL_NOPLLDOWN)
		mhfs[MHF1] |= MHF1_FORCEFASTCLK;

	/* set host flag to enable ucode for srom9: tx power offset based on txpwrctrl word */
	if (D11REV_LT(wlc_hw->corerev, 40)) {
		if (WLCISNPHY(wlc_hw->band) && (wlc_hw->sromrev >= 9)) {
			mhfs[MHF2] |= MHF2_PPR_HWPWRCTL;
		}
	}

#ifdef BCM_DMA_CT
	/* In CTDMA data path, to be less platform dependent, we need to do preloading in ucode. */
	if (BCM_DMA_CT_ENAB(wlc_hw->wlc) &&
	    (D11REV_GE(wlc_hw->corerev, 65) && D11REV_LT(wlc_hw->corerev, 128))) {
		/* enable preloading with CTDMA */
		mhfs[MHF2] |= MHF2_PRELD_GE64;
	}
#endif /* BCM_DMA_CT */

	if (D11REV_GE(wlc_hw->corerev, 128) && D11REV_LE(wlc_hw->corerev, 130)) {
		mhfs[MHF2] |= MHF2_RXWDT;
	}

#ifdef USE_LHL_TIMER
	mhfs[MHF2] |= MHF2_HIB_FEATURE_ENABLE;
#endif /* USE_LHL_TIMER */

	if (CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) {
		/* hostflag to tell the ucode that the interface is USB.
		ucode doesn't pull the HT request from the backplane.
		*/
		mhfs[MHF3] |= MHF3_USB_OLD_NPHYMLADVWAR;
	}

#if defined(WL_EAP_NOISE_MEASUREMENTS)
	/* Enterprise always uses Knoise and we don't want to rely on router configs,
	 * e.g. acsd, to send the IOV_BMAC_NOISE_METRIC to set it once the band is
	 * properly configured.
	 * Note that NOISE_MEASURE_KNOISE is defined in wlioctl.h.
	 */
	wlc_hw->noise_metric = NOISE_MEASURE_KNOISE;
	if (wlc_hw->noise_metric) {
		mhfs[MHF3] |= MHF3_KNOISE;
	}
#endif /* WL_EAP_NOISE_MEASUREMENTS */
}

#if defined(WL_PSMX)
/** set initial psmx host flags value. Ucode interprets these host flags. */
static void
BCMUCODEFN(wlc_mxhfdef)(wlc_hw_info_t *wlc_hw, uint16 *mhfs)
{
	/* Psmx host flags starts from MXHF0 */
	bzero(&mhfs[MXHF0], sizeof(uint16) * MXHFMAX);

	mhfs[MXHF0] = (MXHF0_CHKFID | MXHF0_DYNSND | MXHF0_MUAGGOPT);

	if (D11REV_IS(wlc_hw->corerev, 65) && MU_TX_ENAB(wlc_hw->wlc)) {
		mhfs[MXHF0] |= MXHF0_MUTXUFWAR;
	}
}
#endif /* WL_PSMX */

/**
 * set or clear ucode host flag bits
 * it has an optimization for no-change write
 * it only writes through shared memory when the core has clock;
 * pre-CLK changes should use wlc_write_mhf to get around the optimization
 *
 * bands values are: WLC_BAND_AUTO <--- Current band only
 *                   WLC_BAND_5G   <--- 5G band only
 *                   WLC_BAND_2G   <--- 2G band only
 *                   WLC_BAND_ALL  <--- All bands
 */
void
wlc_bmac_mhf(wlc_hw_info_t *wlc_hw, uint8 idx, uint16 mask, uint16 val, int bands)
{
	uint16 save;
	const uint16 addr[] = {M_HOST_FLAGS(wlc_hw), M_HOST_FLAGS2(wlc_hw), M_HOST_FLAGS3(wlc_hw),
		M_HOST_FLAGS4(wlc_hw), M_HOST_FLAGS5(wlc_hw)};
#if defined(WL_PSMX)
	const uint16 addrx[] = {
		MX_HOST_FLAGS0(wlc_hw->wlc),
		MX_HOST_FLAGS1(wlc_hw->wlc)};
#endif /* WL_PSMX */
	wlc_hwband_t *band;

	ASSERT((val & ~mask) == 0);
#if defined(WL_PSMX)
	ASSERT(idx < MXHF0+MXHFMAX);
	ASSERT(ARRAYSIZE(addrx) == MXHFMAX);
#else
	ASSERT(idx < MHFMAX);
#endif /* WL_PSMX */
	ASSERT(ARRAYSIZE(addr) == MHFMAX);

	switch (bands) {
		/* Current band only or all bands,
		 * then set the band to current band
		 */
	case WLC_BAND_AUTO:
	case WLC_BAND_ALL:
		band = wlc_hw->band;
		break;
	case WLC_BAND_5G:
		band = wlc_hw->bandstate[BAND_5G_INDEX];
		break;
	case WLC_BAND_2G:
		band = wlc_hw->bandstate[BAND_2G_INDEX];
		break;
	default:
		ASSERT(0);
		band = NULL;
	}

	if (band) {
		save = band->mhfs[idx];
		band->mhfs[idx] = (band->mhfs[idx] & ~mask) | val;

		/* optimization: only write through if changed, and
		 * changed band is the current band
		 */
		if (wlc_hw->clk && (band->mhfs[idx] != save) && (band == wlc_hw->band)) {
			if (idx < MXHF0) {
				wlc_bmac_write_shm(wlc_hw, addr[idx], (uint16)band->mhfs[idx]);
#if defined(WL_PSMR1)
				if (PSMR1_ENAB(wlc_hw)) {
					wlc_bmac_write_shm1(
						wlc_hw, addr[idx], (uint16)band->mhfs[idx]);
				}
#endif /* WL_PSMR1 */
			}
#if defined(WL_PSMX)
			else if (PSMX_ENAB(wlc_hw->wlc->pub)) {
				wlc_bmac_write_shmx(
					wlc_hw, addrx[idx-MXHF0], (uint16)band->mhfs[idx]);
			}
#endif /* WL_PSMX */
		}
	}

	if (bands == WLC_BAND_ALL) {
		wlc_hw->bandstate[0]->mhfs[idx] = (wlc_hw->bandstate[0]->mhfs[idx] & ~mask) | val;
		wlc_hw->bandstate[1]->mhfs[idx] = (wlc_hw->bandstate[1]->mhfs[idx] & ~mask) | val;
	}
} /* wlc_bmac_mhf */

uint16
wlc_bmac_mhf_get(wlc_hw_info_t *wlc_hw, uint8 idx, int bands)
{
	wlc_hwband_t *band;
#if defined(WL_PSMX)
	ASSERT(idx < MXHF0+MXHFMAX);
#else
	ASSERT(idx < MHFMAX);
#endif /* WL_PSMX */

	switch (bands) {
	case WLC_BAND_AUTO:
		band = wlc_hw->band;
		break;
	case WLC_BAND_5G:
		band = wlc_hw->bandstate[BAND_5G_INDEX];
		break;
	case WLC_BAND_2G:
		band = wlc_hw->bandstate[BAND_2G_INDEX];
		break;
	default:
		ASSERT(0);
		band = NULL;
	}

	if (!band)
		return 0;

	return band->mhfs[idx];
}

/**
 * @param[in] mhfs    An array of host flags
 */
static void
wlc_write_mhf(wlc_hw_info_t *wlc_hw, uint16 *mhfs)
{
	uint8 idx;
	const uint16 addr[] = {M_HOST_FLAGS(wlc_hw), M_HOST_FLAGS2(wlc_hw), M_HOST_FLAGS3(wlc_hw),
		M_HOST_FLAGS4(wlc_hw), M_HOST_FLAGS5(wlc_hw)};
#if defined(WL_PSMX)
	const uint16 addrx[] = {
		MX_HOST_FLAGS0(wlc_hw->wlc),
		MX_HOST_FLAGS1(wlc_hw->wlc)};
#endif /* WL_PSMX */

	ASSERT(ARRAYSIZE(addr) == MHFMAX);
#if defined(WL_PSMX)
	ASSERT(ARRAYSIZE(addrx) == MXHFMAX);
#endif /* WL_PSMX */

	for (idx = 0; idx < MHFMAX; idx++) {
		wlc_bmac_write_shm(wlc_hw, addr[idx], mhfs[idx]);
#if defined(WL_PSMR1)
		if (PSMR1_ENAB(wlc_hw)) {
			wlc_bmac_write_shm1(wlc_hw, addr[idx], mhfs[idx]);
		}
#endif // endif
	}
#if defined(WL_PSMX)
	if (PSMX_ENAB(wlc_hw->wlc->pub)) {
		for (idx = 0; idx < MXHFMAX; idx++) {
			wlc_bmac_write_shmx(wlc_hw, addrx[idx], mhfs[MXHF0+idx]);
		}
	}
#endif /* WL_PSMX */
}

void
wlc_bmac_ifsctl1_regshm(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val)
{
	osl_t *osh;
	uint32 w;
	volatile uint16 *ifsctl_reg;

	if (D11REV_GE(wlc_hw->corerev, 40))
		return;

	osh = wlc_hw->osh;

	ifsctl_reg = (volatile uint16 *) D11_IFS_CTL1(wlc_hw);

	w = (R_REG(osh, ifsctl_reg) & ~mask) | val;
	W_REG(osh, ifsctl_reg, w);

	wlc_bmac_write_shm(wlc_hw, M_IFSCTL1(wlc_hw), (uint16)w);
}

#if defined(WL_PROXDETECT) || defined(WLC_TSYNC)
/**
 * Proximity detection service - enables a way for mobile devices to pair based on relative
 * proximity.
 */
void wlc_enable_avb_timer(wlc_hw_info_t *wlc_hw, bool enable)
{
	osl_t *osh;

	osh = wlc_hw->osh;

	if (enable) {
		OR_REG(osh, D11_ClockCtlStatus(wlc_hw), CCS_AVBCLKREQ);
		OR_REG(osh, D11_MacControl1(wlc_hw), MCTL1_AVB_ENABLE);
	} else {
		AND_REG(osh, D11_ClockCtlStatus(wlc_hw), ~CCS_AVBCLKREQ);
		AND_REG(osh, D11_MacControl1(wlc_hw), ~MCTL1_AVB_ENABLE);
	}

	/* enable/disable the avb timer */
	si_pmu_avb_clk_set(wlc_hw->sih, osh, enable);
}

void wlc_get_avb_timer_reg(wlc_hw_info_t *wlc_hw, uint32 *clkst, uint32 *maccontrol1)
{
	osl_t *osh;

	osh = wlc_hw->osh;

	if (clkst)
		*clkst = R_REG(osh, D11_ClockCtlStatus(wlc_hw));
	if (maccontrol1)
		*maccontrol1 = R_REG(osh, D11_MacControl1(wlc_hw));
}

void wlc_get_avb_timestamp(wlc_hw_info_t *wlc_hw, uint32* ptx, uint32* prx)
{
	osl_t *osh;

	osh = wlc_hw->osh;
	*prx = R_REG(osh, D11_AvbRxTimeStamp(wlc_hw));

	if (D11REV_IS(wlc_hw->corerev, 129))
		OR_REG(osh, D11_MacControl1(wlc_hw), MCTL1_AVB_TRIGGER);
	*ptx = R_REG(osh, D11_AvbTxTimeStamp(wlc_hw));
	if (D11REV_IS(wlc_hw->corerev, 129))
		AND_REG(osh, D11_MacControl1(wlc_hw), ~MCTL1_AVB_TRIGGER);
}
#endif /* WL_PROXDETECT */

/**
 * set the maccontrol register to desired reset state and
 * initialize the sw cache of the register
 */
void
wlc_mctrl_reset(wlc_hw_info_t *wlc_hw)
{
	/* IHR accesses are always enabled, PSM disabled, HPS off and WAKE on */
	wlc_hw->maccontrol = 0;
	memset(&wlc_hw->suspended_fifos, 0, sizeof(wlc_hw->suspended_fifos));
	wlc_hw->suspended_fifo_count = 0;
	wlc_hw->wake_override = 0;
	wlc_hw->mute_override = 0;
	wlc_hw->classify_fifo_suspend_cnt = 0;
	wlc_bmac_mctrl(wlc_hw, ~0, MCTL_IHR_EN | MCTL_WAKE);
}

/** set or clear maccontrol bits */
void
wlc_bmac_mctrl(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val)
{
	uint32 maccontrol;
	uint32 new_maccontrol;

	ASSERT((val & ~mask) == 0);

	maccontrol = wlc_hw->maccontrol;
	new_maccontrol = (maccontrol & ~mask) | val;

	/* if the new maccontrol value is the same as the old, nothing to do */
	if (new_maccontrol == maccontrol)
		return;

	/* something changed, cache the new value */
	wlc_hw->maccontrol = new_maccontrol;

	/* write the new values with overrides applied */
	wlc_mctrl_write(wlc_hw);

#if defined(WL_PSMR1)
	if (PSMR1_ENAB(wlc_hw)) {
		/* Apply PSMR1 mask to both mask and val. Set only if mask
		 * is non-zero.
		 */
		mask = mask & MCTL_PSMR1_MASK;
		val = val & MCTL_PSMR1_MASK;
		if (mask)
			wlc_bmac_mctrlr1(wlc_hw, mask, val);
	}
#endif /* WL_PSMR1 */
}

#if defined(WL_PSMX)
static void
wlc_mctrlx_reset(wlc_hw_info_t *wlc_hw)
{
	ASSERT(PSMX_ENAB(wlc_hw->wlc->pub));

	/* IHR accesses are always enabled, PSM disabled, HPS off and WAKE on */
	wlc_hw->maccontrol_x = 0;
	wlc_bmac_mctrlx(wlc_hw, ~0, MCTL_IHR_EN);
}

void
wlc_bmac_mctrlx(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val)
{
	uint32 maccontrol_x;
	uint32 new_maccontrol;

	ASSERT((val & ~mask) == 0);
	ASSERT(PSMX_ENAB(wlc_hw->wlc->pub));

	maccontrol_x = wlc_hw->maccontrol_x;
	new_maccontrol = (maccontrol_x & ~mask) | val;

	/* if the new maccontrol value is the same as the old, nothing to do */
	if (new_maccontrol == maccontrol_x)
		return;

	/* something changed, cache the new value */
	wlc_hw->maccontrol_x = new_maccontrol;

	/* write the new values with overrides applied */
	W_REG(wlc_hw->osh, D11_MACCONTROL_psmx(wlc_hw), new_maccontrol);
}

/* Check if hardware supports WL_PSMX. */
static bool
wlc_psmx_hw_supported(uint corerev)
{
	return (D11REV_IS(corerev, 65) || D11REV_GE(corerev, 128));
}
#endif /* WL_PSMX */

#if defined(WL_PSMR1)
static void
wlc_write_mhfr1(wlc_hw_info_t *wlc_hw, uint16 *mhfs)
{
	uint8 idx;
	const uint16 addr[] = {M_HOST_FLAGS(wlc_hw), M_HOST_FLAGS2(wlc_hw), M_HOST_FLAGS3(wlc_hw),
		M_HOST_FLAGS4(wlc_hw), M_HOST_FLAGS5(wlc_hw)};

	ASSERT(ARRAYSIZE(addr) == MHFMAX);

	for (idx = 0; idx < MHFMAX; idx++) {
		wlc_bmac_write_shm1(wlc_hw, addr[idx], mhfs[idx]);
	}
}

/* Standalone mac suspend for PSMr1 */
void
wlc_bmac_suspend_macr1_and_wait(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh = wlc_hw->osh;
	uint32 mc1, mi1;
	wlc_info_t *wlc = wlc_hw->wlc;

	WL_TRACE(("wl%d: %s: bandunit %d\n", wlc_hw->unit, __FUNCTION__,
		wlc_hw->band->bandunit));

	/*
	 * Track overlapping suspend requests
	 */
	wlc_hw->macr1_suspend_depth++;
	if (wlc_hw->macr1_suspend_depth > 1) {
		mc1 = R_REG(osh, D11_MACCONTROL_r1(wlc_hw));
		if (mc1 & MCTL_EN_MAC) {
			WL_PRINT(("%s ERROR: suspend_depth %d maccontrol_r1 0x%x\n",
				__FUNCTION__, wlc_hw->macr1_suspend_depth, mc1));

			wlc_dump_psmr1_fatal(wlc, PSMR1_FATAL_SUSP);

			ASSERT(!(mc1 & MCTL_EN_MAC));
		}
		WL_TRACE(("wl%d: %s: bail: macr1_suspend_depth=%d\n", wlc_hw->unit,
			__FUNCTION__, wlc_hw->macr1_suspend_depth));
		return;
	}

	mc1 = R_REG(osh, D11_MACCONTROL_r1(wlc_hw));

	if (mc1 == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}

	ASSERT(!(mc1 & MCTL_PSM_JMP_0));
	ASSERT(mc1 & MCTL_PSM_RUN);
	ASSERT(mc1 & MCTL_EN_MAC);

	mi1 = R_REG(osh, D11_MACINTSTATUS_R1(wlc_hw));
	if (mi1 == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}

	ASSERT(!(mi1 & MI_MACSSPNDD));

	wlc_bmac_mctrlr1(wlc_hw, MCTL_EN_MAC, 0);

	SPINWAIT(!(R_REG(osh, D11_MACINTSTATUS_R1(wlc_hw)) & MI_MACSSPNDD),
		WLC_MAX_MAC_SUSPEND);

	if (!(R_REG(osh, D11_MACINTSTATUS_R1(wlc_hw)) & MI_MACSSPNDD)) {
		WL_PRINT(("wl%d: %s: waited %d uS and "
			 "MI_MACSSPNDD is still not on.\n",
			 wlc_hw->unit, __FUNCTION__, WLC_MAX_MAC_SUSPEND));

		wlc_dump_psmr1_fatal(wlc, PSMR1_FATAL_SUSP);

		WL_HEALTH_LOG(wlc, MACSPEND_TIMOUT);
		wlc_hw->sw_macintstatus |= MI_GP0;
	}

	mc1 = R_REG(osh, D11_MACCONTROL_r1(wlc_hw));
	if (mc1 == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}

	ASSERT(!(mc1 & MCTL_PSM_JMP_0));
	ASSERT(mc1 & MCTL_PSM_RUN);
	ASSERT(!(mc1 & MCTL_EN_MAC));
} /* wlc_bmac_suspend_macr1_and_wait */

/* Standalone mac enable for PSMR1 */
void
wlc_bmac_enable_macr1(wlc_hw_info_t *wlc_hw)
{
	uint32 mc1, mi1;
	osl_t *osh;

	ASSERT(PSMR1_ENAB(wlc_hw));
	WL_TRACE(("wl%d: %s: bandunit %d\n",
		wlc_hw->unit, __FUNCTION__, wlc_hw->band->bandunit));
	/*
	 * Track overlapping suspend requests
	 */
	ASSERT(wlc_hw->macr1_suspend_depth > 0);
	wlc_hw->macr1_suspend_depth--;
	if (wlc_hw->macr1_suspend_depth > 0) {
		return;
	}

	osh = wlc_hw->osh;
	mc1 = R_REG(osh, D11_MACCONTROL_r1(wlc_hw));
	ASSERT(!(mc1 & MCTL_PSM_JMP_0));
	ASSERT(!(mc1 & MCTL_EN_MAC));

	wlc_bmac_mctrlr1(wlc_hw, MCTL_EN_MAC, MCTL_EN_MAC);

	W_REG(osh, D11_MACINTSTATUS_R1(wlc_hw), MI_MACSSPNDD);

	mc1 = R_REG(osh, D11_MACCONTROL_r1(wlc_hw));

	ASSERT(!(mc1 & MCTL_PSM_JMP_0));
	ASSERT(mc1 & MCTL_EN_MAC);
	ASSERT(mc1 & MCTL_PSM_RUN);

	BCM_REFERENCE(mc1);

	mi1 = R_REG(osh, D11_MACINTSTATUS_R1(wlc_hw));

	ASSERT(!(mi1 & MI_MACSSPNDD));
	BCM_REFERENCE(mi1);
}

static void
wlc_bmac_mctrlr1(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val)
{
	uint32 maccontrol_r1;
	uint32 new_maccontrol;

	ASSERT((val & ~mask) == 0);
	ASSERT(PSMR1_ENAB(wlc_hw));

	maccontrol_r1 = wlc_hw->maccontrol_r1;
	new_maccontrol = (maccontrol_r1 & ~mask) | val;

	/* if the new maccontrol value is the same as the old, nothing to do */
	if (new_maccontrol == maccontrol_r1)
		return;

	/* something changed, cache the new value */
	wlc_hw->maccontrol_r1 = new_maccontrol;

	/* write the new values with overrides applied */
	W_REG(wlc_hw->osh, D11_MACCONTROL_r1(wlc_hw), new_maccontrol);
}

/* Check if hardware supports WL_PSMR1. */
static bool
wlc_psmr1_hw_supported(uint corerev)
{
	return (D11REV_IS(corerev, 128) || D11REV_IS(corerev, 129));
}
#endif /* WL_PSMR1 */

/* Write psm_maccommand */
void
wlc_bmac_psm_maccommand(wlc_hw_info_t *wlc_hw, uint32 val)
{
	W_REG(wlc_hw->osh, D11_PSM_MACCOMMAND(wlc_hw), val);
}

/** write the software state of maccontrol and overrides to the maccontrol register */
static void
wlc_mctrl_write(wlc_hw_info_t *wlc_hw)
{
	uint32 maccontrol = wlc_hw->maccontrol;

	/* OR in the wake bit if overridden */
	if (wlc_hw->wake_override)
		maccontrol |= MCTL_WAKE;

	/* set AP and INFRA bits for mute if needed */
	if (wlc_hw->mute_override) {
		maccontrol &= ~(MCTL_AP);
		maccontrol |= MCTL_INFRA;
	}

	W_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw), maccontrol);
}

void
wlc_ucode_wake_override_set(wlc_hw_info_t *wlc_hw, uint32 override_bit)
{
	ASSERT((wlc_hw->wake_override & override_bit) == 0);

	if (wlc_hw->wake_override || (wlc_hw->maccontrol & MCTL_WAKE)) {
		if (wlc_hw->wake_override) {
			ASSERT((wlc_hw->maccontrol & MCTL_WAKE) != 0);
		}
		mboolset(wlc_hw->wake_override, override_bit);
		return;
	}

	mboolset(wlc_hw->wake_override, override_bit);

	wlc_bmac_mctrl(wlc_hw, MCTL_WAKE, MCTL_WAKE);
	wlc_bmac_wait_for_wake(wlc_hw);

	return;
}

void
wlc_ucode_wake_override_clear(wlc_hw_info_t *wlc_hw, uint32 override_bit)
{
	ASSERT(wlc_hw->wake_override & override_bit);

	mboolclr(wlc_hw->wake_override, override_bit);

	if (!wlc_hw->wake_override) {
		/*
		* update HW reg if wake override is cleared. It's assumed that no updated needed
		* if some one else is holding wake_override.
		*/
		wlc_set_wake_ctrl(wlc_hw->wlc);
	} else {
		/* if wake override is ON, MCTL_WAKE must have been set previously */
		ASSERT((wlc_hw->maccontrol & MCTL_WAKE) != 0);
	}
}

/**
 * Prevents ucode from transmitting beacons and probe responses.
 *
 * When driver needs ucode to stop beaconing, it has to make sure that
 * MCTL_AP is clear and MCTL_INFRA is set
 * Mode           MCTL_AP        MCTL_INFRA
 * AP                1              1
 * STA               0              1 <--- This will ensure no beacons
 * IBSS              0              0
 */
static void
wlc_ucode_mute_override_set(wlc_hw_info_t *wlc_hw)
{
	wlc_hw->mute_override = 1;

	/* if maccontrol already has AP == 0 and INFRA == 1 without this
	 * override, then there is no change to write
	 */
	if ((wlc_hw->maccontrol & (MCTL_AP | MCTL_INFRA)) == MCTL_INFRA)
		return;

	wlc_mctrl_write(wlc_hw);

	return;
}

/** Clear the override on AP and INFRA bits */
static void
wlc_ucode_mute_override_clear(wlc_hw_info_t *wlc_hw)
{
	if (wlc_hw->mute_override == 0)
		return;

	wlc_hw->mute_override = 0;

	/* if maccontrol already has AP == 0 and INFRA == 1 without this
	 * override, then there is no change to write
	 */
	if ((wlc_hw->maccontrol & (MCTL_AP | MCTL_INFRA)) == MCTL_INFRA)
		return;

	wlc_mctrl_write(wlc_hw);
}

/**
 * Updates suspended_fifos admin when suspending a txfifo
 * and may set the ucode wake override bit
 */
bool
wlc_upd_suspended_fifos_set(wlc_hw_info_t *wlc_hw, uint txfifo)
{
	ASSERT(txfifo < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc));

	if (!isset(wlc_hw->suspended_fifos, txfifo)) {
		setbit(wlc_hw->suspended_fifos, txfifo);

		if (wlc_hw->suspended_fifo_count++ == 0) {
			wlc_ucode_wake_override_set(wlc_hw, WLC_WAKE_OVERRIDE_TXFIFO);
		}

		ASSERT(wlc_hw->suspended_fifo_count <= WLC_HW_NFIFO_TOTAL(wlc_hw->wlc));
		return TRUE;
	}

	/* Already suspended */
	return FALSE;
}

/**
 * Updates suspended_fifos admin when resuming a txfifo
 * and may clear the ucode wake override bit
 */
void
wlc_upd_suspended_fifos_clear(wlc_hw_info_t *wlc_hw, uint txfifo)
{
	ASSERT(txfifo < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc));

	if (wlc_hw->suspended_fifo_count != 0 && isset(wlc_hw->suspended_fifos, txfifo)) {
		clrbit(wlc_hw->suspended_fifos, txfifo);

		ASSERT(wlc_hw->suspended_fifo_count != 0);
		if (--wlc_hw->suspended_fifo_count == 0) {
			wlc_ucode_wake_override_clear(wlc_hw, WLC_WAKE_OVERRIDE_TXFIFO);
		}
	}
}

/**
 * Add a MAC address to the rcmta memory in the D11 core. This is an associative memory used to
 * quickly compare a received address with a preloaded set of addresses.
 */
void
wlc_bmac_set_rcmta(wlc_hw_info_t *wlc_hw, int idx, const struct ether_addr *addr)
{
	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	ASSERT(idx >= 0);	/* This routine only for non primary interfaces */

	wlc_bmac_copyto_objmem(wlc_hw, (idx * 2) << 2, addr->octet,
		ETHER_ADDR_LEN, OBJADDR_RCMTA_SEL);
}

void
wlc_bmac_get_rcmta(wlc_hw_info_t *wlc_hw, int idx, struct ether_addr *addr)
{
	ASSERT(idx >= 0);	/* This routine only for non primary interfaces */

	wlc_bmac_copyfrom_objmem(wlc_hw, (idx * 2) << 2, addr->octet,
		ETHER_ADDR_LEN, OBJADDR_RCMTA_SEL);
}

/** for d11 rev >= 40, RCMTA was replaced with AMT (Address Match Table) */
void
wlc_bmac_read_amt(wlc_hw_info_t *wlc_hw, int idx, struct ether_addr *addr, uint16 *attr)
{
	uint32 word[2];

	WL_TRACE(("wl%d: %s: idx %d\n", wlc_hw->unit, __FUNCTION__, idx));
	ASSERT(wlc_hw->corerev >= 40);

	wlc_bmac_copyfrom_objmem(wlc_hw, (idx * 2) << 2, word,
		sizeof(word), OBJADDR_AMT_SEL);

	addr->octet[0] = (uint8)word[0];
	addr->octet[1] = (uint8)(word[0] >> 8);
	addr->octet[2] = (uint8)(word[0] >> 16);
	addr->octet[3] = (uint8)(word[0] >> 24);
	addr->octet[4] = (uint8)word[1];
	addr->octet[5] = (uint8)(word[1] >> 8);
	*attr = (word[1] >> 16);
}

void
wlc_bmac_amt_dump(wlc_hw_info_t *wlc_hw)
{
	struct ether_addr addr;
	uint16 attr = 0;

	wlc_bmac_read_amt(wlc_hw, AMT_IDX_MAC, &addr, &attr);
	WL_ERROR(("%s: mac etheraddr %02x:%02x:%02x:%02x:%02x:%02x  attr %x\n", __FUNCTION__,
		addr.octet[0], addr.octet[1],
		addr.octet[2], addr.octet[3],
		addr.octet[4], addr.octet[5], attr));

	attr = 0;
	wlc_bmac_read_amt(wlc_hw, AMT_IDX_BSSID, &addr, &attr);
	WL_ERROR(("%s: bssid etheraddr %02x:%02x:%02x:%02x:%02x:%02x  attr %x\n", __FUNCTION__,
		addr.octet[0], addr.octet[1], addr.octet[2], addr.octet[3],
		addr.octet[4], addr.octet[5], attr));
}

/** Write a MAC address to the AMT (Address Match Table) */
uint16
wlc_bmac_write_amt(wlc_hw_info_t *wlc_hw, int idx, const struct ether_addr *addr, uint16 attr)
{
	uint32 word[2];
	struct ether_addr prev_addr;
	uint16 prev_attr  = 0;

	WL_TRACE(("wl%d: %s: idx %d\n", wlc_hw->unit, __FUNCTION__, idx));
	ASSERT(wlc_hw->corerev >= 40);

	/* Read/Modify/Write unless entry is being disabled */
	wlc_bmac_read_amt(wlc_hw, idx, &prev_addr, &prev_attr);
	if (attr & AMT_ATTR_VALID) {
		attr |= prev_attr;
	} else {
		attr = 0;
	}

	word[0] = (addr->octet[3] << 24) |
	        (addr->octet[2] << 16) |
	        (addr->octet[1] << 8) |
	        addr->octet[0];
	word[1] = (attr << 16) |
	        (addr->octet[5] << 8) |
	        addr->octet[4];

	wlc_bmac_copyto_objmem(wlc_hw, (idx * 2) << 2, word, sizeof(word), OBJADDR_AMT_SEL);

	return prev_attr;
}

/** Write a MAC address to the given match reg offset in the RXE match engine. */
void
wlc_bmac_set_rxe_addrmatch(wlc_hw_info_t *wlc_hw, int match_reg_offset,
	const struct ether_addr *addr)
{
	uint16 mac_l;
	uint16 mac_m;
	uint16 mac_h;
	osl_t *osh;

	WL_TRACE(("wl%d: %s: offset %d\n", wlc_hw->unit, __FUNCTION__,
	          match_reg_offset));

	ASSERT(wlc_hw->corerev < 40);
	ASSERT((match_reg_offset < RCM_SIZE) || (wlc_hw->corerev == 4));

	/* RCM addrmatch is replaced by AMT in d11 rev40 */
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		WL_ERROR(("wl%d: %s: RCM addrmatch not available on corerev >= 40\n",
		          wlc_hw->unit, __FUNCTION__));
		return;
	}
	mac_l = addr->octet[0] | (addr->octet[1] << 8);
	mac_m = addr->octet[2] | (addr->octet[3] << 8);
	mac_h = addr->octet[4] | (addr->octet[5] << 8);

	osh = wlc_hw->osh;

	/* enter the MAC addr into the RXE match registers */
	W_REG(osh, D11_rcm_ctl(wlc_hw), RCM_INC_DATA | match_reg_offset);
	W_REG(osh, D11_rcm_mat_data(wlc_hw), mac_l);
	W_REG(osh, D11_rcm_mat_data(wlc_hw), mac_m);
	W_REG(osh, D11_rcm_mat_data(wlc_hw), mac_h);

} /* wlc_bmac_set_rxe_addrmatch */

static void
wlc_bmac_set_match_mac(wlc_hw_info_t *wlc_hw, const struct ether_addr *addr)
{
	if (D11REV_LT(wlc_hw->corerev, 40)) {
		wlc_bmac_set_rxe_addrmatch(wlc_hw, RCM_MAC_OFFSET, addr);
	} else {
		wlc_bmac_write_amt(wlc_hw, AMT_IDX_MAC, addr, (AMT_ATTR_VALID | AMT_ATTR_A1));
	}
}

static void
wlc_bmac_clear_match_mac(wlc_hw_info_t *wlc_hw)
{
	if (D11REV_LT(wlc_hw->corerev, 40)) {
		wlc_bmac_set_rxe_addrmatch(wlc_hw, RCM_MAC_OFFSET, &ether_null);
	} else {
		wlc_bmac_write_amt(wlc_hw, AMT_IDX_MAC, &ether_null, 0);
	}
}

static void
wlc_bmac_reset_amt(wlc_hw_info_t *wlc_hw)
{
	int i;

	for (i = 0; i < (int)wlc_hw->wlc->pub->max_addrma_idx; i++)
		wlc_bmac_write_amt(wlc_hw, i, &ether_null, 0);
}

/**
 * Template memory is located in the d11 core, and is used by ucode to transmit frames based on a
 * preloaded template.
 */
void
wlc_bmac_templateptr_wreg(wlc_hw_info_t *wlc_hw, int offset)
{
	ASSERT(ISALIGNED(offset, sizeof(uint32)));

	/* Correct the template read pointer according to mac core using templatebase */
	offset = offset + wlc_hw->templatebase;
	W_REG(wlc_hw->osh, D11_XMT_TEMPLATE_RW_PTR(wlc_hw), offset);
}

uint32
wlc_bmac_templateptr_rreg(wlc_hw_info_t *wlc_hw)
{
	return R_REG(wlc_hw->osh, D11_XMT_TEMPLATE_RW_PTR(wlc_hw));
}

void
wlc_bmac_templatedata_wreg(wlc_hw_info_t *wlc_hw, uint32 word)
{
	W_REG(wlc_hw->osh, D11_XMT_TEMPLATE_RW_DATA(wlc_hw), word);
}

uint32
wlc_bmac_templatedata_rreg(wlc_hw_info_t *wlc_hw)
{
	return R_REG(wlc_hw->osh, D11_XMT_TEMPLATE_RW_DATA(wlc_hw));
}

void
wlc_bmac_write_template_ram(wlc_hw_info_t *wlc_hw, int offset, int len, const void *buf)
{
	uint32 word;
	bool be_bit;
#ifdef IL_BIGENDIAN
	volatile uint16 *dptr = NULL;
#endif /* IL_BIGENDIAN */
	osl_t *osh = wlc_hw->osh;

	WL_TRACE(("wl%d: wlc_bmac_write_template_ram\n", wlc_hw->unit));

	ASSERT(ISALIGNED(offset, sizeof(uint32)));
	ASSERT(ISALIGNED(len, sizeof(uint32)));
	ASSERT((offset & ~0xffff) == 0);

	/* The template region starts where the BMC_STARTADDR starts.
	 * This shouldn't use a #defined value but some parameter in a
	 * global struct.
	 */
	if (D11REV_IS(wlc_hw->corerev, 48) || D11REV_IS(wlc_hw->corerev, 49))
		offset += (D11MAC_BMC_STARTADDR_SRASM << 8);
	wlc_bmac_templateptr_wreg(wlc_hw, offset);

#ifdef IL_BIGENDIAN
	if (BUSTYPE(wlc_hw->sih->bustype) == PCMCIA_BUS)
		dptr = (volatile uint16*)D11_XMT_TEMPLATE_RW_DATA((wlc_hw));
#endif /* IL_BIGENDIAN */

	/* if MCTL_BIGEND bit set in mac control register,
	 * the chip swaps data in fifo, as well as data in
	 * template ram
	 */
	be_bit = (R_REG(osh, D11_MACCONTROL(wlc_hw)) & MCTL_BIGEND) != 0;

	while (len > 0) {
		memcpy(&word, (const uint8 *)buf, sizeof(uint32));

		if (be_bit)
			word = hton32(word);
		else
			word = htol32(word);

		wlc_bmac_templatedata_wreg(wlc_hw, word);

		buf = (const uint8 *)buf + sizeof(uint32);
		len -= sizeof(uint32);
	}
} /* wlc_bmac_write_template_ram */

/** contention window related */
void
wlc_bmac_set_cwmin(wlc_hw_info_t *wlc_hw, uint16 newmin)
{
	wlc_hw->band->CWmin = newmin;

	wlc_bmac_copyto_objmem(wlc_hw, S_DOT11_CWMIN << 2, &newmin,
		sizeof(newmin), OBJADDR_SCR_SEL);
}

void
wlc_bmac_set_cwmax(wlc_hw_info_t *wlc_hw, uint16 newmax)
{
	wlc_hw->band->CWmax = newmax;

	wlc_bmac_copyto_objmem(wlc_hw, S_DOT11_CWMAX << 2, &newmax,
		sizeof(newmax), OBJADDR_SCR_SEL);
}

void
wlc_bmac_bw_set(wlc_hw_info_t *wlc_hw, uint16 bw)
{
	phy_info_t *pi = (phy_info_t *)wlc_hw->band->pi;
	chanspec_t chspec = phy_utils_get_chanspec(pi);

	phy_utils_set_bwstate(pi, bw);

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	if (WLCISACPHY(wlc_hw->band) && (HW_PHYRESET_ON_BW_CHANGE == 0)) {
		wlc_bmac_bw_reset(wlc_hw);
	} else {
		wlc_bmac_phy_reset(wlc_hw);
	}

	/* No need to issue init for acphy on bw change */
	phy_chanmgr_bwinit(pi, chspec);
}

static void
wlc_write_hw_bcntemplate0(wlc_hw_info_t *wlc_hw, void *bcn, int len)
{
	uint shm_bcn_tpl0_base;

	if (D11REV_GE(wlc_hw->corerev, 40))
		shm_bcn_tpl0_base = D11AC_T_BCN0_TPL_BASE;
	else
		shm_bcn_tpl0_base = D11_T_BCN0_TPL_BASE;

	wlc_bmac_write_template_ram(wlc_hw, shm_bcn_tpl0_base, (len + 3) & ~3, bcn);
	/* write beacon length to SCR */
	ASSERT(len < 65536);
	wlc_bmac_write_shm(wlc_hw, M_BCN0_FRM_BYTESZ(wlc_hw), (uint16)len);
	/* mark beacon0 valid */
	OR_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw), MCMD_BCN0VLD);
}

static void
wlc_write_hw_bcntemplate1(wlc_hw_info_t *wlc_hw, void *bcn, int len)
{
	uint shm_bcn_tpl1_base;

	if (D11REV_GE(wlc_hw->corerev, 40))
		shm_bcn_tpl1_base = D11AC_T_BCN1_TPL_BASE;
	else
		shm_bcn_tpl1_base = D11_T_BCN1_TPL_BASE;

	wlc_bmac_write_template_ram(wlc_hw, shm_bcn_tpl1_base, (len + 3) & ~3, bcn);
	/* write beacon length to SCR */
	ASSERT(len < 65536);
	wlc_bmac_write_shm(wlc_hw, M_BCN1_FRM_BYTESZ(wlc_hw), (uint16)len);
	/* mark beacon1 valid */
	OR_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw), MCMD_BCN1VLD);
}

/** mac is assumed to be suspended at this point */
void
wlc_bmac_write_hw_bcntemplates(wlc_hw_info_t *wlc_hw, void *bcn, int len, bool both)
{
	if (both) {
		wlc_write_hw_bcntemplate0(wlc_hw, bcn, len);
		wlc_write_hw_bcntemplate1(wlc_hw, bcn, len);
	} else {
		/* bcn 0 */
		if (!(R_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw)) & MCMD_BCN0VLD))
			wlc_write_hw_bcntemplate0(wlc_hw, bcn, len);
		/* bcn 1 */
		else if (!(R_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw)) & MCMD_BCN1VLD))
			wlc_write_hw_bcntemplate1(wlc_hw, bcn, len);
		else	/* one template should always have been available */
			ASSERT(0);
	}
}

/** returns the time it takes to power up the synthesizer */
static uint16
WLBANDINITFN(wlc_bmac_synthpu_dly)(wlc_hw_info_t *wlc_hw)
{
	uint16 v;

	/* return SYNTHPU_DLY */

	/* BMAC_NOTE: changing this code to always return phy specific synthpu delays
	 * instead of checking STA/AP mode and setting a constant 500us delay.
	 * need to check if values should be conditionalize differently, maybe by
	 * radio type, not phy. Original code from wlc_bsinit().
	 * Filed PR53142 to see if STA/AP and infra checks are needed.
	 */

	if (ISSIM_ENAB(wlc_hw->sih)) {
		v = SYNTHPU_DLY_PHY_US_QT;
	} else {

		if (WLCISNPHY(wlc_hw->band)) {
			v = SYNTHPU_DLY_NPHY_US;
		} else if (WLCISLCN20PHY(wlc_hw->band)) {
			v = SYNTHPU_DLY_LCN20PHY_US;
		} else if (WLCISACPHY(wlc_hw->band)) {
			if (BCM4350_CHIP(wlc_hw->sih->chip))
				v = SYNTHPU_DLY_ACPHY2_US;
			else if (D11REV_GE(wlc_hw->corerev, 65))
				v = SYNTHPU_DLY_ACPHY_4365_US;
			else if (BCM53573_CHIP(wlc_hw->sih->chip))
				if (wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_MIMO) {
					if ((PM_BCNRX_ENAB(wlc_hw->wlc->pub) &&
#if defined(STA) && defined(WLPM_BCNRX)
						wlc_pm_bcnrx_allowed(wlc_hw->wlc) &&
#endif // endif
						TRUE) ||
						(wlc_hw->wlc->stf->rxchain == 1))
					v = SYNTHPU_DLY_ACPHY_4349_MIMO_ONECORE_US;
					else
						v = SYNTHPU_DLY_ACPHY_4349_MIMO_US;
				} else {
					v = SYNTHPU_DLY_ACPHY_4349_RSDB_US;
				}
			/* should this follow 63178 instead? Need to check with Kai */
			else if (BCM4347_CHIP(wlc_hw->sih->chip) ||
			    BCM6878_CHIP(wlc_hw->sih->chip))
				v = SYNTHPU_DLY_ACPHY_4347_US;
			else if (BCM4369_CHIP(wlc_hw->sih->chip))
				v = SYNTHPU_DLY_ACPHY_4369_US;
			else
				v = SYNTHPU_DLY_ACPHY_US;
		} else {
			v = SYNTHPU_DLY_BPHY_US;
		}
	}

	return v;
} /* wlc_bmac_synthpu_dly */

static void
WLBANDINITFN(wlc_bmac_upd_synthpu)(wlc_hw_info_t *wlc_hw)
{
	uint16 v = wlc_bmac_synthpu_dly(wlc_hw);
	wlc_bmac_write_shm(wlc_hw, M_SYNTHPU_DELAY(wlc_hw), v);
}

void wlc_bmac_update_synthpu(wlc_hw_info_t *wlc_hw)
{
	wlc_bmac_upd_synthpu(wlc_hw);
}

void
wlc_bmac_set_extlna_pwrsave_shmem(wlc_hw_info_t *wlc_hw)
{
	uint16 extlna_pwrctl = 0x480;

	BCM_REFERENCE(extlna_pwrctl);

	if (D11REV_LT(wlc_hw->corerev, 40))
		wlc_bmac_write_shm(wlc_hw, M_EXTLNA_PWRSAVE(wlc_hw), extlna_pwrctl);
}

/** AX PHY in hardware is considered an AC PHY within the PHYSW subsystem */
static uint32
wlc_bmac_get_phy_type_from_reg(wlc_hw_info_t *wlc_hw)
{
	uint32 phy_type = PHY_TYPE(R_REG(wlc_hw->osh, D11_PHY_REG_0(wlc_hw)));
	phy_type = (phy_type == PHY_TYPE_AX ? PHY_TYPE_AC : phy_type);
	return phy_type;
}

/** band-specific init */
static void
WLBANDINITFN(wlc_bmac_bsinit)(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool chanswitch_path)
{
	wlc_info_t *wlc = wlc_hw->wlc;

	(void)wlc;

	WL_TRACE(("wl%d: wlc_bmac_bsinit: bandunit %d\n", wlc_hw->unit, wlc_hw->band->bandunit));

	ASSERT(wlc_bmac_get_phy_type_from_reg(wlc_hw) == PHY_TYPE_LCNXN ||
	       wlc_bmac_get_phy_type_from_reg(wlc_hw) == wlc_hw->band->phytype);

	wlc_ucode_bsinit(wlc_hw);

#ifdef BCMULP
	/* ILP slow cal skip during warm boot */
	if (si_is_warmboot() && BCMULP_ENAB()) {
		ulp_p1_module_register(ULP_MODULE_ID_BMAC,
			&wlc_bmac_p1_ctx, (void*)wlc_hw);
	}
#endif /* BCMULP */

	/* phymode switch requires phyinit */
	phy_chanmgr_bsinit((phy_info_t *)wlc_hw->band->pi, chanspec,
		(!chanswitch_path) || phy_init_pending((phy_info_t *)wlc_hw->band->pi));

	wlc_ucode_txant_set(wlc_hw);

	/* cwmin is band-specific, update hardware with value for current band */
	wlc_bmac_set_cwmin(wlc_hw, wlc_hw->band->CWmin);
	wlc_bmac_set_cwmax(wlc_hw, wlc_hw->band->CWmax);

	wlc_bmac_update_slot_timing(wlc_hw,
		BAND_5G(wlc_hw->band->bandtype) ? TRUE : wlc_hw->shortslot);

	/* write phytype and phyvers */
	wlc_bmac_write_shm(wlc_hw, M_PHYTYPE(wlc_hw), (uint16)wlc_hw->band->phytype);
	wlc_bmac_write_shm(wlc_hw, M_PHYVER(wlc_hw), (uint16)wlc_hw->band->phyrev);

#ifdef WL11N
	/* initialize the txphyctl1 rate table since shmem is shared between bands */
	wlc_upd_ofdm_pctl1_table(wlc_hw);
#endif // endif

	wlc_bmac_upd_synthpu(wlc_hw);

	/* Configure BTC GPIOs as bands change */
	if (BAND_5G(wlc_hw->band->bandtype))
		wlc_bmac_mhf(wlc_hw, MHF5, MHF5_BTCX_DEFANT, MHF5_BTCX_DEFANT, WLC_BAND_ALL);
	else
		wlc_bmac_mhf(wlc_hw, MHF5, MHF5_BTCX_DEFANT, 0, WLC_BAND_ALL);

#if defined(WLRSDB) && defined(BCMECICOEX)
	/* Update coex_io_mask on band switch for corerev 50 only */
	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		wlc_cmn_info_t* wlc_cmn = wlc->cmn;
		wlc_info_t *wlc_iter;
		int idx;
		/* update coex_io_mask for all the cores */
		FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
			wlc_btcx_update_coex_iomask(wlc_iter);
		}
	}
	if (WLC_DUALMAC_RSDB(wlc->cmn)) {
		/* 4364: Update COEX GCI mask */
		wlc_btcx_update_gci_mask(wlc);
	}
#endif /* WLRSDB && BCMECICOEX */

#if defined(BCMECICOEX) && defined(WLC_SW_DIVERSITY)
	/* Update GCI on SWDIV */
#ifdef WLRSDB
	if (wlc_rsdb_mode(wlc) == PHYMODE_MIMO)
#endif /* WLRSDB */
	{
		si_gci_direct(wlc->pub->sih, GCI_OFFSETOF(wlc->pub->sih, gci_control_1),
			GCI_WL_SWDIV_ANT_VALID_BIT_MASK, GCI_WL_SWDIV_ANT_VALID_BIT_MASK);
		if (BAND_5G(wlc_hw->band->bandtype)) {
			si_gci_direct(wlc->pub->sih, GCI_OFFSETOF(wlc->pub->sih, gci_output[1]),
				GCI_WL_SWDIV_ANT_VALID_BIT_MASK,
				((WLSWDIV_ENAB(wlc) &&
					wlc_swdiv_supported(wlc->swdiv, 0, FALSE, FALSE)) <<
					GCI_SWDIV_ANT_VALID_SHIFT));
		} else	{
			si_gci_direct(wlc->pub->sih, GCI_OFFSETOF(wlc->pub->sih, gci_output[1]),
				GCI_WL_SWDIV_ANT_VALID_BIT_MASK,
				((WLSWDIV_ENAB(wlc) &&
					wlc_swdiv_supported(wlc->swdiv, 0, TRUE, FALSE)) <<
					GCI_SWDIV_ANT_VALID_SHIFT));
		}
	}
#endif /* BCMECICOEX && WLC_SW_DIVERSITY */

	wlc_bmac_set_extlna_pwrsave_shmem(wlc_hw);
	if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
		wlc_ratelinkmem_update_band(wlc);
	}
} /* wlc_bmac_bsinit */

/**
 * Helper API to apply the phy reset on d11 core unit one for RSDB chip.
 * This is required while there is no core one init happens for cases like MIMO,
 * 80p80 mode.
 */
static void
wlc_bmac_4349_btcx_prisel_war(wlc_hw_info_t *wlc_hw)
{
	int sicoreunit = 0;

	ASSERT(wlc_hw != NULL);
	sicoreunit = si_coreunit(wlc_hw->sih);

	/* Apply the setting to D11 core unit one always */
	if ((wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_MIMO) ||
	(wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_80P80)) {
		WL_INFORM(("MIMO: PRISEL ISSUE WORKAROUND\n"));
		si_d11_switch_addrbase(wlc_hw->sih, 1);
		si_core_cflags(wlc_hw->sih,
			(SICF_MCLKE | SICF_FCLKON | SICF_PCLKE | SICF_PRST | SICF_MPCLKE),
			(SICF_MCLKE | SICF_FCLKON | SICF_PCLKE | SICF_PRST));
	}

	si_d11_switch_addrbase(wlc_hw->sih, sicoreunit);
}

void
wlc_bmac_core_phy_clk(wlc_hw_info_t *wlc_hw, bool clk)
{
	WL_TRACE(("wl%d: wlc_bmac_core_phy_clk: clk %d\n", wlc_hw->unit, clk));

	wlc_hw->phyclk = clk;

	if (OFF == clk) {
		/* CLEAR GMODE BIT, PUT PHY INTO RESET */

		si_core_cflags(wlc_hw->sih,
			(SICF_PRST | SICF_FGC | SICF_GMODE),
			(SICF_PRST | SICF_FGC));
		OSL_DELAY(1);

		si_core_cflags(wlc_hw->sih, (SICF_PRST | SICF_FGC), SICF_PRST);
		OSL_DELAY(1);
	} else {
		/* TAKE PHY OUT OF RESET */

		/* High Speed DAC Configuration */
		if (D11REV_GE(wlc_hw->corerev, 40)) {
			si_core_cflags(wlc_hw->sih, SICF_DAC, 0x100);
			/* Special PHY RESET Sequence for ACPHY to ensure correct Clock Alignment */
			if (BCM53573_CHIP(wlc_hw->sih->chip) &&
#ifdef WLRSDB
				WLC_RSDB_SINGLE_MAC_MODE(WLC_RSDB_CURR_MODE(wlc_hw->wlc)) &&
#endif // endif
				wlc_hw->macunit == 0) {

				si_core_cflags(wlc_hw->sih, (SICF_PRST | SICF_FGC | SICF_PCLKE), 0);
				OSL_DELAY(1);

				si_pmu_chipcontrol(wlc_hw->sih, 5,
						CC5_4349_MAC_PHY_CLK_8_DIV,
						CC5_4349_MAC_PHY_CLK_8_DIV);

				OSL_DELAY(1);

				if (wlc_bmac_rsdb_cap(wlc_hw)) {
					if (!RSDB_ENAB(wlc_hw->wlc->pub) &&
					(wlc_hw->core1_mimo_reset == 0)) {
						wlc_bmac_4349_btcx_prisel_war(wlc_hw);
						wlc_hw->core1_mimo_reset = 1;
					}
				}
				si_core_cflags(wlc_hw->sih, SICF_PCLKE, SICF_PCLKE);
				OSL_DELAY(1);

				si_pmu_chipcontrol(wlc_hw->sih, 5,
						CC5_4349_MAC_PHY_CLK_8_DIV, 0);
				OSL_DELAY(1);

			} else {
				/* turn off phy clocks and bring out of reset */
				si_core_cflags(wlc_hw->sih, (SICF_PRST | SICF_FGC | SICF_PCLKE), 0);
				OSL_DELAY(1);

				/*
				 * CRWLDOT11M-1403: Enabling Core 1 PHY reset bit
				 * in Core 1 ioctrl for dot11macphy_prisel to PHY
				 */
				if (wlc_bmac_rsdb_cap(wlc_hw)) {
					if (!RSDB_ENAB(wlc_hw->wlc->pub) &&
					(wlc_hw->core1_mimo_reset == 0)) {
						wlc_bmac_4349_btcx_prisel_war(wlc_hw);
						wlc_hw->core1_mimo_reset = 1;
					}
				}

				/* reenable phy clocks to resync to mac mac clock */
				si_core_cflags(wlc_hw->sih, SICF_PCLKE, SICF_PCLKE);
				OSL_DELAY(1);
			}
		} else {
			/* turn off phy clocks */
			si_core_cflags(wlc_hw->sih,
				(SICF_PRST | SICF_FGC | SICF_PCLKE),
				SICF_FGC);
			OSL_DELAY(1);

			/* reenable phy clocks to resync to mac mac clock */
			si_core_cflags(wlc_hw->sih, (SICF_FGC | SICF_PCLKE), SICF_PCLKE);
			OSL_DELAY(1);
		}
	}
} /* wlc_bmac_core_phy_clk */

/** Perform a soft reset of the PHY PLL */
void
wlc_bmac_core_phypll_reset(wlc_hw_info_t *wlc_hw)
{
	WL_TRACE(("wl%d: wlc_bmac_core_phypll_reset\n", wlc_hw->unit));

	if (WLCISNPHY(wlc_hw->band)) {

		si_corereg(wlc_hw->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_addr), ~0, 0);
		OSL_DELAY(1);
		si_corereg(wlc_hw->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_data), 0x4, 0);
		OSL_DELAY(1);
		si_corereg(wlc_hw->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_data), 0x4, 4);
		OSL_DELAY(1);
		si_corereg(wlc_hw->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_data), 0x4, 0);
		OSL_DELAY(1);
	}
}

/**
 * light way to turn on phy clock without reset for NPHY only
 *  refer to wlc_bmac_core_phy_clk for full version
 */
void
wlc_bmac_phyclk_fgc(wlc_hw_info_t *wlc_hw, bool clk)
{
	/* support(necessary for NPHY) only */
	if (!WLCISNPHY(wlc_hw->band) && !WLCISACPHY(wlc_hw->band))
		return;

	if (ON == clk)
		si_core_cflags(wlc_hw->sih, SICF_FGC, SICF_FGC);
	else
		si_core_cflags(wlc_hw->sih, SICF_FGC, 0);

}

void
wlc_bmac_macphyclk_set(wlc_hw_info_t *wlc_hw, bool clk)
{
	if (ON == clk)
		si_core_cflags(wlc_hw->sih, SICF_MPCLKE, SICF_MPCLKE);
	else
		si_core_cflags(wlc_hw->sih, SICF_MPCLKE, 0);
}

static uint32
wlc_bmac_clk_bwbits(wlc_hw_info_t *wlc_hw)
{
	uint32 phy_bw_clkbits = 0;

	/* select the phy speed according to selected channel b/w */
	switch (CHSPEC_BW(wlc_hw->chanspec)) {
	case WL_CHANSPEC_BW_10:
		phy_bw_clkbits = SICF_BW10;
		break;
	case WL_CHANSPEC_BW_20:
		phy_bw_clkbits = SICF_BW20;
		break;
	case WL_CHANSPEC_BW_40:
		phy_bw_clkbits = SICF_BW40;
		break;
	case WL_CHANSPEC_BW_80:
	case WL_CHANSPEC_BW_8080:
		phy_bw_clkbits = SICF_BW80;
		break;
	case WL_CHANSPEC_BW_160:
		if (WLC_PHY_AS_80P80(wlc_hw, wlc_hw->chanspec)) {
			phy_bw_clkbits = SICF_BW80;
		} else {
			phy_bw_clkbits = SICF_BW160;
		}
		break;
	default:
		ASSERT(0);	/* should never get here */
	}

	return phy_bw_clkbits;
}

void
wlc_bmac_phy_reset(wlc_hw_info_t *wlc_hw)
{
	uint16 val, val_phyctl, mpclke, phyfgc;
	uint32 phy_bw_clkbits;
	int16 tssival[WLC_TXCORE_MAX] = {0};
	wlc_info_t *wlc;
	osl_t *osh;

	wlc =  wlc_hw->wlc;
	osh = wlc_hw->osh;

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	ASSERT(wlc_hw->band != NULL);

	phy_bw_clkbits = wlc_bmac_clk_bwbits(wlc_hw);

	if (WLCISNPHY(wlc_hw->band) && NREV_GE(wlc_hw->band->phyrev, 3) &&
		NREV_LE(wlc_hw->band->phyrev, 4)) {
		/* Set the PHY bandwidth */
		si_core_cflags(wlc_hw->sih, SICF_BWMASK, phy_bw_clkbits);

		OSL_DELAY(1);

		/* Perform a soft reset of the PHY PLL */
		wlc_bmac_core_phypll_reset(wlc_hw);

		/* reset the PHY */
		si_core_cflags(wlc_hw->sih, (SICF_PRST | SICF_PCLKE),
			(SICF_PRST | SICF_PCLKE));
	} else if (WLCISACPHY(wlc_hw->band)) {
		if (ACREV_IS(wlc->band->phyrev, 47) || ACREV_IS(wlc->band->phyrev, 51) ||
			ACREV_IS(wlc->band->phyrev, 129)) {
			/* read idletssi before phy_reset */
			wlc_idletssi_force((phy_info_t *)wlc_hw->band->pi, tssival, TRUE);
		}
		/* for 43684B1, phy_reset will go through MAC
		 * instread of HOST, JIRA: CRBCAD11MAC-4799
		 */
		if ((wlc_hw->band->phyrev == 47) && (wlc_hw->band->phy_minor_rev == 2)) {
			mpclke = (si_core_cflags(wlc_hw->sih, 0, 0) & SICF_MPCLKE) >>
				SICF_MPCLKE_SHIFT;
			if (mpclke == 0) {
				si_core_cflags(wlc_hw->sih, (SICF_MPCLKE | SICF_BWMASK),
					(SICF_MPCLKE | phy_bw_clkbits));
			} else {
				si_core_cflags(wlc_hw->sih, SICF_BWMASK, phy_bw_clkbits);
			}

			/* phyreset through MAC */
			val = R_REG(osh, D11_PSM_PHY_CTL(wlc));
			ASSERT((val & PHYCTL_PHYRST) >> PHYCTL_PHYRST_SHIFT == 0);
			phyfgc = (val & PHYCTL_PHYFGC) >> PHYCTL_PHYFGC_SHIFT;

			/* for 43684B1, FGC is toggled here */
			/* for other chips, it is done in wlc_bmac_core_phy_clk */
			if (phyfgc == 0) {
				W_REG(osh, D11_PSM_PHY_CTL(wlc),
					(val | PHYCTL_PHYRST | PHYCTL_PHYFGC));
				OSL_DELAY(2);
				W_REG(osh, D11_PSM_PHY_CTL(wlc),
					(val & ~(PHYCTL_PHYRST | PHYCTL_PHYFGC)));
			} else {
				W_REG(osh, D11_PSM_PHY_CTL(wlc), (val | PHYCTL_PHYRST));
				OSL_DELAY(2);
				W_REG(osh, D11_PSM_PHY_CTL(wlc), (val & ~(PHYCTL_PHYRST)));
			}

			/* restore initial SICF_MPCLKE bit */
			if (mpclke == 0) {
				si_core_cflags(wlc_hw->sih, SICF_MPCLKE, 0);
			}
		} else {
			si_core_cflags(wlc_hw->sih,
				(SICF_PRST | SICF_PCLKE | SICF_BWMASK| SICF_FGC),
				(SICF_PRST | SICF_PCLKE | phy_bw_clkbits| SICF_FGC));
		}
	} else {
		si_core_cflags(wlc_hw->sih, (SICF_PRST | SICF_PCLKE | SICF_BWMASK),
			(SICF_PRST | SICF_PCLKE | phy_bw_clkbits));
	}

	OSL_DELAY(2);
	wlc_bmac_core_phy_clk(wlc_hw, ON);

	/* Reset SMC after PHYRESET */
	if ((wlc_hw->band->phyrev == 47) ||
		(wlc_hw->band->phyrev == 51)) {
		val_phyctl = R_REG(osh, D11_PSM_PHY_CTL(wlc));
		W_REG(osh, D11_PSM_PHY_CTL(wlc), (val_phyctl | PHYCTL_PHYRSTSMC_GE128));

		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), PHYREG_SMCBUFFERADDR);
		val = R_REG(wlc->osh, D11_PHY_REG_DATA(wlc));
		W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), val | PHYREG_SMCBUFFERADDR_M2SRST);

		W_REG(osh, D11_PSM_PHY_CTL(wlc), (val_phyctl & ~(PHYCTL_PHYRSTSMC_GE128)));
	}
	/* Rapid-fire wl down/up test loops require additional
	 * delay for 11ac2's internal PHY PLL to lock
	 */
	if (D11REV_GE(wlc_hw->corerev, 65)) {
		OSL_DELAY(100);
	}

	/* Must ensure that BW setting in pi->bw and SiCoreFlags are in Sync. There is a chance
	 * for such a sync loss on wlc_up() path. Currently in this function wlc_hw->chanspec is
	 * used to configure BW in sicoreflags and homechanspec gets reset to different value in
	 * wlc_init(). This can lead to a possible sync loss in BW, hence setting pi->bw here to
	 * avoid such issues.
	 */
	if (wlc_hw->band->phyrev != 17) {
		phy_utils_set_bwstate((phy_info_t *)wlc_hw->band->pi, CHSPEC_BW(wlc_hw->chanspec));
	}
	/* XXX: ??? moving this to phy init caused a calibration failure, even after
	 *	I added 10 ms delay after turning on the analog core tx/rx
	 */
	if (wlc_hw->band->pi != NULL)
		phy_ana_reset((phy_info_t *)wlc_hw->band->pi);
	/* restore idletssi after phy_reset to keep TPC healthy */
	if (WLCISACPHY(wlc_hw->band) && (ACREV_IS(wlc->band->phyrev, 47) ||
		ACREV_IS(wlc->band->phyrev, 51) || ACREV_IS(wlc->band->phyrev, 129))) {
		wlc_idletssi_force((phy_info_t *)wlc_hw->band->pi, tssival, FALSE);
	}

#ifdef WL_BEAMFORMING
	if (D11_CHKWAR_BFD_TXVMEM_RESET(wlc_hw) && TXBF_ENAB(wlc_hw->wlc->pub) &&
		wlc_txbf_get_txbf_tx(wlc_hw->wlc->txbf)) {
		wlc_txbf_sounding_clean_cache(wlc_hw->wlc->txbf);
	}
#endif /* WL_BEAMFORMING */
} /* wlc_bmac_phy_reset */

void
wlc_bmac_bw_reset(wlc_hw_info_t *wlc_hw)
{
	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	si_core_cflags(wlc_hw->sih, SICF_BWMASK, wlc_bmac_clk_bwbits(wlc_hw));
}

static void
BCMATTACHFN(wlc_vasip_preattach_init)(wlc_hw_info_t *wlc_hw)
{
#if defined(WLVASIP)
	uint32 vasipver;

	if (wlc_vasip_support(wlc_hw, &vasipver, TRUE)) {
		wlc_vasip_init(wlc_hw, vasipver, TRUE);
	}
#endif /* WLVASIP */
}

static void
BCMATTACHFN(wlc_bmac_phy_reset_preattach)(wlc_hw_info_t *wlc_hw)
{
	uint32 phy_bw_clkbits = SICF_BW20;

	si_core_cflags(wlc_hw->sih, (SICF_PRST | SICF_PCLKE | SICF_BWMASK| SICF_FGC),
	               (SICF_PRST | SICF_PCLKE | phy_bw_clkbits| SICF_FGC));

	OSL_DELAY(2);
	wlc_bmac_core_phy_clk_preattach(wlc_hw);
} /* wlc_bmac_phy_reset_preattach */

static void
BCMATTACHFN(wlc_bmac_core_phy_clk_preattach)(wlc_hw_info_t *wlc_hw)
{
	/* High Speed DAC Configuration */
	si_core_cflags(wlc_hw->sih, SICF_DAC, 0x100);
	/* Bring out of reset before disabling phy clock */
	si_core_cflags(wlc_hw->sih, SICF_PRST, 0);
	OSL_DELAY(1);
	/* Turn off phy clocks */
	si_core_cflags(wlc_hw->sih, (SICF_FGC | SICF_PCLKE), 0);
	OSL_DELAY(1);
	/* reenable phy clocks to resync to mac mac clock */
	si_core_cflags(wlc_hw->sih, SICF_PCLKE, SICF_PCLKE);
	OSL_DELAY(1);
} /* wlc_bmac_core_phy_clk_preattach */

/** switch to and initialize d11 + PHY for operation on caller supplied band */
static void
WLBANDINITFN(wlc_bmac_setband)(wlc_hw_info_t *wlc_hw, uint bandunit, chanspec_t chanspec)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint32 macintmask;

	ASSERT(NBANDS_HW(wlc_hw) > 1);
	ASSERT(bandunit != wlc_hw->band->bandunit);

	WL_TSLOG(wlc_hw->wlc, __FUNCTION__, TS_ENTER, 0);

	/* XXX WES: Why does setband need to check for a disabled core?
	 * Should it always be called when up?
	 * Looks like down a few lines there is an up check, but if down there should be no
	 * touching of the core in setband_inact(), so there should be no iscoreup() check
	 * here.
	 */
	/* Enable the d11 core before accessing it */
	if (!si_iscoreup(wlc_hw->sih)) {
		wlc_bmac_core_reset(wlc_hw, 0, 0);
		ASSERT(si_iscoreup(wlc_hw->sih));
		wlc_mctrl_reset(wlc_hw);
	}

	macintmask = wlc_setband_inact(wlc_hw, bandunit);

	if (!wlc_hw->up) {
		WL_TSLOG(wlc_hw->wlc, __FUNCTION__, TS_EXIT, 0);
		return;
	}

	if (!(WLCISACPHY(wlc_hw->band)))
		wlc_bmac_core_phy_clk(wlc_hw, ON);

	/* band-specific initializations */
	wlc_bmac_bsinit(wlc_hw, chanspec, TRUE);

	/*
	 * If there are any pending software interrupt bits,
	 * then replace these with a harmless nonzero value
	 * so wlc_dpc() will re-enable interrupts when done.
	 */
	if (wlc_hw->macintstatus)
		wlc_hw->macintstatus = MI_DMAINT;

	/* restore macintmask */
	wl_intrsrestore(wlc->wl, macintmask);

	/* ucode should still be suspended.. */
	ASSERT((R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC) == 0);
	WL_TSLOG(wlc_hw->wlc, __FUNCTION__, TS_EXIT, 0);
} /* wlc_bmac_setband */

/** low-level band switch utility routine */
static void
WLBANDINITFN(wlc_setxband)(wlc_hw_info_t *wlc_hw, uint bandunit)
{
	WL_TRACE(("wl%d: wlc_setxband: bandunit %d\n", wlc_hw->unit, bandunit));

	wlc_hw->band = wlc_hw->bandstate[bandunit];

	/* Update the wlc->band pointer for monolithic driver */
	wlc_pi_band_update(wlc_hw->wlc, bandunit);

	/* set gmode core flag */
	if (wlc_hw->sbclk && !wlc_hw->noreset) {
		si_core_cflags(wlc_hw->sih, SICF_GMODE, ((bandunit == 0) ? SICF_GMODE : 0));
	}
}

static bool
BCMATTACHFN(wlc_isgoodchip)(wlc_hw_info_t *wlc_hw)
{
	/* reject unsupported corerev */
	if (!VALID_COREREV((int)wlc_hw->corerev)) {
		WL_ERROR(("unsupported core rev %d\n", wlc_hw->corerev));
		return FALSE;
	}

	return TRUE;
}

static bool
BCMATTACHFN(wlc_validboardtype)(wlc_hw_info_t *wlc_hw)
{
	bool goodboard = TRUE;
	uint boardtype = wlc_hw->sih->boardtype;
	uint boardrev = wlc_hw->boardrev;

	if (boardrev == 0)
		goodboard = FALSE;
	else if (boardrev > 0xff) {
		uint brt = (boardrev & 0xf000) >> 12;
		uint b0 = (boardrev & 0xf00) >> 8;
		uint b1 = (boardrev & 0xf0) >> 4;
		uint b2 = boardrev & 0xf;

		if ((brt > 2) || (brt == 0) || (b0 > 9) || (b0 == 0) || (b1 > 9) || (b2 > 9))
			goodboard = FALSE;
	}

	if (wlc_hw->sih->boardvendor != VENDOR_BROADCOM)
		return goodboard;

	if ((boardtype == BCM94306MP_BOARD) || (boardtype == BCM94306CB_BOARD)) {
		if (boardrev < 0x40)
			goodboard = FALSE;
	} else if (boardtype == BCM94309MP_BOARD) {
		goodboard = FALSE;
	} else if (boardtype == BCM94309G_BOARD) {
		if (boardrev < 0x51)
			goodboard = FALSE;
	}
	return goodboard;
}

static char *
BCMATTACHFN(wlc_get_macaddr)(wlc_hw_info_t *wlc_hw, uint unit)
{
	const char *varname = rstr_macaddr;
	char ifvarname[16];
	char *macaddr;

	/* Check nvram for the presence of an interface specific macaddr.
	 * Interface specific mac addresses should be of the form wlx_macaddr, where x is the
	 * unit number of the interface
	 */
	snprintf(ifvarname, sizeof(ifvarname), "wl%d_%s", unit, rstr_macaddr);
	if ((macaddr = getvar(wlc_hw->vars, ifvarname)) != NULL)
		return macaddr;

	/* Fallback to get the default macaddr.
	 * If macaddr exists, use it (Sromrev4, CIS, ...).
	 */
	if ((macaddr = getvar(wlc_hw->vars, varname)) != NULL)
		return macaddr;

#ifndef BCMSMALL
	/*
	 * Take care of our legacy: MAC addresses can not change
	 * during sw upgrades!
	 * 4309B0 dualband:  il0macaddr
	 * other  dualband:  et1macaddr
	 * uniband-A cards:  et1macaddr
	 * else:             il0macaddr
	 */
	if (NBANDS_HW(wlc_hw) > 1)
		varname = rstr_et1macaddr;
	else
		varname = rstr_il0macaddr;

	if ((macaddr = getvar(wlc_hw->vars, varname)) == NULL) {
		WL_ERROR(("wl%d: %s: macaddr getvar(%s) not found\n",
			wlc_hw->unit, __FUNCTION__, varname));
	}
#endif /* !BCMSMALL */

	return macaddr;
}

/**
 * Return TRUE if radio is disabled, otherwise FALSE.
 * hw radio disable signal is an external pin, users activate it asynchronously
 * this function could be called when driver is down and w/o clock
 * it operates on different registers depending on corerev and boardflag.
 */
bool
wlc_bmac_radio_read_hwdisabled(wlc_hw_info_t* wlc_hw)
{
	bool v, clk, xtal;

	xtal = wlc_hw->sbclk;
	if (!xtal)
		wlc_bmac_xtal(wlc_hw, ON);

	/* may need to take core out of reset first */
	clk = wlc_hw->clk;
	if (!clk) {
		wlc_bmac_core_reset(wlc_hw, SICF_PCLKE, 0);
		wlc_mctrl_reset(wlc_hw);
	}

	v = ((R_REG(wlc_hw->osh, D11_PHY_DEBUG(wlc_hw)) & PDBG_RFD) != 0);

	/* put core back into reset */
	if (!clk && v) {
		wlc_bmac_core_disable(wlc_hw, 0);
	}

	if (!xtal)
		wlc_bmac_xtal(wlc_hw, OFF);

	return (v);
}

/**
 * JIRA: CRWLPCIEGEN2-17
 *
 * In order to work around the hardware issue, we advertise our device as PCIE gen1 capability in
 * the enumeration stage and set to it gen2 after setting correct xtal frequency value when driver
 * attaches. The function does following things.
 *
 * (1)	Determine the link capability of PCIE Root Complex. Returns if it is gen1.
 * (2)	Determine the link capability of 4360 device. Return if it is already gen2
 * (3)	Save the PCIE configuration space of 4360 device.
 * (4)	Update the xtal frequency to 40MHz in PMU
 * (5)	Issue a PMU watchdog reset
 * (6)	Trigger a hot reset of PCIE root complex
 * (7)	Restore the PCIE configuration space of 4360 device.
 * (8)	Set the link capability of 4360 device to gen2
 *
 */
int
wlc_bmac_4360_pcie2_war(wlc_hw_info_t* wlc_hw, uint32 vcofreq)
{
	extern int do_4360_pcie2_war;
	uint32 xtalfreqi;
	uint32 p1div;
	uint32 xtalfreq1;
	uint32 ndiv_int;
	uint32 is_frac;
	uint32 ndiv_mode;
	uint32 val;
	uint32 data;
	int linkspeed;
	int ret = BCME_OK;

	if (((CHIPID(wlc_hw->sih->chip) != BCM4360_CHIP_ID) &&
	     (CHIPID(wlc_hw->sih->chip) != BCM43460_CHIP_ID) &&
	     (CHIPID(wlc_hw->sih->chip) != BCM4352_CHIP_ID)) ||
	    (CHIPREV(wlc_hw->sih->chiprev) > 2) ||
	    (BUSTYPE(wlc_hw->sih->bustype) != PCI_BUS))
		return ret;

#if !defined(__mips__) && !defined(BCM47XX_CA9)
	if (wl_osl_pcie_rc(wlc_hw->wlc->wl, 0, 0) == 1)	/* pcie gen 1 */
		return ret;
#endif /* !defined(__mips__) && !defined(BCM47XX_CA9) */

	if (do_4360_pcie2_war != 0)
		return ret;

	do_4360_pcie2_war = 1;

	si_corereg(wlc_hw->sih, 3, 0x120, ~0, 0xBC);
	data = si_corereg(wlc_hw->sih, 3, 0x124, 0, 0);
	linkspeed = (data >> 16) & 0xf;

	/* don't need the WAR if linkspeed is already gen2 */
	if (linkspeed == 2)
		return ret;

	/* Save PCI cfg space. (cfg offsets 0x0 - 0x3f) */
	ret = si_pcie_configspace_cache((si_t *)(uintptr)(wlc_hw->sih));
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: Unable to save PCIe Configuration "
			"space\n", wlc_hw->unit, __FUNCTION__));
		return ret;
	}

	xtalfreqi = 40;
	p1div = 2;
	xtalfreq1 = xtalfreqi / p1div;
	ndiv_int = vcofreq / xtalfreq1;
	is_frac = (vcofreq % xtalfreq1) > 0 ? 1 : 0;
	ndiv_mode = is_frac ? 3 : 0;
	val = (ndiv_int << 7) | (ndiv_mode << 4) | (p1div << 0);

	si_pmu_pllcontrol(wlc_hw->sih, 10, ~0, val);

	if (is_frac) {
		uint32 frac = (vcofreq % xtalfreq1) * (1 << 24) / xtalfreq1;
		si_pmu_pllcontrol(wlc_hw->sih, 11, ~0, frac);
	}

	/* update pll */
	si_pmu_pllupd(wlc_hw->sih);

	/* Issuing Watchdog Reset */
	si_watchdog(wlc_hw->sih, 2);
	OSL_DELAY(2000);

	/* hot reset */
#if !defined(__mips__) && !defined(BCM47XX_CA9)
	wl_osl_pcie_rc(wlc_hw->wlc->wl, 1, 0);
#endif /* !defined(__mips__) && !defined(BCM47XX_CA9) */
	OSL_DELAY(50 * 1000);

	ret = si_pcie_configspace_restore((si_t *)(uintptr)(wlc_hw->sih));
	if (ret != BCME_OK) {
		WL_ERROR(("wl%d: %s: Unable to Restore PCIe Configuration "
			"space\n", wlc_hw->unit, __FUNCTION__));
		return ret;
	}

	/* set pcie gen2 capability */
	si_corereg(wlc_hw->sih, 3, 0x120, ~0, 0x4DC);
	data = si_corereg(wlc_hw->sih, 3, 0x124, 0, 0);

	si_corereg(wlc_hw->sih, 3, 0x120, ~0, 0x4DC);
	si_corereg(wlc_hw->sih, 3, 0x124, ~0, (data & 0xfffffff0) | 2);

	si_corereg(wlc_hw->sih, 3, 0x120, ~0, 0x1800);
	data = si_corereg(wlc_hw->sih, 3, 0x124, 0, 0);

	si_corereg(wlc_hw->sih, 3, 0x120, ~0, 0x1800);
	si_corereg(wlc_hw->sih, 3, 0x124, ~0, (data & 0xfffffff0) | 2);

	si_corereg(wlc_hw->sih, 3, 0x120, ~0, 0x1800);
	si_corereg(wlc_hw->sih, 3, 0x124, ~0, data & 0xfffffff0);

	OSL_DELAY(1000);

	si_corereg(wlc_hw->sih, 3, 0x120, ~0, 0xBC);
	data = si_corereg(wlc_hw->sih, 3, 0x124, 0, 0);
	linkspeed = (data >> 16) & 0xf;

	WL_INFORM(("wl%d: pcie gen2 link speed: %d\n", wlc_hw->unit, linkspeed));

	return ret;
} /* wlc_bmac_4360_pcie2_war */

/** Initialize just the hardware when coming out of POR or S3/S5 system states */
void
BCMINITFN(wlc_bmac_hw_up)(wlc_hw_info_t *wlc_hw)
{
	if (wlc_hw->wlc->pub->hw_up)
		return;

#if defined(BCM_BACKPLANE_TIMEOUT)
	si_slave_wrapper_add(wlc_hw->sih);
#endif // endif

	WL_TRACE(("wl%d: %s:\n", wlc_hw->unit, __FUNCTION__));

	/* check if need to reinit pll */
	si_pll_sr_reinit(wlc_hw->sih);

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS && D11REV_GE(wlc_hw->corerev, 40)) {
		si_pmu_res_init(wlc_hw->sih, wlc_hw->osh);
	}

	/**
	 * JIRA: SWWLAN-27305 shut the bbpll off in sleep as well as improve the efficiency of
	 * some internal regulator.
	 */
	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS &&
	    (BCM4350_CHIP(wlc_hw->sih->chip) || BCM43602_CHIP(wlc_hw->sih->chip))) {
		si_pmu_chip_init(wlc_hw->sih, wlc_hw->osh);
		si_pmu_slow_clk_reinit(wlc_hw->sih, wlc_hw->osh);
	}

	/*
	 * Enable pll and xtal, initialize the power control registers,
	 * and force fastclock for the remainder of wlc_up().
	 */
	wlc_bmac_xtal(wlc_hw, ON);
	si_clkctl_init(wlc_hw->sih);
	wlc_clkctl_clk(wlc_hw, CLK_FAST);

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		si_pcie_hw_LTR_war(wlc_hw->sih);
		si_pciedev_crwlpciegen2(wlc_hw->sih);
		si_pciedev_reg_pm_clk_period(wlc_hw->sih);
	}
#ifndef WL_LTR
	si_pcie_ltr_war(wlc_hw->sih);
#endif // endif

	/* Init BTC related GPIOs to clean state on power up as well. This must
	 * be done here as even if radio is disabled, driver needs to
	 * make sure that output GPIO is lowered
	 */
	wlc_bmac_btc_gpio_disable(wlc_hw);

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		/* HW up(initial load, post hibernation resume), core init/fixup */

		if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID)) {
			/* changing the avb vcoFreq as 510M (from default: 500M) */
			/* Tl clk 127.5Mhz */
				WL_INFORM(("wl%d: %s: settng clock to %d\n",
				wlc_hw->unit, __FUNCTION__,	wlc_hw->vcoFreq_4360_pcie2_war));

				if (wlc_bmac_4360_pcie2_war(wlc_hw,
					wlc_hw->vcoFreq_4360_pcie2_war) != BCME_OK) {
					ASSERT(0);
				}
			}
		si_pci_fixcfg(wlc_hw->sih);
	}

#ifdef WLLED
	wlc_bmac_led_hw_init(wlc_hw);
#endif // endif

	/* Inform phy that a POR reset has occurred so it does a complete phy init */
	phy_radio_por_inform(wlc_hw->band->pi);

	wlc_hw->ucode_loaded = FALSE;
	wlc_hw->wlc->pub->hw_up = TRUE;
} /* wlc_bmac_hw_up */

static bool
wlc_dma_rxreset(wlc_hw_info_t *wlc_hw, uint fifo)
{
#ifdef BCMHWA
	HWA_RXPATH_EXPR({
	if (fifo == RX_FIFO && hwa_dev) {
		(void)hwa_rxpath_dma_reset(hwa_dev, wlc_hw->unit);
	}
	});
#endif // endif

	return (dma_rxreset(wlc_hw->di[fifo]));
}

/**
 * d11 core reset
 *   ensure fast clock during reset
 *   reset dma
 *   reset d11(out of reset)
 *   reset phy(out of reset)
 *   clear software macintstatus for fresh new start
 * one testing hack wlc_hw->noreset will bypass the d11/phy reset
 */
void
BCMINITFN(wlc_bmac_corereset)(wlc_hw_info_t *wlc_hw, uint32 flags)
{
	uint i;
	bool fastclk;
	uint32 resetbits = 0;

	if (flags == WLC_USE_COREFLAGS)
		flags = (wlc_hw->band->pi ? wlc_hw->band->core_flags : 0);

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	/* request FAST clock if not on  */
	if (!(fastclk = wlc_hw->forcefastclk))
		wlc_clkctl_clk(wlc_hw, CLK_FAST);

	/* reset the dma engines except if core is in reset (first time thru or bigger hammer) */
	if (si_iscoreup(wlc_hw->sih)) {
		if (!PIO_ENAB_HW(wlc_hw)) {
			hnddma_t *di;

#ifdef BCMHWA
			HWA_TXFIFO_EXPR({
				/* Prepare to disable 3b block */
				hwa_txfifo_disable_prep(hwa_dev, 0);

				/* halt any tx processing by ucode */
				wlc_bmac_suspend_mac_and_wait(wlc_hw);

				/* Disable 3b block to stop posting tx packets */
				hwa_txfifo_enable(hwa_dev, 0, FALSE);
			});
#endif /* BCMHWA */

			for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
				di = wlc_hw->di[i];
#ifdef BCM_DMA_CT
				if (BCM_DMA_CT_ENAB(wlc_hw->wlc) && di) {
					/* Reset Data DMA channel */
					if (!dma_txreset(di)) {
						WL_ERROR(("wl%d: %s: dma_txreset[%d]: cannot stop "
							"dma\n", wlc_hw->unit, __FUNCTION__, i));
					}

					/* Reset AQM DMA channel */
					di = wlc_hw->aqm_di[i];
				}
#endif /* BCM_DMA_CT */
				if (di) {
					if (!dma_txreset(di)) {
						WL_ERROR(("wl%d: %s: dma_txreset[%d]: cannot stop "
							"dma\n", wlc_hw->unit, __FUNCTION__, i));
						WL_HEALTH_LOG(wlc_hw->wlc, DMATX_ERROR);
					}
					wlc_upd_suspended_fifos_clear(wlc_hw, i);
				}
			}
			for (i = 0; i < MAX_RX_FIFO; i++) {
				if ((wlc_hw->di[i] != NULL) && wlc_bmac_rxfifo_enab(wlc_hw, i)) {
					if (!wlc_dma_rxreset(wlc_hw, i)) {
						WL_ERROR(("wl%d: %s: dma_rxreset[%d]:"
							" cannot stop dma\n",
							wlc_hw->unit, __FUNCTION__, i));
						WL_HEALTH_LOG(wlc_hw->wlc, DMARX_ERROR);
					}
				}
			}

#ifdef BCMHWA
			/* It's safe to enabled MAC after DMA reset */
			HWA_TXFIFO_EXPR(wlc_bmac_enable_mac(wlc_hw));
#endif /* BCMHWA */
		} else {
			for (i = 0; i < NFIFO_LEGACY; i++)
				if (wlc_hw->pio[i])
					wlc_pio_reset(wlc_hw->pio[i]);
		}
	}

	/* if noreset, just stop the psm and return */
	if (wlc_hw->noreset) {
		wlc_hw->macintstatus = 0;	/* skip wl_dpc after down */
		wlc_bmac_mctrl(wlc_hw, MCTL_PSM_RUN | MCTL_EN_MAC, 0);
		return;
	}

	/*
	 * corerev >= 18, mac no longer enables phyclk automatically when driver accesses phyreg
	 * throughput mac, AND phy_reset is skipped at early stage when band->pi is invalid
	 * need to enable PHY CLK
	 */
	flags |= SICF_PCLKE;
#if defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM)
	if (D11REV_GE(wlc_hw->corerev, 61)) {
		/* if sw patch control values are zero, save patch control values from hw
		 * before reset. They will be restored unconditionally after reset.
		 */
		if (wlc_hw->pc_mask == 0)
			wlc_hw->pc_flags = R_REG(wlc_hw->osh, D11_MacPatchCtrl(wlc_hw));
			wlc_hw->pc_mask = wlc_hw->pc_flags;

		WL_TRACE(("saving patchCtrl: flags:%x, mask:%x\n", wlc_hw->pc_flags,
			wlc_hw->pc_mask));
	}
#endif /* defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM) */

	/* reset the core
	 * In chips with PMU, the fastclk request goes through d11 core reg 0x1e0, which
	 *  is cleared by the core_reset. have to re-request it.
	 *  This adds some delay and we can optimize it by also requesting fastclk through
	 *  chipcommon during this period if necessary. But that has to work coordinate
	 *  with other driver like mips/arm since they may touch chipcommon as well.
	 *  RSDB chips handle core reset programming of both cores from core 0
	 *  context only.
	 *  RSDB chip does D11 core reset only in Core 0 context.
	 *  In case of SISO 1 mode core reset sequence is required in CORE 1
	 *  context as well.
	 *
	 */
	wlc_hw->clk = FALSE;
	wlc_bmac_core_reset(wlc_hw, flags, resetbits);
	wlc_hw->clk = TRUE;

#if defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM)
	if (D11REV_GE(wlc_hw->corerev, 61)) {
		WL_TRACE(("enabling patchCtrl: after reset!\n"));
		hndd11_write_reg32(wlc_hw->osh, D11_MacPatchCtrl(wlc_hw),
			wlc_hw->pc_mask,
			wlc_hw->pc_flags);
	}
#endif /* defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM) */

	/* PHY Mode has to be written only in Core 0 cflags.
	 * For Core 1 override, switch to core-0 and write it.
	 */
#ifdef WLRSDB
	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		if (wlc_rsdb_is_other_chain_idle(wlc_hw->wlc) == TRUE)
			wlc_rsdb_set_phymode(wlc_hw->wlc, (wlc_rsdb_mode(wlc_hw->wlc)));
	}
#endif /* WLRSDB */

#if defined(BCM_BACKPLANE_TIMEOUT)
	/* Clear previous error log info */
	si_reset_axi_errlog_info(wlc_hw->sih);
#endif /* BCM_BACKPLANE_TIMEOUT */

	/*
	 * If band->phytype & band->phyrev are not yet known, get them from the d11 registers.
	 * The phytype and phyrev are used in WLCISXXX() and XXXREV_XX() macros.
	 */
	ASSERT(wlc_hw->regs != NULL);
	if (wlc_hw->band && wlc_hw->band->phytype == 0 && wlc_hw->band->phyrev == 0) {
		wlc_hw->band->phytype = wlc_bmac_get_phy_type_from_reg(wlc_hw);
		wlc_hw->band->phyrev = (R_REG(wlc_hw->osh, D11_PHY_REG_0(wlc_hw)) & PV_PV_MASK);
	}

	if (wlc_hw->band && wlc_hw->band->pi)
		phy_hw_clk_state_upd(wlc_hw->band->pi, TRUE);

	if (wlc_hw->band && WLCISACPHY(wlc_hw->band)) {
		/* set up highspeed DAC mode to 1 by default
		 * (see default value 0 is undefined mode)
		 */
		si_core_cflags(wlc_hw->sih, SICF_DAC, 0x100);

		/* turn off phy clocks */
		si_core_cflags(wlc_hw->sih, (SICF_FGC | SICF_PCLKE), 0);

		/* re-enable phy clocks to resync to macphy clock */
		si_core_cflags(wlc_hw->sih, SICF_PCLKE, SICF_PCLKE);
	}

	wlc_mctrl_reset(wlc_hw);

	if (PSMX_ENAB(wlc_hw->wlc->pub)) {
		wlc_mctrlx_reset(wlc_hw);
	}

	if (PMUCTL_ENAB(wlc_hw->sih))
		wlc_clkctl_clk(wlc_hw, CLK_FAST);

	if (wlc_hw->band && wlc_hw->band->pi) {
		wlc_bmac_phy_reset(wlc_hw);
	}

	/* turn on PHY_PLL */
	wlc_bmac_core_phypll_ctl(wlc_hw, TRUE);

	/* clear sw intstatus */
	wlc_hw->macintstatus = 0;

	/* Clear rx FIFO info of SW int status */
	wlc_hw->dma_rx_intstatus = 0;

	/* restore the clk setting */
	if (!fastclk)
		wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);

#ifdef WLP2P_UCODE
	wlc_hw->p2p_shm_base = (uint)~0;
#endif // endif
	wlc_hw->cca_shm_base = (uint)~0;
} /* wlc_bmac_corereset */

/* Search mem rw utilities */

/**
 * Method of determining has changed over core revisions, so abstracts this to callee.
 *
 * @return                 Transmit FIFO memory size in [KB] units.
 */
static int
wlc_bmac_get_txfifo_size_kb(wlc_hw_info_t *wlc_hw)
{
	int txfifo_sz;

	if (D11REV_GE(wlc_hw->corerev, 130)) {
		txfifo_sz = ((wlc_hw->machwcap & MCAP_TXFSZ_MASK_GE130) >> MCAP_TXFSZ_SHIFT_GE130);
	} else if (D11REV_GE(wlc_hw->corerev, 128)) {
		txfifo_sz = ((wlc_hw->machwcap & MCAP_TXFSZ_MASK_GE128) >> MCAP_TXFSZ_SHIFT_GE128);
	} else {
		txfifo_sz = ((wlc_hw->machwcap & MCAP_TXFSZ_MASK) >> MCAP_TXFSZ_SHIFT);
	}

	return txfifo_sz * 2;
}

#ifdef MBSS
bool
wlc_bmac_ucodembss_hwcap(wlc_hw_info_t *wlc_hw)
{
	/* add up template space here */
	int templ_ram_sz, fifo_mem_used, i, stat;
	uint blocks = 0;

	for (fifo_mem_used = 0, i = 0; i < NFIFO_LEGACY; i++) {
		stat = wlc_bmac_xmtfifo_sz_get(wlc_hw, i, &blocks);
		if (stat != 0)
			return FALSE;
		fifo_mem_used += blocks;
	}

	templ_ram_sz = wlc_bmac_get_txfifo_size_kb(wlc_hw);

	if ((templ_ram_sz - fifo_mem_used) <
	    (int)MBSS_TPLBLKS(wlc_hw->corerev, WLC_MAX_AP_BSS(wlc_hw->corerev))) {
		WL_ERROR(("wl%d: %s: Insuff mem for MBSS: templ memblks %d fifo memblks %d\n",
			wlc_hw->unit, __FUNCTION__, templ_ram_sz, fifo_mem_used));
		return FALSE;
	}

	return TRUE;
}
#endif /* MBSS */

static void
BCMRAMFN(wlc_xmtfifo_sz_fixup)(wlc_hw_info_t *wlc_hw)
{
	ASSERT(D11REV_LT(wlc_hw->corerev, 40));
}

/**
 * If the ucode that supports corerev 5 is used for corerev 9 and above, txfifo sizes needs to be
 * modified (increased) since the newer cores have more memory.
 */
static void
BCMINITFN(wlc_corerev_fifofixup)(wlc_hw_info_t *wlc_hw)
{
	uint16 fifo_nu;
	uint16 txfifo_startblk = TXFIFO_START_BLK, txfifo_endblk;
	uint16 txfifo_def, txfifo_def1;
	uint16 txfifo_cmd;
	osl_t *osh;

	ASSERT(D11REV_LT(wlc_hw->corerev, 40));

	wlc_xmtfifo_sz_fixup(wlc_hw);

	/* tx fifos start at TXFIFO_START_BLK from the Base address */
#ifdef MBSS
	if (TRUE &&
	    MBSS_ENAB(wlc_hw->wlc->pub) &&
	    wlc_bmac_ucodembss_hwcap(wlc_hw)) {
		ASSERT(WLC_MAX_AP_BSS(wlc_hw->corerev) > 0);
		txfifo_startblk =
			MBSS_TXFIFO_START_BLK(wlc_hw->corerev,
		                              WLC_MAX_AP_BSS(wlc_hw->corerev));
	}
#else
	txfifo_startblk = TXFIFO_START_BLK;
#endif /* MBSS */

	osh = wlc_hw->osh;

	/* sequence of operations:  reset fifo, set fifo size, reset fifo */
	for (fifo_nu = 0; fifo_nu < NFIFO_LEGACY; fifo_nu++) {

		txfifo_endblk = txfifo_startblk + wlc_hw->xmtfifo_sz[fifo_nu];
		txfifo_def = (txfifo_startblk & 0xff) |
			(((txfifo_endblk - 1) & 0xff) << TXFIFO_FIFOTOP_SHIFT);
		txfifo_def1 = ((txfifo_startblk >> 8) & 0x3) |
			((((txfifo_endblk - 1) >> 8) & 0x3) << TXFIFO_FIFOTOP_SHIFT);
		txfifo_cmd = TXFIFOCMD_RESET_MASK | (fifo_nu << TXFIFOCMD_FIFOSEL_SHIFT);

		W_REG(osh, D11_xmtfifocmd(wlc_hw), txfifo_cmd);
		W_REG(osh, D11_xmtfifodef(wlc_hw), txfifo_def);
		W_REG(osh, D11_xmtfifodef1(wlc_hw), txfifo_def1);

		W_REG(osh, D11_xmtfifocmd(wlc_hw), txfifo_cmd);

		txfifo_startblk += wlc_hw->xmtfifo_sz[fifo_nu];
	}

	/* need to propagate to shm location to be in sync since ucode/hw won't do this */
	wlc_bmac_write_shm(wlc_hw, M_FIFOSIZE0(wlc_hw), wlc_hw->xmtfifo_sz[TX_AC_BE_FIFO]);
	wlc_bmac_write_shm(wlc_hw, M_FIFOSIZE1(wlc_hw), wlc_hw->xmtfifo_sz[TX_AC_VI_FIFO]);
	wlc_bmac_write_shm(wlc_hw, M_FIFOSIZE2(wlc_hw), ((wlc_hw->xmtfifo_sz[TX_AC_VO_FIFO] << 8) |
		wlc_hw->xmtfifo_sz[TX_AC_BK_FIFO]));
	wlc_bmac_write_shm(wlc_hw, M_FIFOSIZE3(wlc_hw), ((wlc_hw->xmtfifo_sz[TX_ATIM_FIFO] << 8) |
		wlc_hw->xmtfifo_sz[TX_BCMC_FIFO]));
	/* Check if TXFIFO HW config is proper */
	wlc_bmac_txfifo_sz_chk(wlc_hw);
} /* wlc_corerev_fifofixup */

static void
BCMINITFN(wlc_bmac_btc_init)(wlc_hw_info_t *wlc_hw)
{
	/* make sure 2-wire or 3-wire decision has been made */
	ASSERT((wlc_hw->btc->wire >= WL_BTC_2WIRE) || (wlc_hw->btc->wire <= WL_BTC_4WIRE));

	/* Configure selected BTC mode */
	wlc_bmac_btc_mode_set(wlc_hw, wlc_hw->btc->mode);

	if (wlc_hw->boardflags2 & BFL2_BTCLEGACY) {
		/* Pin muxing changes for BT coex operation in LCNXNPHY */
		if ((CHIPID(wlc_hw->sih->chip) == BCM43217_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM43428_CHIP_ID)) {
			si_btc_enable_chipcontrol(wlc_hw->sih);
			si_pmu_chipcontrol(wlc_hw->sih, PMU1_PLL0_CHIPCTL1, 0x10, 0x10);
		}
	}

	/* starting from ccrev 35, seci, 3/4 wire can be controlled by newly
	 * constructed SECI block.
	 */
	if (wlc_hw->boardflags & BFL_BTCOEX) {
		if ((wlc_hw->boardflags2 & BFL2_BTCLEGACY) && D11REV_LT(wlc_hw->corerev, 129)) {
			/* X19 has its special 4 wire which is not using new SECI block */
			si_seci_init(wlc_hw->sih, SECI_MODE_LEGACY_3WIRE_WLAN);
		} else if (BCMECICOEX_ENAB_BMAC(wlc_hw)) {
			si_eci_init(wlc_hw->sih);
		} else if (BCMSECICOEX_ENAB_BMAC(wlc_hw)) {
			si_seci_init(wlc_hw->sih, SECI_MODE_SECI);
		} else if (BCMGCICOEX_ENAB_BMAC(wlc_hw)) {
			si_gci_init(wlc_hw->sih);
		}
	}

	wlc_btc_init(wlc_hw->wlc);
} /* wlc_bmac_btc_init */

#ifdef WLCX_ATLAS
static void
BCMINITFN(wlc_bmac_wlcx_init)(wlc_hw_info_t *wlc_hw)
{
	/* GCI/SECI initialization for WLCX protocol on FL-ATLAS platform. */
	if ((wlc_hw->boardflags2 & BFL2_WLCX_ATLAS) && si_gci(wlc_hw->sih)) {
		si_gci_seci_init(wlc_hw->sih);
	}
}
#endif /* WLCX_ATLAS */

int
wlc_bmac_update_fastpwrup_time(wlc_hw_info_t *wlc_hw)
{
	int ret = BCME_OK;

	if ((wlc_hw == NULL) || (wlc_hw->osh == NULL)) {
		ret = BCME_BADARG;
		goto end;
	}

	/* program dynamic clock control fast powerup delay register
	 * and update SW delay
	 */
	wlc_hw->fastpwrup_dly = si_clkctl_fast_pwrup_delay(wlc_hw->sih);
	/* Temporary WAR for beacon loss in PM 1/2 mode on 4347a0 */
	if (RSDB_ENAB(wlc_hw->wlc->pub) && D11REV_IS(wlc_hw->corerev, 61))
		wlc_hw->fastpwrup_dly += 500;

	/* Temporary WAR for the beacon loss issue in 4347 PS1 mode */
	if (D11REV_IS(wlc_hw->corerev, 61) && LHL_IS_PSMODE_1(wlc_hw->sih)) {
		wlc_hw->fastpwrup_dly += 300;
	}

	W_REG(wlc_hw->osh, D11_FAST_PWRUP_DLY(wlc_hw), wlc_hw->fastpwrup_dly);
	if (D11REV_GT(wlc_hw->corerev, 40)) {
		/* For corerev >= 40, M_UCODE_DBGST is set after
			* the synthesizer is powered up in wake sequence.
			* So add the synthpu delay to wait for wake functionality.
			*/
		wlc_hw->fastpwrup_dly += wlc_bmac_synthpu_dly(wlc_hw);
		/*
		* SWWLAN-15421: Add additional delay for 43602 to cover PS resume issue
		* seen on MacBook.
		*/
		if (BCM43602_CHIP(wlc_hw->sih->chip)) {
			wlc_hw->fastpwrup_dly += 4500;
		}
	}

end:
	return ret;
}

/* Update the splitrx mode
 * mode : requested mode
 * init : whether from coreinit or by other applications
 *
 * During init path, clocks are assured and can skip wl up checks
 * for other use cases wl up should be checked to prevent invalid reg access
 */
int
wlc_update_splitrx_mode(wlc_hw_info_t *wlc_hw, bool mode, bool init)
{
	uint16 rcv_fifo_ctl = 0;
	wlc_info_t *wlc = wlc_hw->wlc;

	WL_INFORM(("wl: %d %s :  pub up %d int %d mode %d \n",
		wlc_hw->unit, __FUNCTION__, wlc->pub->up, init, mode));

	ASSERT(RXFIFO_SPLIT());

	/* Update SW mode */
	wlc_hw->hdrconv_mode = mode;

	if (!init && !wlc->pub->up) {
		/* dont change HW mode, if not up */
		return BCME_OK;
	}

	/* Update HW mode */

	/* Suspend MAC if not in core init pathh */
	if (!init) {
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
	}
	/* Enable bit */
	if (mode) {
		OR_REG(wlc_hw->osh, D11_RcvHdrConvCtrlSts(wlc_hw), HDRCONV_USR_ENAB);
	} else {
		AND_REG(wlc_hw->osh, D11_RcvHdrConvCtrlSts(wlc_hw), ~HDRCONV_USR_ENAB);
	}

	/* Read current fifo-sel for backup */
	rcv_fifo_ctl = R_REG(wlc_hw->osh, D11_RCV_FIFO_CTL(wlc_hw));

	/* select fifo-0 */
	AND_REG(wlc_hw->osh, D11_RCV_FIFO_CTL(wlc_hw),
		(uint16)(~(RXFIFO_CTL_FIFOSEL_MASK)));

	/* Put back fifo ctrl value */
	W_REG(wlc_hw->osh, D11_RCV_FIFO_CTL(wlc_hw), rcv_fifo_ctl);

	/* Enable back MAC if not in core init path */
	if (!init)
		wlc_bmac_enable_mac(wlc_hw);

	return BCME_OK;
}

/* read MAC soft capability from ucode */
bool
wlc_bmac_swcap_get(wlc_hw_info_t *wlc_hw, uint8 capbit)
{
	uint32 mac_cap;
	bool   bitval;

	mac_cap = wlc_bmac_read_shm(wlc_hw, M_MACHW_CAP_H(wlc_hw));
	mac_cap = (mac_cap << 16) |
		wlc_bmac_read_shm(wlc_hw, M_MACHW_CAP_L(wlc_hw));

	bitval = (mac_cap & capbit);

	return bitval;
}

/* write MAC soft capability to ucode */
void
wlc_bmac_swcap_set(wlc_hw_info_t *wlc_hw, uint8 capbit, bool val)
{
	if (val) {
		setbit(&wlc_hw->machwcap, capbit);
	} else {
		clrbit(&wlc_hw->machwcap, capbit);
	}

	wlc_bmac_write_shm(wlc_hw, M_MACHW_CAP_L(wlc_hw),
		(uint16)(((wlc_hw->machwcap) & 0xffff)));
	wlc_bmac_write_shm(wlc_hw, M_MACHW_CAP_H(wlc_hw),
		(uint16)((wlc_hw->machwcap >> 16) & 0xffff));
}

/**
 * d11 core init
 *   reset PSM
 *   download ucode/PCM
 *   let ucode run to suspended
 *   download ucode inits
 *   config other core registers
 *   init dma/pio
 *   init VASIP
 */
static void
BCMINITFN(wlc_coreinit)(wlc_hw_info_t *wlc_hw)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	bool fifosz_fixup = FALSE;
	uint16 buf[NFIFO_LEGACY] = { 0 };
#ifdef STA
	uint32 seqnum = 0;
#endif // endif
	uint bcnint_us;
	uint i = 0;
	osl_t *osh = wlc_hw->osh;
	uint16 mac_cap_l;
#if defined(MBSS)
	bool ucode9 = TRUE;
	(void)ucode9;
#endif // endif

	WL_TRACE(("wl%d: wlc_coreinit\n", wlc_hw->unit));
#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)
	/* If ctdma is not preconfigured, we need to reattach the DMA so that
	 * DMA is initialized properly with CDTMA on or off.
	 * DMA would not be reattached if user preconfigures ctdma=1 where CTDMA
	 * would always be enabled regardless of MU or SU.
	 */
	if (!(wlc->_dma_ct_flags & DMA_CT_PRECONFIGURED) &&
		(wlc->_dma_ct_flags & DMA_CT_IOCTL_OVERRIDE)) {
		bool wme = FALSE;
#ifdef WME
		wme = TRUE;
#endif /* WME */
		wlc_bmac_detach_dmapio(wlc_hw);

		wlc_hw->wlc->_dma_ct = (wlc_hw->wlc->_dma_ct_flags & DMA_CT_IOCTL_CONFIG)?
			TRUE : FALSE;
		wlc_hw->wlc->_dma_ct_flags &= ~DMA_CT_IOCTL_OVERRIDE;
		if (!wlc_bmac_attach_dmapio(wlc_hw, wme)) {
			WL_ERROR(("%s: DMA attach failed!\n", __FUNCTION__));
		}

		if (BCMSPLITRX_ENAB()) {
			/* Pass down pool info to dma layer */
			if (wlc_hw->di[RX_FIFO])
				dma_pktpool_set(wlc_hw->di[RX_FIFO], wlc->pub->pktpool_rxlfrag);

			/* if second fifo is set, set pktpool for that */
			if (wlc_hw->di[RX_FIFO1])
				dma_pktpool_set(wlc_hw->di[RX_FIFO1],
				(RXFIFO_SPLIT()?wlc->pub->pktpool_rxlfrag:wlc->pub->pktpool));

			/* FIFO- 2 */
			if (wlc_hw->di[RX_FIFO2])
				dma_pktpool_set(wlc_hw->di[RX_FIFO2], wlc->pub->pktpool);
		} else if (POOL_ENAB(wlc->pub->pktpool)) {
			/* set pool for rx dma */
			if (wlc_hw->di[RX_FIFO])
				dma_pktpool_set(wlc_hw->di[RX_FIFO], wlc->pub->pktpool);
		}
	}
#endif /* defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED) */

	wlc_bmac_btc_init(wlc_hw);

#ifdef WLCX_ATLAS
	if ((wlc_hw->boardflags & BFL_BTCOEX) && (wlc_hw->boardflags2 & BFL2_WLCX_ATLAS)) {
		WL_ERROR(("%s: WLCX_ATLAS and BTCOEX both can't be enabled!\n", __FUNCTION__));
		ASSERT(0);
	}

	/* Initializate WLCX SECI/GCI configuration */
	wlc_bmac_wlcx_init(wlc_hw);
#endif // endif

#if (defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)) || !defined(DONGLEBUILD)
	/*
	 * for dual d11 core chips, ucode is downloaded only once
	 * and will be thru core-0
	 */
	if (wlc_hw->macunit == 0) {
		bool ucode_download = TRUE;

#ifdef DONGLEBUILD
		if (D11REV_GE(wlc_hw->corerev, 128)) {
			/* 43684 FD mode doesn't need to download ucode here */
			ucode_download = FALSE;
		}
#endif /* DONGLEBUILD */

		if (ucode_download)
			wlc_ucode_download(wlc_hw);
#if defined(WLVASIP)
		/* vasip intialization
		 * do vasip init() here during attach() because vasip ucode gets reclaimed.
		 */
		if (VASIP_ENAB(wlc->pub)) {
			uint32 vasipver;

			if (wlc_vasip_support(wlc_hw, &vasipver, FALSE)) {
				wlc_vasip_init(wlc_hw, vasipver, FALSE);
			}
		}
#endif /* WLVASIP */
#ifdef WLSMC
		if (wlc_smc_hw_supported(wlc_hw->corerev))
			wlc_smc_download(wlc_hw);
#endif /* WLSMC */

	}
#endif /* (BCM_DMA_CT && !BCM_DMA_CT_DISABLED) || !DONGLEBUILD */

#if defined(SAVERESTORE) && !defined(DONGLEBUILD)
#ifdef SR_ESSENTIALS
	/* Only needs to be done once.
	 * Needs this before si_pmu_res_init() to use sr_isenab()
	 */
	if (SR_ESSENTIALS_ENAB())
		sr_save_restore_init(wlc_hw->sih);
#endif /* SR_ESSENTIALS */
	if (SR_ENAB() && sr_cap(wlc_hw->sih)) {
		/* Download SR code */
		wlc_bmac_sr_init(wlc_hw, wlc_hw->macunit);
	}
#endif /* SAVERESTORE && !DONGLEBUILD */
	/*
	 * FIFOSZ fixup
	 * 1) core5-9 use ucode 5 to save space since the PSM is the same
	 * 2) newer chips, driver wants to controls the fifo allocation
	 */
	fifosz_fixup = TRUE;

	(void) wlc_bmac_wowlucode_start(wlc_hw); /* starts+suspends non-wowl ucode as well */

	wlc_gpio_init(wlc_hw);

	if (D11REV_GE(wlc_hw->corerev, 40)) {
		wlc_bmac_reset_amt(wlc_hw);
	}
#ifdef WL_AUXPMQ
	if (AUXPMQ_ENAB(wlc_hw->wlc->pub)) {
		wlc_pmq_reset_auxpmq(wlc);
	}
#endif // endif

#ifdef STA
	/* store the previous sequence number */
	wlc_bmac_copyfrom_objmem(wlc->hw, S_SEQ_NUM << 2, &seqnum, sizeof(seqnum), OBJADDR_SCR_SEL);
#endif /* STA */

#ifdef BCMUCDOWNLOAD
	if (initvals_ptr) {
		wlc_write_inits(wlc_hw, initvals_ptr);
#ifdef BCM_RECLAIM_INIT_FN_DATA
		MFREE(wlc->osh, initvals_ptr, initvals_len);
		initvals_ptr = NULL;
		initvals_len = 0;
#endif /* BCM_RECLAIM_INIT_FN_DATA */
	} else {
		printf("initvals_ptr is NULL, error in inivals download\n");
	}
#else
#ifdef WLRSDB
	/* init IHR, SHM, and SCR */
	if (wlc_bmac_rsdb_cap(wlc_hw))  {
		if (D11REV_IS(wlc_hw->corerev, 56)) {
			wlc_bmac_rsdb_write_inits(wlc_hw, d11ac24initvals56,
				d11ac24initvals56core1);
		} else
			WL_ERROR(("wl%d: %s: corerev %d is invalid", wlc_hw->unit,
				__FUNCTION__, wlc_hw->corerev));
	} else
#endif /* WLRSDB */

#if defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM)
	if (wlc_uci_check_cap_ucode_rom_axislave(wlc_hw)) {
		wlc_uci_write_inits_with_rom_support(wlc_hw, UCODE_INITVALS);
	} else {
		WL_ERROR(("%s: wl%d: ROM enabled but no axi/ucode-rom cap! %d\n",
			__FUNCTION__, wlc_hw->unit, wlc_hw->corerev));
	}
/* the "#else" below is intentional */
#else /* defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM) */
	if (D11REV_IS(wlc_hw->corerev, 131)) {
		wlc_write_inits(wlc_hw, d11ax129initvals131);
		wlc_write_inits(wlc_hw, d11ax129initvalsx131);
	} else if (D11REV_IS(wlc_hw->corerev, 130)) {
		wlc_write_inits(wlc_hw, d11ax51initvals130);
		wlc_write_inits(wlc_hw, d11ax51initvalsx130);
	} else if (D11REV_IS(wlc_hw->corerev, 129)) {
		wlc_write_inits(wlc_hw, d11ax47initvals129);
#if defined(WL_PSMR1)
		if (PSMR1_ENAB(wlc_hw)) {
			wlc_write_inits(wlc_hw, d11ax47initvalsr1_129);
		}
#endif // endif
		wlc_write_inits(wlc_hw, d11ax47initvalsx129);
	} else if (D11REV_IS(wlc_hw->corerev, 128)) {
		wlc_write_inits(wlc_hw, d11ax47initvals128);
#if defined(WL_PSMR1)
		if (PSMR1_ENAB(wlc_hw)) {
			wlc_write_inits(wlc_hw, d11ax47initvalsr1_128);
		}
#endif // endif
		wlc_write_inits(wlc_hw, d11ax47initvalsx128);
	} else if (D11REV_IS(wlc_hw->corerev, 61)) {
		if (wlc_hw->wlc->pub->corerev_minor == 5) {
			wlc_write_inits(wlc_hw, d11ac128initvals61_5);
		} else {
			WLC_WRITE_INITS(wlc_hw, d11ac40initvals61_1, d11ac40initvals61_1_D11a);
		}
	} else if (D11REV_IS(wlc_hw->corerev, 65)) {
		wlc_write_inits(wlc_hw, d11ac33initvals65);
		wlc_write_inits(wlc_hw, d11ac33initvalsx65);
	} else if (D11REV_IS(wlc_hw->corerev, 49)) {
		wlc_write_inits(wlc_hw, d11ac9initvals49);
	} else if (D11REV_IS(wlc_hw->corerev, 48)) {
		wlc_write_inits(wlc_hw, d11ac8initvals48);
	} else if (D11REV_IS(wlc_hw->corerev, 42)) {
		wlc_write_inits(wlc_hw, d11ac1initvals42);
	} else if (D11REV_IS(wlc_hw->corerev, 30)) {
		if (WLCISNPHY(wlc_hw->band))
			wlc_write_inits(wlc_hw, d11n16initvals30);
	}
#endif /* defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM) */
#endif /* BCMUCDOWNLOAD */
#ifdef ATE_BUILD
	/* Assert dummy values for slow cal */
	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw),	OBJADDR_SCR_SEL | S_STREG);
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	uint16 val16 = R_REG(wlc_hw->osh, D11_objdata(wlc_hw));
	W_REG(wlc_hw->osh, D11_objdata(wlc_hw),
		(val16 & ~C_STREG_SLOWCAL_DN_NBIT) | C_STREG_SLOWCAL_PD_NBIT);
	W_REG(wlc_hw->osh, D11_SLOW_CTL(wlc_hw), 0x0);
	W_REG(wlc_hw->osh, D11_SLOW_TIMER_L(wlc_hw), 0x0);
	W_REG(wlc_hw->osh, D11_SLOW_TIMER_H(wlc_hw), 0x10);
#endif /* ATE_BUILD */
	/* For old ucode, txfifo sizes needs to be modified(increased) for Corerev >= 9 */
	if (D11REV_GE(wlc_hw->corerev, 40)) {
#ifdef WLRSDB
		if (!wlc_bmac_rsdb_cap(wlc_hw) ||
			wlc_rsdb_is_other_chain_idle(wlc_hw->wlc) == TRUE)
#endif /* WLRSDB */
			wlc_bmac_bmc_init(wlc_hw);
	} else if (D11REV_LT(wlc_hw->corerev, 40)) {
		if (fifosz_fixup == TRUE) {
			wlc_corerev_fifofixup(wlc_hw);
		}
		wlc_corerev_fifosz_validate(wlc_hw, buf);
	} else {
		printf("add support for fifo inits for corerev %d......\n", wlc_hw->corerev);
		ASSERT(0);
	}

	/* make sure we can still talk to the mac */
	ASSERT(R_REG(osh, D11_MACCONTROL(wlc_hw)) != 0xffffffff);

	/* band-specific inits done by wlc_bsinit() */

#ifdef MBSS
	if (MBSS_ENAB(wlc->pub)) {
		/* Set search engine ssid lengths to zero */
		if (wlc_bmac_ucodembss_hwcap(wlc_hw) == TRUE) {
			uint32 start; /**< byte offset */
			uint32 swplen, idx;

			swplen = 0;
			for (idx = 0; idx < (uint) wlc->pub->tunables->maxucodebss; idx++) {
				start = SHM_MBSS_SSIDSE_BASE_ADDR(wlc) +
					(idx * SHM_MBSS_SSIDSE_BLKSZ(wlc));
				wlc_bmac_copyto_objmem(wlc_hw, start, &swplen,
					SHM_MBSS_SSIDLEN_BLKSZ, OBJADDR_SRCHM_SEL);
			}
		}
	}
#endif /* MBSS */

	/* Set up frame burst size and antenna swap threshold init values */
	wlc_bmac_write_shm(wlc_hw, M_MBURST_SIZE(wlc_hw), MAXTXFRAMEBURST);
	wlc_bmac_write_shm(wlc_hw, M_MAX_ANTCNT(wlc_hw), ANTCNT);

	/* set intrecvlazy to configured value */
	wlc_bmac_rcvlazy_update(wlc_hw, wlc_hw->intrcvlazy);

	/* set the station mode (BSS STA) */
	wlc_bmac_mctrl(wlc_hw,
	          (MCTL_INFRA | MCTL_DISCARD_PMQ | MCTL_AP),
	          (MCTL_INFRA | MCTL_DISCARD_PMQ));

	if (PIO_ENAB_HW(wlc_hw)) {
		/* set fifo mode for each VALID rx fifo */
		wlc_rxfifo_setpio(wlc_hw);

		for (i = 0; i < NFIFO_LEGACY; i++) {
			if (wlc_hw->pio[i]) {
				wlc_pio_init(wlc_hw->pio[i]);
			}
		}

#ifdef IL_BIGENDIAN
		/* enable byte swapping */
		wlc_bmac_mctrl(wlc_hw, MCTL_BIGEND, MCTL_BIGEND);
#endif /* IL_BIGENDIAN */
	}

	/* set up Beacon interval */
	bcnint_us = 0x8000 << 10;
	W_REG(osh, D11_CFPRep(wlc_hw), (bcnint_us << CFPREP_CBI_SHIFT));
	W_REG(osh, D11_CFPStart(wlc_hw), bcnint_us);
	SET_MACINTSTATUS(osh, wlc_hw, MI_GP1);

	/* Configure selected BTC task */
	if (wlc_hw->btc->task != BTCX_TASK_UNKNOWN) {
		wlc_bmac_btc_task_set(wlc_hw, wlc_hw->btc->task);
	}

	if (D11REV_GE(wlc_hw->corerev, 65)) {
		int idx;
		for (idx = 0; idx < NFIFO_LEGACY; idx++) {
			W_REG(osh, &(D11Reggrp_intctrlregs(wlc, idx)->intmask), 0);
			W_REG(osh, D11Reggrp_altintmask(wlc_hw, idx), 0);

			/* When CT mode is enabled, the above writes to intmasks are not effective
			 * since only the indirect registers are active.
			 * Instead use the indirect AQM DMA register access that are used for CT
			 * mode.
			 * Note: do not program inddma.indintstatus and mask for corerev64
			 */

			// XXX: need to do this for ext FIFOs also?
			if (BCM_DMA_CT_ENAB(wlc_hw->wlc) && wlc_hw->aqm_di[idx]) {
				dma_set_indqsel(wlc_hw->aqm_di[idx], FALSE);
				W_REG(osh, (&(D11Reggrp_indaqm(wlc_hw, 0)->indintmask)), 0);
			}
		}
	}

#if defined(BCMHWA) && defined(HWA_RXFILL_BUILD)
	/* HWA doesn't need RX_FIFO and RX_FIFO1 intr. */
	ASSERT(RXFIFO_SPLIT());
	ASSERT(PKT_CLASSIFY_EN(RX_FIFO2));

	/* disable interrupt mask */
	AND_REG(osh, &(D11Reggrp_intctrlregs(wlc, RX_FIFO)->intmask), ~DEF_RXINTMASK);
	/* disable interrupt for second fifo */
	if (PKT_CLASSIFY_EN(RX_FIFO1) || RXFIFO_SPLIT()) {
		AND_REG(osh, &(D11Reggrp_intctrlregs(wlc, RX_FIFO1)->intmask), ~DEF_RXINTMASK);
	}
#else
	/* write interrupt mask */
	W_REG(osh, &(D11Reggrp_intctrlregs(wlc, RX_FIFO)->intmask), DEF_RXINTMASK);

	/* interrupt for second fifo */
	if (PKT_CLASSIFY_EN(RX_FIFO1) || RXFIFO_SPLIT())
		W_REG(osh, &(D11Reggrp_intctrlregs(wlc, RX_FIFO1)->intmask), DEF_RXINTMASK);
#endif /* BCMHWA && HWA_RXFILL_BUILD */

	/* interrupt for third fifo in MODE 3 and MODE 4 */
	if (PKT_CLASSIFY_EN(RX_FIFO2)) {
		W_REG(osh, &(D11Reggrp_intctrlregs(wlc, RX_FIFO2)->intmask), DEF_RXINTMASK);
	}

#if defined(STS_FIFO_RXEN)
	if (STS_RX_ENAB(wlc->pub)) {
		W_REG(osh, &(D11Reggrp_intctrlregs(wlc, STS_FIFO)->intmask), DEF_RXINTMASK);
	}
#endif // endif

	/* allow the MAC to control the PHY clock (dynamic on/off) */
	wlc_bmac_macphyclk_set(wlc_hw, ON);

	/* program dynamic clock control fast powerup delay register */
	wlc_hw->fastpwrup_dly = si_clkctl_fast_pwrup_delay(wlc_hw->sih);
	W_REG(osh, D11_FAST_PWRUP_DLY(wlc_hw), wlc_hw->fastpwrup_dly);
	if (D11REV_GT(wlc_hw->corerev, 40)) {
		/* For corerev >= 40, M_UCODE_DBGST is set after
			* the synthesizer is powered up in wake sequence.
			* So add the synthpu delay to wait for wake functionality.
			*/
		wlc_hw->fastpwrup_dly += wlc_bmac_synthpu_dly(wlc_hw);
	}
	wlc_bmac_update_fastpwrup_time(wlc_hw);

	/* tell the ucode the corerev */
	wlc_bmac_write_shm(wlc_hw, M_MACHW_VER(wlc_hw),
		D11_MACHW_VER(wlc->pub->corerev, wlc->pub->corerev_minor));
	wlc_bmac_write_shm(wlc_hw, M_UCODE_DBGST(wlc_hw), DBGST_SUSPENDED);
#if defined(WL_PSMR1)
	/* Mirror PSMr0 settings */
	if (PSMR1_ENAB(wlc_hw)) {
		wlc_bmac_write_shm1(wlc_hw, M_MACHW_VER(wlc_hw),
			D11_MACHW_VER(wlc->pub->corerev, wlc->pub->corerev_minor));
		wlc_bmac_write_shm1(wlc_hw, M_UCODE_DBGST(wlc_hw), DBGST_SUSPENDED);
	}
#endif /* WL_PSMR1 */

#if defined(WL_PSMX)
	if (PSMX_ENAB(wlc->pub)) {
		wlc_bmac_write_shmx(wlc_hw, MX_MACHW_VER(wlc_hw),
			D11_MACHW_VER(wlc->pub->corerev, wlc->pub->corerev_minor));
	}
#endif /* WL_PSMX */

	mac_cap_l = (uint16)(((wlc_hw->machwcap) & 0xffff));
	/* Turn off rx deagg by default, for rev128
	 * override M_MACHW_CAP_L|H, bit#14 to let ucode know
	 */
	if (D11REV_IS(wlc_hw->corerev, 128)) {
		setbit(&mac_cap_l, MSWCAP_DEAGG_OFF_NBIT);
	}
	wlc_bmac_write_shm(wlc_hw, M_MACHW_CAP_L(wlc_hw), mac_cap_l);

	wlc_bmac_write_shm(wlc_hw, M_MACHW_CAP_H(wlc_hw),
		(uint16)((wlc_hw->machwcap >> 16) & 0xffff));

#if defined(WL_PSMR1)
	/* Mirror PSMr0 settings */
	if (PSMR1_ENAB(wlc_hw)) {
	wlc_bmac_write_shm1(wlc_hw, M_MACHW_CAP_L(wlc_hw), (uint16)(wlc_hw->machwcap & 0xffff));

		wlc_bmac_write_shm1(wlc_hw, M_MACHW_CAP_H(wlc_hw),
			(uint16)((wlc_hw->machwcap >> 16) & 0xffff));
	}
#endif /* WL_PSMR1 */

	/* write retry limits to SCR, this done after PSM init */
	wlc_bmac_copyto_objmem(wlc_hw, S_DOT11_SRC_LMT << 2, &(wlc_hw->SRL),
		sizeof(wlc_hw->SRL), OBJADDR_SCR_SEL);

	wlc_bmac_copyto_objmem(wlc_hw, S_DOT11_LRC_LMT << 2, &(wlc_hw->LRL),
		sizeof(wlc_hw->LRL), OBJADDR_SCR_SEL);

#ifdef STA
	if (wlc->seq_reset) {
		wlc->seq_reset = FALSE;
	} else {
		/* write the previous sequence number, this done after PSM init */
		wlc_bmac_copyto_objmem(wlc->hw, S_SEQ_NUM << 2, &seqnum,
			sizeof(seqnum), OBJADDR_SCR_SEL);
	}
#endif /* STA */

	/* write rate fallback retry limits */
	wlc_bmac_write_shm(wlc_hw, M_SFRMTXCNTFBRTHSD(wlc_hw), wlc_hw->SFBL);
	wlc_bmac_write_shm(wlc_hw, M_LFRMTXCNTFBRTHSD(wlc_hw), wlc_hw->LFBL);
	AND_REG(osh, D11_IFS_CTL(wlc_hw), 0x0FFF);
	W_REG(osh, D11_IFS_AIFSN(wlc_hw), EDCF_AIFSN_MIN);

	/* dma or pio initializations */
	wlc_dma_pio_init(wlc_hw);

	/**
	 *  The value to be written into the IHR TSF clk_frac registers is (2^26)/(freq)MHz, in
	 *  fixed point format.
	 *  If the CLK is static for the chip, we do not need to set it in driver.
	 *  Instead, the setting is put in ucode's initialization procedure.
	 */
	if (BCM4369_CHIP(wlc_hw->sih->chip)) {
		/* MAC clock frequency for 4369 is 192.599998MHz -> frac = 348436.47298... */
		W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), 0x5114);
		W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), 0x5); /* 'h5_5114 = 348436 */
	} else if (EMBEDDED_2x2AX_CORE(wlc_hw->sih->chip) ||
		BCM6878_CHIP(wlc_hw->sih->chip) ||
		0)
	{
		wlc_bmac_switch_macfreq(wlc_hw, 0);
	}

	/*
	Set the TA at the appropriate SHM location for CTS2SELF frames
	generated by ucode for AC only
	*/
	if (D11REV_GE(wlc_hw->corerev, 40))
		wlc_bmac_set_myaddr(wlc_hw, &(wlc_hw->etheraddr));

	/* initialize btc_params and btc_flags */
	wlc_bmac_btc_param_init(wlc_hw);
#ifdef BCMLTECOEX
	/* config ltecx interface */
	if (BCMLTECOEX_ENAB(wlc->pub))	{
		wlc_ltecx_init(wlc->ltecx);
	}
#endif /* BCMLTECOEX */

#ifdef WLP2P_UCODE
	if (DL_P2P_UC(wlc_hw)) {
		/* enable P2P mode */
		wlc_bmac_mhf(wlc_hw, MHF5, MHF5_P2P_MODE,
		             wlc_hw->_p2p ? MHF5_P2P_MODE : 0, WLC_BAND_ALL);
		/* cache p2p SHM location */
		wlc_hw->p2p_shm_base = wlc_bmac_read_shm(wlc_hw, M_P2P_BLK_PTR(wlc_hw)) << 1;
	}
#endif // endif
	if (D11REV_LT(wlc_hw->corerev, 40)) {
		wlc_hw->cca_shm_base = M_CCA_STATS_BLK(wlc_hw);
	} else {
		wlc_hw->cca_shm_base = (wlc_bmac_read_shm(wlc_hw, M_CCASTATS_PTR(wlc_hw)) << 1);
		wlc_hw->macstat1_shm_base = (wlc_bmac_read_shm(wlc_hw,
			M_PSM2HOST_EXT_PTR(wlc_hw)) << 1);
	}

	/* Shmem pm_dur is reset by ucode as part of auto-init, hence call wlc_reset_accum_pmdur */
	wlc_reset_accum_pmdur(wlc);
	WL_ERROR(("wl%d: CORE INIT : mode %d pktclassify %d rxsplit %d  hdr conversion %d"
		" DMA_CT %s\n", wlc_hw->unit, BCMSPLITRX_MODE(), PKT_CLASSIFY(), RXFIFO_SPLIT(),
		wlc_hw->hdrconv_mode, wlc->_dma_ct ? "Enabled":"Disabled"));

	/* Enable RXE HT request on receiving first valid byte */
	/* From HW JIRA:
	* CRWLDOT11M-1824: SP2DPQ.EOF can get lost when backplane clk transits
	*    to HT and this causes rxdma stuck
	*/
	if (!(D11REV_GE(wlc_hw->corerev, 65) || D11REV_IS(wlc_hw->corerev, 61))) {
		OR_REG(osh, D11_PSMCoreCtlStat(wlc_hw), PSM_CORE_CTL_REHE);
	}

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_hw_init(wlc);
	}
#if defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)
	if (D11REV_GE(wlc_hw->corerev, 130)) {
		wlc_offload_init(wlc->offl, wlc_hw->sts_phyrx_va_non_aligned);
#ifdef WLC_OFFLOADS_RXSTS
		if (STS_RX_OFFLOAD_ENAB(wlc_hw->wlc->pub)) {
			wlc_hw->sts_mempool.ctx = (void *)wlc_hw->wlc->offl;
			wlc_hw->sts_mempool.free_sts_fn = wlc_offload_release_rxstatus;
		}
#endif /* WLC_OFFLOADS_RXSTS */
	}
#endif /* WLC_OFFLOADS_TXSTS || WLC_OFFLOADS_RXSTS */

	/* Initialize D11 WARs flags */
	wlc_hw->d11war_flags = 0;
	if ((BCM43684_CHIP(wlc_hw->sih->chip) && (CHIPREV(wlc_hw->sih->chiprev) < 3))) {
		wlc_hw->d11war_flags |= D11_WAR_BFD_TXVMEM_RESET;
	}
} /* wlc_coreinit */

static void
wlc_dma_pio_init(wlc_hw_info_t *wlc_hw)
{
	uint i = 0;
	wlc_info_t *wlc = wlc_hw->wlc;
	wlc_tunables_t *tune = wlc->pub->tunables;

#ifdef WLPIO
	uint16 buf[NFIFO_LEGACY] = {0, 0, 0, 0, 0, 0};
#endif /* WLPIO */

	if (!PIO_ENAB_HW(wlc_hw)) {
		/* init the tx dma engines */
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
			if (wlc_hw->di[i]) {
				dma_txinit(wlc_hw->di[i]);
			}
		}

#ifdef WLRSDB
		if (wlc_bmac_rsdb_cap(wlc_hw)) {
			if (wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_RSDB)
				wlc_bmac_update_rxpost_rxbnd(wlc_hw, NRXBUFPOST_SMALL, RXBND_SMALL);
			else
				wlc_bmac_update_rxpost_rxbnd(wlc_hw, NRXBUFPOST, RXBND);
		}
#endif /* WLRSDB */

		if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
			/* set M_MU_TESTMODE and TXE_ctmode */
			wlc_bmac_enable_ct_access(wlc_hw, TRUE);
			/* init ct_dma(aqmdma) */
			for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
				if (wlc_hw->aqm_di[i]) {
					dma_txinit(wlc_hw->aqm_di[i]);
				}
			}
		} else {
			wlc_bmac_enable_ct_access(wlc_hw, FALSE);
		}

		for (i = 0; i < MAX_RX_FIFO; i++) {
			if ((wlc_hw->di[i] != NULL) && wlc_bmac_rxfifo_enab(wlc_hw, i)) {
				dma_rxinit(wlc_hw->di[i]);
				wlc_bmac_dma_rxfill(wlc_hw, i);
			}
		}

		if (RXFIFO_SPLIT()) {
			/* Enable Header conversion */
			wlc_update_splitrx_mode(wlc_hw, wlc_hw->hdrconv_mode, TRUE);

			/* copy count value */
			W_REG(wlc_hw->osh, D11_RCV_COPYCNT_Q1(wlc_hw), tune->copycount);

#ifdef DONGLEBUILD
			wl_set_copycount_bytes(wlc_hw->wlc->wl,
				tune->copycount, wlc->datafiforxoff);
#endif /* DONGLEBUILD */
		}
	} else {
		for (i = 0; i < NFIFO_LEGACY; i++) {
			uint tmp = 0;
			if (wlc_pio_txdepthget(wlc_hw->pio[i]) == 0) {
				wlc_pio_txdepthset(wlc_hw->pio[i], (buf[i] << 8));

				tmp = wlc_pio_txdepthget(wlc_hw->pio[i]);
				if (tmp)
					wlc_pio_txdepthset(wlc_hw->pio[i], tmp - 4);
			}
		}
	}
}

/** Reset the PM duration accumulator maintained by SW */
static int
wlc_reset_accum_pmdur(wlc_info_t *wlc)
{
	wlc->pm_dur_clear_timeout = TIMEOUT_TO_READ_PM_DUR;
	wlc->wlc_pm_dur_last_sample =
		wlc_bmac_cca_read_counter(wlc->hw, M_MAC_SLPDUR_L_OFFSET(wlc),
			M_MAC_SLPDUR_H_OFFSET(wlc));
	return BCME_OK;
}

/**
 * On changing the MAC clock frequency, the tsf frac register must be adjusted accordingly.
 * If spur avoidance mode is off, the mac freq will be 80/120/160Mhz
 * If spur avoidance mode is on1, the mac freq will be 82/123/164Mhz
 * If spur avoidance mode is on2, the mac freq will be 84/126/168Mhz
 * Formula is 2^26/freq(MHz). Only called for specific chips. Is called by e.g. PHY code.
 * this function is called only by AC, N, LCN and HT PHYs.
 */
void
wlc_bmac_switch_macfreq(wlc_hw_info_t *wlc_hw, uint8 spurmode)
{
	osl_t *osh;

	ASSERT(WLCISNPHY(wlc_hw->band) || WLCISACPHY(wlc_hw->band) || WLCISLCN20PHY(wlc_hw->band));

	osh = wlc_hw->osh;

	/* ??? better keying, corerev, phyrev ??? */
	if (EMBEDDED_2x2AX_CORE(wlc_hw->sih->chip) ||
		BCM6878_CHIP(wlc_hw->sih->chip)) {
		/* TSF = 2^26 / MAC clock (in MHz) */
		if (spurmode == 1) {
			/* MAC 240.0431MHz */
			W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), 0x4412);
			W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), 0x4);
		} else {
			/* MAC 242.2504MHz */
			W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), 0x3A1F);
			W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), 0x4);
		}
	} else if (
		(CHIPID(wlc_hw->sih->chip) == BCM43217_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM43428_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM6362_CHIP_ID)) {
		if (spurmode == WL_SPURAVOID_ON2) {	/* 126Mhz */
			W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), 0x2082);
			W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), 0x8);
		} else if (spurmode == WL_SPURAVOID_ON1) {	/* 123Mhz */
			W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), 0x5341);
			W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), 0x8);
		} else {	/* 120Mhz */
			W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), 0x8889);
			W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), 0x8);
		}
	} else if (BCM53573_CHIP(wlc_hw->sih->chip) ||
		(BCM4347_CHIP(wlc_hw->sih->chip)) ||
		/* should this follow 63178 instead? Need to check with Kai */
		(BCM6878_CHIP(wlc_hw->sih->chip)) ||
		(BCM4369_CHIP(wlc_hw->sih->chip)) ||
		0)
	{
		uint32 mac_clk;
		uint32 clk_frac;
		uint16 frac_l, frac_h;
		uint32 r_high, r_low;

		mac_clk = si_mac_clk(wlc_hw->sih, wlc_hw->osh);

		/* the mac_clk is scaled by 1000 */
		/* so, multiplier for numerator will be 1 / (mac_clk / 1000): 1000 */
		math_uint64_multiple_add(&r_high, &r_low, (1 << 26), 1000, (mac_clk >> 1));
		math_uint64_divide(&clk_frac, r_high, r_low, mac_clk);

		frac_l =  (uint16)(clk_frac & 0xffff);
		frac_h =  (uint16)((clk_frac >> 16) & 0xffff);

		W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), frac_l);
		W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), frac_h);
	} else if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) ||
		(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
		BCM43602_CHIP(wlc_hw->sih->chip) ||
		BCM4350_CHIP(wlc_hw->sih->chip) ||
		0) {
		/*
		 * PR115835: Whenever driver changes BBPLL frequency it needs to change MAC clock
		 * frequency as well.
		 * mac_freq = bbpll_freq / 6.0
		 * clk_frac = (8.0/mac_freq) * 2^23
		 */
		uint32 bbpll_freq, clk_frac;
		uint32 xtalfreq = si_alp_clock(wlc_hw->sih);

		/*
		 * XXX: This assumes xtalfreq is 40Mhz; Need fix for 37.4Mhz
		 * XXX: For 4350 and dongle, use ucode default for now
		 */
		if (BCM4350_CHIP(wlc_hw->sih->chip) &&
			((BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) || (xtalfreq == 37400000))) {
			WL_ERROR(("%s: 4350 need fix for 37.4Mhz\n", __FUNCTION__));
			return;
		}

		bbpll_freq = si_pmu_get_bb_vcofreq(wlc_hw->sih, osh, 40); /* in [100Hz] units */

		/* 6 * 8 * 10000 * 2^23 = 0x3A980000000 */
		math_uint64_divide(&clk_frac, 0x3A9, 0x80000000, bbpll_freq);

		W_REG(osh, D11_TSF_CLK_FRAC_L(wlc_hw), clk_frac & 0xffff);
		W_REG(osh, D11_TSF_CLK_FRAC_H(wlc_hw), (clk_frac >> 16) & 0xffff);

	}
} /* wlc_bmac_switch_macfreq */

/**
 * This function controls the switching of 1x1 MAC frequency between 120MHz and 160MHz
 * in order to do sample capture
 */
void
wlc_bmac_switch_macfreq_dynamic(wlc_hw_info_t *wlc_hw, uint8 mode)
{
} /* wlc_bmac_switch_macfreq_dynamic */

/** Initialize GPIOs that are controlled by D11 core */
static void
BCMINITFN(wlc_gpio_init)(wlc_hw_info_t *wlc_hw)
{
	uint32 gc, gm;
	osl_t *osh = wlc_hw->osh;
#ifdef WLC_SW_DIVERSITY
	uint16 gpio_enbits;
	uint8 gpio_offset;
	uint32 val;
#endif /* WLC_SW_DIVERSITY */

	BCM_REFERENCE(osh);

	/* use GPIO select 0 to get all gpio signals from the gpio out reg */
	wlc_bmac_mctrl(wlc_hw, MCTL_GPOUT_SEL_MASK, 0);

	/*
	 * Common GPIO setup:
	 *	G0 = LED 0 = WLAN Activity
	 *	G1 = LED 1 = WLAN 2.4 GHz Radio State
	 *	G2 = LED 2 = WLAN 5 GHz Radio State
	 *	G4 = radio disable input (HI enabled, LO disabled)
	 * Boards that support BT Coexistence:
	 *	G7 = BTC
	 *	G8 = BTC
	 * Boards with chips that have fewer gpios and support BT Coexistence:
	 *	G4 = BTC
	 *	G5 = BTC
	 */

	gc = gm = 0;

#ifdef WL11N
	/* Allocate GPIOs for mimo antenna diversity feature */
	if (WLANTSEL_ENAB(wlc)) {
		if (wlc_hw->antsel_type == ANTSEL_2x3 || wlc_hw->antsel_type == ANTSEL_1x2_CORE1 ||
			wlc_hw->antsel_type == ANTSEL_1x2_CORE0) {
			/* Enable antenna diversity, use 2x3 mode */
			wlc_bmac_mhf(wlc_hw, MHF3, MHF3_ANTSEL_EN, MHF3_ANTSEL_EN, WLC_BAND_ALL);
			wlc_bmac_mhf(wlc_hw, MHF3, MHF3_ANTSEL_MODE, MHF3_ANTSEL_MODE,
				WLC_BAND_ALL);
		} else if ((wlc_hw->antsel_type == ANTSEL_2x4) &&
		           ((wlc_hw->sih->boardvendor != VENDOR_APPLE) ||
		            ((wlc_hw->sih->boardtype != BCM94350X14) &&
		             (wlc_hw->sih->boardtype != BCM94350X14P2)))) {
			/* X14 module use GPIO13 as input (Power Throttle) */
			/* XXX GPIO 8 is also defined for BTC_OUT.
			* Just make sure that we don't conflict
			*/
			ASSERT((gm & BOARD_GPIO_12) == 0);
			gm |= gc |= (BOARD_GPIO_12 | BOARD_GPIO_13);
			/* The board itself is powered by these GPIOs (when not sending pattern)
			* So set them high
			*/
			OR_REG(osh, D11_PSM_GPIOEN(wlc_hw), (BOARD_GPIO_12 | BOARD_GPIO_13));
			OR_REG(osh, D11_PSM_GPIOOUT(wlc_hw), (BOARD_GPIO_12 | BOARD_GPIO_13));

			/* Enable antenna diversity, use 2x4 mode */
			wlc_bmac_mhf(wlc_hw, MHF3, MHF3_ANTSEL_EN, MHF3_ANTSEL_EN, WLC_BAND_ALL);
			wlc_bmac_mhf(wlc_hw, MHF3, MHF3_ANTSEL_MODE, 0, WLC_BAND_ALL);

			/* Configure the desired clock to be 4Mhz */
			wlc_bmac_write_shm(wlc_hw, M_ANTSEL_CLKDIV(wlc_hw), ANTSEL_CLKDIV_4MHZ);
		}
	}
#endif /* WL11N */
	/* gpio 9 controls the PA.  ucode is responsible for wiggling out and oe */
	if (wlc_hw->boardflags & BFL_PACTRL)
		gm |= gc |= BOARD_GPIO_PACTRL;

	/* gpio 14(Xtal_up) and gpio 15(PLL_powerdown) are controlled in PCI config space */

#ifdef WLC_SW_DIVERSITY
	if (WLSWDIV_ENAB(wlc_hw->wlc) &&
		(wlc_swdiv_swctrl_en_get(wlc_hw->wlc->swdiv) == SWDIV_SWCTRL_0)) {
		wlc_swdiv_gpio_info_get(wlc_hw->wlc->swdiv, &gpio_offset, &gpio_enbits);
		val = gpio_enbits << gpio_offset;
		gm |= gc |= val;
		OR_REG(osh, D11_PSM_GPIOEN(wlc_hw), gm);
		WL_INFORM(("%s: GPIOctrl=0x%x, psm_gpio_oe=0x%x\n", __FUNCTION__,
			si_gpiocontrol(wlc_hw->sih, 0, 0, GPIO_DRV_PRIORITY),
			R_REG(osh, D11_PSM_GPIOEN(wlc_hw))));
	}
#endif /* WLC_SW_DIVERSITY */

	WL_INFORM(("wl%d: gpiocontrol mask 0x%x value 0x%x\n", wlc_hw->unit, gm, gc));

	/* apply to gpiocontrol register */
	si_gpiocontrol(wlc_hw->sih, gm, gc, GPIO_DRV_PRIORITY);
} /* wlc_gpio_init */

#ifdef ENABLE_CORECAPTURE

/**
 * Method of determining has changed over core revisions, so abstracts this to callee.
 *
 * @param[out] size_words  Ucode memory size in [32 bit word] units
 * @return                 BCME_ return code
 */
static int
wlc_bmac_get_ucode_mem_size(wlc_hw_info_t *wlc_hw, int *size_words)
{
	static const int ucm_size_map[MCAP_UCMSZ_TYPES_GE128] =
		{3328, 4096, 5120, 6144, 8192, 5120, 5120, 6656, 8192};
	int i;

	if (D11REV_GE(wlc_hw->corerev, 130)) {
		i = (wlc_hw->machwcap & MCAP_UCMSZ_MASK_GE130) >> MCAP_UCMSZ_SHIFT_GE130;
	} else if (D11REV_GE(wlc_hw->corerev, 128)) {
		i = (wlc_hw->machwcap & MCAP_UCMSZ_MASK_GE128) >> MCAP_UCMSZ_SHIFT_GE128;
	} else {
		i = (wlc_hw->machwcap & MCAP_UCMSZ_MASK) >> MCAP_UCMSZ_SHIFT;
	}

	if (i >= MCAP_UCMSZ_TYPES_GE128) {
		WL_ERROR(("%s: UCM size unsupported\n", __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	*size_words = ucm_size_map[i] * 2; /* UCM memory word size is 51 bits */

	return BCME_OK;
}

int
wlc_ucode_dump_ucm(wlc_hw_info_t *wlc_hw)
{
	int ucm_size;
	uint32 * ucm_mem;
	int ret;

	if (si_deviceremoved(wlc_hw->sih)) {
		return BCME_NODEVICE;
	}

	if (!wlc_hw->clk) {
		return BCME_NOCLK;
	}

	ret = wlc_bmac_get_ucode_mem_size(wlc_hw, &ucm_size);
	if (ret != BCME_OK) {
		return ret;
	}

	ucm_mem = (uint32*) MALLOC(wlc_hw->osh, ucm_size * sizeof(uint32));

	if (ucm_mem == NULL) {
		WL_ERROR(("%s: No memory : %d\n", __FUNCTION__, (int)(ucm_size * sizeof(uint32))));
		return BCME_NOMEM;
	}

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), (OBJADDR_AUTO_INC | OBJADDR_UCM_SEL));

	for (i = 0; i < ucm_size; i++) {
		ucm_mem[i] = R_REG(wlc_hw->osh, D11_objdata(wlc_hw));
	}

	wl_dump_mem((char *)ucm_mem, ucm_size * sizeof(uint32), WL_DUMP_MEM_UCM);

	MFREE(wlc_hw->osh, ucm_mem, ucm_size * sizeof(uint32));

	return BCME_OK;
}

#endif /* ENABLE_CORECAPTURE */

#ifndef BCMUCDOWNLOAD
static int
BCMUCODEFN(wlc_ucode_download)(wlc_hw_info_t *wlc_hw)
{
	int err = BCME_OK;
	int load_mu_ucode = 0;
	wlc_info_t *wlc = wlc_hw->wlc;
	int ctdma = 0;
#if defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED)

	if (wlc) {
		ctdma = BCM_DMA_CT_ENAB(wlc);
	} else {
		if (D11REV_GE(wlc_hw->corerev, 128)) {
			ctdma = TRUE; /* required in rev128 and up */
		} else if (D11REV_GE(wlc_hw->corerev, 65)) {
			ctdma = (getintvar(NULL, "ctdma") == 1);

			/* enable CTDMA in dongle mode if there is no ctdma specified in NVRAM */
			if ((BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) &&
				D11REV_IS(wlc_hw->corerev, 65) &&
				(getvar(NULL, "ctdma") == NULL)) {
				ctdma = TRUE;
			}
		}
	}

	if (D11REV_GE(wlc_hw->corerev, 128)) {
		/* regardless of MU, etc, always load MU ucode */
		load_mu_ucode = 1;
		if (wlc_hw->ucode_loaded)
			goto done;

	} else if (wlc && ctdma && (wlc->pub->mu_features & MU_FEATURES_MUTX)) {
		load_mu_ucode = 1;
	}
#else
	if (wlc_hw->ucode_loaded)
		goto done;
#endif /* defined(BCM_DMA_CT) && !defined(BCM_DMA_CT_DISABLED) */

	BCM_REFERENCE(wlc);

#if defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM)
	// process and enable patches
	if ((err = wlc_uci_download_rom_patches(wlc_hw)) != BCME_OK) {
		goto done;
	}
/* the "#else" below is intentional */
#else /* defined (UCODE_IN_ROM_SUPPORT) && !defined (ULP_DS1ROM_DS0RAM) */

#if defined(WLDIAG) /* DIAG ucode is available from d11 rev 65 upward */
	if (TRUE) {
		const uint32 *ucode32 = NULL;
		uint nbytes = 0;
		const uint32 *ucodex32 = NULL;
		uint nbytes_x = 0;

		if (ctdma && load_mu_ucode) {
			WL_ERROR(("%s: wl%d: Loading MU diags ucode\n",
				__FUNCTION__, wlc_hw->unit));
		} else {
			WL_ERROR(("%s: wl%d: Loading non-MU diags ucode\n",
				__FUNCTION__, wlc_hw->unit));
		}
		if (D11REV_IS(wlc_hw->corerev, 131)) {
			ASSERT(!load_mu_ucode); /* MU ucode is not ready yet for rev131 */
			ucode32 = d11ucode_maindiag130;
			nbytes = d11ucode_maindiag130sz;
			ucodex32 = d11ucodex_maindiag130;
			nbytes_x = d11ucodex_maindiag130sz;
		} else if (D11REV_IS(wlc_hw->corerev, 130)) {
			ASSERT(!load_mu_ucode); /* MU ucode is not ready yet for rev130 */
			if (wlc->pub->corerev_minor == 1) {
				ucode32 = d11ucode_maindiag130_1;
				nbytes = d11ucode_maindiag130_1sz;
				ucodex32 = d11ucodex_maindiag130_1;
				nbytes_x = d11ucodex_maindiag130_1sz;
			} else {
				ucode32 = d11ucode_maindiag130;
				nbytes = d11ucode_maindiag130sz;
				ucodex32 = d11ucodex_maindiag130;
				nbytes_x = d11ucodex_maindiag130sz;
			}
		} else if (D11REV_IS(wlc_hw->corerev, 129)) {
			ASSERT(!load_mu_ucode); /* MU ucode is not ready yet for rev129 */
			if (!wlc_hw->sih->otpflag) {
				ucode32 = d11ucode_maindiag129;
				nbytes = d11ucode_maindiag129sz;
				ucodex32 = d11ucodex_maindiag129;
				nbytes_x = d11ucodex_maindiag129sz;
			} else {
				ucode32 = d11ucode_maindiag129_1;
				nbytes = d11ucode_maindiag129_1sz;
				ucodex32 = d11ucodex_maindiag129_1;
				nbytes_x = d11ucodex_maindiag129_1sz;
			}
		} else if (D11REV_IS(wlc_hw->corerev, 128)) {
			ASSERT(!load_mu_ucode); /* MU ucode is not ready yet for rev128 */
			ucode32 = d11ucode_maindiag128;
			nbytes = d11ucode_maindiag128sz;
			ucodex32 = d11ucodex_maindiag128;
			nbytes_x = d11ucodex_maindiag128sz;
		} else if (D11REV_IS(wlc_hw->corerev, 65)) {
			if (BCM_DMA_CT_ENAB(wlc) && load_mu_ucode) {
				ucode32 = d11ucode_mudiag65;
				nbytes = d11ucode_mudiag65sz;
				ucodex32 = d11ucodex_mudiag65;
				nbytes_x = d11ucodex_mudiag65sz;
			} else {
				ucode32 = d11ucode_maindiag65;
				nbytes = d11ucode_maindiag65sz;
				ucodex32 = d11ucodex_maindiag65;
				nbytes_x = d11ucodex_maindiag65sz;
			}
		} else {
			ASSERT(0); /* DIAGS ucode not supported for this chip */
		}
		wlc_ucode_write(wlc_hw, ucode32, nbytes);
		wlc_ucodex_write(wlc_hw, ucodex32, nbytes_x);
	} else
#endif /* WLDIAG */

#if defined(WLP2P_UCODE)
	if (DL_P2P_UC(wlc_hw)) {
		const uint32 *ucode32 = NULL;
		const uint8 *ucode8 = NULL;
		uint nbytes = 0;
#if defined(WL_PSMX)
		const uint32 *ucodex32 = NULL;
		uint nbytes_x = 0;
#endif // endif

		if (D11REV_IS(wlc_hw->corerev, 61)) {
			WLC_WRITE_UCODE(wlc_hw, ucode32, nbytes,
				d11ucode_p2p61_1_D11b, d11ucode_p2p61_1_D11bsz,
				d11ucode_p2p61_1_D11b, d11ucode_p2p61_1_D11bsz);
		} else if (D11REV_IS(wlc_hw->corerev, 65)) {
			ucode32 = d11ucode_p2p65;
			nbytes = d11ucode_p2p65sz;
#if defined(WL_PSMX)
			ucodex32 = d11ucodex65;
			nbytes_x = d11ucodex65sz;
#endif // endif
		} else if (D11REV_IS(wlc_hw->corerev, 56)) {
			ucode32 = d11ucode_p2p56;
			nbytes = d11ucode_p2p56sz;
		} else if (D11REV_IS(wlc_hw->corerev, 49)) {
			ucode32 = d11ucode_p2p49;
			nbytes = d11ucode_p2p49sz;
		} else if (D11REV_IS(wlc_hw->corerev, 48)) {
			ucode32 = d11ucode_p2p48;
			nbytes = d11ucode_p2p48sz;
		} else if (D11REV_IS(wlc_hw->corerev, 42)) {
			ucode32 = d11ucode_p2p42;
			nbytes = d11ucode_p2p42sz;
		} else if (WLCISNPHY(wlc_hw->band)) {
			if (D11REV_IS(wlc_hw->corerev, 30)) {
				ucode32 = d11ucode_p2p30_mimo;
				nbytes = d11ucode_p2p30_mimosz;
			}
		}

		if (ucode32 != NULL) {
			wlc_ucode_write(wlc_hw, ucode32, nbytes);
#if defined(WL_PSMX)
			if (PSMX_ENAB(wlc->pub) && (ucodex32 != NULL)) {
				wlc_ucodex_write(wlc_hw, ucodex32, nbytes_x);
			}
#endif // endif
		}
		else if (ucode8 != NULL)
			wlc_ucode_write_byte(wlc_hw, ucode8, nbytes);
		else {
			WL_ERROR(("%s: wl%d: unsupported phy %d in corerev %d for P2P\n",
			          __FUNCTION__, wlc_hw->unit, wlc_hw->band->phytype,
			          wlc_hw->corerev));
			goto done;
		}
	} else
#endif /* WLP2P_UCODE */
	if (D11REV_IS(wlc_hw->corerev, 131)) {
		ASSERT(load_mu_ucode != 0);
		WL_ERROR(("%s: wl%d: Loading 131 MU ucode\n",
		          __FUNCTION__, wlc_hw->unit));
		wlc_ucode_write(wlc_hw, d11ucode_mu131, d11ucode_mu131sz);
		wlc_ucodex_write(wlc_hw, d11ucodex_mu131, d11ucodex_mu131sz);
	} else if (D11REV_IS(wlc_hw->corerev, 130)) {
		ASSERT(load_mu_ucode != 0);
		WL_ERROR(("%s: wl%d: Loading 130 MU ucode\n",
		          __FUNCTION__, wlc_hw->unit));
		if (wlc->pub->corerev_minor == 1) {
			wlc_ucode_write(wlc_hw, d11ucode_mu130_1, d11ucode_mu130_1sz);
			wlc_ucodex_write(wlc_hw, d11ucodex_mu130_1, d11ucodex_mu130_1sz);
		} else {
			wlc_ucode_write(wlc_hw, d11ucode_mu130, d11ucode_mu130sz);
			wlc_ucodex_write(wlc_hw, d11ucodex_mu130, d11ucodex_mu130sz);
		}
	} else if (D11REV_IS(wlc_hw->corerev, 129)) {
		ASSERT(load_mu_ucode != 0);
		WL_ERROR(("%s: wl%d: Loading 129 MU ucode\n",
		          __FUNCTION__, wlc_hw->unit));
		if (!wlc_hw->sih->otpflag) {
			wlc_ucode_write(wlc_hw, d11ucode_mu129, d11ucode_mu129sz);
			wlc_ucodex_write(wlc_hw, d11ucodex_mu129, d11ucodex_mu129sz);
		} else {
			if (BTCX_ENAB(wlc_hw)) {
				wlc_ucode_write(wlc_hw, d11ucode_btcxmu129_1,
						d11ucode_btcxmu129_1sz);
				wlc_ucodex_write(wlc_hw, d11ucodex_btcxmu129_1,
						d11ucodex_btcxmu129_1sz);
			} else {
				wlc_ucode_write(wlc_hw, d11ucode_mu129_1,
						d11ucode_mu129_1sz);
				wlc_ucodex_write(wlc_hw, d11ucodex_mu129_1,
						d11ucodex_mu129_1sz);
			}
		}
	} else if (D11REV_IS(wlc_hw->corerev, 128)) {
		ASSERT(load_mu_ucode != 0);
		WL_ERROR(("%s: wl%d: Loading 128 MU ucode\n",
		          __FUNCTION__, wlc_hw->unit));
		wlc_ucode_write(wlc_hw, d11ucode_mu128, d11ucode_mu128sz);
		wlc_ucodex_write(wlc_hw, d11ucodex_mu128, d11ucodex_mu128sz);
	} else if (D11REV_IS(wlc_hw->corerev, 61)) {
		if (wlc->pub->corerev_minor == 5) {
			wlc_ucode_write(wlc_hw, d11ucode61_5_D11b, d11ucode61_5_D11bsz);
			ASSERT(wlc_hw->macunit == 0);
		} else {
			if (wlc_hw->macunit == 0) {
				wlc_ucode_write(wlc_hw, d11ucode61_D11b, d11ucode61_D11bsz);
			} else if (wlc_hw->macunit == 1) {
				wlc_ucode_write(wlc_hw, d11ucode61_D11a, d11ucode61_D11asz);
			} else {
				/* Error condition */
				ASSERT(0);
			}
		}
	} else if (D11REV_IS(wlc_hw->corerev, 65)) {
		if (ctdma && load_mu_ucode) {
			WL_ERROR(("%s: wl%d: Loading MU ucode\n",
			          __FUNCTION__, wlc_hw->unit));
			wlc_ucode_write(wlc_hw, d11ucode_mu65, d11ucode_mu65sz);
			wlc_ucodex_write(wlc_hw, d11ucodex_mu65, d11ucodex_mu65sz);
		} else {
			WL_ERROR(("%s: wl%d: Loading non-MU ucode\n",
			          __FUNCTION__, wlc_hw->unit));
			wlc_ucode_write(wlc_hw, d11ucode65, d11ucode65sz);
			wlc_ucodex_write(wlc_hw, d11ucodex65, d11ucodex65sz);
		}
	} else if (D11REV_IS(wlc_hw->corerev, 56)) {
		wlc_ucode_write(wlc_hw, d11ucode56, d11ucode56sz);
	} else if (D11REV_IS(wlc_hw->corerev, 49)) {
		wlc_ucode_write(wlc_hw, d11ucode49, d11ucode49sz);
	} else if (D11REV_IS(wlc_hw->corerev, 48)) {
		wlc_ucode_write(wlc_hw, d11ucode48, d11ucode48sz);
	} else if (D11REV_IS(wlc_hw->corerev, 42)) {
		wlc_ucode_write(wlc_hw, d11ucode42, d11ucode42sz);
	} else if (D11REV_IS(wlc_hw->corerev, 30)) {
		if (WLCISNPHY(wlc_hw->band))
			wlc_ucode_write(wlc_hw, d11ucode30_mimo, d11ucode30_mimosz);
		else
			WL_ERROR(("%s: wl%d: unsupported phy in corerev %d\n",
			          __FUNCTION__, wlc_hw->unit, wlc_hw->corerev));
	} else {
		WL_ERROR(("wl%d: %s: corerev %d is invalid\n", wlc_hw->unit,
			__FUNCTION__, wlc_hw->corerev));
	}
#endif /* defined(UCODE_IN_ROM_SUPPORT) && !defined(ULP_DS1ROM_DS0RAM) */
	wlc_hw->ucode_loaded = TRUE;
	/* it's done for NIC case for coming back from wowl to p2p */
	wlc_bmac_autod11_shm_upd(wlc_hw, D11_IF_SHM_STD);
done:
	return err;
} /* wlc_ucode_download */
#endif /* BCMUCDOWNLOAD */

#if !defined(UCODE_IN_ROM_SUPPORT) || defined(ULP_DS1ROM_DS0RAM)
static void
BCMUCODEFN(wlc_ucode_write)(wlc_hw_info_t *wlc_hw, const uint32 ucode[], const uint nbytes)
{
	osl_t *osh = wlc_hw->osh;
	uint i;
	uint count;

	WL_TRACE(("wl%d: wlc_ucode_write\n", wlc_hw->unit));

	ASSERT(ISALIGNED(nbytes, sizeof(uint32)));

	count = (nbytes/sizeof(uint32));

	if (ucode_chunk == 0) {
		W_REG(osh, D11_objaddr(wlc_hw), (OBJADDR_AUTO_INC | OBJADDR_UCM_SEL));
		(void)R_REG(osh, D11_objaddr(wlc_hw));
	}
#ifdef BCM_UCODE_FILES
	if (!count) {
		char ucode_name[UCODE_NAME_LENGTH] = "/lib/firmware/brcm/d11ucode_";
		void *ucode_fp = NULL;
		int reads = UCODE_LINE_LENGTH;
		bcmstrncat(ucode_name, (char *)ucode, 4);
		bcmstrncat(ucode_name, ".txt", 4);
		ucode_fp = (void*)osl_os_open_image(ucode_name);
		if (ucode_fp) {
			uint ucode4 = 0;
			char *ucode_chars = ucode_name, *pucode;
			while (reads == UCODE_LINE_LENGTH) {
				memset(ucode_chars, 0, UCODE_LINE_LENGTH);
				reads = osl_os_get_image_block(ucode_chars, UCODE_LINE_LENGTH,
					ucode_fp);
				for (i = 0; i < 4; i ++) {
					pucode = ucode_chars + 12*i;
					if (pucode[0] != 0x09 ||
						pucode[1] != '0' || pucode[2] != 'x') {
						break;
					} else {
						ucode4 = bcm_strtoul(pucode, NULL, 0);
						W_REG(osh, D11_objdata(wlc_hw), ucode4);
					}
				}
			}
			osl_os_close_image(ucode_fp);
		}
	} else
#endif /* BCM_UCODE_FILES */
	for (i = 0; i < count; i++)
		W_REG(osh, D11_objdata(wlc_hw), ucode[i]);
#ifdef BCMUCDOWNLOAD
	ucode_chunk++;
#endif // endif
}

static void
wlc_ucodex_write(wlc_hw_info_t *wlc_hw, const uint32 ucode[], const uint nbytes)
{
	osl_t *osh = wlc_hw->osh;
	uint i;
	uint count;

	WL_TRACE(("wl%d: wlc_ucodex_write\n", wlc_hw->unit));

	ASSERT(ISALIGNED(nbytes, sizeof(uint32)));

	count = (nbytes/sizeof(uint32));

	if (ucode_chunk == 0) {
		W_REG(osh, D11_objaddr(wlc_hw), (OBJADDR_AUTO_INC | OBJADDR_UCMX_SEL));
		(void)R_REG(osh, D11_objaddr(wlc_hw));
	}
	for (i = 0; i < count; i++)
		W_REG(osh, D11_objdata(wlc_hw), ucode[i]);
#ifdef BCMUCDOWNLOAD
	ucode_chunk++;
#endif // endif
}

#if !defined(BCMUCDOWNLOAD) && defined(WLP2P_UCODE)
static void
BCMINITFN(wlc_ucode_write_byte)(wlc_hw_info_t *wlc_hw, const uint8 ucode[], const uint nbytes)
{
	osl_t *osh = wlc_hw->osh;
	uint i;
	uint32 ucode_word;

	WL_TRACE(("wl%d: wlc_ucode_write\n", wlc_hw->unit));

	if (ucode_chunk == 0)
		W_REG(osh, D11_objaddr(wlc_hw), (OBJADDR_AUTO_INC | OBJADDR_UCM_SEL));
	for (i = 0; i < nbytes; i += 7) {
		ucode_word = ucode[i+3] << 24;
		ucode_word = ucode_word | (ucode[i+4] << 16);
		ucode_word = ucode_word | (ucode[i+5] << 8);
		ucode_word = ucode_word | (ucode[i+6] << 0);
		W_REG(osh, D11_objdata(wlc_hw), ucode_word);

		ucode_word = ucode[i+0] << 16;
		ucode_word = ucode_word | (ucode[i+1] << 8);
		ucode_word = ucode_word | (ucode[i+2] << 0);
		W_REG(osh, D11_objdata(wlc_hw), ucode_word);
	}
#ifdef BCMUCDOWNLOAD
	ucode_chunk++;
#endif // endif
}
#endif // endif
#endif /* !defined(UCODE_IN_ROM_SUPPORT) || defined(ULP_DS1ROM_DS0RAM) */

#ifdef WLRSDB
static void
wlc_bmac_rsdb_write_inits(wlc_hw_info_t *wlc_hw, const d11init_t *common_inits,
	const d11init_t *core1_inits)
{
	/* For RSDB chips, download common initvals d11ac12bsinitvals50
	 * for both cores. Later download the core-1 specific initvals
	 * d11ac12bsinitvals50core1 if macunit is 1 which will overwrite
	 * the initvals d11ac12bsinitvals50 in some places.
	 */

	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		wlc_write_inits(wlc_hw, common_inits);

		/* If it is core-1, write core-1 inits */
		if (wlc_hw->macunit)
			wlc_write_inits(wlc_hw, core1_inits);
	}
}
#endif /* WLRSDB */

static void
wlc_write_inits(wlc_hw_info_t *wlc_hw, const d11init_t *inits)
{
	int i;
	osl_t *osh = wlc_hw->osh;
	volatile uint8 *base;

	WL_TRACE(("wl%d: wlc_write_inits\n", wlc_hw->unit));

	base = (volatile uint8*)wlc_hw->regs;

	for (i = 0; inits[i].addr != 0xffff; i++) {
		uint offset_val = 0;
		ASSERT((inits[i].size == 2) || (inits[i].size == 4));
		if (inits[i].addr == D11CORE_TEMPLATE_REG_OFFSET(wlc_hw)) {
			/* wlc_hw->templatebase is the template base address for core 1/0
			 * For core-0 it is zero and for core 1 it contains the core-1
			 * template offset.
			 */
			offset_val = wlc_hw->templatebase;
		}
		if (inits[i].size == 2)
			W_REG(osh, (uint16*)(uintptr)(base+inits[i].addr), inits[i].value +
			offset_val);
		else if (inits[i].size == 4)
			W_REG(osh, (uint32*)(uintptr)(base+inits[i].addr), inits[i].value +
			offset_val);
	}
}

#if defined(WL_PSMX)
static int
wlc_bmac_wowlucodex_start(wlc_hw_info_t *wlc_hw)
{
	/* let the PSM run to the suspended state, set mode to BSS STA */
	SET_MACINTSTATUS_X(wlc_hw->osh, wlc_hw, -1);
	wlc_bmac_mctrlx(wlc_hw, ~0, (MCTL_IHR_EN | MCTL_PSM_RUN));

	/* wait for ucode to self-suspend after auto-init */
	SPINWAIT(((GET_MACINTSTATUS_X(wlc_hw->osh, wlc_hw) & MI_MACSSPNDD) == 0), 1000 * 1000);

	if ((GET_MACINTSTATUS_X(wlc_hw->osh, wlc_hw) & MI_MACSSPNDD) == 0) {
		WL_ERROR(("wl%d: wlc_coreinit: ucode psmx did not self-suspend!\n", wlc_hw->unit));
		WL_HEALTH_LOG(wlc_hw->wlc, MACSPEND_WOWL_TIMOUT);

		wlc_dump_psmx_fatal(wlc_hw->wlc, PSMX_FATAL_SUSP);

		return BCME_ERROR;
	}
	return BCME_OK;
}
#endif /* WL_PSMX */

/**
 * Despite its function name, this function is also called for non-wowl ucode. At exit of this
 * function, the ucode has been suspended.
 */
int
wlc_bmac_wowlucode_start(wlc_hw_info_t *wlc_hw)
{
	uint32 mctrl = (MCTL_IHR_EN | MCTL_INFRA | MCTL_PSM_RUN | MCTL_WAKE);
	int err = BCME_OK;

	WL_ERROR(("wl%d: wlc_bmac_wowlucode_start mctrl=0x%x  0x%x\n", wlc_hw->unit, mctrl,
		R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw))));
	/* let the PSM run to the suspended state, set mode to BSS STA */
	SET_MACINTSTATUS(wlc_hw->osh, wlc_hw, -1);
	wlc_bmac_mctrl(wlc_hw, ~0, (mctrl | MCTL_PSM_JMP_0));

#if defined(WL_PSMR1)
	if (PSMR1_ENAB(wlc_hw)) {
		WL_INFORM(("wl%d: wlc_bmac_wowlucode_start mctrl1=0x%x  0x%x\n", wlc_hw->unit,
			mctrl, R_REG(wlc_hw->osh, D11_MACCONTROL_r1(wlc_hw))));
	}
#endif /* WL_PSMR1 */

	WL_ERROR(("wl%d: wlc_bmac_wowlucode_start (mctrl | MCTL_PSM_JMP_0)=0x%x 0x%x\n",
	wlc_hw->unit, (mctrl | MCTL_PSM_JMP_0), R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw))));
	wlc_bmac_mctrl(wlc_hw, ~0, mctrl);
	WL_ERROR(("wl%d: wlc_bmac_wowlucode_start mctrl=0x%x 0x%x\n", wlc_hw->unit, mctrl,
		R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw))));

	if (ISSIM_ENAB(wlc_hw->sih)) {
		int count = 0;
		WL_TRACE(("spin waiting for MI_MACSSPNDD..\n"));
		WL_ERROR(("spin waiting for MI_MACSSPNDD..\n"));
		/* wait for ucode to self-suspend after auto-init */
		SPINWAIT(((GET_MACINTSTATUS(wlc_hw->osh, wlc_hw) & MI_MACSSPNDD) == 0), 1);
		while (1) {
			if ((GET_MACINTSTATUS(wlc_hw->osh, wlc_hw) & MI_MACSSPNDD) == 0) {
				count++;
				WL_TRACE(("Further waiting...%d\n", count));
			} else {
				WL_TRACE(("Further waiting done in %d\n", count));
				break;
			}
			if (count > 1000) {
				break;
			}
			OSL_DELAY(10); /* 10usec */
		}
	} else {
		/* wait for ucode to self-suspend after auto-init */
		SPINWAIT(((GET_MACINTSTATUS(wlc_hw->osh, wlc_hw) & MI_MACSSPNDD) == 0),
			1000 * 1000);
	}

	if ((GET_MACINTSTATUS(wlc_hw->osh, wlc_hw) & MI_MACSSPNDD) == 0) {
		WL_ERROR(("wl%d: wlc_coreinit: ucode did not self-suspend!\n", wlc_hw->unit));
		WL_HEALTH_LOG(wlc_hw->wlc, MACSPEND_WOWL_TIMOUT);
		err = BCME_ERROR;
		wlc_dump_ucode_fatal(wlc_hw->wlc, PSM_FATAL_SUSP);
		goto exit;
	}

#ifdef WL_PSMX
	if (PSMX_ENAB(wlc_hw->wlc->pub)) {
		err = wlc_bmac_wowlucodex_start(wlc_hw);
	}
#endif // endif

	err = wlc_hw_verify_fifo_layout(wlc_hw);

exit:
	return err;
}

#ifdef WOWL
/* External API to write the ucode to avoid exposing the details */

#define BOARD_GPIO_3_WOWL 0x8 /* bit mask of 3rd pin */

#ifdef WOWL_GPIO
static bool
wlc_bmac_wowl_config_hw(wlc_hw_info_t *wlc_hw)
{
	/* configure the gpio etc to inform host to wake up etc */

	WL_TRACE(("wl: %s: corerev = 0x%x boardtype = 0x%x\n",  __FUNCTION__,
		wlc_hw->corerev, wlc_hw->sih->boardtype));

	if (!wlc_hw->clk) {
		WL_ERROR(("wl: %s: No hw clk \n",  __FUNCTION__));
		return FALSE;
	}

	if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		if (BCM43602_CHIP(wlc_hw->sih->chip) ||
			(((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID)) &&
			(CHIPREV(wlc_hw->sih->chiprev) <= 2))) {
			si_chipcontrl_srom4360(wlc_hw->sih, TRUE);
		}
	}

	si_gpiocontrol(wlc_hw->sih, 1 << wlc_hw->wowl_gpio, 0, GPIO_DRV_PRIORITY);
	si_gpioout(wlc_hw->sih, 1 << wlc_hw->wowl_gpio,
		wlc_hw->wowl_gpiopol << wlc_hw->wowl_gpio, GPIO_DRV_PRIORITY);
	si_gpioouten(wlc_hw->sih, 1 << wlc_hw->wowl_gpio,
		1 << wlc_hw->wowl_gpio, GPIO_DRV_PRIORITY);

	return TRUE;
}
#endif /* WOWL_GPIO */

int
wlc_bmac_wowlucode_init(wlc_hw_info_t *wlc_hw)
{
#ifdef WOWL_GPIO
	wlc_bmac_wowl_config_hw(wlc_hw);
#endif // endif

	if (!wlc_hw->clk) {
		WL_ERROR(("wl: %s: No hw clk \n",  __FUNCTION__));
		return BCME_ERROR;
	}

	/* Reset ucode. PSM_RUN is needed because current PC is not going to be 0 */
	wlc_bmac_mctrl(wlc_hw, ~0, (MCTL_IHR_EN | MCTL_PSM_JMP_0 | MCTL_PSM_RUN));

	return BCME_OK;
}
int
wlc_bmac_write_inits(wlc_hw_info_t *wlc_hw, void *inits, int len)
{
	BCM_REFERENCE(len);

	wlc_write_inits(wlc_hw, inits);

	return BCME_OK;
}

int
wlc_bmac_wakeucode_dnlddone(wlc_hw_info_t *wlc_hw)
{
#ifdef BCMULP
	/* Switch to point to ulp SHMs */
	wlc_bmac_autod11_shm_upd(wlc_hw, D11_IF_SHM_ULP);
#else /* BCMULP */
	/* Switch to point to wowl SHMs */
	wlc_bmac_autod11_shm_upd(wlc_hw, D11_IF_SHM_WOWL);
#endif /* BCMULP */

	/* tell the ucode the corerev */
	wlc_bmac_write_shm(wlc_hw, M_MACHW_VER(wlc_hw), (uint16)wlc_hw->corerev);

	/* overwrite default long slot timing */
	if (wlc_hw->shortslot)
		wlc_bmac_update_slot_timing(wlc_hw, wlc_hw->shortslot);

	/* write rate fallback retry limits */
	wlc_bmac_write_shm(wlc_hw, M_SFRMTXCNTFBRTHSD(wlc_hw), wlc_hw->SFBL);
	wlc_bmac_write_shm(wlc_hw, M_LFRMTXCNTFBRTHSD(wlc_hw), wlc_hw->LFBL);

	/* Restore the hostflags */
	wlc_write_mhf(wlc_hw, wlc_hw->band->mhfs);

	/* make sure we can still talk to the mac */
	ASSERT(R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) != 0xffffffff);

	wlc_bmac_mctrl(wlc_hw, MCTL_DISCARD_PMQ, MCTL_DISCARD_PMQ);

	wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);

	wlc_bmac_upd_synthpu(wlc_hw);

#ifdef WOWL_GPIO
	OR_REG(wlc_hw->osh, D11_PSM_GPIOEN(wlc_hw), 1 << wlc_hw->wowl_gpio);
	OR_REG(wlc_hw->osh, D11_PSM_GPIOOUT(wlc_hw), wlc_hw->wowl_gpiopol << wlc_hw->wowl_gpio);

	si_gpiocontrol(wlc_hw->sih, 1 << wlc_hw->wowl_gpio, 1 << wlc_hw->wowl_gpio,
			GPIO_DRV_PRIORITY);

#endif /* WOWL_GPIO */
	return BCME_OK;
}
#endif /* WOWL */

#ifdef SAMPLE_COLLECT
/**
 * Load sample collect ucode
 * Ucode inits the SHM and all MAC regs
 * can support all PHY types, implement NPHY for now.
 */
static void
wlc_ucode_sample_init_rev(wlc_hw_info_t *wlc_hw, const uint32 ucode[], const uint nbytes)
{
	if (WLCISNPHY(wlc_hw->band)) {
	/* Restart the ucode (recover from wl out) */
		wlc_bmac_mctrl(wlc_hw, ~0, (MCTL_IHR_EN | MCTL_PSM_RUN | MCTL_EN_MAC));
		return;
	}

	/* Reset ucode. PSM_RUN is needed because current PC is not going to be 0 */
	wlc_bmac_mctrl(wlc_hw, ~0, (MCTL_IHR_EN | MCTL_PSM_JMP_0 | MCTL_PSM_RUN));

	/* Load new d11ucode */
#if !defined(UCODE_IN_ROM_SUPPORT) || defined(ULP_DS1ROM_DS0RAM)
	wlc_ucode_write(wlc_hw, ucode, nbytes);
#endif /* !defined(UCODE_IN_ROM_SUPPORT) || defined(ULP_DS1ROM_DS0RAM) */

	(void) wlc_bmac_wowlucode_start(wlc_hw);

	/* make sure we can still talk to the mac */
	ASSERT(R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) != 0xffffffff);
}

void
wlc_ucode_sample_init(wlc_hw_info_t *wlc_hw)
{
	wlc_ucode_sample_init_rev(wlc_hw, d11sampleucode16, d11sampleucode16sz);
}
#endif	/* SAMPLE_COLLECT */

static void
wlc_ucode_txant_set(wlc_hw_info_t *wlc_hw)
{
	uint16 phyctl;
	uint16 phytxant = wlc_hw->bmac_phytxant;
	uint16 mask = PHY_TXC_ANT_MASK;

	if (D11REV_GE(wlc_hw->corerev, 40)) {
		WL_INFORM(("wl%d: %s: need rev40 update\n", wlc_hw->unit, __FUNCTION__));
		return;
	}

	/* XXX HT FIXME, pub is not available in BMAC driver
	 * if (HT_ENAB(wlc_hw->wlc->pub))
	 *	  mask = PHY_TXC_HTANT_MASK;
	 */

	/* set the Probe Response frame phy control word */
	phyctl = wlc_bmac_read_shm(wlc_hw, M_CTXPRS_BLK(wlc_hw) + C_CTX_PCTLWD_POS);
	phyctl = (phyctl & ~mask) | phytxant;
	wlc_bmac_write_shm(wlc_hw, M_CTXPRS_BLK(wlc_hw) + C_CTX_PCTLWD_POS, phyctl);

	/* set the Response (ACK/CTS) frame phy control word */
	phyctl = wlc_bmac_read_shm(wlc_hw, M_RSP_PCTLWD(wlc_hw));
	phyctl = (phyctl & ~mask) | phytxant;
	wlc_bmac_write_shm(wlc_hw, M_RSP_PCTLWD(wlc_hw), phyctl);
}

void
wlc_bmac_txant_set(wlc_hw_info_t *wlc_hw, uint16 phytxant)
{
	/* update sw state */
	wlc_hw->bmac_phytxant = phytxant;

	/* push to ucode if up */
	if (!wlc_hw->up)
		return;
#ifdef WLC_SW_DIVERSITY
	if (WLSWDIV_ENAB(wlc_hw->wlc))
		return;
#endif /* WLC_SW_DIVERSITY */
	wlc_ucode_txant_set(wlc_hw);

}

uint16
wlc_bmac_get_txant(wlc_hw_info_t *wlc_hw)
{
	return (uint16)wlc_hw->wlc->stf->txant;
}

void
wlc_bmac_antsel_type_set(wlc_hw_info_t *wlc_hw, uint8 antsel_type)
{
	wlc_hw->antsel_type = antsel_type;

	/* Update the antsel type for phy module to use */
	phy_antdiv_antsel_type_set(wlc_hw->band->pi, antsel_type);
}

bool
wlc_bmac_pktpool_empty(wlc_info_t *wlc)
{
	if (POOL_ENAB(wlc->pub->pktpool))
		return (pktpool_avail(wlc->pub->pktpool) == 0);
	return FALSE;
}

void
wlc_bmac_fifoerrors(wlc_hw_info_t *wlc_hw)
{
	bool fatal = FALSE;
	uint unit;
	uint intstatus, idx;

	unit = wlc_hw->unit;
	BCM_REFERENCE(unit);

#ifdef BCMHWA
	/* SW shouldn't change indqsel when HWA 3b enabled */
	HWA_TXFIFO_EXPR(ASSERT(BCM_DMA_CT_ENAB(wlc_hw->wlc)));
	HWA_TXFIFO_EXPR(return);
#endif // endif

	for (idx = 0; idx < WLC_HW_NFIFO_INUSE(wlc_hw->wlc); idx++) {
#if defined(WL_MU_TX) && !defined(WL_MU_TX_DISABLED)
		/* skip FIFO #6 and #7 as they are not used */
		if ((idx == TX_FIFO_6)||(idx == TX_FIFO_7)) {
			continue;
		}
#endif /* #if defined(WL_MU_TX) && !defined(WL_MU_TX_DISABLED) */
		/* read intstatus register and ignore any non-error bits */
		if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
			if (wlc_hw->aqm_di[idx] == NULL)
				continue;
			dma_set_indqsel(wlc_hw->aqm_di[idx], FALSE);
			intstatus = R_REG(wlc_hw->osh,
				(&(D11Reggrp_indaqm(wlc_hw, 0)->indintstatus))) & I_ERRORS;
		}
		else {
			intstatus = R_REG(wlc_hw->osh,
				&(D11Reggrp_intctrlregs(wlc_hw, idx)->intstatus)) & I_ERRORS;
		}
		if (!intstatus)
			continue;

		WL_TRACE(("wl%d: wlc_bmac_fifoerrors: intstatus%d 0x%x\n", unit, idx, intstatus));

		if (intstatus & I_RO) {
			WL_ERROR(("wl%d: fifo %d: receive fifo overflow\n", unit, idx));
			WLCNTINCR(wlc_hw->wlc->pub->_cnt->rxoflo);
			fatal = TRUE;
		}

		if (intstatus & I_PC) {
			WL_ERROR(("wl%d: fifo %d: descriptor error\n", unit, idx));
			WLCNTINCR(wlc_hw->wlc->pub->_cnt->dmade);
			fatal = TRUE;
		}

		if (intstatus & I_PD) {

			WL_ERROR(("wl%d: fifo %d: data error\n", unit, idx));

			WLCNTINCR(wlc_hw->wlc->pub->_cnt->dmada);
			fatal = TRUE;
		}

		if (intstatus & I_DE) {
			WL_ERROR(("wl%d: fifo %d: descriptor protocol error\n", unit, idx));
			WLCNTINCR(wlc_hw->wlc->pub->_cnt->dmape);
			fatal = TRUE;
		}

		if (intstatus & I_RU) {
			WL_ERROR(("wl%d: fifo %d: receive descriptor underflow\n", unit, idx));
			WLCNTINCR(wlc_hw->wlc->pub->_cnt->rxuflo[idx]);
		}

		if (intstatus & I_XU) {
			WL_ERROR(("wl%d: fifo %d: transmit fifo underflow\n", idx, unit));
			WLCNTINCR(wlc_hw->wlc->pub->_cnt->txuflo);
			fatal = TRUE;
		}

#ifdef BCMDBG
		{
			/* dump dma rings to console */
			const int FIFOERROR_DUMP_SIZE = 16384;
			char *tmp;
			struct bcmstrbuf b;
			if (fatal && !PIO_ENAB_HW(wlc_hw) && wlc_hw->di[idx] &&
			    (tmp = MALLOC(wlc_hw->osh, FIFOERROR_DUMP_SIZE))) {
				bcm_binit(&b, tmp, FIFOERROR_DUMP_SIZE);
				dma_dump(wlc_hw->di[idx], &b, TRUE);
				printbig(tmp);
				MFREE(wlc_hw->osh, tmp, FIFOERROR_DUMP_SIZE);
			}
		}

#endif /* BCMDBG */

		if (fatal) {
			WL_HEALTH_LOG(wlc_hw->wlc, DESCRIPTOR_ERROR);
			wlc_fatal_error(wlc_hw->wlc);	/* big hammer */
			break;
		} else {
			if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
				if (wlc_hw->aqm_di[idx] == NULL)
					continue;
				dma_set_indqsel(wlc_hw->aqm_di[idx], FALSE);
				W_REG(wlc_hw->osh, (&(D11Reggrp_indaqm(wlc_hw, 0)->indintstatus)),
					intstatus);
			}
			else {
				W_REG(wlc_hw->osh, &(D11Reggrp_intctrlregs(wlc_hw, idx)->intstatus),
					intstatus);
			}
		}
	}
} /* wlc_bmac_fifoerrors */

/**
 * callback for siutils.c, which has only wlc handler, no wl
 * they both check up, not only because there is no need to off/restore d11 interrupt
 *  but also because per-port code may require sync with valid interrupt.
 */
static uint32
wlc_wlintrsoff(wlc_hw_info_t *wlc_hw)
{
	if (!wlc_hw->up)
		return 0;

	return wl_intrsoff(wlc_hw->wlc->wl);
}

static void
wlc_wlintrsrestore(wlc_hw_info_t *wlc_hw, uint32 macintmask)
{
	if (!wlc_hw->up)
		return;

	wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
}

#ifdef BCMDBG
static bool
wlc_intrs_enabled(wlc_hw_info_t *wlc_hw)
{
	return (wlc_hw->macintmask != 0);
}
#endif /* BCMDBG */

/** Halts transmit operation when 'on' is TRUE */
void
wlc_bmac_mute(wlc_hw_info_t *wlc_hw, bool on, mbool flags)
{
#define MUTE_DATA_FIFO	TX_DATA_FIFO

	if (on) {
		/* suspend tx fifos */
		wlc_bmac_tx_fifo_suspend(wlc_hw, MUTE_DATA_FIFO);
		wlc_bmac_tx_fifo_suspend(wlc_hw, TX_CTL_FIFO);
		wlc_bmac_tx_fifo_suspend(wlc_hw, TX_AC_BK_FIFO);
#ifdef WME
		wlc_bmac_tx_fifo_suspend(wlc_hw, TX_AC_VI_FIFO);
#endif /* WME */
#ifdef AP
		wlc_bmac_tx_fifo_suspend(wlc_hw, TX_BCMC_FIFO);
#endif /* AP */
#ifdef MBSS
		wlc_bmac_tx_fifo_suspend(wlc_hw, TX_ATIM_FIFO);
#endif /* MBSS */

		/* clear the address match register so we do not send ACKs */
		wlc_bmac_clear_match_mac(wlc_hw);
	} else {
		/* resume tx fifos */
		if (!wlc_hw->wlc->tx_suspended) {
			wlc_bmac_tx_fifo_resume(wlc_hw, MUTE_DATA_FIFO);
		}
		wlc_bmac_tx_fifo_resume(wlc_hw, TX_CTL_FIFO);
		wlc_bmac_tx_fifo_resume(wlc_hw, TX_AC_BK_FIFO);
#ifdef WME
		wlc_bmac_tx_fifo_resume(wlc_hw, TX_AC_VI_FIFO);
#endif /* WME */
#ifdef AP
		wlc_bmac_tx_fifo_resume(wlc_hw, TX_BCMC_FIFO);
#endif /* AP */
#ifdef MBSS
		wlc_bmac_tx_fifo_resume(wlc_hw, TX_ATIM_FIFO);
#endif /* MBSS */

		/* Restore address */
		wlc_bmac_set_match_mac(wlc_hw, &wlc_hw->etheraddr);
	}

	wlc_phy_mute_upd(wlc_hw->band->pi, on, flags);

	if (on)
		wlc_ucode_mute_override_set(wlc_hw);
	else
		wlc_ucode_mute_override_clear(wlc_hw);
} /* wlc_bmac_mute */

void
wlc_bmac_set_deaf(wlc_hw_info_t *wlc_hw, bool user_flag)
{
	wlc_phy_set_deaf(wlc_hw->band->pi, user_flag);
}

void
wlc_bmac_clear_deaf(wlc_hw_info_t *wlc_hw, bool user_flag)
{
	wlc_phy_clear_deaf(wlc_hw->band->pi, user_flag);
}

void
wlc_bmac_filter_war_upd(wlc_hw_info_t *wlc_hw, bool set)
{
	phy_misc_set_filt_war(wlc_hw->band->pi, set);
}

void
wlc_bmac_lo_gain_nbcal_upd(wlc_hw_info_t *wlc_hw, bool set)
{
	phy_misc_set_lo_gain_nbcal(wlc_hw->band->pi, set);
}

int
wlc_bmac_xmtfifo_sz_get(wlc_hw_info_t *wlc_hw, uint fifo, uint *blocks)
{
	if (fifo >= NFIFO_LEGACY) {
		WL_ERROR(("wl%d: %s: Out of range fifo:%d\n", wlc_hw->unit, __FUNCTION__, fifo));
		return BCME_RANGE;
	}

	*blocks = wlc_hw->xmtfifo_sz[fifo];

	return 0;
}

int
wlc_bmac_xmtfifo_sz_set(wlc_hw_info_t *wlc_hw, uint fifo, uint16 blocks)
{
	if (fifo >= NFIFO_LEGACY || blocks > 299) {
		WL_ERROR(("wl%d: %s: fifo pair count %d or blocks %d not in range",
			wlc_hw->unit, __FUNCTION__, fifo, blocks));
		return BCME_RANGE;
	}

	wlc_hw->xmtfifo_sz[fifo] = blocks;

	return 0;
}

/* Wrapper for dma_rxfill() that keeps counts of rxfill attempts */
bool
wlc_bmac_dma_rxfill(wlc_hw_info_t *wlc_hw, uint fifo)
{
	bool success;
	wlc_info_t *wlc = wlc_hw->wlc;

	BCM_REFERENCE(wlc);

	ASSERT(fifo < WLC_HW_NFIFO_INUSE(wlc_hw->wlc));
#ifdef STS_FIFO_RXEN
	if (STS_RX_ENAB(wlc->pub) && (fifo == STS_FIFO)) {
		success = dma_sts_rxfill(wlc_hw->di[fifo]);
	} else
#endif // endif
	{
		success = dma_rxfill(wlc_hw->di[fifo]);
	}
#if defined(WL_RX_DMA_STALL_CHECK)
	/* only count if successful */
	if (success) {
		WLCNTINCR(wlc_hw->rx_stall->dma_stall->rxfill[fifo]);
	}
#endif // endif
	return success;
}

/* Get the TX FIFO FLUSH status
 *
 * Get the specified TX FIFO flush status
 * Use a function to accomandate the different flush register address
 * where indirect DMA feature is insdie.
 *
 * return the corresponding channel status bit
 * caller can check if the return value is 0 or not to judge the status
 */
static uint32
wlc_bmac_txfifo_flush_status(wlc_hw_info_t *wlc_hw, uint fifo)
{
	volatile uint32 *chnflushstatus = NULL;
	uint32 chnmask = 0;

	if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
		ASSERT(fifo < WLC_HW_NFIFO_INUSE(wlc_hw->wlc));
		fifo = WLC_HW_MAP_TXFIFO(wlc_hw->wlc, fifo);
		if (fifo >= 64) {
			ASSERT(fifo < 96);
			chnflushstatus = (volatile uint32 *)D11_CHNFLUSH_STATUS2(wlc_hw);
			chnmask = (0x1 << (fifo - 64));
		} else if (fifo >= 32) {
			chnflushstatus = (volatile uint32 *)D11_CHNFLUSH_STATUS1(wlc_hw);
			chnmask = (0x1 << (fifo - 32));
		} else {
			chnflushstatus = (volatile uint32 *)D11_CHNFLUSH_STATUS(wlc_hw);
			chnmask = (0x1 << fifo);
		}
	} else {
		ASSERT(fifo < NFIFO_LEGACY);
		chnflushstatus = D11_CHNSTATUS(wlc_hw);
		chnmask = (0x100 << fifo);
	}

	return (R_REG(wlc_hw->osh, chnflushstatus) & chnmask);
}

/* Get the TX FIFO SUSPEND status
 *
 * Get the specified TX FIFO suspend status
 * Use a function to accomandate the different flush register address
 * where indirect DMA feature is insdie
 */
static uint32
wlc_bmac_txfifo_suspend_status(wlc_hw_info_t *wlc_hw, uint fifo)
{
	volatile uint32 *chnsuspstatus = NULL;
	uint32 chnmask = 0;

	if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
		ASSERT(fifo < WLC_HW_NFIFO_INUSE(wlc_hw->wlc));
		fifo = WLC_HW_MAP_TXFIFO(wlc_hw->wlc, fifo);
		if (fifo >= 64) {
			ASSERT(fifo < 96);
			chnsuspstatus = (volatile uint32 *)D11_CHNSUSP_STATUS2(wlc_hw);
			chnmask = (0x1 << (fifo - 64));
		} else if (fifo >= 32) {
			chnsuspstatus = (volatile uint32 *)D11_CHNSUSP_STATUS1(wlc_hw);
			chnmask = (0x1 << (fifo - 32));
		} else {
			chnsuspstatus = (volatile uint32 *)D11_CHNSUSP_STATUS(wlc_hw);
			chnmask = (0x1 << fifo);
		}
	} else {
		ASSERT(fifo < NFIFO_LEGACY);
		chnsuspstatus = D11_CHNSTATUS(wlc_hw);
		chnmask = (0x1 << fifo);
	}

	return (R_REG(wlc_hw->osh, chnsuspstatus) & chnmask);
}

/**
 * Check the MAC's tx suspend status for a tx fifo.
 *
 * When the MAC acknowledges a tx suspend, it indicates that no more packets will be transmitted out
 * the radio. This is independent of DMA channel suspension---the DMA may have finished suspending,
 * or may still be pulling data into a tx fifo, by the time the MAC acks the suspend request.
 */
bool
wlc_bmac_tx_fifo_suspended(wlc_hw_info_t *wlc_hw, uint tx_fifo)
{
	/* check that a suspend has been requested and is no longer pending */
	if (!PIO_ENAB_HW(wlc_hw)) {
		/*
		 * for DMA mode, the suspend request is set in xmtcontrol of the DMA engine,
		 * and the tx fifo suspend at the lower end of the MAC is acknowledged in the
		 * chnstatus register.
		 * for indirect DMA mode , the suspend request is set in common register, SuspReq
		 * and suspend status is reflected in SuspStatus register.
		 * The tx fifo suspend completion is independent of the DMA suspend completion and
		 *   may be acked before or after the DMA is suspended.
		 */
		ASSERT(tx_fifo < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc));
		if (dma_txsuspended(wlc_hw->di[tx_fifo]) &&
		   wlc_bmac_txfifo_suspend_status(wlc_hw, tx_fifo) == 0) {
			return TRUE;
		}
	} else {
		ASSERT(tx_fifo < NFIFO_LEGACY);
		if (wlc_pio_txsuspended(wlc_hw->pio[tx_fifo]))
			return TRUE;
	}

	return FALSE;
}

#define SUSPEND_FLUSH_TIMEOUT 80000

static void
wlc_bmac_tx_fifo_suspend_wait(wlc_hw_info_t *wlc_hw, uint tx_fifo)
{
	/* Before enabling the MAC, ensure that the ucode has suspended the fifo */
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		uint32 chnstatus;
		d11regs_t *regs = wlc_hw->regs;
		osl_t *osh = wlc_hw->osh;
		dma64regs_t *d64regs = &(D11Reggrp_f64regs(wlc_hw, tx_fifo)->dmaxmt);

		BCM_REFERENCE(d64regs);
		BCM_REFERENCE(regs);

		WL_MQ(("%s: txfifo 0x%x\n", __FUNCTION__, tx_fifo));

		/* Wait for channel status to be cleared, do not wait for dma status to be idle
		*   If the FIFO is full, it will not go to idle
		*/
		if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
			SPINWAIT((chnstatus = wlc_bmac_txfifo_suspend_status(wlc_hw, tx_fifo)) != 0,
			    SUSPEND_FLUSH_TIMEOUT);
		} else {
			SPINWAIT((chnstatus = R_REG(osh, D11_CHNSTATUS(wlc_hw))) != 0,
			    SUSPEND_FLUSH_TIMEOUT);
		}

		/* When a xmtCtrl.suspend is done when the BM is full,
		 * the TxDMA  engine will move into SuspendPending State (xmtStatus0[31:28] = 0x4)
		 * and will not return to Idle, until the BM is freed of some space.
		 * The space in BM will be freed only after MAC is resumed.
		 * However the ucode finishes the suspend sequence
		 * and the CHNSTATUS.SuspPend bit will be cleared. So checking for dmastatus
		 * is not required. Check for dma status is required only if we doing
		 * BM flush.
		 */
		if (chnstatus) {
			if (D11REV_GE(wlc_hw->corerev, 128)) {
				WL_ERROR(("wl%d:%s:MQ ERROR, failure, suspend of dma %d not done ,"
					" chnstatus 0x%04x\n dma_ctrl: 0x%x lo: 0x%x "
					"hi: 0x%x ptr: 0x%x s0: 0x%x s1: 0x%x\n"
					"AQM_QMAP 0x%x AQMF_STATUS 0x%x AQMCT_PRIFIFO: 0x%x\n",
					wlc_hw->unit, __FUNCTION__,
					tx_fifo, chnstatus,
					R_REG(osh, &d64regs->control),
					R_REG(osh, &d64regs->addrlow),
					R_REG(osh, &d64regs->addrhigh),
					R_REG(osh, &d64regs->ptr),
					R_REG(osh, &d64regs->status0),
					R_REG(osh, &d64regs->status1),
					R_REG(osh, D11_AQM_QMAP(wlc_hw)),
					R_REG(osh, D11_AQMF_STATUS(wlc_hw)),
					R_REG(osh, D11_AQMCT_PRIFIFO(wlc_hw))));
			} else {
				WL_ERROR(("wl%d:%s:MQ ERROR, failure, suspend of dma %d not done ,"
					" chnstatus 0x%04x\n dma_ctrl: 0x%x lo: 0x%x "
					"hi: 0x%x ptr: 0x%x s0: 0x%x s1: 0x%x\n"
					"txefs: 0x%04x BMCReadStatus: 0x%04x "
					"AQMFifoReady: 0x%04x\n",
					wlc_hw->unit, __FUNCTION__,
					tx_fifo, chnstatus,
					R_REG(osh, &d64regs->control),
					R_REG(osh, &d64regs->addrlow),
					R_REG(osh, &d64regs->addrhigh),
					R_REG(osh, &d64regs->ptr),
					R_REG(osh, &d64regs->status0),
					R_REG(osh, &d64regs->status1),
					R_REG(osh, D11_XmtSuspFlush(wlc_hw)),
					R_REG(osh, D11_BMCReadStatus(wlc_hw)),
					R_REG(osh, D11_AQMFifoReady(wlc_hw))));
			}

			if (wlc_hw->need_reinit == WL_REINIT_RC_NONE) {
				wlc_hw->need_reinit = WL_REINIT_RC_MQ_ERROR;
			}
			/* Note that using wlc_fatal_error() below causes re-entrancy in
			 * wlc_chanspec_set_chanspec() causing an assert
			 */
			wlc_dump_ucode_fatal(wlc_hw->wlc, PSM_FATAL_TXSUFL);
		}
	}
}

void
wlc_bmac_tx_fifo_suspend(wlc_hw_info_t *wlc_hw, uint tx_fifo)
{
	/* Two clients of this code, 11h Quiet period and scanning. */

	ASSERT(tx_fifo < WLC_HW_NFIFO_INUSE(wlc_hw->wlc));

#ifdef WL_PSMX
	/* When PSMx is used, it needs to be enabled before we can suspend fifos */
	if (PSMX_ENAB(wlc_hw->wlc->pub) && (wlc_hw->macx_suspend_depth > 0))
	{
		WL_INFORM(("%s, not suspending FIFO %d, PSMx not yet ready\n",
			__FUNCTION__, tx_fifo));
		return;
	}
#endif /* WL_PSMX */

	/* Do nothing if already suspended */
	if (wlc_upd_suspended_fifos_set(wlc_hw, tx_fifo) == FALSE) {
		return;
	}

	if (!PIO_ENAB_HW(wlc_hw)) {
		if (wlc_hw->di[tx_fifo]) {
			bool suspend;

			/* Suspending AMPDU transmissions in the middle can cause underflow
			 * which may result in mismatch between ucode and driver
			 * so suspend the mac before suspending the FIFO
			 */
			suspend = !(R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC);

			if (WLC_PHY_11N_CAP(wlc_hw->band) && !suspend)
				wlc_bmac_suspend_mac_and_wait(wlc_hw);

			/* after dma tx suspend for a specifc channel , before going for txresume
			  *  corresponding chnstatus bit needs to checked for zero
			  *  else it could have some bad  impact on dma engine
			 */
			dma_txsuspend(wlc_hw->di[tx_fifo]);

			wlc_bmac_tx_fifo_suspend_wait(wlc_hw, tx_fifo);

			if (WLC_PHY_11N_CAP(wlc_hw->band) && !suspend)
				wlc_bmac_enable_mac(wlc_hw);
		}
	} else {
		ASSERT(tx_fifo < NFIFO_LEGACY);
		wlc_pio_txsuspend(wlc_hw->pio[tx_fifo]);
	}
}

void
wlc_bmac_tx_fifo_resume(wlc_hw_info_t *wlc_hw, uint tx_fifo)
{
	/* Two clients of this code, 11h Quiet period and scanning. */

	ASSERT(tx_fifo < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc));

	if (!PIO_ENAB_HW(wlc_hw)) {
		if (wlc_hw->di[tx_fifo])
			dma_txresume(wlc_hw->di[tx_fifo]);
	} else {
		ASSERT(tx_fifo < NFIFO_LEGACY);
		wlc_pio_txresume(wlc_hw->pio[tx_fifo]);
		/* BMAC_NOTE: XXX This macro uses high level state, we need to do something
		 * about it. PIO fifo needs to be enabled when tx fifo got resumed
		 */
	}

	/* allow core to sleep again */
	wlc_upd_suspended_fifos_clear(wlc_hw, tx_fifo);
}

static void wlc_bmac_service_txstatus(wlc_hw_info_t *wlc_hw);
static void wlc_bmac_flush_tx_fifos(wlc_hw_info_t *wlc_hw, void *fifo_bitmap);
static void wlc_bmac_uflush_tx_fifos(wlc_hw_info_t *wlc_hw, uint fifo_bitmap);
static void wlc_bmac_enable_tx_fifos(wlc_hw_info_t *wlc_hw, void *fifo_bitmap);

/**
 * Called during e.g. joining, excursion, channel switch. Suspends ucode, flushes all tx packets in
 * a caller provided set of hardware fifo's, waits for flush complete, processes all tx statuses,
 * removes remaining packets from the DMA ring and notifies upper layer that the flush completed.
 */
void
wlc_bmac_tx_fifo_sync_all(wlc_hw_info_t *wlc_hw, uint8 flag)
{
	uint8 fifo_bitmap[TX_FIFO_BITMAP_SIZE_MAX];

	memset(fifo_bitmap, 0xff, sizeof(fifo_bitmap));
	wlc_bmac_tx_fifo_sync(wlc_hw, fifo_bitmap, flag);
}

void
wlc_bmac_tx_fifo_sync(wlc_hw_info_t *wlc_hw, void *fifo_bitmap, uint8 flag)
{
	uint i;

#ifdef BCM_BACKPLANE_TIMEOUT
	if (si_deviceremoved(wlc_hw->wlc->pub->sih)) {
		return;
	}
#endif /* BCM_BACKPLANE_TIMEOUT */

	ASSERT(fifo_bitmap != NULL);

#ifdef BCMHWA
	/* Prepare to disable 3b block */
	HWA_TXFIFO_EXPR(hwa_txfifo_disable_prep(hwa_dev, 0));
#endif // endif

	/* halt any tx processing by ucode */
	wlc_bmac_suspend_mac_and_wait(wlc_hw);

#ifdef BCMHWA
	/* Disable 3b block to stop posting tx packets */
	HWA_TXFIFO_EXPR(hwa_txfifo_enable(hwa_dev, 0, FALSE));
#endif // endif

	/* filter the bitmap if DMA tx is not in progress */
	for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
		if (isset(fifo_bitmap, i)) {
#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
			uint physical_fifo = WLC_HW_MAP_TXFIFO(wlc_hw->wlc, i);
			if (hwa_txfifo_dma_active(hwa_dev, 0, physical_fifo)) {
				/* This fifo is going to sync, clear OvflowQ context */
				hwa_txfifo_clear_ovfq(hwa_dev, 0, physical_fifo);
			}
			else
#else
			if (wlc_hw->di[i] == NULL || dma_txactive(wlc_hw->di[i]) == 0)
#endif // endif
			{
				clrbit(fifo_bitmap, i);
			}
		}
	}

	/* clear the hardware fifos */
	wlc_bmac_flush_tx_fifos(wlc_hw, fifo_bitmap);

	/* process any frames that made it out before the suspend */
	wlc_bmac_service_txstatus(wlc_hw);

#if defined(WLAMPDU_UCODE)
	if (AMPDU_UCODE_ENAB(wlc_hw->wlc->pub)) {
		wlc_sidechannel_init(wlc_hw->wlc);
	}
#endif // endif
	/* signal to the upper layer that the fifos are flushed
	 * and any tx packet statuses have been returned
	 */
	wlc_tx_fifo_sync_complete(wlc_hw->wlc, fifo_bitmap, flag);

	/* reenable the fifos once the completion has been signaled */
	wlc_bmac_enable_tx_fifos(wlc_hw, fifo_bitmap);

#ifdef BCMHWA
	/* Enable 3b block to resume posting tx packets */
	HWA_TXFIFO_EXPR(hwa_txfifo_enable(hwa_dev, 0, TRUE));
#endif /* BCMHWA */

	/* allow ucode to run again */
	wlc_bmac_enable_mac(wlc_hw);
} /* wlc_bmac_tx_fifo_sync */

static void
wlc_bmac_service_txstatus(wlc_hw_info_t *wlc_hw)
{
#if defined(BCMHWA) && defined(HWA_TXSTAT_BUILD)
	if (hwa_dev) {
		hwa_txstat_reclaim(hwa_dev, 0);
	}
#else
	bool fatal = FALSE;

	wlc_bmac_txstatus(wlc_hw, FALSE, &fatal);
#endif // endif
}

#define BMC_IN_PROG_CHK_ENAB 0x8000

/**
 * Called during e.g. joining, excursion, channel switch. Workaround for problem in rev 48,49,50 d11
 * ac cores (JIRA CRWLDOT11M-1182/1197). Flushes all tx packets in a caller provided set of hardware
 * fifo's. Ucode does not generate tx statuses for the flushed packets.
 */
static void
wlc_bmac_uflush_tx_fifos(wlc_hw_info_t *wlc_hw, uint fifo_bitmap)
{
	uint i;
	uint chnstatus, status;
	uint count;
	osl_t *osh = wlc_hw->osh;
	uint fbmp;
	dma64regs_t *d64regs;
	bool fastclk;
	uint err_idx = 0;
	const char *err_reason[] = {
		"none", "susp fail", "ucode fail", "final susp fail"
	};

	/* request FAST clock if not on */
	if (!(fastclk = wlc_hw->forcefastclk)) {
		wlc_clkctl_clk(wlc_hw, CLK_FAST);
	}

	/* step 1. request dma suspend fifos */
	/* Do this one DMA at a time, suspending all in a loop causes
	   trouble for dongle drivers...
	*/
	for (i = 0, fbmp = fifo_bitmap; fbmp; i++, fbmp = fbmp >> 1) {
		if ((fbmp & 0x01) == 0)
			continue;

		//no need to update wlc_hw->suspended_fifos cause dma_txsuspend is called hereafter
		dma_txsuspend(wlc_hw->di[i]);
		/* ucode starts flushing now */
		wlc_bmac_write_shm(wlc_hw, M_TXFL_BMAP(wlc_hw), (1 << i));

		d64regs = &(D11Reggrp_f64regs(wlc_hw, i)->dmaxmt);
		count = 0;
		while (count < (80 * 1000)) {
			chnstatus = wlc_bmac_txfifo_suspend_status(wlc_hw, i);
			status = R_REG(osh, &d64regs->status0) & D64_XS0_XS_MASK; /* tX Status */
			if ((wlc_bmac_read_shm(wlc_hw, M_TXFL_BMAP(wlc_hw)) == 0) &&
			    chnstatus == 0 && status == D64_XS0_XS_IDLE)
				break;
#ifdef BCM_BACKPLANE_TIMEOUT
			if ((chnstatus == ID32_INVALID) || (status == ID32_INVALID)) {
				break;
			}
#endif /* BCM_BACKPLANE_TIMEOUT */

			OSL_DELAY(10);
			count += 10;
		}
		if (chnstatus || (status != D64_XS0_XS_IDLE)) {
			err_idx = 1;
			goto mqerr;
		}
	}

	/* step 4. re-wind dma last ptr to the first desc with EOF from current active index  */
	for (i = 0, fbmp = fifo_bitmap; fbmp; i++, fbmp = fbmp >> 1) {
		if ((fbmp & 0x01) == 0)
			continue;
		dma_txrewind(wlc_hw->di[i]);
	}

	/* step 5. un-suspend dma...and do another ucode flush for "partial" frames */
	/* Again, do this one DMA at a time and not all in a loop */
	for (i = 0, fbmp = fifo_bitmap; fbmp; i++, fbmp = fbmp >> 1) {
		if ((fbmp & 0x01) == 0)
			continue;
		dma_txresume(wlc_hw->di[i]);
		//no need to update wlc_hw->suspended_fifos cause dma_txsuspend is called hereafter
		/* (| BMC_IN_PROG_CHK_ENAB) enables BMC in_prog check in ucode */
		wlc_bmac_write_shm(wlc_hw, M_TXFL_BMAP(wlc_hw), ((1 << i) | BMC_IN_PROG_CHK_ENAB));

		count = 0;
		while (count < (80 * 1000)) {
			chnstatus = wlc_bmac_read_shm(wlc_hw, M_TXFL_BMAP(wlc_hw));
			chnstatus &= ~BMC_IN_PROG_CHK_ENAB;
			if (chnstatus == 0)
				break;
			OSL_DELAY(10);
			count += 10;
		}
		if (chnstatus) {
			err_idx = 2;
			goto mqerr;
		}
	}

	/* step 6: have to suspend dma again. otherwise, frame doesn't show up fifordy */
	for (i = 0, fbmp = fifo_bitmap; fbmp; i++, fbmp = fbmp >> 1) {
		if ((fbmp & 0x01) == 0)
			continue;

		wlc_upd_suspended_fifos_set(wlc_hw, i);
		dma_txsuspend(wlc_hw->di[i]);
		d64regs = &(D11Reggrp_f64regs(wlc_hw, i)->dmaxmt);
		count = 0;
		while (count < (80 * 1000)) {
			chnstatus = wlc_bmac_txfifo_suspend_status(wlc_hw, i);
			status = R_REG(osh, &d64regs->status0) & D64_XS0_XS_MASK;
			if (chnstatus == 0 && status == D64_XS0_XS_IDLE)
				break;
#ifdef BCM_BACKPLANE_TIMEOUT
			if ((chnstatus == ID32_INVALID) || (status == ID32_INVALID)) {
				break;
			}
#endif /* BCM_BACKPLANE_TIMEOUT */
			OSL_DELAY(10);
			count += 10;
		}
		if (chnstatus || (status != D64_XS0_XS_IDLE)) {
			err_idx = 3;
			goto mqerr;
		}
	}

	if (!fastclk) {
		wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);
	}

	return;

mqerr:
	d64regs = &(D11Reggrp_f64regs(wlc_hw, i)->dmaxmt);

	if (D11REV_GE(wlc_hw->corerev, 128)) {
		WL_PRINT(("MQ ERROR %s: %s fifo %d xmtsts 0x%04x chnsts 0x%04x bmap 0x%04x"
			"XmtSusp 0x%04x XmtFlush 0x%04x bmcrdsts 0x%04x aqmfifordy 0x%04x "
			"dmabusy 0x%04x\n", __FUNCTION__, err_reason[err_idx], i,
			R_REG(osh, &d64regs->status0),
			R_REG(osh, D11_CHNSTATUS(wlc_hw)),
			wlc_bmac_read_shm(wlc_hw, M_TXFL_BMAP(wlc_hw)),
			R_REG(osh, D11_XmtSusp(wlc_hw)),
			R_REG(osh, D11_XmtFlush(wlc_hw)),
			R_REG(osh, D11_BMCReadStatus(wlc_hw)),
			R_REG(osh, D11_AQMFifoReady(wlc_hw)),
			R_REG(osh, D11_XmtDMABusy(wlc_hw))));
	} else {
		WL_PRINT(("MQ ERROR %s: %s fifo %d xmtsts 0x%04x chnsts 0x%04x bmap 0x%04x"
			"xmtsufl 0x%04x bmcrdsts 0x%04x aqmfifordy 0x%04x dmabusy 0x%04x\n",
			__FUNCTION__, err_reason[err_idx], i, R_REG(osh, &d64regs->status0),
			R_REG(osh, D11_CHNSTATUS(wlc_hw)),
			wlc_bmac_read_shm(wlc_hw, M_TXFL_BMAP(wlc_hw)),
			R_REG(osh, D11_XmtSuspFlush(wlc_hw)),
			R_REG(osh, D11_BMCReadStatus(wlc_hw)),
			R_REG(osh, D11_AQMFifoReady(wlc_hw)),
			R_REG(osh, D11_XmtDMABusy(wlc_hw))));
	}

	if (wlc_hw->need_reinit == WL_REINIT_RC_NONE) {
		wlc_hw->need_reinit = WL_REINIT_RC_MQ_ERROR;
	}
	WLC_FATAL_ERROR(wlc_hw->wlc);
} /* wlc_bmac_uflush_tx_fifos */

// #define WL_MULTIQUEUE_DBG  0

static dma64regs_t *wlc_bmac_get_dma_fifo(wlc_hw_info_t *wlc_hw, uint fifo)
{
	if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
		dma_set_indqsel(wlc_hw->aqm_di[fifo], FALSE);
		return &(D11Reggrp_indaqm(wlc_hw, 0)->dma);
	}

	return &(D11Reggrp_f64regs(wlc_hw, fifo)->dmaxmt);
}

/**
 * Called during e.g. joining, excursion, channel switch. Flushes all tx packets in a caller
 * provided set of hardware fifo's. Does not handle any tx statuses that might result from flushing.
 * For core revs 48,49,50 it calls a workaround which implements a different method of flushing.
 */
static void
wlc_bmac_flush_tx_fifos(wlc_hw_info_t *wlc_hw, void *fifo_bitmap)
{
	uint i;
	uint32 chnstatus;
	uint count;
	osl_t *osh = wlc_hw->osh;
	bool pre64_sfwar = TRUE;
	uint status = 0;
#ifdef WL_MULTIQUEUE_DBG
	volatile uint32 *regaddr;
#endif /* WL_MULTIQUEUE_DBG */
	dma64regs_t *d64regs;
	uint err_idx = 0;
	const char *err_reason[] = {
		"none", "susp fail", "dma busy", "flush fail"
	};

	ASSERT(fifo_bitmap != NULL);

	if ((D11REV_GE(wlc_hw->corerev, 48) && D11REV_LE(wlc_hw->corerev, 49)) ||
		(D11REV_GE(wlc_hw->corerev, 56) && D11REV_LE(wlc_hw->corerev, 61))) {
		wlc_bmac_uflush_tx_fifos(wlc_hw, (*(uint8*)fifo_bitmap) & NFIFO_LEGACY_MASK);
		return;
	}

	/* define variable pre64_sfwar for making sure DMA go into idle state after suspend */
	if (BCM_DMA_CT_ENAB(wlc_hw->wlc))
		pre64_sfwar = FALSE;

	/* WAR 104924:
	 * HW WAR 4360A0/B0, 4350, 43602A0 (d11 core rev ge 40) DMA engine cannot take a
	 * simultaneous suspend and flush request. Software WAR is to set suspend request,
	 * wait for DMA idle indication, then set the flush request
	 * (eg continue w/ regular processing).
	 * XXX: fixed since rev64 (at least we hope)
	 */
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		bool ucode_assist = D11REV_LT(wlc_hw->corerev, 61);
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
			if (!isset(fifo_bitmap, i)) {
				continue;
			}

			WL_MQ(("%s txfifo %d\n", __FUNCTION__, i));

			if (wlc_bmac_tx_fifo_suspended(wlc_hw, i)) {
				WL_INFORM(("%s: suspend request when dma%d suspended, "
					"active=%u suspended=%u\n",
					__FUNCTION__, i,
					dma_txactive(wlc_hw->di[i]),
					isset(wlc_hw->suspended_fifos, i)));
			}

			wlc_upd_suspended_fifos_set(wlc_hw, i);
			dma_txsuspend(wlc_hw->di[i]);

			if (ucode_assist) {
				/* request ucode flush */
				ASSERT(i < 16);
				wlc_bmac_write_shm(wlc_hw, M_TXFL_BMAP(wlc_hw), (uint16)(1 << i));
			}

			/* check chnstatus and ucode flush status */
			count = 0;
			while (count < SUSPEND_FLUSH_TIMEOUT) {
				chnstatus = wlc_bmac_txfifo_suspend_status(wlc_hw, i);
				if (ucode_assist) {
					status = wlc_bmac_read_shm(wlc_hw, M_TXFL_BMAP(wlc_hw));
					if (chnstatus == 0 && status == 0) {
						break;
					}
				} else if (chnstatus == 0) {
					break;
				}
#if defined(BCM_BACKPLANE_TIMEOUT)
				chnstatus = R_REG(osh, D11_CHNSTATUS(wlc_hw));
				if ((chnstatus == ID32_INVALID) || (status == ID32_INVALID)) {
					break;
				}
#endif /* BCM_BACKPLANE_TIMEOUT */
				OSL_DELAY(10);
				count += 10;
			}
			if (chnstatus || status) {
				err_idx = 1;
				goto mqerr; /* suspend request wasn't acked */
			}
		}

		if (pre64_sfwar) {
			uint status_local;
			for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
				if (!isset(fifo_bitmap, i)) {
					continue;
				}

				d64regs = &(D11Reggrp_f64regs(wlc_hw, i)->dmaxmt);

				/* need to make sure dma has become idle (finish any pending tx) */
				count = 0;
				while (count < SUSPEND_FLUSH_TIMEOUT) {
					status_local = R_REG(osh, &d64regs->status0)
									& D64_XS0_XS_MASK;
					if (status_local == D64_XS0_XS_IDLE) {
						break;
					}
					OSL_DELAY(10);
					count += 10;
				}
				if (status_local != D64_XS0_XS_IDLE) {
					err_idx = 2;
					goto mqerr; /* dma still busy */
				}
			}
		}
	}
	/* end WAR 104924 */

	if (!PIO_ENAB_HW(wlc_hw)) {
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
			if (!isset(fifo_bitmap, i)) {
				continue;
			}

#ifdef WL_MULTIQUEUE_DBG
			WL_MQ(("%s: suspend fifo %d\n", __FUNCTION__, i));
#endif // endif

			wlc_upd_suspended_fifos_set(wlc_hw, i);
			dma_txflush(wlc_hw->di[i]);

			/* wait for flush complete */
			count = 0;
			while (count < SUSPEND_FLUSH_TIMEOUT) {
				chnstatus = wlc_bmac_txfifo_flush_status(wlc_hw, i);
				if (chnstatus == 0)
					break;
#if defined(BCM_BACKPLANE_TIMEOUT)
				chnstatus = R_REG(osh, D11_CHNSTATUS(wlc_hw));
				if (chnstatus == ID32_INVALID) {
					chnstatus &= 0xFF00;
					break;
				}
#endif /* BCM_BACKPLANE_TIMEOUT */
				OSL_DELAY(10);
				count += 10;
			}
			if (chnstatus != 0) {
				err_idx = 3;
				goto mqerr; /* flush wasn't acked */
			} else {
				WL_MQ(("MQ: %s: fifo %d waited %d us for success chanstatus 0x%x\n",
				       __FUNCTION__, i, count, chnstatus));
			}
		}

#ifdef WL_MULTIQUEUE_DBG
		/* DBG print */
		if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
			regaddr = D11_CHNFLUSH_STATUS(wlc_hw);
		} else {
			regaddr = D11_CHNSTATUS(wlc_hw);
		}
		chnstatus = R_REG(osh, regaddr);
		WL_MQ(("MQ: %s: post flush req addr 0x%x chnstatus 0x%x\n", __FUNCTION__,
		       *regaddr, chnstatus));

		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
			if (!isset(fifo_bitmap, i)) {
				continue;
			}

			d64regs = wlc_bmac_get_dma_fifo(wlc_hw, i);

			status = ((R_REG(osh, &d64regs->status0) & D64_XS0_XS_MASK) >>
				  D64_XS0_XS_SHIFT);

			WL_MQ(("MQ: %s: post flush req dma %d status %u\n", __FUNCTION__,
			       i, status));
		}
#endif /* WL_MULTIQUEUE_DBG */

		/* Clear the dma flush command */
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
			if (!isset(fifo_bitmap, i)) {
				continue;
			}

			dma_txflush_clear(wlc_hw->di[i]);
		}

#ifdef WL_MULTIQUEUE_DBG
		/* DBG print */
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
			if (!isset(fifo_bitmap, i)) {
				continue;
			}

			d64regs = wlc_bmac_get_dma_fifo(wlc_hw, i);

			status = ((R_REG(osh, &d64regs->status0) & D64_XS0_XS_MASK) >>
			          D64_XS0_XS_SHIFT);

			WL_MQ(("MQ: %s: post flush wait dma %d status %u\n", __FUNCTION__,
			       i, status));
		} /* for */
#endif /* WL_MULTIQUEUE_DBG */
	} else {
		for (i = 0; i < NFIFO_LEGACY; i++) {
			if (isset(fifo_bitmap, i) && wlc_hw->pio[i]) {
				wlc_pio_reset(wlc_hw->pio[i]);
			}
		} /* for */
	} /* else */

	return;

mqerr:
	d64regs = wlc_bmac_get_dma_fifo(wlc_hw, i);
	WL_PRINT(("MQ ERROR %s: %s fifo %d (phys:%d) xmtsts 0x%04x chnsts 0x%04x bmap 0x%04x\n",
		__FUNCTION__, err_reason[err_idx], i, WLC_HW_MAP_TXFIFO(wlc_hw->wlc, i),
		R_REG(osh, &d64regs->status0),
		R_REG(osh, D11_CHNSTATUS(wlc_hw)), wlc_bmac_read_shm(wlc_hw, M_TXFL_BMAP(wlc_hw))));
	if (D11REV_GE(wlc_hw->corerev, 128)) {
		WL_PRINT(("MQ ERROR flshst: 0x%04x 0x%04x 0x%04x, suspst: 0x%04x 0x%04x 0x%04x\n",
			R_REG(osh, D11_CHNFLUSH_STATUS(wlc_hw)),
			R_REG(osh, D11_CHNFLUSH_STATUS1(wlc_hw)),
			R_REG(osh, D11_CHNFLUSH_STATUS2(wlc_hw)),
			R_REG(osh, D11_CHNSUSP_STATUS(wlc_hw)),
			R_REG(osh, D11_CHNSUSP_STATUS1(wlc_hw)),
			R_REG(osh, D11_CHNSUSP_STATUS2(wlc_hw))));
	}
	if (wlc_hw->need_reinit == WL_REINIT_RC_NONE) {
		wlc_hw->need_reinit = WL_REINIT_RC_MQ_ERROR;
	}
	/* Note that using wlc_fatal_error() below causes assert for mac suspend depth	 */
	wlc_dump_ucode_fatal(wlc_hw->wlc, PSM_FATAL_TXSUFL);
} /* wlc_bmac_flush_tx_fifos */

static void
wlc_bmac_enable_tx_fifos(wlc_hw_info_t *wlc_hw, void *fifo_bitmap)
{
	uint i;

	if (!PIO_ENAB_HW(wlc_hw)) {
		for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc_hw->wlc); i++) {
			if (!isset(fifo_bitmap, i) || wlc_hw->di[i] == NULL) {
				continue;
			}

			if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
				dma_txreset(wlc_hw->di[i]);
				dma_txreset(wlc_hw->aqm_di[i]);

				if (D11REV_GE(wlc_hw->corerev, 128)) {
					uint fifo = WLC_HW_MAP_TXFIFO(wlc_hw->wlc, i);
					if (fifo >= 64) {
						ASSERT(fifo < 96);
						AND_REG(wlc_hw->osh, D11_SUSPREQ2(wlc_hw),
						    ~(1 << (fifo - 64)));
					} else if (fifo >= 32) {
						AND_REG(wlc_hw->osh, D11_SUSPREQ1(wlc_hw),
						    ~(1 << (fifo - 32)));
					} else {
						AND_REG(wlc_hw->osh, D11_SUSPREQ(wlc_hw),
						    ~(1 << fifo));
					}
				} else {
					ASSERT(i < 32);
					AND_REG(wlc_hw->osh, D11_SUSPREQ(wlc_hw), ~(1 << i));
				}
			} else {
				dma_txreset(wlc_hw->di[i]);
			}
			wlc_upd_suspended_fifos_clear(wlc_hw, i);
			dma_txinit(wlc_hw->di[i]);

			if (BCM_DMA_CT_ENAB(wlc_hw->wlc)) {
				/* init ct_dma(aqmdma) */
				dma_txinit(wlc_hw->aqm_di[i]);
			}
#ifdef BCMHWA
			HWA_TXFIFO_EXPR(hwa_txfifo_dma_init(hwa_dev, 0,
				WLC_HW_MAP_TXFIFO(wlc_hw->wlc, i)));
#endif // endif
		} /* for */
	} else {
		for (i = 0; i < NFIFO_LEGACY; i++) {
			if (isset(fifo_bitmap, i) && wlc_hw->pio[i]) {
				wlc_pio_reset(wlc_hw->pio[i]);
			}
		} /* for */
	} /* else */
}

/**
 * BMAC portion of wlc_dotxstatus,
 * XXX need to move all DMA/HW dependent preprocessing from high wlc_dotxstatus to here
 */
bool BCMFASTPATH
wlc_bmac_dotxstatus(wlc_hw_info_t *wlc_hw, tx_status_t *txs, uint32 s2)
{
	bool ret;
#ifdef BCMDBG
	if (wlc_hw->wlc->txfifo_detach_pending)
		WL_MQ(("MQ: %s: sync processing of txstatus\n", __FUNCTION__));
#endif /* BCMDBG */

	/* discard intermediate indications for ucode with one legitimate case:
	 *   e.g. if "useRTS" is set. ucode did a successful rts/cts exchange, but the subsequent
	 *   tx of DATA failed. so it will start rts/cts from the beginning (resetting the rts
	 *   transmission count)
	 */
	if (D11REV_LT(wlc_hw->corerev, 40) &&
		!(txs->status.raw_bits & TX_STATUS_AMPDU) &&
		(txs->status.raw_bits & TX_STATUS_INTERMEDIATE)) {
		WL_TRACE(("%s: discard status\n", __FUNCTION__));
		return FALSE;
	}

	ret = wlc_dotxstatus(wlc_hw->wlc, txs, s2);
#ifdef BCMLTECOEX
	if (BCMLTECOEX_ENAB(wlc_hw->wlc->pub)) {
		/* toggle gpio pin if tx-active co-ex enabled and no more pending packets */
		if (wlc_hw->btc->bt_shm_addr && (TXPKTPENDTOT(wlc_hw->wlc) == 0)) {
			int idx;
			wlc_bsscfg_t *bc;
			uint16 ltecx_flags;

			/* only update if the flag was previously set */
			ltecx_flags = wlc_bmac_read_shm(wlc_hw,
					M_LTECX_FLAGS(wlc_hw));
			if ((ltecx_flags & (1 << C_LTECX_FLAGS_TXIND)) == 0)
				goto done;

			/* check all the sw queues to see if they have 2G pending pkts */
			FOREACH_BSS(wlc_hw->wlc, idx, bc) {
				if (bc->wlcif) {
					struct scb *scb;
					struct wlc_txq_info *qi = bc->wlcif->qi;
					while (qi) {
						if (pktq_n_pkts_tot(WLC_GET_CQ(qi))) {
							/* see if the pkt is for 2G band */
							scb = WLPKTTAGSCBGET(
							pktq_peek(WLC_GET_CQ(qi), NULL));
							if (scb && BAND_2G(wlc_hw->band->bandtype))
								goto done;
						}
						qi = qi->next;
					}
				}
			}

			/* no pending packets... clear the flag */
			if (wlc_hw->btc->bt_shm_addr)   {
				ltecx_flags = wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw));
				ltecx_flags &= ~(1 << C_LTECX_FLAGS_TXIND);
				wlc_bmac_write_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw), ltecx_flags);
			}
		}
	}
done:
#endif /* BCMLTECOEX */
	return ret;

}

#ifdef BCMDBG
void wlc_bmac_print_txstatus(wlc_hw_info_t *wlc_hw, tx_status_t* txs);

void wlc_bmac_print_txstatus(wlc_hw_info_t *wlc_hw, tx_status_t* txs)
{

	uint16 s = txs->status.raw_bits;
	uint16 status_bits = txs->status.raw_bits;

	static const char *supr_reason[] = {
		"None", "PMQ Entry", "Flush request",
		"Previous frag failure", "Channel mismatch",
		"Lifetime Expiry", "Underflow", "AB NACK or TX SUPR"
	};

	BCM_REFERENCE(wlc_hw);
	WL_ERROR(("\ntxpkt (MPDU) Complete\n"));

	WL_ERROR(("FrameID: 0x%04x   ", txs->frameid));
	WL_ERROR(("Seq: 0x%04x   ", txs->sequence));
	WL_ERROR(("TxStatus: 0x%04x", s));
	WL_ERROR(("\n"));

	WL_ERROR(("ACK %d IM %d PM %d Suppr %d (%s)",
	       txs->status.was_acked, txs->status.is_intermediate,
	       txs->status.pm_indicated, txs->status.suppr_ind,
	       (txs->status.suppr_ind < ARRAYSIZE(supr_reason) ?
	        supr_reason[txs->status.suppr_ind] : "Unkn supr")));

	WL_ERROR(("PHYTxErr:   0x%04x ", txs->phyerr));
	WL_ERROR(("\n"));

	WL_ERROR(("Raw\n[0]	%d Valid\n", ((status_bits & TX_STATUS_VALID) != 0)));
	WL_ERROR(("[2]    %d IM\n", ((status_bits & TX_STATUS40_INTERMEDIATE) != 0)));
	WL_ERROR(("[3]    %d PM\n", ((status_bits & TX_STATUS40_PMINDCTD) != 0)));
	WL_ERROR(("[7-4]  %d Suppr\n",
		((status_bits & TX_STATUS40_SUPR) >> TX_STATUS40_SUPR_SHIFT)));
	WL_ERROR(("[14:8] %d Ncons\n",
		((status_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT)));
	WL_ERROR(("[15]   %d Acked\n", (status_bits & TX_STATUS40_ACK_RCV) != 0));
}
#endif /* BCMDBG */

#if defined(WL_TXS_LOG)
struct wlc_txs_hist
{
	uint16          len;                /**< length of variable array of ts, txs8/16 structs */
	uint16          idx;                /**< current index into txs array */
#if defined(WLC_UC_TXS_TIMESTAMP)
	uint64         *ts;
#endif // endif
	union {
		wlc_txs_pkg8_t  txs8[1];    /**< array of 8byte txs */
		wlc_txs_pkg16_t txs16[1];   /**< array of 16byte txs */
	} pkg;
};

#if defined(WLC_UC_TXS_TIMESTAMP)
#define WLC_TXS_HIST_LEN(hist_len, pkg_size)  (OFFSETOF(struct wlc_txs_hist, pkg) + \
					       (hist_len) * (pkg_size + sizeof(uint64)))
#else
#define WLC_TXS_HIST_LEN(hist_len, pkg_size)  (OFFSETOF(struct wlc_txs_hist, pkg) + \
					       (hist_len) * (pkg_size))
#endif // endif

#if defined(BCMDBG)
/**
 * Dump the TxStatus history
 */
static int
wlc_txs_hist_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_hw_info_t *wlc_hw = ctx;
	uint i;
	uint len, idx;
	wlc_txs_hist_t *h = (wlc_txs_hist_t*)wlc_hw->txs_hist;
#if defined(WLC_UC_TXS_TIMESTAMP)
	const uint64 usec_per_sec = 1000000ULL;
	uint32 ts_sec, ts_usec;
	uint64 time;
#endif /* WLC_UC_TXS_TIMESTAMP */

	/* the history may not be allocated, early return if so */
	if (h == NULL || (len = h->len) == 0) {
		return BCME_OK;
	}
	idx = h->idx;

	bcm_bprintf(b, "TxStatus History: len %d cur idx %d\n", len, idx);

	for (i = 0; i < len; i++) {
#if defined(WLC_UC_TXS_TIMESTAMP)
		/* break the usecs into seconds and usec */
		time = h->ts[idx];
		ts_sec = (uint32)(time / usec_per_sec);
		ts_usec = (uint32)(time - ((uint64)ts_sec * usec_per_sec));
#endif // endif

		if (D11REV_LT(wlc_hw->corerev, 40)) {
			wlc_txs_pkg8_t *txs8 = &h->pkg.txs8[idx];

#if defined(WLC_UC_TXS_TIMESTAMP)
			bcm_bprintf(b, "%2u: [%u.%06u] %08X %08X\n", idx,
			            ts_sec, ts_usec,
			            txs8->word[0], txs8->word[1]);
#else
			bcm_bprintf(b, "%2u: %08X %08X\n", idx,
			            txs8->word[0], txs8->word[1]);
#endif // endif
		} else {
			wlc_txs_pkg16_t *txs16 = &h->pkg.txs16[idx];

#if defined(WLC_UC_TXS_TIMESTAMP)
			bcm_bprintf(b, "%2u: [%u.%06u] %08X %08X %08X %08X\n", idx,
			            ts_sec, ts_usec,
			            txs16->word[0], txs16->word[1],
			            txs16->word[2], txs16->word[3]);
#else
			bcm_bprintf(b, "%2u: %08X %08X %08X %08X\n", idx,
			            txs16->word[0], txs16->word[1],
			            txs16->word[2], txs16->word[3]);
#endif // endif
		}
		idx++;
		if (idx == len) {
			idx = 0;
		}
	}

	return BCME_OK;
}
#endif // endif

#if defined(WL_DATAPATH_LOG_DUMP)
/**
 * Dump the TxStatus history to the Event Log
 *
 * @param wlc_hw        context pointer to wlc_hw_info_t state structure
 * @param tag           EVENT_LOG tag for output
 */
void
wlc_txs_hist_log_dump(wlc_hw_info_t *wlc_hw, int tag)
{
	wlc_txs_hist_t *h = (wlc_txs_hist_t*)wlc_hw->txs_hist;
	xtlv_uc_txs_t *txs_log;
	osl_t *osh = wlc_hw->osh;
	uint len, idx;
	uint pkg_bytes, pkg_words;
	uint buf_size;
	int8 *hist_data, *start, *bound, *dst;

	/* the history may not be allocated, early return if so */
	if (h == NULL || (len = h->len) == 0) {
		return;
	}

	/* determine txstatus package size based on d11 rev */
	if (D11REV_LT(wlc_hw->corerev, 40)) {
	        pkg_bytes = sizeof(wlc_txs_pkg8_t);
	} else {
		pkg_bytes = sizeof(wlc_txs_pkg16_t);
	}

	pkg_words = pkg_bytes / sizeof(uint32);
	buf_size = XTLV_UCTXSTATUS_FULL_LEN(pkg_words * h->len);

	txs_log = MALLOCZ(osh, buf_size);
	if (txs_log == NULL) {
		EVENT_LOG(tag,
		          "wlc_txs_hist_log_dump(): MALLOC %d failed, malloced %d bytes\n",
		          buf_size, MALLOCED(osh));
		return;
	}

	txs_log->id = EVENT_LOG_XTLV_ID_UCTXSTATUS;
	txs_log->len = buf_size - BCM_XTLV_HDR_SIZE;
	txs_log->entry_size = (uint8)pkg_words;

	idx = h->idx;
	ASSERT(idx < h->len);

	/* get the start pointer, the wrap boundary, and
	 * the beginning of the buffer 'hist_data'
	 * dump oldest to newest by:
	 *    start     to bound
	 *    hist_data to start
	 *
	 * hist_data             start          bound
	 * |                       |              |
	 * V                       V              V
	 * ----------------------------------------
	 * ->    ->      -> newest | oldest ->  ->|
	 */

	/* get a byte pointer to the history txstatus data */
	hist_data = (int8*)&h->pkg;

	/* start is the oldest data, next index to be written */
	start = hist_data + (idx * pkg_bytes);
	bound = hist_data + (h->len * pkg_bytes);

	/* copy to the XTLV data */
	dst = (int8*)txs_log->w;

	/* copy from start to bound */
	memcpy(dst, start, bound - start);

	/* only need the wrap part if 'start' was not at the beginning */
	if (start > hist_data) {
		/* skip over what we already wrote */
		dst += (bound - start);

		/* copy from beginnig of data to start */
		memcpy(dst, hist_data, start - hist_data);
	}

	EVENT_LOG_BUFFER(tag, (uint8*)txs_log, buf_size);

	MFREE(osh, txs_log, buf_size);

#ifdef TXS_LOG_DBG
	{
	int i;

	WL_ERROR(("TxStatus History: len %d cur idx %d pkg %d\n", h->len, idx, pkg_bytes));
	for (i = 0; i < len; i++) {
		if (pkg_bytes == sizeof(wlc_txs_pkg8_t)) {
			wlc_txs_pkg8_t *txs8 = &h->pkg.txs8[idx];
			WL_ERROR(("%2u: %08X %08X\n", idx, txs8->word[0], txs8->word[1]));
		} else {
			wlc_txs_pkg16_t *txs16 = &h->pkg.txs16[idx];
			WL_ERROR(("%2u: %08X %08X %08X %08X\n", idx,
			          txs16->word[0], txs16->word[1],
			          txs16->word[2], txs16->word[3]));
		}
		idx++;
		if (idx == len) {
			idx = 0;
		}
	}
	}
#endif /* TXS_LOG_DBG */

	return;
}
#endif /* WL_DATAPATH_LOG_DUMP */

/**
 * Calculate the size wlc_txs_hist_t struct based on the provided history length
 */
static uint
BCMATTACHFN(wlc_bmac_txs_hist_pkg_size)(wlc_hw_info_t *wlc_hw)
{
	uint pkg_size;

	/* determine txstatus package size based on d11 rev */
	if (D11REV_LT(wlc_hw->corerev, 40)) {
	        pkg_size = sizeof(wlc_txs_pkg8_t);
	} else {
		pkg_size = sizeof(wlc_txs_pkg16_t);
	}

	return pkg_size;
}

/**
 * Allocate a wlc_txs_hist_t struct based on a compile time configurable history length
 */
static wlc_txs_hist_t *
BCMATTACHFN(wlc_bmac_txs_hist_attach)(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh = wlc_hw->osh;
	wlc_txs_hist_t *txs_hist;
	uint16 len;
	uint16 pkg_size;
	uint alloc_size;

	/* compile time configurable history length */
	len = WLC_UC_TXS_HIST_LEN;

	/* determine txstatus package size based on d11 rev */
	pkg_size = (uint16)wlc_bmac_txs_hist_pkg_size(wlc_hw);

	/* total allocation size */
	alloc_size = WLC_TXS_HIST_LEN(len, pkg_size);

	/* alloc txstatus history struct */
	txs_hist = (wlc_txs_hist_t*)MALLOCZ(osh, alloc_size);
	if (txs_hist == NULL) {
		WL_ERROR(("%s: no mem for txs_hist, malloced %d bytes\n", __FUNCTION__,
			MALLOCED(osh)));
		return NULL;
	}
	txs_hist->len = len;

#if defined(WLC_UC_TXS_TIMESTAMP)
	/* the timestamp array starts immediately after the pkg variable array */
	txs_hist->ts = (uint64*)((int8*)&txs_hist->pkg + (pkg_size * len));
#endif // endif

	return txs_hist;
}

/**
 * Free the wlc_txs_hist_t structure from wlc_hw.
 */
static void
BCMATTACHFN(wlc_bmac_txs_hist_detach)(wlc_hw_info_t *wlc_hw)
{
	wlc_txs_hist_t *txs_hist;
	uint16 pkg_size;
	uint alloc_size;

	txs_hist = wlc_hw->txs_hist;
	if (txs_hist == NULL) {
		return;
	}

	/* determine txstatus package size based on d11 rev */
	pkg_size = (uint16)wlc_bmac_txs_hist_pkg_size(wlc_hw);

	/* total allocation size */
	alloc_size = WLC_TXS_HIST_LEN(txs_hist->len, pkg_size);

	MFREE(wlc_hw->osh, txs_hist, alloc_size);
}

void BCMFASTPATH
wlc_bmac_txs_hist_pkg8(wlc_hw_info_t *wlc_hw, wlc_txs_pkg8_t *txs)
{
	wlc_txs_hist_t *txs_hist;
	uint16 hist_len;
	uint16 idx;

	/* add to the txstatus history if active (hist_len > 0) */
	txs_hist = wlc_hw->txs_hist;
	hist_len = txs_hist->len;
	if (hist_len > 0) {
		idx = txs_hist->idx;
		/* copy this package (2 words) to the appropriate array */
		memcpy(&txs_hist->pkg.txs8[idx], txs->word, 8);

#if defined(WLC_UC_TXS_TIMESTAMP)
		txs_hist->ts[idx] = OSL_SYSUPTIME_US64();
#endif // endif

		idx++;
		if (idx < hist_len) {
			txs_hist->idx = idx;
		} else {
			txs_hist->idx = 0;
		}
	}
}

void BCMFASTPATH
wlc_bmac_txs_hist_pkg16(wlc_hw_info_t *wlc_hw, wlc_txs_pkg16_t *txs)
{
	wlc_txs_hist_t *txs_hist;
	uint16 hist_len;
	uint16 idx;

	/* add to the txstatus history if active (hist_len > 0) */
	txs_hist = wlc_hw->txs_hist;
	hist_len = txs_hist->len;
	if (hist_len > 0) {
		idx = txs_hist->idx;
		/* copy this package (4 words) to the appropriate array */
		memcpy(&txs_hist->pkg.txs16[idx], txs->word, 16);

#if defined(WLC_UC_TXS_TIMESTAMP)
		txs_hist->ts[idx] = OSL_SYSUPTIME_US64();
#endif // endif

		idx++;
		if (idx < hist_len) {
			txs_hist->idx = idx;
		} else {
			txs_hist->idx = 0;
		}
	}
}
#endif /* WL_TXS_LOG */

/**
 * Read an 8 byte status package from the TxStatus fifo registers
 * The first word has a valid bit that indicates if the fifo had
 * a valid entry or if the fifo was empty.
 *
 * @return int BCME_OK if the entry was valid, BCME_NOTREADY if
 *         the TxStatus fifo was empty, BCME_NODEVICE if the
 *         read returns 0xFFFFFFFF indicating a target abort
 */
int BCMFASTPATH
wlc_bmac_read_txs_pkg8(wlc_hw_info_t *wlc_hw, wlc_txs_pkg8_t *txs)
{
	osl_t *osh = wlc_hw->osh;
	uint32 s1;

	txs->word[0] = s1 = R_REG(osh, D11_FrmTxStatus(wlc_hw));
	if ((s1 & TXS_V) == 0) {
		return BCME_NOTREADY;
	}
	/* Invalid read indicates a dead chip */
	if (s1 == 0xFFFFFFFF) {
		return BCME_NODEVICE;
	}
	txs->word[1] = R_REG(osh, D11_FrmTxStatus2(wlc_hw));

#if defined(WL_TXS_LOG)
	wlc_bmac_txs_hist_pkg8(wlc_hw, txs);
#endif /* WL_TXS_LOG */

	return BCME_OK;
}

/**
 * Read a 16 byte status package from the TxStatus fifo registers
 * The first word has a valid bit that indicates if the fifo had
 * a valid entry or if the fifo was empty.
 *
 * @return int BCME_OK if the entry was valid, BCME_NOTREADY if
 *         the TxStatus fifo was empty, BCME_NODEVICE if the
 *         read returns 0xFFFFFFFF indicating a target abort
 */
int BCMFASTPATH
wlc_bmac_read_txs_pkg16(wlc_hw_info_t *wlc_hw, wlc_txs_pkg16_t *txs)
{
#ifdef WLC_OFFLOADS_TXSTS
	if (D11REV_GE(wlc_hw->corerev, 130)) {
		if (wlc_offload_get_txstatus(wlc_hw->wlc->offl, txs) == NULL) {
			return BCME_NOTREADY;
		}
	} else
#endif /* WLC_OFFLOADS_TXSTS */
	{
		osl_t *osh = wlc_hw->osh;
		uint32 s1;

		txs->word[0] = s1 = R_REG(osh, D11_FrmTxStatus(wlc_hw));
		if ((s1 & TXS_V) == 0) {
			return BCME_NOTREADY;
		}
		/* Invalid read indicates a dead chip */
		if (s1 == 0xFFFFFFFF) {
			return BCME_NODEVICE;
		}
		txs->word[1] = R_REG(osh, D11_FrmTxStatus2(wlc_hw));
		txs->word[2] = R_REG(osh, D11_FrmTxStatus3(wlc_hw));
		txs->word[3] = R_REG(osh, D11_FrmTxStatus4(wlc_hw));
	}

#if defined(WL_TXS_LOG)
	wlc_bmac_txs_hist_pkg16(wlc_hw, txs);
#endif /* WL_TXS_LOG */

	return BCME_OK;
}

/**
 * process tx completion events in BMAC
 * Return TRUE if more tx status need to be processed. FALSE otherwise.
 */
bool BCMFASTPATH
wlc_bmac_txstatus(wlc_hw_info_t *wlc_hw, bool bound, bool *fatal)
{
	bool morepending = FALSE;
	wlc_info_t *wlc = wlc_hw->wlc;
	int txserr = BCME_OK;

	WL_TRACE(("wl%d: wlc_bmac_txstatus\n", wlc_hw->unit));

#ifdef PKTENG_TXREQ_CACHE
	if (wlc_bmac_pkteng_check_cache(wlc_hw))
		return 0;
#endif /* PKTENG_TXREQ_CACHE */

	if (D11REV_LT(wlc_hw->corerev, 40)) {
		/* corerev >= 5 && < 40 */
		osl_t *osh = wlc_hw->osh;
		tx_status_t txs;
		wlc_txs_pkg8_t pkg;
		uint32 s1, s2;
		uint16 status_bits;
		uint n = 0;
		/* Param 'max_tx_num' indicates max. # tx status to process before break out. */
		uint max_tx_num = bound ? wlc->pub->tunables->txsbnd : -1;
		uint32 tsf_time = 0;

		WL_TRACE(("wl%d: %s: ltrev40\n", wlc_hw->unit, __FUNCTION__));

		/* To avoid overhead time is read only once for the whole while loop
		 * since time accuracy is not a concern for now.
		 */
		tsf_time = R_REG(osh, D11_TSFTimerLow(wlc_hw));
		txs.dequeuetime = 0;

		while (!(*fatal) &&
		       (txserr = wlc_bmac_read_txs_pkg8(wlc_hw, &pkg)) == BCME_OK) {

			s1 = pkg.word[0];
			s2 = pkg.word[1];

			WL_PRHDRS_MSG(("wl%d: %s: Raw txstatus 0x%0X 0x%0X\n",
				wlc_hw->unit, __FUNCTION__, s1, s2));

			status_bits = (s1 & TXS_STATUS_MASK);
			txs.status.raw_bits = status_bits;
			txs.status.was_acked = (status_bits & TX_STATUS_ACK_RCV) != 0;
			txs.status.is_intermediate = (status_bits & TX_STATUS_INTERMEDIATE) != 0;
			txs.status.pm_indicated = (status_bits & TX_STATUS_PMINDCTD) != 0;
			txs.status.suppr_ind =
			        (status_bits & TX_STATUS_SUPR_MASK) >> TX_STATUS_SUPR_SHIFT;
			txs.status.rts_tx_cnt =
			        ((s1 & TX_STATUS_RTS_RTX_MASK) >> TX_STATUS_RTS_RTX_SHIFT);
			txs.status.frag_tx_cnt =
			        ((s1 & TX_STATUS_FRM_RTX_MASK) >> TX_STATUS_FRM_RTX_SHIFT);
			txs.frameid = (s1 & TXS_FID_MASK) >> TXS_FID_SHIFT;
			txs.sequence = s2 & TXS_SEQ_MASK;
			txs.phyerr = (s2 & TXS_PTX_MASK) >> TXS_PTX_SHIFT;
			txs.lasttxtime = tsf_time;
			txs.procflags = 0;

			/* Check if this is an AMPDU BlockAck txstatus.
			 * An AMPDU BA generates a second txstatus pkg which will be
			 * read by wlc_ampdu_dotxstatus().  If this is done successfully,
			 * wlc_ampdu_dotxstatus() will clear the procflags bit
			 * TXS_PROCFLAG_AMPDU_BA_PKG2_READ_REQD.
			 */
			if ((txs.status.raw_bits & TX_STATUS_AMPDU) &&
				(txs.status.raw_bits & TX_STATUS_ACK_RCV)) {
				txs.procflags |= TXS_PROCFLAG_AMPDU_BA_PKG2_READ_REQD;
			}

			*fatal = wlc_bmac_dotxstatus(wlc_hw, &txs, s2);

			/* If this is an AMPDU BA and the txs package 2 has not been read,
			 * read and discard it.
			 */
			if (txs.procflags & TXS_PROCFLAG_AMPDU_BA_PKG2_READ_REQD) {
				(void) wlc_bmac_read_txs_pkg8(wlc_hw, &pkg);
			}
			/* !give others some time to run! */
			if (++n >= max_tx_num)
				break;
		}

		if (txserr == BCME_NODEVICE) {
			WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
			if (wlc_hw->need_reinit == WL_REINIT_RC_NONE) {
				wlc_hw->need_reinit = WL_REINIT_RC_DEVICE_REMOVED;
				WLC_FATAL_ERROR(wlc);
			}
			WL_HEALTH_LOG(wlc_hw->wlc, DEADCHIP_ERROR);
			return morepending;
		}

		if (*fatal)
			return 0;

		if (n >= max_tx_num)
			morepending = TRUE;
	} else {
		/* corerev >= 40 */
		osl_t *osh = wlc_hw->osh;
		tx_status_t txs;
		wlc_txs_pkg16_t pkg;
		/* pkg 1 */
		uint32 v_s1 = 0, v_s2 = 0, v_s3 = 0, v_s4 = 0;
#if defined(WLC_TSYNC)
		/* pkg 3 */
		uint32 v_s9, v_s10, v_s11;
#endif // endif
		uint16 status_bits;
		uint n = 0;
		uint16 ncons;

		/* Param 'max_tx_num' indicates max. # tx status to process before break out. */
		uint max_tx_num = bound ? wlc->pub->tunables->txsbnd : -1;

		WL_TRACE(("wl%d: %s: rev40\n", wlc_hw->unit, __FUNCTION__));

		while (!(*fatal) && (txserr = wlc_bmac_read_txs_pkg16(wlc_hw, &pkg)) == BCME_OK) {

			v_s1 = pkg.word[0];
			v_s2 = pkg.word[1];
			v_s3 = pkg.word[2];
			v_s4 = pkg.word[3];

			WL_TRACE(("%s: s1=%0x ampdu=%d\n", __FUNCTION__, v_s1,
				((v_s1 & 0x4) != 0)));
			txs.frameid = (v_s1 & TXS_FID_MASK) >> TXS_FID_SHIFT;
			txs.sequence = v_s2 & TXS_SEQ_MASK;
			txs.phyerr = (v_s2 & TXS_PTX_MASK) >> TXS_PTX_SHIFT;
			txs.lasttxtime = R_REG(osh, D11_TSFTimerLow(wlc_hw));
			status_bits = v_s1 & TXS_STATUS_MASK;
			txs.status.raw_bits = status_bits;
			txs.status.is_intermediate = (status_bits & TX_STATUS40_INTERMEDIATE) != 0;
			txs.status.pm_indicated = (status_bits & TX_STATUS40_PMINDCTD) != 0;

			ncons = ((status_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT);
			txs.status.was_acked = ((ncons <= 1) ?
				((status_bits & TX_STATUS40_ACK_RCV) != 0) : TRUE);
			txs.status.suppr_ind =
			        (status_bits & TX_STATUS40_SUPR) >> TX_STATUS40_SUPR_SHIFT;

			/* pkg 2 comes always */
			txserr = wlc_bmac_read_txs_pkg16(wlc_hw, &pkg);
			/* store saved extras (check valid pkg) */
			if (txserr != BCME_OK) {
				/* if not a valid package, assert and bail */
				WL_ERROR(("wl%d: %s: package 2 read not valid\n",
				          wlc_hw->unit, __FUNCTION__));
				ASSERT(txserr != BCME_BUSY);
				if (txserr == BCME_NODEVICE) {
					/* dead chip, handle outside while loop */
					break;
				}
				return morepending;
			}

			WL_TRACE(("wl%d: %s calls dotxstatus\n", wlc_hw->unit, __FUNCTION__));

			if (WL_PRHDRS_ON() || WL_MAC_ON()) {
				WL_ERROR(("wl%d: %s:: Raw txstatus %08X %08X %08X %08X "
					"%08X %08X %08X %08X\n",
					wlc_hw->unit, __FUNCTION__,
					v_s1, v_s2, v_s3, v_s4,
					pkg.word[0], pkg.word[1], pkg.word[2], pkg.word[3]));
			}

			txs.status.s1 = v_s1;
			txs.status.s2 = v_s2;
			txs.status.s3 = v_s3;
			txs.status.s4 = v_s4;
			txs.status.s5 = pkg.word[0];
			txs.status.ack_map1 = pkg.word[1];
			txs.status.ack_map2 = pkg.word[2];
			txs.status.s8 = pkg.word[3];

			txs.status.rts_tx_cnt = ((pkg.word[0] & TX_STATUS40_RTS_RTX_MASK) >>
			                         TX_STATUS40_RTS_RTX_SHIFT);
			txs.status.cts_rx_cnt = ((pkg.word[0] & TX_STATUS40_CTS_RRX_MASK) >>
			                         TX_STATUS40_CTS_RRX_SHIFT);

			if ((pkg.word[0] & TX_STATUS64_MUTX)) {
				/* Only RT0 entry is used for frag_tx_cnt in ucode */
				txs.status.frag_tx_cnt = TX_STATUS40_TXCNT_RT0(v_s3);
			} else {
				/* XXX: Need to be recalculated to "txs->status.s3 & 0xffff"
				 * if this tx was fixed rate.
				 * The recalculation is done in wlc_dotxstatus() as we need
				 * TX descriptor from pkt ptr to know if it was fixed rate or not.
				 */
				txs.status.frag_tx_cnt = TX_STATUS40_TXCNT(v_s3, v_s4);
			}

#if defined(WLC_TSYNC)
			if (TSYNC_ENAB(wlc->pub) || PROXD_ENAB_UCODE_TSYNC(wlc->pub)) {
				v_s9 = R_REG(osh, D11_FrmTxStatus(wlc_hw));
				v_s10 = R_REG(osh, D11_FrmTxStatus2(wlc_hw));
				v_s11 = R_REG(osh, D11_FrmTxStatus3(wlc_hw));

				WL_PRHDRS_MSG(("wl%d: %s:: Raw txstatus %08X %08X %08X\n",
					wlc_hw->unit, __FUNCTION__,
					v_s9, v_s10, v_s11));

				if ((v_s9 & TXS_V) == 0) {
					WL_ERROR(("wl%d: %s: package read not valid\n",
						wlc_hw->unit, __FUNCTION__));
					ASSERT(v_s9 != 0xffffffff);
					return morepending;
				}

				txs.status.s9 = v_s9;
				txs.status.s10 = v_s10;
				txs.status.s11 = v_s11;
			}
#endif /* WLC_TSYNC */

			*fatal = wlc_bmac_dotxstatus(wlc_hw, &txs, v_s2);

			/* !give others some time to run! */
#ifdef PROP_TXSTATUS
			/* We must drain out in case of suppress, to avoid Out of Orders */
			if (txs.status.suppr_ind == TX_STATUS_SUPR_NONE)
#endif // endif
				if (++n >= max_tx_num)
					break;
		}

		if (txserr == BCME_NODEVICE) {
			WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
			ASSERT(pkg.word[0] != 0xffffffff);
			WL_HEALTH_LOG(wlc_hw->wlc, DEADCHIP_ERROR);
			return morepending;
		}

		if (*fatal) {
			WL_ERROR(("wl%d: %s:: Raw txstatus %08X %08X %08X %08X "
				"%08X %08X %08X %08X\n",
				wlc_hw->unit, __FUNCTION__,
				v_s1, v_s2, v_s3, v_s4,
				pkg.word[0], pkg.word[1], pkg.word[2], pkg.word[3]));
			WL_ERROR(("error %d caught in %s\n", *fatal, __FUNCTION__));
			return 0;
		}

		if (n >= max_tx_num)
			morepending = TRUE;
	}

	if (wlc->active_queue != NULL && WLC_TXQ_OCCUPIED(wlc)) {
		WLDURATION_ENTER(wlc, DUR_DPC_TXSTATUS_SENDQ);
		wlc_send_q(wlc, wlc->active_queue);
		WLDURATION_EXIT(wlc, DUR_DPC_TXSTATUS_SENDQ);
	}

	return morepending;
} /* wlc_bmac_txstatus */

void
wlc_bmac_write_ihr(wlc_hw_info_t *wlc_hw, uint word_offset, uint16 v)
{
	wlc_bmac_copyto_objmem(wlc_hw, word_offset<<2, &v, sizeof(v), OBJADDR_IHR_SEL);
}

/** @param[in] offset  e.g. S_TXVFREE_BMP */
uint16
wlc_bmac_read_scr(wlc_hw_info_t *wlc_hw, uint offset)
{
	uint16 v;

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), OBJADDR_SCR_SEL | offset);
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	v = (uint16)R_REG(wlc_hw->osh, D11_objdata(wlc_hw));

	return v;
}

void
wlc_bmac_write_scr(wlc_hw_info_t *wlc_hw, uint offset, uint16 v)
{

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), OBJADDR_SCR_SEL | offset);
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	W_REG(wlc_hw->osh, D11_objdata(wlc_hw), v);
}

#ifdef WLRSDB
void
wlc_bmac_update_rxpost_rxbnd(wlc_hw_info_t *wlc_hw, uint8 nrxpost, uint8 rxbnd)
{
	wlc_info_t *wlc = wlc_hw->wlc;

	/* set new nrxpost value */
	wlc_bmac_update_rxpost_rxfill(wlc_hw, RX_FIFO, nrxpost);

	/* set new rxbnd value */
#if defined(PKTC) || defined(PKTC_DONGLE)
	wlc->pub->tunables->pktcbnd = rxbnd;
#else
	wlc->pub->tunables->rxbnd = rxbnd;
#endif // endif
}
#endif /* WLRSDB */

void
wlc_bmac_update_rxfill(wlc_hw_info_t *wlc_hw)
{
	dma_update_rxfill(wlc_hw->di[RX_FIFO]);
}

void
wlc_bmac_update_rxpost_rxfill(wlc_hw_info_t *wlc_hw, uint8 fifo_no, uint8 nrxpost)
{
	/* set new nrxpost value */
	dma_param_set(wlc_hw->di[fifo_no], HNDDMA_NRXPOST, nrxpost);
}

void
wlc_bmac_suspend_mac_and_wait(wlc_hw_info_t *wlc_hw)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	osl_t *osh = wlc_hw->osh;
	uint32 mc, mi;

	WL_TRACE(("wl%d: wlc_bmac_suspend_mac_and_wait: bandunit %d cur depth %d\n", wlc_hw->unit,
		wlc_hw->band->bandunit, wlc_hw->mac_suspend_depth));

	mc = R_REG(osh, D11_MACCONTROL(wlc_hw));
	if (!(mc & MCTL_PSM_RUN)) {
		WL_TRACE(("wl%d: %s: macctrl 0x%x\n", wlc_hw->unit, __FUNCTION__, mc));
		return;
	}

	BCM_REFERENCE(wlc);
	/*
	 * Track overlapping suspend requests
	 */
	wlc_hw->mac_suspend_depth++;
	if (wlc_hw->mac_suspend_depth > 1) {
		if (mc & MCTL_EN_MAC) {
			WL_PRINT(("%s ERROR: suspend_depth %d maccontrol 0x%x\n",
				__FUNCTION__, wlc_hw->mac_suspend_depth, mc));
			wlc_dump_ucode_fatal(wlc_hw->wlc, PSM_FATAL_SUSP);
			ASSERT_PSM_UCODE(!(mc & MCTL_EN_MAC), PSM_FATAL_SUSP, wlc);
		}
		WL_TRACE(("wl%d: %s: bail: mac_suspend_depth=%d\n", wlc_hw->unit,
			__FUNCTION__, wlc_hw->mac_suspend_depth));
		return;
	}

#ifdef STA
	/* force the core awake */
	wlc_ucode_wake_override_set(wlc_hw, WLC_WAKE_OVERRIDE_MACSUSPEND);
#ifdef VSDBWAR
	/* WAR for VSDB issue SWWLAN-126576 -- This is temporary war */
	/* Proper fix is to get a correct radio PU sequence in uCode */
	if (CHSPEC_IS2G(wlc_hw->chanspec)) {
		OSL_DELAY(900);
	}
#endif /* VSDBWAR */
#endif /* STA */
	mc = R_REG(osh, D11_MACCONTROL(wlc_hw));

	if (mc == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}
	ASSERT_PSM_UCODE(!(mc & MCTL_PSM_JMP_0), PSM_FATAL_SUSP, wlc);
	ASSERT_PSM_UCODE(mc & MCTL_EN_MAC, PSM_FATAL_SUSP, wlc);

	mi = R_REG(osh, D11_MACINTSTATUS(wlc_hw));
	if (mi == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}
#ifdef WAR4360_UCODE
	if (mi & MI_MACSSPNDD) {
		WL_ERROR(("wl%d:%s: Hammering due to (mc & MI_MACSPNDD)\n",
			wlc_hw->unit, __FUNCTION__));
		wlc_hw->need_reinit = 4;
		return;
	}
#endif /* WAR4360_UCODE */
	ASSERT_PSM_UCODE(!(mi & MI_MACSSPNDD), PSM_FATAL_SUSP, wlc);

	wlc_bmac_mctrl(wlc_hw, MCTL_EN_MAC, 0);

	SPINWAIT(!(R_REG(osh, D11_MACINTSTATUS(wlc_hw)) & MI_MACSSPNDD), WLC_MAX_MAC_SUSPEND);

	if (!(R_REG(osh, D11_MACINTSTATUS(wlc_hw)) & MI_MACSSPNDD)) {
		WL_PRINT(("wl%d: wlc_bmac_suspend_mac_and_wait: waited %d uS and "
			 "MI_MACSSPNDD is still not on.\n",
			 wlc_hw->unit, WLC_MAX_MAC_SUSPEND));

		if (CHIPID(wlc_hw->sih->chip) != BCM43217_CHIP_ID)
			wlc_dump_ucode_fatal(wlc, PSM_FATAL_SUSP);

		WL_HEALTH_LOG(wlc, MACSPEND_TIMOUT);
		wlc_hw->sw_macintstatus |= MI_GP0;
		return;
	}

	mc = R_REG(osh, D11_MACCONTROL(wlc_hw));
	if (mc == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}
	ASSERT_PSM_UCODE(!(mc & MCTL_PSM_JMP_0), PSM_FATAL_SUSP, wlc);
	ASSERT_PSM_UCODE(!(mc & MCTL_EN_MAC), PSM_FATAL_SUSP, wlc);

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
	{
	    bmac_suspend_stats_t* stats = wlc_hw->suspend_stats;

	    stats->suspend_start = R_REG(osh, D11_TSFTimerLow(wlc_hw));
	    stats->suspend_count++;

	    if (stats->suspend_start > stats->suspend_end) {
			uint32 unsuspend_time = (stats->suspend_start - stats->suspend_end)/100;
			stats->unsuspended += unsuspend_time;
			WL_TRACE(("wl%d: bmac now suspended; time spent active was %d ms\n",
			           wlc_hw->unit, (unsuspend_time + 5)/10));
	    }
	}
#endif // endif
} /* wlc_bmac_suspend_mac_and_wait */

void
wlc_bmac_enable_mac(wlc_hw_info_t *wlc_hw)
{
	uint32 mc, mi, dbgst;
	osl_t *osh;

	WL_TRACE(("wl%d: wlc_bmac_enable_mac: bandunit %d\n",
		wlc_hw->unit, wlc_hw->band->bandunit));
#ifdef WAR4360_UCODE
	if (wlc_hw->need_reinit) {
		return;
	}
#endif // endif
	osh = wlc_hw->osh;

	mc = R_REG(osh, D11_MACCONTROL(wlc_hw));
	if (!(mc & MCTL_PSM_RUN)) {
		WL_TRACE(("wl%d: %s: macctrl 0x%x\n", wlc_hw->unit, __FUNCTION__, mc));
		return;
	}

	/*
	 * Track overlapping suspend requests
	 */
	ASSERT(wlc_hw->mac_suspend_depth > 0);
	/* If wlc_fatal_error is called after wlc_bmac_suspend_mac_and_wait()
	 * and before wlc_bmac_enable_mac(), mac_suspend_depth will not be properly
	 * maintained
	 */
	if ((mc & MCTL_EN_MAC) && (wlc_hw->mac_suspend_depth == 0)) {
		WL_ERROR(("wl%d, %s: macctrl 0x%x, bmac may be enabled by wlc_fatal_error",
			wlc_hw->unit, __FUNCTION__, mc));
		return;
	}
	wlc_hw->mac_suspend_depth--;
	if (wlc_hw->mac_suspend_depth > 0) {
		return;
	}

	ASSERT(!(mc & MCTL_PSM_JMP_0));
	ASSERT(!(mc & MCTL_EN_MAC));

	/* FIXME: The following should be a valid assert, except that we bail out
	 * of the spin loop in wlc_bmac_suspend_mac_and_wait.
	 *
	 * mi = R_REG(osh, D11_MACINTSTATUS(wlc_hw));
	 * ASSERT(mi & MI_MACSSPNDD);
	 */

	wlc_bmac_mctrl(wlc_hw, MCTL_EN_MAC, MCTL_EN_MAC);

	W_REG(osh, D11_MACINTSTATUS(wlc_hw), MI_MACSSPNDD);

	mc = R_REG(osh, D11_MACCONTROL(wlc_hw));
	ASSERT(!(mc & MCTL_PSM_JMP_0));
	ASSERT(mc & MCTL_EN_MAC);
	BCM_REFERENCE(mc);

	mi = R_REG(osh, D11_MACINTSTATUS(wlc_hw));
	ASSERT(!(mi & MI_MACSSPNDD));
	BCM_REFERENCE(mi);

	SPINWAIT(((dbgst = wlc_bmac_read_shm(wlc_hw, M_UCODE_DBGST(wlc_hw))) == DBGST_SUSPENDED),
			WLC_MAX_MAC_ENABLE);
	if (dbgst != DBGST_ACTIVE && !(wlc_hw->sw_macintstatus & MI_GP0)) {
		if (wlc_bmac_report_fatal_errors(wlc_hw, WL_REINIT_RC_MAC_ENABLE)) {
			return;
		}
	}

#ifdef STA
	wlc_ucode_wake_override_clear(wlc_hw, WLC_WAKE_OVERRIDE_MACSUSPEND);
#endif /* STA */

#ifdef MBSS
	if (MBSS_SUPPORT(wlc_hw->wlc->pub)) {
		wlc_mbss_reset_prq(wlc_hw->wlc);
	}
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
	{
	bmac_suspend_stats_t* stats = wlc_hw->suspend_stats;

	stats->suspend_end = R_REG(osh, D11_TSFTimerLow(wlc_hw));

	if (stats->suspend_end > stats->suspend_start) {
		uint32 suspend_time = (stats->suspend_end - stats->suspend_start)/100;

		if (suspend_time > stats->suspend_max) {
			stats->suspend_max = suspend_time;
		}
		stats->suspended += suspend_time;
		WL_TRACE(("wl%d: bmac now active; time spent suspended was %d ms\n",
		          wlc_hw->unit, (suspend_time + 5)/10));
	}
	}
#endif // endif
} /* wlc_bmac_enable_mac */

#if defined(WL_PSMX)
void
wlc_bmac_suspend_macx_and_wait(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh = wlc_hw->osh;
	uint32 mcx, mix;
	wlc_info_t *wlc = wlc_hw->wlc;

	WL_TRACE(("wl%d: %s: bandunit %d\n", wlc_hw->unit, __FUNCTION__,
		wlc_hw->band->bandunit));

	mcx = R_REG(osh, D11_MACCONTROL_psmx(wlc_hw));
	if (!(mcx & MCTL_PSM_RUN)) {
		WL_MAC(("wl%d: %s: macxctrl 0x%x\n", wlc_hw->unit, __FUNCTION__, mcx));
		return;
	}

	/*
	 * Track overlapping suspend requests
	 */
	wlc_hw->macx_suspend_depth++;
	if (wlc_hw->macx_suspend_depth > 1) {
		mcx = R_REG(osh, D11_MACCONTROL_psmx(wlc_hw));
		if (mcx & MCTL_EN_MAC) {
			WL_PRINT(("%s ERROR: suspend_depth %d maccontrol_x 0x%x\n",
				__FUNCTION__, wlc_hw->macx_suspend_depth, mcx));

			wlc_dump_psmx_fatal(wlc, PSMX_FATAL_SUSP);

			ASSERT_PSMX_UCODE(!(mcx & MCTL_EN_MAC), PSMX_FATAL_SUSP, wlc);
		}
		WL_TRACE(("wl%d: %s: bail: macx_suspend_depth=%d\n", wlc_hw->unit,
			__FUNCTION__, wlc_hw->macx_suspend_depth));
		return;
	}

	mcx = R_REG(osh, D11_MACCONTROL_psmx(wlc_hw));

	if (mcx == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}

	ASSERT_PSMX_UCODE(!(mcx & MCTL_PSM_JMP_0), PSMX_FATAL_SUSP, wlc);
	ASSERT_PSMX_UCODE(mcx & MCTL_PSM_RUN, PSMX_FATAL_SUSP, wlc);
	ASSERT_PSMX_UCODE(mcx & MCTL_EN_MAC, PSMX_FATAL_SUSP, wlc);

	mix = R_REG(osh, D11_MACINTSTATUS_psmx(wlc_hw));
	if (mix == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}

	ASSERT_PSMX_UCODE(!(mix & MI_MACSSPNDD), PSMX_FATAL_SUSP, wlc);

	wlc_bmac_mctrlx(wlc_hw, MCTL_EN_MAC, 0);

	SPINWAIT(!(R_REG(osh, D11_MACINTSTATUS_psmx(wlc_hw)) & MI_MACSSPNDD),
		WLC_MAX_MAC_SUSPEND);

	if (!(R_REG(osh, D11_MACINTSTATUS_psmx(wlc_hw)) & MI_MACSSPNDD)) {
		WL_PRINT(("wl%d: %s: waited %d uS and "
			 "MI_MACSSPNDD is still not on.\n",
			 wlc_hw->unit, __FUNCTION__, WLC_MAX_MAC_SUSPEND));

		wlc_dump_psmx_fatal(wlc, PSMX_FATAL_SUSP);

		WL_HEALTH_LOG(wlc, MACSPEND_TIMOUT);
		wlc_hw->sw_macintstatus |= MI_GP0;
	}

	mcx = R_REG(osh, D11_MACCONTROL_psmx(wlc_hw));
	if (mcx == 0xffffffff) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		wl_down(wlc->wl);
		return;
	}

	ASSERT_PSMX_UCODE(!(mcx & MCTL_PSM_JMP_0), PSMX_FATAL_SUSP, wlc);
	ASSERT_PSMX_UCODE(mcx & MCTL_PSM_RUN, PSMX_FATAL_SUSP, wlc);
	ASSERT_PSMX_UCODE(!(mcx & MCTL_EN_MAC), PSMX_FATAL_SUSP, wlc);
} /* wlc_bmac_suspend_macx_and_wait */

void
wlc_bmac_enable_macx(wlc_hw_info_t *wlc_hw)
{
	uint32 mcx, mix;
	osl_t *osh;

	ASSERT(PSMX_ENAB(wlc_hw->wlc->pub));
	WL_TRACE(("wl%d: %s: bandunit %d\n",
		wlc_hw->unit, __FUNCTION__, wlc_hw->band->bandunit));

	osh = wlc_hw->osh;
	mcx = R_REG(osh, D11_MACCONTROL_psmx(wlc_hw));
	if (!(mcx & MCTL_PSM_RUN)) {
		WL_MAC(("wl%d: %s: macxctrl 0x%x\n", wlc_hw->unit, __FUNCTION__, mcx));
		return;
	}

	/*
	 * Track overlapping suspend requests
	 */
	ASSERT(wlc_hw->macx_suspend_depth > 0);
	wlc_hw->macx_suspend_depth--;
	if (wlc_hw->macx_suspend_depth > 0) {
		return;
	}

	mcx = R_REG(osh, D11_MACCONTROL_psmx(wlc_hw));
	ASSERT(!(mcx & MCTL_PSM_JMP_0));
	ASSERT(!(mcx & MCTL_EN_MAC));

	wlc_bmac_mctrlx(wlc_hw, MCTL_EN_MAC, MCTL_EN_MAC);

	W_REG(osh, D11_MACINTSTATUS_psmx(wlc_hw), MI_MACSSPNDD);

	mcx = R_REG(osh, D11_MACCONTROL_psmx(wlc_hw));

	ASSERT(!(mcx & MCTL_PSM_JMP_0));
	ASSERT(mcx & MCTL_EN_MAC);
	ASSERT(mcx & MCTL_PSM_RUN);

	BCM_REFERENCE(mcx);

	mix = R_REG(osh, D11_MACINTSTATUS_psmx(wlc_hw));

	ASSERT(!(mix & MI_MACSSPNDD));
	BCM_REFERENCE(mix);
}
#endif /* WL_PSMX */

void
wlc_bmac_sync_macstate(wlc_hw_info_t *wlc_hw)
{
	bool wake_override = ((wlc_hw->wake_override & WLC_WAKE_OVERRIDE_MACSUSPEND) != 0);
	if (wake_override && wlc_hw->mac_suspend_depth == 1)
		wlc_bmac_enable_mac(wlc_hw);
}

void
wlc_bmac_ifsctl_vht_set(wlc_hw_info_t *wlc_hw, int ed_sel)
{
	uint16 mask, val;
	uint16 val_mask1, val_mask2;
	bool sb_ctrl, enable, err = FALSE;
	volatile uint16 *ifsctl_reg;
	osl_t *osh;
	uint16 chanspec;
	bool suspend = FALSE;

	ASSERT(D11REV_GE(wlc_hw->corerev, 40));
	if (!WLCISACPHY(wlc_hw->band))
		return;

	osh = wlc_hw->osh;
	ifsctl_reg = (volatile uint16 *)D11_IFS_CTL_SEL_PRICRS(wlc_hw);
	mask = IFS_CTL_CRS_SEL_MASK|IFS_CTL_ED_SEL_MASK;

	if (ed_sel == AUTO) {
		val = (uint16)wlc_bmac_read_shm(wlc_hw, M_IFS_PRICRS(wlc_hw));
		enable = (val & IFS_CTL_ED_SEL_MASK) ? TRUE:FALSE;
	} else {
		enable = (ed_sel == ON) ? TRUE : FALSE;
	}

	val_mask1 = enable ? 0xffff : 0x00ff;
	val_mask2 = 0xf;

	chanspec = wlc_hw->chanspec;

	switch (CHSPEC_BW(chanspec)) {
	case WL_CHANSPEC_BW_20:
		val = mask & val_mask1;
		break;

	case WL_CHANSPEC_BW_40:
		/* Secondary first */
		sb_ctrl = (chanspec & WL_CHANSPEC_CTL_SB_MASK) ==  WL_CHANSPEC_CTL_SB_L;
		val = (sb_ctrl ? 0x0202 : 0x0101) & val_mask2;

		/* Primary */
		val = ((wlc_hw->band->mhfs[MHF1] & MHF1_D11AC_DYNBW) ?
			(val ^ 0x303) : 0x303) & val_mask1;

		break;

	case WL_CHANSPEC_BW_80:
		/* Secondary first */
		sb_ctrl =
			(chanspec & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_LL ||
			(chanspec & WL_CHANSPEC_CTL_SB_MASK) == WL_CHANSPEC_CTL_SB_LU;
		val = (sb_ctrl ? 0x0c0c : 0x0303) & val_mask2;
		/* Primary */
		val = ((wlc_hw->band->mhfs[MHF1] & MHF1_D11AC_DYNBW) ?
			(val ^ 0xf0f) : 0xf0f) & val_mask1;

		break;

	case WL_CHANSPEC_BW_160:
	case WL_CHANSPEC_BW_8080:
		val = 0xffff & val_mask1; /* enable all 8 sub-bands */
		if (wlc_hw->band->mhfs[MHF1] & MHF1_D11AC_DYNBW) {
			WL_ERROR(("%s: cannot enable dynbw fow bw160 for now!\n",
				__FUNCTION__));
		}
		break;

	default:
		err = TRUE;
		WL_ERROR(("Unsupported bandwidth - chanspec: 0x%04x\n",
			wlc_hw->chanspec));
		ASSERT(!"Invalid bandwidth in chanspec");
	}

	if (!err) {
		if (D11REV_GE(wlc_hw->corerev, 40))
			wlc_bmac_write_shm(wlc_hw, M_IFS_PRICRS(wlc_hw), val);
		else
			wlc_bmac_write_shm(wlc_hw, M_IFSCTL1(wlc_hw), val);
		W_REG(osh, ifsctl_reg, val);

		wlc_phy_conditional_suspend((phy_info_t *)wlc_hw->band->pi, &suspend);

		/* update phyreg NsyncscramInit1:scramb_dyn_bw_en */
		wlc_acphy_set_scramb_dyn_bw_en(wlc_hw->band->pi, enable);

		wlc_phy_conditional_resume((phy_info_t *)wlc_hw->band->pi, &suspend);
	}
} /* wlc_bmac_ifsctl_vht_set */

void
wlc_bmac_ifsctl_edcrs_set(wlc_hw_info_t *wlc_hw, bool isht)
{
	if (!(WLCISNPHY(wlc_hw->band)) && !WLCISACPHY(wlc_hw->band))
		return;

	if (!isht) {
		/* enable EDCRS for non-11n association */
		wlc_bmac_ifsctl1_regshm(wlc_hw, IFS_CTL1_EDCRS, IFS_CTL1_EDCRS);
	}
	if (WLCISNPHY(wlc_hw->band)) {
		if (CHSPEC_IS20(wlc_hw->chanspec)) {
			/* 20 mhz, use 20U ED only */
			wlc_bmac_ifsctl1_regshm(wlc_hw,
				(IFS_CTL1_EDCRS | IFS_CTL1_EDCRS_20L | IFS_CTL1_EDCRS_40),
				IFS_CTL1_EDCRS);
		} else {
			/* 40 mhz, use 20U 20L and 40 ED */
			wlc_bmac_ifsctl1_regshm(wlc_hw,
				(IFS_CTL1_EDCRS | IFS_CTL1_EDCRS_20L | IFS_CTL1_EDCRS_40),
				(IFS_CTL1_EDCRS | IFS_CTL1_EDCRS_20L | IFS_CTL1_EDCRS_40));
		}
	} else if (WLCISACPHY(wlc_hw->band)) {
		wlc_bmac_ifsctl_vht_set(wlc_hw, ON);
	}
} /* wlc_bmac_ifsctl_edcrs_set */

#ifdef WL11N
/**
 * update the txphyctl in shm table for BA/ACK/CTS
 * XXX: This does not update the single stream mimo rates as yet. OK for now
 *      since ucode never sends any response at those rates
 */
static void
wlc_upd_ofdm_pctl1_table(wlc_hw_info_t *wlc_hw)
{
	uint8 rate;
	const uint8 rates[8] = {
		WLC_RATE_6M, WLC_RATE_9M, WLC_RATE_12M, WLC_RATE_18M,
		WLC_RATE_24M, WLC_RATE_36M, WLC_RATE_48M, WLC_RATE_54M
	};

	uint16 entry_ptr;
	uint16 pctl1, phyctl;
	uint i;

	if (!WLC_PHY_11N_CAP(wlc_hw->band))
		return;

	/* walk the phy rate table and update the entries */
	for (i = 0; i < ARRAYSIZE(rates); i++) {
		rate = rates[i];

		entry_ptr = wlc_bmac_ofdm_ratetable_offset(wlc_hw, rate);

		/* read the SHM Rate Table entry OFDM PCTL1 values */
		pctl1 = wlc_bmac_read_shm(wlc_hw, entry_ptr + M_RT_OFDM_PCTL1_POS(wlc_hw));

		/* modify the STF value */
		if (WLCISNPHY(wlc_hw->band)) {
			pctl1 &= ~PHY_TXC1_MODE_MASK;
			if (wlc_bmac_btc_mode_get(wlc_hw))
				pctl1 |= (PHY_TXC1_MODE_SISO << PHY_TXC1_MODE_SHIFT);
			else
				pctl1 |= (wlc_hw->hw_stf_ss_opmode << PHY_TXC1_MODE_SHIFT);
		}

		/* Update the SHM Rate Table entry OFDM PCTL1 values */
		wlc_bmac_write_shm(wlc_hw, entry_ptr + M_RT_OFDM_PCTL1_POS(wlc_hw), pctl1);
	}

	/* only works for nphy */
	if (D11REV_LT(wlc_hw->corerev, 40) && wlc_bmac_btc_mode_get(wlc_hw))
	{
		uint16 ant_ctl = ((wlc_hw->boardflags2 & BFL2_BT_SHARE_ANT0) == BFL2_BT_SHARE_ANT0)
			? PHY_TXC_ANT_1 : PHY_TXC_ANT_0;
		/* set the Response (ACK/CTS) frame phy control word */
		phyctl = wlc_bmac_read_shm(wlc_hw, M_RSP_PCTLWD(wlc_hw));
		phyctl = (phyctl & ~PHY_TXC_ANT_MASK) | ant_ctl;
		wlc_bmac_write_shm(wlc_hw, M_RSP_PCTLWD(wlc_hw), phyctl);
	}
} /* wlc_upd_ofdm_pctl1_table */

static uint16
wlc_bmac_ofdm_ratetable_offset(wlc_hw_info_t *wlc_hw, uint8 rate)
{
	uint i;
	uint8 plcp_rate = 0;
	struct plcp_signal_rate_lookup {
		uint8 rate;
		uint8 signal_rate;
	};
	/* OFDM RATE sub-field of PLCP SIGNAL field, per 802.11 sec 17.3.4.1 */
	const struct plcp_signal_rate_lookup rate_lookup[] = {
		{WLC_RATE_6M,  0xB},
		{WLC_RATE_9M,  0xF},
		{WLC_RATE_12M, 0xA},
		{WLC_RATE_18M, 0xE},
		{WLC_RATE_24M, 0x9},
		{WLC_RATE_36M, 0xD},
		{WLC_RATE_48M, 0x8},
		{WLC_RATE_54M, 0xC}
	};

	for (i = 0; i < ARRAYSIZE(rate_lookup); i++) {
		if (rate == rate_lookup[i].rate) {
			plcp_rate = rate_lookup[i].signal_rate;
			break;
		}
	}

	/* Find the SHM pointer to the rate table entry by looking in the
	 * Direct-map Table
	 */
	return (2*wlc_bmac_read_shm(wlc_hw, M_RT_DIRMAP_A(wlc_hw) + (plcp_rate * 2)));
}

void
wlc_bmac_band_stf_ss_set(wlc_hw_info_t *wlc_hw, uint8 stf_mode)
{
	wlc_hw->hw_stf_ss_opmode = stf_mode;

	if (wlc_hw->clk)
		wlc_upd_ofdm_pctl1_table(wlc_hw);
}

void
wlc_bmac_txbw_update(wlc_hw_info_t *wlc_hw)
{
	if (wlc_hw->clk)
		wlc_upd_ofdm_pctl1_table(wlc_hw);

}
#endif /* WL11N */

void BCMFASTPATH
wlc_bmac_read_tsf(wlc_hw_info_t* wlc_hw, uint32* tsf_l_ptr, uint32* tsf_h_ptr)
{
	uint32 tsf_l;

	/* read the tsf timer low, then high to get an atomic read */
	tsf_l = R_REG(wlc_hw->osh, D11_TSFTimerLow(wlc_hw));

	if (tsf_l_ptr)
		*tsf_l_ptr = tsf_l;

	if (tsf_h_ptr)
		*tsf_h_ptr = R_REG(wlc_hw->osh, D11_TSFTimerHigh(wlc_hw));

	return;
}

#ifdef WLC_TSYNC
void
wlc_bmac_start_tsync(wlc_hw_info_t* wlc_hw)
{
	OR_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw), MCMD_TSYNC);
}
#endif // endif

uint32
wlc_bmac_read_usec_timer(wlc_hw_info_t* wlc_hw)
{

	/* use usec timer for revisions 40 onwards */
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		return R_REG(wlc_hw->osh, D11_usectimer(wlc_hw));
	}

	return R_REG(wlc_hw->osh, D11_TSFTimerLow(wlc_hw));
}

bool
#ifdef WLDIAG
wlc_bmac_validate_chip_access(wlc_hw_info_t *wlc_hw)
#else
BCMATTACHFN(wlc_bmac_validate_chip_access)(wlc_hw_info_t *wlc_hw)
#endif // endif
{
	uint32 w, valw, valr;
	osl_t *osh = wlc_hw->osh;

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	/* Validate dchip register access */
	wlc_bmac_copyfrom_shm(wlc_hw, 0, &w, sizeof(w));

	/* Can we write and read back a 32bit register? */
	valw = 0xaa5555aa;
	wlc_bmac_copyto_shm(wlc_hw, 0, &valw, sizeof(valw));

	wlc_bmac_copyfrom_shm(wlc_hw, 0, &valr, sizeof(valr));
	if (valr != valw) {
		WL_ERROR(("wl%d: %s: SHM = 0x%x, expected 0x%x\n",
			wlc_hw->unit, __FUNCTION__, valr, valw));
		return (FALSE);
	}

	valw = 0x55aaaa55;
	wlc_bmac_copyto_shm(wlc_hw, 0, &valw, sizeof(valw));

	wlc_bmac_copyfrom_shm(wlc_hw, 0, &valr, sizeof(valr));
	if (valr != valw) {
		WL_ERROR(("wl%d: %s: SHM = 0x%x, expected 0x%x\n",
			wlc_hw->unit, __FUNCTION__, valr, valw));
		return (FALSE);
	}

	wlc_bmac_copyto_shm(wlc_hw, 0, &w, sizeof(w));

	/* clear CFPStart */
	W_REG(osh, D11_CFPStart(wlc_hw), 0);

	w = R_REG(osh, D11_MACCONTROL(wlc_hw));
	if ((w != (MCTL_IHR_EN | MCTL_WAKE)) &&
	    (w != (MCTL_IHR_EN | MCTL_GMODE | MCTL_WAKE))) {
		WL_ERROR(("wl%d: %s: maccontrol = 0x%x, expected 0x%x or 0x%x\n",
		          wlc_hw->unit, __FUNCTION__, w, (MCTL_IHR_EN | MCTL_WAKE),
		          (MCTL_IHR_EN | MCTL_GMODE | MCTL_WAKE)));
		return (FALSE);
	}
	return (TRUE);
} /* wlc_bmac_validate_chip_access */

#define PHYPLL_WAIT_US	100000

void
wlc_bmac_core_phypll_ctl(wlc_hw_info_t* wlc_hw, bool on)
{
	osl_t *osh;
	uint32 req_bits, avail_bits, tmp;

	WL_TRACE(("wl%d: wlc_bmac_core_phypll_ctl\n", wlc_hw->unit));

	osh = wlc_hw->osh;
	if (!wlc_hw->clk ||
		wlc_hw->wlc->pub->hw_off) {
		return;
	}

	/* Do not access registers if core is not up */
	if (wlc_bmac_si_iscoreup(wlc_hw) == FALSE)
		return;

	if (on) {
		if (D11REV_IS(wlc_hw->corerev, 30)) {
			req_bits = PSM_CORE_CTL_PPAR;
			avail_bits = PSM_CORE_CTL_PPAS;

			OR_REG(osh, D11_PSMCoreCtlStat(wlc_hw), req_bits);
			SPINWAIT((R_REG(osh, D11_PSMCoreCtlStat(wlc_hw)) & avail_bits) !=
				avail_bits, PHYPLL_WAIT_US);

			tmp = R_REG(osh, D11_PSMCoreCtlStat(wlc_hw));
		} else {
			req_bits = CCS_ERSRC_REQ_D11PLL | CCS_ERSRC_REQ_PHYPLL;
			avail_bits = CCS_ERSRC_AVAIL_D11PLL | CCS_ERSRC_AVAIL_PHYPLL;

			/* MIMO mode FORCEHWREQOFF is done on core-1. Hence it
			 * requires to be cleared when switching happens from
			 * MIMO to RSDB
			 */
			if (wlc_bmac_rsdb_cap(wlc_hw)) {
				AND_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw),
					~CCS_FORCEHWREQOFF);
			}

			tmp = R_REG(osh, D11_ClockCtlStatus(wlc_hw));
			/* if the req_bits alread set, then bail out */
			if ((tmp & req_bits) != req_bits) {
				OR_REG(osh, D11_ClockCtlStatus(wlc_hw), req_bits);
				/* avail_bit can be set prior to the write of req_bits */
				if ((tmp & avail_bits) == avail_bits) {
					/* break down 64usec delay to 4*16 delay so that
					 * OSL_DELAY will not yield to other thread
					 */
					int i;
					for (i = 0; i < 4; i++) {
						OSL_DELAY(16);
					}
				}
				SPINWAIT((R_REG(osh, D11_ClockCtlStatus(wlc_hw)) & avail_bits) !=
					avail_bits, PHYPLL_WAIT_US);
				tmp = R_REG(osh, D11_ClockCtlStatus(wlc_hw));
			}
		}

		if ((tmp & avail_bits) != avail_bits) {
			WL_ERROR(("%s: turn on PHY PLL failed\n", __FUNCTION__));
			WL_HEALTH_LOG(wlc_hw->wlc, PHY_PLL_ERROR);
			ASSERT(0);
		}
	} else {
		/* Since the PLL may be shared, other cores can still be requesting it;
		 * so we'll deassert the request but not wait for status to comply.
		 */
		if (D11REV_IS(wlc_hw->corerev, 30)) {
			req_bits = PSM_CORE_CTL_PPAR;

			AND_REG(osh, D11_PSMCoreCtlStat(wlc_hw), ~req_bits);
			tmp = R_REG(osh, D11_PSMCoreCtlStat(wlc_hw));
		} else {
			req_bits = CCS_ERSRC_REQ_D11PLL | CCS_ERSRC_REQ_PHYPLL;

			AND_REG(osh, D11_ClockCtlStatus(wlc_hw), ~req_bits);
			tmp = R_REG(osh, D11_ClockCtlStatus(wlc_hw));
		}
	}

	wlc_bmac_4349_core1_hwreqoff(wlc_hw, (on == 0)? TRUE:FALSE);

	WL_TRACE(("%s: clk_ctl_st after phypll(%d) request 0x%x\n",
		__FUNCTION__, on, tmp));
} /* wlc_bmac_core_phypll_ctl */

void
wlc_coredisable(wlc_hw_info_t* wlc_hw)
{
	bool dev_gone;

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	dev_gone = DEVICEREMOVED(wlc_hw->wlc);

	if (dev_gone)
		return;

	if (wlc_hw->noreset)
		return;

	/* Don't assert until we know we're going to execute.
	 * We may return for other reasons.
	 */
	ASSERT(!wlc_hw->up);

	/* radio off */
	phy_radio_switch((phy_info_t *)wlc_hw->band->pi, OFF);

	/* turn off analog core */
	phy_ana_switch((phy_info_t *)wlc_hw->band->pi, OFF);

#ifdef WLRSDB
	/* While MIMO association is on core-0, clear ForceHT on other core to
	 * enter PM mode
	 */
	if (wlc_bmac_rsdb_cap(wlc_hw) && (si_coreunit(wlc_hw->sih) !=  MAC_CORE_UNIT_0) &&
		!(wlc_rsdb_is_other_chain_idle(wlc_hw->wlc) == TRUE)) {
			wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);
	}
#endif // endif

	/* turn off PHYPLL to save power */
	wlc_bmac_core_phypll_ctl(wlc_hw, FALSE);

	/* No need to set wlc->pub->radio_active = OFF
	 * because this function needs down capability and
	 * radio_active is designed for BCMNODOWN.
	 */

	/* remove gpio controls */
	if (wlc_hw->ucode_dbgsel)
		si_gpiocontrol(wlc_hw->sih, ~0, 0, GPIO_DRV_PRIORITY);

	wlc_hw->clk = FALSE;
	wlc_bmac_core_disable(wlc_hw, 0);
	phy_hw_clk_state_upd(wlc_hw->band->pi, FALSE);
} /* wlc_coredisable */

/** power both the pll and external oscillator on/off */
void
wlc_bmac_xtal(wlc_hw_info_t* wlc_hw, bool want)
{
	WL_TRACE(("wl%d: wlc_bmac_xtal: want %d\n", wlc_hw->unit, want));

	/* dont power down if plldown is false or we must poll hw radio disable */
	if (!want && wlc_hw->pllreq)
		return;

	if (wlc_hw->sih)
		si_clkctl_xtal(wlc_hw->sih, XTAL|PLL, want);

	wlc_hw->sbclk = want;
	if (!wlc_hw->sbclk) {
		wlc_hw->clk = FALSE;
		if (wlc_hw->band && wlc_hw->band->pi)
			phy_hw_clk_state_upd(wlc_hw->band->pi, FALSE);
	}
}

void
wlc_bmac_recover_pkts(wlc_info_t *wlc, uint queue)
{
	void *p = NULL;
	volatile uint32 intstatus =
		R_REG(wlc->osh, &(D11Reggrp_intctrlregs(wlc, queue)->intstatus));

	if (intstatus) {
		if (!(intstatus & I_XI))
			WL_ERROR(("%s: failure 0x%x\t", __FUNCTION__, intstatus));

		while ((p = GETNEXTTXP(wlc, queue))) {
			wlc_txfifo_complete(wlc, queue, 1);
			PKTFREE(wlc->osh, p, TRUE);
		}
		W_REG(wlc->osh, &(D11Reggrp_intctrlregs(wlc, queue)->intstatus), intstatus);
	}

#ifdef BULKRX_PKTLIST
#else
	while ((p = dma_rx(WLC_HW_DI(wlc, queue))) != NULL)
#endif /* BULKRX_PKTLIST */
	{
		PKTFREE(wlc->osh, p, FALSE);
	}

#ifdef STS_FIFO_RXEN
#endif /* STS_FIFO_RXEN */
} /* wlc_bmac_recover_pkts */

static void
wlc_flushqueues(wlc_hw_info_t *wlc_hw)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint i;
	int pktcnt;

	BCM_REFERENCE(pktcnt);

	if (!PIO_ENAB_HW(wlc_hw)) {
#ifdef BCMPCIEDEV
		/* Release all pending pktfetch requests */
		wlc_pktfetch_queue_flush(wlc);
#endif /* BCMPCIEDEV */

#if defined(BCMHWA) && defined(HWA_TXSTAT_BUILD)
		if (hwa_dev) {
			hwa_txstat_reclaim(hwa_dev, 0);
		}
#endif /* (BCMHWA && HWA_TXSTAT_BUILD) */

		/* ensure packets freed by reset (dma flush) get their scb ptrs updated */
		(void)wlc_pcb_pktfree_cb_register(wlc->pcb, wlc_pkttag_scb_restore, wlc);

#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
		if (hwa_dev) {
			hwa_txfifo_dma_reclaim(hwa_dev, 0);
		}
#else /* !(BCMHWA && HWA_TXFIFO_BUILD) */
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
			if (wlc_hw->di[i]) {
#ifdef BCM_DMA_CT
				if (wlc_hw->aqm_di[i]) {
					dma_txreclaim(wlc_hw->aqm_di[i], HNDDMA_RANGE_ALL);
				}
#endif // endif

				/* free any posted tx packets */
#ifndef DONGLEBUILD
				if (AMPDU_HOST_ENAB(wlc->pub))
					pktcnt = wlc_txfifo_freed_pkt_cnt_noaqm(wlc, i);
				else
#endif // endif
					pktcnt = dma_txreclaim(wlc_hw->di[i], HNDDMA_RANGE_ALL);

				if (pktcnt > 0) {
					wlc_txfifo_complete(wlc, i, pktcnt);
					WL_ERROR(("wlc_flushqueues: reclaim fifo %d pkts %d\n",
					i, pktcnt));
				}

				pktcnt = TXPKTPENDGET(wlc, i);
				if (pktcnt > 0) {
					WL_ERROR(("wlc_flushqueues: fifo %d REMAINS %d pkts\n",
						i, pktcnt));
				}
				WL_TRACE(("wlc_flushqueues: pktpend fifo %d cleared\n", i));
			}
		}
#endif /* !(BCMHWA && HWA_TXFIFO_BUILD) */

		/* Free the packets which is early reclaimed */
#ifdef WL_RXEARLYRC
		while (wlc_hw->rc_pkt_head) {
			void *p = wlc_hw->rc_pkt_head;
			wlc_hw->rc_pkt_head = PKTLINK(p);
			PKTSETLINK(p, NULL);
			PKTFREE(wlc_hw->osh, p, FALSE);
		}
#endif // endif

#ifdef AP
		wlc_tx_fifo_sync_bcmc_reset(wlc);
#endif /* AP */

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	/* free any pending rx packets */
	if (STS_RX_ENAB(wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc->pub)) {
		wlc_bmac_sts_reset(wlc_hw);
	}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

		/* free any posted rx packets */
#if defined(BCMHWA) && defined(HWA_RXPATH_BUILD)
		if (hwa_dev) {
			hwa_rxpath_dma_reclaim(hwa_dev);
		}
#endif /* BCMHWA && HWA_RXPATH_BUILD */

		for (i = 0; i < MAX_RX_FIFO; i++) {
			if ((wlc_hw->di[i] != NULL) && wlc_bmac_rxfifo_enab(wlc_hw, i)) {
#ifdef STS_FIFO_RXEN
				if (STS_RX_ENAB(wlc->pub) && i == STS_FIFO) {
					dma_sts_rxreclaim(wlc_hw->di[i]);
				}
				else
#endif /* STS_FIFO_RXEN */
				{
					dma_rxreclaim(wlc_hw->di[i]);
				}
			}
		}
		/* unregister scb restore callback */
		(void)wlc_pcb_pktfree_cb_unregister(wlc->pcb, wlc_pkttag_scb_restore, wlc);
	} else {
		for (i = 0; i < NFIFO_LEGACY; i++) {
			if (wlc_hw->pio[i]) {
				/* include reset the counter */
				wlc_pio_txreclaim(wlc_hw->pio[i]);
			}
		}
		/* For PIO, no rx sw queue to reclaim */
	}

} /* wlc_flushqueues */

/** set the PIO mode bit in the control register for the rxfifo */
void
wlc_rxfifo_setpio(wlc_hw_info_t *wlc_hw)
{
	fifo64_t *fiforegs;

	fiforegs = (D11Reggrp_f64regs(wlc_hw, RX_FIFO));
	W_REG(wlc_hw->osh, &fiforegs->dmarcv.control, D64_RC_FM);
}

/**
 * Set the range of objmem memory that is organized as 32bit words to a value.
 * 'byte_offset' needs to be multiple of 4 bytes and
 * Buffer length 'len' must be an multiple of 4 bytes
 * 'sel' selects the type of memory
 */
void
wlc_bmac_set_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 val, int len, uint32 sel)
{
	d11regs_t *regs = wlc_hw->regs;
	int i;
	BCM_REFERENCE(regs);
	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	ASSERT((byte_offset & 3) == 0);
	ASSERT((len & 3) == 0);

	ASSERT(regs != NULL);

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), sel | OBJADDR_AUTO_INC | (byte_offset >> 2));
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));

	for (i = 0; i < len; i += 4) {
		W_REG(wlc_hw->osh, D11_objdata(wlc_hw), val);
	}
}

/**
 * Copy a buffer to an objmem memory that is organized as 32bit words.
 * 'byte_offset' needs to be multiple of 4 bytes and
 * Buffer length 'len' must be an multiple of 4 bytes
 * 'sel' selects the type of memory
 */
void
wlc_bmac_copyto_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, const uint8 *buf, int len,
                         uint32 sel)
{
	d11regs_t *regs = wlc_hw->regs;
	const uint8* p = buf;
	int i;
	uint32 val;
	BCM_REFERENCE(regs);

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	ASSERT((byte_offset & 3) == 0);
	ASSERT((len & 3) == 0);

	ASSERT(regs != NULL);

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), sel | OBJADDR_AUTO_INC | (byte_offset >> 2));
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));

	for (i = 0; i < len; i += 4) {
		val = p[i] | (p[i+1] << 8) | (p[i+2] << 16) | (p[i+3] << 24);
		val = htol32(val);
		W_REG(wlc_hw->osh, D11_objdata(wlc_hw), val);
	}

}

/**
 * Copy objmem memory that is organized as 32bit words to a buffer.
 * 'byte_offset' needs to be multiple of 4 bytes and
 * Buffer length 'len' must be an multiple of 4 bytes
 * 'sel' selects the type of memory
 */
void
wlc_bmac_copyfrom_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint8 *buf, int len, uint32 sel)
{
	d11regs_t *regs = wlc_hw->regs;
	uint8* p = buf;
	int i, len32 = (len/4)*4;
	uint32 val;
	BCM_REFERENCE(regs);
	ASSERT(regs != NULL);

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	ASSERT((byte_offset & 3) == 0);
	ASSERT((len & 3) == 0);

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), sel | OBJADDR_AUTO_INC | (byte_offset >> 2));
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	for (i = 0; i < len32; i += 4) {
		val = R_REG(wlc_hw->osh, D11_objdata(wlc_hw));
		val = ltoh32(val);
		p[i] = val & 0xFF;
		p[i+1] = (val >> 8) & 0xFF;
		p[i+2] = (val >> 16) & 0xFF;
		p[i+3] = (val >> 24) & 0xFF;
	}
}

uint16
wlc_bmac_read_shm(wlc_hw_info_t *wlc_hw, uint byte_offset)
{
	return  wlc_bmac_read_objmem16(wlc_hw, byte_offset, OBJADDR_SHM_SEL);
}

void
wlc_bmac_write_shm(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v)
{
	wlc_bmac_write_objmem16(wlc_hw, byte_offset, v, OBJADDR_SHM_SEL);
}

void
wlc_bmac_update_shm(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v, uint16 mask)
{
	wlc_bmac_update_objmem16(wlc_hw, byte_offset, v, mask, OBJADDR_SHM_SEL);
}

/**
 * Set a range of shared memory to a value.
 * SHM 'offset' needs to be an even address and
 * Buffer length 'len' must be an even number of bytes
 */
void
wlc_bmac_set_shm(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v, int len)
{
	int i;

	/* offset and len need to be even */
	ASSERT((byte_offset & 1) == 0);
	ASSERT((len & 1) == 0);

	if (len <= 0)
		return;

	for (i = 0; i < len; i += 2) {
		wlc_bmac_write_objmem16(wlc_hw, byte_offset + i, v, OBJADDR_SHM_SEL);
	}
}

uint16 wlc_bmac_read_shm_axi_slave(wlc_hw_info_t *wlc_hw, uint offset)
{
	if (D11REV_GE(wlc_hw->corerev, 61)) {
		return R_REG(wlc_hw->osh,
			(uint16*)((unsigned long int)wlc_hw->d11axi_slave_base_addr +
			AXISL_SHM_BASE + offset));
	} else {
		return wlc_bmac_read_shm(wlc_hw, offset);
	}
}

void wlc_bmac_write_shm_axi_slave(wlc_hw_info_t *wlc_hw, uint offset, uint16 value)
{
	if (D11REV_GE(wlc_hw->corerev, 61)) {
		W_REG(wlc_hw->osh,
			(uint16*)((unsigned long int)wlc_hw->d11axi_slave_base_addr +
			AXISL_SHM_BASE + offset), value);
	} else {
		wlc_bmac_write_shm(wlc_hw, offset, value);
	}
}

#if defined(WL_PSMX)
uint16
wlc_bmac_read_shmx(wlc_hw_info_t *wlc_hw, uint byte_offset)
{
	return  wlc_bmac_read_objmem16(wlc_hw, byte_offset, OBJADDR_SHMX_SEL);
}

void
wlc_bmac_write_shmx(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v)
{
	wlc_bmac_write_objmem16(wlc_hw, byte_offset, v, OBJADDR_SHMX_SEL);
}
#endif /* WL_PSMX */

#if defined(WL_PSMR1)
uint16
wlc_bmac_read_shm1(wlc_hw_info_t *wlc_hw, uint byte_offset)
{
	return  wlc_bmac_read_objmem16(wlc_hw, byte_offset, OBJADDR_SHM1_SEL);
}

void
wlc_bmac_write_shm1(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v)
{
	wlc_bmac_write_objmem16(wlc_hw, byte_offset, v, OBJADDR_SHM1_SEL);
}
#endif /* WL_PSMR1 */

#if defined(WL_PSMX) && defined(NOT_YET)
/** @param[in] offset   In [16 bit word] units */
static uint16
wlc_bmac_read_macregx(wlc_hw_info_t *wlc_hw, uint offset)
{
	ASSERT(offset >= D11REG_IHR_BASE);
	offset = (offset - D11REG_IHR_BASE) << 1; /* from word to byte offset */
	return wlc_bmac_read_objmem16(wlc_hw, offset, OBJADDR_IHRX_SEL);
}

/** @param[in] offset   In [16 bit word] units */
static void
wlc_bmac_write_macregx(wlc_hw_info_t *wlc_hw, uint offset, uint16 v)
{
	ASSERT(offset >= D11REG_IHR_BASE);
	offset = (offset - D11REG_IHR_BASE) << 1; /* from word to byte offset */
	wlc_bmac_write_objmem16(wlc_hw, offset, v, OBJADDR_IHRX_SEL);
}
#endif /* WL_PSMX && NOT_YET */

static uint16
wlc_bmac_read_objmem16(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 sel)
{
	d11regs_t *regs = wlc_hw->regs;
	volatile uint16* objdata_lo = (volatile uint16*)(uintptr)D11_objdata(wlc_hw);
	volatile uint16* objdata_hi = objdata_lo + 1;
	uint16 v;
	BCM_REFERENCE(regs);
	ASSERT(regs != NULL);

#ifdef BCM_BACKPLANE_TIMEOUT
	if (si_deviceremoved(wlc_hw->wlc->pub->sih)) {
		WL_ERROR(("%s: offset:%x, sel:%x\n",
			__FUNCTION__, offset, sel));
		return ID16_INVALID;
	}
#endif /* BCM_BACKPLANE_TIMEOUT */

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	ASSERT((byte_offset & 1) == 0);
	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), sel | (byte_offset >> 2));
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	if (byte_offset & 2) {
		v = R_REG(wlc_hw->osh, objdata_hi);
	} else {
		v = R_REG(wlc_hw->osh, objdata_lo);
	}

	return v;
} /* wlc_bmac_read_objmem16 */

static uint32
wlc_bmac_read_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 sel)
{
	uint32 v;

#ifdef BCM_BACKPLANE_TIMEOUT
	if (si_deviceremoved(wlc_hw->wlc->pub->sih)) {
		WL_ERROR(("%s: offset:%x, sel:%x\n",
			__FUNCTION__, byte_offset, sel));
		return ID16_INVALID;
	}
#endif /* BCM_BACKPLANE_TIMEOUT */

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	ASSERT((byte_offset & 3) == 0);

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), sel | (byte_offset >> 2));
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	v = R_REG(wlc_hw->osh, D11_objdata(wlc_hw));

	return v;
} /* wlc_bmac_read_objmem32 */

static void
wlc_bmac_write_objmem16(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v, uint32 sel)
{
	d11regs_t *regs = wlc_hw->regs;
	volatile uint16* objdata_lo = (volatile uint16*)(uintptr)D11_objdata(wlc_hw);
	volatile uint16* objdata_hi = objdata_lo + 1;
	BCM_REFERENCE(regs);

#ifdef BCM_BACKPLANE_TIMEOUT
	if (si_deviceremoved(wlc_hw->wlc->pub->sih)) {
		WL_ERROR(("%s: offset:%x, val%x, sel:%x\n",
			__FUNCTION__, byte_offset, v, sel));
		return;
	}
#endif /* BCM_BACKPLANE_TIMEOUT */

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	ASSERT(regs != NULL);

	ASSERT((byte_offset & 1) == 0);

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), sel | (byte_offset >> 2));
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	if (byte_offset & 2) {
		W_REG(wlc_hw->osh, objdata_hi, v);
	} else {
		W_REG(wlc_hw->osh, objdata_lo, v);
	}
} /* wlc_bmac_write_objmem16 */

static void
wlc_bmac_write_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 v, uint32 sel)
{
	d11regs_t *regs = wlc_hw->regs;
	BCM_REFERENCE(regs);

#ifdef BCM_BACKPLANE_TIMEOUT
	if (si_deviceremoved(wlc_hw->wlc->pub->sih)) {
		WL_ERROR(("%s: offset:%x, val%x, sel:%x\n",
			__FUNCTION__, offset, v, sel));
		return;
	}
#endif /* BCM_BACKPLANE_TIMEOUT */

	ASSERT(wlc_hw->clk);
#ifndef EFI
	PANIC_CHECK_CLK(wlc_hw->clk, "wl%d: %s: NO CLK\n", wlc_hw->unit, __FUNCTION__);
#endif // endif

	ASSERT(regs != NULL);
	ASSERT((byte_offset & 3) == 0);

	W_REG(wlc_hw->osh, D11_objaddr(wlc_hw), sel | (byte_offset >> 2));
	(void)R_REG(wlc_hw->osh, D11_objaddr(wlc_hw));
	W_REG(wlc_hw->osh, D11_objdata(wlc_hw), v);
}

static void
wlc_bmac_update_objmem16(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v, uint16 mask,
                         uint32 objsel)
{
	uint16 objval;

	ASSERT((v & ~mask) == 0);

	objval = wlc_bmac_read_objmem16(wlc_hw, byte_offset, objsel);
	objval = (objval & ~mask) | v;
	wlc_bmac_write_objmem16(wlc_hw, byte_offset, objval, objsel);
}

/**
 * Copy a buffer to shared memory of specified type .
 * SHM 'byte_offset' needs to be an even address and
 * Buffer length 'len' must be an even number of bytes
 * 'sel' selects the type of memory
 */
void
wlc_bmac_copyto_objmem(wlc_hw_info_t *wlc_hw, uint byte_offset, const void* buf, int len,
                       uint32 sel)
{
	const uint8* p = (const uint8*)buf;
	int i;
	uint16 v16;
	uint32 v32;

	/* offset and len need to be even */
	ASSERT((byte_offset & 1) == 0);
	ASSERT((len & 1) == 0);

	if (len <= 0)
		return;

	/* Some of the OBJADDR memories can be accessed as 4 byte
	 * and some as 2 byte
	 */
	if (OBJADDR_2BYTES_ACCESS(sel)) {
		for (i = 0; i < len; i += 2) {
			v16 = htol16(p[i] | (p[i+1] << 8));
			wlc_bmac_write_objmem16(wlc_hw, byte_offset + i, v16, sel);
		}
	} else {
		int len16 = (len%4);
		int len32 = (len/4)*4;

		/* offset needs to be multiple of 4 here */
		ASSERT((byte_offset & 3) == 0);

		/* Write all the 32bit words */
		for (i = 0; i < len32; i += 4) {
			v32 = htol32(p[i] | (p[i+1] << 8) | (p[i+2] << 16) | (p[i+3] << 24));
			wlc_bmac_write_objmem32(wlc_hw, byte_offset + i, v32, sel);
		}

		/* Write the last 16bit if any */
		if (len16) {
			v16 = htol16(p[i] | (p[i+1] << 8));
			wlc_bmac_write_objmem16(wlc_hw, byte_offset + i, v16, sel);
		}

	}
} /* wlc_bmac_copyto_objmem */

/**
 * Copy a piece of shared memory of specified type to a buffer .
 * SHM 'byte_offset' needs to be an even address and
 * Buffer length 'len' must be an even number of bytes
 * 'sel' selects the type of memory
 */
void
wlc_bmac_copyfrom_objmem(wlc_hw_info_t *wlc_hw, uint byte_offset, void* buf, int len, uint32 sel)
{
	uint8* p = (uint8*)buf;
	int i;
	uint16 v16;
	uint32 v32;

	/* offset and len need to be even */
	ASSERT((byte_offset & 1) == 0);
	ASSERT((len & 1) == 0);

	if (len <= 0)
		return;

	/* Some of the OBJADDR memories can be accessed as 4 byte
	 * and some as 2 byte
	 */
	if (OBJADDR_2BYTES_ACCESS(sel)) {
		for (i = 0; i < len; i += 2) {
			v16 = ltoh16(wlc_bmac_read_objmem16(wlc_hw, byte_offset + i, sel));
			p[i] = v16 & 0xFF;
			p[i+1] = (v16 >> 8) & 0xFF;
		}
	} else {
		int len16 = (len%4);
		int len32 = (len/4)*4;

		/* offset needs to be multiple of 4 here */
		ASSERT((byte_offset & 3) == 0);

		/* Read all the 32bit words */
		for (i = 0; i < len32; i += 4) {
			v32 = ltoh32(wlc_bmac_read_objmem32(wlc_hw, byte_offset + i, sel));
			p[i] = v32 & 0xFF;
			p[i+1] = (v32 >> 8) & 0xFF;
			p[i+2] = (v32 >> 16) & 0xFF;
			p[i+3] = (v32 >> 24) & 0xFF;
		}

		/* Read the last 16bit if any */
		if (len16) {
			v16 = ltoh16(wlc_bmac_read_objmem16(wlc_hw, byte_offset + i, sel));
			p[i] = v16 & 0xFF;
			p[i+1] = (v16 >> 8) & 0xFF;
		}

	}
} /* wlc_bmac_copyfrom_objmem */

void
wlc_bmac_copyfrom_vars(wlc_hw_info_t *wlc_hw, char ** buf, uint *len)
{
	WL_TRACE(("wlc_bmac_copyfrom_vars, nvram vars totlen=%d\n", wlc_hw->vars_size));

	if (wlc_hw->vars) {
		*buf = wlc_hw->vars;
		*len = wlc_hw->vars_size;
	}
}

void
wlc_bmac_retrylimit_upd(wlc_hw_info_t *wlc_hw, uint16 SRL, uint16 LRL)
{
	wlc_hw->SRL = SRL;
	wlc_hw->LRL = LRL;

	/* write retry limit to SCR, shouldn't need to suspend */
	if (wlc_hw->up) {
		wlc_bmac_copyto_objmem(wlc_hw, S_DOT11_SRC_LMT << 2, &(wlc_hw->SRL),
			sizeof(wlc_hw->SRL), OBJADDR_SCR_SEL);

		wlc_bmac_copyto_objmem(wlc_hw, S_DOT11_LRC_LMT << 2, &(wlc_hw->LRL),
			sizeof(wlc_hw->LRL), OBJADDR_SCR_SEL);
	}
}

void
wlc_bmac_set_noreset(wlc_hw_info_t *wlc_hw, bool noreset_flag)
{
	wlc_hw->noreset = noreset_flag;
}

bool
wlc_bmac_get_noreset(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->noreset;
}

bool
wlc_bmac_p2p_cap(wlc_hw_info_t *wlc_hw)
{
#ifdef WLP2P_UCODE
	return wlc_hw->corerev >= 15;
#else
	return FALSE;
#endif // endif
}

int
wlc_bmac_p2p_set(wlc_hw_info_t *wlc_hw, bool enable)
{
	if (wlc_hw->_p2p == enable)
		return BCME_OK;
	if (enable &&
	    !wlc_bmac_p2p_cap(wlc_hw))
		return BCME_ERROR;
#ifdef WLP2P_UCODE
#ifdef WLP2P_UCODE_ONLY
	if (!enable)
		return BCME_ERROR;
#endif // endif
	wlc_hw->ucode_loaded = FALSE;
	wlc_hw->_p2p = enable;
#endif /* WLP2P_UCODE */
	return BCME_OK;
}

void
wlc_bmac_pllreq(wlc_hw_info_t *wlc_hw, bool set, mbool req_bit)
{
	ASSERT(req_bit);

	if (set) {
		if (mboolisset(wlc_hw->pllreq, req_bit))
			return;

		mboolset(wlc_hw->pllreq, req_bit);

		if (mboolisset(wlc_hw->pllreq, WLC_PLLREQ_FLIP)) {
			if (!wlc_hw->sbclk) {
				wlc_bmac_xtal(wlc_hw, ON);
			}
		}
	} else {
		if (!mboolisset(wlc_hw->pllreq, req_bit))
			return;

		mboolclr(wlc_hw->pllreq, req_bit);

		if (mboolisset(wlc_hw->pllreq, WLC_PLLREQ_FLIP)) {
			if (wlc_hw->sbclk) {
				wlc_bmac_xtal(wlc_hw, OFF);
			}
		}
	}

	return;
}

void
wlc_bmac_set_clk(wlc_hw_info_t *wlc_hw, bool on)
{
	if (on) {
		/* power up pll and oscillator */
		wlc_bmac_xtal(wlc_hw, ON);

		/* enable core(s), ignore bandlocked
		 * Leave with the same band selected as we entered
		 */
		wlc_bmac_corereset(wlc_hw, WLC_USE_COREFLAGS);
	} else {
		/* if already down, must skip the core disable */
		if (wlc_hw->clk) {
			/* disable core(s), ignore bandlocked */
			wlc_coredisable(wlc_hw);
		}
			/* power down pll and oscillator */
		wlc_bmac_xtal(wlc_hw, OFF);
	}
}

#ifdef BCMASSERT_SUPPORT
bool
wlc_bmac_taclear(wlc_hw_info_t *wlc_hw, bool ta_ok)
{
	return (!wlc_hw->sbclk || !si_taclear(wlc_hw->sih, !ta_ok));
}
#endif // endif

#ifdef WLLED
/** may touch sb register inside */
void
wlc_bmac_led_hw_deinit(wlc_hw_info_t *wlc_hw, uint32 gpiomask_cache)
{
	/* BMAC_NOTE: split mac should not worry about pci cfg access to disable GPIOs. */
	bool xtal_set = FALSE;

	if (!wlc_hw->sbclk) {
		wlc_bmac_xtal(wlc_hw, ON);
		xtal_set = TRUE;
	}

	/* opposite sequence of wlc_led_init */
	if (wlc_hw->sih) {
		si_gpioout(wlc_hw->sih, gpiomask_cache, 0, GPIO_DRV_PRIORITY);
		si_gpioouten(wlc_hw->sih, gpiomask_cache, 0, GPIO_DRV_PRIORITY);
		si_gpioled(wlc_hw->sih, gpiomask_cache, 0);
	}

	if (xtal_set)
		wlc_bmac_xtal(wlc_hw, OFF);
}

void
wlc_bmac_led_hw_mask_init(wlc_hw_info_t *wlc_hw, uint32 mask)
{
	wlc_hw->led_gpio_mask = mask;
}

static void
wlc_bmac_led_hw_init(wlc_hw_info_t *wlc_hw)
{
	uint32 mask = wlc_hw->led_gpio_mask, val = 0;
	struct bmac_led *led;
	bmac_led_info_t *li = wlc_hw->ledh;

	if (!wlc_hw->sbclk)
		return;

	/* designate gpios driving LEDs . Make sure that we have the control */
	si_gpiocontrol(wlc_hw->sih, mask, 0, GPIO_DRV_PRIORITY);
	si_gpioled(wlc_hw->sih, mask, mask);

	/* Begin with LEDs off */
	for (led = &li->led[0]; led < &li->led[WL_LED_NUMGPIO]; led++) {
		if (!led->activehi)
			val |= (1 << led->pin);
	}
	val = val & mask;

	if (!(wlc_hw->boardflags2 & BFL2_TRISTATE_LED)) {
		li->gpioout_cache = si_gpioout(wlc_hw->sih, mask, val, GPIO_DRV_PRIORITY);
		si_gpioouten(wlc_hw->sih, mask, mask, GPIO_DRV_PRIORITY);
	} else {
		si_gpioout(wlc_hw->sih, mask, ~val & mask, GPIO_DRV_PRIORITY);
		li->gpioout_cache = si_gpioouten(wlc_hw->sih, mask, 0, GPIO_DRV_PRIORITY);
		/* for tristate leds, clear gpiopullup/gpiopulldown registers to
		 * allow the tristated gpio to float
		 */
		if (CCREV(wlc_hw->sih->ccrev) >= 20) {
			si_gpiopull(wlc_hw->sih, GPIO_PULLDN, mask, 0);
			si_gpiopull(wlc_hw->sih, GPIO_PULLUP, mask, 0);
		}
	}

	li->gpiomask_cache = mask;

	/* set override bit for the GPIO line controlling the LED */
	val = 0;
	for (led = &li->led[0]; led < &li->led[WL_LED_NUMGPIO]; led++) {
		if (led->pin_ledbh) {
			if (val == 0) {
				val = si_pmu_chipcontrol(wlc_hw->sih, PMU_CHIPCTL1, 0, 0);
			}

			val |= (1 << (PMU_CCA1_OVERRIDE_BIT_GPIO0 + led->pin));
		}
	}

	if (val) {
		si_pmu_chipcontrol(wlc_hw->sih, PMU_CHIPCTL1, 0xFFFFFFFF, val);
	}
} /* wlc_bmac_led_hw_init */

/** called by the led_blink_timer at every li->led_blink_time interval */
static void
wlc_bmac_led_blink_timer(bmac_led_info_t *li)
{
	struct bmac_led *led;
#if OSL_SYSUPTIME_SUPPORT
	uint32 now = OSL_SYSUPTIME();
	/* Timer event can come early, and the LED on/off state change will be missed until the
	 * next li->led_blink_time cycle. Thus, the LED on/off state could be extended. To adjust
	 * for this situation, LED time may need to restart at the end of the current
	 * li->led_blink_time cycle
	 */
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)li->wlc_hw;
	uint time_togo;
	uint restart_time = 0;
	uint time_passed;

	/* blink each pin at its respective blinkrate */
	for (led = &li->led[0]; led < &li->led[WL_LED_NUMGPIO]; led++) {
		if (led->msec_on || led->msec_off) {
			bool change_state = FALSE;
			uint factor;

			time_passed = now - led->timestamp;

			/* Currently off */
			if ((led->next_state) || (led->restart)) {
				if (time_passed > led->msec_off)
					change_state = TRUE;
				else {
					time_togo = led->msec_off - time_passed;
					factor = (led->msec_off > 1000) ? 20 : 10;
					if (time_togo < li->led_blink_time) {
						if (time_togo < led->msec_off/factor ||
							time_togo < LED_BLINK_TIME) {
							if (li->led_blink_time - time_togo >
								li->led_blink_time/10)
								change_state = TRUE;
						} else {
							if (!restart_time)
								restart_time = time_togo;
							else if (time_togo < restart_time)
								restart_time = time_togo;
						}
					}
				}

				/* Blink on */
				if (led->restart || change_state) {
					wlc_bmac_led((wlc_hw_info_t*)li->wlc_hw,
					             (1<<led->pin), (1<<led->pin), led->activehi);
					led->next_state = OFF;
					led->timestamp = now;
					led->restart = FALSE;
				}
			}
			/* Currently on */
			else {
				if (time_passed > led->msec_on)
					change_state = TRUE;
				else {
							time_togo = led->msec_on - time_passed;
					if (time_togo < li->led_blink_time) {
						factor = (led->msec_on > 1000) ? 20 : 10;
						if (time_togo < led->msec_on/factor ||
							time_togo < LED_BLINK_TIME) {
							if (li->led_blink_time - time_togo >
								li->led_blink_time/10)
								change_state = TRUE;
						} else {
							if (!restart_time)
								restart_time = time_togo;
							else if (time_togo < restart_time)
								restart_time = time_togo;
						}
					}
				}

				/* Blink off  */
				if (change_state) {
					wlc_bmac_led((wlc_hw_info_t*)li->wlc_hw,
					             (1<<led->pin), 0, led->activehi);
					led->next_state = ON;
					led->timestamp = now;
				}
			}
		}
	}

	if (restart_time) {
#ifdef BCMDBG
		WL_TRACE(("restart led blink timer in %dms\n", restart_time));
#endif // endif
		wl_del_timer(wlc_hw->wlc->wl, li->led_blink_timer);
		wl_add_timer(wlc_hw->wlc->wl, li->led_blink_timer, restart_time, 0);
		li->blink_start = TRUE;
		li->blink_adjust = TRUE;
	} else if (li->blink_adjust) {
#ifdef BCMDBG
		WL_TRACE(("restore led_blink_time to %d\n", li->led_blink_time));
#endif // endif
		wlc_bmac_led_blink_event(wlc_hw, TRUE);
		li->blink_start = TRUE;
		li->blink_adjust = FALSE;
	}
#else
	for (led = &li->led[0]; led < &li->led[WL_LED_NUMGPIO]; led++) {
		if (led->blinkmsec) {
			if (led->blinkmsec > (int32) led->msec_on) {
				wlc_bmac_led((wlc_hw_info_t*)li->wlc_hw,
				             (1<<led->pin), 0, led->activehi);
			} else {
				wlc_bmac_led((wlc_hw_info_t*)li->wlc_hw,
				             (1<<led->pin), (1<<led->pin), led->activehi);
			}
			led->blinkmsec -= LED_BLINK_TIME;
			if (led->blinkmsec <= 0)
				led->blinkmsec = led->msec_on + led->msec_off;
		}
	}
#endif /* (OSL_SYSUPTIME_SUPPORT) */
} /* wlc_bmac_led_blink_timer */

static void
wlc_bmac_timer_led_blink(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	wlc_hw_info_t *wlc_hw = wlc->hw;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc_hw->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

	wlc_bmac_led_blink_timer(wlc_hw->ledh);
}

bmac_led_info_t *
BCMATTACHFN(wlc_bmac_led_attach)(wlc_hw_info_t *wlc_hw)
{
	bmac_led_info_t *bmac_li;
	bmac_led_t *led;
	int i;
	char name[32];
	char *var;
	uint val;

	if ((bmac_li = (bmac_led_info_t *)MALLOC
			(wlc_hw->osh, sizeof(bmac_led_info_t))) == NULL) {
		printf(rstr_bmac_led_attach_out_of_mem_malloced_D_bytes,
			MALLOCED(wlc_hw->osh));
		goto fail;
	}
	bzero((char *)bmac_li, sizeof(bmac_led_info_t));

	led = &bmac_li->led[0];
	for (i = 0; i < WL_LED_NUMGPIO; i ++) {
		led->pin = i;
		led->activehi = TRUE;
#if OSL_SYSUPTIME_SUPPORT
		/* current time, in ms, for computing LED blink duration */
		led->timestamp = OSL_SYSUPTIME();
		led->next_state = ON; /* default to turning on */
#endif // endif
		led ++;
	}

	/* look for led gpio/behavior nvram overrides */
	for (i = 0; i < WL_LED_NUMGPIO; i++) {
		led = &bmac_li->led[i];

		snprintf(name, sizeof(name), rstr_ledbhD, i);

		if ((var = getvar(wlc_hw->vars, name)) == NULL) {
			snprintf(name, sizeof(name), rstr_wl0gpioD, i);
			if ((var = getvar(wlc_hw->vars, name)) == NULL) {
				continue;
			}
		}

		val = bcm_strtoul(var, NULL, 0);

		/* silently ignore old card srom garbage */
		if ((val & WL_LED_BEH_MASK) >= WL_LED_NUMBEHAVIOR)
			continue;

		led->pin = i;	/* gpio pin# == led index# */
		if (val & WL_LED_PMU_OVERRIDE) {
			led->pin_ledbh = TRUE;
		}
		led->activehi = (val & WL_LED_AL_MASK)? FALSE : TRUE;
	}

	bmac_li->wlc_hw = wlc_hw;
	if (!(bmac_li->led_blink_timer = wl_init_timer
			(wlc_hw->wlc->wl, wlc_bmac_timer_led_blink, wlc_hw->wlc,
	                                          "led_blink"))) {
		printf(rstr_wlD_led_attach_wl_init_timer_for_led_blink_timer_failed,
			wlc_hw->unit);
		goto fail;
	}

#if !OSL_SYSUPTIME_SUPPORT
	bmac_li->led_blink_time = LED_BLINK_TIME;
#endif // endif

	return bmac_li;

fail:
	if (bmac_li) {
		MFREE(wlc_hw->osh, bmac_li, sizeof(bmac_led_info_t));
	}
	return NULL;
} /* wlc_bmac_led_attach */

int
BCMATTACHFN(wlc_bmac_led_detach)(wlc_hw_info_t *wlc_hw)
{
	bmac_led_info_t *li = wlc_hw->ledh;
	int callbacks = 0;

	if (li) {
		if (li->led_blink_timer) {
			if (!wl_del_timer(wlc_hw->wlc->wl, li->led_blink_timer))
				callbacks++;
			wl_free_timer(wlc_hw->wlc->wl, li->led_blink_timer);
			li->led_blink_timer = NULL;
		}

		MFREE(wlc_hw->osh, li, sizeof(bmac_led_info_t));
	}

	return callbacks;
}

static void
wlc_bmac_led_blink_off(bmac_led_info_t *li)
{
	struct bmac_led *led;

	/* blink each pin at its respective blinkrate */
	for (led = &li->led[0]; led < &li->led[WL_LED_NUMGPIO]; led++) {
		if (led->msec_on || led->msec_off) {
			wlc_bmac_led((wlc_hw_info_t*)li->wlc_hw,
				(1<<led->pin), 0, led->activehi);
#if OSL_SYSUPTIME_SUPPORT
			led->restart = TRUE;
#endif // endif
		}
	}
}

int
wlc_bmac_led_blink_event(wlc_hw_info_t *wlc_hw, bool blink)
{
	bmac_led_info_t *li = (bmac_led_info_t *)(wlc_hw->ledh);

	if (blink) {
		wl_del_timer(wlc_hw->wlc->wl, li->led_blink_timer);
		wl_add_timer(wlc_hw->wlc->wl, li->led_blink_timer, li->led_blink_time, 1);
		li->blink_start = TRUE;
	} else {
		if (!wl_del_timer(wlc_hw->wlc->wl, li->led_blink_timer))
			return 1;
		li->blink_start = FALSE;
		wlc_bmac_led_blink_off(li);
	}
	return 0;
}

void
wlc_bmac_led_set(wlc_hw_info_t *wlc_hw, int indx, uint8 activehi)
{
	bmac_led_t *led = &wlc_hw->ledh->led[indx];

	led->activehi = activehi;

	return;
}

void
wlc_bmac_led_blink(wlc_hw_info_t *wlc_hw, int indx, uint16 msec_on, uint16 msec_off)
{
	bmac_led_t *led = &wlc_hw->ledh->led[indx];
#if OSL_SYSUPTIME_SUPPORT
	bmac_led_info_t *li = (bmac_led_info_t *)(wlc_hw->ledh);
	uint num_leds_set = 0;
	uint led_blink_rates[WL_LED_NUMGPIO];
	uint tmp, a, b, i;
	led_blink_rates[0] = 1000; /* 1 sec, default timer */
#endif // endif

	led->msec_on = msec_on;
	led->msec_off = msec_off;

#if !OSL_SYSUPTIME_SUPPORT
	led->blinkmsec = msec_on + msec_off;
#else
	if ((led->msec_on != msec_on) || (led->msec_off != msec_off)) {
		led->restart = TRUE;
	}

	/* recompute to an optimized blink rate timer interval */
	for (led = &li->led[0]; led < &li->led[WL_LED_NUMGPIO]; led++) {
		if (!(led->msec_on || led->msec_off)) {
			led->restart = TRUE;
			continue;
		}

		/* compute the GCF of this particular LED's on+off rates */
		b = led->msec_off;
		a = led->msec_on;
		while (b != 0) {
			tmp = b;
			b = a % b;
			a = tmp;
		}

		led_blink_rates[num_leds_set++] = a;
	}

	/* compute the GCF across all LEDs, if more than one */
	a = led_blink_rates[0];

	for (i = 1; i < num_leds_set; i++) {
		b = led_blink_rates[i];
		while (b != 0) {
			tmp = b;
			b = a % b;
			a = tmp; /* A is the running GCF */
		}
	}

	li->led_blink_time = MAX(a, LED_BLINK_TIME);

	if (num_leds_set) {
		if ((li->blink_start) && !li->blink_adjust) {
			wlc_bmac_led_blink_event(wlc_hw, FALSE);
			wlc_bmac_led_blink_event(wlc_hw, TRUE);
		}
	}

#endif /* !(OSL_SYSUPTIME_SUPPORT) */
	return;
} /* wlc_bmac_led_blink */

void
wlc_bmac_blink_sync(wlc_hw_info_t *wlc_hw, uint32 led_pins)
{
#if OSL_SYSUPTIME_SUPPORT
	bmac_led_info_t *li = wlc_hw->ledh;
	int i;

	for (i = 0; i < WL_LED_NUMGPIO; i++) {
		if (led_pins & (0x1 << i)) {
			li->led[i].restart = TRUE;
		}
	}
#endif // endif

	return;
}

/** turn gpio bits on or off */
void
wlc_bmac_led(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val, bool activehi)
{
	bmac_led_info_t *li = wlc_hw->ledh;
	bool off = (val != mask);

	ASSERT((val & ~mask) == 0);

	if (!wlc_hw->sbclk)
		return;

	if (!activehi)
		val = ((~val) & mask);

	if (wlc_led_cled(wlc_hw->wlc->ledh, mask, val, activehi) == 0)
		return;

	/* Tri-state the GPIO if the board flag is set */
	if (wlc_hw->boardflags2 & BFL2_TRISTATE_LED) {
		if ((!activehi && ((val & mask) == (li->gpioout_cache & mask))) ||
		    (activehi && ((val & mask) != (li->gpioout_cache & mask))))
			li->gpioout_cache = si_gpioouten(wlc_hw->sih, mask, off ? 0 : mask,
			                                 GPIO_DRV_PRIORITY);
	} else {
		/* prevent the unnecessary writes to the gpio */
		if ((val & mask) != (li->gpioout_cache & mask))
			/* Traditional GPIO behavior */
			li->gpioout_cache = si_gpioout(wlc_hw->sih, mask, val,
			                               GPIO_DRV_PRIORITY);
	}
}
#endif /* WLLED */

/* wlc_iocv_fwd interface implementation */
int
wlc_iocv_fwd_disp_iov(void *ctx, uint16 tid, uint32 aid, uint16 type,
	void *p, uint plen, void *a, uint alen, uint v_sz, struct wlc_if *wlcif)
{
	wlc_hw_info_t *wlc_hw = ctx;
	BCM_REFERENCE(type);
	return wlc_iocv_low_dispatch_iov(wlc_hw->iocvi, tid, aid, p, plen, a, alen, v_sz, wlcif);
}

int
wlc_iocv_fwd_disp_ioc(void *ctx, uint16 tid, uint32 cid, uint16 type,
	void *a, uint alen, struct wlc_if *wlcif)
{
	wlc_hw_info_t *wlc_hw = ctx;
	BCM_REFERENCE(type);
	return wlc_iocv_low_dispatch_ioc(wlc_hw->iocvi, tid, cid, a, alen, wlcif);
}

int
wlc_iocv_fwd_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_hw_info_t *wlc_hw = ctx;
	return wlc_iocv_low_dump(wlc_hw->iocvi, b);
}

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
#endif // endif

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_PHYDUMP)
/** register a dump name/callback in bmac */
static int
wlc_bmac_add_dump_fn(wlc_hw_info_t *wlc_hw, const char *name,
	bmac_dump_fn_t fn, const void *ctx)
{
/* unfortunately no way of disposing of const cast - the 'ctx'
 * is a parameter passed back to the callback as it can't force
 * the callback to take a const void *ctx in case the callback
 * does need to modify the object so let's leave that protection
 * to the callback itself.
 */
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == \
	4 && __GNUC_MINOR__ >= 6))
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#endif // endif
	return wlc_dump_reg_add_fn(wlc_hw->dump, name, (wlc_dump_reg_fn_t)fn, (void *)ctx);
#if defined(STRICT_GCC_WARNINGS) && defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == \
	4 && __GNUC_MINOR__ >= 6))
_Pragma("GCC diagnostic pop")
#endif // endif
}
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP) || defined(WLTEST)
/** lookup a dump name in phy and execute it if found */
static int
wlc_bmac_dump_phy(wlc_hw_info_t *wlc_hw, const char *name, struct bcmstrbuf *b)
{
	int ret = BCME_UNSUPPORTED;
	bool ta_ok = FALSE;

#if defined(DBG_PHY_IOV)
	if (!strcmp(name, "radioreg") || !strcmp(name, "phyreg") || !strcmp(name, "phytbl")) {
		ret = phy_dbg_dump((phy_info_t *)wlc_hw->band->pi, name, b);
		ta_ok = TRUE;
	} else
#endif // endif
	{
	bool single_phy, a_only;
	single_phy = (wlc_hw->bandstate[0]->pi == wlc_hw->bandstate[1]->pi) ||
	        (wlc_hw->bandstate[1]->pi == NULL);

	a_only = (wlc_hw->bandstate[0]->pi == NULL);

	if (wlc_hw->bandstate[0]->pi)
		ret = phy_dbg_dump((phy_info_t *)wlc_hw->bandstate[0]->pi, name, b);
	if (!single_phy || a_only)
		ret = phy_dbg_dump((phy_info_t *)wlc_hw->bandstate[1]->pi, name, b);
	}

	ASSERT(wlc_bmac_taclear(wlc_hw, ta_ok) || !ta_ok);
	BCM_REFERENCE(ta_ok);
	return ret;
}
#endif // endif

/** register bmac/si dump names */
static int
wlc_bmac_register_dumps(wlc_hw_info_t *wlc_hw)
{
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_PHYDUMP)
	BCM_REFERENCE(wlc_bmac_add_dump_fn);
#endif // endif

#if defined(BCMDBG)
	wlc_bmac_add_dump_fn(wlc_hw, "btc", (bmac_dump_fn_t)wlc_bmac_btc_dump, wlc_hw);
	wlc_bmac_add_dump_fn(wlc_hw, "bmc", (bmac_dump_fn_t)wlc_bmac_bmc_dump, wlc_hw);
#if defined(WL_TXS_LOG)
	wlc_bmac_add_dump_fn(wlc_hw, "txs_hist", (bmac_dump_fn_t)wlc_txs_hist_dump, wlc_hw);
#endif // endif
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
	wlc_bmac_add_dump_fn(wlc_hw, "macsuspend", (bmac_dump_fn_t)wlc_bmac_suspend_dump, wlc_hw);
#endif // endif

#if !defined(DONGLEBUILD) && (defined(BCMDBG) || defined(WLTEST))
	wlc_bmac_add_dump_fn(wlc_hw, "pcieinfo", (bmac_dump_fn_t)si_dump_pcieinfo, wlc_hw->sih);
	wlc_bmac_add_dump_fn(wlc_hw, "pmuregs", (bmac_dump_fn_t)si_dump_pmuregs, wlc_hw->sih);
#endif // endif

#if defined(BCMHWA) && defined(BCMDBG)
	if (hwa_dev) {
		wlc_bmac_add_dump_fn(wlc_hw, "hwa", (bmac_dump_fn_t)hwa_wl_dump, (void *)hwa_dev);
	}
#endif // endif
#if (defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)) && defined(BCMDBG)
	if (D11REV_GE(wlc_hw->corerev, 130)) {
		wlc_bmac_add_dump_fn(wlc_hw, "offld", (bmac_dump_fn_t)wlc_offload_wl_dump,
			(void *)wlc_hw->wlc->offl);
	}
#endif // endif

	return BCME_OK;
}

int
wlc_bmac_dump(wlc_hw_info_t *wlc_hw, const char *name, struct bcmstrbuf *b)
{
	int ret = BCME_NOTFOUND;

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP) || defined(WLTEST)
	/* dump if 'name' is a bmac/si dump */
	if (wlc_hw->dump != NULL) {
		ret = wlc_dump_reg_invoke_dump_fn(wlc_hw->dump, name, b);
		if (ret == BCME_OK)
			return ret;
	}
	/* dump if 'name' is a phy dump */
	if (ret == BCME_NOTFOUND) {
		ret = wlc_bmac_dump_phy(wlc_hw, name, b);
	}
#endif // endif

	return ret;
}

int
wlc_bmac_dump_clr(wlc_hw_info_t *wlc_hw, const char *name)
{
	int ret = BCME_NOTFOUND;

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
	/* dump if 'name' is a bmac/si dump */
	if (wlc_hw->dump != NULL) {
		ret = wlc_dump_reg_invoke_clr_fn(wlc_hw->dump, name);
		if (ret == BCME_OK)
			return ret;
	}

	/* dump if 'name' is a phy dump */
	if (ret == BCME_NOTFOUND) {
		ret = phy_dbg_dump_clr((phy_info_t *)wlc_hw->band->pi, name);
	}
#endif // endif

	return ret;
}

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && defined(WLTEST)
#ifdef BCMNVRAMW
int
wlc_bmac_ciswrite(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len)
{
	int err = 0;

	WL_TRACE(("%s\n", __FUNCTION__));

	if (len < (int)cis->nbytes)
		return BCME_BUFTOOSHORT;

	switch (si_cis_source(wlc_hw->sih)) {
	case CIS_OTP: {
		uint32 macintmask;
		int region;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
			BCM43602_CHIP(wlc_hw->sih->chip) ||
			(CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
			(BCM4350_CHIP(wlc_hw->sih->chip) &&
			(wlc_hw->sih->chipst &
			(CST4350_HSIC20D_MODE | CST4350_USB20D_MODE | CST4350_USB30D_MODE))) ||
			0)
			region = OTP_SW_RGN;
		else
			region = OTP_HW_RGN;

		err = otp_write_region(wlc_hw->sih, region, tbuf, cis->nbytes / 2,
			cis->flags);

		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;

	}

	case CIS_SROM: {

		if (srom_write(wlc_hw->sih, wlc_hw->sih->bustype,
			(void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
			cis->byteoff, cis->nbytes, tbuf))
			err = BCME_ERROR;
		break;
	}

	case CIS_DEFAULT:
	default:
		err = BCME_NOTFOUND;
		break;
	}

	return err;
} /* wlc_bmac_ciswrite */
#endif /* def BCMNVRAMW */
#endif // endif

#if defined(BCM_CISDUMP_NO_RECLAIM) || (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && \
	defined(WLTEST)
int
wlc_bmac_cisdump(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len)
{
	int err = 0;
	uint32 macintmask;

	WL_TRACE(("%s\n", __FUNCTION__));

	macintmask = wl_intrsoff(wlc_hw->wlc->wl);
	cis->source = WLC_CIS_OTP;
	cis->byteoff = 0;

	switch (si_cis_source(wlc_hw->sih)) {

	case CIS_SROM: {

		cis->source = WLC_CIS_SROM;
		cis->byteoff = 0;
		cis->nbytes = cis->nbytes ? ROUNDUP(cis->nbytes, 2) : SROM_MAX;
		if (len < (int)cis->nbytes) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (srom_read(wlc_hw->sih, wlc_hw->sih->bustype,
			(void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
			0, cis->nbytes, tbuf, FALSE)) {
			err = BCME_ERROR;
		}

		break;
	}

	case CIS_OTP: {
		int region;

		cis->nbytes = len;
		if ((CHIPID(wlc_hw->sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM43460_CHIP_ID) ||
			BCM43602_CHIP(wlc_hw->sih->chip) ||
			(CHIPID(wlc_hw->sih->chip) == BCM4352_CHIP_ID) ||
			(CHIPID(wlc_hw->sih->chip) == BCM43526_CHIP_ID) ||
			(BCM4350_CHIP(wlc_hw->sih->chip) &&
			(wlc_hw->sih->chipst &
			(CST4350_HSIC20D_MODE | CST4350_USB20D_MODE | CST4350_USB30D_MODE))) ||
			0)
			region = OTP_SW_RGN;
		else
			region = OTP_HW_RGN;

		err = otp_read_region(wlc_hw->sih, region, tbuf, &cis->nbytes);
		cis->nbytes *= 2;

		/* Not programmed is ok */
		if (err == BCME_NOTFOUND)
			err = 0;

		break;
	}

	case CIS_DEFAULT:
	case BCME_NOTFOUND:
	default:
		err = BCME_NOTFOUND;
		cis->source = 0;
		cis->byteoff = 0;
		cis->nbytes = 0;
		break;
	}

	wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

	return err;
} /* wlc_bmac_cisdump */
#endif // endif

#if (defined(WLTEST) || defined(WLPKTENG))

static void
wlc_bmac_long_pkt(wlc_hw_info_t *wlc_hw, bool set)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint16 longpkt_shm;

	longpkt_shm = wlc_bmac_read_shm(wlc_hw, M_MAXRXFRM_LEN(wlc));

	if (wlc->pkteng_maxlen == WL_PKTENG_MAXPKTSZ) {
		if (longpkt_shm < WL_PKTENG_LONGPKTSZ) {
			wlc_bmac_write_shm(wlc_hw, M_MAXRXFRM_LEN(wlc),
				(uint16)WL_PKTENG_LONGPKTSZ);
			if (D11REV_GE(wlc->pub->corerev, 128)) {
				wlc_bmac_write_shm(wlc_hw, M_MAXRXMPDU_LEN(wlc),
					(uint16)WL_PKTENG_LONGPKTSZ);
			}
		}
		return; /* longpkt 1 */
	}

	if (!set && (longpkt_shm == WL_PKTENG_LONGPKTSZ)) {
		wlc_bmac_write_shm(wlc_hw, M_MAXRXFRM_LEN(wlc), wlc->longpkt_shm);
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			wlc_bmac_write_shm(wlc_hw, M_MAXRXMPDU_LEN(wlc),
				(uint16)VHT_MPDU_LIMIT_8K);
		}
	} else if (set && (longpkt_shm != WL_PKTENG_LONGPKTSZ)) {
		/* save current values */
		wlc->longpkt_shm = longpkt_shm;
		wlc_bmac_write_shm(wlc_hw, M_MAXRXFRM_LEN(wlc), (uint16)WL_PKTENG_LONGPKTSZ);
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			wlc_bmac_write_shm(wlc_hw, M_MAXRXMPDU_LEN(wlc),
				(uint16)WL_PKTENG_LONGPKTSZ);
		}
	}
}

/** one-shot timer for packet engine running in async tx mode */
static void
wlc_bmac_pkteng_timer_cb(void *arg)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)arg;
	wlc_info_t *wlc = wlc_hw->wlc;
	wlc_phy_t *pi = wlc_hw->band->pi;
	int i, service_txstatus_delay;

	i = wlc_bmac_read_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw));

	if (!(i & MFGTEST_TXMODE)) {
#ifdef PKTENG_TXREQ_CACHE
		if (wlc_bmac_pkteng_check_cache(wlc_hw))
			return;
#endif /* PKTENG_TXREQ_CACHE */

		/* implicit tx stop after synchronous transmit */
		wlc_phy_clear_deaf(pi, (bool)1);
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, FALSE);

		/*
		* service txstatus from ucode synchronously
		* to make sure FIFO has no packets when pkteng
		* running status is made IDLE. Small delay is sufficient.
		*/
		service_txstatus_delay = 0;
		do {
			OSL_DELAY(10);
			service_txstatus_delay += 10;
			wlc_bmac_service_txstatus(wlc_hw);
		} while (TXPKTPENDTOT(wlc_hw->wlc) &&
			service_txstatus_delay < PKTENG_TIMEOUT);

		/* assert if pkts pending */
		ASSERT((TXPKTPENDTOT(wlc) == 0));

		WL_INFORM(("%s DELETE pkteng timer\n", __FUNCTION__));
		wlc_hw->pkteng_status = PKTENG_IDLE;
		if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
			/* restore link_entry content */
			wlc_ratelinkmem_update_link_entry(wlc, WLC_RLM_SPECIAL_LINK_SCB(wlc));
		}
		wl_del_timer(wlc->wl, wlc_hw->pkteng_timer);
	}
}

#ifdef WLCNT
void
wlc_bmac_pkteng_poll_interval_set(wlc_hw_info_t *wlc_hw, uint32 timer_cb_dur)
{
	wlc_hw->pkteng_async->timer_cb_dur = timer_cb_dur;
}

void
wlc_bmac_pkteng_poll_interval_reset(wlc_hw_info_t *wlc_hw)
{
	wlc_hw->pkteng_async->timer_cb_dur = TIMER_INTERVAL_PKTENG;
}
#endif // endif

static void
wlc_bmac_pkteng_timer_async_rx_cb(void *arg)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)arg;
	struct wl_pkteng_async *pkteng_async = wlc_hw->pkteng_async;
	uint32 cmd = pkteng_async->flags & WL_PKTENG_PER_MASK;
	uint32 frame_cnt, delta;

	/* Incrementing delay in ms */
	pkteng_async->delay_cnt += wlc_hw->pkteng_async->timer_cb_dur;

	/* Updating the counters */
	wlc_ctrupd(wlc_hw->wlc, MCSTOFF_RXGOODUCAST);

	frame_cnt = WLCNTVAL(MCSTVAR(wlc_hw->wlc->pub, pktengrxducast));
	delta = frame_cnt - wlc_hw->pkteng_async->ucast_initial_val;

	switch (cmd) {
		case WL_PKTENG_PER_RX_START:
		case WL_PKTENG_PER_RX_WITH_ACK_START:
			if ((pkteng_async->delay_cnt < pkteng_async->rx_delay) &&
				(delta < pkteng_async->pkt_count)) {
					return;
			}
			break;
		default:
			printf("BCME Unsupported\n");
			return;
	}

	wlc_statsupd(wlc_hw->wlc);
	WL_INFORM(("%s DELETE pkteng timer\n", __FUNCTION__));
	/* implicit rx stop after synchronous receive */
	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw), 0);
	wlc_bmac_set_match_mac(wlc_hw, &wlc_hw->etheraddr);
	wlc_hw->pkteng_status = PKTENG_IDLE;
	wl_del_timer(wlc_hw->wlc->wl, wlc_hw->pkteng_async->pkteng_async_timer);

}

static void
wlc_bmac_pkteng_handle_rx(wlc_hw_info_t *wlc_hw, wlc_dpc_info_t *dpc)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint32 macintstatus;
	uint32 fifo0_int;

	/* read macintstatus */
	macintstatus = GET_MACINTSTATUS(wlc->osh, wlc_hw);

	if (macintstatus & MI_DMAINT) {

		/* clear dma interrupt */
		SET_MACINTSTATUS(wlc->osh, wlc_hw, MI_DMAINT);

		fifo0_int = (R_REG(wlc->osh, &(D11Reggrp_intctrlregs(wlc_hw, RX_FIFO)->intstatus)) &
			DEF_RXINTMASK);

		if (fifo0_int) {
			W_REG(wlc->osh, &(D11Reggrp_intctrlregs(wlc_hw, RX_FIFO)->intstatus),
				DEF_RXINTMASK);
			wlc_bmac_recv(wlc_hw, RX_FIFO, TRUE, dpc);
		}
	}
}

/**
 * Receives frames generated by a packet engine.
 *
 * @param cmd    WL_PKTENG_PER_RX_START or WL_PKTENG_PER_RX_WITH_ACK_START
 */
static int
wlc_bmac_pkteng_rx_start(wlc_hw_info_t *wlc_hw, wl_pkteng_t *pkteng, wlc_phy_t *pi, uint32 cmd,
	bool is_sync, bool is_sync_unblk)
{
#if defined(WLCNT)
	uint32 pktengrxducast_start = 0;
	wlc_pub_t	*pub = wlc_hw->wlc->pub;
#endif /* WLCNT */
	uint16 pkteng_mode;

	/* Return If pkteng already running */
	if (wlc_hw->pkteng_status != PKTENG_IDLE) {
		return BCME_BUSY;
	}

	wlc_bmac_long_pkt(wlc_hw, TRUE);

	/* Reset the counters */
	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_LO(wlc_hw), 0);
	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_HI(wlc_hw), 0);
	wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, TRUE);
	if (!is_sync && !is_sync_unblk) {
		wlc_bmac_mhf(wlc_hw, MHF3, MHF3_PKTENG_PROMISC,
			MHF3_PKTENG_PROMISC, WLC_BAND_ALL);
	}

	if (is_sync) {
#if defined(WLCNT)
		/* get counter value before start of pkt engine */
		wlc_ctrupd(wlc_hw->wlc, MCSTOFF_RXGOODUCAST);
		pktengrxducast_start = WLCNTVAL(MCSTVAR(pub, pktengrxducast));
#else
		/* BMAC_NOTE: need to split wlc_ctrupd before supporting this in bmac */
		ASSERT(0);
#endif /* WLCNT */
	} else {
		wlc_hw->pkteng_status = PKTENG_RX_BUSY; /* Indicate Pkteng RX is busy */
	}

	pkteng_mode = (cmd == WL_PKTENG_PER_RX_START) ?
		MFGTEST_RXMODE : MFGTEST_RXMODE_ACK;

	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw), pkteng_mode);

	/* This is to enable averaging of RSSI value in the ucode
	  * and initialize the results to zero
	  */
	if (D11REV_IS(wlc_hw->corerev, 48) || D11REV_IS(wlc_hw->corerev, 49) ||
		D11REV_IS(wlc_hw->corerev, 61)) {
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_RXAVGPWR_ANT0(wlc_hw), 0);
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_RXAVGPWR_ANT1(wlc_hw), 0);
		if (D11REV_IS(wlc_hw->corerev, 49))
			wlc_bmac_write_shm(wlc_hw, M_MFGTEST_RXAVGPWR_ANT2(wlc_hw), 0);
	}

	/* set RA match reg with dest addr */
	wlc_bmac_set_match_mac(wlc_hw, &pkteng->dest);

#if defined(WLCNT)
	/* wait for counter for synchronous receive with a maximum total delay */
	if (is_sync) {
		/* loop delay in msec */
		uint32 delay_msec = 1;
		/* avoid calculation in loop */
		uint32 delay_usec = delay_msec * 1000;
		uint32 total_delay = 0;
		uint32 delta;
		wlc_hw->pkteng_status = PKTENG_RX_BUSY;
		do {
			OSL_DELAY(delay_usec);
			total_delay += delay_msec;
			wlc_ctrupd(wlc_hw->wlc, MCSTOFF_RXGOODUCAST);
			if (WLCNTVAL(MCSTVAR(pub, pktengrxducast))
				> pktengrxducast_start) {
				delta = WLCNTVAL(MCSTVAR(pub, pktengrxducast)) -
					pktengrxducast_start;
			} else {
				/* counter overflow */
				delta = (~pktengrxducast_start + 1) +
					WLCNTVAL(MCSTVAR(pub, pktengrxducast));
			}
		} while (delta < pkteng->nframes && total_delay < pkteng->delay);

		wlc_hw->pkteng_status = PKTENG_IDLE;
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, FALSE);
		/* implicit rx stop after synchronous receive */
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw), 0);
		wlc_bmac_set_match_mac(wlc_hw, &wlc_hw->etheraddr);
	} else if (is_sync_unblk) {
		/* get counter value before start of pkt engine */
		wlc_ctrupd(wlc_hw->wlc, MCSTOFF_RXGOODUCAST);
		wlc_hw->pkteng_async->ucast_initial_val = WLCNTVAL(MCSTVAR(pub,
		pktengrxducast));
		wlc_hw->pkteng_async->rx_delay = pkteng->delay;
		wlc_hw->pkteng_async->delay_cnt = 0;
		wlc_hw->pkteng_async->flags = cmd;
		wlc_hw->pkteng_async->pkt_count = pkteng->nframes;
		wlc_hw->pkteng_status = PKTENG_RX_BUSY;
		wl_add_timer(wlc_hw->wlc->wl, wlc_hw->pkteng_async->pkteng_async_timer,
		wlc_hw->pkteng_async->timer_cb_dur, TRUE);
	}
#endif /* WLCNT */

	return BCME_OK;
} /* wlc_bmac_pkteng_rx_start */

static int
wlc_bmac_pkteng_rx_stop(wlc_hw_info_t *wlc_hw, wlc_phy_t *pi)
{
	if (wlc_hw->pkteng_status == PKTENG_IDLE || wlc_hw->pkteng_status == PKTENG_RX_BUSY) {
		/* PKTENG TX not running, allow RX stop */
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw), 0);
		wlc_bmac_mhf(wlc_hw, MHF3, MHF3_PKTENG_PROMISC,
			0, WLC_BAND_ALL);
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, FALSE);
		/* Restore match address register */
		wlc_bmac_set_match_mac(wlc_hw, &wlc_hw->etheraddr);
		wlc_bmac_long_pkt(wlc_hw, FALSE);
		wlc_hw->pkteng_status = PKTENG_IDLE;
		wl_del_timer(wlc_hw->wlc->wl, wlc_hw->pkteng_async->pkteng_async_timer);
	} else {
		/* handle pkteng stop in WL_PKTENG_PER_TX_STOP */
		/* PKTENG TX is running. Pkteng_stop rx should not have any effect. */
	}

	return BCME_OK;
}

/**
 * Transmits frames generated by a packet engine.
 *
 * @param cmd      WL_PKTENG_PER_TX_START or WL_PKTENG_PER_TX_WITH_ACK_START or ...
 * @param is_sync  If TRUE, waits for ucode tx status on all packets before this function returns.
 * @param is_sync_unblk  If TRUE, queues all packets to the d11 core but does not wait for tx
 *                       completion. transmits packets after this function returns using a timer.
 */
static int
wlc_bmac_pkteng_tx_start(wlc_hw_info_t *wlc_hw, wl_pkteng_t *pkteng, wlc_phy_t *pi, uint32 cmd,
	bool is_sync, bool is_sync_unblk, void *p[], int npkts)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint16 val = MFGTEST_TXMODE;
	struct wl_pkteng_cache *pkteng_cache = wlc_hw->pkteng_cache;
	int i = 0;

	/* check if we need to transmit  RU packet */
	if (cmd == WL_PKTENG_PER_RU_TX_START) {
		val = MFGTEST_RU_TXMODE;
	}

	if (wlc_hw->pkteng_status != PKTENG_IDLE) {
		return BCME_BUSY; /* Return if pkteng already running */
	}

	UNUSED_PARAMETER(pkteng_cache);
	WL_ERROR(("Pkteng TX Start Called\n"));
	WL_INFORM(("%s: wlc_hw = 0x%p reg: 0x%p\n", __FUNCTION__,
		OSL_OBFUSCATE_BUF(wlc_hw), OSL_OBFUSCATE_BUF(wlc_hw->regs)));

	if (cmd == WL_MUPKTENG_PER_TX_START)
		ASSERT(p == NULL);
	else
		ASSERT(p != NULL);

	if ((pkteng->delay < 15) || (pkteng->delay > 1000)) {
		WL_ERROR(("delay out of range, freeing the packet\n"));
		if (cmd != WL_MUPKTENG_PER_TX_START) {
			for (i = 0; i < npkts; i++) {
				ASSERT(p[i] != NULL);
				PKTFREE(wlc_hw->osh, p[i], TRUE);
			}
		}
		return BCME_RANGE;
	}

#ifdef PKTENG_TXREQ_CACHE
	if (pkteng_cache->status == PKTENG_CACHE_RUNNING) {
		if ((pkteng_cache->write_idx - pkteng_cache->read_idx) %
			PKTENG_CACHE_SIZE >= (PKTENG_CACHE_SIZE - 1)) {
			/* Free the Old cache entry */
			PKTFREE(wlc_hw->osh,
				pkteng_cache->pkt_qry[pkteng_cache->read_idx].p, TRUE);
			pkteng_cache->read_idx++;
			if (pkteng_cache->read_idx == PKTENG_CACHE_SIZE) {
				pkteng_cache->read_idx = 0;
			}
			WL_ERROR(("Exceeded maximum pkteng cache limit\n"));
		}
		struct pkteng_querry *pkteng_qry =
			&(pkteng_cache->pkt_qry[pkteng_cache->write_idx]);
		WL_INFORM(("%s Caching pkteng TX request for %d pacekts\n",
			__FUNCTION__, pkteng->nframes));

		pkteng_qry->p = p;
		pkteng_qry->nframes = pkteng->nframes;
		pkteng_qry->delay = pkteng->delay;
		pkteng_qry->flags = pkteng->flags;
		pkteng_qry->length = pkteng->length;

		pkteng_cache->write_idx++;
		if (pkteng_cache->write_idx == PKTENG_CACHE_SIZE) {
			pkteng_cache->write_idx = 0;
		}

		break;
	}

	if (!is_sync) {
		pkteng_cache->status = PKTENG_CACHE_RUNNING;
	}

#endif /* PKTENG_TXREQ_CACHE */

	if (wlc_hw->pkteng_status != PKTENG_IDLE) {
		/* pkteng TX running already */
		return BCME_BUSY;
	}

	wlc_hw->pkteng_status = PKTENG_TX_BUSY;

	/* assert if pkts pending */
	ASSERT((TXPKTPENDTOT(wlc) == 0));

	wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, TRUE);
	wlc_bmac_suspend_mac_and_wait(wlc_hw);

	if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
		/* update link entry for PKTENG content */
		wlc_ratelinkmem_update_link_entry(wlc, WLC_RLM_SPECIAL_LINK_SCB(wlc));
	}

	/*
	 * mute the rx side for the regular TX.
	 * tx_with_ack mode makes the ucode update rxdfrmucastmbss count
	 */
	if ((cmd == WL_PKTENG_PER_TX_START) || (cmd == WL_MUPKTENG_PER_TX_START) ||
		(cmd == WL_PKTENG_PER_RU_TX_START)) {
		wlc_phy_set_deaf(pi, TRUE);
	} else {
		wlc_phy_clear_deaf(pi, TRUE);
	}

	/* set nframes */
	if (pkteng->nframes) {
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_LO(wlc_hw),
			(pkteng->nframes & 0xffff));
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_HI(wlc_hw),
			((pkteng->nframes>>16) & 0xffff));
		val = (cmd == WL_PKTENG_PER_RU_TX_START) ?
			MFGTEST_RU_TXMODE_FRMCNT : MFGTEST_TXMODE_FRMCNT;
	}

	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw), val);

	/* we write to M_MFGTEST_IFS the IFS required in 1/8us factor */
	/* 10 : for factoring difference b/w Tx.crs and energy in air */
	/* 44 : amount of time spent after TX_RRSP to frame start */
	/* IFS */
	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_IO1(wlc_hw), (pkteng->delay - 10)*8 - 44);
	if (is_sync) {
		wlc_bmac_mctrl(wlc_hw, MCTL_DISCARD_TXSTATUS, 1 << 29);
	}

	wlc_bmac_enable_mac(wlc_hw);

	/* Do the low part of wlc_txfifo() */
	for (i = 0; i < npkts; i++) {
		ASSERT(p[i] != NULL);
		wlc_bmac_txfifo(wlc_hw, TX_DATA_FIFO, p[i], TRUE, INVALIDFID, 1);
	}

	/* wait for counter for synchronous transmit */
	/* use timer to check Tx complete status */
	if (is_sync) {
		wlc_info_t *other_wlc = NULL;
		wlc_dpc_info_t dpc = {0};
#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub)) {
			other_wlc = wlc_rsdb_get_other_wlc(wlc);
		}
#endif /* WLRSDB */
		do {
			OSL_DELAY(1000);

			/* handle rx on other slice */
			if (other_wlc && other_wlc->hw->pkteng_status == PKTENG_RX_BUSY) {
				wlc_bmac_pkteng_handle_rx(other_wlc->hw, &dpc);
			}

			i = wlc_bmac_read_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw));
		} while (i & MFGTEST_TXMODE);

		/* implicit tx stop after synchronous transmit */
		wlc_phy_clear_deaf(pi, (bool)1);
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, FALSE);
		for (i = 0; i < npkts; i++) {
			p[i] = wlc_bmac_dma_getnexttxp(wlc, TX_DATA_FIFO,
				HNDDMA_RANGE_TRANSMITTED);
			ASSERT(p[i] != NULL);
			PKTFREE(wlc_hw->osh, p[i], TRUE);
			/* Decrementing txpktpend with '1' since wlc_bmac_txfifo
			 * is also called with txpktpend = 1
			 */
			TXPKTPENDDEC(wlc, TX_DATA_FIFO, 1);
		}
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
		wlc_bmac_mctrl(wlc_hw, MCTL_DISCARD_TXSTATUS, 0);
		wlc_hw->pkteng_status = PKTENG_IDLE;
		if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
			/* restore link_entry content */
			wlc_ratelinkmem_update_link_entry(wlc, WLC_RLM_SPECIAL_LINK_SCB(wlc));
		}
		wlc_bmac_enable_mac(wlc_hw);
	} else if (is_sync_unblk) {
		/** returns to non-pkteng mode by one-shot timerfn wlc_bmac_pkteng_timer_cb() */
		wl_add_timer(wlc->wl, wlc_hw->pkteng_timer,
			wlc_hw->pkteng_async->timer_cb_dur, TRUE);
		wlc_hw->pkteng_status = PKTENG_TX_BUSY;
	}

	return BCME_OK;
} /* wlc_bmac_pkteng_tx_start */

static int
wlc_bmac_pkteng_tx_stop(wlc_hw_info_t *wlc_hw, wlc_phy_t *pi, void *p[])
{
	wlc_info_t *wlc = wlc_hw->wlc;
	int status;
	int service_txstatus_delay;

	if (wlc_hw->pkteng_status == PKTENG_IDLE || wlc_hw->pkteng_status == PKTENG_TX_BUSY) {
		ASSERT(p == NULL);

		WL_INFORM(("Pkteng TX Stop Called\n"));
#ifdef ATE_BUILD
		printf("===> Pkteng TX Stop Called\n");
#endif /* ATE_BUILD */

		/* Check pkteng state */
		status = wlc_bmac_read_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw));
		if (status & MFGTEST_TXMODE) {
			/* Still running
			 * Stop cleanly by setting frame count
			 */
			wlc_bmac_suspend_mac_and_wait(wlc_hw);
			wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_LO(wlc_hw), 1);
			wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_HI(wlc_hw), 0);
			wlc_bmac_write_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw),
				MFGTEST_TXMODE_FRMCNT);
			wlc_bmac_enable_mac(wlc_hw);

			/* Wait for the pkteng to stop */
			do {
				OSL_DELAY(1000);
				status = wlc_bmac_read_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw));
#ifdef ATE_BUILD
				printf("===> Waiting for Packet Engine to Stop Tx....\n");
#endif /* ATE_BUILD */
			} while (status & MFGTEST_TXMODE);
		}

#ifdef ATE_BUILD
		printf("===> Pkteng TX Stopped Gracefully \n");
#endif /* ATE_BUILD */

		/* Clean up */
		wlc_phy_clear_deaf(pi, (bool)1);
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, FALSE);

		/*
		* service txstatus from ucode synchronously
		* to make sure FIFO has no packets when pkteng
		* running status is made IDLE. Small delay is sufficient.
		*/
		service_txstatus_delay = 0;
		do {
			OSL_DELAY(10);
			service_txstatus_delay += 10;
			wlc_bmac_service_txstatus(wlc_hw);
		} while (TXPKTPENDTOT(wlc) && service_txstatus_delay < PKTENG_TIMEOUT);

		/* assert if pkts pending */
		ASSERT((TXPKTPENDTOT(wlc) == 0));

#ifdef PKTENG_TXREQ_CACHE
		if (wlc_hw->pkteng_cache->status) {
			int idx = 0;
			int rd_idx = wlc_hw->pkteng_cache->read_idx;
			int wr_idx = wlc_hw->pkteng_cache->write_idx;
			for (idx = rd_idx; ; idx++) {
				if (idx == PKTENG_CACHE_SIZE)
					idx = 0;
				if (idx == wr_idx)
					break;
				PKTFREE(wlc_hw->osh,
					wlc_hw->pkteng_cache->pkt_qry[idx].p, FALSE);
			}
		}
		memset(wlc_hw->pkteng_cache, 0, sizeof(struct wl_pkteng_cache));
#endif /* PKTENG_TXREQ_CACHE */

		/* Pkteng TX stopped. Set Idle */
		wlc_hw->pkteng_status = PKTENG_IDLE;
		wl_del_timer(wlc->wl, wlc_hw->pkteng_timer);

		if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
			/* restore link_entry content */
			wlc_ratelinkmem_update_link_entry(wlc, WLC_RLM_SPECIAL_LINK_SCB(wlc));
		}
	} else {
		/* PKTENG Rx is running. pkteng_stop tx should have no effect. */
	}

	return BCME_OK;
} /* wlc_bmac_pkteng_tx_stop */

/** transmits or receives caller supplied test frames, invoked via an ioctl */
int
wlc_bmac_pkteng(wlc_hw_info_t *wlc_hw, wl_pkteng_t *pkteng, void* p[], int npkts)
{
	wlc_phy_t *pi = wlc_hw->band->pi;
	uint32 cmd;
	bool is_sync, is_sync_unblk;
	uint err;

#if defined(WLP2P_UCODE)
	if (DL_P2P_UC(wlc_hw) && (CHIPID(wlc_hw->sih->chip) != BCM4360_CHIP_ID) &&
		!BCM43602_CHIP(wlc_hw->sih->chip) &&
		!BCM53573_CHIP(wlc_hw->sih->chip) &&
		(!BCM4347_CHIP(wlc_hw->sih->chip)) &&
		(!BCM6878_CHIP(wlc_hw->sih->chip)) &&
		!BCM4350_CHIP(wlc_hw->sih->chip) &&
		!BCM4365_CHIP(wlc_hw->sih->chip) &&
		!BCM43684_CHIP(wlc_hw->sih->chip) &&
		!EMBEDDED_2x2AX_CORE(wlc_hw->sih->chip) &&
		(!BCM4369_CHIP(wlc_hw->sih->chip)) &&
		(!BCM4367_CHIP(wlc_hw->sih->chip)) &&
		1) {
		int i = 0;

		WL_ERROR(("p2p-ucode does not support pkteng\n"));
		for (i = 0; i < npkts; i++) {
			if (p[i]) {
				PKTFREE(wlc_hw->osh, p[i], TRUE);
			}
		}
		return BCME_UNSUPPORTED;
	}
#endif /* WLP2P_UCODE */

	cmd = pkteng->flags & WL_PKTENG_PER_MASK;
	is_sync = (pkteng->flags & WL_PKTENG_SYNCHRONOUS) ? TRUE : FALSE;
	is_sync_unblk = (pkteng->flags & WL_PKTENG_SYNCHRONOUS_UNBLK) ? TRUE : FALSE;

	switch (cmd) {
	case WL_PKTENG_PER_RX_START:
	case WL_PKTENG_PER_RX_WITH_ACK_START:
		err = wlc_bmac_pkteng_rx_start(wlc_hw, pkteng, pi, cmd, is_sync, is_sync_unblk);
		break;

	case WL_PKTENG_PER_RX_STOP:
		err = wlc_bmac_pkteng_rx_stop(wlc_hw, pi);
		break;

	case WL_PKTENG_PER_TX_START:
	case WL_PKTENG_PER_TX_WITH_ACK_START:
	case WL_PKTENG_PER_RU_TX_START:
	case WL_MUPKTENG_PER_TX_START:
		err = wlc_bmac_pkteng_tx_start(wlc_hw, pkteng, pi, cmd, is_sync, is_sync_unblk,
			p, npkts);
		break;

	case WL_PKTENG_PER_TX_STOP:
	case WL_MUPKTENG_PER_TX_STOP:
		err = wlc_bmac_pkteng_tx_stop(wlc_hw, pi, p);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_bmac_pkteng */

#ifdef PKTENG_TXREQ_CACHE
static bool
wlc_bmac_pkteng_check_cache(wlc_hw_info_t *wlc_hw)
{
	struct wl_pkteng_cache *pkteng_cache = wlc_hw->pkteng_cache;
	wlc_phy_t *pi = wlc_hw->band->pi;
	int i = wlc_bmac_read_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw));

	if (pkteng_cache->pkteng_running)
		return FALSE;

	if (!(i & MFGTEST_TXMODE) &&
		(pkteng_cache->status == PKTENG_CACHE_RUNNING)) {
		pkteng_cache->pkteng_running = TRUE;
		wlc_phy_clear_deaf(pi, (bool)1);
		wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, FALSE);
		wlc_bmac_service_txstatus(wlc_hw);
		wlc_hw->pkteng_status = PKTENG_IDLE;
		wl_del_timer(wlc_hw->wlc->wl, wlc_hw->pkteng_timer);
		if (pkteng_cache->read_idx != pkteng_cache->write_idx) {
			struct pkteng_querry *pkteng_qry =
				&(pkteng_cache->pkt_qry[pkteng_cache->read_idx]);
			wlc_bmac_pkteng_cache(wlc_hw, pkteng_qry->flags,
				pkteng_qry->nframes, pkteng_qry->delay, pkteng_qry->p);
			pkteng_cache->read_idx++;
			if (pkteng_cache->read_idx == PKTENG_CACHE_SIZE)
				pkteng_cache->read_idx = 0;
		} else
			pkteng_cache->status = PKTENG_CACHE_IDLE;
		pkteng_cache->pkteng_running = FALSE;
		return TRUE;
	}
	return FALSE;
}

static int
wlc_bmac_pkteng_cache(wlc_hw_info_t *wlc_hw, uint32 flags,
	uint32 nframes, uint32 delay, void* p)
{
	wlc_phy_t *pi = wlc_hw->band->pi;
	uint32 cmd = flags & WL_PKTENG_PER_MASK;
	uint16 val = MFGTEST_TXMODE;

	wlc_phy_hold_upd(pi, PHY_HOLD_FOR_PKT_ENG, TRUE);
	wlc_bmac_suspend_mac_and_wait(wlc_hw);

	/*
	 * mute the rx side for the regular TX.
	 * tx_with_ack mode makes the ucode update rxdfrmucastmbss count
	 */
	if (cmd == WL_PKTENG_PER_TX_START) {
		wlc_phy_set_deaf(pi, TRUE);
	} else {
		wlc_phy_clear_deaf(pi, TRUE);
	}

	/* set nframes */
	if (nframes) {
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_LO(wlc_hw),
			(nframes & 0xffff));
		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_FRMCNT_HI(wlc_hw),
			((nframes>>16) & 0xffff));
		val = MFGTEST_TXMODE_FRMCNT;
	}

	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_NUM(wlc_hw), val);
	wlc_bmac_write_shm(wlc_hw, M_MFGTEST_IO1(wlc_hw), (delay - 10)*8 - 44);
	wlc_bmac_enable_mac(wlc_hw);

	/* Do the low part of wlc_txfifo() */
#ifndef DONGLEBUILD
	if (wlc_bmac_txfifo(wlc_hw, TX_DATA_FIFO, p, TRUE, INVALIDFID, 1) != BCME_OK) {
		WL_ERROR(("wlc_bmac_txfifo: DMA resources are not available:"));
		return BCME_NORESOURCE;
	}
#else
	wlc_bmac_txfifo(wlc_hw, TX_DATA_FIFO, p, TRUE, INVALIDFID, 1);
#endif /* DONGLEBUILD */

	wl_add_timer(wlc_hw->wlc->wl, wlc_hw->pkteng_timer,
		TIMER_INTERVAL_PKTENG, TRUE);
	wlc_hw->pkteng_status = BCME_BUSY;
	return BCME_OK;
}
#endif /* PKTENG_TXREQ_CACHE */
#endif // endif

/**
 * Lower down relevant GPIOs like LED/BTC when going down w/o
 * doing PCI config cycles or touching interrupts
 */
void
wlc_gpio_fast_deinit(wlc_hw_info_t *wlc_hw)
{
	if ((wlc_hw == NULL) || (wlc_hw->sih == NULL))
		return;

	/* Only chips with internal bus or PCIE cores or certain PCI cores
	 * are able to switch cores w/o disabling interrupts
	 */
	if (!((BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) ||
	      ((BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) &&
	       ((BUSCORETYPE(wlc_hw->sih->buscoretype) == PCIE_CORE_ID) ||
	        (BUSCORETYPE(wlc_hw->sih->buscoretype) == PCIE2_CORE_ID) ||
	        (wlc_hw->sih->buscorerev >= 13))))) {
		return;
	}

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

#ifdef WLLED
	if (wlc_hw->wlc->ledh)
		wlc_led_deinit(wlc_hw->wlc->ledh);
#endif // endif

	wlc_bmac_btc_gpio_disable(wlc_hw);

	return;
}

bool
wlc_bmac_radio_hw(wlc_hw_info_t *wlc_hw, bool enable, bool skip_anacore)
{
	/* Do not access Phy registers if core is not up */
	if (si_iscoreup(wlc_hw->sih) == FALSE)
		return FALSE;

	if (enable) {
		if (PMUCTL_ENAB(wlc_hw->sih)) {
			AND_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw), ~CCS_FORCEHWREQOFF);
		}

		/* need to skip for 5356 in case of radio_pwrsave feature. */
		if (!skip_anacore)
			phy_ana_switch((phy_info_t *)wlc_hw->band->pi, ON);
		phy_radio_switch((phy_info_t *)wlc_hw->band->pi, ON);

		/* resume d11 core */
		wlc_bmac_enable_mac(wlc_hw);
	} else {
		/* suspend d11 core */
		wlc_bmac_suspend_mac_and_wait(wlc_hw);

		phy_radio_switch((phy_info_t *)wlc_hw->band->pi, OFF);
		/* need to skip for 5356 in case of radio_pwrsave feature. */
		if (!skip_anacore)
			phy_ana_switch((phy_info_t *)wlc_hw->band->pi, OFF);

		if (PMUCTL_ENAB(wlc_hw->sih)) {
			OR_REG(wlc_hw->osh, D11_ClockCtlStatus(wlc_hw), CCS_FORCEHWREQOFF);
		}
	}

	return TRUE;
}

void
wlc_bmac_minimal_radio_hw(wlc_hw_info_t *wlc_hw, bool enable)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	if (PMUCTL_ENAB(wlc_hw->sih)) {

		if (enable == TRUE) {
			AND_REG(wlc->osh, D11_ClockCtlStatus(wlc), ~CCS_FORCEHWREQOFF);
		} else {
			OR_REG(wlc->osh, D11_ClockCtlStatus(wlc), CCS_FORCEHWREQOFF);
		}
	}
}

bool
wlc_bmac_si_iscoreup(wlc_hw_info_t *wlc_hw)
{
	return si_iscoreup(wlc_hw->sih);
}

uint16
wlc_bmac_rate_shm_offset(wlc_hw_info_t *wlc_hw, uint8 rate)
{
	uint16 table_ptr;
	uint8 indx;

	/* get the phy specific rate encoding for the PLCP SIGNAL field */
	/* XXX4321 fixup needed ? */
	if (RATE_ISOFDM(rate))
		table_ptr = M_RT_DIRMAP_A(wlc_hw);
	else
		table_ptr = M_RT_DIRMAP_B(wlc_hw);

	/* for a given rate, the LS-nibble of the PLCP SIGNAL field is
	 * the index into the rate table.
	 */
	indx = rate_info[rate] & RATE_INFO_M_RATE_MASK;

	/* Find the SHM pointer to the rate table entry by looking in the
	 * Direct-map Table
	 */
	return (2*wlc_bmac_read_shm(wlc_hw, table_ptr + (indx * 2)));
}

void
wlc_bmac_stf_set_rateset_shm_offset(wlc_hw_info_t *wlc_hw, uint count, uint16 pos, uint16 mask,
wlc_stf_rs_shm_offset_t *stf_rs)
{
	uint16 idx;
	uint16 entry_ptr;
	uint16 val;
	uint8 rate;

	for (idx = 0; idx < count; idx++) {
		rate = stf_rs->rate[idx] & RATE_MASK;
		entry_ptr = wlc_bmac_rate_shm_offset(wlc_hw, rate);
		val = stf_rs->val[idx];
		if (D11REV_GE(wlc_hw->corerev, 40)) {
			val |= (wlc_bmac_read_shm(wlc_hw, (entry_ptr + pos)) & ~mask);
		}
		wlc_bmac_write_shm(wlc_hw, (entry_ptr + pos), val);
	}
}

#ifdef PHYCAL_CACHING
void
wlc_bmac_set_phycal_cache_flag(wlc_hw_info_t *wlc_hw, bool state)
{
	wlc_phy_cal_cache_set(wlc_hw->band->pi, state);
}

bool
wlc_bmac_get_phycal_cache_flag(wlc_hw_info_t *wlc_hw)
{
	return wlc_phy_cal_cache_get(wlc_hw->band->pi);
}
#endif /* PHYCAL_CACHING */

void
wlc_bmac_set_txpwr_percent(wlc_hw_info_t *wlc_hw, uint8 val)
{
	wlc_phy_txpwr_percent_set(wlc_hw->band->pi, val);
}

void
wlc_bmac_set_txpwr_degrade(wlc_hw_info_t *wlc_hw, uint8 val)
{
	wlc_phy_txpwr_degrade(wlc_hw->band->pi, val);
}

/** update auto d11 shmdef specific to ucode downlaod type */
void
wlc_bmac_autod11_shm_upd(wlc_hw_info_t *wlc_hw, uint8 ucodeType)
{
	int corerev = 0;

	ASSERT(wlc_hw != NULL);
	corerev = (wlc_hw->corerev == 28) ? 25 : wlc_hw->corerev;

	switch (ucodeType) {
		case D11_IF_SHM_STD:
			d11shm_select_ucode_std(&wlc_hw->shmdefs, corerev);
			break;
#ifdef BCMULP
		case D11_IF_SHM_ULP:
			d11shm_select_ucode_ulp(&wlc_hw->shmdefs, corerev);
			break;
#else
#ifdef WOWL
		case D11_IF_SHM_WOWL:
			d11shm_select_ucode_wowl(&wlc_hw->shmdefs, corerev);
			break;
#endif /* WOWL */
#endif /* BCMULP */
		default:
			d11shm_select_ucode_std(&wlc_hw->shmdefs, corerev);
			break;
	}
	if (wlc_hw->wlc)
		wlc_hw->wlc->shmdefs = wlc_hw->shmdefs;

	if (wlc_hw->band && wlc_hw->band->pi)
		wlc_phy_set_shmdefs(wlc_hw->band->pi, wlc_hw->shmdefs);
}

uint32
wlc_bmac_read_counter(wlc_hw_info_t* wlc_hw, uint baseaddr, int lo_off, int hi_off)
{
	uint16 high, tmp_high, low;

	ASSERT(baseaddr != (uint)~0);

	tmp_high = wlc_bmac_read_shm(wlc_hw, baseaddr + hi_off);
	low = wlc_bmac_read_shm(wlc_hw, baseaddr + lo_off);
	high = wlc_bmac_read_shm(wlc_hw, baseaddr + hi_off);
	if (high != tmp_high) {
		low = 0;	/* assume it zero */
	}
	return (high << 16) | low;
}

uint32
wlc_bmac_cca_read_counter(wlc_hw_info_t* wlc_hw, int lo_off, int hi_off)
{
	if (wlc_hw->cca_shm_base == ID32_INVALID) {
		return 0;
	}
	return wlc_bmac_read_counter(wlc_hw, wlc_hw->cca_shm_base, lo_off, hi_off);
}

int
wlc_bmac_cca_stats_read(wlc_hw_info_t *wlc_hw, cca_ucode_counts_t *cca_counts)
{
	uint32 tsf_h;

	/* Read shmem */
	cca_counts->txdur =
		wlc_bmac_cca_read_counter(wlc_hw, M_CCA_TXDUR_L_OFFSET(wlc_hw),
			M_CCA_TXDUR_H_OFFSET(wlc_hw));
	cca_counts->ibss =
		wlc_bmac_cca_read_counter(wlc_hw, M_CCA_INBSS_L_OFFSET(wlc_hw),
			M_CCA_INBSS_H_OFFSET(wlc_hw));
	cca_counts->obss =
		wlc_bmac_cca_read_counter(wlc_hw, M_CCA_OBSS_L_OFFSET(wlc_hw),
			M_CCA_OBSS_H_OFFSET(wlc_hw));
	cca_counts->noctg =
		wlc_bmac_cca_read_counter(wlc_hw, M_CCA_NOCTG_L_OFFSET(wlc_hw),
			M_CCA_NOCTG_H_OFFSET(wlc_hw));
	cca_counts->nopkt =
		wlc_bmac_cca_read_counter(wlc_hw, M_CCA_NOPKT_L_OFFSET(wlc_hw),
			M_CCA_NOPKT_H_OFFSET(wlc_hw));
	cca_counts->PM =
		wlc_bmac_cca_read_counter(wlc_hw, M_MAC_SLPDUR_L_OFFSET(wlc_hw),
			M_MAC_SLPDUR_H_OFFSET(wlc_hw));

	if (D11REV_GE(wlc_hw->corerev, 40)) {
		cca_counts->wifi =
			wlc_bmac_cca_read_counter(wlc_hw, M_CCA_WIFI_L_OFFSET(wlc_hw),
				M_CCA_WIFI_H_OFFSET(wlc_hw));
	}

#if defined(ISID_STATS)
	cca_counts->crsglitch = wlc_bmac_read_shm(wlc_hw,
		MACSTAT_ADDR(wlc_hw, MCSTOFF_RXCRSGLITCH));
	cca_counts->badplcp = wlc_bmac_read_shm(wlc_hw,
		MACSTAT_ADDR(wlc_hw, MCSTOFF_RXBADPLCP));
	cca_counts->bphy_crsglitch = wlc_bmac_read_shm(wlc_hw,
		MACSTAT_ADDR(wlc_hw, MCSTOFF_BPHYGLITCH));
	cca_counts->bphy_badplcp = wlc_bmac_read_shm(wlc_hw,
		MACSTAT_ADDR(wlc_hw, MCSTOFF_BPHY_BADPLCP));
#endif // endif
#if (defined(WL_PROT_OBSS) || defined(WL_OBSS_DYNBW))
	if (WLC_PROT_OBSS_ENAB(wlc_hw->wlc->pub)) {
		if (D11REV_GE(wlc_hw->corerev, 40)) {
		cca_counts->rxcrs_pri = wlc_bmac_read_counter(wlc_hw, 0,
			M_CCA_RXPRI_LO(wlc_hw), M_CCA_RXPRI_HI(wlc_hw));
		cca_counts->rxcrs_sec20 = wlc_bmac_read_counter(wlc_hw, 0,
			M_CCA_RXSEC20_LO(wlc_hw), M_CCA_RXSEC20_HI(wlc_hw));
		cca_counts->rxcrs_sec40 = wlc_bmac_read_counter(wlc_hw, 0,
			M_CCA_RXSEC40_LO(wlc_hw), M_CCA_RXSEC40_HI(wlc_hw));
		cca_counts->sec_rssi_hist_hi = wlc_bmac_read_shm(wlc_hw, M_SECRSSI0(wlc_hw));
		cca_counts->sec_rssi_hist_med = wlc_bmac_read_shm(wlc_hw, M_SECRSSI1(wlc_hw));
		cca_counts->sec_rssi_hist_low = wlc_bmac_read_shm(wlc_hw, M_SECRSSI2(wlc_hw));
		cca_counts->rxdrop20s = wlc_bmac_read_shm(wlc_hw, M_RXDROP20S_CNT(wlc_hw));
		cca_counts->rx20s = wlc_bmac_read_shm(wlc_hw, M_RX20S_CNT(wlc_hw));
		}
	}
#endif // endif
#ifdef WL_CCA_STATS_MESH
	if (D11REV_IS(wlc_hw->corerev, 65)) {
		int i;
		for (i = 0; i < 8; i++) {
			int j = i*4;
			cca_counts->txnode[i] = wlc_bmac_cca_read_counter(wlc_hw,
					M_CCA_TXNODE0_L_OFFSET(wlc_hw)+ j*2,
					M_CCA_TXNODE0_H_OFFSET(wlc_hw)+ j*2);
			cca_counts->rxnode[i] = wlc_bmac_cca_read_counter(wlc_hw,
					M_CCA_RXNODE0_L_OFFSET(wlc_hw)+ j*2,
					M_CCA_RXNODE0_H_OFFSET(wlc_hw)+ j*2);
		}
		cca_counts->xxobss = wlc_bmac_cca_read_counter(wlc_hw,
			M_CCA_XXOBSS_L_OFFSET(wlc_hw), M_CCA_XXOBSS_H_OFFSET(wlc_hw));
	}
#endif /* WL_CCA_STATS_MESH */
	wlc_bmac_read_tsf(wlc_hw, &cca_counts->usecs, &tsf_h);
	return 0;
}

int
wlc_bmac_obss_stats_read(wlc_hw_info_t *wlc_hw, wlc_bmac_obss_counts_t *obss_counts)
{
	uint32 tsf_h;

	/* duration */
	obss_counts->txdur =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_TXDUR_L(wlc_hw), M_CCA_TXDUR_H(wlc_hw));
	obss_counts->ibss =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_INBSS_L(wlc_hw), M_CCA_INBSS_H(wlc_hw));
	obss_counts->obss =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_OBSS_L(wlc_hw), M_CCA_OBSS_H(wlc_hw));
	obss_counts->noctg =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_NOCTG_L(wlc_hw), M_CCA_NOCTG_H(wlc_hw));
	obss_counts->nopkt =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_NOPKT_L(wlc_hw), M_CCA_NOPKT_H(wlc_hw));
	obss_counts->PM =
		wlc_bmac_read_counter(wlc_hw, 0, M_MAC_SLPDUR_L(wlc_hw), M_MAC_SLPDUR_H(wlc_hw));
	obss_counts->txopp =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_TXOP_L(wlc_hw), M_CCA_TXOP_H(wlc_hw));
	obss_counts->gdtxdur =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_GDTXDUR_L(wlc_hw), M_CCA_GDTXDUR_H(wlc_hw));
	obss_counts->bdtxdur =
		wlc_bmac_read_counter(wlc_hw, 0, M_CCA_BDTXDUR_L(wlc_hw), M_CCA_BDTXDUR_H(wlc_hw));
	obss_counts->slot_time_txop = R_REG(wlc_hw->osh, D11_IFS_SLOT(wlc_hw));
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		obss_counts->suspend = wlc_bmac_read_counter(wlc_hw, 0,
			M_CCA_SUSP_L(wlc_hw), M_CCA_SUSP_H(wlc_hw));
		obss_counts->suspend_cnt = wlc_bmac_read_shm(wlc_hw, M_MACSUSP_CNT(wlc_hw));
		obss_counts->rxwifi = wlc_bmac_read_counter(wlc_hw, 0,
				M_CCA_WIFI_L(wlc_hw), M_CCA_WIFI_H(wlc_hw));
		obss_counts->edcrs = wlc_bmac_read_counter(wlc_hw, 0,
				M_CCA_EDCRSDUR_L(wlc_hw), M_CCA_EDCRSDUR_H(wlc_hw));
	} else {
#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
		bmac_suspend_stats_t* susp_stats = wlc_hw->suspend_stats;
		uint32 suspend_time = susp_stats->suspended;
		uint32 timenow = R_REG(wlc_hw->osh, D11_TSFTimerLow(wlc_hw));

		if (susp_stats->suspend_start > susp_stats->suspend_end &&
		    timenow > susp_stats->suspend_start) {
			suspend_time += (timenow - susp_stats->suspend_start) / 100;
		}

		obss_counts->suspend = suspend_time;
		obss_counts->suspend_cnt = susp_stats->suspend_count;
#endif // endif
	}

	/* obss */
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		obss_counts->rxcrs_pri = wlc_bmac_read_counter(wlc_hw, 0,
			M_CCA_RXPRI_LO(wlc_hw), M_CCA_RXPRI_HI(wlc_hw));
		obss_counts->rxcrs_sec20 = wlc_bmac_read_counter(wlc_hw, 0,
			M_CCA_RXSEC20_LO(wlc_hw), M_CCA_RXSEC20_HI(wlc_hw));
		obss_counts->rxcrs_sec40 = wlc_bmac_read_counter(wlc_hw, 0,
			M_CCA_RXSEC40_LO(wlc_hw), M_CCA_RXSEC40_HI(wlc_hw));
		obss_counts->sec_rssi_hist_hi = wlc_bmac_read_shm(wlc_hw, M_SECRSSI0(wlc_hw));
		obss_counts->sec_rssi_hist_med = wlc_bmac_read_shm(wlc_hw, M_SECRSSI1(wlc_hw));
		obss_counts->sec_rssi_hist_low = wlc_bmac_read_shm(wlc_hw, M_SECRSSI2(wlc_hw));
		obss_counts->rxdrop20s = wlc_bmac_read_shm(wlc_hw, M_RXDROP20S_CNT(wlc_hw));
		obss_counts->rx20s = wlc_bmac_read_shm(wlc_hw, M_RX20S_CNT(wlc_hw));
	}

	wlc_bmac_read_tsf(wlc_hw, &obss_counts->usecs, &tsf_h);

	/* counters */
	obss_counts->txfrm = wlc_bmac_read_shm(wlc_hw, M_TXFRAME_CNT(wlc_hw));
	obss_counts->rxstrt = wlc_bmac_read_shm(wlc_hw, M_RXSTRT_CNT(wlc_hw));
	obss_counts->crsglitch = wlc_bmac_read_shm(wlc_hw, M_RXCRSGLITCH_CNT(wlc_hw));
	obss_counts->bphy_crsglitch = wlc_bmac_read_shm(wlc_hw, M_BPHYGLITCH_CNT(wlc_hw));
	obss_counts->badplcp = wlc_bmac_read_shm(wlc_hw, M_RXBADPLCP_CNT(wlc_hw));
	obss_counts->bphy_badplcp = wlc_bmac_read_shm(wlc_hw, M_RXBPHY_BADPLCP_CNT(wlc_hw));
	obss_counts->badfcs = wlc_bmac_read_shm(wlc_hw, M_RXBADFCS_CNT(wlc_hw));
#ifdef WL_CCA_STATS_MESH
	if (D11REV_IS(wlc_hw->corerev, 65)) {
		int i;
		for (i = 0; i < 8; i++) {
			int j = i*4;
			obss_counts->txnode[i] = wlc_bmac_cca_read_counter(wlc_hw,
					M_CCA_TXNODE0_L_OFFSET(wlc_hw)+ j*2,
					M_CCA_TXNODE0_H_OFFSET(wlc_hw)+ j*2);
			obss_counts->rxnode[i] = wlc_bmac_cca_read_counter(wlc_hw,
					M_CCA_RXNODE0_L_OFFSET(wlc_hw)+ j*2,
					M_CCA_RXNODE0_H_OFFSET(wlc_hw)+ j*2);
		}
		obss_counts->xxobss = wlc_bmac_cca_read_counter(wlc_hw,
			M_CCA_XXOBSS_L_OFFSET(wlc_hw), M_CCA_XXOBSS_H_OFFSET(wlc_hw));
	}
#endif /* WL_CCA_STATS_MESH */
	return 0;
} /* wlc_bmac_obss_stats_read */

void
wlc_bmac_antsel_set(wlc_hw_info_t *wlc_hw, uint32 antsel_avail)
{
	wlc_hw->antsel_avail = antsel_avail;
}

/* BTC stuff BEGIN */
/**
 * Create space in wlc_hw->btc struct to save a local copy of btc_params when the PSM goes down
 * Copy btc_paramXX and btc_flag information from NVRAM to wlc_info_t structure, so that it can be
 * used during INIT, when NVRAM has already been released (reclaimed).
 */
static int
BCMATTACHFN(wlc_bmac_btc_param_attach)(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 indx;
	char   buf[15];
	uint16 num_params = 0;

	/* Allocate space for shadow btc params in driver.
	 * first check whether the chip includes the underlying shmem
	 * Not needed for MFG images, since radio is always on (i.e. mpc 0)
	 */
#ifdef DONGLEBUILD
	if ((wlc_hw->btc->wlc_btc_params = (uint16 *)
		MALLOCZ(wlc->osh, M_BTCX_BACKUP_SIZE*sizeof(uint16))) == NULL) {
		WL_ERROR(
		("wlc_bmac_attach: no mem for wlc_btc_params, malloced %d bytes\n",
			MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
#endif /* DONGLEBUILD */

	/* Allocate space for Host-based COEX variables */
	if ((wlc_hw->btc->wlc_btc_params_fw = (uint16 *)
		MALLOCZ(wlc->osh, (BTC_FW_NUM_INDICES*sizeof(uint16)))) == NULL) {
		WL_ERROR(
		("wlc_bmac_attach: no mem for wlc_btc_params_fw, malloced %d bytes\n",
			MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	/* first count nr of present btc_params in NVRAM */
	for (indx = 0; indx <= M_BTCX_MAX_INDEX; indx++) {
		snprintf(buf, sizeof(buf), rstr_btc_paramsD, indx);
		if (getvar(wlc_hw->vars, buf) != NULL) {
			num_params++;
		}
	}
	if ((num_params > 0) || (getvar(wlc_hw->vars, rstr_btc_flags) != NULL)) {
		wlc->btc_param_vars = (struct wlc_btc_param_vars_info*) MALLOC(wlc->osh,
			sizeof(struct wlc_btc_param_vars_info) +
			num_params * sizeof(struct wlc_btc_param_vars_entry));
		if (wlc->btc_param_vars == NULL) {
			WL_ERROR(("wlc_btc_param_attach: no mem for btc_param_vars, malloc: %db\n",
				MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		wlc->btc_param_vars->num_entries = 0;
		/* go through all btc_params, and if exist in nvram copy to wlc->btc_param_vars */
		for (indx = 0; indx <= M_BTCX_MAX_INDEX; indx++) {
			snprintf(buf, sizeof(buf), rstr_btc_paramsD, indx);
			if (getvar(wlc_hw->vars, buf) != NULL) {
				wlc->btc_param_vars->param_list[
					wlc->btc_param_vars->num_entries].value =
					(uint16)getintvar(wlc_hw->vars, buf);
				wlc->btc_param_vars->param_list[
					wlc->btc_param_vars->num_entries++].index = indx;
			}
		}
		ASSERT(wlc->btc_param_vars->num_entries == num_params);
		/* check if btc_flags exist in nvram and if so copy to wlc->btc_param_vars */
		if (getvar(wlc_hw->vars, rstr_btc_flags) != NULL) {
			wlc->btc_param_vars->flags = (uint16)getintvar(wlc_hw->vars,
				rstr_btc_flags);
			wlc->btc_param_vars->flags_present = TRUE;
		} else {
			wlc->btc_param_vars->flags_present = FALSE;
		}
	}
	/* Initializing the host-based Bt-coex parameters */
	if (wlc_hw->btc->wlc_btc_params_fw) {

		for (indx = 0; indx < BTC_FW_NUM_INDICES; indx++) {
			wlc_hw->btc->wlc_btc_params_fw[indx] = btc_fw_params_init_vals[indx];
		}
	}
	return BCME_OK;
} /* wlc_bmac_btc_param_attach */

static void
BCMINITFN(wlc_bmac_btc_param_init)(wlc_hw_info_t *wlc_hw)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint16 indx;

	/* cache the pointer to the BTCX shm block, which won't change after coreinit */
	wlc_hw->btc->bt_shm_addr = 2 * wlc_bmac_read_shm(wlc_hw, M_BTCX_BLK_PTR(wlc));

	if (wlc_hw->btc->bt_shm_addr == 0) {
		return;
	}

#ifdef DONGLEBUILD
	if (wlc_hw->btc->wlc_btc_params && wlc_hw->btc->btc_init_flag) {
		for (indx = 0; indx < M_BTCX_BACKUP_SIZE; indx++)
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr+indx*2,
				(uint16)(wlc_hw->btc->wlc_btc_params[indx]));
	} else {
	/* First time initialization from nvram or ucode */
	/* or if wlc_btc_params is not malloced */
		wlc_hw->btc->btc_init_flag = TRUE;
#endif /* DONGLEBUILD */
		if (wlc->btc_param_vars == NULL) {
		/* wlc_btc_param_init: wlc->btc_param_vars unavailable */
			return;
		}
		/* go through all btc_params, if they existed in nvram, overwrite shared memory */
		for (indx = 0; indx < wlc->btc_param_vars->num_entries; indx++)
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
				wlc->btc_param_vars->param_list[indx].index * 2,
				wlc->btc_param_vars->param_list[indx].value);
		/* go through btc_flags list as copied from nvram and initialize them */
		if (wlc->btc_param_vars->flags_present) {
			wlc_hw->btc->flags = wlc->btc_param_vars->flags;
		}
#ifdef DONGLEBUILD
	}
#endif /* DONGLEBUILD */
	wlc_bmac_btc_btcflag2ucflag(wlc_hw);
}

static void
wlc_bmac_btc_btcflag2ucflag(wlc_hw_info_t *wlc_hw)
{
	int indx;
	int btc_flags = wlc_hw->btc->flags;
	uint16 btc_mhf = (btc_flags & WL_BTC_FLAG_PREMPT) ? MHF2_BTCPREMPT : 0;

	wlc_bmac_mhf(wlc_hw, MHF2, MHF2_BTCPREMPT, btc_mhf, WLC_BAND_2G);
	btc_mhf = 0;
	for (indx = BTC_FLAGS_MHF3_START; indx <= BTC_FLAGS_MHF3_END; indx++)
		if (btc_flags & (1 << indx))
			btc_mhf |= btc_ucode_flags[indx].mask;

	/* protection set dynamically */
	btc_mhf &= ~(MHF3_BTCX_ACTIVE_PROT | MHF3_BTCX_PS_PROTECT);
	wlc_bmac_mhf(wlc_hw, MHF3, MHF3_BTCX_DEF_BT | MHF3_BTCX_SIM_RSP |
		MHF3_BTCX_ECI | MHF3_BTCX_SIM_TX_LP, btc_mhf, WLC_BAND_2G);

	/* Ucode needs ECI indication in all bands */
	if ((btc_mhf & ~MHF3_BTCX_ECI) == 0)
		wlc_bmac_mhf(wlc_hw, MHF3, MHF3_BTCX_ECI, btc_mhf & MHF3_BTCX_ECI, WLC_BAND_AUTO);
	btc_mhf = 0;
	for (indx = BTC_FLAGS_MHF3_END + 1; indx < BTC_FLAGS_SIZE; indx++)
		if (btc_flags & (1 << indx))
			btc_mhf |= btc_ucode_flags[indx].mask;

	wlc_bmac_mhf(wlc_hw, MHF5, MHF5_BTCX_LIGHT | MHF5_BTCX_PARALLEL,
		btc_mhf, WLC_BAND_2G);
}

#ifdef STA
void
wlc_bmac_btc_update_predictor(wlc_hw_info_t *wlc_hw)
{
	uint32 tsf;
	uint16 bt_period = 0, bt_last_l = 0, bt_last_h = 0;
	uint32 bt_last, bt_next;

	/* Make sure period is known */
	bt_period = wlc_bmac_read_shm(wlc_hw, M_BTCX_PRED_PER(wlc_hw));

	if (bt_period == 0)
		return;

	tsf = R_REG(wlc_hw->osh, D11_TSFTimerLow(wlc_hw));

	/* Avoid partial read */
	do {
		bt_last_l = wlc_bmac_read_shm(wlc_hw, M_BTCX_LAST_SCO(wlc_hw));
		if (D11REV_LT(wlc_hw->corerev, 40)) {
			bt_last_h = wlc_bmac_read_shm(wlc_hw, M_BTCX_LAST_SCO_H(wlc_hw));
		}
	} while (bt_last_l != wlc_bmac_read_shm(wlc_hw, M_BTCX_LAST_SCO(wlc_hw)));
	bt_last = ((uint32)bt_last_h << 16) | bt_last_l;

	/* Calculate next expected BT slot time */
	bt_next = bt_last + ((((tsf - bt_last) / bt_period) + 1) * bt_period);
	wlc_bmac_write_shm(wlc_hw, M_BTCX_NEXT_SCO(wlc_hw),
		(uint16)(bt_next & 0xffff));
}
#endif /* STA */

/**
 * Bluetooth/WLAN coexistence parameters are exposed for some customers.
 * Rather than exposing all of shared memory, an index that is range-checked
 * is translated to an address.
 */
static bool
wlc_bmac_btc_param_to_shmem(wlc_hw_info_t *wlc_hw, uint32 *pval)
{
	if ((*pval <= M_BTCX_MAX_INDEX) && (wlc_hw->btc->bt_shm_addr)) {
		*pval = wlc_hw->btc->bt_shm_addr + (2 * (*pval));
		return TRUE;
	}
	return FALSE;
}

static bool
wlc_bmac_btc_flags_ucode(uint8 val, uint8 *idx, uint16 *mask)
{
	/* Check that the index is valid */
	if (val >= ARRAYSIZE(btc_ucode_flags))
		return FALSE;

	*idx = btc_ucode_flags[val].idx;
	*mask = btc_ucode_flags[val].mask;

	return TRUE;
}

int
wlc_bmac_btc_period_get(wlc_hw_info_t *wlc_hw, uint16 *btperiod, bool *btactive,
	uint16 *agg_off_bm, uint16 *acl_last_ts, uint16 *a2dp_last_ts)
{
	uint16 bt_period = 0;
	uint32 tmp = 0;

#define BTCX_PER_THRESHOLD 4
#define BTCX_BT_ACTIVE_THRESHOLD 5
#define BTCX_PER_OUT_OF_SYNC_CNT 5

	bt_period = wlc_bmac_read_shm(wlc_hw, M_BTCX_PRED_PER(wlc_hw));
	if (bt_period) {
		/*
		Read PRED_PER_COUNT only for non-ECI chips. For ECI, PRED_PER gets
		cleared as soon as periodic activity ends so there is no need to
		monitor PRED_PER_COUNT.
		*/
		if (!BCMCOEX_ENAB_BMAC(wlc_hw)) {
			if (wlc_bmac_read_shm(wlc_hw, M_BTCX_PRED_COUNT(wlc_hw)) >
			    BTCX_PER_THRESHOLD) {
				tmp = bt_period;
			}
		} else {
			tmp = bt_period;
		}
	}
	/*
	This code has been added to filter out any spurious reads of PRED_PER
	being '0' (this may happen if the value is read right after a mac
	suspend/resume because ucode clears out this value after resumption).
	*/
	if (!tmp) {
		wlc_hw->btc->bt_period_out_of_sync_cnt++;
		if (wlc_hw->btc->bt_period_out_of_sync_cnt <= BTCX_PER_OUT_OF_SYNC_CNT) {
			tmp = wlc_hw->btc->bt_period;
		} else {
			wlc_hw->btc->bt_period_out_of_sync_cnt = BTCX_PER_OUT_OF_SYNC_CNT;
		}
	}
	else {
		wlc_hw->btc->bt_period_out_of_sync_cnt = 0;
	}

	*btperiod = wlc_hw->btc->bt_period = (uint16)tmp;

	if (D11REV_GE(wlc_hw->corerev, 40)) {
		*agg_off_bm = wlc_bmac_read_shm(wlc_hw, M_BTCX_AGG_OFF_BM(wlc_hw));
	} else {
		*agg_off_bm = 0;
	}
	*a2dp_last_ts = wlc_bmac_read_shm(wlc_hw, M_BTCX_LAST_A2DP(wlc_hw));
	*acl_last_ts = wlc_bmac_read_shm(wlc_hw, M_BTCX_LAST_DATA(wlc_hw));

	if (R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_PSM_RUN) {
		tmp = R_REG(wlc_hw->osh, D11_BTCX_CUR_RFACT_TIMER(wlc_hw));
		/* code below can be optimized for speed; however, we choose not
		 * to do that to achieve better readability
		 */
		if (wlc_hw->btc->bt_active) {
			/* active state : switch to inactive when reading 0xffff */
			if (tmp == 0xffff) {
				wlc_hw->btc->bt_active = FALSE;
				wlc_hw->btc->bt_active_asserted_cnt = 0;
			}
		} else {
			/* inactive state : switch to active when bt_active asserted for
			 * more than a certain times
			 */
			if (tmp == 0xffff)
				wlc_hw->btc->bt_active_asserted_cnt = 0;
			/* concecutive asserts, now declare bt is active */
			else if (++wlc_hw->btc->bt_active_asserted_cnt >= BTCX_BT_ACTIVE_THRESHOLD)
				wlc_hw->btc->bt_active = TRUE;
		}
	}

	*btactive = wlc_hw->btc->bt_active;

	return BCME_OK;
} /* wlc_bmac_btc_period_get */

int
wlc_bmac_btc_mode_set(wlc_hw_info_t *wlc_hw, int btc_mode)
{
	uint16 btc_mhfs[MHFMAX];
	bool ucode_up = FALSE;
#if defined(BCMECICOEX)
	si_t *sih = wlc_hw->wlc->pub->sih;
#endif // endif

	if (!BTCX_ENAB(wlc_hw)) {
		return BCME_UNSUPPORTED;
	}

	if (btc_mode > WL_BTC_DEFAULT) {
		WL_ERROR(("wl%d: %s: Bad argument btc_mode:%d\n", wlc_hw->unit, __FUNCTION__,
			btc_mode));
		return BCME_BADARG;
	}

	/* Make sure 2-wire or 3-wire decision has been made */
	ASSERT((wlc_hw->btc->wire >= WL_BTC_2WIRE) || (wlc_hw->btc->wire <= WL_BTC_4WIRE));

	 /* Determine the default mode for the device */
	if (btc_mode == WL_BTC_DEFAULT) {
		if (BCMCOEX_ENAB_BMAC(wlc_hw) || (wlc_hw->boardflags2 & BFL2_BTCLEGACY)) {
			btc_mode = WL_BTC_FULLTDM;
			/* default to hybrid mode for combo boards with 2 or more antennas */
			if (wlc_hw->btc->btcx_aa > 2 && D11REV_LT(wlc_hw->corerev, 129)) {
				btc_mode = WL_BTC_HYBRID;
			}
		} else {
			btc_mode = WL_BTC_DISABLE;
		}
	}

	/* Do not allow an enable without hw support */
	if (btc_mode != WL_BTC_DISABLE) {
		if ((wlc_hw->btc->wire >= WL_BTC_3WIRE) && !MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw)) {
			WL_ERROR(("wl%d: %s: Bad option wire:%d machwcap:%d\n", wlc_hw->unit,
				__FUNCTION__, wlc_hw->btc->wire, wlc_hw->machwcap));
			return BCME_BADOPTION;
		}
	}

	/* Initialize ucode flags */
	bzero(btc_mhfs, sizeof(btc_mhfs));
	wlc_hw->btc->flags = 0;

	if (wlc_hw->up)
		ucode_up = (R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC);

	if (btc_mode != WL_BTC_DISABLE) {
		btc_mhfs[MHF1] |= MHF1_BTCOEXIST;
		if (wlc_hw->btc->wire == WL_BTC_2WIRE) {
			/* BMAC_NOTES: sync the state with HIGH driver ??? */
			/* Make sure 3-wire coex is off */
			if (wlc_hw->boardflags & BFL_BTC2WIRE_ALTGPIO) {
				btc_mhfs[MHF2] |= MHF2_BTC2WIRE_ALTGPIO;
			} else {
				btc_mhfs[MHF2] &= ~MHF2_BTC2WIRE_ALTGPIO;
			}
		} else {
			/* by default we use PS protection unless overriden. */
			if (btc_mode == WL_BTC_HYBRID)
				wlc_hw->btc->flags |= WL_BTC_FLAG_SIM_RSP;
			else if (btc_mode == WL_BTC_LITE) {
				wlc_hw->btc->flags |= WL_BTC_FLAG_LIGHT;
				wlc_hw->btc->flags |= WL_BTC_FLAG_SIM_RSP;
			} else if (btc_mode == WL_BTC_PARALLEL) {
				wlc_hw->btc->flags |= WL_BTC_FLAG_PARALLEL;
				wlc_hw->btc->flags |= WL_BTC_FLAG_SIM_RSP;
			} else if (wlc_hw->btc->wire != WL_BTC_3WIRE) {
				wlc_hw->btc->flags |=
					(WL_BTC_FLAG_PS_PROTECT | WL_BTC_FLAG_ACTIVE_PROT);
			}

			if (BCMCOEX_ENAB_BMAC(wlc_hw)) {
				wlc_hw->btc->flags |= WL_BTC_FLAG_ECI;
			} else {
				if (wlc_hw->btc->wire == WL_BTC_4WIRE) {
					btc_mhfs[MHF3] |= MHF3_BTCX_EXTRA_PRI;
				} else {
					wlc_hw->btc->flags |= (WL_BTC_FLAG_PREMPT |
						WL_BTC_FLAG_BT_DEF);
				}
			}
		}
	} else {
		btc_mhfs[MHF1] &= ~MHF1_BTCOEXIST;
	}

	if (D11REV_GE(wlc_hw->corerev, 48)) {
		/* no auto mode for rev < 48 to preserve existing sisoack setting */
		if (btc_mode == WL_BTC_HYBRID) {
			wlc_btc_siso_ack_set(wlc_hw->wlc, AUTO, FALSE);
		} else {
			wlc_btc_siso_ack_set(wlc_hw->wlc, 0, FALSE);
		}
	}

	wlc_hw->btc->mode = btc_mode;

	/* Set the MHFs only in 2G band
	 * If we are on the other band, update the sw cache for the
	 * 2G band.
	 */
	if (wlc_hw->up && ucode_up)
		wlc_bmac_suspend_mac_and_wait(wlc_hw);

	wlc_bmac_mhf(wlc_hw, MHF1, MHF1_BTCOEXIST, btc_mhfs[MHF1], WLC_BAND_2G);
	wlc_bmac_mhf(wlc_hw, MHF2, MHF2_BTC2WIRE_ALTGPIO, btc_mhfs[MHF2], WLC_BAND_2G);
	/* BTCX_EXTRA_PRI flag used only in DCF code */
	if (D11REV_LT(wlc_hw->corerev, 40)) {
		wlc_bmac_mhf(wlc_hw, MHF3, MHF3_BTCX_EXTRA_PRI, btc_mhfs[MHF3], WLC_BAND_2G);
	}
	wlc_bmac_btc_btcflag2ucflag(wlc_hw);

	if (wlc_hw->up && ucode_up) {
		wlc_bmac_enable_mac(wlc_hw);
	}

#if defined(BCMECICOEX)
	/* send current btc_mode info to BT */
	if (BCMGCICOEX_ENAB_BMAC(wlc_hw)) {
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_control_1),
			GCI_WL_BTC_MODE_MASK, GCI_WL_BTC_MODE_MASK);

		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[1]),
			GCI_WL_BTC_MODE_MASK, wlc_hw->btc->mode << GCI_WL_BTC_MODE_SHIFT);
	}
#endif // endif

	/* phy BTC mode handling */
	phy_btcx_set_mode(wlc_hw->band->pi, wlc_hw->btc->mode);

	return BCME_OK;
} /* wlc_bmac_btc_mode_set */

int
wlc_bmac_btc_mode_get(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->btc->mode;
}

int
wlc_bmac_btc_task_set(wlc_hw_info_t *wlc_hw, int btc_task)
{
	int err = BCME_OK;

	if (btc_task >= BTCX_TASK_MAX) {
		WL_ERROR(("wl%d: %s: Bad argument btc_task:%d\n", wlc_hw->unit, __FUNCTION__,
			btc_task));
		return BCME_BADARG;
	}

	/* Make sure 2-wire, 3-wire or 4-wire decision has been made */
	ASSERT((wlc_hw->btc->wire >= WL_BTC_2WIRE) || (wlc_hw->btc->wire <= WL_BTC_4WIRE));

	/* Do not allow an enable without hw support */
	if (btc_task != BTCX_TASK_UNKNOWN) {
		if ((wlc_hw->btc->wire >= WL_BTC_3WIRE) && !MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw)) {
			WL_ERROR(("wl%d: %s: Bad option wire:%d machwcap:%d\n", wlc_hw->unit,
				__FUNCTION__, wlc_hw->btc->wire, wlc_hw->machwcap));
			return BCME_BADOPTION;
		}
	}

	wlc_hw->btc->task = btc_task;

	/* core clk should be enabled for shared memory to be written */
	if (wlc_hw->clk) {
		/* SW-controlled coex: write task type into ucode's shadow copy of SECI register */
		wlc_bmac_write_shm(wlc_hw, M_BTCX_ECI1(wlc_hw),
				(uint16) ((btc_task << BT_TYPE_OFFSET) & BT_TYPE_MASK));
	}

	return err;
} /* wlc_bmac_btc_task_set */

int
wlc_bmac_btc_task_get(wlc_hw_info_t *wlc_hw)
{
	/* core clk should be enabled for shared memory to be read */
	if (wlc_hw->clk) {
		uint16 int_val = wlc_bmac_read_shm(wlc_hw, M_BTCX_ECI1(wlc_hw));
		wlc_hw->btc->task = (int_val & BT_TYPE_MASK) >> BT_TYPE_OFFSET;
	}

	return wlc_hw->btc->task;
}

int
wlc_bmac_btc_wire_get(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->btc->wire;
}

int
wlc_bmac_btc_wire_set(wlc_hw_info_t *wlc_hw, int btc_wire)
{
	/* Has to be down. Enforced through iovar flag */
	ASSERT(!wlc_hw->up);

	if (btc_wire > WL_BTC_4WIRE) {
		WL_ERROR(("wl%d: %s: Unsupported wire setting btc_wire: %d\n", wlc_hw->unit,
			__FUNCTION__, btc_wire));
		return BCME_BADARG;
	}

	/* default to 4-wire ucode if 3-wire boardflag is set or
	 * - M93 or ECI is enabled
	 * else default to 2-wire
	 */
	if (btc_wire == WL_BTC_DEFWIRE) {
		/* Use the boardflags to finally fix the setting for
		 * boards with correct flags
		 */
		if (BCMCOEX_ENAB_BMAC(wlc_hw))
			wlc_hw->btc->wire = WL_BTC_3WIRE;
		else if (wlc_hw->boardflags2 & BFL2_BTCLEGACY) {
			if (wlc_hw->boardflags2 & BFL2_BTC3WIREONLY)
				wlc_hw->btc->wire = WL_BTC_3WIRE;
			else
				wlc_hw->btc->wire = WL_BTC_4WIRE;
		} else
			wlc_hw->btc->wire = WL_BTC_2WIRE;
	}
	else
		wlc_hw->btc->wire = btc_wire;
	/* flush ucode_loaded so the ucode download will happen again to pickup the right ucode */
	wlc_hw->ucode_loaded = FALSE;

	wlc_bmac_btc_gpio_configure(wlc_hw);

	return BCME_OK;
} /* wlc_bmac_btc_wire_set */

int
wlc_bmac_btc_flags_get(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->btc->flags;
}

static void
wlc_bmac_btc_flags_upd(wlc_hw_info_t *wlc_hw, bool set_clear, uint16 val, uint8 idx, uint16 mask)
{
	if (set_clear) {
		wlc_hw->btc->flags |= val;
		wlc_bmac_mhf(wlc_hw, idx, mask, mask, WLC_BAND_2G);
	} else {
		wlc_hw->btc->flags &= ~val;
		wlc_bmac_mhf(wlc_hw, idx, mask, 0, WLC_BAND_2G);
	}
}

int
wlc_bmac_btc_flags_idx_get(wlc_hw_info_t *wlc_hw, int int_val)
{
	uint8 idx = 0;
	uint16 mask = 0;

	if (!wlc_bmac_btc_flags_ucode((uint8)int_val, &idx, &mask))
		return 0xbad;

	return (wlc_bmac_mhf_get(wlc_hw, idx, WLC_BAND_2G) & mask) ? 1 : 0;
}

int
wlc_bmac_btc_flags_idx_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2)
{
	uint8 idx = 0;
	uint16 mask = 0;

	if (!wlc_bmac_btc_flags_ucode((uint8)int_val, &idx, &mask))
		return BCME_BADARG;

	if (int_val2)
		wlc_bmac_btc_flags_upd(wlc_hw, TRUE, (uint16)(int_val2 << int_val), idx, mask);
	else
		wlc_bmac_btc_flags_upd(wlc_hw, FALSE, (uint16)(1 << int_val), idx, mask);

	return BCME_OK;
}

void
wlc_bmac_btc_stuck_war50943(wlc_hw_info_t *wlc_hw, bool enable)
{
	if (enable) {
		wlc_hw->btc->stuck_detected = FALSE;
		wlc_hw->btc->stuck_war50943 = TRUE;
		wlc_bmac_mhf(wlc_hw, MHF3, MHF3_BTCX_DELL_WAR, MHF3_BTCX_DELL_WAR, WLC_BAND_ALL);
	} else {
		wlc_hw->btc->stuck_war50943 = FALSE;
		wlc_bmac_mhf(wlc_hw, MHF3, MHF3_BTCX_DELL_WAR, 0, WLC_BAND_ALL);
	}
}

int
wlc_bmac_btc_params_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2)
{
	/* btc_params with indices > 1000 are stored in FW.
	 * First check to see whether this is a FW btc_param.
	 */
	if (int_val >= BTC_PARAMS_FW_START_IDX) {
		if (!(wlc_hw->btc->wlc_btc_params_fw)) return BCME_ERROR;
		int_val -= BTC_PARAMS_FW_START_IDX; /* Normalize to a 0-based index */
		if (int_val < BTC_FW_MAX_INDICES) {
			wlc_hw->btc->wlc_btc_params_fw[int_val] = (uint16)int_val2;
			return BCME_OK;
		} else {
			return BCME_BADADDR;
		}
	} else {
		/* If shmem is powered down & wlc_btc_params cached values exist,
		 * then update the relevant cached value based on the int_val index
		 */
		if (!(wlc_hw->up)) {
			if (!(wlc_hw->btc->wlc_btc_params))
				return BCME_ERROR;
			if (int_val < M_BTCX_BACKUP_SIZE) {
				wlc_hw->btc->wlc_btc_params[int_val] = (uint16)int_val2;
				return BCME_OK;
			} else {
				return BCME_BADADDR;
			}
		} else {
			if (!wlc_bmac_btc_param_to_shmem(wlc_hw, (uint32*)&int_val)) {
				return BCME_BADARG;
			}
		wlc_bmac_write_shm(wlc_hw, (uint16)int_val, (uint16)int_val2);
		return BCME_OK;
		}
	}
}

int
wlc_bmac_btc_params_get(wlc_hw_info_t *wlc_hw, int int_val)
{
	/* btc_params with indices > 1000 are stored in FW.
	 * First check to see whether this is a FW btc_param.
	 */
	if (int_val >= BTC_PARAMS_FW_START_IDX) {
		if (!(wlc_hw->btc->wlc_btc_params_fw))
			return BCME_ERROR;
		int_val -= BTC_PARAMS_FW_START_IDX; /* Normalize to a 0-based index */
		if (int_val < BTC_FW_MAX_INDICES) {
			return wlc_hw->btc->wlc_btc_params_fw[int_val];
		} else {
			return 0xbad;
		}
	} else {
		/* If shmem is powered down & wlc_btc_params cached values exist,
		 * then read from the relevant cached value based on the int_val index
		 */
		if (!(wlc_hw->up)) {
			if (!(wlc_hw->btc->wlc_btc_params))
				return 0xbad;
			if (int_val < M_BTCX_BACKUP_SIZE) {
				return wlc_hw->btc->wlc_btc_params[int_val];
			} else {
				return 0xbad;
			}
		} else {
	if (!wlc_bmac_btc_param_to_shmem(wlc_hw, (uint32*)&int_val)) {
		return 0xbad;
	}
			return wlc_bmac_read_shm(wlc_hw, (uint16)int_val);
		}
	}
}

#ifdef DONGLEBUILD
void
BCMUNINITFN(wlc_bmac_btc_params_save)(wlc_hw_info_t *wlc_hw)
{
	uint16 index;
	uint16 bt_shm_addr = wlc_hw->btc->bt_shm_addr;
	uint16* wlc_btc_params = wlc_hw->btc->wlc_btc_params;

	if (!(wlc_hw->btc->btc_init_flag)) {
		return;
	}

	/* core clk should be enabled for shared memory to be read */
	if (wlc_hw->clk) {
		/* Save btc shmem values before going down */
		if (wlc_hw->btc->wlc_btc_params) {
			/* Get pointer to the BTCX shm block */
			if (0 != (bt_shm_addr = 2 * wlc_bmac_read_shm(wlc_hw,
					M_BTCX_BLK_PTR(wlc_hw)))) {
				for (index = 0; index < M_BTCX_BACKUP_SIZE; index++)
					wlc_btc_params[index] =
						wlc_bmac_read_shm(wlc_hw, bt_shm_addr+(index*2));
			}
		}
	} else {
		WL_ERROR(("%s:Core clk not enabled: ASSERT\n", __FUNCTION__));
		ASSERT(wlc_hw->clk);
	}
}
#endif /* DONGLEBUILD */

static void
wlc_bmac_gpio_configure(wlc_hw_info_t *wlc_hw, bool is_uppath)
{
#ifndef BCMPCIDEV
	/* for X87 module; need to change power throttle pin to output
	 * tri-state so that leakage current is minimized.
	 */
	if ((wlc_hw->sih->boardvendor == VENDOR_APPLE) &&
	    (wlc_hw->sih->chip == BCM43602_CHIP_ID)) {
		if (!is_uppath) {
			/* Set to Input Mode */
			si_gpioouten(wlc_hw->sih, BOARD_GPIO_2_WLAN_PWR, 0, GPIO_HI_PRIORITY);
			/* Force the output High to reduce internal leakage via output buffer */
			si_gpioout(wlc_hw->sih, BOARD_GPIO_2_WLAN_PWR,
				BOARD_GPIO_2_WLAN_PWR, GPIO_HI_PRIORITY);
		}
	}
#endif /* BCMPCIDEV */
}

/** configure 2/3/4 wire coex gpio for newer chips */
static void
wlc_bmac_btc_gpio_configure(wlc_hw_info_t *wlc_hw)
{
	if (wlc_hw->boardflags & BFL_BTC2WIRE_ALTGPIO) {
		wlc_hw->btc->gpio_mask = BOARD_GPIO_BTCMOD_OUT | BOARD_GPIO_BTCMOD_IN;
		wlc_hw->btc->gpio_out = BOARD_GPIO_BTCMOD_OUT;
	} else if (wlc_hw->btc->wire != WL_BTC_3WIRE) {
		wlc_hw->btc->gpio_mask = wlc_hw->btc->gpio_out  = 0;
	}
}

/** Lower BTC GPIO through ChipCommon when BTC is OFF or D11 MAC is in reset or on powerup */
static void
wlc_bmac_btc_gpio_disable(wlc_hw_info_t *wlc_hw)
{
	uint32 gm, go;
	si_t *sih;
	bool xtal_set = FALSE;

	if (!wlc_hw->sbclk) {
		wlc_bmac_xtal(wlc_hw, ON);
		xtal_set = TRUE;
	}

	/* Proceed only if BTC GPIOs had been configured */
	if (wlc_hw->btc->gpio_mask == 0)
		return;

	sih = wlc_hw->sih;

	gm = wlc_hw->btc->gpio_mask;
	go = wlc_hw->btc->gpio_out;

	/* Set the control of GPIO back and lower only GPIO OUT pins and not the ones that
	 * are supposed to be IN
	 */
	si_gpiocontrol(sih, gm, 0, GPIO_DRV_PRIORITY);
	/* configure gpio to input to float pad */
	si_gpioouten(sih, gm, 0, GPIO_DRV_PRIORITY);
	si_gpioout(sih, go, 0, GPIO_DRV_PRIORITY);

	if (wlc_hw->clk)
		AND_REG(wlc_hw->osh, D11_PSM_GPIOEN(wlc_hw), ~wlc_hw->btc->gpio_out);

	/* BMAC_NOTE: PCI_BUS check here is actually not relevant; there is nothing PCI
	 * bus specific here it was only meant to be compile time optimization. Now it's
	 * true that it may not anyway be applicable to 4323, but need to see if there are
	 * any more places like this
	 */
	/* On someboards, which give GPIOs to UART via strapping,
	 * GPIO_BTC_OUT is not directly controlled by gpioout on CC
	 */
	if ((BUSTYPE(sih->bustype) == PCI_BUS) && (gm & BOARD_GPIO_BTC_OUT))
		si_btcgpiowar(sih);

	if (xtal_set)
		wlc_bmac_xtal(wlc_hw, OFF);
} /* wlc_bmac_btc_gpio_disable */

/** Set BTC GPIO through ChipCommon when BTC is ON */
void
wlc_bmac_btc_gpio_enable(wlc_hw_info_t *wlc_hw)
{
	uint32 gm, gi;
	si_t *sih;

	ASSERT(wlc_hw->clk);

	/* Proceed only if GPIO-based BTC is configured */
	if (wlc_hw->btc->gpio_mask == 0)
		return;

	sih = wlc_hw->sih;

	gm = wlc_hw->btc->gpio_mask;
	gi = (~wlc_hw->btc->gpio_out) & wlc_hw->btc->gpio_mask;

	OR_REG(wlc_hw->osh, D11_PSM_GPIOEN(wlc_hw), wlc_hw->btc->gpio_out);
	/* Clear OUT enable from GPIOs that the driver expects to be IN */
	si_gpioouten(sih, gi, 0, GPIO_DRV_PRIORITY);

	si_gpiopull(wlc_hw->sih, GPIO_PULLUP, gm, 0);
	si_gpiopull(wlc_hw->sih, GPIO_PULLDN, gm, 0);
	si_gpiocontrol(sih, gm, gm, GPIO_DRV_PRIORITY);
}

#if defined(BCMDBG)
static void
wlc_bmac_btc_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "BTC---\n");
	bcm_bprintf(b, "btc_mode %d btc_wire %d btc_flags %d "
		"btc_gpio_mask %d btc_gpio_out %d btc_stuck_detected %d btc_stuck_war50943 %d "
		"bt_shm_add %d bt_period %d bt_active %d\n",
		wlc_hw->btc->mode, wlc_hw->btc->wire, wlc_hw->btc->flags, wlc_hw->btc->gpio_mask,
		wlc_hw->btc->gpio_out, wlc_hw->btc->stuck_detected, wlc_hw->btc->stuck_war50943,
		wlc_hw->btc->bt_shm_addr, wlc_hw->btc->bt_period, wlc_hw->btc->bt_active);
}
#endif // endif

#if defined(BCMDBG) || defined(BCMDBG_PHYDUMP)
static void
wlc_bmac_suspend_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b)
{
	bmac_suspend_stats_t* stats = wlc_hw->suspend_stats;
	uint32 suspend_time = stats->suspended;
	uint32 unsuspend_time = stats->unsuspended;
	uint32 ratio = 0;
	uint32 timenow = R_REG(wlc_hw->osh, D11_TSFTimerLow(wlc_hw));
	bool   suspend_active = stats->suspend_start > stats->suspend_end;

	bcm_bprintf(b, "bmac suspend stats---\n");
	bcm_bprintf(b, "Suspend count: %d%s\n", stats->suspend_count,
	            suspend_active ? " ACTIVE" : "");

	if (suspend_active) {
		if (timenow > stats->suspend_start) {
			suspend_time += (timenow - stats->suspend_start) / 100;
			stats->suspend_start = timenow;
		}
	} else {
		if (timenow > stats->suspend_end) {
			unsuspend_time += (timenow - stats->suspend_end) / 100;
			stats->suspend_end = timenow;
		}
	}

	bcm_bprintf(b, "    Suspended: %9d millisecs\n", (suspend_time + 5)/10);
	bcm_bprintf(b, "  Unsuspended: %9d millisecs\n", (unsuspend_time + 5)/10);
	bcm_bprintf(b, "  Max suspend: %9d millisecs\n", (stats->suspend_max + 5)/10);
	bcm_bprintf(b, " Mean suspend: %9d millisecs\n",
	           (suspend_time / (stats->suspend_count ? stats->suspend_count : 1) + 5)/10);

	/* avoid problems with arithmetric overflow */
	while ((suspend_time > (1 << 26)) || (unsuspend_time > (1 << 26))) {
		suspend_time >>= 1;
		unsuspend_time >>= 1;
	}

	if (suspend_time && unsuspend_time) {
		ratio = (suspend_time + unsuspend_time) * 10;
		ratio /= suspend_time;

		if (ratio > 0) {
			ratio = 100000 / ratio;
		}
		ratio = (ratio + 5)/10;
	}

	bcm_bprintf(b, "Suspend ratio: %3d / 1000\n", ratio);

	stats->suspend_count = 0;
	stats->unsuspended = 0;
	stats->suspended = 0;
	stats->suspend_max = 0;
}
#endif // endif

/* BTC stuff END */

#ifdef STA
/** Change PCIE War override for some platforms */
void
wlc_bmac_pcie_war_ovr_update(wlc_hw_info_t *wlc_hw, uint8 aspm)
{
	if ((BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) &&
		(BUSCORETYPE(wlc_hw->sih->buscoretype) == PCIE_CORE_ID)) {
		si_pcie_war_ovr_update(wlc_hw->sih, aspm);
	}
}

void
wlc_bmac_pcie_power_save_enable(wlc_hw_info_t *wlc_hw, bool enable)
{
	if ((BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) &&
		(BUSCORETYPE(wlc_hw->sih->buscoretype) == PCIE_CORE_ID)) {
		si_pcie_power_save_enable(wlc_hw->sih, enable);
	}
}
#endif /* STA */

#ifdef BCMUCDOWNLOAD
/** function to write ucode to ucode memory */
int
wlc_handle_ucodefw(wlc_info_t *wlc, wl_ucode_info_t *ucode_buf)
{
	/* for first chunk turn on the clock & do core reset */
	if (ucode_buf->chunk_num == 1) {
		wlc_bmac_xtal(wlc->hw, ON);
		wlc_bmac_corereset(wlc->hw, WLC_USE_COREFLAGS);
	}
	/* write ucode chunk to ucode memory */
	wlc_ucode_write(wlc->hw,  (uint32 *)(&ucode_buf->data_chunk[0]), ucode_buf->chunk_len);

	return 0;
}

/**
 * function to handle initvals & bsinitvals. Initvals chunks are accumulated
 * in the driver & kept allocated till 'wl up'. During 'wl up' initvals
 * are written to the memory & then buffer is freed. Even though bsinitvals
 * implementation is also present it is not being downloaded from the host
 * since the size is small & will not be reclaimed if it is dual band image
 */
int
wlc_handle_initvals(wlc_info_t *wlc, wl_ucode_info_t *ucode_buf)
{
	if (ucode_buf->chunk_num == 1) {
		initvals_len = ucode_buf->num_chunks * ucode_buf->chunk_len * sizeof(uint8);
		initvals_ptr = (d11init_t *)MALLOC(wlc->osh, initvals_len);
	}

	bcopy(ucode_buf->data_chunk, (uint8*)initvals_ptr + cumulative_len, ucode_buf->chunk_len);
	cumulative_len += ucode_buf->chunk_len;

	/* when last chunk is received call the write function  */
	if (ucode_buf->chunk_num == ucode_buf->num_chunks)
		wlc->is_initvalsdloaded = TRUE;
	return 0;
}

/**
 * Generic function to handle different downloadable parts like ucode fw
 * & initvals & bsinitvals
 */
int
wlc_process_ucodeparts(wlc_info_t *wlc, uint8 *buf_to_process)
{
	wl_ucode_info_t *ucode_buf = (wl_ucode_info_t *)buf_to_process;
	if (ucode_buf->ucode_type == INIT_VALS)
		wlc_handle_initvals(wlc, ucode_buf);
	else
		wlc_handle_ucodefw(wlc, ucode_buf);
	return 0;
}
#endif /* BCMUCDOWNLOAD */

/**
 * The function is supposed to enable/disable MI_TBTT or M_P2P_I_PRE_TBTT.
 * But since there is no control over M_P2P_I_PRE_TBTT interrupt ,
 * this is achieved by enabling/disabling MI_P2P interrupt as a whole, though
 * that is not the actual intention. The assumption here is if
 * M_P2P_I_PRE_TBTT is no required, no other P2P interrupt will be required.
 * Do not use this function to enable/disable MI_P2P in other conditions.
 * Smply use wlc_bmac_set_defmacintmask() if required.
 */
void
wlc_bmac_enable_tbtt(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val)
{
	wlc_hw->tbttenablemask = (wlc_hw->tbttenablemask & ~mask) | (val & mask);

	if (wlc_hw->tbttenablemask)
		wlc_bmac_set_defmacintmask(wlc_hw, MI_P2P|MI_TBTT, MI_P2P|MI_TBTT);
	else
		wlc_bmac_set_defmacintmask(wlc_hw, MI_P2P|MI_TBTT, ~(MI_P2P|MI_TBTT));
}

#ifdef WLCHANIM_US
int  wlc_bmaq_lq_stats_read(wlc_hw_info_t *wlc_hw, chanim_cnt_us_t *chanim_cnt_us)
{
	if (D11REV_GE(wlc_hw->corerev, 40)) {
		chanim_cnt_us->rxcrs_pri20 = wlc_bmac_read_counter(wlc_hw, 0,
				M_CCA_RXPRI_LO(wlc_hw), M_CCA_RXPRI_HI(wlc_hw));
		chanim_cnt_us->rxcrs_sec20 = wlc_bmac_read_counter(wlc_hw, 0,
				M_CCA_RXSEC20_LO(wlc_hw), M_CCA_RXSEC20_HI(wlc_hw));
		chanim_cnt_us->rxcrs_sec40 = wlc_bmac_read_counter(wlc_hw, 0,
				M_CCA_RXSEC40_LO(wlc_hw), M_CCA_RXSEC40_HI(wlc_hw));
	}
	return 0;
}
#endif /* WLCHANIM_US */

void
wlc_bmac_set_defmacintmask(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val)
{
	wlc_hw->defmacintmask = (wlc_hw->defmacintmask & ~mask) | (val & mask);
}

#ifdef BPRESET
#include <wlc_scb.h>
void
wlc_full_reset(wlc_hw_info_t *wlc_hw, uint32 val)
{
	osl_t *osh;
	uint32 bar0win;
	uint32 bar0win_after;
	int i;
#ifdef BCMDBG
	uint32 start = OSL_SYSUPTIME();
#endif // endif
	uint tmp_bcn_li_dtim;
	uint32 mac_intmask;
	wlc_info_t *wlc = wlc_hw->wlc;
	int ac;

	if (!BPRESET_ENAB(wlc->pub)) {
		WL_ERROR(("wl%d: BPRESET not enabled, do nothing!\n", wlc->pub->unit));
		return;
	}

	/*
	 * 0:	Just show we are alive
	 * 1:	Basic big hammer
	 * 2:	Bigger hammer, big hammer plus backplane reset
	 * 4:	Extra debugging after wl_init
	 * 8:	Issue wl_down() & wl_up() after wl_init
	 */
	WL_ERROR(("wl%d: %s(0x%x): starting backplane reset\n",
	           wlc_hw->unit, __FUNCTION__, val));

	osh = wlc_hw->osh;

	if (val == 0)
		return;

	/* stop DMA */
	if (!PIO_ENAB(wlc_hw->wlc->pub)) {
		for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); i++) {
			if (wlc_hw->di[i]) {
				if (!dma_txreset(wlc_hw->di[i])) {
					WL_ERROR(("wl%d: %s: dma_txreset[%d]: cannot stop dma\n",
						wlc_hw->unit, __FUNCTION__, i));
					WL_HEALTH_LOG(wlc_hw->wlc, DMATX_ERROR);
				}
				wlc_upd_suspended_fifos_clear(wlc_hw, i);
			}
		}
		if ((wlc_hw->di[RX_FIFO]) && (!wlc_dma_rxreset(wlc_hw, RX_FIFO))) {
			WL_ERROR(("wl%d: %s: dma_rxreset[%d]: cannot stop dma\n",
			          wlc_hw->unit, __FUNCTION__, RX_FIFO));
			WL_HEALTH_LOG(wlc_hw->wlc, DMARX_ERROR);
		}
	} else {
		for (i = 0; i < NFIFO_LEGACY; i++) {
			if (wlc_hw->pio[i]) {
				wlc_pio_reset(wlc_hw->pio[i]);
			}
		}
	}

	WL_NONE(("wl%d: %s: up %d, hw->up %d, sbclk %d, clk %d, hw->clk %d, fastclk %d\n",
	         wlc_hw->unit, __FUNCTION__, wlc_hw->wlc->pub->up, wlc_hw->up,
	         wlc_hw->sbclk, wlc_hw->wlc->clk, wlc_hw->clk, wlc_hw->forcefastclk));

	if (val & 2) {
		/* cause chipc watchdog */
		WL_INFORM(("wl%d: %s: starting chipc watchdog\n",
		           wlc_hw->unit, __FUNCTION__));

		bar0win = OSL_PCI_READ_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32));

		/* Stop interrupt handling */
		wlc_hw->macintmask = 0;

		wlc_bmac_set_ctrl_SROM(wlc_hw);
		if (R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC) {
			wlc_bmac_suspend_mac_and_wait(wlc_hw);
		}

		/* Write the watchdog */
		si_corereg(wlc_hw->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0, 100);

		/* Srom read takes ~12mS */
		OSL_DELAY(20000);

		bar0win_after = OSL_PCI_READ_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32));

		if (bar0win_after != bar0win) {
			WL_ERROR(("wl%d: %s: bar0win before %08x, bar0win after %08x\n",
			          wlc_hw->unit, __FUNCTION__, bar0win, bar0win_after));
			OSL_PCI_WRITE_CONFIG(osh, PCI_BAR0_WIN, sizeof(uint32), bar0win);
		}

		/* If the core is up, the watchdog did not take effect */
		if (si_iscoreup(wlc_hw->sih))
			WL_ERROR(("wl%d: %s: Core still up after WD\n",
			          wlc_hw->unit, __FUNCTION__));

		/* Fixup the state to say the chip (or at least d11) is down */
		wlc_hw->clk = FALSE;

		/* restore hardware related stuff */
		wlc_bmac_up_prep(wlc_hw);
	}

	WL_INFORM(("wl%d: %s: about to wl_init()\n", wlc_hw->unit, __FUNCTION__));

	tmp_bcn_li_dtim = wlc_hw->wlc->bcn_li_dtim;
	wlc_hw->wlc->bcn_li_dtim = 0;
	wlc_fatal_error(wlc_hw->wlc);	/* big hammer */

	/* Propagate rfaware_lifetime setting to ucode */
	wlc_rfaware_lifetime_set(wlc, wlc->rfaware_lifetime);

	/* for full backplane reset, need to reenable interrupt */
	if (val & 2) {
		/* FULLY enable dynamic power control and d11 core interrupt */
		wlc_clkctl_clk(wlc_hw, CLK_DYNAMIC);
		ASSERT(wlc_hw->macintmask == 0);
		ASSERT(!wlc_hw->sbclk || !si_taclear(wlc_hw->sih, TRUE));
		wl_intrson(wlc_hw->wlc->wl);
	}

	mac_intmask = wlc_intrsoff(wlc_hw->wlc);
	wlc_intrsrestore(wlc_hw->wlc, mac_intmask);

	/* Write WME tunable parameters for retransmit/max rate from wlc struct to ucode */
	for (ac = 0; ac < AC_COUNT; ac++) {
		wlc_bmac_write_shm(wlc_hw, M_AC_TXLMT_ADDR(wlc_hw, ac),
			wlc_hw->wlc->wme_retries[ac]);
	}
	/* sanitize any existing scb rates */
	wlc_scblist_validaterates(wlc);
	/* ensure antenna config is up to date */
	wlc_stf_phy_txant_upd(wlc);

	wlc_hw->wlc->bcn_li_dtim = tmp_bcn_li_dtim;

	WL_INFORM(("wl%d: %s: back from wl_init()\n", wlc_hw->unit, __FUNCTION__));
	WL_NONE(("wl%d: %s: up %d, hw->up %d, sbclk %d, clk %d, hw->clk %d, fastclk %d\n",
	         wlc_hw->unit, __FUNCTION__, wlc_hw->wlc->pub->up, wlc_hw->up,
	         wlc_hw->sbclk, wlc_hw->wlc->clk, wlc_hw->clk, wlc_hw->forcefastclk));

	if (val & 8) {
		WL_INFORM(("wl%d: %s: calling wl_down()\n", wlc_hw->unit, __FUNCTION__));
		wl_down(wlc_hw->wlc->wl);

		WL_INFORM(("wl%d: %s: calling wl_up()\n", wlc_hw->unit, __FUNCTION__));
		wl_up(wlc_hw->wlc->wl);
	}
	WL_INFORM(("wl%d: %s(0x%x): done in %dmS\n", wlc_hw->unit, __FUNCTION__, val,
	           OSL_SYSUPTIME() - start));
} /* wlc_full_reset */
#endif	/* BPRESET */

/** Returns 1 if any error is detected in TXFIFO configuration */
static bool
BCMINITFN(wlc_bmac_txfifo_sz_chk)(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh;
	uint16 fifo_nu = 0;
	uint16 txfifo_cmd_org = 0;
	uint16 txfifo_cmd = 0;

	uint16 txfifo_def = 0;
	uint16 txfifo_def1 = 0;

	/* Index of "256 byte" block where this FIFO starts */
	uint16 txfifo_start = 0;
	/* Index of "256 byte" block where this FIFO ends */
	uint16 txfifo_end = 0;
	/* Number of "256 byte" blocks used so far */
	uint16 txfifo_used = 0;
	/* Total number of "256 byte" blocks available in chip */
	uint16 txfifo_total;
	bool err = 0;

	osh = wlc_hw->osh;

	txfifo_total = wlc_bmac_get_txfifo_size_kb(wlc_hw);

	/* Store current value of xmtfifocmd for restoring later */
	txfifo_cmd_org = R_REG(osh, D11_xmtfifocmd(wlc_hw));

	/* Read all configured FIFO size entries and check if they are valid */
	for (fifo_nu = 0; fifo_nu < NFIFO_LEGACY; fifo_nu++) {
		/* Select the FIFO */
		txfifo_cmd = ((txfifo_cmd_org & ~TXFIFOCMD_FIFOSEL_SET(-1)) |
			TXFIFOCMD_FIFOSEL_SET(fifo_nu));
		W_REG(osh, D11_xmtfifocmd(wlc_hw), txfifo_cmd);

		/* Read the current configured size */
		txfifo_def = R_REG(osh, D11_xmtfifodef(wlc_hw));
		txfifo_def1 = R_REG(osh, D11_xmtfifodef1(wlc_hw));

		/* Validate the size of the template fifo too */
		if (fifo_nu == 0) {
			if (TXFIFO_FIFO_START(txfifo_def, txfifo_def1) == 0) {
				WL_ERROR(("wl%d: %s: Template FIFO size is zero\n",
				          wlc_hw->unit, __FUNCTION__));
				ASSERT(0);
				err = 1;
				break;
			}

			/* End of template FIFO is just before start of fifo0 */
			txfifo_end = (TXFIFO_FIFO_START(txfifo_def, txfifo_def1) - 1);
			txfifo_used += ((txfifo_end - txfifo_start) + 1);
		}

		txfifo_start = TXFIFO_FIFO_START(txfifo_def, txfifo_def1);
		/* Check FIFO overlap with previous FIFO */
		if (txfifo_start < txfifo_end) {
			WL_ERROR(("wl%d: %s: FIFO %d overlaps with FIFO %d\n",
				wlc_hw->unit, __FUNCTION__, fifo_nu,
				((fifo_nu == 0) ? -1 : (fifo_nu-1))));
			ASSERT(0);
			err = 1;
			break;

		/* If consecutive blocks are not contiguous, this function cannot check overlap */
		} else if (txfifo_start != (txfifo_end + 1)) {
			WL_ERROR(("wl%d: %s: FIFO %d not contiguous with previous FIFO."
			"Cannot check overlap. (start=%d prev_end=%d)\n",
				wlc_hw->unit, __FUNCTION__, fifo_nu,
				txfifo_start, txfifo_end));
			ASSERT(0);
			err = 1;
			break;
		}
		txfifo_end = TXFIFO_FIFO_END(txfifo_def, txfifo_def1);
		/* Fifo should be configured to atleast 1 block */
		if (txfifo_end < txfifo_start) {
			WL_ERROR(("wl%d: %s: FIFO %d config invalid. start=%d and end=%d\n",
				wlc_hw->unit, __FUNCTION__, fifo_nu,
				txfifo_start, txfifo_end));
			ASSERT(0);
			err = 1;
			break;
		}
		txfifo_used += ((txfifo_end - txfifo_start) + 1);
		/* At any point, FIFO size used should not exceed capacity */
		if (txfifo_used > txfifo_total) {
			WL_ERROR(("wl%d: %s: FIFO %d config causes memblk usage %d"
			"to exceed chip capacity %d\n",
				wlc_hw->unit, __FUNCTION__, fifo_nu,
				txfifo_used, txfifo_total));
			ASSERT(0);
			err = 1;
			break;
		}
		WL_INFORM(("wl%d: %s: FIFO %d block config, "
		"start=%d end=%d sz=%d used=%d avail=%d\n",
			wlc_hw->unit, __FUNCTION__, fifo_nu,
			txfifo_start, txfifo_end,
			((txfifo_end - txfifo_start) + 1),
			txfifo_used, (txfifo_total - txfifo_used)));
	}
	/* Restore xmtfifocmd configuration */
	W_REG(osh, D11_xmtfifocmd(wlc_hw), txfifo_cmd_org);

	return err;
} /* wlc_bmac_txfifo_sz_chk */

#if defined(BCMDBG) && !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
char* wlc_dbg_get_hw_timestamp(void)
{
	static char timestamp[20];
	static uint32 nestcount = 0;

	if (nestcount == 0 && wlc_info_time_dbg)
	{
		struct bcmstrbuf b;
		uint32 t;
		uint32 mins;
		uint32 secs;
		uint32 fraction;

		nestcount++;

		t = wlc_bmac_read_usec_timer(wlc_info_time_dbg->hw);
		secs = t / 1000000;
		fraction = (t - secs*1000000 + 5) / 10;
		mins = secs / 60;
		secs -= mins * 60;

		bcm_binit(&b, timestamp, sizeof(timestamp));
		bcm_bprintf(&b, "[%d:%02d.%05d]:", mins, secs, fraction);

		nestcount--;
		return timestamp;
	}
	return "";
} /* wlc_dbg_get_hw_timestamp */
#endif /* BCMDBG && !BCMDBG_EXCLUDE_HW_TIMESTAMP */

static int
BCMINITFN(wlc_corerev_fifosz_validate)(wlc_hw_info_t *wlc_hw, uint16 *buf)
{
	int i = 0, err = 0;

	ASSERT(D11REV_LT(wlc_hw->corerev, 40));
	ASSERT(TX_ATIM_FIFO < NFIFO_LEGACY);

	/* check txfifo allocations match between ucode and driver */
	buf[TX_AC_BE_FIFO] = wlc_bmac_read_shm(wlc_hw, M_FIFOSIZE0(wlc_hw));
	if (buf[TX_AC_BE_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_BE_FIFO]) {
		i = TX_AC_BE_FIFO;
		err = -1;
	}

	ASSERT(TX_AC_BE_FIFO < ARRAYSIZE(buf));
	buf[TX_AC_VI_FIFO] = wlc_bmac_read_shm(wlc_hw, M_FIFOSIZE1(wlc_hw));
	if (buf[TX_AC_VI_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_VI_FIFO]) {
		i = TX_AC_VI_FIFO;
	        err = -1;
	}
	buf[TX_AC_BK_FIFO] = wlc_bmac_read_shm(wlc_hw, M_FIFOSIZE2(wlc_hw));
	buf[TX_AC_VO_FIFO] = (buf[TX_AC_BK_FIFO] >> 8) & 0xff;
	buf[TX_AC_BK_FIFO] &= 0xff;
	if (buf[TX_AC_BK_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_BK_FIFO]) {
		i = TX_AC_BK_FIFO;
	        err = -1;
	}
	if (buf[TX_AC_VO_FIFO] != wlc_hw->xmtfifo_sz[TX_AC_VO_FIFO]) {
		i = TX_AC_VO_FIFO;
		err = -1;
	}
	buf[TX_BCMC_FIFO] = wlc_bmac_read_shm(wlc_hw, M_FIFOSIZE3(wlc_hw));
	buf[TX_ATIM_FIFO] = (buf[TX_BCMC_FIFO] >> 8) & 0xff;
	buf[TX_BCMC_FIFO] &= 0xff;
	if (buf[TX_BCMC_FIFO] != wlc_hw->xmtfifo_sz[TX_BCMC_FIFO]) {
		i = TX_BCMC_FIFO;
		err = -1;
	}
	if (buf[TX_ATIM_FIFO] != wlc_hw->xmtfifo_sz[TX_ATIM_FIFO]) {
		i = TX_ATIM_FIFO;
		err = -1;
	}
	if (err != 0) {
		WL_ERROR(("wlc_coreinit: txfifo mismatch: ucode size %d driver size %d index %d\n",
			buf[i], wlc_hw->xmtfifo_sz[i], i));
		ASSERT(0);
	}

	BCM_REFERENCE(i);

	return err;
} /* wlc_corerev_fifosz_validate */

static const bmc_params_t bmc_params_42 = {0, 0, 0, 0, 0, 11, {32, 32, 32, 32, 32, 32}};

/* FIFO1 is allocated to 10 512 buffers for PCIe, during PM states only FIFO1 is active */
static const bmc_params_t bmc_params_48 =
#if  defined(BCMSDIODEV_ENABLED) || defined(BCMPCIEDEV_ENABLED)
	{1, 128, 128, 1, 1, 0, {16, 16, 16, 16, 16, 0, 118, 0, 10}};
#else
	/* NIC, USB, BMAC targets  */
	{1, 128, 128, 1, 1, 0, {32, 32, 32, 32, 32, 0, 20, 0, 10}};
#endif // endif

static const bmc_params_t bmc_params_49 =
	{1, 192, 192, 1, 1, 0, {32, 32, 32, 32, 10, 0, 92, 0, 10}};
static const bmc_params_t bmc_params_56 =
	{1, 128, 128, 1, 1, 0, {16, 16, 16, 16, 16, 0, 32, 0, 10}};
static const bmc_params_t bmc_params_61m =
	{1, 128, 128, 1, 1, 0, {16, 32, 16, 16, 16, 2, 96, 0, 32}};
static const bmc_params_t bmc_params_61a =
	{1, 128, 128, 1, 1, 0, {16, 32, 16, 16, 16, 2, 43, 0, 19}};

static const bmc_params_t bmc_params_64 = { /* corerev 64 uses bmc_params_64 */
	1,   /**< .rxq_in_bm = rx queues are allocated in BM */
	128, /**< .rxq0_buf = rx queue 0: 128 buffers of 512 bytes = 64KB */
	128, /**< .rxq1_buf = rx queue 1: 128 buffers of 512 bytes = 64KB */
	1,   /**< .rxbmmap_is_en = */
	1,   /**< .tx_flowctrl_scheme = new tx flow control: don't preallocate as many buffers */
	0,   /**< .full_thresh = unused field since tx_flowctrl_scheme != 0 */
	{32, 32, 32, 32, 32, 0, 64, 0, 32} /**< .minbufs = per tx fifo BMC parameter */
};

/* XXX 43684a0 TODO: these parameters were taken from 4365 and are incorrect. 43684 params are
 * currently (Jun 2016) unknown.
 */
static const bmc_params_t bmc_params_128 = {
	0,   /**< .rxq_in_bm = rx queues are allocated in BM */
	128, /**< .rxq0_buf = rx queue 0: 128 buffers of 512 bytes = 64KB */
	128, /**< .rxq1_buf = rx queue 1: 128 buffers of 512 bytes = 64KB */
	0,   /**< .rxbmmap_is_en = */
	1,   /**< .tx_flowctrl_scheme = new tx flow control: don't preallocate as many buffers */
	0,   /**< .full_thresh = unused field since tx_flowctrl_scheme != 0 */
	/** .minbufs = per fifo BMC parameter */
	{16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 0 - 15 */
	 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 16 - 31 */
	 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 32 - 47 */
	 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 48 - 63 */
	 16, 16, 16, 16, 16, 16, 0,
	 (512 - 64), /**< rxfifo0 */
#ifdef DONGLEBUILD
	 64}         /**< rxfifo1 */
#else
	 0}          /**< rxfifo1 is unused for NIC mode only chips */
#endif /* DONGLEBUILD */
};

/** 640KB, 1280 buffers, each 512 bytes. 0..37 are TX FIFOs, 38 is template, 39/40 are rx FIFOs */
static const bmc_params_t bmc_params_130 = {
	0,   /**< .rxq_in_bm = rx queues are allocated in BM */
	0,   /**< .rxq0_buf = rx queue 0: N/A */
	0,   /**< .rxq1_buf = rx queue 1: N/A */
	0,   /**< .rxbmmap_is_en = */
	1,   /**< .tx_flowctrl_scheme = new tx flow control: don't preallocate as many buffers */
	0,   /**< .full_thresh = unused field since tx_flowctrl_scheme != 0 */
	/** .minbufs = per fifo BMC parameter, each buf is 512 bytes */
	{16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 0 - 15 */
	 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, /* 16 - 31 */
	 16, 16, 16, 16, 16, 16, 0,                                      /* 32 - 38 */
	 (256 - 64), /**< rxfifo0 */
	 0}          /**< rxfifo1 is unused for NIC mode only chips */
};

/** 640KB, 1280 buffers, each 512 bytes. 0..21 are TX FIFOs, 22 is template, 23/24 are rx FIFOs */
static const bmc_params_t bmc_params_131 = {
	0,   /**< .rxq_in_bm = rx queues are allocated in BM */
	0,   /**< .rxq0_buf = rx queue 0: N/A */
	0,   /**< .rxq1_buf = rx queue 1: N/A */
	0,   /**< .rxbmmap_is_en = */
	1,   /**< .tx_flowctrl_scheme = new tx flow control: don't preallocate as many buffers */
	0,   /**< .full_thresh = unused field since tx_flowctrl_scheme != 0 */
	{16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,  /* 0 - 15 tx fifo */
	 16, 16, 16, 16, 16, 16,					  /* 16 - 21 tx fifo */
	 0,								  /* 22, template fifo */
	 192,								  /* 23, rx fifo0 */
	 0}								  /* 24, rx fifo1, unused */
};

#if defined(SAVERESTORE) && defined(SR_ESSENTIALS)
extern CONST uint sr_source_codesz_61_main;
extern CONST uint sr_source_codesz_61_aux;
#endif /* defined(SAVERESTORE) && defined(SR_ESSENTIALS) */

/* return the rxfifo size info */
uint
wlc_bmac_rxfifosz_get(wlc_hw_info_t *wlc_hw)
{
	uint rcvfifo = 0;

	if (D11REV_GE(wlc_hw->corerev, 40)) {
		if (wlc_hw->bmc_params != NULL && wlc_hw->bmc_params->rxq_in_bm != 0) {
			rcvfifo = wlc_hw->bmc_params->rxq0_buf * 512;
		}
		else {
			rcvfifo = ((wlc_hw->machwcap & MCAP_RXFSZ_MASK) >>
			           MCAP_RXFSZ_SHIFT) * 2048;
		}
	}

	ASSERT(rcvfifo != 0);
	return rcvfifo;
}

static void
wlc_bmac_bmc_template_allocstatus(wlc_hw_info_t *wlc_hw, uint32 mac_core_unit, int tplbuf)
{
	volatile uint16 *alloc_status;

	ASSERT(wlc_hw != NULL);
	ASSERT(wlc_hw->regs != NULL);

	if (D11REV_GE(wlc_hw->corerev, 128)) {
		if (mac_core_unit == MAC_CORE_UNIT_0) {
			alloc_status = (volatile uint16 *)
				D11_Core0BMCAllocStatusTplate(wlc_hw);
		} else {
			alloc_status = (volatile uint16 *)
				D11_Core1BMCAllocStatusTplate(wlc_hw);
		}
	} else {
		if (mac_core_unit == MAC_CORE_UNIT_0) {
			alloc_status = (volatile uint16 *)
				D11_Core0BMCAllocStatusTID7(wlc_hw);
		} else {
			alloc_status = (volatile uint16 *)
				D11_Core1BMCAllocStatusTID7(wlc_hw);
		}
	}

	SPINWAIT((R_REG(wlc_hw->osh, alloc_status) != (uint)tplbuf), 10);

	if (R_REG(wlc_hw->osh, alloc_status) != (uint)tplbuf) {
		WL_ERROR(("Error BMC buffer allocation: TID 7 of Core unit %d reg 0x%p val 0x%x",
		mac_core_unit, OSL_OBFUSCATE_BUF(alloc_status), R_REG(wlc_hw->osh, alloc_status)));
	}
}

/* select bmc params based on d11 revid */
static const bmc_params_t *
BCMATTACHFN(wlc_bmac_bmc_params)(wlc_hw_info_t *wlc_hw)
{
	if (D11REV_IS(wlc_hw->corerev, 131))
		wlc_hw->bmc_params = &bmc_params_131;
	else if (D11REV_IS(wlc_hw->corerev, 130))
		wlc_hw->bmc_params = &bmc_params_130;
	else if (D11REV_IS(wlc_hw->corerev, 129) ||
	           D11REV_IS(wlc_hw->corerev, 128))
		wlc_hw->bmc_params = &bmc_params_128;
	else if (D11REV_IS(wlc_hw->corerev, 65))
		wlc_hw->bmc_params = &bmc_params_64;
	else if (D11REV_IS(wlc_hw->corerev, 61)) {
		/* Default is main */
		wlc_hw->bmc_params = &bmc_params_61m;

		/* Dual-mac chip could be configured as norsdb */
		if (RSDB_ENAB(wlc_hw->wlc->pub) && (WLC_DUALMAC_RSDB(wlc_hw->wlc->cmn))) {
			if (wlc_bmac_coreunit(wlc_hw->wlc) == DUALMAC_AUX) {
				wlc_hw->bmc_params = &bmc_params_61a;
			}
		}
	} else if (D11REV_IS(wlc_hw->corerev, 49))
		wlc_hw->bmc_params = &bmc_params_49;
	else if (D11REV_IS(wlc_hw->corerev, 56))
		wlc_hw->bmc_params = &bmc_params_56;
	else if (D11REV_IS(wlc_hw->corerev, 48))
		wlc_hw->bmc_params = &bmc_params_48;
	else if (D11REV_IS(wlc_hw->corerev, 42))
		wlc_hw->bmc_params = &bmc_params_42;
	else {
		WL_ERROR(("corerev %d not supported\n", wlc_hw->corerev));
		ASSERT(0);
	}

	return wlc_hw->bmc_params;
}

static void
wlc_bmac_bmc_get_qinfo(wlc_hw_info_t *wlc_hw, int fifo, uint *qix, uint8 *bqseltype)
{
	ASSERT(qix != NULL);
	ASSERT(bqseltype != NULL);

	if ((qix == NULL) || (bqseltype == NULL)) {
		WL_ERROR(("%s: NULL PTR ERROR! qix:%p bqseltype:%p\n", __FUNCTION__,
			qix, bqseltype));
		return;
	}
	if (fifo == D11MAC_BMC_TPL_IDX(wlc_hw->corerev)) {
		*qix = 0;
		*bqseltype = BMCCmd_BQSelType_Templ;
	} else if (fifo == D11MAC_BMC_RXFIFO0_IDX(wlc_hw->corerev)) {
		*qix = 0;
		*bqseltype = BMCCmd_BQSelType_RX;
	} else if (fifo == D11MAC_BMC_RXFIFO1_IDX(wlc_hw->corerev)) {
		*qix = 1;
		*bqseltype = BMCCmd_BQSelType_RX;
	} else {
		*qix = fifo;
		*bqseltype = BMCCmd_BQSelType_TX;
	}
}

/** buffer manager initialization */
static int
BCMINITFN(wlc_bmac_bmc_init)(wlc_hw_info_t *wlc_hw)
{
	osl_t *osh;
	d11regs_t *regs = NULL;
	d11regs_t *sregs = NULL;
	uint32 bmc_ctl;
	uint16 maxbufs, minbufs, alloc_cnt, alloc_thresh, full_thresh, buf_desclen;
	uint16 rxq0_more_bufs = 0;
	int *bmc_fifo_list;
	int num_of_fifo, rxmapfifosz = 0;

	/* used for BMCCTL */
	uint8 bufsize = D11MAC_BMC_BUFSIZE_512BLOCK;
	uint8 loopback = 0;
	uint8 reset_stats = 0;
	uint8 init = 1;

	int i, fifo;
	uint32 fifo_sz;
	int tplbuf = D11MAC_BMC_TPL_NUMBUFS;		/**< template memory  / template fifo */
	uint32 bmc_startaddr = D11MAC_BMC_STARTADDR;
	uint8 doublebufsize = 0;

	int rxq0buf, rxq1buf;
	uint sicoreunit = 0;

	uint8 shortdesc_len, longdesc_len;

	uint8 bqseltype = BMCCmd_BQSelType_TX;
	uint tpl_idx, rxq0_idx, rxq1_idx;

	tpl_idx = D11MAC_BMC_TPL_IDX(wlc_hw->corerev);
	rxq0_idx = D11MAC_BMC_RXFIFO0_IDX(wlc_hw->corerev);
	rxq1_idx = D11MAC_BMC_RXFIFO1_IDX(wlc_hw->corerev);
	bmc_fifo_list = D11MAC_BMC_FIFO_LIST(wlc_hw->corerev);
	osh = wlc_hw->osh;
	regs = wlc_hw->regs;

	ASSERT(wlc_hw->bmc_params != NULL);

	/* CRWLDOT11M-1160, impacts both revs 48, 49 (e.g. 43602) */
	if (D11REV_IS(wlc_hw->corerev, 48) || D11REV_IS(wlc_hw->corerev, 49)) {
		/* Minimum region of TPL FIFO#7 */
		int tpl_fifo_sz = D11MAC_BMC_TPL_BUFS_BYTES;

		/* Need it to be 512B/buffer */
		bufsize = D11MAC_BMC_BUFSIZE_512BLOCK;

		/* start at 80KB, there are fewer buffers available for BMC use */
		bmc_startaddr = D11MAC_BMC_STARTADDR_SRASM;

#if defined(SAVERESTORE) && defined(SR_ESSENTIALS)
		/* When SR is disabled, allot the unused SR ASM space to RXQ0 FIFO */
		if (SR_ESSENTIALS_ENAB()) {
			/* Allot space in TPL FIFO#7 for 4KB aligned SR ASM */
			tpl_fifo_sz += D11MAC_BMC_SRASM_BYTES;
		} else
#endif /* (SAVERESTORE && SR_ESSENTIALS) */
		{
			/* Increase RXQ0 FIFO#6 by SR ASM unused space */
			rxq0_more_bufs = D11MAC_BMC_SRASM_NUMBUFS;
		}

		/* Number of 512 Bytes buffers for TPL FIFO#7 */
		tplbuf = D11MAC_BMC_BUFS_512(tpl_fifo_sz);
	} else if (D11REV_IS(wlc_hw->corerev, 56) || D11REV_IS(wlc_hw->corerev, 61) ||
		wlc_bmac_rsdb_cap(wlc_hw)) {
		bufsize = D11MAC_BMC_BUFSIZE_256BLOCK;
	}
	/* Steps followed:
	* 1. BMC configuration registers are accessed from core-0.
	* 2. This follows template MSDU initialization which is core specific program.
	*/
	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		sicoreunit = si_coreunit(wlc_hw->sih);
		regs = (d11regs_t *)si_d11_switch_addrbase(wlc_hw->sih, 0);
	}
	/* MCTL_IHR_EN is also used in DEVICEREMOVED macro to identify the device state
	* hence it is not suppose to happen at init.
	* forcing this bit for register access.
	*/
	OR_REG(osh, D11_MACCONTROL_ALTBASE(regs, wlc_hw->regoffsets), MCTL_IHR_EN);

	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		sregs = (d11regs_t *)si_d11_switch_addrbase(wlc_hw->sih, MAC_CORE_UNIT_1);
		OR_REG(osh, D11_MACCONTROL_ALTBASE(sregs, wlc_hw->regoffsets), MCTL_IHR_EN);
		si_d11_switch_addrbase(wlc_hw->sih, MAC_CORE_UNIT_0);
	}

	if (bufsize == D11MAC_BMC_BUFSIZE_256BLOCK) { /* core rev 50...127 */
		doublebufsize = 1;
		if (wlc_bmac_rsdb_cap(wlc_hw)) {
			/* Consider max/min to both core templates and sr array area */
			tplbuf = (si_numcoreunits(wlc_hw->sih, D11_CORE_ID) *
				D11MAC_BMC_TPL_NUMBUFS_PERCORE) + D11MAC_BMC_SR_NUMBUFS;
		} else {
#if defined(SAVERESTORE) && defined(SR_ESSENTIALS)
			if (SR_ENAB() && D11REV_IS(wlc_hw->corerev, 61)) {
				uint32 srfwsz = 0, offset = 0;

				if (wlc_hw->macunit == 0) {
					offset = SR_ASM_ADDR_MAIN_4347 <<
						SR_ASM_ADDR_BLK_SIZE_SHIFT;
					srfwsz = sr_source_codesz_61_main;
				} else if (wlc_hw->macunit == 1) {
					offset = SR_ASM_ADDR_AUX_4347 << SR_ASM_ADDR_BLK_SIZE_SHIFT;
					srfwsz = sr_source_codesz_61_aux;
				}
				/* offset includes template region */
				tplbuf = D11MAC_BMC_BUFS_256(offset + srfwsz);
			} else
#endif /* SAVERESTORE */
			{
				tplbuf = D11MAC_BMC_BUFS_256(D11MAC_BMC_TPL_BYTES);
			}
		}
	}

	fifo_sz = wlc_bmac_get_txfifo_size_kb(wlc_hw) * 1024; /* fifo_sz in [byte] units */

	/* Derive BMC memory size from configuration */
	if (D11REV_IS(wlc_hw->corerev, 65) || D11REV_GE(wlc_hw->corerev, 128)) {
		uint32 sysmem_size = si_sysmem_size(wlc_hw->sih);

		if (D11REV_IS(wlc_hw->corerev, 131) || D11REV_IS(wlc_hw->corerev, 130)) {
			ASSERT(fifo_sz == D11MAC_BMC_MACMEM_MAX_BYTES_REV130);
		} else if (BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) {
			/* Derive from RAMSIZE in dongle mode */
#ifdef RAMSIZE
			ASSERT(sysmem_size > RAMSIZE);
			fifo_sz = sysmem_size - RAMSIZE;
#endif /* RAMSIZE */
		} else {
			/* Up to D11MAC_BMC_SYSMEM_MAX_BYTES* for NIC mode */
			if (D11REV_GE(wlc_hw->corerev, 128) &&
				(sysmem_size > D11MAC_BMC_SYSMEM_MAX_BYTES_REV128)) {
				fifo_sz = D11MAC_BMC_SYSMEM_MAX_BYTES_REV128;
			} else if (sysmem_size > D11MAC_BMC_SYSMEM_MAX_BYTES) {
				fifo_sz = D11MAC_BMC_SYSMEM_MAX_BYTES;
			} else
				fifo_sz = sysmem_size;
		}

		/* Prepare BMC setup */
		if (D11REV_GE(wlc_hw->corerev, 128)) {
			/* take the scheduler out of the special debug mode */
			W_REG(osh, D11_TXE_SCHED_CTL_ALTBASE(regs, wlc_hw->regoffsets), 0);
		}
	}

	/* Account for bmc_startaddr which is specified in units of bufsize(256B or 512B) */
	wlc_hw->bmc_maxbufs = (fifo_sz - (bmc_startaddr << 8)) >> (8 + bufsize);

	WL_MAC(("wl%d: %s bmc_size 0x%x bufsize %d maxbufs %d start_addr 0x%04x\n",
		wlc_hw->unit, __FUNCTION__,
		fifo_sz, 1 << (8 + bufsize), wlc_hw->bmc_maxbufs,
			(bmc_startaddr << 8)));

	if (wlc_hw->bmc_params->rxq_in_bm) { /* TRUE for rev(65,66), FALSE for rev>=128 */
		rxq0buf = wlc_hw->bmc_params->rxq0_buf;
		rxq1buf = wlc_hw->bmc_params->rxq1_buf;

		WL_MAC(("wl%d: %s rxq_in_bm ON. rxbmmap is %s. "
			"RXQ size/ptr below are in 32-bit DW.\n",
			wlc_hw->unit, __FUNCTION__,
			wlc_hw->bmc_params->rxbmmap_is_en ? "enabled" : "disabled"));

		if (wlc_hw->bmc_params->rxbmmap_is_en) {  /* RXBMMAP is enabled */
			/* Convert to word addresses, num of buffer * 512 / 4 */
			W_REG(osh, D11_RCV_BM_STARTPTR_Q0_ALTBASE(regs, wlc_hw->regoffsets), 0);
			W_REG(osh, D11_RCV_BM_ENDPTR_Q0_ALTBASE(regs, wlc_hw->regoffsets),
				(rxq0buf << 7) - 1);

			W_REG(osh, D11_RCV_BM_STARTPTR_Q1_ALTBASE(regs, wlc_hw->regoffsets),
				rxq0buf << 7);
			W_REG(osh, D11_RCV_BM_ENDPTR_Q1_ALTBASE(regs, wlc_hw->regoffsets),
				((rxq0buf + rxq1buf) << 7) - 1);

			WL_MAC(("wl%d: RXQ-0 size 0x%04x start_ptr 0x0000 end_ptr 0x%04x\n",
				wlc_hw->unit, (rxq0buf << 7), ((rxq0buf << 7) - 1)));
			WL_MAC(("wl%d: RXQ-1 size 0x%04x start_ptr 0x%04x end_ptr 0x%04x\n",
				wlc_hw->unit, (rxq1buf << 7),
				(rxq0buf << 7), (((rxq0buf + rxq1buf) << 7) - 1)));
		} else {
			/* This corresponds to the case where rxbmmap
			 * is not present/disabled/passthru
			 * Convert to word addresses, num of buffer * 512 / 4
			 */
			W_REG(osh, D11_RCV_BM_STARTPTR_Q0_ALTBASE(regs, wlc_hw->regoffsets),
				tplbuf << 7);
			W_REG(osh, D11_RCV_BM_ENDPTR_Q0(wlc_hw),
				((tplbuf + rxq0buf) << 7) - 1);

			W_REG(osh, D11_RCV_BM_STARTPTR_Q1_ALTBASE(regs, wlc_hw->regoffsets),
				(tplbuf + rxq0buf) << 7);
			W_REG(osh, D11_RCV_BM_ENDPTR_Q1_ALTBASE(regs, wlc_hw->regoffsets),
				((tplbuf + rxq0buf + rxq1buf) << 7) - 1);
			WL_MAC(("wl%d: RXQ-0 size 0x%04x start_ptr 0x%04x end_ptr 0x%04x\n",
				wlc_hw->unit, (rxq0buf << 7),
				(tplbuf << 7), (((tplbuf +rxq0buf) << 7) - 1)));
			WL_MAC(("wl%d: RXQ-1 size 0x%04x start_ptr 0x%0x4 end_ptr 0x%04x\n",
				wlc_hw->unit, (rxq1buf << 7), ((tplbuf + rxq0buf) << 7),
				(((tplbuf + rxq0buf + rxq1buf) << 7) - 1)));

			tplbuf += rxq0buf + rxq1buf;
		}

		/* Reset the RXQs to have the pointers take effect;resets are self-clearing */
		W_REG(osh, D11_RCV_FIFO_CTL_ALTBASE(regs,
			wlc_hw->regoffsets), 0x101);	/* sel and reset q1 */
		W_REG(osh, D11_RCV_FIFO_CTL_ALTBASE(regs,
			wlc_hw->regoffsets), 0x001);	/* sel and reset q0 */
	}

	/* init the total number for now */
	wlc_hw->bmc_nbufs = wlc_hw->bmc_maxbufs;
	if (D11REV_GE(wlc_hw->corerev, 128))
		W_REG(osh, D11_TXE_BMC_CONFIG1_ALTBASE(regs, wlc_hw->regoffsets),
		      wlc_hw->bmc_nbufs);
	else
		W_REG(osh, D11_BMCConfig_ALTBASE(regs, wlc_hw->regoffsets),
		      wlc_hw->bmc_nbufs);
	bmc_ctl = (loopback << BMCCTL_LOOPBACK_SHIFT)	|
	        (bufsize << BMCCTL_TXBUFSIZE_SHIFT)	|
	        (reset_stats << BMCCTL_RESETSTATS_SHIFT)|
	        (init << BMCCTL_INITREQ_SHIFT);

	W_REG(osh, D11_BMCCTL_ALTBASE(regs, wlc_hw->regoffsets), bmc_ctl);
#ifndef BCMQT
	SPINWAIT((R_REG(wlc_hw->osh, D11_BMCCTL_ALTBASE(regs, wlc_hw->regoffsets)) &
		BMC_CTL_DONE), 200);
#else
	SPINWAIT((R_REG(wlc_hw->osh, D11_BMCCTL_ALTBASE(regs, wlc_hw->regoffsets)) &
		BMC_CTL_DONE), 200000);
#endif /* BCMQT */
	if (R_REG(wlc_hw->osh, D11_BMCCTL_ALTBASE(regs, wlc_hw->regoffsets)) & BMC_CTL_DONE) {
		WL_ERROR(("wl%d: bmc init not done yet :-(\n", wlc_hw->unit));
	}

	shortdesc_len = D11_TXH_SHORT_LEN(wlc_hw->wlc);
	longdesc_len = D11AC_TXH_LEN;

	buf_desclen = ((longdesc_len - DOT11_FCS_LEN - AMPDU_DELIMITER_LEN)
		       << BMCDescrLen_LongLen_SHIFT)
		| (shortdesc_len - DOT11_FCS_LEN - AMPDU_DELIMITER_LEN);

	W_REG(osh, D11_BMCDescrLen_ALTBASE(regs, wlc_hw->regoffsets), buf_desclen);

	if (D11REV_IS(wlc_hw->corerev, 131)) {
		num_of_fifo = D11MAC_BMC_MAXFIFOS_IS131;
		WL_MAC(("wl%d: fifo 0-21: tx-fifos, fifo 22: template; "
				"fifo 23/24: rx-fifos\n", wlc_hw->unit));
	} else if (D11REV_IS(wlc_hw->corerev, 130)) {
		num_of_fifo = D11MAC_BMC_MAXFIFOS_IS130; /* includes tx, template and rx fifo's */
		WL_MAC(("wl%d: fifo 0-37: tx-fifos, fifo 38: template; "
				"fifo 39/40: rx-fifos\n", wlc_hw->unit));
	} else if (D11REV_GE(wlc_hw->corerev, 128)) {
		num_of_fifo = D11MAC_BMC_MAXFIFOS_GE128;
		WL_MAC(("wl%d: fifo 0-69: tx-fifos, fifo 70: template; "
				"fifo 71/72: rx-fifos\n", wlc_hw->unit));
	} else if (wlc_hw->bmc_params->rxbmmap_is_en) {
		num_of_fifo = 9;
		WL_MAC(("wl%d: fifo 0-5: tx-fifos, fifo 7: template; "
			"fifo 6/8: rx-fifos\n", wlc_hw->unit));
	} else {
		num_of_fifo = 7;
		WL_MAC(("wl%d: fifo 0-5: tx-fifos, fifo 7: template; ", wlc_hw->unit));
	}

	WL_MAC(("wl%d: \t maxbuf\t minbuf\t fullthr alloccnt allocthr\n", wlc_hw->unit));

	for (i = 0; i < num_of_fifo; i++) {
		fifo = bmc_fifo_list[i];
		/* configure per-fifo parameters and enable them one fifo by fifo
		 * always init template first to guarantee template start from first buffer
		 */
		if (fifo == tpl_idx) { /* template FIFO */
			if (D11REV_GE(wlc_hw->corerev, 128)) {
				uint q_index;
				/* select this fifo */
				wlc_bmac_bmc_get_qinfo(wlc_hw, fifo, &q_index, &bqseltype);
				W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
					q_index | (bqseltype << BMCCmd_BQSelType_SHIFT_Rev128));
			}
			maxbufs = (uint16)tplbuf;
			minbufs = maxbufs;
			full_thresh = D11REV_GE(wlc_hw->corerev, 128) ? 1 : maxbufs;
			alloc_cnt = minbufs;
			alloc_thresh = alloc_cnt - 4;
			wlc_hw->tpl_sz = minbufs * (1 << (8 + bufsize));
		} else {
			if (fifo == rxq0_idx || fifo == rxq1_idx) {	/* rx fifo */
				if (D11REV_GE(wlc_hw->corerev, 128)) {
					uint rxq = 0;
					/* select this fifo */
					wlc_bmac_bmc_get_qinfo(wlc_hw, fifo, &rxq, &bqseltype);
					W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						rxq | (bqseltype << BMCCmd_BQSelType_SHIFT_Rev128));
				}
				minbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);
				if (wlc_bmac_rsdb_cap(wlc_hw) &&
					(wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_RSDB) &&
					(fifo == rxq0_idx)) {
					minbufs >>=  1;
				}
				if (D11REV_GE(wlc_hw->corerev, 128)) {
					maxbufs = minbufs;
				} else {
					if (fifo == rxq0_idx)
						minbufs += rxq0_more_bufs;
					minbufs += 3;
					maxbufs = minbufs;
					rxmapfifosz = minbufs - 3;
					W_REG(osh, D11_RXMapFifoSize_ALTBASE(regs,
						wlc_hw->regoffsets), rxmapfifosz);
					if (HW_RXFIFO_0_WAR(wlc_hw->corerev)) {
						wlc_war_rxfifo_shm(wlc_hw, fifo, rxmapfifosz);
					}
				}
			} else {                                        /* tx fifo */
				if (D11REV_GE(wlc_hw->corerev, 128)) {
					/* select this fifo */
					bqseltype = BMCCmd_BQSelType_TX;
					W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						fifo | (bqseltype <<
						BMCCmd_BQSelType_SHIFT_Rev128));
				}
				maxbufs = wlc_hw->bmc_nbufs - tplbuf;
				minbufs = wlc_hw->bmc_params->minbufs[fifo] << doublebufsize;
			}

			if (wlc_hw->bmc_params->tx_flowctrl_scheme == 0) {
				full_thresh = wlc_hw->bmc_params->full_thresh;
				alloc_cnt = 2 * full_thresh;
				alloc_thresh = alloc_cnt - 4;
			} else { /* e.g. for rev(64,65,128) */
				full_thresh = 1;
				alloc_cnt = 2;
				alloc_thresh = 2;
			}
		}

		if (fifo == rxq0_idx || fifo == rxq1_idx) {
			WL_MAC(("fifo %d:  %d \t %d \t %d \t %d \t %d\t "
				"rx-fifo buffer cnt: %d\n",
				fifo, maxbufs, minbufs, full_thresh, alloc_cnt, alloc_thresh,
				rxmapfifosz));
		 } else {
			WL_MAC(("fifo %d:  %d \t %d \t %d \t %d \t %d\n",
				fifo, maxbufs, minbufs, full_thresh, alloc_cnt, alloc_thresh));
		 }

		W_REG(osh, D11_BMCMaxBuffers_ALTBASE(regs, wlc_hw->regoffsets), maxbufs);
		W_REG(osh, D11_BMCMinBuffers_ALTBASE(regs, wlc_hw->regoffsets), minbufs);
		W_REG(osh, D11_XmtFIFOFullThreshold_ALTBASE(regs, wlc_hw->regoffsets),
			full_thresh);
		if (D11REV_GE(wlc_hw->corerev, 56)) {
			if (D11REV_GE(wlc_hw->corerev, 128)) {
				W_REG(osh, D11_BMCAllocCtl_ALTBASE(regs, wlc_hw->regoffsets),
					alloc_cnt);
				W_REG(osh, D11_TXE_BMC_ALLOCCTL1_ALTBASE(regs, wlc_hw->regoffsets),
					alloc_thresh);
			} else {
				/* XXX: Rev50 || Rev>52;
				* Refer	http://hwnbu-twiki.sj.broadcom.com/bin/view/
				*	Mwgroup/Dot11macRevMap
				*	BMCAllocCtl.AllocCount [0:10]
				*	BMCAllocCtl.AllocThreshold [11:14]
				*/
				W_REG(osh, D11_BMCAllocCtl_ALTBASE(regs, wlc_hw->regoffsets),
				(alloc_thresh << BMCAllocCtl_AllocThreshold_SHIFT_Rev50) |
					alloc_cnt);
			}

			/* If the MSDUINDEXFIFO for a given TID,
			 * has fewer entries then the buffer arbiter doesn't grant requests
			 */
			W_REG(osh, D11_MsduThreshold_ALTBASE(regs, wlc_hw->regoffsets), 0x8);
		} else {
			W_REG(osh, D11_BMCAllocCtl_ALTBASE(regs, wlc_hw->regoffsets),
			(alloc_thresh << BMCAllocCtl_AllocThreshold_SHIFT) | alloc_cnt);
		}

		if (D11REV_GE(wlc_hw->corerev, 128)) {
			uint q_index;
			wlc_bmac_bmc_get_qinfo(wlc_hw, fifo, &q_index, &bqseltype);
			/* Enable this fifo */
			W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets), q_index |
				(1 << BMCCmd_Enable_SHIFT_rev128) |
				(bqseltype << BMCCmd_BQSelType_SHIFT_Rev128));
		} else {
			/* Enable this fifo */
			W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets), fifo |
				(1 << BMCCmd_Enable_SHIFT));
		}
		if (D11REV_GE(wlc_hw->corerev, 56)) {
			if (fifo == tpl_idx) {
				wlc_bmac_bmc_template_allocstatus(wlc_hw, MAC_CORE_UNIT_0, tplbuf);
			}
		}

		if (wlc_bmac_rsdb_cap(wlc_hw)) {
			W_REG(osh, D11_MsduThreshold_ALTBASE(sregs, wlc_hw->regoffsets), 0x8);
			/* 4349 . Set maccore_sel to 1 for Core 1 */
			if (fifo == tpl_idx) {
				maxbufs =
				D11MAC_BMC_TPL_NUMBUFS_PERCORE + D11MAC_BMC_SR_NUMBUFS;
				minbufs = maxbufs;
				alloc_cnt = minbufs;
				W_REG(osh, D11_BMCMaxBuffers_ALTBASE(regs, wlc_hw->regoffsets),
					maxbufs);
				W_REG(osh, D11_BMCMinBuffers_ALTBASE(regs, wlc_hw->regoffsets),
					minbufs);
				W_REG(osh, D11_BMCAllocCtl_ALTBASE(regs, wlc_hw->regoffsets),
				(alloc_thresh << BMCAllocCtl_AllocThreshold_SHIFT_Rev50) |
				alloc_cnt);

				if (D11REV_GE(wlc_hw->corerev, 128)) {
					uint q_index;
					wlc_bmac_bmc_get_qinfo(wlc_hw, fifo, &q_index, &bqseltype);
					W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						q_index |
						(1 << BMCCmd_ReleasePreAllocAll_SHIFT_rev80) |
						(1 <<  BMCCmd_Enable_SHIFT_rev80) |
						(bqseltype << BMCCmd_BQSelType_SHIFT_Rev80));
				} else {
					/* 4349 . Set maccore_sel to 1 for Core 1 */
					W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						fifo | (1 << 10) | (1 <<  BMCCmd_Enable_SHIFT));
				}
				wlc_bmac_bmc_template_allocstatus(wlc_hw,
				MAC_CORE_UNIT_1, alloc_cnt);
			} else if ((wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_RSDB)) {
				if (D11REV_GE(wlc_hw->corerev, 128)) {
					uint q_index;
					wlc_bmac_bmc_get_qinfo(wlc_hw, fifo, &q_index, &bqseltype);
					W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						q_index |
						(1 << BMCCmd_ReleasePreAllocAll_SHIFT_rev80) |
						(1 <<  BMCCmd_Enable_SHIFT_rev80) |
						(bqseltype << BMCCmd_BQSelType_SHIFT_Rev80));
				} else {
					W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						fifo | (1 << 10) | (1 <<  BMCCmd_Enable_SHIFT));
				}
			}
		}
	}

	if (D11REV_GE(wlc_hw->corerev, 128)) {
		/* Verify some key entries from BMC status */
		 /* fifo-00 for all tx-fifos */
		int bmc_fifoid_is131[3] = {0, 23, 24};
		int bmc_fifoid_is130[3] = {0, 39, 40};
		int bmc_fifoid_ge128[3] = {0, 71, 72};
		int *bmc_fifoid = (D11REV_IS(wlc_hw->corerev, 131) ? bmc_fifoid_is131 :
		                  (D11REV_IS(wlc_hw->corerev, 130) ? bmc_fifoid_is130 :
		                  bmc_fifoid_ge128));
		int bmc_statsel[4] = {4, 5, 6, 12};
		int j, seltype;
		WL_MAC(("BMC stats: 4-nUse 5-nMin 6-nFree 12-nAvalTid\n"));

		for (i = 0; i < 3; i++) {
			fifo = bmc_fifoid[i];
			WL_MAC(("fifo %02d :", fifo));
			if (fifo == rxq0_idx || fifo == rxq1_idx) { //rx fifo
				seltype = BMCCmd_BQSelType_RX;
				fifo -= rxq0_idx;
			} else {                                    //tx fifo
				seltype = BMCCmd_BQSelType_TX;
			}

			for (j = 0; j < 4; j++) {
				W_REG(osh, D11_BMCStatCtl(wlc_hw),
				(bmc_statsel[j] << D11MAC_BMC_NBIT_STASEL_GE128) | fifo |
				(seltype << D11MAC_BMC_SELTYPE_NBIT));
				WL_MAC((" %4x \t", R_REG(osh, D11_BMCStatData(wlc_hw))));
			}
			WL_MAC(("\n"));
		}
	}

	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		tplbuf =  D11MAC_BMC_TPL_NUMBUFS_PERCORE;
	}

	/* init template */
	for (i = 0; i < tplbuf; i ++) {
		int end_idx = i + 2 + doublebufsize;

		if (end_idx >= tplbuf)
			end_idx = tplbuf - 1;
		W_REG(osh, D11_MSDUEntryStartIdx_ALTBASE(regs, wlc_hw->regoffsets), i);
		W_REG(osh, D11_MSDUEntryEndIdx_ALTBASE(regs, wlc_hw->regoffsets), end_idx);
		W_REG(osh, D11_MSDUEntryBufCnt_ALTBASE(regs, wlc_hw->regoffsets), end_idx - i + 1);

		if (D11REV_GE(wlc_hw->corerev, 128)) {
			W_REG(osh, D11_PsmMSDUAccess_ALTBASE(regs, wlc_hw->regoffsets),
				((1 << PsmMSDUAccess_WriteBusy_SHIFT) |
				(i << PsmMSDUAccess_MSDUIdx_SHIFT_rev128) |
				(0x0 << PsmMSDUAccess_BQSel_SHIFT_rev128)));
		} else {
			W_REG(osh,  D11_PsmMSDUAccess_ALTBASE(regs, wlc_hw->regoffsets),
			      ((1 << PsmMSDUAccess_WriteBusy_SHIFT) |
			       (i << PsmMSDUAccess_MSDUIdx_SHIFT) |
			       (tpl_idx << PsmMSDUAccess_TIDSel_SHIFT)));
		}
		SPINWAIT((R_REG(wlc_hw->osh, D11_PsmMSDUAccess_ALTBASE(regs, wlc_hw->regoffsets)) &
			(1 << PsmMSDUAccess_WriteBusy_SHIFT)), 200);
		if (R_REG(wlc_hw->osh, D11_PsmMSDUAccess_ALTBASE(regs, wlc_hw->regoffsets)) &
		    (1 << PsmMSDUAccess_WriteBusy_SHIFT)) {
				WL_ERROR(("wl%d: PSM MSDU init not done yet :-(\n", wlc_hw->unit));
		}
	}

#ifdef WLRSDB
	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		if (wlc_rsdb_mode(wlc_hw->wlc) != PHYMODE_RSDB) {
			/* Override hardware default for core-1.
			 * Force MsduThreshold on core-1 to minimum value to
			 * take effect during dynamic init.
			 */
			for (i = 0; i < num_of_fifo; i++) {
				fifo = bmc_fifo_list[i];
				W_REG(osh, D11_MsduThreshold_ALTBASE(sregs, wlc_hw->regoffsets),
					0x8);
				if (D11REV_GE(wlc_hw->corerev, 128)) {
					uint q_index;
					wlc_bmac_bmc_get_qinfo(wlc_hw, fifo, &q_index, &bqseltype);
				W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						q_index |
						(1 << BMCCmd_ReleasePreAllocAll_SHIFT_rev80) |
						(1 <<  BMCCmd_Enable_SHIFT_rev80) |
						(bqseltype << BMCCmd_BQSelType_SHIFT_Rev80));
				} else {
					W_REG(osh, D11_BMCCmd_ALTBASE(regs, wlc_hw->regoffsets),
						fifo | (1 << 10) | (1 <<  BMCCmd_Enable_SHIFT));
				}
			}
		}

		wlc_rsdb_bmc_smac_template(wlc_hw->wlc, tplbuf, doublebufsize);

		/*
		* If d11 core is greatet than 55,
		* set  core1 template ptr offset to D11MAC_BMC_TPL_BYTES_PERCORE
		*/
		if (D11REV_GE(wlc_hw->corerev, 56)) {
			W_REG(osh, D11_XmtTemplatePtrOffset_ALTBASE(sregs, wlc_hw->regoffsets),
				D11MAC_BMC_TPL_BYTES_PERCORE / 4);
		}
	}
#endif /* WLRSDB */

	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		si_d11_switch_addrbase(wlc_hw->sih, sicoreunit);
	}
	WL_MAC(("wl%d: bmc_init done\n", wlc_hw->unit));

	return 0;
} /* wlc_bmac_bmc_init */

#if defined(BCMDBG)
static int
wlc_bmac_bmc_dump_parse_args(wlc_info_t *wlc, bool *init, uint32 bmp[])
{
	int err = BCME_OK;
	char *args = wlc->dump_args;
	char *p, **argv = NULL;
	uint argc = 0;
	int i, val;
	int d11mac_bmc_max_fifos = D11MAC_BMC_MAXFIFOS(wlc->hw->corerev);

	if (args == NULL || init == NULL) {
		err = BCME_BADARG;
		goto exit;
	}

	/* allocate argv */
	if ((argv = MALLOC(wlc->osh, sizeof(*argv) * DUMP_BMC_ARGV_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: failed to allocate the argv buffer\n",
		          wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	/* get each token */
	p = bcmstrtok(&args, " ", 0);
	while (p && argc < DUMP_BMC_ARGV_MAX-1) {
		argv[argc++] = p;
		p = bcmstrtok(&args, " ", 0);
	}
	argv[argc] = NULL;

	/* initial default */
	*init = FALSE;

	/* parse argv */
	argc = 0;
	while ((p = argv[argc++])) {
		if (!strncmp(p, "-", 1)) {
			switch (*++p) {
				case 'i':
					if (D11REV_GE(wlc->hw->corerev, 48))
						*init = TRUE;
					else
						err = BCME_UNSUPPORTED;
					break;
				case 'f':
					if (D11REV_GE(wlc->hw->corerev, 128)) {
						for (i = 0; i < (d11mac_bmc_max_fifos / 32 + 1);
						i++) {
							bmp[i] = 0;
						}
						while (*p != '\0') {
							p++;
							val = bcm_strtoul(p, &p, 0);
							if (val >= d11mac_bmc_max_fifos) {
								err = BCME_BADARG;
								goto exit;
							}
							bmp[val/32] |= (1 << (val%32));
						}

						*init = TRUE;
					} else {
						err = BCME_UNSUPPORTED;
					}
						break;
				default:
					err = BCME_BADARG;
					goto exit;
			}
		} else {
			err = BCME_BADARG;
			goto exit;
		}
	}

exit:
	if (argv) {
		MFREE(wlc->osh, argv, sizeof(*argv) * DUMP_BMC_ARGV_MAX);
	}

	return err;
}

static int
wlc_bmac_bmc_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b)
{
	osl_t *osh;
	uint16 *pbuf, *p;
	int i, j, num_of_fifo;
	bool init = FALSE;
	/* specify dump which queues */
	uint32 fifo_bmp[D11MAC_BMC_MAXFIFOS_GE128/32 + 1];
	uint16 tmp0, tmp1, bmccmd1;
	int fifo, fifo_phys;
	int *bmc_fifo_list;
	int err = BCME_OK;
	uint tpl_idx, rxq0_idx, rxq1_idx, fifoid;
	int num_statsel;
	uint8 statsel_nbit, bmask_fifosel, seltype;

	osh = wlc_hw->osh;

	if (!wlc_hw->clk)
		return BCME_NOCLK;

	if (D11REV_LT(wlc_hw->corerev, 40)) {
		return BCME_UNSUPPORTED;
	}

	tpl_idx			= D11MAC_BMC_TPL_IDX(wlc_hw->corerev);
	rxq0_idx		= D11MAC_BMC_RXFIFO0_IDX(wlc_hw->corerev);
	rxq1_idx		= D11MAC_BMC_RXFIFO1_IDX(wlc_hw->corerev);
	num_statsel		= D11MAC_BMC_NUM_STASEL(wlc_hw->corerev);
	statsel_nbit	= D11MAC_BMC_NBIT_STASEL(wlc_hw->corerev);
	bmask_fifosel	= D11MAC_BMC_BMASK_FIFOSEL(wlc_hw->corerev);
	bmc_fifo_list	= D11MAC_BMC_FIFO_LIST(wlc_hw->corerev);

	if ((pbuf = (uint16*) MALLOC(osh, num_statsel*sizeof(uint16))) == NULL) {
		WL_ERROR(("wl: %s: MALLOC failure\n", __FUNCTION__));
		return 0;
	}

	for (i = 0; i < D11MAC_BMC_MAXFIFOS_GE128/32+1; i++) {
		fifo_bmp[i] = -1;
	}

	/* Parse args if needed */
	if (wlc_hw->wlc->dump_args) {
		err = wlc_bmac_bmc_dump_parse_args(wlc_hw->wlc, &init, fifo_bmp);
		if (err != BCME_OK) {
			goto dump_bmc_done;
		}
	}

	if (D11REV_IS(wlc_hw->corerev, 131)) {
		num_of_fifo = D11MAC_BMC_MAXFIFOS_IS131;
		WL_INFORM(("wl%d: fifo 0-21: tx-fifos, fifo 22: template; "
				"fifo 23/24: rx-fifos\n", wlc_hw->unit));
		WL_PRINT(("wl%d: fifo 0-21: tx-fifos, fifo 22: template; "
				"fifo 23/24: rx-fifos\n", wlc_hw->unit));
	} else if (D11REV_IS(wlc_hw->corerev, 130)) {
		num_of_fifo = D11MAC_BMC_MAXFIFOS_IS130;
		WL_INFORM(("wl%d: fifo 0-37: tx-fifos, fifo 38: template; "
				"fifo 39/40: rx-fifos\n", wlc_hw->unit));
		WL_PRINT(("wl%d: fifo 0-37: tx-fifos, fifo 38: template; "
				"fifo 39/40: rx-fifos\n", wlc_hw->unit));
	} else if (D11REV_GE(wlc_hw->corerev, 128)) {
		num_of_fifo = D11MAC_BMC_MAXFIFOS_GE128;
		WL_INFORM(("wl%d: fifo 0-69: tx-fifos, fifo 70: template; "
				"fifo 71/72: rx-fifos\n", wlc_hw->unit));
		WL_PRINT(("wl%d: fifo 0-69: tx-fifos, fifo 70: template; "
				"fifo 71/72: rx-fifos\n", wlc_hw->unit));
	} else if (wlc_hw->bmc_params->rxbmmap_is_en) {
		num_of_fifo = 9;
		WL_INFORM(("wl%d: fifo 0-5: tx-fifos, fifo 7: template; "
			"fifo 6/8: rx-fifos\n", wlc_hw->unit));
	} else {
		num_of_fifo = 7;
		WL_INFORM(("wl%d: fifo 0-5: tx-fifos, fifo 7: template; ", wlc_hw->unit));
	}

	if (D11REV_GE(wlc_hw->corerev, 128)) {
		bcm_bprintf(b, "AQQMAP 0x%x AQMF_STATUS 0x%x AQMCT_PRIFIFO 0x%x\n",
			R_REG(osh, D11_AQM_QMAP(wlc_hw)),
			R_REG(osh, D11_AQMF_STATUS(wlc_hw)),
			R_REG(osh, D11_AQMCT_PRIFIFO(wlc_hw)));
	} else if (D11REV_GE(wlc_hw->corerev, 65)) {
		bcm_bprintf(b, "BcmReadStatus 0x%04x rqPrio 0x%x BmcCmd 0x%x "
			"psm_reg_mux 0x%x AQMQMAP 0x%x AQMFifo_Status 0x%x\n",
			R_REG(osh, D11_BMCReadStatus(wlc_hw)),
			R_REG(osh, D11_XmtFifoRqPrio(wlc_hw)),
			R_REG(osh, D11_BMCCmd(wlc_hw)),
			R_REG(osh, D11_PSM_REG_MUX(wlc_hw)),
			R_REG(osh, D11_AQMQMAP(wlc_hw)),
			R_REG(osh, D11_AQMFifo_Status(wlc_hw)));
	} else {
		bcm_bprintf(b, "BcmReadStatus 0x%04x rqPrio 0x%x BmcCmd 0x%x "
			"FifoRdy 0x%x FrmCnt 0x%x\n",
			R_REG(osh, D11_BMCReadStatus(wlc_hw)),
			R_REG(osh, D11_XmtFifoRqPrio(wlc_hw)),
			R_REG(osh, D11_BMCCmd(wlc_hw)),
			R_REG(osh, D11_AQMFifoReady(wlc_hw)),
			R_REG(osh, D11_XmtFifoFrameCnt(wlc_hw)));
	}

	if (D11REV_GE(wlc_hw->corerev, 128)) {
		bcm_bprintf(b, "BMC stats: 0-nfrm 1-nbufRecvd 2-nbuf2DMA 3-nbufMax "
		"4-nbufUse 5-nbufMin 6-nFree\n"
		"%11s7-nDrqPush 8-nDrqPop 9-nDdqPush 10-nDdqPop "
		"11-nOccupied 12-nAvalTid\n", "");
	} else {
		bcm_bprintf(b, "BMC stats: 0-nfrm 1-nbufRecvd 2-nbuf2DMA 3-nbufMax 4-nbufUse "
		"5-nbufMin\n"
		"           6-nFree 7-nDrqPush 8-nDrqPop 9-nDdqPush 10-nDdqPop "
		"11-nOccupied\n");
	}

	for (i = 0; i < num_of_fifo; i++) {
		/* skip template */
		if (i == tpl_idx) {
			continue;
		}
		p = pbuf;

		seltype = 0;
		fifoid = i;
		fifo_phys = i;
		if (D11REV_GE(wlc_hw->corerev, 128)) {
			if ((fifo_bmp[i/32] & (1<<(i%32))) == 0) {
				continue;
			}
			if (i == rxq0_idx || i == rxq1_idx) {
				//rx fifo
				seltype = BMCCmd_BQSelType_RX;
				fifoid = i - rxq0_idx;
			} else {
				//tx fifio
				seltype = BMCCmd_BQSelType_TX;
				fifoid = i;
				fifo_phys = WLC_HW_MAP_TXFIFO(wlc_hw->wlc, i);
			}
		}

		for (j = 0; j < num_statsel; j++, p++) {
			W_REG(osh, D11_BMCStatCtl(wlc_hw),
			(j << statsel_nbit) | (fifoid & (bmask_fifosel)) |
			(seltype << D11MAC_BMC_SELTYPE_NBIT));
			*p = R_REG(osh, D11_BMCStatData(wlc_hw));
		}
		bcm_bprintf(b, "fifo-%02d(p%02d) :", i, fifo_phys);
		p = pbuf;
		for (j = 0; j < num_statsel; j++, p++) {
			bcm_bprintf(b, " %4x", *p);
		}
		bcm_bprintf(b, "\n");
	}

	if (init) {
		if (D11REV_GE(wlc_hw->corerev, 128)) {
			tmp0 = R_REG(osh, D11_TXE_BMC_CONFIG1(wlc_hw)) &
			BMCCONFIG_BUFCNT_MASK_GE128;
		} else {
			tmp0 = R_REG(osh, D11_BMCConfig(wlc_hw)) & BMCCONFIG_BUFCNT_MASK;
		}
		tmp1 = R_REG(osh, D11_BMCCTL(wlc_hw)) & (1 << BMCCTL_TXBUFSIZE_SHIFT);
		bcm_bprintf(b, "\nBMC bufsize %d maxbufs %d start_addr 0x%04x\n",
			(1 << (8 + (tmp1 >> BMCCTL_TXBUFSIZE_SHIFT))), tmp0,
			(R_REG(osh, D11_BMCStartAddr(wlc_hw)) & BMCSTARTADDR_STRTADDR_MASK));

		/* dump RX queue init data */
		if (wlc_hw->bmc_params->rxq_in_bm) {
			bcm_bprintf(b, "rxq_in_bm ON. rxbmmap is %s. "
				"RXQ size/ptr below are in 32-bit DW.\n",
				wlc_hw->bmc_params->rxbmmap_is_en ? "enabled" : "disabled");

			tmp0 = R_REG(osh, D11_RCV_BM_STARTPTR_Q0(wlc_hw));
			tmp1 = R_REG(osh, D11_RCV_BM_ENDPTR_Q0(wlc_hw));
			bcm_bprintf(b, "RXQ-0 size 0x%04x start_ptr 0x%04x end_ptr 0x%04x\n",
				(tmp1 - tmp0 +1), tmp0, tmp1);
			tmp0 = R_REG(osh, D11_RCV_BM_STARTPTR_Q1(wlc_hw));
			tmp1 = R_REG(osh, D11_RCV_BM_ENDPTR_Q1(wlc_hw));
			bcm_bprintf(b, "RXQ-1 size 0x%04x start_ptr 0x%04x end_ptr 0x%04x\n",
				(tmp1 - tmp0 +1), tmp0, tmp1);
		}

		/* dump all fifos init data */
		if (num_of_fifo == D11MAC_BMC_MAXFIFOS_IS131) {
			bcm_bprintf(b, "fifo 0-21: tx-fifos, fifo 22: template; "
			"fifo 23/24: rx-fifos\n");
		} else if (num_of_fifo == D11MAC_BMC_MAXFIFOS_IS130) {
			bcm_bprintf(b, "fifo 0-37: tx-fifos, fifo 38: template; "
			"fifo 39/40: rx-fifos\n");
		} else if (num_of_fifo == D11MAC_BMC_MAXFIFOS_GE128) {
			bcm_bprintf(b, "fifo 0-69: tx-fifos, fifo 70: template; "
			"fifo 71/72: rx-fifos\n");
		} else if (num_of_fifo == D11MAC_BMC_MAXFIFOS_LT80) {
			bcm_bprintf(b, "fifo 0-5: tx-fifos, fifo 7: template; "
			"fifo 6/8: rx-fifos\n");
		} else {
			bcm_bprintf(b, "fifo 0-5: tx-fifos, fifo 7: template;\n");
		}

		tmp1 = R_REG(osh, D11_BMCAllocCtl(wlc_hw));
		if (D11REV_GE(wlc_hw->corerev, 56)) {
			tmp0 = tmp1 & ((1 <<BMCAllocCtl_AllocThreshold_SHIFT_Rev50)-1);
			tmp1 = tmp1 >> BMCAllocCtl_AllocThreshold_SHIFT_Rev50;
		} else {
			tmp0 = tmp1 & ((1 << BMCAllocCtl_AllocThreshold_SHIFT)-1);
			tmp1 = tmp1 >> BMCAllocCtl_AllocThreshold_SHIFT;
		}
		bcm_bprintf(b, "\t xmt_fifo_full_thr %d alloc_cnt %d alloc_thr %d\n",
			R_REG(osh, D11_XmtFIFOFullThreshold(wlc_hw)), tmp0, tmp1);

		wlc_bmac_suspend_mac_and_wait(wlc_hw);
		bmccmd1 = R_REG(osh, D11_BMCCmd1(wlc_hw));

		bcm_bprintf(b, "\t maxbuf\t minbuf  \n");
		for (i = 0; i < num_of_fifo; i++) {
			fifo = fifo_phys = bmc_fifo_list[i];
			if (D11REV_GE(wlc_hw->corerev, 128)) {

				if ((fifo_bmp[fifo/32] & (1<<(fifo%32))) == 0) {
					continue;
				}

				if (fifo == D11MAC_BMC_TPL_IDX(wlc_hw->corerev)) {
					seltype = BMCCmd_BQSelType_Templ;
					/* template */
					fifoid = fifo - D11MAC_BMC_TPL_IDX(wlc_hw->corerev);
				} else if (fifo == rxq0_idx || fifo == rxq1_idx) {
					/* rx fifo */
					seltype = BMCCmd_BQSelType_RX;
					fifoid = fifo - rxq0_idx;
				} else {
					/* tx fifo */
					seltype = BMCCmd_BQSelType_TX;
					fifoid = fifo;
					fifo_phys = WLC_HW_MAP_TXFIFO(wlc_hw->wlc, fifo);
				}
				W_REG(osh, D11_BMCCmd1(wlc_hw), (seltype << BMCCMD1_SELTYPE_NBIT |
				fifoid << BMCCMD1_SELNUM_NBIT | 0x3 << BMCCMD1_RDSRC_SHIFT_GE128));

			} else {
				W_REG(osh, D11_BMCCmd1(wlc_hw), ((bmccmd1 &
				~(0xf << BMCCMD1_TIDSEL_SHIFT)) | (fifo << BMCCMD1_TIDSEL_SHIFT)
				| (0x3 << BMCCMD1_RDSRC_SHIFT_LT128)));
			}

			if (fifo == rxq0_idx|| fifo == rxq1_idx) {
				bcm_bprintf(b,
					"fifo %02d(p%02d):  %d \t %d \trx-fifo buffer cnt: %d\n",
					fifo, fifo_phys,
					R_REG(osh, D11_BMCMaxBuffers(wlc_hw)),
					R_REG(osh, D11_BMCMinBuffers(wlc_hw)),
					R_REG(osh, D11_RXMapFifoSize(wlc_hw)));
			} else {
				bcm_bprintf(b, "fifo %02d(p%02d):  %d \t %d\n",
					fifo, fifo_phys,
					R_REG(osh, D11_BMCMaxBuffers(wlc_hw)),
					R_REG(osh, D11_BMCMinBuffers(wlc_hw)));
			}
		}

		W_REG(osh, D11_BMCCmd1(wlc_hw), bmccmd1);
		wlc_bmac_enable_mac(wlc_hw);
	}

dump_bmc_done:
	if (pbuf) {
		MFREE(osh, pbuf, num_statsel * sizeof(uint16));
	}
	return err;
}
#endif // endif

void
wlc_bmac_tsf_adjust(wlc_hw_info_t *wlc_hw, int delta)
{
	uint32 tsf_l, tsf_h;
	uint32 delta_h;
	osl_t *osh = wlc_hw->osh;

	/* adjust the tsf time by offset */
	wlc_bmac_read_tsf(wlc_hw, &tsf_l, &tsf_h);
	/* check for wrap:
	 * if we are close to an overflow (2 ms) from tsf_l to high,
	 * make sure we did not read tsf_h after the overflow
	 */
	if (tsf_l > (uint32)(-2000)) {
		uint32 tsf_l_new;
		tsf_l_new = R_REG(osh, D11_TSFTimerLow(wlc_hw));
		/* update the tsf_h if tsf_l rolled over since we do not know if we read tsf_h
		 * before or after the roll over
		 */
		if (tsf_l_new < tsf_l)
			tsf_h = R_REG(osh, D11_TSFTimerHigh(wlc_hw));
		tsf_l = tsf_l_new;
	}

	/* sign extend delta to delta_h */
	if (delta < 0)
		delta_h = -1;
	else
		delta_h = 0;

	wlc_uint64_add(&tsf_h, &tsf_l, delta_h, (uint32)delta);

	W_REG(osh, D11_TSFTimerLow(wlc_hw), tsf_l);
	W_REG(osh, D11_TSFTimerHigh(wlc_hw), tsf_h);
}

void
wlc_bmac_update_bt_chanspec(wlc_hw_info_t *wlc_hw,
	chanspec_t chanspec, bool scan_in_progress, bool roam_in_progress)
{
#ifdef BCMECICOEX
	si_t *sih = wlc_hw->sih;
	bool send_channel;
	uint8 infrachan;

	if (BCMGCICOEX_ENAB_BMAC(wlc_hw)) {
		/* Update chanspec related indications on GCI */
		wlc_btcx_upd_gci_chanspec_indications(wlc_hw->wlc);

		/* Indicate WLAN active in 5G to BT */
		wlc_btcx_active_in_5g(wlc_hw->wlc);
	}

	if (!wlc_hw->wlc->cfg)
		return;

	if (wlc_hw->wlc->cfg->associated) {
		infrachan = CHSPEC_CHANNEL(wlc_hw->wlc->cfg->current_bss->chanspec);
	}
	else {
		infrachan = 0;
	}

	if (BCMCOEX_ENAB_BMAC(wlc_hw) && !scan_in_progress && !roam_in_progress) {
		/* Inform BT about the channel change if we are operating in 2Ghz band */
		if ((wlc_hw->wlc->cfg->associated && (CHSPEC_CHANNEL(chanspec) == infrachan)) ||
			(CHSPEC_CHANNEL(chanspec) == 0))
			send_channel = TRUE;   /* Always send for infra channel for all chips */
		else
			send_channel = FALSE;

#ifdef WLRSDB
		/* Don't update chanspec as 0 when the other core is active */
		if ((chanspec == 0) &&
			!wlc_rsdb_is_other_chain_idle(wlc_hw->wlc)) {
			send_channel = FALSE;
		}
#endif // endif

		if (send_channel) {
			if (chanspec && CHSPEC_IS2G(chanspec)) {
				NOTIFY_BT_CHL(sih, CHSPEC_CHANNEL(chanspec));
				if (CHSPEC_IS40(chanspec))
					NOTIFY_BT_BW_40(sih);
				else
					NOTIFY_BT_BW_20(sih);
			} else if (chanspec && CHSPEC_IS5G(chanspec)) {
#ifdef WLRSDB
				/* Do not update chanspec to BT in RSDB mode since current core
				  * is in 5G. Only 2G core's chanspec should get updated to BT.
				  */
				if (wlc_btcx_rsdb_update_bt_chanspec(wlc_hw->wlc)) {
					NOTIFY_BT_CHL(sih, 0xf);
				}
#else
				NOTIFY_BT_CHL(sih, 0xf);
#endif // endif
			} else {
				NOTIFY_BT_CHL(sih, 0);
			}
		}
	}
#endif /* BCMECICOEX */
}

int wlc_bmac_is_singleband_5g(unsigned int device, unsigned int corecap)
{
	return (_IS_SINGLEBAND_5G(device) ||
		((corecap & PHY_PREATTACH_CAP_SUP_5G) && !(corecap & PHY_PREATTACH_CAP_SUP_2G)));
}

int wlc_bmac_srvsdb_force_set(wlc_hw_info_t *wlc_hw, uint8 force)
{
	wlc_hw->sr_vsdb_force = force;
	return BCME_OK;
}

/**
 * Function to set input mac address in SHM for ucode generated CTS2SELF. The Mac addresses are
 * written out 2 bytes at a time at the specific SHM location. For non-AC chips this mac address was
 * retrieved from the RCMTA by ucode directly. For AC chips there is a bug that prevents access to
 * the search engine by ucode. For CTS packets (normal and CTS2SELF), the mac address is bit-
 * substituted before transmission. So we use the address set in this SHM location for CTS2SELF
 * packets. GE40 only.
 */
void
wlc_bmac_set_myaddr(wlc_hw_info_t *wlc_hw, struct ether_addr *mac_addr)
{
	unsigned short mac;

	mac = ((mac_addr->octet[1]) << 8) | mac_addr->octet[0];
	wlc_bmac_write_shm(wlc_hw, M_MYMAC_ADDR_L(wlc_hw), mac);
	mac = ((mac_addr->octet[3]) << 8) | mac_addr->octet[2];
	wlc_bmac_write_shm(wlc_hw, M_MYMAC_ADDR_M(wlc_hw), mac);
	mac = ((mac_addr->octet[5]) << 8) | mac_addr->octet[4];
	wlc_bmac_write_shm(wlc_hw, M_MYMAC_ADDR_H(wlc_hw), mac);
}

/**
 * This function attempts to drain A2DP buffers in BT before granting the antenna to Wl for
 * various calibrations, etc. This can only be done for ECI supported chips (including GCI) since
 * task and buffer count information is needed. It is also assumed that the mac is suspended when
 * this function is called. This function does the following:
 *   - Grant the antenna to BT (ANTSEL and TXCONF set to 0)
 *   - If the BT task type is A2DP and the buffer count is non-zero wait for up to 50 ms
 *     until the buffer count becomes zero.
 *   - If the task type is not A2DP or the buffer count is zero, exit the wait loop
 *   - If BT RF Active is asserted, wait for up to 5 ms for it to de-assert after setting
 *     TXCONF to 1 (don't grant to BT).
 * This functionality has been moved out of common PHY code since it is mac-related.
*/
#define BTCX_FLUSH_WAIT_MAX_MS  50
void
wlc_bmac_coex_flush_a2dp_buffers(wlc_hw_info_t *wlc_hw)
{
#if defined(BCMECICOEX)
	int delay_val;
	uint16 eci_m = 0;
	uint16 a2dp_buffer = 0;
	uint16 bt_task = 0;

	if (BCMCOEX_ENAB_BMAC(wlc_hw)) {
		/* Ucode better be suspended when we mess with BTCX regs directly */
		ASSERT(!(R_REG(wlc_hw->osh, D11_MACCONTROL(wlc_hw)) & MCTL_EN_MAC));

		OR_REG(wlc_hw->osh, D11_BTCX_CTL(wlc_hw),
			BTCX_CTRL_EN | BTCX_CTRL_SW);
		/* Set BT priority and antenna to allow A2DP to catch up
		* TXCONF is active low, granting BT to TX/RX, and ANTSEL=0 for
		* BT, so clear both bits to 0.
		* microcode is already suspended, so no one can change these bits
		*/
		AND_REG(wlc_hw->osh, D11_BTCX_TRANSCTL(wlc_hw),
			~(BTCX_TRANS_TXCONF | BTCX_TRANS_ANTSEL));
		/* Wait for A2DP to flush all pending data.
		* Since some of these bits are over-loaded it is best to ensure that the task
		* type is A2DP.
		* In GCI, A2DP buffer count is at ECI[27:24], BT Task Type is in
		* ECI[21:16] (both are in word 1)
		* Non-GCI has A2DP buffer count in ECI[23:20] (word 1), BT Task type is in
		* ECI[8:3] (word 0),
		*/
		for (delay_val = 0; delay_val < BTCX_FLUSH_WAIT_MAX_MS * 10; delay_val++) {
			if (BCMGCICOEX_ENAB_BMAC(wlc_hw)) {
				/* In GCI, both task type and buffer count are available
				   in the same word
				*/
				W_REG(wlc_hw->osh, D11_BTCX_ECI_ADDR(wlc_hw), 1);
				eci_m = R_REG(wlc_hw->osh, D11_BTCX_ECI_DATA(wlc_hw));
				a2dp_buffer = ((eci_m >> 8) & 0xf);
				bt_task = eci_m  & 0x3f;
			} else {
				W_REG(wlc_hw->osh, D11_BTCX_ECI_ADDR(wlc_hw), 0);
				eci_m = R_REG(wlc_hw->osh, D11_BTCX_ECI_DATA(wlc_hw));
				bt_task = (eci_m >> 4)  & 0x3f;
				W_REG(wlc_hw->osh, D11_BTCX_ECI_ADDR(wlc_hw), 1);
				eci_m = R_REG(wlc_hw->osh, D11_BTCX_ECI_DATA(wlc_hw));
				a2dp_buffer = ((eci_m >> 4) & 0xf);
			}
			if (((bt_task == 4) && (a2dp_buffer == 0)) ||
			    (bt_task != 4)) {
				/* All A2DP data is flushed  or not A2DP */
				goto pri_wlan;
			}
			OSL_DELAY(100);
		}
		if (delay_val == (BTCX_FLUSH_WAIT_MAX_MS * 10)) {
			WL_ERROR(("wl%d: %s: A2DP flush failed, eci_m: 0x%x\n",
				wlc_hw->unit, __FUNCTION__, eci_m));
		}

pri_wlan:
		/* Reenable WLAN priority, and then wait for BT to finish */
		OR_REG(wlc_hw->osh, D11_BTCX_TRANSCTL(wlc_hw), BTCX_TRANS_TXCONF);
		delay_val = 0;
		/* While RF_ACTIVE is asserted... */
		while (R_REG(wlc_hw->osh, D11_BTCX_STAT(wlc_hw)) & BTCX_STAT_RA) {
			if (delay_val++ > BTCX_FLUSH_WAIT_MAX_MS) {
				WL_PRINT(("wl%d: %s: BT still active\n",
					wlc_hw->unit, __FUNCTION__));
				break;
			}
			OSL_DELAY(100);
		}
	}
#endif /* BCMECICOEX */
} /* wlc_bmac_coex_flush_a2dp_buffers */

/**
 * In MIMO mode, some registers that need to path registers are still common registers in 4349A0.
 * However, due to the RSDB support, there are two copies of the common registers, one for
 * each core. This can be exploited to write to the shadow copy of the common register for each
 * of the cores. The workaround to do that is by enabling a bit in PHYMODE register, and this
 * results in a write to common register only writing the core0 copy. The following function
 * implements this workaround of writing to the specific bit in the PHYMODE register.
 */
void
wlc_bmac_exclusive_reg_access_core0(wlc_hw_info_t *wlc_hw, bool set)
{
	uint32 phymode = (si_core_cflags(wlc_hw->sih, 0, 0) & SICF_PHYMODE) >> SICF_PHYMODE_SHIFT;

	ASSERT(phy_get_phymode((phy_info_t *)wlc_hw->band->pi) == PHYMODE_MIMO);
	if (set)
		phymode |= SUPPORT_EXCLUSIVE_REG_ACCESS_CORE0;
	else
		phymode &= ~SUPPORT_EXCLUSIVE_REG_ACCESS_CORE0;

	si_core_cflags(wlc_hw->sih, SICF_PHYMODE, phymode << SICF_PHYMODE_SHIFT);
}
void
wlc_bmac_exclusive_reg_access_core1(wlc_hw_info_t *wlc_hw, bool set)
{
	uint32 phymode = (si_core_cflags(wlc_hw->sih, 0, 0) & SICF_PHYMODE) >> SICF_PHYMODE_SHIFT;

	ASSERT(phy_get_phymode((phy_info_t *)wlc_hw->band->pi) == PHYMODE_MIMO);
	if (set)
		phymode |= SUPPORT_EXCLUSIVE_REG_ACCESS_CORE1;
	else
		phymode &= ~SUPPORT_EXCLUSIVE_REG_ACCESS_CORE1;

	si_core_cflags(wlc_hw->sih, SICF_PHYMODE, phymode << SICF_PHYMODE_SHIFT);
}

#define  STARTBUSY_BIT_POLL_MAX_TIME 50
#define  INCREMENT_ADDRESS 4

#ifdef BCMPCIEDEV
void
wlc_bmac_enable_tx_hostmem_access(wlc_hw_info_t *wlc_hw, bool enabled)
{
	if (BCMPCIEDEV_ENAB()) {
		wlc_info_t *wlc = wlc_hw->wlc;
		wlc_txq_info_t *qi;

		if (!wlc_hw->up) {
			return;
		}
		WL_INFORM(("wlc_bmac_enable_tx_hostmem_access \n"));
		if (!enabled)  {
			/* device power state changed in D3 device can't  */
			/* access host memory any more */
			/* 1. Stop sending data queued in the TCM to dma. */
			/* 2. Flush the FIFOs.  */
			/* 3. Drop all pending dma packets. */
			/* 4. Enable the DMA. */

			/* During excursion (e.g. scan in progress), active queue is not the
			 * primary queue. So, make sure to flush all the primary queues.
			 */
			for (qi = wlc->tx_queues; qi != NULL; qi = qi->next) {
				if (qi != wlc->excursion_queue)
					wlc_sync_txfifo_all(wlc, qi, FLUSHFIFO);
			}
		}
	}
}
#endif /* BCMPCIEDEV */

/* Before take the debug register dump update dma control flags
 * (select active bit set)
 * Can be added as part of regular register dumps also
 */
void
wlc_bmac_update_dma_rx_ctrlflags_bhalt(wlc_hw_info_t *wlc_hw)
{
	int i = 0;
	for (i = 0; i < MAX_RX_FIFO; i++) {
		if ((wlc_hw->di[i] != NULL) && wlc_bmac_rxfifo_enab(wlc_hw, i)) {
			dma_ctrlflags(wlc_hw->di[i], D64_RC_SA, D64_RC_SA);
		}
	}
	OSL_SYS_HALT();
}

extern int my_flag;

void
wlc_bmac_enable_rx_hostmem_access(wlc_hw_info_t *wlc_hw, bool enabled)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint32 q0_cnt;
	uint32 i;
	bool dma_stuckerr = FALSE;

#if defined(BCMDBG)
	char *buf;
	struct bcmstrbuf b;
	const int DUMP_SIZE = 256;
#endif // endif

	/* device power state changed in D3 device can;t access host memory any more */
	/* switch off the classification in the ucode and let the packet come to fifo1 only */
	if (BCMSPLITRX_ENAB()) {
		/* Handle software state change of FIFO0 first.
		* This will happen even when wl is down.
		*/
		dma_rxfill_suspend(wlc_hw->di[RX_FIFO], !enabled);

		/* For the rest, make sure hw is up */
		if (!wlc_hw->up) {
			return;
		}

		if (!enabled)  {
			wlc_bmac_suspend_mac_and_wait(wlc_hw);
		}

		WL_PRINT(("wl%d enable %d: q0 frmcnt %d, wrdcnt %d, q1 frmcnt %d, wrdcnt %d\n",
		wlc->pub->unit, enabled,
		R_REG(wlc->osh, D11_RCV_FRM_CNT_Q0(wlc_hw)),
		R_REG(wlc->osh, D11_RCV_WRD_CNT_Q0(wlc_hw)),
		R_REG(wlc->osh, D11_RCV_FRM_CNT_Q1(wlc_hw)),
		R_REG(wlc->osh, D11_RCV_WRD_CNT_Q1(wlc_hw))));

		if (enabled)  {
			wlc_mhf(wlc, MHF1, MHF1_RXFIFO1, 0, WLC_BAND_ALL);
			wlc_mhf(wlc, MHF3, MHF3_SELECT_RXF1, MHF3_SELECT_RXF1, WLC_BAND_ALL);
			/* Previous suspend might have made sure that there are
			* no pending buffers posted to Rx. So fill it up.
			*/
			wlc_bmac_dma_rxfill(wlc_hw, RX_FIFO);
		} else {

			for (i = 0; i < D11AC_MAX_RXFIFO_DRAIN; i++) {
				q0_cnt = R_REG(wlc->osh, D11_RCV_FRM_CNT_Q0(wlc_hw));
				if (q0_cnt == 0) {
					/* Even when q0 count is zero, completion of DMA may take
					 * little more time. Ensure that DMA is idle after 3-6ms.
					 */
					OSL_DELAY(3000);
					SPINWAIT(!dma_rxidlestatus(wlc_hw->di[RX_FIFO]),
						1000);
					OSL_DELAY(3000);
					SPINWAIT(!dma_rxidlestatus(wlc_hw->di[RX_FIFO]),
						1000);
					break;
				}
				OSL_DELAY(1000);
			}
			if (q0_cnt != 0) {
				WL_PRINT(("looped %d: pending q0 count %d\n", i, q0_cnt));
				WL_PRINT(("q0 frmcnt %d, wrdcnt %d, q1 frmcnt %d, wrdcnt %d\n",
				R_REG(wlc->osh, D11_RCV_FRM_CNT_Q0(wlc_hw)),
				R_REG(wlc->osh, D11_RCV_WRD_CNT_Q0(wlc_hw)),
				R_REG(wlc->osh, D11_RCV_FRM_CNT_Q1(wlc_hw)),
				R_REG(wlc->osh, D11_RCV_WRD_CNT_Q1(wlc_hw))));
				if (dma_activerxbuf(wlc_hw->di[RX_FIFO])) {
					/* TRAP if there are active DMA buffers and qcount is !0 */
					OSL_SYS_HALT();
				}
			}
			/* DIE!!!if dma didn't reach idle even after 32ms */
			if (!dma_rxidlestatus(wlc_hw->di[RX_FIFO])) {
				WL_PRINT(("BAD: DIE as DMA didn't become idle , q0_cnt = %d "
				"frm_cnt = %d\n",
				R_REG(wlc->osh, D11_RCV_FRM_CNT_Q0(wlc_hw)),
				R_REG(wlc->osh, D11_RCV_WRD_CNT_Q0(wlc_hw))));
				WL_PRINT(("PREV q0_cnt=%d\n", q0_cnt));
				dma_stuckerr = TRUE;
#if defined(BCMDBG)
				buf = MALLOCZ(wlc_hw->osh, DUMP_SIZE);
				if (buf != NULL) {
					bcm_binit(&b, buf, DUMP_SIZE);
					for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
						if (wlc_hw->di[i] &&
							wlc_bmac_rxfifo_enab(wlc_hw, i)) {
							dma_dumprx(wlc_hw->di[i], &b, FALSE);
							WL_PRINT(("%s", buf));
						}
					}
				MFREE(wlc_hw->osh, buf, DUMP_SIZE);
				}
#endif // endif
				if (dma_stuckerr) {
					wlc_bmac_update_dma_rx_ctrlflags_bhalt(wlc_hw);
				}
				OSL_SYS_HALT();
			}

			wlc_mhf(wlc, MHF1, MHF1_RXFIFO1, MHF1_RXFIFO1, WLC_BAND_ALL);
			wlc_bmac_enable_mac(wlc->hw);
		}
	}
} /* wlc_bmac_enable_rx_hostmem_access */

#ifndef WL_DUALMAC_RSDB
bool
wlc_bmac_rsdb_cap(wlc_hw_info_t *wlc_hw)
{
	bool chip_rsdb = FALSE;
	bool dm_rsdb = FALSE;

	BCM_REFERENCE(chip_rsdb);
	/* in BMAC_PHASE_1, there is no wlc and so, we need to get the chip check as done in
	 * wlc_bmac_attach where machwcap1 is not available
	 */
	if (wlc_hw->bmac_phase == BMAC_PHASE_1) {
		if (BCM4347_CHIP(wlc_hw->sih->chip))
			chip_rsdb = TRUE;
		dm_rsdb = WLC_DUALMAC_RSDB_WRAP(chip_rsdb);
		wlc_hw->num_mac_chains = si_numcoreunits(wlc_hw->sih, D11_CORE_ID);
	} else {
		dm_rsdb = WLC_DUALMAC_RSDB(wlc_hw->wlc->cmn);
	}

	if (dm_rsdb) {
		return FALSE;
	} else {
		bool hwcap = FALSE;

		ASSERT(wlc_hw != NULL);
		hwcap = (wlc_hw->num_mac_chains > 1) ? TRUE : FALSE;
		return hwcap;
	}
}
#endif /* WL_DUALMAC_RSDB */

void
wlc_bmac_init_core_reset_disable_fn(wlc_hw_info_t *wlc_hw)
{
	ASSERT(wlc_hw != NULL);

#ifdef WLRSDB
	if (wlc_bmac_rsdb_cap(wlc_hw)) {
		wlc_hw->mac_core_reset_fn = si_d11rsdb_core_reset;
		wlc_hw->mac_core_disable_fn = si_d11rsdb_core_disable;

	} else

#endif // endif
	{
		wlc_hw->mac_core_reset_fn = si_core_reset;
		wlc_hw->mac_core_disable_fn = si_core_disable;
	}
}

void
wlc_bmac_core_reset(wlc_hw_info_t *wlc_hw, uint32 flags, uint32 resetflags)
{
	int idx;

	if (!wlc_hw || !wlc_hw->mac_core_reset_fn)
		return;

#ifdef WLRSDB
	if (!wlc_bmac_rsdb_cap(wlc_hw) || (wlc_rsdb_is_other_chain_idle(wlc_hw->wlc) == TRUE))
#endif // endif
	{
		flags |= (D11REV_GE(wlc_hw->corerev, 130) ? SICF_FASTCLKRQ : 0);
		resetflags |= (D11REV_GE(wlc_hw->corerev, 130) ? SICF_FASTCLKRQ : 0);
		if (D11REV_IS(wlc_hw->corerev, 130) &&
		    (wlc_hw->wlc == NULL || wlc_hw->wlc->state != WLC_STATE_GOING_UP)) {
			flags &= ~SICF_FASTCLKRQ; /* otherwise radio remains powered up */
		}
		(wlc_hw->mac_core_reset_fn)(wlc_hw->sih, flags, resetflags); // eg si_core_reset()
	}

	 /* If sysmem on chip, need to make sure the sysmem core is up before use */
	idx = si_coreidx(wlc_hw->sih);
	if (si_setcore(wlc_hw->sih, SYSMEM_CORE_ID, 0)) {
		if (!si_iscoreup(wlc_hw->sih))
			si_core_reset(wlc_hw->sih, 0, 0);
		si_setcoreidx(wlc_hw->sih, idx);

		/* Now program the location the MAC Buffer Memory region begins */
		if (BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) {
			uint32 d11mac_sysm_startaddr_h = 0;
			uint32 d11mac_sysm_startaddr_l = 0;

			/* Enable IHR for programming below regs. */
			wlc_bmac_mctrl(wlc_hw, ~0, MCTL_IHR_EN);

			if (D11REV_GE(wlc_hw->corerev, 128)) {
				d11mac_sysm_startaddr_h = D11MAC_SYSM_STARTADDR_H_REV128;
				d11mac_sysm_startaddr_l = D11MAC_SYSM_STARTADDR_L_REV128;
			} else {
				d11mac_sysm_startaddr_h = D11MAC_SYSM_STARTADDR_H_REV64;
				d11mac_sysm_startaddr_l = D11MAC_SYSM_STARTADDR_L_REV64;
			}

			W_REG(wlc_hw->osh, D11_SysMStartAddrHi(wlc_hw),
				d11mac_sysm_startaddr_h);
			W_REG(wlc_hw->osh, D11_SysMStartAddrLo(wlc_hw),
				d11mac_sysm_startaddr_l);
		}
	}

#if !defined(DONGLEBUILD)
	if (D11REV_IS(wlc_hw->corerev, 128) && BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
		idx = si_coreidx(wlc_hw->sih);
		if (si_setcore(wlc_hw->sih, ARMCA7_CORE_ID, 0)) {
			/* Enable ARM ca7 clock */
			si_core_cflags(wlc_hw->sih, (SICF_FGC | SICF_CLOCK_EN),
				(SICF_FGC | SICF_CLOCK_EN));
			OSL_DELAY(1);
			/* Disable ARM ca7 clock */
			si_core_cflags(wlc_hw->sih, (SICF_FGC | SICF_CLOCK_EN), 0);

			si_setcoreidx(wlc_hw->sih, idx);
		}
	}
#endif /* !DONGLEBUILD */
} /* wlc_bmac_core_reset */

#define IS_2X2AX_A0(sih) (EMBEDDED_2x2AX_CORE(sih->chip) && CHIPREV(sih->chiprev) == 0)

void
wlc_bmac_core_disable(wlc_hw_info_t *wlc_hw, uint32 bits)
{
	if (!wlc_hw || !wlc_hw->mac_core_disable_fn)
		return;
	if (IS_2X2AX_A0(wlc_hw->sih)) {
		if (wlc_hw->mac_core_reset_fn != NULL) {
			wlc_bmac_core_reset(wlc_hw, SICF_PRST | SICF_PCLKE | bits, bits);
		}
		return;
	}
#ifdef WLRSDB
	if (!wlc_bmac_rsdb_cap(wlc_hw) || (wlc_rsdb_is_other_chain_idle(wlc_hw->wlc) == TRUE))
#endif // endif
		(wlc_hw->mac_core_disable_fn)(wlc_hw->sih, bits);
}

/** This returns the context of last D11 core units using wlc_hw->macunit identity. */
bool
wlc_bmac_islast_core(wlc_hw_info_t *wlc_hw)
{
	ASSERT(wlc_hw != NULL);
	ASSERT(wlc_hw->sih != NULL);
	return (wlc_hw->macunit ==
		(wlc_numd11coreunits(wlc_hw->sih) - 1));
}

/**
 * FIFO- interrupt state machine is explained below
 * FIFO0-INT	FIFO1-INT	DECODE AS
 * 0		0		Idle state; no interrupt recieved on this pkt
 * 0		1		fifo-1 interrupt recieved; waiting for fifo-0 int
 * 1		0		fifo-0 interrupt recieved; waiting for fifo-1 int
 */
#if defined(BCMPCIEDEV)
#ifdef BULKRX_PKTLIST
void
wlc_bmac_process_split_fifo_pkt(wlc_hw_info_t *wlc_hw, void* p, uint8 rxh_offset)
{
	wlc_info_t *wlc = wlc_hw->wlc;
	uint16 convstatus;
	bool hdr_converted = FALSE;
	uint16 fifo0len;
	d11rxhdr_t *rxh;

	WL_TRACE(("wl%d: %s BMAC rev pkt\n\n", wlc_hw->unit, __FUNCTION__));

	/* Reset  Rxlfrag flags, XXX PKTFREE doesnt clear rx lfrag flags.
	 * Cant find a better single place to reset this.
	 */
	PKTRESETRXCORRUPTED(wlc->osh, p);

	/* Initialize */
	rxh = (d11rxhdr_t *)(PKTDATA(wlc_hw->osh, p) + rxh_offset);

	/* All rxstatus access below assumes its rev 128+ */
	ASSERT(D11REV_GE(wlc_hw->corerev, 128));

#ifdef RX_DEBUG_ASSERTS
	/* Store 100 entries of previous RXStatus info for debug */
	if (wlc->rxs_bkp) {
		uint16 cur_idx = wlc->rxs_bkp_idx;
		uint8* cur_ptr = (uint8*)((uint8*)wlc->rxs_bkp + (cur_idx * PER_RXS_SIZE));

		/* Store the RXS */
		memcpy(cur_ptr, rxh, PER_RXS_SIZE);

		/* Move to next store location */
		wlc->rxs_bkp_idx = (wlc->rxs_bkp_idx + 1) % MAX_RXS_BKP_ENTRIES;
	}
#endif /* RX_DEBUG_ASSERTS */
	/* length & conv status */
	fifo0len = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, RxFrameSize_0));
	convstatus = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, HdrConvSt));
	hdr_converted = ((convstatus & HDRCONV_ENAB) ? TRUE : FALSE);

	if (hdr_converted) {
		/* Successfull Header conversion */
		/* Check for max host frag length */
		if (fifo0len > PKTFRAGLEN(wlc_hw->osh, p, 1)) {
			WL_ERROR(("%s: Invalid host length : Pkt %p  fifo0len 0x%x "
				"convstatus 0x%04x padlen 0x%x plcplen  0x%x \n",
				__FUNCTION__, p, fifo0len, convstatus,
				RXHDR_GET_PAD_LEN(rxh, wlc),
				D11_PHY_RXPLCP_LEN(wlc_hw->corerev)));

			PKTSETRXCORRUPTED(wlc->osh, p);
		}

		PKTSETFRAGUSEDLEN(wlc_hw->osh, p, fifo0len);
		PKTSETHDRCONVTD(wlc_hw->osh, p);
	} else {
		/* Header conversion Disabled/Failed  */
		wlc_tunables_t *tune = wlc->pub->tunables;
		uint16 copycount = tune->copycount;

		/* If there is a decrypt failure, ucode disables HW header conversion
		 * So catch the cases where conversion failed but encryption passed.
		 * All other pkts should pass hdr conversion.
		 */
		if ((wlc_hw->hdrconv_mode) && RXS_UCODESTS_VALID_REV128(wlc, rxh)) {
			uint16 rxstatus1 = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, RxStatus1));
			uint16 rxstatus2 = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, RxStatus2));
			uint8 dma_flags = (D11RXHDR_GE128_ACCESS_VAL(rxh, dma_flags));

			BCM_REFERENCE(rxstatus1);
			BCM_REFERENCE(dma_flags);
			if (((rxstatus1 & RXS_DECERR) == 0) && ((rxstatus2 & RXS_TKMICERR) == 0)) {
				WL_ERROR(("Header conversion Failed WITHOUT decrypt error: p %p "
					"Conv status 0x%04x Rxstatsu1 0x%04x  dmaflags %02x\n",
					p, convstatus, rxstatus1, dma_flags));
#ifdef RX_DEBUG_ASSERTS
				/* Dump previous RXS info */
				wlc_print_prev_rxs(wlc);
#endif /* RX_DEBUG_ASSERTS */
#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(BCM_11AXHDR_DBG)
				wlc_print_ge128_rxhdr(wlc, &rxh->ge128);
#endif // endif
				ASSERT(0);
			}
		}

#ifdef WL_MONITOR
		if (MONITOR_ENAB(wlc_hw->wlc))
			copycount = 0;
#endif /* WL_MONITOR */
		if (fifo0len <= ((uint16)(copycount * 4))) {
			PKTSETFRAGUSEDLEN(wlc_hw->osh, p, 0);
		} else {
			PKTSETFRAGUSEDLEN(wlc_hw->osh, p,
				(fifo0len - (copycount * 4)));
		}
	}
} /* wlc_bmac_process_split_fifo_pkt */
#else
int
wlc_bmac_process_split_fifo_pkt(wlc_hw_info_t *wlc_hw, uint fifo, void* p)
{
	WL_TRACE(("wl%d: %s BMAC rev pkt on fifo %d \n\n", wlc_hw->unit, __FUNCTION__, fifo));

	/* All rxstatus access below assumes its rev 128+ */
	ASSERT(D11REV_GE(wlc_hw->corerev, 128));

	if (fifo == RX_FIFO) {
		if (PKTISFIFO0INT(wlc_hw->osh, p)) {
			/* FIFO-0 cant be set while processing fifo-0 int */
			WL_ERROR(("Error:FIFO-0 allready set for pkt %p \n", OSL_OBFUSCATE_BUF(p)));
		} else {
			/* fifo-0 is the first int */
			PKTSETFIFO0INT(wlc_hw->osh, p);
		}
	}
	if (fifo == RX_FIFO1) {
		if (PKTISFIFO1INT(wlc_hw->osh, p)) {
			/* fifo-1 int cant be set during F-1 processing */
			WL_ERROR(("Error: fifo-1 allready set for %p \n", OSL_OBFUSCATE_BUF(p)));
		} else {
			/* Set F1 int */
			PKTSETFIFO1INT(wlc_hw->osh, p);
		}

	}

	if (!(PKTISFIFO0INT(wlc_hw->osh, p) && PKTISFIFO1INT(wlc_hw->osh, p))) {
		/* both fifos not set, return */
		return 0;
	} else {
		/* Recieved both interrupts, Proceed with rx processing */
		/* retrieve fifo-0 len */
		uint16 fifo0len;
		d11rxhdr_t *rxh;
		uint16 convstatus = 0;
		bool hdr_converted = FALSE;
		wlc_info_t *wlc = wlc_hw->wlc;

		rxh = (d11rxhdr_t *)PKTDATA(wlc_hw->osh, p);
		/* reset interrupt bits */
		PKTRESETFIFO0INT(wlc_hw->osh, p);
		PKTRESETFIFO1INT(wlc_hw->osh, p);

		/* length & conv status */
		fifo0len = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, RxFrameSize_0));
		convstatus = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, HdrConvSt));
		hdr_converted = ((convstatus & HDRCONV_ENAB) ? TRUE : FALSE);
		if (hdr_converted) {
			/* Successfull Header conversion */

			/* Check for max host frag length */
			if (fifo0len > PKTFRAGLEN(wlc_hw->osh, p, 1)) {
				WL_ERROR(("%s: Invalid host length : Pkt %p  fifo0len 0x%x "
					"convstatus 0x%02x padlen 0x%x plcplen  0x%x \n",
					__FUNCTION__, p, fifo0len, convstatus,
					RXHDR_GET_PAD_LEN(rxh, wlc),
					D11_PHY_RXPLCP_LEN(wlc_hw->corerev)));

#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(BCM_11AXHDR_DBG)
				wlc_print_ge128_rxhdr(wlc, &rxh->ge128);
#endif // endif
				ASSERT(0);
			}
			PKTSETFRAGUSEDLEN(wlc_hw->osh, p, fifo0len);
			PKTSETHDRCONVTD(wlc_hw->osh, p);
		} else {
			/* Header conversion Disabled */
			wlc_tunables_t *tune = wlc->pub->tunables;
			uint16 copycount = tune->copycount;
			uint16 rxstatus1 = ltoh16(D11RXHDR_GE128_ACCESS_VAL(rxh, RxStatus1));

			BCM_REFERENCE(rxstatus1);
#ifdef HW_HDR_CONVERSION
			WL_ERROR(("%s : Header conversion Failed for pkt %p : Conv status 0x%04x "
				"RXstatus 1 0x%04x \n", __FUNCTION__, p,
				convstatus, rxstatus1));

#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(BCM_11AXHDR_DBG)
			wlc_print_ge128_rxhdr(wlc, &rxh->ge128);
#endif // endif

			/* In SPLIT MODE-4, only decrypt failure pkts should fail HDR conversion */
			ASSERT(rxstatus1 & RXS_DECERR);
#endif /* HW_HDR_CONVERSION */

#ifdef WL_MONITOR
			if (MONITOR_ENAB(wlc_hw->wlc))
				copycount = 0;
#endif /* WL_MONITOR */
			if (fifo0len <= ((uint16)(copycount * 4))) {
				PKTSETFRAGUSEDLEN(wlc_hw->osh, p, 0);
			} else {
				PKTSETFRAGUSEDLEN(wlc_hw->osh, p,
					(fifo0len - (copycount * 4)));
			}
		}
		return 1;
	}
} /* wlc_bmac_process_split_fifo_pkt */
#endif /* BCMPCIEDEV */

#endif /* BCMPCIEDEV */

/** Check if given rx fifo is valid */
static uint8
wlc_bmac_rxfifo_enab(wlc_hw_info_t *hw, uint fifo)
{
	switch (fifo) {
		case RX_FIFO :
			return 1;
			break;
		case RX_FIFO1:
#ifdef FORCE_RX_FIFO1
			/* in 4349a0, fifo-2 classification will work only if fifo-1 is enabled */
			return 1;
#endif /* FORCE_RX_FIFO1 */
			return ((uint8)(PKT_CLASSIFY_EN(RX_FIFO1) || RXFIFO_SPLIT()));
			break;
		case RX_FIFO2 :
			return ((uint8)(PKT_CLASSIFY_EN(RX_FIFO2)));
			break;
#ifdef STS_FIFO_RXEN
		case STS_FIFO :
			return (uint8)(STS_RX_ENAB(hw->wlc->pub) ? 1 : 0);
			break;
#endif /* STS_FIFO_RXEN */
		default :
			WL_INFORM(("wl%d: %s: Unsupported FIFO %d\n", hw->unit,
				__FUNCTION__, fifo));
			return 0;
	}
}

#ifdef BCMDBG_TXSTUCK
void wlc_bmac_print_muted(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	if (!wlc->clk) {
		bcm_bprintf(b, "Need clk to dump bmac tx stuck information\n");
		return;
	}

	bcm_bprintf(b, "tx suspended: %d %d, susp depth %d, muted %d %d, mac_enab %d\n",
		wlc_tx_suspended(wlc),
		wlc->tx_suspended,
		wlc->hw->mac_suspend_depth,
		wlc_phy_ismuted(wlc->hw->band->pi),
		wlc->hw->mute_override,
		(R_REG(wlc->hw->osh, D11_MACCONTROL(wlc->hw)) & MCTL_EN_MAC));

	bcm_bprintf(b, "post=%d rxactive=%d txactive=%d txpend=%d\n",
		NRXBUFPOST,
		dma_rxactive(wlc->hw->di[RX_FIFO]),
		dma_txactive(wlc->hw->di[1]),
		dma_txpending(wlc->hw->di[1]));

	if (wlc->pub->pktpool) {
		bcm_bprintf(b, "pktpool callback disabled: %d\n\n",
			pktpool_emptycb_disabled(wlc->pub->pktpool));
	}
}
#endif /* BCMDBG_TXSTUCK */

/* Check if programmed SICF_BW bits match with the current CHANSPEC BW */
int
wlc_bmac_bw_check(wlc_hw_info_t *wlc_hw)
{
	if ((si_core_cflags(wlc_hw->sih, 0, 0) & SICF_BWMASK) == wlc_bmac_clk_bwbits(wlc_hw))
		return BCME_OK;

	return BCME_BADCHAN;
}

/* get slice specific OTP/SROM parameters */
int
getintvar_slicespecific(wlc_hw_info_t *wlc_hw, char *vars, const char *name)
{
	int ret = 0;
	char *name_with_prefix = NULL;
	/* if accessor is initalized with slice/<slice_index>string */
	if (wlc_hw->vars_table_accessor[0] !=  0) {
		uint16 sz = (strlen(name) + strlen(wlc_hw->vars_table_accessor)+1);
		/* freed in same function */
		name_with_prefix = (char *) MALLOC_NOPERSIST(wlc_hw->osh, sz);
		/* Prepare fab name */
		if (name_with_prefix == NULL) {
			WL_ERROR(("wl: %s: MALLOC failure\n", __FUNCTION__));
			return 0;
		}
		/* prefix accessor to the vars-name */
		name_with_prefix[0] = 0;
		bcmstrcat(name_with_prefix, wlc_hw->vars_table_accessor);
		bcmstrcat(name_with_prefix, name);

		ret = (getvar(vars, name_with_prefix) == NULL) ?
			getintvar(vars, name) : getintvar(vars, name_with_prefix);
		MFREE(wlc_hw->osh, name_with_prefix, sz);
	}
	else {
		ret = getintvar(vars, name);
	}
	return ret;
}

int
wlc_bmac_reset_txrx(wlc_hw_info_t *wlc_hw)
{
	int err = BCME_OK;
	wlc_info_t *wlc;

	ASSERT(wlc_hw != NULL);

	wlc = wlc_hw->wlc;
	wlc_sync_txfifo_all(wlc, wlc->active_queue, SYNCFIFO);
	return err;
}

int
wlc_bmac_bmc_dyn_reinit(wlc_hw_info_t *wlc_hw, uint8 bufsize_in_256_blocks)
{
	osl_t *osh;
	uint32 bufsize = D11MAC_BMC_BUFSIZE_512BLOCK;
	uint32 fifo_sz, txfifo_sz, bufsize_tx, bufsize_pertxtid, bufsizeptxid_percore, rxfifo_sz;
	uint32 bmc_startaddr = D11MAC_BMC_STARTADDR;
	uint32 doublebufsize = 0;
	wlc_info_t *wlc = wlc_hw->wlc;
	int num_of_fifo, rxmapfifosz;
	int bmc_tx_fifo_list[5] = {0, 1, 2, 3, 4};
	int bmc_rx_fifo_list[D11AC_MAX_RX_FIFO_NUM] = {6, 8};
	int i, fifo;
	uint16 minbufs;
	uint16 maxbufs;

	osh = wlc_hw->osh;

	WL_TRACE(("wl%d: %s\n", wlc_hw->unit, __FUNCTION__));

	/* Check the input parameters */
	ASSERT(wlc_hw->corerev >= 50);

	ASSERT(wlc_hw->bmc_params != NULL);

	/* bufsize is used everywhere - calculate number of buffers, programming bmcctl etc. */
	bufsize = bufsize_in_256_blocks;

	fifo_sz = wlc_bmac_get_txfifo_size_kb(wlc_hw) * 1024; /* fifo_sz in [bytes] */

	/* Account for bmc_startaddr which is specified in units of 256B */
	wlc_hw->bmc_maxbufs = (fifo_sz - (bmc_startaddr << 8)) >> (8 + bufsize);

	/* Calc:   448k(total) - (2* tplpercore + SR) - (rxq0 + rxq1) - 10(extra)= X
	 * each rx fifo requires 3 additional fifos which makes to 6 with additional
	 * margin as 10 buffers.
	 * X/512=Y buffers
	 * per txtid ie Y/6
	 */
	rxfifo_sz = (wlc_hw->bmc_params->minbufs[6] << (8 + bufsize)) +
	(wlc_hw->bmc_params->minbufs[8] << (8 + bufsize));
	txfifo_sz = fifo_sz - D11MAC_BMC_TPL_BYTES_PERCORE - D11MAC_BMC_SR_BYTES - rxfifo_sz - 10;
	bufsize_tx = txfifo_sz / (8 + bufsize);
	bufsize_pertxtid = bufsize_tx / (D11AC_MAX_FIFO_NUM - D11AC_MAX_RX_FIFO_NUM - 1);

	bufsizeptxid_percore = bufsize_pertxtid / (wlc->cmn->num_d11_cores);
	bufsizeptxid_percore = 0x800;

	if (bufsize ==  D11MAC_BMC_BUFSIZE_256BLOCK)
	{
		doublebufsize = 1;
	}

	/*
	Things to be changed for MIMO
		1.	MinBuffers for the 6 chain 0 TIDs double in size (for 2x2 throughput)
		2.	MinBuffers for the 6 chain 1 TIDs reduce to 0
		3.	TXAllMaxBuffers for chain 0 increased
			to match BM buffer size to eliminate this limit
		4.	RXFIFO size for the two chain 0
			RX channels need to be doubled (for 2x2 throughput)
		5.	Reduce RXFIFO size for the two chain 1 RX channels to minimum allowed (TBD)

		Change BMC to MIMO mode
	*/
	if ((wlc_bmac_rsdb_cap(wlc_hw)) &&
		(wlc_rsdb_mode(wlc_hw->wlc) ==  PHYMODE_MIMO)) {
		WL_TRACE(("wl%d: In dynamic re init:TO MIMO %s\n", wlc_hw->unit, __FUNCTION__));
		/* Decrease core1 rxfifo sz */
		num_of_fifo = D11AC_MAX_RX_FIFO_NUM;

		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_rx_fifo_list[i];
			W_REG(osh, D11_RXMapFifoSize(wlc_hw), 0x1);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxffszlden_SHIFT) |
			(fifo << 1)) | (BMCCmd_Core1_Sel_MASK));
			if (fifo == 6) {
				/* Delay may require to be 1sec */
				SPINWAIT(!(R_REG(wlc_hw->osh,
				D11_RXMapStatus(wlc_hw)) & 0x1), 1000);
				if (R_REG(wlc_hw->osh,
					D11_RXMapStatus(wlc_hw)) & 0x1) {
						WL_INFORM(("wl%d: Success:"
						"Core 1 rx fifo zero decreased\n",
						wlc_hw->unit));
				} else {
					WL_ERROR(("wl%d:Error:"
					"Waiting for core 1 rx fifo zero decrease\n",
					wlc_hw->unit));
				}
			}

			if (fifo == 8) {
				/* Delay may require to be 1sec */
				SPINWAIT(!(R_REG(wlc_hw->osh,
				D11_RXMapStatus(wlc_hw)) & 0x2), 1000);
				if (R_REG(wlc_hw->osh, D11_RXMapStatus(wlc_hw)) & 0x2) {
					WL_INFORM(("wl%d: Success:"
						"Core 1 rx fifo one decreased\n", wlc_hw->unit));
				} else {
					WL_ERROR(("wl%d: Error:"
						"Waiting for core 1 rx fifo one decrease\n",
						wlc_hw->unit));
				}
			}

			/* 3 buffers more than rxmapfifo size */
			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), 0x800);
			W_REG(osh, D11_BMCMinBuffers(wlc_hw), 0x4);
			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) | (BMCCmd_Core1_Sel_MASK));

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
			(1 << BMCCmd1_Minmaxappall_SHIFT));

			/* Read MaxMinMet to confirm the decreased buffer space.
			 * Delay may require to be 1sec
			 */
			SPINWAIT(!(R_REG(wlc_hw->osh,
			D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff), 1000);
			if (R_REG(wlc_hw->osh, D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) {
				WL_INFORM(("wl%d:Success:"
					"Core 1 tx fifo decreased\n", wlc_hw->unit));
			} else {
				WL_ERROR(("wl%d:Error:"
					"Polling for core 1 tx fifo decrease\n", wlc_hw->unit));
			}
		}

		WL_TRACE(("wl%d:To:MIMO:RxCore1 reduced.\n", wlc_hw->unit));
		/* Increase core 0 max buf to max value */
		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_rx_fifo_list[i];
			maxbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize)  + 3;
			minbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize) + 3;
			/* overide to sync with TCL changes */
			maxbufs = 0x800;

			rxmapfifosz = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) | (fifo << 1));
			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), maxbufs);

			W_REG(osh, D11_BMCMinBuffers(wlc_hw), minbufs);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) & (~BMCCmd_Core1_Sel_MASK));

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
			(1 << BMCCmd1_Minmaxappall_SHIFT));

			/* Read MaxMinMet to confirm the decreased buffer space.
			 * Delay may require to be 1sec
			 */
			SPINWAIT((!(R_REG(wlc_hw->osh,
			D11_BMCDynAllocStatus(wlc_hw)) & 0x1ff)), 1000);
			if (R_REG(wlc_hw->osh, D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) {
				WL_INFORM(("wl%d:Success:"
				"Polling for core 0 rx fifo increase\n", wlc_hw->unit));
			} else {
				WL_ERROR(("wl%d:Error:"
				"Polling for core 0 rx fifo increase\n", wlc_hw->unit));
			}

			W_REG(osh, D11_RXMapFifoSize(wlc_hw), rxmapfifosz);

			if (HW_RXFIFO_0_WAR(wlc_hw->corerev)) {
				wlc_war_rxfifo_shm(wlc_hw, fifo, rxmapfifosz);
			}

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
			(1 << BMCCmd1_Minmaxffszlden_SHIFT) |
			(fifo << 1)) & (~BMCCmd_Core1_Sel_MASK));
		}

		WL_TRACE(("wl%d:To:MIMO:RxCore0 increased.\n", wlc_hw->unit));

		/* TX buffers:decrease core 1 min/max buf to 0:Change values: */
		num_of_fifo = 5;

		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_tx_fifo_list[i];

			W_REG(osh, D11_BMCMinBuffers(wlc_hw), 0x0);
			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), 0x800);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) | (BMCCmd_Core1_Sel_MASK));

			W_REG(osh, D11_BMCCmd(wlc_hw), (R_REG(wlc_hw->osh,
				D11_BMCCmd(wlc_hw))) | fifo << 0);
		}

		/* increase core 0 min/max buf to max val */
		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_tx_fifo_list[i];
			minbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);

			W_REG(osh, D11_BMCMinBuffers(wlc_hw), minbufs);

			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), 0x800);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) & (~BMCCmd_Core1_Sel_MASK));

			W_REG(osh, D11_BMCCmd(wlc_hw), (R_REG(wlc_hw->osh,
			D11_BMCCmd(wlc_hw)) | fifo << 0));
		}

		/* 3 buffers per tid *6 buffers for core 1 */
		/* calc buffers per tid *6 buffers for core 0 */

		W_REG(osh, D11_BMCCore1TXAllMaxBuffers(wlc_hw), 0x800);

		W_REG(osh, D11_BMCCore0TXAllMaxBuffers(wlc_hw), 0x800);

		W_REG(osh, D11_BMCCmd1(wlc_hw),
		(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
		(1 << BMCCmd1_Minmaxappall_SHIFT));

		/* Read MaxMinMet to confirm the decreased buffer space. */
		SPINWAIT((!((R_REG(wlc_hw->osh,
		D11_BMCDynAllocStatus(wlc_hw)) & 0xdff) == 0xdff)) ||
		(!((R_REG(wlc_hw->osh,
		D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) == 0x1ff)), 1000);
		if (((R_REG(wlc_hw->osh,
			D11_BMCDynAllocStatus(wlc_hw)) & 0xdff) == 0xdff) ||
			((R_REG(wlc_hw->osh,
			D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) == 0x1ff)) {

			WL_INFORM(("wl%d:Sucess:polling Dyn all sts:"
			"$reg(TXE_BMCDynAllocStatus) $reg(TXE_BMCDynAllocStatus1)\n",
			wlc_hw->unit));
		} else {
			WL_ERROR(("wl%d:Error:polling Dyn all sts:"
				"$reg(TXE_BMCDynAllocStatus) $reg(TXE_BMCDynAllocStatus1)\n",
				wlc_hw->unit));
		}

		WL_TRACE(("wl%d:ToMIMO: Tx core 0 increased\n", wlc_hw->unit));
		WL_TRACE(("wl%d:To MIMO:Tx Core1 reduced\n", wlc_hw->unit));
	}

	/* MIMO to RSDB mode: */
	if ((wlc_bmac_rsdb_cap(wlc_hw)) &&
		(!si_coreunit(wlc_hw->sih) && (wlc_rsdb_mode(wlc_hw->wlc) == PHYMODE_RSDB))) {
		WL_TRACE(("wl%d:In dynamic reinit:TO RSDB\n", wlc_hw->unit));

		/* MinBuffers for the 6 chain 0 tids to reduce to half of that in MIMO.
		 * Decrease core 0 rx fifo size
		 */
		num_of_fifo = D11AC_MAX_RX_FIFO_NUM;
		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_rx_fifo_list[i];
			rxmapfifosz = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);
			if (fifo == 6) {
				/* reduce to half */
				rxmapfifosz = rxmapfifosz >> 1;
			}

			W_REG(osh, D11_RXMapFifoSize(wlc_hw), rxmapfifosz);
			if (HW_RXFIFO_0_WAR(wlc_hw->corerev)) {
				wlc_war_rxfifo_shm(wlc_hw, fifo, rxmapfifosz);
			}

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxffszlden_SHIFT) |
			(fifo << 1)) & (~BMCCmd_Core1_Sel_MASK));
			if (fifo == 6) {
				/* Delay may require to be 1sec */
				SPINWAIT(!(R_REG(wlc_hw->osh,
				D11_RXMapStatus(wlc_hw)) & 0x1), 1000);
				if (R_REG(wlc_hw->osh, D11_RXMapStatus(wlc_hw)) & 0x1) {
						WL_INFORM(("wl%d: Success:"
						"Waiting for core 1 rx fifo zero decrease\n",
						wlc_hw->unit));
				} else {
						WL_ERROR(("wl%d: Error:"
						"Waiting for core 1 rx fifo zero decrease\n",
						wlc_hw->unit));
				}
			}

			if (fifo == 8) {
				/* Delay may require to be 1sec */
				SPINWAIT(!(R_REG(wlc_hw->osh,
				D11_RXMapStatus(wlc_hw)) & 0x2), 1000);
				if (R_REG(wlc_hw->osh, D11_RXMapStatus(wlc_hw)) & 0x2) {
						WL_INFORM(("wl%d: Success:"
						"Waiting for core 1 rx fifo one decrease\n",
						wlc_hw->unit));
				} else {
						WL_ERROR(("wl%d: Error:"
						"Waiting for core 1 rx fifo one decrease\n",
						wlc_hw->unit));
				}
			}

			maxbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);

			if (fifo == 6) {
				/* reduce to half */
				maxbufs = maxbufs >> 1;
			}

			maxbufs = maxbufs + 3;
			minbufs = maxbufs;

			/* overide to sync with TCL changes */
			maxbufs = 0x800;
			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), maxbufs);
			W_REG(osh, D11_BMCMinBuffers(wlc_hw), minbufs);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) & (~BMCCmd_Core1_Sel_MASK));

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
			(1 << BMCCmd1_Minmaxappall_SHIFT));

			SPINWAIT((!(R_REG(wlc_hw->osh,
			D11_BMCDynAllocStatus(wlc_hw)) & 0x1ff)), 1000);
			if (R_REG(wlc_hw->osh, D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) {
				WL_INFORM(("wl%d:Success:"
				"Polling for core 0 rx fifo increase\n", wlc_hw->unit));
			} else {
				WL_ERROR(("wl%d:Error:"
				"Polling for core 0 rx fifo increase\n", wlc_hw->unit));
			}
		}
		WL_TRACE(("wl%d:To RSDB:Core0 Rx fifo size reduced.\n", wlc_hw->unit));
		/* Increase core 1 rxfifo size */
		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_rx_fifo_list[i];

			maxbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);

			if (fifo == 6) {
				/* reduce to half */
				maxbufs = maxbufs >> 1;
			}

			maxbufs = maxbufs + 3;
			minbufs = maxbufs;

			/* overide to sync with TCL changes */
			maxbufs = 0x800;
			rxmapfifosz = minbufs - 3;

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) | (fifo << 1));

			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), maxbufs);

			W_REG(osh, D11_BMCMinBuffers(wlc_hw), minbufs);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) | (BMCCmd_Core1_Sel_MASK));

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
			(1 << BMCCmd1_Minmaxappall_SHIFT));

			/* Read MaxMinMet to confirm the applied buffer space.
			 * Delay may require to be 1sec
			 */
			SPINWAIT((!(R_REG(wlc_hw->osh,
			D11_BMCDynAllocStatus(wlc_hw)) & 0x1ff)), 1000);
			if (R_REG(wlc_hw->osh,
			D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) {
				WL_INFORM(("wl%d:Success:"
				"Polling for core 0 rx fifo increase\n", wlc_hw->unit));
			} else {
				WL_ERROR(("wl%d:Error:"
				"Polling for core 0 rx fifo increase\n", wlc_hw->unit));
			}

			W_REG(osh, D11_RXMapFifoSize(wlc_hw), rxmapfifosz);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxffszlden_SHIFT) | (fifo << 1)) |
			(BMCCmd_Core1_Sel_MASK));
		}

		WL_TRACE(("wl%d:To RSDB:Core1 Rx size increased.\n", wlc_hw->unit));
		/* Core 1 done */

		/* To RSDB:Tx buffers
		 * Tx buffers: Decrease core 0 number of buffers
		 * and increase core 1 number of buffers
		 * Decrease core 0 buffers to half its original value ,ie 0x73/2 = 0x39
		 */
		num_of_fifo = 5;

		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_tx_fifo_list[i];
			minbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);

			W_REG(osh, D11_BMCMinBuffers(wlc_hw), minbufs);

			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), bufsizeptxid_percore);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) & (~BMCCmd_Core1_Sel_MASK));
		}
		/* Core 1 buffers */
		for (i = 0; i < num_of_fifo; i++) {
			fifo =  bmc_tx_fifo_list[i];
			/* increase core 1 min/max buf to max val */
			minbufs = (wlc_hw->bmc_params->minbufs[fifo] << doublebufsize);

			W_REG(osh, D11_BMCMinBuffers(wlc_hw), minbufs);

			W_REG(osh, D11_BMCMaxBuffers(wlc_hw), bufsizeptxid_percore);

			W_REG(osh, D11_BMCCmd1(wlc_hw),
			((R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw)) & 0x0) |
			(1 << BMCCmd1_Minmaxlden_SHIFT) | (fifo << 1)) | (BMCCmd_Core1_Sel_MASK));
		}

		W_REG(osh, D11_BMCCore1TXAllMaxBuffers(wlc_hw), 0x800);

		W_REG(osh, D11_BMCCore0TXAllMaxBuffers(wlc_hw), 0x800);

		W_REG(osh, D11_BMCCmd1(wlc_hw),
		(R_REG(wlc_hw->osh, D11_BMCCmd1(wlc_hw))) |
		(1 << BMCCmd1_Minmaxappall_SHIFT));

		/* Read MaxMinMet to confirm the decreased buffer space. */
		SPINWAIT((!((R_REG(wlc_hw->osh,
		D11_BMCDynAllocStatus(wlc_hw)) & 0xdff) == 0xdff)) ||
		(!((R_REG(wlc_hw->osh,
		D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) == 0x1ff)), 1000);
		if (((R_REG(wlc_hw->osh,
		D11_BMCDynAllocStatus(wlc_hw)) & 0xdff) == 0xdff) ||
		((R_REG(wlc_hw->osh, D11_BMCDynAllocStatus1(wlc_hw)) & 0x1ff) == 0x1ff)) {
			WL_INFORM(("wl%d:Success:polling Dyn all sts:"
			"$reg(TXE_BMCDynAllocStatus)"
			"$reg(TXE_BMCDynAllocStatus1) \n", wlc_hw->unit));
		} else {
			WL_ERROR(("wl%d:Error:polling Dyn all sts:"
			"$reg(TXE_BMCDynAllocStatus)"
			"$reg(TXE_BMCDynAllocStatus1) \n", wlc_hw->unit));
		}
		WL_TRACE(("wl%d:To RSDB: Tx core 0 increased\n", wlc_hw->unit));
		WL_TRACE(("wl%d:To RSDB:Tx Core1 reduced\n", wlc_hw->unit));
	}

	return 0;
}

/* update ucode to operate core/mode specific PM operation */
void
wlc_bmac_rsdb_mode_param_to_shmem(wlc_hw_info_t *wlc_hw)
{
	uint16 phymode;

	if (wlc_hw->macunit == 0) {

		if ((wlc_rsdb_mode(wlc_hw->wlc) ==  PHYMODE_MIMO)) {
			phymode = CORE0_MODE_MIMO;
		} else if ((wlc_rsdb_mode(wlc_hw->wlc) ==  PHYMODE_80P80)) {
			phymode = CORE0_MODE_80P80;
		} else {
			phymode = CORE0_MODE_RSDB;
		}

	} else {
		phymode = CORE1_MODE_RSDB;
	}

	wlc_bmac_write_shm(wlc_hw, M_MODE_CORE(wlc_hw), phymode);
	wlc_hw->shmphymode = phymode;
}

uint16
wlc_bmac_read_eci_data_reg(wlc_hw_info_t *wlc_hw, uint8 reg_num)
{
	W_REG(wlc_hw->osh, D11_BTCX_ECI_ADDR(wlc_hw), reg_num);
	return (R_REG(wlc_hw->osh, D11_BTCX_ECI_DATA(wlc_hw)));
}

/* dump bmac and PHY registered names */
int
wlc_bmac_dump_list(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b)
{
	if (wlc_hw->dump != NULL) {
		int ret = wlc_dump_reg_list(wlc_hw->dump, b);
		if (ret != BCME_OK)
			return ret;
	}

	return phy_dbg_dump_list((phy_info_t *)wlc_hw->band->pi, b);
}

/* dump bmac and PHY registries */
int
wlc_bmac_dump_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b)
{
	if (wlc_hw->dump != NULL) {
		int ret = wlc_dump_reg_dump(wlc_hw->dump, b);
		if (ret != BCME_OK)
			return ret;
	}

	return phy_dbg_dump_dump((phy_info_t *)wlc_hw->band->pi, b);
}

static void wlc_war_rxfifo_shm(wlc_hw_info_t *wlc_hw, uint fifo, uint fifo_size)
{
	uint fifo_0_shm_offset = 0;
	uint16 shm_val = 0;

	ASSERT(HW_RXFIFO_0_WAR(wlc_hw->corerev));

	if (fifo == 6) {
		fifo_0_shm_offset = M_WRDCNT_RXF0OVFL_THRSH(wlc_hw->wlc);
		shm_val = (fifo_size * FIFO_BLOCK_SIZE)>>2;
		wlc_bmac_write_shm(wlc_hw, fifo_0_shm_offset, shm_val);
	} else if (fifo == 8) {
		fifo_0_shm_offset = RXFIFO_1_OFFSET;
		fifo_0_shm_offset = M_WRDCNT_RXF1OVFL_THRSH(wlc_hw->wlc);
		shm_val = ((fifo_size * FIFO_BLOCK_SIZE)>>2) - 0x14;
		wlc_bmac_write_shm(wlc_hw, fifo_0_shm_offset, shm_val);
	}
}

unsigned int
wlc_bmac_shmphymode_dump(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->shmphymode;

}

uint
wlc_bmac_coreunit(wlc_info_t *wlc)
{
#if defined(WL_AUXCORE_TMP_NIC_SUPPORT)
	extern int auxcore;
	return (auxcore == 0) ? DUALMAC_MAIN : DUALMAC_AUX;
#else
	return (wlc->hw->macunit == 0 ? DUALMAC_MAIN : DUALMAC_AUX);
#endif // endif
}

#ifdef WAR4360_UCODE
uint32
wlc_bmac_need_reinit(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->need_reinit;
}
#endif /* WAR4360_UCODE */

bool
wlc_bmac_report_fatal_errors(wlc_hw_info_t *wlc_hw, int rc)
{
	bool ret = FALSE;

	if (wlc_hw->need_reinit != WL_REINIT_RC_NONE) {
		WL_ERROR(("need_reinit already set to %d, new rc %d\n", wlc_hw->need_reinit, rc));
		goto next;
	}
	wlc_hw->need_reinit = rc;

	if (rc == WL_REINIT_RC_MAC_WAKE) {
		WL_ERROR(("wl%d:%s: TRAP MAC still ASLEEP dbgst 0x%08x psmdebug 0x%04x\n",
			wlc_hw->unit, __FUNCTION__,
			wlc_bmac_read_shm(wlc_hw, M_UCODE_DBGST(wlc_hw)),
			R_REG(wlc_hw->wlc->osh, D11_PSM_DEBUG(wlc_hw->wlc))));
	} else if (rc == WL_REINIT_RC_MAC_SUSPEND) {
		WL_ERROR(("wl%d:%s: TRAP MAC still SUSPENDD\n",
			wlc_hw->unit, __FUNCTION__));
	} else if (rc == WL_REINIT_RC_MAC_SPIN_WAIT) {
		WL_ERROR(("wl%d:%s: TRAP MAC SPINWAIT TO error\n",
			wlc_hw->unit, __FUNCTION__));
	}
next:

#ifdef NEED_HARD_RESET
	/* In case of OSX, fatal errors are mapped to hard reset,
	 * which will be performed during power cycle thread call enter.
	 * Return true to indicate that caller must stop further processing
	 * untill reset is complete
	 */
	ret = TRUE;
#else
	ret = FALSE;
#endif // endif

	WLC_FATAL_ERROR(wlc_hw->wlc);	/* big hammer */

	return ret;
}

#ifdef WLRSDB
void
wlc_bmac_switch_pmu_seq(wlc_hw_info_t *wlc_hw, uint mode)
{
#ifdef DUAL_PMU_SEQUENCE
	ai_force_clocks(wlc_hw->sih, FORCE_CLK_ON);
	si_switch_pmu_dependency(wlc_hw->sih, mode);
	ai_force_clocks(wlc_hw->sih, FORCE_CLK_OFF);
#endif /* DUAL_PMU_SEQUENCE */
}
#endif /* WLRSDB */

/* ported from set_mac_clkgating in d11procs.tcl */
static void
wlc_bmac_enable_mac_clkgating(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	si_t *sih = wlc_hw->sih;
	osl_t *osh = wlc_hw->osh;
	d11regs_t *d11regs;
	uint32 val;
	uint corerev = wlc->pub->corerev;

	BCM_REFERENCE(sih);
	BCM_REFERENCE(val);
	wlc_bmac_suspend_mac_and_wait(wlc_hw);

	d11regs = wlc->regs;
	ASSERT(d11regs);
	BCM_REFERENCE(d11regs);

	/* PP commented: RB156035 not sure if we want to do clock gating for 6878,
	 * SR support has been removed
	 */
	if (D11REV_IS(corerev, 61)) { /* 4347/6878 */
		OR_REG(osh, D11_PSMPowerReqStatus(wlc_hw), 1 << AUTO_MEM_STBY_RET_SHIFT);

		/* PowerCtl bit 5: enable auto clock gating of memory clocks when idle */
		OR_REG(osh, D11_POWERCTL(wlc_hw), 1 << PWRCTL_ENAB_MEM_CLK_GATE_SHIFT);

		/* Enabling bm_sram clock gating */
		OR_REG(osh, D11_BMCCTL(wlc_hw), 1 << BMCCTL_CLKGATEEN_SHIFT);

		/* For 4347a0, refer to
		 * http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/Dot11macRev61
		 * http://confluence.broadcom.com/display/WLAN/4347A0+MAC+QT+Verification#
		 * id-4347A0MACQTVerification-ClockgatingRegSettings
		 */
		W_REG(osh, D11_CLK_GATE_REQ0(wlc_hw),
			(CLKREQ_MAC_HT << CLKGTE_PSM_PATCHCOPY_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_RXKEEP_OCP_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_PSM_MAC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_TSF_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_AQM_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_SERIAL_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_TX_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_POSTTX_CLK_REQ_SHIFT));

		W_REG(osh, D11_CLK_GATE_REQ1(wlc_hw),
			(CLKREQ_MAC_HT << CLKGTE_RX_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_TXKEEP_OCP_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_HOST_RW_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_IHR_WR_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_TKIP_KEY_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_TKIP_MISC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_AES_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_WAPI_CLK_REQ_SHIFT));

		W_REG(osh, D11_CLK_GATE_REQ2(wlc_hw),
			(CLKREQ_MAC_HT << CLKGTE_WEP_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_PSM_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_MACPHY_CLK_REQ_BY_PHY_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_FCBS_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_HIN_AXI_MAC_CLK_REQ_SHIFT));

		W_REG(osh, D11_CLK_GATE_STRETCH0(wlc_hw),
			(0x10 << CLKGTE_MAC_HT_CLOCK_STRETCH_SHIFT) |
			(0x10 << CLKGTE_MAC_ALP_CLOCK_STRETCH_SHIFT));

		val = R_REG(osh, D11_CLK_GATE_DIV_CTRL(wlc_hw));
		val = (val & ~CLKGTE_MAC_ILP_OFF_COUNT_MASK) |
			(7 << CLKGTE_MAC_ILP_OFF_COUNT_SHIFT);
		W_REG(osh, D11_CLK_GATE_DIV_CTRL(wlc_hw), val);

		W_REG(osh, D11_CLK_GATE_PHY_CLK_CTRL(wlc_hw),
			(1 << CLKGTE_PHY_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_O2C_HIN_PHY_CLK_EN_SHIFT) |
			(1 << CLKGTE_HIN_PHY_CLK_EN_SHIFT) |
			(1 << CLKGTE_IHRP_PHY_CLK_EN_SHIFT) |
			(1 << CLKGTE_CCA_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_TX_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_HRP_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_SYNC_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_RX_FRAME_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_RX_START_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_FCBS_MAC_PHY_CLK_REQ_SHIFT) |
			(1 << CLKGTE_POSTRX_MAC_PHY_CLK_REQ_EN_SHIFT) |
			(1 << CLKGTE_DOT11_MAC_PHY_RXVALID_SHIFT) |
			(1 << CLKGTE_NOT_PHY_FIFO_EMPTY_SHIFT) |
			(1 << CLKGTE_DOT11_MAC_PHY_BFE_REPORT_DATA_READY));

		W_REG(osh, D11_CLK_GATE_EXT_REQ0(wlc_hw),
			(CLKREQ_MAC_HT << CLKGTE_IFS_GCI_SYNC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_IFS_CRS_SYNC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_BTCX_SYNC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_ERCX_SYNC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_SLOW_SYNC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_HIN_SYNC_MAC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_TXBF_SYNC_MAC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_TOE_SYNC_MAC_CLK_REQ_SHIFT));

		W_REG(osh, D11_CLK_GATE_EXT_REQ1(wlc_hw),
			(CLKREQ_MAC_HT << CLKGTE_PSM_IPC_SYNC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_PMU_MDIS_SYNC_MAC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_RXE_CHAN_SYNC_CLK_REQ_SHIFT) |
			(CLKREQ_MAC_HT << CLKGTE_PHY_FIFO_SYNC_CLK_REQ_SHIFT));

		/* this register should be written in the last */
		val = 0;
		if (D11REV_IS(wlc_hw->corerev, 61)) {
			val |= (CLKREQ_MAC_ILP << CLKGTE_FORCE_MAC_CLK_REQ_SHIFT);
		}

		W_REG(osh, D11_CLK_GATE_STS(wlc_hw), val);
	}
	else {
		ASSERT(0);
	}

	wlc_bmac_enable_mac(wlc_hw);
}

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
/* This function must not be called other than PCIe bus t/p test. */
void
wlc_bmac_tx_fifos_flush(wlc_hw_info_t *wlc_hw, void *fifo_bitmap)
{
	wlc_bmac_flush_tx_fifos(wlc_hw, fifo_bitmap);
}
#endif /* BCMPCIEDEV && BUS_TPUT */

#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)

/* Convert HWA SWPKT from 3a to 3b
 * Argument fifo is logical fifo index
*/
static uint16 BCMFASTPATH
wlc_bmac_hwa_convert_to_3bswpkt(wlc_info_t *wlc, uint fifo, void *p)
{
	uint8 *txd;
	d11txhdr_t *txh;
	struct dot11_header *d11_hdr;
	hwa_txpost_pkt_t *txpost_pkt;
	hwa_txfifo_pkt_t *prev_txfifo_pkt = NULL, *txfifo_pkt;
	void *head = p;
	osl_t *osh = wlc->osh;
	uint16 pkt_count = 0;
	uint8 ac = 0, num_desc = 0;
	uint32 txdma_flags = 0;

	WL_TRACE(("==> wlc%d: %s: convert 3a -> 3b swpkt on fifo %d\n",
		wlc->pub->unit, __FUNCTION__, fifo));

	/* We only need to care about txd updated for re-transmission */
	if (PKTISHWA3BPKT(osh, p)) {
		for (; p; p = PKTNEXT(osh, p)) {
			/* get pkt_count */
			pkt_count++;
			txpost_pkt = LBHWATXPKT(p);
			txfifo_pkt = (hwa_txfifo_pkt_t *)txpost_pkt;
			prev_txfifo_pkt = txfifo_pkt;
			if (p == head) {
				/* get num_desc */
				num_desc = txfifo_pkt->num_desc;
			}
		}
		goto upd_aqm;
	}

	/* AMSDU use PKTNEXT to link MSDUs */
	for (; p; p = PKTNEXT(osh, p)) {
		pkt_count++;
		txpost_pkt = LBHWATXPKT(p);
		txfifo_pkt = (hwa_txfifo_pkt_t *)txpost_pkt;

		txfifo_pkt->hdr_buf = PKTDATA(osh, p);
		txfifo_pkt->hdr_buf_dlen = PKTLEN(osh, p);
		num_desc += (txfifo_pkt->hdr_buf_dlen) ? 1 : 0;

		/* Be careful of memory variation.
		 * These p can be FRAG or not FRAG (mgmt/clone).
		 */
#ifdef BCMLFRAG
		if (BCMLFRAG_ENAB() && PKTISTXFRAG(osh, p)) {
			txfifo_pkt->data_buf_hlen = PKTFRAGTOTLEN(osh, p);
			if (txfifo_pkt->data_buf_hlen) {
				num_desc++;
				PHYSADDR64LOSET(txfifo_pkt->data_buf_haddr,
					(uint32)PKTFRAGDATA_LO(osh, p, 1));
				PHYSADDR64HISET(txfifo_pkt->data_buf_haddr,
					(uint32)PKTFRAGDATA_HI(osh, p, 1));
			}
		} else
#endif // endif
		{
			txfifo_pkt->data_buf_hlen = 0;
		}

		/* Reset num_desc, total len and txdma_flags, we will fix it up later at first pkt.
		 * Only first pkt need to carry these info.
		 */
		txfifo_pkt->num_desc = 0;
		txfifo_pkt->amsdu_total_len = 0;
		txfifo_pkt->txdma_flags = 0;

		//Set up HWA SWPKT link list, it can be different with 3a output.
		if (prev_txfifo_pkt) {
			HWA_PKT_LINK_NEXT(prev_txfifo_pkt, (hwa_txfifo_pkt_t *)LBHWATXPKT(p));
		}
		prev_txfifo_pkt = txfifo_pkt;

		//Mark this packet as 3b SWPKT
		PKTSETHWA3BPKT(osh, p);

#ifdef BCM_BUZZZ
		if ((WLPKTTAG(p)->flags) & WLF_AMSDU) {
			BUZZZ_KPI_PKT1(KPI_PKT_MAC_TXMSDU, 2, PKTFRAGPKTID(osh, p), fifo);
		} else {
			BUZZZ_KPI_PKT1(KPI_PKT_MAC_TXMPDU, 2, PKTFRAGPKTID(osh, p), fifo);
		}
#endif /* BCM_BUZZZ */
	}

upd_aqm:
	/* Terminate 3b-SWPTK next in latest PKT */
	ASSERT(prev_txfifo_pkt);
	HWA_PKT_TERM_NEXT(prev_txfifo_pkt);

	/* Start preparting AQM descriptor fields */

	/* get the txd header */
	wlc_txprep_pkt_get_hdrs(wlc, head, &txd, &txh, &d11_hdr);

	/* construct the rest of first 3b-SWPKT */
	txfifo_pkt = (hwa_txfifo_pkt_t *)LBHWATXPKT(head);

	/* ctrl1 */
	/* SOFPTR : HWA generate */

	/* epoch */
	if (txd[2] & TOE_F2_EPOCH_MASK)
		txdma_flags |= BCM_SBF(((txd[2] & TOE_F2_EPOCH_MASK) >> TOE_F2_EPOCH_SHIFT),
			HWA_TXFIFO_TXDMA_FLAGS_EPOCH);

	/* frame type */
	if (txd[1] & TOE_F1_FT_MASK)
		txdma_flags |= BCM_SBF(((txd[1] & TOE_F1_FT_MASK) >> TOE_F1_FT_SHIFT),
			HWA_TXFIFO_TXDMA_FLAGS_FRAMETYPE);

	/* fragment allow */
	if (txd[1] & TOE_F1_FRAG_ALLOW)
		txdma_flags |= BCM_SBF(1, HWA_TXFIFO_TXDMA_FLAGS_FRAG);

	/* AC */
#ifdef WME
	ac =  prio2fifo[PKTPRIO(head)];
#else
	ac =  (uint8)PKTPRIO(head);
#endif // endif
	txdma_flags |= BCM_SBF(ac, HWA_TXFIFO_TXDMA_FLAGS_AC);

	/* ctrl2 */
	if (!(((d11actxh_t *)txh)->PktInfo.MacTxControlLow & D11AC_TXC_HDR_FMT_SHORT)) {
		txdma_flags |= BCM_SBF(1, HWA_TXFIFO_TXDMA_FLAGS_TXDTYPE);
	}

	// Coherent, NotPCIe
	txdma_flags |= (0U
		| BCM_SBF(1, HWA_TXFIFO_TXDMA_FLAGS_COHERENT)
		/* txdma_flag is specific for head descriptor,
		 * in dongle mode we should set notpcie,
		 * XXX NIC mode ???
		 */
		| BCM_SBF(1, HWA_TXFIFO_TXDMA_FLAGS_NOTPCIE)
		| 0U);

	//Assign to first pkt
	txfifo_pkt->num_desc = num_desc;
	txfifo_pkt->amsdu_total_len = pkttotlen(osh, head) - (uint)((uint8*)d11_hdr - txd);
	txfifo_pkt->txdma_flags = txdma_flags;

	return pkt_count;
}

/* Argument fifo is logical fifo index */
int BCMFASTPATH
wlc_bmac_hwa_txfifo_commit(wlc_info_t *wlc, uint fifo, const char *from)
{
	bool pktchain_ring_full;
	wlc_hwa_txpkt_t *hwa_txpkt = wlc->hwa_txpkt;

	WL_TRACE(("==> wlc%d: %s: fifo %d from \"%s\"\n",
		wlc->pub->unit, __FUNCTION__, fifo, from));

	if (hwa_txpkt->head == NULL) {
		/* need to revisit it, why we go here for each ping packet ? */
		WL_INFORM(("wlc%d %s: local pktc is empty fifo=%d from \"%s\"\n",
			wlc->pub->unit, __FUNCTION__, fifo, from));
		return BCME_OK;
	}

	HWA_ASSERT(hwa_txpkt->fifo == fifo);

	/* hwa_txfifo_pktchain_xmit_request must be successful */
	pktchain_ring_full = hwa_txfifo_pktchain_xmit_request(hwa_dev, 0,
		WLC_HW_MAP_TXFIFO(wlc, hwa_txpkt->fifo), hwa_txpkt->head,
		hwa_txpkt->tail, hwa_txpkt->pkt_count, hwa_txpkt->mpdu_count);

	/* Clear local pktc */
	hwa_txpkt->head = NULL;

	return (pktchain_ring_full ?  BCME_BUSY : BCME_OK);
}

void
wlc_bmac_hwa_txfifo_ring_full(wlc_info_t *wlc, bool isfull)
{
	wlc->hwa_txpkt->isfull = isfull;
}

#endif /* BCMHWA && HWA_TXFIFO_BUILD */

/**
 * This function encapsulates the call to dma_txfast.
 * In case cut through DMA is enabled, this function will also format necessary information
 * and call the required hnddma api to post the AQM dma descriptor.
 */
int BCMFASTPATH
wlc_bmac_dma_txfast(wlc_info_t *wlc, uint fifo, void *p, bool commit)
{
	hnddma_t *tx_di = NULL;
	int err = 0;
	uint8 *txh;
#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
	uint32 pkt_count;
	wlc_hwa_txpkt_t *hwa_txpkt = wlc->hwa_txpkt;
#endif /* BCMHWA && HWA_TXFIFO_BUILD */

	WL_TRACE(("wlc%d: %s fifo=%d commit=%d \n", wlc->pub->unit, __FUNCTION__, fifo, commit));

	BCM_REFERENCE(txh);

#ifdef BCM_DMA_CT

#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
	/* FIXME: We force to construct a PKTC on a 3a-SWPKT chain in hwa_txpost_3aswpkt2pktc()
	 * and then in this function we handle each "p" individually, we need to terminate
	 * the 3b-SWPKT next at the latest PKT.
	 * FIXME: review and enhance it. How do we handle the PKTLINK, PKTNEXT and HWA-SWPKT next?
	 */
	ASSERT(BCM_DMA_CT_ENAB(wlc));

	/* All TX packet must be HWA packet */
	ASSERT(PKTISHWAPKT(wlc->osh, p));

	/* Before proceeding ahead first check if HWA TXFIFO has room for it. */
	if (hwa_txpkt->isfull) {
		WL_TRACE(("wlc%d %s: NO TXFIFO pktchain room available, fifo=%d \n",
			wlc->pub->unit, __FUNCTION__, fifo));
		if (hwa_txfifo_pktchain_ring_isfull(hwa_dev))
			return BCME_BUSY;
		wlc_bmac_hwa_txfifo_ring_full(wlc, FALSE);
	}

	if (PKTISTXFRAG(wlc->osh, p)) {
		hwa_txfifo_pkt_t *tail;

		WL_TRACE(("==> wlc%d: %s fifo=%d commit=%d \n", wlc->pub->unit,
			__FUNCTION__, fifo, commit));

		// Chain it locally and summit to hwa_txfifo_pktchain_xmit_request
		// until commit is TRUE.
chain_hwa_txpkt:
		if (hwa_txpkt->head) {
			// Commit current chain
			if (hwa_txpkt->fifo != fifo) {
				WL_ERROR(("wlc%d: Commit current fifo-%d, new fifo-%d coming.\n",
					wlc->pub->unit, hwa_txpkt->fifo, fifo));
				err = wlc_bmac_hwa_txfifo_commit(wlc, hwa_txpkt->fifo,
					__FUNCTION__);
				if (err != BCME_OK) {
					/* Stop queuing current p because HWA3B Q is full. */
					return BCME_BUSY;
				}

				goto chain_hwa_txpkt;
			}

			// Construct HWA 3b SWPKT
			pkt_count = wlc_bmac_hwa_convert_to_3bswpkt(wlc, fifo, p);

			tail = (hwa_txfifo_pkt_t *)hwa_txpkt->tail;
			//SW update the 3b pkt next to make a link list.
			HWA_PKT_LINK_NEXT(tail, (hwa_txfifo_pkt_t *)LBHWATXPKT(p));
			hwa_txpkt->tail = LBHWATXPKT(pktlast(wlc->osh, p));
			hwa_txpkt->pkt_count += pkt_count;
			hwa_txpkt->mpdu_count++;
		} else {
			// Construct HWA 3b SWPKT
			pkt_count = wlc_bmac_hwa_convert_to_3bswpkt(wlc, fifo, p);

			hwa_txpkt->head = LBHWATXPKT(p);
			hwa_txpkt->tail = LBHWATXPKT(pktlast(wlc->osh, p));
			hwa_txpkt->pkt_count = pkt_count;
			hwa_txpkt->mpdu_count = 1;
			hwa_txpkt->fifo = fifo;
		}

		if (commit) {
			// txfifo commit must be successful but the pktchain_ring maybe
			// full for next commit. Next commit need to check it.
			(void)wlc_bmac_hwa_txfifo_commit(wlc, fifo, __FUNCTION__);
		}
	} else {
		WL_TRACE(("--> wlc%d: %s fifo=%d commit=%d \n", wlc->pub->unit,
			__FUNCTION__, fifo, commit));

		WL_TRACE(("One MGMTPKT @ %p \n", p));

		// Commit current chain if any
		if (hwa_txpkt->head) {
			err = wlc_bmac_hwa_txfifo_commit(wlc, hwa_txpkt->fifo, __FUNCTION__);
			if (err != BCME_OK) {
				/* Stop queuing current p because HWA3B Q is full. */
				return BCME_BUSY;
			}
		}

		// HWA 3a SWPKT header should be set at lb_alloc already.
		ASSERT(LBHWATXPKT(p));
		if (!PKTISMGMTTXPKT(wlc->osh, p)) {
			WL_ERROR(("wlc%d: ERROR @ %s(%d): Why flags 0x%x FIXME!!!\n",
				wlc->pub->unit, __FUNCTION__, __LINE__, LBP(p)->flags));
			ASSERT(0);
		}

		pkt_count = wlc_bmac_hwa_convert_to_3bswpkt(wlc, fifo, p);
		hwa_txpkt->head = LBHWATXPKT(p);
		hwa_txpkt->tail = LBHWATXPKT(pktlast(wlc->osh, p));
		hwa_txpkt->pkt_count = pkt_count;
		hwa_txpkt->mpdu_count = 1;
		hwa_txpkt->fifo = fifo;
		// txfifo commit must be successful but the pktchain_ring maybe
		// full for next commit. Caller need to check it for next commit.
		(void)wlc_bmac_hwa_txfifo_commit(wlc, fifo, __FUNCTION__);
	}

	return BCME_OK;
#endif /* BCMHWA && HWA_TXFIFO_BUILD */

	if (BCM_DMA_CT_ENAB(wlc)) {
		wlc_hw_info_t *wlc_hw = wlc->hw;
		uint16 pre_txout = 0;
		uint numd = 0;
		hnddma_t *aqm_di = NULL;
		uint32 ctrl1 = 0;
		uint32 ctrl2 = 0;
		uint8 ac = 0;
		uint8 *txd;

		struct dot11_header *d11_hdr;
		dma64dd_t dd;
		uintptr data_dd_addr;
		uint mpdulen, tsoHdrSize = 0;

		ASSERT(D11REV_GE(wlc->pub->corerev, 64));

		tx_di = WLC_HW_DI(wlc, fifo);

		aqm_di = WLC_HW_AQM_DI(wlc, fifo);

		ASSERT(aqm_di);

		/* Before proceeding ahead first check if an AQM descriptor is available */
		if (!(*wlc_hw->txavail_aqm[fifo])) {
			WL_INFORM(("wlc%d %s: NO AQM descriptors available, fifo=%d \n",
				wlc->pub->unit, __FUNCTION__, fifo));
			return BCME_BUSY;
		}

		pre_txout = dma_get_next_txd_idx(tx_di, TRUE);

		/* post the data descriptor */
		err = dma_txfast(tx_di, p, commit);
		if (err < 0) {
			WL_ERROR(("wlc%d %s: dma_txfast failed, fifo=%d \n", wlc->pub->unit,
				__FUNCTION__, fifo));
			return BCME_BUSY;
		}

		/* tx dma call was successfull. Post the AQM descriptor */
		numd = dma_get_txd_count(tx_di, pre_txout, TRUE);

		/* get the txd header */
		wlc_txprep_pkt_get_hdrs(wlc, p, &txd, (d11txhdr_t **)&txh, &d11_hdr);

		/* Start preparting AQM descriptor fields */

		/* ctrl1 */
		/* SOFPTR */
		data_dd_addr = dma_get_txd_addr(tx_di, pre_txout);
		ctrl1 |= (D64_AQM_CTRL1_SOFPTR & data_dd_addr);

		/* epoch */
		if (D11REV_LT(wlc_hw->corerev, 128) && (txd[2] & TOE_F2_EPOCH)) {
			ctrl1 |= D64_AQM_CTRL1_EPOCH;
		}

		/* numd */
		ctrl1 |= ((numd << D64_AQM_CTRL1_NUMD_SHIFT) & D64_AQM_CTRL1_NUMD_MASK);

		/* AC */
#ifdef WME
		ac =  prio2fifo[PKTPRIO(p)];
#else
		ac =  (uint8)PKTPRIO(p);
#endif // endif
		ctrl1 |= ((ac << D64_AQM_CTRL1_AC_SHIFT) & D64_AQM_CTRL1_AC_MASK);
		ctrl1 |= (D64_CTRL1_EOF | D64_CTRL1_SOF | D64_CTRL1_IOC);

		/* ctrl2 */
		tsoHdrSize = (uint)(txh - txd);
		mpdulen = pkttotlen(wlc_hw->osh, p) - (uint)((uint8*)d11_hdr - txd);
		ctrl2 |= (mpdulen & D64_AQM_CTRL2_MPDULEN_MASK);
		if (!(*D11_TXH_GET_MACLOW_PTR(wlc, (d11txhdr_t *)txh) &
			D11AC_TXC_HDR_FMT_SHORT)) {
			ctrl2 |= D64_AQM_CTRL2_TXDTYPE;
		}
		if (D11REV_GE(wlc_hw->corerev, 128)) {
			/* epoch */
			if (txd[2] & TOE_F2_EPOCH)
				ctrl2 |= D64_AQM_CTRL2_EPOCH;
			if (txd[2] & TOE_F2_EPOCH_EXT)
				ctrl2 |= D64_AQM_CTRL2_EPOCH_EXT;
			/* Frame Type */
			if (txd[1] & TOE_F1_FRAMETYPE_1)
				ctrl2 |= D64_AQM_CTRL2_FRAMETYPE_1;
			if (txd[1] & TOE_F1_FRAMETYPE_2)
				ctrl2 |= D64_AQM_CTRL2_FRAMETYPE_2;
			/* Fragment allow */
			if (txd[1] & TOE_F1_FRAG_ALLOW)
				ctrl2 |= D64_AQM_CTRL2_FRAG_ALLOW;
		}
		dd.ctrl1 = ctrl1;
		dd.ctrl2 = ctrl2;

		/* get the mem address of the data buffer pointed by the SOF Tx dd */
		dma_get_txd_memaddr(tx_di, DISCARD_QUAL(&dd.addrlow, uint32),
			DISCARD_QUAL(&dd.addrhigh, uint32), (uint)pre_txout);
		dd.addrlow += tsoHdrSize;

		WL_TRACE(("wlc%d %s: pre dma_txdesc dd.ctrl1 = 0x%x, dd.ctrl2 = 0x%x, "
			"dd.addrlow = 0x%x, dd.addrhigh = 0x%x \n",
			wlc->pub->unit, __FUNCTION__, dd.ctrl1, dd.ctrl2,
			dd.addrlow, dd.addrhigh));
		WL_TRACE(("wlc%d %s: pre-dma_txdesc numd = %d, pre_txout = %d  \n", wlc->pub->unit,
			__FUNCTION__, numd, pre_txout));
#ifdef RX_DEBUG_ASSERTS
		/* Make sure Fragments not allowed for now */
		ASSERT(!(dd.ctrl2 & D64_AQM_CTRL2_FRAG_ALLOW));
#endif // endif

		/* post the AQM descriptor */
		err = dma_txdesc(aqm_di, &dd, commit);
		if (err < 0) {
			WL_ERROR(("wlc%d %s: dma_txdesc failed fifo=%d \n", wlc->pub->unit,
				__FUNCTION__, fifo));
			return BCME_BUSY;
		}
	} else
#endif /* BCM_DMA_CT */
	{
		ASSERT(D11REV_LT(wlc->hw->corerev, 128));

		ASSERT(fifo < WLC_HW_NFIFO_INUSE(wlc));
		tx_di = WLC_HW_DI(wlc, fifo);

		err = dma_txfast(tx_di, p, commit);
		if (err < 0) {
			WL_ERROR(("wlc%d %s: dma_txfast failed, fifo=%d \n", wlc->pub->unit,
				__FUNCTION__, fifo));
			return BCME_BUSY;
		}
	}
	return err;
}

/* Suspends processing of Classify FIFO (RX-FIFO2) */
void wlc_bmac_classify_fifo_suspend(wlc_hw_info_t *wlc_hw)
{
	wlc_hw->classify_fifo_suspend_cnt++;
}

/* Resumes processing of Classify FIFO (RX-FIFO2). Function also services any pending
 * RX_INTR_FIFO_2 when there are no more suspend requests
*/
void
wlc_bmac_classify_fifo_resume(wlc_hw_info_t *wlc_hw, bool rollback)
{
	if (!wlc_hw->classify_fifo_suspend_cnt) {
		return;
	}
	wlc_hw->classify_fifo_suspend_cnt--;

	/* No further Rx fifo-2 process if rollback is set */
	if (rollback)
		return;

	/* Process RX fifo-2 if Suspend Count is zero and there are frames to be processed */
	if (!wlc_hw->classify_fifo_suspend_cnt && PKT_CLASSIFY_EN(RX_FIFO2) &&
		(wlc_hw->dma_rx_intstatus & RX_INTR_FIFO_2)) {
		wlc_dpc_info_t dpc;
		wlc_bmac_recv(wlc_hw, RX_FIFO2, FALSE, &dpc);
	}
}

bool
wlc_bmac_classify_fifo_suspended(wlc_hw_info_t *wlc_hw)
{
	return (wlc_hw->classify_fifo_suspend_cnt != 0);
}

#ifdef BULK_PKTLIST
#ifdef BCM_DMA_CT
static uint16 BCMFASTPATH
wlc_bmac_dma_bulk_aqm_txcomplete(wlc_info_t *wlc, uint fifo,
		txd_range_t range, uint16 ncons, bool nonAMPDU)
{
	uint32 npkt;

	for (npkt = 0; npkt < ncons; npkt++)
	{
		int rc = dma_getnexttxdd(WLC_HW_AQM_DI(wlc, fifo), range, NULL);

		if (rc == BCME_OK) {
			continue;
		}

		/* NonAMPDU completion path processes one packet a time (ncons=1),
		 * This path repeatedly calls into the DMA library until there are
		 * no descriptors left.
		 * This code checks both the AQM and packet rings.
		 */

		if ((rc == BCME_NOTFOUND) && (ncons == 1) &&
		 (nonAMPDU) &&
		 (dma_txactive(WLC_HW_DI(wlc, fifo)) == 0))
		{
			break;
		}

		/* This routine does not implement the error handling policy
		 * Error condition exists when ncons != npkt
		 *
		 * It is left up to the callers of this routine to decide on
		 * the errror handling
		 */
		WL_INFORM(("wlc%d: %s: AQM-fifo %d reclaim err=%u, AMPDU=%u nc=%u np=%u\n",
				wlc->pub->unit, __FUNCTION__, fifo, rc, nonAMPDU, ncons, npkt));
#if defined(BCMDBG)
		{
			int npkts;
			uint i;
			for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
				hnddma_t *di;
				di = WLC_HW_DI(wlc, i);
				npkts = dma_txactive(di);
				if (npkts == 0) continue;
				WL_ERROR(("FIFO-%d TXPEND = %d TX-DMA%d =>\n", i, npkts, i));
				dma_dumptx(di, NULL, FALSE);
				WL_ERROR(("CT-DMA%d =>\n", i));
				di = WLC_HW_AQM_DI(wlc, i);
				dma_dumptx(di, NULL, FALSE);
			}
		}
#endif // endif
		break;
	} /* for (npkt=0 ...) */
	return npkt;
}
#define DMA_CT_RANGE(r) HNDDMA_RANGE_ALL
#else
#define DMA_CT_RANGE(r)	r
#endif /* BCM_DMA_CT */

uint16 BCMFASTPATH
wlc_bmac_dma_bulk_txcomplete(wlc_info_t *wlc, uint fifo, uint16 ncons,
		void **list_head, void **list_tail, txd_range_t range, bool nonAMPDU)
{
/* Routine performs the BMAC bulk txcompletion processing.
 * Returns the number of packets processed.
 * This number may be different from ncons, however the number of AQM descriptors
 * reclaimed must match ncons otherwise the return value is zero.
 * It is left up to the caller to decide on the next course of action.
 *
 * Note : The extended error codes from the DMA library are discarded and not passed
 * up to the callers.
 */
	uint16 nproc = 0;

	/* Assert for non-zero ncons, avoid additional check where ncons == nproc == 0 */
	ASSERT(ncons);

	/* This routine minimizes the logical comparisons. nproc will not be updated
	 * if the AQM or data desc cannot be reclaimed, this is normally an error,
	 * it is left up to the caller to take corrective action or ASSERT()
	 */
	if (BCM_DMA_CT_ENAB(wlc)) {
		/* Reclaim AQM descriptor and corresponding data descriptor */
		if (wlc_bmac_dma_bulk_aqm_txcomplete(wlc, fifo,
			DMA_CT_RANGE(range), ncons, nonAMPDU) == ncons) {
			 /* In the case of data desc error nproc will not be updated. */
			dma_bulk_txcomplete(WLC_HW_DI(wlc, fifo), ncons, &nproc,
				list_head, list_tail, DMA_CT_RANGE(range));
		}
		/* This value will be zero if either the AQM desc or
		 * the data descriptor cannot be reclaimed
		 */
		return nproc;
	}

	/* In the case of error nproc will not be updated */
	dma_bulk_txcomplete(WLC_HW_DI(wlc, fifo), ncons, &nproc,
		list_head, list_tail, DMA_CT_RANGE(range));
	return nproc;
}
/**
 * This function encapsulates the call to dma_getnexttxp.
 * In case cut through DMA is enabled, this function will also issue the call to get
 * the next AQM descriptor.
 *
 * wlc_bmac_dma_getnexttxp() returns a single descriptor
 */

void * BCMFASTPATH
wlc_bmac_dma_getnexttxp(wlc_info_t *wlc, uint fifo, txd_range_t range)
{
	void *txp = NULL;

#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
	/* get next data packet and reclaim TXDMA descriptor(s) */
	txp = hwa_txfifo_getnexttxp32(hwa_dev, WLC_HW_MAP_TXFIFO(wlc, fifo), (uint32)range);
	if (txp == NULL) {
		if (range != HNDDMA_RANGE_ALL) {
			WL_ERROR(("wlc%d: %s Could not reclaim data dd(hwa) fifo=%d\n",
				wlc->pub->unit, __FUNCTION__, fifo));
		}
		return NULL;
	}
	return txp;
#endif /* BCMHWA && HWA_TXFIFO_BUILD */

	if (wlc_bmac_dma_bulk_txcomplete(wlc, fifo, 1, &txp, NULL, range, TRUE)) {
		return txp;
	} else {
		/* If the AQM ring is empty, the function returns a zero pkt count
		 * Check to see if the corresponding data fifo ring is also empty.
		 * This situation can arise if we are detaching a queue
		 * at the end of scan.
		 */
		ASSERT(dma_txcommitted(WLC_HW_DI(wlc, fifo)) == 0);
		return NULL;
	}
}
#else
/**
 * This function encapsulates the call to dma_getnexttxp.
 * In case cut through DMA is enabled, this function will also issue the call to get
 * the next AQM descriptor.
 */
void * BCMFASTPATH
wlc_bmac_dma_getnexttxp(wlc_info_t *wlc, uint fifo, txd_range_t range)
{
	void *txp = NULL;

	WL_TRACE(("wlc%d: %s fifo %d \n", wlc->pub->unit, __FUNCTION__, fifo));

#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
	/* get next data packet and reclaim TXDMA descriptor(s) */
	txp = hwa_txfifo_getnexttxp32(hwa_dev, WLC_HW_MAP_TXFIFO(wlc, fifo), (uint32)range);
	if (txp == NULL) {
		if (range != HNDDMA_RANGE_ALL) {
			WL_ERROR(("wlc%d: %s Could not reclaim data dd(hwa) fifo=%d\n",
				wlc->pub->unit, __FUNCTION__, fifo));
		}
		return NULL;
	}
	return txp;
#endif /* BCMHWA && HWA_TXFIFO_BUILD */

#ifdef BCM_DMA_CT
	if (BCM_DMA_CT_ENAB(wlc)) {
		uint32 txdma_flags = 0;

		/* reclaim AQM descriptor */
		if (dma_getnexttxdd(WLC_HW_AQM_DI(wlc, fifo), range, &txdma_flags) != BCME_OK) {
			WL_INFORM(("wlc%d: %s: could not reclaim AQM-fifo %d descriptor\n",
				wlc->pub->unit, __FUNCTION__, fifo));
#if defined(BCMDBG)
			{
			int npkts;
			uint i;
			for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
				hnddma_t *di;
				di = WLC_HW_DI(wlc, i);
				if (di == NULL) continue;
				npkts = dma_txactive(di);
				if (npkts == 0) continue;
				WL_ERROR(("FIFO-%d TXPEND = %d TX-DMA%d =>\n", i, npkts, i));
				dma_dumptx(di, NULL, FALSE);
				WL_ERROR(("CT-DMA%d =>\n", i));
				di = WLC_HW_AQM_DI(wlc, i);
				dma_dumptx(di, NULL, FALSE);
			}
			}
#endif // endif
			/* If the AQM ring is empty check to see if the
			 * corresponding data fifo ring is also empty.
			 * This situation can arise if we are detaching a queue
			 * in the case of a scan end.
			 *
			 * dma_txcommitted() counts all packets in the ring including
			 * completed packets not yet processed
			 */
			ASSERT(dma_txcommitted(WLC_HW_DI(wlc, fifo)) == 0);
			return NULL;
		}

		/* get next data packet and reclaim TXDMA descriptor(s) */
		txp = dma_getnexttxp(WLC_HW_DI(wlc, fifo), HNDDMA_RANGE_ALL);
		if (txp == NULL) {
			WL_ERROR(("wlc%d: %s Could not reclaim data dd fifo=%d\n",
				wlc->pub->unit, __FUNCTION__, fifo));
			return NULL;
		}

		return txp;
	}
#endif /* #ifdef BCM_DMA_CT */

	/* get next data packet and reclaim TXDMA descriptor(s) */
	txp = dma_getnexttxp(WLC_HW_DI(wlc, fifo), range);
	if (txp == NULL) {
		WL_INFORM(("wlc%d: %s Could not reclaim data dd fifo=%d\n",
			wlc->pub->unit, __FUNCTION__, fifo));
		return NULL;
	}
	return txp;
}
#endif /* BULK_PKTLIST */

#ifdef GPIO_TXINHIBIT
void
wlc_bmac_gpio_set_tx_inhibit_tout(wlc_hw_info_t *wlc_hw)
{
	if (si_iscoreup(wlc_hw->sih)) {
		wlc_bmac_write_shm(wlc_hw, M_GPIO_TX_INHIBIT_TOUT, wlc_hw->tx_inhibit_tout);
	}
}

void
wlc_bmac_notify_gpio_tx_inhibit(wlc_info_t *wlc)
{
	/* VAL and INT are current and previous state of gpio13.
	* Either one of the bits being set indicates an edge transition on the gpio.
	 */

	int txinhibit_int = wlc_bmac_read_shm(wlc->hw, M_MACINTSTATUS_EXT) &
		(C_MISE_GPIO_TXINHIBIT_VAL_MASK | C_MISE_GPIO_TXINHIBIT_INT_MASK);

	wlc_mac_event(wlc, WLC_E_SPW_TXINHIBIT, NULL, 0,
		(txinhibit_int & C_MISE_GPIO_TXINHIBIT_VAL_MASK), 0, 0, 0);

}
#endif /* GPIO_TXINHIBIT */

static void
wlc_bmac_enable_ct_access(wlc_hw_info_t *wlc_hw, bool enabled)
{
#ifdef BCM_DMA_CT
	if (D11REV_GE(wlc_hw->corerev, 65) && wlc_hw->clk) {
		W_REG(wlc_hw->osh, D11_CTMode(wlc_hw), enabled ? 0x1 : 0x0);
	}
#endif // endif
}

#ifdef BCM_DMA_CT

void
wlc_bmac_ctdma_update(wlc_hw_info_t *wlc_hw, bool enabled)
{
	bool user_config;

	if (wlc_hw->wlc->_dma_ct_flags & DMA_CT_IOCTL_OVERRIDE) {
		/* User already changed CTDMA on/off previously, so we need to
		 * check against previous changed value.
		 */
		user_config = (wlc_hw->wlc->_dma_ct_flags & DMA_CT_IOCTL_CONFIG) ? TRUE : FALSE;
		if (user_config != enabled)
			wlc_hw->wlc->_dma_ct_flags &= ~DMA_CT_IOCTL_OVERRIDE;
	} else if (wlc_hw->wlc->_dma_ct != enabled) {
		/* Otherwise check against current CTDMA state. If different, then it implies
		 * a state change.
		 */
		wlc_hw->wlc->_dma_ct_flags |= DMA_CT_IOCTL_OVERRIDE;
		wlc_hw->wlc->_dma_ct_flags &= ~DMA_CT_IOCTL_CONFIG;
		if (enabled)
			wlc_hw->wlc->_dma_ct_flags |= DMA_CT_IOCTL_CONFIG;
	}
}
#endif /* BCM_DMA_CT */

void
wlc_bmac_handle_device_halt(wlc_hw_info_t *wlc_hw, bool suspend_TX_DMA,
	bool suspend_CORE, bool disable_RX_DMA)
{
	uint fifo;
	if (suspend_TX_DMA == TRUE) {
		for (fifo = 0; fifo < WLC_HW_NFIFO_TOTAL(wlc_hw->wlc); fifo++) {
			if (wlc_hw->di[fifo]) {
				dma_txsuspend(wlc_hw->di[fifo]);
			}
		}
	}
	if (suspend_CORE == TRUE) {
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
	}
	if (disable_RX_DMA == TRUE) {
		dma_rxreset(wlc_hw->di[RX_FIFO]);
		if (BCMSPLITRX_ENAB()) {
			dma_rxreset(wlc_hw->di[RX_FIFO1]);
		}
	}
}

bool
wlc_bmac_check_d11war(wlc_hw_info_t *wlc_hw, uint32 chk_flags)
{
	return (wlc_hw->d11war_flags & chk_flags) ? TRUE : FALSE;
}

uint32
wlc_bmac_get_d11war_flags(wlc_hw_info_t *wlc_hw)
{
	return wlc_hw->d11war_flags;
}

#ifdef BCMDBG_SR
static int sr_timer_counter = 0;
static void
wlc_sr_timer(void *arg)
{
	sr_timer_counter ++;
}
#endif // endif

#ifdef WL_UTRACE
void
wlc_dump_utrace_logs(wlc_info_t *wlc, uchar *utrace_region_ptr)
{
	uint32 *crash_utrace_data;
	uint32 utrace_size = T_UTRACE_TPL_RAM_SIZE_BYTES;
	uint32 size_words, i, utrace_start = T_UTRACE_BLK_STRT;
	size_words = utrace_size/sizeof(*crash_utrace_data);
	*(uint16 *)utrace_region_ptr = (wlc->hw->unit + 1);
	*(uint16 *)(utrace_region_ptr + 2) = (uint16) size_words;
	wlc_bmac_templateptr_wreg(wlc->hw, utrace_start);
	crash_utrace_data = (uint32 *)(utrace_region_ptr + 4);
	for (i = 0; i < size_words; i++) {
		crash_utrace_data[i] = (uint32) wlc_bmac_templatedata_rreg(wlc->hw);
	}
}
#endif /* WL_UTRACE */

#ifdef AWD_EXT_TRAP
static uint8 *
wlc_bmac_trap_phy(void *arg, trap_t *tr, uint8 *dst, int *dst_maxlen)
{
	uint8 *new_dst = dst;
	phy_dbg_ext_trap_err_t pt = { 0 };
	phy_info_t *pi = (phy_info_t *)arg;

	phy_dbg_dump_ext_trap(pi, &pt);

	if (pt.err && (pt.err != PHY_RC_NOMEM))
	{
		new_dst = bcm_write_tlv_safe(TAG_TRAP_PHY, &pt, sizeof(pt), dst,
				*dst_maxlen);
		if (new_dst != dst)
			*dst_maxlen -= (sizeof(pt) + BCM_TLV_HDR_SIZE);
	} else {
		awd_set_trap_ext_swflag(AWD_EXT_TRAP_SW_FLAG_MEM);
	}
	return new_dst;
}

static uint8 *
wlc_bmac_trap_mac(void *arg, trap_t *tr, uint8 *dst, int *dst_maxlen)
{
	uint8 *new_dst = dst;
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)arg;
	int trap_tag;
	uchar *ext_trap_data;
	uint32 ext_trap_data_len = 0;

	if (wlc_hw->need_reinit == WL_REINIT_RC_PSM_WD)
		trap_tag = TAG_TRAP_PSM_WD;
	else if (wlc_hw->need_reinit == WL_REINIT_RC_MAC_SUSPEND)
		trap_tag = TAG_TRAP_MAC_SUSP;
	else if ((wlc_hw->need_reinit == WL_REINIT_RC_MAC_WAKE) ||
		(wlc_hw->need_reinit == WL_REINIT_RC_ENABLE_MAC)) {
		trap_tag = TAG_TRAP_MAC_WAKE;
	}
	else
		return new_dst;

	ext_trap_data = wlc_macdbg_get_ext_trap_data(wlc_hw->wlc->macdbg, &ext_trap_data_len);
	if ((ext_trap_data == NULL) || (ext_trap_data_len == 0)) {
		return new_dst;
	}

	new_dst = bcm_write_tlv_safe(trap_tag, ext_trap_data, ext_trap_data_len, dst,
		*dst_maxlen);
	if (new_dst != dst)
		*dst_maxlen -= (ext_trap_data_len + BCM_TLV_HDR_SIZE);

	return new_dst;
}

#endif /* AWD_EXT_TRAP */

#if defined(BCMDBG) && defined(BCMSPLITRX)
/* Dump the Host content given the host physical address */
void
wlc_bmac_dump_host_buffer(wlc_info_t * wlc, uint64 haddr64, uint32 len)
{
	uint32 host_mem;
	uint32 base_lo;
	uint32 base_hi;
	uint64 base_bkp64;
	uint16 i;
	uint8* data;

	/* Start the access */
	wl_bus_sbtopcie_access_start(wlc->wl, len, haddr64, &host_mem, &base_lo, &base_hi);

	printf("Host Dump @ 0x%x  : ", (uint32)haddr64);
	/* Dump the host memory */
	data = (uint8*)host_mem;
	for (i = 0; i < len; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");

	base_bkp64 = base_hi;
	base_bkp64 = (base_bkp64 << (NBITS(uint32)) | base_lo);
	/* Stop the access */
	wl_bus_sbtopcie_access_stop(wlc->wl, base_bkp64);
}
#endif /* defined(BCMDBG) && defined(BCMSPLITRX) */

#ifdef BCM43684_HDRCONVTD_ETHER_LEN_WAR
void
wlc_bmac_write_host_buffer(wlc_info_t * wlc, uint64 haddr64, uint8 *buf, uint32 len)
{
	uint32 host_mem;
	uint32 base_lo;
	uint32 base_hi;
	uint64 base_bkp64;

	/* Start the access */
	wl_bus_sbtopcie_access_start(wlc->wl, len, haddr64, &host_mem, &base_lo, &base_hi);

	/* Write the host memory */
	memcpy((uint8*)host_mem, buf, len);

	base_bkp64 = base_hi;
	base_bkp64 = (base_bkp64 << (NBITS(uint32)) | base_lo);
	/* Stop the access */
	wl_bus_sbtopcie_access_stop(wlc->wl, base_bkp64);
}
#endif /* BCM43684_HDRCONVTD_ETHER_LEN_WAR */
#ifdef RX_DEBUG_ASSERTS
/*
 * Handle corrupted RX packets due to invalid fifo-0 length
 * Dump required info & TRAP
 */
void
wlc_rx_invalid_length_handle(wlc_info_t* wlc, void* p, d11rxhdr_t *rxh)
{
	uint64 haddr64;
	/* Dump previous RXS info */
	wlc_print_prev_rxs(wlc);

	haddr64 = PKTFRAGDATA_HI(wlc->osh, p, 1);
	haddr64 = (haddr64 << (NBITS(uint32))) | (PKTFRAGDATA_LO(wlc->osh, p, 1));

	/* Dump Host contents from fifo-0 */
	wlc_bmac_dump_host_buffer(wlc, haddr64, 64);

#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(BCM_11AXHDR_DBG)
	wlc_print_ge128_rxhdr(wlc, &rxh->ge128);
#endif /* BCMDBG || WLMSG_PRHDRS || BCM_11AXHDR_DBG */

	ASSERT(0);
}
#endif /* RX_DEBUG_ASSERTS */
