/*
 * Common [OS-independent] rate management
 * 802.11 Networking Adapter Device Driver.
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_rate.c 774598 2019-04-29 17:29:56Z $
 */

/**
 * @file
 * @brief
 * In 802.11 WLANs, depending on the type of the network (a/b/g/n), each device has a (link) rate
 * set from which it chooses a rate to transmit according to certain criteria. For example, 802.11b
 * supports the rate set {1, 2, 5.5, 11} Mbps; 802.11n supports a much larger rate set ranging from
 * 6.50 to 300 Mbps (mcs 0, to mcs 15 in 40 MHz with short guard interval, or SGI). Therefore it has
 * become a rather important decision to choose a best rate.
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <802.11.h>
#include <hndd11.h>
#include <wlc_rate.h>
#include <wl_dbg.h>
#include <wlc_pub.h>
#include <bcmwifi_rates.h>
#include <wlc.h>
#ifdef WL11AX
#include <wlc_he.h>
#endif /* WL11AX */

/* phycfg for legacy OFDM frames: code rate, modulation scheme, spatial streams
 *   Number of spatial streams: always 1
 *   other fields: refer to table 78 of section 17.3.2.2 of the original .11a standard
 */
typedef struct legacy_phycfg {
	uint32	rate_ofdm;	/**< ofdm mac rate */
	uint8	tx_phy_ctl3;	/**< phy ctl byte 3, code rate, modulation type, # of streams */
} legacy_phycfg_t;

#define LEGACY_PHYCFG_TABLE_SIZE	12 /**< Number of legacy_rate_cfg entries in the table */

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

static bool wlc_rateset_valid(wlc_rateset_t *rs, bool check_brate);

/** Given a rate spec, returns mcs without spatial stream info */
uint8
wlc_ratespec_mcs(ratespec_t rspec)
{
	uint8 mcs;
	if (RSPEC_ISHE(rspec)) {
		mcs = (uint8)(rspec & WL_RSPEC_HE_MCS_MASK);
	} else if (RSPEC_ISVHT(rspec)) {
		mcs = (uint8)(rspec & WL_RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
#if defined(WLPROPRIETARY_11N_RATES)
		mcs = wf_get_single_stream_mcs(rspec & WLC_RSPEC_HT_PROP_MCS_MASK);
#else
		if ((rspec & WL_RSPEC_HT_MCS_MASK) == 32)
			mcs = 32;
		else
			mcs = (uint8)(rspec & WLC_RSPEC_HT_MCS_MASK);
#endif /* WLPROPRIETARY_11N_RATES */
	} else {
		mcs = MCS_INVALID;
	}

	return mcs;
}

/** Returns number of spatial streams, Nss, specified by the ratespec */
uint
wlc_ratespec_nss(ratespec_t rspec)
{
	int Nss;

	if (RSPEC_ISHE(rspec)) {
		/* HE ratespec specifies Nss directly */
		Nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;

	} else if (RSPEC_ISVHT(rspec)) {
		/* VHT ratespec specifies Nss directly */
		Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

	} else if (RSPEC_ISHT(rspec)) {
		uint mcs = rspec & WL_RSPEC_HT_MCS_MASK;

		ASSERT(mcs <= 32 || IS_PROPRIETARY_11N_MCS(mcs));
#if defined(WLPROPRIETARY_11N_RATES)
		Nss = GET_11N_MCS_NSS(mcs); /* HT MCS encodes Nss in MCS index */
#else /* this ifdef prevents ROM abandons */
		if (mcs == 32) {
			Nss = 1;
		} else {
			Nss = 1 + (mcs / 8);
		}
#endif /* WLPROPRIETARY_11N_RATES */
	} else {
		/* Legacy rates are all Nss = 1, just add the expansion */
		Nss = 1;
	}

	ASSERT(Nss != 0);
	return Nss;
}

/** Number of space time streams, Nss + Nstbc expansion, specified by the ratespec */
uint
wlc_ratespec_nsts(ratespec_t rspec)
{
	int Nss;
	int Nsts;

	if (RSPEC_ISHE(rspec)) {
		/* HE ratespec specify Nss directly */
		Nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;

		/* HE STBC only allowed with 1 ss and 2 sts (28.3.11.10) */
		if (RSPEC_ISSTBC(rspec)) {
			Nsts = 2;
		} else {
			Nsts = Nss;
		}
	} else if (RSPEC_ISVHT(rspec)) {
		/* VHT ratespec specify Nss directly */
		Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		/* VHT STBC expansion always is Nsts = Nss*2 */
		if (RSPEC_ISSTBC(rspec)) {
			Nsts = 2*Nss;
		} else {
			Nsts = Nss;
		}
	} else if (RSPEC_ISHT(rspec)) {
		uint mcs = rspec & WL_RSPEC_HT_MCS_MASK;

		ASSERT(mcs <= 32 || IS_PROPRIETARY_11N_MCS(mcs));

		if (mcs == 32) {
			/* 11n does not allow STBC for MCS 32 */
			ASSERT(RSPEC_ISSTBC(rspec) == FALSE);
			Nsts = 1;
		} else {
#if defined(WLPROPRIETARY_11N_RATES)
			Nss = GET_11N_MCS_NSS(mcs);
#else /* this ifdef prevents ROM abandons */
			Nss = 1 + (mcs / 8); /* HT MCS encodes Nss in MCS index */
#endif /* WLPROPRIETARY_11N_RATES */
			if (RSPEC_ISSTBC(rspec)) {
				ASSERT(Nss == 1);
				Nsts = Nss + 1;
			} else {
				Nsts = Nss;
			}
		}
	} else {
		/* Legacy rates are all Nss = 1, and no STBC expansion */
		Nsts = 1;
	}

	ASSERT(Nsts != 0);
	return Nsts;
}

/** Number of Tx chains, NTx, specified by the ratespec */
uint
wlc_ratespec_ntx(ratespec_t rspec)
{
	int Nsts;
	int NTx;

	/* calculate the underlying Space-Time-Streams */
	Nsts = wlc_ratespec_nsts(rspec);

	/* Tx chain expansion is encoded directly in ratespec */
	NTx = Nsts + RSPEC_TXEXP(rspec);

	return NTx;
}

/** bw in Mhz, specified by the ratespec */
uint8
wlc_ratespec_bw(ratespec_t rspec)
{
	const uint8 rspec2bw[] = {0, 20, 40, 80, 160, 10, 5, 2};

	return (rspec2bw[RSPEC2BW(rspec)]);
}
/**
 * Purpose is to determine the (legacy) rate of a response frame.
 * Return value: rate in [500Kbps] units
 */
uint
wlc_rate_rspec_reference_rate(ratespec_t rspec)
{
	uint legacy_rate;

	if (RSPEC_ISHE(rspec)) {
		legacy_rate = wlc_rate_he_basic_reference(rspec & WL_RSPEC_HE_MCS_MASK);
	} else if (RSPEC_ISVHT(rspec)) {
		legacy_rate = wlc_rate_vht_basic_reference(rspec & WL_RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		legacy_rate = wlc_rate_ht_basic_reference(rspec & WL_RSPEC_HT_MCS_MASK);
	} else {
		ASSERT(RSPEC_ISLEGACY(rspec));
		legacy_rate = RSPEC2RATE(rspec);
	}

	return legacy_rate;
}

/**
 * 802.11n-2009 "9.6.2 Non-HT basic rate calculation"
 * defines the calculation of a non-HT reference rate from an HT MCS
 * for use in determining the rate of a response frame.
 * The reference rate for MCS 0-7 is repeated for MCS 8-15, 16-23, and 24-31.
 * Only MCS 0-7 is in this table, so do MOD 8 values to look up.
 * Extending the table for 802.11ac VHT. VHT MCS 0-7 match HT MCS 0-7.
 * Extending table for 802.11ax HT. HE MCS 0-7 match VHT & HT MCS 0-7.
 *
 * Note : All MCS higher than 7 have the same minimum basic ref rate of WLC_RATE_54M
 */
static const uint basic_ref_rate[] = {
	/* MCS 0 */ WLC_RATE_6M,
	/* MCS 1 */ WLC_RATE_12M,
	/* MCS 2 */ WLC_RATE_18M,
	/* MCS 3 */ WLC_RATE_24M,
	/* MCS 4 */ WLC_RATE_36M,
	/* MCS 5 */ WLC_RATE_48M,
	/* MCS 6 */ WLC_RATE_54M,
	/* MCS 7 */ WLC_RATE_54M
};

/* All MCS' (HT / VHT / HE) higher than (>=) MCS 7 have the same basic ref rate : WLC_RATE_54M */
#define BASIC_REF_RATE_MCS_THRESHOLD(mcs)	((mcs > 7) ? 7 : mcs)

/**
 * Purpose is to determine the rate of a response frame. Given a 11n MCS, converts into a legacy
 * reference rate in [500KBps] units.
 */
uint
wlc_rate_ht_basic_reference(uint ht_mcs)
{
	uint ref;

	/* Only handle MCS 0-32. None of the MCS>32 are supported by BRCM */
	ASSERT(ht_mcs <= 32 || IS_PROPRIETARY_11N_MCS(ht_mcs));

	/* calculate the index for the basid_ref_rate[] table */

#if defined(WLPROPRIETARY_11N_RATES)
	if (IS_PROPRIETARY_11N_MCS(ht_mcs)) {
		ht_mcs = 9; /* all QAM256 rates are >= 54Mbps */
	} else
#endif /* WLPROPRIETARY_11N_RATES */

	if (ht_mcs >= 32) {
		/* use MCS0 for MCS32 (equivalent modulation and coding),
		 * or any out of range value
		 */
		ht_mcs = 0;
	} else {
		/* Take (MCS mod 8) to get equivalent modulation
		 * and coding in the 0-7 range.
		 */
		ht_mcs = ht_mcs % 8;
	}

	ref = basic_ref_rate[BASIC_REF_RATE_MCS_THRESHOLD(ht_mcs)];

	return ref;
}

/**
 * Purpose is to determine the rate of a response frame. Given an AC MCS, converts into a legacy
 * reference rate in [500KBps] units.
 */
uint
wlc_rate_vht_basic_reference(uint vht_mcs)
{
	uint ref;

	/* Only MCS 0-11 are valid */
	ASSERT(vht_mcs <= WLC_MAX_VHT_MCS);

	/* use MCS0 for any out of range value */
	if (vht_mcs > WLC_MAX_VHT_MCS) {
		vht_mcs = 0;
	}

	ref = basic_ref_rate[BASIC_REF_RATE_MCS_THRESHOLD(vht_mcs)];

	return ref;
}

/**
 * Purpose is to determine the rate of a response frame. Given an HE MCS, converts into a legacy
 * reference rate in [500KBps] units.
 */
uint
wlc_rate_he_basic_reference(uint he_mcs)
{
	uint ref;

	/* Only MCS 0-11 are valid */
	ASSERT(he_mcs <= WLC_MAX_HE_MCS);

	/* use MCS0 for any out of range value */
	if (he_mcs > WLC_MAX_HE_MCS) {
		he_mcs = 0;
	}

	ref = basic_ref_rate[BASIC_REF_RATE_MCS_THRESHOLD(he_mcs)];

	return ref;
}

/**
 * check if rateset is valid.
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
/**
 * Clear HT MCS'es that cannot be used because the underlying hardware does not support them, or
 * because a maximum number of streams was agreed upon with a remote party.
 */
void
wlc_rateset_mcs_upd(wlc_rateset_t *rs, uint8 nstreams)
{
	int i;

	/* support for unsupported/inactive streams. */
	for (i = nstreams; i < MAX_STREAMS_SUPPORTED; i++) {
		rs->mcs[i] = 0;	/* one byte equivalences to 8 MCS'es */
	}
#if defined(WLPROPRIETARY_11N_RATES)
	for (i = WLC_11N_FIRST_PROP_MCS; i <= WLC_MAXMCS; i++) {
		if (GET_PROPRIETARY_11N_MCS_NSS(i) > nstreams) {
			clrbit(rs->mcs, i);
		}
	}
#endif /* WLPROPRIETARY_11N_RATES */
}

#ifdef WL11AC
#ifdef WL11AX
/** Clear out the unsupported rate sets, for non-existent streams. */
static void
wlc_rateset_clear_rateset_nss(uint16 *mcs_nss, uint8 nstreams)
{
	uint nss;
	uint nss_idx;

	for (nss = nstreams + 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		nss_idx = HE_CAP_MAX_MCS_NSS_GET_SS_IDX(nss);
		*mcs_nss &= (~(HE_CAP_MAX_MCS_MASK << nss_idx));
		*mcs_nss |= (HE_CAP_MAX_MCS_NONE << nss_idx);
	}
}

static void
wlc_rateset_hemcs_upd(wlc_rateset_t *rs, uint8 nstreams)
{
	wlc_rateset_clear_rateset_nss(&rs->he_bw80_tx_mcs_nss, nstreams);
	wlc_rateset_clear_rateset_nss(&rs->he_bw160_tx_mcs_nss, nstreams);
	wlc_rateset_clear_rateset_nss(&rs->he_bw80p80_tx_mcs_nss, nstreams);
}
#endif /* WL11AX */

/** Clear out the unsupported rate sets, for non-existent streams. */
void
wlc_rateset_vhtmcs_upd(wlc_rateset_t *rs, uint8 nstreams)
{
	int nss;

	for (nss = nstreams + 1; nss <= VHT_CAP_MCS_MAP_NSS_MAX; nss++) {
		VHT_MCS_MAP_SET_MCS_PER_SS(nss, VHT_CAP_MCS_MAP_NONE, rs->vht_mcsmap);
	}
}

void
wlc_rateset_prop_vhtmcs_upd(wlc_rateset_t *rs, uint8 nstreams)
{
	int nss;

	for (nss = nstreams + 1; nss <= VHT_CAP_MCS_MAP_NSS_MAX; nss++) {
		VHT_MCS_MAP_SET_MCS_PER_SS(nss, VHT_CAP_MCS_MAP_NONE, rs->vht_mcsmap_prop);
	}
}
#endif /* WL11AC */
#endif /* WL11N */

/* Calculate the minimum mcs set for each NSS based on the input and the filter mcs.
 * 'src_mcs_nss_set' is both input and output mcs set for all NSS,
 * 'fltr_mcs_nss_set' is the input filter mcs set for all NSS.
 */
static void
wlc_rateset_hemcs_filter(uint16 *src_mcs_nss_set, uint16 fltr_mcs_nss_set)
{
	uint nss;
	uint nss_idx;
	uint src_mcs_code;
	uint fltr_mcs_code;

	for (nss = 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		src_mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, *src_mcs_nss_set);

		/* use no mcs or the minimum mcs set for the NSS */
		if (src_mcs_code == HE_CAP_MAX_MCS_NONE) {
			continue;
		}
		fltr_mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, fltr_mcs_nss_set);
		if (fltr_mcs_code == HE_CAP_MAX_MCS_NONE || src_mcs_code > fltr_mcs_code) {
			nss_idx = HE_CAP_MAX_MCS_NSS_GET_SS_IDX(nss);
			*src_mcs_nss_set &= (~(HE_CAP_MAX_MCS_MASK << nss_idx));
			*src_mcs_nss_set |= (fltr_mcs_code << nss_idx);
		}
	}
}

/**
 * Filter 'rs' based on hardware rate set 'hw_rs', and sort filtered rate set with basic bit(s)
 * preserved.
 *     'rs' :    [in+out] :
 *     'hw_rs' : [in]     :
 * Returns TRUE if resulting rate set is valid.
 */
bool
wlc_rate_hwrs_filter_sort_validate(wlc_rateset_t *rs, const wlc_rateset_t *hw_rs, bool check_brate,
	uint8 txstreams)
{
	uint8 rateset[WLC_MAXRATE+1]; /* WLC_MAXRATE corresponds with 54Mbps in [500Kbps] units */
	uint8 r;
	uint count;
	uint i;

	BCM_REFERENCE(txstreams);

	bzero(rateset, sizeof(rateset));
	count = rs->count;

	for (i = 0; i < count; i++) {
		/* mask off "basic rate" bit, WLC_RATE_FLAG */
		r = (int)rs->rates[i] & RATE_MASK;
		if (r > WLC_MAXRATE) {
			continue;
		}
		if (rate_info[r] == 0) {
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

#ifdef WL11AC
	/*
	 * Force back the hw rate set if the caller asked for more than what we can do.
	 */
	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		uint8 mcs, hwrs_mcs;
		mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, rs->vht_mcsmap);
		hwrs_mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, hw_rs->vht_mcsmap);
		if ((mcs != VHT_CAP_MCS_MAP_NONE) && (mcs > hwrs_mcs)) {
			VHT_MCS_MAP_SET_MCS_PER_SS(i, hwrs_mcs, rs->vht_mcsmap);
		}
	}

	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		uint8 mcs, hwrs_mcs;
		mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, rs->vht_mcsmap_prop);
		hwrs_mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, hw_rs->vht_mcsmap_prop);
		if ((mcs != VHT_CAP_MCS_MAP_NONE) && (mcs > hwrs_mcs)) {
			VHT_MCS_MAP_SET_MCS_PER_SS(i, hwrs_mcs, rs->vht_mcsmap_prop);
		}
	}
#endif /* WL11AC */

	wlc_rateset_hemcs_filter(&rs->he_bw80_tx_mcs_nss, hw_rs->he_bw80_tx_mcs_nss);
	wlc_rateset_hemcs_filter(&rs->he_bw80_rx_mcs_nss, hw_rs->he_bw80_rx_mcs_nss);
	wlc_rateset_hemcs_filter(&rs->he_bw160_tx_mcs_nss, hw_rs->he_bw160_tx_mcs_nss);
	wlc_rateset_hemcs_filter(&rs->he_bw160_rx_mcs_nss, hw_rs->he_bw160_rx_mcs_nss);
	wlc_rateset_hemcs_filter(&rs->he_bw80p80_tx_mcs_nss, hw_rs->he_bw80p80_tx_mcs_nss);
	wlc_rateset_hemcs_filter(&rs->he_bw80p80_rx_mcs_nss, hw_rs->he_bw80p80_rx_mcs_nss);

	return wlc_rateset_valid(rs, check_brate);
}

/** Calculate the rate of a received frame and return it as a ratespec */
ratespec_t BCMFASTPATH
wlc_recv_compute_rspec(uint corerev, d11rxhdr_t *rxh, uint8 *plcp)
{
	ratespec_t rspec;
	uint16 phy_ftfmt; /* frame type and format */

	phy_ftfmt = D11PPDU_FTFMT(rxh, corerev);

	/* HT/VHT/HE-SIG-A start from plcp[4] in rev128 */
	plcp += D11_PHY_RXPLCP_OFF(corerev);
#ifdef WL11AX
	/* HETB-SIG-A doesn't include rate info pass it using SIG-B */
	if (phy_ftfmt == HE_FTFMT_HETB) {
		uint16 phy_ulrtinfo = HETB_RSPEC(rxh, corerev);
		plcp[6] = phy_ulrtinfo & 0xff;
		plcp[7] = (phy_ulrtinfo & 0xff00) >> 8;
	}
#endif // endif

	if (wf_plcp_to_rspec(phy_ftfmt, plcp, &rspec) == FALSE) {
		WL_ERROR(("%s: invalid frametype: 0x%x\n", __FUNCTION__, phy_ftfmt));
		ASSERT(0);

		/* Return a valid rspec if not a debug/assert build */
		rspec = OFDM_RSPEC(WLC_RATE_6M);
	}

	return rspec;
} /* wlc_recv_compute_rspec */

/** copy rateset src to dst as-is (no masking or sorting) */
void
wlc_rateset_copy(const wlc_rateset_t *src, wlc_rateset_t *dst)
{
	bcopy(src, dst, sizeof(*dst));
}

/**
 * Copy and selectively filter one rate set to another.
 *     'src'
 *     'dst'. 'src' and 'dst' are allowed to point at the same rate set.
 *     'basic_only' means only copy basic rates.
 *     'rates' indicates cck (11b) and ofdm rates combinations.
 *        - WLC_RATES_CCK_OFDM: cck and ofdm
 *        - WLC_RATES_CCK:      cck only
 *        - WLC_RATES_OFDM:     ofdm only
 *     'xmask' is the copy mask (typically 0x7f or 0xff).
 *     'mcsallow': bitmask carrying bits such as WLC_MCS_ALLOW and WLC_MCS_ALLOW_VHT.
 */
void
wlc_rateset_filter(wlc_rateset_t *src, wlc_rateset_t *dst, bool basic_only, uint8 rates,
	uint xmask, uint8 mcsallow)
{
	uint i;
	uint r;
	uint count;

	count = 0;
	for (i = 0; i < src->count; i++) {
		r = src->rates[i];
		if (basic_only && !(r & WLC_RATE_FLAG))
			continue;
		if ((rates == WLC_RATES_CCK) && RATE_ISOFDM(r))
			continue;
		if ((rates == WLC_RATES_OFDM) && RATE_ISCCK(r))
			continue;
		dst->rates[count++] = r & xmask;
	}
	dst->count = count;

	if (rates == WLC_RATES_OFDM && xmask == RATE_MASK_FULL)
		wlc_rateset_ofdm_fixup(dst); /* fix up basic rates */

#ifdef WL11N
	/**
	 * 1. Both WLC_MCS_ALLOW and WLC_MCS_ALLOW_VHT flags should be set
	 * for copying vht_mcsmap as this copy should not be
	 * done for non VHT devices and both HT and VHT rates
	 * need to be filled for 11ac devices.
	 *
	 * 2. WLC_MCS_ALLOW, WLC_MCS_ALLOW_VHT & WLC_MCS_ALLOW_HE should
	 * be set for copying he_mcs_nss, as this copy should not be done for non
	 * HE devices. HT, VHT & HE rates need to be filled for 11ax devices.
	 */
	if ((mcsallow & WLC_MCS_ALLOW) && rates != WLC_RATES_CCK) {
		if (src != dst)
			memcpy(&dst->mcs[0], &src->mcs[0], MCSSET_LEN);
		if (!(mcsallow & WLC_MCS_ALLOW_PROP_HT))
			wlc_rate_clear_prop_11n_mcses(dst->mcs);
#ifdef WL11AC
		if (mcsallow & WLC_MCS_ALLOW_VHT) {
			dst->vht_mcsmap = src->vht_mcsmap;
		} else {
			/* needed as VHT_CAP_MCS_MAP_NONE_ALL is not 0 */
			dst->vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
		}
		if (mcsallow & WLC_MCS_ALLOW_1024QAM) {
			dst->vht_mcsmap_prop = src->vht_mcsmap_prop;
		} else {
			/* needed as VHT_CAP_MCS_MAP_NONE_ALL is not 0 */
			dst->vht_mcsmap_prop = VHT_CAP_MCS_MAP_NONE_ALL;
		}

#ifdef WL11AX
		/* handling Supported HE MCS & NSS set for the current BW */
		if (mcsallow & WLC_MCS_ALLOW_HE) {
			wlc_rateset_he_cp(dst, src);
		} else {
			wlc_rateset_he_none_all(dst);
		}
#endif /* WL11AX */
#endif /* WL11AC */
	} else {
		wlc_rateset_mcs_clear(dst);
		dst->vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
		dst->vht_mcsmap_prop = VHT_CAP_MCS_MAP_NONE_ALL;
		wlc_rateset_he_none_all(dst);
	}
#endif /* WL11N */
} /* wlc_rateset_filter */

/**
 * After filtering 2.4GHz hw rateset to include ofdm rates only
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

/**
 * Select rate set for a given phy_type and bandtype and filter it, sort it and fill rs_tgt with
 * result.
 *
 * @param rate_mask   Bitmask filtering out certain rates, eg RATE_MASK_FULL
 * @param mcsallow    Bitmask carrying bits such as WLC_MCS_ALLOW and WLC_MCS_ALLOW_VHT
 */
void
wlc_rateset_default(wlc_info_t *wlc, wlc_rateset_t *rs_tgt, const wlc_rateset_t *rs_hw,
	uint phy_type, int bandtype, bool cck_only, uint rate_mask,
	uint8 mcsallow, uint8 bw, uint8 txstreams)
{
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs_sel;

	if ((PHYTYPE_IS(phy_type, PHY_TYPE_N)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_LCN20)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_AC))) {
		if (BAND_5G(bandtype)) {
			rs_dflt = (bw == WLC_20_MHZ ?
				&ofdm_mimo_rates : &ofdm_40bw_mimo_rates);
		} else {
			rs_dflt = (bw == WLC_20_MHZ ?
				&cck_ofdm_mimo_rates : &cck_ofdm_40bw_mimo_rates);
		}
	}
	/* TODO: remove when possible.
	 * can't remove PHY_TYPE_A and PHY_TYPE_G now since NDIS
	 * may issue iovar which in turn calls this function with
	 * PHY_TYPE_A and PHY_TYPE_G.
	 */
#ifdef PHY_TYPE_A
	else if (phy_type == PHY_TYPE_A) {
		rs_dflt = &ofdm_rates;
	}
#endif // endif
#ifdef PHY_TYPE_G
	else if (phy_type == PHY_TYPE_G) {
		rs_dflt = &cck_ofdm_rates;
	}
#endif // endif
	else {
		WL_ERROR(("Unknown PHY type %d\n", phy_type));
		ASSERT(0); /* should not happen */
		rs_dflt = &cck_rates; /* force cck */
	}

	/* if hw rateset is not supplied, assign selected rateset to it */
	if (!rs_hw)
		rs_hw = rs_dflt;

	wlc_rateset_copy(rs_hw, &rs_sel);

#ifdef WL11N
	wlc_rateset_bw_mcs_filter(&rs_sel, bw);
	wlc_rateset_mcs_upd(&rs_sel, txstreams);
#endif /* WL11N */

#ifdef WL11AC
	wlc_rateset_vhtmcs_upd(&rs_sel, txstreams);
	wlc_rateset_prop_vhtmcs_upd(&rs_sel, txstreams);
#endif /* WL11AC */
#ifdef WL11AX
	if (wlc->hei) {
		wlc_he_default_rateset(wlc->hei, &rs_sel);
	}
	wlc_rateset_hemcs_upd(&rs_sel, txstreams);
#endif /* WL11AX */
	wlc_rateset_filter(&rs_sel, rs_tgt, FALSE,
	                   cck_only ? WLC_RATES_CCK : WLC_RATES_CCK_OFDM,
	                   rate_mask, mcsallow);
	wlc_rate_hwrs_filter_sort_validate(rs_tgt, rs_hw, FALSE,
	                                   mcsallow ? txstreams : 1);
} /* wlc_rateset_default */

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

void
wlc_dump_rspec(void *context, ratespec_t rspec, struct bcmstrbuf *b)
{

	BCM_REFERENCE(context);

#if defined(BCMDBG)
	bcm_bprintf(b, "\n\trspec 0x%x: ", rspec);
	/* vht, ht, legacy */
	if (RSPEC_ISHT(rspec)) {
		bcm_bprintf(b, " ht ");
	} else if (RSPEC_ISLEGACY(rspec)) {
		/* ofdm, cck */
		if (RSPEC_ISOFDM(rspec)) {
			bcm_bprintf(b, " ofdm ");
		} else if (RSPEC_ISCCK(rspec)) {
			bcm_bprintf(b, " cck ");
		}
	} else if (RSPEC_ISVHT(rspec)) {
		bcm_bprintf(b, " vht ");
	} else if (RSPEC_ISHE(rspec)) {
		bcm_bprintf(b, " he ");
	}

	/* ldpc, stbc, sgi */
	if (rspec & WL_RSPEC_LDPC) {
		bcm_bprintf(b, " ldpc ");
	}
	if (rspec & WL_RSPEC_STBC) {
		bcm_bprintf(b, " stbc ");
	}
	if (rspec & WL_RSPEC_SGI) {
		bcm_bprintf(b, " sgi ");
	}
	/* rate */
	bcm_bprintf(b, "\n\tRate = %dkbps", wf_rspec_to_rate(rspec));

	if (RSPEC_ISHE(rspec)) {
		bcm_bprintf(b, " mcs = %0x ", (rspec & WL_RSPEC_HE_MCS_MASK));
		bcm_bprintf(b, " nss = %d ",
		            (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT);
	} else if (RSPEC_ISVHT(rspec)) {
		bcm_bprintf(b, " mcs = %0x ", (rspec & WL_RSPEC_VHT_MCS_MASK));
		bcm_bprintf(b, " nss = %d ",
		            (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT);
	} else if (RSPEC_ISHT(rspec)) {
#if defined(WLPROPRIETARY_11N_RATES)
		bcm_bprintf(b, " mcs = %0x ",
			wf_get_single_stream_mcs(rspec & WLC_RSPEC_HT_PROP_MCS_MASK));
#else
		bcm_bprintf(b, " mcs = %0x ", (rspec & WLC_RSPEC_HT_MCS_MASK));
#endif // endif
	}

	/* bandwidth */
	bcm_bprintf(b, " Bandwidth = ");
	switch (RSPEC_BW(rspec)) {
		case WL_RSPEC_BW_UNSPECIFIED:
			bcm_bprintf(b, "unspecified");
			break;
		case WL_RSPEC_BW_20MHZ:
			bcm_bprintf(b, "20MHz");
			break;
		case WL_RSPEC_BW_40MHZ:
			bcm_bprintf(b, "40MHz");
			break;
		case WL_RSPEC_BW_80MHZ:
			bcm_bprintf(b, "80MHz");
			break;
		case WL_RSPEC_BW_160MHZ:
			bcm_bprintf(b, "160MHz");
			break;
		default:
			bcm_bprintf(b, "invalid");
			break;
	}
	bcm_bprintf(b, "\n");
#endif // endif
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

#ifdef WL11AC
/**
 * Dump the VHT mode mcsmap. Pass the map in wlc_rateset_t format, using rateset_vht_map_t items.
 */
void
wlc_dump_vht_mcsmap(const char *name, uint16 vht_mcsmap, struct bcmstrbuf *b)
{
	int nss;

	bcm_bprintf(b, "%s ", name ? name : "");
	for (nss = 1; nss <= VHT_CAP_MCS_MAP_NSS_MAX; nss++) {
		bcm_bprintf(b, "%04x ",
		    VHT_MCS_CODE_TO_MCS_MAP(VHT_MCS_MAP_GET_MCS_PER_SS(nss, vht_mcsmap)));
	}
	bcm_bprintf(b, "(%x)", vht_mcsmap);
}

void
wlc_dump_vht_mcsmap_prop(const char *name, uint16 vht_mcsmap, struct bcmstrbuf *b)
{
	int nss;

	bcm_bprintf(b, "%s ", name ? name : "");
	for (nss = 1; nss <= VHT_CAP_MCS_MAP_NSS_MAX; nss++) {
		bcm_bprintf(b, "%04x ",
		    VHT_PROP_MCS_CODE_TO_PROP_MCS_MAP(VHT_MCS_MAP_GET_MCS_PER_SS(nss, vht_mcsmap)));
	}
	bcm_bprintf(b, "(%x)", vht_mcsmap);
}

/** IEEE 802.11ac D4.1 allowed MCS rates for each operating bandwidth */
const uint16 vht_allowed_mcsmap[BW_MAXMHZ][VHT_CAP_MCS_MAP_NSS_MAX] = {
		/* 20 MHz MCS0-8 except Nss3, Nss6 MCS0-9 */
		{ 0x01ff, 0x01ff, 0x03ff, 0x01ff, 0x01ff, 0x3ff, 0x01ff, 0x01ff },
		/* 40 MHz MCS 0-9 */
		{ 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x03ff, 0x3ff, 0x03ff, 0x03ff },
		/* 80MHz  MCS0-9 except Nss3,Nss7 MCS6 disallowed, Nss6 MCS0-8 */
		{ 0x03ff, 0x03ff, 0x03bf, 0x03ff, 0x03ff, 0x1ff, 0x03bf, 0x03ff },
		/* 160MHz MCS0-9 except Nss3 MCS0-8 */
		{ 0x03ff, 0x03ff, 0x01ff, 0x03ff, 0x03ff, 0x3ff, 0x03ff, 0x03ff },
	};

const uint16 vht_allowed_mcsmap_prop[BW_MAXMHZ][VHT_CAP_MCS_MAP_NSS_MAX] = {
		/* 20 MHz MCS 10-11 */
		{ 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00 },
		/* 40 MHz MCS 10-11 */
		{ 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00 },
		/* 80MHz  MCS 10-11 */
		{ 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00 },
		/* 160MHz MCS 10-11 */
		{ 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00, 0x0c00 },
	};

/**
 * This function returns a valid mcsmap (in wlc_rateset_t form) for the given
 *  mcs_code, bw, and nss.
 * Inputs:
 *     mcscode : mcs range code (none, 0-7, 0-8, 0-9)
 *     bw: bandwidth
 *     ldpc: if set indicates LDPC in use between us and the peer
 *     nss: number of streams
 *     ratemask: Band specific BRCM proprietary rates disallowed by IEE802.11ac D4.1
 */
uint16
wlc_get_valid_vht_mcsmap(uint8 mcscode, uint8 prop_mcscode, uint8 bw, bool ldpc, uint8 nss,
	uint8 ratemask)
{
	uint16 peermcsmap, prop_peermcsmap;
	uint16 mcsmap;
	bool proprates = FALSE;

	ASSERT((bw >= BW_MINMHZ) && (bw <= BW_MAXMHZ));
	ASSERT((nss > 0) && (nss <= VHT_CAP_MCS_MAP_NSS_MAX));

	nss = LIMIT_TO_RANGE(nss, 1, VHT_CAP_MCS_MAP_NSS_MAX);
	peermcsmap = VHT_MCS_CODE_TO_MCS_MAP((mcscode & VHT_CAP_MCS_MAP_M));
	prop_peermcsmap = VHT_PROP_MCS_CODE_TO_PROP_MCS_MAP((prop_mcscode & VHT_CAP_MCS_MAP_M));

	/* Invalid mcs mask */
	if (!peermcsmap) {
		return 0;
	}

	/* Note:
	 * ratemask must be correct for the band as the band info
	 * is not available in the rate module
	 */
	if (ldpc) {
		if (bw == BW_20MHZ) {
			proprates  = (ratemask & (WL_VHT_FEATURES_2G_20M|WL_VHT_FEATURES_5G_20M));
		} else if (bw == BW_80MHZ) {
			proprates = (ratemask & (WL_VHT_FEATURES_5G_80M));
		} else if (bw == BW_160MHZ) {
			proprates = (ratemask & (WL_VHT_FEATURES_5G_160M));
		}

	}

	mcsmap = (proprates ? 0x3ff : vht_allowed_mcsmap[bw - 1][nss - 1]) & peermcsmap;

	if (ldpc) {
		if (ratemask & WL_VHT_FEATURES_1024QAM) {
			mcsmap |= (vht_allowed_mcsmap_prop[bw - 1][nss - 1] & prop_peermcsmap);
		}
	}

	WL_RATE(("%s() map=0x%x code=0x%x code2= 0x%x peermap=0x%x peermap_prop=0x%x rm=0x%x bw=%d "
		"nss=%d \n", __FUNCTION__, mcsmap, mcscode, prop_mcscode, peermcsmap,
		prop_peermcsmap, ratemask, bw, nss));
	return (mcsmap);
}

/**
 * This function returns true/false based on whether given
 *  mcs/nss/bw combination is valid or not.
 * Inputs:
 *     mcs: MCS index
 *     bw: bandwidth
 *     nss: number of streams
 *     ratemask: Band specific BRCM proprietary rates disallowed by IEE802.11ac D4.1
 *     ldpc: if set indicates LDPC in use between us and the peer
 *     mcscode : mcs range code (none, 0-7, 0-8, 0-9)
 */
static bool
wlc_valid_vht_mcs(uint8 mcs, uint8 nss, uint8 bw, uint8 ratemask, bool ldpc, uint8 mcscode,
	uint8 prop_mcscode)
{
	if (((bw >= BW_MINMHZ) && (bw <= BW_MAXMHZ)) &&
		(nss >= 1) && (nss <= VHT_CAP_MCS_MAP_NSS_MAX) && (mcs <= WLC_MAX_VHT_MCS)) {
		uint16 mcsmap =
			wlc_get_valid_vht_mcsmap(mcscode, prop_mcscode, bw, ldpc, nss, ratemask);
		if (mcsmap & (1 << mcs))
			return TRUE;
	}

	return FALSE;
}

/**
 * This function returns the highest supported MCS index.
 * Inputs:
 *     mcs_code: code indicating which MCS set is used
 *     bw: bandwidth
 *     nss: number of streams
 *     ratemask: BRCM proprietary rates disalowed by IEE802.11ac D4.1
 *     ldpc: if set indicates LDPC in use between us and the peer
 * Returns: Highest and valid MCS index
 */
static uint8
wlc_get_highest_vht_mcs_index(uint8 mcs_code, uint8 prop_mcs_code, uint8 bw,
	uint8 nss, bool ldpc, uint8 ratemask)
{
	uint8 mcs_indx = 0;

	/* Proprietary rates only of use if ldpc is in use */
	ratemask = ldpc ? ratemask : 0;
	if (mcs_code == VHT_CAP_MCS_MAP_0_7) {
		mcs_indx = 7;
	} else if (mcs_code == VHT_CAP_MCS_MAP_0_8) {
		mcs_indx = 8;
	} else if (mcs_code == VHT_CAP_MCS_MAP_0_9) {
		mcs_indx = 9;
	}

	if (prop_mcs_code == VHT_PROP_MCS_MAP_10_11) {
		mcs_indx = 11;
	}

	/* Find the highest valid index by checking from highest index.
	 * OK to check the params few times in the calls to
	 * wlc_valid_vht_mcs() as this is control path and allows
	 * code reuse.
	 */
	for (; mcs_indx > 0; mcs_indx--) {
		if (wlc_valid_vht_mcs(mcs_indx, nss, bw, ratemask, ldpc, mcs_code, prop_mcs_code))
			break;
	}

	return mcs_indx;
}

#endif /* WL11AC */

#ifdef WL11AX
/**
 * Dump the HE mcsmap. Pass the map in wlc_rateset_t format, using he_mcs_map_t items.
 */
static void
wlc_dump_he_mcsmap(const char *name, const char* mapname, uint16 he_mcsmap, struct bcmstrbuf *b)
{
	int nss;

	bcm_bprintf(b, "%s ", name ? name : "");
	bcm_bprintf(b, "%s ", mapname ? mapname : "");
	for (nss = 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		bcm_bprintf(b, "%04x ",
			HE_MAX_MCS_TO_MCS_MAP(HE_CAP_MAX_MCS_NSS_GET_MCS(nss, he_mcsmap)));
	}
	bcm_bprintf(b, "(%x)", he_mcsmap);
}

void
wlc_dump_he_rateset(const char *name, wlc_rateset_t *rateset, struct bcmstrbuf *b)
{
	wlc_dump_he_mcsmap(name, "bw80 tx   ", rateset->he_bw80_tx_mcs_nss, b);
	wlc_dump_he_mcsmap(name, "bw80 rx   ", rateset->he_bw80_rx_mcs_nss, b);
	wlc_dump_he_mcsmap(name, "bw160 tx  ", rateset->he_bw160_tx_mcs_nss, b);
	wlc_dump_he_mcsmap(name, "bw160 rx  ", rateset->he_bw160_rx_mcs_nss, b);
	wlc_dump_he_mcsmap(name, "bw80p80 tx", rateset->he_bw80p80_tx_mcs_nss, b);
	wlc_dump_he_mcsmap(name, "bw80p80 rx", rateset->he_bw80p80_rx_mcs_nss, b);
}
#endif /* WL11AX */

/**
 * This function returns the highest rate based on the info from rateset, bw, sgi, and max number of
 * txstreams.
 *
 * Inputs:
 *     rateset:   pointer to rateset info. This info includes the HT and VHT capabilities.
 *     bw:        bandwidth
 *     sgi:       is short guard interval used
 *     txstreams: max number of txstreams supported
 *     ratemask:  Band specific BRCM proprietary rates disallowed by IEE802.11ac D4.1 (VHT)
 *     ldpc:      if set indicates LDPC in use between us and the peer
 * Returns: ratespec_t for the highest rate determined.
 */
ratespec_t
wlc_get_highest_rate(wlc_rateset_t *rateset, uint8 bw, bool sgi,
	bool ldpc, uint8 ratemask, uint8 txstreams)
{
	ratespec_t reprate;
	uint rate;
#ifdef WL11N
	ratespec_t mcs_reprate;
	int8 i, bit;
	uint8 mcs_indx;
	uint max_rate;
#endif /* WL11N */

	WL_TRACE(("%s inputs: vht_mcsmap = 0x%x; vht_mcsmap_prop = 0x%x; he80: 0x%x; he160: 0x%x;"
		" bw = %d; sgi = %d; txstreams = %d \n",
		__FUNCTION__, rateset->vht_mcsmap, rateset->vht_mcsmap_prop,
		rateset->he_bw80_tx_mcs_nss, rateset->he_bw160_tx_mcs_nss, bw, sgi, txstreams));

	rate = rateset->rates[rateset->count - 1] & RATE_MASK;
	reprate = LEGACY_RSPEC(rate);

#ifdef WL11N
	max_rate = rate * 500;

	/* find the maximum rate based on mcs bit vector */
	for (i = 0; i < MCSSET_LEN; i++) {
		if (rateset->mcs[i] == 0)
			continue;
#if !defined(WLPROPRIETARY_11N_RATES)
		if ((txstreams == 1) && (i > 0))
			break;
		if ((txstreams == 2) && (i > 1))
			break;
#endif /* WLPROPRIETARY_11N_RATES */
		for (bit = 0; bit < NBBY; bit++) {
			mcs_indx = (NBBY * i) + bit;
			if (mcs_indx > WLC_MAXMCS)
				break;

#if defined(WLPROPRIETARY_11N_RATES)
			if (isclr(rateset->mcs, mcs_indx) || GET_11N_MCS_NSS(mcs_indx) > txstreams)
				continue;
#endif /* WLPROPRIETARY_11N_RATES */

			mcs_reprate = mcs_indx | WL_RSPEC_ENCODE_HT;

			mcs_reprate |= (bw << WL_RSPEC_BW_SHIFT);

			if (sgi)
				mcs_reprate |= WL_RSPEC_SGI;

			rate = wf_rspec_to_rate(mcs_reprate);

			if (rate > max_rate) {
				max_rate = rate;
				reprate = mcs_reprate;
			}
		}
	}

#ifdef WL11AC
	if (rateset->vht_mcsmap != VHT_CAP_MCS_MAP_NONE_ALL) {
		uint8 nss;
		uint8 mcs_code, prop_mcs_code;
		txstreams = MIN(txstreams, VHT_CAP_MCS_MAP_NSS_MAX);

		for (nss = 1; nss <= txstreams; nss++) {
			mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, rateset->vht_mcsmap);
			prop_mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, rateset->vht_mcsmap_prop);
			if (mcs_code == VHT_CAP_MCS_MAP_NONE) {
				/* continue to next stream */
				continue;
			}
			mcs_indx = wlc_get_highest_vht_mcs_index(mcs_code, prop_mcs_code, bw, nss,
				ldpc, ratemask);
			mcs_reprate = mcs_indx | WL_RSPEC_ENCODE_VHT;
			mcs_reprate |= (nss << WL_RSPEC_VHT_NSS_SHIFT);

			mcs_reprate |= (bw << WL_RSPEC_BW_SHIFT);

			if (sgi)
				mcs_reprate |= WL_RSPEC_SGI;

			rate = wf_rspec_to_rate(mcs_reprate);

			if (rate >= max_rate) {
				/*
				 * report VHT rate in lieu of HT rate with same rate.
				 */
				max_rate = rate;
				reprate = mcs_reprate;
			}
		}
	}

#ifdef WL11AX
	/* Test the two possibly supported bandwidths 80 and 160.
	 * The non contiguous 80p80 is not supported by any HW now or the near future
	 */
	for (i = 0; i < 2; i++) {
		uint8 test_bw;
		uint16 mcs_nss_set;
		uint8 nss;
		uint8 mcs_code;

		if (i == 0) {
			/* 80 Mhz is also used for 20 or 40 */
			if (bw == BW_20MHZ)
				test_bw	= BW_20MHZ;
			else if (bw == BW_40MHZ)
				test_bw	= BW_40MHZ;
			else
				test_bw	= BW_80MHZ;
			mcs_nss_set = rateset->he_bw80_tx_mcs_nss;
		}
		else if (i == 1) {
			test_bw	= BW_160MHZ;
			mcs_nss_set = rateset->he_bw160_tx_mcs_nss;
		}

		if (bw < test_bw)
			continue;
		if (mcs_nss_set == HE_CAP_MAX_MCS_NONE_ALL)
			continue;

		txstreams = MIN(txstreams, HE_CAP_MCS_MAP_NSS_MAX);

		for (nss = 1; nss <= txstreams; nss++) {
			mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, mcs_nss_set);

			if (mcs_code == HE_CAP_MAX_MCS_NONE) {
				/* continue to next stream */
				continue;
			}
			mcs_indx = HE_MAX_MCS_TO_INDEX(mcs_code);
			mcs_reprate = mcs_indx | WL_RSPEC_ENCODE_HE;
			mcs_reprate |= (nss << WL_RSPEC_HE_NSS_SHIFT);
			mcs_reprate |= (test_bw << WL_RSPEC_BW_SHIFT);

			rate = wf_rspec_to_rate(mcs_reprate);

			if (rate >= max_rate) {
				/*
				 * report VHT rate in lieu of HT rate with same rate.
				 */
				max_rate = rate;
				reprate = mcs_reprate;
			}
		}
	}
#endif /* WL11AX */
#endif /* WL11AC */
#endif /* WL11N */

	WL_RATE(("%s outputs: reprate = 0x%x; rate = %d \n",
		__FUNCTION__, (uint32)reprate, (uint32)rate));

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

/** Based on bandwidth passed, allow/disallow MCS 32 in the rateset */
void
wlc_rateset_bw_mcs_filter(wlc_rateset_t *rateset, uint8 bw)
{
	BCM_REFERENCE(bw);
#ifndef DISABLE_MCS32_IN_40MHZ
	if (bw == WLC_40_MHZ)
		setbit(rateset->mcs, 32);
	else
#endif /* !DISABLE_MCS32_IN_40MHZ */
		clrbit(rateset->mcs, 32);
}

/**
 * Convert mcs from HT format to non-HT (VHT, HE) format.
 *
 * HT format:
 *	bits [0..2] MCS
 *	bits [3..6] NSS
 *	bit  [7]    unused
 * non-HT format:
 *	bits [0..3] MCS
 *	bits [4..7] NSS
 *
 * @param mcs	MCS in HT format, of MCS_INVALID(0xff)
 * @return	MCS in non-HT format, or MCS_INVALID(0xff)
 */
uint8
wlc_rate_ht_to_nonht(uint8 mcs)
{
	if (mcs != MCS_INVALID) {
		/* Convert to non-HT format */
#if defined(WLPROPRIETARY_11N_RATES)
		uint8 nss = GET_11N_MCS_NSS(mcs);
		uint8 ht_ss_mcs = wf_get_single_stream_mcs(mcs);

		switch (ht_ss_mcs) {
			case 87:
				mcs = 8; /* 256QAM, CR 3/4 */
				break;
			case 88:
				mcs = 9; /* 256QAM, CR 5/6 */
				break;
			default:
				mcs = ht_ss_mcs;
				break;
		}

		mcs |= (nss << WL_RSPEC_VHT_NSS_SHIFT);
#else
		uint8 nss = ((mcs & WLC_RSPEC_HT_NSS_MASK) >> WLC_RSPEC_HT_NSS_SHIFT) + 1;

		mcs &= WLC_RSPEC_HT_MCS_MASK;
		mcs |= (nss << WL_RSPEC_VHT_NSS_SHIFT);
#endif /* WLPROPRIETARY_11N_RATES */
	}

	return mcs;
}

#ifdef WL11AC
void
wlc_rateset_vhtmcs_build(wlc_rateset_t *rateset, uint8 nstreams)
{
	rateset->vht_mcsmap = cck_ofdm_mimo_rates.vht_mcsmap;
	wlc_rateset_vhtmcs_upd(rateset, nstreams);
}

void
wlc_rateset_prop_vhtmcs_build(wlc_rateset_t *rateset, uint8 nstreams)
{
	rateset->vht_mcsmap_prop = cck_ofdm_mimo_rates.vht_mcsmap_prop;
	wlc_rateset_prop_vhtmcs_upd(rateset, nstreams);
}

uint16
wlc_rateset_filter_mcsmap(uint16 vht_mcsmap, uint8 nstreams, uint8 mcsallow)
{
	int stream;
	for (stream = 1; stream <= nstreams; stream++) {
		uint8 mcs = VHT_MCS_MAP_GET_MCS_PER_SS(stream, vht_mcsmap);
		if ((mcs != VHT_CAP_MCS_MAP_NONE) && (mcs > mcsallow)) {
			VHT_MCS_MAP_SET_MCS_PER_SS(stream, mcsallow, vht_mcsmap);
		}
	}

	/* Set MCSMAP entries for streams > nstreams to disabled */
	for (stream = nstreams + 1; stream <= VHT_CAP_MCS_MAP_NSS_MAX; stream++)
		VHT_MCS_MAP_SET_MCS_PER_SS(stream, VHT_CAP_MCS_MAP_NONE, vht_mcsmap);

	return vht_mcsmap;

}

#ifdef WL11AX
void
wlc_rateset_hemcs_build(wlc_rateset_t *rateset, uint8 nstreams)
{
	wlc_rateset_he_cp(rateset, &cck_ofdm_mimo_rates);
	wlc_rateset_hemcs_upd(rateset, nstreams);
}

uint16
wlc_he_rateset_intersection(uint16 set1, uint16 set2)
{
	uint16 return_set;

	return_set = set1;
	wlc_rateset_hemcs_filter(&return_set, set2);

	return return_set;
}

#endif /* WL11AX */

void
wlc_rateset_he_none_all(wlc_rateset_t *rs)
{
	rs->he_bw80_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	rs->he_bw80_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	rs->he_bw160_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	rs->he_bw160_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	rs->he_bw80p80_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	rs->he_bw80p80_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
}

void
wlc_rateset_he_cp(wlc_rateset_t *rs_dst, const wlc_rateset_t *rs_src)
{
	rs_dst->he_bw80_tx_mcs_nss = rs_src->he_bw80_tx_mcs_nss;
	rs_dst->he_bw80_rx_mcs_nss = rs_src->he_bw80_rx_mcs_nss;
	rs_dst->he_bw160_tx_mcs_nss = rs_src->he_bw160_tx_mcs_nss;
	rs_dst->he_bw160_rx_mcs_nss = rs_src->he_bw160_rx_mcs_nss;
	rs_dst->he_bw80p80_tx_mcs_nss = rs_src->he_bw80p80_tx_mcs_nss;
	rs_dst->he_bw80p80_rx_mcs_nss = rs_src->he_bw80p80_rx_mcs_nss;
}

#endif /* WL11AC */
#endif /* WL11N */

void
wlc_rateset_dump(wlc_rateset_t *rs, struct bcmstrbuf *b)
{
	wlc_dump_rateset("legacy", rs, b);
	bcm_bprintf(b, "\n");
	wlc_dump_mcsset("HT", rs->mcs, b);
	bcm_bprintf(b, "\n");
#if defined(WL11AC)
	wlc_dump_vht_mcsmap("VHT", rs->vht_mcsmap, b);
	bcm_bprintf(b, "\n");
	wlc_dump_vht_mcsmap_prop("VHTprop", rs->vht_mcsmap_prop, b);
	bcm_bprintf(b, "\n");
#if defined(WL11AX)
	wlc_dump_he_rateset("HE", rs, b);
	bcm_bprintf(b, "\n");
#endif /* WL11AX */
#endif /* WL11AC */
}
