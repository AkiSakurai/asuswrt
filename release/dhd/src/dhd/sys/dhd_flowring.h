/*
 * Header file describing the flow rings DHD interfaces.
 *
 * Provides type definitions and function prototypes used to create, delete and manage
 *
 * flow rings at high level
 *
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
 * $Id: dhd_flowrings.h  jaganlv $
 */

/****************
 * Common types *
 */

#ifndef _dhd_flowrings_h_
#define _dhd_flowrings_h_

/* Max pkts held in a flow ring's backup queue */
#define FLOW_RING_QUEUE_THRESHOLD       (2048)

/* Number of H2D common rings : PCIE Spec Rev? */
#define FLOW_RING_COMMON                2

#define FLOWID_INVALID                  (ID16_INVALID)
#define FLOWID_RESERVED                 (FLOW_RING_COMMON)

#define FLOW_RING_STATUS_OPEN           0
#define FLOW_RING_STATUS_PENDING        1
#define FLOW_RING_STATUS_CLOSED         2
#define FLOW_RING_STATUS_DELETE_PENDING 3
#define FLOW_RING_STATUS_FLUSH_PENDING  4

#define DHD_FLOWRING_RX_BUFPOST_PKTSZ	2048

/* Hashing a MacAddress for lkup into a per interface flow hash table */
#define DHD_FLOWRING_HASH_SIZE    256
#define	DHD_FLOWRING_HASHINDEX(ea, prio) \
	       ((((uint8 *)(ea))[3] ^ ((uint8 *)(ea))[4] ^ ((uint8 *)(ea))[5] ^ ((uint8)(prio))) \
		% DHD_FLOWRING_HASH_SIZE)

#define DHD_IF_ROLE(pub, idx)		(((if_flow_lkup_t *)(pub)->if_flow_lkup)[idx].role)
#define DHD_IF_ROLE_AP(pub, idx)	(DHD_IF_ROLE(pub, idx) == WLC_E_IF_ROLE_AP)
#define DHD_IF_ROLE_STA(pub, idx)	(DHD_IF_ROLE(pub, idx) == WLC_E_IF_ROLE_STA)
#define DHD_IF_ROLE_P2PGO(pub, idx)	(DHD_IF_ROLE(pub, idx) == WLC_E_IF_ROLE_P2P_GO)

struct flow_queue;

/* Flow Ring Queue Enqueue overflow callback */
typedef int (*flow_queue_cb_t)(struct flow_queue * queue, void * pkt);

typedef struct flow_queue {
	dll_t  list;                /* manage a flowring queue in a dll */
	void * head;                /* first packet in the queue */
	void * tail;                /* last packet in the queue */
	uint16 len;                 /* number of packets in the queue */
	uint16 max;                 /* maximum or min budget (used in cumm) */
	uint32 threshold;           /* parent's cummulative length threshold */
	void * clen_ptr;            /* parent's cummulative length counter */
	uint32 failures;            /* enqueue failures due to queue overflow */
	flow_queue_cb_t cb;         /* callback invoked on threshold crossing */
	void * lock;		/* OS specific lock handle for Q access protection */
} flow_queue_t;

#define DHD_FLOW_QUEUE_LEN(queue)       ((int)(queue)->len)
#define DHD_FLOW_QUEUE_MAX(queue)       ((int)(queue)->max)
#define DHD_FLOW_QUEUE_THRESHOLD(queue) ((int)(queue)->threshold)
#define DHD_FLOW_QUEUE_EMPTY(queue)     ((queue)->len == 0)
#define DHD_FLOW_QUEUE_FAILURES(queue)  ((queue)->failures)

#define DHD_FLOW_QUEUE_AVAIL(queue)     ((int)((queue)->max - (queue)->len))
#define DHD_FLOW_QUEUE_FULL(queue)      ((queue)->len >= (queue)->max)

#define DHD_FLOW_QUEUE_OVFL(queue, budget)  \
	(((queue)->len) > budget)

#define DHD_FLOW_QUEUE_SET_MAX(queue, budget) \
	((queue)->max) = ((budget) - 1)

/* Queue's cummulative threshold. */
#define DHD_FLOW_QUEUE_SET_THRESHOLD(queue, cumm_threshold) \
	((queue)->threshold) = ((cumm_threshold) - 1)

/* Queue's cummulative length object accessor. */
#define DHD_FLOW_QUEUE_CLEN_PTR(queue)  ((queue)->clen_ptr)

/* Set a queue's cumm_len point to a parent's cumm_ctr_t cummulative length */
#define DHD_FLOW_QUEUE_SET_CLEN(queue, parent_clen_ptr)  \
	((queue)->clen_ptr) = (void *)(parent_clen_ptr)


typedef struct flow_info {
	uint8		tid;
	uint8		ifindex;
	char		sa[ETHER_ADDR_LEN];
	char		da[ETHER_ADDR_LEN];
} flow_info_t;

typedef struct flow_ring_node {
	dll_t		list; /* manage a constructed flowring in a dll, must be at first place */
	flow_queue_t	queue;
	bool		active;
	uint8		status;
	uint16		flowid;
	flow_info_t	flow_info;
	void		*prot_info;
} flow_ring_node_t;
typedef flow_ring_node_t flow_ring_table_t;

typedef struct flow_hash_info {
	uint16			flowid;
	flow_info_t		flow_info;
	struct flow_hash_info	*next;
} flow_hash_info_t;

typedef struct if_flow_lkup {
	bool		status;
	uint8		role; /* Interface role: STA/AP */
	flow_hash_info_t *fl_hash[DHD_FLOWRING_HASH_SIZE]; /* Lkup Hash table */
} if_flow_lkup_t;

static INLINE flow_ring_node_t *
dhd_constlist_to_flowring(dll_t *item)
{
	return ((flow_ring_node_t *)item);
}

/* Exported API */

/* Flow ring's queue management functions */
extern flow_ring_node_t * dhd_flow_ring_node(dhd_pub_t *dhdp, uint16 flowid);
extern flow_queue_t * dhd_flow_queue(dhd_pub_t *dhdp, uint16 flowid);

extern void dhd_flow_queue_init(dhd_pub_t *dhdp, flow_queue_t *queue, int max);
extern void dhd_flow_queue_register(flow_queue_t *queue, flow_queue_cb_t cb);
extern int  dhd_flow_queue_enqueue(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt);
extern void * dhd_flow_queue_dequeue(dhd_pub_t *dhdp, flow_queue_t *queue);
extern void dhd_flow_queue_reinsert(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt);

extern void dhd_flow_ring_config_thresholds(dhd_pub_t *dhdp, uint16 flowid,
                          int queue_budget, int cumm_threshold, void *cumm_ctr);
extern int  dhd_flow_rings_init(dhd_pub_t *dhdp, uint32 num_flow_rings);

extern void dhd_flow_rings_deinit(dhd_pub_t *dhdp);

extern int dhd_flowid_update(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio,
                void *pktbuf);

extern void dhd_flowid_free(dhd_pub_t *dhdp, uint8 ifindex, uint16 flowid);

extern void dhd_flow_rings_delete(dhd_pub_t *dhdp, uint8 ifindex);

extern void dhd_flow_rings_delete_for_peer(dhd_pub_t *dhdp, uint8 ifindex,
                char *addr);

/* Handle Interface ADD, DEL operations */
extern void dhd_update_interface_flow_info(dhd_pub_t *dhdp, uint8 ifindex,
                uint8 op, uint8 role);

/* Handle a STA interface link status update */
extern int dhd_update_interface_link_status(dhd_pub_t *dhdp, uint8 ifindex,
                uint8 status);


#endif /* _dhd_flowrings_h_ */
