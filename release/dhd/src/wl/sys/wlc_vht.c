/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * VHT support
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_vht.c 467328 2014-04-03 01:23:40Z $
 */


#include "wlc_cfg.h"

#ifdef WL11AC
#include "osl.h"
#include "typedefs.h"
#include "proto/802.11.h"
#include "bcmwifi_channels.h"
#include "bcmutils.h"
#include "bcmendian.h"
#include "siutils.h"
#include "wlioctl.h"
#include "wlc_cfg.h"
#include "wlc_pub.h"
#include <bcmdevs.h>
#include "d11.h"
#include "wlc_bsscfg.h"
#include "wlc_rate.h"
#include "wlc.h"
#include "wlc_ie_misc_hndlrs.h"
#include "wlc_scb.h"
#include "wlc_vht.h"
#include <wlc_txbf.h>
#include "wlc_csa.h"
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_reg.h>
#include <wlc_scb_ratesel.h>
#include <wlc_amsdu.h>
#include <wlc_ht.h>
#include <wlc_ampdu_rx.h>
#include <wlc_pub.h>
#include <wlc_pcb.h>
#include <wl_export.h>
#include <wlc_bmac.h>
#include <wlc_stf.h>
#include <wlc_tx.h>

#define TLVLEN(len)		((len) + TLV_HDR_LEN)
#define VHT_IE_ENCAP_NONE	0x0
#define VHT_IE_ENCAP_HDR	0x1
#define VHT_IE_ENCAP_VHTIE	0x2
#define VHT_IE_ENCAP_ALL	(VHT_IE_ENCAP_HDR | VHT_IE_ENCAP_VHTIE)

#define VHT_IE_FEATURES_HDRLEN	(sizeof(vht_features_ie_hdr_t))

/* 2.4G VHT operation is not specified
 * BRCM supports this mode in 2.4 so these IEs have to be wrapped in a proprietary IE
 * when operating in 2.4G.
 * These IEs are dependent on protocol state, so they are not always present
 * These flags tell the IE code to include this in the management frame being built
 * It takes case of the length computation needed by some of the IE processing
 * code in the state machine
 */
#define VHT_IE_DEFAULT		0x0
#define VHT_IE_PWR_ENVELOPE	0x1
#define VHT_IE_CSA_WRAPPER	0x2

struct wlc_vht_info {
	wlc_info_t *wlc;
	int scb_handle;			/* cubby registration */
	vht_cap_ie_t vht_cap;	/* VHT CAP IE being advertised by this node */
	vht_op_ie_t vht_op; 	/* vht op ie used by this node */
};

/* scb cubby functions and fields */
typedef struct wlc_vht_scb_info
{
	uint8	ratemask;
	uint16	flags; /* converted vht flags */
	uint16	rxmcsmap; /* raw vht rxmcsmap */
	uint8	bwcap;
	uint8	stbc_num;
	uint8	oper_mode; /* VHT operational mode */
	bool	oper_mode_enabled; /* VHT operational mode is enabled */
	uint16	amsdu_mtu_pref;	/* preferred VHT AMSDU mtu in bytes */
	uint8   ampdu_max_exp; /* maximum ampdu length exponent */
} wlc_vht_scb_info_t;

struct wlc_vht_scb_cubby {
	wlc_vht_scb_info_t *vht_scb_info;
};

#define SCB_VHT_CUBBY(vhti, scb) \
	(struct wlc_vht_scb_cubby*)(SCB_CUBBY((scb), (vhti)->scb_handle))
#define SCB_VHT_INFO(vhti, scb) (SCB_VHT_CUBBY(vhti, scb))->vht_scb_info

#if defined(BCMDBG)
static const bcm_bit_desc_t vht_scb_flags[] =
{
	{SCB_VHT_LDPCCAP, "LDPC"},
	{SCB_SGI80, "SGI80"},
	{SCB_SGI160, "SGI160"},
	{SCB_VHT_TX_STBCCAP, "TX_STBC"},
	{SCB_VHT_RX_STBCCAP, "RX_STBC"},
	{SCB_SU_BEAMFORMER, "SU_BEAMFORMER"},
	{SCB_SU_BEAMFORMEE, "SU_BEANFORMEE"},
	{SCB_MU_BEAMFORMER, "MU_BEAMFORMER"},
	{SCB_MU_BEAMFORMEE, "MU_BEANFORMEE"},
	{SCB_VHT_TXOP_PS, "TXOP_PS"},
	{SCB_HTC_VHT_CAP, "HTC_VHT"},
	{0, NULL}
};
#endif 

static void
wlc_vht_scb_dump(void *ctx, struct scb *scbptr, struct bcmstrbuf *b);
static int
wlc_vht_scb_init(void *context, struct scb *scb);
static void
wlc_vht_scb_deinit(void *context, struct scb *scb);

/* IE mgmt */
static uint wlc_vht_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_vht_write_cap_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_vht_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_vht_write_op_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_vht_calc_pwr_env_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_vht_write_pwr_env_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_vht_calc_brcm_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_vht_write_brcm_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_vht_calc_op_mode_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_vht_write_op_mode_ie(void *ctx, wlc_iem_build_data_t *data);
static void wlc_vht_parse_cap(vht_cap_ie_t *cap, wlc_bss_info_t *bi);
#ifdef AP
static int wlc_vht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_vht_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_vht_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data);
#endif
static int wlc_vht_parse_op_mode_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_vht_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_vht_scan_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_vht_scan_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data);
#ifdef AP
static uint wlc_vht_csw_calc_pwr_env_ie_len(void *ctx, wlc_iem_calc_data_t *calc);
static int wlc_vht_csw_write_pwr_env_ie(void *ctx, wlc_iem_build_data_t *build);
#endif
#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
static int wlc_vht_tdls_parse_op_ie(void *ctx, wlc_iem_parse_data_t *parse);
static int wlc_vht_tdls_parse_op_mode_ie(void *ctx, wlc_iem_parse_data_t *parse);
static int wlc_vht_tdls_parse_cap_ie(void * ctx, wlc_iem_parse_data_t * parse);
static int wlc_vht_tdls_parse_brcm_ie(void * ctx, wlc_iem_parse_data_t *data);
static uint wlc_vht_tdls_calc_brcm_ie_len(void * ctx, wlc_iem_calc_data_t * data);
static int wlc_vht_tdls_write_brcm_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_vht_tdls_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data);
#endif

/* VHT config management */
static int wlc_vht_set_mode(wlc_vht_info_t *vhti, bool enable);
static int wlc_vht_setup_mode(wlc_vht_info_t *vhti, bool enable);
static uint8 wlc_get_oper_mode(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg);
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_PRHDRS) || \
	defined(WLMSG_PRPKT) || defined(WLMSG_ASSOC) || defined(DHD_DEBUG)
static int wlc_vht_dump_cap(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG */
static void
wlc_vht_upd_rate_mcsmap_ex(wlc_vht_info_t *vhti, struct scb *scb, uint16 rxmcsmap);

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
static dot11_oper_mode_notif_ie_t *
wlc_read_oper_mode_notif_ie(wlc_vht_info_t *vhti, uint8 *tlvs, int tlvs_len,
	dot11_oper_mode_notif_ie_t *ie);
#endif /* WLTDLS && !WLTDLS_DISAB */


#if defined(BCMDBG) || defined(BCMCONDITIONAL_LOGGING)
/* Rx mcsmap */
static uint16 wlc_vht_get_scb_rxmcsmap(wlc_vht_info_t *vhti, struct scb *scb);
/* Tx mcsmap */
static uint16 wlc_vht_get_tx_mcsmap(wlc_vht_info_t *vhti);
#endif /* BCMDBG || BCMCONDITIONAL_LOGGING */

/* finish going up */
static int wlc_vht_up(void *vhti);
static void wlc_vht_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>


/* iovars */
static int
wlc_vht_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int alen, int vsize, struct wlc_if *wlcif);

enum {
	IOV_VHTMODE = 0,
	IOV_VHTFEATURES = 1,
	IOV_VHTLAST = 2
};

static const bcm_iovar_t vht_iovars[] = {
	{"vhtmode", IOV_VHTMODE,
	(IOVF_SET_DOWN|IOVF_OPEN_ALLOW|IOVF_RSDB_SET), IOVT_UINT32, 0
	},
	{"vht_features", IOV_VHTFEATURES,
	(IOVF_SET_DOWN|IOVF_OPEN_ALLOW|IOVF_RSDB_SET), IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0}
};

wlc_vht_info_t *
BCMATTACHFN(wlc_vht_attach)(wlc_info_t *wlc)
{
	wlc_vht_info_t *vhti;
	uint16 capfstbmp =
	        FT2BMP(FC_BEACON) |
	        FT2BMP(FC_PROBE_RESP) |
#ifdef STA
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
#endif
#ifdef AP
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
#endif
	        FT2BMP(FC_PROBE_REQ) |
	        0;
	uint16 opfstbmp =
	        FT2BMP(FC_BEACON) |
	        FT2BMP(FC_PROBE_RESP) |
#ifdef AP
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
#endif
	        0;
	uint16 modefstbmp =
	        FT2BMP(FC_BEACON) |
	        FT2BMP(FC_PROBE_RESP) |
#ifdef STA
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
#endif
#ifdef AP
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
#endif
	        0;
	uint16 brcmfstbmp =
	        FT2BMP(FC_BEACON) |
	        FT2BMP(FC_PROBE_RESP) |
#ifdef STA
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
#endif
#ifdef AP
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
#endif
	        FT2BMP(FC_PROBE_REQ) |
	        0;
	uint16 bcnfstbmp = FT2BMP(FC_BEACON) | FT2BMP(FC_PROBE_RESP);
#ifdef AP
	uint16 arqfstbmp = FT2BMP(FC_ASSOC_REQ) | FT2BMP(FC_REASSOC_REQ);
#endif
	uint16 mode_parse_fstbmp =
#ifdef AP
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_REASSOC_REQ) |
#endif
#ifdef STA
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_RESP) |
	        FT2BMP(FC_BEACON) |
#endif
	        0;
	uint16 scanfstbmp = FT2BMP(WLC_IEM_FC_SCAN_BCN) | FT2BMP(WLC_IEM_FC_SCAN_PRBRSP);

	/* allocate private states */
	if ((vhti = MALLOCZ(wlc->osh, sizeof(wlc_vht_info_t))) == NULL) {
		goto fail;
	}
	vhti->wlc = wlc;

	/* register IE mgmt callbacks */
	/* calc/build */
	/* bcn/prbrsp/assocreq/reassocreq/assocresp/reassocresp/prbreq */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, capfstbmp, DOT11_MNG_VHT_CAP_ID,
	       wlc_vht_calc_cap_ie_len, wlc_vht_write_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht cap ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, opfstbmp, DOT11_MNG_VHT_OPERATION_ID,
	      wlc_vht_calc_op_ie_len, wlc_vht_write_op_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht op ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, bcnfstbmp, DOT11_MNG_VHT_TRANSMIT_POWER_ENVELOPE_ID,
	      wlc_vht_calc_pwr_env_ie_len, wlc_vht_write_pwr_env_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, pwr env ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocreq/reassocreq/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, modefstbmp, DOT11_MNG_OPER_MODE_NOTIF_ID,
	      wlc_vht_calc_op_mode_ie_len, wlc_vht_write_op_mode_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, op mode ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocreq/reassocreq/assocresp/reassocresp/prbreq */
	if (wlc_iem_vs_add_build_fn_mft(wlc->iemi, brcmfstbmp, WLC_IEM_VS_IE_PRIO_BRCM_VHT,
	      wlc_vht_calc_brcm_ie_len, wlc_vht_write_brcm_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_build_fn() failed, brcm ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef AP
	/* bcn/prbrsp */
	if (wlc_ier_add_build_fn(wlc->ier_csw, DOT11_MNG_VHT_TRANSMIT_POWER_ENVELOPE_ID,
	      wlc_vht_csw_calc_pwr_env_ie_len, wlc_vht_csw_write_pwr_env_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, pwr env ie in csw\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif

	/* tdls ie start */
	/* setup_request */
#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
	if (wlc_ier_add_build_fn(wlc->ier_tdls_srq, DOT11_MNG_VHT_CAP_ID,
		wlc_vht_calc_cap_ie_len, wlc_vht_write_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht cap ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* for 2.4 GHz : VHT IE's Encapsulated in BRCM Prop IE */
	if (wlc_ier_add_build_fn(wlc->ier_tdls_srq, DOT11_MNG_VS_ID,
		wlc_vht_tdls_calc_brcm_ie_len, wlc_vht_tdls_write_brcm_ie, vhti)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht brcm ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* setup_response */
	if (wlc_ier_add_build_fn(wlc->ier_tdls_srs, DOT11_MNG_VHT_CAP_ID,
		wlc_vht_calc_cap_ie_len, wlc_vht_write_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht cap ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_ier_add_build_fn(wlc->ier_tdls_srs, DOT11_MNG_OPER_MODE_NOTIF_ID,
		wlc_vht_calc_op_mode_ie_len, wlc_vht_write_op_mode_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, op mode notif in setupresp\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* for 2.4 GHz : VHT IE's Encapsulated in BRCM Prop IE */
	if (wlc_ier_add_build_fn(wlc->ier_tdls_srs, DOT11_MNG_VS_ID,
		wlc_vht_tdls_calc_brcm_ie_len, wlc_vht_tdls_write_brcm_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht brcm ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* setup_confirm */
	if (wlc_ier_add_build_fn(wlc->ier_tdls_scf, DOT11_MNG_VHT_OPERATION_ID,
		wlc_vht_tdls_calc_op_ie_len, wlc_vht_write_op_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht op ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_ier_add_build_fn(wlc->ier_tdls_scf, DOT11_MNG_OPER_MODE_NOTIF_ID,
		wlc_vht_calc_op_mode_ie_len, wlc_vht_write_op_mode_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_ier_add_build_fn failed, op mode notif in setupresp\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* for 2.4 GHz : VHT IE's Encapsulated in BRCM Prop IE */
	if (wlc_ier_add_build_fn(wlc->ier_tdls_scf, DOT11_MNG_VS_ID,
		wlc_vht_tdls_calc_brcm_ie_len, wlc_vht_tdls_write_brcm_ie, vhti)
		!= BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht brcm ie\n",
			wlc->pub->unit, __FUNCTION__));
	}

	/* disc resp */
	if (wlc_ier_add_build_fn(wlc->ier_tdls_drs, DOT11_MNG_VHT_CAP_ID,
		wlc_vht_calc_cap_ie_len, wlc_vht_write_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, vht cap ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* tdls ie end */
#endif /* defined(WLTDLS) && !defined(WLTDLS_DISABLED) */

	/* parse */
#ifdef AP
	/* assocreq/reassocreq */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_VHT_CAP_ID,
	                             wlc_vht_parse_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, cap ie in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, arqfstbmp, DOT11_MNG_VHT_OPERATION_ID,
	                             wlc_vht_parse_op_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, op ie in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, arqfstbmp, WLC_IEM_VS_IE_PRIO_BRCM_VHT,
	                                wlc_vht_parse_brcm_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn() failed, brcm ie in assocreq\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* AP */
	/* assocreq/reassocreq/assocresp/reassocresp/bcn */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, mode_parse_fstbmp, DOT11_MNG_OPER_MODE_NOTIF_ID,
	                             wlc_vht_parse_op_mode_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, mode ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp in scan */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, scanfstbmp, DOT11_MNG_VHT_CAP_ID,
	                             wlc_vht_scan_parse_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, cap ie in scan\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, scanfstbmp, DOT11_MNG_VHT_OPERATION_ID,
	                             wlc_vht_scan_parse_op_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, op ie in scan\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, scanfstbmp, WLC_IEM_VS_IE_PRIO_BRCM_VHT,
	                                wlc_vht_scan_parse_brcm_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn() failed, brcm ie in scan\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* TDLS */
	/* setup_request */
#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
	if (wlc_ier_add_parse_fn(wlc->ier_tdls_srq, DOT11_MNG_VHT_CAP_ID,
		wlc_vht_tdls_parse_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, cap ie in assocreq\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_ier_add_parse_fn(wlc->ier_tdls_srq, DOT11_MNG_VS_ID,
		wlc_vht_tdls_parse_brcm_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, vht brcm ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* setup_response */
	if (wlc_ier_add_parse_fn(wlc->ier_tdls_srs, DOT11_MNG_VHT_CAP_ID,
		wlc_vht_tdls_parse_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, cap ie in assocreq\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_ier_add_parse_fn(wlc->ier_tdls_srs, DOT11_MNG_OPER_MODE_NOTIF_ID,
		wlc_vht_tdls_parse_op_mode_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, mode ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_ier_add_parse_fn(wlc->ier_tdls_srs, DOT11_MNG_VS_ID,
		wlc_vht_tdls_parse_brcm_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, vht brcm ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* setup_confirm */
	if (wlc_ier_add_parse_fn(wlc->ier_tdls_scf, DOT11_MNG_VHT_OPERATION_ID,
		wlc_vht_tdls_parse_op_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, op ie in assocreq\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_ier_add_parse_fn(wlc->ier_tdls_scf, DOT11_MNG_OPER_MODE_NOTIF_ID,
		wlc_vht_tdls_parse_op_mode_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, mode ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_ier_add_parse_fn(wlc->ier_tdls_scf, DOT11_MNG_VS_ID,
		wlc_vht_tdls_parse_brcm_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, vht brcm ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* disc_response */
	if (wlc_ier_add_parse_fn(wlc->ier_tdls_drs, DOT11_MNG_VHT_CAP_ID,
		wlc_vht_tdls_parse_cap_ie, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, cap ie in assocreq\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* defined(WLTDLS) && !defined(WLTDLS_DISABLED) */

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, vht_iovars, "vhti", vhti, wlc_vht_doiovar,
	                        NULL, wlc_vht_up, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	vhti->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(struct wlc_vht_scb_cubby),
		wlc_vht_scb_init, wlc_vht_scb_deinit, wlc_vht_scb_dump, (void *)vhti);

	if (vhti->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		goto fail;
	}
	/* bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_vht_bsscfg_updn, vhti) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_PRHDRS) || \
	defined(WLMSG_PRPKT) || defined(WLMSG_ASSOC) || defined(DHD_DEBUG)
	wlc_dump_register(wlc->pub, "vhtcap", (dump_fn_t)wlc_vht_dump_cap, (void *)wlc);
#endif /* BCMDBG etc. */

	return vhti;
fail:
	wlc_vht_detach(vhti);
	return NULL;
}

static void
wlc_vht_scb_dump(void *ctx, struct scb *scbptr, struct bcmstrbuf *b)
{
#if defined(BCMDBG)
	/* dump cubby */
	char vhtflagsstr[64];
	struct wlc_vht_info *vhti = (struct wlc_vht_info *)ctx;
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scbptr);

	bcm_format_flags(vht_scb_flags, cubby_info->flags, vhtflagsstr, 64);
	if (SCB_VHT_CAP(scbptr)) {
		bcm_bprintf(b, " VHT Flags:0x%x", cubby_info->flags);
		bcm_bprintf(b, " VHT rxmcsmap:0x%x", cubby_info->rxmcsmap);
		if (vhtflagsstr[0] != '\0')
			bcm_bprintf(b, " (%s)", vhtflagsstr);
	}
	if (cubby_info->stbc_num > 0) {
		bcm_bprintf(b, " stbc num = %d", cubby_info->stbc_num);
	}
	if (VHT_ENAB_BAND(vhti->wlc->pub, vhti->wlc->band->bandtype) &&
	    SCB_VHT_CAP(scbptr)) {
		bcm_bprintf(b, "     VHT ratemask: 0x%x\n", cubby_info->ratemask);
		wlc_dump_vht_mcsmap("     VHT mcsmap:", scbptr->rateset.vht_mcsmap, b);
	}
#endif 
}

static int
wlc_vht_scb_init(void *context, struct scb *scb)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)context;
	if ((SCB_VHT_INFO(vhti, scb) =
		MALLOC(vhti->wlc->osh, sizeof(struct wlc_vht_scb_info))) == NULL) {
		return BCME_NOMEM;
	}
	bzero(SCB_VHT_INFO(vhti, scb), sizeof(struct wlc_vht_scb_info));
	return BCME_OK;
}

static void
wlc_vht_scb_deinit(void *context, struct scb *scb)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)context;
	if (SCB_VHT_INFO(vhti, scb)) {
		MFREE(vhti->wlc->osh, SCB_VHT_INFO(vhti, scb),
			sizeof(struct wlc_vht_scb_info));
		SCB_VHT_INFO(vhti, scb) = NULL;
	}
}

/* get current cap info this node uses */
uint32
wlc_vht_get_cap_info(wlc_vht_info_t *vhti)
{
	return vhti->vht_cap.vht_cap_info;
}

void
wlc_vht_set_ldpc_cap(wlc_vht_info_t *vhti, bool enab)
{
	if (enab) {
		vhti->vht_cap.vht_cap_info |= VHT_CAP_INFO_LDPC;
	} else {
		vhti->vht_cap.vht_cap_info &= ~VHT_CAP_INFO_LDPC;
	}
}

void
wlc_vht_set_tx_stbc_cap(wlc_vht_info_t *vhti, bool enab)
{
	if (enab) {
		vhti->vht_cap.vht_cap_info |= VHT_CAP_INFO_TX_STBC;
	} else {
		vhti->vht_cap.vht_cap_info &= ~VHT_CAP_INFO_TX_STBC;
	}
}

void
wlc_vht_set_rx_stbc_cap(wlc_vht_info_t *vhti, int val)
{
	vhti->vht_cap.vht_cap_info &= ~VHT_CAP_INFO_RX_STBC_MASK;
	vhti->vht_cap.vht_cap_info |= (val << VHT_CAP_INFO_RX_STBC_SHIFT);
}
void
wlc_vht_upd_txbf_cap(wlc_vht_info_t *vhti, bool bfr, bool bfe)
{
	wlc_info_t *wlc = vhti->wlc;
	uint32 cap = 0;
	vhti->vht_cap.vht_cap_info &=
		~(VHT_CAP_INFO_SU_BEAMFMR |
		VHT_CAP_INFO_SU_BEAMFMEE |
		VHT_CAP_INFO_NUM_BMFMR_ANT_MASK |
		VHT_CAP_INFO_NUM_SOUNDING_DIM_MASK |
		VHT_CAP_INFO_LINK_ADAPT_CAP_MASK);
	if (bfr)
		cap = VHT_CAP_INFO_SU_BEAMFMR;

	if (bfe)
		cap |= VHT_CAP_INFO_SU_BEAMFMEE;

	/*
	 * Beamformee's capability of max. beamformer's antenas it can support
	 * should be 2 for brcm AC device
	 */

	if (bfr || bfe) {
		cap |= (2 << VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT);
		cap |= ((wlc->stf->txstreams - 1) << VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT);
		cap |= (3 << VHT_CAP_INFO_LINK_ADAPT_CAP_SHIFT); /* both */
	}

	vhti->vht_cap.vht_cap_info |= cap;
}

void
wlc_vht_upd_txbf_virtif_cap(wlc_vht_info_t *vhti, bool bfr, bool bfe, uint32 *cap)
{
	wlc_info_t *wlc = vhti->wlc;

	if (!bfr)
		 *cap &= ~VHT_CAP_INFO_SU_BEAMFMR;

	if (!bfe)
		 *cap &= ~VHT_CAP_INFO_SU_BEAMFMEE;

	if (!(bfr || bfe)) {
		*cap &= ~(2 << VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT);
		*cap &= ~((wlc->stf->txstreams - 1) << VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT);
		*cap &= ~(3 << VHT_CAP_INFO_LINK_ADAPT_CAP_SHIFT); /* both */
	}

}
static int
wlc_vht_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int alen, int vsize, struct wlc_if *wlcif)
{
	int err = BCME_OK;
	wlc_vht_info_t *vhti = (wlc_vht_info_t*)context;
	wlc_info_t *wlc = vhti->wlc;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

#define WLC_VHT_FEATURES_2G_ENAB		0x00000001
#define WLC_VHT_FEATURES_PROPRATES_ENAB		0x00000002
#define WLC_VHT_FEATURES_MCSMAP_0_7		2
#define WLC_VHT_FEATURES_MCSMAP_0_8		1
#define WLC_VHT_FEATURES_MCSMAP_0_9		0
#define WLC_VHT_FEATURES_MCSMAP_NONE		3
#define WLC_VHT_FEATURES_MCSMAP_M		0x00000003
#define WLC_VHT_FEATURES_MCSMAP_S		4
#define WLC_VHT_FEATURES_GET_MCSLIMIT(v)	(((v) >> WLC_VHT_FEATURES_MCSMAP_S) & \
						WLC_VHT_FEATURES_MCSMAP_M)
#define WLC_VHT_FEATURES_SET_MCSLIMIT(v)	((v) << WLC_VHT_FEATURES_MCSMAP_S)
	switch (actionid) {
		case IOV_GVAL(IOV_VHTFEATURES):
			{
			/* VHT features return: bit 0 2G VHT,bit1 VHT ext rates, set if enabled */

				uint8 mcslimit = 0;
				*ret_int_ptr = 0;

				if (WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_2G))
						*ret_int_ptr |= WLC_VHT_FEATURES_2G_ENAB;
				if (WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_RATEMASK))
						*ret_int_ptr |= WLC_VHT_FEATURES_PROPRATES_ENAB;

				/*
				 * Swap around the labels to avoid having to change DVT/SIG scripts
				 * Our default is MCS 0-9
				 */

				switch (WLC_VHT_FEATURES_MCS_GET(wlc->pub))
				{
				case VHT_CAP_MCS_MAP_0_7:
					mcslimit = WLC_VHT_FEATURES_MCSMAP_0_7;
					break;
				case VHT_CAP_MCS_MAP_0_8:
					mcslimit = WLC_VHT_FEATURES_MCSMAP_0_8;
					break;
				case VHT_CAP_MCS_MAP_0_9:
					mcslimit = WLC_VHT_FEATURES_MCSMAP_0_9;
					break;
				case VHT_CAP_MCS_MAP_NONE:
					mcslimit = WLC_VHT_FEATURES_MCSMAP_NONE;
					break;
				}
				*ret_int_ptr |= WLC_VHT_FEATURES_SET_MCSLIMIT(mcslimit);
			}
			break;
		case IOV_SVAL(IOV_VHTFEATURES):
			if (!WLC_PHY_VHT_CAP(wlc->band)) {
				err = BCME_UNSUPPORTED;
			} else if (!(WLC_VHT_PROP_RATES_CAP_PHY(wlc)) &&
				(int_val & WLC_VHT_FEATURES_PROPRATES_ENAB)) {
				/* Check for Proprietary rate support in PHY */
				err = BCME_BADOPTION;
				break;
			} else if ((IS_SINGLEBAND_5G(wlc->deviceid)) &&
				(int_val & WLC_VHT_FEATURES_2G_ENAB)) {
				/* Abort if we try to enable 2G VHT on a singleband 5G device */
				err = BCME_BADOPTION;
				break;
			} else if (WLC_VHT_FEATURES_GET_MCSLIMIT(int_val) ==
				WLC_VHT_FEATURES_MCSMAP_NONE) {
				/* VHT rate disable unsupported, value global for all streams */
				err = BCME_BADOPTION;
			} else {
				uint8 mcslimit = (WLC_VHT_FEATURES_MCS_GET(wlc->pub));

				WLC_VHT_FEATURES_DEFAULT(wlc->pub, wlc->bandstate, wlc->nbands);

			/* VHT features set value:bit 0 2G VHT,bit1 VHT ext rates, set if enabled */
				if (int_val & WLC_VHT_FEATURES_2G_ENAB) {
					WLC_VHT_FEATURES_SET(wlc->pub, WL_VHT_FEATURES_2G);
				} else {
					WLC_VHT_FEATURES_CLR(wlc->pub, WL_VHT_FEATURES_2G);
				}

				if (int_val & WLC_VHT_FEATURES_PROPRATES_ENAB) {
					WLC_VHT_FEATURES_SET(wlc->pub, WL_VHT_FEATURES_RATES_ALL);
				} else {
					WLC_VHT_FEATURES_CLR(wlc->pub, WL_VHT_FEATURES_RATES_ALL);
				}
				/*
				 * Bit 4:5 is the allowed MCS rates
				 * 0 is MCS 0-9
				 * 1 is MCS 0-8
				 * 2 is MCS 0-7
				 * VHT disable (MCS == none) not supported via this mechanism
				 */
				switch (WLC_VHT_FEATURES_GET_MCSLIMIT(int_val))
				{
				case WLC_VHT_FEATURES_MCSMAP_0_9:
					mcslimit = VHT_CAP_MCS_MAP_0_9;
					break;
				case WLC_VHT_FEATURES_MCSMAP_0_8:
					mcslimit = VHT_CAP_MCS_MAP_0_8;
					break;
				case WLC_VHT_FEATURES_MCSMAP_0_7:
					mcslimit = VHT_CAP_MCS_MAP_0_7;
					break;
				}

				WLC_VHT_FEATURES_MCS_SET(wlc->pub, mcslimit);
				wlc_vht_setup_mode(wlc->vhti, TRUE);
			}
			break;
		case IOV_GVAL(IOV_VHTMODE):
			*ret_int_ptr = (int32)(VHT_ENAB(wlc->pub) != 0);
			break;
		case IOV_SVAL(IOV_VHTMODE):
			if (!WLC_PHY_VHT_CAP(wlc->band)) {
				err = BCME_UNSUPPORTED;
				break;
			}
			if ((int_val != OFF) && (int_val != ON)) {
				err = BCME_RANGE;
				break;
			}
			err = wlc_vht_setup_mode(wlc->vhti, (int_val == ON));

			break;
		default:
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

void
BCMATTACHFN(wlc_vht_detach)(wlc_vht_info_t *vhti)
{
	wlc_info_t *wlc;

	if (vhti == NULL)
		return;

	wlc = vhti->wlc;

	wlc_module_unregister(wlc->pub, "vhti", vhti);

	MFREE(wlc->osh, vhti, sizeof(wlc_vht_info_t));
}

/*
 * How to add VHT IEs for automatic encapsulation
 *
 * Provide a length calculating routine for IE, return length --including-- the TLV header length
 *
 * Provide a formatting routine for the IE, at minimum,
 * it must take the buffer pointer, buffer length and return the modified buffer pointer
 *
 * Add length routine to wlc_vht_ie_len() for the desired frametypes
 *
 * Add formatting routine to wlc_vht_write_vht_elements() for the desired frametypes
 *
 * If the IE is not mandatory or can be suppressed for low mem applications, add a flag to
 * vlc_vht.h to the VHT_IE_XXX group of defines
 */
void
BCMATTACHFN(wlc_vht_init_defaults)(wlc_vht_info_t *vhti)
{
	uint i;
	wlc_info_t *wlc = vhti->wlc;
	uint32 cap;
#ifdef WL11N_STBC_RX_ENABLED
	uint32 rx_stbc_Nss;
#endif /* WL11N_STBC_RX_ENABLED */
	ASSERT(wlc);

	/* VHT Capabilites IE (802.11 sec 8.4.2.160) */
	cap = 0;


	/* AMPDU limit for AMSDU agg, support max 11k */
	cap |= VHT_CAP_MPDU_MAX_11K;

	/* Support 20/40/80/80+80/160 MHz based on PHY capabality */
	if (WLC_8080MHZ_CAP_PHY(wlc)) {
		cap |= VHT_CAP_CHAN_WIDTH_SUPPORT_160_8080;
		cap |= VHT_CAP_INFO_SGI_160MHZ;
	}
	else if (WLC_160MHZ_CAP_PHY(wlc)) {
		cap |= VHT_CAP_CHAN_WIDTH_SUPPORT_160;
		cap |= VHT_CAP_INFO_SGI_160MHZ;
	}
	else {
		cap |= VHT_CAP_CHAN_WIDTH_SUPPORT_MANDATORY;
		cap |= VHT_CAP_INFO_SGI_80MHZ;
	}
	cap |= VHT_CAP_INFO_LDPC;
#ifdef WL11N_STBC_RX_ENABLED
	if (wlc->stf->txstreams > 1)
		cap |= VHT_CAP_INFO_TX_STBC;
	/* calculate Rx STBC support as (rx_chain / 2)
	 *
	 * 1   Chain  -> No STBC
	 * 2-3 Chains -> STBC on Nss = 1 (Nsts = 2)
	 * 4-5 Chains -> STBC on Nss = 2 (Nsts = 4)
	 * 6-7 Chains -> STBC on Nss = 3 (Nsts = 6)
	 * 8   Chains -> STBC on Nss = 4 (Nsts = 8)
	 */
	rx_stbc_Nss = WLC_BITSCNT(wlc->stf->hw_rxchain) / 2;
	cap |= (rx_stbc_Nss << VHT_CAP_INFO_RX_STBC_SHIFT);
#endif /* WL11N_STBC_RX_ENABLED */

	/* AMPDU length limit, support max 1MB (2 ^ (13 + 7)) */
#ifdef WLAMPDU
	cap |= (wlc_ampdu_get_rx_factor(wlc) << VHT_CAP_INFO_AMPDU_MAXLEN_EXP_SHIFT);
#else
	/* This is required to avoid the precommit error for builds without AMPDU. */
	cap |= (AMPDU_RX_FACTOR_1024K << VHT_CAP_INFO_AMPDU_MAXLEN_EXP_SHIFT);
#endif /* WLAMPDU */

	vhti->vht_cap.vht_cap_info = cap;

#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub))
		wlc_txbf_upd_bfr_bfe_cap(wlc->txbf);
#endif

	/* Fill out the Rx and Tx MCS support */
	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		uint8 mcs;
		uint8 mcsmap_override = WLC_VHT_FEATURES_MCS_GET(wlc->pub);

		/* Declare MCS support for all tx chains configured */
		if (i <= WLC_BITSCNT(wlc->stf->hw_txchain)) {
			if (BCM256QAM_DSAB(wlc)) {
				mcs = VHT_CAP_MCS_MAP_0_7;
			} else {
				mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i,
					wlc->default_bss->rateset.vht_mcsmap);
				if (mcs != VHT_CAP_MCS_MAP_NONE) {
					mcs = MIN(mcs, mcsmap_override);
				}
			}
		} else {
			mcs = VHT_CAP_MCS_MAP_NONE;
		}

		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, vhti->vht_cap.rx_mcs_map);
		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, vhti->vht_cap.tx_mcs_map);
	}

	WL_TRACE(("%s: cap info=%04x\n", __FUNCTION__, vhti->vht_cap.vht_cap_info));

	wlc->default_bss->vht_capabilities = vhti->vht_cap.vht_cap_info;
	wlc->default_bss->vht_rxmcsmap = vhti->vht_cap.rx_mcs_map;
	wlc->default_bss->vht_txmcsmap = vhti->vht_cap.tx_mcs_map;

	WL_RATE(("%s: rx mcsmap (IE) %04x, tx %04x (hwchain %d)\n", __FUNCTION__,
		wlc->default_bss->vht_rxmcsmap,
		wlc->default_bss->vht_txmcsmap,
		wlc->stf->hw_txchain));
}

#ifdef WLAMPDU
/* Function called from ampdu_rx module to update the vht_rx_factor */
/* when IOVAR (ampdu_rx_factor) set is done */
void
wlc_vht_update_ampdu_cap(wlc_vht_info_t *vhti, uint8 vht_rx_factor)
{
	wlc_info_t *wlc = vhti->wlc;
	WL_TRACE(("wl%d: %s(%04x)\n", wlc->pub->unit, __FUNCTION__, vht_rx_factor));

	/* Update the Max AMPDU Rx Exponent */
	vhti->vht_cap.vht_cap_info &= ~(VHT_CAP_INFO_AMPDU_MAXLEN_EXP_MASK);
	vhti->vht_cap.vht_cap_info |= (vht_rx_factor << VHT_CAP_INFO_AMPDU_MAXLEN_EXP_SHIFT);

	/* Update the Beacon and Probe response */
	if (wlc->pub->up) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}
#endif /* WLAMPDU */

void
wlc_vht_update_mcs_cap(wlc_vht_info_t *vhti)
{
	uint i;
	uint8 mcs;
	wlc_info_t *wlc = vhti->wlc;

	vhti->vht_cap.tx_mcs_map = vhti->vht_cap.rx_mcs_map = 0;

	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {

		/* Set up tx mcs map according to number of tx chains */
		if (i <= WLC_BITSCNT(wlc->stf->hw_txchain)) {
			mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, wlc->default_bss->rateset.vht_mcsmap);
			if (mcs != VHT_CAP_MCS_MAP_NONE)
				mcs = MIN(mcs, WLC_VHT_FEATURES_MCS_GET(wlc->pub));
		} else {
			mcs = VHT_CAP_MCS_MAP_NONE;
		}
		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, vhti->vht_cap.tx_mcs_map);

		/* Set up rx mcs map according to number of rx chains */
		if (i <= WLC_BITSCNT(wlc->stf->hw_rxchain)) {
			mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, wlc->default_bss->rateset.vht_mcsmap);
			if (mcs != VHT_CAP_MCS_MAP_NONE)
				mcs = MIN(mcs, WLC_VHT_FEATURES_MCS_GET(wlc->pub));
		} else {
			mcs = VHT_CAP_MCS_MAP_NONE;
		}
		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, vhti->vht_cap.rx_mcs_map);
	}

	WL_RATE(("%s: defmap %04x rx_mcs_map %04x tx_mcs_map %04x\n",
		__FUNCTION__, wlc->default_bss->rateset.vht_mcsmap,
		vhti->vht_cap.rx_mcs_map, vhti->vht_cap.tx_mcs_map));

	/* update beacon/probe resp for AP */
	if (wlc->pub->up && AP_ENAB(wlc->pub) && wlc->pub->associated) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

/* copy ie int passed in struct, then fix endianness  */
vht_cap_ie_t *
wlc_read_vht_cap_ie(wlc_vht_info_t *vhti, uint8 *tlvs, int tlvs_len, vht_cap_ie_t* cap_ie)
{
	bcm_tlv_t *cap_ie_tlv;
	ASSERT(cap_ie);
	cap_ie_tlv = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_VHT_CAP_ID);
	if (cap_ie_tlv) {
		if (cap_ie_tlv->len >= VHT_CAP_IE_LEN) {
			bcopy(&cap_ie_tlv->data, cap_ie, VHT_CAP_IE_LEN);
			cap_ie->vht_cap_info = ltoh32(cap_ie->vht_cap_info);

			cap_ie->rx_mcs_map = ltoh16(cap_ie->rx_mcs_map);
			cap_ie->rx_max_rate = ltoh16(cap_ie->rx_max_rate);
			cap_ie->tx_mcs_map = ltoh16(cap_ie->tx_mcs_map);
			cap_ie->tx_max_rate = ltoh16(cap_ie->tx_max_rate);

			return cap_ie;
		} else {
			WL_ERROR(("wl%d: %s: std len %d does not match %d\n",
			        vhti->wlc->pub->unit, __FUNCTION__,
				cap_ie_tlv->len, VHT_CAP_IE_LEN));
		}
	}

	return NULL;
}

vht_op_ie_t *
wlc_read_vht_op_ie(wlc_vht_info_t *vhti, uint8 *tlvs, int tlvs_len, vht_op_ie_t* op_ie)
{
	bcm_tlv_t *op_ie_tlv;

	op_ie_tlv = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_VHT_OPERATION_ID);
	if (op_ie_tlv) {
		if (op_ie_tlv->len >= VHT_OP_IE_LEN) {
			bcopy(&op_ie_tlv->data, op_ie, VHT_OP_IE_LEN);
			op_ie->supp_mcs = ltoh16(op_ie->supp_mcs);
			return op_ie;
		}
		else
			WL_ERROR(("wl%d: %s: std len %d does not match %d\n",
				vhti->wlc->pub->unit, __FUNCTION__, op_ie_tlv->len,
				VHT_OP_IE_LEN));
	}

	return NULL;
}

uint8 *
wlc_read_vht_features_ie(wlc_vht_info_t *vhti, uint8 *tlvs, int tlvs_len,
	uint8 *rate_mask, int *prop_tlv_len)
{
	bcm_tlv_t *elt = NULL;
	vht_features_ie_hdr_t *features_ie = NULL;
	uint8 *prop_tlvs = NULL;
	uint8 ouitype = VHT_FEATURES_IE_TYPE;

	elt = bcm_find_vendor_ie(tlvs, tlvs_len, BRCM_PROP_OUI, &ouitype, sizeof(ouitype));
	if (elt) {
		features_ie = (vht_features_ie_hdr_t *)&elt->data;
		*rate_mask = features_ie->rate_mask;
		if (elt->len > (VHT_IE_FEATURES_HDRLEN + 2)) {
			prop_tlvs = (uint8 *)&features_ie[1];
			/* Note elt->len does not include the 2 byte IE header */
			*prop_tlv_len = elt->len - VHT_IE_FEATURES_HDRLEN;
		}
	}
	return prop_tlvs;

}


/* see 802.11ac D3.0 - 8.4.1.50 */
static void
wlc_update_oper_mode_notif_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 narrow_bw,
	dot11_oper_mode_notif_ie_t *ie)
{
	uint8 oper_bw;
	uint8 oper_nss;
	uint8 bss_oper_bw;

	/*
	 * Drop bandwidth of current chanspec from 80Mhz to
	 * 40Mhz/20Mhz due to CLM limitation.  Combine CLM limitation
	 * with existing oper_mode setting and then indicate current
	 * operation bandwidth with Oper-Mode-Notification IE if needed.
	 */
	bss_oper_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(ie->mode);
	oper_bw = bss_oper_bw;
	if (narrow_bw != NARROW_BW_NONE) {
		if (narrow_bw == NARROW_BW_40)
			oper_bw = DOT11_OPER_MODE_40MHZ;
		else if (narrow_bw == NARROW_BW_20)
			oper_bw = DOT11_OPER_MODE_20MHZ;

		if (cfg->oper_mode_enabled) {
			/* if the operational mode BW advertisement is greater
			 * than our association BW, update the operational
			 * mode BW to our required lower BW.
			 */
			if (bss_oper_bw < oper_bw) {
				oper_bw = bss_oper_bw;
			}
			oper_nss = DOT11_OPER_MODE_RXNSS(ie->mode);
		} else {
			oper_nss = wlc->stf->rxstreams;
		}
		ie->mode =
		  DOT11_OPER_MODE(
			DOT11_OPER_MODE_RXNSS_TYPE(ie->mode), oper_nss, oper_bw);
	}

	return;
}

static void
wlc_write_oper_mode_notif_ie(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg,
	dot11_oper_mode_notif_ie_t *ie)
{
	ie->mode = wlc_get_oper_mode(vhti, cfg);
}

void
wlc_vht_update_sgi_rx(wlc_vht_info_t *vhti, uint int_val)
{
	wlc_info_t *wlc = vhti->wlc;
	WL_TRACE(("wl%d: %s(%04x)\n", wlc->pub->unit, __FUNCTION__, int_val));

	vhti->vht_cap.vht_cap_info &= ~(VHT_CAP_INFO_SGI_80MHZ);
	vhti->vht_cap.vht_cap_info |= (int_val & WLC_VHT_SGI_80) ?
		VHT_CAP_INFO_SGI_80MHZ : 0;

	if (wlc->pub->up) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

static void
wlc_write_vht_cap_ie(wlc_bsscfg_t *cfg, vht_cap_ie_t *cap_ie)
{
	wlc_vht_info_t *vhti = cfg->wlc->vhti;
	wlc_info_t *wlc = vhti->wlc;

	uint32 cap;

	ASSERT(&vhti->vht_cap != cap_ie);

	BCM_REFERENCE(wlc);
	cap = vhti->vht_cap.vht_cap_info;
#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub))
		wlc_txbf_upd_virtif_bfr_bfe_cap(wlc->txbf, cfg, &cap);
#endif
	WL_TRACE(("%s: cap info=%04x\n", __FUNCTION__, cap));

	/* copy off what we can do - initted and filled out at attach */
	htol32_ua_store(cap, (uint8*)&cap_ie->vht_cap_info);

	htol16_ua_store(vhti->vht_cap.rx_mcs_map, (uint8*)&cap_ie->rx_mcs_map);
	htol16_ua_store(vhti->vht_cap.rx_max_rate, (uint8*)&cap_ie->rx_max_rate);
	htol16_ua_store(vhti->vht_cap.tx_mcs_map, (uint8*)&cap_ie->tx_mcs_map);
	htol16_ua_store(vhti->vht_cap.tx_max_rate, (uint8*)&cap_ie->tx_max_rate);

	if (BSSCFG_AP(cfg)) {
		cfg->current_bss->vht_rxmcsmap = vhti->vht_cap.rx_mcs_map;
		cfg->current_bss->vht_txmcsmap = vhti->vht_cap.tx_mcs_map;
	}
}

static void
wlc_write_vht_op_ie(wlc_bsscfg_t *cfg, vht_op_ie_t *op_ie)
{
	wlc_bss_info_t *current_bss = cfg->current_bss;
	chanspec_t chspec = current_bss->chanspec;
	uint8 width;
	uint8 chan1, chan2;

	if (CHSPEC_IS80(chspec)) {
		width = VHT_OP_CHAN_WIDTH_80;
	} else if (CHSPEC_IS160(chspec)) {
		width = VHT_OP_CHAN_WIDTH_160;
	} else if (CHSPEC_IS8080(chspec)) {
		width = VHT_OP_CHAN_WIDTH_80_80;
	} else {
		width = VHT_OP_CHAN_WIDTH_20_40;
	}

	if (cfg->oper_mode_enabled) {
		/* If Operating Mode enabled, update width accordingly (11ac 10.41) */
		if (DOT11_OPER_MODE_CHANNEL_WIDTH_20MHZ(cfg->oper_mode) ||
			DOT11_OPER_MODE_CHANNEL_WIDTH_40MHZ(cfg->oper_mode))
			width = VHT_OP_CHAN_WIDTH_20_40;
		if (DOT11_OPER_MODE_CHANNEL_WIDTH_80MHZ(cfg->oper_mode))
			width = VHT_OP_CHAN_WIDTH_80;
		if (CHSPEC_IS8080(chspec) &&
			DOT11_OPER_MODE_CHANNEL_WIDTH_8080MHZ(cfg->oper_mode))
			width = VHT_OP_CHAN_WIDTH_80_80;
		if (CHSPEC_IS160(chspec) &&
			DOT11_OPER_MODE_CHANNEL_WIDTH_160MHZ(cfg->oper_mode))
			width = VHT_OP_CHAN_WIDTH_160;
	}

	if (CHSPEC_IS8080(chspec)) {
		chan1 = wf_chspec_primary80_channel(chspec);
		chan2 = wf_chspec_secondary80_channel(chspec);
	} else {
		chan1 = CHSPEC_CHANNEL(chspec);
		chan2 = 0;
	}

	op_ie->chan_width = width;
	op_ie->chan1 = chan1;
	op_ie->chan2 = chan2;
}

static uint8 *
wlc_write_local_maximum_transmit_pwr(uint8 min_pwr, uint8* cp, int buflen)
{
	*cp = min_pwr;
	cp += sizeof(uint8);

	return cp;
}

static uint8 *
wlc_write_vht_transmit_power_envelope_ie(wlc_vht_info_t *vhti,
	chanspec_t chspec, uint8 *cp, int buflen)
{
	wlcband_t *band;
	uint8 min_pwr;
	dot11_vht_transmit_power_envelope_ie_t *vht_transmit_power_ie;
	wlc_info_t *wlc = vhti->wlc;


	vht_transmit_power_ie = (dot11_vht_transmit_power_envelope_ie_t *)cp;
	vht_transmit_power_ie->id = DOT11_MNG_VHT_TRANSMIT_POWER_ENVELOPE_ID;

	cp += sizeof(dot11_vht_transmit_power_envelope_ie_t);

	band = wlc->bandstate[CHSPEC_IS2G(chspec) ? BAND_2G_INDEX : BAND_5G_INDEX];

	wlc_phy_txpower_sromlimit(WLC_PI_BANDUNIT(wlc, band->bandunit), chspec, &min_pwr, NULL, 0);

	min_pwr /= (WLC_TXPWR_DB_FACTOR / 2);

	vht_transmit_power_ie->local_max_transmit_power_20 = min_pwr;

	if (CHSPEC_IS20(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 0;
	}
	else if (CHSPEC_IS40(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 1;
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
	}
	else if (CHSPEC_IS80(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 2;
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
	}
	else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 3;
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
	}
	else {
		WL_ERROR(("%s: wrong chanspec 0x%04x\n", __FUNCTION__, chspec));
		ASSERT(FALSE);
	}

	vht_transmit_power_ie->len =
	        (sizeof(dot11_vht_transmit_power_envelope_ie_t) - TLV_HDR_LEN) +
	        vht_transmit_power_ie->transmit_power_info;

	return cp;
}

/*
 * Returns bitmask specifying the contents format of the  Proprietary Features IE.
 * Zero if IE is not required.
 */

static uint
wlc_vht_ie_encap(wlc_vht_info_t *vhti, int band)
{
	wlc_info_t *wlc = vhti->wlc;

	if (BAND_5G(band)) {
		return (WLC_VHT_FEATURES_RATES_5G(wlc->pub) != 0) ?
			VHT_IE_ENCAP_HDR : VHT_IE_ENCAP_NONE;
	} else if (BAND_2G(band)) {
		/* On 2.4G encapsulate the VHT IEs and publish the Prop rates if enabled */
		return VHT_IE_ENCAP_ALL;
	} else {
		return VHT_IE_ENCAP_NONE;
	}
}

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
static uint
wlc_vht_tdls_calc_brcm_ie_len(void * ctx, wlc_iem_calc_data_t * data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_info_t *wlc = vhti->wlc;
	uint encap;
	uint len;
	uint8 action_field;

	if (!data->cbparm->vht)
		return 0;

	/* Only applicable for 2.4GHz */
	if (!CHSPEC_IS2G(cfg->current_bss->chanspec))
		return 0;

	if ((encap = wlc_vht_ie_encap(vhti, wlc->band->bandtype)) == VHT_IE_ENCAP_NONE)
		return 0;

	ASSERT(encap & VHT_IE_ENCAP_HDR);

	len = TLVLEN(VHT_IE_FEATURES_HDRLEN);
	if (encap & VHT_IE_ENCAP_VHTIE) {
		action_field = data->cbparm->ft->tdls.action;
		switch (action_field) {
		case TDLS_SETUP_REQ:
			len += TLVLEN(AID_IE_LEN) + TLVLEN(VHT_CAP_IE_LEN);
			break;
		case TDLS_SETUP_RESP:
			len += TLVLEN(AID_IE_LEN) + TLVLEN(VHT_CAP_IE_LEN) +
				TLVLEN(DOT11_OPER_MODE_NOTIF_IE_LEN);
			break;
		case TDLS_SETUP_CONFIRM:
			len += TLVLEN(VHT_OP_IE_LEN) + TLVLEN(DOT11_OPER_MODE_NOTIF_IE_LEN);
			break;
		default:
			len = 0;
			break;
		}
	}
	return len;
}

static int
wlc_vht_tdls_write_brcm_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	uint8 *buf = data->buf;
	uint16 len = (uint16)data->buf_len;
	uint8 *bufend = buf + len;
	uint8 *payload_start;
	uint encap;
	vht_features_ie_hdr_t vht_features_ie;

	if (!data->cbparm->vht)
		return BCME_OK;

	/* Only applicable for 2.4GHz */
	if (!CHSPEC_IS2G(cfg->current_bss->chanspec))
		return BCME_OK;

	if ((encap = wlc_vht_ie_encap(vhti, wlc->band->bandtype)) == VHT_IE_ENCAP_NONE)
		return BCME_ERROR;

	ASSERT(encap & VHT_IE_ENCAP_HDR);

	bcopy(BRCM_PROP_OUI, (void *)&vht_features_ie.oui[0], DOT11_OUI_LEN);
	vht_features_ie.type = VHT_FEATURES_IE_TYPE;
	vht_features_ie.rate_mask = WLC_VHT_FEATURES_RATES_2G(wlc->pub);

	/* Write out the Vendor specific  IE header */
	payload_start = bcm_write_tlv_safe(DOT11_MNG_VS_ID, &vht_features_ie,
		VHT_IE_FEATURES_HDRLEN, buf, BUFLEN(buf, bufend));

	/* Write out IE payload */
	if (encap & VHT_IE_ENCAP_VHTIE) {
		int caplen;
		uint8 *payload_end;
		uint8 *ptr = payload_start;
		vht_cap_ie_t vht_cap;
		vht_op_ie_t vht_op;
		dot11_oper_mode_notif_ie_t notif_ie;
		uint8 action_field = data->cbparm->ft->tdls.action;

		switch (action_field) {
		case TDLS_SETUP_REQ:
			/* AID IE */
			ptr = bcm_write_tlv_safe(DOT11_MNG_AID_ID, &cfg->AID,
				AID_IE_LEN, ptr, BUFLEN(ptr, bufend));

			/* VHT CAP IE */
			wlc_write_vht_cap_ie(cfg, &vht_cap);
			ptr = bcm_write_tlv_safe(DOT11_MNG_VHT_CAP_ID, &vht_cap,
				VHT_CAP_IE_LEN, ptr, BUFLEN(ptr, bufend));
			break;
		case TDLS_SETUP_RESP:
			/* AID IE */
			ptr = bcm_write_tlv_safe(DOT11_MNG_AID_ID, &cfg->AID,
				AID_IE_LEN, ptr, BUFLEN(ptr, bufend));

			/* VHT CAP IE */
			wlc_write_vht_cap_ie(cfg, &vht_cap);
			ptr = bcm_write_tlv_safe(DOT11_MNG_VHT_CAP_ID, &vht_cap,
				VHT_CAP_IE_LEN, ptr, BUFLEN(ptr, bufend));

			/* VHT OPER MODE NOTIF IE */
			wlc_write_oper_mode_notif_ie(vhti, cfg, &notif_ie);
			ptr = bcm_write_tlv_safe(DOT11_MNG_OPER_MODE_NOTIF_ID, &notif_ie,
				DOT11_OPER_MODE_NOTIF_IE_LEN, ptr, BUFLEN(ptr, bufend));
			break;
		case TDLS_SETUP_CONFIRM:
			/* VHT OP IE */
			wlc_write_vht_op_ie(cfg, &vht_op);
			ptr = bcm_write_tlv_safe(DOT11_MNG_VHT_OPERATION_ID, &vht_op,
				VHT_OP_IE_LEN, ptr, BUFLEN(ptr, bufend));
			/* VHT OPER MODE NOTIF IE */
			wlc_write_oper_mode_notif_ie(vhti, cfg, &notif_ie);
			ptr = bcm_write_tlv_safe(DOT11_MNG_OPER_MODE_NOTIF_ID, &notif_ie,
				DOT11_OPER_MODE_NOTIF_IE_LEN, ptr, BUFLEN(ptr, bufend));
			break;
		default:
			break;
		}

		payload_end = ptr;

		/* Add length of payload into BRCM IE header */
		caplen = BUFLEN(payload_start, payload_end);

		((bcm_tlv_t*)buf)->len += (uint8) caplen;
	}

	return BCME_OK;
}

static int
wlc_vht_tdls_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_info_t *wlc = vhti->wlc;
	vht_cap_ie_t vht_cap_ie;
	vht_op_ie_t vht_op_ie;
	dot11_oper_mode_notif_ie_t oper_mode_ie;
	uint8 *prop_tlv;
	int prop_tlv_len = 0;
	uint8 vht_ratemask = 0;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->tdls.result;
	chanspec_t chspec;

	if (data->ie == NULL)
		return BCME_OK;
	if (cfg == NULL)
		return BCME_OK;

	if ((CHSPEC_IS2G(cfg->current_bss->chanspec)) || WLC_VHT_FEATURES_RATES(wlc->pub)) {
		prop_tlv = wlc_read_vht_features_ie(vhti, data->ie, data->ie_len,
		                                    &vht_ratemask, &prop_tlv_len);
		if (prop_tlv == NULL)
			return BCME_OK;

		if (wlc_read_vht_cap_ie(vhti, prop_tlv, prop_tlv_len, &vht_cap_ie) != NULL)
			wlc_vht_parse_cap(&vht_cap_ie, bi);

		if (wlc_read_vht_op_ie(vhti, prop_tlv, prop_tlv_len, &vht_op_ie) != NULL) {
			chspec = wlc_vht_chanspec(vhti, &vht_op_ie, bi->chanspec, FALSE, FALSE);
			if (chspec != INVCHANSPEC)
				bi->chanspec = chspec;
		}
		if (wlc_read_oper_mode_notif_ie(vhti, prop_tlv, prop_tlv_len, &oper_mode_ie))
			bi->oper_mode = oper_mode_ie.mode;
	}

	return BCME_OK;
}
#endif /* WLTDLS */

/*
 * Returns the number of bytes of the Power Envelope IE including TLV header.
 * Varies depending on chanspec
 * Zero if IE is not needed.
 */
static uint
wlc_vht_ie_len_power_envelope(wlc_bsscfg_t *cfg, uint32 flags)
{
	uint ielen = 0;
	chanspec_t chspec;

	if (!(flags & VHT_IE_PWR_ENVELOPE))
		return 0;

	chspec = cfg->current_bss->chanspec;

	if (CHSPEC_IS20(chspec)) {
		ielen = 0;
	}
	else if (CHSPEC_IS40(chspec)) {
		ielen = 1;
	}
	else if (CHSPEC_IS80(chspec)) {
		ielen = 2;
	}
	else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		ielen = 3;
	}

	/* dot11_vht_transmit_power_envelope_ie_t includes the TLV header */
	ielen += (sizeof(dot11_vht_transmit_power_envelope_ie_t));

	return ielen;
}

/*
 * Returns the number of bytes of the Channel Switch Wrapper IE including TLV header.
 * Varies depending on chanspec
 * Zero if IE is not needed.
 */
static uint
wlc_vht_ie_len_csa_wrapper(wlc_bsscfg_t *cfg, uint32 flags)
{
	uint ielen = 0;
	chanspec_t chspec;

	if (!(flags & VHT_IE_CSA_WRAPPER))
		return 0;

	chspec = cfg->current_bss->chanspec;

	ielen = sizeof(dot11_vht_transmit_power_envelope_ie_t);

	/* wb_csa_ie not present in 20MHz channels */
	if (!CHSPEC_IS20(chspec)) {
		ielen += sizeof(dot11_wide_bw_chan_switch_ie_t);
	}

	/* vht transmit power envelope IE length depends on channel width,
	 * update channel wrapper IE length
	 */
	if (CHSPEC_IS40(chspec)) {
		ielen += 1;
	}
	else if (CHSPEC_IS80(chspec)) {
		ielen += 2;
	}
	else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		ielen += 3;
	}
	return ielen;
}

/*
 * Determine the length of the VHT IE components including any proprietary VHT IEs
 * Required by parts of the WLAN state machine thane wants to know the length
 * of the packet before writing it out for buffer allocation purposes
 */
static uint
wlc_vht_ie_len(wlc_bsscfg_t *cfg, int band, uint fc, uint32 flags)
{
	uint ielen = 0;

	switch (fc) {
	case FC_BEACON:
	case FC_PROBE_RESP:
		/* Fixed part, always there */
		ielen = TLVLEN(VHT_CAP_IE_LEN) + TLVLEN(VHT_OP_IE_LEN);

		/* Protocol/state dependent part */

		/* Power Envelope IE */
		if (flags & VHT_IE_PWR_ENVELOPE)
			ielen += wlc_vht_ie_len_power_envelope(cfg, flags);

		/* CSA Wrapper IE */
		if (flags & VHT_IE_CSA_WRAPPER)
			ielen += wlc_vht_ie_len_csa_wrapper(cfg, flags);

		break;

	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		ielen = TLVLEN(VHT_CAP_IE_LEN) + TLVLEN(VHT_OP_IE_LEN);
		break;

	case FC_PROBE_REQ:
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		ielen = TLVLEN(VHT_CAP_IE_LEN);
		break;
	default:
		ielen = 0;
	}

	return ielen;
}

/* Write out the IEs with the encap header */
static uint8*
wlc_vht_write_vht_elements(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg, int band,
	uint fc, uint32 flags, uint8 *buf, uint16 len)
{
	vht_cap_ie_t vht_cap;
	uint8 *bufend = buf + len;
	wlc_info_t *wlc = cfg->wlc;

	BCM_REFERENCE(wlc);

	switch (fc) {
		case FC_BEACON:
		case FC_PROBE_RESP:
			/* VHT Capability IE */
			wlc_write_vht_cap_ie(cfg, &vht_cap);

			buf = bcm_write_tlv_safe(DOT11_MNG_VHT_CAP_ID, &vht_cap,
				VHT_CAP_IE_LEN, buf, BUFLEN(buf, bufend));

			/* VHT Operation IE */
			wlc_write_vht_op_ie(cfg, &vhti->vht_op);

			buf = bcm_write_tlv_safe(DOT11_MNG_VHT_OPERATION_ID, &vhti->vht_op,
				VHT_OP_IE_LEN, buf, BUFLEN(buf, bufend));

			/* Protocol and state dependent variable part */

			/* VHT Power envelope IE */
			if (flags & VHT_IE_PWR_ENVELOPE)
				buf = wlc_write_vht_transmit_power_envelope_ie(vhti,
					cfg->current_bss->chanspec, buf, BUFLEN(buf, bufend));

			/* VHT CSA Switch Wrapper */
			if (flags & VHT_IE_CSA_WRAPPER)
				buf = wlc_csa_write_chan_switch_wrapper_ie(wlc->csa,
					cfg, buf, BUFLEN(buf, bufend));
			break;

		case FC_ASSOC_RESP:
		case FC_REASSOC_RESP:
			/* VHT Capability IE */
			wlc_write_vht_cap_ie(cfg, &vht_cap);
			buf = bcm_write_tlv_safe(DOT11_MNG_VHT_CAP_ID, &vht_cap,
				VHT_CAP_IE_LEN, buf, BUFLEN(buf, bufend));

			/* VHT Operation IE */
			wlc_write_vht_op_ie(cfg, &vhti->vht_op);
			buf = bcm_write_tlv_safe(DOT11_MNG_VHT_OPERATION_ID, &vhti->vht_op,
				VHT_OP_IE_LEN, buf, BUFLEN(buf, bufend));
			break;

		case FC_PROBE_REQ:
		case FC_ASSOC_REQ:
		case FC_REASSOC_REQ:
			/* VHT Capability IE */
			wlc_write_vht_cap_ie(cfg, &vht_cap);

			buf = bcm_write_tlv_safe(DOT11_MNG_VHT_CAP_ID, &vht_cap,
				VHT_CAP_IE_LEN, buf, BUFLEN(buf, bufend));
			break;

		default:
			break;
	}

	return buf;

}

static uint
wlc_vht_get_brcm_ie_len(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg, int band, uint fc,
	uint32 flags)
{
	uint encap;
	uint len;

	if ((encap = wlc_vht_ie_encap(vhti, band)) == VHT_IE_ENCAP_NONE)
		return 0;

	ASSERT(encap & VHT_IE_ENCAP_HDR);

	/* Check for encapsulation header */
	len = TLVLEN(VHT_IE_FEATURES_HDRLEN);
	if (encap & VHT_IE_ENCAP_VHTIE)
		len += wlc_vht_ie_len(cfg, band, fc, flags);

	return len;
}

/* Write out Proprietary VHT features IE */
static uint8 *
wlc_vht_write_vht_brcm_ie(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg, int band,
	uint fc, uint32 flags, uint8 *buf, uint16 len)
{
	uint8 *bufend = buf + len;
	uint8 *payload_start;
	uint encap;
	vht_features_ie_hdr_t vht_features_ie;
	wlc_info_t *wlc = vhti->wlc;

	if ((encap = wlc_vht_ie_encap(vhti, band)) == VHT_IE_ENCAP_NONE)
		return buf;

	ASSERT(encap & VHT_IE_ENCAP_HDR);

	/* Length check *BEFORE* we attempt the ucode template update */
	ASSERT(wlc_vht_get_brcm_ie_len(vhti, cfg, band, fc, flags) <= len);

	bcopy(BRCM_PROP_OUI, (void *)&vht_features_ie.oui[0], DOT11_OUI_LEN);
	vht_features_ie.type = VHT_FEATURES_IE_TYPE;

	if (BAND_5G(band)) {
		vht_features_ie.rate_mask = WLC_VHT_FEATURES_RATES_5G(wlc->pub);
	} else if (BAND_2G(band)) {
		/* On 2.4G encapsulate the VHT IEs and publish the Prop rates if enabled */
		vht_features_ie.rate_mask = WLC_VHT_FEATURES_RATES_2G(wlc->pub);
	}

	/* Write out the Vendor specific  IE header */
	payload_start = bcm_write_tlv_safe(DOT11_MNG_PROPR_ID, &vht_features_ie,
		VHT_IE_FEATURES_HDRLEN, buf, BUFLEN(buf, bufend));

	/* Write out IE payload */
	if (encap & VHT_IE_ENCAP_VHTIE) {
		int caplen;
		uint8 *payload_end;

		payload_end = wlc_vht_write_vht_elements(vhti, cfg, band,
			fc, flags, payload_start, BUFLEN(payload_start, bufend));

		/* Add length of payload into BRCM IE header */
		caplen = BUFLEN(payload_start, payload_end);

		((bcm_tlv_t*)buf)->len += (uint8) caplen;

		return payload_end;
	} else {
		return payload_start;
	}
}

chanspec_t
wlc_vht_chanspec(wlc_vht_info_t *vhti, vht_op_ie_t *op_ie,
	chanspec_t ht_chanspec, bool oper_mode_enab, uint8 oper_mode)
{
	chanspec_t chanspec = 0;
	vht_op_chan_width_t width = (vht_op_chan_width_t)op_ie->chan_width;
	uint8 chan1;
	uint8 chan2;
	uint8 ht_primary;
	int32 newwidth = VHT_OP_CHAN_WIDTH_80_80, bandwidth = WL_CHANSPEC_BW_20;

	if (oper_mode_enab) {
		bandwidth = wlc_vht_get_bw_from_opermode(oper_mode, op_ie->chan_width);

		switch (bandwidth) {
			case WL_CHANSPEC_BW_20:
				/* ht_chanspec can still show 40Mhz. Modify it to 20Mhz */
				ht_chanspec = wf_chspec_ctlchspec(ht_chanspec);
				newwidth = VHT_OP_CHAN_WIDTH_20_40;
				break;
			case WL_CHANSPEC_BW_40:
				newwidth = VHT_OP_CHAN_WIDTH_20_40;
				break;
			case WL_CHANSPEC_BW_80:
				newwidth = VHT_OP_CHAN_WIDTH_80;
				break;
			case WL_CHANSPEC_BW_8080:
				newwidth = VHT_OP_CHAN_WIDTH_80_80;
				break;
			case WL_CHANSPEC_BW_160:
				newwidth = VHT_OP_CHAN_WIDTH_160;
				break;
		}
		width = MIN(width, newwidth);
	}

	if (width == VHT_OP_CHAN_WIDTH_20_40) {
		chanspec = ht_chanspec;
	} else if (width == VHT_OP_CHAN_WIDTH_80) {
		chan1 = op_ie->chan1;
		ht_primary = wf_chspec_ctlchan(ht_chanspec);

		/* check which 20MHz sub-channel is the primary from the HT primary channel */
		if (!CHSPEC_IS40_UNCOND(ht_chanspec)) {

			WL_INFORM(("wl%d: %s: unexpected HT 20MHz channel %d for "
			           "VHT 80MHz channel secondary offset is wrong %d\n",
			           vhti->wlc->pub->unit, __FUNCTION__,
			           ht_primary, chan1));
		}
		chanspec = wf_chspec_80(chan1, ht_primary);

		if (chanspec == INVCHANSPEC) {
			WL_INFORM(("wl%d: %s: unexpected HT 20MHz primary channel %d for "
				           "80MHz center channel %d\n",
				           vhti->wlc->pub->unit, __FUNCTION__,
				           ht_primary, chan1));
		}
	} else if (width == VHT_OP_CHAN_WIDTH_160) {
		ht_primary = wf_chspec_ctlchan(ht_chanspec);
		chanspec = wf_channel2chspec(ht_primary, WL_CHANSPEC_BW_160);

		if (chanspec == INVCHANSPEC)
		{
			WL_TMP(("wl%d: %s: unexpected VHT 160 for channel %d ",
			           vhti->wlc->pub->unit, __FUNCTION__,
			           ht_primary));
		}
	} else if (width == VHT_OP_CHAN_WIDTH_80_80) {
		ht_primary = wf_chspec_ctlchan(ht_chanspec);

		chan1 = op_ie->chan1;
		chan2 = op_ie->chan2;

		chanspec = wf_chspec_get8080_chspec(ht_primary, chan1, chan2);
		if (chanspec == INVCHANSPEC)
		{
			WL_TMP(("wl%d: %s: unexpected VHT 80+80 for channel %d for "
			           "VHT 80+80MHz channel 1 %d channel 2 %d\n",
			           vhti->wlc->pub->unit, __FUNCTION__,
			           ht_primary, chan1, chan2));
		}
	}

	return chanspec;
}

static void
wlc_vht_upd_rate_mcsmap_ex(wlc_vht_info_t *vhti, struct scb *scb, uint16 rxmcsmap)
{
	int i;
	uint16 txmcsmap = vhti->vht_cap.tx_mcs_map;
	uint16 mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
	int nss_max = VHT_CAP_MCS_MAP_NSS_MAX;
	struct wlc_vht_scb_info *cubby_info = SCB_VHT_INFO(vhti, scb);

	if (cubby_info->oper_mode_enabled &&
		!DOT11_OPER_MODE_RXNSS_TYPE(cubby_info->oper_mode))
		nss_max = DOT11_OPER_MODE_RXNSS(cubby_info->oper_mode);

	/* initialize all mcs values over all the allowed number of
	 * operational streams to none.
	 */
	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		uint8 mcs_tx = VHT_MCS_MAP_GET_MCS_PER_SS(i, txmcsmap);
		uint8 mcs_rx = VHT_MCS_MAP_GET_MCS_PER_SS(i, rxmcsmap);
		uint8 mcs = VHT_CAP_MCS_MAP_NONE;

		if (i <= nss_max && mcs_tx != VHT_CAP_MCS_MAP_NONE &&
			mcs_rx != VHT_CAP_MCS_MAP_NONE) {
			mcs = MIN(mcs_tx, mcs_rx);
		}

		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, mcsmap);
	}
	scb->rateset.vht_mcsmap = mcsmap;
}


void
wlc_vht_upd_rate_mcsmap(wlc_vht_info_t *vhti, struct scb *scb)
{
	struct wlc_vht_scb_info *cubby_info = SCB_VHT_INFO(vhti, scb);
	uint16 rxmcsmap = cubby_info->rxmcsmap;
	wlc_vht_upd_rate_mcsmap_ex(vhti, scb, rxmcsmap);

	WL_RATE(("%s: vht:scb:rxmcsmap 0x%x txmcsmap 0x%x\n",
		__FUNCTION__,  wlc_vht_get_scb_rxmcsmap(vhti, scb),
		wlc_vht_get_tx_mcsmap(vhti)));
}

/* VHT Cap IE */
static uint
wlc_vht_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;

	if (!data->cbparm->vht)
		return 0;

	if (BAND_2G(vhti->wlc->band->bandtype))
		return 0;

	return TLV_HDR_LEN + VHT_CAP_IE_LEN;
}

static int
wlc_vht_write_cap_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	vht_cap_ie_t vht_cap;

	if (!data->cbparm->vht) {
		return BCME_OK;
	}

	if (BAND_2G(wlc->band->bandtype)) {
		return BCME_OK;
	}

	wlc_write_vht_cap_ie(cfg, &vht_cap);

	bcm_write_tlv(DOT11_MNG_VHT_CAP_ID, &vht_cap, VHT_CAP_IE_LEN, data->buf);
	return BCME_OK;
}

#ifdef AP
static int
wlc_vht_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;

	if (data->ie == NULL)
		return BCME_OK;

	if (!data->pparm->vht)
		return BCME_OK;

	switch (data->ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

			ftpparm->assocreq.vht_cap_ie = data->ie;
		}
		break;
	}

	return BCME_OK;
}
#endif /* AP */

/* VHT Operation */
static uint
wlc_vht_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;

	if (!data->cbparm->vht)
		return 0;

	if (BAND_2G(wlc->band->bandtype))
		return 0;

	return TLV_HDR_LEN + VHT_OP_IE_LEN;
}

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
static uint
wlc_vht_tdls_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;

	if (!data->cbparm->vht)
		return 0;

	if (!ftcbparm->tdls.vht_op_ie)
		return 0;

	if (BAND_2G(wlc->band->bandtype))
		return 0;

	return TLV_HDR_LEN + VHT_OP_IE_LEN;
}
#endif /* WLTDLS */

static int
wlc_vht_write_op_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if (!data->cbparm->vht) {
		return BCME_OK;
	}

	if (BAND_2G(wlc->band->bandtype)) {
		return BCME_OK;
	}

	wlc_write_vht_op_ie(cfg, &wlc->vhti->vht_op);

	bcm_write_tlv(DOT11_MNG_VHT_OPERATION_ID, &wlc->vhti->vht_op, VHT_OP_IE_LEN, data->buf);
	return BCME_OK;
}

#ifdef AP
static int
wlc_vht_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;

	if (data->ie == NULL)
		return BCME_OK;

	if (!data->pparm->vht)
		return BCME_OK;

	switch (data->ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype)) {
			wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

			ftpparm->assocreq.vht_op_ie = data->ie;
		}
		break;
	}

	return BCME_OK;
}
#endif /* AP */

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
static int
wlc_vht_tdls_parse_op_ie(void *ctx, wlc_iem_parse_data_t *parse)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = parse->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->tdls.result;
	wlc_info_t *wlc = vhti->wlc;
	vht_op_ie_t vht_op_ie;
	chanspec_t chspec;

	if (parse->ie == NULL)
		return BCME_OK;

	if (VHT_ENAB_BAND(wlc->pub, wlc->band->bandtype) &&
		wlc_read_vht_op_ie(vhti, parse->ie, parse->ie_len, &vht_op_ie) &&
		bi->chanspec) {
		chspec = wlc_vht_chanspec(vhti, &vht_op_ie, bi->chanspec, FALSE, FALSE);

		if (chspec != INVCHANSPEC) {
			bi->chanspec = chspec;
			bi->flags2 |= WLC_BSS_80MHZ;
		}
	}

	return BCME_OK;
}

#endif /* WLTDLS && !WLTDLS_DISABLED */

/* VHT Power envelope */
static uint
wlc_vht_calc_pwr_env_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	uint8 buf[257];

	/* TODO: needs a better way to calculete the IE length */

	if (!data->cbparm->vht)
		return 0;

	if (BAND_2G(wlc->band->bandtype))
		return 0;

	if (!cfg->BSS)
		return 0;

	return (uint)(wlc_write_vht_transmit_power_envelope_ie(vhti, cfg->current_bss->chanspec,
	                                                       buf, sizeof(buf)) - buf);
}

static int
wlc_vht_write_pwr_env_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;

	if (!data->cbparm->vht)
		return BCME_OK;

	if (BAND_2G(wlc->band->bandtype))
		return BCME_OK;

	if (!cfg->BSS)
		return BCME_OK;

	wlc_write_vht_transmit_power_envelope_ie(vhti, cfg->current_bss->chanspec,
	                                         data->buf, data->buf_len);
	return BCME_OK;
}

/* BRCM VHT Cap IE */
static uint
wlc_vht_calc_brcm_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	uint32 flags;

	/* TODO: needs better code here to "calculate" the IE length */

	if (!data->cbparm->vht)
		return 0;

	switch (data->ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		flags = VHT_IE_DEFAULT;
		break;
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		flags = VHT_IE_DEFAULT;
		break;
	case FC_PROBE_REQ:
		flags = VHT_IE_DEFAULT;
		break;
	case FC_BEACON:
	case FC_PROBE_RESP:
		flags = VHT_IE_DEFAULT;
		if (BSSCFG_AP(cfg)) {
			flags |= VHT_IE_PWR_ENVELOPE;
			if (WL11H_ENAB(wlc) &&
			    wlc_csa_get_csa_count(wlc->csa, cfg) != 0)
				flags |= VHT_IE_CSA_WRAPPER;
		}
		break;
	default:
		flags = 0;
		break;
	}

	return wlc_vht_get_brcm_ie_len(vhti, cfg, wlc->band->bandtype, data->ft, flags);
}

static int
wlc_vht_write_brcm_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	uint32 flags;

	if (!data->cbparm->vht)
		return BCME_OK;

	switch (data->ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		flags = VHT_IE_DEFAULT;
		break;
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		flags = VHT_IE_DEFAULT;
		break;
	case FC_PROBE_REQ:
		flags = VHT_IE_DEFAULT;
		break;
	case FC_BEACON:
	case FC_PROBE_RESP:
		flags = VHT_IE_DEFAULT;
		if (BSSCFG_AP(cfg)) {
			flags |= VHT_IE_PWR_ENVELOPE;
			if (WL11H_ENAB(wlc) &&
			    wlc_csa_get_csa_count(wlc->csa, cfg) != 0)
				flags |= VHT_IE_CSA_WRAPPER;
		}
		break;
	default:
		flags = 0;
		break;
	}

	wlc_vht_write_vht_brcm_ie(vhti, cfg, wlc->band->bandtype, data->ft, flags,
	                          data->buf, (uint16)data->buf_len);
	return BCME_OK;
}

#ifdef AP
static int
wlc_vht_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;

	if (data->ie == NULL)
		return BCME_OK;

	if (!data->pparm->vht)
		return BCME_OK;

	switch (data->ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		/*
		 * Locate  the VHT cap IE
		 * Encapsulated VHT Prop IE appears if we are running VHT in 2.4G or
		 * the extended rates are enabled
		 */
		if (BAND_2G(wlc->band->bandtype) || WLC_VHT_FEATURES_RATES(wlc->pub)) {
			wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
			uint8 *prop_tlv;
			int prop_tlv_len = 0;

			if ((prop_tlv =
			     wlc_read_vht_features_ie(vhti, data->ie, data->ie_len,
			                              &ftpparm->assocreq.vht_ratemask,
			                              &prop_tlv_len)) == NULL)
				return BCME_OK;

			ftpparm->assocreq.vht_cap_ie = (uint8 *)
			        bcm_parse_tlvs(prop_tlv, prop_tlv_len, DOT11_MNG_VHT_CAP_ID);
			ftpparm->assocreq.vht_op_ie = (uint8 *)
			        bcm_parse_tlvs(prop_tlv, prop_tlv_len, DOT11_MNG_VHT_OPERATION_ID);
		}
		break;
	}

	return BCME_OK;
}
#endif /* AP */

/* Op mode notif */
static uint
wlc_vht_calc_op_mode_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_info_t *wlc = cfg->wlc;
	bool narrow_bw = FALSE;
	bool rxnss_change = FALSE;
	if ((uint)(DOT11_OPER_MODE_RXNSS(cfg->oper_mode)) < WLC_BITSCNT(wlc->stf->hw_rxchain)) {
		rxnss_change = TRUE;
	}
	/* VHT operating on narrow bandwidth (40/20Mhz).
	 * Need to carry Oper-Mode Notification IE into Assoc-Req,
	 * beacon and probe responses to indicate current oper-mode.
	 * If there is no change in the Rxnss, then oper_mode ie will not
	 * be sitting in the beacon.
	 */
	narrow_bw = (data->cbparm->ft->assocreq.narrow_bw != NARROW_BW_NONE);
	if (data->cbparm->vht &&
	    ((cfg->oper_mode_enabled && rxnss_change) || narrow_bw ||
	    (cfg->opermode_info->state == VHT_UPGRADE_PENDING)))
		return TLV_HDR_LEN + DOT11_OPER_MODE_NOTIF_IE_LEN;

	return 0;
}

static int
wlc_vht_write_op_mode_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	bool narrow_bw = FALSE;


	/* VHT operating on narrow bandwidth (40/20Mhz).
	 * Need to carry Oper-Mode Notification IE into Assoc-Req
	 * to indicate current oper-mode.
	 */
	if ((data->ft == FC_REASSOC_REQ) || (data->ft == FC_ASSOC_REQ))
		narrow_bw = (data->cbparm->ft->assocreq.narrow_bw != NARROW_BW_NONE);
	if (data->cbparm->vht &&
	    (cfg->oper_mode_enabled || narrow_bw)) {
		dot11_oper_mode_notif_ie_t notif_ie;

		wlc_write_oper_mode_notif_ie(vhti, cfg, &notif_ie);

		/* update oper mode_notification IE to indicate current
		 * operatoin bandwidth due to narrow bandwidth.
		 */
		if (narrow_bw) {
			wlc_update_oper_mode_notif_ie(wlc, cfg,
				data->cbparm->ft->assocreq.narrow_bw, &notif_ie);
		}
		bcm_write_tlv(DOT11_MNG_OPER_MODE_NOTIF_ID, &notif_ie,
			DOT11_OPER_MODE_NOTIF_IE_LEN, data->buf);
	}
	return BCME_OK;
}

static int
wlc_vht_parse_op_mode_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	dot11_oper_mode_notif_ie_t *oper_mode;
	wlc_iem_ft_pparm_t *ftpparm;
	struct scb *scb = NULL;

	if (!data->pparm->ht && !data->pparm->vht)
		return BCME_OK;

	if (data->ie == NULL)
		return BCME_OK;

	if (data->ie[TLV_LEN_OFF] < DOT11_OPER_MODE_NOTIF_IE_LEN)
		return BCME_OK;

	oper_mode = (dot11_oper_mode_notif_ie_t *)&data->ie[TLV_BODY_OFF];

	switch (data->ft) {
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		ASSERT(data->pparm != NULL);
		ftpparm = data->pparm->ft;
		ASSERT(ftpparm != NULL);
		scb = ftpparm->assocreq.scb;
		ASSERT(scb != NULL);
		break;
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		ASSERT(data->pparm != NULL);
		ftpparm = data->pparm->ft;
		ASSERT(ftpparm != NULL);
		scb = ftpparm->assocresp.scb;
		ASSERT(scb != NULL);
		break;
	case FC_BEACON:
		ASSERT(data->pparm != NULL);
		ftpparm = data->pparm->ft;
		ASSERT(ftpparm != NULL);
		scb = ftpparm->bcn.scb;
		break;
	}

	if (scb != NULL)
		wlc_vht_update_scb_oper_mode(wlc->vhti, scb, oper_mode->mode);
	else
		WL_MODE_SWITCH(("wl%d: %s: Scb is NULL\n", WLCWLUNIT(wlc), __FUNCTION__));

	return BCME_OK;
}

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
static dot11_oper_mode_notif_ie_t *
wlc_read_oper_mode_notif_ie(wlc_vht_info_t *vhti, uint8 *tlvs, int tlvs_len,
	dot11_oper_mode_notif_ie_t *ie)
{
	bcm_tlv_t *tlv;
	wlc_info_t *wlc = vhti->wlc;

	tlv = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_OPER_MODE_NOTIF_ID);
	if (tlv) {
		if (tlv->len >= DOT11_OPER_MODE_NOTIF_IE_LEN) {
			memcpy(ie, tlv->data, DOT11_OPER_MODE_NOTIF_IE_LEN);
			return ie;
		} else {
			WL_INFORM(("wl%d: %s: Operating Mode Notification IE len"
				" %d bytes, expected %d bytes\n",
				wlc->pub->unit, __FUNCTION__, tlv->len,
				DOT11_OPER_MODE_NOTIF_IE_LEN));
			WLCNTINCR(wlc->pub->_cnt->rxbadproto);
		}
	}
	return NULL;
}

/* VHT OP Mode Notif IE */
static int
wlc_vht_tdls_parse_op_mode_ie(void *ctx, wlc_iem_parse_data_t *parse)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = parse->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->tdls.result;
	dot11_oper_mode_notif_ie_t oper_mode_ie;

	if (parse->ie == NULL)
		return BCME_OK;

	/* Get the Operating Mode IE's */
	if (wlc_read_oper_mode_notif_ie(vhti, parse->ie, parse->ie_len, &oper_mode_ie)) {
		bi->oper_mode = oper_mode_ie.mode;
	}

	return BCME_OK;
}
#endif /* WLTDLS && !WLTDLS_DISABLED */

/* This function is responsible to perform the actual upgrade/downgrade
 * of the STA once the initial protocol is followed as per the Operating Mode
 * Notification specification. This will be followed by the remaining protocol
 * to complete the Operating Mode Change.
 */
void
wlc_vht_perform_sta_upgrade_downgrade(wlc_vht_info_t *vhti,
wlc_bsscfg_t * bsscfg)
{
	wlc_info_t *wlc = vhti->wlc;
	uint8 rxnss, chains = 0, i;
	ASSERT(BSSCFG_STA(bsscfg));
	rxnss = DOT11_OPER_MODE_RXNSS(bsscfg->opermode_info->oper_mode_new);
	WL_MODE_SWITCH(("wl%d: %s: new_chspec %x old_chspec %x new_rxnss %x old_rxnss %x\n",
		wlc->pub->unit,
		__FUNCTION__,
		bsscfg->opermode_info->new_chanspec,
		bsscfg->current_bss->chanspec,
		rxnss,
		wlc->stf->op_rxstreams));
	/* Perform the NSS change if there is any change required in no of Rx streams */
	if (rxnss != wlc->stf->op_rxstreams) {
		/* Construct the Rx/Tx chains based on Rx streams */
		for (i = 0; i < rxnss; i++) {
			chains |= (1 << i);
		}
		/* Perform the change of Tx chain first, as it waits for
		 * all the Tx packets to flush out. Change of Rx chain
		 * involves sending out the MIMOOPS action frame, which
		 * can cause postponement of the Tx chain changes
		 * due to pending action frame in Tx FIFO.
		 */
		wlc_stf_txchain_set(wlc, chains, TRUE, WLC_TXCHAIN_ID_USR);
		wlc_stf_rxchain_set(wlc, chains, FALSE);
	}
	/* Perform the bandwidth switch if the current chanspec
	 * is not matching with requested chanspec in the operating
	 * mode notification
	 */
	if (bsscfg->opermode_info->new_chanspec != bsscfg->current_bss->chanspec) {
		wlc_update_bandwidth(wlc, bsscfg, bsscfg->opermode_info->new_chanspec);
	}
}

/* This function resumes the STA/AP downgrade for each bsscfg.
 * This is required if the downgrade was postponed due to pending
 * packets in the Tx FIFO
 */
void
wlc_vht_bsscfg_complete_downgrade(wlc_vht_info_t *vhti)
{
	wlc_info_t *wlc = vhti->wlc;
	uint8 i = 0;
	wlc_bsscfg_t *bsscfg;
	FOREACH_BSS(wlc, i, bsscfg) {
		if (VHT_OPER_MODE_STATE_PENDING(bsscfg)) {
			if (BSSCFG_STA(bsscfg)) {
				wlc_vht_resume_downgrade(wlc->vhti, bsscfg);
			}
			else if (BSSCFG_AP(bsscfg)) {
				wlc_vht_perform_ap_upgrade_downgrade(wlc->vhti, bsscfg,
					bsscfg->opermode_info->new_chanspec);
			}
		}
	}
}

/* This function is responsible to perform the actual upgrade/downgrade
 * of the AP once the initial protocol is followed as per the Operating Mode
 * Notification specification. This will be followed by the remaining protocol
 * to complete the Operating Mode Change.
 */
void
wlc_vht_perform_ap_upgrade_downgrade(wlc_vht_info_t *vhti,
wlc_bsscfg_t *bsscfg, chanspec_t chanspec)
{
	wlc_info_t *wlc = vhti->wlc;
	uint8 rxnss, chains = 0, i;
	ASSERT(BSSCFG_AP(bsscfg));
	rxnss = DOT11_OPER_MODE_RXNSS(bsscfg->opermode_info->oper_mode_new);
	/* Perform the bandwidth switch */
	if (chanspec != bsscfg->current_bss->chanspec) {
		wlc_update_bandwidth(wlc, bsscfg, chanspec);
	}
	/* Perform the NSS change */
	if (rxnss != wlc->stf->op_rxstreams) {
		/* Construct the Rx/Tx chains based on Rx streams */
		for (i = 0; i < rxnss; i++) {
			chains |= (1 << i);
		}
		wlc_stf_txchain_set(wlc, chains, TRUE, WLC_TXCHAIN_ID_USR);
		wlc_stf_rxchain_set(wlc, chains, FALSE);
	}
	/* Here the update in the oper_mode is done so that the upcoming beacons will
	* reflect the oper_mode change.
	*/
	bsscfg->oper_mode = wlc_vht_get_opermode_from_chspec(wlc->vhti, chanspec);
	wlc_bss_update_beacon(wlc, bsscfg);
	wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
	/* Start the Tx queue */
	wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, 0);
	bsscfg->opermode_info->state = VHT_OP_MODE_NOT_PENDING;
}

/* This functions will return the bsscfg's operating
 * mode.
 */
static uint8
wlc_get_oper_mode(wlc_vht_info_t *vhti, wlc_bsscfg_t *bsscfg)
{
	uint8 type = DOT11_OPER_MODE_RXNSS_TYPE(bsscfg->oper_mode);
	uint8 nss = DOT11_OPER_MODE_RXNSS(bsscfg->oper_mode);
	uint8 bw = DOT11_OPER_MODE_CHANNEL_WIDTH(bsscfg->oper_mode);
	wlc_info_t *wlc = vhti->wlc;

	/* Adjust oper_mode according to our capabilities */
	if (!type) {
		nss = MIN(wlc->stf->rxstreams, nss);

		if (bw == DOT11_OPER_MODE_8080MHZ && !WLC_80MHZ_CAP_PHY(wlc))
			bw = DOT11_OPER_MODE_80MHZ;
		if (bw == DOT11_OPER_MODE_160MHZ && !WLC_160MHZ_CAP_PHY(wlc))
			bw = DOT11_OPER_MODE_80MHZ;
		if (bw == DOT11_OPER_MODE_80MHZ && !WLC_80MHZ_CAP_PHY(wlc))
			bw = DOT11_OPER_MODE_40MHZ;
		if (bw == DOT11_OPER_MODE_40MHZ && !WLC_40MHZ_CAP_PHY(wlc))
			bw = DOT11_OPER_MODE_20MHZ;

		return DOT11_OPER_MODE(type, nss, bw);
	}

	/* we should not be here because we don't support nss type 1 for now */
	return bsscfg->oper_mode;
}

/* Timer handler for VHT AP downgrade */
void wlc_vht_oper_mode_timer(void *arg)
{
	wlc_bsscfg_opermode_ctx_t *ctx = (wlc_bsscfg_opermode_ctx_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)ctx->wlc;
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_find_by_ID(wlc, ctx->connID);
	/* in case bsscfg is freed before this callback is invoked */
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, arg));
		return;
	}
	WL_MODE_SWITCH(("wl%d: %s: AP downgrade timer expired..."
		"Perform the downgrade of AP. tx_pend = %d\n",
		wlc->pub->unit,
		__FUNCTION__,
		TXPKTPENDTOT(wlc)));
	bsscfg->oper_mode = bsscfg->opermode_info->oper_mode_new;
	if (!TXPKTPENDTOT(wlc)) {
		wlc_vht_perform_ap_upgrade_downgrade(wlc->vhti, bsscfg,
			bsscfg->opermode_info->new_chanspec);
	}
	else {
		wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, DATA_BLOCK_TXCHAIN);
	}
	return;
}

/* Derives the operating mode from the
 * bandwidth of the given chanspec and
 * configured rx streams inside wlc.
 */
uint8 wlc_vht_get_opermode_from_chspec(wlc_vht_info_t *vhti, chanspec_t chanspec)
{
	wlc_info_t *wlc = vhti->wlc;
	uint8 bw = 0, rxnss = 0, rxnss_type = 0;
	if (CHSPEC_IS8080(chanspec))
		bw = DOT11_OPER_MODE_8080MHZ;
	if (CHSPEC_IS160(chanspec))
		bw = DOT11_OPER_MODE_160MHZ;
	else if (CHSPEC_IS80(chanspec))
		bw = DOT11_OPER_MODE_80MHZ;
	else if (CHSPEC_IS40(chanspec))
		bw = DOT11_OPER_MODE_40MHZ;
	else if (CHSPEC_IS20(chanspec))
		bw = DOT11_OPER_MODE_20MHZ;
	else {
		ASSERT(FALSE);
	}
	rxnss =  MIN(wlc->stf->rxstreams, VHT_CAP_MCS_MAP_NSS_MAX);
	rxnss_type = FALSE; /* Currently only type 0 is supported */
	return DOT11_OPER_MODE(rxnss_type, rxnss, bw);
}

/* This function is responsible to understand the bandwidth
 * of the given operating mode and return the corresponding
 * bandwidth using chanpsec values. This is used to compare
 * the bandwidth of the operating mode and the chanspec
 */
uint32 wlc_vht_get_bw_from_opermode(uint8 oper_mode, vht_op_chan_width_t width)
{
	uint32 opermode_bw_chspec_map[] = {
		WL_CHANSPEC_BW_20,
		WL_CHANSPEC_BW_40,
		WL_CHANSPEC_BW_80,
		/* Op mode value for 80+80 and 160 is same */
		WL_CHANSPEC_BW_8080
	};
	uint32 bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	bw = opermode_bw_chspec_map[bw];
	/* Based on VHT OP IE chan width differentiate between 80+80 and 160 */
	if (bw == WL_CHANSPEC_BW_8080 && width == VHT_OP_CHAN_WIDTH_160) {
		bw = WL_CHANSPEC_BW_160;
	}
	return bw;
}

/* Returns TRUE if the new operating mode settings are lower than
 * the existing operating mode.
 */
bool wlc_vht_is_downgrade(uint8 oper_mode_old, uint8 oper_mode_new)
{
	uint8 old_bw, new_bw, old_nss, new_nss;
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_nss = DOT11_OPER_MODE_RXNSS(oper_mode_old);
	new_nss = DOT11_OPER_MODE_RXNSS(oper_mode_new);
	if ((new_bw < old_bw) || (new_nss < old_nss))
		return TRUE;
	return FALSE;
}


/* This function handles the pending downgrade after the Tx packets are sent out */
void wlc_vht_resume_downgrade(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_opermode_info_t * opmode_info;
	opmode_info = cfg->opermode_info;
	if (opmode_info == NULL)
		return;
	wlc_vht_perform_sta_upgrade_downgrade(wlc->vhti, cfg);
#ifdef STA
	wlc_set_pmstate(cfg, FALSE);
#endif
	opmode_info->PM_0_pending = TRUE;
	opmode_info->PM_1_pending = FALSE;
	return;
}

/* Handle the different states of the operating mode processing. */
void wlc_vht_pm_pending_complete(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_opermode_info_t * opmode_info;

	opmode_info = cfg->opermode_info;

	if (opmode_info == NULL)
		return;

	if ((opmode_info->state == VHT_UPGRADE_PENDING) &&
		(opmode_info->PM_1_pending == TRUE))
	{
		WL_MODE_SWITCH(("wl%d: %s Got the NULL ack for PM 1. Start actual upgrade\n",
			wlc->pub->unit,
			__FUNCTION__));
		wlc_vht_perform_sta_upgrade_downgrade(wlc->vhti, cfg);
#ifdef STA
		wlc_set_pmstate(cfg, FALSE);
#endif
		opmode_info->PM_1_pending = FALSE;
		opmode_info->PM_0_pending = TRUE;
	}
	else if ((opmode_info->state == VHT_UPGRADE_PENDING) &&
		opmode_info->PM_0_pending)
	{
		WL_MODE_SWITCH(("wl%d: %s Got NULL ack for PM 0....Send action frame \n",
			WLCWLUNIT(wlc), __FUNCTION__));
		wlc_send_action_vht_oper_mode(wlc->vhti,	cfg, &cfg->BSSID,
			opmode_info->oper_mode_new);
		opmode_info->PM_0_pending = FALSE;
	}
	else if ((opmode_info->state == VHT_DOWNGRADE_PENDING) &&
		opmode_info->PM_1_pending)
	{
		WL_MODE_SWITCH(("wl%d: %s Got the NULL ack for PM 1."
			"Start actual downgrade pend = %d\n",
			WLCWLUNIT(wlc), __FUNCTION__,	TXPKTPENDTOT(wlc)));
		opmode_info->PM_1_pending = FALSE;
		if (!TXPKTPENDTOT(wlc)) {
			wlc_vht_resume_downgrade(vhti, cfg);
		}
		else {
			wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, DATA_BLOCK_TXCHAIN);
		}
	}
	else if (opmode_info->state == VHT_DOWNGRADE_PENDING &&
		opmode_info->PM_0_pending &&
		!opmode_info->PM_1_pending)
	{
		WL_MODE_SWITCH(("wl%d: %s Got NULL ack for PM 0..\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		WL_MODE_SWITCH(("wl%d: %s Downgraded\n", WLCWLUNIT(wlc), __FUNCTION__));
		/* Start the Tx queue */
		wlc_block_datafifo(wlc, DATA_BLOCK_TXCHAIN, 0);
		opmode_info->state = VHT_OP_MODE_NOT_PENDING;
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
		cfg->oper_mode = wlc_vht_derive_opermode(wlc->vhti, cfg->current_bss->chanspec,
			wlc->stf->rxstreams);
	}
}


int wlc_vht_change_sta_oper_mode(wlc_vht_info_t *vhti, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled)
{
	wlc_info_t * wlc = vhti->wlc;
	struct scb *scb = NULL;
	wlc_bsscfg_opermode_info_t * opmode_info;

	/* Should not be here if not associated */
	ASSERT(bsscfg->associated);
	opmode_info = bsscfg->opermode_info;

	scb = wlc_scbfindband(wlc, bsscfg, &bsscfg->BSSID,
		CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));

	bsscfg->oper_mode_enabled = enabled;

	if (oper_mode == bsscfg->oper_mode)
	{
		WL_MODE_SWITCH(("wl%d: %s No change in mode required. Returning...\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		return BCME_OK;
	}

	opmode_info->oper_mode_new = oper_mode;
	opmode_info->PM_0_pending = FALSE;
	opmode_info->PM_1_pending = FALSE;

	if (enabled) {
		scb->flags3 |= SCB3_OPER_MODE_NOTIF;
		/* Downgrade */
		if (wlc_vht_is_downgrade(bsscfg->oper_mode, oper_mode))
		{
			/* send action frame */
			opmode_info->state = VHT_DOWNGRADE_PENDING;
			wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, TRUE);
			opmode_info->new_chanspec = wlc_vht_find_downgrade_chanspec(bsscfg,
				oper_mode, bsscfg->oper_mode);

			wlc_send_action_vht_oper_mode(wlc->vhti, bsscfg,
				&bsscfg->BSSID, oper_mode);
		}
		/* Upgrade */
		else
		{
			opmode_info->state = VHT_UPGRADE_PENDING;
			wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, TRUE);
			opmode_info->new_chanspec = wlc_vht_find_upgrade_chanspec(vhti,
				bsscfg, oper_mode, bsscfg->oper_mode);
			WL_MODE_SWITCH(("wl%d: %s Upgrading Started ...Setting PM state to TRUE\n",
				WLCWLUNIT(wlc), __FUNCTION__));
#ifdef STA
			wlc_set_pmstate(bsscfg, TRUE);
#endif
			opmode_info->PM_1_pending = TRUE;
		}
	}
	else
	{
		scb->flags3 &= ~(SCB3_OPER_MODE_NOTIF);
		bsscfg->oper_mode = oper_mode;
		WL_MODE_SWITCH(("wl%d: %s Op mode notif disable request enable = %d\n",
			WLCWLUNIT(wlc), __FUNCTION__, enabled));
	}
	return BCME_OK;
}

/* Returns the new chanpsec value which could be used to downgrade
 * the bsscfg.
 */
chanspec_t
wlc_vht_find_downgrade_chanspec(wlc_bsscfg_t *cfg, uint8 oper_mode_new, uint8 oper_mode_old)
{
	chanspec_t ret_chspec = 0, curr_chspec;
	uint8 new_bw, old_bw;
	curr_chspec = cfg->current_bss->chanspec;

	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);

	if (old_bw <= new_bw)
		return curr_chspec;

	if (new_bw == DOT11_OPER_MODE_8080MHZ) {
		ASSERT(FALSE);
	}
	else if (new_bw == DOT11_OPER_MODE_80MHZ)
		ret_chspec = wf_chspec_primary80_chspec(curr_chspec);
	else if (new_bw == DOT11_OPER_MODE_40MHZ)
		ret_chspec = wf_chspec_primary40_chspec(curr_chspec);
	else if (new_bw == DOT11_OPER_MODE_20MHZ)
		ret_chspec = wf_chspec_ctlchspec(curr_chspec);
	return ret_chspec;
}

/* Returns the new chanpsec value which could be used to upgrade
 * the bsscfg.
 */
chanspec_t
wlc_vht_find_upgrade_chanspec(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg,
uint8 oper_mode_new, uint8 oper_mode_old)
{

	chanspec_t curr_chspec, def_chspec;
	uint8 new_bw, old_bw, def_bw, def_opermode;

	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_new);
	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode_old);
	curr_chspec = cfg->current_bss->chanspec;
	def_chspec = cfg->max_opermode_chanspec;
	def_opermode = wlc_vht_get_opermode_from_chspec(vhti, def_chspec);
	def_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(def_opermode);

	WL_MODE_SWITCH(("wl%d: %s Current bw = %x chspec = %x"
		"new_bw = %x orig_bw = %x orig_chspec = %x\n",
		WLCWLUNIT(vhti->wlc), __FUNCTION__, old_bw,
		curr_chspec, new_bw, def_bw, def_chspec));

	/* Compare if the channel is different in max_opermode_chanspec
	 * which is required to avoid an upgrade to a different channel
	 */
	if ((old_bw >= new_bw) ||
		(old_bw >= def_bw))
			return curr_chspec;

	if (new_bw == DOT11_OPER_MODE_80MHZ) {
		if (CHSPEC_IS8080(def_chspec) || CHSPEC_IS160(def_chspec))
			return wf_chspec_primary80_chspec(def_chspec);
	}
	else if (new_bw == DOT11_OPER_MODE_40MHZ) {
		if (CHSPEC_IS8080(def_chspec) || CHSPEC_IS160(def_chspec))
			return wf_chspec_primary40_chspec(wf_chspec_primary80_chspec(def_chspec));
		else if (CHSPEC_IS80(def_chspec)) {
			return wf_chspec_primary40_chspec(def_chspec);
		}
	}
	else if (new_bw == DOT11_OPER_MODE_20MHZ) {
		ASSERT(FALSE);
	}
	return def_chspec;

}


bool
wlc_vht_change_ap_oper_mode(wlc_vht_info_t *vhti, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled)
{
	wlc_bsscfg_opermode_info_t *opmode_info;

	opmode_info = bsscfg->opermode_info;

	if (bsscfg->oper_mode == oper_mode &&
		bsscfg->oper_mode_enabled == enabled)
		return FALSE;

	bsscfg->oper_mode_enabled = enabled;

	if (!enabled)
	{
		bsscfg->oper_mode = oper_mode;
		return TRUE;
	}
	/* Upgrade */
	if (!wlc_vht_is_downgrade(bsscfg->oper_mode, oper_mode)) {

		opmode_info->new_chanspec = wlc_vht_find_upgrade_chanspec(vhti,
			bsscfg, oper_mode, bsscfg->oper_mode);

		WL_MODE_SWITCH(("wl%d: %s Got the new chanspec as %x bw = %x\n",
			WLCWLUNIT(vhti->wlc),
			__FUNCTION__,
			opmode_info->new_chanspec,
			CHSPEC_BW(opmode_info->new_chanspec)));

		opmode_info->state = VHT_UPGRADE_PENDING;
		opmode_info->oper_mode_old = bsscfg->oper_mode;
		opmode_info->oper_mode_new = oper_mode;
		wlc_vht_perform_ap_upgrade_downgrade(vhti, bsscfg,
			opmode_info->new_chanspec);
	}
	/* Downgrade */
	else {
		opmode_info->new_chanspec = wlc_vht_find_downgrade_chanspec(bsscfg,
			oper_mode, bsscfg->oper_mode);
		if (!opmode_info->new_chanspec)
			return FALSE;
		WL_MODE_SWITCH(("wl%d: %s Got the new chanspec as %x bw = %x\n",
			WLCWLUNIT(vhti->wlc),
			__FUNCTION__,
			opmode_info->new_chanspec,
			CHSPEC_BW(opmode_info->new_chanspec)));
		opmode_info->state = VHT_DOWNGRADE_PENDING;
		opmode_info->oper_mode_new = oper_mode;
		opmode_info->oper_mode_old = bsscfg->oper_mode;
		/* Here the update in the oper_mode is done so that the upcoming beacons will
		* reflect the oper_mode change. For downgrade, it happens before the actual
		* change of operating mode so that all the associated STA's (even those in PS)
		* are informed about the upcoming downgrade.
		*/
		bsscfg->oper_mode = oper_mode;
		wlc_bss_update_beacon(vhti->wlc, bsscfg);
		wlc_bss_update_probe_resp(vhti->wlc, bsscfg, TRUE);
	}
	if (opmode_info->state == VHT_UPGRADE_PENDING)
		opmode_info->state = VHT_OP_MODE_NOT_PENDING;

	return TRUE;
}

/* Handles the TBTT interrupt for AP. Required to process
 * the AP downgrade procedure, which is waiting for DTIM
 * beacon to go out before perform actual downgrade of link
 */
void wlc_vht_bss_tbtt(wlc_vht_info_t *vhti, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_opermode_info_t * opmode_info;
	opmode_info = cfg->opermode_info;
	if (cfg->oper_mode_enabled && opmode_info &&
		opmode_info->state == VHT_DOWNGRADE_PENDING &&
		!(wlc->block_datafifo & DATA_BLOCK_TXCHAIN))
	{
		uint32 val;
		/* sanity checks */
		if (opmode_info->op_ctx == NULL) {
			/* Timer context is not initialized */
			if (!(opmode_info->op_ctx =
				(wlc_bsscfg_opermode_ctx_t *)MALLOCZ(wlc->osh,
				sizeof(wlc_bsscfg_opermode_ctx_t)))) {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				goto fail;
			}
			opmode_info->op_ctx->connID = cfg->ID;
			opmode_info->op_ctx->wlc = wlc;
			/* Initialize the timer */
			opmode_info->oper_mode_timer =
				wl_init_timer(wlc->wl, wlc_vht_oper_mode_timer,
				opmode_info->op_ctx,
				"opermode_dgrade_timer");

			if (opmode_info->oper_mode_timer == NULL)
			{
				WL_ERROR(("wl%d: %s Timer alloc failed\n",
					WLCWLUNIT(wlc), __FUNCTION__));
				MFREE(wlc->osh, opmode_info->op_ctx,
					sizeof(wlc_bsscfg_opermode_ctx_t));
				goto fail;
			}
		}
		/* AP downgrade is pending */
		/* If it is the TBTT for DTIM then start the non-periodic timer */
		wlc_bmac_copyfrom_objmem(wlc->hw, (S_DOT11_DTIMCOUNT << 2),
		&val, sizeof(val), OBJADDR_SCR_SEL);
			if (!val) {
			WL_MODE_SWITCH(("wl%d: %s DTIM Tbtt..Start timer for 50 msec \n",
				WLCWLUNIT(wlc),
				__FUNCTION__));
			wl_add_timer(wlc->wl, opmode_info->oper_mode_timer,
				VHT_AP_DOWNGRADE_BACKOFF, FALSE);
		}
	}
	return;
fail:
	/* Downgrade can not be completed. Change the mode switch state
	 * so that a next attempt could be tried by upper layer
	 */
	opmode_info->state = VHT_OP_MODE_NOT_PENDING;
	return;
}


int wlc_vht_handle_oper_mode_notif_request(wlc_vht_info_t *vhti, wlc_bsscfg_t *bsscfg,
	uint8 oper_mode, uint8 enabled)
{
	wlc_info_t *wlc = vhti->wlc;
	uint8 old_bw, new_bw, old_nss, new_nss;

	if (!BSS_VHT_ENAB(wlc, bsscfg) && !BSS_N_ENAB(wlc, bsscfg))
		return BCME_UNSUPPORTED;

	if (!bsscfg->opermode_info)
		return BCME_NOMEM;

	old_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(bsscfg->oper_mode);
	new_bw = DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	old_nss = DOT11_OPER_MODE_RXNSS(bsscfg->oper_mode);
	new_nss = DOT11_OPER_MODE_RXNSS(oper_mode);

	/* For now we only support an upgrade of both Nss and BW, or a downgrade
	 * of both Nss and BW, not a combination of up/downgrade.
	 * Code currently handles an upgrade by changing the operation mode immediately and
	 * annoucing the capability, and handles a downgrade by announcing the lowwer capability,
	 * waiting for peers to act on the notification, then changing the operating mode.
	 * Handling a split up/downgrade would involve more code to change part of our
	 * operational mode immediatly, and part after a delay.
	 * Some hardware may not be able to support the intermmediate mode.
	 * The split up/downgrade is not an important use case, so avoiding the code complication.
	 */

	/* Skip handling of a combined upgrade/downgrade of Nss and BW */
	if (((new_bw < old_bw) && (new_nss > old_nss)) ||
		((new_bw > old_bw) && (new_nss < old_nss)))
		return BCME_BADARG;

	if (BSSCFG_AP(bsscfg)) {
		/* Initialize the AP operating mode if UP */
		if (enabled && !bsscfg->oper_mode_enabled && bsscfg->up)
		{
			bsscfg->max_opermode_chanspec = bsscfg->current_bss->chanspec;
			bsscfg->oper_mode_enabled = enabled;
			/* Initialize the oper_mode */
			bsscfg->oper_mode = wlc_vht_get_opermode_from_chspec(wlc->vhti,
			bsscfg->current_bss->chanspec);
		}
		if (bsscfg->up) {
			wlc_vht_change_ap_oper_mode(vhti, bsscfg, oper_mode, enabled);
		}
		else {
			bsscfg->oper_mode = oper_mode;
			bsscfg->oper_mode_enabled = enabled;
		}
	}

	if (BSSCFG_STA(bsscfg)) {
		/* Dont bother about the operating mode if not associated */
		if (bsscfg->associated) {
			if (enabled && !bsscfg->oper_mode_enabled) {
				bsscfg->oper_mode_enabled = enabled;
				bsscfg->oper_mode =
					wlc_vht_get_opermode_from_chspec(wlc->vhti,
					bsscfg->current_bss->chanspec);
			}
			wlc_vht_change_sta_oper_mode(vhti, bsscfg, oper_mode, enabled);
		}
		else {
			bsscfg->oper_mode = oper_mode;
			bsscfg->oper_mode_enabled = enabled;
		}
	}
	return BCME_OK;
}

/* Prepares the operating mode field based upon the
 * bandwidth specified in the given chanspec and rx streams
 * configured for the WLC.
 */
uint8 wlc_vht_derive_opermode(wlc_vht_info_t *vhti, chanspec_t chanspec,
	uint8 rxstreams)
{
	uint8 bw = DOT11_OPER_MODE_20MHZ, rxnss, rxnss_type;
	/* Initialize the oper_mode */
	if (CHSPEC_IS8080(chanspec))
		bw = DOT11_OPER_MODE_8080MHZ;
	else if (CHSPEC_IS160(chanspec))
		bw = DOT11_OPER_MODE_160MHZ;
	else if (CHSPEC_IS80(chanspec))
		bw = DOT11_OPER_MODE_80MHZ;
	else if (CHSPEC_IS40(chanspec))
		bw = DOT11_OPER_MODE_40MHZ;
	else if (CHSPEC_IS20(chanspec))
		bw = DOT11_OPER_MODE_20MHZ;
	else {
		ASSERT(FALSE);
	}
	rxnss =  MIN(rxstreams, VHT_CAP_MCS_MAP_NSS_MAX);
	rxnss_type = FALSE; /* Currently only type 0 is supported */
	return DOT11_OPER_MODE(rxnss_type, rxnss, bw);
}

static void
wlc_send_action_vht_oper_mode_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_bsscfg_opermode_ctx_t *ctx;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb = NULL;
	wlc_bsscfg_opermode_info_t * opmode_info;

	opmode_info = (wlc_bsscfg_opermode_info_t*)arg;
	ctx = opmode_info->op_ctx;
	bsscfg = wlc_bsscfg_find_by_ID(wlc, ctx->connID);
	/* in case bsscfg is freed before this callback is invoked */
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, arg));
		wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
		/* Sync up the SW and MAC wake states */
		return;
	}
	opmode_info = bsscfg->opermode_info;
	if (BSSCFG_STA(bsscfg)) {
		/* make sure the scb still exists */
		if ((scb = WLPKTTAGSCBGET(ctx->pkt)) == NULL) {
			WL_ERROR(("wl%d: %s: unable to find scb from the pkt %p\n",
			          wlc->pub->unit, __FUNCTION__, ctx->pkt));
			wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
			opmode_info->state = VHT_OP_MODE_NOT_PENDING;
			return;
		}

		/* ack received */
		if (txstatus & TX_STATUS_ACK_RCV) {
			/* Perform the downgrade and report success */
			WL_MODE_SWITCH(("wl%d: %s Action Frame Ack received \n",
			WLCWLUNIT(wlc),	__FUNCTION__));
			if (opmode_info->state == VHT_DOWNGRADE_PENDING) {
				WL_MODE_SWITCH(("wl%d: %s Setting PM to TRUE for downgrade \n",
					WLCWLUNIT(wlc), __FUNCTION__));
				opmode_info->PM_1_pending = TRUE;
#ifdef STA
				wlc_set_pmstate(bsscfg, TRUE);
#endif
			}
			else if (opmode_info->state == VHT_UPGRADE_PENDING)
			{
				bsscfg->oper_mode = bsscfg->opermode_info->oper_mode_new;
				WL_MODE_SWITCH(("wl%d: %s Upgraded\n",
					WLCWLUNIT(wlc),	__FUNCTION__));
				opmode_info->state = VHT_OP_MODE_NOT_PENDING;
				wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
				WL_MODE_SWITCH(("wl%d: %s Upgraded oper mode is %x chanspec = %x\n",
					WLCWLUNIT(wlc),
					__FUNCTION__,
					bsscfg->oper_mode,
					bsscfg->current_bss->chanspec));
			}
		}
		else
		{
			/* Needs handling ?????? */
			opmode_info->state = VHT_OP_MODE_NOT_PENDING;
			wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_VHT, FALSE);
		}
	}

	MFREE(wlc->osh, ctx, sizeof(wlc_bsscfg_opermode_ctx_t));
	opmode_info->op_ctx = NULL;
}

/* Prepare and send the operating mode notification action frame
 * Registers a callback to handle the acknowledgement
 */
void
wlc_send_action_vht_oper_mode(wlc_vht_info_t *vhti, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *ea, uint8 oper_mode_new) {
	struct scb *scb = NULL;
	void *p;
	uint8 *pbody;
	uint body_len;
	struct dot11_action_vht_oper_mode *ahdr;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bsscfg_opermode_info_t *opmode_info;

	ASSERT(bsscfg != NULL);

	opmode_info = bsscfg->opermode_info;

	if (BSSCFG_STA(bsscfg)) {
		scb = wlc_scbfindband(wlc, bsscfg, ea,
			CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));

		if (!(SCB_OPER_MODE_NOTIF_CAP(scb)))
		{
			WL_ERROR(("No capability for opermode switch\n"));
			return;
		}
	}

	body_len = sizeof(struct dot11_action_vht_oper_mode);

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, body_len, &pbody);

	if (p == NULL)
	{
		WL_ERROR(("Unable to allocate the mgmt frame \n"));
		return;
	}

	if ((opmode_info->op_ctx = ((wlc_bsscfg_opermode_ctx_t *)MALLOCZ(vhti->wlc->osh,
		sizeof(wlc_bsscfg_opermode_ctx_t)))) == NULL) {
		WL_ERROR(("Inside %s....cant allocate context \n", __FUNCTION__));
		return;
	}

	ahdr = (struct dot11_action_vht_oper_mode *)pbody;
	ahdr->category = DOT11_ACTION_CAT_VHT;
	ahdr->action = DOT11_VHT_ACTION_OPER_MODE_NOTIF;
	ahdr->mode = oper_mode_new;
	opmode_info->oper_mode_new = oper_mode_new;
	opmode_info->op_ctx->pkt = p;
	opmode_info->op_ctx->connID = bsscfg->ID;

	if (BSSCFG_STA(bsscfg)) {
		wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
		WL_MODE_SWITCH(("wl%d: %s Registering the action frame callback %d %d\n",
			WLCWLUNIT(wlc), __FUNCTION__,
			opmode_info->PM_1_pending, opmode_info->PM_0_pending));
		wlc_pcb_fn_register(wlc->pcb,
			wlc_send_action_vht_oper_mode_complete, (void*)opmode_info, p);
	}
	return;
}

/* Handle the reception of the Operating Mode Notification frame */
void
wlc_frameaction_vht(wlc_vht_info_t *vhti, uint action_id, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	uint8 oper_mode;
	wlc_info_t *wlc = vhti->wlc;

	BCM_REFERENCE(wlc);

	if (scb == NULL)
		return;

	switch (action_id) {
	case DOT11_VHT_ACTION_OPER_MODE_NOTIF:
		WL_MODE_SWITCH(("wl%d: %s Received op mode notif from a station\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		if (body_len < (int)sizeof(struct dot11_action_vht_oper_mode)) {
			WL_MODE_SWITCH(("wl%d: %s: VHT oper mode action frame too small",
				WLCWLUNIT(wlc), __FUNCTION__));
			WLCNTINCR(wlc->pub->_cnt->rxbadproto);
			break;
		}

		oper_mode = ((struct dot11_action_vht_oper_mode *)body)->mode;
		wlc_vht_set_scb_opermode_enab(vhti, scb, TRUE);
		wlc_vht_update_scb_oper_mode(vhti, scb, oper_mode);
		break;
	case DOT11_VHT_ACTION_CBF:
	case DOT11_VHT_ACTION_GID_MGMT:
	default:
		WL_MODE_SWITCH(("wl%d: %s: Ignoring unsupported VHT action, id: %d\n",
			WLCWLUNIT(wlc), __FUNCTION__, action_id));
		break;
	}
}

static void
wlc_vht_parse_cap(vht_cap_ie_t *cap, wlc_bss_info_t *bi)
{
	if (cap != NULL) {
		uint32 vht_cap = load32_ua(&cap->vht_cap_info);

		bi->vht_capabilities = vht_cap;
		bi->vht_rxmcsmap = cap->rx_mcs_map;
		bi->vht_txmcsmap = cap->tx_mcs_map;

		/* Mark the BSS as VHT capable */
		bi->flags2 |= WLC_BSS_VHT;

		/* Set SGI flags */
		if (vht_cap & VHT_CAP_INFO_SGI_80MHZ)
			bi->flags2 |= WLC_BSS_SGI_80;

		if (vht_cap & VHT_CAP_INFO_SGI_160MHZ)
			bi->flags2 |= WLC_BSS_SGI_160;

		/* copy the raw mcs set into the bss rateset struct */
		bi->rateset.vht_mcsmap = bi->vht_rxmcsmap;
	}
}

static int
wlc_vht_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	vht_cap_ie_t cap;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

	if (data->ie == NULL)
		return BCME_OK;

	if (wlc_read_vht_cap_ie(vhti, data->ie, data->ie_len, &cap) != NULL)
		wlc_vht_parse_cap(&cap, ftpparm->scan.result);

	return BCME_OK;
}

#if defined(WLTDLS) && !defined(WLTDLS_DISABLED)
static int
wlc_vht_tdls_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	vht_cap_ie_t cap;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

	if (data->ie == NULL)
		return BCME_OK;

	if (wlc_read_vht_cap_ie(vhti, data->ie, data->ie_len, &cap) != NULL)
		wlc_vht_parse_cap(&cap, ftpparm->tdls.result);

	return BCME_OK;
}
#endif /* WLTDLS  */

static int
wlc_vht_scan_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	vht_cap_ie_t cap;
	uint8 *prop_tlv;
	int prop_tlv_len = 0;
	uint8 vht_ratemask = 0;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;

	uint beacon_band = CHANNEL_BAND(wlc, data->pparm->ft->scan.chan);

	if (data->ie == NULL)
		return BCME_OK;

	if (BAND_2G(beacon_band) || WLC_VHT_FEATURES_RATES(wlc->pub)) {
		prop_tlv = wlc_read_vht_features_ie(vhti, data->ie, data->ie_len,
		                                    &vht_ratemask, &prop_tlv_len);
		if (prop_tlv == NULL)
			return BCME_OK;

		if (wlc_read_vht_cap_ie(vhti, prop_tlv, prop_tlv_len, &cap) != NULL)
			wlc_vht_parse_cap(&cap, ftpparm->scan.result);
	}

	return BCME_OK;
}

static int
wlc_vht_scan_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	vht_op_ie_t *op_p;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;
	wlc_bsscfg_t *cfg = data->cfg;
	vht_op_ie_t op_ie;

	chanspec_t cspec;

	if (data->ie == NULL)
		return BCME_OK;

	if ((op_p = wlc_read_vht_op_ie(vhti, data->ie, data->ie_len, &op_ie)) != NULL) {

		/* determine the chanspec from VHT Operational IE */
		if (cfg && cfg->oper_mode_enabled)
			cspec = wlc_vht_chanspec(vhti, op_p, bi->chanspec,
			cfg->oper_mode_enabled, cfg->oper_mode);
		else
			cspec = wlc_vht_chanspec(vhti, op_p, bi->chanspec, FALSE, FALSE);

		if (cspec == INVCHANSPEC) {
			if (!wlc_read_ht_add_ie(vhti->wlc, data->buf, data->buf_len)) {
				/* HT OP IE missing, hobble along: work with broken APs */
				WL_INFORM(("wl%d: %s: missing ht op ie\n",
					vhti->wlc->pub->unit, __FUNCTION__));
				/* don't return error: use defaults; work with broken APs */
				/* leave bssinfo->chanspec alone */
			} else {
				WL_INFORM(("wl%d: %s: _wlc_vht_chanspec failed\n",
					vhti->wlc->pub->unit, __FUNCTION__));
				return BCME_ERROR;
			}
		} else {
			bi->chanspec = cspec;
		}

		/* Set 80MHZ employed bit on bss */
		if (CHSPEC_IS80(bi->chanspec))
			bi->flags2 |= WLC_BSS_80MHZ;
		else if (CHSPEC_IS8080(bi->chanspec))
			bi->flags2 |= WLC_BSS_8080MHZ;
		else if (CHSPEC_IS160(bi->chanspec))
			bi->flags2 |= WLC_BSS_160MHZ;
	}

	return BCME_OK;
}

#ifdef AP
/* VHT Power Envelop IE in CS Wrapper IE */
static uint
wlc_vht_csw_calc_pwr_env_ie_len(void *ctx, wlc_iem_calc_data_t *calc)
{
	wlc_iem_ft_cbparm_t *ftcbparm = calc->cbparm->ft;
	chanspec_t chspec = ftcbparm->csw.chspec;
	uint len = sizeof(dot11_vht_transmit_power_envelope_ie_t);

	/* vht transmit power envelope IE length depends on channel width,
	 * update channel wrapper IE length
	 */
	if (CHSPEC_IS40(chspec)) {
		len += 1;
	}
	else if (CHSPEC_IS80(chspec)) {
		len += 2;
	}
	else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		len += 3;
	}

	return len;
}

static int
wlc_vht_csw_write_pwr_env_ie(void *ctx, wlc_iem_build_data_t *build)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_iem_ft_cbparm_t *ftcbparm = build->cbparm->ft;
	chanspec_t chspec = ftcbparm->csw.chspec;

	wlc_write_vht_transmit_power_envelope_ie(vhti, chspec, build->buf, build->buf_len);

	return BCME_OK;
}
#endif /* AP */

static int
wlc_vht_up(void *ctx)
{
	/* Add the vht mcs rates to the default and hw ratesets
	 * These are superset HW default caps plus whatever the user has configured
	 */
	uint16 mcsmap = 0;
	wlc_vht_info_t *vhti = (wlc_vht_info_t *)ctx;
	wlc_info_t *wlc = vhti->wlc;
	wlc_bss_info_t *bss = wlc->default_bss;

	wlc_rateset_vhtmcs_build(&bss->rateset, wlc->stf->op_rxstreams);
	mcsmap = wlc_rateset_filter_mcsmap(bss->rateset.vht_mcsmap,
		wlc->stf->txstreams, (WLC_VHT_FEATURES_MCS_GET(wlc->pub)));
	/* Note there is no explicit zeroing out of wlc->vht-cap
	 * to prevent code that shares the structure from falling over e.g. P2P
	 * This means we have to rely on the VHT_ENAB() and VHT_ENAB_BAND() macros to steer
	 * code execution correctly
	 */
	vhti->vht_cap.tx_mcs_map = mcsmap;
	vhti->vht_cap.rx_mcs_map = mcsmap;
	bss->rateset.vht_mcsmap = mcsmap;

	return BCME_OK;
}

static int
wlc_vht_set_mode(wlc_vht_info_t *vhti, bool enable)
{
	uint i;
	int idx;
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_bsscfg_t *bsscfg;
	wlc_info_t *wlc = vhti->wlc;


	if (enable) {
		ASSERT(WLC_PHY_VHT_CAP(wlc->band));
		wlc_set_gmode(wlc, GMODE_AUTO, TRUE);
		wlc_set_nmode(wlc->hti, AUTO);

		/* update WLC_BSS_VHT flag */
		if ((wlc->band->bandtype == WLC_BAND_5G) ||
			WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_2G)) {
			wlc->default_bss->flags2 |= WLC_BSS_VHT;
		} else {
			wlc->default_bss->flags2 &= ~WLC_BSS_VHT;
		}

		/* Init per band rateset */
		for (i = 0; i < NBANDS(wlc); i++) {
			if ((wlc->bandstate[i]->bandtype == WLC_BAND_5G) ||
				WLC_VHT_FEATURES_GET(wlc->pub, WL_VHT_FEATURES_2G)) {
				wlc_rateset_vhtmcs_build(&wlc->bandstate[i]->defrateset,
					wlc->stf->op_rxstreams);
				wlc->bandstate[i]->hw_rateset.vht_mcsmap =
				wlc->bandstate[i]->defrateset.vht_mcsmap;
			} else {
				wlc->bandstate[i]->defrateset.vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
				wlc->bandstate[i]->hw_rateset.vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
			}
			/* Init VHT rates/features called in the up path wlc_vht_features_init() */
		}
#ifdef WLAMPDU
		if (!VHT_ENAB(wlc->pub)) {
			/* Put back the default rx_factor (VHT) when we */
			/* move from non-VHT mode to VHT mode */
			wlc_ampdu_update_rx_factor(wlc, enable);
		}
#endif
	} else {
		wlc->default_bss->flags2 &= ~WLC_BSS_VHT;

		/* delete the mcs rates from the default and hw ratesets */
		wlc->default_bss->rateset.vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
		for (i = 0; i < NBANDS(wlc); i++) {
			wlc->bandstate[i]->defrateset.vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
			wlc->bandstate[i]->hw_rateset.vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
			/* set rspec_override to 0 if it is not 0 for the current band */
			if (RSPEC_ISVHT(wlc->band->rspec_override)) {
				wlc->bandstate[i]->rspec_override = 0;
				wlc_reprate_init(wlc);
			}
			/* set mrspec_override to 0 if it is not 0 for the current band */
			if (RSPEC_ISVHT(wlc->band->mrspec_override)) {
				wlc->bandstate[i]->mrspec_override = 0;
			}
		}

		/* clear VHT related state for SCBs */
		FOREACH_BSS(wlc, idx, bsscfg) {
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				wlc_vht_update_scb_state(vhti, wlc->band->bandtype,
					scb, NULL, NULL, NULL, 0);
			}
		}
#ifdef WLAMPDU
		if (VHT_ENAB(wlc->pub)) {
			/* Put back the default rx_factor (VHT) when we */
			/* move from non-VHT mode to VHT mode */
			wlc_ampdu_update_rx_factor(wlc, enable);
		}
#endif

	}

	return 0;
}

static int
wlc_vht_setup_mode(wlc_vht_info_t *vhti, bool enable)
{
	int err;
	wlc_info_t *wlc = vhti->wlc;

	if (enable) {
		err = (WLC_PHY_VHT_CAP(wlc->band)) ? 0 : BCME_UNSUPPORTED;
		if (err)
			return err;

		/* do not update our current operating vhtmode if country regulations
		 * force vhtmode off
		*/
		if (wlc_channel_locale_flags(wlc->cmi) & WLC_NO_MIMO) {
			return 0;
		}
	}

	err = wlc_vht_set_mode(vhti, enable);
	/* wlc_set_vhtmode() should only return an error if the call
	 * to wlc_vhtmode_validate() above returned an error. Make an
	 * assertion that the two functions remain in sync.
	 */
	ASSERT(!err);
	if (!err) {
		wlc->pub->_vht_enab = enable;
	};

	return err;
}

/*
* Similar to wlc_ht_update_scbstate
* vht State needed for scbs: for now store it all
* set the mtu pref according to size
*/
void
wlc_vht_update_scb_state(wlc_vht_info_t *vhti, int band, struct scb *scb,
	ht_cap_ie_t *cap_ie, vht_cap_ie_t *vht_cap_ie, vht_op_ie_t *vht_op_ie, uint8 vht_ratemask)
{
	wlc_rateset_t new_rateset;
	bool reinit_ratesel = FALSE;
	bool vht_cap = SCB_VHT_CAP(scb);
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	uint16 vht_flags;
	uint32 scb_vht_flags;
	wlc_info_t *wlc = vhti->wlc;

	vht_flags = cubby_info->flags;
	scb_vht_flags = scb->flags3;

	/* clear the vht capabilities first and then set
	 * them as per vht_cap_ie info
	 */
	scb->flags2 &= ~(SCB2_VHTCAP);

	cubby_info->flags &= ~(SCB_VHT_LDPCCAP | SCB_VHT_TX_STBCCAP |
	                    SCB_VHT_RX_STBCCAP | SCB_SGI80 | SCB_SGI160 |
	                    SCB_SU_BEAMFORMER | SCB_SU_BEAMFORMEE |
	                    SCB_MU_BEAMFORMER | SCB_MU_BEAMFORMEE);

	/* Clear VHT operation flag */
	if (vht_op_ie) {
		scb->flags3 &= ~(SCB3_IS_160|SCB3_IS_80_80);
	}

	/* Clear VHT capabilities  if the current band does not
	 * allow VHT or there is no vht cap IE
	 */
	if (!VHT_ENAB_BAND(wlc->pub, band) || vht_cap_ie == NULL) {
		/* clear the vht_mcsmap in scb and scb->rateset */
		cubby_info->rxmcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
		scb->rateset.vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;

		/* Clear Proprietary VHT ratemask */
		cubby_info->ratemask = 0;

		/* Clear VHT 160 and 80+80 operation flags */
		scb->flags3 &= ~(SCB3_IS_160|SCB3_IS_80_80);

		/* if vhtcap was TRUE before, reinit ratesel */
		if (vht_cap)
			wlc_scb_ratesel_init(wlc, scb);
		WL_TRACE(("Skipping vht upd: cap=%p op=%p\n", vht_cap_ie, vht_op_ie));
		return;
	}
	/* Update the intersection of the SCB Proprietary ratemask */
	vht_ratemask = vht_ratemask & (BAND_2G(band) ?
		WLC_VHT_FEATURES_RATES_2G(wlc->pub) : WLC_VHT_FEATURES_RATES_5G(wlc->pub));

	/* selectively fill in scb vht field, expand if needed */
	scb->flags2 |= (SCB2_VHTCAP);

	/* set the scb bandwith based on VHT operation IE */
	if (vht_op_ie && vht_op_ie->chan_width == VHT_OP_CHAN_WIDTH_160) {
		scb->flags3 |= SCB3_IS_160;
		/* If there is a change in operation ratesel reinit has to be done */
	    if (!(scb_vht_flags & SCB3_IS_160)) {
			reinit_ratesel = TRUE;
	    }
	}

	if (vht_op_ie && vht_op_ie->chan_width == VHT_OP_CHAN_WIDTH_80_80) {
		scb->flags3 |= SCB3_IS_80_80;
		/* If there is a change in operation ratesel reinit has to be done */
		if (!(scb_vht_flags & SCB3_IS_80_80)) {
			reinit_ratesel = TRUE;
	    }
	}

	/* Merge incoming rate with MCS rate of this device and store
		temporarily in new_rateset.mcs[]
	*/
	new_rateset.vht_mcsmap = wlc_rateset_filter_mcsmap(
				vht_cap_ie->rx_mcs_map,
				wlc->stf->txstreams,
				WLC_VHT_FEATURES_MCS_GET(wlc->pub));

	if (cubby_info->rxmcsmap != new_rateset.vht_mcsmap) {
		wlc_vht_upd_rate_mcsmap_ex(wlc->vhti, scb, new_rateset.vht_mcsmap);
		cubby_info->rxmcsmap = new_rateset.vht_mcsmap;
		reinit_ratesel = TRUE;
	}

#ifdef WLAMSDU_TX
	if (AMSDU_TX_ENAB(wlc->pub)) {
		uint16 old_mtu = cubby_info->amsdu_mtu_pref;

		scb->flags |= SCB_AMSDUCAP;

		switch ((vht_cap_ie->vht_cap_info & VHT_CAP_INFO_MAX_MPDU_LEN_MASK)) {
			case VHT_CAP_MPDU_MAX_11K:
				cubby_info->amsdu_mtu_pref = VHT_MAX_MPDU;
			break;
			case VHT_CAP_MPDU_MAX_8K:
				cubby_info->amsdu_mtu_pref = HT_MAX_AMSDU;
			break;
			case VHT_CAP_MPDU_MAX_4K:
				cubby_info->amsdu_mtu_pref = HT_MIN_AMSDU;
			break;
		}
		/* if mtu changed, update AMSDU agg bytes */
		if (old_mtu != cubby_info->amsdu_mtu_pref) {
			wlc_amsdu_scb_agglimit_upd(wlc->ami, scb);
		}
	}
#endif /* WLAMSDU_TX */

	/* record stbc + ldpc setting for this scb for vht */
	if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_LDPC) {
		cubby_info->flags |= SCB_VHT_LDPCCAP;
	}

	if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_TX_STBC) {
		cubby_info->flags |= SCB_VHT_TX_STBCCAP;
	}

	if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_SGI_80MHZ) {
		cubby_info->flags |= SCB_SGI80;
	}

	if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_SGI_160MHZ) {
		cubby_info->flags |= SCB_SGI160;
	}

	/* set reinit_ratesel to TRUE if SGI_80 or LDPCCAP flag has changed */
	if (((vht_flags & SCB_SGI80) != (cubby_info->flags & SCB_SGI80)) ||
	    ((vht_flags & SCB_SGI160) != (cubby_info->flags & SCB_SGI160)) ||
	    ((vht_flags & SCB_VHT_LDPCCAP) != (cubby_info->flags & SCB_VHT_LDPCCAP)) ||
	    (vht_ratemask != cubby_info->ratemask)) {
		reinit_ratesel = TRUE;
	}

	/* Rx STBC stream capability */
	cubby_info->stbc_num = (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_RX_STBC_MASK)
		>> VHT_CAP_INFO_RX_STBC_SHIFT;
	if (cubby_info->stbc_num > 4) {
		/* Rx STBC value > 5 is reserved */
		cubby_info->stbc_num = 0;
	}
	/* flag Rx STBC if STA supports any number of spatial streams */
	if (cubby_info->stbc_num > 0) {
		cubby_info->flags |= SCB_VHT_RX_STBCCAP;
	}

	/* beamforming caps */
#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub)) {
		if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_SU_BEAMFMR) {
			 cubby_info->flags |= SCB_SU_BEAMFORMER;
		}

		if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_MU_BEAMFMR) {
			 cubby_info->flags |= SCB_MU_BEAMFORMER;
		}

		if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_SU_BEAMFMEE) {
			 cubby_info->flags |= SCB_SU_BEAMFORMEE;
		}

		if (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_MU_BEAMFMEE) {
			 cubby_info->flags |= SCB_MU_BEAMFORMEE;
		}
		wlc_txbf_update_vht_cap(wlc->txbf, scb, vht_cap_ie->vht_cap_info);
	}
#endif /*  WL_BEAMFORMING */

	/* Maximum A-MPDU length exponent */
	cubby_info->ampdu_max_exp = (vht_cap_ie->vht_cap_info & VHT_CAP_INFO_AMPDU_MAXLEN_EXP_MASK)
		>> VHT_CAP_INFO_AMPDU_MAXLEN_EXP_SHIFT;

	/* reinit retesetl if rxmcsmap, sgi, ldpc  changed */
	if (reinit_ratesel) {
		/* Skip setting the scb ratesel for vht if tx_nss overide is set */
		if (wlc->stf->txstream_value == 0) {
			cubby_info->ratemask = vht_ratemask;
			wlc_scb_ratesel_init(wlc, scb);
		}
	}
}

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_PRHDRS) || \
	defined(WLMSG_PRPKT) || defined(WLMSG_ASSOC) || defined(DHD_DEBUG)
static int
wlc_vht_dump_cap(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_vht_info_t *vhti = wlc->vhti;
	uint32 vht_cap_info = vhti->vht_cap.vht_cap_info;
	uint32 vht_cap_flags;
	uint32 sup_chan_width;
	uint32 max_mpdu_field, max_mpdu;
	uint32 rx_stbc;
	uint32 max_ampdu_exp;
	const char *sup_chan_width_str;
	char flagstr[100];

	const bcm_bit_desc_t attr_flags[] = {
		{VHT_CAP_INFO_LDPC, "ldpc"},
		{VHT_CAP_INFO_SGI_80MHZ, "sgi80MHz"},
		{VHT_CAP_INFO_SGI_160MHZ, "sgi160MHz"},
		{VHT_CAP_INFO_TX_STBC, "tx_stbc"},
		{VHT_CAP_INFO_SU_BEAMFMR, "bmfrmr"},
		{VHT_CAP_INFO_SU_BEAMFMEE, "bmfrmee"},
		{VHT_CAP_INFO_MU_BEAMFMR, "mu_bmfrmr"},
		{VHT_CAP_INFO_MU_BEAMFMEE, "mu_bmfrmee"},
		{VHT_CAP_INFO_TXOPPS, "txopps"},
		{VHT_CAP_INFO_HTCVHT, "vht_htcap"},
		{0, NULL}
	};

	bcm_bprintf(b, "VHT dump:\n");
	bcm_bprintf(b, "VHT Cap 0x%08x\n", vht_cap_info);

	/* extract just the flags field to format */
	vht_cap_flags = vht_cap_info;
	vht_cap_flags &= ~(VHT_CAP_INFO_MAX_MPDU_LEN_MASK | VHT_CAP_INFO_SUPP_CHAN_WIDTH_MASK);
	vht_cap_flags &= ~(VHT_CAP_INFO_RX_STBC_MASK | VHT_CAP_INFO_AMPDU_MAXLEN_EXP_MASK);
	vht_cap_flags &= ~(VHT_CAP_INFO_LINK_ADAPT_CAP_MASK);

	bcm_format_flags(attr_flags, vht_cap_flags, flagstr, 100);
	bcm_bprintf(b, "Flags: %s\n", flagstr);

	max_mpdu_field = (vht_cap_info & VHT_CAP_INFO_MAX_MPDU_LEN_MASK);

	switch (max_mpdu_field) {
	case VHT_CAP_MPDU_MAX_4K:
		max_mpdu = VHT_MPDU_LIMIT_4K;
		break;
	case VHT_CAP_MPDU_MAX_8K:
		max_mpdu = VHT_MPDU_LIMIT_8K;
		break;
	case VHT_CAP_MPDU_MAX_11K:
		max_mpdu = VHT_MPDU_LIMIT_11K;
		break;
	default:
		max_mpdu = 0;
		break;
	}

	if (max_mpdu > 0) {
		bcm_bprintf(b, "Max MPDU=%d\n", max_mpdu);
	} else {
		bcm_bprintf(b, "Max MPDU invalid value\n");
	}

	sup_chan_width = (vht_cap_info & VHT_CAP_INFO_SUPP_CHAN_WIDTH_MASK);

	if (sup_chan_width == VHT_CAP_CHAN_WIDTH_SUPPORT_MANDATORY) {
		sup_chan_width_str = "20/40/80";
	} else if (sup_chan_width == VHT_CAP_CHAN_WIDTH_SUPPORT_160) {
		sup_chan_width_str = "20/40/80 plus 160";
	} else if (sup_chan_width == VHT_CAP_CHAN_WIDTH_SUPPORT_160_8080) {
		sup_chan_width_str = "20/40/80 plus 160, 80+80";
	} else {
		sup_chan_width_str = "<invalid>";
	}

	bcm_bprintf(b, "chan width: %s\n", sup_chan_width_str);

	rx_stbc = (vht_cap_info & VHT_CAP_INFO_RX_STBC_MASK) >> VHT_CAP_INFO_RX_STBC_SHIFT;

	if (rx_stbc <= 4) {
		bcm_bprintf(b, "Rx STBC: %d spatial streams\n", rx_stbc);
	} else {
		bcm_bprintf(b, "Rx STBC: %d <invalid value>\n", rx_stbc);
	}

	bcm_bprintf(b, "max bmfrmr antenas=%d\n",
		((vht_cap_info & VHT_CAP_INFO_NUM_BMFMR_ANT_MASK)
		>> VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT) + 1);

	bcm_bprintf(b, "sounding dims=%d\n",
		((vht_cap_info & VHT_CAP_INFO_NUM_SOUNDING_DIM_MASK)
		>> VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT) + 1);

	max_ampdu_exp = ((vht_cap_info & VHT_CAP_INFO_AMPDU_MAXLEN_EXP_MASK) >>
	                 VHT_CAP_INFO_AMPDU_MAXLEN_EXP_SHIFT);

	bcm_bprintf(b, "max ampdu len exp: %d (2^%d) (%d bytes) \n",
	            max_ampdu_exp, max_ampdu_exp + 13, (1 << (13 + max_ampdu_exp)) - 1);

	bcm_bprintf(b, "link adapt=");

	switch ((vht_cap_info & VHT_CAP_INFO_LINK_ADAPT_CAP_MASK) >>
		VHT_CAP_INFO_LINK_ADAPT_CAP_SHIFT) {
			case 0:
					bcm_bprintf(b, "no feedback");
			break;
			case 2:
					bcm_bprintf(b, "unsolicited");
			break;
			case 3:
					bcm_bprintf(b, "both");
			break;
	}

	bcm_bprintf(b, "wlc->vhti->vht_cap.rx_mcs_map = 0x%x \n", wlc->vhti->vht_cap.rx_mcs_map);
	bcm_bprintf(b, "wlc->vhti->vht_cap.tx_mcs_map = 0x%x \n", wlc->vhti->vht_cap.tx_mcs_map);
	wlc_dump_vht_mcsmap("\nVHT mcsmap", wlc->default_bss->rateset.vht_mcsmap, b);

	bcm_bprintf(b, "\n");
	return 0;
}
#endif /* BCMDBG etc. */

void
wlc_vht_bcn_scb_upd(wlc_vht_info_t *vhti, int band, struct scb *scb,
	ht_cap_ie_t *ht_cap, vht_cap_ie_t *vht_cap, vht_op_ie_t *vht_op,
	uint8 vht_ratemask)
{
	wlc_bsscfg_t *cfg;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	if (ht_cap != NULL && vht_cap != NULL &&
	    cfg->associated &&
	    (cfg->current_bss->flags2 & WLC_BSS_VHT)) {
		wlc_vht_update_scb_state(vhti, band, scb, ht_cap, vht_cap, vht_op, vht_ratemask);
		if (BSSCFG_STA(cfg)) {
			cfg->current_bss->vht_rxmcsmap = vht_cap->rx_mcs_map;
			cfg->current_bss->vht_txmcsmap = vht_cap->tx_mcs_map;
		}
		return;
	}

	if (SCB_VHT_CAP(scb)) {
		wlc_vht_update_scb_state(vhti, band, scb, NULL, NULL, NULL, 0);
		return;
	}
}

void
wlc_vht_update_scb_oper_mode(wlc_vht_info_t *vhti, struct scb *scb, uint8 oper_mode)
{
	wlc_info_t *wlc = vhti->wlc;
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);

	uint8 type;

#if defined(BCMDBG)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */

	if (!cubby_info->oper_mode_enabled || (oper_mode == cubby_info->oper_mode))
		return;

	type = DOT11_OPER_MODE_RXNSS_TYPE(oper_mode);

	if (type != 0)
	{
		return;
	}

#if defined(BCMDBG)
	WL_MODE_SWITCH(("wl%d: updating oper mode: old=%x, new=%x, for scb addr %s\n",
		WLCWLUNIT(wlc), cubby_info->oper_mode, oper_mode,
		bcm_ether_ntoa(&scb->ea, eabuf)));
#endif /* BCMDBG */

	cubby_info->oper_mode = oper_mode;
	cubby_info->oper_mode_enabled = TRUE;
	wlc_scb_ratesel_init(wlc, scb);
}

uint8
wlc_vht_get_scb_ratemask(wlc_vht_info_t *vhti, struct scb *scb)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return 0;
	} else {
		return cubby_info->ratemask;
	}
}

uint16
wlc_vht_get_scb_flags(wlc_vht_info_t *vhti, struct scb *scb)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return 0;
	} else {
		return cubby_info->flags;
	}
}

#if defined(BCMDBG) || defined(BCMCONDITIONAL_LOGGING)
static uint16
wlc_vht_get_scb_rxmcsmap(wlc_vht_info_t *vhti, struct scb *scb)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return 0;
	} else {
		return cubby_info->rxmcsmap;
	}
}

static uint16
wlc_vht_get_tx_mcsmap(wlc_vht_info_t *vhti)
{
	return vhti->vht_cap.tx_mcs_map;
}
#endif /* BCMDBG || BCMCONDITIONAL_LOGGING */

bool
wlc_vht_get_scb_opermode_enab(wlc_vht_info_t *vhti, struct scb *scb)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return FALSE;
	} else {
		return cubby_info->oper_mode_enabled;
	}
}

bool
wlc_vht_set_scb_opermode_enab(wlc_vht_info_t *vhti, struct scb *scb, bool set)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return FALSE;
	} else {
		cubby_info->oper_mode_enabled = set;
		return TRUE;
	}
}


uint8
wlc_vht_get_scb_opermode(wlc_vht_info_t *vhti, struct scb *scb)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return 0;
	} else {
		return cubby_info->oper_mode;
	}
}

uint8
wlc_vht_get_scb_ratemask_per_band(wlc_vht_info_t *vhti, struct scb *scb)
{
	uint8 vht_ratemask = 0;
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return 0;
	}
	if (SCB_VHT_CAP(scb)) {
		vht_ratemask = cubby_info->ratemask;
		if (BAND_2G(vhti->wlc->band->bandtype)) {
			vht_ratemask &= ~(WL_VHT_FEATURES_RATES_5G);
		} else {
			vht_ratemask &= ~(WL_VHT_FEATURES_RATES_2G);
		}
	}
	return vht_ratemask;
}

uint16
wlc_vht_get_scb_amsdu_mtu_pref(wlc_vht_info_t *vhti, struct scb *scb)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return 0;
	} else {
		return cubby_info->amsdu_mtu_pref;
	}
}

uint8
wlc_vht_get_scb_ampdu_max_exp(wlc_vht_info_t *vhti, struct scb *scb)
{
	wlc_vht_scb_info_t *cubby_info =
		(wlc_vht_scb_info_t *)SCB_VHT_INFO(vhti, scb);
	if (!cubby_info) {
		return 0;
	} else {
		return cubby_info->ampdu_max_exp;
	}
}

/* for monitor */
static uint8
wlc_vht_get_gid(uint8 *plcp)
{
	uint32 plcp0 = plcp[0] | (plcp[1] << 8);
	return (plcp0 & VHT_SIGA1_GID_MASK) >> VHT_SIGA1_GID_SHIFT;
}

static uint16
wlc_vht_get_aid(uint8 *plcp)
{
	uint32 plcp0 = plcp[0] | (plcp[1] << 8) | (plcp[2] << 16);
	return (plcp0 & VHT_SIGA1_PARTIAL_AID_MASK) >> VHT_SIGA1_PARTIAL_AID_SHIFT;
}

static bool
wlc_vht_get_txop_ps_not_allowed(uint8 *plcp)
{
	return !!(plcp[2] & (VHT_SIGA1_TXOP_PS_NOT_ALLOWED >> 16));
}

static bool
wlc_vht_get_sgi_nsym_da(uint8 *plcp)
{
	return !!(plcp[3] & VHT_SIGA2_GI_W_MOD10);
}

static bool
wlc_vht_get_ldpc_extra_symbol(uint8 *plcp)
{
	return !!(plcp[3] & VHT_SIGA2_LDPC_EXTRA_OFDM_SYM);
}

static bool
wlc_vht_get_beamformed(uint8 *plcp)
{
	return !!(plcp[4] & (VHT_SIGA2_BEAMFORM_ENABLE >> 8));
}

INLINE bool
wlc_vht_prep_rate_info(wlc_vht_info_t *vhti, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	ratespec_t rspec, struct wl_rxsts *sts)
{
	wlc_info_t *wlc = vhti->wlc;
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		if (PRXS5_ACPHY_DYNBWINNONHT(&wrxh->rxhdr))
			sts->vhtflags |= WL_RXS_VHTF_DYN_BW_NONHT;
		else
			sts->vhtflags &= ~WL_RXS_VHTF_DYN_BW_NONHT;

		switch (PRXS5_ACPHY_CHBWINNONHT(&wrxh->rxhdr)) {
		case PRXS5_ACPHY_CHBWINNONHT_20MHZ:
			sts->bw_nonht = WLC_20_MHZ;
			break;
		case PRXS5_ACPHY_CHBWINNONHT_40MHZ:
			sts->bw_nonht = WLC_40_MHZ;
			break;
		case PRXS5_ACPHY_CHBWINNONHT_80MHZ:
			sts->bw_nonht = WLC_80_MHZ;
			break;
		case PRXS5_ACPHY_CHBWINNONHT_160MHZ:
			sts->bw_nonht = WLC_160_MHZ;
			break;
		default:
			ASSERT(FALSE);
		}
	}

	/* prepare rate/modulation info */
	if (RSPEC_ISVHT(rspec)) {
		int16 ctl_sb = CHSPEC_CTL_SB(sts->chanspec);
		uint32 bw = RSPEC_BW(rspec);

		/* prepare VHT rate/modulation info */
		sts->nss = (rspec & RSPEC_VHT_NSS_MASK) >> RSPEC_VHT_NSS_SHIFT;
		sts->mcs = (rspec & RSPEC_VHT_MCS_MASK);

		if (CHSPEC_IS8080(sts->chanspec) || CHSPEC_IS160(sts->chanspec)) {
			if (bw == RSPEC_BW_20MHZ) {
				switch (CHSPEC_CTL_SB(sts->chanspec)) {
				default:
				case WL_CHANSPEC_CTL_SB_LLL:
					sts->bw = WL_RXS_VHT_BW_20LLL;
					break;
				case WL_CHANSPEC_CTL_SB_LLU:
					sts->bw = WL_RXS_VHT_BW_20LLU;
					break;
				case WL_CHANSPEC_CTL_SB_LUL:
					sts->bw = WL_RXS_VHT_BW_20LUL;
					break;
				case WL_CHANSPEC_CTL_SB_LUU:
					sts->bw = WL_RXS_VHT_BW_20LUU;
					break;
				case WL_CHANSPEC_CTL_SB_ULL:
					sts->bw = WL_RXS_VHT_BW_20ULL;
					break;
				case WL_CHANSPEC_CTL_SB_ULU:
					sts->bw = WL_RXS_VHT_BW_20ULU;
					break;
				case WL_CHANSPEC_CTL_SB_UUL:
					sts->bw = WL_RXS_VHT_BW_20UUL;
					break;
				case WL_CHANSPEC_CTL_SB_UUU:
					sts->bw = WL_RXS_VHT_BW_20UUU;
					break;
				}
			}
			else if (bw == RSPEC_BW_40MHZ) {
				if (ctl_sb == WL_CHANSPEC_CTL_SB_LLL ||
					ctl_sb == WL_CHANSPEC_CTL_SB_LLU) {
					sts->bw = WL_RXS_VHT_BW_40LL;
				}
				else if (ctl_sb == WL_CHANSPEC_CTL_SB_LUL ||
					ctl_sb == WL_CHANSPEC_CTL_SB_LUU) {
					sts->bw = WL_RXS_VHT_BW_40LU;
				}
				else if (ctl_sb == WL_CHANSPEC_CTL_SB_ULL ||
					ctl_sb == WL_CHANSPEC_CTL_SB_ULU) {
					sts->bw = WL_RXS_VHT_BW_40UL;
				}
				else if (ctl_sb == WL_CHANSPEC_CTL_SB_UUL ||
					ctl_sb == WL_CHANSPEC_CTL_SB_UUU) {
					sts->bw = WL_RXS_VHT_BW_40UU;
				}
				else
				{
					sts->bw = WL_RXS_VHT_BW_40LL;
				}
			}
			else if (bw == RSPEC_BW_80MHZ) {
				if (ctl_sb >= WL_CHANSPEC_CTL_SB_LLL &&
					ctl_sb <= WL_CHANSPEC_CTL_SB_LUU) {
					sts->bw = WL_RXS_VHT_BW_80L;
				}
				else if (ctl_sb >= WL_CHANSPEC_CTL_SB_ULL &&
					ctl_sb <= WL_CHANSPEC_CTL_SB_UUU) {
					sts->bw = WL_RXS_VHT_BW_80U;
				}
				else
				{
					sts->bw = WL_RXS_VHT_BW_80L;
				}
			}
			else if (bw == RSPEC_BW_160MHZ) {
				sts->bw = WL_RXS_VHT_BW_160;
			}
		}
		else if (CHSPEC_IS80(sts->chanspec)) {
			if (bw == RSPEC_BW_20MHZ) {
				switch (CHSPEC_CTL_SB(sts->chanspec)) {
					default:
					case WL_CHANSPEC_CTL_SB_LL:
						sts->bw = WL_RXS_VHT_BW_20LL;
						break;
					case WL_CHANSPEC_CTL_SB_LU:
						sts->bw = WL_RXS_VHT_BW_20LU;
						break;
					case WL_CHANSPEC_CTL_SB_UL:
						sts->bw = WL_RXS_VHT_BW_20UL;
						break;
					case WL_CHANSPEC_CTL_SB_UU:
						sts->bw = WL_RXS_VHT_BW_20UU;
						break;
				}
			} else if (bw == RSPEC_BW_40MHZ) {
				switch (CHSPEC_CTL_SB(sts->chanspec)) {
					default:
					case WL_CHANSPEC_CTL_SB_L:
						sts->bw = WL_RXS_VHT_BW_40L;
						break;
					case WL_CHANSPEC_CTL_SB_U:
						sts->bw = WL_RXS_VHT_BW_40U;
						break;
				}
			} else {
				sts->bw = WL_RXS_VHT_BW_80;
			}
		} else if (CHSPEC_IS40(sts->chanspec)) {
			if (bw == RSPEC_BW_20MHZ) {
				switch (CHSPEC_CTL_SB(sts->chanspec)) {
					default:
					case WL_CHANSPEC_CTL_SB_L:
						sts->bw = WL_RXS_VHT_BW_20L;
						break;
					case WL_CHANSPEC_CTL_SB_U:
						sts->bw = WL_RXS_VHT_BW_20U;
						break;
				}
			} else if (bw == RSPEC_BW_40MHZ) {
				sts->bw = WL_RXS_VHT_BW_40;
			}
		} else {
			sts->bw = WL_RXS_VHT_BW_20;
		}

		if (RSPEC_ISSTBC(rspec))
			sts->vhtflags |= WL_RXS_VHTF_STBC;
		if (wlc_vht_get_txop_ps_not_allowed(plcp))
			sts->vhtflags |= WL_RXS_VHTF_TXOP_PS;
		if (RSPEC_ISSGI(rspec)) {
			sts->vhtflags |= WL_RXS_VHTF_SGI;
			if (wlc_vht_get_sgi_nsym_da(plcp))
				sts->vhtflags |= WL_RXS_VHTF_SGI_NSYM_DA;
		}
		if (RSPEC_ISLDPC(rspec)) {
			sts->coding = WL_RXS_VHTF_CODING_LDCP;
			if (wlc_vht_get_ldpc_extra_symbol(plcp)) {
				sts->vhtflags |= WL_RXS_VHTF_LDPC_EXTRA;
			}
		}
		if (wlc_vht_get_beamformed(plcp))
			sts->vhtflags |= WL_RXS_VHTF_BF;

		sts->gid = wlc_vht_get_gid(plcp);
		sts->aid = wlc_vht_get_aid(plcp);
		sts->datarate = RSPEC2KBPS(rspec)/500;
		return TRUE;
	}
	return FALSE;
}

#ifdef WLTXMONITOR
INLINE void
wlc_vht_txmon_chspec(uint16 phytxctl, uint16 phytxctl1,
	uint16 chan_band, uint16 chan_num,
	struct wl_txsts *sts, uint16 *chan_bw)
{
	uint8 sb;

	switch (phytxctl & D11AC_PHY_TXC_BW_MASK) {
		case D11AC_PHY_TXC_BW_20MHZ:
			*chan_bw = WL_CHANSPEC_BW_20;
			break;
		case D11AC_PHY_TXC_BW_40MHZ:
			*chan_bw = WL_CHANSPEC_BW_40;
			break;
		case D11AC_PHY_TXC_BW_80MHZ:
			*chan_bw = WL_CHANSPEC_BW_80;
			break;
		case D11AC_PHY_TXC_BW_160MHZ:
			*chan_bw = WL_CHANSPEC_BW_160;
			break;
		default:
			*chan_bw = WL_CHANSPEC_BW_20;
			ASSERT(FALSE);
	}

	sb = (phytxctl1 & D11AC_PHY_TXC_PRIM_SUBBAND_MASK) << WL_CHANSPEC_CTL_SB_SHIFT;
	sts->chanspec = (chanspec_t)(chan_num | *chan_bw | sb | chan_band);
}

INLINE void
wlc_vht_txmon_htflags(uint16 phytxctl, uint16 phytxctl1, uint8 *plcp,
	uint16 chan_bw, uint16 chan_band, uint16 chan_num, ratespec_t *rspec,
	struct wl_txsts *sts)
{
	uint8 nss, mcs;

	*rspec = wlc_vht_get_rspec_from_plcp(plcp);
	nss = (*rspec & RSPEC_VHT_NSS_MASK) >> RSPEC_VHT_NSS_SHIFT;
	mcs = (*rspec & RSPEC_VHT_MCS_MASK);
	/* Use regular mcs40 till 69 to indicate VHT rates */
	sts->mcs = 30 + (nss * 10) + mcs;

	if (chan_bw == WL_CHANSPEC_BW_80) {
		sts->htflags = WL_RXS_VHTF_80;
	} else if (chan_bw == WL_CHANSPEC_BW_40) {
		if ((phytxctl1 & D11AC_PHY_TXC_PRIM_SUBBAND_MASK) ==
			D11AC_PHY_TXC_PRIM_SUBBAND_LLL)
			sts->htflags = WL_RXS_VHTF_40L;
		else
			sts->htflags = WL_RXS_VHTF_40U;
	} else {
		switch (phytxctl1 & D11AC_PHY_TXC_PRIM_SUBBAND_MASK) {
			case D11AC_PHY_TXC_PRIM_SUBBAND_LLL:
				sts->htflags = WL_RXS_VHTF_20LL;
				break;
			case D11AC_PHY_TXC_PRIM_SUBBAND_LLU:
				sts->htflags = WL_RXS_VHTF_20LU;
				break;
			case D11AC_PHY_TXC_PRIM_SUBBAND_LUL:
				sts->htflags = WL_RXS_VHTF_20UL;
				break;
			case D11AC_PHY_TXC_PRIM_SUBBAND_LUU:
				sts->htflags = WL_RXS_VHTF_20UU;
				break;
		}
	}
	if (*rspec & RSPEC_STBC)
		sts->htflags |= (1 << WL_RXS_HTF_STBC_SHIFT);
	if (*rspec & RSPEC_SHORT_GI)
		sts->htflags |= WL_RXS_HTF_SGI;
	if (*rspec & RSPEC_LDPC_CODING)
		sts->htflags |= WL_RXS_HTF_LDPC;

	sts->encoding = WL_RXS_ENCODING_VHT;
	sts->preamble = (phytxctl & PHY_TXC_SHORT_HDR) ?
		WL_RXS_PREAMBLE_HT_GF: WL_RXS_PREAMBLE_HT_MM;

	sts->datarate = RSPEC2KBPS(*rspec)/500;
	sts->phytype = WL_RXS_PHY_N;
}
#endif /* TXMON */
static void
wlc_vht_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	bi->flags2 &= ~(WLC_BSS_VHT | WLC_BSS_80MHZ | WLC_BSS_SGI_80
			| WLC_BSS_8080MHZ | WLC_BSS_8080MHZ | WLC_BSS_SGI_160);
	if (BSS_VHT_ENAB(wlc, cfg)) {
		bi->flags2 |= WLC_BSS_VHT;
		if (CHSPEC_IS80(bi->chanspec)) {
			bi->flags2 |= WLC_BSS_80MHZ;
			if (bi->vht_capabilities & VHT_CAP_INFO_SGI_80MHZ) {
				bi->flags2 |= WLC_BSS_SGI_80;
			}
		}
		else if (CHSPEC_IS8080(bi->chanspec)) {
			bi->flags2 |= WLC_BSS_8080MHZ;
			if (bi->vht_capabilities & VHT_CAP_INFO_SGI_160MHZ) {
				bi->flags2 |= WLC_BSS_SGI_160;
			}
		}
		else if (CHSPEC_IS160(bi->chanspec)) {
			bi->flags2 |= WLC_BSS_160MHZ;
			if (bi->vht_capabilities & VHT_CAP_INFO_SGI_160MHZ) {
				bi->flags2 |= WLC_BSS_SGI_160;
			}
		}
	}
}

static void
wlc_vht_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_vht_info_t *vhti = (wlc_vht_info_t *) ctx;
	wlc_info_t *wlc =  vhti->wlc;
	wlc_bsscfg_t *cfg = evt->bsscfg;

	if (evt->up) {
		if (BSSCFG_AP(cfg) || BSSCFG_IBSS(cfg)) {
			if (VHT_ENAB(wlc->pub)) {
				wlc_vht_bsscfg_up(wlc, cfg, cfg->current_bss);
			}
		}
	}
}
#endif /* WL11AC */
