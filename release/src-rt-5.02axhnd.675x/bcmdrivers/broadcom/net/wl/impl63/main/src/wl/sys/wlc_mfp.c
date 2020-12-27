/*
 * Broadcom 802.11 Networking Device Driver
 * Management frame protection
 *
 * Copyright 2019 Broadcom
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
 * $Id: wlc_mfp.c 781268 2019-11-15 08:32:33Z $
 *
 * This file provides implementation of interface to MFP functionality
 * defined in wlc_mfp.h
 *
 * It provides
 * 		WLC module support (attach/detach/iovars)
 *      MFP Integrity GTK (IGTK) support
 *			insert, extract IGTK from EAPOL
 *			IGTK generation
 *			IGTK Packet Number (IPN) initialization and update
			note: IGTK is maintained by keymgmt module
 *      Tx/Rx of protected management frames
 *      SA Query
 *      Dump of private per-BSS igtk and per-SCB sa query data
 *		Testing hooks (MFP_TEST)
 */

/**
 * @file
 * @brief
 * See 802.11w standard. Increases the security by providing data confidentiality of management
 * frames, mechanisms that enable data integrity, data origin authenticity, and replay protection.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [ProtectedManagementFrames]
 */

#ifdef MFP
#if !defined(BCMCCMP)
#error "BCMCCMP must be defined when MFP is defined"
#endif /* !BCMCCMP */

#include <wlc_cfg.h>

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>

#include <802.11.h>
#include <eap.h>
#include <eapol.h>
#include <wpa.h>

#include <wlc_types.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_assoc.h>
#include <wlc_frmutil.h>

#include <wl_export.h>

#include <wlc_mfp.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>

#include <aes.h>

#include <wlc_rm.h>

#ifdef MFP_TEST
#include <wlc_mfp_test.h>
#endif // endif
#include <wlc_pm.h>
#ifdef WL_PROXDETECT
#include <wlc_ftm.h>
#endif // endif

#include <wlc_event_utils.h>
#include <sha2.h>

/* random key size for initializing IPN */
#define IPN_INIT_KEY_SZ 32

/* toggle IGTK index for use as ID */
#define ISIGTK1(x) ((x) == WLC_KEY_ID_IGTK_1)
#define IGTK_NEXT_ID(_id)  (ISIGTK1(_id) ? WLC_KEY_ID_IGTK_2 : \
	WLC_KEY_ID_IGTK_1)

/* id for SA query - must not be zero */
#define MFP_SA_QUERY_ID(mfp) (((uint16)(mfp->wlc)->counter) | 0x8000)

/* module state */
struct wlc_mfp_info {
	wlc_info_t  *wlc;	/* wlc info pointer */
	int h_bsscfg;		/* bsscfg cubby handle */
	int	    h_scb;	/* scb cubby handle */
};

/* bsscfg cubby */
struct mfp_bsscfg {
	/* configuration setting */
	uint8 bip_type;
};
typedef struct mfp_bsscfg mfp_bsscfg_t;
#define MFP_BSSCFG(m, b) ((mfp_bsscfg_t*)BSSCFG_CUBBY(b, (m)->h_bsscfg))

/* scb cubby - maintains SA query state */
struct sa_query {
	bool			started;
	uint16			id;
	uint32			timeouts;
	struct wl_timer		*timer;
};
typedef struct sa_query sa_query_t;

struct mfp_scb {
	sa_query_t saq;
	uint32 comeback_time; /* in ms */
};
typedef struct mfp_scb mfp_scb_t;
#define MFP_SCB(m, s) ((mfp_scb_t*)SCB_CUBBY(s, (m)->h_scb))

/* iovar support */
enum wlc_mfp_iov {
	IOV_MFP = 1,
	IOV_MFP_BIP = 2,
	IOV_MFP_SA_QUERY = 3,
	IOV_MFP_DISASSOC = 4,
	IOV_MFP_DEAUTH = 5,
	IOV_MFP_ASSOC = 6,
	IOV_MFP_AUTH = 7,
	IOV_MFP_REASSOC = 8,
	IOV_MFP_BIP_TEST = 9,
	IOV_MFP_LAST
};

static const bcm_iovar_t mfp_iovars[] = {
	{"mfp", IOV_MFP, (0), 0, IOVT_INT32, 0},
#ifdef AP
	{"bip", IOV_MFP_BIP, 0, 0, IOVT_BUFFER, 0},
#endif // endif
#ifdef MFP_TEST
	{"mfp_sa_query", IOV_MFP_SA_QUERY, IOVF_SET_UP, 0, IOVT_INT32, 0},
	{"mfp_disassoc", IOV_MFP_DISASSOC, IOVF_SET_UP, 0, IOVT_INT32, 0},
	{"mfp_deauth", IOV_MFP_DEAUTH, IOVF_SET_UP, 0, IOVT_INT32, 0},
	{"mfp_assoc", IOV_MFP_ASSOC, IOVF_SET_UP, 0, IOVT_INT32, 0},
	{"mfp_auth", IOV_MFP_AUTH, IOVF_SET_UP, 0, IOVT_INT32, 0},
	{"mfp_reassoc", IOV_MFP_REASSOC, IOVF_SET_UP, 0, IOVT_INT32, 0},
	{"mfp_bip_test", IOV_MFP_BIP_TEST, IOVF_SET_UP, 0, IOVT_INT32, 0},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

/* prototypes - forward reference */

static int wlc_mfp_down(void *ctx);

/* sa query */
static void* mfp_send_sa_query(wlc_mfp_info_t *mfp, struct scb *scb,
	uint8 action, uint16 id);

/* igtk  support */
static void mfp_init_ipn(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint32 *lo, uint16 *hi);

/* utils */

/* is category robust */
static bool mfp_robust_cat(const wlc_bsscfg_t *bsscfg, uint8 cat);

/* IE mgmt */
#ifdef AP
static uint wlc_mfp_calc_to_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_mfp_write_to_ie(void *ctx, wlc_iem_build_data_t *data);
#endif // endif
#ifdef STA
static int wlc_mfp_parse_to_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif // endif
static int mfp_bsscfg_init(void* ctx, wlc_bsscfg_t *bsscfg);
static void mfp_bsscfg_deinit(void* ctx, wlc_bsscfg_t *bsscfg);

/* end prototypes */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* iovar callback */
static int
mfp_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	wlc_info_t *wlc = mfp->wlc;
	int err = BCME_OK;
	wlc_bsscfg_t *bsscfg;
	int32 *ret_int_ptr;
	int32 flag;

	BCM_REFERENCE(p_len);
	BCM_REFERENCE(params);
	BCM_REFERENCE(val_size);
	BCM_REFERENCE(len);

	ret_int_ptr = (int32*)arg;
	flag = *ret_int_ptr;
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_MFP):
		*ret_int_ptr = (BSSCFG_IS_MFP_REQUIRED(bsscfg)) ? WL_MFP_REQUIRED :
			(BSSCFG_IS_MFP_CAPABLE(bsscfg) ? WL_MFP_CAPABLE : WL_MFP_NONE);
		break;
	case IOV_SVAL(IOV_MFP): {

#ifdef AP
		mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, bsscfg);
		if (!bss_mfp) {
			WL_ERROR(("wl%d: %s: NULL bss_mfp\n",
					WLCWLUNIT(wlc), __FUNCTION__));
			err = BCME_ERROR;
			break;
		}
#endif /* AP */

		if (bsscfg->up) {
			/* Just return BRCM_OK if setting mfp to the same value */
			if ((flag == WL_MFP_NONE && !BSSCFG_IS_MFP_CAPABLE(bsscfg) &&
					!BSSCFG_IS_MFP_REQUIRED(bsscfg)) ||
				(flag == WL_MFP_REQUIRED && BSSCFG_IS_MFP_REQUIRED(bsscfg)) ||
				(flag == WL_MFP_CAPABLE && BSSCFG_IS_MFP_CAPABLE(bsscfg))) {
				break;
			}
			WL_ERROR(("wl%d.%d: %s: bss is not down\n",
					WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
			err = BCME_NOTDOWN;
			break;
		}

		bsscfg->flags2 &= ~(WLC_BSSCFG_FL2_MFP_REQUIRED|WLC_BSSCFG_FL2_MFP_CAPABLE);
		switch (flag) {
		case WL_MFP_NONE:
			break;
		case WL_MFP_REQUIRED:
			bsscfg->flags2 |= WLC_BSSCFG_FL2_MFP_REQUIRED;
			/* fall through */
		case WL_MFP_CAPABLE:
			bsscfg->flags2 |= WLC_BSSCFG_FL2_MFP_CAPABLE;
			break;
		default:
			err = BCME_BADARG;
			break;
		}

		if (err != BCME_OK)
			break;

		break;
	}
#ifdef AP
	case IOV_SVAL(IOV_MFP_BIP): {
		mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, bsscfg);
		if (!bss_mfp) {
			WL_ERROR(("wl%d: %s: NULL bss_mfp\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			err = BCME_ERROR;
			break;
		}
		/* validate bip setting */
		if (memcmp((const uint8 *)BIP_OUI_TYPE, (uint8 *)arg, WPA_SUITE_LEN) == 0) {
			bss_mfp->bip_type = WPA_CIPHER_BIP;
		} else {
			WL_ERROR(("wl%d: %s: unsupported BIP type\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			err = BCME_BADARG;
		}
		break;
	}
#endif /* AP */
	default:
#ifdef MFP_TEST
		err =  mfp_test_doiovar(mfp->wlc, mfp_test_iov(actionid),
			params, p_len, arg, len, val_size, wlcif);
#else
		err = BCME_UNSUPPORTED;
#endif // endif
		break;
	}

	return err;
}

static int
wlc_mfp_down(void *ctx)
{
	wlc_mfp_info_t *mfp = (wlc_mfp_info_t *)ctx;
	wlc_info_t	*wlc;
	struct scb_iter scbiter;
	struct scb *scb;
	mfp_scb_t* scb_mfp;

	wlc = mfp->wlc;
	/* Clean up timers and cubby (some may not have been
	 * cleaned in scb_free - NIC offloads on sleep or assoc_recreate()
	 * with WLC_BSSCFG_PRESERVE, those marked permanent)
	 */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_mfp = MFP_SCB(mfp, scb);
		if (scb_mfp->saq.timer)  {
			if (scb_mfp->saq.started != FALSE) {
				wl_del_timer(mfp->wlc->wl, scb_mfp->saq.timer);
			}
			wl_free_timer(mfp->wlc->wl, scb_mfp->saq.timer);
		}
		memset(scb_mfp, 0, sizeof(*scb_mfp));
	}
	return BCME_OK;
}

#if defined(BCMDBG)
static void
mfp_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, cfg);

	bcm_bprintf(b, "\tbip: %02x\n", bss_mfp->bip_type);
}
#else
#define mfp_bsscfg_dump NULL
#endif // endif

#ifdef WLRSDB
static int
mfp_bsscfg_get(void *ctx, wlc_bsscfg_t *cfg, uint8 *data, int *len)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, cfg);
	mfp_bsscfg_t *cp = (mfp_bsscfg_t *)data;

	ASSERT(data);
	cp->bip_type = bss_mfp->bip_type;
	*len = sizeof(mfp_bsscfg_t);

	return BCME_OK;
}

static int
mfp_bsscfg_set(void *ctx, wlc_bsscfg_t *cfg, const uint8 *data, int len)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, cfg);
	const mfp_bsscfg_t *cp = (const mfp_bsscfg_t *)data;

	bss_mfp->bip_type = cp->bip_type;

	return BCME_OK;
}
#endif /* WLRSDB */

static int
mfp_scb_init(void* ctx, struct scb* scb)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_scb_t* scb_mfp = MFP_SCB(mfp, scb);
	memset(scb_mfp, 0, sizeof(*scb_mfp));
	return BCME_OK;
}

static void
mfp_scb_deinit(void* ctx, struct scb* scb)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_scb_t* scb_mfp = MFP_SCB(mfp, scb);

	if (scb_mfp->saq.timer)  {
		if (scb_mfp->saq.started != FALSE)
			wl_del_timer(mfp->wlc->wl, scb_mfp->saq.timer);
		wl_free_timer(mfp->wlc->wl, scb_mfp->saq.timer);
	}

	memset(scb_mfp, 0, sizeof(*scb_mfp));
}

#if defined(BCMDBG)
static void
mfp_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_scb_t* scb_mfp = MFP_SCB(mfp, scb);
	sa_query_t *saq = &scb_mfp->saq;

	bcm_bprintf(b, "\tsa query: %s in progress\n", (saq->started ? "": "none"));
	if (saq->started)
		bcm_bprintf(b, "\t\tid: 0x%04x timeouts: %lu\n", saq->id, saq->timeouts);
}
#else
#define mfp_scb_dump NULL
#endif // endif

/* attach */
wlc_mfp_info_t *
BCMATTACHFN(wlc_mfp_attach)(wlc_info_t *wlc)
{
	wlc_mfp_info_t* mfp;
	bsscfg_cubby_params_t cubby_params;
#ifdef AP
	uint16 arsfstbmp = FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP);
#endif // endif
	wlc->pub->_mfp = FALSE;

	/* Assertion to make sure we have an rx IV for MFP */
#if !defined(MFP_DISABLED)
	#define ctr(x) (WPA_CAP_ ## x ## _REPLAY_CNTRS == WLC_REPLAY_CNTRS_VALUE)
	STATIC_ASSERT((ctr(16) && WLC_KEY_NUM_RX_SEQ > 16) ||
		(ctr(4) && WLC_KEY_NUM_RX_SEQ > 4));
	#undef ctr
#endif /* !MFP_DISABLED */

	ASSERT(wlc != NULL);
	mfp = MALLOCZ(wlc->osh, sizeof(*mfp));
	if (mfp == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	mfp->wlc = wlc;
	if (wlc_module_register(wlc->pub, mfp_iovars, "mfp", mfp, mfp_doiovar,
		NULL /* wdog */, NULL /* up */, wlc_mfp_down /* down */) != BCME_OK)
		goto err;
	/* reserve the bsscfg cubby for any mfp specific private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = (void*)mfp;
	cubby_params.fn_init = mfp_bsscfg_init;
	cubby_params.fn_deinit = mfp_bsscfg_deinit;
	cubby_params.fn_dump = mfp_bsscfg_dump;
#ifdef WLRSDB
	cubby_params.fn_get = mfp_bsscfg_get;
	cubby_params.fn_set = mfp_bsscfg_set;
	cubby_params.config_size = sizeof(mfp_bsscfg_t);
#endif /* WLRSDB */

	/* not cleared configuration during init/deinit */
	mfp->h_bsscfg = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(mfp_bsscfg_t), &cubby_params);
	if (mfp->h_bsscfg < 0)
		goto err;

	mfp->h_scb = wlc_scb_cubby_reserve(wlc, sizeof(mfp_scb_t),
		mfp_scb_init, mfp_scb_deinit, mfp_scb_dump, (void*)mfp);
	if (mfp->h_scb < 0)
		goto err;

	/* regsiter IE mgmt callbacks */
#ifdef AP
	/* assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_FT_TI_ID,
	      wlc_mfp_calc_to_ie_len, wlc_mfp_write_to_ie, mfp) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, to in assocresp\n",
		          wlc->pub->unit, __FUNCTION__));
		goto err;
	}
#endif /* AP */

#ifdef STA
	/* assocresp/reassocresp */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arsfstbmp, DOT11_MNG_FT_TI_ID,
	      wlc_mfp_parse_to_ie, mfp) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, to in assocresp\n",
			wlc->pub->unit, __FUNCTION__));
			goto err;
	}
#endif /* STA */

	wlc->pub->_mfp = TRUE;
	return mfp;

err:
	WL_ERROR(("wl%d: wlc_module_register(mfp) failed\n", WLCWLUNIT(wlc)));
	MODULE_DETACH(mfp, wlc_mfp_detach);
	return NULL;
}

/* detach */
void
BCMATTACHFN(wlc_mfp_detach)(wlc_mfp_info_t *mfp)
{
	wlc_info_t *wlc;

	if (mfp == NULL)
		return;

	wlc = mfp->wlc;
	wlc_module_unregister(wlc->pub, "mfp", mfp);
	MFREE(wlc->osh, mfp, sizeof(*mfp));
	wlc->pub->_mfp = FALSE;
}

#if defined(BCMSUP_PSK) || defined(BCMSUPPL)
/** IGTK is Integrity Group Temporary Key, MFP (Management Frame Protection) related */
bool
wlc_mfp_extract_igtk(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	eapol_header_t* eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	uint16 data_len = ntoh16_ua(&body->data_len);
	eapol_wpa2_encap_data_t *data_encap;
	eapol_wpa2_key_igtk_encap_t *igtk_kde = NULL;
	wlc_info_t *wlc;
	wlc_key_info_t key_info;
	wlc_key_t *igtk;
	size_t igtk_key_len;
	int err = BCME_OK;
	wlc_key_id_t key_id = 0xff;

	wlc = mfp->wlc;

	data_encap = wpa_find_kde(body->data, data_len, WPA2_KEY_DATA_SUBTYPE_IGTK);
	if (!data_encap) {
		err = BCME_NOTFOUND;
		goto done;
	}

	igtk_kde = (eapol_wpa2_key_igtk_encap_t *)data_encap->data;
	if (!WLC_KEY_ID_IS_IGTK(igtk_kde->key_id)) {
		err = BCME_BADKEYIDX;
		goto done;
	}

	key_id = (wlc_key_id_t)ltoh16_ua(&igtk_kde->key_id);
	igtk = wlc_keymgmt_get_bss_key(wlc->keymgmt, bsscfg, key_id, &key_info);

	igtk_key_len = data_encap->length -
		((EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
		EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN);

	err = wlc_key_set_data(igtk, CRYPTO_ALGO_BIP, igtk_kde->key, igtk_key_len);
	if (err != BCME_OK)
		goto done;

	err = wlc_key_set_seq(igtk, igtk_kde->ipn, sizeof(igtk_kde->ipn), 0, FALSE /* rx */);
	if (err != BCME_OK)
		goto done;

	err = wlc_keymgmt_set_bss_tx_key_id(wlc->keymgmt, bsscfg, key_id, TRUE);
	if (err != BCME_OK)
		goto done;

done:
	if (err != BCME_OK) {
		if (igtk_kde != NULL)
			WL_ERROR(("wl%d.%d: %s: Error %d setting IGTK data for key id %d\n",
				WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, err, key_id));
		else
			WL_WSEC(("wl%d.%d: %s: Error - %d, IGTK KDE not found\n",
				WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, err));

		return FALSE;
	} else {
		WL_WSEC(("wl%d.%d: %s: IGTK key id %d, ipn %04x%08x\n",
			WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, key_id,
			ltoh16_ua(igtk_kde->ipn+4), ltoh32_ua(igtk_kde->ipn)));
		return TRUE;
	}
}
#endif /* BCMSUP_PSK || BCMSUPPL */

#ifdef BCMAUTH_PSK
/** IGTK is Integrity Group Temporary Key, MFP (Management Frame Protection) related */
int
wlc_mfp_insert_igtk(wlc_mfp_info_t *mfp, const wlc_bsscfg_t *bsscfg,
	eapol_header_t *eapol, uint16 *data_len)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	eapol_wpa2_encap_data_t *data_encap;
	uint16 len = *data_len;
	eapol_wpa2_key_igtk_encap_t *igtk_encap;
	wlc_key_t *igtk;
	wlc_key_info_t key_info;
	wlc_info_t *wlc;

	wlc = mfp->wlc;
	igtk = wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, TRUE, &key_info);
	if (!WLC_KEY_ALGO_IS_BIPXX(key_info.algo)) {
		WL_WSEC(("wl%d.%d: %s: attempt to insert invalid igtk algo %s in eapol\n",
			WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			wlc_keymgmt_get_algo_name(wlc->keymgmt, key_info.algo)));
		goto done;
	}

	data_encap = (eapol_wpa2_encap_data_t *) (body->data + len);
	data_encap->type = DOT11_MNG_PROPR_ID;
	data_encap->length = (EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
		EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN + key_info.key_len;
	memcpy(data_encap->oui, WPA2_OUI, DOT11_OUI_LEN);
	data_encap->subtype = WPA2_KEY_DATA_SUBTYPE_IGTK;
	len += EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
	igtk_encap = (eapol_wpa2_key_igtk_encap_t *) (body->data + len);

	igtk_encap->key_id = key_info.key_id;
	(void)wlc_key_get_seq(igtk, igtk_encap->ipn, sizeof(igtk_encap->ipn), 0, TRUE);
	(void)wlc_key_get_data(igtk, igtk_encap->key, sizeof(igtk_encap->key), NULL);
	len += key_info.key_len + EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN;

done:
	/* return the adjusted data len */
	*data_len = len;
	return (key_info.key_len +
		EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN + EAPOL_WPA2_ENCAP_DATA_HDR_LEN);
}
#endif /* BCMAUTH_PSK */

/** IGTK is Integrity Group Temporary Key, MFP (Management Frame Protection) related */
void
wlc_mfp_reset_igtk(wlc_mfp_info_t* mfp, struct wlc_bsscfg *bsscfg)
{
	wlc_key_t *igtk;
	wlc_keymgmt_t *km = mfp->wlc->keymgmt;
	int err;

	igtk = wlc_keymgmt_get_bss_key(km, bsscfg, WLC_KEY_ID_IGTK_1, NULL);
	wlc_key_reset(igtk);

	igtk = wlc_keymgmt_get_bss_key(km, bsscfg, WLC_KEY_ID_IGTK_2, NULL);
	wlc_key_reset(igtk);

	/* tx key starts w/ igtk1; next(igtk2) = igtk2 */
	err = wlc_keymgmt_set_bss_tx_key_id(mfp->wlc->keymgmt, bsscfg,
		WLC_KEY_ID_IGTK_2, TRUE);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d.%d: %s: error %d setting bss tx key id %d\n",
			WLCWLUNIT(mfp->wlc), WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			err, WLC_KEY_ID_IGTK_2));
	}
}

/** IGTK is Integrity Group Temporary Key, MFP (Management Frame Protection) related */
uint16
wlc_mfp_gen_igtk(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint8 *master_key, uint32 master_key_len)
{
	unsigned char data[ETHER_ADDR_LEN + sizeof(uint32) + sizeof(uint16)];
	unsigned char prf_buff[AES_TK_LEN];
	unsigned char prefix[] = "Group key expansion";
	wlc_keymgmt_t *km;
	int data_len = 0;
	wlc_key_t *igtk;
	wlc_key_id_t ipn_id;
	uint32 ipn_lo;
	uint16 ipn_hi;
	int err;
	uint8 ipn_seq[DOT11_WPA_KEY_RSC_LEN];
	bcm_const_xlvp_t pfx;

	km = mfp->wlc->keymgmt;

	/* generate a fresh ipn */
	ipn_id = wlc_keymgmt_get_bss_tx_key_id(km, bsscfg, TRUE);
	ipn_id = IGTK_NEXT_ID(ipn_id);
	mfp_init_ipn(mfp, bsscfg, &ipn_lo, &ipn_hi);

	/* create the the data portion */
	memcpy((char*)&data[data_len], (char*)&bsscfg->cur_etheraddr,
		ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	*(uint32 *)&data[data_len] = htol32(ipn_lo);
	*(uint16 *)&data[data_len+sizeof(uint32)] = htol16(ipn_hi);
	data_len += 6;

	/* generate the IGTK. note: was SHA1 before */
	pfx.len = sizeof(prefix) - 1;
	pfx.data = prefix;
	err = hmac_sha2_n(HASH_SHA256, master_key, master_key_len, &pfx, 1, data, data_len,
		prf_buff, AES_TK_LEN);
	if (err != BCME_OK)
		goto done;

	/* note: we do not use the key info (algo, key_len) from key_info
	 * from the call below, because the algorithm is initialized lazily
	 */
	igtk = wlc_keymgmt_get_bss_key(mfp->wlc->keymgmt, bsscfg, ipn_id, NULL);

	err = wlc_key_set_data(igtk, CRYPTO_ALGO_BIP, prf_buff, AES_TK_LEN);
	if (err != BCME_OK)
		goto done;

	wlc_key_pn_to_seq(ipn_seq, sizeof(ipn_seq), ipn_lo & 0xffff,
		(ipn_hi << 16) | ((ipn_lo & 0xffff0000) >> 16));
	err = wlc_key_set_seq(igtk, ipn_seq, sizeof(ipn_seq), 0, TRUE /* tx */);
	if (err != BCME_OK)
		goto done;

	err = wlc_keymgmt_set_bss_tx_key_id(km, bsscfg, ipn_id, TRUE);
	if (err != BCME_OK)
		goto done;

done:
	if (err != BCME_OK) {
		WL_ERROR(("wl%d.%d: %s: error %d generating igtk for key id %d\n",
			WLCWLUNIT(mfp->wlc), WLC_BSSCFG_IDX(bsscfg),
			__FUNCTION__, err, ipn_id));
		return 0;
	} else {
		WL_WSEC(("wl%d.%d: %s: generated igtk for key id %d. ipn %04x%08x\n",
			WLCWLUNIT(mfp->wlc), WLC_BSSCFG_IDX(bsscfg),
			__FUNCTION__, ipn_id, ipn_hi, ipn_lo));

		return AES_TK_LEN;
	}
}

static void
mfp_init_ipn(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg, uint32 *lo, uint16 *hi)
{
	unsigned char buff[IPN_INIT_KEY_SZ];
	unsigned char prefix[] = "Init Counter";
	bcm_const_xlvp_t pfx;

	wlc_getrand(mfp->wlc, &buff[0], IPN_INIT_KEY_SZ);

	/* Still not exactly right, but better. */
	pfx.len = sizeof(prefix) - 1;
	pfx.data = prefix;
	(void)hmac_sha2_n(HASH_SHA256, buff, sizeof(buff), &pfx, 1,
		(unsigned char *)&bsscfg->cur_etheraddr, ETHER_ADDR_LEN,
		buff, sizeof(*lo) + sizeof(*hi));

	memcpy(lo, buff, sizeof(*lo));
	memcpy(hi, buff+sizeof(*lo), sizeof(*hi));

	if ((*lo)++ == 0)
		(*hi)++;
}

/* sa query */

static void*
mfp_send_sa_query(wlc_mfp_info_t *mfp, struct scb *scb, uint8 action, uint16 id)
{
	void *p;
	uint8* pbody;
	uint body_len;
	struct dot11_action_sa_query *af;

	WL_WSEC(("wl%d: %s: action %d id %d\n", WLCWLUNIT(mfp->wlc), __FUNCTION__,
		action, id));
	body_len = sizeof(struct dot11_action_sa_query);
	p = wlc_frame_get_action(mfp->wlc, &scb->ea,
		&scb->bsscfg->cur_etheraddr, &scb->bsscfg->BSSID,
		body_len, &pbody, DOT11_ACTION_CAT_SA_QUERY);
	if (p) {
		af = (struct dot11_action_sa_query *)pbody;
		af->category = DOT11_ACTION_CAT_SA_QUERY;
		af->action = action;
		af->id = id;
		wlc_sendmgmt(mfp->wlc, p, scb->bsscfg->wlcif->qi, scb);
	}

	return p;
}

static void
mfp_recv_sa_resp(wlc_mfp_info_t *mfp, struct scb *scb,
	const struct dot11_action_sa_query *af)
{
	mfp_scb_t *mfp_scb = MFP_SCB(mfp, scb);
	sa_query_t *q = &mfp_scb->saq;

	if (q->started != TRUE)
		return;

	if (af->id == q->id) { /* got out response */
		wl_del_timer(mfp->wlc->wl, q->timer);
		q->started = FALSE;
		q->timeouts = 0;
		return;
	}

}

void
wlc_mfp_handle_sa_query(wlc_mfp_info_t *mfp, struct scb *scb, uint action,
	const struct dot11_management_header *hdr, const uint8 *body, int body_len)
{
	const struct dot11_action_sa_query *af;

	BCM_REFERENCE(body_len);
	BCM_REFERENCE(hdr);

	if (scb == NULL)
		return;

	WL_WSEC(("wl%d: rcvd SA query, action %d\n", WLCWLUNIT(mfp->wlc), action));
	af = (const struct dot11_action_sa_query *)body;
	switch (action) {
	case SA_QUERY_REQUEST:
		if (SCB_ASSOCIATED(scb))
			mfp_send_sa_query(mfp, scb, SA_QUERY_RESPONSE, af->id);
		break;
	case SA_QUERY_RESPONSE:
		mfp_recv_sa_resp(mfp, scb, af);
		break;
	default:
		WL_ERROR(("wl %d: unrecognised SA Query\n", WLCWLUNIT(mfp->wlc)));
		break;
	}
}

/* handle a sa query timeout. disassociate on too many timeouts */
static void
mfp_sa_query_timeout(void *arg)
{
	struct scb *scb = (struct scb *)arg;
	wlc_bsscfg_t *cfg = scb->bsscfg;
	wlc_info_t *wlc = cfg->wlc;
	wlc_mfp_info_t *mfp = wlc->mfp;
	mfp_scb_t *mfp_scb = MFP_SCB(mfp, scb);
	sa_query_t *q = &mfp_scb->saq;

	if (BSSCFG_STA(cfg) && cfg->pm->PMenabled && !(q->timeouts & 0x01)) {
		if (!wlc_sendpspoll(wlc, cfg))
			WL_ERROR(("wl%d: %s: wlc_sendpspoll() failed\n", wlc->pub->unit,
				__FUNCTION__));
		q->timeouts++;
		return;
	}

	q->timeouts++;
	if (q->timeouts > WLC_MFP_SA_QUERY_MAX_TIMEOUTS) {
		wl_del_timer(wlc->wl, q->timer);
		q->timeouts = 0;
		q->started = FALSE;

		/* disassoc scb */
		wlc_senddisassoc(wlc, cfg, scb, &scb->ea, &cfg->BSSID,
		                 &cfg->cur_etheraddr, DOT11_RC_NOT_AUTH);
		wlc_scb_clearstatebit(wlc, scb, ASSOCIATED | AUTHORIZED | AUTHENTICATED);
		wlc_deauth_complete(wlc, SCB_BSSCFG(scb),
			WLC_E_STATUS_SUCCESS, &scb->ea,
			DOT11_RC_NOT_AUTH, 0);
#ifdef AP
		if (AP_ENAB(wlc->pub)) {
			wlc_bss_mac_event(wlc, cfg, WLC_E_DISASSOC_IND, &scb->ea,
					WLC_E_STATUS_SUCCESS, DOT11_RC_DISASSOC_LEAVING, 0, 0, 0);
		}
#endif // endif
		/* free SCB only after sending event, as it uses scb->ea */
		wlc_scbfree(wlc, scb);
	} else {
		q->id = MFP_SA_QUERY_ID(mfp);
		mfp_send_sa_query(mfp, scb, SA_QUERY_REQUEST, q->id);
	}
}

void
wlc_mfp_start_sa_query(wlc_mfp_info_t *mfp, const wlc_bsscfg_t *bsscfg,
	struct scb *scb)
{
	wlc_info_t *wlc = mfp->wlc;
	mfp_scb_t *mfp_scb = MFP_SCB(mfp, scb);
	sa_query_t *q = &mfp_scb->saq;

	BCM_REFERENCE(bsscfg);

	if (scb == NULL || !SCB_MFP(scb) || !SCB_AUTHENTICATED(scb) ||
			!SCB_ASSOCIATED(scb))
		return;

	if (q->started) {
		WL_WSEC(("wl%d: ignoring sa query start; already started\n",
			WLCWLUNIT(wlc)));
		return;
	}

	if (q->timer == NULL)
		q->timer = wl_init_timer(wlc->wl, mfp_sa_query_timeout, scb, "sa_query");

	if (q->timer != NULL)  {
		q->started = TRUE;
		q->timeouts = 0;
		q->id = MFP_SA_QUERY_ID(mfp);
		wl_add_timer(wlc->wl, q->timer, WLC_MFP_SA_QUERY_TIMEOUT_MS, TRUE);
		mfp_send_sa_query(mfp, scb, SA_QUERY_REQUEST, q->id);
	}
}

/* misc utils */

/* bitmap indicates robust action categories.
 * see table Table 9-53 - Category values.
 */
static const uint8 mfp_robust_categories[] = {
	0x6f, 0x67, 0x8d, 0x87, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static INLINE bool
mfp_robust_cat(const wlc_bsscfg_t *bsscfg, uint8 cat)
{
	BCM_REFERENCE(bsscfg);
#if defined(WL_PROXDETECT) && defined(WL_FTM)
	if (WLC_BSSCFG_SECURE_FTM(bsscfg) && !cat) {
		return FALSE;
	} else
#endif // endif
		return isset(mfp_robust_categories, cat);
}

#ifdef AP
/* write timeout ie during assoc - indicates time to come back and
 * associate (after the response to SA query we send is received)
 */
static uint
wlc_mfp_calc_to_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_mfp_info_t *mfp = (wlc_mfp_info_t *)ctx;
	wlc_info_t *wlc = mfp->wlc;
	struct scb *scb = data->cbparm->ft->assocresp.scb;

	if (WLC_MFP_ENAB(wlc->pub) &&
	    SCB_MFP(scb) && SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb))
		return sizeof(dot11_timeout_ie_t);

	return 0;
}

static int
wlc_mfp_write_to_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_mfp_info_t *mfp = (wlc_mfp_info_t *)ctx;
	wlc_info_t *wlc = mfp->wlc;
	struct scb *scb = data->cbparm->ft->assocresp.scb;

	if (WLC_MFP_ENAB(wlc->pub) &&
	    SCB_MFP(scb) && SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb)) {
		dot11_timeout_ie_t *pi = (dot11_timeout_ie_t *)data->buf;

		pi->id = DOT11_MNG_FT_TI_ID;
		pi->type = TIE_TYPE_ASSOC_COMEBACK;
		pi->value = htol32(WLC_MFP_COMEBACK_TIE_TU);
		pi->len = sizeof(*pi) - TLV_HDR_LEN;
	}

	return BCME_OK;
}
#endif /* AP */

#ifdef STA
static int
wlc_mfp_parse_to_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_mfp_info_t *mfp = (wlc_mfp_info_t *)ctx;
	wlc_info_t *wlc = mfp->wlc;

	if (data->ie == NULL || data->ie_len <= TLV_BODY_OFF)
		return BCME_OK;

	switch (data->ft) {
		case FC_ASSOC_RESP:
		case FC_REASSOC_RESP: {
			wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
			scb_t *scb = ftpparm->assocresp.scb;

			if (scb == NULL)
				break;

			if (WLC_MFP_ENAB(wlc->pub)) {
				dot11_timeout_ie_t *pi = (dot11_timeout_ie_t *)data->ie;
				mfp_scb_t *mfp_scb = MFP_SCB(mfp, scb);

				if (mfp_scb == NULL)
					break;

				if (pi->id == DOT11_MNG_FT_TI_ID &&
					pi->type == TIE_TYPE_ASSOC_COMEBACK) {
					mfp_scb->comeback_time =
						(pi->value << 10)/1000; /* convert TU to ms */
					WL_WSEC(("wl%d: %s: comeback time %d TU\n",
						WLCWLUNIT(wlc), __FUNCTION__, pi->value));
				}
				break;
			}
		}
		default:
			break;
	}

	return BCME_OK;
}
#endif /* STA */

uint32
wlc_mfp_get_assoc_comeback_time(wlc_mfp_info_t *mfp, scb_t *scb)
{
	mfp_scb_t *mfp_scb = MFP_SCB(mfp, scb);

	return mfp_scb->comeback_time;
}

bool
wlc_mfp_check_rsn_caps(const wlc_mfp_info_t* mfp, wlc_bsscfg_t *cfg, uint16 rsn,
	bool *enable)
{
	bool ret = TRUE;

	BCM_REFERENCE(mfp);

	*enable = FALSE; /* MFP does not apply */
	/* Association is allowed for MFPR 1 and MFPC 0 case */
	if (BSSCFG_IS_MFP_CAPABLE(cfg) && (rsn & RSN_CAP_MFPC))
		*enable = TRUE; /* MFP applies */
	else if (BSSCFG_IS_MFP_REQUIRED(cfg) && !(rsn & RSN_CAP_MFPC))
		ret = FALSE;
	else if (!BSSCFG_IS_MFP_CAPABLE(cfg) && (rsn & RSN_CAP_MFPR))
		ret =  FALSE;

	return ret;
}

uint8
wlc_mfp_get_rsn_caps(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *cfg)
{
	uint8 rsn = 0;
	BCM_REFERENCE(mfp);
	if (BSSCFG_IS_MFP_CAPABLE(cfg))
		rsn |= RSN_CAP_MFPC;
	if (BSSCFG_IS_MFP_REQUIRED(cfg))
		rsn |= RSN_CAP_MFPR;
	return rsn;
}

uint8
wlc_mfp_rsn_caps_to_flags(const wlc_mfp_info_t *mfp, uint8 rsn)
{
	uint8 flags = 0;

	BCM_REFERENCE(mfp);

	if (rsn & RSN_CAP_MFPC)
		flags |= RSN_FLAGS_MFPC;
	if (rsn & RSN_CAP_MFPR)
		flags |= RSN_FLAGS_MFPR;
	return flags;
}

void
mfp_set_bip(wlc_bsscfg_t *bsscfg, uint8 *bip)
{
	wlc_info_t *wlc = bsscfg->wlc;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(wlc->mfp, bsscfg);
	ASSERT(bss_mfp);

	if (memcmp((const uint8 *)BIP_OUI_TYPE, bip, WPA_SUITE_LEN) == 0) {
		bss_mfp->bip_type = WPA_CIPHER_BIP;
	} else {
		bss_mfp->bip_type = 0;
	}
}

bool
mfp_get_bip(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wpa_suite_t *bip)
{
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(wlc->mfp, bsscfg);
	/* Group Management Cipher Suite cannot be zero */
	if (bss_mfp->bip_type == 0)
		return FALSE;
	memcpy(bip->oui, WPA2_OUI, WPA2_OUI_LEN);
	bip->type = bss_mfp->bip_type;
	return TRUE;
}

bool
mfp_has_bip(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = bsscfg->wlc;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(wlc->mfp, bsscfg);

	/* Group Management Cipher Suite cannot be zero */
	return (bss_mfp ? (bss_mfp->bip_type != 0) : FALSE);
}

static bool
mfp_needs_mfp(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	const struct ether_addr* da, uint16 fc, uint8 cat,
	uint *iv_len, uint *tail_len)
{
	bool needs = FALSE;
	wlc_info_t *wlc = mfp->wlc;

	*iv_len = 0;
	*tail_len = 0;

	do {
		struct scb *scb = NULL;
		wlc_key_info_t key_info;

		if (!bsscfg || (!bcmwpa_includes_rsn_auth(bsscfg->WPA_auth)) ||
			!BSSCFG_IS_MFP_CAPABLE(bsscfg)) {
			break;
		}

		if (fc != FC_DEAUTH && fc !=  FC_DISASSOC && fc != FC_ACTION)
			break;

		if ((fc == FC_ACTION) && !mfp_robust_cat(bsscfg, cat))
			break;

		if (BSSCFG_AP(bsscfg) && ETHER_ISMULTI(da)) {
			(void)wlc_keymgmt_get_bss_tx_key(wlc->keymgmt, bsscfg, TRUE, &key_info);
			if (key_info.icv_len != 0) {
				*tail_len = WLC_KEY_MMIC_IE_LEN(&key_info);
				needs = TRUE;
			}
			break;
		}

		if (!ETHER_ISMULTI(da))
			scb = wlc_scbfindband(wlc, bsscfg, da,
				CHSPEC_BANDUNIT(bsscfg->current_bss->chanspec));

		if (!SCB_MFP(scb) || !SCB_AUTHORIZED(scb))
			break;

		/* pairwise key */
		(void)wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
			WLC_KEY_FLAG_NONE, &key_info);
		if (key_info.algo != CRYPTO_ALGO_OFF) {
			needs = TRUE;
			*iv_len = key_info.iv_len;
			*tail_len = key_info.icv_len;
		}
	} while (0);

	if (needs) {
		WL_WSEC(("wl%d: %s: MFP needed - iv: %d bytes, tail: %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, *iv_len, *tail_len));
	}

	return needs;
}

/* rx/tx */

/* protected frame rx procesing. see 11.8.2.7 IEEE 802.11/2012
 *
frame type       Have Key			              No Key
-----------------------------------------------------------------
		Enc		No-Enc			Enc			No-Enc

deauth
disassoc	ok		toss			toss		ok

robust
action      ok		toss			toss		toss

non-robust
action 		toss	ok				toss		ok
 *
 *
 * PKTDATA(p) must point to body
 */
bool
wlc_mfp_rx(wlc_mfp_info_t *mfp, const wlc_bsscfg_t *bsscfg, struct scb *scb,
	d11rxhdr_t *rxhdr, struct dot11_management_header *hdr, void *p)
{
	uint16 fc = ltoh16(hdr->fc);
	uint16 fk = fc & FC_KIND_MASK;
	bool ret = TRUE;
	wlc_info_t *wlc = mfp->wlc;
	uint8 *body;
	int body_len;
	wlc_key_t *key;
	int body_offset;
	wlc_key_info_t key_info;

#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char ea_str[ETHER_ADDR_STR_LEN];
#endif // endif

	/* ignore what MFP does not care about */
	if (scb == NULL || !IS_MFP_FC(fk))
		return TRUE;

	WL_WSEC(("wl%d: %s: management frame from %s,"
		"type = 0x%02x, subtype = 0x%02x\n", WLCWLUNIT(wlc), __FUNCTION__,
		bcm_ether_ntoa(&hdr->sa, ea_str), FC_TYPE(fc), FC_SUBTYPE(fc)));

	body = (uint8*)PKTDATA(wlc->osh, p);
	body_len = PKTLEN(wlc->osh, p);
	body_offset = (int)((uint8*)body - (uint8*)hdr);

	PKTPUSH(wlc->osh, p, body_offset);

	/* If MFP is not applicable, the frame must not be encrypted */
	if (!(SCB_MFP(scb) && bsscfg &&
		(bcmwpa_includes_rsn_auth(bsscfg->WPA_auth)))) {
		ret = !(fc & FC_WEP);
		if (!ret)
			WLCNTINCR(wlc->pub->_cnt->rxundec);
		goto done;
	}

	if (ETHER_ISMULTI(&hdr->da)) { /* bcast/mcast */
		mmic_ie_t *ie;
		wlc_key_algo_t bss_algo;
		int ie_len;

		if (fc & FC_WEP) { /* 8.2.4.1.9 IEEE 802.11/2012 */
			ret = FALSE;
			WL_WSEC(("wl%d: %s: multicast frame from %s "
				"with protected frame bit set, toss\n", WLCWLUNIT(wlc),
				__FUNCTION__, bcm_ether_ntoa(&hdr->sa, ea_str)));
			goto done;
		}

		bss_algo = wlc_keymgmt_get_bss_key_algo(wlc->keymgmt, bsscfg, TRUE);
		if (bss_algo == CRYPTO_ALGO_NONE)
			goto done;

		ie_len = OFFSETOF(mmic_ie_t, mic) +
			((bss_algo == CRYPTO_ALGO_BIP) ?  BIP_MIC_SIZE : AES_BLOCK_SZ);

		if (body_len < ie_len) {
			WL_WSEC(("%s: mmie error: body length %d too small\n",
				__FUNCTION__, body_len));
			WLCNTINCR(wlc->pub->_cnt->rxrunt);
			ret = FALSE;
			goto done;
		}

		ie = (mmic_ie_t *)(body + body_len - ie_len);
		key = wlc_keymgmt_get_bss_key(wlc->keymgmt, bsscfg,
			(wlc_key_id_t)ltoh16_ua(&ie->key_id), &key_info);
	} else {
		ASSERT(scb != NULL);
		key = wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
			WLC_KEY_FLAG_NONE, &key_info);
	}

	if (wlc_key_rx_mpdu(key, p, rxhdr) != BCME_OK)
		ret = FALSE;

	if (fc & FC_WEP) { /* encrypted and unicast */
		if (!ret)
			goto done;

		/* adjust body - strip iv and icv */
		body += key_info.iv_len;
		body_offset += key_info.iv_len;
		body_len -= (key_info.iv_len + key_info.icv_len);

		if (fk == FC_ACTION) /* action must be robust */
			ret = (body_len > 0) && mfp_robust_cat(bsscfg, body[0]);
		if (!ret) {
			WL_WSEC(("wl%d: %s: received encrypted non-robust "
				"action frame from %s, toss\n",
				WLCWLUNIT(wlc), __FUNCTION__,
				bcm_ether_ntoa(&hdr->sa, ea_str)));
			WLCNTINCR(wlc->pub->_cnt->rxbadproto);
			goto done;
		}
		goto done;
	}

	/* unencrypted unicast frame */
	switch (fk) {
	case FC_DEAUTH:
	case FC_DISASSOC:
		if (!ret) { /* disallowed deauth/disassoc get sa query */
			WL_WSEC(("wl%d: %s: starting SA Query %s\n", WLCWLUNIT(wlc),
				__FUNCTION__, bcm_ether_ntoa(&hdr->sa, ea_str)));
			wlc_mfp_start_sa_query(mfp, bsscfg, scb);
		}
		break;
	case FC_ACTION:
		if (!ret)
			break;

		if (body_len < 1) {
			WLCNTINCR(wlc->pub->_cnt->rxrunt);
			ret = FALSE;
			break;
		}

		if (mfp_robust_cat(bsscfg, body[0])) {
			WL_WSEC(("wl%d: %s: received robust action frame unprotected from "
				"%s, toss\n", WLCWLUNIT(wlc), __FUNCTION__,
				bcm_ether_ntoa(&hdr->sa, ea_str)));
			WLCNTINCR(wlc->pub->_cnt->rxbadproto);
			ret = FALSE;
			break;
		}
		break;
	default:
		ASSERT(0);
	}

done:
	PKTPULL(wlc->osh, p, body_offset);
	WL_WSEC(("wl%d: %s: %s\n", WLCWLUNIT(wlc), __FUNCTION__, ret ? "okay" : "failed"));
	return ret;
}

void*
wlc_mfp_frame_get_mgmt(wlc_mfp_info_t *mfp, uint16 fc, uint8 cat,
	const struct ether_addr *da, const struct ether_addr *sa,
	const struct ether_addr *bssid, uint body_len, uint8 **pbody, bool mfp_check_required)
{
	void *p;
	uint iv_len = 0, tail_len = 0;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc = mfp->wlc;
	bool needs_mfp;
#ifdef BCMDBG
	char ea_str[ETHER_ADDR_STR_LEN];
#endif // endif

	bsscfg = wlc_bsscfg_find_by_hwaddr_bssid(wlc, sa, bssid);
	if (bsscfg == NULL)
		bsscfg = wlc_bsscfg_find_by_target_bssid(wlc, bssid);

	if (mfp_check_required == FALSE)
		needs_mfp = FALSE;
	else
		needs_mfp = mfp_needs_mfp(mfp, bsscfg, da, fc, cat, &iv_len, &tail_len);
	if (!needs_mfp) {
		WL_NONE(("wl%d: %s: no frame protection for %s\n",
			WLCWLUNIT(wlc), __FUNCTION__, bcm_ether_ntoa(bssid, ea_str)));
	}

	p = wlc_frame_get_mgmt_ex(wlc, fc, da, sa, bssid, body_len, pbody,
		iv_len, tail_len);

	if (needs_mfp && (p != NULL))
		WLPKTTAG(p)->flags |= WLF_MFP;

	return (p);
}

uint
wlc_mfp_frame_get_sec_len(wlc_mfp_info_t *mfp, uint16 fc, uint8 cat,
	const struct ether_addr *da, wlc_bsscfg_t *bsscfg)
{
	uint iv_len = 0, tail_len = 0;
	uint sec_len = 0;
	wlc_info_t *wlc = mfp->wlc;

	if (!WLC_MFP_ENAB(wlc->pub)) {
		goto done;
	}

	if (mfp_needs_mfp(mfp, bsscfg, da, fc, cat, &iv_len, &tail_len)) {
		sec_len = iv_len;
	}

done:
	return sec_len;
}

#ifdef MFP_TEST
void*
mfp_test_send_sa_query(wlc_info_t *wlc, struct scb *scb,
	uint8 action, uint16 id)
{
	void *p = NULL;
	if (wlc->mfp)
		p = mfp_send_sa_query(wlc->mfp, scb, action, id);
	return p;
}

int
mfp_test_iov(const int mfp_iov)
{
	int iov;
	switch (IOV_ID(mfp_iov)) {
	case IOV_MFP_SA_QUERY: iov = IOV_MFP_TEST_SA_QUERY; break;
	case IOV_MFP_DISASSOC: iov = IOV_MFP_TEST_DISASSOC; break;
	case IOV_MFP_DEAUTH:   iov = IOV_MFP_TEST_DEAUTH;   break;
	case IOV_MFP_ASSOC:    iov = IOV_MFP_TEST_ASSOC;    break;
	case IOV_MFP_AUTH:     iov = IOV_MFP_TEST_AUTH;     break;
	case IOV_MFP_REASSOC:  iov = IOV_MFP_TEST_REASSOC;  break;
	case IOV_MFP_BIP_TEST: iov = IOV_MFP_TEST_BIP;      break;
	default:               iov = IOV_MFP_TEST_INVALID;  break;
	}
	iov = (IOV_ISSET(mfp_iov)) ? IOV_SVAL(iov) : IOV_GVAL(iov);
	return iov;
}
#endif /* MFP_TEST */
static int
mfp_bsscfg_init(void* ctx, wlc_bsscfg_t *bsscfg)
{
	if (WLC_MFP_ENAB(bsscfg->wlc->pub) && wlc_bsscfg_mfp_supported(bsscfg))
		bsscfg->flags2 |= WLC_BSSCFG_FL2_MFP_CAPABLE;
	return BCME_OK;
}

static void
mfp_bsscfg_deinit(void* ctx, wlc_bsscfg_t *bsscfg)
{
	if (WLC_MFP_ENAB(bsscfg->wlc->pub) && wlc_bsscfg_mfp_supported(bsscfg))
		bsscfg->flags2 &= ~WLC_BSSCFG_FL2_MFP_CAPABLE;
}
#endif /* MFP */
