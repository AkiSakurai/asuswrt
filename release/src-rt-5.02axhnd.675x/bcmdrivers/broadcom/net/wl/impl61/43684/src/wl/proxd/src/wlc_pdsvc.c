/*
 * Proximity detection service layer implementation for Broadcom 802.11 Networking Driver
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
 * $Id: wlc_pdsvc.c 692436 2017-03-28 04:54:42Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <osl.h>
#include <sbchipc.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_bmac.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <hndpmu.h>
#include <wlc_pcb.h>
#include <wlc_event_utils.h>

#include <wlc_pdsvc.h>
#include <wlc_pddefs.h>
#include <wlc_pdmthd.h>

#include "pdsvc.h"
#include "pdftm.h"
#include "pdburst.h"
#if defined(WL_FTM) && defined(WLAWDL)
#include "pdftmawdl.h"
#endif // endif

#include <phy_tof_api.h>
#include <wlc_slotted_bss.h>
#include <wlc_dump.h>

#define PROXD_NAME "proxd"

#define ELEM_SWAP(a,b) {uint32 t = (a); (a) = (b); (b) = t;}

typedef struct wlc_pdsvc_config {
	uint16	mode;
	void *method_params; /* points to current method params structure */
	struct ether_addr mcastaddr;	/* Multicast address */
	struct ether_addr bssid;	/* BSSID */
} wlc_pdsvc_config_t;

/* This is the mainstructure of proximity detection service */
struct wlc_pdsvc_info {
	uint32 signature;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	int cfgh;				/* bsscfg cubby handle */
	uint16 method;
	uint16 state;
	wlc_pdsvc_config_t config;
	pdsvc_funcs_t funcs;
	pdmthd_if_t *cur_mif;  /* current method interface */
	pdsvc_payload_t payload;
	uint32 fvco;
	uint32 pllreg;
	proxd_method_create  method_create_fn[PROXD_MAX_METHOD_NUM];
	uint32 ki;
	uint32 kt;
	void * ranging;
	uint8 rptlistnum;
	struct ether_addr *rptlist;
	pdftm_t *ftm;
	wl_proxd_params_tof_tune_t *tunep;
	uint64	clkfactor; /* clock factor */
	uint16	pwrflag;
};

/* Proximity specific private datas in bsscfg */
/* XXX allocate the struct and reserve a pointer to the struct in the bsscfg
 * as the bsscfg cubby when this structure grows larger ...
 */
typedef struct {
	mbool flags;	/* flags for proximity detection */
} bss_proxd_cubby_t;

#define PROXD_FLAG_TXPWR_OVERRIDE	0x1	/* override tx power of the transmit frames */

#define PROXD_BSSCFG_CUBBY(pdsvc_info, cfg) \
	((bss_proxd_cubby_t *)BSSCFG_CUBBY(cfg, (pdsvc_info)->cfgh))

/* IOVAR declarations */
enum {
	/*
	 IOV: IOV_PROXD
	 Purpose: This IOVAR enables/disables proximity detection and sets mode.
	*/
	IOV_PROXD		= 0,
	/*
	 IOV: IOV_PROXD_PARAMS
	 Purpose: This IOVAR sets/gets the parameters for the specific method
	*/
	IOV_PROXD_PARAMS	= 1,
	/*
	 IOV: IOV_PROXD_BSSID
	 Purpose: This IOVAR sets/gets BSSID of proximity detection frames
	*/
	IOV_PROXD_BSSID		= 2,
	/*
	 IOV: IOV_PROXD_MCASTADDR
	 Purpose: This IOVAR sets/gets multicast address of proximity detection frames
	*/
	IOV_PROXD_MCASTADDR	= 3,
	/*
	 IOV: IOV_PROXD_FIND
	 Purpose: Start proximity detection
	*/
	IOV_PROXD_FIND		= 4,
	/*
	 IOV: IOV_PROXD_STOP
	 Purpose: Stop proximity detection
	*/
	IOV_PROXD_STOP		= 5,
	/*
	 IOV: IOV_PROXD_STATUS
	 Purpose: Get proximity detection status
	*/
	IOV_PROXD_STATUS	= 6,
	/*
	 IOV: IOV_PROXD_MONITOR
	 Purpose: Start proximity detection monitor mode
	*/
	IOV_PROXD_MONITOR	= 7,
	/*
	 IOV: IOV_PROXD_PAYLOAD
	 Purpose: Get/Set proximity detection payload content
	*/
	IOV_PROXD_PAYLOAD	= 8,
	/*
	 IOV: IOV_PROXD_COLLECT
	*/
	IOV_PROXD_COLLECT = 9,
	/*
	 IOV: IOV_PROXD_TUNE
	*/
	IOV_PROXD_TUNE = 10,
	/*
		Minimum time required between two consecutive measurement frames (for target)
	*/
	IOV_FTM_PERIOD = 11,
	/*
		REPORT
	*/
	IOV_PROXD_REPORT = 12,
	/*
	 IOV: IOV_AVB_LOCAL_TIME = 13
	*/
	IOV_AVB_LOCAL_TIME = 13,
	/*
		SEQ
	*/
};

/* Iovars */
static const bcm_iovar_t  wlc_proxd_iovars[] = {
	{"proxd", IOV_PROXD, 0, 0, IOVT_BUFFER, sizeof(uint16)*2},
#if defined(WL_TOF)
	{"proxd_params", IOV_PROXD_PARAMS, 0, 0, IOVT_BUFFER, sizeof(wl_proxd_params_iovar_t)},
	{"proxd_bssid", IOV_PROXD_BSSID, 0, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"proxd_mcastaddr", IOV_PROXD_MCASTADDR, 0, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"proxd_find", IOV_PROXD_FIND, 0, 0, IOVT_VOID, 0},
	{"proxd_stop", IOV_PROXD_STOP, 0, 0, IOVT_VOID, 0},
	{"proxd_status", IOV_PROXD_STATUS, 0, 0, IOVT_BUFFER, sizeof(wl_proxd_status_iovar_t)},
	{"proxd_monitor", IOV_PROXD_MONITOR, 0, 0, IOVT_BUFFER, ETHER_ADDR_LEN},
	{"proxd_payload", IOV_PROXD_PAYLOAD, 0, 0, IOVT_BUFFER, 0},
#endif /* WL_TOF */
#ifdef TOF_DBG
	{"proxd_collect", IOV_PROXD_COLLECT, (IOVF_GET_UP | IOVF_SET_UP | IOVF_GET_CLK),
	IOVT_BUFFER, sizeof(wl_proxd_collect_data_t)},
#endif /* TOF_DBG */
#ifdef WL_TOF
	{"proxd_ftmperiod", IOV_FTM_PERIOD, 0, IOVT_UINT32, 0},
#endif /* WL_TOF */
	{"proxd_tune", IOV_PROXD_TUNE, (IOVF_GET_UP | IOVF_SET_UP),
	0, IOVT_BUFFER, sizeof(wl_proxd_params_iovar_t)},
	{"proxd_report", IOV_PROXD_REPORT, 0, 0, IOVT_BUFFER, 0},
#if defined(WL_TOF)
#ifdef WL_PROXD_AVB_TS
	{"avb_local_time", IOV_AVB_LOCAL_TIME, 0, IOVT_BUFFER, sizeof(uint32)},
#endif /* WL_PROXD_AVB_TS */
#endif /* WL_TOF */
#ifdef TOF_DBG_SEQ
	{"tof_seq", IOV_TOF_SEQ,  0, IOVT_UINT32, 0},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

#if defined(WL_TOF)
/* Proximity Default BSSID and Default Multicast address */
STATIC CONST struct ether_addr proxd_default_bssid = {{0x00, 0x90, 0x4c, 0x02, 0x17, 0x03}};
STATIC CONST struct ether_addr proxd_default_mcastaddr = {{0x01, 0x90, 0x4c, 0x02, 0x17, 0x03}};
#endif // endif

#ifdef BCMDBG
static void wlc_proxd_bsscfg_cubby_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_proxd_bsscfg_cubby_dump NULL
#endif // endif

#ifdef WL_FTM
static void proxd_bss_updown(void *ctx, bsscfg_up_down_event_data_t *evt);
static wl_proxd_params_tof_tune_t *proxd_init_tune(wlc_info_t *wlc);
#endif /* WL_FTM */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */

#if defined(WL_TOF)
/* Initialize the RSSI method configuration parameters */
static void
BCMATTACHFN(wlc_pdsvc_init)(wlc_pdsvc_info_t *pdsvc)
{
	/* Setting common default parametrs */
	memcpy(&pdsvc->config.bssid, &proxd_default_bssid, ETHER_ADDR_LEN);
	memcpy(&pdsvc->config.mcastaddr, &proxd_default_mcastaddr, ETHER_ADDR_LEN);

	/* get rrtcal from nvram */
	pdsvc->ki = getintvararray(pdsvc->wlc->pub->vars, "rrtcal", 0);
	pdsvc->kt = getintvararray(pdsvc->wlc->pub->vars, "rrtcal", 1);
}
#endif // endif

/* bsscfg cubby */
static int
wlc_proxd_bsscfg_cubby_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_pdsvc_info_t *pdsvc = (wlc_pdsvc_info_t *)ctx;
	wlc_info_t *wlc = pdsvc->wlc;
	bss_proxd_cubby_t *proxd_bsscfg_cubby;
	uint8 gmode = GMODE_AUTO;
	int err;

	ASSERT(cfg != NULL);

	proxd_bsscfg_cubby = (bss_proxd_cubby_t *)PROXD_BSSCFG_CUBBY(pdsvc, cfg);

	bzero((void *)proxd_bsscfg_cubby, sizeof(*proxd_bsscfg_cubby));

	if (!BSS_PROXD_ENAB(wlc, cfg)) {
		return BCME_OK;
	}

	if (!IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) {
		gmode = wlc->bandstate[BAND_2G_INDEX]->gmode;
	}
	if (gmode == GMODE_LEGACY_B) {
		WL_ERROR(("wl%d: %s: gmode cannot be GMODE_LEGACY_B\n", wlc->pub->unit,
			__FUNCTION__));
		err = BCME_BADRATESET;
		goto exit;
	}

	if ((err = wlc_bsscfg_rateset_init(wlc, cfg, WLC_RATES_OFDM,
			WL_BW_CAP_40MHZ(wlc->band->bw_cap) ? CHSPEC_WLC_BW(wlc->home_chanspec) : 0,
			BSS_N_ENAB(wlc, cfg))) != BCME_OK) {
		WL_ERROR(("wl%d: %s: failed rateset int\n", wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	/* set bsscfg to IBSS */
	cfg->current_bss->bss_type = DOT11_BSSTYPE_INDEPENDENT;

	/* Do not particiate in mchan scheduler since
	   proxd has its own scheduler for channel access
	*/
	/* bsscfg->flags |= WLC_BSSCFG_MCHAN_DISABLE; */

	/* Initialize default flags if needed */

	/* Set BSSID for this bsscfg */
	bcopy(&pdsvc->config.bssid, &cfg->BSSID, ETHER_ADDR_LEN);

	/* if the driver is not up, return here.
	 * BSSID to AMT will be set during the driver up later.
	 * This would fall into one of the following two cases.
	 *  1) wl is down from Host
	 *  2) radio is down due to mpc
	 */
	if (!wlc->pub->up)
		goto exit;

	ASSERT(wlc->clk);

	/* Set BSSID to AMT (or RCMTA) */
	wlc_set_bssid(cfg);

exit:
	return err;
}

static void
wlc_proxd_bsscfg_cubby_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
}

#ifdef BCMDBG
static void
wlc_proxd_bsscfg_cubby_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_pdsvc_info_t *pdsvc = (wlc_pdsvc_info_t *)ctx;
	bss_proxd_cubby_t *proxd_bsscfg_cubby;

	ASSERT(pdsvc != NULL);
	ASSERT(cfg != NULL);

	proxd_bsscfg_cubby = (bss_proxd_cubby_t *)PROXD_BSSCFG_CUBBY(pdsvc, cfg);

	bcm_bprintf(b, "proxd bss flags: %x\n", proxd_bsscfg_cubby->flags);
}
#endif // endif

#if defined(WL_TOF)
/* Enabling the proximity interface */
static int
wlc_proxd_ifadd(wlc_pdsvc_info_t *pdsvc, struct ether_addr *addr)
{
	wlc_info_t *wlc = pdsvc->wlc;
	wlc_bsscfg_t *bsscfg = NULL;
	int idx;
	int err = BCME_OK;
	wlc_bsscfg_type_t type = {BSSCFG_TYPE_PROXD, BSSCFG_SUBTYPE_NONE};

	/* Get the free id to create bsscfg */
	if ((idx = wlc_bsscfg_get_free_idx(wlc)) == -1) {
		WL_ERROR(("wl%d: %s: no free bsscfg\n", wlc->pub->unit, __FUNCTION__));
		err = BCME_NORESOURCE;
		goto exit;
	}
	if ((bsscfg = wlc_bsscfg_alloc(wlc, idx, &type, WLC_BSSCFG_NOIF, 0, NULL)) == NULL) {
		WL_ERROR(("wl%d: %s: cannot create bsscfg\n", wlc->pub->unit, __FUNCTION__));
		err = BCME_NOMEM;
		goto exit;
	}

	if (addr)
		bcopy(addr, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);

	if (wlc_bsscfg_init(wlc, bsscfg) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_pd_bsscfg_init failed \n",
			wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	return BCME_OK;

exit:
	if (bsscfg != NULL)
		wlc_bsscfg_free(wlc, bsscfg);
	pdsvc->bsscfg = NULL;

	return err;
}
/* Deleting the proximity interface */
static void
wlc_proxd_ifdel(wlc_pdsvc_info_t *pdsvc)
{
	wlc_bsscfg_t *bsscfg;

	ASSERT(pdsvc != NULL);
	bsscfg = pdsvc->bsscfg;

	if (bsscfg != NULL) {
		if (bsscfg->enable)
			wlc_bsscfg_disable(pdsvc->wlc, bsscfg);
		wlc_bsscfg_free(pdsvc->wlc, bsscfg);
	}

	pdsvc->bsscfg = NULL;
}

static int
wlc_proxd_stop(void *ctx)
{
	wlc_pdsvc_info_t *pdsvc = (wlc_pdsvc_info_t *)ctx;

	ASSERT(pdsvc != NULL);

	/* Stop proximity detection.
	   It should be safe to call stop even if it is not active.
	*/
	if (pdsvc->cur_mif && pdsvc->cur_mif->mstart) {
		(*pdsvc->cur_mif->mstart)(pdsvc->cur_mif, FALSE);
	}

	return BCME_OK;
}
#endif /* WL_TOF */
#ifdef WL_FTM
/* wl proxd_tune get command */
static int
pdburst_get_tune(wlc_info_t *wlc, wl_proxd_params_tof_tune_t *tune, void *pbuf, int len)
{
	wl_proxd_params_tof_tune_t *tunep = pbuf;
	uint32 *kip = NULL, *ktp = NULL;

	if (len < sizeof(wl_proxd_params_tof_tune_t))
		return BCME_BUFTOOSHORT;

	memcpy(pbuf, tune, sizeof(wl_proxd_params_tof_tune_t));
	if (!tunep->Ki)
		kip = &tunep->Ki;
	if (!tunep->Kt)
		ktp = &tunep->Kt;
	if (kip || ktp) {
		wl_proxd_session_flags_t flags;
		const pdburst_config_t *configp;
		/* get the default burst config */
		configp = pdftm_get_burst_config(wlc->pdsvc_info->ftm, NULL, &flags);
		if (configp) {
			ratespec_t ackrspec;
			if (!tune->vhtack) {
				ackrspec = LEGACY_RSPEC(PROXD_DEFAULT_TX_RATE);
			} else {
				ackrspec = LEGACY_RSPEC(PROXD_DEFAULT_TX_RATE) |
				        WL_RSPEC_ENCODE_VHT;
			}
			wlc_phy_kvalue(WLC_PI(wlc), configp->chanspec,
				proxd_get_ratespec_idx(configp->ratespec, ackrspec),
				kip, ktp,
				((flags & WL_PROXD_SESSION_FLAG_SEQ_EN) ? WL_PROXD_SEQEN : 0));
		}
	}
	return BCME_OK;
}

/* wl proxd_tune set command */
static int
pdburst_set_tune(wl_proxd_params_tof_tune_t *tune, void *pbuf, int len)
{
	wl_proxd_params_tof_tune_t *tunep = pbuf;

	if (len < sizeof(wl_proxd_params_tof_tune_t))
		return BCME_BUFTOOSHORT;

	memcpy(tune, pbuf, sizeof(wl_proxd_params_tof_tune_t));
	if (!(tunep->setflags & WL_PROXD_SETFLAG_K))
		tune->Ki = tune->Kt = 0;
	tunep->setflags &= ~WL_PROXD_SETFLAG_K;

	return BCME_OK;
}
#endif /* WL_FTM */

#include <wlc_patch.h>

/* Iovar processing: Each proximity method is created, deleted, and changes it state by iovars */
static int
wlc_proxd_doiovar(void *ctx, uint32 actionid,
	void *params, uint p_len, void *arg, uint a_len, uint val_size, struct wlc_if *wlcif)
{
	wlc_pdsvc_info_t *pdsvc_info = (wlc_pdsvc_info_t *)ctx;
#if defined(WL_TOF)
	wl_proxd_status_iovar_t *proxd_status_iovar_p;
	uint16 mode = 0;
	bool is_active = FALSE, is_wlup = FALSE;
#endif // endif
	uint16 method = 0;
	wlc_info_t *wlc;
	int err = BCME_OK;
#ifdef WL_PROXD_AVB_TS
	uint32 tx;
	wlc_hw_info_t *wlc_hw;
	osl_t *osh;
	d11regs_t *regs;
	uint32 clkst;
	uint32 macctrl1;
#endif /* WL_PROXD_AVB_TS */
#ifdef WL_FTM
	uint16 iov_version = 0;
#endif // endif

	ASSERT(pdsvc_info != NULL);
	CHECK_SIGNATURE(pdsvc_info, WLC_PDSVC_SIGNATURE);
	ASSERT(pdsvc_info->wlc != NULL);
	wlc = pdsvc_info->wlc;

	/* Process IOVARS */

#ifdef WL_FTM
	/* handle/dispatch new API - this needs cleanup */
	do {
		wlc_bsscfg_t *bsscfg;
		wl_proxd_iov_t *iov;
		int iov_len;
		wl_proxd_cmd_t iov_cmd;
		wl_proxd_method_t iov_method;
		wl_proxd_session_id_t iov_sid;
		wl_proxd_iov_t *rsp_iov;
		int rsp_tlvs_len;

		if (IOV_ID(actionid) != IOV_PROXD)
			break;

		if (p_len < WL_PROXD_IOV_HDR_SIZE)
			break;

		iov = (wl_proxd_iov_t *)params;
		iov_version = ltoh16_ua(&iov->version);

		if (iov_version < WL_PROXD_API_MIN_VERSION)
			break;

		/* check length - iov_len includes ver and len */
		iov_len = ltoh16_ua(&iov->len);
		if (p_len < iov->len) {
			err = BCME_BADLEN;
			break;
		}

		/* all other commands except get version need exact match on version */
		iov_cmd = (wl_proxd_cmd_t) ltoh16_ua(&iov->cmd);

		if (iov_version != WL_PROXD_API_VERSION && iov_cmd != WL_PROXD_CMD_GET_VERSION) {
			err = BCME_UNSUPPORTED;
			break;
		}

		iov_method = (wl_proxd_method_t) ltoh16_ua(&iov->method);
		iov_sid = (wl_proxd_session_id_t)ltoh16_ua(&iov->sid);

		/* set up the result */
		rsp_iov = (wl_proxd_iov_t *)arg;
		if (a_len < WL_PROXD_IOV_HDR_SIZE) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		/* init response - length may be adjusted later */
		memcpy(rsp_iov, iov, WL_PROXD_IOV_HDR_SIZE);
		htol16_ua_store(WL_PROXD_IOV_HDR_SIZE, &rsp_iov->len);

		bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
		ASSERT(bsscfg != NULL);

		switch (iov_method) {
		case WL_PROXD_METHOD_NONE:	/* handled by svc */
			/* session id not used - perhaps ignore/log warning? */
			if (iov_sid != WL_PROXD_SESSION_ID_GLOBAL) {
				err = BCME_BADARG;
				break;
			}
			break;
		case WL_PROXD_METHOD_FTM:	/* handled by method */
			if (IOV_ISSET(actionid)) {
				err = pdftm_set_iov(pdsvc_info->ftm, bsscfg,
					iov_cmd, iov_sid, iov->tlvs,
					iov_len - WL_PROXD_IOV_HDR_SIZE);
			} else {
				err = pdftm_get_iov(pdsvc_info->ftm, bsscfg,
					iov_cmd, iov_sid, iov->tlvs,
					iov_len - WL_PROXD_IOV_HDR_SIZE,
					a_len - WL_PROXD_IOV_HDR_SIZE,
					rsp_iov->tlvs, &rsp_tlvs_len);
				ASSERT((rsp_tlvs_len + WL_PROXD_IOV_HDR_SIZE) <= a_len);
				htol16_ua_store(rsp_tlvs_len + WL_PROXD_IOV_HDR_SIZE,
					&rsp_iov->len);
			}

			if (err != BCME_OK)
				break;

			break;
		default:
			err = BCME_UNSUPPORTED;
			break;

		}

		/* indicate errors that are not BCME_... , loses detail. */
		if (err != BCME_OK) {
			 if (!VALID_BCMERROR(err))
				err = BCME_ERROR;
		}
	} while (0);

	if (iov_version == WL_PROXD_API_VERSION)
		goto done;

#endif /* WL_FTM */

	switch (actionid) {
#if defined(WL_TOF)
	/* wl proxd [0|1] [neutral | initiator | target] */
	case IOV_GVAL(IOV_PROXD):
		*((uint16*)arg) = pdsvc_info->method;
		if (pdsvc_info->method > 0)
			*((uint16*)(arg + sizeof(method))) = pdsvc_info->config.mode;
		else
			*((uint16*)(arg + sizeof(method))) = 0;
		break;

	/* wl proxd [0|1] [neutral | initiator | target] */
	case IOV_SVAL(IOV_PROXD):
		if (p_len >= (uint)sizeof(method))
			bcopy(params, &method, sizeof(method));

		if (p_len >= (uint)(sizeof(method) + sizeof(mode)))
			bcopy(params + sizeof(method), &mode, sizeof(mode));

		/* RSSI method removed */
		if ((mode == WL_PROXD_MODE_NEUTRAL) ||
			(method == PROXD_RSSI_METHOD))
			return BCME_UNSUPPORTED;

		if (method > PROXD_MAX_METHOD_NUM)
			return BCME_BADARG;

		if (method != PROXD_UNDEFINED_METHOD) {
			/* Return BCME_BUSY if it is already enabled */
			if (pdsvc_info->method != PROXD_UNDEFINED_METHOD) {
				return BCME_BUSY;
			}
			if (!wlc->clk)
				return BCME_NOCLK;

			/* Initialize the Transmit call back */
			ASSERT(pdsvc_info->bsscfg == NULL);
			if (ETHER_ISNULLADDR(&wlc->cfg->cur_etheraddr)) {
				WL_ERROR(("wl%d: %s: Primary interface ethernet address is NULL \n",
					wlc->pub->unit, __FUNCTION__));
				return BCME_BADADDR;
			}
			if (mode == WL_PROXD_MODE_INITIATOR || method != PROXD_TOF_METHOD) {
				if ((err = wlc_proxd_ifadd(pdsvc_info, &wlc->cfg->cur_etheraddr))
					!= BCME_OK) {
					WL_ERROR(("wl%d: %s: wlc_proxd_ifadd failed \n",
						wlc->pub->unit, __FUNCTION__));
					break;
				}
			}

			/*   create & init method object   */
			ASSERT(pdsvc_info->cur_mif == NULL);

			/* Create a proximity method */
			ASSERT(pdsvc_info->method_create_fn[method-1] != NULL);

			if (!( pdsvc_info->cur_mif =	/* if Ok returns *mth iface obj */
				(*pdsvc_info->method_create_fn[method-1])(
					wlc, mode, &pdsvc_info->funcs, NULL,
					&pdsvc_info->payload))) {
				WL_ERROR(("wl%d: %s: Create method:%d failed \n",
					wlc->pub->unit, __FUNCTION__, method));
				if (pdsvc_info->bsscfg)
					wlc_proxd_ifdel(pdsvc_info);
				break;
			}

			/* Configure  created method */
			if (pdsvc_info->cur_mif->mconfig) {
				/* Call the method configuration */
				(*pdsvc_info->cur_mif->mconfig)(pdsvc_info->cur_mif, mode,
					pdsvc_info->bsscfg);
			}

			/* FIXME: Need to check return value of *pdsvc_info->cur_mif->mconfig
			          Set pdsvc_info->method and mode only if success
			*/
			pdsvc_info->method = method;
			pdsvc_info->config.mode = mode;

			wlc->pub->_proxd = TRUE;
		} else /*  == PROXD_UNDEFINED_METHOD) */ {
			/* Disable proxmity deteciton if it is enabled */
			if (pdsvc_info->method != 0) {
				int ret = 0;
				ASSERT(pdsvc_info->cur_mif != NULL);
				/* Delete method */
				if (pdsvc_info->cur_mif) {
					/* Stop proximity */
					(void) wlc_proxd_stop((void *)pdsvc_info);

					/* Release the method */
					ret = (*pdsvc_info->cur_mif->mrelease)(pdsvc_info->cur_mif);
					pdsvc_info->cur_mif = NULL;
				}

				/* Delete the proxd interface */
				if (pdsvc_info->bsscfg && !ret) {
					wlc_proxd_ifdel(pdsvc_info);
					pdsvc_info->bsscfg = NULL;
				}
				/* Initiatize all the parameters to NULL */
				pdsvc_info->method = PROXD_UNDEFINED_METHOD;
				pdsvc_info->config.mode = WL_PROXD_MODE_DISABLE;
				wlc->pub->_proxd = FALSE;
			}
		}
		break;

	case IOV_GVAL(IOV_PROXD_PARAMS):
		if (!pdsvc_info->cur_mif)
			return BCME_BADARG;

		if (p_len >= (uint)sizeof(method))
			bcopy(params, &method, sizeof(method));

		/* must be a vallid method and == to created method  */
		if (method == 0 ||
			method > PROXD_MAX_METHOD_NUM ||
			pdsvc_info->method != method)
			return BCME_BADARG;

		/*  read params into the buffer */
		ASSERT(pdsvc_info->cur_mif->rw_params != NULL);
		*(uint8 *)arg = pdsvc_info->method; /* current method */
		err = pdsvc_info->cur_mif->rw_params(pdsvc_info->cur_mif,
			arg+sizeof(method), p_len, 0);

		break;
	case IOV_SVAL(IOV_PROXD_PARAMS):
		/* proximity detection should be in idle state */
		if (pdsvc_info->state != 0)
			return BCME_EPERM;

		if (p_len >= (uint)sizeof(method))
			bcopy(params, &method, sizeof(method));

		/* must be a vallid method and == to created method  */
		if (method == 0 || method > PROXD_MAX_METHOD_NUM ||
			pdsvc_info->method != method) {
			PROXD_TRACE((" cmd mth:%d,  svc cur:%d\n", method, pdsvc_info->method));
			return BCME_BADARG;
		}

		/*  write params into method module  */
		ASSERT(pdsvc_info->cur_mif->rw_params != NULL);
		err = pdsvc_info->cur_mif->rw_params(pdsvc_info->cur_mif,
			params + sizeof(method), p_len, 1);

		break;

	case IOV_GVAL(IOV_PROXD_BSSID):
		bcopy(&pdsvc_info->config.bssid, arg, ETHER_ADDR_LEN);
		break;

	case IOV_SVAL(IOV_PROXD_BSSID):
		/* Don't check if NULL so that to allow clearing bssid */
		bcopy(params, &pdsvc_info->config.bssid, ETHER_ADDR_LEN);

		if (pdsvc_info->bsscfg != NULL) {
			/* Update BSSID */
			bcopy(&pdsvc_info->config.bssid, &pdsvc_info->bsscfg->BSSID,
			      ETHER_ADDR_LEN);
		}
		break;

	case IOV_GVAL(IOV_PROXD_MCASTADDR):
		bcopy(&pdsvc_info->config.mcastaddr, arg, ETHER_ADDR_LEN);
		break;

	case IOV_SVAL(IOV_PROXD_MCASTADDR):
		if (!ETHER_ISMULTI(params))
			return BCME_BADADDR;

		bcopy(params, &pdsvc_info->config.mcastaddr, ETHER_ADDR_LEN);
		break;

	case IOV_SVAL(IOV_PROXD_FIND):
		/* proximity detection should have been enabled to start */
		if (pdsvc_info->method == 0 || !pdsvc_info->cur_mif)
			return BCME_EPERM;

		/* proxd_find and proxd_stop have a dependency on the driver up state.
		 * They are allowed only when the driver is up.
		 * wlc_down_for_mpc() check is required to differentiate it from
		 *  wl down due to MPC
		 */
		is_wlup = wlc->pub->up || wlc_down_for_mpc(wlc);
		if (!is_wlup)
			return BCME_NOTUP;

		/* Check if it is already active */
		if ((*pdsvc_info->cur_mif->mstatus)(pdsvc_info->cur_mif, &is_active, NULL) ==
			BCME_ERROR)
			return BCME_ERROR;

		/* Call start only when it is not active */
		if (!is_active)
			return (*pdsvc_info->cur_mif->mstart)(pdsvc_info->cur_mif, TRUE);
		else
			return BCME_BUSY;
		break;

	case IOV_SVAL(IOV_PROXD_STOP):
		/* proximity detection should have been enabled to stop */
		if (pdsvc_info->method == 0 || !pdsvc_info->cur_mif)
			return BCME_EPERM;

		/* proxd_find and proxd_stop have a dependency on the driver up state.
		 * They are allowed only when the driver is up.
		 * wlc_down_for_mpc() check is required to differentiate it from
		 *  wl down due to MPC
		 */
		is_wlup = wlc->pub->up || wlc_down_for_mpc(wlc);
		if (!is_wlup)
			return BCME_NOTUP;

		(void) wlc_proxd_stop((void *)pdsvc_info);
		break;

	case IOV_GVAL(IOV_PROXD_STATUS):
		/* proximity detection should have been enabled to start */
		if (pdsvc_info->method == 0 || !pdsvc_info->cur_mif)
			return BCME_EPERM;

		if (p_len < sizeof(wl_proxd_status_iovar_t))
			return BCME_BUFTOOSHORT;

		if (pdsvc_info->cur_mif->mstatus) {
			proxd_status_iovar_p = (wl_proxd_status_iovar_t *)arg;
			proxd_status_iovar_p->method = pdsvc_info->method;
			(*pdsvc_info->cur_mif->mstatus)(pdsvc_info->cur_mif, &is_active,
				proxd_status_iovar_p);
		}
		break;

	case IOV_SVAL(IOV_PROXD_MONITOR):
		/* proximity detection goes to monitor mode */
		if (pdsvc_info->method == 0 || !pdsvc_info->cur_mif)
			return BCME_EPERM;

		/* Check if it is already active */
		if ((*pdsvc_info->cur_mif->mstatus)(pdsvc_info->cur_mif, &is_active, NULL) ==
			BCME_ERROR)
			return BCME_ERROR;

		/* Call start only when it is not active */
		if (!is_active)
			(*pdsvc_info->cur_mif->mmonitor)(pdsvc_info->cur_mif, params);
		else
			return BCME_BUSY;
		break;

	case IOV_GVAL(IOV_PROXD_PAYLOAD):
		*((uint16*)arg) = pdsvc_info->payload.len;
		if (pdsvc_info->payload.len > 0)
			memcpy(arg + sizeof(uint16), pdsvc_info->payload.data,
				pdsvc_info->payload.len);
		break;

	case IOV_SVAL(IOV_PROXD_PAYLOAD):
		if (pdsvc_info->payload.data)
			MFREE(wlc->osh, pdsvc_info->payload.data, pdsvc_info->payload.len);
		pdsvc_info->payload.data = NULL;
		if (p_len > 0) {
			pdsvc_info->payload.data = MALLOC(wlc->osh, p_len);
			if (!pdsvc_info->payload.data)
				return BCME_NOMEM;
			memcpy(pdsvc_info->payload.data, arg, p_len);
		}
		pdsvc_info->payload.len = p_len;
		break;
#endif /* WL_TOF */
#if defined(TOF_DBG) && (defined(WL_TOF) || defined(WL_FTM))
	case IOV_GVAL(IOV_PROXD_COLLECT):
	case IOV_SVAL(IOV_PROXD_COLLECT):
		/* proximity detection should be in idle state */
		if (pdsvc_info->state != 0)
			err = BCME_EPERM;
		else if (p_len < (uint)sizeof(wl_proxd_collect_query_t))
			err = BCME_BADARG;
		else {
			uint16 len;
			wl_proxd_collect_query_t quety;
			bcopy(params, &quety, sizeof(quety));

			if (pdsvc_info->cur_mif) {
#ifdef WL_TOF
				/* must be a vallid method and == to created method  */
				if (quety.method == 0 || quety.method > PROXD_MAX_METHOD_NUM ||
					pdsvc_info->method != quety.method) {
					return BCME_BADARG;
				}

				if (pdsvc_info->cur_mif->collect == NULL)
					return BCME_UNSUPPORTED;
				err = pdsvc_info->cur_mif->collect(pdsvc_info->cur_mif,
						&quety, arg, a_len, &len);
#endif /* WL_TOF */
			} else if (pdsvc_info->ftm) {
#if defined(WL_FTM)
				err = pdburst_collection(wlc, NULL, &quety, arg, a_len, &len);
#endif // endif
			} else
				err = BCME_NOTREADY;
		}
		break;
#endif /* TOF_DBG */
	case IOV_GVAL(IOV_PROXD_TUNE):
		if (p_len >= (uint)sizeof(method))
			bcopy(params, &method, sizeof(method));

		if (pdsvc_info->cur_mif) {
#ifdef WL_TOF
			/* must be a vallid method and == to created method  */
			if (method == 0 ||
				method > PROXD_MAX_METHOD_NUM ||
				pdsvc_info->method != method)
				return BCME_BADARG;
			if (method == PROXD_TOF_METHOD) {
				err = wlc_pdtof_get_tune(pdsvc_info->cur_mif,
					arg + OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune),
					p_len - OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune));
			} else
				err = BCME_UNSUPPORTED;
#endif /* WL_TOF */
		} else if (pdsvc_info->ftm && pdsvc_info->tunep) {
#ifdef WL_FTM
			err = pdburst_get_tune(wlc, pdsvc_info->tunep,
				((uint8*) arg + OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune)),
				p_len - OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune));
#endif /* WL_FTM */
		}
		else
			err = BCME_NOTREADY;
		break;
	case IOV_SVAL(IOV_PROXD_TUNE):
		if (p_len >= (uint)sizeof(method))
			bcopy(params, &method, sizeof(method));

		if (pdsvc_info->cur_mif) {
#ifdef WL_TOF
			/* proximity detection should be in idle state */
			if (pdsvc_info->state != 0)
				return BCME_EPERM;

			/* must be a vallid method and == to created method  */
			if (method == 0 || method > PROXD_MAX_METHOD_NUM ||
				pdsvc_info->method != method) {
				PROXD_TRACE((" cmd mth:%d, svc cur:%d\n", method,
					pdsvc_info->method));
				return BCME_BADARG;
			}

			if (method == PROXD_TOF_METHOD)
				err = wlc_pdtof_set_tune(pdsvc_info->cur_mif,
					params + OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune),
					p_len - OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune));
			else
				err = BCME_UNSUPPORTED;
#endif /* WL_TOF */
		} else if (pdsvc_info->ftm && pdsvc_info->tunep) {
#ifdef WL_FTM
			err = pdburst_set_tune(pdsvc_info->tunep,
				((uint8*) params + OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune)),
				p_len - OFFSETOF(wl_proxd_params_iovar_t, u.tof_tune));
#endif /* WL_FTM */
		}
		else
			err = BCME_NOTREADY;
		break;
#ifdef WL_TOF
	case IOV_GVAL(IOV_FTM_PERIOD):
		if (pdsvc_info->method == PROXD_TOF_METHOD) {
			int val = wlc_pdtof_get_ftmperiod(pdsvc_info->cur_mif);
			if (val >= 0) {
				*((uint32 *)arg) = (uint32)val;
				break;
			}
		}
		err = BCME_UNSUPPORTED;
		break;

	case IOV_SVAL(IOV_FTM_PERIOD):
		if (pdsvc_info->method == PROXD_TOF_METHOD) {
			uint32 val = 0;
			if (p_len >= sizeof(uint32))
				bcopy(params, &val, sizeof(val));
			if (wlc_pdtof_set_ftmperiod(pdsvc_info->cur_mif,
				val) == BCME_OK)
				break;
		}
		err = BCME_UNSUPPORTED;
		break;

#ifdef WL_PROXD_AVB_TS
	case IOV_GVAL(IOV_AVB_LOCAL_TIME):
		if (p_len < sizeof(uint32))
			return BCME_BUFTOOSHORT;

		/* proximity detection should be in idle state */
		if (pdsvc_info->state != 0)
			return BCME_EPERM;

		wlc_hw = wlc->hw;
		osh = wlc_hw->osh;
		regs = wlc_hw->regs;

		/* Read the clock state and MAC control registers */
		wlc_get_avb_timer_reg(wlc->hw, &clkst, &macctrl1);
		wlc_enable_avb_timer(wlc->hw, TRUE);
		if (macctrl1 & (1 << 2)) {
			macctrl1 &= ~(1 << 2);
			W_REG(osh, D11_MacControl1(wlc), macctrl1);
			macctrl1 |= (1 << 2);
			W_REG(osh, D11_MacControl1(wlc), macctrl1);
			tx = R_REG(osh, D11_AvbTxTimeStamp(wlc));
		} else {
			macctrl1 |= (1 << 2);
			W_REG(osh, D11_MacControl1(wlc), macctrl1);
			tx = R_REG(osh, D11_AvbTxTimeStamp(wlc));
			macctrl1 &= ~(1 << 2);
			W_REG(osh, D11_MacControl1(wlc), macctrl1);
		}

		((uint32 *)arg)[0] = tx;
		break;
#endif /* WL_PROXD_AVB_TS */
#endif /* WL_TOF */
#ifdef TOF_DBG_SEQ
	case IOV_GVAL(IOV_TOF_SEQ):
#ifdef WL_TOF
		if (pdsvc_info->method == PROXD_TOF_METHOD)
			wlc_tof_dbg_seq_iov(pdsvc_info->cur_mif, 0, (int*)arg);
		else
			err = BCME_UNSUPPORTED;
#endif // endif
#ifdef WL_FTM
		err = wlc_tof_dbg_seq_iov(wlc, 0, (int*)arg);
#endif // endif
		break;
	case IOV_SVAL(IOV_TOF_SEQ):
#ifdef WL_TOF
		if (pdsvc_info->method == PROXD_TOF_METHOD)
			err = wlc_tof_dbg_seq_iov(pdsvc_info->cur_mif, *((int32*)params), NULL);
		else
			err = BCME_UNSUPPORTED;
#endif // endif
#ifdef WL_FTM
		err = wlc_tof_dbg_seq_iov(wlc, *((int32*)params), NULL);
#endif // endif
		break;
#endif	/* TOF_DBG_SEQ */

	case IOV_GVAL(IOV_PROXD_REPORT):
		bzero(arg, ETHER_ADDR_LEN * WL_PROXD_MAXREPORT);
		if (pdsvc_info->rptlistnum)
			bcopy(pdsvc_info->rptlist, arg, ETHER_ADDR_LEN * pdsvc_info->rptlistnum);
		break;

	case IOV_SVAL(IOV_PROXD_REPORT):
		if (pdsvc_info->rptlist)
			MFREE(wlc->osh, pdsvc_info->rptlist,
				pdsvc_info->rptlistnum * ETHER_ADDR_LEN);
		pdsvc_info->rptlist = NULL;
		pdsvc_info->rptlistnum = 0;
		if (p_len > 0) {
			if (ETHER_ISNULLADDR(arg))
				break;
			pdsvc_info->rptlist = MALLOC(wlc->osh, p_len);
			if (!pdsvc_info->rptlist)
				return BCME_NOMEM;
			bcopy(arg, pdsvc_info->rptlist, p_len);
			pdsvc_info->rptlistnum = p_len / ETHER_ADDR_LEN;
		}
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

#ifdef WL_FTM
done:
#endif /* WL_FTM */
	return err;
}

#if defined(WL_TOF)
/* Provides the call back to method to transmit the action frames */
static int
wlc_proxd_transmitaf(wlc_pdsvc_info_t* pdsvc, wl_action_frame_t *af,
	ratespec_t rate_override, pkcb_fn_t fn, struct ether_addr *selfea)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;
	bss_proxd_cubby_t *proxd_bsscfg_cubby;
	struct scb *scb;
	struct ether_addr *bssid;
	uint16 method;
	chanspec_t chanspec;
	uint8* pbody;
	wlc_pkttag_t *pkttag;
	void *pkt;

	ASSERT(pdsvc != NULL);
	ASSERT(af != NULL);

	wlc = pdsvc->wlc;

	if (pdsvc->bsscfg) {
		bsscfg = pdsvc->bsscfg;
	} else {
		bsscfg = wlc_bsscfg_find_by_hwaddr(wlc, selfea);
	}
	if (bsscfg == NULL) {
		WL_ERROR(("%s: Bsscfg Iteration Failed\n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (ETHER_ISNULLADDR(&af->da))
		bcopy(&pdsvc->config.mcastaddr, &af->da, ETHER_ADDR_LEN);
	if (ETHER_ISNULLADDR(&bsscfg->BSSID))
		bssid = &pdsvc->config.bssid;
	else
		bssid = &bsscfg->BSSID;

	/* get allocation of action frame */
	if ((pkt = wlc_frame_get_action(wlc, &af->da, &bsscfg->cur_etheraddr,
		bssid, af->len, &pbody, DOT11_ACTION_CAT_VS)) == NULL) {
		return BCME_NOMEM;
	}

	pkttag = WLPKTTAG(pkt);
	pkttag->shared.packetid = af->packetId;
	WLPKTTAGBSSCFGSET(pkt, bsscfg->_idx);

	/* copy action frame payload */
	bcopy(af->data, pbody, af->len);

	/* Need to set a proper scb in action frame transmission so that lower layer
	   functions can have a correct reference to scb and bsscfg. If scb is not
	   provided on wlc_queue_80211_frag(), it internally uses the default scb
	   which points to a wrong bsscfg.
	*/
	method = pdsvc->method;
	ASSERT(method != 0 && method <= PROXD_MAX_METHOD_NUM);

	/* read chanspec from current method  */
	ASSERT(pdsvc->cur_mif->params_ptr != NULL);
	chanspec = pdsvc->cur_mif->params_ptr->chanspec;

	proxd_bsscfg_cubby = (bss_proxd_cubby_t *)PROXD_BSSCFG_CUBBY(pdsvc, bsscfg);

	if (method == PROXD_RSSI_METHOD)
		mboolset(proxd_bsscfg_cubby->flags, PROXD_FLAG_TXPWR_OVERRIDE);
	else
		mboolclr(proxd_bsscfg_cubby->flags, PROXD_FLAG_TXPWR_OVERRIDE);

	/* Getting bcmc scb for bsscfg */
	if (method == PROXD_RSSI_METHOD) {
		scb = bsscfg->bcmc_scb;
		ASSERT(scb != NULL);
	} else {
		scb = NULL;
	}

	if (fn) {
		wlc_pcb_fn_register(wlc->pcb, fn, pdsvc->cur_mif, pkt);
	}

	/* put into queue and then transmit */
	if (!wlc_queue_80211_frag(wlc, pkt, wlc->active_queue, scb, bsscfg, FALSE, NULL,
		rate_override))
		return BCME_ERROR;

	/* WLF2_PCB1_AF callback is not needed because the action frame was not
	 * initiated from Host. More importantly, queueing up WLC_E_ACTION_FRAME_COMPLETE event
	 * which would be done in the callback would keep the device from going into sleep.
	 */

	return BCME_OK;
}

/* This is a notify call back to the method to inform DHD on proximity detection */
static int
wlc_proxd_notify(void *ctx, struct ether_addr *ea, uint result, uint status,
	uint8 *body, int body_len)
{
	wlc_pdsvc_info_t* pdsvc = ctx;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;

	PROXD_TRACE(("%s result:%d status :%d\n",
		__FUNCTION__, result, status));

	ASSERT(pdsvc != NULL);

	wlc = pdsvc->wlc;
	bsscfg = pdsvc->bsscfg;

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_PROXD, ea, result, status, 0,
		body, body_len);

	return BCME_OK;
}
#endif /* WL_TOF */
#if defined(WL_FTM) || defined(WL_TOF)
/* Get AVB clock factor
 * AVB timer factor  = (2 * Divior * 1000000)/VCO.
 * The factor for 4335b0 and 4335c0 is 6.19834710... to keep good accuracy. Left Shift it 15 bit.
 * After calculation, right shift the result 15 bit.
*/
static uint32
wlc_proxd_AVB_clock_factor(wlc_pdsvc_info_t* pdsvc, uint8 shift, uint32 *ki, uint32 *kt)
{
	uint32 factor;

	ASSERT(pdsvc != NULL);

	if ((CHIPID(pdsvc->wlc->pub->sih->chip)) == BCM4360_CHIP_ID ||
		(CHIPID(pdsvc->wlc->pub->sih->chip)) == BCM4352_CHIP_ID ||
		(CHIPID(pdsvc->wlc->pub->sih->chip)) == BCM43460_CHIP_ID ||
		(CHIPID(pdsvc->wlc->pub->sih->chip)) == BCM43602_CHIP_ID ||
		(CHIPID(pdsvc->wlc->pub->sih->chip)) == BCM4347_CHIP_ID) {
		factor = ((pdsvc->pllreg * 1000) << shift);
	} else {
		factor = (((pdsvc->pllreg & PMU1_PLL0_PC1_M1DIV_MASK) * 1000 * 2) << shift);
	}
	factor = factor / pdsvc->fvco;
	if (ki && pdsvc->ki)
		*ki = pdsvc->ki;
	if (kt && pdsvc->kt)
		*kt = pdsvc->kt;

	PROXD_TRACE(("Shift:%d, pllreg:%x , fvco:%d, factor:%d\n",
		shift, pdsvc->pllreg, pdsvc->fvco, factor));

	return factor;
}
#endif /* WL_FTM || WL_TOF */
wlc_pdsvc_info_t *
BCMATTACHFN(wlc_proxd_attach)(wlc_info_t *wlc)
{
	wlc_pdsvc_info_t *pdsvc = NULL;
	int err;

	ASSERT(wlc != NULL);

	/* Allocate heap space for wlc_pdsvc_info_t */
	pdsvc = MALLOC(wlc->osh, sizeof(wlc_pdsvc_info_t));
	if (pdsvc == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC allocation is failed %d bytes \n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		goto fail;
	}

	/* Proximity detection is disabled in default */
	wlc->pub->_proxd = FALSE;

	/* Clear the allocated space */
	bzero(pdsvc, sizeof(wlc_pdsvc_info_t));
	ASSIGN_SIGNATURE(pdsvc, WLC_PDSVC_SIGNATURE);

	/* save the wlc reference */
	pdsvc->wlc = wlc;
#if defined(WL_TOF)
	/* Hook up the callback interfaces */
	pdsvc->funcs.txaf = wlc_proxd_transmitaf;
	pdsvc->funcs.notify = wlc_proxd_notify;
	pdsvc->funcs.clock_factor = wlc_proxd_AVB_clock_factor;
	pdsvc->funcs.notifyptr = pdsvc;

	/*  attach create_fn for currrently implemented PD methods  */
	pdsvc->method_create_fn[PROXD_RSSI_METHOD-1] = NULL;
	pdsvc->method_create_fn[PROXD_TOF_METHOD-1] =
		wlc_pdtof_create_method;
	/* TODO: pdsvc->method_create_fn[PROXD_AOA_METHOD-1] = wlc_pdaoa_create_method;  */

	/* LEGACY stuff, TODO: move rssi related init into the pdrssi module */
	wlc_pdsvc_init(pdsvc);
#endif /* WL_TOF */
	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((pdsvc->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(bss_proxd_cubby_t),
		wlc_proxd_bsscfg_cubby_init, wlc_proxd_bsscfg_cubby_deinit,
		wlc_proxd_bsscfg_cubby_dump, pdsvc)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		    wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Provide wlc_proxd_stop() for wl down callback, so that
	 * the proximity detection to be stopped upon diver down.
	 * This should be done along with PM mode implementation,
	 * otherwise the proximity detection will be stopped on
	 * entering sleep due to MPC.
	 */
	err = wlc_module_register(wlc->pub, wlc_proxd_iovars, PROXD_NAME, (void *)pdsvc,
		wlc_proxd_doiovar, NULL, NULL, NULL);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed with status %d\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	err = si_pmu_fvco_pllreg(wlc->hw->sih, &pdsvc->fvco, &pdsvc->pllreg);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: get fvco failed with error %d\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

#ifdef WL_FTM
	pdsvc->ftm = pdftm_attach(wlc, pdsvc);
	if (!pdsvc->ftm)  /* callee logged failure */
		goto fail;

	wlc->pub->_proxd = TRUE;

	err = wlc_bsscfg_updown_register(wlc, proxd_bss_updown, pdsvc);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: bsscfg up/down register failed with error %d\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}

	pdsvc->tunep = proxd_init_tune(wlc);
	if (!pdsvc->tunep) {
		WL_ERROR(("wl%d: %s: malloc tune failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	err = wlc_dump_register(wlc->pub, PROXD_NAME, (dump_fn_t)proxd_dump, pdsvc);
	if (err != BCME_OK) {
		WL_ERROR(("wl%d: %s: dump register failed with error %d\n",
			wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
#endif /* BCMDBG || BCMDBG_DUMP */

#if defined(WLAWDL)
	if (AWDL_SUPPORT(wlc->pub)) {
		if (pdsvc != NULL) {
			err = wlc_awdl_st_notif_register(wlc->awdl_info,
				(void *)pdftm_sched_awdl_cb, NULL);
		}
		/* ext sched registration fails */
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s awdl register failed with error %d\n",
				wlc->pub->unit, __FUNCTION__, err));
			goto fail;
		}
	}
#endif /* WLAWDL */
#ifdef WLSLOTTED_BSS
	if (SLOTTED_BSS_SUPPORT(wlc->pub)) {
		if (pdsvc != NULL) {
			err = wlc_slotted_bss_st_notif_register(wlc->sbi,
					(void *)pdftm_ext_sched_cb, NULL);
		}
		/* ext sched registration fails */
		if (err != BCME_OK) {
			WL_ERROR(("wl%d: %s nan register failed with error %d\n",
				wlc->pub->unit, __FUNCTION__, err));
			goto fail;
		}
	}
#endif /* WLSLOTTED_BSS */

#endif /* WL_FTM */

	return pdsvc;

fail:
	if (pdsvc != NULL) {
		(void)wlc_module_unregister(wlc->pub, PROXD_NAME, pdsvc);
		if (pdsvc->tunep)
			MFREE(wlc->osh, pdsvc->tunep, sizeof(wl_proxd_params_tof_tune_t));
		MFREE(wlc->osh, pdsvc, sizeof(wlc_pdsvc_info_t));
	}
	return NULL;
}

/* Detach the proximity service from WLC */
int
BCMATTACHFN(wlc_proxd_detach) (wlc_pdsvc_info_t *const pdsvc)
{
	int callbacks = 0;
	wlc_info_t *wlc;
	if (pdsvc == NULL)
		return callbacks;

	CHECK_SIGNATURE(pdsvc, WLC_PDSVC_SIGNATURE);
	wlc = pdsvc->wlc;

	ASSIGN_SIGNATURE(pdsvc, 0);

#if defined(WL_TOF)
	/* Stop proximity */
	(void) wlc_proxd_stop((void *)pdsvc);
#endif // endif

#ifdef WL_FTM
	pdftm_detach(&pdsvc->ftm);
#endif /* WL_FTM */

	/* This is just to clean up the memory if unloading happens before disabling the method */
	if (pdsvc->cur_mif) {
		(*pdsvc->cur_mif->mrelease)(pdsvc->cur_mif);
	}
#if defined(WL_TOF)
	if (pdsvc->bsscfg) {
		wlc_proxd_ifdel(pdsvc);
	}
#endif /* WL_TOF */
	wlc_module_unregister(wlc->pub, "proxd", pdsvc);

	if (pdsvc->tunep)
		MFREE(wlc->osh, pdsvc->tunep, sizeof(wl_proxd_params_tof_tune_t));
	MFREE(wlc->osh, pdsvc, sizeof(wlc_pdsvc_info_t));

#if defined(WLAWDL) && defined(WL_FTM)
	(void)wlc_awdl_st_notif_unregister(wlc->awdl_info, (void *)pdftm_sched_awdl_cb, NULL);
#endif /* WLAWDL && WL_FTM */
#if defined(WLSLOTTED_BSS) && defined(WL_FTM)
	(void)wlc_slotted_bss_st_notif_unregister(wlc->sbi, pdftm_ext_sched_cb, NULL);
#endif // endif

	return ++callbacks;
}

/* wlc calls to receive the action frames */
int
wlc_proxd_recv_action_frames(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, uint8 *body, int body_len,
	wlc_d11rxhdr_t *wrxh, uint32 rspec)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);
	ASSERT(body != NULL);
	BCM_REFERENCE(bsscfg);

	pdsvc = wlc->pdsvc_info;
	if (!pdsvc)
		return BCME_OK;
#ifdef WL_FTM
	if (pdftm_is_ftm_action(pdsvc->ftm, hdr, body, body_len)) {
		if (bsscfg == NULL) {
			bsscfg = pdsvc->bsscfg;
		}

		(void)pdftm_rx(pdsvc->ftm, bsscfg, hdr, body, body_len, wrxh, rspec);
		goto done;
	}
	else if (pdftm_vs_is_ftm_action(pdsvc->ftm, hdr, body, body_len)) {
		(void) pdftm_vs_rx_frame(pdsvc->ftm, bsscfg == NULL ?  pdsvc->bsscfg : bsscfg,
			hdr, body, body_len, wrxh, rspec);
		goto done;
	}
#endif /* WL_FTM */

#if defined(WL_TOF)
	/* Push the frame to method */
	if (pdsvc->cur_mif && pdsvc->cur_mif->mpushaf)
		(*pdsvc->cur_mif->mpushaf)(pdsvc->cur_mif, &hdr->sa, &hdr->da, wrxh,
			body, body_len, rspec);
#endif // endif
#ifdef WL_FTM
done:
#endif /* WL_FTM */

	return BCME_OK;
}

bool wlc_proxd_frame(wlc_info_t *wlc, wlc_pkttag_t *pkttag)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);

	pdsvc = wlc->pdsvc_info;

	if (pdsvc && pkttag &&
		((pkttag->shared.packetid & 0xffff0000) == (PROXD_FTM_PACKET_TAG |
		PROXD_MEASUREMENT_PKTID)))
	{
		return TRUE;
	}

	return FALSE;
}

void wlc_proxd_tx_conf(wlc_info_t *wlc, uint16 *phyctl, uint16 *mch, wlc_pkttag_t *pkttag)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);

	pdsvc = wlc->pdsvc_info;
	uint8 core_shift = (pdsvc->tunep->core == 255) ? 0 : pdsvc->tunep->core;

	if (pdsvc && pkttag &&
		((pkttag->shared.packetid & 0x7fff0000) == PROXD_FTM_PACKET_TAG))
	{
		uint16 mask;
#if defined(WL_TOF)
		/* measurement packet using one antenna to tx */
		if (pdsvc->cur_mif)
			mask = (wlc_pdtof_get_tx_mask(pdsvc->cur_mif) << D11AC_PHY_TXC_CORE_SHIFT);
		else
#endif // endif
			mask = (1 << core_shift) << D11AC_PHY_TXC_CORE_SHIFT;
		*phyctl = (*phyctl & ~D11AC_PHY_TXC_CORE_MASK) | mask;

		if (pkttag->shared.packetid & PROXD_MEASUREMENT_PKTID) {
			/* signal ucode to enable timestamping for this frame */
			*mch |= D11AC_TXC_TOF;
		}
	}
}

static uint16 wlc_proxd_get_tx_subband(wlc_info_t * wlc, chanspec_t chanspec)
{
	uint16 subband = WL_CHANSPEC_CTL_SB_LLL;
	uint16 ichan, tchan;

	if (CHSPEC_IS80(WLC_BAND_PI_RADIO_CHANSPEC)) {
		/* Target is 80M band */
		tchan = CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC);
		ichan = CHSPEC_CHANNEL(chanspec);
		if (CHSPEC_IS20(chanspec)) {
			/* Initiator is 20 band */
			if (ichan + 2 == tchan)
				subband = WL_CHANSPEC_CTL_SB_LLU;
			else if (ichan == tchan + 2)
				subband = WL_CHANSPEC_CTL_SB_LUL;
			else if (ichan == tchan + 6)
				subband = WL_CHANSPEC_CTL_SB_LUU;
		}
		else if (CHSPEC_IS40(chanspec)) {
			subband = chanspec & WL_CHANSPEC_CTL_SB_MASK;
		}
	} else if (CHSPEC_IS40(WLC_BAND_PI_RADIO_CHANSPEC)) {
		tchan = CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC);
		if (CHSPEC_IS20(chanspec)) {
			ichan = CHSPEC_CHANNEL(chanspec);
			if (ichan == tchan + 2)
				subband = WL_CHANSPEC_CTL_SB_LLU;
		}
	}
	return subband >> WL_CHANSPEC_CTL_SB_SHIFT;
}

void wlc_proxd_tx_conf_subband(wlc_info_t *wlc, uint16 *phyctl, wlc_pkttag_t *pkttag)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);

	pdsvc = wlc->pdsvc_info;

	if (pdsvc && pkttag &&
		((pkttag->shared.packetid & 0x7fff0000) == PROXD_FTM_PACKET_TAG)) {
		uint16 subband;
		chanspec_t chspec = pkttag->shared.packetid & 0xffff;
		subband = wlc_proxd_get_tx_subband(wlc, chspec) & D11AC_PHY_TXC_PRIM_SUBBAND_MASK;
		*phyctl = (*phyctl & ~WL_CHANSPEC_CTL_SB_MASK) |
			(subband << WL_CHANSPEC_CTL_SB_SHIFT);
	}
}

uint32 wlc_pdsvc_sqrt(uint32 x)
{
	int i;
	uint32 answer = 0, old = 1;
		i = 0;
	while (i < 100) {
		answer = (old + (x / old)) >> 1;
		if (answer == old-1 || answer == old)
			break;
		old = answer;
		i++;
	}
	return answer;
}

uint32 wlc_pdsvc_average(uint32 *arr, int n)
{
	int total;
	int i;
	uint32 ret;

	if (n > 1) {
		i = 1;
		total = 0;
		while (i < n)
			total += (arr[i++]-arr[0]);
		total = total*100/n;
		ret = total/100+arr[0];
		if (total%100 >= 50)
			ret++;
		return ret;
	} else if (n == 1)
		return arr[0];
	return 0;
}

uint32 wlc_pdsvc_deviation(uint32 *arr, int n, uint8 decimaldigits)
{
	uint32 sum = 0, mean;
	int i, diff;

	if (n == 0)
		return 0;
	if (decimaldigits > 3)
		decimaldigits = 3;
	mean = wlc_pdsvc_average(arr, n);
	for (i = 0; i < n; i++) {
		diff = arr[i] - mean;
		sum += diff * diff;
	}
	for (i = 0; i < decimaldigits; i++)
		sum *= 100;

	return wlc_pdsvc_sqrt(sum/n);
}

#if defined(WL_TOF)
static int wlc_pdsvc_func(wlc_info_t *wlc, uint8 action, chanspec_t chanspec,
	struct ether_addr *addr, int8 frmcnt, int8 retrycnt, int timeout, uint32 flags)
{
	wlc_pdsvc_info_t* pdsvc;
	wl_proxd_params_tof_method_t tofparam;
	wl_proxd_params_tof_tune_t toftune;
	int ret = 0;

	ASSERT(wlc != NULL);
	ASSERT(wlc->pdsvc_info != NULL);

	pdsvc = wlc->pdsvc_info;
	if (pdsvc && pdsvc->cur_mif) {
		if (action == WL_PROXD_ACTION_START) {
			pdsvc->cur_mif->rw_params(pdsvc->cur_mif, &tofparam,
				sizeof(wl_proxd_params_tof_method_t), 0);
			tofparam.chanspec = chanspec;
			if (timeout != -1)
				tofparam.timeout = timeout;
			if (frmcnt != -1)
				tofparam.ftm_cnt = frmcnt;
			if (retrycnt != -1)
				tofparam.retry_cnt = retrycnt;
			bcopy(addr, &tofparam.tgt_mac, ETHER_ADDR_LEN);
			if (flags & WL_PROXD_FLAG_ONEWAY) {
				/* One side using 6M legacy rate */
				tofparam.tx_rate = 12;
				tofparam.vht_rate = WL_RSPEC_ENCODE_RATE >> 16;
			} else {
				tofparam.tx_rate = 1 << WL_RSPEC_VHT_NSS_SHIFT;
				tofparam.vht_rate = WL_RSPEC_ENCODE_VHT >> 16;
			}
			pdsvc->cur_mif->rw_params(pdsvc->cur_mif, &tofparam,
				sizeof(wl_proxd_params_tof_method_t), 1);
			wlc_pdtof_get_tune(pdsvc->cur_mif, &toftune,
				sizeof(wl_proxd_params_tof_tune_t));
			toftune.flags = flags;
			if (toftune.flags & WL_PROXD_FLAG_SEQ_EN)
			{
				toftune.seq_en = 1;
			} else {
				toftune.seq_en = 0;
			}

			wlc_pdtof_set_tune(pdsvc->cur_mif, &toftune,
				sizeof(wl_proxd_params_tof_tune_t));
		}

		(*pdsvc->cur_mif->mconfig)(pdsvc->cur_mif,
			WL_PROXD_MODE_INITIATOR, pdsvc->bsscfg);
		ret = (*pdsvc->cur_mif->mstart)(pdsvc->cur_mif, (action != WL_PROXD_ACTION_STOP));
	}
	return ret;
}

pdsvc_func_t wlc_pdsvc_register(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, notifypd notify,
	void *notifyptr, int8 fmtcnt, struct ether_addr *allow_addr, bool setonly, uint32 flags)
{
	wlc_pdsvc_info_t* pdsvc;
	wl_proxd_params_tof_method_t tofparam;
	wl_proxd_params_tof_tune_t toftune;

	ASSERT(wlc != NULL);
	ASSERT(wlc->pdsvc_info != NULL);

	pdsvc = wlc->pdsvc_info;
	pdsvc->bsscfg = bsscfg;
	if (notify) {
		pdsvc->funcs.notify = notify;
		pdsvc->funcs.notifyptr = notifyptr;
	}

	if (!setonly && !(pdsvc->cur_mif = (*pdsvc->method_create_fn[PROXD_TOF_METHOD-1])(
			wlc, WL_PROXD_MODE_TARGET, &pdsvc->funcs, NULL,
			&pdsvc->payload))) {
		WL_ERROR(("wl%d: %s: Create TOF method failed \n",
			wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	/* Configure  created method */
	if (!setonly && pdsvc->cur_mif->mconfig) {
		/* Call the method configuration */
		(*pdsvc->cur_mif->mconfig)(pdsvc->cur_mif,
			WL_PROXD_MODE_TARGET, pdsvc->bsscfg);
	}

	pdsvc->cur_mif->rw_params(pdsvc->cur_mif, &tofparam,
		sizeof(wl_proxd_params_tof_method_t), 0);
	if (fmtcnt != -1)
		tofparam.ftm_cnt = fmtcnt;
	tofparam.tx_rate = 1 << WL_RSPEC_VHT_NSS_SHIFT;
	tofparam.vht_rate = WL_RSPEC_ENCODE_VHT >> 16;
	pdsvc->cur_mif->rw_params(pdsvc->cur_mif, &tofparam,
		sizeof(wl_proxd_params_tof_method_t), 1);

	wlc_pdtof_get_tune(pdsvc->cur_mif, &toftune, sizeof(wl_proxd_params_tof_tune_t));
	toftune.vhtack = 1;
	if (fmtcnt != -1 && fmtcnt != 0)
		toftune.totalfrmcnt = fmtcnt+1; /* limit total frames */
	toftune.flags = flags;
	wlc_pdtof_set_tune(pdsvc->cur_mif, &toftune, sizeof(wl_proxd_params_tof_tune_t));

	wlc_pdtof_allowmac(pdsvc->cur_mif, allow_addr);

	pdsvc->method = PROXD_TOF_METHOD;
	pdsvc->config.mode = WL_PROXD_MODE_TARGET;

	wlc->pub->_proxd = TRUE;

	if (!setonly)
		(*pdsvc->cur_mif->mstart)(pdsvc->cur_mif, TRUE);

	return wlc_pdsvc_func;
}

int wlc_pdsvc_deregister(wlc_info_t *wlc, pdsvc_func_t funcp)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);
	ASSERT(wlc->pdsvc_info != NULL);

	pdsvc = wlc->pdsvc_info;
	if (pdsvc->cur_mif && funcp) {
		/* Stop proximity */
		wlc_proxd_stop((void *)pdsvc);

		/* Release the method */
		(*pdsvc->cur_mif->mrelease)(pdsvc->cur_mif);
		pdsvc->cur_mif = NULL;
	}

	/* Initiatize all the parameters to NULL */
	pdsvc->method = PROXD_UNDEFINED_METHOD;
	pdsvc->config.mode = WL_PROXD_MODE_DISABLE;
	wlc->pub->_proxd = FALSE;

	return 0;
}
#endif /* WL_TOF */

/* function to determine if the proxd is supported by the radio card */
bool wlc_is_proxd_supported(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw;

	ASSERT(wlc != NULL);
	wlc_hw = wlc->hw;

	if (D11REV_GE(wlc_hw->corerev, 42)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

uint32 proxd_get_ratespec_idx(ratespec_t rspec, ratespec_t ackrspec)
{
	uint32 idx = 0;

	if (RSPEC_ISLEGACY(rspec)) {
		if (RSPEC2RATE(rspec) == WLC_RATE_6M)
			idx = WL_PROXD_RATE_6M;
		else
			idx = WL_PROXD_RATE_LEGACY;
	} else if (RSPEC_ISHT(rspec)) {
		if (wlc_ratespec_mcs(rspec) > 0)
			idx = WL_PROXD_RATE_MCS;
		else
			idx = WL_PROXD_RATE_MCS_0;
	}

	if (RSPEC_ISLEGACY(ackrspec)) {
		idx |= (WL_PROXD_RATE_6M << 8);
	}

	return idx;
}

#if defined(WL_FTM)

/* internal interface */
wlc_bsscfg_t *
pdsvc_get_bsscfg(wlc_pdsvc_info_t *pdsvc)
{
	wlc_info_t *wlc;

	ASSERT(pdsvc != NULL);
	wlc = pdsvc->wlc;
	if (!pdsvc->bsscfg) {
		pdsvc->bsscfg = wlc->cfg;
	}
	return pdsvc->bsscfg;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
void
proxd_dump(wlc_pdsvc_info_t *pdsvc, struct bcmstrbuf *b)
{
	/* tbd */
	ASSERT(pdsvc != NULL && pdsvc->ftm != NULL);
	pdftm_dump(pdsvc->ftm, b);
}
#endif /* BCMDBG || BCMDBG_DUMP */

static void
proxd_bss_updown(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_pdsvc_info_t *pdsvc = (wlc_pdsvc_info_t *)ctx;

	ASSERT(pdsvc != NULL && pdsvc->ftm != NULL);
	ASSERT(evt != NULL && evt->bsscfg != NULL);
	ASSERT(WL_PROXD_EVENT_NONE == 0);

	pdftm_notify(pdsvc->ftm, evt->bsscfg,
		(evt->up ? PDFTM_NOTIF_BSS_UP : PDFTM_NOTIF_BSS_DOWN), NULL);
}

void
proxd_init_event(wlc_pdsvc_info_t *pdsvc, wl_proxd_event_type_t type,
	wl_proxd_method_t method, wl_proxd_session_id_t sid, wl_proxd_event_t *event)
{
	ASSERT(pdsvc != NULL);
	ASSERT(event != NULL);

	BCM_REFERENCE(pdsvc);

	event->version = htol16(WL_PROXD_API_VERSION);
	event->len = htol16(OFFSETOF(wl_proxd_event_t, tlvs));
	event->type = htol16(type);
	event->method = htol16(method);
	event->sid = htol16(sid);
	memset(event->pad, 0, sizeof(event->pad));
}

void
proxd_send_event(wlc_pdsvc_info_t *pdsvc, wlc_bsscfg_t *bsscfg, wl_proxd_status_t status,
    const struct ether_addr *addr, wl_proxd_event_t *event, uint16 len)
{
	ASSERT(pdsvc != NULL);
	ASSERT(event != NULL);
	ASSERT(len >= OFFSETOF(wl_proxd_event_t, tlvs));

	if (bsscfg == NULL)
		bsscfg = pdsvc->bsscfg;

	event->len = htol16(len);
	wlc_bss_mac_event(pdsvc->wlc, bsscfg, WLC_E_PROXD, addr, WLC_E_STATUS_SUCCESS,
		status, 0 /* auth type */, (uint8 *)event, len);
}

void*
proxd_alloc_action_frame(pdsvc_t *pdsvc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *da, const struct ether_addr *sa,
	const struct ether_addr *bssid, int body_len, uint8 **body,
	uint8 category, uint8 action)
{
	void *pkt;
	dot11_action_frmhdr_t *afhdr;
	scb_t *scb;
	wlc_info_t *wlc;
	uint bandunit;

	ASSERT(pdsvc != NULL);
	ASSERT(bsscfg != NULL);
	ASSERT(body != NULL);

	wlc = pdsvc->wlc;
	if (category == DOT11_ACTION_CAT_VS) {
		ASSERT(body_len >= OFFSETOF(dot11_action_vs_frmhdr_t, data));
	}
	else {
		ASSERT(body_len >= OFFSETOF(dot11_action_frmhdr_t, data));
	}

	if (bsscfg->associated)
		bandunit = CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec);
	else
		bandunit = wlc->band->bandunit;

	/* note: non-bss members must set bssid in public action frame
	 * to wildcard i.e. all 1s - see 11mcd4.0 10.20
	 */
	scb = wlc_scbfindband(wlc, bsscfg, da, bandunit);

	if (scb && SCB_ASSOCIATED(scb)) {
		bssid = &bsscfg->BSSID;
	} else if (ETHER_ISNULLADDR(bssid)) {
		bssid = &ether_bcast;
	}

	pkt = wlc_frame_get_action(wlc, da, sa,
		bssid, body_len, body, category);
	if (!pkt)
		goto done;

	WLPKTTAGBSSCFGSET(pkt, WLC_BSSCFG_IDX(bsscfg));
	WLPKTTAGSCBSET(pkt, scb);

	/*
	 setup non-vendor specific action frame header.
	 Note, vendor-specific Action Frame header should be setup by caller
	*/
	if (category != DOT11_ACTION_CAT_VS) {
		afhdr = (dot11_action_frmhdr_t *) (*body);
		afhdr->category = category;
		afhdr->action = action;
	}

done:
	return pkt;
}

bool
proxd_tx(pdsvc_t *pdsvc, void *pkt, wlc_bsscfg_t *bsscfg, ratespec_t rspec, int status)
{
	wlc_txq_info_t * qi;
	struct scb *scb;

	ASSERT(pdsvc != NULL);
	ASSERT(pkt != NULL);
	ASSERT(bsscfg != NULL);

	scb = WLPKTTAGSCBGET(pkt);

	if (status != WL_PROXD_E_SCAN_INPROCESS && scb && bsscfg->up &&
		(BSSCFG_AP(bsscfg) || bsscfg->associated))
		qi = bsscfg->wlcif->qi;
	else
		qi = pdsvc->wlc->active_queue;

	return wlc_queue_80211_frag(pdsvc->wlc, pkt, qi, scb,
		bsscfg, FALSE, NULL, rspec);
}

static wl_proxd_params_tof_tune_t *proxd_init_tune(wlc_info_t *wlc)
{
	wl_proxd_params_tof_tune_t *tunep;

	tunep = MALLOCZ(wlc->osh, sizeof(wl_proxd_params_tof_tune_t));
	if (tunep) {
		tunep->version = WL_PROXD_TUNE_VERSION_3;
		tunep->N_log2[TOF_BW_20MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_LOG2_20M;
		tunep->N_log2[TOF_BW_40MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_LOG2_40M;
		tunep->N_log2[TOF_BW_80MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_LOG2_80M;
		tunep->N_log2[TOF_BW_SEQTX_INDEX] = TOF_DEFAULT_TX_THRESHOLD_LOG2;
		tunep->N_log2[TOF_BW_SEQRX_INDEX] = TOF_DEFAULT_RX_THRESHOLD_LOG2;
		tunep->N_scale[TOF_BW_20MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_SCALE_20M;
		tunep->N_scale[TOF_BW_40MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_SCALE_40M;
		tunep->N_scale[TOF_BW_80MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_SCALE_80M;
		tunep->N_scale[TOF_BW_SEQTX_INDEX] = TOF_DEFAULT_TX_THRESHOLD_SCALE;
		tunep->N_scale[TOF_BW_SEQRX_INDEX] = TOF_DEFAULT_RX_THRESHOLD_SCALE;
		tunep->ftm_cnt[TOF_BW_20MHZ_INDEX] = TOF_DEFAULT_FTMCNT_20M;
		tunep->ftm_cnt[TOF_BW_40MHZ_INDEX] = TOF_DEFAULT_FTMCNT_40M;
		tunep->ftm_cnt[TOF_BW_80MHZ_INDEX] = TOF_DEFAULT_FTMCNT_80M;
		tunep->ftm_cnt[TOF_BW_SEQTX_INDEX] = TOF_DEFAULT_FTMCNT_SEQ;
		tunep->ftm_cnt[TOF_BW_SEQRX_INDEX] = TOF_DEFAULT_FTMCNT_SEQ;
		tunep->N_log2_2g = TOF_DEFAULT_THRESHOLD_LOG2_2G;
		tunep->N_scale_2g = TOF_DEFAULT_THRESHOLD_SCALE_2G;

		tunep->seq_5g20.N_tx_log2 = TOF_DEFAULT_TX_THRESHOLD_LOG2_5G_20M;
		tunep->seq_5g20.N_tx_scale = TOF_DEFAULT_TX_THRESHOLD_SCALE_5G_20M;
		tunep->seq_5g20.N_rx_log2 = TOF_DEFAULT_RX_THRESHOLD_LOG2_5G_20M;
		tunep->seq_5g20.N_rx_scale = TOF_DEFAULT_RX_THRESHOLD_SCALE_5G_20M;
		tunep->seq_5g20.w_len = TOF_DEFAULT_WINDOW_LEN_5G_20;
		tunep->seq_5g20.w_offset = TOF_DEFAULT_WINDOW_OFFSET_5G_20;

		tunep->sw_adj = TOF_DEFAULT_SW_ADJ;
		tunep->hw_adj = TOF_DEFAULT_HW_ADJ;
		tunep->seq_en = TOF_DEFAULT_SEQ_EN;
		tunep->vhtack = 0;
		tunep->core = TOF_DEFAULT_CORE_SELECTION;

		/* 80MHz */
		tunep->w_len[TOF_BW_80MHZ_INDEX] = 32;
		tunep->w_offset[TOF_BW_80MHZ_INDEX] = 10;
		/* 40MHz */
		tunep->w_len[TOF_BW_40MHZ_INDEX] = 16;
		tunep->w_offset[TOF_BW_40MHZ_INDEX] = 8;
		/* 20MHz */
		tunep->w_len[TOF_BW_20MHZ_INDEX] = 8;
		tunep->w_offset[TOF_BW_20MHZ_INDEX] = 4;

		/* default bitflip and snr thresholds */
		tunep->bitflip_thresh = TOF_DEFAULT_RX_THRESHOLD_BITFLIP;
		tunep->snr_thresh = TOF_DEFAULT_RX_THRESHOLD_SNR;
		tunep->emu_delay = TOF_DEFAULT_EMU_DELAY;
		tunep->core_mask = TOF_DEFAULT_CORE_MASK;
		/* default auto core select GD variance and RSSI thresholds */
		phy_tof_init_gdv_th(WLC_PI(wlc), &(tunep->acs_gdv_thresh));

		tunep->acs_rssi_thresh = TOF_DEFAULT_RX_THRESHOLD_ACS_RSSI;

		/* default smoothing window enable */
		tunep->smooth_win_en = TOF_DEFAULT_RX_SMOOTH_WIN_EN;

		/* default smoothing window enable */
		phy_tof_init_gdmm_th(WLC_PI(wlc), &(tunep->acs_gdmm_thresh));

		tunep->acs_delta_rssi_thresh = TOF_DEFAULT_RX_THRESHOLD_ACS_DELTA_RSSI;
	}
	return tunep;
}

wl_proxd_params_tof_tune_t *proxd_get_tunep(wlc_info_t *wlc, uint64 *tq)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);
	ASSERT(wlc->pdsvc_info != NULL);

	pdsvc = wlc->pdsvc_info;

	if (tq) {
		if (!pdsvc->clkfactor) {
			pdsvc->clkfactor = wlc_proxd_AVB_clock_factor(pdsvc, TOF_SHIFT,
				&pdsvc->tunep->Ki, &pdsvc->tunep->Kt);
		}
		*tq = pdsvc->clkfactor;
	}

	return pdsvc->tunep;
}

void proxd_enable(wlc_info_t *wlc, bool enable)
{
	wlc->pub->_proxd = enable;
}

uint16
proxd_get_tunep_idx(wlc_info_t *wlc, wl_proxd_session_flags_t flags,
	chanspec_t cspec, wl_proxd_params_tof_tune_t **tunepp)
{
	uint16 idx;

	ASSERT(tunepp != NULL);

	if (flags & WL_PROXD_SESSION_FLAG_SEQ_EN) {
		if (flags & WL_PROXD_SESSION_FLAG_INITIATOR)
			idx = TOF_BW_SEQRX_INDEX;
		else
			idx = TOF_BW_SEQTX_INDEX;
	} else if (CHSPEC_IS80(cspec))
		idx = TOF_BW_80MHZ_INDEX;
	else if (CHSPEC_IS40(cspec))
		idx = TOF_BW_40MHZ_INDEX;
	else
		idx = TOF_BW_20MHZ_INDEX;

	*tunepp = proxd_get_tunep(wlc, NULL);
	return idx;
}
#endif /* WL_FTM */

/* update N/S values based on using VHT ACK or not */
void
proxd_update_tunep_values(wl_proxd_params_tof_tune_t *tunep, chanspec_t cspec, bool vhtack)
{
	if (!tunep)
		return;
	if (!(tunep->setflags & WL_PROXD_SETFLAG_N))
	{
		/* the N value based on the channel and rspec for 80/40MHz */
		if (CHSPEC_IS80(cspec)) {
			if (vhtack)
				tunep->N_log2[TOF_BW_80MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_LOG2_80M;
			else
				tunep->N_log2[TOF_BW_80MHZ_INDEX] = TOF_LEGACY_THRESHOLD_LOG2_80M;
		} else if (CHSPEC_IS40(cspec)) {
			if (vhtack)
				tunep->N_log2[TOF_BW_40MHZ_INDEX] = TOF_DEFAULT_THRESHOLD_LOG2_40M;
			else
				tunep->N_log2[TOF_BW_40MHZ_INDEX] = TOF_LEGACY_THRESHOLD_LOG2_40M;
		}
	}
	if (!(tunep->setflags & WL_PROXD_SETFLAG_S))
	{
		/* the S value based on the channel and rspec for 80/40 Mhz */
		if (CHSPEC_IS80(cspec)) {
			if (vhtack)
				tunep->N_scale[TOF_BW_80MHZ_INDEX] =
					TOF_DEFAULT_THRESHOLD_SCALE_80M;
			else
				tunep->N_scale[TOF_BW_80MHZ_INDEX] = TOF_LEGACY_THRESHOLD_SCALE_80M;
		} else if (CHSPEC_IS40(cspec)) {
			if (vhtack)
				tunep->N_scale[TOF_BW_40MHZ_INDEX] =
					TOF_DEFAULT_THRESHOLD_SCALE_40M;
			else
				tunep->N_scale[TOF_BW_40MHZ_INDEX] = TOF_LEGACY_THRESHOLD_SCALE_40M;
		}
	}
}

/* function to get report mac address list */
struct ether_addr *wlc_pdsvc_report_list(wlc_info_t *wlc, int *cntptr)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);
	ASSERT(wlc->pdsvc_info != NULL);

	pdsvc = wlc->pdsvc_info;
	if (cntptr)
		*cntptr = pdsvc->rptlistnum;

	return pdsvc->rptlist;
}

wlc_ftm_t*
wlc_ftm_get_handle(wlc_info_t *wlc)
{
	wlc_pdsvc_info_t* pdsvc;
	wlc_ftm_t *ftm = NULL;

	ASSERT(wlc != NULL);
	pdsvc = wlc->pdsvc_info;
	if (pdsvc)
		ftm = pdsvc->ftm;
	return ftm;
}

void proxd_power(wlc_info_t *wlc, uint8 id, bool on)
{
	wlc_pdsvc_info_t* pdsvc;

	ASSERT(wlc != NULL);
	pdsvc = wlc->pdsvc_info;
	if (!pdsvc || id >= sizeof(pdsvc->pwrflag))
		return;

	if (on) {
		if (!pdsvc->pwrflag) {
			wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_FTM, TRUE);
			wlc_mpc_off_req_set(wlc, MPC_OFF_REQ_FTM_SESSION, MPC_OFF_REQ_FTM_SESSION);
		}
		pdsvc->pwrflag |= (1 << id);
	} else {
		pdsvc->pwrflag &= ~(1 << id);
		if (!pdsvc->pwrflag) {
			wlc_user_wake_upd(wlc, WLC_USER_WAKE_REQ_FTM, FALSE);
			wlc->mpc_delay_off = 0;
			wlc_mpc_off_req_set(wlc, MPC_OFF_REQ_FTM_SESSION, 0);
		}
	}
}

void proxd_undeaf_phy(wlc_info_t *wlc, bool acked)
{
	uint16 shmemptr = wlc_read_shm(wlc, M_TOF_BLK_PTR(wlc)) << 1;
	uint16 rspcmd, i;

	if (acked) {
		/* wait ucode finishing deafing the PHY */
		for (i = 0; i < TOF_MCMD_TIMEOUT; i++) {
			rspcmd = wlc_read_shm(wlc, shmemptr + M_TOF_RSP_OFFSET(wlc));
			if ((rspcmd & TOF_RSP_MASK) == TOF_SUCCESS)
				break;
			OSL_DELAY(1);
		}
	} else {
		/* No Ack, reset ucode state */
		wlc_hw_info_t *wlc_hw = wlc->hw;

		i = 0;
		/* Wait until last command completes */
		while ((R_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw)) & MCMD_TOF) &&
			(i < TOF_MCMD_TIMEOUT)) {
			OSL_DELAY(1);
			i++;
		}

		if (R_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw)) & MCMD_TOF) {
			WL_ERROR(("TOF ucode cmd timeout; maccommand: 0x%x\n",
				(unsigned int)D11_MACCOMMAND(wlc_hw)));
		}

		wlc_write_shm(wlc, shmemptr + M_TOF_CMD_OFFSET(wlc), TOF_RESET);

		W_REG(wlc_hw->osh, D11_MACCOMMAND(wlc_hw), MCMD_TOF);
	}

	/* Now undeaf the PHY */
	phy_tof_cmd(WLC_PI(wlc), FALSE, 0);
}
