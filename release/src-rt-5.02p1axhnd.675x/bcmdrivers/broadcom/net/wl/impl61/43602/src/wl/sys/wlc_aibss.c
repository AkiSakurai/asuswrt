/**
 * Advanced IBSS implementation for Broadcom 802.11 Networking Driver
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
 * $Id: wlc_aibss.c 593242 2015-10-15 20:31:28Z $
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
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/vlan.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wl_export.h>
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif // endif
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#include <wlc_utils.h>
#include <wlc_scb.h>
#include <wlc_pcb.h>
#include <wlc_tbtt.h>
#include <wlc_apps.h>
#include <wlc_aibss.h>
#if defined(SAVERESTORE)
#include <saverestore.h>
#endif /* SAVERESTORE */
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_types.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_stf.h>
#include <wlc_scan.h>

/* iovar table */
enum {
	IOV_AIBSS,			/* enable/disable AIBSS feature */
	IOV_AIBSS_BCN_FORCE_CONFIG,	/* bcn xmit configuration */
	IOV_AIBSS_TXFAIL_CONFIG,
	IOV_AIBSS_IFADD,		/* add AIBSS interface */
	IOV_AIBSS_IFDEL,		/* delete AIBSS interface */
	IOV_AIBSS_PS
	};

static const bcm_iovar_t aibss_iovars[] = {
	{"aibss", IOV_AIBSS, (IOVF_BSS_SET_DOWN), IOVT_BOOL, 0},
	{"aibss_bcn_force_config", IOV_AIBSS_BCN_FORCE_CONFIG,
	(IOVF_BSS_SET_DOWN), IOVT_BUFFER, sizeof(aibss_bcn_force_config_t)},
	{"aibss_txfail_config", IOV_AIBSS_TXFAIL_CONFIG,
	(0), IOVT_BUFFER, (OFFSETOF(aibss_txfail_config_t, max_atim_failure))},
	{"aibss_ifadd", IOV_AIBSS_IFADD, 0, IOVT_BUFFER, sizeof(wl_aibss_if_t)},
	{"aibss_ifdel", IOV_AIBSS_IFDEL, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"aibss_ps", IOV_AIBSS_PS, (IOVF_SET_DOWN), IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};

/* AIBSS module specific state */
typedef struct wlc_aibss_info_priv {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	wlc_pub_t *pub;
	osl_t *osh;
	uint32	initial_min_bcn_dur;	/* duration to check if STA xmitted 1 bcn */
	uint32	bcn_flood_dur;
	uint32	min_bcn_dur;	/* duration to check if STA xmitted 1 bcn after bcn_flood time */
	struct wl_timer *ibss_timer;	/* per entry timer */
	uint32	ibss_start_time;	/* ticks when the IBSS started */
	uint32 last_txbcnfrm;		/* maintains the prev txbcnfrm count */
	int32 cfg_cubby_handle;		/* BSSCFG cubby offset */
	wlc_bsscfg_t *bsscfg;		/* dedicated bsscfg for IBSS */
#ifdef WLC_AIBSS_ROM_COMPAT
	/* Added to avoid RAM-ROM structure mismatch
	 * for 4358-roml/pcie compile, should not be merged to TOB and please don't use them
	 */
	bool use_act_frame;
	bool ctw_enabled;
	struct wl_timer *idle_timer;
	uint32 idle_timeout;
#endif // endif
	struct pktq	atimq;			/* ATIM queue */
	uint32 wakeup_end_time;		/* ticks when initial wakeup time finished */
} wlc_aibss_info_priv_t;

typedef struct  {
	wlc_aibss_info_t aibss_pub;
	wlc_aibss_info_priv_t aibss_priv;
} wlc_aibss_mem_t;

typedef struct aibss_cfg_info {
	uint32 bcn_timeout;		/* dur in seconds to receive 1 bcn */
	uint32 max_tx_retry;		/* no of consecutive no acks to send txfail event */
#ifdef WLC_AIBSS_ROM_COMPAT
	/* added to avoid RAM-ROM structure mismatch for 4358-roml/pcie
	 * compile, not to be merged to TOB and please don't use it
	 */
	uint32 pend_multicast;
#endif /* WLC_AIBSS_ROM_COMPAT */
	bool pm_allowed;
	bool bcmc_pend;
	uint32 max_atim_failure;
	uint32 force_wake;			/* bit mask of wake_bits */
} aibss_cfg_info_t;

/*  number of TXFAIL events to send before initiate scb_free */
#define TXFEVT_CNT 8
#define ATIMQ_LEN MAXSCB				/* ATIM PKT Q length */

#define WLC_AIBSS_INFO_SIZE (sizeof(wlc_aibss_mem_t))
#define UCODE_IBSS_PS_SUPPORT_VER		(0x3ae0000)		/* BOM version 942.0 */

#define TX_FIFO_BITMAP(fifo)		(1<<(fifo))	/* TX FIFO number to bit map */

#define AIBSS_PMQ_INT_THRESH		(0x40) /* keep thres high to avoid PMQ interrupt */
#define AIBSS_SCB_FREE_BCN_THRESH		(3)	/* 3 seconds of no Beacons */
#define AIBSS_SCB_FREE_ATIM_FAIL_THRESSH	(30) /* 30 ATIM failures for scb_free */

static uint16 wlc_aibss_info_priv_offset = OFFSETOF(wlc_aibss_mem_t, aibss_priv);

/* module specific states location */
#define WLC_AIBSS_INFO_PRIV(aibss_info) \
	((wlc_aibss_info_priv_t *)((uint8*)(aibss_info) + wlc_aibss_info_priv_offset))

static int wlc_aibss_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static void wlc_aibss_timer(void *arg);
static void wlc_aibss_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt);
static bool wlc_aibss_validate_chanspec(wlc_info_t *wlc, chanspec_t chanspec);
static void _wlc_aibss_tbtt_impl_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data);
static void _wlc_aibss_tbtt_impl(wlc_aibss_info_t *aibss);
static void _wlc_aibss_pretbtt_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data);
static void _wlc_aibss_ctrl_pkt_txstatus(wlc_info_t *wlc, void *pkt, uint txstatus);
static bool _wlc_aibss_sendatim(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb);
static void _wlc_aibss_send_atim_q(wlc_info_t *wlc, struct pktq *q);
static bool _wlc_aibss_check_pending_data(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool force_all);
static void _wlc_aibss_set_active_scb(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *cfg);
static void wlc_aibss_pretbtt_query_cb(void *ctx, bss_pretbtt_query_data_t *notif_data);
static void _wlc_aibss_data_pkt_txstatus(wlc_info_t *wlc, void *pkt, uint txstatus);
static void _wlc_aibss_set_active_scb_fifo(wlc_aibss_info_t * aibss_info, wlc_bsscfg_t * cfg);
static int wlc_aibss_bcn_parse_ibss_param_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_aibss_scb_init(void *context, struct scb *scb);
static void wlc_aibss_scbfree(wlc_aibss_info_t *aibss_info, struct scb *scb);
static void wlc_aibss_scb_deinit(void *context, struct scb *scb);
/* aibss moduule on wlc BH handler  */
static int wlc_aibss_wl_up(void *ctx);
static void
wlc_aibss_send_txfail_event(wlc_aibss_info_t *aibss_info, struct scb *scb, uint32 evt_sybtype);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

wlc_aibss_info_t *
BCMATTACHFN(wlc_aibss_attach)(wlc_info_t *wlc)
{
	wlc_aibss_info_t *aibss_info;
	wlc_aibss_info_priv_t *aibss;

	/* sanity checks */
	if (!(aibss_info = (wlc_aibss_info_t *)MALLOCZ(wlc->osh, WLC_AIBSS_INFO_SIZE))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss->wlc = wlc;
	aibss->pub = wlc->pub;
	aibss->osh = wlc->osh;

	/* create IBSS timer for xmit extra beacons */
	if ((aibss->ibss_timer =
	    wl_init_timer(wlc->wl, wlc_aibss_timer, aibss_info, "ibss_timer")) == NULL) {
		WL_ERROR(("wl%d: wl_init_timer for AIBSS failed\n", wlc->pub->unit));
		goto fail;
	}

	/* reserve cubby in the bsscfg container for private data */
	if ((aibss->cfg_cubby_handle = wlc_bsscfg_cubby_reserve(wlc, sizeof(aibss_cfg_info_t),
		NULL, NULL, NULL, (void *)aibss_info)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the scb container to monitor per SCB tx stats */
	if ((aibss_info->scb_handle = wlc_scb_cubby_reserve(aibss->wlc,
		sizeof(aibss_scb_info_t), wlc_aibss_scb_init, wlc_aibss_scb_deinit,
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
		NULL, aibss_info, 0)) < 0) {
#else
		NULL, aibss_info)) < 0) {
#endif // endif
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_aibss_bss_updn, aibss_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register pretbtt query callback */
	if (wlc_bss_pretbtt_query_register(wlc, wlc_aibss_pretbtt_query_cb, aibss) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_pretbtt_query_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register packet callback function */
	if (wlc_pcb_fn_set(wlc->pcb, 1, WLF2_PCB2_AIBSS_CTRL, _wlc_aibss_ctrl_pkt_txstatus)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register packet callback function */
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_AIBSS_DATA, _wlc_aibss_data_pkt_txstatus)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, aibss_iovars, "aibss",
		aibss_info, wlc_aibss_doiovar, NULL, wlc_aibss_wl_up, NULL)) {
		WL_ERROR(("wl%d: AIBSS wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	/* bcn atim processing */
	if (wlc_iem_add_parse_fn(wlc->iemi, FC_BEACON, DOT11_MNG_IBSS_PARMS_ID,
	                         wlc_aibss_bcn_parse_ibss_param_ie, aibss_info) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, atim in bcn\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Initialize the pktq */
	pktq_init(&aibss->atimq, 1, ATIMQ_LEN);
	return aibss_info;

fail:
	if (aibss->ibss_timer != NULL) {
		wl_free_timer(wlc->wl, aibss->ibss_timer);
		aibss->ibss_timer = NULL;
	}

	MFREE(wlc->osh, aibss_info, WLC_AIBSS_INFO_SIZE);

	return NULL;
}

void
BCMATTACHFN(wlc_aibss_detach)(wlc_aibss_info_t *aibss_info)
{
	wlc_aibss_info_priv_t *aibss;
	wlc_info_t	*wlc;

	if (!aibss_info)
		return;

	aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc = aibss->wlc;

	if (aibss->ibss_timer != NULL) {
		wl_free_timer(wlc->wl, aibss->ibss_timer);
		aibss->ibss_timer = NULL;
	}

	wlc_module_unregister(aibss->pub, "aibss", aibss_info);

	MFREE(aibss->wlc->osh, aibss, WLC_AIBSS_INFO_SIZE);
}

static int
wlc_aibss_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)hdl;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);

	int32 int_val = 0;
	bool bool_val;
	uint32 *ret_uint_ptr;
	int err = 0;
	wlc_bsscfg_t *bsscfg;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	ret_uint_ptr = (uint32 *)a;

	bsscfg = wlc_bsscfg_find_by_wlcif(aibss->wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {

		case IOV_GVAL(IOV_AIBSS):
			*ret_uint_ptr = AIBSS_ENAB(aibss->pub);
			break;

		case IOV_SVAL(IOV_AIBSS):
			aibss->pub->_aibss = bool_val;
			break;

		case IOV_GVAL(IOV_AIBSS_BCN_FORCE_CONFIG):{
			aibss_bcn_force_config_t *bcn_config = (aibss_bcn_force_config_t *)a;

			store16_ua(&bcn_config->version, AIBSS_BCN_FORCE_CONFIG_VER_0);
			store16_ua(&bcn_config->len, sizeof(aibss_bcn_force_config_t));
			store32_ua(&bcn_config->initial_min_bcn_dur, aibss->initial_min_bcn_dur);
			store32_ua(&bcn_config->bcn_flood_dur, aibss->bcn_flood_dur);
			store32_ua(&bcn_config->min_bcn_dur, aibss->min_bcn_dur);
			break;
		}

		case IOV_SVAL(IOV_AIBSS_BCN_FORCE_CONFIG):{
			aibss_bcn_force_config_t *bcn_config = (aibss_bcn_force_config_t *)p;

			aibss->initial_min_bcn_dur = load32_ua(&bcn_config->initial_min_bcn_dur);
			aibss->bcn_flood_dur = load32_ua(&bcn_config->bcn_flood_dur);
			aibss->min_bcn_dur = load32_ua(&bcn_config->min_bcn_dur);
			break;
		}

		case IOV_GVAL(IOV_AIBSS_TXFAIL_CONFIG): {
			aibss_txfail_config_t *txfail_config = (aibss_txfail_config_t *)a;
			aibss_cfg_info_t	*cfg_cubby;

			if (!BSSCFG_IBSS(bsscfg)) {
				err = BCME_ERROR;
				break;
			}

			cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);
			store16_ua((uint8 *)&txfail_config->version, AIBSS_TXFAIL_CONFIG_CUR_VER);
			store16_ua((uint8 *)&txfail_config->len, sizeof(aibss_txfail_config_t));
			store32_ua((uint8 *)&txfail_config->bcn_timeout, cfg_cubby->bcn_timeout);
			store32_ua((uint8 *)&txfail_config->max_tx_retry, cfg_cubby->max_tx_retry);
			if (alen >= sizeof(aibss_txfail_config_t)) {
				store32_ua((uint8 *)&txfail_config->max_atim_failure,
					cfg_cubby->max_atim_failure);
			}
			break;
		}

		case IOV_SVAL(IOV_AIBSS_TXFAIL_CONFIG): {
			aibss_txfail_config_t *txfail_config = (aibss_txfail_config_t *)a;
			aibss_cfg_info_t	*cfg_cubby;
			uint16 version = load16_ua(&txfail_config->version);
			uint16 len = load16_ua(&txfail_config->len);

			if (!BSSCFG_IBSS(bsscfg)) {
				err = BCME_ERROR;
				break;
			}

			if (version > AIBSS_TXFAIL_CONFIG_CUR_VER) {
				err = BCME_VERSION;
				break;
			}

			cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);
			cfg_cubby->bcn_timeout = load32_ua((uint8 *)&txfail_config->bcn_timeout);
			cfg_cubby->max_tx_retry = load32_ua((uint8 *)&txfail_config->max_tx_retry);
			if (version == AIBSS_TXFAIL_CONFIG_VER_1 &&
				len == sizeof(aibss_txfail_config_t)) {
				cfg_cubby->max_atim_failure =
					load32_ua((uint8 *)&txfail_config->max_atim_failure);
			}
			else if (BSSCFG_IS_AIBSS_PS_ENAB(bsscfg)) {
				cfg_cubby->max_atim_failure = WLC_AIBSS_DEFAULT_ATIM_FAILURE;
			}
			break;
		}
		case IOV_SVAL(IOV_AIBSS_IFADD): {
			wl_aibss_if_t *aibss_if = (wl_aibss_if_t*)a;
			wlc_info_t *wlc = aibss->wlc;
			wlc_bsscfg_t *new_bsscfg;
			int idx;
			uint32 flags = WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB |
				WLC_BSSCFG_TX_SUPR_ENAB;

			/* return failure if
			 *	1) primary interface is running is AP/IBSS mode
			 *	2) a dedicated IBSS interface already exists
			 *	3) TODO: any other AP/GO interface exists
			 */
			if (!wlc->cfg->BSS || BSSCFG_AP(wlc->cfg) || aibss->bsscfg != NULL) {
				err = BCME_BADARG;
				break;
			}

			/* bsscfg with the given MAC address exists */
			if (wlc_bsscfg_find_by_hwaddr(wlc, &aibss_if->addr) != NULL) {
				err = BCME_BADARG;
				break;
			}

			/* validate channel spec */
			if (!wlc_aibss_validate_chanspec(wlc, aibss_if->chspec)) {
				err = BCME_BADARG;
				break;
			}

			/* try to allocate one bsscfg for IBSS */
			if ((idx = wlc_bsscfg_get_free_idx(wlc)) == -1) {
				WL_ERROR(("wl%d: no free bsscfg\n", wlc->pub->unit));
				err = BCME_ERROR;
				break;
			}
			if ((new_bsscfg = wlc_bsscfg_alloc(wlc, idx, flags,
				&aibss_if->addr, FALSE)) == NULL) {
				WL_ERROR(("wl%d: cannot create bsscfg\n", wlc->pub->unit));
				err = BCME_ERROR;
				break;
			}
			if ((err = wlc_bsscfg_init(wlc, new_bsscfg)) != BCME_OK) {
				WL_ERROR(("wl%d: failed to init bsscfg\n", wlc->pub->unit));
				wlc_bsscfg_free(wlc, new_bsscfg);
				break;
			}

#ifdef WLMCHAN
			/* save the chanspec for mchan */
			if (MCHAN_ENAB(wlc->pub))
				new_bsscfg->chanspec = aibss_if->chspec;
#endif // endif
			break;
		}
		case IOV_SVAL(IOV_AIBSS_IFDEL): {
			wlc_info_t *wlc = aibss->wlc;
			wlc_bsscfg_t *del_bsscfg = wlc_bsscfg_find_by_hwaddr(wlc, a);

			/* bsscfg not found */
			if (del_bsscfg == NULL || aibss->bsscfg == NULL) {
				err = BCME_BADARG;
				break;
			}

			/* can't delete current IOVAR processing bsscfg */
			if (del_bsscfg == bsscfg) {
				err = BCME_BADARG;
				break;
			}

			if (del_bsscfg->enable)
				wlc_bsscfg_disable(wlc, del_bsscfg);
			wlc_bsscfg_free(wlc, del_bsscfg);
			aibss->bsscfg = NULL;
			break;
		}

		case IOV_GVAL(IOV_AIBSS_PS):
			*ret_uint_ptr = (int32)aibss->wlc->pub->_aibss_ps;
			break;

		case IOV_SVAL(IOV_AIBSS_PS):
			aibss->wlc->pub->_aibss_ps = bool_val;
			break;

		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

/* bsscfg up/down */
static void
wlc_aibss_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)ctx;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss_cfg_info_t	*cfg_cubby;
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *cfg;

	ASSERT(evt != NULL);

	cfg = evt->bsscfg;
	ASSERT(cfg != NULL);

	if (!AIBSS_ENAB(wlc->pub) || !BSSCFG_IBSS(cfg))
		return;

	cfg_cubby = BSSCFG_CUBBY(cfg, aibss->cfg_cubby_handle);
	if (evt->up) {
		uint32 val = 0;
		uint32 mask = (MCTL_INFRA | MCTL_DISCARD_PMQ);
#ifdef WLMCNX
		int bss = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
#endif /* WLMCNX */

		if (wlc->pub->_aibss_ps && wlc->ucode_rev < UCODE_IBSS_PS_SUPPORT_VER) {
			WL_ERROR(("wl%d: %s: AIBSS PS feature not supported\n",
				aibss->wlc->pub->unit, __FUNCTION__));
			return;
		}

		if (wlc_tbtt_ent_fn_add(wlc->tbtt, cfg, _wlc_aibss_pretbtt_cb,
			_wlc_aibss_tbtt_impl_cb, wlc->aibss_info) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_tbtt_ent_fn_add() failed\n",
			  wlc->pub->unit, __FUNCTION__));
			return;
		}

		aibss->bsscfg = cfg;

		/* Set MCTL_INFRA for non-primary AIBSS interface to support concurrent mode */
		if (cfg != wlc->cfg) {
			val |= MCTL_INFRA;
		}

		/* DisabLe PMQ entries to avoid uCode updating pmq entry on  rx packets */
		val |= MCTL_DISCARD_PMQ;

		/* Update PMQ Interrupt threshold to avoid MI_PMQ interrupt */
		wlc_write_shm(wlc, SHM_PMQ_INTR_THRESH, AIBSS_PMQ_INT_THRESH);

		wlc_ap_ctrl(wlc, TRUE, cfg, -1);
		wlc_mctrl(wlc, mask, val);

		/* Set IBSS and GO bit to enable beaconning */
#ifdef WLMCNX
		wlc_mcnx_mac_suspend(wlc->mcnx);
		wlc_mcnx_write_shm(wlc->mcnx, M_P2P_BSS_ST(bss),
			(M_P2P_BSS_ST_GO | M_P2P_BSS_ST_IBSS));
		wlc_mcnx_mac_resume(wlc->mcnx);
#endif /* WLMCNX */

		/* if configured, start timer to check for bcn xmit */
		if (aibss->initial_min_bcn_dur && aibss->bcn_flood_dur &&
			aibss->min_bcn_dur) {

			/* IBSS interface up, start the timer to monitor the
			 * beacon transmission
			 */
			aibss->ibss_start_time = OSL_SYSUPTIME();
			wl_add_timer(wlc->wl, aibss->ibss_timer, aibss->initial_min_bcn_dur,
				FALSE);
			if (AIBSS_BSS_PS_ENAB(cfg)) {
				cfg_cubby->force_wake |= WLC_AIBSS_FORCE_WAKE_INITIAL;
				aibss->wakeup_end_time =
					OSL_SYSUPTIME() + WLC_AIBSS_INITIAL_WAKEUP_PERIOD*1000;
			}

			/* Disable PM till the flood beacon is complete */
			wlc_set_pmoverride(cfg, TRUE);
		}
#ifdef WME
		if ((cfg->flags & SCB_WMECAP)) {
			if (cfg->bcmc_scb[BAND_2G_INDEX])
				cfg->bcmc_scb[BAND_2G_INDEX]->flags |= SCB_WMECAP;
			if (cfg->bcmc_scb[BAND_5G_INDEX])
				cfg->bcmc_scb[BAND_5G_INDEX]->flags |= SCB_WMECAP;
		}
#endif /* WME */
	}
	else {
		if (wlc_tbtt_ent_fn_del(wlc->tbtt, cfg, _wlc_aibss_pretbtt_cb,
			_wlc_aibss_tbtt_impl_cb, wlc->aibss_info) != BCME_OK) {
			WL_ERROR(("wl%d: %s: wlc_tbtt_ent_fn_del() failed\n",
			  wlc->pub->unit, __FUNCTION__));
			return;
		}
		aibss->bsscfg = NULL;

		/* IBSS not enabled, stop monitoring the link */
		if (aibss->ibss_start_time) {
			aibss->ibss_start_time = 0;
			wl_del_timer(wlc->wl, aibss->ibss_timer);
		}

	}
}

void
wlc_aibss_timer(void *arg)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)arg;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	uint32	timer_dur = aibss->min_bcn_dur;
	wl_cnt_t *cnt = wlc->pub->_cnt;
	wlc_bsscfg_t *cfg = aibss->bsscfg;
	aibss_cfg_info_t	*cfg_cubby = BSSCFG_CUBBY(cfg, aibss->cfg_cubby_handle);
	struct scb_iter scbiter;
	struct scb *scb;

	WL_TRACE(("wl%d: wlc_ibss_timer", wlc->pub->unit));

	if (!wlc->pub->up || !AIBSS_ENAB(wlc->pub)) {
		return;
	}

	/* Delayed SCB Free
	 * SCB is freed if there is no bcn for some time. The scb free is delayed
	 * if there are any pending pkts in the FIFO for the PEER.
	 */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		aibss_scb_info_t	*scb_info = SCB_CUBBY((scb), aibss_info->scb_handle);
		if (scb_info->free_scb) {
			wlc_aibss_scbfree(aibss_info, scb);
		}
	}

	if (aibss->ibss_start_time &&
		((OSL_SYSUPTIME() - aibss->ibss_start_time) < aibss->bcn_flood_dur)) {
		timer_dur = aibss->initial_min_bcn_dur;
	}
	else if (cfg->pm->PM_override) {
		wlc_set_pmoverride(cfg, FALSE);
	}

	if (AIBSS_BSS_PS_ENAB(cfg) && aibss->wakeup_end_time &&
		(OSL_SYSUPTIME() >= aibss->wakeup_end_time)) {
		cfg_cubby->force_wake &= ~WLC_AIBSS_FORCE_WAKE_INITIAL;
		aibss->wakeup_end_time = 0;
	}
	/* Get the number of beacons sent */
	wlc_statsupd(wlc);

	/* If no beacons sent from prev timer, send one */
	if ((cnt->txbcnfrm - aibss->last_txbcnfrm) == 0)
	{
		/* Not sent enough bcn, send bcn in next TBTT
		 * even if we recv bcn from a peer IBSS STA
		 */
		wlc_bmac_mhf(wlc->hw, MHF1, MHF1_FORCE_SEND_BCN,
			MHF1_FORCE_SEND_BCN, WLC_BAND_ALL);
	}
	else {
		wlc_bmac_mhf(wlc->hw, MHF1, MHF1_FORCE_SEND_BCN, 0, WLC_BAND_ALL);

		/* update the beacon counter */
		aibss->last_txbcnfrm = cnt->txbcnfrm;
	}

	/* ADD TIMER */
	wl_add_timer(wlc->wl, aibss->ibss_timer, timer_dur, FALSE);
}

void
wlc_aibss_check_txfail(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	aibss_cfg_info_t	*cfg_cubby = BSSCFG_CUBBY(cfg, aibss->cfg_cubby_handle);
	aibss_scb_info_t	*scb_info = SCB_CUBBY((scb), aibss_info->scb_handle);

	if (cfg_cubby->bcn_timeout &&
		(scb_info->no_bcn_counter >= cfg_cubby->bcn_timeout)) {
		wlc_bss_mac_event(wlc, cfg, WLC_E_AIBSS_TXFAIL, &scb->ea,
		                  WLC_E_STATUS_FAIL, AIBSS_BCN_FAILURE,
		                  0, NULL, 0);

		/* Reset the counters */
		scb_info->no_bcn_counter = 0;
		wlc_aibss_scbfree(aibss_info, scb);
	}

	if (cfg_cubby->bcn_timeout == 0 &&
		scb_info->no_bcn_counter > AIBSS_SCB_FREE_BCN_THRESH) {
		wlc_aibss_scbfree(aibss_info, scb);

	}
}

static bool wlc_aibss_validate_chanspec(wlc_info_t *wlc, chanspec_t chanspec)
{
	/* use default chanspec */
	if (chanspec == 0)
		return TRUE;

	/* validate chanspec */
	if (wf_chspec_malformed(chanspec) || !wlc_valid_chanspec_db(wlc->cmi, chanspec) ||
		wlc_radar_chanspec(wlc->cmi, chanspec))
		return FALSE;

	/* If mchan not enabled, don't allow IBSS on different channel */
	if (!MCHAN_ENAB(wlc->pub) && wlc->pub->associated && chanspec != wlc->home_chanspec) {
		return FALSE;
	}

	return TRUE;
}

void wlc_aibss_tbtt(wlc_aibss_info_t *aibss_info)
{
	_wlc_aibss_tbtt_impl(aibss_info);
}

bool wlc_aibss_sendpmnotif(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *bsscfg,
	ratespec_t rate_override, int prio, bool track)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	aibss_cfg_info_t	*cfg_cubby;

	if (bsscfg == NULL || bsscfg != aibss->bsscfg)
		return FALSE;

	cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);

	/* Let wlc_aibss_atim_window_end take care of putting the device to sleep */
	cfg_cubby->pm_allowed = FALSE;
	wlc_set_ps_ctrl(bsscfg);

	/* Clear PM states since IBSS PS is not depend on it */
	bsscfg->pm->PMpending = FALSE;
	wlc_pm_pending_complete(wlc);

	return TRUE;
}

void _wlc_aibss_ctrl_pkt_txstatus(wlc_info_t *wlc, void *pkt, uint txstatus)
{
	struct scb *scb;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	aibss_cfg_info_t	*cfg_cubby;
	aibss_scb_info_t *scb_info;

	scb = WLPKTTAGSCBGET(pkt);
	if (scb == NULL)
		return;
	bsscfg = SCB_BSSCFG(scb);

	if (bsscfg == NULL || bsscfg != aibss->bsscfg)
		return;

	scb_info = SCB_CUBBY((scb), wlc->aibss_info->scb_handle);

	 if (SCB_ISMULTI(scb)) {
	/* its bcast atim  */
		if (txstatus != 0) {
		/* for bcast atims we assume status 0x1000 is OK */
			scb_info->atim_acked = TRUE;
		} else {
			AIBSS_INFO(("%s BCAST txstatus:%x, atim:%d, bcmc:%d\n",
				__FUNCTION__, txstatus, TXPKTPENDGET(wlc, TX_ATIM_FIFO),
				TXPKTPENDGET(wlc, TX_BCMC_FIFO)));
		}
		return;
	}

	cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);

	if (txstatus & TX_STATUS_ACK_RCV) {
		WL_PS(("%s():wl%d: ATIM frame %p sent\n",
			__FUNCTION__, wlc->pub->unit, pkt));

		scb_info->atim_acked = TRUE;
		scb_info->atim_failure_count = 0;

		/* scb is back or it'sbeen Ok,
		 * cancel scb free(if was ordered) & reinit txfail limiter
		 */
		scb_info->evt_sent_cnt = TXFEVT_CNT;
		scb_info->free_scb = FALSE;

		/* Clear PS state if we receive an ACK for atim frame */
		if (SCB_PS(scb)) {
			wlc_apps_process_ps_switch(wlc, &scb->ea, FALSE);
		}
	}
	else {
		WL_PS(("%s():wl%d: ATIM frame %p sent (no ACK)\n",
			__FUNCTION__, wlc->pub->unit, pkt));
		scb_info->atim_failure_count++;

		if (cfg_cubby->max_atim_failure &&
			scb_info->atim_failure_count >= cfg_cubby->max_atim_failure) {
			wlc_aibss_send_txfail_event(wlc->aibss_info, scb, AIBSS_ATIM_FAILURE);
		}

		scb_info->atim_failure_count++;
		/* XXX minimize PS state change, let atim_window_end
		 * take care of enabling PS
		 */
	}

	if (!pktq_empty(&aibss->atimq)) {
		_wlc_aibss_send_atim_q(wlc, &aibss->atimq);
	}
}

void _wlc_aibss_pretbtt_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data)
{
	_wlc_aibss_tbtt_impl(aibss_info);
}

void _wlc_aibss_tbtt_impl_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data)
{
	/* TBTT CLBK */
}

void _wlc_aibss_tbtt_impl(wlc_aibss_info_t *aibss_info)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	aibss_cfg_info_t	*cfg_cubby;
	uint16 mhf_val;
	uint32 pat_hi, pat_lo;
	struct ether_addr eaddr;
	char eabuf[ETHER_ADDR_STR_LEN];
	volatile uint16 *pmqctrlstatus = (volatile uint16 *)&wlc->regs->pmqreg.w.pmqctrlstatus;
	volatile uint32 *pmqhostdata = (volatile uint32 *)&wlc->regs->pmqreg.pmqhostdata;

	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(eaddr);

	if (bsscfg == NULL) {
		return;
	}

	/* covers rare case when atim(s) got q'ed up from prev prev bcn interval */
	if (TXPKTPENDGET(wlc, TX_ATIM_FIFO)) {
		AIBSS_INFO(("%s !!! ATIM fifo has %d pkts flush it\n",
			__FUNCTION__, TXPKTPENDGET(wlc, TX_ATIM_FIFO)));
		wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_ATIM_FIFO), FLUSHFIFO);
		pktq_flush(wlc->osh, &aibss->atimq, TRUE, NULL, 0);
	}
	/* read entries until empty or pmq exeeding limit */
	while ((R_REG(wlc->osh, pmqhostdata)) & PMQH_NOT_EMPTY) {
		pat_lo = R_REG(wlc->osh, &wlc->regs->pmqpatl);
		pat_hi = R_REG(wlc->osh, &wlc->regs->pmqpath);
		eaddr.octet[5] = (pat_hi >> 8)  & 0xff;
		eaddr.octet[4] =  pat_hi	& 0xff;
		eaddr.octet[3] = (pat_lo >> 24) & 0xff;
		eaddr.octet[2] = (pat_lo >> 16) & 0xff;
		eaddr.octet[1] = (pat_lo >> 8)  & 0xff;
		eaddr.octet[0] =  pat_lo	& 0xff;

		WL_PS(("wl%d.%d: ATIM failed, pmq entry added for %s\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg), bcm_ether_ntoa(&eaddr, eabuf)));
	}

	/* Clear all the PMQ entry before sending ATIM frames */
	W_REG(wlc->osh, pmqctrlstatus, PMQH_DEL_MULT);

	/* Check for BCN Flood status */
	mhf_val =  wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_AUTO);
	if (mhf_val & MHF1_FORCE_SEND_BCN) {
		wl_cnt_t *cnt = wlc->pub->_cnt;

		wlc_statsupd(wlc);

		/* check if we have sent atleast 1 bcn and clear
		 * the force bcn bit
		 */
		if (cnt->txbcnfrm != aibss->last_txbcnfrm) {
			wlc_bmac_mhf(wlc->hw, MHF1, MHF1_FORCE_SEND_BCN, 0, WLC_BAND_ALL);
		}
	}

	/* if off channel (scan is in progress) don't  try sending any dta or bcmc */
	if (SCAN_IN_PROGRESS(wlc->scan) &&
		(bsscfg->chanspec != wlc->chanspec)) {
		AIBSS_INFO(("%s:scan:1 aibss ch:%x, scan:%x, defer all TX\n",
			__FUNCTION__,
			bsscfg->chanspec,
			wlc->chanspec));
		return;
	}

	if (AIBSS_BSS_PS_ENAB(bsscfg)) {
		cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);

		cfg_cubby->pm_allowed = FALSE;
		wlc_set_ps_ctrl(bsscfg);

		/* BCMC traffic */
		if (TXPKTPENDGET(wlc, TX_BCMC_FIFO)) {
			/* XXX uCodes sends the BCMC packet in the atim window
			 * itself - need to debug
			 */
			cfg_cubby->bcmc_pend = TRUE;
			_wlc_aibss_sendatim(wlc, bsscfg, WLC_BCMCSCB_GET(wlc, bsscfg));
		}

		/* Check pending packets for peers and sendatim to them */
		_wlc_aibss_set_active_scb(aibss_info, bsscfg);
		_wlc_aibss_check_pending_data(wlc, bsscfg, FALSE);
	}

	return;
}

void
wlc_aibss_ps_start(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *bcmc_scb;

	WL_PS(("%s: Entering\n", __FUNCTION__));

	/* Enable PS for BCMC scb */
	bcmc_scb = WLC_BCMCSCB_GET(wlc, cfg);
	ASSERT(bcmc_scb);
	bcmc_scb->PS = TRUE;

	/* In case of AIBSS PS on primary interface host might enable
	 * PM mode well before the IBSS PS is enabled. PMEnabled will not
	 * be set untill IBSS PS is set so update correct pm state.
	 */
	wlc_set_pmstate(cfg, (cfg->pm->PM != PM_OFF));

	/* PS for other SCB shall be enabled after first ATIM window */
}

void
wlc_aibss_ps_stop(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb_iter scbiter;
	struct scb *scb, *bcmc_scb;

	WL_PS(("%s: Entering\n", __FUNCTION__));

	/* Assume all sta in PS and pull them out one by one */
	if (!BSSCFG_IBSS(cfg)) {
		return;
	}

	bcmc_scb = WLC_BCMCSCB_GET(wlc, cfg);
	ASSERT(bcmc_scb);
	bcmc_scb->PS = FALSE;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		if (SCB_PS(scb)) {
			wlc_apps_process_ps_switch(wlc, &scb->ea, FALSE);
		}
	}
}

void wlc_aibss_atim_window_end(wlc_info_t *wlc)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	struct scb *scb;
	struct scb_iter scbiter;
	bool be_awake = FALSE;
	aibss_cfg_info_t *cfg_cubby;

	aibss_scb_info_t *bcmc_scb_info =
		SCB_CUBBY((WLC_BCMCSCB_GET(wlc, bsscfg)), wlc->aibss_info->scb_handle);

	if (aibss->bsscfg == NULL || !AIBSS_BSS_PS_ENAB(aibss->bsscfg)) {
		WL_ERROR(("wl%d: %s: ATIM enabled in non AIBSS mode\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		aibss_scb_info_t *scb_info = SCB_CUBBY((scb), wlc->aibss_info->scb_handle);
		bool scb_ps_on = TRUE;

		if ((scb_info->atim_acked) ||
			scb_info->atim_rcvd || !scb_info->ps_enabled) {
			scb_ps_on = FALSE;
		}

		if (SCB_PS(scb) != scb_ps_on) {
			wlc_apps_process_ps_switch(wlc, &scb->ea, scb_ps_on);
		}

		scb_info->atim_rcvd = FALSE;
		scb_info->atim_acked = FALSE;

		if (!SCB_PS(scb)) {
			be_awake = TRUE;
		}
	}

	// if we have no uncast atims sent/rcvd to/from scbs
	// we only stay awake if bcast atim didn't fail. Or if forced
	if (!be_awake && (bcmc_scb_info->atim_acked ||
		cfg_cubby->force_wake)) {
		be_awake = TRUE;

	}
	bcmc_scb_info->atim_acked = FALSE;

	/* Set only if we need to sleep or continue to stay awake */
	/* Set it in pretbtt */
	if (!be_awake) {
		uint16 st;
#ifdef WLMCNX
		int bss = wlc_mcnx_BSS_idx(wlc->mcnx, bsscfg);
#endif /* WLMCNX */

		cfg_cubby->pm_allowed = TRUE;
		wlc_set_ps_ctrl(bsscfg);

#ifdef WLMCNX
		/* clear the wake bit */
		st = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_ST(bss));
		st &= ~M_P2P_BSS_ST_WAKE;
		wlc_mcnx_write_shm(wlc->mcnx, M_P2P_BSS_ST(bss), st);
#endif /* WLMCNX */
	}

	/* flush unsent atims */
	if (TXPKTPENDGET(wlc, TX_ATIM_FIFO)) {
		WL_ERROR(("%s ATIM failed, flush ATIM fifo [atim pending %d]\r\n",
			__FUNCTION__, TXPKTPENDGET(wlc, TX_ATIM_FIFO)));
		wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_ATIM_FIFO), FLUSHFIFO);
	}
	/* clear bcmc pending for this window */
	cfg_cubby->bcmc_pend = FALSE;
}

void
_wlc_aibss_send_atim_q(wlc_info_t *wlc, struct pktq *q)
{
	void *p;
	int prec = 0;
	struct scb *scb;

	/* XXX do we need this ?
	if (wlc->in_send_q) {
		WL_INFORM(("wl%d: in_send_q, qi=%p\n", wlc->pub->unit, qi));
		return;
	}

	wlc->in_send_q = TRUE;
	*/

	/* Send all the enq'd pkts that we can.
	 * Dequeue packets with precedence with empty HW fifo only
	 */
	 while ((p = pktq_deq(q, &prec))) {
		wlc_txh_info_t txh_info;
		wlc_pkttag_t *pkttag = WLPKTTAG(p);

		scb = WLPKTTAGSCBGET(p);
		if ((pkttag->flags & WLF_TXHDR) == 0) {
			wlc_mgmt_ctl_d11hdrs(wlc, p, scb, TX_ATIM_FIFO, 0);
		}

		wlc_get_txh_info(wlc, p, &txh_info);

		if (!(uint)TXAVAIL(wlc, TX_ATIM_FIFO)) {
			/* Mark precedences related to this FIFO, unsendable */
			WLC_TX_FIFO_CLEAR(wlc, TX_ATIM_FIFO);
			break;
		}

		wlc_txfifo(wlc, TX_ATIM_FIFO, p, &txh_info, TRUE, 1);
	}
	/* wlc->in_send_q = FALSE; */
}

void
_wlc_aibss_set_active_scb(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *cfg)
{
	int prec = 0;
	struct pktq *txq = &cfg->wlcif->qi->q;
	void *head_pkt, *pkt;
	struct scb *scb;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);

	PKTQ_PREC_ITER(txq, prec) {
		head_pkt = NULL;
		while (pktq_ppeek(txq, prec) != head_pkt) {
			aibss_scb_info_t	*scb_info;
			pkt = pktq_pdeq(txq, prec);
			if (!head_pkt) {
				head_pkt = pkt;
			}

			scb = WLPKTTAGSCBGET(pkt);
			scb_info = SCB_CUBBY((scb), aibss_info->scb_handle);
			scb_info->pkt_pend = TRUE;
			pktq_penq(txq, prec, pkt);
		}
	}

	/* Check the FIFOs for pending packets */
	if (TXPKTPENDTOT(aibss->wlc)) {
		_wlc_aibss_set_active_scb_fifo(aibss_info, cfg);
	}
}

void
wlc_aibss_ps_process_atim(wlc_info_t *wlc, struct ether_addr *ea)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
	struct scb *scb = wlc_scbfind(wlc, aibss->bsscfg, ea);
	aibss_scb_info_t *scb_info;
	char buf[ETHER_ADDR_STR_LEN];

	BCM_REFERENCE(buf);

	if (scb == NULL) {
		WL_ERROR(("wl%d.%d: ATIM received from unknown peer %s\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(aibss->bsscfg), bcm_ether_ntoa(ea, buf)));
		return;
	}

	WL_PS(("wl%d.%d: ATIM received\n", wlc->pub->unit, WLC_BSSCFG_IDX(aibss->bsscfg)));

	scb_info = SCB_CUBBY((scb), wlc->aibss_info->scb_handle);
	scb_info->atim_rcvd = TRUE;

	if (SCB_PS(scb)) {
		wlc_apps_process_ps_switch(wlc, &scb->ea, FALSE);
	}
	wlc_set_ps_ctrl(aibss->bsscfg);
}

bool
_wlc_aibss_sendatim(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	void *p;
	uint8 *pbody;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
#ifdef BCMDBG
		char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	WL_PS(("%s: Entering\n", __FUNCTION__));

	ASSERT(wlc->pub->up);

	/* HANDLE BCMC ATIM too */
	if (pktq_full(&aibss->atimq)) {
		WL_ERROR(("wl%d.%d: ATIM queue overflow \n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg)));
		return FALSE;
	}

	if ((p = wlc_frame_get_mgmt(wlc, FC_ATIM, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->current_bss->BSSID, 0, &pbody)) == NULL) {
		WL_ERROR(("wl%d.%d: Unable to get pkt for ATIM frame\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg)));
		return FALSE;
	}

	WLPKTTAG(p)->flags |= WLF_PSDONTQ;
	WLF2_PCB2_REG(p, WLF2_PCB2_AIBSS_CTRL);
	WLPKTTAG(p)->shared.packetid = 0;

	WLPKTTAGSCBSET(p, scb);
	WLPKTTAGBSSCFGSET(p, WLC_BSSCFG_IDX(bsscfg));
	PKTSETLEN(wlc->osh, p, DOT11_MGMT_HDR_LEN);

	pktq_penq(&aibss->atimq, 0, p);
	_wlc_aibss_send_atim_q(wlc, &aibss->atimq);
	return TRUE;
}

bool
_wlc_aibss_check_pending_data(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool force_all)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		aibss_scb_info_t *scb_info = SCB_CUBBY((scb), wlc->aibss_info->scb_handle);
		uint psq_len = wlc_apps_psq_len(wlc, scb);

		/* Send ATIM for the peer which are already in PS */
		/* JIRA:SWWLAN-68817 RB:47008
		 * Packet of a long interval,such as VoIP,
		 * will have opportunities to enter the PS mode.
		 * When put PS mode, we can't send packet until next tbtt.
		 * And it will get low quality VoIP.
		 * We check high priority(VO,VI) packet's last time.
		 * If less than 70ms will not go into PS.
		 */
		if ((SCB_PS(scb) && psq_len) || scb_info->pkt_pend ||
			scb_info->pkt_pend_count ||
			((OSL_SYSUPTIME() - scb_info->last_hi_prio_pkt_time) < 70)) {
			scb_info->pkt_pend = FALSE;

			if (scb_info->ps_enabled) {
				_wlc_aibss_sendatim(wlc, bsscfg, scb);
			}
		}
		else if (!SCB_PS(scb) && scb_info->ps_enabled) {
			/* Enable PS right away */
			wlc_apps_process_ps_switch(wlc, &scb->ea, TRUE);
		}
	}

	return TRUE;
}

bool
wlc_aibss_pm_allowed(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *bsscfg)
{
	aibss_cfg_info_t	*cfg_cubby;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);

	if (!BSSCFG_IBSS(bsscfg))
		return TRUE;

	cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);
	return cfg_cubby->pm_allowed;
}

/* return the required pretbtt value */
static void
wlc_aibss_pretbtt_query_cb(void *ctx, bss_pretbtt_query_data_t *notif_data)
{
	wlc_bsscfg_t *cfg;

	ASSERT(notif_data != NULL);

	cfg = notif_data->cfg;
	ASSERT(cfg != NULL);

	if (!BSSCFG_IS_AIBSS_PS_ENAB(cfg)) {
		return;
	}

	/* XXX increase the pre-tbtt by 4 ms inorder for radio to be ready to xmit bcn
	 * after wake-up from sleep, this need to be optimized
	 */
	notif_data->pretbtt += 4000;
}

void wlc_aibss_send_txfail_event(wlc_aibss_info_t *aibss_info, struct scb *scb, uint32 evt_sybtype)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	aibss_scb_info_t *scb_info = SCB_CUBBY(scb, aibss_info->scb_handle);

	// send only limited number of events, then trigger scb free
	if (scb_info->evt_sent_cnt) {
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_AIBSS_TXFAIL, &scb->ea,
			WLC_E_STATUS_FAIL, evt_sybtype, 0, NULL, 0);
		if (--scb_info->evt_sent_cnt == 0) {
			// try free scb, if no timer will do it later
			scb_info->free_scb = TRUE;
			//wlc_aibss_scbfree(aibss_info, scb);
		}
	}
}
static void
_wlc_aibss_data_pkt_txstatus(wlc_info_t *wlc, void *pkt, uint txstatus)
{
	struct scb *scb;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	aibss_scb_info_t *scb_info;
	aibss_cfg_info_t *cfg_cubby;

	scb = WLPKTTAGSCBGET(pkt);
	if (scb == NULL)
		return;
	bsscfg = SCB_BSSCFG(scb);

	scb_info = SCB_CUBBY((scb), wlc->aibss_info->scb_handle);
	if (bsscfg == NULL || bsscfg != aibss->bsscfg || SCB_ISMULTI(scb))
		return;

	cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);

	scb_info->pkt_pend_count--;

	/* Ignore txstatus clbk due to PKTFREE */
	if (txstatus == 0)
		return;

	if (txstatus & TX_STATUS_ACK_RCV) {
		scb_info->tx_noack_count = 0;
		/* scb is back or it'sbeen Ok,
		 * cancel scb free(if was ordered) & reinit txfail limiter
		 */
		scb_info->evt_sent_cnt = TXFEVT_CNT;
		scb_info->free_scb = FALSE;
	}
	else {
			scb_info->tx_noack_count++;

			if (cfg_cubby->max_tx_retry &&
				(scb_info->tx_noack_count >= cfg_cubby->max_tx_retry)) {
				wlc_aibss_send_txfail_event(wlc->aibss_info, scb, AIBSS_TX_FAILURE);
			}
	} /* NOACK */
}

void
_wlc_aibss_set_active_scb_fifo(wlc_aibss_info_t * aibss_info, wlc_bsscfg_t * cfg)
{
	void **phdr_list = NULL;
	hnddma_t* di = NULL;
	int i, j;
	int size;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	int fifo_list[] = { TX_AC_BE_FIFO, TX_AC_BK_FIFO, TX_AC_VI_FIFO, TX_AC_VO_FIFO };
	for (j = 0; j < (sizeof(fifo_list)/sizeof(int)); j++)
	{
		int fifo = fifo_list[j];
		int n = TXPKTPENDGET(wlc, fifo);
		if (n == 0)
			continue;

		size = (n * sizeof(void*));
		phdr_list = MALLOCZ(wlc->osh, size);
		if (phdr_list == NULL) continue;

		di = WLC_HW_DI(wlc, fifo);
		if (di && (dma_peekntxp(di, &n, phdr_list, HNDDMA_RANGE_ALL) == BCME_OK)) {
			for (i = 0; i < n; i++) {
				void *p;
				p = phdr_list[i];
				ASSERT(p);
				if (p != NULL) {
					struct scb *scb = WLPKTTAGSCBGET(p);
					if (scb) {
						aibss_scb_info_t *scb_info =
							SCB_CUBBY((scb),
							wlc->aibss_info->scb_handle);
						/* Set pkt pending */
						scb_info->pkt_pend = TRUE;
					}
				}
			}
		}
		MFREE(wlc->osh, (void*) phdr_list, size);
	}
}

void
wlc_aibss_stay_awake(wlc_aibss_info_t * aibss_info)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);

	if (aibss->bsscfg == NULL)
		return;
	/* SR WAR: uCode requests ALP clock if there are data in the FIFO
	 * so Wake-up uCode and put it back to sleep to enable ALP clock
	 */
	aibss->wlc->wake = TRUE;
	wlc_set_ps_ctrl(aibss->bsscfg);
	aibss->wlc->wake = FALSE;
	wlc_set_ps_ctrl(aibss->bsscfg);
}

void
wlc_aibss_tx_pkt(wlc_aibss_info_t * aibss_info, struct scb *scb, void *pkt)
{
	aibss_scb_info_t *scb_info;
	void *head = NULL, *tail = NULL;
	void *n = NULL;

	BCM_REFERENCE(head);
	BCM_REFERENCE(tail);

	if (scb != NULL && pkt != NULL && !SCB_ISMULTI(scb)) {
		/* Enable packet callback */
		WLF2_PCB1_REG(pkt, WLF2_PCB1_AIBSS_DATA);
		scb_info = SCB_CUBBY((scb), aibss_info->scb_handle);

		/* pkt could be chained, update the correct packet count */
		FOREACH_CHAINED_PKT(pkt, n) {
			scb_info->pkt_pend_count++;
			if (IS_HI_PRIO_PKT(pkt)) {
				scb_info->last_hi_prio_pkt_time = OSL_SYSUPTIME();
			}
			PKTCENQTAIL(head, tail, pkt);
		}
	}
}

static int
wlc_aibss_scb_init(void *context, struct scb *scb)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)context;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss_cfg_info_t *cfg_cubby = BSSCFG_CUBBY(aibss->bsscfg, aibss->cfg_cubby_handle);
	aibss_scb_info_t *scb_info = SCB_CUBBY(scb, aibss_info->scb_handle);

	if (scb->bsscfg == NULL || !BSSCFG_IBSS(scb->bsscfg) ||
		scb->bsscfg != aibss->bsscfg ||
		!AIBSS_BSS_PS_ENAB(aibss->bsscfg)) {
		/* Do nothing */
		return BCME_OK;
	}

	/* Enable PS by default */
	scb_info->ps_enabled = TRUE;

	/* Force wake the device, host might start
	 * sending the packets to the peer immediately
	 */
	cfg_cubby->pm_allowed = FALSE;
	wlc_aibss_stay_awake(aibss_info);

	return BCME_OK;
}

static void
wlc_aibss_scb_deinit(void *context, struct scb *scb)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)context;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss_cfg_info_t *cfg_cubby = BSSCFG_CUBBY(aibss->bsscfg, aibss->cfg_cubby_handle);
	aibss_scb_info_t *scb_info;
	struct scb_iter scbiter;
	struct scb *scbi;

	if (!BSSCFG_IBSS(scb->bsscfg) || scb->bsscfg != aibss->bsscfg) {
		return;
	}

	if (AIBSS_BSS_PS_ENAB(aibss->bsscfg) &&
		(cfg_cubby->force_wake & WLC_AIBSS_FORCE_WAKE_NON_PS_PEER)) {
		FOREACH_BSS_SCB(aibss->wlc->scbstate, &scbiter, aibss->bsscfg, scbi) {
			if (scbi == scb) {
				continue;
			}
			scb_info = SCB_CUBBY((scbi), aibss_info->scb_handle);
			if (!scb_info->ps_enabled) {
				return;
			}
		}
		cfg_cubby->force_wake &= ~WLC_AIBSS_FORCE_WAKE_NON_PS_PEER;
	}
}

static int
wlc_aibss_bcn_parse_ibss_param_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)ctx;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	aibss_scb_info_t *scb_info;
	aibss_cfg_info_t *cfg_cubby;

	/* XXX if we create peer SCB only after recving beacons then we don't need
	 * to check the "atim_window" in every beacon.
	 */
	if (!BSSCFG_IBSS(cfg)) {
		return BCME_OK;
	}

	/* IBSS parameters */
	if (data->ie != NULL &&
	    data->ie[TLV_LEN_OFF] >= DOT11_MNG_IBSS_PARAM_LEN) {
	    struct scb *scb  = ftpparm->bcn.scb;
		uint16 atim_window = ltoh16_ua(&data->ie[TLV_BODY_OFF]);

		if (AIBSS_BSS_PS_ENAB(cfg) && (atim_window == 0)) {
			cfg_cubby = BSSCFG_CUBBY(aibss->bsscfg, aibss->cfg_cubby_handle);
			scb_info = SCB_CUBBY((scb), aibss_info->scb_handle);

			scb_info->ps_enabled = FALSE;
			/* Disable AIBSS PS */
			cfg_cubby->force_wake |= WLC_AIBSS_FORCE_WAKE_NON_PS_PEER;
		}
	}
	return BCME_OK;
}

void
wlc_aibss_set_wake_override(wlc_aibss_info_t *aibss_info, uint32 wake_reason, bool set)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss_cfg_info_t *cfg_cubby;

	if (aibss->bsscfg == NULL) {
		return;
	}

	cfg_cubby = BSSCFG_CUBBY(aibss->bsscfg, aibss->cfg_cubby_handle);

	if (set) {
		cfg_cubby->force_wake |= wake_reason;
	}
	else {
		cfg_cubby->force_wake &= ~wake_reason;
	}
}

void
wlc_aibss_new_peer(wlc_aibss_info_t *aibss_info, struct scb *scb)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wsec_key_t *key = aibss->bsscfg->bss_def_keys[0];
	aibss_scb_info_t *scb_info;

	/* point scb's pairwise & group keys to bsscfg's default key */
	/* that is plumbed into h/w and bsscfg by "wsec_key" */

	AIBSS_INFO(("%s, scb:%p\n", __FUNCTION__, scb));
	ASSERT(scb);
	scb_info = SCB_CUBBY(scb, aibss_info->scb_handle);

	/* init txfail event limit cnt */
	scb_info->evt_sent_cnt = TXFEVT_CNT;

	scb->key = key; /* pairwise key */
#if defined(IBSS_PEER_GROUP_KEY)
	scb->ibss_grp_keys[0] = key; /* group key */
#endif // endif
	WL_WSEC(("%s scb:%p, cfg key ID:%d idx:%d, algo:%d,len:%d\n",
		__FUNCTION__, scb, key->id, key->idx, key->algo, key->len));

	if (AIBSS_PS_ENAB(aibss->pub)) {
		/* put the device in PS State, tx is allowed only after
		 * ATIM window
		 */
		wlc_apps_process_ps_switch(aibss->wlc, &scb->ea, TRUE);
	}
}

static void
wlc_aibss_scbfree(wlc_aibss_info_t *aibss_info, struct scb *scb)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss_scb_info_t *scb_info = SCB_CUBBY(scb, aibss_info->scb_handle);

	AIBSS_INFO(("%s, scb:%p\n", __FUNCTION__, scb));

	if (scb_info->pkt_pend_count &&
		scb_info->pkt_pend_count > wlc_apps_psq_len(aibss->wlc, scb)) {
		scb_info->free_scb = TRUE;
	}
	else {
		wlc_scbfree(aibss->wlc, scb);
	}
}

/*
* cb fn from scan, notifies aibss radio is back to home channel
*/
void
wlc_aibss_back2homechan_notify(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;

	AIBSS_INFO(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	/* for aibss we need to keep mactrl AP bit set, scan clears it */
	wlc_bmac_mctrl(wlc_hw, MCTL_AP, MCTL_AP);
}

/* aibss module BH handler  */
#define WL_WAKE_OVERRIDE_ALL  \
	(WLC_WAKE_OVERRIDE_PHYREG | WLC_WAKE_OVERRIDE_MACSUSPEND | \
	WLC_WAKE_OVERRIDE_CLKCTL | WLC_WAKE_OVERRIDE_TXFIFO | \
	WLC_WAKE_OVERRIDE_FORCEFAST | WLC_WAKE_OVERRIDE_4335_PMWAR)
static int
wlc_aibss_wl_up(void *ctx)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)ctx;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	aibss_cfg_info_t *cfg_cubby;
	wlc_info_t *wlc = aibss->wlc;
	uint32 old_l, old_h;
	uint bcnint, bcnint_us;

	uint32 cfg_atim = wlc->cfg->current_bss->atim_window;
	uint32 d11hw_atim = R_REG(wlc->osh, &wlc->regs->tsf_cfpmaxdur32);
	uint32 mc = R_REG(wlc->osh, &wlc->regs->maccontrol);
	BCM_REFERENCE(mc);
	uint32 val = 0;
	uint32 mask = (MCTL_INFRA | MCTL_DISCARD_PMQ);
#ifdef WLMCNX
	int bss;
#endif /* WLMCNX */

	if (!aibss_info) {
	/* don't do wl down + wl reinit !, we dont support it */
		WL_ERROR(("%s aibss_info is NULL\n", __FUNCTION__));
		return BCME_NOMEM;
	}

	wlc->cfg->current_bss->atim_window = wlc->default_bss->atim_window;
	cfg_atim = wlc->cfg->current_bss->atim_window;

	AIBSS_INFO(("%s ai_inf:%p, ai_priv:%p\n",
		__FUNCTION__, aibss_info, aibss));

	if (cfg_atim == d11hw_atim || (!bsscfg))
		return BCME_OK; /* do nothing on wlc load */

	WL_ERROR(("%s aibss BH occured, cur_bss_atim:%d, d11hw_atim:%d mactrl:%x\n",
		__FUNCTION__, cfg_atim, d11hw_atim, mc));

	AIBSS_INFO(("MCTL: WAKE:%d,HPS:%d,AP:%d,INFRA:%d,PSM_RUN:%d,DISCARD_PMQ:%d\n",
	((MCTL_WAKE & mc) != 0), ((MCTL_HPS & mc) != 0),
	((MCTL_AP & mc) != 0), ((MCTL_INFRA & mc) != 0),
	((MCTL_PSM_RUN & mc) != 0), ((MCTL_DISCARD_PMQ & mc) != 0)));

	/* in case of BH hw atim is cleared, need to set it again */
	W_REG(wlc->osh, &wlc->regs->tsf_cfpmaxdur32, cfg_atim);

	/* on BH it me be set, keep it cleared */
	wlc->check_for_unaligned_tbtt = 0;

	/* DisabLe PMQ entries to avoid uCode updating pmq entry on  rx packets */
	val |= MCTL_DISCARD_PMQ;

	/* Update PMQ Interrupt threshold to avoid MI_PMQ interrupt */
	wlc_write_shm(wlc, SHM_PMQ_INTR_THRESH, AIBSS_PMQ_INT_THRESH);

	wlc_ap_ctrl(wlc, TRUE, bsscfg, -1);
	wlc_mctrl(wlc, mask, val);

#ifdef WLMCNX
	/* Set IBSS and GO bit to enable beaconning */
	bss = wlc_mcnx_BSS_idx(wlc->mcnx, bsscfg);
	wlc_mcnx_mac_suspend(wlc->mcnx);
	wlc_mcnx_write_shm(wlc->mcnx, M_P2P_BSS_ST(bss),
		(M_P2P_BSS_ST_GO | M_P2P_BSS_ST_IBSS));
	wlc_mcnx_mac_resume(wlc->mcnx);
#endif /* WLMCNX */
	wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_AC_BE_FIFO), FLUSHFIFO);
	wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_AC_BK_FIFO), FLUSHFIFO);
	wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_AC_VI_FIFO), FLUSHFIFO);
	wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_AC_VO_FIFO), FLUSHFIFO);
	wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_ATIM_FIFO), FLUSHFIFO);
	wlc_bmac_tx_fifo_sync(wlc->hw, TX_FIFO_BITMAP(TX_BCMC_FIFO), FLUSHFIFO);

	/* just in case clear all wlc_hw MCTL_AWAKE override bits */
	wlc_ucode_wake_override_clear(wlc->hw, WL_WAKE_OVERRIDE_ALL);

	cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);
	cfg_cubby->force_wake |= WLC_AIBSS_FORCE_WAKE_INITIAL;
	aibss->wakeup_end_time =
		OSL_SYSUPTIME() + WLC_AIBSS_INITIAL_WAKEUP_PERIOD*500;

	/* reset tsf, get the atim_end interrupts rolling */
	bcnint = wlc->cfg->current_bss->beacon_period;
	bcnint_us = ((uint32)bcnint << 10);

	wlc_read_tsf(wlc, &old_l, &old_h);
	wlc_tsf_adj(wlc, bsscfg, 0, 0, old_h, old_l, bcnint_us, bcnint_us, FALSE);

	/* fixes the issue with phy errors after big hammer */
	wlc_stf_reinit_chains(wlc);

	return BCME_OK;
}
