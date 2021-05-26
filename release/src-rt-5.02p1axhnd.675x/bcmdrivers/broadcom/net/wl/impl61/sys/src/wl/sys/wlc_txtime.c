/*
 * Broadcom 802.11 Networking Device Driver
 *
 * Functionality relating to calculating the airtime
 * of frames, frame components, and frame exchanges.
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
 * $Id: wlc_txtime.c 743420 2018-01-26 13:58:20Z $
 */
/**
 * @file
 * @brief TX airtime calculation
 * Functionality relating to calculating the airtime
 * of frames, frame components, and frame exchanges.
 */

#include <wlc_cfg.h>
#include "typedefs.h"
#include "osl.h"
#include "wlc_txtime.h"
#include "wlc_rate.h"
#include "802.11.h"
#include "802.11ax.h"
#include <bcmwifi_rates.h>
#include <wl_dbg.h>

static uint wlc_txtime_vht_Nes(uint mcs, uint nss, uint bw);
static uint wlc_txtime_ht_Nes(uint mcs, uint bw);
static uint wlc_vht_txtime(uint mcs, uint nss, uint bw, int sgi, int stbc, int ldpc,
	int sig_ext, uint phy_len);
static uint wlc_ht_txtime(uint mcs, uint bw, int sgi, int stbc, int ldpc,
	int mixed_mode, int sig_ext, uint phy_len);
static uint wlc_txtime_legacy(uint mac_rate, int short_preamble, int sig_ext, uint psdu_len);
static uint wlc_txtime_mcs_Ndbps(uint mcs, uint nss, uint bw);
static uint wlc_he_txtime(uint mcs, uint nss, uint bw, uint gi, int stbc, int ldpc,
	uint phy_len, bool dcm);

uint
wlc_txtime(ratespec_t rspec, bool band2g, int short_preamble, uint psdu_len)
{
	uint txtime = 0;

	if (RSPEC_ISHE(rspec)) {
		uint mcs, nss, bw;
		int stbc, ldpc;
		uint gi;
		bool dcm;

		mcs = (uint8)(rspec & WL_RSPEC_HE_MCS_MASK);
		nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;
		bw = RSPEC_BW(rspec);
		stbc = RSPEC_ISSTBC(rspec);
		ldpc = RSPEC_ISLDPC(rspec);
		gi = RSPEC_HE_LTF_GI(rspec);
		dcm = (rspec & WL_RSPEC_DCM) != 0;

		if ((mcs <= WLC_MAX_HE_MCS) &&
			(nss > 0 && nss <= 4) &&
			(bw == WL_RSPEC_BW_20MHZ ||
			bw == WL_RSPEC_BW_40MHZ ||
			bw == WL_RSPEC_BW_80MHZ ||
			bw == WL_RSPEC_BW_160MHZ)) {
			txtime = wlc_he_txtime(mcs, nss, bw, gi, stbc, ldpc, psdu_len, dcm);
		} else {
			WL_ERROR(("Unsupported HE mcs %d or nss %d or bw %d; can't calc txtime\n",
				mcs, nss, bw));
		}
	} else if (RSPEC_ISVHT(rspec)) {
		uint mcs, nss, bw;
		int sgi, stbc, ldpc;

		mcs = (uint8)(rspec & WL_RSPEC_VHT_MCS_MASK);
		nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;
		bw = RSPEC_BW(rspec);
		sgi = RSPEC_ISSGI(rspec);
		stbc = RSPEC_ISSTBC(rspec);
		ldpc = RSPEC_ISLDPC(rspec);

		if ((mcs <= WLC_MAX_VHT_MCS) &&
		    (nss > 0 && nss <= 4) &&
		    (bw == WL_RSPEC_BW_20MHZ ||
		     bw == WL_RSPEC_BW_40MHZ ||
		     bw == WL_RSPEC_BW_80MHZ ||
		     bw == WL_RSPEC_BW_160MHZ)) {
			txtime = wlc_vht_txtime(mcs, nss, bw, sgi, stbc, ldpc, band2g, psdu_len);
		} else {
			WL_ERROR(("Unsupported vht mcs %d or nss %d or bw %d; can't calc txtime\n",
				mcs, nss, bw));
		}

	} else if (RSPEC_ISHT(rspec)) {
		uint mcs, bw;
		int sgi, stbc, ldpc, mm;

		mcs = (rspec & WL_RSPEC_HT_MCS_MASK);
		bw = RSPEC_BW(rspec);
		sgi = RSPEC_ISSGI(rspec);
		stbc = RSPEC_ISSTBC(rspec);
		ldpc = RSPEC_ISLDPC(rspec);
		mm = !short_preamble;

		if (((mcs <= 32) || IS_PROPRIETARY_11N_MCS(mcs)) &&
		    (bw == WL_RSPEC_BW_20MHZ || bw == WL_RSPEC_BW_40MHZ)) {
			txtime = wlc_ht_txtime(mcs, bw, sgi, stbc, ldpc, mm, band2g, psdu_len);
		} else {
			WL_ERROR(("Unsupported ht mcs %d or bw %d; cannot calculate tx time\n",
				mcs, bw));
		}

	} else if (RSPEC_ISLEGACY(rspec)) {
		uint mac_rate = RSPEC2RATE(rspec); /* 500Kbps units, mac_rate = Mbps * 2 */

		txtime = wlc_txtime_legacy(mac_rate, short_preamble, band2g, psdu_len);

	} else {
		WL_ERROR(("Unsupported rate/mcs; cannot calculate tx time\n"));
	}

	return txtime;
}

/* Nes - Number of BCC Encoding streams
 * VHT MCS parameters,
 * MCS 0-9 sec 21.5 (802.11-2016) Parameters for VHT-MCS
 * MCS 10-11 BRCM proprietary rates
 *
 * VHT
 * 20MHz
 *   MCS: 0  1  2  3  4  5  6  7  8  9 10 11
 * ------------------------------------------
 * Nss=1: 1  1  1  1  1  1  1  1  1  X  1  1
 * Nss=2: 1  1  1  1  1  1  1  1  1  X  1  1
 * Nss=3: 1  1  1  1  1  1  1  1  1  1  1  1
 * Nss=4: 1  1  1  1  1  1  1  1  1  X  1  1
 *
 * 40MHz
 *   MCS: 0  1  2  3  4  5  6  7  8  9  10 11
 * ------------------------------------------
 * Nss=1: 1  1  1  1  1  1  1  1  1  1  1  1
 * Nss=2: 1  1  1  1  1  1  1  1  1  1  1  1
 * Nss=3: 1  1  1  1  1  1  1  1  1  1  2  2
 * Nss=4: 1  1  1  1  1  1  1  1  2  2  2  2
 *
 * 80MHz
 *   MCS: 0  1  2  3  4  5  6  7  8  9  10 11
 * ------------------------------------------
 * Nss=1: 1  1  1  1  1  1  1  1  1  1  1  1
 * Nss=2: 1  1  1  1  1  1  1  2  2  2  2  2
 * Nss=3: 1  1  1  1  1  2  X  2  2  3  3  3
 * Nss=4: 1  1  1  1  2  2  2  3  3  3  4  4
 *
 * 160MHz and 80+80MHz
 *   MCS: 0  1  2  3  4  5  6  7  8  9  10 11
 * ------------------------------------------
 * Nss=1: 1  1  1  1  1  1  1  2  2  2  2  2
 * Nss=2: 1  1  1  1  2  2  2  3  3  3  4  4
 * Nss=3: 1  1  1  2  2  3  3  4  4  X  5  6
 * Nss=4: 1  1  2  2  3  4  4  6  6  6  8  8
 */

/* Nes table indexed Nss
 * Each word is 3 bits per MCS, Low bits for Low MCS
 * Literals are in octal
 */

static const uint32 Nes_table_40MHz[] = {
	                         /* 40MHz */
	                         /*   MCS: 0  1  2  3  4  5  6  7  8  9 */
	                         /* ----------------------------------- */
	/* OCTAL */ 01111111111, /* Nss=1: 1  1  1  1  1  1  1  1  1  1 */
	/* OCTAL */ 01111111111, /* Nss=2: 1  1  1  1  1  1  1  1  1  1 */
	/* OCTAL */ 01111111111, /* Nss=3: 1  1  1  1  1  1  1  1  1  1 */
	/* OCTAL */ 02211111111  /* Nss=4: 1  1  1  1  1  1  1  1  2  2 */
};

static const uint32 Nes_table_80MHz[] = {
	                         /* 80MHz */
	                         /*   MCS: 0  1  2  3  4  5  6  7  8  9 */
	                         /* ----------------------------------- */
	/* OCTAL */ 01111111111, /* Nss=1: 1  1  1  1  1  1  1  1  1  1 */
	/* OCTAL */ 02221111111, /* Nss=2: 1  1  1  1  1  1  1  2  2  2 */
	/* OCTAL */ 03222211111, /* Nss=3: 1  1  1  1  1  2  2  2  2  3 */
	/* OCTAL */ 03332221111  /* Nss=4: 1  1  1  1  2  2  2  3  3  3 */
};

static const uint32 Nes_table_160MHz[] = {
	                         /* 160MHz and 80+80MHz */
	                         /*   MCS: 0  1  2  3  4  5  6  7  8  9 */
	                         /* ----------------------------------- */
	/* OCTAL */ 02221111111, /* Nss=1: 1  1  1  1  1  1  1  2  2  2 */
	/* OCTAL */ 03332221111, /* Nss=2: 1  1  1  1  2  2  2  3  3  3 */
	/* OCTAL */ 04443322111, /* Nss=3: 1  1  1  2  2  3  3  4  4  X */
	/* OCTAL */ 06664432211  /* Nss=4: 1  1  2  2  3  4  4  6  6  6 */
};

#define N_ES_EXTRACT(val, mcs) (((val) >> (3*(mcs))) & 0x7)

/* Additional table for proprietary VHT MCS10 and VHT MCS11 */
/* Lower index is MCS 10 */
static const uint8 Nes_table_vhtPropRates[4][4][2] = {
	/* 20MHz */
	{
		/* 1SS */ { 1 , 1 },
		/* 2SS */ { 1 , 1 },
		/* 3SS */ { 1 , 1 },
		/* 4SS */ { 1 , 1 }
	},
	/* 40MHz */
	{
		/* 1SS */ { 1 , 1 },
		/* 2SS */ { 1 , 1 },
		/* 3SS */ { 2 , 2 },
		/* 4SS */ { 2 , 2 }
	},
	/* 80MHz */
	{
		/* 1SS */ { 1 , 1 },
		/* 2SS */ { 2 , 2 },
		/* 3SS */ { 3 , 3 },
		/* 4SS */ { 4 , 4 }
	},
	/* 160MHz or 80MHz+80MHz */
	{
		/* 1SS */ { 2 , 2 },
		/* 2SS */ { 4 , 4 },
		/* 3SS */ { 5 , 6 },
		/* 4SS */ { 8 , 8 }
	}
};

/* This function returns the number of encoded streams for VHT MCS 10 and MCS 11 */
static uint wlc_txtime_vht_Nes_prop(uint mcs, uint nss, uint bw)
{
	int bwidx = ((bw >> WL_RSPEC_BW_SHIFT) - 1);

	ASSERT((mcs == 10) || (mcs == 11));
	ASSERT((bwidx >= 0) && (bwidx <= 3));
	ASSERT((nss >= 1) && (nss <= 4));

	return (uint)Nes_table_vhtPropRates[bwidx][nss - 1][mcs - 10];
}

static uint
wlc_txtime_vht_Nes(uint mcs, uint nss, uint bw)
{
	uint Nes;

	if (mcs > 9) {
		return wlc_txtime_vht_Nes_prop(mcs, nss, bw);
	}

	if (bw == WL_RSPEC_BW_80MHZ) {
		uint32 val;
		val = Nes_table_80MHz[nss - 1];
		Nes = N_ES_EXTRACT(val, mcs);
	} else if (bw == WL_RSPEC_BW_40MHZ) {
		uint32 val;
		val = Nes_table_40MHz[nss - 1];
		Nes = N_ES_EXTRACT(val, mcs);
	} else if (bw == WL_RSPEC_BW_160MHZ) {
		uint32 val;
		val = Nes_table_160MHz[nss - 1];
		Nes = N_ES_EXTRACT(val, mcs);
	} else {
		ASSERT(bw == WL_RSPEC_BW_20MHZ);
		Nes = 1;
	}

	return Nes;
}

/* Nes - Number of BCC Encoding streams
 * HT MCS parameters, HT sec 20.6
 *
 * HT
 * 20MHz
 * MCS 0 - 23 are all Nes = 1
 *
 * 40MHz
 *   MCS: 0  1  2  3  4  5  6  7
 * -----------------------------
 * Nss=1: 1  1  1  1  1  1  1  1
 *
 *   MCS: 8  9 10 11 12 13 14 15
 * -----------------------------
 * Nss=2: 1  1  1  1  1  1  1  1
 *
 *   MCS:16 17 18 19 20 21 22 23
 * -----------------------------
 * Nss=3: 1  1  1  1  1  2  2  2
 *
 *   MCS:24 25 26 27 28 29 30 31
 * -----------------------------
 * Nss=4: 1  1  1  1  2  2  2  2
 */
static uint
wlc_txtime_ht_Nes(uint mcs, uint bw)
{
	uint Nes;

	if (bw == WL_RSPEC_BW_40MHZ) {
		if ((mcs >= 21 && mcs < 24) ||
		    (mcs >= 28 && mcs < 32) ||
#if defined(WLPROPRIETARY_11N_RATES)
		    /* MCS 99, 100, 101, 102 are Nes 2 in 40MHz */
		    (mcs >= 99) ||
#endif /* WLPROPRIETARY_11N_RATES */
		    FALSE) {
			Nes = 2;
		} else {
			Nes = 1;
		}
	} else {
		ASSERT(bw == WL_RSPEC_BW_20MHZ);
		Nes = 1;
	}

	return Nes;
}

uint
wlc_txtime_Nes(ratespec_t rspec)
{
	uint Nes;

	if (RSPEC_ISHE(rspec)) {
		/* (Nes) number of BCC encoding schemes
		*
		* Note: for HE it's always 1 (802.11ax sec 28.3.11.5.4)
		*/
		Nes = 1;
	} else if (RSPEC_ISVHT(rspec)) {
		uint mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		/* wlc_txtime_vht_Nes() handles mcs 0-11, and nss 1-4
		 * The MCS range is the spec limit.
		 * NSS can be 1-8 by spec, but we only handle 1-4
		 */
		ASSERT(mcs <= WLC_MAX_VHT_MCS);
		ASSERT(nss <= 4 && nss > 0);

		Nes = wlc_txtime_vht_Nes(mcs, nss, RSPEC_BW(rspec));
	} else if (RSPEC_ISHT(rspec)) {
		uint mcs = (rspec & WL_RSPEC_HT_MCS_MASK);

		ASSERT(mcs <= 32 || IS_PROPRIETARY_11N_MCS(mcs));

		Nes = wlc_txtime_ht_Nes(mcs, RSPEC_BW(rspec));
	} else if (RSPEC_ISLEGACY(rspec)) {
		/* legacy rates can be considered as 1 Nes */
		Nes = 1;
	} else {
		ASSERT(0);
		Nes = 0;
	}

	return Nes;
}

static uint
wlc_he_txtime(uint mcs, uint nss, uint bw, uint gi, int stbc, int ldpc, uint phy_len,
	bool dcm)
{
	uint txtime;
	uint Nsym;
	uint Nes;
	uint Ndbps;
	uint Nheltf;
	const uint fixed_time = HE_T_LEG_PREAMBLE +
		HE_T_RL_SIG + HE_T_SIGA + HE_T_STF;
	uint t_heltf, Tsym;

	/* (Nes) number of BCC encoding schemes
	 *
	 * Note: for HE it's always 1 (802.11ax sec 28.3.11.5.4)
	 */
	Nes = 1;

	Ndbps = wf_he_mcs_to_Ndbps(mcs, nss, bw, dcm);

	Nsym = ((8 * phy_len) + HE_N_SERVICE + (HE_N_TAIL * Nes) + Ndbps - 1) / Ndbps;

	/* calculate N_HELTF from Nsts and perform the STBC rounding of Nsym
	 * Calculation is done under the STBC check so that STBC is checked only once
	 * and N_HELTF can be streamlined.
	 */

	if (stbc) {
		/* 802.11ax (28.3.11.10) STBC is allowed only with Nss == 1 and Nsts = Nss*2
		 * Since Nsts is even, the N_HELTF mapping is just one-to-one
		 */

		Nheltf = 2; /* N_HELTF = Nsts = 2*Nss */

		/* Round up to an even number of symbos if STBC
		 * This takes the place of the part of the TXTIME calculation using Mstbc
		 * to provide the rounding.
		 */
		Nsym = (Nsym + 1) & ~1;
	} else {
		/* Nsts = Nss when STBC is not used */
		if (nss < 3) {
			Nheltf = nss; /* N_HELTF = Nsts = Nss, when Nss = 1,2 */
		} else {
			/* For Nsts >= 3, the LTF count matches a
			 * shifted mask operation. N_HELTF = (Nsts + 1) & ~1;
			 *
			 * Nss is always <= 8 by the spec, and currently <=3 for our hardware,
			 * so it will alway fit in a byte. Using the mask 0xFE instead of
			 * 0xFFFFFFFE or ~1 so that the object code can load a smaller literal.
			 * Could also consider a shift down and up for the same effect:
			 * x = (x >> 1)
			 * x = (x << 1)
			 * in case a process can do that faster than load a literal and mask.
			 */
			Nheltf = (nss + 1) & 0xFE;
		}
	}

	/* Get HE-LTF time (in multiples of 10) */
	if (HE_IS_1X_LTF(gi)) {
		t_heltf = HE_T_LTF_1X;
	} else if (HE_IS_2X_LTF(gi)) {
		t_heltf = HE_T_LTF_2X;
	} else {
		/* HE_IS_4X_LTF(gi) */
		t_heltf = HE_T_LTF_4X;
	}

	/**
	 * Tsym1 (OFDM symbol duration with base GI) = symbol time 13.6 usec
	 * Tsym2 (OFDM symbol duration with double GI) = symbol time 14.4 usec
	 * Tsym4 (OFDM symbol duration with quad GI) = symbol time 16 usec
	 */
	if (HE_IS_GI_0_8us (gi)) {
		Tsym = HE_T_SYM1;
	} else if (HE_IS_GI_1_6us (gi)) {
		Tsym = HE_T_SYM2;
	} else {
		Tsym = HE_T_SYM4;
	}

	/* Calculating total HE LTF time (in multiples of 10) */
	txtime = Nheltf * t_heltf;

	/* Compute total Symbol time (also in multiples of 10) :
	 * tot_time = Number of symbols * Individual OFDM Symbol time
	 */
	txtime += (Tsym * Nsym);

	/* Add fixed portion (fixed time) computed above to total Symbol time.
	 *
	 * Note: txtime is a multiple of 100, convert fixed portion also a
	 * multiple of 100 before adding
	 */
	txtime += (fixed_time * 100);

	/* Divide txtime (in multiples of 100) by 100 */
	txtime = CEIL(txtime, 100);

	return txtime;
}

static uint
wlc_vht_txtime(uint mcs, uint nss, uint bw, int sgi, int stbc, int ldpc, int sig_ext, uint phy_len)
{
	uint txtime;
	uint Nsym;
	uint Nes;
	uint Ndbps;
	uint Nvhtltf;
	const uint fixed_time =
	        VHT_T_LEG_PREAMBLE + VHT_T_L_SIG +
	        VHT_T_SIG_A + VHT_T_STF + VHT_T_SIG_B;

	if (!ldpc) {
		Nes = wlc_txtime_vht_Nes(mcs, nss, bw);
	} else {
		Nes = 0;
	}

	Ndbps = wlc_txtime_mcs_Ndbps(mcs, nss, bw);

	Nsym = ((8 * phy_len) + VHT_N_SERVICE + (VHT_N_TAIL * Nes) + Ndbps - 1) / Ndbps;

	/* Table for Nvhtltf from Nsts.
	 * Draft 802.11ac D4.0, table 22-13.
	 *
	 * Nsts | N_VHTLTF
	 *    1 |  1
	 *    2 |  2
	 *    3 |  4
	 *    4 |  4
	 *    5 |  6
	 *    6 |  6
	 *    7 |  8
	 *    8 |  8
	 */

	/* calculate N_VHTLTF from Nsts and perform the STBC rounding of Nsym
	 * Calculation is done under the STBC check so that STBC is checked only once
	 * and N_VHTLTF can be streamlined.
	 */

	if (stbc) {
		/* VHT STBC expansion always is Nsts = Nss*2
		 * Since Nsts is even, the N_VHTLTF mapping is just one-to-one
		 */

		Nvhtltf = 2*nss; /* N_VHTLTF = Nsts = 2*Nss */

		/* Round up to an even number of symbos if STBC
		 * This takes the place of the part of the TXTIME calculation using Mstbc
		 * to provide the rounding.
		 */
		Nsym = (Nsym + 1) & ~1;
	} else {
		/* Nsts = Nss when STBC is not used */
		if (nss < 3) {
			Nvhtltf = nss; /* N_VHTLTF = Nsts = Nss, when Nss = 1,2 */
		} else {
			/* For Nsts >= 3, the LTF count from the table above matches a
			 * shifted mask operation. N_VHTLTF = (Nsts + 1) & ~1;
			 *
			 * Nss is always <= 8 by the spec, and currently <=3 for our hardware,
			 * so it will alway fit in a byte. Using the mask 0xFE instead of
			 * 0xFFFFFFFE or ~1 so that the object code can load a smaller literal.
			 * Could also consider a shift down and up for the same effect:
			 * x = (x >> 1)
			 * x = (x << 1)
			 * in case a process can do that faster than load a literal and mask.
			 */
			Nvhtltf = (nss + 1) & 0xFE;
		}
	}

	/* Add up the fixed portion and the VHT Preamble */
	txtime = fixed_time + Nvhtltf * VHT_T_LTF;

	/* Calculate the Nsym contribution based on SGI or Reg GI
	 *
	 * Tsym = symbol time 4 us, or 3.6 us for SGI
	 * Tsyml = 4   (sym long GI)
	 * Tsyms = 36/10 (sym short GI)
	 *
	 * Short GI round time to 4us symbol time
	 * T = Tsyml * CEILING( Tsyms * Nsym / Tsyml )
	 * T = 4 * CEILING( (36/10) * Nsym / 4 )
	 * T = 4 * CEILING( 36 * Nsym / 10 * 4 )
	 * T = 4 * FLOOR( ((36 * Nsym) + 39) / 40 )
	 *
	 * Long GI:
	 * T = Tsyml * Nsym
	 * T = 4 * Nsym
	 */

	if (sgi) {
		txtime += VHT_T_SYML * ((36 * Nsym + 39) / 40);
	} else {
		txtime += VHT_T_SYML * Nsym;
	}

	if (sig_ext)
		txtime += DOT11_OFDM_SIGNAL_EXTENSION;

	return txtime;
}

static uint
wlc_ht_txtime(uint mcs, uint bw, int sgi, int stbc, int ldpc, int mixed_mode,
              int sig_ext, uint phy_len)
{
	uint txtime;
	uint Nss;
	uint Nsym;
	uint Nes;
	uint Ndbps;
	uint Nltf;

	if (!ldpc) {
		Nes = wlc_txtime_ht_Nes(mcs, bw);
	} else {
		Nes = 0;
	}

	if (mcs == 32) {
		Nss = 1;
		Ndbps = 24;
	}
#if defined(WLPROPRIETARY_11N_RATES)
	else if (mcs > 32) {
		int v_mcs;

		ASSERT(IS_PROPRIETARY_11N_MCS(mcs));

		Nss = GET_PROPRIETARY_11N_MCS_NSS(mcs);

		/* 87, 99, 101 have the same encoding as VHT MCS 8, and
		 * 88, 100, 102 have the same encoding as VHT MCS 9
		 */
		if (mcs == 88 || mcs == 99 || mcs == 101) {
			v_mcs = 8;
		} else {
			v_mcs = 9;
		}

		Ndbps = wlc_txtime_mcs_Ndbps(v_mcs, Nss, bw);
	}
#endif /* WLPROPRIETARY_11N_RATES */
	else {
		int v_mcs = mcs & 7;
		Nss = 1 + (mcs / 8);

		Ndbps = wlc_txtime_mcs_Ndbps(v_mcs, Nss, bw);
	}

	Nsym = ((8 * phy_len) + HT_N_SERVICE + (HT_N_TAIL * Nes) + Ndbps - 1) / Ndbps;

	/* Table for Nltf from Nsts.
	 * 802.11-2012 sec 20.3.9.4.6, table 20-13
	 *
	 * Nsts | N_HTDLTF
	 *    1 |  1
	 *    2 |  2
	 *    3 |  4
	 *    4 |  4
	 */

	/* calculate N_LTF from Nsts and perform the STBC rounding of Nsym
	 * Calculation is done under the STBC check so that STBC is checked only once
	 * and N_LTF can be streamlined.
	 */

	if (stbc) {
		/* BRCM HT STBC expansion only supported for Nss = 1 to Nsts = 2
		 * Since Nsts is even, the N_LTF mapping is just one-to-one
		 */
		ASSERT(Nss == 1);

		Nltf = 2; /* N_LTF = Nsts = 2 */

		/* Round up to an even number of symbos if STBC
		 * This takes the place of the part of the TXTIME calculation using Mstbc
		 * to provide the rounding.
		 */
		Nsym = (Nsym + 1) & ~1;
	} else {
		/* Nsts = Nss when STBC is not used */
		if (Nss < 3) {
			Nltf = Nss; /* N_LTF = Nsts = Nss, when Nss = 1,2 */
		} else {
			/* For Nsts = 3, 4, the LTF count from the table above is 4 */
			Nltf = 4;
		}
	}

	if (mixed_mode) {

		/* Add up the fixed portion and the HT preamble */

		txtime = HT_T_LEG_PREAMBLE + HT_T_L_SIG + HT_T_SIG +
		        HT_T_STF + (HT_T_LTF1 - HT_T_LTFs) + Nltf * HT_T_LTFs;

		/* Calculate the Nsym contribution based on SGI or Reg GI
		 *
		 * Tsym = symbol time 4 us, or 3.6 us for SGI
		 * Tsyml = 4   (sym long GI)
		 * Tsyms = 36/10 (sym short GI)
		 *
		 * Short GI round time to 4us symbol time for Mixed Mode
		 * T = Tsyml * CEILING( Tsyms * Nsym / Tsyml )
		 * T = 4 * CEILING( (36/10) * Nsym / 4 )
		 * T = 4 * CEILING( 36 * Nsym / 10 * 4 )
		 * T = 4 * FLOOR( ((36 * Nsym) + 39) / 40 )
		 *
		 * Long GI:
		 * T = Tsyml * Nsym
		 * T = 4 * Nsym
		 */

		if (sgi) {
			txtime += HT_T_SYML * ((36 * Nsym + 39) / 40);
		} else {
			txtime += HT_T_SYML * Nsym;
		}
	} else {

		/* Add up the fixed portion and the HT GF preamble */

		txtime = HT_T_SIG +
		        HT_T_GF_STF + (HT_T_GF_LTF1 - HT_T_LTFs) + Nltf * HT_T_LTFs;

		/* Calculate the Nsym contribution based on SGI or Reg GI
		 *
		 * Tsym = symbol time 4 us, or 3.6 us for SGI
		 * Tsyml = 4   (sym long GI)
		 * Tsyms = 36/10 (sym short GI)
		 *
		 * Short GI round time to 1us for Green Field
		 * T = CEILING( Tsyms * Nsym )
		 * T = CEILING( (36/10) * Nsym )
		 * T = CEILING( 36 * Nsym / 10 )
		 * T = FLOOR( ((36 * Nsym) + 9) / 10 )
		 *
		 * Long GI:
		 * T = Tsyml * Nsym
		 * T = 4 * Nsym
		 */

		if (sgi) {
			txtime += HT_T_SYML * ((36 * Nsym + 39) / 40);
			/* txtime += ((36 * Nsym + 9) / 10); */
		} else {
			txtime += HT_T_SYML * Nsym;
		}
	}

	if (sig_ext)
		txtime += DOT11_OFDM_SIGNAL_EXTENSION;

	return txtime;
}

/* calculate TXTIME for OFDM and DSSS */
static uint
wlc_txtime_legacy(uint mac_rate, int short_preamble, int sig_ext, uint psdu_len)
{
	uint txtime;

	if (mac_rate > DOT11_RATE_5M5 && mac_rate != DOT11_RATE_11M) {

		/* OFDM rate
		 * 4us symbol time
		 *
		 * Ndbps = Mbps * 4 = mac_rate * 2
		 *
		 * Nsym = CEILING( (SERVICE + len * 8 + TAIL) / Ndbps)
		 * Nsym = FLOOR( ((SERVICE + len * 8 + TAIL) + (Ndbps - 1) / Ndbps )
		 * Nsym = FLOOR( ((SERVICE + len * 8 + TAIL) + (mac_rate*2 - 1) / mac_rate*2 )
		 * Nsym = FLOOR( (len * 4 + (SERVICE + TAIL) / 2 + (mac_rate - 1) / mac_rate )
		 * Nsym = FLOOR( (len * 4 + 22 / 2 + (mac_rate - 1) / mac_rate )
		 *
		 * T = 4us * Nsym
		 * T = 4us * FLOOR( (len * 4 + 11 + (mac_rate - 1) / mac_rate )
		 */

		txtime = ((psdu_len * 4) +
		          (APHY_SERVICE_NBITS + APHY_TAIL_NBITS)/2 + mac_rate - 1) /
		        mac_rate;
		txtime = 4 * txtime;

		txtime += APHY_PREAMBLE_TIME + APHY_SIGNAL_TIME;

		if (sig_ext)
			txtime += DOT11_OFDM_SIGNAL_EXTENSION;

	} else if ((mac_rate == DOT11_RATE_11M) ||
	           (mac_rate == DOT11_RATE_5M5)) {

		/* DSSS rate (11Mbps or 5.5 Mbps
		 * 1us symbols
		 * T = CEILING( len * 8 / Mbps )
		 * T = FLOOR( (len * 8 + (Mbps - 1)) / Mbps )
		 * T = FLOOR( (len * 8 + (mac_rate/2 - 1)) / mac_rate/2 )
		 * T = FLOOR( (len * 8 * 2 + (mac_rate - 1)) / mac_rate )
		 */

		txtime = (psdu_len * 8 * 2 + (mac_rate - 1)) / mac_rate;

		if (short_preamble)
			txtime += BPHY_PLCP_SHORT_TIME;
		else
			txtime += BPHY_PLCP_TIME;

	} else if (mac_rate == DOT11_RATE_2M) {

		/* DSSS rate 2Mbps
		 * T = CEILING( len * 8 / Mbps )
		 * T = CEILING( len * 8 / 2 )
		 * T = ( len * 4 )
		 */

		txtime = (psdu_len * 4);

		if (short_preamble)
			txtime += BPHY_PLCP_SHORT_TIME;
		else
			txtime += BPHY_PLCP_TIME;

	} else if (mac_rate == DOT11_RATE_1M) {

		/* DSSS rate 1Mbps
		 * T = CEILING( len * 8 / Mbps )
		 * T = CEILING( len * 8 / 1 )
		 * T = ( len * 8 )
		 */

		txtime = (psdu_len * 8) + BPHY_PLCP_TIME;

	} else {
		/* invalid rspec, give a good assert line */
		ASSERT(mac_rate == DOT11_RATE_1M ||
		       mac_rate == DOT11_RATE_2M ||
		       mac_rate == DOT11_RATE_5M5 ||
		       mac_rate == DOT11_RATE_11M ||
		       mac_rate == DOT11_RATE_6M ||
		       mac_rate == DOT11_RATE_9M ||
		       mac_rate == DOT11_RATE_12M ||
		       mac_rate == DOT11_RATE_18M ||
		       mac_rate == DOT11_RATE_24M ||
		       mac_rate == DOT11_RATE_36M ||
		       mac_rate == DOT11_RATE_48M ||
		       mac_rate == DOT11_RATE_54M);
		txtime = 0;
	}

	return txtime;
}

uint
wlc_txtime_data(ratespec_t rspec, uint data_len)
{
	uint txtime;

	if (RSPEC_ISHT(rspec) ||
	    RSPEC_ISVHT(rspec)) {
		/* HT or VHT OFDM
		 * Tsym = symbol time 4 us, or 3.6 us for SGI
		 * Tsyml = 4   (sym long GI)
		 * Tsyms = 36/10 (sym short GI)
		 *
		 * T = Tsym * Nsym
		 * Nsym = CEILING( (8*len) / Ndbps )
		 * Nsym = FLOOR( (8*len + Ndbps - 1) / Ndbps )
		 *
		 * Long GI:
		 * T = Tsyml * Nsym
		 * T = 4 * Nsym
		 *
		 * Short GI round time to 4us symbol time
		 * T = Tsyml * CEILING( Tsyms * Nsym / Tsyml )
		 * T = 4 * CEILING( (36/10) * Nsym / 4 )
		 * T = 4 * CEILING( 36 * Nsym / 10 * 4 )
		 * T = 4 * FLOOR( ((36 * Nsym) + 39) / 40 )
		 */
		uint Ndbps, Nsym;

		/* calc Nsym */
		Ndbps = wlc_txtime_Ndbps(rspec);
		Nsym = (8 * data_len + Ndbps - 1) / Ndbps;

		/* adjust the count of 3.6us symbols to a count of 4us symbols */
		if (RSPEC_ISSGI(rspec)) {
			Nsym = (36 * Nsym + 39) / 40;
		}

		txtime = 4 * Nsym;
	} else if (RSPEC_ISLEGACY(rspec)) {
		uint mac_rate = RSPEC2RATE(rspec); /* 500Kbps units, mac_rate = Mbps * 2 */

		/* calculate TXTIME for OFDM and DSSS */

		if (mac_rate > DOT11_RATE_5M5 && mac_rate != DOT11_RATE_11M) {

			/* OFDM rate
			 * 4us symbol time
			 *
			 * Ndbps = Mbps * 4 = mac_rate * 2
			 *
			 * Nsym = CEILING(len * 8 / Ndbps)
			 * Nsym = FLOOR( (len * 8 + (Ndbps - 1) / Ndbps )
			 * Nsym = FLOOR( (len * 8 + (mac_rate*2 - 1) / mac_rate*2 )
			 * Nsym = FLOOR( (len * 4 + (mac_rate - 1) / mac_rate )
			 *
			 * T = 4us * Nsym
			 * T = 4us * FLOOR( (len * 4 + (mac_rate - 1) / mac_rate )
			 */

			txtime = ((data_len * 4) + mac_rate - 1) / mac_rate;
			txtime = 4 * txtime;
		} else if ((mac_rate == DOT11_RATE_11M) ||
		           (mac_rate == DOT11_RATE_5M5)) {

			/* DSSS rate (11Mbps or 5.5 Mbps
			 * 1us symbols
			 * T = CEILING( len * 8 / Mbps )
			 * T = FLOOR( (len * 8 + (Mbps - 1)) / Mbps )
			 * T = FLOOR( (len * 8 + (mac_rate/2 - 1)) / mac_rate/2 )
			 * T = FLOOR( (len * 8 * 2 + (mac_rate - 1)) / mac_rate )
			 */

			txtime = (data_len * 8 * 2 + (mac_rate - 1)) / mac_rate;

		} else if (mac_rate == DOT11_RATE_2M) {

			/* DSSS rate 2Mbps
			 * T = CEILING( len * 8 / Mbps )
			 * T = CEILING( len * 8 / 2 )
			 * T = ( len * 4 )
			 */

			txtime = (data_len * 4);

		} else if (mac_rate == DOT11_RATE_1M) {

			/* DSSS rate 1Mbps
			 * T = CEILING( len * 8 / Mbps )
			 * T = CEILING( len * 8 / 1 )
			 * T = ( len * 8 )
			 */

			txtime = (data_len * 8);

		} else {
			/* invalid rspec, give a good assert line */
			ASSERT(RSPEC_ISOFDM(rspec) || RSPEC_ISCCK(rspec));
			/* Humm, if the rspec is OFDM OR CCK, logic above may be wrong */
			ASSERT(0);
			txtime = 0;
		}
	} else {
		ASSERT(RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec) || RSPEC_ISLEGACY(rspec));
		txtime = 0;
	}

	return txtime;
}

uint
wlc_txtime_Ndbps(ratespec_t rspec)
{
	uint Ndbps;

	if (RSPEC_ISLEGACY(rspec)) {
		uint mac_rate = RSPEC2RATE(rspec); /* 500Kbps units, mac_rate = Mbps * 2 */

		/* calculate Ndbps for DSSS and OFDM */

		if (mac_rate <= DOT11_RATE_11M && mac_rate != DOT11_RATE_6M) {
			/* DSSS sym time is 1us, so Ndbps is just the Mbps value.
			 * Rate 5.5 Mbps will round to 5 bits/sym.
			 */
			Ndbps = mac_rate / 2;
		} else {
			/* OFDM symbol time is 4us, so Ndbps is the Mbps value
			 * times 4us.
			 * Ndbps = Mbps * 4 = rate(500Kbps) * 2
			 */
			Ndbps = mac_rate * 2;
		}
	} else if (RSPEC_ISHT(rspec)) {
		uint mcs = (rspec & WL_RSPEC_HT_MCS_MASK);

		ASSERT(mcs <= 32 || IS_PROPRIETARY_11N_MCS(mcs));

		if (mcs == 32) {
			Ndbps = 24;
		} else {
			uint nss;

#if defined(WLPROPRIETARY_11N_RATES)
			if (mcs > 32) {

				nss = GET_PROPRIETARY_11N_MCS_NSS(mcs);

				/* 87, 99, 101 have the same encoding as VHT MCS 8, and
				 * 88, 100, 102 have the same encoding as VHT MCS 9
				 */
				if (mcs == 87 || mcs == 99 || mcs == 101) {
					mcs = 8;
				} else {
					mcs = 9;
				}
			} else
#endif /* WLPROPRIETARY_11N_RATES */
			{
				nss = 1 + (mcs / 8);
				mcs = mcs % 8;
			}

			Ndbps = wlc_txtime_mcs_Ndbps(mcs, nss, RSPEC_BW(rspec));
		}
	} else if (RSPEC_ISVHT(rspec)) {
		uint mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		ASSERT(mcs <= WLC_MAX_VHT_MCS);
		ASSERT(nss <= 8);

		Ndbps = wlc_txtime_mcs_Ndbps(mcs, nss, RSPEC_BW(rspec));
	} else if (RSPEC_ISHE(rspec)) {
		uint mcs = (rspec & WL_RSPEC_HE_MCS_MASK);
		uint nss = (rspec & WL_RSPEC_HE_NSS_MASK) >>
			WL_RSPEC_HE_NSS_SHIFT;
		bool dcm = (rspec & WL_RSPEC_DCM) != 0;

		ASSERT(mcs <= WLC_MAX_HE_MCS);
		ASSERT(nss <= 8);

		Ndbps = wf_he_mcs_to_Ndbps(mcs, nss,
			RSPEC_BW(rspec), dcm);
	} else {
		ASSERT(0);
		Ndbps = 0;
	}

	return (Ndbps == 0) ? (uint)-1 : Ndbps;
}

static uint
wlc_txtime_mcs_Ndbps(uint mcs, uint nss, uint bw)
{
	uint Ndbps;

	if (mcs == 32) {
		/* just return fixed values for mcs32 instead of trying to parametrize */
		Ndbps = 24;
	} else if (mcs <= WLC_MAX_VHT_MCS) {
		Ndbps = wf_mcs_to_Ndbps(mcs, nss, bw);
	} else {
		Ndbps = 0;
	}

	return Ndbps;
}
