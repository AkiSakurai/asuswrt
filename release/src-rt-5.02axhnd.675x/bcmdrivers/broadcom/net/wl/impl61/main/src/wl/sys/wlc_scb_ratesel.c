/*
 * Wrapper to scb rate selection algorithm of Broadcom
 * 802.11 Networking Adapter Device Driver.
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
 * $Id: wlc_scb_ratesel.c 777844 2019-08-13 07:36:57Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlRateSelection]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <bcmdevs.h>

#include <802.11.h>
#include <d11.h>

#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <phy_tpc_api.h>
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_scb_ratesel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif // endif
#include <wlc_txc.h>

#include <wl_dbg.h>

#include <wlc_vht.h>
#include <wlc_ht.h>
#include <wlc_he.h>
#include <wlc_lq.h>
#include <wlc_stf.h>
#include <wlc_ratelinkmem.h>
#include <wlc_musched.h>

/* iovar table */
enum {
	IOV_SCBRATE_DUMMY,	/**< dummy one in order to register the module */
	IOV_SCB_RATESET,	/**< dump the per-scb rate set */
	IOV_RATE_HISTO_REPORT	/* get per scb rate map histogram */
};

#define LINK_BW_ENTRY	0
static const bcm_iovar_t scbrate_iovars[] = {
	{"scbrate_dummy", IOV_SCBRATE_DUMMY, (IOVF_SET_DOWN), 0, IOVT_BOOL, 0},
#if defined(WLSCB_HISTO)
	{"rate_histo_report", IOV_RATE_HISTO_REPORT,
	(0), 0, IOVT_BUFFER, 0
	},
#endif /* WLSCB_HISTO */
	{NULL, 0, 0, 0, 0, 0}
};

typedef struct ppr_rateset {
	uint16 vht_mcsmap;              /* supported vht mcs nss bit map */
	uint8 mcs[PHY_CORE_MAX];       /* supported mcs index bit map */
	uint16 vht_mcsmap_prop;        /* vht proprietary rates bit map */
} ppr_rateset_t;

/** Supported rates for current chanspec/country */
typedef struct ppr_support_rates {
	chanspec_t chanspec;
	uint8 txstreams;
	clm_country_t country;
	ppr_rateset_t ppr_20_rates;
#if defined(WL11N) || defined(WL11AC)
	ppr_rateset_t ppr_40_rates;
#endif // endif
#ifdef WL11AC
	ppr_rateset_t ppr_80_rates;
	ppr_rateset_t ppr_160_rates;
#endif // endif
} ppr_support_rates_t;

struct wlc_ratesel_info {
	wlc_info_t	*wlc;		/**< pointer to main wlc structure */
	wlc_pub_t	*pub;		/**< public common code handler */
	ratesel_info_t *rsi;
	int32 scb_handle;
	uint16 cubby_sz;
	uint16 itxs_cubby_sz;
	ppr_support_rates_t *ppr_rates;
};

typedef struct ratesel_cubby ratesel_cubby_t;

/** rcb is per scb per ac rate control block. */
struct ratesel_cubby {
	rcb_t *scb_cubby;
#if defined(WL_MU_TX)
	rcb_itxs_t *scb_itxs_cubby;
#endif /* WL_MU_TX */
};

#define SCB_RATESEL_INFO(wss, scb) ((SCB_CUBBY((scb), (wrsi)->scb_handle)))

#if defined(WME_PER_AC_TX_PARAMS)
#define SCB_RATESEL_CUBBY(wrsi, scb, ac)	\
	((void *)(((char*)((ratesel_cubby_t *)SCB_RATESEL_INFO(wrsi, scb))->scb_cubby) + \
		(ac * (wrsi)->cubby_sz)))
#else /* WME_PER_AC_TX_PARAMS */
#define SCB_RATESEL_CUBBY(wrsi, scb, ac)	\
	(((ratesel_cubby_t *)SCB_RATESEL_INFO(wrsi, scb))->scb_cubby)
#endif /* WME_PER_AC_TX_PARAMS */

#if defined(WL_MU_TX)
#define SCB_RATESEL_ITXS_CUBBY(wrsi, scb, ac)	\
	((void *)(((char*)((ratesel_cubby_t *)SCB_RATESEL_INFO(wrsi, scb))->scb_itxs_cubby) + \
		(ac * (wrsi)->itxs_cubby_sz)))
#endif /* WL_MU_TX */

static int wlc_scb_ratesel_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);

static int wlc_scb_ratesel_scb_init(void *context, struct scb *scb);
static void wlc_scb_ratesel_scb_deinit(void *context, struct scb *scb);
#ifdef WLRSDB
static int wlc_scb_ratesel_scb_update(void *context, struct scb *scb, wlc_bsscfg_t* new_cfg);
#endif /* WLRSDB */

#ifdef BCMDBG
static void wlc_scb_ratesel_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_scb_ratesel_dump_scb NULL
#endif // endif

static ratespec_t wlc_scb_ratesel_getcurspec(wlc_ratesel_info_t *wrsi,
	struct scb *scb, uint8 ac);

static rcb_t *wlc_scb_ratesel_get_cubby(wlc_ratesel_info_t *wrsi, struct scb *scb,
	uint8 ac);
static int wlc_scb_ratesel_cubby_sz(void);
#ifdef WL11N
void wlc_scb_ratesel_rssi_enable(rssi_ctx_t *ctx);
void wlc_scb_ratesel_rssi_disable(rssi_ctx_t *ctx);
int wlc_scb_ratesel_get_rssi(rssi_ctx_t *ctx);
#endif // endif
/* Get CLM enabled rates bitmap for a bw */
static  ppr_rateset_t *
wlc_scb_ratesel_get_ppr_rates(wlc_info_t *wlc, wl_tx_bw_t bw);

static void wlc_scb_ratesel_ppr_updbmp(wlc_info_t *wlc, ppr_t *target_pwrs);

static void
wlc_scb_ratesel_ppr_filter(wlc_info_t *wlc, ppr_rateset_t *clm_rates,
	wlc_rateset_t *scb_rates, bool scb_VHT);

static wl_tx_bw_t rspecbw_to_bcmbw(uint8 bw);
#ifdef WL_MIMOPS_CFG
static void wlc_scb_ratesel_siso_downgrade(wlc_info_t *wlc, struct scb *scb);
#endif // endif

#if defined(WLSCB_HISTO)
static int wlc_scb_get_rate_histo(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wl_rate_histo_report_t *req, void *buf, int len);
#endif /* WLSCB_HISTO */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_ratesel_info_t *
BCMATTACHFN(wlc_scb_ratesel_attach)(wlc_info_t *wlc)
{
	wlc_ratesel_info_t *wrsi;
	scb_cubby_params_t cubby_params;

	if (!(wrsi = (wlc_ratesel_info_t *)MALLOCZ(wlc->osh, sizeof(*wrsi)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

#ifdef WL11AC
	if (!(wrsi->ppr_rates = (ppr_support_rates_t *)MALLOCZ(wlc->osh,
		sizeof(ppr_support_rates_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
#endif // endif
	wrsi->wlc = wlc;
	wrsi->pub = wlc->pub;

	if ((wrsi->rsi = wlc_ratesel_attach(wlc)) == NULL) {
		WL_ERROR(("%s: failed\n", __FUNCTION__));
		goto fail;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, scbrate_iovars, "scbrate", wrsi, wlc_scb_ratesel_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s:wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the scb container for per-scb-ac private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = wlc;
	cubby_params.fn_init = wlc_scb_ratesel_scb_init;
	cubby_params.fn_deinit = wlc_scb_ratesel_scb_deinit;
	cubby_params.fn_dump = wlc_scb_ratesel_dump_scb;
#ifdef WLRSDB
	cubby_params.fn_update = wlc_scb_ratesel_scb_update;
#endif /* WLRSDB */

	wrsi->scb_handle = wlc_scb_cubby_reserve_ext(wlc, wlc_scb_ratesel_cubby_sz(),
		&cubby_params);

	if (wrsi->scb_handle < 0) {
		WL_ERROR(("wl%d: %s:wlc_scb_cubby_reserve failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wlc_ratesel_rcb_sz(&wrsi->cubby_sz, &wrsi->itxs_cubby_sz);

	wlc_ratesel_rssi_attach(wrsi->rsi, wlc_scb_ratesel_rssi_enable,
		wlc_scb_ratesel_rssi_disable, wlc_scb_ratesel_get_rssi);

	return wrsi;

fail:

#if defined(WL11AC)
	if (wrsi->ppr_rates) {
		MFREE(wlc->osh, wrsi->ppr_rates, sizeof(*wrsi->ppr_rates));
	}
#endif /* defined(WL11AC) */

	if (wrsi->rsi) {
		MODULE_DETACH(wrsi->rsi, wlc_ratesel_detach);
	}

	MFREE(wlc->osh, wrsi, sizeof(wlc_ratesel_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_scb_ratesel_detach)(wlc_ratesel_info_t *wrsi)
{
	if (!wrsi)
		return;

	wlc_module_unregister(wrsi->pub, "scbrate", wrsi);
	wlc_ratesel_detach(wrsi->rsi);

#ifdef WL11AC
	MFREE(wrsi->pub->osh, wrsi->ppr_rates, sizeof(ppr_support_rates_t));
#endif // endif
	MFREE(wrsi->pub->osh, wrsi, sizeof(wlc_ratesel_info_t));
}

#ifdef WLRSDB
static int
wlc_scb_ratesel_scb_update(void *context, struct scb *scb, wlc_bsscfg_t* new_cfg)
{
#ifdef WL_LPC
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_info_t *new_wlc = new_cfg->wlc;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info = SCB_RATESEL_INFO(wrsi, scb);
	wlc_ratesel_lpc_update(new_wlc->wrsi->rsi, cubby_info->scb_cubby);
#endif // endif
	return BCME_OK;
}
#endif /* WLRSDB */

/* alloc per ac cubby space on scb attach. */
static int
wlc_scb_ratesel_scb_init(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info = SCB_RATESEL_INFO(wrsi, scb);
	int cubby_size;

	WL_RATE(("%s scb %p allocate cubby space.\n",
		__FUNCTION__, OSL_OBFUSCATE_BUF(scb)));
	cubby_info->scb_cubby = NULL;
#if defined(WL_MU_TX)
	cubby_info->scb_itxs_cubby = NULL;
#endif /* defined(WL_MU_TX) */
	if (scb && !SCB_INTERNAL(scb)) {
#if defined(WME_PER_AC_TX_PARAMS)
		if (!RATELINKMEM_ENAB(wlc->pub)) {
			cubby_size = AC_COUNT * wrsi->cubby_sz;
		} else
#endif // endif
		{
			cubby_size = wrsi->cubby_sz;
		}
		cubby_info->scb_cubby = (rcb_t *)MALLOCZ(wlc->osh, cubby_size);
		if (!cubby_info->scb_cubby) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, cubby_size,
				MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
#if defined(WL_MU_TX)
		cubby_size = AC_COUNT * wrsi->itxs_cubby_sz; /* itxs_cubby is always per AC */
		cubby_info->scb_itxs_cubby = (rcb_itxs_t *)MALLOCZ(wlc->osh, cubby_size);
		if (!cubby_info->scb_itxs_cubby) {
			WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, cubby_size,
				MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
#endif /* defined(WL_MU_TX) */
	}

	/* Default mcs set */
	scb->rateset.vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
	scb->rateset.vht_mcsmap_prop = VHT_PROP_MCS_MAP_NONE_ALL;
	wlc_rateset_he_none_all(&scb->rateset);
	return BCME_OK;
}

/* free cubby space after scb detach */
static void
wlc_scb_ratesel_scb_deinit(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info = SCB_RATESEL_INFO(wrsi, scb);
	int cubby_size;

	WL_RATE(("%s scb %p free cubby space.\n",
		__FUNCTION__, OSL_OBFUSCATE_BUF(scb)));
	if (cubby_info->scb_cubby) {
#if defined(WME_PER_AC_TX_PARAMS)
		if (!RATELINKMEM_ENAB(wlc->pub)) {
			cubby_size = AC_COUNT * wrsi->cubby_sz;
		} else
#endif // endif
		{
			cubby_size = wrsi->cubby_sz;
		}
		MFREE(wlc->osh, cubby_info->scb_cubby, cubby_size);
		cubby_info->scb_cubby = NULL;
	}
#if defined(WL_MU_TX)
	if (cubby_info->scb_itxs_cubby) {
		cubby_size = wrsi->itxs_cubby_sz * AC_COUNT;
		MFREE(wlc->osh, cubby_info->scb_itxs_cubby, cubby_size);
		cubby_info->scb_itxs_cubby = NULL;
	}
#endif /* defined(WL_MU_TX) */
}

/* handle SCBRATE related iovars */
static int
wlc_scb_ratesel_doiovar(void *hdl, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint val_size, struct wlc_if *wlcif)
{
	wlc_ratesel_info_t *wrsi = hdl;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;

	BCM_REFERENCE(plen);
	BCM_REFERENCE(val_size);
	BCM_REFERENCE(bsscfg);

	wlc = wrsi->wlc;
	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
#if defined(WLSCB_HISTO)
	case IOV_GVAL(IOV_RATE_HISTO_REPORT):
		if (plen < sizeof(wl_rate_histo_report_t)) {
			err = wlc_scb_get_rate_histo(wlc, bsscfg, NULL, arg, alen);
		} else {
			err = wlc_scb_get_rate_histo(wlc, bsscfg,
				(wl_rate_histo_report_t *) params, arg, alen);
		}
		break;
#endif /* WLSCB_HISTO */

	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}

static rcb_t *
wlc_scb_ratesel_get_cubby(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{

	ASSERT(wrsi);

	ac = (WME_PER_AC_MAXRATE_ENAB(wrsi->pub) ? ac : 0);
	ASSERT(ac < AC_COUNT);

	return (SCB_RATESEL_CUBBY(wrsi, scb, ac));
}

#ifdef BCMDBG
static void
wlc_scb_ratesel_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	int ac;
	rcb_t *rcb;

	if (SCB_INTERNAL(scb))
		return;

	wlc_dump_rspec(wlc->scbstate, wlc_scb_ratesel_get_primary(wlc, scb, NULL), b);

	for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
		rcb = SCB_RATESEL_CUBBY(wrsi, scb, ac);
		wlc_ratesel_dump_rcb(rcb, ac, b);
	}
}
#endif /* BCMDBG */

#if defined(BCMDBG)
/* get the fixrate per scb/ac. */
int
wlc_scb_ratesel_get_fixrate(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_ratesel_info_t *wrsi = (wlc_ratesel_info_t *)ctx;
	int ac;
	rcb_t *state;

	for (ac = 0; ac < WME_MAX_AC(wrsi->wlc, scb); ac++) {
		if (!(state = SCB_RATESEL_CUBBY(wrsi, scb, ac))) {
			WL_ERROR(("skip ac %d\n", ac));
			continue;
		}
		wlc_ratesel_get_fixrate(state, ac, b);
	}
	return 0;
}

/* set the fixrate per scb/ac. */
int
wlc_scb_ratesel_set_fixrate(void *ctx, struct scb *scb, int ac, uint8 val)
{
	wlc_ratesel_info_t *wrsi = (wlc_ratesel_info_t *)ctx;
	int i;
	rcb_t *state = NULL;

	/* check AC validity */
	if (ac == -1) { /* For all access class */
		for (i = 0; i < WME_MAX_AC(wrsi->wlc, scb); i++) {
			if (!(state = SCB_RATESEL_CUBBY(wrsi, scb, i))) {
				WL_ERROR(("ac %d does not exist!\n", ac));
				return BCME_ERROR;
			}

			if (wlc_ratesel_set_fixrate(state, i, val) < 0)
				return BCME_BADARG;
		}
	} else if ((ac >= 0) && (ac < WME_MAX_AC(wrsi->wlc, scb))) { /* For single ac */
		if (!(state = SCB_RATESEL_CUBBY(wrsi, scb, ac))) {
				WL_ERROR(("ac %d does not exist!\n", ac));
				return BCME_ERROR;
		}

		if (wlc_ratesel_set_fixrate(state, ac, val) < 0)
			return BCME_BADARG;
	} else {
		WL_ERROR(("ac %d out of range [0, %d]\n", ac, WME_MAX_AC(wrsi->wlc, scb) - 1));
		return BCME_ERROR;
	}

	return 0;
}

/* dump per-scb state, tunable parameters, upon request, (say by wl ratedump) */
int
wlc_scb_ratesel_scbdump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_ratesel_info_t *wrsi = (wlc_ratesel_info_t *)ctx;
	int32 ac;
	rcb_t *state;

	for (ac = 0; ac < WME_MAX_AC(wrsi->wlc, scb); ac++) {
		state = SCB_RATESEL_CUBBY(wrsi, scb, ac);
		if (state) {
			bcm_bprintf(b, "AC[%d] --- \n\t", ac);
			wlc_ratesel_scbdump(state, b);
		}
	}

	return 0;
}

#endif // endif

#ifdef WL11N
bool
wlc_scb_ratesel_sync(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac, uint now, int rssi)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);

	return wlc_ratesel_sync(state, now, rssi);
}
#endif /* WL11N */

static ratespec_t
wlc_scb_ratesel_getcurspec(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p ac = %d\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(wrsi), OSL_OBFUSCATE_BUF(scb), ac));
		return (WLC_RATE_6M | WL_RSPEC_BW_20MHZ);
	}
	return wlc_ratesel_getcurspec(state);
}

/* given only wlc and scb, return best guess at the primary rate */
ratespec_t
wlc_scb_ratesel_get_primary(wlc_info_t *wlc, struct scb *scb, void *pkt)
{
	ratespec_t rspec = 0;
	wlcband_t *scbband = wlc_scbband(wlc, scb);
	uint8 prio;
	bool rspec_overide;

	prio = 0;
	if ((pkt != NULL) && SCB_QOS(scb)) {
		prio = (uint8)PKTPRIO(pkt);
		ASSERT(prio <= MAXPRIO);
	}

	if (scbband == NULL) {
		ASSERT(0);
		return 0;
	}
	/* XXX 4360: the following rspec calc code is now in 3 places;
	 * here, d11achdrs and d11nhdrs.
	 * Need to consolidate this.
	 */
	rspec_overide = RSPEC_ACTIVE(scbband->rspec_override);

	if (rspec_overide) {
		/* get override if active */
		rspec = scbband->rspec_override;
	} else {
		/* let ratesel figure it out if override not present */
		rspec = wlc_scb_ratesel_getcurspec(wlc->wrsi, scb, WME_PRIO2AC(prio));
	}

	/* Cleanup raw ratespec and return result */
	return (wlc_scb_ratesel_rspec_cleanup(wlc, scb, scbband, rspec, rspec_overide));
}

/* Cleanup raw ratespec from rate module
 * Originally part of wlc_scb_ratesel_get_primary().
 * The cleanup of the rspec flagging has general applicability
 */
ratespec_t
wlc_scb_ratesel_rspec_cleanup(wlc_info_t *wlc, struct scb *scb, wlcband_t *scbband,
	ratespec_t rspec, bool rspec_overide)
{
	uint phyctl1_stf = wlc->stf->ss_opmode;
#ifdef WL11N
	uint32 mimo_txbw = 0;
#if WL_HT_TXBW_OVERRIDE_ENAB
	uint32 _txbw2rspecbw[] = {
		WL_RSPEC_BW_20MHZ, /* WL_TXBW20L	*/
		WL_RSPEC_BW_20MHZ, /* WL_TXBW20U	*/
		WL_RSPEC_BW_40MHZ, /* WL_TXBW40	*/
		WL_RSPEC_BW_40MHZ, /* WL_TXBW40DUP */
		WL_RSPEC_BW_20MHZ, /* WL_TXBW20LL */
		WL_RSPEC_BW_20MHZ, /* WL_TXBW20LU */
		WL_RSPEC_BW_20MHZ, /* WL_TXBW20UL */
		WL_RSPEC_BW_20MHZ, /* WL_TXBW20UU */
		WL_RSPEC_BW_40MHZ, /* WL_TXBW40L */
		WL_RSPEC_BW_40MHZ, /* WL_TXBW40U */
		WL_RSPEC_BW_80MHZ /* WL_TXBW80 */
	};
	int8 txbw_override_idx;
#endif /* WL_HT_TXBW_OVERRIDE_ENAB */
#endif /* WL11N */

	ASSERT(scbband);

	if (N_ENAB(wlc->pub)) {
		/* apply siso/cdd to single stream mcs's or ofdm if rspec is auto selected */
		if (((RSPEC_ISHT(rspec) && IS_SINGLE_STREAM(rspec & WL_RSPEC_HT_MCS_MASK)) ||
		     (RSPEC_ISVHT(rspec) &&
		      ((rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT) == 1) ||
		     (RSPEC_ISHE(rspec) &&
		      ((rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT) == 1) ||
			RSPEC_ISOFDM(rspec)) &&
			!(rspec & WL_RSPEC_OVERRIDE_MODE)) {

			rspec &= ~(WL_RSPEC_TXEXP_MASK | WL_RSPEC_STBC);

			/* For SISO MCS use STBC if possible */
			if (!RSPEC_ISOFDM(rspec) && (WLC_IS_STBC_TX_FORCED(wlc) ||
				((RSPEC_ISVHT(rspec) && WLC_STF_SS_STBC_VHT_AUTO(wlc, scb)) ||
				(RSPEC_ISHT(rspec) && WLC_STF_SS_STBC_HT_AUTO(wlc, scb)) ||
				(RSPEC_ISHE(rspec) && WLC_STF_SS_STBC_HE_AUTO(wlc, scb))))) {
				ASSERT(WLC_STBC_CAP_PHY(wlc));
				rspec |= WL_RSPEC_STBC;
			} else if (phyctl1_stf == PHY_TXC1_MODE_CDD) {
				rspec |= (1 << WL_RSPEC_TXEXP_SHIFT);
			}
		}

		/* bandwidth */
		if (RSPEC_BW(rspec) != WL_RSPEC_BW_UNSPECIFIED) {
			mimo_txbw = RSPEC_BW(rspec);
		} else if ((CHSPEC_IS8080(wlc->chanspec) &&
			scb->flags3 & SCB3_IS_80_80) &&
			(RSPEC_ISVHT(rspec) || RSPEC_ISHE(rspec))) {
			mimo_txbw = WL_RSPEC_BW_160MHZ;
		} else if ((CHSPEC_IS160(wlc->chanspec) &&
			scb->flags3 & SCB3_IS_160) &&
			(RSPEC_ISVHT(rspec) || RSPEC_ISHE(rspec))) {
			mimo_txbw = WL_RSPEC_BW_160MHZ;
		} else if (CHSPEC_BW_GE(wlc->chanspec, WL_CHANSPEC_BW_80) &&
			(RSPEC_ISVHT(rspec) || RSPEC_ISHE(rspec))) {
			mimo_txbw = WL_RSPEC_BW_80MHZ;
		} else if (CHSPEC_BW_GE(wlc->chanspec, WL_CHANSPEC_BW_40)) {
			/* default txbw is 20in40 */
			mimo_txbw = WL_RSPEC_BW_20MHZ;

			if ((RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec) || RSPEC_ISHE(rspec)) &&
			    (scb->flags & SCB_IS40)) {
				mimo_txbw = WL_RSPEC_BW_40MHZ;
#ifdef WLMCHAN
				/* XXX 4360: why would scb->flags indicate is40 if the sta
				 * is associated at a 20MHz? Do we need different flags for
				 * capability (is40) from operational for the current state,
				 * which would be is20?  This same problem needs to be fixed
				 * for the 80MHz case.
				 */
				/* PR95044: if mchan enabled and bsscfg is AP, then must
				 * check the bsscfg chanspec to make sure our AP is
				 * operating on 40MHz channel.
				 */
				if (MCHAN_ENAB(wlc->pub) && BSSCFG_AP(scb->bsscfg) &&
					(CHSPEC_IS20(scb->bsscfg->current_bss->chanspec))) {
					mimo_txbw = WL_RSPEC_BW_20MHZ;
				}
#endif /* WLMCHAN */
			}
		} else	{
			mimo_txbw = WL_RSPEC_BW_20MHZ;
		}

#if WL_HT_TXBW_OVERRIDE_ENAB
		if (CHSPEC_IS40(wlc->chanspec) || CHSPEC_IS80(wlc->chanspec)) {
			WL_HT_TXBW_OVERRIDE_IDX(wlc->hti, rspec, txbw_override_idx);

			if (txbw_override_idx >= 0) {
				mimo_txbw = _txbw2rspecbw[txbw_override_idx];
			}
		}
#endif /* WL_HT_TXBW_OVERRIDE_ENAB */
		rspec &= ~WL_RSPEC_BW_MASK;
		rspec |= mimo_txbw;
	} else {
		rspec |= WL_RSPEC_BW_20MHZ;
		/* for nphy, stf of ofdm frames must follow policies */
		if (WLCISNPHY(scbband) || RSPEC_ISOFDM(rspec)) {
			rspec &= ~WL_RSPEC_TXEXP_MASK;
			if (phyctl1_stf == PHY_TXC1_MODE_CDD) {
				rspec |= (1 << WL_RSPEC_TXEXP_SHIFT);
			}
		}
	}

	if (!rspec_overide) {
		int sgi_tx = WLC_HT_GET_SGI_TX(wlc->hti);

		rspec &= ~(WL_RSPEC_GI_MASK | WL_RSPEC_LDPC);

		if (RSPEC_ISHE(rspec)) {
			int hegi = ((sgi_tx == ON)? WL_RSPEC_HE_2x_LTF_GI_0_8us:
				WL_RSPEC_HE_2x_LTF_GI_1_6us);

			if ((sgi_tx >= WL_HEGI_VAL(WL_RSPEC_HE_2x_LTF_GI_0_8us)) &&
			    (sgi_tx <= WL_HEGI_VAL(WL_RSPEC_HE_4x_LTF_GI_3_2us))) {
				hegi = sgi_tx - WL_SGI_HEGI_DELTA;
			}

			rspec |= HE_GI_TO_RSPEC(hegi);
		} else if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
			if (sgi_tx) {
				rspec |= WL_RSPEC_SGI;
			}
		}

		if (!RSPEC_ISLEGACY(rspec)) {
			/* XXX move this LDPC decision outside,
			 * i.e. turn off when power is a concern?
			 */
			if (wlc->stf->ldpc_tx == ON ||
			    (SCB_LDPC_CAP(scb) && wlc->stf->ldpc_tx == AUTO)) {
				rspec |= WL_RSPEC_LDPC;
			}
		}
	}
	return rspec;
}

/** wrapper function to select transmit rate given per-scb state */
bool BCMFASTPATH
wlc_scb_ratesel_gettxrate(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 *frameid,
	ratesel_txparams_t *ratesel_rates, uint16 *flags)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ratesel_rates->ac);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p ac = %d\n",
		__FUNCTION__, OSL_OBFUSCATE_BUF(wrsi), OSL_OBFUSCATE_BUF(scb), ratesel_rates->ac));
		return FALSE;
	}

	return wlc_ratesel_gettxrate(state, frameid, ratesel_rates, flags);
}

#ifdef WL11N
void
wlc_scb_ratesel_probe_ready(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	bool is_ampdu, uint8 ampdu_txretry, uint8 ac)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p ac = %d\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(wrsi), OSL_OBFUSCATE_BUF(scb), ac));
		return;
	}
	wlc_ratesel_probe_ready(state, frameid, is_ampdu, ampdu_txretry);
}

void BCMFASTPATH
wlc_scb_ratesel_upd_rxstats(wlc_ratesel_info_t *wrsi, ratespec_t rx_rspec, uint16 rxstatus2)
{
	wlc_ratesel_upd_rxstats(wrsi->rsi, rx_rspec, rxstatus2);
}
#endif /* WL11N */

/* non-AMPDU txstatus rate update, default to use non-mcs rates only */
void
wlc_scb_ratesel_upd_txstatus_normalack(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs,
	uint16 sfbl, uint16 lfbl, uint8 tx_mcs /* non-HT format */,
	bool sgi, uint8 antselid, bool fbr, uint8 ac)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p ac = %d\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(wrsi), OSL_OBFUSCATE_BUF(scb), ac));
		return;
	}
	wlc_ratesel_upd_txstatus_normalack(state, txs, sfbl, lfbl, tx_mcs, sgi, antselid, fbr);
}

#ifdef WL11N
void
wlc_scb_ratesel_aci_change(wlc_ratesel_info_t *wrsi, bool aci_state)
{
	wlc_ratesel_aci_change(wrsi->rsi, aci_state);
}

/*
 * Return the fallback rate of the specified mcs rate.
 * Ensure that is a mcs rate too.
 */
ratespec_t
wlc_scb_ratesel_getmcsfbr(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac, uint8 mcs)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	ASSERT(state);

	return (wlc_ratesel_getmcsfbr(state, ac, mcs));
}

/*
 * The case that (mrt+fbr) == 0 is handled as RTS transmission failure.
 */
void
wlc_scb_ratesel_upd_txs_ampdu(wlc_ratesel_info_t *wrsi, struct scb *scb,
	ratesel_txs_t *rs_txs, tx_status_t *txs, uint16 flags)
{
	rcb_t *state;
	rcb_itxs_t *rcb_itxs = NULL;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, rs_txs->ac);
	ASSERT(state);
#if defined(WL_MU_TX)
	rcb_itxs = SCB_RATESEL_ITXS_CUBBY(wrsi, scb, rs_txs->ac);
	ASSERT(rcb_itxs);
#endif /* WL_MU_TX */

	wlc_ratesel_upd_txs_ampdu(state, rcb_itxs, rs_txs, txs, flags);
}

#if defined(WL_MU_TX)
void
wlc_scb_ratesel_init_rcb_itxs(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{
	rcb_itxs_t *rcb_itxs = SCB_RATESEL_ITXS_CUBBY(wrsi, scb, ac);
	ASSERT(rcb_itxs);
	wlc_ratesel_init_rcb_itxs(rcb_itxs);
}

void
wlc_scb_ratesel_upd_itxs(wlc_ratesel_info_t *wrsi, struct scb *scb,
	ratesel_txs_t *rs_txs, tx_status_t *txs, uint16 flags)
{
	rcb_t *state;
	rcb_itxs_t *rcb_itxs = SCB_RATESEL_ITXS_CUBBY(wrsi, scb, rs_txs->ac);

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, rs_txs->ac);
	ASSERT(state);

	wlc_ratesel_upd_itxs(state, rcb_itxs, rs_txs, txs, flags);
}

#if defined(WL11AX)
void
wlc_scb_ratesel_upd_txs_trig(wlc_ratesel_info_t *wrsi, struct scb *scb,
	ratesel_tgtxs_t *tgtxs, tx_status_t *txs)
{
	rcb_t *state;

	BCM_REFERENCE(state);

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, 0);
	ASSERT(state);
	wlc_ratesel_upd_txs_trig(state, tgtxs, txs);
}
#endif /* defined(WL11AX) */
#endif /* defined(WL_MU_TX) */

/* update state upon received BA */
void BCMFASTPATH
wlc_scb_ratesel_upd_txs_blockack(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs,
	uint8 suc_mpdu, uint8 tot_mpdu, bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 mcs /* non-HT format */, bool sgi, uint8 antselid, uint8 ac)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	ASSERT(state);

	wlc_ratesel_upd_txs_blockack(state, txs, suc_mpdu, tot_mpdu, ba_lost, retry, fb_lim,
		tx_error, mcs, sgi, antselid);
}
#endif /* WL11N */

bool
wlc_scb_ratesel_minrate(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs, uint8 ac)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	ASSERT(state);

	return (wlc_ratesel_minrate(state, txs));
}

static void
wlc_scb_ratesel_ppr_filter(wlc_info_t *wlc, ppr_rateset_t *clm_rates,
	wlc_rateset_t *scb_rates, bool scb_VHT)
{
	uint8 i;

	for (i = 0; i < PHYCORENUM(wlc->stf->op_txstreams); i++) {
		scb_rates->mcs[i] &= clm_rates->mcs[i];
#ifdef WL11AC
		if (scb_VHT) {
			/* check VHT 8_9 */
			uint16 clm_vht_rate = VHT_MCS_MAP_GET_MCS_PER_SS(i+1,
				clm_rates->vht_mcsmap);
			uint16 scb_vht_rate = VHT_MCS_MAP_GET_MCS_PER_SS(i+1,
				scb_rates->vht_mcsmap);

			if (scb_vht_rate != VHT_CAP_MCS_MAP_NONE) {
				if (clm_vht_rate == VHT_CAP_MCS_MAP_NONE) {
					VHT_MCS_MAP_SET_MCS_PER_SS(i+1, VHT_CAP_MCS_MAP_NONE,
						scb_rates->vht_mcsmap);
				} else if (scb_vht_rate > clm_vht_rate) {
					VHT_MCS_MAP_SET_MCS_PER_SS(i+1, clm_vht_rate,
						scb_rates->vht_mcsmap);
				}
#ifndef NO_PROPRIETARY_VHT_RATES
				/* check VHT 10_11 */
				clm_vht_rate =
					VHT_MCS_MAP_GET_MCS_PER_SS(i+1,
						clm_rates->vht_mcsmap_prop);

				/* Proprietary rates map can be either
				 * VHT_PROP_MCS_MAP_10_11 or VHT_CAP_MCS_MAP_NONE,
				 * if clm_vht_rate is set to VHT_PROP_MCS_MAP_10_11,
				 * no action required.
				 */
				if (clm_vht_rate == VHT_PROP_MCS_MAP_NONE) {
					VHT_MCS_MAP_SET_MCS_PER_SS(i+1,
						VHT_PROP_MCS_MAP_NONE,
						scb_rates->vht_mcsmap_prop);
				}
#endif /* !NO_PROPRIETARY_VHT_RATES */
			}
		}
#endif /* WL11AC */
	}
}

#ifdef WL11AC
static uint8
wlc_scb_get_bw_from_scb_oper_mode(wlc_vht_info_t *vhti, struct scb *scb)
{
	uint8 bw = 0, bw160_8080 = 0;
	uint8 mode = 0;
	mode = wlc_vht_get_scb_opermode(vhti, scb);
	bw160_8080 = DOT11_OPER_MODE_160_8080(mode);
	mode &= DOT11_OPER_MODE_CHANNEL_WIDTH_MASK;
	if (mode == DOT11_OPER_MODE_20MHZ)
		bw = BW_20MHZ;
	else if (mode == DOT11_OPER_MODE_40MHZ)
		bw = BW_40MHZ;
	else if (mode == DOT11_OPER_MODE_80MHZ && !bw160_8080)
		bw = BW_80MHZ;
	else if (mode == DOT11_OPER_MODE_80MHZ && bw160_8080)
		bw = BW_160MHZ;

	return bw;
}
#endif /* WL11AC */

static uint8
wlc_scb_ratesel_link_bw_upd(wlc_info_t *wlc, struct scb *scb)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	rcb_t *rcb = SCB_RATESEL_CUBBY(wrsi, scb, LINK_BW_ENTRY);
	uint8 bw = BW_20MHZ;
#ifdef WL11AC
	uint8 scb_oper_mode_bw = BW_20MHZ;
#endif // endif
	chanspec_t chanspec = wlc->chanspec;

#ifdef WL11AC
	if ((SCB_VHT_CAP(scb) || SCB_HE_CAP(scb)) &&
		CHSPEC_IS8080(chanspec) &&
		((scb->flags3 & SCB3_IS_80_80)))
		bw = BW_160MHZ;
	else if ((SCB_VHT_CAP(scb) || SCB_HE_CAP(scb)) &&
		CHSPEC_IS160(chanspec) &&
		((scb->flags3 & SCB3_IS_160)))
		bw = BW_160MHZ;
	else if (CHSPEC_BW_GE(chanspec, WL_CHANSPEC_BW_80) &&
		(SCB_VHT_CAP(scb) || SCB_HE_CAP(scb)))
		bw = BW_80MHZ;
	else
#endif /* WL11AC */
	if (((scb->flags & SCB_IS40)) &&
	    CHSPEC_BW_GE(chanspec, WL_CHANSPEC_BW_40))
		bw = BW_40MHZ;

	/* here bw derived from chanspec and capabilities */
#ifdef WL11AC
	/**
	 * Processing operating mode notification for channel bw
	 *
	 * 27.8 Operating mode indication for HE cases:
	 * 27.8.1 General:
	 * An HE STA can change its operating mode setting using either operating mode notification
	 * as described in '11.42' (Notification of operating mode changes), or the operating mode
	 * indication (OMI) procedure.
	 */
	if ((SCB_HE_CAP(scb) || SCB_HT_CAP(scb) || SCB_VHT_CAP(scb)) &&
		wlc_vht_get_scb_opermode_enab(wlc->vhti, scb) &&
		!DOT11_OPER_MODE_RXNSS_TYPE(wlc_vht_get_scb_opermode(wlc->vhti, scb))) {
		scb_oper_mode_bw = wlc_scb_get_bw_from_scb_oper_mode(wlc->vhti, scb);
		bw = (scb_oper_mode_bw < bw) ? scb_oper_mode_bw : bw;
	}
#endif /* WL11AC */

	wlc_ratesel_set_link_bw(rcb, bw);
	return (bw);
}

#ifdef WL_MIMOPS_CFG
/* Filter out any rates above siso rates for the scb if current bsscfg txchain is siso
 * and is different than hw txchain, or if we are in MRC mode
 */
void wlc_scb_ratesel_siso_downgrade(wlc_info_t *wlc, struct scb *scb)
{
	int i = 0;
	bool siso = FALSE;
	wlc_hw_config_t *scb_bsscfg_hw_cfg = wlc_stf_bss_hw_cfg_get(scb->bsscfg);
	wlc_mimo_ps_cfg_t *scb_bsscfg_mimo_ps_cfg = wlc_stf_mimo_ps_cfg_get(scb->bsscfg);

	if (!(scb_bsscfg_hw_cfg && scb_bsscfg_mimo_ps_cfg))
		return;

	/* If MRC mode is enabled, we're really siso */
	if (scb_bsscfg_mimo_ps_cfg && scb_bsscfg_mimo_ps_cfg->mrc_chains_changed)
		siso = TRUE;

	/* If we're single-chain and need to override current HW setting */
	if ((WLC_BITSCNT(scb_bsscfg_hw_cfg->current_txchains) == 1) &&
		(WLC_BITSCNT(wlc->stf->txchain) != 1))
		siso = TRUE;

	/* Filter the rates if needed */
	if (siso) {
		if (SCB_HT_CAP(scb)) {
			for (i = 1; i < MCSSET_LEN; i++)
				scb->rateset.mcs[i] = 0;
		}
#ifdef WL11AC
		if (SCB_VHT_CAP(scb)) {
			scb->rateset.vht_mcsmap = 0xfffe;
		}
#endif // endif
	}
}
#endif /* WL_MIMOPS_CFG */

/* initialize per-scb state utilized by rate selection
 *   ATTEN: this fcn can be called to "reinit", avoid dup MALLOC
 *   this new design makes this function the single entry points for any select_rates changes
 *   this function should be called when any its parameters changed: like bw or stream
 *   this function will build select_rspec[] with all constraint and rateselection will
 *      be operating on this constant array with reference to known_rspec[] for threshold
 */

void
wlc_scb_ratesel_init(wlc_info_t *wlc, struct scb *scb)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_init_t rs_init;
	int32 ac;
	wlc_rateset_t *rateset;
	wlc_rateset_t new_rateset;
	ppr_rateset_t *clm_rateset;
	chanspec_t chanspec;
	uint8 i;
	bool need_rate_filter = TRUE;

	/* Use configured chanspec rather than the instantaneous phy chanspec for WDS links */
	if (SCB_LEGACY_WDS(scb)) {
		chanspec = wlc->home_chanspec;
	} else {
		chanspec = wlc->chanspec;
	}

	rs_init.rcb_sz = wrsi->cubby_sz;
	rs_init.ldpc_tx = 0;
	rs_init.antselid_init = 0;
	rs_init.bw = BW_20MHZ;
	rs_init.active_antcfg_num = 0;
	rs_init.sgi_tx = OFF;

	if (SCB_INTERNAL(scb))
		return;
	if (WLANTSEL_ENAB(wlc))
		wlc_antsel_ratesel(wlc->asi, &rs_init.active_antcfg_num, &rs_init.antselid_init);

	rs_init.bw = wlc_scb_ratesel_link_bw_upd(wlc, scb);

	if ((wlc->stf->ldpc_tx == AUTO) || (wlc->stf->ldpc_tx == ON)) {
		if (rs_init.bw < BW_80MHZ && SCB_LDPC_CAP(scb))
			rs_init.ldpc_tx |= HT_LDPC;
#ifdef WL11AC
		if (SCB_VHT_LDPC_CAP(wlc->vhti, scb))
			rs_init.ldpc_tx |= VHT_LDPC;
#ifdef WL11AX
		if (SCB_HE_LDPC_CAP(wlc->hei, scb))
			rs_init.ldpc_tx |= HE_LDPC;
#endif /* WL11AX */
#endif /* WL11AC */
	}

	if (WLC_HT_GET_SGI_TX(wlc->hti) == AUTO) {
		if (scb->flags2 & SCB2_SGI20_CAP)
			rs_init.sgi_tx |= SGI_BW20;
		if (rs_init.bw >= BW_40MHZ && (scb->flags2 & SCB2_SGI40_CAP))
			rs_init.sgi_tx |= SGI_BW40;
		if (rs_init.bw >= BW_80MHZ && SCB_VHT_SGI80(wlc->vhti, scb))
			rs_init.sgi_tx |= SGI_BW80;
		if (rs_init.bw >= BW_160MHZ && SCB_VHT_SGI160(wlc->vhti, scb))
			rs_init.sgi_tx |= SGI_BW160;
		/* Disable SGI Tx in 20MHz on IPA chips */
		if (rs_init.bw == BW_20MHZ && wlc->stf->ipaon)
			rs_init.sgi_tx = OFF;
	}

	rateset = &scb->rateset;

#ifdef WL11AC
	/* Set up the mcsmap in scb->rateset.vht_mcsmap */
	if (SCB_VHT_CAP(scb))
	{
		if (wlc->stf->txstream_value == 0) {
			wlc_vht_upd_rate_mcsmap(wlc->vhti, scb);
		} else  {
			/* vht rate override
			* for 3 stream the value 0x 11 11 11 11 1110 10 10
			* for 2 stream the value 0x 11 11 11 11 11 11 1010
			* for 1 stream the value 0x 11 11 11 11 11 11 1110
			*/
			if (wlc->stf->txstream_value == 2) {
				rateset->vht_mcsmap = 0xfffa;
			} else if (wlc->stf->txstream_value == 1) {
				rateset->vht_mcsmap = 0xfffe;
			}
		}
	}

	if (!SCB_HE_CAP(scb)) {
		wlc_rateset_he_none_all(rateset);
	}
#ifdef WL11AX
	else {
		wlc_he_upd_scb_rateset_mcs(wlc->hei, scb, rs_init.bw);
	}
#endif /* WL11AX */

	rs_init.bw_auto = TRUE;
#if WL11AX
	/* bw_auto is forced off if DLOFDMA is on for the given STA */
	rs_init.bw_auto = (SCB_DLOFDMA(scb)) ? FALSE : rs_init.bw_auto;
	WL_RATE(("%s bw_auto %d dl_ofdma policy 0x%x\n", __FUNCTION__,
		rs_init.bw_auto, SCB_DLOFDMA(scb)));
#endif // endif

#endif /* WL11AC */

	/* HT rate overide for BTCOEX */
	if ((SCB_HT_CAP(scb) && wlc->stf->txstream_value)) {
		for (i = 1; i < 4; i++) {
			if (i >= wlc->stf->txstream_value) {
				rateset->mcs[i] = 0;
			}
		}
#if defined(WLPROPRIETARY_11N_RATES)
		for (i = WLC_11N_FIRST_PROP_MCS; i <= WLC_MAXMCS; i++) {
			if (GET_PROPRIETARY_11N_MCS_NSS(i) > wlc->stf->txstream_value)
				clrbit(rateset->mcs, i);
		}
#endif /* WLPROPRIETARY_11N_RATES */
	}
#ifdef WL_MIMOPS_CFG
	if (WLC_MIMOPS_ENAB(wlc->pub)) {
		wlc_scb_ratesel_siso_downgrade(wlc, scb);
	}
#endif /* WL_MIMOPS_CFG */

	for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
		rs_init.vht_ratemask = 0;
		rs_init.state = SCB_RATESEL_CUBBY(wrsi, scb, ac);

		if (rs_init.state == NULL) {
			ASSERT(0);
			return;
		}

		bcopy(rateset, &new_rateset, sizeof(new_rateset));

#ifdef WL11N
		if (BSS_N_ENAB(wlc, scb->bsscfg)) {
			if (((WLC_HT_GET_SCB_MIMOPS_ENAB(wlc->hti, scb) &&
				!WLC_HT_GET_SCB_MIMOPS_RTS_ENAB(wlc->hti, scb)) ||
				(wlc->stf->op_txstreams == 1) || (wlc->stf->siso_tx == 1))) {
				new_rateset.mcs[1] = 0;
				new_rateset.mcs[2] = 0;
			} else if (wlc->stf->op_txstreams == 2)
				new_rateset.mcs[2] = 0;
		}
#endif // endif

#ifdef WL11AC
		rs_init.vht_ratemask = wlc_vht_get_scb_ratemask_per_band(wlc->vhti, scb);
#endif // endif
		WL_RATE(("%s: scb 0x%p ac %d state 0x%p bw %s op_txstreams %d"
			" active_ant %d band %d vht:%u vht_rm:0x%x he:%u\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(scb), ac,
			OSL_OBFUSCATE_BUF(rs_init.state), (rs_init.bw == BW_20MHZ) ?
			"20" : ((rs_init.bw == BW_40MHZ) ? "40" :
			(rs_init.bw == BW_80MHZ) ? "80" : "80+80/160"),
			wlc->stf->op_txstreams, rs_init.active_antcfg_num,
			wlc->band->bandtype, SCB_VHT_CAP(scb), rs_init.vht_ratemask,
			SCB_HE_CAP(scb)));

		rs_init.max_rate = 0;
#if defined(WME_PER_AC_TX_PARAMS)
		if (WME_PER_AC_MAXRATE_ENAB(wrsi->pub) && SCB_WME(scb))
			rs_init.max_rate = (uint32)wrsi->wlc->wme_max_rate[ac];
#endif // endif
		need_rate_filter = !((ACREV_IS(wlc->band->phyrev, 32) ||
				ACREV_IS(wlc->band->phyrev, 33)) &&
				(wlc->pub->boardflags2 & BFL2_TXPWRCTRL_EN) == 0) &&
				!(WLCISNPHY(wlc->band) && (wlc->pub->sromrev <= 8));
		if (need_rate_filter) {
			clm_rateset =
				wlc_scb_ratesel_get_ppr_rates(wlc, rspecbw_to_bcmbw(rs_init.bw));
			if (clm_rateset)
				wlc_scb_ratesel_ppr_filter(wlc, clm_rateset, &new_rateset,
					SCB_VHT_CAP(scb));
		}

		rs_init.scb = scb;
		rs_init.rateset = &new_rateset;
		rs_init.min_rate = 0;
		rs_init.chspec = chanspec;

		wlc_ratesel_init(wrsi->rsi, &rs_init);
	}

#ifdef WL_LPC
	wlc_scb_lpc_init(wlc->wlpci, scb);
#endif // endif

#if defined(WL_MU_TX)
	for (ac = 0; ac < AC_COUNT; ac++) {
		rcb_itxs_t *rcb_itxs = SCB_RATESEL_ITXS_CUBBY(wrsi, scb, ac);
		wlc_ratesel_init_rcb_itxs(rcb_itxs);
	}
#endif /* defined(WL_MU_TX) */
}

void
wlc_scb_ratesel_init_all(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	if (wlc->scbstate == NULL) {
		return;
	}

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_scb_ratesel_init(wlc, scb);
	}

#ifdef WL_LPC
	wlc_scb_lpc_init_all(wlc->wlpci);
#endif // endif
	if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
		/* force update of 'special' SCB rate entry */
		wlc_ratelinkmem_update_rate_entry(wlc, WLC_RLM_SPECIAL_RATE_SCB(wlc), NULL, 0);
	}
}

void
wlc_scb_ratesel_init_bss(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		wlc_scb_ratesel_init(wlc, scb);
	}
#ifdef WL_LPC
	wlc_scb_lpc_init_bss(wlc->wlpci, cfg);
#endif // endif
	if (RATELINKMEM_ENAB(wlc->pub) == TRUE) {
		/* force update of 'special' SCB rate entry */
		wlc_ratelinkmem_update_rate_entry(wlc, WLC_RLM_SPECIAL_RATE_SCB(wlc), NULL, 0);
	}
}

void
wlc_scb_ratesel_rfbr(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	ASSERT(state);

	wlc_ratesel_rfbr(state);
}

void
wlc_scb_ratesel_rfbr_bss(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	rcb_t *state;
	int32 ac;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
			state = SCB_RATESEL_CUBBY(wrsi, scb, ac);
			ASSERT(state);

			wlc_ratesel_rfbr(state);
		}
	}
}

static int wlc_scb_ratesel_cubby_sz(void)
{
	return (sizeof(struct ratesel_cubby));
}

#ifdef WL11N
void wlc_scb_ratesel_rssi_enable(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	wlc_lq_sample_req_enab(scb, RX_LQ_SAMP_REQ_RATE_SEL, TRUE);
}

void wlc_scb_ratesel_rssi_disable(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	wlc_lq_sample_req_enab(scb, RX_LQ_SAMP_REQ_RATE_SEL, FALSE);
}

int wlc_scb_ratesel_get_rssi(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc = cfg->wlc;
	return wlc_lq_rssi_get(wlc, cfg, scb);
}
#endif /* WL11N */

#ifdef WL_LPC
/* External functions */
void
wlc_scb_ratesel_get_info(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac,
	uint8 rate_stab_thresh, uint32 *new_rate_kbps, bool *rate_stable,
	rate_lcb_info_t *lcb_info)
{
	rcb_t *state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	wlc_ratesel_get_info(state, rate_stab_thresh, new_rate_kbps, rate_stable, lcb_info);
	return;
}

void
wlc_scb_ratesel_reset_vals(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{
	rcb_t *state = NULL;

	if (!scb)
		return;

	state = SCB_RATESEL_CUBBY(wrsi, scb, ac);
	wlc_ratesel_lpc_init(state);
	return;
}

void
wlc_scb_ratesel_clr_cache(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{
	rcb_t *state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);
	wlc_ratesel_clr_cache(state);
	return;
}
#endif /* WL_LPC */

/* Get current CLM enabled rates bitmap */
static ppr_rateset_t *
wlc_scb_ratesel_get_ppr_rates(wlc_info_t *wlc, wl_tx_bw_t bw)
{
#ifdef WL11AC
	wlc_ratesel_info_t *wrsi = wlc->wrsi;

	if (wrsi->ppr_rates->chanspec != wlc->chanspec ||
		wrsi->ppr_rates->country != wlc_get_country(wlc) ||
		wrsi->ppr_rates->txstreams != wlc->stf->txstreams) {
		wlc_scb_ratesel_ppr_upd(wlc);
	}

	switch (bw) {
	case WL_TX_BW_20:
		return &wrsi->ppr_rates->ppr_20_rates;
	case WL_TX_BW_40:
		return &wrsi->ppr_rates->ppr_40_rates;
	case WL_TX_BW_80:
		return &wrsi->ppr_rates->ppr_80_rates;
	case WL_TX_BW_160:
		return &wrsi->ppr_rates->ppr_160_rates;
	default:
		ASSERT(0);
		return NULL;
	}
#else
	return NULL;
#endif /* WL11AC */
}

static void
wlc_scb_ratesel_get_ppr_rates_bitmp(wlc_info_t *wlc, ppr_t *target_pwrs, wl_tx_bw_t bw,
	ppr_rateset_t *rates)
{
	uint8 chain;
	ppr_vht_mcs_rateset_t mcs_limits;

	rates->vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
	rates->vht_mcsmap_prop = VHT_PROP_MCS_MAP_NONE_ALL;
	for (chain = 0; chain < PHYCORENUM(wlc->stf->op_txstreams); chain++) {
		ppr_get_vht_mcs(target_pwrs, bw, chain+1, WL_TX_MODE_NONE, chain+1, &mcs_limits);
		if (mcs_limits.pwr[0] != WL_RATE_DISABLED) {
			rates->mcs[chain] = 0xff; /* All rates are enabled for this block */
			/* Check VHT rate [8-9] */
#ifdef WL11AC
			if (WLCISACPHY(wlc->band)) {
				if (mcs_limits.pwr[9] != WL_RATE_DISABLED) {
					/* All VHT rates are enabled */
					VHT_MCS_MAP_SET_MCS_PER_SS(chain+1, VHT_CAP_MCS_MAP_0_9,
						rates->vht_mcsmap);
				} else if (mcs_limits.pwr[8] != WL_RATE_DISABLED) {
					/* VHT 0_8 are enabled */
					VHT_MCS_MAP_SET_MCS_PER_SS(chain+1, VHT_CAP_MCS_MAP_0_8,
						rates->vht_mcsmap);
				} else {
					/* VHT 8-9 are disabled in this case */
					VHT_MCS_MAP_SET_MCS_PER_SS(chain+1, VHT_CAP_MCS_MAP_0_7,
						rates->vht_mcsmap);
				}
#ifndef NO_PROPRIETARY_VHT_RATES
				/* Check VHT 10_11 */
				if (mcs_limits.pwr[11] != WL_RATE_DISABLED) {
					/* Both VHT 10_11 are enabled */
					VHT_MCS_MAP_SET_MCS_PER_SS(chain+1,
						VHT_PROP_MCS_MAP_10_11,
						rates->vht_mcsmap_prop);
				} else {
					/* VHT 10_11 are disabled in this case */
					VHT_MCS_MAP_SET_MCS_PER_SS(chain+1,
						VHT_CAP_MCS_MAP_NONE,
						rates->vht_mcsmap_prop);
				}
#endif /* !NO_PROPRIETARY_VHT_RATES */
			}
#endif /* WL11AC */
		}
	}

}

static void
wlc_scb_ratesel_ppr_updbmp(wlc_info_t *wlc, ppr_t *target_pwrs)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	wlc_scb_ratesel_get_ppr_rates_bitmp(wlc, target_pwrs, WL_TX_BW_20,
		&wrsi->ppr_rates->ppr_20_rates);
#if defined(WL11N) || defined(WL11AC)
	if (CHSPEC_BW_GE(wlc->chanspec, WL_CHANSPEC_BW_40)) {
		wlc_scb_ratesel_get_ppr_rates_bitmp(wlc, target_pwrs, WL_TX_BW_40,
			&wrsi->ppr_rates->ppr_40_rates);
	}
#endif // endif
#if defined(WL11AC)
	if (CHSPEC_BW_GE(wlc->chanspec, WL_CHANSPEC_BW_80)) {
		wlc_scb_ratesel_get_ppr_rates_bitmp(wlc, target_pwrs, WL_TX_BW_80,
			&wrsi->ppr_rates->ppr_80_rates);
	}
	if (CHSPEC_IS160(wlc->chanspec)) {
		wlc_scb_ratesel_get_ppr_rates_bitmp(wlc, target_pwrs, WL_TX_BW_160,
			&wrsi->ppr_rates->ppr_160_rates);
	}
#endif // endif
}

/* Update ppr enabled rates bitmap */
extern void
wlc_scb_ratesel_ppr_upd(wlc_info_t *wlc)
{
	clm_country_t country = wlc_get_country(wlc);

	/* update the rate bitmap with the target power offset */
	wlc_scb_ratesel_ppr_updbmp(wlc, wlc->stf->txpwr_ctl);

	wlc->wrsi->ppr_rates->country = country;
	wlc->wrsi->ppr_rates->chanspec = wlc->chanspec;
	wlc->wrsi->ppr_rates->txstreams = wlc->stf->txstreams;
}

#ifdef WLATF
/* Get the rate selection control block pointer from ratesel cubby */
rcb_t *
wlc_scb_ratesel_getrcb(wlc_info_t *wlc, struct scb *scb, uint ac)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ASSERT(wrsi);

	if (!WME_PER_AC_MAXRATE_ENAB(wlc->pub)) {
		ac = 0;
	}
	ASSERT(ac < (uint)WME_MAX_AC(wlc, scb));

	return (SCB_RATESEL_CUBBY(wrsi, scb, ac));
}

#endif /* WLATF */

static wl_tx_bw_t rspecbw_to_bcmbw(uint8 bw)
{
	wl_tx_bw_t bcmbw = WL_TX_BW_20;
	switch (bw) {
	case BW_40MHZ:
		bcmbw = WL_TX_BW_40;
		break;
	case BW_80MHZ:
		bcmbw = WL_TX_BW_80;
		break;
	case BW_160MHZ:
		bcmbw = WL_TX_BW_160;
		break;
	}

	return bcmbw;
}

void
wlc_scb_ratesel_get_ratecap(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 *sgi,
	uint16 mcs_bitmap[], uint8 ac)
{
	rcb_t *state = wlc_scb_ratesel_get_cubby(wrsi, scb, ac);

	wlc_ratesel_get_ratecap(state, sgi, mcs_bitmap);

	return;
}

#ifdef WLRSDB
/* Clear the wrsi->ppr_rates->chanspec
 * variable to force ppr update after mode switch.
 */
void wlc_scb_ratesel_chanspec_clear(wlc_info_t *wlc)
{
	wlc->wrsi->ppr_rates->chanspec = INVCHANSPEC;
	return;
}
#endif /* WLRSDB */

uint8
wlc_scb_ratesel_get_link_bw(wlc_info_t *wlc, struct scb *scb)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	rcb_t *state = SCB_RATESEL_CUBBY(wrsi, scb, LINK_BW_ENTRY);
	return wlc_ratesel_get_link_bw(state);
}

#if defined(WLSCB_HISTO)
/* array of 55 elements (0-54); elements provide mapping to index for
 *	1, 2, 5 (5.5), 6, 9, 11, 12, 18, 24, 36, 48, 54
 * eg.
 *	wlc_legacy_rate_index[1] is 0
 *	wlc_legacy_rate_index[2] is 1
 *	wlc_legacy_rate_index[5] is 2
 *	...
 *	wlc_legacy_rate_index[48] is 10
 *	wlc_legacy_rate_index[54] is 11
 * For numbers in between (eg. 3, 4, 49, 50) maps to closest lower index
 * eg.
 *	wlc_legacy_rate_index[3] is 1 (same as 2Mbps)
 *	wlc_legacy_rate_index[4] is 1 (same as 2Mbps)
 */
uint8 wlc_legacy_rate_index[] = {
	0, 0,					/* 1Mbps (same for 0-1) */
	1, 1, 1,				/* 2Mbps (same for 2-4) */
	2,					/* 5Mbps (5.5) */
	3, 3, 3,				/* 6Mbps (same for 6-8) */
	4, 4,					/* 9Mbps (same for 9-10) */
	5,					/* 11Mbps */
	6, 6, 6, 6, 6, 6,			/* 12Mbps (same for 12-17) */
	7, 7, 7, 7, 7, 7,			/* 18Mbps (same for 18-23) */
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,	/* 24Mbps (same for 24-35) */
	9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,	/* 36Mbps (same for 36-47) */
	10, 10, 10, 10, 10, 10,			/* 48Mbps (same for 48-53) */
	11					/* 54Mbps */
};

const uint8 wlc_legacy_rate_index_len =
	sizeof(wlc_legacy_rate_index) / sizeof(wlc_legacy_rate_index[0]);

/* fill the per scb rate histogram for the SCB corresponding to passed address (req->ea) */
static int
wlc_scb_get_rate_histo(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wl_rate_histo_report_t *req, void *buf, int len)
{
	struct scb *scb = NULL;
	struct scb_iter scbiter;
	wl_rate_histo_report_t *rpt = buf;
	int rpt_len = sizeof(wl_rate_histo_report_t) + sizeof(wl_rate_histo_maps1_t);
	uint8 zero_ea[sizeof(struct ether_addr)] = { 0 };

	if (req != NULL) {
		if (req->ver != WL_HISTO_VER_1) {
		    return BCME_VERSION;
		}
		if (req->type != WL_HISTO_TYPE_RATE_MAP1) {
		    return BCME_UNSUPPORTED;
		}
		if (req->length < WL_HISTO_VER_1_FIXED_LEN) {
		    return BCME_BUFTOOSHORT;
		}
		if (req->fixed_len < WL_HISTO_VER_1_FIXED_LEN) {
			return BCME_BUFTOOSHORT;
		}
	}

	if (rpt == NULL || len < rpt_len) {
		return BCME_BUFTOOSHORT;
	}

	/* find scb if peer MAC is provided and is non-zero */
	if (req != NULL && memcmp(&zero_ea, &req->ea, sizeof(zero_ea)) &&
		(scb = wlc_scbfind(wlc, bsscfg, &req->ea)) == NULL) {
		return BCME_BADADDR;
	}

	/* if peer MAC is not provided or is zero, find the first scb */
	if (scb == NULL) {
		/* if (BSSCFG_AP(bsscfg)) { ... } */
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (SCB_ASSOCIATED(scb)) {
				break;
			}
		}
		if (scb == NULL || !SCB_ASSOCIATED(scb)) {
			return BCME_NOTASSOCIATED;
		}
	}

	rpt->ver = WL_HISTO_VER_1;
	rpt->type = WL_HISTO_TYPE_RATE_MAP1;
	rpt->length = rpt_len;
	memcpy(&rpt->ea, &scb->ea, sizeof(rpt->ea));
	rpt->fixed_len = WL_HISTO_VER_1_FIXED_LEN;

	/* Allocate memory on first call */
	if (scb->histo == NULL) {
		scb->histo = (wl_rate_histo_maps1_t *) MALLOCZ(wlc->osh,
			sizeof(wl_rate_histo_maps1_t));
		if (!scb->histo) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_ERROR;
		}
	}

	/* compute delta time from start timestamp */
	scb->histo->rx.seconds = wlc->pub->now - scb->histo->rx.seconds;
	scb->histo->tx.seconds = wlc->pub->now - scb->histo->tx.seconds;
	memcpy(&rpt->data, scb->histo, sizeof(wl_rate_histo_maps1_t));

	/* reset on read */
	bzero(scb->histo, sizeof(wl_rate_histo_maps1_t));
	scb->histo->rx.seconds = scb->histo->tx.seconds = wlc->pub->now;

	return BCME_OK;
}
#endif /* WLSCB_HISTO */
