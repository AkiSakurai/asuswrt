/*
 * RadioTap utility routines for WL
 * This file housing the functions use by
 * wl driver.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id:$
 */


#include <wlc_cfg.h>

#ifndef WL_MONITOR
#error "WL_MONITOR is not defined"
#endif	/* WL_MONITOR */

#include <typedefs.h>
#include <wl_dbg.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <bcmwifi_channels.h>
#include <wl_radiotap.h>

static void
wl_rtapParseReset(radiotap_parse_t *rtap)
{
	rtap->idx = 0;		/* reset parse index */
	rtap->offset = 0;	/* reset current field pointer */
}

static void*
wl_rtapParseFindField(radiotap_parse_t *rtap, uint search_idx)
{
	uint idx;	/* first bit index to parse */
	uint32 bmap;	/* presence bitmap */
	uint offset, field_offset;
	uint align, len;
	void *ptr = NULL;

	if (search_idx > IEEE80211_RADIOTAP_EXT)
		return ptr;

	if (search_idx < rtap->idx)
		wl_rtapParseReset(rtap);

	bmap = rtap->hdr->it_present;
	idx = rtap->idx;
	offset = rtap->offset;

	/* loop through each field index until we get to the target idx */
	while (idx <= search_idx) {
		/* if field 'idx' is present, update the offset and check for a match */
		if ((1 << idx) & bmap) {
			/* if we hit a field for which we have no parse info
			 * we need to just bail out
			 */
			if (rtap_parse_info[idx].align == 0)
				break;

			/* step past any alignment padding */
			align = rtap_parse_info[idx].align;
			len = rtap_parse_info[idx].len;

			/* ROUNDUP */
			field_offset = ((offset + (align - 1)) / align) * align;

			/* if this field is not in the boulds of the header
			 * just bail out
			 */
			if (field_offset + len > rtap->fields_len)
				break;

			/* did we find the field? */
			if (idx == search_idx)
				ptr = (uint8*)rtap->fields + field_offset;

			/* step past this field */
			offset = field_offset + len;
		}

		idx++;
	}

	rtap->idx = idx;
	rtap->offset = offset;

	return ptr;
}

ratespec_t
wl_calcRspecFromRTap(uint8 *rtap_header)
{
	ratespec_t rspec = 0;
	radiotap_parse_t rtap;
	uint8 rate = 0;
	uint8 flags = 0;
	int flags_present = FALSE;
	uint8 mcs = 0;
	uint8 mcs_flags = 0;
	uint8 mcs_known = 0;
	int mcs_present = FALSE;
	void *p;

	wl_rtapParseInit(&rtap, rtap_header);

	p = wl_rtapParseFindField(&rtap, IEEE80211_RADIOTAP_FLAGS);
	if (p != NULL) {
		flags_present = TRUE;
		flags = ((uint8*)p)[0];
	}

	p = wl_rtapParseFindField(&rtap, IEEE80211_RADIOTAP_RATE);
	if (p != NULL)
		rate = ((uint8*)p)[0];

	p = wl_rtapParseFindField(&rtap, IEEE80211_RADIOTAP_MCS);
	if (p != NULL) {
		mcs_present = TRUE;
		mcs_known = ((uint8*)p)[0];
		mcs_flags = ((uint8*)p)[1];
		mcs = ((uint8*)p)[2];
	}

	if (rate != 0) {
		/* validate the DSSS rates 1,2,5.5,11 */
		if (rate == 2 || rate == 4 || rate == 11 || rate == 22) {
			rspec = LEGACY_RSPEC(rate) | RSPEC_OVERRIDE_RATE;
			if (flags_present && (flags & IEEE80211_RADIOTAP_F_SHORTPRE)) {
				rspec |= RSPEC_OVERRIDE_MODE | RSPEC_SHORT_PREAMBLE;
			}
		}
	} else if (mcs_present) {
		/* validate the MCS value */
		if (mcs <= 23 || mcs == 32 || IS_PROPRIETARY_11N_MCS(mcs)) {
			uint32 override = 0;
			if (mcs_known &
			    (IEEE80211_RADIOTAP_MCS_HAVE_GI |
			     IEEE80211_RADIOTAP_MCS_HAVE_FMT |
			     IEEE80211_RADIOTAP_MCS_HAVE_FEC)) {
				override = RSPEC_OVERRIDE_MODE;
			}

			rspec = HT_RSPEC(mcs) | RSPEC_OVERRIDE_RATE;

			if ((mcs_known & IEEE80211_RADIOTAP_MCS_HAVE_GI) &&
			    (mcs_flags & IEEE80211_RADIOTAP_MCS_SGI))
				rspec |= RSPEC_SHORT_GI;
			if ((mcs_known & IEEE80211_RADIOTAP_MCS_HAVE_FMT) &&
			    (mcs_flags & IEEE80211_RADIOTAP_MCS_FMT_GF))
				rspec |= RSPEC_SHORT_PREAMBLE;
			if ((mcs_known & IEEE80211_RADIOTAP_MCS_HAVE_FEC) &&
			    (mcs_flags & IEEE80211_RADIOTAP_MCS_FEC_LDPC))
				rspec |= RSPEC_LDPC_CODING;

			rspec |= override;
		}
	}

	return rspec;
}

bool
wl_rtapFlags(uint8 *rtap_header, uint8* flags)
{
	radiotap_parse_t rtap;
	void *p;

	wl_rtapParseInit(&rtap, rtap_header);

	p = wl_rtapParseFindField(&rtap, IEEE80211_RADIOTAP_FLAGS);
	if (p != NULL) {
		*flags = ((uint8*)p)[0];
	}

	return (p != NULL);
}

void
wl_rtapParseInit(radiotap_parse_t *rtap, uint8 *rtap_header)
{
	uint rlen;
	uint32 *present_word;
	struct ieee80211_radiotap_header *hdr = (struct ieee80211_radiotap_header*)rtap_header;

	memset(rtap, 0, sizeof(radiotap_parse_t));

	rlen = hdr->it_len; /* total space in rtap_header */

	/* If a precence word has the IEEE80211_RADIOTAP_EXT bit set it indicates
	 * that there is another precence word.
	 * Step over the presence words until we find the end of the list
	 */
	present_word = &hdr->it_present;
	/* remaining length in header past it_present */
	rlen -= sizeof(struct ieee80211_radiotap_header);

	while ((*present_word & (1<<IEEE80211_RADIOTAP_EXT)) && rlen >= 4) {
		present_word++;
		rlen -= 4;	/* account for 4 bytes of present_word */
	}

	rtap->hdr = hdr;
	rtap->fields = (uint8*)(present_word + 1);
	rtap->fields_len = rlen;
	wl_rtapParseReset(rtap);
}

static uint8 *
wl_radiotap_rx_addfield(uint8 *cp, int alignment, uint64 value)
{
	int i;
	int j = 0;
	cp = (uint8 *)(ALIGN_ADDR(cp, alignment));

	for (i = 0; i < alignment; i++) {
		*cp++ = (value >> j) & 0xff;
		j = j + 8;
	}
	return cp;
}


static uint8 *
wl_radiotap_rx_addfield_copyfrom(uint8 *cp, int alignment, const uint8 *from, int size)
{
	cp = (uint8 *)(ALIGN_ADDR(cp, alignment));

	bcopy(from, cp, size);
	cp += size;
	return cp;
}


uint
wl_radiotap_rx(struct dot11_header *mac_header,	wl_rxsts_t *rxsts, bsd_header_rx_t *bsd_header)
{
	int channel_frequency;
	u_int32_t channel_flags;
	u_int8_t flags;
	uint8 *cp;
	u_int32_t field_map;
	u_int16_t fc;
	uint bsd_header_len;
	u_int16_t ampdu_flags = 0;
	struct wl_radiotap_hdr *rtaphdr = &bsd_header->hdr;

	bzero((uint8 *)rtaphdr, sizeof(*rtaphdr));
	fc = LTOH16(mac_header->fc);
	field_map = WL_RADIOTAP_PRESENT_RX;

	if (CHSPEC_IS2G(rxsts->chanspec)) {
		channel_flags = IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN;
		channel_frequency = wf_channel2mhz(wf_chspec_ctlchan(rxsts->chanspec),
			WF_CHAN_FACTOR_2_4_G);
	} else {
		channel_flags = IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM;
		channel_frequency = wf_channel2mhz(wf_chspec_ctlchan(rxsts->chanspec),
			WF_CHAN_FACTOR_5_G);
	}
	WL_TMP(("%s %d: chanspec 0x%x ctlchan 0x%x freq: %d 0x%x \n", __FUNCTION__, __LINE__,
		rxsts->chanspec, wf_chspec_ctlchan(rxsts->chanspec), channel_frequency, fc));

	if (rxsts->nfrmtype == WL_RXS_NFRM_AMPDU_FIRST ||
		rxsts->nfrmtype == WL_RXS_NFRM_AMPDU_SUB) {

		ampdu_flags = IEEE80211_RADIOTAP_AMPDU_LAST_KNOWN;
	}

	flags = IEEE80211_RADIOTAP_F_FCS;

	if (rxsts->preamble == WL_RXS_PREAMBLE_SHORT)
		flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

	if ((fc &  FC_WEP) == FC_WEP)
		flags |= IEEE80211_RADIOTAP_F_WEP;

	if ((fc & FC_MOREFRAG) == FC_MOREFRAG)
		flags |= IEEE80211_RADIOTAP_F_FRAG;

	if (rxsts->pkterror & WL_RXS_CRC_ERROR)
		flags |= IEEE80211_RADIOTAP_F_BADFCS;

	if (rxsts->encoding == WL_RXS_ENCODING_HT)
		field_map = WL_RADIOTAP_PRESENT_RX_HT;
#ifdef WL11AC
	else if (rxsts->encoding == WL_RXS_ENCODING_VHT)
		field_map = WL_RADIOTAP_PRESENT_RX_VHT;

#endif /* WL11AC */

	/* Test for signal/noise values and update length and field bitmap */
	if (rxsts->signal == 0) {
		field_map &= ~(1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL);
	}

	if (rxsts->noise == 0) {
		field_map &= ~(1 << IEEE80211_RADIOTAP_DBM_ANTNOISE);
	}

	rtaphdr->ieee_radiotap.it_version = 0;
	rtaphdr->ieee_radiotap.it_pad = 0;
	rtaphdr->ieee_radiotap.it_present = HTOL32(field_map);

	cp = ((uint8*)(rtaphdr)) + sizeof(struct ieee80211_radiotap_header);
	cp = wl_radiotap_rx_addfield(cp, 8, (uint64)rxsts->mactime);
	cp = wl_radiotap_rx_addfield(cp, 1, flags);

	if (field_map & (1 << IEEE80211_RADIOTAP_RATE)) {
		/* bit  2: IEEE80211_RADIOTAP_RATE */
		cp = wl_radiotap_rx_addfield(cp, 1, (uint8)rxsts->datarate);
	 }
	/* bit  3: IEEE80211_RADIOTAP_CHANNEL */
	cp = wl_radiotap_rx_addfield(cp, 2, channel_frequency);
	cp = wl_radiotap_rx_addfield(cp, 2, channel_flags);

	if (field_map & (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL)) {
		/* bit  5: IEEE80211_RADIOTAP_DBM_ANTSIGNAL */
		cp = wl_radiotap_rx_addfield(cp, 1, (int8)rxsts->signal);
	}

	if (field_map & (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE)) {
		/* bit  6: IEEE80211_RADIOTAP_DBM_ANTNOISE */
		cp = wl_radiotap_rx_addfield(cp, 1, (int8)rxsts->noise);
	}

	if (field_map & (1 << IEEE80211_RADIOTAP_ANTENNA)) {
		/* bit 11: IEEE80211_RADIOTAP_ANTENNA */
		cp = wl_radiotap_rx_addfield(cp, 1, (int8)rxsts->antenna);
	}

	if (field_map & (1 << IEEE80211_RADIOTAP_XCHANNEL)) {
		/* bit 18: IEEE80211_RADIOTAP_XCHANNEL */

		struct wl_radiotap_xchan xc;

		/* Fill in XCHANNEL */
		if (CHSPEC_IS40(rxsts->chanspec)) {
			if (CHSPEC_SB_UPPER(rxsts->chanspec))
				channel_flags |= IEEE80211_CHAN_HT40D;
			else
				channel_flags |= IEEE80211_CHAN_HT40U;
		} else
			channel_flags |= IEEE80211_CHAN_HT20;

		xc.xchannel_flags = HTOL32(channel_flags);
		xc.xchannel_freq = HTOL16(channel_frequency);
		xc.xchannel_channel = wf_chspec_ctlchan(rxsts->chanspec);
		xc.xchannel_maxpower = (17*2);

		cp = wl_radiotap_rx_addfield_copyfrom(cp, 4, (const uint8*)&xc, sizeof(xc));
	}


	if (field_map & (1 << IEEE80211_RADIOTAP_MCS)) {
		/* bit 19: IEEE80211_RADIOTAP_MCS */

		struct wl_htmcs htmcs;
		htmcs.mcs_index = rxsts->mcs;
		htmcs.mcs_known = (IEEE80211_RADIOTAP_MCS_HAVE_BW |
			IEEE80211_RADIOTAP_MCS_HAVE_MCS |
			IEEE80211_RADIOTAP_MCS_HAVE_GI |
			IEEE80211_RADIOTAP_MCS_HAVE_FEC |
			IEEE80211_RADIOTAP_MCS_HAVE_FMT);
		htmcs.mcs_flags = 0;

		switch (rxsts->htflags & WL_RXS_HTF_BW_MASK) {
			case WL_RXS_HTF_20L:
				htmcs.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_20L;
				break;
			case WL_RXS_HTF_20U:
				htmcs.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_20U;
				break;
			case WL_RXS_HTF_40:
				htmcs.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_40;
				break;
			default:
				htmcs.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_20;
		}

		if (rxsts->htflags & WL_RXS_HTF_SGI)
			htmcs.mcs_flags |= IEEE80211_RADIOTAP_MCS_SGI;
		if (rxsts->preamble & WL_RXS_PREAMBLE_HT_GF)
			htmcs.mcs_flags |= IEEE80211_RADIOTAP_MCS_FMT_GF;
		if (rxsts->htflags & WL_RXS_HTF_LDPC)
			htmcs.mcs_flags |= IEEE80211_RADIOTAP_MCS_FEC_LDPC;

		cp = wl_radiotap_rx_addfield_copyfrom(cp, 1, (const uint8*)&htmcs, sizeof(htmcs));
	}


	if (field_map & (1 << IEEE80211_RADIOTAP_AMPDU)) {

		/* bit 20: IEEE80211_RADIOTAP_AMPDU */
		struct wl_radiotap_ampdu ampdu;

		/* fill in A-mpdu Status */
		ampdu.ref_num = HTOL32(mac_header->seq >> 4);
		ampdu.flags = HTOL16(ampdu_flags);
		ampdu.delimiter_crc = 0;
		ampdu.reserved = 0;

		cp = wl_radiotap_rx_addfield_copyfrom(cp, 4, (const uint8*)&ampdu, sizeof(ampdu));
	}

	if (field_map & (1 << IEEE80211_RADIOTAP_VHT))
	{
		/* bit 21: IEEE80211_RADIOTAP_ */

		struct wl_vhtmcs vhtmcs;

		vhtmcs.vht_known = (IEEE80211_RADIOTAP_VHT_HAVE_STBC |
			IEEE80211_RADIOTAP_VHT_HAVE_TXOP_PS |
			IEEE80211_RADIOTAP_VHT_HAVE_GI |
			IEEE80211_RADIOTAP_VHT_HAVE_SGI_NSYM_DA |
			IEEE80211_RADIOTAP_VHT_HAVE_LDPC_EXTRA |
			IEEE80211_RADIOTAP_VHT_HAVE_BF |
			IEEE80211_RADIOTAP_VHT_HAVE_BW |
			IEEE80211_RADIOTAP_VHT_HAVE_GID |
			IEEE80211_RADIOTAP_VHT_HAVE_PAID);

		vhtmcs.vht_flags = HTOL16(rxsts->vhtflags);

		switch (rxsts->bw) {
			case WL_RXS_VHT_BW_20:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20;
				break;
			case WL_RXS_VHT_BW_40:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_40;
				break;
			case WL_RXS_VHT_BW_20L:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20L;
				break;
			case WL_RXS_VHT_BW_20U:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20U;
				break;
			case WL_RXS_VHT_BW_80:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_80;
				break;
			case WL_RXS_VHT_BW_40L:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_40L;
				break;
			case WL_RXS_VHT_BW_40U:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_40U;
				break;
			case WL_RXS_VHT_BW_20LL:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20LL;
				break;
			case WL_RXS_VHT_BW_20LU:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20LU;
				break;
			case WL_RXS_VHT_BW_20UL:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20UL;
				break;
			case WL_RXS_VHT_BW_20UU:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20UU;
				break;
			default:
				vhtmcs.vht_bw = IEEE80211_RADIOTAP_VHT_BW_20;
			break;
		}

		vhtmcs.vht_mcs_nss[0] = (rxsts->mcs << 4) |
			(rxsts->nss & IEEE80211_RADIOTAP_VHT_NSS);
		vhtmcs.vht_mcs_nss[1] = 0;
		vhtmcs.vht_mcs_nss[2] = 0;
		vhtmcs.vht_mcs_nss[3] = 0;

		vhtmcs.vht_coding = rxsts->coding;
		vhtmcs.vht_group_id = rxsts->gid;
		vhtmcs.vht_partial_aid = HTOL16(rxsts->aid);

		cp = wl_radiotap_rx_addfield_copyfrom(cp, 2, (const uint8*)&vhtmcs, sizeof(vhtmcs));
	}

	bsd_header_len = (cp - (uint8*)bsd_header);

	 /* Adjust header */
	 rtaphdr->ieee_radiotap.it_len = HTOL16(bsd_header_len);

	 return bsd_header_len;

}

uint
wl_radiotap_tx(struct dot11_header *mac_header,	wl_txsts_t *txsts, bsd_header_tx_t *bsd_header)
{
	int channel_frequency;
	uint32 channel_flags;
	uint8 flags;
	uint16 fc = LTOH16(mac_header->fc);
	uint bsd_header_len;

	if (CHSPEC_IS2G(txsts->chanspec)) {
		channel_flags = IEEE80211_CHAN_2GHZ;
		channel_frequency = wf_channel2mhz(wf_chspec_ctlchan(txsts->chanspec),
			WF_CHAN_FACTOR_2_4_G);
	} else {
		channel_flags = IEEE80211_CHAN_5GHZ;
		channel_frequency = wf_channel2mhz(wf_chspec_ctlchan(txsts->chanspec),
			WF_CHAN_FACTOR_5_G);
	}
	WL_TMP(("%s %d: chanspec 0x%x ctlchan 0x%x freq: %d 0x%x \n", __FUNCTION__, __LINE__,
		txsts->chanspec, wf_chspec_ctlchan(txsts->chanspec), channel_frequency, fc));

	flags = 0;

	if (txsts->preamble == WL_RXS_PREAMBLE_SHORT)
		flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

	if ((fc &  FC_WEP) == FC_WEP)
		flags |= IEEE80211_RADIOTAP_F_WEP;

	if ((fc & FC_MOREFRAG) == FC_MOREFRAG)
		flags |= IEEE80211_RADIOTAP_F_FRAG;

	if (txsts->datarate != 0) {
		u_int32_t field_map = WL_RADIOTAP_PRESENT_TX;
		struct wl_radiotap_hdr_tx *rtl = &bsd_header->hdr;

		/*
		 * Header length is complicated due to dynamic presence of signal and noise fields
		 * Start with length of wl_radiotap_legacy plus signal/noise/ant
		 */
		bsd_header_len = sizeof(struct wl_radiotap_hdr_tx) + 3;
		bzero((uint8 *)rtl, sizeof(bsd_header));

		rtl->ieee_radiotap.it_version = 0;
		rtl->ieee_radiotap.it_pad = 0;
		rtl->ieee_radiotap.it_len = HTOL16(bsd_header_len);
		rtl->ieee_radiotap.it_present = HTOL32(field_map);

		rtl->tsft = HTOL64((uint64)txsts->mactime);
		rtl->flags = flags;
		rtl->u.rate = txsts->datarate;
		rtl->channel_freq = HTOL16(channel_frequency);
		rtl->channel_flags = HTOL16(channel_flags);
	} else {
		u_int32_t field_map = WL_RADIOTAP_PRESENT_HT_TX;
		struct wl_radiotap_hdr_tx *rtht = &bsd_header->hdr;
		struct wl_radiotap_ht_tail *tail;
		uint pad_len;

		/*
		 * Header length is complicated due to dynamic presence of signal and noise fields
		 * and padding for xchannel following signal/noise/ant.
		 * Start with length of wl_radiotap_ht plus signal/noise/ant
		 */
		bsd_header_len = sizeof(struct wl_radiotap_hdr_tx);

		/* calc pad for xchannel field */
		pad_len = 3;

		/* add the length of the tail end of the structure */
		bsd_header_len += pad_len + sizeof(struct wl_radiotap_ht_tail);
		bzero((uint8 *)rtht, sizeof(*rtht));

		rtht->ieee_radiotap.it_version = 0;
		rtht->ieee_radiotap.it_pad = 0;
		rtht->ieee_radiotap.it_len = HTOL16(bsd_header_len);
		rtht->ieee_radiotap.it_present = HTOL32(field_map);

		rtht->tsft = HTOL64((uint64)txsts->mactime);
		rtht->flags = flags;
		rtht->u.pad = 0;
		rtht->channel_freq = HTOL16(channel_frequency);
		rtht->channel_flags = HTOL16(channel_flags);

		rtht->txflags = HTOL16(txsts->txflags);
		rtht->retries = txsts->retries;

		tail = (struct wl_radiotap_ht_tail *)(bsd_header->ht + pad_len);
		tail = (struct wl_radiotap_ht_tail*)bsd_header->ht;

		/* Fill in XCHANNEL */
		if (CHSPEC_IS40(txsts->chanspec)) {
			if (CHSPEC_SB_UPPER(txsts->chanspec))
				channel_flags |= IEEE80211_CHAN_HT40D;
			else
				channel_flags |= IEEE80211_CHAN_HT40U;
		} else
			channel_flags |= IEEE80211_CHAN_HT20;

		tail->xc.xchannel_flags = HTOL32(channel_flags);
		tail->xc.xchannel_freq = HTOL16(channel_frequency);
		tail->xc.xchannel_channel = wf_chspec_ctlchan(txsts->chanspec);
		tail->xc.xchannel_maxpower = (17*2);

		tail->u.ht.mcs_index = txsts->mcs;
		tail->u.ht.mcs_known = (IEEE80211_RADIOTAP_MCS_HAVE_MCS |
		                        IEEE80211_RADIOTAP_MCS_HAVE_BW |
		                        IEEE80211_RADIOTAP_MCS_HAVE_GI |
		                        IEEE80211_RADIOTAP_MCS_HAVE_FMT |
		                        IEEE80211_RADIOTAP_MCS_HAVE_FEC);
		tail->u.ht.mcs_flags = 0;
		if (txsts->htflags & WL_RXS_HTF_40) {
			tail->u.ht.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_40;
		} else if (CHSPEC_IS40(txsts->chanspec)) {
			if (CHSPEC_SB_UPPER(txsts->chanspec))
				tail->u.ht.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_20U;
			else
				tail->u.ht.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_20L;
		} else
			tail->u.ht.mcs_flags |= IEEE80211_RADIOTAP_MCS_BW_20;

		if (txsts->htflags & WL_RXS_HTF_SGI)
			tail->u.ht.mcs_flags |= IEEE80211_RADIOTAP_MCS_SGI;

		if (txsts->preamble & WL_RXS_PREAMBLE_HT_GF)
			tail->u.ht.mcs_flags |= IEEE80211_RADIOTAP_MCS_FMT_GF;

		if (txsts->htflags & WL_RXS_HTF_LDPC)
			tail->u.ht.mcs_flags |= IEEE80211_RADIOTAP_MCS_FEC_LDPC;
	}
	return bsd_header_len;
}
