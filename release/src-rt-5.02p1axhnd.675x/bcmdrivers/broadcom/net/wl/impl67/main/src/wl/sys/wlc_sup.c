/*
 * wlc_sup.c -- driver-resident supplicants.
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
 * $Id: wlc_sup.c 783430 2020-01-28 14:19:14Z $
 */

/**
 * @file
 * @brief
 * Internal WPA supplicant
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlanSwArchitectureIdsup]
 */

#ifdef BCMINTSUP
#include <wlc_cfg.h>
#endif /* BCMINTSUP */

#ifndef	STA
#error "STA must be defined for wlc_sup.c"
#endif /* STA */
#if !defined(BCMSUP_PSK) && !defined(WLFBT)
#error "BCMSUP_PSK and/or WLFBT must be defined"
#endif /* !defined(BCMSUP_PSK) && !defined(WLFBT) */

#ifdef BCMINTSUP
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <eap.h>
#include <eapol.h>
#include <bcmwpa.h>
#if defined(BCMSUP_PSK) || defined(WLFBT)
#endif /* BCMSUP_PSK || WLFBT */
#ifdef	BCMSUP_PSK
#include <passhash.h>
#endif /* BCMSUP_PSK */
#include <802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_led.h>
#include <wlc_rm.h>
#include <wlc_assoc.h>
#include <wl_export.h>
#include <wlc_scb.h>
#if defined(BCMSUP_PSK) || defined(WLFBT)
#include <wlc_wpa.h>
#endif /* BCMSUP_PSK || WLFBT */
#include <wlc_sup.h>
#include <wlc_sup_priv.h>
#include <wlc_pmkid.h>
#ifdef WOWL
#include <wlc_wowl.h>
#include <aes.h>
#endif // endif

#else /* external supplicant */

#include <stdio.h>
#include <typedefs.h>
#include <wlioctl.h>
#include <eapol.h>
#include <eap.h>
#include <bcmwpa.h>
#include <sup_dbg.h>
#include <bcmutils.h>
#include <string.h>
#include <bcmendian.h>
#include <eapol.h>
#include <bcm_osl.h>
#include "bcm_supenv.h"
#include "wpaif.h"
#include "wlc_sup.h"
#include "wlc_wpa.h"
#endif /* BCMINTSUP */
#if defined(WLFBT)
#include "wlc_fbt.h"
#endif // endif
#ifdef WL_FTM
#include <wlc_ftm.h>
#endif /* WL_FTM */

#include <802.11.h>
#include <wlc_keymgmt.h>

#ifdef MFP
#include <wlc_mfp.h>
#define SUPMFP(s) ((s)->wlc->mfp)
#endif // endif

#include <wlc_event_utils.h>
#include <wlc_iocv_reg.h>
#include <wlc_iocv.h>

#if defined(BCMSUP_PSK)
#define SUP_CHECK_MCIPHER(sup) ((sup->wpa->mcipher != cipher) &&	\
	bcmwpa_is_wpa_auth(sup->wpa->WPA_auth))
#define SUP_CHECK_EAPOL(body) (body->type == EAPOL_WPA_KEY || body->type == EAPOL_WPA2_KEY)

#define SUP_CHECK_WPAPSK_SUP_TYPE(sup) (sup->sup_type == SUP_WPAPSK)

#define WL_SUP_INFO(args)	WL_WSEC(args)
#define WL_SUP_ERROR(args)	WL_WSEC(args)

static int wlc_sup_doiovar(void *handle, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif);

enum {
	IOV_SUP_AUTH_STATUS,
	IOV_SUP_AUTH_STATUS_EXT,	/* Extended supplicant authentication status */
	IOV_SUP_M3SEC_OK,
	IOV_SUP_WPA,
	IOV_SUP_WPA2_EAPVER,
	IOV_SUP_WPA_TMO,
	IOV_LAST
};

static const bcm_iovar_t sup_iovars[] = {
	{"sup_auth_status", IOV_SUP_AUTH_STATUS,
	(0), 0, IOVT_UINT32, 0
	},
	{"sup_auth_status_ext", IOV_SUP_AUTH_STATUS_EXT,
	(0), 0, IOVT_UINT32, 0
	},
	{"sup_m3sec_ok", IOV_SUP_M3SEC_OK,
	(0), 0, IOVT_BOOL, 0
	},
	{"sup_wpa", IOV_SUP_WPA,
	(0), 0, IOVT_BOOL, 0
	},
	{"sup_wpa2_eapver", IOV_SUP_WPA2_EAPVER,
	(0), 0, IOVT_BOOL, 0
	},
	{"sup_wpa_tmo", IOV_SUP_WPA_TMO,
	(0), 0, IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0, 0}
};

typedef struct {
	supplicant_t *sup;
} wlc_sup_copy_t;

static void wlc_sup_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt);

static bool
wlc_sup_retrieve_pmk(supplicant_t *sup, wlc_bsscfg_t *cfg, uint8 *data,
	struct ether_addr *bssid, uint8 *pmk, uint8 pmklen);

static uint32 wlc_sup_get_wpa_psk_tmo(struct supplicant *sup);
static void wlc_sup_set_wpa_psk_tmo(struct supplicant *sup, uint32 tmo);
/* Return the supplicant authentication status */
static sup_auth_status_t wlc_sup_get_auth_status(struct supplicant *sup);

/* Return the extended supplicant authentication status */
static sup_auth_status_t wlc_sup_get_auth_status_extended(struct supplicant *sup);

/* Initiate supplicant private context */
static int wlc_sup_init(void *ctx, wlc_bsscfg_t *cfg);

/** Manage supplicant 4-way handshake timer */
static void wlc_sup_wpa_psk_timer(wlc_sup_info_t *sup_info, wlc_bsscfg_t *sup, bool start);

/* Remove supplicant private context */
static void wlc_sup_deinit(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_sup_handle_joinproc(wlc_sup_info_t *sup_info, bss_assoc_state_data_t *evt_data);
static void wlc_sup_handle_joinstart(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg);
static void wlc_sup_handle_joindone(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg);

#ifdef BCMSUP_PSK
static void wlc_wpa_psk_timer(void *arg);
static void *wlc_wpa_sup_prepeapol(supplicant_t *sup, uint16 flags, wpa_msg_t msg);
#endif // endif

/**
 * Allocate supplicant context, squirrel away the passed values,
 * and return the context handle.
 */
wlc_sup_info_t *
BCMATTACHFN(wlc_sup_attach)(wlc_info_t *wlc)
{
	wlc_sup_info_t *sup_info;
	bsscfg_cubby_params_t cubby_params;

	WL_TRACE(("wl%d: wlc_sup_attach\n", wlc->pub->unit));

	if (!(sup_info = (wlc_sup_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_sup_info_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	sup_info->wlc = wlc;
	sup_info->pub = wlc->pub;
	sup_info->wl = wlc->wl;
	sup_info->osh = wlc->osh;

	/* reserve cubby space in the bsscfg container for per-bsscfg private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = sup_info;
	cubby_params.fn_deinit = wlc_sup_deinit;

	sup_info->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(supplicant_t *), &cubby_params);

	if (sup_info->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          UNIT(sup_info), __FUNCTION__));
		goto err;
	}

	/* bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_sup_bss_updn, sup_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
		          UNIT(sup_info), __FUNCTION__));
		goto err;
	}

	/* register assoc state notification callback */
	if (wlc_bss_assoc_state_register(wlc, (bss_assoc_state_fn_t)wlc_sup_handle_joinproc,
		sup_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	/* notifications done from idsup module */
	if (bcm_notif_create_list(wlc->notif, &sup_info->sup_up_down_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list sup_up_down_notif_hdl\n",
			wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	/* XXX Supplicant hangs off the bsscfg, rather than off wlc, cannot allocate/deallocate
	 * anything since the creation/deletion has to happen during bsscfg init/deinit
	 */

	/* register module */
	if (wlc_module_register(wlc->pub, sup_iovars, "idsup", sup_info, wlc_sup_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: auth wlc_module_register() failed\n", UNIT(sup_info)));
		goto err;
	}

	return sup_info;
err:
	MODULE_DETACH(sup_info, wlc_sup_detach);
	return NULL;
}

/** Toss supplicant context */
void
BCMATTACHFN(wlc_sup_detach)(wlc_sup_info_t *sup_info)
{
	if (!sup_info)
		return;

	WL_TRACE(("wl%d: wlc_sup_detach\n", UNIT(sup_info)));

	/* assoc join-start/done callback */
	(void)wlc_bss_assoc_state_unregister(sup_info->wlc,
		(bss_assoc_state_fn_t)wlc_sup_handle_joinproc, sup_info);

	/* Delete event notification list for join start/done events. */
	if (sup_info->sup_up_down_notif_hdl != NULL)
		bcm_notif_delete_list(&sup_info->sup_up_down_notif_hdl);

	(void)wlc_bsscfg_updown_unregister(sup_info->wlc, wlc_sup_bss_updn, sup_info);

	wlc_module_unregister(sup_info->pub, "idsup", sup_info);
	MFREE(sup_info->osh, sup_info, sizeof(wlc_sup_info_t));
}

static void
wlc_sup_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_sup_info_t *sup_info = (wlc_sup_info_t *)ctx;

	if (!evt->up) {
		wlc_sup_down(sup_info, evt->bsscfg);
	}
}

static int
wlc_sup_doiovar(void *handle, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_sup_info_t *sup_info = (wlc_sup_info_t *)handle;
	supplicant_t *sup;
	wlc_info_t *wlc = sup_info->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val, unhandled = FALSE;

	BCM_REFERENCE(vsize);
	BCM_REFERENCE(alen);

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (BSSCFG_AP(bsscfg)) {
		err = BCME_NOTSTA;
		goto exit;
	}
	sup = SUP_BSSCFG_CUBBY(sup_info, bsscfg);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {
		case IOV_GVAL(IOV_SUP_M3SEC_OK):
			*ret_int_ptr = (int32)sup_info->sup_m3sec_ok;
			break;
		case IOV_SVAL(IOV_SUP_M3SEC_OK):
			sup_info->sup_m3sec_ok = bool_val;
			break;
		case IOV_GVAL(IOV_SUP_WPA2_EAPVER):
			*ret_int_ptr = (int32)sup_info->sup_wpa2_eapver;
			break;
		case IOV_SVAL(IOV_SUP_WPA2_EAPVER):
			sup_info->sup_wpa2_eapver = int_val;
			break;
		case IOV_GVAL(IOV_SUP_WPA):
			if (sup)
				*ret_int_ptr = (int32)sup->sup_enable_wpa;
			else
				*ret_int_ptr = 0;
			break;
		case IOV_SVAL(IOV_SUP_WPA):
			if (bool_val) {
				if (!sup)
					err = wlc_sup_init(sup_info, bsscfg);
			}
			else {
				if (sup) {
					wlc_ioctl(wlc, WLC_DISASSOC, NULL, 0, wlcif);
					wlc_sup_deinit(sup_info, bsscfg);
				}
			}
			break;
		default:
			unhandled  = TRUE;
			break;
	}

	if (unhandled) {
		if (!sup) {
			err = BCME_NOTREADY;
			return err;
		}
		switch (actionid) {
			case IOV_GVAL(IOV_SUP_AUTH_STATUS):
				*((uint32 *)arg) = wlc_sup_get_auth_status(sup);
				break;
			case IOV_GVAL(IOV_SUP_AUTH_STATUS_EXT):
				*((uint32 *)arg) = wlc_sup_get_auth_status_extended(sup);
				break;
#ifdef BCMSUP_PSK
			case IOV_GVAL(IOV_SUP_WPA_TMO):
				*ret_int_ptr = (int32)wlc_sup_get_wpa_psk_tmo(sup);
				break;

			case IOV_SVAL(IOV_SUP_WPA_TMO):
				wlc_sup_set_wpa_psk_tmo(sup, (uint32) int_val);
				if (int_val == 0) {
					wlc_sup_wpa_psk_timer(sup_info, bsscfg, FALSE);
				}
				break;
#endif /* BCMSUP_PSK */
			default:
				err = BCME_UNSUPPORTED;
				break;
		}
	}
exit:	return err;
}
#endif /* defined(BCMSUP_PSK) */

#if defined(BCMINTSUP) || defined(WLFBT)
/** Look for AP's and STA's IE list in probe response and assoc req */
void
wlc_find_sup_auth_ies(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, uint8 **sup_ies,
	uint *sup_ies_len, uint8 **auth_ies, uint *auth_ies_len)
{
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *current_bss = cfg->current_bss;

	BCM_REFERENCE(sup_info);

	if ((current_bss->bcn_prb == NULL) ||
	    (current_bss->bcn_prb_len <= sizeof(struct dot11_bcn_prb))) {
		*auth_ies = NULL;
		*auth_ies_len = 0;
	} else {
		*auth_ies = (uint8 *)&current_bss->bcn_prb[1];
		*auth_ies_len = current_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);
	}

	if ((as->req == NULL) || as->req_len == 0) {
		*sup_ies = NULL;
		*sup_ies_len = 0;
	} else {

		*sup_ies = (uint8 *)&as->req[1];	/* position past hdr */
		*sup_ies_len = as->req_len;

		/* If this was a re-assoc, there's another ether addr to skip */
		if (as->req_is_reassoc) {
			*sup_ies_len -= ETHER_ADDR_LEN;
			*sup_ies += ETHER_ADDR_LEN;
		}
		*sup_ies_len -= sizeof(struct dot11_assoc_req);
	}
}
#endif /* BCMINTSUP || WLFBT */

#ifdef BCMSUP_PSK
/** Build and send an EAPOL WPA key message */
bool
wlc_wpa_sup_sendeapol(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, uint16 flags, wpa_msg_t msg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	void * p;
#ifdef BCMINTSUP
	wlc_info_t *wlc = sup_info->wlc;
#endif // endif

	p = wlc_wpa_sup_prepeapol(sup, flags, msg);

	if (p != NULL) {
#ifdef BCMINTSUP
		wlc_sendpkt(wlc, p, cfg->wlcif);
#else
		(void)SEND_PKT(cfg, p);
#endif // endif
		return TRUE;
	}
	return FALSE;
}

static void *
wlc_wpa_sup_prepeapol(supplicant_t *sup, uint16 flags, wpa_msg_t msg)
{
	uint16 len, key_desc, fbt_len = 0;
	void *p = NULL;
	eapol_header_t *eapol_hdr = NULL;
	eapol_wpa_key_header_t *wpa_key = NULL;
	osl_t *osh = OSH(sup);
	wpapsk_t *wpa = sup->wpa;

	BCM_REFERENCE(osh);

	len = EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN;
	switch (msg) {
	case PMSG2:		/* pair-wise msg 2 */
		if (wpa->sup_wpaie == NULL)
			break;
#ifdef WLFBT
		if (BSSCFG_IS_FBT(sup->cfg) && (wpa->WPA_auth & WPA2_AUTH_FT))
			fbt_len = wlc_fbt_getlen_eapol(sup->wlc->fbt, sup->cfg);
#endif /* WLFBT */

		len += wpa->sup_wpaie_len + fbt_len;
		if ((p = wlc_eapol_pktget(sup->wlc, sup->cfg, &PEER_EA(sup),
			len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(len - EAPOL_HEADER_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | PMSG2_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->tk_len, (uint8 *)&wpa_key->key_len);
		bcopy(wpa->snonce, wpa_key->nonce, EAPOL_WPA_KEY_NONCE_LEN);
		wpa_key->data_len = hton16(wpa->sup_wpaie_len + fbt_len);
		bcopy(wpa->sup_wpaie, wpa_key->data, wpa->sup_wpaie_len);
#ifdef WLFBT
		if (BSSCFG_IS_FBT(sup->cfg) && (wpa->WPA_auth & WPA2_AUTH_FT)) {
			wlc_fbt_addies(sup->wlc->fbt, sup->cfg, wpa_key);
		}
#endif /* WLFBT */
		WL_SUP_INFO(("wl%d: wlc_wpa_sup_sendeapol: sending message 2\n",
			UNIT(sup)));
		break;

	case PMSG4:		/* pair-wise msg 4 */
		if ((p = wlc_eapol_pktget(sup->wlc, sup->cfg, &PEER_EA(sup),
			len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | PMSG4_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->tk_len, (uint8 *)&wpa_key->key_len);
		WL_SUP_INFO(("wl%d: wlc_wpa_sup_sendeapol: sending message 4\n",
			UNIT(sup)));
		break;

	case GMSG2:	       /* group msg 2 */
		if ((p = wlc_eapol_pktget(sup->wlc, sup->cfg, &PEER_EA(sup),
			len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | GMSG2_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->gtk_len, (uint8 *)&wpa_key->key_len);
		break;

	case MIC_FAILURE:	/* MIC failure report */
		if ((p = wlc_eapol_pktget(sup->wlc, sup->cfg, &PEER_EA(sup),
			len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store(flags, (uint8 *)&wpa_key->key_info);
		break;

	default:
		WL_SUP_ERROR(("wl%d: wlc_wpa_sup_sendeapol: unexpected message type %d\n",
		         UNIT(sup), msg));
		break;
	}

	if (p != NULL) {
		/* do common message fields here; make and copy MIC last. */
		eapol_hdr->type = EAPOL_KEY;
		if (bcmwpa_is_rsn_auth(wpa->WPA_auth))
			wpa_key->type = EAPOL_WPA2_KEY;
		else
			wpa_key->type = EAPOL_WPA_KEY;
		bcopy(wpa->replay, wpa_key->replay, EAPOL_KEY_REPLAY_LEN);
		/* If my counter is one greater than the last one of his I
		 * used, then a ">=" test on receipt works AND the problem
		 * of zero at the beginning goes away.  Right?
		 */
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		key_desc = flags & (WPA_KEY_DESC_V1 | WPA_KEY_DESC_V2);
		if (!wpa_make_mic(eapol_hdr, key_desc, wpa->eapol_mic_key,
			wpa_key->mic)) {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_sendeapol: MIC generation failed\n",
			         UNIT(sup)));
			return FALSE;
		}
	}
	return p;
}

#if defined(BCMSUP_PSK)
void
wlc_wpa_send_sup_status(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, uint reason)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	uint status;

	if (!sup || sup->sup_type != SUP_WPAPSK)
		return;
	status = wlc_sup_get_auth_status_extended(sup);

	if (status != WLC_SUP_DISCONNECTED) {
#ifdef BCMINTSUP
		wlc_bss_mac_event(sup->wlc, cfg, WLC_E_PSK_SUP,
		                  NULL, status, reason, 0, 0, 0);
#else
		wpaif_forward_mac_event_cb(cfg, reason, status);
#endif /* BCMINTSUP */
	}
}
#endif /* defined (BCMSUP_PSK) */

#if defined(WLWNM)
void
wlc_wpa_sup_gtk_update(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg,
	int index, int key_len, uint8 *key, uint8 *rsc)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	wpapsk_t *wpa = sup->wpa;

	wpa->gtk_len = (ushort)key_len;
	wpa->gtk_index = (uint8)index;
	memcpy(wpa->gtk, key, key_len);

	WL_WNM(("+ WNM-Sleep GTK update id:%d\n", index));

	wlc_wpa_plumb_gtk(sup->wlc, cfg, wpa->gtk, wpa->gtk_len,
		index, wpa->mcipher, rsc, FALSE);
}
#endif /* WLWNM */

#if defined(BCMSUP_PSK) || defined(BCMAUTH_PSK)
/* Defined in 802.11 protocol */
/* Ver+GroupDataCipher+PairwiseCipherCount+PairwiseCipherList+AKMCount+AKMList+CAP */
#define RSN_PCCNT_OFFSET	(2+4)
#define RSN_AKMCNT_OFFSET(pc_cnt)	(RSN_PCCNT_OFFSET+2+4*(pc_cnt))
#define RSN_PMKID_OFFSET(pc_cnt, akm_cnt)	(RSN_AKMCNT_OFFSET(pc_cnt)+2+4*(akm_cnt)+2)

/* Return TRUE if check is successful */
static bool
wlc_wpa_sup_check_rsn(supplicant_t *sup, bcm_tlv_t *rsn)
{
	bool ret = FALSE;
	bool fbt = FALSE;
	uint16 pc_cnt = 0;
	uint16 akm_cnt = 0;
	bcm_tlv_t *wpaie = (bcm_tlv_t*)sup->wpa->auth_wpaie;
#ifdef WLFBT
	/* initial FBT has an extra pmkr1name in msg3 */
	if (BSSCFG_IS_FBT(sup->cfg) && (sup->wpa->WPA_auth & WPA2_AUTH_FT))
		fbt = TRUE;
#endif // endif
	if (rsn) {
		if (fbt) {
			if (wpaie->len + sizeof(wpa_pmkid_list_t) == rsn->len) {
				/* WPA IE ends at RSN CAP */
				ret = !memcmp(wpaie->data, rsn->data, wpaie->len);
			} else if (wpaie->len + (uint8)WPA2_PMKID_LEN >= rsn->len) {
				/* WPA IE has more optional fields */
				pc_cnt = ltoh16_ua(&rsn->data[RSN_PCCNT_OFFSET]);
				akm_cnt = ltoh16_ua(&rsn->data[RSN_AKMCNT_OFFSET(pc_cnt)]);
				ret = !memcmp(wpaie->data, rsn->data,
						RSN_PMKID_OFFSET(pc_cnt, akm_cnt));
			}
		} else {
			ret = (wpaie->len == rsn->len) &&
					!memcmp(wpaie->data, rsn->data, wpaie->len);
		}
	}
	return ret;
}

static bool
wlc_wpa_sup_eapol(supplicant_t *sup, eapol_header_t *eapol, bool encrypted)
{
	wlc_sup_info_t *sup_info = sup->m_handle;
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	uint16 key_info, key_len, data_len;
	uint16 cipher;
	uint16 prohibited, required;
	wpapsk_t *wpa = sup->wpa;

	key_info = ntoh16_ua(&body->key_info);

	WL_SUP_INFO(("wl%d: wlc_wpa_sup_eapol: received EAPOL_WPA_KEY packet, KI:%x\n",
		UNIT(sup), key_info));

	if ((key_info & WPA_KEY_PAIRWISE) && !(key_info & WPA_KEY_MIC)) {
		/* This is where cipher checks would be done for WDS.
		 * See what NAS' nsup does when that's needed.
		 */
	}

	/* check for replay */
	if (wpa_array_cmp(MAX_ARRAY, body->replay, wpa->replay, EAPOL_KEY_REPLAY_LEN) ==
	    wpa->replay) {
#if defined(BCMDBG) || defined(WLMSG_WSEC)
		uchar *g = body->replay, *s = wpa->replay;
		WL_WSEC(("wl%d: wlc_wpa_sup_eapol: ignoring replay ", UNIT(sup)));
		WL_WSEC(("(got %02x%02x%02x%02x%02x%02x%02x%02x",
				g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7]));
		WL_WSEC((" last saw %02x%02x%02x%02x%02x%02x%02x%02x)\n",
				s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]));
		WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: got replay %02x%02x",
			UNIT(sup), body->replay[6], body->replay[7]));
		WL_SUP_ERROR((" last saw %02x%02x", wpa->replay[6], wpa->replay[7]));
#endif /* BCMDBG || WLMSG_WSEC */

		return TRUE;
	}
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	{
		uchar *g = body->replay, *s = wpa->replay;
		WL_WSEC(("wl%d: wlc_wpa_sup_eapol: NO replay ", UNIT(sup)));
		WL_WSEC(("(got %02x%02x%02x%02x%02x%02x%02x%02x",
				g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7]));
		WL_WSEC((" last saw %02x%02x%02x%02x%02x%02x%02x%02x)\n",
				s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]));
	}
#endif /* BCMDBG || WLMSG_WSEC */

	/* check message MIC */
	if (key_info & WPA_KEY_MIC) {
		if (ntoh16(eapol->length) < OFFSETOF(eapol_wpa_key_header_t, data_len)) {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: missing MIC , discarding pkt\n",
				UNIT(sup)));
			return TRUE;
		}
		if (!wpa_check_mic(eapol, key_info & (WPA_KEY_DESC_V1|WPA_KEY_DESC_V2),
			wpa->eapol_mic_key)) {
			/* 802.11-2007 clause 8.5.3.3 - silently discard MIC failure */
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: MIC failure, discarding pkt\n",
				UNIT(sup)));
			return TRUE;
		}
	}

	/* check data length is okay */
	if (ntoh16(eapol->length) < OFFSETOF(eapol_wpa_key_header_t, data)) {
		WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: too short - no data len, toss pkt\n",
			UNIT(sup)));
		return TRUE;
	}

	data_len = ntoh16_ua(&body->data_len);
	if (ntoh16(eapol->length) < (OFFSETOF(eapol_wpa_key_header_t, data) + data_len)) {
		WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: not enough data - discarding pkt\n",
			UNIT(sup)));
		return TRUE;
	}

	/* if MIC was okay, save replay counter */
	/* last_replay is NOT incremented after transmitting a message */
	bcopy(body->replay, wpa->replay, EAPOL_KEY_REPLAY_LEN);
	bcopy(body->replay, wpa->last_replay, EAPOL_KEY_REPLAY_LEN);

	/* decrypt key data field */
	if ((data_len) && bcmwpa_is_rsn_auth(wpa->WPA_auth) &&
	    (key_info & WPA_KEY_ENCRYPTED_DATA)) {

		uint8 *data, *encrkey;
		rc4_ks_t *rc4key;
		bool decr_status;

		if (!(data = MALLOC(sup->osh, WPA_KEY_DATA_LEN_256))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}
		if (!(encrkey = MALLOC(sup->osh, WPA_MIC_KEY_LEN*2))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
			MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}
		if (!(rc4key = MALLOC(sup->osh, sizeof(rc4_ks_t)))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
			MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
			MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}

		decr_status = wpa_decr_key_data(body, key_info,
		                       wpa->eapol_encr_key, NULL, data, encrkey, rc4key);

		MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
		MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
		MFREE(sup->osh, rc4key, sizeof(rc4_ks_t));

		if (!decr_status) {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: decryption of key"
					"data failed\n", UNIT(sup)));
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}
	}
	else if ((key_info & WPA_KEY_ENCRYPTED_DATA) && (!data_len)) {
		WL_WSEC(("wl%d:wlc_wpa_sup_eapol: zero length encrypted data\n",
				UNIT(sup)));
	}
	key_len = ntoh16_ua(&body->key_len);
	cipher = CRYPTO_ALGO_OFF;

	if (bcmwpa_is_wpa_auth(wpa->WPA_auth) || (key_info & WPA_KEY_PAIRWISE)) {

		/* Infer cipher from message key_len.  Association shouldn't have
		 * succeeded without checking that the cipher is okay, so this is
		 * as good a way as any to find it here.
		 */
		switch (key_len) {
		case TKIP_KEY_SIZE:
			/* fall though */
		case AES_KEY_SIZE:
			{
				int err;
				err = wlc_keymgmt_get_cur_wsec_algo(sup->wlc->keymgmt,
					sup->cfg, key_len, (wlc_key_algo_t *)&cipher);
				if (err == BCME_OK) {
					break;
				} else {
					WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol:"
					" cannot deduce key algo from key size\n", UNIT(sup)));
					return FALSE;
				}
			}
		case WEP128_KEY_SIZE:
			if (!(key_info & WPA_KEY_PAIRWISE)) {
				cipher = CRYPTO_ALGO_WEP128;
						break;
			} else {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol:"
					" illegal use of ucast WEP128\n", UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_BAD_UCAST_WEP128);
				return FALSE;
			}
		case WEP1_KEY_SIZE:
			if (!(key_info & WPA_KEY_PAIRWISE)) {
				cipher = CRYPTO_ALGO_WEP1;
				break;
			} else {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol:"
					" illegal use of ucast WEP40\n", UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_BAD_UCAST_WEP40);
				return FALSE;
			}
		default:
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: unsupported key_len = %d\n",
			         UNIT(sup), key_len));
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_UNSUP_KEY_LEN);
			return FALSE;
		}
	}

	if (key_info & WPA_KEY_PAIRWISE) {
		if (wpa->ucipher != cipher) {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol:"
				" unicast cipher mismatch in pairwise key message\n", UNIT(sup)));
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_PW_KEY_CIPHER);
			return FALSE;
		}

		if (!(key_info & WPA_KEY_MIC)) {

			WL_SUP_INFO(("wl%d: wlc_wpa_sup_eapol: processing message 1\n",
			         UNIT(sup)));

			/* Test message 1 key_info flags */
			prohibited = encrypted ? (PMSG1_PROHIBITED & ~WPA_KEY_SECURE)
				: PMSG1_PROHIBITED;
			required = encrypted ? (PMSG1_REQUIRED & ~WPA_KEY_SECURE) : PMSG1_REQUIRED;
			if (((key_info & required) != required) || ((key_info & prohibited) != 0)) {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: unexpected key_info"
				         " (0x%04x) in WPA pairwise key message 1\n",
				         UNIT(sup), (uint)key_info));
				/*
				 * XXX - Should this return an error?
				 *       Msg3 check does...
				 */
			}
			/* If gtk present, this is forged M3 with MIC reset.
			 * Possible attack attempt, skip further processing
			 */
			if (NULL != wpa_find_gtk_encap(body->data, data_len)) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol:Forged packet, ignore \n",
					UNIT(sup)));
				return FALSE;
			}
			wpa->state = WPA_SUP_STAKEYSTARTP_PREP_M2;

			if ((wpa->WPA_auth & WPA2_AUTH_UNSPECIFIED) &&
#ifdef WLFBT
				(!BSSCFG_IS_FBT(sup->cfg) || !(wpa->WPA_auth & WPA2_AUTH_FT)) &&
#endif /* WLFBT */
				TRUE) {
				eapol_wpa2_encap_data_t *data_encap;

				/* extract PMKID */
				data_len = ntoh16_ua(&body->data_len);
				data_encap = wpa_find_kde(body->data, data_len,
				                          WPA2_KEY_DATA_SUBTYPE_PMKID);
				if (data_encap) {

#if defined(BCMDBG) || defined(WLMSG_WSEC)
					if (WL_WSEC_ON()) {
						uint8 *d = data_encap->data;
						WL_SUP_INFO(("wl%d: PMKID received: ", UNIT(sup)));
						WL_SUP_INFO(("%02x %02x %02x %02x"
							" %02x %02x %02x %02x\n",
							d[0], d[1], d[2], d[3],
							d[4], d[5], d[6], d[7]));
						WL_SUP_INFO(("%02x %02x %02x %02x"
							" %02x %02x %02x %02x\n",
							d[8], d[9], d[10], d[11],
							d[12], d[13], d[14], d[15]));
					}
#endif /* BCMDBG || WLMSG_WSEC */

					if (!wlc_sup_retrieve_pmk(sup, sup->cfg,
						data_encap->data, &BSS_EA(sup),
						sup->wpa_info->pmk, PMK_LEN)) {
						sup->wpa_info->pmk_len = PMK_LEN;
					}
					else
						return TRUE;
				}
			}
#if defined(BCMSUP_PSK)
			else if (wpa->WPA_auth & WPA2_AUTH_PSK) {
				/* If driver based roaming is enabled, PMK can get overwritten.
				 * Restore the PMK derived from PSK.
				 */
				if (sup->wpa_info->pmk_psk_len > 0) {
					WL_SUP_INFO(("wl%d: wlc_wpa_sup_eapol: Restore PMK\n",
						UNIT(sup)));
					bcopy((char*)sup->wpa_info->pmk_psk, sup->wpa_info->pmk,
						sup->wpa_info->pmk_psk_len);
					sup->wpa_info->pmk_len = sup->wpa_info->pmk_psk_len;
				}
				else
					WL_SUP_INFO(("wl%d: wlc_wpa_sup_eapol:"
						" PMK from PSK not saved\n", UNIT(sup)));
			}
#endif /* BCMSUP_PSK */
			if (sup->wpa_info->pmk_len == 0) {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: No PMK available to compose"
				         " pairwise msg 2\n",
				         UNIT(sup)));
				return TRUE;
			}

			/* Save Anonce, generate Snonce, and produce PTK */
			bcopy(body->nonce, wpa->anonce, sizeof(wpa->anonce));
			wlc_getrand(sup->wlc, wpa->snonce, EAPOL_WPA_KEY_NONCE_LEN);

#if defined(WLFBT)
			if (BSSCFG_IS_FBT(sup->cfg) && (wpa->WPA_auth & WPA2_AUTH_FT))
				wlc_fbt_calc_fbt_ptk(sup->wlc->fbt, sup->cfg);
			else
#endif /* WLFBT */

#ifdef MFP
			if (wpa->WPA_auth & (WPA2_AUTH_PSK_SHA256 | WPA2_AUTH_1X_SHA256) ||
			    (sup->cfg->current_bss->wpa2.flags & RSN_FLAGS_SHA256))
				kdf_calc_ptk(&PEER_EA(sup), &CUR_EA(sup),
				       wpa->anonce, wpa->snonce,
				       sup->wpa_info->pmk, (uint)sup->wpa_info->pmk_len,
				       wpa->eapol_mic_key, (uint)wpa->ptk_len);
			else
#endif // endif
			{
				if (!memcmp(&PEER_EA(sup), &CUR_EA(sup), ETHER_ADDR_LEN)) {
					/* something is wrong -- toss; invalid eapol */
					WL_SUP_INFO(("wl%d:wlc_wpa_sup_eapol:"
						"toss msg; same mac\n", UNIT(sup)));
					return TRUE;
				} else {
					wpa_calc_ptk(&PEER_EA(sup), &CUR_EA(sup),
						wpa->anonce, wpa->snonce,
						sup->wpa_info->pmk,
						(uint)sup->wpa_info->pmk_len,
						wpa->eapol_mic_key,
						(uint)wpa->ptk_len);
				}
			}

			/* Send pair-wise message 2 */
			if (wlc_wpa_sup_sendeapol(sup_info, sup->cfg,
				(key_info & PMSG2_MATCH_FLAGS), PMSG2)) {
				wpa->state = WPA_SUP_STAKEYSTARTP_WAIT_M3;
			} else {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol:"
					"send message 2 failed\n", UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_SEND_FAIL);
			}
		} else {
			WL_SUP_INFO(("wl%d: wlc_wpa_sup_eapol: processing message 3\n",
				UNIT(sup)));

			/* Test message 3 key_info flags */
			prohibited = (encrypted || sup_info->sup_m3sec_ok)
			        ? (PMSG3_PROHIBITED & ~WPA_KEY_SECURE)
				: PMSG3_PROHIBITED;
			required = encrypted ? (PMSG3_REQUIRED & ~WPA_KEY_SECURE) : PMSG3_REQUIRED;

			if (bcmwpa_is_rsn_auth(wpa->WPA_auth)) {
				prohibited = 0;
				required = PMSG3_WPA2_REQUIRED;
			}

			/* In case that M4 is lost in the air, M3 may come at the state of
			 * WPA_SUP_KEYUPDATE in case of WPA2.
			 * To process this packet, it needs to restore the state
			 */
			if (bcmwpa_is_rsn_auth(wpa->WPA_auth) &&
				(wpa->state == WPA_SUP_KEYUPDATE)) {
				wpa->state = WPA_SUP_STAKEYSTARTP_WAIT_M3;
				WL_WSEC(("wl%d: restore state to WPA_SUP_WAIT_M3\n", UNIT(sup)));
			}

			if (((key_info & required) != required) ||
				((key_info & prohibited) != 0))
			{
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: unexpected key_info"
				         " (0x%04x) in WPA pairwise key message 3\n",
				         UNIT(sup), (uint)key_info));
				return TRUE;
			} else if (wpa->state < WPA_SUP_STAKEYSTARTP_PREP_M2 ||
			           wpa->state > WPA_SUP_STAKEYSTARTG_PREP_G2) {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: unexpected 4-way msg 3"
				         " in state %d\n",
				         UNIT(sup), wpa->state));
				/* don't accept msg3 unless it follows msg1 */
				return TRUE;
			}
			wpa->state = WPA_SUP_STAKEYSTARTP_PREP_M4;

			/* check anonce */
			if (bcmp(body->nonce, wpa->anonce, sizeof(wpa->anonce))) {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: anonce in key message 3"
				         " doesn't match anonce in key message 1,"
				         " discarding pkt \n", UNIT(sup)));
				return TRUE;
			}

			/* Test AP's WPA IE against saved one */
			data_len = ntoh16_ua(&body->data_len);
			if (bcmwpa_is_rsn_auth(wpa->WPA_auth)) {
				uint16 len;
				bcm_tlv_t *wpa2ie;
				wpa2ie = bcm_find_tlv(body->data, data_len, DOT11_MNG_RSN_ID);
				/* verify RSN IE */
				if (!wlc_wpa_sup_check_rsn(sup, wpa2ie)) {
					WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: WPA IE mismatch in"
						" key message 3\n", UNIT(sup)));
					wlc_wpa_send_sup_status(sup_info, sup->cfg,
						WLC_E_SUP_MSG3_IE_MISMATCH);
					/* should cause a deauth */
					wlc_wpa_senddeauth(sup->cfg, (char *)&PEER_EA(sup),
						DOT11_RC_WPA_IE_MISMATCH);
					return TRUE;
				}
				/* looking for second RSN IE.  deauth if presents */
				len = data_len - (uint16)((uint8*)wpa2ie - (uint8*)body->data);
				if (len > ((uint16)TLV_HDR_LEN + (uint16)wpa2ie->len) &&
					bcm_find_tlv((uint8*)wpa2ie + TLV_HDR_LEN + wpa2ie->len,
					len - (TLV_HDR_LEN + wpa2ie->len), DOT11_MNG_RSN_ID)) {
					WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: WPA IE contains"
						" more than one RSN IE in key message 3\n",
						UNIT(sup)));
					wlc_wpa_send_sup_status(sup_info, sup->cfg,
						WLC_E_SUP_MSG3_TOO_MANY_IE);
					/* should cause a deauth */
					wlc_wpa_senddeauth(sup->cfg,
					                   (char *)&PEER_EA(sup),
					                   DOT11_RC_WPA_IE_MISMATCH);
					return TRUE;
				}
			}
			else if ((wpa->auth_wpaie_len != data_len) ||
			         (bcmp(wpa->auth_wpaie, body->data,
			               wpa->auth_wpaie_len))) {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol:"
				         " WPA IE mismatch in key message 3\n",
				         UNIT(sup)));
				/* should cause a deauth */
				wlc_wpa_senddeauth(sup->cfg, (char *)&PEER_EA(sup),
				                   DOT11_RC_WPA_IE_MISMATCH);
				return TRUE;
			}

			if (wlc_wpa_sup_sendeapol(sup_info, sup->cfg,
				(key_info & PMSG4_MATCH_FLAGS), PMSG4)) {
				wpa->state = WPA_SUP_STAKEYSTARTG_WAIT_G1;
			} else {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: send message 4 failed\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_SEND_FAIL);
				return FALSE;
			}

			if (key_info & WPA_KEY_INSTALL) {
				/* Plumb paired key */
				wlc_wpa_plumb_tk(sup->wlc, sup->cfg,
					(uint8*)wpa->temp_encr_key,
					wpa->tk_len, wpa->ucipher, &PEER_EA(sup));
#ifdef WL_FTM
				if (PROXD_ENAB(sup->wlc->pub)) {
					scb_t *scb;
					wlc_ftm_t *ftm;
					ftm = wlc_ftm_get_handle(sup->wlc);
					scb = wlc_scbfind_dualband(sup->wlc,
						sup->cfg, &PEER_EA(sup));
					if (scb && ftm) {
						int err = wlc_ftm_new_tpk(ftm, scb,
							sup->wpa_info->pmk, sup->wpa_info->pmk_len,
							wpa->anonce, EAPOL_WPA_KEY_NONCE_LEN,
							wpa->snonce, EAPOL_WPA_KEY_NONCE_LEN);
						if (err != BCME_OK) {
							WL_ERROR(("wl%d: %s: "
								"error %d generating new tpk.\n",
								UNIT(sup), __FUNCTION__,  err));
						}
					}
				}
#endif /* WL_FTM */
			} else {
				/* While INSTALL is in the `required' set this
				 * test is a tripwire for when that changes
				 */
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol:"
				         " INSTALL flag unset in 4-way msg 3\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_NO_INSTALL_FLAG);
				return FALSE;
			}

			if (bcmwpa_is_rsn_auth(wpa->WPA_auth)) {
				eapol_wpa2_encap_data_t *data_encap;
				eapol_wpa2_key_gtk_encap_t *gtk_kde;

				/* extract GTK */
				data_encap = wpa_find_gtk_encap(body->data, data_len);
				if (!data_encap || ((uint)(data_encap->length -
					EAPOL_WPA2_GTK_ENCAP_MIN_LEN) > sizeof(wpa->gtk))) {
					WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: encapsulated GTK"
					         " missing from message 3\n", UNIT(sup_info)));
					wlc_wpa_send_sup_status(sup_info, sup->cfg,
						WLC_E_SUP_MSG3_NO_GTK);
					return FALSE;
				}
				wpa->gtk_len = data_encap->length -
				    ((EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
				     EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN);
				gtk_kde = (eapol_wpa2_key_gtk_encap_t *)data_encap->data;
				wpa->gtk_index = (gtk_kde->flags & WPA2_GTK_INDEX_MASK) >>
				    WPA2_GTK_INDEX_SHIFT;
				bcopy(gtk_kde->gtk, wpa->gtk, wpa->gtk_len);

				/* plumb GTK */
				wlc_wpa_plumb_gtk(sup->wlc, sup->cfg, wpa->gtk,
					wpa->gtk_len, wpa->gtk_index, wpa->mcipher, body->rsc,
					gtk_kde->flags & WPA2_GTK_TRANSMIT);
#ifdef MFP
				if (WLC_MFP_ENAB(sup->wlc->pub))
					wlc_mfp_extract_igtk(SUPMFP(sup), sup->cfg, eapol);
#endif // endif
			}
			if (bcmwpa_is_rsn_auth(wpa->WPA_auth)) {
				wpa->state = WPA_SUP_KEYUPDATE;

				WL_SUP_INFO(("wl%d: wlc_wpa_sup_eapol: WPA2 key update complete\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_OTHER);

				/* Authorize scb for data */
#ifdef BCMINTSUP
				(void)wlc_ioctl(sup->wlc, WLC_SCB_AUTHORIZE,
					&PEER_EA(sup), ETHER_ADDR_LEN, sup->cfg->wlcif);

#ifdef WLFBT
				if (BSSCFG_IS_FBT(sup->cfg))
					BSS_FBT_INI_FBT(sup->wlc->fbt, sup->cfg) = FALSE;
#endif /* WLFBT */

#else /* BCMINTSUP */
				AUTHORIZE(sup->cfg);
#endif /* BCMINTSUP */
			} else
				wpa->state = WPA_SUP_STAKEYSTARTG_WAIT_G1;
		}

	} else {
		/* Pairwise flag clear; should be group key message. */
		if (wpa->state <  WPA_SUP_STAKEYSTARTG_WAIT_G1) {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: unexpected group key msg1"
			         " in state %d\n", UNIT(sup), wpa->state));
			return TRUE;
		}

		if (SUP_CHECK_MCIPHER(sup)) {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: multicast cipher mismatch"
			         " in group key message\n",
			         UNIT(sup)));
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_GRP_KEY_CIPHER);
			return FALSE;
		}

		if ((key_info & GMSG1_REQUIRED) != GMSG1_REQUIRED) {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: unexpected key_info (0x%04x)in"
				 "WPA group key message\n",
				 UNIT(sup), (uint)key_info));
			return TRUE;
		}

		wpa->state = WPA_SUP_STAKEYSTARTG_PREP_G2;
		if (bcmwpa_is_rsn_auth(wpa->WPA_auth)) {
			eapol_wpa2_encap_data_t *data_encap;
			eapol_wpa2_key_gtk_encap_t *gtk_kde;

			/* extract GTK */
			data_len = ntoh16_ua(&body->data_len);
			data_encap = wpa_find_gtk_encap(body->data, data_len);
			if (!data_encap || ((uint)(data_encap->length -
				EAPOL_WPA2_GTK_ENCAP_MIN_LEN) > sizeof(wpa->gtk))) {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: encapsulated GTK missing"
					" from group message 1\n", UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_GRP_MSG1_NO_GTK);
				return FALSE;
			}
			wpa->gtk_len = data_encap->length - ((EAPOL_WPA2_ENCAP_DATA_HDR_LEN -
			                                          TLV_HDR_LEN) +
			                                         EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN);
			gtk_kde = (eapol_wpa2_key_gtk_encap_t *)data_encap->data;
			wpa->gtk_index = (gtk_kde->flags & WPA2_GTK_INDEX_MASK) >>
			    WPA2_GTK_INDEX_SHIFT;
			bcopy(gtk_kde->gtk, wpa->gtk, wpa->gtk_len);

			/* plumb GTK */
			wlc_wpa_plumb_gtk(sup->wlc, sup->cfg, wpa->gtk, wpa->gtk_len,
				wpa->gtk_index, wpa->mcipher, body->rsc,
				gtk_kde->flags & WPA2_GTK_TRANSMIT);
#ifdef MFP
			if (WLC_MFP_ENAB(sup->wlc->pub))
				wlc_mfp_extract_igtk(SUPMFP(sup), sup->cfg, eapol);
#endif // endif
		} else {

			uint8 *data, *encrkey;
			rc4_ks_t *rc4key;
			bool decr_status;

			if (!(data = MALLOC(sup->osh, WPA_KEY_DATA_LEN_256))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					UNIT(sup), __FUNCTION__,  MALLOCED(sup_info->osh)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}
			if (!(encrkey = MALLOC(sup->osh, WPA_MIC_KEY_LEN*2))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
				MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}
			if (!(rc4key = MALLOC(sup->osh, sizeof(rc4_ks_t)))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
				MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
				MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}

			decr_status = wpa_decr_gtk(body, key_info, wpa->eapol_encr_key,
			                  wpa->gtk, data, encrkey, rc4key);

			MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
			MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
			MFREE(sup->osh, rc4key, sizeof(rc4_ks_t));

			if (!decr_status || (key_len > sizeof(wpa->gtk))) {
				WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: GTK decrypt failure\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup_info, sup->cfg,
					WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}
			wpa->gtk_len = key_len;

			/* plumb GTK */
			wlc_wpa_plumb_gtk(sup->wlc, sup->cfg, wpa->gtk, wpa->gtk_len,
				(key_info & WPA_KEY_INDEX_MASK) >> WPA_KEY_INDEX_SHIFT,
				cipher, body->rsc, key_info & WPA_KEY_INSTALL);
		}

		/* send group message 2 */
		if (wlc_wpa_sup_sendeapol(sup_info, sup->cfg,
			(key_info & GMSG2_MATCH_FLAGS), GMSG2)) {
			wpa->state = WPA_SUP_KEYUPDATE;

			WL_SUP_INFO(("wl%d: wlc_wpa_sup_eapol: key update complete\n",
			         UNIT(sup)));
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_OTHER);
		} else {
			WL_SUP_ERROR(("wl%d: wlc_wpa_sup_eapol: send grp msg 2 failed\n",
			         UNIT(sup)));
			wlc_wpa_send_sup_status(sup_info, sup->cfg, WLC_E_SUP_SEND_FAIL);
		}

		/* Authorize scb for data */
#ifdef BCMINTSUP
		(void)wlc_ioctl(sup->wlc, WLC_SCB_AUTHORIZE,
			&PEER_EA(sup), ETHER_ADDR_LEN, sup->cfg->wlcif);
#else
		AUTHORIZE(sup->cfg);
#endif // endif
	}
	return TRUE;
}
#endif /* defined(BCMSUP_PSK) || defined(BCMAUTH_PSK) */

#ifdef BCMINTSUP
/* Break a lengthy passhash algorithm into smaller pieces. It is necessary
 * for dongles with under-powered CPUs.
 */
static void
wlc_sup_wpa_passhash_timer(void *arg)
{
	supplicant_t *sup = (supplicant_t *)arg;
	wpapsk_info_t *info = sup->wpa_info;

	if (do_passhash(&info->passhash_states, 256) == 0) {
		WL_WSEC(("wl%d: passhash is done\n", UNIT(sup)));
		get_passhash(&info->passhash_states, info->pmk, PMK_LEN);
		info->pmk_len = PMK_LEN;

#if defined(BCMSUP_PSK)
		/* Store the PMK derived from PSK */
		if (sup->cfg->WPA_auth & WPA2_AUTH_PSK) {
			bcopy((char*)info->pmk, info->pmk_psk, info->pmk_len);
			info->pmk_psk_len = info->pmk_len;
		}
#endif /* BCMSUP_PSK */
		wlc_join_bss_prep(sup->cfg);
		return;
	}

	WL_WSEC(("wl%d: passhash is in progress\n", UNIT(sup)));
	wl_add_timer(info->wlc->wl, info->passhash_timer, 0, 0);
}
#endif /* BCMINTSUP */

static bool
wlc_sup_wpapsk_start(supplicant_t *sup, uint8 *sup_ies, uint sup_ies_len,
	uint8 *auth_ies, uint auth_ies_len)
{
	bool ret = TRUE;
	wpapsk_t *wpa;

	wpa = sup->wpa;

	wlc_wpapsk_free(sup->wlc, wpa);

	wpa->state = WPA_SUP_INITIALIZE;

	if (SUP_CHECK_WPAPSK_SUP_TYPE(sup)) {
		wpa->WPA_auth = sup->cfg->WPA_auth;
	}

	if (!wlc_wpapsk_start(sup->wlc, wpa, sup_ies, sup_ies_len, auth_ies, auth_ies_len)) {
		WL_ERROR(("wl%d: wlc_wpapsk_start() failed\n",
			UNIT(sup)));
		return FALSE;
	}

	if ((sup->sup_type == SUP_WPAPSK) && (sup->wpa_info->pmk_len == 0)) {
		WL_SUP_ERROR(("wl%d: wlc_sup_wpapsk_start: no PMK material found\n", UNIT(sup)));
		ret = FALSE;
	}

	return ret;
}

#if defined(BCMINTSUP)
/* return 0 when succeeded, 1 when passhash is in progress, -1 when failed */
int
wlc_sup_set_ssid(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, uchar ssid[], int len)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (sup == NULL) {
		WL_WSEC(("wlc_sup_set_ssid: called with NULL sup\n"));
		return -1;
	} else if (sup->wpa_info->psk_len == 0) {
		WL_WSEC(("wlc_sup_set_ssid: called with NULL psk\n"));
		return 0;
	} else if (sup->wpa_info->pmk_len != 0) {
		WL_WSEC(("wlc_sup_set_ssid: called with non-NULL pmk\n"));
		return 0;
	}
	return wlc_wpa_cobble_pmk(sup->wpa_info, (char *)sup->wpa_info->psk,
		sup->wpa_info->psk_len, ssid, len);
}
#endif /* BCMINTSUP */

bool
wlc_sup_send_micfailure(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, bool ismulti)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	uint16 flags;

	if (sup == NULL) {
		WL_WSEC(("wlc_sup_send_micfailure called with NULL supplicant\n"));
		return FALSE;
	}

	if (sup->sup_type == SUP_UNUSED)
		return FALSE;

	flags = (uint16) (MIC_ERROR_REQUIRED | sup->wpa->desc);
	if (!ismulti)
		flags |= (uint16) WPA_KEY_PAIRWISE;
	WL_WSEC(("wl%d.%d: wlc_sup_send_micfailure: sending MIC failure report\n",
	         UNIT(sup), WLC_BSSCFG_IDX(cfg)));
	(void) wlc_wpa_sup_sendeapol(sup_info, cfg, flags, MIC_FAILURE);
	return TRUE;
}
#endif	/* BCMSUP_PSK */

#if defined(BCMSUP_PSK)
int
wlc_sup_set_pmk(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg, wsec_pmk_t *pmk, bool assoc)
{
	supplicant_t *sup;

	if (sup_info == NULL || cfg == NULL) {
		WL_ERROR(("%s: missing sup_info or bsscfg\n", __FUNCTION__));
		return BCME_BADARG;
	}

	sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	if (sup == NULL || pmk == NULL) {
		WL_WSEC(("%s: missing required parameter\n", __FUNCTION__));
		return BCME_BADARG;
	}

	return wlc_set_pmk(sup->wlc, sup->wpa_info, sup->wpa, cfg, pmk, assoc);
}

int
wlc_check_sup_state(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (!sup || !sup->sup_enable_wpa) {
		WL_ERROR(("wl%d: supbsspub is in wrong state %s\n", WLC_BSSCFG_IDX(cfg),
			__FUNCTION__));
		return BCME_ERROR;
	}
	return BCME_OK;
}

int
wlc_sup_reset(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (sup->wpa) {
		bzero(&sup->wpa->replay, EAPOL_KEY_REPLAY_LEN);
		bzero(&sup->wpa->eapol_mic_key, WPA_MIC_KEY_LEN);
		return BCME_OK;
	} else {
		return BCME_ERROR;
	}
}
#endif /* BCMSUP_PSK */

#if defined(BCMSUP_PSK) || defined(WLFBT)
int
wlc_set_pmk(wlc_info_t *wlc, wpapsk_info_t *info, wpapsk_t *wpa,
	struct wlc_bsscfg *cfg, wsec_pmk_t *pmk, bool assoc)
{
	/* Zero length means forget what's there now */
	if (pmk->key_len == 0)
		return BCME_OK;

	info->pmk_len = 0;
	info->psk_len = 0;

#if defined(BCMINTSUP) || defined(WLFBT)
	/* A key that needs hashing has to wait until we see the SSID */
	if (pmk->flags & WSEC_PASSPHRASE) {
		WL_WSEC(("wl%d: %s: saving raw PSK\n",
		         wlc->pub->unit, __FUNCTION__));

		if (pmk->key_len == WSEC_MAX_PSK_LEN) {
			info->psk_len = 0;
			/* this size must be legible hex and need not wait */
			if (wlc_wpa_cobble_pmk(info, (char *)pmk->key, pmk->key_len,
				NULL, 0) < 0) {
				return BCME_ERROR;
			} else {
				if (bcmwpa_includes_rsn_auth(wpa->WPA_auth) && assoc) {
					int ret = BCME_OK;
#ifdef BCMSUP_PSK
					if (SUP_ENAB(wlc->pub) &&
						BSS_SUP_ENAB_WPA(wlc->idsup, cfg) &&
						(wpa->WPA_auth & WPA2_AUTH_UNSPECIFIED))
						ret = wlc_sup_set_pmkid(wlc->idsup, cfg,
							&info->pmk[0], info->pmk_len, &cfg->BSSID,
							NULL);
#endif /* BCMSUP_PSK */
#ifdef WLFBT
					if (BSSCFG_IS_FBT(cfg) && (ret == BCME_OK) &&
						(cfg->WPA_auth & WPA2_AUTH_FT))
						wlc_fbt_calc_fbt_ptk(wlc->fbt, cfg);
#endif // endif
					if (ret != BCME_OK)
						return ret;
				}
#ifdef BCMSUP_PSK
				/* For driver based roaming, PSK can be shared by the supplicant
				 * only once during initialization. Save the PMK derived from
				 * the PSK separately so that it can be used for subsequent
				 * PSK associations without intervention from the supplicant.
				 */
				if ((wpa->WPA_auth & WPA2_AUTH_PSK) || !assoc) {
					WL_WSEC(("wl%d: %s: Save the PMK from PSK\n",
						wlc->pub->unit, __FUNCTION__));
					bcopy((char*)info->pmk, info->pmk_psk, info->pmk_len);
					info->pmk_psk_len = info->pmk_len;
				}
#endif /* BCMSUP_PSK */
				return BCME_OK;
			}
		} else if ((pmk->key_len >= WSEC_MIN_PSK_LEN) &&
		    (pmk->key_len < WSEC_MAX_PSK_LEN)) {
			bcopy((char*)pmk->key, info->psk, pmk->key_len);
			info->psk_len = pmk->key_len;
			return BCME_OK;
		}
		return BCME_ERROR;
	}
#endif /* BCMINTSUP || WLFBT */

	/* If it's not a passphrase it must be a proper PMK */
	if (pmk->key_len > TKIP_KEY_SIZE) {
		WL_WSEC(("wl%d: %s: unexpected key size (%d)\n",
		         wlc->pub->unit, __FUNCTION__, pmk->key_len));
		return BCME_BADARG;
	}

	bcopy((char*)pmk->key, info->pmk, pmk->key_len);
	info->psk_len = 0;
	info->pmk_len = pmk->key_len;
#ifdef BCMSUP_PSK
	/* In case PMK is directly passed by the supplicant for PSK associations */
	if ((wpa->WPA_auth & WPA2_AUTH_PSK) || !assoc) {
		WL_WSEC(("wl%d: %s: Save the PMK passed directly\n",
			wlc->pub->unit, __FUNCTION__));
		bcopy((char*)info->pmk, info->pmk_psk, info->pmk_len);
		info->pmk_psk_len = info->pmk_len;
	}
#endif /* BCMSUP_PSK */

#if defined(BCMSUP_PSK)
	if (SUP_ENAB(wlc->pub) && BSS_SUP_ENAB_WPA(wlc->idsup, cfg) &&
	   (wpa->WPA_auth & WPA2_AUTH_UNSPECIFIED) && assoc)
		wlc_sup_set_pmkid(wlc->idsup, cfg, &info->pmk[0], info->pmk_len,
			&cfg->BSSID, NULL);
#endif // endif
#ifdef WLFBT
	if (BSSCFG_IS_FBT(cfg) && (cfg->WPA_auth & WPA2_AUTH_FT))
		wlc_fbt_calc_fbt_ptk(wlc->fbt, cfg);
#endif // endif

	return BCME_OK;
}

uint8
wlc_sup_get_wpa_state(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = sup_info ? SUP_BSSCFG_CUBBY(sup_info, cfg) : NULL;

	return (sup && sup->sup_enable_wpa) ? sup->wpa->state : 0;
}
#endif /* BCMSUP_PSK || WLFBT */

#if defined(BCMSUP_PSK)
void
wlc_sup_set_ea(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, struct ether_addr *ea)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (sup == NULL)
		return;

	bcopy(ea, &sup->peer_ea, ETHER_ADDR_LEN);
}

/* ARGSUSED */
bool
wlc_set_sup(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, int sup_type,
	/* the following parameters are used only for PSK */
	uint8 *sup_ies, uint sup_ies_len, uint8 *auth_ies, uint auth_ies_len)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	bool ret = TRUE;

	if (sup == NULL) {
		WL_WSEC(("wlc_set_sup called with NULL sup context\n"));
		return FALSE;
	}
	sup->sup_type = SUP_UNUSED;
	sup->ap_eapver = 0;

	if (sup_type == SUP_WPAPSK) {
		sup->sup_type = sup_type;
		ret = wlc_sup_wpapsk_start(sup, sup_ies, sup_ies_len, auth_ies, auth_ies_len);
	}

	/* If sup_type is still SUP_UNUSED, the passed type must be bogus */
	if (sup->sup_type == SUP_UNUSED) {
		WL_WSEC(("wl%d: wlc_set_sup: unexpected supplicant type %d\n",
		         UNIT(sup), sup_type));
		return FALSE;
	}
	return ret;
}

/* Convert the basic supplicant state from internal to external format */
static sup_auth_status_t
wlc_sup_conv_auth_state(wpapsk_state_t state)
{
	switch (state) {
		case WPA_SUP_DISCONNECTED:
			return WLC_SUP_DISCONNECTED;
		case WPA_SUP_INITIALIZE:
			return WLC_SUP_AUTHENTICATED;
		case WPA_SUP_AUTHENTICATION:
		case WPA_SUP_STAKEYSTARTP_PREP_M2:
		case WPA_SUP_STAKEYSTARTP_WAIT_M3:
		case WPA_SUP_STAKEYSTARTP_PREP_M4:
		case WPA_SUP_STAKEYSTARTG_WAIT_G1:
		case WPA_SUP_STAKEYSTARTG_PREP_G2:
			return WLC_SUP_KEYXCHANGE;
		case WPA_SUP_KEYUPDATE:
			return WLC_SUP_KEYED;
		default:
			return WLC_SUP_DISCONNECTED;
	}
}

static sup_auth_status_t
wlc_sup_get_auth_status(supplicant_t *sup)
{
	if ((SUP_BSSCFG_CUBBY(sup->m_handle, sup->cfg))->sup_type == SUP_WPAPSK) {
		return wlc_sup_conv_auth_state(sup->wpa->state);
	}

	return WLC_SUP_DISCONNECTED;
}

/* Convert the extended supplicant state from internal to external format */
static sup_auth_status_t
wlc_sup_conv_ext_auth_state(wpapsk_state_t state)
{
	switch (state) {
		case WPA_SUP_STAKEYSTARTP_WAIT_M1:
			return WLC_SUP_KEYXCHANGE_WAIT_M1;
		case WPA_SUP_STAKEYSTARTP_PREP_M2:
			return WLC_SUP_KEYXCHANGE_PREP_M2;
		case WPA_SUP_STAKEYSTARTP_WAIT_M3:
			return WLC_SUP_KEYXCHANGE_WAIT_M3;
		case WPA_SUP_STAKEYSTARTP_PREP_M4:
			return WLC_SUP_KEYXCHANGE_PREP_M4;
		case WPA_SUP_STAKEYSTARTG_WAIT_G1:
			return WLC_SUP_KEYXCHANGE_WAIT_G1;
		case WPA_SUP_STAKEYSTARTG_PREP_G2:
			return WLC_SUP_KEYXCHANGE_PREP_G2;
		default:
			return wlc_sup_conv_auth_state(state);
	}
}

/* Return the extended supplicant authentication state */
static sup_auth_status_t
wlc_sup_get_auth_status_extended(supplicant_t *sup)
{
	sup_auth_status_t	status = wlc_sup_get_auth_status(sup);
#ifdef BCMINTSUP
	if (!sup->cfg->associated) {
		status = WLC_SUP_DISCONNECTED;
	}
	else
#endif /* BCMINTSUP */
	if (status == WLC_SUP_KEYXCHANGE) {
		status = wlc_sup_conv_ext_auth_state(sup->wpa->state);
	}
	return status;
}

/* Dispatch EAPOL to supplicant.
 * Return boolean indicating whether supplicant's use of message means
 * it should be freed or sent up.
 */
bool
wlc_sup_eapol(struct wlc_sup_info *sup_info, struct wlc_bsscfg *cfg, eapol_header_t *eapol_hdr,
	bool encrypted)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (!sup) {
		WL_ERROR(("wl%d:%s: called with NULL sup\n", WLCWLUNIT(cfg->wlc), __FUNCTION__));
		return FALSE;
	}
	if (sup->sup_type == SUP_UNUSED)
		return FALSE;

#ifdef	REDO_THIS_STUFF
	if (eapol_hdr->type == EAPOL_KEY) {
		eapol_wpa_key_header_t *body;

		body = (eapol_wpa_key_header_t *)eapol_hdr->body;
		if (body->type == EAPOL_WPA_KEY) {
		}
	}
#endif /* REDO_THIS_STUFF */
	/* Save eapol version from the AP */
	sup->ap_eapver = eapol_hdr->version;
	/* If the supplicant is set to do a WPA key exchange and this is
	 * a WPA key message, send it to the code for WPA.
	 */
	if ((eapol_hdr->type == EAPOL_KEY) &&
		SUP_CHECK_WPAPSK_SUP_TYPE(sup))
	 {
		eapol_wpa_key_header_t *body;
		if (ntoh16(eapol_hdr->length) < OFFSETOF(eapol_wpa_key_header_t, mic)) {
			WL_WSEC(("wl%d: %s: bad eapol - header too small.\n",
				WLCWLUNIT(sup->wlc), __FUNCTION__));
			WLCNTINCR(sup->wlc->pub->_cnt->rxrunt);
			return FALSE;
		}
		body = (eapol_wpa_key_header_t *)eapol_hdr->body;
		if (SUP_CHECK_EAPOL(body)) {
			(void) wlc_wpa_sup_eapol(sup, eapol_hdr, encrypted);
			return TRUE;
		}
	}

	/* Get here if no supplicant saw the message */
	/* Reset sup state and clear PMK on (re)auth (i.e., EAP Id Request) */
	if (sup->sup_type == SUP_WPAPSK && eapol_hdr->type == EAP_PACKET) {
		eap_header_t *eap_hdr = (eap_header_t *)eapol_hdr->body;

		if (eap_hdr->type == EAP_IDENTITY &&
		    eap_hdr->code == EAP_REQUEST) {
			if (sup->wpa->WPA_auth & WPA_AUTH_UNSPECIFIED ||
			    sup->wpa->WPA_auth & WPA2_AUTH_UNSPECIFIED) {
				WL_SUP_INFO(("wl%d: wlc_sup_eapol: EAP-Identity Request received - "
				         "reset supplicant state and clear PMK\n", UNIT(sup)));
				sup->wpa->state = WPA_SUP_INITIALIZE;
				sup->wpa_info->pmk_len = 0;
			} else {
				WL_SUP_ERROR(("wl%d: wlc_sup_eapol: EAP-Identity Request ignored\n",
				         UNIT(sup)));
			}
		} else {
			WL_SUP_ERROR(("wl%d: wlc_sup_eapol: EAP packet ignored\n", UNIT(sup)));
		}
	}

	return FALSE;
}

int
wlc_sup_down(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	int callbacks = 0;

	if (sup == NULL)
		return callbacks;

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	if (!wl_del_timer(sup->wl, sup->wpa_info->passhash_timer))
		callbacks ++;
	if (!wl_del_timer(sup->wl, sup->wpa_psk_timer))
		callbacks ++;
#endif	/* BCMSUP_PSK && BCMINTSUP */
	return callbacks;
}

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/**
 * Allocate supplicant context, squirrel away the passed values,
 * and return the context handle.
 */
static int
wlc_sup_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_sup_info_t *sup_info = (wlc_sup_info_t *)ctx;

	wlc_info_t *wlc = sup_info->wlc;

	supplicant_t **psup = SUP_BSSCFG_CUBBY_LOC(sup_info, cfg);
	supplicant_t *sup = NULL;

	sup_init_event_data_t evt;

	WL_TRACE(("wl%d: wlc_sup_init\n", UNIT(sup_info)));

	if (BSSCFG_AP(cfg))
		return BCME_NOTSTA;
	if (!(sup = MALLOCZ(sup_info->osh, sizeof(*sup)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          UNIT(sup_info), __FUNCTION__, MALLOCED(sup_info->osh)));
		goto err;
	}
	*psup = sup;

	sup->m_handle = wlc->idsup;
	sup->cfg = cfg;
	sup->wlc = wlc;
	sup->osh = sup_info->osh;
	sup->wl = sup_info->wl;
	sup->pub = sup_info->pub;

	sup->sup_type = SUP_UNUSED;
	sup->sup_enable_wpa = TRUE;

#if defined(BCMSUP_PSK) || defined(WLFBT)
	if (!(sup->wpa = MALLOCZ(sup_info->osh, sizeof(wpapsk_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          UNIT(sup_info), __FUNCTION__, MALLOCED(sup_info->osh)));
		goto err;
	}
	if (!(sup->wpa_info = MALLOCZ(sup_info->osh, sizeof(wpapsk_info_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          UNIT(sup_info), __FUNCTION__, MALLOCED(sup_info->osh)));
		goto err;
	}
	sup->wpa_info->wlc = wlc;
#endif /* BCMSUP_PSK */

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	if (!(sup->wpa_info->passhash_timer =
	           wl_init_timer(sup_info->wl, wlc_sup_wpa_passhash_timer, sup, "passhash"))) {
		WL_ERROR(("wl%d: %s: passhash timer failed\n",
		          UNIT(sup_info), __FUNCTION__));
		goto err;
	}

	if (!(sup->wpa_psk_timer = wl_init_timer(sup_info->wl, wlc_wpa_psk_timer, sup,
	                                         "wpapsk"))) {
		WL_ERROR(("wl%d: wlc_sup_init: wl_init_timer for wpa psk timer failed\n",
		          UNIT(sup_info)));
		goto err;
	}
#endif	/* BCMSUP_PSK && BCMINTSUP */

	evt.bsscfg = cfg;
	evt.wpa = sup->wpa;
	evt.wpa_info = sup->wpa_info;
	evt.up = TRUE;
	bcm_notif_signal(sup_info->sup_up_down_notif_hdl, &evt);
	return BCME_OK;
err:
	if (sup)
		wlc_sup_deinit(sup_info, cfg);
	return BCME_ERROR;
}

/** Toss supplicant context */
static void
wlc_sup_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_sup_info_t *sup_info = (wlc_sup_info_t *)ctx;
	supplicant_t **psup = SUP_BSSCFG_CUBBY_LOC(sup_info, cfg);
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	sup_init_event_data_t evt;

	if (sup != NULL) {
		WL_TRACE(("wl%d: wlc_sup_deinit\n", UNIT(sup)));

		wlc_sup_clear_pmkid_store(sup_info, cfg);

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
		if (sup->wpa_psk_timer)
			wl_free_timer(sup->wl, sup->wpa_psk_timer);

		if (sup->wpa_info) {
			if (sup->wpa_info->passhash_timer)
				wl_free_timer(sup->wl, sup->wpa_info->passhash_timer);
			MFREE(sup->osh, sup->wpa_info, sizeof(wpapsk_info_t));
		}
#endif	/* BCMSUP_PSK && BCMINTSUP */

		evt.bsscfg = cfg;
		evt.wpa = NULL; /* MAY be NULL also */
		evt.wpa_info = NULL;
		evt.up = FALSE;
		bcm_notif_signal(sup_info->sup_up_down_notif_hdl, &evt);

#if defined(BCMSUP_PSK)
		if (sup->wpa) {
			wlc_wpapsk_free(sup->wlc, sup->wpa);
			MFREE(sup->osh, sup->wpa, sizeof(wpapsk_t));
		}
#endif	/* BCMSUP_PSK */

		MFREE(sup->osh, sup, sizeof(*sup));
		*psup = NULL;
	}
}
#endif  /* BCMSUP_PSK */

#if defined(WOWL) && defined(BCMSUP_PSK)
#define PUTU32(ct, st) { \
		(ct)[0] = (uint8)((st) >> 24); \
		(ct)[1] = (uint8)((st) >> 16); \
		(ct)[2] = (uint8)((st) >>  8); \
		(ct)[3] = (uint8)(st); }

/**
 * For Wake-on-wireless lan, broadcast key rotation feature requires a information like
 * KEK - KCK to be programmed in the ucode
 */
void *
wlc_sup_hw_wowl_init(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	uint32 rk[4*(AES_MAXROUNDS+1)];
	int rounds;
	void *gtkp;
	int i;
	wlc_info_t *wlc;
	uint keyrc_offset, kck_offset, kek_offset;
	uint16 ram_base;

	if (!sup)
		return NULL;

	BCM_REFERENCE(rounds);

	wlc = sup->wlc;
	ram_base = wlc_read_shm(wlc, M_AESTABLES_PTR(wlc)) * 2;
	keyrc_offset = M_KEYRC_LAST(wlc);
	kck_offset = M_EAPOLMICKEY_BLK(wlc);
	kek_offset = M_KEK(wlc);

	if (D11REV_LT(sup->wlc->pub->corerev, 40)) {
		if (!ram_base)
			ram_base = WOWL_TX_FIFO_TXRAM_BASE;
	}
	else {
		if (!ram_base)
			ram_base = D11AC_WOWL_TX_FIFO_TXRAM_BASE;
	}

	/* Program last reply counter -- sup->wpa.last_replay. sup->wpa.replay is the expected
	 * value of next message while the ucode requires replay value from the last message
	 */
	wlc_copyto_shm(wlc, keyrc_offset, sup->wpa->last_replay, EAPOL_KEY_REPLAY_LEN);

	/* Prepare a dummy GTK MSG2 packet to program header for WOWL ucode */
	/* We don't care about the actual flag, we just need a dummy frame to create d11hdrs from */
	if (sup->wpa->ucipher != CRYPTO_ALGO_AES_CCM)
		gtkp = wlc_wpa_sup_prepeapol(sup, (WPA_KEY_DESC_V1), GMSG2);
	else
		gtkp = wlc_wpa_sup_prepeapol(sup, (WPA_KEY_DESC_V2), GMSG2);

	if (!gtkp)
		return NULL;

	/* Program KCK -- sup->wpa->eapol_mic_key */
	wlc_copyto_shm(wlc, kck_offset, sup->wpa->eapol_mic_key, WPA_MIC_KEY_LEN);

	/* Program KEK for WEP/TKIP (how do I find what's what) */
	/* Else program expanded key using rijndaelKeySetupEnc and program the keyunwrapping
	 * tables
	 */
	if (sup->wpa->ucipher != CRYPTO_ALGO_AES_CCM)
		wlc_copyto_shm(wlc, kek_offset, sup->wpa->eapol_encr_key, WPA_ENCR_KEY_LEN);
	else {
		rounds = rijndaelKeySetupEnc(rk, sup->wpa->eapol_encr_key,
		                             AES_KEY_BITLEN(WPA_ENCR_KEY_LEN));
		ASSERT(rounds == EXPANDED_KEY_RNDS);

		/* Convert the table to format that ucode expects */
		for (i = 0; i < (int)(EXPANDED_KEY_LEN/sizeof(uint32)); i++) {
			uint32 *v = &rk[i];
			uint8 tmp[4];

			PUTU32(tmp, rk[i]);

			*v = (uint32)*((uint32*)tmp);
		}

		/* Program the template ram with AES key unwrapping tables */
		wlc_write_shm(wlc, M_AESTABLES_PTR(wlc), ram_base);

		wlc_write_template_ram(wlc, ram_base,
		                       ARRAYSIZE(aes_xtime9dbe) * 2, (void *)aes_xtime9dbe);

		wlc_write_template_ram(wlc, ram_base +
		                       (ARRAYSIZE(aes_xtime9dbe) * 2),
		                       ARRAYSIZE(aes_invsbox) * 2,
		                       (void *)aes_invsbox);

		wlc_write_template_ram(wlc, ram_base +
		                       ((ARRAYSIZE(aes_xtime9dbe) + ARRAYSIZE(aes_invsbox)) * 2),
		                       EXPANDED_KEY_LEN, (void *)rk);
	}

	return gtkp;
}

/**
 * Update the Supplicant's software state as the key could have rotated while driver was in
 * Wake mode
 */
void
wlc_sup_sw_wowl_update(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	uint keyrc_offset;

	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (!sup)
		return;

	keyrc_offset = M_KEYRC_LAST(sup->wlc);

	/* Update the replay counter from the AP */
	wlc_copyfrom_shm(sup->wlc, keyrc_offset, sup->wpa->last_replay, EAPOL_KEY_REPLAY_LEN);

	/* Driver's copy of replay counter is one more than APs */
	bcopy(sup->wpa->last_replay, sup->wpa->replay, EAPOL_KEY_REPLAY_LEN);
	wpa_incr_array(sup->wpa->replay, EAPOL_KEY_REPLAY_LEN);
}
#endif /* WOWL && BCMSUP_PSK */

#ifdef BCMSUP_PSK
/** Get WPA-PSK Supplicant 4-way handshake timeout */
static uint32
wlc_sup_get_wpa_psk_tmo(supplicant_t *sup)
{
	return sup->wpa_psk_tmo;
}

/* External wrapper to get the above; return 0 if unavailable */
uint32
wlc_sup_get_bsscfg_wpa_psk_tmo(wlc_bsscfg_t *bsscfg)
{
	supplicant_t *sup;

	ASSERT(bsscfg && bsscfg->wlc && bsscfg->wlc->idsup);

	sup = SUP_BSSCFG_CUBBY(bsscfg->wlc->idsup, bsscfg);
	return sup ? wlc_sup_get_wpa_psk_tmo(sup) : 0;
}

/** Set WPA-PSK Supplicant 4-way handshake timeout */
static void
wlc_sup_set_wpa_psk_tmo(supplicant_t *sup, uint32 tmo)
{
	sup->wpa_psk_tmo = tmo;
	return;
}

static void
wlc_wpa_psk_timer(void *arg)
{
	supplicant_t *sup = (supplicant_t *)arg;

	if (!sup) {
		return;
	}

	sup->wpa_psk_timer_active = 0;

	if (!sup->sup_enable_wpa ||
		(sup->sup_type != SUP_WPAPSK) || !sup->wpa_psk_tmo) {
		return;
	}
	/* Report timeout event */
	if (sup->cfg->associated && sup->wpa->state != WPA_SUP_KEYUPDATE) {
		WL_SUP_ERROR(("wl%d: wlc_wpa_psk_timer: 4-way handshake timeout\n",
		         UNIT(sup)));
		wlc_wpa_send_sup_status(sup->m_handle, sup->cfg, WLC_E_SUP_WPA_PSK_TMO);
	}

	return;
}

static void
wlc_sup_wpa_psk_timer(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, bool start)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (!sup) {
		return;
	}

	if (!sup->sup_enable_wpa ||
		(sup->sup_type != SUP_WPAPSK) || !sup->wpa_psk_tmo) {
		return;
	}

	/* Stop timer */
	if (sup->wpa_psk_timer_active) {
		wl_del_timer(sup->wl, sup->wpa_psk_timer);
		sup->wpa_psk_timer_active = 0;
	}

	/* Start WPA PSK timer */
	if (start == TRUE) {
		wl_add_timer(sup->wl, sup->wpa_psk_timer, sup->wpa_psk_tmo, 0);
		sup->wpa_psk_timer_active = 1;
	}
	return;
}

static void
wlc_sup_handle_joinproc(wlc_sup_info_t *sup_info, bss_assoc_state_data_t *evt_data)
{
	if (evt_data->state == AS_JOIN_INIT)
		wlc_sup_handle_joinstart(sup_info, evt_data->cfg);
	else if (evt_data->state == AS_JOIN_ADOPT)
		wlc_sup_handle_joindone(sup_info, evt_data->cfg);
}

static void
wlc_sup_handle_joinstart(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	wlc_info_t *wlc;
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char *ssidbuf;
	const char *ssidstr;
#endif // endif

	if (!sup || !sup->sup_enable_wpa)
		return;
	wlc = sup->wlc;

#if defined(BCMDBG) || defined(WLMSG_WSEC)
	ssidbuf = (char *) MALLOC(wlc->osh, SSID_FMT_BUF_LEN);
	if (ssidbuf) {
		wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
		ssidstr = ssidbuf;
	} else
		ssidstr = "???";
#endif // endif

	sup->sup_type = SUP_UNUSED;

	if (wlc_assoc_iswpaenab(cfg, TRUE))
		sup->sup_type = SUP_WPAPSK;

	/* check if network is configured for LEAP */
	WL_WSEC(("wl%d: wlc_join_start: checking %s for LEAP\n",
	         wlc->pub->unit, (cfg->SSID_len == 0) ? "<NULL SSID>" : ssidstr));

	/* clear PMKID cache */
	if (wlc_assoc_iswpaenab(cfg, FALSE)) {
		WL_WSEC(("wl%d: wlc_join_start: clearing PMKID cache and candidate list\n",
			wlc->pub->unit));
		wlc_pmkid_clear_store(wlc->pmkid_info, cfg);
	}

#if defined(BCMDBG) || defined(WLMSG_WSEC)
	if (ssidbuf != NULL)
		 MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif // endif
}

static void
wlc_sup_handle_joindone(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	wlc_info_t *wlc;

	if (!sup || !sup->sup_enable_wpa)
		return;
	wlc = sup->wlc;

	if (SUP_ENAB(wlc->pub) && (sup->sup_type != SUP_UNUSED)) {
		bool fast_reassoc = FALSE;

#if defined(WLFBT)
		if (BSSCFG_IS_FBT(cfg) && (cfg->auth_atmptd == DOT11_FAST_BSS)) {
			fast_reassoc = TRUE;
		}
#endif /* WLFBT */

		wlc_sup_set_ea(wlc->idsup, cfg, &cfg->BSSID);
		if (!fast_reassoc) {
			uint8 *sup_ies, *auth_ies;
			uint sup_ies_len, auth_ies_len;

			WL_WSEC(("wl%d: wlc_adopt_bss: calling set sup\n",
				wlc->pub->unit));
			wlc_find_sup_auth_ies(wlc->idsup, cfg, &sup_ies, &sup_ies_len,
				&auth_ies, &auth_ies_len);
			wlc_set_sup(wlc->idsup, cfg, sup->sup_type,
				sup_ies, sup_ies_len, auth_ies, auth_ies_len);
		}
		/* Start WPA-PSK 4-way handshake timer */
		wlc_sup_wpa_psk_timer(wlc->idsup, cfg, TRUE);
	}
}

unsigned char
wlc_sup_geteaphdrver(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (sup_info->sup_wpa2_eapver == 1)
		return WPA_EAPOL_VERSION;
	else if (sup) {
		if (sup_info->sup_wpa2_eapver == -1 && sup->ap_eapver)
			return sup->ap_eapver;
	}
	return WPA2_EAPOL_VERSION;
}

/**
 * wlc_sup_up_down_register()
 *
 * This function registers a callback that will be invoked when join
 * start event occurs from assoc module.
 *
 * Parameters
 *    wlc       Common driver context.
 *    callback  Callback function  to invoke on join-start events.
 *    arg       Client specified data that will provided as param to the callback.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
int BCMATTACHFN(wlc_sup_up_down_register)(struct wlc_info *wlc, sup_init_fn_t callback,
                               void *arg)
{
	wlc_sup_info_t *sup_info = wlc->idsup;

	return (bcm_notif_add_interest(sup_info->sup_up_down_notif_hdl,
	                               (bcm_notif_client_callback)callback,
	                               arg));
}

/**
 * wlc_sup_up_down_unregister()
 *
 * This function unregisters a bsscfg up/down event callback.
 *
 * Parameters
 *    wlc       Common driver context.
 *    callback  Callback function that was previously registered.
 *    arg       Client specified data that was previously registerd.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
int BCMATTACHFN(wlc_sup_up_down_unregister)(struct wlc_info *wlc, sup_init_fn_t callback,
                                        void *arg)
{
	wlc_sup_info_t *sup_info = wlc->idsup;

	return (bcm_notif_remove_interest(sup_info->sup_up_down_notif_hdl,
	                                  (bcm_notif_client_callback)callback,
	                                  arg));
}

/** to retrieve PMK associated with PMKID. */
static bool
wlc_sup_retrieve_pmk(supplicant_t *sup, wlc_bsscfg_t *cfg, uint8 *data,
	struct ether_addr *bssid, uint8 *pmk, uint8 pmklen)
{
	uint i;
	sup_pmkid_t *pmkid = NULL;

	BCM_REFERENCE(cfg);
	BCM_REFERENCE(pmklen);

	for (i = 0; i < sup->npmkid_sup; i++) {
		pmkid = sup->pmkid_sup[i];
		if ((pmkid != NULL) &&
			!bcmp(data, pmkid->PMKID, WPA2_PMKID_LEN) &&
			!bcmp(bssid, &pmkid->BSSID, ETHER_ADDR_LEN)) {
			bcopy(pmkid->PMK, pmk, PMK_LEN);
			break;
		}
	}
	/* Check for npmkid_sup instead of npmkid else retrieving of pmkid fails */
	if (i == sup->npmkid_sup) {
		WL_SUP_ERROR(("wl%d: wlc_sup_retrieve_pmk: unrecognized"
				 " PMKID in WPA pairwise key message 1\n",
				 UNIT(sup)));
		return TRUE;
	}
	else
		return FALSE;
}

void
wlc_sup_add_pmkid(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg,
	struct ether_addr *auth_ea, uint8 *pmkid)
{
	sup_pmkid_t *sup_pmkid = NULL;
	supplicant_t *sup;
	uint i;

	sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	for (i = 0; i < sup->npmkid_sup; i++) {
		sup_pmkid = sup->pmkid_sup[i];
		if ((sup_pmkid != NULL) &&
			!bcmp(auth_ea, &sup_pmkid->BSSID, ETHER_ADDR_LEN))
			break;
	}
	if (i == sup->npmkid_sup) {
		if (sup->npmkid_sup < SUP_MAXPMKID) {
			if (!(sup_pmkid = (sup_pmkid_t *)MALLOC(sup->osh, sizeof(sup_pmkid_t)))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__, MALLOCED(sup->osh)));
				return;
			}
			sup->pmkid_sup[sup->npmkid_sup] = sup_pmkid;
			sup->npmkid_sup++;
		}
		bcopy(auth_ea, &sup_pmkid->BSSID, ETHER_ADDR_LEN);
	}
	bzero(sup_pmkid->PMK, PMK_LEN);
	bcopy(sup->wpa_info->pmk, sup_pmkid->PMK, PMK_LEN);
	bcopy(pmkid, sup_pmkid->PMKID, WPA2_PMKID_LEN);
}

/**
 *  Gets called when 'set pmk' command is given. As finishing part
 * of 'set pmk', if associated, PMKID is calculated for the associated BSSID
 */
int
wlc_sup_set_pmkid(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg, uint8 *pmk, ushort pmk_len,
	struct ether_addr *auth_ea, uint8 *pmkid_out)
{
	sup_pmkid_t *pmkid = NULL;
	uint i;
	supplicant_t *sup;
	bool allocated = 0;

	ASSERT(sup_info && cfg);
	sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	ASSERT(sup);

	if (ETHER_ISNULLADDR(auth_ea)) {
		WL_SUP_ERROR(("wl%d: wlc_sup_set_pmkid: can't calculate PMKID - NULL BSSID\n",
			UNIT(sup)));
		return BCME_BADARG;
	}

	/* Overwrite existing PMKID for the given auth_ea  */
	for (i = 0; i < sup->npmkid_sup; i++) {
		pmkid = sup->pmkid_sup[i];
		if ((pmkid != NULL) &&
			!bcmp(auth_ea, &pmkid->BSSID, ETHER_ADDR_LEN))
			break;
	}

	/* Add new PMKID to store if no existing PMKID was found */
	if (i == sup->npmkid_sup) {
		/* match not found, add a new one or
		 * overwrite the last index when pmkid_sup is full
		 */
		if (sup->npmkid_sup < SUP_MAXPMKID) {
			if (!(pmkid = (sup_pmkid_t *)MALLOC(sup->osh, sizeof(sup_pmkid_t)))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__, MALLOCED(sup->osh)));
				return BCME_NOMEM;
			}
			allocated = 1;
		}
	}

	if (allocated) {
		sup->pmkid_sup[sup->npmkid_sup] = pmkid;
		sup->npmkid_sup++;
	}

	memcpy(&pmkid->BSSID, auth_ea, ETHER_ADDR_LEN);

	/* compute PMKID and add to supplicant store */
	memset(pmkid->PMK, 0, PMK_LEN);
	memcpy(pmkid->PMK, pmk, pmk_len);

#ifdef MFP
	if (WLC_MFP_ENAB(sup->wlc->pub) && ((cfg->wsec & MFP_SHA256) ||
		((cfg->current_bss->wpa2.flags & RSN_FLAGS_SHA256))))
		kdf_calc_pmkid(auth_ea, &cfg->cur_etheraddr, pmk,
		(uint)pmk_len, pmkid->PMKID);
	else
#endif // endif
	wpa_calc_pmkid(auth_ea, &cfg->cur_etheraddr, pmk,
	               (uint)pmk_len, pmkid->PMKID);

	if (pmkid_out)
		bcopy(pmkid->PMKID, pmkid_out, WPA2_PMKID_LEN);
	return BCME_OK;
}

bool
wlc_sup_find_pmkid(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg,
struct ether_addr *bssid, uint8	*pmkid)
{
	uint	j;
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	sup_pmkid_t *pmkid_ary = NULL;

	for (j = 0; j < sup->npmkid_sup; j++) {
		pmkid_ary = sup->pmkid_sup[j];
		if ((pmkid != NULL) &&
			!bcmp(bssid, &pmkid_ary->BSSID, ETHER_ADDR_LEN)) {
			bcopy(pmkid_ary->PMKID, pmkid, WPA2_PMKID_LEN);
			return TRUE;
		}
	}
	return FALSE;
}

void
wlc_sup_clear_pmkid_store(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	uint	j;
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);

	if (!sup || !sup->sup_enable_wpa)
		return;

	for (j = 0; j < sup->npmkid_sup; j++) {
		if (sup->pmkid_sup[j] != NULL) {
			MFREE(sup->osh, sup->pmkid_sup[j], sizeof(sup_pmkid_t));
			sup->pmkid_sup[j] = NULL;
		}
	}
	sup->npmkid_sup = 0;
}

#if defined(BCMSUP_PSK) && defined(WLFBT)
void
wlc_sup_clear_replay(wlc_sup_info_t *sup_info, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(sup_info, cfg);
	if (sup && sup->sup_enable_wpa) {
		bzero(sup->wpa->replay, EAPOL_KEY_REPLAY_LEN);
		bzero(sup->wpa->last_replay, EAPOL_KEY_REPLAY_LEN);
		WL_WSEC(("Reset replay counter to 0\n"));
	}
}
#endif /* BCMSUP_PSK && WLFBT */

int
wlc_get_sup(wlc_sup_info_t *idsup, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(idsup, cfg);
	if (sup == NULL) {
		return SUP_UNUSED;
	}
	return sup->sup_type;
}

bool
wlc_get_wpa_enab(wlc_sup_info_t *idsup, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup = SUP_BSSCFG_CUBBY(idsup, cfg);
	if (sup == NULL) {
		return FALSE;
	}
	return sup->sup_enable_wpa;
}

#endif /* defined(BCMSUP_PSK) */
