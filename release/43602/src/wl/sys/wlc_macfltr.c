/*
 * mac filter module source file
 * Broadcom 802.11 Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_macfltr.h>

/* ioctl table */
static const wlc_ioctl_cmd_t wlc_macfltr_ioctls[] = {
	{WLC_GET_MACLIST, 0, 0},
	{WLC_SET_MACLIST, 0, sizeof(uint)},
	{WLC_GET_MACMODE, 0, sizeof(uint)},
	{WLC_SET_MACMODE, 0, sizeof(uint)}
};

/* module struct */
struct wlc_macfltr_info {
	wlc_info_t *wlc;
	int cfgh;
};

/* cubby struct and access macros */
typedef struct {
	/* MAC filter */
	int macmode;			/* allow/deny stations on maclist array */
	uint nmac;			/* # of entries on maclist array */
	struct ether_addr *maclist;	/* list of source MAC addrs to match */
} bss_macfltr_info_t;
#define BSS_MACFLTR_INFO(mfi, cfg) ((bss_macfltr_info_t *)BSSCFG_CUBBY(cfg, (mfi)->cfgh))

/* forward declaration */
/* ioctl */
static int wlc_macfltr_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif);
/* dump */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_macfltr_dump(void *ctx, struct bcmstrbuf *b);
#endif

/* cubby */
static void wlc_macfltr_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void wlc_macfltr_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_macfltr_bss_dump NULL
#endif

/* module entries */
wlc_macfltr_info_t *
BCMATTACHFN(wlc_macfltr_attach)(wlc_info_t *wlc)
{
	wlc_macfltr_info_t *mfi;

	ASSERT(OFFSETOF(wlc_macfltr_info_t, wlc) == 0);

	if ((mfi = MALLOCZ(wlc->osh, sizeof(wlc_macfltr_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	mfi->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((mfi->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(bss_macfltr_info_t),
	                NULL, wlc_macfltr_bss_deinit, wlc_macfltr_bss_dump,
	                (void *)mfi)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_add_ioctl_fn(wlc->pub, mfi, wlc_macfltr_doioctl,
	                ARRAYSIZE(wlc_macfltr_ioctls), wlc_macfltr_ioctls) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "macfltr", wlc_macfltr_dump, (void *)mfi);
#endif

	return mfi;

	/* error handling */
fail:
	wlc_macfltr_detach(mfi);
	return NULL;
}

void
BCMATTACHFN(wlc_macfltr_detach)(wlc_macfltr_info_t *mfi)
{
	wlc_info_t *wlc;

	if (mfi == NULL)
		return;

	wlc = mfi->wlc;

	wlc_module_remove_ioctl_fn(wlc->pub, mfi);

	MFREE(wlc->osh, mfi, sizeof(wlc_macfltr_info_t));
}

/* ioctl entry */
static int
wlc_macfltr_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif)
{
	wlc_macfltr_info_t *mfi = (wlc_macfltr_info_t *)ctx;
	wlc_info_t *wlc = mfi->wlc;
	wlc_bsscfg_t *cfg;
	bss_macfltr_info_t *bfi;
	int val, *pval;
	int err = BCME_OK;

	/* default argument is generic integer */
	pval = (int *)arg;

	/* This will prevent the misaligned access */
	if (pval != NULL && (uint)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));
	else
		val = 0;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	bfi = BSS_MACFLTR_INFO(mfi, cfg);
	ASSERT(bfi != NULL);

	switch (cmd) {
	case WLC_GET_MACLIST:
		err = wlc_macfltr_list_get(mfi, cfg, (struct maclist *)arg, (uint)len);
		break;

	case WLC_SET_MACLIST:
		err = wlc_macfltr_list_set(mfi, cfg, (struct maclist *)arg, (uint)len);
		break;

	case WLC_GET_MACMODE:
		*pval = bfi->macmode;
		break;

	case WLC_SET_MACMODE:
		WL_INFORM(("Setting mac mode to %d %s\n", val,
			val == 0  ? "disabled" : val == 1 ? "deny" : "allow"));
		bfi->macmode = val;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* debug... */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wlc_macfltr_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_macfltr_info_t *mfi = (wlc_macfltr_info_t *)ctx;
	wlc_info_t *wlc = mfi->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_BSS(wlc, idx, cfg) {
		bcm_bprintf(b, "bsscfg %d >\n", WLC_BSSCFG_IDX(cfg));
		wlc_macfltr_bss_dump(mfi, cfg, b);
	}

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP */

/* bsscfg cubby entries */
static void
wlc_macfltr_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_macfltr_info_t *mfi = (wlc_macfltr_info_t *)ctx;
	wlc_info_t *wlc = mfi->wlc;
	bss_macfltr_info_t *bfi;

	(void)wlc;

	ASSERT(cfg != NULL);

	bfi = BSS_MACFLTR_INFO(mfi, cfg);
	ASSERT(bfi != NULL);

	if (bfi->maclist != NULL) {
		MFREE(wlc->osh, bfi->maclist, OFFSETOF(struct maclist, ea) +
		                              bfi->nmac * ETHER_ADDR_LEN);
		bfi->maclist = NULL;
	}
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void
wlc_macfltr_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_macfltr_info_t *mfi = (wlc_macfltr_info_t *)ctx;
	bss_macfltr_info_t *bfi;
	int i;
	char eabuf[ETHER_ADDR_STR_LEN];

	ASSERT(cfg != NULL);

	bfi = BSS_MACFLTR_INFO(mfi, cfg);
	ASSERT(bfi != NULL);

	bcm_bprintf(b, "\tmacmode %d (%s)\n", bfi->macmode,
		bfi->macmode == 0  ? "disabled" : bfi->macmode == 1 ? "deny" : "allow");
	if (bfi->macmode != WLC_MACMODE_DISABLED) {
		bcm_bprintf(b, "\tnmac %d\n", bfi->nmac);
		for (i = 0; i < (int)bfi->nmac; i++)
			bcm_bprintf(b, "\t%s\n", bcm_ether_ntoa(&bfi->maclist[i], eabuf));
	}
}
#endif /* BCMDBG || BCMDBG_DUMP */

/* APIs */
int
wlc_macfltr_addr_match(wlc_macfltr_info_t *mfi, wlc_bsscfg_t *cfg,
	const struct ether_addr *addr)
{
	bss_macfltr_info_t *bfi;
	uint i;

	ASSERT(cfg != NULL);

	bfi = BSS_MACFLTR_INFO(mfi, cfg);
	ASSERT(bfi != NULL);

	if (bfi->macmode != WLC_MACMODE_DISABLED) {
		for (i = 0; i < bfi->nmac; i++) {
			if (bcmp(addr, &bfi->maclist[i], ETHER_ADDR_LEN) == 0)
				break;
		}
		if (i < bfi->nmac) {
			switch (bfi->macmode) {
			case WLC_MACMODE_DENY:
				return WLC_MACFLTR_ADDR_DENY;
			case WLC_MACMODE_ALLOW:
				return WLC_MACFLTR_ADDR_ALLOW;
			default:
				ASSERT(0);
				break;
			}
		}
		else {
			switch (bfi->macmode) {
			case WLC_MACMODE_DENY:
				return WLC_MACFLTR_ADDR_NOT_DENY;
			case WLC_MACMODE_ALLOW:
				return WLC_MACFLTR_ADDR_NOT_ALLOW;
			default:
				ASSERT(0);
				break;
			}
		}
	}
	return WLC_MACFLTR_DISABLED;
}

/* set/get list */
int
wlc_macfltr_list_set(wlc_macfltr_info_t *mfi, wlc_bsscfg_t *cfg, struct maclist *maclist, uint len)
{
	wlc_info_t *wlc = mfi->wlc;
	bss_macfltr_info_t *bfi;

	if (maclist->count > MAXMACLIST)
		return BCME_RANGE;

	if ((uint)len < OFFSETOF(struct maclist, ea) + maclist->count * ETHER_ADDR_LEN)
		return BCME_BUFTOOSHORT;

	ASSERT(cfg != NULL);

	bfi = BSS_MACFLTR_INFO(mfi, cfg);
	ASSERT(bfi != NULL);

	/* free the old one */
	if (bfi->maclist != NULL) {
		MFREE(wlc->osh, bfi->maclist, OFFSETOF(struct maclist, ea) +
		                              bfi->nmac * ETHER_ADDR_LEN);
		bfi->nmac = 0;
	}
	bfi->maclist = MALLOC(wlc->osh, OFFSETOF(struct maclist, ea) +
	                                maclist->count * ETHER_ADDR_LEN);
	if (bfi->maclist == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	bfi->nmac = maclist->count;
	bcopy(maclist->ea, bfi->maclist, bfi->nmac * ETHER_ADDR_LEN);
	return BCME_OK;
}

int
wlc_macfltr_list_get(wlc_macfltr_info_t *mfi, wlc_bsscfg_t *cfg, struct maclist *maclist, uint len)
{
	bss_macfltr_info_t *bfi;

	ASSERT(cfg != NULL);

	bfi = BSS_MACFLTR_INFO(mfi, cfg);
	ASSERT(bfi != NULL);

	if (len < (bfi->nmac - 1) * sizeof(struct ether_addr) + sizeof(struct maclist))
		return BCME_BUFTOOSHORT;

	maclist->count = bfi->nmac;
	bcopy(bfi->maclist, maclist->ea, bfi->nmac * ETHER_ADDR_LEN);
	return BCME_OK;
}
