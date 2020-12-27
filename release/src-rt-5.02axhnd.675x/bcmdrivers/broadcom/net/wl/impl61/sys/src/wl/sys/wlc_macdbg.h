/*
 * MAC debug and print functions
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_macdbg.h 775546 2019-06-04 02:40:32Z $
 */
#ifndef WLC_MACDBG_H_
#define WLC_MACDBG_H_

#include <typedefs.h>
#include <wlc_types.h>
#include <wlioctl.h>

/* fatal reason code */
#define PSM_FATAL_ANY		0
#define PSM_FATAL_PSMWD		1
#define PSM_FATAL_SUSP		2
#define PSM_FATAL_WAKE		3
#define PSM_FATAL_TXSUFL	4
#define PSM_FATAL_PSMXWD	5
#define PSM_FATAL_TXSTUCK	6
#define PSM_FATAL_LAST		7

#define PSMX_FATAL_ANY		0
#define PSMX_FATAL_PSMWD	1
#define PSMX_FATAL_SUSP		2
#define PSMX_FATAL_TXSTUCK	3
#define PSMX_FATAL_LAST		4

#define PSMR1_FATAL_ANY		0
#define PSMR1_FATAL_PSMWD	1
#define PSMR1_FATAL_SUSP	2
#define PSMR1_FATAL_TXSTUCK	3
#define PSMR1_FATAL_LAST	4

#ifdef WL_UTRACE
#define UTRACE_SPTR_MASK 0xFFFE
#define T_UTRACE_BLK_STRT D11AC_WOWL_PSP_TPL_BASE
#if D11CONF_IS(80)
/* The start and length of the utrace region in the template. */
/* For 4369 use the UTRACE region */
#define T_UTRACE_BLK_STRT_MAIN 0x3800
#define T_UTRACE_BLK_STRT_AUX 0x2C00
#endif /* WL_UTRACE */
#define T_UTRACE_TPL_RAM_SIZE_BYTES (1024 * 4)
#define T_UTRACE_TPL_RAM_SIZE_BYTES_SHARED_MAC (512 * 3)

void wlc_utrace_capture_get(wlc_info_t *wlc, void *data, int length);
void wlc_utrace_init(wlc_info_t *wlc);
#endif /* WL_UTRACE */

/* attach/detach */
extern wlc_macdbg_info_t *wlc_macdbg_attach(wlc_info_t *wlc);
extern void wlc_macdbg_detach(wlc_macdbg_info_t *macdbg);

extern void wlc_dump_ucode_fatal(wlc_info_t *wlc, uint reason);
extern void wlc_psm_watchdog_reason(wlc_info_t *wlc);

/* catch any interrupts from psmx */
#ifdef WL_PSMX
void wlc_bmac_psmx_errors(wlc_info_t *wlc);
void wlc_dump_psmx_fatal(wlc_info_t *wlc, uint reason);
#ifdef WLVASIP
void wlc_dump_vasip_fatal(wlc_info_t *wlc);
#endif	/* WLVASIP */
#else
#define wlc_bmac_psmx_errors(wlc) do {} while (0)
#endif /* WL_PSMX */

#ifdef WL_PSMR1
void wlc_dump_psmr1_fatal(wlc_info_t *wlc, uint reason);
#endif /* WL_PSMR1 */

extern void wlc_dump_mac_fatal(wlc_info_t *wlc, uint reason);

#ifdef DONGLEBUILD
extern void wlc_handle_fatal_error_dump(wlc_info_t *wlc);
#endif /* DONGLEBUILD */

#if defined(BCMDBG) || defined(DUMP_D11CNTS)
extern void wlc_macdbg_upd_d11cnts(wlc_info_t *wlc);
#else
#define wlc_macdbg_upd_d11cnts(wlc) do {} while (0)
#endif // endif

extern void wlc_dump_phytxerr(wlc_info_t *wlc, uint16 PhyErr);

/* enable frameid trace facility for internal/macdbg build */
#ifdef WL_MACDBG
#define WLC_MACDBG_FRAMEID_TRACE
#endif // endif

#ifdef WLC_MACDBG_FRAMEID_TRACE
void wlc_macdbg_frameid_trace_pkt(wlc_macdbg_info_t *macdbg, void *pkt,
	uint8 fifo, uint16 txFrameID, uint16 MacTxControlLow, uint8 epoch, void *scb);
void wlc_macdbg_frameid_trace_txs(wlc_macdbg_info_t *macdbg, void *pkt, tx_status_t *txs);
void wlc_macdbg_frameid_trace_sync(wlc_macdbg_info_t *macdbg, void *pkt);
void wlc_macdbg_frameid_trace_dump(wlc_macdbg_info_t *macdbg, uint fifo);
#endif // endif

#ifdef AWD_EXT_TRAP
extern uchar *wlc_macdbg_get_ext_trap_data(wlc_macdbg_info_t *macdbg, uint32 *len);
extern void wlc_macdbg_store_ext_trap_data(wlc_macdbg_info_t *macdbg, uint32 trap_reason);
#endif /* AWD_EXT_TRAP */

#ifdef BCMDBG
extern void wlc_macdbg_dtrace_log_txs(wlc_macdbg_info_t *macdbg, struct scb *scb,
	ratespec_t *txrspec, tx_status_t *txs);
extern void wlc_macdbg_dtrace_log_txd(wlc_macdbg_info_t *macdbg, struct scb *scb,
	ratespec_t *txrspec, d11txh_rev128_t *txh);
extern void wlc_macdbg_dtrace_log_txr(wlc_macdbg_info_t *macdbg, struct scb *scb,
	uint16 link_idx, d11ratemem_rev128_entry_t  *rate);
extern void wlc_macdbg_dtrace_log_str(wlc_macdbg_info_t *macdbg, struct scb *scb,
	const char *format, ...);
#else
#define wlc_macdbg_dtrace_log_txs(a, b, c, d) do {} while (0)
#define wlc_macdbg_dtrace_log_txd(a, b, c, d) do {} while (0)
#define wlc_macdbg_dtrace_log_txr(a, b, c, d) do {} while (0)
#define wlc_macdbg_dtrace_log_str(a, b, c, ...) do {} while (0)
#endif /* BCMDBG */

#ifdef NOT_YET /* manually compile in when necessary */
#define WL_DPRINT(x)	wlc_macdbg_dtrace_log_str x
#else
#define WL_DPRINT(x) do {} while (0)
#endif // endif

#endif /* WLC_MACDBG_H_ */
