/*
 * bcmwpa.h - interface definitions of shared WPA-related functions
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
 * $Id: bcmwpa.h,v 1.1 2009-11-20 18:44:18 $
 */

#ifndef _BCMWPA_H_
#define _BCMWPA_H_

#include <proto/wpa.h>
#if defined(BCMSUP_PSK) || defined(BCMCCX) || defined(BCMSUPPL)
#include <proto/eapol.h>
#endif // endif
#include <proto/802.11.h>
#ifdef WLP2P
#include <proto/p2p.h>
#endif // endif
#include <wlioctl.h>

/* Field sizes for WPA key hierarchy */
#define WPA_MIC_KEY_LEN		16
#define WPA_ENCR_KEY_LEN	16
#define WPA_TEMP_ENCR_KEY_LEN	16
#define WPA_TEMP_TX_KEY_LEN	8
#define WPA_TEMP_RX_KEY_LEN	8

#define PMK_LEN			32
#if defined(BCMCCX) || defined(BCMEXTCCX)
#define	WEP128_PTK_LEN		48
#define	WEP128_TK_LEN		13
#define	WEP1_PTK_LEN		48
#define	WEP1_TK_LEN		5
#define CKIP_PTK_LEN		48
#define CKIP_TK_LEN		16
#endif /* BCMCCX || BCMEXTCCX */
#define TKIP_PTK_LEN		64
#define TKIP_TK_LEN		32
#define AES_PTK_LEN		48
#define AES_TK_LEN		16

/* limits for pre-shared key lengths */
#define WPA_MIN_PSK_LEN		8
#define WPA_MAX_PSK_LEN		64

#ifdef WLFIPS
#define WLC_SW_KEYS(wlc, bsscfg) ((((wlc)->wsec_swkeys) || \
	((bsscfg)->wsec & (WSEC_SWFLAG | FIPS_ENABLED))))
#else
#define WLC_SW_KEYS(wlc, bsscfg) ((((wlc)->wsec_swkeys) || \
	((bsscfg)->wsec & WSEC_SWFLAG)))
#endif /* WLFIPS */

#ifdef EXT_STA
/* per-packet encryption exemption policy */
/* no exemption...follow whatever standard rules apply */
#define WSEC_EXEMPT_NO			0
/* send unencrypted */
#define WSEC_EXEMPT_ALWAYS		1
/* send unencrypted if no pairwise key */
#define WSEC_EXEMPT_NO_PAIRWISE		2
#endif /* EXT_STA */

#define WSEC_WEP_ENABLED(wsec)	((wsec) & WEP_ENABLED)
#define WSEC_TKIP_ENABLED(wsec)	((wsec) & TKIP_ENABLED)
#define WSEC_AES_ENABLED(wsec)	((wsec) & AES_ENABLED)
#ifdef BCMCCX
#define WSEC_CKIP_KP_ENABLED(wsec)	((wsec) & CKIP_KP_ENABLED)
#define WSEC_CKIP_MIC_ENABLED(wsec)	((wsec) & CKIP_MIC_ENABLED)
#define WSEC_CKIP_ENABLED(wsec)	((wsec) & (CKIP_KP_ENABLED|CKIP_MIC_ENABLED))
#ifdef BCMWAPI_WPI
#define WSEC_ENABLED(wsec) \
	((wsec) & (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED | CKIP_KP_ENABLED |	\
	  CKIP_MIC_ENABLED | SMS4_ENABLED))
#else /* BCMWAPI_WPI */
#define WSEC_ENABLED(wsec) \
		((wsec) & \
		 (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED | CKIP_KP_ENABLED | CKIP_MIC_ENABLED))
#endif /* BCMWAPI_WPI */
#else
#ifdef BCMWAPI_WPI
#define WSEC_ENABLED(wsec)	((wsec) & (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED | SMS4_ENABLED))
#else /* BCMWAPI_WPI */
#define WSEC_ENABLED(wsec)	((wsec) & (WEP_ENABLED | TKIP_ENABLED | AES_ENABLED))
#endif /* BCMWAPI_WPI */
#endif /* BCMCCX */
#define WSEC_SES_OW_ENABLED(wsec)	((wsec) & SES_OW_ENABLED)
#ifdef BCMWAPI_WAI
#define WSEC_SMS4_ENABLED(wsec)	((wsec) & SMS4_ENABLED)
#endif /* BCMWAPI_WAI */
#ifdef BCMCCX
#define IS_WPA_AUTH(auth)	((auth) == WPA_AUTH_NONE || \
				 (auth) == WPA_AUTH_UNSPECIFIED || \
				 (auth) == WPA_AUTH_PSK || \
				 (auth) == WPA_AUTH_CCKM)
#define INCLUDES_WPA_AUTH(auth)	\
			((auth) & (WPA_AUTH_NONE | WPA_AUTH_UNSPECIFIED | \
			WPA_AUTH_PSK | WPA_AUTH_CCKM))
#else
#define IS_WPA_AUTH(auth)	((auth) == WPA_AUTH_NONE || \
				 (auth) == WPA_AUTH_UNSPECIFIED || \
				 (auth) == WPA_AUTH_PSK)
#define INCLUDES_WPA_AUTH(auth)	\
			((auth) & (WPA_AUTH_NONE | WPA_AUTH_UNSPECIFIED | WPA_AUTH_PSK))
#endif /* BCMCCX */

#if defined(BCMCCX) || defined(BCMEXTCCX)
#define IS_WPA2_AUTH(auth)	((auth) == WPA2_AUTH_UNSPECIFIED || \
				 (auth) == WPA2_AUTH_PSK || \
				 (auth) == WPA2_AUTH_CCKM || \
				 (auth) == BRCM_AUTH_PSK || \
				 (auth) == BRCM_AUTH_DPT)
#define INCLUDES_WPA2_AUTH(auth) \
			((auth) & (WPA2_AUTH_UNSPECIFIED | \
				   WPA2_AUTH_PSK | \
				   WPA2_AUTH_CCKM | \
				   BRCM_AUTH_PSK | \
				   BRCM_AUTH_DPT))
#else
#define IS_WPA2_AUTH(auth)	((auth) == WPA2_AUTH_UNSPECIFIED || \
				 (auth) == WPA2_AUTH_PSK || \
				 (auth) == BRCM_AUTH_PSK || \
				 (auth) == BRCM_AUTH_DPT)
#define INCLUDES_WPA2_AUTH(auth) \
			((auth) & (WPA2_AUTH_UNSPECIFIED | \
				   WPA2_AUTH_PSK | \
				   BRCM_AUTH_PSK | \
				   BRCM_AUTH_DPT))
#endif /* BCMCCX || BCMEXTCCX */

#ifdef BCMWAPI_WAI
#define IS_WAPI_AUTH(auth)	((auth) == WAPI_AUTH_UNSPECIFIED || \
				 (auth) == WAPI_AUTH_PSK)
#define INCLUDES_WAPI_AUTH(auth) \
				((auth) & (WAPI_AUTH_UNSPECIFIED | \
					   WAPI_AUTH_PSK))
#endif /* BCMWAPI_WAI */

#if defined(BCMCCX) || defined(BCMEXTCCX)
#define IS_CCKM_AUTH(auth) ((auth) == WPA_AUTH_CCKM || (auth) == WPA2_AUTH_CCKM)
#define INCLUDES_CCKM_AUTH(auth) ((auth) & (WPA_AUTH_CCKM | WPA2_AUTH_CCKM))
#endif /* BCMCCX || BCMEXTCCX */

#define IS_WPA_AKM(akm)	((akm) == RSN_AKM_NONE || \
				 (akm) == RSN_AKM_UNSPECIFIED || \
				 (akm) == RSN_AKM_PSK)
#define IS_WPA2_AKM(akm)	((akm) == RSN_AKM_UNSPECIFIED || \
				 (akm) == RSN_AKM_PSK)
#ifdef BCMWAPI_WAI
#define IS_WAPI_AKM(akm)	((akm) == RSN_AKM_NONE || \
				 (akm) == RSN_AKM_UNSPECIFIED || \
				 (akm) == RSN_AKM_PSK)
#endif /* BCMWAPI_WAI */

/* Broadcom(OUI) authenticated key managment suite */
#define BRCM_AKM_NONE           0
#define BRCM_AKM_PSK            1       /* Proprietary PSK AKM */
#define BRCM_AKM_DPT            2       /* Proprietary DPT PSK AKM */

#define IS_BRCM_AKM(akm)        ((akm) == BRCM_AKM_PSK)

#define MAX_ARRAY 1
#define MIN_ARRAY 0

/* convert wsec to WPA mcast cipher. algo is needed only when WEP is enabled. */
#define WPA_MCAST_CIPHER(wsec, algo)	(WSEC_WEP_ENABLED(wsec) ? \
		((algo) == CRYPTO_ALGO_WEP128 ? WPA_CIPHER_WEP_104 : WPA_CIPHER_WEP_40) : \
			WSEC_TKIP_ENABLED(wsec) ? WPA_CIPHER_TKIP : \
			WSEC_AES_ENABLED(wsec) ? WPA_CIPHER_AES_CCM : \
			WPA_CIPHER_NONE)

/* Return address of max or min array depending first argument.
 * Return NULL in case of a draw.
 */
extern uint8 *BCMROMFN(wpa_array_cmp)(int max_array, uint8 *x, uint8 *y, uint len);

/* Increment the array argument */
extern void BCMROMFN(wpa_incr_array)(uint8 *array, uint len);

/* Convert WPA IE cipher suite to locally used value */
extern bool BCMROMFN(wpa_cipher)(wpa_suite_t *suite, ushort *cipher, bool wep_ok);

/* Look for a WPA IE; return it's address if found, NULL otherwise */
extern wpa_ie_fixed_t *BCMROMFN(bcm_find_wpaie)(uint8 *parse, uint len);

/* Look for a WPS IE; return it's address if found, NULL otherwise */
extern wpa_ie_fixed_t *bcm_find_wpsie(uint8 *parse, uint len);

#ifdef WLP2P
/* Look for a WiFi P2P IE; return it's address if found, NULL otherwise */
extern wifi_p2p_ie_t *bcm_find_p2pie(uint8 *parse, uint len);
/* Look for a WiFi P2P SE; return it's address if found, NULL otherwise */
#define bcm_find_p2pse(parse, len, seid)	bcm_parse_tlvs(parse, len, seid)
#endif // endif

/* Check whether the given IE looks like WFA IE with the specific type. */
extern bool bcm_is_wfa_ie(uint8 *ie, uint8 **tlvs, uint *tlvs_len, uint8 type);
/* Check whether pointed-to IE looks like WPA. */
#define bcm_is_wpa_ie(ie, tlvs, len)	bcm_is_wfa_ie(ie, tlvs, len, WFA_OUI_TYPE_WPA)
/* Check whether pointed-to IE looks like WPS. */
#define bcm_is_wps_ie(ie, tlvs, len)	bcm_is_wfa_ie(ie, tlvs, len, WFA_OUI_TYPE_WPS)
#ifdef WLP2P
/* Check whether the given IE looks like WFA P2P IE. */
#define bcm_is_p2p_ie(ie, tlvs, len)	bcm_is_wfa_ie(ie, tlvs, len, WFA_OUI_TYPE_P2P)
#endif // endif

/* Convert WPA2 IE cipher suite to locally used value */
extern bool BCMROMFN(wpa2_cipher)(wpa_suite_t *suite, ushort *cipher, bool wep_ok);

#if defined(BCMSUP_PSK) || defined(BCMSUPPL)
/* Look for an encapsulated GTK; return it's address if found, NULL otherwise */
extern eapol_wpa2_encap_data_t *BCMROMFN(wpa_find_gtk_encap)(uint8 *parse, uint len);

/* Check whether pointed-to IE looks like an encapsulated GTK. */
extern bool BCMROMFN(wpa_is_gtk_encap)(uint8 *ie, uint8 **tlvs, uint *tlvs_len);

/* Look for encapsulated key data; return it's address if found, NULL otherwise */
extern eapol_wpa2_encap_data_t *BCMROMFN(wpa_find_kde)(uint8 *parse, uint len, uint8 type);
#endif /* defined(BCMSUP_PSK) || defined(BCMSUPPL) */

#ifdef BCMSUP_PSK
/* Calculate a pair-wise transient key */
extern void BCMROMFN(wpa_calc_ptk)(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
                                   uint8 *anonce, uint8* snonce, uint8 *pmk, uint pmk_len,
                                   uint8 *ptk, uint ptk_len);

/* Compute Message Integrity Code (MIC) over EAPOL message */
extern bool BCMROMFN(wpa_make_mic)(eapol_header_t *eapol, uint key_desc, uint8 *mic_key,
                                   uchar *mic);

/* Check MIC of EAPOL message */
extern bool BCMROMFN(wpa_check_mic)(eapol_header_t *eapol, uint key_desc, uint8 *mic_key);

/* Calculate PMKID */
extern void BCMROMFN(wpa_calc_pmkid)(struct ether_addr *auth_ea, struct ether_addr *sta_ea,
                                     uint8 *pmk, uint pmk_len, uint8 *pmkid);

/* Encrypt key data for a WPA key message */
extern bool wpa_encr_key_data(eapol_wpa_key_header_t *body, uint16 key_info,
	uint8 *ekey, uint8 *gtk);

/* Decrypt key data from a WPA key message */
extern bool BCMROMFN(wpa_decr_key_data)(eapol_wpa_key_header_t *body, uint16 key_info,
                                        uint8 *ekey, uint8 *gtk);

/* Decrypt a group transient key from a WPA key message */
extern bool BCMROMFN(wpa_decr_gtk)(eapol_wpa_key_header_t *body, uint16 key_info,
                                   uint8 *ekey, uint8 *gtk);
#endif	/* BCMSUP_PSK */

extern bool BCMROMFN(bcmwpa_akm2WPAauth)(uint8 *akm, uint32 *auth, bool sta_iswpa);

extern bool BCMROMFN(bcmwpa_cipher2wsec)(uint8 *cipher, uint32 *wsec);

#endif	/* _BCMWPA_H_ */
