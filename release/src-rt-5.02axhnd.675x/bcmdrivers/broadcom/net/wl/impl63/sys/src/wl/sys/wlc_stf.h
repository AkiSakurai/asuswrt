/*
 * Code that controls the antenna/core/chain
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_stf.h 779517 2019-10-01 14:48:46Z $
 */

#ifndef _wlc_stf_h_
#define _wlc_stf_h_

#ifdef WL_STF_ARBITRATOR
#include <wlc_stf_arb.h>
#endif // endif
#ifdef WL_MIMOPS_CFG
#include <wlc_stf_mimops.h>
#endif // endif
#include <wlc_types.h>

#define AUTO_SPATIAL_EXPANSION	-1
#define MIN_SPATIAL_EXPANSION	0
#define MAX_SPATIAL_EXPANSION	1

#define PWRTHROTTLE_CHAIN	1
#define PWRTHROTTLE_DUTY_CYCLE	2
#define OCL_RSSI_DELTA				5	/* hysteresis rssi delta */

/* bit0: no txchain downgrade in thermal throttle */
#define THERMAL_THROTTLE_BYPASS_TXCHAIN   (1 << 0)

enum txcore_index {
	CCK_IDX = 0,	/* CCK txcore index */
	OFDM_IDX,
	NSTS1_IDX,
	NSTS2_IDX,
	NSTS3_IDX,
	NSTS4_IDX,
	MAX_CORE_IDX
};

#define ONE_CHAIN_CORE0             0x1
#define ONE_CHAIN_CORE1             0x2

#define WLC_TXCHAIN_ID_USR		0 /* user setting */
#define WLC_TXCHAIN_ID_TEMPSENSE	1 /* chain mask for temperature sensor */
#define WLC_TXCHAIN_ID_PWRTHROTTLE	2 /* chain mask for pwr throttle feature for 43224 board */
#define WLC_TXCHAIN_ID_PWR_LIMIT	3 /* chain mask for tx power offsets */
#define WLC_TXCHAIN_ID_BTCOEX		4 /* chain mask for BT coex */
#define WLC_TXCHAIN_ID_MWS		5 /* chain mask for mws */
#define WLC_TXCHAIN_ID_COUNT		6 /* number of txchain_subval masks */

#define MWS_ANTMAP_DEFAULT	0
#define WL_STF_CONFIG_CHAINS_DISABLED -1
#define WL_STF_CHAINS_NOT_CONFIGURED -1

/* Defines for request entry states; use only defines once */
#define WLC_STF_ARBITRATOR_REQ_STATE_RXTX_INACTIVE  0x00
#define WLC_STF_ARBITRATOR_REQ_STATE_RX_ACTIVE      0x01
#define WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE      0x02
#define WLC_STF_ARBITRATOR_REQ_STATE_RXTX_ACTIVE    0x03

#define WLC_BCNPRS_MIN_TXPWR	(10) /* Min txpwr for beacon and probe response in dBm */

struct wlc_stf_arb_mps_info {
	wlc_info_t  *wlc;
	int cfgh_hwcfg;
	int cfgh_arb;
	int cfgh_mps;
};

typedef struct wlc_stf_ocl_info wlc_stf_ocl_info_t;

/* anything affects the single/dual streams/antenna operation */
struct wlc_stf {
	uint8	hw_txchain;		/**< HW txchain bitmap cfg */
	uint8	txchain;		/**< txchain bitmap being used */
	uint8	op_txstreams;		/**< number of operating txchains being used */
	uint8	txstreams;		/**< number of txchains being used */

	uint8	hw_rxchain;		/**< HW rxchain bitmap cfg */
	uint8	op_rxstreams;		/**< number of operating rxchains being used */
	uint8	rxchain;		/**< rxchain bitmap being used */
	uint8	rxstreams;		/**< number of rxchains being used */

	uint8 	ant_rx_ovr;		/**< rx antenna override */
	int8	txant;			/**< userTx antenna setting */
	uint16	phytxant;		/**< phyTx antenna setting in txheader */

	uint8	ss_opmode;		/**< singlestream Operational mode, 0:siso; 1:cdd */
	bool	ss_algosel_auto;	/**< if TRUE, use wlc->stf->ss_algo_channel; */
					/* else use wlc->band->stf->ss_mode_band; */
	uint16	ss_algo_channel; /**< ss based on per-channel algo: 0: SISO, 1: CDD 2: STBC */
	uint8   no_cddstbc;		/**< stf override, 1: no CDD (or STBC) allowed */

	uint8   rxchain_restore_delay;	/**< delay time to restore default rxchain */

	int8	ldpc;			/**< ON/OFF ldpc RX supported */
	int8	ldpc_tx;		/**< AUTO/ON/OFF ldpc TX supported */
	uint8	txcore_idx;		/**< bitmap of selected core for each Nsts */
	uint8	txcore[MAX_CORE_IDX][2];	/**< bitmap of selected core for each Nsts */
	uint8	txcore_override[MAX_CORE_IDX];	/**< override of txcore for each Nsts */
	int8	spatialpolicy;
	int8	spatialpolicy_pending;
	uint16	shmem_base;
	ppr_t	*txpwr_ctl;
	ppr_ru_t	*ru_txpwr_ctl;

	bool	tempsense_disable;	/**< disable periodic tempsense check */
	uint	tempsense_period;	/**< period to poll tempsense */
	uint 	tempsense_lasttime;
	uint8   siso_tx;		/**< use SISO and limit rates to MSC 0-7 */

	uint	ipaon;			/**< IPA on/off, assume it's the same for 2g and 5g */
	int	pwr_throttle;		/**< enable the feature */
	uint8	throttle_state;		/**< hwpwr/temp throttle state */
	uint8   tx_duty_cycle_pwr;	/**< maximum allowed duty cycle under pwr throttle */
	uint16	tx_duty_cycle_ofdm;	/**< maximum allowed duty cycle for OFDM */
	uint16	tx_duty_cycle_cck;	/**< maximum allowed duty cycle for CCK */
	uint8	pwr_throttle_mask;	/**< core to enable/disable with thermal throttle kick in */
	uint8	pwr_throttle_test;	/**< for testing */
	uint8	txchain_pending;	/**< pending value of txchain */
	bool	rssi_pwrdn_disable;	/**< disable rssi power down feature */
	uint8	txchain_subval[WLC_TXCHAIN_ID_COUNT];	/**< set of txchain enable masks */
	int8	spatial_mode_config[SPATIAL_MODE_MAX_IDX]; /**< setting for each band or sub-band */
	wl_txchain_pwr_offsets_t txchain_pwr_offsets;
	int8 	onechain;		/**< disable 1 TX/RS */
	uint8	pwrthrottle_config;
	uint32	pwrthrottle_pin;	/**< contain the bit map of pin use for pwr throttle */
	uint16	shm_rt_txpwroff_pos;

	uint16	tx_duty_cycle_ofdm_40_5g;	/**< max allowed duty cycle for 5g 40Mhz OFDM */
	uint16	tx_duty_cycle_thresh_40_5g;	/* max rate in 500K to apply 5g 40Mhz duty cycle */
	uint16	tx_duty_cycle_ofdm_80_5g;	/**< max allowed duty cycle for 5g 40Mhz OFDM */
	uint16	tx_duty_cycle_thresh_80_5g;	/* max rate in 500K to apply 5g 40Mhz duty cycle */
	ppr_t	*txpwr_ctl_qdbm;

	/* used by OLPC to register for callback if stored stf state */
	/* changes during phy calibration */
	wlc_stf_txchain_evt_notify txchain_perrate_state_modify;
	uint8	max_offset;
	/* Country code allows txbf */
	uint8	allow_txbf;
	uint8   tx_duty_cycle_thermal;  /**< maximum allowed duty cycle under thermal throttle */
	uint8   txstream_value;
	uint8	hw_txchain_cap;	/**< copy of the nvram for reuse while upgrade/downgrade */
	uint8	hw_rxchain_cap;	/**< copy of the nvram for reuse while upgrade/downgrade */
	uint8	coremask_override;
	uint8	channel_bonding_cores; /**< 80p80/160MHz runs using 2 PHY cores, not tx/rx-chain */

	/* previous override of txcore for each Nsts */
	uint8   prev_txcore_override[MAX_CORE_IDX];

	int8    nap_rssi_delta;
	int8    nap_rssi_threshold;
	uint8	new_rxchain;
	uint8	mimops_send_cnt;
	uint8	mimops_acked_cnt;
	wlc_stf_ocl_info_t	*ocl_info;
	wlc_stf_arb_t    *arb;
	wlc_stf_nss_request_q_t    *arb_req_q;
	uint8   bw_update_in_progress;
	chanspec_t    pending_bw;
	wlc_stf_arb_mps_info_t    *arb_mps_info_hndl;
	uint8	tx_duty_cycle_max;	/**< maximum allowed duty cycle */
	bool bt_rxchain_pending;

	/* sw tx/rx chain params */
	uint8  sw_txchain_mask;
	uint8  sw_rxchain_mask;
	bool   sr13_en_sw_txrxchain_mask;      /* Enable/disable bit for sw chain mask */
	uint8  pending_opstreams;	/* pending op_txstreams setting */
	bool   no_rate_init_yet;	/* to delay ratesel init */

	/* extra back off power for bcn/prs */
	uint8	bcnprs_txpwr_offset;	/* user specified txpwr for bcn/prs in dB */

	/* features set for thermal throttle */
	uint32   thermal_throttle_features;
	bool     core3_p1c; /* +1 core chain mask */
};

extern int wlc_stf_attach(wlc_info_t* wlc);
extern void wlc_stf_detach(wlc_info_t* wlc);
extern void wlc_stf_chain_init(wlc_info_t *wlc);
extern void wlc_stf_txchain_set_complete(wlc_info_t *wlc);
#ifdef WL11AC
extern void wlc_stf_chanspec_upd(wlc_info_t *wlc);
#endif /* WL11AC */
extern void wlc_stf_tempsense_upd(wlc_info_t *wlc);
extern void wlc_stf_ss_algo_channel_get(wlc_info_t *wlc, uint16 *ss_algo_channel,
	chanspec_t chanspec);
extern void wlc_stf_ss_update(wlc_info_t *wlc, wlcband_t *band);
extern void wlc_stf_phy_txant_upd(wlc_info_t *wlc);
extern int wlc_stf_txchain_set(wlc_info_t* wlc, int32 int_val, bool force, uint16 id);
extern int wlc_stf_txchain_subval_get(wlc_info_t* wlc, uint id, uint *txchain);
extern int wlc_stf_rxchain_set(wlc_info_t* wlc, int32 int_val, bool update);
extern bool wlc_stf_rxchain_ishwdef(wlc_info_t* wlc);
extern bool wlc_stf_txchain_ishwdef(wlc_info_t* wlc);
extern bool wlc_stf_stbc_rx_set(wlc_info_t* wlc, int32 int_val);
extern uint8 wlc_stf_txchain_get(wlc_info_t *wlc, ratespec_t rspec);
extern uint8 wlc_stf_txchain_get_cap(wlc_info_t *wlc);
extern uint8 wlc_stf_txcore_get(wlc_info_t *wlc, ratespec_t rspec);
extern void wlc_stf_spatialpolicy_set_complete(wlc_info_t *wlc);
extern void wlc_stf_txcore_shmem_write(wlc_info_t *wlc, bool forcewr);
extern uint16 wlc_stf_spatial_expansion_get(wlc_info_t *wlc, ratespec_t rspec);
extern uint8 wlc_stf_get_pwrperrate(wlc_info_t *wlc, ratespec_t rspec,
	uint16 spatial_map);
extern int wlc_stf_get_204080_pwrs(wlc_info_t *wlc, ratespec_t rspec, txpwr204080_t* pwrs,
        wl_tx_chains_t txbf_chains);
extern int wlc_stf_get_204080_sdm_pwrs(wlc_info_t *wlc, ratespec_t rspec, txpwr204080_t* pwrs);
extern void wlc_set_pwrthrottle_config(wlc_info_t *wlc);
extern int wlc_stf_duty_cycle_set(wlc_info_t *wlc, int duty_cycle, bool isOFDM, bool writeToShm);
extern void wlc_stf_chain_active_set(wlc_info_t *wlc, uint8 active_chains);
#ifdef RXCHAIN_PWRSAVE
extern uint8 wlc_stf_enter_rxchain_pwrsave(wlc_info_t *wlc);
extern void wlc_stf_exit_rxchain_pwrsave(wlc_info_t *wlc, uint8 ht_cap_rx_stbc);
extern int wlc_stf_rxstreams_get(wlc_info_t* wlc);
#else
#define wlc_stf_rxstreams_get(wlc) ((wlc)->stf->rxstreams)
#endif // endif

extern int wlc_stf_ant_txant_validate(wlc_info_t *wlc, int8 val);
extern void wlc_stf_phy_txant_upd(wlc_info_t *wlc);
extern void wlc_stf_phy_chain_calc(wlc_info_t *wlc);
extern uint16 wlc_stf_phytxchain_sel(wlc_info_t *wlc, ratespec_t rspec);
extern uint16 wlc_stf_d11hdrs_phyctl_txant(wlc_info_t *wlc, ratespec_t rspec);
extern uint16 wlc_stf_d11hdrs_phyctl_txcore_80p80phy(wlc_info_t *wlc, uint16 phyctl);
extern void wlc_stf_d11hdrs_phyctl_get_txant_mask(wlc_info_t *wlc, ratespec_t rspec,
	uint16 *phytxant, uint16 *mask);
extern void wlc_stf_wowl_upd(wlc_info_t *wlc);
extern void wlc_stf_shmem_base_upd(wlc_info_t *wlc, uint16 base);
extern void wlc_stf_wowl_spatial_policy_set(wlc_info_t *wlc, int policy);
extern void wlc_update_txppr_offset(wlc_info_t *wlc, ppr_t *txpwr, ppr_ru_t *ru_txpwr);
extern int wlc_stf_spatial_policy_set(wlc_info_t *wlc, int val);
extern int8 wlc_stf_stbc_tx_get(wlc_info_t* wlc);

typedef uint16 wlc_stf_txchain_st;
extern void wlc_stf_txchain_get_perrate_state(wlc_info_t *wlc, wlc_stf_txchain_st *state,
	wlc_stf_txchain_evt_notify func);
extern void
wlc_stf_txchain_restore_perrate_state(wlc_info_t *wlc, wlc_stf_txchain_st *state);
extern bool wlc_stf_saved_state_is_consistent(wlc_info_t *wlc, wlc_stf_txchain_st *state);
#ifdef WL_BEAMFORMING
extern void wlc_stf_set_txbf(wlc_info_t *wlc, bool enable);
#endif // endif
#if defined(WLTXPWR_CACHE)
extern int wlc_stf_txchain_pwr_offset_set(wlc_info_t *wlc, wl_txchain_pwr_offsets_t *offsets);
#endif // endif
#if defined(WL_EXPORT_CURPOWER) || defined(WLC_DTPC)
int8 get_pwr_from_targets(wlc_info_t *wlc, ratespec_t rspec, ppr_t *txpwr);
#endif /* WL_EXPORT_CURPOWER || WLC_DTPC */
extern int wlc_stf_mws_set(wlc_info_t *wlc, uint32 txantmap, uint32 rxantmap);
#ifdef WLRSDB
extern void wlc_stf_phy_chain_calc_set(wlc_info_t *wlc);
#else
extern void BCMATTACHFN(wlc_stf_phy_chain_calc_set)(wlc_info_t *wlc);
#endif // endif
extern void wlc_stf_reinit_chains(wlc_info_t* wlc);
#ifdef WL_STF_ARBITRATOR
extern void wlc_stf_set_arbitrated_chains_complete(wlc_info_t *wlc);
extern void wlc_stf_arb_req_update(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 state);
#else /* stub */
#define wlc_stf_arb_req_update(a, b, c) do {} while (0)
#endif /* WL_STF_ARBITRATOR */

extern int wlc_stf_siso_bcmc_rx(wlc_info_t *wlc,  bool enable);

#define NAP_RSSI_DELTA          3       /* hysteresis rssi delta */
#define NAP_DISABLE_RSSI		-80		/* RSSI to disable NAP */

#ifdef OCL
extern void wlc_stf_ocl_rssi_thresh_handling(wlc_bsscfg_t *bsscfg);
extern void wlc_stf_ocl_update_assoc_pend(wlc_info_t *wlc, bool disable);
extern int wlc_get_ocl_meas_metrics(wlc_info_t *wlc, uint8 *destptr, int destlen);
#endif /* OCL */

void wlc_stf_txduty_upd(wlc_info_t *wlc);
int wlc_stf_txchain_upd(wlc_info_t *wlc, bool set_txchains, uint16 id);

#ifdef WL_MODESW
extern int wlc_stf_set_optxrxstreams(wlc_info_t *wlc, uint8 new_streams);
extern void wlc_stf_op_txrxstreams_complete(wlc_info_t *wlc);
#endif /* WL_MODESW */

#endif /* _wlc_stf_h_ */
