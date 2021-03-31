/*
 * Proxd internal interface
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
 * $Id: pdsvc.h 777082 2019-07-18 14:48:21Z $
 */

#ifndef _pdsvc_h_
#define _pdsvc_h_

#include <typedefs.h>
#include <bcmutils.h>
#include <wlioctl.h>

#include <wlc_types.h>
#include <wlc_pcb.h>
#include <wlc_pddefs.h>
#include <wlc_ftm.h>

typedef wlc_pdsvc_info_t pdsvc_t;

/* distance units. */
enum pd_distu {
	PD_DIST_1BY16M 	= 1,
	PD_DIST_1BY256M = 2,
	PD_DIST_CM		= 3,
	PD_DIST_UNKNOWN = 4
};
typedef int8 pd_distu_t;

enum proxd_pwr_id {
	PROXD_PWR_ID_SCHED = 0,
	PROXD_PWR_ID_BURST = 1
};
typedef uint8 proxd_pwr_id_t;

wlc_bsscfg_t * pdsvc_get_bsscfg(pdsvc_t *pdsvc);

void proxd_dump(pdsvc_t *pdsvc, struct bcmstrbuf *b);

/* initialize event */
void proxd_init_event(pdsvc_t *pdsvc, wl_proxd_event_type_t type,
	wl_proxd_method_t method, wl_proxd_session_id_t sid, wl_proxd_event_t *event);

/* send event; len includes event header */
void proxd_send_event(pdsvc_t *pdsvc, wlc_bsscfg_t *bsscfg, wl_proxd_status_t status,
	const struct ether_addr *addr, wl_proxd_event_t *event, uint16 len);

/* allocate action frame */
void* proxd_alloc_action_frame(pdsvc_t *pdsvc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *da, const struct ether_addr *sa,
	const struct ether_addr *bssid, uint body_len, uint8 **body,
	uint8 category, uint8 action);

/* send a frame; status indicates if this is an error response from method,
 * packet is sent or freed on error
 */
bool proxd_tx(pdsvc_t *svc, void *pkt, wlc_bsscfg_t *bsscfg,
	ratespec_t rspec, int status);

wl_proxd_params_tof_tune_t *proxd_get_tunep(wlc_info_t *wlc, uint64 *tq);
void proxd_enable(wlc_info_t *wlc, bool enable);

uint16
proxd_get_tunep_idx(wlc_info_t *wlc, wl_proxd_session_flags_t flags,
	chanspec_t cspec, wl_proxd_params_tof_tune_t **tunep);
void
proxd_update_tunep_values(wl_proxd_params_tof_tune_t *tunep, chanspec_t cspec, bool vhtack);

void proxd_power(wlc_info_t *wlc, proxd_pwr_id_t id, bool on);
void proxd_undeaf_phy(wlc_info_t *wlc, bool acked);

#endif /* _pdsvc_h_ */
