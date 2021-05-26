/*
 * Common (OS-independent) portion of Broadcom 802.11 Networking Device Driver
 * Tx and PhyRx status transfer module
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
 * $Id:$
 */

#ifndef _WLC_STS_XFER_
#define _WLC_STS_XFER_

#define STS_XFER_PHYRXS_SEQID_INVALID		((uint16) 0xffff)

/* Tx and PhyRx Status Tranfer module */
struct wlc_sts_xfer_info {
	int	unit;		/* Radio unit */
};

/* STS_XFER module attach and detach handlers */
extern wlc_sts_xfer_info_t *wlc_sts_xfer_attach(wlc_info_t *wlc);
extern void wlc_sts_xfer_detach(wlc_sts_xfer_info_t *wlc_sts_xfer_info);

/* STS_XFER handler to flush pending packets and PhyRx Status buffers */
extern void wlc_sts_xfer_flush_queues(wlc_info_t *wlc);

/** STS_XFER PhyRx Status receive handlers */
extern void wlc_sts_xfer_bmac_recv(wlc_info_t *wlc, uint fifo,	rx_list_t *rx_list_release);
extern void wlc_sts_xfer_bmac_recv_done(wlc_info_t *wlc, uint fifo);

/** STS_XFER PhyRx Status buffer release handlers */
extern void wlc_sts_xfer_phyrxs_release(wlc_info_t *wlc, void *pkt);
extern void wlc_sts_xfer_phyrxs_free(wlc_info_t *wlc, void *pkt);

/** Rx packets pending */
extern bool wlc_sts_xfer_phyrxs_rxpend(wlc_info_t *wlc, uint fifo);

#if defined(STS_XFER_M2M_INTR)

extern char * wlc_sts_xfer_irqname(wlc_info_t *wlc, void *btparam);
extern uint wlc_sts_xfer_m2m_si_flag(wlc_info_t *wlc);

/** STS_XFER IRQ handler for M2M core interrupts */
extern bool BCMFASTPATH wlc_sts_xfer_isr(wlc_info_t *wlc, bool *wantdpc);

extern void BCMFASTPATH wlc_sts_xfer_intrsupd(wlc_info_t *wlc);
extern void BCMFASTPATH wlc_sts_xfer_process_m2m_intstatus(wlc_info_t *wlc);

extern void wlc_sts_xfer_intrson(wlc_info_t *wlc);
extern void wlc_sts_xfer_intrsoff(wlc_info_t *wlc);
extern void wlc_sts_xfer_intrsrestore(wlc_info_t *wlc, uint32 macintmask);

#endif /* STS_XFER_M2M_INTR */

#endif /* _WLC_STS_XFER_ */
