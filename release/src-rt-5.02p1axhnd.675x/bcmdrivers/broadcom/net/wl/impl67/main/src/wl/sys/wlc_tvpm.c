/**
 * Thermal, Voltage, and Power Mitigation
 * @file
 * @brief
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
 * $Id: wlc_tvpm.c 785034 2020-03-11 11:53:38Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi_channels.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_bmac.h>
#include <wlc_dump.h>
#include <wlc_ap.h>
#include <wlc_stf.h>
#include <phy_api.h>
#include <phy_tpc_api.h>
#include <phy_temp_api.h>
#include <wlc_tvpm.h>

#define TVPM_TXDC_IDX_VBAT_MAX		3u
#define TVPM_TXDC_IDX_TEMP_MAX		2u
#define TVPM_TXDC_IDX_PWR_MAX		10u
#define TVPM_CHAINS_IDX_VBAT_MAX	3u
#define TVPM_CHAINS_IDX_PPM_MAX		3u
#define TVPM_CHAINS_IDX_VBAT_MAX	3u

#define TVPM_TXBO_IDX_VBAT_MAX	3u
#define TVPM_TXBO_IDX_TEMP_MAX	2u

#define TVPM_PWR_IDX_VAL_MAX	100
#define TVPM_PWR_IDX_VAL_DEF	TVPM_PWR_IDX_VAL_MAX

#define TVPM_PWRIDX_RANGE_LO	1
#define TVPM_PWRIDX_RANGE_HI	100
#define TVPM_DUTYCYCLE_RANGE_LO	1
#define TVPM_DUTYCYCLE_RANGE_HI	100

#define TVPM_TXPWRBACKOFF_INDEX_MAX (TVPM_TXBO_IDX_VBAT_MAX*TVPM_TXBO_IDX_TEMP_MAX)
#define TVPM_TXPWRBACKOFF_RANGE_LO	0
#define TVPM_TXPWRBACKOFF_RANGE_HI	20
#define TVPM_TXPWRBACKOFF_NONE		0

#define TVPM_NO_TXBO			0

/* To do: Add chip-specific defines to change the max # txchains for other chips */
#define TVPM_TXCHAIN_RANGE_LO	1
/* 4357 values */
#define TVPM_TXCHAIN_RANGE_HI	2

typedef enum {
	TVPM_INP_THERM = 0,
	TVPM_INP_CLTM = 1,
	TVPM_INP_PPM = 2,
	TVPM_INP_PWR = 3,		/* Not supported in TVPM */

	TVPM_INP_MAX
} txdc_value_t;

#define TVPM_INP_VBAT TVPM_INP_THERM

typedef struct wlc_tvpm_input {
	int16 temp;
	uint8 vbat;
	uint8 cltm;
	uint8 ppm;
} wlc_tvpm_input_t;

typedef struct wlc_tvpm_config {
	/* All 1-D arrays are variable length and terminated with a zero array element */
	/* temperature could be negative. These arrays are terminated with min defined value */
	/* All 2-D arrays are fixed length and not zero terminated */

	uint8 txdc_vbat[TVPM_TXDC_IDX_VBAT_MAX];		/* voltage threshold */
	int16 txdc_temp[TVPM_TXDC_IDX_TEMP_MAX];		/* temperature threshold */
	uint8 txdc_cltm[TVPM_TXDC_IDX_PWR_MAX + 1];		/* cltm threshold */
	uint8 txdc_ppm[TVPM_TXDC_IDX_PWR_MAX + 1];		/* ppm threshold */
	uint8 txdc_config_temp_vbat [TVPM_TXDC_IDX_TEMP_MAX][TVPM_TXDC_IDX_VBAT_MAX];
	uint8 txdc_config_cltm[TVPM_TXDC_IDX_PWR_MAX + 1];	/* dutycycle values */
	uint8 txdc_config_ppm[TVPM_TXDC_IDX_PWR_MAX + 1];	/* dutycycle values */
	/* Tx chains control */
	uint8 txchain_vbat[TVPM_CHAINS_IDX_VBAT_MAX + 1];	/* voltage threshold */
	uint8 txchain_ppm[TVPM_CHAINS_IDX_PPM_MAX + 1];		/* ppm threshold */
	uint8 txchain_config_vbat[TVPM_CHAINS_IDX_VBAT_MAX + 1]; /* number of chains */
	uint8 txchain_config_ppm[TVPM_CHAINS_IDX_PPM_MAX + 1];	/* number of chains */
	/* Tx backoff control */
	uint8 pwrbackoff_vbat [TVPM_TXBO_IDX_VBAT_MAX];	/* voltage thresholds     */
	int16 pwrbackoff_temp [TVPM_TXBO_IDX_TEMP_MAX]; /* temperature thresholds */
					/* Tx backoff per band in units .25dB */
	int8  pwrbackoff_config_2g[TVPM_TXBO_IDX_TEMP_MAX][TVPM_TXBO_IDX_VBAT_MAX];
	int8  pwrbackoff_config_5g[TVPM_TXBO_IDX_TEMP_MAX][TVPM_TXBO_IDX_VBAT_MAX];

	/* TVPM monitor period in units of 1 sec wdog ticks */
	uint16 monitor_period;
} wlc_tvpm_config_t;

struct wlc_tvpm_info {
	wlc_info_t		*wlc;
	wlc_tvpm_config_t	*config;	/* configurations   */
	wlc_tvpm_input_t	*input;		/* input parameters */

	/* current mitigation values */
	uint16 txduty_cycle;			/* tx duty cycle percentage */
	int16  txpwr_backoff;			/* tx power backoff in units .25 dBm */
	uint16 tx_active_chains;		/* # of active tx chains */

	/* module internal data */
	uint16 monitor_cnt;
	uint8  tx_dc[TVPM_INP_MAX];		/* calculated DC values from each input */
	uint8  tx_chain[TVPM_INP_MAX];		/* calculated number of tx cores by each input */
	uint8  txdc_config;			/* inputs enabled for tx duty cycle */
	uint8  txdc_state;			/* inputs affecting current tx dc */
	uint8  txchain_config;			/* inputs enabled for tx core # */
	uint8  txchain_state;			/* inputs affecting tx core # */
	uint8  txpwrbackoff_config;		/* inputs enabled for tx power */
	uint8  txpwrbackoff_state;		/* inputs affecting tx power */
	int16  tx_bo;				/* calculated txbackoff */

	int32  err;				/* last error */
	bool   enable;				/* feature on/off */
	bool   ready;
};

/* For debugging FW startup failures due to malformed tvpm nvram parameters */
int dbg_tvpm_getvar_err = 0;
const char* dbg_tvpm_getvar_rstr = NULL;
wlc_tvpm_info_t *dbg_tvpm = NULL;
uint8 dbg_last_val = 0;
int dbg_last_idx = 0;

static const char BCMATTACHDATA(rstr_tvpm)[] = "tvpm";
static const char BCMATTACHDATA(rstr_tvpm_monitor_period)[] = "tvpm_monitor_period";
static const char BCMATTACHDATA(rstr_tvpm_dc_cltm)[] = "tvpm_dc_cltm";
static const char BCMATTACHDATA(rstr_tvpm_dc_ppm)[] = "tvpm_dc_ppm";
static const char BCMATTACHDATA(rstr_tvpm_dc_temp_threshold)[] = "tvpm_dc_temp_threshold";
static const char BCMATTACHDATA(rstr_tvpm_dc_vbat_threshold)[] = "tvpm_dc_vbat_threshold";
static const char BCMATTACHDATA(rstr_tvpm_dc_vbat_temp)[] = "tvpm_dc_vbat_temp";
static const char BCMATTACHDATA(rstr_tvpm_txchain_ppm)[] = "tvpm_txchain_ppm";
static const char BCMATTACHDATA(rstr_tvpm_txchain_vbat)[] = "tvpm_txchain_vbat";
static const char BCMATTACHDATA(rstr_tvpm_pwrboff_2g)[] = "tvpm_pwrboff_2g";
static const char BCMATTACHDATA(rstr_tvpm_pwrboff_5g)[] = "tvpm_pwrboff_5g";
static const char BCMATTACHDATA(rstr_tvpm_pwrboff_temp_threshold)[] = "tvpm_pwrboff_temp_threshold";
static const char BCMATTACHDATA(rstr_tvpm_pwrboff_vbat_threshold)[] = "tvpm_pwrboff_vbat_threshold";

/* phy interface functions */
static int16
wlc_tvpm_temp_sense_read(wlc_info_t *wlc)
{
	int16 temp = 0, tempover = 0;

#if defined(BCMDBG) || defined(WLTEST) || defined(TEMPSENSE_OVERRIDE)
	tempover = phy_temp_get_override(WLC_PI(wlc));
#endif /* BCMDBG || WL_TEST || TEMPSENSE_OVERRIDE */

	temp = phy_temp_sense_read(WLC_PI(wlc));
	if (tempover) {
		temp = tempover;
	}

	return temp;
}

static uint8
wlc_tvpm_vbat_sense_read(wlc_info_t *wlc)
{
	return phy_vbat_sense_read(WLC_PI(wlc));
}

/* module control interface */
static int
wlc_tvpm_enable(wlc_tvpm_info_t *tvpm, bool enable)
{
	tvpm->enable = (enable ? TRUE : FALSE);

	/* Keep Tx Power Backoff in sync */
#ifdef TXPWRBACKOFF
	phy_tpc_txbackoff_enable(WLC_PI(tvpm->wlc), tvpm->enable);
#endif /* TXPWRBACKOFF */
	return BCME_OK;
}

/* Update internal measurements: voltage, temperature */
static int
wlc_tvpm_update_inputs(wlc_info_t *wlc, wlc_tvpm_input_t *input, bool all_enable)
{
	if (wlc->stf->tempsense_disable) {
		return BCME_OK;
	}

	/* time scaling for Vbat/Temp measurements
	 * measure every cycle, perhaps add parameter to scale calibrations
	if ((wlc->pub->now - wlc->stf->tempsense_lasttime) < wlc->stf->tempsense_period) {
		return;
	}
	wlc->stf->tempsense_lasttime = wlc->pub->now;
	*/

	input->temp = wlc_tvpm_temp_sense_read(wlc);

	if (all_enable) {
		input->vbat = wlc_tvpm_vbat_sense_read(wlc);
	}

	WL_NONE(("wl%u: V=%u T=%d\n",
		WLCWLUNIT(wlc), input->vbat, input->temp));

	return BCME_OK;
}

/* ******************* Calculate DC ***************************** */

static int
wlc_tvpm_update_txdc_vbat_temp(wlc_tvpm_info_t *tvp, wlc_tvpm_input_t *input)
{
	wlc_tvpm_config_t *conf = tvp->config;

	unsigned idx_temp = 0,
	         idx_vbat = 0,
	         idx, size;

	if (!PHY_VALID_TEMP(tvp->input->temp) || !PHY_VALID_VBAT(tvp->input->vbat)) {
		WL_ERROR(("wl%u: bad temperature or Vbat reading T=%d V=%u\n",
			WLCWLUNIT(tvp->wlc), tvp->input->temp, tvp->input->vbat));
		return BCME_OK;
	}

	/* Check if anything changed */
	if (tvp->input->temp == input->temp &&
		tvp->input->vbat == input->vbat)
		return BCME_OK;

	/* find corresponding table entry */
	for (idx = 0, size = ARRAYSIZE(conf->txdc_vbat);
			idx < size; idx++) {
		if (input->vbat >= conf->txdc_vbat[idx] ||
			conf->txdc_vbat[idx] <= PHY_VBAT_MIN) {
			idx_vbat = idx;
			break;
		}
	}

	for (idx = 0, size = ARRAYSIZE(conf->txdc_temp);
			idx < size; idx++) {
		if (input->temp >= conf->txdc_temp[idx] ||
			conf->txdc_temp[idx] <= PHY_TEMP_MIN) {
			idx_temp = idx;
			break;
		}
	}
	ASSERT(idx_vbat < ARRAYSIZE(conf->txdc_vbat) &&
		idx_temp < ARRAYSIZE(conf->txdc_temp));

	/* update required DC value */
	if (tvp->tx_dc[TVPM_INP_THERM] != conf->txdc_config_temp_vbat[idx_temp][idx_vbat]) {
		tvp->tx_dc[TVPM_INP_THERM]  = conf->txdc_config_temp_vbat[idx_temp][idx_vbat];
		return 1;
	}

	return BCME_OK;
}

static int
wlc_tvpm_update_txdc_cltm(wlc_tvpm_info_t *tvp, wlc_tvpm_input_t *input)
{
	wlc_tvpm_config_t *conf = tvp->config;
	unsigned idx, size;

	for (idx = 0, size = ARRAYSIZE(conf->txdc_cltm);
			idx < size; idx++) {
		if (input->cltm >= conf->txdc_cltm[idx] ||
			conf->txdc_cltm[idx] <= TVPM_PWRIDX_RANGE_LO) {
			break;
		}
	}
	ASSERT(idx < ARRAYSIZE(conf->txdc_cltm));

	if (tvp->tx_dc[TVPM_INP_CLTM] != conf->txdc_config_cltm[idx]) {
		tvp->tx_dc[TVPM_INP_CLTM]  = conf->txdc_config_cltm[idx];
		return 1;
	}

	return BCME_OK;
}

static int
wlc_tvpm_update_txdc_ppm(wlc_tvpm_info_t *tvp, wlc_tvpm_input_t *input)
{
	wlc_tvpm_config_t *conf = tvp->config;
	unsigned idx, size;

	for (idx = 0, size = ARRAYSIZE(conf->txdc_ppm);
			idx < size; idx++) {
		if (input->ppm >= conf->txdc_ppm[idx] ||
			conf->txdc_ppm[idx] <= TVPM_PWRIDX_RANGE_LO) {
			break;
		}
	}
	ASSERT(idx < ARRAYSIZE(conf->txdc_ppm));

	if (tvp->tx_dc[TVPM_INP_PPM] != conf->txdc_config_ppm[idx]) {
		tvp->tx_dc[TVPM_INP_PPM]  = conf->txdc_config_ppm[idx];
		return 1;
	}

	return BCME_OK;
}

static int
wlc_tvpm_update_txdc(wlc_tvpm_info_t *tvpm, wlc_tvpm_input_t *input)
{
	wlc_info_t *wlc = tvpm->wlc;
	wlc_stf_info_t  *stf = wlc->stf;
	int		ret_val = 0;

	/* mask power throttling. Not supported in TVPM */
	if ((tvpm->txdc_config & ~WLC_PWRTHROTTLE_ON) == WLC_THROTTLE_OFF) {
		return ret_val;
	}

	if (tvpm->txdc_config & WLC_TEMPTHROTTLE_ON) {
		ret_val += wlc_tvpm_update_txdc_vbat_temp(tvpm, input);
		tvpm->txdc_state = (tvpm->tx_dc[TVPM_INP_THERM] == NO_DUTY_THROTTLE)
			? tvpm->txdc_state & ~WLC_TEMPTHROTTLE_ON
			: tvpm->txdc_state |  WLC_TEMPTHROTTLE_ON;
	}

	if (tvpm->txdc_config & WLC_CLTMTHROTTLE_ON) {
		ret_val += wlc_tvpm_update_txdc_cltm(tvpm, input);
		tvpm->txdc_state = (tvpm->tx_dc[TVPM_INP_CLTM] == NO_DUTY_THROTTLE)
			? tvpm->txdc_state & ~WLC_CLTMTHROTTLE_ON
			: tvpm->txdc_state |  WLC_CLTMTHROTTLE_ON;
	}

	if (tvpm->txdc_config & WLC_PPMTHROTTLE_ON) {
		ret_val += wlc_tvpm_update_txdc_ppm(tvpm, input);
		tvpm->txdc_state = (tvpm->tx_dc[TVPM_INP_PPM] == NO_DUTY_THROTTLE)
			? tvpm->txdc_state & ~WLC_PPMTHROTTLE_ON
			: tvpm->txdc_state |  WLC_PPMTHROTTLE_ON;
	}

	/* update for compatibility with old stf mitigation */
	stf->throttle_state = (stf->throttle_state & WLC_PWRTHROTTLE_ON) |
		tvpm->txdc_state | tvpm->txchain_state;

	if (ret_val) {
		/* call tx dutycycle update if there is a change in target value */
		wlc_stf_txduty_upd(wlc);
	}

	return ret_val;
}

/* ******************* Tx Chain control ***************************** */

static int
wlc_tvpm_update_txchain_vbat(wlc_tvpm_info_t *tvp, wlc_tvpm_input_t *input)
{
	wlc_tvpm_config_t *conf = tvp->config;
	unsigned idx, size;

	if (!PHY_VALID_VBAT(tvp->input->vbat)) {
		WL_ERROR(("wl%u: bad Vbat reading V=%u\n",
			WLCWLUNIT(tvp->wlc), tvp->input->vbat));
		return BCME_OK;
	}

	for (idx = 0, size = ARRAYSIZE(conf->txchain_vbat); idx < size; idx++) {
		if (input->vbat >= conf->txchain_vbat[idx]) {
			break;
		}
	}
	ASSERT(idx < ARRAYSIZE(conf->txchain_vbat));

	if (tvp->tx_chain[TVPM_INP_VBAT] != conf->txchain_config_vbat[idx]) {
		tvp->tx_chain[TVPM_INP_VBAT]  = conf->txchain_config_vbat[idx];
		return 1;
	}

	return BCME_OK;
}

static int
wlc_tvpm_update_txchain_ppm(wlc_tvpm_info_t *tvp, wlc_tvpm_input_t *input)
{
	wlc_tvpm_config_t *conf = tvp->config;
	unsigned idx, size;

	for (idx = 0, size = ARRAYSIZE(conf->txchain_ppm); idx < size; idx++) {
		if (input->ppm >= conf->txchain_ppm[idx]) {
			break;
		}
	}
	ASSERT(idx < ARRAYSIZE(conf->txchain_ppm));

	if (tvp->tx_chain[TVPM_INP_PPM] != conf->txchain_config_ppm[idx]) {
		tvp->tx_chain[TVPM_INP_PPM]  = conf->txchain_config_ppm[idx];
		return 1;
	}

	return BCME_OK;
}

static int
wlc_tvpm_update_txchain(wlc_tvpm_info_t *tvpm, wlc_tvpm_input_t *input)
{
	wlc_info_t *wlc = tvpm->wlc;
	wlc_stf_info_t  *stf = wlc->stf;
	int		ret_val = 0;
	uint8		tmpstate = 0;
	uint8		txchain = TVPM_TXCHAIN_RANGE_HI, hwtxchain;

	/* mask power throttling. Not supported in TVPM */
	if (tvpm->txchain_config == WLC_THROTTLE_OFF) {
		return ret_val;
	}

	if (tvpm->txchain_config & WLC_VBATTHROTTLE_ON) {
		if (wlc_tvpm_update_txchain_vbat(tvpm, input)) {
			++ret_val;
			if (tvpm->tx_chain[TVPM_INP_VBAT] == TVPM_TXCHAIN_RANGE_HI) {
				/* reset thermal txchain control */
				tvpm->txchain_state &= ~WLC_VBATTHROTTLE_ON;
				wlc_stf_txchain_upd(wlc, FALSE, WLC_TXCHAIN_ID_TEMPSENSE);
			} else {
				tmpstate |= WLC_VBATTHROTTLE_ON;
			}
		}
	}

	if (tvpm->txchain_config & WLC_PPMTHROTTLE_ON) {
		if (wlc_tvpm_update_txchain_ppm(tvpm, input)) {
			++ret_val;
			if (tvpm->tx_chain[TVPM_INP_PPM] == TVPM_TXCHAIN_RANGE_HI) {
				/* reset power txchain control */
				tvpm->txchain_state &= ~WLC_PPMTHROTTLE_ON;
				wlc_stf_txchain_upd(wlc, FALSE, WLC_TXCHAIN_ID_PWRTHROTTLE);
			} else {
				tmpstate |= WLC_PPMTHROTTLE_ON;
			}
		}
	}

	txchain = wlc_tvpm_get_txchain_min(tvpm);
	hwtxchain = WLC_BITSCNT(stf->txchain);
	WL_NONE(("wl%d[%s] txchain: curr=%u, trgt=%u, hw=%u, state=%#X\n",
		wlc->pub->unit, __FUNCTION__,
		tvpm->tx_active_chains, txchain, hwtxchain, tmpstate));

	/* call txchain update if there is a pending throttle request */
	if (ret_val && tmpstate) {
		if (tmpstate & WLC_VBATTHROTTLE_ON) {
			wlc_stf_txchain_upd(wlc, TRUE, WLC_TXCHAIN_ID_TEMPSENSE);
		}
		if (tmpstate & WLC_PPMTHROTTLE_ON) {
			wlc_stf_txchain_upd(wlc, TRUE, WLC_TXCHAIN_ID_PWRTHROTTLE);
		}
	}

	tvpm->txchain_state |= tmpstate;
	tvpm->tx_active_chains = hwtxchain;
	tvpm->tx_active_chains = txchain;

	/* update for compatibility with old stf mitigation */
	stf->throttle_state = (stf->throttle_state & WLC_PWRTHROTTLE_ON) |
		tvpm->txdc_state | tvpm->txchain_state;

	return ret_val;
}

/* ******************* Tx backoff control ***************************** */
/* Tx Baackoff values are different per band.
 * If the system is not band locked per slace additonal corrections may be required
 */
static int8
wlc_tvpm_calc_pwrbackoff(wlc_tvpm_info_t *tvp, unsigned temp_state, unsigned vbat_state)
{
	chanspec_t chanspec = tvp->wlc->chanspec;
	int8 txbo = 0;

	/* find appropriate backoff value based on index to the table and per band table */
	txbo = CHSPEC_IS2G(chanspec)
		? tvp->config->pwrbackoff_config_2g[temp_state][vbat_state]
		: tvp->config->pwrbackoff_config_5g[temp_state][vbat_state];

	return (txbo);
}

static int
wlc_tvpm_update_pwrbackoff_vbat_temp(wlc_tvpm_info_t *tvp, wlc_tvpm_input_t *input)
{
	wlc_tvpm_config_t *conf = tvp->config;

	unsigned idx_temp = 0,
	         idx_vbat = 0,
	         idx, size;
	int8 txpwrbo = 0;

	if (!PHY_VALID_TEMP(tvp->input->temp) || !PHY_VALID_VBAT(tvp->input->vbat)) {
		WL_ERROR(("wl%u: bad temperature or Vbat reading T=%d V=%u\n",
			WLCWLUNIT(tvp->wlc), tvp->input->temp, tvp->input->vbat));
		return BCME_OK;
	}

	/* Check if anything changed */
	if (tvp->input->temp == input->temp &&
		tvp->input->vbat == input->vbat)
		return BCME_OK;

	/* find corresponding table entry */
	for (idx = 0, size = ARRAYSIZE(conf->pwrbackoff_vbat);
			idx < size; idx++) {
		if (input->vbat >= conf->pwrbackoff_vbat[idx] ||
			conf->pwrbackoff_vbat[idx] <= PHY_VBAT_MIN) {
			idx_vbat = idx;
			break;
		}
	}

	for (idx = 0, size = ARRAYSIZE(conf->pwrbackoff_temp);
			idx < size; idx++) {
		if (input->temp >= conf->pwrbackoff_temp[idx] ||
			conf->pwrbackoff_temp[idx] <= PHY_TEMP_MIN) {
			idx_temp = idx;
			break;
		}
	}
	ASSERT(idx_vbat < ARRAYSIZE(conf->pwrbackoff_vbat) &&
		idx_temp < ARRAYSIZE(conf->pwrbackoff_temp));

	/* update required txbackoff value if required */
	txpwrbo = wlc_tvpm_calc_pwrbackoff(tvp, idx_temp, idx_vbat);
	if (txpwrbo != tvp->txpwr_backoff) {
		tvp->tx_bo = txpwrbo;
		return 1;
	}

	return BCME_OK;
}

static int
wlc_tvpm_update_pwrbackoff(wlc_tvpm_info_t *tvpm, wlc_tvpm_input_t *input)
{
	wlc_info_t *wlc = tvpm->wlc;
	int ret_val = 0, rc = BCME_OK;

	/* Check if tx backoff is enabled */
	if (!tvpm->txpwrbackoff_config) {
		return ret_val;
	}

	ret_val += wlc_tvpm_update_pwrbackoff_vbat_temp(tvpm, input);
	if (ret_val) {
		/* set tx power backoff here */
#ifdef TXPWRBACKOFF
		rc = phy_tpc_txbackoff_set(WLC_PI(tvpm->wlc), tvpm->tx_bo);
#endif /* TXPWRBACKOFF */
		if (rc != BCME_OK) {
			WL_ERROR(("wl%d: %s: Failed (%d)to set txbackoff=%d",
				wlc->pub->unit, __FUNCTION__, rc, tvpm->tx_bo));
			return 0;
		}
		tvpm->txpwr_backoff = tvpm->tx_bo;
	}

	tvpm->txpwrbackoff_state = (tvpm->txpwr_backoff == TVPM_TXPWRBACKOFF_NONE)
		? tvpm->txpwrbackoff_state & ~WLC_TEMPTHROTTLE_ON
		: tvpm->txpwrbackoff_state |  WLC_TEMPTHROTTLE_ON;

	return (ret_val);
}

static void
wlc_tvpm_watchdog(void *ctx)
{
	wlc_tvpm_input_t input_new;
	wlc_tvpm_info_t *tvp  = (wlc_tvpm_info_t *)ctx;
	wlc_info_t *wlc  = tvp->wlc;
	int         ret  = 0;

	if (!TVPM_ENAB(wlc->pub) || !tvp->enable) {
		return;
	}
	if (tvp->config->monitor_period == 0) {
		return;
	}
	if (++tvp->monitor_cnt < tvp->config->monitor_period) {
		return;
	}
	tvp->monitor_cnt = 0;

	/* update internal inputs */
	memcpy((void *)&input_new, (void *)tvp->input, sizeof(input_new));
	wlc_tvpm_update_inputs(wlc, &input_new, TRUE);

	/* update mitigations */
	if (!wlc->lpas) {
		wlc_tvpm_update_txdc(tvp, &input_new);

		if ((wlc->pub->now - wlc->stf->tempsense_lasttime) >=
			wlc->stf->tempsense_period) {
			ret += wlc_tvpm_update_txchain(tvp, &input_new);
		}
	}

	if ((wlc->pub->now - wlc->stf->tempsense_lasttime) >=
		wlc->stf->tempsense_period) {
		ret += wlc_tvpm_update_pwrbackoff(tvp, &input_new);
	}

	/* Save last update time for restricted mitigations */
	if (ret) {
		wlc->stf->tempsense_lasttime = wlc->pub->now;
	}

	/* save inputs */
	tvp->input->temp = input_new.temp;
	tvp->input->vbat = input_new.vbat;
}

int
wlc_tvpm_update(wlc_tvpm_info_t *tvpm)
{
	/* External interface to invoke tvpm module update */
	wlc_tvpm_watchdog(tvpm);

	return BCME_OK;
}

int
wlc_tvpm_set_cltm(wlc_tvpm_info_t *tvpm, uint8 index)
{
	wlc_tvpm_input_t input_new;
	wlc_info_t *wlc = tvpm->wlc;

	if (!tvpm->enable) {
		return BCME_ERROR;
	}

	if (index == tvpm->input->cltm)
		return BCME_OK;

	if (!index || index > TVPM_PWR_IDX_VAL_MAX)
		return BCME_RANGE;

	// save value
	memcpy((void *)&input_new, (void *)tvpm->input, sizeof(input_new));
	input_new.cltm = index;

	if (!wlc->lpas) {
		wlc_tvpm_update_txdc(tvpm, &input_new);
	}

	tvpm->input->cltm = index;

	return BCME_OK;
}

int
wlc_tvpm_set_ppm(wlc_tvpm_info_t *tvpm, uint8 index)
{
	wlc_tvpm_input_t input_new;
	int ret = 0;
	wlc_info_t *wlc = tvpm->wlc;

	if (!tvpm->enable) {
		return BCME_ERROR;
	}

	if (index == tvpm->input->ppm)
		return BCME_OK;

	if (!index || index > TVPM_PWR_IDX_VAL_MAX)
		return BCME_RANGE;

	// save value
	memcpy((void *)&input_new, (void *)tvpm->input, sizeof(input_new));
	input_new.ppm = index;

	if (!wlc->lpas) {
		wlc_tvpm_update_txdc(tvpm, &input_new);

		if ((wlc->pub->now - wlc->stf->tempsense_lasttime) >= wlc->stf->tempsense_period) {
			ret += wlc_tvpm_update_txchain(tvpm, &input_new);

			/* Save last update time for restricted mitigations */
			if (ret) {
				wlc->stf->tempsense_lasttime = wlc->pub->now;
			}
		}
	}

	tvpm->input->ppm = index;

	return BCME_OK;
}

uint8
wlc_tvpm_set_target_txdc(wlc_tvpm_info_t *tvpm, uint8 value)
{
	return wlc_tvpm_set_txdc(tvpm, TVPM_INP_MAX, value);
}

uint8
wlc_tvpm_get_target_txdc(wlc_tvpm_info_t *tvpm)
{
	return wlc_tvpm_get_txdc(tvpm, TVPM_INP_MAX);
}

uint8
wlc_tvpm_set_txdc(wlc_tvpm_info_t *tvpm, uint8 idx, uint8 val)
{
	uint8 final;

	if ((!val || val > NO_DUTY_THROTTLE) || (idx > TVPM_INP_MAX)) {
		return NO_DUTY_THROTTLE;
	}

	ASSERT(tvpm);

	final = MAX(MIN_DUTY_CYCLE_ALLOWED, val);

	/* save final DC value */
	if (idx == TVPM_INP_MAX) {
		tvpm->txduty_cycle = final;
	} else {
		tvpm->tx_dc[idx] = final;
	}

	return final;
}

uint8
wlc_tvpm_get_txdc(wlc_tvpm_info_t *tvpm, uint8 idx)
{
	 if (idx > TVPM_INP_MAX)
		 return NO_DUTY_THROTTLE;

	ASSERT(tvpm);

	if (idx == TVPM_INP_MAX) {
		return (uint8)tvpm->txduty_cycle;
	} else {
		return tvpm->tx_dc[idx];
	}
}

uint8
wlc_tvpm_get_txdc_min(wlc_tvpm_info_t *tvpm)
{
	uint8    txdc = NO_DUTY_THROTTLE;
	unsigned idx;

	for (idx = 0; idx < ARRAYSIZE(tvpm->tx_dc); idx++) {
		txdc = MIN(txdc, tvpm->tx_dc[idx]);
	}

	return txdc;
}

uint8
wlc_tvpm_get_txchain_min(wlc_tvpm_info_t *tvpm)
{
	uint8 txchain = TVPM_TXCHAIN_RANGE_HI;
	int idx;

	for (idx = 0; idx < ARRAYSIZE(tvpm->tx_chain); idx++) {
		txchain = MIN(txchain, tvpm->tx_chain[idx]);
	}

	return txchain;
}

#if defined(TVPM_DEBUG)
static void
bprint_uint8_array(struct bcmstrbuf *b, char *name, uint8 *array, int count)
{
	int i;

	bcm_bprintf(b, "%s = ", name);
	for (i = 0; i < count; i++) {
		bcm_bprintf(b, "%u ", array[i]);
	}
	bcm_bprintf(b, "\n");
}

static void
bprint_int16_array(struct bcmstrbuf *b, char *name, int16 *array, int count)
{
	int i;

	bcm_bprintf(b, "%s = ", name);
	for (i = 0; i < count; i++) {
		bcm_bprintf(b, "%d ", array[i]);
	}
	bcm_bprintf(b, "\n");
}

static void
bprint_int8_array(struct bcmstrbuf *b, char *name, int8 *array, int count)
{
	int i;

	bcm_bprintf(b, "%s = ", name);
	for (i = 0; i < count; i++) {
		bcm_bprintf(b, "%d ", array[i]);
	}
	bcm_bprintf(b, "\n");
}

static int
wlc_dump_tvpm(wlc_tvpm_info_t *tvpm, struct bcmstrbuf *b)
{
	wlc_tvpm_config_t* cfg = tvpm->config;
	wlc_tvpm_input_t* input = tvpm->input;
	wlc_info_t *wlc = tvpm->wlc;
	phy_txpwrbackoff_info_t txpwrbo;

	bcm_bprintf(b, "\nTVPM module dump:\n");

	bcm_bprintf(b, "========== wlc_tvpm_config_t ==========\n");
	bcm_bprintf(b, "monitor_period = %u\n", cfg->monitor_period);

	bcm_bprintf(b, "========== Tx duty cycle ==========\n");
	bprint_uint8_array(b, "txdc_vbat", cfg->txdc_vbat,
		ARRAYSIZE(cfg->txdc_vbat));
	bprint_int16_array(b, "txdc_temp", cfg->txdc_temp,
		ARRAYSIZE(cfg->txdc_temp));
	bprint_uint8_array(b, "txdc_cltm", cfg->txdc_cltm,
		ARRAYSIZE(cfg->txdc_cltm));
	bprint_uint8_array(b, "txdc_ppm", cfg->txdc_ppm,
		ARRAYSIZE(cfg->txdc_ppm));

	bprint_uint8_array(b, "txdc_config_temp_vbat",
		(uint8*)cfg->txdc_config_temp_vbat,
		sizeof(cfg->txdc_config_temp_vbat) /
		sizeof(cfg->txdc_config_temp_vbat[0][0]));
	bprint_uint8_array(b, "txdc_config_cltm", cfg->txdc_config_cltm,
		ARRAYSIZE(cfg->txdc_config_cltm));
	bprint_uint8_array(b, "txdc_config_ppm", cfg->txdc_config_ppm,
		ARRAYSIZE(cfg->txdc_config_ppm));

	bcm_bprintf(b, "========== Tx chains ==========\n");
	bprint_uint8_array(b, "txchain_ppm_threshold",
		cfg->txchain_ppm,
		ARRAYSIZE(cfg->txchain_ppm));
	bprint_uint8_array(b, "txchain_ppm_nchains",
		cfg->txchain_config_ppm,
		ARRAYSIZE(cfg->txchain_config_ppm));
	bprint_uint8_array(b, "txchain_vbat_threshold",
		cfg->txchain_vbat,
		ARRAYSIZE(cfg->txchain_vbat));
	bprint_uint8_array(b, "txchain_vbat_nchains",
		cfg->txchain_config_vbat,
		ARRAYSIZE(cfg->txchain_config_vbat));

	memset(&txpwrbo, 0, sizeof(txpwrbo));
#ifdef TXPWRBACKOFF
	phy_tpc_txbackoff_dump(WLC_PI(wlc), &txpwrbo);
#endif /* TXPWRBACKOFF */

	bcm_bprintf(b, "========== Tx power backoff %s [phy=%u]  ==========\n",
		tvpm->txpwrbackoff_config ? "Enabled" : "Disabled", txpwrbo.enable);
	bprint_uint8_array(b, "pwrbackoff_vbat",
		cfg->pwrbackoff_vbat,
		ARRAYSIZE(cfg->pwrbackoff_vbat));
	bprint_int16_array(b, "pwrbackoff_temp",
		cfg->pwrbackoff_temp,
		ARRAYSIZE(cfg->pwrbackoff_temp));
	bprint_int8_array(b, "pwrbackoff_config_2g (in .25dB units)",
		(int8 *)cfg->pwrbackoff_config_2g,
		sizeof(cfg->pwrbackoff_config_2g) / sizeof(cfg->pwrbackoff_config_2g[0][0]));
	bprint_int8_array(b, "pwrbackoff_config_5g (in .25dB units)",
		(int8 *)cfg->pwrbackoff_config_5g,
		sizeof(cfg->pwrbackoff_config_5g) / sizeof(cfg->pwrbackoff_config_5g[0][0]));

	bcm_bprintf(b, "========== wlc_tvpm_input_t ==========\n");
	bcm_bprintf(b, "temp = %d\n", input->temp);
	bcm_bprintf(b, "vbat = %u\n", input->vbat);
	bcm_bprintf(b, "cltm = %u\n", input->cltm);
	bcm_bprintf(b, "ppm  = %u\n", input->ppm);

	bcm_bprintf(b, "========== struct tvpm ==========\n");
	bcm_bprintf(b, "pub->_tvpm = %u\n", tvpm->wlc->pub->_tvpm);
	bcm_bprintf(b, "enable = %u\n", tvpm->enable);
	bcm_bprintf(b, "========== current mitigation ==========\n");
	bcm_bprintf(b, "txduty_cycle  = %u\n", tvpm->txduty_cycle);
	bcm_bprintf(b, "txpwr_backoff = %d [phy=%d](in .25dB units)\n",
		tvpm->txpwr_backoff, txpwrbo.vbat_tempsense_pwrbackoff);
	bcm_bprintf(b, "tx_chains     = %u [HW=%u]\n",
		tvpm->tx_active_chains, WLC_BITSCNT(wlc->stf->txchain));
	bcm_bprintf(b, "========== internal info ==========\n");
	bprint_uint8_array(b, "tx_dc   []", tvpm->tx_dc,
		ARRAYSIZE(tvpm->tx_dc));
	bprint_uint8_array(b, "tx_chain[]", tvpm->tx_chain,
		ARRAYSIZE(tvpm->tx_chain));
	bcm_bprintf(b, "\tconfig:\ntxdc_config=%#x  txchain_config=%#x txpwrbackoff_config=%#x\n",
		tvpm->txdc_config, tvpm->txchain_config, tvpm->txpwrbackoff_config);
	bcm_bprintf(b, "\tstates:\ntxdc_state =%#x  txchain_state =%#x txpwrbackoff_state =%#x\n",
		tvpm->txdc_state, tvpm->txchain_state, tvpm->txpwrbackoff_state);

	return BCME_OK;
}
#endif // endif

/* iovar table */
enum {
	IOV_TVPM,
	IOV_TVPM_LAST
};

static const bcm_iovar_t tvpm_iovars[] = {
	{"tvpm", IOV_TVPM,
	0, 0, IOVT_BUFFER, 0
	},
	{NULL, 0, 0, 0, 0, 0}
};

static int
wlc_tvpm_iov_get(wlc_tvpm_info_t *tvpm, wl_tvpm_req_t *req, uint reqlen, void *a, uint alen)
{
	int32 *ret_int_ptr = (int32 *)a;
	int ret = BCME_OK;

	if (reqlen < sizeof(wl_tvpm_req_t)) {
		ret = BCME_BUFTOOSHORT;
		goto done;
	}
	if (alen < sizeof(*ret_int_ptr)) {
		ret = BCME_BUFTOOSHORT;
		goto done;
	}
	/* TODO: check if req->version > TVPM_REQ_CURRENT_VERSION */

	switch (req->req_type) {
		case WL_TVPM_REQ_CLTM_INDEX:
			*ret_int_ptr = tvpm->input->cltm;
			break;
		case WL_TVPM_REQ_PPM_INDEX:
			*ret_int_ptr = tvpm->input->ppm;
			break;
		case WL_TVPM_REQ_ENABLE:
			*ret_int_ptr = tvpm->enable;
			break;
		case WL_TVPM_REQ_STATUS:
		{
			wl_tvpm_status_t status;
			if (alen < sizeof(status)) {
				ret = BCME_BUFTOOSHORT;
			} else {
				bzero(&status, sizeof(status));
				status.enable            = tvpm->enable;
				status.tx_dutycycle      = tvpm->txduty_cycle;
				status.tx_power_backoff  = tvpm->txpwr_backoff;
				status.num_active_chains = tvpm->tx_active_chains;
				status.temp = tvpm->input->temp;
				status.vbat = tvpm->input->vbat;
				memcpy(a, &status, sizeof(status));
			}
			break;
		}
		default:
			ret = BCME_BADARG;
			break;
	}
done:
	return ret;
}

static int
wlc_tvpm_iov_set(wlc_tvpm_info_t *tvpm, wl_tvpm_req_t *req, uint reqlen, void *a, uint alen)
{
	int32 int_val = 0;
	int ret = BCME_OK;
	bool bool_val;

	if (reqlen < sizeof(wl_tvpm_req_t)) {
		ret = BCME_BUFTOOSHORT;
		goto done;
	}
	/* TODO: check if req->version > TVPM_REQ_CURRENT_VERSION */

	memcpy(&int_val, (uint32*)req->value, sizeof(int_val));
	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (req->req_type) {
		case WL_TVPM_REQ_CLTM_INDEX:
			ret = wlc_tvpm_set_cltm(tvpm, (uint8)int_val);
			break;
		case WL_TVPM_REQ_PPM_INDEX:
			ret = wlc_tvpm_set_ppm(tvpm, (uint8)int_val);
			break;
		case WL_TVPM_REQ_ENABLE:
			ret = wlc_tvpm_enable(tvpm, bool_val);
			break;
		default:
			ret = BCME_BADARG;
			break;
	}
done:
	return ret;
}

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static int
wlc_tvpm_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_tvpm_info_t *tvpm = (wlc_tvpm_info_t *)hdl;
	int err = 0;

	switch (actionid) {
		case IOV_GVAL(IOV_TVPM):
			err = wlc_tvpm_iov_get(tvpm, (wl_tvpm_req_t*)p, plen, a, alen);
			break;
		case IOV_SVAL(IOV_TVPM):
			err = wlc_tvpm_iov_set(tvpm, p, plen, a, alen);
			break;
		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

/* Read a nvram string containing a voltage/temperature 2-D array of values */
static int
BCMATTACHFN(wlc_tvpm_getvar_2d_array)(wlc_info_t* wlc, const char* name,
	void* dest_array, uint dest_rows, uint dest_columns)
{
	int dest_size = dest_rows * dest_columns;
	int arraysz;

	/* Read the 2-D array as if it was a 1-D array in memory */
	arraysz = get_uint8_vararray_slicespecific(wlc->osh, wlc->pub->vars,
		wlc_hw_get_vars_table(wlc), name, dest_array, dest_size);
	if (arraysz == BCME_NOTFOUND) {
		dbg_tvpm_getvar_err = BCME_NOTFOUND;
		dbg_tvpm_getvar_rstr = name;
		goto done;
	}

	/* Assert if nvram did not provide exactly the expected number of values */
	ASSERT(arraysz == dest_size);
done:
	return arraysz;
}

/* Read a nvram string containing an array of pairs of values */
static int
BCMATTACHFN(wlc_tvpm_getvar_pair_array)(wlc_info_t* wlc, const char* name,
	void* dest_array1, void *dest_array2, uint dest_max_pairs)
{
	int i;
	int array_size;
	int dest_size = dest_max_pairs * 2;
	int err = BCME_OK;
	int prefixed_name_sz;
	char *prefixed_name = NULL;
	const char *new_name;

	dbg_tvpm_getvar_rstr = name;
	prefixed_name_sz = get_slicespecific_var_name(wlc->osh, wlc_hw_get_vars_table(wlc),
		name, &prefixed_name);
	if (prefixed_name_sz == 0) {
		dbg_tvpm_getvar_err = BCME_NOMEM;
		return BCME_NOMEM;
	}

	new_name = prefixed_name;
	if (getvar(wlc->pub->vars, new_name) == NULL) {
		/* Try again without the slice prefix in the name */
		new_name = name;
		if (getvar(wlc->pub->vars, new_name) == NULL) {
			err = BCME_NOTFOUND;
			goto done;
		}
	}

	array_size = getintvararraysize(wlc->pub->vars, new_name);
	if (array_size > dest_size) {
		err = BCME_BUFTOOSHORT;
		ASSERT(array_size <= dest_size);
		goto done;
	}

	/* limit the initialization to the size of the nvram array */
	array_size = MIN(array_size, dest_size);

	/* Proceed only if there is an even number of elements */
	if ((array_size & 0x1) != 0) {
		err = BCME_BADLEN;
		ASSERT((array_size & 0x1) == 0);
		goto done;
	}

	/* load the 2 destination arrays with the nvram value pairs */
	for (i = 0; i < array_size/2; i++) {
		((uint8*)dest_array1)[i] =
			(uint8)getintvararray(wlc->pub->vars, new_name, i*2);
		((uint8*)dest_array2)[i] =
			(uint8)getintvararray(wlc->pub->vars, new_name, i*2+1);
	}
	dbg_tvpm_getvar_rstr = NULL;
done:
	MFREE(wlc->osh, prefixed_name, prefixed_name_sz);
	dbg_tvpm_getvar_err = err;
	return err;
}

/* Read in TVPM nvram parameters */
static int
BCMATTACHFN(wlc_tvpm_nvram_init)(wlc_info_t *wlc, wlc_tvpm_info_t *tvpm)
{
	wlc_tvpm_config_t* cfg = tvpm->config;
	int ret = BCME_ERROR;

	/* Feature enable */
	if (getvar(wlc->pub->vars, rstr_tvpm) == NULL) {
		ret = BCME_NOTFOUND;
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm;
		goto end;
	}
	tvpm->enable = (bool)getintvar_slicespecific(wlc->hw, wlc->pub->vars, rstr_tvpm);

	/* tvpm monitor period
	 * run updates every monitor period
	 */
	if (getvar(wlc->pub->vars, rstr_tvpm_monitor_period) == NULL) {
		ret = BCME_NOTFOUND;
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_monitor_period;
		goto end;
	}
	cfg->monitor_period = (uint16)getintvar_slicespecific(wlc->hw, wlc->pub->vars,
		rstr_tvpm_monitor_period);
	if (tvpm->config->monitor_period == 0) {
		tvpm->config->monitor_period = 1;
	}

	/* Tx duty cycle value per CLTM subrange, up to 10 pairs.
	 * tvpm_dc_cltm=<threshold1, value1>, <threshold2, value2>, ...
	 * eg. tvpm_dc_cltm=100,100,75,80,50,50,1,25
	 *     covers the complete range [1:100] in descending order.
	 */
	ret = wlc_tvpm_getvar_pair_array(wlc, rstr_tvpm_dc_cltm,
		cfg->txdc_cltm, cfg->txdc_config_cltm,
		ARRAYSIZE(cfg->txdc_config_cltm) - 1);
	if (ret < 0) {
		goto end;
	}

	/* Tx duty cycle value per PPM subrange, up to 10 pairs.
	 * tvpm_dc_ppm=<threshold1, value1>, <threshold2, value2>, ...
	 * eg. tvpm_dc_cltm=100,100,70,80,40,50,1,25
	 *     covers the complete range [1:100] in descending order.
	 */
	ret = wlc_tvpm_getvar_pair_array(wlc, rstr_tvpm_dc_ppm,
		cfg->txdc_ppm, cfg->txdc_config_ppm,
		ARRAYSIZE(cfg->txdc_config_ppm) - 1);
	if (ret < 0) {
		goto end;
	}

	/* Temperature threshold.  eg. tvpm_dc_temp_threshold=50 */
	ret = get_int16_vararray_slicespecific(wlc->osh, wlc->pub->vars,
		wlc_hw_get_vars_table(wlc), rstr_tvpm_dc_temp_threshold,
		cfg->txdc_temp, ARRAYSIZE(cfg->txdc_temp));
	if (ret < 0) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_dc_temp_threshold;
		goto end;
	}

	/* Vbat thresholds in units of 0.1V.  eg. tvpm_dc_vbat_threshold=33,30 */
	ret = get_uint8_vararray_slicespecific(wlc->osh, wlc->pub->vars,
		wlc_hw_get_vars_table(wlc), rstr_tvpm_dc_vbat_threshold,
		cfg->txdc_vbat, ARRAYSIZE(cfg->txdc_vbat));
	if (ret < 0) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_dc_vbat_threshold;
		goto end;
	}

	/* 2-D Tx duty cycle table as a fn of Vbat and Temperature.
	 * eg. tvpm_dc_vbat_temp=100,80,30,80,50,30
	 * Value ordering in all Voltage/Temperature tables:
	 *               Temp >= Threshold  Temp < Threshold
	 *               -----------------  ----------------
	 * V >= V1:        1                  4
	 * V2 <= V < V1:   2                  5
	 * V < V2:         3                  6
	 */
	ret = wlc_tvpm_getvar_2d_array(wlc, rstr_tvpm_dc_vbat_temp,
		cfg->txdc_config_temp_vbat, TVPM_TXDC_IDX_TEMP_MAX, TVPM_TXDC_IDX_VBAT_MAX);
	if (ret < 0) {
		goto end;
	}

	/*
	 * Read NVRAM Tx chain reduction configuration:
	 */

	/* tx chain number value per PPM subrange:
	 * tvpm_txchain_ppm= <threshold1,value1>,<threshold2,value2>,...
	 * eg. tvpm_txchain_ppm=90,3,71,2,0,1
	 */
	ret = wlc_tvpm_getvar_pair_array(wlc, rstr_tvpm_txchain_ppm,
		cfg->txchain_ppm, cfg->txchain_config_ppm,
		ARRAYSIZE(cfg->txchain_ppm) - 1);
	if (ret < 0) {
		goto end;
	}

	/* tx chain number value per voltage subrange
	 * tvpm_txchain_vbat= <threshold1, value1>, <threshold2, value2>,...
	 * eg. tvpm_txchain_vbat=33,3,30,2,0,1
	 */
	ret = wlc_tvpm_getvar_pair_array(wlc, rstr_tvpm_txchain_vbat,
		cfg->txchain_vbat, cfg->txchain_config_vbat,
		ARRAYSIZE(cfg->txchain_vbat) - 1);
	if (ret < 0) {
		goto end;
	}

	/* Tx Power backoff temperature threshold. eg. tvpm_pwrboff_temp_threshold=50 */
	ret = get_int16_vararray_slicespecific(wlc->osh, wlc->pub->vars,
		wlc_hw_get_vars_table(wlc), rstr_tvpm_pwrboff_temp_threshold,
		cfg->pwrbackoff_temp, ARRAYSIZE(cfg->pwrbackoff_temp));
	if (ret < 0) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_pwrboff_temp_threshold;
		goto end;
	}

	/* Vbat thresholds in units of 0.1V.  eg. tvpm_pwrboff_vbat_threshold=33,30 */
	ret = get_uint8_vararray_slicespecific(wlc->osh, wlc->pub->vars,
		wlc_hw_get_vars_table(wlc), rstr_tvpm_pwrboff_vbat_threshold,
		cfg->pwrbackoff_vbat, ARRAYSIZE(cfg->pwrbackoff_vbat));
	if (ret < 0) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_pwrboff_vbat_threshold;
		goto end;
	}

	/* 2-D Tx power backoff table as a fn of Vbat and Temperature.
	 * eg.	tvpm_pwrboff_2g=3,2,4,0,1,2
	 *	tvpm_pwrboff_5g=3,2,4,0,1,2
	 * Value ordering in all Voltage/Temperature tables:
	 *               Temp >= Threshold  Temp < Threshold
	 *               -----------------  ----------------
	 * V >= V1:        1                  4
	 * V2 <= V < V1:   2                  5
	 * V < V2:         3                  6
	 */

	/* Read power backoff values per band */
	ret = wlc_tvpm_getvar_2d_array(wlc, rstr_tvpm_pwrboff_2g,
		cfg->pwrbackoff_config_2g, TVPM_TXBO_IDX_TEMP_MAX, TVPM_TXBO_IDX_VBAT_MAX);
	if (ret < 0) {
		goto end;
	}

	ret = wlc_tvpm_getvar_2d_array(wlc, rstr_tvpm_pwrboff_5g,
		cfg->pwrbackoff_config_5g, TVPM_TXBO_IDX_TEMP_MAX, TVPM_TXBO_IDX_VBAT_MAX);
	if (ret < 0) {
		goto end;
	}

	ret = BCME_OK;

end:
	return ret;
}

/* Get the last value in a variable-length null-terminated array */
static uint8
BCMATTACHFN(get_array_last_value)(uint8 *array, int array_size, int *last_idx)
{
	int i;
	uint8 val = 0;
	uint8 prev = 0;

	for (i = 0; i < array_size; i++) {
		val = array[i];
		if (val == 0) {
			/* Found null terminator */
			val = prev;
			break;
		}
		prev = val;
	}
	*last_idx = i - 1;
	dbg_last_idx = *last_idx;
	dbg_last_val = val;
	return val;
}

static int
BCMATTACHFN(verify_ordered_array_no_zero_term)(uint8 *array, int array_size,
	int range_lo, int range_hi)
{
	int ret = BCME_OK;
	int i;
	int val = 0;
	int prev_val = 0;

	/* Check that:
	 * - values are in strict descending order.
	 * - values are within the valid range.
	 */
	for (i = 0; i < array_size; i++) {
		val = (int)array[i];

		if (i > 0 && val >= prev_val) {
			/* array is not in descending order */
			ret = BCME_BADOPTION;
			break;
		}
		prev_val = val;

		if (val < range_lo || val > range_hi) {
			/* array value out of range */
			ret = BCME_RANGE;
			break;
		}
	}
	return ret;
}

/* Validate an unordered, non-zero-terminated uint8 configuration array */
static int
BCMATTACHFN(verify_unordered_array_no_zero_term)(uint8 *array, int array_size,
	uint8 range_lo, uint8 range_hi)
{
	int ret = BCME_OK;
	int i;
	uint8 val = 0;

	for (i = 0; i < array_size; i++) {
		val = array[i];
		if (val < range_lo || val > range_hi) {
			ret = BCME_RANGE;
			break;
		}
	}
	return ret;
}

/* Validate all configuration parameters */
static int
BCMATTACHFN(wlc_tvpm_verify_cfg)(wlc_tvpm_info_t *tvpm)
{
	wlc_tvpm_config_t* cfg = tvpm->config;
	int ret, i, j;
	uint8 last_val;
	int last_idx = 0;
	int last_idx1 = 0;
	int last_idx2 = 0;

	ret = verify_array_values((uint8*)cfg->txdc_config_temp_vbat,
		(TVPM_TXDC_IDX_TEMP_MAX * TVPM_TXDC_IDX_VBAT_MAX *
		sizeof(cfg->txdc_config_temp_vbat[0][0])),
		TVPM_DUTYCYCLE_RANGE_LO, TVPM_DUTYCYCLE_RANGE_HI, FALSE);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_config_temp_vbat);
		goto done;
	}

	ret = verify_ordered_array_uint8(cfg->txdc_cltm, ARRAYSIZE(cfg->txdc_cltm),
		TVPM_PWRIDX_RANGE_LO, TVPM_PWRIDX_RANGE_HI);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_cltm);
		goto done;
	}
	last_val = get_array_last_value(cfg->txdc_cltm, ARRAYSIZE(cfg->txdc_cltm), &last_idx);
	if (last_val != TVPM_PWRIDX_RANGE_LO) {
		ret = BCME_NOTFOUND;
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_cltm) + 1;
		goto done;
	}
	ret = verify_array_values((uint8*)cfg->txdc_config_cltm, last_idx + 1,
		TVPM_DUTYCYCLE_RANGE_LO, TVPM_DUTYCYCLE_RANGE_HI, FALSE);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_config_cltm);
		goto done;
	}

	ret = verify_ordered_array_uint8(cfg->txdc_ppm, ARRAYSIZE(cfg->txdc_ppm),
		TVPM_PWRIDX_RANGE_LO, TVPM_PWRIDX_RANGE_HI);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_ppm);
		goto done;
	}
	last_val = get_array_last_value(cfg->txdc_ppm, ARRAYSIZE(cfg->txdc_ppm), &last_idx);
	if (last_val != TVPM_PWRIDX_RANGE_LO) {
		ret = BCME_NOTFOUND;
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_ppm) + 1;
		goto done;
	}
	ret = verify_array_values((uint8*)cfg->txdc_config_ppm, last_idx + 1,
		TVPM_DUTYCYCLE_RANGE_LO, TVPM_DUTYCYCLE_RANGE_HI, FALSE);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_config_ppm);
		goto done;
	}

	ret = verify_ordered_array_int16(cfg->txdc_temp, ARRAYSIZE(cfg->txdc_temp),
		PHY_TEMP_MIN, PHY_TEMP_MAX);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_temp);
		goto done;
	}

	ret = verify_ordered_array_uint8(cfg->txdc_vbat, ARRAYSIZE(cfg->txdc_vbat),
		PHY_VBAT_MIN, PHY_VBAT_MAX);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, txdc_vbat);
		goto done;
	}

	ret = verify_ordered_array_uint8(cfg->txchain_ppm,
		ARRAYSIZE(cfg->txchain_ppm),
		TVPM_PWRIDX_RANGE_LO, TVPM_PWRIDX_RANGE_HI);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_txchain_ppm;
		goto done;
	}
	last_val = get_array_last_value(cfg->txchain_ppm, ARRAYSIZE(cfg->txchain_ppm), &last_idx);
	if (last_val != TVPM_PWRIDX_RANGE_LO) {
		ret = BCME_BADARG;
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_txchain_ppm;
		goto done;
	}
	ret = verify_unordered_array_no_zero_term(cfg->txchain_config_ppm, last_idx + 1,
		TVPM_TXCHAIN_RANGE_LO, TVPM_TXCHAIN_RANGE_HI);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_txchain_ppm;
		goto done;
	}

	/* Find txchain vbat last array index */
	(void) get_array_last_value(cfg->txchain_vbat,
		ARRAYSIZE(cfg->txchain_vbat), &last_idx1);
	(void) get_array_last_value(cfg->txchain_config_vbat,
		ARRAYSIZE(cfg->txchain_config_vbat), &last_idx2);
	last_idx = (last_idx1 > last_idx2) ? last_idx1 : last_idx2;
	if (last_idx == -1) {
		ret = BCME_BADLEN;
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_txchain_vbat;
		goto done;
	}
	/* Check the txchain vbat thresholds are in descending order and in range */
	ret = verify_ordered_array_no_zero_term(cfg->txchain_vbat, last_idx + 1,
		PHY_VBAT_MIN, PHY_VBAT_MAX);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_txchain_vbat;
		goto done;
	}
	/* Check the last vbat threshold is set to the lowest possible threshold value */
	last_val = cfg->txchain_vbat[last_idx];
	if (last_val != PHY_VBAT_MIN) {
		ret = BCME_BADARG;
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_txchain_vbat;
		goto done;
	}
	/* Check the range in the value array */
	ret = verify_unordered_array_no_zero_term(cfg->txchain_config_vbat, last_idx + 1,
		TVPM_TXCHAIN_RANGE_LO, TVPM_TXCHAIN_RANGE_HI);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = ret;
		dbg_tvpm_getvar_rstr = rstr_tvpm_txchain_vbat;
		goto done;
	}

	/* Validate nvram values of pwrbackoff_2g/5g are in range */
	ret = verify_array_values((uint8*)cfg->pwrbackoff_config_2g,
		(TVPM_TXBO_IDX_TEMP_MAX * TVPM_TXBO_IDX_VBAT_MAX *
		sizeof(cfg->pwrbackoff_config_2g[0][0])),
		TXPWRBACKOFF_RANGE_LO, TXPWRBACKOFF_RANGE_HI, FALSE);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, pwrbackoff_config_2g);
		goto done;
	}

	ret = verify_array_values((uint8*)cfg->pwrbackoff_config_5g,
		(TVPM_TXBO_IDX_TEMP_MAX * TVPM_TXBO_IDX_VBAT_MAX *
		sizeof(cfg->pwrbackoff_config_5g[0][0])),
		TXPWRBACKOFF_RANGE_LO, TXPWRBACKOFF_RANGE_HI, FALSE);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, pwrbackoff_config_5g);
		goto done;
	}

	for (i = 0; i < TVPM_TXBO_IDX_TEMP_MAX; i++) {
		for (j = 0; j < TVPM_TXBO_IDX_VBAT_MAX; j++) {
			cfg->pwrbackoff_config_2g[i][j] = -1 * cfg->pwrbackoff_config_2g[i][j];
			cfg->pwrbackoff_config_5g[i][j] = -1 * cfg->pwrbackoff_config_5g[i][j];
		}
	}

	/* Validate nvram values of pwrbackoff_temp & pwrbackoff_vbat */
	ret = verify_ordered_array_int16(cfg->pwrbackoff_temp, ARRAYSIZE(cfg->pwrbackoff_temp),
		PHY_TEMP_MIN, PHY_TEMP_MAX);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, pwrbackoff_temp);
		goto done;
	}

	ret = verify_ordered_array_uint8(cfg->pwrbackoff_vbat, ARRAYSIZE(cfg->pwrbackoff_vbat),
		PHY_VBAT_MIN, PHY_VBAT_MAX);
	if (ret != BCME_OK) {
		dbg_tvpm_getvar_err = OFFSETOF(wlc_tvpm_config_t, pwrbackoff_vbat);
		goto done;
	}

done:
	return ret;
}

static int
BCMUNINITFN(wlc_tvpm_up)(void *hdl)
{
	wlc_tvpm_info_t *tvpm = (wlc_tvpm_info_t *)hdl;
	BCM_REFERENCE(tvpm);

	if (tvpm->enable && tvpm->txpwrbackoff_config) {
#ifdef TXPWRBACKOFF
		if (!phy_tpc_txbackoff_enable(WLC_PI(tvpm->wlc), tvpm->txpwrbackoff_config)) {
			WL_ERROR(("wl%d: TVPM failed to enable TX Backoff\n",
				tvpm->wlc->pub->unit));
			tvpm->txpwrbackoff_config = 0;
		}
#endif /* TXPWRBACKOFF */
	}

	return BCME_OK;
}

static int
BCMUNINITFN(wlc_tvpm_down)(void *hdl)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	BCM_REFERENCE(wlc);
	return BCME_OK;
}

static void
wlc_tvpm_free(wlc_tvpm_info_t *tvpm)
{
#ifndef DONGLEBUILD /* In dongle builds, attach-time mallocs cannot be freed */
	wlc_info_t *wlc;

	if (!tvpm) {
		return;
	}
	wlc = tvpm->wlc;
	if (tvpm->input) {
		MFREE(wlc->osh, tvpm->input, sizeof(*tvpm->input));
	}
	if (tvpm->config) {
		MFREE(wlc->osh, tvpm->config, sizeof(*tvpm->config));
	}
	MFREE(wlc->osh, tvpm, sizeof(wlc_tvpm_info_t));
#endif /* DONGLEBUILD */
}

wlc_tvpm_info_t *
BCMATTACHFN(wlc_tvpm_alloc)(wlc_info_t *wlc)
{
	wlc_tvpm_info_t *tvpm;

	/* check if already allocated */
	if (wlc->tvpm) {
		return (wlc->tvpm);
	}

	tvpm = (wlc_tvpm_info_t*) MALLOCZ(wlc->osh, sizeof(wlc_tvpm_info_t));
	if (!tvpm) {
		WL_ERROR(("wl%d: tvpm alloc failed\n", wlc->pub->unit));
		goto fail;
	}
	tvpm->wlc = wlc;

	tvpm->config = (wlc_tvpm_config_t*) MALLOCZ(wlc->osh, sizeof(wlc_tvpm_config_t));
	if (!tvpm->config) {
		WL_ERROR(("wl%d: tvpm alloc failed\n", wlc->pub->unit));
#ifdef TVPM_DEBUG
		printf("====failed allocate tvpm->config\n");
#endif /* TVPM_DEBUG */
		goto fail;
	}

	tvpm->input = (wlc_tvpm_input_t*) MALLOCZ(wlc->osh, sizeof(wlc_tvpm_input_t));
	if (!tvpm->input) {
		WL_ERROR(("wl%d: tvpm alloc failed\n", wlc->pub->unit));
		goto fail;
	}

	return tvpm;
fail:
	wlc_tvpm_free(tvpm);
	return NULL;
}

wlc_tvpm_info_t *
BCMATTACHFN(wlc_tvpm_attach)(wlc_info_t *wlc)
{
	wlc_tvpm_info_t *tvpm = NULL;
	wlc_tvpm_config_t* cfg;
	int idx, count, zero_count;

	tvpm = wlc_tvpm_alloc(wlc);
	if (!tvpm) {
		dbg_tvpm_getvar_err = BCME_NORESOURCE;
		WL_ERROR(("wl%d: %s failed err=%d\n",
			wlc->pub->unit, __FUNCTION__, dbg_tvpm_getvar_err));
		goto fail;
	}

	dbg_tvpm = tvpm;
	wlc->tvpm = tvpm;
	cfg = tvpm->config;

#ifdef TVPM_DEBUG
	WL_PRINT(("DBG: wlc=%p, wlc->tvpm=%p[%p]\n", wlc, wlc->tvpm, tvpm));
#endif /* TVPM_DEBUG */

	/* Set configuration defaults */
	wlc->pub->_tvpm = TRUE;

	for (idx = 0; idx < ARRAYSIZE(tvpm->config->txdc_temp); idx++) {
		cfg->txdc_temp[idx] = PHY_TEMP_MIN;
	}

	for (idx = 0; idx < TVPM_INP_MAX; idx++) {
		/* Duty cycle 100% == "not using dc" is a default value */
		tvpm->tx_dc[idx] = NO_DUTY_THROTTLE;
	}

	for (idx = 0; idx < TVPM_INP_MAX; idx++) {
		/* Init with Max number of tx chains */
		tvpm->tx_chain[idx] = TVPM_TXCHAIN_RANGE_HI;
	}

	for (idx = 0; idx < ARRAYSIZE(tvpm->config->pwrbackoff_temp); idx++) {
		cfg->pwrbackoff_temp[idx] = PHY_TEMP_MIN;
	}

	/* Init power backoff defaults to invalid values to ensure nvram populates it correctly */
	memset(cfg->pwrbackoff_config_2g, TXPWRBACKOFF_RANGE_HI + 1,
		sizeof(cfg->pwrbackoff_config_2g));
	memset(cfg->pwrbackoff_config_5g, TXPWRBACKOFF_RANGE_HI + 1,
		sizeof(cfg->pwrbackoff_config_5g));

	tvpm->txduty_cycle  = NO_DUTY_THROTTLE;
	tvpm->txpwr_backoff = TVPM_NO_TXBO;
	tvpm->tx_active_chains = TVPM_TXCHAIN_RANGE_HI;

	/* keep for compatibility */
	wlc_tvpm_set_txdc(tvpm, TVPM_INP_PWR, NO_DUTY_THROTTLE);

	/* Read and verify nvram parameters */
	tvpm->err = wlc_tvpm_nvram_init(wlc, tvpm);
	if (tvpm->err != BCME_OK) {
		WL_ERROR(("wl%d: incomplete TVPM nvram %d\n", wlc->pub->unit, tvpm->err));
		goto fail;
	}
	tvpm->err = wlc_tvpm_verify_cfg(tvpm);
	if (tvpm->err != BCME_OK) {
		WL_ERROR(("wl%d: invalid TVPM nvram %d\n", wlc->pub->unit, tvpm->err));
		goto fail;
	}

	/* mitigation config indexes should be enabled in tvpm if appropriate VALID tables
	 * are available from NVRAM.
	 *  Example: tvpm->txdc_config bits WLC_CLTMTHROTTLE_ON & WLC_PPMTHROTTLE_ON)
	 *  Fill initial values for indexes here
	 *  Correspondign bits should be turned on only if all related configs are in place
	 */
	tvpm->txdc_config |= WLC_TEMPTHROTTLE_ON;
	tvpm->txdc_config |= WLC_CLTMTHROTTLE_ON;
	tvpm->txdc_config |= WLC_PPMTHROTTLE_ON;

	tvpm->txchain_config |= WLC_VBATTHROTTLE_ON;
	tvpm->txchain_config |= WLC_PPMTHROTTLE_ON;

	tvpm->txpwrbackoff_config |= WLC_TEMPTHROTTLE_ON; /* inputs enabled for tx power */

	tvpm->input->cltm = TVPM_PWR_IDX_VAL_DEF;
	tvpm->input->ppm  = TVPM_PWR_IDX_VAL_DEF;
	tvpm->input->temp = PHY_TEMP_MIN;
	tvpm->input->vbat = PHY_VBAT_MAX;

	/* If all tvpm_dc_vbat_temp nvram duty cycle values are 100% then clear the
	 * WLC_TEMPTHROTTLE_ON bit to disable this type of mitigation.
	 */
	count = array_value_mismatch_count(TVPM_DUTYCYCLE_RANGE_HI,
		&cfg->txdc_config_temp_vbat[0][0],
		TVPM_TXDC_IDX_TEMP_MAX * TVPM_TXDC_IDX_VBAT_MAX);
	if (count == 0) {
		tvpm->txdc_config &= ~WLC_TEMPTHROTTLE_ON;
	}

	/* If all tvpm_dc_cltm nvram duty cycle values are 100% then clear the
	 * WLC_CLTMTHROTTLE_ON bit to disable this type of mitigation.
	 */
	count = array_value_mismatch_count(TVPM_DUTYCYCLE_RANGE_HI,
		cfg->txdc_config_cltm, ARRAYSIZE(cfg->txdc_config_cltm) - 1);
	if (count == 0) {
		tvpm->txdc_config &= ~WLC_CLTMTHROTTLE_ON;
	}

	/* If all tvpm_dc_ppm nvram duty cycle values are 100% then clear the
	 * WLC_PPMTHROTTLE_ON bit to disable this type of mitigation.
	 */
	count = array_value_mismatch_count(TVPM_DUTYCYCLE_RANGE_HI,
		cfg->txdc_config_ppm, ARRAYSIZE(cfg->txdc_config_ppm) - 1);
	if (count == 0) {
		tvpm->txdc_config &= ~WLC_PPMTHROTTLE_ON;
	}

	/* If all tvpm_txchain_vbat nvram txchain values are max #chains then clear the
	 * WLC_VBATTHROTTLE_ON bit to disable this type of mitigation.
	 */
	count = array_value_mismatch_count(TVPM_TXCHAIN_RANGE_HI,
		cfg->txchain_config_vbat, ARRAYSIZE(cfg->txchain_config_vbat) - 1);
	if (count == 0) {
		tvpm->txchain_config &= ~WLC_VBATTHROTTLE_ON;
	}

	/* If all tvpm_txchain_ppm nvram txchain values are max #chains then clear the
	 * WLC_PPMTHROTTLE_ON bit to disable this type of mitigation.
	 */
	count = array_value_mismatch_count(TVPM_TXCHAIN_RANGE_HI,
		cfg->txchain_config_ppm, ARRAYSIZE(cfg->txchain_config_ppm) - 1);
	if (count == 0) {
		tvpm->txchain_config &= ~WLC_PPMTHROTTLE_ON;
	}

	/* If all tvpm_pwrboff_2g/5gl nvram power backoff values are 0 then disable
	 * tx power backoff mitigation.
	 * Clear the WLC_TEMPTHROTTLE_ON bit to disable this type of mitigation.
	 */
	zero_count = array_zero_count((uint8*)cfg->pwrbackoff_config_2g,
		sizeof(cfg->pwrbackoff_config_2g) / sizeof(cfg->pwrbackoff_config_2g[0][0]));
	zero_count += array_zero_count((uint8*)cfg->pwrbackoff_config_5g,
		sizeof(cfg->pwrbackoff_config_5g) / sizeof(cfg->pwrbackoff_config_5g[0][0]));
	if (zero_count == (sizeof(cfg->pwrbackoff_config_2g) /
		sizeof(cfg->pwrbackoff_config_2g[0][0]) * 2)) {
		tvpm->txdc_config &= ~WLC_TEMPTHROTTLE_ON;
	}

	/* register module */
	tvpm->err = wlc_module_register(wlc->pub, tvpm_iovars, rstr_tvpm, tvpm,
		wlc_tvpm_doiovar, wlc_tvpm_watchdog, wlc_tvpm_up, wlc_tvpm_down);
	if (tvpm->err != BCME_OK) {
		WL_ERROR(("wl%d: wlc_tvpm_iovar_attach failed err=%d\n",
			wlc->pub->unit, tvpm->err));
		goto fail;
	}
	tvpm->ready = TRUE;
#if defined(TVPM_DEBUG)
	wlc_dump_register(wlc->pub, "tvpm", (dump_fn_t)wlc_dump_tvpm, (void *)tvpm);
#endif // endif

	return tvpm;

fail:
#ifdef DONGLEBUILD
	/* Trap here to preserve variables for analysis */
	OSL_SYS_HALT();
#else
	MODULE_DETACH(tvpm, wlc_tvpm_detach);
#endif /* DONGLEBUILD */
	return NULL;
}

void
BCMATTACHFN(wlc_tvpm_detach)(wlc_tvpm_info_t *p)
{
	wlc_tvpm_info_t *tvpm = (wlc_tvpm_info_t *)p;
	wlc_info_t * wlc;

	if (tvpm == NULL)
		return;

	wlc = tvpm->wlc;
	if (tvpm->ready) {
		wlc_module_unregister(wlc->pub, "tvpm", tvpm);
	}
	wlc_tvpm_free(tvpm);
}
