/*
 * Broadcom Full Dongle Host Memory Extension (HME)
 *  Interface to manage Host Memory Extensions.
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
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */

#ifndef _BCMHME_H
#define _BCMHME_H

#include <typedefs.h>
#include <bcmpcie.h>
#include <hndbme.h>

struct osl_info;    // forward decl: osl_decl.h
struct pcie_ipc;    // forward decl: bcmpcie.h

#define HME_NOOP                    do { /* no-op */ } while(0)
#define HME_PRINT                   printf

/** Conditional Compile: Designer builds for extended debug and statistics */
#define HME_DEBUG_BUILD
#define HME_STATS_BUILD

#define HME_CTRS_RDZERO             /* Clear On Read */
#if defined(HME_CTRS_RDZERO)
#define HME_CTR_ZERO(ctr)           (ctr) = 0U
#else
#define HME_CTR_ZERO(ctr)           HME_NOOP
#endif // endif

#if defined(HME_DEBUG_BUILD)
#define HME_ASSERT(expr)            ASSERT(expr)
#define HME_DEBUG(expr)             expr
#else  /* ! HME_DEBUG_BUILD */
#define HME_ASSERT(expr)            HME_NOOP
#define HME_DEBUG(expr)             HME_NOOP
#endif /* ! HME_DEBUG_BUILD */

#if defined(HME_STATS_BUILD)
#define HME_STATS(expr)             expr
#define HME_STATS_ZERO(ctr)         HME_CTR_ZERO(ctr)
#else  /* ! HME_STATS_BUILD */
#define HME_STATS(expr)             HME_NOOP
#define HME_STATS_ZERO(expr)        HME_NOOP
#endif /* ! HME_STATS_BUILD */

/**
 * HME: Memory Management policy.
 *
 * Segment Carving paradigm, is useful for carved allocation of variable sized
 * memory blocks, that persist for the operational duration of the system. This
 * scheme may be used when all blocks are intended to be freed simulataneously,
 * effectively freeing the entire HME memory. E.g. PCIE_IPC_HME_USER_MACIFS.
 *
 * Pool of fixed size memory blocks, with free blocks identified using a
 * dongle resident multi word bitmap.
 *
 * Split-Coalesce support is a WIP. Requires a walk of the free block list in
 * host memory using backplane accesses over PCIE. E.g. PCIE_IPC_HME_USER_LCLPKT
 * Split-Coalesce helps reduce internal and external fragmentation at an
 * exhorbitant cost of CPU cycles for backplane accesses of free-block list.
 * Split-Coalesce paradigm is appropriate when there is a large disparity in
 * typical block-sizes and the sojourn time of allocated blocks.
 *
 * Memory may be allocated from the HME region ONLY after the Dongle and DHD
 * handshake has completed, i.e. the PCIE IPC Link Phase completed.
 *
 */

typedef void * hme_mgr_t;       // HME User Memory Manager opaque handle

/** HME well known User IDs. See PCIE_IPC_HME_USERS_MAX = 8 */
#define HME_USER_SCRMEM    0    // Scratch memory: QT, log, debug, etc
#define HME_USER_HTXHDR    1    // Host based TxHdr (PSM + D11)
#define HME_USER_PKTPGR    1    // PktPgr: Host Tx|Rx BM
#define HME_USER_MACIFS    2    // MACIFs: Tx + Rx FIFO offload
#define HME_USER_LCLPKT    3    // LCLPKT: heap allocated data buffer offload
#define HME_USER_PSQPKT    4    // PSQPKt: PS queue of packets offload
#define HME_USER_UNDEF5    5    // Future use
#define HME_USER_UNDEF6    6    // Future use
#define HME_USER_UNDEF7    7    // Future use

typedef enum hme_policy
{
	HME_MGR_NONE = 0,           // No memory manager
	HME_MGR_SGMT = 1,           // Carve out variable sized segments from front
	HME_MGR_POOL = 2,           // Pool of fixed sized mem blocks
	HME_MGR_SPCL = 3            // Split-Coalesce paradigm: WIP
} hme_policy_t;

/** HME service Initialization. See rte.c :: hnd_init() with backplane osh */
extern int       BCMATTACHFN(hme_init)(struct osl_info *hnd_osh);

/** HME System Debug Dump */
extern void      hme_dump(bool verbose);

/** Allocate memory from HME for a given user */
extern haddr64_t hme_get(int user_id, size_t bytes);

/** Deallocate memory into HME for a given user */
extern int       hme_put(int user_id, size_t bytes, haddr64_t haddr64);

/** Query free and used memory in HME for a given user */
extern int       hme_free(int user_id);
extern int       hme_used(int user_id);

/** Attach a HME User Memory Manager during PCIE IPC Bind phase. */
extern int       BCMATTACHFN(hme_attach_mgr)(int user_id,
                        uint16 pages, uint16 item_size, uint16 items_max,
                        hme_policy_t policy);

/**
 * HME integration into Dongle/Host Bind and Link Phase in pciedev bus layer
 *   + HME Users Memory Extensions are requested using PCIE IPC in Bind Phase
 *   + HME Users Memory Extensions are retrieved from PCIE IPC in Link Phase
 */
extern int      BCMATTACHFN(hme_bind_pcie_ipc)(void *pciedev, struct pcie_ipc *pcie_ipc);
extern int      hme_link_pcie_ipc(void *pciedev, struct pcie_ipc *pcie_ipc);
extern void     hme_dump_pcie_ipc(void *pciedev, struct pcie_ipc *pcie_ipc);

#ifdef HNDBME
/**
 * Library of wrapper routines to leverage ByteMoveEngine based DMA transfers
 * to/from Host Memory Extensions.
 */

/** In HND rte.c, HME registers as BME user BME_USR_H2D and BME_USR_D2H */
extern int BCMATTACHFN(hme_bind_bme)(void); // private to rte.c

/**
 * HME_USER_NOOP may be passed to hme_d2h_xfer or hme_h2d_xfer, to suggest
 * that no HME memory allocation or deallocation operation, is required, and
 * the provided host memory address will be used as-is in the transfer request.
 */
#define HME_USER_NOOP           (-1)

/**
 * hme_d2h_xfer:   Initiate a BME assisted D2H DMA transfer from a dongle memory
 *                 to host memory.
 *                 When a well known HME User Id is specified in hme_user_id, a
 *                 block will be allocated from the users pool allocator, and
 *                 serve as the destination host memory.
 *                 Otherwise, the host memory need not be a HME block.
 *
 * Parameters:
 *    src_daddr32: source dongle memory address
 *    dst_haddr64: pointer to a 64bit destination host memory address
 *                 When hme_user_id == HME_USER_NOOP, a pointer to a valid host
 *                 memory address that may or may not belong to a HME.
 *                 When hme_user_id is a well known HME user id, a HME block
 *                 will be allocated, and its address will be returned through
 *                 this pointer.
 *    len        : Length of the D2H DMA transfer.
 *    sync       : boolean to request a spin loop until xfer completes. User
 *                 may initiate the transfer and later explicitly invoke
 *                 hme_d2h_sync() to sync on transfer completion.
 *   hme_user_id:  HME_USER_NOOP or one of the well known HME User IDs.
 *                 When a well known HME User is specified, the User's memory
 *                 extension must be managed by hme_mgr_pool_t.
 *                 No support for other HME memory managers.
 *
 * Returns:
 *    < 0        : hme not yet initialized
 *                 or no memory, if allocation was requested.
 *      0        : [case sync = TRUE]  BCME_OK: Success
 *    > 0        : [case sync = FALSE] Sync key
 *                 Sync Key to be used in subsequent hme_d2h_sync() invocation
 */

extern int       hme_d2h_xfer(const void *src_daddr32, haddr64_t *dst_haddr64,
                              uint16 len, bool sync, int hme_user_id);

/**
 * hme_h2d_xfer: Initiate a BME assisted H2D DMA transfer from a host memory
 *               to dongle memory. The host memory must be a HME block, if
 *               an explicit deallocation is requested via the parameter
 *               hme_user_id (!= HME_USER_NOOP).
 *
 * Parameters:
 *    src_haddr64: pointer to a 64bit source host memory address
 *    dst_daddr32: destination dongle memory address
 *    len        : Length of the H2D DMA transfer.
 *    sync       : boolean to request a spin loop until xfer completes. User
 *                 may initiate the transfer and later explicitly invoke
 *                 hme_h2d_sync() to sync on transfer completion.
 *
 * Returns:
 *    < 0        : BCME ERROR, BADLEN,
 *      0        : [case sync = TRUE]  BCME_OK: Success
 *    > 0        : [case sync = FALSE] Sync Key
 *                 Sync Key to be used in subsequent hme_d2h_sync() invocation
 */
extern int       hme_h2d_xfer(haddr64_t * src_haddr64, void * dst_daddr32,
                              uint16 len, bool sync, int hme_user_id);

/** Explicitly sync on a previous xfer call, using the returned key. */
extern void      hme_xfer_sync(int cpy_key);

/** Sync on "all" previous ongoing engine xfer transactions. */
extern void      hme_h2d_sync(void);
extern void      hme_d2h_sync(void);

#endif /* HNDBME */

#endif /* _BCMHME_H */
