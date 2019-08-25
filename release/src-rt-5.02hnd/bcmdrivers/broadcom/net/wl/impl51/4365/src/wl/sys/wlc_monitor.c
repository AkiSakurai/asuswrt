/*
 * Monitor modules implementation
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: wlc_monitor.c 527992 2015-01-21 00:08:30Z $
 */


#include <wlc_cfg.h>
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
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_monitor.h>
#include <wlc_bsscfg.h>
#include <wl_export.h>


#if defined(PHYCAL_CACHING)
#include <wlc_phy_hal.h>
#endif

/* default promisc bits */
#if defined(MACOSX)
#define WL_MONITOR_PROMISC_BITS_DEF \
	(WL_MONPROMISC_PROMISC | WL_MONPROMISC_CTRL |WL_MONPROMISC_FCS)
#else
#define WL_MONITOR_PROMISC_BITS_DEF \
	(WL_MONPROMISC_PROMISC | WL_MONPROMISC_CTRL)
#endif /* MACOSX */

/* wlc access macros */
#define WLCUNIT(x) ((x)->wlc->pub->unit)
#define WLCPUB(x) ((x)->wlc->pub)
#define WLCOSH(x) ((x)->wlc->osh)
#define WLC(x) ((x)->wlc)

struct wlc_monitor_info {
	wlc_info_t *wlc;
	uint32 promisc_bits; /* monitor promiscuity bitmap */
	chanspec_t chanspec;
	bool timer_active;
	struct wl_timer * mon_cal_timer;
};

/* IOVar table */
enum {
	IOV_MONITOR_PROMISC_LEVEL = 0,
	IOV_LAST
};

static const bcm_iovar_t monitor_iovars[] = {
	{"monitor_promisc_level", IOV_MONITOR_PROMISC_LEVEL,
	(0), IOVT_UINT32, 0
	},

	{NULL, 0, 0, 0, 0 }
};

/* **** Private Functions Prototypes *** */

/* Forward declarations for functions registered for this module */
static int wlc_monitor_doiovar(
		    void                *hdl,
		    const bcm_iovar_t   *vi,
		    uint32              actionid,
		    const char          *name,
		    void                *p,
		    uint                plen,
		    void                *a,
		    int                 alen,
		    int                 vsize,
		    struct wlc_if       *wlcif);

static void
wlc_monitor_phy_cal_timer(void *arg)
{
	wlc_info_t * wlc = (wlc_info_t *)arg;
	wlc_monitor_info_t * ctxt = wlc->mon_info;

	if (wlc->chanspec == ctxt->chanspec) {
		wlc_clr_quiet_chanspec(wlc->cmi, wlc->chanspec);
		wlc_mute(wlc, OFF, 0);
		wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_JOIN_BSS);
	}
	ctxt->timer_active = FALSE;
}

static int
wlc_monitor_down(void *context)
{
	wlc_monitor_info_t * mon_info = (wlc_monitor_info_t *)context;

	if (mon_info->timer_active) {
		wl_del_timer(WLC(mon_info)->wl, mon_info->mon_cal_timer);
		mon_info->timer_active = FALSE;
	}

	return 0;
}

/* **** Public Functions *** */
/*
 * Initialize the sta monitor private context and resources.
 * Returns a pointer to the sta monitor private context, NULL on failure.
 */
wlc_monitor_info_t *
BCMATTACHFN(wlc_monitor_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	wlc_monitor_info_t *ctxt;

	ctxt = (wlc_monitor_info_t*)MALLOCZ(pub->osh, sizeof(wlc_monitor_info_t));
	if (ctxt == NULL) {
		WL_ERROR(("wl%d: %s: ctxt MALLOCZ failed; total mallocs %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	ctxt->wlc = wlc;
	ctxt->promisc_bits = 0;
	ctxt->mon_cal_timer = wl_init_timer(wlc->wl, wlc_monitor_phy_cal_timer, wlc, "monitor");
	ctxt->chanspec = 0;
	ctxt->timer_active = FALSE;

	/* register module */
	if (wlc_module_register(
			    wlc->pub,
			    monitor_iovars,
			    "monitor",
			    ctxt,
			    wlc_monitor_doiovar,
			    NULL,
			    NULL,
			    wlc_monitor_down)) {
				WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
				    WLCUNIT(ctxt),
				    __FUNCTION__));

				goto fail;
			    }

	return ctxt;

fail:
	MFREE(pub->osh, ctxt, sizeof(wlc_monitor_info_t));

	return NULL;
}

/*
 * Release net detect private context and resources.
 */
void
BCMATTACHFN(wlc_monitor_detach)(wlc_monitor_info_t *ctxt)
{
	if (ctxt != NULL) {
		if (ctxt->mon_cal_timer) {
			wl_free_timer(WLC(ctxt)->wl, ctxt->mon_cal_timer);
			ctxt->mon_cal_timer = NULL;
			ctxt->timer_active = FALSE;
		}

		/* Unregister the module */
		wlc_module_unregister(WLCPUB(ctxt), "monitor", ctxt);
		/* Free the all context memory */
	    MFREE(WLCOSH(ctxt), ctxt, sizeof(wlc_monitor_info_t));
	}
}

void
wlc_monitor_phy_cal_timer_start(wlc_monitor_info_t *ctxt, uint32 tm)
{
	wlc_info_t * wlc = WLC(ctxt);

	if (ctxt->timer_active) {
		wl_del_timer(wlc->wl, ctxt->mon_cal_timer);
	}

	wl_add_timer(wlc->wl, ctxt->mon_cal_timer, tm, 0);
	ctxt->timer_active = TRUE;
}

void
wlc_monitor_phy_cal(wlc_monitor_info_t *ctxt, bool enable)
{
	wlc_info_t * wlc = WLC(ctxt);

#if defined(PHYCAL_CACHING)
	int idx;
	wlc_bsscfg_t *cfg;

	if (ctxt->chanspec == 0)
		goto skip_del_ctx;

	/* Delete previous cal ctx if any */
	FOREACH_BSS(wlc, idx, cfg)
	{
		if ((((BSSCFG_STA(cfg) && cfg->associated) || (BSSCFG_AP(cfg) && cfg->up)) &&
			(cfg->current_bss->chanspec == wlc->mon_info->chanspec))) {
			goto skip_del_ctx;
		}
	}

	wlc_phy_destroy_chanctx(WLC_PI(wlc), ctxt->chanspec);

skip_del_ctx:

#endif /* PHYCAL_CACHING */

	wl_del_timer(wlc->wl, ctxt->mon_cal_timer);
	ctxt->timer_active = FALSE;
	ctxt->chanspec = 0;

	if (enable) {

		/* No need to calibrate the channel when any connections are active */
		if ((wlc->stas_connected != 0) || (wlc->aps_associated != 0)) {
			return;
		}

		ctxt->chanspec = wlc->chanspec;

#if defined(PHYCAL_CACHING)
		wlc_phy_create_chanctx(WLC_PI(wlc), ctxt->chanspec);
#endif /* PHYCAL_CACHING */

		wlc_phy_cal_perical(WLC_PI(wlc), PHY_PERICAL_JOIN_BSS);
	}

	return;
}

void
wlc_monitor_promisc_enable(wlc_monitor_info_t *ctxt, bool enab)
{
	if (ctxt == NULL)
		return;

	/* Add/remove corresponding promisc bits if monitor is enabled/disabled. */
	if (enab)
		ctxt->promisc_bits |= WL_MONITOR_PROMISC_BITS_DEF;
	else
		ctxt->promisc_bits &= ~WL_MONITOR_PROMISC_BITS_DEF;

	wlc_monitor_phy_cal(ctxt, enab);

	wlc_mac_bcn_promisc(WLC(ctxt));
	wlc_mac_promisc(WLC(ctxt));
}

uint32
wlc_monitor_get_mctl_promisc_bits(wlc_monitor_info_t *ctxt)
{
	uint32 promisc_bits = 0;

	if (ctxt != NULL) {
		if (ctxt->promisc_bits & WL_MONPROMISC_PROMISC)
			promisc_bits |= MCTL_PROMISC;
		if (ctxt->promisc_bits & WL_MONPROMISC_CTRL)
			promisc_bits |= MCTL_KEEPCONTROL;
		if (ctxt->promisc_bits & WL_MONPROMISC_FCS)
			promisc_bits |= MCTL_KEEPBADFCS;
	}
	return promisc_bits;
}

/* **** Private Functions *** */

static int
wlc_monitor_doiovar(
	void                *hdl,
	const bcm_iovar_t   *vi,
	uint32              actionid,
	const char          *name,
	void                *p,
	uint                plen,
	void                *a,
	int                 alen,
	int                 vsize,
	struct wlc_if       *wlcif)
{
	wlc_monitor_info_t *ctxt = hdl;
	int32 *ret_int_ptr = (int32 *)a;
	int32 int_val = 0;
	int err = BCME_OK;

	BCM_REFERENCE(vi);
	BCM_REFERENCE(name);
	BCM_REFERENCE(vsize);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
	case IOV_GVAL(IOV_MONITOR_PROMISC_LEVEL):
		*ret_int_ptr = (int32)ctxt->promisc_bits;
		break;

	case IOV_SVAL(IOV_MONITOR_PROMISC_LEVEL):
		ctxt->promisc_bits = (uint32)int_val;
		if (MONITOR_ENAB(WLC(ctxt)))
			wlc_mac_promisc(WLC(ctxt));
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}
