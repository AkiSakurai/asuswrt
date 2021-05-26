/*
 * Private H/W info of
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id$
 */

#ifndef _wlc_hw_priv_h_
#define _wlc_hw_priv_h_

#include "wlc_types.h"
#include <pcie_core.h>

/* Interrupt bit error summary.  Don't include I_RU: we refill DMA at other
 * times; and if we run out, constant I_RU interrupts may cause lockup.  We
 * will still get error counts from rx0ovfl.
 */
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RO | I_XU)
/* default software intmasks */
#define	DEF_RXINTMASK	(I_RI)	/* enable rx int on rxfifo only */

#ifdef BCM_OL_DEV
#define	DEF_MACINTMASK	MI_DMAINT | MI_ALTTFS | MI_BG_NOISE | \
			 MI_HWACI_NOTIFY
#else
#define	DEF_MACINTMASK	(MI_TXSTOP | MI_ATIMWINEND | MI_PMQ | \
			 MI_PHYTXERR | MI_DMAINT | MI_TFS | MI_BG_NOISE | \
			 MI_CCA | MI_TO | MI_GP0 | MI_RFDISABLE | MI_PWRUP | \
			 MI_BT_RFACT_STUCK | MI_BT_PRED_REQ | MI_HWACI_NOTIFY)

#endif // endif

/* Below interrupts can be delayed and piggybacked with other interrupts.
 * We don't want these interrupts to trigger an isr so we can save CPU & power.
 * They are not enabled, but handled when set in interrupt status
 */
#define	DELAYEDINTMASK  (0 /* | MI_BG_NOISE */)

/* default d11 core intrcvlazy value to have no delay before receive interrupt */
#ifndef WLC_INTRCVLAZY_DEFAULT
#define WLC_INTRCVLAZY_DEFAULT IRL_DISABLE
#endif // endif

#ifdef WLC_LOW
struct wlc_hwband {
	int		bandtype;		/* WLC_BAND_2G, WLC_BAND_5G */
	uint		bandunit;		/* bandstate[] index */
	uint16		mhfs[MHFMAX];		/* MHF array shadow */
	uint8		bandhw_stf_ss_mode;	/* HW configured STF type, 0:siso; 1:cdd */
	uint16		CWmin;
	uint16		CWmax;
	uint32          core_flags;

	uint16		phytype;		/* phytype */
	uint16		phyrev;
	uint16		radioid;
	uint16		radiorev;
	wlc_phy_t	*pi;			/* pointer to phy specific information */
	bool		abgphy_encore;
	uint8		bw_cap;			/* Bandwidth bitmask on this band */
};

typedef struct wlc_hw_btc_info {
	int		mode;		/* Bluetooth Coexistence mode */
	int		wire;		/* BTC: 2-wire or 3-wire */
	uint16		flags;		/* BTC configurations */
	uint32		gpio_mask;	/* Resolved GPIO Mask */
	uint32		gpio_out;	/* Resolved GPIO out pins */
	bool		stuck_detected;	/* stuck BT_RFACT has been detected */
	bool		stuck_war50943;
	bool		bt_active;		/* bt is in active session */
	uint8		bt_active_asserted_cnt;	/* 1st bt_active assertion */
	uint8		btcx_aa;		/* a copy of aa2g:aa5g */
	uint16		bt_shm_addr;
	uint16		bt_period;
	uint8		bt_period_out_of_sync_cnt;
	bool		btc_init_flag;
	uint16		*wlc_btc_params;	/* storage for btc shmem when microcode is down */
	uint16		*wlc_btc_params_fw;	/* storage for firmware-based btc parameters */
} wlc_hw_btc_info_t;

/* LTE coex definitions */

typedef struct wlc_hw_ltecx_info {
	uint16		ltecx_chmap;	/* per-ch ltecx bm for olympic (iovar "mws_coex_bitmap") */
	uint16		ltecx_enabled;	/* LTECX enabled/disabled in ucode */
	uint16		ltecx_idle;		/* LTE signalling IDLE */
	uint32		ltecx_flags;
	uint32		ltecxmux;		/* LTECX Pin Mux */
	uint16		baud_rate;		/* SECI uart baud rate */
	uint16		mws_wlanrx_prot;
	uint16		mws_wlanrx_prot_prev;	/* Previous protection mode */
	uint16		mws_frame_config;
	uint16		mws_lterx_prot;
	uint16		mws_lterx_prot_prev;	/* To detect change in mws_lterx_prot */
	uint16		mws_im3_prot;
	uint16		mws_im3_prot_prev;	/* To detect change in mws_lterx_prot */
	uint16		ltetx_adv;
	uint16		ltetx_adv_prev;	/* To detect change in ltetx_adv */
	uint16		adv_tout_prev;
	uint16		scanjoin_prot;
	uint16		scanjoin_prot_prev; /* To detect change in scanjoin_prot */
	uint16		mws_ltecx_txind;
	uint16		mws_ltecx_txind_prev; /* To detect change in mws_ltecx_txind */
	int16		ltecx_rssi_thresh_lmt_nvram;
	int		mws_wifi_sensi;
	int		mws_wifi_sensi_prev;
	int		mws_ltecx_wifi_sensitivity;
	mws_wci2_msg_t	mws_wci2_msg;
	int		mws_debug_mode;
	int32		mws_elna_rssi_thresh; /* elna bypass RSSI threshold */
	int32		mws_ltecx_rssi_thresh_lmt; /* rssi threshold hysteresis loop limit */
	bool		mws_elna_bypass;		/* 1: elna bypassed 0: elna enabled */
	bool		mws_wlan_rx_ack;
	bool		mws_wlan_rx_ack_prev;
	uint8		mws_rx_aggr_off;	/* 1: Rx Aggregation disabled by LTECX */
	uint32		*ltecx_20mhz_modes; /* Unused. Keep for ROM compatibility */
	uint32		*ltecx_10mhz_modes; /* Unused. Keep for ROM compatibility */
	uint16		lte_center_freq_prev;
	uint16		lte_channel_bw_prev;
	uint16		mws_wlanrx_prot_min_ch;
	uint16		mws_lterx_prot_min_ch;
	uint16		mws_scanjoin_prot_min_ch;
	uint16		mws_lte_freq_index;
	uint32		*ltecx_rssi_thresh_20mhz; /* Unused. Keep for ROM compatibility */
	uint32		ltecx_rssi_thresh_10mhz; /* Unused. Keep for ROM compatibility */
	mws_params_t	mws_params;
	wci2_config_t	wci2_config;
	uint32		ltecx_20mhz_modes_prot[LTECX_NVRAM_PARAM_MAX];
					/* wlanrx_prot, lterx_prot, scanjoin_prot */
	uint32		ltecx_10mhz_modes_prot[LTECX_NVRAM_PARAM_MAX];
					/* wlanrx_prot, lterx_prot, scanjoin_prot */
	uint32		ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_RSSI_THRESH_20MHZ]
				[LTECX_NVRAM_MAX_CHANNELS]; /* elna rssi threshold for 20MHz BW */
	uint32		ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_RSSI_THRESH_10MHZ]
				[LTECX_NVRAM_MAX_CHANNELS]; /* elna rssi threshold for 10MHz BW */
	uint16		mws_wlanrxpri_thresh;
	uint32          ltecxpadnum;
	uint32          ltecxfnsel;
	uint32          ltecxgcigpio;
} wlc_hw_ltecx_info_t;

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
typedef struct bmac_suspend_stats {
	uint32 suspend_count;
	uint32 suspended;
	uint32 unsuspended;
	uint32 suspend_start;
	uint32 suspend_end;
	uint32 suspend_max;
} bmac_suspend_stats_t;
#endif /* BCMDBG || BCMDBG_DUMP */
#endif /* WLC_LOW */

struct wlc_hw_info {
	wlc_info_t	*wlc;
	wlc_hw_t	*pub;			/* public API */
	osl_t		*osh;			/* pointer to os handle */
	uint		unit;			/* device instance number */

#ifdef WLC_SPLIT
	rpc_info_t	*rpc;			/* Handle to RPC module */
#endif // endif

	bool		_piomode;		/* true if pio mode */
	bool		_p2p;			/* download p2p ucode */

	/* fifo info shadowed in wlc_hw_t.
	 * These data are "read" directly but and shall be modified via the following APIs:
	 * - wlc_hw_set_di()
	 * - wlc_hw_set_pio()
	 */
	hnddma_t	*di[NFIFO];		/* hnddma handles, per fifo */
	pio_t		*pio[NFIFO];		/* pio handlers, per fifo */

#ifdef WLC_LOW
	/* version info */
	uint16		vendorid;		/* PCI vendor id */
	uint16		deviceid;		/* PCI device id */
	uint		corerev;		/* core revision */
	uint8		sromrev;		/* version # of the srom */
	uint16		boardrev;		/* version # of particular board */
	uint32		boardflags;		/* Board specific flags from srom */
	uint32		boardflags2;		/* More board flags if sromrev >= 4 */

	/* interrupt */
	uint32		macintstatus;		/* bit channel between isr and dpc */
	uint32		macintmask;		/* sw runtime master macintmask value */
	uint32		defmacintmask;		/* default "on" macintmask value */
	uint32		delayedintmask;		/* mask of delayed interrupts */
	uint32		tbttenablemask;		/* mask of tbtt interrupt clients */

	uint32		machwcap;		/* MAC capabilities (corerev >= 13) */
	uint32		machwcap_backup;	/* backup of machwcap (corerev >= 13) */
	uint16		ucode_dbgsel;		/* dbgsel for ucode debug(config gpio) */
	bool		ucode_loaded;		/* TRUE after ucode downloaded */

	wlc_hw_btc_info_t *btc;

	si_t		*sih;			/* SB handle (cookie for siutils calls) */
	char		*vars;			/* "environment" name=value */
	uint		vars_size;		/* size of vars, free vars on detach */
	d11regs_t	*regs;			/* pointer to device registers */
	void		*physhim;		/* phy shim layer handler */
	void		*phy_sh;		/* pointer to shared phy state */
	wlc_hwband_t	*band;			/* pointer to active per-band state */
	wlc_hwband_t	*bandstate[MAXBANDS];	/* per-band state (one per phy/radio) */
	uint16		bmac_phytxant;		/* cache of high phytxant state */
	bool		shortslot;		/* currently using 11g ShortSlot timing */
	uint16		SRL;			/* 802.11 dot11ShortRetryLimit */
	uint16		LRL;			/* 802.11 dot11LongRetryLimit */
	uint16		SFBL;			/* Short Frame Rate Fallback Limit */
	uint16		LFBL;			/* Long Frame Rate Fallback Limit */

	bool		up;			/* d11 hardware up and running */
	uint		now;			/* # elapsed seconds */
	uint		phyerr_cnt;		/* # continuous TXPHYERR counts */
	uint		_nbands;		/* # bands supported */
	chanspec_t	chanspec;		/* bmac chanspec shadow */

	uint		*txavail[NFIFO];	/* # tx descriptors available */
	uint16		*xmtfifo_sz;		/* fifo size in 256B for each xmt fifo */
	uint16		xmtfifo_frmmax[AC_COUNT];	/* max # of frames fifo size can hold */

	mbool		pllreq;			/* pll requests to keep PLL on */

	uint8		suspended_fifos;	/* Which TX fifo to remain awake for */
	uint32		maccontrol;		/* Cached value of maccontrol */
	uint		mac_suspend_depth;	/* current depth of mac_suspend levels */
	uint32		wake_override;		/* Various conditions to force MAC to WAKE mode */
	uint32		mute_override;		/* Prevent ucode from sending beacons */
	struct		ether_addr etheraddr;	/* currently configured ethernet address */
	uint32		led_gpio_mask;		/* LED GPIO Mask */
	bool		noreset;		/* true= do not reset hw, used by WLC_OUT */
	bool		forcefastclk;		/* true if the h/w is forcing the use of fast clk */
	bool		clk;			/* core is out of reset and has clock */
	bool		sbclk;			/* sb has clock */
	bmac_pmq_t	*bmac_pmq;		/*  bmac PM states derived from ucode PMQ */
	bool		phyclk;			/* phy is out of reset and has clock */
	bool		dma_lpbk;		/* core is in DMA loopback */
	uint16		fastpwrup_dly;		/* time in us needed to bring up d11 fast clock */

#ifdef BCMSDIO
	void		*sdh;
	uint		sdiod_drive_strength;	/* SDIO drive strength */
#endif // endif
#ifdef WLLED
	bmac_led_info_t	*ledh;			/* pointer to led specific information */
#endif // endif
#ifdef WLC_LOW_ONLY
	struct wl_timer *wdtimer;		/* timer for watchdog routine */
	struct wl_timer *rpc_agg_wdtimer;	/* rpc agg timer */
	struct ether_addr orig_etheraddr;	/* original hw ethernet address */
	uint16		rpc_dngl_agg;		/* rpc agg control for dongle */
#ifdef DMA_TX_FREE
	wlc_txstatus_flags_t txstatus_ampdu_flags[NFIFO];
#endif // endif
#ifdef WLEXTLOG
	wlc_extlog_info_t *extlog;		/* external log handle */
#endif // endif
	uint32		mem_required_def;	/* memory required to replenish RX DMA ring */
	uint32		mem_required_lower;	/* memory required with lower RX bound */
	uint32		mem_required_least;	/* minimum memory requirement to handle RX */

#endif	/* WLC_LOW_ONLY */

	uint8		hw_stf_ss_opmode;	/* STF single stream operation mode */
	uint8		antsel_type;		/* Type of boardlevel mimo antenna switch-logic
						 * 0 = N/A, 1 = 2x4 board, 2 = 2x3 CB2 board
						 */
	uint32		antsel_avail;		/* put antsel_info_t here if more info is needed */

	bool		btclock_tune_war;	/* workaoround to stablilize bt clock  */
	uint16		noise_metric;		/* To enable/disable knoise measurement. */

	uint32		intrcvlazy;		/* D11 core INTRCVLAZY register setting */
	uint		p2p_shm_base;		/* M_P2P_BSS_BLK SHM base byte offset */
	uint		cca_shm_base;		/* M_CCA_STATS_BLK SHM base byte offset */
#endif /* WLC_LOW */

	/* MHF2_SKIP_ADJTSF ucode host flag manipulation */
	uint32		skip_adjtsf;	/* bitvec, IDs of users requesting to skip ucode TSF adj. */
	/* MCTL_AP maccontrol register bit manipulation when AP_ACTIVE() */
	uint32		mute_ap;	/* bitvec, IDs of users requesting to stop the AP func. */

	/* variables to store BT switch state/override settings */
	int8   btswitch_ovrd_state;
	uint8	antswctl2g;	/* extlna switch control read from SROM (2G) */
	uint8	antswctl5g;	/* extlna switch control read from SROM (5G) */
	bool            reinit;
	uint32	vcoFreq_4360_pcie2_war; /* changing the avb vcoFreq */
#ifdef WLC_LOW
    uint8 sr_vsdb_force;          /* for test/debug purpose */
    bool  sr_vsdb_active;         /* '1' when higher MAC activated srvsdb operation */
#endif // endif
#ifdef BCM_OL_DEV
	uint16 pso_blk;			/* Byte Addr */
	uint16 txs_rptr;		/* Word Addr */
	uint16 txs_addr_blk;	/* Word Addr */
	uint16 txs_addr_end;	/* Word Addr */
#endif /* BCM_OL_DEV */
#ifdef WLC_LOW
#ifdef MACOL
	void		*ol;
	bool		radio_active;		/* radio on/off state */
#endif // endif
#ifdef SCANOL
	wlc_scan_info_t *scan_pub;
	uint8		_n_enab;	/* bitmap of 11N + HT support */
	bool		_11h;
	bool		_11d;
	uint8		txcore[MAX_CORE_IDX][2];	/* bitmap of selected core for each Nsts */
#endif /* SCANOL */
#endif /* WLC_LOW */
#ifdef	WL_RXEARLYRC
	void *rc_pkt_head;
#endif // endif
	uint32 need_reinit;	/* flag indicate wl_reinit need to be call */
#ifdef WLC_LOW_ONLY
    uint8 wowl_gpio;
    bool wowl_gpiopol;
#endif // endif
#ifdef WLC_LOW
	wlc_hw_ltecx_info_t *ltecx_hw;
#endif // endif
	uint8	dma_rx_intstatus;
	bool papd_perratedpd_state;
	uint32          boardflags4;            /* More board flags if sromrev >= 12 */

	uint32	xtalfreq;

#ifdef WLC_LOW
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
	bmac_suspend_stats_t* suspend_stats;    /* pointer to stats tracking track bmac suspend */
#endif /* BCMDBG || BCMDBG_DUMP */
#endif /* WLC_LOW */
#ifdef BCM_OL_DEV
	sbpcieregs_t *pcieregs;
#endif // endif
};

#endif /* !_wlc_hw_priv_h_ */
