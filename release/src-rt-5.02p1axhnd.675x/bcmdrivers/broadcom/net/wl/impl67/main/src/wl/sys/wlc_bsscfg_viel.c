/*
 * Per-BSS vendor IE list management.
 * Used to manage the user plumbed IEs in the BSS.
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:.*>>
 *
 * $Id: wlc_bsscfg_viel.c 783348 2020-01-24 10:25:17Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_vndr_ie_list.h>
#include <wlc_bsscfg_viel.h>
#include <wlc_dbg.h>

/* IOVar table */
/* No ordering is imposed: the table is keyed by name, and the function uses a switch. */
enum {
	IOV_VNDR_IE,
	IOV_LAST
};

static const bcm_iovar_t viel_iovars[] = {
	{"vndr_ie", IOV_VNDR_IE, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, sizeof(int)},
	{NULL, 0, 0, 0, 0, 0}
};

/* module states */
struct wlc_bsscfg_viel_info {
	wlc_info_t *wlc;
	osl_t *osh;
	int cfgh;
};

/* handy macros to access IEs' list */
#define BSS_VNDR_IE_LIST_LOC(vieli, cfg) (vndr_ie_listel_t **)BSSCFG_CUBBY(cfg, (vieli)->cfgh)
#define BSS_VNDR_IE_LIST(vieli, cfg) *BSS_VNDR_IE_LIST_LOC(vieli, cfg)

/* local declarations */
/* wlc module */
static int wlc_bsscfg_viel_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len,
	uint val_size, struct wlc_if *wlcif);

/* bsscfg cubby */
static int wlc_bsscfg_viel_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_bsscfg_viel_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
#if defined(BCMDBG)
static void wlc_bsscfg_viel_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#endif // endif

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_bsscfg_viel_info_t *
BCMATTACHFN(wlc_bsscfg_viel_attach)(wlc_info_t *wlc)
{
	bsscfg_cubby_params_t cubby_params;
	wlc_bsscfg_viel_info_t *vieli;

	/* sanity check */
	ASSERT(wlc != NULL);

	/* module states */
	if ((vieli = MALLOCZ(wlc->osh, sizeof(wlc_bsscfg_viel_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto exit;
	}

	vieli->wlc = wlc;
	vieli->osh = wlc->osh;

	/* reserve the bsscfg cubby for any bss specific private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = vieli;
	cubby_params.fn_init = wlc_bsscfg_viel_bss_init;
	cubby_params.fn_deinit = wlc_bsscfg_viel_bss_deinit;
#if defined(BCMDBG)
	cubby_params.fn_dump = wlc_bsscfg_viel_bss_dump;
#endif // endif
	vieli->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(vndr_ie_listel_t *),
	                                           &cubby_params);

	/* reserve the bsscfg cubby for any bss specific private data */
	if (vieli->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve_ext failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	if (wlc_module_register(wlc->pub, viel_iovars, "viel", vieli,
			wlc_bsscfg_viel_doiovar, NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	return vieli;

exit:
	MODULE_DETACH(vieli, wlc_bsscfg_viel_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_bsscfg_viel_detach)(wlc_bsscfg_viel_info_t *vieli)
{
	wlc_info_t *wlc;

	if (vieli == NULL)
		return;

	wlc = vieli->wlc;

	wlc_module_unregister(wlc->pub, "viel", vieli);

	MFREE(wlc->osh, vieli, sizeof(wlc_bsscfg_viel_info_t));
}

/* iovar dispatch */
static int
wlc_bsscfg_viel_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint len,
	uint val_size, struct wlc_if *wlcif)
{
	wlc_bsscfg_viel_info_t *vieli = (wlc_bsscfg_viel_info_t *)ctx;
	wlc_info_t *wlc = vieli->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;

	BCM_REFERENCE(params);
	BCM_REFERENCE(p_len);
	BCM_REFERENCE(val_size);

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_VNDR_IE): {
		vndr_ie_buf_t *vndr_ie_getbufp;

		/* Extract the vndr ie into a memory-aligned structure, then bcopy
		 * the structure into the ioctl output buffer pointer 'arg'.
		 * This extra copying is necessary in case 'arg' has an unaligned
		 * address due to a "bsscfg:N" prefix.
		 */
		if (!(vndr_ie_getbufp = MALLOC(wlc->osh, len))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			err = BCME_NOMEM;
			break;
		}
		err = wlc_vndr_ie_get(wlc->vieli, bsscfg, vndr_ie_getbufp, len, -1);
		if (err != BCME_OK) {
			MFREE(wlc->osh, vndr_ie_getbufp, len);
			break;
		}
		bcopy(vndr_ie_getbufp, (uint8*)arg,
			((err != BCME_OK) ? sizeof(int) : len));
		MFREE(wlc->osh, vndr_ie_getbufp, len);
		break;
	}

	case IOV_SVAL(IOV_VNDR_IE): {
		int rem = len;
		bool bcn_upd = FALSE;
		bool prbresp_upd = FALSE;
		vndr_ie_setbuf_t *vndr_ie_bufp = (vndr_ie_setbuf_t *)arg;

		if (len < (int)sizeof(ie_setbuf_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		/* Make a memory-aligned bcopy of the arguments in case they start
		 * on an unaligned address due to a "bsscfg:N" prefix.
		 * Note: this is variable length ie, so check
		 * upper limit can only be done when we sparse
		 * the ie below.
		 */
		if (len < (int)sizeof(vndr_ie_setbuf_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (!(vndr_ie_bufp = MALLOC(wlc->osh, len))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			err = BCME_NOMEM;
			break;
		}
		bcopy((uint8*)arg, vndr_ie_bufp, len);

		do {
			int buf_len;
			int bcn_len, prbresp_len;

			if ((buf_len = wlc_vndr_ie_buflen(&vndr_ie_bufp->vndr_ie_buffer,
					rem - VNDR_IE_CMD_LEN, &bcn_len, &prbresp_len)) < 0) {
				err = buf_len;
				break;
			}

			if (rem < VNDR_IE_CMD_LEN + buf_len) {
				err = BCME_BUFTOOSHORT;
				break;
			}

			if (!bcn_upd && bcn_len > 0)
				bcn_upd = TRUE;
			if (!prbresp_upd && prbresp_len > 0)
				prbresp_upd = TRUE;

			if (!strcmp(vndr_ie_bufp->cmd, "add")) {
				err = wlc_vndr_ie_add(wlc->vieli, bsscfg,
						&vndr_ie_bufp->vndr_ie_buffer,
						rem - VNDR_IE_CMD_LEN);
			} else if (!strcmp(vndr_ie_bufp->cmd, "del")) {
				err = wlc_vndr_ie_del(wlc->vieli, bsscfg,
						&vndr_ie_bufp->vndr_ie_buffer,
						rem - VNDR_IE_CMD_LEN);
			} else {
				err = BCME_BADARG;
			}
			if ((err != BCME_OK))
				break;

			rem -= VNDR_IE_CMD_LEN + buf_len;
			if (rem == 0)
				break;

			memmove((uint8 *)vndr_ie_bufp,
			        (uint8 *)vndr_ie_bufp + VNDR_IE_CMD_LEN + buf_len, rem);

			if ((rem < (int) sizeof(vndr_ie_setbuf_t) - 1) ||
			    (strcmp(vndr_ie_bufp->cmd, "add") &&
			     strcmp(vndr_ie_bufp->cmd, "del"))) {
				break;
			}

		}
		while (TRUE);

		MFREE(wlc->osh, vndr_ie_bufp, len);

		if (err == BCME_OK && bsscfg->up && !BSSCFG_STA(bsscfg)) {
			if (bcn_upd)
				wlc_bss_update_beacon(wlc, bsscfg);
			if (prbresp_upd)
				wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
		}

		break;
	}

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* bsscfg cubby */
static int
wlc_bsscfg_viel_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_bsscfg_viel_info_t *vieli = (wlc_bsscfg_viel_info_t *)ctx;
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, cfg);

	/* sanity check */
	ASSERT(*pviel == NULL);

	*pviel = NULL;

	return BCME_OK;
}

static void
wlc_bsscfg_viel_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_bsscfg_viel_info_t *vieli = (wlc_bsscfg_viel_info_t *)ctx;
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, cfg);

	wlc_vndr_ie_list_free(vieli->osh, pviel);

	*pviel = NULL;
}

#if defined(BCMDBG)
static void
wlc_bsscfg_viel_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_bsscfg_viel_info_t *vieli = (wlc_bsscfg_viel_info_t *)ctx;
	vndr_ie_listel_t *viel = BSS_VNDR_IE_LIST(vieli, cfg);

	for (; viel != NULL; viel = viel->next_el) {
		bcm_tlv_t *ie = (bcm_tlv_t *)&viel->vndr_ie_infoel.vndr_ie_data;

		bcm_bprintf(b, "flags: %08x ", viel->vndr_ie_infoel.pktflag);
		wlc_dump_ie(vieli->wlc, ie, b);
		bcm_bprintf(b, "\n");
	}
}
#endif // endif

/**
 * Vendor IE lists
 */

int
wlc_vndr_ie_getlen_ext(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	vndr_ie_list_filter_fn_t filter, uint32 pktflag, int *totie)
{
	vndr_ie_listel_t *viel = BSS_VNDR_IE_LIST(vieli, bsscfg);

	return wlc_vndr_ie_list_getlen_ext(viel, filter, bsscfg, pktflag, totie);
}

uint8 *
wlc_vndr_ie_write_ext(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	vndr_ie_list_write_filter_fn_t filter, uint type, uint8 *cp, int buflen, uint32 pktflag)
{
	vndr_ie_listel_t *viel = BSS_VNDR_IE_LIST(vieli, bsscfg);

	return wlc_vndr_ie_list_write_ext(viel, filter, bsscfg, type, cp, buflen, pktflag);
}

/**
 * Create a vendor IE information element object and add to the list.
 * Return value: address of the new object.
 */
vndr_ie_listel_t *
wlc_vndr_ie_add_elem(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	uint32 pktflag, vndr_ie_t *vndr_iep)
{
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, bsscfg);

	return wlc_vndr_ie_list_add_elem(vieli->osh, pviel, pktflag, vndr_iep);
}

int
wlc_vndr_ie_add(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	const vndr_ie_buf_t *ie_buf, int len)
{
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, bsscfg);

	return wlc_vndr_ie_list_add(vieli->osh, pviel, ie_buf, len);
}

int
wlc_vndr_ie_del(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	const vndr_ie_buf_t *ie_buf, int len)
{
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, bsscfg);

	return wlc_vndr_ie_list_del(vieli->osh, pviel, ie_buf, len);
}

int
wlc_vndr_ie_get(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	vndr_ie_buf_t *ie_buf, int len, uint32 pktflag)
{
	vndr_ie_listel_t *viel = BSS_VNDR_IE_LIST(vieli, bsscfg);

	return wlc_vndr_ie_list_get(viel, ie_buf, len, pktflag);
}

/**
 * Modify the data in the previously added vendor IE info.
 */
vndr_ie_listel_t *
wlc_vndr_ie_mod_elem(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	vndr_ie_listel_t *old_listel, uint32 pktflag, vndr_ie_t *vndr_iep)
{
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, bsscfg);

	return wlc_vndr_ie_list_mod_elem(vieli->osh, pviel, old_listel, pktflag, vndr_iep);
}

int
wlc_vndr_ie_mod_elem_by_type(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg,
	uint8 type, uint32 pktflag, vndr_ie_t *vndr_iep)
{
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, bsscfg);

	return wlc_vndr_ie_list_mod_elem_by_type(vieli->osh, pviel, type, pktflag, vndr_iep);
}

int
wlc_vndr_ie_del_by_type(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg, uint8 type)
{
	vndr_ie_listel_t **pviel = BSS_VNDR_IE_LIST_LOC(vieli, bsscfg);

	return wlc_vndr_ie_list_del_by_type(vieli->osh, pviel, type);
}

uint8 *
wlc_vndr_ie_find_by_type(wlc_bsscfg_viel_info_t *vieli, wlc_bsscfg_t *bsscfg, uint8 type)
{
	vndr_ie_listel_t *viel = BSS_VNDR_IE_LIST(vieli, bsscfg);

	return wlc_vndr_ie_list_find_by_type(viel, type);
}
