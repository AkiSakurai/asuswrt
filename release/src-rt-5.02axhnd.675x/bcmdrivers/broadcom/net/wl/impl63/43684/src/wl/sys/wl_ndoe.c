/*
 * Neighbor Advertisement Offload
 *
 * @file
 * @brief
 * The dongle should be able to handle Neighbor Solicitation request and reply with Neighbor
 * Advertisement without having to wake up the host.
 *
 * The code below implements the Neighbor Advertisement Offload.
 * It supports multihoming hosts and link-local addressing.
 *
 * Supported protocol families: IPV6.
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
 * $Id: wl_ndoe.c 774257 2019-04-17 10:08:19Z $
 */

/**
 * @file
 * @brief
 * IPv6 implements Neighbor Solicitation and Neighbor Advertisement to determine/advertise the link
 * layer address of a node (similar to ARP in IPv4). NS packets are multicast packets and are
 * received by multiple hosts on the same subnet, each demands processing by the host requiring it
 * to wake up. This features implements the neighbor advertisement in the wl to save power by
 * avoiding some of host wake-ups.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [NeighborSolicitationOffload]
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <ethernet.h>
#include <802.3.h>
#include <bcmip.h>
#include <bcmipv6.h>
#include <bcmendian.h>
#include <d11.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_rate.h>
#include <wlc.h>
#include <wl_export.h>
#include <wlc_bsscfg.h>
#include <wl_ndoe.h>

#if defined(BCMDBG) || defined(WLMSG_INFORM)
#define WL_MSGBUF
#endif // endif

#ifdef WLNDOE_RA
#define WL_ND_MAX_RA_NODES 10
typedef struct wl_nd_ra_node {
	void *cached_packet;
	uint32 cached_packet_len;
	uint32 count;
	uint8 sa_ip[IPV6_ADDR_LEN];
	uint32	time_stamp;
} wl_nda_node_t;

typedef struct wl_nd_ra_info {
	struct wl_nd_ra_node nodes[WL_ND_MAX_RA_NODES];
	uint8 ra_filter_enable;
	uint8 ra_filter_ucast;
	uint8 ring_index;
} wl_nd_ra_info_t;
#endif /* WLNDOE_RA */

/* Neighbor Discovery private info structure */
struct wl_nd_info {
	wlc_info_t			*wlc;		/* Pointer back to wlc structure */
	struct ipv6_addr	host_ip[ND_MULTIHOMING_MAX];
	struct ipv6_addr	solicit_ip;	/* local Solicit Ip Address */
	uint8				host_mac[ETHER_ADDR_LEN];
	struct wlc_if 		*wlcif;
	struct ipv6_addr	remote_ip;	/* Win8 specific */
	struct nd_ol_stats_t	nd_ol_stats;
	bool pkt_snooping;
	nd_param_t				param[ND_REQUEST_MAX];
	uint8					req_count;
#ifdef WLNDOE_RA
	struct wl_nd_ra_info *ra_info;
#endif // endif
	/* type of address in host_ip[] table (unicast or anycast) */
	uint8 *host_ip_addr_type;
	struct ipv6_addr host_ll_ip;	/* Host link-local IP */
	uint32 unsolicited_na_filter;	/* Unsolicited NA filtering enable/disable */
};

#ifdef WLNDOE_RA
static int
wl_nd_ra_filter_init(struct wl_nd_info *ndi);
static int
wl_nd_ra_filter_deinit(struct wl_nd_info *ndi);
static int
wl_nd_ra_intercept_packet(struct wl_nd_info *ndi, void *sdu, uint8 *sa, uint8 *da);
#endif /* WLNDOE_RA */

/* forward declarations */
static int nd_doiovar(void *hdl, uint32 actionid,
                       void *p, uint plen, void *a, uint alen,
                       uint vsize, struct wlc_if *wlcif);

static int nd_add_host_ip_address(wl_nd_info_t *ndi, struct ipv6_addr *ip_addr, uint8 type);
static int nd_del_host_ip_address(wl_nd_info_t *ndi, struct ipv6_addr *ip_addr, uint8 type);
static int ipv6_link_local_addr(struct ipv6_addr *ip_addr, uint8 *host_mac);

static int wl_ns_parse(wl_nd_info_t *ndi, void *sdu, bool sentby_host);

static int na_reply_peer(wl_nd_info_t *ndi, struct bcm_nd_msg *ns_req,
	struct nd_msg_opt *options, struct ether_header *eth_hdr,
	struct ipv6_hdr *ip_hdr, bool snap, uint8 req_index);

/* wlc_pub_t struct access macros */
#define WLCUNIT(ndi)	((ndi)->wlc->pub->unit)
#define WLCOSH(ndi)		((ndi)->wlc->osh)

/* special values */
/* 802.3 llc/snap header */
static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};
static uint16 csum_ipv6(wl_nd_info_t *ndi, uint8 *saddr, uint8 *daddr,
	uint8 proto, uint8 *buf, uint32 buf_len);
static uint32 csum_partial_16(uint8 *nptr, int nlen, uint32 x);

/* IOVar table */
enum {
	IOV_ND_HOSTIP,			/* Add/remove local ip address to/from host_ip[] table */
	IOV_ND_HOSTIP_CLEAR,		/* Clear all entries in the host_ip[] table */
	IOV_ND_MAC_ADDRESS,		/* Set MAC address */
	IOV_ND_REMOTEIP,		/* Remote IP Win8 Req */
	IOV_ND_STATUS,
	IOV_ND_SNOOP,			/* Enable/Dis NA packet SNOOPING */
	IOV_ND_SOLICITIP,
	IOV_ND_STATUS_CLEAR,		/* clears all the ND status counters */
	IOV_ND_SET_PARAM,
	IOV_ND_CLEAR_PARAM,
	IOV_ND_RA_FILTER,
	IOV_ND_RA_FILTER_UCAST,
	IOV_ND_RA_FILTER_CACHE_CLEAR,
	IOV_NDOE,			/* Neighbor Advertisement Offload for ipv6 */
	IOV_ND_UNSOLCITED_NA_FILTER
};

static const bcm_iovar_t nd_iovars[] = {
	{"nd_hostip", IOV_ND_HOSTIP,
	(0), 0, IOVT_BUFFER, WL_ND_HOSTIP_FIXED_LEN,
	},
	{"nd_hostip_clear", IOV_ND_HOSTIP_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
	{"nd_macaddr", IOV_ND_MAC_ADDRESS,
	(0), 0, ETHER_ADDR_LEN, 0
	},
	{"nd_status", IOV_ND_STATUS,
	(0), 0, IOVT_BUFFER, sizeof(struct nd_ol_stats_t)
	},
	{"nd_snoop", IOV_ND_SNOOP,
	(0), 0, IOVT_BOOL, 0
	},
	{"nd_status_clear", IOV_ND_STATUS_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
	{"nd_set_param", IOV_ND_SET_PARAM,
	(0), 0, IOVT_BUFFER, sizeof(nd_param_t)
	},
	{"nd_clear_param", IOV_ND_CLEAR_PARAM,
	(0), 0, IOVT_UINT32, sizeof(uint32)
	},
	{"nd_ra_filter_enable", IOV_ND_RA_FILTER,
	(0), 0, IOVT_UINT32, sizeof(uint32)
	},
	{"nd_ra_filter_ucast", IOV_ND_RA_FILTER_UCAST,
	(0), 0, IOVT_UINT32, sizeof(uint32)
	},
	{"nd_ra_filter_cache_clear", IOV_ND_RA_FILTER_CACHE_CLEAR,
	(0), 0, IOVT_VOID, 0
	},
	{"ndoe", IOV_NDOE,
	(IOVF_RSDB_SET), 0, IOVT_BOOL, 0
	},
	{"nd_unsolicited_na_filter", IOV_ND_UNSOLCITED_NA_FILTER,
	(0), 0, IOVT_UINT32, sizeof(uint32)
	},
	{NULL, 0, 0, 0, 0, 0 }
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/*
 * Initialize ND private context.
 * Returns a pointer to the ND private context, NULL on failure.
 */
wl_nd_info_t *
BCMATTACHFN(wl_nd_attach)(wlc_info_t *wlc)
{
	wl_nd_info_t *ndi;

	/* allocate ND private info struct */
	ndi = MALLOCZ(wlc->osh, sizeof(wl_nd_info_t));
	if (!ndi) {
		WL_ERROR(("wl%d: wl_nd_attach: MALLOC failed; total mallocs %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init ND private info struct */
	ndi->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, nd_iovars, "nd", ndi, nd_doiovar,
		NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		MFREE(WLCOSH(ndi), ndi, sizeof(wl_nd_info_t));
		return NULL;
	}

#ifdef WLNDOE_RA
	wl_nd_ra_filter_init(ndi);
#endif // endif

	ndi->host_ip_addr_type = MALLOCZ(wlc->osh, ND_MULTIHOMING_MAX * sizeof(uint8));
	if (!ndi->host_ip_addr_type) {
		WL_ERROR(("wl%d: wl_nd_attach: MALLOC failed; total mallocs %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	return ndi;
}

void
BCMATTACHFN(wl_nd_detach)(wl_nd_info_t *ndi)
{
	WL_INFORM(("wl%d: nd_detach()\n", WLCUNIT(ndi)));

	if (!ndi)
		return;

#ifdef WLNDOE_RA
	wl_nd_ra_filter_deinit(ndi);
#endif // endif
	wlc_module_unregister(ndi->wlc->pub, "nd", ndi);
	MFREE(WLCOSH(ndi), ndi->host_ip_addr_type, ND_MULTIHOMING_MAX * sizeof(uint8));
	MFREE(WLCOSH(ndi), ndi, sizeof(wl_nd_info_t));
}

/* Handling ND-related iovars */
static int
nd_doiovar(void *hdl, uint32 actionid,
            void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wl_nd_info_t *ndi = hdl;
	int err = 0;
	int i;
	uint32 *ret_int_ptr = (uint32 *)a;
	int32 int_val = 0;

#ifndef BCMROMOFFLOAD
	WL_INFORM(("wl%d: nd_doiovar()\n", WLCUNIT(ndi)));
#endif /* !BCMROMOFFLOAD */

	/* change ndi if wlcif is corr to a virtualIF */
	if (wlcif != NULL) {
		if (wlcif->wlif != NULL) {
			ndi = (wl_nd_info_t *)wl_get_ifctx(ndi->wlc->wl, IFCTX_NDI,
			                                   wlcif->wlif);
		}
	}

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
		case IOV_SVAL(IOV_ND_HOSTIP):
		{
			wl_nd_hostip_t *param = (wl_nd_hostip_t *)p;

			/* extended iovar */
			if ((plen >= WL_ND_HOSTIP_FIXED_LEN) &&
					(param->version == WL_ND_HOSTIP_IOV_VER) &&
					(param->op_type < WL_ND_HOSTIP_OP_MAX) &&
					(param->length >= WL_ND_HOSTIP_FIXED_LEN)) {
				switch (param->op_type) {
				/* Add IP address to host IP table */
				case WL_ND_HOSTIP_OP_ADD:
					if (param->length == WL_ND_HOSTIP_WITH_ADDR_LEN) {
						if (ETHER_ISNULLADDR(&ndi->host_mac)) {
							memcpy(ndi->host_mac,
								&ndi->wlc->pub->cur_etheraddr,
								ETHER_ADDR_LEN);
							ipv6_link_local_addr(&ndi->host_ll_ip,
								ndi->host_mac);
						}

						err = nd_add_host_ip_address(ndi,
							&param->u.host_ip.ip_addr,
							param->u.host_ip.type);
					} else {
						err = BCME_BADARG;
					}
					break;
				/* Remove IP address from host IP table */
				case WL_ND_HOSTIP_OP_DEL:
					if (param->length == WL_ND_HOSTIP_WITH_ADDR_LEN) {
						err = nd_del_host_ip_address(ndi,
							&param->u.host_ip.ip_addr, 0);
					} else {
						err = BCME_BADARG;
					}
					break;
				/* Remove unicast IP address from host IP table */
				case WL_ND_HOSTIP_OP_DEL_UC:
					err = nd_del_host_ip_address(ndi, NULL,
							WL_ND_IPV6_ADDR_TYPE_UNICAST);
					break;
				/* Remove anycast IP address from host IP table */
				case WL_ND_HOSTIP_OP_DEL_AC:
					err = nd_del_host_ip_address(ndi, NULL,
							WL_ND_IPV6_ADDR_TYPE_ANYCAST);
					break;
				/* Remove all address from host IP table */
				case WL_ND_HOSTIP_OP_DEL_ALL:
					memset(ndi->host_ip, 0, sizeof(ndi->host_ip));
					memset(ndi->host_ip_addr_type, 0,
							sizeof(uint8) * ND_MULTIHOMING_MAX);
					ndi->nd_ol_stats.host_ip_entries = 0;
					break;
				default:
					return BCME_BADARG;
				};
			} else {
				/* Add one IP address to the host IP table. */
				if (plen < sizeof(struct ipv6_addr))
					return BCME_BUFTOOSHORT;

				if (ETHER_ISNULLADDR(&ndi->host_mac)) {
					memcpy(&ndi->host_mac, &ndi->wlc->pub->cur_etheraddr,
						ETHER_ADDR_LEN);
					ipv6_link_local_addr(&ndi->host_ll_ip, ndi->host_mac);
				}

				err = nd_add_host_ip_address(ndi, (struct ipv6_addr *)p, 0);

			}

			break;
		}

		case IOV_GVAL(IOV_ND_HOSTIP):
		{
			wl_nd_hostip_t *param = (wl_nd_hostip_t *)p;

			/* extended iovar */
			if ((plen >= WL_ND_HOSTIP_FIXED_LEN) &&
					(param->version == WL_ND_HOSTIP_IOV_VER)) {

				if ((param->op_type == WL_ND_HOSTIP_OP_VER) &&
						(param->length == WL_ND_HOSTIP_FIXED_LEN
						 + sizeof(uint16))) {
					/* return iovar version after wl_nd_hostip_t fixed part */
					wl_nd_hostip_t *r = (wl_nd_hostip_t *)a;

					if (alen < WL_ND_HOSTIP_FIXED_LEN + sizeof(uint16)) {
						return BCME_BUFTOOSHORT;
					}

					r->version = WL_ND_HOSTIP_IOV_VER;
					r->op_type = WL_ND_HOSTIP_OP_VER;
					r->length = WL_ND_HOSTIP_FIXED_LEN + sizeof(uint16);
					r->u.version = WL_ND_HOSTIP_IOV_VER;

					WL_INFORM(("wl%d: %s: iovar version %d\n",
						WLCUNIT(ndi), __FUNCTION__, r->u.version));
				} else if ((param->op_type == WL_ND_HOSTIP_OP_LIST) &&
						(param->length == WL_ND_HOSTIP_FIXED_LEN)) {
					/* return host ip address list (wl_nd_host_ip_list_t) */
					wl_nd_host_ip_list_t *list = (wl_nd_host_ip_list_t *)a;
					uint32 count = 0;

					if (alen < sizeof(wl_nd_host_ip_list_t))
						return BCME_BUFTOOSHORT;
					alen -= sizeof(list->count);

					for (i = 0; i < ND_MULTIHOMING_MAX; i++) {
						if (!IPV6_ADDR_NULL(ndi->host_ip[i].addr)) {
							if (alen < sizeof(wl_nd_host_ip_addr_t))
								return BCME_BUFTOOSHORT;
							memcpy(&list->host_ip[count].ip_addr,
									ndi->host_ip[i].addr,
									IPV6_ADDR_LEN);
							list->host_ip[count].type =
								ndi->host_ip_addr_type[i];
							count++;
							alen -= sizeof(wl_nd_host_ip_addr_t);
						}
					}

					list->count = count;
				}

			} else {
				uint8 *hst_ip = (uint8 *)a;
				/*
				 * Return all IP addresses from host table.
				 * The return buffer is a list of valid IP addresses
				 * terminated by an address of all zeroes.
				 */
				for (i = 0; i < ND_MULTIHOMING_MAX; i++) {
					if (!IPV6_ADDR_NULL(ndi->host_ip[i].addr)) {
						if (alen < sizeof(struct ipv6_addr))
							return BCME_BUFTOOSHORT;
						bcopy(ndi->host_ip[i].addr, hst_ip, IPV6_ADDR_LEN);
						hst_ip += IPV6_ADDR_LEN;
						alen -= sizeof(struct ipv6_addr);
					}
				}

				if (alen < sizeof(struct ipv6_addr))
					return BCME_BUFTOOSHORT;

				bzero(hst_ip, IPV6_ADDR_LEN);
			}
			break;
		}

		case IOV_SVAL(IOV_ND_HOSTIP_CLEAR):
		{
			for (i = 0; i < ND_MULTIHOMING_MAX; i++) {
				bzero(ndi->host_ip[i].addr, IPV6_ADDR_LEN);
				ndi->host_ip_addr_type[i] = 0;
			}

			ndi->nd_ol_stats.host_ip_entries = 0;
			break;
		}

		case IOV_GVAL(IOV_ND_MAC_ADDRESS):
		{
			if (alen < ETHER_ADDR_LEN)
				return BCME_BUFTOOSHORT;

			bcopy(&ndi->host_mac, a, ETHER_ADDR_LEN);
			break;
		}

		case IOV_SVAL(IOV_ND_MAC_ADDRESS):
		{
			if (plen < ETHER_ADDR_LEN)
				return BCME_BUFTOOSHORT;

			if (!ETHER_ISNULLADDR(p)) {
				bcopy(p, &ndi->host_mac, ETHER_ADDR_LEN);
			}
			break;
		}

		case IOV_GVAL(IOV_ND_STATUS):
		{
			if (alen < sizeof(struct nd_ol_stats_t))
				return BCME_BUFTOOSHORT;

			bcopy((uint8*)&ndi->nd_ol_stats, a, sizeof(struct nd_ol_stats_t));
			break;
		}

		case IOV_GVAL(IOV_ND_SNOOP):
			{
			if (alen < sizeof(uint32))
				return BCME_BUFTOOSHORT;

			*ret_int_ptr = (int32)ndi->pkt_snooping;
			}
			break;

		case IOV_SVAL(IOV_ND_SNOOP):
			ndi->pkt_snooping = (int_val != 0);
			break;

		case IOV_SVAL(IOV_ND_STATUS_CLEAR):
		{
			ndi->nd_ol_stats.host_ip_overflow = 0;
			ndi->nd_ol_stats.peer_reply_drop = 0;
			ndi->nd_ol_stats.peer_request = 0;
			ndi->nd_ol_stats.peer_request_drop = 0;
			ndi->nd_ol_stats.peer_service = 0;
			break;
		}

		case IOV_SVAL(IOV_ND_SET_PARAM):
		{
			if (ndi->req_count == ND_REQUEST_MAX) {
				WL_ERROR(("wl%d: IOV_ND_SET_PARAM: no room for new req [%d]\n",
					WLCUNIT(ndi), ndi->req_count));
				err = BCME_ERROR;
				break;
			}

			bcopy(p, &ndi->param[ndi->req_count], sizeof(nd_param_t));

			if (ETHER_ISNULLADDR(&ndi->param[ndi->req_count].host_mac)) {
				bcopy(&ndi->wlc->pub->cur_etheraddr,
					&ndi->param[ndi->req_count].host_mac, ETHER_ADDR_LEN);
			}

			ndi->req_count++;
			break;
		}

		case IOV_SVAL(IOV_ND_CLEAR_PARAM):
		{
			uint8 to_copy;

			if (ndi->req_count == 0) {
				err = BCME_ERROR;
				break;
			}

			for (i = 0; i < ndi->req_count; i++) {
				if (ndi->param[i].offload_id == int_val) {
					to_copy = (ndi->req_count - i - 1);
					/* shift the data 1 level up */
					if (to_copy) {
						bcopy(&ndi->param[i], &ndi->param[i+1],
						(sizeof(nd_param_t) * (ndi->req_count - i - 1)));
					}
					else {
						bzero(&ndi->param[i], sizeof(nd_param_t));
					}
					ndi->req_count--;
				}
			}
			break;
		}

		case IOV_SVAL(IOV_ND_RA_FILTER):
		{
#ifdef WLNDOE_RA
			if (ndi && ndi->ra_info) {
				ndi->ra_info->ra_filter_enable =  int_val;
			}
#else
			err = BCME_UNSUPPORTED;
#endif // endif

			break;
		}

		case IOV_GVAL(IOV_ND_RA_FILTER):
		{
#ifdef WLNDOE_RA
			if (ndi && ndi->ra_info)
				*ret_int_ptr = (int32)ndi->ra_info->ra_filter_enable;
			else
				*ret_int_ptr = 0;
#else
			err = BCME_UNSUPPORTED;
#endif // endif
			break;
		}
		case IOV_SVAL(IOV_ND_RA_FILTER_UCAST):
		{
#ifdef WLNDOE_RA
			if (ndi && ndi->ra_info) {
				ndi->ra_info->ra_filter_ucast =  (int_val != 0);
			}
#else
			err = BCME_UNSUPPORTED;
#endif // endif

			break;
		}

		case IOV_GVAL(IOV_ND_RA_FILTER_UCAST):
		{
#ifdef WLNDOE_RA
			if (ndi && ndi->ra_info)
				*ret_int_ptr = (int32)ndi->ra_info->ra_filter_ucast;
			else
				*ret_int_ptr = 0;
#else
			err = BCME_UNSUPPORTED;
#endif // endif
			break;
		}
		case IOV_SVAL(IOV_ND_RA_FILTER_CACHE_CLEAR):
		{
#ifdef WLNDOE_RA
			err = wl_nd_ra_filter_clear_cache(ndi);
#else
			err = BCME_UNSUPPORTED;
#endif // endif
			break;
		}

		case IOV_GVAL(IOV_NDOE):
			*ret_int_ptr = (int32)ndi->wlc->pub->_ndoe;
			break;

		case IOV_SVAL(IOV_NDOE):
			if (NDOE_SUPPORT(ndi->wlc->pub))
				ndi->wlc->pub->_ndoe = (int_val != 0);
			else
				err = BCME_UNSUPPORTED;
			break;

		case IOV_SVAL(IOV_ND_UNSOLCITED_NA_FILTER):
		{
			ndi->unsolicited_na_filter = (int_val != 0) ? 1 : 0;
			break;
		}

		case IOV_GVAL(IOV_ND_UNSOLCITED_NA_FILTER):
		{
			if (alen < sizeof(uint32)) {
				return BCME_BUFTOOSHORT;
			}

			*ret_int_ptr = (int32)ndi->unsolicited_na_filter;
			break;
		}

		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

static int
nd_add_host_ip_address(wl_nd_info_t *ndi, struct ipv6_addr *ip_addr, uint8 type)
{
	int idx;

	if (!ndi || !ip_addr) {
		return BCME_BADADDR;
	}

	/* check if ip addr is null address */
	if (IPV6_ADDR_NULL(ip_addr->addr)) {
		WL_INFORM(("wl%d: %s: given addr is NULL IPv6 addr\n",
				WLCUNIT(ndi), __FUNCTION__));
		return BCME_BADARG;
	}

	/* Check if ip addr is already in the table */
	for (idx = 0; idx <  ND_MULTIHOMING_MAX; idx++) {
		if (!bcmp(ip_addr->addr, ndi->host_ip[idx].addr, IPV6_ADDR_LEN)) {
			/* existing addr. update address type only */
			ndi->host_ip_addr_type[idx] = type;
			WL_INFORM(("wl%d: %s, existing addr\n", WLCUNIT(ndi), __FUNCTION__));
			return 0;
		}
	}

	/* new host ip address, find empty entry in the table */
	for (idx = 0; idx < ND_MULTIHOMING_MAX; idx++) {
		if (IPV6_ADDR_NULL(ndi->host_ip[idx].addr)) {
			/* add new address */
			memcpy(ndi->host_ip[idx].addr, ip_addr->addr, IPV6_ADDR_LEN);
			ndi->host_ip_addr_type[idx] = type;
			ndi->nd_ol_stats.host_ip_entries++;
			WL_INFORM(("wl%d: %s, addr added\n", WLCUNIT(ndi), __FUNCTION__));
			return 0;
		}
	}

	/* no empty slot in the table. failed to add */
	WL_ERROR(("wl%d: %s, no empty slot\n", WLCUNIT(ndi), __FUNCTION__));
	ndi->nd_ol_stats.host_ip_overflow++;

	return BCME_NORESOURCE;
}

static int
nd_del_host_ip_address(wl_nd_info_t *ndi, struct ipv6_addr *ip_addr, uint8 type)
{
	int idx;
	/*
	 * if ip_addr is NULL ptr, delete address with given type
	 * if ip_addr is given, delete given address only
	 */

	if (!ndi) {
		return BCME_BADADDR;
	}

	/* check if ip addr is null address */
	if (ip_addr && IPV6_ADDR_NULL(ip_addr->addr)) {
		WL_INFORM(("wl%d: %s: given addr is NULL IPv6 addr\n",
				WLCUNIT(ndi), __FUNCTION__));
		return BCME_BADARG;
	}

	for (idx = 0; idx < ND_MULTIHOMING_MAX; idx++) {
		if (IPV6_ADDR_NULL(ndi->host_ip[idx].addr)) {
			/* empty slot */
			continue;
		}

		if (ip_addr) {
			if (!bcmp(ip_addr->addr, ndi->host_ip[idx].addr, IPV6_ADDR_LEN)) {
				/* address match, delete entry */
				memset(ndi->host_ip[idx].addr, 0, IPV6_ADDR_LEN);
				ndi->host_ip_addr_type[idx] = 0;
				ndi->nd_ol_stats.host_ip_entries--;
				WL_INFORM(("wl%d: %s, delete addr\n",
						WLCUNIT(ndi), __FUNCTION__));
				return 0;
			}
		} else if (ndi->host_ip_addr_type[idx] == type) {
			/* address type match, delete entry, and find more */
			memset(ndi->host_ip[idx].addr, 0, IPV6_ADDR_LEN);
			ndi->host_ip_addr_type[idx] = 0;
			ndi->nd_ol_stats.host_ip_entries--;
			WL_INFORM(("wl%d: %s, delete addr by type(%d)\n",
					WLCUNIT(ndi), __FUNCTION__, type));
		}
	}

	return 0;
}

/*
 * Generate ipv6 link-local address from 48bit MAC addr
 * (Link local prefix + EUI-64 interface ID)
 * in - host_mac
 * out - ip_addr
 */
static int
ipv6_link_local_addr(struct ipv6_addr *ip_addr, uint8 *host_mac)
{
	if (!ip_addr || !host_mac) {
		return -1;
	}

	memset(ip_addr, 0, sizeof(struct ipv6_addr));

	/* Link local prefix FE80::/10 */
	ip_addr->addr[0] = 0xfe;
	ip_addr->addr[1] = 0x80;

	/* Generate EUI-64 addr from EU-48 MAC addr */
	ip_addr->addr[8] = host_mac[0] | 0x2;
	ip_addr->addr[9] = host_mac[1];
	ip_addr->addr[10] = host_mac[2];
	ip_addr->addr[11] = 0xff;
	ip_addr->addr[12] = 0xfe;
	ip_addr->addr[13] = host_mac[3];
	ip_addr->addr[14] = host_mac[4];
	ip_addr->addr[15] = host_mac[5];

	return 0;
}

/*
 * Process ND frames in receive direction.
 *
 * Return value:
 *	-1					Packet parsing error
 *	ND_REQ_SINK			NS/NA packet not for local host
 *  ND_REPLY_PEER		Sent NA resp from firmware
 *  ND_FORCE_FORWARD	Fw the packet to host
 */
int
wl_nd_recv_proc(wl_nd_info_t *ndi, void *sdu)
{
	int ret;

	WL_INFORM(("wl%d: wl_nd_recv_proc()\n", WLCUNIT(ndi)));
	/* Parse NS packet and do table lookups */
	ret = wl_ns_parse(ndi, sdu, FALSE);

	switch (ret)
	{
		case ND_REPLY_PEER:
			ndi->nd_ol_stats.peer_service++;
			break;

		case ND_REQ_SINK:
			ndi->nd_ol_stats.peer_request_drop++;
			break;

		default:
#ifdef ERR_USE_EVENT_LOG
			WL_INFORM(("wl%d: %s: Unsupported request: %d\n", WLCUNIT(ndi),
				__FUNCTION__, ret));
#else
			WL_ERROR(("wl%d: %s: Unsupported request: %d\n", WLCUNIT(ndi),
				__FUNCTION__, ret));
#endif // endif
			break;
	}

	return ret;
}

/* Returns -1 if frame is not IP; otherwise, returns pointer/length of IP portion */
static int
wl_ns_parse(wl_nd_info_t *ndi, void *sdu, bool sentby_host)
{
	uint8 *frame = PKTDATA(WLCOSH(ndi), sdu);
	int length = PKTLEN(WLCOSH(ndi), sdu);
	uint8 *pt;
	struct ether_header *eth = NULL;
	struct ipv6_hdr *ip = NULL;
	struct bcm_nd_msg *ns_req = NULL;
	struct nd_msg_opt *ns_req_opt = NULL;
	int i;
	int ret = -1;
	uint16 ns_pktlen;
	/* multicast address */
	char multi_da[ETHER_ADDR_LEN] = {0x33, 0x33, 0x00, 0x00, 0x00, 0x01};
	ns_pktlen = (ETHER_HDR_LEN + sizeof(struct bcm_nd_msg)+ sizeof(struct ipv6_hdr));

	if (IPV6_ADDR_NULL(ndi->host_ip[0].addr)) {
		return ret;
	}

	/* Check if the pkt lenght is atleast the NS pkt req size */
	if (length < ns_pktlen) {
		WL_INFORM(("wl%d: wl_nd_parse: short eth frame (%d)\n",
		      WLCUNIT(ndi), length));
		return -1;
	}

	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (ntoh16_ua((const void *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		eth  = (struct ether_header *)frame;
		pt = frame + ETHER_HDR_LEN;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		WL_INFORM(("wl%d: wl_ns_parse: 802.3 LLC/SNAP\n", WLCUNIT(ndi)));
		eth  = (struct ether_header *)frame;
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
	} else {
		WL_ERROR(("wl%d: wl_ns_parse: non-SNAP 802.3 frame\n",
		          WLCUNIT(ndi)));
		return -1;
	}

	ip = (struct ipv6_hdr *)pt;
	pt +=  sizeof(struct ipv6_hdr);
	ns_req = (struct bcm_nd_msg *)pt;

	if ((ntoh16(eth->ether_type) != ETHER_TYPE_IPV6) ||
		(ip->nexthdr != ICMPV6_HEADER_TYPE)) {
		return ret;
	}

	if (length >= (ns_pktlen + sizeof(struct nd_msg_opt))) {
		ns_req_opt = (struct nd_msg_opt *)(pt + sizeof(struct bcm_nd_msg));
	}

#ifdef WLNDOE_RA
	if (ns_req->icmph.icmp6_type == ICMPV6_PKT_TYPE_RA) {
		return wl_nd_ra_intercept_packet(ndi, (void *)ip,
			eth->ether_shost, eth->ether_dhost);
	}
#endif // endif

	if (sentby_host) {
		/* SNOOP host ip from the NA host response */
		if (ns_req->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA)  {
			bcopy(ip->saddr.addr, ndi->host_ip[0].addr, IPV6_ADDR_LEN);

			if (ETHER_ISNULLADDR(&ndi->host_mac)) {
				bcopy(&ndi->wlc->pub->cur_etheraddr, &ndi->host_mac,
					ETHER_ADDR_LEN);
			}

			ndi->nd_ol_stats.host_ip_entries++;
		}
		return -1;
	} else if (ndi->unsolicited_na_filter &&
			(ns_req->icmph.icmp6_type == ICMPV6_PKT_TYPE_NA) &&
			(ns_req->icmph.opt.nd_advt.solicited == 0) &&
			!bcmp(eth->ether_dhost, multi_da, ETHER_ADDR_LEN) &&
			!bcmp(ip->daddr.addr, &all_node_ipv6_maddr, IPV6_ADDR_LEN)) {
		/* Discard incoming Unsolicited NA packet */
		WL_INFORM(("wl%d: %s: Unsolicited NA discarded", WLCUNIT(ndi), __FUNCTION__));
		ndi->nd_ol_stats.peer_reply_drop++;
		return ND_REQ_SINK;
	}

	if (ns_req->icmph.icmp6_type == ICMPV6_PKT_TYPE_NS &&
		!IPV6_ADDR_NULL(ns_req->target.addr)) {
		ndi->nd_ol_stats.peer_request++;
		ret = ND_REQ_SINK;

		/* Check if the ipv6 target addess is for the local
		 * host
		 */
		for (i = 0; i < ND_MULTIHOMING_MAX; i++) {
			if (!bcmp(ns_req->target.addr, ndi->host_ip[i].addr,
				IPV6_ADDR_LEN)) {
				ret = na_reply_peer(ndi, ns_req, ns_req_opt,
					eth, ip, FALSE, i);
				break;
			}
		}
	}
	return ret;
}

static int
na_reply_peer(wl_nd_info_t *ndi, struct bcm_nd_msg *ns_req, struct nd_msg_opt *options,
	struct ether_header *eth_hdr, struct ipv6_hdr *ip_hdr, bool snap, uint8 req_index)
{
	void *pkt;
	uint8 *frame;
	uint16 ns_pktlen = (ETHER_HDR_LEN + sizeof(struct bcm_nd_msg)+ sizeof(struct nd_msg_opt) +
		sizeof(struct ipv6_hdr) + ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	struct ether_header *na_eth_hdr = NULL;
	struct ipv6_hdr *na_ip_hdr = NULL;
	struct bcm_nd_msg *na_res = NULL;
	struct nd_msg_opt *na_res_opt = NULL;

	WL_INFORM(("wl%d: na_reply_peer()\n", WLCUNIT(ndi)));

	if (!(pkt = PKTGET(WLCOSH(ndi), ns_pktlen, TRUE))) {
		WL_ERROR(("wl%d: nd_reply_peer: alloc failed; dropped\n",
		          WLCUNIT(ndi)));
		WLCNTINCR(ndi->wlc->pub->_cnt->rxnobuf);
		return -1;
	}

	WL_INFORM(("wl%d: na_reply_peer: servicing request from peer\n", WLCUNIT(ndi)));

	frame = PKTDATA(WLCOSH(ndi), pkt);
	bzero(frame, ns_pktlen);

	na_eth_hdr = (struct ether_header *)frame;
	frame += ETHER_HDR_LEN;

	if (snap) {
		bcopy(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN);
		hton16_ua_store(ETHER_TYPE_IPV6, frame + ETHER_HDR_LEN + SNAP_HDR_LEN);
		frame += (SNAP_HDR_LEN + ETHER_TYPE_LEN);
	}

	na_ip_hdr = (struct ipv6_hdr *)frame;
	na_res = (struct bcm_nd_msg *)(((uint8*)na_ip_hdr) + sizeof(struct ipv6_hdr));
	na_res_opt = (struct nd_msg_opt *)((uint8 *)na_res + sizeof(struct bcm_nd_msg));

	/* Create 14-byte eth header, plus snap header if applicable */
	bcopy(ndi->host_mac, na_eth_hdr->ether_shost, ETHER_ADDR_LEN);

	/* Get the Dst Mac from the options field if available */
	if (options != NULL && options->type == ICMPV6_ND_OPT_TYPE_SRC_MAC) {
		bcopy(options->mac_addr, na_eth_hdr->ether_dhost, ETHER_ADDR_LEN);
	}
	else {
		bcopy(eth_hdr->ether_shost, na_eth_hdr->ether_dhost, ETHER_ADDR_LEN);
	}

	na_eth_hdr->ether_type = hton16(ETHER_TYPE_IPV6);

	/* Create IPv6 Header */
	if (IPV6_ADDR_NULL(ip_hdr->saddr.addr)) {
		bcopy(all_node_ipv6_maddr.addr, na_ip_hdr->daddr.addr, IPV6_ADDR_LEN);
		na_res->icmph.opt.nd_advt.solicited = 0;

	}
	else {
		bcopy(ip_hdr->saddr.addr, na_ip_hdr->daddr.addr, IPV6_ADDR_LEN);
		na_res->icmph.opt.nd_advt.solicited = 1;
	}

	if (ndi->host_ip_addr_type[req_index] == WL_ND_IPV6_ADDR_TYPE_ANYCAST) {
		/*
		 * Anycast address must not be used as the source address
		 * of an IPv6 packet (RFC-3513 2.6)
		 */
		memcpy(na_ip_hdr->saddr.addr, ndi->host_ll_ip.addr, IPV6_ADDR_LEN);
	} else {
		bcopy(ns_req->target.addr, na_ip_hdr->saddr.addr, IPV6_ADDR_LEN);
	}

	na_ip_hdr->payload_len = hton16(sizeof(struct bcm_nd_msg) + sizeof(struct nd_msg_opt));
	na_ip_hdr->nexthdr = ICMPV6_HEADER_TYPE;
	na_ip_hdr->hop_limit = IPV6_HOP_LIMIT;
	na_ip_hdr->version = IPV6_VERSION;

	/* Create Neighbor Advertisement Msg (ICMPv6) */
	na_res->icmph.icmp6_type = ICMPV6_PKT_TYPE_NA;
	na_res->icmph.icmp6_code = 0;
	/* NA for anycast address SHOULD NOT set Override flag (RFC-4816  7.2.4) */
	if (ndi->host_ip_addr_type[req_index] == WL_ND_IPV6_ADDR_TYPE_ANYCAST) {
		WL_INFORM(("wl%d: %s, reply NA for anycast\n", WLCUNIT(ndi), __FUNCTION__));
		na_res->icmph.opt.nd_advt.override = 0;
	} else {
		WL_INFORM(("wl%d: %s, reply NA for unicast\n", WLCUNIT(ndi), __FUNCTION__));
		na_res->icmph.opt.nd_advt.override = 1;
	}

	bcopy(ns_req->target.addr, na_res->target.addr, IPV6_ADDR_LEN);

	/* Create Neighbor Advertisement Opt Header (ICMPv6) */
	na_res_opt->type = ICMPV6_ND_OPT_TYPE_TARGET_MAC;
	na_res_opt->len = 1;
	bcopy(ndi->host_mac, na_res_opt->mac_addr, ETHER_ADDR_LEN);

	/* Calculate Checksum */
	na_res->icmph.icmp6_cksum = csum_ipv6(ndi, na_ip_hdr->saddr.addr, na_ip_hdr->daddr.addr,
		ICMPV6_HEADER_TYPE, (uint8*)na_res,
		(sizeof(struct bcm_nd_msg) + sizeof(struct nd_msg_opt)));

	wlc_sendpkt(ndi->wlc, pkt, ndi->wlcif);

	return ND_REPLY_PEER;
}

/*
 * Process ND (NS/NA) frames in transmit direction.
 *
 * Return value:
 *	0		ND processing not enabled
 *	-1		Packet parsing error
 */
int
wl_nd_send_proc(wl_nd_info_t *ndi, void *sdu)
{
	int ret;

	if (ndi->pkt_snooping && IPV6_ADDR_NULL(ndi->host_ip[0].addr)) {
		ret = wl_ns_parse(ndi, sdu, TRUE);
	} else {
		ret = 0;
	}

	WL_INFORM(("wl%d: wl_nd_send_proc() ret:%d\n", WLCUNIT(ndi), ret));
	return ret;
}

/* called when a new virtual IF is created.
 *	i/p: primary NDIIF [ndi_p] and the new wlcif,
 *	o/p: new ndi structure populated with inputs and
 *		the global parameters duplicated from ndi_p
 *	side-effects: ndi for a new IF will inherit properties of ndi_p till
 *		the point new ndi is created. After that, for any change in
 *		ndi_p will NOT change the ndi corr to new IF. To change property
 *		of new IF, wl -i wl0.x has to be used.
*/
wl_nd_info_t *
wl_nd_alloc_ifndi(wl_nd_info_t *ndi_p, wlc_if_t *wlcif)
{
	wl_nd_info_t *ndi;
	wlc_info_t *wlc = ndi_p->wlc;

	/* allocate ND private info struct */
	ndi = MALLOCZ(wlc->osh, sizeof(wl_nd_info_t));
	if (!ndi) {
		WL_ERROR(("wl%d: wl_nd_alloc_ifndi: MALLOCZ failed; total mallocs %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init nd private info struct */
	ndi->wlc = wlc;
	ndi->wlcif = wlcif;

	return ndi;
}

void
wl_nd_free_ifndi(wl_nd_info_t *ndi)
{
	if (ndi != NULL) {
		MFREE(WLCOSH(ndi), ndi, sizeof(wl_nd_info_t));
	}
}

void
wl_nd_clone_ifndi(wl_nd_info_t *from_ndi, wl_nd_info_t *to_ndi)
{
	wlc_if_t *wlcif = to_ndi->wlcif;
	wlc_info_t *wlc = to_ndi->wlc;
	bcopy(from_ndi, to_ndi, sizeof(wl_nd_info_t));
	to_ndi->wlc = wlc;
	to_ndi->wlcif = wlcif;
}

/* Parital ip checksum algorithm */
static uint32
csum_partial_16(uint8 *nptr, int nlen, uint32 x)
{
	uint32 new;

	while (nlen)
	{
		new = (nptr[0] << 8) + nptr[1];
		nptr += 2;
		x += new & 0xffff;
		if (x & 0x10000) {
			x++;
			x &= 0xffff;
		}
		nlen -= 2;
	}

	return x;
}

/*
 * Caclulates the checksum for the pseodu IP hdr + NS req/res
 *
 */
static uint16 csum_ipv6(wl_nd_info_t *ndi, uint8 *saddr, uint8 *daddr,
	uint8 proto, uint8 *buf, uint32 buf_len)
{
	uint16 ret;
	uint32 cksum;
	uint32 len = hton32(buf_len);
	uint8 prot[4] = {0, 0, 0, 0};

	prot[3] = proto;

	cksum = csum_partial_16(saddr, IPV6_ADDR_LEN, 0);
	cksum = csum_partial_16(daddr, IPV6_ADDR_LEN, cksum);
	cksum = csum_partial_16((uint8*)&len, 4, cksum);
	cksum = csum_partial_16(prot, 4, cksum);
	cksum = csum_partial_16(buf, buf_len, cksum);

	cksum = ~cksum & 0xFFFF;
	hton16_ua_store((uint16)cksum, &ret);

	return ret;
}

#ifdef WLNDOE_RA
static int
wl_nd_ra_filter_init(wl_nd_info_t *ndi)
{
	ndi->ra_info = MALLOC(WLCOSH(ndi), sizeof(wl_nd_ra_info_t));
	if (!ndi->ra_info) {
		WL_ERROR(("wl: wl_nd_ra_filter_init: MALLOC failed; \n"));
		return NULL;
	}

	bzero(ndi->ra_info, sizeof(wl_nd_ra_info_t));
	/* By default, RA FILTER is disabled */
	ndi->ra_info->ra_filter_enable = 0;
	return 0;
}

int
wl_nd_ra_filter_clear_cache(wl_nd_info_t *ndi)
{
	int i;
	wl_nda_node_t *node;

	if (!ndi || !ndi->ra_info)
		return -1;

	for (i = 0; i < WL_ND_MAX_RA_NODES; i++) {
		node = &ndi->ra_info->nodes[i];

		if (node && node->cached_packet) {
			/* free the cached packet */
			WL_INFORM(("Freeing cached packet_ptr:%p\n",
				OSL_OBFUSCATE_BUF(node->cached_packet)));
			MFREE(WLCOSH(ndi), node->cached_packet, node->cached_packet_len);
			node->cached_packet_len = 0;
			node->cached_packet = NULL;
			node->count = 0;
			bzero(node->sa_ip,  IPV6_ADDR_LEN);
		}
	}

	return 0;
}

static int
wl_nd_ra_filter_deinit(wl_nd_info_t *ndi)
{
	WL_TRACE(("%s: Enter..\n", __func__));

	if (!ndi || !ndi->ra_info)
		return -1;

	wl_nd_ra_filter_clear_cache(ndi);

	MFREE(WLCOSH(ndi), ndi->ra_info, sizeof(wl_nd_ra_info_t));

	return 0;
}

/* byte offset of lifetime into payload */
#define RA_LIFETIME_OFFSET     6
/* RA rate limit interval in millisecs */
#define RA_RATELIMIT_INTERVAL  60000
/* min. RA lifetime threshold before rate limiting kicks in
 * if RA lifetime is "short" relative to rate limit interval, just cache it
 * "short" is currently defined as 3 times the rate limit interval
 */
#define RA_MIN_LT_THRESHOLD    (3 * RA_RATELIMIT_INTERVAL)
#define SEC_TO_MS              1000
static int
wl_nd_ra_intercept_packet(wl_nd_info_t *ndi, void *ip, uint8 *sa, uint8 *da)
{
	int ret = 0;
	bool filter = FALSE;
	struct ipv6_hdr *iphdr = (struct ipv6_hdr *)ip;
	uint32 pkt_len;
	uint8 count = 0;
	wl_nda_node_t *node;
	bool match = FALSE;
	uint8 index = 0;

	if (!ndi || !ndi->ra_info || !iphdr) {
		WL_INFORM(("%s: ndi not initialized ndi:%p, ra_info:%p iphdr:%p\n",
			__func__, OSL_OBFUSCATE_BUF(ndi),
			OSL_OBFUSCATE_BUF(ndi->ra_info), OSL_OBFUSCATE_BUF(iphdr)));
		return -1;
	}

	pkt_len = ntoh16(iphdr->payload_len);
	/* Filter is enabled by default, if NDOE is enabled. But this
	 * can be explicitly disabled via iovar
	 */
	if (!ndi->ra_info->ra_filter_enable) {
		WL_INFORM(("%s: Filter explicitly disabled! Return. %d\n",
			__func__, ndi->ra_info->ra_filter_enable));
		return 0;
	}

	 if (ETHER_ISMULTI(da) || ndi->ra_info->ra_filter_ucast) {
		/* RA lifetime in sec */
		 uint16 lifetime = ntoh16(*(uint16 *)((uint8 *)ip
					+ (sizeof(struct ipv6_hdr)) + RA_LIFETIME_OFFSET));

		/* Router advertisement with Multicast DA */
		while (count < WL_ND_MAX_RA_NODES) {
			node = &ndi->ra_info->nodes[count];
			if (!bcmp(node->sa_ip, iphdr->saddr.addr, IPV6_ADDR_LEN)) {
				match = TRUE;
				WL_INFORM((" Packet from an already cached SRC IP ADDR \n"));

				if ((node->cached_packet_len == pkt_len) &&
					!bcmp((uint8 *)node->cached_packet,
					((uint8 *)ip + (sizeof(struct ipv6_hdr))), pkt_len)) {
					if (lifetime * SEC_TO_MS <= RA_MIN_LT_THRESHOLD) {
						WL_INFORM(("RA lifetime not much less than "
							"rate limit interval: Send it up\n"));
						index = count;
						break;
					}

					if (OSL_SYSUPTIME() >=
						(node->time_stamp + RA_RATELIMIT_INTERVAL)) {
						WL_INFORM(("Identical RA received outside "
							"rate limit interval: Send it up\n"));
						index = count;
						break;
					}

					/* The cached content matches.
					 * Discard the newly arrived frame
					 */
					node->count++;
					WL_INFORM(("Cached content matches. Filter out"
							" the frame. counter:%d \n", node->count));
					filter = TRUE;
					break;
				} else {
					/* Content doesn't match. Cach it up and send it up */
					WL_INFORM((" Packet content doesn't match with the "
							"cached content. Send it up \n"));
					index = count;
					break;
				}
			}
			count++;
		 }

		if (!match) {
			/* Use ring index. Ring Index would always point to available/oldest slot */
			if (ndi->ra_info->ring_index >= WL_ND_MAX_RA_NODES)
				ndi->ra_info->ring_index = 0;
			 index = ndi->ra_info->ring_index;
			 /* Increment and Point to the oldest entry */
			 ndi->ra_info->ring_index++;
		}

		if (!filter) {
			/* Cache the frame */
			node = &ndi->ra_info->nodes[index];
			bcopy(iphdr->saddr.addr, node->sa_ip, IPV6_ADDR_LEN);
			if (node->cached_packet)
				MFREE(WLCOSH(ndi), node->cached_packet, node->cached_packet_len);
			node->cached_packet = MALLOC(WLCOSH(ndi), pkt_len);
			if (!node->cached_packet) {
				WL_ERROR(("%s: Caching failed. Send the packet up \n", __func__));
				return 0;
			}
			bcopy(((uint8 *)ip + sizeof(struct ipv6_hdr)),
				node->cached_packet, pkt_len);
			node->cached_packet_len = pkt_len;
			node->time_stamp = OSL_SYSUPTIME();
		}

	 } else {
			/* Directed Frame. Give it up */
			WL_INFORM(("%s: Not a multicast packet/unicast filter is enabled(%d) \n",
				__func__, ndi->ra_info->ra_filter_ucast));
	 }

	if (filter) {
		ret = ND_REQ_SINK;
	}

	return ret;
}
#endif /* WLNDOE_RA */
