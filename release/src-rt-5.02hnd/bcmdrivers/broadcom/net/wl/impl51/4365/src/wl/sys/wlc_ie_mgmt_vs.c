/*
 * @file
 * IE management module Vendor Specific IE utilities
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: wlc_ie_mgmt_vs.c 467328 2014-04-03 01:23:40Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <proto/802.11.h>
#ifdef BCMCCX
#include <proto/802.11_ccx.h>
#endif
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

/*
 * Known 24 bit OUI + 8 bit type + 8 bit subtype
 */
static const struct {
	struct {
		uint8 oui[3];
		uint8 type;
		uint8 subtype;
	} cdi;
	uint8 id;
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
	uint8 id;
} cdi322id[] = {
	{{BRCM_PROP_OUI, VHT_FEATURES_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_VHT},
	{{BRCM_PROP_OUI, HT_CAP_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_HT},
	{{BRCM_PROP_OUI, BRCM_EXTCH_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_EXT_CH},
#ifndef IBSS_RMC
	{{BRCM_PROP_OUI, RELMCAST_BRCM_PROP_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_RMC},
#endif
	{{BRCM_PROP_OUI, MEMBER_OF_BRCM_PROP_IE_TYPE}, WLC_IEM_VS_IE_PRIO_BRCM_PSTA},
	{{MSFT_OUI, WPA_OUI_TYPE}, WLC_IEM_VS_IE_PRIO_WPA},
	{{MSFT_OUI, WPS_OUI_TYPE}, WLC_IEM_VS_IE_PRIO_WPS},
	{{WFA_OUI, WFA_OUI_TYPE_HS20}, WLC_IEM_VS_IE_PRIO_HS20},
	{{WFA_OUI, WFA_OUI_TYPE_P2P}, WLC_IEM_VS_IE_PRIO_P2P},
#ifdef WLOSEN
	{{WFA_OUI, WFA_OUI_TYPE_OSEN}, WLC_IEM_VS_IE_PRIO_OSEN},
#endif	/* WLOSEN */
#ifdef BCMCCX
	{{CISCO_AIRONET_OUI, CCX_QOS_IE_TYPE}, WLC_IEM_VS_IE_PRIO_CCX_QOS},
	{{CISCO_AIRONET_OUI, CCX_CAC_TS_RATESET_TYPE}, WLC_IEM_VS_IE_PRIO_CCX_TS_RS},
	{{CISCO_AIRONET_OUI, CCX_VERSION_IE_TYPE}, WLC_IEM_VS_IE_PRIO_CCX_VER},
	{{CISCO_AIRONET_OUI, CCX_RM_CAP_IE_TYPE}, WLC_IEM_VS_IE_PRIO_CCX_RM_CAP},
#endif
};

/*
 * Known 24 bit OUI
 */
static const struct {
	uint8 oui[3];
	uint8 id;
} oui2id[] = {
	{BRCM_OUI, WLC_IEM_VS_IE_PRIO_BRCM},
#ifdef IBSS_RMC
	{RMC_PROP_OUI, WLC_IEM_VS_IE_PRIO_BRCM_RMC},
#endif
};

/*
 * Map Vendor Specific IE to an id
 */
uint8
wlc_iem_vs_get_id(wlc_iem_info_t *iem, uint8 *ie)
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

	return WLC_IEM_VS_IE_ID_UNK;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
int
wlc_iem_vs_dump(void *ctx, struct bcmstrbuf *b)
{
	uint i;

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
		bcm_bprhex(b, "", TRUE, (uint8 *)oui2id[i].oui, sizeof(oui2id[i].oui));
	}

	return BCME_OK;
}
#endif /* BCMDBG || BCMDBG_DUMP */
