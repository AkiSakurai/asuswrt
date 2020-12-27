/*
 * Broadcom Dongle Host Driver (DHD), Flow ring specific code at top level
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: dhd_flowrings.c jaganlv $
 */

#include <typedefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmdevs.h>

#include <proto/ethernet.h>
#include <proto/bcmevent.h>
#include <dngl_stats.h>

#include <dhd.h>

#include <dhd_flowring.h>
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>

#ifdef BCM_ROUTER_DHD
extern bool dhd_sta_associated(dhd_pub_t *dhdp, uint32 bssidx, uint8 *mac);
#endif

static INLINE int dhd_flow_queue_throttle(flow_queue_t *queue);

static INLINE uint16 dhd_flowid_find(dhd_pub_t *dhdp, uint8 ifindex,
                                     uint8 prio, char *sa, char *da);

static INLINE uint16 dhd_flowid_alloc(dhd_pub_t *dhdp, uint8 ifindex,
                                      uint8 prio, char *sa, char *da);

static INLINE int dhd_flowid_lookup(dhd_pub_t *dhdp, uint8 ifindex,
                                uint8 prio, char *sa, char *da, uint16 *flowid);
int BCMFASTPATH dhd_flow_queue_overflow(flow_queue_t *queue, void *pkt);


#define FLOW_QUEUE_PKT_NEXT(p)          PKTLINK(p)
#define FLOW_QUEUE_PKT_SETNEXT(p, x)    PKTSETLINK((p), (x))

static INLINE int /* Queue overflow throttle */
dhd_flow_queue_throttle(flow_queue_t *queue)
{
#if defined(BCM_ROUTER_DHD)
	/* Test whether queue's budget and overall cummulative threshold crossed. */
	void *parent_clen_ptr = DHD_FLOW_QUEUE_CLEN_PTR(queue);
	int cumm_threshold = DHD_FLOW_QUEUE_THRESHOLD(queue);
	int ret = ((DHD_FLOW_QUEUE_OVFL(queue, DHD_FLOW_QUEUE_MAX(queue))) &&
	           (DHD_CUMM_CTR_READ(parent_clen_ptr) > cumm_threshold));
	return ret;
#else
	return DHD_FLOW_QUEUE_FULL(queue);
#endif /* ! BCM_ROUTER_DHD */
}

int BCMFASTPATH
dhd_flow_queue_overflow(flow_queue_t *queue, void *pkt)
{
	return BCME_NORESOURCE;
}

flow_ring_node_t * /* Fetch the flow ring given a flowid */
dhd_flow_ring_node(dhd_pub_t *dhdp, uint16 flowid)
{
	flow_ring_node_t * flow_ring_node;

	ASSERT(dhdp != (dhd_pub_t*)NULL);
	ASSERT(flowid < dhdp->num_flow_rings);

	flow_ring_node = &(((flow_ring_node_t*)(dhdp->flow_ring_table))[flowid]);

	ASSERT(flow_ring_node->flowid == flowid);
	return flow_ring_node;
}

flow_queue_t * /* Fetch the queue given a flowid */
dhd_flow_queue(dhd_pub_t *dhdp, uint16 flowid)
{
	flow_ring_node_t * flow_ring_node;

	flow_ring_node = dhd_flow_ring_node(dhdp, flowid);
	return &flow_ring_node->queue;
}

/* Flow ring's queue management functions */

void /* Initialize a flow ring's queue */
dhd_flow_queue_init(dhd_pub_t *dhdp, flow_queue_t *queue, int max)
{
	ASSERT((queue != NULL) && (max > 0));

	dll_init(&queue->list);
	queue->head = queue->tail = NULL;
	queue->len = 0;

	/* Set queue's threshold and queue's parent cummulative length counter */
	ASSERT(max > 1);
	DHD_FLOW_QUEUE_SET_MAX(queue, max);
	DHD_FLOW_QUEUE_SET_THRESHOLD(queue, max);
	DHD_FLOW_QUEUE_SET_CLEN(queue, &dhdp->cumm_ctr);

	queue->failures = 0U;
	queue->cb = &dhd_flow_queue_overflow;

	queue->lock = dhd_os_spin_lock_init(dhdp->osh);
	if (queue->lock == NULL)
		DHD_ERROR(("%s: Failed to init spinlock for queue!\n", __FUNCTION__));
}

void /* Register an enqueue overflow callback handler */
dhd_flow_queue_register(flow_queue_t *queue, flow_queue_cb_t cb)
{
	ASSERT(queue != NULL);
	queue->cb = cb;
}


int BCMFASTPATH /* Enqueue a packet in a flow ring's queue */
dhd_flow_queue_enqueue(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt)
{
	int ret = BCME_OK;

	ASSERT(queue != NULL);

	if (dhd_flow_queue_throttle(queue)) {
		queue->failures++;
		ret = (*queue->cb)(queue, pkt);
		goto done;
	}

	if (queue->head) {
		FLOW_QUEUE_PKT_SETNEXT(queue->tail, pkt);
	} else {
		queue->head = pkt;
	}

	FLOW_QUEUE_PKT_SETNEXT(pkt, NULL);

	queue->tail = pkt; /* at tail */

	queue->len++;
	/* increment parent's cummulative length */
	DHD_CUMM_CTR_INCR(DHD_FLOW_QUEUE_CLEN_PTR(queue));

done:
	return ret;
}

void * BCMFASTPATH /* Dequeue a packet from a flow ring's queue, from head */
dhd_flow_queue_dequeue(dhd_pub_t *dhdp, flow_queue_t *queue)
{
	void * pkt;

	ASSERT(queue != NULL);

	pkt = queue->head; /* from head */

	if (pkt == NULL) {
		ASSERT((queue->len == 0) && (queue->tail == NULL));
		goto done;
	}

	queue->head = FLOW_QUEUE_PKT_NEXT(pkt);
	if (queue->head == NULL)
		queue->tail = NULL;

	queue->len--;
	/* decrement parent's cummulative length */
	DHD_CUMM_CTR_DECR(DHD_FLOW_QUEUE_CLEN_PTR(queue));

	FLOW_QUEUE_PKT_SETNEXT(pkt, NULL); /* dettach packet from queue */

done:
	return pkt;
}

void BCMFASTPATH /* Reinsert a dequeued packet back at the head */
dhd_flow_queue_reinsert(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt)
{
	if (queue->head == NULL) {
		queue->tail = pkt;
	}

	FLOW_QUEUE_PKT_SETNEXT(pkt, queue->head);
	queue->head = pkt;
	queue->len++;
	/* increment parent's cummulative length */
	DHD_CUMM_CTR_INCR(DHD_FLOW_QUEUE_CLEN_PTR(queue));
}

/* Fetch the backup queue for a flowring, and assign thresholds */
void
dhd_flow_ring_config_thresholds(dhd_pub_t *dhdp, uint16 flowid,
                     int queue_budget, int cumm_threshold, void *cumm_ctr)
{
	flow_queue_t * queue;

	ASSERT(dhdp != (dhd_pub_t*)NULL);
	ASSERT(queue_budget > 1);
	ASSERT(cumm_threshold > 1);
	ASSERT(cumm_ctr != (void*)NULL);

	queue = dhd_flow_queue(dhdp, flowid);

	DHD_FLOW_QUEUE_SET_MAX(queue, queue_budget); /* Max queue length */

	/* Set the queue's parent threshold and cummulative counter */
	DHD_FLOW_QUEUE_SET_THRESHOLD(queue, cumm_threshold);
	DHD_FLOW_QUEUE_SET_CLEN(queue, cumm_ctr);
}


/* Init Flow Ring specific data structures */
int
dhd_flow_rings_init(dhd_pub_t *dhdp, uint32 num_flow_rings)
{
	uint32 idx;
	uint32 flow_ring_table_sz;
	uint32 if_flow_lkup_sz;
	void * flowid_allocator;
	flow_ring_table_t *flow_ring_table;
	if_flow_lkup_t *if_flow_lkup;

	DHD_INFO(("%s\n", __FUNCTION__));

	/* Construct a 16bit flow1d allocator */
	flowid_allocator = id16_map_init(dhdp->osh,
	                       num_flow_rings - FLOW_RING_COMMON, FLOWID_RESERVED);
	if (flowid_allocator == NULL) {
		DHD_ERROR(("%s: flowid allocator init failure\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Allocate a flow ring table, comprising of requested number of rings */
	flow_ring_table_sz = (num_flow_rings * sizeof(flow_ring_node_t));
	flow_ring_table = (flow_ring_table_t *)MALLOC(dhdp->osh, flow_ring_table_sz);
	if (flow_ring_table == NULL) {
		DHD_ERROR(("%s: flow ring table alloc failure\n", __FUNCTION__));
		id16_map_fini(dhdp->osh, flowid_allocator);
		return BCME_ERROR;
	}

	/* Initialize flow ring table state */
	DHD_CUMM_CTR_INIT(&dhdp->cumm_ctr);
	bzero((uchar *)flow_ring_table, flow_ring_table_sz);
	for (idx = 0; idx < num_flow_rings; idx++) {
		flow_ring_table[idx].status = FLOW_RING_STATUS_CLOSED;
		flow_ring_table[idx].flowid = (uint16)idx;
		dll_init(&flow_ring_table[idx].list);

		/* Initialize the per flow ring backup queue */
		dhd_flow_queue_init(dhdp, &flow_ring_table[idx].queue,
		                    FLOW_RING_QUEUE_THRESHOLD);
	}

	/* Allocate per interface hash table */
	if_flow_lkup_sz = sizeof(if_flow_lkup_t) * DHD_MAX_IFS;
	if_flow_lkup = (if_flow_lkup_t *)MALLOC(dhdp->osh, if_flow_lkup_sz);
	if (if_flow_lkup == NULL) {
		DHD_ERROR(("%s: if flow lkup alloc failure\n", __FUNCTION__));
		MFREE(dhdp->osh, flow_ring_table, flow_ring_table_sz);
		id16_map_fini(dhdp->osh, flowid_allocator);
		return BCME_ERROR;
	}

	/* Initialize per interface hash table */
	bzero((uchar *)if_flow_lkup, if_flow_lkup_sz);
	for (idx = 0; idx < DHD_MAX_IFS; idx++) {
		int hash_ix;
		if_flow_lkup[idx].status = 0;
		if_flow_lkup[idx].role = 0;
		for (hash_ix = 0; hash_ix < DHD_FLOWRING_HASH_SIZE; hash_ix++)
			if_flow_lkup[idx].fl_hash[hash_ix] = NULL;
	}

	/* Now populate into dhd pub */
	dhdp->num_flow_rings = num_flow_rings;
	dhdp->flowid_allocator = (void *)flowid_allocator;
	dhdp->flow_ring_table = (void *)flow_ring_table;
	dhdp->if_flow_lkup = (void *)if_flow_lkup;

	DHD_INFO(("%s done\n", __FUNCTION__));
	return BCME_OK;
}

/* Deinit Flow Ring specific data structures */
void dhd_flow_rings_deinit(dhd_pub_t *dhdp)
{
	uint16 idx;
	uint32 flow_ring_table_sz;
	uint32 if_flow_lkup_sz;
	flow_ring_table_t *flow_ring_table;
	DHD_INFO(("dhd_flow_rings_deinit\n"));

	if (dhdp->flow_ring_table != NULL) {

		ASSERT(dhdp->num_flow_rings > 0);

		flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;
		for (idx = 0; idx < dhdp->num_flow_rings; idx++) {
			if (flow_ring_table[idx].active) {
				dhd_bus_clean_flow_ring(dhdp->bus, idx);
			}
			ASSERT(DHD_FLOW_QUEUE_EMPTY(&flow_ring_table[idx].queue));

			/* Deinit flow ring queue locks before destroying flow ring table */
			dhd_os_spin_lock_deinit(dhdp->osh, flow_ring_table[idx].queue.lock);
			flow_ring_table[idx].queue.lock = NULL;
		}

		/* Destruct the flow ring table */
		flow_ring_table_sz = dhdp->num_flow_rings * sizeof(flow_ring_table_t);
		MFREE(dhdp->osh, dhdp->flow_ring_table, flow_ring_table_sz);
		dhdp->flow_ring_table = NULL;
	}

	/* Destruct the per interface flow lkup table */
	if (dhdp->if_flow_lkup != NULL) {
		if_flow_lkup_sz = sizeof(if_flow_lkup_t) * DHD_MAX_IFS;
		MFREE(dhdp->osh, dhdp->if_flow_lkup, if_flow_lkup_sz);
		dhdp->if_flow_lkup = NULL;
	}

	/* Destruct the flowid allocator */
	if (dhdp->flowid_allocator != NULL)
		dhdp->flowid_allocator = id16_map_fini(dhdp->osh, dhdp->flowid_allocator);

	dhdp->num_flow_rings = 0U;
}

/* For a given interface, search the hash table for a matching flow */
static INLINE uint16
dhd_flowid_find(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio, char *sa, char *da)
{
	int hash;
	bool ismcast = FALSE;
	flow_hash_info_t *cur;
	if_flow_lkup_t *if_flow_lkup;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

	if (if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_STA) {
		cur = if_flow_lkup[ifindex].fl_hash[prio];
		if (cur) {
			return cur->flowid;
		}

	} else {

		if (ETHER_ISMULTI(da)) {
			ismcast = TRUE;
			hash = 0;
		} else {
			hash = DHD_FLOWRING_HASHINDEX(da, prio);
		}

		cur = if_flow_lkup[ifindex].fl_hash[hash];

		while (cur) {
			if ((ismcast && ETHER_ISMULTI(cur->flow_info.da)) ||
				(!memcmp(cur->flow_info.da, da, ETHER_ADDR_LEN) &&
				(cur->flow_info.tid == prio))) {
				return cur->flowid;
			}
			cur = cur->next;
		}
	}

	return FLOWID_INVALID;
}

/* Allocate Flow ID */
static INLINE uint16
dhd_flowid_alloc(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio, char *sa, char *da)
{
	flow_hash_info_t *fl_hash_node, *cur;
	if_flow_lkup_t *if_flow_lkup;
	int hash;
	uint16 flowid;

	fl_hash_node = (flow_hash_info_t *) MALLOC(dhdp->osh, sizeof(flow_hash_info_t));
	memcpy(fl_hash_node->flow_info.da, da, sizeof(fl_hash_node->flow_info.da));

	ASSERT(dhdp->flowid_allocator != NULL);
	flowid = id16_map_alloc(dhdp->flowid_allocator);

	if (flowid == FLOWID_INVALID) {
		MFREE(dhdp->osh, fl_hash_node,  sizeof(flow_hash_info_t));
		DHD_ERROR(("%s: cannot get free flowid \n", __FUNCTION__));
		return FLOWID_INVALID;
	}

	fl_hash_node->flowid = flowid;
	fl_hash_node->flow_info.tid = prio;
	fl_hash_node->flow_info.ifindex = ifindex;
	fl_hash_node->next = NULL;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
	if (if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_STA) {
		/* For STA we allocate entry based on prio only */
		if_flow_lkup[ifindex].fl_hash[prio] = fl_hash_node;
	} else {

		/* For bcast/mcast assign first slot in in interface */
		hash = ETHER_ISMULTI(da) ? 0 : DHD_FLOWRING_HASHINDEX(da, prio);
		cur = if_flow_lkup[ifindex].fl_hash[hash];
		if (cur) {
			while (cur->next) {
				cur = cur->next;
			}
			cur->next = fl_hash_node;
		} else
			if_flow_lkup[ifindex].fl_hash[hash] = fl_hash_node;
	}

	DHD_INFO(("%s: allocated flowid %d\n", __FUNCTION__, fl_hash_node->flowid));

	return fl_hash_node->flowid;
}

/* Get flow ring ID, if not present try to create one */
static INLINE int
dhd_flowid_lookup(dhd_pub_t *dhdp, uint8 ifindex,
                  uint8 prio, char *sa, char *da, uint16 *flowid)
{
	uint16 id;
	flow_ring_node_t *flow_ring_node;
	flow_ring_table_t *flow_ring_table;

	DHD_INFO(("%s\n", __FUNCTION__));

	flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;

	id = dhd_flowid_find(dhdp, ifindex, prio, sa, da);

	if (id == FLOWID_INVALID) {

		if_flow_lkup_t *if_flow_lkup;
		if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

		if (!if_flow_lkup[ifindex].status)
			return BCME_ERROR;

#ifdef BCM_ROUTER_DHD
		if (!ETHER_ISMULTI(da) &&
		    ((if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_AP) ||
		    (if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_P2P_GO)) &&
		    (!dhd_sta_associated(dhdp, ifindex, da)))
			return BCME_ERROR;
#endif

		id = dhd_flowid_alloc(dhdp, ifindex, prio, sa, da);
		if (id == FLOWID_INVALID) {
			DHD_ERROR(("%s: alloc flowid ifindex %u status %u\n",
			           __FUNCTION__, ifindex, if_flow_lkup[ifindex].status));
			return BCME_ERROR;
		}

		/* register this flowid in dhd_pub */
		dhd_add_flowid(dhdp, ifindex, prio, da, id);
	}

	ASSERT(id < dhdp->num_flow_rings);

	flow_ring_node = (flow_ring_node_t *) &flow_ring_table[id];
	if (flow_ring_node->active) {
		*flowid = id;
		return BCME_OK;
	}

	/* flow_ring_node->flowid = id; */

	/* Init Flow info */
	memcpy(flow_ring_node->flow_info.sa, sa, sizeof(flow_ring_node->flow_info.sa));
	memcpy(flow_ring_node->flow_info.da, da, sizeof(flow_ring_node->flow_info.da));
	flow_ring_node->flow_info.tid = prio;
	flow_ring_node->flow_info.ifindex = ifindex;

	/* Create and inform device about the new flow */
	if (dhd_bus_flow_ring_create_request(dhdp->bus, (void *)flow_ring_node)
	        != BCME_OK) {
		DHD_ERROR(("%s: create error %d\n", __FUNCTION__, id));
		return BCME_ERROR;
	}
	flow_ring_node->active = TRUE;

	*flowid = id;
	return BCME_OK;
}

/* Update flowid information on the packet */
int BCMFASTPATH
dhd_flowid_update(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio, void *pktbuf)
{
	uint8 *pktdata = (uint8 *)PKTDATA(dhdp->osh, pktbuf);
	struct ether_header *eh = (struct ether_header *)pktdata;
	uint16 flowid;

	if (dhd_bus_is_txmode_push(dhdp->bus))
		return BCME_OK;

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS) {
		return BCME_BADARG;
	}

	if (!dhdp->flowid_allocator) {
		DHD_ERROR(("%s: Flow ring not intited yet  \n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (dhd_flowid_lookup(dhdp, ifindex, prio, eh->ether_shost, eh->ether_dhost,
		&flowid) != BCME_OK) {
		return BCME_ERROR;
	}

	DHD_INFO(("%s: prio %d flowid %d\n", __FUNCTION__, prio, flowid));

	/* Tag the packet with flowid */
	DHD_PKT_SET_FLOWID(pktbuf, flowid);
	return BCME_OK;
}

void
dhd_flowid_free(dhd_pub_t *dhdp, uint8 ifindex, uint16 flowid)
{
	int hashix;
	bool found = FALSE;
	flow_hash_info_t *cur, *prev;
	if_flow_lkup_t *if_flow_lkup;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

	for (hashix = 0; hashix < DHD_FLOWRING_HASH_SIZE; hashix++) {

		cur = if_flow_lkup[ifindex].fl_hash[hashix];

		if (cur) {
			if (cur->flowid == flowid) {
				found = TRUE;
			}

			prev = NULL;
			while (!found && cur) {
				if (cur->flowid == flowid) {
					found = TRUE;
					break;
				}
				prev = cur;
				cur = cur->next;
			}
			if (found) {
				if (!prev) {
					if_flow_lkup[ifindex].fl_hash[hashix] = cur->next;
				} else {
					prev->next = cur->next;
				}

				/* deregister flowid from dhd_pub. */
				dhd_del_flowid(dhdp, ifindex, flowid);

				id16_map_free(dhdp->flowid_allocator, flowid);
				MFREE(dhdp->osh, cur, sizeof(flow_hash_info_t));

				return;
			}
		}
	}

	DHD_ERROR(("%s: could not free flow ring hash entry flowid %d\n",
	           __FUNCTION__, flowid));
}


/* Delete all Flow rings assocaited with the given Interface */
void
dhd_flow_rings_delete(dhd_pub_t *dhdp, uint8 ifindex)
{
	uint32 id;
	flow_ring_table_t *flow_ring_table;

	DHD_INFO(("%s: ifindex %u\n", __FUNCTION__, ifindex));

	ASSERT(ifindex < DHD_MAX_IFS);
	if (!dhdp->flow_ring_table)
		return;

	flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;
	for (id = 0; id < dhdp->num_flow_rings; id++) {
		if (flow_ring_table[id].active &&
		    (flow_ring_table[id].flow_info.ifindex == ifindex)) {
			dhd_bus_flow_ring_delete_request(dhdp->bus,
			                                 (void *) &flow_ring_table[id]);
		}
	}
}

/* Delete flow/s for given peer address */
void
dhd_flow_rings_delete_for_peer(dhd_pub_t *dhdp, uint8 ifindex, char *addr)
{
	uint32 id;
	flow_ring_table_t *flow_ring_table;
	if_flow_lkup_t *if_flow_lkup;

	DHD_ERROR(("%s: ifindex %u\n", __FUNCTION__, ifindex));

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS)
		return;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

	if (!dhdp->flow_ring_table)
		return;

	flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;
	for (id = 0; id < dhdp->num_flow_rings; id++) {
		if (flow_ring_table[id].active &&
		    (flow_ring_table[id].flow_info.ifindex == ifindex) &&
		    ((if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_STA) ||
		     !memcmp(flow_ring_table[id].flow_info.da, addr, ETHER_ADDR_LEN)) &&
		    (flow_ring_table[id].status != FLOW_RING_STATUS_DELETE_PENDING)) {
			DHD_INFO(("%s: deleting flowid %d\n",
			          __FUNCTION__, flow_ring_table[id].flowid));
			dhd_bus_flow_ring_delete_request(dhdp->bus,
			                                 (void *) &flow_ring_table[id]);
		}
	}
}

/* Handle Interface ADD, DEL operations */
void
dhd_update_interface_flow_info(dhd_pub_t *dhdp, uint8 ifindex,
                               uint8 op, uint8 role)
{
	if_flow_lkup_t *if_flow_lkup;

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS)
		return;

	DHD_INFO(("%s: ifindex %u op %u role is %u \n",
	          __FUNCTION__, ifindex, op, role));
	if (!dhdp->flowid_allocator) {
		DHD_ERROR(("%s: Flow ring not intited yet  \n", __FUNCTION__));
		return;
	}

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

	if (op == WLC_E_IF_ADD || op == WLC_E_IF_CHANGE) {

		if_flow_lkup[ifindex].role = role;

		if (role != WLC_E_IF_ROLE_STA) {
			if_flow_lkup[ifindex].status = TRUE;
			DHD_INFO(("%s: Mcast Flow ring for ifindex %d role is %d \n",
			          __FUNCTION__, ifindex, role));
			/* Create Mcast Flow */
		}
	} else	if (op == WLC_E_IF_DEL) {
		if_flow_lkup[ifindex].status = FALSE;
		DHD_INFO(("%s: cleanup all Flow rings for ifindex %d role is %d \n",
		          __FUNCTION__, ifindex, role));
	}
}

/* Handle a STA interface link status update */
int
dhd_update_interface_link_status(dhd_pub_t *dhdp, uint8 ifindex, uint8 status)
{
	if_flow_lkup_t *if_flow_lkup;

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS)
		return BCME_BADARG;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
	DHD_INFO(("%s: ifindex %d status %d\n", __FUNCTION__, ifindex, status));

	if (if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_STA) {
		if (status)
			if_flow_lkup[ifindex].status = TRUE;
		else
			if_flow_lkup[ifindex].status = FALSE;
	}
	return BCME_OK;
}
