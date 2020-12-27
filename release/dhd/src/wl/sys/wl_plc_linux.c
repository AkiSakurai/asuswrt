/**
 * @file
 * @brief
 * PLC (Power Line Communication) failover support to dynamically select the link. WLAN or PLC which
 * ever provides optimum performance will be used.
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
 * $Id: wl_plc_linux.c 467328 2014-04-03 01:23:40Z $
 */

/**
 * @file
 * @brief
 * In order to ensure always there is a path in between WIFI+PLC devices, we created a failover
 * feature for this. Failover is running on WIFI+PLC URE and WIFI+PLC AP.
 */


#include <typedefs.h>
#include <linuxver.h>
#include <wlc_cfg.h>
#include <osl.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>

#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>

#ifdef HNDCTF
#include <ctf/hndctf.h>
#endif
#ifdef WLC_HIGH_ONLY
#include "bcm_rpc_tp.h"
#include "bcm_rpc.h"
#include "bcm_xdr.h"
#include "wlc_rpc.h"
#endif

#include <wl_linux.h>
#include <wl_export.h>
#include <wl_plc_linux.h>

/* This becomes netdev->priv and is the link between netdev and wlif struct */
typedef struct priv_link {
	wl_if_t *wlif;
} priv_link_t;

#define WL_DEV_IF(dev)          ((wl_if_t*)((priv_link_t*)DEV_PRIV(dev))->wlif)

#define WL_INFO_GET(dev) ((wl_info_t*)(WL_DEV_IF(dev)->wl))

#define NODE_EA_HASH(ea) \
	((ea[5] + ea[4] + ea[3] + ea[2] + ea[1]) & (NODE_TBL_SZ - 1))
#define EA_CMP(ea1, ea2) \
	(!((((uint16 *)(ea1))[0] ^ ((uint16 *)(ea2))[0]) | \
	   (((uint16 *)(ea1))[1] ^ ((uint16 *)(ea2))[1]) | \
	   (((uint16 *)(ea1))[2] ^ ((uint16 *)(ea2))[2])))

#ifdef WL_PLC_NO_VLAN_TAG_TX
#define VLAN_TAG_NUM	1
#else
#define VLAN_TAG_NUM	3
#endif

#ifdef PLC_WET
#define TIMER_INTERVAL_PLC_WATCHDOG	3000	/* in unit of ms */

static uint32 plc_node_aging_time;

/*
 * Find the wifi/plc node entry using mac address as key
 */
wl_plc_node_t *
wl_plc_node_find(wl_info_t *wl, uint8 *da)
{
	uint32 hash;
	wl_plc_node_t *node;
	wl_if_t *wlif = wl->if_list;

	if (ETHER_ISMULTI(da))
		return NULL;

	hash = NODE_EA_HASH(da);

	/* Node table lookup */
	WL_LOCK(wl);
	node = wlif->plc->node_tbl[hash];
	while (node != NULL) {
		if (EA_CMP(node->ea.octet, da)) {
			WL_UNLOCK(wl);
			return node;
		}

		node = node->next;
	}
	WL_UNLOCK(wl);

	return NULL;
}

/*
 * Find the wifi/plc node entry using link address as key
 */
static wl_plc_node_t *
wl_plc_node_find_by_linkaddr(wl_info_t *wl, uint8 *ea)
{
	uint32 hash;
	wl_plc_node_t *node;
	wl_if_t *wlif = wl->if_list;

	if (ETHER_ISMULTI(ea))
		return NULL;

	/* Node table lookup */
	WL_LOCK(wl);
	for (hash = 0; hash < NODE_TBL_SZ; hash++) {
		node = wlif->plc->node_tbl[hash];
		while (node != NULL) {
			if (EA_CMP(node->link_ea.octet, ea)) {
				WL_UNLOCK(wl);
				return node;
			}

			node = node->next;
		}
	}
	WL_UNLOCK(wl);

	return NULL;
}

/*
 * Set the cost for all wifi/plc node entries matching link address
 */
static int32
wl_plc_node_set_cost_by_linkaddr(wl_info_t *wl, uint8 *ea, uint32 cost)
{
	uint32 hash;
	wl_plc_node_t *node;
	wl_if_t *wlif = wl->if_list;

	if (ETHER_ISMULTI(ea))
		return BCME_ERROR;

	/* Node table lookup */
	WL_LOCK(wl);
	for (hash = 0; hash < NODE_TBL_SZ; hash++) {
		node = wlif->plc->node_tbl[hash];
		while (node != NULL) {
			if (EA_CMP(node->link_ea.octet, ea))
				node->path_cost = cost;
			node = node->next;
		}
	}
	WL_UNLOCK(wl);

	return BCME_OK;
}

/* Get the cost for node with specified mac address */
int32
wl_plc_node_get_cost(wl_info_t *wl, uint8 *ea, uint32 type)
{
	wl_plc_node_t *node;

	if (!PLC_ENAB(wl->pub))
		return BCME_ERROR;

	node = (type == PLC_CMD_MAC_COST) ?  wl_plc_node_find(wl, ea) :
	                                     wl_plc_node_find_by_linkaddr(wl, ea);
	if (node == NULL)
		return BCME_ERROR;

	return node->path_cost;
}


/* Set the cost for node with specified mac address */
int32
wl_plc_node_set_cost(wl_info_t *wl, uint8 *ea, uint32 type, uint32 cost)
{
	wl_plc_node_t *node;

	if (!PLC_ENAB(wl->pub))
		return BCME_OK;

	if (type == PLC_CMD_MAC_COST) {
		node = wl_plc_node_find(wl, ea);
		if (node == NULL)
			return BCME_ERROR;

		node->path_cost = cost;
	} else
		return wl_plc_node_set_cost_by_linkaddr(wl, ea, cost);

	return BCME_OK;
}

/*
 * Add a new entry to wifi plc node table. Called from the bridge layer when
 * new fdb entry is added or when it needs to be updated/inserted.
 */
int32
wl_plc_node_add(wl_info_t *wl, uint32 node_type, uint8 *ea, uint8 *link_ea,
                wl_plc_node_t **node)
{
	uint32 hash;
	wl_if_t *wlif = wl->if_list;
	wl_plc_node_t *n;

	/* Validate the input params */
	if ((wlif == NULL) || (ea == NULL) || (node_type == NODE_TYPE_UNKNOWN)) {
		WL_ERROR(("%s: Invalid input param", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Ignore duplicate add requests */
	if ((n = wl_plc_node_find(wl, ea)) != NULL) {
		/* If node already exists and now it is learned on different
		 * link then update node type to reflect that it is reachable
		 * via both. For instance if node is first learned over plc
		 * then node type will be initially set to plc only, if same
		 * node is learned on wifi also then update type to wifi plc.
		 */
		n->node_type |= node_type;
		n->used = wl->pub->now;
		if (node != NULL)
			*node = n;
		return BCME_OK;
	}

	/* Allocate new entry */
	if ((n = MALLOC(wl->osh, sizeof(wl_plc_node_t))) == NULL) {
		WL_ERROR(("%s: Out of memory allocating wifi plc node\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Initialize the entry and add it to the hash table */
	bzero(n, sizeof(wl_plc_node_t));
	bcopy(ea, &n->ea, ETHER_ADDR_LEN);
	if (link_ea != NULL)
		bcopy(link_ea, &n->link_ea, ETHER_ADDR_LEN);
	n->path_cost = PATH_COST_WIFI;
	n->node_type = node_type;
	n->used = wl->pub->now;
	n->aging_time = plc_node_aging_time;

	hash = NODE_EA_HASH(n->ea.octet);

	WL_LOCK(wl);

	n->next = wlif->plc->node_tbl[hash];
	wlif->plc->node_tbl[hash] = n;

	if (node != NULL)
		*node = n;

	WL_UNLOCK(wl);

	return BCME_OK;
}

/*
 * Delete all the nodes.
 */
void
wl_plc_node_delete_all(wl_info_t *wl)
{
	uint32 hash;
	wl_plc_node_t *node, *tmp;
	wl_if_t *wlif = wl->if_list;

	if ((wlif == NULL) || (wlif->plc == NULL))
		return;

	/* Node table lookup */
	WL_LOCK(wl);
	for (hash = 0; hash < NODE_TBL_SZ; hash++) {
		node = wlif->plc->node_tbl[hash];
		while (node != NULL) {
			/* Remove the entry from hash list */
			wlif->plc->node_tbl[hash] = node->next;
			tmp = node->next;
			MFREE(wl->osh, node, sizeof(wl_plc_node_t));
			node = tmp;
		}
	}
	WL_UNLOCK(wl);
	return;
}


/*
 * Delete an entry from wifi plc node table. Called from the bridge layer when
 * the fdb entry expires or when it needs to be updated/deleted.
 */
int32
wl_plc_node_delete(wl_info_t *wl, uint8 *ea, bool force)
{
	uint32 hash;
	wl_plc_node_t *node, *prev;
	wl_if_t *wlif;

	/* Validate the input params */
	if ((wl == NULL) || (ea == NULL)) {
		WL_ERROR(("%s: Invalid input (wl %p ea %p)", __FUNCTION__, wl, ea));
		return BCME_ERROR;
	}

	hash = NODE_EA_HASH(ea);

	WL_LOCK(wl);

	for (wlif = wl->if_list; wlif != NULL; wlif = wlif->next) {
		node = wlif->plc->node_tbl[hash];

		/* Find the entry and delete */
		prev = NULL;
		while (node != NULL) {
			if (!EA_CMP(node->ea.octet, ea)) {
				prev = node;
				node = node->next;
				continue;
			}

			/* Dont remove the entry if it was being used on PLC link */
			if (!force && (wl->pub->now - node->used < 180)) {
				WL_UNLOCK(wl);
				return BCME_ASSOCIATED;
			}

			/* Remove the entry from hash list */
			if (prev != NULL)
				prev->next = node->next;
			else
				wlif->plc->node_tbl[hash] = node->next;

			WL_UNLOCK(wl);

			MFREE(wl->osh, node, sizeof(wl_plc_node_t));

			return BCME_OK;
		}
	}

	WL_UNLOCK(wl);

	return BCME_ERROR;
}

/* Populate the node list in the input buffer */
void
wl_plc_node_list(wl_info_t *wl, wl_plc_nodelist_t *list)
{
	uint32 hash, i = 0;
	wl_if_t *wlif = wl->if_list;
	wl_plc_node_t *node;

	/* Validate the input params */
	if ((wl == NULL) || (list == NULL)) {
		WL_ERROR(("%s: Invalid input (wl %p list %p)", __FUNCTION__, wl, list));
		return;
	}

	if (!PLC_ENAB(wl->pub))
		return;

	WL_LOCK(wl);
	for (hash = 0; hash < NODE_TBL_SZ; hash++) {
		node = wlif->plc->node_tbl[hash];
		while (node != NULL) {
			list->node[i].ea = node->ea;
			list->node[i].node_type = node->node_type;
			list->node[i].cost = node->path_cost;
			node = node->next;
			i++;
		}
	}
	WL_UNLOCK(wl);

	list->count = i;

	return;
}

int32
wl_plc_node_mac(wl_info_t *wl, wl_plc_node_t *node, uint8 *ea)
{
	ASSERT((node != NULL) && (ea != NULL));
	bcopy(node->ea.octet, ea, ETHER_ADDR_LEN);
	return BCME_OK;
}

bool
wl_plc_node_pref_plc(wl_info_t *wl, wl_plc_node_t *node)
{
	ASSERT(node != NULL);
	return (node->node_type != NODE_TYPE_WIFI_ONLY) &&
	       (((node->node_type == NODE_TYPE_PLC_ONLY) || wl->pub->plc_path ||
	        (node->path_cost == PATH_COST_PLC)) ? TRUE : FALSE);
}

void
wl_plc_node_set_bsscfg(wl_info_t *wl, wl_plc_node_t *node, wlc_bsscfg_t *bsscfg)
{
	ASSERT(node != NULL);
	node->cfg = bsscfg;
}

void
wl_plc_node_set_scb(wl_info_t *wl, wl_plc_node_t *node, struct scb *scb)
{
	ASSERT(node != NULL);
	node->scb = scb;
}

struct scb *
wl_plc_node_scb(wl_info_t *wl, wl_plc_node_t *node)
{
	ASSERT(node != NULL);
	return node->scb;
}

static void
wl_plc_wdtimer(void *arg)
{
	wl_info_t *wl = (wl_info_t *)arg;
	wl_if_t *wlif;
	uint32 hash;
	wl_plc_node_t *node, *prev, *tmp;

	ASSERT(wl);

	wlif = wl->if_list;
	ASSERT(wlif);

	if (!PLC_ENAB(wl->pub))
		return;

	/* Node table lookup */
	WL_LOCK(wl);

	for (hash = 0; hash < NODE_TBL_SZ; hash++) {
		node = wlif->plc->node_tbl[hash];
		prev = NULL;

		while (node != NULL) {
			if ((node->node_type == NODE_TYPE_PLC_ONLY) &&
			    ((node->used + node->aging_time) < wl->pub->now)) {
				/* Remove the entry from hash list */
				if (prev != NULL)
					prev->next = node->next;
				else
					wlif->plc->node_tbl[hash] = node->next;

				WL_PLC("%s: aged out: %02x-%02x-%02x-%02x-%02x-%02x\n",
					__FUNCTION__,
					node->ea.octet[0], node->ea.octet[1],
					node->ea.octet[2], node->ea.octet[3],
					node->ea.octet[4], node->ea.octet[5]);

				tmp = node->next;
				MFREE(wl->osh, node, sizeof(wl_plc_node_t));
				node = tmp;
			} else {
				/* Move to the next one */
				prev = node;
				node = node->next;
			}
		}
	}

	WL_UNLOCK(wl);
}
#endif /* PLC_WET */

void
wl_plc_sendpkt(wl_if_t *wlif, struct sk_buff *skb, struct net_device *dev)
{
#ifdef CTFPOOL
	uint users;
#endif /* CTFPOOL */

	/* Make sure the device is up */
	if ((dev->flags & IFF_UP) == 0) {
		PKTFREE(wlif->wl->osh, skb, TRUE);
		return;
	}

	ASSERT(wlif->plc != NULL);

	WL_PLC("%s: skb %p dev %p\n", __FUNCTION__, skb, dev);

	skb->dev = dev;

#if defined(BCMDBG) && defined(PLCDBG)
	if (dev != wlif->plc->plc_rxdev3)
		prhex("->PLC", skb->data, 64);
#endif /* BCMDBG && PLCDBG */

	/* Frames are first queued to tx_vid (vlan3) to be sent out to
	 * the plc port.
	 */
#ifdef CTFPOOL
	users = atomic_read(&skb->users);
	atomic_inc(&skb->users);
	dev_queue_xmit(skb);
	if (atomic_read(&skb->users) == users)
		PKTFREE(wlif->wl->osh, skb, TRUE);
	else
		atomic_dec(&skb->users);
#else /* CTFPOOL */
	dev_queue_xmit(skb);
#endif /* CTFPOOL */

	return;
}

struct sk_buff *
wl_plc_tx_prep(wl_if_t *wlif, struct sk_buff *skb)
{
	struct ethervlan_header *evh;
	uint16 headroom, prio = PKTPRIO(skb) & VLAN_PRI_MASK;

	headroom = PKTHEADROOM(wlif->wl->osh, skb);

	WL_PLC("%s: shared %d headroom %d\n", __FUNCTION__, PKTSHARED(skb), headroom);

	if (ETHER_ISMULTI(skb->data) || PKTSHARED(skb) ||
	   (headroom < (VLAN_TAG_NUM * VLAN_TAG_LEN))) {
		struct sk_buff *tmp = skb;

		PKTCTFMAP(wlif->wl->osh, tmp);

		skb = skb_copy_expand(tmp, headroom + (VLAN_TAG_NUM * VLAN_TAG_LEN),
		                      skb_tailroom(tmp), GFP_ATOMIC);
#ifdef CTFPOOL
		PKTCLRFAST(wlif->wl->osh, skb);
		CTFPOOLPTR(wlif->wl->osh, skb) = NULL;
#endif /* CTFPOOL */

		if (skb == NULL) {
			WL_ERROR(("wl%d: %s: Out of memory while copying bcast frame\n",
			          wlif->wl->pub->unit, __FUNCTION__));
			return NULL;
		}
	}

	evh = (struct ethervlan_header *)skb_push(skb, VLAN_TAG_NUM * VLAN_TAG_LEN);
	memmove(skb->data, skb->data + (VLAN_TAG_NUM * VLAN_TAG_LEN), 2 * ETHER_ADDR_LEN);

#ifdef WL_PLC_NO_VLAN_TAG_TX
	evh->vlan_type = HTON16(VLAN_TPID);
	evh->vlan_tag = HTON16((prio << VLAN_PRI_SHIFT) | wlif->plc->tx_vid);
#else
	/* Initialize dummy outer vlan tag */
	evh->vlan_type = HTON16(VLAN_TPID);
	evh->vlan_tag = HTON16(wlif->plc->tx_vid);

	/* Initialize outer dummy vlan tag */
	evh = (struct ethervlan_header *)(skb->data + VLAN_TAG_LEN);
	evh->vlan_type = HTON16(VLAN_TPID);
	evh->vlan_tag = HTON16((prio << VLAN_PRI_SHIFT) | wlif->plc->tx_vid);

	/* Initialize dummy inner tag before sending to PLC */
	evh = (struct ethervlan_header *)(skb->data + 2 * VLAN_TAG_LEN);
	evh->vlan_type = HTON16(VLAN_TPID);
	evh->vlan_tag = HTON16((prio << VLAN_PRI_SHIFT) | WL_PLC_DUMMY_VID);
#endif /* WL_PLC_NO_VLAN_TAG_TX */
	return skb;
}

/*
 * Forward between the PLC and WIFI under the AP only.
 */
int32
wl_plc_forward(void *p, struct net_device *dev, wl_plc_t *plc, uint16 if_in)
{
	wl_info_t *wl = NULL;
	wl_if_t *wlif = NULL;
	struct sk_buff *skb = NULL, *nskb = NULL;
	wl_plc_node_t *node;
	struct ether_header *eh;
	bool also_sendup = TRUE;

	/* Get the wl info of plc */
	wl = WL_DEV_IF(plc->wl_dev)->wl;
	ASSERT(wl != NULL);

	wlif = wl->if_list;
	ASSERT(wlif != NULL);

	/* Check PLC enabled */
	if (!PLC_ENAB(wl->pub))
		goto exit;

	/* Convert the packet */
	skb = PKTTONATIVE(wl->osh, p);

	/* Find destion node */
	eh = (struct ether_header *)skb->data;
	node = wl_plc_node_find(wl, eh->ether_dhost);

	if (node == NULL) {
		/* Multicast or unicast to ethernet LAN or local */
		if (ETHER_ISUCAST(skb->data))
			goto exit;
	} else {
		/* Unicast to WIFI or PLC */
		if (if_in == WL_PLC_IF_PLC) {
			if ((node->node_type == NODE_TYPE_WIFI_ONLY) ||
			    (node->node_type == NODE_TYPE_WIFI_PLC &&
			     node->path_cost == PATH_COST_WIFI)) {
				/* Don't need to sendup */
				also_sendup = FALSE;
			} else
				goto exit;
		} else {
			if ((node->node_type == NODE_TYPE_PLC_ONLY) ||
			    (node->node_type == NODE_TYPE_WIFI_PLC &&
			     node->path_cost == PATH_COST_PLC)) {
				/* Don't need to sendup */
				also_sendup = FALSE;
			} else
				goto exit;
		}
	}

	if (if_in == WL_PLC_IF_PLC) {
		/* PLC forward to WIFI */
#if defined(BCMDBG) && defined(PLCDBG)
		prhex("PLC forward to WIFI->", skb->data, 32);
#endif
		/* Check multicast */
		if (ETHER_ISMULTI(skb->data) || PKTSHARED(skb)) {
			nskb = skb_copy(skb, GFP_ATOMIC);
			if (nskb == NULL) {
				WL_PLC("%s: out of memory for skb copy\n", __FUNCTION__);
				goto exit;
			}
#ifdef CTFPOOL
			PKTCLRFAST(NULL, nskb);
			CTFPOOLPTR(NULL, nskb) = NULL;
#endif /* CTFPOOL */
		} else
			nskb = skb;

		/* TX netdev */
		nskb->dev = plc->wl_dev;

		/* Send to WIFI */
		WL_LOCK(wl);
		wlc_sendpkt(WL_INFO_GET(plc->wl_dev)->wlc, (void *)nskb,
			WL_DEV_IF(plc->wl_dev)->wlcif);
		WL_UNLOCK(wl);
	} else {
		/* WIFI forward to PLC */
#if defined(BCMDBG) && defined(PLCDBG)
		prhex("WIFI forward to PLC->", skb->data, 32);
#endif

		/* Add the VLAN headers before sending to PLC, and also check multicast */
		nskb = wl_plc_tx_prep(wlif, skb);
		if (nskb == NULL) {
			WL_PLC("%s: out of memory for wl_plc_tx_prep\n", __FUNCTION__);
			goto exit;
		}

		/* TX netdev */
		nskb->dev = plc->plc_dev;

		/* Send to PLC */
		nskb = PKTTONATIVE(wl->osh, nskb);
		wl_plc_sendpkt(wlif, nskb, wlif->plc->plc_dev);

		if (ETHER_ISUCAST(skb->data)) {
			/* Free the original packet */
			if (nskb != skb) {
				WL_PLC("%s: Free the pkt\n", __FUNCTION__);
				PKTFREE(wl->osh, skb, TRUE);
			}
		}
	}

exit:
	if (also_sendup)
		return BCME_ERROR;
	else
		return BCME_OK;
}

/* Called when the frame is recieved on any of the PLC rxvifs (vlan4,
 * vlan5 or vlan6) or WDS interface. Based on the forwarding information
 * received in the vlan tag we send the frame up the bridge or send it
 * over the WDS link.
 */
int32
wl_plc_recv(struct sk_buff *skb, struct net_device *dev, wl_plc_t *plc, uint16 if_in)
{
	int32 vid_in = -1, vid_in_in = -1, err = 0, vid_out = -1, action = -1;
	int16 if_out = -1;
	struct ethervlan_header *evh;
	bool sendup = FALSE;
	struct sk_buff *nskb = skb;
	struct net_device *dev_out = NULL;

	ASSERT(plc != NULL);

	if (!plc->inited) {
#ifdef PLC_WET
		if (if_in == WL_PLC_IF_PLC) {
			PKTFREE(NULL, skb, FALSE);
			return BCME_OK;
		} else
			/* Will be freed later */
			return -1;
#else /* PLC_WET */
		PKTFREE(NULL, skb, FALSE);
		return BCME_OK;
#endif /* PLC_WET */
	}

	evh = (struct ethervlan_header *)skb->data;

	/* Read the outer tag */
	if (evh->vlan_type == HTON16(ETHER_TYPE_8021Q)) {
		vid_in = NTOH16(evh->vlan_tag) & VLAN_VID_MASK;

		/* See if there is an inner tag */
		evh = (struct ethervlan_header *)(skb->data + VLAN_TAG_LEN);
		if (evh->vlan_type == HTON16(ETHER_TYPE_8021Q))
			vid_in_in = NTOH16(evh->vlan_tag) & VLAN_VID_MASK;
	}


	if (vid_in == plc->rx_vid1) {
		/* Frame received from WDS* with VID 4 or PLC with VID 4,1.
		 * Send it up to the local bridge untagged.
		 */
		if (((if_in == WL_PLC_IF_WDS) && (vid_in_in == -1)) ||
#ifdef WL_PLC_NO_VLAN_TAG_RX
		    ((if_in == WL_PLC_IF_PLC) && ((vid_in_in == 1) || (vid_in_in == -1)))) {
#else
		    ((if_in == WL_PLC_IF_PLC) && (vid_in_in == 1))) {
#endif /* WL_PLC_NO_VLAN_TAG_RX */
			action = WL_PLC_ACTION_UNTAG;
			vid_out = -1;
			if_out = WL_PLC_IF_BR;
			sendup = TRUE;
		}
	} else if (vid_in == plc->rx_vid2) {
		/* Frame received from WDS* with VID 5. Send it down
		 * to PLC. Use the same VID, no untagging necessary.
		 */
		if (if_in == WL_PLC_IF_WDS) {
			if (vid_in_in == -1) {
				action = WL_PLC_ACTION_NONE;
				vid_out = plc->rx_vid2;
				dev_out = plc->plc_rxdev2;
				if_out = WL_PLC_IF_PLC;
			}
		} else if (if_in == WL_PLC_IF_PLC) {
			/* Frame received from PLC with VID 5,4.
			 * Send it down to WDS with VID 4. Only
			 * remove the outer header.
			 */
			if (vid_in_in == plc->rx_vid1) {
				vid_out = plc->rx_vid1;
				action = WL_PLC_ACTION_UNTAG;
				if_out = WL_PLC_IF_WDS;
			} else if (vid_in_in == plc->rx_vid2) {
				/* Frame received from PLC with VID 5,5.
				 * Send it down to WDS with VID 5. Only
				 * remove the outer header.
				 */
				action = WL_PLC_ACTION_UNTAG;
				vid_out = plc->rx_vid2;
				if_out = WL_PLC_IF_WDS;
			}
		}
	} else if (vid_in == plc->rx_vid3) {
		if ((if_in == WL_PLC_IF_WDS) && (vid_in_in == -1)) {
			/* Frame received from WDS* with VID 6. Send it
			 * to PLC. Use the same VID, no modification reqd.
			 */
			action = WL_PLC_ACTION_NONE;
			vid_out = plc->rx_vid3;
			dev_out = plc->plc_rxdev3;
			if_out = WL_PLC_IF_PLC;
		} else if ((if_in == WL_PLC_IF_PLC) && (vid_in_in == -1)) {
			/* Frame received from PLC with VID 6. Send it
			 * to WDS. Use the same VID, no modification reqd.
			 */
			action = WL_PLC_ACTION_NONE;
			vid_out = plc->rx_vid3;
			dev_out = plc->plc_rxdev3;
			if_out = WL_PLC_IF_WDS;
		}
	} else if (vid_in == -1) {
		/* Untagged frame received from WDS*. */
		if (if_in == WL_PLC_IF_WDS) {
			/* Send it up to bridge and send it down to PLC
			 * with VID 5.
			 */
			action = WL_PLC_ACTION_TAG;
			vid_out = plc->rx_vid2;
			dev_out = plc->plc_rxdev2;
			if_out = WL_PLC_IF_PLC;
			sendup = TRUE;
		} else if (if_in == WL_PLC_IF_PLC) {
			/* Untagged frame received from PLC.
			 * Send it up to bridge and send it down to WDS
			 * with VID 4.
			 */
			action = WL_PLC_ACTION_TAG;
			vid_out = plc->rx_vid1;
			if_out = WL_PLC_IF_WDS;
			sendup = TRUE;
		}
	}

	/* Send up the bridge if requested */
	if (sendup) {
		/* Make sure the received frame has no tag */
		if (if_out != WL_PLC_IF_BR) {
			nskb = skb_copy(skb, GFP_ATOMIC);
			if (nskb == NULL)
				return -ENOMEM;
#ifdef CTFPOOL
			PKTCLRFAST(NULL, nskb);
			CTFPOOLPTR(NULL, nskb) = NULL;
#endif /* CTFPOOL */
		}
		skb->dev = plc->wl_dev;
		if (vid_in != -1) {
			uint32 pull = VLAN_TAG_LEN;

			if (vid_in_in != -1)
				pull += VLAN_TAG_LEN;

			memmove(skb->data + pull, skb->data, ETHER_ADDR_LEN * 2);
			skb_pull(skb, pull);
		}
		err = -1;

		/* No processing needed since the frame is only going up to bridge */
		if (if_out == WL_PLC_IF_BR) {
			WL_PLC("%s: pkt %p length: %d\n", __FUNCTION__, skb, skb->len);
#if defined(BCMDBG) && defined(PLCDBG)
			prhex("->BRIDGE", skb->data, 64);
#endif /* BCMDBG && PLCDBG */
			return err;
		}
	}

	if (action == WL_PLC_ACTION_UNTAG) {
		if (vid_in_in != -1) {
			uint32 pull = VLAN_TAG_LEN;

			/* Remove outer/both the tags of double tagged
			 * vlan frames.
			 */
			if (vid_out == -1)
				pull += VLAN_TAG_LEN;

			memmove(nskb->data + pull, nskb->data, ETHER_ADDR_LEN * 2);
			skb_pull(nskb, pull);

			/* Modify the tag if needed. */
			if (vid_out != -1) {
				evh = (struct ethervlan_header *)nskb->data;
				evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
				evh->vlan_tag |= HTON16(vid_out);
			}
		} else {
			/* Untag a single tagged frame */
			memmove(nskb->data + VLAN_TAG_LEN, nskb->data,
			        ETHER_ADDR_LEN * 2);
			skb_pull(nskb, VLAN_TAG_LEN);
		}
	} else if (action == WL_PLC_ACTION_TAG) {
		if (vid_in_in != -1) {
			/* Remove the outer tag and modify the inner tag */
			evh = (struct ethervlan_header *)skb_pull(nskb, VLAN_TAG_LEN);
			evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
			evh->vlan_tag |= HTON16(vid_out);
		} else if (vid_in != -1) {
			/* Modify the tag as the frame has one tag */
			evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
			evh->vlan_tag |= HTON16(vid_out);
		} else {
			/* Add vlan header */
			uint16 prio = PKTPRIO(nskb) & VLAN_PRI_MASK;

			evh = (struct ethervlan_header *)skb_push(nskb, VLAN_TAG_LEN);
			memmove(nskb->data, nskb->data + VLAN_TAG_LEN, 2 * ETHER_ADDR_LEN);
			evh->vlan_type = HTON16(VLAN_TPID);
			evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
			evh->vlan_tag |= HTON16((prio << VLAN_PRI_SHIFT) | vid_out);
		}
	}

#ifdef PLCDBG
	if (vid_out != 6) {
		WL_PLC("%s: Rcvd from %s with VID: %d,%d\n", __FUNCTION__,
		       if_in == WL_PLC_IF_WDS ? "WDS" : "PLC", vid_in, vid_in_in);

		WL_PLC("%s: Sending to %s with VID: %d\n", __FUNCTION__,
		       if_out == WL_PLC_IF_WDS ? "WDS" :
		       if_out == WL_PLC_IF_PLC ? "PLC" : "BRIDGE", vid_out);

		if ((vid_out != 6) && (if_out == WL_PLC_IF_WDS)) {
			WL_PLC("%s: pkt %p length: %d\n", __FUNCTION__, nskb, nskb->len);
#ifdef BCMDBG
			prhex("->WDS", nskb->data, 64);
#endif /* BCMDBG */
		}
	}
#endif /* PLCDBG */

	/* Send out on the specified WDS or PLC interface */
	if (if_out == WL_PLC_IF_WDS) {
		wl_info_t *wl;

		wl = WL_DEV_IF(plc->wl_dev)->wl;

		/* Don't send out frames on WDS when the interface
		 * is down.
		 */
		if (!wl->pub->up) {
			/* Free the skb */
			PKTFREE(wl->osh, nskb, TRUE);
			return BCME_OK;
		}

		WL_LOCK(wl);
		wlc_sendpkt(WL_INFO_GET(plc->wl_dev)->wlc, (void *)nskb,
		            WL_DEV_IF(plc->wl_dev)->wlcif);
		WL_UNLOCK(wl);
	} else if (if_out == WL_PLC_IF_PLC) {
		ASSERT(if_in == WL_PLC_IF_WDS);
		ASSERT(dev_out != NULL);
		/* Add another tag so that switch will remove outer tag */
		evh = (struct ethervlan_header *)skb_push(nskb, VLAN_TAG_LEN);
		memmove(nskb->data, nskb->data + VLAN_TAG_LEN,
		        (2 * ETHER_ADDR_LEN) + VLAN_TAG_LEN);
		evh->vlan_tag &= ~HTON16(VLAN_PRI_MASK << VLAN_PRI_SHIFT);
		wl_plc_sendpkt(WL_DEV_IF(plc->wl_dev), nskb, dev_out);
	} else
		err = -1;

	return err;
}

#ifdef AP
/* This function is called when frames are received on one of the VLANs
 * setup for receiving PLC traffic.
 */
static int32
wl_plc_master_hook(struct sk_buff *skb, struct net_device *dev, void *arg)
{
#ifdef PLC_WET
	wl_info_t *wl;
	wl_plc_node_t *node;
	struct ether_header *eh;
#endif /* PLC_WET */
	wl_plc_t *plc = arg;
	int32 err;

	WL_PLC("%s: From PLC (%s) skb %p data %p mac %p len %d ether_type 0x%04x\n",
	       __FUNCTION__, dev->name, skb, skb->data, eth_hdr(skb),
	       skb->len, NTOH16(((struct ether_header *)skb->data)->ether_type));

	/* Frame received from PLC on one of rxvifs 1/2/3 */
	err = wl_plc_recv(skb, dev, plc, WL_PLC_IF_PLC);
	if (err != BCME_ERROR) {
		WL_PLC("%s: Sending pkt %p to WiFi\n", __FUNCTION__, skb);
		return err;
	}

#ifdef PLC_WET
	eh = (struct ether_header *)skb->data;
	wl = WL_DEV_IF(plc->wl_dev)->wl;

	node = wl_plc_node_find(wl, eh->ether_shost);

	/* Received frame from host/sta behind WET over PLC */
	if (node != NULL) {
		node->used = wl->pub->now;
		node->node_type |= NODE_TYPE_PLC_ONLY;
#ifdef WMF
		if (BSSCFG_AP(node->cfg) &&
		    wlc_wmf_proc(node->cfg, skb, eh, node, TRUE) != BCME_OK)
			return BCME_OK;
#endif /* WMF */
	} else {
		/* We are seeing this node for first time on PLC link.
		 * May be it is not reachable over wifi let's create
		 * a plc only node entry for this.
		 */
		wl_plc_node_add(wl, NODE_TYPE_PLC_ONLY,
		                eh->ether_shost, NULL, &node);

		node->cfg = plc->cfg;
	}

#ifdef MCAST_REGEN
	if (MCAST_REGEN_ENAB(plc->cfg) && BSSCFG_STA(plc->cfg) &&
	    !WLIF_IS_WDS(WL_DEV_IF(plc->wl_dev)))
		wlc_mcast_reverse_translation(eh);
#endif /* MCAST_REGEN */

	WL_PLC("%s: From PLC sending %p up to bridge on rxif1 %s\n",
	       __FUNCTION__, skb, skb->dev->name);

	/* PLC forward to WIFI. AP only, not URE */
	if (AP_ONLY(wl->pub)) {
		if (wl_plc_forward(skb, dev, plc, WL_PLC_IF_PLC) == BCME_OK)
			return BCME_OK;
	}

#endif /* PLC_WET */

	return BCME_ERROR;
}

static void
wl_plc_master_set(wl_plc_t *plc, struct net_device *plc_dev, wl_if_t *wlif)
{
	ASSERT(plc_dev != NULL);

	plc_dev->master = wlif->dev;
	plc_dev->flags |= IFF_SLAVE;
	plc_dev->master_hook = wl_plc_master_hook;
	plc_dev->master_hook_arg = plc;

	/* Disable vlan header removal */
	VLAN_DEV_INFO(plc_dev)->flags &= ~1;
}

int32
wl_plc_init(wl_if_t *wlif)
{
	int8 *var;
	struct net_device *plc_dev;
	wl_info_t *wl;
	char if_name[IFNAMSIZ] = { 0 };
	char wl_plc_nvram[] = "wlXXXXX_plc";
	int wl_plc_val = 0;
	wl_plc_t *plc = NULL;

	ASSERT(wlif != NULL);

	wl = wlif->wl;

	/* See if we need to initialize PLC for this interface */
	sprintf(wl_plc_nvram, "wl%d_plc", wl->pub->unit);
	var = getvar(NULL, wl_plc_nvram);
	if ((var == NULL) || ((wl_plc_val = bcm_atoi(var)) != 1)) {
		wl->pub->_plc = 0;
		return 0;
	} else {
		wl->pub->_plc = 1;
	}

	WL_PLC("wl%d: %s: Is WDS %d\n", wl->pub->unit, __FUNCTION__, WLIF_IS_WDS(wlif));

#ifdef PLC_WET
	/* Ignore WDS interfaces when WET is defined */
	if (WLIF_IS_WDS(wlif))
		return 0;

	if (wlif->plc != NULL)
		return 0;
	else
	{
		/* All secondary interfaces use global plc handle for now */
		if (wl->if_list != wlif) {
			wlif->plc = wl->if_list->plc;
			wlif->plc->users++;
			return 0;
		}
	}
#else /* PLC_WET */
	if (!WLIF_IS_WDS(wlif))
		return 0;
#endif /* PLC_WET */

	WL_PLC("wl%d: %s: Initializing the PLC VIFs\n", wl->pub->unit, __FUNCTION__);

	/* Read plc_vifs to initialize the VIDs to use for receiving
	 * and forwarding the frames.
	 */
	var = getvar(NULL, "plc_vifs");

	if (var == NULL) {
		WL_ERROR(("wl%d: %s: PLC vifs not configured\n",
		          wl->pub->unit, __FUNCTION__));
		return 0;
	}

	WL_PLC("wl%d: %s: plc_vifs = %s\n", wl->pub->unit, __FUNCTION__, var);

	/* Allocate plc info structure */
	plc = MALLOC(wl->osh, sizeof(wl_plc_t));
	if (plc == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wl->pub->unit, __FUNCTION__, MALLOCED(wl->osh)));
		return -ENOMEM;
	}
	bzero(plc, sizeof(wl_plc_t));

	plc->tx_vid = 3;
	plc->rx_vid1 = 4;
	plc->rx_vid2 = 5;
	plc->rx_vid3 = 6;

	/* Initialize the VIDs to use for PLC rx and tx */
	sscanf(var, "vlan%d vlan%d vlan%d vlan%d",
	       &plc->tx_vid, &plc->rx_vid1, &plc->rx_vid2, &plc->rx_vid3);

	WL_PLC("wl%d: %s: tx_vid %d rx_vid1: %d rx_vid2: %d rx_vid3 %d\n",
	       wl->pub->unit, __FUNCTION__, plc->tx_vid, plc->rx_vid1,
	       plc->rx_vid2, plc->rx_vid3);

	/* Save the plc dev pointer for sending the frames */
	sprintf(if_name, "vlan%d", plc->tx_vid);
	plc_dev = dev_get_by_name(if_name);
	ASSERT(plc_dev != NULL);
	dev_put(plc_dev);

	plc->plc_dev = plc_dev;

	WL_PLC("wl%d: %s: PLC dev: %s\n", wl->pub->unit, __FUNCTION__,
	       plc->plc_dev->name);

	/* Register WDS device as master for the PLC so that any
	 * thing received on PLC is sent first to WDS. Frames sent
	 * up to the netif layer will appear as if they are coming
	 * from WDS.
	 */
	wl_plc_master_set(plc, plc_dev, wlif);

	sprintf(if_name, "vlan%d", plc->rx_vid1);
	plc->plc_rxdev1 = dev_get_by_name(if_name);
	ASSERT(plc->plc_rxdev1 != NULL);
	dev_put(plc->plc_rxdev1);
	wl_plc_master_set(plc, plc->plc_rxdev1, wlif);

	sprintf(if_name, "vlan%d", plc->rx_vid2);
	plc->plc_rxdev2 = dev_get_by_name(if_name);
	ASSERT(plc->plc_rxdev2 != NULL);
	dev_put(plc->plc_rxdev2);
	wl_plc_master_set(plc, plc->plc_rxdev2, wlif);

	sprintf(if_name, "vlan%d", plc->rx_vid3);
	plc->plc_rxdev3 = dev_get_by_name(if_name);
	ASSERT(plc->plc_rxdev3 != NULL);
	dev_put(plc->plc_rxdev3);
	wl_plc_master_set(plc, plc->plc_rxdev3, wlif);

	/* Save the wds interface */
	plc->wl_dev = wlif->dev;

	WL_PLC("wl%d: %s: WL dev: %s\n", wl->pub->unit, __FUNCTION__,
	       plc->wl_dev->name);

	/* Save the bsscfg */
	plc->cfg = wlc_bsscfg_find_by_wlcif(wl->wlc, wlif->wlcif);

	wlif->plc = plc;
	plc->users = 0;

#ifdef PLC_WET
	if (!(plc->plc_wdtimer = wl_init_timer(wl, wl_plc_wdtimer, wl, "plc_wdtimer"))) {
		WL_ERROR(("wl%d:  wl_init_timer for PLC wdtimer failed\n", wl->pub->unit));
	} else {
		if ((var = getvar(NULL, "plc_wdtimer_interval")))
			plc->plc_wdtimer_interval = (uint32)bcm_atoi(var) * 1000;
		else
			plc->plc_wdtimer_interval = TIMER_INTERVAL_PLC_WATCHDOG;

		if ((var = getvar(NULL, "plc_node_aging_time")))
			plc_node_aging_time = (uint32)bcm_atoi(var);
		else
			plc_node_aging_time = PLC_NODE_AGING_TIME;

		wl_add_timer(wl, plc->plc_wdtimer, plc->plc_wdtimer_interval, TRUE);
	}
#endif /* PLC_WET */

	plc->inited = TRUE;

	WL_PLC("wl%d: %s: Initialized PLC dev %p\n", wl->pub->unit, __FUNCTION__, wlif->plc);

	return 0;
}
#endif /* AP */

static void
wl_plc_master_clear(wl_plc_t *plc, struct net_device *plc_dev)
{
	ASSERT(plc_dev != NULL);

	plc_dev->master = NULL;
	plc_dev->flags &= ~IFF_SLAVE;
	plc_dev->master_hook = NULL;
	plc_dev->master_hook_arg = NULL;

	return;
}

void
wl_plc_cleanup(wl_if_t *wlif)
{
	wl_info_t *wl;
	wl_plc_t *plc;

	wl = wlif->wl;

	/* Nothing to cleanup if plc is not enabled */
	if (wlif->plc == NULL)
		return;

#ifdef PLC_WET
	if (WLIF_IS_WDS(wlif))
		return;

	/* Clear the plc only when last user does cleanup */
	if (wlif->plc->users-- > 0) {
		wlif->plc = NULL;
		return;
	}

	/* free timer state */
	if (wlif->plc->plc_wdtimer) {
		wl_free_timer(wl, wlif->plc->plc_wdtimer);
		wlif->plc->plc_wdtimer = NULL;
	}
#else
	if (!WLIF_IS_WDS(wlif))
		return;
#endif /* PLC_WET */

	plc = wlif->plc;

	wl_plc_master_clear(plc, plc->plc_dev);
	wl_plc_master_clear(plc, plc->plc_rxdev1);
	wl_plc_master_clear(plc, plc->plc_rxdev2);
	wl_plc_master_clear(plc, plc->plc_rxdev3);

	MFREE(wl->osh, plc, sizeof(wl_plc_t));

	wlif->plc = NULL;

	WL_PLC("%s: Freed wlif plc\n", __FUNCTION__);

	return;
}

#ifdef PLC_WET
/* Try to send the frame over PLC, if either PLC is down or if it is
 * determined that WiFi is best path to use then the frame will be looped
 * back to us. Otherwise the frame will be sent over PLC.
 */
bool
wl_plc_loop(wl_info_t *wl, void *p, wl_if_t *wlif)
{
	struct sk_buff *skb = p, *nskb;

	if (wlif == NULL) {
		wlif = wl->if_list;
		ASSERT(wlif != NULL);
	}

	if (wlif->plc == NULL) {
		PKTFREE(wl->osh, skb, TRUE);
		WL_ERROR(("%s: Freeing frame since PLC is not enabled\n", __FUNCTION__));
		return FALSE;
	}

	/* Add the VLAN headers before sending to PLC */
	nskb = wl_plc_tx_prep(wlif, skb);
	if (nskb == NULL)
		return FALSE;

	WL_PLC("%s: Sending to PLC using VID %d,1\n", __FUNCTION__,
	       wlif->plc->tx_vid);

	/* Send to PLC */
	ASSERT(wlif->plc != NULL);
	nskb = PKTTONATIVE(wl->osh, nskb);
	wl_plc_sendpkt(wlif, nskb, wlif->plc->plc_dev);

	/* Broadcast and multicast frames are mirrored to PLC as well as
	 * Wireless LAN.
	 */
	if (ETHER_ISUCAST(skb->data)) {
		/* Free the original packet */
		if (nskb != skb) {
			WL_PLC("%s: Free the pkt\n", __FUNCTION__);
			PKTFREE(wl->osh, skb, TRUE);
		}
		WL_PLC("%s: Sent to PLC only\n", __FUNCTION__);
		return FALSE;
	}

	if (!wl->pub->up) {
		PKTFREE(wl->osh, skb, TRUE);
		WL_PLC("%s: Freeing frame since WiFi is down\n", __FUNCTION__);
		return FALSE;
	}

	WL_PLC("%s: Also sending %p untagged to WiFi\n", __FUNCTION__, skb);
	return TRUE;
}

int32
wl_plc_send_proc(wl_info_t *wl, void *p, struct wl_if *wlif)
{
	wlc_bsscfg_t *cfg;
	wl_plc_node_t *node;
	struct ether_header *eh = (struct ether_header *)PKTDATA(wl->osh, p);
	bool onex;
	uint16 et;

	cfg = wlc_bsscfg_find_by_wlcif(wl->wlc, wlif->wlcif);
	ASSERT(cfg != NULL);

	/* Send the frame over PLC if this BSS is STA and is not up. */
	if (BSSCFG_STA(cfg) && !cfg->up)
		goto try_plc;

#if defined(AP) && defined(WMF)
	if (wlc_wmf_proc(cfg, p, eh, NULL, FALSE) != BCME_OK)
		return BCME_OK;
#endif /* AP && WMF */

	et = eh->ether_type;
	onex = ((et == HTON16(ETHER_TYPE_802_1X)) ||
	        (et == HTON16(ETHER_TYPE_802_1X_PREAUTH)));

	/* Send onex frames over wifi only */
	if (onex)
		return BCME_ERROR;

	if (wl->if_list != wlif)
		return BCME_ERROR;

	/* Gigle control frames go over PLC */
	if ((et == HTON16(ETHER_TYPE_88E1)) || (et == HTON16(ETHER_TYPE_8912)))
		goto try_plc;

	/* Find the wifi plc node that current frame is destined to.
	 * If node is not found then send the frame over plc.
	 */
	node = wl_plc_node_find(wl, PKTDATA(wl->osh, p));

	/* If node is not found then it is neither wifi only nor
	 * wifi plc device. May be it plc only device, send over plc.
	 */
	if (node == NULL)
		goto try_plc;
	else {
		node->used = wl->pub->now;
		if (wl_plc_node_pref_plc(wl, node))
			goto try_plc;
	}

	/* Send over wifi only */
	return BCME_ERROR;

try_plc:
	/* Try sending over plc, frame may come back to us */
	if (!wl_plc_loop(wl, p, wlif))
		return BCME_OK;

	/* PLC tx done, send over wifi also */
	return BCME_ERROR;
}
#endif /* PLC_WET */

void
wl_plc_power_off(si_t *sih)
{
	uint refvdd_off_plc;    /* plc 3.3V control */
	uint en_off_plc;        /* plc 0.9V control */
	uint reset_plc;         /* plc reset signal */

	refvdd_off_plc = getgpiopin(NULL, "refvdd_off_plc", GPIO_PIN_NOTDEFINED);
	en_off_plc = getgpiopin(NULL, "en_off_plc", GPIO_PIN_NOTDEFINED);
	reset_plc = getgpiopin(NULL, "reset_plc", GPIO_PIN_NOTDEFINED);

	if ((refvdd_off_plc != GPIO_PIN_NOTDEFINED) &&
	    (en_off_plc != GPIO_PIN_NOTDEFINED) &&
	    (reset_plc != GPIO_PIN_NOTDEFINED)) {
		uint refvdd_off_plc_mask = 1 << refvdd_off_plc;
		uint en_off_plc_mask = 1 << en_off_plc;
		uint reset_plc_mask = 1 << reset_plc;

		/* turn off PLC */
		si_gpioout(sih, refvdd_off_plc_mask, refvdd_off_plc_mask, GPIO_HI_PRIORITY);
		si_gpioout(sih, en_off_plc_mask, 0, GPIO_HI_PRIORITY);
		si_gpioout(sih, reset_plc_mask, reset_plc_mask, GPIO_HI_PRIORITY);
	}
}
