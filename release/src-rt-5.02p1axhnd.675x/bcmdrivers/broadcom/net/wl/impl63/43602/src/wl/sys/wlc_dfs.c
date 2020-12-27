/*
 * 802.11h DFS module source file
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
 * $Id$
 */

/**
 * @file
 * @brief
 * Related to radar avoidance. Implements CAC (Clear Channel Assessment) state machine.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [DynamicFrequencySelection]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

/* XXX make sure WLDFS conditional is referenced after it is derived from WL11H and BAND5G.
 * in wlc_cfg.h. Move it up when it becomes an externally defined conditional.
 */
#ifdef WLDFS

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
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wl_export.h>
#include <wlc_ap.h>
#include <wlc_scan.h>
#include <wlc_phy_hal.h>
#include <wlc_quiet.h>
#include <wlc_csa.h>
#include <wlc_11h.h>
#include <wlc_dfs.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_DFS_PREISM,		/* preism cac time */
	IOV_DFS_POSTISM,	/* postism cac time */
	IOV_DFS_STATUS,		/* dfs cac status */
	IOV_DFS_ISM_MONITOR,    /* control the behavior of ISM state */
	IOV_DFS_CHANNEL_FORCED, /* next dfs channel forced */
	IOV_LAST
};

static const bcm_iovar_t wlc_dfs_iovars[] = {
	{"dfs_preism", IOV_DFS_PREISM, 0, IOVT_UINT32, 0},
	{"dfs_postism", IOV_DFS_POSTISM, 0, IOVT_UINT32, 0},
	{"dfs_status", IOV_DFS_STATUS, (0), IOVT_BUFFER, 0},
	{"dfs_ism_monitor", IOV_DFS_ISM_MONITOR, (0), IOVT_UINT32, 0},
	{"dfs_channel_forced", IOV_DFS_CHANNEL_FORCED, (0), IOVT_UINT32, 0},
	/* it is required for regulatory testing */
	{NULL, 0, 0, 0, 0}
};

typedef struct wlc_dfs_cac {
	int	cactime_pre_ism;	/* configured preism cac time in second */
	int	cactime_post_ism;	/* configured postism cac time in second */
	uint32	nop_sec;		/* Non-Operation Period in second */
	int	ism_monitor;		/* 0 for off, non-zero to force ISM to monitor-only mode */
	chanspec_list_t *chanspec_forced_list; /* list of chanspecs to use when radar detected */
	wl_dfs_status_t	status;		/* data structure for handling dfs_status iovar */
	uint	cactime;      /* holds cac time in WLC_DFS_RADAR_CHECK_INTERVAL for current state */
	/* use of duration
	 * 1. used as a down-counter where timer expiry is needed.
	 * 2. (cactime - duration) is time elapsed at current state
	 */
	uint	duration;
	chanspec_t chanspec_next;	/* next dfs channel */
	bool	timer_running;
} wlc_dfs_cac_t;

/* Country module info */
struct wlc_dfs_info {
	wlc_info_t *wlc;
	uint chan_blocked[MAXCHANNEL];	/* 11h: seconds remaining in channel
					 * out of service period due to radar
					 */
	bool dfs_cac_enabled;		/* set if dfs cac enabled */
	struct wl_timer *dfs_timer;	/* timer for dfs cac handler */
	wlc_dfs_cac_t dfs_cac;		/* channel availability check */
	uint32 radar;			/* radar info: just on or off for now */

	bool updown_cb_regd;		/* is updown callback registered */
	bool in_eu;			/* whenever the interface goes up, update whether in EU */
};

/* local functions */
/* module */
static int wlc_dfs_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_dfs_watchdog(void *ctx);
static int wlc_dfs_up(void *ctx);
static int wlc_dfs_down(void *ctx);
static void wlc_dfs_updown_cb(void *ctx, bsscfg_up_down_event_data_t *updown_data);
#ifdef BCMDBG
static int wlc_dfs_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/* others */
static void wlc_dfs_cacstate_init(wlc_dfs_info_t *dfs);
static int wlc_dfs_timer_init(wlc_dfs_info_t *dfs);
static void wlc_dfs_timer_add(wlc_dfs_info_t *dfs);
static bool wlc_dfs_timer_delete(wlc_dfs_info_t *dfs);
static void wlc_dfs_chanspec_oos(wlc_dfs_info_t *dfs, chanspec_t chanspec);
static chanspec_t wlc_dfs_chanspec(wlc_dfs_info_t *dfs, bool radar_detected);
static bool wlc_valid_ap_chanspec(wlc_dfs_info_t *dfs, chanspec_t chspec);
static bool wlc_radar_detected(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_idle_set(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ism_set(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ooc_set(wlc_dfs_info_t *dfs, uint target_state);
static void wlc_dfs_cacstate_idle(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_cac(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ism(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_csa(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ooc(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_handler(void *arg);
static bool wlc_dfs_validate_forced_param(wlc_info_t *wlc, chanspec_t chanspec);

/* IE mgmt */
#ifdef STA
#ifdef BCMDBG
static int wlc_dfs_bcn_parse_ibss_dfs_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif // endif
#endif /* STA */

/* Local Data Structures */
static void (*wlc_dfs_cacstate_fn_ary[WL_DFS_CACSTATES])(wlc_dfs_info_t *dfs) = {
	wlc_dfs_cacstate_idle,
	wlc_dfs_cacstate_cac, /* preism_cac */
	wlc_dfs_cacstate_ism,
	wlc_dfs_cacstate_csa,
	wlc_dfs_cacstate_cac, /* postism_cac */
	wlc_dfs_cacstate_ooc, /* preism_ooc */
	wlc_dfs_cacstate_ooc /* postism_ooc */
};

#if defined(BCMDBG) || defined(WLMSG_DFS)
static const char *wlc_dfs_cacstate_str[WL_DFS_CACSTATES] = {
	"IDLE",
	"PRE-ISM Channel Availability Check",
	"In-Service Monitoring(ISM)",
	"Channel Switching Announcement(CSA)",
	"POST-ISM Channel Availability Check",
	"PRE-ISM Out Of Channels(OOC)",
	"POSTISM Out Of Channels(OOC)"
};
#endif /* BCMDBG || WLMSG_DFS */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/*
 * State change function for easier tracking of state changes.
 */
static INLINE void
wlc_dfs_cac_state_change(wlc_dfs_info_t *dfs, uint newstate)
{
#if defined(BCMDBG) || defined(WLMSG_DFS)
	WL_DFS(("DFS State %s -> %s\n", wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		wlc_dfs_cacstate_str[newstate]));
#endif // endif
	dfs->dfs_cac.status.state = newstate;	/* Controlled (logged) state change */
}

/* called when observing toggle between EU to/from non-EU country */
static void
wlc_dfs_eu_toggle(wlc_dfs_info_t *dfs, bool in_eu)
{
	/* update in structure */
	dfs->in_eu = in_eu;
}

/* Callback from wl up/down. This Function helps reset on up/down events */
static void
wlc_dfs_updown_cb(void *ctx, bsscfg_up_down_event_data_t *updown_data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_dfs_info_t *dfs;

	ASSERT(wlc);
	ASSERT(updown_data);
	if (wlc->dfs == NULL) {
		return;
	}
	dfs = wlc->dfs;

	WL_DFS(("%s:got callback from updown. interface %s\n",
		__FUNCTION__, (updown_data->up?"up":"down")));

	if (updown_data->up == TRUE) {
		bool in_eu = wlc_is_dfs_eu(wlc);
		if (in_eu != dfs->in_eu) {
			wlc_dfs_eu_toggle(dfs, in_eu);
		}
		return;
	}

	/* when brought down, mark current DFS channel as quiet unless in EU */
	if (WL11H_ENAB(wlc) && dfs->radar &&
			wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) &&
			!wlc_is_edcrs_eu(wlc)) {
		wlc_set_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC);
	}
}

/* module */
wlc_dfs_info_t *
BCMATTACHFN(wlc_dfs_attach)(wlc_info_t *wlc)
{
	wlc_dfs_info_t *dfs;

	if ((dfs = MALLOCZ(wlc->osh, sizeof(wlc_dfs_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	dfs->wlc = wlc;
	dfs->updown_cb_regd = FALSE;

	/* register IE mgmt callbacks */
	/* parse */
#ifdef STA
#ifdef BCMDBG
	/* bcn */
	if (wlc_iem_add_parse_fn(wlc->iemi, FC_BEACON, DOT11_MNG_IBSS_DFS_ID,
	                         wlc_dfs_bcn_parse_ibss_dfs_ie, dfs) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, ibss dfs in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif
#endif /* STA */

	if (wlc_dfs_timer_init(dfs) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dfs_timer_init failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef BCMDBG
	if (wlc_dump_register(wlc->pub, "dfs", wlc_dfs_dump, dfs) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif // endif

	/* keep the module registration the last other add module unregistratin
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, wlc_dfs_iovars, "dfs", dfs, wlc_dfs_doiovar,
	                        wlc_dfs_watchdog, wlc_dfs_up, wlc_dfs_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};
	if (wlc_bsscfg_updown_register(wlc, wlc_dfs_updown_cb, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		ASSERT(0);
		goto fail;
	}
	dfs->updown_cb_regd = TRUE;
	return dfs;

	/* error handling */
fail:
	if (dfs != NULL) {
		if (dfs->dfs_timer != NULL)
			wl_free_timer(wlc->wl, dfs->dfs_timer);

		MFREE(wlc->osh, dfs, sizeof(wlc_dfs_info_t));
	}
	return NULL;
}

void
BCMATTACHFN(wlc_dfs_detach)(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	if (dfs->updown_cb_regd && wlc_bsscfg_updown_unregister(wlc,
		wlc_dfs_updown_cb, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_unregister() failed\n",
			wlc->pub->unit, __FUNCTION__));
		dfs->updown_cb_regd = FALSE;
	}
	wlc_module_unregister(wlc->pub, "dfs", dfs);

	if (dfs->dfs_timer != NULL)
		wl_free_timer(wlc->wl, dfs->dfs_timer);

	if (dfs->dfs_cac.chanspec_forced_list)
		MFREE(wlc->osh, dfs->dfs_cac.chanspec_forced_list,
			WL_CHSPEC_LIST_FIXED_SIZE +
			dfs->dfs_cac.chanspec_forced_list->num * sizeof(chanspec_t));
	MFREE(wlc->osh, dfs, sizeof(wlc_dfs_info_t));
}

/* Validate chanspec forced list configuration. */
static bool wlc_dfs_validate_forced_param(wlc_info_t *wlc, chanspec_t chanspec)
{
	if (!WL11H_ENAB(wlc) || !CHSPEC_IS5G(chanspec) ||
		wf_chspec_malformed(chanspec) ||
		(!wlc_valid_chanspec_db(wlc->cmi, chanspec)) ||
		(!N_ENAB(wlc->pub) && (CHSPEC_IS40(chanspec) ||
		CHSPEC_IS80(chanspec) || CHSPEC_IS160(chanspec) ||
		(CHSPEC_CHANNEL(chanspec) > MAXCHANNEL)))) {
		return FALSE;
	}

	return TRUE;
}

static int
wlc_dfs_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;
	bool_val2 = (int_val2 != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val2);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_DFS_PREISM):
		*ret_int_ptr = dfs->dfs_cac.cactime_pre_ism;
		break;

	case IOV_SVAL(IOV_DFS_PREISM):
		if ((int_val < -1) || (int_val >= WLC_DFS_CAC_TIME_SEC_MAX)) {
			err = BCME_RANGE;
			break;
		}
		dfs->dfs_cac.cactime_pre_ism = int_val;
		break;

	case IOV_GVAL(IOV_DFS_POSTISM):
		*ret_int_ptr = dfs->dfs_cac.cactime_post_ism;
		break;

	case IOV_SVAL(IOV_DFS_POSTISM):
		if ((int_val < -1) || (int_val >= WLC_DFS_CAC_TIME_SEC_MAX)) {
			err = BCME_RANGE;
			break;
		}
		dfs->dfs_cac.cactime_post_ism = int_val;
		break;

	case IOV_GVAL(IOV_DFS_STATUS):
		dfs->dfs_cac.status.duration =
		        (dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
		        WLC_DFS_RADAR_CHECK_INTERVAL;
		bcopy((char *)&dfs->dfs_cac.status, (char *)arg, sizeof(wl_dfs_status_t));
		break;

	case IOV_SVAL(IOV_DFS_ISM_MONITOR):
		dfs->dfs_cac.ism_monitor = bool_val;
		break;

	case IOV_GVAL(IOV_DFS_ISM_MONITOR):
		*ret_int_ptr = (int32)dfs->dfs_cac.ism_monitor;
		break;

	/* IOV_DFS_CHANNEL_FORCED is required for regulatory testing */
	case IOV_SVAL(IOV_DFS_CHANNEL_FORCED): {
		uint i;
		wl_dfs_forced_t *dfs_forced;
		chanspec_t chanspec = 0;

		if (p_len <= (int)sizeof(int_val)) {
			/* Validate Configuration */
			if (!wlc_dfs_validate_forced_param(wlc, (chanspec_t)int_val)) {
				err = BCME_BADCHAN;
				break;
			}
			/* Single configuration is required for regulatory testing */
			if (!dfs->dfs_cac.chanspec_forced_list) {
				dfs->dfs_cac.chanspec_forced_list =
					MALLOCZ(wlc->osh, (WL_CHSPEC_LIST_FIXED_SIZE +
					1 * sizeof(chanspec_t)));

			if (!dfs->dfs_cac.chanspec_forced_list) {
					err = BCME_NOMEM;
					break;
				}
			}
			dfs->dfs_cac.chanspec_forced_list->num = 1;

			if (CHSPEC_CHANNEL((chanspec_t)int_val) == 0) {
				/* This is to handle old command syntax - single chanspec */
				dfs->dfs_cac.chanspec_forced_list->list[0] =
					wlc_create_chspec(wlc, (uint8)int_val);
				break;
			}
			dfs->dfs_cac.chanspec_forced_list->list[0] =
				(chanspec_t)int_val;
		} else {
			dfs_forced = (wl_dfs_forced_t*)params;
			if (dfs_forced->version == DFS_PREFCHANLIST_VER) {
				/* We got list configuration */
				if (dfs_forced->chspec_list.num > WL_NUMCHANNELS) {
					err = BCME_BADCHAN;
					break;
				}
				/* Validate Configuration */
				for (i = 0; i < dfs_forced->chspec_list.num; i++) {
					if (N_ENAB(wlc->pub)) {
						chanspec = dfs_forced->chspec_list.list[i];
					} else {
						chanspec = wlc_create_chspec(wlc,
							(uint8)dfs_forced->chspec_list.list[i]);
					}
					if (!wlc_dfs_validate_forced_param(wlc, chanspec)) {
						err = BCME_BADCHAN;
						return err;
					}
				}
				/* Clears existing list if any */
				if (dfs->dfs_cac.chanspec_forced_list) {
					MFREE(wlc->osh, dfs->dfs_cac.chanspec_forced_list,
					  WL_CHSPEC_LIST_FIXED_SIZE +
					  dfs->dfs_cac.chanspec_forced_list->num *
					  sizeof(chanspec_t));
					dfs->dfs_cac.chanspec_forced_list = NULL;
				}
				if (!dfs_forced->chspec_list.num) {
					/* Clear configuration done */
					break;
				}
				dfs->dfs_cac.chanspec_forced_list =
					MALLOCZ(wlc->osh, (WL_CHSPEC_LIST_FIXED_SIZE +
					dfs_forced->chspec_list.num * sizeof(chanspec_t)));

				if (!dfs->dfs_cac.chanspec_forced_list) {
					err = BCME_NOMEM;
					break;
				}
				dfs->dfs_cac.chanspec_forced_list->num =
					dfs_forced->chspec_list.num;

				for (i = 0; i < dfs_forced->chspec_list.num; i++) {
					if (N_ENAB(wlc->pub)) {
						chanspec = dfs_forced->chspec_list.list[i];
					} else {
						chanspec = wlc_create_chspec(wlc,
							(uint8)dfs_forced->chspec_list.list[i]);
					}
					dfs->dfs_cac.chanspec_forced_list->list[i] = chanspec;
				}
			} else {
				err = BCME_VERSION;
				break;
			}
		}
	}
	break;

	case IOV_GVAL(IOV_DFS_CHANNEL_FORCED): {
		uint i;
		wl_dfs_forced_t *dfs_forced;
		chanspec_t chanspec = 0;

		if (p_len < sizeof(wl_dfs_forced_t)) {
			if (dfs->dfs_cac.chanspec_forced_list) {
				if (!N_ENAB(wlc->pub))
					*ret_int_ptr =
					CHSPEC_CHANNEL(dfs->dfs_cac.chanspec_forced_list->list[0]);
				else
					*ret_int_ptr = dfs->dfs_cac.chanspec_forced_list->list[0];
			} else
				*ret_int_ptr = 0;
		} else {
			wl_dfs_forced_t *inp = (wl_dfs_forced_t*)params;
			dfs_forced = (wl_dfs_forced_t*)arg;
			/* This is issued by new application */
			if (inp->version == DFS_PREFCHANLIST_VER) {
				dfs_forced->version = DFS_PREFCHANLIST_VER;
				if (dfs->dfs_cac.chanspec_forced_list) {
					uint nchan = dfs->dfs_cac.chanspec_forced_list->num;
					uint ioctl_size = WL_DFS_FORCED_PARAMS_FIXED_SIZE +
						dfs->dfs_cac.chanspec_forced_list->num *
						sizeof(chanspec_t);

					if (p_len < ioctl_size) {
						err = BCME_BUFTOOSHORT;
						break;
					}
					for (i = 0; i < nchan; i++) {
						chanspec =
							dfs->dfs_cac.chanspec_forced_list->list[i];
						if (!N_ENAB(wlc->pub))
							dfs_forced->chspec_list.list[i] =
								CHSPEC_CHANNEL(chanspec);
						else
							dfs_forced->chspec_list.list[i] = chanspec;
					}
					dfs_forced->chspec_list.num = nchan;
					break;
				} else {
					dfs_forced->chspec_list.num = 0;
					/* wlu reads dfs_forced->chspec when list is not there.
					 * So assigning it with 0 otherwise wlu reports junk.
					 * Driver does not use chspec, it uses chspec_list instead.
					 */
					dfs_forced->chspec = 0;
				}
			} else {
				err = BCME_VERSION;
				break;
			}
		}
	}
	break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_dfs_watchdog(void *ctx)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	/* Restore channels 30 minutes after radar detect */
	if (WL11H_ENAB(wlc) && dfs->radar) {
		int chan;

		for (chan = 0; chan < MAXCHANNEL; chan++) {
			if (dfs->chan_blocked[chan] &&
			    dfs->chan_blocked[chan] != WLC_CHANBLOCK_FOREVER) {
				dfs->chan_blocked[chan]--;
				if (!dfs->chan_blocked[chan]) {
					WL_REGULATORY(("\t** DFS *** Channel %d is"
					               " clean after 30 minutes\n", chan));
				}
			}
		}
	}

	return BCME_OK;
}

/* Nothing to be done for now, Usually none of the bss are up
 * by now.
 */
static int
wlc_dfs_up(void *ctx)
{
	return BCME_OK;
}

static int
wlc_dfs_down(void *ctx)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	int callback = 0;

	wlc_dfs_cacstate_idle_set(dfs);
	dfs->dfs_cac.status.chanspec_cleared = 0;
	/* cancel the radar timer */
	if (!wlc_dfs_timer_delete(dfs))
		callback = 1;
	dfs->dfs_cac_enabled = FALSE;

	return callback;
}

#ifdef BCMDBG
static int
wlc_dfs_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	uint32 i;

	bcm_bprintf(b, "radar %d\n", dfs->radar);
	bcm_bprhex(b, "chan_blocked ", TRUE,
	           (uint8 *)dfs->chan_blocked, sizeof(dfs->chan_blocked));
	bcm_bprintf(b, "cactime_pre_ism %u cactime_post_ism %u nop_sec %u ism_monitor %d\n",
	            dfs->dfs_cac.cactime_pre_ism, dfs->dfs_cac.cactime_post_ism,
	            dfs->dfs_cac.nop_sec, dfs->dfs_cac.ism_monitor);
	if (dfs->dfs_cac.chanspec_forced_list) {
		for (i = 0; i < dfs->dfs_cac.chanspec_forced_list->num; i++)
			bcm_bprintf(b, "chanspec_forced %x status %d cactime %u\n",
			            dfs->dfs_cac.chanspec_forced_list->list[i], dfs->dfs_cac.status,
			            dfs->dfs_cac.cactime, dfs->dfs_cac);
	}
	bcm_bprintf(b, "duration %u chanspec_next %x timer_running %d\n",
	            dfs->dfs_cac.duration, dfs->dfs_cac.chanspec_next,
	            dfs->dfs_cac.timer_running);
	bcm_bprintf(b, "dfs_cac_enabled %d\n", dfs->dfs_cac_enabled);

	return BCME_OK;
}
#endif /* BCMDBG */

uint32
wlc_dfs_get_chan_info(wlc_dfs_info_t *dfs, uint channel)
{
	uint32 result;

	result = 0;
	if (dfs->chan_blocked[channel]) {
		int minutes;

		result |= WL_CHAN_INACTIVE;

		/* Store remaining minutes until channel comes
		 * in-service in high 8 bits.
		 */
		minutes = ROUNDUP(dfs->chan_blocked[channel], 60) / 60;
		result |= ((minutes & 0xff) << 24);
	}

	return (result);
}

#ifdef EXT_STA
static wlc_bsscfg_t*
wlc_get_ap_bsscfg(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *bsscfg = NULL;
	int i;

	if (AP_ACTIVE(wlc)) {
		for (i = 0; i < 2; i++) {
			if (wlc->bsscfg[i] && wlc->bsscfg[i]->_ap && wlc->bsscfg[i]->up) {
				bsscfg = wlc->bsscfg[i];
				/* one ap supported in Win7 */
				break;
			}
		}
	}

	ASSERT(bsscfg);
	return bsscfg;
}
#endif /* EXT_STA */

/*
 * Helper function to use correct pre- and post-ISM CAC time for european weather radar channels
 * which use a different CAC timer (default is 10 minutes for EU weather radar channels, 1 minute
 * for regular radar CAC).
 *
 * Returns cactime in WLC_DFS_RADAR_CHECK_INTERVAL units.
 */
static uint
wlc_dfs_ism_cactime(wlc_info_t *wlc, int secs_or_default)
{
	chanspec_t chspec = WLC_BAND_PI_RADIO_CHANSPEC;
	if (wlc_is_edcrs_eu(wlc) && !wlc_quiet_chanspec(wlc->cmi, chspec)) {
		WL_DFS(("wl%d: Skip CAC - channel 0x%x is already available. Zero duration.\n",
			wlc->pub->unit, chspec));
		return 0; /* zero CAC duration; no need of CAC */
	}

	if (secs_or_default == WLC_DFS_CAC_TIME_USE_DEFAULTS)
	{
		if (wlc_is_european_weather_radar_channel(wlc, WLC_BAND_PI_RADIO_CHANSPEC)) {

			secs_or_default = WLC_DFS_CAC_TIME_SEC_DEF_EUWR;

			WL_DFS(("wl%d: dfs chanspec %04x is european weather radar\n",
				wlc->pub->unit, WLC_BAND_PI_RADIO_CHANSPEC));
		}
		else {
			secs_or_default = WLC_DFS_CAC_TIME_SEC_DEFAULT;
		}
	}

	WL_DFS(("wl%d: dfs chanspec %04x is a radar channel, using %d second CAC time\n",
		wlc->pub->unit, WLC_BAND_PI_RADIO_CHANSPEC, secs_or_default));

	return (secs_or_default*1000)/WLC_DFS_RADAR_CHECK_INTERVAL;

}

static int
BCMATTACHFN(wlc_dfs_timer_init)(wlc_dfs_info_t *dfs)
{
	wlc_info_t* wlc = dfs->wlc;

	dfs->dfs_cac.ism_monitor = FALSE; /* put it to normal mode */

	dfs->dfs_cac.timer_running = FALSE;

	if (!(dfs->dfs_timer = wl_init_timer(wlc->wl, wlc_dfs_cacstate_handler, dfs, "dfs"))) {
		WL_ERROR(("wl%d: wlc_dfs_timer_init failed\n", wlc->pub->unit));
		return -1;
	}
	dfs->dfs_cac.cactime_pre_ism = dfs->dfs_cac.cactime_post_ism
		= WLC_DFS_CAC_TIME_USE_DEFAULTS;   /* use default values */

	dfs->dfs_cac.nop_sec = WLC_DFS_NOP_SEC_DEFAULT;

	return 0;
}

static void
wlc_dfs_timer_add(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;

	if (dfs->dfs_cac.timer_running == FALSE) {
		dfs->dfs_cac.timer_running = TRUE;
		wl_add_timer(wlc->wl, dfs->dfs_timer, WLC_DFS_RADAR_CHECK_INTERVAL, TRUE);
	}
}

static bool
wlc_dfs_timer_delete(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	bool canceled = TRUE;

	if (dfs->dfs_cac.timer_running == TRUE) {
		if (dfs->dfs_timer != NULL) {
			canceled = wl_del_timer(wlc->wl, dfs->dfs_timer);
			ASSERT(canceled == TRUE);
		}
		dfs->dfs_cac.timer_running = FALSE;
	}
	return canceled;
}

static void
wlc_dfs_chanspec_oos(wlc_dfs_info_t *dfs, chanspec_t chanspec)
{
	wlc_info_t* wlc = dfs->wlc;
	bool is_block = FALSE;
	uint8 ctrl_ch, ext_ch;

	if (VHT_ENAB(wlc->pub) && CHSPEC_IS80(chanspec)) {
		uint channel;
		int i;
		channel = LOWER_40_SB(CHSPEC_CHANNEL(chanspec));
		channel = LOWER_20_SB(channel);

		/* work through each 20MHz channel in the 80MHz */
		for (i = 0; i < 4; i++, channel += CH_20MHZ_APART) {
			dfs->chan_blocked[channel] = dfs->dfs_cac.nop_sec;
			WL_DFS(("wl%d: dfs : channel %d put out of service\n", wlc->pub->unit,
				channel));
		}
		ctrl_ch = CHSPEC_CHANNEL(chanspec);
		ext_ch = 0;
	} else if (N_ENAB(wlc->pub) && CHSPEC_IS40(chanspec)) {
		ctrl_ch = LOWER_20_SB(CHSPEC_CHANNEL(chanspec));
		ext_ch = UPPER_20_SB(CHSPEC_CHANNEL(chanspec));
		dfs->chan_blocked[ctrl_ch] = dfs->dfs_cac.nop_sec;
		dfs->chan_blocked[ext_ch] = dfs->dfs_cac.nop_sec;

		WL_DFS(("wl%d: dfs : channel %d & %d put out of service\n", wlc->pub->unit,
			ctrl_ch, ext_ch));
	} else {
		ctrl_ch = CHSPEC_CHANNEL(chanspec);
		ext_ch = 0;
		dfs->chan_blocked[ctrl_ch] = dfs->dfs_cac.nop_sec;

		WL_DFS(("wl%d: dfs : channel %d put out of service\n", wlc->pub->unit, ctrl_ch));
	}

	wlc_set_quiet_chanspec(wlc->cmi, chanspec);

	if (!bcmp("US", wlc_channel_country_abbrev(wlc->cmi), 2) ||
		!bcmp("CA", wlc_channel_country_abbrev(wlc->cmi), 2)) {
		if ((ctrl_ch >= 120 && ctrl_ch <= 128) ||
		   (N_ENAB(wlc->pub) && CHSPEC_IS40(chanspec) && ext_ch >= 120 && ext_ch <= 128))
			is_block = TRUE;
	}

	/* Special US and CA handling, remove set of channels 120, 124, 128 if
	 * any get a radar pulse.  For CA they will be blocked for uptime of the driver.
	 */
	if (is_block) {
		uint32  block_time = !bcmp("CA", wlc_channel_country_abbrev(wlc->cmi), 2) ?
		    WLC_CHANBLOCK_FOREVER : dfs->dfs_cac.nop_sec;

		wlc_set_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(120));
		dfs->chan_blocked[120] = block_time;
		wlc_set_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(124));
		dfs->chan_blocked[124] = block_time;
		wlc_set_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(128));
		dfs->chan_blocked[128] = block_time;
	}
}

/*
 * Random channel selection for DFS
 * Returns a valid chanspec of a valid radar free channel, using the AP configuration
 * to choose 20, 40 or 80 MHz bandwidth and side-band
 * Returns 0 if there are no valid radar free channels available
 */
static chanspec_t
wlc_dfs_chanspec(wlc_dfs_info_t *dfs, bool radar_detected)
{
	wlc_info_t *wlc = dfs->wlc;
	chanvec_t channels20, channels40, channels80, *chanvec;
	chanspec_t chspec;
	uint chan20_cnt, chan40_cnt, chan80_cnt;
	uint chan, rand_idx, rand_channel;
#if defined(BCMDBG)
	char chanbuf[CHANSPEC_STR_LEN];
#endif // endif
	uint i, pref_cnt = 0;
	chanspec_t pref_chspecs[WL_NUMCHANNELS];

	chan20_cnt = chan40_cnt = chan80_cnt = 0;
	bzero(channels20.vec, sizeof(channels20.vec));
	bzero(channels40.vec, sizeof(channels40.vec));
	bzero(channels80.vec, sizeof(channels80.vec));

	if (dfs->dfs_cac.chanspec_forced_list) {
		/* dfs->dfs_cac.chanspec_forced_list->num should be
		 * less that WL_NUMCHANNELS. Validated in iovar path
		 */
		for (i = 0; i < dfs->dfs_cac.chanspec_forced_list->num; i++) {
			chspec = dfs->dfs_cac.chanspec_forced_list->list[i];
			if (wlc_valid_ap_chanspec(dfs, chspec)) {
				pref_chspecs[pref_cnt++] = chspec;
			}
		}

		if (pref_cnt) {
			rand_idx = R_REG(wlc->osh, &wlc->regs->u.d11regs.tsf_random);
			rand_idx = rand_idx % pref_cnt;
			chspec = pref_chspecs[rand_idx];
		} else {
			chspec = 0;
		}

		/* return if suitable channel is in forced list */
		if (chspec != 0) {
			return chspec;
		} else {
			WL_DFS(("no usable channels found in dfs_channel_forced list\n"));
		}
	}
	/* walk the channels looking for good 20MHz channels */
	/* When Radar is detected, look for Non-DFS channels */
	for (chan = 0; chan < MAXCHANNEL; chan++) {
		chspec = CH20MHZ_CHSPEC(chan);
		if (wlc_valid_ap_chanspec(dfs, chspec)) {
			if (wlc_radar_chanspec(wlc->cmi, chspec) && radar_detected) {
				continue;
			} else {
				setbit(channels20.vec, chan);
				chan20_cnt++;
			}
		}
	}

	/* check for 40MHz channels only if we are capable of 40MHz, the default
	 * bss was configured for 40MHz, and the locale allows 40MHz
	 */
	if (N_ENAB(wlc->pub) &&
	    CHSPEC_IS40(wlc->default_bss->chanspec) &&
	    (WL_BW_CAP_40MHZ(wlc->band->bw_cap)) &&
	    !(wlc_channel_locale_flags(wlc->cmi) & WLC_NO_40MHZ)) {
		/* walk the channels looking for good 40MHz channels */
		for (chan = 0; chan < MAXCHANNEL; chan++) {
			chspec = CH40MHZ_CHSPEC(chan, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
			if (wlc_valid_ap_chanspec(dfs, chspec)) {
				if (wlc_radar_chanspec(wlc->cmi, chspec) && radar_detected) {
					continue;
				} else {
					setbit(channels40.vec, chan);
					chan40_cnt++;
				}
			}
		}
	}

	/* check for 80MHz channels only if we are capable of 80MHz, the default
	 * bss was configured for 80MHz, and the locale allows 80MHz
	 */
	if (CHSPEC_IS80(wlc->default_bss->chanspec) &&
	    VALID_80CHANSPEC(wlc, wlc->default_bss->chanspec)) {
		/* walk the channels looking for good 80MHz channels */
		for (chan = 0; chan < MAXCHANNEL; chan++) {
			chspec = CH80MHZ_CHSPEC(chan, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
			if (wlc_valid_ap_chanspec(dfs, chspec)) {
				if (wlc_radar_chanspec(wlc->cmi, chspec) && radar_detected) {
					continue;
				} else {
					setbit(channels80.vec, chan);
					chan80_cnt++;
				}
			}
		}
	}

	if (!chan20_cnt) {
		/* no channel found */
		return 0;
	}

	rand_idx = R_REG(wlc->osh, &wlc->regs->u.d11regs.tsf_random);

	if (chan80_cnt) {
		rand_idx = rand_idx % chan80_cnt;
		chanvec = &channels80;
	} else if (chan40_cnt) {
		rand_idx = rand_idx % chan40_cnt;
		chanvec = &channels40;
	} else {
		rand_idx = rand_idx % chan20_cnt;
		chanvec = &channels20;
	}

	/* choose 'rand_idx'th channel */
	for (rand_channel = 0, chan = 0; chan < MAXCHANNEL; chan++) {
		if (isset(chanvec->vec, chan)) {
			if (rand_idx == 0) {
				rand_channel = chan;
				break;
			}
			rand_idx--;
		}
	}

	ASSERT(rand_channel);

	if (chan80_cnt)
		chspec = CH80MHZ_CHSPEC(rand_channel, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
	else if (chan40_cnt)
		chspec = CH40MHZ_CHSPEC(rand_channel, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
	else
		chspec = CH20MHZ_CHSPEC(rand_channel);

	ASSERT(wlc_valid_chanspec_db(wlc->cmi, chspec));

#if defined(BCMDBG)
	WL_DFS(("wl%d: %s: dfs selected chanspec %s (%04x)\n", wlc->pub->unit, __FUNCTION__,
	        wf_chspec_ntoa(chspec, chanbuf), chspec));
#endif // endif

	return chspec;
}

/* check for a chanspec on which an AP can set up a BSS
 * Returns TRUE if the chanspec is valid for the local, not restricted, and
 * has not been blocked by a recent radar pulse detection.
 * Otherwise will return FALSE.
 */
static bool
wlc_valid_ap_chanspec(wlc_dfs_info_t *dfs, chanspec_t chspec)
{
	uint channel = CHSPEC_CHANNEL(chspec);
	wlc_info_t *wlc = dfs->wlc;

	if (!wlc_valid_chanspec_db(wlc->cmi, chspec) ||
	    wlc_restricted_chanspec(wlc->cmi, chspec))
		return FALSE;

	if (CHSPEC_IS80(chspec)) {
		if (dfs->chan_blocked[LL_20_SB(channel)] ||
		    dfs->chan_blocked[UU_20_SB(channel)] ||
			dfs->chan_blocked[LU_20_SB(channel)] ||
		    dfs->chan_blocked[UL_20_SB(channel)])
			return FALSE;
	} else if (CHSPEC_IS40(chspec)) {
		if (dfs->chan_blocked[LOWER_20_SB(channel)] ||
		    dfs->chan_blocked[UPPER_20_SB(channel)])
			return FALSE;
	} else if (dfs->chan_blocked[channel]) {
		return FALSE;
	}

	return TRUE;
}

static bool
wlc_radar_detected(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	int radar_type;
	int	radar_interval;
	int	min_pw;
#if defined(BCMDBG) || defined(WLMSG_DFS)
	uint i;
	char radar_type_str[24];
	char chanbuf[CHANSPEC_STR_LEN];
	static const struct {
		int radar_type;
		const char *radar_type_name;
	} radar_names[] = {
		{RADAR_TYPE_NONE, "NONE"},
		{RADAR_TYPE_ETSI_1, "ETSI_1"},
		{RADAR_TYPE_ETSI_2, "ETSI_2"},
		{RADAR_TYPE_ETSI_3, "ETSI_3"},
		{RADAR_TYPE_ETSI_4, "ETSI_4"},
		{RADAR_TYPE_STG2, "S2"},
		{RADAR_TYPE_STG3, "S3"},
		{RADAR_TYPE_UNCLASSIFIED, "UNCLASSIFIED"},
		{RADAR_TYPE_FCC_5, "FCC-5"},
		{RADAR_TYPE_JP1_2_JP2_3, "JP1-2/JP2-3"},
		{RADAR_TYPE_JP2_1, "JP2-1"},
		{RADAR_TYPE_JP4, "JP4"}
	};
#endif /* BCMDBG || WLMSG_DFS */
	wlc_bsscfg_t *cfg = wlc->cfg;

	(void)cfg;

	radar_type = wlc_phy_radar_detect_run(wlc->band->pi);
	radar_interval = radar_type >> 14;
	BCM_REFERENCE(radar_interval);
	min_pw = radar_type >> 4 & 0x1ff;
	BCM_REFERENCE(min_pw);
	radar_type = radar_type & 0xf;
	/* Pretend we saw radar - for testing */
	if ((wlc_11h_get_spect_state(wlc->m11h, cfg) & RADAR_SIM) ||
	    radar_type != RADAR_TYPE_NONE) {

#if defined(BCMDBG) || defined(WLMSG_DFS)
		snprintf(radar_type_str, sizeof(radar_type_str),
			"%s", "UNKNOWN");
		for (i = 0; i < ARRAYSIZE(radar_names); i++) {
			if (radar_names[i].radar_type == radar_type)
				snprintf(radar_type_str, sizeof(radar_type_str),
					"%s", radar_names[i].radar_type_name);
		}

		WL_DFS(("WL%d: DFS: %s ########## RADAR DETECTED ON CHANNEL %s"
			" ########## Intv=%d, min_pw=%d, AT %dMS\n", wlc->pub->unit,
			radar_type_str,
			wf_chspec_ntoa(WLC_BAND_PI_RADIO_CHANSPEC, chanbuf),
			radar_interval, min_pw,
			(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
			WLC_DFS_RADAR_CHECK_INTERVAL));
#endif /* BCMDBG || WLMSG_DFS */

		/* clear one-shot radar simulator */
		wlc_11h_set_spect_state(wlc->m11h, cfg, RADAR_SIM, 0);
		return TRUE;
	} else
		return FALSE;
}

/* set cacstate to IDLE and un-mute */
static void
wlc_dfs_cacstate_idle_set(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;

	wlc_dfs_cac_state_change(dfs, WL_DFS_CACSTATE_IDLE);
	wlc_mute(wlc, OFF, PHY_MUTE_FOR_PREISM);

	WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC),
		(dfs->dfs_cac.cactime -
		dfs->dfs_cac.duration)*WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.cactime =  /* unit in WLC_DFS_RADAR_CHECK_INTERVAL */
	dfs->dfs_cac.duration = wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);
}

/* set cacstate to ISM and un-mute */
static void
wlc_dfs_cacstate_ism_set(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	int  cal_mode;

	dfs->dfs_cac.status.chanspec_cleared = WLC_BAND_PI_RADIO_CHANSPEC;
	 /* clear the channel */
	wlc_clr_quiet_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared);

	wlc_dfs_cac_state_change(dfs, WL_DFS_CACSTATE_ISM);
	wlc_mute(wlc, OFF, PHY_MUTE_FOR_PREISM);

	wlc_iovar_getint(wlc, "phy_percal", (int *)&cal_mode);
	wlc_iovar_setint(wlc, "phy_percal", PHY_PERICAL_SPHASE);
	wlc_phy_cal_perical(wlc->band->pi, PHY_PERICAL_UP_BSS);
	wlc_iovar_setint(wlc, "phy_percal", cal_mode);

	WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC),
		(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) * WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.cactime =  /* unit in WLC_DFS_RADAR_CHECK_INTERVAL */
	dfs->dfs_cac.duration = wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wlc->pub)) {
		wlc_bsscfg_t *bsscfg;
		bsscfg = wlc_get_ap_bsscfg(dfs);
		if (bsscfg) {
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_DFS_AP_RESUME, NULL,
			                  WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);
		}
	}
#endif /* EXT_STA */
}

/* set cacstate to OOC and mute */
static void
wlc_dfs_cacstate_ooc_set(wlc_dfs_info_t *dfs, uint target_state)
{
	wlc_info_t *wlc = dfs->wlc;

	wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);

	wlc_dfs_cac_state_change(dfs, target_state);

	WL_DFS(("wl%d: dfs : state to %s at %dms\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
	        WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.duration = dfs->dfs_cac.cactime; /* reset it */

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wlc->pub)) {
		wlc_bsscfg_t *bsscfg;
		bsscfg = wlc_get_ap_bsscfg(dfs);
		if (bsscfg) {
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_DFS_AP_STOP, NULL,
			                  WLC_E_STATUS_NOCHANS, 0, 0, 0, 0);
		}
	}
#endif /* EXT_STA */
}

static void
wlc_dfs_cacstate_idle(wlc_dfs_info_t *dfs)
{
	wlc_dfs_timer_delete(dfs);
	dfs->dfs_cac_enabled = FALSE;
}

static void
wlc_dfs_cacstate_cac(wlc_dfs_info_t *dfs)
{
	wlc_info_t* wlc = dfs->wlc;
	uint target_state;
	wlc_bsscfg_t *cfg = wlc->cfg;

	(void)cfg;

	if (wlc_radar_detected(dfs) == TRUE) {
		wlc_dfs_chanspec_oos(dfs, WLC_BAND_PI_RADIO_CHANSPEC);

		if (!(dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs, TRUE))) {
			/* out of channels */
			if (dfs->dfs_cac.status.state == WL_DFS_CACSTATE_PREISM_CAC) {
				target_state = WL_DFS_CACSTATE_PREISM_OOC;
			} else {
				target_state = WL_DFS_CACSTATE_POSTISM_OOC;
			}
			wlc_dfs_cacstate_ooc_set(dfs, target_state);
			return;
		}

		wlc_do_chanswitch(cfg, dfs->dfs_cac.chanspec_next);

		if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
			/* do cac with new channel */
			WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
				wlc->pub->unit,
				wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
				CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC),
				(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
			        WLC_DFS_RADAR_CHECK_INTERVAL));
			/* Switched to new channel, set up correct pre-ISM timer */
			dfs->dfs_cac.duration =
			dfs->dfs_cac.cactime =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_pre_ism);
			return;
		}
		else {
			wlc_dfs_cacstate_idle_set(dfs); /* set to IDLE */
			return;
		}
	}

	if (!dfs->dfs_cac.duration) {
		/* cac completed. un-mute all. resume normal bss operation */
		wlc_dfs_cacstate_ism_set(dfs);
	}
}

static void
wlc_dfs_cacstate_ism(wlc_dfs_info_t *dfs)
{
	wlc_info_t* wlc = dfs->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg, *apcfg;
	wl_chan_switch_t csa;
	int idx;

	if (wlc_radar_detected(dfs) == FALSE)
		return;

	/* Ignore radar_detect, if STA conencted to upstream AP on radar channel
	 * and local AP is on same radar channel.
	 */
	if (WLC_APSTA_ON_RADAR_CHANNEL(wlc)) {
		WL_DFS(("wl%d: dfs : radar detected but ignoring,"
			"dfs slave present\n", wlc->pub->unit));
		return;
	}

	/* radar has been detected */

	if (dfs->dfs_cac.ism_monitor == TRUE) {
		/* channel switching is disabled */
		WL_DFS(("wl%d: dfs : current channel %d is maintained as channel switching is"
		        " disabled.\n",
		        wlc->pub->unit, CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC)));
		return;
	}

	/* radar detected. mark the channel back to QUIET channel */
	wlc_set_quiet_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared);
	dfs->dfs_cac.status.chanspec_cleared = 0; /* cleare it */

	/* continue with CSA */
	wlc_dfs_chanspec_oos(dfs, WLC_BAND_PI_RADIO_CHANSPEC);
	dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs, TRUE); /* it will be included in csa */

	/* send csa */
	if (!dfs->dfs_cac.chanspec_next) {
	        /* out of channels */
	        /* just use the current channel for csa */
	        csa.chspec = WLC_BAND_PI_RADIO_CHANSPEC;
	} else {
	        csa.chspec = dfs->dfs_cac.chanspec_next;
	}
	csa.mode = DOT11_CSA_MODE_NO_TX;
	csa.count = MAX((WLC_DFS_CSA_MSEC/cfg->current_bss->beacon_period), WLC_DFS_CSA_BEACONS);
	csa.reg = wlc_get_regclass(wlc->cmi, csa.chspec);
	csa.frame_type = CSA_BROADCAST_ACTION_FRAME;
	FOREACH_UP_AP(wlc, idx, apcfg) {
		wlc_csa_do_csa(wlc->csa, apcfg, &csa, FALSE);
	}

	wlc_dfs_cac_state_change(dfs, WL_DFS_CACSTATE_CSA);        /* next state */

	WL_DFS(("wl%d: dfs : state to %s channel current %d next %d at %dms, starting CSA"
		" process\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC), CHSPEC_CHANNEL(csa.chspec),
		(dfs->dfs_cac.cactime -
			dfs->dfs_cac.duration)*WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.duration = dfs->dfs_cac.cactime =
		wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);
}

/* csa transmission */
static void
wlc_dfs_cacstate_csa(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg;

	if ((wlc_11h_get_spect_state(wlc->m11h, cfg) &
	     (NEED_TO_SWITCH_CHANNEL | NEED_TO_UPDATE_BCN)) ||
	    (wlc->block_datafifo & DATA_BLOCK_QUIET))
	        return;

	/* csa completed - TBTT dpc switched channel */

	if (!(dfs->dfs_cac.chanspec_next)) {
	        /* ran out of channels, goto OOC */
	        wlc_dfs_cacstate_ooc_set(dfs, WL_DFS_CACSTATE_POSTISM_OOC);
		return;
	}

	if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
		if (dfs->dfs_cac.cactime_post_ism) {
			wlc_dfs_cac_state_change(dfs, WL_DFS_CACSTATE_POSTISM_CAC);
			WL_DFS(("wl%d: dfs : state to %s at %dms\n",
				wlc->pub->unit,
				wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
			        (dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
			        WLC_DFS_RADAR_CHECK_INTERVAL));

			dfs->dfs_cac.duration =
			dfs->dfs_cac.cactime =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);

			wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);
		}
		else {
			wlc_dfs_cacstate_ism_set(dfs);
		}
	}
	else {
		wlc_dfs_cacstate_idle_set(dfs);
	}

	wlc_update_beacon(wlc);
	wlc_update_probe_resp(wlc, TRUE);
}

/*
 * dfs has run Out Of Channel.
 * wait for a channel to come out of Non-Occupancy Period.
 */
static void
wlc_dfs_cacstate_ooc(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	uint    current_time;

	if (!(dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs, TRUE))) {
		/* still no channel out of channels. Nothing to do */
		return;
	}

	wlc_do_chanswitch(wlc->cfg, dfs->dfs_cac.chanspec_next);

	if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
		current_time = (dfs->dfs_cac.cactime -
			dfs->dfs_cac.duration)*WLC_DFS_RADAR_CHECK_INTERVAL;
		BCM_REFERENCE(current_time);

		/* unit of cactime is WLC_DFS_RADAR_CHECK_INTERVAL */
		if (dfs->dfs_cac.status.state == WL_DFS_CACSTATE_PREISM_OOC) {
			dfs->dfs_cac.cactime = dfs->dfs_cac.duration =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_pre_ism);
			wlc_dfs_cac_state_change(dfs, WL_DFS_CACSTATE_PREISM_CAC);
		} else {
			dfs->dfs_cac.cactime = dfs->dfs_cac.duration =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);
			wlc_dfs_cac_state_change(dfs, WL_DFS_CACSTATE_POSTISM_CAC);
		}

		if (dfs->dfs_cac.cactime) {
			wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);

			WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
				wlc->pub->unit,
				wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
				CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC), current_time));
		} else {
			/* corresponding cac is disabled */
			wlc_dfs_cacstate_ism_set(dfs);
		}
	} else {
		wlc_dfs_cacstate_idle_set(dfs); /* set to idle */
	}
}

static void
wlc_dfs_cacstate_handler(void *arg)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)arg;
	wlc_info_t *wlc = dfs->wlc;

	if (!wlc->pub->up || !dfs->dfs_cac_enabled)
		return;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

#ifdef EXT_STA
	if (EXTSTA_ENAB(wlc->pub) && SCAN_IN_PROGRESS(wlc->scan)) {
		return;
	}
#endif // endif

	ASSERT(dfs->dfs_cac.status.state < WL_DFS_CACSTATES);

	wlc_dfs_cacstate_fn_ary[dfs->dfs_cac.status.state](dfs);

	dfs->dfs_cac.duration--;
}

static void
wlc_dfs_cacstate_init(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg;

	ASSERT(WL11H_AP_ENAB(wlc));

	if (!wlc->pub->up)
		return;

	if (wlc_radar_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared) == TRUE) {
		/* restore QUIET setting unless in EU */
		if (!wlc_is_edcrs_eu(wlc)) {
			wlc_set_quiet_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared);
		}
	}
	dfs->dfs_cac.status.chanspec_cleared = 0; /* clear it */

	wlc_csa_reset_all(wlc->csa, cfg);
	wlc_quiet_reset_all(wlc->quiet, cfg);

	if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
		dfs->dfs_cac_enabled = TRUE;
		wlc_dfs_timer_add(dfs);

		/* unit of cactime is WLC_DFS_RADAR_CHECK_INTERVAL */
		dfs->dfs_cac.cactime = wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_pre_ism);
		if (!WLC_APSTA_ON_RADAR_CHANNEL(wlc) && dfs->dfs_cac.cactime) {
			/* preism cac is enabled */
			wlc_dfs_cac_state_change(dfs, WL_DFS_CACSTATE_PREISM_CAC);
			dfs->dfs_cac.duration = dfs->dfs_cac.cactime;
			wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);
		} else {
			/* preism cac is disabled */
			wlc_dfs_cacstate_ism_set(dfs);
		}

		wlc_radar_detected(dfs); /* refresh detector */

	} else {
		wlc_dfs_cacstate_idle_set(dfs); /* set to idle */
	}

	WL_REGULATORY(("wl%d: %s: state to %s channel %d \n",
		wlc->pub->unit, __FUNCTION__,
		wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC)));
}

void
wlc_set_dfs_cacstate(wlc_dfs_info_t *dfs, int state)
{
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	if (!AP_ENAB(wlc->pub))
		return;

	WL_REGULATORY(("wl%d: %s dfs from %s to %s\n",
		wlc->pub->unit, __FUNCTION__, dfs->dfs_cac_enabled ? "ON":"OFF",
		state ? "ON":"OFF"));

	if (state == OFF) {
		if (dfs->dfs_cac_enabled) {
			wlc_dfs_cacstate_idle_set(dfs);
			wlc_dfs_cacstate_idle(dfs);
		}
	} else {
		wlc_dfs_cacstate_init(dfs);
	}
}

chanspec_t
wlc_dfs_sel_chspec(wlc_dfs_info_t *dfs, bool force)
{
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	if (!force && dfs->dfs_cac.chanspec_next != 0)
		return dfs->dfs_cac.chanspec_next;

	dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs, FALSE);

	WL_REGULATORY(("wl%d: %s: dfs selected channel %d\n",
	               wlc->pub->unit, __FUNCTION__,
	               CHSPEC_CHANNEL(dfs->dfs_cac.chanspec_next)));

	return dfs->dfs_cac.chanspec_next;
}

void
wlc_dfs_reset_all(wlc_dfs_info_t *dfs)
{
	bzero(dfs->chan_blocked, sizeof(dfs->chan_blocked));
}

int
wlc_dfs_set_radar(wlc_dfs_info_t *dfs, int radar)
{
	wlc_info_t *wlc = dfs->wlc;
	wlcband_t *band5G;

	if (radar < 0 || radar > (int)WL_RADAR_SIMULATED) {
		return BCME_RANGE;
	}

	/*
	 * WL_RADAR_SIMULATED is required for Wi-Fi testing.
	 */

	/* Radar must be enabled to pull test trigger */
	if (radar == (int)WL_RADAR_SIMULATED) {
		wlc_bsscfg_t *cfg = wlc->cfg;

		if (dfs->radar != 1) {
			return BCME_BADARG;
		}

		/* Can't do radar detect on non-radar channel */
		if (wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec) != TRUE) {
			return BCME_BADCHAN;
		}

		wlc_11h_set_spect_state(wlc->m11h, cfg, RADAR_SIM, RADAR_SIM);
		return BCME_OK;
	}

	if ((int)dfs->radar == radar) {
		return BCME_OK;
	}

	/* Check there is a 5G band available */
	if (BAND_5G(wlc->band->bandtype)) {
		band5G = wlc->band;
	} else if (NBANDS(wlc) > 1 &&
	           BAND_5G(wlc->bandstate[OTHERBANDUNIT(wlc)]->bandtype)) {
		band5G = wlc->bandstate[OTHERBANDUNIT(wlc)];
	} else {
		return BCME_BADBAND;
	}

	/* bcmerror if APhy rev 3+ support in any bandunit */
	if (WLCISAPHY(band5G) && AREV_LT(band5G->phyrev, 3)) {
		return BCME_UNSUPPORTED;
	}

	dfs->radar = (uint32)radar;

	wlc_phy_radar_detect_enable(wlc->band->pi, radar != 0);

	/* if we are not currently on the APhy, then radar detect
	 * will be initialized in the phy init
	 */

	return BCME_OK;
}

uint32
wlc_dfs_get_radar(wlc_dfs_info_t *dfs)
{
	return dfs->radar;
}

#ifdef STA
#ifdef BCMDBG
static int
wlc_dfs_bcn_parse_ibss_dfs_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	return BCME_OK;
}
#endif /* BCMDBG */
#endif /* STA */
#endif /* WLDFS */
