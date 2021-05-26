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
 * $Id: wlc_apps.h 766795 2018-08-14 12:26:58Z $
*/

#ifndef _wlc_apps_h_
#define _wlc_apps_h_

#ifdef WLC_HIGH
#include <wlc_frmutil.h>
#endif // endif

/* an arbitary number for unbonded USP */
#define WLC_APSD_USP_UNB 0xfff

#ifdef AP

/* these flags are used when exchanging messages
 * about PMQ state between BMAC and HIGH
*/
#define TX_FIFO_FLUSHED 0x80
#define MSG_MAC_INVALID 0x40
#define STA_REMOVED 0x20
#define PMQ_PRETEND_PS 0x10
#define FLUSH_ALL_PKTS 0xFFFF

extern void wlc_apps_bss_wd_ps_check(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

extern int wlc_apps_process_ps_switch(wlc_info_t *wlc, struct ether_addr *ea, int8 ps_on);
extern void wlc_apps_scb_ps_on(wlc_info_t *wlc, struct scb *scb);
extern void wlc_apps_scb_ps_off(wlc_info_t *wlc, struct scb *scb, bool discard);
extern void wlc_apps_process_pend_ps(wlc_info_t *wlc);
extern void wlc_apps_process_pmqdata(wlc_info_t *wlc, uint32 pmqdata);
extern void wlc_apps_pspoll_resp_prepare(wlc_info_t *wlc, struct scb *scb,
                                         void *pkt, struct dot11_header *h, bool last_frag);
extern void wlc_apps_send_psp_response(wlc_info_t *wlc, struct scb *scb, uint16 fc);

extern int wlc_apps_attach(wlc_info_t *wlc);
extern void wlc_apps_detach(wlc_info_t *wlc);

extern int wlc_pspretend_attach(wlc_info_t *wlc);
extern void wlc_pspretend_detach(wlc_pps_info_t* pps_info);

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
extern bool wlc_apps_scb_pspretend_on(wlc_info_t *wlc, struct scb *scb, uint8 flags);

#ifdef PROP_TXSTATUS
extern void wlc_apps_pvb_update_from_host(wlc_info_t *wlc, struct scb *scb);
extern bool wlc_apps_pvb_chkupdate_from_host(wlc_info_t *wlc, struct scb *scb, bool op);
#endif // endif

#if defined(WLTAF) || defined(WLAIBSS)
extern int  wlc_apps_psq_len(wlc_info_t *wlc, struct scb *scb);
#endif // endif

extern void wlc_apps_pvb_update(wlc_info_t *wlc, struct scb *scb);

extern void wlc_apps_psq_norm(wlc_info_t *wlc, struct scb *scb);

#ifdef WLTDLS
extern void wlc_apps_apsd_tdls_send(wlc_info_t *wlc, struct scb *scb);
#endif // endif

#ifdef PSPRETEND
struct wlc_pps_info {
	wlc_info_t *wlc;
	struct  wl_timer * ps_pretend_probe_timer;
	bool    is_ps_pretend_probe_timer_running;
};
#endif /* PSPRETEND */

extern bool wlc_apps_pps_retry(wlc_info_t *wlc, struct scb *scb, void *p, tx_status_t *txs);

#else /* AP */

#ifdef PROP_TXSTATUS
#define wlc_apps_pvb_update_from_host(a, b) do {} while (0)
#endif // endif

#define wlc_apps_attach(a) FALSE
#define wlc_apps_psq(a, b, c) FALSE
#define wlc_apps_suppr_frame_enq(a, b, c, d) FALSE

#define wlc_apps_bss_wd_ps_check(a, b) do {} while (0)

#define wlc_apps_scb_ps_off(a, b, c) do {} while (0)
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

#define wlc_apps_pps_retry(a, b, c, d) FALSE

#endif /* AP */

#if defined(WLC_HIGH) && defined(MBSS)
extern void wlc_apps_bss_ps_off_done(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_apps_bss_ps_on_done(wlc_info_t *wlc);
extern void wlc_apps_update_bss_bcmc_fid(wlc_info_t *wlc);
extern int wlc_apps_bcmc_ps_enqueue(wlc_info_t *wlc, struct scb *bcmc_scb, void *pkt);
#else
#define wlc_apps_bss_ps_off_done(wlc, bsscfg)
#define wlc_apps_bss_ps_on_done(wlc)
#define wlc_apps_update_bss_bcmc_fid(wlc)
#endif /* WLC_HIGH && MBSS */

#ifdef PROP_TXSTATUS
extern void wlc_apps_ps_flush_mchan(wlc_info_t *wlc, struct scb *scb);
#endif /* PROP_TXSTATUS && WLMCHAN */
#ifdef BCMDBG_TXSTUCK
extern void wlc_apps_print_scb_psinfo_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG_TXSTUCK */
extern void wlc_apps_ps_flush_flowid(wlc_info_t *wlc, struct scb *scb, uint16 flowid);
extern void wlc_apps_set_change_scb_state(wlc_info_t *wlc, struct scb *scb, bool reset);
#endif /* _wlc_apps_h_ */
