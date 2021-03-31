/*
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
 * $Id: wlc_sup_priv.h$
 */

#ifndef _wlc_sup_priv_h_
#define _wlc_sup_priv_h_

struct wlc_sup_info {
	/* references to driver `common' things */
	wlc_info_t *wlc;                /**< pointer to main wlc structure */
	wlc_pub_t *pub;                 /**< pointer to wlc public portion */
	void *wl;                       /**< per-port handle */
	osl_t *osh;                     /**< PKT* stuff wants this */
	bool sup_m3sec_ok;              /**< to selectively allow incorrect bit in M3 */
	int sup_wpa2_eapver;            /**< for choosing eapol version in M2 */
	bcm_notif_h                     sup_up_down_notif_hdl; /**< init notifier handle. */
	int cfgh;                       /**< bsscfg cubby handle */
};

/** Supplicant top-level structure hanging off bsscfg */
struct supplicant {
	wlc_info_t *wlc;                /**< pointer to main wlc structure */
	wlc_pub_t *pub;                 /**< pointer to wlc public portion */
	void *wl;                       /**< per-port handle */
	osl_t *osh;                     /**< PKT* stuff wants this */
	wlc_bsscfg_t *cfg;              /**< pointer to sup's bsscfg */
	wlc_sup_info_t *m_handle;       /**< module handle */

	struct ether_addr peer_ea;      /**< peer's ea */

	wpapsk_t *wpa;                  /**< volatile, initialized in set_sup */
	wpapsk_info_t *wpa_info;                /**< persistent wpa related info */
	unsigned char ap_eapver;        /**< eapol version from ap */
#if defined(BCMSUP_PSK)
	uint32 wpa_psk_tmo; /**< 4-way handshake timeout */
	uint32 wpa_psk_timer_active;    /**< 4-way handshake timer active */
	struct wl_timer *wpa_psk_timer; /**< timer for 4-way handshake */
#endif  /* BCMSUP_PSK */

	/* items common to any supplicant */
	int sup_type;                   /**< supplicant discriminator */
	bool sup_enable_wpa;            /**< supplicant WPA on/off */

	uint npmkid_sup;
	sup_pmkid_t *pmkid_sup[SUP_MAXPMKID];
};

typedef struct supplicant supplicant_t;

struct bss_sup_info {
	supplicant_t bss_priv;
};
typedef struct bss_sup_info bss_sup_info_t;

#define SUP_BSSCFG_CUBBY_LOC(sup, cfg) ((supplicant_t **)BSSCFG_CUBBY(cfg, (sup)->cfgh))
#define SUP_BSSCFG_CUBBY(sup, cfg) (*SUP_BSSCFG_CUBBY_LOC(sup, cfg))

/* Simplify maintenance of references to driver `common' items. */
#define UNIT(ptr)       ((ptr)->pub->unit)
#define CUR_EA(ptr)     (((supplicant_t *)ptr)->cfg->cur_etheraddr)
#define PEER_EA(ptr)    (((supplicant_t *)ptr)->peer_ea)
#define BSS_EA(ptr)     (((supplicant_t *)ptr)->cfg->BSSID)
#define BSS_SSID(ptr)   (((supplicant_t *)ptr)->cfg->current_bss->SSID)
#define BSS_SSID_LEN(ptr)       (((supplicant_t *)ptr)->cfg->current_bss->SSID_len)
#define OSH(ptr)        ((ptr)->osh)
#endif /* _wlc_sup_priv_h_ */
