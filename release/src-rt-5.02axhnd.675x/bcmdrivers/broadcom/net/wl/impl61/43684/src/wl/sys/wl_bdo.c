/*
 * Bonjour Dongle Offload
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wl_bdo.c 674961 2016-12-13 12:00:30Z $
 *
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wl_bdo.h>
#include <wl_mdns.h>

/* max based on customer requirement */
#define BDO_DOWNLOAD_SIZE_MAX	2048

typedef struct wl_bdo_cmn_info {
	uint8 enable;
	/* flattened database */
	uint16 total_size;
	uint16 current_size;
	uint16 next_frag_num;
	uint8 *database;
} wl_bdo_cmn_info_t;

/* bdo private info structure */
struct wl_bdo_info {
	wlc_info_t *wlc;	/* pointer back to wlc structure */
	wl_bdo_cmn_info_t *cmn;
	wlc_mdns_info_t *mdns;
};

/* wlc_pub_t struct access macros */
#define WLCUNIT(x)	((x)->wlc->pub->unit)
#define WLCOSH(x)	((x)->wlc->osh)

enum {
	IOV_BDO
};

static const bcm_iovar_t bdo_iovars[] = {
	{"bdo", IOV_BDO, (0), 0, IOVT_BUFFER, OFFSETOF(wl_bdo_t, data)},
	{NULL, 0, 0, 0, 0, 0}
};

static int bdo_get(wl_bdo_info_t *, void *, uint, void *, int);
static int bdo_set(wl_bdo_info_t *bdo_info, void *a, int alen);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* bdo GET iovar */
static int
bdo_get(wl_bdo_info_t *bdo_info, void *p, uint plen, void *a, int alen)
{
	int err = BCME_OK;
	wl_bdo_t *bdo = p;
	wl_bdo_t *bdo_out = a;

	/* verify length */
	if (plen < OFFSETOF(wl_bdo_t, data) ||
		bdo->len > plen - OFFSETOF(wl_bdo_t, data)) {
		return BCME_BUFTOOSHORT;
	}

	/* copy subcommand to output */
	bdo_out->subcmd_id = bdo->subcmd_id;

	/* process subcommand */
	switch (bdo->subcmd_id) {
	case WL_BDO_SUBCMD_ENABLE:
	{
		wl_bdo_enable_t *bdo_enable = (wl_bdo_enable_t *)bdo_out->data;
		bdo_out->len = sizeof(*bdo_enable);
		bdo_enable->enable = bdo_info->cmn->enable;
		break;
	}
	case WL_BDO_SUBCMD_MAX_DOWNLOAD:
	{
		wl_bdo_max_download_t *bdo_max_download = (wl_bdo_max_download_t *)bdo_out->data;
		bdo_out->len = sizeof(*bdo_max_download);
		bdo_max_download->size = BDO_DOWNLOAD_SIZE_MAX;
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

/* free current database */
static void
bdo_free_database(wl_bdo_info_t *bdo_info)
{
	if (bdo_info->cmn->database) {
		wlc_info_t *wlc = bdo_info->wlc;

		MFREE(wlc->osh, bdo_info->cmn->database, bdo_info->cmn->total_size);
		bdo_info->cmn->database = NULL;
		bdo_info->cmn->total_size = 0;
		bdo_info->cmn->current_size = 0;
		bdo_info->cmn->next_frag_num = 0;
	}
}

/* database download */
static int
bdo_database_download(wl_bdo_info_t *bdo_info, wl_bdo_download_t *bdo_download)
{
	int err = BCME_OK;
	wlc_info_t *wlc = bdo_info->wlc;

	if (bdo_info->cmn->enable || bdo_download->total_size > BDO_DOWNLOAD_SIZE_MAX) {
		/* cannot download while enabled or download size exceeds the max */
		return BCME_ERROR;
	}

	/* free current database and initialize for new database */
	if (bdo_download->frag_num == 0) {
		bdo_free_database(bdo_info);

		bdo_info->cmn->database = MALLOC(wlc->osh, bdo_download->total_size);
		if (bdo_info->cmn->database == NULL) {
			return BCME_NOMEM;
		}
		bdo_info->cmn->total_size = bdo_download->total_size;
		bdo_info->cmn->current_size = 0;
		bdo_info->cmn->next_frag_num = 0;
	}

	/* check fragment and save fragment */
	if (bdo_download->frag_num == bdo_info->cmn->next_frag_num &&
		bdo_download->total_size == bdo_info->cmn->total_size &&
		bdo_download->frag_size <= (bdo_info->cmn->total_size -
			bdo_info->cmn->current_size)) {
		memcpy(&bdo_info->cmn->database[bdo_info->cmn->current_size],
			bdo_download->fragment,
			bdo_download->frag_size);
		bdo_info->cmn->current_size += bdo_download->frag_size;
		bdo_info->cmn->next_frag_num++;
	} else {
		/* something gone wrong */
		bdo_free_database(bdo_info);
		err = BCME_ERROR;
	}

	return err;
}

/* returns TRUE if valid database */
static bool
bdo_is_database_valid(wl_bdo_info_t *bdo_info)
{
	if (bdo_info->cmn->database && bdo_info->cmn->total_size > 0 &&
		bdo_info->cmn->current_size == bdo_info->cmn->total_size) {
		return TRUE;
	}
	return FALSE;
}

/* bdo SET iovar */
static int
bdo_set(wl_bdo_info_t *bdo_info, void *a, int alen)
{
	int err = BCME_OK;
	wl_bdo_t *bdo = a;

	/* verify length */
	if (alen < OFFSETOF(wl_bdo_t, data) ||
		bdo->len > alen - OFFSETOF(wl_bdo_t, data)) {
		return BCME_BUFTOOSHORT;
	}

	/* process subcommand */
	switch (bdo->subcmd_id) {
	case WL_BDO_SUBCMD_DOWNLOAD:
	{
		wl_bdo_download_t *bdo_download = (wl_bdo_download_t *)bdo->data;
		if (bdo->len >= OFFSETOF(wl_bdo_download_t, fragment)) {
			err = bdo_database_download(bdo_info, bdo_download);
		} else  {
			err = BCME_BADLEN;
		}
		break;
	}
	case WL_BDO_SUBCMD_ENABLE:
	{
		wl_bdo_enable_t *bdo_enable = (wl_bdo_enable_t *)bdo->data;
		if (bdo->len >= sizeof(*bdo_enable)) {
			if (bdo_enable->enable != bdo_info->cmn->enable) {
				if (bdo_enable->enable && !bdo_is_database_valid(bdo_info)) {
					/* database must be valid to enable */
					err = BCME_ERROR;
				} else {
					bdo_info->cmn->enable = bdo_enable->enable;
					if (bdo_info->cmn->enable) {
						WL_INFORM(("database size: %d\n",
						bdo_info->cmn->total_size));
						if (!wl_mDNS_Init(bdo_info->mdns,
							bdo_info->cmn->database,
							bdo_info->cmn->total_size)) {
							/* mDNS failed to initialize */
							bdo_info->cmn->enable = FALSE;
							err = BCME_ERROR;
						}
					}
					/* disable or failed to initialize */
					if (!bdo_info->cmn->enable) {
						/* free memory and database */
						wl_mDNS_Exit(bdo_info->mdns);
						bdo_free_database(bdo_info);
					}
				}
			}
		} else  {
			err = BCME_BADLEN;
		}
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

/* handling bdo related iovars */
static int
bdo_doiovar(void *hdl, uint32 actionid,
            void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wl_bdo_info_t *bdo_info = hdl;
	int32 int_val = 0;
	int err = BCME_OK;
	ASSERT(bdo_info);

#ifndef BCMROMOFFLOAD
	WL_INFORM(("wl%d: bdo_doiovar()\n", WLCUNIT(bdo_info)));
#endif /* !BCMROMOFFLOAD */

	/* Do nothing if is not supported */
	if (!BDO_SUPPORT(bdo_info->wlc->pub)) {
		return BCME_UNSUPPORTED;
	}

	if (plen >= (int)sizeof(int_val)) {
		bcopy(p, &int_val, sizeof(int_val));
	}

	switch (actionid) {
	case IOV_GVAL(IOV_BDO):
		err = bdo_get(bdo_info, p, plen, a, alen);
		break;
	case IOV_SVAL(IOV_BDO):
		err = bdo_set(bdo_info, a, alen);
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* Wrapper function for mdns_rx */
bool
wl_bdo_rx(wl_bdo_info_t *bdo, void *pkt, uint16 len)
{
	if (mdns_rx(bdo->mdns, pkt, len)) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/*
 * initialize bdo private context.
 * returns a pointer to the bdo private context, NULL on failure.
 */
wl_bdo_info_t *
BCMATTACHFN(wl_bdo_attach)(wlc_info_t *wlc)
{
	wl_bdo_info_t *bdo;

	/* allocate bdo private info struct */
	bdo = MALLOCZ(wlc->osh, sizeof(wl_bdo_info_t));
	if (!bdo) {
		WL_ERROR(("wl%d: %s: MALLOC failed; total mallocs %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	bdo->wlc = wlc;

	/* attach mdns */
	if ((bdo->mdns = wlc_mdns_attach(wlc)) == NULL) {
		WL_ERROR(("wlc_mdns_attach failed\n"));
		wl_bdo_detach(bdo);
		return NULL;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, bdo_iovars, "bdo",
		bdo, bdo_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		wl_bdo_detach(bdo);
		return NULL;
	}
	/* OBJECT REGISTRY: check if shared bdo_cmn_info  is already malloced */

	bdo->cmn = (wl_bdo_cmn_info_t*)
		obj_registry_get(wlc->objr, OBJR_BDO_CMN);

	if (bdo->cmn == NULL) {
		if ((bdo->cmn =  (wl_bdo_cmn_info_t*) MALLOCZ(wlc->osh,
			sizeof(wl_bdo_cmn_info_t))) == NULL) {
			WL_ERROR(("wl%d: %s: anqpo_cmn alloc failed\n",
				WLCWLUNIT(wlc), __FUNCTION__));
			return NULL;
		}
		/* OBJECT REGISTRY: We are the first instance, store value for key */
		obj_registry_set(wlc->objr, OBJR_BDO_CMN, bdo->cmn);
	}
	BCM_REFERENCE(obj_registry_ref(wlc->objr, OBJR_BDO_CMN));

	wlc->pub->_bdo_support = TRUE;
	return bdo;
}

/* cleanup bdo private context */
void
BCMATTACHFN(wl_bdo_detach)(wl_bdo_info_t *bdo)
{
	WL_INFORM(("wl%d: bdo_detach()\n", WLCUNIT(bdo)));

	if (!bdo) {
		return;
	}

	/* disable if currently running */
	if (bdo->cmn->enable) {
		wl_mDNS_Exit(bdo->mdns);
		bdo_free_database(bdo);
	}

	if (bdo->mdns) {
		wl_mdns_detach(bdo->mdns);
	}

	bdo->wlc->pub->_bdo = FALSE;
	if (obj_registry_unref(bdo->wlc->objr, OBJR_BDO_CMN) == 0) {
		obj_registry_set(bdo->wlc->objr, OBJR_BDO_CMN, NULL);
		MFREE(bdo->wlc->osh, bdo->cmn, sizeof(wl_bdo_cmn_info_t));
	}

	wlc_module_unregister(bdo->wlc->pub, "bdo", bdo);
	MFREE(WLCOSH(bdo), bdo, sizeof(wl_bdo_info_t));
}
