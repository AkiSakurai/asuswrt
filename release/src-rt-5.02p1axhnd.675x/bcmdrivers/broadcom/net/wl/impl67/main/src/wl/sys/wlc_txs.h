/*
 * txstatus related routines
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
 * $Id: wlc_txs.h 785562 2020-03-31 09:11:25Z $
 */

#ifndef __wlc_txstatus_h__
#define __wlc_txstatus_h__

#include <typedefs.h>
#include <wlc_types.h>
#include <d11.h>

void wlc_pkttag_scb_restore(void *ctxt, void* p);

bool wlc_dotxstatus(wlc_info_t *wlc, tx_status_t *txs, uint32 frm_tx2);

uint wlc_txs_alias_to_old_fmt(wlc_info_t *wlc, tx_status_macinfo_t* status);
bool wlc_should_retry_suppressed_pkt(wlc_info_t *wlc, void *p, uint status);
void wlc_print_txstatus(wlc_info_t *wlc, tx_status_t* txs);

/* DYNTXC module info */
struct wlc_txs_dyntxc_info {
	wlc_info_t *wlc;
	bool enable;
	uint8 feature;
	uint8 coefset;
	uint16 period_num;
	uint16 subperiod_num;
	uint8 candidate_num;
	uint8 coeff_set[DYNTXC_MAX_COEFNUM];
};
/* module */
extern wlc_txs_dyntxc_info_t *wlc_txs_dyntxc_attach(wlc_info_t *wlc);
extern void wlc_txs_dyntxc_detach(wlc_txs_dyntxc_info_t *dyntxc);

void wlc_txs_dyntxc_update(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec,
	d11ratemem_rev128_rate_t *rate, txpwr204080_t txpwrs_sdm);
void wlc_txs_dyntxc_cnt_update(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec);
bool wlc_txs_dyntxc_status(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec, bool apply_txc);
void wlc_txs_dyntxc_set(wlc_txs_dyntxc_info_t *dyntxc);
void wlc_txs_dyntxc_metric_calc(wlc_info_t *wlc, tx_status_t *txs, scb_t *scb, ratespec_t rspec);
void wlc_txs_dyntxc_metric_init(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec);
#endif /* __wlc_txstatus_h__ */
