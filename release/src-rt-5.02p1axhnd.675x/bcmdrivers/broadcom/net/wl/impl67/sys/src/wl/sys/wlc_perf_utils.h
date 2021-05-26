/*
 * performance measurement/analysis facilities
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
 * $Id: wlc_perf_utils.h 782741 2020-01-03 14:35:11Z $
 */

#ifndef _wlc_perf_utils_h_
#define _wlc_perf_utils_h_

#include <typedefs.h>
#include <wlc_types.h>
#include <bcmutils.h>
#include <wlioctl.h>

#if defined(BCMPCIEDEV) && defined(BUS_TPUT)
typedef struct bmac_hostbus_tput_info {
	struct wl_timer	*dma_timer; /* timer for bus throughput measurement routine */
	struct wl_timer	*end_timer; /* timer for stopping bus throughput measurement */
	bool		test_running; /* bus throughput measurement is running or not */
	uint32		pktcnt_tot; /* total no of dma completed */
	uint32		pktcnt_cur; /* no of descriptors reclaimed after previous commit */
	uint32		max_dma_descriptors;	/* no of host descriptors programmed by */
						    /* the firmware before a dma commit */
	/* block of host memory for measuring bus throughput */
	dmaaddr_t	host_buf_addr;
	uint16		host_buf_len;
} bmac_hostbus_tput_info_t;

extern void wlc_bmac_tx_fifos_flush(wlc_hw_info_t *wlc_hw, uint fifo_bitmap);
extern void wlc_bmac_suspend_mac_and_wait(wlc_hw_info_t *wlc_hw);
#endif /* BCMPCIEDEV && BUS_TPUT */

#if defined(WLPKTDLYSTAT) || defined(WL11K) || defined(WL_PS_STATS)
/* Macro used to get the current local time (in us) */
#define WLC_GET_CURR_TIME(wlc)		(uint32)OSL_SYSUPTIME_US()

#ifdef WLPKTDLYSTAT
extern void wlc_scb_dlystat_dump(scb_t *scb, struct bcmstrbuf *b);
extern void wlc_dlystats_clear(wlc_info_t *wlc);
extern void wlc_delay_stats_upd(scb_delay_stats_t *delay_stats, uint32 delay, uint tr,
	bool ack_recd);
#endif /* WLPKTDLYSTAT */
#endif // endif

#if defined(BCMDBG)
/* Mask for individual stats */
#define WLC_PERF_STATS_ISR		0x01
#define WLC_PERF_STATS_DPC		0x02
#define WLC_PERF_STATS_TMR_DPC		0x04
#define WLC_PERF_STATS_PRB_REQ		0x08
#define WLC_PERF_STATS_PRB_RESP		0x10
#define WLC_PERF_STATS_BCN_ISR		0x20
#define WLC_PERF_STATS_BCNS		0x40

/* Performance statistics interfaces */
void wlc_update_perf_stats(wlc_info_t *wlc, uint32 mask);
void wlc_update_isr_stats(wlc_info_t *wlc, uint32 macintstatus);
void wlc_update_p2p_stats(wlc_info_t *wlc, uint32 macintstatus);
#endif /* BCMDBG */

#ifdef PKTQ_LOG
void wlc_pktq_stats_free(wlc_info_t* wlc, struct pktq* q);
pktq_counters_t *wlc_txq_prec_log_enable(wlc_info_t* wlc, struct pktq* q, uint32 i, bool en_pair);

/* this is the default scale factor */
#define PKTQ_LOG_RATE_SCALE_FACTOR    PERF_LOG_RATE_FACTOR_100

#endif /* PKTQ_LOG */

wlc_perf_utils_t *wlc_perf_utils_attach(wlc_info_t *wlc);
void wlc_perf_utils_detach(wlc_perf_utils_t *pui);

#endif /* _wlc_perf_utils_h_ */
