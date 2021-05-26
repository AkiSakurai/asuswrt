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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_hw_priv.h 788840 2020-07-13 15:10:55Z $
 */

#ifndef _wlc_hw_priv_h_
#define _wlc_hw_priv_h_

#include <typedefs.h>
#include <hnddma.h>
#include <wlc_types.h>
#include <wlc_iocv_types.h>
#include <wlc_dump_reg.h>
#include <d11.h>

/* Interrupt bit error summary.  Don't include I_RU: we refill DMA at other
 * times; and if we run out, constant I_RU interrupts may cause lockup.  We
 * will still get error counts from rx0ovfl.
 */
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RO | I_XU)
/* default software intmasks */
#define	DEF_RXINTMASK	(I_RI)	/**< enable rx int on rxfifo only */

#ifdef PHYTXERR_DUMP
#define MI_PHYTXERR_EN MI_PHYTXERR
#else
#define MI_PHYTXERR_EN 0
#endif /* PHYTXERR_DUMP */

#ifdef WL_PSMX
#define MI_PSMX_EN MI_PSMX
#else
#define MI_PSMX_EN 0
#endif // endif

#define	DEF_MACINTMASK	(MI_TXSTOP | MI_ATIMWINEND | MI_PMQ | \
			 MI_PHYTXERR_EN | MI_DMAINT | MI_TFS | MI_BG_NOISE | \
			 MI_CCA | MI_TO | MI_GP0 | MI_RFDISABLE | MI_HEB | \
			 MI_BT_RFACT_STUCK | MI_BT_PRED_REQ | MI_HWACI_NOTIFY | \
			 MI_BUS_ERROR | MI_PSMX_EN)

#ifdef WL_AIR_IQ
#define DEF_MACINTMASK_X (MIX_AIRIQ | MI_GP0)
#else
#define DEF_MACINTMASK_X MI_GP0
#endif // endif
/* Below interrupts can be delayed and piggybacked with other interrupts.
 * We don't want these interrupts to trigger an isr so we can save CPU & power.
 * They are not enabled, but handled when set in interrupt status
 */
#define	DELAYEDINTMASK  (0 /* | MI_BG_NOISE */)

#define BMAC_PHASE_NORMAL	(0)
#define BMAC_PHASE_1		(1)

/* default d11 core intrcvlazy value to have no delay before receive interrupt */
#ifndef WLC_INTRCVLAZY_DEFAULT
#define WLC_INTRCVLAZY_DEFAULT IRL_DISABLE
#endif // endif

struct si_t;
typedef void (*wlc_core_reset_cb_t) (si_t *sih, uint32 bits, uint32 resetbits);
typedef void (*wlc_core_disable_cb_t) (si_t *sih, uint32 bits);
typedef enum pkteng_status_t {
	PKTENG_IDLE,
	PKTENG_RX_BUSY,
	PKTENG_TX_BUSY
} pkteng_status_type_e;

struct wlc_hwband {
	int		bandtype;		/**< WLC_BAND_2G, WLC_BAND_5G, .. */
	enum wlc_bandunit bandunit;		/**< bandstate[] index */
	uint16		*mhfs;			/**< MHF/MXHF array shadow */
	uint16		mhfs_pad[3];		/**< padding to avoid ROM abandons */
	uint8		bandhw_stf_ss_mode;	/**< HW configured STF type, 0:siso; 1:cdd */
	uint16		CWmin;
	uint16		CWmax;
	uint32          core_flags;

	uint16		phytype;		/**< phytype */
	uint16		phyrev;
	uint16		radioid;
	uint16		radiorev;
	phy_info_t	*pi;			/**< pointer to phy specific information */
	uint8		bw_cap;			/**< Bandwidth bitmask on this band */
	uint16		phy_minor_rev;		/* phy minor rev */
};

typedef struct wlc_hw_btc_info {
	int		mode;		/**< Bluetooth Coexistence mode */
	int		wire;		/**< BTC: 2-wire or 3-wire */
	uint16		flags;		/**< BTC configurations */
	uint32		gpio_mask;	/**< Resolved GPIO Mask */
	uint32		gpio_out;	/**< Resolved GPIO out pins */
	bool		stuck_detected;	/**< stuck BT_RFACT has been detected */
	bool		stuck_war50943;
	bool		bt_active;		/**< bt is in active session */
	uint8		bt_active_asserted_cnt;	/**< 1st bt_active assertion */
	uint8		btcx_aa;		/**< a copy of aa2g:aa5g */
	uint16		bt_shm_addr;
	uint16		bt_period;
	uint8		bt_period_out_of_sync_cnt;
	bool		btc_init_flag;
	uint16		*wlc_btc_params;	/* storage for btc shmem when microcode is down */
	uint16		*wlc_btc_params_fw;	/* storage for firmware-based btc parameters */
	int		task;			/**< BT Coex task: eSCO, BLE scan, etc. */
} wlc_hw_btc_info_t;

typedef struct bmac_suspend_stats {
	uint32 suspend_count;
	uint32 suspended;
	uint32 unsuspended;
	uint32 suspend_start;
	uint32 suspend_end;
	uint32 suspend_max;
} bmac_suspend_stats_t;

#define PKTENG_CACHE_SIZE		16
#define PKTENG_CACHE_IDLE			0
#define PKTENG_CACHE_RUNNING		1

struct pkteng_querry {
	void *p;
	uint32 flags;
	uint32 delay;			/* Inter-packet delay */
	uint32 nframes;			/* Number of frames */
	uint32 length;			/* Packet length */
};

struct wl_pkteng_cache {
	/* Make separate with pointer and count delay */
	struct pkteng_querry pkt_qry[PKTENG_CACHE_SIZE];
	uint16 read_idx;
	uint16 write_idx;
	bool status;
	bool pkteng_running;
};

struct wl_pkteng_async {
	struct wl_timer *pkteng_async_timer;	/* Timer for async mode operation */
	uint32 rx_delay;			/* Delay for pkteng operation */
	uint32 pkt_count;			/* Number of packets  */
	uint32 flags;				/* Mode of Pkteng : tx/rx */
	uint32 delay_cnt;			/* Counter for no. of ms elapsed */
	uint32 ucast_initial_val;		/* Initial value of ucast counter */
	uint32 timer_cb_dur;		/* Value for the timer cb if ota_test is active */
};

typedef struct bmc_params {
	uint8	rxq_in_bm;	    /* 1: rx queues are allocated in BMC, 0: not */
	uint16	rxq0_buf;	    /* number of buffers (in 512 bytes) for rx queue 0
				     * if rxbmmap_is_en == 1, this number indicates
				     * the fifo boundary
				     */
	uint16	rxq1_buf;	    /* number of buffers for rx queue 1 */
	uint8	rxbmmap_is_en;	    /* 1: rx queues are managed as fifo, 0: not */
	uint8	tx_flowctrl_scheme; /* 1: new tx flow control scheme,
				     *	  don't preallocate as many buffers,
				     * 0: old scheme, preallocate
				     */
	uint16	full_thresh;	    /* used when tx_flowctrl_scheme == 0 */
	uint16	minbufs[];
} bmc_params_t;

#if defined(STS_FIFO_RXEN)|| defined(WLC_OFFLOADS_RXSTS)
#ifdef WLCNT
/* Increment prxs stats */
#define PRXSSTATSINCR(_prxs_stats, _stat)  \
	if (_prxs_stats) {  \
		WLCNTINCR((_prxs_stats)->_stat); \
	}
#define PRXSSTATSADD(_prxs_stats, _stat, val)	\
	if (_prxs_stats) {			\
		WLCNTADD((_prxs_stats)->_stat, val);	\
	}
#else
#define PRXSSTATSINCR (prxs_stats, _stat)  do {} while (0)
#define PRXSSTATSADD  (prxs_stats, _stat, val)  do {} while (0)
#endif /* WLCNT */

typedef struct prxs_stats {
	uint32 n_rcvd;
	uint32 n_cons;
	uint32 n_invalid;
	uint32 n_miss;
	uint32 n_cons_intr;
} prxs_stats_t;
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

#define ISSTS_INVALID(sts_buff) \
				((sts_buff)->phystshdr->PhyRxStatusLen == (uint16)-1)
#define STS_MAX_SSSN 256
#define STS_NEXT_SSSN(sssn) MODINC_POW2((sssn), (STS_MAX_SSSN))
#define STS_PREV_SSSN(sssn) MODDEC_POW2((sssn), (STS_MAX_SSSN))
#define STS_SEQNUM_MAX 4096
#define STS_SEQNUM_INVALID (STS_SEQNUM_MAX)
#define IS_STSSEQ_ADVANCED(a, b) \
	((MODSUB_POW2((a), (b), STS_SEQNUM_MAX) > 0) && \
	(MODSUB_POW2((a), (b), STS_SEQNUM_MAX) < (STS_SEQNUM_MAX >> 1)))
#define RXPEN_LIST_IDX0 0
#define RXPEN_LIST_IDX1 1
#define RXPEN_LIST_IDX  RXPEN_LIST_IDX0
#if defined(BCMPCIEDEV)
#define MAX_RXPEN_LIST 2
#else
#define MAX_RXPEN_LIST 1
#endif /* BCMPCIEDEV */

typedef struct rxh_seq_dbg {
	uint16	rd_idx;
	uint16	wr_idx;
	uint16 seq[STS_SEQNUM_MAX];
} rxh_seq_dbg_t;
struct wlc_hw_info {
	wlc_info_t	*wlc;
	wlc_hw_t	*pub;			/**< public API */
	osl_t		*osh;			/**< pointer to os handle */
	uint		unit;			/**< device instance number */
	bool		_piomode;		/**< true if pio mode */
	bool		_p2p;			/**< download p2p ucode */

	/* dma_common_t shadowed in wlc_hw_t
	 * This data are "read" directly but and shall be modified via the following APIs:
	 * - wlc_hw_set_dmacommon()
	 */
	dma_common_t *dmacommon;      /* Global DMA state and info for all DMA channels */

	/* fifo info shadowed in wlc_hw_t.
	 * These data are "read" directly but and shall be modified via the following APIs:
	 * - wlc_hw_set_di()
	 * - wlc_hw_set_aqm_di()
	 * - wlc_hw_set_pio()
	 */
	hnddma_t	*di[NFIFO_EXT_MAX];	/**< hnddma handles, per fifo */
	pio_t		*pio[NFIFO_LEGACY];	/**< pio handlers, per fifo */

	/* version info */
	uint16		vendorid;		/**< PCI vendor id */
	uint16		deviceid;		/**< PCI device id */
	uint		corerev;		/**< core revision */
	uint8		sromrev;		/**< version # of the srom */
	uint16		boardrev;		/**< version # of particular board */
	uint32		boardflags;		/**< Board specific flags from srom */
	uint32		boardflags2;		/**< More board flags if sromrev >= 4 */
	uint32		boardflags4;		/**< More board flags if sromrev >= 12 */
#if defined(WL_EAP_BOARD_RF_5G_FILTER)
	int		board5gfilter;		/**< custom 5G analog filtering, e.g., BCM949408 */
#endif /* WL_EAP_BOARD_RF_5G_FILTER */

	/* interrupt */
	uint32		macintstatus;		/**< bit channel between isr and dpc */
	uint32		macintmask; /**< variable shadowing the contents of the macintmask reg */
	uint32		defmacintmask;		/**< default "on" macintmask value */
	uint32		delayedintmask;		/**< mask of delayed interrupts */
	uint32		tbttenablemask;		/**< mask of tbtt interrupt clients */

	uint32		machwcap;		/**< MAC capabilities (corerev >= 13) */
	uint32		machwcap1;		/**< information about the hardware
						capabilities of the MAC core (corerev >= 14)
						*/
	uint32		num_mac_chains;		/**< Cache number of supported mac chains */
	uint16		ucode_dbgsel;		/**< dbgsel for ucode debug(config gpio) */
	bool		ucode_loaded;		/**< TRUE after ucode downloaded */

	wlc_hw_btc_info_t *btc;

	si_t		*sih;			/**< SB handle (cookie for siutils calls) */
	char		*vars;			/**< "environment" name=value */
	uint		vars_size;		/**< size of vars, free vars on detach */
	d11regs_t	*regs;			/**< pointer to device registers */
	const d11regdefs_t *regoffsets;
	void		*physhim;		/**< phy shim layer handler */
	void		*phy_sh;		/**< pointer to shared phy state */
	wlc_hwband_t	*band;			/**< pointer to active per-band state */
	wlc_hwband_t	*bandstate[MAXBANDS];	/**< per-band state (one per phy/radio) */
	uint16		bmac_phytxant;		/**< cache of high phytxant state */
	bool		shortslot;		/**< currently using 11g ShortSlot timing */
	uint16		SRL;			/**< 802.11 dot11ShortRetryLimit */
	uint16		LRL;			/**< 802.11 dot11LongRetryLimit */
	uint16		SFBL;			/**< Short Frame Rate Fallback Limit */
	uint16		LFBL;			/**< Long Frame Rate Fallback Limit */

	bool		up;			/**< d11 hardware up and running */
	uint		now;			/**< # elapsed seconds */
	uint		unused;
	uint		_bandmask;		/**< bitmask of bands supported */
	chanspec_t	chanspec;		/**< bmac chanspec shadow */
	uint		**txavail;		/* # tx descriptors available */

	uint16		*xmtfifo_sz;		/**< fifo size in 256B for each xmt fifo */

	mbool		pllreq;			/**< pll requests to keep PLL on */

	uint8	suspended_fifos[TX_FIFO_BITMAP_SIZE_MAX]; /**< Which TX fifo to remain awake for */
	uint8		suspended_fifo_count;	/**< Number of suspended FIFOs */
	uint32		maccontrol;		/**< Cached value of maccontrol */
	uint		mac_suspend_depth;	/**< current depth of mac_suspend levels */
	uint32		wake_override;		/**< Various conditions to force MAC to WAKE mode */
	uint32		mute_override;		/**< Prevent ucode from sending beacons */
	struct		ether_addr etheraddr;	/**< currently configured ethernet address */
	uint32		led_gpio_mask;		/**< LED GPIO Mask */
	bool		noreset;		/**< true= do not reset hw, used by WLC_OUT */
	bool		forcefastclk;	/**< true if the h/w is forcing the use of fast clk */
	bool		clk;			/**< core is out of reset and has clock */
	bool		sbclk;			/**< sb has clock */
	bool		phyclk;			/**< phy is out of reset and has clock */
	bool		dma_lpbk;		/**< core is in DMA loopback */
	uint16		fastpwrup_dly;		/**< time in us needed to bring up d11 fast clock */

#ifdef WLLED
	bmac_led_info_t	*ledh;			/**< pointer to led specific information */
#endif // endif
	uint8		hw_stf_ss_opmode;	/**< STF single stream operation mode */
	bool		btclock_tune_war;	/**< workaoround to stablilize bt clock  */
	uint16		noise_metric;		/**< To enable/disable knoise measurement. */

	uint32		intrcvlazy;		/**< D11 core INTRCVLAZY register setting */
	uint		p2p_shm_base;		/**< M_P2P_BSS_BLK SHM base byte offset */
	uint		cca_shm_base;		/**< M_CCA_STATS_BLK SHM base byte offset */
	uint16		macstat1_shm_base;	/**< M_UCODE_MACSTAT1 SHM base byte offset */

	/* MHF2_SKIP_ADJTSF ucode host flag manipulation */
	uint32		skip_adjtsf; /**< bitvec, IDs of users requesting to skip ucode TSF adj. */
	/* MCTL_AP maccontrol register bit manipulation when AP_ACTIVE() */
	uint32		mute_ap;	/**< bitvec, IDs of users requesting to stop the AP func. */

	/* variables to store BT switch state/override settings */
	int8   btswitch_ovrd_state;
	uint8	antswctl2g;	/**< extlna switch control read from SROM (2G) */
	uint8	antswctl5g;	/**< extlna switch control read from SROM (5G) */
	bool            reinit;
	uint32	vcoFreq_4360_pcie2_war; /* changing the avb vcoFreq */
    uint8 sr_vsdb_force;          /* for test/debug purpose */
    bool  sr_vsdb_active;         /* '1' when higher MAC activated srvsdb operation */
#ifdef	WL_RXEARLYRC
	void *rc_pkt_head;
#endif // endif
	uint32 need_reinit;	/**< flag indicate wl_reinit need to be call */
	wlc_iocv_info_t *iocvi;		/**< iocv module handle */
	wlc_dump_reg_info_t *dump;		/**< dump registry */

#ifdef WOWL_GPIO
	uint8   wowl_gpio;
	bool    wowl_gpiopol;
#endif /* WOWL_GPIO */
	uint8   dma_rx_intstatus;
	wlc_core_reset_cb_t mac_core_reset_fn;
	wlc_core_disable_cb_t mac_core_disable_fn;
	uint8   core1_mimo_reset;
#if defined(WL_PSMX)
	uint32		maccontrol_x;		/* Cached value of maccontrol_psmx */
	uint		macx_suspend_depth;	/* current depth of mac_suspend levels for psmx */
#endif /* WL_PSMX */
	uint    shmphymode;             /* shared memory phymode update */
	bmac_suspend_stats_t* suspend_stats; /**< pointer to stats tracking track bmac suspend */
	uint	nfifo_inuse;			/* # of FIFOs that are in use runtime
						 * Based on configuration of certain features like
						 * WL_MU_TX this can change
						 */
	uint		*txavail_aqm[NFIFO_EXT_MAX];		/* # aqm descriptors available */
	hnddma_t	*aqm_di[NFIFO_EXT_MAX]; /* hnddam handles per aqm fifo */
	char    vars_table_accessor[10];
	uint32  pc_flags;               /**<  patch control flags used to apply on every d11reset */
	uint32  pc_mask;                /**<  patch control mask for pc_flags */
	const shmdefs_t *shmdefs;       /* Pointer to Auto-SHM strucutre */
	wlc_rx_stall_info_t *rx_stall;  /**< Rx DMA Stall check */
	wlc_txs_hist_t *txs_hist;       /**< TxStatus history */
	uint16		tx_inhibit_tout;	/* Minimum GPIO based Tx inhibit duration */
	bool hdrconv_mode;              /* HW header conversion mode */

	/* This field indicates if ucode woken after HPS bit is cleared.
	 * if awake_hps_0 == TRUE indicates ucode is awake because of hps == 0
	 * else if awake_hps_0 == FALSE indicates unknown wake state of ucode
	 */
	bool awake_hps_0;

	struct wl_timer *pkteng_timer;		/**< timer for packet engine */
	pkteng_status_type_e pkteng_status;
	struct wl_pkteng_cache *pkteng_cache;
	bool	vasip_loaded;			/* TRUE after vasip code downloaded */
	bool    smc_loaded;                     /* TRUE after smc code downloaded */
	bool    wareng_loaded;          /* TRUE after WAR-engine code downloaded */
	uint8 corerev_minor;		/**< core minor revision */
	uint16 classify_fifo_suspend_cnt;	/* For tracking suspend/resume of Classify FIFO */
	prephy_info_t *prepi;
	uint32 phy_cap;		/* capabilities used for bmac attach */
	uint32 bmac_phase;	/* bmac states: 0: normal-attach, 1: phase1-attach */
	struct wl_timer *srtimer;	/* timer for save restore wakeup */
	struct wl_pkteng_async *pkteng_async;
	const bmc_params_t *bmc_params;
#if defined(WL_PSMR1)
	uint32		maccontrol_r1;		/* Cached value of maccontrol_psmr1 */
	uint		macr1_suspend_depth;	/* current depth of mac_suspend levels for psmx */
	bool		_psmr1;
#endif /* WL_PSMR1 */
	uint16		tpl_sz;		/* template area size in Byte */
	uint16		*vasip_addr;     /* vasip base address */
	uint16		*vasip_addr_int; /* vasip base address accessed internally in the chip */
#if defined(STS_FIFO_RXEN)|| defined(WLC_OFFLOADS_RXSTS)
	sts_buff_t      *sts_buff_t_array; /**< contains sts array that will make up an mpool */
	uint             sts_buff_t_alloc_sz;	/**< #bytes allocated for sts array */
	uint8           *sts_phyrx_va_non_aligned; /**< contains phyrx array */
	uint             sts_phyrx_alloc_sz;	/**< #bytes allocated for phyrx array */
	struct sts_buff_pool sts_mempool; /**< used by DMA subsystem for rxpost buffers */
#ifdef BCM_SECURE_DMA
	dmaaddr_t pap;
	uint32    _alloced;
#endif /* BCM_SECURE_DMA */
	rx_list_t rxpen_list[MAX_RXPEN_LIST];
	uint16 curr_seq[MAX_RXPEN_LIST];
	uint16 cons_seq[MAX_RXPEN_LIST];
	uint16 prxs_rd_idx;
	uint16 prxs_wr_idx;
	rx_list_t  rx_sts_list; /**< linked list, oldest (non-consumed) status being the head */
	bool sts_fifo_intr;
	prxs_stats_t  prxs_stats;
	rxh_seq_dbg_t *rxh_seq_dbg;
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */
	uint32 d11war_flags;	/* flags to store d11 WARS */
	bool is_pcielink_slowspeed;
	uint32 sw_macintstatus; /* fake HW interrupt set by driver */
	uint16	bmc_maxbufs;	/* max bufs based on fifo size */
	uint16	bmc_nbufs;	/* number of bufs per fifo */
};

#endif /* !_wlc_hw_priv_h_ */
