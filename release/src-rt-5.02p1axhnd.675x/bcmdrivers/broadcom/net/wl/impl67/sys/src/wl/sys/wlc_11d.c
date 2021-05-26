/**
 * 802.11d (support for additional regulatory domains) module source file
 * Broadcom 802.11abgn Networking Device Driver
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
 * $Id: wlc_11d.c 789366 2020-07-27 05:26:09Z $
 */

/**
 * @file
 * @brief
 * A device may implement "multi-domain" operation, meaning it can operate in regulatory compliance
 * (using conforming channels, tx power) w/several domains. Thus it must find the domain and any
 * relevant information in order to operate. The Country IE allows a STA to get this info
 * automatically from an AP; in this case the STA should only do passive scanning until it obtains
 * the information from an AP. If the capability is disabled, (default) the STA use built-in
 * defaults, modified by client commands; but still should not transmit unless it has a valid
 * country code. The 802.11d specification also defines IEs for Frequency Hopping info
 * (Parameters and Pattern), and a Request IE which a STA may use (in a Probe Request, for example)
 * to ask for specific IEs to be included in the response from the AP.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlDriver11DH]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifdef WL11D

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_scan_utils.h>
#include <wlc_cntry.h>
#include <wlc_11d.h>
#include <phy_radio_api.h>
#include <phy_utils_api.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_bsscfg.h>

/* IOVar table */
/* No ordering is imposed */
enum wlc_11d_iov {
	IOV_AUTOCOUNTRY_DEFAULT = 1,
	IOV_AUTOCOUNTRY = 2,
	IOV_CNTRY_DEFAULT = 3,
	IOV_CCODE_PR_2G = 4,
	IOV_LAST
};

static const bcm_iovar_t wlc_11d_iovars[] = {
	{"autocountry_default", IOV_AUTOCOUNTRY_DEFAULT, (0), 0, IOVT_BUFFER, 0},
#ifdef STA
	{"autocountry", IOV_AUTOCOUNTRY, (0), 0, IOVT_BOOL, 0},
#ifdef CNTRY_DEFAULT
	{"country_default", IOV_CNTRY_DEFAULT, (0), 0, IOVT_BOOL, 0},
#endif /* CNTRY_DEFAULT */
#ifdef LOCALE_PRIORITIZATION_2G
	{"ccode_pr_2g", IOV_CCODE_PR_2G, (0), 0, IOVT_BOOL, 0},
#endif /* LOCALE_PRIORITIZATION_2G */
#endif /* STA */
	{NULL, 0, 0, 0, 0, 0}
};

/* ioctl table */
static const wlc_ioctl_cmd_t wlc_11d_ioctls[] = {
	{WLC_SET_REGULATORY, 0, sizeof(int)},
	{WLC_GET_REGULATORY, 0, sizeof(int)}
};

/* Country module info */
struct wlc_11d_info {
	wlc_info_t *wlc;
	char autocountry_default[WLC_CNTRY_BUF_SZ];	/* initial country for 802.11d
							 * auto-country mode
							 */
	bool autocountry_adopted_from_ap;	/* whether the current locale is adopted from
						 * country IE of a associated AP
						 */
	bool awaiting_cntry_info;		/* still waiting for country ie in 802.11d mode */
#ifdef CNTRY_DEFAULT
	bool _cntry_default;			/* cntry_default posted by host */
#endif /* CNTRY_DEFAULT */
	bool ccode_pr_2g;
	char autocountry_adopted[WLC_CNTRY_BUF_SZ];
	char autocountry_scan_learned[WLC_CNTRY_BUF_SZ];
	/* For autocountry: track best country so far */
	uint8 autocountry_mode_flags;
	chanvec_t supported_channels;
	char best_abbrev[WLC_CNTRY_BUF_SZ];
	int best_chans;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	struct ether_addr best_bssid;
	chanspec_t best_chanspec;
#endif /* defined(BCMDBG) || defined(WLMSG_INFORM) */
};

#define AUTOCOUNTRY_MODE_FLAGS_DEFER_EVAL	1	/* Evaluate only at end of scan */
#define AUTOCOUNTRY_MODE_FLAGS_SUCCESS_ONLY	2	/* Apply only on WLC_E_STATUS_SUCCESS */

/* local functions */
/* module */
static int wlc_11d_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
#if defined(BCMDBG)
static int wlc_11d_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif
static int wlc_11d_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* module */
wlc_11d_info_t *
BCMATTACHFN(wlc_11d_attach)(wlc_info_t *wlc)
{
	wlc_11d_info_t *m11d;
	wlcband_t *band = wlc->band;
	chanvec_t channels;
	uint j;
	enum wlc_bandunit bandunit;

	if ((m11d = MALLOCZ(wlc->osh, sizeof(wlc_11d_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	m11d->wlc = wlc;

#if defined(BCMDBG)
	if (wlc_dump_register(wlc->pub, "11d", wlc_11d_dump, m11d) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif

	if (wlc_module_register(wlc->pub, wlc_11d_iovars, "11d", m11d, wlc_11d_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	if (wlc_module_add_ioctl_fn(wlc->pub, m11d, wlc_11d_doioctl,
	                            ARRAYSIZE(wlc_11d_ioctls), wlc_11d_ioctls) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Initialize supported channels */
	bzero(&m11d->supported_channels, sizeof(chanvec_t));

	FOREACH_WLC_BAND(wlc, bandunit) {
		band = wlc->bandstate[bandunit];
		phy_radio_get_valid_chanvec(WLC_PI_BANDUNIT(wlc, bandunit),
			band->bandtype, &channels);
		for (j = 0; j < sizeof(chanvec_t); j++)
			m11d->supported_channels.vec[j] |= channels.vec[j];
		if (!IS_MBAND_UNLOCKED(wlc))
			break;
	}

	return m11d;

	/* error handling */
fail:
	MODULE_DETACH(m11d, wlc_11d_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_11d_detach)(wlc_11d_info_t *m11d)
{
	wlc_info_t *wlc;

	if (m11d == NULL)
		return;

	wlc = m11d->wlc;

	(void)wlc_module_remove_ioctl_fn(wlc->pub, m11d);
	wlc_module_unregister(wlc->pub, "11d", m11d);

	MFREE(wlc->osh, m11d, sizeof(wlc_11d_info_t));
}

static int
wlc_11d_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_11d_info_t *m11d = (wlc_11d_info_t *)ctx;
	wlc_info_t *wlc = m11d->wlc;
	int err = BCME_OK;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	BCM_REFERENCE(val_size);
	BCM_REFERENCE(wlcif);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);
	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_AUTOCOUNTRY_DEFAULT):
		if ((uint)len <= strlen(m11d->autocountry_default) + 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		strncpy(arg, m11d->autocountry_default, len - 1);
		break;

	case IOV_SVAL(IOV_AUTOCOUNTRY_DEFAULT): {
		clm_country_t unused;
		char country_abbrev[WLC_CNTRY_BUF_SZ];
		int slen;

		/* find strlen, with string either null terminated or 'len' terminated */
		for (slen = 0; slen < (int)len && ((char*)arg)[slen] != '\0'; slen++)
			;
		if (slen >= WLC_CNTRY_BUF_SZ) {
			err = BCME_BUFTOOLONG;
			break;
		}
		/* copy country code from arg avoiding overruns and null terminating */
		bzero(country_abbrev, WLC_CNTRY_BUF_SZ);
		strncpy(country_abbrev, (char*)arg, slen);

		WL_REGULATORY(("wl%d:%s(): set IOV_AUTOCOUNTRY_DEFAULT %s\n",
			wlc->pub->unit, __FUNCTION__, country_abbrev));

		if (slen == 0 || wlc_country_lookup(wlc, country_abbrev, 0, &unused) !=
			CLM_RESULT_OK) {
			err = BCME_BADARG;
			break;
		}
		strncpy(m11d->autocountry_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
		break;
	}

#ifdef STA
	case IOV_GVAL(IOV_AUTOCOUNTRY):
		*ret_int_ptr = (int)wlc->pub->cmn->_autocountry;
		break;

	case IOV_SVAL(IOV_AUTOCOUNTRY): {
		bool autocountry;
		bool awaiting_cntry_info;

		if (SCAN_IN_PROGRESS(wlc->scan))
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);

		autocountry = wlc->pub->cmn->_autocountry;
		awaiting_cntry_info = m11d->awaiting_cntry_info;

		wlc->pub->cmn->_autocountry = bool_val;

		m11d->awaiting_cntry_info = bool_val;

		if (bool_val) {
			WL_INFORM(("wl%d:%s(): IOV_AUTOCOUNTRY is TRUE, re-init to "
				"autocountry_defualt %s\n", wlc->pub->unit, __FUNCTION__,
				m11d->autocountry_default));

			/* Re-init channels and locale to Auto Country default */
			if (!WLC_CNTRY_DEFAULT_ENAB(wlc))
				err = wlc_set_countrycode(wlc->cmi, m11d->autocountry_default, 0);
		} else {
			WL_INFORM(("wl%d:%s(): IOV_AUTOCOUNTRY is FALSE,\n",
			           wlc->pub->unit, __FUNCTION__));
			err = wlc_cntry_use_default(wlc->cntry);
		}

		if (err) {
			WL_ERROR(("wl%d:%s(): IOV_AUTOCOUNTRY  %d failed\n",
				wlc->pub->unit, __FUNCTION__, bool_val));
			wlc->pub->cmn->_autocountry = autocountry;
			m11d->awaiting_cntry_info = awaiting_cntry_info;
			break;
		}

		m11d->autocountry_adopted_from_ap = FALSE;

		break;
	}
#ifdef CNTRY_DEFAULT
	case IOV_GVAL(IOV_CNTRY_DEFAULT):
		*ret_int_ptr = (int)m11d->_cntry_default;
		break;
	case IOV_SVAL(IOV_CNTRY_DEFAULT):
		m11d->_cntry_default = bool_val;
		break;
#endif /* CNTRY_DEFAULT */
#ifdef LOCALE_PRIORITIZATION_2G
	case IOV_GVAL(IOV_CCODE_PR_2G):
		*ret_int_ptr = (int)m11d->ccode_pr_2g;
		break;
	case IOV_SVAL(IOV_CCODE_PR_2G):
		m11d->ccode_pr_2g = bool_val;
		break;
#endif /* LOCALE_PRIORITIZATION_2G */
#endif /* STA */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_11d_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_11d_info_t *m11d = (wlc_11d_info_t *)ctx;
	wlc_info_t *wlc = m11d->wlc;
	int val = 0, *pval;
	int err = BCME_OK;
	bool bool_val;
	BCM_REFERENCE(wlcif);
	/* default argument is generic integer */
	pval = (int *)arg;

	/* This will prevent the misaligned access */
	if (pval && (uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));

	bool_val = (val != 0) ? TRUE : FALSE;

	switch (cmd) {
	case WLC_SET_REGULATORY:
		wlc->pub->cmn->_11d = bool_val;
		break;

	case WLC_GET_REGULATORY:
		*pval = (int)wlc->pub->cmn->_11d;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#if defined(BCMDBG)
static int
wlc_11d_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_11d_info_t *m11d = (wlc_11d_info_t *)ctx;
	wlc_info_t *wlc = m11d->wlc;

	bcm_bprintf(b, "reg_domain:%d\n", WL11D_ENAB(wlc));
	bcm_bprintf(b, "autocountry:%d autocountry_def:%s adopted_from_ap:%d awaiting:%d\n",
	            WLC_AUTOCOUNTRY_ENAB(wlc), m11d->autocountry_default,
	            m11d->autocountry_adopted_from_ap, m11d->awaiting_cntry_info);

	return BCME_OK;
}
#endif // endif

#ifdef STA
/* Determine if the country channel information is compatible with the current association.
 * Returns TRUE if the country setting includes the channel of the currently associated AP
 * or IBSS, FALSE otherwise.
 */
int
wlc_11d_compatible_country(wlc_11d_info_t *m11d, const char *country_abbrev)
{
	wlc_info_t *wlc = m11d->wlc;
	chanvec_t channels;
	chanspec_t chanspec;

	/* should only be called if associated to an AP */
	ASSERT(wlc->pub->associated);
	if (!wlc->pub->associated)
		return TRUE;

	chanspec = wlc->home_chanspec;
	ASSERT(!wf_chspec_malformed(chanspec));

	if (wlc_channel_get_chanvec(wlc, country_abbrev,
		CHSPEC_BANDTYPE(chanspec), &channels) == FALSE)
		return FALSE;

	if (CHSPEC_IS8080(chanspec)) {
		uint ch;
		int i;
		bool compat = TRUE;

		ch = wf_chspec_primary80_channel(chanspec);

		/* check each 20MHz sub-channel in the first 80MHz channel */
		ch = ch - CH_40MHZ_APART + CH_10MHZ_APART;

		for (i = 0; i < 4; i++, ch += CH_20MHZ_APART) {
			if (!isset(channels.vec, ch)) {
				compat = FALSE;
				break;
			}
		}

		ch = wf_chspec_secondary80_channel(chanspec);
		/* well-formed 80p80 channel should have valid secondary channel */
		ASSERT(ch > 0);

		/* check each 20MHz sub-channel in the 80MHz channel */
		ch = ch - CH_40MHZ_APART + CH_10MHZ_APART;

		for (i = 0; i < 4; i++, ch += CH_20MHZ_APART) {
			if (!isset(channels.vec, ch)) {
				compat = FALSE;
				break;
			}
		}

		return compat;
	}
	else if (CHSPEC_IS160(chanspec)) {
		uint ch;
		int i;
		bool compat = TRUE;

		/* check each 20MHz sub-channel in the 160MHz channel */
		ch = CHSPEC_CHANNEL(chanspec) - CH_80MHZ_APART + CH_10MHZ_APART;

		for (i = 0; i < 8; i++, ch += CH_20MHZ_APART) {
			if (!isset(channels.vec, ch)) {
				compat = FALSE;
				break;
			}
		}
		return compat;
	}
	else if (CHSPEC_IS80(chanspec)) {
		uint ch;
		int i;
		bool compat = TRUE;

		/* check each 20MHz sub-channel in the 80MHz channel */
		ch = CHSPEC_CHANNEL(chanspec) - CH_40MHZ_APART + CH_10MHZ_APART;

		for (i = 0; i < 4; i++, ch += CH_20MHZ_APART) {
			if (!isset(channels.vec, ch)) {
				compat = FALSE;
				break;
			}
		}
		return compat;
	} else if (CHSPEC_IS40(chanspec))
		return isset(channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chanspec))) &&
		        isset(channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chanspec)));
	else
		return isset(channels.vec, CHSPEC_CHANNEL(chanspec));
}

void
wlc_11d_scan_start(wlc_11d_info_t *m11d)
{
	bzero(m11d->best_abbrev, sizeof(m11d->best_abbrev));
	m11d->best_chans = 0;
}

static void
wlc_11d_bss_eval(wlc_11d_info_t *m11d, wlc_bss_info_t *bi,
                         uint8 *bcn_prb, uint bcn_prb_len)
{
	wlc_info_t *wlc = m11d->wlc;
	wlcband_t *band;
	bcm_tlv_t *country_ie;
	uint8 *tags;
	uint tag_len;
	uint j;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	chanvec_t channels, unused;
	int8 ie_tx_pwr[MAXCHANNEL];
	clm_country_t country_new;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale_new;
	int x_band;
	int err;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif	/* BCMDBG || WLMSG_INFORM */

	ASSERT(bi != NULL);
	x_band = 0;

	/* skip IBSS bcn/prb, or scan results with no bcn/prb */
	if (bi->bss_type != DOT11_BSSTYPE_INFRASTRUCTURE ||
	    !bi->bcn_prb || (bi->bcn_prb_len <= DOT11_BCN_PRB_LEN))
		return;

	tag_len = bcn_prb_len - sizeof(struct dot11_bcn_prb);
	tags = bcn_prb + sizeof(struct dot11_bcn_prb);

	/* skip if no Country IE */
	country_ie = bcm_find_tlv(tags, tag_len, DOT11_MNG_COUNTRY_ID);
	if (!country_ie) {
		return;
	}

	/* The country_abbev, channels, ie_tx_pwr are all output (no need to init) */
	err = wlc_cntry_parse_country_ie(wlc->cntry, country_ie, country_abbrev,
	                                 &channels, ie_tx_pwr);
	if (err) {
		WL_REGULATORY(("wl%d: %s: skipping malformed Country IE on "
		               "AP %s \"%s\" channel %d\n",
		               wlc->pub->unit, __FUNCTION__,
		               bcm_ether_ntoa(&bi->BSSID, eabuf),
		               (wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len), ssidbuf),
		               wf_chspec_ctlchan(bi->chanspec)));
		return;
	}

	/* If we're already saving this country, we're done */
	if (!strncmp(m11d->best_abbrev, country_abbrev, WLC_CNTRY_BUF_SZ))
		return;
	/* skip if the Country IE does not have a known country code */
	result = wlc_country_lookup_ext(wlc, country_abbrev, &country_new);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d: %s: skipping unknown country code \"%s\" from "
		               "AP %s \"%s\" channel %d\n",
		               wlc->pub->unit, __FUNCTION__,
		               country_abbrev, bcm_ether_ntoa(&bi->BSSID, eabuf),
		               (wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len), ssidbuf),
		               wf_chspec_ctlchan(bi->chanspec)));
		return;
	}

	band = wlc->band;

	/* count the channels */
	result = wlc_get_locale(country_new, &locale_new);

	/* Don't bother with separating bands */
	if (result == CLM_RESULT_OK) {
		enum wlc_bandunit bandunit;
		FOREACH_WLC_BAND(wlc, bandunit) {
			band = wlc->bandstate[bandunit];
			wlc_locale_get_channels(&locale_new, BANDTYPE2CLMBAND(band->bandtype),
			                        &channels, &unused);

			for (j = 0; j < sizeof(chanvec_t); j++)
				channels.vec[j] &= m11d->supported_channels.vec[j];
			x_band += bcm_bitcount(channels.vec, sizeof(chanvec_t));
			if (!IS_MBAND_UNLOCKED(wlc))
				break;
		}

		WL_REGULATORY(("wl%d: %s: AP %s \"%s\" with Country IE: "
		               "\"%s\" %d total channels\n",
		               wlc->pub->unit, __FUNCTION__,
		               bcm_ether_ntoa(&bi->BSSID, eabuf),
		               (wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len), ssidbuf),
		               country_abbrev, x_band));
	}

	/* Pick the best by most channels */
	if (x_band > m11d->best_chans) {
		if (m11d->best_abbrev[0])
			WL_REGULATORY(("wl%d: %s: Country IE \"%s\" with "
			               "%d channels preferred over "
			               "\"%s\" with %d channels.\n",
			               wlc->pub->unit, __FUNCTION__,
			               country_abbrev, x_band,
			               m11d->best_abbrev, m11d->best_chans));
		m11d->best_chans = x_band;
		strncpy(m11d->best_abbrev, country_abbrev,
		        sizeof(m11d->best_abbrev) - 1);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
		m11d->best_bssid = bi->BSSID;
#endif // endif
	}
}

void
wlc_11d_scan_result(wlc_11d_info_t *m11d, wlc_bss_info_t *bi,
                    uint8 *bcn_prb, uint bcn_prb_len)
{
	wlc_info_t *wlc = m11d->wlc;

	BCM_REFERENCE(wlc);

	/* This routine should only be called if we are still looking for country information. */
	if (!m11d->awaiting_cntry_info)
		return;

	/* If we want to defer evaluation to the end, nothing to do */
	if (m11d->autocountry_mode_flags & AUTOCOUNTRY_MODE_FLAGS_DEFER_EVAL)
		return;

	/* If country_default enabled, country code will not be updated at the end of a scan */
	if (WLC_CNTRY_DEFAULT_ENAB(wlc))
		return;

	/* Ok, evaluate this bss as part of the on-the-fly accumulation */
	wlc_11d_bss_eval(m11d, bi, bcn_prb, bcn_prb_len);
}

void
wlc_11d_scan_complete(wlc_11d_info_t *m11d, int status)
{
	wlc_info_t *wlc = m11d->wlc;
	wlc_bss_info_t *bi;
	uint i;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif	/* BCMDBG || WLMSG_INFORM */

	/* This routine should only be called if we are still looking for country information. */
	if (!m11d->awaiting_cntry_info)
		return;

	/* Filter out failed/aborted scans if configured that way */
	if ((status != WLC_E_STATUS_SUCCESS) &&
	    (m11d->autocountry_mode_flags & AUTOCOUNTRY_MODE_FLAGS_SUCCESS_ONLY)) {
		return;
	}

	/* If country_default enabled, country code will not be updated at the end of a scan */
	if (WLC_CNTRY_DEFAULT_ENAB(wlc))
		return;

	/* If we deferred evaluation to the end, evaluate the list of APs now */
	if (m11d->autocountry_mode_flags & AUTOCOUNTRY_MODE_FLAGS_DEFER_EVAL) {
		for (i = 0; i < wlc->scan_results->count; i++) {
			bi = wlc->scan_results->ptrs[i];
			wlc_11d_bss_eval(m11d, bi, (uint8 *)bi->bcn_prb, bi->bcn_prb_len);
		}
	}

	/* Bail if we haven't recorded anything */
	if (m11d->best_abbrev[0] == 0)
		return;

	/* keep only the 2 char ISO code for our country setting,
	 * dropping the Indoor/Outdoor/Either specification in the 3rd char
	 */
	m11d->best_abbrev[2] = '\0';

	if (wlc->pub->associated &&
	    !wlc_11d_compatible_country(m11d, m11d->best_abbrev)) {
		WL_REGULATORY(("wl%d: %s: Not adopting best choice Country \"%s\" from "
		               "AP %s channel %d since it is incompatible with our "
		               "current association on channel %d\n",
		               wlc->pub->unit, __FUNCTION__, m11d->best_abbrev,
		               bcm_ether_ntoa(&m11d->best_bssid, eabuf),
		               wf_chspec_ctlchan(m11d->best_chanspec),
		               wf_chspec_ctlchan(wlc->home_chanspec)));
		return;
	}

	/* Adopt the best choice */
	WL_INFORM(("wl%d: %s: Adopting Country IE \"%s\" from AP %s channel %d\n",
	           wlc->pub->unit, __FUNCTION__, m11d->best_abbrev,
	           bcm_ether_ntoa(&m11d->best_bssid, eabuf),
	           wf_chspec_ctlchan(m11d->best_chanspec)));

	wlc_set_countrycode(wlc->cmi, m11d->best_abbrev, 0);
	m11d->awaiting_cntry_info = FALSE;

	/* The current channel could be chatty in the new country code */
	if (wlc->pub->up && !wlc_quiet_chanspec(wlc->cmi, phy_utils_get_chanspec(WLC_PI(wlc))))
		wlc_mute(wlc, OFF, 0);
}

void
wlc_11d_adopt_country(wlc_11d_info_t *m11d, char *country_str, bool adopt_country)
{
	wlc_info_t *wlc = m11d->wlc;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	clm_country_t country_new;
	clm_result_t result;
	uint regrev = 0;

	if (!WLC_AUTOCOUNTRY_ENAB(wlc))
		return;

	if (!(m11d->awaiting_cntry_info ||
	      adopt_country ||
	      !m11d->autocountry_adopted_from_ap))
		return;

	WL_REGULATORY(("wl%d: %s: Adopting Country from associated AP\n",
	               wlc->pub->unit, __FUNCTION__));

	/* create the 2 char country code from the ISO country code */
	strncpy(country_abbrev, country_str, sizeof(country_abbrev) - 1);
	country_abbrev[2] = '\0';

	result = wlc_country_lookup_ext(wlc, country_str, &country_new);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d: Ignoring associated AP's Country \"%s\" since "
		               "no match was found in built-in table\n",
		               wlc->pub->unit, country_str));
#ifdef DISABLE_INVALIDCC_ADOPT
		/* This country code is not supported - dropping it */
		return;
#endif // endif
	}
	if (!wlc_11d_compatible_country(wlc->m11d, country_abbrev)) {
		WL_REGULATORY(("wl%d: Ignoring associated AP's Country \"%s\" "
		               "since it is incompatible with the associated channel %d\n",
		               wlc->pub->unit, country_str,
		               wf_chspec_ctlchan(wlc->home_chanspec)));
		/* if country_default feature is enabled and if the channel is incompatible
		* with the cntry_default CCODE, assign
		* autocountry_default CCODE as the new CCODE
		*/
		if (WLC_CNTRY_DEFAULT_ENAB(wlc)) {
			strncpy(country_abbrev,
				m11d->autocountry_default, sizeof(country_abbrev) - 1);
			country_abbrev[2] = '\0';
		}
	}
	WL_INFORM(("wl%d: setting regulatory information from built-in "
		"country \"%s\" matching associated AP's Country\n",
		wlc->pub->unit, country_abbrev));

	if (!strcmp(wlc_channel_country_abbrev(wlc->cmi), country_abbrev) ||
		!strcmp(wlc_channel_ccode(wlc->cmi), country_abbrev)) {
		/* If the desired ccode matches the current advertised ccode
		 * or the current ccode, set regrev for the current regrev
		 * and set ccode for the current ccode
		 */
		strncpy(country_abbrev, wlc_channel_ccode(wlc->cmi), WLC_CNTRY_BUF_SZ-1);
		country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
		regrev = wlc_channel_regrev(wlc->cmi);
	}

	wlc_set_countrycode(wlc->cmi, country_abbrev, regrev);
	m11d->awaiting_cntry_info = FALSE;
	/* The current channel could be chatty in the new country code */
	if (!wlc_quiet_chanspec(wlc->cmi, phy_utils_get_chanspec(WLC_PI(wlc))))
		wlc_mute(wlc, OFF, 0);

	if (adopt_country) {
		m11d->autocountry_adopted_from_ap = TRUE;
		wlc_11d_set_autocountry_adopted(wlc->m11d, country_abbrev);
	}
}
#endif /* STA */

void
wlc_11d_reset_all(wlc_11d_info_t *m11d)
{
	m11d->awaiting_cntry_info = FALSE;
	bzero(m11d->autocountry_scan_learned, WLC_CNTRY_BUF_SZ - 1);
}

/* accessors */
#ifdef CNTRY_DEFAULT
bool
wlc_11d_cntry_default_enabled(wlc_11d_info_t *m11d)
{
	return m11d->_cntry_default;
}
#endif /* CNTRY_DEFAULT */

#ifdef LOCALE_PRIORITIZATION_2G
bool
wlc_11d_locale_prioritization_2g_enabled(wlc_11d_info_t *m11d)
{
	return m11d->ccode_pr_2g;
}
#endif /* LOCALE_PRIORITIZATION_2G */

bool
wlc_11d_autocountry_adopted(wlc_11d_info_t *m11d)
{
	return m11d->autocountry_adopted_from_ap;
}

void
wlc_11d_reset_autocountry_adopted(wlc_11d_info_t *m11d)
{
	m11d->autocountry_adopted_from_ap = FALSE;
	bzero(m11d->autocountry_adopted, WLC_CNTRY_BUF_SZ - 1);
	wlc_country_clear_locales(m11d->wlc->cntry);
}

void
wlc_11d_set_autocountry_adopted(wlc_11d_info_t *m11d, const char *country_abbrev)
{
	strncpy(m11d->autocountry_adopted, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
}

const char *
wlc_11d_get_autocountry_adopted(wlc_11d_info_t *m11d)
{
	return m11d->autocountry_adopted;
}

bool
wlc_11d_autocountry_scan_learned(wlc_11d_info_t *m11d)
{
	if (!m11d->awaiting_cntry_info && m11d->autocountry_scan_learned[0] != '\0')
		return TRUE;
	return FALSE;

}

const char *
wlc_11d_get_autocountry_scan_learned(wlc_11d_info_t *m11d)
{
	return m11d->autocountry_scan_learned;
}

void
wlc_11d_set_autocountry_scan_learned(wlc_11d_info_t *m11d, const char *country_abbrev)
{
	strncpy(m11d->autocountry_scan_learned, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
}

void
wlc_11d_set_autocountry_default(wlc_11d_info_t *m11d, const char *country_abbrev)
{
	strncpy(m11d->autocountry_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
}

const char *
wlc_11d_get_autocountry_default(wlc_11d_info_t *m11d)
{
	return m11d->autocountry_default;
}
#endif /* WL11D */
