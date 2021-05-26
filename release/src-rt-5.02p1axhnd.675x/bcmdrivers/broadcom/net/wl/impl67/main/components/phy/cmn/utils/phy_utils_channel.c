/*
 * PHY utils - chanspec functions.
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
 * $Id: phy_utils_channel.c 786808 2020-05-08 17:59:15Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <osl.h>
#include <bcmutils.h>

#include <wlc_phy_int.h>

#include <phy_utils_channel.h>
#include <phy_utils_api.h>

/* channel info structure */
typedef struct phy_chan_info_basic {
	chanspec_t	chan;		/* channel number */
	uint16	freq;		/* in Mhz */
} phy_chan_info_basic_t;

static const phy_chan_info_basic_t chan_info_all[] = {
	/* 11b/11g */
/* 0 */		{WL_CHANSPEC_BAND_2G | 1,	2412},
/* 1 */		{WL_CHANSPEC_BAND_2G | 2,	2417},
/* 2 */		{WL_CHANSPEC_BAND_2G | 3,	2422},
/* 3 */		{WL_CHANSPEC_BAND_2G | 4,	2427},
/* 4 */		{WL_CHANSPEC_BAND_2G | 5,	2432},
/* 5 */		{WL_CHANSPEC_BAND_2G | 6,	2437},
/* 6 */		{WL_CHANSPEC_BAND_2G | 7,	2442},
/* 7 */		{WL_CHANSPEC_BAND_2G | 8,	2447},
/* 8 */		{WL_CHANSPEC_BAND_2G | 9,	2452},
/* 9 */		{WL_CHANSPEC_BAND_2G | 10,	2457},
/* 10 */	{WL_CHANSPEC_BAND_2G | 11,	2462},
/* 11 */	{WL_CHANSPEC_BAND_2G | 12,	2467},
/* 12 */	{WL_CHANSPEC_BAND_2G | 13,	2472},
/* 13 */	{WL_CHANSPEC_BAND_2G | 14,	2484},

#if BAND5G
/* driver defaults to US so keep these channels first */
/* 11a usa low */
/* 14 */	{WL_CHANSPEC_BAND_5G | 36,	5180},
/* 15 */	{WL_CHANSPEC_BAND_5G | 40,	5200},
/* 16 */	{WL_CHANSPEC_BAND_5G | 44,	5220},
/* 17 */	{WL_CHANSPEC_BAND_5G | 48,	5240},
/* 18 */	{WL_CHANSPEC_BAND_5G | 52,	5260},
/* 19 */	{WL_CHANSPEC_BAND_5G | 54,	5270},
/* 20 */	{WL_CHANSPEC_BAND_5G | 56,	5280},
/* 21 */	{WL_CHANSPEC_BAND_5G | 60,	5300},
/* 22 */	{WL_CHANSPEC_BAND_5G | 62,	5310},
/* 23 */	{WL_CHANSPEC_BAND_5G | 64,	5320},

/* 11a Europe */
/* 24 */	{WL_CHANSPEC_BAND_5G | 100,	5500},
/* 25 */	{WL_CHANSPEC_BAND_5G | 102,	5510},
/* 26 */	{WL_CHANSPEC_BAND_5G | 104,	5520},
/* 27 */	{WL_CHANSPEC_BAND_5G | 108,	5540},
/* 28 */	{WL_CHANSPEC_BAND_5G | 110,	5550},
/* 29 */	{WL_CHANSPEC_BAND_5G | 112,	5560},
/* 30 */	{WL_CHANSPEC_BAND_5G | 116,	5580},
/* 31 */	{WL_CHANSPEC_BAND_5G | 118,	5590},
/* 32 */	{WL_CHANSPEC_BAND_5G | 120,	5600},
/* 33 */	{WL_CHANSPEC_BAND_5G | 124,	5620},
/* 34 */	{WL_CHANSPEC_BAND_5G | 126,	5630},
/* 35 */	{WL_CHANSPEC_BAND_5G | 128,	5640},
/* 36 */	{WL_CHANSPEC_BAND_5G | 132,	5660},
/* 37 */	{WL_CHANSPEC_BAND_5G | 134,	5660},
/* 38 */	{WL_CHANSPEC_BAND_5G | 136,	5680},
/* 39 */	{WL_CHANSPEC_BAND_5G | 140,	5700},

/* 11a japan high */
/* 40 */	{WL_CHANSPEC_BAND_5G | 34,	5170},
/* 41 */	{WL_CHANSPEC_BAND_5G | 38,	5190},
/* 42 */	{WL_CHANSPEC_BAND_5G | 42,	5210},
/* 43 */	{WL_CHANSPEC_BAND_5G | 46,	5230},

#ifdef WL11AC
/* 44 */	{WL_CHANSPEC_BAND_5G | 144,	5720},

/* 11a usa high, ref5 only */
/* 45 */	{WL_CHANSPEC_BAND_5G | 149,	5745},
/* 46 */	{WL_CHANSPEC_BAND_5G | 151,	5755},
/* 47 */	{WL_CHANSPEC_BAND_5G | 153,	5765},
/* 48 */	{WL_CHANSPEC_BAND_5G | 157,	5785},
/* 49 */	{WL_CHANSPEC_BAND_5G | 159,	5795},
/* 50 */	{WL_CHANSPEC_BAND_5G | 161,	5805},
/* 51 */	{WL_CHANSPEC_BAND_5G | 165,	5825},

/* 11a japan */
/* 52 */	{WL_CHANSPEC_BAND_5G | 184,	4920},
/* 53 */	{WL_CHANSPEC_BAND_5G | 185,	4925},
/* 54 */	{WL_CHANSPEC_BAND_5G | 187,	4935},
/* 55 */	{WL_CHANSPEC_BAND_5G | 188,	4940},
/* 56 */	{WL_CHANSPEC_BAND_5G | 189,	4945},
/* 57 */	{WL_CHANSPEC_BAND_5G | 192,	4960},
/* 58 */	{WL_CHANSPEC_BAND_5G | 196,	4980},
/* 59 */	{WL_CHANSPEC_BAND_5G | 200,	5000},
/* 60 */	{WL_CHANSPEC_BAND_5G | 204,	5020},
/* 61 */	{WL_CHANSPEC_BAND_5G | 207,	5035},
/* 62 */	{WL_CHANSPEC_BAND_5G | 208,	5040},
/* 63 */	{WL_CHANSPEC_BAND_5G | 209,	5045},
/* 64 */	{WL_CHANSPEC_BAND_5G | 210,	5050},
/* 65 */	{WL_CHANSPEC_BAND_5G | 212,	5060},
/* 66 */	{WL_CHANSPEC_BAND_5G | 216,	5080},

#else

/* 11a usa high, ref5 only */
/* 44 */	{WL_CHANSPEC_BAND_5G | 149,	5745},
/* 45 */	{WL_CHANSPEC_BAND_5G | 151,	5755},
/* 46 */	{WL_CHANSPEC_BAND_5G | 153,	5765},
/* 47 */	{WL_CHANSPEC_BAND_5G | 157,	5785},
/* 48 */	{WL_CHANSPEC_BAND_5G | 159,	5795},
/* 49 */	{WL_CHANSPEC_BAND_5G | 161,	5805},
/* 50 */	{WL_CHANSPEC_BAND_5G | 165,	5825},

/* 11a japan */
/* 51 */	{WL_CHANSPEC_BAND_5G | 184,	4920},
/* 52 */	{WL_CHANSPEC_BAND_5G | 185,	4925},
/* 53 */	{WL_CHANSPEC_BAND_5G | 187,	4935},
/* 54 */	{WL_CHANSPEC_BAND_5G | 188,	4940},
/* 55 */	{WL_CHANSPEC_BAND_5G | 189,	4945},
/* 56 */	{WL_CHANSPEC_BAND_5G | 192,	4960},
/* 57 */	{WL_CHANSPEC_BAND_5G | 196,	4980},
/* 58 */	{WL_CHANSPEC_BAND_5G | 200,	5000},
/* 59 */	{WL_CHANSPEC_BAND_5G | 204,	5020},
/* 60 */	{WL_CHANSPEC_BAND_5G | 207,	5035},
/* 61 */	{WL_CHANSPEC_BAND_5G | 208,	5040},
/* 62 */	{WL_CHANSPEC_BAND_5G | 209,	5045},
/* 63 */	{WL_CHANSPEC_BAND_5G | 210,	5050},
/* 64 */	{WL_CHANSPEC_BAND_5G | 212,	5060},
/* 65 */	{WL_CHANSPEC_BAND_5G | 216,	5080},
#endif /* WL11AC */

#endif /* BAND5G */
#if BAND6G
#ifdef WFA_6G_NEW_CHANNELIZATION
{WL_CHANSPEC_BAND_6G | 2,	5935},
{WL_CHANSPEC_BAND_6G | 1,	5955},
{WL_CHANSPEC_BAND_6G | 5,	5975},
{WL_CHANSPEC_BAND_6G | 9,	5995},
{WL_CHANSPEC_BAND_6G | 13,	6015},
{WL_CHANSPEC_BAND_6G | 17,	6035},
{WL_CHANSPEC_BAND_6G | 21,	6055},
{WL_CHANSPEC_BAND_6G | 25,	6075},
{WL_CHANSPEC_BAND_6G | 29,	6095},
{WL_CHANSPEC_BAND_6G | 33,	6115},
{WL_CHANSPEC_BAND_6G | 37,	6135},
{WL_CHANSPEC_BAND_6G | 41,	6155},
{WL_CHANSPEC_BAND_6G | 45,	6175},
{WL_CHANSPEC_BAND_6G | 49,	6195},
{WL_CHANSPEC_BAND_6G | 53,	6215},
{WL_CHANSPEC_BAND_6G | 57,	6235},
{WL_CHANSPEC_BAND_6G | 61,	6255},
{WL_CHANSPEC_BAND_6G | 65,	6275},
{WL_CHANSPEC_BAND_6G | 69,	6295},
{WL_CHANSPEC_BAND_6G | 73,	6315},
{WL_CHANSPEC_BAND_6G | 77,	6335},
{WL_CHANSPEC_BAND_6G | 81,	6355},
{WL_CHANSPEC_BAND_6G | 85,	6375},
{WL_CHANSPEC_BAND_6G | 89,	6395},
{WL_CHANSPEC_BAND_6G | 93,	6415},
{WL_CHANSPEC_BAND_6G | 97,	6435},
{WL_CHANSPEC_BAND_6G | 101,	6455},
{WL_CHANSPEC_BAND_6G | 105,	6475},
{WL_CHANSPEC_BAND_6G | 109,	6495},
{WL_CHANSPEC_BAND_6G | 113,	6515},
{WL_CHANSPEC_BAND_6G | 117,	6535},
{WL_CHANSPEC_BAND_6G | 121,	6555},
{WL_CHANSPEC_BAND_6G | 125,	6575},
{WL_CHANSPEC_BAND_6G | 129,	6595},
{WL_CHANSPEC_BAND_6G | 133,	6615},
{WL_CHANSPEC_BAND_6G | 137,	6635},
{WL_CHANSPEC_BAND_6G | 141,	6655},
{WL_CHANSPEC_BAND_6G | 145,	6675},
{WL_CHANSPEC_BAND_6G | 149,	6695},
{WL_CHANSPEC_BAND_6G | 153,	6715},
{WL_CHANSPEC_BAND_6G | 157,	6735},
{WL_CHANSPEC_BAND_6G | 161,	6755},
{WL_CHANSPEC_BAND_6G | 165,	6775},
{WL_CHANSPEC_BAND_6G | 169,	6795},
{WL_CHANSPEC_BAND_6G | 173,	6815},
{WL_CHANSPEC_BAND_6G | 177,	6835},
{WL_CHANSPEC_BAND_6G | 181,	6855},
{WL_CHANSPEC_BAND_6G | 185,	6875},
{WL_CHANSPEC_BAND_6G | 189,	6895},
{WL_CHANSPEC_BAND_6G | 193,	6915},
{WL_CHANSPEC_BAND_6G | 197,	6935},
{WL_CHANSPEC_BAND_6G | 201,	6955},
{WL_CHANSPEC_BAND_6G | 205,	6975},
{WL_CHANSPEC_BAND_6G | 209,	6995},
{WL_CHANSPEC_BAND_6G | 213,	7015},
{WL_CHANSPEC_BAND_6G | 217,	7035},
{WL_CHANSPEC_BAND_6G | 221,	7055},
{WL_CHANSPEC_BAND_6G | 225,	7075},
{WL_CHANSPEC_BAND_6G | 229,	7095},
{WL_CHANSPEC_BAND_6G | 233,	7115},
#else
{WL_CHANSPEC_BAND_6G | 1,	5945},
{WL_CHANSPEC_BAND_6G | 5,	5965},
{WL_CHANSPEC_BAND_6G | 9,	5985},
{WL_CHANSPEC_BAND_6G | 13,	6005},
{WL_CHANSPEC_BAND_6G | 17,	6025},
{WL_CHANSPEC_BAND_6G | 21,	6045},
{WL_CHANSPEC_BAND_6G | 25,	6065},
{WL_CHANSPEC_BAND_6G | 29,	6085},
{WL_CHANSPEC_BAND_6G | 33,	6105},
{WL_CHANSPEC_BAND_6G | 37,	6125},
{WL_CHANSPEC_BAND_6G | 41,	6145},
{WL_CHANSPEC_BAND_6G | 45,	6165},
{WL_CHANSPEC_BAND_6G | 49,	6185},
{WL_CHANSPEC_BAND_6G | 53,	6205},
{WL_CHANSPEC_BAND_6G | 57,	6225},
{WL_CHANSPEC_BAND_6G | 61,	6245},
{WL_CHANSPEC_BAND_6G | 65,	6265},
{WL_CHANSPEC_BAND_6G | 69,	6285},
{WL_CHANSPEC_BAND_6G | 73,	6305},
{WL_CHANSPEC_BAND_6G | 77,	6325},
{WL_CHANSPEC_BAND_6G | 81,	6345},
{WL_CHANSPEC_BAND_6G | 85,	6365},
{WL_CHANSPEC_BAND_6G | 89,	6385},
{WL_CHANSPEC_BAND_6G | 93,	6405},
{WL_CHANSPEC_BAND_6G | 97,	6425},
{WL_CHANSPEC_BAND_6G | 101,	6445},
{WL_CHANSPEC_BAND_6G | 105,	6465},
{WL_CHANSPEC_BAND_6G | 109,	6485},
{WL_CHANSPEC_BAND_6G | 113,	6505},
{WL_CHANSPEC_BAND_6G | 117,	6525},
{WL_CHANSPEC_BAND_6G | 121,	6545},
{WL_CHANSPEC_BAND_6G | 125,	6565},
{WL_CHANSPEC_BAND_6G | 129,	6585},
{WL_CHANSPEC_BAND_6G | 133,	6605},
{WL_CHANSPEC_BAND_6G | 137,	6625},
{WL_CHANSPEC_BAND_6G | 141,	6645},
{WL_CHANSPEC_BAND_6G | 145,	6665},
{WL_CHANSPEC_BAND_6G | 149,	6685},
{WL_CHANSPEC_BAND_6G | 153,	6705},
{WL_CHANSPEC_BAND_6G | 157,	6725},
{WL_CHANSPEC_BAND_6G | 161,	6745},
{WL_CHANSPEC_BAND_6G | 165,	6765},
{WL_CHANSPEC_BAND_6G | 169,	6785},
{WL_CHANSPEC_BAND_6G | 173,	6805},
{WL_CHANSPEC_BAND_6G | 177,	6825},
{WL_CHANSPEC_BAND_6G | 181,	6845},
{WL_CHANSPEC_BAND_6G | 185,	6865},
{WL_CHANSPEC_BAND_6G | 189,	6885},
{WL_CHANSPEC_BAND_6G | 193,	6905},
{WL_CHANSPEC_BAND_6G | 197,	6925},
{WL_CHANSPEC_BAND_6G | 201,	6945},
{WL_CHANSPEC_BAND_6G | 205,	6965},
{WL_CHANSPEC_BAND_6G | 209,	6985},
{WL_CHANSPEC_BAND_6G | 213,	7005},
{WL_CHANSPEC_BAND_6G | 217,	7025},
{WL_CHANSPEC_BAND_6G | 221,	7045},
{WL_CHANSPEC_BAND_6G | 225,	7065},
{WL_CHANSPEC_BAND_6G | 229,	7085},
{WL_CHANSPEC_BAND_6G | 233,	7105},
#endif /* WFA_6G_NEW_CHANNELIZATION */
#endif /* BAND6G */
};

chanspec_t
phy_utils_get_chanspec(phy_info_t *pi)
{
	return pi->radio_chanspec;
}

/*
 * Converts channel number to channel frequency.
 * Returns 0 if the channel is out of range.
 * Also used by some code in wlc_iw.c
 */
int
phy_utils_channel2freq(uint channel)
{
	uint i;

	for (i = 0; i < ARRAYSIZE(chan_info_all); i++) {
		if (chan_info_all[i].chan == channel)
			return (chan_info_all[i].freq);
	}

	return (0);
}

/* fill out a chanvec_t with all the supported channels for the band. */
void
phy_utils_chanspec_band_validch(phy_info_t *pi, uint band, chanvec_t *channels)
{
	uint i;
	uint channel;
	chanspec_t chanspec;

	ASSERT((band == WLC_BAND_2G) || (band == WLC_BAND_5G) ||
		(band == WLC_BAND_6G));

	bzero(channels, sizeof(chanvec_t));

	/* Non-HE capable PHY should not report 6G channels */
	if ((band == WLC_BAND_6G) && !(wlc_phy_cap_get(pi) & PHY_CAP_HE)) {
		return;
	}

	for (i = 0; i < ARRAYSIZE(chan_info_all); i++) {
		chanspec = chan_info_all[i].chan;
		/* ignore entries for other bands */
		if (CHSPEC_BANDTYPE(chanspec) != band)
			continue;

		channel = CHSPEC_CHANNEL(chanspec);
		if (band == WLC_BAND_6G) {
#ifdef WFA_6G_NEW_CHANNELIZATION
			if (chan_info_all[i].freq >= 5935)
#else
			if (chan_info_all[i].freq >= 5945)
#endif /* WFA_6G_NEW_CHANNELIZATION */
				setbit(channels->vec, channel);
			continue;
		}
		if (CHSPEC_BANDTYPE(chanspec) == WLC_BAND_5G) {
			/* disable the high band channels [149-165] for srom ver 1 */
			if ((pi->a_band_high_disable) && (channel >= FIRST_REF5_CHANNUM) &&
			    (channel <= LAST_REF5_CHANNUM))
				continue;

			/* Disable channel 144 unless it's an ACPHY */
			if ((channel == 144) && (!ISACPHY(pi)))
				continue;

			/* For Non-HE capable PHY, ch169, 173 are not supported.
			 * For HE capable PHY, ch169, 173 are supported and corresponding
			 * tuning tables are expected to support these channels, too.
			 */
			if ((channel >= 169) && (channel <= 173) &&
				!(wlc_phy_cap_get(pi) & PHY_CAP_HE)) {
				continue;
			}
		}

		if (((band == WLC_BAND_2G) && (channel <= CH_MAX_2G_CHANNEL)) ||
		    ((band == WLC_BAND_5G) && (channel > CH_MAX_2G_CHANNEL)))
			setbit(channels->vec, channel);
	}
}

/* returns the first hw supported channel in the band */
chanspec_t
phy_utils_chanspec_band_firstch(phy_info_t *pi, uint band)
{
	uint i;
	uint channel;
	chanspec_t chanspec;

	ASSERT((band == WLC_BAND_2G) || (band == WLC_BAND_5G) ||
		(band == WLC_BAND_6G));

	/* Non-HE capable PHY should not report 6G channels */
	if ((band == WLC_BAND_6G) && !(wlc_phy_cap_get(pi) & PHY_CAP_HE)) {
		return (chanspec_t)INVCHANSPEC;
	}

	for (i = 0; i < ARRAYSIZE(chan_info_all); i++) {
		chanspec = chan_info_all[i].chan | WL_CHANSPEC_BW_20;

		/* ignore entries for other bands */
		if (CHSPEC_BANDTYPE(chanspec) != band)
			continue;

		channel = CHSPEC_CHANNEL(chanspec);

		/* If 40MHX b/w then check if there is an upper 20Mhz adjacent channel */
		if (IS40MHZ(pi)) {
			uint j;
			/* check if the upper 20Mhz channel exists */
			for (j = 0; j < ARRAYSIZE(chan_info_all); j++) {
				if (chan_info_all[j].chan == chanspec + CH_10MHZ_APART)
					break;
			}
			/* did we find an adjacent channel */
			if (j == ARRAYSIZE(chan_info_all))
				continue;
			/* Convert channel from 20Mhz num to 40 Mhz number */
			channel = UPPER_20_SB(channel);
			chanspec = channel | CHSPEC_BANDTYPE(chanspec) |
				WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
		}
		if (CHSPEC_BANDTYPE(chanspec) == WLC_BAND_5G) {
			/* disable the high band channels [149-165] for srom ver 1 */
			if ((pi->a_band_high_disable) && (channel >= FIRST_REF5_CHANNUM) &&
			    (channel <= LAST_REF5_CHANNUM))
				continue;
		}

		return chanspec;
	}

	/* should never come here */
	ASSERT(0);

	/* to avoid warning */
	return (chanspec_t)INVCHANSPEC;
}
