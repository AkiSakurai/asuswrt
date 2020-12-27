/**
 * @file
 * @brief
 * Code that controls the antenna/core/chain to aid space/time coding (MIMO/STBC/beamforming)
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
 * $Id: wlc_stf.c 781536 2019-11-22 16:45:12Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi_channels.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_ap.h>
#include <wlc_scb_ratesel.h>
#include <wlc_frmutil.h>
#include <wl_export.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_stf.h>
#include <wlc_txc.h>
#include <wlc_tvpm.h>
#ifdef WL11AC
#include <wlc_vht.h>
#include <wlc_txbf.h>
#endif // endif
#ifdef WL11AX
#include <wlc_he.h>
#endif // endif
#ifdef WLOLPC
#include <wlc_olpc_engine.h>
#endif // endif
#include <wlc_ht.h>
#include <wlc_types.h>
#include <wlc_scb.h>
#ifdef WLRSDB
#include <wlc_rsdb.h>
#endif // endif
#include <wlc_tx.h>

#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif // endif
#if defined(WL_EAP_SAS)
#include <wlc_scan.h>
#endif // endif
#include <wlc_dump.h>
#include <wlc_lq.h>
#include <wlc_iocv.h>
#include <phy_nap_api.h>
#include <phy_tpc_api.h>
#include <phy_ocl_api.h>
#ifdef WL_MU_TX
#include <wlc_mutx.h>
#endif // endif
#include <wlc_lq.h>
#include <phy_stf_api.h>
#include <phy_btcx_api.h>
#include <wlc_btcx.h>
#include <phy_temp_api.h>
#include <wlc_ratelinkmem.h>

/* this macro define all PHYs REV that can NOT receive STBC with one RX core active */
#define WLC_STF_NO_STBC_RX_1_CORE(wlc) (WLCISNPHY(wlc->band) && NREV_IS(wlc->band->phyrev, 17))

/* this macro define all PHYs REV that has Multiple Input/Output Capabilities */
#define WLCISMIMO (WLCISNPHY(wlc->band) || WLCISACPHY(wlc->band))

#define CCK_TX_DIVERSITY_BIT (BPHY_ONE_CORE_TX >> SPATIAL_SHIFT)

#define WLC_ISTHROTTLE_ENB_PHY(wlc) (WLCISNPHY(wlc->band) || WLCISACPHY(wlc->band))
#define WLC_TEMPSENSE_PERIOD		10	/**< 10 second timeout */

#ifdef WLRSDB
#ifndef WLC_RSDB_PER_CORE_TXCHAIN
#define WLC_RSDB_PER_CORE_TXCHAIN	1
#endif /* WLC_RSDB_PER_CORE_TXCHAIN */
#define BWSWITCH_20MHZ 20
#define BWSWITCH_40MHZ 40

#ifndef WLC_RSDB_PER_CORE_RXCHAIN
#define WLC_RSDB_PER_CORE_RXCHAIN	1
#endif /* WLC_RSDB_PER_CORE_RXCHAIN */
#endif /* WLRSDB */

#define PHYREG_OBSSPDSREN	0x12e0
#define PHYREG_OBSSCOLORBMP0	0x1332
#define PHYREG_OBSSCOLORBMP1	0x1333
#define PHYREG_OBSSCOLORBMP2	0x1334
#define PHYREG_OBSSCOLORBMP3	0x1335

struct wlc_stf_ocl_info {
	wlc_info_t  *wlc;
	bool    ocl_en;
	int8    ocl_rssi_delta;
	int8    ocl_rssi_threshold;
};

static int wlc_stf_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static int wlc_stf_ss_get(wlc_info_t* wlc, enum wlc_bandunit bandunit);
static bool wlc_stf_ss_set(wlc_info_t* wlc, int32 int_val, enum wlc_bandunit bandunit);
static bool wlc_stf_ss_auto(wlc_info_t* wlc);
static int  wlc_stf_ss_auto_set(wlc_info_t* wlc, bool enable);
static void wlc_txduty_upd(wlc_info_t *wlc);
static void wlc_stf_txcore_set(wlc_info_t *wlc, uint8 idx, uint8 val);
#if !defined(WLTXPWR_CACHE)
static int  wlc_stf_txchain_pwr_offset_set(wlc_info_t *wlc, wl_txchain_pwr_offsets_t *offsets);
#endif // endif
static void wlc_stf_stbc_rx_ht_update(wlc_info_t *wlc, int val);
static uint8 wlc_stf_core_mask_assign(wlc_stf_t *stf, uint core_count);
static uint8 wlc_stf_uint8_vec_max(uint8 *vec, uint count);
static void wlc_stf_int8_vec_replace(int8 *vec, uint count, int8 find, int8 replace);
static uint wlc_stf_best_dsss_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec);
static uint wlc_stf_best_ofdm_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec);
static uint wlc_stf_best_nsts1_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec);
static uint wlc_stf_best_nsts2_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec);
static uint wlc_stf_best_nsts3_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec);
static void wlc_stf_txcore_select(wlc_info_t *wlc, uint8 *txcore);
static uint wlc_stf_get_ppr_offset(wlc_info_t *wlc, wl_txppr_t *ppr_buf);
static int wlc_stf_spatial_mode_upd(wlc_info_t *wlc, int8 *mode);
static int wlc_stf_srcfg_cmd_get_dispatch(wlc_info_t *wlc,
	sr_config_t *params, char *outbuf, int outlen);
static int wlc_stf_srcfg_cmd_set_dispatch(wlc_info_t *wlc, sr_config_t *params);
static void wlc_stf_init_txcore_default(wlc_info_t *wlc);
static void wlc_stf_spatial_mode_set(wlc_info_t *wlc, chanspec_t chanspec);
#ifdef BCMDBG
static int wlc_stf_pproff_shmem_get(wlc_info_t *wlc, int *retval);
static void wlc_stf_pproff_shmem_set(wlc_info_t *wlc, uint8 rate, uint8 val);
#endif /* BCMDBG */

static void wlc_stf_txchain_reset(wlc_info_t *wlc, uint16 id);
static uint8 wlc_stf_get_target_core(wlc_info_t *wlc);

#ifdef WL11AC
static void wlc_stf_update_160mode_txcore(wlc_stf_t *stf, chanspec_t chanspec);
#endif // endif
#ifdef WL_NAP
static void wlc_stf_nap_assoc_state_cb(void *ctx, bss_assoc_state_data_t *notif_data);
#endif /* WL_NAP */
#ifdef OCL
static void wlc_stf_ocl_assoc_state_cb(void *ctx, bss_assoc_state_data_t *notif_data);
#endif // endif
#if defined(OCL) || defined(WL_MIMOPS_CFG)
static void wlc_stf_rx_bcn_notif(void *ctx, bss_rx_bcn_notif_data_t *notif_data);
#endif // endif

static void _wlc_stf_phy_txant_upd(wlc_info_t *wlc);
static uint16 _wlc_stf_phytxchain_sel(wlc_info_t *wlc, ratespec_t rspec);
static int wlc_stf_down(void *hdl);
static void wlc_stf_pproff_shmem_write(wlc_info_t *wlc);
static void wlc_stf_compute_ack_pprs(wlc_info_t *wlc);

static const uint8 txcore_default[MAX_CORE_IDX][3] = {
	{1, 0x01, 0x02},	/* CCK */
	{1, 0x01, 0x02},	/* OFDM */
	{1, 0x01, 0x02},	/* For Nsts = 1, enable core 1 */
	{2, 0x03, 0x06},	/* For Nsts = 2, enable core 1 & 2 */
	{3, 0x07, 0x07},	/* For Nsts = 3, enable core 1, 2 & 3 */
	{4, 0x0F, 0x0F} 	/* For Nsts = 4, enable all cores */
};

enum {
	CCK_1M_IDX = 0,
	CCK_2M_IDX,
	CCK_5M5_IDX,
	CCK_11M_IDX
};

static const uint8 cck_pwr_idx_table[12] = {
	0x80,
	CCK_1M_IDX,
	CCK_2M_IDX,
	0x80,
	0x80,
	CCK_5M5_IDX,
	CCK_5M5_IDX,
	0x80,
	0x80,
	0x80,
	0x80,
	CCK_11M_IDX
};

enum {
	OFDM_6M_IDX = 0,
	OFDM_9M_IDX,
	OFDM_12M_IDX,
	OFDM_18M_IDX,
	OFDM_24M_IDX,
	OFDM_36M_IDX,
	OFDM_48M_IDX,
	OFDM_54M_IDX
};

static const uint8 ofdm_pwr_idx_table[19] = {
	0x80,		/* 0 Mbps */
	0x80,		/* 3 Mbps */
	OFDM_6M_IDX,	/*  ...   */
	OFDM_9M_IDX,
	OFDM_12M_IDX,
	0x80,
	OFDM_18M_IDX,
	0x80,
	OFDM_24M_IDX,
	0x80,
	0x80,
	0x80,
	OFDM_36M_IDX,
	0x80,
	0x80,
	0x80,
	OFDM_48M_IDX,
	0x80,
	OFDM_54M_IDX
};

static const uint8 mcs_pwr_bw_table[] = {
	WL_TX_BW_20,	/* BW20_IDX */
	WL_TX_BW_40,	/* BW40_IDX */
	WL_TX_BW_80,	/* BW80_IDX */
	WL_TX_BW_160	/* BW160_IDX */
};

static const uint8 ofdm_pwr_bw_table[] = {
	WL_TX_BW_20,	/* BW20_IDX */
	WL_TX_BW_40,	/* BW40_IDX */
	WL_TX_BW_80,	/* BW80_IDX */
	WL_TX_BW_160	/* BW160_IDX */
};

static const uint8 cck_pwr_bw_table[] = {
	WL_TX_BW_20,		/* BW20_IDX */
	WL_TX_BW_20IN40,	/* BW40_IDX */
	WL_TX_BW_20IN80,	/* BW80_IDX */
	WL_TX_BW_20IN160	/* BW160_IDX */
};

/* iovar table */
enum wlc_stf_iov {
	IOV_STF_SS = 1,		/**< MIMO STS coding for single stream mcs or legacy ofdm rates */
	IOV_STF_SS_AUTO = 2,	/**< auto selection of channel-based */
				/* MIMO STS coding for single stream mcs */
				/* OR LEGACY ofdm rates */
	IOV_STF_STBC_RX = 3,	/**< MIMO, STBC RX */
	IOV_STF_STBC_TX = 4,	/**< MIMO, STBC TX */
	IOV_STF_TXSTREAMS = 5,	/**< MIMO, tx stream */
	IOV_STF_TXCHAIN = 6,	/**< MIMO, tx chain */
	IOV_STF_SISO_TX = 7,	/**< MIMO, SISO TX */
	IOV_STF_HW_TXCHAIN = 8,	/**< MIMO, HW tx chain */
	IOV_STF_RXSTREAMS = 9,	/**< MIMO, rx stream */
	IOV_STF_RXCHAIN = 10,	/**< MIMO, rx chain */
	IOV_STF_HW_RXCHAIN = 11,	/**< MIMO, HW rx chain */
	IOV_STF_TXCORE = 12,		/**< MIMO, tx core enable and selected */
	IOV_STF_TXCORE_OVRD = 13,
	IOV_STF_SPATIAL_MODE = 14,	/**< spatial policy to use */
	IOV_STF_TEMPS_DISABLE = 15,
	IOV_STF_TXCHAIN_PWR_OFFSET = 16,
	IOV_STF_RSSI_PWRDN = 17,
	IOV_STF_PPR_OFFSET = 18,
	IOV_STF_PWR_THROTTLE_TEST = 19, /**< testing */
	IOV_STF_PWR_THROTTLE_MASK = 20, /**< core to enable/disable thermal throttling */
	IOV_STF_PWR_THROTTLE = 21,
	IOV_STF_PWR_THROTTLE_STATE = 22,
	IOV_STF_RATETBL_PPR = 23,
	IOV_STF_ONECHAIN = 24,		/**< MIMO, reduce 1 TX or 1 RX chain */
	IOV_STF_DUTY_CYCLE_CCK = 25,	/**< maximum allowed duty cycle for CCK */
	IOV_STF_DUTY_CYCLE_OFDM = 26,	/**< maximum allowed duty cycle for OFDM */
	IOV_STF_DUTY_CYCLE_PWR = 27,	/**< max allowed duty cycle for power throttle feature */
	IOV_STF_DUTY_CYCLE_THERMAL = 28, /**< max allowed duty cycle for thermal throttle feature */
	IOV_STF_NSS_TX = 29,
	IOV_STF_OPER_MODE = 30,		/**< operating mode change */
	IOV_STF_NAP_ENABLE = 31,
	IOV_STF_NAP_STATUS = 32,
	IOV_STF_NAP_RSSI_THRESHOLD = 33,
	IOV_STF_REQ_DUMP = 34,
	IOV_STF_MRC_RSSI_THRESHOLD = 35,
	IOV_STF_MIMO_PS_CFG_CHANGE_WAIT_TIME = 36,
	IOV_STF_MIMO_PS_CFG = 37,
	IOV_STF_MIMO_PS_STATUS = 38,
	IOV_STF_MIMO_PS_LEARNING_CONFIG = 39,
	IOV_STF_OCL_ENABLE = 40,
	IOV_STF_OCL_STATUS = 41,
	IOV_STF_OCL_RSSI_THRESHOLD = 42,
	IOV_STF_OCL_COREMASK = 43,
	IOV_STF_THERMAL_THROTTLE_FEATURES = 44,
	IOV_STF_SR_CFG = 45, /* Spatial Reuse Configuration */
	IOV_STF_LAST
};

static const bcm_iovar_t stf_iovars[] = {
	{"mimo_ss_stf", IOV_STF_SS,
	(IOVF_OPEN_ALLOW), 0, IOVT_INT8, 0
	},
	{"stf_ss_auto", IOV_STF_SS_AUTO,
	(0), 0, IOVT_INT8, 0
	},
	{"stbc_rx", IOV_STF_STBC_RX,
	(IOVF_OPEN_ALLOW), 0, IOVT_UINT8, 0
	},
	{"stbc_tx", IOV_STF_STBC_TX,
	(IOVF_OPEN_ALLOW), 0, IOVT_INT8, 0
	},
	{"siso_tx", IOV_STF_SISO_TX,
	(0), 0, IOVT_BOOL, 0
	},
	{"txstreams", IOV_STF_TXSTREAMS,
	(0), 0, IOVT_UINT8, 0
	},
	{"txchain", IOV_STF_TXCHAIN,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_INT32, 0
	},
	{"hw_txchain", IOV_STF_HW_TXCHAIN,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_UINT8, 0
	},
	{"rxstreams", IOV_STF_RXSTREAMS,
	(0), 0, IOVT_UINT8, 0
	},
	{"hw_rxchain", IOV_STF_HW_RXCHAIN,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_UINT8, 0
	},
	{"rxchain", IOV_STF_RXCHAIN,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_UINT8, 0
	},
	{"txcore", IOV_STF_TXCORE,
	(0), 0, IOVT_BUFFER,  sizeof(uint32)*2
	},
	{"txcore_override", IOV_STF_TXCORE_OVRD,
	(0), 0, IOVT_BUFFER,  sizeof(uint32)*2
	},
	{"tempsense_disable", IOV_STF_TEMPS_DISABLE,
	(0), 0, IOVT_BOOL, 0
	},
	{"txchain_pwr_offset", IOV_STF_TXCHAIN_PWR_OFFSET,
	(0), 0, IOVT_BUFFER, sizeof(wl_txchain_pwr_offsets_t),
	},
	{"curppr", IOV_STF_PPR_OFFSET,
	(0), 0, IOVT_BUFFER, 0
	},
	{"pwrthrottle_test", IOV_STF_PWR_THROTTLE_TEST,
	0, 0, IOVT_UINT8, 0
	},
	{"pwrthrottle_mask", IOV_STF_PWR_THROTTLE_MASK,
	0, 0, IOVT_UINT8, 0
	},
	{"pwrthrottle", IOV_STF_PWR_THROTTLE,
	0, 0, IOVT_INT32, 0
	},
	{"pwrthrottle_state", IOV_STF_PWR_THROTTLE_STATE,
	0, 0, IOVT_INT32, 0
	},
	{"spatial_policy", IOV_STF_SPATIAL_MODE,
	0, 0, IOVT_BUFFER, (sizeof(int) * SPATIAL_MODE_MAX_IDX)
	},
	{"rssi_pwrdn_disable", IOV_STF_RSSI_PWRDN,
	(0), 0, IOVT_BOOL, 0
	},
	{"ratetbl_ppr", IOV_STF_RATETBL_PPR,
	0, 0, IOVT_BUFFER, (sizeof(int) * 12)
	},
	{"onechain", IOV_STF_ONECHAIN,
	(0), 0, IOVT_INT8, 0
	},
	{"dutycycle_cck", IOV_STF_DUTY_CYCLE_CCK,
	0, 0, IOVT_UINT8, 0
	},
	{"dutycycle_ofdm", IOV_STF_DUTY_CYCLE_OFDM,
	0, 0, IOVT_UINT8, 0
	},
	{"dutycycle_pwr", IOV_STF_DUTY_CYCLE_PWR,
	0, 0, IOVT_UINT8, 0
	},
	{"dutycycle_thermal", IOV_STF_DUTY_CYCLE_THERMAL,
	0, 0, IOVT_UINT8, 0
	},
	{"tx_nss", IOV_STF_NSS_TX,
	0, 0, IOVT_UINT8, 0
	},
#ifdef WL11AC
	{"oper_mode", IOV_STF_OPER_MODE, 0, 0, IOVT_UINT16, 0},
#endif /* WL11AC */
	{"nap_enable", IOV_STF_NAP_ENABLE,
	(IOVF_RSDB_SET), 0, IOVT_UINT8, 0
	},
	{"nap_status", IOV_STF_NAP_STATUS,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, sizeof(wl_nap_status_v1_t)
	},
	{"nap_rssi_threshold", IOV_STF_NAP_RSSI_THRESHOLD,
	(IOVF_RSDB_SET), 0, IOVT_INT8, 0
	},
	{"stfreqdump", IOV_STF_REQ_DUMP,
	(0), 0, IOVT_INT8, 0
	},
	{"mrc_rssi_threshold", IOV_STF_MRC_RSSI_THRESHOLD,
	(0), 0, IOVT_INT8, 0
	},
	{"mimo_ps_cfg_change_wait_time", IOV_STF_MIMO_PS_CFG_CHANGE_WAIT_TIME,
	(0), 0, IOVT_UINT32, 0
	},
	{"mimo_ps_cfg", IOV_STF_MIMO_PS_CFG,
	(0), 0, IOVT_BUFFER, sizeof(wl_mimops_cfg_t)
	},
	{"mimo_ps_status", IOV_STF_MIMO_PS_STATUS,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, sizeof(wl_mimo_ps_status_t)
	},
	{"mimo_ps_learning_config", IOV_STF_MIMO_PS_LEARNING_CONFIG,
	(0), 0, IOVT_BUFFER, sizeof(wl_mimops_learning_cfg_t)
	},
	{"ocl_enable", IOV_STF_OCL_ENABLE,
	(IOVF_RSDB_SET), 0, IOVT_UINT8, 0
	},
	{"ocl_status", IOV_STF_OCL_STATUS,
	(0), IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, sizeof(ocl_status_info_t)
	},
	{"ocl_rssi_threshold", IOV_STF_OCL_RSSI_THRESHOLD,
	(IOVF_RSDB_SET), 0, IOVT_INT8, 0
	},
	{"ocl_coremask", IOV_STF_OCL_COREMASK,
	(IOVF_SET_UP | IOVF_GET_UP), 0, IOVT_UINT8, 0
	},
	{"thermal_throttle_features", IOV_STF_THERMAL_THROTTLE_FEATURES,
	(0), 0, IOVT_UINT32, 0
	},
	{"sr_config", IOV_STF_SR_CFG,
	(IOVF_SET_UP|IOVF_OPEN_ALLOW), 0, IOVT_BUFFER, sizeof(sr_config_t)
	},
	{NULL, 0, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/** handle STS related iovars */
static int
wlc_stf_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;
	sr_config_t srcfg_cmd_params;

#if defined(WL11AC)
	wlc_bsscfg_t *bsscfg = NULL;

	/* lookup bsscfg from provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);
#endif /* WL11AC */

	ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	if (plen >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)p + sizeof(int_val)), &int_val2, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {
		case IOV_GVAL(IOV_STF_SS):
			*ret_int_ptr = wlc_stf_ss_get(wlc, (int8)int_val);
			break;

		case IOV_SVAL(IOV_STF_SS):
			if (!wlc_stf_ss_set(wlc, int_val, (int8)int_val2)) {
				err = BCME_RANGE;
			}
			break;

		case IOV_GVAL(IOV_STF_SS_AUTO):
			*ret_int_ptr = (int)wlc_stf_ss_auto(wlc);
			break;

		case IOV_SVAL(IOV_STF_SS_AUTO):
			err = wlc_stf_ss_auto_set(wlc, bool_val);
			break;

		case IOV_GVAL(IOV_STF_SISO_TX):
			*ret_int_ptr = wlc->stf->siso_tx;
			break;

		case IOV_SVAL(IOV_STF_SISO_TX):
			if  (wlc->stf->siso_tx  == bool_val)
				break;
			else
			{
				wlc->stf->siso_tx = bool_val;
			}

			wlc_stf_ss_auto_set(wlc, !bool_val);
			if (!wlc_stf_ss_set(wlc, !bool_val, -1)) {
				err = BCME_RANGE;
			}
			else
				wlc_scb_ratesel_init_all(wlc);
			break;

		case IOV_GVAL(IOV_STF_STBC_TX):
			*ret_int_ptr = wlc_stf_stbc_tx_get(wlc);
			break;

		case IOV_SVAL(IOV_STF_STBC_TX):
			if (!WLC_STBC_CAP_PHY(wlc)) {
				err = BCME_UNSUPPORTED;
				break;
			}

			if (!wlc_ht_stbc_tx_set(wlc->hti, int_val))
				err = BCME_RANGE;

			break;

		case IOV_GVAL(IOV_STF_STBC_RX):
#ifdef RXCHAIN_PWRSAVE
			/* need to get rx_stbc HT capability from saved value if
			 * in rxchain_pwrsave mode
			 */
			*ret_int_ptr = wlc_rxchain_pwrsave_stbc_rx_get(wlc);
#else
			*ret_int_ptr = wlc_ht_stbc_rx_get(wlc->hti);
#endif // endif
			break;

		case IOV_SVAL(IOV_STF_STBC_RX):
			if (!WLC_STBC_CAP_PHY(wlc)) {
				err = BCME_UNSUPPORTED;
				break;
			}
#ifdef RXCHAIN_PWRSAVE
			/* need to exit rxchain_pwrsave mode(turn on all RXCHAIN)
			 * before enable STBC_RX
			 */
			/* PHY cannot receive STBC with only one rx core active */
			if (WLC_STF_NO_STBC_RX_1_CORE(wlc)) {
				if (wlc->ap != NULL)
					wlc_reset_rxchain_pwrsave_mode(wlc->ap);
			}
#endif /* RXCHAIN_PWRSAVE */
			if (!wlc_stf_stbc_rx_set(wlc, int_val))
				err = BCME_RANGE;

			break;

		case IOV_GVAL(IOV_STF_TXSTREAMS):
			*ret_int_ptr = wlc->stf->txstreams;
			break;

		case IOV_GVAL(IOV_STF_TXCHAIN):
			*ret_int_ptr = wlc->stf->txchain;
			break;

		case IOV_SVAL(IOV_STF_TXCHAIN):
		{
#ifdef WL_STF_ARBITRATOR
			uint8 rxchain;
			bool chain_disabled = FALSE;
			BCM_REFERENCE(rxchain);
#endif // endif
			if (!int_val)
				return BCME_BADOPTION;
			if (int_val == WL_STF_CONFIG_CHAINS_DISABLED) {
				int_val = wlc->stf->hw_txchain;
#ifdef WL_STF_ARBITRATOR
				chain_disabled = TRUE;
#endif // endif
			}

#ifdef WL_STF_ARBITRATOR
			if (WLC_STF_ARB_ENAB(wlc->pub)) {
				/* Evaluate txchain request
				 */
				if (chain_disabled) {
					wlc->stf->arb->iovar_rxtxchain_active =
					        (~WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE &
					        wlc->stf->arb->iovar_rxtxchain_active);
					wlc->stf->arb->iovar_req_txchain =
					                  WL_STF_CHAINS_NOT_CONFIGURED;
				} else {
					wlc->stf->arb->iovar_rxtxchain_active |=
					        WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE;
					wlc->stf->arb->iovar_req_txchain = int_val;
				}
				/* Use configured rxchain if we have it configured else use current
				 */
				rxchain = (wlc->stf->arb->iovar_req_rxchain !=
				           WL_STF_CHAINS_NOT_CONFIGURED) ?
				           wlc->stf->arb->iovar_req_rxchain : wlc->stf->rxchain;

				err = wlc_stf_nss_request_update(wlc, wlc->stf->arb->iovar_req,
				          wlc->stf->arb->iovar_rxtxchain_active,
				          int_val, WLC_BITSCNT(int_val),
				          rxchain, WLC_BITSCNT(rxchain));
			} else
#endif  /* WL_STF_ARBITRATOR */
			{
				err = wlc_stf_txchain_set(wlc, int_val, FALSE, WLC_TXCHAIN_ID_USR);
			}
			break;
		}

#if defined(WL11AC)
		case IOV_GVAL(IOV_STF_OPER_MODE):
		{
#ifdef WL_MODESW
			uint8 oper_mode;
			chanspec_t chspec = (bsscfg->associated) ? bsscfg->current_bss->chanspec :
				wlc->default_bss->chanspec;

			oper_mode = wlc_modesw_derive_opermode(wlc->modesw,
					chspec, bsscfg,
					wlc->stf->op_rxstreams);

			if (oper_mode != bsscfg->oper_mode) {
				WL_ERROR(("wl%d: %s: oper_mode mismatch cur:0x%02x calc:0x%02x\n",
						wlc->pub->unit, __FUNCTION__,
						bsscfg->oper_mode, oper_mode));
				err = BCME_ERROR;
				break;
			}
#endif /* WL_MODESW */

			*ret_int_ptr = ((bsscfg->oper_mode_enabled & 0xff) << 8) |
				bsscfg->oper_mode;
			break;
		}

		case IOV_SVAL(IOV_STF_OPER_MODE):
		{
#ifdef WL_MODESW
			if (WLC_MODESW_ENAB(wlc->pub)) {
				bool enabled = (int_val & 0xff00) >> 8;
				uint8 oper_mode = int_val & 0xff;
				wlc_bsscfg_t *cfg = NULL;
				int idx;

				if (wlc_modesw_is_req_valid(wlc->modesw, bsscfg) != TRUE) {
					err = BCME_BUSY;
					break;
				}
				if (DOT11_OPER_MODE_RXNSS_TYPE(oper_mode)) {
					/* we don't support nss type 1 for now */
					err = BCME_BADARG;
					break;
				}

				FOREACH_BSS(wlc, idx, cfg) {
					WL_MODE_SWITCH(("cfg:%d %s modesw_entry\n",
						cfg->_idx, __FUNCTION__));
					err = wlc_modesw_handle_oper_mode_notif_request(wlc->modesw,
						cfg, oper_mode, enabled, 0);
				}
			} else
#endif /* WL_MODESW */
				err = BCME_UNSUPPORTED;
			break;
		}
#endif /* WL11AC */
		case IOV_GVAL(IOV_STF_ONECHAIN): {
			if (wlc->stf == NULL) {
				err = BCME_UNSUPPORTED;
			} else {
				*ret_int_ptr = wlc->stf->onechain;
			}
			break;
		}

		case IOV_SVAL(IOV_STF_ONECHAIN) : {
			uint8 chainmap, bitmap_curr, bitmap_upd;

			/* Error checking */
			if (int_val >= 2) {
				err = BCME_RANGE;
				break;
			} else if (int_val == -1) {
				break;
			}

			/* storeing value */
			wlc->stf->onechain = (int8)int_val;

			/* get current RX-TX bitmap */
			bitmap_curr = ((wlc->stf->rxchain) << 4) | wlc->stf->txchain;

			/* get chain map based on RSSI comparison */
			/* chainmap = wlc_phy_rssi_ant_compare(pi); */
			chainmap = 0;

			/* construct new RX-TX bitmap */
			bitmap_upd = int_val ? ((bitmap_curr & 0xf0) | chainmap) :
				((chainmap << 4) | (bitmap_curr & 0xf));

			wlc_stf_chain_active_set(wlc, bitmap_upd);

			break;
		}

		case IOV_GVAL(IOV_STF_HW_TXCHAIN):
			*ret_int_ptr = wlc->stf->hw_txchain;
			break;

		case IOV_GVAL(IOV_STF_RXSTREAMS):
			*ret_int_ptr = (uint8)WLC_BITSCNT(wlc->stf->rxchain);
			break;

		case IOV_GVAL(IOV_STF_RXCHAIN):
			/* use SW Rx chain state */
			*ret_int_ptr = wlc->stf->rxchain;
			break;

		case IOV_GVAL(IOV_STF_HW_RXCHAIN):
			*ret_int_ptr = wlc->stf->hw_rxchain;
			break;

		case IOV_SVAL(IOV_STF_RXCHAIN):
		{
#ifdef WL_STF_ARBITRATOR
			uint8 txchain;
			bool chain_disabled = FALSE;
			BCM_REFERENCE(txchain);
#endif // endif
			if (!int_val)
				return BCME_BADOPTION;
#ifdef RXCHAIN_PWRSAVE
			if (RXCHAIN_PWRSAVE_ENAB(wlc->ap)) {
				if (int_val == wlc->stf->hw_rxchain)
					wlc_reset_rxchain_pwrsave_mode(wlc->ap);
				else
					wlc_disable_rxchain_pwrsave(wlc->ap);
			}
#endif // endif
			if (int_val == WL_STF_CONFIG_CHAINS_DISABLED) {
				int_val = wlc->stf->hw_rxchain;
#ifdef WL_STF_ARBITRATOR
				chain_disabled = TRUE;
#endif // endif
			}
#ifdef WL_STF_ARBITRATOR
			if (WLC_STF_ARB_ENAB(wlc->pub)) {
				/* Evaluate rxchain request
				 */
				if (chain_disabled) {
					wlc->stf->arb->iovar_rxtxchain_active =
					    (~WLC_STF_ARBITRATOR_REQ_STATE_RX_ACTIVE &
					    wlc->stf->arb->iovar_rxtxchain_active);
					wlc->stf->arb->iovar_req_rxchain =
					            WL_STF_CHAINS_NOT_CONFIGURED;
				} else {
					wlc->stf->arb->iovar_rxtxchain_active |=
					    WLC_STF_ARBITRATOR_REQ_STATE_RX_ACTIVE;
					wlc->stf->arb->iovar_req_rxchain = int_val;
				}
				/* Use configured txchain if we have it configured else use current
				 */
				txchain = (wlc->stf->arb->iovar_req_txchain !=
				           WL_STF_CHAINS_NOT_CONFIGURED) ?
				           wlc->stf->arb->iovar_req_txchain : wlc->stf->txchain;

				wlc_stf_nss_request_update(wlc, wlc->stf->arb->iovar_req,
					wlc->stf->arb->iovar_rxtxchain_active,
					txchain, WLC_BITSCNT(txchain),
					int_val, WLC_BITSCNT(int_val));
			} else
#endif  /* WL_STF_ARBITRATOR */
			{
				err = wlc_stf_rxchain_set(wlc, int_val, TRUE);
				if (err == BCME_OK) {
					/* Update op_rxstreams */
				if (!CHSPEC_IS160(wlc->default_bss->chanspec) &&
						!CHSPEC_IS8080(wlc->default_bss->chanspec)) {
					wlc->stf->op_rxstreams = wlc->stf->rxstreams;
				}
					/* Update rateset */
					wlc_rateset_mcs_build(&wlc->default_bss->rateset,
						wlc->stf->op_rxstreams);
					wlc_rateset_filter(&wlc->default_bss->rateset,
					                   &wlc->default_bss->rateset,
					                   FALSE, WLC_RATES_CCK_OFDM,
					                   RATE_MASK_FULL,
					                   wlc_get_mcsallow(wlc, wlc->cfg));

				}
			}
			break;
		}

		case IOV_GVAL(IOV_STF_TXCORE):
		{
			uint32 core[2] = {0, 0};

			core[0] |= wlc->stf->txcore[NSTS4_IDX][1] << 24;
			core[0] |= wlc->stf->txcore[NSTS3_IDX][1] << 16;
			core[0] |= wlc->stf->txcore[NSTS2_IDX][1] << 8;
			core[0] |= wlc->stf->txcore[NSTS1_IDX][1];
			core[1] |= wlc->stf->txcore[OFDM_IDX][1] << 8;
			core[1] |= wlc->stf->txcore[CCK_IDX][1];
			bcopy(core, a, sizeof(uint32)*2);
			break;
		}

		case IOV_SVAL(IOV_STF_TXCORE):
		{
			uint32 core[2];
			uint8 i, Nsts = 0;
			uint8 txcore_ovrd[MAX_CORE_IDX] = {0, 0, 0, 0, 0, 0};
			uint8 cck, ofdm;
			wlc_bsscfg_t *cfg = NULL;
			bool bw160_wide = FALSE;
			int idx;

			if (!WLCISACPHY(wlc->band)) {
				err = BCME_UNSUPPORTED;
				break;
			}
#ifdef WLOLPC
			if (OLPC_ENAB(wlc)) {
				/* not allowed to change during olpc calibration */
				if (wlc_olpc_eng_has_active_cal(wlc->olpc_info)) {
					err = BCME_BUSY;
					break;
				}
			}
#endif /* WLOLPC */
			FOR_ALL_UP_BSS(wlc, idx, cfg) {
				if (CHSPEC_IS_BW_160_WIDE((cfg->current_bss->chanspec))) {
					bw160_wide = TRUE;
					break;
				}
			}

			if (!bw160_wide) {
				if (CHSPEC_IS_BW_160_WIDE((wlc->default_bss->chanspec))) {
					bw160_wide = TRUE;
				}
			}

			bcopy(p, core, sizeof(uint32)*2);

			cck = core[1] & 0x0f;
			ofdm = (core[1] >> 8) & 0x0f;

			/* For 4366C0 (corerev 65) 160Mhz operation, we need at least two cores.
			 * For 1x1(SISO), we need cores 0,2(0x05) or cores 1,3(0x0A)
			 * For 2x2, we need all the 4 cores(0x0F).
			 * Reject settings other than 0x05, 0x0A, 0x0F
			 */
			if (WLC_PHY_160_HALF_NSS(wlc) && bw160_wide &&
				(ofdm != 0x05 && ofdm != 0x0A && ofdm != 0x0F && ofdm != 0x0)) {
					WL_ERROR(("wl%d: Invalid Txcore setting in 160Mhz\n",
						wlc->pub->unit));
				return BCME_BADARG;
			}

			if (core[0] == 0 && core[1] == 0) {
				bzero(wlc->stf->txcore_override, MAX_CORE_IDX);
				if (wlc->stf->txchain_perrate_state_modify) {
					wlc->stf->txchain_perrate_state_modify(wlc);
				}
				wlc_stf_spatial_policy_set(wlc, wlc->stf->spatialpolicy);
				break;
			}

			/* core[0] contains mcs txcore mask setting
			 * core[1] contains cck & ofdm txcore mask setting
			 */
			if (core[0]) {
				for (i = 0; i < 4; i++) {
					Nsts = ((uint8)core[0] & 0x0f);
					if (Nsts > MAX_STREAMS_SUPPORTED) {
						WL_ERROR(("wl%d: %s: Nsts(%d) out of range\n",
							wlc->pub->unit, __FUNCTION__, Nsts));
						return BCME_RANGE;
					}
					txcore_ovrd[Nsts+OFDM_IDX] =
						((uint8)core[0] & 0xf0) >> 4;

					if (Nsts > WLC_BITSCNT(txcore_ovrd[Nsts+OFDM_IDX])) {
						WL_ERROR(("wl%d: %s: Nsts(%d) >"
							" # of core enabled (0x%x)\n",
							wlc->pub->unit, __FUNCTION__,
							Nsts, txcore_ovrd[Nsts+OFDM_IDX]));
						return BCME_BADARG;
					}
					if (WLC_BITSCNT(txcore_ovrd[Nsts+OFDM_IDX]) >
						wlc->stf->txstreams) {
						WL_ERROR(("wl%d: %s: Nsts(%d) core (0x%x) >"
							" HW core (0x%x)\n",
							wlc->pub->unit, __FUNCTION__,
							Nsts, txcore_ovrd[Nsts+OFDM_IDX],
							wlc->stf->hw_txchain));
						return BCME_BADARG;
					}
					core[0] >>= 8;
				}
			}
			if (core[1]) {
				txcore_ovrd[CCK_IDX] = cck;
				if (WLC_BITSCNT(txcore_ovrd[CCK_IDX]) > wlc->stf->txstreams) {
					WL_ERROR(("wl%d: %s: cck core (0x%x) > HW core (0x%x)\n",
						wlc->pub->unit, __FUNCTION__,
						txcore_ovrd[CCK_IDX], wlc->stf->hw_txchain));
					return BCME_BADARG;
				}
				txcore_ovrd[OFDM_IDX] = ofdm;
				if (WLC_BITSCNT(txcore_ovrd[OFDM_IDX]) > wlc->stf->txstreams) {
					WL_ERROR(("wl%d: %s: ofdm core (0x%x) > HW core (0x%x)\n",
						wlc->pub->unit, __FUNCTION__,
						txcore_ovrd[OFDM_IDX], wlc->stf->hw_txchain));
					return BCME_BADARG;
				}
			}

			bcopy(txcore_ovrd, wlc->stf->txcore_override, MAX_CORE_IDX);
			if (wlc->stf->txchain_perrate_state_modify) {
				wlc->stf->txchain_perrate_state_modify(wlc);
			}
			wlc_stf_spatial_policy_set(wlc, wlc->stf->spatialpolicy);
			break;
		}

		case IOV_GVAL(IOV_STF_TXCORE_OVRD):
		{
			uint32 core[2] = {0, 0};

			core[0] |= wlc->stf->txcore_override[NSTS4_IDX] << 24;
			core[0] |= wlc->stf->txcore_override[NSTS3_IDX] << 16;
			core[0] |= wlc->stf->txcore_override[NSTS2_IDX] << 8;
			core[0] |= wlc->stf->txcore_override[NSTS1_IDX];
			core[1] |= wlc->stf->txcore_override[OFDM_IDX] << 8;
			core[1] |= (wlc->stf->txcore_override[CCK_IDX] &
				CCK_TX_DIVERSITY_BIT) << 16;
			bcopy(core, a, sizeof(uint32)*2);
			break;
		}

		case IOV_GVAL(IOV_STF_TEMPS_DISABLE):
			*ret_int_ptr = wlc->stf->tempsense_disable;
			break;

		case IOV_SVAL(IOV_STF_TEMPS_DISABLE):
			wlc->stf->tempsense_disable = (uint8)int_val;
			if (wlc->stf->tempsense_disable == ON) {
				wlc->stf->throttle_state &= ~WLC_TEMPTHROTTLE_ON;
				wlc_txduty_upd(wlc);
			}
			break;

		case IOV_GVAL(IOV_STF_TXCHAIN_PWR_OFFSET):
			memcpy(a, &wlc->stf->txchain_pwr_offsets, sizeof(wl_txchain_pwr_offsets_t));
			break;

		case IOV_SVAL(IOV_STF_TXCHAIN_PWR_OFFSET):
			err = wlc_stf_txchain_pwr_offset_set(wlc, (wl_txchain_pwr_offsets_t*)p);
			break;

		case IOV_GVAL(IOV_STF_PPR_OFFSET):
		{
			wl_txppr_t *pbuf = (wl_txppr_t *)p;

			if (pbuf->len < WL_TXPPR_LENGTH || plen < sizeof(wl_txppr_t))
				return BCME_BUFTOOSHORT;

			if (pbuf->ver != WL_TXPPR_VERSION)
				return BCME_VERSION;

			if (alen < (int)sizeof(wl_txppr_t))
				return BCME_BADLEN;
			/* need to copy serialization flags/inited mem from inbuf to outbuf */
			bcopy(p, a, plen);
			pbuf = (wl_txppr_t *)a;
			if (!wlc_stf_get_ppr_offset(wlc, pbuf)) {
				return BCME_UNSUPPORTED;
			} else {
				err = 0;
			}
			break;
		}

		case IOV_GVAL(IOV_STF_PWR_THROTTLE_TEST):
			*ret_int_ptr = (int)wlc->stf->pwr_throttle_test;
			break;

		case IOV_SVAL(IOV_STF_PWR_THROTTLE_TEST):
			if (int_val < 0 || int_val >= wlc->stf->hw_txchain)
				return BCME_RANGE;
			wlc->stf->pwr_throttle_test = (uint8) int_val;
			break;

		case IOV_GVAL(IOV_STF_PWR_THROTTLE_MASK):
			*ret_int_ptr = (int)wlc->stf->pwr_throttle_mask;
			break;

		case IOV_SVAL(IOV_STF_PWR_THROTTLE_MASK):
			if (int_val < 0 || int_val > wlc->stf->hw_txchain)
				return BCME_RANGE;
			if (wlc->stf->pwr_throttle_mask == (uint8)int_val)
				break;
			wlc->stf->pwr_throttle_mask = (uint8)int_val;

			if (wlc->stf->throttle_state == WLC_THROTTLE_OFF)
				break;

			/* mask changed and current throttle is active, then clear
			 * the active state before call update throttle state
			 */
			WL_INFORM(("wl%d: %s: Update pwrthrottle due to mask change(0x%x)\n",
				wlc->pub->unit, __FUNCTION__, wlc->stf->pwr_throttle_mask));
			if (wlc->stf->throttle_state & WLC_PWRTHROTTLE_ON) {
				/* reset the txchain so new mask value can be applied */
				wlc_stf_txchain_reset(wlc, WLC_TXCHAIN_ID_PWRTHROTTLE);

				/* clear throttle state flag so that update can happen */
				wlc->stf->throttle_state &= ~WLC_PWRTHROTTLE_ON;
				/* let the watchdog update the HW */
			}
			if (wlc->stf->throttle_state & WLC_TEMPTHROTTLE_ON) {
				/* reset the txchain so new mask value can be applied */
				wlc_stf_txchain_reset(wlc, WLC_TXCHAIN_ID_TEMPSENSE);

				/* clear throttle state flag so that update can happen */
				wlc->stf->throttle_state &= ~WLC_TEMPTHROTTLE_ON;
				wlc->stf->tempsense_lasttime =
					wlc->pub->now - wlc->stf->tempsense_period;
				/* let the watchdog update the HW */
			}
			break;

		case IOV_GVAL(IOV_STF_PWR_THROTTLE):
			*ret_int_ptr = wlc->stf->pwr_throttle;
			break;

		case IOV_SVAL(IOV_STF_PWR_THROTTLE):
			if (int_val != OFF && int_val != ON &&
			    int_val != AUTO)
				return BCME_RANGE;
			wlc->stf->pwr_throttle = int_val;
			break;

		case IOV_GVAL(IOV_STF_PWR_THROTTLE_STATE):
			*ret_int_ptr = wlc->stf->throttle_state;
			break;

		case IOV_GVAL(IOV_STF_SPATIAL_MODE):
		{
			int i, *ptr = (int *)a;
			if (alen < (int)(sizeof(int) * SPATIAL_MODE_MAX_IDX))
				return BCME_BUFTOOSHORT;
			for (i = 0; i < SPATIAL_MODE_MAX_IDX; i++)
				ptr[i] = (int)wlc->stf->spatial_mode_config[i];
			break;
		}

		case IOV_SVAL(IOV_STF_SPATIAL_MODE):
		{
			int i;
			int8 mode[SPATIAL_MODE_MAX_IDX];
			int *ptr = (int *)p;

			if (plen < (sizeof(int) * SPATIAL_MODE_MAX_IDX))
				return BCME_BUFTOOSHORT;
			for (i = 0; i < SPATIAL_MODE_MAX_IDX; i++) {
				mode[i] = (int8)ptr[i];
				if (mode[i] != ON && mode[i] != OFF && mode[i] != AUTO)
					return BCME_RANGE;
			}
		        err = wlc_stf_spatial_mode_upd(wlc, mode);
			break;
		}

		case IOV_GVAL(IOV_STF_RSSI_PWRDN):
			*ret_int_ptr = (int)wlc->stf->rssi_pwrdn_disable;
			break;

		case IOV_SVAL(IOV_STF_RSSI_PWRDN):
			wlc->stf->rssi_pwrdn_disable = bool_val;
			wlc_mhf(wlc, MHF5, MHF5_HTPHY_RSSI_PWRDN,
				(bool_val ? MHF5_HTPHY_RSSI_PWRDN : 0), WLC_BAND_ALL);
			break;

#ifdef BCMDBG
		case IOV_GVAL(IOV_STF_RATETBL_PPR):
		{
			int *ptr = (int *)a;
			if (alen < (int)(sizeof(int) * 12))
				return BCME_BUFTOOSHORT;

			bzero(ptr, (sizeof(int) * 12));
			err = wlc_stf_pproff_shmem_get(wlc, ptr);
			break;
		}

		case IOV_SVAL(IOV_STF_RATETBL_PPR):
		{
			uint8 rate, val;
			int *ptr = (int *)a;
			rate = (ptr[0] & 0xff);
			val = (ptr[1] & 0xff);
			wlc_stf_pproff_shmem_set(wlc, rate, val);
			break;
		}
#endif /* BCMDBG */
		case IOV_SVAL(IOV_STF_DUTY_CYCLE_CCK):
			err = wlc_stf_duty_cycle_set(wlc, int_val, FALSE, wlc->pub->up);
			if (!err)
				wlc->stf->tx_duty_cycle_cck = (uint8)int_val;
			break;

		case IOV_GVAL(IOV_STF_DUTY_CYCLE_CCK):
			*ret_int_ptr = (int)wlc->stf->tx_duty_cycle_cck;
			break;

		case IOV_SVAL(IOV_STF_DUTY_CYCLE_OFDM):
			err = wlc_stf_duty_cycle_set(wlc, int_val, TRUE, wlc->pub->up);
			if (!err)
				wlc->stf->tx_duty_cycle_ofdm = (uint8)int_val;
			break;

		case IOV_GVAL(IOV_STF_DUTY_CYCLE_OFDM):
			*ret_int_ptr = (int)wlc->stf->tx_duty_cycle_ofdm;
			break;

		case IOV_SVAL(IOV_STF_DUTY_CYCLE_PWR):
			if (TVPM_ENAB(wlc->pub)) {
				return BCME_UNSUPPORTED;
			}
			if (int_val > NO_DUTY_THROTTLE || int_val < 0)
				return BCME_RANGE;
			wlc->stf->tx_duty_cycle_pwr = (uint8)MAX(MIN_DUTY_CYCLE_ALLOWED, int_val);
			wlc_txduty_upd(wlc);
			break;

		case IOV_GVAL(IOV_STF_DUTY_CYCLE_PWR):
			*ret_int_ptr = (int)wlc->stf->tx_duty_cycle_pwr;
			break;

		case IOV_SVAL(IOV_STF_DUTY_CYCLE_THERMAL):
#ifdef DUTY_CYCLE_THROTTLING
			return BCME_UNSUPPORTED;
#endif // endif
			if (int_val > NO_DUTY_THROTTLE || int_val < 0)
				return BCME_RANGE;

			int_val = (!int_val) ? NO_DUTY_THROTTLE : int_val;

			wlc->stf->tx_duty_cycle_max =
				(uint8)MAX(MIN_DUTY_CYCLE_ALLOWED, int_val);
			wlc_txduty_upd(wlc);
			break;

		case IOV_GVAL(IOV_STF_DUTY_CYCLE_THERMAL):
			if (TVPM_ENAB(wlc->pub))
				*ret_int_ptr = wlc_tvpm_get_target_txdc(wlc->tvpm);
			else
				*ret_int_ptr = MIN(wlc->stf->tx_duty_cycle_thermal,
						wlc->stf->tx_duty_cycle_max);
			break;

		case IOV_SVAL(IOV_STF_NSS_TX):
		{
			struct scb *scb = NULL;

			if ((uint8)int_val >= WLC_BITSCNT(wlc->stf->hw_txchain))
				return BCME_BADARG;

			if (wlc->stf->txstream_value == int_val)
				return BCME_OK;

			/* store the value in the txstream_value */
			wlc->stf->txstream_value = (uint8)int_val;

			/* Extract the scb */
			if (!(scb = wlc_scbfind(wlc, wlc->cfg, &wlc->cfg->BSSID))) {
				WL_ERROR(("wl%d: %s: out of scbs\n", wlc->pub->unit, __FUNCTION__));
				return BCME_ERROR;
			}
			wlc_scb_ratesel_init(wlc, scb);
			break;
		}

		case IOV_GVAL(IOV_STF_NSS_TX):
		{
			*ret_int_ptr = wlc->stf->txstream_value;
			break;
		}

#ifdef WL_NAP
		case IOV_SVAL(IOV_STF_NAP_ENABLE):
		{
			if (NAP_ENAB(wlc->pub)) {
				if (bool_val) {
					wlc_stf_nap_enable_reason(wlc, NAP_DISABLED_HOST);
				} else {
					wlc_stf_nap_disable_reason(wlc, NAP_DISABLED_HOST);
				}
			} else {
				err = BCME_UNSUPPORTED;
			}
			break;
		}

		case IOV_GVAL(IOV_STF_NAP_ENABLE):
		{
			if (NAP_ENAB(wlc->pub)) {
				uint16 bitmap;
				wlc_phy_nap_status_get(WLC_PI(wlc), &bitmap, NULL, NULL);
				*ret_int_ptr = ((bitmap & NAP_DISABLED_HOST) == 0);
			} else {
				err = BCME_UNSUPPORTED;
			}
			break;
		}

		case IOV_GVAL(IOV_STF_NAP_STATUS):
		{
			if (NAP_ENAB(wlc->pub)) {
				wl_nap_status_v1_t *status = (wl_nap_status_v1_t *)a;
				status->version = WL_NAP_STATUS_VERSION_1;
				status->len = sizeof(*status);
				status->slice_index = wlc->pub->unit;
				wlc_phy_nap_status_get(WLC_PI(wlc), &status->fw_status,
						&status->hw_status, &status->total_disable_dur);
			} else {
				err = BCME_UNSUPPORTED;
			}
			break;
		}

		case IOV_GVAL(IOV_STF_NAP_RSSI_THRESHOLD):
		{
			if (NAP_ENAB(wlc->pub)) {
				*ret_int_ptr = (int8)(wlc->stf->nap_rssi_threshold);
			} else {
				err = BCME_UNSUPPORTED;
			}
			break;
		}

		case IOV_SVAL(IOV_STF_NAP_RSSI_THRESHOLD):
		{
			if (NAP_ENAB(wlc->pub)) {
				if (int_val > -1) {
					err = BCME_RANGE;
				} else {
					wlc->stf->nap_rssi_threshold = (int8)int_val;
				}
			} else {
				err = BCME_UNSUPPORTED;
			}
			break;
		}
#endif /* WL_NAP */
#ifdef WL_STF_ARBITRATOR
		case IOV_GVAL(IOV_STF_REQ_DUMP):
		{
			if (WLC_STF_ARB_ENAB(wlc->pub))
				wlc_stf_nss_request_dump(wlc);
			else
				err = BCME_UNSUPPORTED;
			break;
		}
#endif /* WL_STF_ARBITRATOR */
#ifdef WL_MIMOPS_CFG
		case IOV_GVAL(IOV_STF_MRC_RSSI_THRESHOLD):
			if (WLC_MIMOPS_ENAB(wlc->pub)) {
				wlc_mimo_ps_cfg_t *bsscfg_mimo_ps_cfg =
				                   wlc_stf_mimo_ps_cfg_get(bsscfg);
				*ret_int_ptr = (int8)(bsscfg_mimo_ps_cfg->mrc_rssi_threshold);
			}
			else
				err = BCME_UNSUPPORTED;
			break;

		case IOV_SVAL(IOV_STF_MRC_RSSI_THRESHOLD):
			if (WLC_MIMOPS_ENAB(wlc->pub)) {
				wlc_mimo_ps_cfg_t *bsscfg_mimo_ps_cfg =
				                   wlc_stf_mimo_ps_cfg_get(bsscfg);
				bsscfg_mimo_ps_cfg->mrc_rssi_threshold = (int8)int_val;
			}
			else
				err = BCME_UNSUPPORTED;
			break;

		case IOV_GVAL(IOV_STF_MIMO_PS_CFG_CHANGE_WAIT_TIME):
			if (WLC_MIMOPS_ENAB(wlc->pub)) {
				wlc_mimo_ps_cfg_t *bsscfg_mimo_ps_cfg =
				                   wlc_stf_mimo_ps_cfg_get(bsscfg);
				*ret_int_ptr = (bsscfg_mimo_ps_cfg->mimo_ps_cfg_change_wait_time);
			}
			else
				err = BCME_UNSUPPORTED;
			break;

		case IOV_SVAL(IOV_STF_MIMO_PS_CFG_CHANGE_WAIT_TIME):
			if (WLC_MIMOPS_ENAB(wlc->pub)) {
				wlc_mimo_ps_cfg_t *bsscfg_mimo_ps_cfg =
				                   wlc_stf_mimo_ps_cfg_get(bsscfg);
				if (!bsscfg_mimo_ps_cfg->mimops_dynamic_disabled)
					bsscfg_mimo_ps_cfg->mimo_ps_cfg_change_wait_time = int_val;
				else
					err = BCME_UNSUPPORTED;
			}
			else
				err = BCME_UNSUPPORTED;
			break;

		case IOV_GVAL(IOV_STF_MIMO_PS_LEARNING_CONFIG):
		{
			if (WLC_MIMOPS_ENAB(wlc->pub)) {
				wl_mimops_learning_cfg_t* cfg_ptr =
				                (wl_mimops_learning_cfg_t*)a;
				wlc_mimo_ps_cfg_t *bsscfg_mimo_ps_cfg =
				        wlc_stf_mimo_ps_cfg_get(bsscfg);
				if (bsscfg_mimo_ps_cfg->mimops_dynamic_disabled) {
					err = BCME_UNSUPPORTED;
					break;
				}
				cfg_ptr->no_of_packets_for_learning =
				    bsscfg_mimo_ps_cfg->mimo_ps_smps_no_of_packets;
				if (cfg_ptr->version == WL_MIMO_PS_PS_LEARNING_CFG_V1) {
					cfg_ptr->learning_rssi_threshold =
					bsscfg_mimo_ps_cfg->mimo_ps_learning_rssi_threshold;
				}
				cfg_ptr->flag = 0;
				/* return the last or exsisting data available */
				/* this is because the host might be asleep and */
				/* might have not recieved event */
				/* in such a case it will request for the status again */
				memcpy(&cfg_ptr->mimops_learning_data,
				    &bsscfg_mimo_ps_cfg->mimo_ps_learning_data,
				    sizeof(cfg_ptr->mimops_learning_data));
			} else
				err = BCME_UNSUPPORTED;
			break;
		}

		case IOV_SVAL(IOV_STF_MIMO_PS_LEARNING_CONFIG):
		{
			if (!WLC_MIMOPS_ENAB(wlc->pub)) {
				err = BCME_UNSUPPORTED;
				break;
			}
			if (bsscfg == NULL) {
				err = BCME_ERROR;
				break;
			}
			err = wlc_stf_mimo_ps_learning_config_set(wlc, bsscfg, p);
			break;
		}

		case IOV_GVAL(IOV_STF_MIMO_PS_CFG):
		{
			if (!WLC_MIMOPS_ENAB(wlc->pub)) {
				err = BCME_UNSUPPORTED;
				break;
			}
			if (bsscfg == NULL) {
				err = BCME_ERROR;
				break;
			}
			wlc_stf_mimo_ps_config_get(wlc, bsscfg, a);
			break;
		}

		case IOV_SVAL(IOV_STF_MIMO_PS_CFG):
		{
			if (!WLC_MIMOPS_ENAB(wlc->pub)) {
				err = BCME_UNSUPPORTED;
				break;
			}
			wl_mimops_cfg_t* cfg_ptr = (wl_mimops_cfg_t*)p;
			if (cfg_ptr->version != WL_MIMO_PS_CFG_VERSION_1) {
				err = BCME_VERSION;
				break;
			}
			err = wlc_stf_mimo_ps_cfg(bsscfg, cfg_ptr);
			if (err != BCME_OK) {
				WL_STF_MIMO_PS_ERROR(
				    (" WLC STF MIMO PS : mimo_ps_cfg set iovar FAILED "));
			}
			break;
		}

		case IOV_GVAL(IOV_STF_MIMO_PS_STATUS):
		{
			if (!WLC_MIMOPS_ENAB(wlc->pub)) {
				err = BCME_UNSUPPORTED;
				break;
			}
			if (bsscfg == NULL) {
				err = BCME_ERROR;
				break;
			}
			err = wlc_stf_mimo_ps_status_get(wlc, bsscfg, a);
			if (err != BCME_OK) {
				WL_STF_MIMO_PS_ERROR(
				    (" WLC STF MIMO PS : mimo_ps_cfg get iovar FAILED "));
			}
			break;
		}
#endif /* WL_MIMOPS_CFG */
#ifdef OCL
		case IOV_SVAL(IOV_STF_OCL_ENABLE):
		{
			if (OCL_ENAB(wlc->pub)) {
				ASSERT(wlc->stf->ocl_info != NULL);
				if (wlc->stf->ocl_info->ocl_en != bool_val) {
					wlc->stf->ocl_info->ocl_en = bool_val;
#if defined(WL_PWRSTATS)
					if (PWRSTATS_ENAB(wlc->pub)) {
						/* capture stats snapshot before applying changes */
						wlc_mimo_siso_metrics_snapshot(wlc, FALSE,
								WL_MIMOPS_METRICS_SNAPSHOT_OCL);
					}
#endif /* WL_PWRSTATS */
					phy_ocl_disable_req_set(WLC_PI(wlc), OCL_DISABLED_HOST,
					                            !bool_val, WLC_OCL_REQ_HOST);
				}
			} else
				err = BCME_UNSUPPORTED;
			break;
		}
		case IOV_GVAL(IOV_STF_OCL_ENABLE):
		{
			if (OCL_ENAB(wlc->pub)) {
				ASSERT(wlc->stf->ocl_info != NULL);
				*ret_int_ptr = wlc->stf->ocl_info->ocl_en;
			}
			else
				err = BCME_UNSUPPORTED;
			break;
		}
		case IOV_GVAL(IOV_STF_OCL_STATUS):
		{
			ocl_status_info_t *status_ptr = (ocl_status_info_t *)a;
			uint16 disable_reqs;
			uint8 coremask = 0;
			bool ocl_en = FALSE;

			if (!OCL_ENAB(wlc->pub)) {
				err = BCME_UNSUPPORTED;
				break;
			}

			memset(status_ptr, 0, sizeof(*status_ptr));
			status_ptr->version = WL_OCL_STATUS_VERSION;
			status_ptr->len = (uint8) sizeof(*status_ptr);

			phy_ocl_status_get(WLC_PI(wlc), &disable_reqs, &coremask, &ocl_en);
			status_ptr->fw_status = disable_reqs;

			if (ocl_en)
				status_ptr->hw_status |= OCL_HWCFG;
			if (wlc->stf->rxchain & (wlc->stf->rxchain - 1))
				status_ptr->hw_status |= OCL_HWMIMO;
			if (!wlc->clk)
				status_ptr->hw_status |= OCL_COREDOWN;

			status_ptr->coremask = coremask;

			break;
		}
		case IOV_GVAL(IOV_STF_OCL_RSSI_THRESHOLD):
		{
			if (OCL_ENAB(wlc->pub)) {
				ASSERT(wlc->stf->ocl_info != NULL);
				*ret_int_ptr = (int8)(wlc->stf->ocl_info->ocl_rssi_threshold);
			} else
				err = BCME_UNSUPPORTED;
			break;
		}
		case IOV_SVAL(IOV_STF_OCL_RSSI_THRESHOLD):
		{
			if (OCL_ENAB(wlc->pub)) {
				ASSERT(wlc->stf->ocl_info != NULL);
				if (int_val > -1)
					err = BCME_RANGE;
				else
					wlc->stf->ocl_info->ocl_rssi_threshold = (int8)int_val;
			}
			else
				err = BCME_UNSUPPORTED;
			break;
		}
#endif /* OCL */
		case IOV_SVAL(IOV_STF_THERMAL_THROTTLE_FEATURES):
		{
			wlc->stf->thermal_throttle_features = (uint32)int_val;
			break;
		}
		case IOV_GVAL(IOV_STF_THERMAL_THROTTLE_FEATURES):
		{
			*ret_int_ptr = (int)wlc->stf->thermal_throttle_features;
			break;
		}

		case IOV_GVAL(IOV_STF_SR_CFG):
			bcopy(p, &srcfg_cmd_params, MIN(plen, sizeof(srcfg_cmd_params)));
			err = wlc_stf_srcfg_cmd_get_dispatch(wlc, &srcfg_cmd_params, a, alen);
			break;

		case IOV_SVAL(IOV_STF_SR_CFG):
			bcopy(p, &srcfg_cmd_params, MIN(plen, sizeof(srcfg_cmd_params)));
			err = wlc_stf_srcfg_cmd_set_dispatch(wlc, &srcfg_cmd_params);
			break;

		default:
			err = BCME_UNSUPPORTED;
	}
	return err;
}

static void
wlc_stf_stbc_rx_ht_update(wlc_info_t *wlc, int val)
{
	ASSERT((val == HT_CAP_RX_STBC_NO) || (val == HT_CAP_RX_STBC_ONE_STREAM));

	/* PHY cannot receive STBC with only one rx core active */
	if (WLC_STF_NO_STBC_RX_1_CORE(wlc)) {
		if ((wlc->stf->op_rxstreams == 1) && (val != HT_CAP_RX_STBC_NO))
			return;
	}
	wlc_ht_set_rx_stbc_cap(wlc->hti, val);

#ifdef WL11AC
	wlc_vht_set_rx_stbc_cap(wlc->vhti, val);
#endif /* WL11AC */

	if (wlc->pub->up) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

#if defined(BCMDBG)
static int
wlc_dump_stf(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;
	wlc_stf_t *stf = wlc->stf;
	enum wlc_bandunit bandunit;

	bcm_bprintf(b, "STF dump:\n");

	bcm_bprintf(b, "hw_txchain 0X%x hw_rxchain 0x%x\n",
		wlc->stf->hw_txchain, wlc->stf->hw_rxchain);
	bcm_bprintf(b, "sw_mask enable %d sw_txchain 0X%x sw_rxchain 0x%x %s\n",
		wlc->stf->sr13_en_sw_txrxchain_mask, wlc->stf->sw_txchain_mask,
		wlc->stf->sw_rxchain_mask,
		wlc->stf->core3_p1c ? "+ 0x8 (P1C)" : "");
	bcm_bprintf(b, "txchain 0x%x op_txstreams %d \nrxchain 0x%x op_rxstreams %d\n",
		wlc->stf->txchain, wlc->stf->op_txstreams, wlc->stf->rxchain,
		wlc->stf->op_rxstreams);
	bcm_bprintf(b, "txchain subvals:\n");
	for (i = 0; i < WLC_TXCHAIN_ID_COUNT; i++)
		bcm_bprintf(b, "\tid:%d 0x%x\n", i, stf->txchain_subval[i]);

	bcm_bprintf(b, "txant %d ant_rx_ovr %d phytxant 0x%x\n",
		wlc->stf->txant, wlc->stf->ant_rx_ovr, wlc->stf->phytxant);
	bcm_bprintf(b, "txcore CCK %d, OFDM %d, MCS Nsts[4..1] %d %d %d %d\n",
		wlc->stf->txcore[CCK_IDX][1], wlc->stf->txcore[OFDM_IDX][1],
		wlc->stf->txcore[NSTS4_IDX][1], wlc->stf->txcore[NSTS3_IDX][1],
		wlc->stf->txcore[NSTS2_IDX][1], wlc->stf->txcore[NSTS1_IDX][1]);
	bcm_bprintf(b, "stf_ss_auto: %d ss_opmode %d ss_algo_channel 0x%x no_cddstbc %d\n",
		wlc->stf->ss_algosel_auto, wlc->stf->ss_opmode, wlc->stf->ss_algo_channel,
		wlc->stf->no_cddstbc);
	bcm_bprintf(b, "op_stf_ss: %s", wlc->band->band_stf_stbc_tx == AUTO ? "STBC" :
		(wlc->stf->ss_opmode == PHY_TXC1_MODE_SISO ? "SISO" : "CDD"));

	FOREACH_WLC_BAND(wlc, bandunit) {
		bcm_bprintf(b, "\tband%d stf_ss: %s ", bandunit,
			wlc->bandstate[bandunit]->band_stf_stbc_tx == AUTO ? "STBC" :
			(wlc->bandstate[bandunit]->band_stf_ss_mode == PHY_TXC1_MODE_SISO ?
			"SISO" : "CDD"));
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "ipaon %d\n", wlc->stf->ipaon);

	bcm_bprintf(b, "pwr_throttle %d throttle_state 0x%x\n",
		wlc->stf->pwr_throttle, wlc->stf->throttle_state);
	bcm_bprintf(b, "pwr_throttle_mask 0x%x pwr_throttle_test 0x%x\n",
		wlc->stf->pwr_throttle_mask, wlc->stf->pwr_throttle_test);
	bcm_bprintf(b, "pwr_throttle_gpio 0x%x pwr_throttle_config 0x%x\n",
		wlc->stf->pwrthrottle_pin, wlc->stf->pwrthrottle_config);

	bcm_bprintf(b, "tx_duty_cycle_pwr %d tx_duty_cycle_cck %d tx_duty_cycle_ofdm %d\n",
		wlc->stf->tx_duty_cycle_pwr, wlc->stf->tx_duty_cycle_cck,
		wlc->stf->tx_duty_cycle_ofdm);

	bcm_bprintf(b, "\n");

	return BCME_OK;
}

#define BPRINT_PPR_RATE_LOOP(buf, idx, len, rates)	\
			for (idx = 0; idx < len; idx++) { \
				if (rates[idx] == WL_RATE_DISABLED) \
					bcm_bprintf(buf, "  -");			\
				else \
					bcm_bprintf(buf, " %2d", rates[idx]); \
			}

static int
wlc_dump_ppr(wlc_info_t *wlc, struct bcmstrbuf *b) /* C_CHECK */
{
	uint i, j;
	uint n = (WLC_BITSCNT(wlc->stf->hw_txchain) > 3) ? WL_NUM_4x4_ELEMENTS :
			(PHYTYPE_HT_CAP(wlc->band) ? WL_NUM_3x3_ELEMENTS : WL_NUM_2x2_ELEMENTS);
	uint offset = PHYTYPE_HT_CAP(wlc->band) ? WL_NUM_3x3_ELEMENTS : 0;
	int8 *ptr;
	ppr_t *txpwr_ctl = wlc->stf->txpwr_ctl;
	const char *str = "";
#ifdef WL11AX
	bool is_he = FALSE;
	ppr_he_mcs_rateset_t he_mcs_pwrs;
#endif /* WL11AX */
	ppr_vht_mcs_rateset_t mcs_pwrs;
	ppr_dsss_rateset_t dsss_pwrs;
	ppr_ofdm_rateset_t ofdm_pwrs;
	bool bprint  = FALSE;
	int ret = BCME_OK;
	char chanbuf[CHANSPEC_STR_LEN];

#ifdef WL_SARLIMIT
	txpwr_ctl = wlc->stf->txpwr_ctl_qdbm;
	bcm_bprintf(b, "Power/Rate Dump (in 1/4dB): chanspec %s\n",
		wf_chspec_ntoa_ex(wlc->chanspec, chanbuf));
#else
	bcm_bprintf(b, "Power/Rate Dump (in 1/2dB): chanspec %s\n",
		wf_chspec_ntoa_ex(wlc->chanspec, chanbuf));
#endif /* WL_SARLIMIT */
	bcm_bprintf(b, "20MHz:");

	if (txpwr_ctl == NULL) {
		bcm_bprintf(b, "PPR dump fail.\n");
		return ret;
	}

	if (CHSPEC_IS2G(wlc->home_chanspec)) {
		ppr_get_dsss(txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_pwrs);
		bcm_bprintf(b, "\nCCK            ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_2, &dsss_pwrs);
			bcm_bprintf(b, "\nCCK CDD 1x2    ");
			BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_3, &dsss_pwrs);
				bcm_bprintf(b, "\nCCK CDD 1x3    ");
				BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_4,
						&dsss_pwrs);
					bcm_bprintf(b, "\nCCK CDD 1x4    ");
					BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS,
						dsss_pwrs.pwr);
				}
			}
		}
	}
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM           ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i + offset) {
			case 0:
				str = "MCS-SISO       ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS-CDD        ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS STBC       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS 8~15       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
			case 5:
				ptr = NULL;
				ASSERT(ptr);
				break;
			case 6:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 10:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 11:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 12:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 13:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 14:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 15:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
#ifdef WL11AC
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
#else
			BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_HT_MCS, ptr);
#endif // endif
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	is_he = WLC_HE_CAP_PHY(wlc);

	/* 20MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i + offset) {
				case 0:
					str = "HE MCS-SISO    ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE MCS-CDD     ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					ptr = NULL;
					ASSERT(ptr);
					break;
				case 6:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 10:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 11:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &he_mcs_pwrs,
						WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 12:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &he_mcs_pwrs,
						WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 13:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 14:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 15:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20, WL_TX_NSS_4,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n\n40MHz:\n");
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_pwrs);
	bcm_bprintf(b, "OFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i + offset) {
			case 0:
				str = "MCS-SISO       ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS-CDD        ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS STBC       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS 8~15       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
			case 5:
				ptr = NULL;
				ASSERT(ptr);
				break;
			case 6:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 10:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 11:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 12:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 13:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 14:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 15:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
#ifdef WL11AC
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
#else
			BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_HT_MCS, ptr);
#endif // endif
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 40MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i + offset) {
				case 0:
					str = "HE MCS-SISO    ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE MCS-CDD     ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					ptr = NULL;
					ASSERT(ptr);
					break;
				case 6:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 10:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 11:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 12:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 13:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 14:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 15:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40, WL_TX_NSS_4,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n\n20 in 40MHz:");
	if (CHSPEC_IS2G(wlc->home_chanspec)) {
		ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_pwrs);
		bcm_bprintf(b, "\nCCK         ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_2, &dsss_pwrs);
			bcm_bprintf(b, "\nCCK CDD 1x2    ");
			BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_3,
					&dsss_pwrs);
				bcm_bprintf(b, "\nCCK CDD 1x3    ");
				BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_4,
						&dsss_pwrs);
					bcm_bprintf(b, "\nCCK CDD 1x4    ");
					BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS,
						dsss_pwrs.pwr);
				}
			}
		}
	}
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_pwrs);
	bcm_bprintf(b, "\nOFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			&ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i) {
			case 0:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 5:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 6:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
#ifdef WL11AC
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
#else
			BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_HT_MCS, ptr);
#endif // endif
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 20 in 40MHz */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i) {
				case 0:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 3:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 4:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 5:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 6:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN40,
							WL_TX_NSS_4, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

#ifdef WL11AC
	bcm_bprintf(b, "\n\n80MHz:\n");
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_80, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_pwrs);
	bcm_bprintf(b, "OFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i + offset) {
			case 0:
				str = "MCS-SISO       ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS-CDD        ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS STBC       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS 8~15       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
			case 5:
				ptr = NULL;
				ASSERT(ptr);
				break;
			case 6:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 10:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 11:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 12:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 13:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 14:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 15:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 80MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i + offset) {
				case 0:
					str = "HE MCS-SISO    ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
				break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE MCS-CDD     ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
				break;
				case 2:
				case 3:
				case 4:
				case 5:
					ptr = NULL;
					ASSERT(ptr);
					break;
				case 6:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 10:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 11:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 12:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 13:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 14:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 15:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80, WL_TX_NSS_4,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
			bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n\n20 in 80MHz:");
	if (CHSPEC_IS2G(wlc->home_chanspec)) {
		ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_1, &dsss_pwrs);
		bcm_bprintf(b, "\nCCK            ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_2, &dsss_pwrs);
			bcm_bprintf(b, "\nCCK CDD 1x2    ");
			BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_3,
					&dsss_pwrs);
				bcm_bprintf(b, "\nCCK CDD 1x3    ");
				BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_4,
						&dsss_pwrs);
					bcm_bprintf(b, "\nCCK CDD 1x4    ");
					BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS,
						dsss_pwrs.pwr);
				}
			}
		}
	}
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_pwrs);
	bcm_bprintf(b, "\nOFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			&ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i) {
			case 0:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 5:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 6:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 20 in 80MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i) {
				case 0:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 3:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 4:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 5:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 6:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN80,
							WL_TX_NSS_4, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n\n40 in 80MHz:\n");
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_pwrs);
	bcm_bprintf(b, "OFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			&ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i + offset) {
			case 0:
				str = "MCS-SISO       ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS-CDD        ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS STBC       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS 8~15       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
			case 5:
				ptr = NULL;
				ASSERT(ptr);
				break;
			case 6:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 10:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 11:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 12:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 13:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 14:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 15:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 40 in 80MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i + offset) {
				case 0:
					str = "HE MCS-SISO    ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE MCS-CDD     ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					ptr = NULL;
					ASSERT(ptr);
					break;
				case 6:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 10:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 11:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 12:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 13:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 14:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 15:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN80,
							WL_TX_NSS_4, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

#ifdef WL11AC_160
	bcm_bprintf(b, "\n\n160MHz:\n");
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_160, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_pwrs);
	bcm_bprintf(b, "OFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_160, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i + offset) {
			case 0:
				str = "MCS-SISO       ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS-CDD        ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS STBC       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS 8~15       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
			case 5:
				ptr = NULL;
				ASSERT(ptr);
				break;
			case 6:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 10:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 11:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 12:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 13:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 14:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 15:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 160MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i + offset) {
				case 0:
					str = "HE MCS-SISO    ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE MCS-CDD     ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					ptr = NULL;
					ASSERT(ptr);
					break;
				case 6:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 10:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 11:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 12:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 13:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 14:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 15:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_160, WL_TX_NSS_4,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n\n20 in 160MHz:");
	if (CHSPEC_IS2G(wlc->home_chanspec)) {
		ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_CHAINS_1, &dsss_pwrs);
		bcm_bprintf(b, "\nCCK            ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_CHAINS_2, &dsss_pwrs);
			bcm_bprintf(b, "\nCCK CDD 1x2    ");
			BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_CHAINS_3,
					&dsss_pwrs);
				bcm_bprintf(b, "\nCCK CDD 1x3    ");
				BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS, dsss_pwrs.pwr);
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_CHAINS_4,
						&dsss_pwrs);
					bcm_bprintf(b, "\nCCK CDD 1x4    ");
					BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_DSSS,
						dsss_pwrs.pwr);
				}
			}
		}
	}
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_pwrs);
	bcm_bprintf(b, "\nOFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			&ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i) {
			case 0:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 5:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 6:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 40 in 160MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i) {
				case 0:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 3:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 4:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 5:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 6:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_20IN160,
							WL_TX_NSS_4, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n\n40 in 160MHz:\n");
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_pwrs);
	bcm_bprintf(b, "OFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			&ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i + offset) {
			case 0:
				str = "MCS-SISO       ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS-CDD        ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS STBC       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS 8~15       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
			case 5:
				ptr = NULL;
				ASSERT(ptr);
				break;
			case 6:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 10:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 11:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 12:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 13:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 14:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 15:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 40 in 160MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i + offset) {
				case 0:
					str = "HE MCS-SISO    ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE MCS-CDD     ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					ptr = NULL;
					ASSERT(ptr);
					break;
				case 6:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 10:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 11:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 12:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 13:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 14:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 15:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_40IN160,
							WL_TX_NSS_4, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */

	bcm_bprintf(b, "\n\n80 in 160MHz:\n");
	ppr_get_ofdm(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_pwrs);
	bcm_bprintf(b, "OFDM           ");
	BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ofdm(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			&ofdm_pwrs);
		bcm_bprintf(b, "\nOFDM-CDD       ");
		BPRINT_PPR_RATE_LOOP(b, j, WL_RATESET_SZ_OFDM, ofdm_pwrs.pwr);
	}
	ptr = mcs_pwrs.pwr;
	for (i = 0; i < n; i++) {
		switch (i + offset) {
			case 0:
				str = "MCS-SISO       ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 1:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS-CDD        ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 2:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS STBC       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 3:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "MCS 8~15       ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 4:
			case 5:
				ptr = NULL;
				ASSERT(ptr);
				break;
			case 6:
				str = "1 Nsts 1 Tx    ";
				ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
					WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
				bprint = TRUE;
				break;
			case 7:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "1 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 8:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "1 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 9:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "1 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 10:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
					str = "2 Nsts 2 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 11:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "2 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 12:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "2 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 13:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					str = "3 Nsts 3 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 14:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "3 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			case 15:
				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					str = "4 Nsts 4 Tx    ";
					ppr_get_vht_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					bprint = TRUE;
				}
				break;
			default:
				ptr = NULL;
				ASSERT(ptr);
				break;
		}
		if (bprint) {
			bcm_bprintf(b, "\n%s", str);
			BPRINT_PPR_RATE_LOOP(b, j, sizeof(mcs_pwrs), ptr);
			bprint = FALSE;
		}
	}

#ifdef WL11AX
	/* 80 in 160MHz HE */
	if (is_he) {
		ptr = he_mcs_pwrs.pwr;
		for (i = 0; i < n; i++) {
			switch (i + offset) {
				case 0:
					str = "HE MCS-SISO    ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 1:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE MCS-CDD     ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 2:
				case 3:
				case 4:
				case 5:
					ptr = NULL;
					ASSERT(ptr);
					break;
				case 6:
					str = "HE 1 Nsts 1 Tx ";
					ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160, WL_TX_NSS_1,
						WL_TX_MODE_NONE, WL_TX_CHAINS_1, &he_mcs_pwrs,
						WL_HE_RT_SU);
					bprint = TRUE;
					break;
				case 7:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 1 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 8:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 1 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 9:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 1 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_1, WL_TX_MODE_CDD,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 10:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
						str = "HE 2 Nsts 2 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_2, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 11:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 2 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 12:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 2 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_2, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 13:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
						str = "HE 3 Nsts 3 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_3, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 14:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 3 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_3, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				case 15:
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						str = "HE 4 Nsts 4 Tx ";
						ppr_get_he_mcs(txpwr_ctl, WL_TX_BW_80IN160,
							WL_TX_NSS_4, WL_TX_MODE_NONE,
							WL_TX_CHAINS_4, &he_mcs_pwrs, WL_HE_RT_SU);
						bprint = TRUE;
					}
					break;
				default:
					ptr = NULL;
					ASSERT(ptr);
					break;
			}
			if (bprint) {
				bcm_bprintf(b, "\n%s", str);
				BPRINT_PPR_RATE_LOOP(b, j, sizeof(he_mcs_pwrs), ptr);
				bprint = FALSE;
			}
		}
	}
#endif /* WL11AX */
#endif /* WL11AC_160 */
#endif /* WL11AC */
	bcm_bprintf(b, "\n\n");

	return ret;
}

#endif // endif

static uint
wlc_stf_get_ppr_offset(wlc_info_t *wlc, wl_txppr_t *pbuf)
{
	uint ret = 0;
	ppr_t *txpwr_ctl = wlc->stf->txpwr_ctl;
#if defined(WL_EXPORT_CURPOWER) && defined(WL11AX)
	ppr_ru_t *ru_txpwr_ctl = wlc->stf->ru_txpwr_ctl;
	bool ulofdma = FALSE;	/* UL-OFDMA */

	if (pbuf->flags & WL_TX_POWER_F_HE_RU) {
		ulofdma = TRUE;
	}
	/* clear flags */
	pbuf->flags = 0;
#endif /* WL_EXPORT_CURPOWER && WL11AX */
	pbuf->len = WL_TXPPR_LENGTH;
	pbuf->ver = WL_TXPPR_VERSION;

	pbuf->chanspec = WLC_BAND_PI_RADIO_CHANSPEC;
	if (wlc->pub->associated)
		pbuf->local_chanspec = wlc->home_chanspec;

	pbuf->flags = WL_TX_POWER_F_HT | WL_TX_POWER_F_MIMO;
#ifdef WL11AC
	if (WLCISACPHY(wlc->band))
		pbuf->flags |= WL_TX_POWER_F_VHT;
#endif // endif

#ifdef WL11AX
	if (WLC_HE_CAP_PHY(wlc)) {
		pbuf->flags |= WL_TX_POWER_F_HE;
	}
#endif /* WL11AX */

	if (wlc->stf->txpwr_ctl == NULL) {
		return 0;
	}

#ifdef WL_SARLIMIT
	pbuf->flags |= WL_TX_POWER_F_UNIT_QDBM;
	txpwr_ctl = wlc->stf->txpwr_ctl_qdbm;
#endif /* WL_SARLIMIT */
	(void)ppr_serialize(txpwr_ctl, pbuf->pprbuf, pbuf->buflen, &ret);
#if defined(WL_EXPORT_CURPOWER) && defined(WL11AX)
	if (ret && ulofdma) {
		uint8 *pserbuf;
		uint32 ru_len;

		pserbuf = pbuf->pprbuf + ppr_ser_size(txpwr_ctl);
		ru_len = pbuf->buflen - ppr_ser_size(txpwr_ctl);
		ppr_ru_convert_to_tlv(ru_txpwr_ctl, pserbuf, ru_len,
			WLC_BITSCNT(wlc->stf->hw_txchain));

		pbuf->flags |= WL_TX_POWER_F_HE_RU;
	}
#endif /* WL_EXPORT_CURPOWER && WL11AX */
	return ret;
}

/** formula:  IDLE_BUSY_RATIO_X_16 = (100-duty_cycle)/duty_cycle*16 */
int
wlc_stf_duty_cycle_set(wlc_info_t *wlc, int duty_cycle, bool isOFDM, bool writeToShm)
{
	int idle_busy_ratio_x_16 = 0;
	uint offset = 0;

	if (D11REV_LT(wlc->pub->corerev, 40))
		offset = isOFDM ? M_TX_IDLE_BUSY_RATIO_X_16_OFDM(wlc) :
			M_TX_IDLE_BUSY_RATIO_X_16_CCK(wlc);
	else
		offset = isOFDM ? M_TXDUTY_RATIOX16_OFDM(wlc) : M_TXDUTY_RATIOX16_CCK(wlc);

	if (duty_cycle > NO_DUTY_THROTTLE || duty_cycle < MIN_DUTY_CYCLE_ALLOWED) {
		WL_ERROR(("wl%d:  duty cycle value off limit\n", wlc->pub->unit));
		return BCME_RANGE;
	}
	if (duty_cycle) {
		idle_busy_ratio_x_16 = (100 - duty_cycle) * 16 / duty_cycle;
	}
	/* Only write to shared memory  when wl is up */
	if (writeToShm)
		wlc_write_shm(wlc, offset, (uint16)idle_busy_ratio_x_16);

	return BCME_OK;
}

void
wlc_txduty_upd(wlc_info_t *wlc)
{
	uint8 duty_cycle = NO_DUTY_THROTTLE;
	uint16 duty_cycle_ofdm = wlc->stf->tx_duty_cycle_ofdm;
	uint16 duty_cycle_cck = wlc->stf->tx_duty_cycle_cck;
#ifdef WL11AC
	uint16 strt_rate = 0xffff;
#endif /* WL11AC */

	/* Select minimum of the defined dutycycles */
	if (TVPM_ENAB(wlc->pub)) {
		/* - find max allowed thermal TXDC
		 * - compare it to TXDC values calculated from other inputs
		 * - set the final value
		 */
		duty_cycle = MIN(wlc->stf->tx_duty_cycle_max, wlc_tvpm_get_txdc_min(wlc->tvpm));
		duty_cycle = wlc_tvpm_set_target_txdc(wlc->tvpm, duty_cycle);
	} else {
		if (wlc->stf->throttle_state & WLC_TEMPTHROTTLE_ON) {
			duty_cycle = MIN(wlc->stf->tx_duty_cycle_max,
				wlc->stf->tx_duty_cycle_thermal);
		}
		if (wlc->stf->throttle_state & WLC_PWRTHROTTLE_ON) {
			duty_cycle = MIN(duty_cycle, wlc->stf->tx_duty_cycle_pwr);
#ifdef WL11AC
			if (D11REV_GE(wlc->pub->corerev, 40)) {
				bool isj28 = (wlc->pub->sih->boardvendor == VENDOR_APPLE &&
					((wlc->pub->sih->boardtype == BCM94360J28_D11AC2G) ||
					(wlc->pub->sih->boardtype == BCM94360J28_D11AC5G)));
				chanspec_t chanspec = wlc->chanspec;
				if (CHSPEC_IS5G(chanspec) && !isj28) {
					if (CHSPEC_IS40(chanspec) &&
						wlc->stf->tx_duty_cycle_ofdm_40_5g)
						strt_rate = wlc->stf->tx_duty_cycle_thresh_40_5g;
					else if (CHSPEC_IS80(chanspec) &&
						wlc->stf->tx_duty_cycle_ofdm_80_5g)
						strt_rate = wlc->stf->tx_duty_cycle_thresh_80_5g;
				}
			}
#endif /* WL11AC */
		}
	}

	if (wlc->pub->up) {
#ifdef TVPM_DEBUG
		WL_PRINT("sl.%d set duty_cycle=%u\n",
			wlc->pub->unit, duty_cycle);
#endif /* TVPM_DEBUG */
		duty_cycle_ofdm = MIN(duty_cycle_ofdm, duty_cycle);
		duty_cycle_cck = MIN(duty_cycle_cck, duty_cycle);
		wlc_stf_duty_cycle_set(wlc, duty_cycle_ofdm, TRUE, TRUE);
		wlc_stf_duty_cycle_set(wlc, duty_cycle_cck, FALSE, TRUE);
#ifdef WL11AC
		if (D11REV_GE(wlc->pub->corerev, 40))
			wlc_write_shm(wlc, M_DUTY_STRTRATE(wlc), strt_rate);
#endif /* WL11AC */
	}
}

void
wlc_stf_txduty_upd(wlc_info_t *wlc)
{
	wlc_txduty_upd(wlc);
}
#ifdef WL11AC

#define CORE_MASK(core)	(1<<(core))
/* This function sets up the stf core for 160M and 80p80 operation in dual-core PHY chip
 * Basically NSS=1, but PhyCore=2; Ex: 4349/4355/4359
 * In 80p80/160 mode, sub-band (control-channel) frames needs to be sent only from primary core.
 * Primary Core is defined based on where the control channel is for a given chanspec.
 * i.e: Lower 80 (Core-0:0x1), Upper 80 (Core-1:0x2), Full BW frame (Core-0 and Core-1:0x3)
 */
static void
wlc_stf_update_160mode_txcore(wlc_stf_t *stf, chanspec_t chanspec)
{
	ASSERT(CHSPEC_IS8080(chanspec) || CHSPEC_IS160(chanspec));
	if (stf->channel_bonding_cores == 0)  {
		WL_ERROR(("%s:160M STF not supported\n", __FUNCTION__));
		return;
	}
	if (CHSPEC_CTL_SB(chanspec) <= WL_CHANSPEC_CTL_SB_LUU) {
		stf->txcore[CCK_IDX][1]  = CORE_MASK(0);
		stf->txcore[OFDM_IDX][1] = CORE_MASK(0);
	} else {
		stf->txcore[CCK_IDX][1]  = CORE_MASK(1);
		stf->txcore[OFDM_IDX][1] = CORE_MASK(1);
	}
	stf->txcore[NSTS1_IDX][1] = CORE_MASK(0) | CORE_MASK(1);
	stf->txcore[NSTS2_IDX][1] = 0;
	stf->txcore[NSTS3_IDX][1] = 0;
	stf->txcore[NSTS4_IDX][1] = 0;
}

/** Update tx duty cycle when changing chanspec */
void
wlc_stf_chanspec_upd(wlc_info_t *wlc)
{
	if (!wlc->pub->up)
		return;

	if (!WLCISACPHY(wlc->band))
		return;

	/* wlc->chanspec is updated before calling this function */
	if (!(ACREV_IS(wlc->band->phyrev, 32) || ACREV_IS(wlc->band->phyrev, 33) ||
		ACREV_IS(wlc->band->phyrev, 47)) &&
		(CHSPEC_IS8080(wlc->chanspec) || CHSPEC_IS160(wlc->chanspec))) {
		wlc_stf_update_160mode_txcore(wlc->stf, wlc->chanspec);
	} else {
		wlc_stf_init_txcore_default(wlc);
		wlc_stf_spatial_mode_set(wlc, wlc->chanspec);
	}
	wlc_txduty_upd(wlc);
#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub))
		wlc_txbf_chanspec_upd(wlc->txbf);
#endif // endif
	return;
}
#endif /* WL11AC */

int
wlc_stf_txchain_upd(wlc_info_t *wlc, bool set_txchain, uint16 id)
{
	uint8 txchain = 0;
#ifdef WL_STF_ARBITRATOR
	void *req = NULL;

	if (id == WLC_TXCHAIN_ID_PWRTHROTTLE) {
		req = wlc->stf->arb->pwrthrottle_req;
	} else if (id == WLC_TXCHAIN_ID_TEMPSENSE) {
		req = wlc->stf->arb->tmpsense_req;
	} else {
		return BCME_ERROR;
	}
#endif /* WL_STF_ARBITRATOR */

	/* update the tempsense txchain setting */
	if (set_txchain) {
		if (WLCISACPHY(wlc->band))
			txchain = wlc_stf_get_target_core(wlc);
#ifdef WL_STF_ARBITRATOR
		if (WLC_STF_ARB_ENAB(wlc->pub)) {
			wlc_stf_nss_request_update(wlc, req,
				WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE,
				txchain, WLC_BITSCNT(txchain),
				wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
		} else
#endif /* WL_STF_ARBITRATOR */
			wlc_stf_txchain_set(wlc, txchain, TRUE, id);
	} else {
#ifdef WL_STF_ARBITRATOR
		if (WLC_STF_ARB_ENAB(wlc->pub)) {
			wlc_stf_nss_request_update(wlc, req,
				WLC_STF_ARBITRATOR_REQ_STATE_RXTX_INACTIVE,
				wlc->stf->hw_txchain, WLC_BITSCNT(wlc->stf->hw_txchain),
				wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
		} else
#endif /* WL_STF_ARBITRATOR */
			wlc_stf_txchain_reset(wlc, id);
	}
	WL_PRINT(("wl%d: %s: txchain update: hw_txchain 0x%x stf->txchain 0x%x txchain 0x%x\n",
		wlc->pub->unit, __FUNCTION__, wlc->stf->hw_txchain, wlc->stf->txchain, txchain));
	return BCME_OK;
}

/* every WLC_TEMPSENSE_PERIOD seconds temperature check to decide whether to turn on/off txchain */
void
wlc_stf_tempsense_upd(wlc_info_t *wlc)
{
	wlc_phy_t *pi = WLC_PI(wlc);
	uint16 duty_cycle_active_chains;
	uint8 txchain;
	uint8 temp_throttle_req;
	uint8 duty_cycle;

	if (ISSIM_ENAB(wlc->pub->sih)) {
		return;
	}

	if (!WLCISMIMO || wlc->stf->tempsense_disable) {
		return;
	}

	if ((wlc->pub->now - wlc->stf->tempsense_lasttime) < wlc->stf->tempsense_period) {
		return;
	}
	wlc->stf->tempsense_lasttime = wlc->pub->now;

	/* XXX Check if the chip is too hot. Disable Tx chain(s), if it is
	 * duty_cycle_active_chains: upper 8bits are for Duty Cycle
	 * lower 8bits are for active chain.
	 * active chain format: high 4 bits are for Rx chain, low 4 bits are  for Tx chain
	 */

	duty_cycle_active_chains = phy_stf_duty_cycle_chain_active_get(pi);
	txchain = PHY_STF_GET_TX_CHAINS(duty_cycle_active_chains);
	duty_cycle = PHY_STF_GET_DUTYCYCLE(duty_cycle_active_chains);

	temp_throttle_req = phy_temp_is_heatedup(pi) ?
		WLC_TEMPTHROTTLE_ON : WLC_THROTTLE_OFF;

	if (temp_throttle_req) {
		/*
		 * For RSDB mode, duty_cycle in duty_cycle_active_chains
		 * is only changed in RSDB mode
		*/
		if (wlc->stf->tx_duty_cycle_thermal != duty_cycle) {
			wlc->stf->tx_duty_cycle_thermal = duty_cycle;
		} else {
			if (ACREV_IS(wlc->band->phyrev, 32) || ACREV_IS(wlc->band->phyrev, 33) ||
					ACREV_IS(wlc->band->phyrev, 47) ||
					ACREV_IS(wlc->band->phyrev, 129)) {
				if (((wlc->stf->throttle_state & WLC_TEMPTHROTTLE_ON) ==
					temp_throttle_req) && (WLC_BITSCNT(txchain) ==
					WLC_BITSCNT(wlc->stf->txchain))) {
					return;
				}
			} else {
				if ((wlc->stf->throttle_state & WLC_TEMPTHROTTLE_ON) ==
						temp_throttle_req)
					return;
			}
		}
		wlc->stf->throttle_state |= temp_throttle_req;
	} else {
		wlc->stf->throttle_state &= ~WLC_TEMPTHROTTLE_ON;
	}

	ASSERT(wlc->pub->up);
	if (!wlc->pub->up)
		return;

	wlc_txduty_upd(wlc);

	if (wlc->stf->thermal_throttle_features & THERMAL_THROTTLE_BYPASS_TXCHAIN)
		return;
	/* update the tempsense txchain setting */
	/* The following section is moved to wlc_stf_txchain_upd()
	 * keep it for 4357b0
	 wlc_stf_txchain_upd(wlc, (wlc->stf->throttle_state & WLC_TEMPTHROTTLE_ON)
		? TRUE : FALSE);
	 */
	if (txchain) {
		if (wlc->stf->throttle_state & WLC_TEMPTHROTTLE_ON) {
			if (WLCISACPHY(wlc->band) &&
				!(ACREV_IS(wlc->band->phyrev, 32) ||
					ACREV_IS(wlc->band->phyrev, 33) ||
					ACREV_IS(wlc->band->phyrev, 47) ||
					ACREV_IS(wlc->band->phyrev, 129))) {
				txchain = wlc_stf_get_target_core(wlc);
			}
#ifdef WL_STF_ARBITRATOR
			if (WLC_STF_ARB_ENAB(wlc->pub)) {
				wlc_stf_nss_request_update(wlc, wlc->stf->arb->tmpsense_req,
					WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE,
					txchain, WLC_BITSCNT(txchain),
					wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
			} else
#endif /* WL_STF_ARBITRATOR */
				wlc_stf_txchain_set(wlc, txchain, TRUE, WLC_TXCHAIN_ID_TEMPSENSE);
		} else {
#ifdef WL_STF_ARBITRATOR
			if (WLC_STF_ARB_ENAB(wlc->pub)) {
				wlc_stf_nss_request_update(wlc, wlc->stf->arb->tmpsense_req,
					WLC_STF_ARBITRATOR_REQ_STATE_RXTX_INACTIVE,
					wlc->stf->hw_txchain, WLC_BITSCNT(wlc->stf->hw_txchain),
					wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
			} else
#endif /* WL_STF_ARBITRATOR */
				wlc_stf_txchain_reset(wlc, WLC_TXCHAIN_ID_TEMPSENSE);
		}
		WL_NONE(("wl%d: %s: txchain update: hw_txchain 0x%x "
					"stf->txchain 0x%x txchain 0x%x\n", wlc->pub->unit,
						__FUNCTION__, wlc->stf->hw_txchain,
							wlc->stf->txchain, txchain));
	}
}

void
wlc_stf_ss_algo_channel_get(wlc_info_t *wlc, uint16 *ss_algo_channel, chanspec_t chanspec)
{
	int8 siso_mcs_power;
	int8 cdd_mcs_power = 0;
	ppr_ht_mcs_rateset_t temp_mcs_group;
	wl_tx_bw_t bw;
	int8 stbc_mcs_power = 0;

	/* Clear previous settings */
	*ss_algo_channel = 0;

	if (!wlc->pub->up) {
		*ss_algo_channel = (uint16)-1;
		return;
	}

#ifdef WL11AC
	if (CHSPEC_IS160(chanspec)) {
		bw = WL_TX_BW_160;
	} else if (CHSPEC_IS80(chanspec)) {
		bw = WL_TX_BW_80;
	} else
#endif // endif
	if (CHSPEC_IS40(chanspec)) {
		bw = WL_TX_BW_40;
	} else {
		bw = WL_TX_BW_20;
	}

	ppr_get_ht_mcs(wlc->stf->txpwr_ctl, bw, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&temp_mcs_group);
	siso_mcs_power = (temp_mcs_group.pwr[0] == WL_RATE_DISABLED) ?
		wlc->stf->max_offset : temp_mcs_group.pwr[0];

	if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
		ppr_get_ht_mcs(wlc->stf->txpwr_ctl, bw, WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			&temp_mcs_group);
		cdd_mcs_power = (temp_mcs_group.pwr[0] == WL_RATE_DISABLED) ?
			wlc->stf->max_offset : temp_mcs_group.pwr[0];
		ppr_get_ht_mcs(wlc->stf->txpwr_ctl, bw, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2, &temp_mcs_group);
		stbc_mcs_power = (temp_mcs_group.pwr[0] == WL_RATE_DISABLED) ?
			wlc->stf->max_offset : temp_mcs_group.pwr[0];
	}

	/* criteria to choose stf mode */

	/* contents of wlc->stf->txpwr_ctl are negative offsets in half dBm */
	/* the "+3dbm (6 0.5dBm units)" is to account for the fact that with CDD, tx occurs
	 * on both chains
	 */
	if ((PHYCORENUM(wlc->stf->op_txstreams) == 1) || ((siso_mcs_power + 6) < cdd_mcs_power))
		setbit(ss_algo_channel, PHY_TXC1_MODE_SISO);
	else
		setbit(ss_algo_channel, PHY_TXC1_MODE_CDD);

	/* STBC is ORed into to algo channel as STBC requires per-packet SCB capability check
	 * so cannot be default mode of operation. One of SISO, CDD have to be set
	 */
	if ((PHYCORENUM(wlc->stf->op_txstreams) > 1) && ((siso_mcs_power + 6) >= stbc_mcs_power))
		setbit(ss_algo_channel, PHY_TXC1_MODE_STBC);
}

bool
wlc_stf_stbc_rx_set(wlc_info_t* wlc, int32 int_val)
{
	if ((int_val != HT_CAP_RX_STBC_NO) && (int_val != HT_CAP_RX_STBC_ONE_STREAM)) {
		return FALSE;
	}

	/* PHY cannot receive STBC with only one rx core active */
	if (WLC_STF_NO_STBC_RX_1_CORE(wlc)) {
		if ((int_val != HT_CAP_RX_STBC_NO) && (wlc->stf->op_rxstreams == 1))
			return FALSE;
	}

	wlc_stf_stbc_rx_ht_update(wlc, int_val);
	return TRUE;
}

#ifdef RXCHAIN_PWRSAVE
/** called when enter rxchain_pwrsave mode */
uint8
wlc_stf_enter_rxchain_pwrsave(wlc_info_t *wlc)
{
	uint8 ht_cap_rx_stbc = wlc_ht_stbc_rx_get(wlc->hti);
	/* need to save and disable rx_stbc HT capability before enter rxchain_pwrsave mode */
	/* PHY cannot receive STBC with only one rx core active */
	if (WLC_STF_NO_STBC_RX_1_CORE(wlc) && WLC_STBC_CAP_PHY(wlc) &&
		(ht_cap_rx_stbc != HT_CAP_RX_STBC_NO)) {
		wlc_stf_stbc_rx_set(wlc, HT_CAP_RX_STBC_NO);
	}
	return ht_cap_rx_stbc;
}

/** called when exit rxchain_pwrsave mode */
void
wlc_stf_exit_rxchain_pwrsave(wlc_info_t *wlc, uint8 ht_cap_rx_stbc)
{
	/* need to restore rx_stbc HT capability after exit rxchain_pwrsave mode */
	/* PHY cannot receive STBC with only one rx core active */
	if (WLC_STF_NO_STBC_RX_1_CORE(wlc) && WLC_STBC_CAP_PHY(wlc) &&
		(ht_cap_rx_stbc != HT_CAP_RX_STBC_NO)) {
		wlc_stf_stbc_rx_set(wlc, ht_cap_rx_stbc);
	}
}

int
wlc_stf_rxstreams_get(wlc_info_t* wlc)
{
	if ((wlc->ap != NULL) && RXCHAIN_PWRSAVE_ENAB(wlc->ap)) {
		uint8 rxchain = wlc_rxchain_pwrsave_get_rxchain(wlc);
		return WLC_BITSCNT(rxchain);
	}
	return wlc->stf->rxstreams;
}
#endif /* RXCHAIN_PWRSAVE */

static int
wlc_stf_ss_get(wlc_info_t* wlc, enum wlc_bandunit bandunit)
{
	wlcband_t *wlc_band = NULL;

	if (bandunit == -1)
		wlc_band = wlc->band;
	else if ((bandunit < 0) || (bandunit >= MAXBANDS))
		return BCME_RANGE;
	else
		wlc_band = wlc->bandstate[bandunit];

	return (int)(wlc_band->band_stf_ss_mode);
}

static bool
wlc_stf_ss_set(wlc_info_t* wlc, int32 int_val, enum wlc_bandunit bandunit)
{
	wlcband_t *wlc_band = NULL;

	if (bandunit == -1)
		wlc_band = wlc->band;
	else if ((bandunit < 0) || (bandunit >= MAXBANDS))
		return FALSE;
	else
		wlc_band = wlc->bandstate[bandunit];

	if ((int_val == PHY_TXC1_MODE_CDD) && (wlc->stf->op_txstreams == 1)) {
		return FALSE;
	}

	if (int_val != PHY_TXC1_MODE_SISO && int_val != PHY_TXC1_MODE_CDD)
		return FALSE;

	wlc_band->band_stf_ss_mode = (int8)int_val;
	wlc_stf_ss_update(wlc, wlc_band);

	return TRUE;
}

static bool
wlc_stf_ss_auto(wlc_info_t* wlc)
{
	return wlc->stf->ss_algosel_auto;
}

static int
wlc_stf_ss_auto_set(wlc_info_t *wlc, bool enable)
{
	if (wlc->stf->ss_algosel_auto == enable)
		return 0;

	if (WLC_STBC_CAP_PHY(wlc) && enable)
		wlc_stf_ss_algo_channel_get(wlc, &wlc->stf->ss_algo_channel, wlc->chanspec);

	wlc->stf->ss_algosel_auto = enable;
	wlc_stf_ss_update(wlc, wlc->band);

	return 0;
}

static void
wlc_stf_spatial_mode_set(wlc_info_t *wlc, chanspec_t chanspec)
{
	uint8 channel = CHSPEC_CHANNEL(wlc->chanspec);
	int8 mode = AUTO;

	if (CHSPEC_IS2G(chanspec))
		mode = wlc->stf->spatial_mode_config[SPATIAL_MODE_2G_IDX];
	else {
		if (channel < CHANNEL_5G_MID_START)
			mode = wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_LOW_IDX];
		else if (channel < CHANNEL_5G_HIGH_START)
			mode = wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_MID_IDX];
		else if (channel < CHANNEL_5G_UPPER_START)
			mode = wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_HIGH_IDX];
		else
			mode = wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_UPPER_IDX];
	}
	WL_NONE(("wl%d: %s: channel %d mode %d\n", wlc->pub->unit, __FUNCTION__, channel, mode));

	wlc_stf_spatial_policy_set(wlc, mode);
}

static int
wlc_stf_spatial_mode_upd(wlc_info_t *wlc, int8 *mode)
{
	WL_TRACE(("wl%d: %s: update Spatial Policy\n", wlc->pub->unit, __FUNCTION__));

	if (!WLCISACPHY(wlc->band))
		return BCME_UNSUPPORTED;

	bcopy(mode, wlc->stf->spatial_mode_config, SPATIAL_MODE_MAX_IDX);
	WL_NONE(("wl%d: %s mode %d %d %d %d %d\n", wlc->pub->unit, __FUNCTION__,
		wlc->stf->spatial_mode_config[SPATIAL_MODE_2G_IDX],
		wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_LOW_IDX],
		wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_MID_IDX],
		wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_HIGH_IDX],
		wlc->stf->spatial_mode_config[SPATIAL_MODE_5G_UPPER_IDX]));

	wlc_stf_spatial_mode_set(wlc, wlc->chanspec);
	return BCME_OK;
}

int8
wlc_stf_stbc_tx_get(wlc_info_t* wlc)
{
	return wlc->band->band_stf_stbc_tx;
}

static uint8
wlc_stf_spatial_map(wlc_info_t *wlc, uint8 idx)
{
	uint8 ncores = (uint8)WLC_BITSCNT(wlc->stf->txcore[idx][1]);
	uint8 Nsts = wlc->stf->txcore[idx][0];

	if (wlc->stf->op_txstreams < Nsts)
		return 0;

	ASSERT(ncores <= wlc->stf->txstreams);
	/* ncores can be 0 for non-supported Nsts */
	if (ncores == 0)
		return 0;

	if (Nsts == ncores) return 0;
	else if (Nsts == 1 && ncores == 2) return 1;
	else if (Nsts == 1 && ncores == 3) return 2;
	else if (Nsts == 2 && ncores == 3) return 3;
	else ASSERT(0);
	return 0;
}

static void
wlc_stf_txcore_set(wlc_info_t *wlc, uint8 idx, uint8 core_mask)
{
	WL_TRACE(("wl%d: wlc_stf_txcore_set\n", wlc->pub->unit));

	ASSERT(idx < MAX_CORE_IDX);

	WL_NONE(("wl%d: %s: Nsts %d core_mask %x\n",
		wlc->pub->unit, __FUNCTION__, wlc->stf->txcore[idx][0], core_mask));

	if (WLC_BITSCNT(core_mask) > wlc->stf->txstreams) {
		WL_NONE(("wl%d: %s: core_mask(0x%x) > #tx stream(%d) supported, disable it\n",
			wlc->pub->unit, __FUNCTION__, core_mask, wlc->stf->txstreams));
		core_mask = 0;
	}

	if ((WLC_BITSCNT(core_mask) == wlc->stf->txstreams) &&
	    ((core_mask & ~wlc->stf->txchain) || !(core_mask & wlc->stf->txchain))) {
		WL_INFORM(("wl%d: %s: core_mask(0x%x) mismatch #txchain(0x%x), force to txchain\n",
			wlc->pub->unit, __FUNCTION__, core_mask, wlc->stf->txchain));
		core_mask = wlc->stf->txchain;
	}

	if ((idx == CCK_IDX || idx == OFDM_IDX) && (core_mask == 0)) {
		WL_ERROR(("wl%d: %s: Nsts %d core_mask %x"
			"txstreams = %d txchain = %d band = %d\n",
			wlc->pub->unit, __FUNCTION__, wlc->stf->txcore[idx][1],
			core_mask, wlc->stf->txstreams,
			wlc->stf->txchain, wlc->band->bandtype));
			ASSERT(core_mask);
			if (wlc->stf->txchain > 0)
				core_mask = wlc->stf->txcore[idx][1] = wlc->stf->txchain;
	} else {
		wlc->stf->txcore[idx][1] = core_mask;
	}

	if ((BAND_5G6G(wlc->band->bandtype) && idx == OFDM_IDX) ||
	    (wlc->band->bandtype == WLC_BAND_2G && idx == CCK_IDX)) {
		/* Needs to update beacon and ucode generated response
		 * frames when 1 stream core map changed
		 */
		wlc->stf->phytxant = core_mask << PHY_TXC_ANT_SHIFT;
		wlc_bmac_txant_set(wlc->hw, wlc->stf->phytxant);
		if (wlc->clk &&
		    wlc_valid_rate(wlc, wlc->bcn_rspec, wlc->band->bandtype, FALSE)) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_beacon_phytxctl_txant_upd(wlc, wlc->bcn_rspec);
			wlc_beacon_phytxctl(wlc, wlc->bcn_rspec, wlc->chanspec);
			wlc_beacon_upddur(wlc, wlc->bcn_rspec, 0);
			wlc_enable_mac(wlc);
		}
	}

	WL_NONE(("wl%d: %s: IDX %d: Nsts %d Core mask 0x%x\n",
		wlc->pub->unit, __FUNCTION__, idx, wlc->stf->txcore[idx][0], core_mask));

	/* invalid tx cache due to core mask change */
	if (WLC_TXC_ENAB(wlc) && wlc->txc != NULL)
		wlc_txc_inv_all(wlc->txc);
} /* wlc_stf_txcore_set */

static void
wlc_stf_init_txcore_default(wlc_info_t *wlc)
{
	uint8 i;

	switch (wlc->stf->txchain) {
		case 0x06:
			wlc->stf->txcore_idx = 2;
			break;
		default:
			wlc->stf->txcore_idx = 1;
			break;
	}
	for (i = 0; i < MAX_CORE_IDX; i++) {
		/* fill in the Nsts */
		wlc->stf->txcore[i][0] = txcore_default[i][0];
		/* fill in the txcore bit map for all cores */
		/* XXX override single stream TX due to spur WAR
		 * X51A enabled: use core 1 for SISO TX @ 20MHz BW
		 * dBpad enabled: use core 1 for SISO TX @ 20MHz BW
		 * dBpad enabled: use core 2 for SISO TX @ 40/80MHz BW
		 */
		if (wlc->stf->coremask_override && CHSPEC_IS20(wlc->chanspec))
			wlc->stf->txcore[i][1] = 2; /* use core 1 */
		else if (wlc->stf->coremask_override == SPURWAR_OVERRIDE_DBPAD)
			wlc->stf->txcore[i][1] = 4; /* use core 2 */
		else
			wlc->stf->txcore[i][1] = txcore_default[i][wlc->stf->txcore_idx];
	}
}

/**
 * Return a core mask with the given number of cores, core_count, set.
 * Will return the prefered core mask if there are more cores
 * available for use than requested.
 * Will return a mask of 0 if the core_count is more than the number
 * of cores availible.
 */
static uint8
wlc_stf_core_mask_assign(wlc_stf_t *stf, uint core_count)
{
	uint8 txchain = stf->txchain;
	uint8 mask;

	if (core_count > stf->txstreams)
		return 0;

	/* if we want one core, just return core 0, 1, or 2 or 3,
	 * in that order of preference as available in the txchain mask
	 */
	if (core_count == 1) {
		/* XXX spur WAR for SISO TX:
		 * X51A enabled: use core 1 for SISO TX @ 20MHz BW
		 * dBpad enabled: use core 1 for SISO TX @ 20MHz BW
		 * dBpad enabled: use core 2 for SISO TX @ 40/80MHz BW
		 */
		if (stf->coremask_override == SPURWAR_OVERRIDE_X51A) {
			mask = 2;
			if ((txchain & mask) == 0)
				mask = 4;
			if ((txchain & mask) == 0)
				mask = 1;
			if ((txchain & mask) == 0)
				mask = 8;
			return mask;
		} else if (stf->coremask_override == SPURWAR_OVERRIDE_DBPAD) {
			mask = 4;
			if ((txchain & mask) == 0)
				mask = 1;
			if ((txchain & mask) == 0)
				mask = 2;
			if ((txchain & mask) == 0)
				mask = 8;
			return mask;
		} else {
			mask = 1;
			if ((txchain & mask) == 0)
				mask = 2;
			if ((txchain & mask) == 0)
				mask = 4;
			if ((txchain & mask) == 0)
				mask = 8;
			return mask;
		}
	}

	/* if we want 2 cores, return core numbers {0, 2}, {0, 1}, {1, 2}, {0, 3}, {1, 3}, {2, 3}
	 * in that order of preference as available in the txchain mask
	 */
	if (core_count == 2) {
		mask = 5;
		if ((txchain & mask) != mask)
			mask = 3;
		if ((txchain & mask) != mask)
			mask = 6;
		if ((txchain & mask) != mask)
			mask = 9;
		if ((txchain & mask) != mask)
			mask = 0xa;
		if ((txchain & mask) != mask)
			mask = 0xc;
		return mask;
	}

	/* 3 cores {0, 1, 2}, {0, 1, 3}, {0, 2, 3}, {1, 2, 3}, */
	if (core_count == 3) {
		mask = 7;
		if ((txchain & mask) != mask)
			mask = 0xb;
		if ((txchain & mask) != mask)
			mask = 0xd;
		if ((txchain & mask) != mask)
			mask = 0xe;
		return mask;
	}

	/* 4 cores */
	return 0xf;
}

/** return the max of an array of uint8 values */
static uint8
wlc_stf_uint8_vec_max(uint8 *vec, uint count)
{
	uint i;
	uint8 max = 0;
	uint8 v;

	if (vec == NULL || count == 0)
		return max;

	max = vec[0];
	for (i = 1; i < count; i++) {
		v = vec[i];
		if (v > max) {
			max = v;
		}
	}

	return max;
}

/** replace value 'find' by 'replace' in a vector of int8 values */
static void
wlc_stf_int8_vec_replace(int8 *vec, uint count, int8 find, int8 replace)
{
	uint i;

	if (vec == NULL)
		return;

	for (i = 0; i < count; i++) {
		if (vec[i] == find) {
			vec[i] = replace;
		}
	}

	return;
}

#ifdef WL11AC
#define NUM_MCS_RATES WL_NUM_RATES_VHT
#else
#define NUM_MCS_RATES WL_NUM_RATES_MCS_1STREAM
#endif // endif

static void
wlc_stf_txcore_select(wlc_info_t *wlc, uint8 *txcore) /* C_CHECK */
{
	ppr_t* txpwr = wlc->stf->txpwr_ctl;
	uint core_count[MAX_CORE_IDX];
	uint8 idx;
	uint8 nstreams = wlc->stf->txstreams;
	chanspec_t chanspec = wlc->chanspec;

	/* initialize core_count to just the matching Nsts to be the minimum cores for TX */
	for (idx = 0; idx < MAX_CORE_IDX; idx++)
		core_count[idx] = txcore_default[idx][0];

	if (txpwr == NULL) {
		goto assign_masks;
	}

	ASSERT(nstreams > 0);
	ASSERT(nstreams <= 4);

	/* if there is only one (or none) cores available, use the minimum core count
	 * for all modulations, and just jump to the end to get the mask assignments
	 */
	if (nstreams < 2)
		goto assign_masks;

	/* The txpwr array is 1/2 dB (hdB) offsets from a max power.  The power
	 * calculations that need to find the minimum power among modulation
	 * types take the max over the power offsets, then negate to find the
	 * minimum power target for the modulation type. The calculation is
	 * essentially treating the power offsets as offsets from zero.  Negating
	 * the offsets to actual power values keeps the greater-than/less-than
	 * calculations more straightforward instead of having to invert the
	 * sense of the comparison.
	 */

	/*
	 * CCK: Nsts == 1: 1Tx > 2Tx + 3dB > 3Tx + 4.8dB, then use 1Tx
	 */
	if (wlc->stf->txcore_override[CCK_IDX] & CCK_TX_DIVERSITY_BIT) {
		core_count[CCK_IDX] = 1;	/* always use SISO */
	} else {
		core_count[CCK_IDX] = wlc_stf_best_dsss_cores(wlc, txpwr, chanspec);
	}

	/*
	 * OFDM: 1Tx > 2Tx + 3dB > 3Tx + 4.8dB, then use 1Tx
	 */
	core_count[OFDM_IDX] = wlc_stf_best_ofdm_cores(wlc, txpwr, chanspec);

	/*
	 * Nsts 1: 1Tx >= 2Tx + 3dB >= 3Tx + 4.8dB, then use 1Tx
	 */
	core_count[NSTS1_IDX] = wlc_stf_best_nsts1_cores(wlc, txpwr, chanspec);

	if (nstreams == 2)
		goto assign_masks;

	/*
	 * Nsts 2: 2Tx >= 3Tx + 1.8dB, then use 2Tx
	 * We should never get here if nstreams < 3.
	 */
	core_count[NSTS2_IDX] = wlc_stf_best_nsts2_cores(wlc, txpwr, chanspec);

	if (nstreams == 3)
		goto assign_masks;

	/*
	 * Nsts 3: 3Tx >= 4Tx + 1.25dB, then use 3Tx
	 * We should never get here if nstreams < 4.
	 */
	core_count[NSTS3_IDX] = wlc_stf_best_nsts3_cores(wlc, txpwr, chanspec);

assign_masks:
	/* assign the core masks based on the core count and available cores */
	for (idx = 0; idx < MAX_CORE_IDX; idx++)
		txcore[idx] = wlc_stf_core_mask_assign(wlc->stf, core_count[idx]);

	WL_NONE(("++++++++txstreams %d txchains 0x%x\n", wlc->stf->txstreams, wlc->stf->txchain));
	WL_NONE(("++++++++SET CCK   to 0x%x\n", txcore[CCK_IDX]));
	WL_NONE(("++++++++SET OFDM  to 0x%x\n", txcore[OFDM_IDX]));
	WL_NONE(("++++++++SET Nsts1 to 0x%x\n", txcore[NSTS1_IDX]));
	WL_NONE(("++++++++SET Nsts2 to 0x%x\n", txcore[NSTS2_IDX]));
	WL_NONE(("++++++++SET Nsts3 to 0x%x\n", txcore[NSTS3_IDX]));
	if (nstreams == 4)
		WL_NONE(("++++++++SET Nsts4 to 0x%x\n", txcore[NSTS4_IDX]));
}

static uint
wlc_stf_best_dsss_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec)
{
	uint8 nstreams = wlc->stf->txstreams;
	uint8 max_offset = wlc->stf->max_offset;
	uint core_count;
	int min1, min2, min3, min4;
	wl_tx_bw_t bw;
	ppr_dsss_rateset_t dsss1x1_pwrs;
	ppr_dsss_rateset_t dsss1x2_pwrs;
	ppr_dsss_rateset_t dsss1x3_pwrs;
	ppr_dsss_rateset_t dsss1x4_pwrs;

#ifdef WL11AC
	if (CHSPEC_IS160(chanspec)) {
		bw = WL_TX_BW_20IN160;
	} else if (CHSPEC_IS80(chanspec)) {
		bw = WL_TX_BW_20IN80;
	} else
#endif // endif
	if (CHSPEC_IS40(chanspec)) {
		bw = WL_TX_BW_20IN40;
	} else {
		bw = WL_TX_BW_20;
	}
	if (ACREV_IS(wlc->band->phyrev, 47)) {
	/* XXX
	20698 LO comes with 0/180 degree random phase offset
	The random offset would be disputed after channel switch or radio down/up
	Fold back to 1 path to avoid unintentional BF behavior
	*/
		core_count = 1;
		goto exit;
	}
	ASSERT(nstreams >= 2);

	/* For each of the possible tx expansions, grab the tx power offsets.
	 * Replace the symbolic WL_RATE_DISABLED with the max offset so that the
	 * subsequent 'min' operation is correct.
	 */
	ppr_get_dsss(txpwr, bw, WL_TX_CHAINS_1, &dsss1x1_pwrs);
	wlc_stf_int8_vec_replace(dsss1x1_pwrs.pwr, WL_NUM_RATES_CCK,
	                         WL_RATE_DISABLED, max_offset);

	ppr_get_dsss(txpwr, bw, WL_TX_CHAINS_2, &dsss1x2_pwrs);
	wlc_stf_int8_vec_replace(dsss1x2_pwrs.pwr, WL_NUM_RATES_CCK,
	                         WL_RATE_DISABLED, max_offset);

	if (nstreams >= 3) {
		ppr_get_dsss(txpwr, bw, WL_TX_CHAINS_3, &dsss1x3_pwrs);
		wlc_stf_int8_vec_replace(dsss1x3_pwrs.pwr, WL_NUM_RATES_CCK,
		                          WL_RATE_DISABLED, max_offset);

		if (nstreams >= 4) {
			ppr_get_dsss(txpwr, bw, WL_TX_CHAINS_4, &dsss1x4_pwrs);
			wlc_stf_int8_vec_replace(dsss1x4_pwrs.pwr, WL_NUM_RATES_CCK,
			                          WL_RATE_DISABLED, max_offset);
		}
	}

	/* start by assuming 1 core for TX of DSSS (CCK) */
	core_count = 1;

	/* check if 2 cores is better than 1 for CCK Nsts==1 */
	min1 = - wlc_stf_uint8_vec_max((uint8*)dsss1x1_pwrs.pwr, WL_NUM_RATES_CCK);
	min2 = - wlc_stf_uint8_vec_max((uint8*)dsss1x2_pwrs.pwr, WL_NUM_RATES_CCK);

	if ((min2 + 6) >= min1) /* 3 dB = (3 * 2) = 6 hdB */
		core_count = 2;	/* use CDD */

	if (nstreams < 3) {
		WL_NONE(("++++++++SET CCK to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 N/A\n",
			core_count, min1, min2));
		goto exit;
	}

	/* check if 3 cores is better than 1 or 2 for CCK Nsts==1 */
	min3 = - wlc_stf_uint8_vec_max((uint8*)dsss1x3_pwrs.pwr, WL_NUM_RATES_CCK);
	if ((min3 + 10) >= min1 && /* 4.8 dB = (4.8 * 2) = 10 hdB */
		(min3 + 10) >= (min2 + 6))
		core_count = 3;

	if (nstreams < 4) {
		WL_NONE(("++++++++SET CCK to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 (%d)\n",
			core_count, min1, min2, min3));
		goto exit;
	}

	/* check if 4 cores is better than above */
	min4 = - wlc_stf_uint8_vec_max((uint8*)dsss1x4_pwrs.pwr, WL_NUM_RATES_CCK);
	if ((min4 + 12) >= min1 && /* 6dB = (6 * 2) = 12 hdB */
		(min4 + 12) >= (min2 + 6) &&
		(min4 + 12) >= (min3 + 10)) {
			core_count = 4;
	} else {
		WL_REGULATORY(("wl%d: %s:SET CCK to %d cores, pwr 1x1 (%d) 1x2 (%d)"
			" 1x3 (%d) 1x4 (%d), condition %d %d %d \n",
			wlc->pub->unit, __FUNCTION__, core_count, min1, min2, min3, min4,
			(min4 + 12) >= min1, (min4 + 12) >= (min2 + 6),
			(min4 + 12) >= (min3 + 10)));
	}

	WL_NONE(("++++++++SET CCK to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 (%d) 1x4 (%d) \n",
		core_count, min1, min2, min3, min4));

exit:
	return core_count;
}

static uint
wlc_stf_best_ofdm_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec)
{
	uint8 nstreams = wlc->stf->txstreams;
	uint8 max_offset = wlc->stf->max_offset;
	uint core_count;
	int min1, min2, min3, min4;
	wl_tx_bw_t bw;
	ppr_ofdm_rateset_t ofdm1x1_pwrs;
	ppr_ofdm_rateset_t ofdm1x2_pwrs;
	ppr_ofdm_rateset_t ofdm1x3_pwrs;
	ppr_ofdm_rateset_t ofdm1x4_pwrs;

	bw = PPR_CHSPEC_BW(chanspec);

	ASSERT(nstreams >= 2);

	/* For each of the possible tx expansions, grab the tx power offsets
	 * Replace the symbolic WL_RATE_DISABLED with the max offset so that the
	 * subsequent 'min' operation is correct.
	 */
	ppr_get_ofdm(txpwr, bw, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm1x1_pwrs);
	wlc_stf_int8_vec_replace(ofdm1x1_pwrs.pwr, WL_NUM_RATES_OFDM,
	                         WL_RATE_DISABLED, max_offset);

	ppr_get_ofdm(txpwr, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm1x2_pwrs);
	wlc_stf_int8_vec_replace(ofdm1x2_pwrs.pwr, WL_NUM_RATES_OFDM,
	                         WL_RATE_DISABLED, max_offset);

	if (nstreams >= 3) {
		ppr_get_ofdm(txpwr, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm1x3_pwrs);
		wlc_stf_int8_vec_replace(ofdm1x3_pwrs.pwr, WL_NUM_RATES_OFDM,
		                         WL_RATE_DISABLED, max_offset);

		if (nstreams >= 4) {
			ppr_get_ofdm(txpwr, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_4, &ofdm1x4_pwrs);
			wlc_stf_int8_vec_replace(ofdm1x4_pwrs.pwr, WL_NUM_RATES_OFDM,
			                         WL_RATE_DISABLED, max_offset);
		}
	}

	/* start by assuming 1 core for TX of OFDM */
	core_count = 1;

	/* check if 2 cores is better than 1 for Nsts==1 */
	min1 = - wlc_stf_uint8_vec_max((uint8*)ofdm1x1_pwrs.pwr, WL_NUM_RATES_OFDM);
	min2 = - wlc_stf_uint8_vec_max((uint8*)ofdm1x2_pwrs.pwr, WL_NUM_RATES_OFDM);

	if ((min2 + 6) >= min1) /* 3 dB = (3 * 2) = 6 hdB */
		core_count = 2;	/* use CDD */

	if (nstreams < 3) {
		WL_NONE(("++++++++SET OFDM to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 N/A\n",
			core_count, min1, min2));
		goto exit;
	}

	/* check if 3 cores is better than 1 or 2 for Nsts==1 */
	min3 = - wlc_stf_uint8_vec_max((uint8*)ofdm1x3_pwrs.pwr, WL_NUM_RATES_OFDM);
	if ((min3 + 10) >= min1 && /* 4.8 dB = (4.8 * 2) = 10 hdB */
	    (min3 + 10) >= (min2 + 6))
		core_count = 3;

	if (nstreams < 4) {
		WL_NONE(("++++++++SET OFDM to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 (%d)\n",
			core_count, min1, min2, min3));
		goto exit;
	}

	/* check if 4 cores is better than above */
	min4 = - wlc_stf_uint8_vec_max((uint8*)ofdm1x4_pwrs.pwr, WL_NUM_RATES_OFDM);
	if ((min4 + 12) >= min1 && /* 6dB = (6 * 2) = 12 hdB */
		(min4 + 12) >= (min2 + 6) &&
		(min4 + 12) >= (min3 + 10)) {
			core_count = 4;
	} else {
		WL_REGULATORY(("wl%d: %s:SET OFDM to %d cores, pwr 1x1 (%d) 1x2 (%d)"
			" 1x3 (%d) 1x4 (%d), condition %d %d %d \n",
			wlc->pub->unit, __FUNCTION__, core_count, min1, min2, min3, min4,
			(min4 + 12) >= min1, (min4 + 12) >= (min2 + 6),
			(min4 + 12) >= (min3 + 10)));
	}

	WL_NONE(("++++++++SET OFDM to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 (%d) 1x4 (%d) \n",
		core_count, min1, min2, min3, min4));

exit:
	return core_count;
}

static uint
wlc_stf_best_nsts1_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec)
{
	uint8 nstreams = wlc->stf->txstreams;
	uint8 max_offset = wlc->stf->max_offset;
	uint core_count;
	int min1, min2, min3, min4;
	wl_tx_bw_t bw;
	ppr_vht_mcs_rateset_t mcsx1_pwrs;
	ppr_vht_mcs_rateset_t mcsx2_pwrs;
	ppr_vht_mcs_rateset_t mcsx3_pwrs;
	ppr_vht_mcs_rateset_t mcsx4_pwrs;

	bw = PPR_CHSPEC_BW(chanspec);

	ASSERT(nstreams >= 2);

	/* For each of the possible tx expansions, grab the tx power offsets.
	 * Replace the symbolic WL_RATE_DISABLED with the max offset so that the
	 * subsequent 'min' operation is correct.
	 */
	ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcsx1_pwrs);
	wlc_stf_int8_vec_replace(mcsx1_pwrs.pwr, NUM_MCS_RATES,
	                         WL_RATE_DISABLED, max_offset);

	ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcsx2_pwrs);
	wlc_stf_int8_vec_replace(mcsx2_pwrs.pwr, NUM_MCS_RATES,
	                         WL_RATE_DISABLED, max_offset);

	if (nstreams >= 3) {
		ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3, &mcsx3_pwrs);
		wlc_stf_int8_vec_replace(mcsx3_pwrs.pwr, NUM_MCS_RATES,
		                         WL_RATE_DISABLED, max_offset);

		if (nstreams >= 4) {
			ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_4, &mcsx4_pwrs);
			wlc_stf_int8_vec_replace(mcsx4_pwrs.pwr, NUM_MCS_RATES,
			                         WL_RATE_DISABLED, max_offset);
		}
	}

	/* start by assuming 1 core for TX of Nsts1 */
	core_count = 1;

	/* check if 2 cores is better than 1 */
	min1 = - wlc_stf_uint8_vec_max((uint8*)mcsx1_pwrs.pwr, NUM_MCS_RATES);
	min2 = - wlc_stf_uint8_vec_max((uint8*)mcsx2_pwrs.pwr, NUM_MCS_RATES);

	if ((min2 + 6) >= min1) /* 3 dB = (3 * 2) = 6 hdB */
		core_count = 2;

	if (nstreams < 3) {
		WL_NONE(("++++++++SET Nsts1 to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 N/A\n",
		         core_count, min1, min2));
		goto exit;
	}

	/* check if 3 cores is better than 1 or 2 for Nsts==1 */
	min3 = - wlc_stf_uint8_vec_max((uint8*)mcsx3_pwrs.pwr, NUM_MCS_RATES);
	if ((min3 + 10) >= min1 && /* 4.8 dB = (4.8 * 2) = 10 hdB */
	    (min3 + 10) >= (min2 + 6))
		core_count = 3;

	if (nstreams < 4) {
		WL_NONE(("++++++++SET Nsts1 to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 (%d)\n",
			core_count, min1, min2, min3));
		goto exit;
	}

	/* check if 4 cores is better than above */
	min4 = - wlc_stf_uint8_vec_max((uint8*)mcsx4_pwrs.pwr, NUM_MCS_RATES);
	if ((min4 + 12) >= min1 && /* 6dB = (6 * 2) = 12 hdB */
		(min4 + 12) >= (min2 + 6) &&
		(min4 + 12) >= (min3 + 10)) {
			core_count = 4;
	} else {
		WL_REGULATORY(("wl%d: %s:SET Nsts1 to %d cores, pwr 1x1 (%d) 1x2 (%d)"
			" 1x3 (%d) 1x4 (%d), condition %d %d %d \n",
			wlc->pub->unit, __FUNCTION__, core_count, min1, min2, min3, min4,
			(min4 + 12) >= min1, (min4 + 12) >= (min2 + 6),
			(min4 + 12) >= (min3 + 10)));
	}

	WL_NONE(("++++++++SET Nsts1 to %d cores, pwr 1x1 (%d) 1x2 (%d) 1x3 (%d) 1x4 (%d)\n",
	          core_count, min1, min2, min3, min4));

exit:
	return core_count;
}

static uint
wlc_stf_best_nsts2_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec)
{
	uint8 nstreams = wlc->stf->txstreams;
	uint8 max_offset = wlc->stf->max_offset;
	uint core_count;
	int min2, min3, min4;
	wl_tx_bw_t bw;
	ppr_vht_mcs_rateset_t mcsx2_pwrs;
	ppr_vht_mcs_rateset_t mcsx3_pwrs;
	ppr_vht_mcs_rateset_t mcsx4_pwrs;

	bw = PPR_CHSPEC_BW(chanspec);

	ASSERT(nstreams >= 3);

	/* For each of the possible tx expansions, grab the tx power offsets.
	 * Replace the symbolic WL_RATE_DISABLED with the max offset so that the
	 * subsequent 'min' operation is correct.
	 */
	ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcsx2_pwrs);
	wlc_stf_int8_vec_replace(mcsx2_pwrs.pwr, NUM_MCS_RATES,
	                         WL_RATE_DISABLED, max_offset);

	ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcsx3_pwrs);
	wlc_stf_int8_vec_replace(mcsx3_pwrs.pwr, NUM_MCS_RATES,
	                         WL_RATE_DISABLED, max_offset);

	if (nstreams >= 4) {
		ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_4,
			&mcsx4_pwrs);
		wlc_stf_int8_vec_replace(mcsx4_pwrs.pwr, NUM_MCS_RATES,
		                         WL_RATE_DISABLED, max_offset);
	}

	/* start by assuming 2 cores for TX of Nsts2 */
	core_count = 2;

	/* check if 3 cores is better than 2 */
	min2 = - wlc_stf_uint8_vec_max((uint8*)mcsx2_pwrs.pwr, NUM_MCS_RATES);
	min3 = - wlc_stf_uint8_vec_max((uint8*)mcsx3_pwrs.pwr, NUM_MCS_RATES);

	if ((min3 + 4) >= min2)
		core_count = 3;

	if (nstreams < 4) {
		WL_NONE(("++++++++SET Nsts2 to %d cores, pwr 2x2 (%d) 2x3 (%d)\n",
			core_count, min2, min3));
		goto exit;
	}

	/* check if 4 cores is better than above */
	min4 = - wlc_stf_uint8_vec_max((uint8*)mcsx4_pwrs.pwr, NUM_MCS_RATES);
	if ((min4 + 12) >= (min2 + 6) && /* 6dB = (6 * 2) = 12 hdB */
	    (min4 + 12) >= (min3 + 10)) {
		core_count = 4;
	} else {
		WL_REGULATORY(("wl%d: %s:SET Nsts2 to %d cores, pwr 2x2 (%d) 2x3 (%d) 2x4 (%d)"
			", condition %d %d \n",
			wlc->pub->unit, __FUNCTION__, core_count, min2, min3, min4,
			(min4 + 12) >= (min2 + 6), (min4 + 12) >= (min3 + 10)));
	}

	WL_NONE(("++++++++SET Nsts2 to %d cores, pwr 2x2 (%d) 2x3 (%d) 2x4 (%d)\n",
	          core_count, min2, min3, min4));

exit:
	return core_count;
}

static uint
wlc_stf_best_nsts3_cores(wlc_info_t *wlc, ppr_t* txpwr, chanspec_t chanspec)
{
	uint8 max_offset = wlc->stf->max_offset;
	uint core_count;
	int min3, min4;
	wl_tx_bw_t bw;
	ppr_vht_mcs_rateset_t mcsx3_pwrs;
	ppr_vht_mcs_rateset_t mcsx4_pwrs;

	bw = PPR_CHSPEC_BW(chanspec);

	/* For each of the possible tx expansions, grab the tx power offsets.
	 * Replace the symbolic WL_RATE_DISABLED with the max offset so that the
	 * subsequent 'min' operation is correct.
	 */

	ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcsx3_pwrs);
	wlc_stf_int8_vec_replace(mcsx3_pwrs.pwr, NUM_MCS_RATES,
	                         WL_RATE_DISABLED, max_offset);

	ppr_get_vht_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcsx4_pwrs);
	wlc_stf_int8_vec_replace(mcsx4_pwrs.pwr, NUM_MCS_RATES,
	                         WL_RATE_DISABLED, max_offset);

	/* start by assuming 3 cores for TX of Nsts3 */
	core_count = 3;

	/* check if 4 cores is better than 3 */
	min3 = - wlc_stf_uint8_vec_max((uint8*)mcsx3_pwrs.pwr, NUM_MCS_RATES);
	min4 = - wlc_stf_uint8_vec_max((uint8*)mcsx4_pwrs.pwr, NUM_MCS_RATES);

	if ((min4 + 3 ) >= min3) /* 1.25dB = (1.25 * 2) = 2.5 hdB */
		core_count = 4;

	WL_NONE(("++++++++SET Nsts3 to %d cores, pwr 3x3 %d 3x4 %d\n",
	          core_count, min3, min4));

	return core_count;
}

void
wlc_stf_spatialpolicy_set_complete(wlc_info_t *wlc)
{
	uint8 idx, Nsts;
	uint8 core_mask = 0;
	uint8 txcore[MAX_CORE_IDX];

	wlc->stf->spatialpolicy = wlc->stf->spatialpolicy_pending;
	wlc->stf->spatialpolicy_pending = 0;
	if (wlc->stf->spatialpolicy == AUTO_SPATIAL_EXPANSION) {
		/* set txcore based on txpower for Nsts */
		wlc_stf_txcore_select(wlc, txcore);
	}
	else if (wlc->stf->spatialpolicy == MIN_SPATIAL_EXPANSION) {
		/* set txcore to maximum spatial policy, use all
		 * antenna for any Nsts
		 */
		for (idx = 0; idx < MAX_CORE_IDX; idx++)
			txcore[idx] = wlc_stf_core_mask_assign(wlc->stf, txcore_default[idx][0]);
	} else {
		/* set txcore to minimum spatial policy, use less
		 * amount of antenna for each Nsts
		 */
		for (idx = 0; idx < MAX_CORE_IDX; idx++)
			txcore[idx] = wlc->stf->txchain;
	}

	for (idx = 0; idx < MAX_CORE_IDX; idx++) {
		core_mask = (wlc->stf->txcore_override[idx] & ~CCK_TX_DIVERSITY_BIT) ?
		            wlc->stf->txcore_override[idx] : txcore[idx];

		/* txcore_override:bit:7 is not part of core mask, clear it */
		if (idx == CCK_IDX)
			core_mask &= ~CCK_TX_DIVERSITY_BIT;

		Nsts = wlc->stf->txcore[idx][0];
		/* only initial mcs_txcore to max hw supported */
		if (Nsts > wlc->stf->txstreams) {
			WL_NONE(("wl%d: %s: Nsts (%d) > # of streams hw supported (%d)\n",
			         wlc->pub->unit, __FUNCTION__, Nsts, wlc->stf->txstreams));
			core_mask = 0;
		}

		if (WLC_BITSCNT(core_mask) > wlc->stf->txstreams) {
			WL_NONE(("wl%d: %s: core_mask (0x%02x) > # of HW core enabled (0x%x)\n",
			         wlc->pub->unit, __FUNCTION__, core_mask, wlc->stf->hw_txchain));
			core_mask = 0;
		}

		wlc_stf_txcore_set(wlc, idx, core_mask);
	}

	wlc_stf_txcore_shmem_write(wlc, FALSE);

	/* invalidate txcache since rates are changing */
	if (WLC_TXC_ENAB(wlc) && wlc->txc != NULL)
		wlc_txc_inv_all(wlc->txc);
	/* unless requested to delay, need to update 'special' SCB rate entry. */
	if (!wlc->stf->no_rate_init_yet &&
	    (RATELINKMEM_ENAB(wlc->pub) == TRUE) &&
	    (wlc->scbstate != NULL)) {
		wlc_ratelinkmem_update_rate_entry(wlc, WLC_RLM_SPECIAL_RATE_SCB(wlc), NULL, 0);
	}
}

int
wlc_stf_spatial_policy_set(wlc_info_t *wlc, int val)
{
	WL_TRACE(("wl%d: %s: val %d\n", wlc->pub->unit, __FUNCTION__, val));

	if (!WLCISACPHY(wlc->band))
		return BCME_UNSUPPORTED;

	wlc->stf->spatialpolicy_pending = (int8)val;

	/* If packets are enqueued, then wait for it to drain only if switching to fewer chains */
	if (wlc->stf->spatialpolicy_pending != wlc->stf->spatialpolicy) {
		if (TXPKTPENDTOT(wlc)) {
			wlc_block_datafifo(wlc, DATA_BLOCK_SPATIAL, DATA_BLOCK_SPATIAL);
			return BCME_OK;
		}
	}

	wlc_stf_spatialpolicy_set_complete(wlc);

	return BCME_OK;
}

int
wlc_stf_txchain_subval_get(wlc_info_t* wlc, uint id, uint *txchain)
{
	if (id >= WLC_TXCHAIN_ID_COUNT) {
		return BCME_RANGE;
	}

	*txchain = wlc->stf->txchain_subval[id];

	return BCME_OK;
}

/*
 * store a new value for the given txchain_subval and return the
 * a recalculated AND of all the txchain_subval masks with the
 * available hw chains.
 */
static uint8
wlc_stf_txchain_subval_update(wlc_stf_t *stf, uint id, uint8 txchain_subval)
{
	int i;
	uint8 txchain = stf->hw_txchain;

	stf->txchain_subval[id] = txchain_subval;

	for (i = 0; i < WLC_TXCHAIN_ID_COUNT; i++)
		txchain = txchain & stf->txchain_subval[i];

	return txchain;
}

void wlc_stf_chain_active_set(wlc_info_t *wlc, uint8 active_chains)
{
	uint8 txchain; uint8 rxchain;

	txchain = (0xF & active_chains);
	rxchain = (0xF & (active_chains >> 4));

//ToDo: do we need this?
/*
#ifdef WL_STF_ARBITRATOR
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		wlc_stf_mimops_set_txchain_upd_in_progress(wlc, FALSE);
	}
#endif // endif
*/

	/* TX Disabling: */
	/* if chip has two antennas, then proceed with disabling TX */
	/* if bitmap is less than current active chain */
	/* else will recovery mode or switch current TX antenna */

	/* Disabling TX */
	if (wlc->stf->txchain == wlc->stf->hw_txchain) {
		/* current active TX = 2 */
		if ((txchain < wlc->stf->hw_txchain) & (rxchain == wlc->stf->hw_rxchain)) {
			/* 2RX: turn off 1 tx chain based on best RSSI data */
			wlc_stf_txchain_set(wlc, txchain, TRUE, WLC_TXCHAIN_ID_TEMPSENSE);
		} else if ((txchain < wlc->stf->hw_txchain) & (rxchain < wlc->stf->hw_rxchain)) {
			wlc_stf_txchain_set(wlc, rxchain, TRUE, WLC_TXCHAIN_ID_TEMPSENSE);
		}
	} else if (wlc->stf->txchain < wlc->stf->hw_txchain) {
		/* Current active TX = 1 */
		if (txchain == wlc->stf->hw_txchain) {
			/* case txchain=3: turn back on txchain for tempsense recovery */
			wlc_stf_txchain_set(wlc, txchain, TRUE, WLC_TXCHAIN_ID_TEMPSENSE);
		} else if ((txchain != wlc->stf->txchain) & (rxchain == wlc->stf->hw_rxchain)) {
			/* case txchain=0x1 or 0x2: swap active TX chain only if in 2RX mode */
			wlc_stf_txchain_set(wlc, txchain, TRUE, WLC_TXCHAIN_ID_TEMPSENSE);
		}
	}

	/* Disabling RX */
	if (wlc->stf->rxchain == wlc->stf->hw_rxchain) {
			/* Current active RX = 2 */
			if (rxchain < wlc->stf->hw_rxchain) {
				/* turn off 1 rx chain */
				wlc_stf_rxchain_set(wlc, rxchain, TRUE);
			}
	} else if (wlc->stf->rxchain < wlc->stf->hw_rxchain) {
		/* current active RX = 1 */
		if (rxchain == wlc->stf->hw_rxchain) {
			/* Restore RX */
			wlc_stf_rxchain_set(wlc, rxchain, TRUE);
		}
	}
}

#define WLC_ANTMAP_ANTMASK_PERCORE	0xF

int
wlc_stf_mws_set(wlc_info_t *wlc, uint32 txantmap, uint32 rxantmap)
{
	uint8 txchain = wlc->stf->txchain;
#ifdef WL_STF_ARBITRATOR
	uint8 active = WLC_STF_ARBITRATOR_REQ_STATE_RXTX_INACTIVE;
#endif // endif
	/* As per the iovar assumptions, the chain setting only care about tx */
	/* antmap Rx antmap will never change cores */
	if (txantmap == MWS_ANTMAP_DEFAULT) {

		txchain = wlc->stf->hw_txchain;

	} else {
		uint8 core, coremap;
		uint8 antmap;

		/* antmap format: bitmap with 8 cores, 4 antenna each */
		for (core = 0; core < 8; core++) {
			antmap = (txantmap >> (core*4)) & WLC_ANTMAP_ANTMASK_PERCORE;
			coremap = 1 << core;
			if (wlc->stf->hw_txchain & coremap) {
				if (antmap) {
					txchain = txchain | coremap;
				} else {
					txchain = txchain & (~coremap);
				}
			}
		}
#ifdef WL_STF_ARBITRATOR
		/* Make it active for SISO only; This will work for 2 chain HW , need to relook */
		if (WLC_STF_ARB_ENAB(wlc->pub) && WLC_BITSCNT(txchain) == 1)
			active = WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE;
#endif // endif
	}
#ifdef WL_STF_ARBITRATOR
	if (WLC_STF_ARB_ENAB(wlc->pub)) {
		wlc_stf_nss_request_update(wlc, wlc->stf->arb->lte_req, active, txchain,
		WLC_BITSCNT(txchain), wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
		return BCME_OK;
	} else
#endif /* NOT WL_STF_ARBITRATOR */
	{
		return (wlc_stf_txchain_set(wlc, txchain, FALSE, WLC_TXCHAIN_ID_MWS));
	}
}

int
wlc_stf_txchain_set(wlc_info_t *wlc, int32 int_val, bool force, uint16 id)
{
	uint8 txchain_subval = (uint8)int_val;
	uint8 prev_subval;
	uint8 txchain_pending;
	uint current_streams, new_streams;
	enum wlc_bandunit bandunit;

//ToDo: need this??
/*
#ifdef WL_STF_ARBITRATOR
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		wlc_stf_mimops_set_txchain_upd_in_progress(wlc, FALSE);
	}
#endif // endif
 */
	if (wlc->stf->sr13_en_sw_txrxchain_mask) {
		if (txchain_subval == 0xf)
			txchain_subval = txchain_subval & wlc->stf->sw_txchain_mask;

		if (txchain_subval & ~wlc->stf->sw_txchain_mask)
			return BCME_BADARG;
	}

	/* save the previous subval in case we need to back out the change */
	prev_subval = wlc->stf->txchain_subval[id];

	/* store the new subval and calculate the resulting overall txchain */
	txchain_pending = wlc_stf_txchain_subval_update(wlc->stf, id, txchain_subval);

	/* if the overall value does not change, just return OK */
	if (wlc->stf->txchain == txchain_pending)
		return BCME_OK;

	/* make sure the value does not have bits outside the range of chains, and
	 * has at least one chain on
	 */
	if ((txchain_pending & ~wlc->stf->hw_txchain) ||
	    !(txchain_pending & wlc->stf->hw_txchain)) {
		wlc->stf->txchain_subval[id] = prev_subval;
		WL_ERROR(("wl%d %s: Out of range txchain_pending:%d hw_txchain: %d\n",
			WLCWLUNIT(wlc), __FUNCTION__, txchain_pending, wlc->stf->hw_txchain));
		return BCME_RANGE;
	}

	current_streams = WLC_BITSCNT(wlc->stf->txchain);
	new_streams = WLC_BITSCNT(txchain_pending);

	/* if nrate override is configured to be non-SISO STF mode, reject reducing txchain to 1 */
	if (new_streams == 1 && current_streams > 1) {
		FOREACH_WLC_BAND(wlc, bandunit) {
			wlcband_t *band;

			band = wlc->bandstate[bandunit];
			if ((wlc_ratespec_ntx(band->rspec_override) > 1) ||
			    (wlc_ratespec_ntx(band->mrspec_override) > 1)) {
				if (!force) {
					wlc->stf->txchain_subval[id] = prev_subval;
					return BCME_ERROR;
				}

				/* over-write the override rspec */
				if (wlc_ratespec_ntx(band->rspec_override) > 1) {
					band->rspec_override = 0;
					WL_ERROR(("%s(): clearing multi-chain rspec_override "
						"for single chain operation.\n", __FUNCTION__));
				}
				if (wlc_ratespec_ntx(band->mrspec_override) > 1) {
					band->mrspec_override = 0;
					WL_ERROR(("%s(): clearing multi-chain mrspec_override "
						"for single chain operation.\n", __FUNCTION__));
				}
			}
		} /* FOREACH_WLC_BAND */
		if (wlc_stf_stbc_tx_get(wlc) == ON) {
			FOREACH_WLC_BAND(wlc, bandunit) {
				wlc->bandstate[bandunit]->band_stf_stbc_tx = OFF;
			}
		}
	}

	wlc->stf->txchain_pending = txchain_pending;

	/* If packets are enqueued, then wait for it to drain only if switching to fewer chains */
	if ((wlc->stf->txchain & txchain_pending) != wlc->stf->txchain) {
		if (TXPKTPENDTOT(wlc)) {
			wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, DATA_BLOCK_TXCHAIN);
			return BCME_OK;
		}
	}

	wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, 0);
	wlc_stf_txchain_set_complete(wlc);

	return BCME_OK;
}

void
wlc_stf_txchain_set_complete(wlc_info_t *wlc)
{
	uint8 txstreams = (uint8)WLC_BITSCNT(wlc->stf->txchain_pending);
#ifdef WLOLPC
	bool txchain_diff = (wlc->stf->txchain != wlc->stf->txchain_pending);
#endif /* WLOLPC */
	uint8 txcore_idx = 0;
	enum wlc_bandunit bandunit;

	wlc->stf->txchain = wlc->stf->txchain_pending;
//ToDo: need it?
/*
#ifdef WL_STF_ARBITRATOR
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		wlc_stf_mimops_set_txchain_upd_in_progress(wlc, FALSE);
	}
#endif // endif
*/

	/* if we're not turning everything off, ensure CCK and OFDM overrides conform to txchain */
	/* otherwise, remove overrides */
	if (wlc->stf->txchain) {
		for (txcore_idx = CCK_IDX; txcore_idx <= NSTS4_IDX; txcore_idx++) {
			if (wlc->stf->txcore_override[txcore_idx]) {
				if ((wlc->stf->txcore_override[txcore_idx] & wlc->stf->txchain) !=
						wlc->stf->txcore_override[txcore_idx]) {
					WL_NONE(("override removed for idx %d chain=%x val=%x\n",
							txcore_idx,
							wlc->stf->txcore_override[txcore_idx],
							wlc->stf->txchain));
					wlc->stf->txcore_override[txcore_idx] = 0;
					if (wlc->stf->txchain_perrate_state_modify) {
						wlc->stf->txchain_perrate_state_modify(wlc);
					}
				}
			}
		}
	}
	wlc->stf->txchain_pending = 0;
	wlc->stf->txstreams = txstreams;
	wlc->stf->op_txstreams = txstreams;
	wlc_ht_stbc_tx_set(wlc->hti, wlc->band->band_stf_stbc_tx);

	FOREACH_WLC_BAND(wlc, bandunit) {
		wlc_stf_ss_update(wlc, wlc->bandstate[bandunit]);
	}

	if ((wlc->stf->op_txstreams == 1) && (!WLCISACPHY(wlc->band))) {
		if (wlc->stf->txchain == 1) {
			wlc->stf->txant = ANT_TX_FORCE_0;
		} else if (wlc->stf->txchain == 2) {
			wlc->stf->txant = ANT_TX_FORCE_1;
		} else {
			ASSERT(0);
		}
	} else {
		wlc->stf->txant = ANT_TX_DEF;
	}

	/* push the updated txant to phytxant (used for txheader) */
	_wlc_stf_phy_txant_upd(wlc);

	/* avoid doing early rate inits, will do that at end of function */
	/* this to avoid ASSERTs in rate entry filling */
	wlc->stf->no_rate_init_yet = TRUE;

	/* initialize txcore and spatial policy */
	wlc_stf_init_txcore_default(wlc);
	wlc_stf_spatial_mode_set(wlc, wlc->chanspec);

#ifdef WL11AC
	if (VHT_ENAB(wlc->pub)) {
#ifdef WL_STF_ARBITRATOR
		wlc_vht_set_ac_mode(wlc->vhti);
#endif /* WL_STF_ARBITRATOR */
		wlc_vht_update_mcs_cap(wlc->vhti);
	}
#endif /* WL11AC */
#ifdef WL11AX
	if (HE_ENAB(wlc->pub))
		wlc_he_update_mcs_cap(wlc->hei);
#endif /* WL11AX */

	/* invalidate txcache since rates are changing */
	if (WLC_TXC_ENAB(wlc) && wlc->txc != NULL)
		wlc_txc_inv_all(wlc->txc);

	phy_stf_chain_set(WLC_PI(wlc), wlc->stf->txchain, wlc->stf->rxchain);

#if defined(WL_BEAMFORMING)
	if (TXBF_ENAB(wlc->pub)) {
		wlc_txbf_txchain_upd(wlc->txbf);
	}
#endif /* defined(WL_BEAMFORMING) */
#ifdef WL_UCM
	if (wlc->stf->bt_rxchain_pending) {
		wlc_btc_rxchain_update(wlc);
	}
#endif /* WL_UCM */
#ifdef WL_MU_TX
	if (MU_TX_ENAB(wlc)) {
		wlc_mutx_active_update(wlc->mutx);
	}
#endif  /* WL_MU_TX */

#ifdef WLOLPC
	if (OLPC_ENAB(wlc) && txchain_diff) {
		wlc_olpc_eng_hdl_txchain_update(wlc->olpc_info);
	}
#endif /* WLOLPC */

	/* allow rate inits again now that everything is set correctly */
	wlc->stf->no_rate_init_yet = FALSE;
	/* we need to take care of wlc_rate_init for every scb here */
	wlc_scb_ratesel_init_all(wlc);
}

/* Reset the chains back to original values */
static void
wlc_stf_txchain_reset(wlc_info_t *wlc, uint16 id)
{
	/* reset this ID subval to full hw chains */
	wlc_stf_txchain_set(wlc, wlc->stf->hw_txchain, FALSE, id);
}

#ifndef WLTXPWR_CACHE
static
#endif // endif
int
wlc_stf_txchain_pwr_offset_set(wlc_info_t *wlc, wl_txchain_pwr_offsets_t *offsets)
{
	wlc_stf_t *stf = wlc->stf;
	struct phy_txcore_pwr_offsets phy_offsets;
	struct phy_txcore_pwr_offsets prev_offsets;
	int i;
	int err;
	int8 chain_offset;

	memset(&phy_offsets, 0, sizeof(struct phy_txcore_pwr_offsets));

	for (i = 0; i < WL_NUM_TXCHAIN_MAX; i++) {
		chain_offset = offsets->offset[i];
		if (chain_offset > 0) {
			WL_ERROR(("wl%d %s: chain_offset %d Out of range \n", WLCWLUNIT(wlc),
				__FUNCTION__, chain_offset));
			return BCME_RANGE;
		}

		/* If WL_NUM_TXCHAIN_MAX > PHY_MAX_CORES, make sure the input
		 * does not overrun the phy_offsets.offset[] array.
		 */
#if WL_NUM_TXCHAIN_MAX > PHY_MAX_CORES
		if (chain_offset != 0 && i >= PHY_MAX_CORES) {
			WL_ERROR(("wl%d %s: Bad argument chain_offset %d phy core: %d\n",
				WLCWLUNIT(wlc), __FUNCTION__, chain_offset, i));
			return BCME_BADARG;
		}
#endif // endif
		WL_NONE(("wl%d: %s: setting chain %d to chain_offset %d\n",
		         wlc->pub->unit, __FUNCTION__, i, chain_offset));

		phy_offsets.offset[i] = chain_offset;
	}

	/* the call to wlc_phy_txpower_core_offset_set() leads to wlc_update_txppr_offset()
	 * which references the stf->txchain_pwr_offsets values. Store the values
	 * but keep the previous values in case we need to back out the setting on err
	 */

	/* remember the current offsets in case of error */
	memcpy(&prev_offsets, &stf->txchain_pwr_offsets, sizeof(struct phy_txcore_pwr_offsets));
	/* update stf copy to the new offsets */
	memcpy(&stf->txchain_pwr_offsets, offsets, sizeof(struct phy_txcore_pwr_offsets));

	err = wlc_phy_txpower_core_offset_set(WLC_PI(wlc), &phy_offsets);

	/* restore the settings in our state if error */
	if (err)
		memcpy(&stf->txchain_pwr_offsets,
		       &prev_offsets, sizeof(struct phy_txcore_pwr_offsets));

	return err;
}

bool
wlc_stf_rxchain_ishwdef(wlc_info_t* wlc)
{
	return (wlc->stf->rxchain == wlc->stf->hw_rxchain_cap);
}

bool
wlc_stf_txchain_ishwdef(wlc_info_t* wlc)
{
	return (wlc->stf->txchain == wlc->stf->hw_txchain);
}

/* phy radio chains may need re-init after BH */
void
wlc_stf_reinit_chains(wlc_info_t* wlc)
{
	uint8 mimops_mode;
	uint8 new_rxchain_cnt;

	new_rxchain_cnt = (uint8)WLC_BITSCNT(wlc->stf->rxchain);
	mimops_mode = (new_rxchain_cnt == 1) ? HT_CAP_MIMO_PS_ON : HT_CAP_MIMO_PS_OFF;

	phy_stf_chain_set(WLC_PI(wlc), wlc->stf->txchain, wlc->stf->rxchain);

	wlc_ht_mimops_handle_rxchain_update(wlc->hti, mimops_mode);
}

int
wlc_stf_rxchain_set(wlc_info_t* wlc, int32 int_val, bool update)
{
	uint8 rxchain_cnt;
	uint8 rxchain = (uint8)int_val;
	uint8 mimops_mode;
	uint8 old_rxchain, old_rxchain_cnt;
	wlc_phy_t *pi = WLC_PI(wlc);

	if (wlc->stf->sr13_en_sw_txrxchain_mask) {
		if (rxchain == 0xf)
			rxchain = rxchain & wlc->stf->sw_rxchain_mask;

		if (rxchain & ~wlc->stf->sw_rxchain_mask)
			return BCME_BADARG;
	}

	if (wlc->stf->rxchain == rxchain)
		return BCME_OK;

	if ((rxchain & ~wlc->stf->hw_rxchain) || !(rxchain & wlc->stf->hw_rxchain)) {
		WL_ERROR(("wl%d: %s: Failed to set Rx chain,out of range Rx chain: %d "
			"HW_RX chain: %d\n", WLCWLUNIT(wlc), __FUNCTION__,
			rxchain, wlc->stf->hw_rxchain));
		return BCME_RANGE;
	}

	rxchain_cnt = (uint8)WLC_BITSCNT(rxchain);

	old_rxchain = wlc->stf->rxchain;
	old_rxchain_cnt = wlc->stf->rxstreams;

	wlc->stf->rxchain = rxchain;
	wlc->stf->rxstreams = rxchain_cnt;
	wlc->stf->op_rxstreams = rxchain_cnt;

#ifdef WL11AC
	if (update && VHT_ENAB(wlc->pub))
		wlc_vht_update_mcs_cap(wlc->vhti);
#endif /* WL11AC */
#ifdef WL11AX
	if (update && HE_ENAB(wlc->pub))
		wlc_he_update_mcs_cap(wlc->hei);
#endif /* WL11AX */
#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub)) {
		wlc_txbf_rxchain_upd(wlc->txbf);
	}
#endif /* WL_BEAMFORMING */

	/* PR81503: need to iterate over active bsscfgs instead of
	 * just using wlc->cfg for wlc_mimops_action_ht_send() calls
	 */
	/* if changing to/from 1 rxstream, update MIMOPS mode */
	if (wlc->pub->hw_up)
		wlc_suspend_mac_and_wait(wlc);
	if (rxchain_cnt != old_rxchain_cnt &&
	    (rxchain_cnt == 1 || old_rxchain_cnt == 1)) {
		mimops_mode = (rxchain_cnt == 1) ? HT_CAP_MIMO_PS_ON : HT_CAP_MIMO_PS_OFF;
		wlc_ht_mimops_handle_rxchain_update(wlc->hti, mimops_mode);
	}
	else if (old_rxchain != rxchain)
		phy_stf_chain_set(pi, wlc->stf->txchain, wlc->stf->rxchain);

#ifdef WLPM_BCNRX
	/* Do at end after phy */
	if (PM_BCNRX_ENAB(wlc->pub)) {
		if (!wlc_pm_bcnrx_allowed(wlc))
			wlc_pm_bcnrx_set(wlc, FALSE); /* Disable */
		else
			wlc_pm_bcnrx_set(wlc, TRUE); /* Enable */
	}
#endif // endif
	if (wlc->pub->hw_up)
		wlc_enable_mac(wlc);
	return BCME_OK;
}

/* update wlc->stf->ss_opmode which represents the operational stf_ss mode we're using */
void
wlc_stf_ss_update(wlc_info_t *wlc, wlcband_t *band)
{
	uint8 prev_stf_ss;
	uint8 upd_stf_ss;
	uint8 mhf4_bphytx;

	prev_stf_ss = wlc->stf->ss_opmode;

	/* NOTE: opmode can only be SISO or CDD as STBC is decided on a per-packet basis */
	if (WLC_STBC_CAP_PHY(wlc) &&
	    wlc->stf->ss_algosel_auto && (wlc->stf->ss_algo_channel != (uint16)-1)) {
		ASSERT(isset(&wlc->stf->ss_algo_channel, PHY_TXC1_MODE_CDD) ||
		       isset(&wlc->stf->ss_algo_channel, PHY_TXC1_MODE_SISO));
		upd_stf_ss = (wlc->stf->no_cddstbc || (wlc->stf->op_txstreams == 1) ||
			isset(&wlc->stf->ss_algo_channel, PHY_TXC1_MODE_SISO)) ?
			PHY_TXC1_MODE_SISO : PHY_TXC1_MODE_CDD;
	} else {
		if (wlc->band != band)
			return;
		upd_stf_ss = (wlc->stf->no_cddstbc || (wlc->stf->op_txstreams == 1)) ?
			PHY_TXC1_MODE_SISO : band->band_stf_ss_mode;
	}
	if (prev_stf_ss != upd_stf_ss) {
		wlc->stf->ss_opmode = upd_stf_ss;
		wlc_bmac_band_stf_ss_set(wlc->hw, upd_stf_ss);
	}

	/* Support for 11b single antenna. */
	/* If SISO operating mode or boardflags indicate that it is allowed
	 * to transmit bphy frames on only one core then force bphy Tx on a
	 * single core.
	 */
	mhf4_bphytx = ((wlc->stf->siso_tx) ||
	               ((wlc->pub->boardflags2 & BFL2_BPHY_ALL_TXCORES) == 0) ?
	               MHF4_BPHY_2TXCORES : 0);
	wlc_mhf(wlc, MHF4, MHF4_BPHY_2TXCORES, mhf4_bphytx, WLC_BAND_AUTO);

	return;
}

/**
 * Return the count of tx chains to be used for the given ratespsec given
 * the current txcore setting.
 *
 * The spatial expansion vaule in the input ratespec is ignored. Only the rate
 * and STBC expansion from the ratespec is used to calculate the modulation type
 * of legacy CCK, legacy OFDM, or MCS Nsts value (Nss + STBC expansion)
 */
uint8
wlc_stf_txchain_get(wlc_info_t *wlc, ratespec_t rspec)
{
	uint8 txcore;
	uint8 chains;

	txcore = wlc_stf_txcore_get(wlc, rspec);
	chains = (uint8)(WLC_BITSCNT(txcore));
	if (WLC_PHY_AS_80P80(wlc, wlc->chanspec)) {
		if (chains & 1) {
			ASSERT(!"INVALID rspec for BW80p80");
		} else {
			chains = chains >> 1;
		}
	}
	return chains;
}

/**
 *
 * Return the txcore mapping to be used for the given ratespsec
 *
 * The spatial expansion vaule in the input ratespec is ignored. Only the rate
 * and STBC expansion from the ratespec is used to calculate the modulation type
 * of legacy CCK, legacy OFDM, or MCS Nsts value (Nss + STBC expansion)
 */
uint8
wlc_stf_txcore_get(wlc_info_t *wlc, ratespec_t rspec)
{
	uint8 idx;

	if (RSPEC_ISCCK(rspec)) {
		idx = CCK_IDX;
	} else if (RSPEC_ISOFDM(rspec)) {
		idx = OFDM_IDX;
	} else {
		idx = wlc_ratespec_nsts(rspec) + OFDM_IDX;
	}
	WL_NONE(("wl%d: %s: Nss %d  Nsts %d\n", wlc->pub->unit, __FUNCTION__,
	         wlc_ratespec_nss(rspec), wlc_ratespec_nsts(rspec)));
	ASSERT(idx < MAX_CORE_IDX);
	idx = MIN(idx, MAX_CORE_IDX - 1);	/* cap idx to MAX_CORE_IDX - 1 */
	WL_NONE(("wl%d: %s: wlc->stf->txcore[%d] 0x%02x\n",
		wlc->pub->unit, __FUNCTION__, idx, wlc->stf->txcore[idx][1]));
	return wlc->stf->txcore[idx][1];
}

void
wlc_stf_txcore_shmem_write(wlc_info_t *wlc, bool forcewr)
{
	uint16 offset;
	uint16 map;
	uint16 base;
	uint8 idx;
	uint16 max_coremask_blk;

	WL_TRACE(("wl%d: %s:\n", wlc->pub->unit, __FUNCTION__));
	if (!wlc->clk && !forcewr) {
		return;
	}

	if (D11REV_LT(wlc->pub->corerev, 26) || !WLCISACPHY(wlc->band)) {
		WL_INFORM(("wl%d: %s: For now txcore shmem only supported"
			" by AC PHY for corerev >= 26\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (wlc->stf->shmem_base != wlc->pub->m_coremask_blk &&
		wlc->stf->shmem_base != wlc->pub->m_coremask_blk_wowl) {
		ASSERT("BAD shmem base address" && 0);
		return;
	}

	if (D11REV_GE(wlc->pub->corerev, 40))
	/* M_COREMASK_BFM, M_COREMASK_BFM1 are filled up by TxBF */
		max_coremask_blk = OFDM_IDX + 1;
	else
		max_coremask_blk = MAX_COREMASK_BLK;

	base = wlc->stf->shmem_base;
	for (offset = 0, idx = 0; offset < max_coremask_blk; offset++, idx++) {
		map = (wlc->stf->txcore[idx][1] & TXCOREMASK);

		if (idx == CCK_IDX) {
			/*  move "cck tx diversity" bit to shmem bit:15 */
			/* if set ucode will use only one antenna for b rates */
			map |= (wlc->stf->txcore_override[CCK_IDX] & CCK_TX_DIVERSITY_BIT) <<
				SPATIAL_SHIFT;
		}

		WL_NONE(("%s: Write Spatial mapping to SHMEM 0x%04x map 0x%04x\n", __FUNCTION__,
		      (base+(offset * 2)), map));
		wlc_write_shm(wlc, (base+(offset * 2)), map);
	}
	wlc_stf_pproff_shmem_write(wlc);
}

void
wlc_stf_shmem_base_upd(wlc_info_t *wlc, uint16 base)
{
	wlc->stf->shmem_base = base;
}

void
wlc_stf_wowl_upd(wlc_info_t *wlc)
{
	wlc_stf_txcore_shmem_write(wlc, TRUE);
}

void
wlc_stf_wowl_spatial_policy_set(wlc_info_t *wlc, int policy)
{
	wlc_stf_spatial_policy_set(wlc, policy);
}

// GPIO-controlled throttling
static void wlc_stf_pwrthrottle_upd(wlc_info_t *wlc);

static void
wlc_stf_watchdog(void *ctx)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;

	if (!TVPM_ENAB(wlc->pub)) {
		wlc_stf_pwrthrottle_upd(wlc);

		if (!wlc->lpas) {
			wlc_stf_tempsense_upd(wlc);
		}
	}
}
uint8
wlc_stf_txchain_get_cap(wlc_info_t *wlc)
{
	return (uint8)WLC_BITSCNT(wlc->stf->hw_txchain_cap);
}

/*
* pwr saving feature: if set, wlc will TX B rate frames using one radio core
* the core selection is automatic per ucode tx diversity algo
*/
static const char BCMATTACHDATA(rstr_cck_onecore_tx)[] = "cck_onecore_tx";
static const char BCMATTACHDATA(rstr_temps_period)[] = "temps_period";
static const char BCMATTACHDATA(rstr_txchain)[] = "txchain";
static const char BCMATTACHDATA(rstr_rxchain)[] = "rxchain";
static const char BCMATTACHDATA(rstr_sw_txchain)[] = "sw_txchain_mask";
static const char BCMATTACHDATA(rstr_sw_rxchain)[] = "sw_rxchain_mask";
static const char BCMATTACHDATA(rstr_stf)[] = "stf";
static const char BCMATTACHDATA(rstr_cck_tx_coremask)[] = "cck_tx_coremask";
int
BCMATTACHFN(wlc_stf_attach)(wlc_info_t* wlc)
{
	uint temp;
#ifdef OCL
	wlc_stf_ocl_info_t *ocl_info;
#endif // endif
	int err;
	enum wlc_bandunit bandunit;

	/* register module */
	if ((err = wlc_module_register(wlc->pub, stf_iovars, rstr_stf, wlc, wlc_stf_doiovar,
	                        wlc_stf_watchdog, NULL, wlc_stf_down)) != BCME_OK) {
		WL_ERROR(("wl%d: stf wlc_stf_iovar_attach failed err=%d\n", wlc->pub->unit, err));
		return err;
	}

	/* init d11 core dependent rate table offset */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		wlc->stf->shm_rt_txpwroff_pos = M_RT_TXPWROFF_POS(wlc);
	} else {
		wlc->stf->shm_rt_txpwroff_pos = M_REV40_RT_TXPWROFF_POS(wlc);
	}
#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "stf", (dump_fn_t)wlc_dump_stf, (void *)wlc);
	wlc_dump_register(wlc->pub, "ppr", (dump_fn_t)wlc_dump_ppr, (void *)wlc);
#endif // endif
	FOREACH_WLC_BAND(wlc, bandunit) {
		wlcband_t *band = wlc->bandstate[bandunit];
		if (bandunit == BAND_2G_INDEX) {
			band->band_stf_ss_mode = PHY_TXC1_MODE_SISO;
		} else {
			band->band_stf_ss_mode = PHY_TXC1_MODE_CDD;
		}
	}

	if ((WLCISNPHY(wlc->band)) && (wlc_phy_txpower_hw_ctrl_get(WLC_PI(wlc)) != PHY_TPC_HW_ON))
		wlc->bandstate[BAND_2G_INDEX]->band_stf_ss_mode = PHY_TXC1_MODE_CDD;

	FOREACH_WLC_BAND(wlc, bandunit) {
		wlc_stf_ss_update(wlc, wlc->bandstate[bandunit]);
	}

	wlc_stf_stbc_rx_ht_update(wlc, HT_CAP_RX_STBC_NO);

	FOREACH_WLC_BAND(wlc, bandunit) {
		wlc->bandstate[bandunit]->band_stf_stbc_tx = OFF;
	}

	if (WLC_STBC_CAP_PHY(wlc)) {
		wlc->stf->ss_algosel_auto = TRUE;
		wlc->stf->ss_algo_channel = (uint16)-1; /* Init the default value */
#ifdef WL11N_STBC_RX_ENABLED
		wlc_stf_stbc_rx_ht_update(wlc, HT_CAP_RX_STBC_ONE_STREAM);
		if (wlc->stf->op_txstreams > 1) {
			FOREACH_WLC_BAND(wlc, bandunit) {
				wlc->bandstate[bandunit]->band_stf_stbc_tx = AUTO;
			}
			wlc_ht_cap_enable_tx_stbc(wlc->hti);
#ifdef WL11AC
			wlc_vht_set_tx_stbc_cap(wlc->vhti, TRUE);
#endif /* WL11AC */
		}
#endif /* WL11N_STBC_RX_ENABLED */
	}

	if ((wlc->stf->txpwr_ctl = ppr_create(wlc->pub->osh, ppr_get_max_bw())) == NULL) {
		return BCME_NOMEM;
	}
#ifdef WL11AX
	if ((wlc->stf->ru_txpwr_ctl = ppr_ru_create(wlc->pub->osh)) == NULL) {
		return BCME_NOMEM;
	}
#endif /* WL11AX */
#ifdef WL_SARLIMIT
	if ((wlc->stf->txpwr_ctl_qdbm = ppr_create(wlc->osh, ppr_get_max_bw())) == NULL) {
		ppr_delete(wlc->osh, wlc->stf->txpwr_ctl);
		wlc->stf->txpwr_ctl = NULL;
		return BCME_NOMEM;
	}
#endif /* WL_SARLIMIT */
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		wlc->stf->max_offset = 0x3f;	/* cap to 31.5 dBm Max */
	} else {
		wlc->stf->max_offset = 0x1f;	/* cap to 15.5 dBm Max */
	}

	/* default Spatial mode to ON */
	memset(wlc->stf->spatial_mode_config, ON, SPATIAL_MODE_MAX_IDX);
	wlc->stf->shmem_base = wlc->pub->m_coremask_blk;

	wlc->stf->tempsense_period = WLC_TEMPSENSE_PERIOD;
	temp = getintvar(wlc->pub->vars, rstr_temps_period);
	/* valid range is 1-14. ignore 0 and 0xf to work with old srom/nvram */
	if ((temp != 0) && (temp < 0xf)) {
		/* for 4365, valid range is 2-14 (otherwise, it is WLC_TEMPSENSE_PERIOD=10s) */
		if (!((ACREV_IS(wlc->band->phyrev, 32) || ACREV_IS(wlc->band->phyrev, 33)) &&
			(temp == 1)))
			wlc->stf->tempsense_period = temp;
	}

	/* override tx coremask for cck rates */
	temp = getintvar(wlc->pub->vars, rstr_cck_tx_coremask);
	if (temp != 0) {
		wlc->stf->txcore_override[CCK_IDX] = (uint8)temp;
	}

	/* enable tx diversity for cck rates */
	temp = getintvar(wlc->pub->vars, rstr_cck_onecore_tx);
	if (temp != 0) {
		wlc->stf->txcore_override[CCK_IDX] |= CCK_TX_DIVERSITY_BIT;
	}

	wlc->stf->rssi_pwrdn_disable = TRUE;
	wlc->stf->pwr_throttle_mask = wlc->stf->hw_txchain;
	wlc->stf->txchain_perrate_state_modify = NULL;

#ifdef OCL
	/* Allocate ocl info */
	if (!(ocl_info = (wlc_stf_ocl_info_t *) MALLOCZ(wlc->osh, sizeof(wlc_stf_ocl_info_t)))) {
		WL_ERROR(("wl%d: %s: ocl_info: MALLOCZ failed, size = %d\n",
			WLCWLUNIT(wlc), __FUNCTION__, sizeof(wlc_stf_ocl_info_t)));
		return BCME_NOMEM;
	}
	ocl_info->wlc = wlc;
	wlc->stf->ocl_info = ocl_info;
	/* init of ocl_en is controlled by the nvram */
	/* ocl_info->ocl_en = phy_ocl_get_en(WLC_PI(wlc)); */
	ocl_info->ocl_en = FALSE;
	ocl_info->ocl_rssi_threshold = WLC_RSSI_MINVAL_INT8;
	ocl_info->ocl_rssi_delta = OCL_RSSI_DELTA;

	/* register assoc state change notification callback */
	if ((err = wlc_bss_assoc_state_register(wlc,
	                                        (bss_assoc_state_fn_t)wlc_stf_ocl_assoc_state_cb,
	                                        NULL)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register failed err=%d\n",
		         WLCWLUNIT(wlc), __FUNCTION__, err));
		return err;
	}
#endif /* OCL */

#if defined(OCL) || defined(WL_MIMOPS_CFG)
	/* register bcn notify */
	if ((err = wlc_bss_rx_bcn_register(wlc,
	                                   (bss_rx_bcn_notif_fn_t)wlc_stf_rx_bcn_notif,
	                                   wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register failed err=%d\n",
		         WLCWLUNIT(wlc), __FUNCTION__, err));
		return err;
	}
#endif /* OCL || WL_MIMOPS_CFG */

#ifdef WL_STF_ARBITRATOR
	if (WLC_STF_ARB_ENAB(wlc->pub)) {
		if ((err = wlc_stf_arb_mimops_info_attach(wlc)) != BCME_OK) {
			return err;
		}
		if ((wlc->stf->arb = wlc_stf_arb_attach(wlc,
		                     (void *)wlc->stf->arb_mps_info_hndl)) == NULL) {
			return BCME_NOMEM;
		}
	}
#endif /* WL_STF_ARBITRATOR */
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		if ((err = wlc_stf_mimops_attach(wlc,
		                                 (void *)wlc->stf->arb_mps_info_hndl)) != BCME_OK) {
			return err;
		}
	}
#endif /* WL_MIMOPS_CFG */

#ifdef WL_NAP
	wlc->stf->nap_rssi_threshold = NAP_DISABLE_RSSI;
	wlc->stf->nap_rssi_delta = NAP_RSSI_DELTA;
	/* register assoc state change notification callback */
	if (wlc_bss_assoc_state_register(wlc,
			(bss_assoc_state_fn_t)wlc_stf_nap_assoc_state_cb, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register failed\n",
		         WLCWLUNIT(wlc), __FUNCTION__));
		return BCME_NOMEM;
	}
#endif /* WL_NAP */
	wlc->stf->tx_duty_cycle_thermal = NO_DUTY_THROTTLE;

	return BCME_OK;
}

void
BCMATTACHFN(wlc_stf_detach)(wlc_info_t* wlc)
{
	if (wlc->stf->txpwr_ctl != NULL) {
		ppr_set_ch_bw(wlc->stf->txpwr_ctl, ppr_get_max_bw()); /* Restore the correct BW */
		ppr_delete(wlc->osh, wlc->stf->txpwr_ctl);
	}
#ifdef WL11AX
	if (wlc->stf->ru_txpwr_ctl != NULL) {
		ppr_ru_delete(wlc->osh, wlc->stf->ru_txpwr_ctl);
	}
#endif /* WL11AX */
#ifdef WL_SARLIMIT
	if (wlc->stf->txpwr_ctl_qdbm != NULL) {
		/* Restore the correct BW */
		ppr_set_ch_bw(wlc->stf->txpwr_ctl_qdbm, ppr_get_max_bw());
		ppr_delete(wlc->osh, wlc->stf->txpwr_ctl_qdbm);
	}
#endif /* WL_SARLIMIT */
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		wlc_stf_mimops_detach(wlc, (void *)wlc->stf->arb_mps_info_hndl);
	} /* WL_MIMOPS_CFG */
#endif // endif
#ifdef WL_STF_ARBITRATOR
	if (WLC_STF_ARB_ENAB(wlc->pub)) {
		wlc_stf_arb_detach(wlc, (void *)wlc->stf->arb_mps_info_hndl);
		wlc_stf_arb_mimops_info_detach(wlc);
	}
#endif /* WL_STF_ARBITRATOR */
#ifdef WL_NAP
	/* unregister assoc state change notification callback */
	(void)wlc_bss_assoc_state_unregister(wlc,
			(bss_assoc_state_fn_t)wlc_stf_nap_assoc_state_cb, wlc);
#endif /* WL_NAP */
#ifdef OCL
	if (wlc->stf->ocl_info != NULL) {
		MFREE(wlc->osh, wlc->stf->ocl_info, sizeof(wlc_stf_ocl_info_t));
		wlc->stf->ocl_info = NULL;
	}
	/* unregister assoc state change notification callback */
	(void)wlc_bss_assoc_state_unregister(wlc,
	                                     (bss_assoc_state_fn_t)wlc_stf_ocl_assoc_state_cb, wlc);
#endif /* OCL */

#if defined(OCL) || defined(WL_MIMOPS_CFG)
	/* unregister bcn notify */
	wlc_bss_rx_bcn_unregister(wlc, (bss_rx_bcn_notif_fn_t)wlc_stf_rx_bcn_notif, wlc);
#endif /* OCL || WL_MIMOPS_CFG */

	wlc_module_unregister(wlc->pub, rstr_stf, wlc);
}

int
wlc_stf_ant_txant_validate(wlc_info_t *wlc, int8 val)
{
	int bcmerror = BCME_OK;

	/* when there is only 1 tx_streams, don't allow to change the txant */
	if (WLCISNPHY(wlc->band) && (wlc->stf->op_txstreams == 1))
		return ((val == wlc->stf->txant) ? bcmerror : BCME_RANGE);

	switch (val) {
		case -1:
			val = ANT_TX_DEF;
			break;
		case 0:
			val = ANT_TX_FORCE_0;
			break;
		case 1:
			val = ANT_TX_FORCE_1;
			break;
		case 3:
			val = ANT_TX_LAST_RX;
			break;
		default:
			bcmerror = BCME_RANGE;
			break;
	}

	if (bcmerror == BCME_OK)
		wlc->stf->txant = (int8)val;

	return bcmerror;

}

/**
 * Centralized txant update function. call it whenever wlc->stf->txant and/or wlc->stf->txchain
 *  change
 *
 * Antennas are controlled by ucode indirectly, which drives PHY or GPIO to
 *   achieve various tx/rx antenna selection schemes
 *
 * legacy phy, bit 6 and bit 7 means antenna 0 and 1 respectively, bit6+bit7 means auto(last rx)
 * for NREV<3, bit 6 and bit 7 means antenna 0 and 1 respectively, bit6+bit7 means last rx and
 *    do tx-antenna selection for SISO transmissions
 * for NREV=3, bit 6 and bit _8_ means antenna 0 and 1 respectively, bit6+bit7 means last rx and
 *    do tx-antenna selection for SISO transmissions
 * for NREV>=7, bit 6 and bit 7 mean antenna 0 and 1 respectively, bit6+bit7 means both cores active
*/
static void
_wlc_stf_phy_txant_upd(wlc_info_t *wlc)
{
	int8 txant;

	txant = (int8)wlc->stf->txant;
	ASSERT(txant == ANT_TX_FORCE_0 || txant == ANT_TX_FORCE_1 || txant == ANT_TX_LAST_RX);

	if (txant == ANT_TX_FORCE_0) {
		wlc->stf->phytxant = PHY_TXC_ANT_0;
	} else if (txant == ANT_TX_FORCE_1) {
		wlc->stf->phytxant = PHY_TXC_ANT_1;
	} else {
		/* keep this assert to catch out of sync wlc->stf->txcore */
		ASSERT(wlc->stf->txchain > 0);
		wlc->stf->phytxant = wlc->stf->txchain << PHY_TXC_ANT_SHIFT;
	}

	WL_INFORM(("wl%d: _wlc_stf_phy_txant_upd: set core mask 0x%04x\n",
		wlc->pub->unit, wlc->stf->phytxant));
	wlc_bmac_txant_set(wlc->hw, wlc->stf->phytxant);
}

void
wlc_stf_phy_txant_upd(wlc_info_t *wlc)
{
	_wlc_stf_phy_txant_upd(wlc);
}

#ifdef WLRSDB
void
wlc_stf_phy_chain_calc_set(wlc_info_t *wlc)
#else
void
BCMATTACHFN(wlc_stf_phy_chain_calc_set)(wlc_info_t *wlc)
#endif // endif
{
	int i;
#ifdef WLRSDB
	int8 rsdb_curr_mode = WLC_RSDB_CURR_MODE(wlc);
#endif /* WLRSDB */

	wlc->stf->hw_txchain = wlc->stf->hw_txchain_cap;
	wlc->stf->hw_rxchain = wlc->stf->hw_rxchain_cap;

#ifdef WLRSDB
	/* nvram should have the actual value. override it for rsdb mode */
	if ((RSDB_ENAB(wlc->pub) && !(WLC_DUALMAC_RSDB(wlc->cmn))) ||
#ifdef RSDB_MODE_80P80
	/* norsdb-80p80 builds */
	1 ||
#endif /* 80P80 */
	0) {
		if ((rsdb_curr_mode == WLC_RSDB_MODE_80P80) ||
			(rsdb_curr_mode == WLC_RSDB_MODE_RSDB)) {
			wlc->stf->hw_txchain = WLC_RSDB_PER_CORE_TXCHAIN;
			wlc->stf->hw_rxchain = WLC_RSDB_PER_CORE_RXCHAIN;
			/* for 80p80 initialize new cores_per_fs */
			if (rsdb_curr_mode == WLC_RSDB_MODE_80P80) {
				/* for 80p80/160 MHz we need 2 cores / txchain = 3 */
				ASSERT(wlc->stf->hw_txchain_cap == 3);
				if (wlc->stf->hw_txchain_cap != 3) {
					WL_ERROR(("wl%d: %s 80p80mode txchain(%d) MUST be 3\n",
					wlc->pub->unit, __FUNCTION__,
					wlc->stf->hw_txchain_cap));
				}
				wlc->stf->channel_bonding_cores = wlc->stf->hw_txchain_cap;
				WL_INFORM(("wl%d: channel bonding enabled:%d\n",
					wlc->pub->unit,
					wlc->stf->channel_bonding_cores));
			}
		}
		phy_set_phymode((phy_info_t *)WLC_PI(wlc), (wlc_rsdb_mode(wlc)));
	}
#endif /* WLRSDB */

	if (CHIPID(wlc->pub->sih->chip) == BCM4352_CHIP_ID) {
		if (wlc->stf->hw_txchain == 7) {
			WL_ERROR(("wl%d: %s: wrong txchain setting %x for 4352. Correct it to 3\n",
				wlc->pub->unit, __FUNCTION__, wlc->stf->hw_txchain));
			wlc->stf->hw_txchain = 3;
		}

		if (wlc->stf->hw_rxchain == 7) {
			WL_ERROR(("wl%d: %s: wrong rxchain setting %x for 4352. Correct it to 3\n",
				wlc->pub->unit, __FUNCTION__, wlc->stf->hw_rxchain));
			wlc->stf->hw_rxchain = 3;
		}
	}

	/* these parameter are intended to be used for all PHY types */
	if (wlc->stf->hw_txchain == 0 || wlc->stf->hw_txchain == 0xff) {
		if (WLCISACPHY(wlc->band)) {
			if (D11REV_GE(wlc->pub->corerev, 65)) {
				wlc->stf->hw_txchain = TXCHAIN_DEF_AC2PHY;
			} else
				wlc->stf->hw_txchain = TXCHAIN_DEF_ACPHY;
		} else if (WLCISNPHY(wlc->band)) {
			wlc->stf->hw_txchain = TXCHAIN_DEF_NPHY;
		} else {
			wlc->stf->hw_txchain = TXCHAIN_DEF;
		}
	}

	wlc->stf->txchain = wlc->stf->hw_txchain;
	for (i = 0; i < WLC_TXCHAIN_ID_COUNT; i++)
		wlc->stf->txchain_subval[i] = wlc->stf->hw_txchain;

	wlc->stf->txstreams = (uint8)WLC_BITSCNT(wlc->stf->hw_txchain);
	wlc->stf->op_txstreams = wlc->stf->txstreams;

	if (wlc->stf->hw_rxchain == 0 || wlc->stf->hw_rxchain == 0xff) {
		if (WLCISACPHY(wlc->band)) {
			if (D11REV_GE(wlc->pub->corerev, 65)) {
				wlc->stf->hw_rxchain = RXCHAIN_DEF_AC2PHY;
			} else
				wlc->stf->hw_rxchain = RXCHAIN_DEF_ACPHY;
		} else if (WLCISNPHY(wlc->band)) {
			wlc->stf->hw_rxchain = RXCHAIN_DEF_NPHY;
		} else {
			wlc->stf->hw_rxchain = RXCHAIN_DEF;
		}
	}

	wlc->stf->rxchain = wlc->stf->hw_rxchain;
	wlc->stf->op_rxstreams = (uint8)WLC_BITSCNT(wlc->stf->hw_rxchain);
	wlc->stf->rxstreams = wlc->stf->op_rxstreams;

	/* initialize the txcore table */
	wlc_stf_init_txcore_default(wlc);
	/* default spatial policy */
	wlc_stf_spatial_mode_set(wlc, wlc->chanspec);
}

void
BCMATTACHFN(wlc_stf_phy_chain_calc)(wlc_info_t *wlc)
{
	uint8 sw_txchain, sw_rxchain;

	/* get available rx/tx chains */
	wlc->stf->hw_txchain_cap =
		(uint8)getintvar_slicespecific(wlc->hw, wlc->pub->vars, rstr_txchain);
	wlc->stf->hw_rxchain_cap =
		(uint8)getintvar_slicespecific(wlc->hw, wlc->pub->vars, rstr_rxchain);
	wlc->stf->sr13_en_sw_txrxchain_mask =
		((wlc->pub->boardflags4 & BFL4_SROM13_EN_SW_TXRXCHAIN_MASK) != 0);

	wlc->stf->core3_p1c =
		((wlc->pub->boardflags4 & BFL4_SROM13_CORE3_P1C) != 0) ||
		(CHIPID(wlc->pub->sih->chip) == BCM4363_CHIP_ID);

	/* Apply sw_txrxchain mask if present */
	if (wlc->stf->sr13_en_sw_txrxchain_mask) {
		sw_txchain =
			(uint8)getintvar_slicespecific(wlc->hw, wlc->pub->vars, rstr_sw_txchain);
		sw_rxchain =
			(uint8)getintvar_slicespecific(wlc->hw, wlc->pub->vars, rstr_sw_rxchain);

		if (wlc->stf->core3_p1c) {
			/* XXX 3+1 configurations use sw_rxchain_mask=0xF and txchain 0x7.
			 * Reduce sw_rxchain_mask here for proper function as 3x3
			 */
			sw_rxchain &= 7;
			sw_txchain &= 7;
		}

		wlc->stf->hw_txchain_cap &= sw_txchain;
		wlc->stf->hw_rxchain_cap &= sw_rxchain;
	} else {
		sw_txchain = 0;
		sw_rxchain = 0;
	}
	wlc->stf->sw_txchain_mask = sw_txchain;
	wlc->stf->sw_rxchain_mask = sw_rxchain;

	wlc_stf_phy_chain_calc_set(wlc);
}

static uint16
_wlc_stf_phytxchain_sel(wlc_info_t *wlc, ratespec_t rspec)
{
	uint16 phytxant = wlc->stf->phytxant;

	if (WLCISHECAPPHY(wlc->band)) {
		phytxant = wlc_stf_txcore_get(wlc, rspec);
	} else if (WLCISACPHY(wlc->band)) {
		phytxant = wlc_stf_txcore_get(wlc, rspec) << D11AC_PHY_TXC_CORE_SHIFT;
	} else {
		if (RSPEC_TXEXP(rspec) > 0) {
			ASSERT(wlc->stf->op_txstreams > 1);
			phytxant = wlc->stf->txchain << PHY_TXC_ANT_SHIFT;
		} else if (wlc->stf->txant == ANT_TX_DEF)
			phytxant = wlc->stf->txchain << PHY_TXC_ANT_SHIFT;
		phytxant &= PHY_TXC_ANT_MASK;
	}
	return phytxant;
}

uint16
wlc_stf_phytxchain_sel(wlc_info_t *wlc, ratespec_t rspec)
{
	return _wlc_stf_phytxchain_sel(wlc, rspec);
}

void
wlc_stf_d11hdrs_phyctl_get_txant_mask(wlc_info_t *wlc, ratespec_t rspec,
		uint16 *phytxant, uint16 *mask)
{
	*phytxant = wlc->stf->phytxant;
	*mask = PHY_TXC_ANT_MASK;

	/* for non-siso rates or default setting, use the available chains */
	if (WLCISNPHY(wlc->band)|| WLCISACPHY(wlc->band) ||
		WLCISHECAPPHY(wlc->band)) {
		ASSERT(wlc->stf->txchain != 0);
		*phytxant = _wlc_stf_phytxchain_sel(wlc, rspec);
		if (WLCISHECAPPHY(wlc->band)) {
			/* antenna cfg is in upper nibble, coremask in lower nibble
			 * move antenna cfg to next byte
			 */
			*phytxant |= (*phytxant & D11_REV80_PHY_TXC_ANT_CONFIG_MASK) <<
				D11_REV80_PHY_TXC_ANT_CONFIG_SHIFT;
			*mask = D11_REV80_PHY_TXC_ANT_CORE_MASK;
		} else if (WLCISACPHY(wlc->band))
			*mask = D11AC_PHY_TXC_ANT_MASK;
		else
			*mask = PHY_TXC_HTANT_MASK;
	}
}

uint16
wlc_stf_d11hdrs_phyctl_txant(wlc_info_t *wlc, ratespec_t rspec)
{
	uint16 phytxant, mask;

	wlc_stf_d11hdrs_phyctl_get_txant_mask(wlc, rspec, &phytxant, &mask);
	phytxant |= phytxant & mask;

	return phytxant;
}

/**
 * For 80p80phy for 160Mhz,
 * Txcore mask need to be properly set in PHYTXCTL for subband cases.
 * Tx chain = 0xf
 *	0x3 for L80
 *	0xC for U80
 * Tx chain = 0x5
 *	0x1 for L80
 *	0x4 for U80
 * Tx chain = 0xa
 *	0x2 for L80
 *	0x8 for U80
 */
uint16
wlc_stf_d11hdrs_phyctl_txcore_80p80phy(wlc_info_t *wlc, uint16 phyctl)
{
	uint16 txcore;

	if (CHSPEC_CTL_SB(wlc->chanspec) <= WL_CHANSPEC_CTL_SB_LUU) {
		/* clear core2 and core3 mask for lower case */
		txcore = phyctl & ~(0xc << D11AC_PHY_TXC_CORE_SHIFT);
	} else {
		/* clear core0 and core1 mask for upper case */
		txcore = phyctl & ~(0x3 << D11AC_PHY_TXC_CORE_SHIFT);
	}
	return txcore;
}

uint16
wlc_stf_spatial_expansion_get(wlc_info_t *wlc, ratespec_t rspec)
{
	uint16 spatial_map = 0;
	uint Nsts;
	uint8 idx = 0;

	if (!WLCISACPHY(wlc->band))
		ASSERT(0);

	Nsts = wlc_ratespec_nsts(rspec);
	ASSERT(Nsts <= wlc->stf->op_txstreams);

	if (RSPEC_ISCCK(rspec))
		idx = CCK_IDX;
	else if (RSPEC_ISOFDM(rspec))
		idx = OFDM_IDX;
	else
		idx = (uint8)(Nsts + OFDM_IDX);
	ASSERT(idx < MAX_CORE_IDX);
	idx = MIN(idx, MAX_CORE_IDX - 1);	/* cap idx to MAX_CORE_IDX - 1 */
	spatial_map = wlc_stf_spatial_map(wlc, idx);
	return spatial_map;
}

#ifdef BCMDBG
#define WLC_PPR_STR(a, b) a = b
#else
#define WLC_PPR_STR(a, b)
#endif // endif
uint8 /* C_CHECK */
wlc_stf_get_pwrperrate(wlc_info_t *wlc, ratespec_t rspec, uint16 spatial_map)
{
	uint8 rate;
	uint8 *offset = NULL;
	uint Nsts = wlc_ratespec_nsts(rspec);
	bool is40MHz = RSPEC_IS40MHZ(rspec);
#ifdef WL11AC
	bool is80MHz = RSPEC_IS80MHZ(rspec);
#endif // endif
#ifdef BCMDBG
	const char *str = "";
#endif // endif
	ppr_dsss_rateset_t dsss_pwrs;
	ppr_ofdm_rateset_t ofdm_pwrs;
	ppr_vht_mcs_rateset_t mcs_pwrs;
	ppr_he_mcs_rateset_t he_mcs_pwrs;
	ppr_t *txpwr = wlc->stf->txpwr_ctl;

	BCM_REFERENCE(he_mcs_pwrs);

	if (wlc->stf->txpwr_ctl == NULL) {
		return 0;
	}

	if (RSPEC_ISHE(rspec)) {
		rate = (rspec & WL_RSPEC_HE_MCS_MASK);
	} else if (RSPEC_ISVHT(rspec)) {
		rate = (rspec & WL_RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		rate = (rspec & WLC_RSPEC_HT_MCS_MASK); /* for HT MCS, convert to a 0-7 index */
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
			if ((rspec & WLC_RSPEC_HT_PROP_MCS_MASK) >= WLC_11N_FIRST_PROP_MCS)
				rate = PROP11N_2_VHT_MCS(rspec);
		}
	} else {
		rate = (uint8)RSPEC2RATE(rspec);
	}

	if (RSPEC_ISHE(rspec)) {
		offset = (uint8*)he_mcs_pwrs.pwr;
		if (RSPEC_ISSTBC(rspec)) {
			WL_ERROR(("INVALID! STBC mode in HE rate\n"));
			ASSERT(!"INVALID! STBC mode in HE rate");
#if defined(WL_BEAMFORMING)
		} else if (TXBF_ENAB(wlc->pub) && RSPEC_ISTXBF(rspec) &&
			PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			wl_tx_nss_t nss = wlc_ratespec_nss(rspec);
			wl_tx_chains_t chains = wlc_stf_txchain_get(wlc, rspec);
			if (is80MHz) {
				ppr_get_he_mcs(txpwr, WL_TX_BW_80, nss, WL_TX_MODE_TXBF, chains,
					&he_mcs_pwrs, WL_HE_RT_SU);
			} else if (is40MHz) {
				ppr_get_he_mcs(txpwr, WL_TX_BW_40, nss, WL_TX_MODE_TXBF, chains,
					&he_mcs_pwrs, WL_HE_RT_SU);
			} else {
				ppr_get_he_mcs(txpwr, WL_TX_BW_20, nss, WL_TX_MODE_TXBF, chains,
					&he_mcs_pwrs, WL_HE_RT_SU);
			}
#endif /* defined(WL_BEAMFORMING) */
		} else {
			switch (Nsts) {
			case 1:
				if (is80MHz) {
					if (spatial_map == 1) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 1: b80_1x2cdd_vht");
					} else if (spatial_map == 2) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 2: b80_1x3cdd_vht");
					} else if (spatial_map == 3) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b80_1x4cdd_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_NONE, WL_TX_CHAINS_1,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 1: b80_1x1vht");
					}
				} else if (is40MHz) {
					if (spatial_map == 1) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 1: b40_1x2cdd_vht");
					} else if (spatial_map == 2) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 2: b40_1x3cdd_vht");
					} else if (spatial_map == 3) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b40_1x4cdd_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_NONE, WL_TX_CHAINS_1,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 1: b40_1x1vht");
					}
				} else {
					if (spatial_map == 1) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 1: b20_1x2cdd_vht");
					} else if (spatial_map == 2) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 2: b20_1x3cdd_vht");
					} else if (spatial_map == 3) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b20_1x4cdd_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_NONE, WL_TX_CHAINS_1,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 1: b20_1x1vht");
					}
				}
				break;
			case 2:
				if (is80MHz) {
					if (spatial_map == 4) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 4: b80_2x4sdm_vht");
					} else if (spatial_map == 3) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b80_2x3sdm_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 2: b80_2x2sdm_vht");
					}
				} else if (is40MHz) {
					if (spatial_map == 4) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 4: b40_2x4sdm_vht");
					} else if (spatial_map == 3) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b40_2x3sdm_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 2: b40_2x2sdm_vht");
					}
				} else {
					if (spatial_map == 4) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 4: b20_2x4sdm_vht");
					} else if (spatial_map == 3) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b20_2x3sdm_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 2: b20_2x2sdm_vht");
					}
				}
				break;
			case 3:
				if (is80MHz) {
					if (spatial_map == 4) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b80_3x4sdm_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 3: b80_3x3sdm_vht");
					}
				} else if (is40MHz) {
					if (spatial_map == 4) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 3: b40_3x4sdm_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 3: b40_3x3sdm_vht");
					}
				} else {
					if (spatial_map == 4) {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "spatial_map 4: b20_3x4sdm_vht");
					} else {
						ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3,
							&he_mcs_pwrs, WL_HE_RT_SU);
						WLC_PPR_STR(str, "Nsts 3: b20_3x3sdm_vht");
					}
				}
				break;
			case 4:
				if (is80MHz) {
					ppr_get_he_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &he_mcs_pwrs,
						WL_HE_RT_SU);
					WLC_PPR_STR(str, "Nsts 4: b80_4x4sdm_vht");
				} else if (is40MHz) {
					ppr_get_he_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &he_mcs_pwrs,
						WL_HE_RT_SU);
					WLC_PPR_STR(str, "Nsts 4: b40_4x4sdm_vht");
				} else {
					ppr_get_he_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &he_mcs_pwrs,
						WL_HE_RT_SU);
					WLC_PPR_STR(str, "Nsts 4: b20_4x4sdm_vht");
				}
				break;
			default:
				ASSERT(0);
				break;
			}
		}
	} else if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
		offset = (uint8*)mcs_pwrs.pwr;
		if (RSPEC_ISSTBC(rspec)) {
#ifdef WL11AC
			if (is80MHz) {
				if (spatial_map == 4) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_pwrs);
					WLC_PPR_STR(str, "spatial_map 4: b80_2x4stbc_vht");
				} else if (spatial_map == 3) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_pwrs);
					WLC_PPR_STR(str, "spatial_map 3: b80_2x3stbc_vht");
				} else {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					WLC_PPR_STR(str, "STBC 2: b80_2x2stbc_vht");
				}
			} else
#endif /* WL11AC */
			if (is40MHz) {
				if (spatial_map == 4) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_pwrs);
					WLC_PPR_STR(str, "spatial_map 4: b40_2x4stbc_vht");
				} else if (spatial_map == 3) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_pwrs);
					WLC_PPR_STR(str, "spatial_map 3: b40_2x3stbc_vht");
				} else {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					WLC_PPR_STR(str, "STBC 2: b40_2x2stbc_vht");
				}
			} else {
				if (spatial_map == 4) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_pwrs);
					WLC_PPR_STR(str, "spatial_map 4: b20_2x4stbc_vht");
				} else if (spatial_map == 3) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_pwrs);
					WLC_PPR_STR(str, "spatial_map 3: b20_2x3stbc_vht");
				} else {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_2, &mcs_pwrs);
					WLC_PPR_STR(str, "STBC 2: b20_2x2stbc_vht");
				}
			}
#if defined(WL_BEAMFORMING)
		} else if (TXBF_ENAB(wlc->pub) && RSPEC_ISTXBF(rspec) &&
			PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			wl_tx_nss_t nss = wlc_ratespec_nss(rspec);
			wl_tx_chains_t chains = wlc_stf_txchain_get(wlc, rspec);
#ifdef WL11AC
			if (is80MHz) {
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80, nss, WL_TX_MODE_TXBF, chains,
					&mcs_pwrs);
			} else
#endif /* WL11AC */
			if (is40MHz) {
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40, nss, WL_TX_MODE_TXBF, chains,
					&mcs_pwrs);
			} else {
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20, nss, WL_TX_MODE_TXBF, chains,
					&mcs_pwrs);
			}
#endif /* defined(WL_BEAMFORMING) */
		} else {
			switch (Nsts) {
			case 1:
#ifdef WL11AC
				if (is80MHz) {
					if (spatial_map == 1) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 1: b80_1x2cdd_vht");
					} else if (spatial_map == 2) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 2: b80_1x3cdd_vht");
					} else if (spatial_map == 3) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b80_1x4cdd_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
							WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 1: b80_1x1vht");
					}
				} else
#endif /* WL11AC */
				if (is40MHz) {
					if (spatial_map == 1) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 1: b40_1x2cdd_vht");
					} else if (spatial_map == 2) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 2: b40_1x3cdd_vht");
					} else if (spatial_map == 3) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b40_1x4cdd_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
							WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 1: b40_1x1vht");
					}
				} else {
					if (spatial_map == 1) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_2, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 1: b20_1x2cdd_vht");
					} else if (spatial_map == 2) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 2: b20_1x3cdd_vht");
					} else if (spatial_map == 3) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b20_1x4cdd_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
							WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 1: b20_1x1vht");
					}
				}
				break;
			case 2:
#ifdef WL11AC
				if (is80MHz) {
					if (spatial_map == 4) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 4: b80_2x4sdm_vht");
					} else if (spatial_map == 3) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b80_2x3sdm_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 2: b80_2x2sdm_vht");
					}
				} else
#endif /* WL11AC */
				if (is40MHz) {
					if (spatial_map == 4) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 4: b40_2x4sdm_vht");
					} else if (spatial_map == 3) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b40_2x3sdm_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 2: b40_2x2sdm_vht");
					}
				} else {
					if (spatial_map == 4) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 4: b20_2x4sdm_vht");
					} else if (spatial_map == 3) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b20_2x3sdm_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
							WL_TX_MODE_NONE, WL_TX_CHAINS_2, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 2: b20_2x2sdm_vht");
					}
				}
				break;
			case 3:
#ifdef WL11AC
				if (is80MHz) {
					if (spatial_map == 4) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b80_3x4sdm_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 3: b80_3x3sdm_vht");
					}
				} else
#endif // endif
				if (is40MHz) {
					if (spatial_map == 4) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 3: b40_3x4sdm_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 3: b40_3x3sdm_vht");
					}
				} else {
					if (spatial_map == 4) {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
						WLC_PPR_STR(str, "spatial_map 4: b20_3x4sdm_vht");
					} else {
						ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3,
							WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
						WLC_PPR_STR(str, "Nsts 3: b20_3x3sdm_vht");
					}
				}
				break;
			case 4:
#ifdef WL11AC
				if (is80MHz) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					WLC_PPR_STR(str, "Nsts 4: b80_4x4sdm_vht");
				} else
#endif // endif
				if (is40MHz) {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_pwrs);
					WLC_PPR_STR(str, "Nsts 4: b40_4x4sdm_vht");
				} else {
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_pwrs);
					WLC_PPR_STR(str, "Nsts 4: b20_4x4sdm_vht");
				}
				break;
			default:
				ASSERT(0);
				break;
			}
		}
	} else if (RSPEC_ISOFDM(rspec)) {
		offset = (uint8*)ofdm_pwrs.pwr;
#if defined(WL_BEAMFORMING)
		if (TXBF_ENAB(wlc->pub) && RSPEC_ISTXBF(rspec) &&
			PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			wl_tx_chains_t chains = wlc_stf_txchain_get(wlc, rspec);
#ifdef WL11AC
			if (is80MHz) {
				ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_TXBF, chains,
					&ofdm_pwrs);
			} else
#endif /* WL11AC */
			if (is40MHz) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_TXBF, chains,
					&ofdm_pwrs);
			} else {
				ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_TXBF, chains,
					&ofdm_pwrs);
			}
		} else
#endif /* defined(WL_BEAMFORMING) */

#ifdef WL11AC
		if (is80MHz) {
			if (spatial_map == 1) {
				ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 1: ofdm_80_1x2cdd");
			} else if (spatial_map == 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 2: ofdm_80_1x3cdd");
			} else if (spatial_map == 3) {
				ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_4,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 3: ofdm_80_1x4cdd");
			} else {
				ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "OFDM: ofdm_80_1x1");
			}
		} else
#endif /* WL11AC */
		if (is40MHz) {
			if (spatial_map == 1) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 1: ofdm_40_1x2cdd");
			} else if (spatial_map == 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 2: ofdm_40_1x3cdd");
			} else if (spatial_map == 3) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_4,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 3: ofdm_40_1x4cdd");
			} else {
				ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "OFDM: ofdm_40_1x1");
			}
		} else {
			if (spatial_map == 1) {
				ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 1: ofdm_20_1x2cdd");
			} else if (spatial_map == 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 2: ofdm_20_1x3cdd");
			} else if (spatial_map == 3) {
				ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_4,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "spatial_map 3: ofdm_20_1x4cdd");
			} else {
				ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
					&ofdm_pwrs);
				WLC_PPR_STR(str, "OFDM: ofdm_20_1x1");
			}
		}
		rate = ofdm_pwr_idx_table[rate/6];
		ASSERT(rate != 0x80);
	} else if (RSPEC_ISCCK(rspec)) {
		offset = (uint8*)dsss_pwrs.pwr;
#ifdef WL11AC
		if (is80MHz) {
			if (spatial_map == 1) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_2,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 1: cck_cdd s1x2");
			} else if (spatial_map == 2) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_3,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 2: cck_cdd s1x3");
			} else if (spatial_map == 3) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_4,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 3: cck_cdd s1x4");
			} else {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN80, WL_TX_CHAINS_1,
					&dsss_pwrs);
				WLC_PPR_STR(str, "CCK: cck");
			}
		} else
#endif /* WL11AC */
		if (is40MHz) {
			if (spatial_map == 1) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_2,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 1: cck_cdd s1x2");
			} else if (spatial_map == 2) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_3,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 2: cck_cdd s1x3");
			} else if (spatial_map == 3) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_4,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 3: cck_cdd s1x4");
			} else {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20IN40, WL_TX_CHAINS_1,
					&dsss_pwrs);
				WLC_PPR_STR(str, "CCK: cck");
			}
		} else {
			if (spatial_map == 1) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_2,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 1: cck_cdd s1x2");
			} else if (spatial_map == 2) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_3,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 2: cck_cdd s1x3");
			} else if (spatial_map == 3) {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_4,
					&dsss_pwrs);
				WLC_PPR_STR(str, "spatial_map 3: cck_cdd s1x4");
			} else {
				ppr_get_dsss(wlc->stf->txpwr_ctl, WL_TX_BW_20, WL_TX_CHAINS_1,
					&dsss_pwrs);
				WLC_PPR_STR(str, "CCK: cck");
			}
		}
		rate = cck_pwr_idx_table[rate >> 1];
		ASSERT(rate != 0x80);
	} else {
		WL_ERROR(("INVALID rspec %x\n", rspec));
		ASSERT(!"INVALID rspec");
	}

	if (offset == NULL || rate == 0x80) {
		ASSERT(0);
		return 0;
	}
#ifdef BCMDBG
#define DIV_QUO(num, div) ((num)/div)  /* Return the quotient of division to avoid floats */
#define DIV_REM(num, div) (((num%div) * 100)/div) /* Return the remainder of division */
	WL_NONE(("wl%d: %s: %s[%d] %2d.%-2d\n", wlc->pub->unit, __FUNCTION__,
		str, rate, DIV_QUO(offset[rate], 2), DIV_REM(offset[rate], 2)));
#endif // endif
	/* return the ppr offset in 0.5dB */
	if (offset[rate] == (uint8)(WL_RATE_DISABLED))
		return wlc->stf->max_offset;
	else
		return (offset[rate]);
}

int
wlc_stf_get_204080_pwrs(wlc_info_t *wlc, ratespec_t rspec, txpwr204080_t* pwrs,
	wl_tx_chains_t txbf_chains)
{
	uint8 rate;
	bool is160MHz = RSPEC_IS160MHZ(rspec);
	bool is80MHz = RSPEC_IS80MHZ(rspec) || is160MHz;
	bool is40MHz = RSPEC_IS40MHZ(rspec) || is80MHz;
	ppr_dsss_rateset_t dsss_pwrs;
	ppr_ofdm_rateset_t ofdm_pwrs;
	ppr_vht_mcs_rateset_t mcs_pwrs;
	ppr_he_mcs_rateset_t he_mcs_pwrs;
	ppr_t *txpwr = wlc->stf->txpwr_ctl;
	wl_tx_mode_t mode;
	wl_tx_nss_t nss = wlc_ratespec_nss(rspec);
	wl_tx_chains_t chains = wlc_stf_txchain_get(wlc, rspec);
	uint i, j, bw_idx;

	BCM_REFERENCE(he_mcs_pwrs);

	if (txpwr == NULL) {
		return BCME_ERROR;
	}

	memset(pwrs, WL_RATE_DISABLED, sizeof(*pwrs));
	mode = WL_TX_MODE_NONE;

	if (RSPEC_ISSTBC(rspec)) {
		mode = WL_TX_MODE_STBC;
		/* STBC expansion doubles the Nss */
		nss = nss*2;
		ASSERT((uint8)nss <= (uint8)chains);
	} else if ((int)chains > (int)nss && (nss == 1)) {
		mode = WL_TX_MODE_CDD;
	}

	if (nss > wlc->stf->op_txstreams) {
		return BCME_ERROR;
	}

	if (RSPEC_ISHE(rspec)) {
		rate = (rspec & WL_RSPEC_HE_MCS_MASK);
	} else if (RSPEC_ISVHT(rspec)) {
		rate = (rspec & WL_RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		rate = (rspec & WLC_RSPEC_HT_MCS_MASK); /* for HT MCS, convert to a 0-7 index */
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
			if ((rspec & WLC_RSPEC_HT_PROP_MCS_MASK) >= WLC_11N_FIRST_PROP_MCS)
				rate = PROP11N_2_VHT_MCS(rspec);
		}
	} else {
		rate = (uint8)RSPEC2RATE(rspec);
	}

	if (is160MHz) {
		bw_idx = BW160_IDX;
	} else if (is80MHz) {
		bw_idx = BW80_IDX;
	} else if (is40MHz) {
		bw_idx = BW40_IDX;
	} else {
		bw_idx = BW20_IDX;
	}

	if (RSPEC_ISHE(rspec)) {
		for (i = 0; i <= bw_idx; i++) {
			if (txbf_chains > 1 && txbf_chains >= (int)nss) {
				/* Fill up txpwr for txbf ON */
				ppr_get_he_mcs(txpwr, mcs_pwr_bw_table[i], nss, WL_TX_MODE_TXBF,
					txbf_chains, &he_mcs_pwrs, WL_HE_RT_SU);
				pwrs->pbw[i][TXBF_ON_IDX] = he_mcs_pwrs.pwr[rate];
			}
			/* Fill up txpwr for txbf OFF */
			ppr_get_he_mcs(txpwr, mcs_pwr_bw_table[i], nss, mode,
				chains, &he_mcs_pwrs, WL_HE_RT_SU);
			pwrs->pbw[i][TXBF_OFF_IDX] = he_mcs_pwrs.pwr[rate];
		}
	} else if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
		for (i = 0; i <= bw_idx; i++) {
			if (txbf_chains > 1 && txbf_chains >= (int)nss) {
				/* Fill up txpwr for txbf ON */
				ppr_get_vht_mcs(txpwr, mcs_pwr_bw_table[i], nss, WL_TX_MODE_TXBF,
					txbf_chains, &mcs_pwrs);
				pwrs->pbw[i][TXBF_ON_IDX] = mcs_pwrs.pwr[rate];
			}
			/* Fill up txpwr for txbf OFF */
			ppr_get_vht_mcs(txpwr, mcs_pwr_bw_table[i], nss, mode,
				chains, &mcs_pwrs);
			pwrs->pbw[i][TXBF_OFF_IDX] = mcs_pwrs.pwr[rate];
		}
	} else if (RSPEC_ISOFDM(rspec)) {
		rate = ofdm_pwr_idx_table[rate/6];
		ASSERT(rate != 0x80);
		for (i = 0; i <= bw_idx; i++) {
			if (txbf_chains > 1) {
				/* Fill up txpwr for txbf ON */
				ppr_get_ofdm(txpwr, ofdm_pwr_bw_table[i], WL_TX_MODE_TXBF,
					txbf_chains, &ofdm_pwrs);
				pwrs->pbw[i][TXBF_ON_IDX] = ofdm_pwrs.pwr[rate];
			}
			/* Fill up txpwr for txbf OFF */
			ppr_get_ofdm(txpwr, ofdm_pwr_bw_table[i], mode, chains, &ofdm_pwrs);
			pwrs->pbw[i][TXBF_OFF_IDX] = ofdm_pwrs.pwr[rate];
		}
	} else if (RSPEC_ISCCK(rspec)) {
		rate = cck_pwr_idx_table[rate >> 1];
		ASSERT(rate != 0x80);
		for (i = 0; i <= bw_idx; i++) {
			ppr_get_dsss(txpwr, cck_pwr_bw_table[i], chains, &dsss_pwrs);
			pwrs->pbw[i][TXBF_OFF_IDX] = dsss_pwrs.pwr[rate];
		}
	} else {
		WL_ERROR(("INVALID rspec %x\n", rspec));
		ASSERT(!"INVALID rspec");
		return BCME_BADARG;
	}

	for (i = 0; i <= BW160_IDX; i++) {
		for (j = 0; j <= TXBF_ON_IDX; j++) {
			if (pwrs->pbw[i][j] == (uint8)(WL_RATE_DISABLED)) {
				pwrs->pbw[i][j] = wlc->stf->max_offset;
			}
		}
	}

	return BCME_OK;
}

int
wlc_stf_get_204080_sdm_pwrs(wlc_info_t *wlc, ratespec_t rspec, txpwr204080_t* pwrs)
{
	uint8 rate;
	bool is160MHz = RSPEC_IS160MHZ(rspec);
	bool is80MHz = RSPEC_IS80MHZ(rspec) || is160MHz;
	bool is40MHz = RSPEC_IS40MHZ(rspec) || is80MHz;
	ppr_vht_mcs_rateset_t mcs_pwrs;
	ppr_he_mcs_rateset_t he_mcs_pwrs;
	ppr_t *txpwr = wlc->stf->txpwr_ctl;
	wl_tx_mode_t mode;
	wl_tx_nss_t nss = wlc_ratespec_nss(rspec);
	uint i, bw_idx;

	BCM_REFERENCE(he_mcs_pwrs);

	if (txpwr == NULL) {
		return BCME_ERROR;
	}

	memset(pwrs, WL_RATE_DISABLED, sizeof(*pwrs));
	mode = WL_TX_MODE_NONE;

	if (RSPEC_ISSTBC(rspec)) {
		return BCME_OK;
	}

	if (nss > wlc->stf->op_txstreams) {
		return BCME_ERROR;
	}

	if (RSPEC_ISHE(rspec)) {
		rate = (rspec & WL_RSPEC_HE_MCS_MASK);
	} else if (RSPEC_ISVHT(rspec)) {
		rate = (rspec & WL_RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		rate = (rspec & WLC_RSPEC_HT_MCS_MASK); /* for HT MCS, convert to a 0-7 index */
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
			if ((rspec & WLC_RSPEC_HT_PROP_MCS_MASK) >= WLC_11N_FIRST_PROP_MCS)
				rate = PROP11N_2_VHT_MCS(rspec);
		}
	} else {
		rate = (uint8)RSPEC2RATE(rspec);
	}

	if (is160MHz) {
		bw_idx = BW160_IDX;
	} else if (is80MHz) {
		bw_idx = BW80_IDX;
	} else if (is40MHz) {
		bw_idx = BW40_IDX;
	} else {
		bw_idx = BW20_IDX;
	}

	if (RSPEC_ISHE(rspec)) {
		for (i = 0; i <= bw_idx; i++) {
			/* Fill up txpwr for txbf OFF */
			ppr_get_he_mcs(txpwr, mcs_pwr_bw_table[i], nss, mode,
				nss, &he_mcs_pwrs, WL_HE_RT_SU);
			pwrs->pbw[i][TXBF_OFF_IDX] = he_mcs_pwrs.pwr[rate];
		}
	} else if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
		for (i = 0; i <= bw_idx; i++) {
			/* Fill up txpwr for txbf OFF */
			ppr_get_vht_mcs(txpwr, mcs_pwr_bw_table[i], nss, mode,
				nss, &mcs_pwrs);
			pwrs->pbw[i][TXBF_OFF_IDX] = mcs_pwrs.pwr[rate];
		}
	} else {
		return BCME_OK;
	}

	for (i = 0; i <= BW160_IDX; i++) {
		if (pwrs->pbw[i][0] == (uint8)(WL_RATE_DISABLED)) {
			pwrs->pbw[i][0] = wlc->stf->max_offset;
		}
	}

	return BCME_OK;
}

#ifdef BCMDBG
static int
wlc_stf_pproff_shmem_get(wlc_info_t *wlc, int *retval)
{
	uint16 entry_ptr;
	uint8 rate, idx;
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs;

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: No clock\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOCLK;
	}

	rs_dflt = &cck_ofdm_rates;
	wlc_rateset_copy(rs_dflt, &rs);

	if (rs.count > 12) {
		WL_ERROR(("wl%d: %s: rate count %d\n", wlc->pub->unit, __FUNCTION__, rs.count));
		return BCME_BADARG;
	}

	for (idx = 0; idx < rs.count; idx++) {
		rate = rs.rates[idx] & RATE_MASK;
		entry_ptr = wlc_rate_shm_offset(wlc, rate);
		retval[idx] = (int)wlc_read_shm(wlc, (entry_ptr + wlc->stf->shm_rt_txpwroff_pos));
		if (RATE_ISOFDM(rate))
			retval[idx] |= 0x80;
	}
	return BCME_OK;
}

static void
wlc_stf_pproff_shmem_set(wlc_info_t *wlc, uint8 rate, uint8 val)
{
	uint16 entry_ptr;

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: No clock\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	entry_ptr = wlc_rate_shm_offset(wlc, rate);
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		val = SCALE_5_1_TO_5_2_FORMAT(val);
		val = htol16((val << PHYCTL_TXPWR_CORE1_SHIFT) | val);
	}
	wlc_write_shm(wlc, (entry_ptr + wlc->stf->shm_rt_txpwroff_pos), val);
}
#endif /* BCMDBG */

static void
wlc_stf_pproff_shmem_write(wlc_info_t *wlc) /* C_CHECK */
{
	uint8 idx, rate;
	uint8 *cck_ptr, *ofdm_ptr;
	uint16 entry_ptr = 0, val, mask;
	bool is_40 = CHSPEC_IS40(wlc->chanspec);
#ifdef WL11AC
	bool is_80 = CHSPEC_IS80(wlc->chanspec);
	bool is_160 = CHSPEC_IS160(wlc->chanspec);
	bool is_8080 = CHSPEC_IS8080(wlc->chanspec);
#endif // endif
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs;
	ppr_dsss_rateset_t dsss_pwrs;
	ppr_ofdm_rateset_t ofdm_pwrs;
	wl_tx_bw_t bw;
	wlc_stf_rs_shm_offset_t stf_rs;
	BCM_REFERENCE(entry_ptr);
	if (wlc->stf->txpwr_ctl == NULL) {
		WL_ERROR(("wl%d: %s: tx pwr table does not exist.\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: No clock\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	rs_dflt = &cck_ofdm_rates;
	wlc_rateset_copy(rs_dflt, &rs);

	/* CCK rates */

	cck_ptr = (uint8*)dsss_pwrs.pwr;
	idx = (uint8)WLC_BITSCNT(wlc->stf->txcore[CCK_IDX][1]);
#ifdef WL11AC
	if (is_8080)
		bw = WL_TX_BW_20IN8080;
	else if (is_160)
		bw = WL_TX_BW_20IN160;
	else if (is_80)
		bw = WL_TX_BW_20IN80;
	else
#endif // endif
	bw = is_40 ? WL_TX_BW_20IN40 : WL_TX_BW_20;
	if (idx == 1) {
		ppr_get_dsss(wlc->stf->txpwr_ctl, bw, WL_TX_CHAINS_1, &dsss_pwrs);
	} else if (idx == 2) {
		ppr_get_dsss(wlc->stf->txpwr_ctl, bw, WL_TX_CHAINS_2, &dsss_pwrs);
	} else if (idx == 3) {
		ppr_get_dsss(wlc->stf->txpwr_ctl, bw, WL_TX_CHAINS_3, &dsss_pwrs);
	} else if (idx == 4) {
		ppr_get_dsss(wlc->stf->txpwr_ctl, bw, WL_TX_CHAINS_4, &dsss_pwrs);
	}

	ofdm_ptr = (uint8*)ofdm_pwrs.pwr;
	idx = (uint8)WLC_BITSCNT(wlc->stf->txcore[OFDM_IDX][1]);
#ifdef WL11AC
	if (WLC_PHY_AS_80P80(wlc, wlc->chanspec) && (is_8080 || is_160)) {
		idx = idx / 2;
	}
	if (is_8080)
		bw = WL_TX_BW_8080;
	else if (is_160)
		bw = WL_TX_BW_160;
	else if (is_80)
		bw = WL_TX_BW_80;
	else
#endif // endif
	bw = is_40 ? WL_TX_BW_40 : WL_TX_BW_20;
	if (idx == 1) {
		ppr_get_ofdm(wlc->stf->txpwr_ctl, bw, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_pwrs);
	} else if (idx == 2) {
		ppr_get_ofdm(wlc->stf->txpwr_ctl, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_pwrs);
	} else if (idx == 3) {
		ppr_get_ofdm(wlc->stf->txpwr_ctl, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm_pwrs);
	} else if (idx == 4) {
		ppr_get_ofdm(wlc->stf->txpwr_ctl, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_4, &ofdm_pwrs);
	}

	for (idx = 0; idx < rs.count; idx++) {

		stf_rs.rate[idx] = rate = rs.rates[idx];
		if (RATE_ISOFDM(rate))
			val = (uint16)*ofdm_ptr++;
		else
			val = (uint16)*cck_ptr++;

		if (val == (uint8)(WL_RATE_DISABLED)) {
			val = wlc->stf->max_offset;
		}

		if (D11REV_GE(wlc->pub->corerev, 128)) {
			val = SCALE_5_1_TO_5_2_FORMAT(val);
			val = htol16((val << PHYCTL_TXPWR_CORE1_SHIFT) | val);
		} else if (D11REV_GE(wlc->pub->corerev, 40)) {
			val <<= M_REV40_RT_HTTXPWR_OFFSET_SHIFT;
			val &= M_REV40_RT_HTTXPWR_OFFSET_MASK;
		}

		stf_rs.val[idx] = val;
#ifdef WL11AC
		if (is_80 || is_160 || is_8080)
			WL_NONE(("%s: Write PPR to Rate table in SHMEM %s (%s MHz) %3d(500K): "
				"0x%x = %d\n",
				__FUNCTION__, RATE_ISOFDM(rate) ? "OFDM":"CCK ",
				is_8080 ? "80+80":(is_160 ? "160":"80"), rate,
				(entry_ptr + wlc->stf->shm_rt_txpwroff_pos), val));
		else
#endif // endif
			WL_NONE(("%s: Write PPR to Rate table in SHMEM %s (%s MHz) %3d(500K): "
				"0x%x = %d\n",
				__FUNCTION__, RATE_ISOFDM(rate) ? "OFDM":"CCK ",
				is_40 ? "40":"20", rate,
				(entry_ptr + wlc->stf->shm_rt_txpwroff_pos), val));
	}

	if (D11REV_GE(wlc->pub->corerev, 128))
		mask = M_REV128_RT_HTTXPWR_OFFSET_MASK;
	else
		mask = M_REV40_RT_HTTXPWR_OFFSET_MASK;

	wlc_bmac_stf_set_rateset_shm_offset(wlc->hw, rs.count, wlc->stf->shm_rt_txpwroff_pos,
		mask, &stf_rs);

	wlc_stf_compute_ack_pprs(wlc);
#ifdef WL_UCM
	phy_btcx_ucm_update_only_ack_pwr_offset(WLC_PI(wlc));
#endif // endif
}

static void
wlc_stf_convert_to_s41_hdb(void *context, uint8 *a, uint8 *b)
{
	uint8 *max_offset = (uint8*)context;
	int8 s41;

	/*
	 * Ensure, when calculating offsets, that we don't set anything above max_offset,
	 * except for explicitly disabled rates.
	 */
	if (*a == (uint8)WL_RATE_DISABLED)
		*b = (uint8)WL_RATE_DISABLED;
	else {
		/* Round-up to avoid sending a quarter too much power */
		s41 = (*a + 1) >> 1;
		*b = MIN(s41, *max_offset);
	}
}

#ifdef WL_SARLIMIT
static void
wlc_stf_convert_to_s42_qdb(void *context, uint8 *a, uint8 *b)
{
	uint8 *max_offset = (uint8*)context;

	/*
	 * Ensure, when calculating offsets, that we don't set anything above max_offset,
	 * except for explicitly disabled rates.
	 */
	if (*a == (uint8)WL_RATE_DISABLED)
		*b = (uint8)WL_RATE_DISABLED;
	else {
		*b = MIN(*a, ((*max_offset) * 2));
	}
}
#endif /* WL_SARLIMIT */

static void
wlc_stf_ppr_format_convert(wlc_info_t *wlc, ppr_t *txpwr, ppr_ru_t *ru_txpwr)
{
	int8 max_offset;

	if ((wlc->stf->txpwr_ctl == NULL) || (wlc->stf->ru_txpwr_ctl == NULL)) {
		WL_ERROR(("wl%d: %s: tx pwr table does not exist.\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* calculate max offset and convert to 0.5 dB */
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		max_offset = 0x3f;	/* cap to 31.5 dBm Max */
	} else {
		max_offset = 0x1f;	/* cap to 15.5 dBm Max */
	}
	wlc->stf->max_offset = max_offset;
	ppr_map_vec_all(wlc_stf_convert_to_s41_hdb, (void*)&max_offset, txpwr,
		wlc->stf->txpwr_ctl);
#ifdef WL_SARLIMIT
	ppr_map_vec_all(wlc_stf_convert_to_s42_qdb, (void*)&max_offset, txpwr,
		wlc->stf->txpwr_ctl_qdbm);
#endif /* WL_SARLIMIT */
#ifdef WL11AX
	ppr_ru_map_vec_all(wlc_stf_convert_to_s41_hdb, (void*)&max_offset, ru_txpwr,
		wlc->stf->ru_txpwr_ctl);
#endif /* WL11AX */
}

#define CC_CODE_LEN_BYTES (3)
#define NUM_LOG_ENTRIES (8)

void
wlc_update_txppr_offset(wlc_info_t *wlc, ppr_t *txpwr, ppr_ru_t *ru_txpwr)
{
	wlc_stf_t *stf = wlc->stf;
	int chain;
	uint8 txchain;
	int min_txpwr_limit, max_txpwr_limit;
	int8 min_target;
	wlc_phy_t *pi = WLC_PI(wlc);

	BCM_REFERENCE(max_txpwr_limit);

	WL_TRACE(("wl%d: %s: update txpwr\n", wlc->pub->unit, __FUNCTION__));

	wlc_iovar_getint(wlc, "min_txpower", &min_txpwr_limit);
	min_txpwr_limit = min_txpwr_limit * WLC_TXPWR_DB_FACTOR;	/* make qdbm */
	max_txpwr_limit = (int)phy_tpc_get_target_max(pi);

#ifndef WLTEST
	if (max_txpwr_limit < wlc_phy_maxtxpwr_lowlimit(WLC_PI(wlc))) {
		/* WL_ERROR(("FATAL ERROR: min_txpwr=%d, max_txpwr=%d\n",
		 *	min_txpwr_limit, max_txpwr_limit));
		 * wlc->hw->need_reinit = WL_REINIT_RC_STF_TXPWRLIM;
		 * WLC_FATAL_ERROR(wlc);
		 */
		int8 length, i;
		wlc->maxpwrlimit_fail++;
		length = (strlen(wlc_channel_ccode(wlc->cmi)) + 1);
		for (i = NUM_LOG_ENTRIES-2; i >= 0; i--) {
			strncpy(wlc->ccode + length*(i+1), wlc->ccode + length*i,
					CC_CODE_LEN_BYTES);
			wlc->chanspec_array[i+1] = wlc->chanspec_array[i];
		}
		strncpy(wlc->ccode, wlc_channel_ccode(wlc->cmi), length);
		wlc->chanspec_array[0] = wlc->chanspec;
	}
#endif /* WLTEST */

#ifdef WL_BEAMFORMING
	wlc_txbf_txpower_target_max_upd(wlc->txbf, (int8)max_txpwr_limit);
#endif // endif

	ASSERT(stf->txpwr_ctl);
	/* txpwr_ctrl was allocated on PPR_MAX_BW. init it with correct bandwidth here */
	ppr_set_ch_bw(stf->txpwr_ctl, PPR_CHSPEC_BW(wlc->chanspec));
#ifdef WL_SARLIMIT
	ASSERT(stf->txpwr_ctl_qdbm);
	ppr_set_ch_bw(stf->txpwr_ctl_qdbm, PPR_CHSPEC_BW(wlc->chanspec));
#endif // endif

	/* need to convert from 0.25 dB to 0.5 dB for use in phy ctl word */
	wlc_stf_ppr_format_convert(wlc, txpwr, ru_txpwr);

	/* If the minimum tx power target with the per-chain offset applied
	 * is below the min tx power target limit, disable the core for tx.
	 */
	min_target = (int)phy_tpc_get_target_min(pi);

	/* if the txchain offset brings the chain below the lower limit
	 * disable the chain
	 */
	txchain = stf->hw_txchain;
	if (!OLPC_ENAB(wlc)) {
		for (chain = 0; chain < WL_NUM_TXCHAIN_MAX; chain++) {
			WL_NONE(("wl%d: %s: chain %d, "
				 "min_target %d min_limit %d offset %d\n",
				 wlc->pub->unit, __FUNCTION__, chain,
				 min_target, min_txpwr_limit,
				 stf->txchain_pwr_offsets.offset[chain]));

			if (min_target + stf->txchain_pwr_offsets.offset[chain] <
				min_txpwr_limit) {
				WL_NONE(("wl%d: %s: disable chain %d\n",
					wlc->pub->unit, __FUNCTION__, chain));

				txchain &= ~(1<<chain);
			}
		}
	}
#ifdef WL_STF_ARBITRATOR
	if (WLC_STF_ARB_ENAB(wlc->pub)) {
		if (txchain && (txchain != stf->hw_txchain)) {
			wlc_stf_nss_request_update(wlc, wlc->stf->arb->txppr_req,
				WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE,
				txchain, WLC_BITSCNT(txchain),
				wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
		} else {
			wlc_stf_nss_request_update(wlc, wlc->stf->arb->txppr_req,
				WLC_STF_ARBITRATOR_REQ_STATE_RXTX_INACTIVE,
				wlc->stf->hw_txchain, WLC_BITSCNT(wlc->stf->hw_txchain),
				wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
		}
	} else
#endif /* WL_STF_ARBITRATOR */
	wlc_stf_txchain_set(wlc, txchain, TRUE, WLC_TXCHAIN_ID_PWR_LIMIT);
	wlc_stf_spatial_mode_set(wlc, wlc->chanspec);
	wlc_stf_pproff_shmem_write(wlc);
}

static uint8
wlc_stf_get_target_core(wlc_info_t *wlc)
{
	/* don't disable anymore chains, if we already have one or more
	 * chain(s) disabled
	 */
	if (wlc->stf->txstreams < WLC_BITSCNT(wlc->stf->hw_txchain))
		return wlc->stf->txchain;

	/* priority is disable core 1 (middle), then core 0, then core 2
	 * if core 1 is set and allow to be disable, then disable core 1
	 * if core 0 is set and allow to be disable, then disable core 0
	 * if core 2 is set and allow to be disable, then disable core 2
*/
	if (WLC_ISTHROTTLE_ENB_PHY(wlc)) {
	  if ((wlc->stf->pwr_throttle_mask & 2) && (wlc->stf->txchain & 2)) {
		return (wlc->stf->txchain & ~2);
	  }
	  else if ((wlc->stf->pwr_throttle_mask & 1) && (wlc->stf->txchain & 1))
	    return (wlc->stf->txchain & ~1);
	  else if ((wlc->stf->pwr_throttle_mask & 4) && (wlc->stf->txchain & 4))
	    return (wlc->stf->txchain & ~4);
	}
	return wlc->stf->txchain;
}

static void
wlc_stf_pwrthrottle_upd(wlc_info_t *wlc)
{
	bool shared_ant0 = ((wlc->pub->boardflags2 & BFL2_BT_SHARE_ANT0) ==
	                    BFL2_BT_SHARE_ANT0);
	uint8 chain = wlc->stf->hw_rxchain;
	uint32 gpioin, mask;
	uint8 pwr_throttle_req; /* Power throttle request according to GPIO state */

	if (wlc->stf->pwrthrottle_config == 0 && wlc->stf->pwrthrottle_pin == 0)
		return;

	gpioin = mask = wlc->stf->pwrthrottle_pin;

#ifndef PWRTHROTTLE_GPIO
	/* Check if we disabled the pwrthrottle GPIO */
	if (gpioin == 0)
		gpioin = mask = 1;
	else
#endif // endif
	/* Read GPIO only for AUTO mode */
	if (wlc->stf->pwr_throttle == AUTO)
		gpioin = si_gpioin(wlc->pub->sih);

	/* WLAN_PWR active LOW, (gpioin & mask) == 0 */
	pwr_throttle_req = (((gpioin & mask) == 0) || (wlc->stf->pwr_throttle == ON)) ?
		WLC_PWRTHROTTLE_ON : WLC_THROTTLE_OFF;

	WL_NONE(("wl%d: %s: pwr_throttle:%d gpioin:%d pwr_throttle_test:%d"
	         " throttle_state:0x%02x pwr_throttle_req:0x%02x\n", wlc->pub->unit,
	         __FUNCTION__, wlc->stf->pwr_throttle, gpioin, wlc->stf->pwr_throttle_test,
	         wlc->stf->throttle_state, pwr_throttle_req));

	if ((wlc->stf->throttle_state & WLC_PWRTHROTTLE_ON) == pwr_throttle_req)
		return;

	wlc->stf->throttle_state &= ~WLC_PWRTHROTTLE_ON;
	if (pwr_throttle_req)
		wlc->stf->throttle_state |= pwr_throttle_req;

	ASSERT(wlc->pub->up);
	if (!wlc->pub->up)
		return;

	if (wlc->stf->throttle_state & WLC_PWRTHROTTLE_ON) {
	        bool isj28;
		if (wlc->stf->pwrthrottle_config & PWRTHROTTLE_DUTY_CYCLE)
			chain = shared_ant0? 0x2:0x1;
		else
			chain = wlc_stf_get_target_core(wlc);

#ifdef BCMDBG
		/* For experimentations, use specific chain else board default */
		if (wlc->stf->pwr_throttle_test &&
		    (wlc->stf->pwr_throttle_test < wlc->stf->hw_txchain)) {
			chain = wlc->stf->pwr_throttle_test;
		}
#endif /* BCMDBG */

		isj28 = (wlc->pub->sih->boardvendor == VENDOR_APPLE &&
		         ((wlc->pub->sih->boardtype == BCM94360J28_D11AC2G) ||
		          (wlc->pub->sih->boardtype == BCM94360J28_D11AC5G)));

		if (!isj28) {
#ifdef WL_STF_ARBITRATOR
			if (WLC_STF_ARB_ENAB(wlc->pub)) {
				wlc_stf_nss_request_update(wlc, wlc->stf->arb->pwrthrottle_req,
				    WLC_STF_ARBITRATOR_REQ_STATE_TX_ACTIVE,
				    chain, WLC_BITSCNT(chain),
				    wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
			} else
#endif // endif
				wlc_stf_txchain_set(wlc, chain, TRUE, WLC_TXCHAIN_ID_PWRTHROTTLE);
		}
	} else {
		if (wlc->stf->pwrthrottle_config & PWRTHROTTLE_DUTY_CYCLE) {
#ifdef WL_STF_ARBITRATOR
			if (WLC_STF_ARB_ENAB(wlc->pub)) {
				wlc_stf_nss_request_update(wlc, wlc->stf->arb->pwrthrottle_req,
				    WLC_STF_ARBITRATOR_REQ_STATE_RXTX_INACTIVE,
				    wlc->stf->hw_txchain, WLC_BITSCNT(wlc->stf->hw_txchain),
				    wlc->stf->rxchain, WLC_BITSCNT(wlc->stf->rxchain));
			} else
#endif // endif
				wlc_stf_txchain_set(wlc, wlc->stf->hw_txchain, FALSE,
				                    WLC_TXCHAIN_ID_PWRTHROTTLE);
		}
	}

	/* Duty cycle changes if necessary are handled here */
	if (wlc->stf->pwrthrottle_config & PWRTHROTTLE_DUTY_CYCLE)
	  wlc_txduty_upd(wlc);
}

void
wlc_set_pwrthrottle_config(wlc_info_t *wlc)
{
	bool is4360, isx14;

	is4360 = (wlc->pub->sih->boardvendor == VENDOR_APPLE &&
	          ((wlc->pub->sih->chip == BCM4352_CHIP_ID) ||
	           (wlc->pub->sih->chip == BCM4360_CHIP_ID) ||
	           (wlc->pub->sih->chip == BCM43602_CHIP_ID)));

	is4360 |= ((wlc->pub->sih->boardvendor == VENDOR_BROADCOM) &&
	           (wlc->pub->sih->boardtype == BCM94360CS));

	isx14 = ((wlc->pub->sih->boardvendor == VENDOR_APPLE) &&
	         ((wlc->pub->sih->boardtype == BCM94350X14) ||
	          (wlc->pub->sih->boardtype == BCM94350X14P2)));

	wlc->stf->pwrthrottle_config = 0;
	wlc->stf->pwrthrottle_pin = 0;

	if (!is4360 && !isx14)
		return;

	wlc->stf->pwrthrottle_config = PWRTHROTTLE_CHAIN;
#ifdef PWRTHROTTLE_GPIO
	wlc->stf->pwrthrottle_pin = (isx14) ? BOARD_GPIO_13_WLAN_PWR : BOARD_GPIO_2_WLAN_PWR;
#endif // endif

	/* initialize maximum allowed duty cycle */
	if ((wlc->stf->throttle_state & WLC_PWRTHROTTLE_ON) &&
	    (wlc->stf->pwrthrottle_config & PWRTHROTTLE_DUTY_CYCLE)) {
		wlc_stf_duty_cycle_set(wlc, wlc->stf->tx_duty_cycle_pwr, TRUE, TRUE);
		wlc_stf_duty_cycle_set(wlc, wlc->stf->tx_duty_cycle_pwr, FALSE, TRUE);
	} else if (wlc->stf->throttle_state == WLC_THROTTLE_OFF) {
		wlc_stf_duty_cycle_set(wlc, wlc->stf->tx_duty_cycle_ofdm, TRUE, TRUE);
		wlc_stf_duty_cycle_set(wlc, wlc->stf->tx_duty_cycle_cck, FALSE, TRUE);
	}
}

void
wlc_stf_txchain_get_perrate_state(wlc_info_t *wlc, wlc_stf_txchain_st *state,
	wlc_stf_txchain_evt_notify func)
{
	*state = (wlc->stf->txcore_override[CCK_IDX] & 0xf);
	*state <<= 8;
	*state |= (wlc->stf->txcore_override[OFDM_IDX] & 0xf);
	WL_NONE(("%s: %x %x %x\n", __FUNCTION__, *state, (wlc->stf->txcore_override[CCK_IDX] & 0xf),
		(wlc->stf->txcore_override[OFDM_IDX] & 0xf)));

	/* OLPC uses this currently; do not reuse for other uses - instead switch to notif infra. */
	wlc->stf->txchain_perrate_state_modify = func;
}

void
wlc_stf_txchain_restore_perrate_state(wlc_info_t *wlc, wlc_stf_txchain_st *state)
{
	uint8 txchain = wlc->stf->txchain;
	uint8 core_map;
	WL_NONE(("%s: %x %x %x\n", __FUNCTION__, *state, (wlc->stf->txcore_override[CCK_IDX] & 0xf),
		(wlc->stf->txcore_override[OFDM_IDX] & 0xf)));

	/* don't do callback when restoring state */
	wlc->stf->txchain_perrate_state_modify = NULL;

	/* restore CCK, OFDM rate overrides, then call spatial update */
	core_map = *state & 0xf;
	/* ensure txcore_override doesn't conflict with txchain */
	if ((txchain & core_map) != core_map) {
		core_map = 0;
	}
	wlc->stf->txcore_override[OFDM_IDX] = core_map;

	core_map = (*state >> 8) & 0xf;
	/* ensure txcore_override doesn't conflict with txchain */
	if ((txchain & core_map) != core_map) {
		core_map = 0;
	}
	wlc->stf->txcore_override[CCK_IDX] = core_map;

	WL_NONE(("%s end: %x %x %x\n", __FUNCTION__, *state,
		(wlc->stf->txcore_override[CCK_IDX] & 0xf),
		(wlc->stf->txcore_override[CCK_IDX] & 0xf)));

	/* keep spatial policy and restore old settings of rates above */
	wlc_stf_spatial_policy_set(wlc, wlc->stf->spatialpolicy);
}

bool
wlc_stf_saved_state_is_consistent(wlc_info_t *wlc, wlc_stf_txchain_st *state)
{
	uint8 cck_val, ofdm_val;
	if (*state == 0) {
		/* always ok to restore to 0 */
		return TRUE;
	}
	ofdm_val = *state & 0xf;
	cck_val = (*state >> 8) & 0xf;
	if ((ofdm_val & wlc->stf->txchain) != ofdm_val ||
		(cck_val & wlc->stf->txchain) != cck_val) {
		return FALSE;
	}
	return TRUE;
}

#ifdef WL_BEAMFORMING
void
wlc_stf_set_txbf(wlc_info_t *wlc, bool enable)
{
	if (TXBF_ENAB(wlc->pub)) {
		wlc->stf->allow_txbf = enable;
		wlc_txbf_upd(wlc->txbf);
	}
}
#endif /* WL_BEAMFORMING */

#if defined(WL_EXPORT_CURPOWER) || defined(WLC_DTPC)
/* Get power based on the given rspec and ppr data */
int8
get_pwr_from_targets(wlc_info_t *wlc, ratespec_t rspec, ppr_t *txpwr)
{
	uint8 rate;
	int8 offset;
	bool is160MHz = RSPEC_IS160MHZ(rspec);
	bool is80MHz = RSPEC_IS80MHZ(rspec);
	bool is40MHz = RSPEC_IS40MHZ(rspec);
	ppr_dsss_rateset_t dsss_pwrs;
	ppr_ofdm_rateset_t ofdm_pwrs;
	ppr_vht_mcs_rateset_t mcs_pwrs;
	ppr_he_mcs_rateset_t he_mcs_pwrs;
	wl_tx_mode_t mode;
	wl_tx_nss_t nss = wlc_ratespec_nss(rspec);
	wl_tx_chains_t chains = WL_TX_CHAINS_1;
	chains = wlc_stf_txchain_get(wlc, rspec);
	mode = WL_TX_MODE_NONE;

	BCM_REFERENCE(he_mcs_pwrs);

	if (RSPEC_ISTXBF(rspec)) {
		mode = WL_TX_MODE_TXBF;
	} else if (RSPEC_ISSTBC(rspec)) {
		mode = WL_TX_MODE_STBC;
		/* STBC expansion doubles the Nss */
		nss = nss*2;
		ASSERT((uint8)nss <= (uint8)chains);
	} else if (RSPEC_TXEXP(rspec) && (nss == 1)) {
		mode = WL_TX_MODE_CDD;
	}

	if (RSPEC_ISHE(rspec)) {
		rate = (rspec & WL_RSPEC_HE_MCS_MASK);
	} else if (RSPEC_ISVHT(rspec)) {
		rate = (rspec & WL_RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		rate = (rspec & WLC_RSPEC_HT_MCS_MASK); /* for HT MCS, convert to a 0-7 index */
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
			if ((rspec & WLC_RSPEC_HT_PROP_MCS_MASK) >= WLC_11N_FIRST_PROP_MCS)
				rate = PROP11N_2_VHT_MCS(rspec);
		}
	} else {
		rate = (uint8)RSPEC2RATE(rspec); /* in [500Kbps] units */
	}

	if (RSPEC_ISHE(rspec)) {
		/* Fill up txpwr for txbf OFF */
		if (is160MHz) {
			ppr_get_he_mcs(txpwr, WL_TX_BW_160, nss, mode, chains, &he_mcs_pwrs,
				WL_HE_RT_SU);
		} else if (is80MHz) {
			ppr_get_he_mcs(txpwr, WL_TX_BW_80, nss, mode, chains, &he_mcs_pwrs,
				WL_HE_RT_SU);
		} else if (is40MHz) {
			ppr_get_he_mcs(txpwr, WL_TX_BW_40, nss, mode, chains, &he_mcs_pwrs,
				WL_HE_RT_SU);
		} else {
			ppr_get_he_mcs(txpwr, WL_TX_BW_20, nss, mode, chains, &he_mcs_pwrs,
				WL_HE_RT_SU);
		}
		offset = he_mcs_pwrs.pwr[rate];
	} else if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
		/* Fill up txpwr for txbf OFF */
		if (is160MHz) {
			ppr_get_vht_mcs(txpwr, WL_TX_BW_160, nss, mode, chains, &mcs_pwrs);
		} else if (is80MHz) {
			ppr_get_vht_mcs(txpwr, WL_TX_BW_80, nss, mode, chains, &mcs_pwrs);
		} else if (is40MHz) {
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40, nss, mode, chains, &mcs_pwrs);
		} else {
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20, nss, mode, chains, &mcs_pwrs);
		}
		offset = mcs_pwrs.pwr[rate];
	} else if (RSPEC_ISOFDM(rspec)) {
		rate = ofdm_pwr_idx_table[rate/6];
		ASSERT(rate != 0x80);
		if (is160MHz) {
			ppr_get_ofdm(txpwr, WL_TX_BW_160, mode, chains, &ofdm_pwrs);
		} else if (is80MHz) {
			ppr_get_ofdm(txpwr, WL_TX_BW_80, mode, chains, &ofdm_pwrs);
		} else if (is40MHz) {
			ppr_get_ofdm(txpwr, WL_TX_BW_40, mode, chains, &ofdm_pwrs);
		} else {
			ppr_get_ofdm(txpwr, WL_TX_BW_20, mode, chains, &ofdm_pwrs);
		}
		offset = ofdm_pwrs.pwr[rate];
	} else if (RSPEC_ISCCK(rspec)) {
		rate = cck_pwr_idx_table[rate >> 1];
		ASSERT(rate != 0x80);
		if (is160MHz) {
			ppr_get_dsss(txpwr, WL_TX_BW_20IN160, chains, &dsss_pwrs);
		} else if (is80MHz) {
			ppr_get_dsss(txpwr, WL_TX_BW_20IN80, chains, &dsss_pwrs);
		} else if (is40MHz) {
			ppr_get_dsss(txpwr, WL_TX_BW_20IN40, chains, &dsss_pwrs);
		} else {
			ppr_get_dsss(txpwr, WL_TX_BW_20, chains, &dsss_pwrs);
		}
		offset = dsss_pwrs.pwr[rate];
	} else {
		WL_ERROR(("INVALID rspec %x\n", rspec));
		ASSERT(!"INVALID rspec");
		return BCME_BADARG;
	}
	return offset;
}
#endif /* defined(WL_EXPORT_CURPOWER) || WLC_DTPC */

void
wlc_stf_chain_init(wlc_info_t *wlc)
{
	if (wlc->stf->channel_bonding_cores) {
		phy_stf_chain_init(WLC_PI(wlc),
			wlc->stf->channel_bonding_cores,
			wlc->stf->channel_bonding_cores);
		phy_stf_chain_set(WLC_PI(wlc), wlc->stf->channel_bonding_cores,
			wlc->stf->channel_bonding_cores);
	} else {
		phy_stf_chain_init(WLC_PI(wlc), wlc->stf->hw_txchain, wlc->stf->hw_rxchain);
		phy_stf_chain_set(WLC_PI(wlc), wlc->stf->txchain, wlc->stf->rxchain);
	}
}

static int
BCMUNINITFN(wlc_stf_down)(void *hdl)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	BCM_REFERENCE(wlc);
#if defined(WL_PWRSTATS)
	if (PWRSTATS_ENAB(wlc->pub)) {
		/* Take the current mimo metrics before going down */
		(void) wlc_mimo_siso_metrics_report(wlc, FALSE);
	}
#endif /* WL_PWRSTATS */
	wlc->stf->op_txstreams = wlc->stf->txstreams;
	wlc->stf->op_rxstreams = wlc->stf->rxstreams;
	return 0;
}

#ifdef WL_NAP
void wlc_stf_nap_enable_reason(wlc_info_t *wlc, uint16 reason)
{
	wlc_phy_nap_disable_req_set(WLC_PI(wlc), reason, FALSE, FALSE, 0);
}

void wlc_stf_nap_disable_reason(wlc_info_t *wlc, uint16 reason)
{
	wlc_phy_nap_disable_req_set(WLC_PI(wlc), reason, TRUE, FALSE, 0);
}

void
wlc_stf_nap_rssi_thresh_handling(wlc_info_t* wlc)
{
	wlc_phy_t *pih = WLC_PI(wlc);
	int8 rssi;
	uint16 bits;
	bool disable_rssi;

	wlc_phy_nap_status_get(pih, &bits, NULL, NULL);

	/* Bit indicating nap disabled by host */
	if (bits & NAP_DISABLED_HOST) {
		/* If disabled by host return */
		return;
	} else {
		disable_rssi = bits & NAP_DISABLED_RSSI;
	}

	/* Get the weakest beacon RSSI */
	rssi = wlc_get_weakest_link_chain_rssi(wlc);

	/* Bit indicating nap disabled by rssi */
	if (disable_rssi) {
		if (rssi >= (wlc->stf->nap_rssi_threshold + wlc->stf->nap_rssi_delta)) {
			wlc_stf_nap_enable_reason(wlc, NAP_DISABLED_RSSI);
			WL_INFORM((" WLC STF NAP : NAP is enabled, RSSI %d NAP threshold %d\n",
					rssi, wlc->stf->nap_rssi_threshold));
		}
	} else if (rssi < wlc->stf->nap_rssi_threshold) {
		wlc_stf_nap_disable_reason(wlc, NAP_DISABLED_RSSI);
		WL_INFORM((" WLC STF NAP : AP is disabled, RSSI %d NAP threshold %d\n",
				rssi, wlc->stf->nap_rssi_threshold));
	}
}

static void
wlc_stf_nap_assoc_state_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_bsscfg_t *cfg;
	wlc_info_t *wlc;

	ASSERT(notif_data);
	cfg = notif_data->cfg;
	ASSERT(cfg);
	wlc = cfg->wlc;
	ASSERT(wlc);

	if (!(NAP_ENAB(wlc->pub) && BSSCFG_INFRA_STA(cfg)))
		return;

	switch (notif_data->state) {
		case AS_IDLE:
			/* enable nap upon entering idle sate (success or fail) */
			wlc_stf_nap_enable_reason(wlc, NAP_DISABLED_ASSOC);
			break;
		default:
			/* disable nap during association */
			wlc_stf_nap_disable_reason(wlc, NAP_DISABLED_ASSOC);
			break;
	}
}
#endif /* WL_NAP */

#ifdef OCL
void
wlc_stf_ocl_rssi_thresh_handling(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t* wlc = bsscfg->wlc;
	wlc_phy_t   *pih = WLC_PI(wlc);
	int8 rssi_ant[WL_RSSI_ANT_MAX], rssi = 0;
	uint8 curr_coremask;
	uint16 bits;
	bool disable;
	bool reconfig = FALSE;

	phy_ocl_status_get(pih, &bits, &curr_coremask, NULL);
	wlc_lq_rssi_ant_get_api(wlc, bsscfg, rssi_ant);
	rssi = rssi_ant[curr_coremask >> 1];
	if (rssi != WLC_RSSI_INVALID) {
		ASSERT(wlc->stf->ocl_info != NULL);
		disable = !!(bits & OCL_DISABLED_RSSI);

		if (disable) {
			if (rssi >= (wlc->stf->ocl_info->ocl_rssi_threshold +
					wlc->stf->ocl_info->ocl_rssi_delta)) {
				reconfig = TRUE;
				WL_INFORM(("WLC STF OCL : OCL is enabled, RSSI %d"
				          " OCL threshold %d\n",
				          rssi, wlc->stf->ocl_info->ocl_rssi_threshold));
			}
		} else if (rssi < wlc->stf->ocl_info->ocl_rssi_threshold) {
			reconfig = TRUE;
			WL_INFORM(("WLC STF OCL : OCL is disabled, RSSI %d OCL threshold %d\n",
				rssi, wlc->stf->ocl_info->ocl_rssi_threshold));
		}

		/* if changing state then get stats snapshot */
		if (reconfig) {
#if defined(WL_PWRSTATS)
			if (PWRSTATS_ENAB(wlc->pub)) {
				wlc_mimo_siso_metrics_snapshot(wlc,
					FALSE, WL_MIMOPS_METRICS_SNAPSHOT_OCL);
			}
#endif /* WL_PWRSTATS */
			phy_ocl_disable_req_set(pih, OCL_DISABLED_RSSI,
			                            !disable, WLC_OCL_REQ_RSSI);
		}
	}
}
void
wlc_stf_ocl_update_assoc_pend(wlc_info_t* wlc, bool disable)
{
	wlc_phy_t   *pih = WLC_PI(wlc);
	bool bit_present;
	uint16 bits;

	phy_ocl_status_get(pih, &bits, NULL, NULL);
	bit_present = !!(OCL_DISABLED_ASPEND & bits);

	if ((disable && !bit_present) || (!disable && bit_present)) {
		phy_ocl_disable_req_set(pih, OCL_DISABLED_ASPEND,
			disable, WLC_OCL_REQ_ASPEND);
	}
}

static void
wlc_stf_ocl_assoc_state_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_bsscfg_t *cfg;
	wlc_info_t *wlc;

	ASSERT(notif_data);
	cfg = notif_data->cfg;
	ASSERT(cfg);
	wlc = cfg->wlc;
	ASSERT(wlc);

	if (!((OCL_ENAB(wlc->pub)) && (BSSCFG_INFRA_STA(cfg)) && (cfg == wlc->cfg)))
		return;

	switch (notif_data->state) {
		case AS_IDLE:
			/* upon entering idle sate (success or fail),
			 * clear disable reason for ASPEND
			 */
			wlc_stf_ocl_update_assoc_pend(wlc, FALSE);
			break;
		default:
			/* switching to any other state should disable ocl  */
			wlc_stf_ocl_update_assoc_pend(wlc, TRUE);
			break;
	}
}
#endif /* OCL */

#if defined(OCL) || defined(WL_MIMOPS_CFG)
static void
wlc_stf_rx_bcn_notif(void *ctx, bss_rx_bcn_notif_data_t *notif_data)
{

	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg;

	ASSERT(notif_data && wlc);
	cfg = notif_data->cfg;
	ASSERT(cfg);

	if (!(BSSCFG_INFRA_STA(cfg) && wlc->stas_associated))
		return;

#ifdef OCL
	if (OCL_ENAB(wlc->pub)) {
		wlc_stf_ocl_rssi_thresh_handling(cfg);
	}
#endif // endif
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		wlc_stf_mrc_thresh_handling(cfg);
	}
#endif // endif
}
#endif /* OCL || WL_MIMOPS_CFG */

#ifdef WL_STF_ARBITRATOR
void wlc_stf_set_arbitrated_chains_complete(wlc_info_t *wlc)
{
	 /* push the updated txant to phytxant (used for txheader) */
	_wlc_stf_phy_txant_upd(wlc);
	/* initialize txcore and spatial policy */
	wlc_stf_init_txcore_default(wlc);
	wlc_stf_spatial_mode_set(wlc, wlc->chanspec);
	/* invalidate txcache since rates are changing */
	if (WLC_TXC_ENAB(wlc) && wlc->txc != NULL)
		wlc_txc_inv_all(wlc->txc);

	phy_stf_chain_set(WLC_PI(wlc), wlc->stf->txchain, wlc->stf->rxchain);

#if defined(WL_BEAMFORMING)
	if (TXBF_ENAB(wlc->pub)) {
		wlc_txbf_txchain_upd(wlc->txbf);
	}
#endif /* defined(WL_BEAMFORMING) */
}
#endif /* WL_ARBITRATOR */

/* Disable switch to MIMO on recv-ing multicast TIM if applicable */
int
wlc_stf_siso_bcmc_rx(wlc_info_t *wlc, bool enable)
{
	bool val;
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		val = (wlc_mhf_get(wlc, MHF4, WLC_BAND_AUTO) & MHF4_SISO_BCMC_RX) ? TRUE : FALSE;
		if (val == enable)
			return BCME_OK;
		wlc_mhf(wlc, MHF4, MHF4_SISO_BCMC_RX, enable ? MHF4_SISO_BCMC_RX : 0, WLC_BAND_ALL);
#ifdef WL_MIMOPS_CFG
		if (enable)
			WL_STF_MIMO_PS_INFO(
			    (" WLC STF MIMO PS : AP is SISO setting beacon in SISO reception"));
#endif // endif
	}
	return BCME_OK;
}

static void
wlc_stf_ack_ppr_get_ppr_offset_2g(wlc_info_t* wlc,
		ppr_dsss_rateset_t* dsss_pwrs1, ppr_ofdm_rateset_t* ofdm_pwrs1, uint8* loop_cnt)
{
	uint8 idx;
	wl_tx_bw_t bw;
	bool is_40 = CHSPEC_IS40(wlc->chanspec);
#ifdef WL11AC
	bool is_80 = CHSPEC_IS80(wlc->chanspec);
#endif /* WL11AC */

#ifdef WL11AC
	if (is_80) {
		bw = WL_TX_BW_20IN80;
	} else
#endif /* WL11AC */
	{
		bw = is_40 ? WL_TX_BW_20IN40 : WL_TX_BW_20;
	}
	*loop_cnt = WLC_BITSCNT(wlc->stf->hw_txchain);
	for (idx = 0; idx < *loop_cnt; idx++) {
		ppr_get_dsss(wlc->stf->txpwr_ctl, bw, idx+1, &dsss_pwrs1[idx]);
		ppr_get_ofdm(wlc->stf->txpwr_ctl, bw,
				((idx == 0) ? WL_TX_MODE_NONE : WL_TX_MODE_CDD),
				idx+1, &ofdm_pwrs1[idx]);
	}
	wlc_bmac_write_shm(wlc->hw,  M_ACK_2GCM_NUMB(wlc), *loop_cnt-1);
}

static void
wlc_stf_ack_ppr_get_ppr_offset_5g(wlc_info_t *wlc,
	ppr_ofdm_rateset_t* ofdm_pwrs1, uint8* loop_cnt)

{
	uint8 ant_info;
	wl_tx_bw_t bw;
	bool is_40 = CHSPEC_IS40(wlc->chanspec);
#ifdef WL11AC
	bool is_80 = CHSPEC_IS80(wlc->chanspec);
#endif /* WL11AC */

	ant_info = (uint8)WLC_BITSCNT(wlc->stf->txcore[OFDM_IDX][1]);
	ant_info = ((ant_info == 1) ? WL_TX_CHAINS_1 :
			((ant_info == 2) ? WL_TX_CHAINS_2 : WL_TX_CHAINS_3));
#ifdef WL11AC
	if (is_80) {
		bw = WL_TX_BW_80;
		ppr_get_ofdm(wlc->stf->txpwr_ctl, WL_TX_BW_20IN80,
				((ant_info > 1) ? WL_TX_MODE_CDD : WL_TX_MODE_NONE),
				ant_info, &ofdm_pwrs1[0]);
		ppr_get_ofdm(wlc->stf->txpwr_ctl, WL_TX_BW_40IN80,
				((ant_info > 1) ? WL_TX_MODE_CDD : WL_TX_MODE_NONE),
				ant_info, &ofdm_pwrs1[1]);
		ppr_get_ofdm(wlc->stf->txpwr_ctl, WL_TX_BW_80,
				((ant_info > 1) ? WL_TX_MODE_CDD : WL_TX_MODE_NONE),
				ant_info, &ofdm_pwrs1[2]);
	} else
#endif /* WL11AC */
	{
		if (is_40) {
			bw = WL_TX_BW_40;
			ppr_get_ofdm(wlc->stf->txpwr_ctl, WL_TX_BW_20IN40,
					((ant_info > 1) ? WL_TX_MODE_CDD : WL_TX_MODE_NONE),
					ant_info, &ofdm_pwrs1[0]);
			ppr_get_ofdm(wlc->stf->txpwr_ctl, WL_TX_BW_40,
					((ant_info > 1) ? WL_TX_MODE_CDD : WL_TX_MODE_NONE),
					ant_info, &ofdm_pwrs1[1]);
		} else {
			bw = WL_TX_BW_20;
			ppr_get_ofdm(wlc->stf->txpwr_ctl, WL_TX_BW_20,
				((ant_info > 1) ? WL_TX_MODE_CDD : WL_TX_MODE_NONE),
					ant_info, &ofdm_pwrs1[0]);
		}
	}
	*loop_cnt = bw + 1;
}

static int
wlc_stf_ack_ppr_set_cck_ppr(wlc_info_t* wlc, uint8* cck_ptr,
		wlc_rateset_t* rs, uint8 *cck_offset, uint8 idx, uint8 idx1, uint16 *rate1_offset)
{
	uint16 val = 0, val1 = 0;
	if (CHSPEC_IS2G(wlc->chanspec))
		val = (uint16)*cck_ptr;
	else
		val = 0;
	if (val == (uint8)(WL_RATE_DISABLED)) {
		val = wlc->stf->max_offset;
	}
	if ((rs->rates[idx1] & RATE_MASK) == WLC_RATE_1M) {
		*rate1_offset = val;
	}
	if ((rs->rates[idx1] & RATE_MASK) != WLC_RATE_2M) {
		if (idx == 1) {
			val1 = wlc_bmac_read_shm(wlc->hw,
				(M_ACKPWRTBL_R0_1(wlc)) + ((*cck_offset*2) +
				((idx>>1)&1))*2);
			val = (((val<<8) & 0xff00) | (val1 & 0xff));
		}
		wlc_bmac_write_shm(wlc->hw,
			((M_ACKPWRTBL_R0_1(wlc))) + ((*cck_offset*2)*2) +
			(((idx>>1)&1)*2), val);
		*cck_offset += 1;
	} else if ((rs->rates[idx1] & RATE_MASK) == WLC_RATE_2M) {
		if (*rate1_offset != val) {
			WL_ERROR(("wl%d: %s: ofst for 1 & 2 is diff\n",
				wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}
	}
	return BCME_OK;
}

static int
wlc_stf_ack_ppr_set_ofdm_ppr(wlc_info_t* wlc, uint8* ofdm_ptr,
		wlc_rateset_t* rs, uint8 *ofdm_offset, uint8 idx,
		uint8 idx1, uint16 *rate6_offset, uint16 *rate12_offset)
{
	uint16 val = 0, val1 = 0;
	val = (uint16)*ofdm_ptr;
	if (val == (uint8)(WL_RATE_DISABLED)) {
		val = wlc->stf->max_offset;
	}
	if ((rs->rates[idx1] & RATE_MASK) == WLC_RATE_6M) {
		*rate6_offset = val;
	} else if ((rs->rates[idx1] & RATE_MASK) == WLC_RATE_12M) {
		*rate12_offset = val;
	}
	if (((rs->rates[idx1] & RATE_MASK) != WLC_RATE_9M) &&
		(rs->rates[idx1] & RATE_MASK) != WLC_RATE_18M) {
		if (idx == 1) {
			val1 = wlc_bmac_read_shm(wlc->hw,
				(M_ACKPWRTBL_R3_0(wlc)) +((*ofdm_offset*2) +
				((idx>>1)&1))*2);
			val = (((val<<8) & 0xff00) | (val1 & 0xff));
		}
		wlc_bmac_write_shm(wlc->hw,
			((M_ACKPWRTBL_R3_0(wlc)) + ((*ofdm_offset*2)*2)) +
			(((idx>>1)&1)*2), val);
		*ofdm_offset += 1;
	} else if ((rs->rates[idx1] & RATE_MASK) == WLC_RATE_9M) {
		/* ack pwr follows the subbbnd pwr, added a lut in */
		/* ucode to poluate offset for subband case as well */
		/* and to reduce the number of entries/shms, 6 and 9 */
		/* offset are combined to single entry in the lut, */
		/* reason being, in general the ppr ofset for 6 and 9 */
		/* willbe same, so we shouldnt see any differecen in */
		/* offset between the 2 rates. throw error if they */
		/* differ */
		if (*rate6_offset != val) {
			WL_ERROR(("wl%d: %s: ofst for 6 & 9 is diff\n",
				wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}
	} else if ((rs->rates[idx1] & RATE_MASK) == WLC_RATE_18M) {
		/* ack pwr follows the subbbnd pwr, added a lut in */
		/* ucode to poluate offset for subband case as well */
		/* and to reduce the number of entries/shms, 12 and 18 */
		/* offset are combined to single entry in the lut, */
		/* reason being, in general the ppr ofset for 12 and 18 */
		/* willbe same, so we shouldnt see any differecen in */
		/* offset between the 2 rates. throw error if they */
		/* differ */
		if (*rate12_offset != val) {
			WL_ERROR(("wl%d: %s: ofst for 12 & 18 is diff\n",
				wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}
	}
	return BCME_OK;
}
static void
wlc_stf_compute_ack_pprs(wlc_info_t *wlc)
{
	uint8 idx, idx1, rate;
	uint8 *cck_ptr, *ofdm_ptr;
	uint16 entry_ptr = 0;
	uint8 ofdm_offset = 0;
	uint8 cck_offset = 0;
	uint8 loop_cnt = 0;
	uint16 rate1_offset = 0, rate6_offset = 0, rate12_offset = 0;
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs;
	ppr_dsss_rateset_t dsss_pwrs1[MAX_CORE_IDX];
	ppr_ofdm_rateset_t ofdm_pwrs1[MAX_CORE_IDX];
	BCM_REFERENCE(entry_ptr);
	if (wlc->stf->txpwr_ctl == NULL) {
		WL_ERROR(("wl%d: %s: tx pwr table does not exist.\n",
			wlc->pub->unit, __FUNCTION__));
		ASSERT(0);
		return;
	}

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: No clock\n", wlc->pub->unit, __FUNCTION__));
		ASSERT(0);
		return;
	}

	if (!D11REV_IS(wlc->pub->corerev, 61)) {
		WL_INFORM(("wl%d: %s: For now PPR shmem only supported by PHY for corerev 61\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	rs_dflt = &cck_ofdm_rates;
	wlc_rateset_copy(rs_dflt, &rs);

	idx = (uint8)WLC_BITSCNT(wlc->stf->txcore[CCK_IDX][1]);
	wlc_suspend_mac_and_wait(wlc);

	if (CHSPEC_IS2G(wlc->chanspec)) {
		wlc_stf_ack_ppr_get_ppr_offset_2g(wlc, dsss_pwrs1, ofdm_pwrs1, &loop_cnt);
	} else {
		wlc_stf_ack_ppr_get_ppr_offset_5g(wlc, ofdm_pwrs1, &loop_cnt);
	}
	for (idx = 0; idx < loop_cnt; idx++) {
		cck_ptr = (uint8*)dsss_pwrs1[idx].pwr;
		ofdm_ptr = (uint8*)ofdm_pwrs1[idx].pwr;
		ofdm_offset = 0;
		cck_offset = 0;
		for (idx1 = 0; idx1 < rs.count; idx1++) {
			rate = rs.rates[idx1];
			if (RATE_ISOFDM(rate)) {
				if (wlc_stf_ack_ppr_set_ofdm_ppr(wlc, ofdm_ptr, &rs,
						&ofdm_offset, idx, idx1, &rate6_offset,
						&rate12_offset) == BCME_ERROR) {
					goto enab_mac_return;
				} else {
					ofdm_ptr++;
				}
			} else if (RATE_ISCCK(rate)) {
				if (wlc_stf_ack_ppr_set_cck_ppr(wlc, cck_ptr, &rs,
						&cck_offset, idx, idx1, &rate1_offset) ==
						BCME_ERROR) {
					goto enab_mac_return;
				} else {
					cck_ptr++;
				}
			}
		}
	}
enab_mac_return:
	wlc_enable_mac(wlc);
	return;
}

#ifdef WL_MODESW
void
wlc_stf_op_txrxstreams_complete(wlc_info_t *wlc)
{
	if (wlc->stf->pending_opstreams) {
#ifdef WL_AIR_IQ
		wlc->stf->op_rxstreams = MIN(wlc->stf->pending_opstreams,
			WLC_BITSCNT(wlc->stf->rxchain));
		wlc->stf->op_txstreams = MIN(wlc->stf->pending_opstreams,
			WLC_BITSCNT(wlc->stf->txchain));
#else
		wlc->stf->op_rxstreams = wlc->stf->op_txstreams = wlc->stf->pending_opstreams;
#endif // endif
		wlc->stf->pending_opstreams = 0;
		WL_MODE_SWITCH(("wl%d: %s: op_txstrms set to %d op_rxstrms %d\n", WLCWLUNIT(wlc),
			__FUNCTION__, wlc->stf->op_txstreams, wlc->stf->op_rxstreams));
	}
}

int
wlc_stf_set_optxrxstreams(wlc_info_t *wlc, uint8 new_streams)
{
	uint current_streams;

	if ((new_streams > WLC_BITSCNT(wlc->stf->hw_txchain)) ||
			(new_streams > WLC_BITSCNT(wlc->stf->hw_rxchain))) {
		return BCME_RANGE;
	}

	current_streams = wlc->stf->op_txstreams;
	wlc->stf->pending_opstreams = new_streams;

	/* If packets are enqueued, then wait for it to drain.
	 * Drain only when switching to fewer chains(Downgrade).
	 */
	if (current_streams > new_streams) {
		if (TXPKTPENDTOT(wlc)) {
			wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, DATA_BLOCK_TXCHAIN);
			WL_MODE_SWITCH(("wl%d: %s: BLOCKED DATAFIFO \n", WLCWLUNIT(wlc),
				__FUNCTION__));
			return BCME_OK;
		}
	}

	wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, 0);
	WL_MODE_SWITCH(("wl%d: %s: UNBLOCKED DATAFIFO \n", WLCWLUNIT(wlc),
		__FUNCTION__));
	wlc_stf_op_txrxstreams_complete(wlc);

	return BCME_OK;
}
#endif /* WL_MODESW */

static int
wlc_stf_srcfg_cmd_get_dispatch(wlc_info_t *wlc, sr_config_t *params, char *outbuf, int outlen)
{
	int err = BCME_OK;
	int i;
	const char dis[11][20] = {
		"nsrg_pdmin",
		"nsrg_pdmax",
		"srg_pdmin",
		"srg_pdmax",
		"txpwrref",
		"nsrg_txpwrref0",
		"srg_txpwrref0",
		"srg_obsscolorbmp0",
		"srg_obsscolorbmp1",
		"srg_obsscolorbmp2",
		"srg_obsscolorbmp3",
	};

	bcmstrbuf_t bstr;
	bcm_binit(&bstr, outbuf, outlen);

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: No clock\n", wlc->pub->unit, __FUNCTION__));
		err = BCME_NOCLK;
		goto done;
	}

	wlc_suspend_mac_and_wait(wlc);
	if (!strncmp(params->cmdstr, "display", strlen("display"))) {
		for (i = 0; i < 7; i++) {
			bcm_bprintf(&bstr, "%s : %d dBm\n", dis[i],
					(int16)(wlc_read_shm(wlc, M_D11SR_BLK(wlc) + i*2)));
		}
	} else if (!strncmp(params->cmdstr, "nsrg_pdmin", strlen("nsrg_pdmin"))) {
		bcm_bprintf(&bstr, "nsrg_pdmin %d dBm\n",
				(int16)(wlc_read_shm(wlc, M_D11SR_NSRG_PDMIN(wlc))));
	} else if (!strncmp(params->cmdstr, "nsrg_pdmax", strlen("nsrg_pdmax"))) {
		bcm_bprintf(&bstr, "nsrg_pdmax %d dBm\n",
				(int16)(wlc_read_shm(wlc, M_D11SR_NSRG_PDMAX(wlc))));
	} else if (!strncmp(params->cmdstr, "srg_pdmin", strlen("srg_pdmin"))) {
		bcm_bprintf(&bstr, "srg_pdmin %d dBm\n",
				(int16)(wlc_read_shm(wlc, M_D11SR_SRG_PDMIN(wlc))));
	} else if (!strncmp(params->cmdstr, "srg_pdmax", strlen("srg_pdmax"))) {
		bcm_bprintf(&bstr, "srg_pdmax %d dBm\n",
				(int16)(wlc_read_shm(wlc, M_D11SR_SRG_PDMAX(wlc))));
	} else if (!strncmp(params->cmdstr, "txpwrref", strlen("txpwrref"))) {
		bcm_bprintf(&bstr, "txpwrref %d dBm\n",
				(int16)(wlc_read_shm(wlc, M_D11SR_TXPWRREF(wlc))));
	} else if (!strncmp(params->cmdstr, "nsrg_txpwrref0", strlen("nsrg_txpwrref0"))) {
		bcm_bprintf(&bstr, "nsrg_txpwrref0 %d dBm\n",
				(int16)(wlc_read_shm(wlc, M_D11SR_NSRG_TXPWRREF0(wlc))));
	} else if (!strncmp(params->cmdstr, "srg_txpwrref0", strlen("srg_txpwrref0"))) {
		bcm_bprintf(&bstr, "srg_txpwrref0 %d dBm\n",
				(int16)(wlc_read_shm(wlc, M_D11SR_SRG_TXPWRREF0(wlc))));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp0", strlen("srg_obsscolorbmp0"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP0);
		bcm_bprintf(&bstr, "srg_obsscolorbmp0 %d \n",
				(int16)(R_REG(wlc->osh, D11_PHY_REG_DATA(wlc))));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp1", strlen("srg_obsscolorbmp1"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP1);
		bcm_bprintf(&bstr, "srg_obsscolorbmp1 %d \n",
				(int16)(R_REG(wlc->osh, D11_PHY_REG_DATA(wlc))));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp2", strlen("srg_obsscolorbmp2"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP2);
		bcm_bprintf(&bstr, "srg_obsscolorbmp2 %d \n",
				(int16)(R_REG(wlc->osh, D11_PHY_REG_DATA(wlc))));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp3", strlen("srg_obsscolorbmp3"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP3);
		bcm_bprintf(&bstr, "srg_obsscolorbmp3 %d \n",
				(int16)(R_REG(wlc->osh, D11_PHY_REG_DATA(wlc))));
	} else if (!strncmp(params->cmdstr, "options", strlen("options"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSPDSREN);
		if (R_REG(wlc->osh, D11_PHY_REG_DATA(wlc)) == 0x4) {
			bcm_bprintf(&bstr, "options %d \n", 0);
		} else if (R_REG(wlc->osh, D11_PHY_REG_DATA(wlc)) == 0x5) {
			bcm_bprintf(&bstr, "options %d \n", 1);
		}
	} else {
		err = BCME_NOTFOUND;
	}
	wlc_enable_mac(wlc);

done:
	return err;
}

static int
wlc_stf_srcfg_cmd_set_dispatch(wlc_info_t *wlc, sr_config_t *params)
{
	int err = BCME_OK;

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: No clock\n", wlc->pub->unit, __FUNCTION__));
		err = BCME_NOCLK;
		goto done;
	}

	wlc_suspend_mac_and_wait(wlc);
	if (!strncmp(params->cmdstr, "nsrg_pdmin", strlen("nsrg_pdmin"))) {
		wlc_write_shm(wlc, M_D11SR_NSRG_PDMIN(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "nsrg_pdmax", strlen("nsrg_pdmax"))) {
		wlc_write_shm(wlc, M_D11SR_NSRG_PDMAX(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "srg_pdmin", strlen("srg_pdmin"))) {
		wlc_write_shm(wlc, M_D11SR_SRG_PDMIN(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "srg_pdmax", strlen("srg_pdmax"))) {
		wlc_write_shm(wlc, M_D11SR_SRG_PDMAX(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "txpwrref", strlen("txpwrref"))) {
		wlc_write_shm(wlc, M_D11SR_TXPWRREF(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "nsrg_txpwrref0", strlen("nsrg_txpwrref0"))) {
		wlc_write_shm(wlc, M_D11SR_NSRG_TXPWRREF0(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "srg_txpwrref0", strlen("srg_txpwrref0"))) {
		wlc_write_shm(wlc, M_D11SR_SRG_TXPWRREF0(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp0", strlen("srg_obsscolorbmp0"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP0);
		W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp1", strlen("srg_obsscolorbmp1"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP1);
		W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp2", strlen("srg_obsscolorbmp2"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP2);
		W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "srg_obsscolorbmp3", strlen("srg_obsscolorbmp3"))) {
		W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSCOLORBMP3);
		W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), (int16)(params->val));
	} else if (!strncmp(params->cmdstr, "options", strlen("options"))) {
		if (params->val == 0) {
			if ((wlc_mhf_get(wlc, MHF2, WLC_BAND_AUTO) & MHF2_SR_ENABLE)
					? TRUE : FALSE)
				wlc_mhf(wlc, MHF2, MHF2_SR_ENABLE, 0, WLC_BAND_AUTO);
			W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSPDSREN);
			W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), (uint16)0x4);
		} else if (params->val == 1) {
			if (!((wlc_mhf_get(wlc, MHF2, WLC_BAND_AUTO) & MHF2_SR_ENABLE)
						? TRUE : FALSE))
				wlc_mhf(wlc, MHF2, MHF2_SR_ENABLE, MHF2_SR_ENABLE, WLC_BAND_AUTO);
			W_REG(wlc->osh, D11_PHY_REG_ADDR(wlc), (uint16)PHYREG_OBSSPDSREN);
			W_REG(wlc->osh, D11_PHY_REG_DATA(wlc), (uint16)0x5);
		}
	} else {
		err = BCME_UNSUPPORTED;
	}
	wlc_enable_mac(wlc);

done:
	return err;
}
