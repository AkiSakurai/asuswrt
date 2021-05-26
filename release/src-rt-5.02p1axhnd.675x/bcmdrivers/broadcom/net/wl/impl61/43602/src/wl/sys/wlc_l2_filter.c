/*
 * L2 filter source file
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id:$
 */
#ifndef L2_FILTER
#error "L2_FILTER is not defined"
#endif	/* L2_FILTER */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/802.3.h>
#include <proto/vlan.h>
#include <proto/bcmarp.h>
#include <proto/bcmip.h>
#include <proto/bcmipv6.h>
#include <proto/bcmicmp.h>
#include <proto/bcmudp.h>
#include <proto/bcmdhcp.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_l2_filter.h>
#include <wlc_tdls.h>
#include <wlc_scb.h>
#include <wl_export.h>

#define DEBUG_PROXY_ARP

/* The length of the option including the type and length fields in units of 8 octets */
#define ND_OPTION_LEN_ETHER	1
#define ALIGN_ADJ_BUFLEN		2		/* Adjust for ETHER_HDR_LEN pull in linux
							 * which makes pkt nonaligned
							 */

/* iovar table */
enum {
	IOV_GRAT_ARP_ENABLE,	/* drop gratuitous ARP */
	IOV_BLOCK_PING,	/* drop ping Echo request packets for RX */
	IOV_BLOCK_TDLS,	/* drop TDLS discovery/setup request packets for RX */
	IOV_BLOCK_STA,		/* drop STA to STA packets for TX */
	IOV_BLOCK_MULTICAST,	/* drop multicast packets for TX */
	IOV_DHCP_UNICAST,	/* dhcp response broadcast to unicast conversion for TX */
	IOV_PROXY_ARP,	/* proxy ARP */
	IOV_GTK_PER_STA,	/* unique GTK per STA */
	IOV_LAST
	};

static const bcm_iovar_t l2_filter_iovars[] = {
	{"grat_arp", IOV_GRAT_ARP_ENABLE, 0, IOVT_BOOL, 0},
	{"block_ping", IOV_BLOCK_PING, 0, IOVT_BOOL, 0},
	{"block_tdls", IOV_BLOCK_TDLS, 0, IOVT_BOOL, 0},
	{"block_sta", IOV_BLOCK_STA, 0, IOVT_BOOL, 0},
	{"block_multicast", IOV_BLOCK_MULTICAST, 0, IOVT_BOOL, 0},
	{"dhcp_unicast", IOV_DHCP_UNICAST, 0, IOVT_BOOL, 0},
	{"proxy_arp", IOV_PROXY_ARP, 0, IOVT_BOOL, 0},
	{"gtk_per_sta", IOV_GTK_PER_STA, 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};

/* L2 filter module specific state */
struct l2_filter_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	wlc_pub_t *pub;
	osl_t *osh;
	int	scb_handle;		/* scb cubby handle to retrieve data from scb */
	bool grat_arp_enable;
	bool block_ping;
	bool block_tdls;
	bool block_sta;
	bool block_multicast;
	bool dhcp_unicast;
	bool proxy_arp;
	bool gtk_per_sta;
};

struct proxy_arp_scb_cubby {
	struct ipv4_addr	ipv4;
	struct ipv6_addr	ipv6_local;
	struct ipv6_addr	ipv6_global;
};

#define SCB_PROXY_ARP(l2_filter, scb) (SCB_CUBBY((scb), (l2_filter)->scb_handle))

static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

/* local prototypes */
static int wlc_l2_filter_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *p, uint plen, void *a, int alen, int vsize,
	struct wlc_if *wlcif);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

l2_filter_info_t *
BCMATTACHFN(wlc_l2_filter_attach)(wlc_info_t *wlc)
{
	l2_filter_info_t *l2_filter;

	if (!(l2_filter = (l2_filter_info_t *)MALLOC(wlc->osh, sizeof(l2_filter_info_t)))) {
		WL_ERROR(("wl%d: wlc_l2_filter_attach: out of mem, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)l2_filter, sizeof(l2_filter_info_t));

	l2_filter->wlc = wlc;
	l2_filter->pub = wlc->pub;
	l2_filter->osh = wlc->pub->osh;

	/* reserve cubby in the scb container for per-scb private data */
	l2_filter->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(struct proxy_arp_scb_cubby),
	                                        NULL,
	                                        NULL,
	                                        NULL,
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
	                                        (void *)l2_filter, 0);
#else
						(void *)l2_filter);
#endif // endif
	if (l2_filter->scb_handle < 0) {
		WL_ERROR(("wl%d: %s:wlc_scb_cubby_reserve failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, l2_filter_iovars, "l2_filter",
		l2_filter, wlc_l2_filter_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: L2 filter wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	return l2_filter;

fail:
	MFREE(wlc->osh, l2_filter, sizeof(l2_filter_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_l2_filter_detach)(l2_filter_info_t *l2_filter)
{
	if (!l2_filter)
		return;

	/* sanity */
	wlc_module_unregister(l2_filter->pub, "l2_filter", l2_filter);

	MFREE(l2_filter->wlc->osh, l2_filter, sizeof(l2_filter_info_t));
}

/* handle L2 filter related iovars */
static int
wlc_l2_filter_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	l2_filter_info_t *l2_filter = (l2_filter_info_t *)hdl;
	wlc_info_t *wlc = l2_filter->wlc;
	wlc_bsscfg_t *bsscfg;
	int32 int_val = 0;
	bool bool_val;
	uint32 *ret_uint_ptr;
	int err = 0;

	ASSERT(l2_filter == wlc->l2_filter);

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_uint_ptr = (uint32 *)a;

	switch (actionid) {

	case IOV_GVAL(IOV_GRAT_ARP_ENABLE):
		*ret_uint_ptr = l2_filter->grat_arp_enable;
		break;

	case IOV_SVAL(IOV_GRAT_ARP_ENABLE):
		l2_filter->grat_arp_enable = bool_val;
		break;

	case IOV_GVAL(IOV_BLOCK_PING):
		*ret_uint_ptr = l2_filter->block_ping;
		break;

	case IOV_SVAL(IOV_BLOCK_PING):
		l2_filter->block_ping = bool_val;
		break;

	case IOV_GVAL(IOV_BLOCK_TDLS):
		*ret_uint_ptr = l2_filter->block_tdls;
		break;

	case IOV_SVAL(IOV_BLOCK_TDLS):
		l2_filter->block_tdls = bool_val;
		break;

	case IOV_GVAL(IOV_BLOCK_STA):
		*ret_uint_ptr = l2_filter->block_sta;
		break;

	case IOV_SVAL(IOV_BLOCK_STA):
		l2_filter->block_sta = bool_val;
		break;

	case IOV_GVAL(IOV_BLOCK_MULTICAST):
		*ret_uint_ptr = l2_filter->block_multicast;
		break;

	case IOV_SVAL(IOV_BLOCK_MULTICAST):
		l2_filter->block_multicast = bool_val;
		break;

	case IOV_GVAL(IOV_DHCP_UNICAST):
		*ret_uint_ptr = l2_filter->dhcp_unicast;
		break;

	case IOV_SVAL(IOV_DHCP_UNICAST):
		l2_filter->dhcp_unicast = bool_val;
		break;

	case IOV_GVAL(IOV_PROXY_ARP):
		*ret_uint_ptr = l2_filter->proxy_arp;
		break;

	case IOV_SVAL(IOV_PROXY_ARP):
		if (l2_filter->proxy_arp != bool_val) {
			wlc_bsscfg_t *cfg;
			int idx;
			l2_filter->proxy_arp = bool_val;

			FOREACH_BSS(wlc, idx, cfg) {
				/* update extend capabilities */
				wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_PROXY_ARP,
					(bool_val && BSSCFG_AP(cfg)));
				if (cfg->up &&
				    (BSSCFG_AP(cfg) ||
				    (!cfg->BSS && !BSS_TDLS_ENAB(wlc, cfg)))) {
					/* update AP or IBSS beacons */
					wlc_bss_update_beacon(wlc, cfg);
					/* update AP or IBSS probe responses */
					wlc_bss_update_probe_resp(wlc, cfg, TRUE);
				}
			}
		}

		break;
	case IOV_GVAL(IOV_GTK_PER_STA):
		*ret_uint_ptr = l2_filter->gtk_per_sta;
		break;

	case IOV_SVAL(IOV_GTK_PER_STA): {
		eapol_hdr_t *eapolh;
		struct ether_addr ea;
		uint8 eapol[EAPOL_HDR_LEN+1];
		l2_filter->gtk_per_sta = bool_val;
		bzero(&ea, ETHER_ADDR_LEN);
		eapolh = (eapol_hdr_t *)eapol;
		eapolh->version = WPA2_EAPOL_VERSION;
		eapolh->type = 0xFF;
		eapolh->length = HTON16(1);
		eapol[EAPOL_HDR_LEN] = (uint8)bool_val;
		/* notify NAS about unique gtk per STA */
		wlc_bss_eapol_event(wlc, bsscfg, &ea, eapol, EAPOL_HDR_LEN+1);
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

static int
wlc_l2_filter_get_ether_type(wlc_info_t *wlc, void *sdu,
	uint8 **data_ptr, int *len_ptr, uint16 *et_ptr, bool *snap_ptr)
{
	uint8 *frame = PKTDATA(wlc->osh, sdu);
	int length = PKTLEN(wlc->osh, sdu);
	uint8 *pt;			/* Pointer to type field */
	uint16 ethertype;
	bool snap = FALSE;
	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (length < ETHER_HDR_LEN) {
		WL_ERROR(("wl%d: %s: short eth frame (%d)\n",
		          wlc->pub->unit, __FUNCTION__, length));
		return -1;
	} else if (ntoh16_ua((const void *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		WL_INFORM(("wl%d: %s: 802.3 LLC/SNAP\n", wlc->pub->unit, __FUNCTION__));
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
		snap = TRUE;
	} else {
		WL_INFORM(("wl%d: %s: non-SNAP 802.3 frame\n",
		          wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	ethertype = ntoh16_ua((const void *)pt);

	/* Skip VLAN tag, if any */
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			WL_ERROR(("wl%d: %s: short VLAN frame (%d)\n",
			          wlc->pub->unit, __FUNCTION__, length));
			return -1;
		}

		ethertype = ntoh16_ua((const void *)pt);
	}

	*data_ptr = pt + ETHER_TYPE_LEN;
	*len_ptr = length - (pt + ETHER_TYPE_LEN - frame);
	*et_ptr = ethertype;
	*snap_ptr = snap;
	return 0;
}

static int
wlc_l2_filter_get_ip_type(wlc_info_t *wlc, void *sdu,
	uint8 **data_ptr, int *len_ptr, uint8 *prot_ptr)
{
	struct ipv4_hdr *iph;		/* IP frame pointer */
	int ipl;			/* IP frame length */
	uint16 ethertype, ihl, ippktlen;
	uint16 iph_frag;
	uint8 prot;
	bool snap;

	if (wlc_l2_filter_get_ether_type(wlc, sdu, (uint8 **)&iph, &ipl, &ethertype, &snap) != 0)
		return -1;

	if (ethertype != ETHER_TYPE_IP) {
		return -1;
	}

	/* We support IPv4 only */
	if (ipl < IPV4_OPTIONS_OFFSET || (IP_VER(iph) != IP_VER_4)) {
		return -1;
	}

	/* Header length sanity */
	ihl = IPV4_HLEN(iph);

	/*
	 * Packet length sanity; sometimes we receive eth-frame size bigger
	 * than the IP content, which results in a bad tcp chksum
	 */
	ippktlen = ntoh16(iph->tot_len);
	if (ippktlen < ipl) {
		WL_INFORM(("wl%d: %s: extra frame length ignored (%d)\n",
		          wlc->pub->unit, __FUNCTION__, ipl - ippktlen));
		ipl = ippktlen;
	} else if (ippktlen > ipl) {
		WL_ERROR(("wl%d: %s: truncated IP packet (%d)\n",
		          wlc->pub->unit, __FUNCTION__, ippktlen - ipl));
		return -1;
	}

	if (ihl < IPV4_OPTIONS_OFFSET || ihl > ipl) {
		WL_ERROR(("wl%d: %s: IP-header-len (%d) out of range (%d-%d)\n",
		          wlc->pub->unit, __FUNCTION__, ihl, IPV4_OPTIONS_OFFSET, ipl));
		return -1;
	}

	/*
	 * We don't handle fragmented IP packets.  A first frag is indicated by the MF
	 * (more frag) bit and a subsequent frag is indicated by a non-zero frag offset.
	 */
	iph_frag = ntoh16(iph->frag);

	if ((iph_frag & IPV4_FRAG_MORE) || (iph_frag & IPV4_FRAG_OFFSET_MASK) != 0) {
		WL_INFORM(("wl%d: IP fragment not handled\n",
		           wlc->pub->unit));
		return -1;
	}

	prot = IPV4_PROT(iph);

	*data_ptr = (((uint8 *)iph) + ihl);
	*len_ptr = ipl - ihl;
	*prot_ptr = prot;
	return 0;
}

/* returns 0 if gratuitous ARP or unsolicited neighbour advertisement */
static int
wl_gratuitous_arp(wlc_info_t *wlc, void *sdu)
{
	uint8 *frame = PKTDATA(wlc->osh, sdu);
	uint16 ethertype;
#ifdef WL_MSGBUF
	char ipbuf[32], eabuf[32];
#endif // endif
	int send_ip_offset, target_ip_offset;
	int iplen;
	int minlen;
	uint8 *data;
	int datalen;
	bool snap;

	if (wlc_l2_filter_get_ether_type(wlc, sdu, &data, &datalen, &ethertype, &snap) != 0)
		return -1;

	if (!ETHER_ISBCAST(frame + ETHER_DEST_OFFSET) &&
		bcmp(&ether_ipv6_mcast, frame + ETHER_DEST_OFFSET, sizeof(ether_ipv6_mcast))) {
		return -1;
	}

	if (ethertype == ETHER_TYPE_ARP) {
		WL_PRUSR("ARP RX", data, datalen);
		send_ip_offset = ARP_SRC_IP_OFFSET;
		target_ip_offset = ARP_TGT_IP_OFFSET;
		iplen = IPV4_ADDR_LEN;
		minlen = ARP_DATA_LEN;
	}
	else if (ethertype == ETHER_TYPE_IPV6) {
		WL_PRUSR("Neighbour advertisement RX", data, datalen);
		send_ip_offset = NEIGHBOR_ADVERTISE_SRC_IPV6_OFFSET;
		target_ip_offset = NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET;
		iplen = IPV6_ADDR_LEN;
		minlen = target_ip_offset + iplen;

		/* check for neighbour advertisement */
		if (datalen >= minlen && (data[IPV6_NEXT_HDR_OFFSET] != IP_PROT_ICMP6 ||
			data[NEIGHBOR_ADVERTISE_TYPE_OFFSET] != NEIGHBOR_ADVERTISE_TYPE))
			return -1;
	}
	else {
		return -1;
	}

	if (datalen < minlen) {
		WL_ERROR(("wl%d: wl_gratuitous_arp: truncated packet (%d)\n",
		          wlc->pub->unit, datalen));
		return -1;
	}

	if (bcmp(data + send_ip_offset, data + target_ip_offset, iplen) == 0) {
		WL_PRUSR("gratuitous ARP or unsolicitated neighbour advertisement rx'ed",
			data, datalen);
		return 0;
	}

	return -1;
}

static int
wlc_l2_filter_ping(wlc_info_t *wlc, void *sdu)
{
	struct bcmicmp_hdr *icmph;
	int icmpl;
	uint8 prot;

	if (wlc_l2_filter_get_ip_type(wlc, sdu, (uint8 **)&icmph, &icmpl, &prot) != 0)
		return -1;

	if (prot == IP_PROT_ICMP) {
		WL_L2FILTER(("wl%d: recv ICMP %d\n",
		           wlc->pub->unit, icmph->type));

		if (icmph->type == ICMP_TYPE_ECHO_REQUEST)
			return 0;
	}

	return -1;
}

static int
wlc_l2_filter_tdls(wlc_info_t *wlc, void *sdu)
{
	uint8 *pdata;
	int datalen;
	uint16 ethertype;
	uint8 action_field;
	bool snap;

	if (wlc_l2_filter_get_ether_type(wlc, sdu, &pdata, &datalen, &ethertype, &snap) != 0)
		return -1;

	if (ethertype != ETHER_TYPE_89_0D)
		return -1;

	/* validate payload type */
	if (datalen < TDLS_PAYLOAD_TYPE_LEN + 2) {
		WL_ERROR(("wl%d:%s: wrong length for 89-0d eth frame %d\n",
			wlc->pub->unit, __FUNCTION__, datalen));
		return -1;
	}

	/* validate payload type */
	if (*pdata != TDLS_PAYLOAD_TYPE) {
		WL_ERROR(("wl%d:%s: wrong payload type for 89-0d eth frame %d\n",
			wlc->pub->unit, __FUNCTION__, *pdata));
		return -1;
	}
	pdata += TDLS_PAYLOAD_TYPE_LEN;

	/* validate TDLS action category */
	if (*pdata != TDLS_ACTION_CATEGORY_CODE) {
		WL_ERROR(("wl%d:%s: wrong TDLS Category %d\n", wlc->pub->unit,
			__FUNCTION__, *pdata));
		return -1;
	}
	pdata++;

	action_field = *pdata;

	if ((action_field == TDLS_SETUP_REQ) || (action_field == TDLS_DISCOVERY_REQ))
		return 0;

	return -1;
}

static int
wlc_l2_filter_sta(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	struct ether_header *eh;
	struct scb *src;
	eh = (struct ether_header*) PKTDATA(wlc->osh, sdu);

	src = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)eh->ether_shost);
	if (src && SCB_ASSOCIATED(src) && (src->bsscfg == bsscfg))
		return 0;

	return -1;
}

static int
wlc_l2_filter_dhcp_unicast(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	struct scb *client;
	uint8 *eh = PKTDATA(wlc->osh, sdu);
	uint8 *udph;
	uint8 *dhcp;
	uint8 *chaddr;
	int udpl;
	int dhcpl;
	uint16 port;
	uint8 prot;

	if (!ETHER_ISMULTI(eh + ETHER_DEST_OFFSET))
		return -1;

	if (wlc_l2_filter_get_ip_type(wlc, sdu, &udph, &udpl, &prot) != 0)
		return -1;

	if (prot != IP_PROT_UDP)
		return -1;

	/* check frame length, at least UDP_HDR_LEN */
	if (udpl < UDP_HDR_LEN) {
		WL_ERROR(("wl%d: %s: short UDP frame, ignored\n",
			wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	port = ntoh16_ua((const void *)(udph + UDP_DEST_PORT_OFFSET));
	/* only process DHCP packets from server to client */
	if (port != DHCP_PORT_CLIENT)
		return -1;

	dhcp = udph + UDP_HDR_LEN;
	dhcpl = udpl - UDP_HDR_LEN;

	if (dhcpl < DHCP_CHADDR_OFFSET + ETHER_ADDR_LEN) {
		WL_ERROR(("wl%d: %s: short DHCP frame, ignored\n",
			wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	/* only process DHCP reply(offer/ack) packets */
	if (*(dhcp + DHCP_TYPE_OFFSET) != DHCP_TYPE_REPLY)
		return -1;

	chaddr = dhcp + DHCP_CHADDR_OFFSET;
	client = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)chaddr);

	if (client && SCB_ASSOCIATED(client)) {
		/* replace the Ethernet destination MAC with unicast client HW address */
		bcopy(chaddr, eh + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	}

	return -1;
}

static int
wlc_l2_filter_multicast(wlc_info_t *wlc, void *sdu)
{
	struct ether_header *eh;
	eh = (struct ether_header*) PKTDATA(wlc->osh, sdu);

	if (ETHER_ISMULTI(eh->ether_dhost)) {
		return 0;
	}

	return -1;
}

static bool
null_ip_addr(uint8 *ipa, bool ipv4)
{
	int i;
	int len = ipv4 ? IPV4_ADDR_LEN : IPV6_ADDR_LEN;
	for (i = 0; i < len; i++) {
		if (ipa[i])
			return FALSE;
	}
	return TRUE;
}

static bool
local_ipv6_addr(uint8 *ipa)
{
	return (ipa[0] == 0xFE) && ((ipa[1] & 0x80) != 0);
}

/* The length of the option including the type and length fields in units of 8 octets */
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

#ifdef DEBUG_PROXY_ARP
static void
wlc_l2_filter_dump_packet(uint8 *data, uint32 len)
{
	WL_L2FILTER(("char data[%d] = {\n", len));

	while (len > 7) {
		WL_L2FILTER(("0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,\n",
			data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]));
		data += 8;
		len -= 8;
	}

	while (len) {
		WL_L2FILTER(("0x%x,", data[0]));
		data ++;
		len --;
	}

	WL_L2FILTER(("};\n"));
}
#endif /* DEBUG_PROXY_ARP */

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

static int
wlc_l2_filter_save_ip_addr(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *ipa, uint8 *ea, bool ipv4)
{
	struct scb *scb;
	struct proxy_arp_scb_cubby *scb_proxyarp;
	scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)ea);
	if (scb) {
		scb_proxyarp = (struct proxy_arp_scb_cubby *)SCB_PROXY_ARP(wlc->l2_filter, scb);
		/* update STA IP address */
		if (ipv4)
			bcopy(ipa, scb_proxyarp->ipv4.addr, IPV4_ADDR_LEN);
		else {
			if (local_ipv6_addr(ipa))
				bcopy(ipa, scb_proxyarp->ipv6_local.addr, IPV6_ADDR_LEN);
			else
				bcopy(ipa, scb_proxyarp->ipv6_global.addr, IPV6_ADDR_LEN);
		}
		WL_L2FILTER(("wl%d: %s: ip %d.%d.%d.%d, eth 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
			wlc->pub->unit, __FUNCTION__,
			ipa[0], ipa[1], ipa[2], ipa[3],
			ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]));
		return 0;
	}
	return -1;
}

static int
wlc_l2_filter_arp_reply(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct bcmarp *arp_req, bool snap, bool send)
{
	struct scb *scb;
	struct scb_iter scbiter;
	struct proxy_arp_scb_cubby *scb_proxyarp;
	struct ether_addr *dst_mac = NULL;
	void *pkt;
	uint8 *frame;
	struct bcmarp *arp_reply;
	uint16 pktlen = (ETHER_HDR_LEN +
	                 ARP_DATA_LEN +
	                 ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	uint16 buflen;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_proxyarp = (struct proxy_arp_scb_cubby *)SCB_PROXY_ARP(wlc->l2_filter, scb);
		if (bcmp(scb_proxyarp->ipv4.addr, arp_req->dst_ip, IPV4_ADDR_LEN) == 0) {
			/* find ip address matched scb */
			dst_mac = &(scb->ea);
			break;
		}
	}

	if (!dst_mac) {
		if (send)
			return 0;
		else
			return -1;
	}

	if (send)
		buflen = pktlen + ALIGN_ADJ_BUFLEN;
	else
		buflen = pktlen;

	if (!(pkt = PKTGET(wlc->osh, buflen, (!send)))) {
		WL_ERROR(("wl%d: %s: alloc failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	if (send) {
		/*
		 * Adjust for pkt alignment problems when pkt is pulled by
		 * 14bytes of ETHER_HDR_LEN in linux osl
		 */
		PKTPULL(wlc->osh, pkt, ALIGN_ADJ_BUFLEN);
	}

	frame = PKTDATA(wlc->osh, pkt);

	/* Create 14-byte eth header, plus snap header if applicable */
	bcopy(arp_req->src_eth, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	bcopy(dst_mac, frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);
	if (snap) {
		hton16_ua_store(pktlen, frame + ETHER_TYPE_OFFSET);
		bcopy(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN);
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_HDR_LEN + SNAP_HDR_LEN);
	} else
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_TYPE_OFFSET);

	/* move past eth/eth-snap header */
	arp_reply = (struct bcmarp *)(frame + pktlen - ARP_DATA_LEN);

	/* Create 28-byte arp-reply data frame */

	/* copy first 6 bytes as-is; i.e., htype, ptype, hlen, plen */
	bcopy(arp_req, arp_reply, ARP_OPC_OFFSET);
	arp_reply->oper = HTON16(ARP_OPC_REPLY);
	/* Copy dst eth and ip addresses */
	bcopy(dst_mac, arp_reply->src_eth, ETHER_ADDR_LEN);
	bcopy(arp_req->dst_ip, arp_reply->src_ip, IPV4_ADDR_LEN);
	bcopy(arp_req->src_eth, arp_reply->dst_eth, ETHER_ADDR_LEN);
	bcopy(arp_req->src_ip, arp_reply->dst_ip, IPV4_ADDR_LEN);

	WL_L2FILTER(("wl%d: %s: src %d.%d.%d.%d dst %d.%d.%d.%d src eth %x:%x:%x:%x:%x:%x\n",
		wlc->pub->unit, __FUNCTION__,
		arp_reply->src_ip[0], arp_reply->src_ip[1],
		arp_reply->src_ip[2], arp_reply->src_ip[3],
		arp_reply->dst_ip[0], arp_reply->dst_ip[1],
		arp_reply->dst_ip[2], arp_reply->dst_ip[3],
		arp_reply->src_eth[0], arp_reply->src_eth[1], arp_reply->src_eth[2],
		arp_reply->src_eth[3], arp_reply->src_eth[4], arp_reply->src_eth[5]));

	if (send) {
		wlc_sendup(wlc, bsscfg, NULL, pkt);
	} else {
		wlc_sendpkt(wlc, pkt, bsscfg->wlcif);
	}
	return 0;
}

static int
wlc_l2_filter_neighbor_advertise(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	uint8 *data, bool snap, bool send, uint8 *src_mac)
{
	struct scb *scb;
	struct scb_iter scbiter;
	struct proxy_arp_scb_cubby *scb_proxyarp;
	struct ether_addr *dst_mac = NULL;
	void *pkt;
	uint8 *frame;
	uint8 *na;
	uint16 pktlen = (ETHER_HDR_LEN +
	                 NEIGHBOR_ADVERTISE_DATA_LEN +
	                 ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	uint16 buflen;
	uint16 checksum;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_proxyarp = (struct proxy_arp_scb_cubby *)SCB_PROXY_ARP(wlc->l2_filter, scb);
		if ((bcmp(scb_proxyarp->ipv6_local.addr, data + NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET,
		  IPV6_ADDR_LEN) == 0) ||
		  (bcmp(scb_proxyarp->ipv6_global.addr, data + NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET,
		  IPV6_ADDR_LEN) == 0)) {
			/* find ip address matched scb */
			dst_mac = &(scb->ea);
			break;
		}
	}

	if (!dst_mac) {
		if (send)
			return 0;
		else
			return -1;
	}

	if (send)
		buflen = pktlen + ALIGN_ADJ_BUFLEN;
	else
		buflen = pktlen;

	if (!(pkt = PKTGET(wlc->osh, buflen, (!send)))) {
		WL_ERROR(("wl%d: %s: alloc failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	if (send) {
		/*
		 * Adjust for pkt alignment problems when pkt is pulled by
		 * 14bytes of ETHER_HDR_LEN in linux osl
		 */
		PKTPULL(wlc->osh, pkt, ALIGN_ADJ_BUFLEN);
	}

	frame = PKTDATA(wlc->osh, pkt);

	/* Create 14-byte eth header, plus snap header if applicable */
	bcopy(src_mac, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	bcopy(dst_mac, frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);
	if (snap) {
		hton16_ua_store(pktlen, frame + ETHER_TYPE_OFFSET);
		bcopy(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN);
		hton16_ua_store(ETHER_TYPE_IPV6, frame + ETHER_HDR_LEN + SNAP_HDR_LEN);
	} else
		hton16_ua_store(ETHER_TYPE_IPV6, frame + ETHER_TYPE_OFFSET);

	/* move past eth/eth-snap header */
	na = frame + pktlen - NEIGHBOR_ADVERTISE_DATA_LEN;

	/* Create 72 bytes neighbor advertisement data frame */

	/* Create 40 bytes ipv6 header */
	bcopy(data, na, IPV6_SRC_IP_OFFSET);
	hton16_ua_store((NEIGHBOR_ADVERTISE_DATA_LEN - NEIGHBOR_ADVERTISE_TYPE_OFFSET),
		(na + IPV6_PAYLOAD_LEN_OFFSET));
	*(na + IPV6_HOP_LIMIT_OFFSET) = 255;
	bcopy(data+NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET, na+IPV6_SRC_IP_OFFSET, IPV6_ADDR_LEN);
	bcopy(data+IPV6_SRC_IP_OFFSET, na+IPV6_DEST_IP_OFFSET, IPV6_ADDR_LEN);

	/* Create 32 bytes icmpv6 NA frame body */
	bzero((na + NEIGHBOR_ADVERTISE_TYPE_OFFSET),
		(NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET - NEIGHBOR_ADVERTISE_TYPE_OFFSET));
	*(na + NEIGHBOR_ADVERTISE_TYPE_OFFSET) = NEIGHBOR_ADVERTISE_TYPE;
	*(na + NEIGHBOR_ADVERTISE_FLAGS_OFFSET) = NEIGHBOR_ADVERTISE_FLAGS_VALUE;
	bcopy(data+NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET, na+NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET,
		IPV6_ADDR_LEN);
	*(na + NEIGHBOR_ADVERTISE_OPTION_OFFSET) = OPT_TYPE_TGT_LINK_ADDR;
	*(na + NEIGHBOR_ADVERTISE_OPTION_OFFSET + TLV_LEN_OFF) = ND_OPTION_LEN_ETHER;
	bcopy(dst_mac, na + NEIGHBOR_ADVERTISE_OPTION_OFFSET + TLV_BODY_OFF,
		ETHER_ADDR_LEN);

	/* calculate ICMPv6 check sum */
	checksum = calc_checksum(na+IPV6_SRC_IP_OFFSET, na+IPV6_DEST_IP_OFFSET,
		NEIGHBOR_ADVERTISE_DATA_LEN - NEIGHBOR_ADVERTISE_TYPE_OFFSET,
		IP_PROT_ICMP6, na + NEIGHBOR_ADVERTISE_TYPE_OFFSET);
	*((uint16 *)(na + NEIGHBOR_ADVERTISE_CHECKSUM_OFFSET)) = checksum;

	WL_L2FILTER(("wl%d: %s: ip %x:%x:%x:%x:%x:%x:%x:%x, eth %x:%x:%x:%x:%x:%x\n",
		wlc->pub->unit, __FUNCTION__,
		na[8], na[9], na[18], na[19],
		na[20], na[21], na[22], na[23],
		dst_mac->octet[0], dst_mac->octet[1], dst_mac->octet[2],
		dst_mac->octet[3], dst_mac->octet[4], dst_mac->octet[5]));
#ifdef DEBUG_PROXY_ARP
	wlc_l2_filter_dump_packet(na, NEIGHBOR_ADVERTISE_DATA_LEN);
#endif // endif
	if (send) {
		wlc_sendup(wlc, bsscfg, NULL, pkt);
	} else {
		wlc_sendpkt(wlc, pkt, bsscfg->wlcif);
	}
	return 0;
}

static int
wlc_l2_filter_proxy_arp(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu, bool send)
{
	uint8 *frame = PKTDATA(wlc->osh, sdu);
	uint16 ethertype;
	uint8 *data;
	int datalen;
	bool snap;

	if (wlc_l2_filter_get_ether_type(wlc, sdu, &data, &datalen, &ethertype, &snap) != 0)
		return -1;

	if (ethertype == ETHER_TYPE_ARP) {
		struct bcmarp *arp = (struct bcmarp *)data;

		if (datalen < ARP_DATA_LEN) {
			WL_ERROR(("wl%d: %s: short ARP frame, ignored\n",
				wlc->pub->unit, __FUNCTION__));
			return -1;
		}

		if (arp->oper == HTON16(ARP_OPC_REQUEST)) {
			if (!send) {
				WL_L2FILTER(("ARP_REQ: %d.%d.%d.%d %x:%x:%x:%x:%x:%x\n",
				  arp->dst_ip[0], arp->dst_ip[1], arp->dst_ip[2], arp->dst_ip[3],
				  arp->src_eth[0], arp->src_eth[1], arp->src_eth[2],
				  arp->src_eth[3], arp->src_eth[4], arp->src_eth[5]));
				wlc_l2_filter_save_ip_addr(wlc, bsscfg, arp->src_ip,
					arp->src_eth, TRUE);
			}
			if (bcmp(arp->src_ip, arp->dst_ip, IPV4_ADDR_LEN)) {
				return wlc_l2_filter_arp_reply(wlc, bsscfg, arp, snap, send);
			}
		} else if ((arp->oper == HTON16(ARP_OPC_REPLY)) && !send) {
			WL_L2FILTER(("ARP_REPLY: %d.%d.%d.%d,%d.%d.%d.%d %x:%x:%x:%x:%x:%x\n",
				arp->src_ip[0], arp->src_ip[1], arp->src_ip[2], arp->src_ip[3],
				arp->dst_ip[0], arp->dst_ip[1], arp->dst_ip[2], arp->dst_ip[3],
				arp->src_eth[0], arp->src_eth[1], arp->src_eth[2],
				arp->src_eth[3], arp->src_eth[4], arp->src_eth[5]));
			wlc_l2_filter_save_ip_addr(wlc, bsscfg, arp->src_ip, arp->src_eth, TRUE);
		}
	}
	else if ((ethertype == ETHER_TYPE_IP) && send) {
		uint8 *udph;
		uint8 *dhcp;
		uint16 port;
		uint16 ihl;
		if (datalen < IPV4_OPTIONS_OFFSET || (IP_VER(data) != IP_VER_4)) {
			return -1;
		}
		if (IPV4_PROT(data) != IP_PROT_UDP) {
			return -1;
		}
		ihl = IPV4_HLEN(data);
		if (datalen < ihl + UDP_HDR_LEN + DHCP_CHADDR_OFFSET + ETHER_ADDR_LEN) {
			return -1;
		}
		udph = data + ihl;
		port = ntoh16_ua((const void *)(udph + UDP_DEST_PORT_OFFSET));
		/* only process DHCP packets from server to client */
		if (port != DHCP_PORT_CLIENT) {
			return -1;
		}
		dhcp = udph + UDP_HDR_LEN;
		/* only process DHCP reply(offer/ack) packets */
		if (*(dhcp + DHCP_TYPE_OFFSET) != DHCP_TYPE_REPLY) {
			return -1;
		}
		WL_L2FILTER(("DHCP_REPLY: ip %d.%d.%d.%d eth %x:%x:%x:%x:%x:%x\n",
			dhcp[16], dhcp[17], dhcp[18], dhcp[19],
			dhcp[28], dhcp[29], dhcp[30],
			dhcp[31], dhcp[32], dhcp[33]));
		wlc_l2_filter_save_ip_addr(wlc, bsscfg, (dhcp + DHCP_YIADDR_OFFSET),
			(dhcp + DHCP_CHADDR_OFFSET), TRUE);
	}
	else if (ethertype == ETHER_TYPE_IPV6) {
		bcm_tlv_t *link_addr;
		/* check for neighbour advertisement and solicitation */
		if (datalen < NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET + IPV6_ADDR_LEN)
			return -1;
		if (data[IPV6_NEXT_HDR_OFFSET] != IP_PROT_ICMP6)
			return -1;
#ifdef DEBUG_PROXY_ARP
		if ((data[NEIGHBOR_ADVERTISE_TYPE_OFFSET] == 128) && !send) {
			WL_L2FILTER(("Echo Req: %d.%d.%d.%d src %x:%x:%x:%x:%x:%x\n",
				data[8], data[21], data[22], data[23],
				frame[6], frame[7], frame[8],
				frame[9], frame[10], frame[11]));
			WL_L2FILTER(("Echo Req: dst %x:%x:%x:%x:%x:%x\n",
				frame[0], frame[1], frame[2],
				frame[3], frame[4], frame[5]));
		}
		if ((data[NEIGHBOR_ADVERTISE_TYPE_OFFSET] == 129) && !send) {
			WL_L2FILTER(("Echo Reply: %d.%d.%d.%d src %x:%x:%x:%x:%x:%x\n",
				data[8], data[21], data[22], data[23],
				frame[6], frame[7], frame[8],
				frame[9], frame[10], frame[11]));
			WL_L2FILTER(("Echo Reply: dst %x:%x:%x:%x:%x:%x\n",
				frame[0], frame[1], frame[2],
				frame[3], frame[4], frame[5]));
		}
#endif /* DEBUG_PROXY_ARP */
		if ((data[NEIGHBOR_ADVERTISE_TYPE_OFFSET] == NEIGHBOR_ADVERTISE_TYPE) && !send) {
			link_addr = parse_nd_options(data+NEIGHBOR_ADVERTISE_OPTION_OFFSET,
				datalen-NEIGHBOR_ADVERTISE_OPTION_OFFSET, OPT_TYPE_TGT_LINK_ADDR);
			WL_L2FILTER(("NA:%d.%d::%d.%d.%d.%d %x:%x:%x:%x:%x:%x\n",
				data[48], data[49], data[60], data[61], data[62], data[63],
				frame[6], frame[7], frame[8],
				frame[9], frame[10], frame[11]));
			if ((link_addr != NULL) && (link_addr->len == ND_OPTION_LEN_ETHER)) {
				wlc_l2_filter_save_ip_addr(wlc, bsscfg,
					data + NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET,
					link_addr->data, FALSE);
			} else {
				wlc_l2_filter_save_ip_addr(wlc, bsscfg,
					data + NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET,
					frame + ETHER_SRC_OFFSET, FALSE);
			}
#ifdef DEBUG_PROXY_ARP
			wlc_l2_filter_dump_packet(data, datalen);
#endif // endif
		} else if (data[NEIGHBOR_ADVERTISE_TYPE_OFFSET] == NEIGHBOR_SOLICITATION_TYPE) {
			link_addr = parse_nd_options(data+NEIGHBOR_ADVERTISE_OPTION_OFFSET,
				datalen-NEIGHBOR_ADVERTISE_OPTION_OFFSET, OPT_TYPE_SRC_LINK_ADDR);
			if (null_ip_addr(data + NEIGHBOR_ADVERTISE_SRC_IPV6_OFFSET, FALSE)) {
				/* duplicate address detection */
				if (!send) {
					WL_L2FILTER(("Duplicate detection NS:%d.%d.%d\n",
						data[61], data[62], data[63]));
#ifdef DEBUG_PROXY_ARP
					wlc_l2_filter_dump_packet(data, datalen);
#endif // endif
					/* update neighbor cache, drop this packet */
					if ((link_addr != NULL) &&
						(link_addr->len == ND_OPTION_LEN_ETHER)) {
						wlc_l2_filter_save_ip_addr(wlc, bsscfg,
						    data + NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET,
						    link_addr->data, FALSE);
					} else {
						wlc_l2_filter_save_ip_addr(wlc, bsscfg,
						    data + NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET,
						    frame + ETHER_SRC_OFFSET, FALSE);
					}
					return 0;
				}
				return -1;
			}
			if (!send) {
				WL_L2FILTER(("NS:%d.%d::%d.%d.%d.%d %x:%x:%x:%x:%x:%x\n",
					data[8], data[9], data[20], data[21], data[22], data[23],
					frame[6], frame[7], frame[8],
					frame[9], frame[10], frame[11]));
#ifdef DEBUG_PROXY_ARP
				wlc_l2_filter_dump_packet(data, datalen);
#endif // endif
				if ((link_addr != NULL) &&
					(link_addr->len == ND_OPTION_LEN_ETHER)) {
					wlc_l2_filter_save_ip_addr(wlc, bsscfg,
						data + NEIGHBOR_ADVERTISE_SRC_IPV6_OFFSET,
						link_addr->data, FALSE);
				} else {
					wlc_l2_filter_save_ip_addr(wlc, bsscfg,
						data + NEIGHBOR_ADVERTISE_SRC_IPV6_OFFSET,
						frame + ETHER_SRC_OFFSET, FALSE);
				}
			}
			return wlc_l2_filter_neighbor_advertise(wlc, bsscfg, data, snap, send,
				frame + ETHER_SRC_OFFSET);
		}
	}
	return -1;
}

int
wlc_l2_filter_rcv_data_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	if (wlc->l2_filter == NULL)
		return -1;

	if (wlc->l2_filter->proxy_arp && BSSCFG_AP(bsscfg)) {
		if (wlc_l2_filter_proxy_arp(wlc, bsscfg, sdu, FALSE) == 0) {
			PKTFREE(wlc->osh, sdu, FALSE);
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->grat_arp_enable && BSSCFG_STA(bsscfg)) {
		if (wl_gratuitous_arp(wlc, sdu) == 0) {
			PKTFREE(wlc->osh, sdu, FALSE);
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->block_ping) {
		if (wlc_l2_filter_ping(wlc, sdu) == 0) {
			PKTFREE(wlc->osh, sdu, FALSE);
			WL_L2FILTER(("wl%d: %s: drop ping packet\n",
				wlc->pub->unit, __FUNCTION__));
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->block_tdls) {
		if (wlc_l2_filter_tdls(wlc, sdu) == 0) {
			PKTFREE(wlc->osh, sdu, FALSE);
			/* packet dropped */
			return 0;
		}
	}

	/* packet ignored */
	return -1;
}

int
wlc_l2_filter_send_data_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	if (wlc->l2_filter == NULL)
		return -1;

	if (wlc->l2_filter->proxy_arp && BSSCFG_AP(bsscfg)) {
		if (wlc_l2_filter_proxy_arp(wlc, bsscfg, sdu, TRUE) == 0) {
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->block_sta) {
		if (wlc_l2_filter_sta(wlc, bsscfg, sdu) == 0) {
			WL_L2FILTER(("wl%d: %s: drop STA2STA packet\n",
				wlc->pub->unit, __FUNCTION__));
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->dhcp_unicast) {
		if (wlc_l2_filter_dhcp_unicast(wlc, bsscfg, sdu) == 0) {
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->block_multicast) {
		if (wlc_l2_filter_multicast(wlc, sdu) == 0) {
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->grat_arp_enable && BSSCFG_AP(bsscfg)) {
		if (wl_gratuitous_arp(wlc, sdu) == 0) {
			/* packet dropped */
			return 0;
		}
	}

	/* packet ignored */
	return -1;
}

bool
wlc_l2_filter_proxy_arp_enab(wlc_info_t *wlc)
{
	if (wlc->l2_filter == NULL)
		return FALSE;
	return wlc->l2_filter->proxy_arp;
}
