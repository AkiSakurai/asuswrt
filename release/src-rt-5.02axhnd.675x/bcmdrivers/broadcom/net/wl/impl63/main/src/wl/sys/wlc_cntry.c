/**
 * @file
 * @brief
 * 802.11h/11d Country and wl Country module source file
 * Broadcom 802.11abgn Networking Device Driver
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
 * $Id: wlc_cntry.c 781295 2019-11-18 02:15:09Z $
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

#ifdef WLCNTRY

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
#include <wlc.h>
#ifdef WLP2P
#include <bcmwpa.h>
#endif // endif
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>
#include <wlc_bsscfg.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_reg.h>
#include <wlc_dbg.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_channel.h>
#if defined(WL_GLOBAL_RCLASS)
#define BCMWIFI_GLOBAL_REGULATORY_CLASS		4
#endif /* WL_GLOBAL_RCLASS */
/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_COUNTRY,
	IOV_COUNTRY_ABBREV_OVERRIDE,
	IOV_COUNTRY_IE_OVERRIDE,
	IOV_COUNTRY_REV,
	IOV_CCODE_INFO,
	IOV_LAST
};

static const bcm_iovar_t wlc_cntry_iovars[] = {
	{"country", IOV_COUNTRY, (IOVF_SET_DOWN), 0, IOVT_BUFFER, WLC_CNTRY_BUF_SZ},
	{"country_abbrev_override", IOV_COUNTRY_ABBREV_OVERRIDE, (0), 0, IOVT_BUFFER, 0},
#ifdef BCMDBG
	{"country_ie_override", IOV_COUNTRY_IE_OVERRIDE, (0), 0, IOVT_BUFFER, 5},
#endif /* BCMDBG */
	{"country_rev", IOV_COUNTRY_REV, (0), 0, IOVT_BUFFER, (sizeof(uint32)*(WL_NUMCHANSPECS+1))},
	{"ccode_info", IOV_CCODE_INFO, (0), 0, IOVT_BUFFER, WL_CCODE_INFO_FIXED_LEN},
	{NULL, 0, 0, 0, 0, 0}
};

/* Country module info */
struct wlc_cntry_info {
	wlc_info_t *wlc;
	/* country management */
	char country_default[WLC_CNTRY_BUF_SZ];	/* saved country for leaving 802.11d
						 * auto-country mode
						 */
	char country_abbrev_override[WLC_CNTRY_BUF_SZ];	/* country abbrev override */
	clm_country_locales_t *locales;		/* Locales to be used for cntry IE */
	bcm_tlv_t *cached_cntry_ie;		/* points to allocated memory containing country ie
						 * when country ie has been created previously and
						 * is still valid
						 */
	bool ccode_lockdown;
#ifdef BCMDBG
	bcm_tlv_t *country_ie_override;		/* debug override of announced Country IE */
#endif // endif
};

/* local functions */
/* module */
#ifndef OPENSRC_IOV_IOCTL
int wlc_cntry_external_to_internal(char *buf, int buflen);
#endif /* OPENSRC_IOV_IOCTL */
static int wlc_cntry_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
#ifdef BCMDBG
static int wlc_cntry_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/* IE mgmt */
#ifdef AP
static uint wlc_cntry_calc_cntry_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_cntry_write_cntry_ie(void *ctx, wlc_iem_build_data_t *data);
#endif // endif
#ifdef WLTDLS
static uint wlc_cntry_tdls_calc_cntry_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_cntry_tdls_write_cntry_ie(void *ctx, wlc_iem_build_data_t *data);
#endif // endif

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* module */
wlc_cntry_info_t *
BCMATTACHFN(wlc_cntry_attach)(wlc_info_t *wlc)
{
	wlc_cntry_info_t *cm;
#ifdef AP
	uint16 bcnfstbmp = FT2BMP(FC_BEACON) | FT2BMP(FC_PROBE_RESP);
#endif // endif

	if ((cm = MALLOCZ(wlc->osh, sizeof(wlc_cntry_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	cm->wlc = wlc;

	/* register IE mgmt callback */
#ifdef AP
	/* bcn/prbrsp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, bcnfstbmp, DOT11_MNG_COUNTRY_ID,
	      wlc_cntry_calc_cntry_ie_len, wlc_cntry_write_cntry_ie, cm) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_build_fn failed, cntry in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif
#ifdef WLTDLS
	/* setupreq */
	if (TDLS_ENAB(wlc->pub)) {
		if (wlc_ier_add_build_fn(wlc->ier_tdls_srq, DOT11_MNG_COUNTRY_ID,
			wlc_cntry_tdls_calc_cntry_ie_len, wlc_cntry_tdls_write_cntry_ie, cm)
			!= BCME_OK) {
			WL_ERROR(("wl%d: %s wlc_ier_add_build_fn failed, cntry in setupreq\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		/* setupresp */
		if (wlc_ier_add_build_fn(wlc->ier_tdls_srs, DOT11_MNG_COUNTRY_ID,
			wlc_cntry_tdls_calc_cntry_ie_len, wlc_cntry_tdls_write_cntry_ie, cm)
			!= BCME_OK) {
			WL_ERROR(("wl%d: %s wlc_ier_add_build_fn failed, cntry in setupresp\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* WLTDLS */

#ifdef BCMDBG
	if (wlc_dump_register(wlc->pub, "cntry", wlc_cntry_dump, cm) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif

	/* keep the module registration the last other add module unregistratin
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, wlc_cntry_iovars, "cntry", (void *)cm, wlc_cntry_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	cm->ccode_lockdown = wlc_is_ccode_lockdown(wlc);

	return cm;

	/* error handling */
fail:
	if (cm != NULL)
		MFREE(wlc->osh, cm, sizeof(wlc_cntry_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_cntry_detach)(wlc_cntry_info_t *cm)
{
	wlc_info_t *wlc = cm->wlc;

	wlc_module_unregister(wlc->pub, "cntry", cm);

	if (cm->cached_cntry_ie) {
		MFREE(wlc->osh, cm->cached_cntry_ie, cm->cached_cntry_ie->len + TLV_HDR_LEN);
		cm->cached_cntry_ie = NULL;
	}
	wlc_country_clear_locales(cm);
#ifdef BCMDBG
	if (cm->country_ie_override != NULL) {
		MFREE(wlc->osh, cm->country_ie_override,
		      cm->country_ie_override->len + TLV_HDR_LEN);
		cm->country_ie_override = NULL;
	}
#endif	/* BCMDBG */

	MFREE(wlc->osh, cm, sizeof(wlc_cntry_info_t));
}

/*
 * Converts external country code representation to internal format, for debug builds or
 * disallows for production builds.
 * eg,
 *	ALL -> #a
 *	RDR -> #r
 */
#ifndef OPENSRC_IOV_IOCTL
int
wlc_cntry_external_to_internal(char *buf, int buflen)
{
	int err = BCME_OK;

	/* Translate ALL or RDR to internal 2 char country codes. */
	if (!strncmp(buf, "ALL", sizeof("ALL") - 1)) {
		strncpy(buf, "#a", buflen);
	} else if (!strncmp(buf, "RDR", sizeof("RDR") - 1)) {
		strncpy(buf, "#r", buflen);
	}
#if !defined(BCMDBG) && (!defined(WLTEST) || defined(WLTEST_DISABLED))
	/* Don't allow RDR in production. */
	if (!strncmp(buf, "#r", sizeof("#r") - 1)) {
		err = BCME_BADARG;
	}
#endif /* !defined(BCMDBG) && !defined(WLTEST) */
	return err;
}
#endif /* OPENSRC_IOV_IOCTL */

static int
wlc_cntry_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;
	wlc_info_t *wlc = cm->wlc;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;

	BCM_REFERENCE(wlcif);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	bool_val = (int_val != 0) ? TRUE : FALSE;
	bool_val2 = (int_val2 != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);
	BCM_REFERENCE(bool_val2);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_COUNTRY): {
		wl_country_t *io_country = (wl_country_t*)arg;
		size_t ccode_buflen = (size_t)(len - OFFSETOF(wl_country_t, ccode));

		if (ccode_buflen < strlen(wlc_channel_ccode(wlc->cmi)) + 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		strncpy(io_country->country_abbrev,
			wlc_channel_country_abbrev(wlc->cmi),
			WLC_CNTRY_BUF_SZ-1);
		io_country->country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
		io_country->rev = wlc_channel_regrev(wlc->cmi);
		strncpy(io_country->ccode,
			wlc_channel_ccode(wlc->cmi),
			strlen(wlc_channel_ccode(wlc->cmi)) + 1);
		break;
	}

#ifndef OPENSRC_IOV_IOCTL
	case IOV_SVAL(IOV_COUNTRY): {
		char country_abbrev[WLC_CNTRY_BUF_SZ];
		char country_abbrev_host[WLC_CNTRY_BUF_SZ];
		char ccode[WLC_CNTRY_BUF_SZ];
		int32 rev = -1;
		chanspec_t chanspec = wlc->chanspec;
		int idx;
		wlc_bsscfg_t *cfg;

		if (cm->ccode_lockdown) {
			WL_REGULATORY(("wl%d:%s(): ccode is lockdown \n",
				wlc->pub->unit, __FUNCTION__));
			break;
		}

		WL_REGULATORY(("wl%d:%s(): set IOV_COUNTRY.\n", wlc->pub->unit, __FUNCTION__));
		/* ensure null terminated */
		memset(country_abbrev, 0, sizeof(country_abbrev));
		strncpy(country_abbrev, (char*)arg, MIN(len, WLC_CNTRY_BUF_SZ-1));
		memset(country_abbrev_host, 0, sizeof(country_abbrev_host));
		strncpy(country_abbrev_host, country_abbrev, strlen(country_abbrev));

		/* country_default : In case associated - if the current country code is
		* not adopted from AP and the current channel is valid for the given
		* country code, only then setting country will be allowed.
		*/
		/* XXX: JIRA:SWWLAN-36509 When locale prioritization feature enabled,
		* program the host country even if the current country is adopted
		* from AP. In case the operating channel is invalid for the host
		* supplied country,adopt the autocountry_default.
		*/
		if (WLC_CNTRY_DEFAULT_ENAB(wlc)) {

			FOREACH_AS_STA(wlc, idx, cfg) {
				if (!BSS_P2P_ENAB(wlc, cfg)) {
					if (!WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc)) {
						if (wlc_11d_autocountry_adopted(wlc->m11d)) {
							err = BCME_ASSOCIATED;
							break;
						}
					}
					if (!wlc_valid_chanspec_cntry
					(wlc->cmi, country_abbrev, wlc->home_chanspec) ||
					!wlc_11d_compatible_country(wlc->m11d, country_abbrev)) {
						if (WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) &&
							WLC_AUTOCOUNTRY_ENAB(wlc)) {
							strncpy(country_abbrev,
							wlc_11d_get_autocountry_default(wlc->m11d),
							WLC_CNTRY_BUF_SZ - 1);
						}
						else {
							err = BCME_ASSOCIATED;
							break;
						}
					}
				}
			}
			if (err) {
				/* save default country for exiting 11d regulatory mode */
				strncpy(cm->country_default,
					country_abbrev_host, WLC_CNTRY_BUF_SZ - 1);

				if (WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) &&
					WLC_AUTOCOUNTRY_ENAB(wlc)) {
					break;
				} else {
					memcpy(country_abbrev,
					wlc_11d_get_autocountry_default(wlc->m11d),
					sizeof(country_abbrev));
				}
			}
		}

		err = wlc_cntry_external_to_internal(country_abbrev, sizeof(country_abbrev));
		if (err)
			break;

		/* ensure null terminated */
		memset(ccode, 0, sizeof(ccode));
		if (len >= (int)sizeof(wl_country_t)) {
			rev = load32_ua((uint8*)&((wl_country_t*)arg)->rev);
			strncpy(ccode, ((wl_country_t*)arg)->ccode, WLC_CNTRY_BUF_SZ-1);
		}

		err = wlc_cntry_external_to_internal(ccode, sizeof(ccode));
		if (err)
			break;

		/* Validate country code before setting */
		err = wlc_valid_countrycode(wlc->cmi, country_abbrev, ccode, rev);
		if (err) {
			break;
		}

		if (ccode[0] == '\0')
			err = wlc_set_countrycode(wlc->cmi, country_abbrev);
		else
			err = wlc_set_countrycode_rev(wlc->cmi, ccode, rev);
		if (err)
			break;

		/* Once the country is set, check for edcrs_eu */
		wlc->is_edcrs_eu = wlc_is_edcrs_eu(wlc);

		/* When set country code, check the current chanspec.
		 * If the current chanspec is invalid,
		 * find the first valid chanspec and set it.
		 */
		if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
			chanspec = wlc_channel_next_2gchanspec(wlc->cmi, chanspec);
			/* If didnt get any, Try 5G Band channels */
			if (!chanspec) {
				chanspec = wlc_channel_5gchanspec_rand(wlc, FALSE);
			}
			if (chanspec) {
				int chspec = chanspec;
				err = wlc_iovar_op(wlc, "chanspec",
					NULL, 0, &chspec, sizeof(int), IOV_SET, wlcif);
			}
		}

		/* the country setting may have changed our radio state */
		wlc_radio_upd(wlc);

		/* save default country for exiting 11d regulatory mode */
		strncpy(cm->country_default, country_abbrev_host, WLC_CNTRY_BUF_SZ - 1);
#ifdef STA
		/* setting the country ends the search for country info */
		if (wlc->m11d)
			wlc_11d_reset_all(wlc->m11d);
#endif // endif
		break;
	}
#endif /* OPENSRC_IOV_IOCTL */

	case IOV_GVAL(IOV_COUNTRY_ABBREV_OVERRIDE): {
		char *country_abbrev = (char*)arg;

		if (len < WLC_CNTRY_BUF_SZ) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		strncpy(country_abbrev, cm->country_abbrev_override,
			WLC_CNTRY_BUF_SZ-1);
		country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
		break;
	}

#ifndef OPENSRC_IOV_IOCTL
	case IOV_SVAL(IOV_COUNTRY_ABBREV_OVERRIDE): {
		char country_abbrev[WLC_CNTRY_BUF_SZ];

		WL_REGULATORY(("wl%d:%s(): set IOV_COUNTRY_ABBREV_OVERRIDE\n",
		               wlc->pub->unit, __FUNCTION__));

		memset(country_abbrev, 0, sizeof(country_abbrev));
		if (len < WLC_CNTRY_BUF_SZ) {
			strncpy(country_abbrev, (char*)arg, len);
		} else {
			strncpy(country_abbrev, (char*)arg, WLC_CNTRY_BUF_SZ-1);
		}

		err = wlc_cntry_external_to_internal(country_abbrev, sizeof(country_abbrev));
		if (err)
			break;

		strncpy(cm->country_abbrev_override, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
		cm->country_abbrev_override[WLC_CNTRY_BUF_SZ-1] = '\0';
		wlc_country_clear_locales(wlc->cntry);
		break;
	}
#endif /* OPENSRC_IOV_IOCTL */

#ifdef BCMDBG
	case IOV_GVAL(IOV_COUNTRY_IE_OVERRIDE): {
		bcm_tlv_t *ie = (bcm_tlv_t*)arg;

		if (cm->country_ie_override == NULL) {
			ie->id = DOT11_MNG_COUNTRY_ID;
			ie->len = 0;
			break;
		} else if ((int)len < (cm->country_ie_override->len + TLV_HDR_LEN)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		bcm_copy_tlv(cm->country_ie_override, (uint8 *)ie);
		break;
	}

	case IOV_SVAL(IOV_COUNTRY_IE_OVERRIDE): {
		bcm_tlv_t *ie = (bcm_tlv_t*)arg;

		if (ie->id != DOT11_MNG_COUNTRY_ID || (int)len < (ie->len + TLV_HDR_LEN)) {
			err = BCME_BADARG;
			break;
		}

		/* free any existing override */
		if (cm->country_ie_override != NULL) {
			MFREE(wlc->osh, cm->country_ie_override,
			      cm->country_ie_override->len + TLV_HDR_LEN);
			cm->country_ie_override = NULL;
		}

		/* save a copy of the Country IE override */
		cm->country_ie_override = MALLOC(wlc->osh, ie->len + TLV_HDR_LEN);
		if (cm->country_ie_override == NULL) {
			err = BCME_NORESOURCE;
			break;
		}

		bcm_copy_tlv(ie, (uint8*)cm->country_ie_override);
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
		break;
	}
#endif /* BCMDBG */
	case IOV_GVAL(IOV_CCODE_INFO): {
		err = wlc_populate_ccode_info(wlc, arg, len);
		break;
	}

	case IOV_GVAL(IOV_COUNTRY_REV): {
		wl_uint32_list_t *list = (wl_uint32_list_t *)arg;
		char abbrev[WLC_CNTRY_BUF_SZ];
		clm_country_t iter;
		ccode_t cc;
		unsigned int rev;

		bzero(abbrev, WLC_CNTRY_BUF_SZ);
		strncpy(abbrev, ((char*)params), WLC_CNTRY_BUF_SZ - 1);
		list->count = 0;

		for (clm_iter_init(&iter); clm_country_iter(&iter, cc, &rev) == CLM_RESULT_OK;) {
			/* If not CC we are looking for - going to next region */
			if (strncmp(cc, abbrev, sizeof(ccode_t))) {
				continue;
			}
			/* Adding revision to buffer */
			if (list->count < ((uint32)val_size-1)) {
				list->element[list->count] = rev;
				list->count++;
			}
		}

		break;
	}

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

int
wlc_populate_ccode_info(wlc_info_t *wlc, void *arg, uint len)
{
	wl_ccode_info_t *ci = (wl_ccode_info_t *)arg;
	wl_ccode_entry_t *ce;
	uint16 need = WL_CCODE_INFO_FIXED_LEN +
		(WLC_NUM_CCODE_INFO * MAXBANDS) * sizeof(wl_ccode_entry_t);
	uint8 band;
	uint16 count;

	if (len < need) {
		WL_ERROR(("IOV_CCODE_INFO: Buffer size: Need %d Received %d\n", need, len));
		return BCME_BUFTOOSHORT;
	}
	bzero(&ci->ccodelist, (WLC_NUM_CCODE_INFO * MAXBANDS) * sizeof(wl_ccode_entry_t));
	ci->version = CCODE_INFO_VERSION;
	for (band = 0, ce = &ci->ccodelist[0], count = 0; band < MAXBANDS; band ++) {
		ce->band = band;
		ce->role = WLC_CCODE_ROLE_ACTIVE;
		if ((band == BAND_2G_INDEX &&
			WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) &&
			!WLC_CNTRY_DEFAULT_ENAB(wlc) &&
			WLC_AUTOCOUNTRY_ENAB(wlc)) ||
			/* If we are in an invalid channel of the host defined country,
			 * accode should also be the default country.
			 */
			(WLC_CNTRY_DEFAULT_ENAB(wlc) &&
			 !wlc_valid_chanspec_db(wlc->cmi, wlc->chanspec) &&
			 CHSPEC_BANDUNIT(wlc->chanspec) == band)) {
			strncpy(ce->ccode,
				wlc_11d_get_autocountry_default(wlc->m11d), sizeof(ccode_t));
		} else {
			strncpy(ce->ccode,
				wlc_channel_country_abbrev(wlc->cmi), sizeof(ccode_t));
		}
		ce ++;
		count ++;

		/* Host configured country code */
		strncpy(ce->ccode, wlc_channel_country_abbrev(wlc->cmi), sizeof(ccode_t));
		ce->band = band;
		ce->role = WLC_CCODE_ROLE_HOST;
		ce ++;
		count ++;

		if (wlc_11d_autocountry_adopted(wlc->m11d)) {
			strncpy(ce->ccode,
				wlc_11d_get_autocountry_adopted(wlc->m11d), sizeof(ccode_t));
			ce->band = band;
			ce->role = WLC_CCODE_ROLE_80211D_ASSOC;
			ce ++;
			count ++;
		}
		if (WLC_AUTOCOUNTRY_ENAB(wlc) && wlc_11d_autocountry_scan_learned(wlc->m11d)) {
			strncpy(ce->ccode,
				wlc_11d_get_autocountry_scan_learned(wlc->m11d), sizeof(ccode_t));
			ce->band = band;
			ce->role = WLC_CCODE_ROLE_80211D_SCAN;
			ce ++;
			count ++;
		}
		if (WLC_AUTOCOUNTRY_ENAB(wlc) || WLC_CNTRY_DEFAULT_ENAB(wlc)) {
			strncpy(ce->ccode,
				wlc_11d_get_autocountry_default(wlc->m11d), sizeof(ccode_t));
			ce->band = band;
			ce->role = WLC_CCODE_ROLE_DEFAULT;
			ce ++;
			count ++;
		}
	}
	ci->count = count;
	return BCME_OK;
}
#ifdef BCMDBG
static int
wlc_cntry_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;

	bcm_bprintf(b, "country_default:%s\n", cm->country_default);

	if (cm->country_ie_override != NULL) {
		wlc_print_ies(cm->wlc, (uint8 *)cm->country_ie_override,
		              cm->country_ie_override->len + TLV_HDR_LEN);
	}

	return BCME_OK;
}
#endif /* BCMDBG */

#if defined(AP) || defined(WLTDLS)

void
wlc_country_clear_locales(wlc_cntry_info_t *cm)
{
	if (cm->locales) {
		MFREE(cm->wlc->osh, cm->locales, sizeof(*cm->locales));
		cm->locales = NULL;
	}
}

static void
wlc_country_update_locales(wlc_cntry_info_t *cm)
{
	wlc_info_t *wlc = cm->wlc;
#if defined(STA) && defined(WL11D)
	const char *country_str;
	char ccode[WLC_CNTRY_BUF_SZ];
#endif	/* defined(STA) && defined(WL11D) */
	clm_country_locales_t locales;
	clm_country_t country;
	clm_result_t result;

	wlc_country_clear_locales(cm);

	if (WLC_AUTOCOUNTRY_ENAB(wlc) && !wlc_11d_autocountry_adopted(wlc->m11d))
		return;

#if defined(STA) && defined(WL11D)
	/* Override if country abbrev override is set */
	country_str = wlc_get_country_string(cm);
	ccode[0] = country_str[0];
	ccode[1] = country_str[1];
#ifdef WL_GLOBAL_RCLASS
	ccode[2] = (wlc_cur_opclass_global(wlc) ? BCMWIFI_GLOBAL_REGULATORY_CLASS : ' ');
#else
	ccode[2] = ' '; /* handling both indoor and outdoor */
#endif	/* WL_GLOBAL_RCLASS */

	if (wlc_country_lookup_ext(wlc, (const char *)ccode, &country) != CLM_RESULT_OK) {
		return;
	}
#endif	/* defined(STA) && defined(WL11D) */

	/* get country based on ccode */
	result = wlc_country_lookup_direct(wlc_channel_ccode(wlc->cmi),
		wlc_channel_regrev(wlc->cmi), &country);

	if (result != CLM_RESULT_OK) {
		return;
	}

	/* get locales based on country */
	if (wlc_get_locale(country, &locales) == CLM_RESULT_OK) {
		cm->locales = MALLOC(wlc->osh, sizeof(*cm->locales));
		if (cm->locales != NULL) {
			memcpy(cm->locales, &locales, sizeof(*cm->locales));
		}
	}
}

static uint8 *
wlc_write_country_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_cntry_info_t *cm, uint8 *cp, int buflen)
{
	chanspec_band_t band;
	uint8 cur_chan, max_chan, first_chan, group_pwr, chan_pwr, min_chan;
	int valid_channel;
	char seen_valid;
	const char *country_str;
	bcm_tlv_t *country_ie = (bcm_tlv_t*)cp;
	uint8 * const orig_cp = cp;
	chanspec_t chspec;
	uint8 *valid_channels;

	if (!cm->locales) {
		wlc_country_update_locales(cm);
	}
	if (!cm->locales) {
		return cp;
	}

	/* make sure we have enough for fixed fields of country ie */
	/* if not, return original buffer pointer */
	BUFLEN_CHECK_AND_RETURN(5, buflen, orig_cp);

	country_ie->id = DOT11_MNG_COUNTRY_ID;

	/* Override if country abbrev override is set */
	country_str = wlc_get_country_string(cm);
	country_ie->data[0] = country_str[0];
	country_ie->data[1] = country_str[1];
#ifdef WL_GLOBAL_RCLASS
	country_ie->data[2] = (wlc_cur_opclass_global(wlc) ?
		BCMWIFI_GLOBAL_REGULATORY_CLASS : ' ');
#else
	country_ie->data[2] = ' '; /* handling both indoor and outdoor */
#endif	/* WL_GLOBAL_RCLASS */

	cp += 5;
	/* update buflen */
	buflen -= 5;

	/* Fill in channels & txpwr */
	seen_valid = 0;
	group_pwr = 0;

	if (CHSPEC_IS2G(cfg->current_bss->chanspec)) {
		min_chan = CH_MIN_2G_CHANNEL;
		max_chan = CH_MAX_2G_CHANNEL;
	} else if CHSPEC_IS5G(cfg->current_bss->chanspec) {
		min_chan = CH_MIN_5G_CHANNEL;
		max_chan = MAXCHANNEL_NUM;
	} else if CHSPEC_IS6G(cfg->current_bss->chanspec) {
		min_chan = CH_MIN_6G_CHANNEL;
		max_chan = CH_MAX_6G_CHANNEL;
	} else {
		WL_ERROR(("%s: Unsupported band, chanspec=%04x\n",
			__FUNCTION__, cfg->current_bss->chanspec));
		min_chan = 1;
		max_chan = MAXCHANNEL_NUM;
	}
	valid_channels = wlc_channel_get_valid_channels_vec(wlc->cmi,
		CHSPEC_BANDUNIT(cfg->current_bss->chanspec));
	band = CHSPEC_BAND(cfg->current_bss->chanspec);

	/* iterate over all + 1, to make sure the last entry gets encoded */
	max_chan++;
	for (first_chan = cur_chan = min_chan; cur_chan <= max_chan; cur_chan++) {

		valid_channel = (cur_chan < max_chan) && isset(valid_channels, cur_chan);
		chan_pwr = 0;
		if (valid_channel) {
			chspec = wf_create_20MHz_chspec(cur_chan, band);
			if (chspec != INVCHANSPEC) {
				chan_pwr = wlc_get_reg_max_power_for_channel_ex(wlc->cmi,
					cm->locales, chspec, TRUE);
			}
		}
		if ((seen_valid) && ((!valid_channel || chan_pwr != group_pwr))) {
			/* make sure buffer big enough for 3 byte data */
			/* if not, return original buffer pointer */
			BUFLEN_CHECK_AND_RETURN(3, buflen, orig_cp);
			*cp++ = first_chan;
			*cp++ = cur_chan - first_chan;
			*cp++ = group_pwr;
			/* update buflen */
			buflen -= 3;
			seen_valid = 0;
		}
		if ((valid_channel) && (!seen_valid)) {
			group_pwr = chan_pwr;
			first_chan = cur_chan;
			seen_valid = 1;
		}
	}

	/* Pad if odd length.  Len excludes ID and len itself. */
	country_ie->len = (uint8)(cp - country_ie->data);
	if (country_ie->len & 1) {
		/* make sure buffer big enough for 1 byte pad */
		/* if not, return original buffer pointer */
		BUFLEN_CHECK_AND_RETURN(1, buflen, orig_cp);
		country_ie->len++;
		*cp++ = 0;
		buflen--;
	}

	return cp;
}

const char *
wlc_get_country_string(wlc_cntry_info_t *cm)
{
	return (cm->country_abbrev_override[0] != '\0') ?
		cm->country_abbrev_override : wlc_channel_country_abbrev(cm->wlc->cmi);
}

static uint8 *
wlc_cntry_write_country_ie(wlc_bsscfg_t *cfg, wlc_cntry_info_t *cm, uint8 *cp, int buflen)
{
	wlc_info_t *wlc = cm->wlc;

	if (!WLC_AUTOCOUNTRY_ENAB(wlc) || wlc_11d_autocountry_adopted(wlc->m11d)) {
#ifdef BCMDBG
		/* Override country IE if is configured */
		if (cm->country_ie_override != NULL)
			cp = bcm_copy_tlv_safe(cm->country_ie_override, cp, buflen);
		else
#endif // endif
			cp = wlc_write_country_ie(wlc, cfg, cm, cp, buflen);
	}

	return cp;
}
#endif /* AP || WLTDLS */

#ifdef AP
/* 802.11d, 802.11h Country Element */
static uint
wlc_cntry_calc_cntry_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;
	wlc_info_t *wlc = cm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	uint8 buf[257];
	uint country_ie_bufsize = 0;

	if (cm->cached_cntry_ie != NULL) {
		return cm->cached_cntry_ie->len + TLV_HDR_LEN;
	}

	if ((BSS_WL11H_ENAB(wlc, cfg) || WL11D_ENAB(wlc)) && BSSCFG_AP(cfg)) {
		country_ie_bufsize = (uint)(wlc_cntry_write_country_ie(data->cfg, cm, buf,
			sizeof(buf)) - buf);

		/* Cache the created country ie, to save time when wlc_cntry_write_cntry_ie()
		 * is called. Saves time in VSDB STA/GO use case.
		 */
		if (country_ie_bufsize) {
			cm->cached_cntry_ie = MALLOC(wlc->osh, country_ie_bufsize);
			if (cm->cached_cntry_ie != NULL) {
				memcpy(cm->cached_cntry_ie, buf, country_ie_bufsize);
				ASSERT(cm->cached_cntry_ie->len + TLV_HDR_LEN ==
					country_ie_bufsize);
			}
		}
	}

	return country_ie_bufsize;
}

static int
wlc_cntry_write_cntry_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;
	wlc_info_t *wlc = cm->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if ((BSS_WL11H_ENAB(wlc, cfg) || WL11D_ENAB(wlc)) && BSSCFG_AP(cfg)) {
		/* Usually wlc_cntry_calc_cntry_ie_len() has been called before,
		 * which already created a cached country ie.
		 */
		if (cm->cached_cntry_ie != NULL) {
			data->buf = bcm_copy_tlv_safe(cm->cached_cntry_ie, data->buf,
				data->buf_len);
			/* Country ie only contains txpwr information for valid channels for
			 * current band. To minimize risk, free the cached ie here so band can be
			 * changed without having to consider country ie caching.
			 */
			MFREE(wlc->osh, cm->cached_cntry_ie,
				cm->cached_cntry_ie->len + TLV_HDR_LEN);
			cm->cached_cntry_ie = NULL;
		} else {
			wlc_cntry_write_country_ie(data->cfg, cm, data->buf, data->buf_len);
		}
	}

	return BCME_OK;
}
#endif /* AP */

#ifdef WLTDLS
/* 802.11d, 802.11h Country Element */
static uint
wlc_cntry_tdls_calc_cntry_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;
	wlc_info_t *wlc = cm->wlc;
	uint8 buf[257];

	/* TODO: needs better way to calculate the IE length */

	if (WL11H_ENAB(wlc))
		return (uint)(wlc_cntry_write_country_ie(data->cfg, cm, buf, sizeof(buf)) - buf);

	return 0;
}

static int
wlc_cntry_tdls_write_cntry_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;
	wlc_info_t *wlc = cm->wlc;

	if (WL11H_ENAB(wlc))
		wlc_cntry_write_country_ie(data->cfg, cm, data->buf, data->buf_len);

	return BCME_OK;
}
#endif /* WLTDLS */

#ifdef STA
int
wlc_cntry_parse_country_ie(wlc_cntry_info_t *cm, const bcm_tlv_t *ie,
	char *country, chanvec_t *valid_channels, int8 *tx_pwr)
{
	wlc_info_t *wlc = cm->wlc;
	const uint8 *cp;
	char ie_len = ie->len;
	int8 channel_txpwr;
	uchar channel, channel_len;
	uint8 ch_sep;

	(void)wlc;

	ASSERT(ie->id == DOT11_MNG_COUNTRY_ID);

	bzero(country, 4);
	bzero(valid_channels, sizeof(chanvec_t));

	if (ie_len >= WLC_MIN_CNTRY_ELT_SZ) {
		/* parse country string */
		cp = ie->data;
		country[0] = *(cp++);
		country[1] = *(cp++);
		country[2] = *(cp++);
		country[3] = 0;	/* terminate */
		ie_len -= 3;

		/* Parse all first channel/num channels/tx power triples */
		while (ie_len >= 3) {
			channel = cp[0];
			channel_len = cp[1];
			channel_txpwr = (int8)cp[2];

			ch_sep = (channel <= CH_MAX_2G_CHANNEL)? CH_5MHZ_APART: CH_20MHZ_APART;
			for (; channel_len && channel < MAXCHANNEL; channel += ch_sep,
				channel_len--) {
				setbit(valid_channels->vec, channel);
				tx_pwr[(int)channel] = channel_txpwr;
			}
			ie_len -= 3;
			cp += 3;
		}
	} else {
		WL_REGULATORY(("wl%d: %s: malformed country ie: len %d\n",
			wlc->pub->unit, __FUNCTION__, ie_len));
		return BCME_ERROR;
	}

	return BCME_OK;
}

/* Country IE adopting rules:
 *	1. If no Country IE has ever been adopted, adopt it.
 *	2. If associted AP is an normal AP, adopt it.
 *	3. If associated AP is a GO, adopt ONLY if it has NOT adopted an
 *        Country IE from a normal associated AP.
*/

void
wlc_cntry_adopt_country_ie(wlc_cntry_info_t *cm, wlc_bsscfg_t *cfg, uint8 *tags, int tags_len)
{
	wlc_info_t *wlc = cm->wlc;
	bcm_tlv_t *country_ie;
	bool adopt_country;

	BCM_REFERENCE(cfg);

	country_ie = bcm_parse_tlvs(tags, tags_len, DOT11_MNG_COUNTRY_ID);
#ifdef WLP2P
	adopt_country = bcm_find_p2pie(tags, tags_len) == NULL;
#else
	adopt_country = TRUE;
#endif // endif

	WL_REGULATORY(("wl%d: %s: is_normal_ap = %d, country_ie %s\n",
	               wlc->pub->unit, __FUNCTION__,
	               adopt_country, ((country_ie) ? "PRESENT" : "ABSENT")));

	if (country_ie != NULL) {
		char country_str[WLC_CNTRY_BUF_SZ];
		int8 ie_tx_pwr[MAXCHANNEL];
		chanvec_t channels;

		/* Init the array to max value */
		memset(ie_tx_pwr, WLC_TXPWR_MAX, sizeof(ie_tx_pwr));

		bzero(channels.vec, sizeof(channels.vec));

		if (wlc_cntry_parse_country_ie(cm, country_ie, country_str, &channels, ie_tx_pwr)) {
			WL_REGULATORY(("wl%d: %s: malformed Country IE\n",
			               wlc->pub->unit, __FUNCTION__));
			return;
		}

		/* XXX: JIRA:SWWLAN-36509: When locale prioritization feature enabled, host
		 *  supplied ccode will always be adopted
		 */
		if (WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) &&
			WLC_CNTRY_DEFAULT_ENAB(wlc)) {
			wlc_11d_reset_autocountry_adopted(wlc->m11d);
			wlc_11d_adopt_country(wlc->m11d, cm->country_default, FALSE);
		} else {
			wlc_11d_adopt_country(wlc->m11d, country_str, adopt_country);
		}

		/* Do not update txpwr_local_max if home_chanspec is not in the country
		 * IE (due to bug in AP?)
		 */
		if (WL11H_ENAB(wlc) && wlc->pub->associated &&
		    isset(channels.vec, wf_chspec_ctlchan(wlc->home_chanspec))) {
			uint8 txpwr;
			uint8 constraint;

#ifndef WL_EIRP_OFF
			txpwr = ie_tx_pwr[wf_chspec_ctlchan(wlc->home_chanspec)];
#else
			txpwr = wlc_get_reg_max_power_for_channel(wlc->cmi, wlc->chanspec, TRUE);
			WL_ASSOC(("txpwr by local max: use max txpwr %ddbm from Regulatory"
				" Limits instead of beacon's TPC %ddbm\n",
				txpwr, ie_tx_pwr[wf_chspec_ctlchan(wlc->home_chanspec)]));
#endif /* WL_EIRP_OFF */

			/* Convert from radiated to conducted Tx pwr only for EIRP locales */
			/* Restore legacy behavior where txpwr=0 is special and indicates
			 * NO CONSTRAINT (see wlc_tpc_get_local_constraint_qdbm())
			 */
			if ((txpwr != 0) && (wlc_channel_locale_flags(wlc->cmi) & WLC_EIRP)) {
				txpwr -= wlc->band->antgain / WLC_TXPWR_DB_FACTOR;
			}

			wlc_tpc_set_local_max(wlc->tpc, txpwr);
			constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);
			wlc_channel_set_txpower_limit(wlc->cmi, constraint);
			WL_REGULATORY(("wl%d: Adopting Country IE \"%s\" "
			               "channel %d txpwr local max %d dBm\n", wlc->pub->unit,
			               country_str, wf_chspec_ctlchan(wlc->home_chanspec),
			               txpwr));
		}
	}
	else if (WLC_CNTRY_DEFAULT_ENAB(wlc)) {
		/* If country_default feature is enabled, adopt the cntry_default CCODE
		* if the AP does not support 11d. Reset the adopted country flag so that
		* it does not remain true when the new association is casued due to roaming.
		*/
		wlc_11d_reset_autocountry_adopted(wlc->m11d);
		wlc_11d_adopt_country(wlc->m11d, cm->country_default, FALSE);
	}
}
#endif /* STA */

int
wlc_cntry_use_default(wlc_cntry_info_t *cm)
{
	wlc_info_t *wlc = cm->wlc;

	WL_INFORM(("wl%d:%s(): restore country_default %s.\n",
	           wlc->pub->unit, __FUNCTION__, cm->country_default));

	return wlc_set_countrycode(wlc->cmi, cm->country_default);
}

/* accessors */
void
wlc_cntry_set_default(wlc_cntry_info_t *cm, const char *country_abbrev)
{
	strncpy(cm->country_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
}
#endif /* WLCNTRY */
