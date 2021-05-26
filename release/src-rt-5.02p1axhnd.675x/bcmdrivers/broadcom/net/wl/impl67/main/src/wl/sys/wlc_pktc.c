/*
 * Packet chaining module source file
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_pktc.c 781420 2019-11-20 12:39:07Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <ethernet.h>
#include <802.3.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_keymgmt.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_pktc.h>

#if defined(PKTC_TBL)
#include <wl_pktc.h>
#endif // endif

/* iovars */
enum {
	IOV_PKTC = 1,		/* Packet chaining */
	IOV_PKTCBND = 2,	/* Max packets to chain */
	IOV_PKTC_TX = 3,	/* Disable PKTC for TX in WL driver; supported for NIC only */
	IOV_LAST
};

static const bcm_iovar_t pktc_iovars[] = {
	{"pktc", IOV_PKTC, 0, 0, IOVT_BOOL, 0},
	{"pktcbnd", IOV_PKTCBND, 0, 0, IOVT_INT32, 0},
	{"pktc_tx", IOV_PKTC_TX, 0, 0, IOVT_INT32, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* local declarations */
static void wlc_pktc_scb_state_upd(void *ctx, scb_state_upd_data_t *data);

static int wlc_pktc_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif);
static void wlc_pktc_watchdog(void *ctx);
static void wlc_pktc_scb_deinit(void *ctx, struct scb *scb);

#if defined(PKTC) || defined(PKTC_FDAP)
static const char BCMATTACHDATA(rstr_pktc_policy)[] = "pktc_policy";
#endif // endif

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/* attach/detach */
wlc_pktc_info_t *
BCMATTACHFN(wlc_pktc_attach)(wlc_info_t *wlc)
{
	wlc_pktc_info_t *pktc;

	if ((pktc = MALLOCZ(wlc->osh, sizeof(wlc_pktc_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	pktc->wlc = wlc;

	if (wlc_scb_state_upd_register(wlc, wlc_pktc_scb_state_upd, pktc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_scb_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module -- needs to be last failure prone operation in this function */
	if (wlc_module_register(wlc->pub, pktc_iovars, "pktc", pktc, wlc_pktc_doiovar,
	                        wlc_pktc_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register for scb de-init to clean up scb pktc state */
	if (wlc_scb_cubby_reserve(wlc, 0, NULL, wlc_pktc_scb_deinit, NULL, pktc) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(PKTC) || defined(PKTC_FDAP)
	/* initialize default pktc policy */
	pktc->policy = (uint8)getintvar(wlc->pub->vars, rstr_pktc_policy);
#endif // endif
#if (defined(PKTC_DONGLE) && !defined(PKTC_FDAP))
	pktc->policy = PKTC_POLICY_SENDUPUC;
#endif // endif

	return pktc;

fail:
	MODULE_DETACH(pktc, wlc_pktc_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_pktc_detach)(wlc_pktc_info_t *pktc)
{
	wlc_info_t *wlc;

	if (pktc == NULL)
		return;

	wlc = pktc->wlc;

	wlc_module_unregister(wlc->pub, "pktc", pktc);

	MFREE(wlc->osh, pktc, sizeof(wlc_pktc_info_t));
}

/* module entries */
static void
wlc_pktc_watchdog(void *ctx)
{
	wlc_pktc_info_t *pktc = ctx;
	wlc_info_t *wlc = pktc->wlc;

	if (PKTC_ENAB(wlc->pub)) {
		struct scb *scb;
		struct scb_iter scbiter;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			scb->pktc_pps = 0;
		}
	}
}

static int
wlc_pktc_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_pktc_info_t *pktc = hdl;
	wlc_info_t *wlc = pktc->wlc;
	int err = BCME_OK;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_PKTC):
		*ret_int_ptr = (int32)wlc->pub->_pktc;
#ifdef PKTC_TBL
		*ret_int_ptr |= (int32)wl_pktc_req(PKTC_TBL_GET_TX_MODE, 0, 0, 0);
#endif /* PKTC_TBL */
		break;

	case IOV_SVAL(IOV_PKTC):
		wlc->pub->_pktc = bool_val;
#ifdef PKTC_TBL
		wl_pktc_req(PKTC_TBL_SET_TX_MODE, (uint32)bool_val, 0, 0);
#endif /* PKTC_TBL */
		if (bool_val == FALSE) {
			/* pktc_tx is dependent on pktc, so disable pktc_tx too */
			wlc->pub->_pktc_tx = bool_val;
		}
		break;

	case IOV_GVAL(IOV_PKTC_TX):
#ifdef PKTC_DONGLE
		err = BCME_UNSUPPORTED;
#else
		*ret_int_ptr = (int32)wlc->pub->_pktc_tx;
#endif /* PKTC_DONGLE */
		break;

	case IOV_SVAL(IOV_PKTC_TX):
#ifdef PKTC_DONGLE
		err = BCME_UNSUPPORTED;
#else
		if (PKTC_ENAB(wlc->pub)) {
			wlc->pub->_pktc_tx = bool_val;
		} else {
			WL_ERROR(("wl%d: %s Make sure \"wl -i <intf> pktc\" is enabled first\n",
				wlc->pub->unit, __FUNCTION__));
		}
#endif /* PKTC_DONGLE */
		break;

	case IOV_GVAL(IOV_PKTCBND):
		*ret_int_ptr = wlc->pub->tunables->pktcbnd;
		break;

	case IOV_SVAL(IOV_PKTCBND):
		wlc->pub->tunables->pktcbnd = MAX(int_val, wlc->pub->tunables->rxbnd);
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_doiovar */

/* enable/disable */
void
wlc_scb_pktc_enable(struct scb *scb, const wlc_key_info_t *key_info)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc = bsscfg->wlc;
	wlc_key_info_t tmp_ki;

	/* XXX For now don't enable chaining for following configs.
	 * Will enable as and when functionality is added.
	 */
	if (WET_ENAB(wlc))
		return;

#ifdef PKTC_DONGLE
	if (BSS_TDLS_BUFFER_STA(bsscfg))
		return;

	if (MONITOR_ENAB(wlc))
		return;
#endif // endif

	if (!(SCB_ASSOCIATED(scb) || SCB_LEGACY_WDS(scb)) && !SCB_AUTHORIZED(scb)) {
		return;
	}

	/* No chaining for non qos, non ampdu stas */
	if (!SCB_QOS(scb) || !SCB_WME(scb) || !SCB_AMPDU(scb)) {
		return;
	}

	if (key_info == NULL) {
		(void)wlc_keymgmt_get_scb_key(wlc->keymgmt, scb, WLC_KEY_ID_PAIRWISE,
			WLC_KEY_FLAG_NONE, &tmp_ki);
		key_info = &tmp_ki;
	}

	if (!WLC_KEY_ALLOWS_PKTC(key_info, bsscfg))
		return;

	SCB_PKTC_ENABLE(scb);
}

void
wlc_scb_pktc_disable(wlc_pktc_info_t *pktc, struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	bool cidx;

	if (bsscfg == NULL) {
		return;
	}

	/* Invalidate rfc entry if scb is in it */
	cidx = (BSSCFG_STA(bsscfg) && !(SCB_DWDS_CAP(scb) || SCB_MAP_CAP(scb))) ? 0 : 1;

	if (pktc->rfc[cidx].scb == scb) {
		WL_NONE(("wl%d: %s: Invalidate rfc %d before freeing scb %p\n",
			pktc->wlc->pub->unit, __FUNCTION__, cidx, OSL_OBFUSCATE_BUF(scb)));
		pktc->rfc[cidx].scb = NULL;
	}

#if defined(DWDS)
	/* For DWDS/MAP SCB's, if capabilities are reset before cleaning up chaining_info
	 * then wrong cidx is used to clean pktc_info->rfc.
	 * For sanity, checking for SCB in all entries.
	 */
	if (pktc->rfc[cidx ^ 1].scb == scb) {
		WL_ERROR(("wl%d: %s: ERROR: SCB capabilities are RESET\n",
			pktc->wlc->pub->unit, __FUNCTION__));
		WL_NONE(("wl%d: %s: Invalidate rfc %d before freeing scb %p\n",
			pktc->wlc->pub->unit, __FUNCTION__, cidx, scb));
		pktc->rfc[cidx].scb = NULL;
	}
#endif /* DWDS */

	SCB_PKTC_DISABLE(scb);
}

/* scb state update callback */
static void
wlc_pktc_scb_state_upd(void *ctx, scb_state_upd_data_t *data)
{
	struct scb *scb = data->scb;

	/* When transitioning to ASSOCIATED/AUTHORIZED state try if we can
	 * enable packet chaining for this SCB.
	 */
	if (SCB_ASSOCIATED(scb) || SCB_AUTHORIZED(scb)) {
		wlc_scb_pktc_enable(scb, NULL);
	}
	/* Clear scb pointer in rfc */
	else {
		wlc_scb_pktc_disable(ctx, scb);
	}
}

static void
wlc_pktc_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_pktc_info_t *pktc = (wlc_pktc_info_t *)ctx;
	/* disable rx chainable scb reference before freeing scb */
	if (pktc && scb) {
		if (SCB_ASSOCIATED(scb) || SCB_AUTHORIZED(scb))
			return;

		wlc_scb_pktc_disable(pktc, scb);
	}
}

/* packet manipulations */
static void BCMFASTPATH
wlc_pktc_sdu_prep_copy(wlc_info_t *wlc, struct scb *scb, void *p, void *n, uint32 lifetime)
{
	uint8 *dot3lsh;

	ASSERT(n != NULL);

	WLPKTTAGSCBSET(n, scb);
	WLPKTTAGBSSCFGSET(n, WLPKTTAGBSSCFGGET(p));
	WLPKTTAG(n)->flags = WLPKTTAG(p)->flags;
	WLPKTTAG(n)->flags2 = WLPKTTAG(p)->flags2;
	WLPKTTAG(n)->flags3 = WLPKTTAG(p)->flags3;
	PKTSETPRIO(n, PKTPRIO(p));

	/* When BCM_DHDHDR enabled the DHD host driver will prepare the dot3_mac_llc_snap_header,
	 * dongle only need to change the host data low addr to include it
	 */
#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
	if (PKTISTXFRAG(wlc->osh, n)) {
		ASSERT(PKTISTXFRAG(wlc->osh, p));

		/* now we have llc snap part but just in host */
		PKTSETFRAGDATA_LO(wlc->osh, n, LB_FRAG1,
			PKTFRAGDATA_LO(wlc->osh, n, LB_FRAG1) - DOT11_LLC_SNAP_HDR_LEN);
		PKTSETFRAGLEN(wlc->osh, n, LB_FRAG1,
			PKTFRAGLEN(wlc->osh, n, LB_FRAG1) + DOT11_LLC_SNAP_HDR_LEN);
		PKTSETFRAGTOTLEN(wlc->osh, n,
			PKTFRAGTOTLEN(wlc->osh, n) + DOT11_LLC_SNAP_HDR_LEN);
		goto bypass_dot3lsh;
	}
#endif /* BCM_DHDHDR && DONGLEBUILD */

	if (WLPKTTAG(p)->flags & WLF_NON8023) {
		struct ether_header *eh;
		struct ether_addr sa, da;

		/* save the original sa, da */
		eh = (struct ether_header *)PKTDATA(wlc->osh, n);
		eacopy((char *)&eh->ether_shost, &sa);
		eacopy((char *)&eh->ether_dhost, &da);

		/* Init llc snap hdr and fixup length */
		/* Original is ethernet hdr (14)
		 * Convert to 802.3 hdr (22 bytes) so only need to
		 * another 8 bytes (DOT11_LLC_SNAP_HDR_LEN)
		 */
		dot3lsh = PKTPUSH(wlc->osh, n, DOT11_LLC_SNAP_HDR_LEN);

		/* Copy only 20 bytes & retain orig ether_type */
		if ((((uintptr)PKTDATA(wlc->osh, p) | (uintptr)dot3lsh) & 3) == 0) {
			typedef struct block_s {
				uint32	data[5];
			} block_t;
			*(block_t*)dot3lsh = *(block_t*)PKTDATA(wlc->osh, p);
		} else {
			bcopy(PKTDATA(wlc->osh, p), dot3lsh,
				OFFSETOF(struct dot3_mac_llc_snap_header, type));
		}

		/* restore back the original sa, da */
		eh = (struct ether_header *)dot3lsh;
		eacopy(&sa, (char *)&eh->ether_shost);
		eacopy(&da, (char *)&eh->ether_dhost);
	}

#if defined(BCM_DHDHDR) && defined(DONGLEBUILD)
bypass_dot3lsh:
#endif // endif

	/* Set packet exptime */
	if (lifetime != 0) {
		WLPKTTAG(n)->u.exptime = WLPKTTAG(p)->u.exptime;
	}
}

void BCMFASTPATH
wlc_pktc_sdu_prep(wlc_info_t *wlc, struct scb *scb, void *p, void *n, uint32 lifetime)
{
	struct ether_header *eh;

	wlc_pktc_sdu_prep_copy(wlc, scb, p, n, lifetime);

	eh = (struct ether_header*) PKTDATA(wlc->osh, n);
	eh->ether_type = HTON16(pkttotlen(wlc->osh, n) - ETHER_HDR_LEN);
}
