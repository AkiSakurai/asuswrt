/**
 * @file
 * @brief
 * txstatus processing/debugging
 *
 * Copyright 2019 Broadcom
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
 * $Id: wlc_txs.c 780809 2019-11-04 14:36:50Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc_mbss.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_apps.h>
#include <wlc_scb.h>
#include <wlc_antsel.h>
#ifdef WLNAR
#include <wlc_nar.h>
#endif // endif
#if defined(SCB_BS_DATA)
#include <wlc_bs_data.h>
#endif /* SCB_BS_DATA */
#include <wlc_ampdu.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#include <wlc_p2p.h>
#include <wlc_scb_ratesel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif // endif
#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */
#include <wlc_assoc.h>
#if defined(PROP_TXSTATUS)
#include <wlc_wlfc.h>
#endif // endif
#include <wlc_txbf.h>
#include <wlc_pcb.h>
#include <wlc_pm.h>
#include <wlc_tx.h>
#include <wlc_bsscfg_psq.h>
#include <wlc_pspretend.h>
#include <wlc_qoscfg.h>
#include <wlc_event_utils.h>
#include <wlc_perf_utils.h>
#include <wlc_txs.h>
#include <wlc_phy_hal.h>
#include <wlc_dbg.h>
#include <wlc_macdbg.h>
#include <wlc_stf.h>
#ifdef WLC_SW_DIVERSITY
#include <wlc_swdiv.h>
#endif // endif
#include <wlc_log.h>
#ifdef WL_MU_TX
#include <wlc_mutx.h>
#endif // endif
#include <wlc_vht.h>
#include <wlc_bmac.h>
#ifdef WL_LEAKY_AP_STATS
#include <wlc_leakyapstats.h>
#endif /* WL_LEAKY_AP_STATS */
#ifdef WL_PROXDETECT
#include <wlc_ftm.h>
#include <wlc_pdsvc.h>
#endif // endif
#include <wlc_ratelinkmem.h>
#include <wlc_musched.h>
#include <wlc_ulmu.h>
#include <wlc_iocv.h>

#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

#define TXS_DYNTXC_COEF_OMIT_CORE0	0xe
#define TXS_DYNTXC_COEF_OMIT_CORE1	0xd
#define TXS_DYNTXC_COEF_OMIT_CORE2	0xb
#define TXS_DYNTXC_COEF_OMIT_CORE3	0x7
#define TXS_DYNTXC_COEF_ALLCORES	0xf

#if defined(STA) && defined(WL_TX_STALL) && defined(WL_TX_CONT_FAILURE)
const int8 legacy_rate_to_idx[WLC_MAXRATE/2 + 1] = {
	-1,  0,  1, -1, -1,  2,  4, -1,	/* 1, 2, 5.5 & 6 */
	-1,  5, -1,  3,  6, -1, -1, -1,	/* 9, 11 & 12 */
	-1, -1,  7, -1, -1, -1, -1, -1,	/* 18 */
	 8, -1, -1, -1, -1, -1, -1, -1,	/* 24 */
	-1, -1, -1, -1,  9, -1, -1, -1,	/* 36 */
	-1, -1, -1, -1, -1, -1, -1, -1,
	10, -1, -1, -1, -1, -1, 11,	/* 48 & 54 */
};
#define WL_TXFAIL_FLAG_LEGACY	0x1
#define WL_TXFAIL_FLAG_HT	0x2
#define WL_TXFAIL_FLAG_VHT	0x4
#define WL_TXFAIL_FLAG_AMPDU	0x100

void wlc_txfail_cnt_reset(wlc_info_t *wlc)
{
	wlc_tx_stall_error_info_t *txei = wlc->tx_stall->error;

	ASSERT(txei);
	ASSERT(txei->txfail_consecutive_total != 0);

	if (txei->txfail_consecutive_flags & (WL_TXFAIL_FLAG_HT | WL_TXFAIL_FLAG_VHT)) {
		memset(txei->txfail_mcs_cnt, 0, sizeof(txei->txfail_mcs_cnt));
	}
	if (txei->txfail_consecutive_flags & WL_TXFAIL_FLAG_LEGACY) {
		memset(txei->txfail_legacy_cnt, 0, sizeof(txei->txfail_legacy_cnt));
	}
	txei->txfail_consecutive_total = 0;
	txei->txfail_consecutive_flags = 0;
}

void wlc_txfail_cnt_inc(wlc_info_t* wlc, bool ampdu, ratespec_t tx_rspec)
{
	wlc_tx_stall_error_info_t *txei = wlc->tx_stall->error;
	int nss, rate_idx;

	ASSERT(txei);

	txei->txfail_consecutive_total++;
	if (ampdu)
		txei->txfail_consecutive_flags |= WL_TXFAIL_FLAG_AMPDU;

	if (RSPEC_ISVHT(tx_rspec)) {
		nss = ((tx_rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT) - 1;
		rate_idx = (tx_rspec & WL_RSPEC_VHT_MCS_MASK);
		ASSERT((nss >= 0) && (nss < BCMPHYCORENUM));
		ASSERT((rate_idx >= 0) && (rate_idx < (WL_NUM_RATES_VHT+WL_NUM_RATES_EXTRA_VHT)));
		txei->txfail_mcs_cnt[nss][rate_idx]++;
		txei->txfail_consecutive_flags |= WL_TXFAIL_FLAG_VHT;
	} else if (RSPEC_ISHT(tx_rspec)) {
		nss = (tx_rspec & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT;
		rate_idx = (tx_rspec & WLC_RSPEC_HT_MCS_MASK);
		ASSERT((nss >= 0) && (nss < BCMPHYCORENUM));
		ASSERT((rate_idx >= 0) && (rate_idx < WL_NUM_RATES_MCS_1STREAM));
		txei->txfail_mcs_cnt[nss][rate_idx]++;
		txei->txfail_consecutive_flags |= WL_TXFAIL_FLAG_HT;
	} else {
		ASSERT(RSPEC_ISLEGACY(tx_rspec));
		ASSERT((tx_rspec & WL_RSPEC_LEGACY_RATE_MASK) <= WLC_MAXRATE);
		rate_idx = legacy_rate_to_idx[(tx_rspec & WL_RSPEC_LEGACY_RATE_MASK)/2];
		ASSERT((rate_idx >= 0));
		txei->txfail_legacy_cnt[rate_idx]++;
		txei->txfail_consecutive_flags |= WL_TXFAIL_FLAG_LEGACY;
	}

#ifdef EVENT_LOG_COMPILE
	/* Check for reporting condition */
	if (txei->txfail_consecutive_total < wlc->tx_stall->failure_to_rpt) {
		return;
	}

	if (txei->txfail_consecutive_flags & WL_TXFAIL_FLAG_LEGACY) {
		EVENT_LOG(EVENT_LOG_TAG_TXFAIL,
			"TX consecutive failure[seq=%d][flags=0x%04x][nss=%d]:"
			" %d %d %d %d %d %d %d %d %d %d %d %d\n",
			txei->txfail_consecutive_seq,
			txei->txfail_consecutive_flags & WL_TXFAIL_FLAG_LEGACY,
			0,
			txei->txfail_legacy_cnt[0], txei->txfail_legacy_cnt[1],
			txei->txfail_legacy_cnt[2], txei->txfail_legacy_cnt[3],
			txei->txfail_legacy_cnt[4], txei->txfail_legacy_cnt[5],
			txei->txfail_legacy_cnt[6], txei->txfail_legacy_cnt[7],
			txei->txfail_legacy_cnt[8], txei->txfail_legacy_cnt[9],
			txei->txfail_legacy_cnt[10], txei->txfail_legacy_cnt[11]);
	}

	if (txei->txfail_consecutive_flags & (WL_TXFAIL_FLAG_HT | WL_TXFAIL_FLAG_VHT)) {
		for (nss = 0; nss < BCMPHYCORENUM; nss++) {
			EVENT_LOG(EVENT_LOG_TAG_TXFAIL,
				"TX consecutive failure[seq=%d][flags=0x%04x][nss=%d]:"
				" %d %d %d %d %d %d %d %d %d %d %d %d\n",
				txei->txfail_consecutive_seq,
				txei->txfail_consecutive_flags & ~WL_TXFAIL_FLAG_LEGACY,
				(nss + 1),
				txei->txfail_mcs_cnt[nss][0], txei->txfail_mcs_cnt[nss][1],
				txei->txfail_mcs_cnt[nss][2], txei->txfail_mcs_cnt[nss][3],
				txei->txfail_mcs_cnt[nss][4], txei->txfail_mcs_cnt[nss][5],
				txei->txfail_mcs_cnt[nss][6], txei->txfail_mcs_cnt[nss][7],
				txei->txfail_mcs_cnt[nss][8], txei->txfail_mcs_cnt[nss][9],
				txei->txfail_mcs_cnt[nss][10], txei->txfail_mcs_cnt[nss][11]);
		}
	}
	(txei->txfail_consecutive_seq)++;

	/* Clear all counters to zero after each report */
	wlc_txfail_cnt_reset(wlc);
#endif /* EVENT_LOG_COMPILE */
}
#endif /* STA && WL_TX_STALL && WL_TX_CONT_FAILURE */

static void wlc_pkttag_scb_restore_ex(wlc_info_t* wlc, void* p, wlc_txh_info_t* txh_info,
	wlc_bsscfg_t **bsscfg_out, struct scb **scb_out);

#if defined(BCMDBG) || defined(ENABLE_CORECAPTURE)
static void
wlc_print_txs_status_raw(uint16 s)
{
	printf("Raw\n [15:12]  %d  frame attempts\n",
	       (s & TX_STATUS_FRM_RTX_MASK) >> TX_STATUS_FRM_RTX_SHIFT);
	printf(" [11:8]  %d  rts attempts\n",
	       (s & TX_STATUS_RTS_RTX_MASK) >> TX_STATUS_RTS_RTX_SHIFT);
	printf("    [7]  %d  PM\n", ((s & TX_STATUS_PMINDCTD) ? 1 : 0));
	printf("    [6]  %d  intermediate status\n", ((s & TX_STATUS_INTERMEDIATE) ? 1 : 0));
	printf("    [5]  %d  AMPDU\n", (s & TX_STATUS_AMPDU) ? 1 : 0);
	printf("  [4:2]  %d  Suppress Reason\n",
	   ((s & TX_STATUS_SUPR_MASK) >> TX_STATUS_SUPR_SHIFT));
	printf("    [1]  %d  acked\n", ((s & TX_STATUS_ACK_RCV) ? 1 : 0));
}

static void
wlc_print_txs_status_aqm_raw(tx_status_t* txs)
{
	uint16 status_bits = txs->status.raw_bits;

	printf("Raw\n[0]    %d Valid\n", ((status_bits & TX_STATUS_VALID) != 0));
	printf("[2]    %d IM\n", ((status_bits & TX_STATUS40_INTERMEDIATE) != 0));
	printf("[3]    %d PM\n", ((status_bits & TX_STATUS40_PMINDCTD) != 0));
	printf("[7-4]  %d Suppr\n",
		((status_bits & TX_STATUS40_SUPR) >> TX_STATUS40_SUPR_SHIFT));
	printf("[14:8] %d Ncons\n", ((status_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT));
	printf("[15]   %d Acked\n", (status_bits & TX_STATUS40_ACK_RCV) != 0);
	printf("raw txstatus %04X %04X %04X | s3-5 %08X %08X %08X | "
		"%08X %08X | s8 %08X\n",
		txs->status.raw_bits, txs->frameid, txs->sequence,
		txs->status.s3, txs->status.s4, txs->status.s5,
		txs->status.ack_map1, txs->status.ack_map2, txs->status.s8);
}
#endif /* BCMDBG || ENABLE_CORECAPTURE */

void
wlc_print_txstatus(wlc_info_t* wlc, tx_status_t* txs)
{
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_PRHDRS) || \
	defined(WLMSG_PRPKT) || defined(ENABLE_CORECAPTURE)
	uint16 s = txs->status.raw_bits;
	uint8 txs_mutype = TX_STATUS_MUTP_VHTMU;
	static const char *supr_reason[] = {
		"None", "PMQ Entry", "Flush request",
		"Previous frag failure", "Channel mismatch",
		"Lifetime Expiry", "Underflow", "P2P Nack",
		"PPS", "TKIP phase-1 key", "Unused", "AGG0", "Invalid Linkmem"
	};

	BCM_REFERENCE(txs_mutype);

	printf("\ntxpkt (MPDU) Complete\n");

	printf("FrameID: 0x%04x   ", txs->frameid);
	printf("Seq: 0x%04x   ", txs->sequence);
	printf("TxStatus: 0x%04x", s);
	printf("\n");

	printf("ACK %d IM %d PM %d Suppr %d (%s)\n",
	       txs->status.was_acked, txs->status.is_intermediate,
	       txs->status.pm_indicated, txs->status.suppr_ind,
	       (txs->status.suppr_ind < ARRAYSIZE(supr_reason) ?
	        supr_reason[txs->status.suppr_ind] : "Unkn supr"));
	printf("CNT(rts_tx)=%d CNT(frag_tx_cnt)=%d CNT(cts_rx_cnt)=%d\n",
		txs->status.rts_tx_cnt, txs->status.frag_tx_cnt,
		txs->status.cts_rx_cnt);

	printf("DequeueTime: 0x%08x ", txs->dequeuetime);
	printf("LastTxTime: 0x%08x ", txs->lasttxtime);
	printf("PHYTxErr:   0x%04x ", txs->phyerr);
	printf("RxAckRSSI: 0x%04x ", (txs->ackphyrxsh & PRXS1_JSSI_MASK) >> PRXS1_JSSI_SHIFT);
	printf("RxAckSQ: 0x%04x", (txs->ackphyrxsh & PRXS1_SQ_MASK) >> PRXS1_SQ_SHIFT);
	printf("\n");

#if defined(BCMDBG) || defined(ENABLE_CORECAPTURE)
	if (txs->phyerr) {
		wlc_dump_phytxerr(wlc, txs->phyerr);
	}

	txs_mutype = TX_STATUS_MUTYP(wlc->pub->corerev, txs->status.s5);

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		wlc_print_txs_status_raw(txs->status.raw_bits);
	} else {
		wlc_print_txs_status_aqm_raw(txs);
		if ((txs->status.s5 & TX_STATUS64_MUTX)) {
			if (txs_mutype == TX_STATUS_MUTP_VHTMU) {
				printf("MUTX VHTMU: c%ds%d sgi %d bw %d txpwr %d\n"
					"gid %d gpos %d gbmp 0x%x mutxcnt %d snr %d\n",
					TX_STATUS64_MU_MCS(txs->status.s4),
					TX_STATUS64_MU_NSS(txs->status.s4)+1,
					TX_STATUS64_MU_SGI(txs->status.s3),
					TX_STATUS64_MU_BW(txs->status.s3),
					TX_STATUS64_MU_TXPWR(txs->status.s3),
					TX_STATUS64_MU_GID(txs->status.s3),
					TX_STATUS64_MU_GPOS(txs->status.s4),
					TX_STATUS64_MU_GBMP(txs->status.s4),
					TX_STATUS64_MU_TXCNT(txs->status.s4),
					TX_STATUS64_MU_SNR(txs->status.s4));
			} else if (txs_mutype == TX_STATUS_MUTP_HEOM) {
				printf("MUTX HEOM: x%ds%d ru %d rtidx %d\n",
					TX_STATUS64_MU_MCS(txs->status.s4),
					TX_STATUS64_MU_NSS(txs->status.s4)+1,
					TX_STATUS128_HEOM_RUIDX(txs->status.s4),
					TX_STATUS128_HEOM_RTIDX(txs->status.s4));
			} else if (txs_mutype == TX_STATUS_MUTP_HEMM) {
				printf("MUTX HEMM: x%ds%d gi %d bw %d txpwr %d\n"
					"gpos %d gbmp 0x%x mutxcnt %d snr %d\n",
					TX_STATUS64_MU_MCS(txs->status.s4),
					TX_STATUS64_MU_NSS(txs->status.s4)+1,
					TX_STATUS64_MU_SGI(txs->status.s3),
					TX_STATUS64_MU_BW(txs->status.s3),
					TX_STATUS64_MU_TXPWR(txs->status.s3),
					TX_STATUS64_MU_GPOS(txs->status.s4),
					TX_STATUS64_MU_GBMP(txs->status.s4),
					TX_STATUS64_MU_TXCNT(txs->status.s4),
					TX_STATUS64_MU_SNR(txs->status.s4));
			} else {
				printf("MUTX unsuported MU type: %d\n",
					txs_mutype);
			}
		}
	}
	printf("txpktpend AC_BK %d AC_BE %d AC_VI %d AC_VO %d BCMC %d ATIM %d\n",
		TXPKTPENDGET(wlc, TX_AC_BK_FIFO), TXPKTPENDGET(wlc, TX_AC_BE_FIFO),
		TXPKTPENDGET(wlc, TX_AC_VI_FIFO), TXPKTPENDGET(wlc, TX_AC_VO_FIFO),
		TXPKTPENDGET(wlc, TX_BCMC_FIFO), TXPKTPENDGET(wlc, TX_ATIM_FIFO));
#endif /* BCMDBG || ENABLE_CORECAPTURE */
#endif	/* BCMDBG || BCMDBG_ERR || WLMSG_PRHDRS || WLMSG_PRPKT */
}

static void
wlc_txs_clear_supr(wlc_info_t *wlc, tx_status_macinfo_t *status)
{
	ASSERT(status);
	status->suppr_ind = TX_STATUS_SUPR_NONE;
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		status->raw_bits &= ~TX_STATUS_SUPR_MASK;
	} else {
		status->raw_bits &= ~TX_STATUS40_SUPR;
	}
}

/* XXX 4360: used mainly for pkt callback -- would prefer not using this;
 * XXX can perhaps narrow down to ack, pm ind, suppr?
 */
uint16
wlc_txs_alias_to_old_fmt(wlc_info_t *wlc, tx_status_macinfo_t* status)
{
	ASSERT(status);

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		return status->raw_bits;
	} else {
		uint16 status_bits = 0;

		if (status->is_intermediate) {
			status_bits |= TX_STATUS_INTERMEDIATE;
		}
		if (status->pm_indicated) {
			status_bits |= TX_STATUS_PMINDCTD;
		}
		status_bits |= (status->suppr_ind << TX_STATUS_SUPR_SHIFT);
		if (status->was_acked) {
			status_bits |= TX_STATUS_ACK_RCV;
		}
		/* frag tx cnt */
		if (status->frag_tx_cnt != 0) {
			status_bits |= (TX_STATUS_FRM_RTX_MASK &
				(status->frag_tx_cnt << TX_STATUS_FRM_RTX_SHIFT));
		} else {
			/* RTS retry also shall be treat as TX retry */
			status_bits |= (TX_STATUS_FRM_RTX_MASK &
				(status->rts_tx_cnt << TX_STATUS_FRM_RTX_SHIFT));
		}

		status_bits |= (status->raw_bits & TX_STATUS_VALID);

		return status_bits;
	}
}

static INLINE bool
wlc_txs_reg_mpdu(wlc_info_t *wlc, tx_status_t *txs)
{
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		return ((txs->status.raw_bits & TX_STATUS_AMPDU) == 0);
	} else {
		/* valid indications are lacking for rev >= 40 */
		return FALSE;
	}
}

static void
wlc_txs_dump_status_info(wlc_info_t *wlc, tx_status_t *txs, uint queue)
{
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		WL_ERROR(("wl%d raw txstatus %04X %04X %04X %04X\n",
			wlc->pub->unit, txs->status.raw_bits,
			txs->frameid, txs->sequence, txs->phyerr));
	} else {
		WL_ERROR(("wl%d raw txstatus %04X %04X | %04X %04X | %08X %08X || "
			"%08X %08X | %08X\n", wlc->pub->unit,
			txs->status.raw_bits, txs->frameid, txs->sequence, txs->phyerr,
			txs->status.s3, txs->status.s4, txs->status.s5,
			txs->status.ack_map1, txs->status.ack_map2));
	}
	WL_ERROR(("pktpend: %d %d %d %d %d \n",
		TXPKTPENDGET(wlc, TX_AC_BK_FIFO),
		TXPKTPENDGET(wlc, TX_AC_BE_FIFO),
		TXPKTPENDGET(wlc, TX_AC_VI_FIFO),
		TXPKTPENDGET(wlc, TX_AC_VO_FIFO),
		TXPKTPENDGET(wlc, TX_BCMC_FIFO)));
	if (MU_TX_ENAB(wlc) || HE_MMU_ENAB(wlc->pub)) {
		int i = 8;
		for (; i < WLC_HW_NFIFO_TOTAL(wlc); i += 3) {
			WL_ERROR(("Fifo %2d: %-4d %-4d %-4d%s\n", i,
				TXPKTPENDGET(wlc, i),
				TXPKTPENDGET(wlc, i+1),
				TXPKTPENDGET(wlc, i+2),
				i < WLC_HW_NFIFO_INUSE(wlc) ? "" : " (unused)"));
		}
	}
	WL_ERROR(("frameid 0x%x queue %d\n", txs->frameid, queue));
}
#ifdef WL_TRAFFIC_THRESH
static void
wlc_trf_mgmt_scb_txfail_detect(struct scb *scb, wlc_bsscfg_t *bsscfg,
		wlc_txh_info_t *txh_info, uint8 queue)
{
	struct scb_trf_info *scb_intfer_stats;
	wlc_intfer_params_t *intfer_params;
	int delta;
	uint8 idx, cnt;
	uint32 tot_txfail = 0;
	wlc_traffic_thresh_event_t data;

	if (!(scb->trf_enable_flag & (1 << queue)))
		return;
	intfer_params = &bsscfg->trf_scb_params[queue];
	scb_intfer_stats = &scb->scb_trf_data[queue];
	cnt = intfer_params->num_secs;
	if (!intfer_params->num_secs || !intfer_params->thresh) {
	       return;
	}
	/* cleanup txfail histo, if current one occurs too late from last last txfail
	 * histo[] bin is arrange as bellow:
	 * 1) each element of histo[] contains txfail for one sec
	 * 2) hist[i+1] contains (i+1)th txfail , and histo[i] contains ith txfail
	 * 3) idx points current txfail element in histo[]
	 */
	if (!scb_intfer_stats->timestamp)
		scb_intfer_stats->timestamp = scb->used;
	delta = (scb->used - scb_intfer_stats->timestamp);
	if ((scb->used < scb_intfer_stats->timestamp) || (delta >= intfer_params->num_secs)) {
		WL_TRF_MGMT(("intfer reset histo: used:%d last:%d\n",
			scb->used, scb_intfer_stats->timestamp));
		memset(scb_intfer_stats, 0, sizeof(struct scb_trf_info));
		scb_intfer_stats->timestamp = scb->used;
		scb_intfer_stats->idx = 0;
		scb_intfer_stats->histo[scb_intfer_stats->idx] = 1;
		return;
	}

	/* cleanup the histo data between last txfail and cur txfail
	 * due to no txfail occurs in between
	 */
	for (cnt = 0; cnt < delta; cnt++) {
		scb_intfer_stats->idx = MODINC(scb_intfer_stats->idx, WL_TRF_MAX_SECS);
		scb_intfer_stats->histo[scb_intfer_stats->idx] = 0;
	}

	/* inc txfail counter and update timestamp */
	scb_intfer_stats->histo[scb_intfer_stats->idx] += 1;
	if (delta)
		scb_intfer_stats->timestamp = scb->used;

	WL_TRF_MGMT(("intfer cleanup histo: last:%d, txfail[%d]:%d\n",
		scb_intfer_stats->timestamp, scb_intfer_stats->idx,
		scb_intfer_stats->histo[scb_intfer_stats->idx]));

	for (idx = 0; idx < WL_TRF_MAX_SECS; idx++) {
		WL_TRF_MGMT(("%d\t", scb_intfer_stats->histo[idx]));
	}
	WL_TRF_MGMT(("\n"));

	/* accumulate histo stats and detect intferernce */
	idx = MODSUB(scb_intfer_stats->idx, (intfer_params->num_secs-1), 8);
	for (cnt = 0; cnt < intfer_params->num_secs; cnt++) {
		tot_txfail += scb_intfer_stats->histo[idx];
		idx = MODINC(idx, WL_TRF_MAX_SECS);
	}
	/* Send interference detection event if condition is matched */
	if ((tot_txfail >= (intfer_params->thresh))) {
		/* copy hist_stats to event */
		data.type = queue;
		data.version = WLC_E_TRAFFIC_THRESH_VER;
		data.length = WLC_E_TRAFFIC_THRESH_LEN;
		data.count = tot_txfail;
		/* Send Event */
		wlc_bss_mac_event(bsscfg->wlc, bsscfg, WLC_E_TXFAIL_TRFTHOLD,
				(struct ether_addr *)txh_info->TxFrameRA,
				0, 0, 0, &data,	sizeof(data));
		/* cleanup txfail_histo to restart txfail detection */
		memset(scb_intfer_stats, 0, sizeof(struct scb_trf_info));
	}
}
static void
wlc_traffic_thresh_incr(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_txh_info_t *txh_info, uint queue)
{
	wlc_traffic_thresh_event_t data;
	wlc_trf_data_t *ptr;
	wlc_intfer_params_t *val;
	if ((!wlc) || (!bsscfg) || (!txh_info)) {
		return;
	}
	if (!(bsscfg->trf_cfg_enable & (1 << queue))) {
		return;
	}
	ptr = &bsscfg->trf_cfg_data[queue];
	val = &bsscfg->trf_cfg_params[queue];
	if (!val->num_secs || !val->thresh) {
		return;
	}
	(ptr->num_data[ptr->cur])++;
	(ptr->count)++;
	if (ptr->count >= val->thresh) {
		int i;
		data.type = queue;
		data.version = WLC_E_TRAFFIC_THRESH_VER;
		data.length = WLC_E_TRAFFIC_THRESH_LEN;
		data.count = ptr->count;
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_TXFAIL_TRFTHOLD,
			(struct ether_addr *)txh_info->TxFrameRA,
			0, 0, 0, &data,	sizeof(data));
		for (i = 0; i < val->num_secs; i++) {
			ptr->num_data[i] = 0;
		}
		ptr->count = 0;
		ptr->cur = 0;
	}
}
#endif /* WL_TRAFFIC_THRESH */

#if defined(WL_MU_TX) && defined(WL11AX)

#ifdef WLC_MACDBG_TRIGBSR
static uint16 bsr_scaling_factor [4] = {
	D11_BSR_SHIFT_SF0, D11_BSR_SHIFT_SF1, D11_BSR_SHIFT_SF2, D11_BSR_SHIFT_SF3
};
#endif /* WLC_MACDBG_TRIGBSR */

static void BCMFASTPATH
wlc_print_trigtxstatus(wlc_info_t *wlc, tx_status_t *txs, ratesel_tgtxs_t *tgtxs)
{
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_PRHDRS) || \
	defined(WLMSG_PRPKT) || defined(ENABLE_CORECAPTURE) || defined(WLMSG_MAC)
	uint16 txsid, uidx, uaid;
	char rssi_str[10];
	int16 rssi, trssi;
	uint8 islast, reason;
	uint32 cmi01, cmi12;
	uint16 usri0, usri1, usri2;
#ifdef WLC_MACDBG_TRIGBSR
	uint32 q_size;
	uint16 q_sf, q_sfsize;
#endif /* WLC_MACDBG_TRIGBSR */

	cmi01 = TGTXS_CMNINFOP1(txs->status.s2);
	txsid = TGTXS_SEQ(txs->status.s1);
	uidx  = TGTXS_USRIDX(txs->status.s1);
	reason = TGTXS_REASON(txs->status.s1);

#ifdef WLC_MACDBG_TRIGBSR
	if ((D11TRIGCI_TYPE(cmi01) == HE_TRIG_TYPE_BSR_FRM) && (TGTXS_HIGH(txs->status.s4) != 0)) {
		q_sfsize = TGTXS_LOW(txs->status.s4) >> (D11_BSR_QUEUE_SIZE_ALL_SHIFT - 16);
		q_sf = (TGTXS_HIGH(txs->status.s3) & D11_BSR_SF_MASK) >> D11_BSR_SF_SHIFT;
		q_size = q_sfsize << bsr_scaling_factor[q_sf];
		printf("wl%d: tgtxs %d %d / %d "
			"bsrpinfo: bitmap: %04x | QoSperTID: 0[%04x] 1[%04x] 2[%04x] 3[%04x]"
			"4[%04x] 5[%04x] 6[%04x] 7[%04x] | htc: %04x%04x (Q_size: %dB)\n",
			wlc->pub->unit, txsid, uidx, TGTXS_NUSR(txs->status.s1),
			TGTXS_HIGH(txs->status.s4), TGTXS_LOW(txs->status.s3),
			TGTXS_HIGH(txs->status.s5),	TGTXS_LOW(txs->status.ack_map1),
			TGTXS_HIGH(txs->status.ack_map1), TGTXS_LOW(txs->status.ack_map2),
			TGTXS_HIGH(txs->status.ack_map2), TGTXS_LOW(txs->status.s8),
			TGTXS_HIGH(txs->status.s8), TGTXS_LOW(txs->status.s4),
			TGTXS_HIGH(txs->status.s3), q_size);
		goto done;
	}
#endif /* WLC_MACDBG_TRIGBSR */
	uaid = TGTXS_AID(txs->status.s4);
	rssi = TGTXS_PHYRSSI(txs->status.s8);
	rssi = ((rssi) & PHYRSSI_SIGN_MASK) ? (rssi - PHYRSSI_2SCOMPLEMENT) : rssi;
	islast = TGTXS_LAST(txs->status.s5);
	if (rssi < 0) {
		rssi = -rssi;
		sprintf(rssi_str, "-%d.%02d ", rssi >> 2, (rssi & 0x3) * 25);
	} else {
		sprintf(rssi_str, " %d.%02d ", rssi >> 2, (rssi & 0x3) * 25);
	}

	if ((txsid & 0xff) == 0) {
		/* print a header line now and then */
		printf("Tgtxs: txsid uidx / nusr uaid rspbmp reason | "
			"lfifo gfcs qnull agglen qosctl | ruidx rate rssi uph rtidx txcnt | "
			"tgsts tbcnt trcnt isLast\n");
	}
	printf("wl%d: tgtxs %d %d / %d %d %04x %01x | rxs: %2d %2d %d %6d 0x%04x "
	       "| rate: %3d 0x%02x %s 0x%02x %d %d | sts: %d %d %d%s\n",
	       /* section: group */
	       wlc->pub->unit, txsid, uidx,
	       TGTXS_NUSR(txs->status.s1), uaid,
	       TGTXS_GDRSP(txs->status.s2), reason,
	       /* section: rxs */
	       tgtxs->gdelim, tgtxs->gfcs,
	       TGTXS_QNCNT(txs->status.ack_map1),
	       TGTXS_AGGLEN(txs->status.ack_map1),
	       TGTXS_QOS(txs->status.ack_map2),
	       /* section: rate */
	       tgtxs->ruidx,
	       WL_RSPEC_RATE_MASK & tgtxs->txrspec,
	       rssi_str, TGTXS_UPH(txs->status.s8),
	       tgtxs->rtidx, tgtxs->txcnt,
	       /* section: state */
	       TGTXS_TGSTS(txs->status.s3),
	       TGTXS_BC(txs->status.s8),
	       TGTXS_TRCNT(txs->status.s3),
	       islast ? " L" : "");

	if (uidx == 0) {
		/* for the first txstatus in the group, print commond info */
		cmi12 = TGTXS_CMNINFOP2(txs->status.s3);
		cmi01 = (cmi12 << 16) | cmi01;
		cmi12 = (TGTXS_CMNINFOP3(txs->status.s3) << 16) | cmi12;
		printf("common: %04x %04x %02x | t %d lsig 0x%03x bw %d "
		       "cp %d nltf %d lext %d txpwr %d pe %d\n",
		       TGTXS_CMNINFOP1(txs->status.s2),
		       TGTXS_CMNINFOP2(txs->status.s3),
		       TGTXS_CMNINFOP3(txs->status.s3),
		       D11TRIGCI_TYPE(cmi01),
		       D11TRIGCI_LSIG(cmi01),
		       D11TRIGCI_BW(cmi01),
		       D11TRIGCI_CPLTF(cmi01),
		       D11TRIGCI_NLTF(cmi01),
		       D11TRIGCI_LEXT(cmi01),
		       D11TRIGCI_TXPWR(cmi12),
		       D11TRIGCI_PE(cmi12));
	}
	usri0 = TGTXS_USRINFOP1(txs->status.s4);
	usri1 = TGTXS_USRINFOP2(txs->status.s4);
	usri2 = TGTXS_USRINFOP3(txs->status.ack_map2);
	trssi = D11TRIGUI_TRSSI(usri2);
	printf("usrinfo: %04x %04x %04x | ldpc %d dcm %d trssi %2d (%d dBm) %02x\n",
	       usri0, usri1, usri2,
	       D11TRIGUI_LDPC(usri1), D11TRIGUI_DCM(usri1),
	       trssi, trssi - 110, D11TRIGUI_VAR(usri2));

#ifdef WLC_MACDBG_TRIGBSR
done:
#endif /* WLC_MACDBG_TRIGBSR */
#if defined(DONGLEBUILD)
	printf("raw 0x%08x 0x%08x 0x%08x 0x%08x ",
	       txs->status.s1, txs->status.s2,
	       txs->status.s3, txs->status.s4);
	printf("0x%08x 0x%08x 0x%08x 0x%08x\n",
	       txs->status.s5, txs->status.ack_map1,
	       txs->status.ack_map2, txs->status.s8);
#endif /* DONGLEBUILD */
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_PRHDRS || WLMSG_PRPKT || WLMSG_MAC || ENABLE_CORECAPTURE */
}

static void BCMFASTPATH
wlc_dotrigtxstatus(wlc_info_t *wlc, tx_status_t *txs)
{
	struct scb *scb = NULL;
	struct scb_iter scbiter;
	uint8 nss, mcs;
	ratesel_tgtxs_t tgtxs;

	mcs = TGTXS_MCS(txs->status.s4);
	nss = TGTXS_NSS(txs->status.s4) + 1;
	tgtxs.txrspec = HE_RSPEC(mcs, nss);
	tgtxs.rtidx = TGTXS_RTIDX(txs->status.s8);
	tgtxs.txcnt = TGTXS_TXCNT(txs->status.s8);
	tgtxs.ruidx = TGTXS_RUIDX(txs->status.s4);

	tgtxs.gdelim = TGTXS_LCNT(txs->status.s5);
	tgtxs.gfcs = TGTXS_GDFCSCNT(txs->status.s5);

#ifdef WLC_MACDBG_TRIGBSR
	if (D11TRIGCI_TYPE(TGTXS_CMNINFOP1(txs->status.s2)) == HE_TRIG_TYPE_BSR_FRM) {
		goto done;
	}
#endif /* WLC_MACDBG_TRIGBSR */

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->aid == TGTXS_AID(txs->status.s4)) {
			wlc_ulmu_stats_upd(wlc->ulmu, scb, txs);
#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
			wlc_ulmu_rustats_upd(wlc->ulmu, scb, txs);
#endif // endif
			wlc_scb_ratesel_upd_txs_trig(wlc->wrsi, scb, &tgtxs, txs);
		}
	}
#ifdef WLC_MACDBG_TRIGBSR
done:
#endif /* WLC_MACDBG_TRIGBSR */
	if (WL_PRHDRS_ON() || WL_MAC_ON()) {
		wlc_print_trigtxstatus(wlc, txs, &tgtxs);
	}
	return;
}
#endif /* defined(WL_MU_TX) && defined(WL11AX) */

/** process an individual tx_status_t */
bool BCMFASTPATH
wlc_dotxstatus(wlc_info_t *wlc, tx_status_t *txs, uint32 frm_tx2)
{
	void *p;
	uint queue;
	wlc_txh_info_t txh_info;
	struct scb *scb = NULL, *scb_orig = NULL;
	bool update_rate, free_pdu;
	osl_t *osh;
	int tx_rts, tx_frame_count, tx_rts_count;
	uint totlen, supr_status;
	bool lastframe;
	struct dot11_header *h;
	uint16 fc;
	wlc_pkttag_t *pkttag;
	wlc_bsscfg_t *bsscfg = NULL;
	bool pkt_sent = FALSE;
	bool pkt_max_retries = FALSE;
#if defined(PSPRETEND) || defined(WLPKTDLYSTAT) || defined(WL11K)
	bool ack_recd = FALSE;
#endif // endif
#if defined(WLPKTDLYSTAT) || defined(WL11K)
	uint32 delay, now;
#endif // endif
#ifdef WLPKTDLYSTAT
	uint tr;
	scb_delay_stats_t *delay_stats;
#endif // endif
#ifdef PKTQ_LOG
	uint32 prec_pktlen;
	pktq_counters_t *actq_cnt = 0;
#endif // endif
	uint8 ac;
	bool from_host = TRUE;
#ifdef WL11K
	int prio;
#endif // endif
#if defined(RTS_PER_ITF)
	struct wlc_if *wlcif = NULL;
#endif // endif

#ifdef WL_TX_STALL
	wlc_tx_status_t tx_status = WLC_TX_STS_SUCCESS;
#endif // endif
	uint16 rate_idx = 0;
	uint8 rate_flags;
	ratespec_t pri_rspec = 0; /**< the *primary* rate on which an MPDU/AMPDU was transmitted */
	bool fix_rate;

	bool was_acked = FALSE;
#ifdef PSPRETEND
	bool pps_retry = FALSE;
	uint16	macCtlHigh = 0;
	d11txhdr_t* txh = NULL;
#else
	const bool pps_retry = FALSE;
#endif /* PSPRETEND */
#ifdef WLSCB_HISTO
	bool update_histo = FALSE;
#endif /* WLSCB_HISTO */
	const wlc_rlm_rate_store_t *rstore = NULL;

	BCM_REFERENCE(from_host);
	BCM_REFERENCE(frm_tx2);
	BCM_REFERENCE(rstore);

	if (WL_PRHDRS_ON()) {
		wlc_print_txstatus(wlc, txs);
	}

#if defined(WL_MU_TX) && defined(WL11AX)
	if (HE_ULMU_ENAB(wlc->pub)) {
		if (TGTXS_TGFID(txs->status.s1) == TX_STATUS128_TRIGGERTP) {
			wlc_dotrigtxstatus(wlc, txs);
			return FALSE;
		} else if (D11_TXFID_GET_FIFO(wlc, txs->frameid) == ULMU_TRIG_FIFO) {
			wlc_ulmu_reclaim_utxd(wlc, txs);
			return FALSE;
		}
	}
#endif /* defined(WL_MU_TX) && defined(WL11AX) */

	if (RATELINKMEM_ENAB(wlc->pub)) {
		rate_idx = TX_STATUS128_RATEIDX(txs->status.s8);
		rate_flags = TX_STATUS128_RTFLAGS(txs->status.s8);
		wlc_ratelinkmem_upd_txstatus(wlc, rate_idx, rate_flags);
	}
	/* discard intermediate indications for ucode with legitimate cases:
	 * 1. corerev < 40: e.g. if "useRTS" is set. ucode did a successful rts/cts exchange,
	 * but the subsequent tx of DATA failed. so it will start rts/cts from the beginning
	 * (resetting the rts transmission count)
	 * 2. corerev >= 40:MUTX intermediate txstatus
	 */
	if (txs->status.is_intermediate) {
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			/* Must be MU intermediate txstatus. Return for now */
#if defined(WL_MU_TX)
			wlc_ampdu_aqm_mutx_dotxinterm_status(wlc->ampdu_tx, txs);
#endif /* defined(WL_MU_TX) */
			return FALSE;
		} else if (wlc_txs_reg_mpdu(wlc, txs)) {
			WLCNTADD(wlc->pub->_cnt->txnoack, (txs->status.frag_tx_cnt));
			WL_TRACE(("%s: bail: txs status no ack\n", __FUNCTION__));
			return FALSE;
		}
	}

	osh = wlc->osh;
	queue = D11_TXFID_GET_FIFO(wlc, txs->frameid);

	if (queue >= WLC_HW_NFIFO_TOTAL(wlc) || WLC_HW_DI(wlc, queue) == NULL) {
		p = NULL;
		WL_PRINT(("%s: bail: txs status FID->q %u invalid\n", __FUNCTION__, queue));
		wlc_print_txstatus(wlc, txs);
		goto fatal;
	}

#ifdef BCMDBG_TXSTALL
	wlc->txs_fifo_cnts[queue]++;
#endif /* BCMDBG_TXSTALL */

	supr_status = txs->status.suppr_ind;
	if (supr_status) {
		WL_TX_STS_UPDATE(tx_status,
			wlc_tx_status_map_hw_to_sw_supr_code(wlc, supr_status));
	}

	/* Flushed packets have been re-enqueued, just toss the status */
	if (supr_status == TX_STATUS_SUPR_FLUSH) {
		WL_MQ(("MQ: %s: Flush status for txs->frameid 0x%x\n", __FUNCTION__,
			txs->frameid));
#ifdef WLC_MACDBG_FRAMEID_TRACE
		wlc_macdbg_frameid_trace_txs(wlc->macdbg, NULL, txs);
#endif // endif
		/* we are supposed to get this suppression reason only
		 * when txfifo_detach_pending is set.
		 */
		ASSERT(wlc->txfifo_detach_pending);
		return FALSE;
	}

	p = GETNEXTTXP(wlc, queue);
	WL_TRACE(("%s: pkt fr queue=%d p=%p\n", __FUNCTION__, queue, OSL_OBFUSCATE_BUF(p)));

#if defined(BCMDBG)
	if (p == NULL || wlc->excursion_active || wlc->txfifo_detach_pending) {
		WL_MQ(("MQ: %s: txs->frameid 0x%x\n", __FUNCTION__, txs->frameid));
	}
#endif /* BCMDBG */

	if (p == NULL) {
		void *p1;
		BCM_REFERENCE(p1);
		WL_PRINT(("%s: null ptr2\n", __FUNCTION__));
		wlc_txs_dump_status_info(wlc, txs, queue);
#if defined(ENABLE_CORECAPTURE)
		wlc_print_txstatus(wlc, txs);
		p1 = dma_getnexttxp(WLC_HW_DI(wlc, queue), HNDDMA_RANGE_ALL);
		if (!p1) {
			uint i;
			for (i = 0; i < WLC_HW_NFIFO_TOTAL(wlc); i++) {
				if (!PIO_ENAB(wlc->pub)) {
					if (WLC_HW_DI(wlc, i) == NULL) {
						continue;
					}
				} else {
					ASSERT(i < NFIFO_LEGACY);
					if (WLC_HW_PIO(wlc, i) == NULL) {
						continue;
					}
				}

				if (TXPKTPENDGET(wlc, i) > 0) {
					uint8 * txhdr;
					void *p2;
					p2 = dma_getnexttxp(WLC_HW_DI(wlc, i), HNDDMA_RANGE_ALL);
					if (p2) {
						txhdr = PKTDATA(wlc->osh, p2);
						wlc_print_hdrs(wlc, "DMA txpkt hdr", txhdr,
							txh, NULL,
							PKTLEN(wlc->osh, p2));
						prhex("DMA txpkt body", txhdr,
							PKTLEN(wlc->osh, p2));
					}
				 }
			}
		}
#endif /*  ENABLE_CORECAPTURE  */
#ifdef WLC_MACDBG_FRAMEID_TRACE
		wlc_macdbg_frameid_trace_txs(wlc->macdbg, p, txs);
		wlc_macdbg_frameid_trace_dump(wlc->macdbg, queue);
#endif /* WLC_MACDBG_FRAMEID_TRACE */
		goto fatal;
	}

	ASSERT(p != NULL);

	PKTDBG_TRACE(osh, p, PKTLIST_MI_TFS_RCVD);

	wlc_get_txh_info(wlc, p, &txh_info);

	if ((D11REV_GE(wlc->pub->corerev, 40)) &&
		(ltoh16(txh_info.MacTxControlHigh) & D11AC_TXC_FIX_RATE) &&
		!(txs->status.s5 & TX_STATUS64_MUTX)) {
		/* if using fix rate, retrying 64 mpdus >=4 times can overflow 8-bit cnt.
		 * So ucode treats fix rate specially unless it is MUTX.
		 * Update this before calling AMPDU dotxstatus function.
		 */
		txs->status.frag_tx_cnt = txs->status.s3 & 0xffff;
	}
#ifdef WLC_MACDBG_FRAMEID_TRACE
	wlc_macdbg_frameid_trace_txs(wlc->macdbg, p, txs);
#endif // endif

	if (txs->frameid != htol16(txh_info.TxFrameID)) {
		uint txs_fifo, txh_fifo, txs_seq, txh_seq;

		WL_PRINT(("wl%d: %s: txs->frameid 0x%x txh->TxFrameID 0x%x\n",
			wlc->pub->unit, __FUNCTION__,
			txs->frameid, htol16(txh_info.TxFrameID)));

		/* Show FIFO and SEQ */
		txs_fifo = D11_TXFID_GET_FIFO(wlc, txs->frameid);
		txh_fifo = D11_TXFID_GET_FIFO(wlc, htol16(txh_info.TxFrameID));
		txs_seq = D11_TXFID_GET_SEQ(wlc, txs->frameid);
		txh_seq = D11_TXFID_GET_SEQ(wlc, htol16(txh_info.TxFrameID));
		WL_PRINT(("    FIFO: txs %d txh %d, SEQ: txs %d txh %d delta %d\n",
			txs_fifo, txh_fifo, txs_seq, txh_seq,
			(txs_seq > txh_seq) ? (txs_seq - txh_seq) : (txh_seq - txs_seq)));

#ifdef WLC_MACDBG_FRAMEID_TRACE
		wlc_txs_dump_status_info(wlc, txs, queue);
		printf("bad pkt p:%p flags:0x%x flags3:0x%x seq:0x%x\n",
			OSL_OBFUSCATE_BUF(p), WLPKTTAG(p)->flags,
			WLPKTTAG(p)->flags3, WLPKTTAG(p)->seq);
#if defined(BCMDBG)
		prpkt("bad pkt", wlc->osh, p);
#endif // endif
		wlc_macdbg_frameid_trace_dump(wlc->macdbg, queue);
#endif /* WLC_MACDBG_FRAMEID_TRACE */
		wlc_print_txstatus(wlc, txs);
		ASSERT(txs->frameid == htol16(txh_info.TxFrameID));
		goto fatal;
	}

	fix_rate = (htol16(D11AC_TXC_FIX_RATE) & txh_info.MacTxControlHigh);
	if (RATELINKMEM_ENAB(wlc->pub)) {
		/* Add one more entry of the current rate to keep an accurate histogram. */
		uint16 rmem_idx = ltoh16(txh_info.hdrPtr->rev128.RateMemIdxRateIdx) &
			D11_REV128_RATEIDX_MASK;
		int entry = fix_rate ?
			((ltoh16(txh_info.hdrPtr->rev128.RateMemIdxRateIdx) &
				D11_REV128_RATE_SPECRATEIDX_MASK) >>
				D11_REV128_RATE_SPECRATEIDX_SHIFT) : 0;

		if (D11REV_IS(wlc->pub->corerev, 128)) {
			rate_idx &= D11_RATEIDX_LOW128_MASK;
		}
		if (rate_idx != (rmem_idx &
				(TX_STATUS128_RATEIDX_MASK >> TX_STATUS128_RATEIDX_SHIFT))) {
			WL_PRINT(("wl%d: %s, ratelinkmem rate idx mismatch: "
				"txh idx: %d, txs idx: %d, supr_status: %d\n",
				wlc->pub->unit, __FUNCTION__, rmem_idx, rate_idx, supr_status));
			WL_PRINT(("wl%d: %s: txs->frameid 0x%x txh->TxFrameID 0x%x\n",
				wlc->pub->unit, __FUNCTION__,
				txs->frameid, htol16(txh_info.TxFrameID)));
#ifdef WLC_MACDBG_FRAMEID_TRACE
			wlc_txs_dump_status_info(wlc, txs, queue);
			printf("bad pkt p:%p flags:0x%x flags3:0x%x seq:0x%x\n",
				OSL_OBFUSCATE_BUF(p), WLPKTTAG(p)->flags,
				WLPKTTAG(p)->flags3, WLPKTTAG(p)->seq);
#if defined(BCMDBG)
			prpkt("bad pkt", wlc->osh, p);
#endif // endif
			wlc_macdbg_frameid_trace_dump(wlc->macdbg, queue);
#endif /* WLC_MACDBG_FRAMEID_TRACE */
			wlc_print_txstatus(wlc, txs);
			goto fatal;
		}
		rstore = wlc_ratelinkmem_retrieve_cur_rate_store(wlc, rmem_idx, FALSE);
		pri_rspec = WLC_RATELINKMEM_GET_RSPEC(rstore, entry);
	}

	pkttag = WLPKTTAG(p);

	ac = WME_PRIO2AC(PKTPRIO(p));
#ifdef WL11K
	prio = PKTPRIO(p);
#endif // endif

#ifdef PKTQ_LOG
	if ((WLC_GET_CQ(wlc->active_queue))->pktqlog) {
		actq_cnt =
		(WLC_GET_CQ(wlc->active_queue))->pktqlog->_prec_cnt[WLC_PRIO_TO_PREC(PKTPRIO(p))];
	}
#endif // endif

	/* The h is actually pointed to the 802.11 header enbedded in TX frames,
	 * but only after huge TX header. It is safe from TX status overwrite
	 * by wlc_process_wlhdr_txstatus() call below.
	 */

	h = (struct dot11_header *)(txh_info.d11HdrPtr);
	fc = ltoh16(h->fc);

#ifdef MBSS
	if (MBSS_ENAB(wlc->pub) && (queue == TX_ATIM_FIFO)) {
		/* Under MBSS, all ATIM packets are handled by mbss_dotxstatus */
		wlc_mbss_dotxstatus(wlc, txs, p, fc, pkttag, supr_status);
		WL_TRACE(("%s: bail after mbss dotxstatus\n", __FUNCTION__));
		return FALSE;
	}
#endif /* MBSS */

	scb_orig = WLPKTTAGSCBGET(p);
	wlc_pkttag_scb_restore_ex(wlc, p, &txh_info, &bsscfg, &scb);
	if (!WLPKTFLAG_AMPDU(pkttag)) {
		if (scb_orig != NULL && SCB_INTERNAL(scb_orig)) {
			SCB_PKTS_INFLT_FIFOCNT_DECR(scb_orig, PKTPRIO(p));
		} else if ((scb != NULL) && (scb_orig != NULL) && (scb == scb_orig)) {
			SCB_PKTS_INFLT_FIFOCNT_DECR(scb, PKTPRIO(p));
		}
	}

	if (ETHER_ISMULTI(txh_info.TxFrameRA)) {
		/* XXX The posted packet should not be freed before txstatus is
		 * processed. However bsscfgs and scbs are expected to be freed
		 * before txstatus are processed for DMA posted packets.
		 * Whenever scb and/or bsscfg is needed check null pointers
		 */
#ifdef MBSS
		if (bsscfg != NULL) {
			if (MBSS_ENAB(wlc->pub) && (queue == TX_BCMC_FIFO))
				wlc_mbss_dotxstatus_mcmx(wlc, bsscfg, txs);
		}
#endif /* MBSS */
	} else {
#ifdef PKTQ_LOG
		if (D11REV_GE(wlc->pub->corerev, 40)) {
#if defined(SCB_BS_DATA)
			wlc_bs_data_counters_t *bs_data_counters = NULL;
#endif /* SCB_BS_DATA */
			SCB_BS_DATA_CONDFIND(bs_data_counters, wlc, scb);

			WLCNTCONDADD(actq_cnt, actq_cnt->airtime,
				(uint64)TX_STATUS40_TX_MEDIUM_DELAY(txs));
			SCB_BS_DATA_CONDADD(bs_data_counters, airtime,
				TX_STATUS40_TX_MEDIUM_DELAY(txs));
		}
#endif /* PKTQ_LOG */
	}

#if defined(RTS_PER_ITF)
	if (scb && scb->bsscfg && scb->bsscfg->wlcif) {
		wlcif = scb->bsscfg->wlcif;
	}
#endif // endif

#ifdef WLCNT
	if (N_ENAB(wlc->pub)) {
		if (RATELINKMEM_ENAB(wlc->pub)) {
			if (RSPEC_ISSGI(pri_rspec)) /* TODO: should consider HE GI? */
				WLCNTINCR(wlc->pub->_cnt->txmpdu_sgi);
			if (RSPEC_ISSTBC(pri_rspec))
				WLCNTINCR(wlc->pub->_cnt->txmpdu_stbc);
		} else {
			if (wlc_txh_get_isSGI(wlc, &txh_info))
				WLCNTINCR(wlc->pub->_cnt->txmpdu_sgi);
			if (wlc_txh_get_isSTBC(wlc, &txh_info))
				WLCNTINCR(wlc->pub->_cnt->txmpdu_stbc);
		}
	}
#endif // endif
	was_acked = txs->status.was_acked;

#ifdef PSPRETEND
	if (D11REV_GE(wlc->pub->corerev, 40) && scb && !SCB_INTERNAL(scb) &&
		(!WLPKTFLAG_AMPDU(pkttag) || (pkttag->flags3 & WLF3_AMPDU_REGMPDU))) {
#ifdef BCMDBG
		/* we are draining the fifo yet we received a tx status which isn't suppress -
		 * this is an error we should trap if we are still in the same ps pretend instance
		 */
		if ((BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) &&
			PS_PRETEND_ENABLED(bsscfg)) {
			wlc_pspretend_supr_upd(wlc->pps_info, bsscfg, scb, supr_status);
		}
#endif // endif
		/* pre-logic: we test if tx status has TX_STATUS_SUPR_PPS - if so, clear
		 * the ack status regardless
		 */
		if (PSPRETEND_ENAB(wlc->pub) && supr_status == TX_STATUS_SUPR_PPS) {
			was_acked = FALSE;
		}

		if ((BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) &&
			PS_PRETEND_ENABLED(bsscfg)) {
			/* Mark pps_retry as FALSE on basis of following conditions:
			 * 1: if PROP_TX_STATUS is not enabled OR
			 * 2: if HOST_PROP_TXSTATUS is not enabled OR
			 * 3: if suppresed packet is not from host OR
			 * 4: if suppresed packet is from host AND requested one is from Firmware
			 */
			if (was_acked ||
#ifdef PROP_TXSTATUS
				!PROP_TXSTATUS_ENAB(wlc->pub) ||
				!HOST_PROPTXSTATUS_ACTIVATED(wlc) ||
				!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
					WLFC_PKTFLAG_PKTFROMHOST) ||
				((WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
					WLFC_PKTFLAG_PKTFROMHOST) &&
				(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
					WLFC_PKTFLAG_PKT_REQUESTED)) ||
#endif /* PROP_TXSTATUS */
				FALSE) {
				/* The packet has to actually not have been sent ok (not acked) */
				pps_retry = FALSE;
			} else {
				/* Now test to save the packet with ps pretend (pps_retry flag) */
				pps_retry = wlc_pspretend_pps_retry(wlc, scb, p, txs);
			}

			if (pps_retry) {
				wlc_pkt_get_txh_hdr(wlc, p, &txh);
				macCtlHigh = ltoh16(*D11_TXH_GET_MACHIGH_PTR(wlc, txh));
			}
		}
	}
#endif /* PSPRETEND */

#if defined(WLNAR)
	wlc_nar_dotxstatus(wlc->nar_handle, scb, p, txs, pps_retry);
#endif // endif

#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub) && (txs->status.s5 & TX_STATUS40_IMPBF_MASK)) {
		if (scb) {
			wlc_txbf_imp_txstatus(wlc->txbf, scb, txs);
		}
	}
#endif // endif
#ifdef ENABLE_CORECAPTURE
	if (pkttag->flags & WLF_8021X) {
		wlc_txs_dump_status_info(wlc, txs, queue);
	}
#endif /* ENABLE_CORECAPTURE */

#ifdef BCM_CEVENT
	if (CEVENT_STATE(wlc->pub) && pkttag->flags & WLF_8021X) {
		wlc_send_cevent(wlc, bsscfg, SCB_EA(scb), 0, 0, 0, NULL, 0, CEVENT_D2C_ST_EAP_TX,
				CEVENT_FRAME_DIR_TX | (txs->status.was_acked ?
				CEVENT_D2C_FLAG_SUCCESS : CEVENT_D2C_FLAG_FAIL));
	}
#endif /* BCM_CEVENT */

#ifdef WLAMPDU
	/* Reset txnoprog counter if ucode is giving txstatus */
	wlc_ampdu_reset_txnoprog(wlc->ampdu_tx);

	if (WLPKTFLAG_AMPDU(pkttag)) {
		bool regmpdu;
		bool ret_val;

		if (RATELINKMEM_ENAB(wlc->pub)) {
			/* The rspec is not correct when rstore doesn't exist. */
			if (rstore != NULL && RSPEC_ISLEGACY(pri_rspec)) {
				uint16 ncons = (txs->status.raw_bits & TX_STATUS40_NCONS) >>
					TX_STATUS40_NCONS_SHIFT;
				/* On rate drop, ucode is free to xmit MPDU stand-alone (no agg) */
				if (ncons == 1) {
					pkttag->flags3 |= WLF3_AMPDU_REGMPDU;
				}
			}
		}

		regmpdu = (pkttag->flags3 & WLF3_AMPDU_REGMPDU) != 0;
		if (regmpdu) {
			wlc_ampdu_dotxstatus_regmpdu(wlc->ampdu_tx, scb, p, txs, &txh_info);
			ret_val = BCME_OK;
		} else {
#ifdef STA
			if ((scb != NULL) &&
				wlc_bsscfg_find_by_unique_bssid(wlc, &h->a2)) {
					wlc_adv_pspoll_upd(wlc, scb, p, TRUE, queue);
			}
#endif /* STA */
			ret_val = wlc_ampdu_dotxstatus(wlc->ampdu_tx, scb, p, txs, &txh_info);
			/* Dynamic coremask selection */
			if (D11REV_IS(wlc->pub->corerev, 129)) {
				if ((bsscfg != NULL) && (scb != NULL) &&
					SCB_ASSOCIATED(scb)) {
					if (scb && (wlc->dyntxc->enable == TRUE) &&
					(phy_get_phymode((phy_info_t *)WLC_PI(wlc)) !=
						PHYMODE_BGDFS)) {
						wlc_txs_dyntxc_metric_calc(wlc, txs,
							scb, pri_rspec);
					} else {
						wlc_txs_dyntxc_metric_init(wlc, scb,
							pri_rspec);
					}
				}
			}
		}
#ifdef WL_LPC
		if (LPC_ENAB(wlc) && scb && !RATELINKMEM_ENAB(wlc->pub)) {
			/* Update the LPC database */
			wlc_scb_lpc_update_pwr(wlc->wlpci, scb, ac,
				txh_info.PhyTxControlWord0, txh_info.PhyTxControlWord1);
			/* Reset the LPC vals in ratesel */
			wlc_scb_ratesel_reset_vals(wlc->wrsi, scb, ac);
		}
#endif /* WL_LPC */
		if (regmpdu) {
			/* continue with rest of regular mpdu processing */
		} else {
#ifdef STA
			if (bsscfg != NULL && BSSCFG_STA(bsscfg)) {
				wlc_pm2_ret_upd_last_wake_time(bsscfg, &txs->lasttxtime);
#ifdef WL_LEAKY_AP_STATS
				if (WL_LEAKYAPSTATS_ENAB(wlc->pub)) {
					wlc_leakyapstats_gt_start_time_upd(wlc, bsscfg, txs);
				}
#endif /* WL_LEAKY_AP_STATS */
			}
#endif /* STA */
			if (ret_val) {
				WL_PRINT(("wl%d: %s: %d frameid 0x%x 0x%x\n",
					wlc->pub->unit, __FUNCTION__, __LINE__,
					txs->frameid, htol16(txh_info.TxFrameID)));
				wlc_print_txstatus(wlc, txs);
			}
			return ret_val;
		}
	}

#endif /* WLAMPDU */

#if defined(WL_PROXDETECT) && defined(WL_PROXD_UCODE_TSYNC)
	if (PROXD_ENAB(wlc->pub) && wlc_proxd_frame(wlc, pkttag)) {
		if (PROXD_ENAB_UCODE_TSYNC(wlc->pub)) {
			wlc_proxd_process_tx_rx_status(wlc, txs, NULL, &(h->a1));
		}
	}
#endif // endif

#ifdef PROP_TXSTATUS
	from_host = WL_TXSTATUS_GET_FLAGS(pkttag->wl_hdr_information) & WLFC_PKTFLAG_PKTFROMHOST;
#endif /* PROP_TXSTATUS */

	tx_rts = wlc_txh_has_rts(wlc, &txh_info);
	tx_frame_count = txs->status.frag_tx_cnt;
	tx_rts_count = txs->status.rts_tx_cnt;

	/* count all retransmits */
	if (tx_frame_count) {
		WLCNTADD(wlc->pub->_cnt->txretrans, (tx_frame_count - 1));
		WLCNTSCB_COND_ADD(scb && from_host, scb->scb_stats.tx_pkts_retries,
			(tx_frame_count - 1));
		WLCNTSCB_COND_ADD(scb && !from_host, scb->scb_stats.tx_pkts_fw_retries,
			(tx_frame_count - 1));
	}
	if (tx_rts_count)
		WLCNTADD(wlc->pub->_cnt->txretrans, (tx_rts_count - 1));

#ifdef PKTQ_LOG
	WLCNTCONDADD(actq_cnt && tx_rts_count, actq_cnt->rtsfail, tx_rts_count - 1);
#endif // endif

	lastframe = (fc & FC_MOREFRAG) == 0;

	totlen = pkttotlen(osh, p);
	BCM_REFERENCE(totlen);

	/* plan for success */
	update_rate = (pkttag->flags & WLF_RATE_AUTO) ? TRUE : FALSE;
	free_pdu = TRUE;

	/* In case of RATELINKMEM, update rspec history with used rate now */
	if (RATELINKMEM_ENAB(wlc->pub)) {
		if (bsscfg && !ETHER_ISMULTI(txh_info.TxFrameRA)) {
			/* Add one more entry of the current rate to keep an accurate histogram. */
			bsscfg->txrspec[bsscfg->txrspecidx][0] = pri_rspec;
			bsscfg->txrspec[bsscfg->txrspecidx][1] = 1; /* nfrags */
			bsscfg->txrspecidx = (bsscfg->txrspecidx+1) % NTXRATE;
		}
		/* Update scb stats */
		if (((FC_TYPE(fc) == FC_TYPE_DATA) &&
			((FC_SUBTYPE(fc) != FC_SUBTYPE_NULL) &&
			(FC_SUBTYPE(fc) != FC_SUBTYPE_QOS_NULL)))) {
			WLCNTSCB_COND_SET(scb, scb->scb_stats.tx_rate, pri_rspec);
		}
	}

#ifdef PSPRETEND
	/* Whether we could be doing ps pretend in the case the
	 * tx status is NACK. PS pretend is not operating on multicast scb.
	 * "Conventional" PS mode (SCB_PS_PRETEND_NORMALPS) is excluded to keep the two
	 * mechanisms separated.
	 * Normal PS pretend is supported only if AQM is supported, but THRESHOLD PS pretend
	 * can be used, so this is checked.
	 * Finally, the flag for WLF_PSDONTQ is checked because this is used for probe
	 * packets. We should not operate ps pretend in this case.
	 */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		if (!was_acked && (scb != NULL) && !SCB_INTERNAL(scb) &&
			SCB_PS_PRETEND_THRESHOLD(scb) && !SCB_PS(scb) &&
			!(pkttag->flags & WLF_PSDONTQ)) {
			wlc_pspretend_on(wlc->pps_info, scb, 0);
			supr_status |= TX_STATUS_SUPR_PMQ;
		}
	}
#endif /* PSPRETEND */

#ifdef WL_EVENT_LOG_COMPILE
	if (pkttag->flags & WLF_8021X) {
		wl_event_log_tlv_hdr_t tlv_log = {{0, 0}};
		wl_event_log_eapol_tx_t eapol_sts = {{0, 0, 0}};
		uint32 tlv_log_tmp = 0;
		uint16 tx_rate = 0;

		eapol_sts.status = txs->status.was_acked ? 1 : 0;
		eapol_sts.frag_tx_cnt = txs->status.frag_tx_cnt;

		tlv_log.tag = TRACE_TAG_VENDOR_SPECIFIC;
		tlv_log.length = sizeof(wl_event_log_eapol_tx_t);
		tlv_log_tmp = tlv_log.t;

		tlv_log.tag = TRACE_TAG_RATE_MBPS;
		tlv_log.length = sizeof(uint16);

		if (D11REV_GE(wlc->pub->corerev, 128)) {
			tx_rate = wf_rspec_to_rate(pri_rspec)/500;
		} else if (D11REV_GE(wlc->pub->corerev, 65)) {
			tx_rate = txh_info.hdrPtr->rev40.rev64.RateInfo[0].TxRate;
		} else {
			tx_rate = txh_info.hdrPtr->rev40.rev40.RateInfo[0].TxRate;
		}
		WL_EVENT_LOG((EVENT_LOG_TAG_TRACE_WL_INFO, TRACE_FW_EAPOL_FRAME_TRANSMIT_STOP,
			tlv_log_tmp, eapol_sts.t,
			tlv_log.t, tx_rate));
	}
#endif /* WL_EVENT_LOG_COMPILE */

#ifdef PKTQ_LOG
	if (!was_acked && supr_status) {
		WLCNTCONDINCR(actq_cnt, actq_cnt->suppress);
	}
#endif /* PKTQ_LOG */

	/* process txstatus info */
	if (was_acked) {
		/* success... */
#if defined(PSPRETEND) || defined(WLPKTDLYSTAT) || defined(WL11K)
		ack_recd = TRUE;
#endif // endif

		/* update counters */
		WLCNTINCR(wlc->pub->_cnt->txfrag);
#ifdef WL11K
		wlc_rrm_stat_qos_counter(wlc, scb, prio, OFFSETOF(rrm_stat_group_qos_t, txfrag));
#endif // endif
		WLCNTADD(wlc->pub->_cnt->txnoack, (tx_frame_count - 1));
		WLCIFCNTINCR(scb, txfrag);
#ifdef PKTQ_LOG
		WLCNTCONDINCR(actq_cnt, actq_cnt->acked);
		WLCNTCONDADD(actq_cnt, actq_cnt->retry, (tx_frame_count - 1));
		prec_pktlen = pkttotlen(wlc->osh, p);
		WLCNTCONDADD(actq_cnt, actq_cnt->throughput, (uint64)prec_pktlen);
#endif // endif
		if (tx_frame_count > 1) {
			/* counting number of retries for all data frames. */
			wlc->txretried += tx_frame_count - 1;
			/* counting the number of frames where a retry was necessary
			 * per each station.
			 */
			WLCNTSCB_COND_INCR(scb, scb->scb_stats.tx_pkts_retried);
		}
		if (lastframe) {
			WLCNTINCR(wlc->pub->_cnt->txfrmsnt);
			WLCIFCNTINCR(scb, txfrmsnt);
#ifdef WL11K
			wlc_rrm_tscm_upd(wlc, scb, prio, OFFSETOF(rrm_tscm_t, msdu_tx), 1);
#endif // endif
			if (wlc->txretried >= 1) {
				WLCNTINCR(wlc->pub->_cnt->txretry);
				WLCIFCNTINCR(scb, txretry);
#ifdef WL11K
				wlc_rrm_stat_qos_counter(wlc, scb, prio,
					OFFSETOF(rrm_stat_group_qos_t, txretry));
				wlc_rrm_tscm_upd(wlc, scb, prio,
					OFFSETOF(rrm_tscm_t, msdu_retries), 1);
#endif // endif
			}
			if (wlc->txretried > 1) {
				WLCNTINCR(wlc->pub->_cnt->txretrie);
				WLCIFCNTINCR(scb, txretrie);
#ifdef WL11K
				wlc_rrm_stat_qos_counter(wlc, scb, prio,
					OFFSETOF(rrm_stat_group_qos_t, txretries));
#endif // endif
			}

			WLCNTSCB_COND_INCR(scb && from_host, scb->scb_stats.tx_pkts_total);
			WLCNTSCB_COND_INCR(scb && !from_host, scb->scb_stats.tx_pkts_fw_total);
#if defined(STA) && defined(WL_TX_STALL) && defined(WL_TX_CONT_FAILURE)
			if (wlc->tx_stall->error->txfail_consecutive_total &&
				bsscfg && BSSCFG_INFRA_STA(bsscfg)) {
				wlc_txfail_cnt_reset(wlc);
			}
#endif /* STA && WL_TX_STALL && WL_TX_CONT_FAILURE */
#ifdef WLSCB_HISTO
		update_histo = TRUE;
#endif /* WLSCB_HISTO */
		}
		if (scb) {
			/* If P2P/GO send PROBE_RESP frame to existing P2P/GC,
			 * scb->used should not be updated.
			 * Perhaps, it will be already disconnected situation.
			 */
			if (!(FC_TYPE(fc) == FC_TYPE_MNG &&
				(FC_SUBTYPE(fc) == FC_SUBTYPE_PROBE_RESP))) {
				scb->used = wlc->pub->now;
			}
#if defined(STA) && defined(DBG_BCN_LOSS)
			scb->dbg_bcn.last_tx = wlc->pub->now;
#endif // endif
		}
		if (bsscfg && BSSCFG_STA(bsscfg) && bsscfg->BSS && bsscfg->roam) {
			/* clear the time_since_bcn  */
			if (bsscfg->roam->time_since_bcn != 0) {
#ifdef EVENT_LOG_COMPILE
				EVENT_LOG(EVENT_LOG_TAG_BEACON_LOG, "Beacon miss reset by TX");
#endif // endif
#ifdef STA
				wlc_roam_update_time_since_bcn(bsscfg, 0);
#endif // endif
			}
		}

		pkt_sent = TRUE;

#ifdef PSPRETEND
	} else if (pps_retry) {
		ASSERT(!was_acked);
		update_rate = FALSE;

#ifdef PKTQ_LOG
		if (!supr_status) {
			WLCNTCONDINCR(actq_cnt, actq_cnt->ps_retry);
		}
#endif // endif

		/* re-enqueue the packet suppressed due to ps pretend */
		free_pdu = wlc_pspretend_pkt_retry(wlc->pps_info, bsscfg, scb, p,
				txs, txh, macCtlHigh);

		WL_PS(("wl%d.%d: %s: ps pretend %s suppress for PM frame 0x%x for "MACF" %s\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			SCB_PS_PRETEND(scb) ? "ON":"OFF", txs->frameid,
			ETHERP_TO_MACF(txh_info.TxFrameRA), free_pdu?"tossed":"requeued"));

		/* If the packet is not queued for later delivery, it will be freed
		 * and any packet callback needs to consider this as a un-ACKed packet.
		 * Change the txstatus to simple no-ack instead of suppressed.
		 */
		if (free_pdu) {
			wlc_txs_clear_supr(wlc, &(txs->status));
		}

#endif /* PSPRETEND */
	} else if (!was_acked && !pps_retry &&
		(WLPKTFLAG_AMSDU(pkttag) || WLPKTFLAG_RIFS(pkttag))) {
		update_rate = FALSE;

		/* update counters */
		WLCNTINCR(wlc->pub->_cnt->txfrag);
		WLCIFCNTINCR(scb, txfrag);
#ifdef WL11K
		wlc_rrm_stat_qos_counter(wlc, scb, prio, OFFSETOF(rrm_stat_group_qos_t, txfrag));
#endif // endif
		if (lastframe) {
			WLCNTINCR(wlc->pub->_cnt->txfrmsnt);
			WLCIFCNTINCR(scb, txfrmsnt);
#ifdef WL11K
			wlc_rrm_tscm_upd(wlc, scb, prio, OFFSETOF(rrm_tscm_t, msdu_tx), 1);
#endif // endif
			WLCNTSCB_COND_INCR(scb && from_host, scb->scb_stats.tx_pkts_total);
			WLCNTSCB_COND_INCR(scb && !from_host, scb->scb_stats.tx_pkts_fw_total);
#ifdef WLSCB_HISTO
			update_histo = TRUE;
#endif /* WLSCB_HISTO */
		}

		/* should not update scb->used for ap otherwise the ap probe will not work */
		if (scb && bsscfg && BSSCFG_STA(bsscfg)) {
			scb->used = wlc->pub->now;
#if defined(STA) && defined(DBG_BCN_LOSS)
			scb->dbg_bcn.last_tx = wlc->pub->now;
#endif // endif
		}

		pkt_sent = TRUE;

	} else if (supr_status == TX_STATUS_SUPR_FRAG) {
		/* subsequent frag failure... */
		update_rate = FALSE;
	} else if (supr_status == TX_STATUS_SUPR_BADCH) {
		update_rate = FALSE;
		WLCNTINCR(wlc->pub->_cnt->txchanrej);
	} else if (supr_status == TX_STATUS_SUPR_EXPTIME) {
		WLCNTINCR(wlc->pub->_wme_cnt->tx_expired[(WME_ENAB(wlc->pub) ?
			WME_PRIO2AC(PKTPRIO(p)) : AC_BE)].packets);
		WLCNTINCR(wlc->pub->_cnt->txexptime);	/* generic lifetime expiration */
#ifdef WL11K
		wlc_rrm_tscm_upd(wlc, scb, prio, OFFSETOF(rrm_tscm_t, msdu_exp), 1);
#endif // endif
#ifdef RTS_PER_ITF
		if (wlcif) {
			WLCNTINCR(wlcif->_cnt.txexptime);
		}
#endif // endif
		/* Interference detected */
		if (wlc->rfaware_lifetime) {
			wlc_exptime_start(wlc);
		}
	} else if (supr_status == TX_STATUS_SUPR_PMQ) {
		update_rate = FALSE;

		if ((bsscfg != NULL && BSSCFG_AP(bsscfg)) || BSS_TDLS_ENAB(wlc, bsscfg)) {
#if defined(BCMDBG) || defined(WLMSG_PS)
			char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG_ERR */

			if (scb != NULL && (SCB_ISMULTI(scb) || SCB_ASSOCIATED(scb) ||
				BSS_TDLS_ENAB(wlc, bsscfg)) &&
#ifdef PROP_TXSTATUS
				(!PROP_TXSTATUS_ENAB(wlc->pub)||!from_host) &&
#endif // endif
				TRUE) {
				free_pdu = wlc_apps_suppr_frame_enq(wlc, p, txs, lastframe);
			}

			WL_PS(("wl%d: %s: suppress for PM frame 0x%x for %s %s\n",
				wlc->pub->unit, __FUNCTION__, txs->frameid,
				bcm_ether_ntoa((struct ether_addr *)txh_info.TxFrameRA, eabuf),
				free_pdu?"tossed":"requeued"));
		}

		/* If the packet is not queued for later delivery, it will be freed
		 * and any packet callback needs to consider this as a un-ACKed packet.
		 * Change the txstatus to simple no-ack instead of suppressed.
		 */
		if (free_pdu)
			wlc_txs_clear_supr(wlc, &(txs->status));

	} else if (supr_status == TX_STATUS_SUPR_UF) {
		update_rate = FALSE;
	} else if (supr_status == TX_STATUS_SUPR_AGG0) {
		update_rate = FALSE;
	} else if (supr_status == TX_STATUS_SUPR_TWT) {
		ASSERT(ltoh16(*D11_TXH_GET_MACHIGH_PTR(wlc, txh_info.hdrPtr)) & D11REV128_TXC_TWT);
		update_rate = FALSE;
		if ((bsscfg != NULL) && BSSCFG_AP(bsscfg) && scb && !SCB_ISMULTI(scb) &&
			SCB_ASSOCIATED(scb)) {

			free_pdu = wlc_apps_suppr_twt_frame_enq(wlc, p);
		}
		if (free_pdu)
			wlc_txs_clear_supr(wlc, &(txs->status));
	} else if ((supr_status == TX_STATUS_SUPR_LMEM_INVLD) && RATELINKMEM_ENAB(wlc->pub)) {
		update_rate = FALSE;
			WL_ERROR(("wl%d: %s: bail: tx status for invalid linkmem scb:%p\n"
				"txs frmid:0x%04x txh frmid:0x%04x lmem_idx:%d",
				wlc->pub->unit, __FUNCTION__, scb,
				txs->frameid, htol16(txh_info.TxFrameID), rate_idx));
	} else if (P2P_ABS_SUPR(wlc, supr_status)) {
		update_rate = FALSE;
		if (bsscfg != NULL && BSS_TX_SUPR_ENAB(bsscfg)) {
#ifdef BCMDBG
			if (WL_P2P_ON()) {
#if defined(WLP2P_UCODE)
				int bss = wlc_mcnx_BSS_idx(wlc->mcnx, bsscfg);
				WL_P2P(("wl%d: suppress packet %p for absence, seqnum %d, "
					"TSF 0x%x, state 0x%x\n",
					wlc->pub->unit, OSL_OBFUSCATE_BUF(p),
					ltoh16(h->seq) >> SEQNUM_SHIFT,
					R_REG(wlc->osh, D11_TSFTimerLow(wlc)),
					wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_ST(wlc, bss))));
#endif /* WLP2P_UCODE */
				wlc_print_txdesc(wlc, PKTDATA(osh, p));
				wlc_print_txstatus(wlc, txs);
			}
#endif /* BCMDBG */

			/* This is possible if we got a packet suppression
			 * before getting ABS interrupt
			 */
			if (!BSS_TX_SUPR(bsscfg) &&
#ifdef WLP2P
				(wlc_p2p_noa_valid(wlc->p2p, bsscfg) ||
				wlc_p2p_ops_valid(wlc->p2p, bsscfg)) &&
#endif // endif
				TRUE)
				wlc_bsscfg_tx_stop(wlc->psqi, bsscfg);

			/* requeue packet */
			if (scb != NULL) {
				bool requeue  = TRUE;

				if (BSSCFG_AP(bsscfg) &&
					SCB_ASSOCIATED(scb) && SCB_P2P(scb))
					wlc_apps_scb_tx_block(wlc, scb, 1, TRUE);
#if defined(PROP_TXSTATUS)
				if (PROP_TXSTATUS_ENAB(wlc->pub)) {
					requeue = wlc_should_retry_suppressed_pkt(wlc,
						p, supr_status);
					requeue |= SCB_ISMULTI(scb);
				}
#endif // endif
				if (requeue) {
					if (BSS_TX_SUPR(bsscfg))
						free_pdu = wlc_pkt_abs_supr_enq(wlc, scb, p);
					else
						free_pdu = TRUE;
				}
			}
		}

		/* If the packet is not queued for later delivery, it will be freed
		 * and any packet callback needs to consider this as a un-ACKed packet.
		 * Change the txstatus to simple no-ack instead of suppressed.
		 */
		if (free_pdu)
			wlc_txs_clear_supr(wlc, &(txs->status));
	} else if (txs->phyerr) {
		WL_ERROR(("wl%d: %s, phyerr: %d ;", wlc->pub->unit, __FUNCTION__, txs->phyerr));
		if (WL_ERROR_ON())
			wlc_log_unexpected_tx_frame_log_80211hdr(wlc, h);
		update_rate = FALSE;
		WLCNTINCR(wlc->pub->_cnt->txphyerr);
		WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_PHY_ERROR);
		WL_ERROR(("wl%d: %s: tx phy error (0x%x)\n", wlc->pub->unit,
			__FUNCTION__, txs->phyerr));
#ifdef BCMDBG
		if (WL_ERROR_ON()) {
			prpkt("txpkt (MPDU)", osh, p);
			wlc_print_txdesc(wlc, PKTDATA(osh, p));
			wlc_print_txstatus(wlc, txs);
		}
#endif /* BCMDBG */
	} else if (ETHER_ISMULTI(txh_info.TxFrameRA)) {
		/* mcast success */
		/* update counters */
		if (tx_frame_count) {
			WLCNTINCR(wlc->pub->_cnt->txfrag);
			WLCIFCNTINCR(scb, txfrag);
#ifdef WL11K
			wlc_rrm_stat_qos_counter(wlc, scb, prio,
				OFFSETOF(rrm_stat_group_qos_t, txfrag));
#endif // endif
		}

		if (lastframe) {
			WLCNTINCR(wlc->pub->_cnt->txfrmsnt);
			WLCIFCNTINCR(scb, txfrmsnt);
#ifdef WL11K
			wlc_rrm_tscm_upd(wlc, scb, prio, OFFSETOF(rrm_tscm_t, msdu_tx), 1);
#endif // endif
			if (tx_frame_count) {
				WLCNTINCR(wlc->pub->_cnt->txmulti);
				WLCIFCNTINCR(scb, txmulti);
			}
		}
		pkt_sent = TRUE;
	}
#ifdef STA
	else if (bsscfg != NULL && BSSCFG_STA(bsscfg) && bsscfg->BSS && ETHER_ISMULTI(&h->a3)) {
		if (tx_frame_count) {
			WLCNTINCR(wlc->pub->_cnt->txfrag);
#ifdef WL11K
			wlc_rrm_stat_qos_counter(wlc, scb, prio,
				OFFSETOF(rrm_stat_group_qos_t, txfrag));
#endif // endif
		}
		if (lastframe) {
			WLCNTINCR(wlc->pub->_cnt->txfrmsnt);
			WLCIFCNTINCR(scb, txfrmsnt);
#ifdef WL11K
			wlc_rrm_tscm_upd(wlc, scb, prio, OFFSETOF(rrm_tscm_t, msdu_tx), 1);
#endif // endif
			if (tx_frame_count) {
				WLCNTINCR(wlc->pub->_cnt->txmulti);
				WLCIFCNTINCR(scb, txmulti);
				WLCNTSCB_COND_INCR(scb, scb->scb_stats.tx_mcast_pkts);
#ifdef WL11K
				wlc_rrm_stat_bw_counter(wlc, scb, TRUE);
#endif // endif
			}
		}

		pkt_sent = TRUE;
	}
#endif /* STA */
	else if ((!tx_rts && tx_frame_count >= wlc->SRL) ||
		(tx_rts && (tx_rts_count >= wlc->SRL || tx_frame_count >= wlc->LRL))) {
		WLCNTADD(wlc->pub->_cnt->txnoack, tx_frame_count);
		WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_RETRY_TIMEOUT);
#ifdef PKTQ_LOG
		WLCNTCONDADD(actq_cnt, actq_cnt->retry, tx_frame_count);
#endif // endif

		/* ucast failure */
		if (lastframe) {
#ifdef WL_TRAFFIC_THRESH
			if (bsscfg != NULL && scb && SCB_ASSOCIATED(scb)) {
				if (scb->rssi > bsscfg->far_sta_rssi) {
					if (wlc->pub->_traffic_thresh &&
							bsscfg->traffic_thresh_enab) {
						if (BSSCFG_AP(bsscfg)) {
							wlc_traffic_thresh_incr(wlc, bsscfg,
									&txh_info, ac);
							if (bsscfg->trf_cfg_enable &
									(1 << WL_TRF_TO)) {
								wlc_traffic_thresh_incr(wlc, bsscfg,
									&txh_info, WL_TRF_TO);
							}
						}
					}
					if (wlc->pub->_traffic_thresh &&
							bsscfg->traffic_thresh_enab) {
						wlc_trf_mgmt_scb_txfail_detect(scb, bsscfg,
							&txh_info, ac);
						if (scb->trf_enable_flag &
								(1 << WL_TRF_TO)) {
							wlc_trf_mgmt_scb_txfail_detect(scb,
								bsscfg,
								&txh_info, WL_TRF_TO);
						}
					}
				}
			}
#endif /* WL_TRAFFIC_THRESH */
			WLCNTINCR(wlc->pub->_cnt->txfail);
#ifdef WLC_SW_DIVERSITY
			if (WLSWDIV_ENAB(wlc) && WLSWDIV_BSSCFG_SUPPORT(bsscfg)) {
				wlc_swdiv_txfail(wlc->swdiv, scb);
			}
#endif /* WLC_SW_DIVERSITY */
			WLCNTSCB_COND_INCR(scb && from_host,
				scb->scb_stats.tx_pkts_retry_exhausted);
			WLCNTSCB_COND_INCR(scb && !from_host,
				scb->scb_stats.tx_pkts_fw_retry_exhausted);
			WLCNTSCB_COND_INCR(scb, scb->scb_stats.tx_failures);
#ifdef WL11K
			wlc_rrm_stat_qos_counter(wlc, scb, prio,
				OFFSETOF(rrm_stat_group_qos_t, txfail));
			wlc_rrm_tscm_upd(wlc, scb, prio, OFFSETOF(rrm_tscm_t, msdu_fail), 1);
#endif // endif
			WLCIFCNTINCR(scb, txfail);
#ifdef PKTQ_LOG
			WLCNTCONDINCR(actq_cnt, actq_cnt->retry_drop);
#endif // endif
			if (from_host) {
				if (WL_INFORM_ON() | WL_MAC_ON()) {
					WL_PRINT(("wl%d: %s, ucastfail ;", wlc->pub->unit,
						__FUNCTION__));
					wlc_log_unexpected_tx_frame_log_80211hdr(wlc, h);
				}
			}
			wlc_bss_mac_event(wlc, bsscfg, WLC_E_TXFAIL,
				(struct ether_addr *)txh_info.TxFrameRA,
				WLC_E_STATUS_TIMEOUT, 0, 0, 0, 0);
#if defined(STA) && defined(WL_TX_STALL) && defined(WL_TX_CONT_FAILURE)
			if (wlc->tx_stall->failure_to_rpt &&
				bsscfg && BSSCFG_INFRA_STA(bsscfg)) {
				wlc_txfail_cnt_inc(wlc, WLPKTFLAG_AMPDU(pkttag), pkttag->rspec);
			}
#endif /* STA && WL_TX_STALL && WL_TX_CONT_FAILURE */
		}
#ifdef BCMDBG
		if ((!tx_rts && tx_frame_count != wlc->SRL) ||
			(tx_rts && tx_rts_count != wlc->SRL &&
			tx_frame_count != wlc->LRL)) {
			/* catch ucode retrying too much */
			WL_MAC(("wl%d: %s: status indicates too many retries before "
				"failure\n", wlc->pub->unit, __FUNCTION__));
			WL_MAC(("tx_rts = %d tx_rts_count = %d tx_frame_count = %d ->"
				" srl=%d lrl=%d\n",
				tx_rts, tx_rts_count, tx_frame_count, wlc->SRL, wlc->LRL));
		}
#endif /* BCMDBG */
		pkt_max_retries = TRUE;
	} else {
		/* unexpected tx status */
		WL_MAC(("wl%d: %s, unexpected txs ;", wlc->pub->unit, __FUNCTION__));
		if (WL_MAC_ON())
			wlc_log_unexpected_tx_frame_log_80211hdr(wlc, h);
		update_rate = FALSE;
		WLCNTINCR(wlc->pub->_cnt->txserr);
		WL_TX_STS_UPDATE(tx_status, WLC_TX_STS_TOSS_UNKNOWN);
		WL_TRACE(("wl%d: %s: unexpected tx status returned \n",
			wlc->pub->unit, __FUNCTION__));
		WL_MAC(("tx_rts = %d tx_rts_count = %d tx_frame_count = %d -> srl=%d lrl=%d\n",
			tx_rts, tx_rts_count, tx_frame_count, wlc->SRL, wlc->LRL));
	}
#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		wlc_wlfc_dotxstatus(wlc, bsscfg, scb, p, txs, pps_retry);
	}
#endif /* PROP_TXSTATUS */

#ifdef WL_TX_STALL
	/* Update tx_status statistics */
	if (tx_status) {
		wlc_tx_status_update_counters(wlc, p, scb, bsscfg, tx_status, 1);
		if (free_pdu == FALSE) {
			/* Packet is requeued, update the queued count */
			wlc_tx_status_update_counters(wlc, p, scb, bsscfg,
				WLC_TX_STS_QUEUED, 1);
		}
	}
#endif // endif

	/* Check if interference still there and print external log if needed */
	if (wlc->rfaware_lifetime && wlc->exptime_cnt &&
		supr_status != TX_STATUS_SUPR_EXPTIME)
		wlc_exptime_check_end(wlc);

	/* update rate state and scb used time */
#ifdef WLSCB_HISTO
	if (update_histo && scb) {
#ifdef WL_AMPDU
		if (!WLPKTFLAG_AMPDU(pkttag) || (pkttag->flags3 & WLF3_AMPDU_REGMPDU))
#endif /* WL_AMPDU */
		{
			WLSCB_HISTO_TX(scb, pri_rspec, 1);
		}
	}
#endif /* WLSCB_HISTO */

	wlc_macdbg_dtrace_log_txs(wlc->macdbg, scb, &pri_rspec, txs);

	if (update_rate && scb) {
/* XXX 4360:WES Update this section for new TxH format. Maybe use phytype check to
 * determine what TxH format is in use, and use and determine the arguments for
 * wlc_ratesel_upd_txstatus_normalack().
 * We will not need to update this section when using fixed rate only for VHT
 */
		uint16 sfbl, lfbl, longframe_bm;
		uint8 rspec_rate = MCS_INVALID, antselid = 0;
		bool is_sgi, fbr = FALSE;

		if (RATELINKMEM_ENAB(wlc->pub))
			is_sgi = RSPEC_ISSGI(pri_rspec); /* TODO: HE GI??? */
		else
			is_sgi = wlc_txh_get_isSGI(wlc, &txh_info);

		/* rspec_rate has to be in this non-HT format: bits [0..3] MCS, bits [4..7] NSS */
		if (RATELINKMEM_ENAB(wlc->pub)) {
			if (RSPEC_ISHE(pri_rspec) || RSPEC_ISVHT(pri_rspec)) {
				/* HE/VHT rate is already in non-HT format */
				rspec_rate = pri_rspec & WL_RSPEC_RATE_MASK;
			} else if (RSPEC_ISHT(pri_rspec)) {
				rspec_rate = pri_rspec & WL_RSPEC_HT_MCS_MASK;
				/* Convert MCS in ratespec to non-HT format if needed */
				rspec_rate = wlc_rate_ht_to_nonht(rspec_rate);
			}
		} else {
			uint16 ft_fmt, ft;

			ft_fmt = ltoh16(txh_info.PhyTxControlWord0) &
				D11_PHY_TXC_FTFMT_MASK(wlc->pub->corerev);
			ft = D11_FT(ft_fmt);

			switch (ft) {
			case FT_HE:
				rspec_rate = wf_he_plcp_to_rspec_rate(txh_info.plcpPtr, ft_fmt);
				break;

			case FT_VHT:
				rspec_rate = wf_vht_plcp_to_rspec_rate(txh_info.plcpPtr);
				break;

			case FT_HT:
				rspec_rate = wlc_rate_ht_to_nonht(txh_info.plcpPtr[0] &
					MIMO_PLCP_MCS_MASK);
				break;

			default:
				break;
			}
		}

		if (WLANTSEL_ENAB(wlc))
			antselid = wlc_antsel_antsel2id(wlc->asi,
				ltoh16(txh_info.w.ABI_MimoAntSel));

		ASSERT(!TX_STATUS_UNEXP(txs->status));

		if (queue != TX_BCMC_FIFO) {
			sfbl = WLC_WME_RETRY_SFB_GET(wlc, ac);
			lfbl = WLC_WME_RETRY_LFB_GET(wlc, ac);
		} else {
			sfbl = wlc->SFBL;
			lfbl = wlc->LFBL;
		}

		/* TXC_AMPDU_FBR only exists for non AQM chips. (corerev < 40) */
		fbr = (!AMPDU_AQM_ENAB(wlc->pub) &&
			(ltoh16(txh_info.MacTxControlHigh) & TXC_AMPDU_FBR)) ? TRUE : FALSE;

		/* If not long frame, ucode uses short fallback limit only.
		 * Get correct long frame bit mask per corerev.
		 */
		longframe_bm = AMPDU_AQM_ENAB(wlc->pub) ? D11AC_TXC_LFRM : TXC_LONGFRAME;
		if (!(longframe_bm & ltoh16(txh_info.MacTxControlLow))) {
			lfbl = sfbl;
		}

		wlc_scb_ratesel_upd_txstatus_normalack(wlc->wrsi, scb, txs, sfbl, lfbl,
			rspec_rate, is_sgi, antselid, fbr, ac);
#ifdef WL_LPC
		if (LPC_ENAB(wlc) && scb && !RATELINKMEM_ENAB(wlc->pub)) {
			/* Update the LPC database */
			wlc_scb_lpc_update_pwr(wlc->wlpci, scb, ac,
				txh_info.PhyTxControlWord0, txh_info.PhyTxControlWord1);
			/* Reset the LPC vals in ratesel */
			wlc_scb_ratesel_reset_vals(wlc->wrsi, scb, ac);
		}
#endif /* WL_LPC */
	}

	WL_TRACE(("%s: calling txfifo_complete\n", __FUNCTION__));

	wlc_txfifo_complete(wlc, scb, queue, 1);

#ifdef PSPRETEND
	/* Provide tx status input to the PS Pretend state machine.
	 * The function may cause a tx fifo flush.
	 * As a result, this call needs to be done after
	 * the call to wlc_txfifo_complete() above.
	 */
	if ((bsscfg != NULL) && (BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg)) &&
		PS_PRETEND_ENABLED(bsscfg) &&
		/* if scb is multicast or in normal PS mode, return as we aren't handling
		* ps pretend for these
		*/
		(scb != NULL) && !SCB_INTERNAL(scb) && !SCB_PS_PRETEND_NORMALPS(scb) &&
		!(pkttag->flags & WLF_PSDONTQ)) {
		wlc_pspretend_dotxstatus(wlc->pps_info, bsscfg, scb, ack_recd);
	}
#endif /* PSPRETEND */

#if defined(WLPKTDLYSTAT) || defined(WL11K)
	/* calculate latency and packet loss statistics */
	if (scb && free_pdu) {
		/* Get the current time */
		now = WLC_GET_CURR_TIME(wlc);
		/* Ignore wrap-around case */
		if (now > WLPKTTAG(p)->shared.enqtime) {
			delay = now - WLPKTTAG(p)->shared.enqtime;
#ifdef WLPKTDLYSTAT
			tr = tx_frame_count - 1;

			if (tx_frame_count == 0 || tx_frame_count > RETRY_SHORT_DEF)
				tr = RETRY_SHORT_DEF-1;

			delay_stats = scb->delay_stats + ac;
			wlc_delay_stats_upd(delay_stats, delay, tr, ack_recd);
#endif /* WLPKTDLYSTAT */
#ifdef WL11K
			if (ack_recd) {
				wlc_rrm_delay_upd(wlc, scb, prio, delay);
			}
#endif // endif
		}
	}
#endif /* WLPKTDLYSTAT || WL11K */

#ifdef STA
	/* PM state change */
	/* - in mcast case we always have a valid bsscfg (unless the bsscfg is deleted).
	 * - in ucast case we may not have a valid bsscfg and scb could have been deleted before
	 *   the transmission finishes, in which case lookup the bsscfg from the BSSID
	 * - Infrastructure STAs should always find the bsscfg by RA BSSID match, unless the
	 *   bsscfg has been deleted (in which case we do not care about PM state update for it)
	 * - in case we located neither scb nor bsscfg we assume the PM states have
	 *   been taken care of when deleting the scb or the bsscfg, yes we hope;-)
	 */
	if (bsscfg != NULL) {
#ifdef WL_LEAKY_AP_STATS
		if (WL_LEAKYAPSTATS_ENAB(wlc->pub)) {
			wlc_leakyapstats_gt_start_time_upd(wlc, bsscfg, txs);
		}
#endif /* WL_LEAKY_AP_STATS */
		wlc_update_pmstate(bsscfg, (uint)(wlc_txs_alias_to_old_fmt(wlc, &(txs->status))));

		if ((((fc & FC_KIND_MASK) == FC_DATA) ||
			((fc & FC_KIND_MASK) == FC_QOS_DATA)) && lastframe) {
			wlc_pm2_ret_upd_last_wake_time(bsscfg, &txs->lasttxtime);
		}
	}
#endif /* STA */

	/* update the bytes in/out counts */
	if (PIO_ENAB(wlc->pub)) {
		ASSERT(queue < NFIFO_LEGACY);
		wlc_pio_cntupd(WLC_HW_PIO(wlc, queue), totlen);
	}

#ifdef STA
	/* Roaming decision based upon too many packets at the min tx rate */
	if (scb != NULL && !SCB_INTERNAL(scb) &&
		bsscfg != NULL && BSSCFG_STA(bsscfg) &&
		bsscfg->BSS && bsscfg->associated &&
#ifdef WLP2P
		!BSS_P2P_ENAB(wlc, bsscfg) &&
#endif // endif
		bsscfg->roam && !bsscfg->roam->off)
		wlc_txrate_roam(wlc, scb, txs, pkt_sent, pkt_max_retries, ac);

	if (scb != NULL && lastframe) {
		 if (wlc_bsscfg_find_by_unique_bssid(wlc, &h->a2)) {
			wlc_adv_pspoll_upd(wlc, scb, p, TRUE, queue);
		 }
	}
#endif /* STA */

	/* call any matching pkt callbacks */
	if (free_pdu) {
		wlc_pcb_fn_invoke(wlc->pcb, p,
		                  (uint)(wlc_txs_alias_to_old_fmt(wlc, &(txs->status))));
	}

#ifdef WLP2P
	if (WL_P2P_ON()) {
		if (txs->status.was_acked) {
			WL_P2P(("wl%d: sent packet %p successfully, seqnum %d\n",
				wlc->pub->unit, OSL_OBFUSCATE_BUF(p),
				ltoh16(h->seq) >> SEQNUM_SHIFT));
		}
	}
#endif // endif

	PKTDBG_TRACE(osh, p, PKTLIST_TXDONE);

	/* free the mpdu */
	if (free_pdu) {
		PKTFREE(osh, p, TRUE);
	}

	if (lastframe)
		wlc->txretried = 0;

	return FALSE;

fatal:
	if (p != NULL) {
		uint16 pktcnt = 0;
		PKTDBG_TRACE(osh, p, PKTLIST_TXFAIL);
		wlc_txq_free_pkt(wlc, p, &pktcnt);
		wlc_txfifo_complete(wlc, scb, queue, pktcnt);
	}

	BCM_REFERENCE(pkt_sent);
	BCM_REFERENCE(pkt_max_retries);
	return TRUE;
} /* wlc_dotxstatus */

bool wlc_should_retry_suppressed_pkt(wlc_info_t *wlc, void *p, uint status)
{
	/* TX_STATUS_SUPR_PPS is purposely not tested here */
	if ((status == TX_STATUS_SUPR_PMQ) || (P2P_ABS_SUPR(wlc, status))) {
		/* Retry/Requeue pkt in any of the following cases:
		 * 1. NIC / PropTx Not enabled
		 * 2. Dongle / PropTx Disabled.
		 * 3. Dongle / PropTx Enabled, but Host PropTx inactive
		 * 4. Dongle / PropTx Enabled and Host PropTx active, but dongle(!host) packet
		 */
		if ((!PROP_TXSTATUS_ENAB(wlc->pub)) ||
#ifdef PROP_TXSTATUS
			(!HOST_PROPTXSTATUS_ACTIVATED(wlc)) ||
			(PROP_TXSTATUS_ENAB(wlc->pub) && HOST_PROPTXSTATUS_ACTIVATED(wlc) &&
			(!(WL_TXSTATUS_GET_FLAGS(WLPKTTAG(p)->wl_hdr_information) &
			WLFC_PKTFLAG_PKTFROMHOST))) ||
#endif /* PROP_TXSTATUS */
			FALSE) {
#ifdef PROP_TXSTATUS_DEBUG
			wlc_txh_info_t txh_info;
			struct dot11_header *h;
			uint16 fc;
			wlc_get_txh_info(wlc, p, &txh_info);
			h = (struct dot11_header *)(txh_info.d11HdrPtr);
			fc = ltoh16(h->fc);
			char eabuf[ETHER_ADDR_STR_LEN];
			bcm_ether_ntoa((struct ether_addr *)(txh_info.TxFrameRA), eabuf);
			printf("retry ptk:0x%x FC%x t/st:%x/%x for [%s]\n",
				(WLPKTTAG(p)->wl_hdr_information), fc,
				FC_TYPE(fc), FC_SUBTYPE(fc), eabuf);
#endif /* PROP_TXSTATUS_DEBUG */
			return TRUE;
		}
	}
	return FALSE;
}

static void BCMFASTPATH
wlc_pkttag_scb_restore_ex(wlc_info_t* wlc, void* p, wlc_txh_info_t* txh_info,
	wlc_bsscfg_t **bsscfg_out, struct scb **scb_out)
{
	wlc_bsscfg_t *bsscfg = NULL;
	struct scb *scb = NULL;
	int err;

	BCM_REFERENCE(err);

	/*	use the bsscfg index in the packet tag to find out which
	 *	bsscfg this packet belongs to
	 */
	bsscfg = wlc_bsscfg_find(wlc, WLPKTTAGBSSCFGGET(p), &err);

	if (ETHER_ISMULTI(txh_info->TxFrameRA)) {
		if (bsscfg != NULL) {
			/* XXX bcmc_scb in the bsscfg for AP should be always valid
			 * if the bsscfg is configured for an regular AP in a BSS, otherwise
			 * it should not come to here, in other words, the bcmc frame should
			 * have been dropped early in the send path if the bsscfg is configured
			 * to not have any bcmc traffic.
			 */
			/* With pkt suppression, need to assign SCB for any frame */
			scb = WLC_BCMCSCB_GET(wlc, bsscfg);

			/* BCMC scb is NULL in case of P2P discovery.
			 * For P2P discovery make use of hw rateset scb.
			 */
			if (scb == NULL)
				scb = wlc->band->hwrs_scb;

			ASSERT(scb != NULL);
		}
	} else {
		/* For ucast frames, lookup the scb directly by the RA.
		 * The scb may not be found if we sent a frame with no scb, or if the
		 * scb was deleted while the frame was queued.
		 */
		if (bsscfg != NULL) {
			scb = wlc_scbfindband(wlc, bsscfg,
				(struct ether_addr*)txh_info->TxFrameRA,
				wlc_txhinfo2bandunit(wlc, txh_info));
		}

		if (scb != NULL) {
			bsscfg = SCB_BSSCFG(scb);
			ASSERT(bsscfg != NULL);
		}
#ifdef STA
		else {
			/* no scb, but we may be able to look up a bsscfg by the BSSID if this
			 * was a frame sent by an infra STA to its AP
			 */
			bsscfg = wlc_bsscfg_find_by_bssid(wlc,
				(struct ether_addr *)txh_info->TxFrameRA);
		}
#endif /* STA */
	}

	if (bsscfg != NULL) {
		WLPKTTAGBSSCFGSET(p, WLC_BSSCFG_IDX(bsscfg));
	}
	/* else when NULL, then no-op for bsscfg */

	/* update to usable value (NULL or valid scb ptr) */
	WLPKTTAGSCBSET(p, scb);

	if (bsscfg_out) {
		*bsscfg_out = bsscfg;
	}
	if (scb_out) {
		*scb_out = scb;
	}
} /* wlc_pkttag_scb_restore_ex */

void
wlc_pkttag_scb_restore(void *ctxt, void* p)
{
	wlc_txh_info_t txh_info;
	wlc_info_t* wlc = (wlc_info_t*)ctxt;
	struct scb *scb, *scb_orig;

	scb_orig = WLPKTTAGSCBGET(p);
	if (WLPKTTAG(p)->flags & WLF_TXHDR) {
		wlc_get_txh_info(wlc, p, &txh_info);
		wlc_pkttag_scb_restore_ex(wlc, p, &txh_info, NULL, NULL);
#ifdef WLTAF
		/* may be TAF tagged? */
		WL_TAFF(wlc, "\n");
		wlc_taf_txpkt_status(wlc->taf_handle, WLPKTTAGSCBGET(p), PKTPRIO(p), p,
			TAF_TXPKT_STATUS_PKTFREE_DROP);
#endif // endif
	}

	scb = WLPKTTAGSCBGET(p);
	if (scb_orig != NULL && SCB_INTERNAL(scb_orig)) {
		SCB_PKTS_INFLT_FIFOCNT_DECR(scb_orig, PKTPRIO(p));
	} else if ((scb != NULL) && (scb_orig != NULL) && (scb == scb_orig)) {
		SCB_PKTS_INFLT_FIFOCNT_DECR(scb, PKTPRIO(p));
	}
}
/* attach txs_dyntxc */
wlc_txs_dyntxc_info_t *
BCMATTACHFN(wlc_txs_dyntxc_attach)(wlc_info_t *wlc)
{
	wlc_txs_dyntxc_info_t *dyntxc;
	if ((dyntxc = MALLOCZ(wlc->osh, sizeof(wlc_txs_dyntxc_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	dyntxc->wlc = wlc;
	return dyntxc;

	/* error handling */
fail:
	MODULE_DETACH(dyntxc, wlc_txs_dyntxc_detach);
	return NULL;
}
/* detach txs_dyntxc */
void
BCMATTACHFN(wlc_txs_dyntxc_detach)(wlc_txs_dyntxc_info_t *dyntxc)
{
	wlc_info_t *wlc;

	if (dyntxc == NULL)
		return;

	wlc = dyntxc->wlc;

	(void)wlc_module_remove_ioctl_fn(wlc->pub, dyntxc);
	wlc_module_unregister(wlc->pub, "dyntxc", dyntxc);

	MFREE(wlc->osh, dyntxc, sizeof(wlc_txs_dyntxc_info_t));
}
/* txs_dyntxc param setup */
void
wlc_txs_dyntxc_set(wlc_txs_dyntxc_info_t *dyntxc)
{
	dyntxc->candidate_num = 0;
	if (dyntxc->coefset == 0) {
		dyntxc->coeff_set[0] = TXS_DYNTXC_COEF_ALLCORES;
		dyntxc->coeff_set[1] = TXS_DYNTXC_COEF_OMIT_CORE3;
		dyntxc->coeff_set[2] = TXS_DYNTXC_COEF_OMIT_CORE2;
		dyntxc->coeff_set[3] = TXS_DYNTXC_COEF_OMIT_CORE1;
		dyntxc->coeff_set[4] = TXS_DYNTXC_COEF_OMIT_CORE0;
		dyntxc->candidate_num = 5;
		dyntxc->coefset = 15;
	} else {
		dyntxc->coeff_set[dyntxc->candidate_num] =
			TXS_DYNTXC_COEF_ALLCORES;
		dyntxc->candidate_num ++;
		if ((dyntxc->coefset & 0x8) == 8) {
			dyntxc->coeff_set[dyntxc->candidate_num] =
				TXS_DYNTXC_COEF_OMIT_CORE3;
			dyntxc->candidate_num ++;
		}
		if ((dyntxc->coefset & 0x4) == 4) {
			dyntxc->coeff_set[dyntxc->candidate_num] =
				TXS_DYNTXC_COEF_OMIT_CORE2;
			dyntxc->candidate_num ++;
		}
		if ((dyntxc->coefset & 0x2) == 2) {
			dyntxc->coeff_set[dyntxc->candidate_num] =
				TXS_DYNTXC_COEF_OMIT_CORE1;
			dyntxc->candidate_num ++;
		}
		if ((dyntxc->coefset & 0x1) == 1) {
			dyntxc->coeff_set[dyntxc->candidate_num] =
				TXS_DYNTXC_COEF_OMIT_CORE0;
			dyntxc->candidate_num ++;
		}
	}
	if (dyntxc->subperiod_num == 0) {
		dyntxc->subperiod_num = 5;
	}
	if (dyntxc->feature == 0) {
		dyntxc->feature = 1;
	}
	//metric cal occupied 10% of overall cycle
	dyntxc->period_num = dyntxc->subperiod_num*dyntxc->candidate_num*10;
#ifndef BCMDBG
	// only support sounding stream eqaul to 3 under formal release
	dyntxc->feature = dyntxc->feature & 0x1;
#endif // endif
}
/* Return txs_dyntxc status for metric calc and txc
 * inputs are scb and ratespec
 * output is the valid signal to adopt txs_dyntxc
 */
bool
wlc_txs_dyntxc_status(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec, bool apply_txc)
{
	uint8 nss_sp = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
	uint8 bfen = (rspec & WL_RSPEC_TXBF) >> 21;
	uint8 bfe_sts_cap = wlc_txbf_get_bfe_sts_cap(wlc->txbf, scb);
	if (wlc->dyntxc->enable == FALSE) {
		return FALSE;
	}
	if (!(RSPEC_ISVHT(rspec) || RSPEC_ISHE(rspec) || RSPEC_ISHT(rspec))) {
		/* bypass for non-VHT, non-HE frame, non-HT */
		return FALSE;
	}
	if (apply_txc == TRUE) {
		if (bfen != 0) {
			return FALSE;
		}
		if ((nss_sp == 3) && (bfe_sts_cap == 2) &&
			((wlc->dyntxc->feature >> DYNTXC_CMASK_STS2) & 0x1) == 1) {
			/* 3 stream TXBF cap */
			return TRUE;
		} else if ((nss_sp == 3) && (bfe_sts_cap == 3) &&
			((wlc->dyntxc->feature >> DYNTXC_CMASK_STS3) & 0x1) == 1) {
			/* 4 stream TXBF cap */
			return TRUE;
		} else if (((wlc->dyntxc->feature >> DYNTXC_CMASK_HT) & 0x1) == 1) {
			/* HT rate */
			if ((RSPEC_HT_MCS(rspec) > 15 && RSPEC_HT_MCS(rspec) < 24) ||
				RSPEC_HT_MCS(rspec) == 101 ||
				RSPEC_HT_MCS(rspec) == 102) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	} else {
		if ((nss_sp == 3 || nss_sp == 2) && (bfe_sts_cap == 2) &&
			((wlc->dyntxc->feature >> DYNTXC_CMASK_STS2) & 0x1) == 1) {
			/* 3 stream TXBF cap */
			return TRUE;
		} else if ((nss_sp == 3 || nss_sp == 2) && (bfe_sts_cap == 3) &&
			((wlc->dyntxc->feature >> DYNTXC_CMASK_STS3) & 0x1) == 1) {
			/* 4 stream TXBF cap */
			return TRUE;
		} else if (((wlc->dyntxc->feature >> DYNTXC_CMASK_HT) & 0x1) == 1) {
			/* HT rate */
			if ((RSPEC_HT_MCS(rspec) > 7 && RSPEC_HT_MCS(rspec) < 24) ||
				RSPEC_HT_MCS(rspec) >= 99) {
				return TRUE;
			} else {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	}

}
/* update coremask */
void
wlc_txs_dyntxc_update(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec,
	d11ratemem_rev128_rate_t *rate, txpwr204080_t txpwrs_sdm)
{
	uint i;
	uint8 curbw = ((rspec & WL_RSPEC_BW_MASK) >> WL_RSPEC_BW_SHIFT) - 1;
	uint8 curpwr;
	if (wlc_txs_dyntxc_status(wlc, scb, rspec, TRUE) == FALSE) {
		return;
	}
	if (scb->dyn_coremask != 0) {
		/* overwrite coremask */
		rate->TxPhyCtl[2] = scb->dyn_coremask;
	}
	curpwr = txpwrs_sdm.pbw[curbw][0];
	curpwr = SCALE_5_1_TO_5_2_FORMAT(curpwr);
	rate->TxPhyCtl[3] = rate->TxPhyCtl[4] =
		htol16((curpwr << PHYCTL_TXPWR_CORE1_SHIFT) | curpwr);
	if (scb->dyn_coremask == TXS_DYNTXC_COEF_OMIT_CORE0 ||
		scb->dyn_coremask == TXS_DYNTXC_COEF_OMIT_CORE1 ||
		scb->dyn_coremask == TXS_DYNTXC_COEF_OMIT_CORE2 ||
		scb->dyn_coremask == TXS_DYNTXC_COEF_OMIT_CORE3) {
		/* Configure txpwr_bw and FbwPwr */
		for (i = 0; i < D11_REV128_RATEENTRY_NUM_BWS; i++) {
			rate->txpwr_bw[i] = SCALE_5_1_TO_5_2_FORMAT(txpwrs_sdm.pbw[i][0]);
			if (i < D11_REV128_RATEENTRY_FBWPWR_WORDS) {
				rate->FbwPwr[i] = htol16(
					(SCALE_5_1_TO_5_2_FORMAT(txpwrs_sdm.pbw[i][1]) << 8) |
					SCALE_5_1_TO_5_2_FORMAT(txpwrs_sdm.pbw[i][0]));
			}
		}
	}
}
/* update txs_dyntxc counter to trigger metric calc */
void
wlc_txs_dyntxc_cnt_update(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec)
{
	if (wlc_txs_dyntxc_status(wlc, scb, rspec, FALSE) == FALSE) {
		return;
	}
	/* update statistic cnt */
	scb->scb_stats.dyn_coremask_count ++;
}
/* Find out the best coremask from txs
 * input are txs, scb, ratespec
 * output is the suggested coremask
 */
void
wlc_txs_dyntxc_metric_calc(wlc_info_t *wlc, tx_status_t *txs, scb_t *scb, ratespec_t rspec)
{
	uint16 tx_cnt_sp, txsucc_cnt_sp;
	uint16 current_coremask;
	uint8 mcs, candidate_cnt;
	uint16 avg_thpt_set[5] = {0};
	uint16 sub_period_bound[5] = {0};
	uint16 start_bound;
	uint16 max_avg_thpt = 0;
	uint16 max_avg_thpt2 = 0;
	uint8 thpt[12] = {2, 4, 6, 8, 12, 16, 18, 20, 24, 27, 31, 33};
	uint8 nss_sp;
	if (wlc_txs_dyntxc_status(wlc, scb, rspec, FALSE) == FALSE) {
		return;
	}

	// only check the rate0 txs, ignore other fallback rates
	tx_cnt_sp     = TX_STATUS40_TXCNT_RT0(txs->status.s3);
	txsucc_cnt_sp = TX_STATUS40_ACKCNT_RT0(txs->status.s3);
	if ((txsucc_cnt_sp > tx_cnt_sp) || (tx_cnt_sp == 0)) {
		return;
	}

	current_coremask = wlc_stf_txcore_get(wlc, rspec);
	/*
	* Current schem is limited to improved 3SS_CDD1.
	* Bypass the function for 3 txchain setting/devices
	*/
	if (WLC_BITSCNT(wlc->stf->txchain) <= 3) {
		scb->dyn_coremask = current_coremask;
		return;
	} else {
		/* first set = default result */
		wlc->dyntxc->coeff_set[0] =
			current_coremask;
	}
	if (!RSPEC_ISHT(rspec)) {
		mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
		nss_sp = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
	} else {
		mcs = RSPEC_HT_MCS(rspec);
		if (mcs == 101) {
			mcs = 8;
			nss_sp = 3;
		} else if (mcs == 102) {
			mcs = 9;
			nss_sp = 3;
		} else if (mcs == 99) {
			mcs = 8;
			nss_sp = 2;
		} else if (mcs == 100) {
			mcs = 9;
			nss_sp = 2;
		} else if (mcs > 15 && mcs < 24) {
			mcs = mcs - 16;
			nss_sp = 3;
		} else if (mcs > 7 && mcs < 16) {
			mcs = mcs - 8;
			nss_sp = 2;
		} else {
			mcs = mcs;
			nss_sp = 1;
		}
	}
	for (candidate_cnt = 0; candidate_cnt < wlc->dyntxc->candidate_num;
		candidate_cnt++) {
		sub_period_bound[candidate_cnt] = (candidate_cnt+1) *
			wlc->dyntxc->subperiod_num;
	}

	if (scb->scb_stats.dyn_coremask_count > wlc->dyntxc->period_num) {
		wlc_txs_dyntxc_metric_init(wlc, scb, rspec);
		max_avg_thpt = 0;
		max_avg_thpt2 = 0;
		scb->scb_stats.dyn_coremask_bestidx = 0;
	} else {
	// serach algorithm start
	// for each candidates, collect total R0 packet and their phy rate
		if (scb->scb_stats.dyn_coremask_count == 0) {
		// first time just update the coef, not doing characterization
			scb->scb_stats.dyn_coremask_bestidx = 0;
		} else if (scb->scb_stats.dyn_coremask_count <
			sub_period_bound[wlc->dyntxc->candidate_num-1]+1) {
			for (candidate_cnt = 0; candidate_cnt < wlc->dyntxc->candidate_num;
				candidate_cnt++) {
				if (candidate_cnt == 0) {
					start_bound = 0;
				} else {
					start_bound = sub_period_bound[candidate_cnt-1];
				}
				if ((scb->scb_stats.dyn_coremask_count > start_bound) &&
					(scb->scb_stats.dyn_coremask_count <=
					sub_period_bound[candidate_cnt])) {
					scb->scb_stats.dyn_coremask_thpt[candidate_cnt] +=
						thpt[mcs] * nss_sp * txsucc_cnt_sp * 100/tx_cnt_sp;
					scb->scb_stats.dyn_coremask_pkt[candidate_cnt]++;
					if ((scb->scb_stats.dyn_coremask_count ==
						sub_period_bound[candidate_cnt]) &&
						(candidate_cnt < (wlc->dyntxc->candidate_num-1))) {
						scb->scb_stats.dyn_coremask_bestidx =
							candidate_cnt + 1;
					}
				}
			}
		} else if ((scb->scb_stats.dyn_coremask_count) ==
			(sub_period_bound[wlc->dyntxc->candidate_num-1]+1)) {
		// find out the best avg phyrate and apply it till next search
			for (candidate_cnt = 0; candidate_cnt < wlc->dyntxc->candidate_num;
				candidate_cnt++) {
				if (scb->scb_stats.dyn_coremask_pkt[candidate_cnt] != 0) {
					avg_thpt_set[candidate_cnt] =
					scb->scb_stats.dyn_coremask_thpt[candidate_cnt] /
					scb->scb_stats.dyn_coremask_pkt[candidate_cnt];
				} else {
					avg_thpt_set[candidate_cnt] = 0;
					scb->scb_stats.dyn_coremask_count =
						wlc->dyntxc->period_num + 1;
				}
				WL_NONE(("set=%d, coef=%d Avg thpt=%d Tot thpt=%d pkt=%d\n",
					candidate_cnt, wlc->dyntxc->coeff_set[candidate_cnt],
					avg_thpt_set[candidate_cnt],
					scb->scb_stats.dyn_coremask_thpt[candidate_cnt],
					scb->scb_stats.dyn_coremask_pkt[candidate_cnt]));
				if (avg_thpt_set[candidate_cnt] >= max_avg_thpt) {
					max_avg_thpt = avg_thpt_set[candidate_cnt];
					scb->scb_stats.dyn_coremask_bestidx = candidate_cnt;
				}
			}
			for (candidate_cnt = 0; candidate_cnt < wlc->dyntxc->candidate_num;
				candidate_cnt++) {
				if (avg_thpt_set[candidate_cnt] >= max_avg_thpt2 &&
					candidate_cnt  != scb->scb_stats.dyn_coremask_bestidx) {
					max_avg_thpt2 = avg_thpt_set[candidate_cnt];
				}
			}
			WL_NONE(("max_avg_thpt=%d, max_avg_thpt2=%d \n", max_avg_thpt,
				max_avg_thpt2));
			/* when avg thpt is lower than mcs6
			 *trust pktnum instead of avg thpt
			 */
			if (max_avg_thpt == max_avg_thpt2) {
				scb->scb_stats.dyn_coremask_count = wlc->dyntxc->period_num + 1;
				scb->scb_stats.dyn_coremask_bestidx = 0;
			}
			WL_NONE(("result=%d \n",
				wlc->dyntxc->coeff_set[scb->scb_stats.dyn_coremask_bestidx]));
		}
	}
	scb->dyn_coremask = wlc->dyntxc->coeff_set[scb->scb_stats.dyn_coremask_bestidx];
}
/* init/reset the metic calculation */
void
wlc_txs_dyntxc_metric_init(wlc_info_t *wlc, scb_t *scb, ratespec_t rspec)
{
	uint8 candidate_cnt;
	uint16 current_coremask = wlc_stf_txcore_get(wlc, rspec);
	scb->dyn_coremask = current_coremask;
	for (candidate_cnt = 0; candidate_cnt < wlc->dyntxc->candidate_num;
		candidate_cnt++) {
		scb->scb_stats.dyn_coremask_thpt[candidate_cnt] = 0;
		scb->scb_stats.dyn_coremask_pkt[candidate_cnt] = 0;
	}
	scb->scb_stats.dyn_coremask_count = 0;
	scb->scb_stats.dyn_coremask_bestidx = 0;
}
