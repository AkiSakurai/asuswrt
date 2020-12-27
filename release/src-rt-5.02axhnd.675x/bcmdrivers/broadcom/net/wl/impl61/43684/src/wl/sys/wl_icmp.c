/*
 * ICMP - ICMP Offload
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
 * $Id:$
 */

/**
 * XXX Apple specific
 *
 * @file
 * @brief
 * Used in 'offload' builds. Goal is to increase power saving of the host by waking up
 * the host less. Firmware should:
 *     - respond to ICMP PING requests (up to full MTU packet size)
 * @brief
 * Twiki: [OffloadsPhase2]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <ethernet.h>
#include <802.3.h>
#include <bcmicmp.h>
#include <bcmevent.h>
#include <bcmendian.h>

#include <d11.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wl_export.h>
#include <wlc_types.h>
#include <wl_icmp.h>

/* local module debugging */

static uint32 ip_cksum_partial(uint8 *val8, uint32 count, int j);
static uint16 ip_cksum(uint16 *val16, uint32 count, int j, uint32 sum);
static uint16 incr_chksum(uint16 sum, uint16 m, uint16 m_p);
static int icmp_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static bool icmp_send_echo_reply(wl_icmp_info_t *icmpi, void *sdu, bool snap,
	uint16 iplen, struct ipv4_addr *host_ip);
static bool icmpv6_send_echo_reply(wl_icmp_info_t *icmpi, void *sdu, bool snap, uint16 iplen,
	uint32 psum);
static bool icmp6_verify_address(wl_icmp_info_t *icmpi, struct ipv6_addr *addr);

/* 802.3 llc/snap header */
static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

/** ICMP private info structure */
struct wl_icmp_info {
	wlc_info_t  *wlc;	/* Pointer back to wlc structure */
	bool        icmp_enabled;
	struct ipv4_addr host_ipv4;
	uint32 num_ipv6;
	struct ipv6_addr host_ipv6[MAX_HOST_IPV6];
};

struct ipv6_pseudo_hdr
{
	uint8  saddr[IPV6_ADDR_LEN];
	uint8  daddr[IPV6_ADDR_LEN];
	uint16 payload_len;
	uint16 next_hdr;
};

#define ICMP_IP_BROADCAST 255

/** wlc_pub_t struct access macros */
#define WLCUNITT(icmpi)	((icmpi)->wlc->pub->unit)
#define WLCOSHH(icmpi)	((icmpi)->wlc->osh)

/* IOVar table */
enum {
	IOV_ICMP,	/* Flags for enabling/disabling ICMP offload sub-features */
	IOV_ICMP_HOSTIP,
	IOV_ICMP_HOSTIPV6
};

static const bcm_iovar_t icmp_iovars[] = {
	{"icmp", IOV_ICMP, (0), IOVT_UINT32, 0},
	{"icmp_hostip", IOV_ICMP_HOSTIP, (0), IOVT_UINT32, 0},
	{"icmp_hostipv6", IOV_ICMP_HOSTIPV6, (0), IOVT_BUFFER, WL_ICMP_CFG_IPV6_FIXED_LEN},
	{NULL, 0, 0, 0, 0 }
};

/**
 * Initialize ICMP private context.
 * Returns a pointer to the ICMP private context, NULL on failure.
 */
wl_icmp_info_t *
BCMATTACHFN(wl_icmp_attach)(wlc_info_t *wlc)
{
	wl_icmp_info_t *icmpi;

	/* allocate arp private info struct */
	icmpi = MALLOCZ_PERSIST(wlc->osh, sizeof(wl_icmp_info_t));
	if (!icmpi) {
		WL_ATTACH_ERROR(("wl%d: %s: MALLOCZ failed; total mallocs %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init icmp private info struct */
	icmpi->icmp_enabled = FALSE;
	icmpi->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, icmp_iovars, "icmp", icmpi, icmp_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ATTACH_ERROR(("wl%d: %s wlc_module_register() failed\n",
		                  wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	return icmpi;
}

void
BCMATTACHFN(wl_icmp_detach)(wl_icmp_info_t *icmpi)
{

	WL_INFORM(("wl%d: icmp_detach()\n", WLCUNITT(icmpi)));

	if (!icmpi)
		return;
	wlc_module_unregister(icmpi->wlc->pub, "icmp", icmpi);
	MFREE_PERSIST(WLCOSHH(icmpi), icmpi, sizeof(wl_icmp_info_t));
}

/* Handling ICMP-related iovars */
static int
icmp_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wl_icmp_info_t *icmpi = hdl;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *)a;
	int err = BCME_OK;

	WL_INFORM(("wl%d: icmp_doiovar()\n", WLCUNITT(icmpi)));

	/* donot use this ioctl if the module is not yet enabled */
	if (!ICMP_ENAB(icmpi->wlc->pub)) {
		return BCME_UNSUPPORTED;
	}

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val)) {
		memcpy(&int_val, p, sizeof(int_val));
	}

	/* change arpi if wlcif is corr to a virtualIF */
	if (wlcif != NULL) {
		if (wlcif->wlif != NULL) {
			icmpi = (wl_icmp_info_t *)wl_get_ifctx(icmpi->wlc->wl, IFCTX_ARPI,
				wlcif->wlif);
		}
	}

	switch (actionid) {
		case IOV_SVAL(IOV_ICMP):
			if (int_val < 0) {
				return BCME_BADARG;
			}
			icmpi->icmp_enabled = (int_val != 0) ? TRUE : FALSE;
			break;

		case IOV_GVAL(IOV_ICMP):
			*ret_int_ptr = icmpi->icmp_enabled ? 1 : 0;
			break;

		case IOV_SVAL(IOV_ICMP_HOSTIP):
			/* Add host IP address */
			if (plen < IPV4_ADDR_LEN) {
				return BCME_BUFTOOSHORT;
			}
			memcpy(icmpi->host_ipv4.addr, a, IPV4_ADDR_LEN);
			icmpi->icmp_enabled = (int_val != 0) ? TRUE : FALSE;
			break;
		case IOV_GVAL(IOV_ICMP_HOSTIP):
		{
			uint8 *hostip = (uint8 *)a;

			if (alen < (IPV4_ADDR_LEN * 2)) {
				return BCME_BUFTOOSHORT;
			}
			memcpy(hostip, icmpi->host_ipv4.addr, IPV4_ADDR_LEN);
			hostip += IPV4_ADDR_LEN;
			alen -= sizeof(struct ipv4_addr);
			if (alen < sizeof(struct ipv4_addr))
				return BCME_BUFTOOSHORT;
			memset(hostip, 0, IPV4_ADDR_LEN);
			break;
		}
		case IOV_SVAL(IOV_ICMP_HOSTIPV6):
		{
			wl_icmp_ipv6_cfg_t *cfg = (wl_icmp_ipv6_cfg_t *)a;
			struct ipv6_addr *ipv6;
			int32 i, k;

			if (cfg->version != WL_ICMP_IPV6_CFG_VERSION) {
				return BCME_VERSION;
			}
			/* CLEAR ALL if flag is set */
			if (cfg->flags & WL_ICMP_IPV6_CLEAR_ALL) {
				memset(icmpi->host_ipv6, 0, MAX_HOST_IPV6 * IPV6_ADDR_LEN);
				icmpi->num_ipv6 = 0;
			}
			if (!cfg->num_ipv6) {
				break;
			}
			/* Check length for SET */
			if ((cfg->length != WL_ICMP_CFG_IPV6_LEN(cfg->num_ipv6)) ||
				(alen < cfg->length) ||
				(cfg->fixed_length != WL_ICMP_CFG_IPV6_FIXED_LEN)) {
				return BCME_BADARG;
			}
			/* Check if space is available, it is all or nothing */
			if ((cfg->num_ipv6 + icmpi->num_ipv6) > MAX_HOST_IPV6) {
				return BCME_NORESOURCE;
			}
			/* Check if the ip is already in the table and if not place
			 * it in the first empty slot. Currently selective clear
			 * of ipv6 addresses is not allowed
			 */
			for (k = 0; k < cfg->num_ipv6; k++) {
				ipv6 = &cfg->host_ipv6[k];
				for (i = 0; i < icmpi->num_ipv6; i++) {
					if (!memcmp(icmpi->host_ipv6[i].addr,
						ipv6->addr, IPV6_ADDR_LEN)) {
						break;
					}
				}
				/* If no match found, add it */
				if (i == icmpi->num_ipv6) {
					memcpy(icmpi->host_ipv6[icmpi->num_ipv6].addr,
						ipv6->addr, IPV6_ADDR_LEN);
					icmpi->num_ipv6++;
				}
			}
		}
		break;
		case IOV_GVAL(IOV_ICMP_HOSTIPV6):
		{
			wl_icmp_ipv6_cfg_t *cfg = (wl_icmp_ipv6_cfg_t *)a;

			/* Check buffer length */
			if ((alen < WL_ICMP_CFG_IPV6_LEN(icmpi->num_ipv6))) {
				return BCME_BUFTOOSHORT;
			}
			cfg->version = WL_ICMP_IPV6_CFG_VERSION;
			cfg->fixed_length = WL_ICMP_CFG_IPV6_FIXED_LEN;
			cfg->length = WL_ICMP_CFG_IPV6_LEN(icmpi->num_ipv6);
			cfg->flags = 0;
			cfg->num_ipv6 = icmpi->num_ipv6;
			memcpy((uint8 *)cfg->host_ipv6,
				(uint8 *)icmpi->host_ipv6,
				(icmpi->num_ipv6 * sizeof(struct ipv6_addr)));

		}
		break;
		default:
			err = BCME_UNSUPPORTED;
			break;
	}

	return err;
}

static uint32
ip_cksum_partial(uint8 *val8, uint32 count, int j)
{
	uint32	sum, i;
	uint16	*val16;

	sum = 0;
	val16 = (uint16 *) val8;
	count /= 2;
	for (i = 0; i < count; i++) {
		if (j)
			sum += *val16++;
		else
			sum += ntoh16(*val16++);
	}
	return (sum);
}

static uint16
ip_cksum(uint16 *val16, uint32 count, int j, uint32 sum)
{
	while (count > 1) {
		if (j)
			sum += ntoh16(*val16++);
		else
			sum += *val16++;
		count -= 2;
	}
	/*  Add left-over byte, if any */
	if (count > 0)
		sum += (*(uint8 *)val16);

	/*  Fold 32-bit sum to 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ((uint16)~sum);
}

/* calculate RFC1624 16 bits incremental checksum
	sum = ~(~sum + ~m + m');
 */
static uint16
incr_chksum(uint16 sum, uint16 m, uint16 m_p)
{
	uint32 a_sum = 0;
	uint16 new_sum = 0;
	a_sum = ((~sum & 0xffff) + (~m & 0xffff)) + (m_p & 0xffff);
	/* compensate for overflows */
	a_sum = (a_sum >> 16) + (a_sum & 0xFFFF);
	new_sum = (uint16)a_sum;
	return (~new_sum);
}

static bool
icmp6_verify_address(wl_icmp_info_t *icmpi, struct ipv6_addr *pkt_addr)
{
	int i;

	for (i = 0; i < icmpi->num_ipv6; i++) {
		if (!memcmp(icmpi->host_ipv6[i].addr, pkt_addr->addr, IPV6_ADDR_LEN)) {
			return TRUE;
		}
	}
	/* No match found */
	return FALSE;
}

static bool
icmp_send_echo_reply(wl_icmp_info_t *icmpi, void *sdu, bool snap, uint16 iplen,
	struct ipv4_addr *host_ip)
{
	void *pkt;
	struct ipv4_hdr *ipv4, *ipv4_from;
	struct bcmicmp_hdr *icmp_pkt;
	uint8 *frame;
	uint8 *sdu_frame = PKTDATA(WLCOSHH(icmpi), sdu);
	uint16 eth_pktlen;
	uint16 pktlen;
	wlc_info_t *wlc = icmpi->wlc;

	eth_pktlen = (ETHER_HDR_LEN + ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	pktlen = eth_pktlen + iplen;

	if (!(pkt = PKTGET(WLCOSHH(icmpi), pktlen, TRUE))) {
		WL_ERROR(("icmp_send_echo_reply: PKTGET failed\n"));
		return FALSE;
	}

	frame = PKTDATA(WLCOSHH(icmpi), pkt);
	ipv4 = (struct ipv4_hdr *)(frame + eth_pktlen);
	ipv4_from = (struct ipv4_hdr *)(sdu_frame + eth_pktlen);
	/* note we use both ipv4 and ipv4_from to get length, could do it differently but.. */
	icmp_pkt = (struct bcmicmp_hdr *) ((uint8 *)ipv4 + IPV4_HLEN(ipv4_from));

	/* always copy whole packet to pick up fields we may not change: ie: snap */
	memcpy(frame, sdu_frame, pktlen); 	/* copy original pkt and then muck fields */
	/* switch MAC addrs */
	memcpy(frame + ETHER_DEST_OFFSET, sdu_frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);
	memcpy(frame + ETHER_SRC_OFFSET, sdu_frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);

	/* now fix up IP portion */
	memcpy(ipv4->dst_ip, ipv4_from->src_ip, IPV4_ADDR_LEN);
	WL_TRACE(("%s dst ip %d %d %d %d host ip %d %d %d %d\n", __FUNCTION__,
		ipv4_from->dst_ip[0], ipv4_from->dst_ip[1], ipv4_from->dst_ip[2],
		ipv4_from->dst_ip[3], host_ip->addr[0], host_ip->addr[1], host_ip->addr[2],
		host_ip->addr[3]));
	/* when arpi is enabled OS sets host_ip = ipv4_from->dst_ip (except for when
	   ICMP frame contains ipv4_from->dst_ip[3] = 255) so ICMP should only work
	   when arpi is enabled otherwise driver should drop the packets
	*/
	if ((host_ip != NULL) &&
	    (ipv4_from->dst_ip[0] == host_ip->addr[0]) &&
	    (ipv4_from->dst_ip[1] == host_ip->addr[1]) &&
	    (ipv4_from->dst_ip[2] == host_ip->addr[2])) {
		if (ipv4_from->dst_ip[3] == ICMP_IP_BROADCAST) {
			memcpy(ipv4->src_ip, host_ip, IPV4_ADDR_LEN);
		} else {
			memcpy(ipv4->src_ip, ipv4_from->dst_ip, IPV4_ADDR_LEN);
		}
	} else {
		PKTFREE(wlc->osh, pkt, FALSE);
		return FALSE;
	}

	ipv4->ttl = IP_DEFAULT_TTL;
	ipv4->hdr_chksum = 0;
	ipv4->hdr_chksum = ip_cksum((uint16 *)ipv4, IPV4_HLEN(ipv4), 0, 0);

	if (!(ntoh16(ipv4_from->frag) & IPV4_FRAG_OFFSET_MASK)) {
		icmp_pkt->type = ICMP_TYPE_ECHO_REPLY;
		icmp_pkt->chksum = incr_chksum(icmp_pkt->chksum, ICMP_TYPE_ECHO_REQUEST,
		                               ICMP_TYPE_ECHO_REPLY);
	}

	/* packet has been updated so go send */
	printf("%s: ICMP4 ECHO_REQUEST: send\n", __FUNCTION__);
	wlc_sendpkt(icmpi->wlc, pkt, NULL);

	return TRUE;
}

static bool
icmpv6_send_echo_reply(wl_icmp_info_t *icmpi, void *sdu, bool snap, uint16 iplen, uint32 psum)
{
	void *pkt;
	uint8 *frame;
	uint8 *sdu_frame = PKTDATA(WLCOSHH(icmpi), sdu);
	uint16 eth_pktlen;
	uint16 pktlen;
	struct ipv6_hdr *ipv6, *ipv6_from;
	struct icmp6_hdr *icmpv6;

	eth_pktlen = (ETHER_HDR_LEN + ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	pktlen = eth_pktlen + iplen;
	ipv6_from = (struct ipv6_hdr *)(sdu_frame + eth_pktlen);

	/* Verify ipv6 address */
	if (!icmp6_verify_address(icmpi, &ipv6_from->daddr)) {
		WL_TRACE(("%s: HOST ipv6 does not match pkt DADDR\n", __FUNCTION__));
		return FALSE;
	}
	if (!(pkt = PKTGET(WLCOSHH(icmpi), pktlen, TRUE))) {
		WL_ERROR(("%s:icmpv6_send_echo_reply: PKTGET failed\n", __FUNCTION__));
		return FALSE;
	}

	frame = PKTDATA(WLCOSHH(icmpi), pkt);
	ipv6 = (struct ipv6_hdr *)(frame + eth_pktlen);
	icmpv6 = (struct icmp6_hdr *) ((uint8 *)ipv6 + sizeof(struct ipv6_hdr));

	/* always copy whole packet to pick up fields we may not change: ie: snap */
	memcpy(frame, sdu_frame, pktlen); 	/* copy original pkt and then muck fields */
	/* switch MAC addrs */
	memcpy(frame + ETHER_DEST_OFFSET, sdu_frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);
	memcpy(frame + ETHER_SRC_OFFSET, sdu_frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);

	/* now fix up IPv6 portion */
	memcpy(ipv6->daddr.addr, ipv6_from->saddr.addr, IPV6_ADDR_LEN);
	memcpy(ipv6->saddr.addr, ipv6_from->daddr.addr, IPV6_ADDR_LEN);
	ipv6->hop_limit = IP_DEFAULT_TTL;

	/* now fix up ICMP portion */
	icmpv6->icmp6_type = ICMP6_ECHO_REPLY;
	icmpv6->icmp6_cksum = 0;
	icmpv6->icmp6_cksum = ip_cksum((uint16 *)icmpv6, ntoh16(ipv6->payload_len), 0, psum);

	printf("%s: ICMP6 ECHO_REQUEST: send\n", __FUNCTION__);
	/* packet has been updated so go send */
	wlc_sendpkt(icmpi->wlc, pkt, NULL);

	return TRUE;
}

#ifdef ICMP_DEBUG
struct foo {
	uint8   icmp6_type;
	uint8   icmp6_code;
	uint16  icmp6_cksum;
	uint16	id;
	uint16  seq;
};
#endif /* ICMP_DEBUG */

/** Returns -1 if frame is not icmp; otherwise, returns pointer/length of IP portion */
static bool
wl_icmp_parse(wl_icmp_info_t *icmpi, void *sdu, struct ipv4_addr *host_ip)
{
#ifdef ICMP_DEBUG
	struct foo *fooo;
#endif	/* ICMP_DEBUG */
	uint8 *frame = PKTDATA(WLCOSHH(icmpi), sdu);
	int length = PKTLEN(WLCOSHH(icmpi), sdu);
	uint8 *pt;
	uint16 ethertype;
	struct ipv4_hdr *ipv4_in;
	struct bcmicmp_hdr *icmpv4_in;
	struct ipv6_hdr *ipv6_in;
	struct icmp6_hdr *icmpv6_in;
	struct ipv6_pseudo_hdr ipv6_pseudo;
	int iplen;
	bool snap = FALSE;
	uint16 icmp_len;
	uint16 iphdr_cksum, icmp_cksum;
	uint16 cksum;
	uint32 psum;

	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (length < ETHER_HDR_LEN) {
		return FALSE;
	} else if (ntoh16_ua((const void *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
		snap = TRUE;
	} else {
		/* not a snap packet; do nothing */
		snap = FALSE;
		return FALSE;
	}

	ethertype = ntoh16_ua((const void *)pt);

	/* if not any packet type we are interested in just bail out */
	if (ethertype == ETHER_TYPE_IP) {
		ipv4_in = (struct ipv4_hdr *)(pt + ETHER_TYPE_LEN);

		if (ipv4_in->prot != IP_PROT_ICMP) {
			/* not an ICMP packet type; do nothing */
			return FALSE;
		}

		iplen = IPV4_HLEN(ipv4_in);	/* make it a byte count and not a word count */

		/* now we need to check sum both IP and ICMP before we do anything more.
		 * If the IP checksum fails, then just get out of here without accessing the
		 * icmp ptr, as that could lead to trouble.
		 */
		iphdr_cksum = ipv4_in->hdr_chksum;
		/* zero it out before we calculate */
		ipv4_in->hdr_chksum = 0;
		cksum = ip_cksum((uint16 *)ipv4_in, iplen, 0, 0);
		ipv4_in->hdr_chksum = iphdr_cksum;
		if (cksum != iphdr_cksum) {
			WL_ERROR(("%s: bad ipv4 header checksum: %x != %x (cal)\n", __FUNCTION__,
				cksum, ipv4_in->hdr_chksum));
			return FALSE;
		}

		icmpv4_in = (struct bcmicmp_hdr *)(pt + ETHER_TYPE_LEN + iplen);
		/* check cksum for single icmp frame only as check sum calc is based on entire
		 * frame len and frag frames do not have all frame info in 1 frag frame.
		 */
		if (!(ntoh16(ipv4_in->frag) & IPV4_FRAG_OFFSET_MASK) &&
		    !(ntoh16(ipv4_in->frag) & IPV4_FRAG_MORE)) {
			icmp_cksum = icmpv4_in->chksum;
			icmpv4_in->chksum = 0;	/* zero before calculate */
			icmp_len = ntoh16(ipv4_in->tot_len) - iplen;
			cksum = ip_cksum((uint16 *)icmpv4_in, icmp_len, 0, 0);
			icmpv4_in->chksum = icmp_cksum;
			if (cksum != icmp_cksum) {
				WL_ERROR(("%s: bad ICMP packet checksum tot_len %d, icmp_len %d\n",
				          __FUNCTION__, ntoh16(ipv4_in->tot_len), icmp_len));
				return FALSE;
			}
		}

		/* OK packet looks good, so lets process the ICMP message */
		if ((icmpv4_in->type == ICMP_TYPE_ECHO_REQUEST) ||
		    (ntoh16(ipv4_in->frag) & IPV4_FRAG_OFFSET_MASK)) {
			/* seems return value is ignored, is this good */
			return icmp_send_echo_reply(icmpi, sdu, snap,
				ntoh16(ipv4_in->tot_len), host_ip);
		}
		return FALSE;
	} else if (ethertype == ETHER_TYPE_IPV6) {
		ipv6_in = (struct ipv6_hdr *)(pt + ETHER_TYPE_LEN);
		if (ipv6_in->nexthdr != ICMPV6_HEADER_TYPE) {
			WL_ERROR(("%s: BAD ipv6 header type %x\n", __FUNCTION__, ipv6_in->nexthdr));
			return FALSE;
		}
		icmp_len = ntoh16(ipv6_in->payload_len);
		icmpv6_in = (struct icmp6_hdr *)(pt + ETHER_TYPE_LEN + sizeof(struct ipv6_hdr));

		if (icmpv6_in->icmp6_type != ICMP6_ECHO_REQUEST) {
			return FALSE;
		}
		/* we need to calculate the icmpv6 checksum. Note it uses a pseudo hdr, which is
		 * different than its ipv4 counterpart, but there is no ip layer checksum
		 */
		memset((char *)&ipv6_pseudo, 0, sizeof(struct ipv6_pseudo_hdr));
		memcpy((char *)ipv6_pseudo.saddr, (char *)ipv6_in->saddr.addr, IPV6_ADDR_LEN);
		memcpy((char *)ipv6_pseudo.daddr, (char *)ipv6_in->daddr.addr, IPV6_ADDR_LEN);
		ipv6_pseudo.payload_len = ipv6_in->payload_len;
		ipv6_pseudo.next_hdr =  ntoh16(ipv6_in->nexthdr);

		icmp_cksum = icmpv6_in->icmp6_cksum;
		icmpv6_in->icmp6_cksum = 0;
		psum = ip_cksum_partial((uint8 *)&ipv6_pseudo, sizeof(struct ipv6_pseudo_hdr), 1);
		cksum = ip_cksum((uint16 *)icmpv6_in, icmp_len, 0, psum);
		icmpv6_in->icmp6_cksum = icmp_cksum;
#ifdef ICMP_DEBUG
		fooo = (struct foo *)icmpv6_in;
		printf("wl_icmp_parse: ipv6: len=%d type=%x code=%x ck_sum=%x id=%x seq=%x\n",
		       icmp_len, icmpv6_in->icmp6_type, icmpv6_in->icmp6_code,
		       icmpv6_in->icmp6_cksum, fooo->id, fooo->seq);
#endif /* ICMP_DEBUG */
		if (cksum != icmp_cksum) {
			WL_ERROR(("%s: ipv6 BAD cksums(%d): cksum=%x icmp_cksum=%x\n", __FUNCTION__,
			       sizeof(struct ipv6_addr), cksum, icmp_cksum));
			return FALSE;
		}

		return icmpv6_send_echo_reply(icmpi, sdu, snap, icmp_len +
		                              sizeof(struct ipv6_hdr), psum);
	} else if (ethertype == ETHER_TYPE_8021Q) {
		WL_ERROR(("%s: Dropped 802.1Q packet\n", __FUNCTION__));
	}
	return FALSE;
}

#if NOT_YET
void
wl_icmp_event_handler(wl_icmp_info_t *icmpi, uint32 event, void *event_data)
{
	switch (event) {
		case BCM_E_WOWL_COMPLETE:
			WL_ERROR(("%s: %s: Starting ICMP\n",
			          __FUNCTION__, bcm_ol_event_str[event]));
			icmpi->icmp_enabled = 1;
			break;
		case BCM_E_DEAUTH:
		case BCM_E_DISASSOC:
		case BCM_E_BCN_LOSS:
		case BCM_E_PME_ASSERTED:
			if (icmpi->icmp_enabled == 1) {
				WL_ERROR(("%s: %s: Disabling ICMP\n",
				          __FUNCTION__, bcm_ol_event_str[event]));
				icmpi->icmp_enabled = 0;
			}
			break;
		default:
			WL_TRACE(("%s: unsupported event: %s\n", __FUNCTION__,
			          bcm_ol_event_str[event]));
			break;
	}
}
#endif /* NOT_YET */

/* returns TRUE if the packet is consumed else FALSE */
bool
wl_icmp_rx(wl_icmp_info_t *icmpi, void *sdu)
{
	ASSERT(icmpi != NULL);
	if (IPV4_ADDR_NULL(icmpi->host_ipv4.addr)) {
		return FALSE;
	}
	if (!icmpi->icmp_enabled) {
		return FALSE;
	}

	/* See if this is an ICMP packet */
	return wl_icmp_parse(icmpi, sdu, &icmpi->host_ipv4);
}

bool
wl_icmp_is_running(wl_icmp_info_t *icmpi)
{
	ASSERT(icmpi != NULL);
	return (icmpi->icmp_enabled);
}
