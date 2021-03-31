/**
 *
 * @file
 * @brief
 * Dynamic Tx Power Control module header.
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
 * $Id$
 */

#ifndef _wlc_dtpc_h_
#define _wlc_dtpc_h_

#ifdef WLC_DTPC

/* state machine */
enum {
    DTPC_READY =  0x0,
    DTPC_START =  0x1,
    DTPC_PROBE =  0x2,
    DTPC_STOP =   0x4,
    };

#define DTPC_PWROFFSET_NOCHANGE (-128)
#define DTPC_PWROFFSET_INVALID  (-512)
#define DTPC_BACKOFF_INVALID    (0)

enum {
    DTPC_INACTIVE =  0x0,
    DTPC_INPROG =    0x1,
    DTPC_TXC_INV =   0x2,
    DTPC_UPD_PSR =   0x4,
    };
#define DTPC_SUPPORTED(dtpc)    wlc_dtpc_active(dtpc)

/** default interface forward declarations */
extern wlc_dtpc_info_t *wlc_dtpc_attach(wlc_info_t *wlc);
extern void wlc_dtpc_detach(wlc_dtpc_info_t *dtpci);

/** external API function prototypes */
extern uint8 wlc_dtpc_active(wlc_dtpc_info_t *dtpci);
extern int32 wlc_dtpc_get_psr(wlc_dtpc_info_t *dtpci, scb_t *scb);
extern int16 wlc_dtpc_upd_pwroffset(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
	d11ratemem_rev128_rate_t *ratemem);
extern bool wlc_dtpc_is_pwroffset_changed(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec);
extern void wlc_dtpc_upd_sp(wlc_dtpc_info_t *dtpci, scb_t *scb, bool sp_started);
extern bool wlc_dtpc_power_context_set_channel(wlc_dtpc_info_t *dtpci, chanspec_t chanspec);
extern void wlc_dtpc_get_ready(wlc_dtpc_info_t *dtpci, scb_t *scb);
extern void wlc_dtpc_rate_change(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		bool goup_rateid, uint8 epoch, bool highest_rate, bool nss_chg);
extern uint8 wlc_dtpc_check_txs(wlc_dtpc_info_t *dtpci, scb_t *scb, tx_status_t *txs,
        ratespec_t rspec, uint32 prate_prim, uint32 psr_prim, uint32 prate_dn,
        uint32 psr_dn, uint32 psr_up, uint8 epoch, uint32 nupd, uint32 mrt,
        uint32 mrt_succ, bool go_prb, bool highest_mcs);
extern void wlc_dtpc_collect_psr(wlc_dtpc_info_t *dtpci, scb_t *scb, ratespec_t rspec,
		uint32 mrt, uint32 mrt_succ, uint32 fbr, uint32 fbr_succ,
		uint32 prim_psr, uint32 fbr_psr);

#endif /* WLC_DTPC */

#endif /* _wlc_dtpc_h */
