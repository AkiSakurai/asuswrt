/**
 * Tcp-ip Offload Engine (TOE) Components
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wl_toe.c 764654 2018-05-27 23:20:00Z $
 *
 * @file
 * @brief
 * TOE offloads various small components from the tcp-ip stack to the wl
 * driver.  The components offloaded are checksum for icmp-pkt,
 * tcp-segment, ip-header and non-ip-fragmented' udp-datagrams.
 *
 * Supported protocol families: IPv4.
 *
 * @file
 * @brief
 * To support UDP checksums, we need to support CoF (checksum over IP
 * fragments).	OS's like Windows fragment large udp-datagrams into
 * multiple ip-packets with only one udp-header for them all.  We will
 * have to buffer send and receive UDP fragments.
 *
 * @file
 * @brief
 * Also, Linux can't be told that we support TCP checksum but not UDP,
 * hence UDP-checksum of at least non-ip-fragmented datagrams is implemented.
 *
 * This offload is not related to the hardware offload engines in AC chips.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlTcpOffload]
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
#include <vlan.h>
#include <802.3.h>
#include <bcmip.h>
#include <bcmudp.h>
#include <bcmtcp.h>
#include <bcmicmp.h>
#include <bcmendian.h>

#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>

#include <wl_toe.h>

/* TOE private info structure */
struct wl_toe_info {
	wlc_info_t		*wlc;		/* pointer to main wlc structure */
	uint32			ol_cmpnts;	/* tcp mini offload(ol) components OR'red here. */
	struct toe_ol_stats_t	toe_stats;	/* tx-rx csum offload statistics */
#ifdef TOE_ERRTEST
	uint32			errtest;	/* Error injection test modes */
#endif // endif
};

/* forward declarations */
static int toe_doiovar(void *hdl, uint32 actionid,
                       void *p, uint plen, void *a,
                       uint alen, uint vsize, struct wlc_if *wlcif);

/*
 * IPv4 handler. 'ph' points to protocol specific header,
 * for example, it points to UDP header if it is UDP packet.
 */

/* wlc_pub_t struct access macros */
#define WLCUNIT(toei)	((toei)->wlc->pub->unit)
#define WLCOSH(toei)	((toei)->wlc->osh)

/* special values */
/* 802.3 llc/snap header */
static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

/* IOVar table */
enum {
	IOV_TOE,
	IOV_TOE_OL,		/* Flags for enabling/disabling tx/rx checksum */
	IOV_TOE_STATS,		/* Display checksum offload statistics. */
	IOV_TOE_STATS_CLEAR,	/* Clear checksum offload statistics */
	IOV_TOE_ERRTEST		/* Error test modes */
};

static const bcm_iovar_t toe_iovars[] = {
	{"toe", IOV_TOE, 0, 0, IOVT_BOOL, 0},
	{"toe_ol", IOV_TOE_OL,
	(0), 0, IOVT_UINT32, 0
	},
	{"toe_stats", IOV_TOE_STATS,
	(0), 0, IOVT_BUFFER, sizeof(struct toe_ol_stats_t)
	},
	{"toe_stats_clear", IOV_TOE_STATS_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
#ifdef TOE_ERRTEST
	{"toe_errtest", IOV_TOE_ERRTEST,
	(0), 0, IOVT_UINT32, 0
	},
#endif // endif
	{NULL, 0, 0, 0, 0, 0 }
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/*
 * For Thumb architecture, use a hand-optimized assembly partial checksum routine.
 * The assembly and C versions originate from src/toe/shared/{csum.c,csumthumb.S}.
 */

static uint32
csum_part(const uint8 *pb, int len)
{
	const uint16 *ps;
	uint16 t = 0;
	const uint32 *pl;
	uint32 sum = 0, tmp;
	int odd = ((uint32)(uintptr)pb & 1);	/* starts at odd byte address? */

	if (odd && len > 0) {
		((uint8 *)&t)[1] = *pb++;
		len--;
	}

	ps = (const uint16 *)pb;

	if (((uint32)(uintptr)ps & 3) && len > 1) {
		sum += *ps++;
		len -= 2;
	}

	pl = (const uint32 *)ps;

	while (len & ~15)  {
		tmp = sum + *pl++;	/* ping */
		if (tmp < sum)
			tmp++;		/* add back carry */

		sum = tmp + *pl++;	/* pong */
		if (sum < tmp)
			sum++;		/* add back carry */

		tmp = sum + *pl++;	/* ping */
		if (tmp < sum)
			tmp++;		/* add back carry */

		sum = tmp + *pl++;	/* pong */
		if (sum < tmp)
			sum++;		/* add back carry */

		len -= 16;
	}

	/* fold to make room in upper bits */
	sum = (sum >> 16) + (sum & 0xffff);

	ps = (const uint16 *)pl;

	/* zero to seven 16-bit aligned shorts remaining */
	while (len > 1) {
		sum += *ps++;
		len -= 2;
	}

	/* dangling tail byte remaining? */
	if (len > 0)			/* include odd byte */
		((uint8 *)&t)[0] = *(const uint8 *)ps;

	sum += t;			/* add end bytes */

	/* fold twice */
	sum = (sum >> 16) + (sum & 0xffff);
	sum = (sum >> 16) + (sum & 0xffff);

	/* byte-swap sum if original alignment was odd */
	if (odd)
		sum = (sum << 8 | sum >> 8) & 0xffff;

	return sum;
}

static uint16
csum_inet(void *dataptr, uint16 len)
{
	uint32 acc;

	acc = csum_part(dataptr, len);
	while (acc >> 16) {
		acc = (acc & 0xffff) + (acc >> 16);
	}
	return (uint16)~(acc & 0xffff);
}

static uint16
csum_with_pseudohdr(uint8 *tcph, uint32 sip, uint32 dip,
                    uint8 proto, uint16 proto_len)
{
	uint32 acc;

	acc = 0;

	acc += csum_part(tcph, proto_len);

	/* pseudo header */
	acc += (sip & 0xffffUL);
	acc += ((sip >> 16) & 0xffffUL);
	acc += (dip & 0xffffUL);
	acc += ((dip >> 16) & 0xffffUL);

	acc += (uint32)hton16((uint16)proto);
	acc += (uint32)hton16(proto_len);

	while (acc >> 16) {
		acc = (acc & 0xffffUL) + (acc >> 16);
	}

	return (uint16)~(acc & 0xffffUL);
}

/*
 * Initialize toe private context. It returns a pointer to the
 * toe private context if succeeded. Otherwise it returns NULL.
 */
wl_toe_info_t *
BCMATTACHFN(wl_toe_attach)(wlc_info_t *wlc)
{
	wl_toe_info_t *toei;

	/* allocate toe private info struct */
	toei = MALLOCZ(wlc->osh, sizeof(wl_toe_info_t));
	if (!toei) {
		WL_ERROR(("wl%d: %s: MALLOCZ failed, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init toe private info struct */
	toei->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, toe_iovars, "toe", toei, toe_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	return toei;
}

void
BCMATTACHFN(wl_toe_detach)(wl_toe_info_t *toei)
{
	if (!toei)
		return;
	wlc_module_unregister(toei->wlc->pub, "toe", toei);
	MFREE(WLCOSH(toei), toei, sizeof(wl_toe_info_t));
}

void
BCMATTACHFN(wl_toe_set_olcmpnts)(wl_toe_info_t *toei, int ol_cmpnts)
{
	toei->ol_cmpnts |= ol_cmpnts;
}

/* Handling TOE-related iovars */
static int
toe_doiovar(void *hdl, uint32 actionid,
            void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wl_toe_info_t *toei = hdl;
	int err = 0;
	uint32 *ret_int_ptr = (uint32 *)a;
	bool bool_val;
	int32 int_val = 0;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	/* bool conversion to avoid duplication below */
	bool_val = (int_val != 0);

	switch (actionid) {
	case IOV_GVAL(IOV_TOE):
		*ret_int_ptr = (int32)toei->wlc->pub->_toe;
		break;

	case IOV_SVAL(IOV_TOE):
		toei->wlc->pub->_toe = bool_val;
		break;

	case IOV_SVAL(IOV_TOE_OL):
		bcopy(a, (void *)&toei->ol_cmpnts, sizeof(toei->ol_cmpnts));
		break;

	case IOV_GVAL(IOV_TOE_OL):
		*ret_int_ptr = toei->ol_cmpnts;
		break;

	case IOV_SVAL(IOV_TOE_STATS_CLEAR):
		bzero((void *)&toei->toe_stats, sizeof(struct toe_ol_stats_t));
		break;

	case IOV_GVAL(IOV_TOE_STATS):
		if (alen < sizeof(struct toe_ol_stats_t))
			return BCME_BUFTOOSHORT;

		bcopy((void *)&toei->toe_stats, a, sizeof(struct toe_ol_stats_t));
		break;

#ifdef TOE_ERRTEST
	case IOV_SVAL(IOV_TOE_ERRTEST):
		bcopy(a, (void *)&toei->errtest, sizeof(toei->errtest));
		break;

	case IOV_GVAL(IOV_TOE_ERRTEST):
		bcopy((void *)&toei->errtest, a, sizeof(toei->errtest));
		break;
#endif /* TOE_ERRTEST */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* Returns -1 if frame is not IP; otherwise, returns pointer/length of IP portion */
static int
wl_toe_parse_ip(wl_toe_info_t *toei, void *sdu,
                struct ipv4_hdr **iph_ptr, int *ipl_ptr)
{
	uint8 *frame = PKTDATA(WLCOSH(toei), sdu);
	int length = PKTLEN(WLCOSH(toei), sdu);
	uint8 *pt;			/* Pointer to type field */
	uint16 ethertype, ihl, ippktlen;
	struct ipv4_hdr *iph;		/* IP frame pointer */
	int ipl;			/* IP frame length */
	uint16 iph_frag;

	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (length < ETHER_HDR_LEN) {
		WL_ERROR(("wl%d: %s: short eth frame (%d)\n",
		          WLCUNIT(toei), __FUNCTION__, length));
		return -1;
	} else if (ntoh16(*(uint16 *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		WL_INFORM(("wl%d: %s: 802.3 LLC/SNAP\n", WLCUNIT(toei), __FUNCTION__));
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
	} else {
		WL_ERROR(("wl%d: %s: non-SNAP 802.3 frame\n",
		          WLCUNIT(toei), __FUNCTION__));
		return -1;
	}

	ethertype = ntoh16(*(uint16 *)pt);

	/* Skip VLAN tag, if any */
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			WL_ERROR(("wl%d: %s: short VLAN frame (%d)\n",
			          WLCUNIT(toei), __FUNCTION__, length));
			return -1;
		}

		ethertype = ntoh16(*(uint16 *)pt);
	}

	if (ethertype != ETHER_TYPE_IP) {
		WL_ERROR(("wl%d: %s: non-IP frame (ethertype 0x%x, length %d)\n",
		          WLCUNIT(toei), __FUNCTION__, ethertype, length));
		return -1;
	}

	iph = (struct ipv4_hdr *)(pt + ETHER_TYPE_LEN);
	ipl = length - (pt + ETHER_TYPE_LEN - frame);

	WL_INFORM(("wl%d: %s: IP len %d\n", WLCUNIT(toei), __FUNCTION__, ipl));

	/* We support IPv4 only */
	if (ipl < IPV4_OPTIONS_OFFSET || (IP_VER(iph) != IP_VER_4)) {
		WL_ERROR(("wl%d: %s: short frame (%d) or non-IPv4\n",
		          WLCUNIT(toei), __FUNCTION__, ipl));
		return -1;
	}

	/* Header length sanity */
	ihl = IPV4_HLEN(iph);
	if (ihl < IPV4_OPTIONS_OFFSET || ihl > ipl) {
		WL_ERROR(("wl%d: %s: IP-header-len (%d) out of range (%d-%d)\n",
		          WLCUNIT(toei), __FUNCTION__, ihl, IPV4_OPTIONS_OFFSET, ipl));
		return -1;
	}

	/*
	 * Packet length sanity; sometimes we receive eth-frame size bigger
	 * than the IP content, which results in a bad tcp chksum
	 */
	ippktlen = ntoh16(iph->tot_len);
	if (ippktlen < ipl) {
		WL_ERROR(("wl%d: %s: extra frame length ignored (%d)\n",
		          WLCUNIT(toei), __FUNCTION__, ipl - ippktlen));
		ipl = ippktlen;
	} else if (ippktlen > ipl) {
		WL_ERROR(("wl%d: %s: truncated IP packet (%d)\n",
		          WLCUNIT(toei), __FUNCTION__, ippktlen - ipl));
		return -1;
	}

	/*
	 * We don't handle fragmented IP packets.  A first frag is indicated by the MF
	 * (more frag) bit and a subsequent frag is indicated by a non-zero frag offset.
	 */
	iph_frag = ntoh16(iph->frag);

	if ((iph_frag & IPV4_FRAG_MORE) || (iph_frag & IPV4_FRAG_OFFSET_MASK) != 0) {
		WL_INFORM(("wl%d: %s: IP fragment not handled\n",
		           WLCUNIT(toei), __FUNCTION__));
		return -1;
	}

	*iph_ptr = iph;
	*ipl_ptr = ipl;

	return 0;
}

/*
 * Process frames in transmit direction.
 */
void
wl_toe_send_proc(wl_toe_info_t *toei, void *sdu)
{
	struct ipv4_hdr *iph;
	int ipl;
	uint8 prot;
	uint16 ihl, csum, hstcsum;

	if ((toei->ol_cmpnts & TOE_TX_CSUM_OL) == 0)
		return;

	/* Do nothing if packet already has good checksums */
	if (!PKTSUMNEEDED(sdu)) {
		WL_INFORM(("wl%d: wl_toe_send_proc: summing unnecessary\n", WLCUNIT(toei)));
		toei->toe_stats.tx_summed++;
		return;
	}

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("wl_toe_send_proc: MPDU", WLCOSH(toei), sdu);
#endif // endif

	if (wl_toe_parse_ip(toei, sdu, &iph, &ipl) < 0)
		return;

	/* Calculate checksum of ip-hdr */

	ihl = IPV4_HLEN(iph);
	hstcsum = iph->hdr_chksum;
	iph->hdr_chksum = 0;

	csum = csum_inet(iph, ihl);

	WL_INFORM(("wl%d: wl_toe_send_proc: %s host tx ip-hdr-csum: host=0x%x, dev=0x%x\n",
	           WLCUNIT(toei),
	           (csum == hstcsum) ? "correct" : "wrong",
	           hstcsum, csum));
	BCM_REFERENCE(hstcsum);
	iph->hdr_chksum = csum;
	toei->toe_stats.tx_iph_fill++;

	/* Calculate protocol-specific checksum */
	prot = IPV4_PROT(iph);

	if (prot == IP_PROT_TCP) {
		struct bcmtcp_hdr *tcph = (struct bcmtcp_hdr *)((uint8 *)iph + ihl);
		int tcpl = ipl - ihl;
		uint32 sip, dip;

		hstcsum = tcph->chksum;
		tcph->chksum = 0;

		bcopy(iph->src_ip, (void *)&sip, sizeof(struct ipv4_addr));
		bcopy(iph->dst_ip, (void *)&dip, sizeof(struct ipv4_addr));

		csum = csum_with_pseudohdr((uint8 *)tcph, sip, dip, IP_PROT_TCP, (uint16)tcpl);

		WL_INFORM(("wl%d: wl_toe_send_proc: %s host tcp csum: host=0x%x, dev=0x%x\n",
		           WLCUNIT(toei),
		           (csum == hstcsum) ? "correct" : "wrong",
		           hstcsum, csum));

		tcph->chksum = csum;
		toei->toe_stats.tx_tcp_fill++;

#ifdef TOE_ERRTEST
		/* Leave the csum sent by host for tesing to see if host
		 * has sent csummed or non-csummed pkt.
		 */
		if ((toei->errtest & TOE_ERRTEST_TX_CSUM) != 0) {
			tcph->chksum = hstcsum;
			toei->toe_stats.tx_tcp_errinj++;
		}
#endif /* TOE_ERRTEST */
	}
	else if (prot == IP_PROT_UDP) {
		/*
		 * UDP checksum is spanned between multiple ip-frags if datagram
		 * is bigger.  We hande only nonfragmented IP datagrams.
		 */
		struct bcmudp_hdr *udph = (struct bcmudp_hdr *)((uint8 *)iph + ihl);
		int udpl = ipl - ihl;
		uint32 sip, dip;

		hstcsum = udph->chksum;
		udph->chksum = 0;

		bcopy(iph->src_ip, (void *)&sip, sizeof(struct ipv4_addr));
		bcopy(iph->dst_ip, (void *)&dip, sizeof(struct ipv4_addr));

		csum = csum_with_pseudohdr((uint8 *)udph, sip, dip, IP_PROT_UDP, (uint16)udpl);

		WL_INFORM(("wl%d: wl_toe_send_proc: %s host tx udp csum: host=0x%x, dev=0x%x\n",
		           WLCUNIT(toei),
		           (csum == hstcsum) ? "correct" : "wrong",
		           hstcsum, csum));

		udph->chksum = csum;
		toei->toe_stats.tx_udp_fill++;

#ifdef TOE_ERRTEST
		/* Leave the csum sent by host for tesing to see if host
		 * has sent csummed or non-csummed pkt.
		 */
		if ((toei->errtest & TOE_ERRTEST_TX_CSUM) != 0) {
			udph->chksum = hstcsum;
			toei->toe_stats.tx_udp_errinj++;
		}
#endif /* TOE_ERRTEST */
	}
	else if (prot == IP_PROT_ICMP) {
		struct bcmicmp_hdr *icmph = (struct bcmicmp_hdr *)((uint8 *)iph + ihl);
		int icmpl = ipl - ihl;

		hstcsum = icmph->chksum;
		icmph->chksum = 0;

		csum = csum_inet(icmph, (uint16)icmpl);

		WL_INFORM(("wl%d: wl_toe_send_proc: %s host tx icmp csum: host=0x%x, dev=0x%x\n",
		           WLCUNIT(toei),
		           (csum == hstcsum) ? "correct" : "wrong",
		           hstcsum, csum));

		icmph->chksum = csum;
		toei->toe_stats.tx_icmp_fill++;

#ifdef TOE_ERRTEST
		/*
		 * Leave the csum sent by host for tesing to see if host
		 * has sent csummed or non-csummed pkt.
		 */
		if ((toei->errtest & TOE_ERRTEST_TX_CSUM) != 0)
			icmph->chksum = hstcsum;
			toei->toe_stats.tx_icmp_errinj++;
#endif /* TOE_ERRTEST */
	}
}

/*
 * Process frames in receive direction.
 *
 * Return value:
 *	0 if all checksums are good (packet is also marked good with PKTSETSUMMED)
 *	1 if packet has a known bad sum
 *	-1 if packet could not be parsed or receive sum offload not enabled
 *
 * Packets with a known bad sum should still be forwarded to the host so that the host can
 * keep accurate error stats.
 */
int
wl_toe_recv_proc(wl_toe_info_t *toei, void *sdu)
{
	struct ipv4_hdr *iph;
	int ipl;
	uint8 prot;
	uint16 ihl, csum;

	if ((toei->ol_cmpnts & TOE_RX_CSUM_OL) == 0)
		return -1;

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("%s: MPDU", WLCOSH(toei), sdu);
#endif // endif

	if (wl_toe_parse_ip(toei, sdu, &iph, &ipl) < 0)
		return -1;

	/* Verify checksum of ip-hdr */
	ihl = IPV4_HLEN(iph);
	if (csum_inet(iph, ihl) != 0) {
		WL_ERROR(("wl%d: %s: bad ip-hdr rx csum\n", WLCUNIT(toei), __FUNCTION__));
		PKTSETSUMGOOD(sdu, FALSE);
		toei->toe_stats.rx_iph_bad++;
		return -1;
	} else
		toei->toe_stats.rx_iph_good++;

	WL_INFORM(("wl%d: %s: good ip-hdr rx csum\n", WLCUNIT(toei), __FUNCTION__));

	/* Verify protocol-specific checksum */
	prot = IPV4_PROT(iph);

	if (prot == IP_PROT_TCP) {
		struct bcmtcp_hdr *tcph = (struct bcmtcp_hdr *)((uint8 *)iph + ihl);
		int tcpl = ipl - ihl;
		uint32 sip, dip;

		bcopy(iph->src_ip, (void *)&sip, sizeof(struct ipv4_addr));
		bcopy(iph->dst_ip, (void *)&dip, sizeof(struct ipv4_addr));

		csum = csum_with_pseudohdr((void *)tcph, sip, dip, IP_PROT_TCP, (uint16)tcpl);

		if (csum != 0) {
			WL_ERROR(("wl%d: %s: bad tcp csum 0x%x\n",
			          WLCUNIT(toei), __FUNCTION__, csum));
			PKTSETSUMGOOD(sdu, FALSE);
			toei->toe_stats.rx_tcp_bad++;
			return -1;
		} else
			toei->toe_stats.rx_tcp_good++;

		WL_INFORM(("wl%d: %s: good tcp-seg csum\n", WLCUNIT(toei), __FUNCTION__));

#ifdef TOE_ERRTEST
		/* Silently corrupt the checksum for testing */
		if ((toei->errtest & TOE_ERRTEST_RX_CSUM) != 0) {
			tcph->chksum += 1;
			toei->toe_stats.rx_tcp_errinj++;
		}

		/* Corrupt checksum and force kernel to checksum */
		if ((toei->errtest & TOE_ERRTEST_RX_CSUM2) != 0) {
			tcph->chksum += 1;
			return 1;
		}
#endif /* TOE_ERRTEST */
	}
	else if (prot == IP_PROT_UDP) {
		/*
		 * UDP checksum is spanned between multiple ip-frags if datagram
		 * is bigger.  We checksum only non-fragmented UDP datagrams.
		 */
		struct bcmudp_hdr *udph = (struct bcmudp_hdr *)((uint8 *)iph + ihl);
		int udpl = ipl - ihl;
		uint32 sip, dip;

		bcopy(iph->src_ip, (void *)&sip, sizeof(struct ipv4_addr));
		bcopy(iph->dst_ip, (void *)&dip, sizeof(struct ipv4_addr));

		csum = csum_with_pseudohdr((void *)udph, sip, dip, IP_PROT_UDP, (uint16)udpl);

		if (csum != 0) {
			WL_ERROR(("wl%d: %s: bad udp csum 0x%x\n",
			          WLCUNIT(toei), __FUNCTION__, csum));
			PKTSETSUMGOOD(sdu, FALSE);
			toei->toe_stats.rx_udp_bad++;
			return -1;
		}
			toei->toe_stats.rx_udp_good++;

		WL_INFORM(("wl%d: %s: good udp datagram csum\n", WLCUNIT(toei), __FUNCTION__));

#ifdef TOE_ERRTEST
		/* Silently corrupt the checksum for testing */
		if ((toei->errtest & TOE_ERRTEST_RX_CSUM) != 0) {
			udph->chksum += 1;
			toei->toe_stats.rx_udp_errinj++;
		}

		/* Corrupt checksum and force kernel to checksum */
		if ((toei->errtest & TOE_ERRTEST_RX_CSUM2) != 0) {
			udph->chksum += 1;
			return 1;
		}
#endif /* TOE_ERRTEST */
	}
	else if (prot == IP_PROT_ICMP) {
		struct bcmicmp_hdr *icmph = (struct bcmicmp_hdr *)((uint8 *)iph + ihl);
		int icmpl = ipl - ihl;

		if (csum_inet(icmph, (uint16)icmpl) != 0) {
			WL_ERROR(("wl%d: %s: bad icmp csum\n", WLCUNIT(toei), __FUNCTION__));
			PKTSETSUMGOOD(sdu, FALSE);
			toei->toe_stats.rx_icmp_bad++;
			return -1;
		} else
			toei->toe_stats.rx_icmp_good++;

		WL_INFORM(("wl%d: %s: good icmp csum\n", WLCUNIT(toei), __FUNCTION__));

#ifdef TOE_ERRTEST
		/* Silently corrupt the checksum for testing */
		if ((toei->errtest & TOE_ERRTEST_RX_CSUM) != 0) {
			icmph->chksum += 1;
			toei->toe_stats.rx_icmp_errinj++;
		}
#endif /* TOE_ERRTEST */
	}

	/* Indicate all checksums good */
	WL_INFORM(("wl%d: %s: sums verified\n", WLCUNIT(toei), __FUNCTION__));

	PKTSETSUMGOOD(sdu, TRUE);

	return 0;
}
