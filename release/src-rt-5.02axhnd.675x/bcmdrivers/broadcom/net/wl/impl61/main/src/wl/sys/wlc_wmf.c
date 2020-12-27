/**
 * @file
 * @brief
 * Wireless Multicast Forwarding (WMF)
 *
 * WMF is forwarding multicast packets as unicast packets to
 * multicast group members in a BSS
 *
 * Supported protocol families: IPV4
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_wmf.c 776502 2019-07-01 13:32:22Z $
 */

/**
 * @file
 * @brief
 * The objective of Wireless Multicast Forwarding (WMF) module is to support multicast streaming
 * from Access Point to each participating multicast group members that can be a STA, WET or WDS end
 * point. Streaming between WET/STA devices is also supported.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WirelessMulticastForwarding]
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
#include <802.11.h>
#include <ethernet.h>
#include <vlan.h>
#include <802.3.h>
#include <bcmip.h>
#include <bcmarp.h>
#include <bcmudp.h>
#include <bcmdhcp.h>
#include <bcmendian.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_wmf.h>
#include <emf/emf/emf_cfg.h>
#include <emf/emf/emfc_export.h>
#include <emf/igs/igs_cfg.h>
#include <emf/igs/igsc_export.h>
#include <wlc_ap.h>
#include <wlc_rx.h>
#include <wlc_dump.h>

struct wmf_info {
	wlc_info_t *wlc;
	int	scb_handle;	/* scb cubby handle */
	int	cfg_handle;	/* bsscfg cubby handle */
};

/* bsscfg specific data */
typedef struct {
	bool	wmf_ucast_igmp_query;	/* 1 to enable, 0 by default */
	bool	wmf_ucast_upnp;		/* 1 to enable, 0 by default */
	bool	wmf_psta_disable;	/* shall unicats to psta (default) or not */
	bool wmf_instance;	/* WMF instance instantiated */
	void *emfci;	/* Pointer to emfc instance */
	void *igsci;	/* Pointer to igsc instance */
} bss_wmf_info_t;

/* bsscfg cubby access macro */
#define BSS_WMF_INFO(wmf, cfg)	(bss_wmf_info_t *)BSSCFG_CUBBY(cfg, (wmf)->cfg_handle)

/* forward declarations */
static int wlc_wmf_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);
static void wlc_wmf_scb_deinit(void *context, struct scb *scb);
static void wlc_wmf_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
static void wmf_sendup(void *wrapper, void *p);
static int32 wmf_hooks_register(void *wrapper);
static int32 wmf_hooks_unregister(void *wrapper);
static int32 wmf_igs_broadcast(void *wrapper, uint8 *ip, uint32 length, uint32 mgrp_ip);
#ifdef BCM_NBUFF_WLMCAST
static int32 _wmf_forward(void *wrapper, void *p, uint32 mgrp_ip, void *txif, int rt_port);
#else
static int32 _wmf_forward(void *wrapper, void *p, uint32 mgrp_ip, void *txif, bool rt_port);
#endif // endif
static int wlc_wmf_down(void *hdl);

#if defined(BCMDBG) || defined(WLTEST)
static int wlc_wmf_dump(void *context, struct bcmstrbuf *b);
#endif /* BCMDBG || WLTEST */

/* Add wmf instance to a bsscfg */
static int32 wlc_wmf_instance_add(wmf_info_t *wmf, struct wlc_bsscfg *bsscfg);
/* Delete wmf instance from bsscfg */
static void wlc_wmf_instance_del(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg);
/* Start WMF on the bsscfg */
static int wlc_wmf_start(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg);
/* Stop WMF on the bsscfg */
static void wlc_wmf_stop(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg);
/* Delete a station from the WMF interface list */
static int wlc_wmf_sta_del(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg, struct scb *scb);

/* iovar table */
enum wlc_wmf_iov {
	IOV_WMF_BSS_ENABLE = 1,
	IOV_WMF_UCAST_IGMP = 2,
	IOV_WMF_MCAST_DATA_SENDUP = 3,
	IOV_WMF_PSTA_DISABLE = 4,
	IOV_WMF_UCAST_IGMP_QUERY = 5,
	IOV_WMF_UCAST_UPNP = 6,
	IOV_LAST 		/* In case of a need to check max ID number */
};

static const bcm_iovar_t wmf_iovars[] = {
	{"wmf_bss_enable", IOV_WMF_BSS_ENABLE,
	(0), 0, IOVT_BOOL, 0
	},
	{"wmf_ucast_igmp", IOV_WMF_UCAST_IGMP,
	(0), 0, IOVT_BOOL, 0
	},
	{"wmf_mcast_data_sendup", IOV_WMF_MCAST_DATA_SENDUP,
	(0), 0, IOVT_BOOL, 0
	},
	{"wmf_psta_disable", IOV_WMF_PSTA_DISABLE,
	(IOVF_SET_DOWN), 0, IOVT_BOOL, 0
	},
#ifdef WL_IGMP_UCQUERY
	{"wmf_ucast_igmp_query", IOV_WMF_UCAST_IGMP_QUERY,
	(0), 0, IOVT_BOOL, 0
	},
#endif // endif
#ifdef WL_UCAST_UPNP
	{"wmf_ucast_upnp", IOV_WMF_UCAST_UPNP,
	(0), 0, IOVT_BOOL, 0
	},
#endif // endif
	{NULL, 0, 0, 0, 0, 0 }
};

#define IGMPV2_HOST_MEMBERSHIP_QUERY	0x11
#define MCAST_ADDR_UPNP_SSDP(addr) ((addr) == 0xeffffffa)

#ifdef BCM_NBUFF_WLMCAST_IPV6
extern void *wl_get_device(void *p);
extern void *wl_wmf_hooks_get(int cmd, void *wrapper, void *p);

void *wl_wmf_get_igsc(wlc_bsscfg_t *bsscfg)
{
	wmf_info_t *wmf = bsscfg->wlc->wmfi;
	bss_wmf_info_t *bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);
	return (bssinfo->wmf_instance)?bssinfo->igsci:NULL;

}
#endif // endif
/*
 * WMF module attach function
 */
wmf_info_t *
BCMATTACHFN(wlc_wmf_attach)(wlc_info_t *wlc)
{
	wmf_info_t *wmf;

	if (!(wmf = (wmf_info_t *)MALLOCZ(wlc->osh, sizeof(wmf_info_t)))) {
		WL_ERROR(("wl%d: wlc_wmf_attach: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	wmf->wlc = wlc;

	/* reserve cubby in the scb container for per-scb private data */
	wmf->scb_handle = wlc_scb_cubby_reserve(wlc, 0, NULL, wlc_wmf_scb_deinit,
		NULL, (void *)wmf);
	if (wmf->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_wmf_attach: wlc_scb_cubby_reserve failed\n", wlc->pub->unit));
		MFREE(wlc->osh, wmf, sizeof(wmf_info_t));
		return NULL;
	}

	/* reserve cubby in the cfg container for per-cfg private data */
	wmf->cfg_handle = wlc_bsscfg_cubby_reserve(wlc, sizeof(bss_wmf_info_t),
	                                           NULL, wlc_wmf_bss_deinit, NULL, wmf);
	if (wmf->cfg_handle < 0) {
		WL_ERROR(("wl%d: wlc_wmf_attach: wlc_bsscfg_cubby_reserve failed\n",
		          wlc->pub->unit));
		MFREE(wlc->osh, wmf, sizeof(wmf_info_t));
		return NULL;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, wmf_iovars, "wmf", wmf, wlc_wmf_doiovar,
		NULL, NULL, wlc_wmf_down)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	return wmf;
}

/*
 * WMF module detach function
 */
void
BCMATTACHFN(wlc_wmf_detach)(wmf_info_t *wmf)
{
	wlc_info_t *wlc;

	if (!wmf)
		return;

	wlc = wmf->wlc;
	wlc_module_unregister(wlc->pub, "wmf", wmf);
	MFREE(wlc->osh, wmf, sizeof(wmf_info_t));
}
/*
 * WMF module iovar handler function
 */
static int
wlc_wmf_doiovar(void *hdl, uint32 actionid,
        void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wmf_info_t *wmf = (wmf_info_t *)hdl;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	int32 *ret_int_ptr = (int32 *) a;
	bool bool_val;
	int err = 0;
	int32 int_val = 0;
	bss_wmf_info_t *bssinfo;

	ASSERT(wmf != NULL);
	wlc = wmf->wlc;

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_WMF_BSS_ENABLE):
		*ret_int_ptr = bsscfg->wmf_enable;
		break;

	case IOV_SVAL(IOV_WMF_BSS_ENABLE):
		/* Duplicate request */
		if (bsscfg->wmf_enable == bool_val)
			break;

		if (bool_val) {
			if ((err = wlc_wmf_instance_add(wmf, bsscfg)) != BCME_OK) {
				WL_ERROR(("wl%d: %s: Error in creating WMF instance\n",
					wlc->pub->unit, __FUNCTION__));
				break;
			}

			/* Start WMF if it is enabled for this bsscfg */
			wlc_wmf_start(wmf, bsscfg);

			bsscfg->wmf_enable = TRUE;
		} else {
			bsscfg->wmf_enable = FALSE;

			/* Stop WMF if it is enabled for this BSS */
			wlc_wmf_stop(wmf, bsscfg);

			/* Delete WMF instance for this bsscfg */
			wlc_wmf_instance_del(wmf, bsscfg);
		}
		break;

	case IOV_SVAL(IOV_WMF_UCAST_IGMP):
		if (int_val >= OFF && int_val <= ON)
			bsscfg->wmf_ucast_igmp = int_val;
		else
			err = BCME_RANGE;
		break;

	case IOV_GVAL(IOV_WMF_UCAST_IGMP):
		*ret_int_ptr = bsscfg->wmf_ucast_igmp;
		break;

	case IOV_SVAL(IOV_WMF_MCAST_DATA_SENDUP):
		wlc_wmf_mcast_data_sendup(wmf, bsscfg, TRUE, bool_val);
		break;

	case IOV_GVAL(IOV_WMF_MCAST_DATA_SENDUP):
		*ret_int_ptr = wlc_wmf_mcast_data_sendup(wmf, bsscfg, FALSE, FALSE);
		break;

	case IOV_SVAL(IOV_WMF_PSTA_DISABLE):
		bssinfo->wmf_psta_disable = int_val;
		break;

	case IOV_GVAL(IOV_WMF_PSTA_DISABLE):
		*ret_int_ptr = bssinfo->wmf_psta_disable;
		break;

#ifdef WL_IGMP_UCQUERY
	case IOV_SVAL(IOV_WMF_UCAST_IGMP_QUERY):
		if (int_val >= OFF && int_val <= ON)
			bssinfo->wmf_ucast_igmp_query = int_val;
		else
			err = BCME_RANGE;
		break;

	case IOV_GVAL(IOV_WMF_UCAST_IGMP_QUERY):
		*ret_int_ptr = bssinfo->wmf_ucast_igmp_query;
		break;
#endif // endif
#ifdef WL_UCAST_UPNP
	case IOV_SVAL(IOV_WMF_UCAST_UPNP):
		if (int_val >= OFF && int_val <= ON)
			bssinfo->wmf_ucast_upnp = int_val;
		else
			err = BCME_RANGE;
		break;

	case IOV_GVAL(IOV_WMF_UCAST_UPNP):
		*ret_int_ptr = bssinfo->wmf_ucast_upnp;
		break;
#endif // endif
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

#if defined(BCMDBG) || defined(WLTEST)
static int
wlc_wmf_dump(void *context, struct bcmstrbuf *b)
{
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)context;
	wlc_info_t *wlc = bsscfg->wlc;
	wmf_info_t *wmf = wlc->wmfi;
	emf_cfg_request_t *req;
	emf_cfg_mfdb_list_t *list;
	emf_stats_t *emfs;
	struct scb *scb;
	int32 i;
	bss_wmf_info_t *bssinfo;

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	if (!bssinfo->wmf_instance)
		return BCME_ERROR;

	bcm_bprintf(b, "WMF instance wl%d.%d:\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg));

	if (!(req = (emf_cfg_request_t *)MALLOC(bsscfg->wlc->osh, sizeof(emf_cfg_request_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          bsscfg->wlc->pub->unit, __FUNCTION__, MALLOCED(bsscfg->wlc->osh)));
		return BCME_ERROR;
	}

	bzero((char *)req, sizeof(emf_cfg_request_t));

	/*
	 * Dump some counters and statistics
	 */
#ifdef BCM_NBUFF_WLMCAST_IPV6
	/* here, it is changed to use wireless interface name to be as instance name,
	 * with NIC + Dongle mixed case, the bsscfg->_idx may be duplicated, each inteface
	 * will seperted maintain its own list,also for igs command to retrieve sdb easily
	 * as it check instnace name against interface name
	 */
	(void)snprintf((char *)req->inst_id, sizeof(req->inst_id), "%s",
			wl_ifname(wlc->wl, bsscfg->wlcif->wlif));
#else
	(void)snprintf((char *)req->inst_id, sizeof(req->inst_id), "wmf%d", bsscfg->_idx);
#endif // endif
	req->command_id = EMFCFG_CMD_EMF_STATS;
	req->size = sizeof(emf_stats_t);
	req->oper_type = EMFCFG_OPER_TYPE_GET;

	emfc_cfg_request_process(bssinfo->emfci, req);
	if (req->status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("wl%d: %s failed\n", wlc->pub->unit, __FUNCTION__));
		MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
		return BCME_ERROR;
	}

	emfs = (emf_stats_t *)req->arg;
	bcm_bprintf(b, "McastDataPkts   McastDataFwd    McastFlooded    "
		    "McastDataSentUp McastDataDropped\n");
	bcm_bprintf(b, "%-15d %-15d %-15d %-15d %d\n",
	            emfs->mcast_data_frames, emfs->mcast_data_fwd,
	            emfs->mcast_data_flooded, emfs->mcast_data_sentup,
	            emfs->mcast_data_dropped);
	bcm_bprintf(b, "IgmpPkts        IgmpPktsFwd     "
		    "IgmpPktsSentUp  MFDBCacheHits   MFDBCacheMisses\n");
	bcm_bprintf(b, "%-15d %-15d %-15d %-15d %d\n",
	            emfs->igmp_frames, emfs->igmp_frames_fwd,
	            emfs->igmp_frames_sentup, emfs->mfdb_cache_hits,
	            emfs->mfdb_cache_misses);

	/*
	 * Dump the learned table of scb attached to a group
	 */
	bzero((char *)req, sizeof(emf_cfg_request_t));
#ifdef BCM_NBUFF_WLMCAST_IPV6
	(void)snprintf((char *)req->inst_id, sizeof(req->inst_id), "%s",
			wl_ifname(wlc->wl, bsscfg->wlcif->wlif));
#else
	(void)snprintf((char *)req->inst_id, sizeof(req->inst_id), "wmf%d", bsscfg->_idx);
#endif // endif
	req->command_id = EMFCFG_CMD_MFDB_LIST;
	req->size = sizeof(emf_cfg_mfdb_list_t);
	req->oper_type = EMFCFG_OPER_TYPE_GET;

	emfc_cfg_request_process(bssinfo->emfci, req);
	if (req->status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("wl%d: %s failed\n", wlc->pub->unit, __FUNCTION__));
		MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
		return BCME_ERROR;
	}

	bcm_bprintf(b, "\nGroup \t\tSCB\t\t\tPkts\n");

	list = (emf_cfg_mfdb_list_t *)req->arg;
	for (i = 0; i < list->num_entries; i++)
	{
		bcm_bprintf(b, "%d.%d.%d.%d \t",
			((list->mfdb_entry[i].mgrp_ip) >> 24) & 0xff,
			((list->mfdb_entry[i].mgrp_ip) >> 16) & 0xff,
			((list->mfdb_entry[i].mgrp_ip) >> 8) & 0xff,
			((list->mfdb_entry[i].mgrp_ip) & 0xff));

		scb = (struct scb *)list->mfdb_entry[i].if_ptr;
		bcm_bprintf(b, MACF, ETHER_TO_MACF(scb->ea));

		bcm_bprintf(b, "\t%d\n", list->mfdb_entry[i].pkts_fwd);
	}
	bcm_bprintf(b, "\n");

	MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));

	return 0;
}
#endif /* BCMDBG || WLTEST */

/*
 * SCB free notification
 * We call the WMF specific interface delete function
 */

static void
wlc_wmf_scb_deinit(void *context, struct scb *scb)
{
	wmf_info_t *wmf = (wmf_info_t *)context;
	bss_wmf_info_t *bssinfo;
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	if (!bssinfo->wmf_instance)
		return;

	/* Delete the station from  WMF list */
	wlc_wmf_sta_del(wmf, bsscfg, scb);
}

/*
 * Description: This function is called to instantiate emf
 *		and igs instance on enabling WMF on a
 *              bsscfg.
 *
 * Input:       wlc - pointer to the wlc_info_t structure
 *              bsscfg - pointer to the bss configuration
 */
int32
wlc_wmf_instance_add(wmf_info_t *wmf, struct wlc_bsscfg *bsscfg)
{
	wlc_info_t *wlc = wmf->wlc;
	emfc_wrapper_t wmf_emfc = {0};
	igsc_wrapper_t wmf_igsc = {0};
	char inst_id[10];
	bss_wmf_info_t *bssinfo;

	BCM_REFERENCE(wlc);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	/* Fill in the wmf efmc wrapper functions */
	wmf_emfc.sendup_fn = wmf_sendup;
	wmf_emfc.hooks_register_fn = wmf_hooks_register;
	wmf_emfc.hooks_unregister_fn = wmf_hooks_unregister;
	wmf_emfc.forward_fn = _wmf_forward;
	wmf_emfc.mfdb_add_fn = NULL;
	wmf_emfc.mfdb_delete_fn = NULL;
#ifdef BCM_NBUFF_WLMCAST_IPV6
	wmf_emfc.hooks_get_fn = (void *)wl_wmf_hooks_get;
#endif // endif

	/* Create Instance ID */
#ifdef BCM_NBUFF_WLMCAST_IPV6
	(void)snprintf(inst_id, sizeof(inst_id), "%s", wl_ifname(wlc->wl, bsscfg->wlcif->wlif));
#else
	(void)snprintf(inst_id, sizeof(inst_id), "wmf%d", bsscfg->_idx);
#endif // endif

	/* Create EMFC instance for WMF */
	bssinfo->emfci = emfc_init((int8 *)inst_id, (void *)bsscfg, wlc->osh, &wmf_emfc);
	if (bssinfo->emfci == NULL) {
		WL_ERROR(("wl%d: %s: WMF EMFC init failed\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	WL_WMF(("wl%d: %s: Created EMFC instance\n",
		wlc->pub->unit, __FUNCTION__));

	/* Fill in the wmf igsc wrapper functions */
	wmf_igsc.igs_broadcast = wmf_igs_broadcast;

	/* Create IGSC instance */
	bssinfo->igsci = igsc_init((int8 *)inst_id, (void *)bsscfg, wlc->osh, &wmf_igsc);
	if (bssinfo->igsci == NULL) {
		WL_ERROR(("wl%d: %s: WMF IGSC init failed\n",
			wlc->pub->unit, __FUNCTION__));
		/* Free the earlier allocated resources */
		emfc_exit(bssinfo->emfci);
		return BCME_ERROR;
	}

	WL_WMF(("wl%d: %s: Created IGSC instance\n",
		wlc->pub->unit, __FUNCTION__));

	/* Set the wmf instance pointer inside the bsscfg */
	bssinfo->wmf_instance = TRUE;

	WL_WMF(("wl%d: %s: Addding WLC wmf instance\n",
		wlc->pub->unit, __FUNCTION__));

#if defined(BCMDBG) || defined(WLTEST)
	wlc_dump_register(wlc->pub, "wmf", (dump_fn_t)wlc_wmf_dump, (void *)bsscfg);
#endif /*  defined(BCMDBG) || defined(WLTEST) */

	return BCME_OK;
}

/*
 * Description: This function is called to destroy emf
 *		and igs instances on disabling WMF on
 *              a bsscfg.
 *
 * Input:       bsscfg - pointer to the bss configuration
 */
void
wlc_wmf_instance_del(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = wmf->wlc;
	bss_wmf_info_t *bssinfo;

	BCM_REFERENCE(wlc);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	if (!bssinfo->wmf_instance)
		return;

	WL_WMF(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	/* Free the EMFC instance */
	emfc_exit(bssinfo->emfci);

	/* Free the IGSC instance */
	igsc_exit(bssinfo->igsci);

	/* Make the pointer NULL */
	bssinfo->wmf_instance = FALSE;

	return;
}

/*
 * Description: This function is called to start wmf operation
 *              when a bsscfg is up
 *
 * Input:       bsscfg - pointer to the bss configuration
 */
int
wlc_wmf_start(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = wmf->wlc;
	emf_cfg_request_t *req;
	char inst_id[10];
	bss_wmf_info_t *bssinfo;

	BCM_REFERENCE(wlc);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	WL_WMF(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (!(req = (emf_cfg_request_t *)MALLOC(bsscfg->wlc->osh, sizeof(emf_cfg_request_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          bsscfg->wlc->pub->unit, __FUNCTION__, MALLOCED(bsscfg->wlc->osh)));
		return BCME_ERROR;
	}

	bzero((char *)req, sizeof(emf_cfg_request_t));

#ifdef BCM_NBUFF_WLMCAST_IPV6
	(void)snprintf(inst_id, sizeof(inst_id), "%s", wl_ifname(wlc->wl, bsscfg->wlcif->wlif));
#else
	(void)snprintf(inst_id, sizeof(inst_id), "wmf%d", bsscfg->_idx);
#endif // endif
	strncpy((char *)req->inst_id, inst_id, sizeof(req->inst_id));
	req->inst_id[sizeof(req->inst_id) - 1] = '\0';
	req->command_id = EMFCFG_CMD_EMF_ENABLE;
	req->size = sizeof(bool);
	req->oper_type = EMFCFG_OPER_TYPE_SET;
	*(bool *)req->arg = TRUE;

	emfc_cfg_request_process(bssinfo->emfci, req);
	if (req->status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("wl%d: %s failed\n", wlc->pub->unit, __FUNCTION__));
		MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
		return BCME_ERROR;
	}

	MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));

	return BCME_OK;
}

/*
 * Description: This function is called to stop wmf
 *		operation when bsscfg is down
 *
 * Input:       bsscfg - pointer to the bss configuration
 */
void
wlc_wmf_stop(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = wmf->wlc;
	emf_cfg_request_t *req;
	char inst_id[10];
	bss_wmf_info_t *bssinfo;

	BCM_REFERENCE(wlc);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	if (!bssinfo->wmf_instance) {
		return;
	}

	WL_WMF(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));
	if (!(req = (emf_cfg_request_t *)MALLOC(bsscfg->wlc->osh, sizeof(emf_cfg_request_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          bsscfg->wlc->pub->unit, __FUNCTION__, MALLOCED(bsscfg->wlc->osh)));
		return;
	}

	bzero((char *)req, sizeof(emf_cfg_request_t));

#ifdef BCM_NBUFF_WLMCAST_IPV6
	snprintf(inst_id, sizeof(inst_id), "%s", wl_ifname(wlc->wl, bsscfg->wlcif->wlif));
#else
	snprintf(inst_id, sizeof(inst_id), "wmf%d", bsscfg->_idx);
#endif // endif
	strncpy((char *)req->inst_id, inst_id, sizeof(req->inst_id));
	req->inst_id[sizeof(req->inst_id) - 1] = '\0';
	req->command_id = EMFCFG_CMD_EMF_ENABLE;
	req->size = sizeof(bool);
	req->oper_type = EMFCFG_OPER_TYPE_SET;
	*(bool *)req->arg = FALSE;

	emfc_cfg_request_process(bssinfo->emfci, req);
	if (req->status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("wl%d: %s failed\n", wlc->pub->unit, __FUNCTION__));
		MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
		return;
	}

	MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
}

/*
 * Description: This function is called to delete a station
 *		to emfc interface list when it is
 *              disassociated to the BSS
 *
 * Input:       bsscfg - pointer to the bss configuration
 *              scb - pointer to the scb
 */
int
wlc_wmf_sta_del(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	wlc_info_t *wlc = wmf->wlc;
	bss_wmf_info_t *bssinfo;

	BCM_REFERENCE(wlc);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	WL_WMF(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (igsc_sdb_interface_del(bssinfo->igsci, scb) != SUCCESS) {
		WL_ERROR(("wl%d: %s failed\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	if (igsc_interface_rtport_del(bssinfo->igsci, scb) != SUCCESS) {
		WL_ERROR(("wl%d: %s failed\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	return BCME_OK;
}

/*
 * Description: This function is called by EMFC layer to
 *		forward a frame on an interface
 *
 * Input:       wrapper - pointer to the bss configuration
 *              p     - Pointer to the packet buffer.
 *              mgrp_ip - Multicast destination address.
 *              txif    - Interface to send the frame on.
 *              rt_port    - router port or not
 */
static int32
_wmf_forward(void *wrapper, void *p, uint32 mgrp_ip,
#ifdef BCM_NBUFF_WLMCAST
	void *txif, int rt_port)
#else
	void *txif, bool rt_port)
#endif // endif

{
	struct ether_header *eh;
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)wrapper;
	wlc_info_t *wlc = bsscfg->wlc;
	struct scb *scb;
	osl_t *osh;
	int err;

#ifdef BCM_NBUFF_WLMCAST
	if (rt_port & 0x80) {
		_wmf_forward(wrapper, p, mgrp_ip, txif, (rt_port & 0x7f));
		return WMF_TAKEN;
	}
#endif

	osh = wlc->osh;
	scb = (struct scb *)txif;

	if (!scb || !SCB_ASSOCIATED(scb)) {
		if (p != NULL)
			PKTFREE(osh, p, FALSE);
		WL_WMF(("wl%d: %s: unknown scb %p associated %d\n",
			wlc->pub->unit, __FUNCTION__,
			OSL_OBFUSCATE_BUF(scb), scb ? SCB_ASSOCIATED(scb) : 0));
		return FAILURE;
	}

	WL_WMF(("wl%d: %s: scb "MACF" is associated\n",
		wlc->pub->unit, __FUNCTION__,
		ETHER_TO_MACF(scb->ea)));

#if defined(STS_FIFO_RXEN) || defined(WLC_OFFLOADS_RXSTS)
	if (STS_RX_ENAB(wlc->pub) || STS_RX_OFFLOAD_ENAB(wlc->pub)) {
		wlc_stsbuff_free(wlc, p);
	}
#endif /* STS_FIFO_RXEN || WLC_OFFLOADS_RXSTS */

	/* Since we are going to modify the header below and the
	 * packet may be shared we allocate a header buffer and
	 * prepend it to the original sdu.
	 */
	if (PKTSHARED(p)) {
		void *pkt;

		if ((pkt = PKTGET(osh, TXOFF + ETHER_HDR_LEN, TRUE)) == NULL) {
			WL_ERROR(("wl%d: %s: PKTGET headroom %d failed\n",
			          wlc->pub->unit, __FUNCTION__, (uint)TXOFF));
			WLCNTINCR(wlc->pub->_cnt->txnobuf);
			WLCIFCNTINCR(scb, txnobuf);
			WLCNTSCBINCR(scb->scb_stats.tx_failures);
			return FAILURE;
		}
		PKTPULL(osh, pkt, TXOFF);

		wlc_pkttag_info_move(wlc, p, pkt);
		PKTSETPRIO(pkt, PKTPRIO(p));

		/* Copy ether header from data buffer to header buffer */
		memcpy(PKTDATA(osh, pkt), PKTDATA(osh, p), ETHER_HDR_LEN);
		PKTPULL(osh, p, ETHER_HDR_LEN);

		/* Chain original sdu onto newly allocated header */
		PKTSETNEXT(osh, pkt, p);
		p = pkt;
	}

	/* Fill in the unicast address of the station into the ether dest */
	eh = (struct ether_header *) PKTDATA(osh, p);
	memcpy(eh->ether_dhost, &scb->ea, ETHER_ADDR_LEN);

	/* Intrabss: Clear pkttag information saved in recv path */
	WLPKTTAGCLEAR(p);

	/* Before forwarding, fix the priority */
	if (QOS_ENAB(wlc->pub) && (PKTPRIO(p) == 0))
		pktsetprio(p, FALSE);

	/* Send the packet using bsscfg wlcif */
	err = wlc_sendpkt(wlc, p, bsscfg->wlcif);
	if (err == BCME_OK) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

/*
 * Description: This function is called by EMFC layer to
 *		send up a frame
 *
 * Input:       wrapper - pointer to the bss configuration
 *              p     - Pointer to the packet buffer.
 */
static void
wmf_sendup(void *wrapper, void *p)
{
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)wrapper;
	wlc_info_t *wlc = bsscfg->wlc;
	wmf_info_t *wmf = wlc->wmfi;
	bss_wmf_info_t *bssinfo;

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	WL_WMF(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (!bssinfo->wmf_instance) {
		if (p != NULL)
			PKTFREE(wlc->osh, p, FALSE);
		WL_ERROR(("wl%d: %s: Cannot send packet up because WMF instance does not exist\n",
		          wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* Send the packet up */
	wlc_sendup(wlc, bsscfg, NULL, p);
}

/*
 * Description: This function is called to broadcast an IGMP query
 *		to the BSS
 *
 * Input:       wrapper - pointer to the bss configuration
 *		ip      - pointer to the ip header
 *		length  - length of the packet
 *		mgrp_ip - multicast group ip
 */
static int32
wmf_igs_broadcast(void *wrapper, uint8 *ip, uint32 length, uint32 mgrp_ip)
{
	void *pkt;
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)wrapper;
	wlc_info_t *wlc = bsscfg->wlc;
	wmf_info_t *wmf = wlc->wmfi;
	struct ether_header *eh;
	bss_wmf_info_t *bssinfo;

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	WL_WMF(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (!bssinfo->wmf_instance) {
		WL_ERROR(("wl%d: %s: Cannot send IGMP query because WMF instance does not exist\n",
		          wlc->pub->unit, __FUNCTION__));
		return FAILURE;
	}

	/* Allocate the packet, copy the ip part */
	pkt = PKTGET(wlc->osh, length + ETHER_HDR_LEN, TRUE);
	if (pkt == NULL) {
		WL_ERROR(("wl%d: %s: Out of memory allocating IGMP Query packet\n",
			wlc->pub->unit, __FUNCTION__));
		return FAILURE;
	}

	/* Add the ethernet header */
	eh = (struct ether_header *)PKTDATA(wlc->osh, pkt);
	eh->ether_type = hton16(ETHER_TYPE_IP);
	ETHER_FILL_MCAST_ADDR_FROM_IP(eh->ether_dhost, mgrp_ip);

	/* Add bsscfg address as the source ether address */
	memcpy(eh->ether_shost, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);

	/* Copy the IP part */
	memcpy((uint8 *)eh + ETHER_HDR_LEN, ip, length);

	/* Send the frame on to the bss */
	wlc_sendpkt(wlc, pkt, bsscfg->wlcif);

	return SUCCESS;
}

/*
 * Description: This function is called to register hooks
 *		into wl for packet reception
 *
 * Input:       wrapper  - pointer to the bsscfg
 */
static int32
wmf_hooks_register(void *wrapper)
{
	WL_WMF(("Calling WMF hooks register\n"));

	/*
	 * We dont need to do anything here. WMF enable status will be checked
	 * in the wl before handing off packets to WMF
	 */

	return BCME_OK;
}

/*
 * Description: This function is called to unregister hooks
 *		into wl for packet reception
 *
 * Input:       wrapper  - pointer to the bsscfg
 */
static int32
wmf_hooks_unregister(void *wrapper)
{
	WL_WMF(("Calling WMF hooks unregister\n"));

	/*
	 *  We dont need to do anything here. WMF enable status will be checked
	 * in the wl before handing off packets to WMF
	 */

	return BCME_OK;
}

/*
 * Description: This function is called to do the packet handling by
 *		WMF
 *
 * Input:       bsscfg - pointer to the bss configuration
 *              scb - pointer to the station scb
 *              p - pointer to the packet
 *		frombss - packet from BSS or DS
 */
int
wlc_wmf_packets_handle(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg, struct scb *scb,
	void *p, bool frombss)
{
	wlc_info_t *wlc = wmf->wlc;
	uint8 *iph;
	struct ether_header *eh;
#if defined(WL_IGMP_UCQUERY) || defined(WL_UCAST_UPNP)
	struct scb *scb_ptr;
	struct scb_iter scbiter;
	void *sdu_clone;
	bool ucast_convert = FALSE;
	uint32 dest_ip;
#endif /* WL_IGMP_UCQUERY */
	bss_wmf_info_t *bssinfo;
	struct scb *psta_prim;

	BCM_REFERENCE(wlc);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	/* If the WMF instance is not yet created return */
	if (!bssinfo->wmf_instance)
		return WMF_NOP;

	eh = (struct ether_header *)PKTDATA(wlc->osh, p);
	iph = (uint8 *)eh + ETHER_HDR_LEN;

	/* Only IP packets are handled */
#ifdef BCM_NBUFF_WLMCAST_IPV6
	if ((ntoh16(eh->ether_type) != ETHER_TYPE_IP) &&
	    (ntoh16(eh->ether_type) != ETHER_TYPE_IPV6))
#else
	if (ntoh16(eh->ether_type) != ETHER_TYPE_IP)
#endif // endif
		return WMF_NOP;

	if (scb != NULL) {
		psta_prim = wlc_ap_get_psta_prim(wlc->ap, scb);
	}
	else {
		psta_prim = NULL;
	}

	/* Change interface to primary interface of proxySTA */
	if (frombss && scb && psta_prim && (!bssinfo->wmf_psta_disable)) {
		scb = psta_prim;
	}

	if (WL_WMF_ON() && scb) {
		WL_WMF(("wl%d: %s: to "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
	}

#ifdef BCM_NBUFF_WLMCAST_IPV6
	/* IGMP ucast conversion for IPV4 only, for IPV6, we may need to
	 * do this same in the future? TODO
	 */
	if ((ntoh16(eh->ether_type) == ETHER_TYPE_IP)) {
#endif // endif

#if defined(WL_IGMP_UCQUERY) || defined(WL_UCAST_UPNP)
	dest_ip = ntoh32(*((uint32 *)(iph + IPV4_DEST_IP_OFFSET)));
	if ((!frombss) && (IP_VER(iph) == IP_VER_4)) {
#ifdef WL_UCAST_UPNP
		ucast_convert = bssinfo->wmf_ucast_upnp && MCAST_ADDR_UPNP_SSDP(dest_ip);
#endif /* WL_UCAST_UPNP */
#ifdef WL_IGMP_UCQUERY
		ucast_convert |= bssinfo->wmf_ucast_igmp_query &&
		        (IPV4_PROT(iph) == IP_PROT_IGMP) &&
			(*(iph + IPV4_HLEN(iph)) == IGMPV2_HOST_MEMBERSHIP_QUERY);
#endif /* WL_IGMP_UCQUERY */
		if (ucast_convert) {
			/* Convert upnp/igmp query to unicast for each assoc STA */
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb_ptr) {
				psta_prim = wlc_ap_get_psta_prim(wlc->ap, scb_ptr);
				/* Skip sending to proxy interfaces of proxySTA */
				if (psta_prim != NULL && (!bssinfo->wmf_psta_disable)) {
					continue;
				}
				WL_WMF(("wl%d: %s: send Query to "MACF"\n",
					wlc->pub->unit, __FUNCTION__,
					ETHER_TO_MACF(scb_ptr->ea)));
				if ((sdu_clone = PKTDUP(wlc->osh, p)) == NULL)
					return (WMF_NOP);
				_wmf_forward(bsscfg, sdu_clone, 0, scb_ptr, !frombss);
			}
			PKTFREE(wlc->osh, p, TRUE);
			return WMF_TAKEN;
		}
	}
#endif /* defined(WL_IGMP_UCQUERY) || defined(WL_UCAST_UPNP) */

#ifdef BCM_NBUFF_WLMCAST_IPV6
	}
#endif // endif

#ifdef BCM_NBUFF_WLMCAST_IPV6
	if ((ntoh16(eh->ether_type) == ETHER_TYPE_IPV6))
		return (emfc_ipv6_input(bssinfo->emfci, p, scb, iph, !frombss));
#endif // endif
	return (emfc_input(bssinfo->emfci, p, scb, iph, !frombss));
}

/*
 * WMF module down function
 * Does not do anything now
 */
static
int wlc_wmf_down(void *hdl)
{
	return 0;
}

/* Enable/Disable  sending multicast packets to host  for WMF instance */
int
wlc_wmf_mcast_data_sendup(wmf_info_t *wmf, wlc_bsscfg_t *bsscfg, bool set, bool enable)
{
	wlc_info_t *wlc = wmf->wlc;
	emf_cfg_request_t *req;
	bss_wmf_info_t *bssinfo;
	int rc;

	BCM_REFERENCE(wlc);

	bssinfo = BSS_WMF_INFO(wmf, bsscfg);
	ASSERT(bssinfo != NULL);

	if (!bssinfo->wmf_instance) {
		WL_ERROR(("wl%d: %s failed: WMF not enabled\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	if (!(req = (emf_cfg_request_t *)MALLOC(bsscfg->wlc->osh, sizeof(emf_cfg_request_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          bsscfg->wlc->pub->unit, __FUNCTION__, MALLOCED(bsscfg->wlc->osh)));
		return BCME_ERROR;
	}

	bzero((char *)req, sizeof(emf_cfg_request_t));

#ifdef BCM_NBUFF_WLMCAST_IPV6
	(void)snprintf((char *)req->inst_id, sizeof(req->inst_id), "%s",
			wl_ifname(wlc->wl, bsscfg->wlcif->wlif));
#else
	(void)snprintf((char *)req->inst_id, sizeof(req->inst_id), "wmf%d", bsscfg->_idx);
#endif // endif
	req->command_id = EMFCFG_CMD_MC_DATA_IND;
	req->size = sizeof(bool);
	req->oper_type = EMFCFG_OPER_TYPE_GET;
	if (set) {
		req->oper_type = EMFCFG_OPER_TYPE_SET;
		*(bool *)req->arg = enable;
	}

	emfc_cfg_request_process(bssinfo->emfci, req);
	if (req->status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("wl%d: %s failed\n", wlc->pub->unit, __FUNCTION__));
		MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
		return BCME_ERROR;
	}
	if (set) {
		MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
		return BCME_OK;
	}

	rc = (int)(*((bool *)req->arg));
	MFREE(bsscfg->wlc->osh, req, sizeof(emf_cfg_request_t));
	return rc;
}

static void
wlc_wmf_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wmf_info_t *wmf = (wmf_info_t *)ctx;

	/* Delete WMF instance if it created for this bsscfg */
	wlc_wmf_instance_del(wmf, cfg);
}
