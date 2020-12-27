/**
 * @file
 * @brief
 * wlc_auth.c -- driver-resident authenticator.
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
 * $Id: wlc_auth.c 779457 2019-09-30 08:58:10Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#if defined(BCMAUTH_PSK)

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
#include <passhash.h>
#include <sha2.h>
#include <802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wl_export.h>
#include <wlc_scb.h>
#include <wlc_keymgmt.h>
#include <wlc_wpa.h>
#include <wlc_sup.h>
#include <wlc_auth.h>
#include <bcm_mpool_pub.h>
#include <wlc_ap.h>
#include <wlc_iocv.h>
#include <aeskeywrap.h>
#include <bcmtlv.h>
#include <wlc_event_utils.h>

#ifdef MFP
#include <wlc_mfp.h>
#define AUTHMFP(a) ((a)->wlc->mfp)
#endif // endif
#include <wlc_dump.h>

#define AUTH_WPA2_RETRY		3		/* number of retry attempts */
#define AUTH_WPA2_RETRY_TIMEOUT	1000		/* 1 sec retry timeout */
#define AUTH_WPA2_GTK_ROTATION_TIME 0		/* GTK rotation disabled */
#define AUTH_WPA_BLACKLIST_MICFAIL_THRESH 0	/* MIC based blacklisting disabled */
#define AUTH_WPA_BLACKLIST_AGE 10000		/* Blacklist age in milli  seconds */

#define GMK_LEN			32
#define KEY_COUNTER_LEN		32

#define AUTH_FLAG_GTK_PLUMBED	0x1		/* GTK has been plumbed into h/w */
#define AUTH_FLAG_PMK_PRESENT	0x2		/* auth->psk contains pmk */
#define AUTH_FLAG_REKEY_ENAB	0x4		/* enable rekeying request */
#define AUTH_FLAG_AUTH_STATE	0x8		/* Authenticator enabled/disabled */

/* GTK plumbing index values */
#define GTK_ID_1	1
#define GTK_ID_2	2

/* Toggle GTK index.  Indices 1 - 3 are usable; spec recommends 1 and 2. */
#define GTK_NEXT_ID(auth)	((auth)->gtk_id == GTK_ID_1 ? GTK_ID_2 : GTK_ID_1)

/* determine which key descriptor version to use */
#define KEY_DESC(auth, scb) (SCB_SHA256(scb) ? WPA_KEY_DESC_V3 : (\
	!WSEC_TKIP_ENABLED(scb->wsec) ? WPA_KEY_DESC_V2 :  \
		WPA_KEY_DESC_V1))

#define WL_AUTH_ERROR(args)	WL_ERROR(args)
#define WL_AUTH_INFO(args)	WL_WSEC(args)

/* Authenticator top-level structure hanging off bsscfg */
#define WL_AUTH_CONFIG_BITMAP_AUTH_ENAB				0x01
#define WL_AUTH_CONFIG_BITMAP_GTK_ROTATION			0x02
#define WL_AUTH_CONFIG_BITMAP_EAPOL_RETRY_COUNT			0x04
#define WL_AUTH_CONFIG_BITMAP_EAPOL_RETRY_INTRVL		0x08
#define WL_AUTH_CONFIG_BITMAP_MIC_FAIL_BLKLST_COUNT		0x10
#define WL_AUTH_CONFIG_BITMAP_MIC_FAIL_BLKLST_AGE		0x20

typedef struct auth_config {
	uint16 bitmap;
	uint8 auth_enable;			/* Enable/Disable authentication offload */
	uint32 gtk_rot_interval;		/* GTK rotation interval in ms */
	uint32 eapol_retry_interval;		/* EAPOL Retry interval in ms */
	uint16 eapol_retry_count;		/* EAPOL Retry count */
	uint16 micfail_cnt_for_blacklist;	/* Blacklist error threshold */
	uint32 blacklist_age;			/* blacklisting Age in milli seconds */
} auth_config_t;

typedef struct authenticator {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	wlc_bsscfg_t *bsscfg;		/* pointer to auth's bsscfg */
	wlc_auth_info_t *auth_info;	/* pointer to parent module */
	struct scb *scb_auth_in_prog;	/* pointer to SCB actively being authorized */

	uint16 flags;			/* operation flags */

	/* mixed-mode WPA/WPA2 is not supported */
	int auth_type;			/* authenticator discriminator */

	/* global passphrases used for cobbling pairwise keys */
	ushort psk_len;			/* len of pre-shared key */
	uchar  psk[WSEC_MAX_PSK_LEN];	/* saved pre-shared key */

	/* group key stuff */
	uint8 gtk[TKIP_KEY_SIZE];		/* group transient key */
	uint8 gtk_id;
	uint8 gtk_key_index;
	ushort gtk_len;		/* Group (mcast) key length */
	uint8 global_key_counter[KEY_COUNTER_LEN];	/* global key counter */
	uint8 initial_gkc[KEY_COUNTER_LEN];		/* initial GKC value */
	uint8 gnonce[EAPOL_WPA_KEY_NONCE_LEN];	/* AP's group key nonce */
	uint8 gmk[GMK_LEN];			/* group master key */
	uint8 gtk_rsc[8];
	struct wl_timer *gtk_timer;
	auth_config_t *auth_config;
	wl_idauth_counters_t *auth_counters;
} authenticator_t;

#define BSS_AUTH_LOC(auth, cfg) ((authenticator_t **)BSSCFG_CUBBY(cfg, (auth)->cfg_handle))
#define BSS_AUTH(auth, cfg) (*BSS_AUTH_LOC(auth, cfg))

/* skeletion structure since authenticator_t hangs off the bsscfg, rather than wlc */
struct wlc_auth_info {
	wlc_info_t *wlc;
	int cfg_handle;			/* cfg cubby for per-BSS data */
	int scb_handle;			/* scb cubby for per-STA data */
	bcm_mp_pool_h wpa_mpool_h;	/* Memory pool for 'wpapsk_t' */
	bcm_mp_pool_h wpa_info_mpool_h;	/* Memory pool for 'wpapsk_info_t' */
};

/* scb cubby */
typedef struct scb_auth {
	uint16 flags;			/* operation flags */
	wpapsk_t *wpa;			/* volatile, initialized in set_auth */
	wpapsk_info_t *wpa_info;		/* persistent wpa related info */
} scb_auth_t;
typedef enum wl_idauth_counter_ids {
	WL_IDAUTH_CNTR_AUTH_REQ				= 1,
	WL_IDAUTH_CNTR_MIC_FAIL				= 2,
	WL_IDAUTH_CNTR_4WAYHS_FAIL			= 3,
	WL_IDAUTH_CNTR_LAST
} wl_idauth_counter_ids_t;

#define SCB_AUTH_LOC(auth, scb) ((scb_auth_t **)SCB_CUBBY(scb, (auth)->scb_handle))
#define SCB_AUTH(auth, scb) (*SCB_AUTH_LOC(auth, scb))

/* iovar table */
enum {
	IOV_IDAUTH,
#ifdef BCMDBG
	IOV_AUTH_PTK_M1,		/* send PTK M1 pkt */
	IOV_AUTH_REKEY_INIT,	/* enable grp rekey. must be done before any connection */
	IOV_AUTH_GTK_M1,		/* send GTK M1 pkt */
	IOV_AUTH_GTK_BAD_M1,	/* send bad GTK M1 pkt */
#endif /* BCMDBG */
	_IOV_AUTHENTICATOR_DUMMY	/* avoid empty enum */
};

static const bcm_iovar_t auth_iovars[] = {
	{"idauth", IOV_IDAUTH, IOVF_BSSCFG_AP_ONLY, 0, IOVT_BUFFER, BCM_XTLV_HDR_SIZE},
#ifdef BCMDBG
	{"auth_ptk_m1", IOV_AUTH_PTK_M1, IOVF_BSSCFG_AP_ONLY, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"auth_rekey_init", IOV_AUTH_REKEY_INIT, IOVF_BSSCFG_AP_ONLY, 0, IOVT_BOOL, 0},
	{"auth_gtk_m1", IOV_AUTH_GTK_M1, IOVF_BSSCFG_AP_ONLY, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"auth_gtk_bad_m1", IOV_AUTH_GTK_BAD_M1, IOVF_BSSCFG_AP_ONLY, 0,
	IOVT_BUFFER, ETHER_ADDR_LEN},
#endif /* BCMDBG */
	{NULL, 0, 0, 0, 0, 0}
};

#define AUTH_IN_PROGRESS(auth) (((auth)->scb_auth_in_prog != NULL))

static bool wlc_auth_wpapsk_start(authenticator_t *auth, uint8 *sup_ies, uint sup_ies_len,
                                  uint8 *auth_ies, uint auth_ies_len, struct scb *scb);
static bool wlc_wpa_auth_sendeapol(authenticator_t *auth, uint16 flags,
                                   wpa_msg_t msg, struct scb *scb);
static bool wlc_wpa_auth_recveapol(authenticator_t *auth, eapol_header_t *eapol,
                                   bool encrypted, struct scb *scb);

static void wlc_auth_gen_gtk(authenticator_t *auth, wlc_bsscfg_t *bsscfg);
static void wlc_auth_initialize_gkc(authenticator_t *auth);
static void wlc_auth_incr_gkc(authenticator_t *auth);
static void wlc_auth_initialize_gmk(authenticator_t *auth);
static int wlc_auth_set_ssid(authenticator_t *auth, uchar ssid[], int ssid_len, struct scb *scb);

/* scb stuff */
static int wlc_auth_scb_init(void *context, struct scb *scb);
static void wlc_auth_scb_deinit(void *context, struct scb *scb);
static int wlc_auth_prep_scb(authenticator_t *auth, struct scb *scb);
static void wlc_auth_cleanup_scb(wlc_info_t *wlc, struct scb *scb);

/* module stuff */
static int wlc_auth_down(void *handle);
static int wlc_auth_doiovar(void *handle, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
#ifdef BCMDBG
static int wlc_auth_dump(authenticator_t *auth, struct bcmstrbuf *b);
#endif /* BCMDBG */

static void wlc_auth_endassoc(authenticator_t *auth);

static void wlc_auth_gtk_timer(void *arg);

static int wlc_auth_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_auth_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_auth_bss_updn(void *ctx, bsscfg_up_down_event_data_t *notif);

static void wlc_auth_scb_state_upd(void *ctx, scb_state_upd_data_t *data);

static void wlc_auth_join_complete(authenticator_t *auth, struct ether_addr *ea, bool initialize);

static void wlc_auth_new_gtk(authenticator_t *auth);
static int wlc_auth_plumb_gtk(authenticator_t *auth, uint32 cipher);

static int wlc_idauth_iovar_handler(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool iovar_isset,
	void *params, uint p_len, void *out, uint o_len);
static int wlc_auth_config_set(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg,
	auth_config_t *auth_config);

static int wlc_auth_config_set_cb(void *ctx, const uint8 *buf, uint16 type, uint16 len);
static int wlc_auth_idauth_config_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bcm_xtlv_t *params,
	void *out, uint o_len);
static int wlc_auth_idauth_get_counters(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bcm_xtlv_t *params,
	void *out, uint o_len);
static int wlc_auth_idauth_get_peer_info(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bcm_xtlv_t *params,
	void *out, uint o_len);
static int wlc_auth_blacklist_peer(wlc_bsscfg_t *cfg, struct ether_addr *addr);
static void wlc_authenticator_detach(authenticator_t *auth);
static int wlc_auth_counter_increment(wlc_info_t *wlc,
	wlc_bsscfg_t *cfg, wl_idauth_counter_ids_t id);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* Break a lengthy passhash algorithm into smaller pieces. It is necessary
 * for dongles with under-powered CPUs.
 */
static void
wlc_auth_wpa_passhash_timer(void *arg)
{
	authenticator_t *auth = (authenticator_t *)arg;
	wlc_info_t *wlc = auth->wlc;
	struct scb *scb = auth->scb_auth_in_prog;
	wpapsk_info_t *wpa_info;
	scb_auth_t *scb_auth;

	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: SCB is null\n", wlc->pub->unit,  __FUNCTION__));
		return;
	}
	scb_auth = SCB_AUTH(auth->auth_info, scb);
	wpa_info = scb_auth->wpa_info;

	if (wpa_info == NULL) {
		WL_ERROR(("wl%d: %s: wpa_info is null\n", wlc->pub->unit,  __FUNCTION__));
		return;
	}

	if (do_passhash(&wpa_info->passhash_states, 256) == 0) {
		WL_WSEC(("wl%d: %s: Passhash is done\n", wlc->pub->unit, __FUNCTION__));
		get_passhash(&wpa_info->passhash_states, wpa_info->pmk, PMK_LEN);
		wpa_info->pmk_len = PMK_LEN;
		wlc_auth_join_complete(auth, &scb->ea, FALSE);
		return;
	}

	WL_TRACE(("wl%d: %s: passhash is in progress\n", wlc->pub->unit, __FUNCTION__));
	wl_add_timer(wlc->wl, wpa_info->passhash_timer, 0, 0);
}

static void
wlc_auth_retry_timer(void *arg)
{
	uint16 flags;
	struct scb *scb = (struct scb *)arg;
	wlc_bsscfg_t *bsscfg;
	authenticator_t *auth;
	scb_auth_t *scb_auth;
	wpapsk_t *wpa;
	wlc_info_t *wlc;
	char eabuf[ETHER_ADDR_STR_LEN];
	BCM_REFERENCE(eabuf);

	if (scb == NULL) {
		WL_ERROR(("%s: scb is null\n", __FUNCTION__));
		return;
	}

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

	wlc = bsscfg->wlc;

	if ((auth = BSS_AUTH(wlc->authi, bsscfg)) == NULL) {
		WL_ERROR(("%s: authenticator is NULL \n", __FUNCTION__));
		return;
	} else if (auth->auth_info == NULL) {
		WL_ERROR(("%s: auth->auth_info is NULL\n", __FUNCTION__));
		return;
	}

	if ((scb_auth = SCB_AUTH(wlc->authi, scb)) == NULL) {
		WL_ERROR(("%s: cubby is null\n", __FUNCTION__));
		return;
	}

	if ((wpa = scb_auth->wpa) == NULL) {
		WL_ERROR(("%s: NULL wpa\n", __FUNCTION__));
		return;
	}

	WL_WSEC(("wl%d: retry timer fired in state %d\n",
		wlc->pub->unit, wpa->state));

	flags = KEY_DESC(auth, scb);
	switch (wpa->state) {
	case WPA_AUTH_PTKINITNEGOTIATING:
		if (wpa->retries++ >= auth->auth_config->eapol_retry_count) {
			/* Send deauth when 4-way handshake time out */
			if (auth->scb_auth_in_prog) {
				wlc_auth_counter_increment(wlc, bsscfg,
					WL_IDAUTH_CNTR_4WAYHS_FAIL);
				wlc_bss_mac_event_immediate(wlc, bsscfg, WLC_E_PSK_AUTH,
					&scb->ea, WLC_E_STATUS_PSK_AUTH_WPA_TIMOUT,
					WLC_E_PSK_AUTH_SUB_EAPOL_DONE, bsscfg->auth, NULL, 0);
				WL_AUTH_ERROR(("wl%d.%d:NO EAPOL M2 Message: EAPOL retry Expired,"
					" PEER:%s\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
					bcm_ether_ntoa(&scb->ea, eabuf)));
				wlc_wpa_senddeauth(bsscfg,
					(char *)&auth->scb_auth_in_prog->ea, DOT11_RC_4WH_TIMEOUT);
			}
			wlc_auth_endassoc(auth);
			break;
		}
		/* Bump up replay counter for retried frame(as suggested by Std)
		 * XXX: This is known to cause some issues. If we decide
		 * not to bump up the replay counter, then have to make
		 * a corresponding change in the supplicant where it can
		 * accept the retried frame
		 */
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		wlc_wpa_auth_sendeapol(auth, flags, PMSG1, scb);
		break;

	case WPA_AUTH_PTKINITDONE:
		if (wpa->retries++ >= auth->auth_config->eapol_retry_count) {
			wlc_auth_counter_increment(wlc, bsscfg,
				WL_IDAUTH_CNTR_4WAYHS_FAIL);
			wlc_bss_mac_event_immediate(wlc, bsscfg, WLC_E_PSK_AUTH, &scb->ea,
				WLC_E_STATUS_PSK_AUTH_WPA_TIMOUT, WLC_E_PSK_AUTH_SUB_EAPOL_DONE,
				bsscfg->auth, NULL, 0);
			WL_AUTH_ERROR(("wl%d.%d:NO EAPOL M3 Message: EAPOL retry Expired,"
				" PEER:%s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa(&scb->ea, eabuf)));
			wlc_auth_endassoc(auth);
			break;
		}
		/* Bump up replay counter for retried frame(as suggested by Std)
		 * XXX: This is known to cause some issues. If we decide
		 * not to bump up the replay counter, then have to make
		 * a corresponding change in the supplicant where it can
		 * accept the retried frame
		 */
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		wlc_wpa_auth_sendeapol(auth, flags, PMSG3, scb);
		break;

	case WPA_AUTH_REKEYNEGOTIATING:
		if (wpa->retries++ >= auth->auth_config->eapol_retry_count) {
			wlc_auth_counter_increment(wlc, bsscfg,
				WL_IDAUTH_CNTR_4WAYHS_FAIL);
			wlc_bss_mac_event_immediate(wlc, bsscfg, WLC_E_PSK_AUTH, &scb->ea,
				WLC_E_STATUS_PSK_AUTH_GTK_REKEY_FAIL, WLC_E_PSK_AUTH_SUB_GTK_DONE,
				bsscfg->auth, NULL, 0);
			WL_AUTH_ERROR(("wl%d.%d:NO GTK Re-key M2 Message: EAPOL retry Expired,"
				" PEER:%s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				bcm_ether_ntoa(&scb->ea, eabuf)));
			wlc_auth_endassoc(auth);
			break;
		}
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		if (wpa->WPA_auth == WPA_AUTH_PSK)
			wlc_wpa_auth_sendeapol(auth, flags, GMSG1, scb);
		else
			wlc_wpa_auth_sendeapol(auth, flags, GMSG_REKEY, scb);
		break;

	default:
		break;
	}
}

static int
wlc_auth_encr_key_data(authenticator_t *auth, wpapsk_t *wpa,
	eapol_wpa_key_header_t *wpa_key, uint16 flags,
	uint eapol_ver)
{
	uint8 scrbuf[32];
	rc4_ks_t *rc4key;
	bool enc_status;
	int ret = 0;

	rc4key = MALLOCZ(auth->wlc->osh, sizeof(rc4_ks_t));

	if (!rc4key) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(auth->wlc), __FUNCTION__,
			(int)sizeof(rc4_ks_t), MALLOCED(auth->wlc->osh)));
		ret = -1;
		goto err;
	}

	/* encrypt key data field */
	if (eapol_ver == 2) {
		WL_WSEC(("%s(): EAPOL hdr version is 2, set key iv to 0\n",
			__FUNCTION__));
		memset((uchar*)wpa_key->iv, 0, 16);
	}
	else
		bcopy((uchar*)&auth->global_key_counter[KEY_COUNTER_LEN-16],
			(uchar*)wpa_key->iv, 16);

	/* Pass single buffer for data and encryption key arguments
	 * used for discarding data
	 */
	enc_status = wpa_encr_key_data(wpa_key, flags, wpa->eapol_encr_key,
		NULL, NULL, scrbuf, rc4key);

	if (!enc_status) {
		WL_ERROR(("wl%d: %s: error encrypting key "
		          "data\n", auth->wlc->pub->unit, __FUNCTION__));
		ret = -1;
	}

err:
	if (rc4key)
		MFREE(auth->wlc->osh, rc4key, sizeof(rc4_ks_t));

	return ret;
}

static int
wlc_auth_plumb_gtk(authenticator_t *auth, uint32 cipher)
{
	auth->gtk_key_index = (uint8)wlc_wpa_plumb_gtk(auth->wlc, auth->bsscfg,
		auth->gtk, auth->gtk_len, auth->gtk_id,
		cipher, NULL, 1);
	if (auth->gtk_key_index == (uint8) (-1)) {
		WL_ERROR(("wl%d: %s: invalid gtk_key_index \n",
			auth->wlc->pub->unit,  __FUNCTION__));
		auth->gtk_key_index = 0;
		return -1;
	}

	auth->flags |= AUTH_FLAG_GTK_PLUMBED;

	/* start the update timer */
	if (auth->auth_config->gtk_rot_interval) {
		auth->flags |= AUTH_FLAG_REKEY_ENAB;
		if (!auth->gtk_timer)
			auth->gtk_timer = wl_init_timer(auth->wlc->wl, wlc_auth_gtk_timer,
				auth, "auth_gtk_rotate");
		if (!auth->gtk_timer)
			WL_ERROR(("wl%d: %s: gtk timer alloc failed\n",
				auth->wlc->pub->unit, __FUNCTION__));
		else
			wl_add_timer(auth->wlc->wl, auth->gtk_timer,
				auth->auth_config->gtk_rot_interval, 0);
	}

	return 0;
}

static void
wlc_auth_new_gtk(authenticator_t *auth)
{
	if (!(auth->flags & AUTH_FLAG_GTK_PLUMBED))
		wlc_auth_initialize_gmk(auth);

	wlc_auth_gen_gtk(auth, auth->bsscfg);
	auth->gtk_id = GTK_NEXT_ID(auth);

#ifdef MFP
	if (WLC_MFP_ENAB(auth->wlc->pub))
		wlc_mfp_gen_igtk(AUTHMFP(auth), auth->bsscfg,
			auth->gmk, GMK_LEN);
#endif // endif
	return;
}

static void
wlc_auth_gtk_timer(void *arg)
{
	authenticator_t *auth = (authenticator_t *)arg;
	struct scb *scb = NULL;
	scb_auth_t *scb_auth;
	wpapsk_t *wpa;
	struct scb_iter scbiter;
	int num_scbs = 0;
	uint16 flags = 0;

	memset(&scbiter, 0, sizeof(scbiter));

	/* GTK rekey timer expired.  Set all SCBs associated with this BSS to
	 * WPA_REKEYNEGOTIATING state.  Generate and send new GTK to all SCBs
	 */
	if (!auth || !auth->wlc || !auth->bsscfg) {
		if (auth)
			WL_ERROR(("%s: NULL ptr auth->wlc: %p, auth->bsscfg: %p\n",
				__FUNCTION__, OSL_OBFUSCATE_BUF(auth->wlc),
				OSL_OBFUSCATE_BUF(auth->bsscfg)));
		else
			WL_ERROR(("%s: NULL ptr auth\n", __FUNCTION__));
		return;
	}

	FOREACH_BSS_SCB(auth->wlc->scbstate, &scbiter, auth->bsscfg, scb) {
		if (!SCB_AUTHORIZED(scb)) { /* Skip SCBs that are not authorized */
			continue;
		}
		scb_auth = SCB_AUTH(auth->auth_info, scb);
		if (scb_auth == NULL) {
			WL_ERROR(("%s: wpa is null is for scb %p\n",
				__FUNCTION__, OSL_OBFUSCATE_BUF(scb)));
			continue;
		}

		if ((wpa = scb_auth->wpa) == NULL) {
			WL_ERROR(("%s: wpa is null is for scb %p\n",
				__FUNCTION__, OSL_OBFUSCATE_BUF(scb)));
			continue;
		}

		/* Cobble the GTK for first SCB found */
		if (!num_scbs)
			wlc_auth_new_gtk(auth);

		wpa->state = WPA_AUTH_REKEYNEGOTIATING;
		flags = KEY_DESC(auth, scb);
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		wlc_wpa_auth_sendeapol(auth, flags, GMSG_REKEY, scb);
		num_scbs++;
	}

	if (num_scbs) {
		/* Start timer for the next GTK rotation */
		if (auth->auth_config->gtk_rot_interval)
			wl_add_timer(auth->wlc->wl, auth->gtk_timer,
				auth->auth_config->gtk_rot_interval, 0);
	} else {
		/* No active SCBs on this BSS */
		auth->flags &= ~AUTH_FLAG_GTK_PLUMBED;
	}
}

#ifdef WLRSDB
typedef struct {
	ushort psk_len;
	uchar  psk[WSEC_MAX_PSK_LEN];
} wlc_auth_psk_copy_t;

#define AUTH_PSK_COPY_SIZE	sizeof(wlc_auth_psk_copy_t)

static int
wlc_auth_psk_get(void *ctx, wlc_bsscfg_t *cfg, uint8 *data, int *len)
{
	wlc_auth_psk_copy_t *cp = (wlc_auth_psk_copy_t *)data;
	authenticator_t *auth = BSS_AUTH(cfg->wlc->authi, cfg);

	if (auth == NULL)
		return AUTH_UNUSED;
	cp->psk_len = auth->psk_len;
	bcopy(auth->psk, cp->psk, cp->psk_len);

	return BCME_OK;
}

static int
wlc_auth_psk_set(void *ctx, wlc_bsscfg_t *cfg, const uint8 *data, int len)
{
	const wlc_auth_psk_copy_t *cp = (const wlc_auth_psk_copy_t *)data;
	authenticator_t *auth = BSS_AUTH(cfg->wlc->authi, cfg);

	if (auth == NULL) {
		return AUTH_UNUSED;
	}

	auth->psk_len = cp->psk_len;
	bcopy(cp->psk, auth->psk, auth->psk_len);
	return BCME_OK;
}
#endif /* WLRSDB */

/* return 0 when succeeded, 1 when passhash is in progress, -1 when failed */
int
wlc_auth_set_ssid(authenticator_t *auth, uchar ssid[], int len, struct scb* scb)
{
	wpapsk_info_t *wpa_info;
	scb_auth_t *scb_auth;

	if (auth == NULL) {
		WL_WSEC(("wlc_auth_set_ssid: called with NULL auth\n"));
		return -1;
	}

	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: SCB is null\n", auth->wlc->pub->unit,  __FUNCTION__));
		return -1;
	}

	scb_auth = SCB_AUTH(auth->auth_info, scb);
	wpa_info = scb_auth->wpa_info;

	if (auth->psk_len == 0) {
		WL_WSEC(("%s: called with NULL psk\n", __FUNCTION__));
		return 0;
	} else if (wpa_info->pmk_len != 0) {
		WL_WSEC(("%s: called with non-NULL pmk\n", __FUNCTION__));
		return 0;
	}
	return wlc_wpa_cobble_pmk(wpa_info, (char *)auth->psk, auth->psk_len, ssid, len);
}

/* Allocate authenticator context, squirrel away the passed values,
 * and return the context handle.
 */
wlc_auth_info_t *
BCMATTACHFN(wlc_auth_attach)(wlc_info_t *wlc)
{
	wlc_auth_info_t *auth_info;
	bsscfg_cubby_params_t cubby_params;

	WL_TRACE(("wl%d: wlc_auth_attach\n", wlc->pub->unit));

	if (!(auth_info = (wlc_auth_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_auth_info_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* Create memory pool for 'wpapsk_t' data structs. */
	if (bcm_mpm_create_heap_pool(wlc->mem_pool_mgr, sizeof(wpapsk_t),
	                             "a_wpa", &auth_info->wpa_mpool_h) != BCME_OK) {
		WL_ERROR(("wl%d: bcm_mpm_create_heap_pool failed\n", wlc->pub->unit));
		goto err;
	}

	/* Create memory pool for 'wpapsk_info_t' data structs. */
	if (bcm_mpm_create_heap_pool(wlc->mem_pool_mgr, sizeof(wpapsk_info_t),
	                             "a_wpai", &auth_info->wpa_info_mpool_h) != BCME_OK) {
		WL_ERROR(("wl%d: bcm_mpm_create_heap_pool failed\n", wlc->pub->unit));
		goto err;
	}

	/* XXX Authenticator hangs off the bsscfg, rather than off wlc, cannot allocate/deallocate
	 * anything in scb init/deinit functions since the authenticator context (and scb handle)
	 * is tossed before the scbs are freed.
	 */

	auth_info->scb_handle =
	        wlc_scb_cubby_reserve(wlc, sizeof(scb_auth_t *), wlc_auth_scb_init,
	                              wlc_auth_scb_deinit, NULL, (void*) auth_info);

	if (auth_info->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		goto err;
	}

	/* hook up bsscfg init/deinit callbacks through cubby registration */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = auth_info;
	cubby_params.fn_init = wlc_auth_bss_init;
	cubby_params.fn_deinit = wlc_auth_bss_deinit;
#ifdef WLRSDB
	cubby_params.fn_get = wlc_auth_psk_get;
	cubby_params.fn_set = wlc_auth_psk_set;
	cubby_params.config_size = AUTH_PSK_COPY_SIZE;
#endif /* WLRSDB */

	auth_info->cfg_handle = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(authenticator_t *),
	                                                     &cubby_params);
	if (auth_info->cfg_handle < 0) {
		WL_ERROR(("wl%d: wlc_bsscfg_cubby_reserve() failed\n", wlc->pub->unit));
		goto err;
	}

	/* register scb state change callback */
	if (wlc_scb_state_upd_register(wlc, wlc_auth_scb_state_upd, auth_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scb_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	/* register bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_auth_bss_updn, auth_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, auth_iovars, "auth", auth_info, wlc_auth_doiovar,
		NULL, NULL, wlc_auth_down)) {
		WL_ERROR(("wl%d: auth wlc_module_register() failed\n", wlc->pub->unit));
		goto err;
	}

	auth_info->wlc = wlc;

#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "auth", (dump_fn_t)wlc_auth_dump, (void *)auth_info);
#endif // endif
	return auth_info;

err:
	MODULE_DETACH(auth_info, wlc_auth_detach);
	return NULL;
}

static authenticator_t *
wlc_authenticator_attach(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	authenticator_t *auth;

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (!(auth = (authenticator_t *)MALLOCZ(wlc->osh, sizeof(authenticator_t)))) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)sizeof(authenticator_t), MALLOCED(wlc->osh)));
		return NULL;
	}

	auth->wlc = wlc;
	auth->bsscfg = cfg;
	auth->auth_info = wlc->authi;
	auth->gtk_id = GTK_ID_1;

	if (!(auth->auth_config = (auth_config_t *)MALLOCZ(wlc->osh, sizeof(auth_config_t)))) {
		WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			__FUNCTION__, (int)sizeof(auth_config_t), MALLOCED(wlc->osh)));
		goto auth_detach;
	}
	auth->auth_config->blacklist_age = AUTH_WPA_BLACKLIST_AGE;
	auth->auth_config->eapol_retry_count = AUTH_WPA2_RETRY;
	auth->auth_config->eapol_retry_interval = AUTH_WPA2_RETRY_TIMEOUT;
	auth->auth_config->micfail_cnt_for_blacklist = AUTH_WPA_BLACKLIST_MICFAIL_THRESH;
	auth->auth_config->gtk_rot_interval = AUTH_WPA2_GTK_ROTATION_TIME;
	if (!(auth->auth_counters = (wl_idauth_counters_t *)MALLOCZ(wlc->osh,
			sizeof(wl_idauth_counters_t)))) {
		WL_ERROR(("wl%d: %s: MALLOC(%d) failed, malloced %d bytes\n", WLCWLUNIT(wlc),
			__FUNCTION__, (int)sizeof(wl_idauth_counters_t), MALLOCED(wlc->osh)));
		goto auth_detach;
	}
	return auth;
auth_detach:
	wlc_authenticator_detach(auth);
	return NULL;
}

/* detach the authenticator_t struct from the wlc_auth_info_t struct */
/* exists to preserve scb handle, which is needed to deinit the scbs */

static void
wlc_authenticator_detach(authenticator_t *auth)
{
	if (auth == NULL)
		return;

	if (auth->auth_config)
		MFREE(auth->wlc->osh, auth->auth_config, sizeof(*auth->auth_config));
	if (auth->auth_counters)
		MFREE(auth->wlc->osh, auth->auth_counters, sizeof(*auth->auth_counters));

	MFREE(auth->wlc->osh, auth, sizeof(authenticator_t));
	auth = NULL;
}

/* down and clean the authenticator structure which hangs off bsscfg */
static void
wlc_authenticator_down(authenticator_t *auth)
{
	if (auth == NULL)
		return;

	wlc_keymgmt_reset(auth->wlc->keymgmt, auth->bsscfg, NULL);

	if (auth->gtk_timer) {
		wl_del_timer(auth->wlc->wl, auth->gtk_timer);
		wl_free_timer(auth->wlc->wl, auth->gtk_timer);
		auth->gtk_timer = NULL;
	}

	auth->flags &= ~AUTH_FLAG_GTK_PLUMBED;
	auth->gtk_id = GTK_ID_1;
	auth->gtk_key_index = 0;
	auth->gtk_len = 0;

	memset(auth->gtk, 0, sizeof(auth->gtk));
	memset(auth->global_key_counter, 0, sizeof(auth->global_key_counter));
	memset(auth->initial_gkc, 0, sizeof(auth->initial_gkc));
	memset(auth->gtk_rsc, 0, sizeof(auth->gtk_rsc));
	memset(auth->gmk, 0, sizeof(auth->gmk));
	memset(auth->gnonce, 0, sizeof(auth->gnonce));
}

/* Toss authenticator context */
void
BCMATTACHFN(wlc_auth_detach)(wlc_auth_info_t *auth_info)
{
	wlc_info_t *wlc;

	if (!auth_info)
		return;

	wlc = auth_info->wlc;

	WL_TRACE(("wl%d: wlc_auth_detach\n", wlc->pub->unit));

	(void)wlc_scb_state_upd_unregister(wlc, wlc_auth_scb_state_upd, auth_info);

	wlc_module_unregister(wlc->pub, "auth", auth_info);

	if (auth_info->wpa_info_mpool_h != NULL) {
		bcm_mpm_delete_heap_pool(wlc->mem_pool_mgr,
		                         &auth_info->wpa_info_mpool_h);
	}

	if (auth_info->wpa_mpool_h != NULL) {
		bcm_mpm_delete_heap_pool(wlc->mem_pool_mgr, &auth_info->wpa_mpool_h);
	}

	MFREE(auth_info->wlc->osh, auth_info, sizeof(wlc_auth_info_t));
	auth_info = NULL;
}

/* JRK TBD */
static int
wlc_auth_scb_init(void *context, struct scb *scb)
{
	wlc_auth_info_t *auth_info = (wlc_auth_info_t *)context;
	wlc_info_t *wlc = auth_info->wlc;
	scb_auth_t **scb_auth_loc = SCB_AUTH_LOC(auth_info, scb);
	scb_auth_t *scb_auth = SCB_AUTH(auth_info, scb);

	/* sanity check */
	ASSERT(scb_auth == NULL);

	scb_auth = MALLOCZ(wlc->osh, sizeof(scb_auth_t));

	if (scb_auth == NULL) {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)sizeof(scb_auth_t), MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	*scb_auth_loc = scb_auth;

	return BCME_OK;
}

static void
wlc_auth_scb_deinit(void *context, struct scb *scb)
{
	wlc_auth_info_t *auth_info = (wlc_auth_info_t *)context;
	wlc_info_t *wlc = auth_info->wlc;
	authenticator_t *auth = BSS_AUTH(auth_info, SCB_BSSCFG(scb));
	scb_auth_t **scb_auth_loc = SCB_AUTH_LOC(auth_info, scb);
	scb_auth_t *scb_auth;

	wlc_auth_cleanup_scb(wlc, scb);

	if (auth && auth->scb_auth_in_prog == scb) {
		auth->scb_auth_in_prog = NULL;
	}

	scb_auth = SCB_AUTH(auth_info, scb);

	if (scb_auth != NULL) {
		MFREE(wlc->osh, scb_auth, sizeof(scb_auth_t));
		*scb_auth_loc = NULL;
	}

	return;
}

static int
wlc_auth_down(void *handle)
{
	return 0;
}

static int
wlc_auth_doiovar(void *handle, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_auth_info_t *auth_info = (wlc_auth_info_t*)handle;
	wlc_info_t *wlc = auth_info->wlc;
	authenticator_t *auth;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb;
	int32 int_val = 0;
	int err = BCME_OK;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	auth = BSS_AUTH(auth_info, bsscfg);
	if (auth == NULL) {
		return BCME_UNSUPPORTED;
	}

	BCM_REFERENCE(scb);
	BCM_REFERENCE(err);

	switch (actionid) {
	case IOV_GVAL(IOV_IDAUTH):
	case IOV_SVAL(IOV_IDAUTH):
		err = wlc_idauth_iovar_handler(wlc, bsscfg, IOV_ISSET(actionid),
			p, plen, a, (uint)alen);
		break;
#ifdef BCMDBG
	case IOV_SVAL(IOV_AUTH_PTK_M1):
		scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr*)a);
		if (!(scb && SCB_AUTHORIZED(scb) && !AUTH_IN_PROGRESS(auth))) {
			err = BCME_ERROR;
			break;
		}
		/* restart 4-way handshake */
		wlc_auth_cleanup_scb(wlc, scb);
		auth->flags &= ~AUTH_FLAG_GTK_PLUMBED;
		wlc_auth_join_complete(auth, &scb->ea, TRUE);
		break;
	case IOV_SVAL(IOV_AUTH_REKEY_INIT): {
		if (int_val) {
			if (auth->flags & AUTH_FLAG_REKEY_ENAB)
				/* already enabled */
				break;
			if (wlc_bss_assocscb_getcnt(wlc, bsscfg)) {
				/* must be enabled before association */
				err = BCME_ASSOCIATED;
				break;
			}
			/* enable tx M1 */
			auth->flags |= AUTH_FLAG_REKEY_ENAB;
		} else {	/* disable tx M1 */
			struct scb_iter scbiter;
			if (!(auth->flags & AUTH_FLAG_REKEY_ENAB))
				/* already disabled */
				break;
			if (AUTH_IN_PROGRESS(auth)) {
				err = BCME_BUSY;
				break;
			}
			/* clean up scb */
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				wlc_auth_cleanup_scb(wlc, scb);
			}
			auth->flags &= ~AUTH_FLAG_REKEY_ENAB;
		}
		break;
	}
	case IOV_SVAL(IOV_AUTH_GTK_M1):
	case IOV_SVAL(IOV_AUTH_GTK_BAD_M1): {
		uint16 flags;
		wpapsk_t *wpa;
		scb_auth_t *scb_auth;
		if (!(auth->flags & AUTH_FLAG_REKEY_ENAB)) {
			err = BCME_EPERM;
			break;
		}
		scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr*)a);
		if (!(scb && SCB_ASSOCIATED(scb) && !AUTH_IN_PROGRESS(auth))) {
			err = BCME_ERROR;
			break;
		}

		if (!bcmwpa_is_rsn_auth(bsscfg->WPA_auth) ||
			wlc_bss_assocscb_getcnt(wlc, bsscfg) != 1) {
			err = BCME_UNSUPPORTED;
			break;
		}
		/* rekey */
		flags = KEY_DESC(auth, scb);
		if (actionid == IOV_SVAL(IOV_AUTH_GTK_BAD_M1))
			/* set for negtive test */
			flags |= WPA_KEY_ERROR;
		scb_auth = SCB_AUTH(auth->auth_info, scb);
		wpa = scb_auth->wpa;
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		/* If test is positive generate a new key */
		if (!(flags & WPA_KEY_ERROR)) {
			wlc_auth_new_gtk(auth);
			if (wlc_auth_plumb_gtk(auth, wpa->mcipher) == -1) {
				err = BCME_ERROR;
				break;
			}
		}
		err = wlc_wpa_auth_sendeapol(auth, flags, GMSG_REKEY, scb);
		break;
	}
#endif	/* BCMDBG */
	default:
#ifdef BCMDBG
		err = BCME_UNSUPPORTED;
#else
		err = BCME_OK;
#endif	/* BCMDBG */
		break;
	}

	return err;
}

int
wlc_auth_set_pmk(wlc_auth_info_t *auth_info, wlc_bsscfg_t *cfg, wsec_pmk_t *pmk)
{
	authenticator_t *auth = BSS_AUTH(auth_info, cfg);

	if (auth == NULL || pmk == NULL) {
		if (auth) {
			WL_WSEC(("wl%d: %s: missing required parameter\n",
				auth->wlc->pub->unit, __FUNCTION__));
		} else {
			WL_WSEC(("%s: null auth\n", __FUNCTION__));
		}
		return BCME_BADARG;
	}

	auth->flags &= ~AUTH_FLAG_PMK_PRESENT;

	if (pmk->flags & WSEC_PASSPHRASE) {
		if (pmk->key_len < WSEC_MIN_PSK_LEN ||
		    pmk->key_len > WSEC_MAX_PSK_LEN) {
			return BCME_BADARG;
		}  else if (pmk->key_len == WSEC_MAX_PSK_LEN) {
			wpapsk_info_t psk_info; /* temp variable to hold the cobbled pmk */
			if (wlc_wpa_cobble_pmk(&psk_info, (char *)pmk->key,
			                       pmk->key_len, NULL, 0) == 0) {
				bcopy(&psk_info.pmk, auth->psk, PMK_LEN);
				auth->flags |= AUTH_FLAG_PMK_PRESENT;
				return BCME_OK;
			}
		}
	} else if (pmk->key_len == PMK_LEN) {
		auth->flags |= AUTH_FLAG_PMK_PRESENT;
	} else
		return BCME_BADARG;

	bcopy((char*)pmk->key, auth->psk, pmk->key_len);
	auth->psk_len = pmk->key_len;

	return BCME_OK;
}

static bool
wlc_wpa_auth_recveapol(authenticator_t *auth, eapol_header_t *eapol, bool encrypted,
                       struct scb *scb)
{
	uint16 flags;
	wlc_info_t *wlc = auth->wlc;
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	wpapsk_t *wpa;
	wpapsk_info_t *wpa_info;
	scb_auth_t *scb_auth;
	uint16 key_info, wpaie_len;
	char eabuf[ETHER_ADDR_STR_LEN];
	BCM_REFERENCE(eabuf);

	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: SCB is null\n", auth->wlc->pub->unit,  __FUNCTION__));
		return FALSE;
	}

	WL_WSEC(("wl%d: %s: received EAPOL_WPA_KEY packet.\n",
	         wlc->pub->unit, __FUNCTION__));

	scb_auth = SCB_AUTH(auth->auth_info, scb);
	wpa = scb_auth->wpa;
	wpa_info = scb_auth->wpa_info;

	if (wpa == NULL || wpa_info == NULL) {
		WL_ERROR(("wl%d: %s: wpa or wpa_info is null\n",
			auth->wlc->pub->unit,  __FUNCTION__));
		return FALSE;
	}

	flags = KEY_DESC(auth, scb);
	switch (wpa->WPA_auth) {
	case WPA2_AUTH_PSK:
	case WPA_AUTH_PSK:
		break;
	default:
		WL_ERROR(("wl%d: %s: unexpected... %d\n",
			wlc->pub->unit, __FUNCTION__, wpa->WPA_auth));
		ASSERT(0);
		return FALSE;
	}

	key_info = ntoh16_ua(&body->key_info);

	/* check for replay */
	if (wpa_array_cmp(MAX_ARRAY, body->replay, wpa->replay, EAPOL_KEY_REPLAY_LEN) ==
	    wpa->replay) {
#ifdef BCMDBG
		uchar *g = body->replay, *s = wpa->replay;
		WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: ignoring replay "
			"(got %02x%02x%02x%02x%02x%02x%02x%02x"
			" last saw %02x%02x%02x%02x%02x%02x%02x%02x)\n",
			wlc->pub->unit,
			g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7],
			s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]));
#endif /* BCMDBG */
		WL_AUTH_INFO(("wl%d.%d: EAPOL M2 Phase: Replay counter Error, PEER:%s\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(auth->bsscfg),
			bcm_ether_ntoa(&scb->ea, eabuf)));
		return TRUE;
	}

	switch (wpa->state) {
	case WPA_AUTH_PTKINITNEGOTIATING:

		WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: processing message 2\n",
			wlc->pub->unit));

		if (((key_info & PMSG2_REQUIRED) != PMSG2_REQUIRED) ||
		    ((key_info & PMSG2_PROHIBITED) != 0)) {
			WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: incorrect key_info 0x%x\n",
				wlc->pub->unit, key_info));
			return TRUE;
		}

		/* Save snonce and produce PTK */
		bcopy((char *)body->nonce, wpa->snonce, sizeof(wpa->anonce));

#ifdef MFP
		if (WLC_MFP_ENAB(auth->wlc->pub) && SCB_SHA256(scb)) {
			kdf_calc_ptk(&scb->ea, &auth->bsscfg->cur_etheraddr,
				wpa->anonce, wpa->snonce, wpa_info->pmk,
				wpa_info->pmk_len, wpa->eapol_mic_key, wpa->ptk_len);
		} else
#endif // endif
		if (!memcmp(&scb->ea, &auth->bsscfg->cur_etheraddr, ETHER_ADDR_LEN)) {
			/* something is wrong -- toss; this shouldn't happen */
			WL_WSEC(("wl%d: %s: invalid eapol; identical mac addrs; discard\n",
				auth->wlc->pub->unit, __FUNCTION__));
			return TRUE;
		} else {
			wpa_calc_ptk(&scb->ea, &auth->bsscfg->cur_etheraddr,
				wpa->anonce, wpa->snonce, wpa_info->pmk,
				wpa_info->pmk_len, wpa->eapol_mic_key, wpa->ptk_len);
		}

		/* check message MIC */

		/* ensure we have the MIC field */
		if ((key_info & WPA_KEY_MIC) &&
			(ntoh16(eapol->length) < OFFSETOF(eapol_wpa_key_header_t, data_len))) {
			WL_WSEC(("wl%d: %s: bad eapol - short frame, no mic.\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			WLCNTINCR(wlc->pub->_cnt->rxrunt);
			return TRUE; /* consume it */
		}

		if ((key_info & WPA_KEY_MIC) &&
		    !wpa_check_mic(eapol, key_info & (WPA_KEY_DESC_V1|WPA_KEY_DESC_V2),
		                   wpa->eapol_mic_key)) {
			/* 802.11-2007 clause 8.5.3.2 - silently discard MIC failure */
			wlc_auth_counter_increment(wlc, auth->bsscfg, WL_IDAUTH_CNTR_MIC_FAIL);
			WL_AUTH_INFO(("wl%d.%d: MIC Error, discarding pkt from PEER:%s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(auth->bsscfg),
				bcm_ether_ntoa(&scb->ea, eabuf)));
			wpa->mic_fail_cnt++;
			if (auth->auth_config->micfail_cnt_for_blacklist &&
				auth->auth_config->blacklist_age &&
				(wpa->mic_fail_cnt >
				auth->auth_config->micfail_cnt_for_blacklist)) {
				/*  */
				if (BCME_OK == wlc_auth_blacklist_peer(auth->bsscfg, &scb->ea)) {
					wlc_bss_mac_event_immediate(wlc, auth->bsscfg,
						WLC_E_PSK_AUTH, &scb->ea,
						WLC_E_STATUS_PSK_AUTH_PEER_BLACKISTED,
						WLC_E_PSK_AUTH_SUB_EAPOL_DONE,
						auth->bsscfg->auth, NULL, 0);
				}
			}
			return TRUE;
		}

		/* check the IE */
		if (ntoh16(eapol->length) < OFFSETOF(eapol_wpa_key_header_t, data)) {
			WL_WSEC(("wl%d: %s: bad eapol - short frame, no data_len.\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			WLCNTINCR(wlc->pub->_cnt->rxrunt);
			return TRUE; /* consume it */
		}

		wpaie_len = ntoh16_ua(&body->data_len);
		if (ntoh16(eapol->length) < (wpaie_len +
			OFFSETOF(eapol_wpa_key_header_t, data))) {
			WL_WSEC(("wl%d: %s: bad eapol - short frame, no wpa ie.\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			WLCNTINCR(wlc->pub->_cnt->rxrunt);
			return TRUE; /* consume it */
		}

		if (!wpaie_len || wpaie_len != wpa->sup_wpaie_len ||
		    bcmp(body->data, wpa->sup_wpaie, wpaie_len) != 0) {
			WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: wpaie does not match\n",
				wlc->pub->unit));
			wlc_auth_counter_increment(wlc, auth->bsscfg,
				WL_IDAUTH_CNTR_4WAYHS_FAIL);
			wlc_bss_mac_event_immediate(wlc, auth->bsscfg, WLC_E_PSK_AUTH, &scb->ea,
				WLC_E_STATUS_PSK_AUTH_IE_MISMATCH_ERR,
				WLC_E_PSK_AUTH_SUB_EAPOL_DONE,
				auth->bsscfg->auth, NULL, 0);
			wlc_wpa_senddeauth(auth->bsscfg, (char *)&scb->ea,
				DOT11_RC_WPA_IE_MISMATCH);
			return TRUE;
		}

		/* clear older timer */
		wpa->retries = 0;
		wpa->mic_fail_cnt = 0;
		wl_del_timer(auth->wlc->wl, wpa_info->retry_timer);

		/* if MIC was okay, increment counter */
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);

		/* send msg 3 */
		wlc_wpa_auth_sendeapol(auth, flags, PMSG3, scb);

		break;

	case WPA_AUTH_PTKINITDONE:

		WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: processing message 4\n",
			wlc->pub->unit));

		if (((key_info & PMSG4_REQUIRED) != PMSG4_REQUIRED) ||
		    ((key_info & PMSG4_PROHIBITED) != 0)) {
			WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: incorrect key_info 0x%x\n",
				wlc->pub->unit, key_info));
			return TRUE;
		}

		/* check message MIC */
		if ((key_info & WPA_KEY_MIC) &&
		    !wpa_check_mic(eapol, key_info & (WPA_KEY_DESC_V1|WPA_KEY_DESC_V2),
		                   wpa->eapol_mic_key)) {
			/* 802.11-2007 clause 8.5.3.4 - silently discard MIC failure */
			wlc_auth_counter_increment(wlc, auth->bsscfg, WL_IDAUTH_CNTR_MIC_FAIL);
			WL_AUTH_INFO(("wl%d.%d: MIC Error, discarding pkt from PEER:%s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(auth->bsscfg),
				bcm_ether_ntoa(&scb->ea, eabuf)));
			wpa->mic_fail_cnt++;
			if (auth->auth_config->micfail_cnt_for_blacklist &&
				auth->auth_config->blacklist_age &&
				(wpa->mic_fail_cnt >
				auth->auth_config->micfail_cnt_for_blacklist)) {
				/*  */
				if (BCME_OK == wlc_auth_blacklist_peer(auth->bsscfg, &scb->ea)) {
					wlc_bss_mac_event_immediate(wlc, auth->bsscfg,
						WLC_E_PSK_AUTH, &scb->ea,
						WLC_E_STATUS_PSK_AUTH_PEER_BLACKISTED,
						WLC_E_PSK_AUTH_SUB_EAPOL_DONE,
						auth->bsscfg->auth, NULL, 0);
				}
			}
			return TRUE;
		}

		/* clear older timer */
		wpa->retries = 0;
		wpa->mic_fail_cnt = 0;
		wl_del_timer(auth->wlc->wl, wpa_info->retry_timer);

		/* Plumb paired key */
		wlc_wpa_plumb_tk(wlc, auth->bsscfg, (uint8*)wpa->temp_encr_key,
			wpa->tk_len, wpa->ucipher, &scb->ea);

		if (wpa->WPA_auth == WPA2_AUTH_PSK)
			wpa->state = WPA_AUTH_KEYUPDATE;
		else if (wpa->WPA_auth == WPA_AUTH_PSK) {
			WL_WSEC(("wl%d: %s: Moving into WPA_AUTH_REKEYNEGOTIATING\n",
			         wlc->pub->unit, __FUNCTION__));
			wpa->state = WPA_AUTH_REKEYNEGOTIATING;
			/* create the GTK */
			if (!(auth->flags & AUTH_FLAG_GTK_PLUMBED)) {
				wlc_auth_new_gtk(auth);
				if (wlc_auth_plumb_gtk(auth, wpa->mcipher) == -1) {
					WL_ERROR(("%s: Failed to plumb GTK\n",
						__FUNCTION__));
					break;
				}
			}
			wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
			wlc_wpa_auth_sendeapol(auth, flags, GMSG1, scb);
		}
		if (wpa->WPA_auth == WPA2_AUTH_PSK || wpa->WPA_auth == WPA_AUTH_PSK)
			wlc_ioctl(wlc, WLC_SCB_AUTHORIZE, &scb->ea, ETHER_ADDR_LEN,
			          auth->bsscfg->wlcif);

		/* can support a new authorization now */
		if (wpa->WPA_auth == WPA2_AUTH_PSK)
			wlc_auth_endassoc(auth);
		break;

	case WPA_AUTH_REKEYNEGOTIATING:

		WL_WSEC(("wl%d: %s: Processing group key message 2\n", wlc->pub->unit,
		         __FUNCTION__));

		/* check the MIC */

		/* ensure we have the MIC field */
		if ((key_info & WPA_KEY_MIC) &&
			(ntoh16(eapol->length) < OFFSETOF(eapol_wpa_key_header_t, data_len))) {
			WL_WSEC(("wl%d: %s: bad eapol - short frame, no mic.\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			WLCNTINCR(wlc->pub->_cnt->rxrunt);
			return TRUE; /* consume it */
		}

		if ((key_info & WPA_KEY_MIC) &&
		    !wpa_check_mic(eapol, key_info & (WPA_KEY_DESC_V1|WPA_KEY_DESC_V2),
		                   wpa->eapol_mic_key)) {
			/* 802.11-2007 clause 8.5.3.4 - silently discard MIC failure */
			WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: GMSG1 - MIC failure, "
			         "discarding pkt\n",
			         wlc->pub->unit));
			wlc_auth_counter_increment(wlc, auth->bsscfg, WL_IDAUTH_CNTR_MIC_FAIL);
			WL_AUTH_INFO(("wl%d.%d: GTK M1 phase:MIC Error, PEER:%s\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(auth->bsscfg),
				bcm_ether_ntoa(&scb->ea, eabuf)));
			wpa->mic_fail_cnt++;
			if (auth->auth_config->micfail_cnt_for_blacklist &&
				auth->auth_config->blacklist_age &&
				(wpa->mic_fail_cnt >
				auth->auth_config->micfail_cnt_for_blacklist)) {
				/*  */
				if (BCME_OK == wlc_auth_blacklist_peer(auth->bsscfg, &scb->ea)) {
					wlc_bss_mac_event_immediate(wlc, auth->bsscfg,
						WLC_E_PSK_AUTH, &scb->ea,
						WLC_E_STATUS_PSK_AUTH_PEER_BLACKISTED,
						WLC_E_PSK_AUTH_SUB_GTK_DONE,
						auth->bsscfg->auth, NULL, 0);
				}
			}
			return TRUE;
		}

		/* clear older timer */
		wpa->retries = 0;
		wpa->mic_fail_cnt = 0;
		wl_del_timer(auth->wlc->wl, wpa_info->retry_timer);
		wpa->state = WPA_AUTH_KEYUPDATE;
		wlc_auth_endassoc(auth);
		wlc_bss_mac_event_immediate(wlc, auth->bsscfg, WLC_E_PSK_AUTH, &scb->ea,
			WLC_E_STATUS_SUCCESS, WLC_E_PSK_AUTH_SUB_GTK_DONE,
			auth->bsscfg->auth, NULL, 0);
		break;

	default:
		WL_WSEC(("wl%d: wlc_wpa_auth_recveapol: unexpected state\n",
			wlc->pub->unit));
	}

	return TRUE;
}

static int
auth_get_group_rsc(authenticator_t *auth, uint8 *buf, int indx)
{
	union {
		int index;
		uint8 rsc[EAPOL_WPA_KEY_RSC_LEN];
	} u;

	u.index = indx;
	if (wlc_ioctl(auth->wlc, WLC_GET_KEY_SEQ, &u, sizeof(u), auth->bsscfg->wlcif) != 0)
		return -1;

	bcopy(u.rsc, buf, EAPOL_WPA_KEY_RSC_LEN);

	return 0;
}

static int
wlc_auth_insert_gtk(authenticator_t *auth, eapol_header_t *eapol, uint16 *data_len)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	eapol_wpa2_encap_data_t *data_encap;
	uint16 len = *data_len;
	eapol_wpa2_key_gtk_encap_t *gtk_encap;

	if (auth_get_group_rsc(auth, &auth->gtk_rsc[0], auth->gtk_key_index)) {
		/* Don't use what we don't have. */
		memset(auth->gtk_rsc, 0, sizeof(auth->gtk_rsc));
	}

	/* insert GTK into eapol message */
	/*	body->key_len = htons(wpa->gtk_len); */
	/* key_len is PTK len, gtk len is implicit in encapsulation */
	data_encap = (eapol_wpa2_encap_data_t *) (body->data + len);
	data_encap->type = DOT11_MNG_PROPR_ID;
	data_encap->length = (EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
	        EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN + auth->gtk_len;
	bcopy(WPA2_OUI, data_encap->oui, DOT11_OUI_LEN);
	data_encap->subtype = WPA2_KEY_DATA_SUBTYPE_GTK;
	len += EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
	gtk_encap = (eapol_wpa2_key_gtk_encap_t *) (body->data + len);
	gtk_encap->flags = (auth->gtk_id << WPA2_GTK_INDEX_SHIFT) & WPA2_GTK_INDEX_MASK;
	bcopy(auth->gtk, gtk_encap->gtk, auth->gtk_len);
	len += auth->gtk_len + EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN;

	/* copy in the gtk rsc */
	bcopy(auth->gtk_rsc, body->rsc, sizeof(body->rsc));

	/* return the adjusted data len */
	*data_len = len;

	return (auth->gtk_len + EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN + EAPOL_WPA2_ENCAP_DATA_HDR_LEN);
}

/* Build and send an EAPOL WPA key message */
static bool
wlc_wpa_auth_sendeapol(authenticator_t *auth, uint16 flags, wpa_msg_t msg, struct scb *scb)
{
	wlc_info_t *wlc = auth->wlc;
	wpapsk_t *wpa;
	wpapsk_info_t *wpa_info;
	scb_auth_t *scb_auth;
	uint16 len, key_desc, data_len, buf_len;
	void *p = NULL;
	eapol_header_t *eapol_hdr = NULL;
	eapol_wpa_key_header_t *wpa_key = NULL;
	bool add_mic = FALSE;
	char eabuf[ETHER_ADDR_STR_LEN];
	BCM_REFERENCE(eabuf);

	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: SCB is null\n", auth->wlc->pub->unit,  __FUNCTION__));
		return FALSE;
	}

	scb_auth = SCB_AUTH(auth->auth_info, scb);
	wpa = scb_auth->wpa;
	wpa_info = scb_auth->wpa_info;

	len = EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN;
	switch (msg) {
	case PMSG1:		/* pair-wise msg 1 */
		if ((p = wlc_eapol_pktget(wlc, auth->bsscfg, &scb->ea, len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(wlc->osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero((char *)wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | PMSG1_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->tk_len, (uint8 *)&wpa_key->key_len);
		wlc_getrand(wlc, wpa->anonce, EAPOL_WPA_KEY_NONCE_LEN);
		bcopy(wpa->anonce, wpa_key->nonce, EAPOL_WPA_KEY_NONCE_LEN);
		/* move to next state */
		wpa->state = WPA_AUTH_PTKINITNEGOTIATING;
		WL_WSEC(("wl%d: wlc_wpa_auth_sendeapol: sending message 1\n",
			wlc->pub->unit));
		break;

	case PMSG3:		/* pair-wise msg 3 */
		if (wpa->WPA_auth == WPA2_AUTH_PSK) {
				flags |= PMSG3_WPA2_REQUIRED;
		} else if (wpa->WPA_auth == WPA_AUTH_PSK) {
			flags |= PMSG3_REQUIRED;
		}
		else
			/* nothing else supported for now */
			ASSERT(0);

		data_len = wpa->auth_wpaie_len;
		len += data_len;
		buf_len = 0;
		if (wpa->WPA_auth == WPA2_AUTH_PSK && (flags & WPA_KEY_ENCRYPTED_DATA)) {
			if (!(auth->flags & AUTH_FLAG_GTK_PLUMBED)) {
				/* Cobble the key and plumb it. */
				wlc_auth_new_gtk(auth);
				if (wlc_auth_plumb_gtk(auth, wpa->mcipher) == -1) {
					WL_ERROR(("%s: failed to plumb gtk\n",
						__FUNCTION__));
					break;
				}
			}
#ifdef MFP
			if (WLC_MFP_ENAB(auth->wlc->pub)) {
				wlc_key_t *igtk;
				wlc_key_info_t key_info;
				igtk = wlc_keymgmt_get_bss_tx_key(auth->wlc->keymgmt,
					auth->bsscfg, TRUE, &key_info);
				if (igtk != NULL && key_info.key_len > 0) {
					buf_len += key_info.key_len;
					buf_len += EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN + 8
						+ EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
				} else {
					WL_WSEC(("wl%d.%d: %s: no igtk for tx\n",
						WLCWLUNIT(auth->wlc), WLC_BSSCFG_IDX(auth->bsscfg),
						__FUNCTION__));
				}
			}
#endif /* MFP */

			/* add 8+8 for extra aes bytes and possible padding */
			buf_len += auth->gtk_len + EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN + 8
				+ EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
			/* The encryption result has to be 8-byte aligned */
			if (data_len % AKW_BLOCK_LEN)
				buf_len += (AKW_BLOCK_LEN - (data_len % AKW_BLOCK_LEN));
		}

		buf_len += len;

		if ((p = wlc_eapol_pktget(wlc, auth->bsscfg, &scb->ea, buf_len)) == NULL)
			break;

		eapol_hdr = (eapol_header_t *) PKTDATA(wlc->osh, p);
		eapol_hdr->length = EAPOL_WPA_KEY_LEN;
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero((char *)wpa_key, EAPOL_WPA_KEY_LEN);

		hton16_ua_store(flags, (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->tk_len, (uint8 *)&wpa_key->key_len);
		bcopy(wpa->anonce, wpa_key->nonce, EAPOL_WPA_KEY_NONCE_LEN);
		bcopy((char *)wpa->auth_wpaie, (char *)wpa_key->data,
			wpa->auth_wpaie_len);
		wpa_key->data_len = hton16(data_len);
		if (wpa->WPA_auth == WPA2_AUTH_PSK && (flags & WPA_KEY_ENCRYPTED_DATA)) {
			wlc_auth_insert_gtk(auth, eapol_hdr, &data_len);
			wpa_key->data_len = hton16(data_len);

#ifdef MFP
			if (WLC_MFP_ENAB(auth->wlc->pub) && SCB_MFP(scb)) {
				wlc_mfp_insert_igtk(AUTHMFP(auth), auth->bsscfg,
					eapol_hdr, &data_len);
				wpa_key->data_len = hton16(data_len);
			}
#endif // endif

			/* encrypt key data field */
			if (wlc_auth_encr_key_data(auth, wpa, wpa_key, flags,
				eapol_hdr->version) == -1)
			{
				WL_ERROR(("wl%d: %s: error encrypting key "
				          "data\n", wlc->pub->unit, __FUNCTION__));
				PKTFREE(wlc->osh, p, FALSE);
				p = NULL;
				break;
			}
		} /* if (flags & WPA_KEY_ENCRYPTED_DATA) */
		/* encr algorithm might change the data_len, so pick up the update */
		eapol_hdr->length += ntoh16_ua((uint8 *)&wpa_key->data_len);
		eapol_hdr->length = hton16(eapol_hdr->length);
		add_mic = TRUE;
		wpa->state = WPA_AUTH_PTKINITDONE;
		WL_WSEC(("wl%d: %s: sending message 3\n",
		         wlc->pub->unit, __FUNCTION__));
		break;

	case GMSG1: /* WPA_REKEYNEGOTIATING */ {
		int key_index;
		ASSERT(wpa->WPA_auth == WPA_AUTH_PSK);
		flags |= GMSG1_REQUIRED;
		key_index = (auth->gtk_id << WPA_KEY_INDEX_SHIFT) & WPA_KEY_INDEX_MASK;
		flags |= key_index;
		WL_WSEC(("auth->gtk_id is %u, key_index %u\n", auth->gtk_id, key_index));
		data_len = 0;
		/* make sure to pktget the EAPOL frame length plus the length of the body */
		len += auth->gtk_len;

		if (flags & WPA_KEY_DESC_V2_OR_V3)
			len += 8;

		if ((p = wlc_eapol_pktget(wlc, auth->bsscfg, &scb->ea, len)) == NULL)
			break;

		eapol_hdr = (eapol_header_t *) PKTDATA(wlc->osh, p);
		eapol_hdr->length = EAPOL_WPA_KEY_LEN;
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero((char *)wpa_key, len - EAPOL_HEADER_LEN);
		hton16_ua_store(flags, (uint8 *)&wpa_key->key_info);
		hton16_ua_store(auth->gtk_len, (uint8 *)&wpa_key->key_len);
		bcopy(auth->gnonce, wpa_key->nonce, EAPOL_WPA_KEY_NONCE_LEN);
		bcopy(&auth->global_key_counter[KEY_COUNTER_LEN-16], wpa_key->iv, 16);
		bcopy(auth->gtk_rsc, wpa_key->rsc, sizeof(wpa_key->rsc));

		data_len = auth->gtk_len;
		bcopy(auth->gtk, wpa_key->data, data_len);
		wpa_key->data_len = hton16(data_len);

		if (wlc_auth_encr_key_data(auth, wpa, wpa_key,
			(flags&(WPA_KEY_DESC_V1|WPA_KEY_DESC_V2)),
			eapol_hdr->version) == -1)
		{
			WL_ERROR(("%s: failed to encrypt key data\n", __FUNCTION__));
			PKTFREE(wlc->osh, p, FALSE);
			p = NULL;
			break;
		}
		eapol_hdr->length += ntoh16_ua((uint8 *)&wpa_key->data_len);
		eapol_hdr->length = hton16(eapol_hdr->length);
		add_mic = TRUE;
		wpa->state = WPA_AUTH_REKEYNEGOTIATING;
		WL_WSEC(("wl%d: wlc_wpa_auth_sendeapol: sending message G1\n",
			wlc->pub->unit));
		break;
	}

	case GMSG_REKEY: {	/* sending gtk rekeying request */
#ifdef BCMDBG
		bool neg_test;

		neg_test = (flags & WPA_KEY_ERROR) ? TRUE : FALSE;
		flags &= ~WPA_KEY_ERROR;
#endif /* BCMDBG */

		if (wpa->WPA_auth != WPA2_AUTH_PSK)
			return FALSE;

		flags |= (GMSG1_REQUIRED | WPA_KEY_ENCRYPTED_DATA);

		data_len = wpa->auth_wpaie_len;
		len += data_len;
		buf_len = 0;

#ifdef MFP
		if (WLC_MFP_ENAB(auth->wlc->pub)) {
			buf_len += AES_TK_LEN;
			buf_len += EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN + 8 +
				EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
		}
#endif // endif
		/* add 8+8 for extra aes bytes and possible padding */
		buf_len += auth->gtk_len + EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN + 8 +
			EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
		/* The encryption result has to be 8-byte aligned */
		if (data_len % AKW_BLOCK_LEN)
			buf_len += (AKW_BLOCK_LEN - (data_len % AKW_BLOCK_LEN));

		buf_len += len;

		if ((p = wlc_eapol_pktget(wlc, auth->bsscfg, &scb->ea, buf_len)) == NULL)
			break;

		eapol_hdr = (eapol_header_t *) PKTDATA(wlc->osh, p);
		eapol_hdr->length = EAPOL_WPA_KEY_LEN;
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero((char *)wpa_key, EAPOL_WPA_KEY_LEN);

		hton16_ua_store(flags, (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->tk_len, (uint8 *)&wpa_key->key_len);
		bcopy(wpa->anonce, wpa_key->nonce, EAPOL_WPA_KEY_NONCE_LEN);
		bcopy((char *)wpa->auth_wpaie, (char *)wpa_key->data, wpa->auth_wpaie_len);
		wpa_key->data_len = hton16(data_len);

#ifdef BCMDBG
		if (!neg_test) {
#endif /* BCMDBG */
			/* for negtive test not include gtk encapsulation */
			wlc_auth_insert_gtk(auth, eapol_hdr, &data_len);
			wpa_key->data_len = hton16(data_len);
#ifdef BCMDBG
		}
#endif /* BCMDBG */

#ifdef MFP
		if (WLC_MFP_ENAB(auth->wlc->pub)) {
			wlc_mfp_insert_igtk(AUTHMFP(auth), auth->bsscfg, eapol_hdr, &data_len);
			wpa_key->data_len = hton16(data_len);
		}
#endif // endif

		if (wlc_auth_encr_key_data(auth, wpa, wpa_key, flags, eapol_hdr->version) == -1) {
			WL_ERROR(("%s: failed to encrypt key data\n", __FUNCTION__));
			PKTFREE(wlc->osh, p, FALSE);
			p = NULL;
			break;
		}

		/* encr algorithm might change the data_len, so pick up the update */
		eapol_hdr->length += ntoh16_ua((uint8 *)&wpa_key->data_len);
		eapol_hdr->length = hton16(eapol_hdr->length);
		add_mic = TRUE;
#ifdef BCMDBG
		if (!neg_test)
#endif /* BCMDBG */
			wpa->state = WPA_AUTH_REKEYNEGOTIATING;
		WL_WSEC(("wl%d: %s: sending group rekeying message\n",
		         wlc->pub->unit, __FUNCTION__));
		break;
	}

	default:
		WL_ERROR(("wl%d: %s: unexpected message type %d\n",
		         wlc->pub->unit, __FUNCTION__, msg));
		break;
	}

	if (p != NULL) {
		/* do common message fields here; make and copy MIC last. */
		eapol_hdr->type = EAPOL_KEY;
		if (wpa->WPA_auth == WPA2_AUTH_PSK)
			wpa_key->type = EAPOL_WPA2_KEY;
		else
			wpa_key->type = EAPOL_WPA_KEY;
		bcopy((char *)wpa->replay, (char *)wpa_key->replay,
		      EAPOL_KEY_REPLAY_LEN);
		key_desc = flags & (WPA_KEY_DESC_V1 |  WPA_KEY_DESC_V2);
		if (add_mic) {
			if (!wpa_make_mic(eapol_hdr, key_desc, wpa->eapol_mic_key,
					wpa_key->mic)) {
				WL_WSEC(("wl%d: %s: MIC generation failed\n",
				         wlc->pub->unit, __FUNCTION__));
				return FALSE;
			}
		}

		wlc_sendpkt(wlc, p, auth->bsscfg->wlcif);

		/* start the retry timer */
		wl_add_timer(wlc->wl, wpa_info->retry_timer,
			auth->auth_config->eapol_retry_interval, 0);
		if (wpa->retries == 0) {
			switch (msg) {
				case PMSG1:
					wlc_bss_mac_event_immediate(wlc, auth->bsscfg,
						WLC_E_PSK_AUTH, &scb->ea, WLC_E_STATUS_SUCCESS,
						WLC_E_PSK_AUTH_SUB_EAPOL_START,
						auth->bsscfg->auth, NULL, 0);
					break;
				case PMSG3:
					WL_AUTH_INFO(("wl%d.%d:EAPOL M3 Phase:"
						" M3 Message Sent Successfully,"
						" PEER:%s\n",
						wlc->pub->unit, WLC_BSSCFG_IDX(auth->bsscfg),
						bcm_ether_ntoa(&scb->ea, eabuf)));
					break;
				case GMSG1:
				case GMSG_REKEY:
					WL_AUTH_INFO(("wl%d.%d: GTK Rekey: M1 Sent, PEER:%s\n",
						wlc->pub->unit, WLC_BSSCFG_IDX(auth->bsscfg),
						bcm_ether_ntoa(&scb->ea, eabuf)));
					break;
				default:
					return TRUE;

			}
		}

		return TRUE;
	}
	return FALSE;
}

static bool
wlc_auth_wpapsk_start(authenticator_t *auth, uint8 *sup_ies, uint sup_ies_len,
                      uint8 *auth_ies, uint auth_ies_len, struct scb *scb)
{
	wpapsk_info_t *wpa_info;
	wpapsk_t *wpa;
	scb_auth_t *scb_auth;
	uint16 flags;
	bool ret = TRUE;

	if (sup_ies == NULL) {
		return FALSE;
	}
	if (scb == NULL) {
		WL_ERROR(("wl%d: %s: SCB is null\n", auth->wlc->pub->unit,  __FUNCTION__));
		return FALSE;
	}

	/* find the cubby */
	scb_auth = SCB_AUTH(auth->auth_info, scb);
	wpa = scb_auth->wpa;
	wpa_info = scb_auth->wpa_info;

	wlc_wpapsk_free(auth->wlc, wpa);

	wpa->state = WPA_AUTH_INITIALIZE;
	/*
	 * XXX: you can't just inherit all the wpa_auth capabilities of the bsscfg,
	 * need to pick the right wpa auth for the current association
	 * and go from there
	*/
	{
	wpa_ie_fixed_t *ie = (wpa_ie_fixed_t *)sup_ies;
	if (ie->tag == DOT11_MNG_RSN_ID)
		wpa->WPA_auth = auth->bsscfg->WPA_auth & WPA2_AUTH_PSK;
	else
		wpa->WPA_auth = auth->bsscfg->WPA_auth & WPA_AUTH_PSK;
	}

	if (!wlc_wpapsk_start(auth->wlc, wpa, sup_ies, sup_ies_len,
		auth_ies, auth_ies_len)) {
		WL_ERROR(("wl%d: wlc_wpapsk_start() failed\n",
		        auth->wlc->pub->unit));
		return FALSE;
	}

	if ((auth->auth_type == AUTH_WPAPSK) && (wpa_info->pmk_len == 0)) {
		WL_WSEC(("wl%d: %s: no PMK material found\n",
		         auth->wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}

	/* clear older timer */
	wpa->retries = 0;
	wpa->mic_fail_cnt = 0;
	wl_del_timer(auth->wlc->wl, wpa_info->retry_timer);

	wpa->state = WPA_AUTH_PTKSTART;
	flags = KEY_DESC(auth, scb);
	wlc_wpa_auth_sendeapol(auth, flags, PMSG1, scb);

	return ret;
}

bool
wlc_set_auth(wlc_auth_info_t *auth_info, wlc_bsscfg_t *cfg, int auth_type,
	uint8 *sup_ies, uint sup_ies_len, uint8 *auth_ies, uint auth_ies_len, struct scb *scb)
{
	authenticator_t *auth = BSS_AUTH(auth_info, cfg);
	bool ret = TRUE;

	if (auth == NULL) {
		WL_WSEC(("wlc_set_auth called with NULL auth context\n"));
		return FALSE;
	}

	/* sanity */
	/* ASSERT(auth->auth_type == AUTH_UNUSED); */

	if (auth_type == AUTH_WPAPSK) {
		auth->auth_type = auth_type;
		ret = wlc_auth_wpapsk_start(auth, sup_ies, sup_ies_len, auth_ies, auth_ies_len,
		                            scb);
	} else {
		WL_ERROR(("wl%d: %s: unexpected auth type %d\n",
		         auth->wlc->pub->unit, __FUNCTION__, auth_type));
		return FALSE;
	}
	return ret;
}

/* Dispatch EAPOL to authenticator.
 * Return boolean indicating whether it should be freed or sent up.
 */
bool
wlc_auth_eapol(wlc_auth_info_t *auth_info, wlc_bsscfg_t *cfg,
	eapol_header_t *eapol_hdr, bool encrypted, struct scb *scb)
{
	authenticator_t *auth = BSS_AUTH(auth_info, cfg);

	if (!auth) {
		/* no unit to report if this happens */
		WL_ERROR(("%s: called with NULL auth\n", __FUNCTION__));
		return FALSE;
	}

	if ((eapol_hdr->type == EAPOL_KEY) && (auth->auth_type == AUTH_WPAPSK)) {
		eapol_wpa_key_header_t *body;

		/* ensure we have all of fixed eapol_wpa_key_header_t fields */
		if (ntoh16(eapol_hdr->length) < OFFSETOF(eapol_wpa_key_header_t, mic)) {
			WL_WSEC(("wl%d: %s: bad eapol - header too small.\n",
				WLCWLUNIT(auth->wlc), __FUNCTION__));
			WLCNTINCR(auth->wlc->pub->_cnt->rxrunt);
			return FALSE;
		}

		body = (eapol_wpa_key_header_t *)eapol_hdr->body;
		if (body->type == EAPOL_WPA2_KEY || body->type == EAPOL_WPA_KEY) {
			wlc_wpa_auth_recveapol(auth, eapol_hdr, encrypted, scb);
			return TRUE;
		}
	}

	return FALSE;
}

static void
wlc_auth_join_complete(authenticator_t *auth, struct ether_addr *ea, bool initialize)
{
	wlc_info_t *wlc = auth->wlc;
	wlc_bsscfg_t *bsscfg = auth->bsscfg;
	struct scb *scb;
	wpapsk_info_t *wpa_info;
	scb_auth_t *scb_auth;
	uint auth_ies_len;
	uint8 *auth_ies;
	bool stat = 0;

	scb = wlc_scbfindband(wlc, bsscfg, ea,
	                      CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec));
	if (!scb) {
#ifdef BCMDBG_ERR
		char eabuf[ETHER_ADDR_STR_LEN];
		WL_ERROR(("wl%d: %s: scb not found for ea %s\n",
		          wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(ea, eabuf)));
#endif // endif
		return;
	}

	/* can only support one STA authenticating at a time */
	if (initialize && AUTH_IN_PROGRESS(auth)) {
		WL_ERROR(("wl%d: %s: Authorization blocked for current authorization in progress\n",
		         auth->wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (initialize) {
		if (wlc_auth_prep_scb(auth, scb) != BCME_OK)
			return;
	}

	scb_auth = SCB_AUTH(auth->auth_info, scb);
	wpa_info = scb_auth->wpa_info;

	if (!(auth->flags & AUTH_FLAG_GTK_PLUMBED))
		wlc_auth_initialize_gkc(auth);

	/* init per scb WPA_auth */
	scb->WPA_auth = bsscfg->WPA_auth;

	auth->scb_auth_in_prog = scb;

	/* auth->psk is pmk */
	if (auth->flags & AUTH_FLAG_PMK_PRESENT) {
		bcopy(auth->psk, wpa_info->pmk, PMK_LEN);
		wpa_info->pmk_len = PMK_LEN;
	}

	/* kick off authenticator */
	if (wpa_info->pmk_len == PMK_LEN) {
		uint8 *wpaie = NULL;
		uint wpaie_len = 0;

		auth_ies_len = wlc->pub->bcn_tmpl_len;
		if ((auth_ies = (uint8 *)MALLOC(wlc->osh, auth_ies_len)) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return;
		}

		wlc_bcn_prb_body(wlc, FC_PROBE_RESP, SCB_BSSCFG(scb),
		                 auth_ies, (int *)&auth_ies_len);
#ifdef AP
		wlc_ap_find_wpaie(wlc->ap, scb, &wpaie, &wpaie_len);
#endif // endif
		stat = wlc_set_auth(wlc->authi, bsscfg, AUTH_WPAPSK,
		                    wpaie, wpaie_len,
		                    (uint8 *)auth_ies + sizeof(struct dot11_bcn_prb),
		                    auth_ies_len - sizeof(struct dot11_bcn_prb),
		                    auth->scb_auth_in_prog);
		if (!stat) {
			WL_ERROR(("wl%d: %s: 4-way handshake config problem\n",
			          wlc->pub->unit, __FUNCTION__));
		}

		auth_ies_len = wlc->pub->bcn_tmpl_len;
		MFREE(wlc->osh, (void *)auth_ies, auth_ies_len);
	}
	/* derive pmk from psk */
	else {
		wlc_auth_set_ssid(auth,
		                  (uchar *)&bsscfg->SSID, bsscfg->SSID_len, scb);
	}
}

static void
wlc_auth_gen_gtk(authenticator_t *auth, wlc_bsscfg_t *bsscfg)
{
	unsigned char prefix[] = "Group key expansion";
	bcm_const_xlvp_t pfxs[3];
	int npfxs = 0;

	/* Select a mcast cipher: only support wpa for now, otherwise change alg field */
	switch (WPA_MCAST_CIPHER(bsscfg->wsec, 0)) {
	case WPA_CIPHER_TKIP:
		WL_WSEC(("%s: TKIP\n", __FUNCTION__));
		auth->gtk_len = TKIP_TK_LEN;
		break;
	case WPA_CIPHER_AES_CCM:
		WL_WSEC(("%s: AES\n",  __FUNCTION__));
		auth->gtk_len = AES_TK_LEN;
		break;
	default:
		WL_WSEC(("%s: not supported multicast cipher\n", __FUNCTION__));
		return;
	}
	WL_WSEC(("%s: gtk_len %d\n", __FUNCTION__, auth->gtk_len));

	pfxs[npfxs].len = sizeof(prefix) - 1;
	pfxs[npfxs++].data = (uint8 *)prefix;

	pfxs[npfxs].len = ETHER_ADDR_LEN;
	pfxs[npfxs++].data = (const uint8 *)&bsscfg->cur_etheraddr;

	bcopy(auth->global_key_counter, auth->gnonce, EAPOL_WPA_KEY_NONCE_LEN);
	wlc_auth_incr_gkc(auth);

	pfxs[npfxs].len = EAPOL_WPA_KEY_NONCE_LEN;
	pfxs[npfxs++].data = (const uint8 *)&auth->gnonce;

	/* generate the GTK */
	(void)hmac_sha2_n(HASH_SHA1, auth->gmk, (int)sizeof(auth->gmk),
		pfxs, npfxs, NULL, 0, auth->gtk, auth->gtk_len);

	/* The driver clears the IV when it gets a new key, so
	 * clearing RSC should be consistent with that, right?
	 */
	memset(auth->gtk_rsc, 0, sizeof(auth->gtk_rsc));

	WL_WSEC(("%s: done\n", __FUNCTION__));
}

/* generate the initial global_key_counter */
#define AUTH_GKC_KEY_LEN 32
static void
wlc_auth_initialize_gkc(authenticator_t *auth)
{
	wlc_info_t *wlc = auth->wlc;
	unsigned char key[AUTH_GKC_KEY_LEN];
	unsigned char prefix[] = "Init Counter";
	bcm_const_xlvp_t pfx;

	wlc_getrand(wlc, &key[0], 16);
	wlc_getrand(wlc, &key[16], 16);

	/* Still not exactly right, but better. */
	pfx.len = sizeof(prefix) - 1;
	pfx.data  = prefix;
	(void)hmac_sha2_n(HASH_SHA256, key, (int)sizeof(key), &pfx, 1,
		(const uint8 *)&auth->bsscfg->cur_etheraddr, ETHER_ADDR_LEN,
		auth->global_key_counter, KEY_COUNTER_LEN);

	memcpy(auth->initial_gkc, auth->global_key_counter, KEY_COUNTER_LEN);
}

static void
wlc_auth_incr_gkc(authenticator_t *auth)
{
	wpa_incr_array(auth->global_key_counter, KEY_COUNTER_LEN);

	/* if key counter is now equal to the original one, reset it */
	if (!bcmp(auth->global_key_counter, auth->initial_gkc, KEY_COUNTER_LEN))
		wlc_auth_initialize_gmk(auth);
}

static void
wlc_auth_initialize_gmk(authenticator_t *auth)
{
	wlc_info_t *wlc = auth->wlc;
	unsigned char *gmk = (unsigned char *)auth->gmk;

	wlc_getrand(wlc, &gmk[0], 16);
	wlc_getrand(wlc, &gmk[16], 16);
}

#ifdef BCMDBG
static int
wlc_auth_dump(authenticator_t *auth, struct bcmstrbuf *b)
{
	return 0;
}
#endif /* BCMDBG */

static int
wlc_auth_prep_scb(authenticator_t *auth, struct scb *scb)
{
	wlc_auth_info_t *auth_info = auth->auth_info;
	scb_auth_t *scb_auth = SCB_AUTH(auth_info, scb);

	if (!(scb_auth->wpa = bcm_mp_alloc(auth_info->wpa_mpool_h))) {
		WL_ERROR(("wl%d: %s: bcm_mp_alloc() of wpa failed\n",
		          auth->wlc->pub->unit, __FUNCTION__));
		goto err;
	}
	bzero(scb_auth->wpa, sizeof(wpapsk_t));

	if (!(scb_auth->wpa_info = bcm_mp_alloc(auth_info->wpa_info_mpool_h))) {
		WL_ERROR(("wl%d: %s: bcm_mp_alloc() of wpa_info failed\n",
		          auth->wlc->pub->unit, __FUNCTION__));
		goto err;
	}
	bzero(scb_auth->wpa_info, sizeof(wpapsk_info_t));
	scb_auth->wpa_info->wlc = auth->wlc;

	if (!(scb_auth->wpa_info->passhash_timer =
	      wl_init_timer(auth->wlc->wl, wlc_auth_wpa_passhash_timer, auth,
	                    "passhash"))) {
		WL_ERROR(("wl%d: %s: passhash timer "
		          "failed\n", auth->wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	if (!(scb_auth->wpa_info->retry_timer =
	      wl_init_timer(auth->wlc->wl, wlc_auth_retry_timer, scb, "auth_retry"))) {
		WL_ERROR(("wl%d: %s: retry timer failed\n",
		          auth->wlc->pub->unit, __FUNCTION__));
		goto err;
	}

	return BCME_OK;

err:
	if (scb_auth->wpa_info) {
		if (scb_auth->wpa_info->passhash_timer) {
			wl_free_timer(auth->wlc->wl, scb_auth->wpa_info->passhash_timer);
			scb_auth->wpa_info->passhash_timer = NULL;
		}
		if (scb_auth->wpa_info->retry_timer) {
			wl_free_timer(auth->wlc->wl, scb_auth->wpa_info->retry_timer);
			scb_auth->wpa_info->retry_timer = NULL;
		}
		bcm_mp_free(auth_info->wpa_info_mpool_h, scb_auth->wpa_info);
		scb_auth->wpa_info = NULL;
	}

	if (scb_auth->wpa) {
		bcm_mp_free(auth_info->wpa_mpool_h, scb_auth->wpa);
		scb_auth->wpa = NULL;
	}
	return BCME_NOMEM;
}

static void
wlc_auth_endassoc(authenticator_t *auth)
{
	WL_TRACE(("Wl%d: %s: ENTER\n", auth->wlc->pub->unit, __FUNCTION__));

	if (!auth->scb_auth_in_prog)
		return;

	if (!(auth->flags & AUTH_FLAG_REKEY_ENAB))
		wlc_auth_cleanup_scb(auth->wlc, auth->scb_auth_in_prog);
	auth->scb_auth_in_prog = NULL;
}

static void
wlc_auth_cleanup_scb(wlc_info_t *wlc, struct scb *scb)
{
	scb_auth_t *scb_auth;

	WL_TRACE(("wl%d: %s: Freeing SCB data at 0x%p\n",
		wlc->pub->unit, __FUNCTION__, OSL_OBFUSCATE_BUF(scb)));
	scb_auth = SCB_AUTH(wlc->authi, scb);

	if (scb_auth == NULL)
		return;

	if (scb_auth->wpa) {
		wlc_wpapsk_free(wlc, scb_auth->wpa);
		bcm_mp_free(wlc->authi->wpa_mpool_h, scb_auth->wpa);
		scb_auth->wpa = NULL;
	}

	if (scb_auth->wpa_info) {
		if (scb_auth->wpa_info->passhash_timer) {
			wl_del_timer(wlc->wl, scb_auth->wpa_info->passhash_timer);
			wl_free_timer(wlc->wl, scb_auth->wpa_info->passhash_timer);
			scb_auth->wpa_info->passhash_timer = NULL;
		}
		if (scb_auth->wpa_info->retry_timer) {
			wl_del_timer(wlc->wl, scb_auth->wpa_info->retry_timer);
			wl_free_timer(wlc->wl, scb_auth->wpa_info->retry_timer);
			scb_auth->wpa_info->retry_timer = NULL;
		}
		bcm_mp_free(wlc->authi->wpa_info_mpool_h, scb_auth->wpa_info);
		scb_auth->wpa_info = NULL;
	}
}

void wlc_auth_tkip_micerr_handle(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	if (wlc_keymgmt_tkip_cm_enabled(wlc->keymgmt, bsscfg)) {
		struct scb *scb;
		struct scb_iter scbiter;

		/* deauth all client */
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (SCB_ASSOCIATED(scb)) {
				WL_TRACE(("\n   DEAUTH: %02x:%02x:%02x:%02x:%02x:%02x!",
				          scb->ea.octet[0], scb->ea.octet[1],
				          scb->ea.octet[2],
				          scb->ea.octet[3], scb->ea.octet[4],
				          scb->ea.octet[5]));

				wlc_scb_set_auth(wlc, bsscfg, scb,
				                 FALSE, AUTHENTICATED, DOT11_RC_AUTH_INVAL);
			}
		}
	}
}

static int
wlc_auth_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_auth_info_t *auth_info = (wlc_auth_info_t *)ctx;
	wlc_info_t *wlc = auth_info->wlc;
	authenticator_t **pauth = BSS_AUTH_LOC(auth_info, cfg);
	authenticator_t *auth = BSS_AUTH(auth_info, cfg);

	if (!BSSCFG_AP(cfg)) {
		return BCME_OK;
	}

	/* XXX: This should move to a spot where it is dynamically attached
	 * and detached based on the security settings... ToDo
	 */
	ASSERT(auth == NULL);

	if (BCMAUTH_PSK_ENAB(wlc->pub) && !(cfg->flags & WLC_BSSCFG_NO_AUTHENTICATOR)) {
		if ((auth = wlc_authenticator_attach(wlc, cfg)) == NULL) {
			WL_ERROR(("wl%d: %s: wlc_authenticator_attach failed\n",
				wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}
		*pauth = auth;
	}

	return BCME_OK;
}

static void
wlc_auth_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_auth_info_t *auth_info = (wlc_auth_info_t *)ctx;
	wlc_info_t *wlc = auth_info->wlc;
	authenticator_t **pauth = BSS_AUTH_LOC(auth_info, cfg);
	authenticator_t *auth = BSS_AUTH(auth_info, cfg);

	BCM_REFERENCE(wlc);

	/* free the authenticator */
	if (BCMAUTH_PSK_ENAB(wlc->pub) && auth != NULL) {
		wlc_authenticator_detach(auth);
		*pauth = NULL;
	}
}

static void
wlc_auth_bss_updn(void *ctx, bsscfg_up_down_event_data_t *notif)
{
	wlc_auth_info_t *auth_info = (wlc_auth_info_t *)ctx;
	wlc_info_t *wlc = auth_info->wlc;
	wlc_bsscfg_t *cfg = notif->bsscfg;
	authenticator_t *auth;

	if (notif->up || !BSSCFG_AP(cfg)) {
		return;
	}

	auth = BSS_AUTH(auth_info, cfg);

	if (BCMAUTH_PSK_ENAB(wlc->pub) && (auth != NULL))
		wlc_authenticator_down(auth);
	else
		wlc_keymgmt_reset(wlc->keymgmt, cfg, NULL);
}

/* enable during associaton state complete via scb state notif */
static void
wlc_auth_scb_state_upd(void *ctx, scb_state_upd_data_t *data)
{
	wlc_auth_info_t *auth_info = (wlc_auth_info_t *)ctx;
	wlc_info_t *wlc = auth_info->wlc;
	struct scb *scb = data->scb;
	wlc_bsscfg_t *cfg;
	authenticator_t *auth;

	ASSERT(scb != NULL);
	BCM_REFERENCE(wlc);

	/* hndl transition from unassoc to assoc */
	if (!SCB_ASSOCIATED(scb))
		return;

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	auth = BSS_AUTH(auth_info, cfg);

	/* kick off 4 way handshaking */
	if (BCMAUTH_PSK_ENAB(wlc->pub) && auth != NULL) {
		wlc_auth_join_complete(auth, &scb->ea, TRUE);
	}
}

int
wlc_get_auth(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg)
{
	authenticator_t *auth = BSS_AUTH(authi, cfg);
	if (auth == NULL)
		return AUTH_UNUSED;
	return auth->auth_type;
}
bool wlc_auth_get_offload_state(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg)
{
	authenticator_t *auth;
	auth = BSS_AUTH(authi, cfg);
	return auth ? (auth->flags & AUTH_FLAG_AUTH_STATE): FALSE;
}
static int wlc_auth_config_set(wlc_auth_info_t *authi, wlc_bsscfg_t *cfg,
	auth_config_t *auth_config)
{
	authenticator_t *auth;
	auth = BSS_AUTH(authi, cfg);
	ASSERT(auth != NULL);
	if (!auth_config->bitmap)
		return BCME_BADARG;
	if (cfg->wlc->pub->up && (auth_config->bitmap &WL_AUTH_CONFIG_BITMAP_AUTH_ENAB))
		return BCME_NOTDOWN;
	/* Enable/disable authenticator offload */
	if (auth_config->bitmap &WL_AUTH_CONFIG_BITMAP_AUTH_ENAB) {
		if (auth_config->auth_enable) {
			auth->flags |= AUTH_FLAG_AUTH_STATE | AUTH_FLAG_REKEY_ENAB;
		} else {
			auth->flags &= ~(AUTH_FLAG_AUTH_STATE | AUTH_FLAG_REKEY_ENAB);
		}
	}
	/* Set GTK rotation interval for authenticator offload */
	if (auth_config->bitmap &WL_AUTH_CONFIG_BITMAP_GTK_ROTATION) {
		if (auth->auth_config->gtk_rot_interval != auth_config->gtk_rot_interval) {
			/* start the GTK timer */
			if (auth->flags &AUTH_FLAG_GTK_PLUMBED) {
				if (auth_config->gtk_rot_interval == 0) {
					auth->auth_config->gtk_rot_interval = 0;
					wl_del_timer(auth->wlc->wl, auth->gtk_timer);
				} else {
					if (!auth->gtk_timer) {
						auth->gtk_timer = wl_init_timer(auth->wlc->wl,
							wlc_auth_gtk_timer,
							auth, "auth_gtk_rotate");
						if (!auth->gtk_timer) {
							WL_ERROR(("wl%d: %s: gtk timer"
								" alloc failed\n",
								auth->wlc->pub->unit,
								__FUNCTION__));
							return BCME_NOMEM;
						}
					}
					if (auth->auth_config->gtk_rot_interval)
						wl_del_timer(auth->wlc->wl, auth->gtk_timer);
					auth->auth_config->gtk_rot_interval =
						auth_config->gtk_rot_interval;
					wl_add_timer(auth->wlc->wl, auth->gtk_timer,
						auth->auth_config->gtk_rot_interval, 0);
				}
			} else {
				auth->auth_config->gtk_rot_interval =
					auth_config->gtk_rot_interval;
			}
		}
	}
	/* Set EAPOL retry interval for authenticator offload */
	if (auth_config->bitmap &WL_AUTH_CONFIG_BITMAP_EAPOL_RETRY_INTRVL) {
		auth->auth_config->eapol_retry_interval = auth_config->eapol_retry_interval ?
			auth_config->eapol_retry_interval : AUTH_WPA2_RETRY_TIMEOUT;
	}
	/* Set EAPOL retry count for authenticator offload */
	if (auth_config->bitmap &WL_AUTH_CONFIG_BITMAP_EAPOL_RETRY_COUNT) {
		auth->auth_config->eapol_retry_count = auth_config->eapol_retry_count;
	}
	/* Set MIC fail count for Blacklisting */
	if (auth_config->bitmap &WL_AUTH_CONFIG_BITMAP_MIC_FAIL_BLKLST_COUNT) {
		auth->auth_config->micfail_cnt_for_blacklist =
			auth_config->micfail_cnt_for_blacklist;
	}
	/* Set Blacklisting age */
	if (auth_config->bitmap &WL_AUTH_CONFIG_BITMAP_MIC_FAIL_BLKLST_AGE) {
		auth->auth_config->blacklist_age = auth_config->blacklist_age;
	}
	return BCME_OK;
}

static int wlc_auth_config_set_cb(void *ctx, const uint8 *buf, uint16 type, uint16 len)
{
	uint32 val;
	auth_config_t *config = (auth_config_t *)ctx;
	int err = BCME_OK;
	switch (type) {
		case WL_IDAUTH_XTLV_AUTH_ENAB:
			if (len != sizeof(config->auth_enable)) {
				return BCME_BADARG;
			}
			config->auth_enable = buf[0];
			config->bitmap |= WL_AUTH_CONFIG_BITMAP_AUTH_ENAB;
			break;
		case WL_IDAUTH_XTLV_GTK_ROTATION:
			if (len != sizeof(config->gtk_rot_interval)) {
				return BCME_BADARG;
			}
			memcpy(&config->gtk_rot_interval, buf, sizeof(config->gtk_rot_interval));
			config->bitmap |= WL_AUTH_CONFIG_BITMAP_GTK_ROTATION;
			break;
		case WL_IDAUTH_XTLV_EAPOL_COUNT:
			if (len != sizeof(config->eapol_retry_count)) {
				return BCME_BADARG;
			}
			memcpy(&config->eapol_retry_count, buf, sizeof(config->eapol_retry_count));
			config->bitmap |= WL_AUTH_CONFIG_BITMAP_EAPOL_RETRY_COUNT;
			break;
		case WL_IDAUTH_XTLV_EAPOL_INTRVL:
			if (len != sizeof(config->eapol_retry_interval)) {
				return BCME_BADARG;
			}
			memcpy(&val, buf, sizeof(val));
			if (val) {
				config->eapol_retry_interval = val;
				config->bitmap |= WL_AUTH_CONFIG_BITMAP_EAPOL_RETRY_INTRVL;
			} else {
				return BCME_BADARG;
			}
			break;
		case WL_IDAUTH_XTLV_BLKLIST_COUNT:
		case WL_IDAUTH_XTLV_BLKLIST_AGE:
			/* Blacklisting is not supported */
			return BCME_UNSUPPORTED;
			break;
		default:
			err = BCME_BADOPTION;
			break;
		}
	return err;

}

static int
wlc_idauth_iovar_handler(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool iovar_isset,
	void *params, uint p_len, void *out, uint o_len)
{
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;
	const bcm_xtlv_opts_t align = BCM_XTLV_OPTION_ALIGN32;
	bcm_xtlv_t *auth_tlv;
	uint16 len, auth_category;
	int err;
	auth_config_t auth_config;

	/* all commands start with an XTLV container */
	auth_tlv = (bcm_xtlv_t*)params;
	memset(&auth_config, 0, sizeof(auth_config_t));
	if (p_len < BCM_XTLV_HDR_SIZE || !bcm_valid_xtlv(auth_tlv, p_len, no_pad)) {
		return BCME_BADARG;
	}
	/* collect the LE id/len values */
	auth_category = ltoh16(auth_tlv->id);
	len = ltoh16(auth_tlv->len);

	/* contianer must have room for at least one xtlv header */
	if (iovar_isset && len < BCM_XTLV_HDR_SIZE) {
		return BCME_BADARG;
	}
	if (iovar_isset) {
		switch (auth_category) {
		case WL_IDAUTH_CMD_CONFIG:
			err = bcm_unpack_xtlv_buf(&auth_config, (const uint8 *)auth_tlv->data,
				len, align, wlc_auth_config_set_cb);
			if (BCME_OK == err)
				err = wlc_auth_config_set(wlc->authi, cfg, &auth_config);
			break;
		default:
			err = BCME_UNSUPPORTED;
			break;
		}
	} else {
		/* GET handling */
		bcm_xtlv_t *val_xtlv = (bcm_xtlv_t*)auth_tlv->data;
		switch (auth_category) {
		case WL_IDAUTH_CMD_CONFIG:
			err = wlc_auth_idauth_config_get(cfg->wlc, cfg, val_xtlv, out, o_len);
			break;
		case WL_IDAUTH_CMD_PEER_INFO:
			err = wlc_auth_idauth_get_peer_info(cfg->wlc, cfg, val_xtlv, out, o_len);
			break;
		case WL_IDAUTH_CMD_COUNTERS:
			err = wlc_auth_idauth_get_counters(cfg->wlc, cfg, val_xtlv, out, o_len);
			break;

		default:
			err = BCME_UNSUPPORTED;
			break;
		}
	}
	return err;
}

static int
wlc_auth_idauth_config_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bcm_xtlv_t *params,
	void *out, uint o_len)
{
	bcm_xtlv_t *auth_rx;
	bcm_xtlvbuf_t tbuf;
	authenticator_t *auth;
	uint8 auth_active;
	int err = BCME_OK;
	auth_config_t *config;
	/* local list for params copy, or for request of all attributes */
	uint16 req_id_list[] = {
		WL_IDAUTH_XTLV_AUTH_ENAB,
		WL_IDAUTH_XTLV_GTK_ROTATION,
		WL_IDAUTH_XTLV_EAPOL_COUNT,
		WL_IDAUTH_XTLV_EAPOL_INTRVL,
		WL_IDAUTH_XTLV_BLKLIST_COUNT,
		WL_IDAUTH_XTLV_BLKLIST_AGE
	};
	uint req_id_count, i;
	uint16 val_id;

	auth = BSS_AUTH(wlc->authi, cfg);
	/* size (in elements) of the req buffer */
	req_id_count = ARRAYSIZE(req_id_list);

	/* start formatting the output buffer */

	/* AUTH container XTLV comes first */
	if (o_len < BCM_XTLV_HDR_SIZE) {
		return BCME_BUFTOOSHORT;
	}

	auth_rx = out;
	auth_rx->id = htol16(WL_IDAUTH_CMD_CONFIG);

	/* adjust len for the auth_rx header */
	o_len -= BCM_XTLV_HDR_SIZE;

	/* bcm_xtlv_buf_init() takes length up to uint16 */
	o_len = MIN(o_len, 0xFFFF);

	err = bcm_xtlv_buf_init(&tbuf, auth_rx->data, (uint16)o_len, BCM_XTLV_OPTION_ALIGN32);
	if (err) {
		return err;
	}

	/* walk the requests and write the value to the 'out' buffer */
	config = auth->auth_config;
	for (i = 0; i < req_id_count; i++) {
		val_id = req_id_list[i];
		switch (val_id) {
			case WL_IDAUTH_XTLV_AUTH_ENAB:
				auth_active = (auth->flags &AUTH_FLAG_AUTH_STATE) ? 0x01 : 0;
				err = bcm_xtlv_put_data(&tbuf, val_id,
					(const uint8 *)&auth_active,
					sizeof(auth_active));
				break;
			case WL_IDAUTH_XTLV_GTK_ROTATION:
				err = bcm_xtlv_put_data(&tbuf, val_id,
					(const uint8 *)&config->gtk_rot_interval,
					sizeof(config->gtk_rot_interval));
				break;
			case WL_IDAUTH_XTLV_EAPOL_COUNT:
				err = bcm_xtlv_put_data(&tbuf, val_id,
					(const uint8 *)&config->eapol_retry_count,
					sizeof(config->eapol_retry_count));
				break;
			case WL_IDAUTH_XTLV_EAPOL_INTRVL:
				err = bcm_xtlv_put_data(&tbuf, val_id,
					(const uint8 *)&config->eapol_retry_interval,
					sizeof(config->eapol_retry_interval));
				break;
			case WL_IDAUTH_XTLV_BLKLIST_COUNT:
				err = bcm_xtlv_put_data(&tbuf, val_id,
					(const uint8 *)&config->micfail_cnt_for_blacklist,
					sizeof(config->micfail_cnt_for_blacklist));
				break;
			case WL_IDAUTH_XTLV_BLKLIST_AGE:
				err = bcm_xtlv_put_data(&tbuf, val_id,
					(const uint8 *)&config->blacklist_age,
					sizeof(config->blacklist_age));
				break;
			default: /* unknown attribute ID */
				return BCME_BADOPTION;
		}
	}
	/* now we can write the container payload len */
	auth_rx->len = htol16(bcm_xtlv_buf_len(&tbuf));
	return err;
}
static int
wlc_auth_idauth_get_counters(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bcm_xtlv_t *params,
	void *out, uint o_len)
{
	bcm_xtlv_t *auth_rx;
	bcm_xtlvbuf_t tbuf;
	authenticator_t *auth;
	int err = BCME_OK;
	/* local list for params copy, or for request of all attributes */
	uint16 val_id;

	auth = BSS_AUTH(wlc->authi, cfg);
	/* start formatting the output buffer */
	/* Auth container XTLV comes first */
	if (o_len < BCM_XTLV_HDR_SIZE) {
		return BCME_BUFTOOSHORT;
	}

	auth_rx = out;
	auth_rx->id = htol16(WL_IDAUTH_CMD_COUNTERS);

	/* adjust len for the auth_rx header */
	o_len -= BCM_XTLV_HDR_SIZE;

	/* bcm_xtlv_buf_init() takes length up to uint16 */
	o_len = MIN(o_len, 0xFFFF);

	err = bcm_xtlv_buf_init(&tbuf, auth_rx->data, (uint16)o_len, BCM_XTLV_OPTION_ALIGN32);
	if (err) {
		return err;
	}
	/* walk the requests and write the value to the 'out' buffer */
	val_id = WL_IDAUTH_XTLV_COUNTERS;
	/* pack an XTLV with counters structure */
	err = bcm_xtlv_put_data(&tbuf, val_id, (const uint8 *)auth->auth_counters,
		sizeof(*auth->auth_counters));
	/* now we can write the container payload len */
	auth_rx->len = htol16(bcm_xtlv_buf_len(&tbuf));
	return err;
}
static int
wlc_auth_idauth_get_peer_info(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bcm_xtlv_t *params,
	void *out, uint o_len)
{
	bcm_xtlv_t *auth_rx;
	bcm_xtlvbuf_t tbuf;
	auth_peer_t *peer;
	struct scb *scb;
	struct scb_iter scbiter;
	scb_module_t *scbstate = wlc->scbstate;
	int err = BCME_OK;
	bcm_xtlv_t *xtlv;
	uint16 offset = 0;

	/* local list for params copy, or for request of all attributes */
	BCM_REFERENCE(wlc);

	/* AUTH container XTLV comes first */
	if (o_len < BCM_XTLV_HDR_SIZE) {
		return BCME_BUFTOOSHORT;
	}

	if (!BSSCFG_AP(cfg))
		return BCME_UNSUPPORTED;

	auth_rx = out;
	auth_rx->id = htol16(WL_IDAUTH_CMD_PEER_INFO);

	/* adjust len for the auth_rx header */
	o_len -= BCM_XTLV_HDR_SIZE;

	/* bcm_xtlv_buf_init() takes length up to uint16 */
	o_len = MIN(o_len, 0xFFFF);

	err = bcm_xtlv_buf_init(&tbuf, auth_rx->data, (uint16)o_len, BCM_XTLV_OPTION_ALIGN32);
	if (err) {
		return err;
	}
	/* walk the requests and write the value to the 'out' buffer */
	xtlv = (bcm_xtlv_t *)bcm_xtlv_buf(&tbuf);
	bcm_xtlv_put_data(&tbuf, WL_IDAUTH_XTLV_PEERS_INFO, NULL, 0);
	FOREACH_BSS_SCB(scbstate, &scbiter, cfg, scb) {
		peer = (auth_peer_t *)(xtlv->data + offset);
		if (bcm_xtlv_buf_rlen(&tbuf) < sizeof(auth_peer_t))
			return BCME_BUFTOOSHORT;
		peer->state = 0;
		/* pack an XTLV with the single value */
		if (SCB_AUTHORIZED(scb)) {
			peer->state = WL_AUTH_PEER_STATE_AUTHORISED;
		} else if (SCB_ASSOCIATED(scb)) {
			peer->state = WL_AUTH_PEER_STATE_4WAY_HS_ONGOING;
		}
		if (peer->state) {
			peer->blklist_end_time = 0;
			memcpy((void*)&peer->peer_addr, (void*)&scb->ea, ETHER_ADDR_LEN);
			offset += sizeof(auth_peer_t);
			tbuf.buf += sizeof(auth_peer_t);
		}
	}
	xtlv->len = offset;
	/* now we can write the container payload len */
	auth_rx->len = htol16(bcm_xtlv_buf_len(&tbuf));
	return err;
}
int wlc_auth_authreq_recv_update(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	return wlc_auth_counter_increment(wlc, cfg, WL_IDAUTH_CNTR_AUTH_REQ);

}
static int wlc_auth_counter_increment(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wl_idauth_counter_ids_t id)
{
	authenticator_t *auth;
	int ret = BCME_OK;
	auth = BSS_AUTH(wlc->authi, cfg);
	switch (id) {
		case WL_IDAUTH_CNTR_AUTH_REQ:
			auth->auth_counters->auth_reqs++;
			break;
		case WL_IDAUTH_CNTR_MIC_FAIL:
			auth->auth_counters->mic_fail++;
			break;
		case WL_IDAUTH_CNTR_4WAYHS_FAIL:
			auth->auth_counters->four_way_hs_fail++;
			break;
		default:
			ret = BCME_UNSUPPORTED;
	}
	return ret;
}
static int
wlc_auth_blacklist_peer(wlc_bsscfg_t *cfg, struct ether_addr *addr)
{
	int ret = BCME_UNSUPPORTED;
	BCM_REFERENCE(ret);
	return ret;
}
#endif /* BCMAUTH_PSK */
