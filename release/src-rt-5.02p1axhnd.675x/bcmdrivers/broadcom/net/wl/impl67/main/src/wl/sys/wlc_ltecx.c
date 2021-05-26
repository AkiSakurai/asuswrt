/**
 * @file
 * @brief
 * WLC LTE Coex module API definition
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_ltecx.c 786648 2020-05-04 14:21:00Z $
 */

/**
 * @file
 * @brief
 * LTE Band 40 (TDD) and Band 7 (FDD) have close proximity to WLAN 2.4GHz ISM band. Depending on
 * WLAN and LTE channel, if WLAN and LTE operating frequency are close to each other then this can
 * create mutual interference (since WLAN and MWS* are collocated). This interference leads to
 * degraded performance in both the technologies.
 *
 * The effect of interference can be mitigated by (time) sharing the Air-time between WLAN and LTE
 * Modem. LTE's Tx may impact WLAN Rx and WLAN's Tx may impact LTE's Rx. Hence, the key to good
 * coexistence solution is to avoid simultaneous WLAN_Rx+LTE_Tx or WLAN_Tx+LTE_Rx operations. This
 * is possible by sharing LTE and WLAN medium usage information with each other. The relevant
 * information from LTE Modem to WLAN is LTE frame configuration, upcoming UE (User Equipment) DL
 * (Downlink or Rx)/UL (Uplink or Tx) allocation time, DRX (LTE Inactivity) pattern. WLAN shares
 * WLAN-Priority with LTE. Since LTE has higher priority, WLAN has to use LTE's information and try
 * to fit its Tx/Rx into available LTE free window. LTE may have to defer its Tx/Rx operation for a
 * while when WLAN is in critical situation (indicated by asserting WLAN-Priority), e.g. more than
 * threshold number of BCNs are lost or WLAN Tx/Rx data rate has dropped below threshold, WLAN is in
 * association phase, etc.
 *
 * The Coexistence information between LTE and WLAN can be shared either by ERCX or by UART (BT-SIG
 * or WCI-2) interface.
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <sbgci.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi_channels.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_btcx.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_ap.h>
#include <wlc_stf.h>
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#include <wlc_hw_priv.h>
#ifdef WLC_SW_DIVERSITY
#include <wlc_swdiv.h>
#endif // endif
#include <wlc_ltecx.h>
#include <wlc_lq.h>
#include <phy_ocl_api.h>
#include <phy_noise_api.h>
#include <phy_utils_api.h>
#include <wl_export.h>

#ifdef BCMLTECOEX
enum {
	IOV_LTECX_MWS_COEX_BITMAP,
	IOV_LTECX_MWS_WLANRX_PROT,
	IOV_LTECX_MWS_WLANRXPRI_THRESH,
	IOV_LTECX_WCI2_TXIND,
	IOV_LTECX_WCI2_CONFIG,
	IOV_LTECX_MWS_NOISE_MEAS,
	IOV_LTECX_MWS_PARAMS,
	IOV_LTECX_MWS_COEX_BAUDRATE,
	IOV_LTECX_MWS_SCANJOIN_PROT,
	IOV_LTECX_MWS_LTETX_ADV_PROT,
	IOV_LTECX_MWS_LTERX_PROT,
	IOV_LTECX_MWS_ELNA_RSSI_THRESH,
	IOV_LTECX_MWS_IM3_PROT,
	IOV_LTECX_MWS_WIFI_SENSITIVITY,
	IOV_LTECX_MWS_DEBUG_MSG,
	IOV_LTECX_MWS_DEBUG_MODE,
	IOV_LTECX_WCI2_MSG,
	IOV_LTECX_WCI2_LOOPBACK,
	IOV_LTECX_MWS_FRAME_CONFIG,
	IOV_LTECX_MWS_TSCOEX_BITMAP,
	IOV_LTECX_MWS_ANTMAP,
	IOV_LTECX_MWS_RXBREAK_DIS,
	IOV_LTECX_MWS_CELLSTATUS,
	IOV_LTECX_MWS_SCANREQ_BM,
	IOV_LTECX_MWS_OCLMAP
};

/* LTE coex iovars */
static const bcm_iovar_t ltecx_iovars[] = {
	{"mws_coex_bitmap", IOV_LTECX_MWS_COEX_BITMAP,
	(0), 0, IOVT_UINT16, 0
	},
	{"mws_wlanrx_prot", IOV_LTECX_MWS_WLANRX_PROT,
	(0), 0, IOVT_UINT8, 0
	},
	{"mws_wlanrxpri_thresh", IOV_LTECX_MWS_WLANRXPRI_THRESH,
	(0), 0, IOVT_UINT16, 0
	},
	{"wci2_txind", IOV_LTECX_WCI2_TXIND,
	(0), 0, IOVT_BOOL, 0
	},
	{"mws_noise_meas", IOV_LTECX_MWS_NOISE_MEAS,
	(0), 0, IOVT_UINT16, 0
	},
	{"mws_params", IOV_LTECX_MWS_PARAMS,
	(0), 0,
	IOVT_BUFFER, sizeof(mws_params_t)
	},
	{"wci2_config", IOV_LTECX_WCI2_CONFIG,
	(0), 0,
	IOVT_BUFFER, sizeof(wci2_config_t)
	},
	{"mws_baudrate", IOV_LTECX_MWS_COEX_BAUDRATE,
	(IOVF_SET_DOWN), 0, IOVT_UINT8, 0
	},
	{"mws_scanjoin_prot", IOV_LTECX_MWS_SCANJOIN_PROT,
	(0), 0, IOVT_UINT16, 0
	},
	{"mws_ltetx_adv", IOV_LTECX_MWS_LTETX_ADV_PROT,
	(0), 0, IOVT_UINT16, 0
	},
	{"mws_lterx_prot", IOV_LTECX_MWS_LTERX_PROT,
	(0), 0, IOVT_BOOL, 0
	},
	{"mws_im3_prot", IOV_LTECX_MWS_IM3_PROT,
	(0), 0, IOVT_BOOL, 0
	},
	{"mws_wifi_sensitivity", IOV_LTECX_MWS_WIFI_SENSITIVITY,
	(0), 0, IOVT_INT16, 0
	},
	{"mws_debug_msg", IOV_LTECX_MWS_DEBUG_MSG,
	(IOVF_SET_UP | IOVF_GET_UP), 0,
	IOVT_BUFFER, sizeof(mws_wci2_msg_t)
	},
	{"mws_debug_mode", IOV_LTECX_MWS_DEBUG_MODE,
	(IOVF_SET_UP | IOVF_GET_UP), 0, IOVT_UINT16, 0
	},
	{"mws_elna_rssi_thresh", IOV_LTECX_MWS_ELNA_RSSI_THRESH,
	(0), 0, IOVT_INT16, 0
	},
	{"wci2_msg", IOV_LTECX_WCI2_MSG,
	(IOVF_SET_UP), 0, IOVT_UINT16, 0
	},
	{"wci2_loopback", IOV_LTECX_WCI2_LOOPBACK,
	(IOVF_SET_UP | IOVF_GET_UP), 0,
	IOVT_BUFFER, sizeof(wci2_loopback_t)
	},
	{"mws_antenna_selection", IOV_LTECX_MWS_ANTMAP,
	(0), 0, IOVT_BUFFER, sizeof(mws_ant_map_t)
	},
	{"mws_cellstatus", IOV_LTECX_MWS_CELLSTATUS,
	(0), 0, IOVT_UINT8, 0
	},
	{"mws_frame_config", IOV_LTECX_MWS_FRAME_CONFIG,
	(0), 0,
	IOVT_BUFFER, sizeof(mws_frame_config_t)
	},
	{"mws_tscoex_bitmap", IOV_LTECX_MWS_TSCOEX_BITMAP,
	(0), 0, IOVT_UINT16, 0
	},
	{"mws_rxbreak_dis", IOV_LTECX_MWS_RXBREAK_DIS,
	(0), 0, IOVT_BOOL, 0
	},
	{"mws_scanreq_bm", IOV_LTECX_MWS_SCANREQ_BM,
	(0), 0,
	IOVT_BUFFER, sizeof(mws_scanreq_params_t)
	},
	{"mws_ocl_override", IOV_LTECX_MWS_OCLMAP,
	(0), 0, IOVT_BUFFER, sizeof(wl_mws_ocl_override_t)
	},
	{NULL, 0, 0, 0, 0, 0}
};

#if defined(BCMLTECOEX) || defined(OCL)
static uint16 olympic_5g_chmap[48] = {
	34,  36,  38,  40,  42,  44,  46,  48,  52,  54,  56,  60,  62,  64,  100, 102,
	104, 108, 110, 112, 116, 118, 120, 124, 126, 128, 132, 134, 136, 140, 149, 151,
	153, 157, 159, 161, 165, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static uint16 *
BCMRAMFN(get_olympic_5g_chmap)(void)
{
	return olympic_5g_chmap;
}
#endif // endif

static int wlc_ltecx_doiovar(void *hdl, uint32 actionid,
        void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static void wlc_ltecx_watchdog(void *arg);
static void wlc_ltecx_init_getvar_array(char* vars, const char* name,
	void* dest_array, uint dest_size, ltecx_arr_datatype_t dest_type);
static void wlc_ltecx_handle_joinproc(void *ctx, bss_assoc_state_data_t *evt_data);
static void wlc_ltecx_assoc_in_prog(wlc_ltecx_info_t *ltecx, int val);
static int wlc_mws_lte_ant_errcheck(mws_ant_map_t *antmap);
static void wlc_ltecx_update_coex_iomask(wlc_ltecx_info_t *ltecx);

static const char BCMATTACHDATA(rstr_ltecx)[]                    = "ltecx";
static const char BCMATTACHDATA(rstr_ltecx_rssi_thresh_lmt)[]    = "ltecx_rssi_thresh_lmt";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_mode)[]         = "ltecx_20mhz_mode";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_mode)[]         = "ltecx_10mhz_mode";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2390_rssi_th)[] = "ltecx_20mhz_2390_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2385_rssi_th)[] = "ltecx_20mhz_2385_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2380_rssi_th)[] = "ltecx_20mhz_2380_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2375_rssi_th)[] = "ltecx_20mhz_2375_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2370_rssi_th)[] = "ltecx_20mhz_2370_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2395_rssi_th)[] = "ltecx_10mhz_2395_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2390_rssi_th)[] = "ltecx_10mhz_2390_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2385_rssi_th)[] = "ltecx_10mhz_2385_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2380_rssi_th)[] = "ltecx_10mhz_2380_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2375_rssi_th)[] = "ltecx_10mhz_2375_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2370_rssi_th)[] = "ltecx_10mhz_2370_rssi_th";
static const char BCMATTACHDATA(rstr_ltecxmux)[]                 = "ltecxmux";
static const char BCMATTACHDATA(rstr_ltecxpadnum)[]              = "ltecxpadnum";
static const char BCMATTACHDATA(rstr_ltecxfnsel)[]               = "ltecxfnsel";
static const char BCMATTACHDATA(rstr_ltecxwci2baudrate)[]        = "ltecxwci2baudrate";
static const char BCMATTACHDATA(rstr_ltecxgcigpio)[]             = "ltecxgcigpio";
static const char BCMATTACHDATA(rstr_xtalfreq)[]                 = "xtalfreq";

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_ltecx_info_t *
BCMATTACHFN(wlc_ltecx_attach)(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_ltecx_info_t *ltecx;

	if ((ltecx = (wlc_ltecx_info_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_ltecx_info_t))) != NULL) {
		if ((ltecx->cmn = (wlc_ltecx_cmn_info_t *)MALLOCZ(wlc->osh,
			sizeof(wlc_ltecx_cmn_info_t))) != NULL) {
			ltecx->cmn->mws_scanreq_bms =
				MALLOCZ(wlc->osh, sizeof(mws_scanreq_bms_t));
		}
	}

	if ((ltecx == NULL) || (ltecx->cmn == NULL) || (ltecx->cmn->mws_scanreq_bms == NULL)) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	/* Initialize the back-pointer */
	ltecx->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, ltecx_iovars, rstr_ltecx, ltecx, wlc_ltecx_doiovar,
		wlc_ltecx_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* assoc join-start/done callback */
	if (wlc_bss_assoc_state_register(wlc, (bss_assoc_state_fn_t)wlc_ltecx_handle_joinproc,
		ltecx) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (getvar(wlc_hw->vars, rstr_ltecx_rssi_thresh_lmt) != NULL) {
			ltecx->ltecx_rssi_thresh_lmt_nvram =
				(uint8)getintvar(wlc_hw->vars, rstr_ltecx_rssi_thresh_lmt);
	}
	else {
			ltecx->ltecx_rssi_thresh_lmt_nvram = LTE_RSSI_THRESH_LMT;
	}
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_20mhz_mode,
		ltecx->cmn->ltecx_20mhz_modes, LTECX_NVRAM_PARAM_MAX, C_LTECX_DATA_TYPE_UINT32);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_10mhz_mode,
		ltecx->cmn->ltecx_10mhz_modes, LTECX_NVRAM_PARAM_MAX, C_LTECX_DATA_TYPE_UINT32);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_20mhz_2390_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_20mhz[LTECX_NVRAM_20M_RSSI_2390],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_20mhz_2385_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_20mhz[LTECX_NVRAM_20M_RSSI_2385],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_20mhz_2380_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_20mhz[LTECX_NVRAM_20M_RSSI_2380],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_20mhz_2375_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_20mhz[LTECX_NVRAM_20M_RSSI_2375],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_20mhz_2370_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_20mhz[LTECX_NVRAM_20M_RSSI_2370],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_10mhz_2395_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_10mhz[LTECX_NVRAM_10M_RSSI_2395],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_10mhz_2390_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_10mhz[LTECX_NVRAM_10M_RSSI_2390],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_10mhz_2385_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_10mhz[LTECX_NVRAM_10M_RSSI_2385],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_10mhz_2380_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_10mhz[LTECX_NVRAM_10M_RSSI_2380],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_10mhz_2375_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_10mhz[LTECX_NVRAM_10M_RSSI_2375],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);
	wlc_ltecx_init_getvar_array(wlc_hw->vars, rstr_ltecx_10mhz_2370_rssi_th,
		ltecx->cmn->ltecx_rssi_thresh_10mhz[LTECX_NVRAM_10M_RSSI_2370],
		LTECX_NVRAM_MAX_CHANNELS, C_LTECX_DATA_TYPE_INT16);

	if (getvar(wlc_hw->vars, rstr_ltecxmux) != NULL) {
		ltecx->ltecxmux = (uint32)getintvar(wlc_hw->vars, rstr_ltecxmux);
	}
	if (getvar(wlc_hw->vars, rstr_ltecxpadnum) != NULL) {
		ltecx->ltecxpadnum = (uint32)getintvar(wlc_hw->vars, rstr_ltecxpadnum);
	}
	if (getvar(wlc_hw->vars, rstr_ltecxfnsel) != NULL) {
		ltecx->ltecxfnsel = (uint32)getintvar(wlc_hw->vars, rstr_ltecxfnsel);
	}
	if (getvar(wlc_hw->vars, rstr_ltecxgcigpio) != NULL) {
		ltecx->ltecxgcigpio = (uint32)getintvar(wlc_hw->vars, rstr_ltecxgcigpio);
	}
	if (getvar(wlc_hw->vars, rstr_ltecxwci2baudrate) != NULL) {
		ltecx->baud_rate = (uint32)getintvar(wlc_hw->vars, rstr_ltecxwci2baudrate);
	}
	if (getvar (wlc_hw->vars, rstr_xtalfreq) != NULL) {
		ltecx->xtalfreq = (uint32) getintvar(wlc_hw->vars, rstr_xtalfreq);
	}

	/* LTECX disabled from boardflags or Invalid/missing NVRAM parameter */
	if (((wlc_hw->boardflags & BFL_LTECOEX) == 0) || (ltecx->ltecxpadnum == 0) ||
		(ltecx->ltecxfnsel == 0) || (ltecx->ltecxgcigpio == 0)) {
		wlc->pub->_ltecx = FALSE;
	} else {
		wlc->pub->_ltecx = TRUE;
	}
	wlc->pub->_ltecxgci = (wlc_hw->sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT) &&
	                      MCAP_BTCX_SUPPORTED_WLCHW(wlc_hw;

	ltecx->mws_oclmap.bitmap_2g = MWS_OCLMAP_DEFAULT_2G;
	ltecx->mws_oclmap.bitmap_5g_lo = MWS_OCLMAP_DEFAULT_5G_LO;
	ltecx->mws_oclmap.bitmap_5g_mid = MWS_OCLMAP_DEFAULT_5G_MID;
	ltecx->mws_oclmap.bitmap_5g_high = MWS_OCLMAP_DEFAULT_5G_HIGH;
	ltecx->mws_ltecx_txind = TRUE;
	return ltecx;

fail:
	MODULE_DETACH(ltecx, wlc_ltecx_detach);
	return NULL;
}

static void
wlc_ltecx_init_getvar_array(char* vars, const char* name,
	void* dest_array, uint dest_size, ltecx_arr_datatype_t dest_type)
{
	int i;
	int array_size;

	if (getvar(vars, name) != NULL) {
		array_size = (uint32)getintvararraysize(vars, name);

		/* limit the initialization to the size of the dest array */
		array_size = MIN(array_size, dest_size);

		if (dest_type == C_LTECX_DATA_TYPE_INT16)	{
			/* initialzie the destination array with the intvar values */
			for (i = 0; i < array_size; i++) {
				((int16*)dest_array)[i] = (int16)getintvararray(vars, name, i);
			}
		} else {
			/* initialzie the destination array with the intvar values */
			for (i = 0; i < array_size; i++) {
				((uint32*)dest_array)[i] = (uint32)getintvararray(vars, name, i);
			}
		}
	}
}

void
BCMATTACHFN(wlc_ltecx_detach)(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc;

	if (ltecx == NULL)
		return;

	wlc = ltecx->wlc;
	wlc->pub->_ltecx = FALSE;
	wlc_bss_assoc_state_unregister(wlc, wlc_ltecx_handle_joinproc, ltecx);
	wlc_module_unregister(wlc->pub, rstr_ltecx, ltecx);

	if (ltecx->cmn) {
		if (ltecx->cmn->mws_scanreq_bms) {
			MFREE(wlc->osh,
				ltecx->cmn->mws_scanreq_bms, sizeof(mws_scanreq_bms_t));
		}
		MFREE(ltecx->wlc->osh, ltecx->cmn, sizeof(wlc_ltecx_cmn_info_t));
	}
	MFREE(wlc->osh, ltecx, sizeof(wlc_ltecx_info_t));
}

void
BCMINITFN(wlc_ltecx_init)(wlc_ltecx_info_t *ltecx)
{
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint32 ltecx_mode;
	uint16 ltecx_hflags;
	/* cache the pointer to the LTECX shm block, which won't change after coreinit */
	ltecx->ltecx_shm_addr = 2 * wlc_bmac_read_shm(wlc_hw, M_LTECX_BLK_PTR(wlc_hw));

	/* Enable LTE coex in ucode */
	if (wlc_hw->up && (wlc_hw->reinit == FALSE)) {
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
	}
	/* Clear history */
	ltecx->ltecx_enabled		= 0;
	ltecx->mws_wlanrx_prot_prev	= 0;
	ltecx->mws_lterx_prot_prev	= 0;
	ltecx->ltetx_adv_prev		= 0;
	ltecx->adv_tout_prev		= 0;
	ltecx->scanjoin_prot_prev	= 0;
	ltecx->mws_ltecx_txind_prev	= 0;
	ltecx->mws_wlan_rx_ack_prev	= 0;
	ltecx->mws_rx_aggr_off		= 0;
	ltecx->mws_wifi_sensi_prev	= 0;
	ltecx->mws_im3_prot_prev	= 0;
	ltecx->mws_rxbreak_dis = FALSE;
	ltecx->antmap_in_use.wlan_txmap2g = MWS_ANTMAP_DEFAULT;
	ltecx->antmap_in_use.wlan_txmap5g = MWS_ANTMAP_DEFAULT;
	ltecx->antmap_in_use.wlan_rxmap2g = MWS_ANTMAP_DEFAULT;
	ltecx->antmap_in_use.wlan_rxmap5g = MWS_ANTMAP_DEFAULT;

	/* If INVALID, set baud_rate to default */
	if (ltecx->baud_rate == LTECX_WCI2_INVALID_BAUD) {
		ltecx->baud_rate = LTECX_WCI2_DEFAULT_BAUD;
	}

	wlc_ltecx_update_all_states(ltecx);
	if (wlc_hw->up && (wlc_hw->reinit == FALSE)) {
		wlc_bmac_enable_mac(wlc_hw);
	}

	ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));

	if (BCMLTECOEXGCI_ENAB(ltecx->wlc->pub))
	{
		ltecx_mode = LTECX_EXTRACT_MUX(ltecx->ltecxmux, LTECX_MUX_MODE_IDX);
		if (ltecx_mode == LTECX_MUX_MODE_GPIO) {
			/* Enable LTECX ERCX interface for Core 0 only */
			if (si_coreunit(wlc_hw->sih) == 0) {
				si_ercx_init(wlc_hw->sih, ltecx->ltecxmux,
					ltecx->ltecxpadnum, ltecx->ltecxfnsel, ltecx->ltecxgcigpio);
			}
			ltecx_hflags |= (1 << C_LTECX_HOST_INTERFACE);
		} else {
			/* Enable LTECX WCI-2 UART interface for Core 0 only */
			if (si_coreunit(wlc_hw->sih) == 0) {
				si_wci2_init(wlc_hw->sih, ltecx->baud_rate,
					ltecx->ltecxmux, ltecx->ltecxpadnum,
					ltecx->ltecxfnsel, ltecx->ltecxgcigpio,
					ltecx->xtalfreq);
			}
			if (D11REV_GE(ltecx->wlc->pub->corerev, 56))
			{
				/* Polarity needs to be set for Framesync (bit0) so that an event */
				/* is generated only on pos edge of the event */
				W_REG(wlc_hw->osh, D11_BTCX_ECI_EVENT_ADDR(wlc_hw),
					(BTCX_ECI_EVENT_POLARITY | LTECX_ECI_SLICE_12));
				W_REG(wlc_hw->osh, D11_BTCX_ECI_EVENT_DATA(wlc_hw),
					LTECX_FRAME_SYNC_MASK);
				/* set with Slice 12 for Frame Sync */
				W_REG(wlc_hw->osh, D11_BTCX_ECI_EVENT_ADDR(wlc_hw),
					LTECX_ECI_SLICE_12);
			}
			ltecx_hflags &= ~(1 << C_LTECX_HOST_INTERFACE);
		}
	}
	else {
		/* Enable LTECX ERCX interface for Core 0 only */
		if (si_coreunit(wlc_hw->sih) == 0) {
			si_ercx_init(wlc_hw->sih, ltecx->ltecxmux,
				ltecx->ltecxpadnum, ltecx->ltecxfnsel, ltecx->ltecxgcigpio);
		}
		ltecx_hflags |= (1 << C_LTECX_HOST_INTERFACE);
	}
	wlc_ltecx_update_coex_iomask(ltecx);
	/* Configure Interface in ucode */
	wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecx_hflags);
	if (BCMLTECOEX_ENAB(wlc_hw->wlc->pub))
		wlc_ltecx_interface_active(wlc_hw);
}

static int
wlc_ltecx_doiovar(void *ctx, uint32 actionid,
        void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_ltecx_info_t *ltecx = (wlc_ltecx_info_t *)ctx;
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	int err = 0;
	BCM_REFERENCE(wlc);

	if (p_len >= (int)sizeof(int_val)) {
		bcopy(params, &int_val, sizeof(int_val));
	}

	if (p_len >= (int)sizeof(int_val) * 2) {
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));
	}

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_LTECX_MWS_COEX_BITMAP):
		*ret_int_ptr = (int32) ltecx->ltecx_chmap;
	    break;
	case IOV_SVAL(IOV_LTECX_MWS_COEX_BITMAP):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			uint16 bitmap = int_val;
			ltecx->ltecx_chmap = bitmap;
			/* Enable LTE coex in ucode */
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_check_chmap(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WLANRX_PROT):
		*ret_int_ptr = (int32) ltecx->mws_wlanrx_prot;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WLANRX_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			uint8 val = (uint8) int_val;
			if (val > C_LTECX_MWS_WLANRX_PROT_AUTO) {
				err = BCME_BADARG;
				break;
			}

			ltecx->mws_wlanrx_prot = val;
			/* Enable wlan rx protection in ucode */
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_set_wlanrx_prot(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WLANRXPRI_THRESH):
		if (len >= sizeof(uint16))
			*ret_int_ptr = ltecx->mws_wlanrxpri_thresh;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WLANRXPRI_THRESH):
		if (p_len >= sizeof (uint16))
		{
			uint16 val;
			bcopy((char *)params, (char *)&val, sizeof(uint16));

			if ((uint16)val > 11) {
				err = BCME_BADARG;
				break;
			}
			ltecx->mws_wlanrxpri_thresh = val;
			wlc_ltecx_set_wlanrxpri_thresh(ltecx);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_WCI2_TXIND):
		*ret_int_ptr = (int32) ltecx->mws_ltecx_txind;
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_TXIND):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			bool w = int_val;
			ltecx->mws_ltecx_txind = w;
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltetx_indication(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_WCI2_CONFIG):
		bcopy(&ltecx->cmn->wci2_config, (char *)arg,
		sizeof(wci2_config_t));
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_CONFIG):
		bcopy((char *)params, &ltecx->cmn->wci2_config,
		sizeof(wci2_config_t));
		/* update the WCI2 configuration */
		if (wlc_hw->up) {
			wlc_bmac_suspend_mac_and_wait(wlc_hw);
		}
		wlc_ltecx_update_wci2_config(ltecx);
		if (wlc_hw->up) {
			wlc_bmac_enable_mac(wlc_hw);
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_NOISE_MEAS):
		if (len >= sizeof(uint16))
			*ret_int_ptr = ltecx->mws_noise_meas;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_NOISE_MEAS):
		if (p_len >= sizeof (uint16)) {
			uint16 val;
			bcopy((char *)params, (char *)&val, sizeof(uint16));

			ltecx->mws_noise_meas = (val != 0);
			wlc_ltecx_set_noise_meas(ltecx);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_PARAMS):
		bcopy(&ltecx->cmn->mws_params, (char *)arg,
		sizeof(mws_params_t));
		break;
	case IOV_SVAL(IOV_LTECX_MWS_PARAMS):
		bcopy((char *)params, &ltecx->cmn->mws_params,
		sizeof(mws_params_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_COEX_BAUDRATE):
		*ret_int_ptr = (int32) ltecx->baud_rate;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_COEX_BAUDRATE):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			uint8 baudrate = int_val;
			/* we use 25 for 2.5 baudrate */
			if ((baudrate == 2) || (baudrate == 25) || (baudrate == 3) ||
				(baudrate == 4)) {
				ltecx->baud_rate = baudrate;
			} else {
				err = BCME_BADARG;
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_SCANJOIN_PROT):
		*ret_int_ptr = (int32) ltecx->scanjoin_prot;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_SCANJOIN_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			uint16 w = int_val;
			ltecx->scanjoin_prot = w;
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_scanjoin_prot(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_LTETX_ADV_PROT):
		*ret_int_ptr = (int32) ltecx->ltetx_adv;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_LTETX_ADV_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			uint16 lookahead_dur = int_val;
			ltecx->ltetx_adv = lookahead_dur;
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_update_ltetx_adv(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_LTERX_PROT):
		*ret_int_ptr = (int32) ltecx->mws_lterx_prot;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_LTERX_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			bool w = int_val;
			ltecx->mws_lterx_prot = w;
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_update_lterx_prot(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_ELNA_RSSI_THRESH):
		*ret_int_ptr = ltecx->mws_elna_rssi_thresh;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_ELNA_RSSI_THRESH):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			int16 rssi_thresh = int_val;
			if (rssi_thresh > 0) {
				err = BCME_BADARG;
				break;
			}
			ltecx->mws_elna_rssi_thresh = rssi_thresh;
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_update_wl_rssi_thresh(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_IM3_PROT):
		*ret_int_ptr =  (int32) ltecx->mws_im3_prot;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_IM3_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			bool w = int_val;
			ltecx->mws_im3_prot = w;
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_update_im3_prot(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WIFI_SENSITIVITY):
		*ret_int_ptr = (int32) ltecx->mws_ltecx_wifi_sensitivity;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WIFI_SENSITIVITY):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			int16 wifi_sensitivity = int_val;
			if (wifi_sensitivity > 0) {
				err = BCME_BADARG;
				break;
			}
			ltecx->mws_ltecx_wifi_sensitivity = wifi_sensitivity;
			if (wlc_hw->up) {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			wlc_ltecx_wifi_sensitivity(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}

		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_DEBUG_MSG):
		bcopy(&ltecx->mws_wci2_msg, (char *)arg,
		sizeof(mws_wci2_msg_t));
		break;

	case IOV_SVAL(IOV_LTECX_MWS_DEBUG_MSG):
		bcopy((char *)params, &ltecx->mws_wci2_msg,
		sizeof(mws_wci2_msg_t));

		if (!wlc_hw->up) {
			err = BCME_NOTUP;
			break;
		} else {
			wlc_bmac_suspend_mac_and_wait(wlc_hw);
		}
		wlc_ltecx_update_debug_msg(ltecx);
		if (wlc_hw->up) {
			wlc_bmac_enable_mac(wlc_hw);
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_DEBUG_MODE):
		*ret_int_ptr =  ltecx->mws_debug_mode;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_DEBUG_MODE):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			uint16 w = int_val;
			if (!wlc_hw->up) {
				err = BCME_NOTUP;
				break;
			} else {
				wlc_bmac_suspend_mac_and_wait(wlc_hw);
			}
			ltecx->mws_debug_mode = w;
			wlc_ltecx_update_debug_mode(ltecx);
			if (wlc_hw->up) {
				wlc_bmac_enable_mac(wlc_hw);
			}
		}
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_MSG):
		if (p_len >= sizeof (uint16))	{
			uint16 w;
			bcopy((char *)params, (char *)&w, sizeof(uint16));
			w = w | (1 << LTECX_WCI2_TST_VALID);
			wlc_bmac_write_shm(wlc->hw, M_LTECX_WCI2_TST_MSG(wlc_hw), w);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_WCI2_LOOPBACK):
	{
		wci2_loopback_rsp_t rsp;
		rsp.nbytes_rx = (int)wlc_bmac_read_shm(wlc_hw,
			M_LTECX_WCI2_TST_LPBK_NBYTES_RX(wlc_hw));
		rsp.nbytes_tx = (int)wlc_bmac_read_shm(wlc_hw,
			M_LTECX_WCI2_TST_LPBK_NBYTES_TX(wlc_hw));
		rsp.nbytes_err = (int)wlc_bmac_read_shm(wlc_hw,
			M_LTECX_WCI2_TST_LPBK_NBYTES_ERR(wlc_hw));
		bcopy(&rsp, (char *)arg, sizeof(wci2_loopback_rsp_t));
	}
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_LOOPBACK):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (wlc->pub->associated) {
			err = BCME_ASSOCIATED;
		} else {
			wci2_loopback_t wci2_loopback;
			uint16 w = 0;
			bcopy((char *)params, (char *)(&wci2_loopback), sizeof(wci2_loopback));
			if (wci2_loopback.loopback_type) {
				/* prevent setting of incompatible loopback mode */
				w = wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw));
				if ((w & LTECX_FLAGS_LPBK_MASK) &&
					((w & LTECX_FLAGS_LPBK_MASK) !=
					wci2_loopback.loopback_type)) {
					err = BCME_BADARG;
					break;
				}
				w |= wci2_loopback.loopback_type;
				if (wci2_loopback.loopback_type != LTECX_FLAGS_LPBK_OFF) {
					/* Reuse CRTI code to start test */
					ltecx->mws_debug_mode = 1;
					/* Init counters */
					wlc_bmac_write_shm(wlc_hw,
						M_LTECX_WCI2_TST_LPBK_NBYTES_RX(wlc_hw),
						htol16(0));
					wlc_bmac_write_shm(wlc_hw,
						M_LTECX_WCI2_TST_LPBK_NBYTES_TX(wlc_hw),
						htol16(0));
					wlc_bmac_write_shm(wlc_hw,
						M_LTECX_WCI2_TST_LPBK_NBYTES_ERR(wlc_hw),
						htol16(0));
					wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_MSG(wlc_hw),
						htol16(0));
					/* CRTI_REPEATS=0 presumes Olympic Rx loopback test */
					/* Initialized for Olympic Tx loopback test further below */
					wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_REPEATS(wlc_hw),
						htol16(0));
					/* Suppress scans during lpbk */
					wlc_hw->wlc->scan->state |= SCAN_STATE_SUPPRESS;
				}
				if (wci2_loopback.loopback_type == LTECX_FLAGS_LPBKSRC_MASK) {
					/* Set bit15 of CRTI_MSG to distinguish Olympic Tx
					 * loopback test from RIM test
					 */
					wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_MSG(wlc_hw),
						0x8000 | wci2_loopback.packet);
					wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_INTERVAL(wlc_hw),
						20); /* TODO: hardcoded for now */
					wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_REPEATS(wlc_hw),
						htol16(wci2_loopback.repeat_ct));
				}
			}
			else {
				/* Lpbk disabled. Reenable scans */
				wlc_hw->wlc->scan->state &= ~SCAN_STATE_SUPPRESS;
				/* CRTI ucode used to start test - now stop test */
				ltecx->mws_debug_mode = 0;
			}
			wlc_bmac_write_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw), w);
		}
		break;
	case IOV_SVAL(IOV_LTECX_MWS_ANTMAP):
	    if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else {
			mws_ant_map_t  *antmap_temp_t = (mws_ant_map_t *)params;
			if (wlc_mws_lte_ant_errcheck(antmap_temp_t)) {
				err = BCME_BADARG;
			} else {
				bcopy((char *)params, &ltecx->mws_antmap,
				sizeof(mws_ant_map_t));
			}
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_ANTMAP):
		bcopy(&ltecx->mws_antmap, (char *)arg,
		sizeof(mws_ant_map_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_CELLSTATUS):
		if (!wlc->clk) {
			err = BCME_NOCLK;
		} else {
			int cellstatus, ant_tx;
			cellstatus = (wlc_bmac_read_shm(wlc->hw, M_LTECX_STATE(wlc_hw)) &
				(LTECX_CELLSTATUS_MASK << C_LTECX_ST_LTE_ACTIVE))
				>> C_LTECX_ST_LTE_ACTIVE;

			if (cellstatus & LTECX_CELLSTATUS_UKNOWN)
				cellstatus = LTECX_CELLSTATUS_UKNOWN;
			else
				cellstatus = cellstatus & 0x1;

			ant_tx = (wlc_bmac_read_shm(wlc->hw, M_LTECX_FLAGS(wlc->hw)) &
				(LTECX_CELLANT_TX_MASK << C_LTECX_FLAGS_MWS_TYPE7_CELL_TX_ANT)) >>
				C_LTECX_FLAGS_MWS_TYPE7_CELL_TX_ANT;
			*ret_int_ptr = (((uint8)ant_tx & 0xF) << 4) | ((uint8)cellstatus & 0xF);
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_FRAME_CONFIG):
		bcopy(&ltecx->cmn->mws_frame_config, (char *)arg,
		sizeof(mws_frame_config_t));
		break;
	case IOV_SVAL(IOV_LTECX_MWS_FRAME_CONFIG):
		bcopy((char *)params, &ltecx->cmn->mws_frame_config,
		sizeof(mws_frame_config_t));
		/* update the LTE frame configuration */
		wlc_ltecx_update_frame_config(ltecx);
		break;
	case IOV_GVAL(IOV_LTECX_MWS_TSCOEX_BITMAP):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) ltecx->ltecx_ts_chmap;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_TSCOEX_BITMAP):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint16 ts_bitmap;
			bool ucode_up = FALSE;
			bcopy((char *)params, (char *)&ts_bitmap, sizeof(uint16));

			ltecx->ltecx_ts_chmap = ts_bitmap;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, D11_MACCONTROL(wlc)) &
					MCTL_EN_MAC);

			/* Enable LTE coex in ucode */
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_check_tscoex_chmap(ltecx);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_RXBREAK_DIS):
		if (len >= sizeof(bool))
			*ret_int_ptr = ltecx->mws_rxbreak_dis;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_RXBREAK_DIS):
		ltecx->mws_rxbreak_dis = (int_val != 0) ? TRUE : FALSE;
		break;
#ifdef OCL
	case IOV_SVAL(IOV_LTECX_MWS_OCLMAP):
		if (!(BCMLTECOEX_ENAB(wlc->pub) & OCL_ENAB(wlc->pub))) {
			err = BCME_UNSUPPORTED;
		} else {
			wl_mws_ocl_override_t* ocl_ptr = (wl_mws_ocl_override_t*)params;
			if (ocl_ptr->version != WL_MWS_OCL_OVERRIDE_VERSION) {
				err = BCME_VERSION;
				break;
			}
			bcopy((char *)params, &ltecx->mws_oclmap,
				sizeof(wl_mws_ocl_override_t));
			ltecx->ocl_iovar_set = 1;
		}
		break;
	case IOV_GVAL(IOV_LTECX_MWS_OCLMAP):
		if (!(BCMLTECOEX_ENAB(wlc->pub) & OCL_ENAB(wlc->pub))) {
			err = BCME_UNSUPPORTED;
		} else {
			bcopy(&ltecx->mws_oclmap, (char *)arg,
				sizeof(wl_mws_ocl_override_t));
		}
		break;
		case IOV_SVAL(IOV_LTECX_MWS_SCANREQ_BM):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			mws_scanreq_params_t mws_scanreq_params;
			bcopy((char *)params, (char *)&mws_scanreq_params,
				sizeof(mws_scanreq_params_t));
			wlc_ltecx_update_scanreq_bm(ltecx, &mws_scanreq_params);
			wlc_ltecx_update_scanreq_bm_channel(ltecx, phy_utils_get_chanspec(wlc->pi));
		}
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_SCANREQ_BM):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			mws_scanreq_params_t mws_scanreq_params;
			memset(&mws_scanreq_params, 0, sizeof(mws_scanreq_params_t));
			wlc_ltecx_read_scanreq_bm(ltecx, &mws_scanreq_params, int_val);
			bcopy(&mws_scanreq_params, (char *)arg, sizeof(mws_scanreq_params_t));
		}
		else
			err = BCME_BUFTOOSHORT;
		break;
#endif /* OCL */
	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}

static void
wlc_ltecx_watchdog(void *arg)
{
	wlc_ltecx_info_t *ltecx = (wlc_ltecx_info_t *)arg;
	wlc_info_t *wlc = ltecx->wlc;
	int noise_delta = 0;
	int noise_lte = 0;
	BCM_REFERENCE(wlc);
	int noise = wlc_lq_noise_ma_upd(wlc, phy_noise_avg(WLC_PI(wlc)));

	if (BCMLTECOEX_ENAB(wlc->pub)) {
		wlc_ltecx_update_all_states(ltecx);

		uint16 re_enable_rxaggr_off;
		if (BT3P_HW_COEX(wlc) && CHSPEC_IS2G(wlc->chanspec)) {
			int btc_mode = wlc_btc_mode_get(wlc);
			if (wlc_btc_mode_not_parallel(btc_mode)) {
				/* Make sure STA is on the home channel to avoid changing AMPDU
				 * state during scanning
				 */
				if (AMPDU_ENAB(wlc->pub) && !SCAN_IN_PROGRESS(wlc->scan) &&
					wlc->pub->associated) {
					if (ltecx->mws_lterx_prot) {
						if (wlc_ltecx_get_lte_status(ltecx) &&
							!ltecx->mws_elna_bypass) {
							ltecx->mws_rx_aggr_off = TRUE;
						}
						if (wlc_ltecx_get_lte_status(ltecx)) {
							ltecx->tx_aggr_off = TRUE;
						}
					}
					if (wlc_ltecx_turnoff_rx_aggr(ltecx)) {
						ltecx->mws_rx_aggr_off = TRUE;
					} else {
						if (ltecx->mws_rx_aggr_off) {
							re_enable_rxaggr_off = wlc_bmac_read_shm(
								wlc->hw, M_LTECX_RX_REAGGR(wlc));
							if (!re_enable_rxaggr_off) {
								/* Resume Rx aggregation per
								  * SWWLAN-32809
								  */
								ltecx->mws_rx_aggr_off = FALSE;
							}
						}
					}
					if (wlc_ltecx_turnoff_tx_aggr(ltecx)) {
						ltecx->tx_aggr_off = TRUE;
					} else {
						wlc_ampdu_agg_state_update_tx_all(wlc, ON);
						ltecx->tx_aggr_off = FALSE;
					}
				}
			} else {
				if (wlc_ltecx_turnoff_rx_aggr(ltecx)) {
					ltecx->mws_rx_aggr_off = TRUE;
				} else {
					re_enable_rxaggr_off = wlc_bmac_read_shm(wlc->hw,
						M_LTECX_RX_REAGGR(wlc));
					if (!re_enable_rxaggr_off) {
						/* Resume Rx aggregation per SWWLAN-32809 */
						ltecx->mws_rx_aggr_off = FALSE;
					}
				}
				/* Don't resume tx aggr while LTECX is active */
				if (wlc_ltecx_turnoff_tx_aggr(ltecx)) {
					ltecx->tx_aggr_off = TRUE;
				} else {
					ltecx->tx_aggr_off = FALSE;
				}
			}
		}
		wlc_btcx_ltecx_apply_agg_state(wlc);
		if (wlc->hw->btc->bt_shm_addr) {
			wlc_bmac_write_shm(wlc->hw, M_LTECX_TXNOISE_CNT(wlc),
				LTECX_TXNOISE_CNT);
		}
		/* update noise delta = noise_lte!=0 ? noise_lte - noise : 0 */
		noise_lte = wlc_lq_read_noise_lte(wlc);
		if (noise_lte && noise)
			noise_delta = noise_lte - noise;

		if (wlc->hw->btc->bt_shm_addr) {
			if (WLCISACPHY(wlc->band)) {
				/* For DCF ucode, M_LTECX_NOISE_DELTA location does not exist */
				wlc_bmac_write_shm(wlc->hw, M_LTECX_NOISE_DELTA(wlc),
					noise_delta);
			}
		}
	}
}

void
wlc_ltecx_update_all_states(wlc_ltecx_info_t *ltecx)
{
	/* update ltecx parameters based on nvram and lte freq */
	wlc_ltecx_update_status(ltecx);
	/* enable/disable ltecx based on channel map */
	wlc_ltecx_check_chmap(ltecx);
	/* enable/disable ts coex ltecx based on channel map */
	wlc_ltecx_check_tscoex_chmap(ltecx);
	/* update elna status based on rssi_thresh
	 * First update the elna status and
	 * update the other flags
	 */
	wlc_ltecx_update_wl_rssi_thresh(ltecx);
	/* update protection type */
	wlc_ltecx_set_wlanrx_prot(ltecx);
	/* Allow WLAN TX during LTE TX based on eLNA bypass status */
	wlc_ltecx_update_wlanrx_ack(ltecx);
	/* update look ahead and protection
	 * advance duration
	 */
	wlc_ltecx_update_ltetx_adv(ltecx);
	/* update lterx protection type */
	wlc_ltecx_update_lterx_prot(ltecx);
	/* update im3 protection type */
	wlc_ltecx_update_im3_prot(ltecx);
	/* update scanjoin protection */
	wlc_ltecx_scanjoin_prot(ltecx);
	/* update ltetx indication */
	wlc_ltetx_indication(ltecx);
	/* Check RSSI with WIFI Sensitivity */
	wlc_ltecx_wifi_sensitivity(ltecx);
	wlc_ltecx_update_debug_mode(ltecx);
	/* update the LTE frame configuration */
	wlc_ltecx_update_frame_config(ltecx);
	wlc_ltecx_update_seci_rxbreak(ltecx);
	/* update the WCI2 configuration */
	wlc_ltecx_update_wci2_config(ltecx);
}

void wlc_ltecx_update_seci_rxbreak(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	uint32 sbregCorestatus, rxBreakStatus;
	sbregCorestatus = si_corereg(wlc->hw->sih, GCI_CORE_IDX(wlc->hw->sih),
		GCI_OFFSETOF(wlc->hw->sih, gci_corestat), 0, 0);
	rxBreakStatus = (sbregCorestatus & 1);
	if (rxBreakStatus == 0) {
		si_corereg(wlc->hw->sih, GCI_CORE_IDX(wlc->hw->sih),
			GCI_OFFSETOF(wlc->hw->sih, gci_secilcr), 1 << SECI_ULCR_LBC_POS,
			rxBreakStatus<< SECI_ULCR_LBC_POS);
	}
	else {
		if ((wlc->ltecx->mws_rxbreak_dis == FALSE) &&
			(wlc->ltecx->ltecx_chmap != 0))
			si_corereg(wlc->hw->sih, GCI_CORE_IDX(wlc->hw->sih),
			GCI_OFFSETOF(wlc->hw->sih, gci_secilcr), 1 << SECI_ULCR_LBC_POS,
			rxBreakStatus<< SECI_ULCR_LBC_POS);
	}
}

void
wlc_ltecx_update_frame_config(wlc_ltecx_info_t *ltecx)
{
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	int i = 0;
	uint16 txdur;
	if (!(wlc_hw->up)) {
		return;
	}

	/* Add Framesync Assert Offset to Type6 Prot Adv Time */
	if (ltecx->ltecx_shm_addr) {
		wlc_bmac_write_shm(wlc_hw, M_LTECX_FS_OFFSET(wlc_hw),
			-(ltecx->cmn->mws_frame_config.mws_framesync_assert_offset));

		while (i < LTECX_MAX_NUM_PERIOD_TYPES) {
			if (ltecx->cmn->mws_frame_config.mws_period_type[i] ==
				LTECX_FRAME_UPLINK_TYPE) {
				/* Write the actual LTE tx duration into the shm */
				txdur = ltecx->cmn->mws_frame_config.mws_period_dur[i];
				/* Update the shm incase of a change in the IOVAR */
				if (txdur != ltecx->mws_ltetx_dur_prev) {
					wlc_bmac_write_shm(wlc_hw,
						M_LTECX_ACTUALTX_DURATION(ltecx->wlc),
						ltecx->cmn->mws_frame_config.mws_period_dur[i]);
					ltecx->mws_ltetx_dur_prev = txdur;
				}
			}
			i++;
		}
	}
}

void wlc_ltecx_update_wci2_config(wlc_ltecx_info_t *ltecx)
{
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;

	if (!(wlc_hw->up)) {
		return;
	}
	ltecx->ltetx_adv = -(ltecx->cmn->wci2_config.mws_tx_assert_offset);
	wlc_ltecx_update_ltetx_adv(ltecx);
}

void wlc_ltecx_check_tscoex_chmap(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 ltecx_flags, ltecx_host_flags;
	uint16 ltecx_ts_en;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	ltecx_ts_en = 0;        /* IMP: Initialize to disable */
	/* Decide if ltecx algo needs to be enabled */
	if (CHSPEC_IS2G(wlc->chanspec)) {
		/* enable ltecx algo as per ltecx_chmap */
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if (ltecx->ltecx_ts_chmap & (1 << (chan - 1))) {
			ltecx_ts_en     = 1;
		}
	}

	if (ltecx->ltecx_shm_addr == NULL)
		return;

	if (ltecx->ltecx_shm_addr) {
		ltecx_flags = wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw));
		ltecx_host_flags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
		if (ltecx_ts_en == 1) {
			ltecx_flags |= (1<<C_LTECX_FLAGS_TSCOEX_EN);
			/* radar 21716815: Olympic hack.  */
			/* Implicitly enable wlanrx_prot if tscoex_bitmap is set */
			ltecx_host_flags &= ~(1<<C_LTECX_HOST_RX_ALWAYS);
		}
		else  {
			ltecx_flags &= ~(1<<C_LTECX_FLAGS_TSCOEX_EN);
		}
		wlc_bmac_write_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw), ltecx_flags);
		wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecx_host_flags);
	}
}
void
wlc_ltecx_check_chmap(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 ltecx_hflags, ltecx_state;
	bool ltecx_en;

	if (!si_iscoreup(wlc_hw->sih)) {
		return;
	}

	ltecx_en = 0;	/* IMP: Initialize to disable */
	/* Decide if ltecx algo needs to be enabled */
	if (CHSPEC_IS2G(wlc->chanspec)) {
		/* enable ltecx algo as per ltecx_chmap */
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if (ltecx->ltecx_chmap & (1 << (chan - 1))) {
			ltecx_en	= 1;
		}
	}

	if (ltecx->ltecx_shm_addr == NULL)
		return;

	ltecx_state = wlc_bmac_read_shm(wlc_hw, M_LTECX_STATE(wlc_hw));

	/* Update ucode ltecx flags */
	if (ltecx->ltecx_enabled != ltecx_en)	{
		if (ltecx->ltecx_shm_addr) {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
			if (ltecx_en == 1) {
				ltecx_hflags |= (1<<C_LTECX_HOST_COEX_EN);
			}
			else  {
				ltecx_hflags &= ~(1<<C_LTECX_HOST_COEX_EN);
				ltecx_state  |= (1<<C_LTECX_ST_IDLE);
				wlc_bmac_write_shm(wlc_hw, M_LTECX_STATE(wlc_hw),
					ltecx_state);
			}
			wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw),
				ltecx_hflags);
			ltecx->ltecx_enabled = ltecx_en;
		}
	}
	ltecx->ltecx_idle = (ltecx_state >> C_LTECX_ST_IDLE) & 0x1;
}

void wlc_ltecx_set_wlanrx_prot(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint8 prot;
	uint16 shm_val;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	prot = ltecx->mws_wlanrx_prot;
	/* Set protection to NONE if !2G, or 2G but eLNA bypass */
	if (!CHSPEC_IS2G(wlc->chanspec) || (ltecx->mws_elna_bypass)) {
		prot = C_LTECX_MWS_WLANRX_PROT_NONE;
	}

	if (ltecx->ltecx_shm_addr) {
		shm_val = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
		/* Clear all protection type bits  */
		shm_val = (shm_val &
			~((1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP)
			|(1 << C_LTECX_HOST_PROT_TYPE_PM_CTS)
			|(1 << C_LTECX_HOST_PROT_TYPE_AUTO)));
		/* Set appropriate protection type bit */
		if (prot == C_LTECX_MWS_WLANRX_PROT_NONE) {
			shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
			/* radar 21716815: Olympic hack. */
			/* set RX_ALWAYS if tscoex not enabled for current channel */
			if (BAND_2G(wlc_hw->wlc->band->bandtype)) {
				chanspec_t chan = CHSPEC_CHANNEL(wlc_hw->wlc->chanspec);
				if (!(ltecx->ltecx_ts_chmap & (1 << (chan - 1)))) {
					shm_val |= (1 << C_LTECX_HOST_RX_ALWAYS);
				}
			}
		} else if (prot == C_LTECX_MWS_WLANRX_PROT_CTS) {
			shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_PM_CTS);
		} else if (prot == C_LTECX_MWS_WLANRX_PROT_PM) {
			shm_val = shm_val | (0 << C_LTECX_HOST_PROT_TYPE_PM_CTS);
		} else if (prot == C_LTECX_MWS_WLANRX_PROT_AUTO) {
			shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_AUTO);
		}
		wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), shm_val);
		ltecx->mws_wlanrx_prot_prev = prot;
	}
}

void
wlc_ltecx_update_ltetx_adv(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 adv_tout = 0;
	uint16 prot_type;

	if (!si_iscoreup(wlc_hw->sih)) {
		return;
	}

	/* Update ltetx_adv and adv_tout for 2G band only */
	if (CHSPEC_IS2G(wlc->chanspec))	{
		if (ltecx->ltecx_shm_addr)	{
			if (ltecx->ltetx_adv != ltecx->ltetx_adv_prev)	{
				wlc_bmac_write_shm(wlc_hw, M_LTECX_TX_LOOKAHEAD_DUR(wlc),
					ltecx->ltetx_adv);
				ltecx->ltetx_adv_prev = ltecx->ltetx_adv;
			}
			/* NOTE: C_LTECX_HOST_PROT_TYPE_CTS may be changed by ucode in AUTO mode */
			prot_type = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc));
		    if (prot_type & (1 << C_LTECX_HOST_PROT_TYPE_CTS)) {
				if (ltecx->ltetx_adv >= 500) {
				    adv_tout = ltecx->ltetx_adv - 500;
				} else {
				    adv_tout = ltecx->ltetx_adv;
				}
			} else {
			    if (ltecx->ltetx_adv >= 800) {
					adv_tout = ltecx->ltetx_adv - 800;
				} else {
					adv_tout = ltecx->ltetx_adv;
				}
			}
			if (adv_tout != ltecx->adv_tout_prev)	{
				wlc_bmac_write_shm(wlc_hw, M_LTECX_PROT_ADV_TIME(wlc),
					adv_tout);
				ltecx->adv_tout_prev = adv_tout;
			}
		}
	}
}

void
wlc_ltecx_update_lterx_prot(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 ltecx_hflags;
	bool lterx_prot;

	if (!si_iscoreup (wlc_hw->sih)) {
		return;
	}

	lterx_prot = 0;
	if (CHSPEC_IS2G(wlc->chanspec))	{
		lterx_prot	= ltecx->mws_lterx_prot;
	}
	if (lterx_prot != ltecx->mws_lterx_prot_prev)	{
		if (ltecx->ltecx_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
			if (lterx_prot == 0) {
				ltecx_hflags |= (1 << C_LTECX_HOST_TX_ALWAYS);
			} else {
				ltecx_hflags &= ~(1 << C_LTECX_HOST_TX_ALWAYS);
			}
			wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecx_hflags);
			ltecx->mws_lterx_prot_prev = lterx_prot;
		}
	}
}

void
wlc_ltecx_scanjoin_prot(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 scanjoin_prot, ltecx_hflags;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	scanjoin_prot = 0;
	if (CHSPEC_IS2G(wlc->chanspec)) {
		scanjoin_prot = ltecx->scanjoin_prot;
	}
	if (scanjoin_prot != ltecx->scanjoin_prot_prev)	{
		if (ltecx->ltecx_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
			if (scanjoin_prot == 0)
				ltecx_hflags &= ~(1 << C_LTECX_HOST_SCANJOIN_PROT);
			else
				ltecx_hflags |= (1 << C_LTECX_HOST_SCANJOIN_PROT);
			wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw),
				ltecx_hflags);
			ltecx->scanjoin_prot_prev = scanjoin_prot;
		}
	}
}

void
wlc_ltetx_indication(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 ltecx_hflags;
	bool ltecx_txind;

	if (!si_iscoreup(wlc_hw->sih)) {
		return;
	}

	ltecx_txind = 0;
	if (CHSPEC_IS2G(wlc->chanspec)) {
		ltecx_txind = ltecx->mws_ltecx_txind;
	}
	if (ltecx_txind != ltecx->mws_ltecx_txind_prev)	{
		if (ltecx->ltecx_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
			if (ltecx_txind == 0) {
				ltecx_hflags &= ~(1 << C_LTECX_HOST_TXIND);
			} else {
				ltecx_hflags |= (1 << C_LTECX_HOST_TXIND);
			}
			wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecx_hflags);
			ltecx->mws_ltecx_txind_prev = ltecx_txind;
		}
	}
}

void wlc_ltecx_set_wlanrxpri_thresh(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 thrsh, shm_val;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	thrsh = ltecx->mws_wlanrxpri_thresh;
	shm_val = wlc_bmac_read_shm(wlc_hw, M_LTECX_RXPRI_THRESH(wlc_hw));
	/* Set to NONE if !2G */
	if (!BAND_2G(wlc->band->bandtype)) {
		shm_val = 0;
	}
	else {
		shm_val = thrsh;
	}
	wlc_bmac_write_shm(wlc_hw, M_LTECX_RXPRI_THRESH(wlc_hw), shm_val);
}
void wlc_ltecx_set_noise_meas(wlc_ltecx_info_t *ltecx)
{
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	int ltecx_noise_ms;

	if (!si_iscoreup(wlc_hw->sih)) {
		return;
	}
	ltecx_noise_ms = wlc_bmac_read_shm(wlc_hw, M_BTCX_CONFIG(wlc_hw));
	if (ltecx->mws_noise_meas) {
		ltecx_noise_ms |= (1 << LTECX_DO_NOISE_EST_POS);
	} else {
		ltecx_noise_ms &= ~(1 << LTECX_DO_NOISE_EST_POS);
	}
	wlc_bmac_write_shm(wlc_hw, M_BTCX_CONFIG(wlc_hw), ltecx_noise_ms);
}
static int wlc_mws_lte_ant_errcheck(mws_ant_map_t *antmap)
{
	uint16 ant_map_temp;
	/* Support only use specific antennas in bitmap for DL constraint */
	if (antmap->combo1 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (antmap->combo1 & MWS_ANTMAP_DL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	if (antmap->combo2 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (antmap->combo2 & MWS_ANTMAP_DL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	if (antmap->combo3 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (antmap->combo3 & MWS_ANTMAP_DL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	if (antmap->combo4 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (antmap->combo4 & MWS_ANTMAP_DL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	/* Support only use specific antennas in bitmap for UL constraint */
	if (antmap->combo1 & MWS_ANTMAP_UL_ANTMODE_MASK) {
		if (antmap->combo1 & MWS_ANTMAP_UL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	if (antmap->combo2 & MWS_ANTMAP_UL_ANTMODE_MASK) {
		if (antmap->combo2 & MWS_ANTMAP_UL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	if (antmap->combo3 & MWS_ANTMAP_UL_ANTMODE_MASK) {
		if (antmap->combo3 & MWS_ANTMAP_UL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	if (antmap->combo4 & MWS_ANTMAP_UL_ANTMODE_MASK) {
		if (antmap->combo4 & MWS_ANTMAP_UL_ANTSEL_MASK)
			return BCME_RANGE;
	}
	/* Check the DL antenna bitmap is in correct format */
	if (antmap->combo1 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (((antmap->combo1 & MWS_ANTMAP_DL_CORE0) == 0) ||
			(!(antmap->combo1 & MWS_ANTMAP_DL_CORE1))) {
				return BCME_RANGE;
		}
	}
	if (antmap->combo2 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (((antmap->combo2 & MWS_ANTMAP_DL_CORE0) == 0) ||
			(!(antmap->combo2 & MWS_ANTMAP_DL_CORE1))) {
				return BCME_RANGE;
		}
	}
	if (antmap->combo3 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (((antmap->combo3 & MWS_ANTMAP_DL_CORE0) == 0) ||
			(!(antmap->combo3 & MWS_ANTMAP_DL_CORE1))) {
				return BCME_RANGE;
		}
	}
	if (antmap->combo4 & MWS_ANTMAP_DL_ANTMODE_MASK) {
		if (((antmap->combo4 & MWS_ANTMAP_DL_CORE0) == 0) ||
			(!(antmap->combo4 & MWS_ANTMAP_DL_CORE1))) {
				return BCME_RANGE;
		}
	}
	/* check if TX map is correctly matching to RX map */
	if ((antmap->combo1 & MWS_ANTMAP_UL_ANTMODE_MASK) &&
		(antmap->combo1 & MWS_ANTMAP_DL_ANTMODE_MASK)) {
			if ((antmap->combo1 & MWS_ANTMAP_UL_CORE0) != 0) {
				ant_map_temp = (antmap->combo1 & MWS_ANTMAP_UL_CORE0) >> 7;
				if (ant_map_temp != (antmap->combo1 & MWS_ANTMAP_DL_CORE0))
					return BCME_RANGE;
			}
	}
	if ((antmap->combo2 & MWS_ANTMAP_UL_ANTMODE_MASK) &&
		(antmap->combo2 & MWS_ANTMAP_DL_ANTMODE_MASK)) {
			if ((antmap->combo2 & MWS_ANTMAP_UL_CORE0) != 0) {
				ant_map_temp = (antmap->combo2 & MWS_ANTMAP_UL_CORE0) >> 7;
				if (ant_map_temp != (antmap->combo2 & MWS_ANTMAP_DL_CORE0))
					return BCME_RANGE;
		}
	}
	if ((antmap->combo3 & MWS_ANTMAP_UL_ANTMODE_MASK) &&
		(antmap->combo3 & MWS_ANTMAP_DL_ANTMODE_MASK)) {
			if ((antmap->combo3 & MWS_ANTMAP_UL_CORE0) != 0) {
				ant_map_temp = (antmap->combo3 & MWS_ANTMAP_UL_CORE0) >> 7;
				if (ant_map_temp != (antmap->combo3 & MWS_ANTMAP_DL_CORE0))
					return BCME_RANGE;
		}
	}
	if ((antmap->combo4 & MWS_ANTMAP_UL_ANTMODE_MASK) &&
		(antmap->combo4 & MWS_ANTMAP_DL_ANTMODE_MASK)) {
			if ((antmap->combo4 & MWS_ANTMAP_UL_CORE0) != 0) {
				ant_map_temp = (antmap->combo4 & MWS_ANTMAP_UL_CORE0) >> 7;
				if (ant_map_temp != (antmap->combo4 & MWS_ANTMAP_DL_CORE0))
					return BCME_RANGE;
		}
	}
	return BCME_OK;
}
static mws_wlan_ant_map_t wlc_get_antmap(uint16 antcombo1, uint16 antcombo2)
{
	/* Returns 2G UL Antenna Map in format <A3 A2 A1 A0> */
	mws_wlan_ant_map_t antmap;
	antmap.wlan_txmap2g = (antcombo1 & UL_ANT_MASK_ANT0) >> 10;
	antmap.wlan_txmap2g = antmap.wlan_txmap2g | ((antcombo1 & UL_ANT_MASK_ANT1) >> 8);
	antmap.wlan_txmap2g = antmap.wlan_txmap2g | (antcombo1 & UL_ANT_MASK_ANT2) >> 4;

	/* Returns 2G DL Antenna Map in format */
	antmap.wlan_rxmap2g = (antcombo1 & DL_ANT_MASK_ANT0) >> 3;
	antmap.wlan_rxmap2g = antmap.wlan_rxmap2g | ((antcombo1 & DL_ANT_MASK_ANT1) >> 1);
	antmap.wlan_rxmap2g = antmap.wlan_rxmap2g | ((antcombo1 & DL_ANT_MASK_ANT2) << 3);

	/* Returns 5G UL Antenna Map in format <A3 A2 A1 A0> */
	antmap.wlan_txmap5g = (antcombo2 & UL_ANT_MASK_ANT0) >> 10;
	antmap.wlan_txmap5g = antmap.wlan_txmap5g | ((antcombo2 & UL_ANT_MASK_ANT1) >> 8);
	antmap.wlan_txmap5g = antmap.wlan_txmap5g | (antcombo2 & UL_ANT_MASK_ANT2) >> 4;

	/* Returns 5G DL Antenna Map in format */
	antmap.wlan_rxmap5g = (antcombo2 & DL_ANT_MASK_ANT0) >> 3;
	antmap.wlan_rxmap5g = antmap.wlan_rxmap5g | ((antcombo2 & DL_ANT_MASK_ANT1) >> 1);
	antmap.wlan_rxmap5g = antmap.wlan_rxmap5g | ((antcombo2 & DL_ANT_MASK_ANT2) << 3);

	return antmap;
}

static int wlc_mws_lte_ant(wlc_ltecx_info_t *ltecx, uint16 txantmap2g, uint16 rxantmap2g,
		uint16 txantmap5g, uint16 rxantmap5g)
{
	wlc_info_t *wlc = ltecx->wlc;
	int ret_ant_sel = BCME_OK;
	if (BAND_2G(wlc->band->bandtype)) {
		if ((ret_ant_sel = wlc_stf_mws_set(wlc, txantmap2g, rxantmap2g)) != BCME_OK)
			return ret_ant_sel;
	}
	else {
		if ((ret_ant_sel = wlc_stf_mws_set(wlc, txantmap5g, rxantmap5g)) != BCME_OK)
			return ret_ant_sel;
	}
#ifdef WLC_SW_DIVERSITY
	if (WLSWDIV_ENAB(wlc)) {
		ret_ant_sel = wlc_swdiv_antpref_update(wlc->swdiv, SWDIV_REQ_FROM_LTE,
			rxantmap2g, rxantmap5g, txantmap2g, txantmap5g);
	}
#endif // endif
	return ret_ant_sel;
}

void wlc_ltecx_ant_update(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc;
	wlc_hw_info_t *wlc_hw;
	mws_wlan_ant_map_t antmap;
	int cellstatus, ant_tx, lte_ant_ret;

	if (ltecx == NULL)
		return;

	wlc = ltecx->wlc;
	wlc_hw = ltecx->wlc->hw;

	if (!si_iscoreup(wlc_hw->sih))
		return;
	cellstatus = (wlc_bmac_read_shm(wlc_hw, M_LTECX_STATE(wlc_hw)) &
		(LTECX_CELLSTATUS_MASK << C_LTECX_ST_LTE_ACTIVE)) >> C_LTECX_ST_LTE_ACTIVE;
	ant_tx = (wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw)) &
		(LTECX_CELLANT_TX_MASK << C_LTECX_FLAGS_MWS_TYPE7_CELL_TX_ANT))
		>> C_LTECX_FLAGS_MWS_TYPE7_CELL_TX_ANT;
	if (cellstatus & LTECX_CELLSTATUS_UKNOWN || (!(cellstatus & LTECX_CELLSTATUS_ON))) {
		antmap.wlan_txmap2g =  MWS_ANTMAP_DEFAULT; /* default selection by wifi */
		antmap.wlan_txmap5g =  MWS_ANTMAP_DEFAULT; /* default selection by wifi */
		antmap.wlan_rxmap2g =  MWS_ANTMAP_DEFAULT; /* default selection by wifi */
		antmap.wlan_rxmap5g =  MWS_ANTMAP_DEFAULT; /* default selection by wifi */
	}
	else {
		if (ant_tx == 0) {
			antmap = wlc_get_antmap(ltecx->mws_antmap.combo1, ltecx->mws_antmap.combo3);
		}
		else {
			antmap = wlc_get_antmap(ltecx->mws_antmap.combo2, ltecx->mws_antmap.combo4);
		}
	}
	if ((wlc->ltecx->antmap_in_use.wlan_txmap2g != antmap.wlan_txmap2g) ||
		(wlc->ltecx->antmap_in_use.wlan_rxmap2g != antmap.wlan_rxmap2g) ||
			(wlc->ltecx->antmap_in_use.wlan_txmap5g != antmap.wlan_txmap5g) ||
			(wlc->ltecx->antmap_in_use.wlan_rxmap5g != antmap.wlan_rxmap5g)) {
			wlc->ltecx->antmap_in_use.wlan_txmap2g = antmap.wlan_txmap2g;
			wlc->ltecx->antmap_in_use.wlan_rxmap2g = antmap.wlan_rxmap2g;
			wlc->ltecx->antmap_in_use.wlan_txmap5g = antmap.wlan_txmap5g;
			wlc->ltecx->antmap_in_use.wlan_rxmap5g = antmap.wlan_rxmap5g;
			lte_ant_ret = wlc_mws_lte_ant(ltecx, antmap.wlan_txmap2g,
					antmap.wlan_rxmap2g, antmap.wlan_txmap5g,
					antmap.wlan_rxmap5g);
			if (lte_ant_ret != BCME_OK)
				WL_INFORM(("lte_ant_ret %d\n", lte_ant_ret));
	}
}

bool
wlc_ltecx_get_lte_status(wlc_ltecx_info_t *ltecx)
{
	if (ltecx->ltecx_enabled && !ltecx->ltecx_idle) {
		return TRUE;
	} else {
		return FALSE;
	}
}

#ifdef OCL
static uint16
wlc_ltecx_update_oclmap(uint16 chnum)
{
	int i;
	uint16 ocl_dis_chmap = 0;
	for (i = 0; i < 48; i++) {
		if (get_olympic_5g_chmap()[i] == chnum) {
			if (i <= 15) {
				ocl_dis_chmap = 1 << i;
			} else if (i > 15 && i <= 31) {
				ocl_dis_chmap = 1 << (i-16);
			} else if (i > 31) {
				ocl_dis_chmap = 1 << (i-32);
			}
		}
	}
	return ocl_dis_chmap;
}

static uint16
wlc_ltecx_process_oclmap(wlc_ltecx_info_t *ltecx, wlc_bss_info_t *bss)
{
	uint16 bss_chnum;
	uint16 disable_ocl = 0;
	uint16 channel_disable_map = 0;
	uint16 ch_edge_lo;
	uint16 ch_edge_hi;

	ch_edge_lo = CH_FIRST_20_SB(bss->chanspec);
	ch_edge_hi = CH_LAST_20_SB(bss->chanspec);
	bss_chnum = ch_edge_lo;

	if (CHSPEC_IS2G(bss->chanspec)) {
		while (bss_chnum <= ch_edge_hi) {
			channel_disable_map = 1 << (bss_chnum);
			bss_chnum += CH_10MHZ_APART;
		};
		disable_ocl = channel_disable_map
			& (~ltecx->mws_oclmap.bitmap_2g);
	}
	if (CHSPEC_IS5G(bss->chanspec)) {
		while (!disable_ocl && bss_chnum <= ch_edge_hi) {
			channel_disable_map = wlc_ltecx_update_oclmap(bss_chnum);
			if (bss_chnum <= get_olympic_5g_chmap()[15]) {
				disable_ocl = channel_disable_map
					& (~ltecx->mws_oclmap.bitmap_5g_lo);
			} else if (bss_chnum <= get_olympic_5g_chmap()[31]) {
				disable_ocl = channel_disable_map
					& (~ltecx->mws_oclmap.bitmap_5g_mid);
			} else {
				disable_ocl = channel_disable_map
					& (~ltecx->mws_oclmap.bitmap_5g_high);
			}
			bss_chnum += CH_10MHZ_APART;
		};
	}
	return disable_ocl;
}

void
wlc_lte_ocl_update(wlc_ltecx_info_t *ltecx, int cellstatus)
{
	bool disable;
	uint16 disable_current = 0;

	if ((cellstatus & LTECX_CELLSTATUS_UKNOWN) || (!(cellstatus & LTECX_CELLSTATUS_ON))) {
		disable = 0; /* OCL is decided by wifi */
	} else {
		if (BSSCFG_INFRA_STA(ltecx->wlc->primary_bsscfg)) {
			if (ltecx->wlc->primary_bsscfg->associated) {
				disable_current = wlc_ltecx_process_oclmap(ltecx,
					ltecx->wlc->primary_bsscfg->current_bss);

			}
		}
		disable = (disable_current != 0) ? 1 : 0;
	}
	wlc_stf_ocl_lte(ltecx->wlc, disable);

}

void
wlc_stf_ocl_lte(wlc_info_t *wlc, bool disable)
{
	phy_info_t *pi = WLC_PI(wlc);
	bool bit_present;
	uint16 bits;

	phy_ocl_status_get(pi, &bits, NULL, NULL);

	bit_present = !!(OCL_DISABLED_LTEC  & bits);

	/* if changing state then get stats snapshot */
	if ((disable && !bit_present) || (!disable && bit_present)) {
#if defined(WL_PWRSTATS)
		if (PWRSTATS_ENAB(wlc->pub)) {
			wlc_mimo_siso_metrics_snapshot(wlc,
			    FALSE, WL_MIMOPS_METRICS_SNAPSHOT_OCL);
		}
#endif /* WL_PWRSTATS */
		phy_ocl_disable_req_set(pi, OCL_DISABLED_LTEC,
			disable, WLC_OCL_REQ_LTEC);
	}
}
#endif /* OCL */

bool
wlc_ltecx_turnoff_rx_aggr(wlc_ltecx_info_t *ltecx)
{
	/* Turn Off Rx Aggr if LTECX is active and not in eLNA bypass mode */
	return (wlc_ltecx_get_lte_status(ltecx) &&
		!ltecx->mws_elna_bypass);
}

bool
wlc_ltecx_turnoff_tx_aggr(wlc_ltecx_info_t *ltecx)
{
	return (wlc_ltecx_get_lte_status(ltecx));
}

bool
wlc_ltecx_get_lte_map(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	if (CHSPEC_IS2G(wlc->chanspec) && (BCMLTECOEX_ENAB(wlc->pub))) {
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if ((ltecx->ltecx_enabled) &&
			(ltecx->ltecx_chmap & (1 << (chan - 1)))) {
			return TRUE;
		}
	}
	return FALSE;
}

void
wlc_ltecx_update_wl_rssi_thresh(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	int16 ltecx_rssi_thresh;
	uint8 thresh_lmt;

	if (!si_iscoreup (wlc_hw->sih)) {
		return;
	}

	if (CHSPEC_IS2G(wlc->chanspec)) {
		if (ltecx->ltecx_shm_addr && wlc_ltecx_get_lte_status(wlc->ltecx) &&
			(wlc->primary_bsscfg->link->rssi != WLC_RSSI_INVALID)) {
			ltecx_rssi_thresh = ltecx->mws_elna_rssi_thresh;
			/* mws_ltecx_rssi_thresh_lmt will be zero the very first time
			 * to make the init time decision if we are entering the hysterisis from
			 * lower RSSI side or higher RSSI side (w.r.t. RSSI thresh)
			 */
			thresh_lmt = ltecx->mws_ltecx_rssi_thresh_lmt;
			if (wlc->primary_bsscfg->link->rssi >=
				(int) (ltecx_rssi_thresh + thresh_lmt)) {

				ltecx->mws_elna_bypass = TRUE;
				ltecx->mws_ltecx_rssi_thresh_lmt =
					ltecx->ltecx_rssi_thresh_lmt_nvram;
			} else if (wlc->primary_bsscfg->link->rssi <
				(int) (ltecx_rssi_thresh - thresh_lmt)) {

				ltecx->mws_elna_bypass = FALSE;
				ltecx->mws_ltecx_rssi_thresh_lmt =
					ltecx->ltecx_rssi_thresh_lmt_nvram;
			}
		} else {
			ltecx->mws_elna_bypass = FALSE;
		}
	}
}

void
wlc_ltecx_update_wlanrx_ack(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 ltecx_hflags;
	bool wlan_rx_ack;

	if (!si_iscoreup (wlc_hw->sih)) {
		return;
	}

	wlan_rx_ack = 0;
	if (CHSPEC_IS2G(wlc->chanspec))   {
		wlan_rx_ack  = ltecx->mws_elna_bypass;
	}
	if (wlan_rx_ack != ltecx->mws_wlan_rx_ack_prev)    {
		if (ltecx->ltecx_shm_addr)   {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
			if (wlan_rx_ack == 1) {
				ltecx_hflags |= (1 << C_LTECX_HOST_RX_ACK);
			} else {
				ltecx_hflags &= ~(1 << C_LTECX_HOST_RX_ACK);
			}
			wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecx_hflags);
			ltecx->mws_wlan_rx_ack_prev = wlan_rx_ack;
		}
	}
}

int
wlc_ltecx_chk_elna_bypass_mode(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	if (CHSPEC_IS2G(wlc->chanspec) && (BCMLTECOEX_ENAB(wlc->pub))) {
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if ((ltecx->ltecx_enabled) &&
			(ltecx->ltecx_chmap & (1 << (chan - 1)))) {
			if (ltecx->mws_elna_bypass == TRUE) {
				return 1;
			} else {
				return 0;
			}
		}
	}
	return 0;
}

void
wlc_ltecx_update_status(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	int i, ch, lte_freq_half_bw = 0, freq, coex_chmap = 0;
	uint8 wlanrx_prot_min_ch = 0, lterx_prot_min_ch = 0, scanjoin_prot_min_ch = 0;
	uint8 freq_index = 0, index;
	uint32 wlanrx_prot_info = 0, lterx_prot_info = 0, scan_join_prot_info = 0;
	chanspec_t chan;

	/* No processing required if !2G */
	if (!CHSPEC_IS2G(wlc->chanspec)) {
		return;
	}
	chan = CHSPEC_CHANNEL(wlc->chanspec);

	if (!ltecx->cmn->ltecx_10mhz_modes[LTECX_NVRAM_WLANRX_PROT] &&
		!ltecx->cmn->ltecx_20mhz_modes[LTECX_NVRAM_WLANRX_PROT] &&
		!ltecx->cmn->ltecx_10mhz_modes[LTECX_NVRAM_LTERX_PROT] &&
		!ltecx->cmn->ltecx_20mhz_modes[LTECX_NVRAM_LTERX_PROT]) {
		return;
	}
	if ((ltecx->cmn->mws_params.mws_tx_channel_bw !=
		ltecx->lte_channel_bw_prev)||
		(ltecx->cmn->mws_params.mws_tx_center_freq !=
		ltecx->lte_center_freq_prev)) {
		ltecx->lte_channel_bw_prev =
			ltecx->cmn->mws_params.mws_tx_channel_bw;
		ltecx->lte_center_freq_prev =
			ltecx->cmn->mws_params.mws_tx_center_freq;

		if (ltecx->cmn->mws_params.mws_tx_channel_bw == LTE_CHANNEL_BW_20MHZ)	{
			wlanrx_prot_info =
				ltecx->cmn->ltecx_20mhz_modes[LTECX_NVRAM_WLANRX_PROT];
			lterx_prot_info =
				ltecx->cmn->ltecx_20mhz_modes[LTECX_NVRAM_LTERX_PROT];
			scan_join_prot_info =
				ltecx->cmn->ltecx_20mhz_modes[LTECX_NVRAM_SCANJOIN_PROT];
			lte_freq_half_bw = LTE_20MHZ_INIT_STEP;
		} else if (ltecx->cmn->mws_params.mws_tx_channel_bw == LTE_CHANNEL_BW_10MHZ) {
			wlanrx_prot_info =
				ltecx->cmn->ltecx_10mhz_modes[LTECX_NVRAM_WLANRX_PROT];
			lterx_prot_info =
				ltecx->cmn->ltecx_10mhz_modes[LTECX_NVRAM_LTERX_PROT];
			scan_join_prot_info =
				ltecx->cmn->ltecx_10mhz_modes[LTECX_NVRAM_SCANJOIN_PROT];
			lte_freq_half_bw = LTE_10MHZ_INIT_STEP;
		} else {
			return;
		}
		for (freq = (LTE_BAND40_MAX_FREQ - lte_freq_half_bw);
			freq > LTE_BAND40_MIN_FREQ;
			freq = freq- LTE_FREQ_STEP_SIZE) {
			if ((ltecx->cmn->mws_params.mws_tx_center_freq +
				LTE_MAX_FREQ_DEVIATION) >= freq) {
				break;
			}
			freq_index ++;
		}
		if (freq_index < LTE_FREQ_STEP_MAX) {
			wlanrx_prot_min_ch =  (wlanrx_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
			lterx_prot_min_ch =  (lterx_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
			scanjoin_prot_min_ch =  (scan_join_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
		}
		ltecx->mws_wlanrx_prot_min_ch = wlanrx_prot_min_ch;
		ltecx->mws_lterx_prot_min_ch = lterx_prot_min_ch;
		ltecx->mws_scanjoin_prot_min_ch = scanjoin_prot_min_ch;
		ltecx->mws_lte_freq_index = freq_index;

		ch = (wlanrx_prot_min_ch >= lterx_prot_min_ch)
					? wlanrx_prot_min_ch: lterx_prot_min_ch;
		for (i = 0; i < ch; i++) {
			coex_chmap |= (1<<i);
		}
		/* update coex_bitmap */
		ltecx->ltecx_chmap = coex_chmap;
	}

	wlanrx_prot_min_ch = ltecx->mws_wlanrx_prot_min_ch;
	lterx_prot_min_ch = ltecx->mws_lterx_prot_min_ch;
	scanjoin_prot_min_ch = ltecx->mws_scanjoin_prot_min_ch;

	/* update wlanrx protection */
	if (wlanrx_prot_min_ch && chan <= wlanrx_prot_min_ch) {
		ltecx->mws_wlanrx_prot = 1;
	} else {
		ltecx->mws_wlanrx_prot = 0;
	}
	/* update lterx protection */
	if (lterx_prot_min_ch && chan <= lterx_prot_min_ch) {
		ltecx->mws_lterx_prot = 1;
	} else {
		ltecx->mws_lterx_prot = 0;
	}
	/* update scanjoin protection */
	if (scanjoin_prot_min_ch && chan <= scanjoin_prot_min_ch) {
		ltecx->scanjoin_prot = 1;
	} else {
		ltecx->scanjoin_prot = 0;
	}
	/* update wl rssi threshold */
	index = ltecx->mws_lte_freq_index;
	if (ltecx->lte_channel_bw_prev == LTE_CHANNEL_BW_20MHZ) {
		if (index < LTECX_NVRAM_RSSI_THRESH_20MHZ) {
			ltecx->mws_elna_rssi_thresh =
				ltecx->cmn->ltecx_rssi_thresh_20mhz[index][chan-1];
		} else {
			ltecx->mws_elna_rssi_thresh = 0;
		}
	} else if (ltecx->lte_channel_bw_prev == LTE_CHANNEL_BW_10MHZ) {
		if (index < LTECX_NVRAM_RSSI_THRESH_10MHZ) {
			ltecx->mws_elna_rssi_thresh =
				ltecx->cmn->ltecx_rssi_thresh_10mhz[index][chan-1];
		} else {
			ltecx->mws_elna_rssi_thresh = 0;
		}
	}
}

void
wlc_ltecx_update_im3_prot(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 ltecx_hflags = 0;

	if (!si_iscoreup (wlc_hw->sih)) {
		return;
	}

	if (CHSPEC_IS2G(wlc->chanspec) && (ltecx->ltecx_shm_addr) &&
		(ltecx->mws_im3_prot != ltecx->mws_im3_prot_prev)) {
		ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
		if (ltecx->mws_im3_prot) {
			ltecx_hflags |= (1 << C_LTECX_HOST_PROT_TXRX);
		} else {
			ltecx_hflags &= ~(1 << C_LTECX_HOST_PROT_TXRX);
		}
		wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecx_hflags);
		ltecx->mws_im3_prot_prev = ltecx->mws_im3_prot;
	}
}

void
wlc_ltecx_wifi_sensitivity(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	int ltecx_hflags;
	int16 mws_wifi_sensi = 0;

	if (!si_iscoreup (wlc_hw->sih)) {
		return;
	}

	if (CHSPEC_IS2G(wlc->chanspec) && ltecx->ltecx_shm_addr && ltecx->mws_wlanrx_prot) {
		if (wlc->primary_bsscfg->link->rssi > (int) ltecx->mws_ltecx_wifi_sensitivity) {
			mws_wifi_sensi = 1;
		} else {
			mws_wifi_sensi = 0;
		}

		if (mws_wifi_sensi != ltecx->mws_wifi_sensi_prev) {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
			if (mws_wifi_sensi) {
				ltecx_hflags |= (1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
			} else {
				ltecx_hflags &= ~(1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
			}
			wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecx_hflags);
			ltecx->mws_wifi_sensi_prev = mws_wifi_sensi;
		}
	}
}

void
wlc_ltecx_update_debug_msg(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;

	if (!si_iscoreup(wlc_hw->sih)) {
		return;
	}

	if (CHSPEC_IS2G(wlc->chanspec) && (ltecx->ltecx_shm_addr)) {
		wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_MSG(wlc_hw),
			ltecx->mws_wci2_msg.mws_wci2_data);
		wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_INTERVAL(wlc_hw),
			ltecx->mws_wci2_msg.mws_wci2_interval);
		wlc_bmac_write_shm(wlc_hw, M_LTECX_CRTI_REPEATS(wlc_hw),
			ltecx->mws_wci2_msg.mws_wci2_repeat);
	}
}

#define GCI_INTMASK_RXFIFO_NOTEMPTY 0x4000
#define NO_OF_RETRIES 3
void
wlc_ltecx_update_debug_mode(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	si_t *sih = wlc_hw->sih;
	uint16 ltecx_flags;
	uint no_of_writes = 0;
	if (!si_iscoreup(sih)) {
		return;
	}

	if ((ltecx->ltecx_shm_addr) &&
		(ltecx->mws_debug_mode != ltecx->mws_debug_mode_prev)) {
		ltecx->mws_debug_mode_prev = ltecx->mws_debug_mode;
		ltecx_flags = wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc));
		if (ltecx->mws_debug_mode) {
			/* Disable inbandIntMask for FrmSync, LTE_Rx and LTE_Tx
			  * Note: FrameSync, LTE Rx & LTE Tx happen to share the same REGIDX
			  * Hence a single Access is sufficient
			  */
			si_gci_indirect(sih, GCI_REGIDX(GCI_LTE_FRAMESYNC_POS),
				GCI_OFFSETOF(sih, gci_inbandeventintmask),
				((1 << GCI_BITOFFSET(GCI_LTE_FRAMESYNC_POS))
				|(1 << GCI_BITOFFSET(GCI_LTE_RX_POS))
				|(1 << GCI_BITOFFSET(GCI_LTE_TX_POS))),
				((0 << GCI_BITOFFSET(GCI_LTE_FRAMESYNC_POS))
				|(0 << GCI_BITOFFSET(GCI_LTE_RX_POS))
				|(0 << GCI_BITOFFSET(GCI_LTE_TX_POS))));

			/* Enable Inband interrupt for RX FIFO NON EMPTY bit */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_intmask),
				GCI_INTMASK_RXFIFO_NOTEMPTY, GCI_INTMASK_RXFIFO_NOTEMPTY);
			/* Route Rx-data through RXFIFO */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_rxfifo_common_ctrl),
				ALLONES_32, 0xFF00);
			ltecx_flags |= (1 << C_LTECX_FLAGS_CRTI_DEBUG_MODE);
			/*
			 * Refer to Jira#80092 To avoid FW crush because ucode is not
			 * available during loopback test, we have to bailout watchdog
			 * functionality from here
			 */
			wlc_req_wd_block(wlc, WLC_BIT_SET, WD_BLOCK_REQ_LTECX);
		} else {
			/* Enable inbandIntMask for FrmSync Only; disable LTE_Rx and LTE_Tx
			  * Note: FrameSync, LTE Rx & LTE Tx happen to share the same REGIDX
			  * Hence a single Access is sufficient
			  */
			si_gci_indirect(sih,
				GCI_REGIDX(GCI_LTE_FRAMESYNC_POS),
				GCI_OFFSETOF(sih, gci_inbandeventintmask),
				((1 << GCI_BITOFFSET(GCI_LTE_FRAMESYNC_POS))
				|(1 << GCI_BITOFFSET(GCI_LTE_RX_POS))
				|(1 << GCI_BITOFFSET(GCI_LTE_TX_POS))),
				((1 << GCI_BITOFFSET(GCI_LTE_FRAMESYNC_POS))
				|(0 << GCI_BITOFFSET(GCI_LTE_RX_POS))
				|(0 << GCI_BITOFFSET(GCI_LTE_TX_POS))));
			/* Disable Inband interrupt for RX FIFO NON EMPTY bit */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_intmask),
				GCI_INTMASK_RXFIFO_NOTEMPTY, 0);
			/* Route Rx-data through AUX register */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_rxfifo_common_ctrl),
				ALLONES_32, 0xFF);
			ltecx_flags &= ~(1 << C_LTECX_FLAGS_CRTI_DEBUG_MODE);
			wlc_req_wd_block(wlc, WLC_BIT_CLEAR, WD_BLOCK_REQ_LTECX);
		}
		do {
			wlc_bmac_write_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw), ltecx_flags);
			++no_of_writes; //performing upto 3 re-tries
		} while (((ltecx_flags & (1 << C_LTECX_FLAGS_CRTI_DEBUG_MODE)) !=
				(wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw)) &
				(1 << C_LTECX_FLAGS_CRTI_DEBUG_MODE))) &&
				(no_of_writes < NO_OF_RETRIES));
		if ((ltecx_flags & (1 << C_LTECX_FLAGS_CRTI_DEBUG_MODE)) !=
			(wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw)) &
			(1 << C_LTECX_FLAGS_CRTI_DEBUG_MODE))) {
			wlc_bmac_mctrl(wlc->hw, MCTL_PSM_RUN, 0);
			WLC_FATAL_ERROR(wlc);
		}
	}
}

static void
wlc_ltecx_handle_joinproc(void *ctx, bss_assoc_state_data_t *evt_data)
{
	wlc_ltecx_info_t *ltecx = (wlc_ltecx_info_t *)ctx;

	if (evt_data) {
		if (evt_data->state == AS_JOIN_INIT) {
			wlc_ltecx_assoc_in_prog(ltecx, TRUE);
		} else if (evt_data->state == AS_JOIN_ADOPT) {
			wlc_ltecx_assoc_in_prog(ltecx, FALSE);
		} else if (evt_data->state == AS_IDLE) {
			wlc_ltecx_assoc_in_prog(ltecx, FALSE);
		}
	}
}

static void
wlc_ltecx_assoc_in_prog(wlc_ltecx_info_t *ltecx, int val)
{
	uint16 ltecxHostFlag;
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	if (!wlc_hw || !si_iscoreup(wlc_hw->sih)) {
		return;
	}
	if (CHSPEC_IS2G(wlc->chanspec) && (ltecx->ltecx_shm_addr)) {
		ltecxHostFlag = wlc_bmac_read_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw));
		if (val == TRUE) {
			ltecxHostFlag |= (1 << C_LTECX_HOST_ASSOC_PROG);
		} else {
			ltecxHostFlag &= (~(1 << C_LTECX_HOST_ASSOC_PROG));
		}
		wlc_bmac_write_shm(wlc_hw, M_LTECX_HOST_FLAGS(wlc_hw), ltecxHostFlag);
	}
}

void
wlc_ltecx_update_coex_iomask(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc = ltecx->wlc;
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;

	/* IOMASK available on REV ID 59 and above.
	 * IOMASK by default for ERCX TXCONF and PRISEL signals.
	 */
	if (D11REV_GE(wlc->pub->corerev, 59)) {
		int iomask = ((1 << COEX_IOMASK_WLPRIO_POS) | (1 << COEX_IOMASK_WLTXON_POS));

		ASSERT(wlc_hw->clk);
		/* Unmask coex_io_mask on core0 to 0x3y */
		OR_REG(wlc_hw->osh, D11_COEX_IO_MASK(wlc), iomask);
	}
}

void wlc_ltecx_interface_active(wlc_hw_info_t *wlc_hw)
{
		if (si_iscoreup(wlc_hw->sih) && wlc_hw->btc->bt_shm_addr) {
			uint16 ltecx_flags;
			ltecx_flags = wlc_bmac_read_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw));
			ltecx_flags |= (1<<C_LTECX_FLAGS_WCI2_4TXPWRCAP);
			wlc_bmac_write_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw),
					ltecx_flags);
			}
}
void wlc_ltecx_interface_disable(wlc_hw_info_t *wlc_hw)
{
	if (si_iscoreup(wlc_hw->sih) && wlc_hw->btc->bt_shm_addr) {
		uint16 ltecx_flags;
		ltecx_flags = wlc_bmac_read_shm(wlc_hw,
				M_LTECX_FLAGS(wlc_hw));
		ltecx_flags &= ~(1<<C_LTECX_FLAGS_WCI2_4TXPWRCAP);
		wlc_bmac_write_shm(wlc_hw, M_LTECX_FLAGS(wlc_hw), ltecx_flags);
	}
}

bool wlc_ltecx_rx_agg_off(wlc_info_t *wlc)
{
	if (BCMLTECOEX_ENAB(wlc->pub))
		return wlc->ltecx->mws_rx_aggr_off;
	else
		return FALSE;
}

bool wlc_ltecx_tx_agg_off(wlc_info_t *wlc)
{
	if (BCMLTECOEX_ENAB(wlc->pub))
		return wlc->ltecx->tx_aggr_off;
	else
		return FALSE;
}
void
wlc_ltecx_read_scanreq_bm(wlc_ltecx_info_t *ltecx,
	mws_scanreq_params_t* mws_scanreq_params, int Lte_idx)
{
	mws_scanreq_params->idx = Lte_idx;
	if ((Lte_idx < 1) || (Lte_idx > 31))
		return;
	int elem_i;
	int i;
	for (i = 0; i < 16; i++) {
		elem_i = (ltecx->cmn->mws_scanreq_bms->bm_2g[i] & (1 << Lte_idx)) >> Lte_idx;
		mws_scanreq_params->bm_2g = (mws_scanreq_params->bm_2g | (elem_i << i));
	}
	for (i = 0; i < 16; i++) {
		elem_i = (ltecx->cmn->mws_scanreq_bms->bm_5g_lo[i] & (1 << Lte_idx)) >> Lte_idx;
		mws_scanreq_params->bm_5g_lo = (mws_scanreq_params->bm_5g_lo | (elem_i << i));
	}
	for (i = 0; i < 16; i++) {
		elem_i = (ltecx->cmn->mws_scanreq_bms->bm_5g_mid[i] & (1 << Lte_idx)) >> Lte_idx;
		mws_scanreq_params->bm_5g_mid = (mws_scanreq_params->bm_5g_mid | (elem_i << i));
	}
	for (i = 0; i < 16; i++) {
		elem_i = (ltecx->cmn->mws_scanreq_bms->bm_5g_hi[i] & (1 << Lte_idx)) >> Lte_idx;
		mws_scanreq_params->bm_5g_hi = (mws_scanreq_params->bm_5g_hi | (elem_i << i));
	}
}
void
wlc_ltecx_update_scanreq_bm(wlc_ltecx_info_t *ltecx, mws_scanreq_params_t *scanreq_params)
{
	uint32 *bm_2g = ltecx->cmn->mws_scanreq_bms->bm_2g;
	uint32 *bm_5g_lo = ltecx->cmn->mws_scanreq_bms->bm_5g_lo;
	uint32 *bm_5g_mid = ltecx->cmn->mws_scanreq_bms->bm_5g_mid;
	uint32 *bm_5g_hi = ltecx->cmn->mws_scanreq_bms->bm_5g_hi;
	int idx = scanreq_params->idx;
	int elem_idx_i;
	int i;

	for (i = 0; i < 16; i++) {
		elem_idx_i = (scanreq_params->bm_2g & (1 << i)) >> i;
		bm_2g[i] = (bm_2g[i] & ~(1 << idx)) | (elem_idx_i << idx);
	}
	for (i = 0; i < 16; i++) {
		elem_idx_i = (scanreq_params->bm_5g_lo & (1 << i)) >> i;
		bm_5g_lo[i] = (bm_5g_lo[i] & ~(1 << idx)) | (elem_idx_i << idx);
	}
	for (i = 0; i < 16; i++) {
		elem_idx_i = (scanreq_params->bm_5g_mid & (1 << i)) >> i;
		bm_5g_mid[i] = (bm_5g_mid[i] & ~(1 << idx)) | (elem_idx_i << idx);
	}
	for (i = 0; i < 16; i++) {
		elem_idx_i = (scanreq_params->bm_5g_hi & (1 << i)) >> i;
		bm_5g_hi[i] = (bm_5g_hi[i] & ~(1 << idx)) | (elem_idx_i << idx);
	}
}

void
wlc_ltecx_update_scanreq_bm_channel(wlc_ltecx_info_t *ltecx, chanspec_t chanspec)
{
	wlc_hw_info_t *wlc_hw = ltecx->wlc->hw;
	uint16 ltecx_mws_scan_bm_lo = 0, ltecx_mws_scan_bm_hi = 0;
	uint32 *bm_2g = ltecx->cmn->mws_scanreq_bms->bm_2g;
	uint32 *bm_5g_lo = ltecx->cmn->mws_scanreq_bms->bm_5g_lo;
	uint32 *bm_5g_mid = ltecx->cmn->mws_scanreq_bms->bm_5g_mid;
	uint32 *bm_5g_hi = ltecx->cmn->mws_scanreq_bms->bm_5g_hi;
	int i;
	int chnum = CHSPEC_CHANNEL(chanspec);

	if (!si_iscoreup(wlc_hw->sih)) {
		return;
	}

	if (ltecx->ltecx_shm_addr) {
		if (CHSPEC_IS2G(chanspec)) {
			ltecx_mws_scan_bm_lo = (uint16) (bm_2g[chnum]&0xFFFF);
			ltecx_mws_scan_bm_hi = (uint16) ((bm_2g[chnum]&0xFFFF0000)>>16);
		}
		if (CHSPEC_IS5G(chanspec)) {
			for (i = 0; i < 48; i++) {
				if (get_olympic_5g_chmap()[i] == chnum) {
					if (i < 16) {
						ltecx_mws_scan_bm_lo =
							(uint16) (bm_5g_lo[i]&0xFFFF);
						ltecx_mws_scan_bm_hi =
							(uint16) ((bm_5g_lo[i]&0xFFFF0000)>>16);
					} else if (i >= 15 && i < 32) {
						ltecx_mws_scan_bm_lo =
							(uint16) (bm_5g_mid[i-16]&0xFFFF);
						ltecx_mws_scan_bm_hi =
							(uint16) ((bm_5g_mid[i-16]&0xFFFF0000)>>16);
					} else if (i > 32) {
						ltecx_mws_scan_bm_lo =
							(uint16) (bm_5g_hi[i-32]&0xFFFF);
						ltecx_mws_scan_bm_hi =
							(uint16) ((bm_5g_hi[i-32]&0xFFFF0000)>>16);
					}
				}
			}
		}
		wlc_bmac_write_shm(wlc_hw,
			M_LTECX_MWSSCAN_BM_LO(wlc_hw), ltecx_mws_scan_bm_lo);
		wlc_bmac_write_shm(wlc_hw,
		        M_LTECX_MWSSCAN_BM_HI(wlc_hw), ltecx_mws_scan_bm_hi);
	}
}

#endif /* BCMLTECOEX */
