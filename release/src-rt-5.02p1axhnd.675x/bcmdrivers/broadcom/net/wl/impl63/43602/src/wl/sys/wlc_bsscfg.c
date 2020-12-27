/*
 * BSS Configuration routines for
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_bsscfg.c 767468 2018-09-12 08:14:28Z $
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
#include <proto/802.11.h>
#include <proto/wpa.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#if defined(BCMSUP_PSK) || defined(BCMCCX)
#include <proto/eapol.h>
#endif // endif
#include <bcmwpa.h>
#ifdef BCMCCX
#include <bcmcrypto/ccx.h>
#endif // endif
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc_vndr_ie_list.h>
#include <wlc.h>
#include <wlc_phy_hal.h>
#include <wlc_scb.h>
#if defined(BCMAUTH_PSK)
#include <wlc_auth.h>
#endif // endif
#include <wl_export.h>
#include <wlc_channel.h>
#include <wlc_cntry.h>
#include <wlc_ap.h>
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
#ifdef WMF
#include <wlc_wmf.h>
#endif // endif
#include <wlc_scan.h>
#include <wlc_assoc.h>
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#ifdef AP
#include <wlc_bmac.h>
#endif // endif
#include <wlc_apps.h>
#include <bcm_notif_pub.h>
#include <wlc_11h.h>
#include <wlc_rate_sel.h>
#ifdef PSTA
#include <wlc_psta.h>
#endif // endif
#include <wlc_hrt.h>
#include <wlc_11u.h>
#ifdef L2_FILTER
#include <wlc_l2_filter.h>
#endif // endif
#ifdef WLAWDL
#include <wlc_awdl.h>
#endif // endif
#ifdef WL_PWRSTATS
#include <wlc_pwrstats.h>
#endif // endif
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#ifdef BCM_HOST_MEM_SCB
#include <wlc_scb_alloc.h>
#endif // endif
#ifdef WDS
#include <wlc_wds.h>
#endif /* WDS */

#ifdef SMF_STATS
/* the status/reason codes of interest */
uint16 const smfs_sc_table[] = {
	DOT11_SC_SUCCESS,
	DOT11_SC_FAILURE,
	DOT11_SC_CAP_MISMATCH,
	DOT11_SC_REASSOC_FAIL,
	DOT11_SC_ASSOC_FAIL,
	DOT11_SC_AUTH_MISMATCH,
	DOT11_SC_AUTH_SEQ,
	DOT11_SC_AUTH_CHALLENGE_FAIL,
	DOT11_SC_AUTH_TIMEOUT,
	DOT11_SC_ASSOC_BUSY_FAIL,
	DOT11_SC_ASSOC_RATE_MISMATCH,
	DOT11_SC_ASSOC_SHORT_REQUIRED,
	DOT11_SC_ASSOC_SHORTSLOT_REQUIRED
};

uint16 const smfs_rc_table[] = {
	DOT11_RC_RESERVED,
	DOT11_RC_UNSPECIFIED,
	DOT11_RC_AUTH_INVAL,
	DOT11_RC_DEAUTH_LEAVING,
	DOT11_RC_INACTIVITY,
	DOT11_RC_BUSY,
	DOT11_RC_INVAL_CLASS_2,
	DOT11_RC_INVAL_CLASS_3,
	DOT11_RC_DISASSOC_LEAVING,
	DOT11_RC_NOT_AUTH,
	DOT11_RC_BAD_PC
};

#define MAX_SCRC_EXCLUDED	16
#endif /* SMF_STATS */

/* structure for storing per-cubby client info */
struct bsscfg_cubby_info {
	bsscfg_cubby_init_t	fn_init;	/* fn called during bsscfg malloc */
	bsscfg_cubby_deinit_t	fn_deinit;	/* fn called during bsscfg free */
	bsscfg_cubby_dump_t 	fn_dump;	/* fn called during bsscfg dump */
	void			*ctx;		/* context to be passed to all cb fns */
};
typedef struct bsscfg_cubby_info bsscfg_cubby_info_t;

/* structure for storing global bsscfg module state */
struct bsscfg_module {
	wlc_info_t			*wlc;		/* pointer to wlc */
	uint				totsize;	/* total bsscfg size including container */
	uint 				ncubby;		/* current num of cubbies */
	bsscfg_cubby_info_t			*cubby_info;	/* cubby client info */
	bcm_notif_h			up_down_notif_hdl; /* up/down notifier handle. */
	bcm_notif_h tplt_upd_notif_hdl; /* s/w bcn/prbrsp update notifier handle. */
	bcm_notif_h mute_upd_notif_hdl; /* ibss mute update notifier handle. */
	bcm_notif_h pretbtt_query_hdl;	/* pretbtt query handle. */
	bcm_notif_h as_st_notif_hdl;	/* assoc state notifier handle. */
};

/* Flags that should not be cleared on AP bsscfg up */
#define WLC_BSSCFG_PERSIST_FLAGS (0 | \
		WLC_BSSCFG_WME_DISABLE | \
		WLC_BSSCFG_PRESERVE | \
		WLC_BSSCFG_BTA | \
		WLC_BSSCFG_NOBCMC | \
		WLC_BSSCFG_NOIF | \
		WLC_BSSCFG_11N_DISABLE | \
		WLC_BSSCFG_P2P | \
		WLC_BSSCFG_11H_DISABLE | \
		WLC_BSSCFG_NATIVEIF | \
		WLC_BSSCFG_SRADAR_ENAB | \
		WLC_BSSCFG_DYNBCN | \
		WLC_BSSCFG_AP_NORADAR_CHAN | \
		WLC_BSSCFG_BSSLOAD_DISABLE | \
		WLC_BSSCFG_TX_SUPR_ENAB | \
		WLC_BSSCFG_NO_AUTHENTICATOR | \
		WLC_BSSCFG_PS_OFF_TRANS | \
	0)
/* Clear non-persistant flags; by default, HW beaconing and probe resp */
#define WLC_BSSCFG_FLAGS_INIT(cfg) do { \
		(cfg)->flags &= WLC_BSSCFG_PERSIST_FLAGS; \
		(cfg)->flags |= (WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB); \
	} while (0)

/* Flags for special bsscfgs */
#define WLC_BSSCFG_EXAMPT_FLAGS (0 | \
		WLC_BSSCFG_DPT | \
	0)

/* Local Functions */

static int wlc_bsscfg_wlc_up(void *ctx);

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int wlc_bsscfg_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif

#ifdef MBSS
static int wlc_bsscfg_macgen(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#endif // endif

static int _wlc_bsscfg_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct ether_addr *ea, uint flags, bool ap);
#if defined(AP) || defined(STA)
static void _wlc_bsscfg_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif // endif

static int wlc_bsscfg_bcmcscbinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint bandindex);

#ifdef AP
static int wlc_bsscfg_ap_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
static void wlc_bsscfg_ap_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif // endif
#ifdef STA
static int wlc_bsscfg_sta_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
static void wlc_bsscfg_sta_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif // endif

static int wlc_bsscfg_alloc_ext(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, int idx,
	struct ether_addr *ea, uint flags, bool ap);

static bool wlc_bsscfg_preserve(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

#ifdef SMF_STATS
static int wlc_bsscfg_smfsfree(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
#endif /* SMF_STATS */

static wlc_bsscfg_t *wlc_bsscfg_malloc(wlc_info_t *wlc);
static void wlc_bsscfg_mfree(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

/* module */
#define BSSCFG_CUBBY_INFO_LENGTH(wlc) (sizeof(bsscfg_cubby_info_t) * \
	wlc->pub->tunables->maxbsscfgcubbies)
#define BSSCFG_MODULE_STRUCT_LENGTH(wlc) (\
		sizeof(bsscfg_module_t) + \
		BSSCFG_CUBBY_INFO_LENGTH(wlc) + \
		0)

bsscfg_module_t *
BCMATTACHFN(wlc_bsscfg_attach)(wlc_info_t *wlc)
{
	bsscfg_module_t *bcmh;
	int len;

	len = BSSCFG_MODULE_STRUCT_LENGTH(wlc);

	if ((bcmh = MALLOCZ(wlc->osh, len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bcmh->wlc = wlc;

	bcmh->cubby_info = (bsscfg_cubby_info_t *)((uintptr)bcmh + sizeof(bsscfg_module_t));

	/* Create notification list for bsscfg up/down events. */
	if (bcm_notif_create_list(wlc->notif, &bcmh->up_down_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (updn)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Create notification list for s/w bcn/prbrsp update events. */
	if (bcm_notif_create_list(wlc->notif, &bcmh->tplt_upd_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (swbcn)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Create notification list for ibss mute update event. */
	if (bcm_notif_create_list(wlc->notif, &bcmh->mute_upd_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (mute)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Create callback list for pretbtt query. */
	if (bcm_notif_create_list(wlc->notif, &bcmh->pretbtt_query_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (pretbtt)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Create notification list for assoc state */
	if (bcm_notif_create_list(wlc->notif, &bcmh->as_st_notif_hdl) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list failed (asst)\n",
		         wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	bcmh->totsize = sizeof(wlc_bsscfg_t);

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "bsscfg", (dump_fn_t)wlc_bsscfg_dump, (void *)wlc);
#endif // endif

	if (wlc_module_register(wlc->pub, NULL, "bsscfg", bcmh, NULL,
	                        NULL, wlc_bsscfg_wlc_up, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return bcmh;

fail:
	wlc_bsscfg_detach(bcmh);
	return NULL;
}

void
BCMATTACHFN(wlc_bsscfg_detach)(bsscfg_module_t *bcmh)
{
	wlc_info_t *wlc;
	int len;

	if (bcmh == NULL)
		return;

	wlc = bcmh->wlc;

	wlc_module_unregister(wlc->pub, "bsscfg", bcmh);

	len = BSSCFG_MODULE_STRUCT_LENGTH(wlc);

	/* Delete event notification list. */
	if (bcmh->as_st_notif_hdl != NULL)
		bcm_notif_delete_list(&bcmh->as_st_notif_hdl);
	if (bcmh->pretbtt_query_hdl != NULL)
		bcm_notif_delete_list(&bcmh->pretbtt_query_hdl);
	if (bcmh->mute_upd_notif_hdl != NULL)
		bcm_notif_delete_list(&bcmh->mute_upd_notif_hdl);
	if (bcmh->tplt_upd_notif_hdl != NULL)
		bcm_notif_delete_list(&bcmh->tplt_upd_notif_hdl);
	if (bcmh->up_down_notif_hdl != NULL)
		bcm_notif_delete_list(&bcmh->up_down_notif_hdl);
	MFREE(wlc->osh, bcmh, len);
}

static int
wlc_bsscfg_wlc_up(void *ctx)
{
#ifdef STA
	bsscfg_module_t *bcmh = (bsscfg_module_t *)ctx;
	wlc_info_t *wlc = bcmh->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

#ifdef WLMCNX
	if (MCNX_ENAB(wlc->pub))
		return BCME_OK;
#endif // endif

	/* Update tsf_cfprep if associated and up */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->up) {
			uint32 bi;

			/* get beacon period from bsscfg and convert to uS */
			bi = cfg->current_bss->beacon_period << 10;
			/* update the tsf_cfprep register */
			/* since init path would reset to default value */
			W_REG(wlc->osh, &wlc->regs->tsf_cfprep, (bi << CFPREP_CBI_SHIFT));

			/* Update maccontrol PM related bits */
			wlc_set_ps_ctrl(cfg);

			break;
		}
	}
#endif /* STA */

	return BCME_OK;
}

/**
 * Multiple modules have the need of reserving some private data storage related to a specific BSS
 * configuration. During ATTACH time, this function is called multiple times, typically one time per
 * module that requires this storage. This function does not allocate memory, but calculates values
 * to be used for a future memory allocation instead. The private data is located at the end of a
 * bsscfg.
 *
 * Returns the offset of the private data to the beginning of an allocated bsscfg structure,
 * negative values are errors.
 */
int
BCMATTACHFN(wlc_bsscfg_cubby_reserve)(wlc_info_t *wlc, uint size,
	bsscfg_cubby_init_t fn_init, bsscfg_cubby_deinit_t fn_deinit,
	bsscfg_cubby_dump_t fn_dump,
	void *ctx)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	bsscfg_cubby_info_t *cubby_info;
	uint offset;

	ASSERT((bcmh->totsize % PTRSZ) == 0);

	if (bcmh->ncubby >= (uint)wlc->pub->tunables->maxbsscfgcubbies) {
		ASSERT(bcmh->ncubby < (uint)wlc->pub->tunables->maxbsscfgcubbies);
		return BCME_NORESOURCE;
	}

	/* housekeeping info is stored in bsscfg_module struct */
	cubby_info = &bcmh->cubby_info[bcmh->ncubby];

	cubby_info->fn_init = fn_init;
	cubby_info->fn_deinit = fn_deinit;
	cubby_info->fn_dump = fn_dump;
	cubby_info->ctx = ctx;

	bcmh->ncubby++;

	/* actual cubby data is stored at the end of bsscfg's */
	offset = bcmh->totsize;

	/* roundup to pointer boundary */
	bcmh->totsize = ROUNDUP(bcmh->totsize + size, PTRSZ);

	return offset;
}

/**
 * wlc_bsscfg_updown_register()
 *
 * This function registers a callback that will be invoked when either a bsscfg
 * up or down event occurs.
 *
 * Parameters
 *    wlc       Common driver context.
 *    callback  Callback function  to invoke on up/down events.
 *    arg       Client specified data that will provided as param to the callback.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
int
BCMATTACHFN(wlc_bsscfg_updown_register)(wlc_info_t *wlc, bsscfg_up_down_fn_t callback, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->up_down_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)callback, arg);
}

/**
 * wlc_bsscfg_updown_unregister()
 *
 * This function unregisters a bsscfg up/down event callback.
 *
 * Parameters
 *    wlc       Common driver context.
 *    callback  Callback function that was previously registered.
 *    arg       Client specified data that was previously registerd.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
int
BCMATTACHFN(wlc_bsscfg_updown_unregister)(wlc_info_t *wlc, bsscfg_up_down_fn_t callback, void *arg)
{
	bcm_notif_h hdl;
	if (!wlc->bcmh)
		return BCME_OK;
	hdl = wlc->bcmh->up_down_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)callback, arg);
}

/* These functions register/unregister/invoke the callback
 * when bcn or prbresp template needs to be updated for the bsscfg that is of
 * WLC_BSSCFG_SW_BCN or WLC_BSSCFG_SW_PRB.
 */
int
wlc_bss_tplt_upd_register(wlc_info_t *wlc, bss_tplt_upd_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->tplt_upd_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
wlc_bss_tplt_upd_unregister(wlc_info_t *wlc, bss_tplt_upd_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->tplt_upd_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

void
wlc_bss_tplt_upd_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int type)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	bss_tplt_upd_data_t notif_data;

	notif_data.cfg = cfg;
	notif_data.type = type;
	bcm_notif_signal(bcmh->tplt_upd_notif_hdl, &notif_data);
}

/**
 * These functions register/unregister/invoke the callback
 * when an IBSS is requested to be muted/unmuted.
 */
int
wlc_ibss_mute_upd_register(wlc_info_t *wlc, ibss_mute_upd_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->mute_upd_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
wlc_ibss_mute_upd_unregister(wlc_info_t *wlc, ibss_mute_upd_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->mute_upd_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

void
wlc_ibss_mute_upd_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool mute)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	ibss_mute_upd_data_t notif_data;

	notif_data.cfg = cfg;
	notif_data.mute = mute;
	bcm_notif_signal(bcmh->mute_upd_notif_hdl, &notif_data);
}

/**
 * These functions register/unregister/invoke the callback
 * when a pretbtt query is requested.
 */
int
BCMATTACHFN(wlc_bss_pretbtt_query_register)(wlc_info_t *wlc, bss_pretbtt_query_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->pretbtt_query_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
BCMATTACHFN(wlc_bss_pretbtt_query_unregister)(wlc_info_t *wlc, bss_pretbtt_query_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->pretbtt_query_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

static void
wlc_bss_pretbtt_max(uint *max_pretbtt, bss_pretbtt_query_data_t *notif_data)
{
	if (notif_data->pretbtt > *max_pretbtt)
		*max_pretbtt = notif_data->pretbtt;
}

uint
wlc_bss_pretbtt_query(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint minval)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	bss_pretbtt_query_data_t notif_data;
	uint max_pretbtt = minval;

	notif_data.cfg = cfg;
	notif_data.pretbtt = minval;
	bcm_notif_signal_ex(bcmh->pretbtt_query_hdl, &notif_data,
	                    (bcm_notif_server_callback)wlc_bss_pretbtt_max,
	                    (bcm_notif_client_data)&max_pretbtt);

	return max_pretbtt;
}

/**
 * These functions register/unregister/invoke the callback
 * when an association state has changed.
 */
int
wlc_bss_assoc_state_register(wlc_info_t *wlc, bss_assoc_state_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->as_st_notif_hdl;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
wlc_bss_assoc_state_unregister(wlc_info_t *wlc, bss_assoc_state_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->bcmh->as_st_notif_hdl;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

void
wlc_bss_assoc_state_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint type, uint state)
{
	bcm_notif_h hdl = wlc->bcmh->as_st_notif_hdl;
	bss_assoc_state_data_t notif_data;

	notif_data.cfg = cfg;
	notif_data.type = type;
	notif_data.state = state;
	bcm_notif_signal(hdl, &notif_data);
}

/** Return the number of AP bsscfgs that are UP */
int
wlc_ap_bss_up_count(wlc_info_t *wlc)
{
	uint16 i, apbss_up = 0;
	wlc_bsscfg_t *bsscfg;

	FOREACH_UP_AP(wlc, i, bsscfg) {
		apbss_up++;
	}

	return apbss_up;
}

/** Return the number of PSTA bsscfgs */
#ifdef PSTA
int
wlc_psta_bss_count(wlc_info_t *wlc)
{
	uint16 i, psta_bss = 0;
	wlc_bsscfg_t *bsscfg;

	FOREACH_PSTA(wlc, i, bsscfg) {
		psta_bss++;
	}

	return psta_bss;
}
#endif /* PSTA */

#ifdef MBSS
/**
 * Allocate and set up a software packet template
 * @param count The number of packets to use; must be <= WLC_SPT_COUNT_MAX
 * @param len The length of the packets to be allocated
 *
 * Returns 0 on success, < 0 on error.
 */

static int
wlc_spt_init(wlc_info_t *wlc, wlc_spt_t *spt, int count, int len)
{
	int idx;
	int tso_hdr_overhead = (wlc->toe_bypass ? 0 : sizeof(d11ac_tso_t));

	/* Pad for header overhead */
	len += tso_hdr_overhead;

	ASSERT(spt != NULL);
	ASSERT(count <= WLC_SPT_COUNT_MAX);

	for (idx = 0; idx < count; idx++) {
		if (spt->pkts[idx] == NULL &&
		    (spt->pkts[idx] = PKTGET(wlc->osh, len, TRUE)) == NULL) {
			return -1;
		}
	}

	spt->latest_idx = -1;

	return 0;
}

/**
 * Clean up a software template object;
 * if pkt_free_force is TRUE, will not check if the pkt is in use before freeing.
 * Note that if "in use", the assumption is that some other routine owns
 * the packet and will free appropriately.
 */

static void
wlc_spt_deinit(wlc_info_t *wlc, wlc_spt_t *spt, int pkt_free_force)
{
	int idx;

	if (spt != NULL) {
		for (idx = 0; idx < WLC_SPT_COUNT_MAX; idx++) {
			if (spt->pkts[idx] != NULL) {
				if (pkt_free_force || !SPT_IN_USE(spt, idx)) {
					PKTFREE(wlc->osh, spt->pkts[idx], TRUE);
					spt->pkts[idx] = NULL;
				} else {
					WLPKTFLAG_BSS_DOWN_SET(WLPKTTAG(spt->pkts[idx]), TRUE);
				}
			}
		}
		bzero(spt, sizeof(*spt));
	}
}

static void
mbss_ucode_set(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bool cur_val, new_val;

	/* Assumes MBSS_EN has same value in all cores */
	cur_val = ((wlc_mhf_get(wlc, MHF1, WLC_BAND_AUTO) & MHF1_MBSS_EN) != 0);
	new_val = (MBSS_ENAB(wlc->pub) != 0);

	if (cur_val != new_val) {
		wlc_suspend_mac_and_wait(wlc);
		/* enable MBSS in uCode */
		WL_MBSS(("%s MBSS mode\n", new_val ? "Enabling" : "Disabling"));
		(void)wlc_mhf(wlc, MHF1, MHF1_MBSS_EN, new_val ? MHF1_MBSS_EN : 0, WLC_BAND_ALL);
		wlc_enable_mac(wlc);
	}
}

/** BCMC_FID_SHM_COMMIT - Committing FID to SHM; move driver's value to bcmc_fid_shm */
void
bcmc_fid_shm_commit(wlc_bsscfg_t *bsscfg)
{
	bsscfg->bcmc_fid_shm = bsscfg->bcmc_fid;
	bsscfg->bcmc_fid = INVALIDFID;
}

/** BCMC_FID_INIT - Set driver and shm FID to invalid */
#define BCMC_FID_INIT(bsscfg) do { \
		if (!(bsscfg->flags & WLC_BSSCFG_PS_OFF_TRANS)) { \
			(bsscfg)->bcmc_fid = INVALIDFID; \
			(bsscfg)->bcmc_fid_shm = INVALIDFID; \
		} \
	} while (0)

static int
mbss_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int result = 0;
	int idx;
	wlc_bsscfg_t *bsscfg;
	int  bcn_tmpl_len;

	bcn_tmpl_len = wlc->pub->bcn_tmpl_len;

	/* Assumes MBSS is enabled for this BSS config herein */

	/* Set pre TBTT interrupt timer to 10 ms for now; will be shorter */
	wlc_write_shm(wlc, SHM_MBSS_PRE_TBTT, wlc->ap->pre_tbtt_us);

	/* if the BSS configs hasn't been given a user defined address or
	 * the address is duplicated, we'll generate our own.
	 */
	FOREACH_BSS(wlc, idx, bsscfg) {
		if (bsscfg == cfg)
			continue;
		if (bcmp(&bsscfg->cur_etheraddr, &cfg->cur_etheraddr, ETHER_ADDR_LEN) == 0)
			break;
	}
	if (ETHER_ISNULLADDR(&cfg->cur_etheraddr) || idx < WLC_MAXBSSCFG) {
		result = wlc_bsscfg_macgen(wlc, cfg);
		if (result != BCME_OK) {
			WL_ERROR(("wl%d.%d: %s: unable to generate MAC address\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return result;
		}
	}

	/* Set the uCode index of this config */
	cfg->_ucidx = EADDR_TO_UC_IDX(cfg->cur_etheraddr, wlc->mbss_ucidx_mask);
	ASSERT(cfg->_ucidx <= wlc->mbss_ucidx_mask);
	wlc->hw2sw_idx[cfg->_ucidx] = WLC_BSSCFG_IDX(cfg);

	/* Allocate DMA space for beacon software template */
	result = wlc_spt_init(wlc, cfg->bcn_template, BCN_TEMPLATE_COUNT, bcn_tmpl_len);
	if (result != BCME_OK) {
		WL_ERROR(("wl%d.%d: %s: unable to allocate beacon templates",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return result;
	}
	/* Set the BSSCFG index in the packet tag for beacons */
	for (idx = 0; idx < BCN_TEMPLATE_COUNT; idx++) {
		WLPKTTAGBSSCFGSET(cfg->bcn_template->pkts[idx], WLC_BSSCFG_IDX(cfg));
	}

	/* Make sure that our SSID is in the correct uCode
	 * SSID slot in shared memory
	 */
	wlc_shm_ssid_upd(wlc, cfg);

	BCMC_FID_INIT(cfg);

	cfg->flags &= ~(WLC_BSSCFG_SW_BCN | WLC_BSSCFG_SW_PRB);
	cfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
	if (!MBSS_ENAB16(wlc->pub)) {
		cfg->flags |= (WLC_BSSCFG_SW_BCN | WLC_BSSCFG_SW_PRB);
	} else {
		cfg->flags |= (WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
	}

	return result;
}
#endif /* MBSS */

int
wlc_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int ret = BCME_OK;
	bsscfg_module_t *bcmh = wlc->bcmh;
	bsscfg_up_down_event_data_t evt_data;
	bool stop = FALSE;

	ASSERT(cfg != NULL);
	ASSERT(cfg->enable);

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_up(%s): stas/aps/associated %d/%d/%d"
			"flags = 0x%x\n", wlc->pub->unit, (BSSCFG_AP(cfg) ? "AP" : "STA"),
			wlc->stas_associated, wlc->aps_associated, wlc->pub->associated,
			cfg->flags));

#ifdef AP
	if (BSSCFG_AP(cfg)) {
		bool radar_chan;
#ifdef STA
		bool mpc_out = wlc->mpc_out;
#endif // endif

#ifdef STA
		/* bringup the driver */
		wlc->mpc_out = TRUE;
		wlc_radio_mpc_upd(wlc);
#endif // endif

		/* AP mode operation must have the driver up before bringing
		 * up a configuration
		 */
		if (!wlc->pub->up) {
			ret = BCME_NOTUP;
			goto end;
		}

		/* wlc_ap_up() only deals with getting cfg->target_bss setup correctly.
		 * This should not have any affects that need to be undone even if we
		 * don't end up bring the AP up.
		 */
		ret = wlc_ap_up(wlc->ap, cfg);
		if (ret != BCME_OK)
			goto end;

		radar_chan = wlc_radar_chanspec(wlc->cmi, cfg->target_bss->chanspec);

		/* for softap and extap, following special radar rules */
		/* return bad channel error if radar channel */
		/* when no station associated */
		/* won't allow soft/ext ap to be started on radar channel */
		if (BSS_11H_SRADAR_ENAB(wlc, cfg) &&
		    radar_chan &&
		    !wlc->stas_associated) {
			WL_ERROR(("no assoc STA and starting soft or ext AP on radar channel %d\n",
			  CHSPEC_CHANNEL(cfg->target_bss->chanspec)));
			cfg->up = FALSE;
			ret = BCME_BADCHAN;
			goto end;
		}

		/* for softap and extap with AP_NORADAR_CHAN flag set, don't allow
		 * bss to start if on a radar channel.
		 */
		if (BSS_11H_AP_NORADAR_CHAN_ENAB(wlc, cfg) && radar_chan) {
			WL_ERROR(("AP_NORADAR_CHAN flag set, disallow ap on radar channel %d\n",
			          CHSPEC_CHANNEL(cfg->target_bss->chanspec)));
			cfg->up = FALSE;
			ret = BCME_BADCHAN;
			goto end;
		}

		/* No SSID configured yet... */
		if (cfg->SSID_len == 0) {
			cfg->up = FALSE;
			/* XXX Do not return an error for this case.  MacOS UTF
			 * tests first enable the bsscfg and then set its SSID.
			 */
			goto end;
		}

#ifdef STA
		/* defer to any STA association in progress */
		if (APSTA_ENAB(wlc->pub) && !wlc_apup_allowed(wlc)) {
			WL_APSTA_UPDN(("wl%d: wlc_bsscfg_up: defer AP UP, STA associating: "
				       "stas/aps/associated %d/%d/%d, assoc_state/type %d/%d\n",
				       wlc->pub->unit, wlc->stas_associated, wlc->aps_associated,
				       wlc->pub->associated, cfg->assoc->state, cfg->assoc->type));
			cfg->up = FALSE;
			stop = TRUE;
			ret = BCME_OK;
			goto end;
		}
#endif /* STA */

		/* it's ok to update beacon from onwards */
		/* bsscfg->flags &= ~WLC_BSSCFG_DEFER_BCN; */
		/* will be down next anyway... */

		/* Init (non-persistant) flags */
		WLC_BSSCFG_FLAGS_INIT(cfg);
		if (cfg->flags & WLC_BSSCFG_DYNBCN)
			cfg->flags &= ~WLC_BSSCFG_HW_BCN;

		WL_APSTA_UPDN(("wl%d: wlc_bsscfg_up(%s): flags = 0x%x\n",
			wlc->pub->unit, (BSSCFG_AP(cfg) ? "AP" : "STA"), cfg->flags));

#ifdef MBSS
		if (MBSS_ENAB(wlc->pub)) {
			if ((ret = mbss_bsscfg_up(wlc, cfg)) != BCME_OK)
				goto end;

			/* XXX JFH - This is redundant right now, we're also
			 * writing the MAC in wlc_BSSinit() but we want this
			 * to be done prior to enabling MBSS per George
			 * We should probably be using wlc_set_mac() here..?
			 */
			wlc_write_mbss_basemac(wlc, &wlc->vether_base);
		}
		mbss_ucode_set(wlc, cfg);
#endif /* MBSS */

		cfg->up = TRUE;

		wlc_bss_up(wlc->ap, cfg);
#if defined(WL11K_AP) && defined(WL11K)
		rrm_add_pilot_timer(wlc, cfg);
#endif // endif

		if (WLEXTSTA_ENAB(wlc->pub)) {
			chanspec_t chanspec = wlc_get_home_chanspec(cfg);
			/* indicate AP starting with channel spec info */
			wlc_bss_mac_event(wlc, cfg, WLC_E_AP_STARTED, NULL,
				WLC_E_STATUS_SUCCESS, 0, 0, &chanspec, sizeof(chanspec));
		}

	end:
		if (cfg->up)
			WL_INFORM(("wl%d: BSS %d is up\n", wlc->pub->unit, cfg->_idx));
#ifdef STA
		wlc->mpc_out = mpc_out;
		wlc_radio_mpc_upd(wlc);
		wlc_set_wake_ctrl(wlc);
#endif // endif
	}
#endif /* AP */

#ifdef STA
	if (BSSCFG_STA(cfg)) {
		cfg->up = TRUE;
	}
#endif // endif
#ifdef WL11AC
	if (BSSCFG_AP(cfg) || !cfg->BSS) {
		wlc_bss_info_t *bi = cfg->current_bss;
		bi->flags2 &= ~(WLC_BSS_VHT | WLC_BSS_80MHZ | WLC_BSS_SGI_80);
		if (BSS_VHT_ENAB(wlc, cfg)) {
			bi->flags2 |= WLC_BSS_VHT;
			if (CHSPEC_IS80(bi->chanspec)) {
				bi->flags2 |= WLC_BSS_80MHZ;
				if (bi->vht_capabilities & VHT_CAP_INFO_SGI_80MHZ) {
					bi->flags2 |= WLC_BSS_SGI_80;
				}
			}
		}
	}
#endif // endif

	if (stop || ret != BCME_OK)
		return ret;

	/* invoke bsscfg up callbacks */
	memset(&evt_data, 0, sizeof(evt_data));
	evt_data.bsscfg = cfg;
	evt_data.up     = TRUE;
	bcm_notif_signal(bcmh->up_down_notif_hdl, &evt_data);

	/* presence of multiple active bsscfgs changes defkeyvalid, so update on bsscfg up/down */
	if (wlc->pub->up) {
		uint16 defkeyvalid = wlc_key_defkeyflag(wlc);

		wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, defkeyvalid, WLC_BAND_ALL);
		WL_WSEC(("wl%d: %s: updating MHF1_DEFKEYVALID to %d\n",
		         wlc->pub->unit, __FUNCTION__, defkeyvalid != 0));
	}

	return ret;
}

/** Enable: always try to force up */
int
wlc_bsscfg_enable(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
#ifdef WLAWDL
	WL_APSTA_UPDN(("wl%d.%d: wlc_bsscfg_enable %p currently %s\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), bsscfg,
	          (bsscfg->enable ? "ENABLED" : "DISABLED")));
#else
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_enable %p currently %s\n",
	          wlc->pub->unit, bsscfg, (bsscfg->enable ? "ENABLED" : "DISABLED")));
#endif // endif

	ASSERT(bsscfg != NULL);

	if (!MBSS_ENAB(wlc->pub)) {
		/* block simultaneous multiple AP connection */
		if (BSSCFG_AP(bsscfg) && AP_ACTIVE(wlc)) {
			WL_ERROR(("wl%d: Cannot enable multiple AP bsscfg\n", wlc->pub->unit));
			return BCME_ERROR;
		}

		/* block simultaneous IBSS and AP connection */
		if (BSSCFG_AP(bsscfg)) {
			uint8 ibss_bsscfgs = wlc->ibss_bsscfgs;
#if defined(WLAWDL) || defined(WL_PROXDETECT)
			int idx;
			wlc_bsscfg_t *bc = NULL;
			/* infrastructure STA(BSS) can co-exist with AWDL STA(IBSS) */
			/* and PROXD STA(IBSS) */
			FOREACH_AS_STA(wlc, idx, bc) {
				if (BSSCFG_AWDL(wlc, bc) || BSS_PROXD_ENAB(wlc, bc) ||
					BSSCFG_NAN(bc))
					ibss_bsscfgs--;
			}
#endif // endif
			if (ibss_bsscfgs) {
				WL_ERROR(("wl%d: Cannot enable AP bsscfg with a IBSS\n",
					wlc->pub->unit));
				return BCME_ERROR;
			}
		}
	}

	bsscfg->enable = TRUE;

	if (BSSCFG_AP(bsscfg)) {
#ifdef MBSS
		/* make sure we don't exceed max */
		if (MBSS_ENAB16(wlc->pub) &&
		    ((uint32)AP_BSS_UP_COUNT(wlc) >= wlc->max_ap_bss)) {
			bsscfg->enable = FALSE;
			WL_ERROR(("wl%d: max %d ap bss allowed\n",
			          wlc->pub->unit, wlc->max_ap_bss));
			return BCME_ERROR;
		}
#endif /* MBSS */

		return wlc_bsscfg_up(wlc, bsscfg);
	}

	/* wlc_bsscfg_up() will be called for STA assoication code:
	 * - for IBSS, in wlc_join_start_ibss() and in wlc_join_BSS()
	 * - for BSS, in wlc_assoc_complete()
	 */
	/*
	 * if (BSSCFG_STA(bsscfg)) {
	 *	return BCME_OK;
	 * }
	 */

	return BCME_OK;
}

static void
wlc_bsscfg_down_cbpend_sum(uint *cbpend_sum, bsscfg_up_down_event_data_t *notif_data)
{
	*cbpend_sum = *cbpend_sum + notif_data->callbacks_pending;
}

int
wlc_bsscfg_down(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int callbacks = 0;
	bsscfg_module_t *bcmh = wlc->bcmh;
	bsscfg_up_down_event_data_t evt_data;
	uint cbpend_sum = 0;

	ASSERT(cfg != NULL);

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_down %p currently %s %s; stas/aps/associated %d/%d/%d\n",
	          wlc->pub->unit, cfg, (cfg->up ? "UP" : "DOWN"), (BSSCFG_AP(cfg) ? "AP" : "STA"),
	          wlc->stas_associated, wlc->aps_associated, wlc->pub->associated));

	if (!cfg->up) {
		/* Are we in the process of an association? */
#ifdef STA
		if ((BSSCFG_STA(cfg) && cfg->assoc->state != AS_IDLE))
			wlc_assoc_abort(cfg);
#endif /* STA */
#ifdef AP
		if (BSSCFG_AP(cfg) && cfg->associated) {
			/* For AP, cfg->up can be 0 but down never called.
			 * Thus, it's best to check for both !up and !associated
			 * before we decide to skip the down procedures.
			 */
			WL_APSTA_UPDN(("wl%d: AP cfg up = %d but associated, "
			               "continue with down procedure.\n",
			               wlc->pub->unit, cfg->up));
		}
		else
#endif // endif
		return callbacks;
	}

	if (!wlc_bsscfg_preserve(wlc, cfg)) {
		/* invoke bsscfg down callbacks */
		memset(&evt_data, 0, sizeof(evt_data));
		evt_data.bsscfg = cfg;
		bcm_notif_signal_ex(bcmh->up_down_notif_hdl, &evt_data,
			(bcm_notif_server_callback)wlc_bsscfg_down_cbpend_sum,
			(bcm_notif_server_context)&cbpend_sum);
		/* Clients update the number of pending asynchronous callbacks in the
		 * driver down path.
		 */
		callbacks += cbpend_sum;
	}

#ifdef AP
	if (BSSCFG_AP(cfg)) {

		/* bring down this config */
		cfg->up = FALSE;
#if defined(WL11K_AP)
		wl_del_timer(wlc->wl, cfg->pilot_timer);
#endif // endif
		callbacks += wlc_ap_down(wlc->ap, cfg);
#ifdef MBSS
		{
		uint clear_len = FALSE;
		wlc_bsscfg_t *bsscfg;
		uint8 ssidlen = cfg->SSID_len;
		uint i;

		wlc_spt_deinit(wlc, cfg->bcn_template, FALSE);

		if (cfg->probe_template != NULL) {
			PKTFREE(wlc->osh, cfg->probe_template, TRUE);
			cfg->probe_template = NULL;
		}

#ifdef WLLPRS
		if (cfg->lprs_template != NULL) {
			PKTFREE(wlc->osh, cfg->lprs_template, TRUE);
			cfg->lprs_template = NULL;
		}
#endif /* WLLPRS */

		/* If we clear ssid length of all bsscfgs while doing
		 * a wl down the ucode can get into a state where it
		 * will keep searching  for non-zero ssid length thereby
		 * causing mac_suspend_and_wait messages. To avoid that
		 * we will prevent clearing the ssid len of at least one BSS
		 */
		FOREACH_BSS(wlc, i, bsscfg) {
			if (bsscfg->up) {
				clear_len = TRUE;
				break;
			}
		}
		if (clear_len) {
			cfg->SSID_len = 0;

			/* update uCode shared memory */
			wlc_shm_ssid_upd(wlc, cfg);
			cfg->SSID_len = ssidlen;

			/* Clear the HW index */
			wlc->hw2sw_idx[cfg->_ucidx] = WLC_BSSCFG_IDX_INVALID;
		}
		}
#endif /* MBSS */

#ifdef BCMAUTH_PSK
		if (BCMAUTH_PSK_ENAB(wlc->pub) && (cfg->authenticator != NULL))
			wlc_authenticator_down(cfg->authenticator);
		else
			wlc_key_remove_all(wlc, cfg);
#else
		wlc_key_remove_all(wlc, cfg);
#endif // endif

		if (!AP_ACTIVE(wlc) && wlc->pub->up) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_ap_ctrl(wlc, FALSE, cfg, -1);
			wlc_enable_mac(wlc);
#ifdef STA
			if (APSTA_ENAB(wlc->pub)) {
				int idx;
				wlc_bsscfg_t *bc;
				FOREACH_AS_STA(wlc, idx, bc) {
					if (bc != wlc->cfg)
						continue;
					WL_APSTA_UPDN(("wl%d: wlc_bsscfg_down: last AP down,"
					               "sync STA: assoc_state %d type %d\n",
					               wlc->pub->unit, bc->assoc->state,
					               bc->assoc->type));
					/* We need to update tsf due to moving from APSTA -> STA.
					 * This is needed only for non-P2P ucode
					 * If not in idle mode then roam will update us.
					 * Otherwise unaligned-tbtt recovery will update tsf.
					 */
					if (!MCNX_ENAB(wlc->pub) && bc->assoc->state == AS_IDLE) {
						ASSERT(bc->assoc->type == AS_NONE);
						wlc_assoc_change_state(bc, AS_SYNC_RCV_BCN);
					}
				}
			}
#endif /* STA */
		}

#ifdef STA
		wlc_radio_mpc_upd(wlc);
		wlc_set_wake_ctrl(wlc);
#endif // endif
	}
#endif /* AP */

#ifdef STA
	if (BSSCFG_STA(cfg)) {
		/* cancel any apsd trigger timer */
		if (!wl_del_timer(wlc->wl, cfg->pm->apsd_trigger_timer))
			callbacks++;
		/* cancel any pspoll timer */
		if (!wl_del_timer(wlc->wl, cfg->pm->pspoll_timer))
			callbacks ++;
		/* cancel any roam timer */
		if (!wl_del_timer(wlc->wl, cfg->roam->timer))
			callbacks ++;
		cfg->roam->timer_active = FALSE;

		/* abort any assocaitions or roams in progress */
		callbacks += wlc_assoc_abort(cfg);

		/* delete the pm2_ret_timer object */
		if (cfg->pm->pm2_ret_timer != NULL) {
			wlc_hrt_del_timeout(cfg->pm->pm2_ret_timer);
		}
		/* delete the pm2_recv_timer object */
		if (cfg->pm->pm2_rcv_timer != NULL) {
			wlc_hrt_del_timeout(cfg->pm->pm2_rcv_timer);
		}

		cfg->up = FALSE;
	}
#endif /* STA */

#ifdef WL_BSSCFG_TX_SUPR
	if (cfg->psq != NULL)
		pktq_flush(wlc->osh, cfg->psq, TRUE, NULL, 0);
#endif // endif

	/* presence of multiple active bsscfgs changes defkeyvalid, so update on bsscfg up/down */
	if (wlc->pub->up) {
		uint16 defkeyvalid = wlc_key_defkeyflag(wlc);

		wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, defkeyvalid, WLC_BAND_ALL);
		WL_WSEC(("wl%d: %s: updating MHF1_DEFKEYVALID to %d\n",
		         wlc->pub->unit, __FUNCTION__, defkeyvalid != 0));
	}
#ifdef WL_GLOBAL_RCLASS
	if (BSSCFG_AP(cfg)) {
		cfg->scb_without_gbl_rclass = 0;
	}
#endif /* WL_GLOBAL_RCLASS */
	return callbacks;
}

int
wlc_bsscfg_disable(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int callbacks = 0;

	ASSERT(bsscfg != NULL);

#ifdef WLAWDL
	WL_APSTA_UPDN(("wl%d.%d: wlc_bsscfg_disable %p currently %s\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), bsscfg,
	          (bsscfg->enable ? "ENABLED" : "DISABLED")));
#else
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_disable %p currently %s\n",
	          wlc->pub->unit, bsscfg, (bsscfg->enable ? "ENABLED" : "DISABLED")));
#endif // endif

	/* If a bss is already disabled, don't do anything */
	if (!bsscfg->enable) {
		ASSERT(!bsscfg->up);
		return 0;
	}

	callbacks += wlc_bsscfg_down(wlc, bsscfg);
	ASSERT(!bsscfg->up);

#ifdef STA
	if (BSSCFG_STA(bsscfg)) {
		/* WES FIXME: we need to fix wlc_disassociate_client() to work when down
		 * For now do lame update of associated flags.
		 */
		if (bsscfg->associated) {
			if (wlc->pub->up && !BSSCFG_AWDL(wlc, bsscfg)) {
				wlc_disassociate_client(bsscfg, !bsscfg->block_disassoc_tx);
			} else {
				wlc_sta_assoc_upd(bsscfg, FALSE);
			}
		}
#ifdef WLMCHAN
		else if (MCHAN_ENAB(wlc->pub) && (bsscfg->chan_context != NULL)) {
			WL_MCHAN(("wl%d.%d: %s: Delete chanctx for cfg disable, not associated\n",
			          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
			wlc_mchan_delete_bss_chan_context(wlc, bsscfg);
		}
#endif /* WLMCHAN */

		/* make sure we don't retry */
		if (bsscfg->assoc != NULL) {
			wlc_assoc_t *as = bsscfg->assoc;
			if (as->timer != NULL) {
				if (!wl_del_timer(wlc->wl, as->timer)) {
					as->rt = FALSE;
					callbacks ++;
				}
			}
		}
	}
#endif /* STA */

#ifdef PSTA
	if (PSTA_ENAB(wlc->pub)) {
		if (bsscfg == wlc_bsscfg_primary(wlc)) {
			wlc_psta_disable_all(wlc->psta);
		} else if (BSSCFG_PSTA(bsscfg)) {
			wlc_psta_disable(wlc->psta, bsscfg);
		}
	}
#endif /* PSTA */

	bsscfg->flags &= ~WLC_BSSCFG_PRESERVE;

	bsscfg->enable = FALSE;

	wlc_scb_bsscfg_scbclear(wlc, bsscfg, FALSE);

	return callbacks;
}

#ifdef WME
static void
wlc_bsscfg_wme_initparams(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_wme_t *wme = cfg->wme;

	ASSERT(wme != NULL);

	if (BSSCFG_AP(cfg)) {
		wlc_wme_initparams_ap(wlc->ap, &wme->wme_param_ie);
		wlc_wme_initparams_sta(wlc, wme->wme_param_ie_ad);
		return;
	}
	wlc_wme_initparams_sta(wlc, &wme->wme_param_ie);
}
#endif /* WME */

static int
wlc_bsscfg_cubby_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	uint i;
	bsscfg_cubby_info_t *cubby_info;
	int err;

	for (i = 0; i < bcmh->ncubby; i++) {
		cubby_info = &bcmh->cubby_info[i];
		if (cubby_info->fn_init != NULL &&
		    (err = (cubby_info->fn_init)(cubby_info->ctx, cfg)) != BCME_OK) {
			WL_ERROR(("wl%d.%d: %s: cubby init failed at entry %p\n",
			          wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
			          __FUNCTION__, cubby_info->fn_init));
			return err;
		}
	}

	return BCME_OK;
}

static void
wlc_bsscfg_cubby_deinit(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	uint i;
	bsscfg_cubby_info_t *cubby_info;

	for (i = 0; i < bcmh->ncubby; i++) {
		uint j = bcmh->ncubby - 1 - i;
		cubby_info = &bcmh->cubby_info[j];
		if (cubby_info->fn_deinit != NULL) {
			(cubby_info->fn_deinit)(cubby_info->ctx, cfg);
		}
	}
}

#ifdef AP
/** Disable and free all but the primary cfg */
void
wlc_bsscfg_disablemulti(wlc_info_t *wlc)
{
	int i;
	wlc_bsscfg_t * bsscfg;

	/* iterate along the ssid cfgs */
	for (i = 1; i < WLC_MAXBSSCFG; i++) {
		if ((bsscfg = WLC_BSSCFG(wlc, i))) {
			wlc_bsscfg_disable(wlc, bsscfg);
			wlc_bsscfg_free(wlc, bsscfg);
		}
	}
}

#ifdef WL11K_AP
static void
wlc_pilot_timer(void *arg)
{
#ifdef WL11K_ALL_MEAS
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = cfg->wlc;
	struct dot11_action_frmhdr *action_hdr;
	dot11_mprep_t *mp_rep;
	void *p = NULL;
	uint8 *pbody;
	unsigned int len;
	const char *country_str;

	if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
		return;

	len = DOT11_ACTION_FRMHDR_LEN + DOT11_MPREP_LEN;
	p = wlc_frame_get_mgmt(wlc, FC_ACTION, &ether_bcast,
		&cfg->cur_etheraddr, &cfg->BSSID, len, &pbody);

	if (p == NULL) {
		WL_ERROR(("%s: failed to get mgmt frame\n", __FUNCTION__));
		return;
	}
	action_hdr = (struct dot11_action_frmhdr *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_PUBLIC;
	action_hdr->action = DOT11_PUB_ACTION_MP;
	mp_rep = (dot11_mprep_t *)&action_hdr->data[0];
	memset(mp_rep, 0, DOT11_MPREP_LEN);

	if (BSS_WL11H_ENAB(wlc, cfg))
		mp_rep->cap_info |= DOT11_MP_CAP_SPECTRUM;
	if (wlc->band->gmode && wlc->shortslot)
		mp_rep->cap_info |= DOT11_MP_CAP_SHORTSLOT;

	country_str = wlc_get_country_string(wlc, wlc->cntry);
	mp_rep->country[0] = country_str[0];
	mp_rep->country[1] = country_str[1];

	mp_rep->opclass = wlc_get_regclass(wlc->cmi, cfg->current_bss->chanspec);
	mp_rep->channel = CHSPEC_CHANNEL(cfg->current_bss->chanspec);
	mp_rep->mp_interval = cfg->mp_period;

	if (p != NULL) {
		wlc_sendmgmt(wlc, p, cfg->wlcif->qi, NULL);
	}
#endif /* WL11K_ALL_MEAS */
}
#endif /* WL11K_AP */

static int
wlc_bsscfg_ap_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
#ifdef WME
	wlc_wme_t *wme = bsscfg->wme;
#endif // endif

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_ap_init: bsscfg %p\n", wlc->pub->unit, bsscfg));

	/* Init flags: Beacons/probe resp in HW by default */
	bsscfg->flags |= (WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
	if (bsscfg->flags & WLC_BSSCFG_DYNBCN)
		bsscfg->flags &= ~WLC_BSSCFG_HW_BCN;

#if defined(MBSS) || defined(WLP2P)
	bsscfg->maxassoc = wlc->pub->tunables->maxscb;
#endif /* MBSS || WLP2P */
#if defined(MBSS)
	BCMC_FID_INIT(bsscfg);
	bsscfg->prb_ttl_us = WLC_PRB_TTL_us;
#endif // endif

	bsscfg->_ap = TRUE;
	bsscfg->BSS = TRUE;	/* set the mode to INFRA */

#if defined(BCMAUTH_PSK)
	/* XXX: This should move to a spot where it is dynamically attached
	 * and detached based on the security settings... ToDo
	 */
	ASSERT(bsscfg->authenticator == NULL);
	if (BCMAUTH_PSK_ENAB(wlc->pub) && !(bsscfg->flags & WLC_BSSCFG_NO_AUTHENTICATOR)) {
		if ((bsscfg->authenticator = wlc_authenticator_attach(wlc, bsscfg)) == NULL) {
			WL_ERROR(("wl%d: %s: wlc_authenticator_attach failed\n",
				wlc->pub->unit, __FUNCTION__));
			return BCME_ERROR;

		}
	}
#endif	/* BCMAUTH_PSK */

#ifdef WME
	/* WME ACP advertised in bcn/prbrsp */
	if (wme != NULL &&
	    (wme->wme_param_ie_ad = (wme_param_ie_t *)
	     MALLOCZ(wlc->osh, sizeof(wme_param_ie_t))) == NULL)
		return BCME_NOMEM;
	wlc_bsscfg_wme_initparams(wlc, bsscfg);
#endif // endif

#ifdef WL_BSSCFG_TX_SUPR
	/* allocate suppression queue */
	if (!bsscfg->psq && (bsscfg->flags & WLC_BSSCFG_TX_SUPR_ENAB)) {
		if ((bsscfg->psq = (struct pktq *)
			MALLOCZ(wlc->osh, sizeof(struct pktq))) == NULL)
			return BCME_NOMEM;
		pktq_init(bsscfg->psq, WLC_PREC_COUNT, PKTQ_LEN_DEFAULT);
	}
#endif // endif

#if defined(WL11K_AP)
	if (!(bsscfg->pilot_timer = wl_init_timer(wlc->wl, wlc_pilot_timer, bsscfg, "pilot"))) {
		WL_ERROR(("pilot_timer init failed\n"));
		return BCME_NOMEM;
	}
#endif // endif

#ifdef BCMPCIEDEV_ENABLED
	bsscfg->ap_isolate = AP_ISOLATE_SENDUP_ALL;
#endif // endif

	/* Extended Channel Switch Support */
	wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_EXT_CHAN_SWITCHING, TRUE);
#ifdef WL_GLOBAL_RCLASS
	wlc_channel_set_cur_rclass(wlc, BCMWIFI_RCLASS_TYPE_GBL);
#endif /* WL_GLOBAL_RCLASS */
	/* invoke bsscfg cubby init function */
	return wlc_bsscfg_cubby_init(wlc, bsscfg);
}

static void
wlc_bsscfg_ap_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
#ifdef WME
	wlc_wme_t *wme = bsscfg->wme;
#endif // endif

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_ap_deinit: bsscfg %p\n", wlc->pub->unit, bsscfg));

	/* invoke bsscfg cubby deinit function */
	wlc_bsscfg_cubby_deinit(wlc, bsscfg);

#if defined(WL11K_AP)
	if (bsscfg->pilot_timer) {
		wl_free_timer(wlc->wl, bsscfg->pilot_timer);
	}
#endif // endif
#ifdef WME
	/* WME AC parms */
	if (wme != NULL &&
	    wme->wme_param_ie_ad != NULL) {
		MFREE(wlc->osh, wme->wme_param_ie_ad, sizeof(wme_param_ie_t));
		wme->wme_param_ie_ad = NULL;
	}
#endif // endif

#if defined(BCMAUTH_PSK)
	/* free the authenticator */
	if (BCMAUTH_PSK_ENAB(wlc->pub) && bsscfg->authenticator) {
		wlc_authenticator_detach(bsscfg->authenticator);
		bsscfg->authenticator = NULL;
	}
#endif /* BCMAUTH_PSK */

	_wlc_bsscfg_deinit(wlc, bsscfg);

	bsscfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
}
#endif /* AP */

#ifdef STA
static int
wlc_bsscfg_sta_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_roam_t *roam = bsscfg->roam;
	wlc_assoc_t *as = bsscfg->assoc;
	wlc_pm_st_t *pm = bsscfg->pm;
#ifdef WLDPT
	wlc_dpt_t *dc;
#endif // endif
#ifdef WLTDLS
	wlc_tdls_t *tc;
#endif // endif

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_sta_init: bsscfg %p\n", wlc->pub->unit, bsscfg));

	bsscfg->_ap = FALSE;
#ifdef DWDS
	bsscfg->dwds_loopback_filter = FALSE;
#endif /* DWDS */

	bzero(roam, sizeof(*roam));
	bzero(as, sizeof(*as));
	bzero(pm, sizeof(*pm));

	/* init beacon timeouts */
	roam->bcn_timeout = WLC_BCN_TIMEOUT;

	/* roam scan inits */
	roam->scan_block = 0;
	roam->partialscan_period = WLC_ROAM_SCAN_PERIOD;
	roam->fullscan_period = WLC_FULLROAM_PERIOD;
	roam->ap_environment = AP_ENV_DETECT_NOT_USED;
	roam->motion_timeout = ROAM_MOTION_TIMEOUT;
	roam->nfullscans = ROAM_FULLSCAN_NTIMES;
	roam->ci_delta = ROAM_CACHEINVALIDATE_DELTA;
	roam->roam_chn_cache_limit = WLC_SRS_DEF_ROAM_CHAN_LIMIT;
	roam->max_roam_time_thresh = (uint16) wlc->pub->tunables->maxroamthresh;
	roam->roam_rssi_boost_thresh = WLC_JOIN_PREF_RSSI_BOOST_MIN;
	if (WLRCC_ENAB(wlc->pub)) {
		if ((bsscfg->roam->rcc_channels = (chanspec_t *)
			MALLOCZ(wlc->osh, sizeof(chanspec_t) * MAX_ROAM_CHANNEL)) == NULL) {
			return BCME_NOMEM;
		}
	}

	/* create association timer */
	if ((as->timer =
	     wl_init_timer(wlc->wl, wlc_assoc_timeout, bsscfg, "assoc")) == NULL) {
		WL_ERROR(("wl%d: wl_init_timer for bsscfg %d assoc_timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		return BCME_NORESOURCE;
	}

	as->retry_max = WLC_ASSOC_RETRY_MAX;
	as->recreate_bi_timeout = WLC_ASSOC_RECREATE_BI_TIMEOUT;
	as->listen = WLC_ADVERTISED_LISTEN;

	/* default AP disassoc/deauth timeout */
	as->verify_timeout = WLC_ASSOC_VERIFY_TIMEOUT;

	/* join preference */
	if ((bsscfg->join_pref = (wlc_join_pref_t *)
	     MALLOCZ(wlc->osh, sizeof(wlc_join_pref_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	/* init join pref */
	bsscfg->join_pref->band = WLC_BAND_AUTO;

	/* create apsd trigger timer */
	if ((pm->apsd_trigger_timer =
	     wl_init_timer(wlc->wl, wlc_apsd_trigger_timeout, bsscfg, "apsd_trigger")) == NULL) {
		WL_ERROR(("wl%d: bsscfg %d wlc_apsd_trigger_timeout failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		return BCME_NORESOURCE;
	}

	/* create pspoll timer */
	if ((pm->pspoll_timer =
	     wl_init_timer(wlc->wl, wlc_pspoll_timer, bsscfg, "pspoll")) == NULL) {
		WL_ERROR(("wl%d: bsscfg %d pspoll_timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		return BCME_NORESOURCE;
	}

	/* create roam timer */
	if ((roam->timer =
	     wl_init_timer(wlc->wl, wlc_roam_timer_expiry, bsscfg, "roam")) == NULL) {
		WL_ERROR(("wl%d: wl_init_timer for bsscfg %d roam_timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		return BCME_NORESOURCE;
	}

	/* allocate pm2_ret_timer object */
	if ((pm->pm2_ret_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
		WL_ERROR(("wl%d.%d: %s: failed to alloc PM2 timeout\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
		return BCME_NORESOURCE;
	}

	/* allocate pm2_rcv_timer object */
	if ((pm->pm2_rcv_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
		WL_ERROR(("wl%d.%d: %s: failed to alloc PM2 timeout\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));
		return BCME_NORESOURCE;
	}

	/* Set the default PM2 return to sleep time */
	pm->pm2_sleep_ret_time = PM2_SLEEP_RET_MS_DEFAULT;
	pm->pm2_sleep_ret_time_left = pm->pm2_sleep_ret_time;
	pm->pm2_bcn_sleep_ret_time = 0;
	pm->pm2_md_sleep_ext = PM2_MD_SLEEP_EXT_DISABLED;
	pm->pspoll_md_counter = 0;
	pm->pspoll_md_cnt = 1;

#ifdef WLDPT
	if ((dc = MALLOCZ(wlc->osh, sizeof(wlc_dpt_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	bsscfg->dpt = dc;

	/* default friendly name is MAC addr */
	bcm_ether_ntoa(&bsscfg->cur_etheraddr, (char *)dc->fname);
	dc->fnlen = (uint8)strlen((char *)dc->fname);
	/* default security setting */
	dc->wsec = AES_ENABLED;
	dc->WPA_auth = BRCM_AUTH_DPT;
#endif // endif

#ifdef WLTDLS
	if (TDLS_SUPPORT(wlc->pub)) {
		if ((tc = MALLOCZ(wlc->osh, sizeof(wlc_tdls_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		bsscfg->tdls = tc;

		/* default security setting */
		tc->wsec = AES_ENABLED;
		tc->WPA_auth = WPA2_AUTH_TPK;
		tc->ps_allowed = TRUE;
		tc->ps_pending = FALSE;
	}
#endif /* WLTDLS */

#ifdef WME
	wlc_bsscfg_wme_initparams(wlc, bsscfg);
	bsscfg->wme->apsd_trigger_ac = AC_BITMAP_ALL;
#endif // endif
	/* Extended Channel Switch Support */
	wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_EXT_CHAN_SWITCHING, FALSE);
	/* invoke bsscfg cubby init function */
	/* return success only if cubbies also all succeed */
	return wlc_bsscfg_cubby_init(wlc, bsscfg);
}

static void
wlc_bsscfg_sta_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_sta_deinit: bsscfg %p\n", wlc->pub->unit, bsscfg));

#ifdef DWDS
	bsscfg->dwds_loopback_filter = FALSE;
#endif /* DWDS */

	/* invoke bsscfg cubby deinit function */
	wlc_bsscfg_cubby_deinit(wlc, bsscfg);

	/* free the association timer */
	if (bsscfg->assoc != NULL) {
		wlc_assoc_t *as = bsscfg->assoc;

		if (as->timer != NULL) {
			wl_free_timer(wlc->wl, as->timer);
			as->timer = NULL;
		}
		/* Need to free the allocated memory here because if we were to init
		 * this bsscfg as a STA again, bsscfg->assoc will get zeroed out and
		 * the allocated memory elements would be lost and never freed.
		 */
		if (as->ie != NULL) {
			MFREE(wlc->osh, as->ie, as->ie_len);
			as->ie = NULL;
		}
		if (as->req != NULL) {
			MFREE(wlc->osh, as->req, as->req_len);
			as->req = NULL;
		}
		if (as->resp != NULL) {
			MFREE(wlc->osh, as->resp, as->resp_len);
			as->resp = NULL;
		}
	}

	if (bsscfg->join_pref != NULL) {
		MFREE(wlc->osh, bsscfg->join_pref, sizeof(wlc_join_pref_t));
		bsscfg->join_pref = NULL;
	}

	if (bsscfg->pm != NULL) {
		wlc_pm_st_t *pm = bsscfg->pm;

		if (pm->apsd_trigger_timer) {
			wl_free_timer(wlc->wl, pm->apsd_trigger_timer);
			pm->apsd_trigger_timer = NULL;
		}

		if (pm->pspoll_timer) {
			wl_free_timer(wlc->wl, pm->pspoll_timer);
			pm->pspoll_timer = NULL;
		}

		/* free the pm2_rcv_timer object */
		if (pm->pm2_rcv_timer != NULL) {
			wlc_hrt_free_timeout(pm->pm2_rcv_timer);
			pm->pm2_rcv_timer = NULL;
		}

		/* free the pm2_rcv_timeout object */
		if (pm->pm2_ret_timer != NULL) {
#ifdef WL_PWRSTATS
			if (PWRSTATS_ENAB(wlc->pub)) {
				wlc_pwrstats_frts_end(wlc->pwrstats);
			}
#endif /* WL_PWRSTATS */

			wlc_hrt_del_timeout(pm->pm2_ret_timer);
			wlc_hrt_free_timeout(pm->pm2_ret_timer);
			pm->pm2_ret_timer = NULL;
		}
	}

	if (bsscfg->roam != NULL) {
		wlc_roam_t *roam = bsscfg->roam;

		if (roam->timer != NULL) {
			wl_free_timer(wlc->wl, roam->timer);
			roam->timer = NULL;
		}
		if (WLRCC_ENAB(wlc->pub) && (bsscfg->roam->rcc_channels != NULL)) {
			MFREE(wlc->osh, bsscfg->roam->rcc_channels,
				(sizeof(chanspec_t) * MAX_ROAM_CHANNEL));
		}
	}

#ifdef WLDPT
	if (bsscfg->dpt != NULL) {
		MFREE(wlc->osh, bsscfg->dpt, sizeof(wlc_dpt_t));
		bsscfg->dpt = NULL;
	}
#endif // endif

#ifdef WLTDLS
	if (bsscfg->tdls != NULL) {
		MFREE(wlc->osh, bsscfg->tdls, sizeof(wlc_tdls_t));
		bsscfg->tdls = NULL;
	}
#endif // endif

	_wlc_bsscfg_deinit(wlc, bsscfg);

	wlc_bsscfg_scan_params_reset(wlc, bsscfg);
	wlc_bsscfg_assoc_params_reset(wlc, bsscfg);
}
#endif /* STA */

#ifdef WLBTAMP
int
wlc_bsscfg_bta_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_bta_init: bsscfg %p\n",
	          wlc->pub->unit, bsscfg));

	/* N.B.: QoS settings configured implicitly */
	bsscfg->flags |= WLC_BSSCFG_BTA;

	return wlc_bsscfg_init(wlc, bsscfg);
}
#endif /* WLBTAMP */

static int
wlc_bsscfg_bss_rsinit(wlc_info_t *wlc, wlc_bss_info_t *bi, uint8 rates, uint8 bw, uint8 mcsallow)
{
	wlc_rateset_t *src = &wlc->band->hw_rateset;
	wlc_rateset_t *dst = &bi->rateset;

	wlc_rateset_filter(src, dst, FALSE, rates, RATE_MASK_FULL, mcsallow);
	if (dst->count == 0)
		return BCME_NORESOURCE;
#ifdef WL11N
	wlc_rateset_bw_mcs_filter(dst, bw);
#endif // endif
	wlc_rate_lookup_init(wlc, dst);

	return BCME_OK;
}

int
wlc_bsscfg_rateset_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 rates, uint8 bw, uint8 mcsallow)
{
	int err;

	if ((err = wlc_bsscfg_bss_rsinit(wlc, cfg->target_bss, rates, bw, mcsallow)) != BCME_OK)
		return err;
	if ((err = wlc_bsscfg_bss_rsinit(wlc, cfg->current_bss, rates, bw, mcsallow)) != BCME_OK)
		return err;

	return err;
}
#ifdef WLAWDL
int wlc_bsscfg_awdl_rate_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 bw, uint8 minrate)
{
	int rc;
	rc = wlc_bsscfg_rateset_init(wlc, bsscfg, WLC_RATES_OFDM, bw, TRUE);
	if ((rc == BCME_OK) && minrate)
		wlc_ratesel_filter_minrateset(&bsscfg->current_bss->rateset,
			&bsscfg->current_bss->rateset, (WLC_40_MHZ == bw),
			minrate);
	return rc;
}
#endif /* WLAWDL */

#ifdef WLDPT
void
wlc_bsscfg_dpt_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_bss_info_t *current_bss = bsscfg->current_bss;
	bool cck_only;

	bsscfg->_ap = FALSE;

	bsscfg->BSS = 0;

	/* init chanspec and rateset */
	current_bss->chanspec = wlc->home_chanspec;
	if (BAND_2G(wlc->band->bandtype) && wlc->band->gmode == GMODE_LEGACY_B)
		cck_only = TRUE;
	else
		cck_only = FALSE;
	wlc_bsscfg_rateset_init(wlc, bsscfg,
	                        cck_only ? WLC_RATES_CCK : WLC_RATES_CCK_OFDM,
	                        WL_BW_CAP_40MHZ(wlc->band->bw_cap) ?
	                        CHSPEC_WLC_BW(wlc->home_chanspec) : 0,
	                        cck_only ? 0 : wlc_get_mcsallow(wlc, NULL));

	bsscfg->flags |= WLC_BSSCFG_DPT;

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wlc->pub))
		bsscfg->wlcif->if_flags |= WLC_IF_PKT_80211;
#endif // endif

#ifdef WME
	wlc_bsscfg_wme_initparams(wlc, bsscfg);
#endif // endif

	bsscfg->up = TRUE;
}
#endif /* WLDPT */

#ifdef WLTDLS
int
wlc_bsscfg_tdls_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool initiator)
{
	int ret;

	bsscfg->_ap = FALSE;

	bsscfg->BSS = 0;

	if ((ret = wlc_bsscfg_init(wlc, bsscfg)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: cannot init bsscfg\n", wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	bsscfg->flags |= WLC_BSSCFG_TDLS;
	bsscfg->tdls->initiator = initiator;

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wlc->pub))
		bsscfg->wlcif->if_flags |= WLC_IF_PKT_80211;
#endif // endif

	bsscfg->pm->PM = 0;

	bsscfg->up = TRUE;
exit:
	return ret;
}
#endif /* WLTDLS */

int
wlc_bsscfg_vif_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int ret = 0;

	cfg->wlcif->if_flags |= WLC_IF_VIRTUAL;

	ret = wlc_bsscfg_init(wlc, cfg);
	if (ret)
		goto exit;

exit:
	return ret;

}

int
wlc_bsscfg_set_infra_mode(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool infra)
{
	bool old_BSS;
	ASSERT(cfg != NULL);

	old_BSS = cfg->BSS;
	cfg->BSS = infra ? TRUE : FALSE;

	if (cfg->BSS != old_BSS) {
		/* infra changed, send up if event */
		wlc_if_event(wlc, WLC_E_IF_CHANGE, cfg->wlcif);
	}

	/* AP has these flags set in wlc_bsscfg_ap_init() */
	if (BSSCFG_STA(cfg)) {
		/* IBSS deploys PSM bcn/prbrsp */
#ifdef WLAWDL
		if (!cfg->BSS && !BSSCFG_AWDL(wlc, cfg) &&
#else
		if (!cfg->BSS &&
#endif /* WLAWDL */
		    !(cfg->flags & (WLC_BSSCFG_SW_BCN | WLC_BSSCFG_SW_PRB)))
			cfg->flags |= (WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
		/* reset in case of a role change between Infra STA and IBSS STA */
		else
			cfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
	}

	return BCME_OK;
}

int
wlc_bsscfg_get_free_idx(wlc_info_t *wlc)
{
	int idx;

	for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
		if (wlc->bsscfg[idx] == NULL)
			return idx;
	}

	return BCME_ERROR;
}

void
wlc_bsscfg_ID_assign(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	bsscfg->ID = wlc->next_bsscfg_ID;
	wlc->next_bsscfg_ID ++;
}

/**
 * After all the modules indicated how much cubby space they need in the bsscfg, the actual
 * wlc_bsscfg_t can be allocated. This happens one time fairly late within the attach phase, but
 * also when e.g. communication with a new remote party is started.
 */
static wlc_bsscfg_t *
wlc_bsscfg_malloc(wlc_info_t *wlc)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	osl_t *osh;
	wlc_bsscfg_t *cfg;

	osh = wlc->osh;

	if ((cfg = (wlc_bsscfg_t *)MALLOCZ(osh, bcmh->totsize)) == NULL)
		goto fail;

	/* XXX optimization opportunities:
	 * some of these subsidaries are STA specific therefore allocate them only for
	 * STA bsscfg...
	 */

#ifdef MBSS
	if ((cfg->bcn_template = (wlc_spt_t *)
	     MALLOCZ(osh, sizeof(wlc_spt_t))) == NULL)
		goto fail;
#endif /* MBSS */

#ifdef WLC_HIGH
	if ((cfg->multicast = (struct ether_addr *)
	     MALLOCZ(osh, (sizeof(struct ether_addr)*MAXMULTILIST))) == NULL) {
		goto fail;
	}
#endif /* WLC_HIGH */

	if ((cfg->assoc = (wlc_assoc_t *)
	     MALLOCZ(osh, sizeof(wlc_assoc_t))) == NULL)
		goto fail;
	if ((cfg->roam = (wlc_roam_t *)
	     MALLOCZ(osh, sizeof(wlc_roam_t))) == NULL)
		goto fail;
	if ((cfg->cxn = (wlc_cxn_t *)
	     MALLOCZ(osh, sizeof(wlc_cxn_t))) == NULL)
		goto fail;
	if ((cfg->link = (wlc_link_qual_t *)
	     MALLOCZ(osh, sizeof(wlc_link_qual_t))) == NULL)
		goto fail;
#ifdef WLC_HIGH
	if ((cfg->link->rssi_pkt_window = (int *)
	     MALLOCZ(osh, sizeof(int) * MA_WINDOW_SZ)) == NULL)
		goto fail;
	cfg->link->rssi_pkt_win_sz = MA_WINDOW_SZ;
	cfg->link->snr_pkt_win_sz = MA_WINDOW_SZ;
#endif // endif
	if ((cfg->link->rssi_event = (wl_rssi_event_t *)
	     MALLOCZ(osh, sizeof(wl_rssi_event_t))) == NULL)
		goto fail;
	if ((cfg->current_bss = (wlc_bss_info_t *)
	     MALLOCZ(osh, sizeof(wlc_bss_info_t))) == NULL)
		goto fail;
	if ((cfg->target_bss = (wlc_bss_info_t *)
	     MALLOCZ(osh, sizeof(wlc_bss_info_t))) == NULL)
		goto fail;
#if defined(MBSS) && defined(WLCNT)
	if ((cfg->cnt = (wlc_mbss_cnt_t *)
	     MALLOCZ(osh, sizeof(wlc_mbss_cnt_t))) == NULL)
		goto fail;
#endif // endif

	if ((cfg->pm = (wlc_pm_st_t *)
	     MALLOCZ(osh, sizeof(wlc_pm_st_t))) == NULL)
		goto fail;

	/* EDCF/APSD/WME */
	if ((cfg->wme = (wlc_wme_t *)
	     MALLOCZ(osh, sizeof(wlc_wme_t))) == NULL)
		goto fail;

	/* BRCM IE */
	if ((cfg->brcm_ie = (uint8 *)
	     MALLOCZ(osh, WLC_MAX_BRCM_ELT)) == NULL)
		goto fail;

	/* obss */
	if ((cfg->obss = (wlc_obss_info_t *)
	     MALLOCZ(osh, sizeof(wlc_obss_info_t))) == NULL)
		goto fail;

#ifdef	MULTIAP
	/* Multi-Ap IE */
	if ((cfg->multiap_ie = (uint8 *)
		MALLOCZ(osh, MAP_IE_MAX_LEN)) == NULL) {
		goto fail;
	}
#endif	/* MULTIAP */

#ifdef TRAFFIC_MGMT
	/*
	 * Allocate traffic management resources for this new bsscfg.
	 */
	if (TRAFFIC_MGMT_ENAB(wlc->pub))
	    wlc_trf_mgmt_bsscfg_allocate(wlc->trf_mgmt_ctxt, cfg);
#endif // endif

	/* roam scan params */
	if ((cfg->roam_scan_params = (wl_join_scan_params_t *)
	     MALLOCZ(osh, sizeof(wl_join_scan_params_t))) == NULL)
		goto fail;

	/* bss load reporting */
	if (BSSLOAD_REPORT_ENAB(wlc->pub) && !BSSCFG_AWDL(wlc, cfg) && !BSSCFG_AP(cfg)) {
		if ((cfg->bssload = (wlc_bssload_t *)
		     MALLOCZ(osh, sizeof(wlc_bssload_t))) == NULL)
			goto fail;
	}

	return cfg;

fail:
	wlc_bsscfg_mfree(wlc, cfg);
	return NULL;
}

static void
wlc_bsscfg_mfree(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bsscfg_module_t *bcmh = wlc->bcmh;
	osl_t *osh;

	(void)bcmh;

	if (cfg == NULL)
		return;

	osh = wlc->osh;

#ifdef WLC_HIGH
	if (cfg->multicast) {
		MFREE(osh, cfg->multicast, (sizeof(struct ether_addr) * MAXMULTILIST));
		cfg->multicast = NULL;
	}
#endif // endif

#ifdef MBSS
	if (cfg->bcn_template) {
		MFREE(osh, cfg->bcn_template, sizeof(wlc_spt_t));
		cfg->bcn_template = NULL;
	}
#endif /* MBSS */

	if (cfg->assoc != NULL) {
		wlc_assoc_t *as = cfg->assoc;
		if (as->ie != NULL)
			MFREE(osh, as->ie, as->ie_len);
		if (as->req != NULL)
			MFREE(osh, as->req, as->req_len);
		if (as->resp != NULL)
			MFREE(osh, as->resp, as->resp_len);
		MFREE(osh, as, sizeof(wlc_assoc_t));
		cfg->assoc = NULL;
	}
	if (cfg->roam != NULL) {
		MFREE(osh, cfg->roam, sizeof(wlc_roam_t));
		cfg->roam = NULL;
	}
	if (cfg->cxn != NULL) {
		MFREE(osh, cfg->cxn, sizeof(wlc_cxn_t));
		cfg->cxn = NULL;
	}
	if (cfg->link != NULL) {
		wlc_link_qual_t *link = cfg->link;
#ifdef WLC_HIGH
		if (link->rssi_pkt_window != NULL)
			MFREE(osh, link->rssi_pkt_window, sizeof(int) * MA_WINDOW_SZ);
#endif // endif
		if (link->rssi_event_timer) {
			wl_del_timer(wlc->wl, link->rssi_event_timer);
			wl_free_timer(wlc->wl, link->rssi_event_timer);
			link->rssi_event_timer = NULL;
			link->is_rssi_event_timer_active = FALSE;
		}
		if (link->rssi_event != NULL)
			MFREE(osh, link->rssi_event, sizeof(wl_rssi_event_t));
		MFREE(osh, link, sizeof(wlc_link_qual_t));
		cfg->link = NULL;
	}
	if (cfg->current_bss != NULL) {
		wlc_bss_info_t *current_bss = cfg->current_bss;
		/* WES FIXME: move this to detach so that assoc_recreate will have a bcn_prb
		 * over a down/up transition.
		 */
		if (current_bss->bcn_prb != NULL) {
			MFREE(osh, current_bss->bcn_prb, current_bss->bcn_prb_len);
			current_bss->bcn_prb = NULL;
			current_bss->bcn_prb_len = 0;
		}
		MFREE(osh, current_bss, sizeof(wlc_bss_info_t));
		cfg->current_bss = NULL;
	}
	if (cfg->target_bss != NULL) {
		MFREE(osh, cfg->target_bss, sizeof(wlc_bss_info_t));
		cfg->target_bss = NULL;
	}

#if defined(MBSS) && defined(WLCNT)
	if (cfg->cnt) {
		MFREE(osh, cfg->cnt, sizeof(wlc_mbss_cnt_t));
		cfg->cnt = NULL;
	}
#endif // endif

	if (cfg->pm != NULL) {
		MFREE(osh, cfg->pm, sizeof(wlc_pm_st_t));
		cfg->pm = NULL;
	}

	/* EDCF/APSD/WME */
	if (cfg->wme != NULL) {
		MFREE(osh, cfg->wme, sizeof(wlc_wme_t));
		cfg->wme = NULL;
	}

	/* BRCM IE */
	if (cfg->brcm_ie != NULL) {
		MFREE(osh, cfg->brcm_ie, WLC_MAX_BRCM_ELT);
		cfg->brcm_ie = NULL;
	}

	/* obss */
	if (cfg->obss != NULL) {
		MFREE(osh, cfg->obss, sizeof(wlc_obss_info_t));
		cfg->obss = NULL;
	}

#ifdef	MULTIAP
	/* Multi-AP IE */
	if (cfg->multiap_ie != NULL) {
		MFREE(osh, cfg->multiap_ie, MAP_IE_MAX_LEN);
		cfg->multiap_ie = NULL;
	}
#endif	/* MULTIAP */

#ifdef TRAFFIC_MGMT
	/*
	 * Free the traffic management resources for this bsscfg.
	 */
	if (TRAFFIC_MGMT_ENAB(wlc->pub))
	    wlc_trf_mgmt_bsscfg_free(wlc->trf_mgmt_ctxt, cfg);
#endif // endif

	/* XXX:
	 * It's not difficult to imagine a better scheme for memory alloc/free
	 * of all these little pieces.
	 * FIXME
	 */
	/* roam scan params */
	if (cfg->roam_scan_params != NULL) {
		MFREE(osh, cfg->roam_scan_params, sizeof(wl_join_scan_params_t));
		cfg->roam_scan_params = NULL;
	}

	if (BSSLOAD_REPORT_ENAB(wlc->pub)) {
		if (cfg->bssload != NULL) {
			MFREE(osh, cfg->bssload, sizeof(wlc_bssload_t));
			cfg->bssload = NULL;
		}
	}
	MFREE(osh, cfg, bcmh->totsize);
}

/**
 * Called when e.g. a wireless interface is added, during the ATTACH phase, or as a result of an
 * IOVAR.
 */
wlc_bsscfg_t *
wlc_bsscfg_alloc(wlc_info_t *wlc, int idx, uint flags, struct ether_addr *ea, bool ap)
{
	wlc_bsscfg_t *bsscfg;
	int err;

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_alloc: index %d flags 0x%08x ap %d\n",
	               wlc->pub->unit, idx, flags, ap));

	if (idx < 0 || idx >= WLC_MAXBSSCFG) {
		return NULL;
	}

	if ((bsscfg = wlc_bsscfg_malloc(wlc)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	if ((err = wlc_bsscfg_alloc_ext(wlc, bsscfg, idx,
	                        ea != NULL ? ea : &wlc->pub->cur_etheraddr,
	                        flags, ap)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_alloc_ext failed with %d\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	return bsscfg;

fail:
	if (bsscfg != NULL)
		wlc_bsscfg_free(wlc, bsscfg);
	return NULL;
}

int
wlc_bsscfg_vif_reset(wlc_info_t *wlc, int idx, uint flags, struct ether_addr *ea, bool ap)
{
	wlc_bsscfg_t *bsscfg;
	int err;

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_reset: index %d flags 0x%08x ap %d\n",
	               wlc->pub->unit, idx, flags, ap));

	ASSERT((idx > 0 && idx < WLC_MAXBSSCFG));
	if ((idx < 0) || (idx >= WLC_MAXBSSCFG)) {
		return BCME_RANGE;
	}

	if ((idx < 0) || (idx >= WLC_MAXBSSCFG)) {
		return BCME_RANGE;
	}

	if (wlc->bsscfg[idx] == NULL)
		return BCME_ERROR;

	bsscfg = wlc->bsscfg[idx];

	/* clear SSID */
	memset(bsscfg->SSID, 0, DOT11_MAX_SSID_LEN);
	bsscfg->SSID_len = 0;

	err = _wlc_bsscfg_init(wlc, bsscfg, ea != NULL ? ea : &wlc->pub->cur_etheraddr, flags, ap);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: wlc_bsscfg_vif_reset: _wlc_bsscfg_init() failed\n",
			wlc->pub->unit));
		return err;
	}

	err = wlc_bsscfg_vif_init(wlc, bsscfg);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: wlc_bsscfg_vif_reset: Cannot init bsscfg, err = %d\n",
			wlc->pub->unit, err));
	}

	bsscfg->up = FALSE;

	return err;

}

static void
wlc_bsscfg_bcmcscbdeinit(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	int ii;

	if (wlc->scbstate == NULL)
		return;

	for (ii = 0; ii < MAXBANDS; ii++) {
		if (bsscfg->bcmc_scb[ii]) {
			WL_INFORM(("bcmc_scb: band %d: free internal scb for 0x%p\n",
			           ii, bsscfg->bcmc_scb[ii]));
			wlc_internalscb_free(wlc, bsscfg->bcmc_scb[ii]);
			bsscfg->bcmc_scb[ii] = NULL;
			break;
		}
	}

	for (ii++; ii < MAXBANDS; ii++) {
		bsscfg->bcmc_scb[ii] = NULL;
	}
}

#if defined(AP) || defined(STA)
static void
_wlc_bsscfg_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	uint ii;

	WL_APSTA_UPDN(("wl%d: _wlc_bsscfg_deinit: bsscfg %p\n", wlc->pub->unit, bsscfg));

	/* process event queue */
	wlc_eventq_flush(wlc->eventq);

#if defined(AP) || defined(WLDPT)
	wlc_vndr_ie_list_free(wlc->osh, &bsscfg->vndr_ie_listp);
#endif // endif

#ifdef EXT_STA
	/* free exempt list */
	if (WLEXTSTA_ENAB(wlc->pub)) {
		if (bsscfg->exempt_list) {
			MFREE(wlc->osh, bsscfg->exempt_list,
				WLC_EXEMPT_LIST_LEN(bsscfg->exempt_list->entries));
			bsscfg->exempt_list = NULL;
		}
	}
#endif /* EXT_STA */

	/*
	 * If the index into the wsec_keys table is less than WSEC_MAX_DEFAULT_KEYS,
	 * the keys were allocated statically, and should not be deleted or removed.
	 */
	if (bsscfg != wlc_bsscfg_primary(wlc)) {
		for (ii = 0; ii < ARRAYSIZE(bsscfg->bss_def_keys); ii ++) {
			if (bsscfg->bss_def_keys[ii] == NULL)
				continue;
			wlc_key_delete(wlc, bsscfg, bsscfg->bss_def_keys[ii]);
		}
	}

	/* free RCMTA keys if keys allocated */
	if (bsscfg->rcmta != NULL)
		wlc_key_del_bssid(wlc, bsscfg);

#ifdef WL_BSSCFG_TX_SUPR
	if (bsscfg->psq != NULL) {
#ifdef PKTQ_LOG
		wlc_pktq_stats_free(wlc, bsscfg->psq);
#endif /* PKTQ_LOG */
		MFREE(wlc->osh, bsscfg->psq, sizeof(struct pktq));
		bsscfg->psq = NULL;
	}
#endif /* WL_BSSCFG_TX_SUPR */
}
#endif /* AP || STA */

void
wlc_bsscfg_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	if (bsscfg->_ap) {
#ifdef AP
		wlc_bsscfg_ap_deinit(wlc, bsscfg);
#endif // endif
	}
	else {
#ifdef STA
		wlc_bsscfg_sta_deinit(wlc, bsscfg);
#endif // endif
	}
}

static void
wlc_bsscfg_free_ext(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	/* free all scbs */
	wlc_bsscfg_bcmcscbdeinit(wlc, bsscfg);
}

void
wlc_bsscfg_free(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int idx;

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_free: bsscfg %p, flags = 0x%x\n",
		wlc->pub->unit, bsscfg, bsscfg->flags));

	/* XXX [NDIS restruct] Consider merging here from Falcon p2p code
	 * (under WLP2P || WLDSTA)
	 */

	/* Make sure that any active scan is not associated to this cfg */
	if (SCAN_IN_PROGRESS(wlc->scan) && (bsscfg == wlc_scan_bsscfg(wlc->scan))) {
		/* ASSERT(bsscfg != wlc_scan_bsscfg(wlc->scan)); */
		WL_ERROR(("wl%d.%d: %s: scan still active using cfg %p\n", WLCWLUNIT(wlc),
		          WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, bsscfg));
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
	}

#ifdef WLMCHAN
	/* if context still exists here, delete it */
	if (MCHAN_ENAB(wlc->pub) && bsscfg->chan_context) {
		WL_MCHAN(("%s: context still exist, delete\n", __FUNCTION__));
		wlc_mchan_delete_bss_chan_context(wlc, bsscfg);
	}
#endif // endif

	wlc_bsscfg_deinit(wlc, bsscfg);

#ifdef WMF
	/* Delete WMF instance if it created for this bsscfg */
	if (WMF_ENAB(bsscfg)) {
		wlc_wmf_instance_del(bsscfg);
	}
#endif // endif

#ifdef SMF_STATS
	wlc_bsscfg_smfsfree(wlc, bsscfg);
#endif // endif

#ifdef STA
	wlc_bsscfg_wsec_key_buf_free(wlc, bsscfg);
#endif // endif

	/* process event queue */
	wlc_eventq_flush(wlc->eventq);

	wlc_bsscfg_free_ext(wlc, bsscfg);

	if (!(bsscfg->flags & WLC_BSSCFG_P2P_RECREATE_BSSIDX)) {
		/* delete the upper-edge driver interface */
		if (bsscfg != wlc_bsscfg_primary(wlc) && bsscfg->wlcif != NULL) {
			wlc_if_event(wlc, WLC_E_IF_DEL, bsscfg->wlcif);
			if (bsscfg->wlcif->wlif != NULL) {
				wl_del_if(wlc->wl, bsscfg->wlcif->wlif);
				bsscfg->wlcif->wlif = NULL;
			}
		}
		wlc_wlcif_free(wlc, wlc->osh, bsscfg->wlcif);
		bsscfg->wlcif = NULL;
	}

	/* free the wlc_bsscfg struct if it was an allocated one */
	idx = bsscfg->_idx;
	wlc_bsscfg_mfree(wlc, bsscfg);
	wlc->bsscfg[idx] = NULL;
}

int
wlc_bsscfg_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int ret = BCME_OK;
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_init: bsscfg %p\n", wlc->pub->unit, bsscfg));

#if defined(AP) && defined(STA)
	if (bsscfg->_ap)
		ret = wlc_bsscfg_ap_init(wlc, bsscfg);
	else
		ret = wlc_bsscfg_sta_init(wlc, bsscfg);
#elif defined(AP)
	ret = wlc_bsscfg_ap_init(wlc, bsscfg);
#elif defined(STA)
	ret = wlc_bsscfg_sta_init(wlc, bsscfg);
#endif // endif

#ifdef WL11AC
	/* VHT STA shall always set the oper mode notification in extended caps.
	 * For simplicity we do this for HT and VHT
	 * Initial value for Operating mode is set to NONE
	 */
	if (N_ENAB(wlc->pub) || VHT_ENAB(wlc->pub)) {
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_OPER_MODE_NOTIF, TRUE);

		/* Indicate no operation mode insertion
		* to beacon, probresp or assocreq by default
		*/
		bsscfg->oper_mode = 0;
		bsscfg->oper_mode_enabled = FALSE;
	}
#endif /* WL11AC */

	return ret;
}

int
wlc_bsscfg_reinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool ap)
{
#if defined(AP) && defined(STA)
	int ret;
#endif // endif

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_reinit: bsscfg %p ap %d\n", wlc->pub->unit, bsscfg, ap));

	if (bsscfg->_ap == ap)
		return BCME_OK;

#if defined(AP) && defined(STA)
	if (ap) {
		wlc_bsscfg_sta_deinit(wlc, bsscfg);
		ret = wlc_bsscfg_ap_init(wlc, bsscfg);
		if (ret != BCME_OK)
			return ret;
		if (bsscfg != wlc_bsscfg_primary(wlc))
			wlc_if_event(wlc, WLC_E_IF_CHANGE, bsscfg->wlcif);
		return ret;
	}
	wlc_bsscfg_ap_deinit(wlc, bsscfg);
	ret = wlc_bsscfg_sta_init(wlc, bsscfg);
	if (ret != BCME_OK)
		return ret;
	if (bsscfg != wlc_bsscfg_primary(wlc))
		wlc_if_event(wlc, WLC_E_IF_CHANGE, bsscfg->wlcif);
	return ret;
#else
	return BCME_OK;
#endif /* AP && STA */
}

/**
 * Get a bsscfg pointer, failing if the bsscfg does not alreay exist.
 * Sets the bsscfg pointer in any event.
 * Returns BCME_RANGE if the index is out of range or BCME_NOTFOUND
 * if the wlc->bsscfg[i] pointer is null
 */
wlc_bsscfg_t *
wlc_bsscfg_find(wlc_info_t *wlc, int idx, int *perr)
{
	wlc_bsscfg_t *bsscfg;

	if ((idx < 0) || (idx >= WLC_MAXBSSCFG)) {
		*perr = BCME_RANGE;
		return NULL;
	}

	bsscfg = wlc->bsscfg[idx];
	*perr = bsscfg ? 0 : BCME_NOTFOUND;

	return bsscfg;
}

/**
 * allocs/inits that can't go in wlc_bsscfg_malloc() (which is
 * called when wlc->wl for example may be invalid...)
 * come in here.
 */
static int
wlc_bsscfg_alloc_ext(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, int idx,
	struct ether_addr *ea, uint flags, bool ap)
{
	int err;

	wlc->bsscfg[idx] = bsscfg;
	bsscfg->_idx = (int8)idx;

	wlc_bsscfg_ID_assign(wlc, bsscfg);

	if ((err = _wlc_bsscfg_init(wlc, bsscfg, ea, flags, ap)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: _wlc_bsscfg_init() failed with %d\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	return BCME_OK;

fail:
	wlc_bsscfg_free_ext(wlc, bsscfg);
	return err;
}

wlc_bsscfg_t *
wlc_bsscfg_primary(wlc_info_t *wlc)
{
	return wlc->cfg;
}

/**
 * Called fairly late during wlc_attach(), when most modules already completed attach.
 */
int
BCMATTACHFN(wlc_bsscfg_primary_init)(wlc_info_t *wlc)
{
	wlc_bsscfg_t *bsscfg;
	int err;

	if ((bsscfg = wlc_bsscfg_malloc(wlc)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	wlc->cfg = bsscfg;

	if ((err = wlc_bsscfg_alloc_ext(wlc, bsscfg, 0,
	                               &wlc->pub->cur_etheraddr,
	                               0, wlc->pub->_ap)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_alloc_ext() failed with %d\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	if ((err = wlc_bsscfg_init(wlc, bsscfg)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_init() failed with %d\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	return BCME_OK;

fail:
	if (bsscfg != NULL)
		wlc_bsscfg_free(wlc, bsscfg);
	return err;
}

/*
 * Find a bsscfg from matching cur_etheraddr, BSSID, SSID, or something unique.
 */

/** match wlcif */
wlc_bsscfg_t *
wlc_bsscfg_find_by_wlcif(wlc_info_t *wlc, wlc_if_t *wlcif)
{
	/* wlcif being NULL implies primary interface hence primary bsscfg */
	if (wlcif == NULL)
		return wlc_bsscfg_primary(wlc);

	switch (wlcif->type) {
	case WLC_IFTYPE_BSS:
		return wlcif->u.bsscfg;
#ifdef AP
	case WLC_IFTYPE_WDS:
		return SCB_BSSCFG(wlcif->u.scb);
#endif // endif
	}

	WL_ERROR(("wl%d: Unknown wlcif %p type %d\n", wlc->pub->unit, wlcif, wlcif->type));
	return NULL;
}

/** match cur_etheraddr */
wlc_bsscfg_t * BCMFASTPATH
wlc_bsscfg_find_by_hwaddr(wlc_info_t *wlc, struct ether_addr *hwaddr)
{
	int i;
	wlc_bsscfg_t *bsscfg;

#ifdef  P2PONEINT

	wlc_bsscfg_t *found_bsscfg = NULL;

	if (ETHER_ISNULLADDR(hwaddr) || ETHER_ISMULTI(hwaddr))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (bcmp(hwaddr->octet, bsscfg->cur_etheraddr.octet, ETHER_ADDR_LEN) == 0 &&
			(bsscfg->flags & WLC_BSSCFG_EXAMPT_FLAGS) == 0) {
			found_bsscfg = bsscfg;
		}
	}

	return found_bsscfg;

#else

	if (ETHER_ISNULLADDR(hwaddr) || ETHER_ISMULTI(hwaddr))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (eacmp(hwaddr->octet, bsscfg->cur_etheraddr.octet) == 0 &&
			(bsscfg->flags & WLC_BSSCFG_EXAMPT_FLAGS) == 0) {
			return bsscfg;
		}
	}

	return NULL;
#endif  /* P2PONEINT */
}

/** match BSSID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_bssid(wlc_info_t *wlc, const struct ether_addr *bssid)
{
	int i;
	wlc_bsscfg_t *bsscfg;

#ifdef  P2PONEINT

	wlc_bsscfg_t *found_bsscfg = NULL;

	if (ETHER_ISNULLADDR(bssid) || ETHER_ISMULTI(bssid))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (bcmp(bssid->octet, bsscfg->BSSID.octet, ETHER_ADDR_LEN) == 0) {
			found_bsscfg = bsscfg;
		}
	}

	return found_bsscfg;

#else

	if (ETHER_ISNULLADDR(bssid) || ETHER_ISMULTI(bssid))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (eacmp(bssid->octet, bsscfg->BSSID.octet) == 0)
			return bsscfg;
	}

	return NULL;
#endif /* P2PONEINT */
}

/** match cur_etheraddr and BSSID */
wlc_bsscfg_t * BCMFASTPATH
wlc_bsscfg_find_by_hwaddr_bssid(wlc_info_t *wlc,
                                const struct ether_addr *hwaddr,
                                const struct ether_addr *bssid)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	if (ETHER_ISMULTI(hwaddr) || ETHER_ISMULTI(bssid))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if ((eacmp(hwaddr->octet, bsscfg->cur_etheraddr.octet) == 0) &&
		    (eacmp(bssid->octet, bsscfg->BSSID.octet) == 0))
			return bsscfg;
	}

	return NULL;
}

/** match target_BSSID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_target_bssid(wlc_info_t *wlc, const struct ether_addr *bssid)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	if (ETHER_ISNULLADDR(bssid) || ETHER_ISMULTI(bssid))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (!BSSCFG_STA(bsscfg))
			continue;
		if (eacmp(bssid->octet, bsscfg->target_bss->BSSID.octet) == 0)
			return bsscfg;
	}

	return NULL;
}

/** match SSID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_ssid(wlc_info_t *wlc, uint8 *ssid, int ssid_len)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (ssid_len > 0 &&
		    ssid_len == bsscfg->SSID_len && bcmp(ssid, bsscfg->SSID, ssid_len) == 0)
			return bsscfg;
	}

	return NULL;
}

/** match ID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_ID(wlc_info_t *wlc, uint16 id)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (bsscfg->ID == id)
			return bsscfg;
	}

	return NULL;
}

static void
wlc_bsscfg_bss_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_bss_info_t * bi = wlc->default_bss;

	bcopy((char*)bi, (char*)bsscfg->target_bss, sizeof(wlc_bss_info_t));
	bcopy((char*)bi, (char*)bsscfg->current_bss, sizeof(wlc_bss_info_t));

	if (bi->infra == 1)
		bsscfg->BSS = TRUE;	/* set the mode to INFRA */
}

static int
_wlc_bsscfg_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct ether_addr *ea, uint flags, bool ap)
{
	brcm_ie_t *brcm_ie;
#ifdef	MULTIAP
	multiap_ie_t *multiap_ie = NULL;
#endif	/* MULTIAP */

	ASSERT(bsscfg != NULL);
	ASSERT(ea != NULL);

	bsscfg->wlc = wlc;

	bsscfg->flags = flags;
	bsscfg->_ap = ap;

	bcopy(ea, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);

	/* XXX [NDIS restruct] Consider merging here from Falcon p2p code
	 * (under WLDSTA || WLP2P)
	 */

#ifdef WL_BSSCFG_TX_SUPR
	/* allocate suppression queue */
	if (flags & WLC_BSSCFG_TX_SUPR_ENAB) {
		if ((bsscfg->psq = (struct pktq *)
			MALLOCZ(wlc->osh, sizeof(struct pktq))) == NULL) {
			WL_ERROR(("wl%d: %s: failed to allocate sizeof(struct pktq)=%d\n",
				wlc->pub->unit, __FUNCTION__, (int)sizeof(struct pktq)));
			return BCME_NOMEM;
		}
		pktq_init(bsscfg->psq, WLC_PREC_COUNT, PKTQ_LEN_DEFAULT);
	}
#endif // endif

	/* initialize security state */
	bsscfg->wsec_index = -1;

	bsscfg->wsec = WLC_MFP_ENAB(wlc->pub) ? MFP_CAPABLE : 0;

	/* Match Wi-Fi default of true for aExcludeUnencrypted,
	 * instead of 802.11 default of false.
	 */
	bsscfg->wsec_restrict = TRUE;

	/* disable 802.1X authentication by default */
	bsscfg->eap_restrict = FALSE;

	/* disable WPA by default */
	bsscfg->WPA_auth = WPA_AUTH_DISABLED;

	/* APSD defaults */
	bsscfg->wme->wme_apsd = TRUE;
#ifdef ACKSUPR_MAC_FILTER
	/* acksupr initialization */
	bsscfg->acksupr_mac_filter = FALSE;
#endif /* ACKSUPR_MAC_FILTER */
#ifdef BRCMAPIVTW
	/* Broadcom AP IV trace window disabled by default. */
	bsscfg->brcm_ap_iv_tw = FALSE;
#endif /* BRCMAPIVTW */

	wlc_bsscfg_bss_init(wlc, bsscfg);

	/* Allocate a broadcast SCB for one of the band & reuse it for the other */
	if (!(bsscfg->flags & WLC_BSSCFG_NOBCMC)) {
		struct scb *scb;

		if (!IS_SINGLEBAND_5G(wlc->deviceid)) {
			if (wlc_bsscfg_bcmcscbinit(wlc, bsscfg, BAND_2G_INDEX)) {
				WL_ERROR(("wl%d: %s: wlc_bsscfg_bcmcscbinit failed for 2G\n",
					wlc->pub->unit, __FUNCTION__));
				return BCME_NOMEM;
			}
		} else {
			if (wlc_bsscfg_bcmcscbinit(wlc, bsscfg, BAND_5G_INDEX)) {
				WL_ERROR(("wl%d: %s: wlc_bsscfg_bcmcscbinit failed for 5G\n",
					wlc->pub->unit, __FUNCTION__));
				return BCME_NOMEM;
			}
		}

		if (NBANDS(wlc) > 1) {
			bsscfg->bcmc_scb[BAND_5G_INDEX] = bsscfg->bcmc_scb[BAND_2G_INDEX];
		}

		/* update the correct band for broadcast SCB */
		scb = WLC_BCMCSCB_GET(wlc, bsscfg);
		if  (scb->bandunit != wlc->band->bandunit) {
			wlc_internal_scb_switch_band(wlc, scb, wlc->band->bandunit);
		}
	}

#ifdef SMF_STATS
	if (wlc_bsscfg_smfsinit(wlc, bsscfg)) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_smfsinit failed\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}
#endif // endif

	/* create a new upper-edge driver interface */
	if (!(flags & WLC_BSSCFG_P2P_RESET)) {
		wlc_txq_info_t *primary_queue =  wlc->active_queue;
#ifdef WL_MULTIQUEUE
		primary_queue = wlc->primary_queue;
#endif /* WL_MULTIQUEUE */
		bsscfg->wlcif = wlc_wlcif_alloc(wlc, wlc->osh, WLC_IFTYPE_BSS, primary_queue);
		if (bsscfg->wlcif == NULL) {
			WL_ERROR(("wl%d: %s: failed to alloc wlcif\n",
			          wlc->pub->unit, __FUNCTION__));
			return BCME_NOMEM;
		}
		bsscfg->wlcif->u.bsscfg = bsscfg;

		/* create an OS interface */
		if (bsscfg == wlc_bsscfg_primary(wlc)) {
			/* primary interface has an implicit wlif which is assumed when
			 * the wlif pointer is NULL.
			 */
			/* XXX WES: really want to make this assumption go away and
			* have the wlcif of the primary interface have a non-null
			* wlif pointer.
			*/
			bsscfg->wlcif->if_flags |= WLC_IF_LINKED;
		}
		/* XXX All the wl_if support code is under the AP ifdef, so only
		* handle non-primary if creation for AP compiles
		*/
		else {
			if (!BSSCFG_HAS_NOIF(bsscfg)) {
				uint idx = WLC_BSSCFG_IDX(bsscfg);
				bsscfg->wlcif->wlif = wl_add_if(wlc->wl, bsscfg->wlcif, idx, NULL);
				if (bsscfg->wlcif->wlif == NULL) {
					WL_ERROR(("wl%d: %s: wl_add_if failed for"
						" index %d\n", wlc->pub->unit, __FUNCTION__, idx));
					return BCME_ERROR;
				}
				bsscfg->wlcif->if_flags |= WLC_IF_LINKED;
			}
			wlc_if_event(wlc, WLC_E_IF_ADD, bsscfg->wlcif);
		}
	}

	/* initialize our proprietary elt */
	brcm_ie = (brcm_ie_t *)&bsscfg->brcm_ie[0];
	bzero((char*)brcm_ie, sizeof(brcm_ie_t));
	brcm_ie->id = DOT11_MNG_PROPR_ID;
	brcm_ie->len = BRCM_IE_LEN - TLV_HDR_LEN;
	bcopy(BRCM_OUI, &brcm_ie->oui[0], DOT11_OUI_LEN);
	brcm_ie->ver = BRCM_IE_VER;

	wlc_bss_update_brcm_ie(wlc, bsscfg);

	/* obss init */
	wlc_ht_obss_scanparam_init(&bsscfg->obss->params);
	/* coex enable/disable */
	wlc_ht_coex_enab(bsscfg, COEX_ENAB(wlc->pub));

#ifdef MULTIAP
	/* initialize multiap  ie */
	multiap_ie = (multiap_ie_t *)&bsscfg->multiap_ie[0];
	multiap_ie->id = DOT11_MNG_VS_ID;
	multiap_ie->len = MAP_IE_FIXED_LEN;
	memcpy(&multiap_ie->oui[0], WFA_OUI,  DOT11_OUI_LEN);
	multiap_ie->type = WFA_OUI_TYPE_MULTIAP;
	wlc_bss_update_multiap_ie(wlc, bsscfg);
#endif	/* MULTIAP */

	if (WLWNM_ENAB(wlc->pub)) {
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_BSSTRANS_MGMT, TRUE);
	}
#ifdef L2_FILTER
	if (L2_FILTER_ENAB(wlc->pub)) {
		if (wlc_l2_filter_proxy_arp_enab(wlc) && ap) {
			wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_PROXY_ARP, TRUE);
		}
	}
#endif // endif
	return BCME_OK;
}

static int
wlc_bsscfg_bcmcscbinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint band)
{
	ASSERT(bsscfg != NULL);
	ASSERT(wlc != NULL);

	if (!bsscfg->bcmc_scb[band]) {
		bsscfg->bcmc_scb[band] =
		        wlc_internalscb_alloc(wlc, bsscfg, &ether_bcast, wlc->bandstate[band]);
		WL_INFORM(("wl%d: wlc_bsscfg_bcmcscbinit: band %d: alloc internal scb 0x%p "
		           "for bsscfg 0x%p\n",
		           wlc->pub->unit, band, bsscfg->bcmc_scb[band], bsscfg));
	}
	if (!bsscfg->bcmc_scb[band]) {
		WL_ERROR(("wl%d: %s: fail to alloc scb for bsscfg 0x%p\n",
		          wlc->pub->unit, __FUNCTION__, bsscfg));
		return BCME_NOMEM;
	}

	return  0;
}

#ifdef MBSS
/**
 * Write the base MAC/BSSID into shared memory.  For MBSS, the MAC and BSSID
 * are required to be the same.
 */
int
wlc_write_mbss_basemac(wlc_info_t *wlc, const struct ether_addr *addr)
{
	uint16 mac_l;
	uint16 mac_m;
	uint16 mac_h;

	mac_l = addr->octet[0] | (addr->octet[1] << 8);
	mac_m = addr->octet[2] | (addr->octet[3] << 8);
	/* Mask low bits of BSSID base */
	mac_h = addr->octet[4] | ((addr->octet[5] & ~(wlc->mbss_ucidx_mask)) << 8);

	wlc_write_shm(wlc, SHM_MBSS_BSSID0, mac_l);
	wlc_write_shm(wlc, SHM_MBSS_BSSID1, mac_m);
	wlc_write_shm(wlc, SHM_MBSS_BSSID2, mac_h);

	return BCME_OK;
}

/** Generate a MAC address for the MBSS AP BSS config */
static int
wlc_bsscfg_macgen(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int ii, jj;
	bool collision = TRUE;
	int cfg_idx = WLC_BSSCFG_IDX(cfg);
	struct ether_addr newmac;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	if (ETHER_ISNULLADDR(&wlc->vether_base)) {
		/* initialize virtual MAC base for MBSS
		 * the base should come from an external source,
		 * this initialization is in case one isn't provided
		 */
		bcopy(&wlc->pub->cur_etheraddr, &wlc->vether_base, ETHER_ADDR_LEN);
		/* avoid collision */
		wlc->vether_base.octet[5] += 1;

		/* force locally administered address */
		ETHER_SET_LOCALADDR(&wlc->vether_base);
	}

	bcopy(&wlc->vether_base, &newmac, ETHER_ADDR_LEN);

	/* brute force attempt to make a MAC for this interface,
	 * the user didn't provide one.
	 * outside loop limits the # of times we increment the low byte of
	 * the MAC address we're attempting to create, and the inner loop
	 * checks for collisions with other configs.
	 */
	for (ii = 0; (ii < WLC_MAXBSSCFG) && (collision == TRUE); ii++) {
		collision = FALSE;
		for (jj = 0; jj < WLC_MAXBSSCFG; jj++) {
			/* don't compare with the bss config we're updating */
			if (jj == cfg_idx || (!wlc->bsscfg[jj]))
				continue;
			if (EADDR_TO_UC_IDX(wlc->bsscfg[jj]->cur_etheraddr, wlc->mbss_ucidx_mask) ==
			    EADDR_TO_UC_IDX(newmac, wlc->mbss_ucidx_mask)) {
				collision = TRUE;
				break;
			}
		}
		if (collision == TRUE) /* increment and try again */
			newmac.octet[5] = (newmac.octet[5] & ~(wlc->mbss_ucidx_mask))
			        | (wlc->mbss_ucidx_mask & (newmac.octet[5]+1));
		else
			bcopy(&newmac, &cfg->cur_etheraddr, ETHER_ADDR_LEN);
	}

	if (ETHER_ISNULLADDR(&cfg->cur_etheraddr)) {
		WL_MBSS(("wl%d.%d: wlc_bsscfg_macgen couldn't generate MAC address\n",
		         wlc->pub->unit, cfg_idx));

		return BCME_BADADDR;
	}
	else {
		WL_MBSS(("wl%d.%d: wlc_bsscfg_macgen assigned MAC %s\n",
		         wlc->pub->unit, cfg_idx,
		         bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf)));
		return BCME_OK;
	}
}
#endif /* MBSS */

#if defined(AP)
uint16
wlc_bsscfg_newaid(wlc_bsscfg_t *cfg)
{
	int pos;

	ASSERT(cfg);

	/* get an unused number from aidmap */
	for (pos = 0; pos < cfg->wlc->pub->tunables->maxscb; pos++) {
		if (isclr(cfg->aidmap, pos)) {
			WL_ASSOC(("wlc_bsscfg_newaid marking bit = %d for "
			          "bsscfg %d AIDMAP\n", pos,
			          WLC_BSSCFG_IDX(cfg)));
			/* mark the position being used */
			setbit(cfg->aidmap, pos);
			break;
		}
	}
	ASSERT(pos < cfg->wlc->pub->tunables->maxscb);

	return ((uint16)AIDMAP2AID(pos));
}
#endif /* AP */

#ifdef STA
/** Set/reset association parameters */
int
wlc_bsscfg_assoc_params_set(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len)
{
	ASSERT(wlc != NULL);
	ASSERT(bsscfg != NULL);

	if (bsscfg->assoc_params != NULL) {
		MFREE(wlc->osh, bsscfg->assoc_params, bsscfg->assoc_params_len);
		bsscfg->assoc_params = NULL;
		bsscfg->assoc_params_len = 0;
	}
	if (assoc_params == NULL || assoc_params_len == 0)
		return BCME_OK;
	if ((bsscfg->assoc_params = MALLOC(wlc->osh, assoc_params_len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	bcopy(assoc_params, bsscfg->assoc_params, assoc_params_len);
	bsscfg->assoc_params_len = (uint16)assoc_params_len;

	return BCME_OK;
}

void
wlc_bsscfg_assoc_params_reset(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	if (bsscfg != NULL)
		wlc_bsscfg_assoc_params_set(wlc, bsscfg, NULL, 0);
}

/** Set/reset scan parameters */
int
wlc_bsscfg_scan_params_set(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wl_join_scan_params_t *scan_params)
{
	ASSERT(bsscfg != NULL);

	if (scan_params == NULL) {
		if (bsscfg->scan_params != NULL) {
			MFREE(wlc->osh, bsscfg->scan_params, sizeof(wl_join_scan_params_t));
			bsscfg->scan_params = NULL;
		}
		return BCME_OK;
	}
	else if (bsscfg->scan_params != NULL ||
	         (bsscfg->scan_params = MALLOC(wlc->osh, sizeof(wl_join_scan_params_t))) != NULL) {
		bcopy(scan_params, bsscfg->scan_params, sizeof(wl_join_scan_params_t));
		return BCME_OK;
	}

	WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
	return BCME_NOMEM;
}

void
wlc_bsscfg_scan_params_reset(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	if (bsscfg != NULL)
		wlc_bsscfg_scan_params_set(wlc, bsscfg, NULL);
}
#endif /* STA */

void
wlc_bsscfg_SSID_set(wlc_bsscfg_t *bsscfg, uint8 *SSID, int len)
{
	ASSERT(bsscfg != NULL);
	ASSERT(len <= DOT11_MAX_SSID_LEN);

	if ((bsscfg->SSID_len = (uint8)len) > 0) {
		ASSERT(SSID != NULL);
		/* need to use memove here to handle overlapping copy */
		memmove(bsscfg->SSID, SSID, len);

		if (len < DOT11_MAX_SSID_LEN)
			bzero(&bsscfg->SSID[len], DOT11_MAX_SSID_LEN - len);
		return;
	}

	bzero(bsscfg->SSID, DOT11_MAX_SSID_LEN);
}

/**
 * Vendor IE lists
 */

int
wlc_vndr_ie_getlen_ext(wlc_bsscfg_t *bsscfg, vndr_ie_filter_fn_t filter,
	uint32 pktflag, int *totie)
{
	return wlc_vndr_ie_list_getlen_ext(bsscfg->vndr_ie_listp,
		(vndr_ie_list_filter_fn_t)filter, bsscfg, pktflag, totie);
}

uint8 *
wlc_vndr_ie_write_ext(wlc_bsscfg_t *bsscfg, vndr_ie_write_filter_fn_t filter,
	uint type, uint8 *cp, int buflen, uint32 pktflag)
{
	return wlc_vndr_ie_list_write_ext(bsscfg->vndr_ie_listp,
		(vndr_ie_list_write_filter_fn_t)filter, bsscfg, type,
		cp, buflen, pktflag);
}

/**
 * Create a vendor IE information element object and add to the list.
 * Return value: address of the new object.
 */
vndr_ie_listel_t *
wlc_vndr_ie_add_elem(wlc_bsscfg_t *bsscfg, uint32 pktflag, vndr_ie_t *vndr_iep)
{
	return wlc_vndr_ie_list_add_elem(bsscfg->wlc->osh, &bsscfg->vndr_ie_listp,
		pktflag, vndr_iep);
}

int
wlc_vndr_ie_add(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len)
{
	return wlc_vndr_ie_list_add(bsscfg->wlc->osh, &bsscfg->vndr_ie_listp, ie_buf, len);
}

int
wlc_vndr_ie_del(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len)
{
	return wlc_vndr_ie_list_del(bsscfg->wlc->osh, &bsscfg->vndr_ie_listp, ie_buf, len);
}

int
wlc_vndr_ie_get(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len, uint32 pktflag)
{
	return wlc_vndr_ie_list_get(bsscfg->vndr_ie_listp, ie_buf, len, pktflag);
}

/**
 * Modify the data in the previously added vendor IE info.
 */
vndr_ie_listel_t *
wlc_vndr_ie_mod_elem(wlc_bsscfg_t *bsscfg, vndr_ie_listel_t *old_listel,
	uint32 pktflag, vndr_ie_t *vndr_iep)
{
	return wlc_vndr_ie_list_mod_elem(bsscfg->wlc->osh, &bsscfg->vndr_ie_listp,
		old_listel, pktflag, vndr_iep);
}

int
wlc_vndr_ie_mod_elem_by_type(wlc_bsscfg_t *bsscfg, uint8 type,
	uint32 pktflag, vndr_ie_t *vndr_iep)
{
	return wlc_vndr_ie_list_mod_elem_by_type(bsscfg->wlc->osh, &bsscfg->vndr_ie_listp,
		type, pktflag, vndr_iep);
}

int
wlc_vndr_ie_del_by_type(wlc_bsscfg_t *bsscfg, uint8 type)
{
	return wlc_vndr_ie_list_del_by_type(bsscfg->wlc->osh, &bsscfg->vndr_ie_listp, type);
}

uint8 *
wlc_vndr_ie_find_by_type(wlc_bsscfg_t *bsscfg, uint8 type)
{
	return wlc_vndr_ie_list_find_by_type(bsscfg->vndr_ie_listp, type);
}

uint8 *
wlc_bsscfg_get_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 ie_type)
{
	uint8 *ie_data = NULL;

	if (wlc_11u_is_11u_ie(wlc->m11u, ie_type)) {
		ie_data = wlc_11u_get_ie(wlc->m11u, bsscfg, ie_type);
	}
	else {
		ie_data = wlc_vndr_ie_find_by_type(bsscfg, ie_type);
	}

	return ie_data;
}

int
wlc_bsscfg_set_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *ie_data,
	bool *bcn_upd, bool *prbresp_upd)
{
	int err = BCME_OK;
	uint8 ie_type;

	ie_type = ie_data[TLV_TAG_OFF];

	if (wlc_11u_is_11u_ie(wlc->m11u, ie_type)) {
		err = wlc_11u_set_ie(wlc->m11u, bsscfg, ie_data, bcn_upd,
			prbresp_upd);
	}
	else {
		err = BCME_UNSUPPORTED;
	}

	return err;
}

static void
wlc_bsscfg_update_ext_cap_len(wlc_bsscfg_t *bsscfg)
{
	int i;

	for (i = DOT11_EXTCAP_LEN_MAX - 1; i >= 0; i--) {
		if (bsscfg->ext_cap[i] != 0)
			break;
	}

	bsscfg->ext_cap_len = i + 1;

	if (isset(bsscfg->ext_cap, DOT11_EXT_CAP_SPSMP)) {
		if (bsscfg->ext_cap_len < DOT11_EXTCAP_LEN_SI)
			bsscfg->ext_cap_len = DOT11_EXTCAP_LEN_SI;
	}
}

void
wlc_bsscfg_set_ext_cap(wlc_bsscfg_t *bsscfg, uint32 bit, bool val)
{
	if (val)
		setbit(bsscfg->ext_cap, bit);
	else
		clrbit(bsscfg->ext_cap, bit);
	wlc_bsscfg_update_ext_cap_len(bsscfg);
}

void
wlc_bsscfg_set_ext_cap_spsmp(wlc_bsscfg_t *bsscfg, bool spsmp, uint8 si)
{
	uint8 val;
	val = bsscfg->ext_cap[DOT11_EXT_CAP_SI / 8];
	val &= ~DOT11_EXT_CAP_SI_MASK;
	if (spsmp) {
		setbit(bsscfg->ext_cap, DOT11_EXT_CAP_SPSMP);
		val |= (si << (DOT11_EXT_CAP_SI % 8));
	} else {
		clrbit(bsscfg->ext_cap, DOT11_EXT_CAP_SPSMP);
	}
	bsscfg->ext_cap[DOT11_EXT_CAP_SI / 8] = val;
	wlc_bsscfg_update_ext_cap_len(bsscfg);
}

#ifdef SMF_STATS
static void
_wlc_bsscfg_smfsinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	uint8 i;
	wlc_smf_stats_t *smf_stats;

	ASSERT(bsscfg->smfs_info);

	bzero(bsscfg->smfs_info, sizeof(wlc_smfs_info_t));

	bsscfg->smfs_info->enable = 1;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		smf_stats = &bsscfg->smfs_info->smf_stats[i];

		smf_stats->smfs_main.type = i;
		smf_stats->smfs_main.version = SMFS_VERSION;

		if ((i == SMFS_TYPE_AUTH) || (i == SMFS_TYPE_ASSOC) ||
			(i == SMFS_TYPE_REASSOC))
			smf_stats->smfs_main.codetype = SMFS_CODETYPE_SC;
		else
			smf_stats->smfs_main.codetype = SMFS_CODETYPE_RC;
	}

}
int
wlc_bsscfg_smfsinit(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	if (!bsscfg->smfs_info) {
		bsscfg->smfs_info = MALLOC(wlc->osh, sizeof(wlc_smfs_info_t));
		if (!bsscfg->smfs_info) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
	}

	_wlc_bsscfg_smfsinit(wlc, bsscfg);

	return 0;

}

static int
smfs_elem_free(struct wlc_info *wlc, wlc_smf_stats_t *smf_stats)
{
	wlc_smfs_elem_t *headptr = smf_stats->stats;
	wlc_smfs_elem_t *curptr;

	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		MFREE(wlc->osh, curptr, sizeof(wlc_smfs_elem_t));
	}
	smf_stats->stats = NULL;
	return 0;
}

static int
wlc_bsscfg_smfsfree(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	int i;

	if (!bsscfg->smfs_info)
		return 0;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		wlc_smf_stats_t *smf_stats = &bsscfg->smfs_info->smf_stats[i];
		smfs_elem_free(wlc, smf_stats);
	}
	MFREE(wlc->osh, bsscfg->smfs_info, sizeof(wlc_smfs_info_t));
	bsscfg->smfs_info = NULL;

	return 0;
}

static int
linear_search_u16(const uint16 array[], uint16 key, int size)
{
	int n;
	for (n = 0; n < size; ++n) {
		if (array[ n ] == key) {
			return n;
		}
	}
	return -1;
}

static wlc_smfs_elem_t *
smfs_elem_create(osl_t *osh, uint16 code)
{
	wlc_smfs_elem_t *elem = NULL;
	elem = MALLOC(osh, sizeof(wlc_smfs_elem_t));

	if (elem) {
		elem->next = NULL;
		elem->smfs_elem.code = code;
		elem->smfs_elem.count = 0;
	}

	return elem;
}

static wlc_smfs_elem_t *
smfs_elem_find(uint16 code, wlc_smfs_elem_t *start)
{
	while (start != NULL) {
		if (code == start->smfs_elem.code)
			break;
		start = start->next;
	}
	return start;
}

/** sort based on code define */
static void
smfs_elem_insert(wlc_smfs_elem_t **rootp, wlc_smfs_elem_t *new)
{
	wlc_smfs_elem_t *curptr;
	wlc_smfs_elem_t *previous;

	curptr = *rootp;
	previous = NULL;

	while (curptr && (curptr->smfs_elem.code < new->smfs_elem.code)) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;

	if (previous == NULL)
		*rootp = new;
	else
		previous->next = new;
}

static bool
smfstats_codetype_included(uint16 code, uint16 codetype)
{
	bool included = FALSE;
	int indx = -1;

	if (codetype == SMFS_CODETYPE_SC)
		indx = linear_search_u16(smfs_sc_table, code,
		  sizeof(smfs_sc_table)/sizeof(uint16));
	else
		indx = linear_search_u16(smfs_rc_table, code,
		  sizeof(smfs_rc_table)/sizeof(uint16));

	if (indx != -1)
		included = TRUE;

	return included;
}

static int
smfstats_update(wlc_info_t *wlc, wlc_smf_stats_t *smf_stats, uint16 code)
{
	uint8 codetype = smf_stats->smfs_main.codetype;
	uint32 count_excl = smf_stats->count_excl;
	wlc_smfs_elem_t *elem = smf_stats->stats;
	wlc_smfs_elem_t *new_elem = NULL;
	bool included = smfstats_codetype_included(code, codetype);
	osl_t *osh;

	if (!included && (count_excl > MAX_SCRC_EXCLUDED)) {
		WL_INFORM(("%s: sc/rc  outside the scope, discard\n", __FUNCTION__));
		return 0;
	}

	osh = wlc->osh;
	new_elem = smfs_elem_find(code, elem);

	if (!new_elem) {
		new_elem = smfs_elem_create(osh, code);

		if (!new_elem) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		else {
			smfs_elem_insert(&smf_stats->stats, new_elem);
			if (!included)
				smf_stats->count_excl++;
			smf_stats->smfs_main.count_total++;
		}
	}
	new_elem->smfs_elem.count++;

	return 0;
}

int
wlc_smfstats_update(struct wlc_info *wlc, wlc_bsscfg_t *cfg, uint8 smfs_type, uint16 code)
{
	wlc_smf_stats_t *smf_stats;
	int err = 0;

	ASSERT(cfg->smfs_info);

	if (!SMFS_ENAB(cfg))
		return err;

	smf_stats = &cfg->smfs_info->smf_stats[smfs_type];

	if (code == SMFS_CODE_MALFORMED) {
		smf_stats->smfs_main.malformed_cnt++;
		return 0;
	}

	if (code == SMFS_CODE_IGNORED) {
		smf_stats->smfs_main.ignored_cnt++;
		return 0;
	}

	err = smfstats_update(wlc, smf_stats, code);

	return err;
}

int
wlc_bsscfg_get_smfs(wlc_bsscfg_t *cfg, int idx, char *buf, int len)
{
	wlc_smf_stats_t *smf_stat;
	wlc_smfs_elem_t *elemt;
	int used_len = 0;
	int err = 0;

	ASSERT((uint)len >= sizeof(wl_smf_stats_t));

	if (idx < 0 || idx >= SMFS_TYPE_MAX) {
		err = BCME_RANGE;
		return err;
	}

	smf_stat =  &cfg->smfs_info->smf_stats[idx];
	bcopy(&smf_stat->smfs_main, buf, sizeof(wl_smf_stats_t));

	buf += WL_SMFSTATS_FIXED_LEN;
	used_len += WL_SMFSTATS_FIXED_LEN;

	elemt = smf_stat->stats;

	while (elemt) {
		used_len += sizeof(wl_smfs_elem_t);
		if (used_len > len) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(&elemt->smfs_elem, buf, sizeof(wl_smfs_elem_t));
		elemt = elemt->next;
		buf += sizeof(wl_smfs_elem_t);
	}
	return err;
}

int
wlc_bsscfg_clear_smfs(struct wlc_info *wlc, wlc_bsscfg_t *cfg)
{
	int i;

	if (!cfg->smfs_info)
		return 0;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		wlc_smf_stats_t *smf_stats = &cfg->smfs_info->smf_stats[i];
		smfs_elem_free(wlc, smf_stats);

		smf_stats->smfs_main.length = 0;
		smf_stats->smfs_main.ignored_cnt = 0;
		smf_stats->smfs_main.malformed_cnt = 0;
		smf_stats->smfs_main.count_total = 0;
		smf_stats->count_excl = 0;
	}
	return 0;
}
#endif /* SMF_STATS */

#ifdef WL_BSSCFG_TX_SUPR
void
wlc_bsscfg_tx_stop(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = bsscfg->wlc;
	struct pktq *txq;
	void *pkt;
	int prec;
	struct scb *scb;
	bool norm = FALSE;

	/* Nothing to do */
	if (BSS_TX_SUPR(bsscfg))
		return;

	bsscfg->flags |= WLC_BSSCFG_TX_SUPR;

	/* PropTx: wlc_txfifo_suppress() and wlc_wlfc_flush_pkts_to_host()
	 * takes care of flushing out packets
	 */
	if (PROP_TXSTATUS_ENAB(wlc->pub))
		return;

	/* If there is anything in the data fifo then allow it to drain */
	if (TXPKTPENDTOT(wlc) > 0)
		wlc->block_datafifo |= DATA_BLOCK_TX_SUPR;

	txq = &bsscfg->wlcif->qi->q;

	WL_P2P(("wl%d.%d: %s: pending %d packets %d\n",
	        wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
	        TXPKTPENDTOT(wlc), pktq_len(txq)));

	/* s/w tx suppression: move all packets in the s/w queue to appropriate queue
	 * before ucode's starting to suppresspackets in the DMA in order to maintain
	 * the correct packets' ordering...
	 */
	PKTQ_PREC_ITER(txq, prec) {
		void *tail_pkt = pktq_ppeek_tail(txq, prec);

		while ((pkt = pktq_ppeek(txq, prec)) != NULL) {
			pkt = pktq_pdeq(txq, prec);
			scb = WLPKTTAGSCBGET(pkt);
			ASSERT(scb != NULL);
			if (SCB_BSSCFG(scb) == bsscfg) {
				if (wlc_pkt_abs_supr_enq(wlc, scb, pkt)) {
					if (pkt == tail_pkt) {
						PKTFREE(wlc->osh, pkt, TRUE);
						break;
					}
					PKTFREE(wlc->osh, pkt, TRUE);
				}
				else {
					if (pkt == tail_pkt)
						break;
				}
				norm = TRUE;
				continue;
			}
			pktq_penq(txq, prec, pkt);
			if (pkt == tail_pkt)
				break;
		}
	}

	if (norm)
		wlc_bsscfg_tx_supr_norm(wlc);
}

/** Call after the FIFO has drained */
void
wlc_bsscfg_tx_check(wlc_info_t *wlc)
{
	ASSERT(TXPKTPENDTOT(wlc) == 0);

	WL_P2P(("wl%d: %s: TX SUPR %d\n",
	        wlc->pub->unit, __FUNCTION__,
	        (wlc->block_datafifo & DATA_BLOCK_TX_SUPR) != 0));

	if (wlc->block_datafifo & DATA_BLOCK_TX_SUPR) {
		int i;
		wlc_bsscfg_t *bsscfg;

		wlc->block_datafifo &= ~DATA_BLOCK_TX_SUPR;

		/* Now complete all the pending transitions */
		FOREACH_BSS(wlc, i, bsscfg) {
			if (bsscfg->tx_start_pending) {
				bsscfg->tx_start_pending = FALSE;
				wlc_bsscfg_tx_start(bsscfg);
			}
		}

		wlc_bsscfg_tx_supr_norm(wlc);
	}
}

/** "normalize" all queues in all BSSs - all STA's PS queues, all BSS's suppression queues */
void
wlc_bsscfg_tx_supr_norm(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;
#if defined(BCMDBG) || defined(WLMSG_PS)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	FOREACH_UP_AP(wlc, idx, cfg) {
		struct scb_iter scbiter;
		struct scb *scb;

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			if (!SCB_PS(scb) || SCB_ISMULTI(scb) ||
			    !SCB_ASSOCIATED(scb) || !SCB_P2P(scb))
				continue;
			WL_PS(("wl%d.%d: %s: normalize PSQ for STA %s PS %d\n",
			       wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			       bcm_ether_ntoa(&scb->ea, eabuf), SCB_PS(scb)));
			wlc_apps_scb_psq_norm(wlc, scb);
		}
	}

	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg->psq == NULL ||
#ifdef WLP2P
		    !BSS_P2P_ENAB(wlc, cfg) ||
#endif // endif
		    FALSE)
			continue;
		WL_PS(("wl%d: %s: normalize TXQ for BSS %s\n",
		       wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&cfg->BSSID, eabuf)));
		wlc_pktq_supr_norm(wlc, cfg->psq);
	}
}

void
wlc_bsscfg_tx_start(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = bsscfg->wlc;
	struct pktq *txq;
	void *pkt;
	int prec;

	/* Nothing to do */
	if (!BSS_TX_SUPR(bsscfg))
		return;

	WL_P2P(("wl%d.%d: %s: TX SUPR %d pending %d packets %d\n",
	        wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
	        (wlc->block_datafifo & DATA_BLOCK_TX_SUPR) != 0,
	        TXPKTPENDTOT(wlc), pktq_len(bsscfg->psq)));

	if (wlc->block_datafifo & DATA_BLOCK_TX_SUPR) {
		/* Finish the transition first to avoid reordering frames */
		if (TXPKTPENDTOT(wlc) > 0) {
			bsscfg->tx_start_pending = TRUE;
			return;
		}
		wlc->block_datafifo &= ~DATA_BLOCK_TX_SUPR;
	}

	bsscfg->flags &= ~WLC_BSSCFG_TX_SUPR;

	/* Dump all the packets from bsscfg->psq to txq but to the front */
	/* This is done to preserve the ordering w/o changing the precedence level
	 * since AMPDU module keeps track of sequence numbers according to their
	 * precedence!
	 */
	txq = &bsscfg->wlcif->qi->q;

	while ((pkt = pktq_deq_tail(bsscfg->psq, &prec))) {
		if (!wlc_prec_enq_head(wlc, txq, pkt, prec, TRUE)) {
			WL_P2P(("wl%d: wlc_bsscfg_tx_start: txq full, frame discarded\n",
			          wlc->pub->unit));
			PKTFREE(wlc->osh, pkt, TRUE);
			WLCNTINCR(wlc->pub->_cnt->txnobuf);
		}
	}

	if (!pktq_empty(txq)) {
		WL_P2P(("wl%d.%d: %s: resend packets %d\n",
		        wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		        __FUNCTION__, pktq_len(txq)));
		wlc_send_q(wlc, bsscfg->wlcif->qi);
	}
}

bool
wlc_bsscfg_tx_supr_enq(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt)
{
	ASSERT(pkt != NULL);

	WLPKTTAG(pkt)->flags3 |= WLF3_SUPR;
	return wlc_bsscfg_tx_abs_enq(wlc, cfg, pkt, WLC_PRIO_TO_HI_PREC(PKTPRIO(pkt)));
}

bool
wlc_bsscfg_tx_abs_enq(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu, uint prec)
{
	ASSERT(bsscfg != NULL);

	/* Caller should free the packet if it cannot be accomodated */
	if (!wlc_prec_enq(wlc, bsscfg->psq, sdu, prec)) {
		WL_P2P(("wl%d: %s: txq full, frame discarded\n",
		        wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return TRUE;
	}

	return FALSE;
}
#endif /* WL_BSSCFG_TX_SUPR */

#ifdef AP
void
wlc_bsscfg_bcn_disable(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_bcn_disable %p #of stas %d\n",
	          wlc->pub->unit, cfg, wlc_bss_assocscb_getcnt(wlc, cfg)));

	cfg->flags &= ~WLC_BSSCFG_HW_BCN;
	if (cfg->up) {
		wlc_suspend_mac_and_wait(wlc);
		wlc_bmac_write_ihr(wlc->hw, 0x47, 3);
		wlc_enable_mac(wlc);
	}
}

void
wlc_bsscfg_bcn_enable(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_bcn_enable %p #of stas %d\n",
	          wlc->pub->unit, cfg, wlc_bss_assocscb_getcnt(wlc, cfg)));

	cfg->flags |= WLC_BSSCFG_HW_BCN;
	wlc_bss_update_beacon(wlc, cfg);
}
#endif /* AP */

#ifdef STA
int
wlc_bsscfg_wsec_key_buf_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	if (!bsscfg->wsec_key_buf_info) {
		bsscfg->wsec_key_buf_info = MALLOC(wlc->osh, sizeof(wsec_key_buf_info_t));
		if (!bsscfg->wsec_key_buf_info) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
	}

	memset(bsscfg->wsec_key_buf_info, 0, sizeof(wsec_key_buf_info_t));

	return 0;
}

int
wlc_bsscfg_wsec_key_buf_free(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	if (!bsscfg->wsec_key_buf_info)
		return 0;

	MFREE(wlc->osh, bsscfg->wsec_key_buf_info, sizeof(wsec_key_buf_info_t));
	bsscfg->wsec_key_buf_info = NULL;

	return 0;
}

void
wlc_bsscfg_wsec_session_reset(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wsec_key_buf_info_t *buf_info = cfg->wsec_key_buf_info;

	(void)wlc;

	if (buf_info == NULL)
		return;

	buf_info->eapol_4way_m1_rxed = TRUE;
	buf_info->eapol_4way_m4_txed = FALSE;
	memset(buf_info->key_buffered, 0, sizeof(bool) * BSSCFG_BUF_KEY_NUM);
}
#endif /* STA */

/** helper function for per-port code to call to query the "current" chanspec of a BSS */
chanspec_t
wlc_get_home_chanspec(wlc_bsscfg_t *cfg)
{
	ASSERT(cfg != NULL);

	if (cfg->associated)
		return cfg->current_bss->chanspec;

	return cfg->wlc->home_chanspec;
}

/** helper function for per-port code to call to query the "current" bss info of a BSS */
wlc_bss_info_t *
wlc_get_current_bss(wlc_bsscfg_t *cfg)
{
	ASSERT(cfg != NULL);

	return cfg->current_bss;
}

/**
 * Do multicast filtering
 * returns TRUE on success (reject/filter frame).
 */
bool
wlc_bsscfg_mcastfilter(wlc_bsscfg_t *cfg, struct ether_addr *da)
{
	unsigned int i;

	for (i = 0; i < cfg->nmulticast; i++) {
		if (bcmp((void*)da, (void*)&cfg->multicast[i],
			ETHER_ADDR_LEN) == 0)
			return FALSE;
	}

	return TRUE;
}

/** per bsscfg init tx reported rate mechanism */
void
wlc_bsscfg_reprate_init(wlc_bsscfg_t *bsscfg)
{
	bsscfg->txrspecidx = 0;
	bzero((char*)bsscfg->txrspec, sizeof(bsscfg->txrspec));
}

#if defined(STA) && defined(DBG_BCN_LOSS)
int wlc_get_bcn_dbg_info(wlc_bsscfg_t *cfg, struct ether_addr *addr,
	struct wlc_scb_dbg_bcn *dbg_bcn)
{
	wlc_info_t *wlc = cfg->wlc;

	if (cfg->BSS) {
		struct scb *ap_scb = wlc_scbfindband(wlc, cfg, addr,
			CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec));
		if (ap_scb) {
			memcpy(dbg_bcn, &(ap_scb->dbg_bcn), sizeof(struct wlc_scb_dbg_bcn));
			return BCME_OK;
		}
	}
	return BCME_ERROR;
}
#endif /* defined(STA) && defined(DBG_BCN_LOSS) */

/**
 * Loop over bsscfg specific txrspec history, looking up rate bins, and summing
 * nfrags into appropriate supported rate bin. Return pointers to
 * most used ratespec and highest used ratespec.
 */
void
wlc_get_rate_histo_bsscfg(wlc_bsscfg_t *bsscfg, wl_mac_ratehisto_res_t *rhist,
	ratespec_t *most_used_ratespec, ratespec_t *highest_used_ratespec)
{
	int i;
	ratespec_t rspec;
	uint max_frags = 0;
	uint rate, mcs, nss;
	uint high_frags = 0;

	*most_used_ratespec = 0x0;
	*highest_used_ratespec = 0x0;

	/* [0] = rspec, [1] = nfrags */
	for (i = 0; i < NTXRATE; i++) {
		rspec = bsscfg->txrspec[i][0]; /* circular buffer of prev MPDUs tx rates */
		/* skip empty rate specs */
		if (rspec == 0)
			continue;
		if (RSPEC_ISVHT(rspec)) {
			mcs = rspec & RSPEC_VHT_MCS_MASK;
			nss = ((rspec & RSPEC_VHT_NSS_MASK) >> RSPEC_VHT_NSS_SHIFT) - 1;
			ASSERT(mcs < WL_RATESET_SZ_VHT_MCS);
			ASSERT(nss < WL_TX_CHAINS_MAX);
			rhist->vht[mcs][nss] += bsscfg->txrspec[i][1]; /* [1] is for fragments */
			if (rhist->vht[mcs][nss] > max_frags) {
				max_frags = rhist->vht[mcs][nss];
				*most_used_ratespec = rspec;
			}
		} else if (IS_MCS(rspec)) {
			mcs = rspec & RSPEC_RATE_MASK;

#if defined(WLPROPRIETARY_11N_RATES) /* avoid ROM abandoning of this function */
			if (IS_PROPRIETARY_11N_MCS(mcs)) {
				mcs -= WLC_11N_FIRST_PROP_MCS;
				rhist->prop11n_mcs[mcs] += bsscfg->txrspec[i][1];
				if (rhist->prop11n_mcs[mcs] > max_frags) {
					max_frags = rhist->prop11n_mcs[mcs];
					*most_used_ratespec = rspec;
				}
			} else {
#endif /* WLPROPRIETARY_11N_RATES */
				/* ASSERT(mcs < WL_RATESET_SZ_HT_IOCTL * WL_TX_CHAINS_MAX); */
				if (mcs >= WL_RATESET_SZ_HT_IOCTL * WL_TX_CHAINS_MAX)
					continue;  /* Ignore mcs 32 if it ever comes through. */
				rhist->mcs[mcs] += bsscfg->txrspec[i][1];
				if (rhist->mcs[mcs] > max_frags) {
					max_frags = rhist->mcs[mcs];
					*most_used_ratespec = rspec;
				}
#if defined(WLPROPRIETARY_11N_RATES) /* avoid ROM abandoning of this function */
			}
#endif /* WLPROPRIETARY_11N_RATES */
		} else {
			rate = rspec & RSPEC_RATE_MASK;
			ASSERT(rate < (WLC_MAXRATE + 1));
			rhist->rate[rate] += bsscfg->txrspec[i][1];
			if (rhist->rate[rate] > max_frags) {
				max_frags = rhist->rate[rate];
				*most_used_ratespec = rspec;
			}
		}

		rate = RSPEC2KBPS(rspec);
		if (rate > high_frags) {
			high_frags = rate;
			*highest_used_ratespec = rspec;
		}
	}
	return;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
/* Set this definition to 1 for additional verbosity */
#define BSSCFG_EXTRA_VERBOSE 1

#define SHOW_SHM(wlc, bf, addr, name) do { \
		uint16 tmpval; \
		tmpval = wlc_read_shm((wlc), (addr)); \
		bcm_bprintf(bf, "%15s     offset: 0x%04x (0x%04x)     0x%04x (%6d)\n", \
			name, addr / 2, addr, tmpval, tmpval); \
	} while (0)

static void
wlc_assoc_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_assoc_t *as;

	ASSERT(cfg != NULL);

	as = cfg->assoc;
	if (as == NULL)
		return;

	bcm_bprintf(b, "-------- assoc states (%d) --------\n", WLC_BSSCFG_IDX(cfg));
	bcm_bprintf(b, "type %u state %u flags 0x%x\n", as->type, as->state, as->flags);
	bcm_bprintf(b, "preserved %d recreate_bi_to %u verify_to %u\n",
	            as->preserved, as->recreate_bi_timeout, as->verify_timeout);
	bcm_bprintf(b, "retry_max %u ess_retries %u bss_retries %u\n",
	            as->retry_max, as->ess_retries, as->bss_retries);
}

static void
wlc_roam_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_roam_t *roam;

	ASSERT(cfg != NULL);

	roam = cfg->roam;
	if (roam == NULL)
		return;

	bcm_bprintf(b, "-------- roam states (%d) --------\n", WLC_BSSCFG_IDX(cfg));
	bcm_bprintf(b, "off %d\n", roam->off);
	bcm_bprintf(b, "reason %u\n", roam->reason);
	bcm_bprintf(b, "bcn_timeout %u time_since_bcn %u bcns_lost %d\n",
	            roam->bcn_timeout, roam->time_since_bcn, roam->bcns_lost);
#ifdef BCMDBG
	bcm_bprintf(b, "tbtt_since_bcn %u\n", roam->tbtt_since_bcn);
#endif // endif
}

static void
wlc_cxn_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_cxn_t *cxn;

	ASSERT(cfg != NULL);

	cxn = cfg->cxn;
	if (cxn == NULL)
		return;

	bcm_bprintf(b, "-------- connection states (%d) --------\n", WLC_BSSCFG_IDX(cfg));
	bcm_bprintf(b, "ign_bcn_lost_det 0x%x\n", cxn->ign_bcn_lost_det);
}

static int
_wlc_bsscfg_dump(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	char bssbuf[ETHER_ADDR_STR_LEN];
	char ifname[32];
	wsec_key_t *key;
	int i, j;
	vndr_ie_listel_t *vndrie;
	int bsscfg_idx = WLC_BSSCFG_IDX(cfg);
	bsscfg_module_t *bcmh = wlc->bcmh;
	char flagstr[64];
	const bcm_bit_desc_t bsscfg_flags[] = {
		{WLC_BSSCFG_PRESERVE, "PRESERVE"},
		{WLC_BSSCFG_WME_DISABLE, "WME_DIS"},
		{WLC_BSSCFG_PS_OFF_TRANS, "PSOFF_TRANS"},
		{WLC_BSSCFG_SW_BCN, "SW_BCN"},
		{WLC_BSSCFG_SW_PRB, "SW_PRB"},
		{WLC_BSSCFG_HW_BCN, "HW_BCN"},
		{WLC_BSSCFG_HW_PRB, "HW_PRB"},
		{WLC_BSSCFG_DPT, "DPT"},
		{WLC_BSSCFG_BTA, "BTA"},
		{WLC_BSSCFG_NOIF, "NOIF"},
		{WLC_BSSCFG_11N_DISABLE, "11N_DIS"},
		{WLC_BSSCFG_P2P, "P2P"},
		{WLC_BSSCFG_11H_DISABLE, "11H_DIS"},
		{WLC_BSSCFG_NATIVEIF, "NATIVEIF"},
		{WLC_BSSCFG_P2P_DISC, "P2P_DISC"},
		{WLC_BSSCFG_TDLS, "TDLS"},
		{0, NULL}
	};

	wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);

	strncpy(ifname, wl_ifname(wlc->wl, cfg->wlcif->wlif), sizeof(ifname));
	ifname[sizeof(ifname) - 1] = '\0';

	bcm_bprintf(b, "BSS Config %d (0x%p): \"%s\". BSSID: %s\n", bsscfg_idx, cfg, ssidbuf,
		bcm_ether_ntoa(&cfg->BSSID, bssbuf));

	bcm_bprintf(b, "_ap %d BSS %d enable %d. up %d. associated %d\n",
	            cfg->_ap, cfg->BSS, cfg->enable, cfg->up, cfg->associated, cfg->flags);
	bcm_format_flags(bsscfg_flags, cfg->flags, flagstr, sizeof(flagstr));
	bcm_bprintf(b, "flags: 0x%x [%s]\n", cfg->flags, flagstr);

	/* allmulti and multicast lists */
	bcm_bprintf(b, "allmulti %d\n", cfg->allmulti);
	bcm_bprintf(b, "nmulticast %d\n", cfg->nmulticast);
	if (cfg->nmulticast) {
		for (i = 0; i < (int)cfg->nmulticast; i++)
			bcm_bprintf(b, "%s ", bcm_ether_ntoa(&cfg->multicast[i], bssbuf));
		bcm_bprintf(b, "\n");
	}

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub)) {
		char chanbuf[CHANSPEC_STR_LEN];
		bcm_bprintf(b, "context: %p, ctxt_chanspec %s, wlcif->qi = %p\n",
		            cfg->chan_context,
		            (cfg->chan_context ?
		             wf_chspec_ntoa(cfg->chan_context->chanspec, chanbuf) :
		             "0"),
		            cfg->wlcif->qi);
		bcm_bprintf(b, "chanspec %s\n",
		            wf_chspec_ntoa(cfg->chanspec, chanbuf));
	}
#endif /* WLMCHAN */
#ifdef WLTDLS
	if (BSS_TDLS_ENAB(wlc, cfg)) {
		wlc_tdls_t *tc = cfg->tdls;

		bcm_bprintf(b, "up_time: %d, SA life time : %d\n",
			tc->up_time, tc->tpk_lifetime);
		bcm_bprintf(b, "TDLS bsscfg: initiator = %s, TDLS_PMEnable = %s, "
			"TDLS_PMAwake = %s\n", tc->initiator ? "TRUE" : "FALSE",
			tc->tdls_PMEnable ? "TRUE" : "FALSE",
			tc->tdls_PMAwake? "TRUE" : "FALSE");
		bcm_bprintf(b, "tdls_cap : 0x%02x\n", tc->tdls_cap);
#ifdef WL11N
		bcm_bprintf(b, "Supported Regulatory Classes: %d\n", tc->rclen);
		for (i = 0; i < tc->rclen; i++) {
			bcm_bprintf(b, " %d ", tc->rclist[i]);
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "HT capinfo: 0x%04x\n", tc->ht_capinfo);
#endif /* WL11N */
		bcm_bprintf(b, "\n");
	}
	else if (cfg == wlc->cfg) {
		bcm_bprintf(b, "TDLS parent: ts_allowed = %s\n",
			cfg->tdls->ps_allowed ? "TRUE" : "FALSE");
	}
#endif /* WLTDLS */

	bcm_bprintf(b, "cur_etheraddr %s\n", bcm_ether_ntoa(&cfg->cur_etheraddr, bssbuf));
	bcm_bprintf(b, "wlcif: flags 0x%x wlif 0x%p \"%s\" qi 0x%p\n",
	            cfg->wlcif->if_flags, cfg->wlcif->wlif, ifname, cfg->wlcif->qi);
	bcm_bprintf(b, "ap_isolate %d\n", cfg->ap_isolate);
	bcm_bprintf(b, "nobcnssid %d nobcprbresp %d\n",
		cfg->closednet_nobcnssid, cfg->closednet_nobcprbresp);
	bcm_bprintf(b, "wsec 0x%x auth %d wsec_index %d wep_algo %d\n",
		cfg->wsec, cfg->auth, cfg->wsec_index, WSEC_BSS_DEFAULT_KEY(cfg) ?
		WSEC_BSS_DEFAULT_KEY(cfg)->algo : 0);
	bcm_bprintf(b, "WPA_auth 0x%x wsec_restrict %d eap_restrict %d",
		cfg->WPA_auth, cfg->wsec_restrict, cfg->eap_restrict);
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "Extended Capabilities: ");
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_OBSS_COEX_MGMT))
		bcm_bprintf(b, "obss_coex ");
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_SPSMP))
		bcm_bprintf(b, "spsmp ");
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_PROXY_ARP))
		bcm_bprintf(b, "proxy_arp ");
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_BSSTRANS_MGMT))
		bcm_bprintf(b, "bsstrans ");
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_IW))
		bcm_bprintf(b, "inwk ");
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_SI))
		bcm_bprintf(b, "si ");
	if (isset(cfg->ext_cap, DOT11_EXT_CAP_OPER_MODE_NOTIF))
		bcm_bprintf(b, "oper_mode ");
	bcm_bprintf(b, "\n");

#ifdef BCMAUTH_PSK
	bcm_bprintf(b, " authenticator %p\n", cfg->authenticator);
#endif // endif
	bcm_bprintf(b, "tkip_countermeasures %d tk_cm_dt %d tk_cm_bt %d tk_cm_activate %d\n",
		cfg->tkip_countermeasures, cfg->tk_cm_dt, cfg->tk_cm_bt, cfg->tk_cm_activate);

	for (i = 0; i < WLC_DEFAULT_KEYS; i++) {
		key = cfg->bss_def_keys[i];
		if (key) {
			bcm_bprintf(b, "Key ID: %d%s %s\tidx %d len %2d data ",
				key->id, (i == cfg->wsec_index) ? "*" : " ",
			        bcm_crypto_algo_name(key->algo),
				key->idx, key->len);

			if (key->len)
				bcm_bprintf(b, "0x");

			for (j = 0; j < (int)key->len; j++)
				bcm_bprintf(b, "%02X", key->data[j]);

			for (j = 0; j < (int)key->len; j++)
				if (!bcm_isprint(key->data[j]))
					break;
			if (j == (int)key->len)
				bcm_bprintf(b, " (%.*s)", (int)key->len, key->data);
			bcm_bprintf(b, "\n");

		}
	}

	if (cfg->rcmta != NULL)
		bcm_bprintf(b, "BSSID rcmta: %d(%p)\n",
		            WSEC_KEY_INDEX(wlc, cfg->rcmta), cfg->rcmta);

#ifdef MBSS
	bcm_bprintf(b, "PS trans %u.\n", WLCNTVAL(cfg->cnt->ps_trans));
#if defined(WLC_SPT_DEBUG)
	bcm_bprintf(b, "BCN: bcn tx cnt %u. bcn suppressed %u\n",
		cfg->bcn_template->tx_count, cfg->bcn_template->suppressed);
#endif /* WLC_SPT_DEBUG */
	bcm_bprintf(b, "PrbResp: soft-prb-resp %s. directed req %d, alloc_fail %d, tx_fail %d\n",
		SOFTPRB_ENAB(cfg) ? "enabled" : "disabled",
		WLCNTVAL(cfg->cnt->prq_directed_entries), WLCNTVAL(cfg->cnt->prb_resp_alloc_fail),
		WLCNTVAL(cfg->cnt->prb_resp_tx_fail));
	bcm_bprintf(b, "PrbResp: TBTT suppressions %d. TTL expires %d. retrx fail %d.\n",
		WLCNTVAL(cfg->cnt->prb_resp_retrx), WLCNTVAL(cfg->cnt->prb_resp_ttl_expy),
		WLCNTVAL(cfg->cnt->prb_resp_retrx_fail));
	bcm_bprintf(b, "BCN: soft-bcn %s. bcn in use bmap 0x%x. bcn fail %u\n",
		SOFTBCN_ENAB(cfg) ? "enabled" : "disabled",
		cfg->bcn_template->in_use_bitmap, WLCNTVAL(cfg->cnt->bcn_tx_failed));
	bcm_bprintf(b, "BCN: HW MBSS %s. bcn in use bmap 0x%x. bcn fail %u\n",
		HWBCN_ENAB(cfg) ? "enabled" : "disabled",
		cfg->bcn_template->in_use_bitmap, WLCNTVAL(cfg->cnt->bcn_tx_failed));
	bcm_bprintf(b, "PRB: HW MBSS %s.\n",
		HWPRB_ENAB(cfg) ? "enabled" : "disabled");
	bcm_bprintf(b, "MC pkts in fifo %u. Max %u\n", cfg->mc_fifo_pkts,
		WLCNTVAL(cfg->cnt->mc_fifo_max));
	if (wlc->clk) {
		wlc_ssid_t ssid;
		uint16 shm_fid;

		shm_fid = wlc_read_shm((wlc), SHM_MBSS_WORD_OFFSET_TO_ADDR(5 + cfg->_ucidx));
		bcm_bprintf(b, "bcmc_fid 0x%x. bcmc_fid_shm 0x%x. shm last fid 0x%x. "
			"bcmc TX pkts %u\n", cfg->bcmc_fid, cfg->bcmc_fid_shm, shm_fid,
			WLCNTVAL(cfg->cnt->bcmc_count));
		wlc_shm_ssid_get(wlc, bsscfg_idx, &ssid);
		if (ssid.SSID_len > DOT11_MAX_SSID_LEN) {
			WL_ERROR(("Warning: Invalid MBSS ssid length %d for BSS %d\n",
				ssid.SSID_len, bsscfg_idx));
			ssid.SSID_len = DOT11_MAX_SSID_LEN;
		}
		wlc_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
		bcm_bprintf(b, "MBSS: ucode idx %d; shm ssid >%s< of len %d\n",
			WLC_BSSCFG_UCIDX(cfg), ssidbuf, ssid.SSID_len);
	} else {
		bcm_bprintf(b, "Core clock disabled, not dumping SHM info\n");
	}
#endif /* MBSS */

	bcm_bprintf(b, "rssi %d snr %d\n", cfg->link->rssi, cfg->link->snr);

#ifdef WL_BSSCFG_TX_SUPR
	if (cfg->psq != NULL) {
		bcm_bprintf(b, "%s, length %d\n",
		            BSS_TX_SUPR(cfg)? "suppressed":"not suppressed",
		            pktq_len(cfg->psq));
	}
#endif // endif

	/* vendor IEs */
	for (vndrie = cfg->vndr_ie_listp; vndrie != NULL; vndrie = vndrie->next_el) {
		bcm_tlv_t *ie = (bcm_tlv_t *)&vndrie->vndr_ie_infoel.vndr_ie_data;

		bcm_bprintf(b, "flags: %08x ", vndrie->vndr_ie_infoel.pktflag);
		wlc_dump_ie(wlc, ie, b);
		bcm_bprintf(b, "\n");
	}

	wlc_assoc_bss_dump(wlc, cfg, b);
	wlc_roam_bss_dump(wlc, cfg, b);
	wlc_cxn_bss_dump(wlc, cfg, b);

#ifdef DWDS
	/* Display DWDS SA list */
	if (BSSCFG_STA(cfg) && MAP_ENAB(cfg) && cfg->dwds_loopback_filter) {
		wlc_dwds_dump_sa_list(wlc, cfg, b);
	}
#endif /* DWDS */

	/* invoke bsscfg cubby dump function */
	bcm_bprintf(b, "ncubby: %d\n", bcmh->ncubby);
	for (i = 0; i < (int)bcmh->ncubby; i++) {
		bsscfg_cubby_info_t *cubby_info = &bcmh->cubby_info[i];
		bcm_bprintf(b, "  cubby %d: init %p deinit %p dump %p\n", i,
		            cubby_info->fn_init, cubby_info->fn_deinit, cubby_info->fn_dump);
		if (cubby_info->fn_dump != NULL) {
			(cubby_info->fn_dump)(cubby_info->ctx, cfg, b);
		}
	}

	/* display bsscfg up/down function pointers */
	bcm_notif_dump_list(bcmh->up_down_notif_hdl, b);

	return 0;
}

static int
wlc_bsscfg_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int i;
	wlc_bsscfg_t *bsscfg;

#ifdef MBSS
	bcm_bprintf(b, "MBSS Build.  MBSS is %s. SW MBSS MHF band 0: %s; band 1: %s\n",
		MBSS_ENAB(wlc->pub) ? "enabled" : "disabled",
		(wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_2G) & MHF1_MBSS_EN) ? "set" : "clear",
		(wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_5G) & MHF1_MBSS_EN) ? "set" : "clear");
	bcm_bprintf(b, "Pkts suppressed from ATIM:  %d. Bcn Tmpl not ready/done %d/%d\n",
		WLCNTVAL(wlc->pub->_cnt->atim_suppress_count),
		WLCNTVAL(wlc->pub->_cnt->bcn_template_not_ready),
		WLCNTVAL(wlc->pub->_cnt->bcn_template_not_ready_done));
#if defined(WLC_HIGH) && defined(WLC_LOW)
	bcm_bprintf(b, "WLC: cached prq base 0x%x, current prq rd 0x%x\n", wlc->prq_base,
		wlc->prq_rd_ptr);
#endif /* WLC_HIGH && WLC_LOW */
	bcm_bprintf(b, "Late TBTT counter %d\n",
		WLCNTVAL(wlc->pub->_cnt->late_tbtt_dpc));
	if (BSSCFG_EXTRA_VERBOSE && wlc->clk) {
		bcm_bprintf(b, "MBSS shared memory offsets and values:\n");
		SHOW_SHM(wlc, b, SHM_MBSS_BSSID0, "BSSID0");
		SHOW_SHM(wlc, b, SHM_MBSS_BSSID1, "BSSID1");
		SHOW_SHM(wlc, b, SHM_MBSS_BSSID2, "BSSID2");
		SHOW_SHM(wlc, b, SHM_MBSS_BCN_COUNT, "BCN_COUNT");
		SHOW_SHM(wlc, b, SHM_MBSS_PRQ_BASE, "PRQ_BASE");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID0, "BC_FID0");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID1, "BC_FID1");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID2, "BC_FID2");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID3, "BC_FID3");
		SHOW_SHM(wlc, b, SHM_MBSS_PRE_TBTT, "PRE_TBTT");
		SHOW_SHM(wlc, b, SHM_MBSS_SSID_LEN0, "SSID_LEN0");
		SHOW_SHM(wlc, b, SHM_MBSS_SSID_LEN1, "SSID_LEN1");
		SHOW_SHM(wlc, b, SHM_MBSS_PRQ_READ_PTR, "PRQ_RD");
		SHOW_SHM(wlc, b, SHM_MBSS_PRQ_WRITE_PTR, "PRQ_WR");
		SHOW_SHM(wlc, b, M_HOST_FLAGS1, "M_HOST1");
		SHOW_SHM(wlc, b, M_HOST_FLAGS2, "M_HOST2");
	}
	/* Dump out data at current PRQ ptrs */
	bcm_bprintf(b, "PRQ entries handled %d. Undirected %d. Bad %d\n",
		WLCNTVAL(wlc->pub->_cnt->prq_entries_handled),
		WLCNTVAL(wlc->pub->_cnt->prq_undirected_entries),
		WLCNTVAL(wlc->pub->_cnt->prq_bad_entries));

	if (BSSCFG_EXTRA_VERBOSE && wlc->clk) {
		uint16 rdptr, wrptr, base, totbytes, offset;
		int j;
		shm_mbss_prq_entry_t entry;
		char ea_buf[ETHER_ADDR_STR_LEN];

		base = wlc_read_shm(wlc, SHM_MBSS_PRQ_BASE);
		rdptr = wlc_read_shm(wlc, SHM_MBSS_PRQ_READ_PTR);
		wrptr = wlc_read_shm(wlc, SHM_MBSS_PRQ_WRITE_PTR);
		totbytes = SHM_MBSS_PRQ_ENTRY_BYTES * SHM_MBSS_PRQ_ENTRY_COUNT;
		if (rdptr < base || (rdptr >= base + totbytes)) {
			bcm_bprintf(b, "WARNING: PRQ read pointer out of range\n");
		}
		if (wrptr < base || (wrptr >= base + totbytes)) {
			bcm_bprintf(b, "WARNING: PRQ write pointer out of range\n");
		}

		bcm_bprintf(b, "PRQ data at %8s %25s\n", "TA", "PLCP0  Time");
		for (offset = base * 2, j = 0; j < SHM_MBSS_PRQ_ENTRY_COUNT;
			j++, offset += SHM_MBSS_PRQ_ENTRY_BYTES) {
			wlc_copyfrom_shm(wlc, offset, &entry, sizeof(entry));
			bcm_bprintf(b, "  0x%04x:", offset);
			bcm_bprintf(b, "  %s ", bcm_ether_ntoa(&entry.ta, ea_buf));
			bcm_bprintf(b, " 0x%0x 0x%02x 0x%04x", entry.prq_info[0],
				entry.prq_info[1], entry.time_stamp);
			if (SHM_MBSS_PRQ_ENT_DIR_SSID(&entry) ||
				SHM_MBSS_PRQ_ENT_DIR_BSSID(&entry)) {
				int uc, sw;

				uc = SHM_MBSS_PRQ_ENT_UC_BSS_IDX(&entry);
				sw = WLC_BSSCFG_HW2SW_IDX(wlc, uc);
				bcm_bprintf(b, "  (bss uc %d/sw %d)", uc, sw);
			}
			bcm_bprintf(b, "\n");
		}
	}
#endif /* MBSS */

	FOREACH_BSS(wlc, i, bsscfg) {
		_wlc_bsscfg_dump(wlc, bsscfg, b);
		bcm_bprintf(b, "\n");
	}

	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */

static bool
wlc_bsscfg_preserve(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bool preserve = FALSE;

	ASSERT(cfg);
#if defined(STA) && defined(WLOFFLD)
	/* return true if Offloads is enable and STA is associated */
	if (WLOFFLD_CAP(wlc) && WLOFFLD_ENAB(wlc->pub)) {
		if (BSSCFG_STA(cfg) && cfg->associated)
			preserve = TRUE;
	}
#endif /* STA && WLOFFLD */

#ifdef WOWL
	/* return false if WoWL is not enable or active */
	if (!WOWL_ENAB(wlc->pub) || !WOWL_ACTIVE(wlc->pub))
		preserve = FALSE;
#endif /* WOWL */
	return preserve;
}

/* This function checks to see if the interface is up.
* It computes the bsscfg from the connID and checks if the
* interface is up.
*/
bool
wlc_bsscfg_is_intf_up(wlc_info_t *wlc, uint16 connID)
{
	wlc_bsscfg_t *cfg;

	if (wlc->pub->up == FALSE)
		return FALSE;

	cfg = wlc_bsscfg_find_by_ID(wlc, connID);

	if (cfg == NULL || cfg->up == FALSE)
		return FALSE;

	return TRUE;
}

#ifdef AP
uint32
wlc_bsscfg_getdtim_count(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint32 dtim_cnt;
#ifdef WLMCNX
	if (P2P_ENAB(wlc->pub)) {
		int idx;
		idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
		dtim_cnt = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_DTIM_CNT(idx));
	}
	else
#endif // endif
	{
		wlc_bmac_copyfrom_objmem(wlc->hw, (S_DOT11_DTIMCOUNT << 2),
			&dtim_cnt, sizeof(dtim_cnt), OBJADDR_SCR_SEL);
	}
	return dtim_cnt;
}
#endif /* AP */

#ifndef NO_ROAMOFFL_SUPPORT
bool
wlc_bsscfg_chk_roamoffl_bssid(wlc_bsscfg_t *cfg, struct ether_addr *addr)
{
	int i;

	if (!ROAMOFFL_ENAB(cfg))
		return FALSE;

	if (!cfg->roamoffl_bssid_list || cfg->roamoffl_bssid_list->cnt == 0) {
		if (cfg->roamoffl == ROAMOFFL_ON_BSSID_LIST)
			return FALSE;
		return TRUE;
	}

	for (i = 0; i < cfg->roamoffl_bssid_list->cnt; i++) {
		if (eacmp(&(cfg->roamoffl_bssid_list->bssid[i]), addr) == 0)
			return TRUE;
	}

	return FALSE;
}
#endif /* !NO_ROAMOFFL_SUPPORT */

#ifdef BCMDBG_TXSTUCK
void
wlc_bsscfg_print_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b)
{
#ifdef WL_BSSCFG_TX_SUPR
	int i;
	wlc_bsscfg_t *cfg;
	char eabuf[ETHER_ADDR_STR_LEN];

	FOREACH_BSS(wlc, i, cfg) {
		if (cfg->psq != NULL) {
			bcm_bprintf(b, "%s: %s, length %d\n",
				bcm_ether_ntoa(&cfg->BSSID, eabuf),
				BSS_TX_SUPR(cfg)? "suppressed":"not suppressed",
				pktq_len(cfg->psq));
		} else {
			bcm_bprintf(b, "%s, no psq\n", bcm_ether_ntoa(&cfg->BSSID, eabuf));
		}
	}
#endif // endif
}
#endif /* BCMDBG_TXSTUCK */

#ifdef WL_GLOBAL_RCLASS
bool
wlc_cur_opclass_global(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg)
{
	uint8 cur_rclass = wlc_channel_get_cur_rclass(wlc);
	if (cur_rclass == BCMWIFI_RCLASS_TYPE_GBL) {
		return TRUE;
	} else {
		return FALSE;
	}
}

void
wlc_bsscfg_update_rclass(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg)
{
	uint8 cur_rclass;

	cur_rclass = wlc_channel_get_cur_rclass(wlc);
	WL_INFORM(("wl%d: scb_without_gbl_rclass[%d], cur_rclass[%d] \n",
		wlc->pub->unit, bsscfg->scb_without_gbl_rclass, cur_rclass));
	/* All sta in bsscfg understand global operating class, switch to global
	 * operating class, else maintain Country specific Operating class
	 * operation
	 */
	if ((!bsscfg->scb_without_gbl_rclass) && (cur_rclass != BCMWIFI_RCLASS_TYPE_GBL)) {
		wlc_channel_set_cur_rclass(wlc, BCMWIFI_RCLASS_TYPE_GBL);
		WL_TRACE(("wl%d: change from Country to Global operating class\n",
			wlc->pub->unit));
		wlc_update_rcinfo(wlc->cmi, TRUE);
		return;
	}

	if ((bsscfg->scb_without_gbl_rclass) && (cur_rclass != BCMWIFI_RCLASS_TYPE_COUNTRY)) {
		wlc_channel_set_cur_rclass(wlc, BCMWIFI_RCLASS_TYPE_COUNTRY);
		WL_TRACE(("wl%d: change from Global to country specifc operating class\n",
			wlc->pub->unit));
		wlc_update_rcinfo(wlc->cmi, FALSE);
		return;
	}
}
#endif /* WL_GLOBAL_RCLASS */
