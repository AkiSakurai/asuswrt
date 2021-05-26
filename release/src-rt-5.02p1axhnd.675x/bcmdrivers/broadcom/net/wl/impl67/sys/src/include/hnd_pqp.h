/*
 * +--------------------------------------------------------------------------+
 *  HND Packet Queue Pager (PQP) Service
 *
 *  Copyright 2020 Broadcom
 *
 *  This program is the proprietary software of Broadcom and/or
 *  its licensors, and may only be used, duplicated, modified or distributed
 *  pursuant to the terms and conditions of a separate, written license
 *  agreement executed between you and Broadcom (an "Authorized License").
 *  Except as set forth in an Authorized License, Broadcom grants no license
 *  (express or implied), right to use, or waiver of any kind with respect to
 *  the Software, and Broadcom expressly reserves all rights in and to the
 *  Software and all intellectual property rights therein.  IF YOU HAVE NO
 *  AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 *  WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *  THE SOFTWARE.
 *
 *  Except as expressly set forth in the Authorized License,
 *
 *  1. This program, including its structure, sequence and organization,
 *  constitutes the valuable trade secrets of Broadcom, and you shall use
 *  all reasonable efforts to protect the confidentiality thereof, and to
 *  use this information only in connection with your use of Broadcom
 *  integrated circuit products.
 *
 *  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 *  "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 *  REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 *  OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 *  DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 *  NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 *  ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *  CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 *  OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 *  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 *  BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 *  SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 *  IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 *  IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 *  ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 *  OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *  NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *  <<Broadcom-WL-IPTag/Proprietary:>>
 *
 *  $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * +--------------------------------------------------------------------------+
 */

#ifndef __hnd_pqp_h_included__
#define __hnd_pqp_h_included__

#include <typedefs.h>

#ifndef PQP_REQ_MAX
#define PQP_REQ_MAX     256     /* 256 paging requests: 10 KBytes */
#endif // endif

/**
 * +--------------------------------------------------------------------------+
 *  Host Memory Extension:
 *  Total memory = (PQP_PKT_SZ * PQP_PKT_MAX) + (PQP_LCL_SZ * PQP_LCL_MAX)
 *               = (256 Bytes  *  8192 Items) + (2048 Bytes *   256 Items)
 *               = (2 MBytes) + (512 KBytes)
 *               = 2.5 MBytes
 * +--------------------------------------------------------------------------+
 */
#ifndef PQP_PKT_SZ
#define PQP_PKT_SZ      256     /* TxLfrag Context + D11 buffer */
#endif // endif
#ifndef PQP_PKT_MAX
#define PQP_PKT_MAX     8192    /* Total packets (w/ D11) in host */
#endif // endif

#ifndef PQP_LCL_SZ
#define PQP_LCL_SZ      2048    /* Management packet's local data buffer size */
#endif // endif
#ifndef PQP_LCL_MAX
#define PQP_LCL_MAX     256    /* Total management local data buffers in host */
#endif // endif

#define PQP_HME_SZ  (((PQP_PKT_MAX * PQP_PKT_SZ) + (PQP_LCL_MAX * PQP_LCL_SZ))

/**
 * +--------------------------------------------------------------------------+
 * PQP Theory of Opearation:
 * +--------------------------------------------------------------------------+
 *
 * hnd_pktq.h defines a single precedence packet queue, namely pktq_prec_t and
 * referred here to as "pktq". A pktq specifies the head packet, the tail packet
 * and number of packets held in the queue. In addition, other attributes like
 * maximum holding capacity and queue operation statistics.
 *
 * Packet Queue Pager, allows all packets held in a pktq to be offloaded
 * to Host Memory using "PAGE-OUT" operation and subsequently retrieved back
 * into Dongle Memory via a "PAGE-IN" operation. Upon a PAGE-IN, the packet may
 * occupy a different memory location and hence each packet's pointers will need
 * to be relocated, specifically Lbuf::<head, end, data, next, link>.
 * Pointers in a PKTTAG are not relocated.
 * Upon a PAGE-OUT, the pktq is deemed managed by the PQP. On a PAGE_OUT the
 * dongle resident packets are released to the free pool. pktq::<head,tail>
 * must not be accessed until a subsequent PAGE-IN has completed.
 * Moreover, the pktq::<head,tail> would likely point to packets in a different
 * location upon PAGE-IN.
 *
 *
 * Asynchronous PAGE OPS:
 * PQP PAGE-OUT and PAGE-IN operations are not run-to-completion and uses the
 * asynchronous, descriptor based mem2mem engines, in the M2MCORE. Users may
 * request a PAGE-OUT operation on a pktq with an accompanied callback
 * to be invoked when the PAGE-OUT eventually completes. The user's callback
 * is responsible for freeing all packets in the original pktq.
 *
 *
 * A "D11 PACKET is a 802.11 MPDU" i.e. a PKTNEXT linked list of Lfrags(MSDUs).
 * A QUEUE is essentially a Power Save Queue (or a Suppression PSQ) represented
 *    as a PKTLINK linked list of MPDUs with MSDUs encoded in a PKTNEXT linked
 *    list of Lfrags (lfrag_info may hold more than one MSDU's host info).
 *
 * Packets managed by PSQ and SuprPSQ are 802.11 formatted.
 *    PSQ      = D11 Packets are extracted from Common Queue. The PSQ is
 *               built in entirety before page-out. i.e. No append to tail.
 *    SuprPSQ  = D11 Packets suppressed via TxStatus. On each TxStatus, a list
 *               suppressed D11 packets are appended to the SuprPSQ.
 *               Unlike a PSQ, a SuprPSQ may be incrementally constructed with
 *               append to tail of a new set of D11 packets.
 *    On exit of PowerSave, all D11 packets in PSQ are appended to the
 *    head of PSQ to retain order.
 *
 * Each PSQ and SuprPSQ are individually (independently) tracked by PQP and
 * consume one PQP Request resource. A PQP Request is not exported.
 *
 * Resources:
 * ----------
 *  - M2M dma descriptors in channel #0 (PAGE-OUT) and channel #1 (PAGE-IN,CLR).
 *  - PAGE-OUT transaction: Host Packet and/or LCL data buffer
 *  - PAGE-IN  transaction: HWA TxBM Packet + D11 and/or LCL data buffer
 *                          HWA 2.3: TxBM Packet includes storage for D11.
 *
 * Caveats:
 * --------
 *  - PQP is intended for paging out entire Power Save Queues.
 *  - To support append-to-tail mode for SuprPSQ, the tail packet is not paged.
 *
 */

// TBD. Asynchronous Resume Callbacks upon Resource availability
// - callback when HWA TxBM LFrag / D11 become available for PAGE_IN
// - callback when LCL become available.
//   With HWA2.3 PktPgr, can we retain LCL in Host .. would PP try to copy H2H?

typedef struct lbuf      hndpkt_t;  /* hnd_lbuf.h */
typedef struct pktq_prec hndpktq_t; /* hnd_pktq.h */

typedef enum pqp_op {   /* OPERATION  PKT_CB QUE_CB OPERATION-MODE */
	PQP_OP_NOP  = 0,
	PQP_OP_PGO  = 1,    /* PAGE-OUT    Yes    Yes   Asynchronous */
	PQP_OP_PGI  = 2,    /* PAGE-IN     No     Yes   Asynchronous */
	PQP_OP_MAX  = 3
} pqp_op_t;

/**
 * +--------------------------------------------------------------------------+
 *  User specified per D11 Packet and Queue Pager-OP Callback
 * +--------------------------------------------------------------------------+
 *
 *  + pqp_pcb_t : PACKET Callback invoked on PAGE-OUT per D11 PACKET
 *                D11 Packet is a list of hndpkt(s) linked using PKTNEXT
 *  + pqp_qcb_t : QUEUE  Callback invoked on PAGE-IN of Queue
 */
typedef void (*pqp_pgo_pcb_t)(hndpktq_t *pktq, hndpkt_t *pkt);
typedef void (*pqp_pgi_qcb_t)(hndpktq_t *pktq);

/**
 * +--------------------------------------------------------------------------+
 *  PQP Helper Routine
 * +--------------------------------------------------------------------------+
 *
 *  + pqp_pop()     : Query whether a pktq_prec_t has an ongoing page operation
 *                    i.e. managed by PQP.
 *                    Returns PQP_OP_NOP, if the pktq is not PQP managed.
 */
pqp_op_t pqp_pop(hndpktq_t *pktq);

/**
 * +--------------------------------------------------------------------------+
 *  PQP Functional Interface (Service Debug and Service Initialization)
 * +--------------------------------------------------------------------------+
 *
 *  + pqp_dump()    : Debug dump the internal PQP runtime state
 *  + pqp_stats()   : Debug dump the internal PQP runtime statistics
 *
 *  + pqp_fini()    : Detach the PQP subsystem
 *  + pqp_init()    : Initialize the PQP subsystem with max-queues capability
 *
 *  + pqp_bind_hme(): Bind to HME segments: specify storage requirement in pages
 *  + pqp_link_hme(): Link to HME segments: fetch storage addresses
 *
 * +--------------------------------------------------------------------------+
 */
void pqp_dump(bool verbose);
void pqp_stats(void);

int  BCMATTACHFN(pqp_fini)(osl_t *osh);
int  BCMATTACHFN(pqp_init)(osl_t *osh, int pqp_req_max);

int  pqp_bind_hme(void);
int  pqp_link_hme(void);

/**
 * +--------------------------------------------------------------------------+
 *  PQP PAGE OUT Operational Interface
 * +--------------------------------------------------------------------------+
 *
 *  PAGE_OUT, PAGE_IN Implementation Caveats (necessitated by memory)
 *  +  pktq::<head,tail> are re-purposed by PQP (pktq_prec is not extended).
 *  +  pktq::<head,tail,n_pkts> must be NULL on first invocation of the pqp_pgo.
 *     Caller needs to explicitly zero out pktq::<head,tail,n_pkts> on first
 *     invocation of a pqp_pgo()
 *  +  Upon invoking a PGO operation on a pktq, the pktq::<head,tail>
 *     may not be directly accessed.
 *  +  A PSQ may be incrementally built by appending to tail a new packet list
 *     <head,tail, n_pkts> of suppressed packets to a previously PAGED OUT pktq.
 *  +  SupprPSQ do not have a requirement to append to tail.
 *
 *  Failure to accept a PAGE-OUT request, a negative BCME_<error> is returned.
 *  A PQP request will be asynchronously serviced upon resource availability,
 *  and the pqp_pgo_pcb_t and pqp_pgi_qcb_t callbacks are used to notify caller
 *  that a D11 Packet has been PAGED out or an entire Queue has been PAGED in.
 *
 * +--------------------------------------------------------------------------+
 */
int  pqp_pgo(hndpktq_t *pktq, hndpkt_t *head, hndpkt_t *tail, int n_pkts,
             pqp_pgo_pcb_t pcb, pqp_pgi_qcb_t qcb, bool append_policy);

int  pqp_pgi(hndpktq_t *pktq);

#endif /* __hnd_pqp_h_included__ */
