/*
 * Event mechanism
 *
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
 * $Id: wlc_event.c 784961 2020-03-10 06:53:38Z $
 */

/**
 * @file
 * @brief
 * The WLAN driver currently has tight coupling between different components. In particular,
 * components know about each other, and call each others functions, access data, and invoke
 * callbacks. This means that maintenance and new features require changing these
 * relationships. This is fundamentally a tightly coupled system where everything touches
 * many other things.
 *
 * @brief
 * We can reduce the coupling between our features by reducing their need to directly call
 * each others functions, and access each others data. An mechanism for accomplishing this is
 * a generic event signaling mechanism. The event infrastructure enables modules to communicate
 * indirectly through events, rather than directly by calling each others routines and
 * callbacks.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlanSwArchitectureEventNotification]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wl_dbg.h>

#include <wlc_pub.h>
#include <wlc_key.h>
#include <wl_export.h>
#include <wlc_event.h>
#include <bcm_mpool_pub.h>

/* For wlc.h */
#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_rate_sel.h>
#ifdef MSGTRACE
#include <msgtrace.h>
#endif // endif
#ifdef LOGTRACE
#include <logtrace.h>
#endif // endif

/* Local prototypes */
#ifndef WLNOEIND
static int wlc_event_sendup(wlc_eventq_t *eq, const wlc_event_t *e,
	struct ether_addr *da, struct ether_addr *sa, uint8 *data, uint32 len, void *p);
#endif /* WLNOEIND */
static void wlc_timer_cb(void *arg);
static int wlc_eventq_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);

enum {
	IOV_EVENT_MSGS,
	IOV_EVENT_MSGS_EXT
};

static const bcm_iovar_t eventq_iovars[] = {
	{"event_msgs", IOV_EVENT_MSGS,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, WL_EVENTING_MASK_LEN
	},
	{"event_msgs_ext", IOV_EVENT_MSGS_EXT,
	(IOVF_OPEN_ALLOW), IOVT_BUFFER, EVENTMSGS_EXT_STRUCT_SIZE
	},
	{NULL, 0, 0, 0, 0 }
};

#define WL_EVENTING_MASK_EXT_LEN \
	MAX(WL_EVENTING_MASK_LEN, (ROUNDUP(WLC_E_LAST, NBBY)/NBBY))

/* Private data structures */
struct wlc_eventq
{
	wlc_event_t		*head;
	wlc_event_t		*tail;
	struct wlc_info		*wlc;
	void			*wl;
	wlc_pub_t 		*pub;
	bool			tpending;
	bool			workpending;
	struct wl_timer		*timer;
	wlc_eventq_cb_t		cb;
	bcm_mp_pool_h		mpool_h;
	uint8			event_inds_mask[WL_EVENTING_MASK_EXT_LEN];
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/*
 * Export functions
 */
static const char BCMATTACHDATA(rstr_eventq)[] = "eventq";
wlc_eventq_t*
BCMATTACHFN(wlc_eventq_attach)(wlc_pub_t *pub, struct wlc_info *wlc, void *wl, wlc_eventq_cb_t cb)
{
	wlc_eventq_t *eq;

	eq = (wlc_eventq_t*)MALLOC(pub->osh, sizeof(wlc_eventq_t));
	if (eq == NULL)
		return NULL;

	bzero(eq, sizeof(wlc_eventq_t));

	/* Create memory pool for 'wlc_event_t' data structs. */
	if (bcm_mpm_create_heap_pool(wlc->mem_pool_mgr, sizeof(wlc_event_t),
	                             "event", &eq->mpool_h) != BCME_OK) {
		WL_ERROR(("wl%d: bcm_mpm_create_heap_pool failed\n", pub->unit));
		MFREE(pub->osh, eq, sizeof(wlc_eventq_t));
		return NULL;
	}

	eq->cb = cb;
	eq->wlc = wlc;
	eq->wl = wl;
	eq->pub = pub;

	/* register event module */
	if (wlc_module_register(pub, eventq_iovars, rstr_eventq, eq, wlc_eventq_doiovar,
		NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: event wlc_module_register() failed\n", pub->unit));
		MFREE(eq->pub->osh, eq, sizeof(wlc_eventq_t));
		return NULL;
	}

	if (!(eq->timer = wl_init_timer(eq->wl, wlc_timer_cb, eq, rstr_eventq))) {
		WL_ERROR(("wl%d: wlc_eventq_attach: timer failed\n", pub->unit));
		wlc_module_unregister(eq->pub, "eventq", eq);
		MFREE(eq->pub->osh, eq, sizeof(wlc_eventq_t));
		return NULL;
	}

	return eq;
}

int
BCMATTACHFN(wlc_eventq_detach)(wlc_eventq_t *eq)
{
	/* Clean up pending events */
	wlc_eventq_down(eq);

	wlc_module_unregister(eq->pub, rstr_eventq, eq);

	if (eq->timer) {
		if (eq->tpending) {
			wl_del_timer(eq->wl, eq->timer);
			eq->tpending = FALSE;
		}
		wl_free_timer(eq->wl, eq->timer);
		eq->timer = NULL;
	}

	ASSERT(wlc_eventq_avail(eq) == FALSE);

	bcm_mpm_delete_heap_pool(eq->wlc->mem_pool_mgr, &eq->mpool_h);

	MFREE(eq->pub->osh, eq, sizeof(wlc_eventq_t));
	return 0;
}

static int
wlc_eventq_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_eventq_t *eq = (wlc_eventq_t *)hdl;
	int err = BCME_OK;
	wlc_info_t *wlc = eq->wlc;

	BCM_REFERENCE(wlc);

	switch (actionid) {
		case IOV_GVAL(IOV_EVENT_MSGS): {
			bzero(arg, len);
			err = wlc_eventq_query_ind(wlc->eventq, arg);
			break;
		}

		case IOV_SVAL(IOV_EVENT_MSGS): {
			err = wlc_eventq_register_ind(wlc->eventq, arg);
			break;
		}

		case IOV_GVAL(IOV_EVENT_MSGS_EXT): {
			err = wlc_eventq_query_ind_ext(wlc->eventq, (eventmsgs_ext_t*)params,
				(eventmsgs_ext_t*)arg, len);
			break;
		}

		case IOV_SVAL(IOV_EVENT_MSGS_EXT): {
			err = wlc_eventq_register_ind_ext(wlc->eventq, (eventmsgs_ext_t*) arg, len);
			break;
		}

		default: {
			err = BCME_UNSUPPORTED;
			break;
		}
	}
	return err;
}

int
BCMUNINITFN(wlc_eventq_down)(wlc_eventq_t *eq)
{
	int callbacks = 0;
	if (eq->tpending && !eq->workpending) {
		if (!wl_del_timer(eq->wl, eq->timer))
			callbacks++;

		ASSERT(wlc_eventq_avail(eq) == TRUE);
		ASSERT(eq->workpending == FALSE);
		eq->workpending = TRUE;
		if (eq->cb)
			eq->cb(eq->wlc);

		ASSERT(eq->workpending == TRUE);
		eq->workpending = FALSE;
		eq->tpending = FALSE;
	}
	else {
		ASSERT(eq->workpending || wlc_eventq_avail(eq) == FALSE);
	}
	return callbacks;
}

wlc_event_t*
wlc_event_alloc(wlc_eventq_t *eq, uint eventid)
{
	wlc_event_t *e;

	e = (wlc_event_t *) bcm_mp_alloc(eq->mpool_h);

	if (e == NULL)
		return NULL;

	bzero(e, sizeof(wlc_event_t));
	return e;
}

void*
wlc_event_data_alloc(wlc_eventq_t *eq, osl_t *osh, uint32 datalen, uint32 event_id)
{
	return MALLOC(osh, datalen);
}

int
wlc_event_data_free(wlc_eventq_t *eq, osl_t *osh, void *data, uint32 datalen)
{
	MFREE(osh, data, datalen);
	return 0;
}

void*
wlc_event_pktget(wlc_eventq_t *eq, osl_t *osh, uint pktlen, uint32 event_id)
{
	return PKTGET(osh, pktlen, FALSE);
}

void
wlc_event_free(wlc_eventq_t *eq, wlc_event_t *e)
{
	ASSERT(e->data == NULL);
	ASSERT(e->next == NULL);
	bcm_mp_free(eq->mpool_h, e);
}

void
wlc_eventq_enq(wlc_eventq_t *eq, wlc_event_t *e)
{
	ASSERT(e->next == NULL);
	e->next = NULL;

	if (eq->tail) {
		eq->tail->next = e;
		eq->tail = e;
	}
	else
		eq->head = eq->tail = e;

	if (!eq->tpending) {
		eq->tpending = TRUE;
		/* Use a zero-delay timer to trigger
		 * delayed processing of the event.
		 */
		wl_add_timer(eq->wl, eq->timer, 0, 0);
	}
}

wlc_event_t*
wlc_eventq_deq(wlc_eventq_t *eq)
{
	wlc_event_t *e;

	e = eq->head;
	if (e) {
		eq->head = e->next;
		e->next = NULL;

		if (eq->head == NULL)
			eq->tail = eq->head;
	}
	return e;
}

wlc_event_t*
wlc_eventq_next(wlc_eventq_t *eq, wlc_event_t *e)
{
#ifdef BCMDBG
	wlc_event_t *etmp;

	for (etmp = eq->head; etmp; etmp = etmp->next) {
		if (etmp == e)
			break;
	}
	ASSERT(etmp != NULL);
#endif // endif

	return e->next;
}

int
wlc_eventq_cnt(wlc_eventq_t *eq)
{
	wlc_event_t *etmp;
	int cnt = 0;

	for (etmp = eq->head; etmp; etmp = etmp->next)
		cnt++;

	return cnt;
}

bool
wlc_eventq_avail(wlc_eventq_t *eq)
{
	return (eq->head != NULL);
}

#ifndef WLNOEIND
int
wlc_eventq_register_ind(wlc_eventq_t *eq, void *bitvect)
{
	bcopy(bitvect, eq->event_inds_mask, WL_EVENTING_MASK_LEN);

	wlc_enable_probe_req(
		eq->wlc,
		PROBE_REQ_EVT_MASK,
		wlc_eventq_test_ind(eq, WLC_E_PROBREQ_MSG)? PROBE_REQ_EVT_MASK:0);
#if defined(MSGTRACE) || defined(LOGTRACE)
	if (isset(eq->event_inds_mask, WLC_E_TRACE)) {
#ifdef MSGTRACE
		msgtrace_start();
#endif // endif
#ifdef LOGTRACE
		logtrace_start();
#endif // endif
	} else {
#ifdef MSGTRACE
		msgtrace_stop();
#endif // endif
#ifdef LOGTRACE
		logtrace_stop();
#endif // endif
	}
#endif /* MSGTRACE || LOGTRACE */
	return 0;
}

int
wlc_eventq_register_ind_ext(wlc_eventq_t *eq, eventmsgs_ext_t* iovar_msg, uint32 len)
{
	int i;
	int current_event_mask_size;

	if (len < (iovar_msg->len + EVENTMSGS_EXT_STRUCT_SIZE))
		return BCME_BUFTOOSHORT;

	if (iovar_msg->ver != EVENTMSGS_VER)
		return BCME_VERSION;

	current_event_mask_size = MIN(WL_EVENTING_MASK_EXT_LEN, iovar_msg->len);

	switch (iovar_msg -> command) {
		case EVENTMSGS_SET_BIT:
			for (i = 0; i < current_event_mask_size; i++)
				eq->event_inds_mask[i] |= iovar_msg->mask[i];
			break;
		case EVENTMSGS_RESET_BIT:
			for (i = 0; i < current_event_mask_size; i++)
				eq->event_inds_mask[i] &= iovar_msg->mask[i];
			break;
		case EVENTMSGS_SET_MASK:
			bcopy(iovar_msg->mask, eq->event_inds_mask, current_event_mask_size);
			break;
	};
	return 0;
}

int
wlc_eventq_query_ind_ext(wlc_eventq_t *eq,
	eventmsgs_ext_t* in_iovar_msg, eventmsgs_ext_t* out_iovar_msg, uint32 outlen)
{
	if (outlen < (EVENTMSGS_EXT_STRUCT_SIZE + in_iovar_msg->len))
		return BCME_BUFTOOSHORT;

	if (in_iovar_msg->ver != EVENTMSGS_VER)
		return BCME_VERSION;

	out_iovar_msg->len = MIN(WL_EVENTING_MASK_EXT_LEN, in_iovar_msg->len);
	out_iovar_msg->maxgetsize = WL_EVENTING_MASK_EXT_LEN;
	bcopy(eq->event_inds_mask, out_iovar_msg->mask, out_iovar_msg->len);
	return 0;
}

int
wlc_eventq_query_ind(wlc_eventq_t *eq, void *bitvect)
{
	bcopy(eq->event_inds_mask, bitvect, WL_EVENTING_MASK_LEN);
	return 0;
}

int
wlc_eventq_test_ind(wlc_eventq_t *eq, int et)
{
	return isset(eq->event_inds_mask, et);
}

int
wlc_eventq_handle_ind(wlc_eventq_t *eq, wlc_event_t *e)
{
	wlc_bsscfg_t *cfg;
	struct ether_addr *da;
	struct ether_addr *sa;

	cfg = wlc_bsscfg_find_by_wlcif(eq->wlc, e->wlcif);
	ASSERT(cfg != NULL);

	da = &cfg->cur_etheraddr;
	sa = &cfg->cur_etheraddr;

	if (wlc_eventq_test_ind(eq, e->event.event_type))
		wlc_event_sendup(eq, e, da, sa, e->data, e->event.datalen, NULL);
	return 0;
}

void
wlc_eventq_flush(wlc_eventq_t *eq)
{
	if (eq == NULL)
		return;

	if (eq->cb)
		eq->cb(eq->wlc);
	if (eq->tpending) {
		wl_del_timer(eq->wl, eq->timer);
		eq->tpending = FALSE;
	}
}
#endif /* !WLNOEIND */

/*
 * Local Functions
 */
static void
wlc_timer_cb(void *arg)
{
	struct wlc_eventq* eq = (struct wlc_eventq*)arg;

	ASSERT(eq->tpending == TRUE);
	ASSERT(wlc_eventq_avail(eq) == TRUE);
	ASSERT(eq->workpending == FALSE);
	eq->workpending = TRUE;

	if (eq->cb)
		eq->cb(eq->wlc);

	ASSERT(wlc_eventq_avail(eq) == FALSE);
	ASSERT(eq->tpending == TRUE);
	eq->workpending = FALSE;
	eq->tpending = FALSE;
}

#ifndef WLNOEIND
/* Abandonable helper function for PROP_TXSTATUS */
static void
wlc_event_mark_packet(wlc_info_t *wlc, void *p)
{
#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		PKTSETTYPEEVENT(wlc->pub->osh, p);
		/* this is implied for event packets anyway */
		PKTSETNODROP(wlc->pub->osh, p);
	}
#endif // endif
}

void
wlc_assign_event_msg(wlc_info_t *wlc, wl_event_msg_t *msg, const wlc_event_t *e,
                     uint8 *data, uint32 len, uint16 adjlen)
{
	void *databuf;

	ASSERT(msg && e);
	ASSERT((adjlen <= len) && (adjlen <= e->event.datalen));

	/* translate the wlc event into bcm event msg */
	msg->version = hton16(BCM_EVENT_MSG_VERSION);
	msg->event_type = hton32(e->event.event_type);
	msg->status = hton32(e->event.status);
	msg->reason = hton32(e->event.reason);
	msg->auth_type = hton32(e->event.auth_type);
	msg->datalen = hton32(e->event.datalen - adjlen);
	msg->flags = hton16(e->event.flags);
	bzero(msg->ifname, sizeof(msg->ifname));
	strncpy(msg->ifname, e->event.ifname, sizeof(msg->ifname) - 1);
	msg->ifidx = e->event.ifidx;
	msg->bsscfgidx = e->event.bsscfgidx;

	if (e->addr)
		bcopy(e->event.addr.octet, msg->addr.octet, ETHER_ADDR_LEN);

	databuf = (char *)(msg + 1);
	if (len)
		bcopy(data, databuf, len - adjlen);
}

/* Checks for optional bssinfo at the end, returns the length that can be stripped */
static uint16
wlc_event_strip_bssinfo(wlc_eventq_t *eq, const wlc_event_t *e, uint8 *data, uint32 len)
{
	wlc_bsscfg_t *cfg;

	cfg = wlc_bsscfg_find_by_wlcif(eq->wlc, e->wlcif);
	if (cfg == NULL)
		return 0;

	if ((len > (BRCM_PROP_IE_LEN + sizeof(wl_bss_info_t))) &&
	    (wlc_event_needs_bssinfo_tlv(e->event.event_type, cfg) != NULL)) {
		uint32 bssinfo_off = len - (BRCM_PROP_IE_LEN + sizeof(wl_bss_info_t));
		brcm_prop_ie_t *event_ie = (brcm_prop_ie_t*)(data + bssinfo_off);

		/* Check if bssinfo tlv is present at the end of eventdata. */
		if ((event_ie->id == DOT11_MNG_PROPR_ID) &&
		    (event_ie->len == (BRCM_PROP_IE_LEN - TLV_HDR_LEN) + sizeof(wl_bss_info_t)) &&
		    !memcmp(event_ie->oui, BRCM_PROP_OUI, DOT11_OUI_LEN) &&
		    (event_ie->type == BRCM_EVT_WL_BSS_INFO)) {
			return (BRCM_PROP_IE_LEN + sizeof(wl_bss_info_t));
		}
	}

	/* No stripping allowed */
	return 0;
}

/* Placeholder to allow requeuing */
static int
wlc_event_tracking(wlc_eventq_t *eq, const wlc_event_t *e, bool success)
{
	return BCME_OK;
}

static int
wlc_event_sendup(wlc_eventq_t *eq, const wlc_event_t *e,
	struct ether_addr *da, struct ether_addr *sa, uint8 *data, uint32 len, void *p)
{
	wlc_info_t *wlc = eq->wlc;
	char *ptr;
	bcm_event_t *msg;
	uint pktlen;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;
	uint16 adjlen = 0;
	bool use_alloced_pkt = FALSE;

	BCM_REFERENCE(wlc);

	ASSERT(e != NULL);
	ASSERT(e->wlcif != NULL);

#if defined(EXT_STA) && !defined(DONGLEBUILD)
	if (WLEXTSTA_ENAB(wlc->pub)) {
		wl_event_sendup(eq->wl, e, data, len);
		return BCME_OK;
	}
#endif // endif
	if (!p) {
		use_alloced_pkt = TRUE;
		pktlen = (BCMEXTRAHDROOM + 2) + sizeof(bcm_event_t) + len + 2;
		if ((p = wlc_event_pktget(eq, wlc->osh, pktlen, e->event.event_type)) == NULL) {
			if (EVDATA_BSSINFO_ENAB(wlc->pub)) {
				/* If we can remove optional info, try again */
				adjlen = wlc_event_strip_bssinfo(eq, e, data, len);
				ASSERT((adjlen <= len) && (adjlen <= e->event.datalen));
				if (adjlen != 0) {
					p = wlc_event_pktget(eq, wlc->osh, pktlen - adjlen,
						e->event.event_type);
				}
			}

			if (p == NULL) {
				WL_ERROR(("wl%d: wlc_event_sendup: failed to get a pkt\n",
				          wlc->pub->unit));
				return wlc_event_tracking(eq, e, FALSE);
			}
		} else {
			wlc_event_tracking(eq, e, TRUE);
		}
		ASSERT(ISALIGNED(PKTDATA(wlc->osh, p), sizeof(uint32)));
		/* make room for headers; ensure we start on an odd 16 bit offset */
		PKTPULL(wlc->osh, p, BCMEXTRAHDROOM + 2);
	}
	msg = (bcm_event_t *) PKTDATA(wlc->osh, p);

	bcopy(da, &msg->eth.ether_dhost, ETHER_ADDR_LEN);
	bcopy(sa, &msg->eth.ether_shost, ETHER_ADDR_LEN);

	/* Set the locally administered bit on the source mac address if both
	 * SRC and DST mac addresses are the same. This prevents the downstream
	 * bridge from dropping the packet.
	 * Clear it if both addresses are the same and it's already set.
	 */
	if (!bcmp(&msg->eth.ether_shost, &msg->eth.ether_dhost, ETHER_ADDR_LEN))
		ETHER_TOGGLE_LOCALADDR(&msg->eth.ether_shost);

	msg->eth.ether_type = hton16(ETHER_TYPE_BRCM);

	/* BCM Vendor specific header... */
	msg->bcm_hdr.subtype = hton16(BCMILCP_SUBTYPE_VENDOR_LONG);
	msg->bcm_hdr.version = BCMILCP_BCM_SUBTYPEHDR_VERSION;
	bcopy(BRCM_OUI, &msg->bcm_hdr.oui[0], DOT11_OUI_LEN);
	/* vendor spec header length + pvt data length (private indication
	 * hdr + actual message itself)
	 */
	msg->bcm_hdr.length = hton16(BCMILCP_BCM_SUBTYPEHDR_MINLENGTH +
	                             BCM_MSG_LEN +
	                             (uint16)len - adjlen);
	msg->bcm_hdr.usr_subtype = hton16(BCMILCP_BCM_SUBTYPE_EVENT);

	/* update the event struct */
	if (use_alloced_pkt)
		wlc_assign_event_msg(wlc, &msg->event, e, data, len, adjlen);
	else
		wlc_assign_event_msg(wlc, &msg->event, e, NULL, 0, 0);

	/* fixup lengths */
	msg->bcm_hdr.length = ntoh16(msg->bcm_hdr.length);
	msg->bcm_hdr.length += sizeof(wl_event_msg_t);
	msg->bcm_hdr.length = hton16(msg->bcm_hdr.length);

	PKTSETLEN(wlc->osh, p, (sizeof(bcm_event_t) + len + 2));

	ptr = (char *)(msg + 1);
	/* Last 2 bytes of the message are 0x00 0x00 to signal that there are
	 * no ethertypes which are following this
	 */
	ptr[len + 0] = 0x00;
	ptr[len + 1] = 0x00;

	wlc_event_mark_packet(wlc, p);

	cfg = wlc_bsscfg_find_by_wlcif(wlc, e->wlcif);
	ASSERT(cfg != NULL);

	if (e->wlcif->type == WLC_IFTYPE_WDS)
		scb = e->wlcif->u.scb;
	wlc_sendup(wlc, cfg, scb, p);

	return BCME_OK;
}

/* This function is used to send Event using passed packet buffer */

/* If return is not BCME_OK, caller responsible to free the packet p; else no need to free p */
int
wlc_event_pkt_prep_send(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
                          const struct ether_addr* addr,
                          uint result, uint status, uint auth_type, void *data, int datalen,
                          wl_event_rx_frame_data_t *rxframe_data, void *p)
{
	wlc_event_t *e;
	bcm_event_t *be;
	struct ether_addr addr_copy;

	if (!wlc_eventq_test_ind(wlc->eventq, msg))
		return BCME_ERROR;
	ASSERT((uint)PKTHEADROOM(wlc->osh, p) >=
		((rxframe_data ? sizeof(wl_event_rx_frame_data_t) : 0) +
		sizeof(bcm_event_t)));
	if ((uint)PKTHEADROOM(wlc->osh, p) < (BCMEXTRAHDROOM + 2))
		return BCME_ERROR;
	e = (wlc_event_t *)MALLOC(wlc->osh, sizeof(wlc_event_t));
	if (!e)
		return BCME_ERROR;
	/* Store address in temp buffer as its part of pkt which we will be overwritten
	 * with bcm hdr
	 */
	bcopy(addr->octet, addr_copy.octet, ETHER_ADDR_LEN);

	/* Copy over rxframe data */
	if (rxframe_data) {
		PKTPUSH(wlc->osh, p, sizeof(wl_event_rx_frame_data_t));
		bcopy(rxframe_data, PKTDATA(wlc->osh, p), sizeof(wl_event_rx_frame_data_t));
		datalen += sizeof(wl_event_rx_frame_data_t);
	}
	/* Point to bcm_event_t before calling sendup */
	PKTPUSH(wlc->pub->osh, p, sizeof(bcm_event_t));
	be = (bcm_event_t *)PKTDATA(wlc->osh, p);
	/* Clear up bcm event info */
	bzero(be, sizeof(bcm_event_t));
	bzero(e, sizeof(wlc_event_t));
	/* Update event info */
	e->event.event_type = msg;
	e->event.status = result;
	e->event.reason = status;
	e->event.auth_type = auth_type;
	e->event.datalen = datalen;
	wlc_event_if(wlc, bsscfg, e, &addr_copy);

	wlc_event_sendup(wlc->eventq, e, &bsscfg->cur_etheraddr, &bsscfg->cur_etheraddr,
		NULL, e->event.datalen, p);

	MFREE(wlc->osh, e, sizeof(wlc_event_t));
	return BCME_OK;
}

#if defined(MSGTRACE) || defined(LOGTRACE)
void
wlc_event_sendup_trace(wlc_info_t *wlc, hndrte_dev_t *wl_rtedev, uint8 *hdr, uint16 hdrlen,
                       uint8 *buf, uint16 buflen)
{
	void *p;
	bcm_event_t *msg;
	char *ptr, *databuf;
	struct lbuf *lb;
	uint16 len;
	osl_t *osh = wlc->osh;
	hndrte_dev_t *busdev = wl_rtedev->chained;

	if (busdev == NULL)
		return;

	if (! wlc_eventq_test_ind(wlc->eventq, WLC_E_TRACE))
		return;

	len = hdrlen + buflen;
	ASSERT(len < (wlc->pub->tunables->rxbufsz - sizeof(bcm_event_t) - 2));

	if ((p = PKTGET(osh, wlc->pub->tunables->rxbufsz, FALSE)) == NULL) {
		return;
	}

	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));

	/* make room for headers; ensure we start on an odd 16 bit offset */
	PKTPULL(osh, p, BCMEXTRAHDROOM + 2);

	msg = (bcm_event_t *) PKTDATA(osh, p);

	msg->eth.ether_type = hton16(ETHER_TYPE_BRCM);

	/* BCM Vendor specific header... */
	msg->bcm_hdr.subtype = hton16(BCMILCP_SUBTYPE_VENDOR_LONG);
	msg->bcm_hdr.version = BCMILCP_BCM_SUBTYPEHDR_VERSION;
	bcopy(BRCM_OUI, &msg->bcm_hdr.oui[0], DOT11_OUI_LEN);
	/* vendor spec header length + pvt data length (private indication hdr + actual message
	 * itself)
	 */
	msg->bcm_hdr.length = hton16(BCMILCP_BCM_SUBTYPEHDR_MINLENGTH + BCM_MSG_LEN + (uint16)len);
	msg->bcm_hdr.usr_subtype = hton16(BCMILCP_BCM_SUBTYPE_EVENT);

	PKTSETLEN(osh, p, (sizeof(bcm_event_t) + len + 2));

	/* update the event struct */
	/* translate the wlc event into bcm event msg */
	msg->event.version = hton16(BCM_EVENT_MSG_VERSION);
	msg->event.event_type = hton32(WLC_E_TRACE);
	msg->event.status = hton32(WLC_E_STATUS_SUCCESS);
	msg->event.reason = 0;
	msg->event.auth_type = 0;
	msg->event.datalen = hton32(len);
	msg->event.flags = 0;
	bzero(msg->event.ifname, sizeof(msg->event.ifname));

	/* fixup lengths */
	msg->bcm_hdr.length = ntoh16(msg->bcm_hdr.length);
	msg->bcm_hdr.length += sizeof(wl_event_msg_t);
	msg->bcm_hdr.length = hton16(msg->bcm_hdr.length);

	PKTSETLEN(osh, p, (sizeof(bcm_event_t) + len + 2));

	/* Copy the data */
	databuf = (char *)(msg + 1);
	bcopy(hdr, databuf, hdrlen);
	bcopy(buf, databuf+hdrlen, buflen);

	ptr = (char *)databuf;

	PKTSETMSGTRACE(p, TRUE);

	/* Last 2 bytes of the message are 0x00 0x00 to signal that there are no ethertypes which
	 * are following this
	 */
	ptr[len+0] = 0x00;
	ptr[len+1] = 0x00;
	lb = PKTTONATIVE(osh, p);

	if (busdev->funcs->xmit(NULL, busdev, lb) != 0) {
		lb_free(lb);
	}
}
#endif /* MSGTRACE */

int
wlc_eventq_set_ind(wlc_eventq_t* eq, uint et, bool enab)
{
	if (et >= WLC_E_LAST)
		return -1;
	if (enab)
		setbit(eq->event_inds_mask, et);
	else
		clrbit(eq->event_inds_mask, et);

	if (et == WLC_E_PROBREQ_MSG)
		wlc_enable_probe_req(eq->wlc, PROBE_REQ_EVT_MASK, enab? PROBE_REQ_EVT_MASK:0);

	return 0;
}
#endif /* !WLNOEIND */

#if defined(BCMDBG) || defined(WLMSG_INFORM)
void
wlc_print_event(wlc_info_t *wlc, wlc_event_t *e)
{
	uint msg = e->event.event_type;
	struct ether_addr *addr = e->addr;
	uint result = e->event.status;
	char eabuf[ETHER_ADDR_STR_LEN];

	uint auth_type = e->event.auth_type;
	const char *auth_str;
	const char *event_name;
	uint status = e->event.reason;
	char ssidbuf[SSID_FMT_BUF_LEN];

	event_name = bcmevent_get_name(msg);

	if (addr != NULL)
		bcm_ether_ntoa(addr, eabuf);
	else
		strncpy(eabuf, "<NULL>", 7);

	switch (msg) {
	case WLC_E_START:
	case WLC_E_DEAUTH:
	case WLC_E_ASSOC_IND:
	case WLC_E_REASSOC_IND:
	case WLC_E_DISASSOC:
	case WLC_E_EAPOL_MSG:
	case WLC_E_BCNRX_MSG:
	case WLC_E_BCNSENT_IND:
	case WLC_E_ROAM_PREP:
	case WLC_E_BCNLOST_MSG:
	case WLC_E_PROBREQ_MSG:
#ifdef WLP2P
	case WLC_E_PROBRESP_MSG:
	case WLC_E_P2P_PROBREQ_MSG:
#endif // endif
#ifdef BCMWAPI_WAI
	case WLC_E_WAI_STA_EVENT:
	case WLC_E_WAI_MSG:
#endif /* BCMWAPI_WAI */
#if defined(NDIS) && (NDISVER >= 0x0620)
	case WLC_E_ASSOC_IND_NDIS:
	case WLC_E_REASSOC_IND_NDIS:
	case WLC_E_IBSS_COALESCE:
#endif /* NDIS && (NDISVER >= 0x0620) */
	case WLC_E_AUTHORIZED:
	case WLC_E_PROBREQ_MSG_RX:
		WL_INFORM(("wl%d: MACEVENT: %s, MAC %s\n",
		           WLCWLUNIT(wlc), event_name, eabuf));
		break;

	case WLC_E_ASSOC:
	case WLC_E_REASSOC:
		if (result == WLC_E_STATUS_SUCCESS) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, SUCCESS\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_TIMEOUT) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, TIMEOUT\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_ABORT) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, ABORT\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_NO_ACK) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, NO_ACK\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_UNSOLICITED) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, UNSOLICITED\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_FAIL) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, FAILURE, status %d\n",
			           WLCWLUNIT(wlc), event_name, eabuf, (int)status));
		} else {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, unexpected status %d\n",
			           WLCWLUNIT(wlc), event_name, eabuf, (int)result));
		}
		break;

	case WLC_E_DEAUTH_IND:
	case WLC_E_DISASSOC_IND:
		WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, reason %d\n",
		           WLCWLUNIT(wlc), event_name, eabuf, (int)status));
		break;

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	case WLC_E_AUTH:
	case WLC_E_AUTH_IND: {
		char err_msg[32];

		if (auth_type == DOT11_OPEN_SYSTEM)
			auth_str = "Open System";
		else if (auth_type == DOT11_SHARED_KEY)
			auth_str = "Shared Key";
		else {
			snprintf(err_msg, sizeof(err_msg), "AUTH unknown: %d", (int)auth_type);
			auth_str = err_msg;
		}

		if (msg == WLC_E_AUTH_IND) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, %s\n",
			           WLCWLUNIT(wlc), event_name, eabuf, auth_str));
		} else if (result == WLC_E_STATUS_SUCCESS) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, %s, SUCCESS\n",
			           WLCWLUNIT(wlc), event_name, eabuf, auth_str));
		} else if (result == WLC_E_STATUS_TIMEOUT) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, %s, TIMEOUT\n",
			           WLCWLUNIT(wlc), event_name, eabuf, auth_str));
		} else if (result == WLC_E_STATUS_FAIL) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, %s, FAILURE, status %d\n",
			           WLCWLUNIT(wlc), event_name, eabuf, auth_str, (int)status));
		}
		break;
	}
#endif /* BCMDBG || WLMSG_INFORM */
	case WLC_E_JOIN:
	case WLC_E_ROAM:
	case WLC_E_BSSID:
	case WLC_E_SET_SSID:
		if (result == WLC_E_STATUS_SUCCESS) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_FAIL) {
			WL_INFORM(("wl%d: MACEVENT: %s, failed\n",
			           WLCWLUNIT(wlc), event_name));
		} else if (result == WLC_E_STATUS_NO_NETWORKS) {
			WL_INFORM(("wl%d: MACEVENT: %s, no networks found\n",
			           WLCWLUNIT(wlc), event_name));
		} else if (result == WLC_E_STATUS_ABORT) {
			WL_INFORM(("wl%d: MACEVENT: %s, ABORT\n",
			           wlc->pub->unit, event_name));
		} else {
			WL_INFORM(("wl%d: MACEVENT: %s, unexpected status %d\n",
			           WLCWLUNIT(wlc), event_name, (int)result));
		}
		break;

	case WLC_E_BEACON_RX:
		if (result == WLC_E_STATUS_SUCCESS) {
			WL_INFORM(("wl%d: MACEVENT: %s, SUCCESS\n",
			           WLCWLUNIT(wlc), event_name));
		} else if (result == WLC_E_STATUS_FAIL) {
			WL_INFORM(("wl%d: MACEVENT: %s, FAIL\n",
			           WLCWLUNIT(wlc), event_name));
		} else {
			WL_INFORM(("wl%d: MACEVENT: %s, result %d\n",
			           WLCWLUNIT(wlc), event_name, result));
		}
		break;

	case WLC_E_NDIS_LINK:
#ifdef EXT_STA
		WL_INFORM(("wl%d: MACEVENT: LINK_QUALITY_INDICATION\n",
		           WLCWLUNIT(wlc)));
		break;
#endif /* EXT_STA */

	case WLC_E_LINK:
		WL_INFORM(("wl%d: MACEVENT: %s %s\n",
		           WLCWLUNIT(wlc), event_name,
		           (e->event.flags&WLC_EVENT_MSG_LINK)?"UP":"DOWN"));
		break;

	case WLC_E_MIC_ERROR:
		WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, Group: %s, Flush Q: %s\n",
		           WLCWLUNIT(wlc), event_name, eabuf,
		           (e->event.flags&WLC_EVENT_MSG_GROUP)?"Yes":"No",
		           (e->event.flags&WLC_EVENT_MSG_FLUSHTXQ)?"Yes":"No"));
		break;

	case WLC_E_ICV_ERROR:
	case WLC_E_UNICAST_DECODE_ERROR:
	case WLC_E_MULTICAST_DECODE_ERROR:
		WL_INFORM(("wl%d: MACEVENT: %s, MAC %s\n",
		           WLCWLUNIT(wlc), event_name, eabuf));
		break;

	case WLC_E_TXFAIL:
		/* TXFAIL messages are too numerous for WL_INFORM() */
		break;

	case WLC_E_COUNTRY_CODE_CHANGED: {
		char cstr[16];
		memset(cstr, 0, sizeof(cstr));
		memcpy(cstr, e->data, MIN(e->event.datalen, sizeof(cstr) - 1));
		WL_INFORM(("wl%d: MACEVENT: %s New Country: %s\n", WLCWLUNIT(wlc),
		           event_name, cstr));
		break;
	}

	case WLC_E_RETROGRADE_TSF:
		WL_INFORM(("wl%d: MACEVENT: %s, MAC %s\n",
		           WLCWLUNIT(wlc), event_name, eabuf));
		break;

#ifdef WIFI_ACT_FRAME
	case WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE:
#endif // endif
	case WLC_E_SCAN_COMPLETE:
		if (result == WLC_E_STATUS_SUCCESS) {
			WL_INFORM(("wl%d: MACEVENT: %s, SUCCESS\n",
			           WLCWLUNIT(wlc), event_name));
		} else if (result == WLC_E_STATUS_ABORT) {
			WL_INFORM(("wl%d: MACEVENT: %s, ABORTED\n",
			           WLCWLUNIT(wlc), event_name));
		} else {
			WL_INFORM(("wl%d: MACEVENT: %s, result %d\n",
			           WLCWLUNIT(wlc), event_name, result));
		}
		break;

	case WLC_E_AUTOAUTH:
		WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, result %d\n",
		           WLCWLUNIT(wlc), event_name, eabuf, (int)result));
		break;

	case WLC_E_ADDTS_IND:
		if (result == WLC_E_STATUS_SUCCESS) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, SUCCESS\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_TIMEOUT) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, TIMEOUT\n",
			           WLCWLUNIT(wlc), event_name, eabuf));
		} else if (result == WLC_E_STATUS_FAIL) {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, FAILURE, status %d\n",
			           WLCWLUNIT(wlc), event_name, eabuf, (int)status));
		} else {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, unexpected status %d\n",
			           WLCWLUNIT(wlc), event_name, eabuf, (int)result));
		}
		break;

	case WLC_E_DELTS_IND:
		if (result == WLC_E_STATUS_SUCCESS) {
			WL_INFORM(("wl%d: MACEVENT: %s success ...\n",
			           WLCWLUNIT(wlc), event_name));
		} else if (result == WLC_E_STATUS_UNSOLICITED) {
			WL_INFORM(("wl%d: MACEVENT: DELTS unsolicited %s\n",
			           WLCWLUNIT(wlc), event_name));
		} else {
			WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, unexpected status %d\n",
			           WLCWLUNIT(wlc), event_name, eabuf, (int)result));
		}
		break;

	case WLC_E_PFN_NET_FOUND:
	case WLC_E_PFN_NET_LOST:
		WL_INFORM(("wl%d: PFNEVENT: %s, SSID %s, SSID len %d\n",
		           WLCWLUNIT(wlc), event_name,
		           (wlc_format_ssid(ssidbuf, e->data, e->event.datalen), ssidbuf),
		           e->event.datalen));
		break;

	case WLC_E_PSK_SUP:
		WL_INFORM(("wl%d: MACEVENT: %s, state %d, reason %d\n", WLCWLUNIT(wlc),
		           event_name, result, status));
		break;

#if defined(IBSS_PEER_DISCOVERY_EVENT)
	case WLC_E_IBSS_ASSOC:
		WL_INFORM(("wl%d: MACEVENT: %s, PEER %s\n", WLCWLUNIT(wlc), event_name, eabuf));
		break;
#endif /* defined(IBSS_PEER_DISCOVERY_EVENT) */

#ifdef EXT_STA
	case WLC_E_RESET_COMPLETE:
		WL_INFORM(("wl%d: MACEVENT: %s, SUCCESS\n", WLCWLUNIT(wlc), event_name));
		break;

	case WLC_E_ASSOC_START:
		WL_INFORM(("wl%d: MACEVENT: %s, BSSID %s\n", WLCWLUNIT(wlc), event_name, eabuf));
		break;
#endif /* EXT_STA */

	case WLC_E_PSM_WATCHDOG:
		WL_INFORM(("wl%d: MACEVENT: %s, psmdebug 0x%x, phydebug 0x%x, psm_brc 0x%x\n",
		           WLCWLUNIT(wlc), event_name, result, status, auth_type));
		break;

	case WLC_E_TRACE:
		/* We don't want to trace the trace event */
		break;

#ifdef WIFI_ACT_FRAME
	case WLC_E_ACTION_FRAME_COMPLETE:
		WL_INFORM(("wl%d: MACEVENT: %s status: %s\n", WLCWLUNIT(wlc), event_name,
		           (result == WLC_E_STATUS_NO_ACK?"NO ACK":"ACK")));
		break;
#endif /* WIFI_ACT_FRAME */

	/*
	 * Events that don't require special decoding
	 */
	case WLC_E_ASSOC_REQ_IE:
	case WLC_E_ASSOC_RESP_IE:
	case WLC_E_PMKID_CACHE:
	case WLC_E_PRUNE:
	case WLC_E_RADIO:
	case WLC_E_IF:
	case WLC_E_EXTLOG_MSG:
	case WLC_E_RSSI:
	case WLC_E_ESCAN_RESULT:
	case WLC_E_PFN_SCAN_COMPLETE:
	case WLC_E_DCS_REQUEST:
	case WLC_E_CSA_COMPLETE_IND:
#ifdef EXT_STA
	case WLC_E_JOIN_START:
	case WLC_E_ROAM_START:
#endif // endif
#if defined(BCMCCX) && defined(CCX_SDK)
	case WLC_E_CCX_ASSOC_START:
	case WLC_E_CCX_ASSOC_ABORT:
#endif // endif
#ifdef WLBTAMP
	case WLC_E_BTA_HCI_EVENT:
#endif // endif
#ifdef WIFI_ACT_FRAME
	case WLC_E_ACTION_FRAME:
	case WLC_E_ACTION_FRAME_RX:
#endif // endif
#ifdef WLP2P
	case WLC_E_P2P_DISC_LISTEN_COMPLETE:
#endif // endif
	case WLC_E_PRE_ASSOC_IND:
	case WLC_E_PRE_REASSOC_IND:
#if defined(NDIS) && (NDISVER >= 0x0620)
	case WLC_E_AP_STARTED:
	case WLC_E_DFS_AP_STOP:
	case WLC_E_DFS_AP_RESUME:
#endif // endif
#if defined(NDIS) && (NDISVER >= 0x0630)
	case WLC_E_ACTION_FRAME_RX_NDIS:
	case WLC_E_AUTH_REQ:
	case WLC_E_SPEEDY_RECREATE_FAIL:
	case WLC_E_ASSOC_RECREATED:
#endif /* NDIS && (NDISVER >= 0x0630) */
#ifdef PROP_TXSTATUS
	case WLC_E_FIFO_CREDIT_MAP:
	case WLC_E_BCMC_CREDIT_SUPPORT:
#endif // endif
#ifdef P2PO
	case WLC_E_SERVICE_FOUND:
	case WLC_E_P2PO_ADD_DEVICE:
	case WLC_E_P2PO_DEL_DEVICE:
#endif // endif
#if defined(P2PO) || defined(ANQPO)
	case WLC_E_GAS_FRAGMENT_RX:
	case WLC_E_GAS_COMPLETE:
#endif // endif
	case WLC_E_WAKE_EVENT:
	case WLC_E_NATIVE:
#ifdef WLAWDL
	case WLC_E_AWDL_AW_EXT_END:
	case WLC_E_AWDL_AW_EXT_START:
	case WLC_E_AWDL_AW_START:
	case WLC_E_AWDL_RADIO_OFF:
	case WLC_E_AWDL_PEER_STATE:
	case WLC_E_AWDL_SYNC_STATE_CHANGED:
	case WLC_E_AWDL_CHIP_RESET:
	case WLC_E_AWDL_INTERLEAVED_SCAN_START:
	case WLC_E_AWDL_INTERLEAVED_SCAN_STOP:
	case WLC_E_AWDL_PEER_CACHE_CONTROL:
#endif // endif
#if defined(WLBSSLOAD_REPORT)
	case WLC_E_BSS_LOAD:
#endif // endif
	case WLC_E_PRE_ASSOC_RSEP_IND:
		WL_INFORM(("wl%d: MACEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
#ifdef WLTDLS
	case WLC_E_TDLS_PEER_EVENT:
		WL_INFORM(("wl%d: MACEVENT: %s, MAC %s, reason %d\n",
			WLCWLUNIT(wlc), event_name, eabuf, (int)status));
		break;
#endif /* WLTDLS */
#if defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND)
	case WLC_E_PKTDELAY_IND:
		WL_INFORM(("wl%d: MACEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
#endif /* defined(WLPKTDLYSTAT) && defined(WLPKTDLYSTAT_IND) */
	case WLC_E_PSTA_PRIMARY_INTF_IND:
		WL_INFORM(("wl%d: MACEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
#if defined(WL_PROXDETECT)
	case WLC_E_PROXD:
		WL_INFORM(("wl%d: MACEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
#endif /* defined(WL_PROXDETECT) */

	case WLC_E_CCA_CHAN_QUAL:
		WL_INFORM(("wl%d: MACEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
#ifdef WLINTFERSTAT
	case WLC_E_TXFAIL_THRESH:
		WL_INFORM(("wl%d: MACEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
#endif  /* WLINTFERSTAT */
	case WLC_E_RMC_EVENT:
		WL_INFORM(("wl%d: MACEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
	case WLC_E_DPSTA_INTF_IND:
		WL_INFORM(("wl%d: DPSTAEVENT: %s\n", WLCWLUNIT(wlc), event_name));
		break;
	default:
		WL_ASSOC(("wl%d: MACEVENT: UNSUPPORTED %d, MAC %s, result %d, status %d,"
			" auth %d\n", WLCWLUNIT(wlc), msg, eabuf, (int)result, (int)status,
			(int)auth_type));
		break;
	}
}
#endif /* BCMDBG || BCMDBG_ERR */
