/*
 * H/W info API of
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_hw.h 783499 2020-01-30 09:28:27Z $
 */

#ifndef _wlc_hw_h_
#define _wlc_hw_h_

#include <wlc_types.h>
#include <wlc_phy_types.h>
#include <hnddma.h>
#include <wlc_pio.h>

#define WLC_HW_DMACOMMON(wlc)		((wlc)->hw_pub->dmacommon)
#define WLC_HW_DI(wlc, fifo)		((wlc)->hw_pub->di[fifo])
#ifdef BCM_DMA_CT
#define WLC_HW_AQM_DI(wlc, fifo)	((wlc)->hw_pub->aqm_di[fifo])
#endif // endif
#define WLC_HW_PIO(wlc, fifo)		((wlc)->hw_pub->pio[fifo])

#define WLC_HW_NFIFO_INUSE(wlc)		((wlc)->hw_pub->nfifo_inuse)
#define WLC_HW_NFIFO_TOTAL(wlc)		((wlc)->hw_pub->nfifo_total)

#define WLC_HW_MAP_TXFIFO(wlc, fifo)	\
	(D11REV_GE((wlc)->pub->corerev, 128) ? \
		wlc_hw_map_txfifo((wlc)->hw, (fifo)) : \
		(fifo))
#define WLC_HW_UNMAP_TXFIFO(wlc, fifo)	\
	(D11REV_GE((wlc)->pub->corerev, 128) ? \
		wlc_hw_unmap_txfifo((wlc)->hw, (fifo)) : \
		(fifo))

/* PIO Mode Support */
#ifdef WLPIO
#define PIO_ENAB_HW(wlc_hw)		((wlc_hw)->_piomode)
#else
#define PIO_ENAB_HW(wlc_hw)		0
#endif /* WLPIO */

/* Data APIs */
/* Code outside the HW module (wlc_bmac, wlc_intr, wlc_diag,
 * etc.) shall not modify nor read these data directly, and
 * they shall use APIs or MACROs in case these data are moved to inside the
 * module later when performance is determined to be a non-issue.
 *
 * These data shall be read via the following MACROs:
 * - WLC_HW_DMACOMMON()
 * - WLC_HW_DI()
 * - WLC_HW_AQM_DI()
 * - WLC_HW_PIO()
 * - WLC_HW_NFIFO_INUSE()
 * - WLC_HW_NFIFO_TOTAL()
 * These data shall be modified via the following APIs:
 * - wlc_hw_set_dmacommon()
 * - wlc_hw_set_di()
 * - wlc_hw_set_aqm_di()
 * - wlc_hw_set_pio()
 * - wlc_hw_update_nfifo()
 */
struct wlc_hw {
	dma_common_t	*dmacommon;		/* Global DMA state and info for all DMA chn */

	pio_t		*pio[NFIFO_LEGACY];	/* pio handlers, per fifo */
	hnddma_t	*di[NFIFO_EXT_MAX];	/* hnddma handles, per fifo */
	hnddma_t	*aqm_di[NFIFO_EXT_MAX];	/* hnddma handles, per AQM fifo */

	uint		nfifo_total;		/* Total number of TX FIFOs supported by chip */
	uint		nfifo_inuse;		/* Number of TX FIFOs that are in use */
};

/*
 * Detect Card removed.
 * Even checking an sbconfig register read will not false trigger when the core is in reset.
 * it breaks CF address mechanism. Accessing gphy phyversion will cause SB error if aphy
 * is in reset on 4306B0-DB. Need a simple accessible reg with fixed 0/1 pattern
 * (some platforms return all 0).
 * If clocks are present, call the sb routine which will figure out if the device is removed.
 */
#ifdef BCMNODOWN
#define DEVICEREMOVED(wlc)	0
#else
#define DEVICEREMOVED(wlc)      wlc_hw_deviceremoved((wlc)->hw)
#endif /* BCMNODOWN */

/* Function APIs */
extern wlc_hw_info_t *wlc_hw_attach(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err);
extern void wlc_hw_detach(wlc_hw_info_t *wlc_hw);

extern void wlc_hw_update_nfifo(wlc_hw_info_t *wlc_hw);
extern int wlc_hw_verify_fifo_layout(wlc_hw_info_t *wlc_hw);
extern uint wlc_hw_map_txfifo(wlc_hw_info_t *wlc_hw, uint fifo);
extern uint wlc_hw_unmap_txfifo(wlc_hw_info_t *wlc_hw, uint fifo);
extern void wlc_hw_set_piomode(wlc_hw_info_t *wlc_hw, bool piomode);
extern bool wlc_hw_get_piomode(wlc_hw_info_t *wlc_hw);

extern void wlc_hw_set_dmacommon(wlc_hw_info_t *wlc_hw, dma_common_t *dmacommon);
extern void wlc_hw_set_di(wlc_hw_info_t *wlc_hw, uint fifo, hnddma_t *di);
extern void wlc_hw_set_aqm_di(wlc_hw_info_t *wlc_hw, uint fifo, hnddma_t *di);
extern void wlc_hw_set_pio(wlc_hw_info_t *wlc_hw, uint fifo, pio_t *pio);
extern void wlc_hw_set_nfifo_inuse(wlc_hw_info_t *wlc_hw, uint fifo_inuse);

extern bool wlc_hw_deviceremoved(wlc_hw_info_t *wlc_hw);
extern uint32 wlc_hw_get_wake_override(wlc_hw_info_t *wlc_hw);
extern uint wlc_hw_get_bandunit(wlc_hw_info_t *wlc_hw);
extern uint32 wlc_hw_get_macintmask(wlc_hw_info_t *wlc_hw);
extern uint32 wlc_hw_get_macintstatus(wlc_hw_info_t *wlc_hw);

/* MHF2_SKIP_ADJTSF ucode host flag manipulation - global user ID */
#define WLC_SKIP_ADJTSF_SCAN		0
#define WLC_SKIP_ADJTSF_RM		1
#define WLC_SKIP_ADJTSF_MCNX		2
#define WLC_SKIP_ADJTSF_USER_MAX	4
extern void wlc_skip_adjtsf(wlc_info_t *wlc, bool skip, wlc_bsscfg_t *cfg, uint32 user, int bands);
/* MCTL_AP maccontrol register bit manipulation - global user ID */
#define WLC_AP_MUTE_SCAN	0
#define WLC_AP_MUTE_RM		1
#define WLC_AP_MUTE_USER_MAX	4
/* mux s/w MCTL_AP on/off request */
#ifdef AP
extern void wlc_ap_mute(wlc_info_t *wlc, bool mute, wlc_bsscfg_t *cfg, uint32 user);
#else
#define wlc_ap_mute(wlc, mute, cfg, user) do {} while (0)
#endif // endif
/* force MCTL_AP off; mux s/w MCTL_AP on request */
extern void wlc_ap_ctrl(wlc_info_t *wlc, bool on, wlc_bsscfg_t *cfg, uint32 user);

extern char* wlc_hw_get_vars_table(wlc_info_t *wlc);
#endif /* !_wlc_hw_h_ */
