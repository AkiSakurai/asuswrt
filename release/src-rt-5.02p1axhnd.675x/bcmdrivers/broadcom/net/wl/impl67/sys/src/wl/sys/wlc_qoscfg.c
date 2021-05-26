/*
 * 802.11 QoS/EDCF/WME/APSD configuration and utility module
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_qoscfg.c 788459 2020-07-01 20:44:38Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_bsscfg.h>
#include <wlc_scb.h>
#include <wlc_bmac.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_helper.h>
#include <wlc_ie_reg.h>
#include <wlc_qoscfg.h>
#include <wlc_pm.h>
#include <wlc_dump.h>
#ifdef PROP_TXSTATUS
#include <wlc_wlfc.h>
#endif // endif
/* iovar table */
enum wlc_qoscfg_iov {
	IOV_WME = 1,
	IOV_WME_BSS_DISABLE = 2,
	IOV_WME_NOACK = 3,
	IOV_WME_APSD = 4,
	IOV_WME_APSD_TRIGGER = 5,	/* APSD Trigger Frame interval in ms */
	IOV_WME_AUTO_TRIGGER = 6,	/* enable/disable APSD AUTO Trigger frame */
	IOV_WME_TRIGGER_AC = 7,		/* APSD trigger frame AC */
	IOV_WME_QOSINFO = 9,
	IOV_WME_DP = 10,
	IOV_WME_COUNTERS = 11,
	IOV_WME_CLEAR_COUNTERS = 12,
	IOV_WME_PREC_QUEUING = 13,
	IOV_WME_TX_PARAMS = 14,
	IOV_WME_MBW_PARAMS = 15,
	IOV_SEND_FRAME = 16,		/* send a frame */
	IOV_WME_AC_STA = 17,
	IOV_WME_AC_AP = 18,
	IOV_EDCF_ACP = 19,		/* ACP used by the h/w */
	IOV_EDCF_ACP_AD = 20,		/* ACP advertised in bcn/prbrsp */
	IOV_WME_DYNAMIC = 21,		/* Dynamic edcf settings in bcn */
	IOV_WME_DYN_IDLE = 22,		/* seconds to cosnider STA idle */
	IOV_LAST
};

static const bcm_iovar_t wme_iovars[] = {
	{"wme", IOV_WME, IOVF_SET_DOWN|IOVF_OPEN_ALLOW, 0, IOVT_INT32, 0},
	{"wme_bss_disable", IOV_WME_BSS_DISABLE, IOVF_OPEN_ALLOW, 0, IOVT_INT32, 0},
	{"wme_noack", IOV_WME_NOACK, IOVF_OPEN_ALLOW, 0, IOVT_BOOL, 0},
	{"wme_apsd", IOV_WME_APSD, IOVF_OPEN_ALLOW, 0, IOVT_BOOL, 0},
#ifdef STA
	{"wme_apsd_trigger", IOV_WME_APSD_TRIGGER,
	IOVF_OPEN_ALLOW|IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT32, 0},
	{"wme_trigger_ac", IOV_WME_TRIGGER_AC, IOVF_OPEN_ALLOW, 0, IOVT_UINT8, 0},
	{"wme_auto_trigger", IOV_WME_AUTO_TRIGGER, IOVF_OPEN_ALLOW, 0, IOVT_BOOL, 0},
	{"wme_qosinfo", IOV_WME_QOSINFO,
	IOVF_BSS_SET_DOWN|IOVF_OPEN_ALLOW|IOVF_BSSCFG_STA_ONLY, 0, IOVT_UINT8, 0},
#endif /* STA */
	{"wme_dp", IOV_WME_DP, IOVF_SET_DOWN|IOVF_OPEN_ALLOW, 0, IOVT_UINT8, 0},
	{"wme_prec_queuing", IOV_WME_PREC_QUEUING, IOVF_OPEN_ALLOW, 0, IOVT_BOOL, 0},
#if defined(WLCNT)
	{"wme_counters", IOV_WME_COUNTERS, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER,
	sizeof(wl_wme_cnt_t)},
	{"wme_clear_counters", IOV_WME_CLEAR_COUNTERS, IOVF_OPEN_ALLOW, 0,
	IOVT_VOID, 0},
#endif // endif
#ifndef WLP2P_UCODE_MACP
	{"wme_ac_sta", IOV_WME_AC_STA, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, sizeof(edcf_acparam_t)},
	{"wme_ac_ap", IOV_WME_AC_AP, 0, 0, IOVT_BUFFER, sizeof(edcf_acparam_t)},
#endif /* WLP2P_UCODE_MACP */
	{"edcf_acp", IOV_EDCF_ACP, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, sizeof(edcf_acparam_t)},
	{"edcf_acp_ad", IOV_EDCF_ACP_AD, IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, sizeof(edcf_acparam_t)},
#if defined(WME_PER_AC_TUNING) && defined(WME_PER_AC_TX_PARAMS)
	{"wme_tx_params", IOV_WME_TX_PARAMS,
	IOVF_OPEN_ALLOW, 0, IOVT_BUFFER, WL_WME_TX_PARAMS_IO_BYTES},
#endif // endif
#ifdef STA
	{"send_frame", IOV_SEND_FRAME, IOVF_SET_UP, 0, IOVT_BUFFER, 0},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

/* module info */
struct wlc_qos_info {
	wlc_info_t *wlc;
};

/** TX FIFO number to WME/802.1E Access Category */
const uint8 wme_fifo2ac[NFIFO_LEGACY] = { AC_BK, AC_BE, AC_VI, AC_VO, AC_BE, AC_BE };

/** WME/802.1E Access Category to TX FIFO number */
const uint8 wme_ac2fifo[AC_COUNT] = {
	TX_AC_BE_FIFO,
	TX_AC_BK_FIFO,
	TX_AC_VI_FIFO,
	TX_AC_VO_FIFO,
};

/* Shared memory location index for various AC params */
#define wme_shmemacindex(ac)	wme_ac2fifo[ac]

const char *const aci_names[] = {"AC_BE", "AC_BK", "AC_VI", "AC_VO"};

static void wlc_edcf_acp_set_sw(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *edcf_acp);
static void wlc_edcf_acp_set_hw(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *edcf_acp);
static void wlc_edcf_acp_ad_get_all(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp);
static int wlc_edcf_acp_ad_set_one(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp);

static const wme_param_ie_t stadef = {
	WME_OUI,
	WME_OUI_TYPE,
	WME_SUBTYPE_PARAM_IE,
	WME_VER,
	0,
	0,
	{
		{ EDCF_AC_BE_ACI_STA, EDCF_AC_BE_ECW_STA, HTOL16(EDCF_AC_BE_TXOP_STA) },
		{ EDCF_AC_BK_ACI_STA, EDCF_AC_BK_ECW_STA, HTOL16(EDCF_AC_BK_TXOP_STA) },
		{ EDCF_AC_VI_ACI_STA, EDCF_AC_VI_ECW_STA, HTOL16(EDCF_AC_VI_TXOP_STA) },
		{ EDCF_AC_VO_ACI_STA, EDCF_AC_VO_ECW_STA, HTOL16(EDCF_AC_VO_TXOP_STA) }
	}
};

/** Initialize a WME Parameter Info Element with default STA parameters from WMM Spec, Table 12 */
static void
wlc_wme_initparams_sta(wlc_info_t *wlc, wme_param_ie_t *pe)
{
	BCM_REFERENCE(wlc);
	STATIC_ASSERT(sizeof(*pe) == WME_PARAM_IE_LEN);
	memcpy(pe, &stadef, sizeof(*pe));
}

static const wme_param_ie_t apdef = {
	WME_OUI,
	WME_OUI_TYPE,
	WME_SUBTYPE_PARAM_IE,
	WME_VER,
	0,
	0,
	{
		{ EDCF_AC_BE_ACI_AP, EDCF_AC_BE_ECW_AP, HTOL16(EDCF_AC_BE_TXOP_AP) },
		{ EDCF_AC_BK_ACI_AP, EDCF_AC_BK_ECW_AP, HTOL16(EDCF_AC_BK_TXOP_AP) },
		{ EDCF_AC_VI_ACI_AP, EDCF_AC_VI_ECW_AP, HTOL16(EDCF_AC_VI_TXOP_AP) },
		{ EDCF_AC_VO_ACI_AP, EDCF_AC_VO_ECW_AP, HTOL16(EDCF_AC_VO_TXOP_AP) }
	}
};

/* Initialize a WME Parameter Info Element with default AP parameters from WMM Spec, Table 14 */
static void
wlc_wme_initparams_ap(wlc_info_t *wlc, wme_param_ie_t *pe)
{
	BCM_REFERENCE(wlc);
	STATIC_ASSERT(sizeof(*pe) == WME_PARAM_IE_LEN);
	memcpy(pe, &apdef, sizeof(*pe));
}

/**
 * Extract the AC info from the wme params IE, then set the info in the appropriate AC shm table
 * locations (per Ling's ecdf_spec.txt rev1.8). 'aci' is one set of the AC params in wme params IE.
 * It also update other WME info in 'cfg' based on the AC info.
 * FIXME:Hardware EDCF function is broken now, so ucode AIFS regs will not be updated.
 */
static void
wlc_edcf_setparams(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint aci, bool suspend)
{
	uint8 i;
	edcf_acparam_t *acp;

	/*
	 * EDCF params are getting updated, so reset the frag thresholds for ACs,
	 * if those were changed to honor the TXOP settings
	 */
	for (i = 0; i < AC_COUNT; i++)
		wlc_fragthresh_set(wlc, i, wlc->usr_fragthresh);

	ASSERT(aci < AC_COUNT);

	acp = &cfg->wme->wme_param_ie.acparam[aci];

	wlc_edcf_acp_set_sw(wlc, cfg, acp);

	/* Only apply params to h/w if the core is out of reset and has clocks */
	if (!wlc->clk || !cfg->associated)
		return;

	if (suspend)
		wlc_suspend_mac_and_wait(wlc);

	wlc_edcf_acp_set_hw(wlc, cfg, acp);

	if (BSSCFG_AP(cfg)) {
		WL_APSTA_BCN(("wl%d.%d: wlc_edcf_setparams() -> wlc_update_beacon()\n",
		              wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		wlc_bss_update_beacon(wlc, cfg);
		wlc_bss_update_probe_resp(wlc, cfg, FALSE);
	}

	if (suspend)
		wlc_enable_mac(wlc);
}

/**
 * Extract one AC param set from the WME IE acparam 'acp' and use them to update WME into in 'cfg'
 */
static void
wlc_edcf_acp_set_sw(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp)
{
	uint aci;
	wlc_wme_t *wme = cfg->wme;

	/* find out which ac this set of params applies to */
	aci = (acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT;
	ASSERT(aci < AC_COUNT);
	/* set the admission control policy for this AC */
	wme->wme_admctl &= ~(1 << aci);
	if (acp->ACI & EDCF_ACM_MASK) {
		wme->wme_admctl |= (1 << aci);
	}
	/* convert from units of 32us to us for ucode */
	wme->edcf_txop[aci] = EDCF_TXOP2USEC(ltoh16(acp->TXOP));

	WL_INFORM(("wl%d.%d: Setting %s: admctl 0x%x txop 0x%x\n",
	           wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
	           aci_names[aci], wme->wme_admctl, wme->edcf_txop[aci]));
}

/** Extract one AC param set from the WME IE acparam 'acp' and plumb them into h/w */
static void
wlc_edcf_acp_set_hw(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp)
{
	uint aci, j;
	shm_acparams_t acp_shm;
	uint16 *shm_entry;
	uint offset;

	bzero((char*)&acp_shm, sizeof(shm_acparams_t));
	/* find out which ac this set of params applies to */
	aci = (acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT;
	ASSERT(aci < AC_COUNT);

	/* fill in shm ac params struct */
	acp_shm.txop = ltoh16(acp->TXOP);
	/* convert from units of 32us to us for ucode */

	acp_shm.txop = EDCF_TXOP2USEC(acp_shm.txop);

	acp_shm.aifs = (acp->ACI & EDCF_AIFSN_MASK);

	if (acp_shm.aifs < EDCF_AIFSN_MIN || acp_shm.aifs > EDCF_AIFSN_MAX) {
		WL_ERROR(("wl%d.%d: %s: Setting %s: bad aifs %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		          aci_names[aci], acp_shm.aifs));
		return;
	}

	/* CWmin = 2^(ECWmin) - 1 */
	acp_shm.cwmin = EDCF_ECW2CW(acp->ECW & EDCF_ECWMIN_MASK);
	/* CWmax = 2^(ECWmax) - 1 */
	acp_shm.cwmax =
	        EDCF_ECW2CW((acp->ECW & EDCF_ECWMAX_MASK) >> EDCF_ECWMAX_SHIFT);
	acp_shm.cwcur = acp_shm.cwmin;
	acp_shm.bslots = R_REG(wlc->osh, D11_TSF_RANDOM(wlc)) & acp_shm.cwcur;
	acp_shm.reggap = acp_shm.bslots + acp_shm.aifs;
	/* Indicate the new params to the ucode */
	offset = M_EDCF_BLKS(wlc) + wme_shmemacindex(aci) * M_EDCF_QLEN(wlc) +
		M_EDCF_STATUS_OFF(wlc);
	acp_shm.status = wlc_read_shm(wlc, offset);
	acp_shm.status |= WME_STATUS_NEWAC;

	/* Fill in shm acparam table */
	shm_entry = (uint16*)&acp_shm;
	for (j = 0; j < (int)sizeof(shm_acparams_t); j += 2) {
		offset = M_EDCF_BLKS(wlc) + wme_shmemacindex(aci) * M_EDCF_QLEN(wlc) + j;
		wlc_write_shm(wlc, offset, *shm_entry++);
	}

	WL_INFORM(("wl%d.%d: Setting %s: txop 0x%x cwmin 0x%x cwmax 0x%x "
	           "cwcur 0x%x aifs 0x%x bslots 0x%x reggap 0x%x status 0x%x\n",
	           wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
	           aci_names[aci], acp_shm.txop, acp_shm.cwmin,
	           acp_shm.cwmax, acp_shm.cwcur, acp_shm.aifs,
	           acp_shm.bslots, acp_shm.reggap, acp_shm.status));
} /* wlc_edcf_acp_set_hw */

/** Extract all AC param sets from WME IE 'ie' and write them into host struct 'acp' */
static void
_wlc_edcf_acp_get_all(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wme_param_ie_t *ie, edcf_acparam_t *acp)
{
	uint i;
	edcf_acparam_t *acp_ie = ie->acparam;
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(cfg);
	for (i = 0; i < AC_COUNT; i++, acp_ie++, acp ++) {
		acp->ACI = acp_ie->ACI;
		acp->ECW = acp_ie->ECW;
		/* convert to host order */
		acp->TXOP = ltoh16(acp_ie->TXOP);
	}
}

/** Extract all AC param sets used by the BSS and write them into host struct 'acp' */
void
wlc_edcf_acp_get_all(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp)
{
	_wlc_edcf_acp_get_all(wlc, cfg, &cfg->wme->wme_param_ie, acp);
}

/** Extract all AC param sets used in BCN/PRBRSP and write them into host struct 'acp' */
static void
wlc_edcf_acp_ad_get_all(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp)
{
	ASSERT(BSSCFG_AP(cfg));
	_wlc_edcf_acp_get_all(wlc, cfg, cfg->wme->wme_param_ie_ad, acp);
}

/** Extract one set of AC params from host struct 'acp' and write them into WME IE 'ie' */
static int
_wlc_edcf_acp_set_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp, wme_param_ie_t *ie)
{
	uint aci_in;
	edcf_acparam_t *acp_ie;

	/* Determine requested entry */
	aci_in = (acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT;
	if (aci_in >= AC_COUNT) {
		WL_ERROR(("wl%d.%d: Set of AC Params with bad ACI %d\n", WLCWLUNIT(wlc),
			WLC_BSSCFG_IDX(cfg), aci_in));
		return BCME_RANGE;
	}

	/* Set the contents as specified */
	acp_ie = &ie->acparam[aci_in];
	acp_ie->ACI = acp->ACI;
	acp_ie->ECW = acp->ECW;
	/* convert to network order */
	acp_ie->TXOP = htol16(acp->TXOP);

	WL_INFORM(("wl%d.%d: setting %s RAW AC Params ACI 0x%x ECW 0x%x TXOP 0x%x\n",
	           wlc->pub->unit, WLC_BSSCFG_IDX(cfg), aci_names[aci_in],
	           acp_ie->ACI, acp_ie->ECW, acp_ie->TXOP));

	return BCME_OK;
} /* _wlc_edcf_acp_set_ie */

/**
 * Extract one set of AC params from host struct 'acp' and put them into WME IE whose AC params are
 * used by h/w.
 */
static int
wlc_edcf_acp_set_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp)
{
	return _wlc_edcf_acp_set_ie(wlc, cfg, acp, &cfg->wme->wme_param_ie);
}

/**
 * Extract one set of AC params from host struct 'acp' and write them into WME IE used in
 * BCN/PRBRSP.
 */
static int
wlc_edcf_acp_ad_set_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp)
{
	wme_param_ie_t *ie;

	ASSERT(BSSCFG_AP(cfg));

	/* APs need to notify any clients */
	ie = cfg->wme->wme_param_ie_ad;
	/* Increment count field to notify associated STAs of parameter change */
	cfg->edca_update_count++;
	cfg->edca_update_count &= WME_QI_AP_COUNT_MASK;
	ie->qosinfo &= ~WME_QI_AP_COUNT_MASK;
	ie->qosinfo |= cfg->edca_update_count;

	return _wlc_edcf_acp_set_ie(wlc, cfg, acp, ie);
}

/**
 * Extract one set of AC params from host struct 'acp', write them into the WME IE whose AC param
 * are used by h/w, and them to the h/w'. It also updates other WME info in 'cfg' based on 'acp'.
 */
static int
wlc_edcf_acp_set(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp, bool suspend)
{
	int err;
	uint aci;

	err = wlc_edcf_acp_set_ie(wlc, cfg, acp);
	if (err != BCME_OK)
		return err;

	aci = (acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT;
	ASSERT(aci < AC_COUNT);

	wlc_edcf_setparams(wlc, cfg, aci, suspend);
	return BCME_OK;
}

/* APIs to external users including ioctl/iovar handlers... */

/**
 * Function used by ioctl/iovar code. Extract one set of AC params from host struct 'acp'
 * and write them into the WME IE whose AC params are used by h/w and update the h/w.
 */
int
wlc_edcf_acp_set_one(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp, bool suspend)
{
	uint aci = (acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT;
#ifndef WLP2P_UCODE_MACP
	int idx;
	wlc_bsscfg_t *bc;
	int err;
#endif // endif

	if (aci >= AC_COUNT)
		return BCME_RANGE;

#ifndef WLP2P_UCODE_MACP
	/* duplicate to all BSSs if the h/w supports only a single set of WME params... */
	FOREACH_BSS(wlc, idx, bc) {
		if (bc == cfg)
			continue;
		WL_INFORM(("wl%d: Setting %s in cfg %d...\n",
		           wlc->pub->unit, aci_names[aci], WLC_BSSCFG_IDX(bc)));
		err = wlc_edcf_acp_set_ie(wlc, bc, acp);
		if (err != BCME_OK)
			return err;
	}
#endif /* !WLP2P_UCODE_MACP */

	return wlc_edcf_acp_set(wlc, cfg, acp, suspend);
}

/**
 * Function used by ioctl/iovar code. Extract one set of AC params from host struct 'acp'
 * and write them into the WME IE used in BCN/PRBRSP and update the h/w.
 */
static int
wlc_edcf_acp_ad_set_one(wlc_info_t *wlc, wlc_bsscfg_t *cfg, edcf_acparam_t *acp)
{
	uint aci = (acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT;
#ifndef WLP2P_UCODE_MACP
	int idx;
	wlc_bsscfg_t *bc;
#endif // endif

	if (aci >= AC_COUNT)
		return BCME_RANGE;

#ifndef WLP2P_UCODE_MACP
	/* duplicate to all BSSs if the h/w supports only a single set of WME params... */
	FOREACH_AP(wlc, idx, bc) {
		if (bc == cfg)
			continue;
		WL_INFORM(("wl%d: Setting advertised %s in cfg %d...\n",
		           wlc->pub->unit, aci_names[aci], WLC_BSSCFG_IDX(bc)));
		wlc_edcf_acp_ad_set_ie(wlc, bc, acp);
	}
#endif /* !WLP2P_UCODE_MACP */

	return wlc_edcf_acp_ad_set_ie(wlc, cfg, acp);
}

/** Apply all AC param sets stored in WME IE in 'cfg' to the h/w */
int
wlc_edcf_acp_apply(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend)
{
	edcf_acparam_t acp[AC_COUNT];
	uint i;
	int err = BCME_OK;
#ifndef WLP2P_UCODE_MACP
	int idx;
	wlc_bsscfg_t *tplt;
#endif // endif
#ifdef PROP_TXSTATUS
	bool bCancelCreditBorrow = FALSE;
#endif // endif
#ifndef WLP2P_UCODE_MACP
	/* find the first STA (primary STA has priority) and use its AC params */
	FOREACH_AS_STA(wlc, idx, tplt) {
		if (BSS_WME_AS(wlc, tplt))
			break;
	}
	if (tplt != NULL) {
		WL_INFORM(("wl%d: Using cfg %d's AC params...\n",
		           wlc->pub->unit, WLC_BSSCFG_IDX(tplt)));
		cfg = tplt;
	}
#endif // endif

	wlc_edcf_acp_get_all(wlc, cfg, acp);
#ifdef PROP_TXSTATUS
	for (i = AC_VI; i < AC_COUNT; ++i) {
		if ((acp[i].ACI & EDCF_AIFSN_MASK) > (acp[i-1].ACI & EDCF_AIFSN_MASK))
			bCancelCreditBorrow = TRUE;
	}
	wlfc_enable_cred_borrow(wlc, !bCancelCreditBorrow);
#endif /* PROP_TXSTATUS */
	for (i = 0; i < AC_COUNT; i++) {
		err = wlc_edcf_acp_set_one(wlc, cfg, &acp[i], suspend);
		if (err != BCME_OK)
			return err;
	}
	return BCME_OK;
}

/* module interfaces */
static int
wlc_qos_wlc_up(void *ctx)
{
	wlc_qos_info_t *qosi = (wlc_qos_info_t *)ctx;
	wlc_info_t *wlc = qosi->wlc;

	/* Set EDCF hostflags */
	if (WME_ENAB(wlc->pub)) {
		wlc_mhf(wlc, MHF1, MHF1_EDCF, MHF1_EDCF, WLC_BAND_ALL);
#if defined(WME_PER_AC_TX_PARAMS)
		if (!RATELINKMEM_ENAB(wlc->pub)) {
			int ac;
			bool enab = FALSE;

			for (ac = 0; ac < AC_COUNT; ac++) {
				if (wlc->wme_max_rate[ac] != 0) {
					enab = TRUE;
					break;
				}
			}
			WL_RATE(("Setting per ac maxrate to %d \n", enab));
			wlc->pub->_per_ac_maxrate = enab;
		}
#endif /* WME_PER_AC_TX_PARAMS */
	} else {
		wlc_mhf(wlc, MHF1, MHF1_EDCF, 0, WLC_BAND_ALL);
	}

	return BCME_OK;
}

/** Write WME tunable parameters for retransmit/max rate from wlc struct to ucode */
void
wlc_wme_retries_write(wlc_info_t *wlc)
{
	int ac;

	/* Need clock to do this */
	if (!wlc->clk)
		return;

	for (ac = 0; ac < AC_COUNT; ac++) {
		wlc_write_shm(wlc, M_AC_TXLMT_ADDR(wlc, ac), wlc->wme_retries[ac]);
	}
}

/** Convert discard policy AC bitmap to Precedence bitmap */
static uint16
wlc_convert_acbitmap_to_precbitmap(ac_bitmap_t acbitmap)
{
	uint16 prec_bitmap = 0;

	if (AC_BITMAP_TST(acbitmap, AC_BE))
		prec_bitmap |= WLC_PREC_BMP_AC_BE;

	if (AC_BITMAP_TST(acbitmap, AC_BK))
		prec_bitmap |= WLC_PREC_BMP_AC_BK;

	if (AC_BITMAP_TST(acbitmap, AC_VI))
		prec_bitmap |= WLC_PREC_BMP_AC_VI;

	if (AC_BITMAP_TST(acbitmap, AC_VO))
		prec_bitmap |= WLC_PREC_BMP_AC_VO;

	return prec_bitmap;
}

void
wlc_wme_shm_read(wlc_info_t *wlc)
{
	uint ac;

	for (ac = 0; ac < AC_COUNT; ac++) {
		uint16 temp = wlc_read_shm(wlc, M_AC_TXLMT_ADDR(wlc, ac));

		if (WLC_WME_RETRY_SHORT_GET(wlc, ac) == 0)
			WLC_WME_RETRY_SHORT_SET(wlc, ac, WME_RETRY_SHORT_GET(temp, ac));

		if (WLC_WME_RETRY_SFB_GET(wlc, ac) == 0)
			WLC_WME_RETRY_SFB_SET(wlc, ac, WME_RETRY_SFB_GET(temp, ac));

		if (WLC_WME_RETRY_LONG_GET(wlc, ac) == 0)
			WLC_WME_RETRY_LONG_SET(wlc, ac, WME_RETRY_LONG_GET(temp, ac));

		if (WLC_WME_RETRY_LFB_GET(wlc, ac) == 0)
			WLC_WME_RETRY_LFB_SET(wlc, ac, WME_RETRY_LFB_GET(temp, ac));
	}
}

/* Nybble swap; QoS Info bits are in backward order from AC bitmap */
static const ac_bitmap_t qi_bitmap_to_ac_bitmap[16] = {
	0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
};
/* Max USP Length field decoding (WMM 2.2.1) */
static const uint16 maxsp[4] = { WLC_APSD_USP_UNB, 2, 4, 6 };

/** Decode the WMM 2.1.1 QoS Info field, as sent by WMM STA, and save in scb */
void
	wlc_qosinfo_update(struct scb *scb, uint8 qosinfo, bool ac_upd)
{
	scb->apsd.maxsplen =
	        maxsp[(qosinfo & WME_QI_STA_MAXSPLEN_MASK) >> WME_QI_STA_MAXSPLEN_SHIFT];

	/* Update ac_defl during assoc and reassoc (static power save settings). */
	scb->apsd.ac_defl = qi_bitmap_to_ac_bitmap[qosinfo & WME_QI_STA_APSD_ALL_MASK];

	/* Initial config; can be updated by TSPECs, and revert back to ac_defl on DELTS. */
	if (ac_upd) {
		/* Do not update trig and delv for reassoc resp with same AP */
		scb->apsd.ac_trig = scb->apsd.ac_defl;
		scb->apsd.ac_delv = scb->apsd.ac_defl;
	}
}

#include <wlc_patch.h>

#ifndef WLP2P_UCODE_MACP
#endif /* WLP2P_UCODE_MACP */

static int
wlc_qos_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint val_size, struct wlc_if *wlcif)
{
	wlc_qos_info_t *qosi = (wlc_qos_info_t *)ctx;
	wlc_info_t *wlc = qosi->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = BCME_OK;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	wlc_wme_t *wme;

	BCM_REFERENCE(val_size);

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	wme = bsscfg->wme;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* Do the actual parameter implementation */
	switch (actionid) {

#ifndef WLP2P_UCODE_MACP
	case IOV_GVAL(IOV_WME_AC_STA): {
		edcf_acparam_t acp_all[AC_COUNT];

		if (!WME_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (alen < (int)sizeof(acp_all)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (BSSCFG_STA(bsscfg))
			wlc_edcf_acp_get_all(wlc, bsscfg, acp_all);
		if (BSSCFG_AP(bsscfg))
			wlc_edcf_acp_ad_get_all(wlc, bsscfg, acp_all);
		memcpy(arg, acp_all, sizeof(acp_all));    /* Copy to handle unaligned */
		break;
	}
	case IOV_GVAL(IOV_WME_AC_AP):
#endif /* WLP2P_UCODE_MACP */
	case IOV_GVAL(IOV_EDCF_ACP): {
		edcf_acparam_t acp_all[AC_COUNT];

		if (!WME_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (alen < (int)sizeof(acp_all)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		wlc_edcf_acp_get_all(wlc, bsscfg, acp_all);
		memcpy(arg, acp_all, sizeof(acp_all));    /* Copy to handle unaligned */
		break;
	}
#ifdef AP
	case IOV_GVAL(IOV_EDCF_ACP_AD): {
		edcf_acparam_t acp_all[AC_COUNT];

		if (!WME_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (alen < (int)sizeof(acp_all)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (!BSSCFG_AP(bsscfg)) {
			err = BCME_ERROR;
			break;
		}

		wlc_edcf_acp_ad_get_all(wlc, bsscfg, acp_all);
		memcpy(arg, acp_all, sizeof(acp_all));    /* Copy to handle unaligned */
		break;
	}
#endif /* AP */

#ifndef WLP2P_UCODE_MACP
	case IOV_SVAL(IOV_WME_AC_STA): {
		edcf_acparam_t acp;

		if (!WME_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		memcpy(&acp, arg, sizeof(acp));
		if (BSSCFG_STA(bsscfg))
			err = wlc_edcf_acp_set_one(wlc, bsscfg, &acp, TRUE);
		if (BSSCFG_AP(bsscfg))
			err = wlc_edcf_acp_ad_set_one(wlc, bsscfg, &acp);
		break;
	}
	case IOV_SVAL(IOV_WME_AC_AP):
#endif /* WLP2P_UCODE_MACP */
	case IOV_SVAL(IOV_EDCF_ACP): {
		edcf_acparam_t acp;

		if (!WME_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		memcpy(&acp, arg, sizeof(acp));
		err = wlc_edcf_acp_set_one(wlc, bsscfg, &acp, TRUE);
		break;
	}
#ifdef AP
	case IOV_SVAL(IOV_EDCF_ACP_AD): {
		edcf_acparam_t acp;

		if (!WME_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (!BSSCFG_AP(bsscfg)) {
			err = BCME_ERROR;
			break;
		}

		memcpy(&acp, arg, sizeof(acp));
		err = wlc_edcf_acp_ad_set_one(wlc, bsscfg, &acp);
		break;
	}
#endif /* AP */

	case IOV_GVAL(IOV_WME):
		*ret_int_ptr = (int32)wlc->pub->_wme;
		break;

	case IOV_SVAL(IOV_WME):
		if (int_val < AUTO || int_val > ON) {
			err = BCME_RANGE;
			break;
		}

		/* For AP, AUTO mode is same as ON */
		if (AP_ENAB(wlc->pub) && int_val == AUTO)
			wlc->pub->_wme = ON;
		else
			wlc->pub->_wme = int_val;

#ifdef STA
		/* If not in AUTO mode, PM is always allowed
		 * In AUTO mode, PM is allowed on for UAPSD enabled AP
		 */
		if (int_val != AUTO) {
			int idx;
			wlc_bsscfg_t *bc;

			FOREACH_BSS(wlc, idx, bc) {
				if (!BSSCFG_STA(bc))
					continue;
				bc->pm->WME_PM_blocked = FALSE;
			}
		}
#endif // endif
		break;

	case IOV_GVAL(IOV_WME_BSS_DISABLE):
		*ret_int_ptr = ((bsscfg->flags & WLC_BSSCFG_WME_DISABLE) != 0);
		break;

	case IOV_SVAL(IOV_WME_BSS_DISABLE):
		WL_INFORM(("%s(): set IOV_WME_BSS_DISABLE to %s\n", __FUNCTION__,
			bool_val ? "TRUE" : "FALSE"));
		if (bool_val) {
			bsscfg->flags |= WLC_BSSCFG_WME_DISABLE;
		} else {
			bsscfg->flags &= ~WLC_BSSCFG_WME_DISABLE;
		}
		break;

	case IOV_GVAL(IOV_WME_NOACK):
		*ret_int_ptr = (int32)wme->wme_noack;
		break;

	case IOV_SVAL(IOV_WME_NOACK):
		wme->wme_noack = bool_val;
		break;

	case IOV_GVAL(IOV_WME_APSD):
		*ret_int_ptr = (int32)wme->wme_apsd;
		break;

	case IOV_SVAL(IOV_WME_APSD):
		wme->wme_apsd = bool_val;
		if (BSSCFG_AP(bsscfg) && wlc->clk && !bsscfg->up) {
			wlc_bss_update_beacon(wlc, bsscfg);
			wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
		}
		break;

	case IOV_SVAL(IOV_SEND_FRAME): {
		bool status;
		osl_t *osh = wlc->osh;
		void *pkt;

		/* Reject runts and jumbos */
		if (plen < ETHER_MIN_LEN || plen > ETHER_MAX_LEN || params == NULL) {
			err = BCME_BADARG;
			break;
		}
		pkt = PKTGET(osh, plen + wlc->txhroff, TRUE);
		if (pkt == NULL) {
			err = BCME_NOMEM;
			break;
		}

		PKTPULL(osh, pkt, wlc->txhroff);
		bcopy(params, PKTDATA(osh, pkt), plen);
		PKTSETLEN(osh, pkt, plen);
		status = wlc_sendpkt(wlc, pkt, wlcif);
		if (status)
			err = BCME_NORESOURCE;
		break;
	}

#ifdef STA
	case IOV_GVAL(IOV_WME_APSD_TRIGGER):
	        *ret_int_ptr = (int32)bsscfg->pm->apsd_trigger_timeout;
		break;

	case IOV_SVAL(IOV_WME_APSD_TRIGGER):
	        /*
		 * Round timeout up to an even number because we set
		 * the timer at 1/2 the timeout period.
		 */
		bsscfg->pm->apsd_trigger_timeout = ROUNDUP((uint32)int_val, 2);
		wlc_apsd_trigger_upd(bsscfg, TRUE);
		break;

	case IOV_GVAL(IOV_WME_TRIGGER_AC):
		*ret_int_ptr = (int32)wme->apsd_trigger_ac;
		break;

	case IOV_SVAL(IOV_WME_TRIGGER_AC):
		if (int_val <= AC_BITMAP_ALL)
			wme->apsd_trigger_ac = (ac_bitmap_t)int_val;
		else
			err = BCME_BADARG;
		break;

	case IOV_GVAL(IOV_WME_AUTO_TRIGGER):
		*ret_int_ptr = (int32)wme->apsd_auto_trigger;
		break;

	case IOV_SVAL(IOV_WME_AUTO_TRIGGER):
		wme->apsd_auto_trigger = bool_val;
		break;
#endif /* STA */

	case IOV_GVAL(IOV_WME_DP):
		*ret_int_ptr = (int32)wlc->wme_dp;
		break;

	case IOV_SVAL(IOV_WME_DP):
		wlc->wme_dp = (uint8)int_val;
		wlc->wme_dp_precmap = wlc_convert_acbitmap_to_precbitmap(wlc->wme_dp);
		break;

#if defined(WLCNT)
	case IOV_GVAL(IOV_WME_COUNTERS):
		bcopy(wlc->pub->_wme_cnt, arg, sizeof(wl_wme_cnt_t));
		break;
	case IOV_SVAL(IOV_WME_CLEAR_COUNTERS):
		/* Zero the counters but reinit the version and length */
		bzero(wlc->pub->_wme_cnt,  sizeof(wl_wme_cnt_t));
		WLCNTSET(wlc->pub->_wme_cnt->version, WL_WME_CNT_VERSION);
		WLCNTSET(wlc->pub->_wme_cnt->length, sizeof(wl_wme_cnt_t));
		break;
#endif /* WLCNT */
	case IOV_GVAL(IOV_WME_PREC_QUEUING):
		*ret_int_ptr = wlc->wme_prec_queuing ? 1 : 0;
		break;
	case IOV_SVAL(IOV_WME_PREC_QUEUING):
		wlc->wme_prec_queuing = bool_val;
		break;

#ifdef STA
	case IOV_GVAL(IOV_WME_QOSINFO):
		*ret_int_ptr = wme->apsd_sta_qosinfo;
		break;

	case IOV_SVAL(IOV_WME_QOSINFO):
		wme->apsd_sta_qosinfo = (uint8)int_val;
		break;
#endif /* STA */

#if defined(WME_PER_AC_TUNING) && defined(WME_PER_AC_TX_PARAMS)
	case IOV_GVAL(IOV_WME_TX_PARAMS):
		if (WME_PER_AC_TX_PARAMS_ENAB(wlc->pub)) {
			int ac;
			wme_tx_params_t *prms;

			prms = (wme_tx_params_t *)arg;

			for (ac = 0; ac < AC_COUNT; ac++) {
				prms[ac].max_rate = wlc->wme_max_rate[ac];
				prms[ac].short_retry = WLC_WME_RETRY_SHORT_GET(wlc, ac);
				prms[ac].short_fallback = WLC_WME_RETRY_SFB_GET(wlc, ac);
				prms[ac].long_retry = WLC_WME_RETRY_LONG_GET(wlc, ac);
				prms[ac].long_fallback = WLC_WME_RETRY_LFB_GET(wlc, ac);
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_SVAL(IOV_WME_TX_PARAMS):
		if (WME_PER_AC_TX_PARAMS_ENAB(wlc->pub)) {
			int ac;
			wme_tx_params_t *prms;

			prms = (wme_tx_params_t *)arg;

			wlc->LRL = prms[AC_BE].long_retry;
			wlc->SRL = prms[AC_BE].short_retry;
			wlc_bmac_retrylimit_upd(wlc->hw, wlc->SRL, wlc->LRL);

			for (ac = 0; ac < AC_COUNT; ac++) {
				wlc->wme_max_rate[ac] = prms[ac].max_rate;
				WLC_WME_RETRY_SHORT_SET(wlc, ac, prms[ac].short_retry);
				WLC_WME_RETRY_SFB_SET(wlc, ac, prms[ac].short_fallback);
				WLC_WME_RETRY_LONG_SET(wlc, ac, prms[ac].long_retry);
				WLC_WME_RETRY_LFB_SET(wlc, ac, prms[ac].long_fallback);
			}

			wlc_wme_retries_write(wlc);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

#endif /* WME_PER_AC_TUNING && PER_AC_TX_PARAMS */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_qos_doiovar */

#if defined(BCMDBG)
/** Print out the AC Params in an IE */
static void
wlc_dump_edcf_acp(wlc_info_t *wlc, struct bcmstrbuf *b, edcf_acparam_t *acp_ie, const char *desc)
{
	int ac;
	BCM_REFERENCE(wlc);
	bcm_bprintf(b, "\nEDCF params for %s\n", desc);

	for (ac = 0; ac < AC_COUNT; ac++, acp_ie++) {
		bcm_bprintf(b,
		               "%s: ACI 0x%02x ECW 0x%02x "
		               "(aci %d acm %d aifsn %d ecwmin %d ecwmax %d txop 0x%x)\n",
		               aci_names[ac],
		               acp_ie->ACI, acp_ie->ECW,
		               (acp_ie->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT,
		               (acp_ie->ACI & EDCF_ACM_MASK) ? 1 : 0,
		               acp_ie->ACI & EDCF_AIFSN_MASK,
		               acp_ie->ECW & EDCF_ECWMIN_MASK,
		               (acp_ie->ECW & EDCF_ECWMAX_MASK) >> EDCF_ECWMAX_SHIFT,
		               ltoh16(acp_ie->TXOP));
	}

	bcm_bprintf(b, "\n");
}

/** Print out the AC Params in use by ucode */
static void
wlc_dump_wme_shm(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int ac;

	bcm_bprintf(b, "\nEDCF params in shared memory\n");

	for (ac = 0; ac < AC_COUNT; ac++) {
		shm_acparams_t shm_acp;
		uint16 *element;
		int j;

		element = (uint16 *)&shm_acp;

		/* fill in the ac param element from the shm locations */
		for (j = 0; j < (int)sizeof(shm_acparams_t); j += 2) {
			uint offset = M_EDCF_BLKS(wlc) + wme_shmemacindex(ac) *
				M_EDCF_QLEN(wlc) + j;
			*element++ = wlc_read_shm(wlc, offset);
		}

		bcm_bprintf(b, "%s: txop 0x%x cwmin 0x%x cwmax 0x%x cwcur 0x%x\n"
		               "       aifs 0x%x bslots 0x%x reggap 0x%x status 0x%x\n",
		               aci_names[ac],
		               shm_acp.txop, shm_acp.cwmin, shm_acp.cwmax, shm_acp.cwcur,
		               shm_acp.aifs, shm_acp.bslots, shm_acp.reggap, shm_acp.status);
	}

	bcm_bprintf(b, "\n");
}

static void
wlc_qos_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_qos_info_t *qosi = (wlc_qos_info_t *)ctx;
	wlc_info_t *wlc = qosi->wlc;
	wlc_wme_t *wme = cfg->wme;
	uint8 qi;

	qi = wme->wme_param_ie.qosinfo;

	bcm_bprintf(b, "\ncfg %d WME %d apsd %d count %d admctl 0x%x\n",
	            WLC_BSSCFG_IDX(cfg), BSS_WME_ENAB(wlc, cfg),
	            (qi & WME_QI_AP_APSD_MASK) >> WME_QI_AP_APSD_SHIFT,
	            (qi & WME_QI_AP_COUNT_MASK) >> WME_QI_AP_COUNT_SHIFT,
	            wme->wme_admctl);

	wlc_dump_edcf_acp(wlc, b, wme->wme_param_ie.acparam,
	                  BSSCFG_AP(cfg) ? "AP" : "STA");

	if (BSSCFG_AP(cfg)) {
		wlc_dump_edcf_acp(wlc, b, wme->wme_param_ie_ad->acparam, "BCN/PRBRSP");
	}
}

static int
wlc_dump_wme(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint32 cwcur, cwmin, cwmax, fifordy;
	osl_t *osh;
	int idx;
	wlc_bsscfg_t *bsscfg;

	bcm_bprintf(b, "up %d WME %d dp 0x%x\n",
	            wlc->pub->up, WME_ENAB(wlc->pub), wlc->wme_dp);

	FOREACH_BSS(wlc, idx, bsscfg) {
		wlc_qos_bss_dump(wlc->qosi, bsscfg, b);
	}

	if (!WME_ENAB(wlc->pub))
		return BCME_OK;

	if (!wlc->pub->up)
		return BCME_OK;

	wlc_dump_wme_shm(wlc, b);

	osh = wlc->osh;

	/* read current cwcur, cwmin, cwmax */
	wlc_bmac_copyfrom_objmem(wlc->hw, S_DOT11_CWMIN << 2, &cwmin,
		sizeof(cwmin), OBJADDR_SCR_SEL);
	wlc_bmac_copyfrom_objmem(wlc->hw, S_DOT11_CWMAX << 2, &cwmax,
		sizeof(cwmax), OBJADDR_SCR_SEL);
	wlc_bmac_copyfrom_objmem(wlc->hw, S_DOT11_CWCUR << 2, &cwcur,
		sizeof(cwcur), OBJADDR_SCR_SEL);

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		if (D11REV_GE(wlc->pub->corerev, 65)) {
			fifordy = R_REG(osh, D11_AQMFifoRdy_L(wlc));
			fifordy = fifordy | (R_REG(osh, D11_AQMFifoRdy_H(wlc)) << 16);
		} else if (D11REV_GE(wlc->pub->corerev, 40)) {
			fifordy = R_REG(osh, D11_AQMFifoReady(wlc));
		} else {
			fifordy = R_REG(osh, D11_xmtfifordy(wlc));
		}

		bcm_bprintf(b, "xfifordy 0x%x psm_b0 0x%x txf_cur_idx 0x%x\n", fifordy,
			R_REG(osh, (D11REV_GE(wlc->pub->corerev, 65) ?
				D11_PSM_BASE_0(wlc) : D11_PSM_INTSEL_0(wlc))),
			wlc_read_shm(wlc, M_CUR_TXF_INDEX(wlc)));
	}
	bcm_bprintf(b, "cwcur 0x%x cwmin 0x%x cwmax 0x%x\n", cwcur, cwmin, cwmax);

	bcm_bprintf(b, "\n");

	return BCME_OK;
} /* wlc_dump_wme */
#else
#define wlc_qos_bss_dump NULL
#endif // endif

static int
wlc_qos_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_qos_info_t *qosi = (wlc_qos_info_t *)ctx;
	wlc_info_t *wlc = qosi->wlc;
	int err = BCME_OK;

	/* EDCF/APSD/WME */
	if ((cfg->wme = (wlc_wme_t *)
	     MALLOCZ(wlc->osh, sizeof(wlc_wme_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}

	/* APSD defaults */
	cfg->wme->wme_apsd = TRUE;

	if (BSSCFG_STA(cfg)) {
		cfg->wme->apsd_trigger_ac = AC_BITMAP_ALL;
		wlc_wme_initparams_sta(wlc, &cfg->wme->wme_param_ie);
	}
	/* WME ACP advertised in bcn/prbrsp */
	else if (BSSCFG_AP(cfg)) {
		if ((cfg->wme->wme_param_ie_ad =
		     MALLOCZ(wlc->osh, sizeof(wme_param_ie_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		    err = BCME_NOMEM;
		    goto fail;
		}
		wlc_wme_initparams_ap(wlc, &cfg->wme->wme_param_ie);
		wlc_wme_initparams_sta(wlc, cfg->wme->wme_param_ie_ad);
	}

	return BCME_OK;
fail:
	return err;
}

static void
wlc_qos_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_qos_info_t *qosi = (wlc_qos_info_t *)ctx;
	wlc_info_t *wlc = qosi->wlc;

	/* EDCF/APSD/WME */
	if (cfg->wme != NULL) {
		/* WME AC parms */
		if (cfg->wme->wme_param_ie_ad != NULL) {
			MFREE(wlc->osh, cfg->wme->wme_param_ie_ad, sizeof(wme_param_ie_t));
		}
		MFREE(wlc->osh, cfg->wme, sizeof(wlc_wme_t));
	}
}

/** Vendor specific WME parameters IE (WLC_IEM_VS_IE_PRIO_WME) */
static uint
wlc_bss_calc_wme_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	bool iswme = FALSE;

	if (!BSS_WME_ENAB(wlc, cfg))
		return 0;

	switch (data->ft) {
	case FC_BEACON:
	case FC_PROBE_RESP:
		/* WME IE for beacon responses in IBSS when 11n or 11ax enabled */
		if (BSSCFG_AP(cfg) || (BSSCFG_IBSS(cfg) && (data->cbparm->ht || data->cbparm->he)))
			iswme = TRUE;
		break;
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		/* Include a WME info element if the AP supports WME */
		if (ftcbparm->assocreq.target->flags & WLC_BSS_WME)
			iswme = TRUE;
		break;
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		if (SCB_WME(ftcbparm->assocresp.scb))
			iswme = TRUE;
		break;
	}

	if (!iswme) {
		return 0;
	}

	if (BSSCFG_AP(cfg))
		return TLV_HDR_LEN + sizeof(wme_param_ie_t);
	else
		return TLV_HDR_LEN + sizeof(wme_ie_t);

	return 0;
}

static int
wlc_bss_write_wme_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_wme_t *wme = cfg->wme;
	wlc_iem_ft_cbparm_t *ftcbparm = data->cbparm->ft;
	bool iswme = FALSE;
	bool apapsd = FALSE;

	if (!BSS_WME_ENAB(wlc, cfg))
		return BCME_OK;

	switch (data->ft) {
	case FC_BEACON:
	case FC_PROBE_RESP:
		if (BSSCFG_AP(cfg))
			iswme = TRUE;
		/* WME IE for beacon responses in IBSS when 11n or ax is enabled */
		else if (BSSCFG_IBSS(cfg) && (data->cbparm->ht || data->cbparm->he)) {
			apapsd = (cfg->current_bss->wme_qosinfo & WME_QI_AP_APSD_MASK) ?
			        TRUE : FALSE;
			iswme = TRUE;
		}
		break;
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		/* Include a WME info element if the AP supports WME */
		if (ftcbparm->assocreq.target->flags & WLC_BSS_WME) {
			apapsd = (ftcbparm->assocreq.target->wme_qosinfo & WME_QI_AP_APSD_MASK) ?
			        TRUE : FALSE;
			iswme = TRUE;
		}
		break;
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		if (SCB_WME(ftcbparm->assocresp.scb))
			iswme = TRUE;
		break;
	}
	if (!iswme)
		return BCME_OK;

	/* WME parameter info element in infrastructure beacons responses only */
	if (BSSCFG_AP(cfg)) {
		edcf_acparam_t *acp_ie = wme->wme_param_ie.acparam;
		wme_param_ie_t *ad = wme->wme_param_ie_ad;
		uint8 i = 0;

		if (wme->wme_apsd)
			ad->qosinfo |= WME_QI_AP_APSD_MASK;
		else
			ad->qosinfo &= ~WME_QI_AP_APSD_MASK;

		/* update the ACM value in WME IE */
		for (i = 0; i < AC_COUNT; i++, acp_ie++) {
			if (acp_ie->ACI & EDCF_ACM_MASK)
				ad->acparam[i].ACI |= EDCF_ACM_MASK;
			else
				ad->acparam[i].ACI &= ~EDCF_ACM_MASK;
		}

		bcm_write_tlv(DOT11_MNG_VS_ID, (uint8 *)ad, sizeof(wme_param_ie_t), data->buf);
	}
	else {
		wme_ie_t wme_ie;

		ASSERT(sizeof(wme_ie) == WME_IE_LEN);
		bcopy(WME_OUI, wme_ie.oui, WME_OUI_LEN);
		wme_ie.type = WME_OUI_TYPE;
		wme_ie.subtype = WME_SUBTYPE_IE;
		wme_ie.version = WME_VER;
		if (!wme->wme_apsd || !apapsd) {
			wme_ie.qosinfo = 0;
		} else {
			wme_ie.qosinfo = wme->apsd_sta_qosinfo;
		}

		bcm_write_tlv(DOT11_MNG_VS_ID, (uint8 *)&wme_ie, sizeof(wme_ie), data->buf);
	}

	return BCME_OK;
}

static int
wlc_bss_parse_wme_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	struct scb *scb;
	bcm_tlv_t *wme_ie = (bcm_tlv_t *)data->ie;
	wlc_wme_t *wme = cfg->wme;

	scb = wlc_iem_parse_get_assoc_bcn_scb(data);
	ASSERT(scb != NULL);

	/* Do not parse IE if TLV length doesn't matches the size of the structure */
	if (wme_ie != NULL && (wme_ie->len != sizeof(wme_param_ie_t)) &&
		(wme_ie->len != sizeof(wme_ie_t))) {
		WL_ERROR(("%s Incorrect TLV - IE len: %d instead of WME IE len: %d "
			"or WME param IE len: %d\n", __FUNCTION__, wme_ie->len,
			(uint)sizeof(wme_ie_t), (uint)sizeof(wme_param_ie_t)));
		if (BSS_WME_ENAB(wlc, cfg)) {
			/* clear WME flags */
			scb->flags &= ~(SCB_WMECAP | SCB_APSDCAP);
			cfg->flags &= ~WLC_BSSCFG_WME_ASSOC;

			/* Clear Qos Info by default */
			wlc_qosinfo_update(scb, 0, TRUE);
		}
		return BCME_OK;
	}

	switch (data->ft) {
#ifdef AP
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:

		/* Handle WME association */
		scb->flags &= ~(SCB_WMECAP | SCB_APSDCAP);

		if (!BSS_WME_ENAB(wlc, cfg))
			break;

		wlc_qosinfo_update(scb, 0, TRUE);     /* Clear Qos Info by default */

		if (wme_ie == NULL)
			break;

		scb->flags |= SCB_WMECAP;

		/* Note requested APSD parameters if AP supporting APSD */
		if (!wme->wme_apsd)
			break;

		wlc_qosinfo_update(scb, ((wme_ie_t *)wme_ie->data)->qosinfo, TRUE);
		if (scb->apsd.ac_trig & AC_BITMAP_ALL)
			scb->flags |= SCB_APSDCAP;

		break;
#endif /* AP */
#ifdef STA
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP: {
		wlc_pm_st_t *pm = cfg->pm;
		bool upd_trig_delv;

		/* If WME is enabled, check if response indicates WME association */
		scb->flags &= ~SCB_WMECAP;
		cfg->flags &= ~WLC_BSSCFG_WME_ASSOC;

		if (!BSS_WME_ENAB(wlc, cfg))
			break;

		/* Do not update ac_delv and ac for ReassocResp with same AP */
		/* upd_trig_delv is FALSE for ReassocResp with same AP, TRUE otherwise */
		upd_trig_delv = !((data->ft == FC_REASSOC_RESP) &&
			(!bcmp((char *)&cfg->prev_BSSID,
			(char *)&cfg->target_bss->BSSID, ETHER_ADDR_LEN)));
		wlc_qosinfo_update(scb, 0, upd_trig_delv);

		if (wme_ie == NULL)
			break;

		scb->flags |= SCB_WMECAP;
		cfg->flags |= WLC_BSSCFG_WME_ASSOC;

		/* save the new IE, or params IE which is superset of IE */
		bcopy(wme_ie->data, &wme->wme_param_ie, wme_ie->len);
		/* Apply the STA AC params sent by AP,
		 * will be done in wlc_join_adopt_bss()
		 */
		/* wlc_edcf_acp_apply(wlc, cfg, TRUE); */
		/* Use locally-requested APSD config if AP advertised APSD */
		/* STA is in AUTO WME mode,
		 *     AP has UAPSD enabled, then allow STA to use wlc->PM
		 *            else, don't allow STA to sleep based on wlc->PM only
		 *                  if it's BRCM AP not capable of handling
		 *                                  WME STAs in PS,
		 *                  and leave PM mode if already set
		 */
		if ((wme->wme_param_ie.qosinfo & WME_QI_AP_APSD_MASK) && (wme->wme_apsd)) {
			wlc_qosinfo_update(scb, wme->apsd_sta_qosinfo, upd_trig_delv);
			pm->WME_PM_blocked = FALSE;
			if (pm->PM == PM_MAX)
				wlc_set_pmstate(cfg, TRUE);
		}
		else if (WME_AUTO(wlc) &&
		         (scb->flags & SCB_BRCM)) {
			if (!(scb->flags & SCB_WMEPS)) {
				pm->WME_PM_blocked = TRUE;
				WL_RTDC(wlc, "wlc_recvctl: exit PS", 0, 0);
				wlc_set_pmstate(cfg, FALSE);
			}
			else {
				pm->WME_PM_blocked = FALSE;
				if (pm->PM == PM_MAX)
					wlc_set_pmstate(cfg, TRUE);
			}
		}
		break;
	}
	case FC_BEACON:
		/* WME: check if the AP has supplied new acparams */
		/* WME: check if IBSS WME_IE is present */
		if (!BSS_WME_AS(wlc, cfg))
			break;

		if (scb && BSSCFG_IBSS(cfg)) {
			if (wme_ie != NULL) {
				scb->flags |= SCB_WMECAP;
			}
			break;
		}

		if (wme_ie == NULL) {
			WL_ERROR(("wl%d: %s: wme params ie missing\n",
			          wlc->pub->unit, __FUNCTION__));
			/* for non-wme association, BE ACI is 2 */
			wme->wme_param_ie.acparam[0].ACI = NON_EDCF_AC_BE_ACI_STA;
			wlc_edcf_acp_apply(wlc, cfg, TRUE);
			break;
		}

		if ((((wme_ie_t *)wme_ie->data)->qosinfo & WME_QI_AP_COUNT_MASK) !=
		    (wme->wme_param_ie.qosinfo & WME_QI_AP_COUNT_MASK)) {
			/* save and apply new params ie */
			bcopy(wme_ie->data, &wme->wme_param_ie,	sizeof(wme_param_ie_t));
			/* Apply the STA AC params sent by AP */
			wlc_edcf_acp_apply(wlc, cfg, TRUE);
		}
		break;
#endif /* STA */
	default:
		(void)wlc;
		(void)scb;
		(void)wme_ie;
		(void)wme;
		break;
	}

	return BCME_OK;
}

wlc_qos_info_t *
BCMATTACHFN(wlc_qos_attach)(wlc_info_t *wlc)
{
	/* WME Vendor Specific IE */
	uint16 wme_build_fstbmp =
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_REQ) |
	        FT2BMP(FC_REASSOC_RESP) |
	        FT2BMP(FC_PROBE_RESP) |
	        FT2BMP(FC_BEACON) |
	        0;
	uint16 wme_parse_fstbmp =
	        FT2BMP(FC_ASSOC_REQ) |
	        FT2BMP(FC_ASSOC_RESP) |
	        FT2BMP(FC_REASSOC_REQ) |
	        FT2BMP(FC_REASSOC_RESP) |
	        FT2BMP(FC_BEACON) |
	        0;
	wlc_qos_info_t *qosi;
	bsscfg_cubby_params_t cubby_params;

	ASSERT(M_EDCF_QLEN(wlc) == sizeof(shm_acparams_t));

	/* allocate module info */
	if ((qosi = MALLOCZ(wlc->osh, sizeof(*qosi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	qosi->wlc = wlc;

#ifdef WLCNT
	if ((wlc->pub->_wme_cnt = MALLOCZ(wlc->osh, sizeof(wl_wme_cnt_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	WLCNTSET(wlc->pub->_wme_cnt->version, WL_WME_CNT_VERSION);
	WLCNTSET(wlc->pub->_wme_cnt->length, sizeof(wl_wme_cnt_t));
#endif /* WLCNT */
	/* reserve the bsscfg cubby for any bss specific private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = qosi;
	cubby_params.fn_init = wlc_qos_bss_init;
	cubby_params.fn_deinit = wlc_qos_bss_deinit;
#if defined(BCMDBG)
	cubby_params.fn_dump = wlc_qos_bss_dump;
#endif // endif

	/* reserve bss init/deinit */
	if (wlc_bsscfg_cubby_reserve_ext(wlc, 0,
		&cubby_params) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* build/calc IE */
	if (wlc_iem_vs_add_build_fn_mft(wlc->iemi, wme_build_fstbmp, WLC_IEM_VS_IE_PRIO_WME,
			wlc_bss_calc_wme_ie_len, wlc_bss_write_wme_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_build_fn failed, wme ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* parse IE */
	if (wlc_iem_vs_add_parse_fn_mft(wlc->iemi, wme_parse_fstbmp, WLC_IEM_VS_IE_PRIO_WME,
			wlc_bss_parse_wme_ie, wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, wme ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module entries */
	if (wlc_module_register(wlc->pub, wme_iovars, "wme", qosi, wlc_qos_doiovar,
			NULL, wlc_qos_wlc_up, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "wme", (dump_fn_t)wlc_dump_wme, (void *)wlc);
#endif // endif

	return qosi;

fail:
	MODULE_DETACH(qosi, wlc_qos_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_qos_detach)(wlc_qos_info_t *qosi)
{
	wlc_info_t *wlc;

	if (qosi == NULL)
		return;

	wlc = qosi->wlc;

	(void)wlc_module_unregister(wlc->pub, "wme", qosi);

#ifdef WLCNT
	if (wlc->pub->_wme_cnt) {
		MFREE(wlc->osh, wlc->pub->_wme_cnt, sizeof(wl_wme_cnt_t));
		wlc->pub->_wme_cnt = NULL;
	}
#endif // endif

	MFREE(wlc->osh, qosi, sizeof(*qosi));
}
