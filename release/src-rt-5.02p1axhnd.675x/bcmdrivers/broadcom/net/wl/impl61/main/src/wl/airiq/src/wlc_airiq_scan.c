/*
 * @file
 * @brief
 *
 *  Air-IQ Scan
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
 * $Id$
 *
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wl_export.h>
#include <wlc_ap.h>
#include <wlc_scan.h>
#include <wlc_phy_hal.h>
#include <phy_radar_api.h>
#include <wlc_quiet.h>
#include <wlc_csa.h>
#include <wlc_11h.h>
#include <wlc_dfs.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_bmac.h>
#include <wlc_dump.h>
#include <wlc_hw.h>
#include <wlc_iocv.h>
#include <wlc_event_utils.h>
#include <wlc_stf.h>
#include <wlc_hw_priv.h>
#include <wl_dbg.h>
#include <phy_misc_api.h>
#include <phy_calmgr_api.h>
#include <phy_ac_info.h>
#include <wlc_modesw.h>

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
//#include <wlc_tx.h>
#include <wlc_scandb.h>
#include <wlc.h>
#include <phy_ac.h>
#include <phy_utils_reg.h>
#include <wlc_phyreg_ac.h>
#include <wlc_bmac.h>
#include <wl_export.h>
#include <wlc_scan.h>
#include <wlc_types.h>
#include <wlc_airiq.h>
#include <wlc_modesw.h>
#include <wlc_dfs.h>
#include <wlc_radioreg_20693.h>
#include <wlc_tx.h>

void
wlc_airiq_default_scan_channels(airiq_info_t *airiqh)
{
	int cnt = 0;

	airiqh->scan.chanspec_list[cnt] = 1   | WL_CHANSPEC_BW_20 | WL_CHANSPEC_BAND_2G;
	airiqh->scan.dwell_interval_ms[cnt] = 100;
	airiqh->scan.capture_interval_us[cnt] = 100;
	airiqh->scan.capture_count[cnt] = 0;
	airiqh->scan.core_config[cnt] = 0;
	cnt++;

	airiqh->scan.channel_cnt = cnt;
}

int
airiq_update_scan_config(airiq_info_t *airiqh, airiq_config_t *sc)
{
	int32 cnt;
	int err = 0, k;
	uint16 phy_mode;

	cnt = MIN(MAX_CHANSPECS, sc->chanspec_cnt); /* ensure bounds */

	if (cnt <= 0) {
		WL_ERROR(("%s: wl%d: Invalid chanspec count %d\n",
			__FUNCTION__, WLCWLUNIT(airiqh->wlc), cnt));
		return BCME_UNSUPPORTED;
	}

	if (sc->phy_mode == PHYMODE_3x3_1x1 && D11REV_LT(airiqh->wlc->pub->corerev, 65)) {
		WL_ERROR(("%s: wl%d: Requested 3+1 mode scan on core rev %d\n",
			__FUNCTION__, WLCWLUNIT(airiqh->wlc), airiqh->wlc->pub->corerev));
		return BCME_UNSUPPORTED;
	}

	memcpy(airiqh->scan.chanspec_list,
		sc->chanspec_list,
		cnt * sizeof(chanspec_t));

	memcpy(airiqh->scan.dwell_interval_ms,
		sc->dwell_interval_ms,
		cnt * sizeof(sc->dwell_interval_ms[0]));

	memcpy(airiqh->scan.capture_interval_us,
		sc->capture_interval_us,
		cnt * sizeof(sc->capture_interval_us[0]));

	memcpy(airiqh->scan.capture_count,
		sc->capture_count,
		cnt * sizeof(sc->capture_count[0]));

	memcpy(airiqh->scan.core_config,
		sc->core_config,
		cnt * sizeof(sc->core_config[0]));

	phy_mode = sc->phy_mode;

	WL_AIRIQ(("wl%d: %s: %d channels, phy_mode = %d (core=%d)\n",
		WLCWLUNIT(airiqh->wlc), __FUNCTION__, cnt, phy_mode, sc->core_config[0]));

	for (k = 0; k < cnt; k++) {
		WL_AIRIQ(("wl%d: [%4x] core %d dwell=%4dms fft intvl=%4dusec capcnt=%4d\n",
			WLCWLUNIT(airiqh->wlc),
			sc->chanspec_list[k], sc->core_config[k], sc->dwell_interval_ms[k],
			sc->capture_interval_us[k], sc->capture_count[k]));

		if ((sc->core_config[k] != 3) && (phy_mode == PHYMODE_3x3_1x1)) {
			WL_ERROR(("wl%d: %s: Core 3 reqd for 3+1 mode (core %d specified)\n",
				WLCWLUNIT(airiqh->wlc), __FUNCTION__, sc->core_config[k]));
			return BCME_UNSUPPORTED;
		}
		/*  3+1 and offloads not supported  */
	}

	airiqh->scan.channel_cnt = cnt;
	airiqh->scan.channel_idx = cnt - 1;

	if (airiqh->phy_mode == PHYMODE_3x3_1x1 && phy_mode != airiqh->phy_mode) {
		wlc_airiq_scan_abort(airiqh, TRUE);
	}
	airiqh->phy_mode = phy_mode;
	airiqh->core = sc->core_config[0];

	return err;
}

int
airiq_get_scan_config(airiq_info_t *airiqh, airiq_config_t *sc, int size)
{
	int32 cnt = airiqh->scan.channel_cnt;

	memcpy(sc->chanspec_list,
	       airiqh->scan.chanspec_list,
	       cnt * sizeof(chanspec_t));
	memcpy(sc->dwell_interval_ms,
	       airiqh->scan.dwell_interval_ms,
	       cnt * sizeof(sc->dwell_interval_ms[0]));
	memcpy(sc->capture_interval_us,
	       airiqh->scan.capture_interval_us,
	       cnt * sizeof(sc->capture_interval_us[0]));
	memcpy(sc->capture_count,
	       airiqh->scan.capture_count,
	       cnt * sizeof(sc->capture_count[0]));
	memcpy(sc->core_config,
	       airiqh->scan.core_config,
	       cnt * sizeof(sc->core_config[0]));

	sc->chanspec_cnt = airiqh->scan.channel_cnt;
	sc->start = airiqh->scan_enable;
	return 0;
}

bool
chanspec_list_valid(airiq_info_t *airiqh, airiq_config_t *sc)
{
	int i;
	if ((airiqh->phy_mode == PHYMODE_3x3_1x1) &&
		(IS_WAVE2_DB(airiqh->wlc->deviceid) || IS_WAVE2_2G(airiqh->wlc->deviceid) ||
		IS_43694MC(airiqh->wlc->deviceid) || IS_43694MC2(airiqh->wlc->deviceid))) {
		return TRUE;
	} else if ((NBANDS(airiqh->wlc) > 1)) {
		return TRUE;
	} else {
		/* For single band radio check if it supports requested band */
		for (i = 0; i < sc->chanspec_cnt; i++) {
			if (CHSPEC2WLC_BAND(sc->chanspec_list[i]) !=
				(uint)airiqh->wlc->band->bandtype) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

#ifdef VASIP_HW_SUPPORT
void wlc_airiq_scan_set_chanspec_3p1(airiq_info_t *airiqh, phy_info_t *pi,
	chanspec_t home_chanspec, chanspec_t scan_chanspec)
{
#ifdef __AIRIQ_DBG
	uint32 tsf_l=0, tsf_h=0;
	uint32 tsf2_l=0, tsf2_h=0;

	wlc_read_tsf(airiqh->wlc, &tsf_l, &tsf_h);
#endif /* __AIRIQ_DBG */
	phy_ac_chanmgr_set_val_sc_chspec(PHY_AC_CHANMGR(pi), scan_chanspec);
	phy_ac_chanmgr_set_val_phymode(PHY_AC_CHANMGR(pi), PHYMODE_3x3_1x1);
#ifdef __AIRIQ_DBG
	wlc_read_tsf(airiqh->wlc, &tsf2_l, &tsf2_h);
	SCAN_DBG("%s took (%d) %d usec -- 3x3 @ 0x%x +1 @ 0x%x\n", __FUNCTION__, tsf2_h - tsf_h,tsf2_l - tsf_l,
		home_chanspec, scan_chanspec);
#endif /* __AIRIQ_DBG */
	airiqh->upgrade_pending = TRUE;
}

/* timer function to implement channel scanning in normal 3+1 and LTE scan mode. The interface */
/* is assumed to be in the UP state. */
static void wlc_airiq_scantimer_3p1(void *arg)
{
	airiq_info_t *airiqh;
	chanspec_t chanspec;
	int chancnt;
	phy_info_t *pi;
	uint32 dummy_tsf_high, tsf;
	bool sweep_begin = FALSE;
	uint16 fft_smpl_ctrl = 0;

	airiqh = (airiq_info_t*)arg;

	if (!airiqh) {
		WL_ERROR(("%s(%d): null invalid airiq handle\n",
					__FUNCTION__, __LINE__));
		return;
	}

	chancnt = airiqh->scan.channel_cnt;

	SCAN_DBG("-->%s\n", __FUNCTION__);

	if (!airiqh->wlc->pub->up) {
		WL_AIRIQ(("wl%d: %s(%d): not up. Scan aborted/Interface down.\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__, __LINE__));
		wlc_airiq_set_enable(airiqh, FALSE);
		airiqh->scan.run_phycal = FALSE;
		airiqh->wlc->scan->state &= ~(SCAN_STATE_PROHIBIT | SCAN_STATE_PASSIVE);
		SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
		if (airiqh->sweep_count >= 0) {
			if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
				/* Send a scan completion notification (we need to add status) */
				wl_airiq_sendup_scan_complete_alternate(airiqh, AIRIQ_SCAN_ABORTED);
			} else {
				/* Send a scan abort notification (we need to add status) */
				wl_lte_u_send_scan_abort_event(airiqh, LTE_U_SCAN_ABORT_WL_DOWN);

			}
		}
		return;
	}

	if (!airiqh->scan_enable) {
		WL_AIRIQ(("wl%d %s: scan stopped.\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__));
		SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);

		if (airiqh->sweep_count >= 0) {
			if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
				/* Send a scan completion notification (we need to add status) */
				wl_airiq_sendup_scan_complete_alternate(airiqh,
					AIRIQ_SCAN_ABORTED);
			} else {
				/* Send a scan abort notification (we need to add status) */
				wl_lte_u_send_scan_abort_event(airiqh, LTE_U_SCAN_ABORT_CMD);
			}
		}
		airiqh->scan.run_phycal = FALSE;
		return;

	}

	pi = (phy_info_t*)WLC_PI(airiqh->wlc);

	/* For 11ac chips, check if the bandwidth of the 3x3 matches the scan
	   channel bandwidth, otherwise we abort. In case of 11ax the +1 is allowed to have
	   a different bandwidth compared to 3x3 and so we do not abort the scan. */
	if ((CHSPEC_BW(pi->radio_chanspec) !=
			(airiqh->scan.chanspec_list[airiqh->scan.channel_idx] &
			 WL_CHANSPEC_BW_MASK)) &&
			 D11REV_LT(airiqh->wlc->pub->corerev, 128)) {
		/* Bandwidth changed */
		WL_AIRIQ(("wl%d: %s: BW change (0x%x) chspec = 0x%x scan chspec = 0x%x\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__, CHSPEC_BW(pi->radio_chanspec),
			(airiqh->wlc->chanspec & WL_CHANSPEC_BW_MASK),
			(airiqh->scan.chanspec_list[airiqh->scan.channel_idx] &
			WL_CHANSPEC_BW_MASK)));
		wlc_airiq_set_enable(airiqh, FALSE);
		airiqh->scan.run_phycal = FALSE;
		airiqh->wlc->scan->state &= ~(SCAN_STATE_PROHIBIT | SCAN_STATE_PASSIVE);
		if (airiqh->sweep_count >= 0) {
			if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
				/* Send a scan completion notification */
				wl_airiq_sendup_scan_complete_alternate(airiqh,
					AIRIQ_SCAN_ABORTED);
			} else {
				/* Send a scan abort notification */
				wl_lte_u_send_scan_abort_event(airiqh,
					LTE_U_SCAN_ABORT_BW_CHANGED);
			}
		}
		return;
	}

	/* Age out gain hints  and issue mac event */
	if (airiqh->scan.scan_start) {
		/* first pass we do not want to age, or create a scan complete event! */
		airiqh->scan.scan_start = FALSE;
		airiqh->scan.channel_idx = 0;

		airiqh->fft_count = 0;
		airiqh->capture_limit = MAX(10,
			airiqh->scan.capture_count[airiqh->scan.channel_idx]);

		SCAN_DBG("%s: ***SCANSTART***\n",  __FUNCTION__);
		sweep_begin = TRUE;
	} else {
		wlc_read_tsf(airiqh->wlc, &tsf, &dummy_tsf_high);
		if ((airiqh->scan_type == SCAN_TYPE_LTE_U) &&
		    (airiqh->lte_interrupt_received == TRUE)) {
			wlc_lte_u_send_status(airiqh);
			airiqh->lte_interrupt_received = FALSE;
		}
		wlc_svmp_mem_read_axi(airiqh->wlc->hw, &fft_smpl_ctrl, SVMP_SMPL_CTRL_ADDR, 1);

		if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
			if (airiqh->fft_count >= airiqh->capture_limit) {
				SCAN_DBG("%s: ch[%d]:%3d fftcnt=%4d > %4d: %6d us\n",
					__FUNCTION__, airiqh->scan.channel_idx,
					CHSPEC_CHANNEL(airiqh->scan.
					chanspec_list[airiqh->scan.channel_idx]),
					airiqh->fft_count, airiqh->capture_limit,
					tsf - airiqh->chan_tsf);

				airiqh->scan.channel_idx = (airiqh->scan.channel_idx + 1) % chancnt;
			} else if (fft_smpl_ctrl == 0) {
				// FFT_SMPL_CTRL is a countdown counter. if 0, success
				// and is useful in offload case.
				SCAN_DBG("%s: ch[%d]:[%3d] fftcnt=%4d, target = %4d:%6dus. SMPL_CTRL=0\n",
					__FUNCTION__,  airiqh->scan.channel_idx,
					CHSPEC_CHANNEL(airiqh->scan.
					chanspec_list[airiqh->scan.channel_idx]),
					airiqh->fft_count, airiqh->capture_limit,
					tsf - airiqh->chan_tsf);
				airiqh->scan.channel_idx = (airiqh->scan.channel_idx + 1) % chancnt;
			} else if ((tsf - airiqh->chan_tsf) < (4000 *
				airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx]) &&
					(airiqh->fft_count < airiqh->capture_limit / 2)) {
				// max dwell is 4x the nominal value.
				SCAN_DBG("%s: ch[%d]:[%3d] fftcnt=%4d < %4d: %6d us ***WAIT***\n",
					__FUNCTION__,  airiqh->scan.channel_idx,
					CHSPEC_CHANNEL(airiqh->scan.
					chanspec_list[airiqh->scan.channel_idx]),
					airiqh->fft_count, airiqh->capture_limit,
					tsf - airiqh->chan_tsf);

				wl_add_timer(airiqh->wlc->wl, airiqh->timer, 10, 0);

				SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
				return;
			} else {
				SCAN_DBG("%s: ch[%d]:%3d fftcnt=%4d<%4d: %6d us > %6d us TIMEOUT\n",
					__FUNCTION__, airiqh->scan.channel_idx,
					CHSPEC_CHANNEL(airiqh->scan.
					chanspec_list[airiqh->scan.channel_idx]),
					airiqh->fft_count, airiqh->capture_limit,
					tsf - airiqh->chan_tsf,
					(2000 * airiqh->scan.
					dwell_interval_ms[airiqh->scan.channel_idx]));
				airiqh->scan.channel_idx = (airiqh->scan.channel_idx + 1) % chancnt;
			}
		} else {
			/* scan_type == SCAN_TYPE_LTE_U */
			sweep_begin = TRUE;
			if ((fft_smpl_ctrl == 0)) {
				/* FFT_SMPL_CTRL is a countdown counter.
				 * This is a successful result.
				 */
				SCAN_DBG("%s: [%3d] int recvd=%4d, target=%4d:%6d us (DONE)\n",
					__FUNCTION__,
					CHSPEC_CHANNEL(airiqh->scan.
					chanspec_list[airiqh->scan.channel_idx]),
					airiqh->lte_interrupt_received, airiqh->capture_limit,
					tsf - airiqh->chan_tsf);
				airiqh->scan.channel_idx = (airiqh->scan.channel_idx + 1) % chancnt;
			} else if ((tsf - airiqh->chan_tsf) <
				(1000 * airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx]) &&
				(airiqh->lte_interrupt_received  ==  FALSE)) {
				/* max dwell is the nominal value. */
				SCAN_DBG("%s:[%3d] lte_int=%4d cap_lim=%4d:%6dus (WAIT)\n",
					__FUNCTION__,
					CHSPEC_CHANNEL(airiqh->scan.
					chanspec_list[airiqh->scan.channel_idx]),
					airiqh->lte_interrupt_received, airiqh->capture_limit,
					tsf - airiqh->chan_tsf);

				wl_add_timer(airiqh->wlc->wl, airiqh->timer, 10, 0);
				SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
				return;
			} else {
				SCAN_DBG("%s:ch[%d]:%3d lte_int=%4d cap_lim=%4d:%6dus>%6d (TO)\n",
					__FUNCTION__, airiqh->scan.channel_idx,
					CHSPEC_CHANNEL(airiqh->scan.
					chanspec_list[airiqh->scan.channel_idx]),
					airiqh->lte_interrupt_received, airiqh->capture_limit,
					tsf - airiqh->chan_tsf,
					1000 * (airiqh->scan.
					dwell_interval_ms[airiqh->scan.channel_idx]));

				airiqh->scan.channel_idx = (airiqh->scan.channel_idx + 1) % chancnt;
			}
		}

		airiqh->fft_count = 0;
		airiqh->capture_limit = MAX(10, airiqh->scan.capture_count[airiqh->scan.channel_idx]);

		if ((airiqh->scan.channel_idx == 0) && !sweep_begin) {
			if (airiqh->sweep_count >= 0) {
				airiqh->sweep_count--;
				if (airiqh->sweep_count <= 0) {
					if ((airiqh->scan.home_scan) || (airiqh->phy_mode == PHYMODE_3x3_1x1)) {
						if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
							wlc_airiq_phy_disable_fft_capture(airiqh);
						} else {
							wlc_lte_u_phy_disable_iq_capture_shm(airiqh);
						}
						wlc_airiq_set_enable(airiqh, FALSE);
						airiqh->scan.run_phycal = FALSE;
					} else {
						wlc_airiq_scan_abort(airiqh, FALSE);
						wlc_phy_hold_upd(WLC_PI(airiqh->wlc),
							PHY_HOLD_FOR_SCAN, FALSE);
					}
					wl_airiq_sendup_scan_complete_alternate(airiqh,
						AIRIQ_SCAN_SUCCESS);

#ifdef WLOFFLD
					// Offload-indicate to offload core that scan completed
					if (WLOFFLD_AIRIQ_ENAB(airiqh->wlc->pub)) {
						wlc_airiq_scan_complete_ol(airiqh,
							AIRIQ_SCAN_SUCCESS);
					}
#endif // endif
					SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
					return;
				}
			}
		}
	}

	if (airiqh->scan.home_scan) {
		wlc_airiq_phy_enable_fft_capture(airiqh,
			airiqh->scan.capture_interval_us[airiqh->scan.channel_idx],
			airiqh->gain.code[CHSPEC_CHANNEL(airiqh->wlc->home_chanspec)],
			airiqh->wlc->home_chanspec,
			airiqh->capture_limit);
		wlc_read_tsf(airiqh->wlc, &airiqh->chan_tsf, &dummy_tsf_high);

		wl_add_timer(airiqh->wlc->wl, airiqh->timer,
			airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx], 0);
		return;
	}

	/*  */
	/* get chanspec from list */
	/*  */
	chanspec = airiqh->scan.chanspec_list[airiqh->scan.channel_idx];

	/* Read the time before beginning the channel change process */
	wlc_read_tsf(airiqh->wlc, &tsf, &dummy_tsf_high);

	/* Disabling FFT capture (and resetting gain to defaults on 43460)
	 * will cause gain change in the middle of an ongoing FFT capture. This causes the
	 * incorrect gain to be used for the last FFT on the channel. Therefore, call this
	 * function after suspending the mac.
	 */
	if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
		wlc_airiq_phy_disable_fft_capture(airiqh);
	} else {
		wlc_lte_u_phy_disable_iq_capture_shm(airiqh);
	}

	/* set channel */
	wlc_airiq_scan_set_chanspec_3p1(airiqh, pi, pi->radio_chanspec, chanspec);

	if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
		wlc_airiq_phy_enable_fft_capture(airiqh,
			airiqh->scan.capture_interval_us[airiqh->scan.channel_idx],
			airiqh->gain.code[CHSPEC_CHANNEL(chanspec)],
			chanspec,
			airiqh->capture_limit);
	} else {
		wlc_lte_u_phy_enable_iq_capture_shm(airiqh,
			airiqh->scan.capture_interval_us[airiqh->scan.channel_idx],
			airiqh->gain.code[CHSPEC_CHANNEL(chanspec)],
			chanspec,
			airiqh->capture_limit);
	}

	wlc_read_tsf(airiqh->wlc, &airiqh->chan_tsf, &dummy_tsf_high);
	/* XXX vasip clock loses a few microseconds during channel change,
	 * so we track it and correct for this offset on VASIP FFT timestamps.
	 */
	airiqh->start_time_mac = airiqh->chan_tsf;
	airiqh->latch_vasip_start_time = TRUE;

	SCAN_DBG("%s: [%d] channel switch elapsed time: %d us\n",
		__FUNCTION__, CHSPEC_CHANNEL(chanspec), airiqh->chan_tsf - tsf);
	/* Set timer for next channel scan */
	wl_add_timer(airiqh->wlc->wl, airiqh->timer,
		airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx], 0);

	SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
}

#endif /* VASIP_HW_SUPPORT */

/* return current scan chanspec*/
chanspec_t wlc_airiq_get_current_scan_chanspec(wlc_info_t * wlc) {
	airiq_info_t *airiqh = wlc->airiq;
	return airiqh->scan.chanspec_list[airiqh->scan.channel_idx];
}
/* timer function to implement channel scanning in 4x4 phy mode. The interface */
/* is assumed to be in the UP state. */
void wlc_airiq_scantimer(void *arg)
{
	//static uint cnt;
	airiq_info_t *airiqh;
	chanspec_t chanspec;
	int chancnt;
	uint32 dummy_tsf_high, tsf;
	bool sweep_begin = FALSE;
	uint16 fft_smpl_ctrl = 0;

	airiqh = (airiq_info_t*)arg;

	if (!airiqh) {
		WL_ERROR(("%s(%d): null invalid airiq handle\n",
					__FUNCTION__, __LINE__));
		return;
	}

#ifdef VASIP_HW_SUPPORT
	if (airiqh->phy_mode == PHYMODE_3x3_1x1) {
		wlc_airiq_scantimer_3p1(arg);
		return;
	}
#endif // endif

	if (airiqh->scan_type != SCAN_TYPE_AIRIQ) {
		WL_ERROR(("%s(%d): invalid AIRIQ 4x4 scan type: 0x%X\n",
					__FUNCTION__, __LINE__, airiqh->scan_type));
		return;
	}

	/*  */
	/* Increment to next channel */
	/*  */
	chancnt = airiqh->scan.channel_cnt;

	SCAN_DBG("-->%s\n", __FUNCTION__);

	if (airiqh->tx_suspending) {
		if (wlc_tx_suspended(airiqh->wlc)) {
			airiqh->tx_suspending = FALSE;
		} else {
			/* wait */
			WL_PRINT(("%s: tx still suspending. TXPKTPENDTOT=%d\n",__FUNCTION__,
						TXPKTPENDTOT(airiqh->wlc)));
			/* wait a little longer and force it - (see guard timer in wlc_scan.c) */
			airiqh->tx_suspending = FALSE;
			wl_add_timer(airiqh->wlc->wl, airiqh->timer, 10, 0);
			SCAN_DBG("<--%s(%d)\n",__FUNCTION__,__LINE__);
			return;
		}
	}

	if (!airiqh->wlc->pub->up) {
		WL_AIRIQ(("%s(%d): wl%d not up. Scan aborted/Interface down.\n",
			__FUNCTION__, __LINE__, airiqh->wlc->pub->unit));
		wlc_airiq_set_enable(airiqh, FALSE);
		airiqh->scan.run_phycal = FALSE;
		wlc_airiq_set_scan_in_progress(airiqh, FALSE);
		airiqh->wlc->scan->state &= ~(SCAN_STATE_PROHIBIT | SCAN_STATE_PASSIVE);
		SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
		if (airiqh->sweep_count >= 0) {
			/* Send a scan completion notification (we need to add status) */
			wl_airiq_sendup_scan_complete_alternate(airiqh, AIRIQ_SCAN_ABORTED);
		}
		return;
	}

	if (!airiqh->scan_enable) {
		WL_AIRIQ(("%s(%d): wl%d scan stopped.\n",
			__FUNCTION__, __LINE__, airiqh->wlc->pub->unit));
		SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
		if (airiqh->sweep_count >= 0) {
			/* Send a scan completion notification (we need to add status) */
			wl_airiq_sendup_scan_complete_alternate(airiqh,
				AIRIQ_SCAN_ABORTED);
		}
		airiqh->scan.run_phycal = FALSE;
		return;

	}

	/* Age out gain hints  and issue mac event */
	if (airiqh->scan.scan_start) {

		/* first pass we do not want to age, or create a scan complete event! */
		airiqh->scan.scan_start = FALSE;
		airiqh->scan.channel_idx = 0;

		airiqh->fft_count = 0;
		airiqh->capture_limit = MAX(10,
			airiqh->scan.capture_count[airiqh->scan.channel_idx]);

		SCAN_DBG("%s: ***SCANSTART***\n",  __FUNCTION__);
		sweep_begin = TRUE;
	} else {
		wlc_read_tsf(airiqh->wlc, &tsf, &dummy_tsf_high);
		fft_smpl_ctrl = wlc_read_shm(airiqh->wlc, M_FFT_SMPL_CTRL(airiqh->wlc));

		if (fft_smpl_ctrl == 0) {
			// FFT_SMPL_CTRL is a countdown counter.
			WL_AIRIQ(("%s: ch[%d]:%3d fftcnt=%4d, target = %4d:%6dus. SMPL_CTRL=0\n",
				__FUNCTION__, airiqh->scan.channel_idx,
				CHSPEC_CHANNEL(airiqh->wlc->chanspec),
				airiqh->fft_count, airiqh->capture_limit,
				tsf - airiqh->chan_tsf));

			airiqh->scan.channel_idx = (airiqh->scan.channel_idx + 1) % chancnt;
		} else if ((tsf - airiqh->chan_tsf) < (20 +
			airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx])*1000) {
			// max dwell is 20 ms+  the nominal value.
			WL_AIRIQ(("%s: ch[%d]:%3d fftcnt=%4d < %4d: %6d us ***WAIT***\n",
				__FUNCTION__, airiqh->scan.channel_idx,
				CHSPEC_CHANNEL(airiqh->wlc->chanspec),
				airiqh->fft_count, airiqh->capture_limit,
				tsf - airiqh->chan_tsf));
			// extend time by 5ms interval
			wl_add_timer(airiqh->wlc->wl, airiqh->timer, 5, 0);

			SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
			return;
		} else {
			WL_AIRIQ(("%s: ch[%d]:%3d fftcnt=%4d<%4d: actual dwell time: %6d us"
				"vs targeted: %6d us TIMEOUT\n",__FUNCTION__,
				airiqh->scan.channel_idx,
				CHSPEC_CHANNEL(airiqh->scan.
				chanspec_list[airiqh->scan.channel_idx]),
				airiqh->fft_count, airiqh->capture_limit,
				tsf - airiqh->chan_tsf,
				airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx]*1000));
			// stop current fft capture
			wlc_airiq_phy_disable_fft_capture(airiqh);
			airiqh->scan.channel_idx = (airiqh->scan.channel_idx + 1) % chancnt;
		}

		airiqh->fft_count = 0;
		airiqh->capture_limit = MAX(10, airiqh->scan.capture_count[airiqh->scan.channel_idx]);

		if ((airiqh->scan.channel_idx == 0) && !sweep_begin && (airiqh->sweep_count >= 0)) {
			airiqh->sweep_count--;
			if (airiqh->sweep_count <= 0) {
				if (airiqh->scan.home_scan) {
					wlc_airiq_phy_disable_fft_capture(airiqh);
					wlc_airiq_set_enable(airiqh, FALSE);
					airiqh->scan.run_phycal = FALSE;
					wlc_airiq_set_scan_in_progress(airiqh, FALSE);
				} else {
					wlc_airiq_scan_abort(airiqh, FALSE);
					wlc_phy_hold_upd(WLC_PI(airiqh->wlc), PHY_HOLD_FOR_SCAN, FALSE);
				}
				wl_airiq_sendup_scan_complete_alternate(airiqh,
					AIRIQ_SCAN_SUCCESS);
#ifdef WLOFFLD
				// Offload-indicate to offload core that scan completed
				if (WLOFFLD_AIRIQ_ENAB(airiqh->wlc->pub)) {
					wlc_airiq_scan_complete_ol(airiqh,
						AIRIQ_SCAN_SUCCESS);
				}
#endif // endif
				SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
				return;
			}
		}
	}

	if (airiqh->scan.home_scan) {
		wlc_airiq_phy_enable_fft_capture(airiqh,
			airiqh->scan.capture_interval_us[airiqh->scan.channel_idx],
			airiqh->gain.code[CHSPEC_CHANNEL(airiqh->wlc->home_chanspec)],
			airiqh->wlc->home_chanspec,
			airiqh->capture_limit);
		wlc_read_tsf(airiqh->wlc, &airiqh->chan_tsf, &dummy_tsf_high);
		wl_add_timer(airiqh->wlc->wl, airiqh->timer,
			airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx], 0);
		return;
	}

	/*  */
	/* get chanspec from list */
	/*  */
	chanspec = airiqh->scan.chanspec_list[airiqh->scan.channel_idx];

	/* Read the time before beginning the channel change process */
	wlc_read_tsf(airiqh->wlc, &tsf, &dummy_tsf_high);

	/* Disabling FFT capture (and resetting gain to defaults on 43460)
	 * will cause gain change in the middle of an ongoing FFT capture. This causes the
	 * incorrect gain to be used for the last FFT on the channel. Therefore, call this
	 * function after suspending the mac.
	 */
	wlc_airiq_phy_disable_fft_capture(airiqh);

	/* set channel */
	// Indicate that we may scan 'prohibited' channels
	airiqh->wlc->scan->state |= SCAN_STATE_PROHIBIT | SCAN_STATE_PASSIVE;
	wlc_suspend_mac_and_wait(airiqh->wlc);

	/* Add block to suppress tx packets */
	wlc_block_datafifo(airiqh->wlc, DATA_BLOCK_CHANSW, DATA_BLOCK_CHANSW);

	/* mute AP - suspend FIFO's */
	wlc_ap_mute(airiqh->wlc, TRUE, NULL, WLC_AP_MUTE_SCAN);

	wlc_set_chanspec(airiqh->wlc, chanspec,CHANSW_REASON(CHANSW_IOVAR));
	wlc_phy_set_deaf(WLC_PI(airiqh->wlc),TRUE);
	wlc_enable_mac(airiqh->wlc);

	wlc_airiq_phy_enable_fft_capture(airiqh,
		airiqh->scan.capture_interval_us[airiqh->scan.channel_idx],
		airiqh->gain.code[CHSPEC_CHANNEL(chanspec)],
		chanspec,
		airiqh->capture_limit);

	wlc_read_tsf(airiqh->wlc, &airiqh->chan_tsf, &dummy_tsf_high);
	/* XXX vasip clock loses a few microseconds during channel change,
	 * so we track it and correct for this offset on VASIP FFT timestamps.
	 */
	airiqh->start_time_mac = airiqh->chan_tsf;
	airiqh->latch_vasip_start_time = TRUE;

	SCAN_DBG("%s: [%d] channel switch elapsed time: %d us\n",
		__FUNCTION__, CHSPEC_CHANNEL(chanspec), airiqh->chan_tsf - tsf);
	/* Set timer for next channel scan */

	wl_add_timer(airiqh->wlc->wl, airiqh->timer,
		airiqh->scan.dwell_interval_ms[airiqh->scan.channel_idx], 0);

	SCAN_DBG("<--%s(%d)\n", __FUNCTION__, __LINE__);
}

int wlc_airiq_start_scan_phase2(airiq_info_t *airiqh)
{
	uint32 dummy_tsf_high;

	if (!airiqh->scan.scan_start) {
		/* no scan requested: (if using AIRIQ_3PLUS1 iovar) */
		return 0;
	}

	/* save the start time */
	wlc_read_tsf(airiqh->wlc, &airiqh->scan.timestamp_us, &dummy_tsf_high);

	wl_add_timer(airiqh->wlc->wl, airiqh->timer, 0, 0);

	/* Reset MPC: note that setting scan->in_progress insures the radio will stay on
	 * In STA mode, must update the MPC setting to put
	 * the chip back in low power mode if necessary
	 */
	airiqh->wlc->mpc_scan = FALSE;
#ifdef STA
	wlc_radio_mpc_upd(airiqh->wlc);
#endif /* STA */

	if (airiqh->phy_mode != PHYMODE_3x3_1x1) {
		/* Add block to suppress tx packets */
		wlc_block_datafifo(airiqh->wlc, DATA_BLOCK_CHANSW, DATA_BLOCK_CHANSW);
		/* mute AP */
		wlc_ap_mute(airiqh->wlc, TRUE, NULL, WLC_AP_MUTE_SCAN);
		/* suspend tx fifos */

		wlc_tx_suspend(airiqh->wlc);
		airiqh->tx_suspending = TRUE;
	}

	return 0;
}

/*
 * initiate channel scanning for airiq.
 *
 */
int wlc_airiq_start_scan(airiq_info_t *airiqh, int sweep_cnt, int home)
{
	int err;

	if (!airiqh) {
		WL_AIRIQ(("wl%d: %s: null airiq handle\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__));
		return -1;
	}
	if (airiqh->scan_enable || SCAN_IN_PROGRESS(airiqh->wlc->scan)) {
		WL_ERROR(("wl%d: %s: scan in progress.\n", WLCWLUNIT(airiqh->wlc), __FUNCTION__));
		return BCME_BUSY;
	}
#if defined(WL_MODESW)
	/* attempt to get lock as OMN master before issuing modesw calls */
	if ((err = wlc_modesw_omn_master_set(airiqh->wlc->modesw, WL_OMNM_MOD_AIRIQ, 0)) != BCME_OK) {
		WL_AIRIQ(("wl%d: %s: airiq could not get lock as OMN master (%d)\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__, err));
		return err;
	}
	if (airiqh->modeswitch_state == AIRIQ_MODESW_DOWNGRADE_IN_PROGRESS ||
	    airiqh->modeswitch_state == AIRIQ_MODESW_UPGRADE_IN_PROGRESS) {
		WL_ERROR(("wl%d: %s: cannot start. modeswitch_state=%d.\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__, airiqh->modeswitch_state));
		return BCME_BUSY;
	}
#endif // endif
	/* if radio is disabled due to disassociation, turn it on */
	airiqh->wlc->mpc_scan = TRUE;
	/* In station mode, must update the MPC setting to put
	 * the chip back in low power mode if necessary
	 */
#ifdef STA
	wlc_radio_mpc_upd(airiqh->wlc);
#endif /* STA */

	if (!airiqh->wlc->pub->up) {
		return BCME_NOTUP;
	}

#ifdef VASIP_HW_SUPPORT
	if (airiqh->phy_mode == PHYMODE_3x3_1x1) {
#ifdef RXCHAIN_PWRSAVE
		/* If AP is currently in rxchain power save, come out of it */
		if (wlc_ap_in_rxchain_power_save(airiqh->wlc->ap)) {
			wlc_reset_rxchain_pwrsave_mode(airiqh->wlc->ap);
		}
#endif /* RXCHAIN_PWRSAVE */

		/* prepare interface if necessary */
		err = wlc_airiq_3p1_scan_prep(airiqh);

		if (err == BCME_OK) {
			WL_AIRIQ(("wl%d: %s: initiated 3+1 scan. radio successfully downgraded.\n",
				WLCWLUNIT(airiqh->wlc), __FUNCTION__));
#if defined(WL_MODESW)
			wlc_airiq_modeswitch_state_upd(airiqh, AIRIQ_MODESW_DOWNGRADE_FINISHED);
#endif // endif
		} else if (err == BCME_BUSY) {
			/* Must reconfigure and wait for callback */
			WL_AIRIQ(("wl%d: %s: preparing for 3+1 scan. downgrading radio.\n",
				WLCWLUNIT(airiqh->wlc), __FUNCTION__));
			airiqh->scan.scan_start = TRUE;
			airiqh->scan.home_scan = home;
			wlc_airiq_set_enable(airiqh, TRUE);
			airiqh->sweep_count = sweep_cnt;
#if defined(WL_MODESW)
			wlc_airiq_modeswitch_state_upd(airiqh, AIRIQ_MODESW_DOWNGRADE_IN_PROGRESS);
#endif // endif

			return 0;
		} else {
			WL_ERROR(("wl%d: %s: error starting 3+1 scan, couldn't downgrade err=%d.\n",
				WLCWLUNIT(airiqh->wlc), __FUNCTION__, err));
			return err;
		}

	} else if (PHYMODE(airiqh->wlc) == PHYMODE_3x3_1x1) {
		WL_AIRIQ(("wl%d: %s: Preparing for 4x4 mode scan. "
			"Aborting previous scan to upgrade.\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__));
		/* Handle transition from 3+1 mode to 4x4 mode */
		wlc_airiq_scan_abort(airiqh, TRUE);
		return BCME_BUSY;
	}
#endif /* VASIP_HW_SUPPORT */
	airiqh->scan.scan_start = TRUE;
	airiqh->scan.home_scan = home;
	wlc_airiq_set_enable(airiqh, TRUE);
	if (airiqh->phy_mode != PHYMODE_3x3_1x1) {
		wlc_airiq_set_scan_in_progress(airiqh, TRUE);
	}
	airiqh->sweep_count = sweep_cnt;

	err = wlc_airiq_start_scan_phase2(airiqh);

	return err;
}

int wlc_airiq_scan_abort(airiq_info_t *airiqh, bool upgrade)
{
	if (!airiqh) {
		WL_AIRIQ(("wl%d: %s: null airiq handle\n", WLCWLUNIT(airiqh->wlc), __FUNCTION__));
		return -1;
	}
	if (airiqh->scan_enable) {
		/* disable FFT capture */
		if (airiqh->scan_type == SCAN_TYPE_AIRIQ) {
			airiqh->wlc->scan->state &= ~(SCAN_STATE_PROHIBIT | SCAN_STATE_PASSIVE);
			wlc_airiq_phy_disable_fft_capture(airiqh);
		} else {
			wlc_lte_u_phy_disable_iq_capture_shm(airiqh);
		}
	}

	if (airiqh->phy_mode != PHYMODE_3x3_1x1 ) {
		wlc_airiq_set_scan_in_progress(airiqh, FALSE);
	}

	if (upgrade) {
		WL_AIRIQ(("wl%d: %s: upgrading: phymode=%d(%d) modesw_state=%d, enab=%d\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__, airiqh->phy_mode,
			PHYMODE(airiqh->wlc), airiqh->modeswitch_state,
			WLC_MODESW_ENAB(airiqh->wlc->pub)));
#if defined(VASIP_HW_SUPPORT)
		if (airiqh->phy_mode == PHYMODE_3x3_1x1) {
			airiqh->phy_mode = 0;
#if defined(WL_MODESW)
			if (airiqh->modeswitch_state != AIRIQ_MODESW_UPGRADE_IN_PROGRESS) {
				if (WLC_MODESW_ENAB(airiqh->wlc->pub)) {
					wlc_airiq_3p1_upgrade_wlc(airiqh->wlc);
				}
			}
#else
			wlc_airiq_3p1_upgrade_phy(airiqh);
#endif /* WL_MODESW */
		}
#endif /* VASIP_HW_SUPPORT */
	} else if (airiqh->phy_mode == PHYMODE_3x3_1x1) {
		/* return for no upgrade case */
		return 0;
	}

	airiqh->scan.run_phycal = FALSE;
	wlc_airiq_set_enable(airiqh, FALSE);

	wlc_suspend_mac_and_wait(airiqh->wlc);

	wlc_phy_clear_deaf(WLC_PI(airiqh->wlc), FALSE);

	/* set channel */
	wlc_set_chanspec(airiqh->wlc, airiqh->wlc->home_chanspec, CHANSW_REASON(CHANSW_IOVAR));

	/* unmute AP */
	wlc_ap_mute(airiqh->wlc, FALSE, NULL, WLC_AP_MUTE_SCAN);

#ifdef STA
	/* enable CFP and TSF update */
	wlc_mhf(airiqh->wlc, MHF2, MHF2_SKIP_CFP_UPDATE, 0, WLC_BAND_ALL);
	wlc_skip_adjtsf(airiqh->wlc, FALSE, NULL, WLC_SKIP_ADJTSF_SCAN, WLC_BAND_ALL);
#endif /* STA */

	wlc_enable_mac(airiqh->wlc);

	//TODO pere wlc_phy_clear_tssi(WLC_PI(airiqh->wlc));
	/* Remove fifo suspend for tx data */
	wlc_tx_resume(airiqh->wlc);
	/* Remove block for tx data */
	wlc_block_datafifo(airiqh->wlc, DATA_BLOCK_CHANSW, 0);
	/* run txq if not empty */
	wlc_send_active_q(airiqh->wlc);

	/* In station mode, must update the MPC setting to put
	 * the chip back in low power mode if necessary
	 */
#ifdef STA
	wlc_radio_mpc_upd(airiqh->wlc);
#endif /* STA */
WL_AIRIQ(("PERE>>> wl%d: %s(%d): upgrading: phymode=%d(%d) modesw_state=%d, enab=%d\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__,__LINE__, airiqh->phy_mode,
			PHYMODE(airiqh->wlc), airiqh->modeswitch_state,
			WLC_MODESW_ENAB(airiqh->wlc->pub)));

	return 0;
}

int wlc_airiq_start_scan_extern(airiq_info_t *airiqh)
{
	int err;

	// external method only performs one sweep.
	err = wlc_airiq_start_scan(airiqh, 1, 0);

	return err;
}

bool wlc_airiq_scan_in_progress(wlc_info_t *wlc)
{
	if (wlc->airiq) {
		return wlc->airiq->scan_enable;
	}
	return FALSE;
}

/* 3+1 mode scanning for wave2 */
int
wlc_airiq_3p1_scan_prep(airiq_info_t *airiqh)
{
	uint16 cur_phymode = PHYMODE(airiqh->wlc);

	if (cur_phymode == PHYMODE_3x3_1x1) {
		/* the PHY is already configured for 3+1 */
		return BCME_OK;
	}

	/* Must reconfigure the radio for 3+1 */
#ifdef WL_MODESW
	/* registering the modesw notif callback is already done
	 * in the attach function, but it is retried here if that
	 * attempt was not successful. This is done prior to requesting
	 * the mode switch/downgrade
	 */
	if (!airiqh->modesw_cb_regd) {
		if (airiqh->wlc->modesw &&
			wlc_modesw_notif_cb_register(airiqh->wlc->modesw,
				wlc_airiq_opmode_change_cb, airiqh->wlc) == BCME_OK) {
			airiqh->modesw_cb_regd = TRUE;
		} else {
			WL_ERROR(("wl%d: %s: Could not initialize modesw notification callback\n",
				WLCWLUNIT(airiqh->wlc), __FUNCTION__));
		}
	}

	/* downgrade from 4x4 to 3x3 + 1 radar scan core mode after announcing */
	if (airiqh->modesw_cb_regd &&
	    wlc_airiq_3p1_downgrade_wlc(airiqh->wlc) == BCME_BUSY) {
		/* downgrade phy later */
		return BCME_BUSY;
	} else
#endif /* WL_MODESW */
	{
		/* downgrade and set to first chanspec */
		wlc_airiq_3p1_downgrade_phy(airiqh, airiqh->scan.chanspec_list[0]);
		return BCME_OK;
	}
}

int
lte_u_get_scan_config(airiq_info_t *airiqh, airiq_config_t *sc, int size)
{
	int32 cnt = airiqh->scan.user_channel_cnt;

	memcpy(sc->chanspec_list,
	       airiqh->scan.user_chanspec_list,
	       cnt * sizeof(chanspec_t));
	memcpy(sc->dwell_interval_ms,
	       airiqh->scan.dwell_interval_ms,
	       cnt * sizeof(sc->dwell_interval_ms[0]));
	memcpy(sc->capture_interval_us,
	       airiqh->scan.capture_interval_us,
	       cnt * sizeof(sc->capture_interval_us[0]));
	memcpy(sc->capture_count,
	       airiqh->scan.capture_count,
	       cnt * sizeof(sc->capture_count[0]));
	sc->chanspec_cnt = airiqh->scan.channel_cnt;
	return 0;
}

int
lte_u_get_detector_config(airiq_info_t *airiqh, lte_u_detector_config_t *dc, int size)
{
	memcpy(dc, &(airiqh->detector_config),
		sizeof(lte_u_detector_config_t));
	return 0;
}

bool
lte_u_chanspec_list_valid(airiq_info_t *airiqh, airiq_config_t *sc)
{
	int i;

	for (i = 0; i < sc->chanspec_cnt; i++) {
		if (! BAND_5G(CHSPEC2WLC_BAND(sc->chanspec_list[i]))) {
			WL_AIRIQ(("wl%d: %s: BW is not 5G BW of channel [%d] = %x\n",
				WLCWLUNIT(airiqh->wlc), __FUNCTION__,
				i, CHSPEC2WLC_BAND(sc->chanspec_list[i])));
			return FALSE;
		}
	}

	return TRUE;
}

int
lte_u_update_detector_config(airiq_info_t *airiqh, lte_u_detector_config_t *dc)
{
	memcpy(&airiqh->detector_config, dc, sizeof(lte_u_detector_config_t));
	airiqh->detector_config.detector_configured = TRUE;
	return 0;
}

int
lte_u_update_scan_config(airiq_info_t *airiqh, airiq_config_t *sc)
{
	int32 cnt;
	int err = 0, i, j, k;
	uint16 bw;
	uint16 ch;
	int scan_translation_40MHz[2];
	int scan_translation_80MHz[4];

	cnt = MIN(MAX_CHANSPECS, sc->chanspec_cnt); /* ensure bounds */
	memcpy(airiqh->scan.user_chanspec_list,
	       sc->chanspec_list,
	       cnt * sizeof(chanspec_t));
	airiqh->scan.user_channel_cnt = cnt;

	airiqh->scan.channel_idx = cnt - 1;
	airiqh->phy_mode = PHYMODE_3x3_1x1;
	airiqh->core = 3;
	airiqh->lte_u_scan_configured = TRUE;

	bw = CHSPEC_BW(airiqh->scan.user_chanspec_list[0]);

	switch (bw) {
	case WL_CHANSPEC_BW_20:
		memcpy(airiqh->scan.chanspec_list, sc->chanspec_list,
			cnt * sizeof(chanspec_t));
		memcpy(airiqh->scan.dwell_interval_ms, sc->dwell_interval_ms,
			cnt * sizeof(sc->dwell_interval_ms[0]));
		memcpy(airiqh->scan.capture_interval_us, sc->capture_interval_us,
			cnt * sizeof(sc->capture_interval_us[0]));
		memcpy(airiqh->scan.capture_count, sc->capture_count,
			cnt * sizeof(sc->capture_count[0]));
		airiqh->scan.channel_cnt = cnt;
		for (i = 0; i < cnt; i++) {
			airiqh->user_channel_mapping[i] = i;
		}
		break;
	case WL_CHANSPEC_BW_40:
		k = 0;
		airiqh->scan.channel_cnt  = 0;
		for (i = 0; i < cnt; i++) {
			ch = CHSPEC_CHANNEL(sc->chanspec_list[i]);
			if ((ch == 38) || (ch == 46)) {
				/* user Channels 38,46  maps to 36,40,44,48
				 * example 38 maps to 36 and 40
				 */
				scan_translation_40MHz[0] = ch - 2;
				scan_translation_40MHz[1] = ch + 2;
				for (j = 0; j < 2; j++) {
					airiqh->scan.chanspec_list[k] =
						scan_translation_40MHz[j] | WL_CHANSPEC_BW_40 |
						WL_CHANSPEC_BAND_5G;

					airiqh->scan.dwell_interval_ms[k] =
						sc->dwell_interval_ms[i];

					airiqh->scan.capture_interval_us[k] =
						sc->capture_interval_us[i];

					airiqh->scan.capture_count[k] = sc->capture_count[i];
					airiqh->user_channel_mapping[k] = i;
					k++;
				}
				airiqh->scan.channel_cnt  += 2;
			} else if ((ch == 151) || (ch == 159)) {
				/* user Channels 151,159 maps to 149,153,157,161 scan channels */
				scan_translation_40MHz[0] = ch - 2;
				scan_translation_40MHz[1] = ch + 2;
				for (j = 0; j < 2; j++) {
					airiqh->scan.chanspec_list[k] =
						scan_translation_40MHz[j] | WL_CHANSPEC_BW_40 |
						WL_CHANSPEC_BAND_5G;
					airiqh->scan.dwell_interval_ms[k] =
						sc->dwell_interval_ms[i];
					airiqh->scan.capture_interval_us[k] =
						sc->capture_interval_us[i];
					airiqh->scan.capture_count[k] = sc->capture_count[i];
					airiqh->user_channel_mapping[k] = i;
					k++;
				}
				airiqh->scan.channel_cnt  += 2;
			} else if (ch == 165) {
				airiqh->scan.chanspec_list[k] =
					ch | WL_CHANSPEC_BW_40 | WL_CHANSPEC_BAND_5G;
				airiqh->scan.dwell_interval_ms[k] = sc->dwell_interval_ms[i];
				airiqh->scan.capture_interval_us[k] = sc->capture_interval_us[i];
				airiqh->scan.capture_count[k] = sc->capture_count[i];
				airiqh->user_channel_mapping[k] = i;
				k++;
				airiqh->scan.channel_cnt  += 1;
			} else {
				airiqh->lte_u_scan_configured = FALSE;
				WL_ERROR(("%s: unknown channel/bw combination ch=%d bw=0x%x\n",
					__FUNCTION__, ch, bw));
			}
		}
		break;
	case WL_CHANSPEC_BW_80:
		k = 0;
		airiqh->scan.channel_cnt  = 0;
		for (i = 0; i < cnt; i++) {
			ch = CHSPEC_CHANNEL(sc->chanspec_list[i]);
			if (ch == 42) {
				scan_translation_80MHz[0] = 36;
				scan_translation_80MHz[1] = 40;
				scan_translation_80MHz[2] = 44;
				scan_translation_80MHz[3] = 48;
				for (j = 0; j < 4; j++) {
					airiqh->scan.chanspec_list[k] = scan_translation_80MHz[j] |
						WL_CHANSPEC_BW_80 | WL_CHANSPEC_BAND_5G;
					airiqh->scan.dwell_interval_ms[k] =
						sc->dwell_interval_ms[i];
					airiqh->scan.capture_interval_us[k] =
						sc->capture_interval_us[i];
					airiqh->scan.capture_count[k] = sc->capture_count[i];
					airiqh->user_channel_mapping[k] = i;
					k++;
				}
				airiqh->scan.channel_cnt  += 4;
			} else if (ch == 155) {
				scan_translation_80MHz[0] = 149;
				scan_translation_80MHz[1] = 153;
				scan_translation_80MHz[2] = 157;
				scan_translation_80MHz[3] = 161;
				for (j = 0; j < 4; j++) {
					airiqh->scan.chanspec_list[k] = scan_translation_80MHz[j] |
						WL_CHANSPEC_BW_80 | WL_CHANSPEC_BAND_5G;
					airiqh->scan.dwell_interval_ms[k] =
						sc->dwell_interval_ms[i];
					airiqh->scan.capture_interval_us[k] =
						sc->capture_interval_us[i];
					airiqh->scan.capture_count[k] = sc->capture_count[i];
					airiqh->user_channel_mapping[k] = i;
					k++;
				}
				airiqh->scan.channel_cnt  += 4;
			} else if (ch == 165) {
				airiqh->scan.chanspec_list[k] =
					165 | WL_CHANSPEC_BW_80 | WL_CHANSPEC_BAND_5G;
				airiqh->scan.dwell_interval_ms[k] = sc->dwell_interval_ms[i];
				airiqh->scan.capture_interval_us[k] = sc->capture_interval_us[i];
				airiqh->scan.capture_count[k] = sc->capture_count[i];
				airiqh->user_channel_mapping[k] = i;
				k++;
				airiqh->scan.channel_cnt  += 1;
			} else {
				airiqh->lte_u_scan_configured = FALSE;
				WL_ERROR(("%s: unknown channel/bw combination ch=%d bw=0x%x\n",
					__FUNCTION__, ch, bw));
			}
		}
		break;
	default:
		airiqh->lte_u_scan_configured = FALSE;
		airiqh->scan.channel_cnt  = 0;
		WL_ERROR(("%s: unknown bw 0x%x\n", __FUNCTION__, bw));
		break;
	}

	for (i = 0; i < airiqh->scan.channel_cnt; i++) {
		// Init lte_scan_status
		airiqh->scan.lte_scan_status[i].lte_u_present  = FALSE;
		airiqh->scan.lte_scan_status[i].chanspec       = sc->chanspec_list[i];
		airiqh->scan.lte_scan_status[i].timestamp      = 0x10000000;
		airiqh->scan.lte_scan_status[i].prevTimestamp  = 0;
		airiqh->scan.lte_scan_status[i].lte_u_active   = FALSE;
	}

	return err;
}

int wlc_lte_u_scan_abort(airiq_info_t *airiqh, bool upgrade)
{
	int err = 0;

	if (!airiqh) {
		WL_AIRIQ(("wl%d: %s: null lte_u handle\n", WLCWLUNIT(airiqh->wlc), __FUNCTION__));
		return -1;
	}
	if (airiqh->scan_enable) {
		/* disable IQ capture */
		wlc_lte_u_phy_disable_iq_capture_shm(airiqh);
	}

	OSL_DELAY(50); /* last IQ data is still in the pipe.  Wait for it to complete. */

	if (upgrade) {
		WL_AIRIQ(("wl%d: %s: upgrading: phymode=%d(%d) modesw_state=%d, enab=%d\n",
			WLCWLUNIT(airiqh->wlc), __FUNCTION__, airiqh->phy_mode,
			PHYMODE(airiqh->wlc), airiqh->modeswitch_state,
			WLC_MODESW_ENAB(airiqh->wlc->pub)));
#if defined(VASIP_HW_SUPPORT)
		if (airiqh->phy_mode == PHYMODE_3x3_1x1) {
			airiqh->phy_mode = 0;
#if defined(WL_MODESW)
			if (airiqh->modeswitch_state != AIRIQ_MODESW_UPGRADE_IN_PROGRESS) {
				wlc_airiq_modeswitch_state_upd(airiqh,
					AIRIQ_MODESW_UPGRADE_IN_PROGRESS);
				if (WLC_MODESW_ENAB(airiqh->wlc->pub)) {
					wlc_airiq_3p1_upgrade_wlc(airiqh->wlc);
				}
			}
#else
			wlc_airiq_3p1_upgrade_phy(airiqh);
#endif /* WL_MODESW */
		}
#endif /* VASIP_HW_SUPPORT */
	}
	if (airiqh->scan_enable == FALSE) {
		/* no active scan, just return */
		return 0;
	}
	wlc_airiq_set_enable(airiqh, FALSE);

	wlc_suspend_mac_and_wait(airiqh->wlc);

	wlc_phy_clear_deaf(WLC_PI(airiqh->wlc), FALSE);

	/* set channel */
	wlc_set_chanspec(airiqh->wlc, airiqh->wlc->home_chanspec, CHANSW_REASON(CHANSW_IOVAR));

	/* unmute AP */
	wlc_ap_mute(airiqh->wlc, FALSE, NULL, WLC_AP_MUTE_SCAN);

#ifdef STA
	/* enable CFP and TSF update */
	wlc_mhf(airiqh->wlc, MHF2, MHF2_SKIP_CFP_UPDATE, 0, WLC_BAND_ALL);
	wlc_skip_adjtsf(airiqh->wlc, FALSE, NULL, WLC_SKIP_ADJTSF_SCAN, WLC_BAND_ALL);
#endif /* STA */

	wlc_enable_mac(airiqh->wlc);

//TODO	pere wlc_phy_clear_tssi(WLC_PI(airiqh->wlc));
	/* Remove fifo suspend for tx data */
	wlc_tx_resume(airiqh->wlc);
	/* Remove block for tx data */
	wlc_block_datafifo(airiqh->wlc, DATA_BLOCK_TXCHAIN, 0);
	/* run txq if not empty */
	wlc_send_active_q(airiqh->wlc);

	/* In station mode, must update the MPC setting to put
	 * the chip back in low power mode if necessary
	 */
#ifdef STA
	wlc_radio_mpc_upd(airiqh->wlc);
#endif /* STA */

	return err;
}
