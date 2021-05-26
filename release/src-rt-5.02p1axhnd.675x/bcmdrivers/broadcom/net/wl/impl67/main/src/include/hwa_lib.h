/*
 * HWA library routines for PCIE and MAC facing blocks.
 *
 * Block level management:
 *
 * Each block needs to implement an attach, detach, init and fini handler.
 * A split in "attach" and "init" phase allows reclaimation of attach text
 * segment and permits an intermediary stage, e.g. Dongle Syncs with DHD.
 * Attach phase will be followed by DHD handshake during which Host will
 * convey to Dongle various host side addresses e.g. RD and WR indices arrays,
 * and advertize its capabilities e.g. 32b or 64b addressing, coherency, etc.
 * Blocks are enabled in init phase.
 *
 * Design Disclaimers:
 * - Used global hwa_dev_t.
 * - No consideration for ROM based dongles. HWA is a datapath subsystem.
 * - Defaults to HWA-2.0 support, in particular WI formats.
 *
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
#ifndef _HWA_LIB_H
#define _HWA_LIB_H

#include <hwa_export.h>
#include <hwa_regs.h>
#include <hwa_defs.h>
#include <bcm_ring.h>
#include <bcmpcie.h>

/*
 * -----------------------------------------------------------------------------
 *
 * Section: HWA 2.0 Software Driver Version History
 *
 *   1: Validation driver for Full Dongle
 *        - HWA-2.0 hardware specification with all blocks enabled.
 *        - 43684 generics specification with 1 MAC core.
 *        - Full Dongle with 64 bit host address capability.
 *        - PCIE IPC 1a, 2b, 4b using ACWI with AGGR factor of 4.
 *          Txpost use CWI format with no aggregation. (Runner complexity)
 *        - Integration with CFP.
 *   2: SW driver enhance to support HWA2.1 reg spec in Full Dongle
 *
 * -----------------------------------------------------------------------------
 */
#define HWA_DRIVER_VERSION          2

/*
 * -----------------------------------------------------------------------------
 * Section: Common macros and helper utilities
 * -----------------------------------------------------------------------------
 */

// Forward declarations
struct pcie_ipc;
struct pcie_ipc_rings;
typedef struct hwa_dev hwa_dev_t;

/**
 * SW Driver configurations
 * - "Host" refers to driver component running off-chip, NIC or DHD in FD mode
 * - "Dngl" refers to driver component running on-chip in FD mode - aka firmware
 *
 * In FullDongle mode, Host configuration will be advertised to dongle by DHD.
 */

#define HWA_CONFIG_INVALID          (~0U)       // See hwa_config()

// NIC versus FullDongle mode of operation
#ifdef DONGLEBUILD
#define HWA_NIC_MODE                (0U)        // NIC Mode
#define HWA_FD_MODE                 (1U)        // Full Dongle Mode
#else
#define HWA_NIC_MODE                (1U)
#define HWA_FD_MODE                 (0U)
#endif /* DONGLEBUILD */

#ifdef BCMPCIEDEV
#define HWA_DRIVER_MODE             (HWA_FD_MODE)
#ifndef PCIE_DMAINDEX16
#error "PCIE_DMAINDEX16 is not defined"
#endif // endif
#else  /* ! BCMPCIEDEV */
#define HWA_DRIVER_MODE             (HWA_NIC_MODE)
#endif /* ! BCMPCIEDEV */

// HW Coherency support
#define HWA_SW_COHERENCY            (0U)        // SW coherency model
#define HWA_HW_COHERENCY            (1U)        // HW coherency model
#define HWA_DNGL_COHERENCY          HWA_HW_COHERENCY

// Addressing scheme: Do not change below values
#define HWA_64BIT_ADDRESSING        (0U)        // 64bit addressing scheme
#define HWA_32BIT_ADDRESSING        (1U)        // 32bit addressing scheme

#define HWA_DNGL_ADDRESSING         HWA_32BIT_ADDRESSING

// 4908 64bit Linux platform uses a fixed hiaddr.
#define HWA_LINUX64_HIADDR          (0xffffffc0)
#define HWA_INVALID_HIADDR          (0xdeaddead)

// MACIF placement of various MAC facing FIFOs
#define HWA_MACIF_IN_HOSTMEM        (0U)        // HWA MAC interfaces in DDR
#define HWA_MACIF_IN_DNGLMEM        (1U)        // HWA MAC interfaces in Sysmem
/*
 * NIC mode: To reduce PHY underruns, all MAC Interfaces may be placed in device
 * memory. This would require a host-based DMA engine or leverage one of the
 * non descriptor based mem2mem DMA engines. Until such a feature is implemented
 * all MAC Interfaces are placed in host memory in a NIC mode driver.
 */

// DMA transfer over PCIE: Do not change below values
#define HWA_DMA_XFER_PCIE           (0U)       // Host memory DMA xfer(o/ PCIE)
#define HWA_DMA_XFER_NOTPCIE        (1U)       // Device memory DMA xfer
#if (HWA_MODE == HWA_NIC_MODE)
#define HWA_DMA_XFER                (HWA_DMA_XFER_PCIE)
#else
#define HWA_DMA_XFER                (HWA_DMA_XFER_NOTPCIE)
#endif // endif

// Statistics min busy time threshold beyond which stall durations are counted.
#define HWA_STATISTICS_MIN_BUSY     10          // 10 microsecs

// Compute an element index, given a table base and an element pointer
#define HWA_TABLE_INDX(type, base, elem) \
	((int)(((type*)(elem)) - ((type*)(base))))

// Compute an element pointer, given a table base and the element's index
#define HWA_TABLE_ELEM(type, base, indx) \
	(((type*)(base)) + (indx))

// Compute an element address, given a table base and the element's index
#define HWA_TABLE_ADDR(type, base, indx) \
	((base) + ((indx) * sizeof(type)))

/*
 * -----------------------------------------------------------------------------
 *
 * Section: hwa_ring_t
 *
 * hwa_ring_t abstracts a SW to/from HWA interface implemented as a circular
 * ring. It uses a producer consumer paradigm with read and write indexes as
 * implemented in bcm_ring.h.
 *
 * Producer updates WR index and fetches RD index from consumer context.
 * Consumer updates RD index and fetches WR index from producer context.
 *
 * Producer or Consumer may be HWA depending on H2S and S2H ring direction,
 * respectively. HWA maintains its local RD and WR index in registers, that SW
 * needs to fetch or flush.
 *
 * Get and Put APIs are provided to fetch and flush a HW register respectively.
 * These APIs allow SW to sync up with a HWA managed (register based) RD or WR
 * index. In the S2H direction, as a producer, SW gets the RD index from a HW
 * register and puts the WR index to a HW register. In the H2S direction, SW as
 * a consumer, gets the WR index from a HW register and puts its RD index to HW.
 *
 * Each hwa_ring, maintains a local bcm_ring state and is initialized with the
 * locations of the HW RD and WR registers, the ring memory base and depth and
 * debug info.
 *
 * Design Note:
 * hwa_ring_prod_upd() allows the caller to commit the WR index to HWA.
 * hwa_ring_cons_upd() does not allow the caller to commit RD index. An explicit
 * invocation of hwa_ring_cons_put() is required.
 *
 * -----------------------------------------------------------------------------
 */

#define HWA_RING_DEBUG                  // Enable HWA_RING debug support
#if defined(HWA_RING_DEBUG)
#define HWA_RING_ASSERT(exp)            ASSERT(exp)
#else  /* ! HWA_RING_DEBUG */
#define HWA_RING_ASSERT(exp)            HWA_NOOP
#endif /* ! HWA_RING_DEBUG */

// List of all circular ring interfaces between HWA|PP and SW
typedef enum hwa_ring_num
{
	HWA_RXPOST_WI_H2S_RINGNUM       = 0, // 1a->SW: RxPost WorkItems
	HWA_RXFILL_RXFREE_S2H_RINGNUM   = 1, // SW->1b: free RxBuffers  "FREEIDXSRC"
	HWA_RXFILL_RXFIFO_H2S_RINGNUM   = 2, // 1b->SW: RxBufIndex posted "D11BDEST"
	HWA_RXCPLE_CED_S2H_RINGNUM      = 3, // SW->2b: Completion Entry Desc ring
	HWA_TXPOST_SCHEDCMD_S2H_RINGNUM = 4, // SW->3a: Schedule Commands
	HWA_TXPOST_PKTCHAIN_H2S_RINGNUM = 5, // 3a->SW: PktChain schedule response
	HWA_TXFIFO_PKTCHAIN_S2H_RINGNUM = 6, // SW->3b: PktChain transmit request
	HWA_TXSTAT_QUEUE_H2S_RINGNUM    = 7, // 4a->SW: TxStatus to SW
	HWA_TXCPLE_CED_S2H_RINGNUM      = 8, // SW->4b: Completion Entry Desc ring
	HWA_TXPOST_TXFREE_S2H_RINGNUM   = 9, // SW->3a: Free TxBuffers "FREEIDXTX"

	// With PacketPager, SW does not directly interface to HWA blocks.
	// List of PacketPager circular ring interfaces: 5 Request + 3 Response
	HWA_PKTPGR_PAGEIN_S2H_RINGNUM   = 10, // SW->PP: PAGEIN  Request
	HWA_PKTPGR_PAGEIN_H2S_RINGNUM   = 11, // PP->SW: PAGEIN  Response
	HWA_PKTPGR_PAGEOUT_S2H_RINGNUM  = 12, // SW->PP: PAGEOUT Request
	HWA_PKTPGR_PAGEOUT_H2S_RINGNUM  = 13, // PP->SW: PAGEOUT Response
	HWA_PKTPGR_PAGEMGR_S2H_RINGNUM  = 14, // SW->PP: PAGEMGR Request
	HWA_PKTPGR_PAGEMGR_H2S_RINGNUM  = 15, // PP->SW: PAGEMGR Response
	HWA_PKTPGR_FREEPKT_S2H_RINGNUM  = 16, // SW->PP: FREEPKT(Rx, Tx) Request
	HWA_PKTPGR_FREERPH_S2H_RINGNUM  = 17, // SW->PP: FREE(RPH, D11) Request

	HWA_INTERFACE_RING_NUM_MAX      = 18  // 6 bit ring_num: hence < 63

} hwa_ring_num_t;

// hwa_ring_id encodes HWA block_id, direction and ring number. Used in debug.
typedef union hwa_ring_id
{
	uint16 u16;                         // 16bit hwa_ring_id
	struct {
		HWA_LE_EXPR(
			uint16 ring_num : 6;        // hwa_ring_num_t lsb6
			uint16 ring_dir : 2;        // S2H or H2S
			uint16 block_id : 8; )      // HWA block id
		HWA_BE_EXPR(
			uint16 block_id : 8;        // HWA block id
			uint16 ring_dir : 2;        // S2H or H2S
			uint16 ring_num : 6; )      // hwa_ring_num_t lsb6
	};
} hwa_ring_id_t;

// Encode/Decode macros for a hwa_ring_id
#define HWA_RING_DIR_SHIFT      6
#define HWA_RING_DIR_MASK       (0x3 << HWA_RING_DIR_SHIFT)
#define HWA_BLOCK_ID_SHIFT      8
#define HWA_BLOCK_ID_MASK       (0xff << HWA_BLOCK_ID_SHIFT)
#define HWA_RING_NUM_SHIFT      0
#define HWA_RING_NUM_MASK       (0x3f << HWA_RING_NUM_SHIFT)

#define HWA_RING_ID(block_id, ring_dir, ring_num) \
	(((block_id) << HWA_BLOCK_ID_SHIFT) \
	| (((ring_dir) << HWA_RING_DIR_SHIFT) & HWA_RING_DIR_MASK) \
	| (((ring_num) << HWA_RING_NUM_SHIFT) & HWA_RING_NUM_MASK))

// Miscellaneous macros
#define HWA_RING_NULL           ((hwa_ring_t *)NULL)
#define HWA_RING_NAME_SIZE      4
#define HWA_RING_STATE(hwa_ring) (&((hwa_ring)->state))

// hwa_ring object to implement an interface between HWA and SW
typedef struct hwa_ring                 // Producer/Consumer circular ring
{
	bcm_ring_t      state;              // SW context: read and write state
	hwa_reg_addr_t  reg_wr;             // HW context: write register H2S ring
	hwa_reg_addr_t  reg_rd;             // HW context: read register S2H ring

	void            *memory;            // memory for ring
	uint16          depth;              // ring depth: num elements in ring
	hwa_ring_id_t   id;                 // ring identifier for debug
	char            name[HWA_RING_NAME_SIZE]; // debug
} hwa_ring_t;

// Locate an element of specified type in a hwa_ring at a given index.
#define HWA_RING_ELEM(type, hwa_ring, index) \
({ \
	HWA_RING_ASSERT((hwa_ring) != HWA_RING_NULL); \
	HWA_RING_ASSERT((index) < ((hwa_ring)->depth)); \
	((type*)((hwa_ring)->memory)) + (index); \
})

// Locate the position where the next element may be produced.
#define HWA_RING_PROD_ELEM(type, hwa_ring) \
({ \
	HWA_RING_ASSERT((hwa_ring) != HWA_RING_NULL); \
	HWA_RING_ELEM(type, (hwa_ring), (hwa_ring)->state.write); \
})

// Locate the position from where the next element may be consumed
#define HWA_RING_CONS_ELEM(type, hwa_ring) \
({ \
	HWA_RING_ASSERT((hwa_ring) != HWA_RING_NULL); \
	HWA_RING_ELEM(type, (hwa_ring), (hwa_ring)->state.read); \
})

// Exported HWA Ring API

// Ring construction, destruction and debug dump
void    hwa_ring_init(hwa_ring_t *ring, const char *name,
            uint8 block_id, uint8 ring_dir, uint8 ring_num,
            uint16 depth, void *memory,
            hwa_reg_addr_t reg_wr, hwa_reg_addr_t reg_rd);
void    hwa_ring_fini(hwa_ring_t *ring);
void    hwa_ring_dump(hwa_ring_t *h2s_ring, struct bcmstrbuf *b, const char *prefix);

// SW is producer and HWA is consumer in a S2H direction ring
static INLINE void hwa_ring_prod_put(hwa_ring_t *s2h_ring); // Flush WR to HWA
static INLINE void hwa_ring_prod_get(hwa_ring_t *s2h_ring); // Fetch RD from HWA
static INLINE void hwa_ring_prod_upd(hwa_ring_t *s2h_ring,  // Produce elements
	                   const uint16 items, const bool commit);

// SW is consumer and HWA is producer in a H2S direction ring
static INLINE void hwa_ring_cons_put(hwa_ring_t *h2s_ring); // Flush RD to HWA
static INLINE void hwa_ring_cons_get(hwa_ring_t *h2s_ring); // Fetch WR from HWA
static INLINE int  hwa_ring_cons_upd(hwa_ring_t *h2s_ring); // Consume 1 element

// SW tests whether elements are available for consumption in a H2S ring
static INLINE bool hwa_ring_is_empty(hwa_ring_t *h2s_ring); // H2S work to cons

// SW tests whether space is available for production of elements in a S2H ring
static INLINE bool hwa_ring_is_full(hwa_ring_t *s2h_ring);  // S2H work to prod

static INLINE void // Given a S2H ring, SW flushes WR index to HWA
hwa_ring_prod_put(hwa_ring_t *s2h_ring)
{
	HWA_RING_ASSERT(s2h_ring != HWA_RING_NULL);
	HWA_RING_ASSERT(s2h_ring->reg_wr != NULL);
	HWA_WR_REG_ADDR(s2h_ring->name, // flush WR index from SW local state to HWA
	                s2h_ring->reg_wr, HWA_RING_STATE(s2h_ring)->write);
}

static INLINE void // Given a S2H ring, SW fetches RD index from HWA
hwa_ring_prod_get(hwa_ring_t *s2h_ring)
{
	HWA_RING_ASSERT(s2h_ring != HWA_RING_NULL);
	HWA_RING_ASSERT(s2h_ring->reg_rd != NULL);
	HWA_RING_STATE(s2h_ring)->read = // fetch HWA RD index into SW local state
		HWA_RD_REG_ADDR(s2h_ring->name, s2h_ring->reg_rd);
}

static INLINE void // Produce elements into a ring, and commit WR index to HWA
hwa_ring_prod_upd(hwa_ring_t *s2h_ring, const uint16 items, const bool commit)
{
	HWA_RING_ASSERT(s2h_ring != HWA_RING_NULL);
	HWA_RING_STATE(s2h_ring)->write = (HWA_RING_STATE(s2h_ring)->write + items)
		                               % s2h_ring->depth;
	if (commit == TRUE) {
		// Memory barrier before posting the descriptor
		DMB();

		hwa_ring_prod_put(s2h_ring); // CAUTION: WR index register is updated!
	}
}

static INLINE void // Produce elements into a ring, and commit WR index to HWA
hwa_ring_prod_replace(hwa_ring_t *s2h_ring, int write_index, const bool commit)
{
	HWA_RING_ASSERT(s2h_ring != HWA_RING_NULL);

	HWA_RING_STATE(s2h_ring)->write = write_index;

	if (commit == TRUE) {
		// Memory barrier before posting the descriptor
		DMB();

		hwa_ring_prod_put(s2h_ring); // CAUTION: WR index register is updated!
	}
}

// NOTE: caller need to update s2h_ring SW context
static INLINE int // Given a S2H ring, SW fetches RD index from HWA
hwa_ring_prod_avail(hwa_ring_t *s2h_ring)
{
	HWA_RING_ASSERT(s2h_ring != HWA_RING_NULL);
	return bcm_ring_prod_avail(HWA_RING_STATE(s2h_ring), s2h_ring->depth);
}

static INLINE void // Given a H2S ring, SW flushes RD index to HWA
hwa_ring_cons_put(hwa_ring_t *h2s_ring)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	HWA_RING_ASSERT(h2s_ring->reg_rd != NULL);
	HWA_WR_REG_ADDR(h2s_ring->name, // flush RD index from SW local state to HWA
	                h2s_ring->reg_rd, HWA_RING_STATE(h2s_ring)->read);
}

static INLINE void // Given a H2S ring, SW update user specified RD index to HWA
hwa_ring_cons_put_rd(hwa_ring_t *h2s_ring, int read_index)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	HWA_RING_ASSERT(h2s_ring->reg_rd != NULL);
	HWA_WR_REG_ADDR(h2s_ring->name, // flush RD index from SW local state to HWA
	                h2s_ring->reg_rd, read_index);
}

static INLINE void // Given a H2S ring, SW fetches WR index from HWA
hwa_ring_cons_get(hwa_ring_t *h2s_ring)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	HWA_RING_ASSERT(h2s_ring->reg_wr != NULL);
	HWA_RING_STATE(h2s_ring)->write = // fetch HWA WR index into local SW state
		HWA_RD_REG_ADDR(h2s_ring->name, h2s_ring->reg_wr);
}

static INLINE int // Given a H2S ring, SW fetches the next element to consume
hwa_ring_cons_upd(hwa_ring_t *h2s_ring)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	// NOTE: HWA's RD index is NOT updated. May return empty.
	return bcm_ring_cons(HWA_RING_STATE(h2s_ring), h2s_ring->depth);
}

static INLINE int // Given a H2S ring, SW fetches the next element to consume in "pend" mode
hwa_ring_cons_pend(hwa_ring_t *h2s_ring, int *pend_read)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	// NOTE: HWA's RD index is NOT updated. May return empty.
	return bcm_ring_cons_pend(HWA_RING_STATE(h2s_ring), pend_read, h2s_ring->depth);
}

static INLINE void // Given a H2S ring, SW commit a previously pending read
hwa_ring_cons_done(hwa_ring_t *h2s_ring, int read_index)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	bcm_ring_cons_done(HWA_RING_STATE(h2s_ring), read_index);
}

static INLINE void // Given a H2S ring, SW set ring in state where all elements are consumed.
hwa_ring_cons_all(hwa_ring_t *h2s_ring)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	bcm_ring_cons_all(HWA_RING_STATE(h2s_ring));
}

static INLINE bool // Given a H2S ring, SW checks whether work available
hwa_ring_is_empty(hwa_ring_t *h2s_ring)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	return bcm_ring_is_empty(HWA_RING_STATE(h2s_ring));
}

static INLINE bool // Given a S2H ring, SW checks whether space is available
hwa_ring_is_full(hwa_ring_t *s2h_ring)
{
	HWA_RING_ASSERT(s2h_ring != HWA_RING_NULL);

	if (bcm_ring_is_full(HWA_RING_STATE(s2h_ring), s2h_ring->depth)) {
		hwa_ring_prod_get(s2h_ring); // lazy RD update
	}

	return bcm_ring_is_full(HWA_RING_STATE(s2h_ring), s2h_ring->depth);
}

static INLINE bool // Given a S2H ring, SW checks whether work is available
hwa_ring_is_cons_all(hwa_ring_t *s2h_ring)
{
	HWA_RING_ASSERT(s2h_ring != HWA_RING_NULL);

	hwa_ring_prod_get(s2h_ring);
	return hwa_ring_is_empty(s2h_ring);
}

// NOTE: caller need to update h2s_ring SW context
static INLINE int // Given a H2S ring, SW checks how many availables.
hwa_ring_cons_avail(hwa_ring_t *h2s_ring)
{
	HWA_RING_ASSERT(h2s_ring != HWA_RING_NULL);
	return bcm_ring_cons_avail(HWA_RING_STATE(h2s_ring), h2s_ring->depth);
}

static INLINE void
hwa_ring_next_index(int *index, int ring_size)
{
	*index = (*index + 1) % ring_size;
}

/*
 * -----------------------------------------------------------------------------
 *
 * Section: Upstream subsystems register their callback handlers.
 *
 * HWA driver is implemented as a library of functions. In the case of work
 * to be transferred to the upper layer (originating from a HWA interrupt), a
 * series of upstream call back handlers need to be registered by the upper
 * layer. HWA driver will service a request to handle an interrupt (e.g. H2S
 * interface). As a consumer of H2S interface, work will be forwarded to the
 * upstream handler.
 *
 * For each H2S Interface a pair of callbacks may be registered, namely
 * - Process one item from the H2S interface
 * - Done processing N number of items, where N is defined as a budget or a
 *   end-of-request. Done processing handler is optional. A Done processing
 *   handler may be defined, for a semantics of End-Of-Processing.
 *   E.g. one schedcmd request may result in several pktchain response items.
 *
 * The default handler is hwa_callback_noop()
 *
 * HWA1a and HWA1b are both functional and so HWA1a ONLY is deprecated
 *
 * In HWA Packet Pager based interfaces, a callback is invoked in all H2S with
 * an in-place pointer to the received element. Packet Pager uses a fixed
 * 16 Byte structure for all requests and responses. Marshalling to/from a
 * Packet Pager formatted request or response is in the caller. H2S callbacks
 * are directly passed a pointer (in-place) to the ring element to be processed.
 * Caller (consumer) should NEVER modify the ring element contents (i.e. const).
 *
 * -----------------------------------------------------------------------------
 */
typedef enum hwa_callback
{
	                                        // Handlers Registered
#if !defined(HWA_PKTPGR_BUILD)
	                                        // HWA_RXPOST_WI_H2S
	HWA_RXPOST_PROC_CB  = 0,                //   ---hwa_callback_noop---
	HWA_RXPOST_DONE_CB  = 1,                //   ---hwa_callback_noop---
	                                        // HWA_RXFILL_RXFIFO_H2S
	HWA_RXFIFO_PROC_CB  = 2,                //   hwa_rxfill_bmac_recv()
	HWA_RXFIFO_DONE_CB  = 3,                //   hwa_rxfill_bmac_done()
	                                        // HWA_TXPOST_PKTCHAIN_H2S
	HWA_TXPOST_PROC_CB  = 4,                //   hwa_txpost_sendup()
	HWA_TXPOST_DONE_CB  = 5,                //   hwa_txpost_schedcmd_done()
	                                        // HWA_TXSTAT_QUEUE_H2S
	HWA_TXSTAT_PROC_CB  = 6,                //   hwa_txstat_bmac_proc()
	HWA_TXSTAT_DONE_CB  = 7,                //   hwa_txstat_bmac_done()

	HWA_CALLBACK_MAX    = 8

#else  /* HWA_PKTPGR_BUILD */

	// All response interface commands are packed to end of previous
	HWA_PKTPGR_PAGEIN_CALLBACK       = 0,   /* ref: hwa_pp_pagein_cmd_t */
	    HWA_PKTPGR_PAGEIN_RXPROCESS  =      /* hwa_pktpgr_pagein_rxprocess() */
	        (HWA_PKTPGR_PAGEIN_CALLBACK + HWA_PP_PAGEIN_RXPROCESS),
	    HWA_PKTPGR_PAGEIN_TXSTATUS   =      /* hwa_pktpgr_pagein_txstatus() */
	        (HWA_PKTPGR_PAGEIN_CALLBACK + HWA_PP_PAGEIN_TXSTATUS),
	    HWA_PKTPGR_PAGEIN_TXPOST     =      /* hwa_pp_pagein_cmd_txpost() */
		    (HWA_PKTPGR_PAGEIN_CALLBACK + HWA_PP_PAGEIN_TXPOST_WITEMS),
	    HWA_PKTPGR_PAGEIN_TXPOST_FRC =      /* hwa_pp_pagein_cmd_txpost() */
	        (HWA_PKTPGR_PAGEIN_CALLBACK + HWA_PP_PAGEIN_TXPOST_WITEMS_FRC),

	HWA_PKTPGR_PAGEOUT_CALLBACK      =      /* ref: hwa_pp_pageout_cmd_t */
	        (HWA_PP_PAGEIN_CMD_MAX),
	    HWA_PKTPGR_PAGEOUT_PKTLIST   =      /* hwa_pktpgr_pageout_pktlist() */
	        (HWA_PKTPGR_PAGEOUT_CALLBACK + HWA_PP_PAGEOUT_PKTLIST_WR),
	    HWA_PKTPGR_PAGEOUT_LOCAL     =      /* hwa_pktpgr_pageout_local() */
	        (HWA_PKTPGR_PAGEOUT_CALLBACK + HWA_PP_PAGEOUT_PKTLOCAL),

	HWA_PKTPGR_PAGEMGR_CALLBACK      =      /* ref: hwa_pp_pagemgr_cmd_t */
	        (HWA_PP_PAGEIN_CMD_MAX + HWA_PP_PAGEOUT_CMD_MAX),
	    HWA_PKTPGR_PAGEMGR_ALLOC_RX  =      /* hwa_pktpgr_pagemgr_alloc_rx() */
	        (HWA_PKTPGR_PAGEMGR_CALLBACK + HWA_PP_PAGEMGR_ALLOC_RX),
	    HWA_PKTPGR_PAGEMGR_ALLOC_RX_RPH =   /* hwa_pktpgr_pagemgr_alloc_rx_rph() */
	        (HWA_PKTPGR_PAGEMGR_CALLBACK + HWA_PP_PAGEMGR_ALLOC_RX_RPH),
	    HWA_PKTPGR_PAGEMGR_ALLOC_TX  =      /* hwa_pktpgr_pagemgr_alloc_tx() */
	        (HWA_PKTPGR_PAGEMGR_CALLBACK + HWA_PP_PAGEMGR_ALLOC_TX),
	    HWA_PKTPGR_PAGEMGR_PUSH      =      /* hwa_pktpgr_pagemgr_push() */
	        (HWA_PKTPGR_PAGEMGR_CALLBACK + HWA_PP_PAGEMGR_PUSH),
	    HWA_PKTPGR_PAGEMGR_PULL      =      /* hwa_pktpgr_pagemgr_pull() */
	        (HWA_PKTPGR_PAGEMGR_CALLBACK + HWA_PP_PAGEMGR_PULL),
	    HWA_PKTPGR_PAGEMGR_PUSH_PKTTAG =    /* hwa_pktpgr_pagemgr_push() */
	        (HWA_PKTPGR_PAGEMGR_CALLBACK + HWA_PP_PAGEMGR_PUSH_PKTTAG),
	    HWA_PKTPGR_PAGEMGR_PULL_KPFL_LINK = /* hwa_pktpgr_pagemgr_pull() */
	        (HWA_PKTPGR_PAGEMGR_CALLBACK + HWA_PP_PAGEMGR_PULL_KPFL_LINK),

	HWA_CALLBACK_MAX =                      /* all response commands */
	(HWA_PP_PAGEIN_CMD_MAX + HWA_PP_PAGEOUT_CMD_MAX + HWA_PP_PAGEMGR_CMD_MAX)
#endif /* HWA_PKTPGR_BUILD */

} hwa_callback_t;

// Application subsystems callback handlers
#if !defined(HWA_PKTPGR_BUILD)
typedef int (* hwa_callback_fn_t)(void *context, uintptr arg1, uintptr arg2,
                   uint32 arg3, uint32 arg4);
#else  /* HWA_PKTPGR_BUILD */
typedef int (* hwa_callback_fn_t)(void *context,
                   hwa_dev_t *dev, /* const */ hwa_pp_cmd_t *pp_cmd);
#endif /* HWA_PKTPGR_BUILD */

typedef struct hwa_handler {                // upstream subsystem's handler
	void              *context;             // opaque callback context
	hwa_callback_fn_t callback;             // callback function
} hwa_handler_t;

// Upstream subsystem register's its callback handler with HWA
void    hwa_register(hwa_dev_t *dev,
            hwa_callback_t cb, void *cb_ctx, hwa_callback_fn_t cb_fn);

/*
 * -----------------------------------------------------------------------------
 *
 * Section: HWA1a RxPost block
 *
 * Library of functions to manage HWA1a RxPost block
 * - HWA1a block bringup: attach, detach and init phases
 * - Support for H2S RxPost work item forwarding to upstream
 *   when HWA1b is disabled.
 * - Support for RPH allocations. [No use case presently]
 *
 * -----------------------------------------------------------------------------
 */
#if defined(HWA_RXPOST_BUILD)

// Depth of PCIe Full Dongle HWA1a RxPost to SW Interface
#define HWA_RXPOST_MSGBUF_DEPTH     512

// RxPost RD index update interrupt aggregation
#define HWA_RXPOST_INTRAGGR_COUNT   32
#define HWA_RXPOST_INTRAGGR_TMOUT   100 // usecs

struct hwa_rxpost;

// Handler for parsing RxPost WI formats: CWI32, CWI64, ACWI32 and ACWI64
typedef uint32 (* hwa_rxpost_wi_parser_fn_t)(struct hwa_rxpost *rxpost,
                      hwa_ring_t *rxpost_ring, uint32 elem_ix,
                      hwa_handler_t *rxpost_handler);

// RxPost WI configuration
typedef struct hwa_rxpost_config
{
	hwa_rxpost_wi_parser_fn_t wi_parser; // CWI32, CWI64, ACWI32 and ACWI64
	uint8 wi_format;       // compact or aggregated 32bit or 64bit haddrs
	uint8 wi_size;         // size of workitem
	uint8 len_offset;      // offset of Host RxBuffer length field in RxPost WI
	uint8 addr_offset;     // offset of Host RxBuffer address field in RxPost WI
	char wi_name[8];       // workitem format debug name
} hwa_rxpost_config_t;

typedef struct hwa_rxpost
{
	NO_HWA_PKTPGR_EXPR(bool pending_rph_req);
	NO_HWA_PKTPGR_EXPR(bool pre_rph_allocated);
	HWA_STATS_EXPR(uint32 rph_alloc_cnt);
	HWA_STATS_EXPR(uint32 rph_fails_cnt);

	const hwa_rxpost_config_t *config;

	hwa_mem_addr_t            rxpost_addr;    // AXI address of HWA1b RxPost memory
} hwa_rxpost_t;

int     hwa_rxpost_preinit(hwa_rxpost_t *rxpost);
void    hwa_rxpost_free(hwa_rxpost_t *rxpost);
int     hwa_rxpost_init(hwa_rxpost_t *rxpost);
int     hwa_rxpost_deinit(hwa_rxpost_t *rxpost);
void    hwa_rph_reclaim(hwa_rxpost_t *rxpost, uint32 core);

#if defined(BCMDBG) || defined(HWA_DUMP)
void    hwa_rxpost_dump(hwa_rxpost_t *rxpost, struct bcmstrbuf *b, bool verbose);
#endif // endif

#endif /* HWA_RXPOST_BUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: HWA1b RxFill block
 *
 * Library of functions to manage HWA1b RxFill block
 * - HWA1b block bringup: attach, detach and init phases
 * - Support for freeing Host RxBuffers for posting to RxFIFO(0)
 * - Support for freeing Dongle SysMem RxBuffers for posting to RxFIFO(1)
 * - Debug support: block level, traffic status, localfifo status
 *
 * HWA1b Terminology used:
 *  "FREEIDXSRC" refers to RXFREE S2H Interface. SW frees Simple or Paired
 *      RxBuffers by posting the RxBuffer's index into Rx BufferManager into
 *      the S2H Interface ring. "SRC" implies HWA1b is destination, i.e. S2H.
 *  "D11BDEST" refers to RXFIFO H2S Interface. For each RxBuffer posted to a
 *      MAC FIFO0, HWA1b will post the RxBuffer's index in the Rx BufferManager
 *      into the H2S Interface ring. SW uses this ring similar to how di->rxp
 *      is used in hnddma, to save a shadow array of pointers to the packets.
 *      "DEST" implies HWA1b is the source of this interface, i.e. H2S.
 *
 * PCIe Full Dongle Mode Notes:
 *  - MAC FIFO0 and FIFO1 are configure for SplitHdr PCIE Full Dongle
 *  - Single S2H RxBuffer (Simple/Paired) Index Interface "FREEIDXSRC"
 *  - Single H2S RXFIFO Interface "D11BDEST"
 *
 * NIC Mode:
 *  - MAC FIFO0 only is configured
 *  - Single S2H RxBuffer (Simple/Paired) Index Interface "FREEIDXSRC"
 *  - Single H2S RXFIFO Interface "D11BDEST"
 *
 * -----------------------------------------------------------------------------
 */
#if defined(HWA_RXFILL_BUILD)

// MAC FIFO0 and FIFO1 Configuration
#define HWA_RXFIFO_MAX                  2     // number of RxFIFO filled
#define HWA_RXFIFO_DESC_SIZE            sizeof(dma64dd_t)

// Number of Host RxBuffers to be reserved for SW
#if defined(HWA_PKTPGR_BUILD)
#define HWA_RPH_RESERVE_COUNT           0
#else
#define HWA_RPH_RESERVE_COUNT           32
#endif // endif

// S2H Free RxBuf Index ring aka "FREEIDXSRC"
#define HWA_RXFILL_RXFREE_DEPTH         512

// Interrupt aggregation specification for H2S and S2H ring interfaces
#define HWA_RXFILL_RING_INTRAGGR_COUNT  32
#define HWA_RXFILL_RING_INTRAGGR_TMOUT  200   // usecs

// Interrupt aggregation specification for MAC FIFOs
#define HWA_RXFILL_FIFO_INTRAGGR_COUNT  16
#define HWA_RXFILL_FIFO_INTRAGGR_TMOUT  100   // usecs

#define HWA_RXFILL_FIFO_MIN_THRESHOLD   256   // min FIFO fill threshold

// Alert software is FIFO depth stays below threshold for duration
#define HWA_RXFILL_FIFO_ALERT_THRESHOLD 64
#define HWA_RXFILL_FIFO_ALERT_DURATION  1000  // 1 millisec

// Lazy update of the RxFIFO RD Index
#define HWA_RXFILL_LAZY_RD_UPDATE       64

typedef struct hwa_rxfill_config
{
	uint8 rph_size;         // Size of RxPost Host Info
	uint8 wrxh_offset;      // rph_size + 8B alignment pad for OFFSETOF(wlc_d11rxhdr_t, rxhdr)
	uint8 d11_offset;       // OFFSETOF(wlc_d11rxhdr_t, rxhdr)
	uint8 len_offset;       // offset of length field in RPH = ~0 in HWA-2.0
	uint8 addr_offset;      // offset of data_buf_haddr 32|64 field in RPH
	uint16 rx_size;	        // maximum rx buffer size to save rxhdr
} hwa_rxfill_config_t;

typedef struct hwa_rxfill
{
	uintptr     rxbm_base;  // loaddr of the Rx Buffer Manager

	hwa_ring_t  rxfifo_ring[HWA_RX_CORES_MAX]; // H2S interface "D11BDEST"
	hwa_ring_t  rxfree_ring[HWA_RX_CORES_MAX]; // S2H interface "FREEIDXSRC"

	HWA_STATS_EXPR(uint32 rxfree_cnt[HWA_RX_CORES_MAX]);
	HWA_STATS_EXPR(uint32 rxfifo_cnt[HWA_RX_CORES_MAX]);

	// MAC RxFIFOs configuration for HWA1b
	void        *wlc[HWA_RX_CORES_MAX];
	dma64addr_t fifo_addr[HWA_RX_CORES_MAX][HWA_RXFIFO_MAX];
	uint32      fifo_depth[HWA_RX_CORES_MAX][HWA_RXFIFO_MAX];
	uint32      dmarcv_ptr[HWA_RX_CORES_MAX][HWA_RXFIFO_MAX];
	bool        inited[HWA_RX_CORES_MAX][HWA_RXFIFO_MAX];
	bool        hme_macifs[HWA_RX_CORES_MAX][HWA_RXFIFO_MAX];

	hwa_rxfill_config_t config;

	void        *rx_head;
	void        *rx_tail;

#ifdef HWA_RXFILL_RXFREE_AUDIT_ENABLED
	// rxfree audit
	struct bcm_mwbmap *rxfree_map;
#endif // endif
	uint32      tsf_l; // TSF Low counter
	uint16      rxp_data_buf_len;    // A copy from pcie_ipc_rings

#if defined(HWA_PKTPGR_BUILD)
	// HWA_PKTPGR_D11B_AUDIT_ENABLED or 6715A0 SW WAR
	bcm_ring_t  d11b_audit_state;    // SW context: read and write state
	void        *d11b_audit_table;   // memory for ring
	uint16      d11b_audit_depth;    // ring depth: num elements in ring
	uint16      d11b_audit_dirty;    // D11B:rd to D11B:wr

	// PageIn Rx recv histogram
	HWA_BCMDBG_EXPR(uint32 rx_recv_histogram[HWA_PGI_RX_RECV_HISTOGRAM_MAX]);
#endif // endif
} hwa_rxfill_t;

hwa_rxfill_t *BCMATTACHFN(hwa_rxfill_attach)(hwa_dev_t *dev);
void    BCMATTACHFN(hwa_rxfill_detach)(hwa_rxfill_t *rxfill);
int     hwa_rxfill_preinit(hwa_rxfill_t *rxfill);
void    hwa_rxfill_free(hwa_rxfill_t *rxfill);

int     hwa_rxfill_init(hwa_rxfill_t *rxfill);
int     hwa_rxfill_deinit(hwa_rxfill_t *rxfill);

// Reclaim all RxBuffers in RxBM
void    hwa_rxfill_rxbuffer_reclaim(hwa_dev_t *dev, uint32 core);

// Process H2S RxFIFO interface WR index update interrupt from MAC, see: HWA2a
int     hwa_rxfill_rxbuffer_process(hwa_dev_t *dev, uint32 core, bool bound);

// Return the number of available RxBuffers for reception in the RX FIFOs
uint32  hwa_rxfill_fifo_avail(hwa_rxfill_t *rxfill, uint32 core);

#if defined(BCMDBG) || defined(HWA_DUMP)
// Debug support for HWA1b block
void    hwa_rxfill_dump(hwa_rxfill_t *rxfill, struct bcmstrbuf *b, bool verbose);
#if defined(WLTEST) || defined(HWA_DUMP)
// Debug dump of various transfer status
#define HWA_RXFILL_TFRSTATUS_DEFINE(mgr) \
void hwa_rxfill_##mgr##_tfrstatus(hwa_rxfill_t *rxfill, uint32 core)
HWA_RXFILL_TFRSTATUS_DEFINE(rxpmgr);    // hwa_rxfill_rxpmgr_tfrstatus
HWA_RXFILL_TFRSTATUS_DEFINE(d0mgr);     // hwa_rxfill_d0mgr_tfrstatus
HWA_RXFILL_TFRSTATUS_DEFINE(d1mgr);     // hwa_rxfill_d1mgr_tfrstatus
HWA_RXFILL_TFRSTATUS_DEFINE(d11bmgr);   // hwa_rxfill_d11bmgr_tfrstatus
HWA_RXFILL_TFRSTATUS_DEFINE(freeidxmgr); // hwa_rxfill_freeidxmgr_tfrstatus

// Debug dump of various localfifo configurations
#define HWA_RXFILL_LOCALFIFO_STATUS_DEFINE(mgr) \
void hwa_rxfill_##mgr##_localfifo_status(hwa_rxfill_t *rxfill, uint32 core)
HWA_RXFILL_LOCALFIFO_STATUS_DEFINE(rxp);
HWA_RXFILL_LOCALFIFO_STATUS_DEFINE(d0);
HWA_RXFILL_LOCALFIFO_STATUS_DEFINE(d1);
HWA_RXFILL_LOCALFIFO_STATUS_DEFINE(d11b);
HWA_RXFILL_LOCALFIFO_STATUS_DEFINE(freeidx);

// Dump the current status of RxFILL
void    hwa_rxfill_status(hwa_rxfill_t *rxfill, uint32 core);
#endif // endif
#endif /* BCMDBG */

#endif /* HWA_RXFILL_BUILD */

#if defined(HWA_RXPATH_BUILD)
/*
 * -----------------------------------------------------------------------------
 *
 * Section: HWA RxPATH Blocks 1a and 1b related common functions
 *
 * - Manage various RxPath blocks: 1a and 1b.
 *
 * -----------------------------------------------------------------------------
 */

/* XXX, CRBCAHWA-558, CRBCAHWA-617
 * 1. SW WAR: set HWA_RXFILL_MIN_FETCH_THRESH_FREEIDX to <= 1
 *    can avoid error in RX free in PAIR.
 * 2. HWA Revision >= 130 fix CRBCAHWA-558, CRBCAHWA-617
 * 3. Although HWA Revision 129 + SW WAR, or HWA Revision 130
 *    have fix it, we still cannot free as HWA_RXFILL_RXFREE_PAIRED because
 *    dongle need to free all RxPost resources to Host as possible.
 * 4. In PKTPGR plaform we are able to free it as HWA_RXFILL_RXFREE_PAIRED
 *    since we have new Rx recycle process.
 */
#define HWA_RXFILL_MIN_FETCH_THRESH_RXP     3 // 2^^3 = 8 ???
#define HWA_RXFILL_MIN_FETCH_THRESH_FREEIDX 3 // 2^^3 = 8 ???

typedef struct hwa_rxpath
{
	hwa_rxpath_stats_t stats[HWA_RX_CORES]; // Common block stats 1a + 1b
} hwa_rxpath_t;

// RxPath block level management for HWA1x blocks 1a + 1b
void    BCMATTACHFN(hwa_rxpath_detach)(hwa_rxpath_t *rxpath);
hwa_rxpath_t *BCMATTACHFN(hwa_rxpath_attach)(hwa_dev_t *dev);

// Add rxcple workitems to pciedev directly.
int     hwa_rxpath_queue_rxcomplete_fast(hwa_dev_t *dev, uint32 pktid);
// Xmit rxcple workitems from pciedev to host
void    hwa_rxpath_xmit_rxcomplete_fast(hwa_dev_t *dev);
// Flush all rxcpl workitmes from pciedev to host
void    hwa_rxpath_flush_rxcomplete(hwa_dev_t *dev);

// Common block level statistics per core for HWA1x blocks 1a + 1b
void    hwa_rxpath_stats_clear(hwa_rxpath_t *rxpath, uint32 core);
void    hwa_rxpath_stats_dump(hwa_rxpath_t *rxpath, struct bcmstrbuf *b, uint8 clear_on_copy);

// Determine if any Rx path block HWA1a or HWA1b has an error
int     hwa_rxpath_error(hwa_dev_t *dev, uint32 core); // return 0 = no error

#if defined(BCMPCIE_IPC_HPA)
// A HWA wrap function to Test a PktId on entry and exit from dongle
void    hwa_rxpath_hpa_req_test(hwa_dev_t *dev, uint32 pktid);
#endif // endif

#if defined(BCMDBG) || defined(HWA_DUMP)
// Debug support for HWA1x blocks 1a + 1b
void    hwa_rxpath_dump(hwa_rxpath_t *rxpath, struct bcmstrbuf *b, bool verbose, bool dump_regs);
#if defined(WLTEST) || defined(HWA_DUMP)
void    hwa_rxpath_regs_dump(hwa_rxpath_t *rxpath, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */

#endif /* HWA_RXPATH_BUILD */

/*
 * -----------------------------------------------------------------------------
 *
 * Section: HWA2a RxData block
 *
 * - Management of HWA2a Filter Hwardware Resource (FHR) block in the MAC
 *   - Constructing filters and data filling up parameter list
 *   - Addition of filters to FHR
 *   - Deletion of filters in in FHR
 * - Management of FHR statistics
 *
 * -----------------------------------------------------------------------------
 */
#if defined(HWA_RXDATA_BUILD)
typedef enum hwa_rxdata_fhr_filter_type
{
	HWA_RXDATA_FHR_FILTER_DISABLED = 0,
	HWA_RXDATA_FHR_PKTFETCH = 1,              // Full Dongle host packet fetch
	HWA_RXDATA_FHR_L2FILTER = 2,              // L2Filter packet drop
	HWA_RXDATA_FHR_LLC_SNAP_DA = 3,
	HWA_RXDATA_FHR_FILTER_TYPE_MAX = 4
} hwa_rxdata_fhr_filter_type_t;

typedef struct hwa_rxdata_fhr_filter hwa_rxdata_fhr_filter_t;
struct hwa_rxdata_fhr_filter
{
	hwa_rxdata_fhr_filter_t    *next;         // single linked list
	hwa_rxdata_fhr_entry_t     filter;        // FHR filter entry in HWA2a
};

typedef struct hwa_rxdata
{
	uint32                     mac_fhr_base;  // AXI access of FHR regfile
	uint32                     mac_fhr_stats; // AXI access of FHR stats

	uint32                     fhr_pktfetch;  // enabled pktfetch filter bitmap
	uint32                     fhr_l2filter;  // enabled l2filter filter bitmap
	uint32                     llc_snap_da_filter;  // enabled l2filter filter bitmap
	uint32                     udpv6_filter;   // l2filter filter bitmap for UDPv6
	uint32                     udpv4_filter;  // l2filter filter bitmap for UDPv4
	uint32                     tcp_filter;    // l2filter filter bitmap for TCP
	uint32                     chainable_filters;   // chainable filter bitmap

	uint32                     rxfilteren;    // Enabled filters in HWA2a

	uint32                     param_count;   // index of next build parameter
	hwa_rxdata_fhr_filter_t    *fhr_build;    // current FHR filter being built
	hwa_rxdata_fhr_filter_t    *fhr_flist;    // free list of filters

	hwa_rxdata_fhr_filter_t    fhr[HWA_RXDATA_FHR_FILTERS_MAX];

	HWA_STATS_EXPR(uint32      fhr_ins_cnt);  // number of FHR filter adds
	HWA_STATS_EXPR(uint32      fhr_del_cnt);  // number of FHR filter deletes
	HWA_STATS_EXPR(uint32      fhr_err_cnt);  // number of filter failures

#ifdef WLNDOE
	uint32                     ndoe_filter;   // pktfetch filter bitmap for NDOE
#endif // endif
#if defined(ICMP)
	uint32                     ip_filter;     // pktfetch filter bitmap for IP packet
#endif // endif
#ifdef WL_TBOW
	uint32                     tbow_filter;   // pktfetch filter bitmap for TBOW
#endif // endif
} hwa_rxdata_t;

void    BCMATTACHFN(hwa_rxdata_detach)(hwa_rxdata_t *rxdata);
hwa_rxdata_t *BCMATTACHFN(hwa_rxdata_attach)(hwa_dev_t *dev);
void    hwa_rxdata_init(hwa_rxdata_t *rxdata);
void    hwa_rxdata_deinit(hwa_rxdata_t *rxdata);

// HWA2a FHR Table Management

// Init exist known pktfetch filter
void    hwa_rxdata_fhr_filter_init_pktfetch(hwa_rxdata_t *rxdata);

// Reinit exist known filter
void    hwa_rxdata_fhr_filter_reinit(hwa_rxdata_t *rxdata);

// Allocate a new filter and start building parameters before configuring HWA2a
int     hwa_rxdata_fhr_filter_new(hwa_rxdata_fhr_filter_type_t filter_type,
            uint32 filter_polarity, uint32 param_count);

// Build the filter by specifying parameters, and finally configure in HWA2a FHR
int     hwa_rxdata_fhr_param_add(uint32 filter_id,
            uint32 polarity, uint32 offset,
            uint8 *bitmask, uint8 *pattern, uint32 match_sz);

// Add the built filter into MAC FHR register file
int     hwa_rxdata_fhr_filter_add(uint32 filter_id);

// Delete a previously configured FHR filter
int     hwa_rxdata_fhr_filter_del(uint32 filter_id);

#ifdef WLNDOE
void    hwa_rxdata_fhr_filter_ndoe(bool enable);
#endif // endif
#if defined(ICMP)
void    hwa_rxdata_fhr_filter_ip(bool enable);
#endif // endif
#ifdef WL_TBOW
void    hwa_rxdata_fhr_filter_tbow(bool enable);
#endif // endif

// FHR Filter Match Statistics Management

// Get the filter hits statistics
uint32  hwa_rxdata_fhr_hits_get(uint32 filter_id);

// Clear one or all filter hits statistics
void    hwa_rxdata_fhr_hits_clr(uint32 filter_id); // ALL: filter_id = ~0U

// HWA2a Debug Support
#if defined(BCMDBG) || defined(HWA_DUMP)
void    hwa_rxdata_fhr_dump(hwa_rxdata_t *rxdata, struct bcmstrbuf *b, bool verbose);
void    hwa_rxdata_dump(hwa_rxdata_t *rxdata, struct bcmstrbuf *b, bool verbose);
#endif /* BCMDBG */

#endif /* HWA_RXDATA_BUILD */

/*
 * -----------------------------------------------------------------------------
 *
 * Section: HWA3a TxPost block
 *
 * Library of functions to manage the HWA3a TxPost block.
 * - HWA3a block bringup: attach, detach and init phases
 * - Support for requesting HWA3a to DMA RD indices
 * - Support for posting Schedule Commands in SchedCmd S2H hwa_ring
 * - Support for retrieving Packet Chains from PktChain H2S hwa_ring
 *   Transaction ID between SchedCmd and PktChain responses are used to audit
 *   that HWA3a does indeed transfer the required number of TxPost work items.
 * - Flow Ring Configuration [FRC] management
 * - Per Interface Priority based FlowId Lookup Table management
 * - Unique SADA lookup table management
 * - FlowId Hash based Lookup Table management
 * - Support for HWA3a block common and FRC statistics registers
 * - Support for debug - SW state and HWA3a registers
 *
 * -----------------------------------------------------------------------------
 */
#if defined(HWA_TXPOST_BUILD)

#if defined(HWA_PKTPGR_BUILD)
// Max value of uint8 is 256
#define HWA_TXPOST_SCHEDCMD_RING_DEPTH  256
#define HWA_TXPOST_PKTPGR_TRANS_ID_INVALID  (0xFFFF)
#else
// Depth of S2H schedule command interface to HWA3a
#define HWA_TXPOST_SCHEDCMD_RING_DEPTH  256
#endif /* HWA_PKTPGR_BUILD */

// Depth of H2S packet chain interface from HWA3a
#define HWA_TXPOST_PKTCHAIN_RING_DEPTH  1024

// Lazy Interrupt configuration for SchedCmd Ring.
#define HWA_TXPOST_SCHEDCMD_RING_LAZYCOUNT  1 // disable via interrupt mask
#define HWA_TXPOST_SCHEDCMD_RING_LAZYTMOUT  0

// Maximum transfer count per request
#define HWA_TXPOST_SCHEDCMD_TRANSFER_COUNT  128

// Lazy Interrupt configuration for PktChain Ring. Immediate interrupt.
#define HWA_TXPOST_PKTCHAIN_RING_LAZYCOUNT  1 // every pktchain response
#define HWA_TXPOST_PKTCHAIN_RING_LAZYTMOUT  0 // immediate

// SW uses an asynchronous RD index update, and polls for previous update done
#define HWA_TXPOST_RDIDX_DMA_BUSY_BURNLOOP  256 // burnloop until DMA finishes

// SW may only confirm that a schedcmd has completed on a trans_id change
#define HWA_SCHEDCMD_CONFIRMED_DONE         1 // trans_id has changed
#define HWA_SCHEDCMD_POTENTIALLY_DONE       0 // trans_id has not yet changed

#define HWA_TXPOST_PRIO_CFG_MAX             32
#define HWA_TXPOST_SADA_CFG_MAX             256

#define HWA_TXPOST_FLOW_LUT_COLL_DEPTH      8 // Flow LUT collision list depth

// S2H TxFree Queue Interface interrupt aggregation
#define HWA_TXPOST_RING_INTRAGGR_COUNT      4
#define HWA_TXPOST_RING_INTRAGGR_TMOUT      200   // usecs

#define HWA_TXPOST_MIN_FETCH_THRESH_FREEIDX 2 // 2^^2 = 4 ???

// S2H TX Free TxBuf Index ring aka "FREEIDXTX"
#define HWA_TXPOST_TXFREE_DEPTH             1024

// Flow Ring Configuration customization
typedef enum hwa_txpost_frc_field
{
	HWA_TXPOST_FRC_SRS_IDX = 1,             // bind a Statistics Set
	HWA_TXPOST_FRC_FLOWID = 2,
	HWA_TXPOST_FRC_LKUP_OVERRIDE = 3,
	HWA_TXPOST_FRC_PYLD_MIN_LENGTH = 4,
	HWA_TXPOST_FRC_LKUP_TYPE = 5,           // DA, SA, DA, prio
	HWA_TXPOST_FRC_ENABLES = 6,             // etype_ip_enable, audit_enable
} hwa_txpost_frc_field_t;

#if !defined(HWA_NO_LUT)
// SW maintains sada entries in single linked list
typedef struct hwa_txpost_sada          // extends hwa_txpost_sada_lut_elem_t
{
	struct hwa_txpost_sada *next;       // sll free and sll active
	hwa_txpost_sada_lut_elem_t elem;    // actual element
} hwa_txpost_sada_t;

// SW SADA lookup table
typedef struct hwa_txpost_sada_swt
{
	hwa_txpost_sada_t table[HWA_TXPOST_SADA_LUT_DEPTH];
} hwa_txpost_sada_swt_t;

// SW maintains flow entries as a single linked list
typedef struct hwa_txpost_flow          // extends hwa_txpost_flow_lut_elem_t
{
	struct hwa_txpost_flow *next;       // sll free and sll active
	union {
		uint32 u32;
		struct {
			uint16 flowid;
			uint8  ifid;
			uint8  prio;
		};
	};
} hwa_txpost_flow_t;

// SW Flow lookup table
typedef struct hwa_txpost_flow_swt
{
	hwa_txpost_flow_t table[HWA_TXPOST_FLOW_LUT_DEPTH];
} hwa_txpost_flow_swt_t;
#endif /* !HWA_NO_LUT */

typedef struct hwa_txpost               // HWA3a TxPost state
{
	// Interfaces to/from HWA3a
	hwa_ring_t              schedcmd_ring;  // S2H Schedule Command ring context
	hwa_ring_t              pktchain_ring;  // H2S Packet Chain ring context

	// Runtime state
	uint8                   schedcmd_id;    // current schedcmd transaction id
	uint8                   pktchain_id;    // current pktchain transaction id

	// flowing id recoder per schedcmd id
	uint16                  flowring_id[HWA_TXPOST_SCHEDCMD_RING_DEPTH];

	// HWA fetch request flags per schedcmd id
	uint8                   schedule_flags[HWA_TXPOST_SCHEDCMD_RING_DEPTH];

#if defined(HWA_PKTPGR_BUILD)
	uint16                  pktpgr_trans_id[HWA_TXPOST_SCHEDCMD_RING_DEPTH];
	uint8                   *frc_bitmap;
	// Audit HWA32 schedcmd and pktchain WI transfer
	uint8                   wi_count[HWA_TXPOST_SCHEDCMD_RING_DEPTH];
#else
	// Audit HWA32 schedcmd and pktchain WI transfer
	HWA_DEBUG_EXPR(uint8    wi_count[HWA_TXPOST_SCHEDCMD_RING_DEPTH]);
#endif /* HWA_PKTPGR_BUILD */
	HWA_STATS_EXPR(uint32   pkt_proc_cnt);  // total packets processed
	HWA_STATS_EXPR(uint32   oct_proc_cnt);  // total octets processed

	// Flow Ring Contexts(FRC)
	hwa_txpost_frc_t        *frc_table;     // FlowRing Contexts in SysMem

#if !defined(HWA_NO_LUT)
	// Lookup Tables - SW state in SysMem
	hwa_txpost_prio_lut_t   prio_lut;       // per if priority LUT in AXI mem
	uint32                  prio_cfg[HWA_TXPOST_PRIO_CFG_MAX / NBU32];

	hwa_txpost_sada_swt_t   sada_swt;       // unique SADA SW table
	hwa_txpost_sada_t       *sada_alist;    // active list of entries in table
	hwa_txpost_sada_t       *sada_flist;    // free list of entries in table
	uint32                  sada_cfg[HWA_TXPOST_SADA_CFG_MAX / NBU32];
	HWA_STATS_EXPR(uint32   sada_ins_cnt);  // number of inserts
	HWA_STATS_EXPR(uint32   sada_del_cnt);  // number of deletes
	HWA_STATS_EXPR(uint32   sada_err_cnt);  // number of failures

	hwa_txpost_flow_swt_t   flow_swt;       // flow LUT in AXI mem
	hwa_txpost_flow_t       *flow_flist;    // flow entry free list
	uint32                  flow_collsz[HWA_TXPOST_SADA_CFG_MAX];
	HWA_STATS_EXPR(uint32   flow_ins_cnt);  // number of inserts
	HWA_STATS_EXPR(uint32   flow_del_cnt);  // number of deletes
	HWA_STATS_EXPR(uint32   flow_err_cnt);  // number of failures

	// HWA3a AXI memory addresses
	hwa_mem_addr_t          prio_addr;      // AXI address of HWA3a prio_lut
	hwa_mem_addr_t          sada_addr;      // AXI address of HWA3a sada_lut
	hwa_mem_addr_t          flow_addr;      // AXI address of HWA3a flow_lut
#endif /* !HWA_NO_LUT */

	// HWA3a Block statistics
	hwa_txpost_stats_t      stats;          // block level statistics

	// FRC Statistics Register Set(SRS)
	hwa_txpost_frc_srs_t    frc_srs[HWA_TXPOST_FRC_SRS_MAX];

	uint8                   loopcnt_hwm;    // Tx BM transaction loopcnt high wm
	uint8                   wi_size;        // size of a TxPost WI
	uint8                   aggr_spec;      // aggregation depth 4b and mode 1b

	hwa_ring_t              txfree_ring;    // S2H interface "FREEIDXTX"
	HWA_STATS_EXPR(uint32   txfree_cnt);

	// HWA3a schedule command histogram
	HWA_BCMDBG_EXPR(uint32 schecmd_histogram[HWA_TXPOST_HISTOGRAM_MAX]);
} hwa_txpost_t;

#define TXPOST_SCHED_FLAGS(txpost, cmd_id)	((txpost)->schedule_flags[(cmd_id)])

#define TXPOST_RESP_PEND_FLAGS_ISSET(flags)	((flags) & TXPOST_SCHED_FLAGS_RESP_PEND_MASK)
#define TXPOST_RESP_PEND_SET(txpost, cmd_id) \
	(TXPOST_SCHED_FLAGS(txpost, cmd_id) |= TXPOST_SCHED_FLAGS_RESP_PEND_MASK)

#define TXPOST_RESP_PEND_ISSET(txpost, cmd_id) \
	(TXPOST_RESP_PEND_FLAGS_ISSET(TXPOST_SCHED_FLAGS((txpost), (cmd_id))))

#define TXPOST_SCHED_FLAGS_SQS_FORCE_ISSET(flags) \
	((flags) & TXPOST_SCHED_FLAGS_SQS_FORCE_MASK)
#define TXPOST_SCHED_FLAGS_SQS_FORCE_SET(txpost, cmd_id) \
	(TXPOST_SCHED_FLAGS(txpost, cmd_id) |= TXPOST_SCHED_FLAGS_SQS_FORCE_MASK)

#define TXPOST_SCHED_FLAGS_SQS_ISSET(txpost, cmd_id) \
	(TXPOST_SCHED_FLAGS_SQS_FORCE_ISSET(TXPOST_SCHED_FLAGS((txpost), (cmd_id))))

// HWA3a block level management
void    BCMATTACHFN(hwa_txpost_detach)(hwa_txpost_t *txpost);
hwa_txpost_t *BCMATTACHFN(hwa_txpost_attach)(hwa_dev_t *dev);
void    hwa_txpost_init(hwa_txpost_t *txpost);
void    hwa_txpost_deinit(hwa_txpost_t *txpost);
void    hwa_txpost_wait_to_finish(hwa_txpost_t *txpost);

// Request HWA3a to DMA transfer RD indices to Host
void    hwa_txpost_rdidx_update(hwa_txpost_t *txpost,
            uint32 offset, uint32 count); // transfer a set of RD indices
void    hwa_txpost_flowring_rdidx_update(hwa_txpost_t *txpost,
            uint32 ring_id); // transfer a single flowring's RD index

// Process one or more packet chains responses from HWA3a to schedule requests
int     hwa_txpost_pktchain_process(hwa_dev_t *dev);

// HWA3a FRC Management
void    hwa_txpost_frc_table_init(hwa_txpost_t *txpost);
void    hwa_txpost_frc_table_customize(hwa_txpost_t *txpost, uint32 ring_id,
            hwa_txpost_frc_field_t field, uint32 arg1, uint32 arg2);

#if !defined(HWA_NO_LUT)
// HWA3a Priority LUT Management
void    hwa_txpost_prio_lut_init(hwa_txpost_t *txpost);
void    hwa_txpost_prio_lut_config(hwa_txpost_t *txpost,
            uint8 ifid, const uint16 *flowid_lut);
void    hwa_txpost_prio_lut_reset(hwa_txpost_t *txpost, uint8 ifid);

// HWA3a SADA LUT Management
void    hwa_txpost_sada_swt_init(hwa_txpost_t *txpost);
int     hwa_txpost_sada_swt_find(hwa_txpost_t *txpost, const uint16 *ea);
int     hwa_txpost_sada_swt_insert(hwa_txpost_t *txpost, const uint16 *ea);
int     hwa_txpost_sada_swt_delete(hwa_txpost_t *txpost, const uint16 *ea);
void    hwa_txpost_sada_swt_audit(hwa_txpost_t *txpost);
void    hwa_txpost_sada_lut_config(hwa_txpost_t *txpost,
            int sada_idx, bool insert);

// HWA3a Flow LUT Management
void    hwa_txpost_flow_swt_init(hwa_txpost_t *txpost);
int     hwa_txpost_flow_lut_insert(hwa_txpost_t *txpost,
            const uint16 *ea, uint8 ifid, uint8 prio, uint16 flowid);
int     hwa_txpost_flow_lut_delete(hwa_txpost_t *txpost,
            const uint16 *ea, uint8 ifid, uint8 prio, uint16 flowid);
void    hwa_txpost_flow_lut_reset(hwa_txpost_t *txpost, const uint16 *ea);
void    hwa_txpost_flow_lut_config(hwa_txpost_t *txpost,
            const hwa_txpost_flow_t *prev, const hwa_txpost_flow_t *flow,
            int sada_idx);
#endif /* !HWA_NO_LUT */

// HWA3a Statistics Register Set management ... set_idx [0 .. 15]
void    hwa_txpost_stats_clear(hwa_txpost_t *txpost, uint32 set_idx);
void    hwa_txpost_stats_dump(hwa_txpost_t *txpost, struct bcmstrbuf *b,
            uint32 set_idx, uint8 clear_on_copy);

// HWA3a Debug Support
#if defined(BCMDBG) || defined(HWA_DUMP)
// Use local SW shadowed state, to dump HWA3a AXI lookup tables
void    hwa_txpost_frc_table_dump(hwa_txpost_t *txpost, struct bcmstrbuf *b, bool verbose);
#if !defined(HWA_NO_LUT)
void    hwa_txpost_prio_lut_dump(hwa_txpost_t *txpost, struct bcmstrbuf *b, bool verbose);
void    hwa_txpost_sada_lut_dump(hwa_txpost_t *txpost, struct bcmstrbuf *b, bool verbose);
void    hwa_txpost_flow_lut_dump(hwa_txpost_t *txpost, struct bcmstrbuf *b, bool verbose);
#endif /* !HWA_NO_LUT */

// Dump all SW and HWA3a state
void    hwa_txpost_dump(hwa_txpost_t *txpost, struct bcmstrbuf *b, bool verbose, bool dump_regs);
#if defined(WLTEST) || defined(HWA_DUMP)
// Dump HWA3a registers
void    hwa_txpost_regs_dump(hwa_txpost_t *txpost, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */

void    hwa_txpost_bm_lb_init(hwa_dev_t *dev, void *memory, uint16 pkt_total,
		uint16 pkt_size, uint16 hw_size);

#endif /* HWA_TXPOST_BUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: HWA3b TxFIFO block
 * -----------------------------------------------------------------------------
 */
#if defined(HWA_TXFIFO_BUILD)

// Max number of TxFIFOs supported by SW driver
#define HWA_TX_FIFOS_MAX                128
#if (HWA_TX_FIFOS > HWA_TX_FIFOS_MAX)
#error "TxFIFO support SW limit"
#endif // endif

// Depth of S2H packet chain transmit interface to HWA3b
#define HWA_TXFIFO_PKTCHAIN_RING_DEPTH  64

#define HWA_TXFIFO_PKTCNT_THRESHOLD     0
#define HWA_TXFIFO_AGGR_AQM_DESC_THRESHOLD 3

typedef struct hwa_txfifo
{
	hwa_ring_t              pktchain_ring;  // S2H Packet Chain ring context
	uint32                  pktchain_hiaddr; // hiaddr for pktchain head tail
	uint8                   pktchain_id;    // Pktchain xmit request id
	uint8                   pktchain_fmt;   // 32bit or 64bit pktchain head/tail
	uint8                   fifo_config;    // total TxFIFOs configured
	uint8                   fifo_total;     // total TxFIFOs supported
	uint32                  fifo_enab[HWA_TX_FIFOS_MAX / NBU32]; // bitmap

	// MAC TxFIFOs configuration for HWA3b
	dma64addr_t             fifo_base[HWA_TX_FIFOS_MAX];
	uint32                  fifo_depth[HWA_TX_FIFOS_MAX];
	dma64addr_t             aqm_fifo_base[HWA_TX_FIFOS_MAX];
	uint32                  aqm_fifo_depth[HWA_TX_FIFOS_MAX];
	bool                    hme_macifs[HWA_TX_FIFOS_MAX];

	HWA_STATS_EXPR(uint32   pktchain_cnt);  // count of pktchain xfer requests
	HWA_STATS_EXPR(uint32   pkt_xmit_cnt);  // count of total packets xmitted
	HWA_STATS_EXPR(uint32   tx_desc_cnt);   // count of total descr programmed
	HWA_STATS_EXPR(uint32   req_cnt[HWA_TX_FIFOS]); // total requests per FIFO

	// HWA3a AXI memory addresses
	hwa_mem_addr_t          ovflwq_addr;    // AXI address of HWA3b overflowq
	hwa_mem_addr_t          txfifo_addr;    // AXI address of HWA3b AQM, TXFIFOs

	hwa_txfifo_stats_t      stats;          // TBD statistics per core

	void                    *txfifo_shadow;  // TxFIFOs shadow context
} hwa_txfifo_t;

// HWA3b block level management
void    BCMATTACHFN(hwa_txfifo_detach)(hwa_txfifo_t *txfifo);
hwa_txfifo_t *BCMATTACHFN(hwa_txfifo_attach)(hwa_dev_t *dev);
void    hwa_txfifo_init(hwa_txfifo_t *txfifo);
void    hwa_txfifo_deinit(hwa_txfifo_t *txfifo);

// Statistics management
void    hwa_txfifo_stats_clear(hwa_txfifo_t *txfifo, uint32 core);
void    hwa_txfifo_stats_dump(hwa_txfifo_t *txfifo, struct bcmstrbuf *b, uint8 clear_on_copy);

// HWA3b Debug Support
#if defined(BCMDBG) || defined(HWA_DUMP)
// Dump HWA3b state
void    hwa_txfifo_state(hwa_dev_t *dev);
// Dump all SW and HWA3b state
void    hwa_txfifo_dump(hwa_txfifo_t *txfifo, struct bcmstrbuf *b, bool verbose, bool dump_regs,
            bool dump_txfifo_shadow, uint8 *fifo_bitmap);

#if defined(WLTEST) || defined(HWA_DUMP)
// Dump HWA3b registers
void    hwa_txfifo_regs_dump(hwa_txfifo_t *txfifo, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */

// Get Overflow Queue Context
int     hwa_txfifo_get_ovfq(hwa_dev_t *dev, uint32 fifo_idx, hwa_txfifo_ovflwqctx_t *ovflwq);

#endif /* HWA_TXFIFO_BUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: HWA4a TxSTAT block
 * -----------------------------------------------------------------------------
 */
#if defined(HWA_TXSTAT_BUILD)

// H2S TxStatus Queue Interface depth
#define HWA_TXSTAT_QUEUE_DEPTH          1024

// H2S TxStatus Queue Interface interrupt aggregation
//#define HWA_TXSTAT_INTRAGGR_COUNT       32
//#define HWA_TXSTAT_INTRAGGR_TMOUT       100 // usecs
// No interrupt coalescing to reduce latency by default
#define HWA_TXSTAT_INTRAGGR_COUNT       1
#define HWA_TXSTAT_INTRAGGR_TMOUT       0 // usecs

#define HWA_TXSTAT_RING_ELEM_SIZE       32 // 32 Bytes per TxStatus

// Read a 16 byte status package from the TxStatus fifo registers
#define HWA_TXSTAT_PKG_SIZE             16

// Lazy update of the RxFIFO RD Index
#define HWA_TXSTAT_LAZY_RD_UPDATE       128

typedef struct hwa_txstat_status
{
	uint8 u8[HWA_TXSTAT_RING_ELEM_SIZE];
} hwa_txstat_status_t;

typedef struct hwa_txstat
{
	uint32      status_size[HWA_TX_CORES_MAX]; // Size of a TxStatus
	hwa_ring_t  status_ring[HWA_TX_CORES_MAX]; // H2S TxStatus Interface

	// number of times woken
	HWA_STATS_EXPR(uint32 wake_cnt[HWA_TX_CORES_MAX]);
	// total TxStatus items processed
	HWA_STATS_EXPR(uint32 proc_cnt[HWA_TX_CORES_MAX]);

	hwa_txstat_stats_t stats[HWA_TX_CORES]; // Common block stats

	hwa_mem_addr_t          txstat_addr;    // AXI address of HWA4b TxStat memory

	bool                    status_stall;   // WAR for TXs stall.

#if defined(HWA_PKTPGR_BUILD)
	bool        status_mismatch;   // WAR for TXs framid mismatch
	int         elem_ix_ack;
	int         elem_ix_bad;
	int         pagein_req_cnt;
#endif // endif
} hwa_txstat_t;

// HWA4a TxStat block level management
void    BCMATTACHFN(hwa_txstat_detach)(hwa_txstat_t *txstat);
hwa_txstat_t *BCMATTACHFN(hwa_txstat_attach)(hwa_dev_t *dev);
void    hwa_txstat_init(hwa_txstat_t *txstat);
void    hwa_txstat_deinit(hwa_txstat_t *txstat);
void    hwa_txstat_wait_to_finish(hwa_txstat_t *txstat, uint32 core);

// Consume all txstatus in H2S txstatus interface
int     hwa_txstat_process(struct hwa_dev *dev, uint32 core, bool bound);

// HWA4a TxStat  block level statistics per core
void    hwa_txstat_stats_clear(hwa_txstat_t *txstat, uint32 core);
void    hwa_txstat_stats_dump(hwa_txstat_t *txstat, struct bcmstrbuf *b, uint8 clear_on_copy);

#if defined(BCMDBG) || defined(HWA_DUMP)
// Debug support for HWA1x blocks 1a + 1b + 2a
void    hwa_txstat_dump(hwa_txstat_t *txstat, struct bcmstrbuf *b, bool verbose, bool dump_regs);
#if defined(WLTEST) || defined(HWA_DUMP)
void    hwa_txstat_regs_dump(hwa_txstat_t *txstat, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */
#endif /* HWA_TXSTAT_BUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: HWA2b and HWA4b CplEng blocks
 *
 * Design Note:
 * Completion Engines will ALWAYS use Aggregation Mode with a default
 * aggregation factor of 4. This applies to both TxCPl and RxCpl
 *
 * -----------------------------------------------------------------------------
 */

#ifdef HWA_RXCPLE_BUILD

// Depth of S2H Rx CED Queue interfaces
#define HWA_RXCPLE_CEDQ_RING_DEPTH      1024

// Depth of local driver storage of RxCpl Aggregated Compact Work Items
#define HWA_RXCPLE_ACWI_RING_DEPTH      HWA_RXCPLE_CEDQ_RING_DEPTH

// CEDQ and WI array SW implementation restriction
#if (HWA_RXCPLE_CEDQ_RING_DEPTH != HWA_RXCPLE_ACWI_RING_DEPTH)
#error "RXCPLE ring management assumes equal CEDQ and ACWI array depth"
#endif // endif

// RxCpl flush threshold in units of number of ACWIs
#define HWA_RXCPLE_FLUSH_THRESHOLD      16  // 16 ACWI x 4 aggr_size = 64 WIs

// RxCpl WR Index update interrupt aggregation
#define HWA_RXCPLE_INTRAGGR_COUNT       8   // interrupt host every 8 xfers
#define HWA_RXCPLE_INTRAGGR_TMOUT       100 // usecs

#endif /* HWA_RXCPLE_BUILD */

#ifdef HWA_TXCPLE_BUILD

// Depth of S2H Tx CED Queue interfaces
#define HWA_TXCPLE_CEDQ_RING_DEPTH      1024

// Depth of local driver storage of TxCpl Agrgegated Compact Work Items
#define HWA_TXCPLE_ACWI_RING_DEPTH      (HWA_TXCPLE_CEDQ_RING_DEPTH)

// CEDQ and WI array SW implementation restriction
#if (HWA_TXCPLE_CEDQ_RING_DEPTH != HWA_TXCPLE_ACWI_RING_DEPTH)
#error "TXCPLE ring management assumes equal CEDQ and ACWI array depth"
#endif // endif

// TxCpl Flush threshold in units of number of ACWIs
#define HWA_TXCPLE_FLUSH_THRESHOLD      32  // 32 ACWI x 4 aggr-size = 128 WIs

// TxCpl WR Index update interrupt aggregation
#define HWA_TXCPLE_INTRAGGR_COUNT       16  // interrupt host every 16 xfers
#define HWA_TXCPLE_INTRAGGR_TMOUT       200 // usecs

#endif /* HWA_TXCPLE_BUILD */

#if defined(HWA_CPLENG_BUILD)

#if (HWA_AGGR_MAX != 4)
#error "HWA CPLENG designed for Aggregation Mode ONLY with a factor of 4"
#endif /* HWA_AGGR_MAX */

#define HWA_CPLENG_CEDQ_FW_WAKEUP       4     // wakeup FW every 4 CED

// One CEDQ instance per PCIE IPC D2H Completion ring
typedef enum hwa_cpleng_cedq_idx
{
	HWA_CPLENG_CEDQ_TX = 0,                   // single TX cedq[0]
	HWA_CPLENG_CEDQ_RX = 1,                   // single RX cedq[1]
#ifdef RXCPL4
	HWA_CPLENG_CEDQ_TOT = 5
#else
	HWA_CPLENG_CEDQ_TOT = 2
#endif // endif
} hwa_cpleng_cedq_idx_t;

// Generic Rx and Tx completion engine SW state
typedef struct hwa_cple
{
	// Local circular ring of CPL work items
	void                    *awi_curr;        // pointer to current AWI
	uint16                  awi_entry;        // index of current AWI in ring
	uint16                  wi_entry;         // current WI entry in current AWI
	uint16                  wi_added;         // number of WI added
	uint16                  awi_pend;         // number AWI not yet flushed
	uint16                  awi_free;         // number of free AWI
	uint16                  awi_comm;         // commit threshold for pend AWI
	uint16                  awi_size;         // size of an AWI
	uint16                  awi_depth;        // depth of AWI array
	bcm_ring_t              awi_ring;         // manage local circular ring
	void                    *awi_xfer;        // xfer base of pending AWI
	void                    *awi_base;        // circular ring memory for AWI

	// S2H CEDQ Interface
	uint16                  cpl_ring_idx;     // D2H cpl ring idx
	uint16                  aggr_max;         // max aggregation cnt
	hwa_ring_t              cedq_ring;        // S2H Rx CED Queue ring context
	hwa_cpleng_ced_t        *cedq_base;       // pointer to item in cedq
	hwa_cpleng_cedq_idx_t   cedq_idx;         // CEDQ interface instance

	// Statistics for WI and CEDQ
	HWA_STATS_EXPR(uint32   wi_total_count);  // count of WI added
	HWA_STATS_EXPR(uint32   awi_xfer_count);  // count of AWI transferred
	HWA_STATS_EXPR(uint32   awi_sync_count);  // count of AWI sync invocations
	HWA_STATS_EXPR(uint32   awi_full_count);  // AWI ring full occurrences

	// Statistics for CEDQ
	HWA_STATS_EXPR(uint32   cedq_upd_count);  // CEDQ interface refreshed
	HWA_STATS_EXPR(uint32   cedq_xfer_count); // count of CED requests

} hwa_cple_t;

// HWA2b and HWA4b Completion Engine State
// Even though 43684 can support a Rx Completion Engine per Access Category,
// only one PCIE D2H RxCompletion ring is supported in host DHD/Runner.
typedef struct hwa_cpleng
{
	HWA_TXCPLE_EXPR(hwa_cple_t txcple);       // 1 Tx Completion Engine
#ifdef RXCPL4
	HWA_RXCPLE_EXPR(hwa_cple_t rxcple[4]);       // 4 Rx Completion Engine
#else
	HWA_RXCPLE_EXPR(hwa_cple_t rxcple);       // 1 Rx Completion Engine
#endif // endif
	hwa_mem_addr_t          ring_addr;        // Ring configuration in AXI mem

	hwa_cpleng_common_stats_t common_stats;
	hwa_cpleng_cedq_stats_t cedq_stats[HWA_CPLENG_CEDQ_TOT];
} hwa_cpleng_t;

// HWA2b and HWA4b block level management
void    BCMATTACHFN(hwa_cpleng_detach)(hwa_cpleng_t *cpleng);
hwa_cpleng_t *BCMATTACHFN(hwa_cpleng_attach)(hwa_dev_t *dev);
int     hwa_cpleng_init(hwa_cpleng_t *cpleng);
void    hwa_cpleng_deinit(hwa_cpleng_t *cpleng);

// Fetch the HWA2b and HWA4b CplEng status
void    hwa_cpleng_status(hwa_cpleng_t *cpleng, struct bcmstrbuf *b);

// HWA2b and HWA4b CplEng block level statistics
void    hwa_cpleng_stats_clear(hwa_cpleng_t *cpleng, uint32 eng_id);
void    hwa_cpleng_stats_dump(hwa_cpleng_t *cpleng, struct bcmstrbuf *b, uint8 clear_on_copy);

#if defined(BCMDBG) || defined(HWA_DUMP)
// Debug support for HWA2b and HWA4b
void    hwa_cple_dump(hwa_cple_t *cple, struct bcmstrbuf *b, const char *name, bool verbose);
void    hwa_cpleng_dump(hwa_cpleng_t *cpleng, struct bcmstrbuf *b, bool verbose, bool dump_regs);
#if defined(WLTEST) || defined(HWA_DUMP)
void    hwa_cpleng_regs_dump(hwa_cpleng_t *cpleng, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */
#endif /* HWA_CPLENG_BUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: HWA Packet Pager
 * -----------------------------------------------------------------------------
 */
#if defined(HWA_PKTPGR_BUILD)

// #define PSPL_TX_TEST      // For PUSH, PULL functionality test

typedef enum hwa_dbm_instance
{
	HWA_HDBM  = 0,        // Host Data BM
	HWA_DDBM  = 1         // Dongle Data BM
} hwa_dbm_instance_t;

// Depth of PUSH/PULL request info ring
#define HWA_PKTPGR_PSPL_DEPTH       32

// Depth of PCIe Full Dongle HWA1a RxPost to SW Interface
#define HWA_PKTPGR_INTERFACE_DEPTH  512

// No interrupt coalescing to reduce latency by default
#define HWA_PKTPGR_INTRAGGR_COUNT   1
#define HWA_PKTPGR_INTRAGGR_TMOUT   0       // usecs

// WAITTIME is for debug tuning.  It's the delay cycles for next requests process.
// BESTEFFORT is for number of packets return from PAGEIN_RXPRCOESS.
// (sw:pkt_Count, sw:pkt_Bound and hw:pkt_Ready)
// if R < C return R else
// if R < B return R else
// if BESTEFFORT == 1 then return B [R > B]
// else return R [R > B]
#define HWA_PKTPGR_REQ_WAITTIME     0       // delay cycles between requests
#define HWA_PKTPGR_RX_BESTEFFORT    1

#define HWA_PKTPGR_RX_COUNT_MAX     64
#define HWA_PKTPGR_RX_BOUND_MAX     (HWA_PKTPGR_RX_COUNT_MAX * 3)

// We have three pktpool available threshold.
// PPHDBMFullInt, PPHDBMTH1Int, PPHDBMTH2Int
// PPHDBMFullInt: Set when available count change from 1 to 0
// PPHDBMTH1Int: Set when available count chnage from PPHDTH1 to PPHDTH1-1
// PPHDBMTH2Int: Set when available count chnage from PPHDTH2 to PPHDTH2-1
// No order of them.  PPDDTH1, PPDDTH2 have the same.
#define HWA_PKTPGR_HOSTPKTPOOL_TH1  64
#define HWA_PKTPGR_HOSTPKTPOOL_TH2  128
#define HWA_PKTPGR_DNGLPKTPOOL_TH1  64
#define HWA_PKTPGR_DNGLPKTPOOL_TH2  128

// No order of D11BDEST threshold level, they are independent.
#define HWA_PKTPGR_D11BDEST_TH0     16
#define HWA_PKTPGR_D11BDEST_TH1     32
#define HWA_PKTPGR_D11BDEST_TH2     128

#define HWA_PKTPGR_D11BDEST_DEPTH   2048

#define HWA_D11BDEST_RXPKT_READY_INVALID 0xFFFF

// TXDMA
#define HWA_PKTPGR_MACAQM_EMPTY_CNT  (HWA_TXFIFO_AGGR_AQM_DESC_THRESHOLD + 1)
#define HWA_PKTPGR_MACFIFO_EMPTY_CNT HWA_TXFIFO_EMPTY_THRESHOLD
// Start pageout local when HDBM free count > HWA_PKTPGR_HDBM_AVAILCNT
#define HWA_PKTPGR_HDBM_AVAILCNT     8

// Group Ints in common::pageintstatus
// PAGEIN: exclude PPINREQINT and all errors
#define HWA_COMMON_PAGEIN_INT_MASK \
	(HWA_COMMON_PAGEINTSTATUS_PPINRSPINT_MASK)
// PAGEOUT: exclude PPOUTREQINT and all errors
#define HWA_COMMON_PAGEOUT_INT_MASK \
	(HWA_COMMON_PAGEINTSTATUS_PPOUTRSPINT_MASK)
// PAGEMGR: exclude PPALLOCREQINT, PPFREEREQINT, PPFREERPHREQINT and all errors
#define HWA_COMMON_PAGEMGR_INT_MASK \
	(HWA_COMMON_PAGEINTSTATUS_PPALLOCRSPINT_MASK)
// PAGERBM: exclude PP#DBMFULLINT, PP#DBMTH2INT and alloc/free err
#define HWA_COMMON_PAGERBM_INT_MASK \
	(HWA_COMMON_PAGEINTSTATUS_PPHDBMTH1INT_MASK | \
	 HWA_COMMON_PAGEINTSTATUS_PPDDBMTH1INT_MASK)

// First level IntStatus: common::pageintstatus
// - exclude PAGER_BM:
//           HDBM(full, Th2, alloc/free err)
//           DDBM(full, Th2, alloc/free err)
#define HWA_PKTPGR_INT_MASK \
	(HWA_COMMON_PAGEIN_INT_MASK  | \
	 HWA_COMMON_PAGEOUT_INT_MASK | \
	 HWA_COMMON_PAGEMGR_INT_MASK | \
	 HWA_COMMON_PAGERBM_INT_MASK)

// Second level IntStatus: PAGEIN, PAGEOUT, PAGEMGR, PAGERBM
// These second levle IntStatus are READ-ONLY for further ERROR handling
// NOTE: We don't turn on ERROR bits in common::pageintstatus

/*
 * (see hwa_reg_defs.h)
 * Errors reported via PAGEIN, PAGEOUT, PAGEMGR, PAGERBM
 *
 *      HWA_PAGER_PAGEIN_ERRORS_MASK
 *          + HWA_PAGER_PAGEIN_REQ_ERRORS_MASK
 *          + HWA_PAGER_PAGEIN_RSP_ERRORS_MASK
 *      HWA_PAGER_PAGEOUT_ERRORS_MASK
 *          + HWA_PAGER_PAGEOUT_REQ_ERRORS_MASK
 *          + HWA_PAGER_PAGEOUT_RSP_ERRORS_MASK
 *      HWA_PAGER_PAGEMGR_ERRORS_MASK
 *          + HWA_PAGER_PAGEMGR_REQ_ERRORS_MASK
 *          + HWA_PAGER_PAGEMGR_RSP_ERRORS_MASK
 *      HWA_PAGER_PAGERBM_ERRORS_MASK
 *          + HWA_PAGER_HDBM_ERRORS_MASK
 *          + HWA_PAGER_DDBM_ERRORS_MASK
 *
 * hwa_pktpgr_status() queries above errors in PAGEIN, PAGEOUT, PAGEMGR, PAGERBM
 */

typedef enum hwa_pktpgr_ring
{
	hwa_pktpgr_pagein_req_ring     = 0,
	hwa_pktpgr_pageout_req_ring    = 1,
	hwa_pktpgr_pagemgr_req_ring    = 2,
	hwa_pktpgr_freepkt_req_ring    = 3,
	hwa_pktpgr_freerph_req_ring    = 4,
	hwa_pktpgr_req_ring_max        = 5,

	hwa_pktpgr_pagein_rsp_ring     = 0,
	hwa_pktpgr_pageout_rsp_ring    = 1,
	hwa_pktpgr_pagemgr_rsp_ring    = 2,
	hwa_pktpgr_rsp_ring_max        = 3,

	hwa_pktpgr_ring_max            = hwa_pktpgr_req_ring_max
	                               + hwa_pktpgr_rsp_ring_max
} hwa_pktpgr_ring_t;

typedef struct hwa_pktpgr
{
	// Table of Packet Pager Request and Response hwa_ring contexts
	hwa_ring_t    * req_ring[hwa_pktpgr_req_ring_max];
	hwa_ring_t    * rsp_ring[hwa_pktpgr_rsp_ring_max];

	uint8           trans_id[hwa_pktpgr_req_ring_max];

	hwa_ring_t      pagein_req_ring;        // PageIN Request ring
	hwa_ring_t      pagein_rsp_ring;        // PageIN Response ring
	hwa_ring_t      pageout_req_ring;       // PageOUT Request ring
	hwa_ring_t      pageout_rsp_ring;       // PageOUT Response ring
	hwa_ring_t      pagemgr_req_ring;       // PageMGR Request ring
	hwa_ring_t      pagemgr_rsp_ring;       // PageMGR Response ring
	hwa_ring_t      freepkt_req_ring;       // PageMGR FREEPKT Request ring
	hwa_ring_t      freerph_req_ring;       // PageMGR FREERPH Request ring

	// Table of Host and Dongle Packet Pool context
	uint16          hostpktpool_max;        // Host Lbuf Pool max items
	uint16          dnglpktpool_max;        // Dngl Lbuf Pool max items
	haddr64_t       hostpktpool_haddr64;    // Host Lbuf Pool memory address
	void          * dnglpktpool_mem;        // Dngl Lbuf Pool memory pointer

#ifdef PSPL_TX_TEST
	// PUSH/PULL tx test request info ring contexts
	bcm_ring_t      pspl_state;             // SW context: read and write state
	void          * pspl_table;             // memory for ring
	uint16          pspl_depth;             // ring depth: num elements in ring
	bool            pspl_test_en;           // Enable/Disable pspl test
#endif /* PSPL_TX_TEST */

	// SW maintain variables.
	int             pgo_local_req;          // Counter of pageout local request
	int             pgi_txs_req[HWA_TX_FIFOS_MAX]; // Counter of pagein txs request
	int             pgi_txs_req_tot;        // Counter of pagein txs request
	int             pgi_rxpkt_req;          // Counter of pagein rx request
	int             pgi_rxpkt_in_trans;     // Counter of pagein rx packet
	hwa_pp_rxlbuf_pool_t rxlbufpool;        // Local rxlbuf storage for SW
	hwa_pp_txlbuf_pool_t txlbufpool;        // Local txlbuf storage for SW

	int             tag_pull_req;           // Counter of tagged pull request
	struct spktq    tag_pull_q;             // Single priority tagged pull packets queue

	int             tag_push_req;           // Counter of tagged push request
	hwa_pp_ring_t * req_ring_reg[hwa_pktpgr_req_ring_max];
} hwa_pktpgr_t;

// Post a hwa_pp_cmd_t request command to a S2H Request ring
int     hwa_pktpgr_request(hwa_dev_t *dev,
                           hwa_pktpgr_ring_t req_ring, void *hwa_pp_cmd);

hwa_ring_t * BCMATTACHFN(hwa_pktpgr_ring_init)(hwa_ring_t *hwa_ring,
                         const char *ring_name, uint8 ring_dir, uint8 ring_num,
                         uint16 depth, void *memory,
                         hwa_regs_t *regs, hwa_pp_ring_t *hwa_pp_ring);

hwa_pktpgr_t *BCMATTACHFN(hwa_pktpgr_attach)(hwa_dev_t *dev);
void    BCMATTACHFN(hwa_pktpgr_detach)(hwa_pktpgr_t *pktpgr);

void    hwa_pktpgr_preinit(hwa_pktpgr_t *pktpgr);

void    hwa_pktpgr_init(hwa_pktpgr_t *pktpgr);
void    hwa_pktpgr_deinit(hwa_pktpgr_t *pktpgr);

// HWA Packet Pager Debug Support
void    hwa_pktpgr_status(hwa_dev_t *dev);

// Print HDBM and DDBM pooluse.
void    hwa_pktpgr_print_pooluse(hwa_dev_t *dev);

#if defined(BCMDBG) || defined(HWA_DUMP)
// Dump all SW and HWA Packet Pager state
void    hwa_pktpgr_dump(hwa_pktpgr_t *pktpgr, struct bcmstrbuf *b,
                        bool verbose, bool dump_regs, uint8 *fifo_bitmap);
#if defined(WLTEST) || defined(HWA_DUMP)
// Dump HWA Packet Pager registers
void    hwa_pktpgr_regs_dump(hwa_pktpgr_t *pktpgr, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */

// PageIn, PageOut, PageMgr APIs.

void    hwa_pktpgr_response(hwa_dev_t *dev, hwa_ring_t *rsp_ring,
            hwa_callback_t callback);

// PageIn Rx
int     hwa_rxfill_pagein_rx_req(hwa_dev_t *dev, uint32 core, uint16 rxpkt_ready, bool bound);
void    hwa_rxfill_pagein_rx_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id,
            int pkt_count, hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail);

// PageIn Tx
void    hwa_txpost_pagein_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id, int pkt_count,
            uint32 oct_count, hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail);

// PageIn Txs
void    hwa_txstat_pagein_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id, uint8 tagged,
            int fifo_idx, int mpdu_count, int pkt_count, hwa_pp_lbuf_t *pktlist_head,
            hwa_pp_lbuf_t *pktlist_tail);

// PageOut Tx
void    hwa_txfifo_pageout_local_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id, int fifo_idx,
            int mpdu_count, int total_txdma_desc, hwa_pp_lbuf_t *pkt_local);
void    hwa_txfifo_pageout_pktlist_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id, int fifo_idx,
            int mpdu_count, int total_txdma_desc, hwa_pp_lbuf_t *pkt_local);

// PageMgr Tx allocate
void    hwa_txpost_pagemgr_alloc_tx_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id,
            int pkt_count, hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail);

// PageMgr Rx, RPH allocate
void    hwa_rxpath_pagemgr_alloc_rx_rph_rsp(hwa_dev_t *dev, uint8 pktpgr_trans_id,
            int pkt_count, hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail);

// PageMgr Push, Pull
void    hwa_pktpgr_push_req(hwa_dev_t *dev, uint32 fifo_idx, uint8 tagged, void *pktlist_head,
            void *pktlist_tail, uint16 pkt_count, uint16 mpdu_count);

// PUSH pkttag part only
void    hwa_pktpgr_push_pkttag_req(hwa_dev_t *dev, uint32 fifo_idx, uint8 tagged,
            void *pktlist_head, void *pktlist_tail, uint16 pkt_count, uint16 mpdu_count);

#ifdef PSPL_TX_TEST
void    hwa_txfifo_pull_pktchain_xmit_request(hwa_dev_t *dev, uint8 pktpgr_trans_id,
            int fifo_idx, int mpdu_count, int pkt_count,
            hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail);
#endif // endif
// PageMgr Tx, Rx lbuf pool.
void    hwa_txpost_txlbufpool_fill(hwa_dev_t *dev);
void    hwa_rxpath_rxlbufpool_fill(hwa_dev_t *dev, uint16 force_cnt);

// Get trans_id from specific pktpgr ring
uint8   hwa_pktpgr_get_trans_id(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring);

// Update trans_id to specific pktpgr ring
void    hwa_pktpgr_update_trans_id(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring,
            uint8 trans_id);

// Update wr_index to specific pktpgr ring
void    hwa_pktpgr_update_ring_wr_index(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring,
            uint16 wr_index);

// Wait for number of "countes" empty slots is available for specific pktpgr ring
bool    hwa_pktpgr_req_ring_wait_for_avail(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring,
            uint32 counts, bool wait);

// Enuqeqe Tx pacekts to SW shadow
void    hwa_txfifo_shadow_enq(hwa_dev_t *dev, int fifo_idx, int mpdu_count, int pkt_count,
            hwa_pp_lbuf_t *pktlist_head, hwa_pp_lbuf_t *pktlist_tail);

// Sanity check and fixup head and end for local packet
void    hwa_txstat_pagein_rsp_fixup_local(hwa_dev_t *dev, hwa_pp_lbuf_t *txlbuf,
	int fifo_idx, int mpdu_count);

// Sanity check and fixup head and end
void    hwa_txstat_pagein_rsp_fixup(hwa_dev_t *dev, hwa_pp_lbuf_t *txlbuf, int mpdu_count);

// Wait for all pagein TxS be processed
void    hwa_pktpgr_txstatus_wait_to_finish(hwa_dev_t *dev, int fifo_idx);

// Invalid request
void    hwa_pktpgr_req_ring_invalid_req(hwa_dev_t *dev, hwa_pktpgr_ring_t pp_ring,
            uint8 cmdtype, const char *cmdstr);

// Fetch all RxBuffers
void    hwa_rxfill_d11b_fetch_all(hwa_dev_t *dev, uint32 core);

// Wait for all pagein RxProcess be processed
void    hwa_pktpgr_rxprocess_wait_to_finish(hwa_dev_t *dev);

// Free a buffer to HDBM or DDBM base on its buf_idx
int     hwa_pktpgr_dbm_free(hwa_dev_t *dev, hwa_dbm_instance_t dbm_instance, uint16 buf_idx);

// Free DDBM in pktlist.
void    hwa_pktpgr_free_ddbm_pkt(hwa_dev_t *dev, hwa_pp_lbuf_t *pktlist_head,
            hwa_pp_lbuf_t * pktlist_tail, int pkt_count);

// Map pkts in HW shadow
void    hwa_pktpgr_map_pkts(hwa_dev_t *dev, uint32 fifo_idx, void *cb, void *ctx);

// Wait for all PageOut local response.
void    hwa_pktpgr_pageout_local_rsp_wait_to_finish(hwa_dev_t *dev);

static INLINE uint16
hwa_pktpgr_hdbm_ptr2idx(hwa_pktpgr_t *pktpgr, uint32 buf_lo)
{
	uint32 pkt_mapid;
	HWA_ASSERT(HADDR64_LO(pktpgr->hostpktpool_haddr64) <= buf_lo);
	pkt_mapid = (buf_lo - pktpgr->hostpktpool_haddr64.loaddr) / HWA_PP_LBUF_SZ;
	HWA_ASSERT(pkt_mapid < HWA_HOST_PKTS_MAX);
	return pkt_mapid;
}

#endif /* HWA_PKTPGR_BUILD */

#ifdef HWA_DPC_BUILD
/*
 * -----------------------------------------------------------------------------
 * Section: HWA Internal DMA Engine managing multiple DMA channels
 * -----------------------------------------------------------------------------
 */
typedef enum hwa_dma_engine
{
	HWA_XFER_SYSMEM_DMA_ENGINE = 0,         // Engine 0 = HWA - TCM
	HWA_XFER_PCIE_DMA_ENGINE = 1            // Engine 1 = HWA - PCIE
} hwa_dma_engine_t;

typedef union hwa_dma_status                // DMA channel Xmt and Rcv status
{
	uint32 u32;
	struct {
		uint8 xmt_state;                    // 31:28 XmtStatus0::XmtState
		uint8 xmt_error;                    // 31:28 XmtStatus1::XmtErr
		uint8 rcv_state;                    // 31:28 RcvStatus0::RcvState
		uint8 rcv_error;                    // 31:28 RcvStatus1::RcvErr
	};
} hwa_dma_status_t;

typedef struct hwa_dma
{
	uint8 channels, channels_max;             // channel count
	uint8 burst_length_max;                   // burst length
	struct {
		uint8 tx;                         // tx burst length
		uint8 rx;                         // rx burst length
	} burst_length[HWA_DMA_CHANNELS_MAX];
	uint8 outstanding_rds, outstanding_rds_max;
	uint8 arbitration;                        // 0 = RR, 1 = fixed Chnl# prio

	bool  enabled;                            // all channels, tx and rx
} hwa_dma_t;

void    BCMATTACHFN(hwa_dma_attach)(hwa_dev_t *dev);
void    hwa_dma_enable(hwa_dma_t *dma);     // HWA's DMA engines enabled
void    hwa_dma_rx_burstlen_adjust(hwa_dma_t *dma);
hwa_dma_status_t hwa_dma_channel_status(hwa_dma_t *dma, uint32 channel);

#if defined(BCMDBG) || defined(HWA_DUMP)
void    hwa_dma_dump(hwa_dma_t *dma, struct bcmstrbuf *b, bool verbose, bool dump_regs);
#if defined(WLTEST) || defined(HWA_DUMP)
void    hwa_dma_regs_dump(hwa_dma_t *dma, struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */
#endif /* HWA_DPC_BUILD */

/*
 * -----------------------------------------------------------------------------
 * Section: HWA Buffer Manager
 * -----------------------------------------------------------------------------
 */

#define HWA_BM_NAME_SIZE    12
typedef enum hwa_bm_instance
{
	HWA_TX_BM = 0,              // Tx BM
	HWA_RX_BM = 1               // Rx BM
} hwa_bm_instance_t;

typedef struct hwa_bm
{
	struct hwa_dev          *dev;           // backpointer to main hwa device

	uint16                  pkt_total;      // total SW packets in pool
	uint16                  pkt_size;       // SW packet size in Bytes
	dma64addr_t             pkt_base;       // base of SW packet pool 32b
	void                    *memory;        // base memory

	HWA_STATS_EXPR(uint32   allocs);        // number of SW allocs
	HWA_STATS_EXPR(uint32   frees);         // number of SW frees
	HWA_STATS_EXPR(uint32   fails);         // number of failures

	uint32                  loopcnt_hwm;    // loop count hwm
	uint16                  pkt_max;
	uint16                  avail_sw;       // available BM buffer, SW accounting.
	HWA_STATS_EXPR(uint32   avail_sw_low);  // number of times of low avail_sw
	bool                    enabled;
	hwa_bm_instance_t       instance;
	char                    name[HWA_BM_NAME_SIZE];

} hwa_bm_t;

// HWA Buffer Manager
void    hwa_bm_config(hwa_dev_t *dev, hwa_bm_t *bm,
            const char *name, hwa_bm_instance_t bm_instance,
            uint16 pkt_total, uint16 pkt_size,
            uint32 pkt_base_loaddr, uint32 pkt_base_hiaddr, void *memory);
void    hwa_bm_init(hwa_bm_t *bm, bool enable);
int     hwa_bm_alloc(hwa_bm_t *bm, dma64addr_t *buf_addr);
int     hwa_bm_free(hwa_bm_t *bm, uint16 buf_idx);
uint32  hwa_bm_avail(hwa_bm_t *bm);

#if defined(BCMDBG) || defined(HWA_DUMP)
void    hwa_bm_dump(hwa_bm_t *bm, struct bcmstrbuf *b, bool verbose, bool dump_regs);
#if defined(WLTEST) || defined(HWA_DUMP)
void    hwa_bm_regs_dump(hwa_bm_t *bm,  struct bcmstrbuf *b);
#endif // endif
#endif /* BCMDBG */

static INLINE uint16
hwa_bm_ptr2idx(hwa_bm_t *bm, void *buf)
{
	uint32 buf_idx = (uint32)
		(((uintptr)buf - (uintptr)bm->pkt_base.loaddr) / bm->pkt_size);
	HWA_ASSERT(buf_idx < bm->pkt_total);
	HWA_ASSERT((((uintptr)buf - (uintptr)bm->pkt_base.loaddr) % bm->pkt_size) == 0);
	return buf_idx;
}

/*
 * -----------------------------------------------------------------------------
 * Section: Simple Statistics Register Set Management
 * Note: Statistics set may be copied out into an address with hiaddr = 0x0
 * -----------------------------------------------------------------------------
 */

// Per HWA block's stats callback handler, e.g. dump a block's stats
typedef void (*hwa_stats_cb_t)(hwa_dev_t *dev, uintptr arg1, uint32 arg2);

void    hwa_stats_clear(hwa_dev_t *dev, hwa_stats_set_index_t set_idx);
void    hwa_stats_copy(hwa_dev_t *dev, hwa_stats_set_index_t set_idx,
            uint32 loaddr, uint32 hiaddr, uint32 num_sets, uint8 clear_on_copy,
            hwa_stats_cb_t stats_cb, uintptr arg1, uint32 arg2);

/*
 * -----------------------------------------------------------------------------
 * Section: Miscellaneous HWA Controls
 * -----------------------------------------------------------------------------
 */

// HWA Clock Control: enum values picked from register top::clkctlstatus
typedef enum hwa_clkctl_cmd
{
	                             // Enable or Disable request to PMU
	HWA_FORCE_ALP_REQUEST  = 0,  // FA: select a bkpl clk as fast as ALP clk
	HWA_FORCE_HT_REQUEST   = 1,  // FH: select HT clock for use as the bkpl clk
	HWA_FORCE_ILP_REQUEST  = 2,  // FI: select a bkpl clk as fast as ILP clk
	HWA_ALP_AVAIL_REQUEST  = 3,  // AQ: up oscillator for ALP clk
	HWA_HT_AVAIL_REQUEST   = 4,  // HQ: up PLL for HT clk
	HWA_FORCE_HW_CLK_REQUEST_OFF = 5, // FC: disables HW-based clock
	HWA_HQ_CLOCK_REQUIRED  = 6,  // HR: ALP, HT req, avail from High Qual
	HWA_HWA_CLK_REQUEST    = 8,  // MR: enable HWA core clk
	HWA_CLK_REQUEST_MAX    = 15,

	                             // Below for queries of current clk status
	HWA_IS_ALP_CLK_AVAIL   = 16, // AA: is ALP clk is available
	HWA_IS_HT_CLK_AVAIL    = 17, // HA: is HT clk available
	HWA_IS_BP_ON_ALP       = 18, // BA: is backplane running on ALP
	HWA_IS_BP_ON_HT        = 19, // BH: is backplane running on HT
	HWA_IS_HWA_CLK_AVAIL   = 24  // MA: is HWA core clk available
} hwa_clkctl_cmd_t;

// Enable or Disable a clk request, or fetch current clk status
uint32  hwa_clkctl_request(hwa_dev_t *dev, hwa_clkctl_cmd_t cmd, bool enable);

// HWA Power Control:
typedef enum hwa_pwrctl_cmd
{
	HWA_MEM_CLK_GATING     = 0, // top::powercontrol::enable_mem_clk_gating
	HWA_POWER_KEEP_ON      = 1, // top::powercontrol::powerctl
} hwa_pwrctl_cmd_t;

// Control a HWA Memory clk gating and power
uint32  hwa_pwrctl_request(hwa_dev_t *dev, hwa_pwrctl_cmd_t cmd, uint32 val);

// HWA Module Control: list of registers using same bit settings for blocks
typedef enum hwa_module_cmd
{
	HWA_MODULE_CLK_ENABLE  = 0, // common::module_clk_enable
	HWA_MODULE_CLK_GATING  = 1, // common::module_clkgating_enable
	HWA_MODULE_RESET       = 2, // common::moudle_reset
	HWA_MODULE_CLK_AVAIL   = 3, // common::module_clkavail
	HWA_MODULE_ENABLE      = 4, // common::module_enable
	HWA_MODULE_IDLE        = 5  // common::module_idle
} hwa_module_cmd_t;

// HWA Module block: enum values picked from register common::module_enable
typedef enum hwa_module_block
{
	HWA_MODULE_RXCORE0    = 0, // Core0: HWA1a RxPost HWA1b RxFILL HWA2a RxDATA
	HWA_MODULE_RXCORE1    = 1, // Core1: HWA1a RxPost HWA1b RxFILL HWA2a RxDATA
	HWA_MODULE_RX_BM      = 2, // Rx Buffer Manager
	HWA_MODULE_CPLENG     = 3, // HWA2b RxCPLE and HWA4b TxCPLE
	HWA_MODULE_TXSTAT0    = 4, // Core0: HWA4a TxSTAT
	HWA_MODULE_TXPOST     = 5, // HWA3a TxPOST
	HWA_MODULE_TXFIFO     = 6, // HWA3b TxFIFO
	HWA_MODULE_TXSTAT1    = 7, // Core1: HWA4a TxSTAT
	HWA_MODULE_STATS      = 8, // Statistics Engine
	HWA_MODULE_PAGER      = 9  // Pager Block
} hwa_module_block_t;

// Control a HWA module by programming the corresponding common register
uint32  hwa_module_request(hwa_dev_t *dev, hwa_module_block_t blk,
            hwa_module_cmd_t cmd, bool enable);

/*
 * -----------------------------------------------------------------------------
 * Section: HWA Device carrying SW driver state of all HWA blocks.
 * A single global hwa_dev_g is instantiated and accessible via "HWA_DEVP()".
 *
 * In FD mode, several hwa_dev fields may ONLY be accessed in INIT phase.
 *
 * HWA SW driver does not have its own private hnd_dev (or linux netdevice).
 * All HWA library routines may be invoked in the context of existing of an
 * existing hnd_dev/DPC.
 * Example: In Fulldongle the pciedev and mac device and corresponding DPC.
 *
 * hwa_dev_t is a device driver for interfacing with the HWA HW blocks. There
 * isn't a seperation of info, public, and private components.
 *
 * In NIC Mode (Linux) an additional character device may be instantiated,
 * along with procfs/sysfs for user space command line utilities.
 * In Full Dongle mode, the "bus:hwa_xyz" interface may be used to transfer
 * user commands to configure, query or debug HWA.
 *
 * -----------------------------------------------------------------------------
 */

struct hwa_dev
{
	hwa_regs_t      *regs;                     // HWA registers in APB space
	volatile hc_hin_regs_t *mac_regs;          // Handcoded Host Interface regs

	uint32          intmask, defintmask, intstatus; // DPC

	HWA_PKTPGR_EXPR(uint32 pageintmask);
	HWA_PKTPGR_EXPR(uint32 pageintstatus);

	// Platform contexts
	void            *wlc;
	si_t            *sih;                     // SOC Interconnects handler
	osl_t           *osh;                     // OS abstraction layer handler
	uint32          coreid, corerev;          // HWA core context
	uint32          device, bustype;          // HWA device context
	uint32		unit;			  // HWA instance# (same as WL instance#)
	hwa_mem_addr_t	axi_mem_base;		  // HWA axi memory base address
#ifndef DONGLEBUILD
	uint32		*bar1_va;		  // PCI bar1 address to map HAX AXI Memory Region
	uint32		bar1_mask;		  // PCI bar1 mask
	uint32		si_flag;		  // si_flag used for PCIEIntMask
#endif /* !DONGLEBUILD */
	hwa_handler_t   handlers[HWA_CALLBACK_MAX]; // WLAN upstream handlers

	HWA_DPC_EXPR(void            *sys_dev);   // DPC context

	// HWA common: shared by multiple blocks
	HWA_DPC_EXPR(hwa_dma_t       dma);        // HWA DMA engine SW state

	HWA_PKTPGR_EXPR(hwa_pktpgr_t pktpgr);     // SW state of HWA Packet Pager

	HWA_RXPATH_EXPR(hwa_rxpath_t rxpath);     // SW state HWA1a, HWA1b and HWA2a

#ifdef HWA_MAC_BUILD // HWA MAC facing blocks, used in Dongle and NIC mode
	HWA_RXFILL_EXPR(hwa_bm_t     rx_bm);      // Rx Buffer Manager HWA1b
	HWA_RXFILL_EXPR(hwa_rxfill_t rxfill);     // SW state HWA1b
	HWA_RXDATA_EXPR(hwa_rxdata_t rxdata);     // SW state HWA2a
	HWA_TXFIFO_EXPR(hwa_txfifo_t txfifo);     // SW state HWA3b
	HWA_TXSTAT_EXPR(hwa_txstat_t txstat);     // SW state HWA4a
#endif /* HWA_MAC_BUILD */

#ifdef HWA_BUS_BUILD // HWA PCIE BUS facing blocks: used in Dongle mode ONLY
	HWA_RXPOST_EXPR(hwa_rxpost_hostinfo_t rph_req);
	HWA_RXPOST_EXPR(hwa_rxpost_t rxpost);     // SW state HWA1a
	HWA_TXPOST_EXPR(hwa_bm_t     tx_bm);      // Tx Buffer Manager HWA3a
	HWA_TXPOST_EXPR(hwa_txpost_t txpost);     // SW state HWA3a
	HWA_CPLENG_EXPR(hwa_cpleng_t cpleng);     // SW state HWA2b and HWA4b

	// PCIEIPC shared: Host advertized capabilities and ring configuration
	struct pcie_ipc         *pcie_ipc;        // PCIE IPC shared struct   - INIT
	struct pcie_ipc_rings   *pcie_ipc_rings;  // PCIE Ring configuration  - INIT

	uint8                   wi_aggr_cnt;      // WorkItem Aggregation     - INIT

	// PCIEDEV variables in data path
	struct dngl_bus         *pciedev;
	struct dngl             *dngl;
#endif /* HWA_BUS_BUILD */

	                                          // Used as 0U or ~0U masks  - INIT
	uint32                  driver_mode;      // NIC or FullDongle mode
	uint32                  macif_placement;  // NIC: MAC Ifs in SysMem
	uint32                  macif_coherency;  // NIC: MAC Ifs coherency
	uint32                  host_coherency;   // Host HW coherency support
	uint32                  host_addressing;  // 32bit or 64bit addresses
	uint32                  host_physaddrhi;  // fixed hi32 in 32b hosts  - INIT

	// For design verification.

#ifdef RXCPL4
	uint8                   rxcpl_inuse;      // indicate which rxcpl channel is used.
#endif // endif
	// New direct interface to update AQM/TX DMA last index from HWA 3B to MAC.
	uint8                   txfifo_apb;       // 0:direct, 1:APB
	// HWA 4A support to get MAC TX status with AXI interface.
	uint8                   txstat_apb;       // 0:AXI, 1:APB
	// HWA 3b update packet chain next.
	uint8                   txfifo_hwupdnext; // 0:doesn't update, 1:update
	uint8                   hme_macifs_modes; // 0:none, 1:rx, 2:tx, 3:rx/tx
	// HWA DMA or DMA TX/RX
	uint8                   dmasel; // 0: use legacy HWA dma, 1: use dma tx/rx
	// TXDMA currentIdx calculation method
	uint8                   txd_cal_mode; // 0: read AQM descriptor, 1: from mac.
#if defined(HWA_PKTPGR_BUILD)
	// Internal D11B AXI memory
	uint8                   d11b_axi;
	// Use more than one TxLfrag with next ptr for one MPDU.
	uint8                   flexible_txlfrag;
	// D11B recycle WAR for A0
	bool                    d11b_recycle_war;
	// RxProcess.
	uint8                   rx_pkt_req_max;
	uint32                  rx_pkt_count_max;
#endif /* HWA_PKTPGR_BUILD */

	bool                    inited;
	bool                    up;
	bool                    reinit;
	struct hwa_dev          *self;            // Used in audit, point to self
};

#define HWA_DEV_NULL            ((hwa_dev_t*)NULL)

// Place holder to insert per HWA block state audit/debug
#define HWA_AUDIT()             ({ HWA_NOOP; }) // audit any block's state

#ifdef DONGLEBUILD
#define HWA_AUDIT_DEV(DEV) \
({ \
	HWA_ASSERT((DEV) == hwa_dev); \
	HWA_ASSERT(hwa_dev->self == hwa_dev); \
	HWA_AUDIT_REGS((DEV)->regs); \
	HWA_AUDIT(); \
})
#else /* !DONGLEBUILD */
#define HWA_AUDIT_DEV(DEV) \
({ \
	HWA_ASSERT((DEV) != HWA_DEV_NULL); \
	HWA_ASSERT((DEV)->self == (DEV)); \
	HWA_AUDIT_REGS((DEV)->regs); \
	HWA_AUDIT(); \
})
#endif /* DONGLEBUILD */

// Locate the parent hwa_dev_t object given a pointer to a member.
#define HWA_DEV(CHILD) \
({  /* child: a pointer to member, must match member's name in hwa_dev_t */ \
	hwa_dev_t *devptr; \
	typeof(((hwa_dev_t*)0)->CHILD) *__mptr = (CHILD); \
	HWA_ASSERT(CHILD != NULL); \
	devptr = (hwa_dev_t*)((char*)__mptr - ((size_t)&((hwa_dev_t*)0)->CHILD)); \
	HWA_AUDIT_DEV(devptr); \
	devptr; \
})

// Design Note: All Access to hwa_dev global must be via HWA_DEVP()
#ifdef DONGLEBUILD
#define HWA_DEVP(AUDIT) \
({ if (AUDIT) { HWA_AUDIT_DEV(hwa_dev); } hwa_dev; })
#endif /* DONGLEBUILD */

#define HWA_MODULE_IDLE_BURNLOOP	256
#define HWA_FSM_IDLE_POLLLOOP		10 * 1000

/* Perform bulk cache inv */
#ifdef DONGLEBUILD
#define HWA_BULK_CACHE_INV(type, hwa_ring)	HWA_NOOP
#else /* !DONGLEBUILD */
#define HWA_BULK_CACHE_INV(type, hwa_ring) \
({ \
	int wr, rd; \
	void *va; \
	rd = HWA_RING_STATE(hwa_ring)->read; \
	wr = HWA_RING_STATE(hwa_ring)->write; \
	if (rd <= wr) { \
		va = HWA_RING_ELEM(type, hwa_ring, rd); \
		OSL_CACHE_INV(va, (wr - rd) * sizeof(type)); \
	} else { \
		va = HWA_RING_ELEM(type, hwa_ring, rd); \
		OSL_CACHE_INV(va, ((hwa_ring)->depth - rd) * sizeof(type)); \
		if (wr) { \
			va = HWA_RING_ELEM(type, hwa_ring, 0); \
			OSL_CACHE_INV(va, wr * sizeof(type)); \
		} \
	} \
})
#endif /* !DONGLEBUILD */

// HWA Device exported API
void    hwa_init(hwa_dev_t *dev);                   // INIT PHASE
void    hwa_deinit(hwa_dev_t *dev);                 // HWA DEINIT PHASE
void    hwa_reinit(hwa_dev_t *dev);                 // HWA REINIT PHASE
void    hwa_enable(hwa_dev_t *dev);                 // ENABLE PHASE
void    hwa_rx_enable(hwa_dev_t *dev);              // ENABLE RX
void    hwa_disable(hwa_dev_t *dev);                // DISABLE PHASE

void    hwa_init_axi_addr(hwa_dev_t *dev);
hwa_mem_addr_t hwa_axi_addr(hwa_dev_t *dev, const uint32 hwa_mem_offset);

void    hwa_error(hwa_dev_t *dev);

#ifdef HWA_DPC_BUILD
void    hwa_intrson(hwa_dev_t *dev);
void    hwa_intrsoff(hwa_dev_t *dev);
void    hwa_intrsupd(hwa_dev_t *dev);
uint32  hwa_intstatus(hwa_dev_t *dev);
bool    hwa_dispatch(hwa_dev_t *dev);
bool    hwa_txdma_worklet(hwa_dev_t *dev, uint32 intstatus);
bool    hwa_worklet(hwa_dev_t *dev);
#endif /* HWA_DPC_BUILD */

#if defined(BCMDBG) || defined(HWA_DUMP)
#if defined(WLTEST) || defined(HWA_DUMP)
void    hwa_regs_dump(hwa_dev_t *dev, struct bcmstrbuf *b, uint32 block_bitmap);
void    hwa_reg_read(hwa_dev_t *dev, uint32 reg_offset);
#endif // endif
#endif /* BCMDBG */

#if defined(BCMHWA) && defined(HWA_RXPATH_BUILD)
void    hwa_wl_reclaim_rx_packets(hwa_dev_t *dev);
#endif // endif
void    hwa_wl_flush_all(hwa_dev_t *dev);
void    hwa_wlc_mac_event(hwa_dev_t *dev, uint reason);
int     hwa_wlc_module_register(hwa_dev_t *dev);
#ifdef BCMHME
int     hwa_wl_hme_macifs_upd(hwa_dev_t *dev, dma64addr_t hmeaddr, uint32 hmelen);
#endif // endif

#endif /* _HWA_LIB_H */
