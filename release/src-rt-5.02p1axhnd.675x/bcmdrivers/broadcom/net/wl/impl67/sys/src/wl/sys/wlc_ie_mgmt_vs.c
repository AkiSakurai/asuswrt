/*
 * @file
 * IE management module Vendor Specific IE utilities
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
 * $Id: wlc_ie_mgmt_vs.c 789641 2020-08-04 04:36:33Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <802.11.h>
#include <bcmutils.h>
#include <wlc_ie_mgmt_vs.h>

/* TODO:
 *
 * Compile entries in each table in an ascending order and
 * apply some faster search e.g. binary search to speed up
 * the ID query process...
 */

/* Where should it go? */
#define MSFT_OUI "\x00\x50\xf2"
#define RMC_PROP_OUI "\x00\x16\x32"

static bool wlc_ignore_assoc_oui(sta_vendor_oui_t  *assoc_oui,  uint8 *ie);

/*
 * Known 24 bit OUI + 8 bit type + 8 bit subtype
 */
static const struct {
	struct {
		uint8 oui[3];
		uint8 type;
		uint8 subtype;
	} cdi;
	wlc_iem_tag_t id;
} cdi482id[] = {
	{{MSFT_OUI, WME_OUI_TYPE, WME_SUBTYPE_IE}, WLC_IEM_VS_IE_PRIO_WME},
	{{MSFT_OUI, WME_OUI_TYPE, WME_SUBTYPE_PARAM_IE}, WLC_IEM_VS_IE_PRIO_WME},
	{{MSFT_OUI, WME_OUI_TYPE, WME_SUBTYPE_TSPEC}, WLC_IEM_VS_IE_PRIO_WME_TS},
};

/*
 * Known 24 bit OUI + 8 bit type
 */
static const struct {
	struct {
		uint8 oui[3];
		uint8 type;
	} cdi;
	wlc_iem_tag_t id;
} cdi322id[] = {
	{{BRCM_PROP_OUI, VHT_FEATURES_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_VHT},
	{{BRCM_PROP_OUI, HT_CAP_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_HT},
	{{BRCM_PROP_OUI, BRCM_EXTCH_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_EXT_CH},
#ifndef IBSS_RMC
	{{BRCM_PROP_OUI, RELMCAST_BRCM_PROP_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_RMC},
#endif // endif
	{{BRCM_PROP_OUI, MEMBER_OF_BRCM_PROP_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_PSTA},
	{{MSFT_OUI, WPA_OUI_TYPE}, WLC_IEM_VS_IE_PRIO_WPA},
	{{MSFT_OUI, WPS_OUI_TYPE}, WLC_IEM_VS_IE_PRIO_WPS},
	{{WFA_OUI, WFA_OUI_TYPE_HS20}, WLC_IEM_VS_IE_PRIO_HS20},
	{{WFA_OUI, WFA_OUI_TYPE_P2P}, WLC_IEM_VS_IE_PRIO_P2P},
#ifdef WLOSEN
	{{WFA_OUI, WFA_OUI_TYPE_OSEN}, WLC_IEM_VS_IE_PRIO_OSEN},
#endif	/* WLOSEN */
#if defined(WL_MBO) || defined(WL_OCE)
	{{WFA_OUI, WFA_OUI_TYPE_MBO_OCE}, WLC_IEM_VS_IE_PRIO_MBO_OCE},
#endif /* WL_MBO || WL_OCE */
#ifdef MULTIAP
	{{WFA_OUI, WFA_OUI_TYPE_MULTIAP}, WLC_IEM_VS_IE_PRIO_MULTIAP},
#endif	/* MULTIAP */
};

/*
 * Known 24 bit OUI
 */
static const struct {
	uint8 oui[3];
	wlc_iem_tag_t id;
} oui2id[] = {
	{BRCM_OUI, WLC_IEM_VS_IE_PRIO_BRCM},
#ifdef IBSS_RMC
	{RMC_PROP_OUI, WLC_IEM_VS_IE_PRIO_BRCM_RMC},
#endif // endif
};

/*
 *  * Ignore duplicate OUI's
 */
static bool wlc_ignore_assoc_oui(sta_vendor_oui_t  *assoc_oui,  uint8 *ie)
{
	int i;
	for (i = 0; i < assoc_oui->count; i++) {
		if (bcmp(&ie[TLV_BODY_OFF], &assoc_oui->oui[i], 3) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}
void
wlc_iem_vs_get_oui(wlc_iem_pvsie_data_t *data)
{
	uint8 *ie;
	wlc_iem_ft_pparm_t *ftpparm;
	sta_vendor_oui_t  *assoc_oui = NULL;

	ie = data->ie;
	ftpparm = data->pparm->ft;
	if (!ftpparm || !ie) {
		return;
	}

	if (data->ft == FC_ASSOC_REQ)
		assoc_oui = ftpparm->assocreq.assoc_oui;
	else if (data->ft == FC_ASSOC_RESP)
		assoc_oui = ftpparm->assocresp.assoc_oui;

	if (!assoc_oui) {
		return;
	}

	if ((data->ie_len < (TLV_BODY_OFF + DOT11_OUI_LEN)) || ie[0] != DOT11_MNG_VS_ID) {
		return;
	}
	if (wlc_ignore_assoc_oui(assoc_oui, ie)) {
		return;
	}

	if (assoc_oui->count < WLC_MAX_ASSOC_OUI_NUM) {
		assoc_oui->oui[assoc_oui->count][0] = ie[TLV_BODY_OFF];
		assoc_oui->oui[assoc_oui->count][1] = ie[TLV_BODY_OFF + 1];
		assoc_oui->oui[assoc_oui->count][2] = ie[TLV_BODY_OFF + 2];
		assoc_oui->count ++;
	}
}

/*
 * Map Vendor Specific IE to an id
 */
wlc_iem_tag_t
wlc_iem_vs_get_id(uint8 *ie)
{
	uint i;

	ASSERT(ie != NULL);

	/* TODO: arrange the elements in applicable arrays in a sorted order
	 * (by CDI decimal value) and apply a binary search here when arrays
	 * grow large...
	 */

	if (ie[TLV_LEN_OFF] >= 5) {
		for (i = 0; i < ARRAYSIZE(cdi482id); i ++) {
			if (bcmp(&ie[TLV_BODY_OFF], &cdi482id[i].cdi, 5) == 0)
				return cdi482id[i].id;
		}
	}
	if (ie[TLV_LEN_OFF] >= 4) {
		for (i = 0; i < ARRAYSIZE(cdi322id); i ++) {
			if (bcmp(&ie[TLV_BODY_OFF], &cdi322id[i].cdi, 4) == 0)
				return cdi322id[i].id;
		}
	}
	if (ie[TLV_LEN_OFF] >= 3) {
		for (i = 0; i < ARRAYSIZE(oui2id); i ++) {
			if (bcmp(&ie[TLV_BODY_OFF], oui2id[i].oui, 3) == 0)
				return oui2id[i].id;
		}
	}

	return WLC_IEM_VS_ID_MAX;
}

#if defined(BCMDBG)
int
wlc_iem_vs_dump(void *ctx, struct bcmstrbuf *b)
{
	uint i;

	BCM_REFERENCE(ctx);

	for (i = 0; i < ARRAYSIZE(cdi482id); i ++) {
		bcm_bprintf(b, "%u: ", cdi482id[i].id);
		bcm_bprhex(b, "", FALSE, (uint8 *)&cdi482id[i].cdi.oui,
			sizeof(cdi482id[i].cdi.oui));
		bcm_bprintf(b, "%02x", cdi482id[i].cdi.type);
		bcm_bprintf(b, "%02x\n", cdi482id[i].cdi.subtype);
	}
	for (i = 0; i < ARRAYSIZE(cdi322id); i ++) {
		bcm_bprintf(b, "%u: ", cdi322id[i].id);
		bcm_bprhex(b, "", FALSE, (uint8 *)&cdi322id[i].cdi.oui,
			sizeof(cdi322id[i].cdi.oui));
		bcm_bprintf(b, "%02x\n", cdi322id[i].cdi.type);
	}
	for (i = 0; i < ARRAYSIZE(oui2id); i ++) {
		bcm_bprintf(b, "%u: ", oui2id[i].id);
		bcm_bprhex(b, "", TRUE, oui2id[i].oui, sizeof(oui2id[i].oui));
	}

	return BCME_OK;
}
#endif // endif
