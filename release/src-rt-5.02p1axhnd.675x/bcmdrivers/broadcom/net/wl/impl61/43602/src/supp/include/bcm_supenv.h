/*
 * bcm_supenv.h -- Minimal data structures to support wlc_sup.c in user space
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
 * $Id: bcm_supenv.h,v 1.9 2010-03-08 22:49:25 $
 */

#ifndef _bcm_supenv_h
#define _bcm_supenv_h

#include <proto/ethernet.h>
#include <proto/802.11.h>

#if defined(BCM_OSL)
#include "osl.h"
#else
#include <bcm_osl.h>
#include <bcm_lbuf.h>
#endif // endif

#include <bcmutils.h>

typedef struct wsec_iv {
	uint32		hi;	/* upper 32 bits of IV */
	uint16		lo;	/* lower 16 bits of IV */
} wsec_iv_t;

#include <wlioctl.h>

#if defined(BCM_OSL)
#define TXOFF 170
#endif // endif

typedef struct wlc_bss_info
{
	struct ether_addr BSSID;	/* network BSSID */
	uint8		SSID_len;	/* the length of SSID */
	uint8		SSID[32];	/* SSID string */
	struct dot11_bcn_prb *bcn_prb;	/* beacon/probe response frame (ioctl na) */
	uint16		bcn_prb_len;	/* beacon/probe response frame length (ioctl na) */

} wlc_bss_info_t;

struct wlc_pub {
	uint unit;
	wlc_bss_info_t	current_bss;	/* STA BSS if active, else first AP BSS */
	osl_t		*osh;			/* pointer to os handle */
#ifdef WLCNT
	wl_cnt_t	_cnt;			/* monolithic counters struct */
	wl_wme_cnt_t	_wme_cnt;		/* Counters for WMM */
#endif /* WLCNT */
#ifdef BCMWPA2
	pmkid_cand_t	pmkid_cand[MAXPMKID];	/* PMKID candidate list */
	uint		npmkid_cand;	/* num PMKID candidates */
	pmkid_t		pmkid[MAXPMKID];	/* PMKID cache */
	uint		npmkid;			/* num cached PMKIDs */
#endif /* BCMWPA2 */

};
typedef struct wlc_pub wlc_pub_t;

#ifdef WLCNT
#define WLCNTINCR(a) ((a)++)
#else
#define WLCNTINCR(a)
#endif /* WLCNT */

/* Lots of fakery to avoid source code changes! */

#define BCM_SUPFRMBUFSZ		2048
#define WLC_MAXBSSCFG		4

struct wlc_bsscfg {
	struct wlc_info	*wlc;		/* wlc to which this bsscfg belongs. */
	bool		inuse;	/* is this config in use */
	bool		up;		/* is this configuration up */
	bool		enable;		/* is this configuration enabled */
	bool		_ap;		/* is this configuration an AP */

	void		*sup;		/* pointer to supplicant state */
	int		sup_type;	/* type of supplicant */
	void		*authenticator;	/* pointer to authenticator state */
	uint8		SSID_len;	/* the length of SSID */
	uint8		SSID[DOT11_MAX_SSID_LEN];	/* SSID string */

	struct ether_addr   BSSID;      /* BSSID */
	struct ether_addr   cur_etheraddr;  /* BSSID */
	uint16		WPA_auth;	/* WPA: authenticated key management */

	wsec_iv_t	wpa_none_txiv;	/* global txiv for WPA_NONE, tkip and aes */
	bool have_keys;		/* keys installed */
	bool auth_pending;
	ctx_t  ctx;	/* holds context pointers */

	uint8 sup_ies[256];		/* assoc req ie */
	uint8	sup_ies_len;
	uint8 auth_ies[256];	/* probe resp ie */
	uint8 auth_ies_len;
	struct dot11_assoc_resp *assoc_resp;	/* last (re)association response */
	uint		assoc_resp_len;
	int			btamp_enabled;

};

typedef struct wlc_bsscfg wlc_bsscfg_t;
struct wlc_info {

	wlc_pub_t	pub;			/* wlc public state (must be first field of wlc) */
	void		*wl;			/* pointer to os-specific private state */
	bool        sta_associated; /* true if STA bsscfg associated to AP */
	/* BSS Configurations */
	wlc_bsscfg_t	bsscfg[WLC_MAXBSSCFG];	/* set of BSS configurations */

	wlc_bsscfg_t cfg;			/* the primary bsscfg (can be AP or STA) */
	int sup_wpa2_eapver;
	bool sup_m3sec_ok;
	struct dot11_assoc_resp *assoc_resp; /* last (re)association response */
	uint					 assoc_resp_len;

};

typedef struct wlc_info wlc_info_t;

typedef struct supplicant supplicant_t;

/* Absolutely minimalist pktbuffer utilities */

extern void bcm_supenv_init(void);

extern void
wlc_mac_event(wlc_info_t* wlc, uint msg, const struct ether_addr* addr,
	uint result, uint status, uint auth_type, void *data, int datalen);

extern void
wlc_getrand(wlc_info_t *wlc, uint8 *buf, int buflen);

extern char *
bcm_ether_ntoa(const struct ether_addr *ea, char *buf);

/* macros to help with different environments */
#define SEND_PKT(bsscfg, p) wpaif_sendpkt((bsscfg), (p))

#define PLUMB_TK(a, b)	wpaif_plumb_ptk((a), (b))

#define PLUMB_GTK(pkey, bsscfg) \
	wpaif_plumb_gtk((pkey), (bsscfg))

#define AUTHORIZE(bsscfg) \
				wpaif_set_authorize(bsscfg)
#define PUSH_KRK_TO_WLDRIVER(bsscfg, krk, krk_len)	\
				wpaif_set_krk(bsscfg, krk, krk_len)

#define DEAUTHENTICATE(bsscfg, reason) \
				wpaif_set_deauth(bsscfg, reason);

extern int
wpaif_plumb_ptk(wl_wsec_key_t *key, wlc_bsscfg_t *bsscfg);
extern int
wpaif_plumb_gtk(wl_wsec_key_t *key, wlc_bsscfg_t *bsscfg);
extern int
wpaif_set_authorize(wlc_bsscfg_t *bsscfg);
extern int
wpaif_set_deauth(wlc_bsscfg_t *bsscfg, int reason);

extern int
wpaif_sendpkt(wlc_bsscfg_t *bsscfg, void *p);
extern int
wpaif_set_krk(wlc_bsscfg_t *bsscfg, uint8 *krk, int krk_len);

/* supplicant status callback */
void wpaif_forward_mac_event_cb(wlc_bsscfg_t *bsscfg, uint reason, uint status);

#endif /* _bcm_supenv_h */
