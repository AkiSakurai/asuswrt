/*
 * PCIEDEV device API
 * pciedev is the bus device for pcie full dongle
 * Core bus operations are as below
 * 1. interrupt mechanism
 * 2. circular buffer
 * 3. message buffer protocol
 * 4. DMA handling
 * pciedev.h exposes pciedev functions required outside
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
 * $Id: pciedev.h  $
 */

#ifndef	_pciedev_h_
#define	_pciedev_h_

#include <typedefs.h>
#include <osl.h>

/*
 * Latency Tolerance Reporting (LTR) time tolerance
 */
#define LTR_LATENCY_60US		60	/* unit in usec */
#define LTR_LATENCY_100US		100	/* unit in usec */
#define LTR_LATENCY_140US		140	/* unit in usec */
#define LTR_LATENCY_150US		150	/* unit in usec */
#define LTR_LATENCY_200US		200	/* unit in usec */
#define LTR_LATENCY_300US		300	/* unit in usec */
#define LTR_LATENCY_3MS			3	/* unit in msec */

/* Unit of latency stored in LTR registers
 * 2 => latency in microseconds
 * 4 => latency in milliseconds
 */
#define LTR_SCALE_US			2
#define LTR_SCALE_MS			4

/* Chip operations */
struct dngl_bus *pciedev_attach(void *drv, uint vendor, uint device, osl_t *osh,
	volatile void *regs, uint bus);

#ifdef UART_TRAP_DBG
void pciedev_dump_pcie_info(uint32 arg);
void pciedev_trap_from_uart(uint32 arg);
bool pciedev_uart_debug_enable(struct dngl_bus *pciedev, int enab);
void ai_dump_APB_Bridge_registers(si_t *sih);
#endif // endif

void pciedev_sr_stats(struct dngl_bus *pciedev, bool arm_wakeup);
uint32 pciedev_flow_ring_flush_pending(struct dngl_bus * pciedev, char* buf, uint32 inlen,
	uint32 *outlen);
#if defined(PCIE_DMA_INDEX) && defined(SBTOPCIE_INDICES)
extern void pciedev_sync_h2d_read_ptrs(struct dngl_bus * pciedev, msgbuf_ring_t* ring);
#endif /* PCIE_DMA_INDEX & SBTOPCIE_INDICES */

/* Exported APIs/Macros */
struct dngl_bus* pciedev_get_handle(void);
void *pciedev_dngl(struct dngl_bus *pciedev);
void pciedev_xmit_txstatus(struct dngl_bus *pciedev);
void pciedev_xmit_rxcomplete(struct dngl_bus *pciedev);
uint32 pcie_h2ddma_tx_get_burstlen(struct dngl_bus *pciedev);
uint32 pcie_h2ddma_rx_get_burstlen(struct dngl_bus *pciedev);

#ifdef BCMHWA
#ifdef HWA_RXPATH_BUILD
int pciedev_hwa_queue_rxcomplete_fast(struct dngl_bus *pciedev, uint32 pktid);
void pciedev_hwa_flush_rxcomplete(struct dngl_bus *pciedev);
#endif /* HWA_RXPATH_BUILD */

#ifdef HWA_TXPOST_BUILD
/* Datastructure filled by BUS layer for HWA to handle CFP enqueue */
typedef struct hwa_cfp_tx_info {
	bool cfp_capable;	/* CFP Capable List */
	uint8 pktlist_prio;	/* Packet list priority */
	uint16 cfp_flowid;	/* Packet list CFP Flow ID */
	uint32 expiry_time;	/* Expiry time read from TSF */
	uint32 pktlist_count;	/* Packet count in the list */
	void* pktlist_tail;	/* Tail packet in the list */
	void* pktlist_head;	/* Head packet in the list */
} hwa_cfp_tx_info_t;

#ifdef WLCFP
/* Check for CFP capable given a HWA packet chain */
extern void pciedev_hwa_cfp_tx_enabled(struct dngl_bus *pciedev, uint32 flowid,
	uint16 eth_type, uint8 prio, hwa_cfp_tx_info_t *hwa_cfp_tx_info);
#endif /* WLCFP */
/* Check flowring status for suppress pending or flush pending or INVALID CFP_FLOWID */
uint16  pciedev_flowring_state_get(struct dngl_bus *pciedev, uint16 flowid);
void pciedev_hwa_txpost_pkt2native(struct dngl_bus *pciedev, void *head,
	uint32 pkt_count, uint32 total_octets, hwa_cfp_tx_info_t *hwa_cfp_tx_info,
	void **pktcs, uint32 *pktc_cnts);
void pciedev_hwa_process_pending_flring_resp(struct dngl_bus *pciedev, uint16 ringid);
#ifdef WLSQS
void pciedev_sqs_v2r_dequeue(struct dngl_bus *pciedev, uint16 ringid, uint8 prio,
	uint16 pkt_count, bool sqs_force);
#endif /* WLSQS */
#endif /* HWA_TXPOST_BUILD */

#ifdef HWA_TXCPLE_BUILD
void hwa_upd_last_queued_flowring(struct dngl_bus * pciedev, uint16 ringid);
void hwa_sync_flowring_read_ptrs(struct dngl_bus *pciedev);
#endif /* HWA_TXCPLE_BUILD */
#endif /* BCMHWA */

#if defined(BCMHWA) && defined(HWA_RXCPLE_BUILD)
#define PCIEDEV_RXCPL_RESOURCE_AVAIL_CHECK(pciedev) \
	(hwa_rxcple_resource_avail_check(hwa_dev))
#define PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev) \
	(hwa_rxcple_pend_item_cnt(hwa_dev))
#define PCIEDEV_XMIT_RXCOMPLETE(pciedev) \
	hwa_rxcple_commit(hwa_dev)
#else /* !(BCMHWA && HWA_RXCPLE_BUILD) */
#define PCIEDEV_RXCPL_RESOURCE_AVAIL_CHECK(pciedev) \
	(pciedev_resource_avail_check(pciedev, (pciedev)->dtoh_rxcpl))
#define PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev) \
	((pciedev)->dtoh_rxcpl->buf_pool->pend_item_cnt)
#define PCIEDEV_XMIT_RXCOMPLETE(pciedev) \
	pciedev_xmit_rxcomplete(pciedev)
#endif /* !(BCMHWA && HWA_RXCPLE_BUILD) */

#if defined(BCMHWA) && defined(HWA_TXCPLE_BUILD)

#if defined(HWA_PKTPGR_BUILD)
#define PCIEDEV_TXCPL_RESOURCE_AVAIL_CHECK(pciedev, p) \
	(hwa_txcple_resources_avail_check(hwa_dev, p))
/* pciedev_queue_txstatus must be successful because of PCIEDEV_TXCPL_RESOURCE_AVAIL_CHECK */
#define PCIEDEV_QUEUE_MULTIPLE_TXSTATUS(pciedev, pktid, ifindx, ringid, txstatus, ts, p) \
	pciedev_queue_multiple_txstatus(pciedev, pktid, ifindx, ringid, txstatus, ts, p)
#define PCIEDEV_UPDATE_MULTIPLE_TXSTATUS(pciedev, status, rindex, flowid, seq, hold, kseq, p) \
	pciedev_update_multiple_txstatus(pciedev, status, rindex, flowid, seq, hold, kseq, p)
#else
#define PCIEDEV_TXCPL_RESOURCE_AVAIL_CHECK(pciedev, p) \
	(hwa_txcple_resource_avail_check(hwa_dev))
#define PCIEDEV_QUEUE_MULTIPLE_TXSTATUS(pciedev, pktid, ifindx, ringid, txstatus, ts, p) \
	(PCIEDEV_QUEUE_TXSTATUS(pciedev, pktid, ifindx, ringid, txstatus, ts))
#define PCIEDEV_UPDATE_MULTIPLE_TXSTATUS(pciedev, status, rindex, flowid, seq, hold, kseq, p) \
	pciedev_update_txstatus(pciedev, status, rindex, flowid, seq, hold, kseq)
#endif /* HWA_PKTPGR_BUILD */

#define PCIEDEV_TXCPL_RESOURCE_AVAILABLE(pciedev, p) \
	(PCIEDEV_TXCPL_RESOURCE_AVAIL_CHECK(pciedev, p))
#define PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev) \
	(hwa_txcple_pend_item_cnt(hwa_dev))
#define PCIEDEV_QUEUE_TXSTATUS(pciedev, pktid, ifindx, ringid, txstatus, ts) \
	({BCM_REFERENCE(txstatus); BCM_REFERENCE(ts); \
	hwa_txcple_wi_add(hwa_dev, pktid, ringid, ifindx);})
#define PCIEDEV_XMIT_TXSTATUS(pciedev) \
	hwa_txcple_commit(hwa_dev)

#else /* !(BCMHWA && HWA_TXCPLE_BUILD) */

#define PCIEDEV_TXCPL_RESOURCE_AVAIL_CHECK(pciedev, p) \
	(pciedev_resource_avail_check(pciedev, (pciedev)->dtoh_txcpl))
#define PCIEDEV_TXCPL_RESOURCE_AVAILABLE(pciedev, p) \
	(PCIEDEV_TXCPL_RESOURCE_AVAIL_CHECK(pciedev, p) && \
	!IS_TXSTS_PENDING(pciedev))
#define PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev) \
	((pciedev)->dtoh_txcpl->buf_pool->pend_item_cnt)
#define PCIEDEV_QUEUE_TXSTATUS(pciedev, pktid, ifindx, ringid, txstatus, ts) \
	pciedev_queue_txstatus(pciedev, pktid, ifindx, ringid, txstatus, ts)
#define PCIEDEV_QUEUE_MULTIPLE_TXSTATUS(pciedev, pktid, ifindx, ringid, txstatus, ts, p) \
	(PCIEDEV_QUEUE_TXSTATUS(pciedev, pktid, ifindx, ringid, txstatus, ts))
#define PCIEDEV_XMIT_TXSTATUS(pciedev) \
	pciedev_xmit_txstatus(pciedev)
#define PCIEDEV_UPDATE_MULTIPLE_TXSTATUS(pciedev, status, rindex, flowid, seq, hold, kseq, p) \
	pciedev_update_txstatus(pciedev, status, rindex, flowid, seq, hold, kseq)

#endif /* !(BCMHWA && HWA_TXCPLE_BUILD) */

#ifdef PCIEDEV_FAST_DELETE_RING
#if defined(HWA_PKTPGR_BUILD)
#define PCIEDEV_FASTDELETE_NOTXSTATUS(pciedev, flow_ring, rindex, txstatus, ts, p) \
	pciedev_fastdelete_queue_txstatus(pciedev, flow_ring, rindex, txstatus, ts, p)
#else
#define PCIEDEV_FASTDELETE_NOTXSTATUS(pciedev, flow_ring, rindex, txstatus, ts, p) \
	({BCM_REFERENCE(pciedev); BCM_REFERENCE(txstatus); \
	BCM_REFERENCE(ts); BCM_REFERENCE(p); \
	pciedev_fastdelete_notxstatus(flow_ring, rindex);})
#endif /* HWA_PKTPGR_BUILD */
#endif /* PCIEDEV_FAST_DELETE_RING */

typedef enum bcmpcie_ipc_path {
	BCMPCIE_IPC_PATH_CONTROL  = 0, /* Control  path transactions */
	BCMPCIE_IPC_PATH_RECEIVE  = 1, /* Receive  path transactions */
	BCMPCIE_IPC_PATH_TRANSMIT = 2, /* Transmit path transactions */
	BCMPCIE_IPC_PATH_MAX      = 3
} bcmpcie_ipc_path_t;

typedef enum bcmpcie_ipc_trans {
	BCMPCIE_IPC_TRANS_REQUEST  = 0,  /* H2D request  ring's transaction id */
	BCMPCIE_IPC_TRANS_RESPONSE = 1, /* D2H response ring's transaction id */
	BCMPCIE_IPC_TRANS_ROLLBACK = 2, /* TxPost flowring requests rollbacks */
	BCMPCIE_IPC_TRANS_MAX = 3
} bcmpcie_ipc_trans_t;

/** PCIE IPC Host PacketId Audit */
/**
 * Audit pktid received from host.
 *
 * HOST_PKTID_AUDIT is not defined within the PCIE IPC specification. IPC does
 * not enforce a semantics on the values used in a closed transaction's
 * "request_id" exchanged between a request and a response. Host side driver is
 * permitted to use any mapping of a transaction id. The transaction Id may be
 * global scoped or per control|rx|tx paths. The transaction Id need not be a
 * value between [0..N] and could be a pointer to a packet or a disjoint map.
 * Dongle firmware carries a 32bit transaction Id opaquely from a request to
 * a response. Often, it may be required to debug transaction id corruptions.
 *
 * Disclaimers:
 *
 * Dongle Host Driver built with DHD_PCIE_PKTID, typically derive a request_id
 * using a packet pointer to a PKTID Mapper, with a PKTID passed as request_id.
 * PKTID values would be in a range [1..MAX_PKTID_ITEMS). Packets used for all
 * transactions (control, receive and transmit), share a single PKTID mapper.
 * PCIE IPC does not govern nor provides an ability for DHD to convey the PKTID
 * allocation policy. Hence the following need to be explicitly matched:
 *     BCMPCIE_IPC_HPA_PKTID_TOTAL must match dhd_msgbuf.c MAX_PKTID_ITEMS
 * PktId stored in PCIE IPC Common message header is assumed to be in LENDIAN.
 *
 * As new forms of host side transaction id formats are introduced, HPA support
 * for these forms would need to be designed in.
 *     E.g. DoR support for FPM/BPM/SKB/FKB/CPUTX.
 *
 * HPA may be moved into a top level library. Currently in pcie bus layer.
 */

#define BCMPCIE_IPC_HPA_DHD_PCIE_PKTID      /* DHD built with DHD_PCIE_PKTID */
#define DHD_MSGBUF_MAX_PKTID_ITEMS          ((36 * 1024) - 1) /* dhd_msgbuf.c */

#define BCMPCIE_IPC_HPA_PKTID_TOTAL         (DHD_MSGBUF_MAX_PKTID_ITEMS)

#define BCMPCIE_IPC_HPA_MWBMAP_ITEMS        (8 * 1024) /* power of 2 */
#define BCMPCIE_IPC_HPA_MWBMAP_INDEX(pktid) ((pktid) >> 13)     /* 8K */
#define BCMPCIE_IPC_HPA_MWBMAP_BITIX(pktid) ((pktid) & 0x1fff)  /* 0 .. 8K-1 */

#define BCMPCIE_IPC_HPA_MWBMAP_TOTAL \
	(ROUNDUP(BCMPCIE_IPC_HPA_PKTID_TOTAL, BCMPCIE_IPC_HPA_MWBMAP_ITEMS) \
		/ BCMPCIE_IPC_HPA_MWBMAP_ITEMS)

#define BCMPCIE_IPC_HPA_PKTID_ERR_LOG       /* Enable history logging */
#define BCMPCIE_IPC_HPA_PKTID_ERR_MAX       (16)

#if defined(BCMPCIE_IPC_HPA_PKTID_ERR_LOG)
#define BCMPCIE_IPC_HPA_PKTID_ERR(hpa, pktid) \
	(hpa)->log[(hpa)->errors++ % BCMPCIE_IPC_HPA_PKTID_ERR_MAX] = pktid;
#else
#define BCMPCIE_IPC_HPA_PKTID_ERR(hpa, host_pktid)  (hpa)->errors++
#endif // endif

#if defined(BCMPCIE_IPC_HPA)

#if !defined(BCMPCIE_IPC_HPA_DHD_PCIE_PKTID)
#error "HPA supported only for DHD compiled with DHD_PCIE_PKTID (BuPC STA)"
#endif // endif

typedef struct bcmpcie_ipc_hpa_stats {
	uint32 test[BCMPCIE_IPC_TRANS_MAX]; /* Request, Response, Rollback */
	uint32 fail[BCMPCIE_IPC_TRANS_MAX]; /* Request, Response, Rollback */
} bcmpcie_ipc_hpa_stats_t;

typedef struct bcmpcie_ipc_hpa {
	void *pcie_hndl;
	struct bcm_mwbmap *mwbmap[BCMPCIE_IPC_HPA_MWBMAP_TOTAL];
	/* Host Pktid Audit statistics [ctrl,rx,tx] */
	uint32 errors;
	bcmpcie_ipc_hpa_stats_t stats[BCMPCIE_IPC_PATH_MAX];
#ifdef BCMPCIE_IPC_HPA_PKTID_ERR_LOG
	uint32 log[BCMPCIE_IPC_HPA_PKTID_ERR_MAX];
#endif /* BCMPCIE_IPC_HPA_PKTID_ERR_LOG */
} bcmpcie_ipc_hpa_t;

#define BCMPCIE_IPC_HPA_NULL			    ((bcmpcie_ipc_hpa_t *)NULL)

/** Host PktId Audit implementation in pciedev bus layer (covers HWA) */
bcmpcie_ipc_hpa_t *bcmpcie_ipc_hpa_init(osl_t *osh);
void bcmpcie_ipc_hpa_dump(bcmpcie_ipc_hpa_t *hpa);
void bcmpcie_ipc_hpa_trap(bcmpcie_ipc_hpa_t *hpa, uint32 pktid,
	const bcmpcie_ipc_path_t hpa_path, const bcmpcie_ipc_trans_t hpa_trans);
void bcmpcie_ipc_hpa_test(struct dngl_bus *pciedev, uint32 pktid,
	const bcmpcie_ipc_path_t hpa_path, const bcmpcie_ipc_trans_t hpa_trans);
#endif   /* BCMPCIE_IPC_HPA */

#if defined(BCMPCIE_IPC_HPA)

#define PCIEDEV_TO_HPA(pciedev)     ((bcmpcie_ipc_hpa_t *)((pciedev)->hpa_hndl))

#define BCMPCIE_IPC_HPA_INIT(osh)           bcmpcie_ipc_hpa_init(osh)

#define BCMPCIE_IPC_HPA_DUMP(pciedev) \
	bcmpcie_ipc_hpa_dump(PCIEDEV_TO_HPA(pciedev))

#define BCMPCIE_IPC_HPA_TRAP(pciedev, pktid, hpa_path, hpa_trans) \
	bcmpcie_ipc_hpa_trap(PCIEDEV_TO_HPA(pciedev), pktid, hpa_path, hpa_trans)

#define BCMPCIE_IPC_HPA_TEST(pciedev, pktid, hpa_path, hpa_trans) \
	bcmpcie_ipc_hpa_test(pciedev, pktid, hpa_path, hpa_trans)

#else  /* ! BCMPCIE_IPC_HPA */
#define BCMPCIE_IPC_HPA_INIT(b)             ((void *)NULL)
#define BCMPCIE_IPC_HPA_DUMP(b)             do { /* no-op */ } while (0)
#define BCMPCIE_IPC_HPA_TRAP(b, i, p, t)    do { /* no-op */ } while (0)
#define BCMPCIE_IPC_HPA_TEST(b, i, p, d)    do { /* no-op */ } while (0)
#endif /* ! BCMPCIE_IPC_HPA */

/* Put the packet back to flow ring and rewind the fetch pointer */
void pciedev_lfrag_suppress_to_host(struct dngl_bus *pciedev, uint16 flowid, void* lfrag);

/* Flow ring state exchanged between pciedev and HWA after the fetch complete */
typedef enum flow_ring_state
{
	FLOW_RING_STATE_ACTIVE		= 0,
	FLOW_RING_STATE_SUPPRESS_PEND	= 1,
	FLOW_RING_STATE_FLUSH_PEND	= 2
} flow_ring_stae_t;

#endif	/* _pciedev_h_ */
