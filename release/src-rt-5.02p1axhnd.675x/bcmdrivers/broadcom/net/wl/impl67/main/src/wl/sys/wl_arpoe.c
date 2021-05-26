/**
 * @file
 * @brief
 *
 * ARP Offload
 *
 * The dongle should be able to handle as much of the ARP request and reply traffic as possible
 * without having to wake up the host.  There are 3 ways of offloading the ARP traffic.
 *
 * 1. Partial ARP offload (ARP filtering)
 *	Only the host-ip/host-mac is maintained on the dongle and non-relevant ARP requests
 *	to the host are filtered.
 * 2. ARP Agent
 *	Host-ip/host-mac and arp-table are maintained on the dongle. The dongle may reply to
 *	ARP requests from either the host or peers.
 * 3. Full ARP offload, complete ARP handling is offloaded to the dongle. The dongle must
 *	manage the ethernet SA/DA field of all packets.
 *
 * The code below implements the ARP Agent method of arp-offloading.
 * It supports multihoming hosts and link-local addressing.
 *
 * Supported protocol families: IPV4.
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
 * $Id: wl_arpoe.c 783417 2020-01-28 10:36:24Z $
 */

/**
 * XXX
 * @file
 * @brief
 * Apple specific feature.
 * Apple has requested ARP Offload as a method of saving power. The dongle should be able to handle
 * as much of the ARP request and reply traffic as possible without having to wake up the host.
 * Twiki: [WlArpOffload]
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
#include <bcmip.h>
#include <bcmarp.h>
#include <bcmudp.h>
#include <vlan.h>
#include <bcmendian.h>

#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wl_export.h>
#include <wlc_bsscfg.h>
#include <wl_arpoe.h>
#include <wlc_pcb.h>

#if defined(BCMDBG) || defined(WLMSG_INFORM)
#define WL_MSGBUF
#endif // endif

#define ARP_TABLE_SIZE			16		/* Number of peer host ip addresses */
#define ARP_MAX_AGE_DEFAULT		(20 * 60) 	/* 20 minutes */
#define ARP_PROBE_SIP			0x0		/* arp-probe sip is 0.0.0.0 */
#define ARP_LINK_LOCAL_IPADDR		0xA9FE		/* 169.254 link local address range */
#define ALIGN_ADJ_BUFLEN		2		/* Adjust for ETHER_HDR_LEN pull in linux
							 * which makes pkt nonaligned
							 */

#define LINKLOCAL(ipa)			((uint16)((ntoh32((ipa))) >> 16))
#define ISLINKLOCAL_IPADDR(ipa)		(LINKLOCAL(ipa) == ARP_LINK_LOCAL_IPADDR)
#define ARP_ENTRY_VALID()		(arpi->arp_table[i].age > 0)

/* max number of ARP requests supported at a given point of time */
#define MAX_ARP_REQUESTS_SUPPORTED	2

/* Arp-Table/Host-Cache lookup results */

#define SAT	0x8	/* Source IP found in Arp-Table */
#define DAT	0x4	/* Dest IP found in Arp-Table */
#define SHC	0x2	/* Source IP found in Host-Cache */
#define DHC	0x1	/* Dest IP found in Host-Cache */

struct arp_entry {
	struct ipv4_addr	ipa;
	struct ether_addr	ea;
	uint32			age;		/* Entry valid when age > 0 */
};

/* ARP private info structure */
struct wl_arp_info {
	wlc_info_t		*wlc;		/* Pointer back to wlc structure */
	uint32			ol_cmpnts;	/* Offload (ol) components OR'd together */
	struct ipv4_addr	host_ip[ARP_MULTIHOMING_MAX];
	struct ether_addr	host_mac;
	struct arp_entry	arp_table[ARP_TABLE_SIZE];
	struct arp_ol_stats_t	arp_ol_stats;
	uint32			arp_max_age;
	bool			fake_arp_reply;	/* Used in rx to distinguish betn real
						 * and fake arp_reply
						 */
#ifdef ARP_ERRTEST
	uint32			errtest;	/* Error injection test modes */
#endif // endif
	wlc_if_t        *wlcif;
	struct ipv4_addr	 remote_ip;
	struct ether_addr	 src_mac;
	uint8 max_arps;
	osl_t *osh;
};

/* forward declarations */
static int arp_doiovar(void *hdl, uint32 actionid,
                       void *p, uint plen, void *a, uint alen,
                       uint vsize, struct wlc_if *wlcif);
/*
 * IPv4 handler. 'ph' points to protocol specific header,
 * for example, it points to UDP header if it is UDP packet.
 */

/* wlc_pub_t struct access macros */
#define WLCUNIT(arpi)	((arpi)->wlc->pub->unit)
#define WLCOSH(arpi)	((arpi)->wlc->osh)

/* special values */
/* 802.3 llc/snap header */
static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

static uint32 m_ol_cmpnts;
/* IOVar table */
enum {
	IOV_ARPOE,
	IOV_ARP_OL,		/* Flags for enabling/disabling ARP offload sub-features */
	IOV_ARP_PEERAGE,	/* Set peer's age of the ip-mac-address in the arp-table */
	IOV_ARP_TABLE_CLEAR,	/* Clear all entries in the arp_table[] */
	IOV_ARP_HOSTIP,		/* Add local ip address to host_ip[] table */
	/* Add local ip address to host_ip[] table with specific ID */
	IOV_ARP_HOSTIP_ID,
	/* Clear all entries in the host_ip[] table */
	IOV_ARP_HOSTIP_CLEAR,
	/* Clear one entry in the host_ip[] table with specific ID */
	IOV_ARP_HOSTIP_CLEAR_ID,
	IOV_ARP_STATS,		/* Display arp offload statistics. */
	IOV_ARP_STATS_CLEAR,	/* Clear arp offload statistics. */
	IOV_ARP_ERRTEST,		/* Error test modes */
	IOV_ARP_VERSION,		/* ARP version */
};

static const bcm_iovar_t arp_iovars[] = {
	{"arpoe", IOV_ARPOE,
	(0), 0, IOVT_BOOL, 0},
	{"arp_ol", IOV_ARP_OL,
	(0), 0, IOVT_UINT32, 0
	},
	{"arp_peerage", IOV_ARP_PEERAGE,
	(0), 0, IOVT_UINT32, 0
	},
	{"arp_table_clear", IOV_ARP_TABLE_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
	{"arp_hostip", IOV_ARP_HOSTIP,
	(0), 0, IOVT_UINT32, 0
	},
	{"arp_hostip_id", IOV_ARP_HOSTIP_ID,
	(0), 0, IOVT_UINT32, 0
	},
	{"arp_hostip_clear_id", IOV_ARP_HOSTIP_CLEAR_ID,
	(0), 0, IOVT_UINT32, 0
	},
	{"arp_hostip_clear", IOV_ARP_HOSTIP_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
	{"arp_stats", IOV_ARP_STATS,
	(0), 0, IOVT_BUFFER, sizeof(struct arp_ol_stats_t)
	},
	{"arp_stats_clear", IOV_ARP_STATS_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
#ifdef ARP_ERRTEST
	{"arp_errtest", IOV_ARP_ERRTEST,
	(0), 0, IOVT_UINT32, 0
	},
#endif // endif
	{"arp_version", IOV_ARP_VERSION,
	(0), 0, IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0, 0 }
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static void
arp_clean_aged_entries(wl_arp_info_t *arpi)
{
	uint8 i;
#ifdef WL_MSGBUF
	char buf[32];
#endif // endif

	/* cleanup expired entries from the ARP table */
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID()) {
			arpi->arp_table[i].age--;
			/* expire old entry */
			if (arpi->arp_table[i].age == 0) {
				WL_INFORM(("%s:AO:ipaddr %s ix %d\n", __FUNCTION__,
					bcm_ip_ntoa(&arpi->arp_table[i].ipa, buf), i));
			}
		}
	}
}

/*
 * Age entries from the arp-table.
 */
static void
arp_watchdog(void *cntxt)
{
	wl_arp_info_t *arpi = (wl_arp_info_t *)cntxt;
	wlc_if_t *wlcif = arpi->wlc->wlcif_list;
	struct wl_info *wl = arpi->wlc->wl;

	if (ARPOE_ENAB(arpi->wlc->pub)) {
		/* clean expired arp entries for all iF's */
		while (wlcif != NULL) {
			if (wlcif->wlif != NULL) {
				arpi = wl_get_ifctx(wl, IFCTX_ARPI, wlcif->wlif);
				if (arpi != NULL)
					arp_clean_aged_entries(arpi);
			}
			wlcif = wlcif->next;
		}
	}
}
/*
 * Initialize ARP private context.
 * Returns a pointer to the ARP private context, NULL on failure.
 */
wl_arp_info_t *
BCMATTACHFN(wl_arp_attach)(wlc_info_t *wlc)
{
	wl_arp_info_t *arpi;

	/* allocate arp private info struct */
	arpi = MALLOCZ(wlc->osh, sizeof(wl_arp_info_t));
	if (!arpi) {
		WL_ERROR(("wl%d: %s: MALLOC failed; total mallocs %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init arp private info struct */
	arpi->wlc = wlc;
	arpi->osh = wlc->osh;

	/* register module */
	if (wlc_module_register(wlc->pub, arp_iovars, "arp", arpi, arp_doiovar,
	                        arp_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	arpi->arp_max_age = ARP_MAX_AGE_DEFAULT;
	arpi->fake_arp_reply = FALSE;

	return arpi;
}

void
BCMATTACHFN(wl_arp_detach)(wl_arp_info_t *arpi)
{

	WL_INFORM(("wl%d: arp_detach()\n", WLCUNIT(arpi)));

	if (!arpi)
		return;
	wlc_module_unregister(arpi->wlc->pub, "arp", arpi);
	MFREE(WLCOSH(arpi), arpi, sizeof(wl_arp_info_t));
}

/* Handling ARP-related iovars */
static int
arp_doiovar(void *hdl, uint32 actionid,
            void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wl_arp_info_t *arpi = hdl;
	int err = 0;
	int i;
	uint32 ipaddr;
	uint32 *ret_int_ptr = (uint32 *)a;
	uint32 arp_version = 0x20120910;
	bool bool_val;
	int32 int_val = 0;

#ifndef BCMROMOFFLOAD
	WL_INFORM(("wl%d: arp_doiovar(%u)\n", WLCUNIT(arpi), actionid));
#endif /* !BCMROMOFFLOAD */

	/* change arpi if wlcif is corr to a virtualIF */
	if (wlcif != NULL) {
		if (wlcif->wlif != NULL) {
			arpi = (wl_arp_info_t *)wl_get_ifctx(arpi->wlc->wl, IFCTX_ARPI,
			                                     wlcif->wlif);
		}
	}

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	/* bool conversion to avoid duplication below */
	bool_val = (int_val != 0);

	switch (actionid) {

	case IOV_GVAL(IOV_ARPOE):
		*ret_int_ptr = (int32)arpi->wlc->pub->_arpoe;
		break;

	case IOV_SVAL(IOV_ARPOE):
		if (ARPOE_SUPPORT(arpi->wlc->pub))
			arpi->wlc->pub->_arpoe = bool_val;
		else
			err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_ARP_OL):
		bcopy(a, &m_ol_cmpnts, sizeof(m_ol_cmpnts));
		break;

	case IOV_GVAL(IOV_ARP_OL):
		bcopy(&m_ol_cmpnts, a, sizeof(m_ol_cmpnts));
		break;

	case IOV_SVAL(IOV_ARP_PEERAGE):
		bcopy(a, &arpi->arp_max_age, sizeof(uint32));
		break;

	case IOV_GVAL(IOV_ARP_PEERAGE):
		*ret_int_ptr = arpi->arp_max_age;
		break;

	case IOV_SVAL(IOV_ARP_TABLE_CLEAR):
		/* Invalidate all entries (set age to zero) */
		bzero(arpi->arp_table, sizeof(struct arp_entry) * ARP_TABLE_SIZE);
		break;

	case IOV_SVAL(IOV_ARP_HOSTIP_CLEAR):
		bzero(arpi->host_ip, sizeof(struct ipv4_addr) * ARP_MULTIHOMING_MAX);
		break;

	case IOV_SVAL(IOV_ARP_HOSTIP):
	{
		/* Add one IP address to the host IP table. */
		if (plen < sizeof(struct ipv4_addr))
			return BCME_BUFTOOSHORT;

		if (ETHER_ISNULLADDR(&arpi->host_mac)) {
#if defined(BCMDBG) || defined(WLMSG_INFORM)
			char buf[32];
#endif /* BCMDBG || WLMSG_INFORM */
			if (arpi->wlcif != NULL) {
				bcopy(&arpi->wlcif->u.bsscfg->cur_etheraddr,
					&arpi->host_mac, ETHER_ADDR_LEN);
				WL_INFORM(("my ether: %s\n",
					bcm_ether_ntoa(&arpi->wlcif->u.bsscfg->cur_etheraddr,
					buf)));
			} else {
				bcopy(&arpi->wlc->pub->cur_etheraddr,
					&arpi->host_mac, ETHER_ADDR_LEN);
				WL_INFORM(("my ehter: %s\n",
					bcm_ether_ntoa(&arpi->wlc->pub->cur_etheraddr, buf)));
			}
		}

		/*
		 * Requested ip-addr not found in the host_ip[] table; add it.
		 * Use link-local static address setting ONLY for testing
		 * and warn.  Link-local is used for dynamic addr configuration.
		 */
		bcopy(a, &ipaddr, IPV4_ADDR_LEN);
		if (ISLINKLOCAL_IPADDR(ipaddr)) {
			WL_ERROR(("wl%d: link local addresses being set! watch out!!\n",
			          WLCUNIT(arpi)));
			/* return BCME_BADADDR; */
		}

		/* copy the host-requested ip-addr into an empty entry of host_ip[] */
		err = BCME_NORESOURCE;

		for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
			if (IPV4_ADDR_NULL(arpi->host_ip[i].addr)) {
				bcopy(a, arpi->host_ip[i].addr, IPV4_ADDR_LEN);
				err = 0;
				break;
			}
		}

		break;
	}

	case IOV_GVAL(IOV_ARP_HOSTIP):
	{
		uint8 *hst_ip = (uint8 *)a;

		/*
		 * Return all IP addresses from host table.
		 * The return buffer is a list of valid IP addresses terminated by an address
		 * of all zeroes.
		 */

		for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
			if (!IPV4_ADDR_NULL(arpi->host_ip[i].addr)) {
				if (alen < sizeof(struct ipv4_addr))
					return BCME_BUFTOOSHORT;
				bcopy(arpi->host_ip[i].addr, hst_ip, IPV4_ADDR_LEN);
				hst_ip += IPV4_ADDR_LEN;
				alen -= sizeof(struct ipv4_addr);
			}
		}

		if (alen < sizeof(struct ipv4_addr))
			return BCME_BUFTOOSHORT;

		bzero(hst_ip, IPV4_ADDR_LEN);
		break;
	}

	case IOV_SVAL(IOV_ARP_HOSTIP_ID):
	{
		uint8 id = *((uint8 *)a + IPV4_ADDR_LEN);
		/* Add one IP address to the host IP table. */
		if (plen < sizeof(struct ipv4_addr) + sizeof(uint8))
			return BCME_BUFTOOSHORT;

		if (ETHER_ISNULLADDR(&arpi->host_mac))
			bcopy(&arpi->wlc->pub->cur_etheraddr, &arpi->host_mac, ETHER_ADDR_LEN);

		if (id >= ARP_MULTIHOMING_MAX)
			err = BCME_RANGE;
		else
			bcopy(a, arpi->host_ip[id].addr, IPV4_ADDR_LEN);

		break;
	}

	case IOV_SVAL(IOV_ARP_HOSTIP_CLEAR_ID):
	{
		uint id = *(uint *)a;
		if (id >= ARP_MULTIHOMING_MAX)
			err = BCME_RANGE;
		else
			bzero(&arpi->host_ip[id], sizeof(struct ipv4_addr));
		break;
	}

	case IOV_SVAL(IOV_ARP_STATS_CLEAR):
		bzero(&arpi->arp_ol_stats, sizeof(struct arp_ol_stats_t));
		break;

	case IOV_GVAL(IOV_ARP_STATS):
		if (alen < sizeof(struct arp_ol_stats_t))
			return BCME_BUFTOOSHORT;

		/* find number of valid arp entries */
		arpi->arp_ol_stats.arp_table_entries = 0;
		for (i = 0; i < ARP_TABLE_SIZE; i++) {
			if (ARP_ENTRY_VALID())
				arpi->arp_ol_stats.arp_table_entries++;
		}

		/* find number of valid host_ip entries */
		arpi->arp_ol_stats.host_ip_entries = 0;
		for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
			if (!IPV4_ADDR_NULL(arpi->host_ip[i].addr))
				arpi->arp_ol_stats.host_ip_entries++;
		}

		bcopy(&arpi->arp_ol_stats, a, sizeof(struct arp_ol_stats_t));
		break;

#ifdef ARP_ERRTEST
	case IOV_SVAL(IOV_ARP_ERRTEST):
		bcopy(a, &arpi->errtest, sizeof(arpi->errtest));
		break;

	case IOV_GVAL(IOV_ARP_ERRTEST):
		bcopy(&arpi->errtest, a, sizeof(arpi->errtest));
		break;
#endif // endif

	case IOV_GVAL(IOV_ARP_VERSION):
		bcopy(&arp_version, a, sizeof(uint32));
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}
static bool
lookuparptable(wl_arp_info_t *arpi, uint8 *ipa, struct ether_addr **ea)
{
	int i;
#ifdef WL_MSGBUF
	char eabuf[32], ipbuf[32];
#endif // endif

	WL_INFORM(("wl%d: lookuparptable: ip=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf)));

	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID() &&
		    (bcmp(arpi->arp_table[i].ipa.addr, ipa, IPV4_ADDR_LEN) == 0)) {
			*ea = &arpi->arp_table[i].ea;
			WL_INFORM(("wl%d: lookuparptable: found; ea=%s\n",
			           WLCUNIT(arpi),
			           bcm_ether_ntoa((struct ether_addr *)*ea, eabuf)));
			return TRUE;
		}
	}

	WL_INFORM(("wl%d: lookuparptable: not found\n", WLCUNIT(arpi)));

	*ea = NULL;
	return FALSE;
}

static bool
lookuphstcache(wl_arp_info_t *arpi, uint8 *ipa)
{
	int i;
#ifdef WL_MSGBUF
	char ipbuf[32];
#endif // endif
	uint32 ipaddr;

	bcopy(ipa, &ipaddr, IPV4_ADDR_LEN);
	if (ipaddr == 0)
		return FALSE;

	WL_INFORM(("wl%d: lookuphstcache: ip=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf)));

	for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
		if (bcmp(arpi->host_ip[i].addr, ipa, IPV4_ADDR_LEN) == 0) {
			WL_INFORM(("wl%d: lookuphstcache: found\n", WLCUNIT(arpi)));
			return TRUE;
		}
	}

	WL_INFORM(("wl%d: lookuphstcache: not found\n", WLCUNIT(arpi)));

	return FALSE;
}

static int
savehostipmac(wl_arp_info_t *arpi, uint8 *ipa, uint8 *ea)
{
	int i;
#ifdef WL_MSGBUF
	char ipbuf[32], eabuf[32];
#endif // endif

	WL_INFORM(("wl%d: savehostipmac: ip=%s ea=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf),
	           bcm_ether_ntoa((struct ether_addr *)ea, eabuf)));

	/*
	 * Allow only non-null and non-broadcast addresses to be
	 * stored in the tables
	 */
	if ((IPV4_ADDR_NULL(ipa)) || (IPV4_ADDR_BCAST(ipa)) ||
	    (ETHER_ISNULLADDR(ea)) || (ETHER_ISBCAST(ea))) {
		WL_INFORM(("wl%d: savehostipmac: null or bcast ignored\n",
		           WLCUNIT(arpi)));
		return -1;
	}

	/* The first entry to be set in the host table determines the host MAC address. */
	if (ETHER_ISNULLADDR(&arpi->host_mac)) {
		bcopy(ipa, arpi->host_ip[0].addr, IPV4_ADDR_LEN);
		bcopy(ea, &arpi->host_mac, ETHER_ADDR_LEN);
		WL_INFORM(("wl%d: savehostipmac: first entry; set ea\n",
		           WLCUNIT(arpi)));
		return 0;
	}

	/* if mac-addr is set then it should be equal to what is passed by the caller */
	if (bcmp(&arpi->host_mac, ea, ETHER_ADDR_LEN) != 0) {
		WL_ERROR(("wl%d: %s: not adding host IP with different MAC\n",
		          WLCUNIT(arpi), __FUNCTION__));
		return -1; /* mac address error */
	}

	/* see if ip address already exists in host_ip[] table */
	for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
		if (bcmp(arpi->host_ip[i].addr, ipa, IPV4_ADDR_LEN) == 0) {
			WL_INFORM(("wl%d: savehostipmac: ip-addr already in table\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	}

	/* save the ip-addr in host_ip[] table */
	for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
		if (IPV4_ADDR_NULL(arpi->host_ip[i].addr)) {
			bcopy(ipa, arpi->host_ip[i].addr, IPV4_ADDR_LEN);
			WL_INFORM(("wl%d: savehostipmac: added\n", WLCUNIT(arpi)));
			return 0;
		}
	}

	arpi->arp_ol_stats.host_ip_overflow++;

	WL_ERROR(("wl%d: %s: overflow\n", WLCUNIT(arpi), __FUNCTION__));

	return -1; /* couldn't save ip-address in the host_ip[] table */
}

static int
savearptable(wl_arp_info_t *arpi, uint8 *ipa, uint8 *ea)
{
	int i;
#ifdef WL_MSGBUF
	char eabuf[32], ipbuf[32];
#endif // endif

	WL_INFORM(("wl%d: savearptable: ip=%s ea=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf),
	           bcm_ether_ntoa((struct ether_addr *)ea, eabuf)));

	/* Allow only non-null and non-broadcast addresses to be
	 * stored in the tables
	 */
	if ((IPV4_ADDR_NULL(ipa)) || (IPV4_ADDR_BCAST(ipa)) ||
	    (ETHER_ISNULLADDR(ea)) || (ETHER_ISBCAST(ea)))
		return -1;

	/* check if ipaddr-ethaddr entry already exists in the arp-table
	 * and it is not fake arp-reply, renew the age.
	 */

	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID() &&
		    (bcmp(&arpi->arp_table[i].ea, ea, ETHER_ADDR_LEN) == 0) &&
		    (bcmp(arpi->arp_table[i].ipa.addr, ipa, IPV4_ADDR_LEN) == 0)) {

			/* Refresh the arp entry.  Ideally, should be done
			 * in rx-path ONLY but we also do it in tx arp-reply
			 * as its anyway a response to arp-request
			 * that had refreshed the arp-table "recently".
			 */
			arpi->arp_table[i].age = arpi->arp_max_age;

			WL_INFORM(("wl%d: savearptable: refreshed age of existing entry\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	}

	/* add new entry to arp-table */
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (arpi->arp_table[i].age == 0) {
			bcopy(ipa, arpi->arp_table[i].ipa.addr, IPV4_ADDR_LEN);
			bcopy(ea, &arpi->arp_table[i].ea, ETHER_ADDR_LEN);
			arpi->arp_table[i].age = arpi->arp_max_age;
			WL_INFORM(("wl%d: savearptable: added\n", WLCUNIT(arpi)));
			return 0;
		}
	}

	WL_INFORM(("wl%d: savearptable: overflow\n", WLCUNIT(arpi)));

	arpi->arp_ol_stats.arp_table_overflow++;

	return -1;

}

static bool
peer_smac_changed(wl_arp_info_t *arpi, struct bcmarp *arp)
{
	int i;
#ifdef WL_MSGBUF
	char eabuf[32], ipbuf[32];
#endif // endif

	WL_INFORM(("wl%d: peer_smac_changed: ip=%s ea=%s\n",
	           WLCUNIT(arpi),
	           bcm_ether_ntoa((struct ether_addr *)arp->src_eth, eabuf),
	           bcm_ip_ntoa((struct ipv4_addr *)arp->src_ip, ipbuf)));

	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID() && (bcmp(arpi->arp_table[i].ipa.addr,
		                               arp->src_ip, IPV4_ADDR_LEN) == 0)) {
			if (bcmp(&arpi->arp_table[i].ea, arp->src_eth,
			         ETHER_ADDR_LEN) != 0) {
				WL_INFORM(("wl%d: update existing entry\n", WLCUNIT(arpi)));
				/* rfc2002.  Update arp-table with new mac address */
				bcopy(arp->src_eth, &arpi->arp_table[i].ea, ETHER_ADDR_LEN);
				return TRUE;
			}
		}
	}

	WL_INFORM(("wl%d: peer_smac_changed: no change\n", WLCUNIT(arpi)));

	return FALSE;
}

static int
arp_reply_host(wl_arp_info_t *arpi, struct bcmarp *arp_req, struct ether_addr *dst_mac, bool snap)
{
	void *pkt;
	uint8 *frame;
	struct bcmarp *arp_reply;
	uint16 pktlen = (ETHER_HDR_LEN +
	                 ARP_DATA_LEN +
	                 ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	uint32 ipaddr;
	uint16 header_overhead = BCMDONGLEOVERHEAD*3;

	WL_INFORM(("wl%d: arp_reply_host()\n", WLCUNIT(arpi)));

	if (!(pkt = PKTGET(WLCOSH(arpi), pktlen + ALIGN_ADJ_BUFLEN + header_overhead, FALSE))) {
		WL_ERROR(("wl%d: cannot do local arp-resp: unable to alloc\n",
		          WLCUNIT(arpi)));
		WLCNTINCR(arpi->wlc->pub->_cnt->rxnobuf);
		return -1;
	}

	/*
	 * Adjust for pkt alignment problems when pkt is pulled by
	 * 14bytes of ETHER_HDR_LEN in linux osl
	 */
	PKTPULL(WLCOSH(arpi), pkt, ALIGN_ADJ_BUFLEN + header_overhead);

	WL_INFORM(("wl%d: arp_reply_host: servicing request from host\n", WLCUNIT(arpi)));

	frame = PKTDATA(WLCOSH(arpi), pkt);

	/* Create 14-byte eth header, plus snap header if applicable */

	/* broadcast reply if ip-addr is link-local */
	bcopy(arp_req->dst_ip, &ipaddr, IPV4_ADDR_LEN);

	/*  XXX : do not use broadcast ARP reply due to customer request
	 *	ARP reply should use broadcast address
	 *	if it's IP address is one of link local (i.e. 169.254.xxx.xxx)
	 *	Customer request to use unicast address even it is not compliant to RFC3927.
	 *	modify ARP response logic as following
	 */
	if (ISLINKLOCAL_IPADDR(ipaddr))
		bcopy(&ether_bcast, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	else
		bcopy(arp_req->src_eth, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);

#ifdef ARP_ERRTEST
	/* Corrupt peer mac-addr for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_HOST)
		dst_mac->octet[0]++;
#endif // endif

	bcopy(dst_mac, frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);

#ifdef ARP_ERRTEST
	/* Restore corrupted peer mac-addr for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_HOST)
		dst_mac->octet[0]--;
#endif // endif

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

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("arp_reply_host: response packet", WLCOSH(arpi), pkt);
#endif // endif

	arpi->arp_ol_stats.host_service++;

	arpi->fake_arp_reply = TRUE;
	wl_sendup(arpi->wlc->wl, ((arpi->wlcif != NULL) ? arpi->wlcif->wlif : NULL), pkt, 1);
	arpi->fake_arp_reply = FALSE;

	return ARP_REPLY_HOST;
}

/* PCB function for the ARP response sent by dongle */
static void
wlc_send_arp_reply_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wl_arp_info_t *arpi = (wl_arp_info_t *)arg;

	/* decement the ARP packet counter after getting do txstatus */
	if (arpi->max_arps) {
		arpi->max_arps--;
	}
}

static int
arp_reply_peer(wl_arp_info_t *arpi, struct bcmarp *arp_req, bool snap)
{
	void *pkt;
	uint8 *frame;
	struct bcmarp *arp_reply;
	wlc_info_t *wlc = arpi_p->wlc;
	bool err = FALSE;
	uint16 pktlen = (ETHER_HDR_LEN +
	                 ARP_DATA_LEN +
	                 ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	uint32 ipaddr;

	WL_INFORM(("wl%d: arp_reply_peer()\n", WLCUNIT(arpi)));

	/* At a given point we do not want to support more than 2 ARP requests
	* It might deplete our pktid pools and heap as well. Hence avoiding it.
	* JIRA REFERENCE: SWWLAN-141380 (4361B0 PCIE)
	*/
	if (arpi->max_arps >= MAX_ARP_REQUESTS_SUPPORTED) {
		arpi->arp_ol_stats.peer_request_drop++;
		WL_ERROR(("wl%d: %s: dropped ARP exceeded arp requests %d\n",
		          WLCUNIT(arpi), __FUNCTION__, arpi->max_arps));
		return ARP_REQ_SINK;
	}

	/* Allocate extra tx headroom offset space */
	if (!(pkt = PKTGET(WLCOSH(arpi), wlc->txhroff + pktlen, TRUE))) {
		WL_ERROR(("wl%d: %s: alloc failed; dropped\n",
		          WLCUNIT(arpi), __FUNCTION__));
		WLCNTINCR(wlc->pub->_cnt->rxnobuf);
		/* Force to forward frame to handle it at host side */
		return ARP_FORCE_FORWARD;
	}

	/* pcb = packet tx complete callback management */
	if (wlc_pcb_fn_register(wlc->pcb, wlc_send_arp_reply_complete, arpi, pkt)) {
		PKTFREE(WLCOSH(arpi), pkt, FALSE);
		return ARP_REQ_SINK;
	}

	arpi->max_arps++;

	PKTPULL(WLCOSH(arpi), pkt, wlc->txhroff);

	WL_INFORM(("wl%d: arp_reply_peer: servicing request from peer(%d)\n",
		WLCUNIT(arpi), arpi->arp_ol_stats.peer_service));

	frame = PKTDATA(WLCOSH(arpi), pkt);

	/* Create 14-byte eth header, plus snap header if applicable */

	/* broadcast reply if ip-addr is link-local */
	bcopy(arp_req->src_ip, &ipaddr, IPV4_ADDR_LEN);

	/*  XXX : do not use broadcast ARP reply due to customer request
	 *	ARP reply should use broadcast address
	 *	if it's IP address is one of link local (i.e. 169.254.xxx.xxx)
	 *	Customer request to use unicast address even it is not compliant to RFC3927.
	 *	modify ARP response logic as following
	 */
	if (ISLINKLOCAL_IPADDR(ipaddr))
		bcopy(&ether_bcast, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	else {
		/* If ether address passed is 00:00:00:00:00:00, change to broadcast.
		 * This is needed for case of sending GARP
		 */
		if (!bcmp(arp_req->src_eth, &ether_null, ETHER_ADDR_LEN))
			bcopy(&ether_bcast, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
		else
			bcopy(arp_req->src_eth, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	}

#ifdef ARP_ERRTEST
	/* corrupt host mac addr for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_PEER)
		arpi->host_mac.octet[0]++;
#endif // endif

	bcopy(&arpi->host_mac, frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);

#ifdef ARP_ERRTEST
	/* Restore hosts corrupted mac-addr used for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_PEER)
		arpi->host_mac.octet[0]--;
#endif // endif

	if (snap) {
		hton16_ua_store(pktlen, frame + ETHER_TYPE_OFFSET);
		bcopy(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN);
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_HDR_LEN + SNAP_HDR_LEN);
	} else
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_TYPE_OFFSET);

	/* move past eth/eth-snap header */
	arp_reply = (struct bcmarp *)(frame + pktlen - ARP_DATA_LEN);

	/* Create 28-byte arp-reply data frame */

	/* copy first 6 bytes as-is: htype, ptype, hlen, plen */
	bcopy(arp_req, arp_reply, ARP_OPC_OFFSET);
	arp_reply->oper = HTON16(ARP_OPC_REPLY);

	/* Copy dst eth and ip addresses */
	bcopy(&arpi->host_mac, arp_reply->src_eth, ETHER_ADDR_LEN);

	bcopy(arp_req->dst_ip, arp_reply->src_ip, IPV4_ADDR_LEN);
	bcopy(arp_req->src_eth, arp_reply->dst_eth, ETHER_ADDR_LEN);
	bcopy(arp_req->src_ip, arp_reply->dst_ip, IPV4_ADDR_LEN);

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("arp_reply_peer: response packet", WLCOSH(arpi), pkt);
#endif // endif

	arpi->arp_ol_stats.peer_service++;

	err = wlc_sendpkt(wlc, pkt, arpi->wlcif);
	/* Force to forward frame to handle it at host side */
	if (err == TRUE) {
		WL_ERROR(("wl%d: %s: ARPoe failed\n",
		          WLCUNIT(arpi), __FUNCTION__));
		return ARP_FORCE_FORWARD;
	}

	return ARP_REPLY_PEER;
}

/* Returns -1 if frame is not ARP; Returns -2, if truncated ARP packet,
 * otherwise, returns pointer/length of IP portion
 */
static int
wl_arp_parse(wl_arp_info_t *arpi, void *sdu,
             struct bcmarp **arp_out, bool *snap_out)
{
	uint8 *frame = PKTDATA(WLCOSH(arpi), sdu);
	int length = PKTLEN(WLCOSH(arpi), sdu);
	int32 length_cur_sdu;
	struct lbuf *next_lb = PKTNEXT(WLCOSH(arpi), sdu);
	uint8 *pt;
	uint16 ethertype;
	struct bcmarp *arp;
	int arplen;
	bool snap = FALSE;
#ifdef WL_MSGBUF
	char ipbuf[32], eabuf[32];
#endif // endif

	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (length < ETHER_HDR_LEN) {
		WL_INFORM(("wl%d: %s: short eth frame (%d)\n",
		          WLCUNIT(arpi), __FUNCTION__, length));
			return NON_ARP;
		} else if (ntoh16_ua((const void *) (frame + ETHER_TYPE_OFFSET))
			>= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		WL_INFORM(("wl%d: wl_arp_parse: 802.3 LLC/SNAP\n", WLCUNIT(arpi)));
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
		snap = TRUE;
	} else {
		WL_TRACE(("wl%d: %s: non-SNAP 802.3 frame\n",
			WLCUNIT(arpi), __FUNCTION__));
		return NON_ARP;
	}

	ethertype = ntoh16_ua((const void *)pt);

	/* Skip VLAN tag, if any */
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			WL_ERROR(("wl%d: %s: short VLAN frame (%d)\n",
			          WLCUNIT(arpi), __FUNCTION__, length));
			return NON_ARP;
		}

		ethertype = ntoh16_ua((const void *)pt);
	}

	if (ethertype != ETHER_TYPE_ARP) {
		WL_INFORM(("wl%d: wl_arp_parse: non-ARP frame (ethertype 0x%x, length %d)\n",
		           WLCUNIT(arpi), ethertype, length));
		return NON_ARP;
	}

	/* Compute length of current pkt */
	length_cur_sdu = PKTLEN(WLCOSH(arpi), sdu);

	/*
	  If we reach here, it implies that current pkt contains atleast ETH_HDR.
	    Thus, if current pkt length != (ETHER_HDR_LEN + ARP_DATA_LEN),
	    then we know that a next chained pkt exists, as ARP has to be
	    completely in next pkt, or split across both pkts
	    - use this to increment total length
	 */
	if (next_lb)
			length += PKTLEN(WLCOSH(arpi), next_lb);

	/*
	  If Length of current pkt == ETHER_HDR_LEN, it implies that ARP is completely located in
	  next pkt
	*/
	if (next_lb && (length_cur_sdu == ETHER_HDR_LEN)) {
		/* Compute pointer and length based on start and size of next pkt only */
		arp = (struct bcmarp *)(PKTDATA(WLCOSH(arpi), next_lb));
		arplen = PKTLEN(WLCOSH(arpi), next_lb);
	}
	/*
	  If Length of current pkt is between
	  (ETHER_HDR_LEN, ETHER_HDR_LEN+ARP_DATA_LEN), it implies
	  that ARP is split across pkts
	*/
	else if (next_lb &&
		((length_cur_sdu > ETHER_HDR_LEN) &&
		(length_cur_sdu < (ETHER_HDR_LEN + ARP_DATA_LEN)))) {
		/* Not handled for now - need to update */
		WL_ERROR(("wl%d: %s: ARP packet split across SDUs, case not handled \n",
			WLCUNIT(arpi), __FUNCTION__));
		ASSERT((length_cur_sdu > ETHER_HDR_LEN) &&
			(length_cur_sdu < (ETHER_HDR_LEN + ARP_DATA_LEN)));
		return NON_ARP;
	}
	/*
	  If Length of current pkt == (ETHER_HDR_LEN + ARP_DATA_LEN), it implies that ARP is
	  completely located in current pkt
	*/
	else {
		/*
		  Compute pointer and length based on start and size of current pkt -
		  implies that Ether hdr needs to be skipped
		*/
		arp = (struct bcmarp *)(pt + ETHER_TYPE_LEN);
		arplen = length - (pt + ETHER_TYPE_LEN - frame);
	}

	if (arplen > ARP_DATA_LEN) {
		/* Sometimes there are pad bytes */
		WL_INFORM(("wl%d: %s: extra frame length ignored (%d)\n",
		          WLCUNIT(arpi), __FUNCTION__, arplen - ARP_DATA_LEN));
		arplen = ARP_DATA_LEN;
	} else if (arplen < ARP_DATA_LEN) {
		WL_ERROR(("wl%d: %s: truncated ARP packet (%d)\n",
		          WLCUNIT(arpi), __FUNCTION__, arplen));
		return TRUNCATED_ARP;
	}

	/* non probe pkt; could be link-local or routable arp-request */

	WL_INFORM(("wl%d: wl_arp_parse: ARP %s, %sSNAP\n",
	           WLCUNIT(arpi),
	           (arp->oper == HTON16(ARP_OPC_REQUEST)) ? "request" : "response",
	           snap ? "" : "non-"));
	WL_INFORM(("wl%d: wl_arp_parse:   SA %s  SIP %s\n",
	           WLCUNIT(arpi),
	           bcm_ether_ntoa((struct ether_addr *)arp->src_eth, eabuf),
	           bcm_ip_ntoa((struct ipv4_addr *)arp->src_ip, ipbuf)));
	WL_INFORM(("wl%d: wl_arp_parse:   DA %s  DIP %s\n",
	           WLCUNIT(arpi),
	           bcm_ether_ntoa((struct ether_addr *)arp->dst_eth, eabuf),
	           bcm_ip_ntoa((struct ipv4_addr *)arp->dst_ip, ipbuf)));

	*arp_out = arp;
	*snap_out = snap;

	return arplen;
}

/*
 * Check ARP frames whether is truncated ARP packet.
 *
 * For PCIE, only Ethernet header can read in transmit direction. We need to fetch its
 * contents for APROE application.
 *
 * Return value:
 *	TRUE	truncated ARP, packet fetch required
 *	FALSE	no packet fetch required
 */
bool
wl_arp_send_pktfetch_required(wl_arp_info_t *arpi, void *sdu)
{
	struct bcmarp *arp;
	bool snap;

	return (wl_arp_parse(arpi, sdu, &arp, &snap) == TRUNCATED_ARP);
}

/*
 * Process ARP frames in transmit direction.
 *
 * xxx fixme - need well-defined set of return codes that inform upper layer what
 * xxx   happened and whether to discard the incoming packet instead of forwarding.
 * Return value:
 *	0		ARP processing not enabled
 *	-1		Packet parsing error
 */
int
wl_arp_send_proc(wl_arp_info_t *arpi, void *sdu)
{
	struct bcmarp *arp;
	bool snap;
	bool sip_in_arp_table, dip_in_arp_table, sip_in_host_cache, dip_in_host_cache;
	uint32 sat_dat_shc_dhc;
	struct ether_addr *dst_mac, *src_mac;
	uint32 ipaddr;

	WL_INFORM(("wl%d: wl_arp_send_proc()\n", WLCUNIT(arpi)));

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("wl_arp_send_proc: arp txpkt (MPDU)", WLCOSH(arpi), sdu);
#endif // endif

	/* Parse ARP packet and do table lookups */
	if (wl_arp_parse(arpi, sdu, &arp, &snap) < 0)
		return -1;

	/*
	 * Create a 4-bit value by collating return values if sip/dip exists in arp-table[]
	 * or host_ip[] table.  We will switch case on these 16 values to take various actions.
	 */

	sip_in_arp_table = lookuparptable(arpi, arp->src_ip, &src_mac);		/* sat */
	dip_in_arp_table = lookuparptable(arpi, arp->dst_ip, &dst_mac);		/* dat */
	sip_in_host_cache = lookuphstcache(arpi, arp->src_ip);			/* shc */
	dip_in_host_cache = lookuphstcache(arpi, arp->dst_ip);			/* dhc */

	sat_dat_shc_dhc = ((sip_in_arp_table ? SAT : 0) +
	                   (dip_in_arp_table ? DAT : 0) +
	                   (sip_in_host_cache ? SHC : 0) +
	                   (dip_in_host_cache ? DHC : 0));

	if (arp->oper == HTON16(ARP_OPC_REQUEST)) {

		WL_INFORM(("wl%d: wl_arp_send_proc: arp-tx-request\n", WLCUNIT(arpi)));
		arpi->arp_ol_stats.host_request++;

		/* sip for link-local arp-probe is 0.0.0.0, 3 such probes are expected in
		 * 3 seconds.
		 */
		bcopy(arp->src_ip, &ipaddr, IPV4_ADDR_LEN);
		if (ipaddr == ARP_PROBE_SIP) {
			WL_INFORM(("wl%d: wl_arp_send_proc: tx-arp-probe for link-local ip\n",
			           WLCUNIT(arpi)));
			return 0;
		}

		switch (sat_dat_shc_dhc) {
		case 0:
			if (m_ol_cmpnts & ARP_OL_SNOOP) {
				if (savehostipmac(arpi, arp->src_ip, arp->src_eth) != 0)
					return -1; /* bypass complete arp-agent */

				WL_INFORM(("wl%d: wl_arp_send_proc: saved host-ip/mac\n",
				           WLCUNIT(arpi)));
				return 0;
			}
			return 0;
		case DAT:
			if (m_ol_cmpnts & ARP_OL_SNOOP) {
				if (savehostipmac(arpi, arp->src_ip, arp->src_eth) == 0) {
					WL_INFORM(("wl%d: wl_arp_send_proc: saved host-ip/mac\n",
					           WLCUNIT(arpi)));
					if (m_ol_cmpnts & ARP_OL_HOST_AUTO_REPLY)
						return arp_reply_host(arpi, arp, dst_mac, snap);
				} else
					return -1; /* bypass complete arp-agent */
			}
			return 0;

		case DAT + SHC:
			if (m_ol_cmpnts & ARP_OL_HOST_AUTO_REPLY) {
				WL_INFORM(("wl%d: wl_arp_send_proc: "
				           "dip in arp table, sip in hostcache \n",
				           WLCUNIT(arpi)));
				return arp_reply_host(arpi, arp, dst_mac, snap);
			}
			/* Fall Through */

		default:
			WL_INFORM(("wl%d: wl_arp_send_proc: conflicting arp-req case\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	} else if (arp->oper == HTON16(ARP_OPC_REPLY)) {

		WL_INFORM(("wl%d: wl_arp_send_proc: arp-tx-reply\n", WLCUNIT(arpi)));
		arpi->arp_ol_stats.host_reply++;

		switch (sat_dat_shc_dhc) {
		case 0:
			if (m_ol_cmpnts & ARP_OL_SNOOP) {
				if (savehostipmac(arpi, arp->src_ip, arp->src_eth) != 0) {
					return -1;
				}
			}

			if (savearptable(arpi, arp->dst_ip, arp->dst_eth) != 0)
				WL_ERROR(("wl%d: %s: savearptable unsuccessful!\n",
				          WLCUNIT(arpi), __FUNCTION__));

			return 0; /* Behaves as if arp-table was not there */

		case SHC:
			savearptable(arpi, arp->dst_ip, arp->dst_eth);
			return 0;
		case DAT:
			if (m_ol_cmpnts & ARP_OL_SNOOP) {
				if (savehostipmac(arpi, arp->src_ip, arp->src_eth) != 0) {
					return -1;
				}
			}

			return 0;
		default:
			WL_INFORM(("wl%d: wl_arp_send_proc: conflicting arp-reply case\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	} else {
		WL_ERROR(("wl%d: %s: unknown ARP oper 0x%x\n",
		          WLCUNIT(arpi), __FUNCTION__, ntoh16_ua((const void *)&arp->oper)));
		return -1;
	}

	return 0;
}

/*
 * Process ARP frames in receive direction.
 *
 * xxx fixme - need well-defined set of return codes that inform upper layer what
 * xxx   happened and whether to discard the incoming packet instead of forwarding.
 * Return value:
 *	0		ARP processing not enabled
 *	-1		Packet parsing error
 *	ARP_REQ_SINK	???
 */
int
wl_arp_recv_proc(wl_arp_info_t *arpi, void *sdu)
{
	struct bcmarp *arp;
	bool snap;
	bool sip_in_arp_table, dip_in_arp_table, sip_in_host_cache, dip_in_host_cache;
	uint32 sat_dat_shc_dhc;
	struct ether_addr *dst_mac, *src_mac;
	uint32 ipaddr;
	WL_INFORM(("wl%d: wl_arp_recv_proc()\n", WLCUNIT(arpi)));

	if ((m_ol_cmpnts & ARP_OL_AGENT) == 0 || arpi->wlc == NULL)
		return -1;

	/* Parse ARP packet and do table lookups */
	if (wl_arp_parse(arpi, sdu, &arp, &snap) < 0)
		return -1;
	/* Give fake arp-reply to host directly without getting into arp-offload-engine. */
	if (arpi->fake_arp_reply == TRUE)
		return 0;

	/*
	 * Create a 4-bit value by collating return values if sip/dip exists in arp-table[]
	 * or host_ip[] table.  We will switch case on these 16 values to take various actions.
	 */

	sip_in_arp_table = lookuparptable(arpi, arp->src_ip, &src_mac);		/* sat */
	dip_in_arp_table = lookuparptable(arpi, arp->dst_ip, &dst_mac);		/* dat */
	sip_in_host_cache = lookuphstcache(arpi, arp->src_ip);			/* shc */
	dip_in_host_cache = lookuphstcache(arpi, arp->dst_ip);			/* dhc */

	sat_dat_shc_dhc = ((sip_in_arp_table ? SAT : 0) +
	                   (dip_in_arp_table ? DAT : 0) +
	                   (sip_in_host_cache ? SHC : 0) +
	                   (dip_in_host_cache ? DHC : 0));

	bcopy(arp->src_ip, &ipaddr, IPV4_ADDR_LEN);

	if (arp->oper == HTON16(ARP_OPC_REQUEST)) {
		WL_INFORM(("wl%d: wl_arp_recv_proc: arp-rx-request\n", WLCUNIT(arpi)));
		arpi->arp_ol_stats.peer_request++;
		WL_INFORM(("wl%d: wl_arp_recv_proc: arp-rx-request(%d)\n", WLCUNIT(arpi),
			arpi->arp_ol_stats.peer_request));
		/* If host_mac value is not set, should not arp_reply_peer() */
		if (ETHER_ISNULLADDR(&arpi->host_mac))
			return ARP_FORCE_FORWARD;

		/*
		 * Multi-homing host sends arp-req from its primary ipaddr,
		 * not extra multihomed addresses, resulting in host_ip[] table
		 * having only ONE ipaddr. This table will be updated with
		 * multi-homed address when peer tries to talk to host's
		 * multi-homed addresses and then host sends an arp-req/resp.
		 */
		switch (sat_dat_shc_dhc) {
/*
 * Host to accept unknown arp requests which need not be destined to it or
 * host_ip[] table is empty.  Flag ARP_OL_SNOOP which is normally used in
 * tx direction is overloaded for this purpose in rx direction.
 */
#define ARP_ACCEPT_UNKNOWN	ARP_OL_SNOOP
		case 0:
			if (m_ol_cmpnts & ARP_ACCEPT_UNKNOWN)
				return 0;
			else {
				arpi->arp_ol_stats.peer_request_drop++;
				return ARP_REQ_SINK;
			}

		case DHC:
			savearptable(arpi, arp->src_ip, arp->src_eth);
			WL_INFORM(("wl%d: wl_arp_recv_proc: saved peer-ip/mac\n", WLCUNIT(arpi)));
			if (m_ol_cmpnts & ARP_OL_PEER_AUTO_REPLY) {
				arp_reply_peer(arpi, arp, snap);
			}

			/* We received an ARP probe using our IP address.  Generate
			 * a gratuitous ARP
			 */
			if (ipaddr == 0) {
				bcopy(arp->dst_ip, arp->src_ip, IPV4_ADDR_LEN);
				bcopy(&ether_null, arp->src_eth, ETHER_ADDR_LEN);
				arp_reply_peer(arpi, arp, snap);
			}

			/* Give the arp-request pkt to the host so that he updates his arp-table */
			return 0;

		case DAT:
			/*
			 * rfc2002, dont care about the dip, check only sip in arp-table.
			 * Discard ARP from known peer but not to our host
			 */
			arpi->arp_ol_stats.peer_request_drop++;
			return ARP_REQ_SINK;

		case SAT:
		case DAT + SAT:
			/*
			 * -Gratuitous arps go to host to update host's arpcache.
			 * -Arps with multihoming ON (ARP_ACCEPT_UNKNOWN) also go to host.
			 * -Arps where 2 peers are talking to eachother and multihoming OFF
			 * get dropped.
			 */
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;

			if (bcmp(arp->src_ip, arp->dst_ip, IPV4_ADDR_LEN) &&
				!(m_ol_cmpnts & ARP_ACCEPT_UNKNOWN)) {
				arpi->arp_ol_stats.peer_request_drop++;
				return ARP_REQ_SINK;
			} else
				return 0;

		case SAT + DHC:
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;	/* Give request to host to update his arp-table */

			/* don't check peer_dmac_changed() in arp-req-received as the
			 * dmac will be 0
			 */
			return ((m_ol_cmpnts & ARP_OL_PEER_AUTO_REPLY) ?
			        arp_reply_peer(arpi, arp, snap) : 0);
		default:
			WL_INFORM(("wl%d: wl_arp_recv_proc: conflicting arp-req case\n",
			           WLCUNIT(arpi)));
			return 0;
		}

	}
	else if (arp->oper == HTON16(ARP_OPC_REPLY)) {

		arpi->arp_ol_stats.peer_reply++;
		WL_INFORM(("wl%d: wl_arp_recv_proc: arp-rx-reply(%d)\n",
			WLCUNIT(arpi), arpi->arp_ol_stats.peer_reply));

		switch (sat_dat_shc_dhc) {

		case 0:
			/*
			 * Drop arp-reply between 2 peers, linklocal reply is broadcast.
			 *
			 * Caution : If, for linklocal host,
			 * -'wl arp_*_clear' are used after the connection was established and
			 * - host's arp-cache is cleared and
			 * - snooping is OFF,
			 * connection will not go through.  The arp-reply from peer will be dropped.
			 */

			if (ISLINKLOCAL_IPADDR(ipaddr)) {
				if (m_ol_cmpnts & ARP_ACCEPT_UNKNOWN)
					return 0;
				else {
					arpi->arp_ol_stats.peer_reply_drop++;
					return ARP_REQ_SINK;
				}
			} else
				return 0;

		case DHC:
			/*
			 * dip is present in host_ip table, not in arp-table.
			 * sip isn't in host_ip cache nor in arp-table.
			 *
			 * check for mac-id match. if it matches, update the arp-table
			 * with sip, else conflict.
			 */
			if (bcmp(arp->dst_eth, &arpi->host_mac, ETHER_ADDR_LEN) == 0)
				savearptable(arpi, arp->src_ip, arp->src_eth);

			return 0;

		case SAT:
			/* sip in arp-table.  link-local reply; conflict if mac-id is not same. */
			/* fall through */
		case SAT + DHC:
			/* sip in arp-table, dip in host_ip[] table */
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;
			/* entry is already in arp-table; refresh age */
			savearptable(arpi, arp->src_ip, arp->src_eth);
			return 0;

		case SAT + DAT:
			/* 2 link-local peers talking to each other; arp-reply not for us */
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;

			arpi->arp_ol_stats.peer_reply_drop++;
			return ARP_REQ_SINK;

		case DAT + SHC:
			/*
			 * sip in host-cache & dip in ARP table.
			 * this is our own arp reply to a peer in the same BSS
			 * and since promisc is enabled packet reached till here.
			 * no need to forward this packet to host.
			 */
			return ARP_REQ_SINK;

		default:
			WL_INFORM(("wl%d: wl_arp_recv_proc: conflicting arp-reply case\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	} else {
		WL_ERROR(("wl%d: %s: unknown ARP oper 0x%x\n",
		          WLCUNIT(arpi), __FUNCTION__, ntoh16_ua((const void *)&arp->oper)));
		return -1;
	}

	return 0;
}

/* called when a new virtual IF is created.
 *	i/p: primary ARPIIF [arpi_p] and the new wlcif,
 *	o/p: new arpi structure populated with inputs and
 *		the global parameters duplicated from arpi_p
 *	side-effects: arpi for a new IF will inherit properties of arpi_p till
 *		the point new arpi is created. After that, for any change in
 *		arpi_p will NOT change the arpi corr to new IF. To change property
 *		of new IF, wl -i wl0.x has to be used.
*/
wl_arp_info_t *
wl_arp_alloc_ifarpi(wl_arp_info_t *arpi_p, wlc_if_t *wlcif)
{
	wl_arp_info_t *arpi;
	wlc_info_t *wlc = arpi_p->wlc;

	/* allocate arp private info struct */
	arpi = MALLOCZ(wlc->osh, sizeof(wl_arp_info_t));
	if (!arpi) {
		WL_ERROR(("wl%d: wl_arp_alloc_ifarpi: MALLOCZ failed; total mallocs %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init arp private info struct */
	arpi->wlc = wlc;
	arpi->wlcif = wlcif;
	arpi->osh = wlc->osh;

	bcopy(&wlcif->u.bsscfg->cur_etheraddr, &arpi->host_mac, ETHER_ADDR_LEN);
	/* copy global info from primary arpi structure */
	arpi->arp_max_age = arpi_p->arp_max_age;
	arpi->fake_arp_reply = arpi_p->fake_arp_reply;

	return arpi;
}

void
wl_arp_free_ifarpi(wl_arp_info_t *arpi)
{
	if (arpi != NULL)
		MFREE(arpi->osh, arpi, sizeof(wl_arp_info_t));
}

/* check if arp offload is enabled */
bool
wl_arp_components_enab(void)
{
	if ((m_ol_cmpnts & ARP_OL_AGENT) == 0 ||
	    (!(m_ol_cmpnts & ARP_OL_HOST_AUTO_REPLY) && !(m_ol_cmpnts & ARP_OL_SNOOP))) {
		return FALSE;
	}
	return TRUE;
}
