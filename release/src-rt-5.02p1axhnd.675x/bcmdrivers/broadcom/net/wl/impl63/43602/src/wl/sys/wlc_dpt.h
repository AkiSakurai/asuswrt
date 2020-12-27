/*
 * DPT (Direct Packet Transfer) related header file
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
 * $Id: wlc_dpt.h 383589 2013-02-07 04:29:53Z $
*/

#ifndef _wlc_dpt_h_
#define _wlc_dpt_h_

#ifdef WLDPT
extern dpt_info_t *wlc_dpt_attach(wlc_info_t *wlc);
extern void wlc_dpt_detach(dpt_info_t *dpt);
extern bool wlc_dpt_cap(dpt_info_t *dpt);
extern void wlc_dpt_update_pm_all(dpt_info_t *dpt, wlc_bsscfg_t *cfg, bool state);
extern bool wlc_dpt_pm_pending(dpt_info_t *dpt, wlc_bsscfg_t *cfg);
extern struct scb *wlc_dpt_query(dpt_info_t *dpt, wlc_bsscfg_t *cfg,
	void *sdu, struct ether_addr *ea);
extern void wlc_dpt_used(dpt_info_t *dpt, struct scb *scb);
extern bool wlc_dpt_rcv_pkt(dpt_info_t *dpt, struct wlc_frminfo *f);
extern int wlc_dpt_set(dpt_info_t *dpt, bool on);
extern void wlc_dpt_cleanup(dpt_info_t *dpt, wlc_bsscfg_t *parent);
extern void wlc_dpt_free_scb(dpt_info_t *dpt, struct scb *scb);
extern void wlc_dpt_wpa_passhash_done(dpt_info_t *dpt, struct ether_addr *ea);
extern void wlc_dpt_port_open(dpt_info_t *dpt, struct ether_addr *ea);
extern wlc_bsscfg_t *wlc_dpt_get_parent_bsscfg(wlc_info_t *wlc, struct scb *scb);
#else	/* stubs */
#define wlc_dpt_attach(a) (dpt_info_t *)0x0dadbeef
#define	wlc_dpt_detach(a) do {} while (0)
#define	wlc_dpt_cap(a) FALSE
#define	wlc_dpt_update_pm_all(a, b, c) do {} while (0)
#define wlc_dpt_pm_pending(a, b) FALSE
#define wlc_dpt_query(a, b, c, d) NULL
#define wlc_dpt_used(a, b) do {} while (0)
#define wlc_dpt_rcv_pkt(a, b, c, d) do {} (FALSE)
#define wlc_dpt_set(a, b) do {} while (0)
#define wlc_dpt_cleanup(a, b) do {} while (0)
#define wlc_dpt_free_scb(a, b) do {} while (0)
#define wlc_dpt_wpa_passhash_done(a, b) do {} while (0)
#define wlc_dpt_port_open(a, b) do {} while (0)
#define wlc_dpt_get_parent_bsscfg(a, b) NULL
#endif /* WLDPT */

#endif /* _wlc_dpt_h_ */
