/*
 * L2 Filter handling functions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dhd_l2_filter.c 473138 2014-04-30 14:01:06Z $
 *
 */
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmdevs.h>

#include <dngl_stats.h>
#include <dhd_dbg.h>
#include <dhd.h>
#include <dhd_l2_filter.h>

#include <proto/bcmip.h>
#include <proto/bcmipv6.h>
#include <proto/bcmudp.h>
#include <proto/bcmarp.h>
#include <proto/bcmicmp.h>
#include <proto/bcmproto.h>
#include <proto/bcmdhcp.h>

/* Adjust for ETHER_HDR_LEN pull in linux
 * which makes pkt nonaligned
 */
#define ALIGN_ADJ_BUFLEN		2

/* Proxy ARP processing return values */
#define PARP_DROP			0
#define PARP_NOP			1
#define PARP_TAKEN			2

#define	DHD_PARP_TABLE_SIZE		32		/* proxyarp hash table bucket size */
#define	DHD_PARP_TABLE_MASK		0x1f	/* proxyarp hash table index mask */
#define	DHD_PARP_TABLE_INDEX(val)	(val & DHD_PARP_TABLE_MASK)

#define	DHD_PARP_TIMEOUT		60000	/* proxyarp cache entry timerout duration(10 min) */

#define DHD_PARP_IS_TIMEOUT(pub, entry)	\
			(pub->tickcnt - entry->used > DHD_PARP_TIMEOUT)

#define	DHD_PARP_ANNOUNCE_WAIT		200	/* proxyarp announce wait duration(2 sec) */

#define DHD_PARP_ANNOUNCE_WAIT_REACH(pub, entry) \
	(pub->tickcnt - entry->used > DHD_PARP_ANNOUNCE_WAIT)

#define DHD_ARP_TABLE_UPDATE_TIMEOUT	100

#define DHD_IF_ARP_TABLE_LOCK_INIT(ptable) spin_lock_init(&(ptable)->arp_table_lock)
#define DHD_IF_ARP_TABLE_LOCK(ptable, flags) \
	spin_lock_irqsave(&(ptable)->arp_table_lock, (flags))
#define DHD_IF_ARP_TABLE_UNLOCK(ptable, flags) \
	spin_unlock_irqrestore(&(ptable)->arp_table_lock, (flags))

typedef struct parp_entry {
	struct parp_entry	*next;
	uint32			used;		/* time stamp */
	struct ether_addr	ea;
	bcm_tlv_t		ip;
} parp_entry_t;

typedef struct arp_table {
	parp_entry_t	*parp_table[DHD_PARP_TABLE_SIZE];   /* proxyarp entries in cache table */
	parp_entry_t	*parp_candidate_list;		    /* proxyarp entries in candidate list */
	uint8 parp_smac[ETHER_ADDR_LEN];		    /* L2 SMAC from DHCP Req */
	uint8 parp_cmac[ETHER_ADDR_LEN];		    /* Bootp Client MAC from DHCP Req */
	spinlock_t	arp_table_lock;
} arp_table_t;

static int dhd_parp_addentry(dhd_pub_t *pub, int ifidx, struct ether_addr *ea,
	uint8 *ip, uint8 ip_ver, bool cached);
static int dhd_parp_delentry(dhd_pub_t *pub, int ifidx, struct ether_addr *ea,
	uint8 *ip, uint8 ip_ver, bool cached);
static parp_entry_t * dhd_parp_findentry(dhd_pub_t *pub, int ifidx, uint8 *ip,
	uint8 ip_ver, bool cached);
static void * dhd_proxyarp_alloc_reply(dhd_pub_t *pub, uint32 pktlen, struct ether_addr *src_ea,
	struct ether_addr *dst_ea, uint16 ea_type, bool snap, void **p);
static uint8 dhd_handle_proxyarp(dhd_pub_t *pub, int ifidx, frame_proto_t *fp, void **reply,
	uint8 *reply_to_bss, void *pktbuf, bool istx);
static uint8 dhd_handle_proxyarp_icmp6(dhd_pub_t *pub, int ifidx, frame_proto_t *fp, void **reply,
	uint8 *reply_to_bss, void *pktbuf, bool istx);
static uint8 dhd_handle_proxyarp_dhcp4(dhd_pub_t *pub, int ifidx, frame_proto_t *fp, bool istx);
static int dhd_parp_modifyentry(dhd_pub_t *pub, int ifidx, struct ether_addr *ea,
	uint8 *ip, uint8 ip_ver, bool cached);
extern int dhd_get_pkt_ether_type(dhd_pub_t *dhd, void *skb, uint8 **data_ptr,
	int *len_ptr, uint16 *et_ptr, bool *snap_ptr);
extern int dhd_sendup(dhd_pub_t *dhdp, int ifidx, void *p);
extern bool dhd_sta_associated(dhd_pub_t *dhdp, uint32 bssidx, uint8 *mac);
extern void *dhd_get_ifp_arp_table_handle(dhd_pub_t *dhdp, uint32 bssidx);
extern bool dhd_parp_discard_is_enabled(dhd_pub_t *dhdp, uint32 ifidx);
extern bool dhd_parp_allnode_is_enabled(dhd_pub_t *dhdp, uint32 ifidx);

#ifdef DHD_DUMP_ARPTABLE
void dhd_parp_dump_table(dhd_pub_t *pub, int ifidx);
#endif

#ifdef DHD_DUMP_ARPTABLE
void
dhd_parp_dump_table(dhd_pub_t *pub, int ifidx)
{
	parp_entry_t *entry;
	uint16 idx, ip_len;
	arp_table_t *ptable;
	unsigned long flags;

	ip_len = IPV4_ADDR_LEN;
	ptable = (arp_table_t*)dhd_get_ifp_arp_table_handle(pub, ifidx);
	for (idx = 0; idx < DHD_PARP_TABLE_SIZE; idx++) {
		DHD_IF_ARP_TABLE_LOCK(ptable, flags);
		entry = ptable->parp_table[idx];
		DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
		while (entry) {
			printf("Cached entries..\n");
			printf("%d: %d.%d.%d.%d", idx, entry->ip.data[0], entry->ip.data[1],
				entry->ip.data[2], entry->ip.data[3]);
			printf("%02x:%02x:%02x:%02x:%02x:%02x", entry->ea.octet[0],
				entry->ea.octet[1], entry->ea.octet[2], entry->ea.octet[3],
				entry->ea.octet[4], entry->ea.octet[5]);
			printf("\n");
			entry = entry->next;
		}
	}
	DHD_IF_ARP_TABLE_LOCK(ptable, flags);
	entry = ptable->parp_candidate_list;
	DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
	while (entry) {
		printf("Candidate entries..\n");
		printf("%d.%d.%d.%d", entry->ip.data[0], entry->ip.data[1],
			entry->ip.data[2], entry->ip.data[3]);
		printf("%02x:%02x:%02x:%02x:%02x:%02x", entry->ea.octet[0],
			entry->ea.octet[1], entry->ea.octet[2], entry->ea.octet[3],
			entry->ea.octet[4], entry->ea.octet[5]);

		printf("\n");
		entry = entry->next;
	}
}
#endif /* DHD_DUMP_ARPTABLE */

void* dhd_init_l2_arp_table(dhd_pub_t* pub, int ifidx)
{
	arp_table_t *ptable = (arp_table_t*)MALLOCZ(pub->osh, sizeof(arp_table_t));
	ASSERT(ptable);
	DHD_IF_ARP_TABLE_LOCK_INIT(ptable);
	return (void*)ptable;
}

void dhd_deinit_l2_arp_table(dhd_pub_t* pub, void* ptable)
{
	MFREE(pub->osh, ptable, sizeof(arp_table_t));
}

/* modify the mac address for IP, in arp table */
static int
dhd_parp_modifyentry(dhd_pub_t *pub, int ifidx, struct ether_addr *ea,
	uint8 *ip, uint8 ip_ver, bool cached)
{
	parp_entry_t *entry;
	uint16 idx, ip_len;
	arp_table_t *ptable;
	unsigned long flags;

	if (ip_ver == IP_VER_4 && !IPV4_ADDR_NULL(ip) && !IPV4_ADDR_BCAST(ip)) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV4_ADDR_LEN - 1]);
		ip_len = IPV4_ADDR_LEN;
	}
	else if (ip_ver == IP_VER_6 && !IPV6_ADDR_NULL(ip)) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV6_ADDR_LEN - 1]);
		ip_len = IPV6_ADDR_LEN;
	}
	else {
	    return BCME_ERROR;
	}

	ptable = (arp_table_t*)dhd_get_ifp_arp_table_handle(pub, ifidx);
	DHD_IF_ARP_TABLE_LOCK(ptable, flags);
	if (cached) {
	    entry = ptable->parp_table[idx];
	} else {
	    entry = ptable->parp_candidate_list;
	}
	while (entry) {
		if (bcmp(entry->ip.data, ip, ip_len) == 0) {
			/* entry matches, overwrite mac content and return */
			bcopy((void *)ea, (void *)&entry->ea, ETHER_ADDR_LEN);
			entry->used = pub->tickcnt;
			DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
#ifdef DHD_DUMP_ARPTABLE
			dhd_parp_dump_table(pub, ifidx);
#endif
			return BCME_OK;
		}
		entry = entry->next;
	}
	DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
#ifdef DHD_DUMP_ARPTABLE
	dhd_parp_dump_table(pub, ifidx);
#endif
	return BCME_ERROR;
}

/* Add the IP entry in ARP table based on Cached argument, if cached argument is
 * non zero positive value: it adds to parp_table, else adds to
 * parp_candidate_list
 */
static int
dhd_parp_addentry(dhd_pub_t *pub, int ifidx, struct ether_addr *ea,
	uint8 *ip, uint8 ip_ver, bool cached)
{
	parp_entry_t *entry;
	uint16 idx, ip_len;
	arp_table_t *ptable;
	unsigned long flags;

	if (ip_ver == IP_VER_4 && !IPV4_ADDR_NULL(ip) && !IPV4_ADDR_BCAST(ip)) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV4_ADDR_LEN - 1]);
		ip_len = IPV4_ADDR_LEN;
	}
	else if (ip_ver == IP_VER_6 && !IPV6_ADDR_NULL(ip)) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV6_ADDR_LEN - 1]);
		ip_len = IPV6_ADDR_LEN;
	}
	else {
	    return BCME_ERROR;
	}

	if ((entry = MALLOCZ(pub->osh, sizeof(parp_entry_t) + ip_len)) == NULL) {
	    DHD_ERROR(("Allocating new parp_entry for IPv%d failed!!\n", ip_ver));
	    return BCME_NOMEM;
	}

	bcopy((void *)ea, (void *)&entry->ea, ETHER_ADDR_LEN);
	entry->used = pub->tickcnt;
	entry->ip.id = ip_ver;
	entry->ip.len = ip_len;
	bcopy(ip, entry->ip.data, ip_len);
	ptable = (arp_table_t*)dhd_get_ifp_arp_table_handle(pub, ifidx);
	DHD_IF_ARP_TABLE_LOCK(ptable, flags);
	if (cached) {
	    entry->next = ptable->parp_table[idx];
	    ptable->parp_table[idx] = entry;
	} else {
	    entry->next = ptable->parp_candidate_list;
	    ptable->parp_candidate_list = entry;
	}
	DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
#ifdef DHD_DUMP_ARPTABLE
	dhd_parp_dump_table(pub, ifidx);
#endif
	return BCME_OK;
}

/* Delete the IP entry in ARP table based on Cached argument, if cached argument is
 * non zero positive value: it delete from parp_table, else delete from
 * parp_candidate_list
 */
static int
dhd_parp_delentry(dhd_pub_t *pub, int ifidx, struct ether_addr *ea,
	uint8 *ip, uint8 ip_ver, bool cached)
{
	parp_entry_t *entry, *prev = NULL;
	uint16 idx, ip_len;
	arp_table_t *ptable;
	unsigned long flags;

	if (ip_ver == IP_VER_4) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV4_ADDR_LEN - 1]);
		ip_len = IPV4_ADDR_LEN;
	}
	else if (ip_ver == IP_VER_6) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV6_ADDR_LEN - 1]);
		ip_len = IPV6_ADDR_LEN;
	}
	else {
	    return BCME_ERROR;
	}
	ptable = (arp_table_t*)dhd_get_ifp_arp_table_handle(pub, ifidx);
	DHD_IF_ARP_TABLE_LOCK(ptable, flags);
	if (cached) {
	    entry = ptable->parp_table[idx];
	} else {
		entry = ptable->parp_candidate_list;
	}
	DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
	while (entry) {
		if (entry->ip.id == ip_ver &&
		    bcmp(entry->ip.data, ip, ip_len) == 0 &&
		    bcmp(&entry->ea, ea, ETHER_ADDR_LEN) == 0) {
			if (prev == NULL) {
			    DHD_IF_ARP_TABLE_LOCK(ptable, flags);
			    if (cached) {
				ptable->parp_table[idx] = entry->next;
			    } else {
				ptable->parp_candidate_list = entry->next;
			    }
			    DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
			} else {
			    prev->next = entry->next;
			}
			break;
		}
		prev = entry;
		entry = entry->next;
	}
	if (entry != NULL)
		MFREE(pub->osh, entry, sizeof(parp_entry_t) + ip_len);
#ifdef DHD_DUMP_ARPTABLE
	dhd_parp_dump_table(pub, ifidx);
#endif
	return BCME_OK;
}

/* search the IP entry in ARP table based on Cached argument, if cached argument is
 * non zero positive value: it searches from parp_table, else search from
 * parp_candidate_list
 */
static parp_entry_t *
dhd_parp_findentry(dhd_pub_t *pub, int ifidx, uint8 *ip, uint8 ip_ver, bool cached)
{
	parp_entry_t *entry;
	uint16 idx, ip_len;
	arp_table_t *ptable;
	unsigned long flags;

	if (ip_ver == IP_VER_4) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV4_ADDR_LEN - 1]);
		ip_len = IPV4_ADDR_LEN;
	} else if (ip_ver == IP_VER_6) {
		idx = DHD_PARP_TABLE_INDEX(ip[IPV6_ADDR_LEN - 1]);
		ip_len = IPV6_ADDR_LEN;
	} else {
		return NULL;
	}
	ptable = (arp_table_t*)dhd_get_ifp_arp_table_handle(pub, ifidx);
	DHD_IF_ARP_TABLE_LOCK(ptable, flags);
	if (cached) {
	    entry = ptable->parp_table[idx];
	} else {
	    entry = ptable->parp_candidate_list;
	}
	DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
	while (entry) {
	    if (entry->ip.id == ip_ver && bcmp(entry->ip.data, ip, ip_len) == 0) {
			/* time stamp of adding the station entry to arp table for ifp */
			entry->used = pub->tickcnt;
			break;
	    }
	    entry = entry->next;
	}
	return entry;
}

/* create 42 byte ARP packet for ARP response, aligned the Buffer */
static void *
dhd_proxyarp_alloc_reply(dhd_pub_t *pub, uint32 pktlen, struct ether_addr *src_ea,
	struct ether_addr *dst_ea, uint16 ea_type, bool snap, void **p)
{
	void *pkt;
	uint8 *frame;

	/* adjust pktlen since skb->data is aligned to 2 */
	pktlen += ALIGN_ADJ_BUFLEN;

	if ((pkt = PKTGET(pub->osh, pktlen, FALSE)) == NULL) {
		DHD_ERROR(("%s %d: PKTGET failed\n", __func__, __LINE__));
		return NULL;
	}
	/* adjust for pkt->data aligned */
	PKTPULL(pub->osh, pkt, ALIGN_ADJ_BUFLEN);
	frame = PKTDATA(pub->osh, pkt);

	/* Create 14-byte eth header, plus snap header if applicable */
	bcopy(src_ea, frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);
	bcopy(dst_ea, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	if (snap) {
		hton16_ua_store(pktlen, frame + ETHER_TYPE_OFFSET);
		bcopy(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN);
		hton16_ua_store(ea_type, frame + ETHER_HDR_LEN + SNAP_HDR_LEN);
	} else
		hton16_ua_store(ea_type, frame + ETHER_TYPE_OFFSET);

	*p = (void *)(frame + ETHER_HDR_LEN + (snap ? SNAP_HDR_LEN + ETHER_TYPE_LEN : 0));

	return pkt;
}

/* process ARP packets at recieving and transmitting time.
 * return values:
 *	PARP_TAKEN: ARP request has been converted to ARP response
 *	PARP_NOP:   No operation, pass the packet to corresponding next layer
 *	PARP_DROP:  free the packet resouce
 */
int
dhd_l2fltr_pkt_handle(dhd_pub_t *pub, int ifidx, void *pktbuf, bool istx)
{
	void *reply = NULL;
	uint8 reply_to_bss = 0;
	uint8 result = PARP_NOP;
	frame_proto_t fp;

	/* get frame type */
	if (hnd_frame_proto(PKTDATA(pub->osh, pktbuf), PKTLEN(pub->osh, pktbuf), &fp) != BCME_OK) {
		return BCME_ERROR;
	}

	if (fp.l3_t == FRAME_L3_ARP_H) {
		result = dhd_handle_proxyarp(pub, ifidx, &fp, &reply, &reply_to_bss, pktbuf, istx);
	} else if (fp.l4_t == FRAME_L4_ICMP6_H) {
		result = dhd_handle_proxyarp_icmp6(pub, ifidx, &fp, &reply, &reply_to_bss,
			pktbuf, istx);
	} else if (fp.l4_t == FRAME_L4_UDP_H) {
		result = dhd_handle_proxyarp_dhcp4(pub, ifidx, &fp, istx);
	}
	switch (result) {
		case PARP_TAKEN:
			if (reply != NULL) {
				if (reply_to_bss)
					dhd_sendpkt(pub, ifidx, reply);
				else
					dhd_sendup(pub, ifidx, reply);

				/* return OK to drop original packet */
				return BCME_OK;
			}
			break;
		case PARP_DROP:
			return BCME_OK;
			break;
		default:
			break;
	}

	/* return fail to let original packet keep traverse */
	return BCME_ERROR;
}

/* store the arp source /destination IP and MAC entries, based on ARP request
 * and ARP probe command. Create the ARP response packet, if destination entry
 * exist in ARP table, else if the parp_discard flag is set, packet will be
 * dropped.
 * return values :
 *	FRAME_TAKEN:	packet response has been created, response will be directed either to
 *			dhd_sendpkt or dhd_sendup(network stack)
 *	FRAME_DROP:	packet will be freed
 *	FRAME_NOP:	No operation
 */

static uint8
dhd_handle_proxyarp(dhd_pub_t *pub, int ifidx, frame_proto_t *fp, void **reply,
	uint8 *reply_to_bss, void *pktbuf, bool istx)
{
	parp_entry_t *entry;
	struct bcmarp *arp;
	uint16 op;

	arp = (struct bcmarp *)fp->l3;
	op = ntoh16(arp->oper);

	/* basic ether addr check */
	if (ETHER_ISNULLADDR(arp->src_eth) || ETHER_ISBCAST(arp->src_eth) ||
	    ETHER_ISMULTI(arp->src_eth)) {
		return PARP_NOP;
	}

	if (op > ARP_OPC_REPLY) {
		DHD_ERROR(("dhd%d: Invalid ARP operation(%d)\n", ifidx, op));
		return PARP_NOP;
	}

	/* handle learning on ARP-REQ|ARP-REPLY|ARP-Announcement */
	if (!IPV4_ADDR_NULL(arp->src_ip) && !IPV4_ADDR_BCAST(arp->src_ip)) {
		entry = dhd_parp_findentry(pub, ifidx, arp->src_ip, IP_VER_4, TRUE);
		if (entry == NULL) {
			dhd_parp_addentry(pub, ifidx, (struct ether_addr *)arp->src_eth,
			arp->src_ip, IP_VER_4, TRUE);
		} else {
			/* overwrite the mac value corresponding to existing IP, else add */
			dhd_parp_modifyentry(pub, ifidx, (struct ether_addr *)arp->src_eth,
			arp->src_ip, IP_VER_4, TRUE);
		}
	} else {
		/* only learning ARP-Probe(DAD) in receiving path */
		if (!istx && op == ARP_OPC_REQUEST) {
			entry = dhd_parp_findentry(pub, ifidx, arp->dst_ip,
				IP_VER_4, TRUE);
			if (entry == NULL)
				entry = dhd_parp_findentry(pub, ifidx, arp->dst_ip,
					IP_VER_4, FALSE);
			if (entry == NULL)
				dhd_parp_addentry(pub, ifidx, (struct ether_addr *)arp->src_eth,
					arp->dst_ip, IP_VER_4, FALSE);
		}
	}

	/* perform candidate entry delete if some STA reply with ARP-Announcement */
	if (op == ARP_OPC_REPLY) {
		entry = dhd_parp_findentry(pub, ifidx, arp->src_ip, IP_VER_4, FALSE);
		if (entry) {
			struct ether_addr ea;
			bcopy(&entry->ea, &ea, ETHER_ADDR_LEN);
			dhd_parp_delentry(pub, ifidx, &ea, arp->src_ip, IP_VER_4, FALSE);
		}
	}

	/* handle sending path */
	if (istx) {
		/* Drop ARP-Announcement(Gratuitous ARP) on sending path */
		if (bcmp(arp->src_ip, arp->dst_ip, IPV4_ADDR_LEN) == 0) {
			return PARP_DROP;
		}

		if (op == ARP_OPC_REQUEST) {
			struct bcmarp *arp_reply;
			uint16 pktlen = ETHER_HDR_LEN + ARP_DATA_LEN;
			bool snap = FALSE;

			if ((entry = dhd_parp_findentry(pub, ifidx,
				arp->dst_ip, IP_VER_4, TRUE)) == NULL) {
				DHD_INFO(("No entry avaailable for %d.%d.%d.%d\n",
					arp->dst_ip[0], arp->dst_ip[1], arp->dst_ip[2],
					arp->dst_ip[3]));

				if (dhd_parp_discard_is_enabled(pub, ifidx))
					return PARP_DROP;
				else
					return PARP_NOP;
			}

			if (bcmp(arp->src_eth, (uint8 *)&entry->ea, ETHER_ADDR_LEN) == 0)
				return PARP_DROP;

			/* STA asking to some address not belong to BSS. Drop frame */
			if (!dhd_sta_associated(pub, ifidx, (uint8 *)&entry->ea.octet)) {
				return PARP_DROP;
			}

			/* determine dst is within bss or outside bss */
			if (dhd_sta_associated(pub, ifidx, arp->src_eth)) {
				/* dst is within bss, mark it */
				*reply_to_bss = 1;
			}

			if (fp->l2_t == FRAME_L2_SNAP_H || fp->l2_t == FRAME_L2_SNAPVLAN_H) {
				pktlen += SNAP_HDR_LEN + ETHER_TYPE_LEN;
				snap = TRUE;
			}

			/* Create 42-byte arp-reply data frame */
			if ((*reply = dhd_proxyarp_alloc_reply(pub, pktlen, &entry->ea,
				(struct ether_addr *)arp->src_eth, ETHER_TYPE_ARP, snap,
				(void **)&arp_reply)) == NULL) {
				DHD_ERROR(("dhd%d: failed to allocate reply frame. drop it\n",
					ifidx));
				return PARP_NOP;
			}

			/* copy first 6 bytes from ARP-Req to ARP-Reply(htype, ptype, hlen, plen) */
			bcopy(arp, arp_reply, ARP_OPC_OFFSET);
			hton16_ua_store(ARP_OPC_REPLY, &arp_reply->oper);
			bcopy(&entry->ea, arp_reply->src_eth, ETHER_ADDR_LEN);
			bcopy(&entry->ip.data, arp_reply->src_ip, IPV4_ADDR_LEN);
			bcopy(arp->src_eth, arp_reply->dst_eth, ETHER_ADDR_LEN);
			bcopy(arp->src_ip, arp_reply->dst_ip, IPV4_ADDR_LEN);
			return PARP_TAKEN;
		}
		/* ARP REPLY */
		else {
			entry = dhd_parp_findentry(pub, ifidx, arp->src_ip, IP_VER_4, TRUE);

			/* If SMAC-SIP in reply frame is inconsistent
			 * to exist entry, drop frame(HS2-4.5.C)
			 */
			if (entry && bcmp(arp->src_eth, &entry->ea, ETHER_ADDR_LEN) != 0) {
				return PARP_DROP;
			}
		}

	}
	return PARP_NOP;
}

/* returns 0 if gratuitous ARP or unsolicited neighbour advertisement */
int
dhd_process_gratuitous_arp(dhd_pub_t *pub, void *pktbuf)
{
	uint8 *frame = PKTDATA(pub->osh, pktbuf);
	uint16 ethertype;
	int send_ip_offset, target_ip_offset;
	int iplen;
	int minlen;
	uint8 *data;
	int datalen;
	bool snap;

	if (dhd_get_pkt_ether_type(pub, pktbuf, &data, &datalen, &ethertype, &snap) != BCME_OK)
	    return BCME_ERROR;

	if (!ETHER_ISBCAST(frame + ETHER_DEST_OFFSET) &&
	    bcmp(&ether_ipv6_mcast, frame + ETHER_DEST_OFFSET, sizeof(ether_ipv6_mcast))) {
	    return BCME_ERROR;
	}

	if (ethertype == ETHER_TYPE_ARP) {
		DHD_INFO(("%s ARP RX data : %p: datalen : %d\n",  __FUNCTION__, data, datalen));
		send_ip_offset = ARP_SRC_IP_OFFSET;
		target_ip_offset = ARP_TGT_IP_OFFSET;
		iplen = IPV4_ADDR_LEN;
		minlen = ARP_DATA_LEN;
	} else if (ethertype == ETHER_TYPE_IPV6) {
		send_ip_offset = NEIGHBOR_ADVERTISE_SRC_IPV6_OFFSET;
		target_ip_offset = NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET;
		iplen = IPV6_ADDR_LEN;
		minlen = target_ip_offset + iplen;

		/* check for neighbour advertisement */
		if (datalen >= minlen && (data[IPV6_NEXT_HDR_OFFSET] != IP_PROT_ICMP6 ||
		    data[NEIGHBOR_ADVERTISE_TYPE_OFFSET] != NEIGHBOR_ADVERTISE_TYPE))
			return BCME_ERROR;
	} else {
		return BCME_ERROR;
	}

	if (datalen < minlen) {
		DHD_ERROR(("dhd: dhd_gratuitous_arp: truncated packet (%d)\n", datalen));
		return BCME_ERROR;
	}

	if (bcmp(data + send_ip_offset, data + target_ip_offset, iplen) == 0) {
		return BCME_OK;
	}

	return BCME_ERROR;
}

/* update arp table entries for every proxy arp enable interface */
void
dhd_l2_filter_arp_table_update(dhd_pub_t *pub, int ifidx, bool all, uint8 *del_ea, bool periodic)
{
	parp_entry_t *prev, *entry, *delentry;
	uint16 idx, ip_ver;
	struct ether_addr ea;
	uint8 ip[IPV6_ADDR_LEN];
	arp_table_t *ptable;
	unsigned long flags;

	ptable = (arp_table_t*)dhd_get_ifp_arp_table_handle(pub, ifidx);
	for (idx = 0; idx < DHD_PARP_TABLE_SIZE; idx++) {
		DHD_IF_ARP_TABLE_LOCK(ptable, flags);
		entry = ptable->parp_table[idx];
		DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
		while (entry) {
			/* check if the entry need to be removed */
			if (all || (periodic && DHD_PARP_IS_TIMEOUT(pub, entry)) ||
			    (del_ea != NULL && !bcmp(del_ea, &entry->ea, ETHER_ADDR_LEN))) {
				/* copy frame here */
				ip_ver = entry->ip.id;
				bcopy(entry->ip.data, ip, entry->ip.len);
				bcopy(&entry->ea, &ea, ETHER_ADDR_LEN);
				entry = entry->next;
				dhd_parp_delentry(pub, ifidx, &ea, ip, ip_ver, TRUE);
			}
			else {
				entry = entry->next;
			}
		}
	}

	/* remove candidate or promote to real entry */
	prev = delentry = NULL;
	entry = ptable->parp_candidate_list;
	while (entry) {
		/* remove candidate */
		if (all || (periodic && DHD_PARP_ANNOUNCE_WAIT_REACH(pub, entry)) ||
		    (del_ea != NULL && !bcmp(del_ea, (uint8 *)&entry->ea, ETHER_ADDR_LEN))) {
			bool promote = (periodic && DHD_PARP_ANNOUNCE_WAIT_REACH(pub, entry)) ?
				TRUE: FALSE;
			parp_entry_t *node = NULL;

			ip_ver = entry->ip.id;

			if (prev == NULL)
				ptable->parp_candidate_list = entry->next;
			else
				prev->next = entry->next;

			node = dhd_parp_findentry(pub, ifidx,
				entry->ip.data, IP_VER_6, TRUE);
			if (promote && node == NULL) {
				dhd_parp_addentry(pub, ifidx, &entry->ea, entry->ip.data,
					entry->ip.id, TRUE);
			}
			MFREE(pub->osh, entry, sizeof(parp_entry_t) + entry->ip.len);

			if (prev == NULL) {
				entry = ptable->parp_candidate_list;
			} else {
				entry = prev->next;
			}
		}
		else {
			prev = entry;
			entry = entry->next;
		}
	}
}

/* watchdog timer routine calls arp update routine per interface every 1 sec */
void
dhd_l2_filter_watchdog(dhd_pub_t *dhdp)
{
	int i;
	static uint32 cnt = 0;

	cnt++;

	if (cnt != DHD_ARP_TABLE_UPDATE_TIMEOUT)
		return;
	cnt = 0;

	for (i = 0; i < DHD_MAX_IFS; i++) {
		if (dhd_get_parp_status(dhdp, i))
			dhd_l2_filter_arp_table_update(dhdp, i, FALSE, NULL, TRUE);
	}
}

/* Derived from wlc_wnm.c */
static uint16
calc_checksum(uint8 *src_ipa, uint8 *dst_ipa, uint32 ul_len, uint8 prot, uint8 *ul_data)
{
	uint16 *startpos;
	uint32 sum = 0;
	int i;
	uint16 answer = 0;

	if (src_ipa) {
		uint8 ph[8];
		for (i = 0; i < (IPV6_ADDR_LEN / 2); i++) {
			sum += *((uint16 *)src_ipa);
			src_ipa += 2;
		}

		for (i = 0; i < (IPV6_ADDR_LEN / 2); i++) {
			sum += *((uint16 *)dst_ipa);
			dst_ipa += 2;
		}

		*((uint32 *)ph) = hton32(ul_len);
		*((uint32 *)(ph+4)) = 0;
		ph[7] = prot;
		startpos = (uint16 *)ph;
		for (i = 0; i < 4; i++) {
			sum += *startpos++;
		}
	}

	startpos = (uint16 *)ul_data;
	while (ul_len > 1) {
		sum += *startpos++;
		ul_len -= 2;
	}

	if (ul_len == 1) {
		*((uint8 *)(&answer)) = *((uint8 *)startpos);
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;

	return answer;
}

/* Derived from wlc_wnm.c.  The length of the option including
 * the type and length fields in units of 8 octets
 */
static bcm_tlv_t *
parse_nd_options(void *buf, int buflen, uint key)
{
	bcm_tlv_t *elt;
	int totlen;

	elt = (bcm_tlv_t*)buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= TLV_HDR_LEN) {
		int len = elt->len * 8;

		/* validate remaining totlen */
		if ((elt->id == key) &&
		    (totlen >= len))
			return (elt);

		elt = (bcm_tlv_t*)((uint8*)elt + len);
		totlen -= len;
	}

	return NULL;
}

/* store the arp source /destination IPV6 and MAC entries, based on ARP request
 * and ARP probe command. Create the ARP response packet, if destination entry
 * exist in ARP table, else if the parp_discard flag is set, packet will be
 * dropped.
 * return values :
 *	PARP_TAKEN:	packet response has been created, response will be directed either to
 *			dhd_sendpkt or dhd_sendup(network stack)
 *	PARP_DROP:	packet will be freed
 *	PARP_NOP:	No operation
 */
static uint8
dhd_handle_proxyarp_icmp6(dhd_pub_t *pub, int ifidx, frame_proto_t *fp, void **reply,
	uint8 *reply_to_bss, void *pktbuf, bool istx)
{
	struct ether_header *eh = (struct ether_header *)fp->l2;
	struct ipv6_hdr *ipv6_hdr = (struct ipv6_hdr *)fp->l3;
	struct nd_msg *nd_msg = (struct nd_msg *)(fp->l3 + sizeof(struct ipv6_hdr));
	parp_entry_t *entry;
	struct ether_addr *entry_ea = NULL;
	uint8 *entry_ip = NULL;
	bool dad = FALSE;	/* Duplicate Address Detection */
	uint8 link_type = 0;
	bcm_tlv_t *link_addr = NULL;
	int16 ip6_icmp6_len = sizeof(struct ipv6_hdr) + sizeof(struct nd_msg);
	char ipbuf[64], eabuf[32];

	/* basic check */
	if ((fp->l3_len < ip6_icmp6_len) ||
	    (ipv6_hdr->nexthdr != ICMPV6_HEADER_TYPE))
		return PARP_NOP;
	/* Neighbor Solicitation */
	if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NS) {
	    link_type = ICMPV6_ND_OPT_TYPE_SRC_MAC;
	    if (IPV6_ADDR_NULL(ipv6_hdr->saddr.addr)) {
		/* ip6 src field is null, set offset to icmp6 target field */
		entry_ip = nd_msg->target.addr;
		dad = TRUE;
	    } else {
		/* ip6 src field not null, set offset to ip6 src field */
		entry_ip = ipv6_hdr->saddr.addr;
	    }
	}
	/* Neighbor Advertisement */
	else if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA) {
	    link_type = ICMPV6_ND_OPT_TYPE_TARGET_MAC;
	    entry_ip = nd_msg->target.addr;
	} else {
	    /* not an interesting frame, return without action */
	    return PARP_NOP;
	}

	/* if icmp6-option exists, retrieve layer2 link address from icmp6-option */
	if (fp->l3_len > ip6_icmp6_len) {
	    link_addr = parse_nd_options(fp->l3 + ip6_icmp6_len,
	    fp->l3_len - ip6_icmp6_len, link_type);
	    if (link_addr && link_addr->len == ICMPV6_ND_OPT_LEN_LINKADDR)
		entry_ea = (struct ether_addr *)&link_addr->data;
	}
	/* if no ea, retreive layer2 link address from ether header */
	if (entry_ea == NULL)
	    entry_ea = (struct ether_addr *)eh->ether_shost;

	/* basic ether addr check */
	if (ETHER_ISNULLADDR(eh->ether_shost) || ETHER_ISBCAST(eh->ether_shost) ||
	    ETHER_ISMULTI(eh->ether_shost)) {
	    DHD_ERROR(("dhd%d: Invalid Ether addr(%s) of icmp6 pkt\n", ifidx,
	    bcm_ether_ntoa((struct ether_addr *)eh->ether_shost, eabuf)));
	    return PARP_NOP;
	}

	/* handle learning on Neighbor-Advertisement | Neighbor-Solicition(non-DAD) */
	if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA ||
	    (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NS && !dad)) {
	    entry = dhd_parp_findentry(pub, ifidx, entry_ip, IP_VER_6, TRUE);
	    if (entry == NULL) {
		DHD_ERROR(("dhd%d: Add new parp_entry by ICMP6 %s %s\n",
		    ifidx, bcm_ether_ntoa(entry_ea, eabuf),
		    bcm_ipv6_ntoa((void *)entry_ip, ipbuf)));
		dhd_parp_addentry(pub, ifidx, entry_ea, entry_ip, IP_VER_6, TRUE);
	    }
	} else {
	    /* only learning Neighbor-Solicition(DAD) in receiving path */
	    if (!istx) {
		entry = dhd_parp_findentry(pub, ifidx, entry_ip, IP_VER_6, TRUE);
		if (entry == NULL)
		    entry = dhd_parp_findentry(pub, ifidx, entry_ip, IP_VER_6, FALSE);
		if (entry == NULL) {
		    DHD_ERROR(("dhd%d: create candidate parp_entry by ICMP6 %s %s\n",
		    ifidx, bcm_ether_ntoa((struct ether_addr *)entry_ea,
		    eabuf), bcm_ipv6_ntoa((void *)entry_ip, ipbuf)));
		    dhd_parp_addentry(pub, ifidx, entry_ea, entry_ip, IP_VER_6, FALSE);
		}
	    }
	}

	/* perform candidate entry delete if some STA reply with Neighbor-Advertisement */
	if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA) {
	    entry = dhd_parp_findentry(pub, ifidx, entry_ip, IP_VER_6, FALSE);
	    if (entry) {
		struct ether_addr ea;
		bcopy(&entry->ea, &ea, ETHER_ADDR_LEN);
		DHD_ERROR(("dhd%d: withdraw candidate parp_entry IPv6 %s %s\n",
		    ifidx, bcm_ether_ntoa(&ea, eabuf), bcm_ipv6_ntoa((void *)entry_ip, ipbuf)));
		dhd_parp_delentry(pub, ifidx, &ea, entry_ip, IP_VER_6, FALSE);
	    }
	}

	/* handle sending path */
	if (istx) {
	    if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA) {
		/* Drop Unsolicited Network Advertisment packet from STA */
		if (!(nd_msg->icmph.opt.nd_advt.router) &&
			(!nd_msg->icmph.opt.nd_advt.solicited)) {
			return PARP_DROP;
		}
	    }
	    /* try to reply if trying to send arp request frame */
	    if (nd_msg->icmph.icmp6_type == ICMPV6_PKT_TYPE_NS) {
		struct ether_addr *reply_mac;
		struct ipv6_hdr *ipv6_reply;
		struct nd_msg *nd_msg_reply;
		struct nd_msg_opt *nd_msg_opt_reply;
		uint16 pktlen = ETHER_HDR_LEN + sizeof(struct ipv6_hdr) +
		    sizeof(struct nd_msg) + sizeof(struct nd_msg_opt);
		bool snap = FALSE;
		uint8 ipv6_mcast_allnode_ea[6] = {0x33, 0x33, 0x0, 0x0, 0x0, 0x1};
		uint8 ipv6_mcast_allnode_ip[16] = {0xff, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1};
		if ((entry = dhd_parp_findentry(pub, ifidx, nd_msg->target.addr,
			IP_VER_6, TRUE)) == NULL) {
		    if (dhd_parp_discard_is_enabled(pub, ifidx))
			return PARP_DROP;
		    else
			return PARP_NOP;
		}
		/* STA asking itself address. drop this frame */
		if (bcmp(entry_ea, (uint8 *)&entry->ea, ETHER_ADDR_LEN) == 0) {
		    return PARP_DROP;
		}
		/* STA asking to some address not belong to BSS.  Drop frame */
		if (!dhd_sta_associated(pub, ifidx, (uint8 *)&entry->ea.octet)) {
		    return PARP_DROP;
		}
		/* determine dst is within bss or outside bss */
		if (dhd_sta_associated(pub, ifidx, eh->ether_shost)) {
		    /* dst is within bss, mark it */
		    *reply_to_bss = 1;
		}

		if (fp->l2_t == FRAME_L2_SNAP_H || fp->l2_t == FRAME_L2_SNAPVLAN_H) {
		    pktlen += SNAP_HDR_LEN + ETHER_TYPE_LEN;
		    snap = TRUE;
		}
		/* Create 72 bytes neighbor advertisement data frame */
		/* determine l2 mac address is unicast or ipv6 mcast */
		if (dad) {
		    if (dhd_parp_allnode_is_enabled(pub, ifidx))
			reply_mac = (struct ether_addr *)ipv6_mcast_allnode_ea;
		    else
			reply_mac = (struct ether_addr *)eh->ether_shost;
		} else {
		    reply_mac = entry_ea;
		}
		if ((*reply = dhd_proxyarp_alloc_reply(pub, pktlen, &entry->ea, reply_mac,
			ETHER_TYPE_IPV6, snap, (void **)&ipv6_reply)) == NULL) {
		    return PARP_NOP;
		}
		/* construct 40 bytes ipv6 header */
		bcopy((uint8 *)ipv6_hdr, (uint8 *)ipv6_reply, sizeof(struct ipv6_hdr));
		hton16_ua_store(sizeof(struct nd_msg) + sizeof(struct nd_msg_opt),
			&ipv6_reply->payload_len);
		ipv6_reply->hop_limit = 255;
		bcopy(nd_msg->target.addr, ipv6_reply->saddr.addr, IPV6_ADDR_LEN);
		/* if Duplicate address detected, filled all-node address as destination */
		if (dad)
		    bcopy(ipv6_mcast_allnode_ip, ipv6_reply->daddr.addr, IPV6_ADDR_LEN);
		else
		    bcopy(entry_ip, ipv6_reply->daddr.addr, IPV6_ADDR_LEN);
		/* Create 32 bytes icmpv6 NA frame body */
		nd_msg_reply = (struct nd_msg *)
		    (((uint8 *)ipv6_reply) + sizeof(struct ipv6_hdr));
		nd_msg_reply->icmph.icmp6_type = ICMPV6_PKT_TYPE_NA;
		nd_msg_reply->icmph.icmp6_code = 0;
		nd_msg_reply->icmph.opt.reserved = 0;
		nd_msg_reply->icmph.opt.nd_advt.override = 1;
		/* from observing win7 behavior, only non dad will set solicited flag */
		if (!dad)
		    nd_msg_reply->icmph.opt.nd_advt.solicited = 1;
		bcopy(nd_msg->target.addr, nd_msg_reply->target.addr, IPV6_ADDR_LEN);
		nd_msg_opt_reply = (struct nd_msg_opt *)
					(((uint8 *)nd_msg_reply) + sizeof(struct nd_msg));
		nd_msg_opt_reply->type = ICMPV6_ND_OPT_TYPE_TARGET_MAC;
		nd_msg_opt_reply->len = ICMPV6_ND_OPT_LEN_LINKADDR;
		bcopy((uint8 *)&entry->ea, nd_msg_opt_reply->mac_addr, ETHER_ADDR_LEN);
		/* calculate ICMPv6 check sum */
		nd_msg_reply->icmph.icmp6_cksum = 0;
		nd_msg_reply->icmph.icmp6_cksum = calc_checksum(ipv6_reply->saddr.addr,
			ipv6_reply->daddr.addr, sizeof(struct nd_msg) +
			sizeof(struct nd_msg_opt), IP_PROT_ICMP6, (uint8 *)nd_msg_reply);
		return	PARP_TAKEN;
	    }
	}
	return PARP_NOP;
}

static uint8
dhd_handle_proxyarp_dhcp4(dhd_pub_t *pub, int ifidx, frame_proto_t *fp, bool istx)
{
	uint8 *dhcp;
	bcm_tlv_t *msg_type;
	uint16 opt_len, offset = DHCP_OPT_OFFSET;
	arp_table_t *ptable;
	unsigned long flags;
	uint8 smac_addr[ETHER_ADDR_LEN];
	uint8 cmac_addr[ETHER_ADDR_LEN];
	char eabuf[32];

	ptable = (arp_table_t*)dhd_get_ifp_arp_table_handle(pub, ifidx);
	dhcp = (uint8 *)(fp->l4 + UDP_HDR_LEN);

	/* First option must be magic cookie */
	if ((dhcp[offset + 0] != 0x63) || (dhcp[offset + 1] != 0x82) ||
	    (dhcp[offset + 2] != 0x53) || (dhcp[offset + 3] != 0x63))
		return PARP_NOP;

	/* skip 4 byte magic cookie and calculate dhcp opt len */
	offset += 4;
	opt_len = fp->l4_len - UDP_HDR_LEN - offset;

	DHD_IF_ARP_TABLE_LOCK(ptable, flags);
	bcopy((void*)ptable->parp_smac, (void*)smac_addr, ETHER_ADDR_LEN);
	bcopy((void*)ptable->parp_cmac, (void*)cmac_addr, ETHER_ADDR_LEN);
	DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);

	/* sending path, process DHCP Ack frame only */
	if (istx) {
		msg_type = bcm_parse_tlvs(&dhcp[offset], opt_len, DHCP_OPT_MSGTYPE);
		if (msg_type == NULL || msg_type->data[0] != DHCP_OPT_MSGTYPE_ACK)
			return PARP_NOP;

		/* compared to DHCP Req client mac */
		if (bcmp((void*)cmac_addr, &dhcp[DHCP_CHADDR_OFFSET], ETHER_ADDR_LEN)) {
			bcopy(cmac_addr, eabuf, ETHER_ADDR_LEN);
			DHD_ERROR(("dhd%d: Unmatch DHCP Req Client MAC (%s)",
				ifidx, eabuf));
			DHD_ERROR(("to DHCP Ack Client MAC(%s)\n",
				bcm_ether_ntoa((struct ether_addr *)&dhcp[DHCP_CHADDR_OFFSET],
				eabuf)));
			return PARP_NOP;
		}

		/* If client transmit DHCP Inform, server will response DHCP Ack with NULL YIADDR */
		if (IPV4_ADDR_NULL(&dhcp[DHCP_YIADDR_OFFSET]))
			return PARP_NOP;

		/* STA asking to some address not belong to BSS. Drop frame */
		if (!dhd_sta_associated(pub, ifidx, (uint8 *)smac_addr)) {
			parp_entry_t *entry = dhd_parp_findentry(pub, ifidx,
				&dhcp[DHCP_YIADDR_OFFSET], IP_VER_4, TRUE);

			if (entry == NULL) {
				dhd_parp_addentry(pub, ifidx, (struct ether_addr*)smac_addr,
					&dhcp[DHCP_YIADDR_OFFSET], IP_VER_4, TRUE);
			}
		}
	}
	else {	/* receiving path, process DHCP Req frame only */
		struct ether_header *eh = (struct ether_header *)fp->l2;

		msg_type = bcm_parse_tlvs(&dhcp[offset], opt_len, DHCP_OPT_MSGTYPE);
		if (msg_type == NULL || msg_type->data[0] != DHCP_OPT_MSGTYPE_REQ)
			return FRAME_NOP;

		/* basic ether addr check */
		if (ETHER_ISNULLADDR(&dhcp[DHCP_CHADDR_OFFSET]) ||
		    ETHER_ISBCAST(&dhcp[DHCP_CHADDR_OFFSET]) ||
		    ETHER_ISMULTI(&dhcp[DHCP_CHADDR_OFFSET]) ||
		    ETHER_ISNULLADDR(eh->ether_shost) ||
		    ETHER_ISBCAST(eh->ether_shost) ||
		    ETHER_ISMULTI(eh->ether_shost)) {
			DHD_ERROR(("dhd%d: Invalid Ether addr(%s)", ifidx,
				bcm_ether_ntoa((struct ether_addr *)eh->ether_shost, eabuf)));
			DHD_ERROR(("(%s) of DHCP Req pkt\n",
				bcm_ether_ntoa((struct ether_addr *)&dhcp[DHCP_CHADDR_OFFSET],
				eabuf)));
			return PARP_NOP;
		}
		/*
		 * In URE mode, EA might different from Client Mac in BOOTP and SMAC in L2 hdr.
		 * We need to saved SMAC addr and Client Mac.  So that when receiving DHCP Ack,
		 * we can compare saved Client Mac and Client Mac in DHCP Ack frame.  If it's
		 * matched, then our target MAC would be saved L2 SMAC
		 */
		DHD_IF_ARP_TABLE_LOCK(ptable, flags);
		bcopy(eh->ether_shost, ptable->parp_smac, ETHER_ADDR_LEN);
		bcopy(&dhcp[DHCP_CHADDR_OFFSET], ptable->parp_cmac, ETHER_ADDR_LEN);
		DHD_IF_ARP_TABLE_UNLOCK(ptable, flags);
	}

	return PARP_NOP;
}
