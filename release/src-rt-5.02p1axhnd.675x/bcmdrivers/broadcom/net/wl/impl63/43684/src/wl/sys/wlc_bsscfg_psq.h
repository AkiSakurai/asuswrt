/*
 * Per-BSS psq interface.
 * Used to save suppressed packets in the BSS.
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
 * $Id: wlc_bsscfg_psq.h 634444 2016-04-28 02:09:26Z $
 */
#ifndef _wlc_bsscfg_psq_h_
#define _wlc_bsscfg_psq_h_

#include <typedefs.h>
#include <wlc_types.h>

wlc_bsscfg_psq_info_t *wlc_bsscfg_psq_attach(wlc_info_t *wlc);
void wlc_bsscfg_psq_detach(wlc_bsscfg_psq_info_t *psqi);

#ifdef WL_BSSCFG_TX_SUPR
void wlc_bsscfg_tx_stop(wlc_bsscfg_psq_info_t *psqi, wlc_bsscfg_t *cfg);
void wlc_bsscfg_tx_start(wlc_bsscfg_psq_info_t *psqi, wlc_bsscfg_t *cfg);
bool wlc_bsscfg_tx_psq_enq(wlc_bsscfg_psq_info_t *psqi, wlc_bsscfg_t *cfg, void *sdu, uint prec);
void wlc_bsscfg_tx_check(wlc_bsscfg_psq_info_t *psqi);
bool wlc_bsscfg_tx_supr_enq(wlc_bsscfg_psq_info_t *psqi, wlc_bsscfg_t *cfg, void *pkt);
uint wlc_bsscfg_tx_pktcnt(wlc_bsscfg_psq_info_t *psqi, wlc_bsscfg_t *cfg);
void wlc_bsscfg_tx_flush(wlc_bsscfg_psq_info_t *psqi, wlc_bsscfg_t *cfg);
struct pktq * wlc_bsscfg_get_psq(wlc_bsscfg_psq_info_t *psqi, wlc_bsscfg_t *cfg);
#else /* WL_BSSCFG_TX_SUPR */
#define wlc_bsscfg_tx_stop(a, b) do { } while (0)
#define wlc_bsscfg_tx_start(a, b) do { } while (0)
#define wlc_bsscfg_tx_psq_enq(a, b, c, d) FALSE
#define wlc_bsscfg_tx_check(a) do { } while (0)
#define wlc_bsscfg_tx_supr_enq(a, b, c) ((void)(b), FALSE)
#define wlc_bsscfg_tx_pktcnt(a, b) 0
#define wlc_bsscfg_tx_flush(a, b) do { } while (0)
#define wlc_bsscfg_get_psq(a, b) (NULL)
#endif /* WL_BSSCFG_TX_SUPR */

#endif /* _wlc_bsscfg_psq_h_ */
