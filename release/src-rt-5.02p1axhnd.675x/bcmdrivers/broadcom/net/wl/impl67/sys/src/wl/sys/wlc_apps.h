/**
 * AP Powersave state related code
 * This file aims to encapsulating the Power save state of sbc,wlc structure.
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
 * $Id: wlc_apps.h 788023 2020-06-18 09:52:36Z $
*/

#ifndef _wlc_apps_h_
#define _wlc_apps_h_

#include <wlc_frmutil.h>

#ifdef AP

#include <wlc_pspretend.h>

/* these flags are used when exchanging messages
 * about PMQ state between BMAC and HIGH
*/
#define PS_SWITCH_OFF		0
#define PS_SWITCH_PMQ_ENTRY     0x01
#define PS_SWITCH_PMQ_SUPPR_PKT 0x02
#define PS_SWITCH_OMI		0x04
#define PS_SWITCH_RESERVED	0x08
#define PS_SWITCH_PMQ_PSPRETEND	0x10
#define PS_SWITCH_STA_REMOVED	0x20
#define PS_SWITCH_FIFO_FLUSHED	0x80

extern void wlc_apps_ps_flush(wlc_info_t *wlc, struct scb *scb);
extern void wlc_apps_scb_ps_on(wlc_info_t *wlc, struct scb *scb);
extern void wlc_apps_scb_ps_off(wlc_info_t *wlc, struct scb *scb, bool discard);
extern void wlc_apps_ps_requester(wlc_info_t *wlc, struct scb *scb,
	uint8 on_rqstr, uint8 off_rqstr);
extern void wlc_apps_process_ps_switch(wlc_info_t *wlc, struct scb *scb, uint8 ps_on);
extern void wlc_apps_process_pend_ps(wlc_info_t *wlc);
extern void wlc_apps_process_pmqdata(wlc_info_t *wlc, uint32 pmqdata);
extern void wlc_apps_pspoll_resp_prepare(wlc_info_t *wlc, struct scb *scb,
                                         void *pkt, struct dot11_header *h, bool last_frag);
extern void wlc_apps_send_psp_response(wlc_info_t *wlc, struct scb *scb, uint16 fc);

extern int wlc_apps_attach(wlc_info_t *wlc);
extern void wlc_apps_detach(wlc_info_t *wlc);
#if defined(BCMDBG)
extern void wlc_apps_wlc_down(wlc_info_t *wlc);
#endif /* BCMDBG */

extern void wlc_apps_psq_ageing(wlc_info_t *wlc);
extern bool wlc_apps_psq(wlc_info_t *wlc, void *pkt, int prec);
extern void wlc_apps_tbtt_update(wlc_info_t *wlc);
extern bool wlc_apps_suppr_frame_enq(wlc_info_t *wlc, void *pkt, tx_status_t *txs, bool lastframe);
extern void wlc_apps_ps_prep_mpdu(wlc_info_t *wlc, void *pkt);
extern void wlc_apps_apsd_trigger(wlc_info_t *wlc, struct scb *scb, int ac);
extern void wlc_apps_apsd_prepare(wlc_info_t *wlc, struct scb *scb, void *pkt,
                                  struct dot11_header *h, bool last_frag);

extern uint8 wlc_apps_apsd_ac_available(wlc_info_t *wlc, struct scb *scb);
extern uint8 wlc_apps_apsd_ac_buffer_status(wlc_info_t *wlc, struct scb *scb);

extern void wlc_apps_scb_tx_block(wlc_info_t *wlc, struct scb *scb, uint reason, bool block);
extern void wlc_apps_scb_psq_norm(wlc_info_t *wlc, struct scb *scb);
extern bool wlc_apps_scb_supr_enq(wlc_info_t *wlc, struct scb *scb, void *pkt);
extern int wlc_apps_scb_apsd_cnt(wlc_info_t *wlc, struct scb *scb);

extern void wlc_apps_process_pspretend_status(wlc_info_t *wlc, struct scb *scb,
                                              bool pps_recvd_ack);
#ifdef PROP_TXSTATUS
bool wlc_apps_pvb_upd_in_transit(wlc_info_t *wlc, struct scb *scb, uint vqdepth, bool op);
#endif // endif

#if defined(WLTAF)
extern int  wlc_apps_psq_len(wlc_info_t *wlc, struct scb *scb);
#endif // endif

void wlc_apps_pvb_update(wlc_info_t *wlc, struct scb *scb);

#ifdef BCMDBG
void wlc_apps_print_ps_stuck_info(wlc_info_t *wlc, struct scb *scb);
#endif // endif

extern void wlc_apps_psq_norm(wlc_info_t *wlc, struct scb *scb);

#ifdef WLTDLS
extern void wlc_apps_apsd_tdls_send(wlc_info_t *wlc, struct scb *scb);
#endif // endif

extern int  wlc_apps_psq_len(wlc_info_t *wlc, struct scb *scb);

void wlc_apps_ps_trans_upd(wlc_info_t *wlc, struct scb *scb);

void wlc_apps_set_listen_prd(wlc_info_t *wlc, struct scb *scb, uint16 listen);
uint16 wlc_apps_get_listen_prd(wlc_info_t *wlc, struct scb *scb);

extern uint32 wlc_apps_release_count(wlc_info_t *wlc, struct scb *scb, int prec);
extern void wlc_apps_trigger_on_complete(wlc_info_t *wlc, scb_t *scb);

#ifdef WLTWT
extern void wlc_apps_twt_enter(wlc_info_t *wlc, scb_t *scb);
extern void wlc_apps_twt_exit(wlc_info_t *wlc, scb_t *scb, bool enter_pm);
extern bool wlc_apps_suppr_twt_frame_enq(wlc_info_t *wlc, void *pkt);
extern bool wlc_apps_twt_sp_enter_ps(wlc_info_t *wlc, scb_t *scb);
extern void wlc_apps_twt_sp_release_ps(wlc_info_t *wlc, scb_t *scb);
#else
#define wlc_apps_suppr_twt_frame_enq(a, b) FALSE
#define wlc_apps_twt_sp_release_ps(a, b) do {} while (0)
#endif /* WLTWT */

#else /* AP */

#ifdef PROP_TXSTATUS
#define wlc_apps_pvb_upd_in_transit(a, b, c, d) FALSE
#endif // endif

#define wlc_apps_pvb_update(a, b) do {} while (0)

#define wlc_apps_attach(a) FALSE
#define wlc_apps_psq(a, b, c) FALSE
#define wlc_apps_suppr_frame_enq(a, b, c, d) FALSE

#define wlc_apps_scb_ps_on(a, b) do {} while (0)
#define wlc_apps_scb_ps_off(a, b, c) do {} while (0)
#define wlc_apps_ps_requester(a, b, c, d) do {} while (0)
#define wlc_apps_process_pend_ps(a) do {} while (0)

#define wlc_apps_process_pmqdata(a, b) do {} while (0)
#define wlc_apps_pspoll_resp_prepare(a, b, c, d, e) do {} while (0)
#define wlc_apps_send_psp_response(a, b, c) do {} while (0)

#define wlc_apps_detach(a) do {} while (0)
#define wlc_apps_process_ps_switch(a, b, c) do {} while (0)
#define wlc_apps_psq_ageing(a) do {} while (0)
#define wlc_apps_tbtt_update(a) do {} while (0)
#define wlc_apps_ps_prep_mpdu(a, b) do {} while (0)
#define wlc_apps_apsd_trigger(a, b, c) do {} while (0)
#define wlc_apps_apsd_prepare(a, b, c, d, e) do {} while (0)
#define wlc_apps_apsd_ac_available(a, b) 0
#define wlc_apps_apsd_ac_buffer_status(a, b) 0

#define wlc_apps_scb_tx_block(a, b, c, d) do {} while (0)
#define wlc_apps_scb_psq_norm(a, b) do {} while (0)
#define wlc_apps_scb_supr_enq(a, b, c) FALSE

#define wlc_apps_set_listen_prd(a, b, c) do {} while (0)
#define wlc_apps_get_listen_prd(a, b) 0
#ifdef WLTDLS
#define wlc_apps_apsd_tdls_send(a, b) do {} while (0)
#endif // endif

#define wlc_apps_release_count(a, b, c) FALSE
#define wlc_apps_trigger_on_complete(a, b)

#define wlc_apps_scb_in_ps_time(a, b) 0
#define wlc_apps_scb_in_pvb_time(a, b) 0
#endif /* AP */

#if defined(MBSS)
extern void wlc_apps_bss_ps_off_done(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_apps_bss_ps_on_done(wlc_info_t *wlc);
extern int wlc_apps_bcmc_ps_enqueue(wlc_info_t *wlc, struct scb *bcmc_scb, void *pkt);
#else
#define wlc_apps_bss_ps_off_done(wlc, bsscfg)
#define wlc_apps_bss_ps_on_done(wlc)
#endif /* MBSS */

#ifdef PROP_TXSTATUS
extern void wlc_apps_ps_flush_mchan(wlc_info_t *wlc, struct scb *scb);
#endif /* PROP_TXSTATUS && WLMCHAN */

void wlc_apps_dbg_dump(wlc_info_t *wlc, int hi, int lo);

extern uint wlc_apps_scb_txpktcnt(wlc_info_t *wlc, struct scb *scb);
struct pktq * wlc_apps_get_psq(wlc_info_t * wlc, struct scb * scb);
extern void wlc_apps_map_pkts(wlc_info_t *wlc, struct scb *scb, map_pkts_cb_fn cb, void *ctx);
extern void wlc_apps_set_change_scb_state(wlc_info_t *wlc, struct scb *scb, bool reset);

#ifdef WLCFP
#if defined(AP) && defined(PROP_TXSTATUS)
void wlc_apps_pvb_dec_in_transit(wlc_info_t *wlc, struct scb *scb);
void wlc_apps_scb_psq_resp(wlc_info_t *wlc, struct scb *scb);
void wlc_apps_scb_apsd_tx_pending(wlc_info_t *wlc, struct scb *scb, uint32 extra_flags);
#else /* !AP && !PROP_TXSTATUS */
#define wlc_apps_pvb_dec_in_transit(a, b) do {} while (0)
#define wlc_apps_scb_psq_resp(a, b) do {} while (0)
#define wlc_apps_scb_apsd_tx_pending(a, b, c) do {} while (0)
#endif /* !AP && !PROP_TXSTATUS */

extern void wlc_cfp_apps_ps_send(wlc_info_t *wlc, struct scb *scb, void *pkt, uint8 prio);
#endif /* WLCFP */

#ifdef WL_MBSSID
void wlc_apps_pvb_update_bcmc(wlc_info_t *wlc, struct scb *scb, bool on);
#endif /* WL_MBSSID */
#endif /* _wlc_apps_h_ */
