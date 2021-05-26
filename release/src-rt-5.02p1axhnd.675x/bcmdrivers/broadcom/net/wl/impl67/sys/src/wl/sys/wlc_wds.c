/*
 * Dynamic WDS module source file
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
 * $Id: wlc_wds.c 790417 2020-08-25 06:45:04Z $
 */

/**
 * @file
 * @brief
 * DynamicWDS (DWDS) is used to bridge the networks over wireless. User should be able to establish
 * WDS connection dynamically by just using the upstream APs ssid. There are two parts in this DWDS:
 * DWDS client which scans and joins the UAP and indicates that it wants to establish WDS
 * connection. DWDS AP: on seeing a client association with DWDS request creates WDS connection with
 * that client.
 *
 * In DWDS, an infrastructure STA uses 4-address data frames to provide wireless bridging on behalf
 * of multiple downstream network devices. The STA acts a normal infrastructure STA in all ways
 * except that it uses 4-address (FromDS & ToDS) frame format for all data frames to/from the AP to
 * which it is associated.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [DynamicWDS]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifdef WDS

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_wds.h>
#include <wlc_ap.h>
#include <wl_export.h>
#include <wlc_event.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_helper.h>
#include <wlc_pspretend.h>
#include <wlc_psta.h>
#include <wlc_event_utils.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#ifdef WLAMPDU
#include <wlc_ampdu_cmn.h>
#endif // endif
#ifdef WLCSA
#include <wlc_11h.h>
#include <wlc_csa.h>
#include <wlc_dfs.h>
#endif /* WLCSA */

#define EA_CMP(e1, e2) \
	(!((((uint16 *)(e1))[0] ^ ((uint16 *)(e2))[0]) | \
	   (((uint16 *)(e1))[1] ^ ((uint16 *)(e2))[1]) | \
	   (((uint16 *)(e1))[2] ^ ((uint16 *)(e2))[2])))

#ifndef IFNAMSIZ
#define IFNAMSIZ		16
#endif /* IFNAMSIZ */

static int wlc_get_free_wds_index(wlc_bsscfg_t *cfg);

/* IOVar table */
enum {
	IOV_WDS_WPA_ROLE = 1,
	IOV_WDSTIMEOUT = 2,
	IOV_WDS_ENABLE = 3,	/* enable/disable wds link events */
	IOV_DWDS = 4,
	IOV_DWDS_CONFIG = 5,
	IOV_WDS_TYPE = 6,
	IOV_WDS_AP_IFNAME = 7,
#ifdef DWDS
	IOV_DWDS_LB_FILTER = 8,
#endif // endif
	IOV_LAST
};

static const bcm_iovar_t wlc_wds_iovars[] = {
	{"wds_wpa_role", IOV_WDS_WPA_ROLE,
	(IOVF_SET_UP), 0, IOVT_BUFFER, ETHER_ADDR_LEN+1
	},
	{"wdstimeout", IOV_WDSTIMEOUT,
	(IOVF_WHL), 0, IOVT_UINT32, 0
	},
	{"wds_enable", IOV_WDS_ENABLE,
	(IOVF_SET_UP), 0, IOVT_BOOL, 0
	},
#ifdef DWDS
	{"dwds", IOV_DWDS, (IOVF_SET_DOWN), 0, IOVT_BOOL, 0},
	{"dwds_config", IOV_DWDS_CONFIG, (IOVF_SET_UP), 0, IOVT_BUFFER, sizeof(wlc_dwds_config_t)},
#endif // endif
	{"wds_type", IOV_WDS_TYPE,
	(0), 0, IOVT_UINT32, 0
	},
	{"wds_ap_ifname", IOV_WDS_AP_IFNAME, (0), 0, IOVT_BUFFER, 0},
#ifdef DWDS
	{"dwds_lb_filter", IOV_DWDS_LB_FILTER, (0), 0, IOVT_BOOL, 0},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

/* wds module info */
struct wlc_wds_info {
	wlc_info_t	*wlc;
	wlc_pub_t	*pub;
	bool		lazywds;	/* create WDS partners on the fly */
	bool		wdsactive;	/* There are one or more WDS i/f(s) */
	uint		wds_timeout;	/* inactivity timeout for WDS links */
};

/* local functions */
static void wlc_wds_watchdog(void *arg);
static int wlc_wds_wpa_role_set(wlc_wds_info_t *mwds, struct scb *scb, uint8 role);
static int wlc_wds_wpa_role_get(wlc_wds_info_t *mwds, wlc_bsscfg_t *cfg, struct ether_addr *ea,
                                uint8 *role);
static void wlc_ap_wds_timeout(wlc_wds_info_t *mwds);
static void wlc_ap_wds_probe(wlc_wds_info_t *mwds, struct scb *scb);
#ifdef DWDS
static int wlc_dwds_config(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_dwds_config_t *dwds);
static void wlc_dwds_scb_state_upd(void *ctx, scb_state_upd_data_t *data);
#endif /* DWDS */

#if defined(DPSTA) || defined(DWDS)
static void wlc_dwds_mode_enable(wlc_info_t *wlc, struct scb *scb,
	wlc_bsscfg_t *bsscfg, bool enable);
#endif /* defined (DPSTA) || defined (DWDS) */

#ifdef BCMDBG
static int wlc_dump_wds(wlc_wds_info_t *mwds, struct bcmstrbuf *b);
#endif /* BCMDBG */

/* module */
static int wlc_wds_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static int wlc_wds_ioctl(void *hdl, uint cmd, void *arg, uint len, struct wlc_if *wlcif);

/* scb cubby */
static void wlc_wds_scb_deinit(void *ctx, struct scb *scb);
static void wlc_wds_free_if(wlc_info_t *wlc, struct scb *scb);

/* IE mgmt callbacks */
static int wlc_wds_bcn_parse_wme_ie(void *ctx, wlc_iem_parse_data_t *data);
#if defined(DWDS)
static int wlc_dwds_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif // endif
#ifdef MULTIAP
static uint wlc_wds_calc_multiap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_wds_write_multiap_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_wds_parse_multiap_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif	/* MULTIAP */
#ifdef PSPRETEND
#include <wlc_apps.h>
#endif /* PSPRETEND */

/* PS listen interval (no of beacon intervals) for WDS SCB.
 * Used for PS pretend probing timeout also.
 */
#define WDS_SCB_LISTEN_INT	20

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_wds_info_t *
BCMATTACHFN(wlc_wds_attach)(wlc_info_t *wlc)
{
	wlc_wds_info_t *mwds;
	wlc_pub_t *pub = wlc->pub;
	int err = 0;
#ifdef MULTIAP
	uint16 map_fstbmp = FT2BMP(FC_ASSOC_RESP) | FT2BMP(FC_REASSOC_RESP) |
		FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);
#endif	/* MULTIAP */

	if ((mwds = MALLOCZ(wlc->osh, sizeof(wlc_wds_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero(mwds, sizeof(wlc_wds_info_t));

	mwds->wlc = wlc;
	mwds->pub = pub;

#ifdef BCMDBG
	wlc_dump_register(pub, "wds", (dump_fn_t)wlc_dump_wds, (void *)mwds);
#endif // endif

	if (wlc_module_register(pub, wlc_wds_iovars, "wds", mwds, wlc_wds_doiovar,
	                        wlc_wds_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	err = wlc_module_add_ioctl_fn(wlc->pub, (void *)mwds, wlc_wds_ioctl, 0, NULL);
	if (err) {
		WL_ERROR(("%s: wlc_module_add_ioctl_fn err=%d\n",
		          __FUNCTION__, err));
		goto fail;
	}

#if defined(DWDS)
	if (wlc_scb_state_upd_register(wlc, wlc_dwds_scb_state_upd, (void*)wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scb_state_upd_register failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	{
	uint16 brcm_parse_fstbmp =
#ifdef STA
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
	        FT2BMP(FC_BEACON) |
#endif // endif
#ifdef AP
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
#endif // endif
	        0;
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, brcm_parse_fstbmp, WLC_IEM_VS_IE_PRIO_BRCM,
	                            wlc_dwds_parse_brcm_ie, mwds) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_vs_add_parse_fn failed, brcm ie in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	}
#endif /* DWDS */

	if (wlc_scb_cubby_reserve(wlc, 0, NULL, wlc_wds_scb_deinit, NULL, mwds) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_iem_vs_add_parse_fn(wlc->iemi, WLC_IEM_FC_AP_BCN, WLC_IEM_VS_IE_PRIO_WME,
	                            wlc_wds_bcn_parse_wme_ie, mwds) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_vs_add_parse_fn failed, wme ie in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef MULTIAP
	if ((err = wlc_iem_vs_add_build_fn_mft(wlc->iemi, map_fstbmp, WLC_IEM_VS_IE_PRIO_MULTIAP,
		wlc_wds_calc_multiap_ie_len, wlc_wds_write_multiap_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_build_fn failed, err %d, multiap ie\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_vs_add_parse_fn_mft(wlc->iemi, map_fstbmp, WLC_IEM_VS_IE_PRIO_MULTIAP,
		wlc_wds_parse_multiap_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, err %d, multiap ie in assocreq\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
#endif	/* MULTIAP */
	return mwds;

	/* error handling */
fail:
	MODULE_DETACH(mwds, wlc_wds_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_wds_detach)(wlc_wds_info_t *mwds)
{
	wlc_info_t *wlc;

	if (mwds == NULL)
		return;

	wlc = mwds->wlc;
#if defined(DWDS)
	wlc_scb_state_upd_unregister(wlc, wlc_dwds_scb_state_upd, (void*)wlc);
#endif /* DWDS */
	wlc_module_unregister(wlc->pub, "wds", mwds);

	(void)wlc_module_remove_ioctl_fn(wlc->pub, (void *)mwds);

	MFREE(wlc->osh, mwds, sizeof(wlc_wds_info_t));
}

static int
wlc_wds_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_wds_info_t *mwds = (wlc_wds_info_t*)ctx;
	wlc_info_t *wlc = mwds->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_WDS_WPA_ROLE): {
		/* params buf is an ether addr */
		uint8 role = 0;
		if (p_len < ETHER_ADDR_LEN) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		err = wlc_wds_wpa_role_get(mwds, bsscfg, params, &role);
		*(uint8*)arg = role;
		break;
	}

	case IOV_SVAL(IOV_WDS_WPA_ROLE): {
		/* arg format: <mac><role> */
		struct scb *scb;
		uint8 *mac = (uint8 *)arg;
		uint8 *role = mac + ETHER_ADDR_LEN;
		if (!(scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)mac))) {
			err = BCME_NOTFOUND;
			goto exit;
		}
		err = wlc_wds_wpa_role_set(mwds, scb, *role);
		break;
	}

	case IOV_GVAL(IOV_WDSTIMEOUT):
		*ret_int_ptr = (int32)mwds->wds_timeout;
		break;

	case IOV_SVAL(IOV_WDSTIMEOUT):
		mwds->wds_timeout = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_WDS_ENABLE):
		/* do nothing */
		break;

	case IOV_SVAL(IOV_WDS_ENABLE):
		if (wlcif == NULL || wlcif->type != WLC_IFTYPE_WDS) {
			WL_ERROR(("invalid interface type for IOV_WDS_ENABLE\n"));
			err = BCME_NOTFOUND;
			goto exit;
		}
		err = wlc_wds_create_link_event(wlc, wlcif->u.scb, TRUE);
		break;

#ifdef DWDS
	case IOV_GVAL(IOV_DWDS):
		*ret_int_ptr = (int32)bsscfg->_dwds;
		break;

	case IOV_SVAL(IOV_DWDS):
		if (bool_val) {
			/* enable dwds */
			bsscfg->_dwds = TRUE;
		} else {
			bsscfg->_dwds = FALSE;
		}
#ifdef WL11AX
		if (HE_ENAB(wlc->pub) && (BSSCFG_IS_PRIMARY(bsscfg)) &&
			!HE_ULMU_STA_ENAB(wlc->pub)) {
				WLC_HE_FEATURES_SET(wlc->pub, WL_HE_FEATURES_ULOMU);
		}
#endif /* WL11AX */
		break;

	case IOV_SVAL(IOV_DWDS_CONFIG): {
		wlc_dwds_config_t dwds;

		bcopy(arg, &dwds, sizeof(wlc_dwds_config_t));
		err = wlc_dwds_config(wlc, bsscfg, &dwds);
		break;
	}
#endif /* DWDS */

	case IOV_GVAL(IOV_WDS_TYPE): {
		*ret_int_ptr = WL_WDSIFTYPE_NONE;
		if (wlcif->type == WLC_IFTYPE_WDS)
			*ret_int_ptr = SCB_DWDS(wlcif->u.scb) ?
				WL_WDSIFTYPE_DWDS : WL_WDSIFTYPE_WDS;
		break;
	 }

	case IOV_GVAL(IOV_WDS_AP_IFNAME): {
		 if ((wlcif->type == WLC_IFTYPE_WDS) && BSSCFG_AP(bsscfg)) {
			strncpy(arg, wl_ifname(wlc->wl, bsscfg->wlcif->wlif), IFNAMSIZ);
			break;
		 }
		 err = BCME_UNSUPPORTED;
		 break;
	}

#ifdef DWDS
	case IOV_GVAL(IOV_DWDS_LB_FILTER):
		*ret_int_ptr = (int32)bsscfg->dwds_loopback_filter;
		break;

	case IOV_SVAL(IOV_DWDS_LB_FILTER): {
		if (MAP_ENAB(bsscfg) && BSSCFG_STA(bsscfg)) {
			if (bool_val) {
				/* enable dwds loopback filter */
				bsscfg->dwds_loopback_filter = TRUE;
			} else {
				bsscfg->dwds_loopback_filter = FALSE;
				wlc_dwds_flush_salist(wlc, bsscfg);
			}
			break;
		}
		err = BCME_UNSUPPORTED;
		break;
	}
#endif /* DWDS */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

exit:
	return err;
}

static int
wlc_wds_ioctl(void *hdl, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_wds_info_t *mwds = (wlc_wds_info_t *) hdl;
	wlc_info_t *wlc = mwds->wlc;
	int val = 0, *pval;
	bool bool_val;
	int bcmerror = 0;
	uint i;
	struct maclist *maclist;
	struct wds_maclist *wds_maclist;
	struct dwds_maclist *dwds_maclist;
	wlc_bsscfg_t *bsscfg;
	struct scb_iter scbiter;
	struct scb *scb = NULL;

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* default argument is generic integer */
	pval = (int *)arg;
	/* This will prevent the misaligned access */
	if (pval && (uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));

	/* bool conversion to avoid duplication below */
	bool_val = (val != 0);

	switch (cmd) {

	case WLC_SET_WDSLIST:
		ASSERT(arg != NULL);
		maclist = (struct maclist *) arg;
		ASSERT(maclist);
		if (maclist->count > (uint) wlc->pub->tunables->maxscb) {
			bcmerror = BCME_RANGE;
			break;
		}

		if (len < (uint)(OFFSETOF(struct maclist, ea) + maclist->count * ETHER_ADDR_LEN)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		/* Mark current wds nodes for reclamation */
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (scb->wds)
				scb->permanent = FALSE;
		}

		if (maclist->count == 0)
			mwds->wdsactive = FALSE;

		/* set new WDS list info */
		for (i = 0; i < maclist->count; i++) {
			if (ETHER_ISMULTI(&maclist->ea[i])) {
				bcmerror = BCME_BADARG;
				break;
			}
			if (!(scb = wlc_scblookup(wlc, bsscfg, &maclist->ea[i]))) {
				bcmerror = BCME_NOMEM;
				break;
			}

			if (SCB_DEL_IN_PROGRESS(scb) || SCB_MARKED_DEL(scb)) {
				/* Delete and realloc for WDS client */
				wlc_scbfree(wlc, scb);
				if (!(scb = wlc_scblookup(wlc, bsscfg, &maclist->ea[i]))) {
					bcmerror = BCME_NOMEM;
					break;
				}
			}
			bcmerror = wlc_wds_create(wlc, scb, 0);
			if (bcmerror) {
				wlc_scbfree(wlc, scb);
				break;
			}

			/* WDS creation was successful so mark the scb permanent and
			 * note that WDS is active
			 */
			mwds->wdsactive = TRUE;
		}

		/* free (only) stale wds entries */
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (scb->wds && !scb->permanent && !SCB_DWDS(scb)) {
				wlc_scbfree(wlc, scb);
			}
		}

#ifdef STA
		wlc_radio_mpc_upd(wlc);
#endif // endif

		/* if we are "associated" as an AP, we have already founded the BSS
		 * and adjusted aCWmin. If not associated, then we need to adjust
		 * aCWmin for the WDS link
		 */
		if (wlc->pub->up && !wlc->pub->associated && BAND_2G(wlc->band->bandtype)) {
			wlc_suspend_mac_and_wait(wlc);

			if (maclist->count > 0)
				/* Update aCWmin based on basic rates. */
				wlc_cwmin_gphy_update(wlc, &bsscfg->current_bss->rateset, TRUE);
			else
				/* Unassociated gphy CWmin */
				wlc_set_cwmin(wlc, APHY_CWMIN);

			wlc_enable_mac(wlc);
		}
		break;

	case WLC_GET_WDSLIST:
		ASSERT(arg != NULL);
		wds_maclist = (struct wds_maclist *) arg;

		ASSERT(wds_maclist);
		wds_maclist->magic = WDS_MACLIST_MAGIC;
		wds_maclist->version = WDS_MACLIST_VERSION;

		/* count WDS stations */
		val = 0;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if ((scb->wds) && !SCB_DWDS(scb))
				val++;
		}
		if (wds_maclist->count < (uint)val) {
			bcmerror = BCME_RANGE;
			break;
		}
		if (len < (uint)(((val - 1) * (int)sizeof(struct wds_client_info)) +
			(int)sizeof(struct wds_maclist))) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		wds_maclist->count = 0;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if ((scb->wds) && !SCB_DWDS(scb)) {
				struct wds_client_info * wds_client;
				wds_client = &(wds_maclist->client_list[wds_maclist->count]);

				/* Copy wds client ifname */
				strncpy((void *)wds_client->ifname,
					wl_ifname(wlc->wl, (SCB_WDS(scb))->wlif), IFNAMSIZ);
				wds_client->ifname[IFNAMSIZ - 1] = '\0';

				/* Copy WDS client MAC address */
				bcopy((void*)&scb->ea, (void*)&wds_client->ea, ETHER_ADDR_LEN);
				wds_maclist->count++;
			}
		}
		ASSERT(wds_maclist->count == (uint)val);
		break;

	case WLC_GET_DWDSLIST:
		ASSERT(arg != NULL);
		dwds_maclist = (struct dwds_maclist *) arg;

		ASSERT(dwds_maclist);

		val = 0;
		/* count DWDS stations */
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (SCB_DWDS(scb))
				val++;
		}

		if (len < ((int)sizeof(*dwds_maclist) +
			(val * (int)sizeof(struct wds_client_info)))) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		dwds_maclist->version = DWDS_MACLIST_VER_1;
		dwds_maclist->data_offset = sizeof(*dwds_maclist);
		dwds_maclist->count = 0;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (SCB_DWDS(scb)) {
				struct wds_client_info * client_info;
				client_info = &(dwds_maclist->client_info[dwds_maclist->count]);

				/* Copy DWDS client ifname */
				if (SCB_WDS(scb)) {
					strncpy((void *)client_info->ifname,
						wl_ifname(wlc->wl, (SCB_WDS(scb))->wlif), IFNAMSIZ);
					client_info->ifname[IFNAMSIZ - 1] = '\0';
				}

				/* Copy DWDS client MAC address */
				bcopy((void*)&scb->ea, (void*)&client_info->ea, ETHER_ADDR_LEN);
				dwds_maclist->count++;
			}
		}

		ASSERT(dwds_maclist->count == (uint)val);
		dwds_maclist->length = (uint16)sizeof(*dwds_maclist) +
			(dwds_maclist->count * (uint16)sizeof(struct wds_client_info));
		break;

	case WLC_GET_LAZYWDS:
		if (pval) {
			*pval = (int)mwds->lazywds;
		}
		break;

	case WLC_SET_LAZYWDS:
		mwds->lazywds = bool_val;
		if (wlc->aps_associated && wlc_update_brcm_ie(wlc)) {
			WL_APSTA_BCN(("wl%d: WLC_SET_LAZYWDS -> wlc_update_beacon()\n",
				wlc->pub->unit));
			wlc_update_beacon(wlc);
			wlc_update_probe_resp(wlc, TRUE);
		}
		break;

	case WLC_WDS_GET_REMOTE_HWADDR:
		if (wlcif == NULL || wlcif->type != WLC_IFTYPE_WDS) {
			WL_ERROR(("invalid interface type for WLC_WDS_GET_REMOTE_HWADDR\n"));
			bcmerror = BCME_NOTFOUND;
			break;
		}

		ASSERT(arg != NULL);
		bcopy(&wlcif->u.scb->ea, arg, ETHER_ADDR_LEN);
		break;

	case WLC_WDS_GET_WPA_SUP: {
		uint8 sup;
		ASSERT(pval != NULL);
		bcmerror = wlc_wds_wpa_role_get(mwds, bsscfg, (struct ether_addr *)pval, &sup);
		if (!bcmerror)
			*pval = sup;
		break;
	}

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return (bcmerror);
}

static void
wlc_wds_watchdog(void *arg)
{
	wlc_wds_info_t *mwds = (wlc_wds_info_t *) arg;
	wlc_info_t *wlc = mwds->wlc;

	BCM_REFERENCE(wlc);

	if (AP_ENAB(wlc->pub)) {
		/* DWDS does not use this. */
		wlc_ap_wds_timeout(mwds);
	}
}

#ifdef BCMDBG
static int
wlc_dump_wds(wlc_wds_info_t *mwds, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "lazywds %d\n", mwds->lazywds);

	return 0;
}
#endif /* BCMDBG */

int
wlc_wds_create(wlc_info_t *wlc, struct scb *scb, uint flags)
{
#ifdef PSPRETEND
	wlc_bss_info_t *current_bss;
	ASSERT((scb != NULL) && (scb->bsscfg != NULL));

	current_bss = scb->bsscfg->current_bss;
	ASSERT(current_bss);
#else
	ASSERT(scb != NULL);
#endif /* PSPRETEND */
	/* honor the existing WDS link */
	if (scb->wds != NULL) {
		if (!(flags & WDS_DYNAMIC))
			scb->permanent = TRUE;
		return BCME_OK;
	}

	if (!(flags & WDS_INFRA_BSS) && SCB_ISMYAP(scb)) {
#ifdef BCMDBG_ERR
		char eabuf[ETHER_ADDR_STR_LEN];
		WL_ERROR(("wl%d: rejecting WDS %s, associated to it as our AP\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
#endif /* BCMDBG_ERR */
		return BCME_ERROR;
	}

	/* allocate a wlc_if_t for the wds interface and fill it out */
	scb->wds = wlc_wlcif_alloc(wlc, wlc->osh, WLC_IFTYPE_WDS,
			(wlc->primary_queue ? wlc->primary_queue : wlc->def_primary_queue));

	if (scb->wds == NULL) {
		WL_ERROR(("wl%d: wlc_wds_create: failed to alloc wlcif\n",
		          wlc->pub->unit));
		return BCME_NOMEM;
	}
	scb->wds->u.scb = scb;
	scb->if_stats = scb->wds->_cnt;

#ifdef AP
	/* create an upper-edge interface */
	if (!(flags & WDS_INFRA_BSS)) {
		/* a WDS scb has an AID for a unique WDS interface unit number */
		wlc_wds_wpa_role_set(wlc->mwds, scb, WL_WDS_WPA_ROLE_AUTO);
		scb->wds->wlif = wl_add_if(wlc->wl, scb->wds, scb->aid, &scb->ea);
		if (scb->wds->wlif == NULL) {
			wlc_wlcif_free(wlc, wlc->osh, scb->wds);
			scb->wds = NULL;
			return BCME_NOMEM;
		}
		scb->bsscfg->wlcif->if_flags |= WLC_IF_LINKED;
		wlc_if_event(wlc, WLC_E_IF_ADD, scb->wds);
	} else
		wlc_wds_wpa_role_set(wlc->mwds, scb, WL_WDS_WPA_ROLE_AUTO);
#endif /* AP */

	/* Dont do this for DWDS. */
	if (!(flags & WDS_DYNAMIC)) {
		/* override WDS nodes rates to the full hw rate set */
		wlc_rateset_filter(&wlc->band->hw_rateset /* src */, &scb->rateset /* dst */, FALSE,
			WLC_RATES_CCK_OFDM, RATE_MASK, wlc_get_mcsallow(wlc, scb->bsscfg));
		wlc_scb_ratesel_init(wlc, scb);

		scb->permanent = TRUE;
		scb->flags &= ~SCB_MYAP;
#ifdef PSPRETEND
		/* scb->listen is used by the AP for timing out PS pkts,
		 * ensure pkts are held for at least one dtim period
		 * Setting listen interval here as there is no assoc req for WDS links
		 */
		wlc_apps_set_listen_prd(wlc, scb, MAX(current_bss->dtim_period,
			WDS_SCB_LISTEN_INT));
#endif /* PSPRETEND */
		/* legacy WDS does 4-addr nulldata and 8021X frames */
		scb->flags3 |= SCB3_A4_NULLDATA;
		scb->flags3 |= SCB3_A4_8021X;
	} else
		SCB_DWDS_ACTIVATE(scb);

	SCB_A4_DATA_ENABLE(scb);
#if defined(PKTC) && defined(DWDS)
	if (flags & WDS_DYNAMIC)
		SCB_PKTC_DISABLE(scb); /* disable pktc for WDS scb */
#endif /* PKTC && DWDS */

	return BCME_OK;
}

static void
wlc_wds_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_wds_info_t *mwds = (wlc_wds_info_t *)ctx;
	wlc_info_t *wlc = mwds->wlc;
	wlc_wds_free_if(wlc, scb);
}

static void
wlc_wds_free_if(wlc_info_t *wlc, struct scb *scb)
{
	/* free WDS state */
	if (scb->wds == NULL)
		return;

	/* process event queue */
	wlc_eventq_flush(wlc->eventq);

	if (scb->wds->wlif) {
		wlc_update_wds_index(scb->bsscfg, scb->wds->wlif, FALSE);
		wlc_if_event(wlc, WLC_E_IF_DEL, scb->wds);
		wl_del_if(wlc->wl, scb->wds->wlif);
		scb->wds->wlif = NULL;
		SCB_DWDS_DEACTIVATE(scb);
	}
	wlc_wlcif_free(wlc, wlc->osh, scb->wds);
	scb->wds = NULL;
}

void
wlc_scb_wds_free(struct wlc_info *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->wds) {
			scb->permanent = FALSE;
			wlc_scbfree(wlc, scb);
		}
	}
}

static void
wlc_ap_wds_timeout(wlc_wds_info_t *mwds)
{
	wlc_info_t *wlc = mwds->wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_bsscfg_t *cfg = NULL;

	/* check wds link connectivity */
	if ((mwds->wdsactive && mwds->wds_timeout &&
	     ((wlc->pub->now % mwds->wds_timeout) == 0)) != TRUE)
		return;

	wlc->any_wds_peer_160mhz = FALSE;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		uint scb_activity_time;

		if (!SCB_LEGACY_WDS(scb))
			continue;

		cfg = SCB_BSSCFG(scb);
		if (!cfg->up) {
			/* Skip wds probe if the bsscfg is down */
			continue;
		}

		/* mark the WDS link up if we have had recent traffic,
		 * or probe the WDS link if we have not.
		 */
		if ((((scb_activity_time = wlc_ap_get_activity_time(wlc->ap)) != 0 &&
		     (wlc->pub->now - scb->used) >= scb_activity_time) ||
		    !(scb->flags & SCB_WDS_LINKUP)) && !(scb->flags & SCB_PENDING_PROBE)) {
			wlc_ap_wds_probe(mwds, scb);
		}
		/* Check for any 160Mhz peer */
		if ((scb->flags & SCB_WDS_LINKUP) && (scb->flags3 & SCB3_IS_160)) {
			wlc->any_wds_peer_160mhz = TRUE;
		}
	}
	wlc_update_phy_tdcs(wlc);
}

static int
wlc_ap_sendnulldata_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, void *data)
{
	/* register packet callback */
	WLF2_PCB1_REG(pkt, WLF2_PCB1_STA_PRB);
	return BCME_OK;
}

/* Send null packets to wds partner and check for response */
static void
wlc_ap_wds_probe(wlc_wds_info_t *mwds, struct scb *scb)
{
	wlc_info_t* wlc = mwds->wlc;
	ratespec_t rate_override;

	/* use the lowest basic rate */
	rate_override = wlc_lowest_basicrate_get(scb->bsscfg);

	ASSERT(VALID_RATE(wlc, rate_override));

	if (!wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, rate_override,
		SCB_PS_PRETEND(scb) ? WLF_PSDONTQ : 0,
		SCB_PS_PRETEND(scb) ? PRIO_8021D_VO : PRIO_8021D_BE,
		wlc_ap_sendnulldata_cb, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_sendnulldata failed\n",
		          wlc->pub->unit, __FUNCTION__));
	} else {
		scb->flags |= SCB_PENDING_PROBE;
	}
}

/*  Check for ack, if there is no ack, reset the rssi value */
void
wlc_ap_wds_probe_complete(wlc_info_t *wlc, uint txstatus, struct scb *scb)
{
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	uint scb_activity_time;

	ASSERT(scb != NULL);

	scb->flags &= ~(SCB_PENDING_PROBE | SCB_PSPRETEND_PROBE);

#ifdef PSPRETEND
	if (SCB_PS_PRETEND_PROBING(scb)) {
		scb->flags &= ~SCB_PSPRETEND_PROBE;
		if ((txstatus & TX_STATUS_MASK) == TX_STATUS_ACK_RCV) {
			/* probe response OK - exit PS Pretend state */
			WL_PS(("wl%d.%d: received ACK to wds ps pretend probe "MACF"\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			        ETHER_TO_MACF(scb->ea)));

			/* Assert check that the fifo was cleared before exiting ps mode */
			if (SCB_PS_PRETEND_BLOCKED(scb)) {
				WL_ERROR(("wl%d.%d: %s: SCB_PS_PRETEND_BLOCKED, "
				          "expected to see PMQ PPS entry\n", wlc->pub->unit,
				          WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__));
			}

#if defined(WL_PS_SCB_TXFIFO_BLK)
			if (SCB_PS_TXFIFO_BLK(scb) == FALSE)
#else /* ! WL_PS_SCB_TXFIFO_BLK */
			if (!(wlc->block_datafifo & DATA_BLOCK_PS))
#endif /* ! WL_PS_SCB_TXFIFO_BLK */
			{
				wlc_apps_scb_ps_off(wlc, scb, FALSE);
			} else {
				WL_ERROR(("wl%d.%d: %s: annother PS flush in progress, "
				          "unable to resume\n", wlc->pub->unit,
				          WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__));
			}
		} else {
			WL_PS(("wl%d.%d: no response to WDS ps pretend probe "MACF"\n",
			        wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
			        ETHER_TO_MACF(scb->ea)));

			/* we re-probe using ps pretend probe timer if not stalled,
			 * so return from here
			 */
			return;

		}
	}
#endif /* PSPRETEND */

	/* ack indicates the sta is there */
	if (txstatus & TX_STATUS_MASK) {
		if (!(scb->flags & SCB_WDS_LINKUP)) {
			scb->flags |= SCB_WDS_LINKUP;
#ifdef WL_HAPD_WDS
			wlc_wds_create_link_event(wlc, scb, TRUE);
#endif /* WL_HAPD_WDS */
#ifdef WLAMPDU
			/* Send delba and cleanup ampdu session when peer is back */
			if (SCB_AMPDU(scb)) {
				WL_INFORM(("wl%d: %s: scb ampdu cleanup for %s\n", wlc->pub->unit,
					__FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
				scb_ampdu_cleanup(wlc, scb);
			}
#endif /* WLAMPDU */
			WL_ERROR(("wl%d: %s: WDS link up\n", wlc->pub->unit,
				wl_ifname(wlc->wl, scb->wds->wlif)));
		}
		return;
	}

	WL_INFORM(("wl%d: %s: no ACK from %s for Null Data\n",
	           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));

	if (((scb_activity_time = wlc_ap_get_activity_time(wlc->ap)) != 0) &&
	     ((wlc->pub->now - scb->used) < scb_activity_time)) {
		WL_INFORM(("%s: There was recent traffic (%u sec) with %s so don't block link\n",
			__FUNCTION__, wlc->pub->now - scb->used, bcm_ether_ntoa(&scb->ea, eabuf)));
		return;
	}

#ifdef PSPRETEND
	if (SCB_PS_PRETEND(scb)) {
		/* We need to come out of PsPretend before marking link down */
#if defined(WL_PS_SCB_TXFIFO_BLK)
		if (SCB_PS_TXFIFO_BLK(scb) == FALSE)
#else /* ! WL_PS_SCB_TXFIFO_BLK */
		if (!(wlc->block_datafifo & DATA_BLOCK_PS))
#endif /* ! WL_PS_SCB_TXFIFO_BLK */
		{
			wlc_apps_scb_ps_off(wlc, scb, FALSE);
		} else {
			/* Don't mark link down yet */
			return;
		}
	}
#endif /* PSPRETEND */

	scb->flags &= ~SCB_WDS_LINKUP;

#ifdef WLAMPDU
	/* cleanup ampdu when peer is lost */
	if (SCB_AMPDU(scb)) {
		WL_INFORM(("wl%d: %s: scb ampdu cleanup for %s\n", wlc->pub->unit,
			__FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
		scb_ampdu_cleanup(wlc, scb);
	}
#endif /* WLAMPDU */
}

/*
 * Determine who is WPA supplicant and who is WPA authenticator over a WDS link.
 * The one that has the lower MAC address in numeric value is supplicant (802.11i D5.0).
 */
static int
wlc_wds_wpa_role_set(wlc_wds_info_t *mwds, struct scb *scb, uint8 role)
{
	wlc_info_t *wlc = mwds->wlc;

	switch (role) {
	/* auto, based on mac address value, lower is supplicant */
	case WL_WDS_WPA_ROLE_AUTO:
		if (bcmp(wlc->pub->cur_etheraddr.octet, scb->ea.octet, ETHER_ADDR_LEN) > 0)
			scb->flags |= SCB_WPA_SUP;
		else
			scb->flags &= ~SCB_WPA_SUP;
		break;
	/* local is supplicant, remote is authenticator */
	case WL_WDS_WPA_ROLE_SUP:
		scb->flags &= ~SCB_WPA_SUP;
		break;
	/* local is authenticator, remote is supplicant */
	case WL_WDS_WPA_ROLE_AUTH:
		scb->flags |= SCB_WPA_SUP;
		break;
	/* invalid roles */
	default:
		WL_ERROR(("wl%d: invalid WPA role %u\n", wlc->pub->unit, role));
		return BCME_BADARG;
	}
	return 0;
}

/*
 * Set 'role' to WL_WDS_WPA_ROLE_AUTH if the remote end of the WDS link identified by
 * the given mac address is WPA supplicant; set 'role' to WL_WDS_WPA_ROLE_SUP otherwise.
 */
static int
wlc_wds_wpa_role_get(wlc_wds_info_t *mwds, wlc_bsscfg_t *cfg, struct ether_addr *ea, uint8 *role)
{
	wlc_info_t *wlc = mwds->wlc;
	struct scb *scb;

	if (!(scb = wlc_scbfind(wlc, cfg, ea))) {
		WL_ERROR(("wl%d: failed to find SCB for %02x:%02x:%02x:%02x:%02x:%02x\n",
			wlc->pub->unit, ea->octet[0], ea->octet[1],
			ea->octet[2], ea->octet[3], ea->octet[4], ea->octet[5]));
		return BCME_NOTFOUND;
	}
	*role = SCB_LEGACY_WDS(scb) && (scb->flags & SCB_WPA_SUP) ?
		WL_WDS_WPA_ROLE_AUTH : WL_WDS_WPA_ROLE_SUP;
	return 0;
}

int
wlc_wds_create_link_event(wlc_info_t *wlc, struct scb *scb, bool isup)
{
	wlc_event_t *e;

#ifdef WL_HAPD_WDS
	if (SCB_LEGACY_WDS(scb) && isup && (scb->flags & SCB_WDS_LINKUP)) {
		if (scb->wds->link_event)
			return 0;
		else
			scb->wds->link_event = TRUE;
	}
#endif // endif
	/* create WDS LINK event */
	e = wlc_event_alloc(wlc->eventq, WLC_E_LINK);
	if (e == NULL) {
		WL_ERROR(("wl%d: wlc_wds_create wlc_event_alloc failed\n", wlc->pub->unit));
		return BCME_NOMEM;
	}

	e->event.event_type = WLC_E_LINK;
	e->event.flags = isup ? WLC_EVENT_MSG_LINK : 0;

	wlc_event_if(wlc, SCB_BSSCFG(scb), e, &scb->ea);

	wlc_process_event(wlc, e);

	return 0;
}

bool
wlc_wds_lazywds_is_enable(wlc_wds_info_t *mwds)
{
	if (mwds && mwds->lazywds)
		return TRUE;
	else
		return FALSE;
}

static int
wlc_wds_bcn_parse_wme_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	struct scb *scb = data->pparm->ft->bcn.scb;

	if (scb == NULL || !SCB_LEGACY_WDS(scb))
		return BCME_OK;

	if (data->ie != NULL) {
		scb->flags |= SCB_WMECAP;
	}
	else {
		scb->flags &= ~SCB_WMECAP;
	}

	return BCME_OK;
}

bool
wlc_wds_is_active(wlc_info_t *wlc)
{
	struct scb_iter scbiter;
	struct scb *scb;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_WDS(scb) && (scb->flags & SCB_WDS_LINKUP)) {
			return TRUE;
		}
	}
	return FALSE;
}

#ifdef DWDS
static int
wlc_dwds_config(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_dwds_config_t *dwds)
{
	struct scb *scb = NULL;
	int idx;
	wlc_bsscfg_t *cfg = NULL;
	struct ether_addr *peer;
#ifdef BCMDBG_ERR
	char addr[32];
#endif // endif
	/* use mode not bsscfg since the wds create
	 * ioctl is issued on the main interface.  if we are a first hop extender our main
	 * interface is in sta mode and we end up looking up the wrong peer
	 */
	if (dwds->mode)
		peer = &bsscfg->current_bss->BSSID;
	else
		peer = &dwds->ea;

	if (ETHER_ISNULLADDR(peer))
		return (BCME_BADADDR);

	/* request for wds interface comes from primary interface even though
	 * scb might be associated to another bsscfg. so need to search for
	 * scb across bsscfgs.
	 */
	FOREACH_BSS(wlc, idx, cfg) {
		/* Find the scb matching peer mac */
		if ((scb = wlc_scbfind(wlc, cfg, peer)) != NULL)
			break;
	}

	if ((scb == NULL) || (cfg == NULL)) {
		WL_ERROR(("wl%d: %s: no scb/bsscfg found for %s \n",
		           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(peer, addr)));
		return (BCME_BADARG);
	}

	if (dwds->enable) {
		if (BSSCFG_AP(cfg)) {
			wlc_wds_create(wlc, scb, WDS_DYNAMIC);
		}
		/* make this scb to do 4-addr data frame from now */
		SCB_A4_DATA_ENABLE(scb);
		SCB_DWDS_ACTIVATE(scb);
		wlc_mctrl(wlc, MCTL_PROMISC, 0);
	} else {
		/* free WDS state */
		if (scb->wds != NULL) {
			/* process event queue */
			wlc_eventq_flush(wlc->eventq);
			if (scb->wds->wlif) {
				wlc_update_wds_index(scb->bsscfg, scb->wds->wlif, FALSE);
				wlc_if_event(wlc, WLC_E_IF_DEL, scb->wds);
				wl_del_if(wlc->wl, scb->wds->wlif);
				scb->wds->wlif = NULL;
			}
			wlc_wlcif_free(wlc, wlc->osh, scb->wds);
			scb->wds = NULL;
		}
		SCB_A4_DATA_DISABLE(scb);
		SCB_DWDS_DEACTIVATE(scb);
	}

	return (0);
}

#ifdef DPSTA
/* Downstream client lookup */
struct scb *
wlc_dwds_client_is_ds_sta(wlc_info_t *wlc, struct ether_addr *mac)
{
	uint32 idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	FOREACH_UP_AP(wlc, idx, cfg) {
		scb = wlc_scbfind(wlc, cfg, mac);
		if (scb != NULL)
			break;
	}

	return scb;
}

/* See if the downstream client is associated and authorized */
bool
wlc_dwds_is_ds_sta(wlc_info_t *wlc, struct ether_addr *mac)
{
	struct scb *scb;
	wlc_bsscfg_t *cfg;
	bool ret = FALSE;

	scb = wlc_dwds_client_is_ds_sta(wlc, mac);
	if (scb == NULL)
		return FALSE;

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* AP is down */
	if (!wlc_bss_connected(cfg))
		return FALSE;

	ret = (cfg->WPA_auth != WPA_AUTH_DISABLED &&
		WSEC_ENABLED(cfg->wsec)) ? SCB_AUTHORIZED(scb) : SCB_ASSOCIATED(scb);

	return ret;
}

bool
wlc_dwds_authorized(wlc_bsscfg_t *cfg)
{
	return (wlc_bss_connected(cfg) && WLC_PORTOPEN(cfg));
}
#endif /* DPSTA */

#if defined(DPSTA) || defined(DWDS)
static void
wlc_dwds_mode_enable(wlc_info_t *wlc, struct scb *scb, wlc_bsscfg_t *bsscfg, bool enable)
{
	wl_dpsta_intf_event_t dpsta_prim_event;

	WL_PSTA(("wl%d: %s: enable=%d\n", wlc->pub->unit, __FUNCTION__, enable));

	if (enable == TRUE) {
		/* Send WLC_E_DWDS_INTF_IND event for dwds register */
		if (scb) {
			dpsta_prim_event.intf_type = WL_INTF_DWDS;
			WL_ASSOC(("wl%d.%d scb:"MACF"\n", wlc->pub->unit,
				WLC_BSSCFG_IDX(bsscfg), ETHER_TO_MACF(scb->ea)));

			wlc_bss_mac_event(wlc, bsscfg, WLC_E_DPSTA_INTF_IND,
				&scb->ea, WLC_E_STATUS_SUCCESS, 0, 0,
				&dpsta_prim_event, sizeof(wl_dpsta_intf_event_t));
		}
	}
}
#endif /* defined (DPSTA) || defined (DWDS) */

/* enable during associaton state complete via scb state notif */
static void
wlc_dwds_scb_state_upd(void *ctx, scb_state_upd_data_t *data)
{
	struct scb *scb = data->scb;
	wlc_bsscfg_t *cfg;
	wlc_info_t *wlc = (wlc_info_t *)ctx;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	if (!((DWDS_ENAB(cfg) && SCB_DWDS_CAP(scb)) ||
		(MAP_ENAB(cfg) && SCB_MAP_CAP(scb)))) {
		/* Not DWDS and MAP capable */
		return;
	}

	/* hndl transition from unassoc to assoc */
	if (!(data->oldstate & ASSOCIATED) && SCB_ASSOCIATED(scb)) {
		/* Enable dwds_mode if this is a dwds sta */
		if (BSSCFG_STA(cfg)) {
#if defined(DPSTA) || defined(DWDS)
			wlc_dwds_mode_enable(wlc, scb, cfg, TRUE);
#endif /* defined (DPSTA) || defined (DWDS) */
			/*
			 * make this scb to do 4-addr data frame from now.
			 * SCB_DWDS_CAP being TRUE implies that DWDS is enabled.
			 */
			SCB_A4_DATA_ENABLE(scb);
			SCB_DWDS_ACTIVATE(scb);
		}
		/* create dwds i/f for this scb:
		 *  a) if security is disabled;
		 *  b) if security is enabled and security type is WEP;
		 * For all other security types will create the interface
		 * once scb is authorized.
		 */
		else if (BSSCFG_AP(cfg) &&
		         (!WSEC_ENABLED(cfg->wsec) || WSEC_WEP_ENABLED(cfg->wsec))) {
			wlc_wds_create(wlc, scb, WDS_DYNAMIC);
		}
	}

	/* hndl transition from auth to deauth */
	if (BSSCFG_AP(cfg) &&
		(data->oldstate & AUTHENTICATED) && !(data->oldstate & ASSOCIATED) &&
		!SCB_ASSOCIATED(scb) && !SCB_AUTHENTICATED(scb)) {

		/* Flushing all the queues before interface free */
		wlc_scb_flush_queues(wlc, cfg, scb);
		wlc_wds_free_if(wlc, scb);
	}
}

static int
wlc_dwds_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	struct scb *scb;
	brcm_ie_t *brcm_ie;

	if (data->ie == NULL)
		return BCME_OK;

	if (data->ie_len <= OFFSETOF(brcm_ie_t, flags1))
		return BCME_OK;

	scb = wlc_iem_parse_get_assoc_bcn_scb(data);
	if (scb == NULL)
		return BCME_OK;

	brcm_ie = (brcm_ie_t *)data->ie;
	if (brcm_ie->flags1 & BRF1_DWDS) {
		scb->flags3 |= SCB3_DWDS_CAP;
	} else {
#ifdef PSTA
		wlc_wds_info_t *mwds = (wlc_wds_info_t *)ctx;
		wlc_info_t *wlc = mwds->wlc;
		/*
		 * In case if driver has been dynamically switched
		 * to DWDS mode from one of the PSTA mode then the
		 * function below will restore that particulate PSTA
		 * mode back.
		 */
		wlc_psta_mode_update(wlc->psta, data->cfg, NULL,
		                     PSTA_MODE_UPDATE_ACTION_RESTORE);
#endif /* PSTA */
		scb->flags3 &= ~SCB3_DWDS_CAP;
	}

	return BCME_OK;
}
#endif /* DWDS */

#ifdef WL_HAPD_WDS
bool
wlc_wds_is_legacy_wds_sta(wlc_info_t *wlc, struct ether_addr *remote_mac)
{
	struct scb *remote_scb;
	bool ret = TRUE;

	remote_scb = wlc_scbfind(wlc, wlc->primary_bsscfg, remote_mac);

	if (remote_scb == NULL)
		return FALSE;

	/* Return TRUE if this AP is supplicant else return FALSE
	 * To check if this is sta check if remote is supplicant,
	 * if remote supplicant then return FALSE indicating authenticator
	 */
	if ((SCB_LEGACY_WDS(remote_scb)) && (remote_scb->flags & SCB_WPA_SUP))
		ret = FALSE;

	return ret;
}

void
wlc_wds_reset_link_flag(wlc_info_t *wlc, struct scb *scb)
{
	if (SCB_LEGACY_WDS(scb)) {
		scb->wds->link_event = FALSE;
		scb->flags &= ~SCB_WDS_LINKUP;
	}
	return;
}

bool
wlc_wds_check_association(wlc_info_t *wlc, struct scb *scb)
{
	wlc_bsscfg_t *cfg;
	bool ret = FALSE;
	uint8 role;

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* If remote is supplicant this is authenticator */
	role = SCB_LEGACY_WDS(scb) && (scb->flags & SCB_WPA_SUP) ?
		WL_WDS_WPA_ROLE_AUTH : WL_WDS_WPA_ROLE_SUP;

	if (role == WL_WDS_WPA_ROLE_SUP)
		return FALSE;

	if (cfg->WPA_auth != WPA_AUTH_DISABLED && WSEC_ENABLED(cfg->wsec) && !SCB_AUTHORIZED(scb)) {
		wlc_wds_reset_link_flag(wlc, scb);
		ret = TRUE;
	}
	return ret;
}
#endif /* WL_HAPD_WDS */

#ifdef WLCSA
void
wlc_wds_process_csa(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wl_chan_switch_t *csa)
{
	wlc_csa_info_t *csam = wlc->csa;
	wlc_bsscfg_t *apcfg;
	int idx;

	if (wlc_dfs_scan_in_progress(wlc->dfs)) {
		return;
	}

	if (wf_chspec_malformed(csa->chspec)) {
		return;
	}

	FOREACH_UP_AP(wlc, idx, apcfg) {
		uint spect_state = wlc_11h_get_spect_state(wlc->m11h, apcfg);
		chanspec_t chspec = wlc_csa_get_csa_chspec(csam, apcfg);
		uint8 run_count = wlc_csa_get_csa_count(csam, apcfg);

		/* If channel switch is in progress and no change in requested chanspec or
		 * channel switch count, ignore CSA request.
		 */
		if ((spect_state & NEED_TO_SWITCH_CHANNEL) && (chspec == csa->chspec) &&
			(run_count <= (csa->count + 2))) {
			WL_REGULATORY(("wl%d.%d: %s : Already have a scheduled channel switch!\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			break;
		}
		wlc_csa_do_channel_switch(csam, apcfg, csa->chspec, csa->mode, csa->count,
		       csa->reg, CSA_BROADCAST_ACTION_FRAME);
	}
	return;
}
#endif /* WLCSA */

#ifdef MULTIAP
/* VS: Multi-AP IE */
static uint
wlc_wds_calc_multiap_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_iem_ft_cbparm_t *ftcbparm = NULL;
	struct scb *scb = NULL;
	uint len = 0;

	if (!MAP_ENAB(cfg)) {
		return len;
	}

	switch (data->ft) {
		case FC_ASSOC_RESP:
		case FC_REASSOC_RESP:
			ASSERT(data->cbparm != NULL);
			ftcbparm = data->cbparm->ft;
			ASSERT(ftcbparm != NULL);
			scb = ftcbparm->assocresp.scb;
			ASSERT(scb != NULL);
			break;
	}

	if (scb && !SCB_MAP_CAP(scb)) {
		return len;
	}

	if (cfg->multiap_ie[TLV_LEN_OFF] > 0) {
		len = TLV_HDR_LEN + cfg->multiap_ie[TLV_LEN_OFF];
	}

	return len;
}

/* VS: Multi-AP IE */
static int
wlc_wds_write_multiap_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_iem_ft_cbparm_t *ftcbparm = NULL;
	struct scb *scb = NULL;

	if (!MAP_ENAB(cfg)) {
		return BCME_OK;
	}

	switch (data->ft) {
		case FC_ASSOC_RESP:
		case FC_REASSOC_RESP:
			ASSERT(data->cbparm != NULL);
			ftcbparm = data->cbparm->ft;
			ASSERT(ftcbparm != NULL);
			scb = ftcbparm->assocresp.scb;
			ASSERT(scb != NULL);
			break;
	}

	if (scb && !SCB_MAP_CAP(scb)) {
		return BCME_OK;
	}

	if (cfg->multiap_ie[TLV_LEN_OFF] > 0) {
		bcm_copy_tlv(cfg->multiap_ie, data->buf);
	}

	return BCME_OK;
}

/* VS: Multi-AP IE */
static int
wlc_wds_parse_multiap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm;
	struct scb *scb = NULL;

	switch (data->ft) {
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			ASSERT(data->pparm != NULL);
			ftpparm = data->pparm->ft;
			ASSERT(ftpparm != NULL);
			scb = ftpparm->assocreq.scb;
			ASSERT(scb != NULL);
			break;
		case FC_ASSOC_RESP:
		case FC_REASSOC_RESP:
			ASSERT(data->pparm != NULL);
			ftpparm = data->pparm->ft;
			ASSERT(ftpparm != NULL);
			scb = ftpparm->assocresp.scb;
			ASSERT(scb != NULL);
			break;
	}

	if (scb != NULL && data->ie != NULL) {
		wlc_process_multiap_ie(wlc, scb, (multiap_ie_t *)data->ie, data->ft);
	}

	return BCME_OK;
}
#endif	/* MULTIAP */

#ifdef DWDS
dwds_sa_t *
wlc_dwds_findsa(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 *ea)
{
	uint32 hash;
	dwds_sa_t *sta;

	hash = DWDS_SA_HASH(ea);

	sta = cfg->sa_list[hash];
	while (sta != NULL) {
		if (EA_CMP(&sta->sa, ea)) {
			sta->last_used = wlc->pub->now;
			return sta;
		}
		sta = sta->next;
	}
	return NULL;
}

dwds_sa_t *
wlc_dwds_addsa(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 *ea)
{
	uint32 hash;
	dwds_sa_t *sta;

	if ((sta = MALLOC(wlc->osh, sizeof(dwds_sa_t))) == NULL) {
		WL_ERROR(("%s: MALLOC failure\n", __FUNCTION__));
		return NULL;
	}

	eacopy(ea, &sta->sa);
	sta->last_used = wlc->pub->now;

	hash = DWDS_SA_HASH(ea);
	sta->next = cfg->sa_list[hash];
	cfg->sa_list[hash] = sta;

	return sta;
}

void
wlc_dwds_expire_sa(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int hash = 0;
	dwds_sa_t *sta;
	dwds_sa_t *prev_sta;

	for (hash = 0; hash < DWDS_SA_HASH_SZ; hash++) {
		prev_sta = NULL;
		sta = cfg->sa_list[hash];

		while (sta != NULL) {
			/* If last used time is more than expire, knock it off the list */
			if ((wlc->pub->now - sta->last_used) > DWDS_SA_EXPIRE_TIME) {
				if (prev_sta == NULL) {
					cfg->sa_list[hash] = sta->next;
				} else {
					prev_sta->next = sta->next;
				}

				MFREE(wlc->osh, sta, sizeof(dwds_sa_t));
				sta = prev_sta ? prev_sta->next : cfg->sa_list[hash];
				continue;
			}

			prev_sta = sta;
			sta = sta->next;
		}
	}
}

void
wlc_dwds_flush_salist(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int hash = 0;
	dwds_sa_t *sta;
	dwds_sa_t *p;

	for (hash = 0; hash < DWDS_SA_HASH_SZ; hash++) {
		sta = cfg->sa_list[hash];
		while (sta != NULL) {
			p = sta->next;
			MFREE(wlc->osh, sta, sizeof(dwds_sa_t));
			sta = p;
		}
		cfg->sa_list[hash] = NULL;
	}
}

void
wlc_dwds_dump_sa_list(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	int hash = 0;
	dwds_sa_t *sta;
	char eabuf[ETHER_ADDR_STR_LEN];

	bcm_bprintf(b, "----- DWDS STA loop back list ---------\n", WLC_BSSCFG_IDX(cfg));
	for (hash = 0; hash < DWDS_SA_HASH_SZ; hash++) {
		sta = cfg->sa_list[hash];
		while (sta != NULL) {
			bcm_bprintf(b, "Hash: %02d, EA: %s, LastUsed: %d sec(s) back\n",
				hash, bcm_ether_ntoa(&sta->sa, eabuf),
				wlc->pub->now - sta->last_used);
			sta = sta->next;
		}
	}
}
#endif /* DWDS */
void
wlc_dwds_scb_switch_queue(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_txq_info_t *new_q)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		if (SCB_ASSOCIATED(scb) && SCB_DWDS(scb) && SCB_WDS(scb)) {
			scb->wds->qi = new_q;
		}
	}
}
/* Add/del the  wds index for a wds interface add/rem */
int wlc_update_wds_index(wlc_bsscfg_t *cfg, struct wl_if *wlif, bool op)
{
	int index = 0;

	if (!wlif || !cfg)
		return BCME_ERROR;

	index = op ? wlc_get_free_wds_index(cfg) : (wlc_get_wlif_wdsindex(wlif) - 1);

	if (index < 0) {
		WL_ERROR(("%s wrong wds Index - %d\n", __FUNCTION__, index));
		return index;
	}

	if (op == TRUE) {
		setbit((void *)&cfg->wds_bitmap, index);
	} else {
		clrbit((void *)&cfg->wds_bitmap, index);
	}

	WL_INFORM(("%s: wds index = %d\n", __FUNCTION__, index));

	return (index + 1);
}
/* Get the least available free wds index */
static int wlc_get_free_wds_index(wlc_bsscfg_t *cfg)
{
	uint8 index = 0;
	/* To get the free wds index for each interface, checking each bits of the wds_bitmap. */
#if (WLC_MAX_WDS_BMP > 0)
	if (cfg->wds_bitmap >= (uint32) WLC_MAX_WDS_BMP) {
		WL_ERROR(("%s: Max wds count Exceeded\n", __FUNCTION__));
		return BCME_ERROR;
	}
#else
	return -1;
#endif // endif

	while (isset((void *)&cfg->wds_bitmap, index))
		index++;
	return (index);
}
#endif /* WDS */
