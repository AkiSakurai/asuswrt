/*
 * Proxd FTM method support. See twiki FineTimingMeasurement.
 * This header is specifies external s/w interface to FTM
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
 * $Id: wlc_ftm.h 777286 2019-07-25 19:43:30Z $
 */

#ifndef _wlc_ftm_h_
#define _wlc_ftm_h_

#include <typedefs.h>
#include <bcmutils.h>
#include <wlc_types.h>
#include <wlioctl.h>
#include <wlc_scb.h>

/* Disable all these feature to save memory */
#define WL_FTM_11K
#define WL_FTM_RANGE
#define WL_FTM_TSF_SYNC
#define WL_FTM_SECURITY
#define WL_FTM_VS_MF_BUF

/* handle to FTM */
struct pdftm;
typedef struct pdftm wlc_ftm_t;

struct pdftm_session;
typedef struct pdftm_session wlc_ftm_session_t;

/* event data  - note: session may not be valid once callback returns */
struct pdftm_event_data {
	wlc_ftm_t				*ftm;
	wlc_ftm_session_t		*session;
	wl_proxd_event_type_t	event_type;
	wl_proxd_session_id_t	sid;
	wl_proxd_status_t		status;
};
typedef struct pdftm_event_data wlc_ftm_event_data_t;

typedef void (*wlc_ftm_event_callback_t)(void *cb_ctx,
	wlc_ftm_event_data_t *event_data);

/* lookup ftm handle for wlc */
wlc_ftm_t* wlc_ftm_get_handle(wlc_info_t *wlc);

#if defined(WL_PROXDETECT) || defined(WL_FTM)
bool wlc_ftm_bsscfg_is_secure(const wlc_bsscfg_t *bsscfg);
#endif // endif

#if defined(WL_PROXDETECT) && defined(WL_FTM)
#define WLC_BSSCFG_SECURE_FTM(bsscfg)  (PROXD_ENAB(bsscfg->wlc->pub) && \
	wlc_ftm_bsscfg_is_secure(bsscfg))
#else
	#define WLC_BSSCFG_SECURE_FTM(bsscfg)  0
#endif // endif

/* lookup wlc from ftm. note: ftm may decouple from wlc for rsdb */
wlc_info_t* wlc_ftm_get_wlc(wlc_ftm_t *ftm);

/* is ftm enabled */
bool wlc_ftm_is_enabled(wlc_ftm_t *ftm,  wlc_bsscfg_t *bsscfg);

/* stop a session */
int wlc_ftm_stop_session(wlc_ftm_t *ftm, wl_proxd_session_id_t sid, int status);

/* destroy a session */
int wlc_ftm_delete_session(wlc_ftm_t *ftm, wl_proxd_session_id_t sid);

/* some convenience get functionality. use iov support for other gets and set */

/* get session info */
int wlc_ftm_get_session_info(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	wl_proxd_ftm_session_info_t *sip);

/* get session tsf */
int wlc_ftm_get_session_tsf(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	uint64 *tsf);

/* get session config - one tlv data returned in LE format */
int wlc_ftm_get_tlv_data(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	uint16 tlv_id, uint8 *buf, int buf_len, int *out_len);

#ifdef WL_FTM_RANGE
/* ranging support */

/* callback. note: valid to call into ftm. results may be received calling into
 * ftm or via ftm events - see WL_PROXD_EVENT_RANGING. ranging status is
 * provided in the callback along with an indication of session/event.
 */
typedef void (*wlc_ftm_ranging_cb_t)(wlc_info_t *wlc, wlc_ftm_t *ftm,
	wl_proxd_status_t status, wl_proxd_event_type_t event,
	wl_proxd_session_id_t sid, void *cb_ctx);

/* create a ranging context. */
int wlc_ftm_ranging_create(wlc_ftm_t *ftm,
	wlc_ftm_ranging_cb_t cb, void *cb_ctx, wlc_ftm_ranging_ctx_t **rctx);

/* destroy a ranging context. clear rctx, optionally delete sessions */
int wlc_ftm_ranging_destroy(wlc_ftm_ranging_ctx_t **rctx);

/* start ranging for the context */
int wlc_ftm_ranging_start(wlc_ftm_ranging_ctx_t *rctx);

/* cancel ranging for the context */
int wlc_ftm_ranging_cancel(wlc_ftm_ranging_ctx_t *rctx);

/* configuration support via ftm iovars. set operations are not
 * performance critical, and get operations should be infrequent because
 * caller knows the configuration to create sessions etc.
 */
int wlc_ftm_set_iov(wlc_ftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len);

int wlc_ftm_get_iov(wlc_ftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len);

/* add a session. must be initiator, can be partially configured, if session
 * does not exist or is global sid, one will be created, added to the list
 * of sessions to be ranged together and returned in sid.
 */
int wlc_ftm_ranging_add_sid(wlc_ftm_ranging_ctx_t *rctx,
	wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t *sid);

/* configure ranging flags, e.g. delete-sessions on stop */
int wlc_ftm_ranging_set_flags(wlc_ftm_ranging_ctx_t *rctx,
	wl_proxd_ranging_flags_t flags, wl_proxd_ranging_flags_t mask);

/* lookup a session based on mac address */
int wlc_ftm_ranging_lookup_sid(wlc_ftm_ranging_ctx_t *rctx, const struct ether_addr *addr,
	wl_proxd_session_id_t *sid);

/* lookup session ids for a ranging context - limited to max_sids,
 * BCME_BUFTOOSHORT returned for insufficient space
 */
int wlc_ftm_ranging_get_sids(wlc_ftm_ranging_ctx_t *rctx,
	int max_sids, wl_proxd_session_id_list_t *sids);

/* get results for a given session */
int wlc_ftm_ranging_get_result(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_session_id_t sid,
	int max_rtt, wl_proxd_rtt_result_t *res);

/* note: results may also be obtained from WL_PROXD_EVENT_RANGING. when ranging is enabled
 * for a set of sessions, burst end events are suppressed and results are delivered via
 * ranging events.
 */

/* note: ftm dump will dump all current ranging contexts */

/* return status etc for ranging */
int wlc_ftm_ranging_get_info(wlc_ftm_ranging_ctx_t *rctx, wl_proxd_ranging_info_t *info);

/* set the allocation range for ranging */
int wlc_ftm_ranging_set_sid_range(wlc_ftm_ranging_ctx_t *rctx,
	wl_proxd_session_id_t sid_min, wl_proxd_session_id_t sid_max);

/* register for events - event mask not currently honored */
int wlc_ftm_event_register(wlc_ftm_t *ftm, wl_proxd_event_mask_t events,
    wlc_ftm_event_callback_t cb, void *cb_ctx);

/* unregister for events */
int wlc_ftm_event_unregister(wlc_ftm_t *ftm,
    wlc_ftm_event_callback_t cb, void *cb_ctx);
#endif /* WL_FTM_RANGE */

void wlc_ftm_stop_sess_inprog(wlc_ftm_t *ftm, wlc_bsscfg_t *cfg);

int wlc_ftm_start_session(wlc_ftm_t *ftm, wl_proxd_session_id_t sid);
int wlc_ftm_get_session_result(wlc_ftm_t * ftm, wl_proxd_session_id_t sid,
	wl_proxd_rtt_result_t * res, int num_rtt);

#ifdef WL_RANGE_SEQ
/* security support */
int wlc_ftm_pmk_to_tpk(wlc_ftm_t *ftm, scb_t *scb, const uint8 *pmk, int pmk_len,
	const uint8 *anonce, int anonce_len, const uint8 *snonce, int snonce_len);

/* security support */
int wlc_ftm_set_tpk(wlc_ftm_t *ftm, scb_t *scb, const uint8 *tpk, uint16 tpk_len);
#else
int wlc_ftm_new_tpk(wlc_ftm_t *ftm, scb_t *scb, const uint8 *pmk, int pmk_len,
	const uint8 *anonce, int anonce_len, const uint8 *snonce, int snonce_len);
#endif /* WL_RANGE_SEQ */

int wlc_ftm_get_session_params(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	dot11_ftm_params_t *ftm_params);
int wlc_ftm_set_session_event_bsscfg(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	wlc_bsscfg_t *event_cfg);
uint16 wlc_ftm_build_vs_req_params(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
	dot11_ftm_vs_ie_pyld_t *vs_ie_out, const dot11_ftm_vs_ie_pyld_t *vs_ie_in,
	uint8 vs_ie_in_len);
int wlc_ftm_set_session_vs_req_params(wlc_ftm_t * ftm, wl_proxd_session_id_t sid,
	const dot11_ftm_vs_ie_pyld_t *vs_ie, uint16 vs_ie_len);

#endif /* _wlc_ftm_h_ */
