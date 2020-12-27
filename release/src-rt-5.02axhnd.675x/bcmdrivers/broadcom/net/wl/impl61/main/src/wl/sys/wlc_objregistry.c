/*
 * WLC Object Registry API Implementation
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_objregistry.c 668295 2016-11-02 20:17:03Z $
 */

/**
 * @file
 * @brief
 * Chip/Feature specific enable/disable need to be done for Object registry
 * Moved object registry related functions from wl/sys to utils folder
 * A wrapper is provided in this layer/file to enab/disab obj registry for each key
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */

#ifdef WL_OBJ_REGISTRY

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <wl_dbg.h>
#include <wlc_types.h>
#include <bcm_objregistry.h>
#include <wlc_objregistry.h>

#if defined(BCMDBG) || defined(BCMDBG_ERR)
#define WLC_OBJR_ERROR(args)	printf args
#else
#define WLC_OBJR_ERROR(args)
#endif // endif

/* WLC OBj reg to include Chip/feature specific enable/disable support for data sharing */
struct wlc_obj_registry {
	obj_registry_t *objr;
	uint8 key_enab[OBJR_MAX_KEYS/NBBY + 1];
};

wlc_obj_registry_t*
BCMATTACHFN(obj_registry_alloc)(osl_t *osh, int count)
{
	wlc_obj_registry_t *wlc_objr = NULL;
	if ((wlc_objr = MALLOCZ(osh, sizeof(wlc_obj_registry_t))) == NULL) {
			WLC_OBJR_ERROR(("bcm obj registry %s: out of memory, malloced %d bytes\n",
				__FUNCTION__, MALLOCED(osh)));
	} else {
		wlc_objr->objr = bcm_obj_registry_alloc(osh, count);
		if (wlc_objr->objr) {
			memset(wlc_objr->key_enab, 0xff, OBJR_MAX_KEYS / NBBY + 1);
		} else {
			MFREE(osh, wlc_objr, sizeof(wlc_obj_registry_t));
			wlc_objr = NULL;
		}
	}
	return wlc_objr;
}

void
BCMATTACHFN(obj_registry_free)(wlc_obj_registry_t *wlc_objr, osl_t *osh)
{
	if (wlc_objr) {
		if (wlc_objr->objr)
			bcm_obj_registry_free(wlc_objr->objr, osh);
		MFREE(osh, wlc_objr, sizeof(wlc_obj_registry_t));
	}
}

int
BCMATTACHFN(obj_registry_set)(wlc_obj_registry_t *wlc_objr, obj_registry_key_t key, void *value)
{
	int ret = BCME_OK;
	ASSERT(key < OBJR_MAX_KEYS);
	if (isset(wlc_objr->key_enab, key)) {
		ret = bcm_obj_registry_set(wlc_objr->objr, key, value);
	}
	return ret;
}

void*
BCMATTACHFN(obj_registry_get)(wlc_obj_registry_t *wlc_objr, obj_registry_key_t key)
{
	void *ret = NULL;
	ASSERT(key < OBJR_MAX_KEYS);
	if (isset(wlc_objr->key_enab, key)) {
		ret = bcm_obj_registry_get(wlc_objr->objr, key);
	}
	return ret;
}

int
BCMATTACHFN(obj_registry_ref)(wlc_obj_registry_t *wlc_objr, obj_registry_key_t key)
{
	int ret = 1;
	ASSERT(key < OBJR_MAX_KEYS);
	if (isset(wlc_objr->key_enab, key)) {
		ret = bcm_obj_registry_ref(wlc_objr->objr, key);
	}
	return ret;
}

int
BCMATTACHFN(obj_registry_unref)(wlc_obj_registry_t *wlc_objr, obj_registry_key_t key)
{
	int ret = 0;
	ASSERT(key < OBJR_MAX_KEYS);
	if (isset(wlc_objr->key_enab, key)) {
		ret = bcm_obj_registry_unref(wlc_objr->objr, key);
	}
	return ret;
}

int
BCMATTACHFN(obj_registry_get_ref)(wlc_obj_registry_t *wlc_objr, obj_registry_key_t key)
{
	int ret = 0;
	ASSERT(key < OBJR_MAX_KEYS);
	if (isset(wlc_objr->key_enab, key)) {
		ret = bcm_obj_registry_get_ref(wlc_objr->objr, key);
	}
	return ret;
}

/* A special helper function to identify if we are cleaning up for the finale WLC */
int
obj_registry_islast(wlc_obj_registry_t *wlc_objr)
{
	return bcm_obj_registry_islast(wlc_objr->objr);
}

void
BCMATTACHFN(obj_registry_disable)(wlc_obj_registry_t *wlc_objr, obj_registry_key_t key)
{
	ASSERT(key < OBJR_MAX_KEYS);
	clrbit(wlc_objr->key_enab, key);

	/* Un Ref the registry and then reset the value of the stored pointer to NULL */
	if (obj_registry_unref(wlc_objr, key) == 0) {
		obj_registry_set(wlc_objr, key, NULL);
	}
}

#if defined(BCMDBG)
int
BCMATTACHFN(wlc_dump_objr)(wlc_obj_registry_t *wlc_objr, struct bcmstrbuf *b)
{
	int i = 0;
	if (wlc_objr) {
		bcm_bprintf(b, "\nDumping WLC Object Registry Key Enable/Disable value\n");
		bcm_bprintf(b, "Key\tEnabled?\n");
		for (i = 0; i < OBJR_MAX_KEYS; i++) {
			bcm_bprintf(b, "%d\t%d\n",
				i, isset(wlc_objr->key_enab, i));
		}
		bcm_dump_objr(wlc_objr->objr, b);
	}
	else {
		bcm_bprintf(b, "\nWLC Object Registry is not present\n");
	}
	return 0;
}
#endif /* BCMDBG */

#endif /* WL_OBJ_REGISTRY */
