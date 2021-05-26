/*
 * 802.11ax MU scheduler and scheduler statistics module
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
 * $Id:$
 */

#ifndef _wlc_muscheduler_h_
#define _wlc_muscheduler_h_

#include <wlc_types.h>

/* musched dl policy definition */
#define MUSCHED_DL_POLICY_DISABLE		0
#define MUSCHED_DL_POLICY_FIXED			1
#define MUSCHED_DL_POLICY_TRIVIAL		2
#define MUSCHED_DL_POLICY_MAX			MUSCHED_DL_POLICY_TRIVIAL
#define MUSCHED_DL_POLICY_AUTO			(-1)

/* RU alloc mode */
#define MUSCHED_RUALLOC_AUTO		(-1)
#define MUSCHED_RUALLOC_RUCFG		0 /* badsed on rucfg table */
#define MUSCHED_RUALLOC_UCODERU		1 /* vasip based- ucode ru alloc */
#define MUSCHED_RUALLOC_MINRU		2 /* vasip based- minRU alloc */
#define MUSCHED_RUALLOC_MAX		MUSCHED_RUALLOC_MINRU

#define MUSCHED_RU_IDX_NUM		HE_MAX_2x996_TONE_RU_INDX /* 68 */
#define MUSCHED_RU_TYPE_NUM		D11AX_RU_MAX_TYPE
#define MUSCHED_RU_160MHZ		2
#define MUSCHED_RU_80MHZ		1
#define MUSCHED_RU_BMP_ROW_SZ		MUSCHED_RU_160MHZ
#define MUSCHED_RU_BMP_COL_SZ		((MUSCHED_RU_IDX_NUM - 1) / 8 + 1) /* 9 */

/* attach/detach */
wlc_muscheduler_info_t *wlc_muscheduler_attach(wlc_info_t *wlc);
void wlc_muscheduler_detach(wlc_muscheduler_info_t *musched);

/* DL OFDMA */
#define WLC_OFMDMA_MAX_RU	16 /* Max number of RUs per PPDU for an OFDMA frame
				    * This number will chip specific
				    */

bool wlc_musched_is_dlofdma_allowed(wlc_muscheduler_info_t *musched);
extern int wlc_musched_set_dlpolicy(wlc_muscheduler_info_t *musched, int16 dl_policy);
extern int wlc_scbmusched_set_dlschpos(wlc_muscheduler_info_t *musched, scb_t* scb, int8 schpos);
extern int wlc_scbmusched_get_dlsch(wlc_muscheduler_info_t *musched, scb_t* scb,
	int8* schidx, int8* schpos);
extern void wlc_scbmusched_set_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb, bool enable);
#ifdef MAC_AUTOTXV_OFF
extern int wlc_scbmusched_fifogrpidx_get(wlc_muscheduler_info_t *musched, scb_t* scb,
	uint8 * fifo_idx);
#endif // endif

extern uint8 wlc_musched_get_rualloctype(wlc_muscheduler_info_t *musched);

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
extern int wlc_musched_upd_ru_stats(wlc_muscheduler_info_t *musched, struct scb *scb,
	tx_status_t *txs);
#endif // endif

#ifdef PKTQ_LOG
int wlc_musched_dp_stats(wlc_muscheduler_info_t *musched, tx_status_t *txs, uint8 * count);
#endif // endif

bool wlc_musched_scb_isdlofdma_eligible(wlc_muscheduler_info_t *musched, scb_t* scb);
extern void wlc_musched_fill_link_entry(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg,
	scb_t *scb, d11linkmem_entry_t *link_entry);
extern void wlc_musched_link_entry_dump(d11linkmem_entry_t *link_entry, bcmstrbuf_t *b);
extern void wlc_musched_admit_dlclients(wlc_muscheduler_info_t *musched);
extern void wlc_musched_get_min_dlofdma_users(wlc_muscheduler_info_t *musched,
	uint16 *min_users, bool *allow_bw160);
extern void wlc_musched_update_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb);
extern bool wlc_musched_wfa20in80_enab(wlc_muscheduler_info_t *musched);
#endif /* _wlc_muscheduler_h_ */
