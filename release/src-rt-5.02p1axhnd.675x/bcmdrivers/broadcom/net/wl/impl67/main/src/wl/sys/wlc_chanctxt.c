/*
 * Common (OS-independent) portion of
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_chanctxt.c 786546 2020-04-30 06:09:59Z $
 */

/* Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <802.11.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc_bsscfg_psq.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_assoc.h>
#include <wlc_tx.h>
#include <wlc_scb.h>
#ifdef PROP_TXSTATUS
#include <wlc_ampdu.h>
#include <wlc_apps.h>
#include <wlc_wlfc.h>
#endif /* PROP_TXSTATUS */
#ifdef WLMCHAN
#include <wlc_mchan.h>
#include <wlc_p2p.h>
#endif /* WLMCHAN */
#include <wlc_lq.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif // endif
#include <wlc_msch.h>
#include <wlc_chanctxt.h>
#include <wlc_ap.h>
#include <wlc_dfs.h>
#include <wlc_pm.h>
#include <wlc_srvsdb.h>
#include <wlc_sta.h>
#include <phy_chanmgr_api.h>
#include <phy_calmgr_api.h>
#include <phy_cache_api.h>
#ifdef WDS
#include <wlc_wds.h>
#endif /* WDS */

#define SCHED_MAX_TIME_GAP	200
#define SCHED_TIMER_DELAY	100

/* bss chan context */
struct wlc_chanctxt {
	struct wlc_chanctxt *next;
	wlc_txq_info_t *qi;		/* tx queue */
	chanspec_t	chanspec;	/* channel specific */
	uint16		count;		/* count for txq attached */
	uint16	fragthresh[AC_COUNT];	/* context based fragthreshold */
	uint8	usage_countdown;	/* keeping track of cleaup if stale */
	bool	in_passive_use;		/* retain context while creating new one */
	bool	piggyback;		/* share the channel */
};

struct wlc_chanctxt_info {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	wlc_chanctxt_t *chanctxt_list;	/* chan context link list */
	wlc_chanctxt_t *curr_chanctxt;	/* current chan context */
	int cfgh;			/* bsscfg cubby handle */
	wlc_chanctxt_t *piggyback_chanctxt;	/* piggyback chan context */
	chanspec_t excursion_chanspec;	/* excursion queue request channel */
	uint16 excursion_count;		/* count for excursion queue attached */
};

/* per BSS data */
typedef struct {
	wlc_chanctxt_t	*chanctxt;	/* current chan context for this bss */
	bool		on_channel;	/* whether the bss is on channel */
	bool		multi_channel;	/* support multi channels */
	uint16		num_channel;	/* how many channel sequence */
	wlc_chanctxt_t	**mchanctxt;	/* chan context list for multi chan seq */
} chanctxt_bss_info_t;

/* locate per BSS data */
#define CHANCTXT_BSS_CUBBY_LOC(chanctxt_info, cfg) \
	((chanctxt_bss_info_t **)BSSCFG_CUBBY((cfg), (chanctxt_info)->cfgh))
#define CHANCTXT_BSS_INFO(chanctxt_info, cfg) \
	(*CHANCTXT_BSS_CUBBY_LOC(chanctxt_info, cfg))

/* local prototypes */
static void wlc_chanctxt_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *evt);
static int wlc_chanctxt_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_chanctxt_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
static bool wlc_chanctxt_in_use(wlc_info_t *wlc, wlc_chanctxt_t *chanctxt,
	wlc_bsscfg_t *filter_cfg);
static bool wlc_tx_qi_exist(wlc_info_t *wlc, wlc_txq_info_t *qi);
static wlc_txq_info_t *wlc_get_qi_with_no_context(wlc_info_t *wlc);
static void wlc_chanctxt_set_channel(wlc_chanctxt_info_t *chanctxt_info,
	wlc_chanctxt_t *chanctxt, chanspec_t chanspec, bool destroy_old);
static wlc_chanctxt_t *wlc_chanctxt_alloc(wlc_chanctxt_info_t *chanctxt_info,
	chanspec_t chanspec);
static void wlc_chanctxt_free(wlc_chanctxt_info_t *chanctxt_info, wlc_chanctxt_t *chanctxt);
static wlc_chanctxt_t *wlc_get_chanctxt(wlc_chanctxt_info_t *chanctxt_info,
	chanspec_t chanspec);
static bool wlc_all_cfg_chanspec_is_same(wlc_info_t *wlc,
	wlc_chanctxt_t *chanctxt, chanspec_t chanspec);
static void wlc_chanctxt_delete_bsscfg_ctxt(wlc_chanctxt_info_t *chanctxt_info,
	wlc_bsscfg_t *bsscfg);
static wlc_chanctxt_t *wlc_chanctxt_find_bsscfg_ctxt(wlc_chanctxt_info_t *chanctxt_info,
	wlc_bsscfg_t *bsscfg, chanspec_t chanspec);
static wlc_chanctxt_t *wlc_chanctxt_get_start_ctxt(wlc_chanctxt_info_t *chanctxt_info,
	wlc_bsscfg_t *bsscfg, chanspec_t chanspec);
static wlc_chanctxt_t *wlc_chanctxt_get_end_ctxt(wlc_chanctxt_info_t *chanctxt_info,
	wlc_bsscfg_t *bsscfg);
static void wlc_chanctxt_start_queue(wlc_info_t *wlc, wlc_chanctxt_t *chanctxt);
static void wlc_chanctxt_switch_queue(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_chanctxt_t *chanctxt,
	uint oldqi_stopped_flag);
static wlc_chanctxt_t *wlc_chanctxt_create_txqueue(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	chanspec_t chanspec);
static int wlc_chanctxt_delete_txqueue(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_chanctxt_t *chanctxt, uint *oldqi_info);

static int wlc_chanctxt_down(void *context);
static void wlc_chanctxt_watchdog(void *context);
static void wlc_chanctxt_delete_unused_ctxt(wlc_chanctxt_info_t *chanctxt_info,
	wlc_chanctxt_t *chanctxt);

#ifdef SRHWVSDB
static void wlc_chanctxt_srvsdb_upd(wlc_chanctxt_info_t *chanctxt_info);
#endif // endif

#ifdef PHYCAL_CACHING
static int wlc_chanctxt_is_deleted_calcache(wlc_info_t *wlc, chanspec_t chanspec);
static void wlc_chanctxt_chk_calcache(wlc_info_t *wlc);
static int wlc_chanctxt_chk_calcache_cb(void *context);
#endif // endif

/* debugging... */
#define WL_CHANCTXT_TRACE(args)		WL_TRACE(args)
#define WL_CHANCTXT_INFO(args)		WL_INFORM(args)
#define WL_CHANCTXT_WARN(args)		WL_INFORM(args)
#define WL_CHANCTXT_ERROR(args)		WL_ERROR(args)
#define WL_CHANCTXT_ASSERT(exp, args)	ASSERT(exp)

wlc_chanctxt_info_t *
BCMATTACHFN(wlc_chanctxt_attach)(wlc_info_t *wlc)
{
	wlc_chanctxt_info_t *chanctxt_info;

	/* module states */
	chanctxt_info = (wlc_chanctxt_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_chanctxt_info_t));
	if (!chanctxt_info) {
		WL_ERROR(("wl%d: wlc_chanctxt_attach: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	chanctxt_info->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, NULL, "chanctxt", chanctxt_info, NULL,
		wlc_chanctxt_watchdog, NULL, wlc_chanctxt_down)) {
		WL_ERROR(("wl%d: wlc_chanctxt_attach: wlc_module_register() failed\n",
		          wlc->pub->unit));
		goto fail;
	}

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((chanctxt_info->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(chanctxt_bss_info_t *),
		wlc_chanctxt_bss_init, wlc_chanctxt_bss_deinit, NULL, chanctxt_info)) < 0) {
		WL_ERROR(("wl%d: wlc_chanctxt_attach: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit));
		goto fail;
	}

	if (wlc_bsscfg_state_upd_register(wlc, wlc_chanctxt_bsscfg_state_upd, chanctxt_info)
		!= BCME_OK) {
		WL_ERROR(("wl%d: wlc_chanctxt_attach: wlc_bsscfg_state_upd_register() failed\n",
		          wlc->pub->unit));
		goto fail;
	}

#ifdef PHYCAL_CACHING
	phy_cache_register_cb(WLC_PI(wlc), wlc_chanctxt_chk_calcache_cb, (void *)wlc);
#endif /* PHYCAL_CACHING */

	return chanctxt_info;

fail:
	/* error handling */
	MODULE_DETACH(chanctxt_info, wlc_chanctxt_detach);

	return NULL;
}

void
BCMATTACHFN(wlc_chanctxt_detach)(wlc_chanctxt_info_t *chanctxt_info)
{
	wlc_info_t *wlc;
	wlc_chanctxt_t *chanctxt_entity;

	ASSERT(chanctxt_info);
	wlc = chanctxt_info->wlc;

	/* delete all chan contexts */
	while ((chanctxt_entity = chanctxt_info->chanctxt_list)) {
		wlc_chanctxt_free(chanctxt_info, chanctxt_entity);
	}

	(void)wlc_bsscfg_state_upd_unregister(wlc, wlc_chanctxt_bsscfg_state_upd, chanctxt_info);

	/* unregister module */
	wlc_module_unregister(wlc->pub, "chanctxt", chanctxt_info);

	MFREE(wlc->osh, chanctxt_info, sizeof(wlc_chanctxt_info_t));
}

static int
wlc_chanctxt_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_chanctxt_info_t *chanctxt_info = (wlc_chanctxt_info_t *)ctx;
	wlc_info_t *wlc = chanctxt_info->wlc;
	chanctxt_bss_info_t **cbi_loc;
	chanctxt_bss_info_t *cbi;

	/* allocate cubby info */
	if ((cbi = MALLOCZ(wlc->osh, sizeof(chanctxt_bss_info_t))) == NULL) {
		WL_ERROR(("wl%d: wlc_chanctxt_bss_init: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	cbi_loc = CHANCTXT_BSS_CUBBY_LOC(chanctxt_info, cfg);
	*cbi_loc = cbi;

	return BCME_OK;
}

static void
wlc_chanctxt_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_chanctxt_info_t *chanctxt_info = (wlc_chanctxt_info_t *)ctx;
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, cfg);
	chanctxt_bss_info_t **cbi_loc;

	if (cbi != NULL) {
		wlc_chanctxt_delete_bsscfg_ctxt(chanctxt_info, cfg);
		if (cbi->multi_channel) {
			MFREE(chanctxt_info->wlc->osh, cbi->mchanctxt, cbi->num_channel *
				sizeof(wlc_chanctxt_t *));
		}

		MFREE(chanctxt_info->wlc->osh, cbi, sizeof(chanctxt_bss_info_t));
		cbi_loc = CHANCTXT_BSS_CUBBY_LOC(chanctxt_info, cfg);
		*cbi_loc = NULL;
	}
}

static void
wlc_chanctxt_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *evt)
{
	wlc_bsscfg_t *cfg = evt->cfg;
	ASSERT(cfg != NULL);

	if (evt->old_enable && !cfg->enable) {
		wlc_chanctxt_info_t *chanctxt_info = (wlc_chanctxt_info_t *)ctx;
		chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, cfg);

		ASSERT(cbi);

		wlc_chanctxt_delete_bsscfg_ctxt(chanctxt_info, cfg);
		cbi->chanctxt = NULL;
	}
}

static int
wlc_chanctxt_down(void * context)
{
	wlc_chanctxt_info_t *chanctxt_info = (wlc_chanctxt_info_t *)context;
	wlc_info_t *wlc = chanctxt_info->wlc;
	wlc_chanctxt_t *ctxt_iter = chanctxt_info->chanctxt_list;

	while (ctxt_iter) {
		/* immediately free up, if no one is using this chanctxt */
		if (!wlc_chanctxt_in_use(wlc, ctxt_iter, NULL))  {
			wlc_chanctxt_t *chanctxt_iter_save = ctxt_iter->next;
			wlc_chanctxt_delete_unused_ctxt(chanctxt_info, ctxt_iter);
			ctxt_iter = chanctxt_iter_save;
			continue;
		}
		ctxt_iter = ctxt_iter->next;
	}
	return 0;
}

static void
wlc_chanctxt_watchdog(void * context)
{
	wlc_chanctxt_info_t *chanctxt_info = (wlc_chanctxt_info_t *)context;
	wlc_info_t *wlc = chanctxt_info->wlc;
	wlc_chanctxt_t *ctxt_iter = chanctxt_info->chanctxt_list;

	while (ctxt_iter) {
		/* first check if no cfg is using this context */
		if (!wlc_chanctxt_in_use(wlc, ctxt_iter, NULL))  {
			/*
			* next check if countdown expired or not
			* if not decrement the value and defer cleanup till 0
			*/
			if (--ctxt_iter->usage_countdown > 0) {
				goto next;
			}
			else {
				/* usage timed out. Clean this stale context */
				wlc_chanctxt_t *chanctxt_iter_save = ctxt_iter->next;
				wlc_chanctxt_delete_unused_ctxt(chanctxt_info, ctxt_iter);
				ctxt_iter = chanctxt_iter_save;
				continue;
			}
		}
next:
		ctxt_iter = ctxt_iter->next;
	}
}

static void
wlc_chanctxt_delete_unused_ctxt(wlc_chanctxt_info_t *chanctxt_info,
	wlc_chanctxt_t *chanctxt)
{
	wlc_lq_chanim_delete_bss_chan_context(chanctxt_info->wlc, chanctxt->chanspec);
	if (chanctxt == chanctxt_info->curr_chanctxt) {
		/* If chanctxt is curr_chanctxt, make the
		 * curr_chanctxt pointer in chanctxt_info to NULL.
		 */
		chanctxt_info->curr_chanctxt = NULL;
	}
	wlc_chanctxt_free(chanctxt_info, chanctxt);
}

static void
wlc_chanctxt_delete_bsscfg_ctxt(wlc_chanctxt_info_t *chanctxt_info,
	wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = chanctxt_info->wlc;
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, bsscfg);

	if (cbi != NULL) {
		if (cbi->multi_channel) {
			int i;
			for (i = 0; i < cbi->num_channel; i++) {
				if (cbi->mchanctxt[i]) {
					wlc_chanctxt_delete_txqueue(wlc, bsscfg,
						cbi->mchanctxt[i], NULL);
				}
			}
			MFREE(wlc->osh, cbi->mchanctxt, cbi->num_channel *
				sizeof(wlc_chanctxt_t *));
			cbi->multi_channel = FALSE;
			cbi->num_channel = 0;
		} else if (cbi->chanctxt) {
			wlc_chanctxt_delete_txqueue(wlc, bsscfg, cbi->chanctxt, NULL);
		}
		cbi->chanctxt = NULL;
	}
}

static wlc_chanctxt_t *
wlc_chanctxt_find_bsscfg_ctxt(wlc_chanctxt_info_t *chanctxt_info, wlc_bsscfg_t *bsscfg,
	chanspec_t chanspec)
{
	wlc_info_t *wlc = chanctxt_info->wlc;
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, bsscfg);
	wlc_chanctxt_t *chanctxt = NULL;

	if (cbi != NULL) {
		int i, idx = -1;
		if (cbi->multi_channel) {
			for (i = 0; i < cbi->num_channel; i++) {
				chanctxt = cbi->mchanctxt[i];
				if (chanctxt) {
					if (WLC_CHAN_COEXIST(chanctxt->chanspec,
						chanspec)) {
						idx = i;
						break;
					}
				} else if (idx < 0) {
					idx = i;
				}
			}
			WL_CHANCTXT_INFO(("wl%d.%d: wlc_chanctxt_find_bsscfg_ctxt: "
				"chanctxt[%d] = %p\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), idx,
				chanctxt));
			ASSERT((idx < cbi->num_channel) && (idx != -1));
		}

		/* chanctxt is now the previous channel context held by this cfg */
		chanctxt = cbi->chanctxt;

		if (chanctxt == NULL || ((chanctxt->chanspec != chanspec) &&
			(!WLC_CHAN_COEXIST(chanctxt->chanspec, chanspec) ||
			CHSPEC_BW_GT(chanspec, CHSPEC_BW(chanctxt->chanspec))))) {
			/*
			* for multi_channel case, create_txqueue below will not allocate
			* new channel context, rather it will re-use the existing chan_ctxt
			* for this cfg and also invoke tx_flow_control. For other cases, it will
			* create a new channel context should one not exist before
			*/
			chanctxt = wlc_chanctxt_create_txqueue(wlc, bsscfg, chanspec);
			if (cbi->multi_channel) {
				cbi->mchanctxt[idx] = chanctxt;
			}
		}
	}

	return chanctxt;
}

static wlc_chanctxt_t *
wlc_chanctxt_get_start_ctxt(wlc_chanctxt_info_t *chanctxt_info, wlc_bsscfg_t *bsscfg,
	chanspec_t chanspec)
{
	wlc_info_t *wlc = chanctxt_info->wlc;
	wlc_chanctxt_t *chanctxt = NULL;

	if (bsscfg != NULL) {
		chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, bsscfg);
		ASSERT(cbi);

		chanctxt = wlc_chanctxt_find_bsscfg_ctxt(chanctxt_info, bsscfg, chanspec);
		ASSERT(chanctxt);

		if (cbi->on_channel) {
			WL_CHANCTXT_WARN(("[WARNING]wl%d.%d: %s: "
				"cfg is already on channel, count <%d, %d>\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
				chanctxt->count, chanctxt_info->excursion_count));
			return NULL;
		}

		cbi->on_channel = TRUE;
	} else {
		if (chanctxt_info->excursion_count == 0) {
			chanctxt_info->excursion_chanspec = chanspec;
		} else {
			WL_CHANCTXT_ASSERT(chanctxt_info->excursion_chanspec != 0 &&
				WLC_CHAN_COEXIST(chanctxt_info->excursion_chanspec,
				chanspec), ("[ASSERT]wl%d: %s: excursion queue "
				"chanspec 0x%04x is not equal to current chanspec 0x%04x\n",
				wlc->pub->unit, __FUNCTION__, chanctxt_info->excursion_chanspec,
				chanspec));
		}

		chanctxt_info->excursion_count++;

		chanctxt = wlc_get_chanctxt(chanctxt_info, chanspec);
		if (chanctxt == NULL) {
			if (chanctxt_info->excursion_count == 1) {
				wlc_excursion_start(wlc);
			} else {
				WL_CHANCTXT_ASSERT(wlc->excursion_active,
					("[ASSERT]wl%d: %s: excursion queue "
					"is not active yet, count[%d]\n", wlc->pub->unit,
					__FUNCTION__, chanctxt_info->excursion_count));
			}
			WL_CHANCTXT_INFO(("wl%d: %s: using excursion queue: "
				"active %d, count %d\n", wlc->pub->unit, __FUNCTION__,
				wlc->excursion_active, chanctxt_info->excursion_count));
			return NULL;
		}

		chanctxt->piggyback = TRUE;
		chanctxt_info->piggyback_chanctxt = chanctxt;
	}

	return chanctxt;
}

static wlc_chanctxt_t *
wlc_chanctxt_get_end_ctxt(wlc_chanctxt_info_t *chanctxt_info, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = chanctxt_info->wlc;
	wlc_chanctxt_t *chanctxt = NULL;

	if (bsscfg != NULL) {
		chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, bsscfg);
		ASSERT(cbi);

		chanctxt = cbi->chanctxt;
		if (!cbi->on_channel || !chanctxt) {
			WL_CHANCTXT_WARN(("[WARNING]wl%d.%d: %s: cfg is already off channel, "
				"count <%d, %d>\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				__FUNCTION__, chanctxt? chanctxt->count : -100,
				chanctxt_info->excursion_count));
			return NULL;
		}
		cbi->on_channel = FALSE;

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			wlc_wlfc_mchan_interface_state_update(wlc, bsscfg,
				WLFC_CTL_TYPE_INTERFACE_CLOSE, FALSE);
		}
#endif /* PROP_TXSTATUS */
	} else {
		if (chanctxt_info->excursion_count == 0) {
			WL_CHANCTXT_ASSERT(chanctxt_info->excursion_chanspec == 0 &&
				!chanctxt_info->piggyback_chanctxt,
				("[ASSERT]wl%d: %s: excursion chanspec is not "
				"cleared yet\n", wlc->pub->unit, __FUNCTION__));
			WL_CHANCTXT_WARN(("[WARNING]wl%d: %s: "
				"excursion is already off channel, active %d, count %d\n",
				wlc->pub->unit, __FUNCTION__, wlc->excursion_active,
				chanctxt_info->excursion_count));
			return NULL;
		}

		WL_CHANCTXT_ASSERT(chanctxt_info->excursion_count > 0,
			("[ASSERT]wl%d: %s: excursion count [%d]\n",
			wlc->pub->unit, __FUNCTION__, chanctxt_info->excursion_count));
		chanctxt_info->excursion_count--;

		chanctxt = chanctxt_info->piggyback_chanctxt;
		if (chanctxt == NULL) {
			if (chanctxt_info->excursion_count == 0) {
				chanctxt_info->excursion_chanspec = 0;
				wlc_excursion_end(wlc);
			} else {
				WL_CHANCTXT_ASSERT(wlc->excursion_active,
					("[ASSERT]wl%d: %s: excursion queue has been inactive, "
					"count[%d]\n", wlc->pub->unit, __FUNCTION__,
					chanctxt_info->excursion_count));
			}
			WL_CHANCTXT_INFO(("wl%d: %s: ending excursion queue: "
				"active %d, count %d\n", wlc->pub->unit, __FUNCTION__,
				wlc->excursion_active, chanctxt_info->excursion_count));
			return NULL;
		} else {
			/*
			* if piggyback is ON, it must be using
			* current chanctxt's chanspec
			*/
			WL_CHANCTXT_ASSERT(WLC_CHAN_COEXIST(chanctxt_info->excursion_chanspec,
				chanctxt_info->curr_chanctxt->chanspec),
				("[ASSERT]wl%d: %s: excursion queue "
				"chanspec 0x%04x is not equal to current chanspec 0x%04x\n",
				wlc->pub->unit, __FUNCTION__, chanctxt_info->excursion_chanspec,
				chanctxt_info->curr_chanctxt->chanspec));
		}

		if (chanctxt_info->excursion_count == 0) {
			chanctxt->piggyback = FALSE;
			chanctxt_info->excursion_chanspec = 0;
			chanctxt_info->piggyback_chanctxt = NULL;
		}
	}

	return chanctxt;
}

void
wlc_txqueue_start(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chanspec)
{
	wlc_chanctxt_info_t *chanctxt_info = wlc->chanctxt_info;
	wlc_chanctxt_t *chanctxt;
	wlc_txq_info_t *qi;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	WL_CHANCTXT_TRACE(("[ENTRY]wl%d.%d: %s: channel 0x%04x, call by %p\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		chanspec, CALL_SITE));

	if (WLCNTVAL(wlc->pub->_cnt->txqueue_end) > 0) {
		WL_ERROR(("wl%d.%d txqueue_end was in progress\n",
			wlc->pub->unit, cfg ? WLC_BSSCFG_IDX(cfg) : -1));
		OSL_SYS_HALT();
	}
	else {
		WLCNTINCR(wlc->pub->_cnt->txqueue_start);
	}

	WL_MQ(("wl%d.%d: %s: channel %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		wf_chspec_ntoa(chanspec, chanbuf)));

	if ((chanctxt = wlc_chanctxt_get_start_ctxt(chanctxt_info, cfg, chanspec)) == NULL) {
		goto done;
	}

	qi = chanctxt->qi;

	WL_MQ(("wl%d.%d: %s: qi %p, primary %p, active %p\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		OSL_OBFUSCATE_BUF(qi), OSL_OBFUSCATE_BUF(wlc->primary_queue),
		OSL_OBFUSCATE_BUF(wlc->active_queue)));

	/* Attaches tx qi */
	chanctxt->count++;
	WL_CHANCTXT_INFO(("wl%d.%d: %s: using chanctxt queue: count <%d, %d>\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, chanctxt->count,
		chanctxt_info->excursion_count));

	if (chanctxt->count == 1) {
		uint8 idx;

		if (chanctxt_info->excursion_count > 0) {
			WL_CHANCTXT_ASSERT(chanctxt_info->excursion_chanspec != 0 &&
				WLC_CHAN_COEXIST(chanctxt_info->excursion_chanspec,
				chanspec), ("[ASSERT]wl%d.%d: %s: excursion queue "
				"chanspec 0x%04x is not equal to current chanspec 0x%04x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				chanctxt_info->excursion_chanspec, chanspec));

			chanctxt->count += (chanctxt_info->excursion_count - (cfg? 0 : 1));
			chanctxt->piggyback = TRUE;
			chanctxt_info->piggyback_chanctxt = chanctxt;

			wlc_excursion_end(wlc);
		}

		WL_CHANCTXT_ASSERT(!wlc->excursion_active,
			("[ASSERT]wl%d.%d: %s: excursion queue is still active\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

		wlc_suspend_mac_and_wait(wlc);
		wlc_primary_queue_set(wlc, qi);
		wlc_enable_mac(wlc);

		/* restore the chancontext->fragthreshold to wlc while going on channel */
		for (idx = 0; idx < AC_COUNT; idx++) {
			wlc_fragthresh_set(wlc, idx, chanctxt->fragthresh[idx]);
		}
	}
	else {
		/* now excursion cannot be active, only piggyback should happen */
		ASSERT(wlc->excursion_active == FALSE);
	}

	WL_CHANCTXT_ASSERT(wlc->active_queue == qi,
		("[ASSERT]wl%d.%d: %s: chanctxt queue is not active queue: "
		"count <%d, %d>\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		chanctxt->count, chanctxt_info->excursion_count));

	wlc_chanctxt_start_queue(wlc, chanctxt);

done:
	WLCNTDECR(wlc->pub->_cnt->txqueue_start);
	WL_CHANCTXT_TRACE(("[EXIT]wl%d.%d: %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
}

void
wlc_txqueue_end(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_chanctxt_info_t *chanctxt_info = wlc->chanctxt_info;
	wlc_chanctxt_t *chanctxt;
	wlc_txq_info_t *qi;

	WL_CHANCTXT_TRACE(("[ENTRY]wl%d.%d: %s: call by %p\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, CALL_SITE));

	if (WLCNTVAL(wlc->pub->_cnt->txqueue_start) > 0) {
		WL_ERROR(("wl%d.%d txqueue_start was in progress\n",
			wlc->pub->unit, cfg ? WLC_BSSCFG_IDX(cfg) : -1));
		OSL_SYS_HALT();
	}
	else {
		WLCNTINCR(wlc->pub->_cnt->txqueue_end);
	}

	if ((chanctxt = wlc_chanctxt_get_end_ctxt(chanctxt_info, cfg)) == NULL) {
		goto done;
	}

	ASSERT(chanctxt->count > 0);

	chanctxt->count--;
	if (chanctxt != chanctxt_info->curr_chanctxt) {
		WL_CHANCTXT_INFO(("wl%d.%d: %s: chanctxt is not current chanctxt\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		goto done;
	}

	qi = chanctxt->qi;
	WL_MQ(("wl%d.%d: %s: qi %p, primary %p, active %p\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		OSL_OBFUSCATE_BUF(qi), OSL_OBFUSCATE_BUF(wlc->primary_queue),
		OSL_OBFUSCATE_BUF(wlc->active_queue)));

	/* Detaches tx qi */
	WL_CHANCTXT_INFO(("wl%d.%d: %s: ending chanctxt queue: count <%d, %d>\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, chanctxt->count,
		chanctxt_info->excursion_count));
	if (chanctxt->count == 0) {
		uint idx;

		wlc_txflowcontrol_override(wlc, qi, ON, TXQ_STOP_FOR_MSCH_FLOW_CNTRL);

		wlc_suspend_mac_and_wait(wlc);
		wlc_primary_queue_set(wlc, NULL);
		wlc_enable_mac(wlc);

		/* save the chancontext->fragthreshold from wlc while going off channel */
		for (idx = 0; idx < AC_COUNT; idx++) {
			chanctxt->fragthresh[idx] = wlc->fragthresh[idx];
		}
	} else {
		wlc_txflowcontrol_override(wlc, qi, OFF, TXQ_STOP_FOR_MSCH_FLOW_CNTRL);
	}

done:
	WLCNTDECR(wlc->pub->_cnt->txqueue_end);
	WL_CHANCTXT_TRACE(("[EXIT]wl%d.%d: %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
}

bool
wlc_txqueue_active(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);

	ASSERT(cbi);

	return (cbi->chanctxt && cbi->on_channel);
}

bool
wlc_has_chanctxt(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);

	ASSERT(cbi);

	return cbi->chanctxt != NULL;
}

chanspec_t
wlc_get_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);
	wlc_chanctxt_t *chanctxt;

	ASSERT(cbi);
	chanctxt = cbi->chanctxt;

	return (chanctxt ? chanctxt->chanspec : INVCHANSPEC);
}

static bool
_wlc_shared_chanctxt(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_chanctxt_t *shared_chanctxt)
{
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);
	wlc_chanctxt_t *chanctxt;

	ASSERT(cbi);
	chanctxt = cbi->chanctxt;

	if (chanctxt) {
		if (chanctxt == shared_chanctxt)
			return TRUE;

		if (cbi->multi_channel) {
			int i;
			for (i = 0; i < cbi->num_channel; i++) {
				if (cbi->mchanctxt[i] == shared_chanctxt)
					return TRUE;
			}
		}
	}
	return FALSE;
}

bool
wlc_shared_chanctxt_on_chan(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chan)
{
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);
	wlc_chanctxt_t *chanctxt;

	ASSERT(cbi);
	chanctxt = cbi->chanctxt;

	if (chanctxt) {
		if (chanctxt->chanspec == chan)
			return TRUE;

		if (cbi->multi_channel) {
			int i;
			for (i = 0; i < cbi->num_channel; i++) {
				if (cbi->mchanctxt[i]->chanspec == chan)
					return TRUE;
			}
		}
	}
	return FALSE;
}

bool
wlc_shared_chanctxt(wlc_info_t *wlc, wlc_bsscfg_t *cfg1, wlc_bsscfg_t *cfg2)
{
	chanctxt_bss_info_t *cbi1 = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg1);
	chanctxt_bss_info_t *cbi2 = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg2);

	ASSERT(cbi1 && cbi2);

	return cbi1->chanctxt != NULL && cbi2->chanctxt != NULL &&
		cbi1->chanctxt == cbi2->chanctxt;
}

bool
wlc_shared_current_chanctxt(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_chanctxt_info_t *chanctxt_info = wlc->chanctxt_info;
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, cfg);
	wlc_chanctxt_t *chanctxt;

	ASSERT(cbi);
	chanctxt = cbi->chanctxt;

	return ((chanctxt != NULL) && (chanctxt == chanctxt_info->curr_chanctxt));
}

bool
_wlc_ovlp_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chspec, uint chbw)
{
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);
	wlc_chanctxt_t *chanctxt;

	ASSERT(cbi);
	chanctxt = cbi->chanctxt;

	return chanctxt == NULL ||
	        CHSPEC_CTLOVLP(chanctxt->chanspec, chspec, (int)chbw);
}

bool
wlc_ovlp_chanspec(wlc_info_t *wlc, wlc_bsscfg_t *cfg1, wlc_bsscfg_t *cfg2, uint chbw)
{
	chanctxt_bss_info_t *cbi1 = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg1);
	chanctxt_bss_info_t *cbi2 = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg2);
	wlc_chanctxt_t *chantxt1, *chantxt2;

	ASSERT(cbi1 && cbi2);
	chantxt1 = cbi1->chanctxt;
	chantxt2 = cbi2->chanctxt;

	return chantxt1 == NULL || chantxt2 == NULL || chantxt1 == chantxt2 ||
	        CHSPEC_CTLOVLP(chantxt1->chanspec, chantxt2->chanspec, (int)chbw);
}

/*
* set TRUE if required to preserve old chanctxt while reqeusting new one
* set to FALSE by default
*/
void
wlc_chanctxt_set_passive_use(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool value)
{
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);
	wlc_chanctxt_t *chanctxt;

	ASSERT(cbi);
	chanctxt = cbi->chanctxt;
	chanctxt->in_passive_use = value;
}

static bool
wlc_chanctxt_in_use(wlc_info_t *wlc, wlc_chanctxt_t *chanctxt, wlc_bsscfg_t *filter_cfg)
{
	int idx;
	wlc_bsscfg_t *cfg;

	ASSERT(chanctxt != NULL);
	if (chanctxt->piggyback) {
		return (TRUE);
	}

	FOREACH_BSS(wlc, idx, cfg) {
		if ((cfg != filter_cfg) &&
			_wlc_shared_chanctxt(wlc, cfg, chanctxt)) {
			return (TRUE);
		}
	}
	return (FALSE);
}

static bool
wlc_tx_qi_exist(wlc_info_t *wlc, wlc_txq_info_t *qi)
{
	wlc_txq_info_t *qi_list;

	/* walk through all queues */
	for (qi_list = wlc->tx_queues; qi_list; qi_list = qi_list->next) {
		if (qi == qi_list) {
			/* found it in the list, return true */
			return TRUE;
		}
	}

	return FALSE;
}

static wlc_txq_info_t *
wlc_get_qi_with_no_context(wlc_info_t *wlc)
{
	wlc_chanctxt_info_t *chanctxt_info = wlc->chanctxt_info;
	wlc_chanctxt_t *chanctxt_entity;
	wlc_txq_info_t *qi_list;
	bool found;

	/* walk through all queues */
	for (qi_list = wlc->tx_queues; qi_list; qi_list = qi_list->next) {
		found = FALSE;
		/* walk through all context */
		chanctxt_entity = chanctxt_info->chanctxt_list;
		for (; chanctxt_entity; chanctxt_entity = chanctxt_entity->next) {
			if (chanctxt_entity->qi == qi_list) {
				found = TRUE;
				break;
			}
		}
		/* make sure it is not excursion_queue */
		if (qi_list == wlc->excursion_queue) {
			found = TRUE;
		}
		/* if found is false, current qi is without context */
		if (!found) {
			WL_MQ(("found qi %p with no associated ctx\n", OSL_OBFUSCATE_BUF(qi_list)));
			return (qi_list);
		}
	}

	return (NULL);
}

/** Take care of all the stuff needed when a ctx channel is set */
static void
wlc_chanctxt_set_channel(wlc_chanctxt_info_t *chanctxt_info,
	wlc_chanctxt_t *chanctxt, chanspec_t chanspec, bool destroy_old)
{
#ifdef PHYCAL_CACHING
	/* delete phy context for calibration */
	wlc_info_t *wlc = chanctxt_info->wlc;
	/* if CSA failed, need to del last CSA phycal entry */
	if ((wlc->last_CSA_chanspec != 0) && (chanspec != wlc->last_CSA_chanspec)) {
		phy_chanmgr_destroy_ctx(WLC_PI(chanctxt_info->wlc),
			wlc->last_CSA_chanspec);
	}
	wlc->last_CSA_chanspec = 0;
#endif /* PHYCAL_CACHING */

	if (destroy_old) {
#ifdef PHYCAL_CACHING
		/* delete phy context for calibration */
		phy_chanmgr_destroy_ctx(WLC_PI(chanctxt_info->wlc),
			chanctxt->chanspec);
#endif /* PHYCAL_CACHING */
	}
	chanctxt->chanspec = chanspec;

#ifdef PHYCAL_CACHING
	/* create phy context for calibration */
	phy_chanmgr_create_ctx(WLC_PI(chanctxt_info->wlc), chanspec);
#endif /* PHYCAL_CACHING */
}

#ifdef SRHWVSDB
static void
wlc_chanctxt_srvsdb_upd(wlc_chanctxt_info_t *chanctxt_info)
{
	wlc_chanctxt_t *chanctxt = chanctxt_info->chanctxt_list;
	wlc_info_t *wlc = chanctxt_info->wlc;

	if (SRHWVSDB_ENAB(wlc->pub)) {
		/* reset the engine so that previously saved stuff will be gone */
		wlc_srvsdb_reset_engine(wlc);

		/* Attach SRVSDB module */
		if (wlc_multi_chanctxt(wlc)) {
			wlc_srvsdb_set_chanspec(wlc, chanctxt->chanspec, chanctxt->next->chanspec);
			if (wlc_bmac_activate_srvsdb(wlc->hw, chanctxt->chanspec,
				chanctxt->next->chanspec) != BCME_OK) {
				wlc_bmac_deactivate_srvsdb(wlc->hw);
			}
		} else {
			wlc_bmac_deactivate_srvsdb(wlc->hw);
		}
	}
}
#endif /* SRHWVSDB */

static wlc_chanctxt_t *
wlc_chanctxt_alloc(wlc_chanctxt_info_t *chanctxt_info, chanspec_t chanspec)
{
	wlc_info_t *wlc = chanctxt_info->wlc;
	osl_t *osh = wlc->osh;
	wlc_chanctxt_t *chanctxt;
	wlc_txq_info_t *qi;
	uint idx;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	WL_MQ(("wl%d: %s: channel %s\n", wlc->pub->unit, __FUNCTION__,
		wf_chspec_ntoa(chanspec, chanbuf)));

	chanctxt = (wlc_chanctxt_t *)MALLOCZ(osh, sizeof(wlc_chanctxt_t));
	if (chanctxt == NULL) {
		WL_ERROR(("wl%d: wlc_chanctxt_alloc: failed to allocate "
			"mem for chan context\n", wlc->pub->unit));
		return (NULL);
	}

	qi = wlc_get_qi_with_no_context(wlc);
	/* if qi with no context found, then use it */
	if (qi) {
		chanctxt->qi = qi;
		WL_MQ(("use existing qi = %p\n", OSL_OBFUSCATE_BUF(qi)));
	}
	/* else allocate a new queue */
	else {
		chanctxt->qi = wlc_txq_alloc(wlc, osh);
		WL_MQ(("allocate new qi = %p\n", OSL_OBFUSCATE_BUF(chanctxt->qi)));
	}

	ASSERT(chanctxt->qi != NULL);
	wlc_chanctxt_set_channel(chanctxt_info, chanctxt, chanspec, FALSE);

	/* initialize chanctxt specific fragthreshold to default value */
	for (idx = 0; idx < AC_COUNT; idx++) {
		chanctxt->fragthresh[idx] = wlc->usr_fragthresh;
	}

	/* add chanctxt to head of context list */
	chanctxt->next = chanctxt_info->chanctxt_list;
	chanctxt_info->chanctxt_list = chanctxt;

#ifdef SRHWVSDB
	wlc_chanctxt_srvsdb_upd(chanctxt_info);
#endif /* SRHWVSDB */

	return (chanctxt);
}

static void
wlc_chanctxt_free(wlc_chanctxt_info_t *chanctxt_info, wlc_chanctxt_t *chanctxt)
{
	wlc_info_t *wlc = chanctxt_info->wlc;
	osl_t *osh = wlc->osh;
	wlc_txq_info_t *oldqi;
#ifdef PHYCAL_CACHING
	chanspec_t chanspec = chanctxt->chanspec; /* Need to know which chanspec to delete */
#endif /* PHYCAL_CACHING */
	wlc_chanctxt_t *prev, *next;
	int idx,  prec;
	wlc_bsscfg_t *cfg;
	void * pkt;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	WL_MQ(("wl%d: %s: channel %s\n", wlc->pub->unit, __FUNCTION__,
		wf_chspec_ntoa(chanctxt->chanspec, chanbuf)));

	ASSERT(chanctxt != NULL);

	/* save the qi pointer, delete from context list and free context */
	oldqi = chanctxt->qi;

	/* remove target from list and chain context list to next one */
	prev = (wlc_chanctxt_t *)&chanctxt_info->chanctxt_list;

	if (prev == chanctxt) {
		chanctxt_info->chanctxt_list = prev->next;
	} else {
		while ((next = prev->next)) {
			if (next == chanctxt) {
				prev->next = chanctxt->next;
				break;
			}
			prev = next;
		}
	}
	MFREE(osh, chanctxt, sizeof(wlc_chanctxt_t));

#ifdef PHYCAL_CACHING
	/* delete phy context for calibration */
	phy_chanmgr_destroy_ctx(WLC_PI(wlc), chanspec);
#endif /* PHYCAL_CACHING */

#ifdef SRHWVSDB
	wlc_chanctxt_srvsdb_upd(chanctxt_info);
#endif /* SRHWVSDB */

	chanctxt = chanctxt_info->chanctxt_list;

	/* continue with deleting this queue only if there are more contexts */
	if (chanctxt == NULL) {
		/* flush the queue */
		WL_MQ(("wl%d: %s: our queue is only context queue, don't delete, "
			"simply flush the queue, qi 0x%p len %d\n", wlc->pub->unit,
			__FUNCTION__, OSL_OBFUSCATE_BUF(oldqi), (WLC_GET_CQ(oldqi))->n_pkts_tot));
		/* flush lowtxq first to prevent ampdu bar packet enqueue to common queue */
		wlc_low_txq_flush(wlc->txqi, oldqi->low_txq);
		cpktq_flush(wlc, &oldqi->cpktq);
		ASSERT(pktq_empty(WLC_GET_CQ(oldqi)));
		return;
	}

	/* if this queue is the primary_queue, detach it first */
	if (oldqi == wlc->primary_queue) {
		ASSERT(oldqi != chanctxt->qi);
		WL_MQ(("wl%d: %s: detach primary queue %p\n",
			wlc->pub->unit, __FUNCTION__,
			OSL_OBFUSCATE_BUF(wlc->primary_queue)));
		wlc_suspend_mac_and_wait(wlc);
		wlc_primary_queue_set(wlc, NULL);
		wlc_enable_mac(wlc);
	}

	/* Before freeing this queue, any bsscfg that is using this queue
	 * should now use the primary queue.
	 * Active queue can be the excursion queue if we're scanning so using
	 * the primary queue is more appropriate.  When scan is done, active
	 * queue will be switched back to primary queue.
	 * This can be true for bsscfg's that never had contexts.
	 */
	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg->wlcif->qi == oldqi) {
			WL_MQ(("wl%d.%d: %s: cfg's qi %p is about to get deleted, "
				"move to queue %p\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
				__FUNCTION__, oldqi, OSL_OBFUSCATE_BUF(chanctxt->qi)));
			cfg->wlcif->qi = chanctxt->qi;
		}
	}

	/* flush the queue */
	/* wlc_txq_pktq_flush(wlc, &oldqi->q); */
	while ((pkt = cpktq_deq(&oldqi->cpktq, &prec))) {
#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			wlc_process_wlhdr_txstatus(wlc,
				WLFC_CTL_PKTFLAG_DROPPED, pkt, FALSE);
		}
#endif /* PROP_TXSTATUS */
		PKTFREE(osh, pkt, TRUE);
	}

	ASSERT(pktq_empty(WLC_GET_CQ(oldqi)));

	/* delete the queue only if it is not default primary queue.
	 * def_primary_queue should be freed only in wlc_detach().
	 */
	if (oldqi != wlc->def_primary_queue) {
		wlc_txq_free(wlc, osh, oldqi);
		WL_MQ(("wl%d %s: flush and free q\n", wlc->pub->unit, __FUNCTION__));
	}
}

/**
 * This function returns channel context pointer based on chanspec passed in
 * If no context associated to chanspec, will return NULL.
 */
static wlc_chanctxt_t *
wlc_get_chanctxt(wlc_chanctxt_info_t *chanctxt_info, chanspec_t chanspec)
{
	wlc_chanctxt_t *chanctxt = chanctxt_info->chanctxt_list;

	while (chanctxt) {
		if (WLC_CHAN_COEXIST(chanctxt->chanspec, chanspec)) {
			return chanctxt;
		}
		chanctxt = chanctxt->next;
	}

	return NULL;
}

/**
 * Given a chanctxt and chanspec as input, looks at all bsscfg using this
 * chanctxt and see if bsscfg->chanspec matches exactly with chanspec.
 * Returns TRUE if all bsscfg->chanspec matches chanspec.  Else returns FALSE.
 */
static bool
wlc_all_cfg_chanspec_is_same(wlc_info_t *wlc,
	wlc_chanctxt_t *chanctxt, chanspec_t chanspec)
{
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_AS_BSS(wlc, idx, cfg) {
		chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(wlc->chanctxt_info, cfg);
		if (cbi->chanctxt == chanctxt &&
		    !WLC_CHAN_COEXIST(cfg->current_bss->chanspec, chanspec)) {
			return (FALSE);
		}
	}
	return (TRUE);
}

static void
wlc_chanctxt_start_queue(wlc_info_t *wlc, wlc_chanctxt_t *chanctxt)
{
	wlc_chanctxt_info_t *chanctxt_info = wlc->chanctxt_info;
	wlc_txq_info_t *qi = chanctxt->qi;

	wlc_txflowcontrol_override(wlc, qi, OFF, TXQ_STOP_FOR_MSCH_FLOW_CNTRL);

	/* run txq if not empty */
	if (WLC_TXQ_OCCUPIED(wlc)) {
		wlc_send_q(wlc, qi);
	}

	if (chanctxt_info->curr_chanctxt != NULL &&
		chanctxt_info->curr_chanctxt->chanspec != chanctxt->chanspec) {
		/* Tell CHANIM that we're about to switch channels */
		if (wlc_lq_chanim_adopt_bss_chan_context(wlc, chanctxt->chanspec,
			chanctxt_info->curr_chanctxt->chanspec) != BCME_OK) {
			WL_INFORM(("wl%d %s: chanim adopt blocked scan/assoc/rm: \n",
			wlc->pub->unit, __FUNCTION__));
		}
	}

	chanctxt_info->curr_chanctxt = chanctxt;
	chanctxt->usage_countdown = (uint8)wlc->pub->tunables->max_wait_for_ctxt_delete;
}

static void
wlc_chanctxt_switch_queue(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_chanctxt_t *chanctxt,
	uint oldqi_stopped_flag)
{
	wlc_txq_info_t *oldqi = cfg->wlcif->qi;
	bool flowcontrol;

	cfg->wlcif->qi = chanctxt->qi;
#ifdef WDS
	/* update primary queue of wds scb if any */
	wlc_dwds_scb_switch_queue(wlc, cfg, chanctxt->qi);
#endif /* WDS */

	/* check old qi to see if we need to perform flow control on new qi.
	 * Only need to check TXQ_STOP_FOR_PKT_DRAIN because it is cfg specific.
	 * All other TXQ_STOP types are queue specific.
	 * Existing flow control setting only pertinent if we already have chanctxt.
	 */
	flowcontrol = wlc_txflowcontrol_override_isset(wlc, oldqi, TXQ_STOP_FOR_PKT_DRAIN);

	/* if we switched queue/context, need to take care of flow control */
	if (oldqi != chanctxt->qi) {
		/* evaluate to see if we need to enable flow control for PKT_DRAIN
		 * If flowcontrol is true, this means our old chanctxt has flow control
		 * of type TXQ_STOP_FOR_PKT_DRAIN enabled.  Transfer this setting to new qi.
		 */
		if (flowcontrol) {
			WL_MQ(("wl%d.%d: %s: turn ON flow control for "
				  "TXQ_STOP_FOR_PKT_DRAIN on new qi %p\n",
				  wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				  OSL_OBFUSCATE_BUF(chanctxt->qi)));
			wlc_txflowcontrol_override(wlc, chanctxt->qi,
				ON, TXQ_STOP_FOR_PKT_DRAIN);
			/* Check our oldqi, if it is different and exists,
			 * then we need to disable flow control on it
			 * since we just moved queue.
			 */
			if (wlc_tx_qi_exist(wlc, oldqi)) {
				WL_MQ(("wl%d.%d: %s: turn OFF flow control for "
					  "TXQ_STOP_FOR_PKT_DRAIN on old qi %p\n",
					  wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
					  OSL_OBFUSCATE_BUF(oldqi)));
				wlc_txflowcontrol_override(wlc, oldqi, OFF,
					TXQ_STOP_FOR_PKT_DRAIN);
			}
		}
		else {
			/* This is to take care of cases where old context was deleted
			 * while flow control was on without TXQ_STOP_FOR_PKT_DRAIN being set.
			 * This means our cfg's interface is flow controlled at the wl layer.
			 * Need to disable flow control now that we have a new queue.
			 */
			if (oldqi_stopped_flag) {
				WL_MQ(("wl%d.%d: %s: turn OFF flow control for "
					  "TXQ_STOP_FOR_PKT_DRAIN on new qi %p\n",
					  wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
					  OSL_OBFUSCATE_BUF(oldqi)));
				wlc_txflowcontrol_override(wlc, chanctxt->qi, OFF,
					TXQ_STOP_FOR_PKT_DRAIN);
			}
		}
	}
}

/** before bsscfg register to MSCH, call this function */
static wlc_chanctxt_t *
wlc_chanctxt_create_txqueue(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chanspec)
{
	wlc_chanctxt_info_t *chanctxt_info = wlc->chanctxt_info;
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, cfg);
	wlc_chanctxt_t *chanctxt, *new_chanctxt;
	uint oldqi_stopped_flag = 0;
#ifdef ENABLE_FCBS
	phy_info_t *pi = WLC_PI(wlc);
#endif // endif
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(cbi);
	chanctxt = cbi->chanctxt;

	WL_CHANCTXT_TRACE(("[ENTRY]wl%d.%d: wlc_chanctxt_create_txqueue: "
		"chanspec 0x%04x, call by %p\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
		chanspec, CALL_SITE));

	WL_MQ(("wl%d.%d: %s: called on %s chanspec %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		BSSCFG_AP(cfg) ? "AP" : "STA",
		wf_chspec_ntoa_ex(chanspec, chanbuf)));

	/* fetch context for this chanspec */
	new_chanctxt = wlc_get_chanctxt(chanctxt_info, chanspec);

	/* This cfg has an existing context, do some checks to determine what to do */
	if (chanctxt) {
		WL_MQ(("wl%d.%d: %s: cfg alredy has context!\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

		if (chanctxt->chanspec != chanspec) {
			wlc_lq_chanim_create_bss_chan_context(wlc, chanspec, chanctxt->chanspec);
		}

		if (WLC_CHAN_COEXIST(chanctxt->chanspec, chanspec)) {
			/* we know control channel is same but if chanspecs are not
			 * identical, will want chan context to adopt to new chanspec,
			 * if no other cfg's using it or all cfg's using that context
			 * are using new chanspec.
			 */
			if ((chanctxt->chanspec != chanspec) &&
				(!wlc_chanctxt_in_use(wlc, chanctxt, cfg) ||
				wlc_all_cfg_chanspec_is_same(wlc, chanctxt,
					chanspec)) &&
				((chanspec == wlc_get_cur_wider_chanspec(wlc, cfg)))) {

				WL_MQ(("context is the same but need to adopt "
				          "from chanspec 0x%x to 0x%x\n",
				          chanctxt->chanspec, chanspec));
#ifdef ENABLE_FCBS
				wlc_phy_fcbs_uninit(pi, chanctxt->chanspec);
				wlc_phy_fcbs_arm(pi, chanspec, 0);
#endif // endif
				wlc_chanctxt_set_channel(chanctxt_info, chanctxt, chanspec,
					!BSSCFG_IS_MODESW_BWSW(cfg));
			}
			else {
				WL_MQ(("context is the same as new one, do nothing\n"));
			}
			return chanctxt;
		}
		/* Requested nonoverlapping channel. */
		else {
			if (!wlc_chanctxt_in_use(wlc, chanctxt, cfg) &&
				!chanctxt->in_passive_use && !cbi->multi_channel) {
				/* delete if not required to preserve chancxt immediately */
				wlc_chanctxt_delete_txqueue(wlc, cfg, chanctxt,
					&oldqi_stopped_flag);
			}
			/*
			* else chanctxt will get deleted in watchdog
			* if not used for 'usage_countdown' seconds
			*/
		}
		cbi->chanctxt = NULL;
	}

	/* check to see if a context for this chanspec already exist */
	if (new_chanctxt == NULL) {
		/* context for this chanspec doesn't exist, create a new one */
		WL_MQ(("wl%d.%d: %s: allocate new context\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		new_chanctxt = wlc_chanctxt_alloc(chanctxt_info, chanspec);
#ifdef ENABLE_FCBS
		if (!wlc_phy_fcbs_arm(pi, chanspec, 0))
			WL_MQ(("%s: wlc_phy_fcbs_arm FAILed\n", __FUNCTION__));
#endif // endif
	}

	ASSERT(new_chanctxt != NULL);
	if (new_chanctxt == NULL)
		return NULL;
	wlc_lq_chanim_create_bss_chan_context(wlc, chanspec, new_chanctxt->chanspec);

	/* assign context to cfg */
	wlc_chanctxt_switch_queue(wlc, cfg, new_chanctxt, oldqi_stopped_flag);
	cbi->chanctxt = new_chanctxt;

	if (CHSPEC_BW_GT(chanspec, CHSPEC_BW(new_chanctxt->chanspec))) {
		WL_MQ(("wl%d.%d: %s: Upgrade chanctxt chanspec "
			"from 0x%x to 0x%x\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			new_chanctxt->chanspec, chanspec));
#ifdef ENABLE_FCBS
		wlc_phy_fcbs_uninit(pi, new_chanctxt->chanspec);
		wlc_phy_fcbs_arm(pi, chanspec, 0);
#endif // endif
		wlc_chanctxt_set_channel(chanctxt_info, new_chanctxt, chanspec,
			!BSSCFG_IS_MODESW_BWSW(cfg));
	}

	WL_MQ(("wl%d.%d: %s: cfg chanctxt chanspec set to %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		wf_chspec_ntoa_ex(new_chanctxt->chanspec, chanbuf)));

	return new_chanctxt;
}

/** Everytime bsscfg becomes disassociated, call this function */
static int
wlc_chanctxt_delete_txqueue(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_chanctxt_t *chanctxt,
	uint *oldqi_info)
{
	wlc_chanctxt_info_t *chanctxt_info = wlc->chanctxt_info;
	chanctxt_bss_info_t *cbi = CHANCTXT_BSS_INFO(chanctxt_info, cfg);
	bool saved_state = cfg->associated;
	bool flowcontrol;
	DBGONLY(char chanbuf[CHANSPEC_STR_LEN]; )

	ASSERT(cbi);

	WL_CHANCTXT_TRACE(("[ENTRY]wl%d.%d: wlc_chanctxt_delete_txqueue: "
		"on_channel %d, call by %p\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
		cbi->on_channel, CALL_SITE));

	/* if context was not created due to AP up being deferred and we called down,
	 * can have a chan_ctxt that is NULL.
	 */
	if (chanctxt == NULL) {
		return (BCME_OK);
	}

	WL_MQ(("wl%d.%d: %s: called on %s chanspec %s\n",
		wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		BSSCFG_AP(cfg) ? "AP" : "STA",
		wf_chspec_ntoa_ex(chanctxt->chanspec, chanbuf)));

	/* chanim stas are per radio stats. This has to be deleted
	 * only when there are no other active bss's available.
	 */
	if (wlc_bsscfg_none_bss_active(wlc)) {
		wlc_lq_chanim_delete_bss_chan_context(wlc, chanctxt->chanspec);
	}

	/* temporarily clear the cfg associated state.
	 * during roam, we are associated so when we do roam,
	 * we can potentially be deleting our old context first
	 * before creating our new context.  Just want to make sure
	 * that when we delete context, we're not associated.
	 */
	cfg->associated = FALSE;

	if (chanctxt != cbi->chanctxt) {
		goto delete_chanctxt;
	}

	if (cbi->on_channel) {
		ASSERT(chanctxt->count > 0);
		chanctxt->count--;
		cbi->on_channel = FALSE;
	}

#ifdef STA
	if (BSSCFG_STA(cfg)) {
#if defined(WLMCHAN) && defined(WLP2P)
		if (MCHAN_ENAB(wlc->pub) && P2P_CLIENT(wlc, cfg)) {
			if (wlc_p2p_noa_valid(wlc->p2p, cfg)) {
				mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_MCHAN_ABS);
			}
		} else
#endif /* WLP2P */
		{
			mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_CHANSW);
		}
		wlc_update_pmstate(cfg, TX_STATUS_NO_ACK);
	}
#endif /* STA */
#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_wlfc_flush_pkts_to_host(wlc, cfg);
		wlfc_sendup_ctl_info_now(wlc->wlfc);
	}
#endif /* PROP_TXSTATUS */

	flowcontrol =  (chanctxt->qi->stopped) & ~TXQ_STOP_FOR_PRIOFC_MASK;

	if (flowcontrol) {
		wlc_txflowcontrol_override(wlc, chanctxt->qi, OFF,
			TXQ_STOP_FOR_MSCH_FLOW_CNTRL);
	}

	/* if txq currently in use by cfg->wlcif->qi is deleted in
	 * wlc_mchanctxt_free(), cfg->wlcif->qi will be set to the primary queue
	 */

	/* Take care of flow control for this cfg.
	 * If it was turned on for the queue, then this cfg has flow control on.
	 * Turn it off for this cfg only.
	 * Allocate a stale queue and temporarily attach cfg to this queue.
	 * Copy over the qi->stopped flag and turn off flow control.
	 */
	if (chanctxt->qi->stopped) {
		if (oldqi_info != NULL) {
			*oldqi_info = chanctxt->qi->stopped;
		}
		else
		{
			wlc_txq_info_t *qi = wlc_txq_alloc(wlc, wlc->osh);
			wlc_txq_info_t *orig_qi = cfg->wlcif->qi;

			WL_MQ(("wl%d.%d: %s: flow control on, turn it off!\n", wlc->pub->unit,
			          WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			/* Use this new q to drain the wl layer packets to make sure flow control
			 * is turned off for this interface.
			 */
			qi->stopped = chanctxt->qi->stopped;
			cfg->wlcif->qi = qi;
			/* reset_qi() below will not allow tx flow control to turn back on */
			wlc_txflowcontrol_reset_qi(wlc, qi);
			/* if txq currently in use by cfg->wlcif->qi is deleted in
			 * wlc_infra_sched_free(), cfg->wlcif->qi will be set to the primary queue
			 */
			cpktq_flush(wlc, &qi->cpktq);
			ASSERT(pktq_empty(WLC_GET_CQ(qi)));
			wlc_txq_free(wlc, wlc->osh, qi);
			cfg->wlcif->qi = orig_qi;
		}
	}

	/* no one else using this context, safe to delete it */
delete_chanctxt:
	if (!wlc_chanctxt_in_use(wlc, chanctxt, cfg)) {
#ifdef ENABLE_FCBS
		if (!wlc_phy_fcbs_uninit(WLC_PI(wlc), chanctxt->chanspec))
			WL_INFORM(("%s: wlc_phy_fcbs_uninit() FAILed\n", __FUNCTION__));
#endif // endif
		if (chanctxt == chanctxt_info->curr_chanctxt) {
#ifdef STA
			/* If we were in the middle of transitioning to PM state because of this
			 * BSS, then cancel it
			 */
			if (wlc->PMpending) {
				wlc_bsscfg_t *icfg;
				int idx;
				FOREACH_AS_STA(wlc, idx, icfg) {
					if (icfg->pm->PMpending &&
						_wlc_shared_chanctxt(wlc, icfg, chanctxt)) {
						wlc_update_pmstate(icfg, TX_STATUS_BE);
					}
				}
			}
#endif /* STA */
			chanctxt_info->curr_chanctxt = NULL;
		}

		wlc_chanctxt_free(chanctxt_info, chanctxt);
		chanctxt = NULL;
	}
	/* XXX - chanspec downgrade shall be done at the bsscfg level
	 * when a bsscfg is added/free or modified so that MCHAN and
	 * non-MCHAN cases are handled in the same fashion.
	 */

	/* restore cfg associated state */
	cfg->associated = saved_state;

	return BCME_OK;
}

#ifdef PHYCAL_CACHING
static int
wlc_chanctxt_chk_calcache_cb(void *context)
{
	wlc_info_t *wlc = (wlc_info_t *)context;

	wlc_chanctxt_chk_calcache(wlc);

	return BCME_OK;
}

static void
wlc_chanctxt_chk_calcache(wlc_info_t *wlc)
{
	int idx;
	chanspec_t chanlist[PREALLOCATE_CHANCTXT_TOTAL];

	memset(chanlist, 0, sizeof(chanlist));
	wlc_phy_get_all_cached_ctx(wlc->pi, chanlist);

	for (idx = 0; idx < PREALLOCATE_CHANCTXT_TOTAL; idx++) {
		if (chanlist[idx] &&
		    wlc_chanctxt_is_deleted_calcache(wlc, chanlist[idx]))
			phy_chanmgr_destroy_ctx(WLC_PI(wlc), chanlist[idx]);
	}
}

static int
wlc_chanctxt_is_deleted_calcache(wlc_info_t *wlc, chanspec_t chanspec)
{
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_BSS(wlc, idx, cfg) {
		if (wlc_shared_chanctxt_on_chan(wlc, cfg, chanspec)) {
			return FALSE;
		}
	}

	return TRUE;
}
#endif /* PHYCAL_CACHING */
