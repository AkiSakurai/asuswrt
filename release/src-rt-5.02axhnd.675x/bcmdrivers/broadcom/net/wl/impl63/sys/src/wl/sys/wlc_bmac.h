/*
 * Common (OS-independent) portion of
 * Broadcom 802.11bang Networking Device Driver
 *
 * BMAC driver external interface
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
 * $Id: wlc_bmac.h 780490 2019-10-28 17:07:24Z $
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
#include <hnddma.h>
#define WOWL_GPIO_INVALID_VALUE	0xFF

/* For 4349 core revision 50 */
#define D11CORE_TEMPLATE_REG_OFFSET(hw) D11_XMT_TEMPLATE_RW_PTR_OFFSET(hw)

/* BCMECICOEX support */
#ifdef BCMECICOEX

#define MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw) \
	(D11REV_GE((wlc_hw)->corerev, 130) ? \
	        ((wlc_hw)->machwcap1 & MCAP1_BTCX_GE130) : \
	        ((wlc_hw)->machwcap & MCAP_BTCX_SUP((wlc_hw)->corerev)))

#define MCAP_BTCX_SUPPORTED_WLC(wlc) \
	(D11REV_GE((wlc)->pub->corerev, 130) ? \
	        ((wlc)->machwcap1 & MCAP1_BTCX_GE130) : \
	        ((wlc)->machwcap & MCAP_BTCX_SUP((wlc)->pub->corerev)))

#define BCMECICOEX_ENAB(wlc) \
	((((wlc_info_t*)(wlc))->pub->sih->cccaps & CC_CAP_ECI) && \
	    MCAP_BTCX_SUPPORTED_WLC((wlc_info_t*)wlc))

/* ECI combo chip */
#define BCMECICOEX_ENAB_BMAC(wlc_hw) \
	((((wlc_hw_info_t*)(wlc_hw))->boardflags & BFL_BTCOEX) && \
	!(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTCLEGACY) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps & CC_CAP_ECI) && \
	!(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_SECI_PRESENT) && \
	MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw))

/* SECI combo board (SECI enabled) */
#define BCMSECICOEX_ENAB_BMAC(wlc_hw) \
	((((wlc_hw_info_t*)(wlc_hw))->boardflags & BFL_BTC2WIRE) && \
	!(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTCLEGACY) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_SECI_PRESENT) && \
	MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw))

/* GCI combo chip */
#define BCMGCICOEX_ENAB_BMAC(wlc_hw) \
	((((wlc_hw_info_t*)(wlc_hw))->boardflags & BFL_BTCOEX) && \
	!(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTCLEGACY) && \
	(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTC3WIREONLY) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT) && \
	MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw))

/* either ECI combo chip, SECI combo board(SECI enabled) or GCI combo chip */
#define BCMCOEX_ENAB_BMAC(wlc_hw) \
	(BCMECICOEX_ENAB_BMAC(wlc_hw) || BCMSECICOEX_ENAB_BMAC(wlc_hw) || \
		BCMGCICOEX_ENAB_BMAC(wlc_hw))

#define NOTIFY_BT_CHL(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_CHANNEL_MASK(CCREV(sih->ccrev)), \
		((val) << ECI_OUT_CHANNEL_SHIFT(CCREV(sih->ccrev))), TRUE)
#define NOTIFY_BT_BW_20(sih) \
	si_eci_notify_bt((sih), ECI_OUT_BW_MASK(CCREV(sih->ccrev)), \
		((ECI_BW_25) << ECI_OUT_BW_SHIFT(CCREV(sih->ccrev))), TRUE)
#define NOTIFY_BT_BW_40(sih) \
	si_eci_notify_bt((sih), ECI_OUT_BW_MASK(CCREV(sih->ccrev)), \
		((ECI_BW_45) << ECI_OUT_BW_SHIFT(CCREV(sih->ccrev))), TRUE)
#define NOTIFY_BT_NUM_ANTENNA(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_ANTENNA_MASK(CCREV(sih->ccrev)), \
		((val) << ECI_OUT_ANTENNA_SHIFT(CCREV(sih->ccrev))), TRUE)
#define NOTIFY_BT_TXRX(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_SIMUL_TXRX_MASK(CCREV(sih->ccrev)), \
		((val) << ECI_OUT_SIMUL_TXRX_SHIFT(CCREV(sih->ccrev)), TRUE))
#else
#define MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw) (0)
#define MCAP_BTCX_SUPPORTED_WLC(wlc)    (FALSE)
#define BCMECICOEX_ENAB(wlc)            (0)
#define BCMECICOEX_ENAB_BMAC(wlc_hw)	(0)
#define BCMSECICOEX_ENAB_BMAC(wlc_hw)	(0)
#define BCMGCICOEX_ENAB_BMAC(wlc_hw)	(0)
#define BCMCOEX_ENAB_BMAC(wlc_hw)	(0)
#define NOTIFY_BT_CHL(sih, val) {}
#define NOTIFY_BT_BW_20(sih) {}
#define NOTIFY_BT_BW_40(sih) {}
#define NOTIFY_BT_NUM_ANTENNA(sih, val) {}
#define NOTIFY_BT_TXRX(sih, val) {}
#endif /* BCMECICOEX */

#define BTCX_ENAB(wlc_hw)		(wlc_hw->boardflags & BFL_BTCOEX)
#define SWBTCX_ENAB(wlc_hw)		(wlc_hw->boardflags2 & BFL2_BTCLEGACY)

/* BMC = Buffer Memory Controller */
#define D11MAC_BMC_TPL_IDX_IS132	70	/* Template FIFO#70 */
#define D11MAC_BMC_TPL_IDX_IS131	22	/* Template FIFO#22 */
#define D11MAC_BMC_TPL_IDX_IS130	38	/* Template FIFO#38 */
#define D11MAC_BMC_TPL_IDX_GE128	70	/* Template FIFO#70 */
#define D11MAC_BMC_TPL_IDX_GE80		10	/* Template FIFO#10 */
#define D11MAC_BMC_TPL_IDX_LT80		7	/* Template FIFO#7 */
#define D11MAC_BMC_TPL_IDX(rev)		(D11REV_IS(rev, 132) ? D11MAC_BMC_TPL_IDX_IS132: \
					(D11REV_IS(rev, 131) ? D11MAC_BMC_TPL_IDX_IS131: \
					(D11REV_GE(rev, 130) ? D11MAC_BMC_TPL_IDX_IS130: \
					(D11REV_GE(rev, 128) ? D11MAC_BMC_TPL_IDX_GE128: \
					(D11REV_GE(rev, 80) ? D11MAC_BMC_TPL_IDX_GE80: \
					D11MAC_BMC_TPL_IDX_LT80)))))

/* Used to indicate the type of ucode */
typedef enum {
	D11_IF_SHM_STD = 0,
	D11_IF_SHM_WOWL = 1,
	D11_IF_SHM_ULP = 2,
	D11_IF_SHM_EAP = 3
} d11_if_shm_type_t;
extern void wlc_bmac_autod11_shm_upd(wlc_hw_info_t *wlc_hw, uint8 ucodeType);
extern void wlc_bmac_update_bt_chanspec(wlc_hw_info_t *wlc_hw,
	chanspec_t chanspec, bool scan_in_progress, bool roam_in_progress);

/** Revision and other info required from BMAC driver for functioning of high ONLY driver */
typedef struct wlc_bmac_revinfo {
	uint		vendorid;	/**< PCI vendor id */
	uint		deviceid;	/**< device id of chip */

	uint		boardrev;	/**< version # of particular board */
	uint		corerev;	/**< core revision */
	uint		sromrev;	/**< srom revision */
	uint		chiprev;	/**< chip revision */
	uint		chip;		/**< chip number */
	uint		chippkg;	/**< chip package */
	uint		boardtype;	/**< board type */
	uint		boardvendor;	/**< board vendor */
	uint		bustype;	/**< SB_BUS, PCI_BUS  */
	uint		buscoretype;	/**< PCI_CORE_ID, PCIE_CORE_ID, PCMCIA_CORE_ID */
	uint		buscorerev; 	/**< buscore rev */
	uint32		issim;		/**< chip is in simulation or emulation */

	uint		nbands;
	uint		boardflags; /* boardflags */
	uint	        boardflags2; /* boardflags2 */
	uint		boardflags4; /* boardflags4 */

	struct band_info {
	enum wlc_bandunit bandunit; /**< To match on both sides */
		uint bandtype; /**< To match on both sides */
		uint radiorev;
		uint phytype;
		uint phyrev;
		uint anarev;
		uint radioid;
		uint phy_minor_rev;
	} band[MAXBANDS]; /** Indexed by enum wlc_bandunit */

	/* put flags that advertize BMAC capabilities to high mac here  */
	uint32 _wlsrvsdb;		/**< 'true' if bmac is capable of srvsdb operation */
	uint		ampdu_ba_rx_wsize;
	uint		ampdu_mpdu;
	bool        is_ss; /**< bus support for super speed */

	uint32      wowl_gpio;
	uint32      wowl_gpiopol;
	uint		corerev_minor;	/**< core minor revision */
} wlc_bmac_revinfo_t;

/** dup state between BMAC(wlc_hw_info_t) and HIGH(wlc_info_t) driver */
typedef struct wlc_bmac_state {
	uint32		machwcap;     /**< mac hw capability */
	uint32		machwcap1;    /**< mac hw capability */
	uint32		preamble_ovr; /**< preamble override */
} wlc_bmac_state_t;

typedef enum {
	WLCHW_STATE_ATTACH,
	WLCHW_STATE_CLK,
	WLCHW_STATE_UP,
	WLCHW_STATE_ASSOC,
	WLCHW_STATE_LAST
} wlc_bmac_state_id_t;

/** CCA and OBSS stats */
typedef struct {
	uint32 txdur;	/**< num usecs tx'ing */
	uint32 ibss;	/**< num usecs rx'ing cur bss */
	uint32 obss;	/**< num usecs rx'ing other data */
	uint32 noctg;	/**< 802.11 of unknown type */
	uint32 nopkt;	/**< non 802.11 */
	uint32 usecs;	/**< usecs in this interval */
	uint32 PM;	/**< usecs MAC spent in doze mode for PM */

	uint32 crsglitch;	/**< num rxcrsglitchs */
	uint32 badplcp;		/**< num bad plcp */
	uint32 bphy_crsglitch;  /**< num bphy rxcrsglitchs */
	uint32 bphy_badplcp;    /**< num bphy bad plcp */

	uint32 txopp;		/**< Slot counter */
	uint32 gdtxdur;		/**< Good Tx usecs */
	uint32 bdtxdur;		/**< Bad Tx usecs */
	uint32 slot_time_txop;	/**< Slot Time For txopp conversion */

	uint32 rxdrop20s;		/**< rx sec dropped */
	uint32 rx20s;			/**< rx sec pkts */
	uint32 sec_rssi_hist_hi;	/**< rx sec rssi high histogram */
	uint32 sec_rssi_hist_med;	/**< rx sec rssi medium histogram */
	uint32 sec_rssi_hist_low;	/**< rx sec rssi low histogram */
	uint32 rxcrs_pri;		/**< rx crs primary */
	uint32 rxcrs_sec20;		/**< rx crs secondary 20 */
	uint32 rxcrs_sec40;		/**< rx crs secondary 40 */

	uint32 suspend;			/**< usecs ucode suspended */
	uint32 suspend_cnt;		/**< num ucode suspended */
	uint32 txfrm;			/**< num txing */
	uint32 rxstrt;			/**< num rxing */
	uint32 badfcs;			/**< num bad fcs */
	uint32 rxwifi;			/**< rx wifi usec */
	uint32 edcrs;			/**< edcrs usec */
	uint32 txnode[8];		/**< capture tx stats for a specific node */
	uint32 rxnode[8];		/**< capture rx stats for a specific node */
	uint32 xxobss;			/**< Mark as obss for another non-node */
} wlc_bmac_obss_counts_t;

typedef struct wlc_stf_rs_shm_offset {
	uint8 rate[WLC_NUMRATES];
	uint16 val[WLC_NUMRATES];
} wlc_stf_rs_shm_offset_t;

/** TxStatus package, 8 bytes, for d11 rev < 40
 * Used by wlc_bmac_read_txs_pkg8()
 */
typedef struct wlc_txs_pkg8 {
	uint32 word[2];
} wlc_txs_pkg8_t;

/** TxStatus package, 16 bytes, for d11 rev >= 40
 * Used by wlc_bmac_read_txs_pkg16()
 */
typedef struct wlc_txs_pkg16 {
	uint32 word[4];
} wlc_txs_pkg16_t;

extern int wlc_bmac_attach(wlc_info_t *wlc, uint16 vendor, uint16 device, uint unit,
	bool piomode, osl_t *osh, volatile void *regsva, uint bustype, void *btparam, uint macunit);
extern int wlc_bmac_detach(wlc_info_t *wlc);

wlc_hw_info_t *wlc_bmac_phase1_attach(osl_t *osh, si_t *sih, char *vars, uint varsz,
	uint *perr, uint unit);

int wlc_bmac_phase1_detach(wlc_hw_info_t *wlc_hw);

extern si_t * wlc_bmac_si_attach(uint device, osl_t *osh, volatile void *regs, uint bustype,
	void *btparam, char **vars, uint *varsz);
extern void wlc_bmac_si_detach(osl_t *osh, si_t *sih);

extern void wlc_bmac_watchdog(void *arg);
extern void wlc_bmac_info_init(wlc_hw_info_t *wlc_hw);
/* up/down, reset, clk */
extern void wlc_bmac_xtal(wlc_hw_info_t* wlc_hw, bool want);

extern void wlc_bmac_copyto_objmem(wlc_hw_info_t *wlc_hw,
                                   uint byte_offset, const void* buf, int len, uint32 sel);
extern void wlc_bmac_copyfrom_objmem(wlc_hw_info_t *wlc_hw,
                                     uint byte_offset, void* buf, int len, uint32 sel);

#define wlc_bmac_copyfrom_shm(wlc_hw, byte_offset, buf, len)                 \
	wlc_bmac_copyfrom_objmem(wlc_hw, byte_offset, buf, len, OBJADDR_SHM_SEL)
#define wlc_bmac_copyto_shm(wlc_hw, byte_offset, buf, len)                   \
	wlc_bmac_copyto_objmem(wlc_hw, byte_offset, buf, len, OBJADDR_SHM_SEL)

#define wlc_bmac_copyfrom_shmx(wlc_hw, byte_offset, buf, len)                 \
	wlc_bmac_copyfrom_objmem(wlc_hw, byte_offset, buf, len, OBJADDR_SHMX_SEL)
#define wlc_bmac_copyto_shmx(wlc_hw, byte_offset, buf, len)                   \
	wlc_bmac_copyto_objmem(wlc_hw, byte_offset, buf, len, OBJADDR_SHMX_SEL)

extern void wlc_bmac_copyfrom_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint8 *buf,
	int len, uint32 sel);
extern void wlc_bmac_copyto_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, const uint8 *buf,
	int len, uint32 sel);
extern void wlc_bmac_set_objmem32(wlc_hw_info_t *wlc_hw, uint byte_offset, uint32 val,
	int len, uint32 sel);

extern void wlc_bmac_core_phy_clk(wlc_hw_info_t *wlc_hw, bool clk);
extern void wlc_bmac_core_phypll_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_core_phypll_ctl(wlc_hw_info_t* wlc_hw, bool on);
extern void wlc_bmac_phyclk_fgc(wlc_hw_info_t *wlc_hw, bool clk);
extern void wlc_bmac_macphyclk_set(wlc_hw_info_t *wlc_hw, bool clk);
extern void wlc_bmac_phy_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_bw_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_corereset(wlc_hw_info_t *wlc_hw, uint32 flags);
extern void wlc_bmac_reset(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_init(wlc_hw_info_t *wlc_hw, chanspec_t chanspec);
extern int wlc_bmac_up_prep(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_up_finish(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_set_ctrl_SROM(wlc_hw_info_t *wlc_hw);
int wlc_bmac_set_btswitch_ext(wlc_hw_info_t *wlc_hw, int8 state);

extern int wlc_bmac_down_prep(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_down_finish(wlc_hw_info_t *wlc_hw);

extern void wlc_bmac_corereset(wlc_hw_info_t *wlc_hw, uint32 flags);
extern void wlc_bmac_switch_macfreq(wlc_hw_info_t *wlc_hw, uint8 spurmode);
extern void wlc_bmac_switch_macfreq_dynamic(wlc_hw_info_t *wlc_hw, uint8 mode);
extern void wlc_mctrl_reset(wlc_hw_info_t *wlc_hw);

/* chanspec, ucode interface */
extern int wlc_bmac_bandtype(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_set_chanspec(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute,
	ppr_t *txpwr, wl_txpwrcap_tbl_t *txpwrcap_tbl, int* cellstatus);

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
extern void wlc_bmac_write_shm(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v);
extern void wlc_bmac_update_shm(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v, uint16 mask);
extern uint16 wlc_bmac_read_shm(wlc_hw_info_t *wlc_hw, uint byte_offset);
extern void wlc_bmac_set_shm(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v, int len);
extern void wlc_bmac_write_template_ram(wlc_hw_info_t *wlc_hw, int offset, int len,
		const void *buf);
extern void wlc_bmac_templateptr_wreg(wlc_hw_info_t *wlc_hw, int offset);
extern uint32 wlc_bmac_templateptr_rreg(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_templatedata_wreg(wlc_hw_info_t *wlc_hw, uint32 word);
extern uint32 wlc_bmac_templatedata_rreg(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_copyfrom_vars(wlc_hw_info_t *wlc_hw, char ** buf, uint *len);
extern void wlc_bmac_enable_mac(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_update_rxpost_rxbnd(wlc_hw_info_t *wlc_hw, uint8 nrxpost, uint8 rxbnd);
extern void wlc_bmac_update_rxfill(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_suspend_mac_and_wait(wlc_hw_info_t *wlc_hw);
uint16 wlc_bmac_read_shm_axi_slave(wlc_hw_info_t *wlc_hw, uint offset);
void wlc_bmac_write_shm_axi_slave(wlc_hw_info_t *wlc_hw, uint offset, uint16 value);
#if defined(WL_PSMX)
extern uint16 wlc_bmac_read_shmx(wlc_hw_info_t *wlc_hw, uint byte_offset);
extern void wlc_bmac_write_shmx(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v);
extern void wlc_bmac_enable_macx(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_suspend_macx_and_wait(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_mctrlx(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val);
#else
#define wlc_bmac_enable_macx(a) do {} while (0)
#define wlc_bmac_suspend_macx_and_wait(a) do {} while (0)
#define wlc_bmac_mctrlx(a, b, c) do {} while (0)
#endif /* WL_PSMX */

#if defined(WL_PSMR1)
extern void wlc_bmac_enable_macr1(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_suspend_macr1_and_wait(wlc_hw_info_t *wlc_hw);
extern uint16 wlc_bmac_read_shm1(wlc_hw_info_t *wlc_hw, uint byte_offset);
extern void wlc_bmac_write_shm1(wlc_hw_info_t *wlc_hw, uint byte_offset, uint16 v);
#endif /* WL_PSMR1 */

extern void wlc_bmac_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea);
extern void wlc_bmac_set_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea);
extern bool wlc_bmac_validate_chip_access(wlc_hw_info_t *wlc_hw);

extern bool wlc_bmac_radio_read_hwdisabled(wlc_hw_info_t* wlc_hw);
extern void wlc_bmac_set_shortslot(wlc_hw_info_t *wlc_hw, bool shortslot);
extern void wlc_bmac_mute(wlc_hw_info_t *wlc_hw, bool want, mbool flags);
extern void wlc_bmac_set_deaf(wlc_hw_info_t *wlc_hw, bool user_flag);

extern void wlc_bmac_clear_deaf(wlc_hw_info_t *wlc_hw, bool user_flag);
extern void wlc_bmac_band_stf_ss_set(wlc_hw_info_t *wlc_hw, uint8 stf_mode);
extern void wlc_bmac_txbw_update(wlc_hw_info_t *wlc_hw);

extern void wlc_bmac_filter_war_upd(wlc_hw_info_t *wlc_hw, bool set);
extern void wlc_bmac_lo_gain_nbcal_upd(wlc_hw_info_t *wlc_hw, bool set);

extern int wlc_bmac_btc_mode_set(wlc_hw_info_t *wlc_hw, int btc_mode);
extern int wlc_bmac_btc_mode_get(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_task_set(wlc_hw_info_t *wlc_hw, int btc_task);
extern int wlc_bmac_btc_task_get(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_wire_set(wlc_hw_info_t *wlc_hw, int btc_wire);
extern int wlc_bmac_btc_wire_get(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_flags_idx_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2);
extern int wlc_bmac_btc_flags_idx_get(wlc_hw_info_t *wlc_hw, int val);
extern int wlc_bmac_btc_flags_get(wlc_hw_info_t *wlc_hw);
extern int wlc_bmac_btc_params_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2);
extern int wlc_bmac_btc_params_get(wlc_hw_info_t *wlc_hw, int int_val);
extern int wlc_bmac_btc_period_get(wlc_hw_info_t *wlc_hw, uint16 *btperiod, bool *btactive,
	uint16 *agg_off_bm, uint16 *acl_last_ts, uint16 *a2dp_last_ts);
extern void wlc_bmac_btc_stuck_war50943(wlc_hw_info_t *wlc_hw, bool enable);
extern void wlc_bmac_btc_gpio_enable(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_wait_for_wake(wlc_hw_info_t *wlc_hw);
extern bool wlc_bmac_dma_rxfill(wlc_hw_info_t *wlc_hw, uint fifo);
extern bool wlc_bmac_tx_fifo_suspended(wlc_hw_info_t *wlc_hw, uint tx_fifo);
extern void wlc_bmac_tx_fifo_suspend(wlc_hw_info_t *wlc_hw, uint tx_fifo);
extern void wlc_bmac_tx_fifo_resume(wlc_hw_info_t *wlc_hw, uint tx_fifo);
extern int  wlc_bmac_activate_srvsdb(wlc_hw_info_t *wlc_hw, chanspec_t chan0, chanspec_t chan1);
extern void wlc_bmac_deactivate_srvsdb(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_tsf_adjust(wlc_hw_info_t *wlc_hw, int delta);
extern int  wlc_bmac_srvsdb_force_set(wlc_hw_info_t *wlc_hw, uint8 force);

extern void wlc_bmac_tx_fifo_sync_all(wlc_hw_info_t *wlc_hw, uint8 flag);
extern void wlc_bmac_tx_fifo_sync(wlc_hw_info_t *wlc_hw, void *fifo_bitmap, uint8 flag);

extern void wlc_ucode_wake_override_set(wlc_hw_info_t *wlc_hw, uint32 override_bit);
extern void wlc_ucode_wake_override_clear(wlc_hw_info_t *wlc_hw, uint32 override_bit);
extern bool wlc_upd_suspended_fifos_set(wlc_hw_info_t *wlc_hw, uint txfifo);
extern void wlc_upd_suspended_fifos_clear(wlc_hw_info_t *wlc_hw, uint txfifo);

extern void wlc_bmac_set_rcmta(wlc_hw_info_t *wlc_hw, int idx, const struct ether_addr *addr);
extern uint16 wlc_bmac_write_amt(wlc_hw_info_t *wlc_hw, int idx,
	const struct ether_addr *addr, uint16 attr);

extern void wlc_bmac_read_amt(wlc_hw_info_t *wlc_hw, int idx,
	struct ether_addr *addr, uint16 *attr);

extern void wlc_bmac_get_rcmta(wlc_hw_info_t *wlc_hw, int idx, struct ether_addr *addr);

extern void wlc_bmac_set_rxe_addrmatch(wlc_hw_info_t *wlc_hw,
	int match_reg_offset, const struct ether_addr *addr);
extern void wlc_bmac_write_hw_bcntemplates(wlc_hw_info_t *wlc_hw, void *bcn, int len, bool both);

extern void wlc_bmac_read_tsf(wlc_hw_info_t* wlc_hw, uint32* tsf_l_ptr, uint32* tsf_h_ptr);
extern uint32 wlc_bmac_read_usec_timer(wlc_hw_info_t* wlc_hw);
extern void wlc_bmac_set_cwmin(wlc_hw_info_t *wlc_hw, uint16 newmin);
extern void wlc_bmac_set_cwmax(wlc_hw_info_t *wlc_hw, uint16 newmax);
#ifdef WLC_TSYNC
extern void wlc_bmac_start_tsync(wlc_hw_info_t* wlc_hw);
#endif // endif
extern void wlc_bmac_set_noreset(wlc_hw_info_t *wlc, bool noreset_flag);
extern bool wlc_bmac_get_noreset(wlc_hw_info_t *wlc);

#if defined(WL_PROXDETECT) || defined(WLC_TSYNC)
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
extern bool wlc_bmac_pktpool_empty(wlc_info_t *wlc);

/* API for BMAC driver (e.g. wlc_phy.c etc) */

extern void wlc_bmac_bw_set(wlc_hw_info_t *wlc_hw, uint16 bw);
extern int wlc_bmac_bw_check(wlc_hw_info_t *wlc_hw);
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

int wlc_bmac_dump(wlc_hw_info_t *wlc_hw, const char *name, struct bcmstrbuf *b);
int wlc_bmac_dump_clr(wlc_hw_info_t *wlc_hw, const char *name);

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && defined(WLTEST)
#ifdef BCMNVRAMW
extern int wlc_bmac_ciswrite(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len);
#endif // endif
#endif // endif
#if defined(BCM_CISDUMP_NO_RECLAIM) || (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && \
	defined(WLTEST)
extern int wlc_bmac_cisdump(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len);
#endif // endif
#if (defined(WLTEST) || defined(WLPKTENG))
extern int wlc_bmac_pkteng(wlc_hw_info_t *wlc_hw, wl_pkteng_t *pkteng, void* p[], int npkts);
#endif // endif

#ifdef WLCNT
extern void wlc_bmac_pkteng_poll_interval_set(wlc_hw_info_t *wlc_hw, uint32 timer_cb_dur);
extern void wlc_bmac_pkteng_poll_interval_reset(wlc_hw_info_t *wlc_hw);
#endif // endif

extern void wlc_gpio_fast_deinit(wlc_hw_info_t *wlc_hw);

#ifdef SAMPLE_COLLECT
extern void wlc_ucode_sample_init(wlc_hw_info_t *wlc_hw);
#endif // endif

extern bool wlc_bmac_radio_hw(wlc_hw_info_t *wlc_hw, bool enable, bool skip_anacore);
extern uint16 wlc_bmac_rate_shm_offset(wlc_hw_info_t *wlc_hw, uint8 rate);
extern void wlc_bmac_stf_set_rateset_shm_offset(wlc_hw_info_t *wlc_hw, uint count, uint16 pos,
  uint16 mask, wlc_stf_rs_shm_offset_t *stf_rs);

extern void wlc_bmac_assert_type_set(wlc_hw_info_t *wlc_hw, uint32 type);
extern void wlc_bmac_set_txpwr_percent(wlc_hw_info_t *wlc_hw, uint8 val);
extern void wlc_bmac_ifsctl_edcrs_set(wlc_hw_info_t *wlc_hw, bool isht);
extern void wlc_bmac_set_txpwr_degrade(wlc_hw_info_t *wlc_hw, uint8 val);

extern int wlc_bmac_cca_stats_read(wlc_hw_info_t *wlc_hw, cca_ucode_counts_t *cca_counts);
extern int wlc_bmac_obss_stats_read(wlc_hw_info_t *wlc_hw, wlc_bmac_obss_counts_t *obss_counts);
#ifdef WLCHANIM_US
extern int  wlc_bmaq_lq_stats_read(wlc_hw_info_t *wlc_hw, chanim_cnt_us_t *chanIm_cnt_us);
#endif /* WLCHANIM_US */
extern uint32 wlc_bmac_cca_read_counter(wlc_hw_info_t* wlc_hw, int lo_off, int hi_off);
extern uint32 wlc_bmac_read_counter(wlc_hw_info_t* wlc_hw, uint baseaddr, int lo_off, int hi_off);
extern void wlc_bmac_antsel_set(wlc_hw_info_t *wlc_hw, uint32 antsel_avail);

extern void wlc_bmac_write_ihr(wlc_hw_info_t *wlc_hw, uint offset, uint16 v);
extern void wlc_bmac_write_scr(wlc_hw_info_t *wlc_hw, uint offset, uint16 v);
extern uint16 wlc_bmac_read_ihr(wlc_hw_info_t *wlc_hw, uint offset);
extern uint16 wlc_bmac_read_scr(wlc_hw_info_t *wlc_hw, uint offset);

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
#endif /* WOWL */

extern int wlc_bmac_process_ucode_sr(wlc_hw_info_t *wlc_hw);

extern bool wlc_bmac_recv(wlc_hw_info_t *wlc_hw, uint fifo, bool bound, wlc_dpc_info_t *dpc);

#ifdef WLP2P_UCODE
extern void wlc_p2p_bmac_int_proc(wlc_hw_info_t *wlc_hw);
#else
#define wlc_p2p_bmac_int_proc(wlc_hw) do {} while (FALSE)
#endif // endif

#ifdef STA
extern void wlc_bmac_btc_update_predictor(wlc_hw_info_t *wlc_hw);
#endif // endif

/** Read an 8 byte status package from the TxStatus fifo registers */
int wlc_bmac_read_txs_pkg8(wlc_hw_info_t *wlc_hw, wlc_txs_pkg8_t *txs);
/** Read a 16 byte status package from the TxStatus fifo registers */
int wlc_bmac_read_txs_pkg16(wlc_hw_info_t *wlc_hw, wlc_txs_pkg16_t *txs);

/** Dump the TxStatus history to the Event Log */
void wlc_txs_hist_log_dump(wlc_hw_info_t *wlc_hw, int tag);

#ifdef WL_TXS_LOG
void wlc_bmac_txs_hist_pkg8(wlc_hw_info_t *wlc_hw, wlc_txs_pkg8_t *txs);
void wlc_bmac_txs_hist_pkg16(wlc_hw_info_t *wlc_hw, wlc_txs_pkg16_t *txs);
#endif /* WL_TXS_LOG */

extern bool wlc_bmac_txstatus(wlc_hw_info_t *wlc_hw, bool bound, bool *fatal);
extern bool wlc_bmac_txstatus_shm(wlc_hw_info_t *wlc_hw, bool bound, bool *fatal);

#ifdef WLRXOV
extern void wlc_rxov_int(wlc_info_t *wlc);
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
extern uint wlc_rateset_get_legacy_rateidx(ratespec_t rspec);
extern void wlc_bmac_amt_dump(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_sync_macstate(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_update_synthpu(wlc_hw_info_t *wlc_hw);

#ifdef FWID
extern uint32 gFWID;
#endif // endif
extern void wlc_bmac_coex_flush_a2dp_buffers(wlc_hw_info_t *wlc_hw);

#ifdef ENABLE_CORECAPTURE
extern int wlc_ucode_dump_ucm(wlc_hw_info_t *wlc_hw);
#endif /* ENABLE_CORECAPTURE */

extern void wlc_bmac_enable_rx_hostmem_access(wlc_hw_info_t *wlc_hw, bool hostmem_access);
#ifdef BCMPCIEDEV
extern void wlc_bmac_enable_tx_hostmem_access(wlc_hw_info_t *wlc_hw, bool hostmem_access);
#endif /* BCMPCIEDEV */

/* 4349a0 phymode war for common registers that should have been path registers */
extern void wlc_bmac_exclusive_reg_access_core0(wlc_hw_info_t *wlc_hw, bool set);
extern void wlc_bmac_exclusive_reg_access_core1(wlc_hw_info_t *wlc_hw, bool set);

#ifndef WL_DUALMAC_RSDB
extern bool wlc_bmac_rsdb_cap(wlc_hw_info_t *wlc_hw);
#else
#define wlc_bmac_rsdb_cap(wlc_hw) (FALSE)
#endif // endif

extern void wlc_bmac_ifsctl_vht_set(wlc_hw_info_t *wlc_hw, int ed_sel);

extern void wlc_bmac_rcvlazy_update(wlc_hw_info_t *wlc_hw, uint32 intrcvlazy);
extern void wlc_bmac_ifsctl1_regshm(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val);

#ifdef BCMDBG_TXSTUCK
extern void wlc_bmac_print_muted(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG_TXSTUCK */

extern bool wlc_bmac_pio_enab_check(wlc_hw_info_t *wlc_hw);

/* get slice specific OTP/SROM parameters */
int getintvar_slicespecific(wlc_hw_info_t *wlc_hw, char *vars, const char *name);
int wlc_bmac_reset_txrx(wlc_hw_info_t *wlc_hw);
int wlc_bmac_bmc_dyn_reinit(wlc_hw_info_t *wlc_hw, uint8 bufsize_in_256_blocks);
void wlc_bmac_rsdb_mode_param_to_shmem(wlc_hw_info_t *wlc_hw);
void wlc_bmac_4349_core1_hwreqoff(wlc_hw_info_t *wlc_hw, bool mode);
extern uint16 wlc_bmac_read_eci_data_reg(wlc_hw_info_t *wlc_hw, uint8 reg_num);
unsigned int wlc_bmac_shmphymode_dump(wlc_hw_info_t *wlc_hw);

extern int  wlc_sleep(struct wlc_info *wlc);
extern int  wlc_resume(struct wlc_info *wlc);
extern int wlc_update_splitrx_mode(wlc_hw_info_t *wlc_hw, bool mode, bool init);
#ifdef MBSS
extern bool wlc_bmac_ucodembss_hwcap(wlc_hw_info_t *wlc_hw);
#else
#define wlc_bmac_ucodembss_hwcap(a) 0
#endif // endif

extern void wlc_coredisable(wlc_hw_info_t* wlc_hw);
extern void wlc_rxfifo_setpio(wlc_hw_info_t *wlc_hw);

int wlc_bmac_dump_list(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b);
int wlc_bmac_dump_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b);

extern uint wlc_bmac_coreunit(wlc_info_t *wlc);

#ifdef WAR4360_UCODE
extern uint32 wlc_bmac_need_reinit(wlc_hw_info_t *wlc_hw);
#endif /* WAR4360_UCODE */

extern bool wlc_bmac_report_fatal_errors(wlc_hw_info_t *wlc_hw, int rc);

extern int wlc_bmac_update_fastpwrup_time(wlc_hw_info_t *wlc_hw);

#if defined(WL_RX_DMA_STALL_CHECK)

/*
 * RX DMA Stall detection support
 * WLCNT support is required for rx overflow tracking.
 */
uint wlc_bmac_rx_dma_stall_timeout_get(wlc_hw_info_t *wlc_hw);
int wlc_bmac_rx_dma_stall_timeout_set(wlc_hw_info_t *wlc_hw, uint timeout);
int wlc_bmac_rx_dma_stall_force(wlc_hw_info_t *wlc_hw, uint32 testword);

#endif /* WL_RX_DMA_STALL_CHECK */

#ifdef WLRSDB
extern void wlc_bmac_switch_pmu_seq(wlc_hw_info_t *wlc_hw, uint mode);
#endif /* WLRSDB */
#ifdef GPIO_TXINHIBIT
extern void wlc_bmac_gpio_set_tx_inhibit_tout(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_notify_gpio_tx_inhibit(wlc_info_t *wlc);
#endif // endif

extern void ai_force_clocks(si_t *sih, uint clock_state);

extern uint16 wlc_bmac_dma_bulk_txcomplete(wlc_info_t *wlc, uint fifo,
		uint16 ncons, void **list_head, void **list_tail, txd_range_t range, bool nonAMPDU);
#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
extern void wlc_bmac_hostbus_tpt_prog_d11dma_timer(void *arg);
extern void wlc_bmac_hostbus_tpt_prog_d11dma_timer_stop(void *arg);
#endif /* BCMPCIEDEV && BUS_TPUT */
extern void wlc_bmac_psm_maccommand(wlc_hw_info_t *wlc_hw, uint16 val);
extern int wlc_bmac_dma_txfast(wlc_info_t *wlc, uint fifo, void *p, bool commit);
#if defined(BCMHWA) && defined(HWA_TXFIFO_BUILD)
extern int wlc_bmac_hwa_txfifo_commit(wlc_info_t *wlc, uint fifo, const char *from);
extern void wlc_bmac_hwa_txfifo_ring_full(wlc_info_t *wlc, bool isfull);
#endif // endif
extern void * wlc_bmac_dma_getnexttxp(wlc_info_t *wlc, uint fifo, txd_range_t range);
extern void wlc_bmac_ctdma_update(wlc_hw_info_t *wlc_hw, bool enabled);

void wlc_bmac_update_rxpost_rxfill(wlc_hw_info_t *wlc_hw, uint8 fifo_no, uint8 nrxpost);

#ifdef BCMULP
extern int wlc_bmac_ulp_preattach(void);
extern int wlc_bmac_p2_retrieve_cb(void *handle, osl_t *osh, uint8 *p2_cache_data);
#endif /* BCMULP */

#ifdef BCMFRWDPOOLREORG
void wlc_register_dma_rxfill_cb(wlc_info_t *wlc, bool reg);
#endif // endif

#if defined(WL_MU_RX) && defined(WLCNT) && (defined(BCMDBG) || defined(WLDUMP) || \
	defined(BCMDBG_MU))

extern void wlc_bmac_upd_murate(wlc_info_t *wlc, d11rxhdr_t *rxhdr, uchar *plcp);
extern void wlc_bmac_save_murate2plcp(wlc_info_t *wlc, d11rxhdr_t *rxhdr, void *p);
#endif	/* WL_MU_RX */

extern void wlc_bmac_set_myaddr(wlc_hw_info_t *wlc_hw, struct ether_addr *mac_addr);
extern void wlc_bmac_handle_device_halt(wlc_hw_info_t *wlc_hw, bool TX, bool CORE, bool RX);

extern void wlc_bmac_classify_fifo_suspend(wlc_hw_info_t *wlc_hw);
extern void wlc_bmac_classify_fifo_resume(wlc_hw_info_t *wlc_hw, bool rollback);
extern bool wlc_bmac_classify_fifo_suspended(wlc_hw_info_t *wlc_hw);

uint wlc_bmac_rxfifosz_get(wlc_hw_info_t *wlc_hw);
extern void *wlc_bmac_dmatx_peeknexttxp(wlc_info_t *wlc, int fifo);

extern void wlc_dma_map_pkts(wlc_hw_info_t *wlc_hw, map_pkts_cb_fn cb, void *ctx);

#ifdef BCMDBG_SR
extern int wlc_bmac_sr_verify(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *b);
extern int wlc_bmac_sr_testmode(wlc_hw_info_t *wlc_hw, int mode);
#endif // endif

#ifdef WL_UTRACE
extern void wlc_dump_utrace_logs(wlc_info_t *wlc, uchar *p);
#endif // endif

extern bool wlc_bmac_swcap_get(wlc_hw_info_t *wlc_hw, uint8 capbit);
extern void wlc_bmac_swcap_set(wlc_hw_info_t *wlc_hw, uint8 capbit, bool val);
extern bool wlc_bmac_dotxstatus(wlc_hw_info_t *wlc, tx_status_t *txs, uint32 s2);

extern void wlc_bmac_recover_pkts(wlc_info_t *wlc, uint queue);

extern bool wlc_bmac_check_d11war(wlc_hw_info_t *wlc_hw, uint32 chk_flags);
extern uint32 wlc_bmac_get_d11war_flags(wlc_hw_info_t *wlc_hw);

#define ASSERT_PSM_UCODE(expr, reason, wlc)
#define ASSERT_PSMX_UCODE(expr, reason, wlc)

#if defined(BCMPCIEDEV)
#ifdef BULKRX_PKTLIST
extern void wlc_bmac_process_split_fifo_pkt(wlc_hw_info_t *wlc_hw, void* p, uint8 rxh_offset);
#else
extern int wlc_bmac_process_split_fifo_pkt(wlc_hw_info_t *wlc_hw, uint fifo, void* p);
#endif // endif
#endif /* BCMPCIEDEV */
#if defined(STS_FIFO_RXEN)|| defined(WLC_OFFLOADS_RXSTS)
extern void
wlc_bmac_recv_append_sts_list(wlc_info_t *wlc, rx_list_t *sts_list1, rx_list_t *sts_list2);
extern void wlc_bmac_recv_process_sts(wlc_hw_info_t *wlc_hw, uint fifo,
        rx_list_t *rx_list, rx_list_t *sts_list, uint8 rxh_offset);
extern void wlc_bmac_stsbuff_free(wlc_hw_info_t *wlc_hw, void *p);
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */
#if defined(BCMDBG) && defined(BCMSPLITRX)
extern void wlc_bmac_dump_host_buffer(wlc_info_t * wlc,
	uint64 haddr64, uint32 len);
#endif /* defined(BCMDBG) && defined(BCMSPLITRX) */
#ifdef BCM43684_HDRCONVTD_ETHER_LEN_WAR
extern void wlc_bmac_write_host_buffer(wlc_info_t *wlc,
	uint64 haddr64, uint8 *buf, uint32 len);
#endif /* BCM43684_HDRCONVTD_ETHER_LEN_WAR */
extern bool
wlc_bmac_is_pcielink_slowspeed(wlc_hw_info_t *wlc_hw);
#ifdef RX_DEBUG_ASSERTS
extern void wlc_rx_invalid_length_handle(wlc_info_t* wlc, void* p, d11rxhdr_t *rxh);
#endif /* RX_DEBUG_ASSERTS */
extern void wlc_bmac_hfifo_rdy_get(wlc_hw_info_t *wlc_hw, uint16 fifordy[]);
extern void wlc_bmac_hfifo_frmcnt_get(wlc_hw_info_t *wlc_hw, uint16 frmcnt[]);
#endif /* _WLC_BMAC_H_ */
