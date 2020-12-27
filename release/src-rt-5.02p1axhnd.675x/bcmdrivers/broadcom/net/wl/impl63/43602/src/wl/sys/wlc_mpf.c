/*
 * Motion ProFiles for WLAN functionality (initially PNO).
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

#include <wlc_cfg.h>

#ifdef WL_MPF

#include <typedefs.h>
#include <osl.h>
#include <wl_dbg.h>
#include <event_log.h>

#include <bcmutils.h>
#include <siutils.h>

#include <wlioctl.h>
#include <wlc_pub.h>
#include <wl_export.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc_pub.h>
#include <wlc.h>

#include <wlc_mpf.h>

/* Client and GPIO state structures maintained by the module */
typedef struct wlc_mpf_client {
	struct wlc_mpf_client *next;
	mpf_callback_t notify_fn;
	void *handle;
	uintptr subhandle;
	uint32 mode_flags;
} wlc_mpf_client_t;

typedef struct wlc_mpf_config {
	uint16 mask;
	uint8 count;
	uint8 override;
	uint16 prev_bits;
	uint16 prev_state;
	uint16 override_state;
	wlc_mpf_client_t *clients;
	wl_mpf_val_t vals[WL_MPF_MAX_STATES]; /* contains val, state, name */
} wlc_mpf_config_t;

#define MPF_STATE_TIMER_ACTIVE 0x1
struct wlc_mpf_info {
	wlc_info_t *wlc;
	struct wl_timer *mpf_timer;
	uint16 prev_bits;
	uint16 totalmask;		/* OR of all configs */
	uint8  nconfig;			/* Number of configs */
	uint8  state_info;		/* Various flags */
	uint8  debounce;		/* Debounce time */
	uint8  pollprint;		/* To limit poll logging */
	uint8  pollprintcnt;		/* Counter for poll logging */
	uint8  active_configs;		/* Configs with map and client */
	bool   driver_up;		/* Reflect driver up state */
	wlc_mpf_config_t *config;	/* Array of config structs */
};

/* IOVar table */
enum {
	IOV_MPF_MAP,
	IOV_MPF_STATE,
	IOV_MPF_DEBOUNCE,
	IOV_MPF_POLLPRINT
};

static const bcm_iovar_t
wlc_mpf_iovars[] = {
	{"mpf_map", IOV_MPF_MAP,
	0, IOVT_BUFFER, sizeof(wl_mpf_map_t),
	},
	{"mpf_state", IOV_MPF_STATE,
	0, IOVT_BUFFER, sizeof(wl_mpf_state_t),
	},
	{"mpf_debounce", IOV_MPF_DEBOUNCE,
	0, IOVT_UINT8, 0
	},
#ifdef EVENT_LOG_COMPILE
	{"mpf_pollprint", IOV_MPF_POLLPRINT,
	0, IOVT_UINT8, 0
	},
#endif /* EVENT_LOG_COMPILE */
	{NULL, 0, 0, 0, 0 }
};

/* ---------------------------------- */
/* IOVar functions for state handling */
/* ---------------------------------- */

static int
wlc_get_mpf_map(wlc_mpf_info_t *mpf, void *arg, int len, void *params, uint p_len)
{
	wlc_mpf_config_t *config;
	wl_mpf_map_t iomap;

	if (p_len < OFFSETOF(wl_mpf_map_t, mask))
		return BCME_BUFTOOSHORT;

	memset(&iomap, 0, sizeof(wl_mpf_map_t));
	memcpy(&iomap, params, OFFSETOF(wl_mpf_map_t, mask));

	if (iomap.version != WL_MPF_VERSION)
		return BCME_VERSION;

	if (iomap.type >= mpf->nconfig)
		return BCME_BADARG;

	config = mpf->config + iomap.type;

	iomap.mask = config->mask;
	iomap.count = config->count;
	memcpy(iomap.vals, config->vals, sizeof(wl_mpf_map_t) - OFFSETOF(wl_mpf_map_t, vals));

	/* Copy into actual return buffer */
	memcpy(arg, &iomap, sizeof(wl_mpf_map_t));
	return BCME_OK;
}

static int
wlc_set_mpf_map(wlc_mpf_info_t *mpf, void *arg, int len)
{
	wlc_mpf_config_t *config;
	struct {
		wl_mpf_map_t input;
		char term;
	} iomap;
	wl_mpf_map_t *iomapp;
	wl_mpf_val_t *valp, *valp2;
	uint8 count;
	bool timer_needed;

	iomapp = &iomap.input;
	memcpy(iomapp, arg, sizeof(wl_mpf_map_t));
	iomap.term = '\0';

	/* Sanity checks and argument validation */

	if (iomapp->version != WL_MPF_VERSION)
		return BCME_VERSION;

	if (iomapp->type >= mpf->nconfig)
		return BCME_BADARG;

	if (bcm_bitcount((uint8*)&iomapp->mask, sizeof(iomapp->mask)) > WL_MPF_MAX_BITS)
		return BCME_BADARG;

	count = (1 << bcm_bitcount((uint8*)&iomapp->mask, sizeof(iomapp->mask)));

	if (iomapp->count != count) {
		WL_MPF_DEBUG(("wlc_set_mpf_map: bad count %d != %d for mask %04x\n",
		              iomapp->count, count, iomapp->mask));
		return BCME_BADARG;
	}

	for (valp = iomapp->vals; count; count--, valp++) {
		if (valp->val & ~iomapp->mask) {
			WL_MPF_DEBUG(("wlc_set_mpf_map: val %04x has bits outside mask %04x\n",
			              valp->val, iomapp->mask));
			return BCME_BADARG;
		}
		for (valp2 = iomapp->vals; valp2 < valp; valp2++) {
			if (valp->val == valp2->val) {
				WL_MPF_DEBUG(("wlc_set_mpf_map: val %04x duplicated\n",
				              valp->val));
				WL_MPF_DEBUG(("val %d(%08x) and val %d(%08x)\n",
				              (valp - iomapp->vals), (uintptr)valp,
				              (valp2 - iomapp->vals), (uintptr)valp2));
				return BCME_BADARG;
			}
		}
		if (valp->state >= iomapp->count) {
			WL_MPF_DEBUG(("wlc_set_mpf_map: state %d > %d\n",
			              valp->state, iomapp->count));
			return BCME_BADARG;
		}
		if (strlen(valp->name) >= WL_MPF_STATE_NAME_MAX) {
			WL_MPF_DEBUG(("wlc_set_mpf_map: %d-byte name (%02x %02x ...) "
			              "for val %04x too long\n", (int)strlen(valp->name),
			              valp->name[0], valp->name[1], valp->val));
			return BCME_BADARG;
		}
	}

	/* Ok, locate the appropriate config */
	config = mpf->config + iomapp->type;

	/* If this newly (de)activates a config, adjust the count */
	if (config->clients) {
		if (!config->mask && iomapp->mask)
			mpf->active_configs++;
		else if (config->mask && !iomapp->mask)
			mpf->active_configs--;
	}

	/* Copy arg data to the config, adjust the total mask */
	config->mask = iomapp->mask;
	config->count = iomapp->count;
	memcpy(config->vals, iomapp->vals, sizeof(wl_mpf_map_t) - OFFSETOF(wl_mpf_map_t, vals));

	mpf->totalmask = 0;
	for (count = 0, config = mpf->config; count < mpf->nconfig; count++, config++) {
		mpf->totalmask |= config->mask;
	}
	mpf->prev_bits &= mpf->totalmask;

	/* May need to start the MPF timer */
	WL_MPF_DEBUG(("MPF Map: active %d total %d pollprint %d\n",
	              mpf->active_configs, mpf->totalmask, mpf->pollprint));
	timer_needed = mpf->active_configs || (mpf->totalmask && mpf->pollprint);
	if (timer_needed && !mpf->driver_up && !(mpf->state_info & MPF_STATE_TIMER_ACTIVE)) {
		WL_MPF_INFO(("MPF Set Map: starting MPF timer\n"));
		mpf->state_info |= MPF_STATE_TIMER_ACTIVE;
		wl_add_timer(mpf->wlc->wl, mpf->mpf_timer, 1000, TRUE);
	} else if (!timer_needed && (mpf->state_info & MPF_STATE_TIMER_ACTIVE)) {
		WL_MPF_INFO(("MPF Set Map: stopping MPF timer\n"));
		wl_del_timer(mpf->wlc->wl, mpf->mpf_timer);
		mpf->state_info &= ~MPF_STATE_TIMER_ACTIVE;
	}

	return BCME_OK;
}

static int
wlc_get_mpf_state(wlc_mpf_info_t *mpf, void *arg, int len, void *params, uint p_len)
{
	wlc_mpf_config_t *config;
	wl_mpf_state_t mpf_state;
	wl_mpf_val_t *valp;
	uint8 count;

	if (p_len < OFFSETOF(wl_mpf_state_t, state))
		return BCME_BUFTOOSHORT;

	memset(&mpf_state, 0, sizeof(wl_mpf_state_t));
	memcpy(&mpf_state, params, OFFSETOF(wl_mpf_state_t, state));

	if (mpf_state.version != WL_MPF_VERSION)
		return BCME_VERSION;
	if (mpf_state.type >= mpf->nconfig)
		return BCME_BADARG;

	config = mpf->config + mpf_state.type;

	if (config->override) {
		mpf_state.force = 1;
		mpf_state.state = config->override_state;
	} else {
		mpf_state.force = 0;
		mpf_state.state = config->prev_state;
	}

	/* Find the matching state val and copy the name */
	for (count = config->count, valp = config->vals; count; count--, valp++) {
		if (mpf_state.state == valp->state)
			break;
	}
	if (count)
		memcpy(mpf_state.name, valp->name, WL_MPF_STATE_NAME_MAX);

	/* Copy into actual return buffer */
	memcpy(arg, &mpf_state, sizeof(wl_mpf_state_t));

	return BCME_OK;
}

/* Force the state: if there's a name, use it; else use the state */
static int
wlc_set_mpf_state(wlc_mpf_info_t *mpf, void *arg, int len)
{
	wlc_mpf_config_t *config;
	wl_mpf_state_t state;
	wl_mpf_val_t *valp;
	uint8 count;
	char lastc = '\0';
	uint16 old_state, new_state;
	wlc_mpf_client_t *client;

	memcpy(&state, arg, sizeof(wl_mpf_state_t));

	if (state.version != WL_MPF_VERSION)
		return BCME_VERSION;
	if (state.type >= mpf->nconfig)
		return BCME_BADARG;

	lastc = state.name[WL_MPF_STATE_NAME_MAX-1];
	state.name[WL_MPF_STATE_NAME_MAX-1] = '\0';
	if (lastc && (strlen(state.name) == WL_MPF_STATE_NAME_MAX-1))
		return BCME_BADARG;

	/* Find the relevant config */
	config = mpf->config + state.type;

	/* Record old (current) reported state for later notification */
	old_state = (config->override ? config->override_state : config->prev_state);

	if (state.force == 1) {
		/* If forcing, set override and find the name or state */
		for (count = config->count, valp = config->vals; count; count--, valp++) {
			if (state.name[0]) {
				if (strcmp(state.name, valp->name) == 0)
					break;
			} else if (state.state == valp->state)
				break;
		}

		if (count == 0) {
			if (state.name[0]) {
				WL_MPF_DEBUG(("state: no match for name (%02x %02x)\n",
				              state.name[0], state.name[1]));
			} else {
				WL_MPF_DEBUG(("state: no match for state %d (config has %d)\n",
				              state.state, config->count));
			}
			return BCME_NOTFOUND;
		}

		/* Update the override state */
		config->override = 1;
		config->override_state = valp->state;
	} else {
		config->override = 0;
	}

	/* Determine the new state and notify clients as necessary */
	new_state = (config->override ? config->override_state : config->prev_state);
	if (new_state != old_state) {
		WL_MPF_INFO(("Notify clients: old %d new %d\n", old_state, new_state));
		for (client = config->clients; client; client = client->next)
			client->notify_fn(client->handle, client->subhandle, old_state, new_state);
	} else {
		WL_MPF_INFO(("Skip notification: no change from %d\n", old_state));
	}

	return BCME_OK;
}

static int wlc_mpf_doiovar(void *handle, const bcm_iovar_t *vi, uint32 actionid,
                           const char *name, void *params, uint p_len, void *arg, int len,
                           int val_size, struct wlc_if *wlcif)
{
	wlc_mpf_info_t *mpf = handle;
	int err = BCME_OK;

	int32 int_val = 0;
	int32 *ret_int_ptr;

	if (mpf == NULL)
		return BCME_EPERM;

	/* Convenience int for first 4 bytes of input */
	if (p_len >= (int)sizeof(int_val))
		memcpy(&int_val, params, sizeof(int_val));

	/* Convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_MPF_MAP):
		err = wlc_get_mpf_map(mpf, arg, len, params, p_len);
		break;

	case IOV_SVAL(IOV_MPF_MAP):
		err = wlc_set_mpf_map(mpf, arg, len);
		break;

	case IOV_GVAL(IOV_MPF_STATE):
		err = wlc_get_mpf_state(mpf, arg, len, params, p_len);
		break;

	case IOV_SVAL(IOV_MPF_STATE):
		err = wlc_set_mpf_state(mpf, arg, len);
		break;

	case IOV_GVAL(IOV_MPF_DEBOUNCE):
		*ret_int_ptr = (int32)mpf->debounce;
		break;

	case IOV_SVAL(IOV_MPF_DEBOUNCE):
		if ((uint32)int_val > 200)
			err = BCME_RANGE;
		else
			mpf->debounce = (uint8)int_val;
		break;

#ifdef EVENT_LOG_COMPILE
	case IOV_GVAL(IOV_MPF_POLLPRINT):
		*ret_int_ptr = (int32)mpf->pollprint;
		break;

	case IOV_SVAL(IOV_MPF_POLLPRINT):
		mpf->pollprint = (uint8)int_val;
		/* Force print/log on the next poll */
		mpf->pollprintcnt = mpf->pollprint - 1;
		break;
#endif /* EVENT_LOG_COMPILE */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* -------------------------- */
/* Client interface functions */
/* -------------------------- */
int
wlc_mpf_register(wlc_info_t *wlc, uint type, mpf_callback_t cb_fn,
                 void *handle, uintptr subhandle, uint32 mode_flags)
{
	wlc_mpf_info_t *mpf = wlc->mpf;
	wlc_mpf_config_t *config;
	wlc_mpf_client_t *client;
	bool timer_needed;

	/* Initial sanity checks */
	if (mpf == NULL)
		return BCME_NOTFOUND;
	if (type >= mpf->nconfig)
		return BCME_BADARG;
	if (cb_fn == NULL)
		return BCME_BADARG;

	config = mpf->config + type;

	client = MALLOCZ(wlc->osh, sizeof(wlc_mpf_client_t));
	if (client == NULL)
		return BCME_NOMEM;

	/* Adjust the active count as needed */
	if (config->mask && (config->clients == NULL))
		mpf->active_configs++;

	client->notify_fn = cb_fn;
	client->handle = handle;
	client->subhandle = subhandle;
	client->mode_flags = mode_flags;

	client->next = config->clients;
	config->clients = client;

	/* May need to start the MPF timer */
	WL_MPF_DEBUG(("MPF Register: active %d total %d pollprint %d\n",
	              mpf->active_configs, mpf->totalmask, mpf->pollprint));
	timer_needed = mpf->active_configs || (mpf->totalmask && mpf->pollprint);
	if (timer_needed && !mpf->driver_up && !(mpf->state_info & MPF_STATE_TIMER_ACTIVE)) {
		WL_MPF_INFO(("MPF Register: starting MPF timer\n"));
		mpf->state_info |= MPF_STATE_TIMER_ACTIVE;
		wl_add_timer(mpf->wlc->wl, mpf->mpf_timer, 1000, TRUE);
	}

	return BCME_OK;
}

int
wlc_mpf_unregister(wlc_info_t *wlc, uint type, mpf_callback_t cb_fn,
                   void *handle, uintptr subhandle)
{
	wlc_mpf_info_t *mpf = wlc->mpf;
	wlc_mpf_config_t *config;
	wlc_mpf_client_t *client, **clientp;
	bool timer_needed;

	if (mpf == NULL)
		return BCME_NOTFOUND;
	if (type >= mpf->nconfig)
		return BCME_BADARG;

	config = mpf->config + type;

	for (clientp = &config->clients; *clientp; clientp = &(*clientp)->next) {
		if (((*clientp)->notify_fn == cb_fn) &&
		    ((*clientp)->handle == handle) &&
		    ((*clientp)->subhandle == subhandle))
			break;
		clientp = &(*clientp)->next;
	}
	if (*clientp) {
		client = *clientp;
		*clientp = (*clientp)->next;
		MFREE(wlc->osh, client, sizeof(wlc_mpf_client_t));

		/* Adjust the active count as needed */
		if (config->mask && (config->clients == NULL))
			mpf->active_configs--;

		/* May need to stop the MPF timer */
		WL_MPF_DEBUG(("MPF Unregister: active %d total %d pollprint %d\n",
		              mpf->active_configs, mpf->totalmask, mpf->pollprint));
		timer_needed = mpf->active_configs || (mpf->totalmask && mpf->pollprint);
		if (!timer_needed && (mpf->state_info & MPF_STATE_TIMER_ACTIVE)) {
			WL_MPF_INFO(("MPF Unregister: stopping MPF timer\n"));
			wl_del_timer(wlc->wl, mpf->mpf_timer);
			mpf->state_info &= ~MPF_STATE_TIMER_ACTIVE;
		}
	} else {
		return BCME_NOTFOUND;
	}

	return BCME_OK;
}

int
wlc_mpf_current_state(wlc_info_t *wlc, uint16 type, uint16 *statep)
{
	wlc_mpf_info_t *mpf = wlc->mpf;
	wlc_mpf_config_t *config;

	ASSERT(statep);

	if (mpf == NULL)
		return BCME_NOTFOUND;
	if (type >= mpf->nconfig)
		return BCME_BADARG;

	config = mpf->config + type;
	if (config->mask == 0)
		return BCME_NOTREADY;

	if (statep)
		*statep = (config->override ? config->override_state : config->prev_state);

	return BCME_OK;
}

/* --------------------------------- */
/* Actual state polling/notification */
/* --------------------------------- */

static void
wlc_mpf_poll(void *arg)
{
	wlc_mpf_info_t *mpf = arg;
	wlc_info_t *wlc = mpf->wlc;
	uint16 new_bits, tmp_bits;

	uint nconfig, count;
	wlc_mpf_config_t *config;
	wl_mpf_val_t *valp;
	uint16 old_state;
	wlc_mpf_client_t *client;

	if (mpf == NULL)
		return;

#ifdef EVENT_LOG_COMPILE
	if (mpf->pollprint && (++mpf->pollprintcnt == mpf->pollprint)) {
		mpf->pollprintcnt = 0;
		WL_MPF_INFO(("MPF Polling %d\n", mpf->pollprint));
	}
#endif /* EVENT_LOG_COMPILE */

	/* Grab gpio, allow override for now */
	new_bits = si_gpioin(wlc->pub->sih);
	new_bits &= mpf->totalmask;

	/* If no change, we're done */
	if ((new_bits ^ mpf->prev_bits) == 0)
		return;

	/* Debounce */
	OSL_DELAY(mpf->debounce);
	tmp_bits = si_gpioin(wlc->pub->sih);
	tmp_bits &= mpf->totalmask;

	/* If inconsistent, skip for now */
	if ((new_bits ^ tmp_bits) != 0) {
		WL_MPF_WARN(("MPF: glitch %04x, %04x, %04x, ignore\n",
		             mpf->prev_bits, new_bits, tmp_bits));
		return;
	}

	WL_MPF_INFO(("MPF: change %04x -> %04x\n", mpf->prev_bits, new_bits));

	/* Check each config and apply relevant changes */
	for (nconfig = mpf->nconfig, config = mpf->config; nconfig; nconfig--, config++) {
		tmp_bits = new_bits & config->mask;

		if (tmp_bits == config->prev_bits)
			continue;

		old_state = config->prev_state;

		/* Map to state */
		for (count = config->count, valp = config->vals; count; count--, valp++) {
			if (valp->val == tmp_bits) {
				WL_MPF_DEBUG(("Change %04x (%d) to %04x (%d) <%02x .. %02x>\n",
				              config->prev_bits, config->prev_state,
				              valp->val, valp->state, valp->name[0],
				              valp->name[strlen(valp->name) - 1]));
				config->prev_bits = valp->val;
				config->prev_state = valp->state;
				break;
			}
		}
		if (!count)
			WL_MPF_ERR(("No val match for %04x?\n", tmp_bits));

		/* Issue any required callback(s) */
		if (config->override == 0) {
			WL_MPF_DEBUG(("Notify clients: old %d new %d\n",
			              old_state, config->prev_state));
			for (client = config->clients; client; client = client->next)
				client->notify_fn(client->handle, client->subhandle,
				                  old_state, config->prev_state);
		} else {
			WL_MPF_INFO(("Skipping change because of state override\n"));
		}
	}

	/* Update global state */
	mpf->prev_bits = new_bits;
}

static void
wlc_mpf_timer(void *arg)
{
	wlc_mpf_poll(arg);
}

static int
wlc_mpf_watchdog(void *handle)
{
	int rc = BCME_OK;
	wlc_mpf_info_t *mpf = (wlc_mpf_info_t*)handle;

	/* Sanity check */
	if (mpf == NULL)
		return rc;

	/* Make sure the independent timer is off */
	if (mpf->state_info & MPF_STATE_TIMER_ACTIVE) {
		WL_MPF_INFO(("Watchdog stopping MPF timer\n"));
		wl_del_timer(mpf->wlc->wl, mpf->mpf_timer);
		mpf->state_info &= ~MPF_STATE_TIMER_ACTIVE;
	}

	/* Poll if active configs, or for info printing */
	if (mpf->active_configs || (mpf->totalmask && mpf->pollprint)) {
		wlc_mpf_poll(mpf);
	}

	return BCME_OK;
}

static int
wlc_mpf_up(void *handle)
{
	wlc_mpf_info_t *mpf = handle;

	if (mpf == NULL)
		return BCME_OK;

	/* Watchdog should start again, let it turn off separate MPF timer */
	ASSERT(!mpf->driver_up);
	mpf->driver_up = TRUE;

	return BCME_OK;
}

static int
wlc_mpf_down(void *handle)
{
	wlc_mpf_info_t *mpf = handle;
	wlc_info_t *wlc;
	bool timer_needed;

	if (mpf == NULL)
		return BCME_OK;
	wlc = mpf->wlc;

	ASSERT(mpf->driver_up);
	mpf->driver_up = FALSE;

	/* We lose the watchdog here, depend on the MPF timer as needed */
	WL_MPF_DEBUG(("MPF down: active %d mask %d poll %d\n",
	             mpf->active_configs, mpf->totalmask, mpf->pollprint));
	WL_MPF_DEBUG(("MPF down: override %d going %d radio_d %04x\n",
	             wlc->down_override, wlc->going_down, wlc->pub->radio_disabled));

	timer_needed = mpf->active_configs || (mpf->totalmask && mpf->pollprint);
	if (timer_needed) {
		if (!(mpf->state_info & MPF_STATE_TIMER_ACTIVE)) {
			WL_MPF_INFO(("Starting MPF timer\n"));
			mpf->state_info |= MPF_STATE_TIMER_ACTIVE;
			wl_add_timer(wlc->wl, mpf->mpf_timer, 1000, TRUE);
		}
	} else if ((mpf->state_info & MPF_STATE_TIMER_ACTIVE)) {
		WL_MPF_INFO(("Stopping unexpected MPF timer\n"));
		wl_del_timer(wlc->wl, mpf->mpf_timer);
		mpf->state_info &= ~MPF_STATE_TIMER_ACTIVE;
	}

	return BCME_OK;
}

/* Could become a tunable -- or an nvram config */
#define WLC_MPF_MAX_CONFIG 1
#define WLC_MPF_DEBOUNCE 50

static const char BCMATTACHDATA(rstr_wlc_mpf)[] = "wlc_mpf";
wlc_mpf_info_t *
BCMATTACHFN(wlc_mpf_attach)(wlc_info_t *wlc)
{
	wlc_mpf_info_t *mpf;
	uint nconfig;

	STATIC_ASSERT((1 << WL_MPF_MAX_BITS) <= WL_MPF_MAX_STATES);

	nconfig = WLC_MPF_MAX_CONFIG;

	if ((mpf = MALLOCZ(wlc->osh, sizeof(struct wlc_mpf_info))) != NULL) {
		mpf->wlc = wlc;
		mpf->nconfig = nconfig;
		mpf->debounce = WLC_MPF_DEBOUNCE;
		mpf->config = MALLOCZ(wlc->osh, nconfig * sizeof(wlc_mpf_config_t));
		mpf->mpf_timer = wl_init_timer(wlc->wl, wlc_mpf_timer, mpf, "mpf");
	}

	if ((mpf == NULL) || (mpf->config == NULL) || (mpf->mpf_timer == NULL)) {
		WL_ERROR(("wl%d: %s: alloc failure\n",
		          wlc->pub->unit, __FUNCTION__));
		goto error;
	}

	if (wlc_module_register(wlc->pub, wlc_mpf_iovars, rstr_wlc_mpf, mpf, wlc_mpf_doiovar,
	                        wlc_mpf_watchdog, wlc_mpf_up, wlc_mpf_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: module registration failure\n",
		          wlc->pub->unit, __FUNCTION__));
		goto error;
	}

#ifdef EVENT_LOG_COMPILE
	event_log_tag_start(EVENT_LOG_TAG_MPF_ERR, EVENT_LOG_SET_WL,
	                    EVENT_LOG_TAG_FLAG_LOG);
#endif /* EVENT_LOG_COMPILE */

	wlc->pub->_mpf = TRUE;
	return mpf;

error:
	wlc_mpf_detach(mpf);
	return NULL;
}

int
BCMATTACHFN(wlc_mpf_detach)(wlc_mpf_info_t *mpf)
{
	int callbacks = 0;

	if (mpf == NULL)
		return callbacks;

	if (mpf->config)
		MFREE(mpf->wlc->osh, mpf->config, mpf->nconfig * sizeof(wlc_mpf_config_t));
	if (mpf->mpf_timer) {
		if (!wl_del_timer(mpf->wlc->wl, mpf->mpf_timer))
			callbacks++;

		wl_free_timer(mpf->wlc->wl, mpf->mpf_timer);
	}

	MFREE(mpf->wlc->osh, mpf, sizeof(wlc_mpf_info_t));
	return callbacks;
}

#endif /* WL_MPF */
