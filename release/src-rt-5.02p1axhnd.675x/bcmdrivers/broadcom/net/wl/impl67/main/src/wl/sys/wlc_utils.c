/*
 * driver utility functions
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_utils.c 784487 2020-02-28 06:14:45Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <ethernet.h>
#include <bcmip.h>
#include <wlioctl_defs.h>
#include <wlc_utils.h>
#include <wlioctl_utils.h>
#include <wlc.h>

const uint8 wlc_802_1x_hdr[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e};

void
wlc_uint64_add(uint32* high, uint32* low, uint32 inc_high, uint32 inc_low)
{
	uint32 old_l;
	uint32 new_l;

	old_l = *low;
	new_l = old_l + inc_low;
	*low = new_l;
	if (new_l < old_l) {
		/* carry */
		inc_high += 1;
	}
	*high += inc_high;
}

void
wlc_uint64_sub(uint32* a_high, uint32* a_low, uint32 b_high, uint32 b_low)
{
	if (b_low > *a_low) {
		/* low half needs a carry */
		b_high += 1;
	}
	*a_low -= b_low;
	*a_high -= b_high;
}

bool
wlc_uint64_lt(uint32 a_high, uint32 a_low, uint32 b_high, uint32 b_low)
{
	return (a_high < b_high ||
		(a_high == b_high && a_low < b_low));
}

uint32
wlc_uint64_div(uint64 a, uint64 b)
{
	while ((a > UINT32_MAX) || (b > UINT32_MAX)) {
		a >>= 1;
		b >>= 1;
	}
	if (b > 0) {
		return ((uint32)a) / ((uint32)b);
	}
	else {
		return 0;
	}
}

/* Given the beacon interval in kus, and a 64 bit TSF in us,
 * return the offset (in us) of the TSF from the last TBTT
 */
uint32
wlc_calc_tbtt_offset(uint32 bp, uint32 tsf_h, uint32 tsf_l)
{
	uint32 k, btklo, btkhi, offset;

	/* TBTT is always an even multiple of the beacon_interval,
	 * so the TBTT less than or equal to the beacon timestamp is
	 * the beacon timestamp minus the beacon timestamp modulo
	 * the beacon interval.
	 *
	 * TBTT = BT - (BT % BIu)
	 *      = (BTk - (BTk % BP)) * 2^10
	 *
	 * BT = beacon timestamp (usec, 64bits)
	 * BTk = beacon timestamp (Kusec, 54bits)
	 * BP = beacon interval (Kusec, 16bits)
	 * BIu = BP * 2^10 = beacon interval (usec, 26bits)
	 *
	 * To keep the calculations in uint32s, the modulo operation
	 * on the high part of BT needs to be done in parts using the
	 * relations:
	 * X*Y mod Z = ((X mod Z) * (Y mod Z)) mod Z
	 * and
	 * (X + Y) mod Z = ((X mod Z) + (Y mod Z)) mod Z
	 *
	 * So, if BTk[n] = uint16 n [0,3] of BTk.
	 * BTk % BP = SUM((BTk[n] * 2^16n) % BP , 0<=n<4) % BP
	 * and the SUM term can be broken down:
	 * (BTk[n] *     2^16n)    % BP
	 * (BTk[n] * (2^16n % BP)) % BP
	 *
	 * Create a set of power of 2 mod BP constants:
	 * K[n] = 2^(16n) % BP
	 *      = (K[n-1] * 2^16) % BP
	 * K[2] = 2^32 % BP = ((2^16 % BP) * 2^16) % BP
	 *
	 * BTk % BP = BTk[0-1] % BP +
	 *            (BTk[2] * K[2]) % BP +
	 *            (BTk[3] * K[3]) % BP
	 *
	 * Since K[n] < 2^16 and BTk[n] is < 2^16, then BTk[n] * K[n] < 2^32
	 */

	/* BTk = BT >> 10, btklo = BTk[0-3], bkthi = BTk[4-6] */
	btklo = (tsf_h << 22) | (tsf_l >> 10);
	btkhi = tsf_h >> 10;

	/* offset = BTk % BP */
	offset = btklo % bp;

	/* K[2] = ((2^16 % BP) * 2^16) % BP */
	k = (uint32)(1<<16) % bp;
	k = (uint32)(k * 1<<16) % (uint32)bp;

	/* offset += (BTk[2] * K[2]) % BP */
	offset += ((btkhi & 0xffff) * k) % bp;

	/* BTk[3] */
	btkhi = btkhi >> 16;

	/* k[3] = (K[2] * 2^16) % BP */
	k = (k << 16) % bp;

	/* offset += (BTk[3] * K[3]) % BP */
	offset += ((btkhi & 0xffff) * k) % bp;

	offset = offset % bp;

	/* convert offset from kus to us by shifting up 10 bits and
	 * add in the low 10 bits of tsf that we ignored
	 */
	offset = (offset << 10) + (tsf_l & 0x3FF);

#ifdef DEBUG_TBTT
	{
	uint32 offset2 = tsf_l % ((uint32)bp << 10);
	/* if the tsf is still in 32 bits, we can check the calculation directly */
	if (offset2 != offset && tsf_h == 0) {
		WL_ERROR(("tbtt offset calc error, offset2 %d offset %d\n",
		          offset2, offset));
	}
	}
#endif /* DEBUG_TBTT */

	return offset;
}

/* use the 64 bit tsf_timer{low,high} to extrapolahe 64 bit tbtt
 * to avoid any inconsistency between the 32 bit tsf_cfpstart and
 * the 32 bit tsf_timerhigh.
 * 'bcn_int' is in 1024TU.
 */
void
wlc_tsf64_to_next_tbtt64(uint32 bcn_int, uint32 *tsf_h, uint32 *tsf_l)
{
	uint32 bcn_offset;

	/* offset to last tbtt */
	bcn_offset = wlc_calc_tbtt_offset(bcn_int, *tsf_h, *tsf_l);
	/* last tbtt */
	wlc_uint64_sub(tsf_h, tsf_l, 0, bcn_offset);
	/* next tbtt */
	wlc_uint64_add(tsf_h, tsf_l, 0, bcn_int << 10);
}

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* rsn parms lookup */
bool
wlc_rsn_akm_lookup(struct rsn_parms *rsn, uint8 akm)
{
	uint count;

	for (count = 0; count < rsn->acount; count++) {
		if (rsn->auth[count] == akm)
			return TRUE;
	}
	return FALSE;
}

bool
wlc_rsn_ucast_lookup(struct rsn_parms *rsn, uint8 auth)
{
	uint i;

	for (i = 0; i < rsn->ucount; i++) {
		if (rsn->unicast[i] == auth)
			return TRUE;
	}

	return FALSE;
}
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

/* map Frame Type FC_XXXX to VNDR_IE_XXXX_FLAG */
static const uint32 fst2vieflag[] = {
	/* FC_SUBTYPE_ASSOC_REQ	0 */ VNDR_IE_ASSOCREQ_FLAG,
	/* FC_SUBTYPE_ASSOC_RESP 1 */ VNDR_IE_ASSOCRSP_FLAG,
	/* FC_SUBTYPE_REASSOC_REQ 2 */ VNDR_IE_ASSOCREQ_FLAG,
	/* FC_SUBTYPE_REASSOC_RESP 3 */ VNDR_IE_ASSOCRSP_FLAG,
	/* FC_SUBTYPE_PROBE_REQ 4 */ VNDR_IE_PRBREQ_FLAG,
	/* FC_SUBTYPE_PROBE_RESP 5 */ VNDR_IE_PRBRSP_FLAG,
	0,
	0,
	/* FC_SUBTYPE_BEACON 8 */ VNDR_IE_BEACON_FLAG,
	0,
	/* FC_SUBTYPE_DISASSOC 10 */ VNDR_IE_DISASSOC_FLAG,
	/* FC_SUBTYPE_AUTH 11 */ 0,
	/* FC_SUBTYPE_DEAUTH 12 */ 0
};

uint32
wlc_ft2vieflag(uint16 ft)
{
	uint16 fst = FT2FST(ft);

	ASSERT(fst < ARRAYSIZE(fst2vieflag));

	return fst < ARRAYSIZE(fst2vieflag) ? fst2vieflag[fst] : 0;
}

uint32
wlc_fst2vieflag(uint16 fst)
{
	ASSERT(fst < ARRAYSIZE(fst2vieflag));

	return fst < ARRAYSIZE(fst2vieflag) ? fst2vieflag[fst] : 0;
}

/* map Sequence Number in FC_ATUH to VNDR_IE_XXXX_FLAG */
static const uint32 auth2vieflag[] = {
	/* seq 1 */ VNDR_IE_AUTHREQ_FLAG,
	/* seq 2 */ VNDR_IE_AUTHRSP_FLAG,
	/* seq 3 */ 0,
	/* seq 4 */ 0
};

uint32
wlc_auth2vieflag(int seq)
{
	ASSERT(seq >= 1 && (uint)seq <= ARRAYSIZE(auth2vieflag));

	return --seq >= 0 && (uint)seq < ARRAYSIZE(auth2vieflag) ? auth2vieflag[seq] : 0;
}

/*
 * Description: This function is called to do the reverse translation
 *
 * Input    eh - pointer to the ethernet header
 */
int32
wlc_mcast_reverse_translation(struct ether_header *eh)
{
	uint8 *iph;
	uint32 dest_ip;

	iph = (uint8 *)eh + ETHER_HDR_LEN;
	dest_ip = ntoh32(*((uint32 *)(iph + IPV4_DEST_IP_OFFSET)));

	/* Only IP packets are handled */
	if (eh->ether_type != hton16(ETHER_TYPE_IP))
		return BCME_ERROR;

	/* Non-IPv4 multicast packets are not handled */
	if (IP_VER(iph) != IP_VER_4)
		return BCME_ERROR;

	/*
	 * The packet has a multicast IP and unicast MAC. That means
	 * we have to do the reverse translation
	 */
	if (IPV4_ISMULTI(dest_ip) && !ETHER_ISMULTI(&eh->ether_dhost)) {
		ETHER_FILL_MCAST_ADDR_FROM_IP(eh->ether_dhost, dest_ip);
		return BCME_OK;
	}

	return BCME_ERROR;
}

bool
is_igmp(struct ether_header *eh)
{
	uint8 *iph = (uint8 *)eh + ETHER_HDR_LEN;

	if ((eh->ether_type == HTON16(ETHER_TYPE_IP)) &&
	    (IP_VER(iph) == IP_VER_4) &&
	    (IPV4_PROT(iph) == IP_PROT_IGMP))
		return TRUE;
	else
		return FALSE;
}

/* map between DOT11_BSSTYPE_ value space and WL_BSSTYPE_ value space */
/* TODO: wl2dot11/wlc_bsstype_wl2dot11() and dot112wl/wlc_bsstype_dot112wl
 * are for wl utils compatibility. Remove them once we no longer need them.
 */

static const uint8 wl2dot11[] = {
	DOT11_BSSTYPE_INDEPENDENT,
	DOT11_BSSTYPE_INFRASTRUCTURE,
	DOT11_BSSTYPE_ANY
};

uint
wlc_bsstype_wl2dot11(uint wl)
{
	if (wl < ARRAYSIZE(wl2dot11)) {
		return wl2dot11[wl];
	}
	ASSERT(wl >= ARRAYSIZE(wl2dot11));
	return DOT11_BSSTYPE_INFRASTRUCTURE;
}

static const uint8 dot112wl[] = {
	WL_BSSTYPE_INFRA,
	WL_BSSTYPE_INDEP,
	WL_BSSTYPE_ANY
};

uint
wlc_bsstype_dot112wl(uint dot11)
{
	if (dot11 < ARRAYSIZE(dot112wl)) {
		return dot112wl[dot11];
	}
	ASSERT(dot11 >= ARRAYSIZE(dot112wl));
	return WL_BSSTYPE_INFRA;
}

/* For debugging - return the string name of a dot11_bsstype_ value */

static const char *dot11bsstypename[] = {
	"Infra",
	"IBSS",
	"Any"
};

const char *
wlc_bsstype_dot11name(uint dot11)
{
	if (dot11 < ARRAYSIZE(dot11bsstypename))
		return dot11bsstypename[dot11];
	return "Unknown";
}

/* XXX These should be in wl_util.c or similar file.
 * no such file, so parking these here for now.
 * Since all these reason codes seems to be part of core code,
 * should we rename them to prefix WLC_ instead of WL_
 */
static const struct {
	int rc;
	char name[20];
} wl_reinit_names[] = {
	{WL_REINIT_RC_NONE,             "NONE"},            /* 0 */
	{WL_REINIT_RC_PS_SYNC,          "PS_SYNC"},         /* 1 */
	{WL_REINIT_RC_PSM_WD,           "PSM_WD"},          /* 2 */
	{WL_REINIT_RC_MAC_WAKE,         "MAC_WAKE"},        /* 3 */
	{WL_REINIT_RC_MAC_SUSPEND,      "MAC_SUSP"},        /* 4 */
	{WL_REINIT_RC_MAC_SPIN_WAIT,    "SPIN_WAIT"},       /* 5 */
	{WL_REINIT_RC_AXI_BUS_ERROR,    "AXI_BUS_ERR"},     /* 6 */
	{WL_REINIT_RC_DEVICE_REMOVED,   "DEV_REMOVED"},     /* 7 */
	{WL_REINIT_RC_PCIE_FATAL_ERROR, "PCIE_AER"},        /* 8 */
	{WL_REINIT_RC_OL_FW_TRAP,       "FW_TRAP"},         /* 9 */
	{WL_REINIT_RC_FIFO_ERR,         "FIFO_ERR"},        /* 10 */
	{WL_REINIT_RC_INV_TX_STATUS,    "INV_TX_STS"},      /* 11 */
	{WL_REINIT_RC_MQ_ERROR,		"MQ_ERROR"},        /* 12 */
	{WL_REINIT_RC_PHYTXERR_THRESH,	"PHYTX_THRESHOLD"}, /* 13 */
	{WL_REINIT_RC_USER_FORCED,      "USER_INITIATED"},  /* 14 */
	{WL_REINIT_RC_FULL_RESET,       "FULL_RESET"},      /* 15 */
	{WL_REINIT_RC_AP_BEACON,        "AP_BCN"},          /* 16 */
	{WL_REINIT_RC_PM_EXCESSED,      "PM_EXCESS"},       /* 17 */
	{WL_REINIT_RC_NO_CLK,           "NO_CLOCK"},        /* 18 */
	{WL_REINIT_RC_SW_ASSERT,        "ASSERT"},          /* 19 */
	{WL_REINIT_RC_PSM_JMP0,         "PSM_JMP0"},        /* 20 */
	{WL_REINIT_RC_PSM_RUN,          "PSM_RUN"},         /* 21 */
	{WL_REINIT_RC_ENABLE_MAC,       "MAC_ENABLE"},      /* 22 */
	{WL_REINIT_RC_SCAN_TIMEOUT,     "SCAN_TIMEOUT"},    /* 23 */
	{WL_REINIT_RC_JOIN_TIMEOUT,     "JOIN_TIMEOUT"},    /* 24 */
	{WL_REINIT_RC_LINK_NOT_ACTIVE,	"PCIeLinkDown"},    /* 25 */
	{WL_REINIT_RC_PCI_CFG_RD_FAIL,	"PCIeCfgRdFailed"}, /* 26 */
	{WL_REINIT_RC_INV_VEN_ID,	"PCIeVenIDInv"},    /* 27 */
	{WL_REINIT_RC_INV_DEV_ID,	"PCIeDevIDInv"},    /* 28 */
	{WL_REINIT_RC_INV_BAR0,		"PCIeBar0Inv"},     /* 29 */
	{WL_REINIT_RC_INV_BAR2,		"PCIBar2Inv"},      /* 30 */
	{WL_REINIT_RC_AER_UC_FATAL,	"PCIeAERURFatal"},    /* 31 */
	{WL_REINIT_RC_AER_UC_NON_FATAL,	"PCIeAERURNonFatal"}, /* 32 */
	{WL_REINIT_RC_AER_CORR,		"PCIeAERCorr"},       /* 33 */
	{WL_REINIT_RC_AER_DEV_STS,	"PCIeErr-DevStatus"}, /* 34 */
	{WL_REINIT_RC_PCIe_STS,		"PCIErr-Status"},     /* 35 */
	{WL_REINIT_RC_MMIO_RD_FAIL,	"MMIORegRdFail"},     /* 36 */
	{WL_REINIT_RC_MMIO_RD_INVAL,	"MMIORegRdInv"},      /* 37 */
	{WL_REINIT_RC_MMIO_ARM_MEM_RD_FAIL,	"ARMMemRdFail"},        /* 38 */
	{WL_REINIT_RC_MMIO_ARM_MEM_INVAL,	"ARMMemRdInval"},       /* 39 */
	{WL_REINIT_RC_SROM_LOAD_FAILED,		"SROMLoadFail"},        /* 40 */
	{WL_REINIT_RC_PHY_CRASH,		"PHY_CRASH"},           /* 41 */
	{WL_REINIT_TX_STALL,            "TX_STALL"},                    /* 42 */
	{WL_REINIT_RC_TX_FLOW_CONTROL_BLOCKED,	"TxFLC_blocked"},       /* 43 */
	{WL_REINIT_RC_RX_HC_FAIL,	"RX_STALL"}, /* 44 */
	{WL_REINIT_RC_RX_DMA_STALL,	"RXDMA_STALL"},         /* 45 */
	{WL_REINIT_UTRACE_BUF_OVERLAP_SR, "Utrace_into_SR"},	/* 46 */
	{WL_REINIT_UTRACE_TPL_OUT_BOUNDS, "UtraceTemplateOut"},	/* 47 */
	{WL_REINIT_UTRACE_TPL_OSET_STRT0, "UtraceOffset0"},	/* 48 */
	{WL_REINIT_RC_PHYTXERR,           "PHYTXERR"},	        /* 49 */
	{WL_REINIT_RC_PSM_FATAL_SUSP,     "PSM_FATAL_SUSP"},	/* 50 */
	{WL_REINIT_RC_TX_FIFO_SUSP,       "TX_FIFO_SUSP"},	/* 51 */
	{WL_REINIT_RC_MAC_ENABLE,         "RC_MAC_ENABLE"},	/* 52 */
	{WL_REINIT_RC_SCAN_STALLED,       "SCAN_STALLED"},	/* 53 */
	{WL_REINIT_RC_RX_DMA_BUF_EMPTY,   "RxDmaBufferEmptyFail"},	/* 54 */
};

const char *
wl_get_reinit_rc_name(int rc)
{
	uint32 i;

	for (i = 0; i < (sizeof(wl_reinit_names)/sizeof(wl_reinit_names[0])); i++) {
		if (wl_reinit_names[i].rc == rc) {
			return wl_reinit_names[i].name;
		}
	}

	return "UNKNOWN";
}

struct wlc_bsscfg *
bcm_iov_bsscfg_find_from_wlcif(struct wlc_info *wlc, struct wlc_if *wl_if)
{
		return wlc_bsscfg_find_by_wlcif((wlc) ? wlc : NULL, wl_if);
}

bool
wlc_ssid_cmp(uint8 *ssid1, uint8 *ssid2, uint16 len1, uint16 len2)
{
	if (len1 == len2) {
		return (memcmp(ssid1, ssid2, len1) == 0);
	}
	return FALSE;
}

uint8
wlc_chspec_bw2bwcap_bit(uint16 bw)
{
	switch (bw) {
	case WL_CHANSPEC_BW_20:
		return WLC_BW_20MHZ_BIT;
	case WL_CHANSPEC_BW_40:
		return WLC_BW_40MHZ_BIT;
	case WL_CHANSPEC_BW_80:
		return WLC_BW_80MHZ_BIT;
	case WL_CHANSPEC_BW_8080:
	case WL_CHANSPEC_BW_160:
		return WLC_BW_160MHZ_BIT;
	default:
		ASSERT(0);
	}
	return 0;
}

#pragma GCC diagnostic push  // requires GCC 4.6
#pragma GCC diagnostic ignored "-Wcast-qual"
uint32
wlc_calc_short_ssid(const uint8* const_ssid, const int ssid_len)
{
	uint8 *ssid = (uint8*) const_ssid;

	return ~hndcrc32(ssid, ssid_len, CRC32_INIT_VALUE);
}
#pragma GCC diagnostic pop
