/*
 * Exposed interfaces of wlc_sup.c
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
 * $Id: wlc_sup.h,v 1.3 2010-03-08 22:49:25 $
 */

#ifndef _wlc_sup_h_
#define _wlc_sup_h_

#if defined(BCMCCX) || defined(BCMSUP_LEAP)
#include <proto/802.11_ccx.h>
#endif /* BCMCCX */

/* Initiate supplicant private context */
extern void *wlc_sup_attach(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

/* Remove supplicant private context */
extern void wlc_sup_detach(struct supplicant *sup);

/* Down the supplicant, return the number of callbacks/timers pending */
extern int wlc_sup_down(struct supplicant *sup);

/* Send received EAPOL to supplicant; Return whether packet was used
 * (might still want to send it up to other supplicant)
 */
extern bool wlc_sup_eapol(struct supplicant *sup, eapol_header_t *eapol_hdr,
	bool encrypted, bool *auth_pending);

/* Values for type parameter of wlc_set_sup() */
#define SUP_UNUSED	0 /* Supplicant unused */
#if defined(BCMCCX) || defined(BCMSUP_LEAP)
#define SUP_LEAP	1 /* Used for Leap */
#endif /* BCMCCX || BCMSUP_LEAP */
#ifdef	BCMSUP_PSK
#define SUP_WPAPSK	2 /* Used for WPA-PSK */
#if defined(BCMCCX) || defined(BCMSUP_LEAP)
#define SUP_LEAP_WPA	3 /* Used for LEAP-WPA */
#endif /* BCMCCX || BCMSUP_LEAP */
#endif /* BCMSUP_PSK */

extern bool wlc_set_sup(struct supplicant *sup, int type,
	/* parameters used only for PSK follow */
	uint8 *sup_ies, uint sup_ies_len, uint8 *auth_ies, uint auth_ies_len,
	int btamp_enabled);
extern void wlc_sup_set_ea(supplicant_t *sup, struct ether_addr *ea);
/* helper fn to find supplicant and authenticator ies from assocreq and prbresp */
extern void wlc_find_sup_auth_ies(wlc_info_t *wlc, uint8 **sup_ies,
	uint *sup_ies_len, uint8 **auth_ies, uint *auth_ies_len);

#ifdef	BCMSUP_PSK
#ifdef	BCMWPA2
extern void wlc_sup_clear_pmkid_store(struct supplicant *sup);
extern void wlc_sup_pmkid_cache_req(struct supplicant *sup);
#endif /* BCMWPA2 */

/* Install WPA PSK material in supplicant */
extern int wlc_sup_set_pmk(struct supplicant *sup, wsec_pmk_t *psk, bool assoc);

/* Send SSID to supplicant for PMK computation */
extern int wlc_sup_set_ssid(struct supplicant *sup, uchar ssid[], int ssid_len);

/* send deauthentication */
extern void wlc_wpa_senddeauth(wlc_bsscfg_t *bsscfg, char *da, int reason);

/* tell supplicant to send a MIC failure report */
extern void wlc_sup_send_micfailure(struct supplicant *sup, bool ismulti);
#endif	/* BCMSUP_PSK */

#if defined(BCMSUP_LEAP) && !defined(BCMCCX)
extern int wlc_set_leapauth(struct supplicant*sup, int auth);
#endif // endif

#if defined(BCMSUP_LEAP) || defined(BCMCCX)
extern void wlc_sup_leap_starttime(struct supplicant *sup, int leap_start);
extern int wlc_sup_get_leap_starttime(struct supplicant* sup);
extern void wlc_set_leap_state(struct supplicant *sup, bool state);
extern bool wlc_get_leap_state(struct supplicant *sup);
#endif /* BCMSUP_LEAP || BCMCCX */

#if	defined(BCMCCX) || defined(BCMEXTCCX) || defined(BCMSUP_LEAP)
/* Return whether the given SSID matches on in the LEAP list. */
extern bool wlc_ccx_leap_ssid(struct supplicant *sup, uchar SSID[], int len);

/* Handle WLC_[GS]ET_LEAP_LIST ioctls */
extern int wlc_ccx_set_leap_list(struct supplicant *sup, void *pval);
extern int wlc_ccx_get_leap_list(struct supplicant *sup, void *pval);

/* Time-out  LEAP authentication and presume the AP is a rogue */
extern void wlc_ccx_rogue_timer(struct supplicant *sup, struct ether_addr *ap_mac);

/* Register a rogue AP report */
extern void wlc_ccx_rogueap_update(struct supplicant *sup, uint16 reason,
	struct ether_addr *ap_mac);

/* Return whether the supplicant state indicates successful authentication */
extern bool wlc_ccx_authenticated(struct supplicant *sup);

/* leap supplicant retry timer callback */
extern void wlc_leapsup_timer_callback(struct supplicant *sup);

extern void wlc_ccx_sup_init(struct supplicant *sup, int sup_type);
#endif /* BCMCCX || BCMEXTCCX  || BCMSUP_LEAP */

#if defined(BCMCCX) || defined(BCMEXTCCX)
#if defined(BCMSUP_PSK) || defined(BCMEXTSUP)
/* Compute reassoc MIC */
extern void wlc_cckm_calc_reassocreq_MIC(cckm_reassoc_req_ie_t *cckmie,
	struct ether_addr *bssid, wpa_ie_fixed_t *rsnie, struct ether_addr *cur_ea,
	uint32 rn, uint8 *key_refresh_key, uint16 WPA_auth);

/* Populate the CCKM reassoc req IE */
extern void wlc_cckm_gen_reassocreq_IE(struct supplicant *sup, cckm_reassoc_req_ie_t *cckmie,
	uint32 tsf_h, uint32 tsf_l, struct ether_addr *bssid, wpa_ie_fixed_t *rsnie);

/* Check for, validate, and process the CCKM reassoc resp IE */
extern bool wlc_cckm_reassoc_resp(struct supplicant *sup);
#endif /* BCMSUP_PSK || BCMEXTSUP */
#endif	/* BCMCCX || BCMEXTCCX */

#ifdef BCMSUP_PSK
/* Manage supplicant 4-way handshake timer */
extern uint32 wlc_sup_get_wpa_psk_tmo(struct supplicant *sup);
extern void wlc_sup_set_wpa_psk_tmo(struct supplicant *sup, uint32 tmo);
extern void wlc_sup_wpa_psk_timer(struct supplicant *sup, bool start);
#endif /* BCMSUP_PSK */

#if defined(BCMCCX) || defined(BCMSUP_PSK) || defined(BCMSUP_LEAP)
/* Return the supplicant authentication status */
extern sup_auth_status_t wlc_sup_get_auth_status(struct supplicant *sup);

/* Return the extended supplicant authentication status */
extern sup_auth_status_t wlc_sup_get_auth_status_extended(struct supplicant *sup);

/* Send a supplicant status event */
extern void wlc_wpa_send_sup_status(struct supplicant *sup, uint reason);
#else
#define wlc_wpa_send_sup_status(sup, reason) { }
#endif /* defined(BCMCCX) || defined (BCMSUP_PSK) */

#if defined BCMEXTCCX
/* For external supplicant */
extern void
wlc_cckm_set_assoc_resp(supplicant_t *sup, uint8 *assoc_resp, int len);
extern void
wlc_cckm_set_rn(supplicant_t *sup, int rn);
#endif // endif

#if defined(WOWL) && defined(BCMSUP_PSK)
extern uint16 aes_invsbox[];
extern uint16 aes_xtime9dbe[];
extern void *wlc_sup_hw_wowl_init(struct supplicant *sup);
extern void wlc_sup_sw_wowl_update(struct supplicant *sup);
#else
#define wlc_sup_hw_wowl_init(a) NULL
#define wlc_sup_sw_wowl_update(a) do { } while (0)
#endif /* defined(WOWL) && defined (BCMSUP_PSK) */

#endif	/* _wlc_sup_h_ */
