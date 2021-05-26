/*
 * Broadcom 802.11 Networking Device Driver
 * Management frame protection
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
 * $Id$
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

#include <bcmcrypto/prf.h>
#include <proto/802.11.h>
#include <proto/eap.h>
#include <proto/eapol.h>
#include <proto/wpa.h>

#include <wlc_types.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_assoc.h>
#include <wlc_security.h>
#include <wlc_frmutil.h>

#include <wl_export.h>

#include <wlc_mfp.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>

#ifdef MFP_TEST
#include <wlc_mfp_test.h>
#endif // endif

/* buffer size to fPRF */
#define PRF_RESULT_LEN 80

/* random key size for initializing IPN */
#define IPN_INIT_KEY_SZ 32

/* toggle IGTK index for use as ID */
#define ISIGTK1(x) ((x) == IGTK_INDEX_1)
#define IGTK_NEXT_ID(k)  (ISIGTK1(k->id) ? IGTK_INDEX_2 : IGTK_INDEX_1)

/* id for SA query - must not be zero */
#define MFP_SA_QUERY_ID(mfp) (((uint16)(mfp->wlc)->counter) | 0x8000)

/* module state */
struct wlc_mfp_info {
	wlc_info_t	*wlc;	/* wlc info pointer */
	int h_bsscfg;		/* bsscfg cubby handle */
	int h_scb;			/* scb cubby handle */
};

/* bsscfg cubby - maintains igtk */
struct mfp_bsscfg {
	wlc_mfp_igtk_info_t igtk;
	uint8 bip_type;
};
typedef struct mfp_bsscfg mfp_bsscfg_t;
#define MFP_BSSCFG(m, b) ((mfp_bsscfg_t*)BSSCFG_CUBBY(b, (m)->h_bsscfg))

/* scb cubby - maintains SA query state */
struct sa_query {
	bool        		started;
	uint16      		id;
	uint32      		timeouts;
	struct wl_timer		*timer;
};
typedef struct sa_query sa_query_t;

struct mfp_scb {
	sa_query_t saq;
};
typedef struct mfp_scb mfp_scb_t;
#define MFP_SCB(m, s) ((mfp_scb_t*)SCB_CUBBY(s, (m)->h_scb))

/* iovar support */
enum {
	IOV_MFP,
#ifdef AP
	IOV_MFP_BIP,
#endif // endif
#ifdef MFP_TEST
	IOV_MFP_SHA256,
	IOV_MFP_SA_QUERY,
	IOV_MFP_DISASSOC,
	IOV_MFP_DEAUTH,
	IOV_MFP_ASSOC,
	IOV_MFP_AUTH,
	IOV_MFP_REASSOC,
	IOV_MFP_BIP_TEST
#endif // endif
};

static const bcm_iovar_t mfp_iovars[] = {
	{"mfp", IOV_MFP, IOVF_BSS_SET_DOWN, IOVT_INT32, 0},
#ifdef AP
	{"bip", IOV_MFP_BIP, IOVF_SET_UP, IOVT_BUFFER, 0},
#endif // endif
#ifdef MFP_TEST
	{"mfp_sha256", IOV_MFP_SHA256, IOVF_BSS_SET_DOWN, IOVT_INT32, 0},
	{"mfp_sa_query", IOV_MFP_SA_QUERY, IOVF_SET_UP, IOVT_INT32, 0},
	{"mfp_disassoc", IOV_MFP_DISASSOC, IOVF_SET_UP, IOVT_INT32, 0},
	{"mfp_deauth", IOV_MFP_DEAUTH, IOVF_SET_UP, IOVT_INT32, 0},
	{"mfp_assoc", IOV_MFP_ASSOC, IOVF_SET_UP, IOVT_INT32, 0},
	{"mfp_auth", IOV_MFP_AUTH, IOVF_SET_UP, IOVT_INT32, 0},
	{"mfp_reassoc", IOV_MFP_REASSOC, IOVF_SET_UP, IOVT_INT32, 0},
	{"mfp_bip_test", IOV_MFP_BIP_TEST, IOVF_SET_UP, IOVT_INT32, 0},
#endif // endif
	{NULL, 0, 0, 0, 0}
};

/* values for IOV_MFP arg */
enum {
	IOV_MFP_NONE = 0,
	IOV_MFP_CAPABLE,
	IOV_MFP_REQUIRED 	/* implies capable */
};

/* prototypes - forward reference */

/* rx */
static bool mfp_rx_mcast(wlc_mfp_igtk_info_t *igtk, d11rxhdr_t *rxh,
	const struct dot11_management_header *hdr, void *body, int body_len);
static bool mfp_rx_ucast(wlc_mfp_info_t *mfp, d11rxhdr_t *rxh,
	struct dot11_management_header *hdr, void *p, struct scb *scb);

/* tx */
static void mfp_tx_mod_len(const struct scb *scb, uint *iv_len, uint *tail_len);

/* compute MIC for BIP; body length includes the entire trailing MIC IE.
 * mic must be of size aes block - AES_BLOCK_SZ
 */
static void mfp_bip_mic(const struct dot11_management_header *hdr,
	const uint8 *body, int body_len, const uint8 *key, uint8 * mic);

/* sa query */
static void* mfp_send_sa_query(wlc_mfp_info_t *mfp, struct scb *scb,
	uint8 action, uint16 id);

/* igtk  support */
static void mfp_init_ipn(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	wlc_mfp_igtk_info_t *igtk);

/* utils */

/* is category robust */
static bool mfp_robust_cat(uint8 cat);

/* IE mgmt */
#ifdef AP
static uint wlc_mfp_calc_to_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_mfp_write_to_ie(void *ctx, wlc_iem_build_data_t *data);
#endif // endif

/* end prototypes */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* iovar callback */
static int
mfp_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	wlc_info_t *wlc = mfp->wlc;
	int err = BCME_OK;
	wlc_bsscfg_t *bsscfg;
	int32 *ret_int_ptr;
	int32 flag;

	ret_int_ptr = (int32*)arg;
	flag = *ret_int_ptr;
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_MFP):
		*ret_int_ptr = (bsscfg->wsec & MFP_REQUIRED) ? IOV_MFP_REQUIRED :
			((bsscfg->wsec & MFP_CAPABLE) ? IOV_MFP_CAPABLE : IOV_MFP_NONE);
		break;
	case IOV_SVAL(IOV_MFP): {
		uint32 wsec = bsscfg->wsec & ~(MFP_CAPABLE|MFP_REQUIRED);
		switch (flag) {
		case IOV_MFP_NONE:
			break;
		case IOV_MFP_REQUIRED:
			wsec |= MFP_REQUIRED;
			/* fall through */
		case IOV_MFP_CAPABLE:
			wsec |= MFP_CAPABLE;
			break;
		default:
			err = BCME_BADARG;
			break;
		}

		if (err != BCME_OK)
			break;

		bsscfg->wsec  = wsec;
		break;
	}
#ifdef AP
	case IOV_SVAL(IOV_MFP_BIP): {
		 mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, bsscfg);
		/* Validate only since we only support one algorithm for now */
		if (bcmp((const uint8 *)BIP_OUI_TYPE, (uint8 *)arg, WPA_SUITE_LEN)) {
			WL_ERROR(("wl%d: %s: unsupported BIP type\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			err = BCME_BADARG;
		}
		else {
			bss_mfp->bip_type = WPA_CIPHER_BIP;
		}
		break;
	}
#endif /* AP */
	default:
#ifdef MFP_TEST
		err = mfp_test_doiovar(mfp->wlc, vi, mfp_test_iov(actionid), name,
			params, p_len, arg, len, val_size, wlcif);
#else
		err = BCME_UNSUPPORTED;
#endif // endif
		break;
	}

	return err;
}

/* cubby support */
static int
mfp_bsscfg_init(void* ctx, wlc_bsscfg_t* cfg)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, cfg);
	memset(bss_mfp, 0, sizeof(*bss_mfp));
	return BCME_OK;
}

static void
mfp_bsscfg_deinit(void* ctx, wlc_bsscfg_t* cfg)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, cfg);
	memset(bss_mfp, 0, sizeof(*bss_mfp));
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void
mfp_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_mfp_info_t* mfp = (wlc_mfp_info_t*)ctx;
	mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(mfp, cfg);
	wlc_mfp_igtk_info_t *igtk = &bss_mfp->igtk;

	bcm_bprintf(b, "\tbip: %02x\n", bss_mfp->bip_type);
	if (!igtk->key_len) {
		bcm_bprintf(b, "\tigtk: not configured\n");
		return;
	}

	bcm_bprintf(b, "\tigtk: \n");
	bcm_bprhex(b, "\t\tkey: ", TRUE, igtk->key, sizeof(igtk->key));
	bcm_bprintf(b, "\t\tid: %04x len: %hu ipn: 0x%04x%08x\n",
		igtk->id, igtk->key_len, igtk->ipn_hi, igtk->ipn_lo);
}
#else
#define mfp_bsscfg_dump NULL
#endif /* BCMDBG || BCMDBG_DUMP */

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

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
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
#ifdef AP
	uint16 arsfstbmp = FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP);
#endif // endif
	wlc->pub->_mfp = FALSE;

	/* Assertion to make sure we have an rx IV for MFP */
#if !defined(MFP_DISABLED)
	#define ctr(x) (WPA_CAP_ ## x ## _REPLAY_CNTRS == WLC_REPLAY_CNTRS_VALUE)
	STATIC_ASSERT((ctr(16) && WLC_NUMRXIVS > 16) ||
		(ctr(4) && WLC_NUMRXIVS > 4));
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
		NULL /* wdog */, NULL /* up */, NULL /* down */) != BCME_OK)
		goto err;

	mfp->h_bsscfg = wlc_bsscfg_cubby_reserve(wlc, sizeof(mfp_bsscfg_t),
		mfp_bsscfg_init, mfp_bsscfg_deinit,
		mfp_bsscfg_dump, (void*)mfp);
	if (mfp->h_bsscfg < 0)
		goto err;

	mfp->h_scb = wlc_scb_cubby_reserve(wlc, sizeof(mfp_scb_t),
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
		mfp_scb_init, mfp_scb_deinit, mfp_scb_dump, (void*)mfp, 0);
#else
		mfp_scb_init, mfp_scb_deinit, mfp_scb_dump, (void*)mfp);
#endif // endif
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

	wlc->pub->_mfp = TRUE;
	return mfp;

err:
	WL_ERROR(("wl%d: wlc_module_register(mfp) failed\n", WLCWLUNIT(wlc)));
	wlc_mfp_detach(mfp);
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

/* igtk support */
#if defined(WLWNM) && (defined(BCMSUP_PSK) || defined(BCMSUPPL))
int
wlc_mfp_igtk_update(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg, int key_len, uint16 key_id,
	uint8 *pn, uint8 *key)
{
	wlc_mfp_igtk_info_t *igtk = &MFP_BSSCFG(mfp, bsscfg)->igtk;

	if (key_len < AES_TK_LEN) {
		WL_ERROR(("wl%d: %s: IGTK length is not %d\n", WLCWLUNIT(mfp->wlc),
			__FUNCTION__, AES_TK_LEN));
		igtk->key_len = 0;
		return BCME_ERROR;
	}

	igtk->id = key_id;
	igtk->ipn_lo = ltoh32_ua(pn);
	igtk->ipn_hi = ltoh16_ua(pn + 4);
	igtk->key_len = AES_TK_LEN;

	memcpy(igtk->key, key, AES_TK_LEN);

	WL_WNM(("WNM-Sleep IGTK update id:%d\n", igtk->id));
	WL_WSEC(("%s: ipn %04x%08x\n", __FUNCTION__, igtk->ipn_hi, igtk->ipn_lo));

	return BCME_OK;
}
#endif /* WLWNM && (BCMSUP_PSK || BCMSUPPL) */

#if defined(BCMSUP_PSK) || defined(BCMSUPPL)
bool
wlc_mfp_extract_igtk(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	const eapol_header_t* eapol)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	uint16 data_len = ntoh16_ua(&body->data_len);
	eapol_wpa2_encap_data_t *data_encap;
	eapol_wpa2_key_igtk_encap_t *igtk_kde;
	wlc_mfp_igtk_info_t *igtk = &MFP_BSSCFG(mfp, bsscfg)->igtk;

	/* extract IGTK */
	data_encap = wpa_find_kde(body->data, data_len, WPA2_KEY_DATA_SUBTYPE_IGTK);
	if (!data_encap) {
		WL_WSEC(("wl%d: %s: IGTK KDE not found in EAPOL\n",
			WLCWLUNIT(mfp->wlc), __FUNCTION__));
		igtk->key_len = 0;
		return FALSE;
	}

	igtk->key_len = data_encap->length -
		((EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
		EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN);

	if (igtk->key_len < AES_TK_LEN) {
		WL_ERROR(("wl%d: %s: IGTK length is not %d\n", WLCWLUNIT(mfp->wlc),
			__FUNCTION__, AES_TK_LEN));
		igtk->key_len = 0;
		return FALSE;
	}

	igtk_kde = (eapol_wpa2_key_igtk_encap_t *)data_encap->data;
	igtk->id = igtk_kde->key_id;
	igtk->ipn_lo = ltoh32(*(uint32 *)igtk_kde->ipn);
	igtk->ipn_hi = ltoh16(*(uint16 *)(igtk_kde->ipn + 4));
	igtk->key_len = AES_TK_LEN;

	WL_WNM(("Regular IGTK update id:%d\n", igtk->id));

	ASSERT(AES_TK_LEN <= sizeof(igtk->key));
	memcpy(igtk->key, igtk_kde->key, AES_TK_LEN);

	WL_WSEC(("%s: ipn %04x%08x\n", __FUNCTION__, igtk->ipn_hi, igtk->ipn_lo));
	return TRUE;
}
#endif /* BCMSUP_PSK || BCMSUPPL */

#ifdef BCMAUTH_PSK
int
wlc_mfp_insert_igtk(const wlc_mfp_info_t *mfp, const wlc_bsscfg_t *bsscfg,
	eapol_header_t *eapol, uint16 *data_len)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	eapol_wpa2_encap_data_t *data_encap;
	uint16 len = *data_len;
	eapol_wpa2_key_igtk_encap_t *igtk_encap;
	wlc_mfp_igtk_info_t *igtk = &MFP_BSSCFG(mfp, bsscfg)->igtk;

	data_encap = (eapol_wpa2_encap_data_t *) (body->data + len);
	data_encap->type = DOT11_MNG_PROPR_ID;
	data_encap->length = (EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
		EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN + igtk->key_len;
	memcpy(data_encap->oui, WPA2_OUI, DOT11_OUI_LEN);
	data_encap->subtype = WPA2_KEY_DATA_SUBTYPE_IGTK;
	len += EAPOL_WPA2_ENCAP_DATA_HDR_LEN;
	igtk_encap = (eapol_wpa2_key_igtk_encap_t *) (body->data + len);

	igtk_encap->key_id = igtk->id;
	*(uint32 *)igtk_encap->ipn = htol32(igtk->ipn_lo);
	*(uint16 *)(igtk_encap->ipn + 4) = htol16(igtk->ipn_hi);

	memcpy(igtk_encap->key, igtk->key, igtk->key_len);
	len += igtk->key_len + EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN;

	/* return the adjusted data len */
	*data_len = len;
	return (igtk->key_len + EAPOL_WPA2_KEY_IGTK_ENCAP_HDR_LEN + EAPOL_WPA2_ENCAP_DATA_HDR_LEN);
}
#endif /* BCMAUTH_PSK */

void
wlc_mfp_reset_igtk(const wlc_mfp_info_t* mfp, wlc_bsscfg_t* bsscfg)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	memset(mfp_bsscfg, 0, sizeof(*mfp_bsscfg));
	mfp_bsscfg->igtk.id = IGTK_INDEX_2; /* we start from INDEX_1 */
}

uint16
wlc_mfp_gen_igtk(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint8 *master_key, uint32 master_key_len)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_mfp_igtk_info_t *igtk = &mfp_bsscfg->igtk;
	unsigned char data[ETHER_ADDR_LEN +
		sizeof(igtk->ipn_lo) + sizeof(igtk->ipn_hi)];
	unsigned char prf_buff[PRF_RESULT_LEN];
	unsigned char prefix[] = "Group key expansion";
	int data_len = 0;

	igtk->id = IGTK_NEXT_ID(igtk);
	igtk->key_len = AES_TK_LEN;

	/* create the the data portion */
	memcpy((char*)&data[data_len], (char*)&bsscfg->cur_etheraddr,
		ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	/* generate a fresh ipn also */
	mfp_init_ipn(mfp, bsscfg, igtk);

	*(uint32 *)&data[data_len] = htol32(igtk->ipn_lo);
	*(uint16 *)&data[data_len+sizeof(uint32)] = htol16(igtk->ipn_hi);
	data_len += 6;

	/* generate the IGTK */
	fPRF(master_key, master_key_len, prefix, (sizeof(prefix) - 1),
		data, data_len, prf_buff, AES_TK_LEN);

	ASSERT(AES_TK_LEN <= sizeof(igtk->key));
	memcpy(igtk->key, prf_buff, AES_TK_LEN);

	WL_WSEC(("%s: generate igtk id %d ipn %04x%08x\n", __FUNCTION__,
		igtk->id, igtk->ipn_hi, igtk->ipn_lo));

	return igtk->key_len;
}

uint16
wlc_mfp_igtk_len(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_mfp_igtk_info_t *igtk = &mfp_bsscfg->igtk;
	return igtk->key_len;
}

void
wlc_mfp_igtk_ipn(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint32* lo, uint16* hi)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_mfp_igtk_info_t *igtk = &mfp_bsscfg->igtk;
	*lo = igtk->ipn_lo;
	*hi = igtk->ipn_hi;
}

uint16
wlc_mfp_igtk_id(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_mfp_igtk_info_t *igtk = &mfp_bsscfg->igtk;
	return igtk->id;
}

void
wlc_mfp_igtk_key(const wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	uint8 *key)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_mfp_igtk_info_t *igtk = &mfp_bsscfg->igtk;

	memcpy(key, igtk->key, BIP_KEY_SIZE);
}

static void
mfp_init_ipn(wlc_mfp_info_t *mfp, wlc_bsscfg_t *bsscfg,
	wlc_mfp_igtk_info_t *igtk)
{
	unsigned char buff[IPN_INIT_KEY_SZ];
	unsigned char prf_buff[PRF_RESULT_LEN];
	unsigned char prefix[] = "Init Counter";

	wlc_getrand(mfp->wlc, &buff[0], IPN_INIT_KEY_SZ);

	/* Still not exactly right, but better. */
	fPRF(buff, sizeof(buff), prefix, sizeof(prefix) - 1,
		(unsigned char *)&bsscfg->cur_etheraddr, ETHER_ADDR_LEN, prf_buff, 32);
	memcpy(&igtk->ipn_lo, prf_buff, sizeof(uint32));
	memcpy(&igtk->ipn_hi, prf_buff+sizeof(uint32), sizeof(uint16));

	if (igtk->ipn_lo++ == 0)
		igtk->ipn_hi++;

	WL_WSEC(("%s:  generated ipn %04x%08x \n", __FUNCTION__,
		igtk->ipn_hi, igtk->ipn_lo));
}

void wlc_mfp_igtk_from_key(const wlc_mfp_info_t *mfp, wlc_bsscfg_t* bsscfg,
	const wl_wsec_key_t* key)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_mfp_igtk_info_t *igtk = &mfp_bsscfg->igtk;

	ASSERT(key != NULL);
	if (key->len > sizeof(igtk->key)) {
		WL_WSEC(("%s: igtk key length %d is too long, truncating.\n",
			__FUNCTION__, key->len));
		igtk->key_len = sizeof(igtk->key);
	} else {
		igtk->key_len = (ushort)key->len;
	}

	memcpy(&igtk->key, &key->data, igtk->key_len);
	igtk->id = (uint16)key->index;

	/* Note: lo and hi are interchanged between wsec_key and igtk. */
	igtk->ipn_lo = key->rxiv.hi;
	igtk->ipn_hi = key->rxiv.lo;

	WL_WSEC(("%s: set igtk from key. ipn %04x%08x key_len 0x%x\n", __FUNCTION__,
		igtk->ipn_hi, igtk->ipn_lo, key->len));
}

void
wlc_mfp_set_igtk(const wlc_mfp_info_t* mfp, wlc_bsscfg_t* bsscfg,
	const wlc_mfp_igtk_info_t* igtk)
{
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_mfp_igtk_info_t *bss_igtk = &mfp_bsscfg->igtk;

	if (igtk)
		*bss_igtk = *igtk;
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
	p = wlc_frame_get_action(mfp->wlc, FC_ACTION, &scb->ea,
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

	/* id mismatch */
	q->id = MFP_SA_QUERY_ID(mfp);
	mfp_send_sa_query(mfp, scb, SA_QUERY_REQUEST, q->id);
}

void
wlc_mfp_handle_sa_query(wlc_mfp_info_t *mfp, struct scb *scb, uint action,
	const struct dot11_management_header *hdr, const uint8 *body, int body_len)
{
	const struct dot11_action_sa_query *af;

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
		wlc_scb_clearstatebit(scb, ASSOCIATED | AUTHORIZED);
		wlc_scb_disassoc_cleanup(wlc, scb);
#ifdef EXT_STA
		if (WLEXTSTA_ENAB(wlc->pub)) {

			wlc_disassoc_complete(cfg, WLC_E_STATUS_SUCCESS, &cfg->BSSID,
				DOT11_RC_DISASSOC_LEAVING, DOT11_BSSTYPE_INFRASTRUCTURE);
		}
#endif // endif
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
 * see table 8-38 IEEE 802.11/ 2012.  should be ROMmed
 */
static const uint8 mfp_robust_categories[] = {
	0x6f, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static INLINE bool
mfp_robust_cat(uint8 cat)
{
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

bool
wlc_mfp_check_rsn_caps(const wlc_mfp_info_t* mfp, uint32 wsec, uint16 rsn,
	bool *enable)
{
	bool ret = TRUE;

	*enable = FALSE; /* MFP does not apply */
	/* Association is allowed for MFPR 1 and MFPC 0 case.
	 * It is to fix Nexux 7 interop issue with Atlas router.
	 */
	if ((wsec & MFP_CAPABLE) && (rsn & RSN_CAP_MFPC))
		*enable = TRUE; /* MFP applies */
	else if ((wsec & MFP_REQUIRED) && !(rsn & RSN_CAP_MFPC))
		ret = FALSE;
	else if (!(wsec & MFP_CAPABLE) && (rsn & RSN_CAP_MFPR))
		ret =  FALSE;

	return ret;
}

uint8
wlc_mfp_wsec_to_rsn_caps(const wlc_mfp_info_t *mfp, uint32 wsec)
{
	uint8 rsn = 0;
	if (wsec & MFP_CAPABLE)
		rsn |= RSN_CAP_MFPC;
	if (wsec & MFP_REQUIRED)
		rsn |= RSN_CAP_MFPR;
	return rsn;
}

uint8
wlc_mfp_rsn_caps_to_flags(const wlc_mfp_info_t *mfp, uint8 rsn)
{
	uint8 flags = 0;
	if (rsn & RSN_CAP_MFPC)
		flags |= RSN_FLAGS_MFPC;
	if (rsn & RSN_CAP_MFPR)
		flags |= RSN_FLAGS_MFPR;
	return flags;
}

/* determine space to reserve for iv and icv */
static void
mfp_tx_mod_len(const struct scb *scb, uint *iv_len, uint *tail_len)
{
	if (scb == NULL || scb->key == NULL) {
		*iv_len = 0;
		*tail_len = 0;
		return;
	}

	*iv_len = scb->key->iv_len;
	*tail_len = scb->key->icv_len;
}

bool
mfp_get_bip(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wpa_suite_t *bip)
{
	 mfp_bsscfg_t* bss_mfp = MFP_BSSCFG(wlc->mfp, bsscfg);
	 /* Group Management Cipher Suite cannot be zero */
	if (bss_mfp->bip_type == WPA_CIPHER_NONE) {
		 return FALSE;
	}
	memcpy(bip->oui, WPA2_OUI, WPA2_OUI_LEN);
	bip->type = bss_mfp->bip_type;
	return TRUE;
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

		if (!bsscfg || (!bcmwpa_includes_rsn_auth(bsscfg->WPA_auth)) ||
#ifdef WL_SAE
			(!bcmwpa_includes_wpa3_auth(bsscfg->WPA_auth)) ||
#endif /* WL_SAE */
			!(bsscfg->wsec & MFP_CAPABLE))
			break;

		if (fc != FC_DEAUTH && fc !=  FC_DISASSOC && fc != FC_ACTION)
			break;

		if ((fc == FC_ACTION) && !mfp_robust_cat(cat))
			break;

		if (BSSCFG_AP(bsscfg) && ETHER_ISMULTI(da) && (fc != FC_ACTION)) {
			mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
			if (mfp_bsscfg->igtk.key_len) {
				*tail_len = OFFSETOF(mmic_ie_t, mic) + BIP_MIC_SIZE;
				needs = TRUE;
			}
			break;
		}

		if (!ETHER_ISMULTI(da))
			scb = wlc_scbfindband(wlc, bsscfg, da,
				CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));

		if (!SCB_MFP(scb))
			break;

		ASSERT(scb != NULL);
		if (scb->key) {
			needs = TRUE;
			mfp_tx_mod_len(scb, iv_len, tail_len);
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
	mfp_bsscfg_t *mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	wlc_info_t *wlc = mfp->wlc;
	uint8* body;
	int body_len;
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char ea_str[ETHER_ADDR_STR_LEN];
#endif // endif

	ASSERT(fk == FC_DEAUTH || fk == FC_DISASSOC || fk == FC_ACTION);

	WL_WSEC(("wl%d: %s: management frame from %s,"
		"type = 0x%02x, subtype = 0x%02x\n", WLCWLUNIT(wlc), __FUNCTION__,
		bcm_ether_ntoa(&hdr->sa, ea_str), FC_TYPE(fc), FC_SUBTYPE(fc)));

	if (scb && scb->key && scb->key->algo != CRYPTO_ALGO_AES_CCM)
		return ret; /* Non-AES CCM key okay */

	/* If MFP is not applicable, the frame must not be encrypted */
	if (!(SCB_MFP(scb) && bsscfg && (
#ifdef WL_SAE
		bcmwpa_includes_wpa3_auth(bsscfg->WPA_auth) ||
#endif /* WL_SAE */
		bcmwpa_includes_rsn_auth(bsscfg->WPA_auth)))) {
		ret = !(fc & FC_WEP);
		if (!ret)
			WLCNTINCR(wlc->pub->_cnt->rxundec);
		return ret;
	}

	if (ETHER_ISMULTI(&hdr->da)) { /* bcast/mcast */
		if (fc & FC_WEP) { /* 8.2.4.1.9 IEEE 802.11/2012 */
			ret = FALSE;
			WL_WSEC(("wl%d: %s: multicast frame from %s "
				"with protected frame bit set, toss\n",
				WLCWLUNIT(wlc), __FUNCTION__,
				bcm_ether_ntoa(&hdr->sa, ea_str)));
			return ret;
		}

		body = (uchar*)PKTDATA(wlc->osh, p);
		body_len = PKTLEN(wlc->osh, p);
		ret = mfp_rx_mcast(&mfp_bsscfg->igtk, rxhdr, hdr, body, body_len);
		WL_WSEC(("wl%d: %s: decryption %s for"
			" protected bcast/mcast mgmt frame from %s\n",
			WLCWLUNIT(wlc), __FUNCTION__,
			ret ? "successful" : "failed", bcm_ether_ntoa(&hdr->sa, ea_str)));

		if (!ret)
			WLCNTINCR(wlc->pub->_cnt->rxundec);
		return ret;
	}

	/* handle ucast encrypted */
	ASSERT(scb != NULL);
	if (fc & FC_WEP) { /* encrypted */
		if (scb->key) { /* encrypted and have key */
			ret = mfp_rx_ucast(mfp, rxhdr, hdr, p, scb);
			WL_WSEC(("wl%d: %s: decryption %s for"
				" protected unicast mgmt frame from %s\n",
				WLCWLUNIT(wlc), __FUNCTION__,
				ret ? "successful" : "failed",
				bcm_ether_ntoa(&hdr->sa, ea_str)));
			if (!ret) {
				WLCNTINCR(wlc->pub->_cnt->rxundec);
				return ret;
			}

			body = (uchar*)PKTDATA(wlc->osh, p);
			body_len = PKTLEN(wlc->osh, p);
			if (fk == FC_ACTION) /* action must be robust */
				ret = mfp_robust_cat(body[0]);
			if (!ret) {
				WL_WSEC(("wl%d: %s: received encrypted"
					" non-robust action frame from %s, toss\n",
					WLCWLUNIT(wlc), __FUNCTION__,
					bcm_ether_ntoa(&hdr->sa, ea_str)));
				WLCNTINCR(wlc->pub->_cnt->rxbadproto);
				return ret;
			}
		} else { /* encrypted no key */
			ret = FALSE;
			WL_WSEC(("wl%d: %s: have no key, discarding"
				" protected mgmt frame from %s, toss\n",
				WLCWLUNIT(wlc), __FUNCTION__,
				bcm_ether_ntoa(&hdr->sa, ea_str)));
			WLCNTINCR(wlc->pub->_cnt->rxundec);
		}
	} else if (scb->key) { /* not encrypted, have key */
		switch (fk) {
		case FC_DEAUTH:
		case FC_DISASSOC:
			WL_WSEC(("wl%d: %s: have key, discarding"
				" unprotected mgmt frame from %s\n",
				WLCWLUNIT(wlc), __FUNCTION__,
				bcm_ether_ntoa(&hdr->sa, ea_str)));
			WLCNTINCR(wlc->pub->_cnt->wepexcluded);
			if (SCB_MFP(scb) && bsscfg->associated) {
				WL_WSEC(("wl%d: %s: starting SA Query %s\n",
					WLCWLUNIT(wlc), __FUNCTION__,
					bcm_ether_ntoa(&hdr->sa, ea_str)));
				wlc_mfp_start_sa_query(mfp, bsscfg, scb);
			}
			ret = FALSE;
			break;
		case FC_ACTION:
			body_len = PKTLEN(wlc->osh, p);
			if (body_len <= 0) {
				WLCNTINCR(wlc->pub->_cnt->rxrunt);
				ret = FALSE;
				break;
			}
			body = (uchar*)PKTDATA(wlc->osh, p);
			if (mfp_robust_cat(body[0])) {
				WL_WSEC(("wl%d: %s: received robust"
					" action frame unprotected from " "%s, toss\n",
					WLCWLUNIT(wlc), __FUNCTION__,
					bcm_ether_ntoa(&hdr->sa, ea_str)));
				WLCNTINCR(wlc->pub->_cnt->rxbadproto);
				ret = FALSE;
				break;
			}
			break;
		default:
			ASSERT(0);
		}
	} /* else not encrypted, no key - okay */

	return ret;
}

/* validate and receive encrypted unicast mgmt frame */
static bool
mfp_rx_ucast(wlc_mfp_info_t *mfp, d11rxhdr_t *rxh,
	struct dot11_management_header *hdr, void *p, struct scb *scb)
{
	struct wlc_frminfo f;
	wlc_info_t *wlc = mfp->wlc;
	uint offset;

	ASSERT(scb != NULL);
	if (scb->key == NULL || scb->key->algo != CRYPTO_ALGO_AES_CCM)
		return FALSE;

	WLPKTTAG(p)->flags |= WLF_MFP; /* mark mgmt pkt as protected */

	/* decrypt packet */
	memset(&f, 0, sizeof(struct wlc_frminfo));
	f.pbody = (uint8*)PKTDATA(wlc->osh, p);	/* start from iv */
	f.body_len = PKTLEN(wlc->osh, p);
	f.fc = ltoh16(hdr->fc);
	f.ismulti = FALSE;
	f.rx_wep = TRUE;
	f.h = (struct dot11_header *)hdr;
	f.rxh = rxh;
	f.ividx = WLC_MFP_IVIDX;
	f.prio = 0;
	f.WPA_auth = scb->WPA_auth;
	f.da = &hdr->da;
	f.sa = &hdr->sa;

	/* wlc_wsec_recvdata expects f.p starts from 802.11 hdr */
	offset = (uint)((uint8*)f.pbody - (uint8*)hdr);
	PKTPUSH(wlc->osh, p, offset);
	f.p = p;
	f.len = PKTLEN(wlc->osh, p);
	f.totlen = pkttotlen(wlc->osh, p);

	/* receive and decrypt pkt */
	if (!wlc_wsec_recvdata(wlc, wlc->osh, scb, &f, f.prio)) {
		WL_ERROR(("wl%d: %s: decrypt mgmt packet failed\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		return FALSE;
	}

	/* just provide the base for offset to MIC */
	f.eh = (struct ether_header *)PKTDATA(wlc->osh, p);

	/* wlc_wsec_recvdata_decrypt checks for replays */
	wlc_wsec_rxiv_update(wlc, &f);

	/* remove 802.11 hdr and CCMP header */
	PKTPULL(wlc->osh, p, offset + scb->key->iv_len);

	return TRUE;
}

static bool
mfp_rx_mcast(wlc_mfp_igtk_info_t *igtk, d11rxhdr_t *rxh,
	const struct dot11_management_header *hdr, void *body, int body_len)
{
	mmic_ie_t *ie;
	uint32 ipn_lo;
	uint16 ipn_hi;
	uint8  mic[AES_BLOCK_SZ];

	if (igtk->key_len == 0) {
		WL_WSEC(("%s: no igtk plugged yet, ignore pkt\n", __FUNCTION__));
		return FALSE;
	}

	ie = (mmic_ie_t *)bcm_parse_tlvs(body, body_len, DOT11_MNG_MMIE_ID);
	if ((ie == NULL) || (ie->len != 16)) {
		WL_WSEC(("%s: mmie error: %s\n", __FUNCTION__,
			(ie == NULL ? "not found" : "wrong length")));
		return FALSE;
	}
	if (ie->key_id != igtk->id) {
		WL_WSEC(("%s: wrong mmie key id: %d\n", __FUNCTION__, ie->key_id));
		return FALSE;
	}

	ipn_lo = ltoh32_ua(ie->ipn);
	ipn_hi = ltoh16_ua(ie->ipn + 4);
	if ((igtk->ipn_hi > ipn_hi) ||
		((igtk->ipn_hi == ipn_hi) && (igtk->ipn_lo >= ipn_lo))) {
		WL_WSEC(("%s: replay detected: expected %04x%08x : received %04x%08x\n",
			__FUNCTION__, igtk->ipn_hi, igtk->ipn_lo,
			ipn_hi, ipn_lo));
		return FALSE;
	}

	/* calculcate and check MIC */
	mfp_bip_mic(hdr, body, body_len, igtk->key, mic);

#ifdef BCMDBG
	if (WL_WSEC_DUMP_ON()) {
		uint len = (uint)(body_len + ((uint8*)body - (uint8*)hdr));
		prhex("mfp_rx_mcast: packet", (uint8*)hdr, len);
		prhex("mfp_rx_mcast: computed mic", mic, BIP_MIC_SIZE);
	}
#endif // endif

	if (memcmp(mic, ie->mic, BIP_MIC_SIZE))
		return FALSE; /* bad mic */

	/* properly protected non-replay ==> update expected PN */
	igtk->ipn_hi = ipn_hi;
	igtk->ipn_lo = ++ipn_lo;
	if (!igtk->ipn_lo) {
		igtk->ipn_hi++;
	}

	return TRUE;
}

int
wlc_mfp_tx_mcast(const wlc_mfp_info_t *mfp, void *p, const wlc_bsscfg_t *bsscfg,
	uchar *body, uint body_len)
{
	struct dot11_management_header *hdr;
	mmic_ie_t *ie;
	uint8  mic[AES_BLOCK_SZ];
	mfp_bsscfg_t *mfp_bsscfg;
	wlc_mfp_igtk_info_t *igtk;
	uint16 mmic_ie_len = 0;

	ASSERT(bsscfg != NULL);

	mfp_bsscfg = MFP_BSSCFG(mfp, bsscfg);
	igtk = &mfp_bsscfg->igtk;
	if (igtk->key_len == 0)
		return BCME_ERROR;

	hdr = (struct dot11_management_header*)PKTDATA(mfp->wlc->osh, p);

	/* set fc field and mark as MFP - mcast frames do not have WEP bit set */
	hdr->fc = htol16(ltoh16(hdr->fc) & ~FC_WEP);
	WLPKTTAG(p)->flags |= WLF_MFP;

	/* Update the pktlen to include the mmic ie len */
	mmic_ie_len = OFFSETOF(mmic_ie_t, mic) + BIP_MIC_SIZE;
	PKTSETLEN(mfp->wlc->osh, p, PKTLEN(mfp->wlc->osh, p) + mmic_ie_len);

	ie = (mmic_ie_t *) (body + body_len);
	memset(ie, 0, mmic_ie_len);
	ie->id = DOT11_MNG_MMIE_ID;
	ie->len = 16;
	ie->key_id = igtk->id;

	/* Note: value expected by receiver is > the counter */
	igtk->ipn_lo++;
	if (igtk->ipn_lo == 0)
		igtk->ipn_hi++;

	*(uint32*)ie->ipn = igtk->ipn_lo;
	*(uint16*)(ie->ipn + 4) = igtk->ipn_hi;
	mfp_bip_mic(hdr, body, body_len + mmic_ie_len, igtk->key, mic);

#ifdef BCMDBG
	if (WL_WSEC_DUMP_ON()) {
		uint len = (uint)(body_len + ((uint8*)body - (uint8*)hdr));
		prhex("wlc_mfp_tx_mcast: packet", (uint8*)hdr, len);
		prhex("wlc_mfp_tx_mcast: computed mic", mic, BIP_MIC_SIZE);
	}
#endif // endif

	memcpy((char *)ie->mic, mic, BIP_MIC_SIZE);
	return BCME_OK;
}

int
wlc_mfp_tx_ucast(wlc_mfp_info_t *mfp, void *p, struct scb *scb)
{
	struct dot11_management_header *hdr;
	wsec_key_t *key;
	wlc_info_t *wlc = mfp->wlc;
	int status = BCME_OK;

	ASSERT(scb != NULL);
	key = scb->key;
	if (key == NULL)
		return BCME_OK;

	if (key->algo != CRYPTO_ALGO_AES_CCM)
		return BCME_UNSUPPORTED;

	hdr = (struct dot11_management_header*)PKTDATA(wlc->osh, p);

	/* set fc wep/protected frame */
	hdr->fc = htol16(ltoh16(hdr->fc) | FC_WEP);

	/* mark frame as MFP frame */
	WLPKTTAG(p)->flags |= WLF_MFP;

	/* update tx iv and fill tx iv field */
	wlc_key_iv_update(wlc, SCB_BSSCFG(scb), key, (uint8*)(hdr + 1), TRUE, FALSE);

	/* encrypt frame */
	if (!wlc_wsec_sw_encrypt_data(wlc, wlc->osh, p, SCB_BSSCFG(scb),
		scb, key)) {
		WL_ERROR(("wl%d: %s: encrypt mgmt pkt failed\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		status = BCME_ERROR;
	}

	return status;
}

static void
mfp_bip_mic(const struct dot11_management_header *hdr,
	const uint8 *body, int body_len, const uint8 *key, uint8 * mic)
{
	uint data_len;
	uchar micdata[256 + sizeof(mmic_ie_t)];
	uint16 fc;

	ASSERT(body_len >= BIP_MIC_SIZE);

	memset(mic, 0, BIP_MIC_SIZE);
	if ((sizeof(fc)+3*ETHER_ADDR_LEN+body_len) > sizeof(micdata)) {
		return; /* higher layers detect the error */
	}

	data_len = 0;

	/* calc mic */
	fc = htol16(ltoh16(hdr->fc) & ~(FC_RETRY | FC_PM | FC_MOREDATA));
	memcpy((char *)&micdata[data_len], (uint8 *)&fc, 2);
	data_len += 2;
	memcpy((char *)&micdata[data_len], (uint8 *)&hdr->da, ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	memcpy((char *)&micdata[data_len], (uint8 *)&hdr->sa, ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;
	memcpy((char *)&micdata[data_len], (uint8 *)&hdr->bssid, ETHER_ADDR_LEN);
	data_len += ETHER_ADDR_LEN;

	/* copy body without mic */
	memcpy(&micdata[data_len], body, body_len - BIP_MIC_SIZE);
	data_len += body_len - BIP_MIC_SIZE;
	memset(&micdata[data_len], 0, BIP_MIC_SIZE);
	data_len += BIP_MIC_SIZE;

	aes_cmac_calc(micdata, data_len, key, BIP_KEY_SIZE, mic);
}

void*
wlc_mfp_frame_get_mgmt(wlc_mfp_info_t *mfp, uint16 fc, uint8 cat,
	const struct ether_addr *da, const struct ether_addr *sa,
	const struct ether_addr *bssid, uint body_len, uint8 **pbody)
{
	void *p;
	uint iv_len = 0, tail_len = 0;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc = mfp->wlc;
	bool needs_mfp;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char ea_str[ETHER_ADDR_STR_LEN];
#endif // endif

	bsscfg = wlc_bsscfg_find_by_hwaddr_bssid(wlc, sa, bssid);
	if (bsscfg == NULL)
		bsscfg = wlc_bsscfg_find_by_target_bssid(wlc, bssid);

	needs_mfp = mfp_needs_mfp(mfp, bsscfg, da, fc, cat, &iv_len, &tail_len);
	if (!needs_mfp) {
#if defined(BCMCCX) && defined(CCX_SDK)
		return wlc_ccx_frame_get_mgmt(wlc, fc, cat, da, sa, bssid,
			body_len, body);
#endif // endif
		WL_INFORM(("wl%d: %s: no frame protection for %s\n",
			WLCWLUNIT(wlc), __FUNCTION__, bcm_ether_ntoa(bssid, ea_str)));
	}

	p = wlc_frame_get_mgmt_ex(wlc, fc, da, sa, bssid, body_len, pbody,
		iv_len, tail_len);

	if (needs_mfp && (p != NULL))
		WLPKTTAG(p)->flags |= WLF_MFP;

	return (p);
}

bool
wlc_mfp_check_key_error(const wlc_mfp_info_t *mfp, uint16 fc, wsec_key_t *key)
{
	uint16 fk = (fc & FC_KIND_MASK);
	bool ret = FALSE;

	if ((key->flags & WSEC_MFP_ACT_ERROR) && (fk == FC_ACTION)) {
		ret = TRUE;
		key->flags &= ~WSEC_MFP_ACT_ERROR;
	}
	if ((key->flags & WSEC_MFP_DISASSOC_ERROR) && (fk == FC_DISASSOC)) {
		ret = TRUE;
		key->flags &= ~WSEC_MFP_DISASSOC_ERROR;
	}
	if ((key->flags & WSEC_MFP_DEAUTH_ERROR) && (fk == FC_DEAUTH)) {
		ret = TRUE;
		key->flags &= ~WSEC_MFP_DEAUTH_ERROR;
	}

	return ret;
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

void
mfp_test_bip_mic(const struct dot11_management_header *hdr,
	const uint8 *body, int body_len, const uint8 *key, uint8 * mic)
{
	mfp_bip_mic(hdr, body, body_len, key, mic);
}

int
mfp_test_iov(const int mfp_iov)
{
	int iov;
	switch (IOV_ID(mfp_iov)) {
	case IOV_MFP_SHA256:   iov = IOV_MFP_TEST_SHA256; break;
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
#endif /* MFP */
