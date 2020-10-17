/*
 * WLTEST interface.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_test.h 771567 2019-01-31 14:21:54Z $
 */

#ifndef _wlc_test_h_
#define _wlc_test_h_

#include <wlc_types.h>

wlc_test_info_t *wlc_test_attach(wlc_info_t *wlc);
void wlc_test_detach(wlc_test_info_t *test);

#if defined(WLTEST) || defined(WLPKTENG)
void *wlc_tx_testframe(wlc_info_t *wlc, struct ether_addr *da,
	struct ether_addr *sa, ratespec_t rate_override,
	int length, void *src_pkt, bool doampdu, wlc_bsscfg_t *bsscfg_perif,
	void *userdata, int userdata_len);
/* Create a test frame and enqueue into tx fifo */
extern void *wlc_mutx_testframe(wlc_info_t *wlc, struct scb *scb, struct ether_addr *sa,
                 ratespec_t rate_override, int fifo, int length, uint16 seq);

int wlc_d11_dma_lpbk_init(wlc_test_info_t *testi);
int wlc_d11_dma_lpbk_uninit(wlc_test_info_t *testi);
int wlc_d11_dma_lpbk_run(wlc_test_info_t *testi, uint8 *buf, int len);

extern bool wlc_test_pkteng_run(wlc_test_info_t *testi);
extern bool wlc_test_pkteng_run_dlofdma(wlc_test_info_t *testi);
extern bool wlc_test_pkteng_en(wlc_test_info_t *testi);
extern int wlc_test_pkteng_get_max_dur(wlc_test_info_t *testi);
extern int wlc_test_pkteng_get_min_dur(wlc_test_info_t *testi);
extern int wlc_test_pkteng_get_mode(wlc_test_info_t *testi);

#ifdef TESTBED_AP_11AX
extern void update_pkt_eng_ulstate(wlc_info_t *wlc, bool on);
extern void update_pkt_eng_ulnss(wlc_info_t *wlc, uint16 aid, uint16 nss);
extern void update_pkt_eng_ulbw(wlc_info_t *wlc, uint16 aid, uint16 bw);
extern void update_pkt_eng_trigcnt(wlc_info_t *wlc, uint16 trigger_count);
extern void update_pkt_eng_trigger_enab(wlc_info_t *wlc, uint16 aid, bool enab);
extern void update_pkt_eng_auto_ru_idx_enab(wlc_info_t *wlc);
extern void update_pkt_eng_ul_lsig_len(wlc_info_t *wlc, uint16 ul_lsig_len);

#endif /* TESTBED_AP_11AX */

#endif // endif

#endif /* _wlc_test_h_ */
