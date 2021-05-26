/*
 * MU-MIMO transmit module for Broadcom 802.11 Networking Adapter Device Drivers
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
 * $Id: wlc_mutx.h $
 */

#ifndef _wlc_mutx_h_
#define _wlc_mutx_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <wlc_mumimo.h>
#include <wlc_rate_sel.h>

/* Standard attach and detach functions */
wlc_mutx_info_t* BCMATTACHFN(wlc_mutx_attach)(wlc_info_t *wlc);
void BCMATTACHFN(wlc_mutx_detach)(wlc_mutx_info_t *mu_info);

/* Other public APIs. */
uint16 wlc_mutx_sta_client_index(wlc_mutx_info_t *mu_info, struct scb *scb);
bool wlc_mutx_is_group_member(wlc_mutx_info_t *mu_info, struct scb *scb, uint16 group_id);
bool wlc_mutx_is_muclient(wlc_mutx_info_t *mu_info, struct scb *scb);
void wlc_mutx_update_vht_cap(wlc_mutx_info_t *mu_info, struct scb *scb);
void wlc_mutx_update_he_cap(wlc_mutx_info_t *mu_info, struct scb *scb);

/* Call this API when a configuration or state change may affect whether MU TX can be active */
void wlc_mutx_active_update(wlc_mutx_info_t *mu_info);

#ifdef WL_MUPKTENG
extern uint8 wlc_mutx_pkteng_on(wlc_mutx_info_t *mu_info);
#endif // endif
#ifdef WLCNT
void wlc_mutx_update_txcounters(wlc_mutx_info_t *mu_info, struct scb *scb,
	bool txs_mu, tx_status_t *txs, ratesel_txs_t *rs_txs, uint8 rnum);
#endif /* WLCNT */

#if defined(BCMDBG) || defined(BCMDBG_MU)
extern uint8 wlc_mutx_on(wlc_mutx_info_t *mu_info);
#endif // endif
void wlc_mutx_update(wlc_info_t *wlc);
bool wlc_mutx_switch_in_progress(wlc_info_t *wlc);
#ifdef WL_PSMX
void wlc_mutx_hostflags_update(wlc_info_t *wlc);
#endif /* WL_PSMX */
bool wlc_mutx_sta_on_hold(wlc_mutx_info_t *mu_info, struct scb *scb);
bool wlc_mutx_sta_mu_link_permit(wlc_mutx_info_t *mu_info, struct scb *scb);
#if defined(WL_MU_TX)
#ifdef WLCNT
void BCMFASTPATH wlc_mutx_upd_interm_counters(wlc_mutx_info_t *mu_info,
    struct scb *scb, tx_status_t *txs);
#endif // endif
extern void wlc_mutx_txfifo_complete(wlc_info_t *wlc);
#endif /* defined(WL_MU_TX) */

/* mutx state bitfield */
#define MUTX_OFF	0
#define MUTX_ON		1

typedef struct {
	uint8	state;
} mutx_state_upd_data_t;

extern int wlc_mutx_sta_mucidx_get(wlc_mutx_info_t *mu_info, struct scb *scb);
#ifdef WL_MU_TX
extern int wlc_mutx_state_upd_register(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg);
extern int wlc_mutx_state_upd_unregister(wlc_info_t *wlc, bcm_notif_client_callback fn, void *arg);
extern uint8 wlc_mutx_get_muclient_nrx(wlc_mutx_info_t *mu_info);
extern void wlc_mutx_update_hemmu_dsmps(wlc_mutx_info_t *mu_info, scb_t *scb);
#else
#define wlc_mutx_state_upd_register(wlc, fn, arg) (0)
#define wlc_mutx_state_upd_unregister(wlc, fn, arg) (0)
#endif // endif
extern bool wlc_mutx_is_hedlmmu_enab(wlc_mutx_info_t *mu_info);
extern void wlc_mutx_update_scb_oper_mode(wlc_mutx_info_t *mu_info, scb_t *scb, uint8 oper_mode);
extern void wlc_mutx_evict_or_admit_muclient(wlc_mutx_info_t *mu_info, scb_t *scb);
extern void wlc_mutx_upd_min_dlofdma_users(wlc_mutx_info_t *mu_info);
extern void wlc_mutx_nrx_policy_upd(wlc_info_t *wlc, int mu_supported_Ntx);
extern void wlc_mutx_evict_all_muclients(wlc_mutx_info_t *mu_info);
extern void wlc_mutx_sounding_period_upd(wlc_mutx_info_t *mu_info, uint16 period);
#endif   /* _wlc_mutx_h_ */
