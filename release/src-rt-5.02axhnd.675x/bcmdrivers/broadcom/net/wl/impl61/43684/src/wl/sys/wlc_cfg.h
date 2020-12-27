/*
 * Configuration-related definitions for
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_cfg.h 777286 2019-07-25 19:43:30Z $
 */

#ifndef _wlc_cfg_h_
#define _wlc_cfg_h_

#define USE_NEW_RSPEC_DEFS

/**************************************************
 * Get customized tunables to override the default*
 * ************************************************
 */
#if !defined(WL_UNITTEST)
#include "wlconf.h"
#endif // endif

/***********************************************
 * Feature-related macros to optimize out code *
 * *********************************************
 */

/* ROM_ENAB_RUNTIME_CHECK may be set based upon the #define below (for ROM builds). It may also
 * be defined via makefiles (e.g. ROM auto abandon unoptimized compiles).
 */
#if defined(BCMROMBUILD)
	#ifndef ROM_ENAB_RUNTIME_CHECK
		#define ROM_ENAB_RUNTIME_CHECK
	#endif
#endif /* BCMROMBUILD */

#ifndef PHYCAL_CACHING
#if (defined(SRHWVSDB) && !defined(SRHWVSDB_DISABLED)) || (defined(WLMCHAN) && \
	!defined(WLMCHAN_DISABLED)) || defined(BCMPHYCAL_CACHING)
#define PHYCAL_CACHING
#endif // endif
#endif /* !PHYCAL_CACHING */

#if defined(NLO) && !defined(WLPFN)
#error "NLO needs WLPFN to be defined."
#endif // endif

#if defined(GSCAN) && !defined(WLPFN)
#error "GSCAN needs WLPFN to be defined."
#endif // endif

#if !defined(DONGLEBUILD)
#ifdef NVSRCX
#error "NVRAM and SROM can't be enabled at the same time in non-dongle build."
#endif // endif
#endif /* !DONGLEBUILD */

/* DUALBAND Support */
#ifdef DBAND
#define NBANDS(wlc) ((wlc)->pub->_nbands)
#define NBANDS_PUB(pub) ((pub)->_nbands)
#define NBANDS_HW(hw) ((hw)->_nbands)
#else
#define NBANDS(wlc) 1
#define NBANDS_PUB(wlc) 1
#define NBANDS_HW(hw) 1
#endif /* DBAND */

#define _IS_SINGLEBAND_5G_RELEASED_CHIPS(device) \
	((device == BCM4360_D11AC5G_ID) || \
	 (device == BCM4352_D11AC5G_ID) || \
	 (device == BCM43602_D11AC5G_ID) || \
	 (device == BCM4365_D11AC5G_ID) || \
	 (device == BCM4366_D11AC5G_ID) || \
	 (device == BCM4347_D11AC5G_ID) || \
	 (device == BCM53573_D11AC5G_ID) || \
	 (device == BCM47189_D11AC5G_ID) || \
	 (device == BCM43684_D11AX5G_ID) || \
	 (device == EMBEDDED_2x2AX_DEV5G_ID) || \
	 (device == BCM6710_D11AX5G_ID) || \
	 (device == BCM6878_D11AC5G_ID))

#ifdef UNRELEASEDCHIP
#define _IS_SINGLEBAND_5G_UNRELEASED_CHIPS(device) \
	(0)
#else
	#define _IS_SINGLEBAND_5G_UNRELEASED_CHIPS(device) FALSE
#endif /* UNRELEASEDCHIP */

#define _IS_SINGLEBAND_5G(device)\
	(_IS_SINGLEBAND_5G_RELEASED_CHIPS(device) || _IS_SINGLEBAND_5G_UNRELEASED_CHIPS(device))

int wlc_is_singleband_5g(unsigned int device, unsigned int corecap);
int wlc_bmac_is_singleband_5g(unsigned int device, unsigned int corecap);
#define IS_SINGLEBAND_5G(device, corecap)	(bool)wlc_is_singleband_5g(device, corecap)

#ifndef WLRXEXTHDROOM
#ifdef DONGLEBUILD
#define WLRXEXTHDROOM -1 /* keep -1 default BCMEXTRAHDROOM for full-dongle */
#else
#define WLRXEXTHDROOM 0
#endif /* DONGLEBUILD */
#endif /* WLRXEXTHDROOM */

/* **** Core type/rev defaults **** */
#define D11_DEFAULT	0x40000000	/* Supported  D11 revs: 30 (43217) */
#define D11_DEFAULT2	0x21030400	/* Supported  D11 revs: Rev 42(4360b0),
					 * 48(43570a2), 49 (43602), 56(47189), 61(6878)
					 */
#define D11_DEFAULT3	0x00000002	/* Supported  D11 revs: 65(4365c0) */
#define D11_DEFAULT4	0x0		/* Supported  D11 revs: 0 */
#define D11_DEFAULT5	0x0000000e	/* Supported  D11 revs: 129(43684b0),
					 * 130(63178a0), 131(6710a0)
					 */
#define D11_MINOR_DEFAULT    0x00000023	/* Supported  D11 minor revs: 0-1, 5 */

/*
 * The supported PHYs are either specified explicitly in the wltunable_xxx.h file, or the
 * defaults are used.
 * If PHYCONF_DEFAULTS is set to 1, then all defaults will be used for PHYs that are not predefined.
 * If PHYCONF_DEFAULTS is set to 0 (or non-existent), then all PHYs that are not predefined will
 * be disabled.
 */
#ifndef PHYCONF_DEFAULTS	/* possibly defined in wltunable_xxx.h, to use defaults */
#  if (defined(ACCONF) || defined(ACCONF2) || defined(ACCONF5) || defined(LCN40CONF) || \
	defined(LCNCONF) || defined(LCN20CONF) || defined(NCONF) || defined(HTCONF))
	/* do not use default phy configs since specific phy support is specified */
#    define PHYCONF_DEFAULTS	0
#  else
	/* use default phy configs since nothing was specified */
#    define PHYCONF_DEFAULTS	1
#  endif
#endif /* !PHYCONF_DEFAULTS */

#define NPHY_DEFAULT		0x00020000	/* Supported nphy revs:
						 *	17	43217
						 */
#define LCNPHY_DEFAULT		0x0		/* No supported lcnphy revs */
#define LCN40PHY_DEFAULT	0x0		/* No supported lcn40phy revs */
#define LCN20PHY_DEFAULT	0x0		/* No supported lcn20phy revs */
#define ACPHY_DEFAULT		0x01060002	/* Supported acphy revs:
						 *	1	4360b0
						 *	17	43570a2
						 *	18	43602a3
						 *	24	47189
						 */
#define ACPHY_DEFAULT2		0x00088002	/* Supported acphy revs:
						 *	33	4365c0
						 *      47      43684a0
						 *      51      63178a0
						 */
#define ACPHY_DEFAULT3		0x0		/*	64-95	unused rev */
#define ACPHY_DEFAULT4		0x0		/*	96-127	unused rev */
#define ACPHY_DEFAULT5		0x00000003	/* Supported acphy revs:
						 *	128	6878
						 *	129	6710a0
						 */

#define D11CONF1_BASE		0
#define D11CONF2_BASE		32
#define D11CONF3_BASE		64
#define D11CONF4_BASE		96
#define D11CONF5_BASE		128
#define D11CONF_WIDTH		32
#define D11CONF_CHK(val, base)	(((val) >= (base)) && ((val) < ((base) + D11CONF_WIDTH)))

/* Windows needs D11CONF_REV():
 *   "shift count negative or too big, undefined behavior"
 */
#define D11CONF_REV(val, base)	(D11CONF_CHK(val, base) ? ((val) - (base)) : (0))

/* We need similar macros for ACPHY */
#define ACCONF1_BASE		0
#define ACCONF2_BASE		32
#define ACCONF5_BASE		128
#define ACCONF_WIDTH		32
#define ACCONF_CHK(val, base)	(((val) >= (base)) && ((val) < ((base) + ACCONF_WIDTH)))
#define ACCONF_REV(val, base)	(ACCONF_CHK(val, base) ? ((val) - (base)) : (0))

/* To avoid compile warnings, we cannot compare uint with 0, which is always true */
#define ACCONF_CHK0(val, base)	((val) < ((base) + ACCONF_WIDTH))
#define ACCONF_REV0(val, base)	(ACCONF_CHK0(val, base) ? ((val) - (base)) : (0))

/* For undefined values, use defaults */
#ifndef D11CONF
#define D11CONF	D11_DEFAULT
#endif // endif
#ifndef D11CONF2
#define D11CONF2 D11_DEFAULT2
#endif // endif
#ifndef D11CONF3
#define D11CONF3 D11_DEFAULT3
#endif // endif
#ifndef D11CONF4
#define D11CONF4 D11_DEFAULT4
#endif // endif
#ifndef D11CONF5
#define D11CONF5 D11_DEFAULT5
#endif // endif

#ifndef D11CONF_MINOR
#define D11CONF_MINOR	D11_MINOR_DEFAULT
#endif // endif

#if PHYCONF_DEFAULTS
/* Use default configurations for phy configs that are not specified */
#  ifndef NCONF
#    define NCONF	NPHY_DEFAULT
#  endif /* !NCONF */
#  ifndef LCN20CONF
#    define LCN20CONF	LCN20PHY_DEFAULT
#  endif /* !LCN20CONF */
#  ifndef ACCONF
#    define ACCONF	ACPHY_DEFAULT
#  endif /* !ACCONF */
#  ifndef ACCONF2
#    define ACCONF2	ACPHY_DEFAULT2
#  endif /* !ACCONF2 */
#  ifndef ACCONF5
#    define ACCONF5	ACPHY_DEFAULT5
#  endif /* !ACCONF5 */
#else /* PHYCONF_DEFAULTS == 0 */
/* Do not configure any phy support by default. Only specified phy configs will be non-zero. */
#  ifndef NCONF
#    define NCONF	0
#  endif /* !NCONF */
#  ifndef LCN20CONF
#    define LCN20CONF	0
#  endif /* !LCN20CONF */
#  ifndef ACCONF
#    define ACCONF	0
#  endif /* !ACCONF */
#  ifndef ACCONF2
#    define ACCONF2	0
#  endif /* !ACCONF2 */
#  ifndef ACCONF5
#    define ACCONF5	0
#  endif /* !ACCONF5 */
#endif /* PHYCONF_DEFAULTS */

/* TODO: remove when d11ucode_xxx.c are fixed */
#undef ACONF
#define ACONF	0
#undef GCONF
#define GCONF	0
#undef LPCONF
#define LPCONF	0
#undef SSLPNCONF
#define SSLPNCONF	0
#undef LCNCONF
#define LCNCONF	0
#undef LCN40CONF
#define LCN40CONF	0
#undef HTCONF
#define HTCONF	0

/* support 2G band */
#if NCONF || LCNCONF || HTCONF || LCN40CONF || LCN20CONF || ACCONF || ACCONF2 || \
	ACCONF5
#define BAND2G
#endif // endif

/* support 5G band */
#if defined(DBAND)
#define BAND5G
#endif // endif

#ifdef WL11N
#if NCONF || ACCONF || ACCONF2 || ACCONF5
#define WLANTSEL	1
#endif // endif

#if defined(WLAMSDU) && !defined(DONGLEBUILD)
#define WLAMSDU_TX      1
#endif // endif
#endif /* WL11N */

/***********************************
 * Some feature consistency checks *
 * *********************************
 */
#if defined(BCMSUP_PSK) && defined(LINUX_CRYPTO)
/* testing #error	"Only one supplicant can be defined; BCMSUP_PSK or LINUX_CRYPTO" */
#endif // endif

#if defined(BCMSUP_PSK) && !defined(STA)
#error	"STA must be defined when BCMSUP_PSK is defined"
#endif // endif

#if defined(WET) && !defined(STA)
#error	"STA must be defined when WET is defined"
#endif // endif

#if (!defined(AP) || !defined(STA)) && defined(APSTA)
#error "APSTA feature requested without both AP and STA defined"
#endif // endif

#if defined(WOWL) && !defined(STA)
#error "STA should be defined for WOWL"
#endif // endif

#if !defined(WLPLT)

#if !defined(AP) && !defined(STA)
// #error	"Neither AP nor STA were defined"
#endif // endif
#if defined(BAND5G) && !defined(WL11H)
#error	"WL11H is required when 5G band support is configured"
#endif // endif

#endif /* !WLPLT */

#if !defined(WME) && defined(WLCAC)
#error	"WME support required"
#endif // endif

/* RXCHAIN_PWRSAVE/WL11N consistency check */
#if defined(RXCHAIN_PWRSAVE) && !defined(WL11N)
#error "WL11N should be defined for RXCHAIN_PWRSAVE"
#endif // endif

/* AP TPC and 11h consistency check */
#if defined(WL_AP_TPC) && !defined(WL11H)
#error "WL11H should be defined for WL_AP_TPC"
#endif // endif

/* AMPDU/WL11N consistency check */
#if defined(WLAMPDU) && !defined(WL11N)
#error "WL11N should be defined for WLAMPDU"
#endif // endif

/* AMSDU/WL11N consistency check */
#if defined(WLAMSDU) && !defined(WL11N)
#error "WL11N should be defined for WLAMSDU"
#endif // endif

/* WL11N/WL11AC consistency check */
#if defined(WL11AC) && !defined(WL11N)
#error "WL11N should be defined for WL11AC"
#endif // endif

/* WL11AC/WL11AX consistency check */
#if defined(WL11AX) && !defined(WL11AC)
#error "WL11AC should be defined for WL11AX"
#endif // endif

#if defined(BCMWAPI_WAI) && defined(WL11K)
/* #error "BCMWAPI_WAI and WL11K can't be defined at the same time" */
#endif // endif

#if defined(DWDS) && !defined(WDS)
#error "WDS should be defined for DWDS"
#endif // endif
/* AMPDU Host reorder config checks */
#ifdef WLAMPDU_HOSTREORDER
#if !defined(BRCMAPIVTW)
#error "WLAMPDU_HOSTREORDER feature depends on BRCMAPIVTW feature"
#endif // endif
#if !defined(DONGLEBUILD)
#error "WLAMPDU_HOSTREORDER feature is a valid feature only for full dongle builds"
#endif // endif
#if !(defined(BCMPCIEDEV) || defined(PROP_TXSTATUS))
#error "WLAMPDU_HOSTREORDER requires PROP_TXSTATUS for non PCIE DEV builds"
#endif // endif
#if !defined(WLAMPDU_HOSTREORDER_DISABLED) && !(defined(PROP_TXSTATUS_ENABLED) || \
	defined(BCMPCIEDEV_ENABLED))
#error "WLAMPDU_HOSTREORDER requires PROP_TXSTATUS_ENABLED for non PCIE FULL Dongle "
#endif // endif
#endif /* WLAMPDU_HOSTREORDER */

#if defined(MBSS) && !defined(MBSS_DISABLED) && defined(WLP2P_UCODE_ONLY)
#error "MBSS requires non-P2P ucode"
#endif // endif

#if defined(BCM_DMA_CT) && !defined(BCM_DMA_INDIRECT)
#error "BCM_DMA_CT feature depends on BCM_DMA_INDIRECT"
#endif // endif

#if defined(WL_MU_TX) && !defined(BCM_DMA_INDIRECT)
#error "WL_MU_TX feature depends on BCM_DMA_INDIRECT"
#endif // endif

#if defined(BCM_DMA_CT) && defined(DMA_TX_FREE) && !defined(DMA_TX_FREE_DISABLED)
#error "DMA_TX_FREE must not be defined along with BCM_DMA_CT"
#endif // endif

#if defined(STS_FIFO_RXEN) && !defined(BULKRX_PKTLIST)
#error "STS_FIFO_RXEN feature depends on BULKRX_PKTLIST"
#endif // endif

#if defined(WLMCNX) && defined(DONGLEBUILD)
	#ifndef WLP2P_UCODE
		#error WLP2P_UCODE is required for MCNX on dongles
	#endif	/* !WLP2P_UCODE */
	#if !defined(ROM_ENAB_RUNTIME_CHECK) && !defined(WLP2P_UCODE_ONLY) && \
	!defined(WLMCNX_DISABLED)
		#error WLP2P_UCODE_ONLY is required for MCNX on dongles
	#endif  /* !WLP2P_UCODE_ONLY */
#endif /* WLMCNX && DONGLEBUILD */

/********************************************************************
 * Phy/Core Configuration.  Defines macros to to check core phy/rev *
 * compile-time configuration.  Defines default core support.       *
 * ******************************************************************
 */

/* Basic macros to check a configuration bitmask */

#define IS_MULTI_REV(config)	(((config) & ((config) - 1)) != 0) /* is more than one bit set? */

#define IS_MULTI_REV2(config1, config2) (IS_MULTI_REV(config1) || IS_MULTI_REV(config2) || \
	((config1) && (config2)))

#define IS_MULTI_REV3(config1, config2, config3) \
	(IS_MULTI_REV(config1) || IS_MULTI_REV(config2) || IS_MULTI_REV(config3) || \
	((config1) && (config2)) || ((config1) && (config3)) || ((config2) && (config3)))

#define CONF_HAS(config, val)	(((config) & (1U << (val))) != 0)
#define CONF_MSK(config, mask)	(((config) & (mask)) != 0)
#define MSK_RANGE(low, hi)	(((1U << ((hi) + 1)) - (1U << (low))) != 0)
#define CONF_RANGE(config, low, hi) (CONF_MSK(config, MSK_RANGE(low, high)))

#define CONF_IS(config, val)	((config) == (1U << (val)))
#define CONF_GE(config, val)	(((config) & (0 - (1U << (val)))) != 0)
#define CONF_GT(config, val)	(((config) & (0 - 2 * (1U << (val)))) != 0)
#define CONF_LT(config, val)	(((config) & ((1U << (val)) - 1)) != 0)
#define CONF_LE(config, val)	(((config) & (2 * (1U << (val)) - 1)) != 0)

/* Wrappers for some of the above, specific to config constants */

#define NCONF_HAS(val)	CONF_HAS(NCONF, val)
#define NCONF_MSK(mask)	CONF_MSK(NCONF, mask)
#define NCONF_IS(val)	CONF_IS(NCONF, val)
#define NCONF_GE(val)	CONF_GE(NCONF, val)
#define NCONF_GT(val)	CONF_GT(NCONF, val)
#define NCONF_LT(val)	CONF_LT(NCONF, val)
#define NCONF_LE(val)	CONF_LE(NCONF, val)

#define LCN20CONF_HAS(val)	CONF_HAS(LCN20CONF, val)
#define LCN20CONF_MSK(mask)	CONF_MSK(LCN20CONF, mask)
#define LCN20CONF_IS(val)	CONF_IS(LCN20CONF, val)
#define LCN20CONF_GE(val)	CONF_GE(LCN20CONF, val)
#define LCN20CONF_GT(val)	CONF_GT(LCN20CONF, val)
#define LCN20CONF_LT(val)	CONF_LT(LCN20CONF, val)
#define LCN20CONF_LE(val)	CONF_LE(LCN20CONF, val)

#define ACCONF_MSK(mask)	CONF_MSK(ACCONF, mask)
#define ACCONF2_MSK(mask)	CONF_MSK(ACCONF2, mask)
#define ACCONF5_MSK(mask)	CONF_MSK(ACCONF5, mask)
#define ACCONF_HAS(val) (\
	(ACCONF_CHK0(val, ACCONF1_BASE) && CONF_HAS(ACCONF, ACCONF_REV0(val, ACCONF1_BASE))) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && CONF_HAS(ACCONF2, ACCONF_REV(val, ACCONF2_BASE))) || \
	(ACCONF_CHK(val, ACCONF5_BASE) && CONF_HAS(ACCONF5, ACCONF_REV(val, ACCONF5_BASE))))
#define ACCONF_IS(val) (\
	(ACCONF_CHK0(val, ACCONF1_BASE) && !ACCONF2 && !ACCONF5 &&	\
	 CONF_IS(ACCONF, ACCONF_REV0(val, ACCONF1_BASE))) ||	\
	(ACCONF_CHK(val, ACCONF2_BASE) && !ACCONF && !ACCONF5 &&	\
	 CONF_IS(ACCONF2, ACCONF_REV(val, ACCONF2_BASE))) ||	\
	 (ACCONF_CHK(val, ACCONF5_BASE) && !ACCONF && !ACCONF2 &&	\
	 CONF_IS(ACCONF5, ACCONF_REV(val, ACCONF5_BASE))))
#define ACCONF_GE(val) (\
	(ACCONF_CHK0(val, ACCONF1_BASE) && CONF_GE(ACCONF, ACCONF_REV0(val, ACCONF1_BASE))) || \
	(ACCONF_CHK0(val, ACCONF1_BASE) && (ACCONF2 || ACCONF5)) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && CONF_GE(ACCONF2, ACCONF_REV(val, ACCONF2_BASE))) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && (ACCONF5)) || \
	(ACCONF_CHK(val, ACCONF5_BASE) && CONF_GE(ACCONF5, ACCONF_REV(val, ACCONF5_BASE))))
#define ACCONF_GT(val) (\
	(ACCONF_CHK0(val, ACCONF1_BASE) && CONF_GT(ACCONF, ACCONF_REV0(val, ACCONF1_BASE))) || \
	(ACCONF_CHK0(val, ACCONF1_BASE) && (ACCONF2 || ACCONF5)) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && CONF_GT(ACCONF2, ACCONF_REV(val, ACCONF2_BASE))) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && ACCONF5) || \
	(ACCONF_CHK(val, ACCONF5_BASE) && CONF_GT(ACCONF5, ACCONF_REV(val, ACCONF5_BASE))))
#define ACCONF_LT(val) (\
	(ACCONF_CHK0(val, ACCONF1_BASE) && CONF_LT(ACCONF, ACCONF_REV0(val, ACCONF1_BASE))) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && CONF_LT(ACCONF2, ACCONF_REV(val, ACCONF2_BASE))) || \
	(ACCONF_CHK(val, ACCONF5_BASE) && CONF_LT(ACCONF5, ACCONF_REV(val, ACCONF5_BASE))) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && ACCONF) || \
	(ACCONF_CHK(val, ACCONF5_BASE) && (ACCONF || ACCONF2)))
#define ACCONF_LE(val) (\
	(ACCONF_CHK0(val, ACCONF1_BASE) && CONF_LE(ACCONF, ACCONF_REV0(val, ACCONF1_BASE))) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && CONF_LE(ACCONF2, ACCONF_REV(val, ACCONF2_BASE))) || \
	(ACCONF_CHK(val, ACCONF5_BASE) && CONF_LE(ACCONF5, ACCONF_REV(val, ACCONF5_BASE))) || \
	(ACCONF_CHK(val, ACCONF2_BASE) && ACCONF) || \
	(ACCONF_CHK(val, ACCONF5_BASE) && (ACCONF || ACCONF2)))

#define D11CONF_HAS(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_HAS(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) ||\
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_HAS(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) ||\
	(D11CONF_CHK(val, D11CONF3_BASE) && CONF_HAS(D11CONF3, D11CONF_REV(val, D11CONF3_BASE))) ||\
	(D11CONF_CHK(val, D11CONF4_BASE) && CONF_HAS(D11CONF4, D11CONF_REV(val, D11CONF4_BASE))) ||\
	(D11CONF_CHK(val, D11CONF5_BASE) && CONF_HAS(D11CONF5, D11CONF_REV(val, D11CONF5_BASE))))
#define D11CONF_IS(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && !D11CONF2 && !D11CONF3 && !D11CONF4 && !D11CONF5 &&	\
	 CONF_IS(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) ||	\
	(D11CONF_CHK(val, D11CONF2_BASE) && !D11CONF && ! D11CONF3 && !D11CONF4 && !D11CONF5 &&	\
	 CONF_IS(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) ||	\
	(D11CONF_CHK(val, D11CONF3_BASE) && !D11CONF && ! D11CONF2 && !D11CONF4 && !D11CONF5 &&	\
	 CONF_IS(D11CONF3, D11CONF_REV(val, D11CONF3_BASE))) ||	\
	(D11CONF_CHK(val, D11CONF4_BASE) && !D11CONF && ! D11CONF2 && !D11CONF3 && !D11CONF5 &&	\
	 CONF_IS(D11CONF4, D11CONF_REV(val, D11CONF4_BASE))) ||	\
	(D11CONF_CHK(val, D11CONF5_BASE) && !D11CONF && ! D11CONF2 && !D11CONF3 && !D11CONF4 &&	\
	 CONF_IS(D11CONF5, D11CONF_REV(val, D11CONF5_BASE))))
#define D11CONF_GE(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_GE(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF1_BASE) && (D11CONF2 || D11CONF3 || D11CONF4 || D11CONF5)) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_GE(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && (D11CONF3 || D11CONF4 || D11CONF5)) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && CONF_GE(D11CONF3, D11CONF_REV(val, D11CONF3_BASE))) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && (D11CONF4 ||D11CONF5)) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && CONF_GE(D11CONF4, D11CONF_REV(val, D11CONF4_BASE))) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && D11CONF5) || \
	(D11CONF_CHK(val, D11CONF5_BASE) && CONF_GE(D11CONF5, D11CONF_REV(val, D11CONF5_BASE))))
#define D11CONF_GT(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_GT(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF1_BASE) && (D11CONF2 || D11CONF3 || D11CONF4 || D11CONF5)) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_GT(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && (D11CONF3 || D11CONF4 || D11CONF5)) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && CONF_GT(D11CONF3, D11CONF_REV(val, D11CONF3_BASE))) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && (D11CONF4 ||D11CONF5)) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && CONF_GT(D11CONF4, D11CONF_REV(val, D11CONF4_BASE))) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && D11CONF5) || \
	(D11CONF_CHK(val, D11CONF5_BASE) && CONF_GT(D11CONF5, D11CONF_REV(val, D11CONF5_BASE))))
#define D11CONF_LT(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_LT(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_LT(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && CONF_LT(D11CONF3, D11CONF_REV(val, D11CONF3_BASE))) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && CONF_LT(D11CONF4, D11CONF_REV(val, D11CONF4_BASE))) || \
	(D11CONF_CHK(val, D11CONF5_BASE) && CONF_LT(D11CONF5, D11CONF_REV(val, D11CONF5_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && D11CONF) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && (D11CONF || D11CONF2)) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && (D11CONF || D11CONF2 || D11CONF3)) || \
	(D11CONF_CHK(val, D11CONF5_BASE) && (D11CONF || D11CONF2 || D11CONF3 || D11CONF4)))
#define D11CONF_LE(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_LE(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_LE(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && CONF_LE(D11CONF3, D11CONF_REV(val, D11CONF3_BASE))) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && CONF_LE(D11CONF4, D11CONF_REV(val, D11CONF4_BASE))) || \
	(D11CONF_CHK(val, D11CONF5_BASE) && CONF_LE(D11CONF5, D11CONF_REV(val, D11CONF5_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && D11CONF) || \
	(D11CONF_CHK(val, D11CONF3_BASE) && (D11CONF || D11CONF2)) || \
	(D11CONF_CHK(val, D11CONF4_BASE) && (D11CONF || D11CONF2 || D11CONF3)) || \
	(D11CONF_CHK(val, D11CONF5_BASE) && (D11CONF || D11CONF2 || D11CONF3 || D11CONF4)))

#define D11CONF_MINOR_HAS(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_HAS(D11CONF_MINOR, \
	D11CONF_REV(val, D11CONF1_BASE))))
#define D11CONF_MINOR_IS(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_IS(D11CONF_MINOR, \
	D11CONF_REV(val, D11CONF1_BASE))))
#define D11CONF_MINOR_GE(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_GE(D11CONF_MINOR, \
	D11CONF_REV(val, D11CONF1_BASE))))
#define D11CONF_MINOR_GT(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_GT(D11CONF_MINOR, \
	D11CONF_REV(val, D11CONF1_BASE))))
#define D11CONF_MINOR_LT(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_LT(D11CONF_MINOR, \
	D11CONF_REV(val, D11CONF1_BASE))))
#define D11CONF_MINOR_LE(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_LE(D11CONF_MINOR, \
	D11CONF_REV(val, D11CONF1_BASE))))

#define PHYCONF_HAS(val) CONF_HAS(PHYTYPE, val)
#define PHYCONF_IS(val)	CONF_IS(PHYTYPE, val)

/* Macros to check (but override) a run-time value; compile-time
 * override allows unconfigured code to be optimized out.
 *
 * Note: the XXCONF values contain 0 or more bits, each of which represents a supported revision
 */

#if IS_MULTI_REV(NCONF)
#define NREV_IS(var, val)	(NCONF_HAS(val) && (var) == (val))
#define NREV_GE(var, val)	(!NCONF_LT(val) || (var) >= (val))
#define NREV_GT(var, val)	(!NCONF_LE(val) || (var) > (val))
#define NREV_LT(var, val)	(!NCONF_GE(val) || (var) < (val))
#define NREV_LE(var, val)	(!NCONF_GT(val) || (var) <= (val))
#else
#define NREV_IS(var, val)	NCONF_HAS(val)
#define NREV_GE(var, val)	NCONF_GE(val)
#define NREV_GT(var, val)	NCONF_GT(val)
#define NREV_LT(var, val)	NCONF_LT(val)
#define NREV_LE(var, val)	NCONF_LE(val)
#endif	/* IS_MULTI_REV(NCONF) */

#if IS_MULTI_REV(LCN20CONF)
#define LCN20REV_IS(var, val)	(LCN20CONF_HAS(val) && (var) == (val))
#define LCN20REV_GE(var, val)	(!LCN20CONF_LT(val) || (var) >= (val))
#define LCN20REV_GT(var, val)	(!LCN20CONF_LE(val) || (var) > (val))
#define LCN20REV_LT(var, val)	(!LCN20CONF_GE(val) || (var) < (val))
#define LCN20REV_LE(var, val)	(!LCN20CONF_GT(val) || (var) <= (val))
#else
#define LCN20REV_IS(var, val)	LCN20CONF_HAS(val)
#define LCN20REV_GE(var, val)	LCN20CONF_GE(val)
#define LCN20REV_GT(var, val)	LCN20CONF_GT(val)
#define LCN20REV_LT(var, val)	LCN20CONF_LT(val)
#define LCN20REV_LE(var, val)	LCN20CONF_LE(val)
#endif	/* IS_MULTI_REV(LCN20CONF) */

#if IS_MULTI_REV3(ACCONF, ACCONF2, ACCONF5)
#define ACREV_IS(var, val)	(ACCONF_HAS(val) && (var) == (val))
#define ACREV_GE(var, val)	(!ACCONF_LT(val) || (var) >= (val))
#define ACREV_GT(var, val)	(!ACCONF_LE(val) || (var) > (val))
#define ACREV_LT(var, val)	(!ACCONF_GE(val) || (var) < (val))
#define ACREV_LE(var, val)	(!ACCONF_GT(val) || (var) <= (val))
#else
#define ACREV_IS(var, val)	ACCONF_HAS(val)
#define ACREV_GE(var, val)	ACCONF_GE(val)
#define ACREV_GT(var, val)	ACCONF_GT(val)
#define ACREV_LT(var, val)	ACCONF_LT(val)
#define ACREV_LE(var, val)	ACCONF_LE(val)
#endif	/* IS_MULTI_REV3(ACCONF, ACCONF2, ACCONF5) */

#define D11REV_IS(var, val)	(D11CONF_HAS(val) && (D11CONF_IS(val) || ((var) == (val))))
#define D11REV_GE(var, val)	(D11CONF_GE(val) && (!D11CONF_LT(val) || ((var) >= (val))))
#define D11REV_GT(var, val)	(D11CONF_GT(val) && (!D11CONF_LE(val) || ((var) > (val))))
#define D11REV_LT(var, val)	(D11CONF_LT(val) && (!D11CONF_GE(val) || ((var) < (val))))
#define D11REV_LE(var, val)	(D11CONF_LE(val) && (!D11CONF_GT(val) || ((var) <= (val))))

#define IS_D11_ACREV(wlc) D11REV_GT((wlc)->pub->corerev, 40)

#define PHYTYPE_IS(var, val)	(PHYCONF_HAS(val) && (PHYCONF_IS(val) || ((var) == (val))))

#define D11MINORREV_IS(var, val)	\
	(D11CONF_MINOR_HAS(val) && (D11CONF_MINOR_IS(val) || ((var) == (val))))
#define D11MINORREV_GE(var, val)	\
	(D11CONF_MINOR_GE(val) && (!D11CONF_MINOR_LT(val) || ((var) >= (val))))
#define D11MINORREV_GT(var, val)	\
	(D11CONF_MINOR_GT(val) && (!D11CONF_MINOR_LE(val) || ((var) > (val))))
#define D11MINORREV_LT(var, val)	\
	(D11CONF_MINOR_LT(val) && (!D11CONF_MINOR_GE(val) || ((var) < (val))))
#define D11MINORREV_LE(var, val)	\
	(D11CONF_MINOR_LE(val) && (!D11CONF_MINOR_GT(val) || ((var) <= (val))))

/* First ACPHY rev where HECAP was introduced */
#define HECAP_FIRST_ACREV	(44)

/* HECAP related configuration for 802.11ax */

/* Define HECAP_DONGLE as 1 only for HE capable phy's in tunable file. */
#ifndef HECAP_DONGLE
#define HECAP_DONGLE	(0)
#endif /* HECAP_DONGLE */

/* ACPHY rev to HECAP rev mapping
 * Note: HECAP_REV 0xFF is invalid revision
 *	ACPHY rev --	HECAP rev
 *	44		0
 * Whenever there is a HECap change, add a new HEcap rev corresponding to-
 *	the respective ACPHY rev.
 */
#ifdef NOT_YET
static const int acphy_hecap_rev[] = {
    0	/* ACPHY rev: 44 */
};

#define ACPHY2HECAPREV(phyrev)	\
	(((phyrev) >= HECAP_FIRST_ACREV) && \
	((phyrev) - HECAP_FIRST_ACREV < ARRAYSIZE(acphy_hecap_rev)) ? \
		acphy_hecap_rev[(phyrev) - HECAP_FIRST_ACREV] : \
		0xFF)

#endif /* HEREV_USING_ARRAY */

#define ACPHY2HECAPREV(phyrev)	\
	((ACREV_GE(phyrev, HECAP_FIRST_ACREV) && (phyrev != 128)) ? 0 : \
	0xFF)

#define HECAPREV_IS(phyrev, val)	(ACPHY2HECAPREV(phyrev) == val)
#define HECAPREV_GE(phyrev, val)	(ACPHY2HECAPREV(phyrev) >= val)
#define HECAPREV_LE(phyrev, val)	(ACPHY2HECAPREV(phyrev) <= val)
#define HECAPREV_GT(phyrev, val)	(ACPHY2HECAPREV(phyrev) > val)
#define HECAPREV_LT(phyrev, val)	(ACPHY2HECAPREV(phyrev) < val)

/**
 * indicates if this a HE generation chip (including the ones that were optionally restricted to
 * VHT)
 */
#define WLCISHECAPPHY(band)		(HECAP_DONGLE ||\
					ACPHY2HECAPREV((band)->phyrev) != 0xFF)

#if D11CONF_GE(64) && D11CONF_LT(80)
#define VASIP_SPECTRUM_ANALYSIS

/* MU-PKTENG only for NIC Build */

#ifndef DONGLEBUILD
#if defined(AP) && defined(WL_BEAMFORMING) && defined(WL_MU_TX) && (defined(WLTEST) || \
	defined(WLPKTENG))
#define WL_MUPKTENG
#endif // endif
#endif /* !DONGLEBUILD */
#else
#endif /* D11CONF_GE(64) && D11CONF_LT(80) */

#if D11CONF_GE(64)
#define WL_HWKTAB
#endif /* D11CONF_GE(64) */

#ifndef DONGLEBUILD
#if (D11CONF_GE(64) || D11CONF_IS(61))
#define WLVASIP
#endif /* D11CONF_GE(64) || D11CONF_IS(61) */
#endif /* !DONGLEBUILD */

#if D11CONF_HAS(65)
#define VASIP_11AC_4X4_ENAB 1
#define VASIP_11AC_3X3_ENAB 1
#endif // endif
#if (D11CONF_HAS(128) || D11CONF_HAS(129))
#define VASIP_11AX_4X4_ENAB 1
#endif // endif
#if D11CONF_HAS(130)
#define VASIP_11AX_2X2_ENAB 1
#endif // endif

#ifndef DONGLEBUILD
#if D11CONF_GE(128)
#define WLSMC
#endif /* D11CONF_GE(128) */
#endif /* DONGLEBUILD */

/* Finally, early-exit from switch case if anyone wants it... */

#define CASECHECK(config, val)	if (!(CONF_HAS(config, val))) break
#define CASEMSK(config, mask)	if (!(CONF_MSK(config, mask))) break

#if (D11CONF ^ (D11CONF & D11_DEFAULT))
#error "Unsupported MAC revision configured"
#endif // endif
#if (NCONF ^ (NCONF & NPHY_DEFAULT))
#error "Unsupported NPHY revision configured"
#endif // endif
#if (LCNCONF ^ (LCNCONF & LCNPHY_DEFAULT))
#error "Unsupported LCNPHY revision configured"
#endif // endif
#if (LCN40CONF ^ (LCN40CONF & LCN40PHY_DEFAULT))
#error "Unsupported LCN40PHY revision configured"
#endif // endif
#if (LCN20CONF ^ (LCN20CONF & LCN20PHY_DEFAULT))
#error "Unsupported LCN20PHY revision configured"
#endif // endif
#if (ACCONF ^ (ACCONF & ACPHY_DEFAULT)) || (ACCONF2 ^ (ACCONF2 & ACPHY_DEFAULT2)) || \
	(ACCONF5 ^ (ACCONF5 & ACPHY_DEFAULT5))
#error "Unsupported ACPHY revision configured"
#endif // endif

/* *** Consistency checks *** */
#if !D11CONF && !D11CONF2 && !D11CONF3 && !D11CONF4 && !D11CONF5
#error "No MAC revisions configured!"
#endif // endif

#if !NCONF && !LCN20CONF && !ACCONF && !ACCONF2 && !ACCONF5
#error "No PHY configured!"
#endif // endif

/* Set up PHYTYPE automatically: (depends on PHY_TYPE_X, from d11.h) */
#if NCONF
#define _PHYCONF_N (1U << PHY_TYPE_N)
#else
#define _PHYCONF_N 0
#endif /* NCONF */

#if LCN20CONF
#define _PHYCONF_LCN20 (1U << PHY_TYPE_LCN20)
#else
#define _PHYCONF_LCN20 0
#endif /* LCN20CONF */

#if ACCONF || ACCONF2 || ACCONF5
#define _PHYCONF_AC (1U << PHY_TYPE_AC)
#else
#define _PHYCONF_AC 0
#endif /* ACCONF || ACCONF2 || ACCONF5 */

#define PHYTYPE (_PHYCONF_N | _PHYCONF_LCN20 | _PHYCONF_AC)

/* Utility macro to identify 802.11n (HT) capable PHYs */
#define PHYTYPE_11N_CAP(phytype) \
	(PHYTYPE_IS(phytype, PHY_TYPE_N) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_LCN20) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_AC) ||	\
	 0)

/* Utility macro to identify MIMO capable PHYs */
#define PHYTYPE_MIMO_CAP(phytype)	PHYTYPE_11N_CAP(phytype)

#define PHYTYPE_HT_CAP(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_AC)

/* Utility macro to identify 802.11ac (VHT) capable PHYs */
#define PHYTYPE_VHT_CAP(phytype) \
	(PHYTYPE_IS(phytype, PHY_TYPE_AC) || \
	 0)

/* Last but not least: shorter wlc-specific var checks */
#define WLCISNPHY(band)		PHYTYPE_IS((band)->phytype, PHY_TYPE_N)
#define WLCISLCN20PHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_LCN20)
#define WLCISACPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_AC)
#define WLC_PHY_11N_CAP(band)	PHYTYPE_11N_CAP((band)->phytype)
#define WLC_PHY_VHT_CAP(band)	PHYTYPE_VHT_CAP((band)->phytype)
/* WLC_HE_CAP_PHY is defined elsewhere */

/* END_OF HECAP related configuration */

/**********************************************************************
 * ------------- End of Core phy/rev configuration. ----------------- *
 * ********************************************************************
 */

/*************************************************
 * Defaults for tunables (e.g. sizing constants)
 *
 * For each new tunable, add a member to the end
 * of wlc_tunables_t in wlc_pub.h to enable
 * runtime checks of tunable values. (Directly
 * using the macros in code invalidates ROM code)
 *
 * ***********************************************
 */
#ifndef NTXD
#define NTXD		512   /* Max # of entries in Tx FIFO. 512 maximum */
#endif /* NTXD */
#ifndef NRXD
#define NRXD		256   /* Max # of entries in Rx FIFO. 512 maximum */
#endif /* NRXD */

/* Separate tunable for DMA descriptor ring size for cores
 * with large descriptor ring support (4k descriptors)
 */
#ifndef NTXD_LARGE
#define NTXD_LARGE		1024		/* TX descriptor ring */
#endif // endif

#ifndef NRXD_LARGE
#define NRXD_LARGE		NRXD		/* RX descriptor ring */
#endif // endif

#ifndef NTXD_TRIG_MAX
#define NTXD_TRIG_MAX		128		/* Max Trigger Tx descriptor ring */
#endif // endif

#ifndef NTXD_TRIG_MIN
#define NTXD_TRIG_MIN		64		/* Min Trigger Tx descriptor ring */
#endif // endif

#ifndef NTXD_BCMC
#define NTXD_BCMC		NTXD		/* Max # of entries in BCMC Tx FIFO */
#endif /* NTXD_BCMC */

#ifndef NRXBUFPOST
/* NRXBUFPOST must be smaller than NRXD */
#define	NRXBUFPOST	64		/* try to keep this # rbufs posted to the chip */
#endif /* NRXBUFPOST */

#ifndef NRXBUFPOST_SMALL
#define	NRXBUFPOST_SMALL	32	/* try to keep this # rbufs posted to the core1 chip */
#endif /* NRXBUFPOST_SMALL */

#ifndef NRXBUFPOST_FRWD
/* try to keep this # rbufs posted to the both chips
 * when poolreorg for frwd path happens.
 */
#define	NRXBUFPOST_FRWD		2
#endif /* NRXBUFPOST_FRWD */

#ifndef NRXD_STS
#define NRXD_STS 512
#endif // endif

#ifndef NRXBUFPOST_STS
#define NRXBUFPOST_STS 256
#endif // endif

#ifndef STSBUF_MP_N_OBJ
#define STSBUF_MP_N_OBJ 512
#endif // endif

#ifndef WL_RXBND
#define	WL_RXBND	(NRXBUFPOST / 2) /* try to keep these many rx chain packets in chip */
#endif /* WL_RXBND */

#ifndef WL_RXBND_SMALL
#define	WL_RXBND_SMALL	(NRXBUFPOST_SMALL / 2) /* try to keep these many rx chain in core1 chip */
#endif /* WL_RXBND_SMALL */

#ifndef WLC_AMSDU_RXPOST_THRESH
#define WLC_AMSDU_RXPOST_THRESH	64 /* min rxpost threshold required to enable amsdu_rx by default */
#endif /* WLC_AMSDU_RXPOST_THRESH */

#ifdef WL_MU_MONITOR
#define MONPKTBUFSZ 4096 /* pktbufsz for mu monitor mode */
#else
#define MONPKTBUFSZ PKTBUFSZ
#endif /* WL_MU_MONITOR */

#ifndef TXMR
#define TXMR			2	/* number of outstanding reads */
#endif // endif

#ifndef TXPREFTHRESH
#define TXPREFTHRESH		8	/* prefetch threshold */
#endif // endif

#ifndef TXPREFCTL
#define TXPREFCTL		16	/* max descr allowed in prefetch request */
#endif // endif

#ifndef TXBURSTLEN
#define TXBURSTLEN		1024	/* burst length for dma reads */
#endif // endif

#ifndef RXPREFTHRESH
#define RXPREFTHRESH		8	/* prefetch threshold */
#endif // endif

#ifndef RXPREFCTL
#define RXPREFCTL		16	/* max descr allowed in prefetch request */
#endif // endif

#ifndef RXBURSTLEN
#define RXBURSTLEN		128	/* burst length for dma writes */
#endif // endif

#ifndef MRRS
#define MRRS			AUTO	/* Max read request size */
#endif // endif

#ifndef TXMR_AC2
#define TXMR_AC2		12	/* number of outstanding reads */
#endif // endif

#ifndef TXPREFTHRESH_AC2
#define TXPREFTHRESH_AC2	8	/* prefetch threshold */
#endif // endif

#ifndef TXPREFCTL_AC2
#define TXPREFCTL_AC2		16	/* max descr allowed in prefetch request */
#endif // endif

#ifndef TXBURSTLEN_AC2
#define TXBURSTLEN_AC2		1024	/* burst length for dma reads */
#endif // endif

#ifndef RXPREFTHRESH_AC2
#define RXPREFTHRESH_AC2	8	/* prefetch threshold */
#endif // endif

#ifndef RXPREFCTL_AC2
#define RXPREFCTL_AC2		16	/* max descr allowed in prefetch request */
#endif // endif

#ifndef RXBURSTLEN_AC2
#define RXBURSTLEN_AC2		128	/* burst length for dma writes */
#endif // endif

#ifndef MRRS_AC2
#define MRRS_AC2		1024	/* Max read request size */
#endif // endif

#ifndef WLC_MAXMODULES
#define WLC_MAXMODULES		89	/* max #  wlc_module_register() calls */
#endif // endif

#ifndef MAXSCB				/* station control blocks in cache */
#ifdef AP
#define	MAXSCB		128		/* Maximum SCBs in cache for AP */
#else
#define MAXSCB		32		/* Maximum SCBs in cache for STA */
#endif /* AP */
#endif /* MAXSCB */

#ifndef DEFMAXSCB
#define DEFMAXSCB		MAXSCB	/* default value of max # of STAs allowed to join */
#endif /* DEFMAXSCB */

#ifndef MAXSCBCUBBIES
#define MAXSCBCUBBIES		47	/* max number of cubbies in scb container */
#endif // endif

#ifndef MAXBSSCFGCUBBIES
#define MAXBSSCFGCUBBIES	55	/* max number of cubbies in bsscfg container */
#endif // endif

#ifdef AMPDU_NUM_MPDU			/* Used for the memory limited dongles. */
#define AMPDU_NUM_MPDU_1SS_D11LEGACY	AMPDU_NUM_MPDU /* Small fifo D11 Maccore < 16 */
#define AMPDU_NUM_MPDU_2SS_D11LEGACY	AMPDU_NUM_MPDU /* Small fifo D11 Maccore < 16 */
#define AMPDU_NUM_MPDU_3SS_D11LEGACY	AMPDU_NUM_MPDU /* Small fifo D11 Maccore < 16 */
#define AMPDU_NUM_MPDU_1SS_D11HT	AMPDU_NUM_MPDU /* Medium fifo 16 < D11 Maccore < 40 */
#define AMPDU_NUM_MPDU_2SS_D11HT	AMPDU_NUM_MPDU /* Medium fifo 16 < D11 Maccore < 40 */
#define AMPDU_NUM_MPDU_3SS_D11HT	AMPDU_NUM_MPDU /* Medium fifo 16 < D11 Maccore < 40 */
#define AMPDU_NUM_MPDU_1SS_D11AQM	AMPDU_NUM_MPDU /* Large fifo D11 Maccore > 40 */
#define AMPDU_NUM_MPDU_2SS_D11AQM	AMPDU_NUM_MPDU /* Large fifo D11 Maccore > 40 */
#define AMPDU_NUM_MPDU_3SS_D11AQM	AMPDU_NUM_MPDU /* Large fifo D11 Maccore > 40 */
#define AMPDU_NUM_MPDU_3STREAMS		AMPDU_NUM_MPDU
#define AMPDU_MAX_MPDU			AMPDU_NUM_MPDU
#define AMPDU_NUM_MPDU_LEGACY		AMPDU_NUM_MPDU
#else
#ifndef AMPDU_NUM_MPDU_1SS_D11LEGACY
#define AMPDU_NUM_MPDU_1SS_D11LEGACY	16 /* Small fifo D11 Maccore < 16 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_2SS_D11LEGACY
#define AMPDU_NUM_MPDU_2SS_D11LEGACY	16 /* Small fifo D11 Maccore < 16 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_3SS_D11LEGACY
#define AMPDU_NUM_MPDU_3SS_D11LEGACY	16 /* Small fifo D11 Maccore < 16 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_1SS_D11HT
#define AMPDU_NUM_MPDU_1SS_D11HT	32 /* Medium fifo 16 < D11 Maccore < 40 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_2SS_D11HT
#define AMPDU_NUM_MPDU_2SS_D11HT	32 /* Medium fifo 16 < D11 Maccore < 40 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_3SS_D11HT
#define AMPDU_NUM_MPDU_3SS_D11HT	32 /* Medium fifo 16 < D11 Maccore < 40 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_1SS_D11AQM
#define AMPDU_NUM_MPDU_1SS_D11AQM	32 /* Large fifo D11 Maccore > 40 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_2SS_D11AQM
#define AMPDU_NUM_MPDU_2SS_D11AQM	48 /* Large fifo D11 Maccore > 40 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_3SS_D11AQM
#define AMPDU_NUM_MPDU_3SS_D11AQM	64 /* Large fifo D11 Maccore > 40 */
#endif // endif

#ifndef AMPDU_NUM_MPDU_3STREAMS
#define AMPDU_NUM_MPDU_3STREAMS		AMPDU_NUM_MPDU_3SS_D11LEGACY
#endif // endif

#define AMPDU_NUM_MPDU			AMPDU_NUM_MPDU_2SS_D11LEGACY
#endif /* AMPDU_NUM_MPDU */

#ifndef AMPDU_PKTQ_LEN
#ifdef DONGLEBUILD
#define AMPDU_PKTQ_LEN		1024
#else
#define AMPDU_PKTQ_LEN		3000 /* enough to accomodate 4MB TCP window on NIC driver */
#endif /* DONGLEBUILD */
#endif /* AMPDU_PKTQ_LEN */

#ifndef AMPDU_PKTQ_FAVORED_LEN
#define AMPDU_PKTQ_FAVORED_LEN	1024
#endif // endif

#ifndef MAXPCBCDS
/* size of packet class callback class descriptor array */
#define MAXPCBCDS 4
#endif // endif

#ifndef MAXCD1PCBS
/* max # class 1 packet class callbacks */
#define MAXCD1PCBS 16
#endif // endif

#ifndef MAXCD2PCBS
/* max # class 2 packet class callbacks */
#define MAXCD2PCBS 4
#endif // endif

#ifndef MAXCD3PCBS
/* max # class 3 packet class callbacks */
#define MAXCD3PCBS 2
#endif // endif

#ifndef MAXCD4PCBS
/* max # class 4 packet class callbacks */
#define MAXCD4PCBS 2
#endif // endif

#ifndef AMPDU_MAX_MPDU
#if D11CONF_GE(40)
#define AMPDU_MAX_MPDU		64 /* max number of mpdus in an ampdu */
#else
#define AMPDU_MAX_MPDU		32 /* max number of mpdus in an ampdu */
#endif // endif
#endif /* AMPDU_MAX_MPDU */

#ifndef AMPDU_NUM_MPDU_LEGACY
#define AMPDU_NUM_MPDU_LEGACY	16
#endif // endif

#ifndef AMPDU_LOW_RATE_THRESH
#define AMPDU_LOW_RATE_THRESH   100000 /* Low rate threshold for small aggregation, in Kbps */
#endif // endif
#ifndef AMPDU_HIGH_RATE_THRESH
#define AMPDU_HIGH_RATE_THRESH   1000000 /* High rate threshold for large aggregation, in Kbps */
#endif // endif

#ifndef AMPDU_PKTQ_HI_LEN
#define AMPDU_PKTQ_HI_LEN	1536
#endif // endif

#ifndef AMPDU_PKTQ_FAVORED_LEN
#define AMPDU_PKTQ_FAVORED_LEN	1024
#endif // endif

#ifndef NAN_MAX_PEERS
#define NAN_MAX_PEERS	16
#endif // endif

#ifndef NAN_MAX_AVAIL
#ifdef WLRSDB
#define NAN_MAX_AVAIL 2
#else
#define NAN_MAX_AVAIL 1
#endif /* WLRSDB */
#endif /* NAN_MAX_AVAIL */

#ifndef NAN_MAX_NDC
#define NAN_MAX_NDC 4
#endif // endif

#ifndef RXPKTPOOLSZ
#define RXPKTPOOLSZ		1280
#endif // endif

#ifndef RX1PKTPOOLSZ
#define RX1PKTPOOLSZ		64
#endif // endif

#ifndef TXPKTPOOLSZ
#ifdef DONGLEBUILD
#define TXPKTPOOLSZ		1024
#else
#define TXPKTPOOLSZ		(AMPDU_PKTQ_LEN + 10) /* must be > data plane aggr queue */
#endif /* DONGLEBUILD */
#endif /* TXPKTPOOLSZ */

#ifndef TX2PKTPOOLSZ
#define TX2PKTPOOLSZ		8192
#endif // endif

#define D11HT_CORE	16
#define D11AQM_CORE	40 /* MAC supports hardware qggregation */

/* Count of packet callback structures. either of following
 * 1. Set to the number of SCBs since a STA
 * can queue up a rate callback for each IBSS STA it knows about, and an AP can
 * queue up an "are you there?" Null Data callback for each associated STA
 * 2. controlled by tunable config file
 */
#ifndef MAXPKTCB
#define MAXPKTCB	MAXSCB	/* Max number of packet callbacks */
#endif /* MAXPKTCB */

#ifndef WLC_MAXTDLS
#ifdef WLTDLS
#define WLC_MAXTDLS		5	/* Max # of TDLS peer table entries */
#else
#define WLC_MAXTDLS		0
#endif // endif
#endif /* WLC_MAXTDLS */

#ifndef WLC_MAXDLS_TIMERS
#ifdef WLTDLS
/*
* WLC_MAXTDLS_TIMERS
* Per Lookaside Entry: 1 Peer Timer, 1 ChannelSW Timer
* Per TDLS: 1 TDLS_PM_Timer, 1 BETDLS_RETRY_TIMER
*/
#define WLC_MAXDLS_TIMERS	((2*WLC_MAXTDLS)+(2*2))
#else
#define WLC_MAXDLS_TIMERS	0
#endif // endif
#endif /* WLC_MAXDLS_TIMERS */

#ifndef WLC_MAXTDLS_CONN
#ifdef WLTDLS
#define WLC_MAXTDLS_CONN	WLC_MAXTDLS	/* Max # of TDLS connections */
#else
#define WLC_MAXTDLS_CONN	0
#endif // endif
#endif /* WLC_MAXTDLS_CONN */

#ifndef WLC_TDLS_DISC_BLACKLIST_MAX
#ifdef WLTDLS
#define WLC_TDLS_DISC_BLACKLIST_MAX	5	/* Max # of TDLS discovery blacklist */
#else
#define WLC_TDLS_DISC_BLACKLIST_MAX	0
#endif /* WLTDLS */
#endif /* WLC_TDLS_DISC_BLACKLIST_MAX */

#ifndef WLC_MAX_WAIT_CTXT_DELETE
#define WLC_MAX_WAIT_CTXT_DELETE 2 /* Max wait(sec) to delete stale channel ctxt */
#endif // endif

#ifndef WLC_MAX_ASSOC_SCAN_RESULTS
/* Max number of assoc/roam scan results FW can hold */
/* This also acts as a feature enab flag hence initialize to zero */
#define WLC_MAX_ASSOC_SCAN_RESULTS	0
#endif /* WLC_MAX_ASSOC_SCAN_RESULTS */

#ifndef WLC_MAX_SCANCACHE_RESULTS
/* This also acts as a feature enab flag hence initialize to zero */
#define WLC_MAX_SCANCACHE_RESULTS	0
#endif /* WLC_MAX_SCANCACHE_RESULTS */

#ifndef WLC_MAXMFPS
#ifdef MFP
#if defined(NDIS630) || defined(NDIS640)
#define WLC_MAXMFPS	1	/* Win8 onwards only supports 802.11w on primary port */
#else
#define WLC_MAXMFPS	8	/* in general max times for MFP */
#endif /* NDIS630 || NDIS640 */
#else /* !MFP */
#define WLC_MAXMFPS	0
#endif /* MFP */
#endif /* WLC_MAXMFPS */

#ifndef CTFPOOLSZ
#define CTFPOOLSZ       128
#endif /* CTFPOOLSZ */

#define WLC_MAX_URE_STA	8
/* NetBSD also needs to keep track of this */
#ifndef WLC_MAX_UCODE_BSS
#define WLC_MAX_UCODE_BSS	(16)		/* Number of BSS handled in ucode bcn/prb */
#endif /* WLC_MAX_UCODE_BSS */
#define WLC_MAX_UCODE_BSS4	(4)		/* Number of BSS handled in sw bcn/prb */
#ifndef WLC_MAXBSSCFG
#ifdef PSTA
#define WLC_MAXBSSCFG		(1 + 50)		/* max # BSS configs */
#else /* PSTA */
#ifdef AP
#define WLC_MAXBSSCFG		(WLC_MAX_UCODE_BSS)	/* max # BSS configs */
#else
#define WLC_MAXBSSCFG		(1 )	/* max # BSS configs */
#endif /* AP */
#endif /* PSTA */
#endif /* WLC_MAXBSSCFG */

#ifndef MAXBSS
#define MAXBSS		128	/* max # available networks */
#endif /* MAXBSS */

#ifdef DONGLEBUILD
#define MAXUSCANBSS	16	/* reduce active limit for user scans */
#if (MAXBSS < MAXUSCANBSS)
#undef MAXUSCANBSS
#endif // endif
#endif /* DONGLEBUILD */
#ifndef MAXUSCANBSS
#define MAXUSCANBSS MAXBSS
#endif // endif

/* Max delay for timer interrupt. Platform specific and hence is a tunable */
#ifndef MCHAN_TIMER_DELAY_US
#define MCHAN_TIMER_DELAY_US 200
#endif /* MCHAN_TIMER_DELAY_US */

#ifndef WLC_DATAHIWAT
#define WLC_DATAHIWAT		50	/* data msg txq hiwat mark */
#endif /* WLC_DATAHIWAT */

#ifndef WLC_AMPDUDATAHIWAT
#define WLC_AMPDUDATAHIWAT	128	/* enough to cover tow full size aggr */
#endif /* WLC_AMPDUDATAHIWAT */

/* bounded rx loops */
#ifndef RXBND
#define RXBND		8	/* max # frames to process in wlc_recv() */
#endif	/* RXBND */

/* bounded rx loops during rsdb */
#ifndef RXBND_SMALL
#define RXBND_SMALL		RXBND	/* max # frames to process in wlc_recv() */
#endif	/* RXBND_SMALL */

#ifndef TXSBND
#define TXSBND		8	/* max # tx status to process in wlc_txstatus() */
#endif	/* TXSBND */

#ifndef PKTCBND
#define PKTCBND		8	/* max # frames to chain in wlc_recv() */
#endif	/* PKTCBND */

/* VHT 3x3 tunable value defaults */
#ifndef NTXD_AC3X3
#define NTXD_AC3X3		NTXD	/* TX descriptor ring */
#endif // endif

#ifndef NRXD_AC3X3
#define NRXD_AC3X3		NRXD	/* RX descriptor ring */
#endif // endif

#ifndef NTXD_LARGE_AC3X3
#define NTXD_LARGE_AC3X3	NTXD_LARGE
#endif // endif

#ifndef NRXD_LARGE_AC3X3
#define NRXD_LARGE_AC3X3	NRXD_LARGE
#endif // endif

#ifndef NRXBUFPOST_AC3X3
#define NRXBUFPOST_AC3X3	NRXBUFPOST	/* # rx buffers posted */
#endif // endif

#ifndef RXBND_AC3X3
#define RXBND_AC3X3		RXBND	/* max # rx frames to process */
#endif // endif

#ifndef PKTCBND_AC3X3
#define PKTCBND_AC3X3		PKTCBND	/* max # frames to chain in wlc_recv() */
#endif // endif

#ifndef CTFPOOLSZ_AC3X3
#define CTFPOOLSZ_AC3X3		CTFPOOLSZ	/* max buffers in ctfpool */
#endif // endif

/* HE 4x4 tunable value defaults */
#ifndef TXMR_HE
#define TXMR_HE			12
#endif // endif

#ifndef TXPREFCTL_HE
#define TXPREFCTL_HE		32
#endif // endif

#ifndef TXBURSTLEN_HE
#ifdef BCMQT
#define TXBURSTLEN_HE		256
#else
#define TXBURSTLEN_HE		1024
#endif /* BCMQT */
#endif /* TXBURSTLEN_HE */

#ifndef RXBURSTLEN_HE
#ifdef BCMQT
#define RXBURSTLEN_HE		128
#else
#define RXBURSTLEN_HE		256
#endif /* BCMQT */
#endif /* RXBURSTLEN_HE */

#ifndef MRRS_HE
#ifdef BCMQT
#define MRRS_HE			256
#else
#define MRRS_HE			1024
#endif /* BCMQT */
#endif /* MRRS_HE */

#ifdef BCMSUP_PSK
#define IDSUP_NOTIF_SRVR_OBJS	1
#define IDSUP_NOTIF_CLNT_OBJS	5
#else
#define IDSUP_NOTIF_SRVR_OBJS	0
#define IDSUP_NOTIF_CLNT_OBJS	0
#endif // endif

#if defined(WL_PROXDETECT) && defined(WL_FTM)
#define PROXD_NOTIF_SRVR_OBJS 1
#define PROXD_NOTIF_CLNT_OBJS 1
#else
#define PROXD_NOTIF_SRVR_OBJS 0
#define PROXD_NOTIF_CLNT_OBJS 0
#endif // endif

#ifdef WLRSDB
#define RSDB_NOTIF_SRVR_OBJS  2
#define RSDB_NOTIF_CLNT_OBJS  9
#else
#define RSDB_NOTIF_SRVR_OBJS  0
#define RSDB_NOTIF_CLNT_OBJS  0
#endif // endif

#ifdef NAN
#define NAN_NOTIF_SRVR_OBJS  2
#define NAN_NOTIF_CLNT_OBJS  0
#else
#define NAN_NOTIF_SRVR_OBJS  0
#define NAN_NOTIF_CLNT_OBJS  0
#endif // endif

#ifdef WL_MODESW
#define MODESW_NOTIF_SRVR_OBJS  1
#define MODESW_NOTIF_CLNT_OBJS  1
#else
#define MODESW_NOTIF_SRVR_OBJS  0
#define MODESW_NOTIF_CLNT_OBJS  0
#endif // endif

#ifdef WL_RANDMAC
#define RANDMAC_NOTIF_SRVR_OBJS  1
#define RANDMAC_NOTIF_CLNT_OBJS  0
#else
#define RANDMAC_NOTIF_SRVR_OBJS  0
#define RANDMAC_NOTIF_CLNT_OBJS  0
#endif // endif

#ifdef WL_SHIF
#define	SHUB_NOTIF_CLNT_OBJ	1
#else
#define	SHUB_NOTIF_CLNT_OBJ	0
#endif /* WL_SHIF */

#ifdef WLMCHAN
#define	MCHAN_NOTIF_CLNT_OBJ	3
#else
#define	MCHAN_NOTIF_CLNT_OBJ	0
#endif /* WL_SHIF */

#ifdef WLCSA
#define CSA_NOTIF_SRVR_OBJS	1
#define CSA_NOTIF_CLNT_OBJS	1  /* For now only AWDL,  add more in future if needed */
#else
#define CSA_NOTIF_SRVR_OBJS	0
#define CSA_NOTIF_CLNT_OBJS	0
#endif /* WLCSA */

#define SCB_NOTIF_CLNT_OBJ	1

#ifdef WLWNM
#define	WNM_NOTIF_CLNT_OBJ	2
#else
#define	WNM_NOTIF_CLNT_OBJ	0
#endif /* WLWNM */

/* Maximum number of notification servers. */
#ifndef MAX_NOTIF_SERVERS
#define MAX_NOTIF_SERVERS	(24 + \
	IDSUP_NOTIF_SRVR_OBJS + \
	RSDB_NOTIF_SRVR_OBJS + \
	NAN_NOTIF_SRVR_OBJS + \
	MODESW_NOTIF_SRVR_OBJS + \
	RANDMAC_NOTIF_SRVR_OBJS + \
	PROXD_NOTIF_SRVR_OBJS + \
	CSA_NOTIF_SRVR_OBJS)
#endif // endif

/* Maximum number of notification clients. */
#ifndef MAX_NOTIF_CLIENTS
#define MAX_NOTIF_CLIENTS	(56 + \
	IDSUP_NOTIF_CLNT_OBJS + \
	RSDB_NOTIF_CLNT_OBJS + \
	NAN_NOTIF_CLNT_OBJS + \
	MODESW_NOTIF_CLNT_OBJS + \
	RANDMAC_NOTIF_CLNT_OBJS + \
	PROXD_NOTIF_CLNT_OBJS + \
	SHUB_NOTIF_CLNT_OBJ + \
	MCHAN_NOTIF_CLNT_OBJ + \
	CSA_NOTIF_CLNT_OBJS + \
	SCB_NOTIF_CLNT_OBJ + \
	WNM_NOTIF_CLNT_OBJ)
#endif // endif

/* Maximum number of memory pools. */
#ifndef MAX_MEMPOOLS
#define MAX_MEMPOOLS	8
#endif /* MAX_MEMPOOLS */

/* Maximum # of IE build callbacks */
#ifndef MAXIEBUILDCBS
#define MAXIEBUILDCBS 176
#endif // endif

/* Maximum # of Vendor Specif IE build callbacks */
#ifndef MAXVSIEBUILDCBS
#define MAXVSIEBUILDCBS 80
#endif // endif

/* Maximum # of IE parse callbacks */
#ifndef MAXIEPARSECBS
#define MAXIEPARSECBS 135
#endif // endif

/* Maximum # of Vendor Specific IE parse callbacks */
#ifndef MAXVSIEPARSECBS
#define MAXVSIEPARSECBS 64
#endif // endif

/* Maximum # of IE registries */
#ifndef MAXIEREGS
#define MAXIEREGS 16
#endif // endif

#ifdef PROP_TXSTATUS
/* FIFO credit values given to host */
#ifndef WLFCFIFOCREDITAC0
#define WLFCFIFOCREDITAC0 2
#endif // endif

#ifndef WLFCFIFOCREDITAC1
#if defined(BCMTPOPT_TXMID)  /* memredux14 */
#define WLFCFIFOCREDITAC1 10
#elif defined(BCMTPOPT_TXHI) /* memredux16 */
#define WLFCFIFOCREDITAC1 12
#else  /* memredux or default */
#define WLFCFIFOCREDITAC1 8
#endif // endif
#endif /* WLFCFIFOCREDITAC1 */

#ifndef WLFCFIFOCREDITAC2
#define WLFCFIFOCREDITAC2 2
#endif // endif

#ifndef WLFCFIFOCREDITAC3
#define WLFCFIFOCREDITAC3 2
#endif // endif

#ifndef WLFCFIFOCREDITBCMC
#define WLFCFIFOCREDITBCMC 2
#endif // endif

#ifndef WLFCFIFOCREDITOTHER
#define WLFCFIFOCREDITOTHER 2
#endif // endif

#ifndef WLFC_INDICATION_TRIGGER
#define WLFC_INDICATION_TRIGGER 1	/* WLFC_CREDIT_TRIGGER or WLFC_TXSTATUS_TRIGGER */
#endif // endif

/* total credits to max pending credits ratio in borrow case */
#ifndef WLFC_FIFO_BO_CR_RATIO
#define WLFC_FIFO_BO_CR_RATIO 3 /* pending cr thresh = total_credits/WLFC_FIFO_BO_CR_RATIO */
#endif // endif

/* Pending Compressed Txstatus Thresholds */
#ifndef WLFC_COMP_TXSTATUS_THRESH
#define WLFC_COMP_TXSTATUS_THRESH 8
#endif // endif

/* FIFO Credit Pending Thresholds */
#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_BK
#define WLFC_FIFO_CR_PENDING_THRESH_AC_BK 2
#endif // endif

#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_BE
#define WLFC_FIFO_CR_PENDING_THRESH_AC_BE 4
#endif // endif

#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_VI
#define WLFC_FIFO_CR_PENDING_THRESH_AC_VI 3
#endif // endif

#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_VO
#define WLFC_FIFO_CR_PENDING_THRESH_AC_VO 2
#endif // endif

#ifndef WLFC_FIFO_CR_PENDING_THRESH_BCMC
#define WLFC_FIFO_CR_PENDING_THRESH_BCMC  2
#endif // endif
#endif /* PROP_TXSTATUS */

#if !defined(WLC_DISABLE_DFS_RADAR_SUPPORT)
/* Radar support */
#if defined(WL11H) && defined(BAND5G)
#define RADAR
#endif /* WL11H && BAND5G */
/* DFS */
#if defined(AP) && defined(RADAR)
#define WLDFS
#endif /* AP && RADAR */
#endif /* !WLC_DISABLE_DFS_RADAR_SUPPORT */

#if defined(RADAR) && !defined(BAND5G)
#ifndef RADAR_DISABLED
#define RADAR_DISABLED
#endif // endif
#endif /* RADAR && BAND5G */

#if defined(RADAR_DISABLED)
#define WLDFS_DISABLED
#endif // endif

#if defined(BAND5G)
#define BAND_5G(bt)	((bt) == WLC_BAND_5G)
#else
#define BAND_5G(bt)	((void)(bt), 0)
#endif // endif

#if defined(BAND2G)
#define BAND_2G(bt)	((bt) == WLC_BAND_2G)
#else
#define BAND_2G(bt)	((void)(bt), 0)
#endif // endif

/* Some phy initialization code/data can't be reclaimed in dualband mode */
#if defined(DBAND)
#define WLBANDINITDATA(_data)	_data
#define WLBANDINITFN(_fn)	_fn
#else
#define WLBANDINITDATA(_data)	BCMINITDATA(_data)
#define WLBANDINITFN(_fn)	BCMINITFN(_fn)
#endif // endif

/* FIPS support */
#ifdef WLFIPS
#define FIPS_ENAB(wlc) ((wlc)->cfg->wsec & FIPS_ENABLED)
#else
#define FIPS_ENAB(wlc) 0
#endif // endif

#ifdef WLPLT
	#if defined(ROM_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define WLPLT_ENAB(wlc_pub)     (wlc_pub->_plt)
	#elif defined(WLPLT_ENABLED)
		#define WLPLT_ENAB(wlc_pub)     1
	#else
		#define WLPLT_ENAB(wlc_pub)     0
	#endif
#else
	#define WLPLT_ENAB(wlc_pub)     0
#endif /* WLPLT */

#ifdef WLANTSEL
#define WLANTSEL_ENAB(wlc)	1
#else
#define WLANTSEL_ENAB(wlc)	0
#endif /* WLANTSEL */

#ifdef BCMWAPI_WPI
#define WAPI_HW_WPI_CAP(wlc)       D11REV_GE((wlc)->pub->corerev, 39)
#else
#define WAPI_HW_WPI_CAP(wlc)       0
#endif // endif

#if D11CONF_GE(40)
	#if ACCONF || ACCONF2 || ACCONF5
	#define D11AC_TXD
	#endif	/* ACCONF || ACCONF2 || ACCONF5 */
#define WLTOEHW		/* TOE should always be on for all chips with D11 coreref >= 40 */
#endif /* D11CONF >= 40 */

#ifdef BAND5G
#define WL_NUMCHANSPECS_5G_20	29
#define WL_NUMCHANSPECS_5G_2P5	WL_NUMCHANSPECS_5G_20
#define WL_NUMCHANSPECS_5G_5	WL_NUMCHANSPECS_5G_20
#define WL_NUMCHANSPECS_5G_10	WL_NUMCHANSPECS_5G_20
#ifdef WL11N
#define WL_NUMCHANSPECS_5G_40	24
#else
#define WL_NUMCHANSPECS_5G_40	0
#endif /* WL11N */
#if defined(WL11AC) || defined(WL11AX)
#define WL_NUMCHANSPECS_5G_80	24
#define WL_NUMCHANSPECS_5G_8080	96
#define WL_NUMCHANSPECS_5G_160  16
#else
#define WL_NUMCHANSPECS_5G_80	0
#define WL_NUMCHANSPECS_5G_8080	0
#define WL_NUMCHANSPECS_5G_160	0
#endif /* WL11AC || WL11AX */
#else
#define WL_NUMCHANSPECS_5G_20	0
#define WL_NUMCHANSPECS_5G_2P5	0
#define WL_NUMCHANSPECS_5G_5	0
#define WL_NUMCHANSPECS_5G_10	0
#define WL_NUMCHANSPECS_5G_40	0
#define WL_NUMCHANSPECS_5G_80	0
#define WL_NUMCHANSPECS_5G_8080	0
#define WL_NUMCHANSPECS_5G_160	0
#endif /* band5g */

#ifdef BAND2G
#define WL_NUMCHANSPECS_2G_20	14
#define WL_NUMCHANSPECS_2G_2P5	WL_NUMCHANSPECS_2G_20
#define WL_NUMCHANSPECS_2G_5	WL_NUMCHANSPECS_2G_20
#define WL_NUMCHANSPECS_2G_10	WL_NUMCHANSPECS_2G_20
#ifdef WL11N
#define WL_NUMCHANSPECS_2G_40	18
#else
#define WL_NUMCHANSPECS_2G_40	0
#endif /* WL11N */
#else
#define WL_NUMCHANSPECS_2G_20	0
#define WL_NUMCHANSPECS_2G_2P5	0
#define WL_NUMCHANSPECS_2G_5	0
#define WL_NUMCHANSPECS_2G_10	0
#define WL_NUMCHANSPECS_2G_40	0
#endif /* band 2g */
#define WL_NUMCHANSPECS_2G (WL_NUMCHANSPECS_2G_20 + WL_NUMCHANSPECS_2G_40)

#define WL_NUMCHANSPECS_5G (WL_NUMCHANSPECS_5G_20 + WL_NUMCHANSPECS_5G_40 +\
		WL_NUMCHANSPECS_5G_80)

/* Wave2 devices */
#define IS_AC2_DEV(d) (((d) == BCM4366_D11AC_ID) || \
	                 ((d) == BCM4366_D11AC2G_ID) || \
	                 ((d) == BCM4366_D11AC5G_ID) || \
	                 ((d) == BCM4365_D11AC_ID) || \
	                 ((d) == BCM4365_D11AC2G_ID) || \
	                 ((d) == BCM4365_D11AC5G_ID))

#define IS_DEV_AC3X3(d) (((d) == BCM4360_D11AC_ID) || \
	                 ((d) == BCM4360_D11AC2G_ID) || \
	                 ((d) == BCM4360_D11AC5G_ID) || \
	                 ((d) == BCM43602_D11AC_ID) || \
	                 ((d) == BCM43602_D11AC2G_ID) || \
	                 ((d) == BCM43602_D11AC5G_ID))

#define IS_DEV_AC2X2(d) (((d) == BCM4352_D11AC_ID) ||	\
	                 ((d) == BCM4352_D11AC2G_ID) || \
	                 ((d) == BCM4352_D11AC5G_ID) || \
	                 ((d) == BCM4350_D11AC_ID) || \
	                 ((d) == BCM4350_D11AC2G_ID) || \
	                 ((d) == BCM4350_D11AC5G_ID) || \
			 ((d) == BCM53573_D11AC_ID) || \
			 ((d) == BCM53573_D11AC2G_ID) || \
			 ((d) == BCM53573_D11AC5G_ID) || \
			 ((d) == BCM47189_D11AC_ID) || \
			 ((d) == BCM47189_D11AC2G_ID) || \
			 ((d) == BCM47189_D11AC5G_ID) || \
	                 ((d) == BCM6878_D11AC_ID) || \
	                 ((d) == BCM6878_D11AC2G_ID) || \
	                 ((d) == BCM6878_D11AC5G_ID))

/* HE devices */
#define IS_HE_DEV(d) (((d) == BCM43684_CHIP_ID) || \
		      ((d) == BCM43684_D11AX_ID) || \
		      ((d) == BCM43684_D11AX2G_ID) || \
		      ((d) == BCM43684_D11AX5G_ID) || \
		      ((d) == EMBEDDED_2x2AX_ID) || \
		      ((d) == EMBEDDED_2x2AX_DEV2G_ID) || \
		      ((d) == EMBEDDED_2x2AX_DEV5G_ID) || \
		      ((d) == BCM6710_CHIP_ID) || \
		      ((d) == BCM6710_D11AX_ID) || \
		      ((d) == BCM6710_D11AX2G_ID) || \
		      ((d) == BCM6710_D11AX5G_ID))

/* Airtime fairness */
#ifdef WLATF
#ifndef WLC_ATF_ENABLE_DEFAULT
#define WLC_ATF_ENABLE_DEFAULT		0
#endif /* WLC_ATF_ENABLE_DEFAULT */
#endif /* WLATF */
#ifndef NTXD_LFRAG
#define NTXD_LFRAG			1024
#endif // endif
#ifndef NRXD_FIFO1
#define NRXD_FIFO1			32
#endif // endif
#ifndef NRXD_CLASSIFIED_FIFO
#define NRXD_CLASSIFIED_FIFO		32
#endif // endif

#ifndef NRXBUFPOST_CLASSIFIED_FIFO
#define NRXBUFPOST_CLASSIFIED_FIFO	6
#endif // endif

#ifndef NRXBUFPOST_CLASSIFIED_FIFO_FRWD
#define NRXBUFPOST_CLASSIFIED_FIFO_FRWD 12
#endif // endif

#ifndef NRXBUFPOST_FIFO1
#define NRXBUFPOST_FIFO1		6
#endif // endif

#ifndef	NRXBUFPOST_FIFO2
#define NRXBUFPOST_FIFO2		6
#endif // endif
#ifndef PKT_CLASSIFY_FIFO
#define PKT_CLASSIFY_FIFO		2
#endif // endif

#ifndef SPLIT_RXMODE
#define SPLIT_RXMODE			0
#endif // endif
#ifndef COPY_CNT_BYTES
#define COPY_CNT_BYTES			64
#endif /* ! COPY_CNT_BYTES */

#ifndef WLC_DEFAULT_SETTLE_TIME
#define WLC_DEFAULT_SETTLE_TIME		3	/* default settle time after a scan/roam */
#endif /* WLC_DEFAULT_SETTLE_TIME */

#ifndef MIN_SCBALLOC_MEM
#define MIN_SCBALLOC_MEM		(16*1024)
#endif // endif

#ifdef BCMRESVFRAGPOOL
#ifndef RPOOL_DISABLED_AMPDU_MPDU
#define RPOOL_DISABLED_AMPDU_MPDU	24
#endif // endif
#endif /* BCMRESVFRAGPOOL */

#if !defined(DONGLEBUILD)	/* More control for debug on dongle */
/* Derive WL_MACDBG */
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP) || \
	defined(WLTEST) || defined(TDLS_TESTBED) || defined(BCMDBG_AMPDU) || \
	defined(BCMDBG_TXBF) || defined(BCMDBG_DUMP_RSSI) || defined(MCHAN_MINIDUMP)
#define WL_MACDBG
#else
#undef WL_MACDBG
#endif // endif
#endif /* DONGLEBUILD */

#if defined(WL_DATAPATH_HC)
/* DATAPATH_LOG_DUMP is compiled by default for Datapath HC
 * if Event Log is available. The HC information is reported
 * by way of Event Log, and extra Datapath information is
 * provided by WL_DATAPATH_LOG_DUMP.
 * Non-EventLog builds use standard printf sytle dumps for
 * datapath information.
 */
#if defined(EVENT_LOG_COMPILE)
#  define WL_DATAPATH_LOG_DUMP
#endif // endif
#endif /* WL_DATAPATH_HC */

/* Define WL_RX_HANDLER if either of the RX health check features are defined */
#if defined(WL_RX_STALL) || defined(WL_RX_DMA_STALL_CHECK)
#define WL_RX_HANDLER
#endif // endif

/* Add all common datapath healthcheck code under one common definition. */
#if defined(WL_RX_HANDLER) || defined(WL_TX_STALL) || defined(WL_SCAN_STALL_CHECK)
#define WL_DD_HANDLER
#endif /* WL_RX_HANDLER || WL_TX_STALL || WL_SCAN_STALL_CHECK */

/*
 * ucode TxStatus logging
 */

#if !defined(TXS_LOG_DISABLED)

/* WL_TXS_LOG is compiled by default */
#define WL_TXS_LOG

/* tunable history length */
#ifndef WLC_UC_TXS_HIST_LEN
#define WLC_UC_TXS_HIST_LEN 10
#endif // endif

/* configurable timestap option - off by default to save memory */
/* WLC_UC_TXS_TIMESTAMP */

#endif /* !TXS_LOG_DISABLED */

/* check required support for Rx DMA Stall check */
#if defined(WL_RX_DMA_STALL_CHECK)
#  if !defined(WLCNT)
#    error "WL_RX_DMA_STALL_CHECK requires WLCNT"
#  endif
#endif /* WL_RX_DMA_STALL_CHECK */

#if defined(WL_DATAPATH_LOG_DUMP) && !defined(EVENT_LOG_COMPILE)
#error "Need EVENT_LOG_COMPILE for WL_DATAPATH_LOG_DUMP"
#endif // endif

#ifdef USE_RSDBAUXCORE_TUNE
#ifndef WL_NRXD_AUX
#define WL_NRXD_AUX WL_NRXD
#endif /* WL_NRXD_AUX */
#ifndef NRXBUFPOST_AUX
#define NRXBUFPOST_AUX	NRXBUFPOST
#endif /* NRXBUFPOST_AUX */
#ifndef WL_RXBND_AUX
#define WL_RXBND_AUX WL_RXBND
#endif /* WL_RXBND_AUX */
#ifndef NTXD_LFRAG_AUX
#define NTXD_LFRAG_AUX NTXD_LFRAG
#endif /* NTXD_LFRAG_AUX */
#endif /* USE_RSDBAUXCORE_TUNE */

#ifndef MAX_PFN_LIST_COUNT
#define MAX_PFN_LIST_COUNT 64
#endif /* MAX_PFN_LIST_COUNT */

#define WLAUTOD11REGS 1

#if defined(WL_EAP_BOARD_RF_5G_FILTER)
/* Generic board defines for 5G analog band-pass filtering.
 * Introduced for the BCM949408EAP's simultaneous dual-port 5G operation.
 */
#define BOARD_5G_FILTER_ABSENT		(-1) /* doesn't exist */
#define BOARD_5G_FILTER_BLOCKS_NONE	(0)
/* Keep following bit-defined */
#define BOARD_5G_FILTER_BLOCKS_UNII1	(1 << 0)
#define BOARD_5G_FILTER_BLOCKS_UNII2A	(1 << 1)
#define BOARD_5G_FILTER_BLOCKS_UNII2C	(1 << 2)
#define BOARD_5G_FILTER_BLOCKS_UNII3	(1 << 3)
#endif /* WL_EAP_BOARD_RF_5G_FILTER */

/* Check required support for HWA */
#if defined(BCMHWA) && defined(HWA_PKT_MACRO)
#if !defined(BCMLFRAG)
#error "HWA depends on BCMLFRAG"
#elif !defined(PKTC_TX_DONGLE)
#error "HWA depends on PKTC_TX_DONGLE"
#endif /* !defined (BCMLFRAG) */
#endif /* BCMHWA */

#ifndef MAXMACLIST
#define MAXMACLIST		64	/* max # source MAC matches */
#endif /* !MAXMACLIST */

#endif /* _wlc_cfg_h_ */
