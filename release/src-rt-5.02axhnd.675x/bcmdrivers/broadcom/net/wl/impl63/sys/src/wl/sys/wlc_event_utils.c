/**
 * @file
 * @brief
 * event wrapper related routines
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
 * $Id: wlc_event_utils.c 780096 2019-10-15 12:42:42Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <hndd11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_event.h>
#include <wl_export.h>
#include <wlc_event_utils.h>
#include <wlc_wowlpf.h>
#include <wlc_rsdb.h>
#ifdef WLSLOTTED_BSS
#include <wlc_slotted_bss.h>
#endif /* WLSLOTTED_BSS */

static void _wlc_event_if(wlc_info_t *wlc, wlc_event_t *e,
	wlc_bsscfg_t *src_bsscfg, wlc_if_t *src_wlfif, wlc_if_t *dst_wlcif);

void
wlc_if_event(wlc_info_t *wlc, uint8 what, struct wlc_if *wlcif)
{
	wlc_event_t *e;
	wlc_bsscfg_t *cfg;
	wl_event_data_if_t *d;
	uint8 r;
	wlc_bsscfg_t *primary = wlc_bsscfg_primary(wlc);

	/*
	 * In wlc_detach function wlc_bsscfg_free is called to free the allocated
	 * bsscfgs from index 0. Unfortunately index 0 is our primary bsscfg which
	 * is used by bsscfgs at non-0 indices to send up WLC_E_IF:WLC_IF_DEL
	 * therefore the error message may be seen during detach. To fix it
	 * we can delay the free of the primary bsscfg.
	 */
	if (primary == NULL) {
		WL_ERROR(("wl%d: %s: ignore i/f event with opcode %u\n",
		          wlc->pub->unit, __FUNCTION__, what));
		return;
	}

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);
#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub) && cfg && BSSCFG_IS_RSDB_IF(cfg)) {
		WL_INFORM(("wl%d: %s: ignore i/f event with opcode %u\n",
		          wlc->pub->unit, __FUNCTION__, what));
		return;
	}
#endif /* WLRSDB */

	/* Don't do anything if the eventing system has been shutdown */
	if (wlc->eventq == NULL) {
		WL_ERROR(("wl%d: %s: eventing subsystem is down\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		return;
	}

	e = wlc_event_alloc(wlc->eventq, WLC_E_IF);
	if (e == NULL) {
		WL_ERROR(("wl%d: %s: wlc_event_alloc failed\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		return;
	}

	bcopy(&cfg->cur_etheraddr.octet, e->event.addr.octet, ETHER_ADDR_LEN);
	e->addr = &(e->event.addr);

	e->event.event_type = WLC_E_IF;
	e->event.reason = what;
	e->event.datalen = sizeof(wl_event_data_if_t);
	e->data = wlc_event_data_alloc(wlc->eventq, e->event.datalen, WLC_E_IF);
	if (e->data == NULL) {
		WL_ERROR(("wl%d: %s MALLOC failed\n", WLCWLUNIT(wlc), __FUNCTION__));
		wlc_event_free(wlc->eventq, e);
		return;
	}

	d = (wl_event_data_if_t *)e->data;
	d->ifidx = wlcif->index;
	d->opcode = what;

	d->reserved = 0;
	if (BSSCFG_HAS_NOIF(cfg)) {
		d->reserved |= WLC_E_IF_FLAGS_BSSCFG_NOIF;
	}

	d->bssidx = WLC_BSSCFG_IDX(cfg);
	if (wlcif->type == WLC_IFTYPE_BSS) {
		if (BSSCFG_AP(cfg)) {
#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg))
				r = WLC_E_IF_ROLE_P2P_GO;
			else
#endif // endif
				r = WLC_E_IF_ROLE_AP;
		} else {
#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg))
				r = WLC_E_IF_ROLE_P2P_CLIENT;
			else
#endif // endif
			if (BSSCFG_IBSS(cfg))
				r = WLC_E_IF_ROLE_IBSS;
			else
				r = WLC_E_IF_ROLE_STA;
		}
	} else {
		r = WLC_E_IF_ROLE_WDS;
#ifdef WL_HAPD_WDS
		if (SCB_LEGACY_WDS(wlcif->u.scb)) {
			if (wlcif->u.scb->flags & SCB_WPA_SUP) {
				d->reserved |= WLC_E_IF_FLAGS_LEGACY_WDS_AP;
			}
			else {
				d->reserved |= WLC_E_IF_FLAGS_LEGACY_WDS_STA;
			}
		}
#endif /* WL_HAPD_WDS */
	}

	d->role = r;

	/* always send the WLC_E_IF event to the main interface */
	_wlc_event_if(wlc, e, cfg, wlcif, primary->wlcif);

	wlc_process_event(wlc, e);
} /* wlc_if_event */

/**
 * Immediate event processing. Process the event and any events generated during the processing,
 * and queue all these events for deferred processing as well.
 */
void
wlc_process_event(wlc_info_t *wlc, wlc_event_t *e)
{
#if defined(WL_EVENT_LOG_COMPILE)
	wlc_log_event(wlc, e);
#endif // endif

#if defined(WOWLPF)
	if (WOWLPF_ENAB(wlc->pub)) {
		wlc_wowlpf_event_cb(wlc, e->event.event_type, e->event.reason);
	}
#endif // endif
	wlc_event_process(wlc->eventq, e);
}

/**
 * Record the destination interface pointer dst_wlcif in e->wlcif field and init e->event.ifname
 * field with a string that each port maintains for the OS interface associated with the source
 * interface pointed by src_wlcif.
 * dst_wlcif may become stale if one deletes the bsscfg associated with it due to the async nature
 * of events processing therefore an explicit flush of all generated events is required when
 * deleting a bsscfg. See wlc_eventq_flush() in wlc_bsscfg_free().
 */
static void
_wlc_event_if(wlc_info_t *wlc, wlc_event_t *e,
	wlc_bsscfg_t *src_bsscfg, wlc_if_t *src_wlcif, wlc_if_t *dst_wlcif)
{
	wl_if_t *wlif = NULL;

	ASSERT(dst_wlcif != NULL);

	e->wlcif = dst_wlcif;

	if (src_bsscfg != NULL) {
		e->event.bsscfgidx = WLC_BSSCFG_IDX(src_bsscfg);
	} else {
		e->event.bsscfgidx = 0;
		e->event.flags |= WLC_EVENT_MSG_UNKBSS;
	}
	if (src_wlcif != NULL) {
		e->event.ifidx = src_wlcif->index;
	} else {
		e->event.ifidx = 0;
		e->event.flags |= WLC_EVENT_MSG_UNKIF;
	}

	if (src_wlcif != NULL)
		wlif = src_wlcif->wlif;

	strncpy(e->event.ifname, wl_ifname(wlc->wl, wlif), sizeof(e->event.ifname) - 1);
	e->event.ifname[sizeof(e->event.ifname) - 1] = 0;
}

/**
 * Send the event to the OS interface associated with the scb (used by WDS, looked up by the addr
 * parm if valid) or the bsscfg. In case the bsscfg parm is unknown (NULL) or the bsscfg doesn't
 * have an OS visible interface send the event to the primary interface.
 */
void
wlc_event_if(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_event_t *e, const struct ether_addr *addr)
{
	struct scb *scb;
	wlc_if_t *wlcif;

	if (addr != NULL && !ETHER_ISMULTI(addr)) {
		bcopy(addr->octet, e->event.addr.octet, ETHER_ADDR_LEN);
		e->addr = &(e->event.addr);
	}
#ifdef WLSLOTTED_BSS
	if (addr != NULL && cfg != NULL && BSSCFG_SLOTTED_BSS(cfg) &&
		(wlcif = wlc_slotted_bss_wlcif_by_addr(wlc->sbi, addr)) != NULL) {
		_wlc_event_if(wlc, e, cfg, wlcif, wlcif);
	} else
#endif /* WLSLOTTED_BSS */
	if (addr != NULL && !ETHER_ISMULTI(addr) && cfg != NULL &&
	    (scb = wlc_scbfind(wlc, cfg, addr)) != NULL &&
	    (wlcif = SCB_WDS(scb)) != NULL && wlcif->wlif != NULL && !SCB_DWDS(scb))
		_wlc_event_if(wlc, e, SCB_BSSCFG(scb), wlcif, wlcif);
	else if (cfg != NULL && (wlcif = cfg->wlcif) != NULL) {
		if (wlcif->wlif != NULL)
			_wlc_event_if(wlc, e, cfg, wlcif, wlcif);
		else
			_wlc_event_if(wlc, e, cfg, wlcif, wlc->cfg->wlcif);
	} else {
		_wlc_event_if(wlc, e, NULL, NULL, wlc->cfg->wlcif);
	}
}

/**
 * Check whether an event requires a wl_bss_info_t TLV to be appended to it.
 * If yes, returns the bssinfo to append.  If no, returns NULL.
 */
static wlc_bss_info_t*
wlc_event_needs_bssinfo_tlv(uint msg, wlc_bsscfg_t *bsscfg)
{
	if (msg == WLC_E_AUTH || msg == WLC_E_ASSOC || msg == WLC_E_REASSOC)
		return bsscfg->target_bss;
	else if (msg == WLC_E_SET_SSID || msg == WLC_E_DEAUTH_IND ||
		msg == WLC_E_DISASSOC_IND || msg == WLC_E_ROAM)
		return bsscfg->current_bss;
	else {
		return NULL;
	}
}

void
wlc_bss_mac_event_immediate(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
	const struct ether_addr* addr, uint result, uint status, uint auth_type, void *data,
	int datalen)
{
	wlc_bss_mac_event(wlc, bsscfg, msg, addr, result, status, auth_type, data, datalen);
	/* send/flush the event immediately */
	wlc_eventq_flush(wlc->eventq);
}

/** Some events require specifying the BSS config (assoc in particular). */
void
wlc_bss_mac_event(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
	const struct ether_addr* addr,
	uint result, uint status, uint auth_type, void *data, int datalen)
{
#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub) && bsscfg && BSSCFG_IS_RSDB_IF(bsscfg)) {
		WL_INFORM(("wl%d: %s: ignore i/f event with msg %u\n",
		          wlc->pub->unit, __FUNCTION__, msg));
		return;
	}
#endif /* WLRSDB */
	wlc_bss_mac_rxframe_event(wlc, bsscfg, msg, addr, result, status, auth_type,
	                          data, datalen, NULL);
}

int
wlc_bss_mac_rxframe_event(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
                          const struct ether_addr* addr,
                          uint result, uint status, uint auth_type, void *data, int datalen,
                          wl_event_rx_frame_data_t *rxframe_data)
{
	wlc_event_t *e;
	wlc_bss_info_t *current_bss = NULL; /* if not NULL, add bss_info to event data */
	uint8 *bi_data = NULL;
	uint edatlen = 0;
	brcm_prop_ie_t *event_ie;
	int ret = BCME_ERROR;

	if (wlc->eventq == NULL) {
		WL_ERROR(("wl%d: %s: event queue is uninitialized\n",
		          wlc->pub->unit, __FUNCTION__));
		return ret;
	}

#if defined(WLRSDB) && !defined(WL_NO_BSSCFG_SHARE)
	if (RSDB_ENAB(wlc->pub) && bsscfg) {
		if (bsscfg->_idx == RSDB_INACTIVE_CFG_IDX) {
			/* bsscfg idx 1 in RSDB is not used by host. So, if any events come on
			* bsscfg 1, re-direct these to wlc-0 & bsscfg-0
			*/
			bsscfg = wlc->bsscfg[0];
			wlc = bsscfg->wlc;
		} else if (BSSCFG_IS_RSDB_IF(bsscfg)) {
			/* For RSDB_IF cfg the events should be
			 * sent to the corresponding linked primary cfg
			 */
			bsscfg =  wlc_rsdb_bsscfg_get_linked_cfg(wlc->rsdbinfo, bsscfg);
			if (bsscfg == NULL) {
				/* if WLC_BSSCFG_RSDB_IF flag is set,
				 * bsscfg should not be NULL here
				 */
				 ASSERT(0);
				return BCME_ERROR;
			}
			wlc = bsscfg->wlc;
		}
	}
#endif /* WLRSDB && !WL_NO_BSSCFG_SHARE */

	/* Get the bss_info TLV to add to selected events */
	if (EVDATA_BSSINFO_ENAB(wlc->pub) && (bsscfg != NULL) && (rxframe_data == NULL))
		current_bss = wlc_event_needs_bssinfo_tlv(msg, bsscfg);

	/* There are many WLC_E_TXFAIL but currently no special handling;
	 * possible optimization is to drop it here unless requested.
	 *
	 * if (msg == WLC_E_TXFAIL)
	 *	if (!wlc_eventq_test_ind(eq, e->event.event_type))
	 *		return;
	 */
	e = wlc_event_alloc(wlc->eventq, msg);
	if (e == NULL) {
		WL_ERROR(("wl%d: %s wlc_event_alloc failed\n", wlc->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

	e->event.event_type = msg;
	e->event.status = result;
	e->event.reason = status;
	e->event.auth_type = auth_type;

	if (rxframe_data)
		datalen += sizeof(wl_event_rx_frame_data_t);

	e->event.datalen = datalen;
	/* If a bss_info TLV needs to be appended to the event data
	 *   Add the bss_info TLV size to event data size and allocate memory
	 *   for the event data.
	 *   If that fails then try alloc from the eventd pool.
	 *   If that fails then the NULL bi_data indicates do not include the
	 *   bss_info TLV in the event data.
	 */
	if (EVDATA_BSSINFO_ENAB(wlc->pub) && (current_bss != NULL)) {
		edatlen = datalen + BRCM_PROP_IE_LEN + sizeof(wl_bss_info_t);
		e->data = wlc_event_data_alloc(wlc->eventq, edatlen, msg);
		if (e->data != NULL)
			bi_data = e->data;
	}
	if ((e->event.datalen > 0) && (data != NULL)) {
		if (EVDATA_BSSINFO_ENAB(wlc->pub)) {
			if (e->data == NULL)
				e->data = wlc_event_data_alloc(wlc->eventq, datalen,
						msg);
		} else {
			e->data = wlc_event_data_alloc(wlc->eventq, datalen, msg);
		}
		if (e->data == NULL) {
			wlc_event_free(wlc->eventq, e);
			WL_ERROR((WLC_BSS_MALLOC_ERR, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg),
				__FUNCTION__, (int)e->event.datalen,
				MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}

		if (rxframe_data) {
			bcopy(rxframe_data, e->data, sizeof(wl_event_rx_frame_data_t));
			bcopy(data, ((wl_event_rx_frame_data_t *)(e->data)) + 1,
			      e->event.datalen - sizeof(wl_event_rx_frame_data_t));
		} else
			bcopy(data, e->data, e->event.datalen);
		if (EVDATA_BSSINFO_ENAB(wlc->pub) && (bi_data != NULL))
			bi_data =  (uint8*)e->data + e->event.datalen;
	}

	/* Add bssinfo TLV to bi_data */
	if (EVDATA_BSSINFO_ENAB(wlc->pub) && (bi_data != NULL)) {
		e->event.datalen = edatlen;
		event_ie = (brcm_prop_ie_t*)bi_data;
		event_ie->id = DOT11_MNG_PROPR_ID;
		event_ie->len = (BRCM_PROP_IE_LEN - TLV_HDR_LEN) + sizeof(wl_bss_info_t);
		bcopy(BRCM_PROP_OUI, event_ie->oui, DOT11_OUI_LEN);
		event_ie->type = BRCM_EVT_WL_BSS_INFO;

		if (wlc_bss2wl_bss(wlc, current_bss,
			(wl_bss_info_t*)((uint8 *)event_ie + BRCM_PROP_IE_LEN),
			sizeof(wl_bss_info_t), FALSE)) {

			/* Really shouldn't happen, but if so then omit the bssinfo TLV */
			e->event.datalen = datalen;
		}

	}

	wlc_event_if(wlc, bsscfg, e, addr);

	wlc_process_event(wlc, e);

	return ret;
} /* wlc_bss_mac_rxframe_event */

/** Wrapper for wlc_bss_mac_event when no BSS is specified. */
void
wlc_mac_event(wlc_info_t* wlc, uint msg, const struct ether_addr* addr,
	uint result, uint status, uint auth_type, void *data, int datalen)
{
	wlc_bss_mac_event(wlc, NULL, msg, addr, result, status, auth_type, data, datalen);
}

#if !defined(WLNOEIND)
/** Send BRCM encapsulated EAPOL Events to applications. */
void
wlc_bss_eapol_event(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
	uint8 *data, uint32 len)
{
	wlc_pub_t *pub = wlc->pub;
	wlc_event_t *e;

	BCM_REFERENCE(pub);
	/* `data' should point to a WPA info element */
	if (data == NULL || len == 0) {
		WL_ERROR(("wl%d: 802.1x missing", pub->unit));
		return;
	}

	e = wlc_event_alloc(wlc->eventq, WLC_E_EAPOL_MSG);
	if (e == NULL) {
		WL_ERROR(("wl%d: %s wlc_event_alloc failed\n", pub->unit, __FUNCTION__));
		return;
	}

	e->event.event_type = WLC_E_EAPOL_MSG;
	e->event.status = WLC_E_STATUS_SUCCESS;
	e->event.reason = 0;
	e->event.auth_type = 0;

	e->event.datalen = len;
	e->data = wlc_event_data_alloc(wlc->eventq, e->event.datalen, WLC_E_EAPOL_MSG);
	if (e->data == NULL) {
		wlc_event_free(wlc->eventq, e);
		WL_ERROR(("wl%d: %s MALLOC failed\n", pub->unit, __FUNCTION__));
		return;
	}

	bcopy(data, e->data, e->event.datalen);

	wlc_event_if(wlc, bsscfg, e, ea);

#ifdef ENABLE_CORECAPTURE
	WIFICC_LOGDEBUG(("wl%d: notify NAS of 802.1x frame data len %d\n", pub->unit, len));
#endif // endif
	WL_WSEC(("wl%d: notify NAS of 802.1x frame data len %d\n", pub->unit, len));
	wlc_process_event(wlc, e);
} /* wlc_bss_eapol_event */

void
wlc_eapol_event_indicate(wlc_info_t *wlc, void *p, struct scb *scb, wlc_bsscfg_t *bsscfg,
	struct ether_header *eh)
{
#if defined(BCMDBG)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* defined(BCMDBG) */

	ASSERT(WLEIND_ENAB(wlc->pub));

	if (wlc->mac_spoof)
		return;

	/* encapsulate 802.1x frame within 802.3 LLC/SNAP */
	/*
	* Encap. is for userland applications such as NAS to tell the
	* origin of the data frame when they can't directly talk to the
	* interface, for example, they run behind a software bridge
	* where interface info is lost (it can't be identified only
	* from MAC address, for example, both WDS interface and wireless
	* interface have the save MAC address).
	*
	* If IBSS, only send up if this is from a station that is a peer.
	* Also send original one up.
	*/
	ASSERT(ntoh16(eh->ether_type) == ETHER_TYPE_802_1X);

	if (BSSCFG_AP(bsscfg)) {
		WL_APSTA_RX(("wl%d: %s: gen EAPOL event for AP %s\n",
		             wlc->pub->unit, __FUNCTION__,
		             bcm_ether_ntoa(&scb->ea, eabuf)));
	}
	if (bsscfg->BSS || SCB_IS_IBSS_PEER(scb)) {
		/*
		 * Generate EAPOL event ONLY when it is destined
		 * to the local interface address.
		 */
		if (bcmp(&eh->ether_dhost, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN) == 0) {
			wlc_bss_eapol_event(wlc, bsscfg, &scb->ea,
				PKTDATA(wlc->osh, p) + ETHER_HDR_LEN,
				PKTLEN(wlc->osh, p) - ETHER_HDR_LEN);
		}
#if defined(WLMSG_INFORM)
		else {
			char eabuf_dst[ETHER_ADDR_STR_LEN];
			char eabuf_src[ETHER_ADDR_STR_LEN];
			struct ether_addr *dst = (struct ether_addr *)&eh->ether_dhost[0];
			WL_INFORM(("wl%d: %s: Prevent gen EAPOL event from %s to %s\n",
			           wlc->pub->unit, __FUNCTION__,
			           bcm_ether_ntoa(&scb->ea, eabuf_src),
			           bcm_ether_ntoa(dst, eabuf_dst)));
		}
#endif /* WLMSG_INFORM */
	}
}

#ifdef BCM_CEVENT
/* generate WLC_E_CEVENT event based on arguments passed */
int
wlc_send_cevent(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr* addr,
                          uint result, uint status, uint auth_type, void *data, int datalen,
                          uint32 ce_subtype, uint32 ce_flags)
{
	wl_cevent_t *pce;
	int ce_len = sizeof(*pce) + datalen;
	int ret = BCME_OK;

	if (!CEVENT_STATE(wlc->pub) || datalen < 0) {
		return ret;
	}

	if (!(pce = (wl_cevent_t *) MALLOCZ(wlc->osh, ce_len))) {
		WL_ERROR(("wl%d: cevent out of mem, malloced %d bytes\n",
				wlc->pub->unit, MALLOCED(wlc->osh)));
		return BCME_ERROR;
	}

	pce->version = WLC_E_CEVENT_VER;
	pce->length = ce_len;
	pce->type = CEVENT_TYPE_D2C;
	pce->data_offset = sizeof(*pce);
	pce->flags = ce_flags;
	pce->subtype = ce_subtype;
	pce->timestamp = OSL_SYSUPTIME();

	if (data && datalen > 0) {
		memcpy(pce->data, data, datalen);
	}

	ret = wlc_bss_mac_rxframe_event(wlc, bsscfg, WLC_E_CEVENT, addr,
			result, status, auth_type, pce, ce_len, NULL);

	MFREE(wlc->osh, pce, ce_len);

	return ret;
}
#endif /* BCM_CEVENT */

#endif /* !WLNOEIND */
