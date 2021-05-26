/**
 * @file
 * @brief
 * Common (OS-independent) portion of Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_ido.c 502978 2014-09-16 22:45:09Z $
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.1d.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <wlc_frmutil.h>
#include <wlc_pcb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_rate_sel.h>
#include <wlc_amsdu.h>
#ifdef WL11AC
#include <wlc_vht.h>
#endif /* WL11AC */
#include <wl_export.h>

#ifdef ACMAJORREV2_THROUGHPUT_OPT
#define PLDO_MOVING_AVG_AGING	        4
#define PLDO_MOVING_AVG_NORM	        12
/* 15% per threshold for Tx. PER = 1 - #success/#mpdus */
#define PLDO_PER_THRESH		        ((17 << PLDO_MOVING_AVG_NORM) / 20)
/* 5% per threshold for Rx. PER = #holes/#mpdus */
#define PLDO_HOLE_THRESH	        ((1 << PLDO_MOVING_AVG_NORM) / 20)
/* Number of Rx ampdu frames to be accumulated */
#define PLDO_RX_NFRM_LOG2               9
/* Maximum allowed number of xtalldo changes per second */
#define PLDO_TIME_OUT_THRESH            15
/* Freeze time if number of xtalldo change exceeds PLDO_TIME_OUT_THRESH in one sec period */
#define PLDO_TIME_OUT                   30
/* Period in sec to monitor how many seconds have xtalldo change */
#define PLDO_GLACIAL_WINDOW_LOG2        3
/* Moving average normalization for glacial monitor */
#define PLDO_GLACIAL_MONITOR_NORM       12
/* Max avg number of seconds that have xtalldo change allowed in the past PLDO_GLACIAL_WINDOW */
#define PLDO_GLACIAL_THRESH             (3 << (PLDO_GLACIAL_MONITOR_NORM - 2))
/* Glacial time to freeze any change of xtalldo */
#define PLDO_GLACIAL_TIME               180

/* XXX JIRA: SWWLAN-49399
 * Function: monitor per-device PER per mcs8/9 and try alternative radio ldo setting
 */
void
wlc_temp_evm_war_init(wlc_info_t *wlc)
{
	wlc_per_ldo_t *pldo;
	pldo = &wlc->pldo;
	memset(pldo, 0, sizeof(wlc_per_ldo_t));
	pldo->psr = (1 << PLDO_MOVING_AVG_NORM);
}

void
wlc_temp_evm_war(wlc_info_t *wlc, wlc_txh_info_t* txh_info, tx_status_t *txs, uint supr_status)
{
	wlc_per_ldo_t *pldo;
	uint8 mcs, frametype;
	uint txcnt, succ, succ_ratio;
	uint16 mctl, mch;
	pldo = &wlc->pldo;

	if (supr_status || txs->phyerr)
		return;

	mctl = ltoh16(txh_info->MacTxControlLow);
	if (!(mctl & D11AC_TXC_IACK))
		return;

	if (pldo->ldo_time_out > 0) {
		/* During Time Out, clear Tx counter and no accumulation */
		pldo->psr = (1 << PLDO_MOVING_AVG_NORM);
		return;
	}
	frametype = ltoh16(txh_info->PhyTxControlWord0) & PHY_TXC_FT_MASK;
	if (frametype != FT_VHT)
		return;

	if ((mcs = (wlc_txh_info_get_mcs(wlc, txh_info) & 0xf)) < 8)
		return;

	mch = ltoh16(txh_info->MacTxControlHigh);
	if (mch & D11AC_TXC_FIX_RATE) {
		txcnt = txs->status.s3 & 0xffff;
		succ = (txs->status.s3 >> 16) & 0xffff;
	} else {
		txcnt = txs->status.s3 & 0xff;
		succ = (txs->status.s3 >> 8) & 0xff;
	}
	if (txcnt == 0) return;

	/* Moving average of psr */
	succ_ratio = (succ << PLDO_MOVING_AVG_NORM) / txcnt;
	pldo->psr -= ((pldo->psr) >> PLDO_MOVING_AVG_AGING);
	pldo->psr += (succ_ratio >> PLDO_MOVING_AVG_AGING);

	if (pldo->psr < PLDO_PER_THRESH) {
		pldo->cur_idx = 1 - pldo->cur_idx;
		wlc_acphy_set_ldo(wlc->band->pi, pldo->cur_idx);
		WL_INFORM(("%s: cnt %u mcs 0x%x txcnt/succ %d %d, psr %u, "
			"currstate %s, thresh is %d \n",
			__FUNCTION__, pldo->count, mcs, txcnt, succ, pldo->psr,
			pldo->cur_idx ? "alternative" : "default", PLDO_PER_THRESH));
		pldo->psr = (1 << PLDO_MOVING_AVG_NORM);
		/* Clear Rx status after set xtalldo to avoid racing condition */
		pldo->nholes = 0;
		pldo->nrxfrm = 0;
		pldo->rxper = 0;
		pldo->count ++;
	} else {
		WL_INFORM(("%s: cnt %u mcs 0x%x txcnt/succ %d %d psr %u \n",
			__FUNCTION__, pldo->count, mcs, txcnt, succ, pldo->psr));
	}
	WL_INFORM(("%s:: Raw txstatus %08X %08X %08X || "
		"%08X %08X %08X %08X\n",
		__FUNCTION__,
		txs->status.raw_bits, txs->status.s3, txs->status.s4,
		txs->status.s5, txs->status.ack_map1, txs->status.ack_map2, txs->status.s8));

	return;
}

void wlc_ldo_war_rx_status(wlc_info_t *wlc, ratespec_t rspec, bool is_amsdu, uint8 isretry,
	uint16 aggtype, uint8 plcp_valid)
{
	/* Monitor Rx status for xtalldo switch */
	wlc_per_ldo_t *pldo;
	pldo = &wlc->pldo;

	if (plcp_valid) {
		/* First mpdu in ampdu */
		if (RSPEC_ISVHT(rspec) && ((rspec & RSPEC_VHT_MCS_MASK) > 7)) {
			wlc->isvhtmcs = 1;
		} else {
			wlc->isvhtmcs = 0;
		}
	}

	if (pldo->ldo_time_out > 0) {
		/* During Time Out, clear rx counters and no accumulation */
		pldo->nholes = 0;
		pldo->nrxfrm = 0;
		pldo->rxper = 0;
		return;
	}

	if (wlc->isvhtmcs) {
		if (!is_amsdu) {
			pldo->nrxfrm += 1;
		} else {
			if (aggtype == RXS_AMSDU_FIRST) {
				pldo->nrxfrm += 1;
			}
		}
	}

	if (pldo->nrxfrm == (1 << PLDO_RX_NFRM_LOG2)) {
		pldo->rxper = (pldo->nholes << (PLDO_MOVING_AVG_NORM -
			PLDO_RX_NFRM_LOG2));
		if (pldo->rxper > PLDO_HOLE_THRESH) {
			pldo->cur_idx = 1 - pldo->cur_idx;
			WL_INFORM(("%s: cur_idx %d, nrxfrm %d, nretry %d, retry ratio %d,"
				"threshold %d\n", __FUNCTION__, pldo->cur_idx,
				pldo->nrxfrm, pldo->nholes, pldo->rxper, PLDO_HOLE_THRESH));
			wlc_acphy_set_ldo(wlc->band->pi, pldo->cur_idx);
			pldo->count ++;
			/* Clear Tx status after set xtalldo to avoid racing condition */
			pldo->psr = (1 << PLDO_MOVING_AVG_NORM);
		}
		/* Clear Rx counters after 2^PLDO_RX_NFRM_LOG2 frames */
		pldo->nholes = 0;
		pldo->nrxfrm = 0;
		pldo->rxper = 0;
	}
}

void wlc_ldo_war(wlc_info_t *wlc)
{
	/* Monitor xtalldo changes, and set time out */
	wlc_per_ldo_t *pldo;
	uint8 istimeout = 0;

	pldo = &wlc->pldo;
	WL_INFORM(("%s: curidx %d, count %d, glacial_monitor %d long_ldo_time_out %d "
		"time_out %d, nrxfrm %d, nretr %d, psr %d\n",
		__FUNCTION__, pldo->cur_idx, pldo->count, pldo->glacial_monitor,
		pldo->long_ldo_time_out, pldo->ldo_time_out, pldo->nrxfrm,
		pldo->nholes, pldo->psr));

	if (pldo->long_ldo_time_out > 0) {
		pldo->long_ldo_time_out--;
		pldo->ldo_time_out = PLDO_TIME_OUT;
		return;
	} else {
		if (pldo->ldo_time_out > 0) {
			pldo->ldo_time_out--;
			return;
		}
	}

	/* Too many xtalldo changes in one second, time out and reset ldo state */
	if (pldo->count > PLDO_TIME_OUT_THRESH) {
		pldo->ldo_time_out = PLDO_TIME_OUT;
		istimeout = 1;
	}

	/* Moving average of the number of seconds that have xtalldo change in the past */
	pldo->glacial_monitor -= ((pldo->glacial_monitor) >> PLDO_GLACIAL_WINDOW_LOG2);
	pldo->glacial_monitor += ((istimeout <<
				   PLDO_GLACIAL_MONITOR_NORM) >> PLDO_GLACIAL_WINDOW_LOG2);

	/* Too many continuous changes of xtalldo in the past, glacial for PLDO_GLACIAL_TIME */
	if (pldo->glacial_monitor > PLDO_GLACIAL_THRESH) {
		pldo->long_ldo_time_out = PLDO_GLACIAL_TIME;
		pldo->glacial_monitor = 0;
	}

	pldo->count = 0;
}

#endif /* ACMAJORREV2_THROUGHPUT_OPT */
