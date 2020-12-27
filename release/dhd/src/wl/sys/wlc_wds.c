/*
 * Dynamic WDS module source file
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_wds.c 467328 2014-04-03 01:23:40Z $
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

/* IOVar table */
enum {
	IOV_WDS_WPA_ROLE,
	IOV_WDSTIMEOUT,
	IOV_WDS_ENABLE,	/* enable/disable wds link events */
#ifdef DWDS
	IOV_DWDS,
	IOV_DWDS_CONFIG,
#endif
	IOV_WDS_TYPE,
	IOV_LAST
};

static const bcm_iovar_t wlc_wds_iovars[] = {
	{"wds_wpa_role", IOV_WDS_WPA_ROLE,
	(IOVF_SET_UP), IOVT_BUFFER, ETHER_ADDR_LEN+1
	},
	{"wdstimeout", IOV_WDSTIMEOUT,
	(IOVF_WHL), IOVT_UINT32, 0
	},
	{"wds_enable", IOV_WDS_ENABLE,
	(IOVF_SET_UP), IOVT_BOOL, 0
	},
#ifdef DWDS
	{"dwds", IOV_DWDS, (IOVF_SET_DOWN), IOVT_BOOL, 0},
	{"dwds_config", IOV_DWDS_CONFIG, (IOVF_SET_UP), IOVT_BUFFER, sizeof(wlc_dwds_config_t)},
#endif
	{"wds_type", IOV_WDS_TYPE,
	(0), IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0}
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
static int wlc_wds_create_link_event(wlc_wds_info_t *mwds, struct scb *scb);
static void wlc_ap_wds_timeout(wlc_wds_info_t *mwds);
static void wlc_ap_wds_probe(wlc_wds_info_t *mwds, struct scb *scb);
#ifdef DWDS
static int wlc_dwds_config(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_dwds_config_t *dwds);
#endif

#ifdef BCMDBG
static int wlc_dump_wds(wlc_wds_info_t *mwds, struct bcmstrbuf *b);
#endif /* BCMDBG */

/* module */
static int wlc_wds_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_wds_ioctl(void *hdl, int cmd, void *arg, int len, struct wlc_if *wlcif);

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
#endif

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

	return mwds;

	/* error handling */
fail:
	wlc_wds_detach(mwds);
	return NULL;
}

void
BCMATTACHFN(wlc_wds_detach)(wlc_wds_info_t *mwds)
{
	wlc_info_t *wlc;

	if (mwds == NULL)
		return;

	wlc = mwds->wlc;

	wlc_module_unregister(wlc->pub, "wds", mwds);

	wlc_module_remove_ioctl_fn(wlc->pub, (void *)mwds);

	MFREE(wlc->osh, mwds, sizeof(wlc_wds_info_t));
}

static int
wlc_wds_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
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
		err = wlc_wds_create_link_event(mwds, wlcif->u.scb);
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

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

exit:
	return err;
}

static int
wlc_wds_ioctl(void *hdl, int cmd, void *arg, int len, struct wlc_if *wlcif)
{
	wlc_wds_info_t *mwds = (wlc_wds_info_t *) hdl;
	wlc_info_t *wlc = mwds->wlc;
	int val = 0, *pval;
	bool bool_val;
	int bcmerror = 0;
	uint i;
	struct maclist *maclist;
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

		if (len < (int)(OFFSETOF(struct maclist, ea) + maclist->count * ETHER_ADDR_LEN)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}

		if (WIN7_AND_UP_OS(wlc->pub)) {
			bcmerror = BCME_UNSUPPORTED;
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
			if (scb->wds && !scb->permanent)
				wlc_scbfree(wlc, scb);
		}

#ifdef STA
		wlc_radio_mpc_upd(wlc);
#endif

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
		maclist = (struct maclist *) arg;
		ASSERT(maclist);
		/* count WDS stations */
		val = 0;
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (scb->wds)
				val++;
		}
		if (maclist->count < (uint)val) {
			bcmerror = BCME_RANGE;
			break;
		}
		if (len < ((int)maclist->count - 1)* (int)sizeof(struct ether_addr)
			+ (int)sizeof(struct maclist)) {
			bcmerror = BCME_BUFTOOSHORT;
			break;
		}
		maclist->count = 0;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (scb->wds)
				bcopy((void*)&scb->ea, (void*)&maclist->ea[maclist->count++],
					ETHER_ADDR_LEN);
		}
		ASSERT(maclist->count == (uint)val);
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
	ASSERT(scb != NULL);

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
	scb->wds = wlc_wlcif_alloc(wlc, wlc->osh, WLC_IFTYPE_WDS, wlc->active_queue);
	if (scb->wds == NULL) {
		WL_ERROR(("wl%d: wlc_wds_create: failed to alloc wlcif\n",
		          wlc->pub->unit));
		return BCME_NOMEM;
	}
	scb->wds->u.scb = scb;

#ifdef AP
	/* create an upper-edge interface */
	if (!(flags & WDS_INFRA_BSS)) {
		/* a WDS scb has an AID for a unique WDS interface unit number */
		if (scb->aid == 0)
			scb->aid = wlc_bsscfg_newaid(scb->bsscfg);
		scb->wds->wlif = wl_add_if(wlc->wl, scb->wds, AID2PVBMAP(scb->aid), &scb->ea);
		if (scb->wds->wlif == NULL) {
			MFREE(wlc->osh, scb->wds, sizeof(wlc_if_t));
			scb->wds = NULL;
			return BCME_NOMEM;
		}
		scb->bsscfg->wlcif->if_flags |= WLC_IF_LINKED;
		wlc_if_event(wlc, WLC_E_IF_ADD, scb->wds);
	}

	wlc_wds_wpa_role_set(wlc->mwds, scb, WL_WDS_WPA_ROLE_AUTO);
#endif /* AP */

	/* Dont do this for DWDS. */
	if (!(flags & WDS_DYNAMIC)) {
		/* override WDS nodes rates to the full hw rate set */
		wlc_rateset_filter(&wlc->band->hw_rateset, &scb->rateset, FALSE,
			WLC_RATES_CCK_OFDM, RATE_MASK, wlc_get_mcsallow(wlc, scb->bsscfg));
		wlc_scb_ratesel_init(wlc, scb);

		scb->permanent = TRUE;
		scb->flags &= ~SCB_MYAP;

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

	/* check wds link connectivity */
	if ((mwds->wdsactive && mwds->wds_timeout &&
	     ((wlc->pub->now % mwds->wds_timeout) == 0)) != TRUE)
		return;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_LEGACY_WDS(scb)) {
			/* mark the WDS link up if we have had recent traffic,
			 * or probe the WDS link if we have not.
			 */
			if ((wlc->ap->scb_activity_time && (wlc->pub->now - scb->used) >=
			     wlc->ap->scb_activity_time) ||
			    !(scb->flags & SCB_WDS_LINKUP))
				wlc_ap_wds_probe(mwds, scb);
		}
	}
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
	uint8 rate_override;

	/* use the lowest basic rate */
	rate_override = wlc_lowest_basicrate_get(scb->bsscfg);

	ASSERT(VALID_RATE(wlc, rate_override));

	if (!wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, rate_override,
		SCB_PS_PRETEND(scb) ? WLF_PSDONTQ : 0, PRIO_8021D_BE,
		wlc_ap_sendnulldata_cb, NULL))
		WL_ERROR(("wl%d: %s: wlc_sendnulldata failed\n",
		          wlc->pub->unit, __FUNCTION__));
}

/*  Check for ack, if there is no ack, reset the rssi value */
void
wlc_ap_wds_probe_complete(wlc_info_t *wlc, uint txstatus, struct scb *scb)
{
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	ASSERT(scb != NULL);

	/* ack indicates the sta is there */
	if (txstatus & TX_STATUS_MASK) {
		scb->flags |= SCB_WDS_LINKUP;
		return;
	}


	WL_INFORM(("wl%d: %s: no ACK from %s for Null Data\n",
	           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));

	scb->flags &= ~SCB_WDS_LINKUP;
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

static int
wlc_wds_create_link_event(wlc_wds_info_t *mwds, struct scb *scb)
{
	wlc_info_t *wlc = mwds->wlc;
	wlc_event_t *e;

	/* create WDS LINK event */
	e = wlc_event_alloc(wlc->eventq);
	if (e == NULL) {
		WL_ERROR(("wl%d: wlc_wds_create wlc_event_alloc failed\n", wlc->pub->unit));
		return BCME_NOMEM;
	}

	e->event.event_type = WLC_E_LINK;
	e->event.flags = WLC_EVENT_MSG_LINK;

	wlc_event_if(wlc, SCB_BSSCFG(scb), e, &scb->ea);

	wlc_eventq_enq(wlc->eventq, e);

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
#endif
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
		if (BSSCFG_AP(cfg))
			wlc_wds_create(wlc, scb, WDS_DYNAMIC);

		/* make this scb to do 4-addr data frame from now */
		SCB_A4_DATA_ENABLE(scb);
		SCB_DWDS_ACTIVATE(scb);
		wlc_mctrl(wlc, MCTL_PROMISC, 0);
	} else {
		/* free WDS state */
		if (scb->wds != NULL) {
			if (scb->wds->wlif) {
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
#endif /* DWDS */
#endif /* WDS */
