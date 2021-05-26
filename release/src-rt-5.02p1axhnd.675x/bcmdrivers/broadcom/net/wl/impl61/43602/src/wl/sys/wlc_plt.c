/*
 * Common (OS-independent) portion of
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
 * $Id: wlc_plt.c 440256 2013-12-01 16:26:41Z $:
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
#include <proto/802.1d.h>
#include <proto/802.11.h>
#include <proto/802.11e.h>
#ifdef	BCMCCX
#include <proto/802.11_ccx.h>
#endif	/* BCMCCX */
#include <proto/wpa.h>
#include <proto/vlan.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <wlplt.h>
#include <epivers.h>
#include <proto/eapol.h>
#include <bcmwpa.h>
#include <bcmcrypto/wep.h>
#ifdef BCMCCX
#include <bcmcrypto/ccx.h>
#endif /* BCMCCX */

#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
#include <wlc.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <wlc_led.h>
#include <wlc_frmutil.h>
#include <wlc_security.h>
#include <wlc_amsdu.h>
#include <wlc_ampdu.h>
#include <wlc_event.h>

#ifdef WLDIAG
#include <wlc_diag.h>
#endif /* WLDIAG */

#include <wl_export.h>
#include "d11ucode.h"

#if defined(BCMSUP_PSK) || defined(BCMCCX)
#include <wlc_sup.h>
#endif /* defined(BCMSUP_PSK) || defined(BCMCCX) */

#ifdef BCMSDIO
#include <bcmsdh.h>
#endif /* BCMSDIO */

#ifdef WET
#include <wlc_wet.h>
#endif /* WET */

#if defined(BCMNVRAMW) || defined(WLTEST)
#include <sbchipc.h>
#include <bcmotp.h>
#endif // endif

#ifdef BCMCCMP
#include <bcmcrypto/aes.h>
#endif /* BCMCCMP */
#include <wlc_rm.h>
#ifdef BCMCCX
#include <wlc_ccx.h>
#include <wlc_cac.h>
#endif	/* BCMCCX */

#ifdef BCM_WL_EMULATOR
#include <wl_bcm57emu.h>
#endif // endif

#include <wlc_apcs.h>
#include <wlc_ap.h>

#include <wlc_scan.h>
#include <wlc_plt.h>
#include <wlc_bmac.h>

#if defined(WL_PLT) && defined(WLPLT_ENABLED) && defined(BCMNODOWN)
#error "PLT module can't be enabled when the driver can't be bring up/down as per the commands"
#endif // endif

typedef struct plt_info {
	struct wlc_plt_info	plt_pub;
	wlc_info_t		*wlc;

	/* rx */
	wl_plt_rxper_results_t	rxper_stats_s;
	wl_plt_rxper_results_t	rxper_stats_e;
	bool			rxper_multicast;
	bool			rxper_lostcnt;
	bool			rxper_in_progress;

	/* tx */
	uint8			txmode;
	uint32			plt_tx_status;
	bool			mpc_orig;
	ratespec_t		txrspec;
	int32			txpower_qdBm;
	bool			txper_in_progress;
	uint8			pwrctl_mode;

	/* btcoex test support variables  */
	bool			btcx_testmode;
	uint32			btcx_gpiocontrol_save;
	uint32			btcx_gpioout_save;
	uint32			seci_mode;
	bool			seci;
	bool			ibs_enter;
} plt_info_t;

static int
wlc_plt_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name,	void *params, uint p_len, void *arg,
	int len, int val_size, struct wlc_if *wlcif);

static int
_wlc_plt_doiovar_(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name,	void *params, uint p_len, void *arg,
	int len, int val_size, struct wlc_if *wlcif);

static int wlc_plt_txrate_set(plt_info_t *wlc, wl_plt_srate_t srate);
static int wlc_plt_txpreamble_set(wlc_info_t *wlc, int8 preamble_override, bool htmode);
static void wlc_plt_upd_rxper_stats(plt_info_t *plt_info, bool clear);

static int wlc_plt_prepare(plt_info_t *plt, int8 channel, int8 band, bool bandlock);
static void wlc_plt_done(plt_info_t *plt);
static int wlc_plt_txpower_set(plt_info_t *plt);

/* PLT btcoex test routines */
static int wlc_plt_btcx_enter(wlc_info_t *wlc);
static int wlc_plt_btcx_config(wlc_info_t *wlc, uint32 pins_out);
static int wlc_plt_btcx_state(wlc_info_t *wlc, uint32 *pin_val, bool set);
static int wlc_plt_btcx_exit(wlc_info_t *wlc);
static int wlc_plt_pkteng(plt_info_t *plt_info, wl_pkteng_t *pkteng);

/* IOVar table */

/* Parameter IDs, for use only internally to wlc -- in the wlc_iovars
 * table and by the wlc_doiovar() function.  No ordering is imposed:
 * the table is keyed by name, and the function uses a switch.
 */
enum {
	IOV_PLT_DUMMY,
	IOV_PLT_DEVICE_VER,
	IOV_PLT_INIT,
	IOV_PLT_TX_CONTINUOUS,
	IOV_PLT_TX_CW,
	IOV_PLT_TXPER_START,
	IOV_PLT_TX_STOP,
	IOV_PLT_RXPER_START,
	IOV_PLT_RXPER_STOP,
	IOV_PLT_RXPER_RESULTS,
	IOV_PLT_RX_CW,
	IOV_PLT_RX_RSSI,
	IOV_PLT_MACADDR,
	IOV_PLT_TXPOWER,
	IOV_PLT_TSSI,
	IOV_PLT_BTCX_ENTER,
	IOV_PLT_BTCX_CONFIG,
	IOV_PLT_BTCX_STATE,
	IOV_PLT_BTCX_EXIT,
	IOV_PLT_PMU_CBUCKMODE,
	IOV_PLT_IBS_ENTER,
	IOV_PLT_IBS_EXIT,
	IOV_PLT_SLEEP_CLK,
	IOV_LAST 		/* In case of a need to check max ID number */
};

/* PLT IO Vars */
static const bcm_iovar_t wlc_plt_iovars[] = {
	{"plt_device_ver", IOV_PLT_DEVICE_VER,
	(0), IOVT_BUFFER, 0
	},
	{"plt_init", IOV_PLT_INIT,
	(0), IOVT_VOID, 0
	},
	{"plt_tx_continuous", IOV_PLT_TX_CONTINUOUS,
	(0), IOVT_BUFFER, sizeof(wl_plt_continuous_tx_t)
	},
	{"plt_tx_cw", IOV_PLT_TX_CW,
	(0), IOVT_BUFFER, sizeof(wl_plt_tx_tone_t)
	},
	{"plt_txper_start", IOV_PLT_TXPER_START,
	(0), IOVT_BUFFER, sizeof(wl_plt_txper_start_t)
	},
	{"plt_tx_stop", IOV_PLT_TX_STOP,
	(0), IOVT_VOID, 0
	},
	{"plt_rxper_start", IOV_PLT_RXPER_START,
	(0), IOVT_BUFFER, 0
	},
	{"plt_rxper_stop", IOV_PLT_RXPER_STOP,
	(0), IOVT_VOID, 0
	},
	{"plt_rxper_results", IOV_PLT_RXPER_RESULTS,
	(0), IOVT_BUFFER, 0
	},
	{"plt_rx_cw", IOV_PLT_RX_CW,
	(0), IOVT_BUFFER, sizeof(wl_plt_channel_t)
	},
	{"plt_rx_rssi", IOV_PLT_RX_RSSI,
	(0), IOVT_UINT32, 0
	},
	{"plt_macaddr", IOV_PLT_MACADDR,
	(0), IOVT_BUFFER, 0
	},
	{"plt_txpower", IOV_PLT_TXPOWER,
	(0), IOVT_UINT32, 0
	},
	{"plt_tssi", IOV_PLT_TSSI,
	(0), IOVT_UINT32, 0
	},
	{"plt_btcx_enter", IOV_PLT_BTCX_ENTER,
	(0), IOVT_VOID, 0
	},
	{"plt_btcx_config", IOV_PLT_BTCX_CONFIG,
	(0), IOVT_UINT32, 0
	},
	{"plt_btcx_state", IOV_PLT_BTCX_STATE,
	(0), IOVT_UINT32, 0
	},
	{"plt_btcx_exit", IOV_PLT_BTCX_EXIT,
	(0), IOVT_VOID, 0
	},
	{"plt_pmu_cbuck_mode", IOV_PLT_PMU_CBUCKMODE,
	(0), IOVT_UINT32, 0
	},
	{"plt_ibs_enter", IOV_PLT_IBS_ENTER,
	(0), IOVT_VOID, 0
	},
	{"plt_ibs_exit", IOV_PLT_IBS_EXIT,
	(0), IOVT_VOID, 0
	},
	{"plt_sleep_clk", IOV_PLT_SLEEP_CLK,
	(0), IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0 }
};

#define PLT_TX_CONTINUOUS_IN_PROGRESS	0x00000001
#define PLT_TX_CW_IN_PROGRESS		0x00000002
#define PLT_TX_PER_IN_PROGRESS		0x00000004

#define PLT_TXCTS_IN_PROGRESS(plt_info)	(plt_info->plt_tx_status |=  PLT_TX_CONTINUOUS_IN_PROGRESS)
#define PLT_TXCW_IN_PROGRESS(plt_info)	(plt_info->plt_tx_status |=  PLT_TX_CW_IN_PROGRESS)

#define PLT_TXCTS_STOPPED(plt_info)	(plt_info->plt_tx_status &= ~PLT_TX_CONTINUOUS_IN_PROGRESS)
#define PLT_TXCW_STOPPED(plt_info)	(plt_info->plt_tx_status &=  ~PLT_TX_CW_IN_PROGRESS)

wlc_plt_pub_t *
BCMATTACHFN(wlc_plt_attach)(wlc_info_t *wlc)
{
	plt_info_t *plt_info;
	int	err = 0;

	WL_ERROR(("registering the PLT attach\n"));
	plt_info = (plt_info_t *)MALLOC(wlc->osh, sizeof(plt_info_t));
	if (plt_info != NULL) {
		bzero((char*)plt_info, sizeof(plt_info_t));
		plt_info->wlc = wlc;
	}
	else
		err = BCME_ERROR;

	if (!err) {
		WL_ERROR(("registering the PLT iovars\n"));
		if ((err = wlc_module_register(wlc->pub, wlc_plt_iovars, "wlc_plt_iovars",
		                               plt_info, wlc_plt_doiovar,
		                               NULL, NULL, NULL))) {
			WL_ERROR(("wl%d: %s: wlc_module_register err=%d\n",
				wlc->pub->unit, __FUNCTION__, err));
		}
	}
	if (err && plt_info) {
		MFREE(wlc->osh, plt_info, sizeof(plt_info_t));
		plt_info = NULL;
	}
	if (plt_info) {
		plt_info->pwrctl_mode = PLT_TXPWR_CTRL_CLOSED;

		/* Init Values */
		plt_info->rxper_stats_e.rssi = 0;
		plt_info->rxper_stats_e.snr = plt_info->rxper_stats_e.rssi - PHY_NOISE_FIXED_VAL;
		plt_info->rxper_lostcnt = TRUE;
	}

	return (wlc_plt_pub_t *)plt_info;
}

int
BCMATTACHFN(wlc_plt_detach)(wlc_plt_pub_t *wlc_plt_info)
{
	plt_info_t *plt_info = (plt_info_t *)wlc_plt_info;
	wlc_info_t *wlc;

	if (plt_info) {
		wlc = plt_info->wlc;

		wlc_module_unregister(wlc->pub, "wlc_plt_iovars", plt_info);

		MFREE(wlc->osh, plt_info, sizeof(plt_info_t));
	}
	return 0;
}

#define CTR_DIFF(a, b)	((uint32)(a) - (uint32)(b))	/* properly handles 32-bit wrap-around */

static const char *
_wlc_plt_fwversion(void)
{
	return EPI_VERSION_STR;
}

static int
_wlc_plt_doiovar_(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	plt_info_t *plt_info = (plt_info_t *)hdl;
	wlc_info_t *wlc = plt_info->wlc;
	int bcmerror = 0;

	int32 int_val = 0;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* Do the actual parameter implementation */
	switch (actionid)
	{
	case IOV_GVAL(IOV_PLT_DEVICE_VER):
	{
		wlc_rev_info_t rev_info;
		wl_plt_desc_t *plt_desc = (wl_plt_desc_t *)arg;
		char build_type[4] = "FMAC";

		bzero((char *)plt_desc, sizeof(wl_plt_desc_t));
		bcmerror = wlc_get_revision_info(wlc, (void *)&rev_info,
			(uint)sizeof(wlc_rev_info_t));
		if (!bcmerror) {
			strncpy((char *)plt_desc->build_type, build_type,
			        sizeof(plt_desc->build_type));
			plt_desc->build_type[sizeof(plt_desc->build_type) - 1] = 0;

			strncpy((char *)plt_desc->build_ver, _wlc_plt_fwversion(),
			        sizeof(plt_desc->build_ver));
			plt_desc->build_ver[sizeof(plt_desc->build_ver) - 1] = 0;

			plt_desc->chipnum = rev_info.chipnum;
			plt_desc->chiprev = rev_info.chiprev;
			plt_desc->boardrev = rev_info.boardrev;
			plt_desc->boardid = rev_info.boardid;
			plt_desc->ucoderev = rev_info.ucoderev;
		}
		else {
			WL_ERROR(("Error getting the plt_device_ver: %d\n", bcmerror));
		}
		break;
	}

	case IOV_SVAL(IOV_PLT_INIT):
		wl_down(wlc->wl);
		break;

	case IOV_SVAL(IOV_PLT_TX_CONTINUOUS):
	{
		wl_plt_continuous_tx_t plt_tx;
		wl_pkteng_t pkteng;

		if (p_len < sizeof(wl_plt_continuous_tx_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			WL_ERROR(("IOV_PLT_TX_CONTINUOUS:  expected %d, got %d\n",
				(uint)sizeof(wl_plt_continuous_tx_t), p_len));
			break;
		}
		bcopy(params, &plt_tx, sizeof(wl_plt_continuous_tx_t));
#ifdef BCMDBG_ERR
		WL_ERROR(("plt_tx.band = %d\n", plt_tx.band));
		WL_ERROR(("plt_tx.channel = %d\n", plt_tx.channel));
		WL_ERROR(("plt_tx.preamble = %d\n", plt_tx.preamble));
		WL_ERROR(("plt_tx.power = %d\n", plt_tx.power));
		WL_ERROR(("plt_tx.pwrctl = %d\n", plt_tx.pwrctl));
		WL_ERROR(("plt_tx.carrier_suppress = %d\n", plt_tx.carrier_suppress));
		WL_ERROR(("plt_tx.delay = %d\n", plt_tx.ifs));
		WL_ERROR(("plt_tx_srate: mode %d, igi %d, rate %d, idx %d\n",
			plt_tx.srate.mode, plt_tx.srate.igi,
			plt_tx.srate.tx_rate, plt_tx.srate.m_idx));
#endif /* BCMDBG */

		if (plt_info->plt_tx_status) {
			WL_ERROR(("PLT TX Side test is already in progress,test 0x%x\n",
				plt_info->plt_tx_status));
			bcmerror =  BCME_ERROR;
			break;
		}

		bcmerror = wlc_plt_prepare(plt_info, plt_tx.channel, plt_tx.band, TRUE);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_prepare failed with %d\n", __FUNCTION__, bcmerror));
			break;
		}

		bcmerror = wlc_plt_txrate_set(plt_info, plt_tx.srate);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_txrate_set failed with %d\n",
				__FUNCTION__, bcmerror));
			break;
		}
		bcmerror = wlc_plt_txpreamble_set(wlc, plt_tx.preamble, plt_tx.srate.mode);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_txpreamble_set failed with %d\n",
				__FUNCTION__, bcmerror));
			break;
		}

		plt_info->pwrctl_mode = plt_tx.pwrctl;

		/* The special case (plt_tx.power == 0) means to skip
		 * setting power and leave hardware defaults
		 */
		plt_info->txpower_qdBm = (plt_tx.power * 4);

		bcmerror = wlc_plt_txpower_set(plt_info);
		if (bcmerror)
			break;

		/* set carrier suppress */
		if (RSPEC2RATE(plt_info->txrspec) != WLC_RATE_2M)
			plt_tx.carrier_suppress  = 0;

		bcmerror = wlc_iovar_setint(wlc, "carrier_suppress", plt_tx.carrier_suppress);
		if (bcmerror) {
			WL_ERROR(("iovar carrier_suppress failed: %d\n", bcmerror));
			break;
		}

		WL_ERROR(("Calling the Pktgen engine\n"));
		/* call pkteng phycode */
		bzero(&pkteng, sizeof(wl_pkteng_t));
		pkteng.flags = WL_PKTENG_PER_TX_START;
		pkteng.delay = plt_tx.ifs;
		if (IS_CCK(plt_info->txrspec))
			pkteng.length = 256;
		else
			pkteng.length = 1024;
		bcmerror =  wlc_plt_pkteng(plt_info, &pkteng);
		if (bcmerror) {
			WL_ERROR(("wlc_plt_pkteng failed: %d\n", bcmerror));
			break;
		}

		PLT_TXCTS_IN_PROGRESS(plt_info);
		plt_info->txper_in_progress = TRUE;

		WL_ERROR(("Plt Tx continuous iovar called \n"));
		break;
	}

	case IOV_SVAL(IOV_PLT_TX_CW):
	{
		wl_plt_tx_tone_t plt_tx_cw;

		if (p_len < sizeof(wl_plt_tx_tone_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			WL_ERROR(("IOV_PLT_TX_CW:  expected %d, got %d\n",
				(uint)sizeof(wl_plt_tx_tone_t), p_len));
			break;
		}
		bcopy(params, &plt_tx_cw, sizeof(wl_plt_tx_tone_t));
		if (plt_info->plt_tx_status) {
			WL_ERROR(("PLT TX Side test is already in progress,test 0x%x\n",
				plt_info->plt_tx_status));
			bcmerror =  BCME_ERROR;
			break;
		}

#ifdef BCMDBG_ERR
		WL_ERROR(("tx:cw band = %d, channel %d, scarrier %d\n", plt_tx_cw.plt_channel.band,
			plt_tx_cw.plt_channel.channel, plt_tx_cw.sub_carrier_idx));
#endif /* BCMDBG */

		bcmerror = wlc_plt_prepare(plt_info, plt_tx_cw.plt_channel.channel,
			plt_tx_cw.plt_channel.band, TRUE);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_prepare failed with %d\n", __FUNCTION__, bcmerror));
			break;
		}

		/* out command now */
		/* send the WLC_FREQ_ACCURACY ioctl to phycode */

		/* SSLPNCONF transmits a few frames before running PAPD Calibration */
		/* SSLPNCONF does papd calibration each time it enters a new channel */
		/* We cannot be down for this reason */
#ifndef BCMNODOWN
		if (WLCISSSLPNPHY(wlc->band) || WLCISLCNPHY(wlc->band))
			wlc_out(wlc);
#endif /* BCMNODOWN */
		/* need to use different iovar to get tone on centre vs sub carrier */
		if (plt_tx_cw.sub_carrier_idx == 0) {
			bool ta_ok = FALSE;
			int32 fqchan = plt_tx_cw.plt_channel.channel;
			bcmerror = wlc_phy_ioctl(wlc->band->pi, WLC_FREQ_ACCURACY, sizeof(uint32),
				&fqchan, &ta_ok);
			if (bcmerror) {
				WL_ERROR(("%s: wlc_phy_ioctl failed with %d\n",
					__FUNCTION__, bcmerror));
				break;
			}
		}
		else {
			int32 freq;
			int8 idx;

			idx = plt_tx_cw.sub_carrier_idx;
			if (idx <= WLPLT_SUBCARRIER_LEFT_OF_CENTER)
				freq = -(WLPLT_SUBCARRIER_FREQ_SPACING * idx);
			else {
				freq = (WLPLT_SUBCARRIER_FREQ_SPACING *
					(idx - WLPLT_SUBCARRIER_LEFT_OF_CENTER));
			}

			bcmerror = wlc_iovar_setint(wlc, "phy_tx_tone_hz", freq);
			if (bcmerror) {
				WL_ERROR(("%s: phy_tx_tone_hz iovar failed with %d\n",
					__FUNCTION__, bcmerror));
				break;
			}
		}
		PLT_TXCW_IN_PROGRESS(plt_info);

		WL_ERROR(("Plt Tx Carrier Wave iovar called \n"));
		break;
	}

	case IOV_SVAL(IOV_PLT_TXPER_START):
	{
		wl_plt_txper_start_t plt_tx;
		wl_pkteng_t pkteng;

		if (p_len < sizeof(wl_plt_txper_start_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			WL_ERROR(("IOV_PLT_TXPER_START:  expected %d, got %d\n",
				(uint)sizeof(wl_plt_txper_start_t), p_len));
			break;
		}
		bcopy(params, &plt_tx, sizeof(wl_plt_txper_start_t));
#ifdef BCMDBG_ERR
		WL_ERROR(("plt_tx.band = %d\n", plt_tx.band));
		WL_ERROR(("plt_tx.channel = %d\n", plt_tx.channel));
		WL_ERROR(("plt_tx.preamble = %d\n", plt_tx.preamble));
		WL_ERROR(("plt_tx.length = %d\n", plt_tx.length));
		WL_ERROR(("plt_tx.nframes = %d\n", plt_tx.nframes));
		WL_ERROR(("plt_tx.pwrctl = %d\n", plt_tx.pwrctl));
		WL_ERROR(("plt_tx.seq_ctl = %d\n", plt_tx.seq_ctl));
		WL_ERROR(("plt_tx_srate: mode %d, igi %d, rate %d, idx %d\n",
			plt_tx.srate.mode, plt_tx.srate.igi,
			plt_tx.srate.tx_rate, plt_tx.srate.m_idx));
		{
			char macbuf[64];
			WL_ERROR(("plt_tx:src_mac %s\n",
				bcm_ether_ntoa((struct ether_addr *)&plt_tx.src_mac, macbuf)));
			WL_ERROR(("plt_tx:dest_mac %s\n",
				bcm_ether_ntoa((struct ether_addr *)&plt_tx.dest_mac, macbuf)));
		}
#endif /* BCMDBG */
		/* if CW Tests are running already, user has to stop them first */
		if (plt_info->plt_tx_status) {
			WL_ERROR(("PLT TX Side test is already in progress,test 0x%x\n",
				plt_info->plt_tx_status));
			bcmerror =  BCME_ERROR;
			break;
		}

		bcmerror = wlc_plt_prepare(plt_info, plt_tx.channel, plt_tx.band, TRUE);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_prepare failed with %d\n", __FUNCTION__, bcmerror));
			break;
		}
		/* set rate override, preamble override */
		bcmerror = wlc_plt_txrate_set(plt_info, plt_tx.srate);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_txrate_set failed with %d\n",
				__FUNCTION__, bcmerror));
			break;
		}
		bcmerror = wlc_plt_txpreamble_set(wlc, plt_tx.preamble, plt_tx.srate.mode);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_txpreamble_set failed with %d\n",
				__FUNCTION__, bcmerror));
			break;
		}

		plt_info->pwrctl_mode = plt_tx.pwrctl;

		bcmerror = wlc_plt_txpower_set(plt_info);
		if (bcmerror)
			break;

		/* call pkteng phycode */
		bzero(&pkteng, sizeof(wl_pkteng_t));
		pkteng.flags = WL_PKTENG_PER_TX_START;
		pkteng.delay = 30;
		pkteng.nframes = plt_tx.nframes;
		pkteng.length = plt_tx.length;
		if (plt_tx.seq_ctl)
			pkteng.seqno = TRUE;
		bcopy(&plt_tx.dest_mac[0], &pkteng.dest, ETHER_ADDR_LEN);
		bcopy(&plt_tx.src_mac[0], &pkteng.src, ETHER_ADDR_LEN);

		bcmerror =  wlc_plt_pkteng(plt_info, &pkteng);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_pkteng failed with %d\n", __FUNCTION__, bcmerror));
			break;
		}
		plt_info->txper_in_progress = TRUE;

		break;
	}

	case IOV_SVAL(IOV_PLT_TX_STOP):
	{
		wl_pkteng_t pkteng;
		/* stop the current tx test running */
		/* call phyiovars to stop the pkteng code */
		WL_ERROR(("Plt Tx Stop iovar called \n"));
		if (!(plt_info->plt_tx_status || plt_info->txper_in_progress)) {
			WL_ERROR(("No TX test is running now, so no need to stop anything :-)\n"));
			break;
		}

		if (plt_info->txper_in_progress) {
			WL_ERROR(("Stopping the TXPER Test\n"));
			/* check if they pkteng is active and stop it if needed */
			bzero(&pkteng, sizeof(wl_pkteng_t));
			pkteng.flags = WL_PKTENG_PER_TX_STOP;
			bcmerror =  wlc_plt_pkteng(plt_info, &pkteng);
			if (bcmerror)
				break;
		}
		WL_ERROR(("plt_tx_status is 0x%x\n", plt_info->plt_tx_status));
		if (!plt_info->plt_tx_status)
			break;

		/* Only one test would be in pending, so clear all the flags */
		if (plt_info->plt_tx_status & PLT_TX_CW_IN_PROGRESS) {
			bool ta_ok = FALSE;
			int32 fqchan = 0;
			bcmerror = wlc_phy_ioctl(wlc->band->pi, WLC_FREQ_ACCURACY, sizeof(uint32),
				&fqchan, &ta_ok);
			if (bcmerror) {
				WL_ERROR(("%s: %d: wlc_phy_ioctl failed with %d\n",
					__FUNCTION__, bcmerror, __LINE__));
			}
		}
		plt_info->plt_tx_status = 0;
		wlc_plt_done(plt_info);
		break;
	}

	case IOV_SVAL(IOV_PLT_RXPER_START):
	{
		wl_plt_rxper_start_t plt_rx;
		wl_pkteng_t pkteng;

		if (p_len < sizeof(wl_plt_rxper_start_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			WL_ERROR(("IOV_PLT_RXPER_START:  expected %d, got %d\n",
				(uint)sizeof(wl_plt_rxper_start_t), p_len));
			break;
		}
		bcopy(params, &plt_rx, sizeof(wl_plt_rxper_start_t));
#ifdef BCMDBG_ERR
		WL_ERROR(("plt_rx.band = %d\n", plt_rx.band));
		WL_ERROR(("plt_rx.channel = %d\n", plt_rx.channel));
		WL_ERROR(("plt_rx.seq_ctl = %d\n", plt_rx.seq_ctl));
		{
			char macbuf[64];
			WL_ERROR(("plt_rx:dest_mac %s\n",
				bcm_ether_ntoa((struct ether_addr *)&plt_rx.dst_mac, macbuf)));
		}
#endif /* BCMDBG */

		bcmerror = wlc_plt_prepare(plt_info, plt_rx.channel, plt_rx.band, TRUE);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_prepare failed with %d\n", __FUNCTION__, bcmerror));
			break;
		}

		/* clear the stats */
		wlc_plt_upd_rxper_stats(plt_info, TRUE);

		bzero(&pkteng, sizeof(wl_pkteng_t));
		pkteng.flags = WL_PKTENG_PER_RX_START;

		if (plt_rx.dst_mac[0] & 0x01) {
			plt_info->rxper_multicast = TRUE;
			/* default, M/B cast pkts are recd by driver, so don't change macaddr */
			bcopy(&wlc->cfg->cur_etheraddr, &pkteng.dest, ETHER_ADDR_LEN);
		}
		else {
			bcopy(&plt_rx.dst_mac[0], &pkteng.dest, ETHER_ADDR_LEN);
			plt_info->rxper_multicast = FALSE;
		}
		if (plt_rx.seq_ctl) {
			pkteng.seqno = TRUE;
			plt_info->rxper_lostcnt = TRUE;
		}
		else
			plt_info->rxper_lostcnt = FALSE;

		/* call pkteng phycode */
		bcmerror =  wlc_plt_pkteng(plt_info, &pkteng);
		if (bcmerror) {
			WL_ERROR(("%s: wlc_plt_pkteng failed with %d\n", __FUNCTION__, bcmerror));
			break;
		}
		plt_info->rxper_in_progress = TRUE;

		WL_ERROR(("Plt Rx PER Start iovar called \n"));

		break;
	}

	case IOV_SVAL(IOV_PLT_RXPER_STOP):
	{
		wl_pkteng_t pkteng;
		/* call phyiovars to stop the pkteng code */

		WL_ERROR(("Plt Rx PER Stop iovar called \n"));

		if (!plt_info->rxper_in_progress) {
			WL_ERROR(("rxper is not in progress\n"));
			break;
		}

		bzero(&pkteng, sizeof(wl_pkteng_t));
		pkteng.flags = WL_PKTENG_PER_RX_STOP;
		bcmerror =  wlc_plt_pkteng(plt_info, &pkteng);
		if (bcmerror)
			WL_ERROR(("%s: wlc_plt_pkteng failed with %d\n", __FUNCTION__, bcmerror));

		wlc_plt_upd_rxper_stats(plt_info, FALSE);

		plt_info->rxper_in_progress = FALSE;

		wlc_plt_done(plt_info);
		break;
	}

	case IOV_SVAL(IOV_PLT_RXPER_RESULTS):
	{
		/* clear the results */
		WL_ERROR(("Plt Rx PER results clear iovar called \n"));

		if (!wlc->pub->up) {
			bcopy(&plt_info->rxper_stats_e, &plt_info->rxper_stats_s,
				sizeof(wl_plt_rxper_results_t));
			plt_info->rxper_stats_e.rssi = 0;
			plt_info->rxper_stats_e.snr = plt_info->rxper_stats_e.rssi -
				PHY_NOISE_FIXED_VAL;
		}
		else
			wlc_plt_upd_rxper_stats(plt_info, TRUE);

		break;
	}

	case IOV_GVAL(IOV_PLT_RXPER_RESULTS):
	{
		wl_plt_rxper_results_t rxper_e;

		bzero(&rxper_e, sizeof(wl_plt_rxper_results_t));

		WL_ERROR(("Plt Rx PER results get iovar called \n"));

		if (wlc->pub->up && plt_info->rxper_in_progress)
			wlc_plt_upd_rxper_stats(plt_info, FALSE);

		rxper_e.frames = CTR_DIFF(plt_info->rxper_stats_e.frames,
			plt_info->rxper_stats_s.frames);
		if (!plt_info->rxper_lostcnt)
			rxper_e.lost_frames = 0;
		else
			rxper_e.lost_frames = CTR_DIFF(plt_info->rxper_stats_e.lost_frames,
				plt_info->rxper_stats_s.lost_frames);
		rxper_e.fcs_errs = CTR_DIFF(plt_info->rxper_stats_e.fcs_errs,
			plt_info->rxper_stats_s.fcs_errs);
		rxper_e.plcp_errs = CTR_DIFF(plt_info->rxper_stats_e.plcp_errs,
			plt_info->rxper_stats_s.plcp_errs);
		rxper_e.rssi = plt_info->rxper_stats_e.rssi;
		rxper_e.snr = plt_info->rxper_stats_e.snr;

		bcopy(&rxper_e, arg, sizeof(wl_plt_rxper_results_t));

#ifdef BCMDBG_ERR
		WL_ERROR(("frames: %d\n", rxper_e.frames));
		WL_ERROR(("lost_frames: %d\n", rxper_e.lost_frames));
		WL_ERROR(("fcs_errors: %d\n", rxper_e.fcs_errs));
		WL_ERROR(("plcp_errors: %d\n", rxper_e.plcp_errs));
		WL_ERROR(("snr: %d\n", rxper_e.snr));
		WL_ERROR(("rssi: %d\n", rxper_e.rssi));
#endif /* BCMDBG_ERR */
		break;
	}

	case IOV_SVAL(IOV_PLT_RX_CW):
	{
		/* make sure we are in up state */
		wl_plt_channel_t plt_chan;

		WL_ERROR(("Plt RX CW Set iovar called \n"));

		if (p_len < sizeof(wl_plt_channel_t)) {
			bcmerror = BCME_BUFTOOSHORT;
			WL_ERROR(("IOV_PLT_RX_CW:  expected %d, got %d\n",
				(uint)sizeof(wl_plt_channel_t), p_len));
			break;
		}
		bcopy(params, &plt_chan, sizeof(wl_plt_channel_t));
#ifdef BCMDBG_ERR
		WL_ERROR(("PLTRX:  Band %d and Channel %d\n", plt_chan.band, plt_chan.channel));
#endif /* BCMDBG_ERR */
		bcmerror = wlc_plt_prepare(plt_info, plt_chan.channel, plt_chan.band, TRUE);
		if (bcmerror)
			WL_ERROR(("%s: wlc_plt_prepare iovar failed with %d\n",
				__FUNCTION__, bcmerror));
		break;
	}

	case IOV_GVAL(IOV_PLT_RX_RSSI):
	{
		uint32 unmod_rssi;
		/* send the phycommand to read the value of the RSSI */
		bcmerror = wlc_iovar_op(wlc, "unmod_rssi", NULL, 0, &unmod_rssi,
			sizeof(uint32), IOV_GET, NULL);
		if (bcmerror) {
			WL_ERROR(("%s: unmod_rssi iovar failed with %d\n", __FUNCTION__, bcmerror));
			break;
		}
		WL_ERROR(("Plt Rx RSSI Get iovar called rssi = %d\n", unmod_rssi));
		bcopy(&unmod_rssi, arg, sizeof(int));
		break;
	}
	case IOV_GVAL(IOV_PLT_MACADDR):
	{
		/* read mac addr */
		WL_ERROR(("Plt MACADDR Get iovar called \n"));
		bcopy(&wlc->pub->cur_etheraddr, arg, ETHER_ADDR_LEN);
		break;
	}

	case IOV_SVAL(IOV_PLT_MACADDR):
	{
		/* s mac addr */
		WL_ERROR(("Plt MACADDR Set iovar called \n"));
		/* down, update addr and then up */
		wl_down(wlc->wl);
		bcopy(params, &wlc->pub->cur_etheraddr, ETHER_ADDR_LEN);
#ifdef BCMDBG_ERR
		{
			char macbuf[64];
			WL_ERROR(("set_mac %s\n",
			          bcm_ether_ntoa(&wlc->pub->cur_etheraddr, macbuf)));
		}
#endif /* BCMDBG_ERR */
		bcopy(params, &wlc->bsscfg[0]->cur_etheraddr, ETHER_ADDR_LEN);
		wl_up(wlc->wl);
		break;
	}

	case IOV_GVAL(IOV_PLT_TSSI):
	{
		int pwrval;
		bool cck_rate;
		int txpwr = 0;
		bool ta_ok;

		WL_ERROR(("Plt TSSI Get iovar called \n"));
		if (IS_CCK(plt_info->txrspec))
			cck_rate = TRUE;
		else
			cck_rate = FALSE;

		/* init to 0 value */
		bcopy(&txpwr, arg, sizeof(uint32));

		if (!wlc->pub->up)
			break;

		bcmerror = wlc_phy_ioctl(wlc->band->pi, WLC_GET_TSSI, sizeof(uint32), &pwrval,
			&ta_ok);
		if (bcmerror) {
			WL_ERROR(("GET_TSSI IOCTL call failed with %d error\n", bcmerror));
			break;
		}
		if (plt_info->txmode != PLT_SRATE_HTMODE) {
			/* CCK or OFDM power depending on what rate is being used */
			if (cck_rate)
				txpwr = pwrval & 0xFF;
			else
				txpwr = (pwrval >> 8) & 0xFF;
			WL_ERROR(("1.power Value is %d, txpwr is %d\n", pwrval, txpwr));
			txpwr /= 4;
			bcopy(&txpwr, arg, sizeof(uint32));
		}
		else {
			WL_ERROR(("No Support for HTMode yet in PLT\n"));
			bcmerror = BCME_UNSUPPORTED;
		}
		break;
	}

	case IOV_SVAL(IOV_PLT_TXPOWER):
	{
		/* up the driver */
		if (!wlc->pub->up)
			wl_up(wlc->wl);

		int_val *= 4;
		plt_info->txpower_qdBm = int_val;

		bcmerror = wlc_plt_txpower_set(plt_info);
		break;
	}
	case IOV_GVAL(IOV_PLT_TXPOWER):
	{
		uint32 qdBm, dBm;

		bcmerror = wlc_phy_txpower_get(wlc->band->pi, &qdBm, NULL);

		dBm = qdBm / 4;
		bcopy(&dBm, arg, sizeof(uint32));
		break;
	}
	case IOV_SVAL(IOV_PLT_BTCX_ENTER):
	{
		bcmerror = wlc_plt_btcx_enter(wlc);
		break;
	}
	case IOV_SVAL(IOV_PLT_BTCX_CONFIG):
	{
		bcmerror = wlc_plt_btcx_config(wlc, (uint32)int_val);
		break;
	}
	case IOV_SVAL(IOV_PLT_BTCX_STATE):
	{
		bcmerror = wlc_plt_btcx_state(wlc, (uint32 *)&int_val, TRUE);
		break;
	}
	case IOV_GVAL(IOV_PLT_BTCX_STATE):
	{
		uint32 btcxval = 0;
		bcmerror = wlc_plt_btcx_state(wlc, &btcxval, FALSE);
		bcopy(&btcxval, arg, sizeof(uint32));
		break;
	}
	case IOV_SVAL(IOV_PLT_BTCX_EXIT):
	{
		bcmerror = wlc_plt_btcx_exit(wlc);
		break;
	}

	case IOV_GVAL(IOV_PLT_PMU_CBUCKMODE):
	{
		if (CHIPID(wlc->pub->sih->chip) == BCM4336_CHIP_ID ||
		    CHIPID(wlc->pub->sih->chip) == BCM43362_CHIP_ID) {
			uint32 cbuck_mode = PLT_CBUCKMODE_PWM;
			uint32 chipstatus;

			chipstatus = si_corereg(wlc->pub->sih, SI_CC_IDX,
			                        OFFSETOF(chipcregs_t, SECI_config), 0, 0);

			switch ((chipstatus & CST4336_CBUCK_MODE_MASK) >>
			        CST4336_CBUCK_MODE_SHIFT) {
			case 1:
				cbuck_mode = PLT_CBUCKMODE_BURST;
				break;
			case 3:
				cbuck_mode = PLT_CBUCKMODE_LPPWM;
				break;
			}

			bcopy(&cbuck_mode, arg, sizeof(uint32));
		} else
			bcmerror = BCME_UNSUPPORTED;
		break;
	}
	case IOV_SVAL(IOV_PLT_IBS_ENTER):
	{
		/* Put the driver is UP position */
		plt_info->mpc_orig = wlc->mpc;
		wlc->mpc = 0;
#ifdef STA
		wlc_radio_mpc_upd(wlc);
#endif /* STA */
		if (!wlc->pub->up)
			wl_up(wlc->wl);

		if (!wlc->pub->up) {
			bcmerror = BCME_NOTUP;
			break;
		}

		wlc_minimal_up(wlc);
		plt_info->ibs_enter = TRUE;
		plt_info->plt_pub.plt_in_progress = TRUE;
		wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_PLT, TRUE);
		bcmerror = wlc_minimal_down(wlc);
		break;
	}

	case IOV_SVAL(IOV_PLT_IBS_EXIT):
	{
		if (plt_info->ibs_enter != TRUE)
			break;
		plt_info->ibs_enter = FALSE;
		plt_info->plt_pub.plt_in_progress = FALSE;
		wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_PLT, FALSE);

		/* now try to restore the original mpc state */
		wlc->mpc = plt_info->mpc_orig;

		wlc_minimal_up(wlc);
		break;
	}

	case IOV_GVAL(IOV_PLT_SLEEP_CLK):
	{
		uint32 sleep_clk;
		uint32 chipstatus;

		chipstatus = si_corereg(wlc->pub->sih, SI_CC_IDX,
			OFFSETOF(chipcregs_t, pmustatus), 0, 0);

		if (chipstatus & PST_EXTLPOAVAIL)
			sleep_clk = PLT_SLEEPCLK_EXTERNAL;
		else
			sleep_clk = PLT_SLEEPCLK_INTERNAL;

		bcopy(&sleep_clk, arg, sizeof(uint32));
		break;
	}
	default:
		WL_ERROR(("Unupported PLT Module iovar called \n"));
		bcmerror = BCME_UNSUPPORTED;
		break;
	}
	if (bcmerror)
		WL_ERROR(("Bcmerror from the iovars %d\n", bcmerror));
	return bcmerror;
}

/*
 * XXX: Please refer to wlc.c: wlc_doiovar implementaion to see why the iovar implementation is
 * split between wlc_plt_doiovar and _wlc_plt_doiovar_
 */
static int
wlc_plt_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 aid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	return (_wlc_plt_doiovar_(hdl, vi, aid, name, params, p_len, arg, len, val_size, wlcif));
}

static void
wlc_plt_done(plt_info_t *plt)
{
	uint32 scan_suppress = 0;
	wlc_info_t *wlc = plt->wlc;

	plt->plt_pub.plt_in_progress = FALSE;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_PLT, FALSE);
	plt->txper_in_progress = FALSE;
	plt->plt_tx_status = 0;

	wlc->mpc = plt->mpc_orig;
	plt->mpc_orig = 0;

	wlc_bandlock(wlc, WLC_BAND_AUTO);

	wlc_scan_ioctl(wlc->scan, WLC_SET_SCANSUPPRESS, &scan_suppress, sizeof(uint32), NULL);
#ifdef STA
	wlc_radio_mpc_upd(wlc);
#endif /* STA */
	wl_up(wlc->wl);
	wl_down(wlc->wl);

}

static int
wlc_plt_prepare(plt_info_t *plt_info, int8 channel, int8 band, bool bandlock)
{
	int bcmerror = 0;
	chanspec_t chanspec = CH20MHZ_CHSPEC(channel);
	uint32 scan_suppress = 1;
	wlc_info_t *wlc = plt_info->wlc;

	/* Put the driver is UP position */
	plt_info->mpc_orig = wlc->mpc;
	wlc->mpc = 0;
#ifdef STA
	wlc_radio_mpc_upd(wlc);
#endif /* STA */
	if (!wlc->pub->up)
		wl_up(wlc->wl);

	if (!wlc->pub->up) {
		bcmerror = BCME_NOTUP;
		goto done;
	}

	wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);

	wlc_scan_ioctl(wlc->scan, WLC_SET_SCANSUPPRESS, &scan_suppress, sizeof(uint32), NULL);

	WL_ERROR(("Setting the channel to chanspec 0x%x\n", chanspec));
	/* suspend mac and set channel */
	wlc_suspend_mac_and_wait(wlc);
	wlc_set_chanspec(wlc, chanspec);
	wlc_enable_mac(wlc);

	bcmerror = wlc_bandlock(wlc, band);
	if (bcmerror)
		goto done;

	/* init the tx status */
	plt_info->txper_in_progress = FALSE;
	plt_info->plt_tx_status = 0;

	plt_info->plt_pub.plt_in_progress = TRUE;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_PLT, TRUE);
done:
	if (bcmerror)
		WL_ERROR(("%s: failed with %d error\n", __FUNCTION__, bcmerror));
	return bcmerror;
}

static int
wlc_plt_txrate_set(plt_info_t *plt, wl_plt_srate_t srate)
{
	wlc_info_t *wlc = plt->wlc;
	int bcmerror = 0;
	ratespec_t rspec = 0;
	if (!WLC_PHY_11N_CAP(wlc->band)) {
		if (srate.mode == PLT_SRATE_HTMODE) {
			WL_ERROR(("Invalid HTMode for Non N capable PHY\n"));
			return BCME_BADARG;
		}
	}

#ifdef WL11N
	if (srate.mode == PLT_SRATE_HTMODE) {

		rspec = HT_RSPEC(srate.m_idx);

		/* Short GI */
		if (srate.igi == PLT_SRATE_IGI_400ns) {
			rspec |= RSPEC_SHORT_GI;
		}

		bcmerror = wlc_set_ratespec_override(wlc, wlc->band->bandtype, rspec, FALSE);
		if (bcmerror)
			goto done;
	}
	else {
#endif /* WL11N */
		/* bcmerror check: set both mcast and ucast rate overrides */
		rspec = LEGACY_RSPEC(srate.tx_rate);

		if (IS_MBAND_UNLOCKED(wlc)) {
			WL_ERROR(("%s: band must be set\n", __FUNCTION__));
			bcmerror = BCME_ERROR;
			goto done;
		}

		bcmerror = wlc_set_ratespec_override(wlc, wlc->band->bandtype, rspec, FALSE);
		if (bcmerror)
			goto done;
		bcmerror = wlc_set_ratespec_override(wlc, wlc->band->bandtype, rspec, TRUE);
		if (bcmerror)
			goto done;
#ifdef WL11N
	}
#endif /* WL11N */
	plt->txrspec = rspec;
	plt->txmode = srate.mode;
done:
	if (bcmerror)
		WL_ERROR(("%s: failed with %d error\n", __FUNCTION__, bcmerror));
	return bcmerror;
}

static int
wlc_plt_txpreamble_set(wlc_info_t *wlc, int8 preamble_override, bool htmode)
{
	if (preamble_override == PLT_PREAMBLE_LONG) {
#ifdef WL11N
		if (htmode)
			/* Set the Mixed Mode header */
			wlc_phy_preamble_override_set(wlc->band->pi, WLC_N_PREAMBLE_MIXEDMODE);
		else
#endif /* WL11N */
			wlc->cfg->PLCPHdr_override = WLC_PLCP_LONG;
	}
	else {
#ifdef WL11N
		if (htmode)
			wlc_phy_preamble_override_set(wlc->band->pi, WLC_N_PREAMBLE_GF);
		else
#endif /* WL11N */
			wlc->cfg->PLCPHdr_override = WLC_PLCP_SHORT;
	}
	return 0;
}

#define MACSTATOFF(name) ((uint)((char *)(&wlc->pub->_cnt->name) - \
	(char *)(&wlc->pub->_cnt->txallfrm)))

static void
wlc_plt_upd_rxper_stats(plt_info_t *plt_info, bool clear)
{
	wl_pkteng_stats_t pkteng_s;
	wlc_info_t *wlc = plt_info->wlc;

#ifdef WLCNT
	WL_ERROR(("Updating the stats\n"));
	wlc_ctrupd(wlc, OFFSETOF(macstat_t, rxbadfcs), MACSTATOFF(rxbadfcs));
	wlc_ctrupd(wlc, OFFSETOF(macstat_t, rxbadplcp), MACSTATOFF(rxbadplcp));
	wlc_ctrupd(wlc, OFFSETOF(macstat_t, pktengrxducast), MACSTATOFF(pktengrxducast));
	wlc_ctrupd(wlc, OFFSETOF(macstat_t, pktengrxdmcast), MACSTATOFF(pktengrxdmcast));

	if (plt_info->rxper_multicast)
		plt_info->rxper_stats_e.frames = wlc->pub->_cnt->pktengrxdmcast;
	else
		plt_info->rxper_stats_e.frames = wlc->pub->_cnt->pktengrxducast;

	plt_info->rxper_stats_e.fcs_errs = wlc->pub->_cnt->rxbadfcs;
	plt_info->rxper_stats_e.plcp_errs = wlc->pub->_cnt->rxbadplcp;
#else
	WL_ERROR(("Not updating the stats as WLCNT is not defined \n"));
#endif /* WLCNT */

	bzero(&pkteng_s, sizeof(wl_pkteng_stats_t));
	wlc_iovar_op(wlc, "pkteng_stats", NULL, 0, &pkteng_s,
		(uint)sizeof(wl_pkteng_stats_t), IOV_GET, NULL);

	plt_info->rxper_stats_e.lost_frames = pkteng_s.lostfrmcnt;
	plt_info->rxper_stats_e.snr = (uint8)pkteng_s.snr;
	plt_info->rxper_stats_e.rssi = (uint8)pkteng_s.rssi;

	if (clear) {
		bcopy(&plt_info->rxper_stats_e, &plt_info->rxper_stats_s,
			sizeof(wl_plt_rxper_results_t));
	}
}

/* Radio is on the proper channel and is ready to TXPER.
 * The special power value 0 means to skip setting power and leave hardware defaults
 */
static int
wlc_plt_txpower_set(plt_info_t *plt_info)
{
	uint32 txpwr_index;
	int bcmerror = 0;
	int8 qdBm;
	wlc_info_t *wlc = plt_info->wlc;
	ppr_t *txpwr;

	if (plt_info->txpower_qdBm == 0)
		return 0;

	qdBm = (int8)plt_info->txpower_qdBm;

	if (qdBm) {
		WL_INFORM(("Set txpower %d and mode %d\n", qdBm, plt_info->pwrctl_mode));

		if ((txpwr = ppr_create(wlc->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
			return bcmerror;
		}

		wlc_channel_reg_limits(wlc->cmi, wlc->chanspec, txpwr);
		ppr_apply_max(txpwr, WLC_TXPWR_MAX);
		bcmerror = wlc_phy_txpower_set(wlc->band->pi, qdBm, FALSE, txpwr);
		ppr_delete(wlc->osh, txpwr);

		if (bcmerror) {
			WL_ERROR(("%s: Set the qtxpower(%d) iovar failed with error %d\n",
			          __FUNCTION__, qdBm, bcmerror));
			return bcmerror;
		}
	}
	else
		WL_INFORM(("txpower: use default value, mode %d", plt_info->pwrctl_mode));

	if (plt_info->pwrctl_mode != PLT_TXPWR_CTRL_OPEN)
		return 0;

	/* openloop txpower setting */
	if (qdBm)
		txpwr_index = (qdBm > 96)  ? 0 : (96 - qdBm);
	else
		txpwr_index =  0;

	bcmerror = wlc_iovar_setint(wlc, "phy_txpwrindex", txpwr_index);
	if (bcmerror) {
		WL_ERROR(("%s: %d: OPENLOOP: setting the pwrindex(%d) failed %d\n",
			__FUNCTION__, __LINE__, qdBm, bcmerror));
		return bcmerror;
	}
	return 0;
}

/* Enter BTCX test mode */
static int
wlc_plt_btcx_enter(wlc_info_t *wlc)
{
	plt_info_t *plt = (plt_info_t *)wlc->plt;
	bool suspend;

	if (plt->btcx_testmode)
		return 0;

	plt->mpc_orig = wlc->mpc;
	wlc->mpc = 0;
#ifdef STA
	wlc_radio_mpc_upd(wlc);
#endif /* STA */
	if (!wlc->pub->up) {
		wl_up(wlc->wl);
	}
	if (!wlc->pub->up)
		return BCME_NOTUP;

#ifndef BCMNODOWN
	if (wlc->pub->up && wlc->clk && wlc->pub->hw_up)
		wlc_out(wlc);
#endif /* BCMNODOWN */

	/* btcx_rfactive, btcx_status, btcx_txconf */

#ifdef BCMECICOEX
	if (si_seci(wlc->pub->sih)) {
		plt->seci = TRUE;
		plt->seci_mode = si_corereg(wlc->pub->sih, SI_CC_IDX,
			OFFSETOF(chipcregs_t, SECI_config), 0, 0);
		si_seci_init(wlc->pub->sih, SECI_MODE_LEGACY_3WIRE_WLAN);
	}
#endif /* BCMECICOEX */

	suspend = !(R_REG(wlc->osh, &wlc->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		wlc_suspend_mac_and_wait(wlc);

	plt->plt_pub.plt_in_progress = TRUE;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_PLT, FALSE);

	/* Enable BTCX input mode:
	 *   Setting gpiocontrol bit 10 connects GPIO 3 to txconf, 4 to status, and 5 to
	 *   rfactive.  It also stops BTCX from driving txconf.
	 *   Clearing gpiocontrol 3, 4, 5 makes sure those pins are assigned to chipcommon.
	 *   GPIO 3, 4 and 5 are set to inputs so they can be read.
	 */
	plt->btcx_gpioout_save = si_gpioouten(wlc->pub->sih, 0, 0, 0);
	si_gpioouten(wlc->pub->sih, (1 << 3) | (1 << 4) | (1 << 5), 0, 0);

	plt->btcx_gpiocontrol_save = si_gpiocontrol(wlc->pub->sih, 0, 0, 0);
	si_gpiocontrol(wlc->pub->sih, (1 << 3) | (1 << 4) | (1 << 5) | (1 << 10), (1 << 10), 0);

	plt->btcx_testmode = TRUE;

	return 0;
}

/* Set BTCX test pin direction: bitmask of PLT_BTCX_xx (in/out = 0/1) */
static int
wlc_plt_btcx_config(wlc_info_t *wlc, uint32 pins_out)
{
	/* xxx Configuring pins as output not supported by all hardware (4336) and customer
	 * doesn't need it.  We currently support testing BTC pins as inputs only.
	 */
	if (pins_out != 0)
		return BCME_UNSUPPORTED;

	return 0;
}

/* Set or get BTCX pin values by reading/writing IHR regs */
static int
wlc_plt_btcx_state(wlc_info_t *wlc, uint32 *val, bool set)
{
	uint16 t;
	plt_info_t *plt = (plt_info_t *)wlc->plt;

	if (!plt->btcx_testmode) {
		int bcmerror = wlc_plt_btcx_enter(wlc);
		if (bcmerror != 0)
			return bcmerror;
	}

	if (!set) {
		*val = 0;

		t = si_gpioin(wlc->pub->sih);
		if (t & (1 << 3))
			*val |= (1 << PLT_BTCX_TXCONF);
		if (t & (1 << 4))
			*val |= (1 << PLT_BTCX_TXSTATUS);
		if (t & (1 << 5))
			*val |= (1 << PLT_BTCX_RFACTIVE);
	}

	return 0;
}

/* Exit BTCX test mode */
static int
wlc_plt_btcx_exit(wlc_info_t *wlc)
{
	plt_info_t *plt = (plt_info_t *)wlc->plt;

	if (!plt->btcx_testmode)
		return 0;

	si_gpiocontrol(wlc->pub->sih, (1 << 3) | (1 << 4) | (1 << 5) | (1 << 10),
	               plt->btcx_gpiocontrol_save, 0);
	si_gpioouten(wlc->pub->sih, (1 << 3) | (1 << 4) | (1 << 5),
	             plt->btcx_gpioout_save, 0);

	plt->plt_pub.plt_in_progress = FALSE;
	wlc->mpc = plt->mpc_orig;
	plt->mpc_orig = 0;
	plt->btcx_testmode = FALSE;
#ifdef STA
	wlc_radio_mpc_upd(wlc);
#endif /* STA */
	wl_up(wlc->wl);
	if (!wlc->pub->up)
		return 0;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_PLT, FALSE);
#ifdef BCMECICOEX
	if (plt->seci)
		si_seci_init(wlc->pub->sih, plt->seci_mode);
#endif /* BCMECICOEX */

	wl_down(wlc->wl);
	return 0;
}

static int
wlc_plt_pkteng(plt_info_t *plt_info, wl_pkteng_t *pkteng)
{
#ifdef WLTEST
	uint32 flags;
	struct ether_addr *sa;
	void *pkt = NULL;
	wlc_info_t *wlc = plt_info->wlc;

	flags = pkteng->flags  & WL_PKTENG_PER_MASK;
	/* Prepare the packet for Tx */
	if ((flags == WL_PKTENG_PER_TX_START) || (flags == WL_PKTENG_PER_TX_WITH_ACK_START)) {
		sa = (ETHER_ISNULLADDR(&pkteng->src)) ?
			&wlc->pub->cur_etheraddr : &pkteng->src;
		/* pkt will be freed in wlc_bmac_pkteng() */
		pkt = wlc_tx_testframe(wlc, &pkteng->dest, sa, 0, pkteng->length);
		if (pkt == NULL)
			return BCME_NOMEM;

	}
	return (wlc_bmac_pkteng(wlc->hw, pkteng, pkt));
#else /* WLTEST */
	return 0;
#endif /* WLTEST */
}
