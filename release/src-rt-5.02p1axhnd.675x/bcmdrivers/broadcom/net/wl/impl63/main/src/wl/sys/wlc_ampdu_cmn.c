/**
 * A-MPDU (with extended Block Ack protocol) source file
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_ampdu_cmn.c 779875 2019-10-09 10:31:01Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [AmpduUcode] [AmpduAQM]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifndef WLAMPDU
#error "WLAMPDU is not defined"
#endif	/* WLAMPDU */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_scb.h>
#include <wlc_frmutil.h>
#ifdef AP
#include <wlc_apps.h>
#endif // endif
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#endif // endif
#if defined(EVENT_LOG_COMPILE)
#include <event_log.h>
#if defined(ECOUNTERS)
#include <ecounters.h>
#endif // endif
#endif /* EVENT_LOG_COMPILE */
#include <wlc_scb_ratesel.h>
#include <wl_export.h>
#include <wlc_pm.h>
#include <wlc_dump.h>
#include <wlc_musched.h>
#include <wlc_he.h>

#ifdef WLCNT
#if defined(BCMDBG) || defined(WLTEST) || defined(WLPKTDLYSTAT) || \
	defined(BCMDBG_AMPDU)
#define WLC_AMPDU_DUMP_CLR
#endif // endif
#endif /* WLCNT */

#ifdef BCMDBG
uint32 wl_ampdu_dbg = WL_AMPDU_ERR_VAL;
#endif // endif

/** iovar table */
enum {
	IOV_AMPDU,		/* enable/disable ampdu */
	IOV_AMPDU_BA_WSIZE,	/* ampdu ba window size :kept for backward compatibility */
	IOV_AMPDU_DENSITY,	/* ampdu density */
	IOV_ACTIVATE_TEST,
	IOV_AMPDU_DBG,
	IOV_AMPDU_COUNTERS,	 /* counters */
	IOV_AMPDU_LAST
};

static const bcm_iovar_t ampdu_iovars[] = {
	{"ampdu", IOV_AMPDU, (IOVF_SET_DOWN|IOVF_RSDB_SET), 0, IOVT_BOOL, 0},	/* only if down */
	{"ampdu_ba_wsize", IOV_AMPDU_BA_WSIZE, (0), 0, IOVT_INT8, 0},
	{"ampdu_density", IOV_AMPDU_DENSITY, (0), 0, IOVT_UINT8, 0},
#ifdef BCMDBG
	{"ampdu_dbg", IOV_AMPDU_DBG, (0), 0, IOVT_UINT32, 0},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

static int wlc_ampdu_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
#if defined(EVENT_LOG_COMPILE)
#if defined(ECOUNTERS)
static int wlc_ampdu_ecounter_dump(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len);
#endif // endif
static void wlc_ampdu_chansw_update(void *arg, wlc_chansw_notif_data_t *data);
#endif /* EVENT_LOG_COMPILE */
#endif // endif

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

#ifdef WLC_AMPDU_DUMP
static int
wlc_ampdu_dump_tx(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;

	if (wlc->ampdu_tx != NULL) {
		wlc_ampdu_tx_dump(wlc->ampdu_tx, b);
	}
	return 0;
}

static int
wlc_ampdu_dump_tx_scb(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;
	if (wlc->ampdu_tx != NULL) {
		wlc_ampdu_tx_scb_dump(wlc->ampdu_tx, b);
	}
	return 0;
}
static int
wlc_ampdu_dump_rx(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;

	if (wlc->ampdu_rx != NULL) {
		wlc_ampdu_rx_dump(wlc->ampdu_rx, b);
	}
	return 0;
}

static int
wlc_ampdu_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = ctx;

#ifndef BCMDBG_AMPDU_NO_DUMP_IOVAR
	if (wlc->ampdu_tx != NULL) {
		wlc_ampdu_tx_dump(wlc->ampdu_tx, b);
		wlc_ampdu_tx_scb_dump(wlc->ampdu_tx, b);
	}
	if (wlc->ampdu_rx != NULL)
		wlc_ampdu_rx_dump(wlc->ampdu_rx, b);
#elif defined(EVENT_LOG_COMPILE)
#if defined(ECOUNTERS)
	/* Dump AMPDU stats through event log + ecounter
	 * In case of no ecounter support, try to dump only the MCS rate counters
	 */
	if (EVENT_LOG_IS_ON(EVENT_LOG_TAG_AMPDU_DUMP)) {
		wlc_ampdu_ecounter_dump(EVENT_LOG_TAG_AMPDU_DUMP, wlc);
	} else
#endif /* ECOUNTERS */
	if (EVENT_LOG_IS_ON(EVENT_LOG_TAG_RATE_CNT)) {
		wlc_ampdu_rxmcs_counter_report(wlc->ampdu_rx, EVENT_LOG_TAG_RATE_CNT);
		wlc_ampdu_txmcs_counter_report(wlc->ampdu_tx, EVENT_LOG_TAG_RATE_CNT);
	}
#endif /* !BCMDBG_AMPDU_NO_DUMP_IOVAR && EVENT_LOG_COMPILE */

	return 0;
}

#ifdef WLC_AMPDU_DUMP_CLR

static int
wlc_ampdu_dump_clr(void *ctx)
{
	wlc_info_t *wlc = ctx;

	wlc_ampdu_clear_tx_dump(wlc->ampdu_tx);
	wlc_ampdu_clear_rx_dump(wlc->ampdu_rx);

	return BCME_OK;
}
#else
#define wlc_ampdu_dump_clr NULL
#endif // endif
#endif /* WLC_AMPDU_DUMP */

int
BCMATTACHFN(wlc_ampdu_init)(wlc_info_t *wlc)
{
#ifdef WLC_AMPDU_DUMP
	wlc_dump_add_fns(wlc->pub, "ampdu", wlc_ampdu_dump, wlc_ampdu_dump_clr, wlc);
	wlc_dump_add_fns(wlc->pub, "ampdu_tx", wlc_ampdu_dump_tx, NULL, wlc);
	wlc_dump_add_fns(wlc->pub, "ampdu_tx_scb", wlc_ampdu_dump_tx_scb, NULL, wlc);
	wlc_dump_add_fns(wlc->pub, "ampdu_rx", wlc_ampdu_dump_rx, NULL, wlc);
#ifdef WLWRR
	wlc_dump_add_fns(wlc->pub, "ampdu_wrr", wlc_ampdu_dump_wrr, NULL, wlc);
#endif /* WLWRR */
#endif /* WLC_AMPDU_DUMP */
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
#if defined(EVENT_LOG_COMPILE)
#if defined(ECOUNTERS)
	if (ECOUNTERS_ENAB() && ((RSDB_ENAB(wlc->pub) && wlc->pub->unit == MAC_CORE_UNIT_0) ||
		!RSDB_ENAB(wlc->pub)) &&
		wl_ecounters_register_source(WL_IFSTATS_XTLV_WL_SLICE_TX_AMPDU_DUMP,
			wlc_ampdu_ecounter_dump, (void *)wlc)) {
		WL_ERROR(("wl%d: ampdu ecounters_register_source() failed\n", wlc->pub->unit));
		goto fail;
	}
	if (ECOUNTERS_ENAB() && ((RSDB_ENAB(wlc->pub) && wlc->pub->unit == MAC_CORE_UNIT_0) ||
		!RSDB_ENAB(wlc->pub)) &&
		wl_ecounters_register_source(WL_IFSTATS_XTLV_WL_SLICE_RX_AMPDU_DUMP,
			wlc_ampdu_ecounter_dump, (void *)wlc)) {
		WL_ERROR(("wl%d: ampdu ecounters_register_source() failed\n", wlc->pub->unit));
		goto fail;
	}
#endif /* ECOUNTERS */

	/* In case of RDSB register on both slices */
	if (wlc_chansw_notif_register(wlc, wlc_ampdu_chansw_update, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: ampdu wlc_chansw_notif_unregister() failed\n", wlc->pub->unit));
		goto fail;
	}
#endif /* EVENT_LOG_COMPILE */
#endif // endif

	/* register module */
	if (wlc_module_register(wlc->pub, ampdu_iovars, "ampdu", wlc, wlc_ampdu_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: ampdu wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	return BCME_OK;

fail:
	wlc_ampdu_deinit(wlc);
	return BCME_ERROR;
}

void
BCMATTACHFN(wlc_ampdu_deinit)(wlc_info_t *wlc)
{
	if (!wlc)
		return;

	wlc_module_unregister(wlc->pub, "ampdu", wlc);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
#if defined(EVENT_LOG_COMPILE)
	wlc_chansw_notif_unregister(wlc, wlc_ampdu_chansw_update, wlc);
#endif /* EVENT_LOG_COMPILE */
#endif // endif
}

/** handle AMPDU related iovars */
static int
wlc_ampdu_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *) a;
	int err = 0;
	bool bool_val;

	BCM_REFERENCE(alen);
	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlcif);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {
	case IOV_GVAL(IOV_AMPDU):
		*ret_int_ptr = (int32)(wlc->pub->_ampdu_tx);
		break;

	case IOV_SVAL(IOV_AMPDU):
		if ((err = wlc_ampdu_tx_set(wlc->ampdu_tx, bool_val)))
			return err;
		if ((err = wlc_ampdu_rx_set(wlc->ampdu_rx, bool_val))) {
			wlc_ampdu_tx_set(wlc->ampdu_tx, FALSE);
			return err;
		}
		break;

	case IOV_GVAL(IOV_AMPDU_BA_WSIZE):
		*ret_int_ptr = (int32)wlc_ampdu_tx_get_ba_tx_wsize(wlc->ampdu_tx);
		break;

	case IOV_SVAL(IOV_AMPDU_BA_WSIZE):
		if ((int_val == 0) ||
			(int_val > wlc_ampdu_tx_get_ba_max_tx_wsize(wlc->ampdu_tx)) ||
			(int_val > wlc_ampdu_rx_get_ba_max_rx_wsize(wlc->ampdu_rx))) {
			err = BCME_BADARG;
			break;
		}
		wlc_ampdu_tx_set_ba_tx_wsize(wlc->ampdu_tx, (uint8)int_val);
		wlc_ampdu_rx_set_ba_rx_wsize(wlc->ampdu_rx, (uint8)int_val);

		break;

	case IOV_GVAL(IOV_AMPDU_DENSITY):
		*ret_int_ptr = (int32)wlc_ampdu_rx_get_mpdu_density(wlc->ampdu_rx);
		break;

	case IOV_SVAL(IOV_AMPDU_DENSITY):
		if (int_val > AMPDU_MAX_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}

		if (int_val < AMPDU_DEF_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}
		wlc_ampdu_rx_set_mpdu_density(wlc->ampdu_rx, (uint8)int_val);
		wlc_ampdu_tx_set_mpdu_density(wlc->ampdu_tx, (uint8)int_val);
		wlc_ampdu_update_ie_param(wlc->ampdu_rx);
		break;

#ifdef BCMDBG
	case IOV_GVAL(IOV_AMPDU_DBG):
		*ret_int_ptr = wl_ampdu_dbg;
		break;

	case IOV_SVAL(IOV_AMPDU_DBG):
	        wl_ampdu_dbg = (uint32)int_val;
		break;
#endif /* BCMDBG */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
#if defined(EVENT_LOG_COMPILE)
#if defined(ECOUNTERS)
int wlc_ampdu_ecounter_pack_xtlv(uint16 tag, xtlv_desc_t *xtlv_desc,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, uint16 *attempted_write_len, uint16 index)
{
	struct bcm_xtlvbuf local_xtlvbuf;
	uint16 i = 0;
	int err = BCME_OK;
	xtlv_desc_t *tlv_desc;

	if (*cookie == ECOUNTERS_CLIENT_PARAM_COOKIE_INVALID) {
		i = 0;
	} else {
		i = *cookie;

		if (i > index) {
			/* Illegal configuration. We are done */
			*cookie = ECOUNTERS_CLIENT_PARAM_COOKIE_INVALID;
			err = BCME_ERROR;
			goto fail;
		}
	}
	/* This one needs a container. */
	/* So create a local xtlv buffer context to fill in data */
	bcm_xtlv_buf_init(&local_xtlvbuf,
		(uint8 *) (bcm_xtlv_buf(xtlvbuf) + BCM_XTLV_HDR_SIZE),
		(bcm_xtlv_buf_rlen(xtlvbuf) - BCM_XTLV_HDR_SIZE),
		BCM_XTLV_OPTION_ALIGN32);
	while (i <= index) {
		tlv_desc = &xtlv_desc[i];
		/* Write data in the allocated buffer */
		err = bcm_xtlv_put_data(&local_xtlvbuf,
			tlv_desc->type,
			(const uint8 *) tlv_desc->ptr, tlv_desc->len);
		if (err == BCME_NOMEM || err == BCME_BUFTOOSHORT) {
			uint16 local_len, write_len = 0;
			/* We could not fit in data but did we write anything
			 * meaningful i.e. at least one complete XTLV?
			 */
			local_len = bcm_xtlv_buf_len(&local_xtlvbuf);
			/* These many bytes were "taken" in buffer preparation.
			 * i.e. the XTLV header size is consumed for container
			 * type and length mentioned above
			 */
			write_len = BCM_XTLV_HDR_SIZE;
			/* Yes we wrote at least one XTLV. Then complete the container
			 * and tell how much data could not fit in the buffer provided.
			 */
			if (local_len) {
				/* Complete the outer container with type and length
				 * only.
				 */
				bcm_xtlv_put_data(xtlvbuf, tag, NULL, local_len);
			}

			/* buf too short happened when this much length was about
			 * to be written to buffer passed in xtlv context.
			 */
			/* Failed to write complete TLV so note it */
			/* Ask some more space as we also need to fill the container
			 * type and length.
			 */
			write_len += tlv_desc->len;
			*attempted_write_len = write_len;

			/* We stopped at this index. When called back, we will resume
			 * from this value.
			 */
			*cookie = i;
			err = BCME_BUFTOOSHORT;
			break;
		}
		i++;
	}

	/* Done finally. Fill up the container */
	if (err == BCME_OK) {
		/* Complete the outer container with type and length
		 * only.
		 */
		bcm_xtlv_put_data(xtlvbuf,
			tag,
			NULL, bcm_xtlv_buf_len(&local_xtlvbuf));
		/* Clean up cookie usage after our work. */
		*cookie = ECOUNTERS_CLIENT_PARAM_COOKIE_INVALID;
	}
fail:
	return err;
}

static int
wlc_ampdu_ecounter_dump(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	uint16 slice_index = 0;
	int ret = BCME_OK;
	ASSERT(stats_type == WL_IFSTATS_XTLV_WL_SLICE_TX_AMPDU_DUMP ||
		stats_type == WL_IFSTATS_XTLV_WL_SLICE_RX_AMPDU_DUMP);
	/* slice index to report Only two slices to choose from */
	slice_index = (req->slice_mask &
		ECOUNTERS_STATS_TYPES_SLICE_MASK_SLICE0) ? 0 : 1;

#ifdef WLRSDB
	/* get the right wlc */
	wlc = RSDB_ENAB(wlc->pub) ? wlc->cmn->wlc[slice_index]: wlc;
#endif // endif
	if (stats_type == WL_IFSTATS_XTLV_WL_SLICE_TX_AMPDU_DUMP) {
		ret = wlc_ampdu_ecounter_tx_dump(wlc->ampdu_tx, stats_type,
			xtlvbuf, cookie, attempted_write_len);
	} else if (stats_type == WL_IFSTATS_XTLV_WL_SLICE_RX_AMPDU_DUMP) {
		ret = wlc_ampdu_ecounter_rx_dump(wlc->ampdu_rx, stats_type,
		xtlvbuf, cookie, attempted_write_len);
	}
	return ret;
}
#endif /* ECOUNTERS */

static void wlc_ampdu_chansw_update(void *arg, wlc_chansw_notif_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;

	/* Each slice in RSDB is independent of each other. So channel switch could be
	 * done independently. Report whenever any slice switches channels
	 */
	if (EVENT_LOG_IS_ON(EVENT_LOG_TAG_RATE_CNT)) {
		wlc_ampdu_rxmcs_counter_report(wlc->ampdu_rx, EVENT_LOG_TAG_RATE_CNT);
		wlc_ampdu_txmcs_counter_report(wlc->ampdu_tx, EVENT_LOG_TAG_RATE_CNT);
	}
}
#endif /* EVENT_LOG_COMPILE */
#endif // endif

/* FOLLOWING FUNCTIONS 2B MOVED TO COMMON (ampdu and ba) CODE ONCE DELAYED BA IS REVIVED */

/** Action frame with request to set up or tear down connection was received */
int
wlc_frameaction_ampdu(wlc_info_t *wlc, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	uint8 action_id;
	int ret = BCME_OK;
	int expected_len;

	ASSERT((body[0] & DOT11_ACTION_CAT_MASK) == DOT11_ACTION_CAT_BLOCKACK);
	BCM_REFERENCE(expected_len);

	if (!scb) {
		WL_AMPDU_CTL(("wl%d: wlc_frameaction_ampdu: scb not found\n",
			wlc->pub->unit));
		return BCME_ERROR;
	}

	if (body_len < 2) {
		ret = BCME_BADLEN;
		expected_len = 2;
		action_id = -1; /* Set to invalid action id */
		goto err;
	}

	action_id = body[1];

	switch (action_id) {

	case DOT11_BA_ACTION_ADDBA_REQ:
		if (body_len < DOT11_ADDBA_REQ_LEN) {
			ret = BCME_BADLEN;
			expected_len = DOT11_ADDBA_REQ_LEN;
			goto err;
		}
		wlc_ampdu_recv_addba_req(wlc, scb, body, body_len);
		break;

	case DOT11_BA_ACTION_ADDBA_RESP:
		if (body_len < DOT11_ADDBA_RESP_LEN) {
			ret = BCME_BADLEN;
			expected_len = DOT11_ADDBA_RESP_LEN;
			goto err;
		}
		wlc_ampdu_recv_addba_resp(wlc->ampdu_tx, scb, body, body_len);
		break;

	case DOT11_BA_ACTION_DELBA:
		if (body_len < DOT11_DELBA_LEN) {
			ret = BCME_BADLEN;
			expected_len = DOT11_DELBA_LEN;
			goto err;
		}
		wlc_ampdu_recv_delba(wlc, scb, body, body_len);
		break;

	default:
		WL_AMPDU_ERR(("wl%d: FC_ACTION: Invalid BA action id %d\n",
			wlc->pub->unit, action_id));
		ret = BCME_ERROR;
		goto action_id_err;
	}

	return ret;

err:
	WL_AMPDU_ERR(("wl%d: %s: Action id %d recd invalid frame of length %d < %d\n",
		WLCWLUNIT(wlc), __FUNCTION__, action_id, body_len, expected_len));
action_id_err:
	WLCNTINCR(wlc->pub->_cnt->rxbadproto);
	wlc_send_action_err(wlc, scb->bsscfg, hdr, body, body_len);
	return ret;
} /* wlc_frameaction_ampdu */

/**
 * Enable AMPDU connection with a specific remote station. Called in the course of an association.
 */
void
wlc_scb_ampdu_enable(wlc_info_t *wlc, struct scb *scb)
{
	ASSERT(scb);
	/* If only txaggr of at least one TID of the bsscfg is ON */
	if (wlc_ampdu_tx_get_bsscfg_aggr(wlc->ampdu_tx, SCB_BSSCFG(scb)) != 0) {
		wlc_ampdu_tx_scb_enable(wlc->ampdu_tx, scb);
	}
}

/** disable AMPDU connection with a specific remote station */
void
wlc_scb_ampdu_disable(wlc_info_t *wlc, struct scb *scb)
{
	ASSERT(scb);

	wlc_ampdu_tx_scb_disable(wlc->ampdu_tx, scb);
	scb_ampdu_rx_flush(wlc->ampdu_rx, scb);
}

/** called on management frame (BA/BAR) reception */
void
wlc_ampdu_recv_ctl(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len, uint16 fk)
{

	if (!scb || !SCB_AMPDU(scb)) {
		WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_ctl: AMPDU not advertized by remote\n",
			wlc->pub->unit));
		return;
	}

	if (fk == FC_BLOCKACK_REQ) {
		if (body_len < DOT11_BAR_LEN)
			goto err;
		wlc_ampdu_recv_bar(wlc->ampdu_rx, scb, body, body_len);
	} else if (fk == FC_BLOCKACK) {
		if (body_len < (DOT11_BA_LEN + DOT11_BA_CMP_BITMAP_LEN))
			goto err;
		wlc_ampdu_recv_ba(wlc->ampdu_tx, scb, body, body_len);
	} else {
		ASSERT(0);
	}

	return;

err:
	WL_AMPDU_ERR(("wl%d: %s: recd invalid frame of length %d\n",
		wlc->pub->unit, __FUNCTION__, body_len));
	WLCNTINCR(wlc->pub->_cnt->rxbadproto);
}

void
wlc_ampdu_recv_addba_req(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len)
{
	dot11_addba_req_t *addba_req;

	ASSERT(scb);

	addba_req = (dot11_addba_req_t *)body;

	/* check if it is action err frame */
	if (addba_req->category & DOT11_ACTION_CAT_ERR_MASK)
		wlc_ampdu_recv_addba_req_ini(wlc->ampdu_tx, scb, addba_req, body_len);
	else
		wlc_ampdu_recv_addba_req_resp(wlc->ampdu_rx, scb, addba_req, body_len);

}

/**
 * In order to create an AMPDU connection, a 'block ack add' frame has to be sent to the caller
 * specified remote party ('scb'). This frame should specify for what traffic class ('tid') a
 * connection is requested.
 *
 * Does not have any dependency on ampdu, so can be used for delayed ba as well
 */
int
wlc_send_addba_req(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 wsize,
	uint8 ba_policy, uint8 delba_timeout)
{
	dot11_addba_req_t *addba_req;
	uint16 param_set, start_seq;
	void *p;
	uint8 *pbody, *addba_extie;
#if defined(STA) && defined(WME)
	wlc_bsscfg_t *cfg;
#endif /* defined(STA) && defined(WME) */
	struct ether_addr *sa;
	struct ether_addr *bssid;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(tid < AMPDU_MAX_SCB_TID);
	ASSERT(wsize <= wlc_ampdu_tx_get_ba_tx_wsize(wlc->ampdu_tx));

	wlc_scb_get_cfg_etheraddr_bssid(scb, &sa, &bssid);

	if (wlc->block_datafifo)
		return BCME_NOTREADY;

	if (SCB_HE_CAP(scb) && SCB_HE_FRAGRX_CAP(wlc->hei, scb)) {
		p = wlc_frame_get_action(wlc, &scb->ea, sa,
			bssid, (DOT11_ADDBA_REQ_LEN + TLV_HDR_LEN + ADDBA_CAP_SIZE),
			&pbody, DOT11_ACTION_CAT_BLOCKACK);
	} else {
		p = wlc_frame_get_action(wlc, &scb->ea, sa,
			bssid, DOT11_ADDBA_REQ_LEN, &pbody, DOT11_ACTION_CAT_BLOCKACK);
	}

	if (p == NULL)
		return BCME_NOMEM;

	addba_req = (dot11_addba_req_t *)pbody;
	addba_req->category = DOT11_ACTION_CAT_BLOCKACK;
	addba_req->action = DOT11_BA_ACTION_ADDBA_REQ;
	addba_req->token = (uint8)wlc->counter;

	/* token cannot be zero */
	if (!addba_req->token) {
		wlc->counter++;
		addba_req->token++;
	}

	/*
	 * Calc the Block Ack Parameter Set field
	 */

	/* Advertise the TID, buffer count, and Block Ack Policy */
	param_set = ((tid << DOT11_ADDBA_PARAM_TID_SHIFT) & DOT11_ADDBA_PARAM_TID_MASK) |
		((wsize << DOT11_ADDBA_PARAM_BSIZE_SHIFT) & DOT11_ADDBA_PARAM_BSIZE_MASK) |
		((ba_policy << DOT11_ADDBA_PARAM_POLICY_SHIFT) & DOT11_ADDBA_PARAM_POLICY_MASK);

	/* Advertise A-MSDU Supported bit if we are configured for A-MSDU Tx */
	if (AMSDU_TX_ENAB(wlc->pub) && (AMSDU_2G_ENAB(wlc->pub) ||
		CHSPEC_IS5G(SCB_BSSCFG(scb)->current_bss->chanspec))) {
		param_set |= DOT11_ADDBA_PARAM_AMSDU_SUP;
	}

	htol16_ua_store(param_set, (uint8 *)&addba_req->addba_param_set);
	htol16_ua_store(delba_timeout, (uint8 *)&addba_req->timeout);
	start_seq = (SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1)) << SEQNUM_SHIFT;
	htol16_ua_store(start_seq, (uint8 *)&addba_req->start_seqnum);

	/* Add ADDBA extention IE */
	if (SCB_HE_CAP(scb) && SCB_HE_FRAGRX_CAP(wlc->hei, scb)) {
		addba_extie = pbody + sizeof(dot11_addba_req_t);
		addba_extie[TLV_TAG_OFF] = DOT11_MNG_EXT_ADDBA_ID;
		addba_extie[TLV_LEN_OFF] = ADDBA_CAP_SIZE;
		addba_extie[TLV_HDR_LEN] = (HE_MAC_FRAG_ONE_PER_AMPDU << ADDBA_CAP_NOFRAG_FSZ);
	}

	WL_AMPDU_CTL(("wl%d: %s: param 0x%04x (tid %u wsize %u policy %u amsdu-tx %d) "
	              "timeout 0x%x seq 0x%x\n",
	              wlc->pub->unit, __FUNCTION__,
	              param_set, tid, wsize, ba_policy,
	              ((param_set & DOT11_ADDBA_PARAM_AMSDU_SUP) != 0),
	              delba_timeout, start_seq));

	/* set same priority as tid */
	PKTSETPRIO(p, tid);
	wlc_sendmgmt(wlc, p, SCB_WLCIFP(scb)->qi, scb);

#if defined(STA) && defined(WME)
	/* If STA has PM with APSD enabled, then send null data frame as the trigger frame
	 * so that the addba response can be delivered
	 */
	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	if (BSSCFG_STA(cfg) && cfg->BSS && cfg->pm->PM != PM_OFF &&
	    AC_BITMAP_TST(scb->apsd.ac_delv, WME_PRIO2AC(tid))) {

		WL_AMPDU_CTL(("wl%d: %s: sending null data frame on tid %d\n",
			wlc->pub->unit, __FUNCTION__, tid));
		wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, 0, 0, tid,
			NULL, NULL);
	}
#endif /* defined(STA) && defined(WME) */

	return 0;
} /* wlc_send_addba_req */

/**
 * Tears down an existing AMPDU connection with a caller specified station ('scb') and traffic
 * class ('tid).
 *
 * Does not have any dependency on ampdu, so can be used for delayed ba as well
 */
int
wlc_send_delba(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 initiator, uint16 reason)
{
	dot11_delba_t *delba;
	uint16 tmp;
	void *p;
	uint8 *pbody;
	struct ether_addr *sa;
	struct ether_addr *bssid;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	wlc_scb_get_cfg_etheraddr_bssid(scb, &sa, &bssid);

	if (wlc->block_datafifo)
		return BCME_NOTREADY;

	p = wlc_frame_get_action(wlc, &scb->ea, sa,
			bssid, DOT11_DELBA_LEN, &pbody, DOT11_ACTION_CAT_BLOCKACK);
	if (p == NULL)
		return BCME_NOMEM;

	delba = (dot11_delba_t *)pbody;
	delba->category = DOT11_ACTION_CAT_BLOCKACK;
	delba->action = DOT11_BA_ACTION_DELBA;
	tmp = ((tid << DOT11_DELBA_PARAM_TID_SHIFT) & DOT11_DELBA_PARAM_TID_MASK) |
		((initiator << DOT11_DELBA_PARAM_INIT_SHIFT) & DOT11_DELBA_PARAM_INIT_MASK);
	delba->delba_param_set = htol16(tmp);
	delba->reason = htol16(reason);

	WL_AMPDU_CTL(("wl%d: wlc_send_delba: tid %d initiator %d reason %d\n",
		wlc->pub->unit, tid, initiator, reason));

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	wlc_sendmgmt(wlc, p, SCB_WLCIFP(scb)->qi, scb);

	return 0;
} /* wlc_send_delba */

/**
 * BARs (Block Ack Requests) are sent to solicit a BA (block ack) from a remote party.
 * Caller specifies remote party (scb) and traffic class (tid).
 *
 * Does not have any dependency on ampdu, so can be used for delayed ba as well
 */
void *
wlc_send_bar(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 start_seq,
	uint16 cf_policy, bool enq_only, bool *blocked)
{
	struct dot11_ctl_header *hdr;
	struct dot11_bar *bar;
	void *p;
	uint16 tmp;
	struct ether_addr *sa;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	if (wlc->block_datafifo) {
		*blocked = TRUE;
		return NULL;
	}

	sa = wlc_scb_get_cfg_etheraddr(scb);

	*blocked = FALSE;

	p = wlc_frame_get_ctl(wlc, DOT11_CTL_HDR_LEN + DOT11_BAR_LEN);
	if (p == NULL)
		return NULL;

	hdr = (struct dot11_ctl_header *)PKTDATA(wlc->osh, p);
	hdr->fc = htol16(FC_BLOCKACK_REQ);
	hdr->durid = 0;
	bcopy(&scb->ea, &hdr->ra, ETHER_ADDR_LEN);
	bcopy(sa, &hdr->ta, ETHER_ADDR_LEN);

	bar = (struct dot11_bar *)&hdr[1];
	tmp = tid << DOT11_BA_CTL_TID_SHIFT;
	tmp |= (cf_policy & DOT11_BA_CTL_POLICY_MASK);
	tmp |= DOT11_BA_CTL_COMPRESSED;
	bar->bar_control = htol16(tmp);
	bar->seqnum = htol16(start_seq << SEQNUM_SHIFT);

	WL_AMPDU_ERR(("wl%d.%d: %s: for "MACF" seq 0x%x tid %d \n", wlc->pub->unit,
		WLC_BSSCFG_IDX(scb->bsscfg),  __FUNCTION__, ETHERP_TO_MACF(&scb->ea),
		start_seq, tid));

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	if (wlc_sendctl(wlc, p, SCB_WLCIFP(scb)->qi, scb, 0, enq_only)) {
		return p;
	} else {
		/* queue is full, consider this as blocked instead of memory shortage */
		*blocked = TRUE;
		return NULL;
	}
} /* wlc_send_bar */

/** Remote party requested to tear down a connection by sending us a DELBA action frame */
void
wlc_ampdu_recv_delba(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len)
{
	dot11_delba_t *delba;
	uint16 param_set;
	uint8 tid;
	uint16 reason, initiator;

	BCM_REFERENCE(body_len);

	ASSERT(scb);

	delba = (dot11_delba_t *)body;

	param_set = ltoh16(delba->delba_param_set);
	reason = ltoh16(delba->reason);

	tid = (param_set & DOT11_DELBA_PARAM_TID_MASK) >> DOT11_DELBA_PARAM_TID_SHIFT;
	initiator = (param_set & DOT11_DELBA_PARAM_INIT_MASK) >> DOT11_DELBA_PARAM_INIT_SHIFT;

	if (initiator)
		wlc_ampdu_rx_recv_delba(wlc->ampdu_rx, scb, tid, delba->category, initiator,
			reason);
	else
		wlc_ampdu_tx_recv_delba(wlc->ampdu_tx, scb, tid, delba->category, initiator,
			reason);
}

/** When a remote party (scb) disassociates, AMPDU connections have to be cleaned */
void
scb_ampdu_cleanup(wlc_info_t *wlc, struct scb *scb)
{
	uint8 tid;

	WL_AMPDU_UPDN(("scb_ampdu_cleanup: enter\n"));

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		if (SCB_LEGACY_WDS(scb) && (scb->flags & SCB_WDS_LINKUP)) {
			/* Send delba for Legacy WDS clients before AMPDU session cleanup.
			 * Should not be called for non-wds peers since action frames sent after
			 * disassociation causes devices to send disassociation
			 * which triggers our device to resend action frames
			 */
			wlc_ampdu_rx_send_delba(wlc->ampdu_rx, scb, tid, FALSE,
					DOT11_RC_UNSPECIFIED);
			wlc_ampdu_tx_send_delba(wlc->ampdu_tx, scb, tid, TRUE,
					DOT11_RC_UNSPECIFIED);
		} else {
			ampdu_cleanup_tid_resp(wlc->ampdu_rx, scb, tid);
			ampdu_cleanup_tid_ini(wlc->ampdu_tx, scb, tid, TRUE);
		}
	}
}

/** called e.g. on loss of association */
void
scb_ampdu_cleanup_all(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	WL_AMPDU_UPDN(("scb_ampdu_cleanup_all: enter\n"));
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu_tx_flush(wlc->ampdu_tx, scb);
			scb_ampdu_rx_flush(wlc->ampdu_rx, scb);
		}
	}
}

void
wlc_ampdu_agg_state_update_all(wlc_info_t *wlc, bool aggr)
{
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_AS_BSS(wlc, idx, cfg) {
		wlc_ampdu_rx_set_bsscfg_aggr_override(wlc->ampdu_rx, cfg, aggr);
		wlc_ampdu_tx_set_bsscfg_aggr_override(wlc->ampdu_tx, cfg, aggr);
	}
}

#if defined(WLPROPRIETARY_11N_RATES)
/**
 * Saves RAM because there is a big gap of unused MCS'es between MCS32 and MCS87. Mapping:
 * MCS    array_index
 *   0              0
 *  ..             ..
 *  31             31  == AMPDU_MAX_MCS
 *  32             32
 *  87             33  == WLC_11N_FIRST_PROP_MCS
 *  88             34
 *  99             35
 * 100             36
 * 101             37
 * 102             38  == WLC_11N_LAST_PROP_MCS
 */
uint8
mcs2idx(uint mcs)
{
	/*                   87 88 89 90 91 92 93 94 95 96 97 98 99 100 101 102 */
	const uint8 map[] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  2,  3,  4,  5};
	ASSERT(mcs <= AMPDU_MAX_MCS || mcs == 32 ||
		(mcs >= WLC_11N_FIRST_PROP_MCS && mcs <= WLC_11N_LAST_PROP_MCS));
	if (mcs == 32) return AMPDU_MAX_MCS + 1;
	if (mcs < WLC_11N_FIRST_PROP_MCS) return (uint8) mcs;
	return AMPDU_MAX_MCS + 2 + map[mcs - WLC_11N_FIRST_PROP_MCS];
}
#endif /* WLPROPRIETARY_11N_RATES */
