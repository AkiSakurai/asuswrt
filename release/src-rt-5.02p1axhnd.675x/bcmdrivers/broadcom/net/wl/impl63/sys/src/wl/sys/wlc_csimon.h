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

#if defined(DONGLEBUILD)

#include <bcm_csimon.h>
#include <hndm2m.h>         /* m2m_dd_done_cb_t */
#include <d11vasip_code.h>
#include <wlc_vasip.h>

#define CSIMON_NOOP		    do { /* no-op */ } while(0)

#if defined(CSIMON_DEBUG_BUILD)
#define CSIMON_DEBUG(fmt, arg...) \
    printf("%s: " fmt, __FUNCTION__, ##arg)

#define CSIMON_ASSERT(expr)         ASSERT(expr)
#else  /* ! CSIMON_DEBUG_BUILD */
#define CSIMON_DEBUG(fmt, arg...)   CSIMON_NOOP
#define CSIMON_ASSERT(expr)         CSIMON_NOOP
#endif /* ! CSIMON_DEBUG_BUILD */

/* Maximum CSI report and header size */
#define CSIMON_REPORT_SIZE 1952
#define CSIMON_HEADER_SIZE 64

#define CSIMON_SVMP_RING_DEPTH 2
#define CSIMON_DNGL_RING_DEPTH 32   // < 32 wlc_csimon_header xfers in progress

/* Max number of clients supported simultaneously for CSI reporting */
#define CSIMON_MAX_STA 10

#define C_CSI_IDX_NUM  2 // Number of SVMP memory-resident CSI reports

/* M_CSI_STATUS */
typedef enum
{
	C_CSI_IDX_NBIT          = 0,    // CSI index
	C_CSI_IDX_LB            = 4,
	C_CSI_ARM_NBIT          = 5     // ARM CSI capture for next RX frame
} eCsiStatusRegBitDefinitions;

#define C_CSI_IDX_BSZ          NBITS(C_CSI_IDX)
#define C_CSI_IDX_MASK         0x1F

/* C_CSI_VSEQN_POS */
typedef enum
{
	C_CSI_SEQN_NBIT         = 0,    // CSI index
	C_CSI_SEQN_LB           = 14,
	C_CSI_VLD_NBIT          = 15    // Record is valid
} eCsiValidSeqNBitDefinitions;
#define C_CSI_SEQN_BSZ         NBITS(C_CSI_SEQN)

/** csi info block: M_CSI_BLKS */
typedef enum
{
	C_CSI_ADDRL_POS         = 0,    // MAC ADDR LOW
	C_CSI_ADDRM_POS         = 1,    // MAC ADDR MED
	C_CSI_ADDRH_POS         = 2,    // MAC ADDR HI
	C_CSI_VSEQN_POS         = 3,    // Frame seq number
	C_CSI_RXTSFL_POS        = 4,    // CSI RX-TSF-time: lower 16 bits
	C_CSI_RXTSFML_POS       = 5,    // CSI RX-TSF-time: higher 16 bits
	C_CSI_RSSI0_POS         = 6,    // Ant0, Ant1 RSSI
	C_CSI_RSSI1_POS         = 7,    // Ant2, Ant3 RSSI
	C_CSI_BLK_WSZ
} eCsiInfoWordDefinitions;

#define M_CSI_MACADDRL(wlc)    (M_CSI_BLKS(wlc) + (C_CSI_ADDRL_POS * 2))
#define M_CSI_MACADDRM(wlc)    (M_CSI_BLKS(wlc) + (C_CSI_ADDRM_POS * 2))
#define M_CSI_MACADDRH(wlc)    (M_CSI_BLKS(wlc) + (C_CSI_ADDRH_POS * 2))
#define M_CSI_VSEQN(wlc)       (M_CSI_BLKS(wlc) + (C_CSI_VSEQN_POS * 2))
#define M_CSI_RXTSFL(wlc)      (M_CSI_BLKS(wlc) + (C_CSI_RXTSFL_POS * 2))
#define M_CSI_RXTSFML(wlc)     (M_CSI_BLKS(wlc) + (C_CSI_RXTSFML_POS * 2))
#define M_CSI_RSSI0(wlc)       (M_CSI_BLKS(wlc) + (C_CSI_RSSI0_POS * 2))
#define M_CSI_RSSI1(wlc)       (M_CSI_BLKS(wlc) + (C_CSI_RSSI1_POS * 2))

/** M2M callback data used by the callback function after the M2M xfer */
typedef struct csimon_m2m_cb_data {
	scb_t   *scb;		/**< pointer to the SCB */
	uint16  seqn;		/**< Sequence number in SHM - has valid bit */
	uint16  svmp_rpt_idx;	/**< CSI Report index in SVMP */
} csimon_m2m_cb_data_t;

typedef struct wlc_csimon_sta {
	struct ether_addr ea;       /**< MAC addr - also in SCB if associated */
	uint16 timeout;				/**< CSI timeout(ms) for STA */
	struct wl_timer *timer;			/**< timer for CSI monitor */
	uint32 assoc_ts;			/**< client association timestamp */
	/**< Callback data for M2M DMA xfer of the CSI data */
	csimon_m2m_cb_data_t m2m_cb_data;
	/**< run time boolean indicating if M2M copy is in progress */
	bool m2mcpy_busy;

	/**< Null frames sent */
	uint32 null_frm_cnt;
	/**< Number of CSI records successfully xferred by M2M DMA; wraps around */
	uint32 m2mxfer_cnt;
	/**< Null frames not successfully acked */
	uint32 ack_fail_cnt;
	/**< The application is not reading the reports fast enough */
	uint32 rec_ovfl_cnt;
	/**< M2M xfer failures */
	uint32 xfer_fail_cnt;
	/**< Both the CSI records/reports in mac memory were invalid */
	uint32 rpt_invalid_cnt;

	scb_t *scb;				/**< back pointer to SCB */
} wlc_csimon_sta_t;

union wlc_csimon_hdr {
	 uint8 u8[CSIMON_HEADER_SIZE];
	 struct {
	     uint32 format_id;		/**< Id/version of the CSI report format */
	     uint16 client_ea[3];	/**< client MAC address - 3 16-bit words */
	     uint16 bss_ea[3];		/**< BSS MAC address - 3 16-bit words */
	     chanspec_t chanspec;	/**< band, channel, bandwidth */
	     uint8 txstreams;		/**< number of tx spatial streams */
	     uint8 rxstreams;		/**< number of rx spatial streams */
	     uint32 report_ts;		/**< CSI Rx TSF timer timestamp */
	     uint32 assoc_ts;		/**< TSF timer timestamp at association time */
	     uint8 rssi[4];		/**< RSSI for each rx chain */
	 };
} __csimon_aligned;
typedef union wlc_csimon_hdr wlc_csimon_hdr_t;

typedef struct wlc_csimon_rec {
	wlc_csimon_hdr_t csimon_hdr;
	uint16 rpt_words[(CSIMON_RING_ITEM_SIZE-CSIMON_HEADER_SIZE)/sizeof(uint16)];
} wlc_csimon_rec_t;

extern bool wlc_csimon_enabled(scb_t *scb);

/** Attach the CSIMON WLC module */
extern wlc_csimon_info_t *wlc_csimon_attach(wlc_info_t *wlc);

/** Detach the CSIMON WLC module */
extern void wlc_csimon_detach(wlc_info_t *wlc);

/** Initialize the csimon xfer subsystem and attach a m2m callback */
extern int wlc_csimon_init(wlc_info_t *wlc,
                           m2m_dd_done_cb_t csimon_m2m_dd_done_cb);

/** CSIMON SCB Initialization */
extern int wlc_csimon_scb_init(wlc_info_t *wlc, scb_t *scb);

/** Dump the csimon internal state */
extern int wlc_csimon_internal_dump(bool verbose);

/** Transfer CSI record (= header in sysmem + report in SVMP mem) to host mem */
extern int wlc_csimon_record_copy(wlc_info_t *wlc, scb_t *scb);

/** Handle Null frame ack failure */
void wlc_csimon_ack_failure_process(wlc_info_t *wlc, scb_t *scb);

/** Timer function for processing periodic CSI reports */
extern void wlc_csimon_scb_timer(void *arg);

/** Function called back after the M2M DMA transfer is complete */
extern void wlc_csimon_m2m_dd_done_cb(void *usr_cbdata,
    dma64addr_t *xfer_src, dma64addr_t *xfer_dst, int xfer_len,
    uint32 xfer_arg);

#endif /* DONGLEBUILD */

#endif /* _wlc_csimon_h */
