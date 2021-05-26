/**
 * Channel Context module WLC module wrapper - A SHARED CHANNEL CONTEXT MANAGEMENT.
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
 * $Id: wlc_chctx.c 782933 2020-01-09 10:44:46Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <wlc_types.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_dump.h>
#include <wlc_chctx_reg.h>
#include <wlc_chctx.h>
#include <wlioctl.h>

/* List required number of clients and cubbies for each chctx users
* Add all clietns and cubbies at the end.
* The default number of context is commonly defined
*/
/*
 *      1 chctx client
 */
#ifndef WLC_CHCTX_CUBBIE_RATECACHE
#define WLC_CHCTX_CUBBIE_RATECACHE            (0)
#endif /* WLC_CHCTX_CUBBIE_RATECACHE */

#define WLC_CHCTX_CLIENT_RATECACHE             1

/* # registered clients for all features */
#ifndef WLC_CHCTX_MCLNK_CLIENTS
#define WLC_CHCTX_MCLNK_CLIENTS         (WLC_CHCTX_CLIENT_RATECACHE)
#endif // endif

/* # operating channels for all links */
#ifndef WLC_CHCTX_MCLNK_CONTEXTS
#define WLC_CHCTX_MCLNK_CONTEXTS        4
#endif // endif

/* # registered cubbies for all wlc wide features */
#ifndef WLC_CHCTX_MCLNK_CUBBIES_WLC
#define WLC_CHCTX_MCLNK_CUBBIES_WLC     (WLC_CHCTX_CUBBIE_RATECACHE)
#endif // endif

/* # registered cubbies for all bss wide features */
#ifndef WLC_CHCTX_MCLNK_CUBBIES_CFG
#define WLC_CHCTX_MCLNK_CUBBIES_CFG	0
#endif // endif
/* # registered cubbies for all link wide features */
#ifndef WLC_CHCTX_MCLNK_CUBBIES_SCB
#define WLC_CHCTX_MCLNK_CUBBIES_SCB	0
#endif // endif
/* # registered cubbies for all features */
#ifndef WLC_CHCTX_MCLNK_CUBBIES
#define WLC_CHCTX_MCLNK_CUBBIES \
	(WLC_CHCTX_MCLNK_CUBBIES_WLC + \
	 WLC_CHCTX_MCLNK_CUBBIES_CFG * WLC_MAXBSSCFG + \
	 WLC_CHCTX_MCLNK_CUBBIES_SCB * MAXSCB + \
	 0)
#endif // endif

/* local functions declarations */
static void wlc_chctx_chansw_cb(void *arg, wlc_chansw_notif_data_t *data);
#if defined(BCMDBG)
static int wlc_chctx_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

/* pre attach/detach - called early in the wlc attach path without other dependencies */
wlc_chctx_info_t *
BCMATTACHFN(wlc_chctx_pre_attach)(wlc_info_t *wlc)
{
	wlc_chctx_reg_t *chctx;
	wlc_chctx_info_t *info;

	/* allocate module private state structure */
	if ((info = MALLOCZ(wlc->osh, sizeof(wlc_chctx_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed, allocated %d ytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	info->wlc = wlc;

	if ((chctx = wlc_chctx_reg_attach(wlc->osh, 0, WLC_CHCTX_REG_H_TYPE,
			WLC_CHCTX_MCLNK_CLIENTS, WLC_CHCTX_MCLNK_CUBBIES,
			WLC_CHCTX_MCLNK_CONTEXTS)) == NULL) {
		MFREE(wlc->osh, info, sizeof(wlc_chctx_info_t));
		WL_ERROR(("wl%d: %s: wlc_chctx_reg_attach failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}
	info->chctx = chctx;

	return info;
}

void
BCMATTACHFN(wlc_chctx_post_detach)(wlc_chctx_info_t *info)
{
	wlc_info_t *wlc;

	if (info == NULL) {
		return;
	}

	wlc = info->wlc;
	MODULE_DETACH(info->chctx, wlc_chctx_reg_detach);

	MFREE(wlc->osh, info, sizeof(wlc_chctx_info_t));
}

/* attach/detach - called in normal wlc module attach after wlc_assoc_attach */
int
BCMATTACHFN(wlc_chctx_attach)(wlc_chctx_info_t *info)
{
	wlc_info_t *wlc = info->wlc;
	int err;

	if ((err = wlc_chansw_notif_register(wlc, wlc_chctx_chansw_cb, info)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_chansw_notif_register failed. err=%d\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

#if defined(BCMDBG)
	wlc_dump_add_fns(wlc->pub, "chctx", wlc_chctx_dump, NULL, info);
#endif // endif

	return BCME_OK;

fail:
	MODULE_DETACH(info, wlc_chctx_detach);
	return err;
}

void
BCMATTACHFN(wlc_chctx_detach)(wlc_chctx_info_t *info)
{
	if (info == NULL) {
		return;
	}

	wlc_chansw_notif_unregister(info->wlc, wlc_chctx_chansw_cb, info);
}

/* hook info channel switch procedure... */
static void
wlc_chctx_chansw_cb(void *ctx, wlc_chansw_notif_data_t *data)
{
	wlc_chctx_info_t *info = ctx;
	wlc_chctx_reg_t *chctx = info->chctx;
	wlc_chctx_notif_t notif;

	notif.event = WLC_CHCTX_ENTER_CHAN;
	notif.chanspec = data->new_chanspec;

	(void)wlc_chctx_reg_notif(chctx, &notif);
}

#if defined(BCMDBG)
static int
wlc_chctx_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_chctx_info_t *info = ctx;
	wlc_chctx_reg_t *chctx = info->chctx;

	return wlc_chctx_reg_dump(chctx, b);
}
#endif // endif
