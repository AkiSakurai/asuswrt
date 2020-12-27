/*
 * Common (OS-independent) portion of
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

/**
 * @file
 * @brief
 * This file implements the address matching interface to be used
 * by the high driver or monolithic driver. d11 corerev >= 40 supports
 * AMT with attributes for matching in addition to the address. Prior
 * versions ignore the attributes provided in the interface
 */

#include <wlc_cfg.h>

#if defined(WLC_LOW) && !defined(WLC_HIGH)
#error "File is for use in monolithic or high driver only"
#endif  /* WLC_LOW */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/bcmeth.h>
#include <proto/bcmevent.h>
#include <bcmwifi_channels.h>
#include <siutils.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlioctl.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_bmac.h>
#include <wlc_txbf.h>

#include <wlc_addrmatch.h>

#define HAS_AMT(wlc) D11REV_GE(wlc->pub->corerev, 40)
#define IS_PRE_AMT(wlc) D11REV_LT(wlc->pub->corerev, 40)

uint16
wlc_set_addrmatch(wlc_info_t *wlc, int idx, const struct ether_addr *addr,
	uint16 attr)

{
	uint16 prev_attr = 0;

#ifdef BCMDBG
	if (WL_WSEC_ON()) {
		char addr_str[ETHER_ADDR_STR_LEN];
		WL_WSEC(("wl%d: %s: idx %d addr %s attr 0x%04x\n", WLCWLUNIT(wlc),
			__FUNCTION__, idx, bcm_ether_ntoa(addr, addr_str), attr));
	}
#endif /* BCMDBG */

	ASSERT(wlc->pub->corerev > 4);
	if (HAS_AMT(wlc)) {
		switch (idx) {
		case WLC_ADDRMATCH_IDX_MAC:
			prev_attr = wlc_bmac_write_amt(wlc->hw, AMT_IDX_MAC, addr, attr);
			break;
		case WLC_ADDRMATCH_IDX_BSSID:
			prev_attr = wlc_bmac_write_amt(wlc->hw, AMT_IDX_BSSID, addr, attr);
			break;
		default:
			ASSERT(idx >= 0);
			if (idx < AMT_SIZE) {
				prev_attr = wlc_bmac_write_amt(wlc->hw, idx, addr, attr);
#ifdef WL_BEAMFORMING
				if (TXBF_ENAB(wlc->pub))
					wlc_txfbf_update_amt_idx(wlc->txbf, idx, addr);
#endif
			}
			break;
		}
		goto done;
	}

	switch (idx) {
	case WLC_ADDRMATCH_IDX_MAC:
		wlc_bmac_set_rxe_addrmatch(wlc->hw, RCM_MAC_OFFSET, addr);
		break;
	case WLC_ADDRMATCH_IDX_BSSID:
		wlc_bmac_set_rxe_addrmatch(wlc->hw, RCM_BSSID_OFFSET, addr);
		break;
	default:
		ASSERT(idx >= 0);
		if (idx < RCMTA_SIZE)
			wlc_bmac_set_rcmta(wlc->hw, idx, addr);
		break;
	}

done:
	return prev_attr;
}

uint16
wlc_clear_addrmatch(wlc_info_t *wlc, int idx)
{
	return wlc_set_addrmatch(wlc, idx, &ether_null, 0);
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WL_BEAMFORMING)
void
wlc_get_addrmatch(wlc_info_t *wlc, int idx, struct ether_addr *addr,
	uint16 *attr)
{
	ASSERT(wlc->pub->corerev > 4);

	if (HAS_AMT(wlc)) {
		switch (idx) {
		case WLC_ADDRMATCH_IDX_MAC: idx = AMT_IDX_MAC; break;
		case WLC_ADDRMATCH_IDX_BSSID: idx = AMT_IDX_BSSID; break;
		default: break;
		}
		wlc_bmac_read_amt(wlc->hw, idx, addr, attr);
		return;
	}

	/* no support for reading the rxe address match registers for now.
	 * can be added if necessary by supporting it in the bmac layer
	 * and the corresponding RPCs for the split driver.
	 */
	if (idx >= 0) {
		wlc_bmac_get_rcmta(wlc->hw, idx, addr);
		*attr =  !ETHER_ISNULLADDR(addr) ? AMT_ATTR_VALID : 0;
	} else {
		memset(addr, 0, sizeof(*addr));
		*attr = 0;
	}
}
#endif 

#if defined(BCMDBG_DUMP)
static int
wlc_dump_rcmta(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int i;
	struct ether_addr ea;
	char eabuf[ETHER_ADDR_STR_LEN];

	ASSERT(IS_PRE_AMT(wlc));

	if (!wlc->clk)
		return BCME_NOCLK;

	for (i = 0; i < RCMTA_SIZE; i ++) {
		uint16 attr;
		wlc_get_addrmatch(wlc, i, &ea, &attr);
		if (ETHER_ISNULLADDR(&ea) && !(attr & AMT_ATTR_VALID))
			continue;
		bcm_bprintf(b, "%d %s\n", i, bcm_ether_ntoa(&ea, eabuf));
	}

	return BCME_OK;
}

static int
wlc_dump_amt(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int i;
	struct ether_addr ea;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 attr;
	char flagstr[64];
	static const bcm_bit_desc_t attr_flags[] = {
		{AMT_ATTR_VALID, "Valid"},
		{AMT_ATTR_A1, "A1"},
		{AMT_ATTR_A2, "A2"},
		{AMT_ATTR_A3, "A3"},
		{0, NULL}
	};

	ASSERT(HAS_AMT(wlc));

	if (!wlc->clk)
		return BCME_NOCLK;

	for (i = 0; i < AMT_SIZE; i ++) {
		wlc_get_addrmatch(wlc, i, &ea, &attr);
		if (ETHER_ISNULLADDR(&ea) && !(attr & AMT_ATTR_VALID))
			continue;

		bcm_ether_ntoa(&ea, eabuf);
		if (attr != 0) {
			bcm_format_flags(attr_flags, attr, flagstr, 64);
			bcm_bprintf(b, "%02d %s 0x%04x (%s)\n", i, eabuf, attr, flagstr);
		} else {
			bcm_bprintf(b, "%02d %s 0x%04x\n", i, eabuf, attr);
		}
	}

	return BCME_OK;
}

int
wlc_dump_addrmatch(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	ASSERT(wlc->pub->corerev > 4);

	if (!wlc->clk)
		return BCME_NOCLK;

	if (HAS_AMT(wlc)) {
		wlc_dump_amt(wlc, b);
		return BCME_OK;
	}
	wlc_dump_rcmta(wlc, b);
	return BCME_OK;
}
#endif 
