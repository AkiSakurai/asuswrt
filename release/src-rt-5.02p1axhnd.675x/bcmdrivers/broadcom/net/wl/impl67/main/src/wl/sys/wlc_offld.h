/*
 * Common (OS-independent) portion of Broadcom 802.11 Networking Device Driver
 * TX and RX status offload module
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
 * $Id: wlc_offld.h 999999 2017-01-04 16:02:31Z $
 */

#ifndef _WLC_OFFLD_H_
#define _WLC_OFFLD_H_

/**
 * bme_attach initializes the BME part of the DMA channels. This function is to be called at
 * initialization phase. There will be no checking in the rest of the functions on whether or
 * not this attach call was made. This function initializes some of the BME registers. This
 * function is of the BCMATTACHFN type. So it can only be called during the attach phase.
 *
 * @param[in]	sih
 *
 * @param[in]	channel
 *   Enum, of either BME_CHANNEL_0, or BME_CHANNEL_1. BME_CHANNEL_0 maps to DMA channel 2, while
 *   BME_CHANNEL_1 maps to DMA channel 3. Start from d11 rev 130, BME_CHANNEL_1 has tx/phyrx status
 *   offload support.
 *
 * @returns
 *   wlc_offload_t * (bme_info): which is either pointer to an opaque structure which caller should
 *   pass on to every bme function or a NULL pointer in case of an error.: BCME_OK for success,
 *   otherwise failed to get stats.
 */
extern wlc_offload_t *wlc_offload_attach(wlc_info_t *wlc);

/**
 * With bme_detach the BME module can be de-initialized. Should only be called during attach
 * phase, to cleanup data. Function is of type BCMATTACHFN.
 *
 * @param[in]	wlc_offl
 *   Pointer to wlc_offl data as returned by wlc_offload_attach. If attach failed and returned NULL
 *   then this function should not be called.
 *
 * @returns
 *   <none>
 */
extern void wlc_offload_detach(wlc_offload_t *wlc_offl);

/**
 * (Re)initializes the BME hardware and software. Enables interrupts. Usually called on a 'wl up'.
 *
 * Prerequisite: firmware has progressed passed the BCMATTACH phase
 *
 * @returns
 *   <none>
 */
extern void wlc_offload_init(wlc_offload_t *wlc_offl, uint8* va);

/**
 * Disables interrupts. Usually called on a 'wl down'.
 *
 * Prerequisite: firmware has progressed passed the BCMATTACH phase
 *
 * @param[in]	va
 *   Pointer to sts_phyrx_va_non_aligned (if RX status offload is enabled, NULL otherwise)
 *
 * @returns
 *   <none>
 */
extern void wlc_offload_deinit(wlc_offload_t *wlc_offl);

#ifdef WLC_OFFLOADS_TXSTS
/**
 * For chips that support tx/phyrx status offloading only. Returns a pointer to one txstatus in the
 * circular buffer that was written by the d11 core but not yet consumed by software.
 *
 * param[in] caller_buf   Caller allocated buffer, large enough to contain one tx status package.
 */
extern void * wlc_offload_get_txstatus(wlc_offload_t *wlc_offl, void *caller_buf);

#endif /* WLC_OFFLOADS_TXSTS */

#if defined(WLC_OFFLOADS_TXSTS) || defined(WLC_OFFLOADS_RXSTS)
#if defined(BCMDBG)
/**
 * Dump BME status offload register and its software control block content.
 */
extern int wlc_offload_wl_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif

#endif /* WLC_OFFLOADS_TXSTS || WLC_OFFLOADS_RXSTS */

#ifdef WLC_OFFLOADS_RXSTS
/**
 * Returns a pointer to one phyrxstatus in the circular buffer from last processed index
 *
 * @param[in]	bmebufidx	bme saved the Phy RxStatus circular buffer index in the pointer
 *
 * @return			Pointer to a phy rxsts bufer if succeeded, otherwise return NULL
 */
extern void * wlc_offload_get_rxstatus(wlc_offload_t *wlc_offl, int16 *bmebufidx);

/**
 * Notify BME channel rxststus offload function to update rd_idx to hardware register. With this
 * bme can keep every posted Phy RxStatus content was not overridden before it processed.
 *
 * @param[in]	bmebufidx	Phy RxStatus circular buffer index to be freed
 */
extern void wlc_offload_release_rxstatus(void *ctx, int16 bmebufidx);

/**
 * Check if new phyrxstatus circular buffer has arrived.
 */
extern bool wlc_offload_rxsts_new_ready(wlc_offload_t *wlc_offl);

#endif /* WLC_OFFLOADS_RXSTS */

#if defined(WLC_OFFLOADS_M2M_INTR)

extern char * wlc_offload_irqname(wlc_info_t *wlc, void *btparam);
extern uint wlc_offload_m2m_si_flag(wlc_offload_t *wlc_offload);

/** IRQ handler for M2M core interrupts */
extern bool BCMFASTPATH wlc_offload_isr(wlc_info_t *wlc, bool *wantdpc);
extern void BCMFASTPATH wlc_offload_intrsupd(wlc_offload_t *wlc_offload);
extern void BCMFASTPATH wlc_offload_process_m2m_intstatus(wlc_info_t *wlc);

extern void wlc_offload_intrson(wlc_offload_t *wlc_offload);
extern void wlc_offload_intrsoff(wlc_offload_t *wlc_offload);
extern void wlc_offload_intrsrestore(wlc_offload_t *wlc_offload, uint32 macintmask);

#endif /* WLC_OFFLOADS_M2M_INTR */

#endif /* _WLC_OFFLD_H_ */
