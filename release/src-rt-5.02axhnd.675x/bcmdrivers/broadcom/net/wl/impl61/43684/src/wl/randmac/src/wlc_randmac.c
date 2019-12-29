/*
 * MAC Address Randomization implementation for Broadcom 802.11 Networking Driver
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
 * $Id$
 */

#include <randmacpvt.h>

#ifdef WL_PRQ_RAND_SEQ
#include <wlc_tx.h>
#endif // endif

#define CHECK_SIGNATURE(obj, value)		ASSERT((obj)->signature == (uint32)(value))
#define ASSIGN_SIGNATURE(obj, value)	((obj)->signature = value)

/* Singature declarations for service and each methods */
#define WLC_RANDMAC_SIGNATURE					0x80806051

#define RANDMAC_MACADDR_CHANGE_BIT		0
#define RANDMAC_MACADDR_ONLY_MAC_OUI_SET_BIT	1
#define RANDMAC_MACADDR_UNASSOC_ONLY_BIT	2

/* Randmac Default MAC address (OUI) and bitmask */
STATIC CONST struct ether_addr randmac_default_etheraddr = {{0x00, 0x10, 0x18, 0x00, 0x00, 0x00}};
STATIC CONST struct ether_addr randmac_default_bitmask = {{0x00, 0x00, 0x00, 0xff, 0xff, 0xff}};

static void randmac_event_signal(wlc_info_t *wlc, wl_randmac_event_type_t event_type);
static void randmac_bss_updown_cb(void *ctx, bsscfg_up_down_event_data_t *evt);
static void randmac_as_upd_cb(void *ctx, bss_assoc_state_data_t *notif_data);
static void wl_randmac_fill_mac_nic_bytes(wlc_randmac_info_t * randmac_info, uint8 *buf);
static int randmac_macaddr_apply(wlc_randmac_info_t *randmac, wlc_bsscfg_t *cfg,
	wlc_randmac_bsscfg_t *randmac_bsscfg, bool apply);
static int randmac_config(wlc_randmac_info_t *randmac_info, wl_randmac_config_t *config);
static int randmac_get(wlc_randmac_info_t *randmac, void *params, uint p_len, void *arg, int len);
static int randmac_set(wlc_randmac_info_t *randmac_info, void *arg, int len);
static int randmac_init_config_defaults(wlc_randmac_info_t *randmac_info);
static void randmac_free_bsscfg(wlc_randmac_info_t *randmac_info);
static void randmac_free_bsscfg_idx(wlc_randmac_info_t *randmac_info, int8 idx);
static int randmac_request_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req);
#ifdef WL_PROXDETECT
static void wl_randmac_ftm_evcb(void *ctx, wlc_ftm_event_data_t *evdata);
#endif /* WL_PROXDETECT */

/*
 * Send events to subscribers
 * TBD: Event(s) ??
 */
static void
randmac_event_signal(wlc_info_t *wlc, wl_randmac_event_type_t event_type)
{
	wlc_randmac_event_data_t evd;
	wlc_randmac_info_t *randmac = (wlc_randmac_info_t *)wlc->randmac_info;

	memset(&evd, 0, sizeof(evd));
	evd.randmac = (wlc_randmac_t *)randmac;
	evd.event_type = event_type;
	evd.status = BCME_OK;
	(void)bcm_notif_signal(randmac->h_notif, &evd);
}

/*
 * Iovar processing: randmac is enabled/disabled and configured by iovars
 */
static int
wlc_randmac_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint a_len, uint val_size, struct wlc_if *wlcif)
{
	wlc_randmac_info_t *randmac_info = (wlc_randmac_info_t *)ctx;
	int err = BCME_OK;
	uint16 iov_version = 0;

	ASSERT(randmac_info != NULL);
	/* Process IOVARS */

	/* handle/dispatch new API - this needs cleanup */
	do {
		wl_randmac_t *iov;
		wl_randmac_t *rsp_iov;
		wl_randmac_subcmd_t subcmd;

		if (IOV_ID(actionid) != IOV_RANDMAC) {
			WL_ERROR(("%s[%d]: invlaid IOV actionid %d IOV %d\n",
				__FUNCTION__, __LINE__, IOV_ID(actionid), IOV_RANDMAC));
			break;
		}

		if (p_len < WL_RANDMAC_IOV_HDR_SIZE) {
			WL_ERROR(("%s[%d]: invlaid parameter length %d\n",
				__FUNCTION__, __LINE__, p_len));
			break;
		}

		iov = (wl_randmac_t *)params;
		iov_version = ltoh16_ua(&iov->version);

		if (iov_version < WL_RANDMAC_API_MIN_VERSION) {
			WL_ERROR(("%s[%d]: invlaid version %04x\n",
				__FUNCTION__, __LINE__, iov_version));
			break;
		}

		/* check length - iov_len includes ver and len */
		if (p_len < iov->len) {
			err = BCME_BADLEN;
			WL_ERROR(("%s[%d]: invlaid len %d\n",
				__FUNCTION__, __LINE__, p_len));
			break;
		}

		/* all other commands except get version need exact match on version */
		subcmd = (wl_randmac_subcmd_t) ltoh16_ua(&iov->subcmd_id);

		if (iov_version != WL_RANDMAC_API_VERSION &&
			subcmd != WL_RANDMAC_SUBCMD_GET_VERSION) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* set up the result */
		rsp_iov = (wl_randmac_t *)arg;
		if (ltoh32(a_len) < WL_RANDMAC_IOV_HDR_SIZE) {
			err = BCME_BUFTOOSHORT;
			WL_ERROR(("%s[%d]: invlaid arg len %d\n",
				__FUNCTION__, __LINE__, a_len));
			break;
		}

		/* init response - length may be adjusted later */
		memcpy(rsp_iov, iov, WL_RANDMAC_IOV_HDR_SIZE);

		switch (actionid) {
		case IOV_GVAL(IOV_RANDMAC):
			err = randmac_get(randmac_info, params, p_len, arg, a_len);
			break;

		case IOV_SVAL(IOV_RANDMAC):
			err = randmac_set(randmac_info, params, ltoh32(p_len));
			break;

		default:
			err = BCME_UNSUPPORTED;
			break;
		}
		/* indicate errors that are not BCME_... , loses detail. */
		if (err != BCME_OK) {
			 if (!VALID_BCMERROR(err))
				err = BCME_ERROR;
		}
	} while (0);

	return err;
}

/*
 * Set Randmac configuration
 */
static int
randmac_config(wlc_randmac_info_t *randmac_info, wl_randmac_config_t *config)
{
	int err = BCME_OK;
	wlc_info_t *wlc = randmac_info->wlc;

	if (!randmac_info->config->enable) {
		/* randmac not enabled */
		err = BCME_NOTREADY;
		goto error;
	}

	/* don't allow multicast MAC */
	if (ETHER_ISMULTI(&config->addr)) {
		err = BCME_BADADDR;
		goto error;
	}

	/* save config */
	if (randmac_info->config->enable) {
		if ((config->flags & WL_RANDMAC_FLAGS_ADDR) ||
			config->flags == WL_RANDMAC_FLAGS_ALL) {
			memcpy(&randmac_info->config->rand_addr, &config->addr,
				ETHER_ADDR_LEN);
			randmac_info->flags |= (ENABLE << RANDMAC_MACADDR_CHANGE_BIT);
			WL_TRACE(("%s[%d]: Addr "MACF"\n", __FUNCTION__, __LINE__,
				ETHERP_TO_MACF(&(randmac_info->config->rand_addr))));
		}
		if ((config->flags & WL_RANDMAC_FLAGS_MASK) ||
			config->flags == WL_RANDMAC_FLAGS_ALL) {
			memcpy(&randmac_info->config->rand_mask, &config->addr_mask,
				ETHER_ADDR_LEN);
			randmac_info->flags |= (ENABLE << RANDMAC_MACADDR_ONLY_MAC_OUI_SET_BIT);
			WL_TRACE(("%s[%d]: Mask "MACF"\n",
				__FUNCTION__, __LINE__,
				ETHERP_TO_MACF(&(randmac_info->config->rand_mask))));
		}
		if ((config->flags & WL_RANDMAC_FLAGS_METHOD) ||
			config->flags == WL_RANDMAC_FLAGS_ALL) {
			randmac_info->config->method = ltoh16(config->method);
		}
	}

	/* Register for events to external methods */
	if (randmac_info->config->method & WL_RANDMAC_USER_FTM) {
#ifdef WL_PROXDETECT
		/* FTM method is enabled */
		if (PROXD_ENAB(wlc->pub)) {
			wlc_ftm_t *ftm = wlc_ftm_get_handle(wlc);
			if (!ftm) {
				WL_ERROR(("wl%d: Invalid FTM handle\n",
					wlc->pub->unit));
				ASSERT(FALSE);
			} else {
				err = wlc_ftm_event_register(ftm, WL_PROXD_EVENT_MASK_ALL,
					wl_randmac_ftm_evcb, (void *)randmac_info);
				if (err != BCME_OK) {
					WL_ERROR(("wl%d: Error Registering FTM Event callback\n",
						wlc->pub->unit));
					ASSERT(FALSE);
				}
			}
		}
#endif /* WL_PROXDETECT */
	}
	/* TODO: register callback for scan, NAN and other methods */

error:
	return err;
}

/*
 * Handle randmac GET iovar
 */
static int
randmac_get(wlc_randmac_info_t *randmac_info, void *params, uint p_len, void *arg, int len)
{
	int err = BCME_OK;
	wl_randmac_t *rm = params;
	wl_randmac_t *rm_out = arg;
	wl_randmac_subcmd_t subcmd;
	uint16 iovlen;

	ASSERT(rm != NULL);
	ASSERT(rm_out != NULL);
	if (!rm) {
		err = BCME_ERROR;
		goto fail;
	}
	subcmd = ltoh16(rm->subcmd_id);
	iovlen = ltoh16(rm->len);
	/* verify length */
	if (p_len < OFFSETOF(wl_randmac_t, data) ||
		iovlen > p_len - OFFSETOF(wl_randmac_t, data)) {
		return BCME_BUFTOOSHORT;
	}

	/* copy subcommand to output */
	rm_out->subcmd_id = rm->subcmd_id;

	/* process subcommand */
	switch (subcmd) {
	case WL_RANDMAC_SUBCMD_ENABLE:
	case WL_RANDMAC_SUBCMD_DISABLE:
	{
		rm_out->len = htol16(WL_RANDMAC_IOV_HDR_SIZE);
		if (randmac_info->config->enable == RANDMAC_ENABLED) {
			rm_out->subcmd_id = WL_RANDMAC_SUBCMD_ENABLE;
		} else {
			rm_out->subcmd_id = WL_RANDMAC_SUBCMD_DISABLE;
		}
		break;
	}
	case WL_RANDMAC_SUBCMD_CONFIG:
	{
		wl_randmac_config_t *rm_config = (wl_randmac_config_t *)(&rm_out->data[0]);
		rm_out->len = htol16(sizeof(wl_randmac_config_t) + WL_RANDMAC_IOV_HDR_SIZE);
		memcpy(&rm_config->addr, &randmac_info->config->rand_addr,
			ETHER_ADDR_LEN);
		memcpy(&rm_config->addr_mask, &randmac_info->config->rand_mask,
			ETHER_ADDR_LEN);
		rm_config->method = htol16(randmac_info->config->method);
		break;
	}
	case WL_RANDMAC_SUBCMD_GET_VERSION:
	{
		wl_randmac_version_t *rm_version = (wl_randmac_version_t *)(&rm_out->data[0]);
		rm->version = hton16(WL_RANDMAC_API_VERSION);
		rm_out->len = htol16(sizeof(wl_randmac_version_t) + WL_RANDMAC_IOV_HDR_SIZE);
		rm_version->version = htol16(WL_RANDMAC_API_VERSION);
		break;
	}
	case WL_RANDMAC_SUBCMD_STATS:
	{
		wl_randmac_stats_t *rm_stats = (wl_randmac_stats_t *)(&rm_out->data[0]);
		memcpy(rm_stats, &randmac_info->stats, sizeof(*rm_stats));
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
fail:
	return err;
}

/*
 * Handle randmac SET iovar
 */
static int
randmac_set(wlc_randmac_info_t *randmac_info, void *arg, int len)
{
	int err = BCME_OK;
	wl_randmac_t *rm = arg;
	wl_randmac_subcmd_t subcmd;
	uint16 iovlen;

	ASSERT(rm != NULL);
	if (!rm) {
		err = BCME_ERROR;
		goto fail;
	}
	subcmd = ltoh16(rm->subcmd_id);
	iovlen = ltoh16(rm->len);
	/* verify length */
	if (len < OFFSETOF(wl_randmac_t, data)) {
		return BCME_BUFTOOSHORT;
	}

	/* process subcommand */
	switch (subcmd) {
	case WL_RANDMAC_SUBCMD_ENABLE:
	{
		randmac_info->config->enable = RANDMAC_ENABLED;
		break;
	}
	case WL_RANDMAC_SUBCMD_DISABLE:
	{
		randmac_info->config->enable = RANDMAC_DISABLED;
		break;
	}
	case WL_RANDMAC_SUBCMD_CONFIG:
	{
		wl_randmac_config_t *rm_config = (wl_randmac_config_t *)(&rm->data[0]);
		if (iovlen >= sizeof(wl_randmac_config_t)) {
			err = randmac_config(randmac_info, rm_config);
		} else  {
			err = BCME_BADLEN;
		}
		break;
	}
	case WL_RANDMAC_SUBCMD_CLEAR_STATS:
	{
		memset(&randmac_info->stats, 0, sizeof(wlc_randmac_stats_t));
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
fail:
	return err;
}

/*
 * Using original bsscfg->_idx
 * Free randmac_bsscfg when all references to a real bsscfg
 * are done using it. This is called when refcnt == 0 and
 * original MAC address (cur_etheraddr) of bsscfg is restored.
 */
static void
randmac_free_bsscfg_idx(wlc_randmac_info_t *randmac, int8 idx)
{
	wlc_randmac_bsscfg_t *cfg;

	if (idx > WL_MAXBSSCFG) {
		ASSERT(0);
	}

	if (!(cfg = randmac->randmac_bsscfg[idx])) {
		ASSERT(0);
		goto done;
	}

	MFREE(randmac->wlc->osh, randmac->randmac_bsscfg[idx],
		sizeof(wlc_randmac_bsscfg_t));
	randmac->randmac_bsscfg[idx] = NULL;
done:
	return;
}

/*
 * Using randmac_bsscfg pointer:
 * Free all randmac_bsscfg when the module is detached.
 */
static void
randmac_free_bsscfg(wlc_randmac_info_t *randmac)
{
	uint8 bsscfg_idx;

	ASSERT(randmac != NULL);
	if (!randmac->randmac_bsscfg) {
		goto done;
	}

	for (bsscfg_idx = 0; bsscfg_idx < WL_MAXBSSCFG; bsscfg_idx++) {
		if (randmac->randmac_bsscfg[bsscfg_idx] != NULL) {
			randmac_free_bsscfg_idx(randmac, bsscfg_idx);
		}
	}
done:
	return;
}

/*
 * Send notification to subscribers.
 */
void
randmac_notify(wlc_randmac_info_t *randmac, wlc_bsscfg_t *bsscfg,
	randmac_notify_t notif, randmac_notify_info_t *info)
{
	int err = BCME_OK;

	ASSERT(randmac != NULL);
	ASSERT(bsscfg != NULL);

	BCM_REFERENCE(err);
	BCM_REFERENCE(info);

	switch (notif) {
	case RANDMAC_NOTIF_BSS_UP:
	case RANDMAC_NOTIF_BSS_DOWN:
		if (notif == RANDMAC_NOTIF_BSS_DOWN) {
#ifdef WL_PROXDETECT
			if (PROXD_ENAB(randmac->wlc->pub)) {
				wlc_ftm_t *ftm;

				ftm = wlc_ftm_get_handle(randmac->wlc);
				ASSERT(ftm != NULL);
				wlc_ftm_stop_sess_inprog(ftm, bsscfg);
			}
#endif /* WL_PROXDETECT */
			randmac_free_bsscfg_idx(randmac, WLC_BSSCFG_IDX(bsscfg));
		}
		randmac->stats.events_sent++;
		break;
	case RANDMAC_NOTIF_EVENT_TYPE:
		ASSERT(info != NULL);
		randmac_event_signal(randmac->wlc, info->event_type);
		randmac->stats.events_sent++;
		break;
	case RANDMAC_NOTIF_NONE: /* fall through */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	WL_INFORM(("%s: bss index %d notif %d status %d\n",	__FUNCTION__,
		WLC_BSSCFG_IDX(bsscfg), notif, err));
}

/*
 * Initialization: Attach randmac module
 */
wlc_randmac_info_t *
BCMATTACHFN(wlc_randmac_attach)(wlc_info_t *wlc)
{
	wlc_randmac_info_t *randmac = NULL;
	int err;

	ASSERT(wlc != NULL);

	/* Allocate randmac method wlc_randmac_info_t */
	randmac = MALLOCZ(wlc->osh, sizeof(wlc_randmac_info_t));
	if (randmac == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		goto fail;
	}

	/* Randmac detection is disabled in default */
	wlc->pub->_randmac = FALSE;

	/* save the wlc reference */
	randmac->wlc = wlc;
	ASSIGN_SIGNATURE(randmac, WLC_RANDMAC_SIGNATURE);

	/* Allocate randmac configuration */
	randmac->config =  (wlc_randmac_config_t *) MALLOCZ(wlc->osh, sizeof(wlc_randmac_config_t));
	if (randmac->config == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC randmac config allocation failed %d bytes \n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		goto fail;
	}

	/* Allocate randmac bsscfg */
	randmac->randmac_bsscfg = MALLOCZ(wlc->osh, sizeof(wlc_randmac_bsscfg_t *) * WL_MAXBSSCFG);
	if (!randmac->randmac_bsscfg) {
		WL_ERROR(("wl%d: %s: MALLOC randmac bsscfg allocation failed %d bytes \n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		goto fail;
	}

	randmac_init_config_defaults(randmac);

	err = wlc_module_register(wlc->pub, wlc_randmac_iovars, RANDMAC_NAME, (void *)randmac,
		wlc_randmac_doiovar, NULL, NULL, NULL);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed with status %d\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	/* register bsscfg up/down callback */
	if ((err = wlc_bsscfg_updown_register(wlc, randmac_bss_updown_cb, randmac)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	/* register association state change notification callback */
	if ((err = wlc_bss_assoc_state_register(wlc, randmac_as_upd_cb, randmac)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register() failed\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	/* Create notification list */
	err = bcm_notif_create_list(wlc->notif, &randmac->h_notif);
	if (err != BCME_OK) {
		ASSERT(0);
		goto fail;
	}

	return randmac;

fail:
	if (randmac->randmac_bsscfg != NULL) {
		randmac_free_bsscfg(randmac);
	}

	if (randmac->config != NULL) {
		MFREE(wlc->osh, randmac->config, sizeof(wlc_randmac_config_t));
		randmac->config = NULL;
	}

	if (randmac != NULL) {
		(void)wlc_module_unregister(wlc->pub, RANDMAC_NAME, randmac);
		MFREE(wlc->osh, randmac, sizeof(wlc_randmac_info_t));
	}

	return NULL;
}

/*
 * Detach the randmac service from WLC
 */
void
BCMATTACHFN(wlc_randmac_detach) (wlc_randmac_info_t *const randmac)
{
	wlc_info_t *wlc;
	if (randmac == NULL) {
		return;
	}

	CHECK_SIGNATURE(randmac, WLC_RANDMAC_SIGNATURE);
	wlc = randmac->wlc;

	ASSIGN_SIGNATURE(randmac, 0);

	randmac_free_bsscfg(randmac);

	if (randmac->h_notif != NULL) {
		bcm_notif_delete_list(&randmac->h_notif);
	}

	(void)wlc_bsscfg_updown_unregister(wlc, randmac_bss_updown_cb, randmac);

	(void)wlc_bss_assoc_state_unregister(wlc, randmac_as_upd_cb, randmac);

	(void)wlc_module_unregister(wlc->pub, "randmac", randmac);

	randmac->signature = 0xdeaddead;

	MFREE(wlc->osh, randmac, sizeof(wlc_randmac_info_t));
}

/*
 * Set default MAC address, bitmask and method configuration.
 */
static int
randmac_init_config_defaults(wlc_randmac_info_t *randmac_info)
{
	int err = BCME_OK;

	randmac_info->config->method = WL_RANDMAC_USER_NONE;

	memcpy(&randmac_info->config->rand_addr,
		&randmac_default_etheraddr, ETHER_ADDR_LEN);

	memcpy(&randmac_info->config->rand_mask,
		&randmac_default_bitmask, ETHER_ADDR_LEN);

	/* Only Unassociated MAC randomization is supported */
	randmac_info->flags |= (ENABLE << RANDMAC_MACADDR_UNASSOC_ONLY_BIT);
	randmac_info->is_randmac_config_updated = FALSE;
	randmac_info->config->enable = RANDMAC_DISABLED;

	randmac_info->max_cfg = WLC_MAXBSSCFG;

	return err;
}

/*
 * Callback for bss up/down
 */
static void
randmac_bss_updown_cb(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_randmac_info_t *randmac = (wlc_randmac_info_t *)ctx;
	wlc_bsscfg_t *bsscfg;
	randmac_notify_info_t ni;

	ASSERT(randmac != NULL);
	ASSERT(evt != NULL && evt->bsscfg != NULL);
	ASSERT(WL_RANDMAC_EVENT_NONE == 0);
	bsscfg = evt->bsscfg;

	randmac->stats.events_rcvd++;
	memset(&ni, 0, sizeof(ni));
	ni.bsscfg_idx = bsscfg->_idx;
	ni.event_type = WL_RANDMAC_EVENT_BSSCFG_STATUS;

	if (evt->up) {
		randmac_notify(randmac, evt->bsscfg, RANDMAC_NOTIF_BSS_UP, &ni);
	} else {
		randmac_notify(randmac, evt->bsscfg, RANDMAC_NOTIF_BSS_DOWN, &ni);
	}
}

/*
 * External API to provide randmac method handle.
 */
wlc_randmac_t*
wlc_randmac_get_handle(wlc_info_t *wlc)
{
	wlc_randmac_t *randmac = NULL;

	ASSERT(wlc != NULL);
	randmac = (wlc_randmac_t *)wlc->randmac_info;

	return randmac;
}

/*
 * External API to modules to subscribe events from randmac.
 */
int
wlc_randmac_event_register(wlc_randmac_info_t *randmac, wl_randmac_event_mask_t events,
	randmac_event_callback_t cb, void *cb_ctx)
{
	int err;

	ASSERT(randmac != NULL);
	ASSERT(randmac->h_notif != NULL);
	BCM_REFERENCE(events);

	err = bcm_notif_add_interest(randmac->h_notif, (bcm_notif_client_callback)cb,
		cb_ctx);

	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_add_interest() failed\n",
			randmac->wlc->pub->unit, __FUNCTION__, err));
	}

	return err;
}

/*
 * External API to modules to unsubscribe events from randmac.
 */
int
wlc_randmac_event_unregister(wlc_randmac_info_t *randmac, randmac_event_callback_t cb, void *cb_ctx)
{
	int err;

	ASSERT(randmac != NULL);
	ASSERT(randmac->h_notif != NULL);
	err = bcm_notif_remove_interest(randmac->h_notif, (bcm_notif_client_callback)cb,
		cb_ctx);

	if (err != BCME_OK) {
		ASSERT(FALSE);
	}

	return err;
}

/*
 * This routine saves the original MAC address and sets randomized
 * MAC address.
 */
static int
randmac_macaddr_apply(wlc_randmac_info_t *randmac, wlc_bsscfg_t *cfg,
	wlc_randmac_bsscfg_t *randmac_bsscfg, bool apply)
{
	int setrc = BCME_ERROR;

	/*
	 * Apply random addr to interface.
	 */
	if (cfg->wlcif) {
		/*
		 * TBD: if it is not a primary interface we may not need to set
		 * primary interface addr.
		 * Set
		 */
		if (apply) {
			/*
			 * Save current interface addr.
			 */
			if (memcmp(&cfg->cur_etheraddr, &randmac_bsscfg->random_addr,
				ETHER_ADDR_LEN) != 0) {
				memcpy(&randmac_bsscfg->intf_cfg_addr, &cfg->cur_etheraddr,
					ETHER_ADDR_LEN);
				setrc = BCME_OK;
				randmac->stats.set_success++;
			} else {
				randmac->stats.set_fail++;
				ASSERT(0);
			}
		} else if (!apply) {
			/*
			 * Restore original addr.
			 */
			if (memcmp(&cfg->cur_etheraddr, &randmac_bsscfg->random_addr,
				ETHER_ADDR_LEN) == 0) {
				/*
				 * Only restore if the address is not changed under us.
				 */
				memcpy(&cfg->cur_etheraddr, &randmac_bsscfg->intf_cfg_addr,
					ETHER_ADDR_LEN);
				setrc = BCME_OK;
				randmac->stats.restore_success++;
			} else {
				randmac->stats.restore_fail++;
				ASSERT(0);
			}
		}
	} else {
		struct ether_addr curmac;
		int getrc = BCME_ERROR;
		/*
		 * Is the primary interface. Retrieve the interface addr.
		 */
		getrc = wlc_iovar_op(randmac->wlc, "cur_etheraddr", NULL, 0,
			&curmac, ETHER_ADDR_LEN, IOV_GET, NULL);
		/*
		 * Set
		 */
		if (apply && (getrc == BCME_OK)) {
			/*
			 * Save current interface addr if they are not the same.
			 */
			if (memcmp(&curmac, &randmac_bsscfg->random_addr, ETHER_ADDR_LEN) != 0) {
				memcpy(&randmac_bsscfg->intf_cfg_addr, &curmac, ETHER_ADDR_LEN);
				/*
				 * Set the interface address with generated random MAC.
				 */
				setrc = wlc_iovar_op(randmac->wlc, "cur_etheraddr", NULL, 0,
					&randmac_bsscfg->random_addr, ETHER_ADDR_LEN, IOV_SET,
					NULL);
				randmac->stats.set_success++;
			} else {
				randmac->stats.set_fail++;
				ASSERT(0);
			}
		} else if (!apply && (getrc == BCME_OK)) {
			/*
			 * Restore original addr.
			 */
			if (memcmp(&curmac, &randmac_bsscfg->random_addr, ETHER_ADDR_LEN) == 0) {
				/*
				 * Only restore if the address is not changed under us.
				 */
				setrc = wlc_iovar_op(randmac->wlc, "cur_etheraddr", NULL, 0,
					&randmac_bsscfg->intf_cfg_addr, ETHER_ADDR_LEN, IOV_SET,
					NULL);
				randmac->stats.restore_success++;
			} else {
				randmac->stats.restore_fail++;
				ASSERT(0);
			}
		}

	}

	return setrc;
}

/*
 * This routine fills the bitmasked part of the MAC address with
 * random bytes.
 */
static void
wl_randmac_fill_mac_nic_bytes(wlc_randmac_info_t * randmac_info, uint8 *buf)
{
	wlc_info_t *wlc;
	int i = 0;

	if (randmac_info == NULL) {
		goto done;
	}
	wlc = randmac_info->wlc;
	/*
	 * Generate random addr.
	 */
	wlc_getrand(randmac_info->wlc, buf, ETHER_ADDR_LEN);

	/*
	 * Use bitmask to set fixed addr bytes.
	 */
	while (i < ETHER_ADDR_LEN) {
		if (randmac_info->config->rand_mask.octet[i] == 0x00) {
			buf[i] = randmac_info->config->rand_addr.octet[i];
		}
		i++;
	}
done:
	return;
}

/*
 * When MAc address randomization is requested, this routine will allocated
 * a randmac_bsscfg to save original MAC and _idx. Uses the bitmask and
 * configured MAC address (or OUI) and generates a new random MAC address
 * and copies the random MAC address into cur_etheraddr of bsscfg and
 * sets the same to the interface.
 */
static void
wl_randmac_macaddr_change(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool apply)
{
	wlc_randmac_info_t *randmac_info = (wlc_randmac_info_t *)wlc->randmac_info;
	wlc_randmac_bsscfg_t *randmac_bsscfg;
	bool randomized = FALSE;
	int8 bsscfg_idx = -1;

	ASSERT(wlc);
	ASSERT(cfg);
	ASSERT(randmac_info);
	bsscfg_idx = WLC_BSSCFG_IDX(cfg);
	ASSERT(bsscfg_idx < WLC_MAXBSSCFG);

	randmac_bsscfg = randmac_info->randmac_bsscfg[bsscfg_idx];

	/*
	 * apply == TRUE (randomize)
	 * apply == FALSE (restore)
	 */
	if (apply) {
		/* New or existing bsscfg */
		if (randmac_bsscfg == NULL) {

			/* First request for this bsscfg */
			randmac_bsscfg = MALLOCZ(wlc->osh, sizeof(wlc_randmac_bsscfg_t));
			if (!randmac_bsscfg) {
				WL_ERROR(("%s[%d]: randmac_bsscfg MALLOCZ failed\n",
					__FUNCTION__, __LINE__));
				goto done;
			}

			randmac_info->randmac_bsscfg[bsscfg_idx] = randmac_bsscfg;
		}

		if (!randmac_bsscfg->refcnt) {
			/* Save the original MAC address */
			memcpy(&randmac_bsscfg->bsscfg_addr, &cfg->cur_etheraddr,
				ETHER_ADDR_LEN);
			/*
			 * Generate random address
			 */
			wl_randmac_fill_mac_nic_bytes(randmac_info,
				&(randmac_bsscfg->random_addr.octet[0]));

			WL_TRACE(("%s[%d]: random mac "MACF"\n",
				__FUNCTION__, __LINE__,
				ETHERP_TO_MACF(&(randmac_bsscfg->random_addr))));
			/* Apply random address to interface */
			randomized = randmac_macaddr_apply(randmac_info, cfg,
				randmac_bsscfg, apply);
			if (randomized == BCME_OK) {
				/*
				 * Apply set address to bsscfg.
				 */
				memcpy(&cfg->cur_etheraddr, &randmac_bsscfg->random_addr,
					ETHER_ADDR_LEN);

#if defined(WL_PRQ_RAND_SEQ)
				/*
				 * Enable randomization of probe seq number
				 */
				wlc_tx_prq_rand_seq_enable(wlc, cfg, TRUE);
#endif /* WL_PRQ_RAND_SEQ */
			}
			WL_TRACE(("%s[%d]: saved mac "MACF"\n",
				__FUNCTION__, __LINE__,
				ETHERP_TO_MACF(&(randmac_bsscfg->bsscfg_addr))));
		}
		/*
		 * If interface address already randomized
		 * Happens when more than one bsscfg is using randmac. Subsequent requests
		 * receive random address from previous request without applying another
		 * random address to the interface. Always increment the reference count
		 */
		randmac_bsscfg->refcnt++;
		randmac_info->stats.set_reqs++;
	} else {
		randmac_bsscfg->refcnt--;
		randmac_info->stats.reset_reqs++;
		if (randmac_bsscfg->refcnt == 0) {
			/* No client using random addr so restore and apply original addr */
			randomized = randmac_macaddr_apply(randmac_info, cfg,
				randmac_bsscfg, apply);
			if (randomized == BCME_OK) {
				/*
				 * Restore bsscfg cur_etheraddr with saved addr
				 */
				memcpy(&cfg->cur_etheraddr, &randmac_bsscfg->bsscfg_addr,
					ETHER_ADDR_LEN);
			}

			memcpy(&cfg->cur_etheraddr, &randmac_bsscfg->bsscfg_addr,
				ETHER_ADDR_LEN);
			MFREE(wlc->osh, randmac_bsscfg, sizeof(*randmac_bsscfg));
			randmac_info->randmac_bsscfg[bsscfg_idx] = NULL;

#if defined(WL_PRQ_RAND_SEQ)
			/* Disable randomize probe seq seq */
			wlc_tx_prq_rand_seq_enable(wlc, cfg, FALSE);
#endif /* WL_PRQ_RAND_SEQ */
		}
	}

done:
	return;
}

/*
 * External API to decide if randmac MAC address change is enabled/disabled
 */
bool
is_randmac_macaddr_change_enabled(wlc_info_t *wlc)
{
	wlc_randmac_info_t *randmac = (wlc_randmac_info_t *)wlc->randmac_info;
	bool ret = FALSE;

	if (!(randmac->flags & (ENABLE << RANDMAC_MACADDR_CHANGE_BIT))) {
		goto done;
	}

	if (!(randmac->flags & (ENABLE << RANDMAC_MACADDR_UNASSOC_ONLY_BIT)) ||
		!wlc->stas_connected) {
		ret = TRUE;
	}
done:
	return ret;
}

static int
randmac_request_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req)
{
	int err = BCME_OK;

	switch (req) {
#ifdef WL_PROXDETECT
	case WLC_ACTION_RTT:
		if (AS_IN_PROGRESS(wlc) || SCAN_IN_PROGRESS(wlc->scan)) {
			err = BCME_ERROR;
		}
		break;
#endif /* WL_PROXDETECT */
	default:
		err = BCME_ERROR;
	}
	return err;
}

/*
 * Event handler for FTM module.
 * Each module requesting randmac service must provide a event
 * handler to invoke start/end of MAC address randomization.
 */
#ifdef WL_PROXDETECT
static void
wl_randmac_ftm_evcb(void *ctx, wlc_ftm_event_data_t *evdata)
{
	wlc_info_t *wlc = wlc_ftm_get_wlc(evdata->ftm);
	wlc_randmac_info_t *randmac = (wlc_randmac_info_t *)ctx;
	wlc_ftm_t *ftm;
	wlc_bsscfg_t *bsscfg = NULL;
	wl_proxd_session_id_t sid;
	wl_proxd_ftm_session_info_t sip;
	int idx;
	int err = BCME_OK;
	bool rand_mac = FALSE;

	ASSERT(wlc != NULL);
	ASSERT(evdata != NULL);
	ASSERT(randmac != NULL);
	if (evdata == NULL) {
		goto done;
	}

	sid = evdata->sid;
	ftm = evdata->ftm;

	/* Check if association or scan not in progress */
	rand_mac = (randmac_request_entry(wlc, NULL, WLC_ACTION_RTT) != BCME_OK);

	randmac->stats.events_rcvd++;
	switch (evdata->event_type) {
	case WL_PROXD_EVENT_SESSION_START:
		if (!rand_mac) {
			/* Check if MAC randomization is enabled */
			if (is_randmac_macaddr_change_enabled(wlc)) {
				err = wlc_ftm_get_session_info(ftm, sid, &sip);
				if (err != BCME_OK) {
					goto done;
				}
				FOREACH_BSS(wlc, idx, bsscfg) {
					if (idx == sip.bss_index) {
						break;
					}
				}
				if (bsscfg) {
					wl_randmac_macaddr_change(wlc, bsscfg, TRUE);
				}
			}
		}
		break;
	case WL_PROXD_EVENT_SESSION_END:
		if (is_randmac_macaddr_change_enabled(wlc)) {
			err = wlc_ftm_get_session_info(ftm, sid, &sip);
			if (err != BCME_OK) {
				goto done;
			}
				FOREACH_BSS(wlc, idx, bsscfg) {
					if (idx == sip.bss_index) {
						break;
					}
				}
				if (bsscfg) {
					wl_randmac_macaddr_change(wlc, bsscfg, FALSE);
				}
		}
		break;

	default:
		goto done;
	}

done:
	return;
}
#endif /* WL_PROXDETECT */

/*
 * Assoc, reassoc, roam, disassoc state change notification callback handler
 */
static void
randmac_as_upd_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_randmac_info_t *randmac = (wlc_randmac_info_t *)ctx;
	wlc_bsscfg_t *cfg;
#ifdef WL_PROXDETECT
	wlc_ftm_t *ftm;

	ASSERT(randmac != NULL);
	if (PROXD_ENAB(randmac->wlc->pub)) {
		ftm = wlc_ftm_get_handle(randmac->wlc);
		ASSERT(ftm != NULL);
	}
#endif /* WL_PROXDETECT */

	ASSERT(notif_data != NULL);
	cfg = notif_data->cfg;
	ASSERT(cfg != NULL);

	switch (notif_data->state) {
	case AS_JOIN_INIT:
	case AS_JOIN_ADOPT:
	case AS_SCAN:
#ifdef WL_PROXDETECT
		if (PROXD_ENAB(randmac->wlc->pub)) {
			wlc_ftm_stop_sess_inprog(ftm, cfg);
			randmac->stats.events_rcvd++;
		}
#endif /* WL_PROXDETECT */
		break;

	default:
		break;
	}
}
