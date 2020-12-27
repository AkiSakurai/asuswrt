/*
 * Common [OS-independent] rate management
 * 802.11 Networking Adapter Device Driver.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_rate.c,v 1.101.2.5 2011-02-08 00:07:24 Exp $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <proto/802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wl_dbg.h>
#include <wlc_pub.h>

/* Rate info per rate: It tells whether a rate is ofdm or not and its phy_rate value */
const uint8 rate_info[WLC_MAXRATE + 1] = {
	/*  0     1     2     3     4     5     6     7     8     9 */
/*   0 */ 0x00, 0x00, 0x0a, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  10 */ 0x00, 0x37, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8f, 0x00,
/*  20 */ 0x00, 0x00, 0x6e, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  30 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8e, 0x00, 0x00, 0x00,
/*  40 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x00,
/*  50 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  60 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  70 */ 0x00, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  80 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  90 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
/* 100 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8c
};

/* rates are in units of Kbps */
const mcs_info_t mcs_table[MCS_TABLE_SIZE] = {
	/* MCS  0: SS 1, MOD: BPSK,  CR 1/2 */
	{6500,   13500,  CEIL(6500*10, 9),   CEIL(13500*10, 9),  0x00, WLC_RATE_6M},
	/* MCS  1: SS 1, MOD: QPSK,  CR 1/2 */
	{13000,  27000,  CEIL(13000*10, 9),  CEIL(27000*10, 9),  0x08, WLC_RATE_12M},
	/* MCS  2: SS 1, MOD: QPSK,  CR 3/4 */
	{19500,  40500,  CEIL(19500*10, 9),  CEIL(40500*10, 9),  0x0A, WLC_RATE_18M},
	/* MCS  3: SS 1, MOD: 16QAM, CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9),  0x10, WLC_RATE_24M},
	/* MCS  4: SS 1, MOD: 16QAM, CR 3/4 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x12, WLC_RATE_36M},
	/* MCS  5: SS 1, MOD: 64QAM, CR 2/3 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0x19, WLC_RATE_48M},
	/* MCS  6: SS 1, MOD: 64QAM, CR 3/4 */
	{58500,  121500, CEIL(58500*10, 9),  CEIL(121500*10, 9), 0x1A, WLC_RATE_54M},
	/* MCS  7: SS 1, MOD: 64QAM, CR 5/6 */
	{65000,  135000, CEIL(65000*10, 9),  CEIL(135000*10, 9), 0x1C, WLC_RATE_54M},
	/* MCS  8: SS 2, MOD: BPSK,  CR 1/2 */
	{13000,  27000,  CEIL(13000*10, 9),  CEIL(27000*10, 9),  0x40, WLC_RATE_6M},
	/* MCS  9: SS 2, MOD: QPSK,  CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9),  0x48, WLC_RATE_12M},
	/* MCS 10: SS 2, MOD: QPSK,  CR 3/4 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x4A, WLC_RATE_18M},
	/* MCS 11: SS 2, MOD: 16QAM, CR 1/2 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0x50, WLC_RATE_24M},
	/* MCS 12: SS 2, MOD: 16QAM, CR 3/4 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0x52, WLC_RATE_36M},
	/* MCS 13: SS 2, MOD: 64QAM, CR 2/3 */
	{104000, 216000, CEIL(104000*10, 9), CEIL(216000*10, 9), 0x59, WLC_RATE_48M},
	/* MCS 14: SS 2, MOD: 64QAM, CR 3/4 */
	{117000, 243000, CEIL(117000*10, 9), CEIL(243000*10, 9), 0x5A, WLC_RATE_54M},
	/* MCS 15: SS 2, MOD: 64QAM, CR 5/6 */
	{130000, 270000, CEIL(130000*10, 9), CEIL(270000*10, 9), 0x5C, WLC_RATE_54M},
	/* MCS 16: SS 3, MOD: BPSK,  CR 1/2 */
	{19500,  40500,  CEIL(19500*10, 9),  CEIL(40500*10, 9),  0x80, WLC_RATE_6M},
	/* MCS 17: SS 3, MOD: QPSK,  CR 1/2 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x88, WLC_RATE_12M},
	/* MCS 18: SS 3, MOD: QPSK,  CR 3/4 */
	{58500,  121500, CEIL(58500*10, 9),  CEIL(121500*10, 9), 0x8A, WLC_RATE_18M},
	/* MCS 19: SS 3, MOD: 16QAM, CR 1/2 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0x90, WLC_RATE_24M},
	/* MCS 20: SS 3, MOD: 16QAM, CR 3/4 */
	{117000, 243000, CEIL(117000*10, 9), CEIL(243000*10, 9), 0x92, WLC_RATE_36M},
	/* MCS 21: SS 3, MOD: 64QAM, CR 2/3 */
	{156000, 324000, CEIL(156000*10, 9), CEIL(324000*10, 9), 0x99, WLC_RATE_48M},
	/* MCS 22: SS 3, MOD: 64QAM, CR 3/4 */
	{175500, 364500, CEIL(175500*10, 9), CEIL(364500*10, 9), 0x9A, WLC_RATE_54M},
	/* MCS 23: SS 3, MOD: 64QAM, CR 5/6 */
	{195000, 405000, CEIL(195000*10, 9), CEIL(405000*10, 9), 0x9B, WLC_RATE_54M},
	/* MCS 24: SS 4, MOD: BPSK,  CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9), 0xC0, WLC_RATE_6M},
	/* MCS 25: SS 4, MOD: QPSK,  CR 1/2 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0xC8, WLC_RATE_12M},
	/* MCS 26: SS 4, MOD: QPSK,  CR 3/4 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0xCA, WLC_RATE_18M},
	/* MCS 27: SS 4, MOD: 16QAM, CR 1/2 */
	{104000, 216000, CEIL(104000*10, 9), CEIL(216000*10, 9), 0xD0, WLC_RATE_24M},
	/* MCS 28: SS 4, MOD: 16QAM, CR 3/4 */
	{156000, 324000, CEIL(156000*10, 9), CEIL(324000*10, 9), 0xD2, WLC_RATE_36M},
	/* MCS 29: SS 4, MOD: 64QAM, CR 2/3 */
	{208000, 432000, CEIL(208000*10, 9), CEIL(432000*10, 9), 0xD9, WLC_RATE_48M},
	/* MCS 30: SS 4, MOD: 64QAM, CR 3/4 */
	{234000, 486000, CEIL(234000*10, 9), CEIL(486000*10, 9), 0xDA, WLC_RATE_54M},
	/* MCS 31: SS 4, MOD: 64QAM, CR 5/6 */
	{260000, 540000, CEIL(260000*10, 9), CEIL(540000*10, 9), 0xDB, WLC_RATE_54M},
	/* MCS 32: SS 1, MOD: BPSK,  CR 1/2 */
	{0,      6000,   0, CEIL(6000*10, 9), 0x00, WLC_RATE_6M},
	};


/* phycfg for legacy OFDM frames: code rate, modulation scheme, spatial streams
 *   Number of spatial streams: always 1
 *   other fields: refer to table 78 of section 17.3.2.2 of the original .11a standard
 */
typedef struct legacy_phycfg {
	uint32	rate_ofdm;	/* ofdm mac rate */
	uint8	tx_phy_ctl3;	/* phy ctl byte 3, code rate, modulation type, # of streams */
} legacy_phycfg_t;

#define LEGACY_PHYCFG_TABLE_SIZE	12	/* Number of legacy_rate_cfg entries in the table */

/* In CCK mode LPPHY overloads OFDM Modulation bits with CCK Data Rate */
/* Eventually MIMOPHY would also be converted to this format */
/* 0 = 1Mbps; 1 = 2Mbps; 2 = 5.5Mbps; 3 = 11Mbps */
static const legacy_phycfg_t	legacy_phycfg_table[LEGACY_PHYCFG_TABLE_SIZE] = {
	{WLC_RATE_1M,  0x00},	/* CCK  1Mbps,  data rate  0 */
	{WLC_RATE_2M,  0x08},	/* CCK  2Mbps,  data rate  1 */
	{WLC_RATE_5M5,  0x10},	/* CCK  5.5Mbps,  data rate  2 */
	{WLC_RATE_11M,  0x18},	/* CCK  11Mbps,  data rate   3 */
	{WLC_RATE_6M,  0x00},	/* OFDM  6Mbps,  code rate 1/2, BPSK,   1 spatial stream */
	{WLC_RATE_9M,  0x02},	/* OFDM  9Mbps,  code rate 3/4, BPSK,   1 spatial stream */
	{WLC_RATE_12M, 0x08},	/* OFDM  12Mbps, code rate 1/2, QPSK,   1 spatial stream */
	{WLC_RATE_18M, 0x0A},	/* OFDM  18Mbps, code rate 3/4, QPSK,   1 spatial stream */
	{WLC_RATE_24M, 0x10},	/* OFDM  24Mbps, code rate 1/2, 16-QAM, 1 spatial stream */
	{WLC_RATE_36M, 0x12},	/* OFDM  36Mbps, code rate 3/4, 16-QAM, 1 spatial stream */
	{WLC_RATE_48M, 0x19},	/* OFDM  48Mbps, code rate 2/3, 64-QAM, 1 spatial stream */
	{WLC_RATE_54M, 0x1A},	/* OFDM  54Mbps, code rate 3/4, 64-QAM, 1 spatial stream */
};

/* Hardware rates (also encodes default basic rates) */

const wlc_rateset_t cck_ofdm_mimo_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

const wlc_rateset_t ofdm_mimo_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{       0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

/* Default ratesets that include MCS32 for 40BW channels */
const wlc_rateset_t cck_ofdm_40bw_mimo_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

const wlc_rateset_t ofdm_40bw_mimo_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

const wlc_rateset_t cck_ofdm_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

const wlc_rateset_t gphy_legacy_rates = {
	4,
	{ /*	1b,   2b,   5.5b,  11b Mbps */
		0x82, 0x84, 0x8b, 0x96
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

const wlc_rateset_t ofdm_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

const wlc_rateset_t cck_rates = {
	4,
	{ /*	1b,   2b,   5.5,  11 Mbps */
		0x82, 0x84, 0x0b, 0x16
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

/* set of rates in supported rates elt when splitting rates between it and the Extended Rates elt */
const wlc_rateset_t wlc_lrs_rates = {
	8,
	{ /*	1,    2,    5.5,  11,   18,   24,   36,   54 Mbps */
		0x02, 0x04, 0x0b, 0x16, 0x24, 0x30, 0x48, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};


/* Rateset including only 1 and 2 Mbps, without basic rate annotation.
 * This is used for a wlc_rate_hwrs_filter_sort_validate param when limiting a rateset for
 * Japan Channel 14 regulatory compliance using early radio revs
 */
const wlc_rateset_t rate_limit_1_2 = {
	2,
	{ /*	1,    2  Mbps */
		0x02, 0x04
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	}
};

static bool wlc_rateset_valid(wlc_rateset_t *rs, bool check_brate);

/* check if rateset is valid.
 * if check_brate is true, rateset without a basic rate is considered NOT valid.
 */
static bool
wlc_rateset_valid(wlc_rateset_t *rs, bool check_brate)
{
	uint idx;

	if (!rs->count)
		return FALSE;

	if (!check_brate)
		return TRUE;

	/* error if no basic rates */
	for (idx = 0; idx < rs->count; idx++) {
		if (rs->rates[idx] & WLC_RATE_FLAG)
			return TRUE;
	}
	return FALSE;
}

#ifdef WL11N
void
wlc_rateset_mcs_upd(wlc_rateset_t *rs, uint8 nstreams)
{
	int i;

	for (i = nstreams; i < MAX_STREAMS_SUPPORTED; i++)
		rs->mcs[i] = 0;
}
#endif

/* filter BSS Membership from rateset
 */
bool
wlc_bss_membership_filter(wlc_rateset_t *rs)
{
	uint idx, rate_idx;
	wlc_rateset_t local;

	if (!rs->count)
		return FALSE;

	bzero(&local, sizeof(wlc_rateset_t));

	for (idx = rate_idx = 0; idx < rs->count; idx++) {
		if ((rs->rates[idx] & RATE_MASK) == WLC_HTPHY)
			rs->htphy_membership = rs->rates[idx];
		else {
			local.rates[rate_idx++] = rs->rates[idx];
			local.count++;
		}
	}
	rs->count = local.count;
	bcopy(local.rates, rs->rates, WLC_NUMRATES);

	return TRUE;
}

/* validate both rs1, rs2 and merge rs1 and rs2 and result into rs1 */
void
wlc_rateset_merge(wlc_rateset_t *rs1, wlc_rateset_t *rs2)
{
	uint8 rateset[WLC_MAXRATE+1];
	uint8 r;
	uint i;

	bzero(rateset, sizeof(rateset));

	WL_INFORM(("rs1 count is %d and rs2 ratecount is %d\n", rs1->count, rs2->count));

	/* validate both ratesets */
	for (i = 0; i < rs1->count; i++) {
		/* mask off "basic rate" bit, WLC_RATE_FLAG */
		r = (int)rs1->rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			WL_INFORM(("%s: bad rate in rs1 rate set 0x%x\n", __FUNCTION__, r));
			continue;
		}
		rateset[r] |= rs1->rates[i]; /* preserve basic bit! */
	}

	for (i = 0; i < rs2->count; i++) {
		/* mask off "basic rate" bit, WLC_RATE_FLAG */
		r = (int)rs2->rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			WL_INFORM(("%s: bad rate in rs2 rate set 0x%x\n", __FUNCTION__, r));
			continue;
		}
		rateset[r] |= rs2->rates[i]; /* preserve basic bit! */
	}

	/* merge it now */
	rs1->count = 0;
	for (i = 0; i < WLC_MAXRATE+1; i++) {
		if (rateset[i]) {
			rs1->rates[rs1->count] = rateset[i];
			rs1->count++;
			if (rs1->count > WLC_NUMRATES) {
				WL_ERROR(("%s: number of rates %d can't be more than %d\n",
					__FUNCTION__, rs1->count, WLC_NUMRATES));
				ASSERT(0);
				return;
			}
		}
	}
}

/* filter based on hardware rateset, and sort filtered rateset with basic bit(s) preserved,
 * and check if resulting rateset is valid.
*/
bool
wlc_rate_hwrs_filter_sort_validate(wlc_rateset_t *rs, const wlc_rateset_t *hw_rs, bool check_brate,
	uint8 txstreams)
{
	uint8 rateset[WLC_MAXRATE+1];
	uint8 r;
	uint count;
	uint i;

	bzero(rateset, sizeof(rateset));
	count = rs->count;

	for (i = 0; i < count; i++) {
		/* mask off "basic rate" bit, WLC_RATE_FLAG */
		r = (int)rs->rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			WL_INFORM(("wlc_rate_hwrs_filter_sort: bad rate in rate set 0x%x\n", r));
			continue;
		}
		rateset[r] = rs->rates[i]; /* preserve basic bit! */
	}

	/* fill out the rates in order, looking at only supported rates */
	count = 0;
	for (i = 0; i < hw_rs->count; i++) {
		r = hw_rs->rates[i] & RATE_MASK;
		ASSERT(r <= WLC_MAXRATE);
		if (rateset[r])
			rs->rates[count++] = rateset[r];
	}

	rs->count = count;

#ifdef WL11N
	/* only set the mcs rate bit if the equivalent hw mcs bit is set */
	for (i = 0; i < MCSSET_LEN; i++)
		rs->mcs[i] = (rs->mcs[i] & hw_rs->mcs[i]);
#endif /* WL11N */

	if (wlc_rateset_valid(rs, check_brate))
		return TRUE;
	else
		return FALSE;
}

/* caluclate the rate of a rx'd frame and return it as a ratespec */
ratespec_t BCMFASTPATH
wlc_recv_compute_rspec(wlc_d11rxhdr_t *wrxh, uint8 *plcp)
{
	d11rxhdr_t *rxh = &wrxh->rxhdr;
	int phy_type;
	ratespec_t rspec = PHY_TXC1_BW_20MHZ << RSPEC_BW_SHIFT;

	phy_type = ((rxh->RxChan & RXS_CHAN_PHYTYPE_MASK) >> RXS_CHAN_PHYTYPE_SHIFT);
#ifdef WL11N

	if ((phy_type == PHY_TYPE_N) || (phy_type == PHY_TYPE_SSN) ||
	    (phy_type == PHY_TYPE_LCN) || (phy_type == PHY_TYPE_HT)) {
		switch (rxh->PhyRxStatus_0 & PRXS0_FT_MASK) {
			case PRXS0_CCK:
				rspec = CCK_PHY2MAC_RATE(((cck_phy_hdr_t *)plcp)->signal);
				break;
			case PRXS0_OFDM:
				rspec = OFDM_PHY2MAC_RATE(((ofdm_phy_hdr_t *)plcp)->rlpt[0]);
				break;
			case PRXS0_PREN:
				rspec = (plcp[0] & MIMO_PLCP_MCS_MASK) | RSPEC_MIMORATE;
				if (plcp[0] & MIMO_PLCP_40MHZ) {
					/* indicate rspec is for 40 MHz mode */
					rspec &= ~RSPEC_BW_MASK;
					rspec |= (PHY_TXC1_BW_40MHZ << RSPEC_BW_SHIFT);
				}
				break;
			case PRXS0_STDN:
				/* fallthru */
			default:
				/* not supported */
				ASSERT(0);
				break;
		}
	if (PLCP3_ISSGI(plcp[3]))
		rspec |= RSPEC_SHORT_GI;
	} else
#endif /* WL11N */
	if ((phy_type == PHY_TYPE_A) || (rxh->PhyRxStatus_0 & PRXS0_OFDM))
		rspec = OFDM_PHY2MAC_RATE(((ofdm_phy_hdr_t *)plcp)->rlpt[0]);
	else
		rspec = CCK_PHY2MAC_RATE(((cck_phy_hdr_t *)plcp)->signal);

	return rspec;
}

/* copy rateset src to dst as-is (no masking or sorting) */
void
wlc_rateset_copy(const wlc_rateset_t *src, wlc_rateset_t *dst)
{
	bcopy(src, dst, sizeof(wlc_rateset_t));
}


/*
 * Copy and selectively filter one rateset to another.
 * 'basic_only' means only copy basic rates.
 * 'rates' indicates cck (11b) and ofdm rates combinations.
 *    - 0: cck and ofdm
 *    - 1: cck only
 *    - 2: ofdm only
 * 'xmask' is the copy mask (typically 0x7f or 0xff).
 */
void
wlc_rateset_filter(wlc_rateset_t *src, wlc_rateset_t *dst, bool basic_only, uint8 rates,
	uint xmask, bool mcsallow)
{
	uint i;
	uint r;
	uint count;

	count = 0;
	for (i = 0; i < src->count; i++) {
		r = src->rates[i];
		if (basic_only && !(r & WLC_RATE_FLAG))
			continue;
		if ((rates == WLC_RATES_CCK) && IS_OFDM((r & RATE_MASK)))
			continue;
		if ((rates == WLC_RATES_OFDM) && IS_CCK((r & RATE_MASK)))
			continue;
		dst->rates[count++] = r & xmask;
	}
	dst->count = count;

	if (rates == WLC_RATES_OFDM && xmask == RATE_MASK_FULL)
		wlc_rateset_ofdm_fixup(dst);

	dst->htphy_membership = src->htphy_membership;

#ifdef WL11N
	if (mcsallow && rates != WLC_RATES_CCK)
		bcopy(&src->mcs[0], &dst->mcs[0], MCSSET_LEN);
	else
		wlc_rateset_mcs_clear(dst);
#endif /* WL11N */
}

/* After filtering 2.4GHz hw rateset to include ofdm rates only
 * we lost basic rates info, so fixup it here.
 */
void
wlc_rateset_ofdm_fixup(wlc_rateset_t *rs)
{
	uint i;

	for (i = 0; i < rs->count; i ++) {
		uint j;
		bool basic = FALSE;

		for (j = 0; j < ofdm_rates.count; j ++) {
			if ((rs->rates[i] & RATE_MASK) == (ofdm_rates.rates[j] & RATE_MASK)) {
				if (ofdm_rates.rates[j] & WLC_RATE_FLAG)
					basic = TRUE;
				break;
			}
		}
		if (basic)
			rs->rates[i] |= WLC_RATE_FLAG;
	}
}

/* select rateset for a given phy_type and bandtype and filter it, sort it
 * and fill rs_tgt with result
 */
void
wlc_rateset_default(wlc_rateset_t *rs_tgt, const wlc_rateset_t *rs_hw, uint phy_type,
	int bandtype, bool cck_only, uint rate_mask, bool mcsallow, uint8 bw, uint8 txstreams)
{
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs_sel;

	if ((PHYTYPE_IS(phy_type, PHY_TYPE_HT)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_N)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_LCN)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_SSN))) {
		if (BAND_5G(bandtype)) {
			rs_dflt = (bw == WLC_20_MHZ ?
				&ofdm_mimo_rates : &ofdm_40bw_mimo_rates);
		} else {
			rs_dflt = (bw == WLC_20_MHZ ?
				&cck_ofdm_mimo_rates : &cck_ofdm_40bw_mimo_rates);
		}
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_LP)) {
		rs_dflt = (BAND_5G(bandtype)) ? &ofdm_rates : &cck_ofdm_rates;
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_A)) {
		rs_dflt = &ofdm_rates;
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_G)) {
		rs_dflt = &cck_ofdm_rates;
	} else {
		ASSERT(0); /* should not happen */
		rs_dflt = &cck_rates; /* force cck */
	}

	/* if hw rateset is not supplied, assign selected rateset to it */
	if (!rs_hw)
		rs_hw = rs_dflt;

	wlc_rateset_copy(rs_hw, &rs_sel);
#ifdef WL11N
	wlc_rateset_mcs_upd(&rs_sel, txstreams);
#endif
	wlc_rateset_filter(&rs_sel, rs_tgt, FALSE,
	                   cck_only ? WLC_RATES_CCK : WLC_RATES_CCK_OFDM, rate_mask, mcsallow);
	wlc_rate_hwrs_filter_sort_validate(rs_tgt, rs_hw, FALSE, mcsallow ? txstreams : 1);
}

int16 BCMFASTPATH
wlc_rate_legacy_phyctl(uint rate)
{
	uint i;

	for (i = 0; i < LEGACY_PHYCFG_TABLE_SIZE; i++)
		if (rate == legacy_phycfg_table[i].rate_ofdm)
			return legacy_phycfg_table[i].tx_phy_ctl3;

	return -1;
}

int
wlc_dump_rateset(const char *name, wlc_rateset_t *rateset, struct bcmstrbuf *b)
{
	uint r, i;
	bool bl;

	bcm_bprintf(b, "%s [ ", name ? name : "");
	for (i = 0; i < rateset->count; i++) {
		r = rateset->rates[i] & RATE_MASK;
		bl = rateset->rates[i] & WLC_RATE_FLAG;
		bcm_bprintf(b, "%d%s%s ", (r / 2), (r % 2) ? ".5" : "", bl ? "(b)" : "");
	}
	bcm_bprintf(b, "]");

	return 0;
}

int
wlc_dump_mcsset(const char *name, uchar *mcs, struct bcmstrbuf *b)
{
	int i;

	bcm_bprintf(b, "%s ", name ? name : "");
	for (i = 0; i < MCSSET_LEN; i++)
		bcm_bprintf(b, "%02x ", *mcs++);

	return 0;
}

ratespec_t
wlc_get_highest_rate(wlc_rateset_t *rateset, bool isbw40, bool sgi, uint8 txstreams)
{
	ratespec_t reprate;
#ifdef WL11N
	ratespec_t mcs_reprate;
	int8 i, bit;
	uint8 mcs_indx;
	uint max_rate;
#endif /* WL11N */

	reprate = rateset->rates[rateset->count - 1] & RATE_MASK;

#ifdef WL11N
	max_rate = reprate * 500;

	/* find the maximum rate based on mcs bit vector */
	for (i = 0; i < MCSSET_LEN; i++) {
		if (rateset->mcs[i] == 0)
			continue;
		if ((txstreams == 1) && (i > 0))
			break;
		for (bit = 0; bit < NBBY; bit++) {
			mcs_indx = (NBBY * i) + bit;
			if (mcs_indx > WLC_MAXMCS)
				break;
			mcs_reprate = mcs_indx | RSPEC_MIMORATE;

			/* mcs > 7 must be stf SDM
			 * mcs <=7 use CDD or SISO, default is CDD
			 */
			if (IS_SINGLE_STREAM(mcs_indx))
				mcs_reprate |=
				(((txstreams == 1) ? PHY_TXC1_MODE_SISO : PHY_TXC1_MODE_CDD)
					<< RSPEC_STF_SHIFT);
			else
				mcs_reprate |= (PHY_TXC1_MODE_SDM << RSPEC_STF_SHIFT);

			if (isbw40)
				mcs_reprate |= (PHY_TXC1_BW_40MHZ << RSPEC_BW_SHIFT);
			else
				mcs_reprate |= (PHY_TXC1_BW_20MHZ << RSPEC_BW_SHIFT);

			if (sgi)
				mcs_reprate |= RSPEC_SHORT_GI;

			if (RSPEC2RATE(mcs_reprate) > max_rate) {
				max_rate = RSPEC2RATE(mcs_reprate);
				reprate = mcs_reprate;
			}
		}
	}
#endif /* WL11N */
	return reprate;
}

#ifdef WL11N
void
wlc_rateset_mcs_clear(wlc_rateset_t *rateset)
{
	uint i;

	for (i = 0; i < MCSSET_LEN; i++)
		rateset->mcs[i] = 0;
}

void
wlc_rateset_mcs_build(wlc_rateset_t *rateset, uint8 nstreams)
{
	bcopy(&cck_ofdm_mimo_rates.mcs[0], &rateset->mcs[0], MCSSET_LEN);
	wlc_rateset_mcs_upd(rateset, nstreams);
}

/* Based on bandwidth passed, allow/disallow MCS 32 in the rateset */
void
wlc_rateset_bw_mcs_filter(wlc_rateset_t *rateset, uint8 bw)
{
	if (bw == WLC_40_MHZ)
		setbit(rateset->mcs, 32);
	else
		clrbit(rateset->mcs, 32);
}
#endif /* WL11N */
