/*
 * Common (OS-independent) portion of
 * Broadcom 802.11bang Networking Device Driver
 *
 * BMAC driver external interface
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
 * $Id: wlc_bmac.h 708276 2017-06-30 11:38:32Z $
 */

/* XXXXX this interface is under wlc.c by design
 * http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/WlBmacDesign
 *
 *        high driver files(e.g. wlc_ampdu.c wlc_ccx.c etc)
 *             wlc.h/wlc.c
 *         wlc_bmac.h/wlc_bmac.c
 *
 *  So don't include this in files other than wlc.c, wlc_bmac* wl_rte.c(dongle port) and wl_phy.c
 *  create wrappers in wlc.c if needed
 */

#ifndef _WLC_BMAC_H_
#define _WLC_BMAC_H_

#define WOWL_GPIO_INVALID_VALUE	0xFF

#define PHY_TXC_FRAMETYPE(r)	(RSPEC_ISVHT(r) ? FT_VHT : \
				(RSPEC_ISHT(r) ? FT_HT : \
				(IS_CCK(r) ? FT_CCK : FT_OFDM)))

/* BCMECICOEX support */
#ifdef BCMECICOEX

#define BCMECICOEX_ENAB(wlc) \
	(D11REV_GE(((wlc_info_t*)(wlc))->pub->corerev, 15) && \
	(((wlc_info_t*)(wlc))->pub->sih->cccaps & CC_CAP_ECI) && \
	(((wlc_info_t*)(wlc))->machwcap & MCAP_BTCX))
/* ECI combo chip */
#define BCMECICOEX_ENAB_BMAC(wlc_hw) \
	(D11REV_GE(((wlc_hw_info_t*)(wlc_hw))->corerev, 15) && \
	(((wlc_hw_info_t*)(wlc_hw))->boardflags & BFL_BTC2WIRE) && \
	!(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTCLEGACY) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps & CC_CAP_ECI) && \
	!(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_SECI_PRESENT) && \
	(((wlc_hw_info_t*)(wlc_hw))->machwcap & MCAP_BTCX))
/* SECI combo board (SECI enabled) */
#define BCMSECICOEX_ENAB_BMAC(wlc_hw) \
	((((wlc_hw_info_t*)(wlc_hw))->boardflags & BFL_BTC2WIRE) && \
	!(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTCLEGACY) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_SECI_PRESENT) && \
	(((wlc_hw_info_t*)(wlc_hw))->machwcap & MCAP_BTCX))
/* GCI combo chip */
#define BCMGCICOEX_ENAB_BMAC(wlc_hw) \
	((((wlc_hw_info_t*)(wlc_hw))->boardflags & BFL_BTCOEX) && \
	!(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTCLEGACY) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT) && \
	(((wlc_hw_info_t*)(wlc_hw))->machwcap & MCAP_BTCX))
/* either ECI combo chip, SECI combo board(SECI enabled) or GCI combo chip */
#define BCMCOEX_ENAB_BMAC(wlc_hw) \
	(BCMECICOEX_ENAB_BMAC(wlc_hw) || BCMSECICOEX_ENAB_BMAC(wlc_hw) || \
		BCMGCICOEX_ENAB_BMAC(wlc_hw))

#define NOTIFY_BT_CHL(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_CHANNEL_MASK(sih->ccrev), \
		((val) << ECI_OUT_CHANNEL_SHIFT(sih->ccrev)), TRUE)
#define NOTIFY_BT_BW_20(sih) \
	si_eci_notify_bt((sih), ECI_OUT_BW_MASK(sih->ccrev), \
		((ECI_BW_25) << ECI_OUT_BW_SHIFT(sih->ccrev)), TRUE)
#define NOTIFY_BT_BW_40(sih) \
	si_eci_notify_bt((sih), ECI_OUT_BW_MASK(sih->ccrev), \
		((ECI_BW_45) << ECI_OUT_BW_SHIFT(sih->ccrev)), TRUE)
#define NOTIFY_BT_NUM_ANTENNA(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_ANTENNA_MASK(sih->ccrev), \
		((val) << ECI_OUT_ANTENNA_SHIFT(sih->ccrev)), TRUE)
#define NOTIFY_BT_TXRX(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_SIMUL_TXRX_MASK(sih->ccrev), \
		((val) << ECI_OUT_SIMUL_TXRX_SHIFT(sih->ccrev), TRUE))

#else /* BCMECICOEX */

#define BCMECICOEX_ENAB(wlc)            (0)
#define BCMECICOEX_ENAB_BMAC(wlc_hw)	(0)
#define BCMSECICOEX_ENAB_BMAC(wlc_hw)	(0)
#define BCMGCICOEX_ENAB_BMAC(wlc_hw)	(0)
#define BCMCOEX_ENAB_BMAC(wlc_hw) 	(0)
#define NOTIFY_BT_CHL(sih, val) {}
#define NOTIFY_BT_BW_20(sih) {}
#define NOTIFY_BT_BW_40(sih) {}
#define NOTIFY_BT_NUM_ANTENNA(sih, val) {}
#define NOTIFY_BT_TXRX(sih, val) {}

#endif /* BCMECICOEX */

extern void wlc_bmac_update_bt_chanspec(wlc_hw_info_t *wlc_hw,
	chanspec_t chanspec, bool scan_in_progress, bool roam_in_progress);

/* Revision and other info required from BMAC driver for functioning of high ONLY driver */
typedef struct wlc_bmac_revinfo {
	uint		vendorid;	/* PCI vendor id */
	uint		deviceid;	/* device id of chip */

	uint		boardrev;	/* version # of particular board */
	uint		corerev;	/* core revision */
	uint		sromrev;	/* srom revision */
	uint		chiprev;	/* chip revision */
	uint		chip;		/* chip number */
	uint		chippkg;	/* chip package */
	uint		boardtype;	/* board type */
	uint		boardvendor;	/* board vendor */
	uint		bustype;	/* SB_BUS, PCI_BUS  */
	uint		buscoretype;	/* PCI_CORE_ID, PCIE_CORE_ID, PCMCIA_CORE_ID */
	uint		buscorerev; 	/* buscore rev */
	uint32		issim;		/* chip is in simulation or emulation */

	uint		nbands;
    uint        boardflags; /* boardflags */
    uint        boardflags2; /* boardflags2 */

	struct band_info {
		uint bandunit; /* To match on both sides */
		uint bandtype; /* To match on both sides */
		uint radiorev;
		uint phytype;
		uint phyrev;
		uint anarev;
		uint radioid;
		bool abgphy_encore;
	} band[MAXBANDS];

	/* put flags that advertize BMAC capabilities to high mac here  */
	uint32 _wlsrvsdb;			/* 'true' if bmac is capable of srvsdb operation */
	uint		ampdu_ba_rx_wsize;
	uint		host_rpc_agg_size;
	uint		ampdu_mpdu;
	bool        is_ss; /* bus support for super speed */

    uint32      wowl_gpio;
    uint32      wowl_gpiopol;
    uint        boardflags4;    /* boardflags4 */
} wlc_bmac_revinfo_t;

/* dup state between BMAC(wlc_hw_info_t) and HIGH(wlc_info_t) driver */
typedef struct wlc_bmac_state {
	uint32		machwcap;	/* mac hw capibility */
	uint32		preamble_ovr;	/* preamble override */
} wlc_bmac_state_t;

enum {
	IOV_BMAC_DIAG,
	IOV_BMAC_SBGPIOTIMERVAL,
	IOV_BMAC_SBGPIOOUT,
	IOV_BMAC_CCGPIOCTRL,		/* CC GPIOCTRL REG */
	IOV_BMAC_CCGPIOOUT,		/* CC GPIOOUT REG */
	IOV_BMAC_CCGPIOOUTEN,	/* CC GPIOOUTEN REG */
	IOV_BMAC_CCGPIOIN,		/* CC GPIOIN REG */
	IOV_BMAC_WPSGPIO,		/* WPS push button GPIO pin */
	IOV_BMAC_OTPDUMP,
	IOV_BMAC_OTPSTAT,
	IOV_BMAC_PCIEASPM,		/* obfuscation clkreq/aspm control */
	IOV_BMAC_PCIEADVCORRMASK,	/* advanced correctable error mask */
	IOV_BMAC_PCIECLKREQ,        /* PCIE 1.1 clockreq enab support */
	IOV_BMAC_PCIELCREG,         /* PCIE LCREG */
	IOV_BMAC_SBGPIOTIMERMASK,
	IOV_BMAC_RFDISABLEDLY,
	IOV_BMAC_PCIEREG,		/* PCIE REG */
	IOV_BMAC_PCICFGREG,		/* PCI Config register */
	IOV_BMAC_PLLRESET,	/* PLL reset */
	IOV_BMAC_PCIESERDESREG,	/* PCIE SERDES REG (dev, 0}offset) */
	IOV_BMAC_PCIEGPIOOUT,	/* PCIEOUT REG */
	IOV_BMAC_PCIEGPIOOUTEN,	/* PCIEOUTEN REG */
	IOV_BMAC_PCIECLKREQENCTRL,	/* clkreqenctrl REG (PCIE REV > 6.0 */
	IOV_BMAC_DMALPBK,
	IOV_BMAC_CCREG,
	IOV_BMAC_COREREG,
	IOV_BMAC_SFLAGS,
	IOV_BMAC_CFLAGS,
	IOV_BMAC_JTAGUREG,
	IOV_BMAC_COMA,
	IOV_BMAC_SDCIS,
	IOV_BMAC_SDIO_DRIVE,
	IOV_BMAC_OTPW,
	IOV_BMAC_NVOTPW,
	IOV_BMAC_SROM,
	IOV_BMAC_SRCRC,
	IOV_BMAC_DEVPATH,
	IOV_BMAC_CIS_SOURCE,
	IOV_BMAC_CISVAR,
	IOV_BMAC_OTPLOCK,
	IOV_BMAC_OTP_CHIPID,
	IOV_BMAC_CUSTOMVAR1,
	IOV_BMAC_BOARDFLAGS,
	IOV_BMAC_BOARDFLAGS2,
	IOV_BMAC_WPSLED,
	IOV_BMAC_BTCLOCK_TUNE_WAR,
	IOV_BMAC_NVRAM_SOURCE,
	IOV_BMAC_OTP_RAW_READ,
	IOV_BMAC_GENERIC_DLOAD,
	IOV_BMAC_UCDLOAD_STATUS,
	IOV_BMAC_UC_CHUNK_LEN,
	IOV_BMAC_NOISE_METRIC,
	IOV_BMAC_AVIODCNT,
	IOV_BMAC_FILT_WAR,
#if defined(BCMDBG_SUSPEND_MAC)
	IOV_BMAC_SUSPEND_MAC,
#endif // endif
	IOV_BMAC_RCVLAZY,
	IOV_BMAC_BTSWITCH,
	IOV_BMAC_PCIESSID,
	IOV_BMAC_PCIEBAR0,
	IOV_BMAC_OTP_RAW,
	IOV_BMAC_4360_PCIE2_WAR,
	IOV_BMAC_BMC_NBUFS,
	IOV_BMAC_EDCRS,
	IOV_BMAC_COREWRAPPERREG,
	IOV_BMAC_LEDDC,
	IOV_BMAC_SR_VERIFY,
	IOV_BMAC_PLLTEST_DELAY,
	IOV_BMAC_PLLTEST_OFFDELAY,
	IOV_BMAC_PLLTEST_CONFIG,
	IOV_BMAC_PLLTEST,
	IOV_BMAC_BT_REGS_READ,
	IOV_BMAC_PCIE_BUS_TPUT,
	IOV_BMAC_LAST
};

typedef enum {
	BMAC_DUMP_GPIO_ID,
	BMAC_DUMP_SI_ID,
	BMAC_DUMP_SIREG_ID,
	BMAC_DUMP_SICLK_ID,
	BMAC_DUMP_CCREG_ID,
	BMAC_DUMP_PCIEREG_ID,
	BMAC_DUMP_PHYREG_ID,
	BMAC_DUMP_PHYTBL_ID,
	BMAC_DUMP_PHYTBL2_ID,
	BMAC_DUMP_PHY_RADIOREG_ID,
	BMAC_DUMP_PHY_ACI_ID,
	BMAC_DUMP_PHY_PHYCAL_ID,
	BMAC_DUMP_PHY_PAPD_ID,
	BMAC_DUMP_PHY_NOISE_ID,
	BMAC_DUMP_PHY_STATE_ID,
	BMAC_DUMP_PHY_MEASLO_ID,
	BMAC_DUMP_PHY_LNAGAIN_ID,
	BMAC_DUMP_PHY_INITGAIN_ID,
	BMAC_DUMP_PHY_HPF1TBL_ID,
	BMAC_DUMP_PHY_LPPHYTBL0_ID,
	BMAC_DUMP_PHY_CHANEST_ID,
	BMAC_DUMP_BTC_ID,
	BMAC_DUMP_BMC_ID,
	BMAC_DUMP_SUSPEND_ID,
#ifdef ENABLE_FCBS
	BMAC_DUMP_PHY_FCBS_ID,
#endif /* ENABLE_FCBS */
	BMAC_DUMP_PCIEINFO_ID,
	BMAC_DUMP_PHY_TXV0_ID,
#ifdef WLTEST
	BMAC_DUMP_PHY_CH4RPCAL_ID,
#endif /* WLTEST */
	BMAC_DUMP_LAST
} wlc_bmac_dump_id_t;

typedef enum {
	WLCHW_STATE_ATTACH,
	WLCHW_STATE_CLK,
	WLCHW_STATE_UP,
	WLCHW_STATE_ASSOC,
	WLCHW_STATE_LAST
} wlc_bmac_state_id_t;

/* CCA and OBSS stats */
typedef struct {
	uint32 txdur;	/* num usecs tx'ing */
	uint32 ibss;	/* num usecs rx'ing cur bss */
	uint32 obss;	/* num usecs rx'ing other data */
	uint32 noctg;	/* 802.11 of unknown type */
	uint32 nopkt;	/* non 802.11 */
	uint32 usecs;	/* usecs in this interval */
	uint32 PM;	/* usecs MAC spent in doze mode for PM */

	uint32 crsglitch;	/* num rxcrsglitchs */
	uint32 badplcp;	/* num bad plcp */
	uint32 bphy_crsglitch;  /* num bphy rxcrsglitchs */
	uint32 bphy_badplcp;    /* num bphy bad plcp */

	uint32 txopp; /* Slot counter */
	uint32 gdtxdur; /* Good Tx usecs */
	uint32 bdtxdur; /* Bad Tx usecs */
	uint32 slot_time_txop; /* Slot Time For txopp conversion */

	uint32 rxdrop20s;		/* rx sec dropped */
	uint32 rx20s;			/* rx sec pkts */
	uint32 sec_rssi_hist_hi;	/* rx sec rssi high histogram */
	uint32 sec_rssi_hist_med;	/* rx sec rssi medium histogram */
	uint32 sec_rssi_hist_low;	/* rx sec rssi low histogram */
	uint32 rxcrs_pri;		/* rx crs primary */
	uint32 rxcrs_sec20;		/* rx crs secondary 20 */
	uint32 rxcrs_sec40;		/* rx crs secondary 40 */
} wlc_bmac_obss_counts_t;

typedef struct wlc_stf_rs_shm_offset {
	uint8 rate[WLC_NUMRATES];
	uint16 val[WLC_NUMRATES];
} wlc_stf_rs_shm_offset_t;

/* XXX ALERT: for any new BMAC module external API, stub functions are required in
 *  wlc_bmac_stubs.c/wlc_high_stubs.c to support BMAC driver
 *  refer to wlc_bmac_up_prep/WLRPC_WLC_BMAC_UP_PREP_ID
 */
extern int wlc_bmac_attach(wlc_info_t *wlc, uint16 vendor, uint16 device, uint unit,
	bool piomode, osl_t *osh, void *regsva, uint bustype, void *btparam);
extern int wlc_bmac_detach(wlc_info_t *wlc);

extern si_t * wlc_bmac_si_attach(uint device, osl_t *osh, void *regs, uint bustype,
	void *btparam, char **vars, uint *varsz);
extern void wlc_bmac_si_detach(osl_t *osh, si_t *sih);

extern void wlc_bmac_watchdog(void *arg);
#ifndef BCM_OL_DEV
extern void wlc_bmac_rpc_agg_watchdog(void *arg);
#endif /* BCM_OL_DEV */
extern void wlc_bmac_info_init(wlc_hw_info_t *wlc_hw);

/* up/down, reset, clk */
#ifdef WLC_LOW
extern void wlc_bmac_xtal(wlc_hw_info_t* wlc_hw, bool want);
#endif // endif

extern void wlc_bmac_copyto_objmem(wlc_hw_info_t *wlc_hw,
                                   uint offset, const void* buf, int len, uint32 sel);
extern void wlc_bmac_copyfrom_objmem(wlc_hw_info_t *wlc_hw,
                                     uint offset, void* buf, int len, uint32 sel);
#define wlc_bmac_copyfrom_shm(wlc_hw, offset, buf, len)                 \
	wlc_bmac_copyfrom_objmem(wlc_hw, offset, buf, len, OBJADDR_SHM_SEL)
#define wlc_bmac_copyto_shm(wlc_hw, offset, buf, len)                   \
	wlc_bmac_copyto_objmem(wlc_hw, offset, buf, len, OBJADDR_SHM_SEL)

extern void wlc_bmac_core_phy_clk(wlc_hw_info_t *wlc_hw, bool clk);
extern void wlc_bmac_core_phypll_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_core_phypll_ctl(wlc_hw_info_t* wlc_hw, bool on);
extern void wlc_bmac_phyclk_fgc(wlc_hw_info_t *wlc_hw, bool clk);
extern void wlc_bmac_macphyclk_set(wlc_hw_info_t *wlc_hw, bool clk);
extern void wlc_bmac_phy_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_bw_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_corereset(wlc_hw_info_t *wlc_hw, uint32 flags);
extern void wlc_bmac_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_init(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute,
	uint32 defmacintmask);
extern int wlc_bmac_up_prep(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_up_finish(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_set_ctrl_ePA(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_set_ctrl_SROM(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_set_ctrl_bt_shd0(wlc_hw_info_t *wlc_hw, bool enable);
extern int wlc_bmac_set_btswitch(wlc_hw_info_t *wlc_hw, int8 state);
#ifdef BCMNODOWN
#define wlc_bmac_down_prep(wlc_hw) (BCME_OK)
#define wlc_bmac_down_finish(wlc_hw) (BCME_OK)
#else
extern int wlc_bmac_down_prep(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_down_finish(wlc_hw_info_t *wlc_hw);
#endif // endif
extern void wlc_bmac_corereset(wlc_hw_info_t *wlc_hw, uint32 flags);
extern void wlc_bmac_switch_macfreq(wlc_hw_info_t *wlc_hw, uint8 spurmode);
extern int wlc_bmac_4331_epa_init(wlc_hw_info_t *wlc_hw);

/* chanspec, ucode interface */
extern int wlc_bmac_bandtype(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_set_chanspec(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute,
	ppr_t *txpwr);

extern void wlc_bmac_txfifo(wlc_hw_info_t *wlc_hw, uint fifo, void *p, bool commit, uint16 frameid,
	uint8 txpktpend);
extern int wlc_bmac_xmtfifo_sz_get(wlc_hw_info_t *wlc_hw, uint fifo, uint *blocks);
#ifdef PHYCAL_CACHING
extern void wlc_bmac_set_phycal_cache_flag(wlc_hw_info_t *wlc_hw, bool state);
extern bool wlc_bmac_get_phycal_cache_flag(wlc_hw_info_t *wlc_hw);
#endif // endif
extern void wlc_bmac_phy_txpwr_cache_invalidate(wlc_hw_info_t *wlc_hw);

extern void wlc_bmac_mhf(wlc_hw_info_t *wlc_hw, uint8 idx, uint16 mask, uint16 val, int bands);
extern void wlc_bmac_mctrl(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val);
extern uint16 wlc_bmac_mhf_get(wlc_hw_info_t *wlc_hw, uint8 idx, int bands);
extern int  wlc_bmac_xmtfifo_sz_set(wlc_hw_info_t *wlc_hw, uint fifo, uint16 blocks);
extern void wlc_bmac_txant_set(wlc_hw_info_t *wlc_hw, uint16 phytxant);
extern uint16 wlc_bmac_get_txant(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_antsel_type_set(wlc_hw_info_t *wlc_hw, uint8 antsel_type);
extern int  wlc_bmac_revinfo_get(wlc_hw_info_t *wlc_hw, wlc_bmac_revinfo_t *revinfo);
extern int  wlc_bmac_state_get(wlc_hw_info_t *wlc_hw, wlc_bmac_state_t *state);
extern void wlc_bmac_write_shm(wlc_hw_info_t *wlc_hw, uint offset, uint16 v);
extern void wlc_bmac_update_shm(wlc_hw_info_t *wlc_hw, uint offset, uint16 v, uint16 mask);
extern uint16 wlc_bmac_read_shm(wlc_hw_info_t *wlc_hw, uint offset);
extern void wlc_bmac_set_shm(wlc_hw_info_t *wlc_hw, uint offset, uint16 v, int len);
extern void wlc_bmac_write_template_ram(wlc_hw_info_t *wlc_hw, int offset, int len, void *buf);
extern void wlc_bmac_templateptr_wreg(wlc_hw_info_t *wlc_hw, int offset);
extern uint32 wlc_bmac_templateptr_rreg(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_templatedata_wreg(wlc_hw_info_t *wlc_hw, uint32 word);
extern uint32 wlc_bmac_templatedata_rreg(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_copyfrom_vars(wlc_hw_info_t *wlc_hw, char ** buf, uint *len);
extern void wlc_bmac_enable_mac(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_suspend_mac_and_wait(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_ampdu_set(wlc_hw_info_t *wlc_hw, uint8 mode);

extern void wlc_bmac_process_ps_switch(wlc_hw_info_t *wlc, struct ether_addr *ea, int8 ps_on);
extern void wlc_bmac_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea);
extern void wlc_bmac_set_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea);
extern bool wlc_bmac_validate_chip_access(wlc_hw_info_t *wlc_hw);

extern bool wlc_bmac_radio_read_hwdisabled(wlc_hw_info_t* wlc_hw);
extern void wlc_bmac_set_shortslot(wlc_hw_info_t *wlc_hw, bool shortslot);
extern void wlc_bmac_mute(wlc_hw_info_t *wlc_hw, bool want, mbool flags);
extern void wlc_bmac_set_deaf(wlc_hw_info_t *wlc_hw, bool user_flag);
#if defined(WLTEST)
extern void wlc_bmac_clear_deaf(wlc_hw_info_t *wlc_hw, bool user_flag);
#endif // endif
#ifdef WL11N
extern void wlc_bmac_band_stf_ss_set(wlc_hw_info_t *wlc_hw, uint8 stf_mode);
extern void wlc_bmac_txbw_update(wlc_hw_info_t *wlc_hw);
#endif // endif
extern void wlc_bmac_filter_war_upd(wlc_hw_info_t *wlc_hw, bool set);

extern int wlc_bmac_btc_mode_set(wlc_hw_info_t *wlc_hw, int btc_mode);
extern int wlc_bmac_btc_mode_get(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_wire_set(wlc_hw_info_t *wlc_hw, int btc_wire);
extern int wlc_bmac_btc_wire_get(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_flags_idx_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2);
extern int wlc_bmac_btc_flags_idx_get(wlc_hw_info_t *wlc_hw, int val);
extern int wlc_bmac_btc_flags_get(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_params_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2);
extern int wlc_bmac_btc_params_get(wlc_hw_info_t *wlc_hw, int int_val);
extern void wlc_bmac_btc_params_save(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_period_get(wlc_hw_info_t *wlc_hw, uint16 *btperiod, bool *btactive,
	uint16 *agg_off_bm);
extern void wlc_bmac_btc_rssi_threshold_get(wlc_hw_info_t *wlc_hw, uint8 *, uint8 *, uint8 *);
extern void wlc_bmac_btc_stuck_war50943(wlc_hw_info_t *wlc_hw, bool enable);
extern void wlc_bmac_wait_for_wake(wlc_hw_info_t *wlc_hw);
extern bool wlc_bmac_tx_fifo_suspended(wlc_hw_info_t *wlc_hw, uint tx_fifo);
extern void wlc_bmac_tx_fifo_suspend(wlc_hw_info_t *wlc_hw, uint tx_fifo);
extern void wlc_bmac_tx_fifo_resume(wlc_hw_info_t *wlc_hw, uint tx_fifo);
extern int  wlc_bmac_activate_srvsdb(wlc_hw_info_t *wlc_hw, chanspec_t chan0, chanspec_t chan1);
extern void wlc_bmac_deactivate_srvsdb(wlc_hw_info_t *wlc_hw);
#if defined(BCMDBG) || defined(WLMCHAN) || defined(SRHWVSDB)
extern void wlc_bmac_tsf_adjust(wlc_hw_info_t *wlc_hw, uint32 delta);
#endif /* BCMDBG || WLMCHAN || SRHWVSDB */
extern int  wlc_bmac_srvsdb_force_set(wlc_hw_info_t *wlc_hw, uint8 force);
#ifdef WL_MULTIQUEUE
extern void wlc_bmac_tx_fifo_sync(wlc_hw_info_t *wlc_hw, uint fifo_bitmap, uint8 flag);
#endif // endif
#ifdef STA
#if defined(WLRM)
extern void wlc_bmac_rm_cca_measure(wlc_hw_info_t *wlc_hw, uint32 us);
extern void wlc_bmac_rm_cca_int(wlc_hw_info_t *wlc_hw);
#endif // endif
#endif /* STA */

extern void wlc_ucode_wake_override_set(wlc_hw_info_t *wlc_hw, uint32 override_bit);
extern void wlc_ucode_wake_override_clear(wlc_hw_info_t *wlc_hw, uint32 override_bit);
extern void wlc_upd_suspended_fifos_set(wlc_hw_info_t *wlc_hw, uint txfifo);
extern void wlc_upd_suspended_fifos_clear(wlc_hw_info_t *wlc_hw, uint txfifo);

extern void wlc_bmac_set_rcmta(wlc_hw_info_t *wlc_hw, int idx, const struct ether_addr *addr);
extern uint16 wlc_bmac_write_amt(wlc_hw_info_t *wlc_hw, int idx,
	const struct ether_addr *addr, uint16 attr);

extern void wlc_bmac_read_amt(wlc_hw_info_t *wlc_hw, int idx,
	struct ether_addr *addr, uint16 *attr);
void
wlc_bmac_get_rcmta(wlc_hw_info_t *wlc_hw, int idx, struct ether_addr *addr);

extern void wlc_bmac_set_rxe_addrmatch(wlc_hw_info_t *wlc_hw,
	int match_reg_offset, const struct ether_addr *addr);
extern void wlc_bmac_write_hw_bcntemplates(wlc_hw_info_t *wlc_hw, void *bcn, int len, bool both);

extern void wlc_bmac_read_tsf(wlc_hw_info_t* wlc_hw, uint32* tsf_l_ptr, uint32* tsf_h_ptr);
extern void wlc_bmac_set_cwmin(wlc_hw_info_t *wlc_hw, uint16 newmin);
extern void wlc_bmac_set_cwmax(wlc_hw_info_t *wlc_hw, uint16 newmax);
extern void wlc_bmac_set_noreset(wlc_hw_info_t *wlc, bool noreset_flag);

#ifdef WL_PROXDETECT
extern void wlc_enable_avb_timer(wlc_hw_info_t *wlc_hw, bool enable);
extern void wlc_get_avb_timer_reg(wlc_hw_info_t *wlc_hw, uint32 *clkst, uint32 *maccontrol1);
extern void wlc_get_avb_timestamp(wlc_hw_info_t *wlc_hw, uint32* ptx, uint32* prx);
#endif // endif

/* is the h/w capable of p2p? */
extern bool wlc_bmac_p2p_cap(wlc_hw_info_t *wlc_hw);
/* load p2p ucode */
extern int wlc_bmac_p2p_set(wlc_hw_info_t *wlc_hw, bool enable);
/* p2p ucode provides multiple connection support. */
#define wlc_bmac_mcnx_cap(hw) wlc_bmac_p2p_cap(hw)
#define wlc_bmac_mcnx_enab(hw, en) wlc_bmac_p2p_set(hw, en)

extern void wlc_bmac_retrylimit_upd(wlc_hw_info_t *wlc_hw, uint16 SRL, uint16 LRL);

extern void wlc_bmac_fifoerrors(wlc_hw_info_t *wlc_hw);

#ifdef WLC_HIGH_ONLY
extern void wlc_bmac_dngl_reboot(rpc_info_t*);
extern void wlc_bmac_dngl_rpc_agg(rpc_info_t*, uint16 agg);
extern void wlc_bmac_dngl_rpc_msglevel(rpc_info_t*, uint16 level);
extern void wlc_bmac_dngl_rpc_txq_wm_set(rpc_info_t *rpc, uint32 wm);
extern void wlc_bmac_dngl_rpc_txq_wm_get(rpc_info_t *rpc, uint32 *wm);
extern void wlc_bmac_dngl_rpc_agg_limit_set(rpc_info_t *rpc, uint32 val);
extern void wlc_bmac_dngl_rpc_agg_limit_get(rpc_info_t *rpc, uint32 *pval);
extern int  wlc_bmac_debug_template(wlc_hw_info_t *wlc_hw);
extern int  wlc_bmac_dngl_wd_keep_alive(wlc_hw_info_t *wlc_hw, uint32 delay);
#endif // endif

/* API for BMAC driver (e.g. wlc_phy.c etc) */

extern void wlc_bmac_bw_set(wlc_hw_info_t *wlc_hw, uint16 bw);
extern void wlc_bmac_pllreq(wlc_hw_info_t *wlc_hw, bool set, mbool req_bit);
extern void wlc_bmac_set_clk(wlc_hw_info_t *wlc_hw, bool on);
extern bool wlc_bmac_taclear(wlc_hw_info_t *wlc_hw, bool ta_ok);
extern void wlc_bmac_hw_up(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_hw_down(wlc_hw_info_t *wlc_hw);

#ifdef WLLED
extern void wlc_bmac_led_hw_deinit(wlc_hw_info_t *wlc_hw, uint32 gpiomask_cache);
extern void wlc_bmac_led_hw_mask_init(wlc_hw_info_t *wlc_hw, uint32 mask);
extern bmac_led_info_t *wlc_bmac_led_attach(wlc_hw_info_t *wlc_hw);
extern int  wlc_bmac_led_detach(wlc_hw_info_t *wlc_hw);
extern int  wlc_bmac_led_blink_event(wlc_hw_info_t *wlc_hw, bool blink);
extern void wlc_bmac_led_set(wlc_hw_info_t *wlc_hw, int indx, uint8 activehi);
extern void wlc_bmac_led_blink(wlc_hw_info_t *wlc_hw, int indx, uint16 msec_on, uint16 msec_off);
extern void wlc_bmac_led(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val, bool activehi);
extern void wlc_bmac_blink_sync(wlc_hw_info_t *wlc_hw, uint32 led_pins);
#endif // endif

extern int wlc_bmac_iovars_dispatch(wlc_hw_info_t *wlc_hw, uint32 actionid, uint16 type,
	void *params, uint p_len, void *arg, int len, int val_size);
extern int wlc_bmac_phy_iovar_dispatch(wlc_hw_info_t *wlc_hw, uint32 actionid, uint16 type,
	void *p, uint plen, void *a, int alen, int vsize);

#ifdef WLC_LOW_ONLY
extern void wlc_bmac_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b, wlc_bmac_dump_id_t dump_id,
int *offset);
#else
extern void wlc_bmac_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b, wlc_bmac_dump_id_t dump_id);
#endif // endif

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && defined(WLTEST)
#ifdef BCMNVRAMW
extern int wlc_bmac_ciswrite(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len);
#endif // endif
extern int wlc_bmac_cisdump(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len);
#endif // endif
#if (defined(WLTEST) || defined(WLPKTENG))
extern int wlc_bmac_pkteng(wlc_hw_info_t *wlc_hw, wl_pkteng_t *pkteng, void* p);
#endif // endif

extern void wlc_gpio_fast_deinit(wlc_hw_info_t *wlc_hw);

#ifdef SAMPLE_COLLECT
extern void wlc_ucode_sample_init(wlc_hw_info_t *wlc_hw);
#endif // endif

extern bool wlc_bmac_radio_hw(wlc_hw_info_t *wlc_hw, bool enable, bool skip_anacore);
extern uint16 wlc_bmac_rate_shm_offset(wlc_hw_info_t *wlc_hw, uint8 rate);
extern void wlc_bmac_stf_set_rateset_shm_offset(wlc_hw_info_t *wlc_hw, uint count, uint16 pos,
  uint16 mask, wlc_stf_rs_shm_offset_t *stf_rs);

#ifdef WLEXTLOG
extern void wlc_bmac_extlog_cfg_set(wlc_hw_info_t *wlc_hw, wlc_extlog_cfg_t *cfg);
#endif // endif

extern void wlc_bmac_assert_type_set(wlc_hw_info_t *wlc_hw, uint32 type);
extern void wlc_bmac_set_txpwr_percent(wlc_hw_info_t *wlc_hw, uint8 val);
extern void wlc_bmac_ifsctl_edcrs_set(wlc_hw_info_t *wlc_hw, bool isht);

extern int wlc_bmac_cca_stats_read(wlc_hw_info_t *wlc_hw, cca_ucode_counts_t *cca_counts);
extern int wlc_bmac_obss_stats_read(wlc_hw_info_t *wlc_hw, wlc_bmac_obss_counts_t *obss_counts);
#ifdef WLCHANIM_US
extern int  wlc_bmaq_lq_stats_read(wlc_hw_info_t *wlc_hw, chanim_cnt_us_t *chanIm_cnt_us);
#endif // endif
extern uint32 wlc_bmac_cca_read_counter(wlc_hw_info_t* wlc_hw, int lo_off, int hi_off);
extern uint32 wlc_bmac_read_counter(wlc_hw_info_t* wlc_hw, uint baseaddr, int lo_off, int hi_off);
extern void wlc_bmac_antsel_set(wlc_hw_info_t *wlc_hw, uint32 antsel_avail);

extern void wlc_bmac_write_ihr(wlc_hw_info_t *wlc_hw, uint offset, uint16 v);
#ifdef STA
extern void wlc_bmac_pcie_power_save_enable(wlc_hw_info_t *wlc_hw, bool enable);
extern void wlc_bmac_pcie_war_ovr_update(wlc_hw_info_t *wlc_hw, uint8 aspm);
#endif // endif
extern void wlc_bmac_minimal_radio_hw(wlc_hw_info_t *wlc_hw, bool enable);
extern bool wlc_bmac_si_iscoreup(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_wowlucode_start(wlc_hw_info_t *wlc_hw);
#ifdef WOWL
extern int wlc_bmac_wowlucode_init(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_write_inits(wlc_hw_info_t *wlc_hw,  void *inits, int len);
extern int wlc_bmac_wakeucode_dnlddone(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_dngldown(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_wowl_config_4331_5GePA(wlc_hw_info_t *wlc_hw, bool is_5G, bool is_4331_12x9);
#endif /* WOWL */

extern int wlc_bmac_process_ucode_sr(uint16 device, osl_t *osh, void *regsva,
	uint bustype, void *btparam);

extern bool wlc_bmac_recv(wlc_hw_info_t *wlc_hw, uint fifo, bool bound, wlc_dpc_info_t *dpc);

#ifdef WLP2P_UCODE
extern void wlc_p2p_bmac_int_proc(wlc_hw_info_t *wlc_hw);
#else
#define wlc_p2p_bmac_int_proc(wlc_hw) do {} while (FALSE)
#endif // endif

#ifdef STA
extern void wlc_bmac_btc_update_predictor(wlc_hw_info_t *wlc_hw);
#endif // endif
extern bool wlc_bmac_processpmq(wlc_hw_info_t *wlc, bool bounded);
extern bool wlc_bmac_txstatus(wlc_hw_info_t *wlc_hw, bool bound, bool *fatal);
extern bool wlc_bmac_txstatus_shm(wlc_hw_info_t *wlc_hw, bool bound, bool *fatal);

#ifdef DMATXRC
extern void wlc_dmatx_reclaim(wlc_hw_info_t *wlc_hw);
#endif // endif
#ifdef WLRXOV
extern void wlc_rxov_int(wlc_info_t *wlc);
#endif // endif
#ifdef WLC_LOW_ONLY
extern void wlc_bmac_reload_mac(wlc_hw_info_t *wlc_hw);
#endif // endif
#define TBTT_AP_MASK 		1
#define TBTT_P2P_MASK 		2
#define TBTT_MCHAN_MASK 	4
#define TBTT_WD_MASK 		8
#define TBTT_TBTT_MASK 		0x10
extern void wlc_bmac_enable_tbtt(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val);
extern void wlc_bmac_set_defmacintmask(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val);
#ifdef BPRESET
extern void wlc_full_reset(wlc_hw_info_t *wlc_hw, uint32 val);
extern void wlc_dma_hang_trigger(wlc_hw_info_t *wlc_hw);
#endif // endif
extern void wlc_bmac_set_extlna_pwrsave_shmem(wlc_hw_info_t *wlc_hw);
#ifdef MACOL
extern void wlc_macol_attach(wlc_hw_info_t *wlc_hw, int *err);
extern void wlc_macol_detach(wlc_hw_info_t *wlc_hw);
extern void wlc_macol_watchdog(void *hdl);
extern bool wlc_macol_frameid_match(wlc_hw_info_t *wlc_hw, uint16 frameid);
extern bool wlc_macol_rxpkt_consumed(wlc_hw_info_t *wlc_hw, void *p, wlc_d11rxhdr_t *wrxh);
extern void wlc_macol_print_txdesc(wlc_txd_t *txd);
extern void wlc_macol_dequeue_dma_pkt(wlc_hw_info_t *wlc_hw, tx_status_t *txs);
extern int wlc_bmac_macol_iovars_dispatch(wlc_hw_info_t *wlc_hw, uint32 actionid, uint16 type,
	void *p, uint plen, void *a, int alen, int vsize);
#else
#define wlc_macol_attach(a, b)		(0)
#define wlc_macol_detach(a)		do {} while (0)
#define wlc_macol_watchdog(a)		do {} while (0)
#define wlc_macol_frameid_match(a, b)	(0)
#define wlc_macol_rxpkt_consumed(a, b, c)	(0)
#define wlc_macol_print_txdesc(a)	do {} while (0)
#define wlc_macol_dequeue_dma_pkt(a, b)	do {} while (0)
#define wlc_bmac_macol_iovars_dispatch(a, b, c, d, e, f, g, h) (0)
#endif /* MACOL */

#ifdef SCANOL
extern void wlc_scanol_attach(wlc_hw_info_t *wlc_hw, void *wl, int *err);
extern void wlc_scanol_detach(wlc_hw_info_t *wlc_hw);
extern void wlc_scanol_watchdog(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_scanol_iovars_dispatch(wlc_hw_info_t *wlc_hw, uint32 actionid, uint16 type,
	void *p, uint plen, void *a, int alen, int vsize);
#else
#define wlc_scanol_attach(a, b, c)	do {} while (0)
#define wlc_scanol_detach(a)		do {} while (0)
#define wlc_scanol_watchdog(hw)		do {} while (0)
#define wlc_bmac_scanol_iovars_dispatch(a, b, c, d, e, f, g, h)	(0)
#endif /* SCANOL */

#ifdef BCM_OL_DEV
void wlc_bmac_update_iv(wlc_hw_info_t * wlc_hw, tx_status_t * txs);
extern void wlc_bmac_set_wake_ctrl(wlc_hw_info_t *wlc_hw, bool wake);
extern void wlc_bmac_sync_sw_state(wlc_hw_info_t *wlc_hw);
#ifdef BCMDBG
extern void wlc_bmac_dump_dma(wlc_hw_info_t *wlc_hw);
#endif /* BCMDBG */
#endif /* BCM_OL_DEV */

extern int wlc_bmac_bmc_init(wlc_hw_info_t *wlc_hw, uint8 loopback, uint8 bufsize,
	uint8 reset_stats, uint8 init);

extern uint wlc_rateset_get_legacy_rateidx(ratespec_t rspec);
extern void wlc_bmac_amt_dump(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_sync_macstate(wlc_hw_info_t *wlc_hw);

#ifdef WL_LTR
extern void wlc_ltr_init_reg(si_t *sih, uint32 reg, uint32 latency, uint32 unit);
extern int wlc_ltr_init(wlc_hw_info_t *wlc_hw);
extern int wlc_ltr_up_prep(wlc_hw_info_t *wlc_hw, uint32 ltr_state);
extern int wlc_ltr_hwset(wlc_hw_info_t *wlc_hw, d11regs_t *regs, uint32 ltr_state);
extern int wlc_ltr_set(wlc_info_t *wlc, uint32 ltr_state);
extern int wlc_ltr_get(wlc_info_t * wlc);
#endif // endif

#ifdef FWID
extern uint32 gFWID;
#endif // endif

extern void wlc_bmac_coex_flush_a2dp_buffers(wlc_hw_info_t *wlc_hw);
extern bool wlc_bmac_coex_antsel(wlc_info_t *wlc);
#if defined(WLAWDL) && defined(MACOSX)
extern uint32 wlc_bmac_current_pmu_time(wlc_info_t *wlc);
#endif // endif

#if (defined(WLTEST) || defined(WLPKTENG))
extern void wlc_bmac_set_perratedpd(wlc_info_t *wlc, ratespec_t rspec);
#endif // endif

extern void wlc_bmac_enable_rx_hostmem_access(wlc_hw_info_t *wlc_hw, bool hostmem_access);
#ifdef WLC_HIGH
#ifdef WL_MULTIQUEUE
extern void wlc_bmac_enable_tx_hostmem_access(wlc_hw_info_t *wlc_hw, bool hostmem_access);
#endif /* WL_MULTIQUEUE */
#endif /* WLC_HIGH */

extern void wlc_bmac_clear_band_pwr_offset(ppr_t *txpwr_offsets, wlc_hw_info_t *wlc_hw);

#ifdef BUS_TPUT
extern void wlc_bmac_hostbus_tpt_prog_d11dma_timer(void *arg);
extern void wlc_bmac_hostbus_tpt_prog_d11dma_timer_stop(void *arg);
#endif /* BUS_TPUT */

extern void wlc_bmac_update_wl_ant_info_to_bt(wlc_hw_info_t *wlc_hw,
	uint8 txchain, uint8 rxchain);
extern uint16 wlc_bmac_read_eci_data_reg(wlc_hw_info_t *wlc_hw, uint8 reg_num);
#ifdef BCMDBG_TXSTUCK
extern void wlc_bmac_print_muted(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG_TXSTUCK */
#endif /* _WLC_BMAC_H_ */
