/**
 * +--------------------------------------------------------------------------+
 *
 * wlc_csimon.h
 *
 * Dongle Interface of Channel State Information (CSI) Producer subsystem
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * +--------------------------------------------------------------------------+
 */
#ifndef _wlc_csimon_h_
#define _wlc_csimon_h_

#include <bcm_csimon.h>

#if defined(BCM_CSIMON) && !defined(HNDM2M)
#error "BCM_CSIMON requires HNDM2M"
#endif // endif

#include <hndm2m.h>         /* m2m_dd_done_cb_t */
#include <d11vasip_code.h>
#include <wlc_vasip.h>

#define CSIMON_NOOP		    do { /* no-op */ } while(0)

//#define CSIMON_DEBUG_BUILD
#if defined(CSIMON_DEBUG_BUILD)
#if defined(DONGLEBUILD)
#define CSIMON_DEBUG(fmt, arg...) \
    printf("%s: " fmt, __FUNCTION__, ##arg)
#else /* ! DONGLEBUILD */
#define CSIMON_DEBUG(fmt, arg...) \
	do { WL_TIMESTAMP (); printf ("%s: " fmt, __FUNCTION__, ##arg); } while (0)
#endif /* ! DONGLEBUILD */

#define CSIMON_ASSERT(expr)         ASSERT(expr)
#else  /* ! CSIMON_DEBUG_BUILD */
#define CSIMON_DEBUG(fmt, arg...)   CSIMON_NOOP
#define CSIMON_ASSERT(expr)         CSIMON_NOOP
#endif /* ! CSIMON_DEBUG_BUILD */

/* Maximum CSI report and header size */
#define CSIMON_REPORT_SIZE 1952
#define CSIMON_HEADER_SIZE 64

#define CSIMON_SVMP_RING_DEPTH 2
#define CSIMON_LOCAL_RING_DEPTH 32   // < 32 wlc_csimon_record xfers in progress

/* Max number of clients supported simultaneously for CSI reporting */
#define CSIMON_MAX_STA 10

extern bool wlc_csimon_enabled(scb_t *scb);

/** Attach the CSIMON WLC module */
extern wlc_csimon_info_t *wlc_csimon_attach(wlc_info_t *wlc);

/** Detach the CSIMON WLC module */
extern void wlc_csimon_detach(wlc_csimon_info_t *ci);

/** Initialize the csimon xfer subsystem and attach a m2m callback */
extern int wlc_csimon_init(wlc_info_t *wlc);

/** CSIMON SCB Initialization */
extern int wlc_csimon_scb_init(wlc_info_t *wlc, scb_t *scb);

/** Transfer CSI record (= header in sysmem + report in SVMP mem) to host mem */
extern int wlc_csimon_record_copy(wlc_info_t *wlc, scb_t *scb);

/** Handle Null frame ack failure */
void wlc_csimon_ack_failure_process(wlc_info_t *wlc, scb_t *scb);

/** Timer function for processing periodic CSI reports */
extern void wlc_csimon_scb_timer(void *arg);

/** Function called back after the M2M DMA transfer is complete */
extern void wlc_csimon_m2m_dd_done_cb(void *usr_cbdata,
    dma64addr_t *xfer_src, dma64addr_t *xfer_dst, int xfer_len,
    void *xfer_arg);

/** Add/start client-based timer */
void wlc_csimon_timer_add(wlc_info_t *wlc, struct scb *scb);

/** Delete/free client-based timer */
void wlc_csimon_timer_del(wlc_info_t *wlc, struct scb *scb);

/**  Check if client-based timer is allocated */
bool wlc_csimon_timer_isnull(struct scb *scb);

/** Set the association timestamp as a reference */
void wlc_csimon_assocts_set(wlc_info_t *wlc, struct scb *scb);

/** Construct and return name of M2M IRQ */
char * wlc_csimon_irqname(wlc_info_t *wlc, void *btparam);

#if defined(DONGLEBUILD)

/** Dump the csimon internal state */
extern int wlc_csimon_internal_dump(wlc_info_t *wlc, bool verbose);

#else /* ! DONGLEBUILD */

/** Get the SI flag for the M2M system */
uint wlc_csimon_m2m_si_flag(wlc_info_t *wlc);

/** ISR for M2M interrupts */
extern bool BCMFASTPATH wlc_csimon_isr(wlc_info_t *wlc, bool* wantdpc);

/** DPC for M2M interrupts */
extern void BCMFASTPATH wlc_csimon_dpc(wlc_info_t *wlc);

/** Disable M2M interrupts */
extern void wlc_csimon_intrsoff(wlc_info_t *wlc);

#endif /* ! DONGLEBUILD */

#endif /* _wlc_csimon_h */
