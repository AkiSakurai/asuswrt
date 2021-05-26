/*
 * Required functions exported by the PHY module (phy-dependent)
 * to common (os-independent) driver code.
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
 * $Id: wlc_phy_hal.h 786104 2020-04-16 16:40:59Z $
 */

#ifndef _wlc_phy_h_
#define _wlc_phy_h_

#include <typedefs.h>
#include <wlioctl.h>
#include <bcmutils.h>
#include <osl.h>
#include <siutils.h>
#include <d11.h>
#include <wlc_phy_shim.h>
#include <wlc_ppr.h>

typedef struct phy_info phy_info_t;
typedef struct prephy_info prephy_info_t;

#define PHY_TPC_HW_OFF		FALSE
#define PHY_TPC_HW_ON		TRUE

#define VARS_ACCESSOR_SZ 10 /* Vars table accessor size for dual PHY chips */

#define PHY_MAX_CORES		4	/* max number of cores supported by PHY HAL */

#define WLC_TXPWR_DB_FACTOR	4	/* most txpower parameters are in 1/4 dB unit */

/* iovar table */
enum {
	/* ==========================================
	 * unified phy iovar, independent of PHYTYPE
	 * ==========================================
	 */
	IOV_PHYHAL_MSG = 300,
	IOV_PHY_WATCHDOG,
	IOV_FAST_TIMER,
	IOV_SLOW_TIMER,
	IOV_GLACIAL_TIMER,
	IOV_PHY_FIXED_NOISE,
	IOV_PHYNOISE_POLL,
	IOV_PHY_SPURAVOID,
	IOV_CARRIER_SUPPRESS,
	IOV_ACI_EXIT_CHECK_PERIOD,
	IOV_NUM_STREAM,
	IOV_PHYWREG_LIMIT,
	IOV_MIN_TXPOWER,
	IOV_PHY_MUTED,
	IOV_IQ_IMBALANCE_METRIC_DATA,
	IOV_IQ_IMBALANCE_METRIC,
	IOV_IQ_IMBALANCE_METRIC_PASS,
	IOV_PHY_SAMPLE_COLLECT,
	IOV_PHY_SAMPLE_COLLECT_GAIN_ADJUST,
	IOV_PHY_SAMPLE_COLLECT_GAIN_INDEX,
	IOV_PHY_SAMPLE_DATA,
	IOV_PHY_TXPWRCTRL,
	IOV_PHY_RESETCCA,
	IOV_PHY_GLITCHK,
	IOV_PHY_NOISE_UP,
	IOV_PHY_NOISE_DWN,
	IOV_PHY_TXPWRINDEX,
	IOV_PHY_AVGTSSI_REG,
	IOV_PHY_IDLETSSI_REG,
	IOV_PHY_CAL_DISABLE,
	IOV_PHY_FORCECAL,
	IOV_PHY_TXRX_CHAIN,
	IOV_PHY_BPHY_EVM,
	IOV_PHY_BPHY_RFCS,
	IOV_PHY_ENABLERXCORE,
	IOV_PHY_EST_TONEPWR,
	IOV_PHY_GPIOSEL,
	IOV_PHY_5G_PWRGAIN,
	IOV_PHY_RFSEQ,
	IOV_PHY_SCRAMINIT,
	IOV_PHY_TEMPSENSE,
	IOV_PHY_VBATSENSE,
	IOV_PHY_TEST_TSSI,
	IOV_PHY_TEST_TSSI_OFFS,
	IOV_PHY_TEST_IDLETSSI,
	IOV_PHY_TX_TONE_SYMM,
	IOV_PHY_TX_TONE_HZ,
	IOV_PHY_MAXP,
	IOV_PATRIM,
	IOV_POVARS,
	IOV_RPCALVARS,
	IOV_PHYCAL_STATE,
	IOV_PHY_TXIQCC,
	IOV_PHY_TXLOCC,
	IOV_PHYCAL_TEMPDELTA,
	IOV_PHY_PAPD_DEBUG,
	IOV_PHY_ACTIVECAL,
	IOV_NOISE_MEASURE,
	IOV_PHY_IQLOCALIDX,
	IOV_PHY_LOWPOWER_BEACON_MODE,
	IOV_PHY_SROM_TEMPSENSE,
	IOV_PHY_GAIN_CAL_TEMP,
	IOV_PHY_SETRPTBL,
	IOV_PHY_FORCEIMPBF,
	IOV_PHY_FORCESTEER,
	IOV_PHY_TXFILTER_SM_OVERRIDE,
	IOV_PHY_DEAF,
	IOV_NTD_GDS_LOWTXPWR,
	IOV_LCNPHY_SAMP_CAP,
	IOV_PHY_SUBBAND5GVER,
	IOV_PHY_FCBSINIT,
	IOV_PHY_FCBS,
	IOV_PHY_FCBSARM,
	IOV_PHY_FCBSEXIT,
	IOV_LCNPHY_TXPWRCLAMP_DIS,
	IOV_LCNPHY_TXPWRCLAMP_OFDM,
	IOV_LCNPHY_TXPWRCLAMP_CCK,
	IOV_PHY_DEBUG_CMD,
	IOV_PHY_FORCE_FDIQI,
	IOV_PHY_BBMULT,
	IOV_LNLDO2,
	IOV_PAVARS2,
	IOV_PHY_DSSF,
	IOV_EDCRS,
	IOV_DYNAMIC_ED_THRESH_EN,
	IOV_DYNAMIC_ED_THRESH_ACPHY_NVRAM,
	IOV_DED_SETUP,
	IOV_PHY_FORCECAL_OBT,
	IOV_PHY_DYN_ML,
	IOV_PHY_ACI_NAMS,
	IOV_PHY_RSSI_GAIN_CAL_TEMP,
	IOV_PHY_MAC_TRIGGERED_SAMPLE_COLLECT,
	IOV_PHY_MAC_TRIGGERED_SAMPLE_DATA,
	IOV_PHY_VCOCAL,
	IOV_PHY_RXGAININDEX,
	IOV_PHY_AUXPGA,
	IOV_HIRSSI_PERIOD,
	IOV_HIRSSI_EN,
	IOV_HIRSSI_BYP_RSSI,
	IOV_HIRSSI_RES_RSSI,
	IOV_HIRSSI_BYP_CNT,
	IOV_HIRSSI_RES_CNT,
	IOV_HIRSSI_STATUS,
	IOV_PHY_MAC_TRIGGERD_SAMPLE_COLLECT,
	IOV_PHY_MAC_TRIGGERD_SAMPLE_DATA,
	IOV_PHY_AFE_OVERRIDE,
	IOV_CAL_PERIOD,
	IOV_SROM_REV,
	IOV_PHY_TXCAL_GAINSWEEP,
	IOV_PHY_TXCAL_GAINSWEEP_MEAS,
	IOV_PHY_TXCAL_PWR_TSSI_TBL,
	IOV_PHY_TXCAL_APPLY_PWR_TSSI_TBL,
	IOV_PHY_READ_ESTPWR_LUT,
	IOV_PHY_DUMP_PAGE,
	IOV_PHY_DUMP_ENTRY,
	IOV_PHY_MASTER_OVERRIDE,
	IOV_PAPD_EPS_OFFSET,
	IOV_TPC_AV,
	IOV_TPC_VMID,
	IOV_PHY_TXCALVER, /* version for handing different structrues of txcal */
	IOV_PHY_BPHYMRC,
	IOV_PHY_BSSCOLOR,
	IOV_TXPWR_CAP,
	IOV_PHY_SNIFFER_ALIGN,
	IOV_RPCALPHASEVARS,
	IOV_PHY_LAST	/* insert before this one */
};

/* forward declaration */
typedef struct shared_phy shared_phy_t;

/* Public phy info structure */
struct phy_pub;
typedef struct phy_pub wlc_phy_t;

typedef struct shared_phy_params {
	void 	*osh;		/* pointer to OS handle */
	si_t	*sih;		/* si handle (cookie for siutils calls) */
	void	*physhim;	/* phy <-> wl shim layer for wlapi */
	uint	unit;		/* device instance number */
	uint	corerev;	/* core revision */
	uint	bustype;	/* SI_BUS, PCI_BUS  */
	uint	buscorerev; 	/* buscore rev */
	char	*vars;		/* phy attach time only copy of vars */
	uint16	vid;		/* vendorid */
	uint16	did;		/* deviceid */
	uint	chip;		/* chip number */
	uint	chiprev;	/* chip revision */
	uint	chippkg;	/* chip package option */
	uint	sromrev;	/* srom revision */
	uint	boardtype;	/* board type */
	uint	boardrev;	/* board revision */
	uint	boardvendor;	/* board vendor */
	uint32	boardflags;	/* board specific flags from srom */
	uint32	boardflags2;	/* more board flags if sromrev >= 4 */
	uint32	boardflags4;	/* more board flags if sromrev >= 12 */
#if defined(WL_EAP_BOARD_RF_5G_FILTER)
	int	board5gfilter;	/* custom 5G analog filtering, e.g., BCM949408 */
#endif /* WL_EAP_BOARD_RF_5G_FILTER */
	char	vars_table_accessor[VARS_ACCESSOR_SZ];
} shared_phy_params_t;

/* parameter structure for wlc_phy_txpower_core_offset_get/set functions */
struct phy_txcore_pwr_offsets {
	int8 offset[PHY_MAX_CORES];	/* quarter dBm signed offset for each chain */
};

/* phy_tx_power_t.flags bits */
#define WL_TX_POWER_F_ENABLED	1
#define WL_TX_POWER_F_HW		2
#define WL_TX_POWER_F_MIMO		4
#define WL_TX_POWER_F_SISO		8
#define WL_TX_POWER_F_HT		0x10
#ifdef WL11AX
typedef struct {
	uint8 frame_type;
	uint8 he_format;
	bool not_sounding;
	bool preamble;
	uint8 mcs_nss;
	bool stbc;
	uint8 n_pwr;
	uint8 n_user;
	bool mu;
	uint8 core_mask;
	uint8 ant_cfg;
	uint8 pktbw;
	uint8 subband;
	uint8 partial_ofdma_subband;
	bool dynBW_present;
	bool dynBW_mode;
	uint8 scrambler;
	uint32 cfo_comp_val;
	bool cfo_comp_en;
	bool has_trigger_info;
	uint8 cp_ltf_size;
	bool heltf_mask;
	uint8 num_heltf;
	bool ldpc_extra;
	uint8 afactor;
	bool disamb;
	bool doppler;
	bool ldpc;
	uint8 mcs;
	bool dcm;
	uint8 nss_start_pos;
	uint8 nss;
	uint8 pwr_offset[4];
	uint8 bfm_mu[16];
	uint8 usridx_mu[16];
	uint8 ruidx_mu[16];
	uint8 mcs_nss_mu[16];
	bool ldpc_mu[16];
	uint16 aid_mu[16];
	uint8 rl_backoff;
} phy_txctrl_info_t;
#endif /* WL11AX */
typedef struct {
	uint32 flags;
	chanspec_t chanspec;		/* txpwr report for this channel */
	chanspec_t local_chanspec;	/* channel on which we are associated */
	uint8 rf_cores;				/* count of RF Cores being reported */
	uint8 display_core;			/* the displayed core in curpower */
	uint8 est_Pout[4];			/* Latest tx power out estimate per RF chain */
	uint8 est_Pout_act[4]; /* Latest tx power out estimate per RF chain w/o adjustment */
	uint8 est_Pout_cck;			/* Latest CCK tx power out estimate */
	uint8 tx_power_max[4];		/* Maximum target power among all rates */
	/* XXX this parameter is no longer being populated because the data structures being
	 * indexed were removed as part of the PPR_OPT overhaul. This can probably be deleted.
	 */
	uint tx_power_max_rate_ind[4];		/* Index of the rate with the max target power */
	ppr_t *ppr_board_limits;
	ppr_t *ppr_target_powers;
#ifdef WL11AX
	phy_txctrl_info_t txctrl_info;
#endif /* WL11AX */
#ifdef WL_SARLIMIT
	int8 SARLIMIT[PHY_MAX_CORES];
#endif // endif
} phy_tx_power_t;

#define NUM_SUBBANDS_FOR_AVVMID 5
typedef struct wlc_phy_avvmid {
	uint8   Av;
	uint8   Vmid;
} wlc_phy_avvmid_t;

typedef struct wlc_phy_avvmid_bands {
	wlc_phy_avvmid_t avvmid[PHY_MAX_CORES][NUM_SUBBANDS_FOR_AVVMID];
} wlc_phy_avvmid_txcal_t;

#ifdef TXPWR_TIMING
extern uint32 hnd_time_us(void);
extern uint wlc_phy_txpower_limit_set_time;
#endif // endif

#define WLC_PHY_OCL_INFO(args)    WL_INFORM(args)

/*
 * The PHY APIs below are implemented by the PHY infrastructure module
 */
/* attach/detach */
extern shared_phy_t *wlc_phy_shared_attach(shared_phy_params_t *shp);
extern void  wlc_phy_shared_detach(shared_phy_t *phy_sh);
extern void wlc_phy_read_patrim_srom(phy_info_t *pi, int16* start, uint16 len);

#ifdef WFD_PHY_LL
extern void wlc_phy_wfdll_chan_active(phy_info_t *pi, bool chan_active);
#endif // endif

/* Calibration Mgmt */
extern void wlc_phy_cal_perical(phy_info_t *pi, uint8 reason);

/*
 * The PHY APIs below are implemented by the PHY modules. Each module
 * may have the following functions:
 *	attach
 *	init
 *	reset
 *	detach
 *	and other configuration and action functions	.
 * The function declaration corresponding to a module will be moved to
 * phy_<module>_api.h when it is adapted to the new PHY architecture.
 */
/* PHY Module (Init, Channel Configuration) */

/* flow */

/* AFE Module (Init, Channel Configuration) */

/* Power Control Module */
int wlc_phy_get_est_pout(phy_info_t *pi, uint8* est_Pout, uint8* est_Pout_adj,
	uint8* est_Pout_cck);
#ifdef WL11AX
int wlc_phy_get_txctrl_mu(phy_info_t *pi, phy_txctrl_info_t* txctrl_info);
#endif /* WL11AX */
/* Calibration caching Module (regular caching, per channel, smart cal) */
extern void wlc_phy_avvmid_txcal(phy_info_t *pi, wlc_phy_avvmid_txcal_t *val, bool set);
/* Calibration caching Module (regular caching, per channel, smart cal) */

extern const uint8 * wlc_phy_get_ofdm_rate_lookup(void);
extern uint32 wlc_phy_cap_get(phy_info_t *pi);
/* Test Module (pkt engine, Sample capture, phy/radio reg dumps) */

/* Configuration, Utilities */
extern void wlc_phy_preamble_override_set(phy_info_t *pi, int8 override);

#ifdef ATE_BUILD
void wlc_ate_gpiosel_disable(phy_info_t *pi);
#endif // endif

extern uint8 wlc_phy_get_bfe_ndp_recvstreams(phy_info_t *pi);

/* *************************************************************** */
/* *************************************************************** */
/*                     Functions shared by HT and N PHY                                          */
/* *************************************************************** */
/* *************************************************************** */
int wlc_phy_acimode_noisemode_reset(phy_info_t *pi, chanspec_t chanspec,
	bool clear_aci_state, bool clear_noise_state, bool disassoc);
int wlc_phy_interference_set(phy_info_t *pi, bool init);

/* *************************** */
/* ********** NON-AC ********** */
/* *************************** */
/* Calibration Mgmt */
extern void wlc_phy_cal_mode(phy_info_t *pi, uint mode);

#endif	/* _wlc_phy_h_ */
