/**
 * @file
 * @brief
 * Power statistics: Collect power related stats and expose to other modules
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_pwrstats.c 772932 2019-03-07 09:10:45Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc_assoc.h>
#include <wlc_hw.h>
#include <wlc_phy_hal.h>
#include <wlc_hw_priv.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_scan.h>
#include <wlc_rm.h>
#include <wlc_pwrstats.h>
#include <wl_export.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif /* WLMCNX */

#ifdef BCMSDIODEV
#include <sdiovar.h>
#endif /* BCMSDIODEV */

#ifdef WL_MIMO_SISO_STATS
#include <wlc_stf.h>
#endif // endif
#include <phy_ocl_api.h>
#ifdef ECOUNTERS
#include <ecounters.h>
#endif // endif

#ifndef TIME_DIFF
#define TIME_DIFF(timestamp1, timestamp2) (((timestamp1) - (timestamp2)))
#endif // endif

/* For recording connection stats */
typedef struct wlc_conn_stats {
	uint32	connect_count;
	uint32	connect_dur;
	uint32	connect_start;
	uint32	connect_end;
	bool	connect_in_progress;
} wlc_conn_stats_t;

/* Info block related to pwrstats */
struct wlc_pwrstats_info {
	wlc_info_t *wlc;
	bool   wakelog;		/* Do pmwake logging */

	/* Transient logging state */
	uint32 *pmwake_start_time;
	uint32 pmwake_prev_reason;

	/* For recording phy (tx/rx duration) info */
	wl_pwr_phy_stats_t *phy;

	/* absolute cumulative drift value; reset on wrapping */
	uint32	cum_drift_abs;
	wlc_pm_debug_t *pm_state;
	uint32 *pmd_event_wake_dur;

	/* Wake info/history for reporting */
	pm_awake_data_v2_t	*pmwake;
	wlc_conn_stats_t *cs;
	uint32	frts_start; /* ms at start of current frts period */
	bool	frts_in_progress;
	bool	frts_data; /* Is current frts started due to data frame? */
	uint32	frts_data_dur; /* cumulative frts dur due to data frame */
};

typedef struct wlc_pwrstats_mem {
	wlc_pwrstats_info_t info;
	wl_pwr_phy_stats_t phy;
	pm_awake_data_v2_t pmwake;
	wlc_conn_stats_t cs;
	wlc_pm_debug_t pm_state[WLC_STA_AWAKE_STATES_MAX]; /* timestamped wake bits */
	uint32 pmd_event_wake_dur[WLC_PMD_EVENT_MAX];      /* cumulative usecs per wake reason */
	uint32 pmwake_start_time[WLC_PMD_EVENT_MAX];

	uint32 tx_dur_last;	/* TX duration snapshot */
	uint32 rx_dur_last;	/* RX duration snapshot */
} wlc_pwrstats_mem_t;

/* iovar table */
enum {
	IOV_PWRSTATS,
	IOV_DRIFT_STATS_RESET,
	IOV_LAST
};

static const bcm_iovar_t pwrstats_iovars[] = {
	{"pwrstats", IOV_PWRSTATS,
	0, IOVF2_RSDB_CORE_OVERRIDE, IOVT_BUFFER, WL_PWR_STATS_HDRLEN,
	},
	{"drift_stats_reset", IOV_DRIFT_STATS_RESET,
	0, 0, IOVT_VOID, 0
	},
	{NULL, 0, 0, 0, 0, 0}
};

static int wlc_pwrstats_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static int wlc_pwrstats_get(wlc_info_t *wlc, void *params, uint p_len,
	void *arg, int len, wlc_bsscfg_t *bsscfg);
static int wlc_pwrstats_get_wake(wlc_info_t *wlc, uint8 *destptr, int destlen);
static int wlc_pwrstats_get_connection(wlc_pwrstats_info_t *ps, uint8 *destptr, int destlen);
static void wlc_pwrstats_watchdog(void *context);
static void wlc_pwrstats_phy_stats_upd(wlc_info_t *wlc);
static void wlc_pwrstats_reset_drift_stats(wlc_pwrstats_info_t *ps);
static int wlc_pwrstats_up(void *hdl);
static int wlc_pwrstats_down(void *hdl);

#ifdef WLMCNX
static void wlc_pwrstats_drift_upd(wlc_pwrstats_info_t *ps, int32 drift);
#endif /* WLMCNX */
/* Size of total accumulated data */
#define WLC_PWRSTATS_PM_AWAKE_V2_TOTAL_LEN	(sizeof(wl_pwr_pm_awake_stats_v2_t) +	\
	sizeof(wlc_pm_debug_t) * WLC_STA_AWAKE_STATES_MAX +	\
	sizeof(uint32) * WLC_PMD_EVENT_MAX)

#ifdef ECOUNTERS
static int wlc_ecounters_pwrstats_phy(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len);
static int wlc_ecounters_pwrstats_wake(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len);
#endif // endif
static void
pwrstats_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt_data)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)ctx;
	wlc_bsscfg_t *cfg;

	BCM_REFERENCE(ps);
	if (evt_data == NULL || evt_data->bsscfg == NULL) {
		WL_ERROR(("%s:evt_data or bsscfg NULL\n", __FUNCTION__));
		return;
	}

	cfg = evt_data->bsscfg;

	if (!evt_data->up) {
		/* Only invoke for primary infra STA */
		if (cfg->BSS && BSSCFG_STA(cfg) && cfg == cfg->wlc->cfg) {
			wlc_pwrstats_frts_end(cfg->wlc->pwrstats);
		}
	}
}

static int
BCMUNINITFN(wlc_pwrstats_up)(void *hdl)
{
	wlc_pwrstats_mem_t *ps_mem = (wlc_pwrstats_mem_t *)hdl;

	/* Reset TX/RX dur for WL up - shm values are reset by ucode */
	ps_mem->tx_dur_last = 0;
	ps_mem->rx_dur_last = 0;

	return BCME_OK;
}

static int
BCMUNINITFN(wlc_pwrstats_down)(void *hdl)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)hdl;

	/* Get the last update before going down */
	if (PWRSTATS_ENAB(ps->wlc->pub))
		wlc_pwrstats_phy_stats_upd(ps->wlc);

	return BCME_OK;
}

void *
BCMATTACHFN(wlc_pwrstats_attach)(wlc_info_t *wlc)
{
	wlc_pwrstats_info_t *ps = NULL;
	wlc_pwrstats_mem_t *ps_mem = NULL;
	int err;
	/* try and allocate the power stats stuff */
	if ((ps_mem = MALLOCZ(wlc->osh, sizeof(wlc_pwrstats_mem_t)))) {
		ps = (wlc_pwrstats_info_t *)ps_mem;
		ps->pmwake = &ps_mem->pmwake;
		ps->phy = &ps_mem->phy;
		ps->cs = &ps_mem->cs;
		ps->pm_state = ps_mem->pm_state;
		ps->pmd_event_wake_dur = ps_mem->pmd_event_wake_dur;
		ps->pmwake_start_time = ps_mem->pmwake_start_time;
		ps->pmwake->pm_state_offset = sizeof(pm_awake_data_v2_t);
		ps->pmwake->pm_state_len = WLC_STA_AWAKE_STATES_MAX;
		ps->pmwake->pmd_event_wake_dur_offset = sizeof(pm_awake_data_v2_t) +
			WLC_STA_AWAKE_STATES_MAX * sizeof(wlc_pm_debug_t);
		ps->pmwake->pmd_event_wake_dur_len = WLC_PMD_EVENT_MAX;
		ps->pmwake->flags = 0;
	} else
		goto fail;

	/* register module */
	if (wlc_module_register(wlc->pub, pwrstats_iovars, "pwrstats", ps, wlc_pwrstats_doiovar,
		wlc_pwrstats_watchdog, wlc_pwrstats_up, wlc_pwrstats_down)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wlc->pub->_pwrstats = TRUE;
	ps->wakelog = TRUE;
	ps->wlc = wlc;

#ifdef EVENT_LOG_COMPILE
#ifdef EVENT_LOG_PWRSTATS
	/* Enabling Scan high and Scan Error logs */
	event_log_tag_start(EVENT_LOG_TAG_PWRSTATS_INFO, EVENT_LOG_SET_WL,
	                    EVENT_LOG_TAG_FLAG_LOG | EVENT_LOG_TAG_FLAG_PRINT);
#endif /* EVENT_LOG_PWRSTATS */
#endif /* EVENT_LOG_COMPILE */

	/* bss up/down */
	err = wlc_bsscfg_updown_register(wlc, pwrstats_bsscfg_up_down, (void *)ps);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register failed, error %d\n",
		        WLCWLUNIT(wlc), __FUNCTION__, err));
		goto fail;
	}
	/* EVENT_LOG_COMPILE flag present below will be removed after
	* MACOS completes porting of ecounters. Without this flag,
	* precommit fails in NIC and NIC off load builds.
	*/
#if defined(ECOUNTERS) && defined(EVENT_LOG_COMPILE)
	if (ECOUNTERS_ENAB() && ((RSDB_ENAB(wlc->pub) && (wlc->pub->unit == MAC_CORE_UNIT_0)) ||
		!RSDB_ENAB(wlc->pub))) {
		wl_ecounters_register_source(WL_IFSTATS_XTLV_WL_SLICE_PWRSTATS_PHY,
			wlc_ecounters_pwrstats_phy, (void*)wlc);
		wl_ecounters_register_source(WL_IFSTATS_XTLV_WL_SLICE_PWRSTATS_WAKE_V2,
			wlc_ecounters_pwrstats_wake, (void*)wlc);
	}
#endif // endif

#ifdef WL_MIMO_SISO_STATS
	if ((wlc->mimo_siso_metrics = MALLOCZ(wlc->osh,
		sizeof(wl_mimo_meas_metrics_t))) == NULL) {
		WL_ERROR(("wl%d: wl_mimo_meas_metrics_t alloc failed\n", WLCWLUNIT(wlc)));
		goto fail;
	}
	if ((wlc->last_mimo_siso_cnt = MALLOCZ(wlc->osh,
		sizeof(wl_mimo_meas_metrics_t))) == NULL) {
		WL_ERROR(("wl%d: wl_mimo_siso_stats_t alloc failed\n", WLCWLUNIT(wlc)));
		goto fail;
	}
#endif /* WL_MIMO_SISO_STATS */

	return ps;
fail:
	MODULE_DETACH(ps, wlc_pwrstats_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_pwrstats_detach)(void *pwrstats)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)pwrstats;
	wlc_info_t * wlc;

	if (ps == NULL)
		return;

	wlc = ps->wlc;

	/* unregister for bss up/down */
	wlc_bsscfg_updown_unregister(wlc, pwrstats_bsscfg_up_down, (void *)ps);
	wlc_module_unregister(wlc->pub, "pwrstats", ps);

#ifdef WL_MIMO_SISO_STATS
	if (wlc->mimo_siso_metrics) {
		MFREE(wlc->osh, wlc->mimo_siso_metrics, sizeof(wl_mimo_meas_metrics_t));
		wlc->mimo_siso_metrics = NULL;
	}
	if (wlc->last_mimo_siso_cnt) {
		MFREE(wlc->osh, wlc->last_mimo_siso_cnt, sizeof(wl_mimo_meas_metrics_t));
		wlc->last_mimo_siso_cnt = NULL;
	}
#endif /* WL_MIMO_SISO_STATS */

	MFREE(wlc->osh, ps, sizeof(wlc_pwrstats_mem_t));
}

#include <wlc_patch.h>

static int
wlc_pwrstats_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)hdl;
	wlc_info_t * wlc = ps->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = BCME_OK;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_PWRSTATS):
		if (PWRSTATS_ENAB(wlc->pub)) {
			err = wlc_pwrstats_get(wlc, params, p_len, arg, len, bsscfg);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	case IOV_SVAL(IOV_DRIFT_STATS_RESET):
		wlc_pwrstats_reset_drift_stats(ps);
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

static void
wlc_pwrstats_watchdog(void *context)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)context;
	/* Update some power stats regardless of CCA_STATS definiiton. */
	if (PWRSTATS_ENAB(ps->wlc->pub))
		wlc_pwrstats_phy_stats_upd(ps->wlc);
}

/* Invoked to start frts time accounting */
void
wlc_pwrstats_frts_start(void *ctx)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)ctx;

	if (ps->frts_in_progress == FALSE) {
		ps->frts_in_progress = TRUE;
		ps->frts_start = OSL_SYSUPTIME();
	}
}
/* Invoked at the end of frts to update cumulative frts time */
void
wlc_pwrstats_frts_end(void *ctx)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)ctx;

	if (ps->frts_in_progress) {
		ps->frts_in_progress = FALSE;
		ps->pmwake->frts_time += OSL_SYSUPTIME() - ps->frts_start;
		ps->pmwake->frts_end_cnt++;
		if (ps->frts_data)
			ps->frts_data_dur += OSL_SYSUPTIME() - ps->frts_start;
		ps->frts_data = 0;
	}
}

/* This function updates frts time till now and restarts frts only if it is in progress.
 * It is invoked to report accurate time to host while the timer is active.
 */
void
wlc_pwrstats_frts_checkpoint(wlc_pwrstats_info_t *ps)
{
	uint32 data_backup = ps->frts_data;
	if (ps->frts_in_progress) {
		wlc_pwrstats_frts_end(ps);
		ps->frts_data = data_backup;
		wlc_pwrstats_frts_start(ps);
	}
}

/* Fill pm_alert with pwrstats maintainted data and return updated ptr */
uint8 *
wlc_pwrstats_fill_pmalert(wlc_info_t *wlc, uint8 *data)
{
	wlc_pwrstats_info_t *ps = wlc->pwrstats;
	wl_pmalert_pmstate_t *pmstate;
	wl_pmalert_event_dur_t *event_dur;
	wl_pmalert_fixed_t *fixed;

	wlc_pwrstats_frts_checkpoint(wlc->pwrstats);

	fixed = (wl_pmalert_fixed_t *)data;
	fixed->last_drift = ps->pmwake->last_drift;
	fixed->min_drift = ps->pmwake->min_drift;
	fixed->max_drift = ps->pmwake->max_drift;
	fixed->avg_drift = ps->pmwake->avg_drift;
	fixed->drift_cnt = ps->pmwake->drift_cnt;
	fixed->frts_time = ps->frts_data_dur;
	fixed->frts_end_cnt = ps->pmwake->frts_end_cnt;

	data += sizeof(wl_pmalert_fixed_t);
	pmstate = (wl_pmalert_pmstate_t *)data;
	pmstate->type = WL_PMALERT_PMSTATE;
	pmstate->len = sizeof(wl_pmalert_pmstate_t) +
		sizeof(wlc_pm_debug_t) * (WLC_STA_AWAKE_STATES_MAX - 1);
	pmstate->pmwake_idx = ps->pmwake->pmwake_idx;
	memcpy(pmstate->pmstate, ps->pm_state, pmstate->len);

	data += pmstate->len;
	event_dur = (wl_pmalert_event_dur_t *)data;
	event_dur->type = WL_PMALERT_EVENT_DUR;
	event_dur->len = sizeof(wl_pmalert_event_dur_t) +
		sizeof(uint32) * (WLC_PMD_EVENT_MAX - 1);
	memcpy(event_dur->event_dur, ps->pmd_event_wake_dur,
		sizeof(event_dur->event_dur[0]) * WLC_PMD_EVENT_MAX);

	data += event_dur->len;

	event_dur = (wl_pmalert_event_dur_t *)data;
	event_dur->type = WL_PMALERT_EPM_START_EVENT_DUR;
	event_dur->len = sizeof(wl_pmalert_event_dur_t) +
		sizeof(uint32) * (WLC_PMD_EVENT_MAX - 1);
	memcpy(event_dur->event_dur, wlc->excess_pmwake->pp_start_event_dur,
		sizeof(event_dur->event_dur[0]) * WLC_PMD_EVENT_MAX);

	data += event_dur->len;

	return data;
}

static void
wlc_pwrstats_phy_stats_upd(wlc_info_t *wlc)
{
	wlc_pwrstats_info_t *ps = wlc->pwrstats;
	wlc_pwrstats_mem_t *ps_mem = (wlc_pwrstats_mem_t *)ps;
	cca_ucode_counts_t tmp;

	ASSERT(ps);
	if (wlc_bmac_cca_stats_read(wlc->hw, &tmp) == BCME_OK) {
		uint32 rx_dur_tmp = tmp.ibss + tmp.obss + tmp.noctg + tmp.nopkt;
		ps->phy->tx_dur += tmp.txdur - ps_mem->tx_dur_last;
		ps->phy->rx_dur += rx_dur_tmp - ps_mem->rx_dur_last;
		ps_mem->tx_dur_last = tmp.txdur;
		ps_mem->rx_dur_last = rx_dur_tmp;
	}
}

static int
wlc_pwrstats_put_slice_index(wlc_info_t *wlc, uint8 *destptr, int destlen)
{
	wl_pwr_slice_index_t pwr_slice;
	uint16 taglen = sizeof(wl_pwr_slice_index_t);

	/* Make sure there's room for this section */
	if (destlen < ROUNDUP(sizeof(wl_pwr_slice_index_t),
		sizeof(uint32)))
		return BCME_BUFTOOSHORT;

	pwr_slice.type = WL_PWRSTATS_TYPE_SLICE_INDEX;
	pwr_slice.len = taglen;
	pwr_slice.slice_index = (uint8) wlc->pub->unit;
	memcpy(destptr, &pwr_slice, taglen);

	/* Report use of this segment (plus padding) */
	return (ROUNDUP(taglen, sizeof(uint32)));
}

static int
wlc_pwrstats_get_phy(wlc_pwrstats_info_t *ps, uint8 *destptr, int destlen)
{
	wl_pwr_phy_stats_t *phy_stats = ps->phy;
	uint16 taglen = sizeof(wl_pwr_phy_stats_t);

	/* Make sure there's room for this section */
	if (destlen < ROUNDUP(sizeof(wl_pwr_phy_stats_t), sizeof(uint32)))
		return BCME_BUFTOOSHORT;

	/* Update common structure fields and copy into the destination */
	phy_stats->type = WL_PWRSTATS_TYPE_PHY;
	phy_stats->len = taglen;
	memcpy(destptr, phy_stats, taglen);

	/* Report use of this segment (plus padding) */
	return (ROUNDUP(taglen, sizeof(uint32)));
}

static int
wlc_pwrstats_get_wake(wlc_info_t *wlc, uint8 *destptr, int destlen)
{
	bool wasup;
	wlc_pwrstats_info_t *ps = wlc->pwrstats;
	wl_pwr_pm_awake_stats_v2_t wake_stats;
	uint16 taglen = sizeof(wake_stats) +
		sizeof(wlc_pm_debug_t) * WLC_STA_AWAKE_STATES_MAX +
		sizeof(uint32) * WLC_PMD_EVENT_MAX;

	/* Make sure there's room for this section */
	if (destlen < ROUNDUP(taglen, sizeof(uint32)))
		return BCME_BUFTOOSHORT;

	/* Update fixed/common structure fields */
	wake_stats.type = WL_PWRSTATS_TYPE_PM_AWAKE2;
	wake_stats.len = taglen;

	wlc_pwrstats_frts_checkpoint(wlc->pwrstats);

	/* Copy struct with accumulated debug/wake state data */
	memcpy(&wake_stats.awake_data, ps->pmwake,
		sizeof(wake_stats.awake_data));

	/* Update immediate fields */
	wake_stats.awake_data.curr_time = OSL_SYSUPTIME();
	wasup = RSDB_ENAB(wlc->pub)? wlc->pub->hw_up : si_iscoreup(wlc->pub->sih);
	wake_stats.awake_data.pm_dur = wlc_get_accum_pmdur(wlc);
	wake_stats.awake_data.flags &= ~WL_PWR_PM_AWAKE_STATS_WAKE_MASK;
	wake_stats.awake_data.sw_macc = wlc->hw->maccontrol;
	if (wasup) {
		wake_stats.awake_data.hw_macc = R_REG(wlc->osh, D11_MACCONTROL(wlc));
		wake_stats.awake_data.flags |= WL_PWR_PM_AWAKE_STATS_WAKE;
	} else {
		wake_stats.awake_data.hw_macc = 0;
		wake_stats.awake_data.flags |= WL_PWR_PM_AWAKE_STATS_ASLEEP;
	}
	wake_stats.awake_data.mpc_dur = wlc_get_mpc_dur(wlc);
	/* Avoid divide by zero */
	if (ps->pmwake->drift_cnt)
		wake_stats.awake_data.avg_drift =
			ps->cum_drift_abs/ps->pmwake->drift_cnt;
	else
		/* Make sure min_drift is not reported as max int-32 value
		 * immediately after drift stats are reset.
		 */
		wake_stats.awake_data.min_drift = 0;

	memcpy(destptr, &wake_stats, sizeof(wake_stats));
	destptr += sizeof(wake_stats);

	memcpy(destptr, ps->pm_state,
		sizeof(wlc_pm_debug_t) * WLC_STA_AWAKE_STATES_MAX);

	destptr += sizeof(wlc_pm_debug_t) * WLC_STA_AWAKE_STATES_MAX;
	memcpy(destptr, ps->pmd_event_wake_dur,
		sizeof(uint32) * WLC_PMD_EVENT_MAX);
	/* Report use of this segment (plus padding) */
	return (ROUNDUP(taglen, sizeof(uint32)));
}

#define WLC_PWRSTATS_DEFTYPES { \
		WL_PWRSTATS_TYPE_SLICE_INDEX, \
		WL_PWRSTATS_TYPE_PHY, \
		WL_PWRSTATS_TYPE_SCAN, \
		WL_PWRSTATS_TYPE_CONNECTION, \
		WL_PWRSTATS_TYPE_PCIE, \
		WL_PWRSTATS_TYPE_MIMO_PS_METRICS, \
		WL_PWRSTATS_TYPE_PM_AWAKE2, \
		WL_PWRSTATS_TYPE_SDIO \
	}

void
wlc_pwrstats_reset_drift_stats(wlc_pwrstats_info_t *ps)
{
	ps->cum_drift_abs = 0;
	ps->pmwake->drift_cnt = 0;
	ps->pmwake->min_drift = BCM_INT32_MAX;
	ps->pmwake->max_drift = 0;
	ps->pmwake->last_drift = 0;
}

void
wlc_pwrstats_bcn_process(wlc_bsscfg_t *bsscfg, void *pwrstats, uint16 seq)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)pwrstats;
	int32 drift = 0;

	/* To silence compiler warning that variable is unused */
	BCM_REFERENCE(drift);

	/* Reset drift parameters on receiving 1st bcn after assoc */
	if (bsscfg->assoc->state == AS_WAIT_RCV_BCN) {
		wlc_pwrstats_reset_drift_stats(ps);
	}
#ifdef WLMCNX
	else if (MCNX_ENAB(ps->wlc->pub)) {
		/* Assume drift is never beyond 31-bit magnitude */
		drift = wlc_mcnx_read_shm(ps->wlc->mcnx, M_P2P_TSF_DRIFT(ps->wlc, 1)) << 16;
		drift |= wlc_mcnx_read_shm(ps->wlc->mcnx, M_P2P_TSF_DRIFT(ps->wlc, 0));
		wlc_pwrstats_drift_upd(ps, drift);
	}
#endif /* WLMCNX */
	WL_PWRSTATS_INFO("Bcn recvd: seq:%d drift:%d\n",
		seq, drift, 0, 0);
}

static int
wlc_pwrstats_get_connection(wlc_pwrstats_info_t *ps, uint8 *destptr, int destlen)
{
	wlc_conn_stats_t *cs = ps->cs;
	wl_pwr_connect_stats_t connect_stats;
	uint16 taglen = sizeof(wl_pwr_connect_stats_t);

	/* Make sure there's room for this section */
	if (destlen < ROUNDUP(sizeof(wl_pwr_connect_stats_t), sizeof(uint32)))
		return BCME_BUFTOOSHORT;

	/* Update common structure fields and copy into the destination */
	connect_stats.type = WL_PWRSTATS_TYPE_CONNECTION;
	connect_stats.len = taglen;
	connect_stats.count = cs->connect_count;
	connect_stats.dur = cs->connect_dur + wlc_pwrstats_curr_connect_time(ps);

	memcpy(destptr, &connect_stats, taglen);

	/* Report use of this segment (plus padding) */
	return (ROUNDUP(taglen, sizeof(uint32)));
}

int
wlc_pwrstats_get(wlc_info_t *wlc, void *params, uint p_len,
	void *arg, int len, wlc_bsscfg_t *bsscfg)
{
	int rc = BCME_OK;
	uint16 ntypes = 0;
	uint16 typelist[] = WLC_PWRSTATS_DEFTYPES;
	uint16 *tptr = typelist;
	int16 retlen = 0;
	uint idx;
	uint16 type;

	uint8 *destptr;
	wl_pwrstats_t *pwrstats = arg;
	int totlen = WL_PWR_STATS_HDRLEN;

	ASSERT(ISALIGNED(pwrstats, sizeof(uint32)));

	/* If there are parameters, get count of types */
	if (p_len > sizeof(uint16)) {
		memcpy(&ntypes, params, sizeof(uint16));
		if (p_len < sizeof(uint16) * (ntypes + 1)) {
			rc = BCME_BADARG;
			goto done;
		}
	}

	/* Malloc for the input list and copy it, set up for default */
	if (ntypes) {
		tptr = (uint16*)MALLOC(wlc->osh, ntypes * sizeof(uint16));
		if (tptr == NULL) {
			rc = BCME_NOMEM;
			goto done;
		}
		memcpy(tptr, (uint16*)params + 1, ntypes * sizeof(uint16));
	} else {
		ntypes = ARRAYSIZE(typelist);
	}

	/* Prepare the return header */
	pwrstats->version = WL_PWRSTATS_VERSION;
	destptr = (uint8*)arg + WL_PWR_STATS_HDRLEN;

	/* Go through the requested types */
	for (idx = 0; idx < ntypes; idx++, totlen += retlen, destptr += retlen) {
		type = tptr[idx];

		retlen = 0;
		switch (type) {
		case WL_PWRSTATS_TYPE_SLICE_INDEX:
			retlen = wlc_pwrstats_put_slice_index(wlc, destptr, len - totlen);
			break;

		case WL_PWRSTATS_TYPE_PHY:
			retlen = wlc_pwrstats_get_phy(wlc->pwrstats, destptr, len - totlen);
			break;

		case WL_PWRSTATS_TYPE_SCAN:
			retlen = wlc_pwrstats_get_scan(wlc->scan, destptr, len - totlen);
			break;

		case WL_PWRSTATS_TYPE_PM_AWAKE2:
			retlen = wlc_pwrstats_get_wake(wlc, destptr, len-totlen);
			break;

		case WL_PWRSTATS_TYPE_CONNECTION:
			retlen = wlc_pwrstats_get_connection(wlc->pwrstats, destptr, len - totlen);
			break;
		case WL_PWRSTATS_TYPE_MIMO_PS_METRICS:
			retlen = wlc_get_mimo_siso_meas_metrics(wlc, destptr, len - totlen);
			break;
#ifdef DONGLEBUILD
		case WL_PWRSTATS_TYPE_PCIE:
#ifdef BCMPCIEDEV
			if (BCMPCIEDEV_ENAB()) {
				/* Need to generate an ioctl/iovar request to the bus */
				rc = strlen("bus:pwrstats");
				if ((len - totlen) > (rc + 1 + sizeof(uint16))) {
					memcpy((char*)destptr, "bus:pwrstats", rc);
					destptr[rc] = '\0';
					memcpy(&destptr[rc + 1], &type, sizeof(uint16));
					rc = wl_busioctl(wlc->wl, BUS_GET_VAR, destptr,
						(len - totlen), NULL, NULL, FALSE);
					/* The result is iovar status, convert as needed */
					if (rc == BCME_OK) {
						retlen = ((wl_pwr_pcie_stats_t*)destptr)->len;
						retlen = ROUNDUP(retlen, sizeof(uint32));
					}
				} else {
					rc = BCME_UNSUPPORTED;
				}
			}
#endif /* BCMPCIEDEV */
			break;
#endif /* DONGLEBUILD */

#ifdef DONGLEBUILD
		case WL_PWRSTATS_TYPE_SDIO:
#ifdef BCMSDIODEV
			if (BCMSDIODEV_ENAB()) {
				/* Need to generate an ioctl/iovar request to the bus */
				rc = strlen("bus:pwrstats");
				if ((len - totlen) > (rc + 1 + sizeof(uint16))) {
					memcpy((char*)destptr, "bus:pwrstats", rc);
					destptr[rc] = '\0';
					memcpy(&destptr[rc + 1], &type, sizeof(uint16));
					rc = wl_busioctl(wlc->wl, BUS_GET_VAR, destptr,
						(len - totlen), NULL, NULL, FALSE);
					/* The result is iovar status, convert as needed */
					if (rc == BCME_OK) {
						retlen = ((wl_pwr_sdio_stats_t*)destptr)->len;
						retlen = ROUNDUP(retlen, sizeof(uint32));
					}
				} else {
					rc = BCME_UNSUPPORTED;
				}
			}
#endif /* BCMSDIODEV */
			break;
#endif /* DONGLEBUILD */

		default:
			/* XXX: TBD: Return some sort of indication that
			 * this tag is not supported in this firmware.
			 */
			rc = BCME_UNSUPPORTED;
			break;
		}

		/* Exit loop on error */
		if (retlen < 0) {
			rc = BCME_BADARG;
			break;
		}
	}

	/* Update total length */
	pwrstats->length = totlen;

done:
	if (tptr != NULL && tptr != typelist)
		MFREE(wlc->osh, tptr, ntypes * sizeof(uint16));

	return rc;
}

#ifdef WLMCNX
void
wlc_pwrstats_drift_upd(wlc_pwrstats_info_t *ps, int32 drift)
{
	ps->pmwake->last_drift = drift;

	if (ABS(ps->pmwake->min_drift) > ABS(drift))
		ps->pmwake->min_drift = drift;
	if (ABS(ps->pmwake->max_drift) < ABS(drift))
		ps->pmwake->max_drift = drift;

	/* Overflow/Wrapping? reset the drifts */
	if ((0xffffffff - ps->cum_drift_abs) < ABS(drift)) {
		ps->cum_drift_abs = ABS(drift);
		ps->pmwake->drift_cnt = 1;
		ps->pmwake->min_drift = drift;
		ps->pmwake->max_drift = drift;
	}
	else {
		ps->cum_drift_abs += ABS(drift);
		ps->pmwake->drift_cnt++;
	}
}
#endif /* WLMCNX */

void
wlc_pwrstats_wake_reason_upd(wlc_info_t *wlc, bool stay_awake)
{
	uint16 wake_mask = 0;
	int idx;
	wlc_bsscfg_t *cfg;
	wlc_pwrstats_info_t *ps = wlc->pwrstats;

	/* If wakelog is false we're done */
	if (!ps->wakelog) {
		return;
	}
	/* Nothing on now: if no change, return now. */
	if (!stay_awake && !ps->pmwake_prev_reason) {
		return;
	}

	/* check conditions again and record in bitmask */
	if (wlc->wake)
		wake_mask |= WLC_PMD_WAKE_SET;

	if (wlc->PMawakebcn)
		wake_mask |= WLC_PMD_PM_AWAKE_BCN;

	if (SCAN_IN_PROGRESS(wlc->scan))
		wake_mask |= WLC_PMD_SCAN_IN_PROGRESS;

	if (WLC_RM_IN_PROGRESS(wlc))
		wake_mask |= WLC_PMD_RM_IN_PROGRESS;

	if (AS_IN_PROGRESS(wlc))
		wake_mask |= WLC_PMD_AS_IN_PROGRESS;

	if (wlc->PMpending)
		wake_mask |= WLC_PMD_PM_PEND;

	if (wlc->PSpoll)
		wake_mask |= WLC_PMD_PS_POLL;

	if (wlc->check_for_unaligned_tbtt)
		wake_mask |= WLC_PMD_CHK_UNALIGN_TBTT;

	if (wlc->apsd_sta_usp)
		wake_mask |= WLC_PMD_APSD_STA_UP;

	if (wlc->gptimer_stay_awake_req)
		wake_mask |= WLC_PMD_GPTIMER_STAY_AWAKE;

	if (wlc->pm2_radio_shutoff_pending)
		wake_mask |= WLC_PMD_PM2_RADIO_SOFF_PEND;

	ps->pmwake->mpc_dur = wlc_get_mpc_dur(wlc);
	ps->pmwake->pm_dur = wlc_get_accum_pmdur(wlc);

	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg != wlc->cfg &&
#ifdef WLP2P
			!BSS_P2P_ENAB(wlc, cfg) &&
#endif // endif
			TRUE)
			wake_mask |= WLC_PMD_NON_PRIM_STA_UP;
	}

	/* stay awake as long as an infra STA is associating */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS) {
			if (cfg->assoc != NULL &&
			    cfg->assoc->state != AS_IDLE)
			{
				wake_mask |= WLC_PMD_AS_IN_PROGRESS;
			}
		}
	}

	FOREACH_UP_AP(wlc, idx, cfg) {
#ifdef WLP2P
		if (!BSS_P2P_ENAB(wlc, cfg))
#endif // endif
		{
			wake_mask |= WLC_PMD_AP_UP;
		}
	}

	/* We couldn't determine the stay awake cause. */
	if (stay_awake && wake_mask == 0) {
		/* Add it to the AP bit until until we have an 'unknown' reason */
		wake_mask |= WLC_PMD_AP_UP;
	}

	if (ps->pmwake->pmwake_idx == WLC_STA_AWAKE_STATES_MAX)
		ps->pmwake->pmwake_idx = 0;

	if (wake_mask != ps->pmwake_prev_reason) {
		int i;
		uint32 curr_time = OSL_SYSUPTIME();

		ps->pm_state[ps->pmwake->pmwake_idx].timestamp = curr_time;
		ps->pm_state[ps->pmwake->pmwake_idx++].reason = wake_mask;

		/* Check which events added to the duration */
		for (i = 0; i < WLC_PMD_EVENT_MAX; i++) {
			if ((wake_mask & (1 << i)) &&
				!(ps->pmwake_prev_reason & (1 << i))) {
				/* Event turned on: log time for later computation */
				ps->pmwake_start_time[i] = curr_time;
			} else if (!(wake_mask & (1 << i)) &&
				(ps->pmwake_prev_reason & (1 << i))) {
				/* Event turned off: counts towards duration for event */
				if (curr_time > ps->pmwake_start_time[i])
					ps->pmd_event_wake_dur[i] +=
						(curr_time - ps->pmwake_start_time[i]);
				else
					ps->pmd_event_wake_dur[i] +=
						(ps->pmwake_start_time[i] - curr_time);

				ps->pmwake_start_time[i] = 0;
			}
		}
	}

	ps->pmwake_prev_reason = wake_mask;
}

void
wlc_pwrstats_copy_event_wake_dur(void *buf, void *pwrstats)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)pwrstats;
	memcpy(buf, ps->pmd_event_wake_dur, sizeof(uint32) * WLC_PMD_EVENT_MAX);
}

/* This is invoked at the end of connection. This function updates
 * the cumulative connect time and returns the last connect time.
 */
uint32
wlc_pwrstats_connect_time_upd(void *pwrstats)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)pwrstats;
	uint32 connect_dur = 0;
	wlc_conn_stats_t *cs = ps->cs;

	if (cs->connect_in_progress == TRUE) {
		cs->connect_in_progress = FALSE;
		cs->connect_end = OSL_SYSUPTIME();
		connect_dur = cs->connect_end - cs->connect_start;
		cs->connect_dur += connect_dur;
		cs->connect_count++;
	}
	return connect_dur;
}

uint32
wlc_pwrstats_curr_connect_time(void *pwrstats)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)pwrstats;
	wlc_conn_stats_t *cs = ps->cs;
	uint32 curr_connect_time = 0;

	/* Calculate awake time due to active connection */
	if (cs->connect_in_progress == TRUE) {
		curr_connect_time = OSL_SYSUPTIME() - cs->connect_start;
	}
	return curr_connect_time;
}

void
wlc_pwrstats_connect_start(void *pwrstats)
{
	wlc_pwrstats_info_t *ps = (wlc_pwrstats_info_t *)pwrstats;
	wlc_conn_stats_t *cs = ps->cs;
	if (cs->connect_in_progress == FALSE) {
		cs->connect_start = OSL_SYSUPTIME();
		cs->connect_in_progress = TRUE;
	}
}

void
wlc_pwrstats_set_frts_data(wlc_pwrstats_info_t *ps, bool isdata)
{
	ps->frts_data = isdata;
}
uint32
wlc_pwrstats_get_frts_data_dur(wlc_pwrstats_info_t *ps)
{
	return ps->frts_data_dur;
}

int
wlc_pwrstats_get_active_dur(wlc_pwrstats_info_t *ps, uint8 *destptr, int destlen)
{
	return wlc_pwrstats_get_phy(ps, destptr, destlen);
}

#ifdef WL_MIMO_SISO_STATS

int wlc_mimo_siso_metrics_report(wlc_info_t *wlc, bool el)
{
	wl_mimo_meas_metrics_v1_t *metrics;

	/* Only corerev 42/48/49/58/59/61 support this */
	if (!(D11REV_IS(wlc->pub->corerev, 48) ||
		D11REV_IS(wlc->pub->corerev, 42) ||
		D11REV_IS(wlc->pub->corerev, 49) ||
		D11REV_IS(wlc->pub->corerev, 61))) {
		WL_ERROR(("wl%d: %s, unsupported corerev %d\n",
		         wlc->pub->unit, __FUNCTION__, wlc->pub->corerev));
		return BCME_UNSUPPORTED;
	}

	wlc_mimo_siso_metrics_snapshot(wlc, FALSE, WL_MIMOPS_METRICS_SNAPSHOT_REPORT);

	metrics = (wl_mimo_meas_metrics_v1_t *)wlc->mimo_siso_metrics;
	metrics->type = WL_PWRSTATS_TYPE_MIMO_PS_METRICS;
	metrics->len = (uint16)(sizeof(*metrics) - sizeof(metrics->type) - sizeof(metrics->len));
	/* Generated event log or not */
	if (el) {
#if defined(EVENT_LOG_COMPILE)
		if (EVENT_LOG_IS_ON(EVENT_LOG_TAG_MIMO_PS_STATS)) {
			EVENT_LOG_BUFFER(EVENT_LOG_TAG_MIMO_PS_STATS,
				(void*)(metrics),
				sizeof(*metrics));
		}
#endif /* EVENT_LOG_COMPILE */
	}
	return BCME_OK;
}

static void
wlc_mimo_siso_metrics_reset(wlc_info_t *wlc)
{
	wl_mimo_meas_metrics_t	*last;

	last = wlc->last_mimo_siso_cnt;

	/* zero cached counters */
	last->total_idle_time_mimo = 0;
	last->total_idle_time_siso = 0;
	last->total_rx_time_mimo = 0;
	last->total_rx_time_siso = 0;
	last->total_tx_time_1chain = 0;
	last->total_tx_time_2chain = 0;
	last->total_tx_time_3chain = 0;
}

/* XXX SHOULD ALWAYS BE CALLED BEFORE chain/slottime changes.
 * Exception: called from IOVAR 'pwrstats 9'
 */
void wlc_mimo_siso_metrics_snapshot(wlc_info_t *wlc, bool reset_last, uint16 reason)
{
	uint slottime;
	uint16 mhfval;

	uint32 idle_slotcnt_mimo, idle_slotcnt_siso;
	uint32 rx_time_mimo, rx_time_siso;
	uint32 tx_time_1chain, tx_time_2chain, tx_time_3chain;
	uint32 sifs_time_mimo, sifs_time_siso;

	uint32 delta_idle_mimo, delta_idle_siso;
	uint32 delta_sifs_mimo, delta_sifs_siso;
	uint32 delta_rx_mimo, delta_rx_siso;

	wl_mimo_ps_metrics_snapshot_trace_t trace;
	wl_mimo_meas_metrics_t	*curr;
	wl_mimo_meas_metrics_t	*last;

	wlc_hw_info_t *wlc_hw = wlc->hw;

	curr = wlc->mimo_siso_metrics;
	last = wlc->last_mimo_siso_cnt;

#ifdef OCL
	bool ocl_en = FALSE;
#endif // endif

	/* Only corerev 42/48/49/61 support this */
	if (!(D11REV_IS(wlc->pub->corerev, 48) ||
		D11REV_IS(wlc->pub->corerev, 42) ||
		D11REV_IS(wlc->pub->corerev, 49) ||
		D11REV_IS(wlc->pub->corerev, 61))) {
		WL_ERROR(("wl%d: %s, unsupported corerev %d\n",
		         wlc->pub->unit, __FUNCTION__, wlc->pub->corerev));
		return;
	}

	if (wlc->pub->hw_up && wlc->clk) {
		/* retrieve the current slot time and beacon-siso flag */
		slottime = wlc_bmac_read_shm(wlc_hw, M_DOT11_SLOT(wlc_hw));
		mhfval = wlc_mhf_get(wlc, MHF3, WLC_BAND_AUTO);
#ifdef OCL
		if (OCL_ENAB(wlc->pub)) {
			phy_ocl_status_get(WLC_PI(wlc), NULL, NULL, &ocl_en);
		}
#endif // endif

		/* read current values from ucode */
		idle_slotcnt_mimo = wlc_bmac_read_counter(wlc_hw, 0, M_MIMO_TXOP_L(wlc_hw),
				M_MIMO_TXOP_H(wlc_hw));
		idle_slotcnt_siso = wlc_bmac_read_counter(wlc_hw, 0, M_SISO_TXOP_L(wlc_hw),
				M_SISO_TXOP_H(wlc_hw));
		rx_time_mimo = wlc_bmac_read_counter(wlc_hw, 0, M_MIMO_RXDUR_L(wlc_hw),
				M_MIMO_RXDUR_H(wlc_hw));
		rx_time_siso = wlc_bmac_read_counter(wlc_hw, 0, M_SISO_RXDUR_L(wlc_hw),
				M_SISO_RXDUR_H(wlc_hw));
		tx_time_1chain = wlc_bmac_read_counter(wlc_hw, 0, M_MIMO_TXDUR_1X_L(wlc_hw),
				M_MIMO_TXDUR_1X_H(wlc_hw));
		tx_time_2chain = wlc_bmac_read_counter(wlc_hw, 0, M_MIMO_TXDUR_2X_L(wlc_hw),
				M_MIMO_TXDUR_2X_H(wlc_hw));
		tx_time_3chain = wlc_bmac_read_counter(wlc_hw, 0, M_MIMO_TXDUR_3X_L(wlc_hw),
				M_MIMO_TXDUR_3X_H(wlc_hw));
		sifs_time_siso = wlc_bmac_read_counter(wlc_hw, 0, M_SISO_SIFS_L(wlc_hw),
				M_SISO_SIFS_H(wlc_hw));
		sifs_time_mimo = wlc_bmac_read_counter(wlc_hw, 0, M_MIMO_SIFS_L(wlc_hw),
				M_MIMO_SIFS_H(wlc_hw));

		/* calculate change from last time, converting slots to time */
		delta_idle_mimo = TIME_DIFF(idle_slotcnt_mimo,
		                            last->total_idle_time_mimo);
		delta_idle_siso = TIME_DIFF(idle_slotcnt_siso,
		                            last->total_idle_time_siso);
		delta_idle_mimo *= slottime;
		delta_idle_siso *= slottime;

		delta_sifs_mimo = TIME_DIFF(sifs_time_mimo, last->total_sifs_time_mimo);
		delta_sifs_siso = TIME_DIFF(sifs_time_siso, last->total_sifs_time_siso);
		delta_sifs_mimo *= 16;
		delta_sifs_siso *= 16;

		delta_rx_mimo = TIME_DIFF(rx_time_mimo,
		                          last->total_rx_time_mimo);
		delta_rx_siso = TIME_DIFF(rx_time_siso,
		                          last->total_rx_time_siso);

		if (mhfval & MHF3_PM_BCNRX) {
			/* Ucode MIMO/SISO counts apply directly */
			curr->total_idle_time_mimo += delta_idle_mimo;
			curr->total_idle_time_mimo += delta_sifs_mimo;
			curr->total_idle_time_siso += delta_idle_siso;
			curr->total_idle_time_siso += delta_sifs_siso;
			curr->total_rx_time_mimo += delta_rx_mimo;
			curr->total_rx_time_siso += delta_rx_siso;
#ifdef OCL
			if (ocl_en) {
				curr->total_idle_time_ocl += delta_idle_mimo;
				curr->total_idle_time_ocl += delta_sifs_mimo;
				curr->total_rx_time_ocl += delta_rx_mimo;
			}
#endif /* OCL */
		} else {
			/* Ucode split is meaningless, apply total to actual chain config */
			uint32 total_idle_time = delta_idle_mimo + delta_idle_siso +
				delta_sifs_mimo + delta_sifs_siso;
			uint32 total_rx_time = delta_rx_mimo + delta_rx_siso;

			if (wlc->stf->rxchain & (wlc->stf->rxchain - 1)) {
				/* More than one bit in bitmask */
				curr->total_idle_time_mimo += total_idle_time;
				curr->total_rx_time_mimo += total_rx_time;
#ifdef OCL
				if (ocl_en) {
					curr->total_rx_time_ocl +=
							total_rx_time;
					curr->total_idle_time_ocl +=
							total_idle_time;
				}
#endif /* OCL */
			} else {
				curr->total_idle_time_siso += total_idle_time;
				curr->total_rx_time_siso += total_rx_time;
			}
		}

		/* Update per-chain transmit counters */
		curr->total_tx_time_1chain +=
			TIME_DIFF(tx_time_1chain, last->total_tx_time_1chain);
		curr->total_tx_time_2chain +=
			TIME_DIFF(tx_time_2chain, last->total_tx_time_2chain);
		curr->total_tx_time_3chain +=
			TIME_DIFF(tx_time_3chain, last->total_tx_time_3chain);

		/* prep for trace */
		trace.idle_slotcnt_mimo = idle_slotcnt_mimo;
		trace.last_idle_slotcnt_mimo = last->total_idle_time_mimo;
		trace.idle_slotcnt_siso = idle_slotcnt_siso;
		trace.last_idle_slotcnt_siso = last->total_idle_time_siso;
		trace.rx_time_mimo = rx_time_mimo;
		trace.last_rx_time_mimo = last->total_rx_time_mimo;
		trace.rx_time_siso = rx_time_siso;
		trace.last_rx_time_siso = last->total_rx_time_siso;
		trace.tx_time_1chain = tx_time_1chain;
		trace.last_tx_time_1chain = last->total_tx_time_1chain;
		trace.tx_time_2chain = tx_time_2chain;
		trace.last_tx_time_2chain = last->total_tx_time_2chain;
		trace.tx_time_3chain = tx_time_3chain;
		trace.last_tx_time_3chain = last->total_tx_time_3chain;

		/* Save current values as ref for next time */
		last->total_idle_time_mimo = idle_slotcnt_mimo;
		last->total_idle_time_siso = idle_slotcnt_siso;
		last->total_rx_time_mimo = rx_time_mimo;
		last->total_rx_time_siso = rx_time_siso;
		last->total_tx_time_1chain = tx_time_1chain;
		last->total_tx_time_2chain = tx_time_2chain;
		last->total_tx_time_3chain = tx_time_3chain;
		last->total_sifs_time_mimo = sifs_time_mimo;
		last->total_sifs_time_siso = sifs_time_siso;

		trace.len = sizeof(trace) - sizeof(trace.type) - sizeof(trace.len);
		trace.type = WL_MIMO_PS_METRICS_SNAPSHOT_TRACE_TYPE;
		trace.reason = reason;
		trace.reset_last = reset_last;
#ifdef EVENT_LOG_COMPILE
		EVENT_LOG_BUFFER(EVENT_LOG_TAG_MIMO_PS_TRACE, (void *)(&trace), sizeof(trace));
#endif // endif
	}

	if (reset_last) {
		/* clean up stale last values to sync with ucode
		 * SHM values reset (i.e. wlc_coreinit()) or
		 * reset_last=TRUE when required by the caller.
		 */
		wlc_mimo_siso_metrics_reset(wlc);
	}
}

int wlc_get_mimo_siso_meas_metrics(wlc_info_t *wlc, uint8 *destptr, int destlen)
{
	int rc;
	uint16 len;
	wl_mimo_meas_metrics_t * meas_metrics_ptr = (wl_mimo_meas_metrics_t*)destptr;
#ifdef OCL
	if (OCL_ENAB(wlc->pub)) {
		len = STRUCT_SIZE_THROUGH(meas_metrics_ptr, total_rx_time_ocl);
	} else
#endif /* OCL */
	{
		len = STRUCT_SIZE_THROUGH(meas_metrics_ptr, total_tx_time_3chain);
	}

	/* Make sure there's room for this section */
	if ((size_t) destlen < ROUNDUP(len, sizeof(uint32)))
		return BCME_BUFTOOSHORT;

	if (wlc->pub->hw_up && wlc->clk) {
		rc = wlc_mimo_siso_metrics_report(wlc, FALSE);
		if (rc != BCME_OK) {
			return rc;
		}
	}

	memcpy(destptr, (uint8 *)wlc->mimo_siso_metrics, len);
	meas_metrics_ptr->type = WL_PWRSTATS_TYPE_MIMO_PS_METRICS;
	meas_metrics_ptr->len = len;
	/* Report use of this segment (plus padding) */
	return (ROUNDUP(len, sizeof(uint32)));
}
#endif /* WL_MIMO_SISO_STATS */

#if defined(ECOUNTERS) && defined(EVENT_LOG_COMPILE)
static int
wlc_ecounters_pwrstats_phy(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len)
{
	wlc_info_t *wlc = (wlc_info_t*)context;
	uint16 slice_index;
	int ret = BCME_OK;
	/* Get a word aligned temporary buffer */
	uint32 buf[(ROUNDUP(sizeof(wl_pwr_phy_stats_t), sizeof(uint32))) / sizeof(uint32)];

	/* slice index to report */
	slice_index = (req->slice_mask & ECOUNTERS_STATS_TYPES_SLICE_MASK_SLICE0) ? 0 : 1;

	/* get the right wlc */
#ifdef WLRSDB
	wlc = RSDB_ENAB(wlc->pub) ? wlc->cmn->wlc[slice_index]:wlc;
#endif /* WLRSDB */

	ret = wlc_pwrstats_get_phy(wlc->pwrstats, (uint8*)buf,
			ROUNDUP(sizeof(wl_pwr_phy_stats_t), sizeof(uint32)));

	if (ret < 0)
	{
		goto fail;
	}

	ret = bcm_xtlv_put_data(xtlvbuf, stats_type, (uint8*)buf,
		ROUNDUP(sizeof(wl_pwr_phy_stats_t), sizeof(uint32)));
fail:
	if (ret == BCME_NOMEM) {
		*attempted_write_len = BCM_XTLV_HDR_SIZE + sizeof(wl_pwr_phy_stats_t);
	}
	return ret;
}

static int
wlc_ecounters_pwrstats_wake(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len)
{
	wlc_info_t *wlc = (wlc_info_t*)context;
	uint16 slice_index;
	int ret = BCME_OK;

	/* Get a word aligned temporary buffer */
	uint32 bf[(ROUNDUP(WLC_PWRSTATS_PM_AWAKE_V2_TOTAL_LEN, sizeof(uint32))) / sizeof(uint32)];

	/* slice index to report */
	slice_index = (req->slice_mask &
		ECOUNTERS_STATS_TYPES_SLICE_MASK_SLICE0) ? 0 : 1;

	/* get the right wlc */
#ifdef WLRSDB
	wlc = RSDB_ENAB(wlc->pub) ? wlc->cmn->wlc[slice_index]:wlc;
#endif /* WLRSDB */

	/* Copy to local buffer first */
	ret = wlc_pwrstats_get_wake(wlc, (uint8*)bf,
			ROUNDUP(WLC_PWRSTATS_PM_AWAKE_V2_TOTAL_LEN, sizeof(uint32)));

	if (ret < 0)
	{
		goto fail;
	}

	/* Write in event log format. Write in one shot. Each invocation of
	 * ecounters write function results in an event log record.
	 * We want all power stats in one record
	 */
	ret =  bcm_xtlv_put_data(xtlvbuf, stats_type, (uint8*)bf,
			ROUNDUP(sizeof(wl_pwr_pm_awake_stats_v2_t), sizeof(uint32)));
fail:
	if (ret == BCME_NOMEM) {
		*attempted_write_len = BCM_XTLV_HDR_SIZE + sizeof(wl_pwr_pm_awake_stats_v2_t);
	}
	return ret;
}
#endif /* ECOUNTERS */

int
wlc_pwrstats_get_radio(wlc_info_t *wlc, void *arg, uint16 len, uint8 *buf)
{
	uint8 *ptr = buf;
	wl_pwr_phy_stats_t *phy_stats;
	wl_pwr_pm_awake_stats_v2_t *wake_stats;
	uint32 *radio = (uint32 *)arg;
	int err;

	if ((err = wlc_pwrstats_get_phy(wlc->pwrstats, buf, len)) < 0) {
		return err;
	}
	phy_stats = (wl_pwr_phy_stats_t *) ptr;
	radio[2] = phy_stats->tx_dur;
	radio[3] = phy_stats->rx_dur;

	if ((err = wlc_pwrstats_get_wake(wlc, buf, len)) < 0) {
		return err;
	}
	wake_stats = (wl_pwr_pm_awake_stats_v2_t *) ptr;
	radio[0] = wake_stats->awake_data.pm_dur;
	radio[1] = wake_stats->awake_data.mpc_dur;

	return BCME_OK;
}
