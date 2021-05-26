/*
 * AP Module Public Interface
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
 * $Id: wlc_ap.h 767468 2018-09-12 08:14:28Z $
 */
#ifndef _WLC_AP_H_
#define _WLC_AP_H_

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)
#define WLPWRSAVERXFADD(wlc, v)	do { if ((wlc)->ap != NULL) (wlc)->ap->rxframe += (v); } while (0)
#define WLPWRSAVERXFINCR(wlc)	do { if ((wlc)->ap != NULL) (wlc)->ap->rxframe++; } while (0)
#define WLPWRSAVETXFINCR(wlc)	do { if ((wlc)->ap != NULL) (wlc)->ap->txframe++; } while (0)
#define WLPWRSAVERXFVAL(wlc)	(((wlc)->ap != NULL) ? (wlc)->ap->rxframe : 0)
#define WLPWRSAVETXFVAL(wlc)	(((wlc)->ap != NULL) ? (wlc)->ap->txframe : 0)
#endif // endif

struct wlc_ap_info {
	bool		shortslot_restrict;		/* only allow assoc by shortslot STAs */
	uint8		radio_pwrsave_enable;		/* radio duty cycle power save enable */
	uint16		pre_tbtt_us;		/* Current pre-TBTT us value */
	uint32		pre_tbtt_max_lat_us;	/* Max permitted latency for TBTT DPC */
	chanspec_t	pref_chanspec;			/* User preferred chanspec */
#ifdef EXT_STA
	uint		sta_scan_home_time;	/* scan home time when AP not started */
#endif /* EXT_STA */
	uint16		txbcn_timeout;			/* txbcn inactivity timeout */
	uint32		rxframe;		/* receive frame counter */
	uint32		txframe;		/* transmit frame counter */
#ifndef BCM_DCS_AND_RXCHAIN_PWRSAVE_NOT_IN_ROM /* WA for 4345b0 ROM */
	bool		dcs_enabled;
	uint8		rxchain_pwrsave_enable;	/* rxchain based power save enable */
#endif // endif
};

/* Time to live for probe response frames in microseconds; timed from when request arrived */
#if !defined(WLC_PRB_TTL_us)
#define WLC_PRB_TTL_us 40000
#endif /* ttl defined */

#ifdef BAND5G
#ifdef WLBTAMP
#define WL11H_AP_ENAB(wlc)	(AP_ENAB((wlc)->pub) && WL11H_ENAB(wlc) && \
				 !BTA_IN_PROGRESS(wlc) && !BTA_ACTIVE(wlc))
#else
#define WL11H_AP_ENAB(wlc)	(AP_ENAB((wlc)->pub) && WL11H_ENAB(wlc))
#endif /* WLBTAMP */
#else
#define WL11H_AP_ENAB(wlc)	0
#endif /* BAND5G */

#ifdef RXCHAIN_PWRSAVE
#define RXCHAIN_PWRSAVE_ENAB(ap) ((ap)->rxchain_pwrsave_enable)
#else
#define RXCHAIN_PWRSAVE_ENAB(ap) 0
#endif // endif

#ifdef RADIO_PWRSAVE
#define RADIO_PWRSAVE_ENAB(ap) ((ap)->radio_pwrsave_enable)
#else
#define RADIO_PWRSAVE_ENAB(ap) 0
#endif // endif

/* Idea is to find if STA (connected to upstream AP) and
 * local AP are on overlapping RADAR channels. If overlapping
 * and both on radar channel, then ap_sta_onradar should help
 * local AP to not start CAC, instead proceed to ISM. In ISM
 * local AP ignores radar detection now that STA is conneccted
 * to upstream AP.
 * At the point STA connected to upstream AP dis-connects, this
 * flag will help in not ignore the radar_detect in ISM.
 *
 * NOTE: There is an assumption here that in AP STA mode, always
 * STA comes up first and connectes to upstream AP before even
 * we bringup local AP interface. Otherwise if AP happens to come
 * up first then we get into CAC (phy_muted) which will stop
 * sta from connecting to upstream-AP.
 */
#define WLC_APSTA_ON_RADAR_CHANNEL(wlc)	(wlc_apsta_on_radar_channel(wlc->ap) == TRUE)

#ifdef AP
extern wlc_ap_info_t* wlc_ap_attach(wlc_info_t *wlc);
extern void wlc_ap_detach(wlc_ap_info_t *ap);
extern int wlc_ap_up(wlc_ap_info_t *apinfo, wlc_bsscfg_t *bsscfg);
extern int wlc_ap_down(wlc_ap_info_t *apinfo, wlc_bsscfg_t *bsscfg);
extern int wlc_ap_mbss4_tbtt(wlc_info_t *wlc, uint32 macintstatus);
extern int wlc_ap_mbss16_tbtt(wlc_info_t *wlc, uint32 macintstatus);
extern void wlc_ap_mbss16_write_prbrsp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend);
extern void wlc_mbss16_updssid(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_restart_ap(wlc_ap_info_t *ap);
extern void wlc_ap_authresp(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, uint8 *body, uint len, bool, bool);
extern void wlc_wme_setup_req(wlc_ap_info_t *ap, struct dot11_management_header *hdr,
	uint8 *body, int body_len);
extern void wlc_wme_initparams_ap(wlc_ap_info_t *ap, wme_param_ie_t *pe);
extern void wlc_eapol_event(wlc_ap_info_t *ap, const struct ether_addr *ea, uint8 *data,
	uint32 len);
extern void wlc_ap_process_assocreq(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, uint8 *body, uint body_len, struct scb *scb, bool);
extern void wlc_ap_process_assocreq_decision(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc);
extern bool wlc_roam_check(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct ether_header *eh, uint len);
extern bool wlc_apsta_on_radar_channel(wlc_ap_info_t *apinfo);

extern uint8 wlc_lowest_basicrate_get(wlc_bsscfg_t *cfg);

extern void wlc_ap_pspretend_probe(void *arg);
extern bool wlc_ap_do_pspretend_probe(wlc_info_t *wlc, struct scb *scb, uint32 elapsed_time);

#ifdef WL11N
extern bool wlc_ht_ap_coex_tea_chk(wlc_bsscfg_t *cfg, ht_cap_ie_t *cap_ie);
extern void wlc_ht_ap_coex_tebc_proc(wlc_bsscfg_t *cfg);
extern bool wlc_ht_ap_coex_ted_chk(wlc_bsscfg_t *cfg, bcm_tlv_t *tlv, uint8 vec);
extern void wlc_ht_coex_switch_bw(wlc_bsscfg_t *cfg, bool downgrade, uint8 rc);
extern void wlc_ht_coex_update_fid_time(wlc_bsscfg_t *cfg);
extern void wlc_ht_coex_update_permit(wlc_bsscfg_t *cfg, bool permit);
extern void wlc_switch_ab_11n(wlc_info_t *wlc, bool ab, bool init);
#endif /* WL11N */

extern uint wlc_ap_stas_associated(wlc_ap_info_t *ap);

extern void wlc_ap_bsscfg_scb_cleanup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_ap_scb_cleanup(wlc_info_t *wlc);

#ifdef RXCHAIN_PWRSAVE
extern void wlc_reset_rxchain_pwrsave_mode(wlc_ap_info_t *ap);
extern void wlc_disable_rxchain_pwrsave(wlc_ap_info_t *ap);
#ifdef WL11N
extern uint8 wlc_rxchain_pwrsave_stbc_rx_get(wlc_info_t *wlc);
#endif /* WL11N */
#endif /* RXCHAIN_PWRSAVE */

extern int wlc_ap_get_maxassoc(wlc_ap_info_t *ap);
extern void wlc_ap_set_maxassoc(wlc_ap_info_t *ap, int val);
extern int wlc_ap_get_maxassoc_limit(wlc_ap_info_t *ap);

#ifdef RADIO_PWRSAVE
extern int wlc_radio_pwrsave_in_power_save(wlc_ap_info_t *ap);
extern void wlc_radio_pwrsave_enter_mode(wlc_info_t *wlc, bool dtim);
extern void wlc_radio_pwrsave_exit_mode(wlc_ap_info_t *ap);
extern void wlc_radio_pwrsave_on_time_start(wlc_ap_info_t *ap, bool dtim);
#endif // endif

extern void wlc_bss_up(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg);

extern int wlc_ap_sendauth(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
		struct scb *scb, int auth_alg, int auth_seq, int status,
		uint8 *challenge_text, bool short_preamble, bool send_auth);
#else /* AP */

/* Stub functions help eliminate using #ifdef AP macros */
#define wlc_ap_attach(a) (wlc_ap_info_t *)(uintptr)0xdeadc0de
#define wlc_ap_detach(a) do {} while (0)
#define wlc_ap_up(a, b) do {} while (0)
#define wlc_ap_down(a, b) 0
#define wlc_ap_mbss4_tbtt(a, b) 0
#define wlc_ap_mbss16_tbtt(a, b) 0
#define wlc_ap_mbss16_write_prbrsp(a, b, c) 0
#define wlc_mbss16_updssid(a, b) do {} while (0)
#define wlc_restart_ap(a) do {} while (0)
#define wlc_ap_authresp(a, b, c, d, e, f, g) do {} while (0)
#define wlc_wme_setup_req(a, b, c, d) do {} while (0)
#define wlc_wme_initparams_ap(a, b) do {} while (0)

#define wlc_eapol_event(a, b, c, d) do {} while (0)
#define wlc_ap_process_assocreq(a, b, c, d, e, f, g) do {} while (0)
#define wlc_roam_check(a, b, c, d) FALSE
#define wlc_ht_ap_coex_tea_chk(a, b) 0
#define wlc_ht_ap_coex_tebc_proc(a) do {} while (0)
#define wlc_ht_ap_coex_ted_chk(a, b, c) 0
#define wlc_ht_coex_update_fid_time(a) do {} while (0)
#define wlc_ht_coex_update_permit(a, b) do {} while (0)
#define wlc_ht_coex_switch_bw(a, b, c) do {} while (0)

#define wlc_ap_stas_associated(ap) 0
#define wlc_ap_scb_cleanup(a) do {} while (0)

#define wlc_bss_up(ap, cfg, bcn, len) do {} while (0)
#define wlc_apsta_on_radar_channel(a)     FALSE

#endif /* AP */

#ifdef MBSS
extern bool wlc_prq_process(wlc_info_t *wlc, bool bounded);
#else
#define wlc_prq_process(wlc, bounded) FALSE;
#endif // endif

#if defined(AP) && defined(STA)
extern void wlc_ap_sta_onradar_upd(wlc_bsscfg_t *cfg);
#else
#define wlc_ap_sta_onradar_upd(c)	do {} while (0)
#endif /* AP && STA */

extern ratespec_t wlc_force_bcn_rspec(wlc_info_t *wlc);
#ifdef USBAP
extern bool wlc_wlancoex_on(wlc_info_t *wlc);
#else
#define wlc_wlancoex_on(c) FALSE
#endif /* USBAP */
#ifdef WL_GLOBAL_RCLASS
bool wlc_ap_scb_support_global_rclass(wlc_info_t *wlc, struct scb *scb);
#endif /* WL_GLOBAL_RCLASS */
#endif /* _WLC_AP_H_ */
