/*
 * PCIEDEV timing critical datapath functions
 * compiled for performance rather than size
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: pciedev_fastpath.c  $
 */

#include <typedefs.h>
#include <osl.h>
#include <pciedev_priv.h>
#include <pciedev.h>
#include <pciedev_dbg.h>
#include <event_log.h>
#include <wlfc_proto.h>
#include <msgtrace.h>
#include <logtrace.h>

/* Static definitions */
typedef int (*d2h_msg_handler)(struct dngl_bus *pciedev, void *p);

/* Static functions */
static bool pciedev_read_host_buffer(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf);
static void pciedev_schedule_flow_ring_read_buffer(struct dngl_bus *pciedev);
static void pciedev_flowring_fetch_cb(struct fetch_rqst *fr, bool cancelled);
static int8 pciedev_allocate_flowring_fetch_rqst_entry(struct dngl_bus *pciedev);
static void pciedev_free_flowring_fetch_rqst_entry(struct dngl_bus *pciedev, uint8 index);
static void pciedev_adjust_flow_fetch_ptr(msgbuf_ring_t *flow_ring, uint16 index);
static void pciedev_update_rdp_ptr_unacked(msgbuf_ring_t *flow_ring);
static int pciedev_update_txstatus(struct dngl_bus *pciedev, uint32 status,
	uint16 ring_idx, uint16 flowid);
static void pciedev_free_lclbuf_pool(msgbuf_ring_t * ring, void* p);
static int pciedev_get_host_addr(struct dngl_bus * pciedev, uint32* bufid, uint16 * len,
	dma64addr_t *haddr, dma64addr_t *meta_addr, uint16 *meta_len);
static void pciedev_add_to_inuselist(struct dngl_bus *pciedev, void* p, uint8 max_items);
static void pciedev_process_rxpost_msg(void* p, uint32* bufid, uint16 * len, dma64addr_t *haddr,
	dma64addr_t *haddr_meta, uint16 *metadata_len);
static int pciedev_htoddma_queue_avail(struct dngl_bus *pciedev);
static void pciedev_enque_fetch_cmpltq(struct dngl_bus *pciedev, struct fetch_rqst *fr);
static struct fetch_rqst * pciedev_deque_fetch_cmpltq(struct dngl_bus *pciedev);
static void* pciedev_get_src_addr(msgbuf_ring_t * ring, uint16* available_len, uint16 max_len);
static void pciedev_ring_update_readptr(msgbuf_ring_t *ring, uint16 bytes_read);
static void pciedev_ring_update_writeptr(msgbuf_ring_t *ring, uint16 bytes_written);
static uint32 pciedev_get_ring_space(struct dngl_bus *pciedev,
	msgbuf_ring_t *msgbuf, uint16 msglen);
static bool pciedev_resource_avail_for_txmetadata(struct dngl_bus *pciedev);
static bool pciedev_check_process_d2h_message(struct dngl_bus *pciedev, uint32 txdesc,
	uint32 rxdesc, void *p, d2h_msg_handler* rtn);
static int pciedev_process_d2h_rxpyld(struct dngl_bus *pciedev, void *p);
static int pciedev_process_d2h_txmetadata(struct dngl_bus *pciedev, void *p);
static uint16 pciedev_htoddma_deque(struct dngl_bus *pciedev, msgbuf_ring_t **msgbuf,
	uint8 *msgtype);
static void pciedev_queue_rxcomplete_msgring(struct dngl_bus *pciedev,  msgbuf_ring_t *ring);
static int pciedev_create_d2h_messages(struct dngl_bus *bus, void *p, msgbuf_ring_t *msgbuf);
static dma64addr_t pciedev_get_haddr_from_lfrag(struct dngl_bus *pciedev, void* p, uint32* bufid,
	dma64addr_t *haddr_meta, uint16 *metadata_len, uint16 *dataoffset);
static void pciedev_xmit_txstatus(struct dngl_bus *pciedev, msgbuf_ring_t *msgring);
static void pciedev_queue_txstatus(struct dngl_bus *pciedev, uint32 pktid,
	uint8 ifindx, uint16 ringid, uint16 txstatus, uint16 metadata_len, msgbuf_ring_t *ring);
static void pciedev_xmit_rxcomplete(struct dngl_bus *pciedev, msgbuf_ring_t *ring);
static int pciedev_tx_msgbuf(struct dngl_bus *bus, void *p, ret_buf_t *ret_buf, uint16 msglen,
	msgbuf_ring_t *msgbuf);
static uint16 pciedev_handle_h2d_pyld(struct dngl_bus *pciedev, uint8 msgtype, void *p,
	uint16 pktlen, msgbuf_ring_t *msgbuf);
static void pciedev_process_tx_post(struct dngl_bus *bus, void* p, uint16 msglen,
	uint16 ring, uint16 fetch_idx);
static void pciedev_h2dmsgbuf_dma(struct dngl_bus *pciedev, dma64addr_t src, uint16 src_len,
	uint8 *dest, uint8 *dummy_rxoff, msgbuf_ring_t *msgbuf, uint8 msgtype);
static void pciedev_push_pkttag_tlv_info(struct dngl_bus *pciedev, void* p,
	msgbuf_ring_t *flow_ring, uint16 index);

#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
static inline void pciedev_host_pktid_audit(struct bcm_mwbmap * handle,
	uint32 pktid, const bool alloc);

static inline void
pciedev_host_pktid_audit(struct bcm_mwbmap * handle, uint32 pktid, const bool alloc)
{
	if (handle == NULL) {
		PCI_ERROR(("%s Host PktId Audit: Handle NULL\n", __FUNCTION__));
		return;
	}

	if ((pktid == 0) || (pktid > PCIEDEV_HOST_PKTID_AUDIT_MAX)) {
		PCI_ERROR(("%s Host PktId Audit: Invalid pktid<%d>\n",
		           __FUNCTION__, pktid));
		return;
	}

	if (alloc) {
		if (!bcm_mwbmap_isfree(handle, pktid)) {
			PCI_ERROR(("%s ERRIR: Host Pktid<%d> is not free. recv duplicate\n",
			           __FUNCTION__, pktid));
			return;
		}
		bcm_mwbmap_force(handle, pktid);
	} else {
		if (bcm_mwbmap_isfree(handle, pktid)) {
			PCI_ERROR(("%s ERROR: Host Pktid<%d> is freed. send duplicate\n",
			           __FUNCTION__, pktid));
			return;
		}
		bcm_mwbmap_free(handle, pktid);
	}
}
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */

static void pciedev_manage_h2d_dma_clocks(struct dngl_bus *pciedev);
/* Handler for message buffer update interrupts from host */
/* Decode host read/write pointers and schedule DMA to pull down messages */
bool
pciedev_msgbuf_intr_process(struct dngl_bus *pciedev)
{
	/* Host->Dongle --> DoorBell Rang
	 * Need to read/dma the msgbuf from the host into the local
	 * circular buffer (which is also another circular buf)

	* Doorbell intr can be triggered either due to
	* data written to data ring or control ring on Host side
	* Need to check both data ring and control ring on the host,
	* but give first preference to the control ring

	 * MsgBuf is a generic implementation and does
	 * not get into how to read and where to read into. We need to provide the core_read
	 * routine (for eg. htod_msgbuf_read_handler) and ctx which is the pciedev in our case
	 */
	if (pciedev->common_rings_attached == FALSE) {
		if (pciedev_init_sharedbufs(pciedev) != 0)
			return FALSE;
		if (!pciedev_init_flowrings(pciedev))
			return FALSE;

		pciedev->common_rings_attached = TRUE;
	}

	if (pciedev->ioctl_lock == FALSE) {
		pciedev_read_host_buffer(pciedev, pciedev->htod_ctrl);
	}
	/* the above call could change the ioctl lock status */
	if (pciedev->ioctl_lock == FALSE) {
		pciedev_read_host_buffer(pciedev, pciedev->htod_rx);
#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
		pciedev_read_host_buffer(pciedev, pciedev->htod_tx);
#else
		pciedev_schedule_flow_ring_read_buffer(pciedev);
#endif /* BCMPCIE_SUPPORT_TX_PUSH_RING */
	}

	return FALSE;
}

static bool
pciedev_read_host_buffer(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf)
{
#ifdef PCIE_DEBUG_CYCLE_COUNT
	static uint32 cbuf_time = 0, tot_time = 0;
	static int iter = 0;
	uint32 start_time = osl_getcycles();
#endif /* PCIE_DEBUG_CYCLE_COUNT */

	uint8 *dest_addr;
	uint8 *src_addr;
	uint16 src_len;

	if (pciedev->in_d3_suspend) {
		return FALSE;
	}

	while(1){
		/* First check if there is any data available in the circular buffer */
		if (READ_AVAIL_SPACE(DNGL_RING_WPTR(msgbuf), msgbuf->rd_pending,
			RING_MAX_ITEM(msgbuf)) == 0)
			return FALSE;

		/* Before proceeding with circular buffer, check if enough descrs are available */
		/* minimum of 2 desc are required per messages */
		/* another 2 are required for ioctl request */
		if (PCIEDEV_GET_AVAIL_DESC(pciedev, HTOD, RXDESC) < MIN_RXDESC_AVAIL) {
			PCI_TRACE(("rxdesc not avialable. Cur count %d  \n",
				PCIEDEV_GET_AVAIL_DESC(pciedev, HTOD, RXDESC)));
			return TRUE;
		}

		if (!LCL_BUFPOOL_AVAILABLE(msgbuf)) {

			PCI_TRACE(("Ring: %c%c%c%c Local ring bufs not available,"
				"Dont read out from host ring \n",
				msgbuf->name[0], msgbuf->name[1],
				msgbuf->name[2], msgbuf->name[3]));
			return TRUE;
		}

		/* Update the local copy of the write/end ptr with the shared register's value */
		src_addr = pciedev_get_src_addr(msgbuf, &src_len, 0);
		if (src_addr == NULL) {
			return FALSE;
		}

		/* Get from pool of lcl buffers */
		dest_addr = pciedev_get_lclbuf_pool(msgbuf);
		if (dest_addr != NULL) {
			/* Now we have the src details as well the dest details to copy/dma the data
			 * from htod_msgbuf into the local_htod msgbuf. Schedule the DMA now.
			 */
			dma64addr_t haddr;

			pciedev->msg_pending++;

			PHYSADDR64HISET(haddr,
				(uint32) HIGH_ADDR_32(msgbuf->ringmem->base_addr));
			PHYSADDR64LOSET(haddr, (uint32) src_addr);

			pciedev_h2dmsgbuf_dma(pciedev, haddr, src_len, dest_addr,
				pciedev->dummy_rxoff, msgbuf, MSG_TYPE_API_MAX_RSVD);
		} else {

			PCI_TRACE(("Ring: Local ring get failed %d \n",	__LINE__));
			return TRUE;
		}
	}

	return FALSE;
}

/* If we have pending flush and need to drain more, fetch those; */
/* if there are pending flush/delete response then send them now */
static void
pciedev_process_pending_flring_resp(struct dngl_bus * pciedev, msgbuf_ring_t *flow_ring)
{
	if ((flow_ring->status & FLOW_RING_FLUSH_PENDING) &&
		!(flow_ring->status & FLOW_RING_NOFLUSH_TXUPDATE)) {
		if ((DNGL_RING_WPTR(flow_ring) == DNGL_RING_RPTR(flow_ring)) ||
			((DNGL_RING_WPTR(flow_ring) == flow_ring->rd_pending) &&
			!flow_ring->flow_info.pktinflight && !flow_ring->fetch_pending)) {
			flow_ring->status &= ~FLOW_RING_FLUSH_PENDING;
			if (flow_ring->status & FLOW_RING_FLUSH_RESP_PENDING)
				pciedev_send_flow_ring_flush_resp(pciedev, flow_ring->ringid,
				BCMPCIE_SUCCESS);
			else if (flow_ring->status & FLOW_RING_DELETE_RESP_PENDING)
				pciedev_send_flow_ring_delete_resp(pciedev, flow_ring->ringid,
				BCMPCIE_SUCCESS);
		} else
			pciedev_read_flow_ring_host_buffer(pciedev, flow_ring);
	}
}

/* Temp PULL model/partial push : Check for write pointer updates from all data flow rings */
static void
pciedev_schedule_flow_ring_read_buffer(struct dngl_bus *pciedev)
{
	uint32 w_ptr;
	bool flow_schedule = FALSE;
	msgbuf_ring_t *flow_ring;
	struct dll *cur, *prio;

	if (pciedev->in_d3_suspend || pciedev->flow_sch_timer_on) {
		return;
	}

	/* get first priority ring out of pool */
	prio = dll_head_p(&pciedev->active_prioring_list);

	/* loop through all active priority rings */
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;

		prioring->schedule = FALSE;

		/* get first flow ring out of pool */
		cur = dll_head_p(&prioring->active_flowring_list);

		/* loop through all active flow rings */
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			flow_ring = ((flowring_pool_t *)cur)->ring;
			w_ptr = DNGL_RING_WPTR(flow_ring);

			/* Check write pointer of the flow ring and mark it for pending pkt pull */
			if (!READ_AVAIL_SPACE(w_ptr, flow_ring->rd_pending,
				RING_MAX_ITEM(flow_ring))) {

				cur = dll_next_p(cur);
				continue;
			}

			flow_ring->status |= FLOW_RING_PKT_PENDING;
			prioring->schedule = TRUE;

			/* get next flow ring node */
			cur = dll_next_p(cur);
		}

		/* adjust maxpktcnt to make the hightest priority ring use max fetch count, */
		/* and others use the min fetch count */
		if (!flow_schedule) {
			if (prioring->maxpktcnt < PCIEDEV_MAX_PACKETFETCH_COUNT &&
				prioring->schedule) {
				prioring->maxpktcnt += PCIEDEV_INC_PACKETFETCH_COUNT;
				if (prioring->maxpktcnt > PCIEDEV_MAX_PACKETFETCH_COUNT)
					prioring->maxpktcnt = PCIEDEV_MAX_PACKETFETCH_COUNT;
			}
		} else {
			prioring->maxpktcnt = PCIEDEV_MIN_PACKETFETCH_COUNT;
		}
		flow_schedule |= prioring->schedule;

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}

	if (flow_schedule && !pciedev->flow_sch_timer_on) {
		dngl_add_timer(pciedev->flow_schedule_timer, 0, FALSE);
		pciedev->flow_sch_timer_on = TRUE;
	}
}

/* Scheduler for Data Flow rings */
/* Pull the packets from the host and send up as required */
void
pciedev_flow_schedule_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) t->context;
	msgbuf_ring_t	*flow_ring;
	struct dll * cur, * prio;
	int	fetch = 1;
	bool ret;

	pciedev->flow_sch_timer_on = FALSE;

	if (pciedev->in_d3_suspend) {
		return;
	}

	/* always start from the highest priority rings */
	prio = dll_head_p(&pciedev->active_prioring_list);

	/* loop through all active priority rings */
	while (!dll_end(prio, &pciedev->active_prioring_list) && fetch) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;

		if (!prioring->schedule) {
			/* all flowrings in the prio are empty, skip and move on */
			goto nextprio;
		}

		/* start fom the last fetch node to be fair to all rings */
		cur = dll_next_p(prioring->last_fetch_node);

		/* loop through all nodes */
		while (fetch) {
			/* skip the head node which does not hold any info */
			if (dll_end(cur, &prioring->active_flowring_list))
				goto nextnode;

			/* get flow ring from nodes */
			flow_ring = ((flowring_pool_t *)cur)->ring;

			if (!flow_ring->flow_info.pktinflight &&
				!flow_ring->fetch_pending)
				flow_ring->status &= ~FLOW_RING_SUP_PENDING;

			if (flow_ring->status & FLOW_RING_SUP_PENDING)
				goto nextnode;

			flow_ring->flow_info.maxpktcnt = prioring->maxpktcnt;

			if ((flow_ring->status & FLOW_RING_PORT_CLOSED)) {
				if ((flow_ring->status & FLOW_RING_PKT_PENDING) &&
					(flow_ring->flow_info.flags
					& FLOW_RING_FLAG_INFORM_PKTPEND) &&
					!(flow_ring->flow_info.flags
					& FLOW_RING_FLAG_LAST_TIM)) {
					ret = dngl_flowring_update(pciedev->dngl,
						flow_ring->flow_info.ifindex,
						(uint8) flow_ring->handle,
						FLOW_RING_TIM_SET,
						(uint8 *)&flow_ring->flow_info.sa,
						(uint8 *)&flow_ring->flow_info.da,
						(uint8) flow_ring->flow_info.tid);


					if (ret) {
						flow_ring->flow_info.flags |=
							FLOW_RING_FLAG_LAST_TIM;
						goto nextnode;
					} else {
						flow_ring->flow_info.maxpktcnt = 1;
						flow_ring->flow_info.reqpktcnt += 1;
						flow_ring->flow_info.flags |=
							FLOW_RING_FLAG_PKT_REQ;
					}
				} else
					goto nextnode;
			}

		PCI_TRACE(("h2d FLOW RING %d write read %p   %p  WI %d R I%d\n",
			flow_ring->ringid,
			flow_ring->tcm_rs_w_ptr,
			flow_ring->tcm_rs_r_ptr,
			DNGL_RING_WPTR(flow_ring),
			flow_ring->rd_pending));

			if (!(flow_ring->status & FLOW_RING_PKT_PENDING))
				goto nextnode;

			/* Read out messages from the flow ring */
			fetch = pciedev_read_flow_ring_host_buffer(pciedev, flow_ring);
			if (fetch) {
				prioring->last_fetch_node = cur;
			}

			flow_ring->status &= ~FLOW_RING_PKT_PENDING;

	nextnode:
			if (cur == prioring->last_fetch_node)
				break;

			cur = dll_next_p(cur);
		}

	nextprio:
		/* get next priority ring node */
		prio = dll_next_p(prio);
	}

	pciedev_schedule_flow_ring_read_buffer(pciedev);
}

/* Get max number of packets that can be fetched from the ring */
static uint pciedev_get_maxpkt_fetch_count(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring)
{
	uint32 lbuf_avail = 0, ret_len;

	/* if we are flushing packets, fetch all & send tx status bypassing wl */
	if (flow_ring->status & FLOW_RING_FLUSH_PENDING)
		return MIN(READ_AVAIL_SPACE(DNGL_RING_WPTR(flow_ring),
		flow_ring->rd_pending, RING_MAX_ITEM(flow_ring)),
		flow_ring->buf_pool->item_cnt);
#ifdef BCMFRAGPOOL
	/* If there is no lbuf/lfrags wait for freeups */
	if (!(lbuf_avail = pktpool_avail(pciedev->pktpool_lfrag))) {
		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"No lbus/lfrags Available\n");
		return 0;
	}
#endif
	/* Fetch packets only if we have enough lbuf/pktpool lbuf_avail */
	ret_len = lbuf_avail;

	/* Consider fetch packets pending in fetch queue */
	if (ret_len > pciedev->fetch_pending)
		ret_len -= pciedev->fetch_pending;
	else {
		PCI_INFORM(("Extra Tx Lbuf allocated\n"));
		return 0;
	}
	/* Do not fetch more than maxpktcnt packets for this ring */
	ret_len = MIN(ret_len, flow_ring->flow_info.maxpktcnt);

	/* Cap till contiguous available buffer */
	ret_len = MIN(ret_len, READ_AVAIL_SPACE(DNGL_RING_WPTR(flow_ring),
	           flow_ring->rd_pending, RING_MAX_ITEM(flow_ring)));
	return ret_len;

}

int
pciedev_read_flow_ring_host_buffer(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring)
{
	uint8 *dest_addr;
	uint8 *src_addr;
	uint16 src_len, ret_len;
	int8 i;
	int k;


	/* If there is no flow fetch request come later */
	if ((i = pciedev_allocate_flowring_fetch_rqst_entry(pciedev)) < 0) {
		PCI_ERROR(("Fetch Req alloc error read later %d \n", flow_ring->ringid));
		return 0;
	}
	if (!LCL_BUFPOOL_AVAILABLE(flow_ring)) {
		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"flow ring name: %c%c%c%c Local ring bufs not available\n",
			flow_ring->name[0], flow_ring->name[1],
			flow_ring->name[2], flow_ring->name[3]);
		goto cleanup;
	}
	pciedev->flowring_fetch_rqsts[i].start_ringinx = flow_ring->rd_pending;

	ret_len = pciedev_get_maxpkt_fetch_count(pciedev, flow_ring);

	/* If next fetched packets are already status_cmpl do not fetch them */
	for (k = 0; k < ret_len; k++) {
		if (isset(flow_ring->status_cmpl,
			(flow_ring->rd_pending + k) % RING_MAX_ITEM(flow_ring))) {
			ret_len = k;
			break;
		}
	}
	/* Nothing to fetch go cleanup */
	if (!ret_len)
		goto cleanup;

	ret_len *= RING_LEN_ITEMS(flow_ring);

	/* Update the local copy of the write/end ptr with the shared register's value */
	src_addr = pciedev_get_src_addr(flow_ring, &src_len, ret_len);
	if (src_addr == NULL) {
		goto cleanup;
	}

	/* Get from pool of lcl buffers for now */
	dest_addr = pciedev_get_lclbuf_pool(flow_ring);

	set_bitrange(flow_ring->inprocess, pciedev->flowring_fetch_rqsts[i].start_ringinx,
		(pciedev->flowring_fetch_rqsts[i].start_ringinx +
		(src_len/RING_LEN_ITEMS(flow_ring)) - 1) % RING_MAX_ITEM(flow_ring),
		RING_MAX_ITEM(flow_ring) - 1);

	if (dest_addr != NULL) {
		/* Place the fetch request */
		pciedev->flowring_fetch_rqsts[i].rqst.size = src_len;
		pciedev->flowring_fetch_rqsts[i].rqst.dest = dest_addr;
		PHYSADDR64HISET(pciedev->flowring_fetch_rqsts[i].rqst.haddr,
			(uint32) ltoh32(HIGH_ADDR_32(flow_ring->ringmem->base_addr)));
		PHYSADDR64LOSET(pciedev->flowring_fetch_rqsts[i].rqst.haddr,
			(uint32) ltoh32(src_addr));

		pciedev->flowring_fetch_rqsts[i].rqst.cb = pciedev_flowring_fetch_cb;
		pciedev->flowring_fetch_rqsts[i].rqst.ctx =
			(void *)&pciedev->flowring_fetch_rqsts[i];
		pciedev->flowring_fetch_rqsts[i].rqst.flags = 0;
		pciedev->flowring_fetch_rqsts[i].rqst.next = NULL;
		pciedev->flowring_fetch_rqsts[i].msg_ring = flow_ring;
#ifdef BCMPCIEDEV_ENABLED
		hndrte_fetch_rqst(&pciedev->flowring_fetch_rqsts[i].rqst);
#endif
		pciedev->last_fetch_ring = FLRING_INX(flow_ring->ringid);
		flow_ring->fetch_pending += src_len/RING_LEN_ITEMS(flow_ring);
		pciedev->fetch_pending += src_len/RING_LEN_ITEMS(flow_ring);
	} else {
		src_len = 0;
	}
	pciedev_update_rdp_ptr_unacked(flow_ring);
	return src_len / RING_LEN_ITEMS(flow_ring);
cleanup:
	pciedev_update_rdp_ptr_unacked(flow_ring);
	pciedev_free_flowring_fetch_rqst_entry(pciedev, i);
	return 0;
}

static void
pciedev_flowring_fetch_cb(struct fetch_rqst *fr, bool cancelled)
{
	flow_fetch_rqst_t *flow_fetch = fr->ctx;
	struct dngl_bus *pciedev = flow_fetch->msg_ring->pciedev;

	/* If cancelled, retry: drop request for now
	 *  Might need to retry sending it down HostFetch
	 */
	if (cancelled) {
		PCI_ERROR(("pciedev_flowring_fetch_cb: Request cancelled!...\n"));
		goto cleanup;
	}
	pciedev_handle_h2d_msg_txsubmit(pciedev, fr->dest, fr->size, flow_fetch->msg_ring,
		flow_fetch->start_ringinx);

cleanup:
	/* handle Clean up */
	/* free up local mesage space */
	pciedev_free_lclbuf_pool(flow_fetch->msg_ring, fr->dest);

	flow_fetch->msg_ring->fetch_pending -= (fr->size/RING_LEN_ITEMS(flow_fetch->msg_ring));
	pciedev->fetch_pending -= (fr->size/RING_LEN_ITEMS(flow_fetch->msg_ring));
	if (!flow_fetch->msg_ring->flow_info.pktinflight &&
		!flow_fetch->msg_ring->fetch_pending)
		flow_fetch->msg_ring->status &= ~FLOW_RING_SUP_PENDING;

	pciedev_free_flowring_fetch_rqst_entry(pciedev, flow_fetch->index);

	pciedev_process_pending_flring_resp(pciedev, flow_fetch->msg_ring);
	return;
}

static int8 pciedev_allocate_flowring_fetch_rqst_entry(struct dngl_bus *pciedev)
{
	uint8 i;
	for (i = 0; i < PCIEDEV_MAX_FLOWRINGS_FETCH_REQUESTS; i++)
		if (!pciedev->flowring_fetch_rqsts[i].used) {
			pciedev->flowring_fetch_rqsts[i].used = TRUE;
			return i;
		}
	return BCME_ERROR;
}

static inline void pciedev_free_flowring_fetch_rqst_entry(struct dngl_bus *pciedev, uint8 index)
{
	pciedev->flowring_fetch_rqsts[index].used = FALSE;
}

/* Adjust Read-pending/next start of fetch pointer
 * Move rdpending to be lowest of rdpending and index, This is to
 * allow all packet suppressed/need-to-be refetched until the index
 * index points to index of the suppressed packet
 */

static void pciedev_adjust_flow_fetch_ptr(msgbuf_ring_t *flow_ring, uint16 index)
{
	if ((flow_ring->rd_pending >= DNGL_RING_RPTR(flow_ring))) {
		/* No wrap condition */
		if ((index >= DNGL_RING_RPTR(flow_ring)) && (index < flow_ring->rd_pending))
			flow_ring->rd_pending = index;
	} else {
		/* Wrap condition */
		if ((index < flow_ring->rd_pending) || (index >= DNGL_RING_RPTR(flow_ring)))
			flow_ring->rd_pending = index;
	}
}

/* If first packet to be fetched in is already status_cmpl then
 * move rd pending to first non-acked packet skip packets that are alreday acked.
 * This can happen if we get status ok/non-suppressed packet in between
 * the packet stream.
 */
static void pciedev_update_rdp_ptr_unacked(msgbuf_ring_t *flow_ring)
{
	uint16 tmp_len, k, rdpt;
	if (!isset(flow_ring->status_cmpl, flow_ring->rd_pending))
		return;

	if (DNGL_RING_WPTR(flow_ring) >= flow_ring->rd_pending)
		tmp_len = DNGL_RING_WPTR(flow_ring) - flow_ring->rd_pending;
	else
		tmp_len = (RING_MAX_ITEM(flow_ring) - flow_ring->rd_pending) +
		             DNGL_RING_WPTR(flow_ring);

	rdpt = flow_ring->rd_pending;
	for (k = 0; k < tmp_len; k++) {
		if ((isset(flow_ring->inprocess, (rdpt + k) %
			    RING_MAX_ITEM(flow_ring))) &&
			    !(isset(flow_ring->status_cmpl, (rdpt + k) %
				    RING_MAX_ITEM(flow_ring)))) {
			flow_ring->rd_pending = (rdpt + k) % RING_MAX_ITEM(flow_ring);
			break;
		} else if ((isset(flow_ring->inprocess, (rdpt + k) %
			    RING_MAX_ITEM(flow_ring))) &&
			    (isset(flow_ring->status_cmpl, (rdpt + k) %
				    RING_MAX_ITEM(flow_ring))))
				    flow_ring->rd_pending = (flow_ring->rd_pending + 1) %
				    RING_MAX_ITEM(flow_ring);
	}
}
static int pciedev_update_txstatus(struct dngl_bus *pciedev, uint32 status,
	uint16 rindex, uint16 flowid)
{
	msgbuf_ring_t *flow_ring;
	uint16 index, rdptr;
	int ret = BCME_OK;
	int i = 0;
#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	return;
#endif
	index = flowid - BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START;
	flow_ring = &pciedev->flow_ring_msgbuf[index];
	flow_ring->flow_info.pktinflight--;
	pciedev->pend_user_tx_pkts--;
	rdptr = DNGL_RING_RPTR(flow_ring);

	if ((status == 0) && !(flow_ring->hole_pending))  {
		if (rdptr == rindex) {
			clrbit(flow_ring->inprocess, rindex);
			clrbit(flow_ring->status_cmpl, rindex);
			rdptr = (rdptr + 1) % RING_MAX_ITEM(flow_ring);
			BCMMSGBUF_RING_SET_R_PTR(flow_ring, rdptr);
			goto done;
		}
	}

	if (!(flow_ring->status & FLOW_RING_FLUSH_PENDING) &&
		((status == WLFC_CTL_PKTFLAG_D11SUPPRESS) ||
		(status == WLFC_CTL_PKTFLAG_WLSUPPRESS))) {
		ret = BCME_NOTREADY;
		if (flow_ring->flow_info.pktinflight || flow_ring->fetch_pending)
			flow_ring->status |= FLOW_RING_SUP_PENDING;
		pciedev_adjust_flow_fetch_ptr(flow_ring, rindex);
	} else if (isset(flow_ring->status_cmpl, rindex) ||
	           isclr(flow_ring->inprocess, rindex)) {
			PCI_ERROR(("I %d RD %d RDp %d i %d W %d\n", rindex,
				DNGL_RING_RPTR(flow_ring),
				pciedev->flow_ring_msgbuf[index].rd_pending,
			          i, DNGL_RING_WPTR(flow_ring)));
			ret = BCME_NOTREADY;
	} else {

		/* Set acked bit to avoid any holes in acked and unacked list while fetching */
		setbit(flow_ring->status_cmpl, rindex);
		/* Find index until non pending packet and update the read index */
		while (i < RING_MAX_ITEM(flow_ring))
		{
			if (!isset(flow_ring->status_cmpl, (rdptr + i) % RING_MAX_ITEM(flow_ring)))
				break;
			/* Clear inprocess bit, we are done with the packet now */
			clrbit(flow_ring->status_cmpl, (rdptr + i) % RING_MAX_ITEM(flow_ring));
			clrbit(flow_ring->inprocess, (rdptr + i) % RING_MAX_ITEM(flow_ring));
			i++;
		}
		BCMMSGBUF_RING_SET_R_PTR(flow_ring, (rdptr + i) % RING_MAX_ITEM(flow_ring));
		if ((rdptr + i) % RING_MAX_ITEM(flow_ring) != rindex)
			flow_ring->hole_pending = TRUE;
		else
			flow_ring->hole_pending = FALSE;
	}
done:
	if (!flow_ring->flow_info.pktinflight && !flow_ring->fetch_pending)
		flow_ring->status &= ~FLOW_RING_SUP_PENDING;

	pciedev_process_pending_flring_resp(pciedev, flow_ring);
	pciedev_schedule_flow_ring_read_buffer(pciedev);
	return ret;

}

/* Return a lcl buf from free list */
void*
pciedev_get_lclbuf_pool(msgbuf_ring_t * ring)
{
	void* ret;
	lcl_buf_pool_t * pool = ring->buf_pool;

	/* check for avail space */
	if (pool->free == NULL) {
		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"Pool empty Ring name: %c%c%c%c\n",
			ring->name[0], ring->name[1],
			ring->name[2], ring->name[3]);

		return NULL;
	}

	/* Retrieve back the buffer */
	ret = pool->free->p;
	ASSERT(ret != NULL);

	/* Make pkt in cur node as NULL */
	pool->free->p = NULL;

	pool->free = pool->free->nxt;
#ifdef PCIEDEV_DBG_LOCAL_POOL
	pciedev_check_free_p(ring, pool->free, ret, __FUNCTION__);
#endif /* PCIEDEV_DBG_LOCAL_POOL */
	pool->availcnt--;
	return ret;
}

/* Admit the buffer back to free list */
static void
pciedev_free_lclbuf_pool(msgbuf_ring_t * ring, void* p)
{
	lcl_buf_pool_t * pool = ring->buf_pool;
	lcl_buf_t * free = pool->free;

	if (free == pool->head) {
		PCI_ERROR(("pool allready full cant admit %x to ring name: %c%c%c%c\n",
			(uint32)p,
			ring->name[0], ring->name[1],
			ring->name[2], ring->name[3]));

		ASSERT(0);
		return;
	}

	if (free == NULL) {
		/* If all items are exhausted, free will point to NULL */
		/* restore free to tail the moment we get atleast 1 pkt back */
		free = pool->tail;
		free->p = p;
		pool->free = free;
	} else {
#ifdef PCIEDEV_DBG_LOCAL_POOL
		/* Store pkt in previous node */
		pciedev_check_free_p(ring, free, p, __FUNCTION__);
#endif /* PCIEDEV_DBG_LOCAL_POOL */
		free->prv->p = p;
		/* Move free ptr to previous node */
		pool->free = free->prv;
	}
	/* Increment avail count */
	pool->availcnt++;
}

/* return back
	1. 64 bit host address
	2. host addr len
	3. 64 bit metadata addr
	4. metadata len
*/
static int
pciedev_get_host_addr(struct dngl_bus * pciedev, uint32* bufid, uint16 * len, dma64addr_t *haddr,
	dma64addr_t *meta_addr, uint16 *meta_len)
{
	uint8 r, depth;
	uint8 max_item;
	void* lcl_buf = NULL;
	uint8* p = NULL;
	msgbuf_ring_t * ring = pciedev->htod_rx;
	inuse_lclbuf_pool_t * rxpool = ring->buf_pool->inuse_pool;

	/* intialize haddr to NULL */
	PHYSADDR64HISET(*haddr, 0);
	PHYSADDR64LOSET(*haddr, 0);

	r = rxpool->r_ptr;
	depth = rxpool->depth;
	/* Check if any buffer locally available */
	if (!NTXPACTIVE(rxpool->r_ptr, rxpool->w_ptr, depth)) {
		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"pciedev_get_host_addr : No active element in local array \n");
		return -1;
	}

	/* Retrieve the head buffer pointer */
	lcl_buf = rxpool->buf[r].p;
	p = (uint8*)lcl_buf;

	/* max no of host buffers available in this chunk */
	max_item = rxpool->buf[r].max_items;
	if ((max_item) && (p != NULL)) {

		/* retrieve individual message */
		p = p + (RING_LEN_ITEMS(ring) * (max_item - 1));

		/* process rx post message */
		pciedev_process_rxpost_msg(p, bufid, len, haddr, meta_addr, meta_len);

#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
		pciedev_host_pktid_audit(pciedev->host_pktid_audit, *bufid, TRUE);
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */

		rxpool->buf[r].max_items --;

		if (rxpool->buf[r].max_items == 0) {
			/* local buffer full used up. free that into pool */
			pciedev_free_lclbuf_pool(ring, lcl_buf);

			/* Check for more rxbuffers in the ring */
			pciedev_msgbuf_intr_process(pciedev);

			/* Update read pointer */
			rxpool->r_ptr = NEXTTXP(rxpool->r_ptr, depth);
		}
		return 0;
	}
	return -1;
}
int
pciedev_fillup_rxcplid(pktpool_t *pool, void *arg, void *p, bool dummy)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) arg;
	rxcpl_info_t *p_rxcpl_info;

	if (PKTRXCPLID(pciedev->osh, p) != 0) {
		return 0;
	}
	p_rxcpl_info = bcm_alloc_rxcplinfo();
	if (p_rxcpl_info == NULL) {
		PCI_ERROR(("couldn't allocate rxcpl info for lbuf: %x \n", (uint32)p));
		PCIEDEV_RXCPL_ERR_INCR(pciedev);
		return -1;
	}
	if (p_rxcpl_info->rxcpl_id.idx == 0) {
		PCI_ERROR(("lbuf: got the p_rxcpl_info->rxcpl_id.idx as zero\n"));
		ASSERT(p_rxcpl_info->rxcpl_id.idx != 0);
	}
	PKTSETRXCPLID(pciedev->osh, p, p_rxcpl_info->rxcpl_id.idx);
	return 0;
}
/* Takes in a frag as input */
/* update frag with host address & len */
int
pciedev_fillup_haddr(pktpool_t *pool, void* arg, void* frag, bool rxcpl_needed)
{

	struct dngl_bus *pciedev = (struct dngl_bus *) arg;
	uint16 len, meta_len;
	dma64addr_t haddr, meta_addr;
	uint32 bufid;
	rxcpl_info_t *p_rxcpl_info = NULL;

	/* Check if lfrag allready has host address associated with it */
	if (PKTRXCPLID(pciedev->osh, frag) != 0)
		rxcpl_needed = FALSE;

	if (rxcpl_needed)  {
		/* Check if we have space for rx completion ID */
		p_rxcpl_info = bcm_alloc_rxcplinfo();
		if (p_rxcpl_info == NULL) {
			PCI_ERROR(("couldn't allocate rxcpl info: %x \n", (uint32)frag));
			PCI_ERROR(("couldn't allocate rxcpl\n"));
			return -1;
		}
	}
	if (PKTISRXFRAG(pciedev->osh, frag)) {
		if (rxcpl_needed)
			PKTSETRXCPLID(pciedev->osh, frag, p_rxcpl_info->rxcpl_id.idx);
		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"Frag allready filled up : %p \n", (uint32)frag);
		return 0;
	}

	/* Check if buffers available locally */
	if (pciedev_get_host_addr(pciedev, &bufid, &len, &haddr, &meta_addr, &meta_len)) {
		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"No lcl RX post message available \n");
		if (p_rxcpl_info != NULL)
			bcm_free_rxcplinfo(p_rxcpl_info);
		return -1;
	}

	/* setup the rx completion ID */
	if (rxcpl_needed) {
		if (p_rxcpl_info->rxcpl_id.idx == 0) {
			PCI_ERROR(("rxlfrag: got the p_rxcpl_info->rxcpl_id.idx as zero\n"));
			ASSERT(p_rxcpl_info->rxcpl_id.idx != 0);
		}
		PKTSETRXCPLID(pciedev->osh, frag, p_rxcpl_info->rxcpl_id.idx);
	}

	/* Load 64 bit host address */
	PKTSETFRAGDATA_HI(pciedev->osh, frag, 1, PHYSADDR64HI(haddr));
	PKTSETFRAGDATA_LO(pciedev->osh, frag, 1, PHYSADDR64LO(haddr) + PKTRXFRAGSZ);

	/* frag len */
	PKTSETFRAGLEN(pciedev->osh, frag, 1, (len - PKTRXFRAGSZ));
	/* pktid */
	PKTSETFRAGPKTID(pciedev->osh, frag, bufid);

	/* set the meta data pointers */
	PKTSETFRAGMETADATALEN(pciedev->osh, frag, meta_len);
	/* Access addr only length is valid */
	if (meta_len) {
		PKTSETFRAGMETADATA_HI(pciedev->osh, frag, PHYSADDR64HI(meta_addr));
		PKTSETFRAGMETADATA_LO(pciedev->osh, frag, PHYSADDR64LO(meta_addr));
	}

	/* Mark rxfrag that host addr is valid */
	PKTSETRXFRAG(pciedev->osh, frag);

	return 0;
}

/* Add an item to inuse list */
static void
pciedev_add_to_inuselist(struct dngl_bus *pciedev, void* p, uint8 max_items)
{
	msgbuf_ring_t * ring = pciedev->htod_rx;
	inuse_lclbuf_pool_t * rxpool = ring->buf_pool->inuse_pool;
	uint8 w, r, depth;

	w = rxpool->w_ptr;
	depth = rxpool->depth;
	r = rxpool->r_ptr;

	/* check if inuse list has space */
	if (!NTXPAVAIL(r, w, depth)) {
		return;
	}

	/* Store buf pointer & max items in the buff */
	rxpool->buf[w].p = p;
	rxpool->buf[w].max_items = max_items;

	/* Update write pointer */
	rxpool->w_ptr = NEXTTXP(rxpool->w_ptr, depth);
}

static void
pciedev_manage_h2d_dma_clocks(struct dngl_bus *pciedev)
{
	if (pciedev->force_ht_on) {
		PCI_TRACE(("%s: force clock on\n", __FUNCTION__));
		OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.clk_ctl_st, CCS_FORCEHT);
	}
	else {
		PCI_TRACE(("%s: removing the force clock on\n", __FUNCTION__));
		AND_REG(pciedev->osh, &pciedev->regs->u.pcie2.clk_ctl_st, ~CCS_FORCEHT);
	}
	return;
}

/* process rx post message form host */
static void
pciedev_process_rxpost_msg(void* p, uint32* bufid, uint16 * len, dma64addr_t *haddr,
	dma64addr_t *haddr_meta, uint16 *metadata_len)
{
	host_rxbuf_post_t *rx_post = (host_rxbuf_post_t *)p;

	/* PKTLEN */
	*len =  ltoh16(rx_post->data_buf_len);

	/* BUFID */
	*bufid = ltoh32(rx_post->cmn_hdr.request_id);

	/* HOST address */
	PHYSADDR64HISET(*haddr, (uint32) rx_post->data_buf_addr.high_addr);
	PHYSADDR64LOSET(*haddr, (uint32) rx_post->data_buf_addr.low_addr);

	/* mark it as pcie address */
	PHYSADDR64HISET(*haddr, PHYSADDR64HI(*haddr) | PCIE_ADDR_OFFSET);

	/* Metadata info */
	*metadata_len = ltoh16(rx_post->metadata_buf_len);

	/* Access addr only length is valid */
	if (*metadata_len) {
		PHYSADDR64HISET(*haddr_meta, (uint32) rx_post->metadata_buf_addr.high_addr);
		PHYSADDR64LOSET(*haddr_meta, (uint32) rx_post->metadata_buf_addr.low_addr);
	}
}

static inline int
pciedev_htoddma_queue_avail(struct dngl_bus *pciedev)
{
	uint16 rd_idx = pciedev->htod_dma_rd_idx;
	uint16 wr_idx = pciedev->htod_dma_wr_idx;

	/* NOTE: The maximum no. of elements that can be held in the Q
	 * at a time is (MAX_DMA_QUEUE_LEN - 1). When Q is full, wr_idx
	 * will point to an empty Q location, just before rd_idx. So for Q
	 * to have adequate space for enque, this utility function should return > 1
	 */
	return (rd_idx > wr_idx ? (rd_idx - wr_idx) : (MAX_DMA_QUEUE_LEN_H2D - wr_idx + rd_idx));
}
int
pciedev_dispatch_fetch_rqst(struct fetch_rqst *fr, void *arg)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)arg;
	uint32 txdesc, rxdesc;
	dma64addr_t src;
	uint16 dmalen;
	int dma_qavail;

	if (pciedev->in_d3_suspend) {
		return BCME_ERROR;
	}

	PHYSADDR64HISET(src, (uint32) ltoh32(PHYSADDR64HI(fr->haddr)));
	PHYSADDR64LOSET(src, (uint32) ltoh32(PHYSADDR64LO(fr->haddr)));
	dmalen = align(fr->size, 4);

	txdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, HTOD, TXDESC);
	rxdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, HTOD, RXDESC);
	dma_qavail = pciedev_htoddma_queue_avail(pciedev);

	if (txdesc >= MIN_TXDESC_AVAIL && rxdesc >= MIN_RXDESC_AVAIL && (dma_qavail > 1)) {
		pciedev_enque_fetch_cmpltq(pciedev, fr);
		pciedev_h2dmsgbuf_dma(pciedev, src, dmalen,
			(uint8*) fr->dest, pciedev->dummy_rxoff, NULL, MSG_TYPE_HOST_FETCH);
		return BCME_OK;
	} else {
		return BCME_ERROR;
	}
}
static void
pciedev_enque_fetch_cmpltq(struct dngl_bus *pciedev, struct fetch_rqst *fr)
{
	pciedev_fetch_cmplt_q_t *fcq = pciedev->fcq;

	if (fcq->head == NULL)
		fcq->head = fcq->tail = fr;
	else {
		fcq->tail->next = fr;
		fcq->tail = fr;
	}
	fcq->count++;
	fcq->tail->next = NULL;
}
static struct fetch_rqst *
pciedev_deque_fetch_cmpltq(struct dngl_bus *pciedev)
{
	pciedev_fetch_cmplt_q_t *fcq = pciedev->fcq;
	struct fetch_rqst *fr;

	if (fcq->head == NULL) {

		fr = NULL;
	} else if (fcq->head == fcq->tail) {
		ASSERT(fcq->count > 0);
		fcq->count--;
		fr = fcq->head;
		fcq->head = fcq->tail = NULL;
	} else {
		ASSERT(fcq->count > 0);
		fcq->count--;
		fr = fcq->head;
		fcq->head = fcq->head->next;
		fr->next = NULL;
	}
	return fr;
}


void
pciedev_process_tx_payload(struct dngl_bus *pciedev)
{
	struct fetch_rqst *fr;
	bool cancelled = FALSE;

	if (pciedev->in_d3_suspend)
		return;

	fr = pciedev_deque_fetch_cmpltq(pciedev);
	if (fr == NULL)
		return;

	/* If the fetch_rst was cancelled while in bus DMA queue,
	 * need to return it back to Host indicating the cancellation
	 */
	if (FETCH_RQST_FLAG_GET(fr, FETCH_RQST_CANCELLED)) {
		cancelled = TRUE;
		/* Clear the fetch_rqst flag now, to avoid misunderstandings later */
		FETCH_RQST_FLAG_CLEAR(fr, FETCH_RQST_CANCELLED);
	}

	FETCH_RQST_FLAG_CLEAR(fr, FETCH_RQST_IN_BUS_LAYER);

	/* Call the registered callback function */
	if (fr->cb)
		fr->cb(fr, cancelled);
	else {
		PCI_ERROR(("pciedev_process_tx_payload: No callback registered for fetch_rqst!\n"));
		ASSERT(0);
	}
}
/* H2D direction: return host address of ring to be read from */
static void*
pciedev_get_src_addr(msgbuf_ring_t * ring, uint16* available_len, uint16 max_len)
{
	uint16 w_ptr;
	uint16 r_ptr;
	uint16 depth;
	void* ret_addr = NULL;

	w_ptr = DNGL_RING_WPTR(ring);
	r_ptr = ring->rd_pending;
	depth = RING_MAX_ITEM(ring);

	/* First check if there is any data available in the circular buffer */
	*available_len = READ_AVAIL_SPACE(w_ptr, r_ptr, depth);
	if (*available_len == 0)
		return NULL;
	if (max_len) {
		max_len = max_len/RING_LEN_ITEMS(ring);
		*available_len = MIN(MIN(*available_len, ring->buf_pool->item_cnt), max_len);
	} else
		*available_len = MIN(*available_len, ring->buf_pool->item_cnt);
	ASSERT(*available_len <= depth);

	/* We dont do dma on wrapped around space */
	/* do it in two steps where you read end region first followed by top region */
	ret_addr = (uint8*)RING_START_PTR(ring) + (ring->rd_pending * RING_LEN_ITEMS(ring));

	/*
	 * Please note that we do not update the read pointer here. Only
	 * read pending pointer is updated, so that next reader knows where
	 * to read data from.
	 * read pointer can only be updated when the read is complete.
	 */
	if ((ring->rd_pending + *available_len) >= depth)
		ring->rd_pending = 0;
	else
		ring->rd_pending += *available_len;

	ASSERT(ring->rd_pending < depth);

	/* Make it byte count rather than index count */
	*available_len = *available_len * RING_LEN_ITEMS(ring);
	return ret_addr;
}
/* H2D direction: update h2d ring read pointer after h2d dma is complete */
static void
pciedev_ring_update_readptr(msgbuf_ring_t *ring, uint16 bytes_read)
{
	uint16 index_read;
	uint16 rd_idx;

	if (ring == NULL)
		return;

	index_read = bytes_read / RING_LEN_ITEMS(ring);
	ASSERT(index_read <= RING_MAX_ITEM(ring));

	if (index_read == 0)
		return;

	rd_idx = DNGL_RING_RPTR(ring);

	/* Update the read pointer */
	if ((rd_idx + index_read) >= RING_MAX_ITEM(ring))
		BCMMSGBUF_RING_SET_R_PTR(ring, 0);
	else
		BCMMSGBUF_RING_SET_R_PTR(ring, rd_idx + index_read);

	PCI_TRACE(("ring name: %c%c%c%c  updating the R_PTR to %d\n",
		ring->name[0], ring->name[1],
		ring->name[2], ring->name[3],
		DNGL_RING_RPTR(ring)));
}
/* D2H direction: Update write ptr after d2h dma complete */
static void
pciedev_ring_update_writeptr(msgbuf_ring_t *ring, uint16 bytes_written)
{
	uint16 wrt_idx;
	uint16 index;

	if (ring == NULL)
		return;

	index = bytes_written / RING_LEN_ITEMS(ring);
	ASSERT(index <= RING_MAX_ITEM(ring));

	if (index == 0)
		return;

	wrt_idx = DNGL_RING_WPTR(ring);

	/* Update Write pointer */
	if ((wrt_idx + index) >= RING_MAX_ITEM(ring)) {
		BCMMSGBUF_RING_SET_W_PTR(ring, 0);
	} else {
		BCMMSGBUF_RING_SET_W_PTR(ring, wrt_idx + index);
	}
#ifdef PCIE_PHANTOM_DEV
	phtm_ring_dtoh_doorbell(ring->pciedev->phtm);
#else
	pciedev_generate_host_db_intr(ring->pciedev, 0x12345678, PCIE_DB_DEV2HOST_0);
#endif

}
/* D2H direction: return ring ptr to put d2h messages */
/* Update write pending pointers */
static uint32
pciedev_get_ring_space(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf, uint16 msglen)
{
	uint16 wp_idx = msgbuf->wr_pending;
	uint32 retaddr;
	uint16 index = msglen / RING_LEN_ITEMS(msgbuf);

	uint16 avail_ring_entry = CHECK_WRITE_SPACE(DNGL_RING_RPTR(msgbuf), WRT_PEND(msgbuf),
		RING_MAX_ITEM(msgbuf));

	ASSERT(index < RING_MAX_ITEM(msgbuf));

	if (avail_ring_entry < index) {
		PCI_ERROR(("msgbuf name: In ring %c%c%c%c %d"
			"slots not available, cur avail space %d, msglen %d\n",
			msgbuf->name[0], msgbuf->name[1],
			msgbuf->name[2], msgbuf->name[3],
			index, avail_ring_entry, msglen));
		return NULL;
	}

	/* Return space */
	retaddr = RING_START_PTR(msgbuf) + (wp_idx * RING_LEN_ITEMS(msgbuf));

	/* Update write pending */
	if ((msgbuf->wr_pending + index) >= RING_MAX_ITEM(msgbuf)) {
		ASSERT((msgbuf->wr_pending + index) <= RING_MAX_ITEM(msgbuf));
		msgbuf->wr_pending = 0;
	}
	else
		msgbuf->wr_pending += index;

	ASSERT(msgbuf->wr_pending < RING_MAX_ITEM(msgbuf));

	return retaddr;
}

static bool
pciedev_resource_avail_for_txmetadata(struct dngl_bus *pciedev)
{
	if (!LCL_BUFPOOL_AVAILABLE(pciedev->dtoh_txcpl)) {
		/* not enough resources to queue txstatus */
		return FALSE;
	}

	if (CHECK_WRITE_SPACE(DNGL_RING_RPTR(pciedev->dtoh_txcpl),
		WRT_PEND(pciedev->dtoh_txcpl),
		RING_MAX_ITEM(pciedev->dtoh_txcpl)) <= 0) {
		/* no space in d2h_txcpl ring to queue txstatus */
		return FALSE;
	}

	if ((PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, TXDESC) < (MIN_TXDESC_AVAIL + 2)) ||
		(PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC) < (MIN_RXDESC_AVAIL + 3))) {
		/* not enough descriptors to queue txstatus */
		return FALSE;
	}

	return TRUE;
}

static bool
pciedev_check_process_d2h_message(struct dngl_bus *pciedev, uint32 txdesc, uint32 rxdesc,
	void *p, d2h_msg_handler *msg_handler)
{
	uint32  txdesc_needed;
	uint32  rxdesc_needed;
	cmn_msg_hdr_t *msg;

	msg = (cmn_msg_hdr_t *)PKTDATA(pciedev->osh, p);

	/* check if dma resources are available to send payload */
	/* check if dma resources are available to dma msgbuf */
	/* check if local buffer slot is available */

	switch (msg->msg_type) {
		case MSG_TYPE_WL_EVENT:
			txdesc_needed = 2 + 2;
			rxdesc_needed = 2 + 3;
			if (!LCL_BUFPOOL_AVAILABLE(pciedev->dtoh_ctrlcpl)) {
				return FALSE;
			}
			if (HOST_DMA_BUF_POOL_EMPTY(pciedev->event_pool)) {
				PCI_TRACE(("not sending it because no host buffer avail\n"));
				pciedev->event_delivery_pend = TRUE;
				return FALSE;
			}
			pciedev->event_delivery_pend = FALSE;

			*msg_handler = pciedev_process_d2h_wlevent;
			break;

		case MSG_TYPE_TXMETADATA_PYLD:
			/* Check if there are resources to queue txstatus */
			if (!pciedev_resource_avail_for_txmetadata(pciedev))
				return FALSE;

			txdesc_needed = MIN_TXDESC_AVAIL + 2;
			rxdesc_needed = MIN_RXDESC_AVAIL + 3;
			*msg_handler = pciedev_process_d2h_txmetadata;
			break;

		case MSG_TYPE_RX_PYLD:
			/* Check if there is space in D2H Rx Complete ring */
			if (CHECK_WRITE_SPACE(DNGL_RING_RPTR(pciedev->dtoh_rxcpl),
				WRT_PEND(pciedev->dtoh_rxcpl),
				RING_MAX_ITEM(pciedev->dtoh_rxcpl)) <= 0) {
				return FALSE;
			}

			/* Check if local buf pool is available */
			if (!LCL_BUFPOOL_AVAILABLE(pciedev->dtoh_rxcpl)) {
				return FALSE;
			}

			txdesc_needed = MIN_TXDESC_AVAIL + 2;
			rxdesc_needed = MIN_RXDESC_AVAIL + 3;
			*msg_handler = pciedev_process_d2h_rxpyld;
			break;

#ifdef PCIE_DMAXFER_LOOPBACK
		case MSG_TYPE_LPBK_DMAXFER_PYLD:
			txdesc_needed = MIN_TXDESC_AVAIL;
			rxdesc_needed = MIN_RXDESC_AVAIL;
			*msg_handler = pciedev_process_d2h_dmaxfer_pyld;
			break;
#endif
		default:
			PCI_ERROR(("unknown message on D2H Rxq %d\n", msg->msg_type));
			PKTFREE(pciedev->osh, p, TRUE);
			*msg_handler = NULL;
			return TRUE;
	}
	if (txdesc < txdesc_needed || rxdesc < rxdesc_needed)
		return FALSE;
	return TRUE;
}

static int
pciedev_process_d2h_rxpyld(struct dngl_bus *pciedev, void *p)
{
	uint8 ifidx = 0;
	uint16 pktlen_new;
	uint16 pktlen;
	uint32 bufid = 0;
	uint16 dataoffset = 0, haddr_meta_len = 0;
	dma64addr_t haddr = {0, 0};
	dma64addr_t haddr_meta = {0, 0};
	uint16 len = 0;
	uint16 rxpkt_meta_data_len = 0;
	uint16 rxcpl_id;
	rxcpl_info_t *p_rxcpl_info = NULL;
	bool	queue_rxcpl;

#ifdef TEST_DROP_PCIEDEV_RX_FRAMES
	pciedev_test_drop_rxframe++;
	if (!PKTNEEDRXCPL(pciedev->osh, p)) {
		pciedev_test_drop_norxcpl++;
		if (pciedev_test_drop_norxcpl == pciedev_test_drop_norxcpl_max) {
			pciedev_test_drop_norxcpl = 0;
			pciedev_test_dropped_norxcpls++;
			PKTFREE(pciedev->osh, p, TRUE);
			return 0;
		}
	}
	else if (pciedev_test_drop_rxframe > pciedev_test_drop_rxframe_max) {
		pciedev_test_drop_rxframe = 0;
		pciedev_test_dropped_rxframes++;
		PKTFREE(pciedev->osh, p, TRUE);
		return 0;
	}
#endif /* TEST_DROP_PCIEDEV_RX_FRAMES */

	ifidx = PKTIFINDEX(pciedev->osh, p);

	/* remove cmn_msg_hdr_t added from proto_push */
	PKTPULL(pciedev->osh, p, sizeof(cmn_msg_hdr_t));

	/* pull the metadata from the packet */
	if (PKTDATAOFFSET(p)) {
		rxpkt_meta_data_len = PKTDATAOFFSET(p) * 4;
		PKTPULL(pciedev->osh, p, rxpkt_meta_data_len);
	}
	/* need to look for metadata if present */
	/* if metadata then setup the right length where the info needs to be sent to */

	pktlen = PKTLEN(pciedev->osh, p);
	if (PKTISRXFRAG(pciedev->osh, p)) {
		pktlen += PKTFRAGUSEDLEN(pciedev->osh, p);
		haddr = pciedev_get_haddr_from_lfrag(pciedev, p, &bufid, &haddr_meta,
			&haddr_meta_len, &dataoffset);
	} else {
		/* host addr not valid : return from local array */
		/* Account for the data offset coming form pcie dma here */
		dataoffset = pciedev->d2h_dma_scratchbuf_len;
		pciedev_get_host_addr(pciedev, &bufid, &len, &haddr, &haddr_meta,
			&haddr_meta_len);
		/* Check this out */
		rxcpl_id = PKTRXCPLID(pciedev->osh, p);
		if ((haddr.loaddr != NULL) && (rxcpl_id == 0)) {
			p_rxcpl_info = bcm_alloc_rxcplinfo();
			if (p_rxcpl_info == NULL) {
				PCI_ERROR(("HOST RX BUF: RXCPL ID not free\n"));
				/* Try to send an error to host */
				PKTFREE(pciedev->osh, p, TRUE);
				ASSERT(p_rxcpl_info);
				return 0;
			}
			PKTSETRXCPLID(pciedev->osh, p, p_rxcpl_info->rxcpl_id.idx);
		}
	}
	if (haddr.loaddr == NULL) {
		PCI_ERROR(("HOST RX BUF: ret buf not available \n"));
		/* Try to send an error to host */
		PKTFREE(pciedev->osh, p, TRUE);
		return 0;
	}
	rxcpl_id = PKTRXCPLID(pciedev->osh, p);
	PKTRESETRXCPLID(pciedev->osh, p);
	p_rxcpl_info = bcm_id2rxcplinfo(rxcpl_id);
	if (p_rxcpl_info == NULL) {
		PCI_ERROR(("rxcpl_id is %d, and rxcpl_info is NULL, lb is %x\n",
			rxcpl_id, *(int *)p));
		ASSERT(p_rxcpl_info);
		PKTFREE(pciedev->osh, p, TRUE);
		return 0;
	}
	queue_rxcpl = PKTNEEDRXCPL(pciedev->osh, p);
	PCI_TRACE(("PKTDATAOFFSET is %d, metadata_len is %d, pktlen %d, pktlen_new %d, ifdx %d\n",
		PKTDATAOFFSET(p), haddr_meta_len, pktlen,  PKTLEN(pciedev->osh, p), ifidx));

	if ((pciedev->force_no_rx_metadata) || ((rxpkt_meta_data_len >= PCIE_MEM2MEM_DMA_MIN_LEN) &&
		(rxpkt_meta_data_len > haddr_meta_len)))
	{
		PCI_TRACE(("don't have enough space at the host buffer\n"));
		haddr_meta_len = 0;
	}
	else {
		/* Push rx meta data onto the packet */
		PKTPUSH(pciedev->osh, p, rxpkt_meta_data_len);
		haddr_meta_len = rxpkt_meta_data_len;
	}
	pktlen_new = PKTLEN(pciedev->osh, p);

	/* pktlen could change due to pad bytes in return_haddr_pool */
	PCI_TRACE(("PKTDATAOFFSET is %d, metadata_len is %d, pktlen %d, pktlen_new %d, ifdx %d\n",
		PKTDATAOFFSET(p), haddr_meta_len, pktlen,  PKTLEN(pciedev->osh, p), ifidx));

	if (!(pciedev_tx_pyld(pciedev, p, (ret_buf_t *)&haddr,
		(pktlen_new + pciedev->d2h_dma_scratchbuf_len),
		(ret_buf_t *)&haddr_meta, haddr_meta_len, MSG_TYPE_RX_PYLD)))
	{
		PKTFREE(pciedev->osh, p, TRUE);
		PCI_ERROR(("pciedev_process_d2h_rxpyld: BAD ERROR: shouldn't happen, "
			"pciedev_tx_pyld shouldn't fail\n"));
		ASSERT(0);
	}
	BCM_RXCPL_CLR_IN_TRANSIT(p_rxcpl_info);
	BCM_RXCPL_SET_VALID_INFO(p_rxcpl_info);

	p_rxcpl_info->host_pktref = bufid;
	p_rxcpl_info->rxcpl_len.metadata_len_w = haddr_meta_len >> 2;
	p_rxcpl_info->rxcpl_len.dataoffset = dataoffset;
	p_rxcpl_info->rxcpl_len.datalen =  pktlen;
	p_rxcpl_info->rxcpl_id.ifidx = (ifidx & BCM_MAX_RXCPL_IFIDX);
	p_rxcpl_info->rxcpl_id.dot11 = (PKT80211(p)) ? 1 : 0;

	PCI_TRACE(("bufid: 0x%04x, metadata_len_w %d(%d), datalen %d, offset %d\n",
		p_rxcpl_info->host_pktref, p_rxcpl_info->rxcpl_len.metadata_len_w, haddr_meta_len,
		p_rxcpl_info->rxcpl_len.datalen, p_rxcpl_info->rxcpl_len.dataoffset));

	/* Transfer RX complete message with orig len */
	/* Host shouldnt see the pad bytes. so data offset should cover pad too */
	if (queue_rxcpl || BCM_RXCPL_FRST_IN_FLUSH(p_rxcpl_info)) {
		pciedev_queue_rxcomplete_local(pciedev, p_rxcpl_info, pciedev->dtoh_rxcpl,
			BCM_RXCPL_FRST_IN_FLUSH(p_rxcpl_info));
	}
	return 0;
}

static int
pciedev_process_d2h_txmetadata(struct dngl_bus *pciedev, void *p)
{
	ret_buf_t haddr;
	uint16 metadata_len = 0;
	uint16 txstatus = 0;
	uint8 ifindx;
	uint32 pktid;
	uint16 ringid, r_index;
	uint8 metadatabuf_len;
	bool txfrag = FALSE;
	int ret_val;
	uint32 status = 0;

	/* assert that the packet has metadata on it */
	/* assert that the packet has big enough metadata len to carry it */
	ASSERT(PKTISTXFRAG(pciedev->osh, p));

	PKTPULL(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
	metadatabuf_len = PKTFRAGMETADATALEN(pciedev->osh, p);
	metadata_len = PKTLEN(pciedev->osh, p);
	if (metadatabuf_len) {
		if (metadata_len >= (BCMPCIE_D2H_METADATA_HDRLEN + TLV_HDR_LEN + sizeof(uint32))) {
			/* Copy txstatus which is the first 4 bytes after metdata header */
			memcpy(&status, (char *)PKTDATA(pciedev->osh, p) +
				BCMPCIE_D2H_METADATA_HDRLEN + TLV_HDR_LEN, sizeof(uint32));
			status = WL_TXSTATUS_GET_FLAGS(status);
		}
		/* Access addr only if length is valid */
		haddr.low_addr = PKTFRAGMETADATA_LO(pciedev->osh, p);
		haddr.high_addr = PKTFRAGMETADATA_HI(pciedev->osh, p);
	} else {
		if (metadata_len == TXSTATUS_LEN)
			status = *((uint8*)PKTDATA(pciedev->osh, p) + BCMPCIE_D2H_METADATA_HDRLEN);
	}

	txstatus = status;

	PKTRESETHASMETADATA(pciedev->osh, (struct lbuf *)p);
	ifindx = PKTIFINDEX(pciedev->osh, p);
	pktid = PKTFRAGPKTID(pciedev->osh, p);
	ringid = PKTFRAGFLOWRINGID(pciedev->osh, p);
	r_index = PKTFRAGRINGINDEX(pciedev->osh, p);

	if (pciedev_update_txstatus(pciedev, status, r_index, ringid)) {
#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
		pciedev_host_pktid_audit(pciedev->host_pktid_audit, pktid, FALSE);
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */
		PKTFREE(pciedev->osh, p, FALSE);
		return 0;
	}

	PCI_TRACE(("haddr is 0x%04x, metadatabuf_len %d, metadata_len %d\n",
		haddr.low_addr, metadatabuf_len, metadata_len));

	if ((haddr.low_addr == 0) || (metadata_len  == 0) || (metadatabuf_len < metadata_len)) {
		PCI_TRACE(("metadata not solvagable address 0x%04x, "
			"meta_data_len %d, meta_data_buf_len %d\n",
			haddr.low_addr, metadata_len, metadatabuf_len));
		/* no need for the callback now */
		PKTFREE(pciedev->osh, p, FALSE);
		goto queue_txstatus;
	}

	/* need something like detach the host buffer from this */
	if (PKTISTXFRAG(pciedev->osh, p)) {
		PKTRESETTXFRAG(pciedev->osh, p);
		txfrag = TRUE;
	}
	PCI_TRACE(("metadata len %d first word %d\n", metadata_len,
		*(uint32 *)((uint8 *)PKTDATA(pciedev->osh, p) + 4)));
	ret_val = pciedev_tx_pyld(pciedev, p, NULL, 0, &haddr, D2H_MSGLEN(pciedev, metadata_len),
		MSG_TYPE_TXMETADATA_PYLD);
	if (txfrag)
		PKTSETTXFRAG(pciedev->osh, p);
	if (!ret_val)
	{
		PKTFREE(pciedev->osh, p, FALSE);
		PCI_ERROR(("pciedev_process_d2h_txmetadata: BAD ERROR: shouldn't happen, "
			"pciedev_tx_pyld shouldn't fail\n"));
		ASSERT(0);
	}

queue_txstatus:
	/* Transfer RX complete message with orig len */
	/* Host shouldnt see the pad bytes. so data offset should cover pad too */
	PCI_TRACE(("generating the txstatus for pktid 0x%04x, ringid %d, ifindx %d\n",
		pktid, ringid, ifindx));
	pciedev_queue_txstatus(pciedev, pktid, ifindx, ringid, txstatus,
		metadata_len, pciedev->dtoh_txcpl);
	return 0;
}

static uint16
pciedev_htoddma_deque(struct dngl_bus *pciedev, msgbuf_ring_t **msgbuf, uint8 *msgtype)
{
	uint16 rd_idx = pciedev->htod_dma_rd_idx;
	pciedev->htod_dma_rd_idx = (rd_idx + 1) % MAX_DMA_QUEUE_LEN_H2D;
	*msgbuf = pciedev->htod_dma_q[rd_idx].msgbuf;
	*msgtype = pciedev->htod_dma_q[rd_idx].msg_type;

	/* Call the generic HNDRTE layer callback signalling that bus layer
	 * DMA descrs were freed / something was dequed from the bus layer DMA queue
	 */
#ifdef BCMPCIEDEV_ENABLED
	hndrte_dmadesc_avail_cb();
#endif
	return (pciedev->htod_dma_q[rd_idx].len);
}

void
pciedev_queue_rxcomplete_local(struct dngl_bus *pciedev, rxcpl_info_t *p_rxcpl_info,
	msgbuf_ring_t *ring, bool check_flush)
{
	uint32 count = 0;
	uint16 next_idx = 0;

	if (p_rxcpl_info == NULL) {
		PCI_ERROR(("Rxcomplete queue with bogus rxcplID\n"));
		ASSERT(0);
	}

	while (p_rxcpl_info != NULL) {
		if (BCM_RXCPL_IN_TRANSIT(p_rxcpl_info)) {
			if (check_flush)
				BCM_RXCPL_SET_FRST_IN_FLUSH(p_rxcpl_info);
			if (pciedev->rxcpl_list_t)
				pciedev->rxcpl_list_t->rxcpl_id.next_idx = 0;
			p_rxcpl_info = NULL;
			continue;
		}
		next_idx = p_rxcpl_info->rxcpl_id.next_idx;
		if (BCM_RXCPL_VALID_INFO(p_rxcpl_info)) {
			if (pciedev->rxcpl_list_h == NULL) {
				pciedev->rxcpl_list_h = p_rxcpl_info;
				pciedev->rxcpl_list_t = p_rxcpl_info;
			}
			else {
				pciedev->rxcpl_list_t->rxcpl_id.next_idx =
					p_rxcpl_info->rxcpl_id.idx;
				pciedev->rxcpl_list_t = p_rxcpl_info;
			}
			count++;
		}
		else {
			bcm_free_rxcplinfo(p_rxcpl_info);
		}
		p_rxcpl_info = bcm_id2rxcplinfo(next_idx);
	}

	pciedev->rxcpl_pend_cnt += count;

	/* need to check if this need to be queued or not */
	pciedev_queue_rxcomplete_msgring(pciedev, ring);
}

static void
pciedev_queue_rxcomplete_msgring(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	lcl_buf_pool_t *pool = ring->buf_pool;
	host_rxbuf_cmpl_t *rxcmplt_h;
	uint32 avail_ring_entry;
	rxcpl_info_t *rxcpl_info;

	if (pciedev->rxcompletion_pend)
		return;

	while (pciedev->rxcpl_list_h != NULL) {
	if (pool->local_buf_in_use == NULL) {
		pool->local_buf_in_use = pciedev_get_lclbuf_pool(ring);
		ASSERT(pool->local_buf_in_use);
		if (pool->local_buf_in_use == NULL) {
			PCI_ERROR(("Queue_rxcomplete: all local buffers in Use\n"));
			return;
		}
		pool->pend_item_cnt = 0;
	}
	ASSERT(pool->pend_item_cnt < pool->item_cnt);
	if (pool->pend_item_cnt >= pool->item_cnt) {
		PCI_ERROR(("ERROR: rxcpl count overflowing locl buf, %d, %d..\n",
			pool->pend_item_cnt, pool->item_cnt));
		ASSERT(0);
	}

	rxcmplt_h = (host_rxbuf_cmpl_t *)(pool->local_buf_in_use +
		(RING_LEN_ITEMS(ring) * pool->pend_item_cnt));

		rxcpl_info = pciedev->rxcpl_list_h;
		if (rxcpl_info == pciedev->rxcpl_list_t)
			pciedev->rxcpl_list_h = pciedev->rxcpl_list_t = NULL;
		else
			pciedev->rxcpl_list_h = bcm_id2rxcplinfo(rxcpl_info->rxcpl_id.next_idx);

		pciedev->rxcpl_pend_cnt--;

		pool->pend_item_cnt++;

	rxcmplt_h->cmn_hdr.msg_type = MSG_TYPE_RX_CMPLT;
	rxcmplt_h->rx_status_0 = 0;
	rxcmplt_h->rx_status_1 = 0;
	/* For now we are setting only 802.3 or 802.11 in the flags */
	if (!rxcpl_info->rxcpl_id.dot11)
		rxcmplt_h->flags = htol16(BCMPCIE_PKT_FLAGS_FRAME_802_3);
	else
		rxcmplt_h->flags = htol16(BCMPCIE_PKT_FLAGS_FRAME_802_11);
	/* fill the useful part now */
	rxcmplt_h->cmn_hdr.flags = (rxcmplt_h->cmn_hdr.flags & (~MSGBUF_RING_INIT_PHASE)) |
		(ring->current_phase & MSGBUF_RING_INIT_PHASE);
		rxcmplt_h->cmn_hdr.if_id = rxcpl_info->rxcpl_id.ifidx;
		rxcmplt_h->cmn_hdr.request_id = htol32(rxcpl_info->host_pktref);
		rxcmplt_h->data_offset = htol16(rxcpl_info->rxcpl_len.dataoffset);
		rxcmplt_h->data_len = htol16(rxcpl_info->rxcpl_len.datalen);
		rxcmplt_h->metadata_len = htol16((rxcpl_info->rxcpl_len.metadata_len_w << 2));
		bcm_free_rxcplinfo(rxcpl_info);

	ASSERT(pool->pend_item_cnt <=
		(CHECK_WRITE_SPACE(DNGL_RING_RPTR(ring), WRT_PEND(ring), RING_MAX_ITEM(ring))));

	avail_ring_entry = MIN(pool->item_cnt,
		CHECK_WRITE_SPACE(DNGL_RING_RPTR(ring), WRT_PEND(ring), RING_MAX_ITEM(ring)));

#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
	pciedev_host_pktid_audit(pciedev->host_pktid_audit,
	                         rxcmplt_h->cmn_hdr.request_id, FALSE);
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */

#if defined(PCIE_M2M_D2H_SYNC)
	PCIE_M2M_D2H_SYNC_MARKER_INSERT(rxcmplt_h, RING_LEN_ITEMS(ring),
		pciedev->rxbuf_cmpl_epoch);
#endif /* PCIE_M2M_D2H_SYNC */

	if (pool->pend_item_cnt >= avail_ring_entry) {
		ASSERT(pool->pend_item_cnt <= avail_ring_entry);
		pciedev_xmit_rxcomplete(pciedev, ring);
		if (pciedev->rxcompletion_pend)
			break;
	}
}
}

/* D2H transfers for message packets */
uint8
pciedev_xmit_msgbuf_packet(struct dngl_bus *pciedev, void *p, uint16  msglen, msgbuf_ring_t *msgbuf)
{
	ret_buf_t ret_buf = {0};
	cmn_msg_hdr_t *msg;
	uint8 current_phase = msgbuf->current_phase;

	/* get ring space from d2h ring */
	ret_buf.low_addr = (uint32)pciedev_get_ring_space(pciedev, msgbuf, msglen);

	if (ret_buf.low_addr == NULL) {

		PCI_ERROR(("msgbuf name: DtoH%c pciedev_xmit_msgbuf_packet:"
			"DTOH ring not available \n", msgbuf->name[3]));
		return FALSE;
	}
	ret_buf.high_addr = (uint32)HIGH_ADDR_32(msgbuf->ringmem->base_addr);

	/* Inject the phase bit into flags in cmn_msg_hdr_t
	 * queued MSG_TYPE_RX_CMPLT and MSG_TYPE_TX_STATUS
	 * have the phase bit set already
	 */
	msg = (cmn_msg_hdr_t *)p;
	msg->flags = (msg->flags & (~MSGBUF_RING_INIT_PHASE)) |
		(current_phase & MSGBUF_RING_INIT_PHASE);

#if defined(PCIE_M2M_D2H_SYNC)
	if (msg->msg_type == MSG_TYPE_TX_STATUS)
		PCIE_M2M_D2H_SYNC_MARKER_REPLACE((host_txbuf_cmpl_t *)p,
			RING_LEN_ITEMS(msgbuf));
	else if (msg->msg_type == MSG_TYPE_RX_CMPLT)
		PCIE_M2M_D2H_SYNC_MARKER_REPLACE((host_rxbuf_cmpl_t *)p,
			RING_LEN_ITEMS(msgbuf));
	else
		PCIE_M2M_D2H_SYNC_MARKER_INSERT((ctrl_compl_msg_t *)p,
			RING_LEN_ITEMS(msgbuf), pciedev->ctrl_compl_epoch);
#endif /* PCIE_M2M_D2H_SYNC */

	pciedev_tx_msgbuf(pciedev, (void *) p, &ret_buf, msglen, msgbuf);

	/*
	 * Update IPC Data
	 * queued rx_cmplt and tx_status are already counted
	 * through pciedev.c
	 */
	if ((msg->msg_type != MSG_TYPE_TX_STATUS) && (msg->msg_type != MSG_TYPE_RX_CMPLT))
		pciedev->metrics.num_completions++;

	if (msgbuf->wr_pending == 0) {
		msgbuf->current_phase = (~current_phase) & MSGBUF_RING_INIT_PHASE;
		PCI_TRACE(("msgbuf name:  DtoH flipping the phase from 0x%02x to 0x%02x\n",
			current_phase, msgbuf->current_phase));
	}

	return TRUE;
}
int
pciedev_create_d2h_messages_tx(struct dngl_bus *pciedev, void *p)
{
#ifdef PCIE_PWRMGMT_CHECK
	if ((pciedev->in_d3_suspend) && (pciedev->host_wake_gpio == CC_GCI_GPIO_INVALID)) {
		PKTFREE(pciedev->osh, p, TRUE);
		return 0;
	}
#endif /* PCIE_PWRMGMT_CHECK */

	return (pciedev_create_d2h_messages(pciedev, p, pciedev->dtoh_rxcpl));
}

/* Prepare the Messages for D2H message transfers */
/* Add header info according to message type and trigger actual dma */
static int
pciedev_create_d2h_messages(struct dngl_bus *bus, void *p, msgbuf_ring_t *msgbuf)
{
	int ret = TRUE;
	uint16  msglen;
	uint8 msgtype;
	void * pkt;
	struct dngl_bus *pciedev = (struct dngl_bus *)bus;
	cmn_msg_hdr_t * msg;


#ifdef PCIE_PWRMGMT_CHECK
	if (pciedev->in_d3_suspend) {
		/* Send Host_Wake Signal */
		pciedev_host_wake_gpio_enable(pciedev, TRUE);
	}
#endif /* PCIE_PWRMGMT_CHECK */


	msg = (cmn_msg_hdr_t *)PKTDATA(pciedev->osh, p);
	msgtype = msg->msg_type;
	msglen = msgbuf->ringmem->len_items;

	switch (msgtype) {
		case MSG_TYPE_LOOPBACK:
			PCI_TRACE(("MSG_TYPE_LOOPBACK: \n"));
			pkt = MALLOC(pciedev->osh, msglen);
			if (pkt == NULL) {
				PCI_ERROR(("Could not allocate memory, malloc failed\n"));
				PCIEDEV_MALLOC_ERR_INCR(pciedev);
				return FALSE;
			}
			bcopy(PKTDATA(pciedev->osh, p), pkt, msglen);
			PKTFREE(pciedev->osh, p, TRUE);
			if (!pciedev_xmit_msgbuf_packet(pciedev, pkt, msglen,
				pciedev->dtoh_ctrlcpl)) {
				MFREE(pciedev->osh, pkt, msglen);
				return FALSE;
			}
			return TRUE;
			break;
		case MSG_TYPE_WL_EVENT:
			PCI_TRACE(("MSG_TYPE_WL_EVENT: \n"));
#ifdef UART_TRANSPORT
			/* check for MSGTRACE event and send that up to host over UART */
			if (pciedev->uarttrans_enab) {
				bcm_event_t *evtmsg = (bcm_event_t *) &msg[1];
				uint32 event_type = ntoh32_ua(&evtmsg->event.event_type);
				if (event_type < WLC_E_LAST &&
				    isset(pciedev->uart_event_inds_mask, event_type)) {
					h5_send_msgbuf((uchar *)msg, PKTLEN(pciedev->osh, p),
					               msgtype, pciedev->event_seqnum);
					++pciedev->event_seqnum;
					if (event_type == WLC_E_TRACE) {
						msgtrace_hdr_t *trace_msg;
						trace_msg = (msgtrace_hdr_t*)&evtmsg[1];
#ifdef LOGTRACE
						if (trace_msg->trace_type == MSGTRACE_HDR_TYPE_LOG)
							logtrace_sent();
#endif
#ifdef MSGTRACE
						if (trace_msg->trace_type == MSGTRACE_HDR_TYPE_MSG)
							msgtrace_sent();
#endif
					}
					PKTFREE(pciedev->osh, p, TRUE);
					return TRUE;
				}
			}
#endif /* UART_TRANSPORT */
			pciedev_queue_d2h_req(pciedev, p);
			if (pciedev->ioctl_pend != TRUE)
				pciedev_queue_d2h_req_send(pciedev);
			break;
		case MSG_TYPE_RX_PYLD:
			PCI_TRACE(("MSG_TYPE_RX_PYLD\n"));
			if (pciedev->in_d3_suspend &&
				++pciedev->in_d3_pktcount > PCIE_IN_D3_SUSP_PKTMAX) {
				PCI_TRACE(("In D3 Suspend. Dropping packet..\n"));
				PKTFREE(pciedev->osh, p, TRUE);
				return FALSE;
			}
			pciedev_queue_d2h_req(pciedev, p);
			break;
		default:
			PCI_TRACE(("pciedev_create_d2h_messages:"
				"Unknown msgtype %d \n", msgtype));
			break;
	}
	return ret;
}

/* Return host address & bufid stored in the rx frag */
/* Returned address is used by PCIe dma to transfer pending portions of payload + .3 hdr */
/* returned address should account for local pktlength + pad + dma offset */
/* Returned dataoffset specifies the start-addr of the payload in the host buffer */
static dma64addr_t
pciedev_get_haddr_from_lfrag(struct dngl_bus *pciedev, void* p, uint32* bufid,
	dma64addr_t *metaaddr, uint16 *metalen, uint16 *dataoffset)
{
	dma64addr_t haddr;
	uint32 addr_offset;
	uint32 pktlen = PKTLEN(pciedev->osh, p);


	*metalen = PKTFRAGMETADATALEN(pciedev->osh, p);
	/* Access addr only if length is valid */
	if (*metalen) {
		metaaddr->hiaddr = PKTFRAGMETADATA_HI(pciedev->osh, p);
		metaaddr->loaddr = PKTFRAGMETADATA_LO(pciedev->osh, p);
	}

	/* Packet structure in host
	 * Unused area
	 * dma_rx_offset(pciedev->d2h_dma_rxoffset)
	 * alignment for 4 bytes(pad)
	 * pending pkt from TCM + .3 hdr(pktlen)
	 * pkt dmaed by d11 dma(start addr at addr_offset)
	 * -----------------
	*/
	if (PKTFRAGUSEDLEN(pciedev->osh, p)) {
		/* Some part of host buffer already contains partial payload. So, we */
		/* need to stitch the packet up from the start of the existing payload. */

		/* We had reserved PKTRXFRAGSZ to be stitched back by mem2mem dma */
		/* But after hdr conversion, if length is less than that */
		/* account for unused area in data offset */
		*dataoffset = PKTRXFRAGSZ - pktlen;

		/* Retrieve host address stored in rx frag */
		addr_offset = PKTFRAGDATA_LO(pciedev->osh, p, 1);
		haddr.hiaddr = PKTFRAGDATA_HI(pciedev->osh, p, 1);
		/* account for data offset coming from pcie dma here */
		haddr.loaddr = (addr_offset - pktlen - pciedev->d2h_dma_scratchbuf_len);
	} else {
		/* No part of host buffer has been used yet. */
		/* account for data offset coming from pcie dma here */
		*dataoffset = pciedev->d2h_dma_scratchbuf_len;
		haddr.hiaddr = PKTFRAGDATA_HI(pciedev->osh, p, 1);
		haddr.loaddr = PKTFRAGDATA_LO(pciedev->osh, p, 1) - PKTRXFRAGSZ;
	}

#ifdef PCIE_PHANTOM_DEV
	/* Destination address has to be 4 byte aligned for phantom dev dmas */
	uint8 pad = 0;
	pad = ((uint32)haddr.loaddr & 3);

	if (pad) {
		pad = 4 - pad;
		/* Adjust host addrress */
		haddr.loaddr = haddr.loaddr - pad;
		PKTPUSH(pciedev->osh, p, pad);
	}
#endif

	/* Buffer id */
	*bufid = PKTFRAGPKTID(pciedev->osh, p);

	/* host address for this frag was used by d11 dma */
	/* reset host addr avail flag */
	PKTRESETRXFRAG(pciedev->osh, p);

	/* Return dest addr for PCIe dma */
	return haddr;
}

static void
pciedev_xmit_txstatus(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	lcl_buf_pool_t *pool = ring->buf_pool;

	if (pool->pend_item_cnt == 0)
		return;

	ASSERT(pool->pend_item_cnt <=
		CHECK_WRITE_SPACE(DNGL_RING_RPTR(ring), WRT_PEND(ring), RING_MAX_ITEM(ring)));
	if ((PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC) < MIN_RXDESC_AVAIL) ||
		(PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, TXDESC) < MIN_TXDESC_AVAIL)) {
		pciedev->txcompletion_pend = TRUE;
		PCI_TRACE(("tx cplt message failed :len %d\n",
			(RING_LEN_ITEMS(ring) * pool->pend_item_cnt)));
		return;
	}
	if (!pciedev_xmit_msgbuf_packet(pciedev, pool->local_buf_in_use,
		(RING_LEN_ITEMS(ring) * pool->pend_item_cnt), ring))
	{
		pciedev->txcompletion_pend = TRUE;
		PCI_ERROR(("tx cplt message failed : not handled now , len %d\n",
			(RING_LEN_ITEMS(ring) * pool->pend_item_cnt)));
		ASSERT(0);
		return;
	}

	/* Update IPC Stats */
	pciedev->metrics.num_txstatus_drbl++;
	pciedev->metrics.num_txstatus += pool->pend_item_cnt;
	pciedev->metrics.num_completions += pool->pend_item_cnt;

	pool->pend_item_cnt = 0;
	pool->local_buf_in_use = NULL;
	pciedev->txcompletion_pend = FALSE;
}


/* Call back fn for every pkt free */
/* for tx frag, send out tx status */
/* for rx frags with host addr valid, reclaim host addresses */
bool
pciedev_lbuf_callback(void *arg, void* p)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) arg;

	if (PKTISTXFRAG(pciedev->osh, p)) {
		cmn_msg_hdr_t *cmn_msg;
		if (!PKTHASMETADATA(pciedev->osh, (struct lbuf *)p))
			return FALSE;

		PKTPUSH(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
		cmn_msg = (cmn_msg_hdr_t *)PKTDATA(pciedev->osh, p);
		/* check for tx frag */
		if (PKTHEADROOM(pciedev->osh, p) < 8) {
			PCI_ERROR(("PKTHEADROOM is less than needed 8, %d\n",
				PKTHEADROOM(pciedev->osh, p)));
			ASSERT(0);
		}
		cmn_msg->msg_type = MSG_TYPE_TXMETADATA_PYLD;
		/* if (pktid != DMA_XFER_PKTID) */
		/* if the txstatus is pending for a packet should we reduce the count here */
		if (pciedev->d3_wait_for_txstatus) {
			pciedev->d3_wait_for_txstatus--;
			if (!pciedev->d3_wait_for_txstatus && pciedev->d3_ack_pending)
				pciedev_D3_ack(pciedev);
		}
		pciedev_queue_d2h_req(pciedev, p);
		return TRUE;
	} else if (!PKTISTXFRAG(pciedev->osh, p)) {
		uint16 rxcpl_id;
		rxcpl_info_t *p_rxcpl_info;

		rxcpl_id = PKTRXCPLID(pciedev->osh, p);
		if (rxcpl_id == 0)
			return FALSE;

		/* that means pkt did not go through pciedev rx path */
		/* see if this is carrying a chain of rxcplids  */
		p_rxcpl_info = bcm_id2rxcplinfo(rxcpl_id);

		if (!BCM_RXCPL_IN_TRANSIT(p_rxcpl_info))
			return FALSE;

		PKTRESETRXCPLID(pciedev->osh, p);
		BCM_RXCPL_CLR_IN_TRANSIT(p_rxcpl_info);
		BCM_RXCPL_CLR_VALID_INFO(p_rxcpl_info);
		/* call the queue logic, let it handle the dropping of rxcpl info */
		if (PKTNEEDRXCPL(pciedev->osh, p) || BCM_RXCPL_FRST_IN_FLUSH(p_rxcpl_info)) {
			pciedev_queue_rxcomplete_local(pciedev, p_rxcpl_info, pciedev->dtoh_rxcpl,
				BCM_RXCPL_FRST_IN_FLUSH(p_rxcpl_info));
		}
	}
	return FALSE;
}

static void
pciedev_queue_txstatus(struct dngl_bus *pciedev, uint32 bufid, uint8 ifindx, uint16 ringid,
	uint16 txstatus, uint16 metadata_len, msgbuf_ring_t *ring)
{
	lcl_buf_pool_t *pool = ring->buf_pool;
	host_txbuf_cmpl_t *txcmplt_h;
	uint32 avail_ring_entry;

	if (bufid == 0) {
#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
		pciedev_host_pktid_audit(pciedev->host_pktid_audit, bufid, FALSE);
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */
		PCI_ERROR(("pktid is NULL\n"));
		ASSERT(0);
		return;
	}

	if (pool->local_buf_in_use == NULL) {
		pool->local_buf_in_use = pciedev_get_lclbuf_pool(ring);
		if (pool->local_buf_in_use == NULL)
			return;

		ASSERT(pool->local_buf_in_use);
		pool->pend_item_cnt = 0;
	}
	ASSERT(pool->pend_item_cnt < pool->item_cnt);

	txcmplt_h = (host_txbuf_cmpl_t *)(pool->local_buf_in_use +
		(RING_LEN_ITEMS(ring) * pool->pend_item_cnt));
	pool->pend_item_cnt++;

	txcmplt_h->cmn_hdr.msg_type = MSG_TYPE_TX_STATUS;
	txcmplt_h->cmn_hdr.if_id = ifindx;
	txcmplt_h->cmn_hdr.flags = (txcmplt_h->cmn_hdr.flags & (~MSGBUF_RING_INIT_PHASE)) |
		(ring->current_phase & MSGBUF_RING_INIT_PHASE);
	txcmplt_h->compl_hdr.status = 0;
	txcmplt_h->compl_hdr.flow_ring_id = ringid;

	/* useful status */
	txcmplt_h->cmn_hdr.request_id = htol32(bufid);
	txcmplt_h->metadata_len = htol16(metadata_len);
	txcmplt_h->tx_status = htol16(txstatus);

	ASSERT(pool->pend_item_cnt <=
		CHECK_WRITE_SPACE(DNGL_RING_RPTR(ring), WRT_PEND(ring), RING_MAX_ITEM(ring)));

#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
	pciedev_host_pktid_audit(pciedev->host_pktid_audit,
	                         txcmplt_h->cmn_hdr.request_id, FALSE);
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */

#if defined(PCIE_M2M_D2H_SYNC)
	PCIE_M2M_D2H_SYNC_MARKER_INSERT(txcmplt_h, RING_LEN_ITEMS(ring),
		pciedev->txbuf_cmpl_epoch);
#endif /* PCIE_M2M_D2H_SYNC */

	avail_ring_entry = MIN(pool->item_cnt,
		CHECK_WRITE_SPACE(DNGL_RING_RPTR(ring), WRT_PEND(ring), RING_MAX_ITEM(ring)));

	if (pool->pend_item_cnt >= avail_ring_entry) {
		ASSERT(pool->pend_item_cnt <= avail_ring_entry);
		pciedev_xmit_txstatus(pciedev, ring);
	}
}

static void
pciedev_xmit_rxcomplete(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	lcl_buf_pool_t *pool = ring->buf_pool;

	if (pool->pend_item_cnt == 0)
		return;
	if ((PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC) < MIN_RXDESC_AVAIL) ||
		(PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, TXDESC) < MIN_TXDESC_AVAIL)) {
		pciedev->rxcompletion_pend = TRUE;
		PCI_ERROR(("rx cplt message failed : size %d \n",
			(RING_LEN_ITEMS(ring) * pool->pend_item_cnt)));
		return;
	}
	if (!pciedev_xmit_msgbuf_packet(pciedev, pool->local_buf_in_use,
		(RING_LEN_ITEMS(ring) * pool->pend_item_cnt), ring))
	{
		pciedev->rxcompletion_pend = TRUE;
		PCI_ERROR(("rx cplt message failed : not handled now, size %d \n",
			(RING_LEN_ITEMS(ring) * pool->pend_item_cnt)));
		ASSERT(0);
		return;
	}

	/* Update IPC Stats */
	pciedev->metrics.num_rxcmplt_drbl++;
	pciedev->metrics.num_rxcmplt += pool->pend_item_cnt;
	pciedev->metrics.num_completions += pool->pend_item_cnt;

	pool->pend_item_cnt = 0;
	pool->local_buf_in_use = NULL;
	pciedev->rxcompletion_pend = FALSE;
}
void
pciedev_queue_d2h_req_send(struct dngl_bus *pciedev)
{
	uint32 txdesc, rxdesc;
	void *p;

	if (pciedev->in_d3_suspend) {
		/* Do not service the queue if the device is in D3 suspend */
		PCI_TRACE(("pciedev_queue_d2h_req_send:In D3 Suspend\n"));
		return;
	}
	if (pciedev->ioctl_pend == TRUE) {
		/* ioctl is pending waiting for resources to be avilable */
		/* so check it the IOCTL could be processed now */
		/* all the needed from the IOCTL request is already in dongle memory */
		pciedev_process_ioctl_pend(pciedev);
		if (pciedev->ioctl_pend) {
			PCI_ERROR(("not enough resources to process ioctl request\n"));
			return;
		}
	}
	if (pciedev->rxcompletion_pend == TRUE) {
		/* tried to send rxcompletion but looks like failed with no resources */
		/* so try it now */
		pciedev_xmit_rxcomplete(pciedev, pciedev->dtoh_rxcpl);
	}
	if (pciedev->txcompletion_pend == TRUE) {
		/* tried to send rxcompletion but looks like failed with no resources */
		/* so try it now */
		pciedev_xmit_txstatus(pciedev, pciedev->dtoh_txcpl);
	}

	/* check if there are any pending rxcompletes waiting for space on local buf */
	pciedev_queue_rxcomplete_msgring(pciedev, pciedev->dtoh_rxcpl);

	if (pktq_empty(&pciedev->d2h_req_q))
		return;

	txdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, TXDESC);
	rxdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC);

	while ((txdesc > MIN_TXDESC_AVAIL) && (rxdesc > MIN_RXDESC_AVAIL)) {
		d2h_msg_handler msg_handler;
		p = pktq_pdeq(&pciedev->d2h_req_q, 0);
		if (p == NULL)
			break;
		if (!pciedev_check_process_d2h_message(pciedev, txdesc, rxdesc, p, &msg_handler)) {
			PCI_TRACE(("not enough resources to send event to Host\n"));
			pktq_penq_head(&pciedev->d2h_req_q, 0, p);
			break;
		}
		if (msg_handler) {
			(msg_handler)(pciedev, p);
		}
		if (pciedev->rxcompletion_pend | pciedev->ioctl_pend | pciedev->txcompletion_pend) {
			PCI_TRACE(("pends are Rx: %d, Tx: %d, IOCTL: %d\n",
				pciedev->rxcompletion_pend, pciedev->txcompletion_pend,
				pciedev->ioctl_pend));
			break;
		}

		txdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, TXDESC);
		rxdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC);
	}
	/* Send rx complete for the chain of pkts */
	pciedev_xmit_rxcomplete(pciedev, pciedev->dtoh_rxcpl);
}

void
pciedev_queue_d2h_req(struct dngl_bus *pciedev, void *p)
{
	if (pktq_full(&pciedev->d2h_req_q)) {
		PCI_ERROR(("PANIC: pciedev_queue_d2h_req full\n"));

		/* Reset the Metadata flag to avoid recursion of PKTFREE */
		PKTRESETHASMETADATA(pciedev->osh, (struct lbuf *) p);

		/* Callbacks should not be invoked again */
		PKTFREE(pciedev->osh, p, FALSE);
		return;
	}
	pktq_penq(&pciedev->d2h_req_q, 0, p);
}

int
pciedev_tx_pyld(struct dngl_bus *pciedev, void *p, ret_buf_t *data_buf, uint16 data_len,
	ret_buf_t *meta_data_buf, uint16 meta_data_len, uint8 msgtype)
{
	dma64addr_t addr = {0, 0};
	uint16 msglen;
	uint8 txdesc = 0;
	uint8 rxdesc = 0;


	if ((meta_data_len > 0) && (meta_data_len < PCIE_MEM2MEM_DMA_MIN_LEN)) {
		meta_data_len = PCIE_MEM2MEM_DMA_MIN_LEN;
		ASSERT(0);
	}
	if ((data_len > 0) && (data_len < PCIE_MEM2MEM_DMA_MIN_LEN)) {
		data_len = PCIE_MEM2MEM_DMA_MIN_LEN;
	}

	msglen = data_len + meta_data_len;
	if (msglen < PCIE_MEM2MEM_DMA_MIN_LEN) {
		PCI_ERROR(("msglen is %d, data_len %d, meta_data_len %d\n",
			msglen, data_len, meta_data_len));
		ASSERT(0);
	}

#ifdef PCIE_PHANTOM_DEV
	/* For phantom devices PCIE_ADDR_OFFSET is still required for sdio dma */
	/* for pcie dma devices, outbound dma is hardwired */
	/* to take in only pcie addr as dest space. so no need to give extra offset */
	if (pciedev->phtm) {
		PHYSADDR64HISET(addr, data_buf->high_addr);
		PHYSADDR64LOSET(addr, data_buf->low_addr);
		pcie_phtm_tx(pciedev->phtm, p, addr, msglen, l_msgtype, &txdesc, &rxdesc);
	}
#else
	/* Program RX descriptor for rxoffset */
	/* offset is handled through dataoffset portion of rx completion  for rx payloads */
	/* None of the other messages have this dataoffset field */
	if ((pciedev->d2h_dma_scratchbuf_len) && (msgtype != MSG_TYPE_RX_PYLD)) {
		if (dma_rxfast(pciedev->d2h_di, pciedev->d2h_dma_scratchbuf, 8)) {
			PCI_ERROR(("pciedev_tx_pyld: dma_rxfast failed "
			"for rxoffset, descs_avail %d\n",
				PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
		} else {
			rxdesc++;
		}
	}
	/* check if there is a need to transfer metadata */
	if (meta_data_len) {
		meta_data_buf->high_addr = meta_data_buf->high_addr & ~PCIE_ADDR_OFFSET;
		PHYSADDR64HISET(addr, meta_data_buf->high_addr);
		PHYSADDR64LOSET(addr, meta_data_buf->low_addr);
		if (dma_rxfast(pciedev->d2h_di, addr, (uint32)meta_data_len)) {
			PCI_ERROR(("pciedev_tx_pyld : dma_rxfast"
				"failed for meta data, descs avail %d\n",
				PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
		} else {
			rxdesc++;
		}
	}
	/* check if there is a need to transfer real data */
	if (data_len) {
		data_buf->high_addr = data_buf->high_addr & ~PCIE_ADDR_OFFSET;
		PHYSADDR64HISET(addr, data_buf->high_addr);
		PHYSADDR64LOSET(addr, data_buf->low_addr);
		if (dma_rxfast(pciedev->d2h_di, addr, (uint32)data_len)) {
			PCI_ERROR(("pciedev_tx_pyld : dma_rxfast failed for data, descs avail %d\n",
				PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
		} else {
			rxdesc++;
		}
	}

	if ((PKTLEN(pciedev->osh, p) < msglen) && (msglen == PCIE_MEM2MEM_DMA_MIN_LEN)) {
		PCI_INFORM(("ALIGN: dmalen(%d) < minimum(%d), tailroom %d, msgtype %d\n",
			PKTLEN(pciedev->osh, p), msglen,  PKTTAILROOM(pciedev->osh, p), msgtype));
		/* to cover one of the HW wars we need packet len to be a min of 8 */
		if (PKTTAILROOM(pciedev->osh, p) < msglen - pkttotlen(pciedev->osh, p)) {
			PCI_ERROR(("BAD CASE: dmalen(%d) < minimum(%d), tailroom %d, msgtype %d\n",
				PKTLEN(pciedev->osh, p), msglen,
				PKTTAILROOM(pciedev->osh, p), msgtype));
		}
		PKTSETLEN(pciedev->osh, p, msglen);
	}
	dma_txfast(pciedev->d2h_di, p, TRUE);
	txdesc++;
#endif /* PCIE_PHANTOM_DEV */

	pciedev_dma_tx_account(pciedev, msglen, msgtype, 0, NULL, txdesc, rxdesc);
	return TRUE;
}


/* Handle D2H dma transfers for messages and payload */
/* Program RX and TX descriptors */
static int
pciedev_tx_msgbuf(struct dngl_bus *pciedev, void *p, ret_buf_t *ret_buf,
	uint16 msglen, msgbuf_ring_t *msgbuf)
{
	cmn_msg_hdr_t *cmn_hdr = (cmn_msg_hdr_t *) p;
	dma64addr_t addr = {0, 0};
	uint32 phase_offset = 0;
	bool commit = TRUE;
	uint32 ringupd_len;
	uint8 flags = 0;
	bool evnt_ctrl = 0;
	uint8 txdesc = 0;
	uint8 rxdesc = 0;

	if (msgbuf->phase_supported) {
		phase_offset = sizeof(cmn_msg_hdr_t);
		flags |= PCIEDEV_INTERNAL_SENT_D2H_PHASE;
	}
	ringupd_len = pciedev->d2h_ringupd_buf_len;
	if (pciedev->d2h_ringupd_inited && msgbuf->dmarx_indices_supported && ringupd_len)
		flags |= PCIEDEV_INTERNAL_SENT_D2H_RINGUPD;
	else
		ringupd_len = 0;

	ASSERT(msglen >= (phase_offset + PCIE_MEM2MEM_DMA_MIN_LEN));

#ifndef PCIE_PHANTOM_DEV
	/* For phantom devices PCIE_ADDR_OFFSET is still required for sdio dma */
	/* for pcie dma devices, outbound dma is hardwired */
	/* to take in only pcie addr as dest space. so no need to give extra offset */
#endif

#ifdef PCIE_PHANTOM_DEV
	PHYSADDR64HISET(addr, ret_buf->high_addr);
	PHYSADDR64LOSET(addr, ret_buf->low_addr + phase_offset);
	if (pciedev->phtm)
		pcie_phtm_tx(pciedev->phtm, p, addr, msglen, cmn_hdr->msgtype, &txdesc, &rxdesc);
#else
	ret_buf->high_addr = ret_buf->high_addr & ~PCIE_ADDR_OFFSET;
	/* Program RX descriptor */
	if (pciedev->d2h_dma_scratchbuf_len) {
		if (dma_rxfast(pciedev->d2h_di, pciedev->d2h_dma_scratchbuf, 8)) {
			PCI_ERROR(("msgbuf name: pciedev_tx_msgbuf: DtoH%c"
				"dma_rxfast failed for rxoffset, descs_avail %d\n",
				msgbuf->name[3], PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
		} else {
			rxdesc++;
		}
	}
	PHYSADDR64HISET(addr, ret_buf->high_addr);
	PHYSADDR64LOSET(addr, ret_buf->low_addr + phase_offset);
	/* rx buffer for data excluding the phase bits */
	if (dma_rxfast(pciedev->d2h_di, addr, (uint32)(msglen - phase_offset))) {
		PCI_ERROR(("msgbuf name :DtoH  pciedev_tx_msgbuf :DtoH%c"
			"dma_rxfast failed for data, descs avail %d\n",
			msgbuf->name[3], PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
	} else {
		rxdesc++;
	}

	if (phase_offset) {
		/* rx buffer for phase offset */
		PHYSADDR64LOSET(addr, ret_buf->low_addr);
		if (dma_rxfast(pciedev->d2h_di, addr, phase_offset)) {
			PCI_ERROR(("msgbuf name : DtoH%c  pciedev_tx_msgbuf :"
				"dma_rxfast failed for data, descs avail %d\n",
				msgbuf->name[3], PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
		} else {
			rxdesc++;
		}
	}
	/* check if we need to dma the dongle ring dma updates to host */
	if (ringupd_len) {
		PCI_TRACE(("rx: sending the ringupd as well, len %d\n", ringupd_len));
		if (dma_rxfast(pciedev->d2h_di, pciedev->d2h_ringupd_buf, ringupd_len)) {
			PCI_ERROR(("msgbuf name: DtoH  pciedev_tx_msgbuf: DtoH%c"
				"dma_rxfast failed for ringupd, descs avail %d\n",
				msgbuf->name[3], PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
		} else {
			rxdesc++;
		}
		commit = FALSE;
	}
	if (phase_offset) {
		/* tx for data excluding the phase offset */
		PHYSADDR64HISET(addr, (uint32) 0);
		PHYSADDR64LOSET(addr, (uint32)p + phase_offset);
		if (dma_msgbuf_txfast(pciedev->d2h_di, addr, FALSE, msglen - phase_offset,
			TRUE, FALSE)) {
			evnt_ctrl = PCIE_ERROR1;
			goto fail;
		} else {
			txdesc++;
		}
		/* tx for phase offset */
		PHYSADDR64LOSET(addr, (uint32)p);
		if (dma_msgbuf_txfast(pciedev->d2h_di,
			addr, commit, phase_offset, FALSE, commit)) {
			evnt_ctrl = PCIE_ERROR2;
			goto fail;
		} else {
			txdesc++;
		}
	}
	else {
		PHYSADDR64HISET(addr, (uint32) 0);
		PHYSADDR64LOSET(addr, (uint32)p);
		if (dma_msgbuf_txfast(pciedev->d2h_di, addr, commit, msglen, TRUE, commit)) {
			evnt_ctrl = PCIE_ERROR3;
			goto fail;
		} else {
			txdesc++;
		}
	}
	if (ringupd_len) {
		PCI_TRACE(("tx: sending the ringupd as well, len %d\n", ringupd_len));
		if (dma_msgbuf_txfast(pciedev->d2h_di, pciedev->local_d2h_ringupd_dmabuf,
			TRUE, ringupd_len, FALSE, TRUE)) {
			evnt_ctrl = PCIE_ERROR4;
			goto fail;
		} else {
			txdesc++;
		}
	}
#endif /* PCIE_PHANTOM_DEV */
fail:

	pciedev_dma_tx_account(pciedev, msglen, cmn_hdr->msg_type, flags, msgbuf, txdesc, rxdesc);

	if (evnt_ctrl) {
		PCI_ERROR(("pciedev_tx_msgbuf : dma fill failed %d\n", evnt_ctrl));
		ASSERT(0);
		}

	return TRUE;
}

void
pciedev_dma_tx_account(struct dngl_bus *pciedev, uint16 msglen, uint8 msgtype,
	uint8 flags, msgbuf_ring_t *msgbuf, uint8 txdesc, uint8 rxdesc)
{
	uint16 wr_idx = pciedev->dtoh_dma_wr_idx;

	pciedev->dtoh_dma_q[wr_idx].len = msglen;
	pciedev->dtoh_dma_q[wr_idx].flags = flags;
	pciedev->dtoh_dma_q[wr_idx].msg_type = msgtype;
	pciedev->dtoh_dma_q[wr_idx].msgbuf = msgbuf;
	pciedev->dtoh_dma_q[wr_idx].txpend = txdesc;
	pciedev->dtoh_dma_q[wr_idx].rxpend = rxdesc;

	pciedev->dtoh_dma_wr_idx = (wr_idx+1) % MAX_DMA_QUEUE_LEN_D2H;

	ASSERT(pciedev->dtoh_dma_wr_idx != pciedev->dtoh_dma_rd_idx);
}

static uint16
pciedev_handle_h2d_pyld(struct dngl_bus *pciedev, uint8 msgtype, void *p,
	uint16 pktlen, msgbuf_ring_t *msgbuf)
{
	switch (msgtype) {
		case MSG_TYPE_HOST_FETCH:
			pciedev_process_tx_payload(pciedev);
			break;
		case MSG_TYPE_LPBK_DMAXFER_PYLD:
			break;
		default:
			PCI_ERROR(("Unknown internal a message type 0x%02x\n", msgtype));
			break;
	}
	return 0;
}

uint16
pciedev_handle_h2d_msg_rxsubmit(struct dngl_bus *pciedev, void *p,
	uint16 pktlen, msgbuf_ring_t *msgbuf)
{
	uint8 msglen = RING_LEN_ITEMS(pciedev->htod_rx);

	ASSERT((pktlen % msglen) == 0);

	/* Increment IPC Data for rxsubmit */
	pciedev->metrics.num_submissions += pktlen / msglen;
	PCI_INFORM(("h2d_drbl:%d, new rxsubmit:%d, tot_num_submissions:%d\n",
		pciedev->metrics.num_h2d_doorbell, pktlen/msglen,
		pciedev->metrics.num_submissions));

	pciedev_add_to_inuselist(pciedev, p, pktlen/msglen);

	/* Invoke dma rxfill every time you get host rx buffers */
	if (BCMSPLITRX_ENAB())
		pktpool_invoke_dmarxfill(pciedev->pktpool_rxlfrag);

	return 0;
}

uint16
pciedev_handle_h2d_msg_txsubmit(struct dngl_bus *pciedev, void *p,
	uint16 pktlen, msgbuf_ring_t *msgbuf, uint16 f_idx)
{
	uint8 msglen = RING_LEN_ITEMS(msgbuf);
	host_txbuf_post_t *txdesc;

	ASSERT((pktlen % msglen) == 0);

	/* Increment IPC Data for txsubmit */
	pciedev->metrics.num_submissions += pktlen / msglen;
	PCI_INFORM(("h2d_drbl:%d, new txsubmit:%d, tot_num_submissions:%d\n",
		pciedev->metrics.num_h2d_doorbell, pktlen/msglen,
		pciedev->metrics.num_submissions));

	while (pktlen) {
		txdesc = (host_txbuf_post_t *)p;

		ASSERT(txdesc->cmn_hdr.msg_type == MSG_TYPE_TX_POST);

		if (msgbuf->status & FLOW_RING_FLUSH_PENDING) {
			/* Check if there are resources to queue txstatus */
			if (!pciedev_resource_avail_for_txmetadata(pciedev)) {
				pciedev_adjust_flow_fetch_ptr(msgbuf, f_idx);
			} else {
				msgbuf->status |= FLOW_RING_NOFLUSH_TXUPDATE;
				msgbuf->flow_info.pktinflight++;
				pciedev->pend_user_tx_pkts++;

				if (!pciedev_update_txstatus(pciedev, BCMPCIE_PKT_FLUSH, f_idx,
					msgbuf->ringid)) {
					pciedev_queue_txstatus(pciedev,
						ltoh32(txdesc->cmn_hdr.request_id),
						txdesc->cmn_hdr.if_id, msgbuf->ringid,
						BCMPCIE_PKT_FLUSH, 0, pciedev->dtoh_txcpl);
				}
				msgbuf->status &= ~FLOW_RING_NOFLUSH_TXUPDATE;
			}
		} else if (!(msgbuf->status & FLOW_RING_SUP_PENDING)) {
			pciedev_process_tx_post(pciedev, p, msglen, msgbuf->ringid, f_idx);
		} else {
			/* If we are already in suppress, no need to push these packets,
			 * adjust rd pending and move on
			 */
			pciedev_adjust_flow_fetch_ptr(msgbuf, f_idx);
		}
		f_idx = (f_idx + 1) %  RING_MAX_ITEM(msgbuf);
		pktlen = pktlen - msglen;
		p = p + msglen;
	}

#ifdef PKTC_TX_DONGLE
	/* We can send the chained packets here */
	if (pciedev->pkthead) {
		/* Send up data */
		dngl_sendup(pciedev->dngl, pciedev->pkthead);
		pciedev->pkt_chain_cnt = 0;
		pciedev->pkthead = pciedev->pkttail = NULL;
	}
#endif /* PKTC_TX_DONGLE */
	return pktlen;
}

/* Handler for a tx post message recieved form host
 * 1. Decode message
 * 2. Get LFRAG
 * 3. Update frag details
 * 4. Sendup to wl layer
*/
static void
pciedev_process_tx_post(struct dngl_bus *pciedev, void* p, uint16 msglen,
	uint16 ringid, uint16 fetch_idx)
{
	host_txbuf_post_t * txdesc;
	uint32 pktid;
	uint8 priority;
	uint8 exempt;
	/* Find a way to point to TXOFF in pciedev */
	/* Need 202 bytes of headroom for TXOFF, 22 bytes for amsdu path */
	uint16 headroom = 224;	/* TXOFF + amsdu headroom */
	void *lfrag;
	uint8 hdrlen = ETHER_HDR_LEN;
#ifndef BCMPCIE_SUPPORT_TX_PUSH_RING
	msgbuf_ring_t *flow_ring = &pciedev->flow_ring_msgbuf[ringid -
		BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START];
#endif
#ifdef PKTC_TX_DONGLE
	struct ether_header *eh;
	bool break_chain = FALSE;
#endif

	txdesc = (host_txbuf_post_t *)p;
#ifdef PKTC_TX_DONGLE
	eh = (struct ether_header *)txdesc->txhdr;
	if (ntoh16(eh->ether_type) == ETHER_TYPE_802_1X)
		break_chain = TRUE;
#endif
	pktid = ltoh32(txdesc->cmn_hdr.request_id);
	priority = (txdesc->flags &
		BCMPCIE_PKT_FLAGS_PRIO_MASK) >> BCMPCIE_PKT_FLAGS_PRIO_SHIFT;
	exempt = (txdesc->flags >> BCMPCIE_PKT_FLAGS_FRAME_EXEMPT_SHIFT) &
		BCMPCIE_PKT_FLAGS_FRAME_EXEMPT_MASK;

	/* nsegs supported is 1....err */

	/* Allocate a lbuf_frag  with hdrlen + headroom */
#ifndef BCMFRAGPOOL
	lfrag = PKTGETLF(pciedev->osh, headroom + hdrlen, TRUE, lbuf_frag);
	if (lfrag == NULL) {
		/* No free packets in heap. Just drop this packet. */
		PCI_TRACE(("pciedev_process_tx_post: %d: No free packets"
		"in the pool. Dropping packets here\n",
			__LINE__));
		pciedev->dropped_txpkts ++;
		return;
	}
#else
	lfrag = pktpool_get(pciedev->pktpool_lfrag);
	if (lfrag == NULL) {

		/* No free packets in the pool. Just drop this packet. */
		PCI_ERROR(("pciedev_process_tx_post:%d: No"
		"free packets in the pool. Dropping packets here\n",
			__LINE__));
		flow_ring->flow_info.pktinflight++;
		pciedev->pend_user_tx_pkts++;
		pciedev_update_txstatus(pciedev, WLFC_CTL_PKTFLAG_WLSUPPRESS, fetch_idx,  ringid);
		pciedev->dropped_txpkts ++;
		return;
	}
#endif /* BCMFRAGPOOL */

#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
	pciedev_host_pktid_audit(pciedev->host_pktid_audit, pktid, TRUE);
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */

	PKTPULL(pciedev->osh, lfrag, headroom);	/* Push headroom */

	/* Copy ether header to lfrag */
	ehcopy32(txdesc->txhdr, PKTDATA(pciedev->osh, lfrag));

	/* Set frag params */
	PKTTAG_SET_VALUE(lfrag, exempt << 16); /* WLF_EXEMPT_MASK is shifted 16 bits */
	PKTSETLEN(pciedev->osh, lfrag, hdrlen);	/* Set Len */
	PKTSETFRAGPKTID(pciedev->osh, lfrag, pktid);
	PKTSETFRAGTOTNUM(pciedev->osh, lfrag, 1);
	PKTSETPRIO(lfrag, priority);
	PKTSETIFINDEX(pciedev->osh, lfrag, txdesc->cmn_hdr.if_id);
	PKTSETFRAGDATA_HI(pciedev->osh, lfrag, 1,
		(ltoh32(txdesc->data_buf_addr.high_addr) | 0x80000000));
	PKTSETFRAGDATA_LO(pciedev->osh, lfrag, 1,
		(ltoh32(txdesc->data_buf_addr.low_addr)));
	PKTSETFRAGLEN(pciedev->osh, lfrag, 1, ltoh16(txdesc->data_len));

	/* Set tot len and tot fragment count */
	PKTSETFRAGTOTLEN(pciedev->osh, lfrag, ltoh16(txdesc->data_len));

#ifndef BCMPCIE_SUPPORT_TX_PUSH_RING
	PKTSETFRAGFLOWRINGID(pciedev->osh, lfrag, ringid);
	flow_ring->flow_info.pktinflight++;
	PKTFRAGSETRINGINDEX(pciedev->osh, lfrag, fetch_idx);
	pciedev->pend_user_tx_pkts++;
#endif
	if (ltoh16(txdesc->metadata_buf_len)) {
		PKTSETFRAGMETADATA_HI(pciedev->osh, lfrag,
			(ltoh32(txdesc->metadata_buf_addr.high_addr)));
		PKTSETFRAGMETADATA_LO(pciedev->osh, lfrag,
			(ltoh32(txdesc->metadata_buf_addr.low_addr)));
		PKTSETFRAGMETADATALEN(pciedev->osh, lfrag,
			ltoh16(txdesc->metadata_buf_len));
	}
	else {
#ifdef TEST_USE_DMASCRATCH_AS_METADATA_HDR
		PKTSETFRAGMETADATA_HI(pciedev->osh, lfrag, pciedev->d2h_dma_scratchbuf.hiaddr);
		PKTSETFRAGMETADATA_LO(pciedev->osh, lfrag, pciedev->d2h_dma_scratchbuf.loaddr);
		PKTSETFRAGMETADATALEN(pciedev->osh, lfrag, pciedev->d2h_dma_scratchbuf_len);
#endif
	}
	PKTSETHASMETADATA(pciedev->osh, (struct lbuf *)lfrag);

	PCI_TRACE(("frag tot len %d, totlen %d, lfrag is %x\n",
		PKTFRAGTOTLEN(pciedev->osh, lfrag),
		pkttotlen(pciedev->osh, lfrag), (uint32)lfrag));

	if (pciedev->lpback_test_mode) {
		pciedev->lpback_test_drops++;
		PKTFREE(pciedev->osh, lfrag, TRUE);
		return;
	}
	if (flow_ring->flow_info.flags & FLOW_RING_FLAG_PKT_REQ)
		pciedev_push_pkttag_tlv_info(pciedev, lfrag, flow_ring, fetch_idx);

#ifdef PKTC_TX_DONGLE
	/* Chain this packet to the existing chain */
	if (break_chain) {
		PKTSETCLINK(lfrag, NULL);
		dngl_sendup(pciedev->dngl, lfrag);
	} else {
		if (pciedev->pkttail) {
			PKTSETCLINK(pciedev->pkttail, lfrag);
			pciedev->pkttail = lfrag;
		} else {
			pciedev->pkthead = pciedev->pkttail = lfrag;
		}
		PKTSETCLINK(pciedev->pkttail, NULL);
		pciedev->pkt_chain_cnt++;
	}
#else
	/* No packet chaining. Send it out immediately */
	PKTSETCLINK(lfrag, NULL);
	dngl_sendup(pciedev->dngl, lfrag);
#endif /* PKTC_TX_DONGLE */
}

/* Core DMA scheduling routine for host to dongle messages */
/* Also used to pull down non inline ioctl requests */
static void pciedev_h2dmsgbuf_dma(struct dngl_bus *pciedev, dma64addr_t src,
	uint16 src_len, uint8 *dst, uint8 *dummy_rxoff, msgbuf_ring_t *msgbuf, uint8 msgtype)
{

	uint16 wr_idx;
#ifdef PCIE_PHANTOM_DEV
	if (pciedev->phtm)
		pcie_phtm_msgbuf_dma(pciedev->phtm, dst, src, src_len);
#else
	uint16 dma_len, rx_len;
	bool evnt_ctrl = 0;
	dma64addr_t addr = {0, 0};

	if (src_len  <  PCIE_MEM2MEM_DMA_MIN_LEN)
		dma_len = PCIE_MEM2MEM_DMA_MIN_LEN;
	else
		dma_len = src_len;

	rx_len = dma_len;
#ifdef PCIE_PWRMGMT_CHECK
	if (pciedev->in_d3_suspend) {
		PCI_TRACE(("pciedev_h2dmsgbuf_dma:  IN D3 Suspend!\n"));
		ASSERT(0);

		return;
	}
#endif /* PCIE_PWRMGMT_CHECK */

	if (!pciedev->force_ht_on) {
		PCI_TRACE(("%s: requesting force HT for this core\n", __FUNCTION__));
		pciedev->force_ht_on = TRUE;
		pciedev_manage_h2d_dma_clocks(pciedev);
	}

	PHYSADDR64LOSET(addr, (uint32)dst);
	if (pciedev->h2d_dma_rxoffset) {
		/* RX descriptor for rx offset */
		PHYSADDR64LOSET(addr, (uint32)dummy_rxoff);
		if (dma_rxfast(pciedev->h2d_di, addr, 8)) {
			evnt_ctrl = PCIE_ERROR1;
		}

		/* actual rx decriptor */
		PHYSADDR64LOSET(addr, (uint32)dst);
		if (dma_rxfast(pciedev->h2d_di, addr, rx_len))
			evnt_ctrl = PCIE_ERROR2;
	} else {
		if (dma_rxfast(pciedev->h2d_di, addr, rx_len))
			evnt_ctrl = PCIE_ERROR3;
	}

	if (evnt_ctrl)
		PCI_ERROR(("pciedev_h2dmsgbuf_dma : dma_rxfast failed %d\n", evnt_ctrl));

	/* TX descriptor */
	if (dma_msgbuf_txfast(pciedev->h2d_di, src, TRUE, dma_len, TRUE, TRUE))
		PCI_ERROR(("pciedev_h2dmsgbuf_dma : dma fill failed  \n"));
#endif /* PCIE_PHANTOM_DEV */

	/* Queue up pktlength */
	/* since we are using dummy rxoffset region, length is not obtained from rxoffset */
	wr_idx = pciedev->htod_dma_wr_idx;
	pciedev->htod_dma_wr_idx = (wr_idx + 1) % MAX_DMA_QUEUE_LEN_H2D;
	pciedev->htod_dma_q[wr_idx].len = src_len;
	pciedev->htod_dma_q[wr_idx].msg_type = msgtype;
	pciedev->htod_dma_q[wr_idx].msgbuf = msgbuf;

	ASSERT(pciedev->htod_dma_wr_idx != pciedev->htod_dma_rd_idx);
}

/* Processing for d2h dma completion */
/* Free up the packet if its payload or update circular buf pointers */
uint8
pciedev_handle_d2h_dmacomplete(struct dngl_bus *pciedev, void *buf)
{
	uint8 ignore_cnt = 0;
	dma_queue_t *item = &pciedev->dtoh_dma_q[pciedev->dtoh_dma_rd_idx];


	pciedev->dtoh_dma_rd_idx = (pciedev->dtoh_dma_rd_idx+1) % MAX_DMA_QUEUE_LEN_D2H;

#if defined(MSGTRACE) || defined(LOGTRACE)
	if (MESSAGE_PAYLOAD(item->msg_type)) {

		if (item->msg_type == MSG_TYPE_EVENT_PYLD) {
			bcm_event_t *bcm_hdr;

			bcm_hdr = (bcm_event_t*)PKTDATA(pciedev->osh, buf);
			if (bcm_hdr->event.event_type == hton32(WLC_E_TRACE)) {
					msgtrace_hdr_t *trace_msg;

					trace_msg = (msgtrace_hdr_t*)&bcm_hdr[1];
#ifdef LOGTRACE
					if (trace_msg->trace_type == MSGTRACE_HDR_TYPE_LOG) {
						logtrace_sent();
					}
#endif
#ifdef MSGTRACE
					if (trace_msg->trace_type == MSGTRACE_HDR_TYPE_MSG) {
							msgtrace_sent();
					}
#endif
			}
		}
	}

#endif /* defined(MSGTRACE) || defined(LOGTRACE) */
	if (MESSAGE_PAYLOAD(item->msg_type)) {
		/* Payload */
#ifdef PCIE_DMAXFER_LOOPBACK
		if (item->msg_type == MSG_TYPE_LPBK_DMAXFER_PYLD) {
			pciedev_process_do_dest_lpbk_dmaxfer_done(pciedev, buf);
		}
		else
#endif /* PCIE_DMAXFER_LOOPBACK */
		if (item->msg_type != MSG_TYPE_IOCT_PYLD)
		{
			/* IOCTL Payload free is handled with the IOCTL completion message */
			PKTFREE(pciedev->osh, buf, FALSE);
		}
	} else {
		/* free local message space */
		pciedev_ring_update_writeptr(item->msgbuf, item->len);
		if (item->flags & PCIEDEV_INTERNAL_SENT_D2H_PHASE) {
			ignore_cnt++;
			pciedev_free_lclbuf_pool(item->msgbuf, buf - sizeof(cmn_msg_hdr_t));
		}
		else {
			pciedev_free_lclbuf_pool(item->msgbuf, buf);
		}
		if (item->msg_type == MSG_TYPE_IOCTL_CMPLT) {
			pciedev_process_ioctl_done(pciedev);
		}
		/* DMA the indices back to host memory  */
		if (item->flags & PCIEDEV_INTERNAL_SENT_D2H_RINGUPD) {
			ignore_cnt++;
		}
	}
	return ignore_cnt;
}

#ifndef PCIE_PHANTOM_DEV
bool
pciedev_handle_d2h_dma(struct dngl_bus *pciedev)
{
	void * prev;
	uint8 ignore_txd;

	/* free up the rx descriptors */
	while ((prev = dma_getnextrxp(pciedev->d2h_di, FALSE))) {
		ASSERT(pciedev->dtoh_dma_q[pciedev->dtoh_dma_rd_idx].rxpend);
		pciedev->dtoh_dma_q[pciedev->dtoh_dma_rd_idx].rxpend--;

		if (pciedev->dtoh_dma_q[pciedev->dtoh_dma_rd_idx].rxpend == 0) {
			int rdidx;

			/* all rx transactions are done, process/free up the corresponding tx */
			prev = dma_getnexttxp(pciedev->d2h_di, HNDDMA_RANGE_TRANSMITTED);
			ASSERT(prev);

			pciedev->dtoh_dma_q[pciedev->dtoh_dma_rd_idx].txpend--;
			rdidx = pciedev->dtoh_dma_rd_idx;

			ignore_txd = pciedev_handle_d2h_dmacomplete(pciedev, prev);

			/* ideally ignore_txd should be same as txpend, */
			/* we should be able get rid of ignore_txd */
			ASSERT(ignore_txd == pciedev->dtoh_dma_q[rdidx].txpend);

			/* to support phase bit for msgbufs same buffer was split up into 2 */
			while (ignore_txd--) {
				prev = dma_getnexttxp(pciedev->d2h_di, HNDDMA_RANGE_TRANSMITTED);
				pciedev->dtoh_dma_q[rdidx].txpend--;
			}
		}
	}
	pciedev_queue_d2h_req_send(pciedev);
	return FALSE;
}

/* Handler for h2d dma completion
 * host to dongle dma completion interrupt is recieved.
 * Has to decode message or sendup payload
 * 1. get pkt address form dma module
 * 2. Decode message
 * 3. sendup payload
*/
bool
pciedev_handle_h2d_dma(struct dngl_bus *pciedev)
{
	void *txpkt, *rxpkt;
	msgbuf_ring_t *msgbuf;
	uint16 dmalen;
	uint8 msgtype;

	PCI_TRACE(("handle the h2d interrupt\n"));

	/* Release tx descriptors */
	while ((txpkt = dma_getnexttxp(pciedev->h2d_di, HNDDMA_RANGE_TRANSMITTED)));

	while (1) {
		/* Rx offset Pkt */
		rxpkt = dma_getnextrxp(pciedev->h2d_di, FALSE);
		if (rxpkt == NULL)
			break;

		if (rxpkt == pciedev->dummy_rxoff)
			continue;

		/* Retrieve queued up pktlength */
		dmalen = pciedev_htoddma_deque(pciedev, &msgbuf, &msgtype);

		/* internal payload frames */
		if (msgtype  & MSG_TYPE_INTERNAL_USE_START) {
			pciedev_handle_h2d_pyld(pciedev, msgtype, rxpkt, dmalen, msgbuf);
			continue;
		}
		else {
			ASSERT(msgbuf);
			(msgbuf->process_rtn)(pciedev, rxpkt, dmalen, msgbuf);

			/* Update read pointer */
			pciedev_ring_update_readptr(msgbuf, dmalen);

			/* free up local mesage space  if inuse pool is not used */
			if (!INUSEPOOL(msgbuf)) {
				pciedev_free_lclbuf_pool(msgbuf, rxpkt);
			}
			pciedev->msg_pending--;
		}
	}

	/* Flush any pending rx completions */
	pciedev_queue_d2h_req_send(pciedev);

	/* There might be new messages in the circular buffer. Schedule DMA for those too */
	pciedev_msgbuf_intr_process(pciedev);

	if (pciedev->htod_dma_wr_idx == pciedev->htod_dma_rd_idx) {
		if (pciedev->force_ht_on) {
			PCI_TRACE(("%s: no more H2D DMA, so no more force HT\n", __FUNCTION__));
			pciedev->force_ht_on = FALSE;
			pciedev_manage_h2d_dma_clocks(pciedev);
		}
	}

	return FALSE;
}
#endif /* PCIE_PHTM */

void pciedev_upd_flr_port_handle(struct dngl_bus * pciedev, uint8 handle, bool open)
{
	msgbuf_ring_t	*flow_ring;
	struct dll * cur, * prio;

	/* loop through nodes */
	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			flow_ring = ((flowring_pool_t *)cur)->ring;
			if (flow_ring->handle != handle) {
				cur = dll_next_p(cur);
				continue;
			}
			if (!open)
				flow_ring->status |= FLOW_RING_PORT_CLOSED;
			else
				flow_ring->status &= ~FLOW_RING_PORT_CLOSED;
			flow_ring->flow_info.flags &= ~FLOW_RING_FLAG_LAST_TIM;

			/* next node */
			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}
}

void pciedev_upd_flr_if_state(struct dngl_bus * pciedev, uint8 ifindex, bool open)
{
	msgbuf_ring_t	*flow_ring;
	struct dll * cur, * prio;

	/* loop through nodes */
	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			flow_ring = ((flowring_pool_t *)cur)->ring;

			if (flow_ring->flow_info.ifindex != ifindex) {
				cur = dll_next_p(cur);
				continue;
			}
			if (!open)
				flow_ring->status |= FLOW_RING_PORT_CLOSED;
			else
				flow_ring->status &= ~FLOW_RING_PORT_CLOSED;

			/* next node */
			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}
}

void pciedev_upd_flr_tid_state(struct dngl_bus * pciedev, uint8 tid, bool open)
{
	msgbuf_ring_t	*flow_ring;
	struct dll * cur, * prio;

	/* loop through nodes */
	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			flow_ring = ((flowring_pool_t *)cur)->ring;

			if (flow_ring->flow_info.tid != tid) {
				cur = dll_next_p(cur);
				continue;
			}
			if (!open)
				flow_ring->status |= FLOW_RING_PORT_CLOSED;
			else
				flow_ring->status &= ~FLOW_RING_PORT_CLOSED;

			/* next node */
			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}
}

void pciedev_upd_flr_hanlde_map(struct dngl_bus * pciedev, uint8 handle, bool add, uint8 *addr)
{
	msgbuf_ring_t	*flow_ring;
	struct dll * cur, * prio;

	/* loop through nodes */
	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			flow_ring = ((flowring_pool_t *)cur)->ring;
			if (memcmp(flow_ring->flow_info.da, addr,
				ETHER_ADDR_LEN)) {
				cur = dll_next_p(cur);
				continue;
			}
			if (add)
				flow_ring->handle = handle;
			else
				flow_ring->handle = 0xff;

			/* Next node */
			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}
}

void pciedev_process_reqst_packet(struct dngl_bus * pciedev,
	uint8 handle, uint8 ac_bitmap, uint8 count)
{
	uint32 w_ptr;
	int i;
	uint16 prev_maxpktcnt;
	msgbuf_ring_t	*flow_ring;
	msgbuf_ring_t	*fptr[PCIE_MAX_TID_COUNT];
	struct dll * cur, * prio;

	if (pciedev->in_d3_suspend || !count) {
		return;
	}
	bzero(fptr, sizeof(fptr));

	/* loop through active queues */
	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			flow_ring = ((flowring_pool_t *)cur)->ring;
			if (flow_ring->handle != handle) {
				cur = dll_next_p(cur);
				continue;
			}

			if (ac_bitmap & (0x1 << flow_ring->flow_info.tid))
				fptr[flow_ring->flow_info.tid] = flow_ring;

			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}

	i = PCIE_MAX_TID_COUNT - 1;
	while (i >= 0) {
		if (fptr[i] == NULL) {
			i--;
			continue;
		}
		w_ptr = DNGL_RING_WPTR(fptr[i]);

		/* Check write pointer of the flow ring and mark it for pending pkt pull */
		if (!READ_AVAIL_SPACE(w_ptr, fptr[i]->rd_pending, RING_MAX_ITEM(fptr[i]))) {
			fptr[i]->status &= ~FLOW_RING_PKT_PENDING;
			if (fptr[i]->flow_info.flags & FLOW_RING_FLAG_LAST_TIM) {
				dngl_flowring_update(pciedev->dngl, fptr[i]->flow_info.ifindex,
					(uint8) fptr[i]->handle, FLOW_RING_TIM_RESET,
					(uint8 *)&fptr[i]->flow_info.sa,
					(uint8 *)&fptr[i]->flow_info.da,
					(uint8) fptr[i]->flow_info.tid);
				fptr[i]->flow_info.flags &= ~FLOW_RING_FLAG_LAST_TIM;
			}
			i--;
			continue;
		}
		if (count > 0) {
			prev_maxpktcnt = fptr[i]->flow_info.maxpktcnt;
			fptr[i]->flow_info.maxpktcnt = count;
			fptr[i]->flow_info.reqpktcnt += count;
			fptr[i]->flow_info.flags |= FLOW_RING_FLAG_PKT_REQ;
			count -= pciedev_read_flow_ring_host_buffer(pciedev, fptr[i]);
			fptr[i]->flow_info.maxpktcnt = prev_maxpktcnt;
			i--;
		} else
			break;
	}
}

/* Add WLFC_PKTFLAG_PKT_REQUESTED flag onto wl header in TLV format */
static void pciedev_push_pkttag_tlv_info(struct dngl_bus *pciedev, void* p,
       msgbuf_ring_t *flow_ring, uint16 index)
{
	uint8 *buf;
	uint32 pkt_flags = 0;
	uint8 flags = 0;
	uint8 len = ROUNDUP((TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_PKTTAG + WLFC_CTL_VALUE_LEN_SEQ),
		sizeof(uint32));
	if (PKTHEADROOM(pciedev->osh, p) < len) {
		PCI_ERROR(("pciedev_push_pkttag_tlv_info: No room for pkttag TLV\n"));
		return;
	}
	PKTPUSH(pciedev->osh, p, len);
	buf = PKTDATA(pciedev->osh, p);
	PKTSETDATAOFFSET(p, len >> 2);
	buf[TLV_TAG_OFF] = WLFC_CTL_TYPE_PKTTAG;
	buf[TLV_LEN_OFF] = WLFC_CTL_VALUE_LEN_PKTTAG + WLFC_CTL_VALUE_LEN_SEQ;
	/* for packets sent as a result of Ps-poll from peer STA in PS */
	if (flow_ring->flow_info.flags & FLOW_RING_FLAG_PKT_REQ) {
		flags |= WLFC_PKTFLAG_PKT_REQUESTED;
		WL_TXSTATUS_SET_FLAGS(pkt_flags, flags);
		memcpy(&buf[TLV_HDR_LEN], &pkt_flags, sizeof(uint32));
		flow_ring->flow_info.reqpktcnt--;
		if (!flow_ring->flow_info.reqpktcnt)
			flow_ring->flow_info.flags &= ~FLOW_RING_FLAG_PKT_REQ;
	}
}

/* Send out chained pkt completions */
void
pciedev_flush_chained_pkts(void *ctx)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)ctx;

	/* dequeue and send out rx payloads and rx completes */
	if (pciedev->ioctl_pend != TRUE) {
		pciedev_queue_d2h_req_send(pciedev);
	}

	/* Send out tx completes */
	if (pciedev->dtoh_txcpl->buf_pool->pend_item_cnt) {
		PCI_TRACE(("TX: pend count is %d\n", pciedev->dtoh_txcpl->buf_pool->pend_item_cnt));
		pciedev_xmit_txstatus(pciedev, pciedev->dtoh_txcpl);
	}
}
