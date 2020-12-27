/**
 * Advanced IBSS implementation for Broadcom 802.11 Networking Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_aibss.c 462810 2014-03-18 23:24:40Z $
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
#endif
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif
#include <wlc_utils.h>
#include <wlc_scb.h>
#include <wlc_pcb.h>
#include <wlc_tbtt.h>
#include <wlc_apps.h>
#include <wlc_aibss.h>

/* iovar table */
enum {
	IOV_AIBSS,			/* enable/disable AIBSS feature */
	IOV_AIBSS_BCN_FORCE_CONFIG,	/* bcn xmit configuration */
	IOV_AIBSS_TXFAIL_CONFIG,
	IOV_AIBSS_IFADD,		/* add AIBSS interface */
	IOV_AIBSS_IFDEL,		/* delete AIBSS interface */
	};

enum {
	WLC_AIBSS_CONV_START_REQ = 1,
	WLC_AIBSS_CONV_START_ACK = 2,
	WLC_AIBSS_CONV_END_REQ = 3,
	};

static const bcm_iovar_t aibss_iovars[] = {
	{"aibss", IOV_AIBSS, (IOVF_BSS_SET_DOWN), IOVT_BOOL, 0},
	{"aibss_bcn_force_config", IOV_AIBSS_BCN_FORCE_CONFIG,
	(IOVF_BSS_SET_DOWN), IOVT_BUFFER, sizeof(aibss_bcn_force_config_t)},
	{"aibss_txfail_config", IOV_AIBSS_TXFAIL_CONFIG,
	(0), IOVT_BUFFER, (sizeof(aibss_txfail_config_t))},
	{"aibss_ifadd", IOV_AIBSS_IFADD, 0, IOVT_BUFFER, sizeof(wl_aibss_if_t)},
	{"aibss_ifdel", IOV_AIBSS_IFDEL, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
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
	bool use_act_frame;		/* if action frame is used for conversation handshake */
	bool ctw_enabled;
	struct wl_timer *idle_timer;	/* scb no traffic time out timer */
	uint32 idle_timeout;		/* Idle timeout for SCBs */
} wlc_aibss_info_priv_t;

typedef struct  {
	wlc_aibss_info_t aibss_pub;
	wlc_aibss_info_priv_t aibss_priv;
} wlc_aibss_mem_t;

/* Vendor specific IE header */
typedef struct aibss_vs_ie_hdr
{
	uint8 OUI[3];
	uint8 sub_type;
} aibss_vs_ie_hdr_t;

/* Vendor specific action frame header */
typedef struct aibss_vs_af_hdr
{
	uint8 category;
	uint8 OUI[3];
	uint8 tag;
	uint8 tag_len;
} aibss_vs_af_hdr_t;

typedef struct aibss_conv_msg_hdr
{
	aibss_vs_af_hdr_t af_hdr;
	aibss_vs_ie_hdr_t ie_hdr;
	uint8 msg_type;
	uint8 msg_len;
} aibss_conv_msg_hdr_t;

#define IE_VENDOR_SPECIFIC 221			/* DOT11_MNG_PROPR_ID */
#define AIBSS_DEFAULT_IDLE_TIMEOUT 500		/* Default timeout in ms */

#define WLC_AIBSS_INFO_SIZE (sizeof(wlc_aibss_mem_t))

static uint16 wlc_aibss_info_priv_offset = OFFSETOF(wlc_aibss_mem_t, aibss_priv);

/* module specific states location */
#define WLC_AIBSS_INFO_PRIV(aibss_info) \
	((wlc_aibss_info_priv_t *)((uint8*)(aibss_info) + wlc_aibss_info_priv_offset))

static int wlc_aibss_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static void wlc_aibss_timer(void *arg);
#ifdef WLMCNX
static void wlc_aibss_tbtt_intr_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data);
#endif
static void wlc_aibss_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt);
static bool wlc_aibss_validate_chanspec(wlc_info_t *wlc, chanspec_t chanspec);

static void _wlc_aibss_set_ctw(wlc_info_t *wlc, int ctw);
static void _wlc_aibss_idle_timer_cb(void *arg);
static void _wlc_aibss_tbtt_impl_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data);
static void _wlc_aibss_tbtt_impl(wlc_aibss_info_t *aibss);
static void _wlc_aibss_pretbtt_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data);
static void _wlc_aibss_ctrl_pkt_txstatus(wlc_info_t *wlc, void *pkt, uint txstatus);
static void _wlc_aibss_reset_pend_status(wlc_aibss_info_t *aibss_info, struct scb *scb);
static bool _wlc_aibss_check_pending_data(wlc_info_t *wlc, bool force_all);
static void _wlc_aibss_send_conv_end_req(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb);
static void _wlc_aibss_send_conv_end_req_null(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct scb *scb);
static void _wlc_aibss_init_conv_msg(wlc_info_t *wlc, aibss_conv_msg_hdr_t *msg_hdr, int msg_type,
	int msg_body_len);
static bool _wlc_aibss_validate_conv_msg(wlc_info_t *wlc, aibss_conv_msg_hdr_t *msg_hdr);
static bool _wlc_aibss_prepare_off(wlc_info_t *wlc, wlc_aibss_info_priv_t *aibss,
	wlc_bsscfg_t *bsscfg, bool force_all);
static void _wlc_aibss_set_hps(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool ctw_suppr);
static void _wlc_aibss_ack_conv_start_req(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb);
static void _wlc_aibss_ack_conv_start_req_null(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct scb *scb);
static void _wlc_aibss_scb_off(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb);
static void _wlc_aibss_bsscfg_off(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

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

	BCM_REFERENCE(_wlc_aibss_set_ctw);
	/* sanity checks */
	if (!(aibss_info = (wlc_aibss_info_t *)MALLOC(wlc->osh, WLC_AIBSS_INFO_SIZE))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)aibss_info, WLC_AIBSS_INFO_SIZE);
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

	if ((aibss->idle_timer =
	    wl_init_timer(wlc->wl, _wlc_aibss_idle_timer_cb, aibss_info, "idle_timer")) == NULL) {
		WL_ERROR(("wl%d: add idle_timer failed\n", wlc->pub->unit));
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
		sizeof(aibss_scb_info_t), NULL, NULL, NULL, NULL)) < 0) {
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

#ifdef WLMCNX
	/* register preTBTT callback */
	if (wlc_mcnx_intr_register(wlc->mcnx, wlc_aibss_tbtt_intr_cb, aibss_info) != BCME_OK) {
		WL_ERROR(("wl%d:%s: wlc_mcnx_intr_register failed \n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif

	/* register packet callback function */
	if (wlc_pcb_fn_set(wlc->pcb, 1, WLF2_PCB2_AIBSS_CTRL, _wlc_aibss_ctrl_pkt_txstatus)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_pcb_fn_set() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, aibss_iovars, "aibss",
		aibss_info, wlc_aibss_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: AIBSS wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

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

	if (aibss->idle_timer != NULL) {
		wl_free_timer(wlc->wl, aibss->idle_timer);
		aibss->idle_timer = NULL;
	}

#ifdef WLMCNX
	wlc_mcnx_intr_unregister(wlc->mcnx, wlc_aibss_tbtt_intr_cb, aibss_info);
#endif

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
			store16_ua((uint8 *)&txfail_config->version, AIBSS_TXFAIL_CONFIG_VER_0);
			store16_ua((uint8 *)&txfail_config->len, sizeof(aibss_txfail_config_t));
			store32_ua((uint8 *)&txfail_config->bcn_timeout, cfg_cubby->bcn_timeout);
			store32_ua((uint8 *)&txfail_config->max_tx_retry, cfg_cubby->max_tx_retry);
			break;
		}

		case IOV_SVAL(IOV_AIBSS_TXFAIL_CONFIG): {
			aibss_txfail_config_t *txfail_config = (aibss_txfail_config_t *)a;
			aibss_cfg_info_t	*cfg_cubby;

			if (!BSSCFG_IBSS(bsscfg)) {
				err = BCME_ERROR;
				break;
			}

			if ((load16_ua(&txfail_config->version)) != AIBSS_TXFAIL_CONFIG_VER_0) {
				err = BCME_VERSION;
				break;
			}

			cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);
			cfg_cubby->bcn_timeout = load32_ua((uint8 *)&txfail_config->bcn_timeout);
			cfg_cubby->max_tx_retry = load32_ua((uint8 *)&txfail_config->max_tx_retry);
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

			if (wlc_tbtt_ent_fn_add(wlc->tbtt, new_bsscfg, _wlc_aibss_pretbtt_cb,
				_wlc_aibss_tbtt_impl_cb, wlc->aibss_info) != BCME_OK) {
				WL_ERROR(("wl%d: %s: wlc_tbtt_ent_fn_add() failed\n",
				  wlc->pub->unit, __FUNCTION__));
				return BCME_NORESOURCE;
			}

#ifdef WLMCHAN
			/* save the chanspec for mchan */
			if (MCHAN_ENAB(wlc->pub))
				new_bsscfg->chanspec = aibss_if->chspec;
#endif
			new_bsscfg->flags2 |= WLC_BSSCFG_FL2_AIBSS;
			aibss->bsscfg = new_bsscfg;
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
		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

#ifdef WLMCNX
static void
wlc_aibss_tbtt_intr_cb(void *ctx, wlc_mcnx_intr_data_t *notif_data)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)ctx;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t	*wlc = aibss->wlc;
	wlc_bsscfg_t *cfg =  notif_data->cfg;
	uint16 mhf_val;

	if (!AIBSS_ENAB(wlc->pub) || !BSSCFG_IBSS(cfg)) {
		return;
	}

	mhf_val =  wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_AUTO);

	if (mhf_val & MHF1_FORCE_SEND_BCN) {
		wl_cnt_t *cnt = wlc->pub->_cnt;

		wlc_statsupd(wlc);

		/* check if we have sent atleast 1 bcn and clear
		 * the force bcn bit
		 */
		if (cnt->txbcnfrm != aibss->last_txbcnfrm) {
			wlc_bmac_mhf(wlc->hw, MHF1, MHF1_FORCE_SEND_BCN, 0,
				WLC_BAND_ALL);
		}
	}
}
#endif /* WLMCNX */

/* bsscfg up/down */
static void
wlc_aibss_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)ctx;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *cfg;
	uint32 tsf_h, tsf_l;

	ASSERT(evt != NULL);

	cfg = evt->bsscfg;
	ASSERT(cfg != NULL);

	if (!AIBSS_ENAB(wlc->pub) || !BSSCFG_IBSS(cfg))
		return;

	if (evt->up) {
		/* if configured, start timer to check for bcn xmit */
		if (aibss->initial_min_bcn_dur && aibss->bcn_flood_dur &&
			aibss->min_bcn_dur) {
			uint32 val = 0;
			uint32 mask = (MCTL_INFRA | MCTL_DISCARD_PMQ);

			/* Disable PMQ entries for now as ucode doesn't handle PMQ for IBSS properly
			 * AIBSS needs both INFRA and AP bit to be set
			 */
			if (BSSCFG_IS_AIBSS(cfg)) {
				val |= MCTL_DISCARD_PMQ;
				val |= MCTL_INFRA;
				wlc_ap_ctrl(wlc, TRUE, cfg, -1);
				wlc_mctrl(wlc, mask, val);
			}
			aibss->idle_timeout = AIBSS_DEFAULT_IDLE_TIMEOUT;

			/* IBSS interface up, start the timer to monitor the
			 * beacon transmission
			 */
			aibss->ibss_start_time = OSL_SYSUPTIME();
			wl_add_timer(wlc->wl, aibss->ibss_timer, aibss->initial_min_bcn_dur,
				FALSE);

			wlc_read_tsf(wlc, &tsf_l, &tsf_h);
#ifdef WLMCNX
			/* Set the tsf to get callback every bcn period */
			wlc_mcnx_tbtt_set(wlc->mcnx, cfg, tsf_h, tsf_l, tsf_h, tsf_h);
#endif
		}
	}
	else {
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

	WL_TRACE(("wl%d: wlc_ibss_timer", wlc->pub->unit));

	if (!wlc->pub->up || !AIBSS_ENAB(wlc->pub)) {
		return;
	}

	if (aibss->ibss_start_time &&
		((OSL_SYSUPTIME() - aibss->ibss_start_time) < aibss->bcn_flood_dur)) {
		timer_dur = aibss->initial_min_bcn_dur;
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

	if  ((cfg_cubby->max_tx_retry &&
		(scb_info->tx_noack_count >= cfg_cubby->max_tx_retry)) ||
		(cfg_cubby->bcn_timeout &&
		(scb_info->no_bcn_counter >= cfg_cubby->bcn_timeout))) {

			if (!WIN7_AND_UP_OS(aibss->pub)) {
				wlc_bss_mac_event(wlc, cfg, WLC_E_AIBSS_TXFAIL, &scb->ea,
				                  WLC_E_STATUS_FAIL, 0, 0, NULL, 0);
			}

			/* Reset the counters */
			scb_info->no_bcn_counter = 0;
			scb_info->tx_noack_count = 0;
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

void wlc_aibss_notify_ps_pkt(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *bsscfg,
	struct scb *scb, void *p)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss_cfg_info_t *cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);
	aibss_scb_info_t *scb_info = SCB_CUBBY((scb), aibss_info->scb_handle);

	if (!BSSCFG_IS_AIBSS(bsscfg))
		return;

	if (SCB_ISMULTI(scb)) {
		cfg_cubby->pend_multicast++;
	} else {
		scb_info->pend_unicast++;
	}
}

void wlc_aibss_update_bi(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *bsscfg, int bi)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);

	if (bsscfg == NULL || bsscfg != aibss->bsscfg)
		return;

	bi = bi >> 10;
	/* TODO: adjust idle timeout based on beacon interval */
	aibss->idle_timeout = 50; /* millisecond leve */
}

void wlc_aibss_recv_convmsg(wlc_aibss_info_t *aibss_info, struct dot11_management_header *hdr,
	uint8 *body, int body_len, wlc_d11rxhdr_t *wrxh, uint32 rspec)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	struct scb *scb;
	aibss_conv_msg_hdr_t *msg_hdr = (aibss_conv_msg_hdr_t*)body;

	if (bsscfg == NULL || bsscfg != aibss->bsscfg)
		return;

	if (bcmp(&bsscfg->BSSID, &hdr->bssid, ETHER_ADDR_LEN) != 0)
		return;

	if (!_wlc_aibss_validate_conv_msg(wlc, msg_hdr)) {
		WL_ERROR(("invalid conversation message\n"));
		return;
	}

	scb = wlc_scbfind(wlc, bsscfg, &hdr->sa);
	if (scb == NULL) {
		WL_ERROR(("No SCB found for conv start sender\n"));
		return;
	}


	if (msg_hdr->msg_type == WLC_AIBSS_CONV_START_REQ) {
		struct ether_addr *request_mac_addr = (struct ether_addr *)(msg_hdr + 1);
		int scb_num = msg_hdr->msg_len / ETHER_ADDR_LEN;

		WL_ERROR(("recv conv_start_req, peers:"));
		while (scb_num-- > 0) {
			prhex(NULL, (uchar*)request_mac_addr, ETHER_ADDR_LEN);
			if (bcmp(&bsscfg->cur_etheraddr, request_mac_addr, ETHER_ADDR_LEN) == 0) {
				/* When a conversation start request is received, we should
				 * . ACK the request
				 * . Enable bsscfg so packet won't be suppressed at bsscfg level
				 * . Turn off PS on coressponding scb
				 */

				if (aibss->use_act_frame)
					_wlc_aibss_ack_conv_start_req(wlc, bsscfg, scb);
				else
					_wlc_aibss_ack_conv_start_req_null(wlc, bsscfg, scb);

				wlc_bsscfg_tx_start(bsscfg);
				_wlc_aibss_reset_pend_status(aibss_info, scb);
				wlc_apps_process_ps_switch(wlc, &hdr->sa, FALSE);
				wlc_aibss_restart_idle_timeout(wlc->aibss_info, FALSE);

			}
			request_mac_addr++;
		}
	} else if (msg_hdr->msg_type == WLC_AIBSS_CONV_START_ACK) {
		/* Receiving a conversation start request ACK, remote peer is available now */
		WL_ERROR(("recv conv_start_ack\n"));
		wlc_bsscfg_tx_start(bsscfg);
		_wlc_aibss_reset_pend_status(aibss_info, scb);
		wlc_apps_process_ps_switch(wlc, &hdr->sa, FALSE);
	} else if (msg_hdr->msg_type == WLC_AIBSS_CONV_END_REQ) {
		/* Receiving a conversation end request, remote peer will enter doze state */
		WL_ERROR(("recv conv_end_req"));
		prhex(NULL, (uchar*)&hdr->sa, ETHER_ADDR_LEN);
		_wlc_aibss_reset_pend_status(aibss_info, scb);
		wlc_apps_process_ps_switch(wlc, &hdr->sa, TRUE);
	}
}

void wlc_aibss_recv_null(wlc_aibss_info_t *aibss_info, struct scb *scb, struct wlc_frminfo *f)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;

	if (bsscfg == NULL || bsscfg != SCB_BSSCFG(scb))
		return;

	if (scb != wlc_scbfind(wlc, bsscfg, &f->h->a2)) {
		WL_ERROR(("No SCB found for conv start ACK sender\n"));
		return;
	}

	if (f->fc & FC_PM) { /* Peer enters PS mode, end conversation */
		WL_ERROR(("recv conv_end_req"));
		prhex(NULL, (uchar*)&scb->ea, ETHER_ADDR_LEN);
		_wlc_aibss_reset_pend_status(aibss_info, scb);
		wlc_apps_process_ps_switch(wlc, &scb->ea, TRUE);

	} else { /* ACK to conv start */
		WL_ERROR(("recv conv_start_ack"));
		prhex(NULL, (uchar*)&scb->ea, ETHER_ADDR_LEN);
		_wlc_aibss_reset_pend_status(aibss_info, scb);
		wlc_bsscfg_tx_start(bsscfg);
		wlc_apps_process_ps_switch(wlc, &scb->ea, FALSE);
	}
}

void wlc_aibss_tbtt(wlc_aibss_info_t *aibss_info)
{
	_wlc_aibss_tbtt_impl(aibss_info);
}

void wlc_aibss_restart_idle_timeout(wlc_aibss_info_t *aibss_info, bool short_timeout)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;

	aibss->idle_timeout = AIBSS_DEFAULT_IDLE_TIMEOUT;
	wl_del_timer(wlc->wl, aibss->idle_timer);
	wl_add_timer(wlc->wl, aibss->idle_timer, aibss->idle_timeout, FALSE);
}


bool wlc_aibss_sendpmnotif(wlc_aibss_info_t *aibss_info, wlc_bsscfg_t *bsscfg,
	ratespec_t rate_override, int prio, bool track)
{
	struct scb *scb;
	struct scb_iter scbiter;
	bool no_active_scb = TRUE;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;

	if (bsscfg == NULL || bsscfg != aibss->bsscfg)
		return FALSE;

	/* Don't send NULL packet when coming out of PS, _wlc_aibss_check_pending_data will
	 * wake up the receivers
	 */
	if (!bsscfg->pm->PMenabled)
		return FALSE;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_PS(scb)) {
			no_active_scb = FALSE;
			_wlc_aibss_send_conv_end_req_null(wlc, bsscfg, scb);
		}
	}

	/* Proceed with PM change immediately if there is no one talking to us */
	if (no_active_scb)
		return FALSE;

	return TRUE;
}

void _wlc_aibss_ctrl_pkt_txstatus(wlc_info_t *wlc, void *pkt, uint txstatus)
{
	struct scb *scb;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;

	scb = WLPKTTAGSCBGET(pkt);
	if (scb == NULL)
		return;
	bsscfg = SCB_BSSCFG(scb);
	if (bsscfg == NULL || bsscfg != aibss->bsscfg)
		return;

	if (WLPKTTAG(pkt)->shared.packetid == WLC_AIBSS_CONV_START_REQ) {
		WL_ERROR(("conv_start_req sent status 0x%x\n", txstatus));
	}
	else if (WLPKTTAG(pkt)->shared.packetid == WLC_AIBSS_CONV_END_REQ) {
		WL_ERROR(("conv_end sent status 0x%x\n", txstatus));
		_wlc_aibss_scb_off(wlc, bsscfg, scb);

	} else {
		WL_ERROR(("unknown packet %d txstatus 0x%x\n", WLPKTTAG(pkt)->shared.packetid,
			txstatus));
	}

}

void _wlc_aibss_pretbtt_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data)
{
}

void _wlc_aibss_tbtt_impl_cb(void *aibss_info, wlc_tbtt_ent_data_t *notif_data)
{
	_wlc_aibss_tbtt_impl(aibss_info);
}

void _wlc_aibss_tbtt_impl(wlc_aibss_info_t *aibss_info)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	wlc_mcnx_info_t *mcnx = wlc->mcnx;
	int bss;
	uint16 state;

	if (bsscfg == NULL)
		return;

	bss = wlc_mcnx_BSS_idx(mcnx, bsscfg);
	wlc_mcnx_hps_upd(mcnx, bsscfg, M_P2P_HPS_CTW(bss), FALSE);

	state = wlc_mcnx_read_shm(mcnx, M_P2P_BSS_ST(bss));
	state &= ~M_P2P_BSS_ST_SUPR;
	state |= M_P2P_BSS_ST_CTW;
	wlc_mcnx_mac_suspend(mcnx);
	wlc_mcnx_write_shm(mcnx, M_P2P_BSS_ST(bss), state);
	wlc_mcnx_mac_resume(mcnx);

	_wlc_aibss_check_pending_data(wlc, FALSE);

	return;
}

void _wlc_aibss_set_ctw(wlc_info_t *wlc, int ctw)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	wlc_mcnx_info_t *mcnx = wlc->mcnx;
	int bss;
	uint16 state;

	if (bsscfg == NULL)
		return;

	bss = wlc_mcnx_BSS_idx(mcnx, bsscfg);
	WL_ERROR(("Set CTW to %d\n", ctw));
	ctw = (ctw<<3)>>10;
	wlc_mcnx_mac_suspend(mcnx);
	state = wlc_mcnx_read_shm(mcnx, M_P2P_BSS_ST(bss));
	state &= ~M_P2P_BSS_ST_SUPR;
	state |= M_P2P_BSS_ST_CTW;
	wlc_mcnx_write_shm(mcnx, M_P2P_BSS_ST(bss), state);
	wlc_mcnx_write_shm(mcnx, M_P2P_BSS_CTW(bss), ctw);
	wlc_mcnx_write_shm(mcnx, M_P2P_BSS_ST(bss), state);
	wlc_mcnx_mac_resume(mcnx);
}

void _wlc_aibss_init_conv_msg(wlc_info_t *wlc, aibss_conv_msg_hdr_t *msg_hdr, int msg_type,
	int msg_body_len)
{

	memset(msg_hdr, 0, sizeof(*msg_hdr));
	msg_hdr->af_hdr.category = DOT11_ACTION_CAT_VS;
	bcopy(BRCM_OUI, &msg_hdr->af_hdr.OUI, sizeof(msg_hdr->af_hdr.OUI));
	msg_hdr->af_hdr.tag = IE_VENDOR_SPECIFIC;
	msg_hdr->af_hdr.tag_len = sizeof(aibss_conv_msg_hdr_t) - sizeof(aibss_vs_af_hdr_t)
		+ msg_body_len;
	bcopy(BRCM_PROP_OUI, &msg_hdr->ie_hdr.OUI, sizeof(msg_hdr->ie_hdr.OUI));
	msg_hdr->ie_hdr.sub_type = BCM_AIBSS_IE_TYPE;
	msg_hdr->msg_type = msg_type;
	msg_hdr->msg_len = msg_body_len;
}

bool _wlc_aibss_validate_conv_msg(wlc_info_t *wlc, aibss_conv_msg_hdr_t *msg_hdr)
{
	if (msg_hdr->af_hdr.category != DOT11_ACTION_CAT_VS)
		return FALSE;
	if (bcmp(&msg_hdr->af_hdr.OUI, BRCM_OUI, DOT11_OUI_LEN) != 0)
		return FALSE;
	if (msg_hdr->af_hdr.tag != IE_VENDOR_SPECIFIC)
		return FALSE;
	if (msg_hdr->af_hdr.tag_len < sizeof(aibss_conv_msg_hdr_t) - sizeof(aibss_vs_af_hdr_t))
		return FALSE;
	if (bcmp(&msg_hdr->ie_hdr.OUI, BRCM_PROP_OUI, DOT11_OUI_LEN) != 0)
		return FALSE;
	if (msg_hdr->ie_hdr.sub_type != BCM_AIBSS_IE_TYPE)
		return FALSE;
	return TRUE;
}

void _wlc_aibss_ack_conv_start_req(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	void *pkt = NULL;
	aibss_conv_msg_hdr_t *conv_msg_hdr = NULL;
	uint8 msg_body_len = 0;
	uint8 msg_len = sizeof(*conv_msg_hdr) + msg_body_len;

	pkt = wlc_frame_get_mgmt(wlc, FC_ACTION, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, msg_len, (uint8**)&conv_msg_hdr);
	if (pkt == NULL || conv_msg_hdr == NULL) {
		WL_ERROR(("wlc_frame_get_mgmt failed\n"));
		return;
	}

	PKTSETPRIO(pkt, PRIO_8021D_NC);
	WLPKTTAG(pkt)->flags |= WLF_PSDONTQ;
	WLF2_PCB2_REG(pkt, WLF2_PCB2_AIBSS_CTRL);
	WLPKTTAG(pkt)->shared.packetid = WLC_AIBSS_CONV_START_ACK;
	_wlc_aibss_init_conv_msg(wlc, conv_msg_hdr, WLC_AIBSS_CONV_START_ACK, msg_body_len);

	if (!wlc_sendctl(wlc, pkt, bsscfg->wlcif->qi, scb, TX_DATA_FIFO, 0, FALSE))
		WL_ERROR(("failed to send mgmt pkt\n"));
	WL_ERROR(("conv_start_ack sent to:"));
	prhex(NULL, scb->ea.octet, ETHER_ADDR_LEN);
}


void _wlc_aibss_ack_conv_start_req_null(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	void *pkt = NULL;

	pkt = wlc_sendnulldata(wlc, bsscfg, &scb->ea, 0, WLF_PSDONTQ, PRIO_8021D_BE);
	if (pkt == NULL)
		return;
	WLPKTTAG(pkt)->flags3 |= WLF3_TXQ_SHORT_LIFETIME;
	WL_ERROR(("conv_start_req ACK sent to:"));
	prhex(NULL, scb->ea.octet, ETHER_ADDR_LEN);
}


void _wlc_aibss_send_conv_end_req(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	void *pkt = NULL;
	aibss_conv_msg_hdr_t *conv_msg_hdr = NULL;
	uint8 msg_body_len = 0;
	uint8 msg_len = sizeof(*conv_msg_hdr) + msg_body_len;

	WL_ERROR(("send conv_end to:"));
	prhex(NULL, scb->ea.octet, ETHER_ADDR_LEN);
	pkt = wlc_frame_get_mgmt(wlc, FC_ACTION, &scb->ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, msg_len, (uint8**)&conv_msg_hdr);
	if (pkt == NULL || conv_msg_hdr == NULL) {
		WL_ERROR(("wlc_frame_get_mgmt failed\n"));
		return;
	}

	WLPKTTAG(pkt)->flags |= WLF_PSDONTQ;
	WLF2_PCB2_REG(pkt, WLF2_PCB2_AIBSS_CTRL);
	WLPKTTAG(pkt)->shared.packetid = WLC_AIBSS_CONV_END_REQ;
	_wlc_aibss_init_conv_msg(wlc, conv_msg_hdr, WLC_AIBSS_CONV_END_REQ, msg_body_len);

	if (!wlc_sendctl(wlc, pkt, bsscfg->wlcif->qi, scb, TX_DATA_FIFO, 0, FALSE))
		WL_ERROR(("failed to send mgmt pkt\n"));

}

void _wlc_aibss_send_conv_end_req_null(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	void *pkt = NULL;

	WL_ERROR(("send conv_end null to:"));
	prhex(NULL, scb->ea.octet, ETHER_ADDR_LEN);
	pkt = wlc_sendnulldata(wlc, bsscfg, &scb->ea, 0, WLF_PSDONTQ, PRIO_8021D_BE);
	if (pkt == NULL)
		return;

	WLPKTTAG(pkt)->flags3 |= WLF3_TXQ_SHORT_LIFETIME;
	WLPKTTAG(pkt)->shared.packetid = WLC_AIBSS_CONV_END_REQ;
	WLF2_PCB2_REG(pkt, WLF2_PCB2_AIBSS_CTRL);
}

void _wlc_aibss_scb_off(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *cur_scb)
{
	struct scb *scb;
	struct scb_iter scbiter;
	bool stop_bsscfg = TRUE;

	/* Inactivate current SCB */
	_wlc_aibss_reset_pend_status(wlc->aibss_info, cur_scb);
	wlc_apps_process_ps_switch(wlc, &cur_scb->ea, TRUE);

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_PS(scb))
			stop_bsscfg = FALSE;
	}

	if (stop_bsscfg)
		_wlc_aibss_bsscfg_off(wlc, bsscfg);
}

void _wlc_aibss_set_hps(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool suppress_packet)
{
	wlc_mcnx_info_t *mcnx = wlc->mcnx;
	int bss;
	uint16 state;

	bss = wlc_mcnx_BSS_idx(mcnx, bsscfg);
	state = wlc_mcnx_read_shm(mcnx, M_P2P_BSS_ST(bss));
	if (suppress_packet) {
		state &= ~M_P2P_BSS_ST_CTW;
		state |= M_P2P_BSS_ST_SUPR;
	} else {
		state &= ~M_P2P_BSS_ST_SUPR;
		state |= M_P2P_BSS_ST_CTW;
	}
	wlc_mcnx_mac_suspend(mcnx);
	wlc_mcnx_write_shm(mcnx, M_P2P_BSS_ST(bss), state);
	wlc_mcnx_mac_resume(mcnx);
	wlc_mcnx_hps_upd(mcnx, bsscfg, M_P2P_HPS_CTW(bss), TRUE);
}

void _wlc_aibss_bsscfg_off(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_bsscfg_tx_stop(bsscfg);
	_wlc_aibss_set_hps(wlc, bsscfg, TRUE);
}

bool _wlc_aibss_prepare_off(wlc_info_t *wlc, wlc_aibss_info_priv_t *aibss, wlc_bsscfg_t *bsscfg,
	bool force_all)
{
	bool no_active_scb = TRUE;
	struct scb *scb;
	struct scb_iter scbiter;
	aibss_cfg_info_t *cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);

	UNUSED_PARAMETER(force_all);
	if (cfg_cubby->pend_multicast) {
		WL_ERROR(("Prepare off (had multi)\n"));
		force_all = TRUE;
	}

	_wlc_aibss_set_hps(wlc, bsscfg, FALSE);
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		aibss_scb_info_t *scb_info = SCB_CUBBY(scb, wlc->aibss_info->scb_handle);

		if (!SCB_PS(scb)) {
			no_active_scb = FALSE;
			if (aibss->use_act_frame)
				_wlc_aibss_send_conv_end_req(wlc, bsscfg, scb);
			else
				_wlc_aibss_send_conv_end_req_null(wlc, bsscfg, scb);
		} else if (scb_info->pend_unicast || !SCB_PS(scb))
			WL_ERROR(("skip conv_end, PS %d pend_unicast %d pend_multicast %d\n",
			SCB_PS(scb), scb_info->pend_unicast, cfg_cubby->pend_multicast));
	}

	if (no_active_scb)
		_wlc_aibss_bsscfg_off(wlc, bsscfg);

	return no_active_scb;
}

void _wlc_aibss_idle_timer_cb(void *arg)
{
	wlc_aibss_info_t *aibss_info = (wlc_aibss_info_t *)arg;
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	wlc_info_t *wlc = aibss->wlc;
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;

	if (bsscfg == NULL || bsscfg->pm->PM == PM_OFF)
		return;

	_wlc_aibss_prepare_off(wlc, aibss, bsscfg, FALSE);
}

void _wlc_aibss_reset_pend_status(wlc_aibss_info_t *aibss_info, struct scb *scb)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(aibss_info);
	aibss_cfg_info_t *cfg_cubby = BSSCFG_CUBBY(aibss->bsscfg, aibss->cfg_cubby_handle);
	aibss_scb_info_t *scb_info = SCB_CUBBY((scb), aibss_info->scb_handle);

	cfg_cubby->pend_multicast = 0;
	scb_info->pend_unicast = 0;
}


bool _wlc_aibss_check_pending_data(wlc_info_t *wlc, bool force_all)
{
	wlc_aibss_info_priv_t *aibss = WLC_AIBSS_INFO_PRIV(wlc->aibss_info);
	wlc_bsscfg_t *bsscfg = aibss->bsscfg;
	aibss_cfg_info_t *cfg_cubby = BSSCFG_CUBBY(bsscfg, aibss->cfg_cubby_handle);
	struct scb *scb;
	struct scb_iter scbiter;
	struct scb *first_scb = NULL;
	int inactive_scb_num = 0;
	void *pkt = NULL;
	aibss_conv_msg_hdr_t *conv_msg_hdr = NULL;
	uint8 msg_body_len;
	uint8 msg_len;
	struct ether_addr broadcast_ea = {.octet = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
	struct ether_addr *request_mac_addr;
	bool idle_soon = TRUE;
	int bss;
	wlc_mcnx_info_t *mcnx = wlc->mcnx;

	if (bsscfg == NULL)
		return FALSE;

	if (cfg_cubby->pend_multicast) {
		WL_ERROR(("Pending multicast\n"));
		force_all = TRUE;
	}

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		aibss_scb_info_t *scb_info = SCB_CUBBY((scb), wlc->aibss_info->scb_handle);
		if (force_all || scb_info->pend_unicast) {
			idle_soon = FALSE;
			if (SCB_PS(scb))
				inactive_scb_num++;
		}
	}
	if (idle_soon)
		wlc_aibss_restart_idle_timeout(wlc->aibss_info, TRUE);

	if (inactive_scb_num == 0) {
		if (force_all)
			WL_ERROR(("bsscfg has no SCB\n"));
		return FALSE;
	}

	msg_body_len = inactive_scb_num * ETHER_ADDR_LEN;
	msg_len = sizeof(*conv_msg_hdr) + msg_body_len;
	pkt = wlc_frame_get_mgmt(wlc, FC_ACTION, &broadcast_ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, msg_len, (uint8**)&conv_msg_hdr);
	if (pkt == NULL || conv_msg_hdr == NULL) {
		WL_ERROR(("wlc_frame_get_mgmt failed\n"));
		return FALSE;
	}

	WLF2_PCB2_REG(pkt, WLF2_PCB2_AIBSS_CTRL);
	PKTSETPRIO(pkt, PRIO_8021D_NC);
	WLPKTTAG(pkt)->flags |= WLF_PSDONTQ;
	WLPKTTAG(pkt)->shared.packetid = WLC_AIBSS_CONV_START_REQ;

	_wlc_aibss_init_conv_msg(wlc, conv_msg_hdr, WLC_AIBSS_CONV_START_REQ, msg_body_len);
	request_mac_addr = (struct ether_addr *)(conv_msg_hdr + 1);
	WL_ERROR(("send conv_start_req to:"));
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		aibss_scb_info_t *scb_info = SCB_CUBBY((scb), wlc->aibss_info->scb_handle);

		if ((force_all || scb_info->pend_unicast) && SCB_PS(scb)) {
			if (first_scb == NULL)
				first_scb = scb;
			*request_mac_addr++ = scb->ea;
			prhex(NULL, (uchar*)&scb->ea, ETHER_ADDR_LEN);
		}
	}

	bss = wlc_mcnx_BSS_idx(mcnx, bsscfg);
	wlc_mcnx_hps_upd(mcnx, bsscfg, M_P2P_HPS_CTW(bss), FALSE);

	if (!wlc_sendctl(wlc, pkt, bsscfg->wlcif->qi, first_scb, TX_CTL_FIFO, 0, FALSE))
		WL_ERROR(("failed to send mgmt pkt\n"));
	return TRUE;
}
