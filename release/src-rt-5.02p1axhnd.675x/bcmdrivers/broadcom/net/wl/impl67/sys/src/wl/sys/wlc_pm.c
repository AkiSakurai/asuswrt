/*
 * Power Management Mode PM_FAST (PM2) functions
 *
 *   Copyright 2020 Broadcom
 *
 *   This program is the proprietary software of Broadcom and/or
 *   its licensors, and may only be used, duplicated, modified or distributed
 *   pursuant to the terms and conditions of a separate, written license
 *   agreement executed between you and Broadcom (an "Authorized License").
 *   Except as set forth in an Authorized License, Broadcom grants no license
 *   (express or implied), right to use, or waiver of any kind with respect to
 *   the Software, and Broadcom expressly reserves all rights in and to the
 *   Software and all intellectual property rights therein.  IF YOU HAVE NO
 *   AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *   WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *   THE SOFTWARE.
 *
 *   Except as expressly set forth in the Authorized License,
 *
 *   1. This program, including its structure, sequence and organization,
 *   constitutes the valuable trade secrets of Broadcom, and you shall use
 *   all reasonable efforts to protect the confidentiality thereof, and to
 *   use this information only in connection with your use of Broadcom
 *   integrated circuit products.
 *
 *   2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *   "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *   REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *   OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *   DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *   NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *   ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *   CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *   OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *   3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *   BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *   SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *   IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *   IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *   ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *   OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *   NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 *   <<Broadcom-WL-IPTag/Proprietary:>>
 *
 *   $Id: wlc_pm.c 786740 2020-05-06 14:10:37Z $
 */

/**
 * @file
 * @brief
 * Twiki: [WlDriverPowerSave]
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmwifi_channels.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.1d.h>
#include <802.11.h>
#include <802.11e.h>
#include <bcmip.h>
#include <wpa.h>
#include <vlan.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#if defined(WLTEST)
#include <bcmnvram.h>
#endif // endif
#include <wlc_event_utils.h>
#include <wlioctl.h>
#if defined(BCMSUP_PSK) || defined(STA) || defined(LINUX_CRYPTO)
#include <eapol.h>
#endif // endif
#include <bcmwpa.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <hndpmu.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_cca.h>
#include <wlc_interfere.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <wlc_led.h>
#include <wlc_frmutil.h>
#include <wlc_stf.h>
#ifdef WLNAR
#include <wlc_nar.h>
#endif // endif
#ifdef WLAMSDU
#include <wlc_amsdu.h>
#endif // endif
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#endif // endif
#ifdef WLDLS
#include <wlc_dls.h>
#endif // endif
#ifdef L2_FILTER
#include <wlc_l2_filter.h>
#endif // endif
#ifdef WLMCNX
#include <wlc_mcnx.h>
#include <wlc_tbtt.h>
#endif // endif
#ifdef WLP2P
#include <wlc_p2p.h>
#endif // endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#include <wlc_scb_ratesel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif /* WL_LPC */
#include <wlc_event.h>
#ifdef WOWL
#include <wlc_wowl.h>
#endif // endif
#ifdef WOWLPF
#include <wlc_wowlpf.h>
#endif // endif
#include <wlc_seq_cmds.h>
#ifdef WLOTA_EN
#include <wlc_ota_test.h>
#endif /* WLOTA_EN */
#ifdef WLDIAG
#include <wlc_diag.h>
#endif // endif
#include <wl_export.h>
#include "d11ucode.h"
#if defined(BCMSUP_PSK)
#include <wlc_sup.h>
#endif // endif
#if defined(BCMAUTH_PSK)
#include <wlc_auth.h>
#endif // endif
#ifdef WET
#include <wlc_wet.h>
#endif // endif
#ifdef PSTA
#include <wlc_psta.h>
#endif /* PSTA */
#if defined(BCMNVRAMW) || defined(WLTEST)
#include <bcmotp.h>
#endif // endif
#ifdef BCMCCMP
#include <aes.h>
#endif // endif
#include <wlc_rm.h>
#include "wlc_cac.h"
#include <wlc_ap.h>
#include <wlc_scan.h>
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
#ifdef WLWNM
#include <wlc_wnm.h>
#endif // endif
#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)
#include <rwl_shared.h>
#include <rwl_uart.h>
#endif /* RWL_DONGLE || UART_REFLECTOR */
#include <wlc_assoc.h>
#if defined(RWL_WIFI) || defined(WIFI_REFLECTOR)
#include <wlc_rwl.h>
#endif // endif
#ifdef WLPFN
#include <wl_pfn.h>
#endif // endif
#ifdef STA
#include <wlc_wpa.h>
#endif /* STA */
#include <wlc_lq.h>
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_dfs.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>
#include <bcm_mpool_pub.h>
#include <wlc_utils.h>
#include <wlc_hrt.h>
#include <wlc_prot.h>
#include <wlc_prot_g.h>
#define _inc_wlc_prot_n_preamble_	/* include static INLINE uint8 wlc_prot_n_preamble() */
#include <wlc_prot_n.h>
#include <wlc_probresp.h>
#ifdef WLTOEHW
#include <wlc_tso.h>
#endif /* WLTOEHW */
#ifdef WL11AC
#include <wlc_vht.h>
#endif // endif
#if defined(BCMWAPI_WPI) || defined(BCMWAPI_WAI)
#include <wlc_wapi.h>
#endif // endif
#include <wlc_pcb.h>
#include <wlc_txc.h>
#ifdef MFP
#include <wlc_mfp.h>
#endif // endif
#include <wlc_macfltr.h>
#include <wlc_addrmatch.h>
#ifdef WL_RELMCAST
#include "wlc_relmcast.h"
#endif // endif
#include <wlc_btcx.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_reg.h>
#include <wlc_akm_ie.h>
#include <wlc_ht.h>
#include <wlc_hs20.h>

#include <wlc_pm.h>

#ifdef WL_EXCESS_PMWAKE
#ifndef WL_PWRSTATS
#error WL_PWRSTATS is required for WL_EXCESS_PMWAKE
#endif /* !WL_PWRSTATS */
#endif /* WL_EXCESS_PMWAKE */

#ifdef WL_PWRSTATS
#include <wlc_pwrstats.h>
#endif /* WL_PWRSTATS */

#ifdef WL_LEAKY_AP_STATS
#include <wlc_leakyapstats.h>
#endif /* WL_LEAKY_AP_STATS */

#include <wlc_qoscfg.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>

#include <phy_calmgr_api.h>
#include <phy_utils_api.h>
#include <wlc_tx.h>

#ifdef WLC_ADPS
#include <wlc_adps.h>
#endif /* WLC_ADPS */

#ifdef STA
#ifdef WL_EXCESS_PMWAKE
static void wlc_get_ucode_dbg(wlc_info_t *wlc, wl_pmalert_ucode_dbg_t *ud);
#endif /* WL_EXCESS_PMWAKE */

/* Start the PM2 return to sleep timer */
void
wlc_pm2_sleep_ret_timer_start(wlc_bsscfg_t *cfg, uint period)
{
	wlc_pm_st_t *pm = cfg->pm;

#ifdef WL_PWRSTATS
	if (PWRSTATS_ENAB(cfg->wlc->pub)) {
		wlc_pwrstats_frts_start(cfg->wlc->pwrstats);
	}
#endif /* WL_PWRSTATS */
	/* the new timeout will supersede the old one so delete old one first */
	wlc_hrt_del_timeout(pm->pm2_ret_timer);

	/* If pm2 return to sleep timer is about to start for a cfg when there is no active
	* association then return without starting the timer. This is done to avoid
	* start <-> stop loop
	*/
	if (!cfg->associated)
		return;

	/* Schedule the timer with specific period, or use current threshold by default */
	wlc_hrt_add_timeout(pm->pm2_ret_timer,
	                    WLC_PM2_TICK_GP(period == 0 ? pm->pm2_sleep_ret_threshold : period),
	                    wlc_pm2_sleep_ret_timeout_cb, (void *)cfg);
}

/* Stop the PM2 tick timer */
void
wlc_pm2_sleep_ret_timeout_cb(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;

	if (cfg->pm->PM == PM_FAST) {
		wlc_pm2_sleep_ret_timeout(cfg);
	}
}

void
wlc_pm2_sleep_ret_timer_stop(wlc_bsscfg_t *cfg)
{
	wlc_pm_st_t *pm = cfg->pm;

#ifdef WL_PWRSTATS
	if (PWRSTATS_ENAB(cfg->wlc->pub)) {
		wlc_pwrstats_frts_end(cfg->wlc->pwrstats);
	}
#endif /* WL_PWRSTATS */

	if (pm->pm2_ret_timer != NULL)
		wlc_hrt_del_timeout(pm->pm2_ret_timer);

	WL_RTDC2(cfg->wlc, "wlc_pm2_sleep_ret_timer_stop", 0, 0);
}

uint8
wlc_get_pm_state(wlc_bsscfg_t *cfg)
{
	return cfg->pm->PM;
}

/* PM2 tick timer timeout handler */
void
wlc_pm2_sleep_ret_timeout(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	uint wake_time;
	uint last_wake_time;
	uint time_left;

	/* Ignore timeout if it occurs just as we exited PM2 (race condition). */
	if (pm->PM != PM_FAST) {
		WL_RTDC(cfg->wlc, "wlc_pm2_timeout: not PM2", 0, 0);
		return;
	}

	/* Ignore timeout if wl is no longer up.  wlc_hrt functions cannot do
	 * register reads when no clock is provided to the core.
	 */
	if (!wlc->pub->up) {
		WL_RTDC(cfg->wlc, "wlc_pm2_timeout: not up", 0, 0);
		return;
	}

	/* Do not pass &pm->pm2_last_wake_time directly to wlc_hrt_getdelta().
	 * wlc_hrt_getdelta() has a side effect of modifying the timestamp
	 * pointed to by its 2nd parameter.
	 */
	last_wake_time = pm->pm2_last_wake_time;
	wake_time = wlc_hrt_getdelta(wlc->hrti, &last_wake_time);
	wake_time = wake_time/1024;

	time_left = pm->pm2_sleep_ret_threshold - wake_time;

	/* 5 ms is to account for delays to avoid re-fires for small time */
	/* The check for > pm->pm2_sleep_ret_threshold is for the case of wraparound */
	if (time_left <= 5 || time_left > pm->pm2_sleep_ret_threshold) {
#ifdef WL_PWRSTATS
		if (PWRSTATS_ENAB(wlc->pub)) {
			wlc_pwrstats_frts_end(cfg->wlc->pwrstats);
		}
#endif /* WL_PWRSTATS */
		wlc_pm2_enter_ps(cfg);
	} else {
		/* Reschedule the PM2 tick timer for the remaining wake time */
#ifdef WLC_ADPS
		if (ADPS_ENAB(wlc->pub) &&
			wlc_adps_pm2_step_down(cfg) == PM_MAX) {
			return;
		}
#endif /* WLC_ADPS */
		wlc_pm2_sleep_ret_timer_start(cfg, time_left);

		if (SCAN_IN_PROGRESS(wlc->scan) && wlc_roam_scan_islazy(wlc, cfg, TRUE)) {
			if (wlc_lazy_roam_scan_suspend(wlc, cfg)) {
				/* Postpone background roam scan in presence of any activity
				 * Wait for the next power save mode enter check
				 */
				WL_SRSCAN(("Extend background roam scan home time by %dms "
					"(wake=%dms)",
					time_left + pm->pm2_sleep_ret_time, wake_time));
				WL_ASSOC(("Extend background roam scan home time by %dms "
					"(wake=%dms)",
					time_left + pm->pm2_sleep_ret_time, wake_time));
				wlc_scan_timer_update(wlc->scan,
					time_left + pm->pm2_sleep_ret_time);
			} else {
				/* Cancel background roam scan in presence of any activity */
				WL_SRSCAN(("Cancel background roam scan (wake=%dms)", wake_time));
				WL_ASSOC(("Cancel background roam scan (wake=%dms)", wake_time));
				wlc_assoc_abort(cfg);
			}
		}
	}
}

void
wlc_pm2_ret_upd_last_wake_time(wlc_bsscfg_t *cfg, uint32* tsf_l)
{
	if (!cfg)
		return;
	if (!cfg->pm)
		return;

	if (tsf_l) {
		cfg->pm->pm2_last_wake_time = *tsf_l;
#ifdef WLC_ADPS
		/* XXX wlc_adps_pm2_step_up() puts here temporarily to minimize a memory size
		 * increase in consideration of a absence of rx callbak funtion and locations where
		 * this function should call - wlc_recvdata, wlc_recvdata_orderd, and wlc_sendpkt.
		 */
		if (ADPS_ENAB(cfg->wlc->pub)) {
			wlc_adps_pm2_step_up(cfg);
		}
#endif /* WLC_ADPS */
	} else {
		/* Note: for BMAC drivers, calling wlc_hrt_gettime() here
		 * results in a very slow read across the bus.  This reduces
		 * throughput significantly if called from the data path.
		 */
		cfg->pm->pm2_last_wake_time = 0;
	}
}

/* Try enter PS mode in power mgmt mode PM_FAST.
 * If entered PS mode, stop the PM2 timers.
 */
void
wlc_pm2_enter_ps(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	uint txpktcnt = 0;

	/* Clear counters for next cycle before sleep */
	wlc_dfrts_reset_counters(cfg);

	txpktcnt = wlc_txpktcnt(wlc);

	/* Do not enter PS mode if:
	 * a) a scan is in progress, or
	 * b) there are pending tx packets and the Receive Throttle feature is
	 *    not enabled.
	 *    (Not entering PS mode for the OFF part of the receive
	 *    throttle duty cycle for any reason will defeat the main purpose
	 *    of the receive throttle feature - heat reduction.)
	 * c) we are no longer in PM2
	 * d) we are no longer associated to an AP
	 * e) PMenabledModuleId is non-zero, meaning another module put us in PS already
	 */
	if ((!PM2_RCV_DUR_ENAB(cfg) && (txpktcnt > 0)) ||
	    pm->PM != PM_FAST ||
	    !cfg->associated ||
	    pm->PM_override ||
	    pm->PMenabledModuleId) {

		WL_RTDC(wlc, "wlc_pm2_enter_ps: no PS, scan=%u txcnt=%u",
			SCAN_IN_PROGRESS(wlc->scan), wlc_txpktcnt(wlc));
		WL_RTDC(wlc, "    stas=%u AS_IN_PROGRESS=%d",
			wlc->stas_associated, AS_IN_PROGRESS(wlc));
		WL_RTDC(wlc, "    PM=%u", pm->PM, 0);
	}
	else if (AS_IN_PROGRESS(wlc) || SCAN_IN_PROGRESS(wlc->scan)) {
		WL_RTDC(wlc, "wlc_pm2_enter_ps: no PS, scan=%u AS_IN_PROGRESS=%d",
			SCAN_IN_PROGRESS(wlc->scan), AS_IN_PROGRESS(wlc));

		if (wlc_roam_scan_islazy(wlc, cfg, TRUE) &&
		    phy_utils_get_chanspec(WLC_PI(wlc)) == cfg->current_bss->chanspec) {
			/* Continue with scan in absence of any activity */
			WL_SRSCAN(("Resume background roam scan now"));
			wlc_scan_timer_update(wlc->scan, 0);
		}
	}
	else {
		/* Start entering PS mode */
		wlc_set_pmstate(cfg, TRUE);
#ifdef WLC_ADPS
		if (ADPS_ENAB(wlc->pub)) {
			wlc_adps_enter_sleep(cfg);
		}
#endif	/* WLC_ADPS */
#ifdef WLWNM
		if (WLWNM_ENAB(wlc->pub)) {
			(void)wlc_wnm_enter_sleep_mode(cfg);
		}
#endif /* WLWNM */
	}

	/* If we succeeded in starting to enter PS mode or
	 * if another module already put us in PS mode.
	 */
	if (pm->PMpending || pm->PMenabledModuleId) {
		WL_RTDC(wlc, "wlc_pm2_enter_ps: succeeded", 0, 0);

#ifdef WL_LEAKY_AP_STATS
		if (WL_LEAKYAPSTATS_ENAB(wlc->pub)) {
			wlc_leakyapstats_gt_reason_upd(wlc, wlc->primary_bsscfg,
				WL_LEAKED_GUARD_TIME_FRTS);
		}
#endif /* WL_LEAKY_AP_STATS */

		/* Enter the Receive Throttle feature state where we are in the
		 * transition between the ON and OFF parts of the duty cycle.  In this
		 * state we are waiting for a PM-indicated ACK to complete entering PS
		 * mode.
		 */
#if defined(WL_PM2_RCV_DUR_LIMIT)
		if (PM2_RCV_DUR_ENAB(cfg)) {
			/* non-zero pm->PMenabledModuleId means another module has already
			 * put us in PS mode.  This means that we won't get PM-indicated
			 * ACK because we're already in PS mode.  Thus, proceed to the next
			 * state, which is PM2RD_WAIT_BCN.
			 */
			if (pm->PMenabledModuleId) {
				pm->pm2_rcv_state = PM2RD_WAIT_BCN;
				WL_RTDC(wlc, "pm2_rcv_state=PM2RD_WAIT_BCN", 0, 0);
			}
			else {
				pm->pm2_rcv_state = PM2RD_WAIT_RTS_ACK;
				WL_RTDC(wlc, "pm2_rcv_state=PM2RD_WAIT_RTS_ACK", 0, 0);
			}
			wlc_pm2_rcv_timer_stop(cfg);
		}
#endif /* WL_PM2_RCV_DUR_LIMIT */

		/* Stop the PM2 tick timer */
		wlc_pm2_sleep_ret_timer_stop(cfg);
	}
	/* else we did not start entering PS mode */
	else {
		/* Restart the return to sleep timer to try again later */
		WL_RTDC2(wlc, "wlc_pm2_enter_ps: restart timer", 0, 0);
		wlc_pm2_sleep_ret_timer_start(cfg, 0);

#ifdef BCMDBG
		WL_RTDC(wlc, "wlc_pm2_enter_ps: enter PS failed, PMep=%02u AW=%02u",
			(pm->PMenabled ? 10 : 0) | pm->PMpending,
			(PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
		WL_RTDC(wlc, "    bss=%u assoc=%u", cfg->BSS, cfg->associated);
		WL_RTDC(wlc, "    dptpend=%u portopen=%u",
			FALSE, WLC_PORTOPEN(cfg));
#endif /* BCMDBG */
	}
}

#if defined(WL_PM2_RCV_DUR_LIMIT)
/* Reset the PM2 receive throttle duty cycle feature */
void
wlc_pm2_rcv_reset(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;

	/* The Receive Throttle feature overrides the TBTT wakeup interval
	 * specified by bcn_li_bcn and bcn_li_dtim.  So we must restore the
	 * wakeup interval to match bcn_li_bcn and bcn_li_dtim again.
	 */
	wlc_bcn_li_upd(wlc);

	/* Stop the receive throttle timer */
	wlc_pm2_rcv_timer_stop(cfg);
	/* Next beacon should start the timer */

	pm->pm2_rcv_state = PM2RD_IDLE;
	WL_RTDC(wlc, "pm2_rcv_state=PM2RD_IDLE", 0, 0);
}

/* Start the PM2 receive throttle duty cycle timer.
 *
 * This timer should be started at the beginning of the ON part of the
 * PM2 receive duty cycle.
 * When this timer expires, the OFF part of the duty cycle begins.
 * At the end of the current beacon period, the duty cycle ends.
 * (The end of the current beacon period is indicated by the next TBTT
 * wakeup.  This function adjusts the TBTT wake up interval to ensure
 * the wakeup interval is the same as the beacon interval.)
 */
void
wlc_pm2_rcv_timer_start(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;

	 /* Start the PM2 receive duration timer */
	 /* the new timeout will supersede the old one so delete old one first */
	wlc_hrt_del_timeout(pm->pm2_rcv_timer);
	wlc_hrt_add_timeout(pm->pm2_rcv_timer,
	                    WLC_PM2_TICK_GP(pm->pm2_rcv_time),
	                    wlc_pm2_rcv_timeout_cb, (void *)cfg);

	pm->pm2_rcv_state = PM2RD_WAIT_TMO;
	WL_RTDC(wlc, "pm2_rcv_state=PM2RD_WAIT_TMO", 0, 0);

	/* Ensure we will get our next TBTT wake up interrupt at the next beacon:
	 * update the beacon listen interval in shared memory to instruct ucode to
	 * wake up to listen to every beacon, as if bcn_li_bcn == 1 and
	 * bcn_li_dtim == 0.
	 *
	 * This is needed because the wakeup interval varies between 1 and DTIM
	 * beacon intervals depending on recent tx/rx activity.
	 */
	wlc_write_shm(wlc, M_PS_MORE_DTIM_TBTT(wlc),
		(0 /* bcn_li_dtim */  << 8) | 1 /* bcn_li_bcn */);
}

/* Stop the PM2 tick timer */
void
wlc_pm2_rcv_timeout_cb(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;

	if (cfg->pm->PM == PM_FAST) {
		wlc_pm2_rcv_timeout(cfg);
	}
}

void
wlc_pm2_rcv_timer_stop(wlc_bsscfg_t *cfg)
{
	wlc_pm_st_t *pm = cfg->pm;

	if (pm->pm2_rcv_timer != NULL)
		wlc_hrt_del_timeout(pm->pm2_rcv_timer);

	WL_RTDC2(cfg->wlc, "wlc_pm2_rcv_timer_stop", 0, 0);
}

/* PM2 tick timer timeout handler */
void
wlc_pm2_rcv_timeout(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;

	(void)wlc;

	/* Ignore timeout if it occurs just as we exited PM2 (race condition). */
	if (pm->PM != PM_FAST) {
		WL_RTDC(wlc, "wlc_pm2_rcv_timeout: not PM2", 0, 0);
		return;
	}

	/* Decrement the Receive Throttle countdown timers.
	 * If timer reaches 0, enter PS mode and stop the PM2 timer.
	 */
	if (PM2_RCV_DUR_ENAB(cfg)) {
		WL_RTDC(wlc, "wlc_pm2_rcv_timeout: RTDC enter ps", 0, 0);
		wlc_pm2_enter_ps(cfg);
		return;
	}

	/* Let the next beacon start next timer again */
}
#endif /* WL_PM2_RCV_DUR_LIMIT */

void
wlc_dfrts_reset_counters(wlc_bsscfg_t *bsscfg)
{
	WL_RTDC(bsscfg->wlc, "dfrts_reset", 0, 0);
	bsscfg->pm->dfrts_rx_pkts = 0;
	bsscfg->pm->dfrts_tx_pkts = 0;
	bsscfg->pm->dfrts_rx_bytes = 0;
	bsscfg->pm->dfrts_tx_bytes = 0;
	bsscfg->pm->dfrts_reached_threshold = FALSE;
	bsscfg->pm->pm2_sleep_ret_threshold = bsscfg->pm->pm2_sleep_ret_time;
}

/* Look at the updated tx/rx counters and restart the PM2 return to sleep
 * timer if needed.  This applies to both the FRTS and DFRTS features.
 */
void
wlc_update_sleep_ret(wlc_bsscfg_t *bsscfg, bool inc_rx, bool inc_tx,
	uint rxbytes, uint txbytes)
{
	wlc_pm_st_t *pm = bsscfg->pm;
	bool sleep_tmr_restart = FALSE;
	uint sleep_tmr_new_threshold = pm->pm2_sleep_ret_time;

	if (pm->PM != PM_FAST || !bsscfg->associated || pm->PMblocked)
		return;

	if (pm->PMenabled) {
		wlc_set_pmstate(bsscfg, FALSE);
		sleep_tmr_restart = TRUE;
	}

	/* Check if this is the first rx pkt since starting the sleep return
	 * timer for pm2_bcn_sleep_ret_time.
	 */
	if (inc_rx && pm->pm2_rx_pkts_since_bcn == 1 && pm->pm2_bcn_sleep_ret_time != 0)
	{
		WL_RTDC(bsscfg->wlc, "frts_upd: bcnsr -> frts", 0, 0);
		sleep_tmr_restart = TRUE;
	}

	/* If Dynamic FRTS is enabled and threshold is not reached */
	if ((pm->dfrts_logic != WL_DFRTS_LOGIC_OFF) && !pm->dfrts_reached_threshold) {
		if (inc_rx) {
			pm->dfrts_rx_pkts++;
			pm->dfrts_rx_bytes += rxbytes;
		}
		if (inc_tx) {
			pm->dfrts_tx_pkts++;
			pm->dfrts_tx_bytes += txbytes;
		}
		if (pm->dfrts_logic == WL_DFRTS_LOGIC_AND) {
			/* do the AND of all conditions whose algo enable bits are 1 */
			if (pm->dfrts_rx_pkts >= pm->dfrts_rx_pkts_threshold &&
					pm->dfrts_tx_pkts >= pm->dfrts_tx_pkts_threshold &&
					pm->dfrts_tx_pkts + pm->dfrts_rx_pkts >=
					pm->dfrts_txrx_pkts_threshold &&
					pm->dfrts_rx_bytes >= pm->dfrts_rx_bytes_threshold &&
					pm->dfrts_tx_bytes >= pm->dfrts_tx_bytes_threshold &&
					pm->dfrts_tx_bytes + pm->dfrts_rx_bytes >=
					pm->dfrts_txrx_bytes_threshold) {
				WL_RTDC(bsscfg->wlc, "frts_upd: dfrts lo ->hi, AND", 0, 0);
				pm->dfrts_reached_threshold = TRUE;
				sleep_tmr_restart = TRUE;
				sleep_tmr_new_threshold = pm->dfrts_high_ms;
			}
		}
		else if (pm->dfrts_logic == WL_DFRTS_LOGIC_OR) {
			/* do the OR of all conditions whose algo enable bits are 1 */
			if ((pm->dfrts_rx_pkts_threshold > 0 &&
					pm->dfrts_rx_pkts >= pm->dfrts_rx_pkts_threshold) ||
					(pm->dfrts_tx_pkts_threshold > 0 &&
					pm->dfrts_tx_pkts >= pm->dfrts_tx_pkts_threshold) ||
					(pm->dfrts_txrx_pkts_threshold > 0 &&
					(pm->dfrts_tx_pkts + pm->dfrts_rx_pkts) >=
					pm->dfrts_txrx_pkts_threshold) ||
					(pm->dfrts_rx_bytes_threshold > 0 &&
					pm->dfrts_rx_bytes >= pm->dfrts_rx_bytes_threshold) ||
					(pm->dfrts_tx_bytes_threshold > 0 &&
					pm->dfrts_tx_bytes >= pm->dfrts_tx_bytes_threshold) ||
					(pm->dfrts_txrx_bytes_threshold > 0 &&
					(pm->dfrts_tx_bytes + pm->dfrts_rx_bytes) >=
					pm->dfrts_txrx_bytes_threshold)) {
				WL_RTDC(bsscfg->wlc, "frts_upd: dfrts lo ->hi, OR", 0, 0);
				pm->dfrts_reached_threshold = TRUE;
				sleep_tmr_restart = TRUE;
				sleep_tmr_new_threshold = pm->dfrts_high_ms;
			}
		}
	}

	if (sleep_tmr_restart) {
		/* Immediately restart the sleep return timer */
		WL_RTDC(bsscfg->wlc, "frts_upd: restart timer for %u", sleep_tmr_new_threshold, 0);

#ifdef WL_PWRSTATS
		if (PWRSTATS_ENAB(bsscfg->wlc->pub)) {
			wlc_pwrstats_set_frts_data(bsscfg->wlc->pwrstats, TRUE);
		}
#endif /* WL_PWRSTATS */

		pm->pm2_sleep_ret_threshold = sleep_tmr_new_threshold;
		wlc_pm2_sleep_ret_timer_start(bsscfg, 0);
	}
}

#ifdef WL_EXCESS_PMWAKE
static void
wlc_create_pmalert_data(wlc_info_t *wlc, wl_pmalert_t **pm_alert_data, uint32 *datalen)
{
	uint8 *data;
	wl_pmalert_fixed_t *fixed;
	uint32 len;
	*datalen = 0;

	len = sizeof(wl_pmalert_t) - 1 + sizeof(wl_pmalert_fixed_t) +
		sizeof(wl_pmalert_pmstate_t) + 2 * sizeof(wl_pmalert_event_dur_t) +
		2 * sizeof(uint32) * (WLC_PMD_EVENT_MAX - 1) +
		sizeof(wlc_pm_debug_t) * (WLC_STA_AWAKE_STATES_MAX - 1) +
		sizeof(wl_pmalert_ucode_dbg_t);

	ASSERT(pm_alert_data != NULL && datalen != NULL && *pm_alert_data == NULL);
	*pm_alert_data = MALLOCZ(wlc->osh, len);

	if (*pm_alert_data != NULL) {
		*datalen = len;
		data = (*pm_alert_data)->data;

		fixed = (wl_pmalert_fixed_t *)data;
		fixed->type = WL_PMALERT_FIXED;
		fixed->len = sizeof(wl_pmalert_fixed_t);
		fixed->prev_pm_dur = wlc->excess_pm_last_pmdur;
		fixed->pm_dur = wlc_get_accum_pmdur(wlc);

		fixed->curr_time = OSL_SYSUPTIME();
		fixed->prev_stats_time = wlc->excess_pm_last_osltime;

		fixed->cal_dur = phy_calmgr_get_cal_dur(WLC_PI(wlc));
		fixed->prev_cal_dur = wlc->excess_pmwake->last_cal_dur;
		fixed->prev_frts_dur = wlc->excess_pmwake->last_frts_dur;
#ifdef WLPFN
		fixed->prev_mpc_dur = wlc->excess_pm_last_mpcdur;
		fixed->mpc_dur = wlc_get_mpc_dur(wlc);
#endif // endif
		fixed->hw_macc = R_REG(wlc->osh, D11_MACCONTROL(wlc));
		fixed->sw_macc = wlc->hw->maccontrol;
		if (PWRSTATS_ENAB(wlc->pub)) {
			data = wlc_pwrstats_fill_pmalert(wlc, data);
		}
		wlc_get_ucode_dbg(wlc, (wl_pmalert_ucode_dbg_t *)data);
	}
}

void
wlc_generate_pm_alert_event(wlc_info_t *wlc, uint32 reason, void *data, uint32 datalen)
{
	uint8 temp[MIN_PM_ALERT_LEN];
	wl_pmalert_t *pm_alert = (wl_pmalert_t *) data;
	if (pm_alert == NULL) {
		pm_alert = (wl_pmalert_t *)temp;
		datalen = MIN_PM_ALERT_LEN;
	}
	pm_alert->version = WL_PM_ALERT_VERSION;
	pm_alert->length = datalen;
	pm_alert->reasons = reason;
	wlc_mac_event(wlc, WLC_E_EXCESS_PM_WAKE_EVENT,
		NULL, 0, 0, 0, pm_alert, datalen);
}

uint32
wlc_get_pfn_ms(wlc_info_t *wlc)
{
	uint32 curr_pfn_scan_time = 0;

	/* Calculate awake time due to active pfn scan */
	if (SCAN_IN_PROGRESS(wlc->scan)) {
#if defined(WLPFN)
		if (WLPFN_ENAB(wlc->pub)) {
			if (wl_pfn_scan_in_progress(wlc->pfn)) {
				curr_pfn_scan_time = wlc_get_curr_scan_time(wlc);
			}
		}
#endif /* WLPFN */
	}
	return wlc->excess_pmwake->pfn_scan_ms + curr_pfn_scan_time;
}

uint32
wlc_get_roam_ms(wlc_info_t *wlc)
{
	return wlc->excess_pmwake->roam_ms + wlc_curr_roam_scan_time(wlc) +
		wlc_pwrstats_curr_connect_time(wlc->pwrstats);
}

static void
wlc_get_ucode_dbg(wlc_info_t *wlc, wl_pmalert_ucode_dbg_t *ud)
{
	uint32 i;
	ud->type = WL_PMALERT_UCODE_DBG_V2;
	ud->len = sizeof(wl_pmalert_ucode_dbg_t);
	ud->macctrl = R_REG(wlc->osh, D11_MACCONTROL(wlc));
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		ud->m_p2p_hps = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_HPS_OFFSET(wlc));
		for (i = 0; i < MAX_P2P_BSS_DTIM_PRD; i++)
			ud->m_p2p_bss_dtim_prd[i] =
				wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_DTIM_PRD(wlc, i));
	}
#endif /* WLMCNX */
	for (i = 0; i < 20; i++) {
		ud->psmdebug[i] = R_REG(wlc->osh, D11_PSM_DEBUG(wlc));
		ud->phydebug[i] = R_REG(wlc->osh, D11_PHY_DEBUG(wlc));
	}
	ud->psm_brc = R_REG(wlc->osh, D11_PSM_BRC_0(wlc));
	ud->ifsstat = R_REG(wlc->osh, D11_IFS_STAT(wlc));
#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub)) {
		uint32 j;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 12; j++) {
				ud->M_P2P_BSS[i][j] =
					wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS(wlc, i, j));
			}
			ud->M_P2P_PRE_TBTT[i] = wlc_mcnx_read_shm(wlc->mcnx,
					M_P2P_PRE_TBTT(wlc, i));
		}
	}
#endif /* WLMCNX */

	ud->psm_maccommand = R_REG(wlc->osh, D11_PSM_MACCOMMAND(wlc));
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		ud->txe_status1 = R_REG(wlc->osh, D11_TXE_STATUS1(wlc));
		if (D11REV_GE(wlc->pub->corerev, 65)) {
			ud->AQMFifoReady = R_REG(wlc->osh, D11_AQMFifoRdy_L(wlc));
			ud->AQMFifoReady = ud->AQMFifoReady |
				(R_REG(wlc->osh, D11_AQMFifoRdy_H(wlc)) << 16);
		} else {
			ud->AQMFifoReady = R_REG(wlc->osh, D11_AQMFifoReady(wlc));
		}
	} else {
		ud->xmtfifordy = R_REG(wlc->osh, D11_xmtfifordy(wlc));
	}
}

/* reset epm dur at start of new pm period */
void
wlc_reset_epm_dur(wlc_info_t *wlc)
{
	wlc_excess_pm_wake_info_t *epm = wlc->excess_pmwake;
	wlc->excess_pm_last_osltime = OSL_SYSUPTIME();
#ifdef WLPFN
	wlc->excess_pm_last_mpcdur = wlc_get_mpc_dur(wlc);
#endif // endif
	wlc->excess_pm_last_pmdur = wlc_get_accum_pmdur(wlc);
	if (wlc->clk) {
		/* Note the roam and pfn ts for next pm_wake period */
		epm->last_pm_prd_roam_ts = wlc_get_roam_ms(wlc);
		epm->last_pm_prd_pfn_ts = wlc_get_pfn_ms(wlc);
	}
	wlc_pwrstats_frts_checkpoint(wlc->pwrstats);
	epm->last_frts_dur = wlc_pwrstats_get_frts_data_dur(wlc->pwrstats);
	epm->last_cal_dur = phy_calmgr_get_cal_dur(WLC_PI(wlc));
	wlc_pwrstats_copy_event_wake_dur(epm->pp_start_event_dur, wlc->pwrstats);
	if (wlc->excess_pmwake->pm_alert_data) {
		MFREE(wlc->osh, wlc->excess_pmwake->pm_alert_data,
			wlc->excess_pmwake->pm_alert_datalen);
		wlc->excess_pmwake->pm_alert_data = NULL;
	}
	wlc->excess_pmwake->pm_alert_datalen = 0;
}

void
wlc_reset_epm_ca(wlc_info_t *wlc)
{
	wlc_excess_pm_wake_info_t *epm = wlc->excess_pmwake;
	epm->last_ca_pmdur = wlc_get_accum_pmdur(wlc);
	epm->last_ca_osl_time = OSL_SYSUPTIME();
	epm->ca_txrxpkts = 0;
	wlc_pwrstats_copy_event_wake_dur(epm->ca_start_event_dur, wlc->pwrstats);
}

#define SEC_TO_MS      1000

/* excess pm constant awake */
static void
wlc_process_epm_ca(wlc_info_t *wlc, uint32 cur_time, uint32 cur_pmdur)
{
	wlc_excess_pm_wake_info_t *epm = wlc->excess_pmwake;

	if (epm->ca_thresh == 0)
		return;

#if defined(ENABLE_DUMP_PM)
	/* XXX: In order to debug PM, just sendup WLC_E_EXCESS_PM_WAKE_EVENT
	 * It don't need event DATA, calling wlc_fatal_error().
	 */
	if (epm->last_ca_pmdur == cur_pmdur) {
		/* const awake thresh exceeded? */
		if (cur_time - epm->last_ca_osl_time >= (epm->ca_thresh * SEC_TO_MS)) {
			if (epm->ca_txrxpkts <= epm->ca_pkts_allowance) {
				/* Crossing of threshold results in alert */
				wlc_generate_pm_alert_event(wlc, CONST_AWAKE_DUR_ALERT, NULL, 0);
			}
			wlc_reset_epm_ca(wlc);
		}
	} else {
		wlc_reset_epm_ca(wlc);
	}
#else
	if (epm->last_ca_pmdur == cur_pmdur) {
		/* const awake thresh exceeded? */
		if (cur_time - epm->last_ca_osl_time >= (epm->ca_thresh * 1000)) {
			if (epm->ca_txrxpkts <= epm->ca_pkts_allowance) {
				wl_pmalert_t *pm_alert_data = NULL;
				uint32 datalen;
				/* Trigger recovery on second crossing of threshold */
				if (epm->ca_alert_pmdur_valid &&
					epm->ca_alert_pmdur == cur_pmdur) {
					uint8 pm_backup = wlc->primary_bsscfg->pm->PM;
					wlc_create_pmalert_data(wlc, &pm_alert_data, &datalen);
					wlc_generate_pm_alert_event(wlc, CONST_AWAKE_DUR_RECOVERY,
						pm_alert_data, pm_alert_data ? datalen : 0);

					wlc_fatal_error(wlc);

					/* PM mode transition */
					wlc_set_pm_mode(wlc, 0, wlc->primary_bsscfg);
					wlc_set_pm_mode(wlc, pm_backup, wlc->primary_bsscfg);
					epm->ca_alert_pmdur = 0;
					epm->ca_alert_pmdur_valid = 0;

				} else {
					/* 1st crossing of threshold results in alert */
					wlc_create_pmalert_data(wlc, &pm_alert_data, &datalen);
					wlc_generate_pm_alert_event(wlc, CONST_AWAKE_DUR_ALERT,
						pm_alert_data, pm_alert_data ? datalen : 0);
					epm->ca_alert_pmdur = cur_pmdur;
					epm->ca_alert_pmdur_valid = 1;
				}

				if (pm_alert_data)
					MFREE(wlc->osh, pm_alert_data, datalen);
			} else {
				epm->ca_alert_pmdur = 0;
				epm->ca_alert_pmdur_valid = 0;
			}

			/* Reset dur to generate next event */
			wlc_reset_epm_ca(wlc);
		}
	} else {
		/* pmdur changed -> !constantly awake */
		wlc_reset_epm_ca(wlc);
		epm->ca_alert_pmdur = 0;
		epm->ca_alert_pmdur_valid = 0;
	}
#endif /* ENABLE_DUMP_PM */
}

void
wlc_check_excess_pm_awake(wlc_info_t *wlc)
{
	uint32 cur_time, delta_time;
	uint32 cur_pmdur;
	uint32 cur_dur, last_dur;

	wlc_excess_pm_wake_info_t *epm = wlc->excess_pmwake;
	bool wasup = wlc->pub->hw_up;

	if (!wasup)
		wlc_corereset(wlc, WLC_USE_COREFLAGS);

	cur_time = OSL_SYSUPTIME();
	cur_pmdur = wlc_get_accum_pmdur(wlc);

	if (wlc->stas_associated)
		wlc_process_epm_ca(wlc, cur_time, cur_pmdur);

	delta_time = cur_time - wlc->excess_pm_last_osltime;

	if (wlc->stas_associated) {
		cur_dur = cur_pmdur;
		last_dur = wlc->excess_pm_last_pmdur;
	} else
#ifdef WLPFN
	{
		cur_dur = wlc_get_mpc_dur(wlc);
		last_dur = wlc->excess_pm_last_mpcdur;
	}
#else
	goto exit;
#endif /* WLPFN */

	if (wlc->excess_pm_period && (cur_dur >= last_dur) &&
		(delta_time >= (cur_dur - last_dur))) {

		uint32 idle_time = delta_time - (cur_dur - last_dur);
		uint32 excess_pm_threshold =
			(uint32)(wlc->excess_pm_percent * wlc->excess_pm_period * 10);

		/* Check idle awake time >= excess_pm_percent of sampled period */
		if ((idle_time >= excess_pm_threshold) &&
			(wlc->excess_pmwake->pm_alert_data == NULL)) {
			uint32 scantime_adj = 0;
			/* Reduce the wake time because of pfn or roam */
			if (epm->pfn_alert_thresh) {
				scantime_adj = (wlc_get_pfn_ms(wlc) - epm->last_pm_prd_pfn_ts);
				if (idle_time >= scantime_adj) {
					idle_time -= scantime_adj;
				} else {
					idle_time = 0;
					WL_ERROR(("Idle wrap, idl %d pfn %d %d", idle_time,
							wlc_get_pfn_ms(wlc),
							epm->last_pm_prd_pfn_ts));
				}
			}
			if (epm->roam_alert_thresh && idle_time) {
				scantime_adj = (wlc_get_roam_ms(wlc) - epm->last_pm_prd_roam_ts);
				if (idle_time >= scantime_adj) {
					idle_time -= scantime_adj;
				} else {
					idle_time = 0;
					WL_ERROR(("Idle wrap, idl %d roam %d %d ", idle_time,
							wlc_get_roam_ms(wlc),
							epm->last_pm_prd_roam_ts));
				}
			}

			/* Create PM alert if the adjusted awake time hit the threshhold */
			if (idle_time >= excess_pm_threshold)
				wlc_create_pmalert_data(wlc, &wlc->excess_pmwake->pm_alert_data,
					&wlc->excess_pmwake->pm_alert_datalen);
		}
	}

	if (wlc->excess_pm_period && delta_time >= (uint32)wlc->excess_pm_period * 1000) {
		/* Check if PM Alert data has been collected */
		if (wlc->excess_pmwake->pm_alert_datalen) {
			uint32 reason;
			if (wlc->primary_bsscfg->pm->PM)
				reason = PM_DUR_EXCEEDED;
			else
				/* For mpc mode a separate timer (to trigger at
				 * excess_pm_period) is not started. So the event
				 * (reason code MPC_DUR_EXCEEDED) gets sent
				 * on next chip-wake up. So for e.g. if PFN scan
				 * is scheduled at 30-sec interval,
				 * excess_pm_period = 10sec, excess_pm_percent = 10%.
				 * If the PFN takes more than 1 sec but less than 3 sec
				 * to complete, the event would not  be sent. It did exceed
				 * the 10 percent (i.e. 1sec) if last excess_pm_period is
				 * considered but since we check it on next wakeup i.e.
				 * after 30-seconds, it did not exceed 10 percent of this
				 * interval.
				 */
				reason = MPC_DUR_EXCEEDED;
			if (wlc->excess_pmwake->pm_alert_data) {
				wlc_generate_pm_alert_event(wlc, reason,
					wlc->excess_pmwake->pm_alert_data,
					wlc->excess_pmwake->pm_alert_datalen);
			} else {
				/* Allocate PM alert data seems failed, send the event
				 * without data
				 */
				wlc_generate_pm_alert_event(wlc, reason, NULL, 0);
			}
		}
		wlc_reset_epm_dur(wlc);
	}
#ifndef WLPFN
exit:
#endif /* WLPFN */

	if (!wasup)
		wlc_coredisable(wlc->hw);
	return;
}

void
wlc_check_roam_alert_thresh(wlc_info_t *wlc)
{
	wlc_excess_pm_wake_info_t *epm = wlc->excess_pmwake;

	if (epm->roam_alert_enable) {
		uint32 roam_time_spent = wlc_get_roam_ms(wlc) - epm->roam_alert_thresh_ts;
		if (roam_time_spent > BCM_INT32_MAX) {
			WL_ERROR(("roam_time err:rm_ms %d thr_ts %d",
				wlc->excess_pmwake->roam_ms, epm->roam_alert_thresh_ts));
		}
		else if (roam_time_spent > epm->roam_alert_thresh) {
			wlc_generate_pm_alert_event(wlc, ROAM_ALERT_THRESH_EXCEEDED, NULL, 0);
			/* Disable further events */
			epm->roam_alert_enable = FALSE;
		}
	}
}
void
wlc_epm_roam_time_upd(wlc_info_t *wlc, uint32 connect_dur)
{
	wlc->excess_pmwake->roam_ms += connect_dur;
	wlc_check_roam_alert_thresh(wlc);
}
#endif /* WL_EXCESS_PMWAKE */

/* Start PS mode depending on RX activity */
int
wlc_pm2_start_ps(wlc_bsscfg_t *cfg)
{
	wlc_pm_st_t *pm = cfg->pm;

	ASSERT(BSSCFG_PM_ALLOWED(cfg));

	if (pm->PMenabled) {
		return BCME_ERROR;
	}

	/* wlc_pm2_sleep_ret_timeout routine
	 * will decide whether to enter PS mode immediately
	 * or to postpone it if RX is active.
	 */
	wlc_pm2_sleep_ret_timeout(cfg);

	return BCME_OK;
}

static void
_wlc_pspoll_timer(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;

	if (pm->PMblocked || AS_IN_PROGRESS(wlc))
		return;

	if (pm->PMpending)
		return;

	if (wlc_sendpspoll(wlc, cfg) == FALSE) {
		WL_ERROR(("wl%d: %s: wlc_sendpspoll() failed\n", wlc->pub->unit, __FUNCTION__));
	}
}

static void
wlc_pspoll_timer(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;

	ASSERT(cfg->associated && cfg->pm->PMenabled && cfg->pm->pspoll_prd != 0);

	_wlc_pspoll_timer(cfg);
}

#if defined(STA) && defined(ADV_PS_POLL)
void
wlc_adv_pspoll_upd(wlc_info_t *wlc, struct scb *scb, void *pkt, bool sent, uint fifo)
{
	wlc_bsscfg_t *cfg;
	wlc_pm_st_t *pm;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	pm = cfg->pm;

	/* If this was the last frame sent, send a pspoll to check any buffered
	 * frames at AP, before going back to sleep
	 */
	if (!BSSCFG_STA(cfg) || BSSCFG_IBSS(cfg) || !pm->adv_ps_poll)
		return;

	ASSERT(fifo < WLC_HW_NFIFO_INUSE(wlc));

	ASSERT(WLPKTTAG(pkt)->flags & WLF_TXHDR);

	if (!sent &&
	    pm->PM == PM_MAX) {
		struct dot11_management_header *h = (struct dot11_management_header *)
		        ((uint8 *)PKTDATA(wlc->osh, pkt) + D11_TXH_LEN_EX(wlc));
		uint16 fc = ltoh16(h->fc);
		bool qos = FC_SUBTYPE_ANY_QOS(FC_SUBTYPE(fc));

		if (!qos ||
		    !(fifo <= TX_AC_VO_FIFO &&
		      AC_BITMAP_TST(scb->apsd.ac_trig, WME_PRIO2AC(PKTPRIO(pkt))))) {
			pm->send_pspoll_after_tx = TRUE;
		}
	} else if (sent && !pm->PSpoll &&
	         TXPKTPENDTOT(wlc) == 0) {
		if (pm->send_pspoll_after_tx) {
			/* Re-using existing implementation of pspoll_prd */
			_wlc_pspoll_timer(cfg);
			pm->send_pspoll_after_tx = FALSE;
		}
	}
} /* wlc_adv_pspoll_upd */
#endif /* STA && ADV_PS_POLL */

static const uint8 apsd_trig_acbmp2maxprio[] = {
	PRIO_8021D_BE, PRIO_8021D_BE, PRIO_8021D_BK, PRIO_8021D_BK,
	PRIO_8021D_VI, PRIO_8021D_VI, PRIO_8021D_VI, PRIO_8021D_VI,
	PRIO_8021D_VO, PRIO_8021D_VO, PRIO_8021D_VO, PRIO_8021D_VO,
	PRIO_8021D_VO, PRIO_8021D_VO, PRIO_8021D_VO, PRIO_8021D_VO
};

static bool
_wlc_sendapsdtrigger(wlc_info_t *wlc, struct scb *scb)
{
	wlc_bsscfg_t *cfg;
	wlc_wme_t *wme;
	int prio;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	wme = cfg->wme;

	/* Select the prio based on valid values and user override. */
	prio = (int)apsd_trig_acbmp2maxprio[wme->apsd_trigger_ac & scb->apsd.ac_trig];

	return wlc_sendnulldata(wlc, cfg, &scb->ea, 0, 0, prio, NULL, NULL);
}

bool
wlc_sendapsdtrigger(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	ASSERT(cfg != NULL);
	ASSERT(cfg->associated);

	scb = wlc_scbfind(wlc, cfg, &cfg->BSSID);
	if (scb == NULL) {
		WL_ERROR(("wl%d.%d: %s: unable to find scb for %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		          bcm_ether_ntoa(&cfg->BSSID, eabuf)));
		return FALSE;
	}

	if (!_wlc_sendapsdtrigger(wlc, scb)) {
		WL_ERROR(("wl%d.%d: %s: unable to send apsd trigger frame to %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		          bcm_ether_ntoa(&cfg->BSSID, eabuf)));
		return FALSE;
	}

	return TRUE;
}

/**
 * callback function for the APSD Trigger Frame Timer. This is also used by WME_AUTO_TRIGGER routine
 * to send out QoS NULL DATA frame as trigger to AP
 */
static void
wlc_apsd_trigger_timeout(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = cfg->wlc;
	struct scb *scb;

	WL_INFORM(("wl%d: %s, entering\n", wlc->pub->unit, __FUNCTION__));

	ASSERT(cfg->pm->PMenabled);
	ASSERT(cfg->associated);

	/*
	 * The Trigger Frame Timer has expired, so send a QoS NULL
	 * data packet of the highest WMM APSD-enabled AC priority.
	 * The Trigger Frame Timer will be reloaded elsewhere.
	 */
	scb = wlc_scbfindband(wlc, cfg, &cfg->BSSID,
	                      CHSPEC_BANDUNIT(cfg->current_bss->chanspec));
	if (scb == NULL) {
		WL_ERROR(("wl%d: %s : failed to find scb\n", wlc->pub->unit, __FUNCTION__));
		/* Link is down so clear the timer */
		wlc_apsd_trigger_upd(cfg, FALSE);
		return;
	}

	/*
	 * Don't send excessive null frames. Bail out if a QoS Data/Null frame
	 * was send out recently.
	 */
	if (scb->flags & SCB_SENT_APSD_TRIG) {
		scb->flags &= ~SCB_SENT_APSD_TRIG;
		WL_PS(("wl%d.%d: %s: trigger frame was recently send out\n",
		       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return;
	}

	if (!_wlc_sendapsdtrigger(wlc, scb))
		WL_ERROR(("wl%d: failed to send apsd trigger frame\n", wlc->pub->unit));

	return;
}

/*
 * Check if STA's bit is set in TIM
 * See Tality MS/futils.c: boFUTILS_InTIM()
 */
static bool
wlc_InTIM(wlc_bsscfg_t *cfg, bcm_tlv_t *tim_ie, uint tim_ie_len)
{
	uint pvboff, AIDbyte;
	uint pvblen;

	if (tim_ie == NULL ||
	    tim_ie->len < DOT11_MNG_TIM_FIXED_LEN ||
	    tim_ie->data[DOT11_MNG_TIM_DTIM_COUNT] >= tim_ie->data[DOT11_MNG_TIM_DTIM_PERIOD]) {
		/* sick AP, prevent going to power-save mode */
#ifdef BCMDBG
		if (tim_ie == NULL) {
			WL_PRINT(("wl%d.%d: %s: no TIM\n",
			          cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		}
		else if (tim_ie->len < DOT11_MNG_TIM_FIXED_LEN) {
			WL_PRINT(("wl%d.%d: %s: short TIM %d\n",
			          cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			          tim_ie->len));
		}
		else {
			WL_PRINT(("wl%d.%d: %s: bad DTIM count %d\n",
			          cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			          tim_ie->data[DOT11_MNG_TIM_DTIM_COUNT]));
		}
#endif // endif
		wlc_set_pmoverride(cfg, TRUE);
		return FALSE;
	}

	if (cfg->pm->PM_override) {
#ifdef BCMDBG
		WL_PRINT(("wl%d.%d: %s: good TIM is restored\n",
			cfg->wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
			__FUNCTION__));
#endif // endif
		wlc_set_pmoverride(cfg, FALSE);
	}
	if (cfg->dtim_programmed == 0) {
		wlc_adopt_dtim_period(cfg, tim_ie->data[DOT11_MNG_TIM_DTIM_PERIOD]);
	}

#if defined(DEBUG_TIM)
	if (WL_INFORM_ON()) {
		prhex("BCN_INFO: TIM", tim_ie, TLV_HDR_LEN + tim_ie->len);
	}
#endif /* defined(DEBUG_TIM) */

	/* extract bitmap offset (N1) from bitmap control field */
	pvboff = tim_ie->data[DOT11_MNG_TIM_BITMAP_CTL] & 0xfe;

	/* compute bitmap length (N2 - N1) from info element length */
	pvblen = tim_ie->len - DOT11_MNG_TIM_FIXED_LEN;

	/* bit[0] is used to indicate BCMC */
	cfg->pm->tim_bits_in_last_bcn = (tim_ie->data[DOT11_MNG_TIM_BITMAP_CTL] & 0x1) +
		(bcm_bitcount(&tim_ie->data[DOT11_MNG_TIM_PVB], pvblen) << 1);

	/* bail early if our AID precedes the TIM */
	AIDbyte = (cfg->AID & DOT11_AID_MASK) >> 3;
	if (AIDbyte < pvboff || AIDbyte >= pvboff + pvblen)
		return FALSE;

	/* check our AID in bitmap */
	return (tim_ie->data[DOT11_MNG_TIM_PVB + AIDbyte - pvboff] &
	        (1 << (cfg->AID & 0x7))) ? TRUE : FALSE;
}

/*
 * tell the AP that STA is ready to
 * receive traffic if it is there.
 */
void
wlc_bcn_tim_ie_pm2_action(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_RTDC(wlc, "wlc_proc_bcn: inTIM PMep=%02u AW=%02u xPS",
		(cfg->pm->PMenabled ? 10 : 0) | cfg->pm->PMpending,
		(PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
	wlc_set_pmstate(cfg, FALSE);

	/* Reset PM2 Rx Pkts Count */
	cfg->pm->pm2_rx_pkts_since_bcn = 0;

	/* use pm2_bcn_sleep_ret_time instead
	 * of pm2_sleep_ret_time if configured
	 */
	cfg->pm->pm2_sleep_ret_threshold = cfg->pm->pm2_bcn_sleep_ret_time ?
		cfg->pm->pm2_bcn_sleep_ret_time : cfg->pm->pm2_sleep_ret_time;

#ifdef WL_PWRSTATS
	if (PWRSTATS_ENAB(wlc->pub))
		wlc_pwrstats_set_frts_data(wlc->pwrstats, TRUE);
#endif /* WL_PWRSTATS */

	wlc_pm2_sleep_ret_timer_start(cfg, 0);
	/* Start the receive throttle timer to limit how
	 * long we can receive data before returning to
	 * PS mode.
	 */
	if (PM2_RCV_DUR_ENAB(cfg)) {
		wlc_pm2_rcv_timer_start(cfg);
	}
}

/* TIM */
static int
wlc_bcn_parse_tim_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_pm_st_t *pm = cfg->pm;
	wlc_wme_t *wme = cfg->wme;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	struct scb *scb = ftpparm->bcn.scb;
	bool intim;

	if (!BSSCFG_STA(cfg))
		return BCME_OK;

	if (BSSCFG_IBSS(cfg))
		return BCME_OK;

	if (pm->PMblocked)
		return BCME_OK;

	intim = wlc_InTIM(cfg, (bcm_tlv_t *)data->ie, data->ie_len);

	/* Check the TIM to see if data is buffered for us...
	 * else if PSpoll was sent, then cancel it
	 */
	if (intim) {
		/* Fast PM mode: leave PS mode */
		if (pm->PM == PM_FAST) {
			wlc_bcn_tim_ie_pm2_action(wlc, cfg);
		}
		/* WMM/APSD 3.6.2.3: Don't send PS poll if all ACs are
		 * delivery-enabled.
		 * wlc->PMenabled? Probably not due to 1->0 transition
		 */
		else if (!WME_ENAB(wlc->pub) || !wme->wme_apsd ||
		         (scb != NULL && scb->apsd.ac_delv != AC_BITMAP_ALL)) {
			if (pm->PMenabled) {
				if (wlc_sendpspoll(wlc, cfg) == FALSE) {
					WL_ERROR(("wl%d: %s: sendpspoll() failed\n",
					          wlc->pub->unit, __FUNCTION__));
				}
			} else {
				/* we do not have PS enabled but AP thinks
				 * we do (it has the tim bit set),
				 * so send a null frame to bring in sync
				 */
				wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0, PKTPRIO_NON_QOS_HIGH,
					NULL, NULL);
				WL_PS(("wl%d.%d: unexpected tim bit set, send null frame\n",
				       wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
			}
		}
		/* Send APSD trigger frames to get the buffered
		 * packets
		 */
		else if (wme->apsd_auto_trigger) {

			/* clear the scb flag to force transmit a trigger frame */
			if (pm->PMenabled) {
				if (scb != NULL)
					scb->flags &= ~SCB_SENT_APSD_TRIG;
				wlc_apsd_trigger_timeout(cfg);
			}

			/* force core awake if above trigger frame succeeded */
			wlc_set_ps_ctrl(cfg);
		}
	} else {
		/* If periodic ps-poll is enabled, ignore beacon with no TIM
		 * bit set since the pspoll and beacon may cross paths and the
		 * AP will attempt to send a data frame after we go to sleep.
		 */
		if (pm->PSpoll && pm->pspoll_prd == 0) {
			WL_PS(("wl%d.%d: PS-Poll timeout...go to sleep (%d)\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
			       STAY_AWAKE(wlc)));
			wlc_set_pspoll(cfg, FALSE);
		}

		if (PM2_RCV_DUR_ENAB(cfg) && pm->PM == PM_FAST &&
		    pm->pm2_rcv_state == PM2RD_WAIT_BCN) {
			WL_RTDC(wlc, "wlc_proc_bcn: !inTIM WBCN PMep=%02u AW=%02u",
			        (pm->PMenabled ? 10 : 0) | pm->PMpending,
			        (PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
			wlc_pm2_rcv_reset(cfg);
		}
	}

#ifdef WLWNM
	if (WLWNM_ENAB(wlc->pub) && pm->PM == PM_FAST) {
		(void)wlc_wnm_update_sleep_mode(cfg, intim);
	}
#endif /* WLWNM */

	/* Forcing quick return to sleep if no data buffered in AP */
	if (SCAN_IN_PROGRESS(wlc->scan) &&
	    (pm->PM == PM_FAST) &&
	    wlc_roam_scan_islazy(wlc, cfg, TRUE)) {
		uint bi = cfg->current_bss->beacon_period;
		pm->pm2_sleep_ret_threshold = MAX(
			(intim ? pm->pm2_sleep_ret_threshold : 0), (bi / 8));
		wlc_pm2_sleep_ret_timer_start(cfg, 0);

		/* Extend lazy scan home time beyond next beacon in case of long PM sleep return.
		 * The lazy scan will be resumed upon PM sleep return (by triggering scan timer
		 * timeout immediately).
		 */
		wlc_scan_timer_update(wlc->scan, 2 * bi);
	}

	return BCME_OK;
}
#endif /* STA */

int
wlc_update_pm_history(bool state, void *caller)
{
#ifdef WL_PWRSTATS
	if (state) { /* PM1 */
		WL_PWRSTATS_INFO_STR_0_ARGS("sleep\n");
	} else { /* PM0 */
		WL_PWRSTATS_INFO_1_ARGS("wake from %p\n", OSL_OBFUSCATE_BUF((uint32)caller));
	}
#endif /* WL_PWRSTATS */
	return BCME_OK;
}

/* ******** WORK IN PROGRESS ******** */

/* module info */
struct wlc_pm_info {
	wlc_info_t *wlc;
	uint16 pm_bcmc_wait;    /**< wait time in usecs for bcmc traffic */
	bool pm_bcmc_wait_force_zero;	/**< force BCMC timeout to zero */
	uint16 pm_bcmc_moredata_wait;	/**< wait time in usecs for bcmc traffic with MoreData=0 */
	int cfgh;
	bcm_notif_h pspoll_notif_hdl; /**< recv data notifher handle for PS-POLL */
};

#ifdef STA
/* iovar table */
enum wlc_pm_iov {
	IOV_PM2_RCV_DUR = 1,	/* PM=2 burst receive duration limit, in ms */
	IOV_SEND_NULLDATA = 2,	/* Used to tx a null frame to the given mac addr */
	IOV_PSPOLL_PRD = 3,	/* PS poll interval in milliseconds */
	IOV_BCN_LI_BCN = 4,	/* Beacon listen interval in # of beacons */
	IOV_BCN_LI_DTIM = 5,	/* Beacon listen interval in # of DTIMs */
	IOV_ADV_PS_POLL = 6,
	IOV_PM2_SLEEP_RET = 7,	/* PM=2 inactivity return to PS mode time, in ms */
	IOV_PM2_SLEEP_RET_EXT = 8,	/* Extended PM=2 inactivity return to sleep */
	IOV_PM2_REFRESH_BADIV = 9,	/* Refresh PM2 timeout with bad iv frames */
	IOV_PM_DUR = 10,	/* Retrieve accumulated PM duration and reset accumulator */
	IOV_PM2_RADIO_SHUTOFF_DLY = 11,	/* PM2 Radio Shutoff delay after NULL PM=1 */
	IOV_EXCESS_PM_PERIOD = 12,
	IOV_EXCESS_PM_PERCENT = 13,
	IOV_PS_RESEND_MODE = 14,
	IOV_SLEEP_BETWEEN_PS_RESEND = 15,
	IOV_PM2_BCN_SLEEP_RET = 16,
	IOV_PM2_MD_SLEEP_EXT = 17,
	IOV_PFN_ROAM_ALERT_THRESH = 18,
	IOV_CONST_AWAKE_THRESH = 19,
	IOV_PM_BCMC_WAIT = 20,
	IOV_PM_BCMC_MOREDATA_WAIT = 21,
	IOV_TIMIE = 22,
	IOV_LAST
};

static const bcm_iovar_t pm_iovars[] = {
#if defined(WL_PM2_RCV_DUR_LIMIT)
	{"pm2_rcv_dur", IOV_PM2_RCV_DUR, 0, 0, IOVT_UINT16, 0},
#endif /* WL_PM2_RCV_DUR_LIMIT */
	{"send_nulldata", IOV_SEND_NULLDATA,
	IOVF_SET_UP | IOVF_BSSCFG_STA_ONLY, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"pspoll_prd", IOV_PSPOLL_PRD, IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT16, 0},
	{"bcn_li_bcn", IOV_BCN_LI_BCN, 0, 0, IOVT_UINT8, 0},
	{"bcn_li_dtim", IOV_BCN_LI_DTIM, 0, 0, IOVT_UINT8, 0},
#if defined(ADV_PS_POLL)
	{"adv_ps_poll", IOV_ADV_PS_POLL, IOVF_BSSCFG_STA_ONLY, 0, IOVT_BOOL, 0},
#endif // endif
	{"pm2_sleep_ret", IOV_PM2_SLEEP_RET, 0, 0, IOVT_INT16, 0},
	{"pm2_sleep_ret_ext", IOV_PM2_SLEEP_RET_EXT,
	0, 0, IOVT_BUFFER, sizeof(wl_pm2_sleep_ret_ext_t)},
	{"pm2_refresh_badiv", IOV_PM2_REFRESH_BADIV, IOVF_BSSCFG_STA_ONLY, 0, IOVT_INT16, 0},
	{"pm_dur", IOV_PM_DUR, 0, 0, IOVT_UINT32, 0},
	{"pm2_radio_shutoff_dly", IOV_PM2_RADIO_SHUTOFF_DLY, 0, 0, IOVT_UINT16, 0},
#ifdef WL_EXCESS_PMWAKE
	{"excess_pm_period", IOV_EXCESS_PM_PERIOD, IOVF_WHL, 0, IOVT_INT16, 0},
	{"excess_pm_percent", IOV_EXCESS_PM_PERCENT, IOVF_WHL, 0, IOVT_INT16, 0},
#endif // endif
	{"ps_resend_mode", IOV_PS_RESEND_MODE, 0, 0,  IOVT_UINT8, 0},
	{"sleep_between_ps_resend", IOV_SLEEP_BETWEEN_PS_RESEND, 0, 0,  IOVT_UINT8, 0},
	{"pm2_bcn_sleep_ret", IOV_PM2_BCN_SLEEP_RET, 0, 0, IOVT_INT16, 0},
	{"pm2_md_sleep_ext", IOV_PM2_MD_SLEEP_EXT, IOVF_WHL, 0, IOVT_INT8, 0},
#ifdef WL_EXCESS_PMWAKE
	{"pfn_roam_alert_thresh", IOV_PFN_ROAM_ALERT_THRESH, 0, 0, IOVT_BUFFER, 8},
	{"const_awake_thresh", IOV_CONST_AWAKE_THRESH, 0, 0, IOVT_UINT32, 0},
#endif // endif
	{"pm_bcmc_wait", IOV_PM_BCMC_WAIT,
	0, 0, IOVT_UINT16, 0
	},
	{"pm_bcmc_moredata_wait", IOV_PM_BCMC_MOREDATA_WAIT,
	0, 0, IOVT_UINT16, 0
	},
	{"timie", IOV_TIMIE,
	(0), 0, IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */

#include <wlc_patch.h>

/* iovar dispatcher */
static int
wlc_pm_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint a_len, uint val_size, struct wlc_if *wlcif)
{
	wlc_pm_info_t *pmi = (wlc_pm_info_t *)ctx;
	wlc_info_t *wlc = pmi->wlc;
	int err = BCME_OK;
	bool bool_val;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	wlc_bsscfg_t *bsscfg;
	wlc_pm_st_t *pm;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* bool conversion to avoid duplication below */
	bool_val = (int_val != 0);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	pm = bsscfg->pm;

	/* Do the actual parameter implementation */
	switch (actionid) {

#if defined(WL_PM2_RCV_DUR_LIMIT)
	case IOV_GVAL(IOV_PM2_RCV_DUR):
		*ret_int_ptr = pm->pm2_rcv_percent;
		break;

	case IOV_SVAL(IOV_PM2_RCV_DUR):
		if (int_val == 0) {
			/* Setting pm2_rcv_dur = 0 disables the Receive Throttle feature */
			pm->pm2_rcv_percent = 0;
			wlc_pm2_rcv_reset(bsscfg);
			break;
		}
		if (int_val >= WLC_PM2_RCV_DUR_MIN &&
		    int_val <= WLC_PM2_RCV_DUR_MAX) {
			pm->pm2_rcv_percent = (uint16)int_val;
			pm->pm2_rcv_time =
			        bsscfg->current_bss->beacon_period * pm->pm2_rcv_percent/100;
		} else {
			err = BCME_RANGE;
		}
		break;
#endif /* WL_PM2_RCV_DUR_LIMIT */

	case IOV_SVAL(IOV_SEND_NULLDATA): {
#if defined(BCMDBG) || defined(BCMDBG_ERR)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

		if (!bsscfg->BSS) {
			WL_ERROR(("wl%d.%d: not an Infra STA\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
			err = BCME_ERROR;
			break;
		}
		if (!bsscfg->associated) {
			WL_ERROR(("wl%d.%d: not associated\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
			err = BCME_NOTASSOCIATED;
			break;
		}

		/* transmit */
		if (!wlc_sendapsdtrigger(wlc, bsscfg)) {
			WL_ERROR(("wl%d: unable to send null frame to %s\n",
			          wlc->pub->unit, bcm_ether_ntoa(&bsscfg->BSSID, eabuf)));
			err = BCME_NORESOURCE;
		}
		break;
	}

	case IOV_GVAL(IOV_PSPOLL_PRD):
		*ret_int_ptr = pm->pspoll_prd;
		break;

	case IOV_SVAL(IOV_PSPOLL_PRD):
		pm->pspoll_prd = (uint16)int_val;
		wlc_pspoll_timer_upd(bsscfg, TRUE);
		break;

	case IOV_GVAL(IOV_BCN_LI_BCN):
		*ret_int_ptr = wlc->bcn_li_bcn;
		break;

	case IOV_SVAL(IOV_BCN_LI_BCN):
		wlc->bcn_li_bcn = (uint8)int_val;
	        if (wlc->pub->up)
			wlc_bcn_li_upd(wlc);
		break;

	case IOV_GVAL(IOV_BCN_LI_DTIM):
		*ret_int_ptr = wlc->bcn_li_dtim;
		break;

	case IOV_SVAL(IOV_BCN_LI_DTIM):
		wlc->bcn_li_dtim = (uint8)int_val;
	        if (wlc->pub->up)
			wlc_bcn_li_upd(wlc);
		break;

	case IOV_GVAL(IOV_PM2_SLEEP_RET):
		*ret_int_ptr = (uint32)pm->pm2_sleep_ret_time;
		break;

	case IOV_SVAL(IOV_PM2_SLEEP_RET):
		if ((int_val < WLC_PM2_TICK_MS) || (int_val > WLC_PM2_MAX_MS)) {
			WL_ERROR(("should be in range %d-%d\n",
				WLC_PM2_TICK_MS, WLC_PM2_MAX_MS));
			err = BCME_RANGE;
			break;
		}
		pm->pm2_sleep_ret_time = (uint)int_val;
		pm->pm2_sleep_ret_threshold = pm->pm2_sleep_ret_time;
#ifdef WLC_ADPS
		if (ADPS_ENAB(wlc->pub)) {
			wlc_adps_store_pm2_sleep_ret(bsscfg, pm->pm2_sleep_ret_time);
		}
#endif /* WLC_ADPS */
		break;

	case IOV_GVAL(IOV_PM2_SLEEP_RET_EXT): {
		wl_pm2_sleep_ret_ext_t *sleep_ret_ext = (wl_pm2_sleep_ret_ext_t *)arg;
		bzero(sleep_ret_ext, sizeof(wl_pm2_sleep_ret_ext_t));
		sleep_ret_ext->logic = pm->dfrts_logic;
		if (sleep_ret_ext->logic != WL_DFRTS_LOGIC_OFF) {
			sleep_ret_ext->low_ms = (uint16) pm->pm2_sleep_ret_time;
			sleep_ret_ext->high_ms = pm->dfrts_high_ms;
			sleep_ret_ext->rx_pkts_threshold = pm->dfrts_rx_pkts_threshold;
			sleep_ret_ext->tx_pkts_threshold = pm->dfrts_tx_pkts_threshold;
			sleep_ret_ext->txrx_pkts_threshold = pm->dfrts_txrx_pkts_threshold;
			sleep_ret_ext->rx_bytes_threshold = pm->dfrts_rx_bytes_threshold;
			sleep_ret_ext->tx_bytes_threshold = pm->dfrts_tx_bytes_threshold;
			sleep_ret_ext->txrx_bytes_threshold = pm->dfrts_txrx_bytes_threshold;
		}
		break;
	}
	case IOV_SVAL(IOV_PM2_SLEEP_RET_EXT): {
		wl_pm2_sleep_ret_ext_t *sleep_ret_ext = (wl_pm2_sleep_ret_ext_t *)arg;
#ifdef WLC_ADPS
		if (ADPS_ENAB(wlc->pub)) {
			if (wlc_adps_enabled(bsscfg)) {
				err = BCME_EPERM;
				break;
			}
		}
#endif	/* WLC_ADPS */
		pm->dfrts_logic = (uint8)sleep_ret_ext->logic;
		if (sleep_ret_ext->logic == WL_DFRTS_LOGIC_OFF) {
			/* Disable this feature */
			pm->pm2_sleep_ret_time = PM2_SLEEP_RET_MS_DEFAULT;
		} else {
			pm->pm2_sleep_ret_time = (uint)sleep_ret_ext->low_ms;
			pm->dfrts_high_ms = (uint16)sleep_ret_ext->high_ms;
			pm->dfrts_rx_pkts_threshold = (uint16)sleep_ret_ext->rx_pkts_threshold;
			pm->dfrts_tx_pkts_threshold = (uint16)sleep_ret_ext->tx_pkts_threshold;
			pm->dfrts_txrx_pkts_threshold = (uint16)sleep_ret_ext->txrx_pkts_threshold;
			pm->dfrts_rx_bytes_threshold = (uint32)sleep_ret_ext->rx_bytes_threshold;
			pm->dfrts_tx_bytes_threshold = (uint32)sleep_ret_ext->tx_bytes_threshold;
			pm->dfrts_txrx_bytes_threshold =
				(uint32)sleep_ret_ext->txrx_bytes_threshold;
		}
		wlc_dfrts_reset_counters(bsscfg);
		break;
	}
	case IOV_GVAL(IOV_PM2_REFRESH_BADIV):
		*ret_int_ptr = pm->pm2_refresh_badiv;
		break;

	case IOV_SVAL(IOV_PM2_REFRESH_BADIV):
		pm->pm2_refresh_badiv = bool_val;
		break;

#if defined(STA) && defined(ADV_PS_POLL)
	case IOV_GVAL(IOV_ADV_PS_POLL):
		*ret_int_ptr = (int32)pm->adv_ps_poll;
		break;

	case IOV_SVAL(IOV_ADV_PS_POLL):
		pm->adv_ps_poll = bool_val;
		break;
#endif // endif

	case IOV_GVAL(IOV_PM2_RADIO_SHUTOFF_DLY):
		*ret_int_ptr = wlc->pm2_radio_shutoff_dly;
		break;

	case IOV_SVAL(IOV_PM2_RADIO_SHUTOFF_DLY): {
		uint16 dly = (uint16) int_val;

		if (dly > 10) {
			err = BCME_BADARG;
			break;
		}
		wlc->pm2_radio_shutoff_dly = dly;
		break;
	}

	case IOV_GVAL(IOV_PM_DUR):
		*ret_int_ptr = wlc_get_accum_pmdur(wlc);
		break;

#ifdef WL_EXCESS_PMWAKE
	case IOV_GVAL(IOV_EXCESS_PM_PERIOD):
		*ret_int_ptr = (int32)wlc->excess_pm_period;
		break;

	case IOV_SVAL(IOV_EXCESS_PM_PERIOD):
		wlc->excess_pm_period = (int16)int_val;
		wlc_reset_epm_dur(wlc);
		break;

	case IOV_GVAL(IOV_EXCESS_PM_PERCENT):
		*ret_int_ptr = (int32)wlc->excess_pm_percent;
		break;

	case IOV_SVAL(IOV_EXCESS_PM_PERCENT):
		if (int_val > 100) {
			err = BCME_RANGE;
			break;
		}
		wlc->excess_pm_percent = (int16)int_val;
		break;
#endif /* WL_EXCESS_PMWAKE */

	case IOV_GVAL(IOV_PS_RESEND_MODE):
		*ret_int_ptr = (uint8) pm->ps_resend_mode;
		break;
	case IOV_SVAL(IOV_PS_RESEND_MODE):
		if (int_val >= PS_RESEND_MODE_MAX)
			err = BCME_RANGE;
		else
			pm->ps_resend_mode = (uint8) int_val;
		break;

	/* IOV_SLEEP_BETWEEN_PS_RESEND is an Olympic aliased subset of IOV_PS_RESEND_MODE */
	case IOV_GVAL(IOV_SLEEP_BETWEEN_PS_RESEND):
		if (pm->ps_resend_mode == PS_RESEND_MODE_BCN_NO_SLEEP)
			*ret_int_ptr = 0;
		else if (pm->ps_resend_mode == PS_RESEND_MODE_BCN_SLEEP)
			*ret_int_ptr = 1;
		else if (pm->ps_resend_mode == PS_RESEND_MODE_WDOG_ONLY)
			*ret_int_ptr = 2;
		else
			*ret_int_ptr = pm->ps_resend_mode;
		break;
	case IOV_SVAL(IOV_SLEEP_BETWEEN_PS_RESEND):
		if (int_val == 0)
			pm->ps_resend_mode = PS_RESEND_MODE_BCN_NO_SLEEP;
		else if (int_val == 1)
			pm->ps_resend_mode = PS_RESEND_MODE_BCN_SLEEP;
		else
			/* To set other values, use IOV_PS_RESEND_MODE instead */
			err = BCME_RANGE;
		break;

	case IOV_GVAL(IOV_PM2_BCN_SLEEP_RET): {
		*ret_int_ptr = pm->pm2_bcn_sleep_ret_time;
		break;
	}
	case IOV_SVAL(IOV_PM2_BCN_SLEEP_RET): {
		pm->pm2_bcn_sleep_ret_time = (uint)int_val;
		break;
	}

	case IOV_GVAL(IOV_PM2_MD_SLEEP_EXT): {
		*ret_int_ptr = (int32)pm->pm2_md_sleep_ext;
		break;
	}
	case IOV_SVAL(IOV_PM2_MD_SLEEP_EXT): {
		pm->pm2_md_sleep_ext = (int8)int_val;
		break;
	}

#ifdef WL_EXCESS_PMWAKE
	case IOV_GVAL(IOV_PFN_ROAM_ALERT_THRESH):
	{
		wl_pfn_roam_thresh_t *thresh = arg;
		if ((uint)a_len < sizeof(wl_pfn_roam_thresh_t)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		thresh->pfn_alert_thresh = wlc->excess_pmwake->pfn_alert_thresh;
		thresh->roam_alert_thresh = wlc->excess_pmwake->roam_alert_thresh;
		break;
	}
	case IOV_SVAL(IOV_PFN_ROAM_ALERT_THRESH):
	{
		wl_pfn_roam_thresh_t *thresh = (wl_pfn_roam_thresh_t *)params;

		if (WLPFN_ENAB(wlc->pub) || thresh->pfn_alert_thresh == 0) {
			wlc->excess_pmwake->pfn_alert_thresh = thresh->pfn_alert_thresh;
			wlc->excess_pmwake->pfn_alert_enable = !!thresh->pfn_alert_thresh;
			wlc->excess_pmwake->pfn_alert_thresh_ts = wlc_get_pfn_ms(wlc);
		} else {
			return BCME_ERROR;
		}
		wlc->excess_pmwake->roam_alert_thresh = thresh->roam_alert_thresh;
		wlc->excess_pmwake->roam_alert_enable = !!thresh->roam_alert_thresh;
		wlc->excess_pmwake->roam_alert_thresh_ts = wlc_get_roam_ms(wlc);
		break;
	}
	case IOV_GVAL(IOV_CONST_AWAKE_THRESH):
		*ret_int_ptr = wlc->excess_pmwake->ca_thresh;
		break;
	case IOV_SVAL(IOV_CONST_AWAKE_THRESH):
	{
		wlc_excess_pm_wake_info_t *epm = wlc->excess_pmwake;
		epm->ca_thresh = int_val;
		epm->ca_pkts_allowance = epm->ca_thresh;
		epm->ca_alert_pmdur = 0;
		epm->ca_alert_pmdur_valid = 0;
		wlc_reset_epm_ca(wlc);
	}
	break;
#endif /* WL_EXCESS_PMWAKE */

	case IOV_SVAL(IOV_PM_BCMC_WAIT):
		if (D11REV_LT(wlc->pub->corerev, 40))
			return BCME_UNSUPPORTED;
		pmi->pm_bcmc_wait = (uint16)int_val;

		/* If the core is not up, the shmem will be updated in wlc_up */
		if (wlc->pub->hw_up) {
			wlc_write_shm(wlc, M_BCMC_TIMEOUT(wlc), (uint16)int_val);
		}
		break;
	case IOV_GVAL(IOV_PM_BCMC_WAIT):
		if (D11REV_LT(wlc->pub->corerev, 40))
			return BCME_UNSUPPORTED;
		*ret_int_ptr = pmi->pm_bcmc_wait;
		break;

	case IOV_SVAL(IOV_PM_BCMC_MOREDATA_WAIT):
	{
		if (D11REV_LT(wlc->pub->corerev, 40)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (int_val >= 0x8000 && int_val != 0xFFFF) {
			err = BCME_BADARG;
			break;
		}

		pmi->pm_bcmc_moredata_wait = (uint16)int_val;

		/* If the core is not up, the shmem will be updated in wlc_up */
		if (si_iscoreup(wlc->pub->sih)) {
			wlc_write_shm(wlc, M_BCMCROLL_TMOUT(wlc), (uint16)int_val);
		}
		break;
	}

	case IOV_GVAL(IOV_PM_BCMC_MOREDATA_WAIT):
	{
		if (D11REV_LT(wlc->pub->corerev, 40)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = pmi->pm_bcmc_moredata_wait;
		break;
	}

	case IOV_GVAL(IOV_TIMIE):
		if (!bsscfg->associated) {
			err = BCME_NOTASSOCIATED;
			break;
		}
		*ret_int_ptr = pm->tim_bits_in_last_bcn;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_pm_doiovar */

/* ioctl dispatcher */
static int
wlc_pm_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_pm_info_t *pmi = ctx;
	wlc_info_t *wlc = pmi->wlc;
	wlc_bsscfg_t *bsscfg;
	int val = 0, *pval;
	int bcmerror = BCME_OK;
#ifdef STA
	wlc_pm_st_t *pm;
#endif // endif

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

#ifdef STA
	pm = bsscfg->pm;
#endif // endif

	pval = (int *)arg;

	/* This will prevent the misaligned access */
	if ((uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));

	switch (cmd) {

	case WLC_GET_PM:
		*pval = pm->PM;
		break;

	case WLC_SET_PM:
#ifdef BCM_DISABLE_PM
		break;
#endif /* BCM_DISABLE_PM */
		/* save host requested pmstate */
		if (BSSCFG_IS_PRIMARY(bsscfg))
			wlc_btc_save_host_requested_pm(wlc, (uint8)val);
		if (val == PM_FORCE_OFF) {
			val = PM_OFF;
		}
#ifdef BTCX_PM0_IDLE_WAR
		else {
			if (wlc_btc_get_bth_active(wlc) && (PM_OFF == val))
				val = PM_FAST;
		}
#endif	/* BTCX_PM0_IDLE_WAR */
		bcmerror = wlc_set_pm_mode(wlc, val, bsscfg);
#ifdef WLC_ADPS
		if (ADPS_ENAB(wlc->pub) && (bcmerror == BCME_OK)) {
			wlc_adps_store_PM(bsscfg, val);
		}
#endif /* WLC_ADPS */

		break;

#ifdef BCMDBG
	case WLC_GET_WAKE:
		*pval = wlc->wake;
		break;

	case WLC_SET_WAKE:
		WL_PS(("wl%d: setting WAKE to %d\n", WLCWLUNIT(wlc), val));
		wlc->wake = val ? TRUE : FALSE;

		/* apply to the mac */
		wlc_set_wake_ctrl(wlc);
		break;
#endif /* BCMDBG */

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return bcmerror;
}

static int
wlc_pm_wlc_up(void *ctx)
{
	wlc_pm_info_t *pmi = ctx;
	wlc_info_t *wlc = pmi->wlc;

	if (D11REV_GE(wlc->pub->corerev, 40)) {
		wlc_write_shm(wlc, M_BCMC_TIMEOUT(wlc), (uint16)pmi->pm_bcmc_wait);
		wlc_write_shm(wlc, M_BCMCROLL_TMOUT(wlc), (uint16)pmi->pm_bcmc_moredata_wait);
	}

	return BCME_OK;
}

#endif /* STA */

/* bsscfg cubby */
static int
wlc_pm_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_pm_info_t *pmi = (wlc_pm_info_t *)ctx;
	wlc_info_t *wlc = pmi->wlc;
	int err;
#ifdef STA
	wlc_pm_st_t *pm;
#endif // endif

	/* TODO: by design only Infra. STA uses pm states so
	 * need to check !BSSCFG_STA(cfg) before proceed.
	 * need to make sure nothing other than Infra. STA
	 * accesses pm structures though.
	 */

	if ((cfg->pm = (wlc_pm_st_t *)MALLOCZ(wlc->osh, sizeof(*(cfg->pm)))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}

#ifdef STA
	/* TODO: move the check all the way up to before the malloc */
	if (!BSSCFG_STA(cfg)) {
		err = BCME_OK;
		goto fail;
	}

	pm = cfg->pm;

	/* create apsd trigger timer */
	if ((pm->apsd_trigger_timer =
	     wl_init_timer(wlc->wl, wlc_apsd_trigger_timeout, cfg, "apsd_trigger")) == NULL) {
		WL_ERROR(("wl%d: bsscfg %d wlc_apsd_trigger_timeout failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* create pspoll timer */
	if ((pm->pspoll_timer =
	     wl_init_timer(wlc->wl, wlc_pspoll_timer, cfg, "pspoll")) == NULL) {
		WL_ERROR(("wl%d: bsscfg %d pspoll_timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* allocate pm2_ret_timer object */
	if ((pm->pm2_ret_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
		WL_ERROR(("wl%d.%d: %s: failed to alloc PM2 timeout\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* allocate pm2_rcv_timer object */
	if ((pm->pm2_rcv_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
		WL_ERROR(("wl%d.%d: %s: failed to alloc PM2 timeout\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		err = BCME_NORESOURCE;
		goto fail;
	}

	/* Set the default PM2 return to sleep time */
	pm->pm2_sleep_ret_time = PM2_SLEEP_RET_MS_DEFAULT;
	pm->pm2_sleep_ret_threshold = pm->pm2_sleep_ret_time;
#endif /* STA */

	return BCME_OK;

fail:
	return err;
}

static void
wlc_pm_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_pm_info_t *pmi = (wlc_pm_info_t *)ctx;
	wlc_info_t *wlc = pmi->wlc;

	if (cfg->pm != NULL) {
		wlc_pm_st_t *pm = cfg->pm;

#ifdef STA
		if (pm->apsd_trigger_timer) {
			wl_free_timer(wlc->wl, pm->apsd_trigger_timer);
		}

		if (pm->pspoll_timer) {
			wl_free_timer(wlc->wl, pm->pspoll_timer);
		}

		/* free the pm2_rcv_timer object */
		if (pm->pm2_rcv_timer != NULL) {
			wlc_hrt_free_timeout(pm->pm2_rcv_timer);
		}

		/* free the pm2_rcv_timeout object */
		if (pm->pm2_ret_timer != NULL) {
			wlc_hrt_del_timeout(pm->pm2_ret_timer);
			wlc_hrt_free_timeout(pm->pm2_ret_timer);
		}
#endif /* STA */

		MFREE(wlc->osh, pm, sizeof(*pm));
		cfg->pm = NULL;
	}
}

#if defined(BCMDBG)
static void
wlc_pm_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "WME_PM_blocked %d\n", cfg->pm->WME_PM_blocked);
	bcm_bprintf(b, "PM mode %d PMenabled %d PM_override %d PSpoll %d\n",
	            cfg->pm->PM, cfg->pm->PMenabled, cfg->pm->PM_override, cfg->pm->PSpoll);
	bcm_bprintf(b, "PMpending %d priorPMstate %d\n",
	            cfg->pm->PMpending, cfg->pm->priorPMstate);
}
#endif // endif

#ifdef STA
/* handle bsscfg state change */
static void
wlc_pm_bss_state_upd(void *ctx, bsscfg_state_upd_data_t *evt)
{
	wlc_pm_info_t *pmi = (wlc_pm_info_t *)ctx;
	wlc_info_t *wlc = pmi->wlc;
	wlc_bsscfg_t *cfg = evt->cfg;

	if (evt->old_up && !cfg->up) {
		int callbacks = 0;

		if (BSSCFG_STA(cfg) && cfg->pm != NULL) {
			/* cancel any apsd trigger timer */
			if (!wl_del_timer(wlc->wl, cfg->pm->apsd_trigger_timer)) {
				callbacks++;
			}
			/* cancel any pspoll timer */
			if (!wl_del_timer(wlc->wl, cfg->pm->pspoll_timer)) {
				callbacks ++;
			}
		}

		/* return to the caller */
		evt->callbacks_pending = callbacks;
	}
	/* stopped the PM2 timer when the device is down */
	if (!cfg->up && BSSCFG_STA(cfg) && cfg->pm->PM == PM_FAST) {
		wlc_pm2_sleep_ret_timer_stop(cfg);
	}
}
#endif /* STA */

#if defined(BCMDBG) || defined(ENABLE_DUMP_PM)
static int
wlc_dump_pm(wlc_info_t *wlc, struct bcmstrbuf *b)
{
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
	bcm_bprintf(b, "STAY_AWAKE() %d "
	            "SCAN_IN_PROGRESS() %d WLC_RM_IN_PROGRESS() %d AS_IN_PROGRESS() %d "
	            "wlc->wake %d wlc->PMpending %d wlc->PSpoll %d wlc->apsd_sta_usp %d\n",
	            STAY_AWAKE(wlc),
	            SCAN_IN_PROGRESS(wlc->scan), WLC_RM_IN_PROGRESS(wlc), AS_IN_PROGRESS(wlc),
	            wlc->wake, wlc->PMpending, wlc->PSpoll, wlc->apsd_sta_usp);
	bcm_bprintf(b, "wlc->PMawakebcn %d "
	            "wlc->check_for_unaligned_tbtt %d\n",
	            wlc->PMawakebcn,
	            wlc->check_for_unaligned_tbtt);
	bcm_bprintf(b, "wlc->gptimer_stay_awake_req %d wlc->pm2_radio_shutoff_pending %d"
				"wlc->user_wake_req %d \n",
				wlc->gptimer_stay_awake_req, wlc->pm2_radio_shutoff_pending,
				wlc->user_wake_req);
#ifdef WL11K
	bcm_bprintf(b, "wlc_rrm_inprog %d\n", wlc_rrm_inprog(wlc));
#endif // endif
	FOREACH_BSS(wlc, idx, cfg) {
		wlc_pm_st_t *pm = cfg->pm;
		if (BSSCFG_STA(cfg) && (cfg->associated || BSS_TDLS_ENAB(wlc, cfg))) {
			bcm_bprintf(b, "bsscfg %d BSS %d PS_ALLOWED() %d WLC_PORTOPEN() %d "
		            "dtim_programmed %d PMpending %d priorPMstate %d PMawakebcn %d "
		            "WME_PM_blocked %d PM %d PMenabled %d PSpoll %d apsd_sta_usp %d "
		            "check_for_unaligned_tbtt %d PMblocked 0x%x\n",
		            WLC_BSSCFG_IDX(cfg), cfg->BSS, PS_ALLOWED(cfg), WLC_PORTOPEN(cfg),
		            cfg->dtim_programmed, pm->PMpending, pm->priorPMstate, pm->PMawakebcn,
		            pm->WME_PM_blocked, pm->PM, pm->PMenabled, pm->PSpoll, pm->PMpending,
		            pm->check_for_unaligned_tbtt, pm->PMblocked);
		}
	}
#endif /* STA */

	return BCME_OK;
}
#endif // endif

/* module attach/detach interfaces */
wlc_pm_info_t *
BCMATTACHFN(wlc_pm_attach)(wlc_info_t *wlc)
{
	wlc_pm_info_t *pmi;
	bsscfg_cubby_params_t cubby_params;

	/* allocate module info */
	if ((pmi = MALLOCZ(wlc->osh, sizeof(*pmi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	pmi->wlc = wlc;

	/* reserve the bsscfg cubby for any bss specific private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = pmi;
	cubby_params.fn_init = wlc_pm_bss_init;
	cubby_params.fn_deinit = wlc_pm_bss_deinit;
#if defined(BCMDBG)
	cubby_params.fn_dump = wlc_pm_bss_dump;
#endif // endif
	pmi->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, 0, &cubby_params);
	if (pmi->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve_ext failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef STA
	/* IE parse */
	if (wlc_iem_add_parse_fn(wlc->iemi, FC_BEACON, DOT11_MNG_TIM_ID,
			wlc_bcn_parse_tim_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, tim ie in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* bsscfg state change */
	if (wlc_bsscfg_state_upd_register(wlc, wlc_pm_bss_state_upd, pmi) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* ioctl dispatcher requires an unique "hdl" */
	if (wlc_module_add_ioctl_fn(wlc->pub, pmi, wlc_pm_doioctl, 0, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Create notification list for recvpackets */
	if (bcm_notif_create_list(wlc->notif, &pmi->pspoll_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s bcm_notif_create_list failed\n",
			wlc->pub->unit, __FUNCTION__));
	}

	if (wlc_module_register(wlc->pub, pm_iovars, "pm", pmi, wlc_pm_doiovar,
			NULL, wlc_pm_wlc_up, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* STA */

#if defined(BCMDBG) || defined(ENABLE_DUMP_PM)
	wlc_dump_register(wlc->pub, "pm", (dump_fn_t)wlc_dump_pm, (void *)wlc);
#endif // endif

	/* init BCMC wait time to disabled value */
	pmi->pm_bcmc_wait = 0xFFFF;

	/* init BCMC moredata wait time to disabled value */
	pmi->pm_bcmc_moredata_wait = 0xFFFF;

	return pmi;

fail:
	MODULE_DETACH(pmi, wlc_pm_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_pm_detach)(wlc_pm_info_t *pmi)
{
	wlc_info_t *wlc;

	if (pmi == NULL)
		return;

	wlc = pmi->wlc;

#ifdef STA
	(void)wlc_module_unregister(wlc->pub, "pm", pmi);
	(void)wlc_module_remove_ioctl_fn(wlc->pub, pmi);
	(void)wlc_bsscfg_state_upd_unregister(wlc, wlc_pm_bss_state_upd, pmi);
	if (pmi->pspoll_notif_hdl != NULL) {
		bcm_notif_delete_list(&pmi->pspoll_notif_hdl);
	}
#endif // endif

	MFREE(wlc->osh, pmi, sizeof(*pmi));
}

void
wlc_pm_ignore_bcmc(wlc_pm_info_t *pmi,  bool ignore_bcmc)
{
	wlc_info_t *wlc;

	if (pmi == NULL)
		return;

	wlc = pmi->wlc;

	if (ignore_bcmc) {
		pmi->pm_bcmc_wait_force_zero = TRUE;
		if (D11REV_GE(wlc->pub->corerev, 40))
			wlc_write_shm(wlc, M_BCMC_TIMEOUT(wlc), 0x0);
	} else {
		pmi->pm_bcmc_wait_force_zero = FALSE;
		if (D11REV_GE(wlc->pub->corerev, 40))
			wlc_write_shm(wlc, M_BCMC_TIMEOUT(wlc), (uint16)pmi->pm_bcmc_wait);
	}
}

#ifdef STA
/* These functions register/unregister/invoke the callback */
int
BCMATTACHFN(wlc_pm_pspoll_notif_register)(wlc_pm_info_t *pmi,
	pm_pspoll_notif_fn_t fn, void *arg)
{
	bcm_notif_h hdl = pmi->pspoll_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
BCMATTACHFN(wlc_pm_pspoll_notif_unregister) (wlc_pm_info_t *pmi,
	pm_pspoll_notif_fn_t fn, void *arg)
{
	bcm_notif_h hdl = pmi->pspoll_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

void
wlc_pm_update_rxdata(wlc_bsscfg_t *cfg, struct scb *scb, struct dot11_header *h, uint8 prio)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_st_t *pm = cfg->pm;
	wlc_pm_info_t *pmi = wlc->pm;

	if (pm->pspoll_prd && pmi->pspoll_notif_hdl) {
		pm_pspoll_notif_data_t data;

		data.is_state = TRUE;
		data.cfg = cfg;
		data.scb = scb;
		data.h = h;
		data.prio = prio;
		bcm_notif_signal(pmi->pspoll_notif_hdl, &data);
	}
}

void
wlc_pm_update_pspoll_state(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_pm_info_t *pmi = wlc->pm;

	if (pmi->pspoll_notif_hdl) {
		pm_pspoll_notif_data_t data;

		data.is_state = FALSE;
		data.state = state;
		data.scb = wlc_scbfind(wlc, cfg, &cfg->BSSID);
		bcm_notif_signal(pmi->pspoll_notif_hdl, &data);
	}
}
#endif /* STA */

/* ******** WORK IN PROGRESS ******** */
