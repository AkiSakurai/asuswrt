/*
 * Exposed interfaces of wlc_sup.c
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: wlc_sup.h 487487 2014-06-26 01:51:34Z $
 */


#ifndef _wlc_sup_h_
#define _wlc_sup_h_

#ifdef BCMCCX
#include <proto/802.11_ccx.h>
#endif /* BCMCCX */

#ifdef BCMSUP_PSK
#include <bcmwpa.h>
#endif
#include <wlc_wpa.h>
#include <wlc_pmkid.h>

#ifdef WLOFFLD
#include <bcm_ol_msg.h>
#endif

typedef struct {
	int cfgh;			/* bsscfg cubby handle */
} wlc_sup_pub_t;

typedef struct {
	/* items common to any supplicant */
	int sup_type;			/* supplicant discriminator */
	bool		sup_enable_wpa;	/* supplicant WPA on/off */
} sup_bss_pub_t;


#define WLC_SUP_INFO_CFGH(idsup) (((wlc_sup_pub_t *)(idsup))->cfgh)

#define BSS_SUP_INFO(idsup, cfg) (*(sup_bss_pub_t **)BSSCFG_CUBBY(cfg, WLC_SUP_INFO_CFGH(idsup)))
#define BSS_SUP_TYPE(idsup, cfg) (BSS_SUP_INFO(idsup, cfg) ? \
							BSS_SUP_INFO(idsup, cfg)->sup_type : 0)
#define BSS_SUP_ENAB_WPA(idsup, cfg) (BSS_SUP_INFO(idsup, cfg) ? \
						BSS_SUP_INFO(idsup, cfg)->sup_enable_wpa : 0)

extern wlc_sup_info_t * wlc_sup_attach(wlc_info_t *wlc);
extern void wlc_sup_detach(wlc_sup_info_t *sup_info);

/* Initiate supplicant private context */
extern int wlc_sup_init(void *ctx, wlc_bsscfg_t *cfg);

/* Down the supplicant, return the number of callbacks/timers pending */
extern int wlc_sup_down(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg);

#if defined(BCMCCX) || defined(BCMSUP_PSK) || defined(BCMAUTH_PSK)
/* Send received EAPOL to supplicant; Return whether packet was used
 * (might still want to send it up to other supplicant)
 */
extern bool wlc_sup_eapol(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg,
	eapol_header_t *eapol_hdr, bool encrypted);
#endif /* !defined(BCMCCX) && !defined(BCMSUP_PSK) && !defined(BCMAUTH_PSK) */


/* Values for type parameter of wlc_set_sup() */
#define SUP_UNUSED	0 /* Supplicant unused */
#ifdef	BCMCCX
#define SUP_LEAP	1 /* Used for Leap */
#endif /* BCMCCX */
#if defined(BCMSUP_PSK)
#define SUP_WPAPSK	2 /* Used for WPA-PSK */
#ifdef	BCMCCX
#define SUP_LEAP_WPA	3 /* Used for LEAP-WPA */
#endif /* BCMCCX */
#endif /* BCMSUP_PSK */

extern bool wlc_set_sup(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg, int type,
	/* parameters used only for PSK follow */
	uint8 *sup_ies, uint sup_ies_len, uint8 *auth_ies, uint auth_ies_len);
extern void wlc_sup_set_ea(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, struct ether_addr *ea);
/* helper fn to find supplicant and authenticator ies from assocreq and prbresp */
extern void wlc_find_sup_auth_ies(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg,
	uint8 **sup_ies, uint *sup_ies_len, uint8 **auth_ies, uint *auth_ies_len);

extern unsigned char wlc_sup_geteaphdrver(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg);

#if defined(BCMSUP_PSK) || defined(WLFBT)
extern int wlc_set_pmk(wlc_info_t *wlc, wpapsk_info_t *info, wpapsk_t *wpa,
	struct wlc_bsscfg *cfg, wsec_pmk_t *pmk, bool assoc);
#endif /* defined(BCMSUP_PSK) || defined(WLFBT) */

#ifdef	BCMSUP_PSK
/* Install WPA PSK material in supplicant */
extern int wlc_sup_set_pmk(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg,
	wsec_pmk_t *psk, bool assoc);

/* Send SSID to supplicant for PMK computation */
extern int wlc_sup_set_ssid(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg,
	uchar ssid[], int ssid_len);

/* tell supplicant to send a MIC failure report */
extern bool wlc_sup_send_micfailure(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg,
	bool ismulti);
#endif	/* BCMSUP_PSK */

#ifdef BCMSUP_PSK
/* Manage supplicant 4-way handshake timer */
extern void wlc_sup_wpa_psk_timer(wlc_sup_info_t *sup_info, wlc_bsscfg_t *sup, bool start);
extern bool	wlc_wpa_sup_sendeapol(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg,
	uint16 flags, wpa_msg_t msg);
#endif /* BCMSUP_PSK */


#if defined(BCMCCX) || defined(BCMSUP_PSK)
/* Send a supplicant status event */
extern void wlc_wpa_send_sup_status(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg,
	uint reason);
#else
#define wlc_wpa_send_sup_status(sup, reason) do {} while (0)
#endif /* defined(BCMCCX) || defined (BCMSUP_PSK) */

#if defined(WOWL) && defined(BCMSUP_PSK)
extern uint16 aes_invsbox[];
extern uint16 aes_xtime9dbe[];
extern void *wlc_sup_hw_wowl_init(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg);
extern void wlc_sup_sw_wowl_update(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg);
#else
#define wlc_sup_hw_wowl_init(a) NULL
#define wlc_sup_sw_wowl_update(a) do { } while (0)
#endif /* defined(WOWL) && defined (BCMSUP_PSK) */

#ifdef WLWNM
extern void wlc_wpa_sup_gtk_update(wlc_sup_info_t *sup, wlc_bsscfg_t *cfg,
	int index, int key_len, uint8 *key, uint8 *rsc);
#endif

extern bool
wlc_sup_find_pmkid(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg,
struct ether_addr *bssid, uint8	*pmkid);

extern void
wlc_sup_clear_pmkid_store(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg);


/* sup init event data. */
typedef struct sup_init_event_data
{
	/* BSSCFG instance data. */
	wlc_bsscfg_t	*bsscfg;
	wpapsk_t *wpa;
	wpapsk_info_t *wpa_info;
	bool	up;
} sup_init_event_data_t;

typedef void (*sup_init_fn_t)(void *ctx, sup_init_event_data_t *evt);
extern int BCMATTACHFN(wlc_sup_up_down_register)(struct wlc_info *wlc, sup_init_fn_t callback,
	void *arg);

extern int BCMATTACHFN(wlc_sup_up_down_unregister)(struct wlc_info *wlc, sup_init_fn_t callback,
	void *arg);

#if defined(BCMSUP_PSK) && defined(WLFBT)
extern void wlc_sup_clear_replay(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg);
#endif

extern int wlc_sup_set_pmkid(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, uint8 *pmk,
	ushort pmk_len, struct ether_addr *auth_ea, uint8 *pmkid_out);

#ifdef WLOFFLD
extern bool wlc_wpa_sup_get_rekey_info(struct wlc_sup_info *sup_info,
	wlc_bsscfg_t *cfg, rsn_rekey_params * rsnkey);
extern bool wlc_wpa_sup_set_rekey_info(struct wlc_sup_info *sup_info,
	wlc_bsscfg_t *cfg, rsn_rekey_params * rsnkey);
#endif

#endif	/* _wlc_sup_h_ */
