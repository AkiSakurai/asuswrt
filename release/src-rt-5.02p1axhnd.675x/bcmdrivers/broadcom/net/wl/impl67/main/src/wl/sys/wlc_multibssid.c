/*
 * Multiple BSSID implementation
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
 * $Id: wlc_multibssid.c 791207 2020-09-21 04:00:08Z $
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <802.1d.h>
#include <802.11.h>
#include <802.11e.h>
#ifdef WL11AX
#include <802.11ax.h>
#endif /* WL11AX */
#include <wlioctl.h>
#include <epivers.h>
#include <d11_cfg.h>
#include <wlc_pub.h>
#include <wlc_interfere.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_phy_shim.h>
#include <wlc_vht.h>
#include <wlc_addrmatch.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_helper.h>
#include <wlc_ie_reg.h>
#include <wlc_akm_ie.h>
#include <wlc_ht.h>
#include <wlc_he.h>
#include <wlc_multibssid.h>
#include <wlc_assoc.h>
#include <wlc_rx.h>
#include <wlc_mbss.h>

static uint8 *wlc_multibssid_locate_profile(wlc_info_t *wlc,
  dot11_mbssid_ie_t *mbssid_ie, uint8 current_cnt, uint8 *prof_buf_len);
static uint8 *wlc_multibssid_match_bcnie_in_nontransprofile(wlc_info_t *wlc,
  wlc_iem_tag_t bcn_ie_tag, uint8 *prof_buf, uint8 prof_buf_len, uint32 *prof_ies_bitmap);
static int wlc_multibssid_update_bcn(wlc_info_t *wlc,
  struct dot11_management_header *hdr, uint8 *body, uint body_len,
  uint8 *prof_buf, uint8 prof_buf_len, uint8 new_bssid_lsb);

/* module attach/detach interfaces */
wlc_multibssid_info_t *
BCMATTACHFN(wlc_multibssid_attach)(wlc_info_t *wlc)
{
	wlc_multibssid_info_t *mbssid_i;

	/* allocate module info */
	if ((mbssid_i = MALLOCZ(wlc->osh, sizeof(*mbssid_i))) == NULL) {
		WL_SCAN_ERROR(("wl%d: wlc_multibssid_attach: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		goto fail;
	}
	mbssid_i->wlc = wlc;
#if defined(WL_MBSSID)
	mbssid_i->wlc->pub->cmn->_multibssid = TRUE;
#endif /* WL_MBSSID */

	return mbssid_i;

fail:
	MODULE_DETACH(mbssid_i, wlc_multibssid_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_multibssid_detach)(wlc_multibssid_info_t *mbssid_i)
{
	wlc_info_t *wlc;

	if (mbssid_i == NULL)
		return;

	wlc = mbssid_i->wlc;

	if (wlc->primary_bsscfg) {
		/* clear ext_cap 22-th bit for multiple BSSID support */
		wlc_bsscfg_set_ext_cap(wlc->primary_bsscfg, DOT11_EXT_CAP_MBSSID, FALSE);
	}
	MFREE(wlc->osh, mbssid_i, sizeof(*mbssid_i));
}

/* set all multiple BSSID related configuration to uCode */
void
wlc_multibssid_config(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 attr)
{
	wlc_bss_info_t *current_bss = cfg->current_bss;
	uint16 prev_amtinfo;

	ASSERT(BSSCFG_STA(cfg));
	if (!current_bss->max_mbssid_indicator) {
		return;
	}
	if (current_bss->mbssid_index) {
		/* set transmitted BSSID only when
		 * STA is associated to nontransmitted BSSID
		 */
		prev_amtinfo = wlc_read_amtinfo_by_idx(wlc, cfg->mbssid_transbss_amtidx);
		wlc_write_amtinfo_by_idx(wlc, cfg->mbssid_transbss_amtidx,
		                         (prev_amtinfo | AMTINFO_BMP_BSSID));
		wlc_set_addrmatch(wlc, cfg->mbssid_transbss_amtidx,
		                  &current_bss->mbssid_transmitted, attr);
	}
}

/* calculate the total number of nontranmitted BSSID
 * profiles in one multiple BSSID IE
 */
uint8
wlc_multibssid_caltotal(wlc_info_t *wlc, wlc_bsscfg_t *cfg, dot11_mbssid_ie_t *mbssid_ie)
{
	int profiles_total_len, prof_subie_len;
	uint8 profiles_cnt = 0, *profile_subie;

	ASSERT(mbssid_ie);

	profiles_total_len = mbssid_ie->len - 1;
	profile_subie = (uint8 *)&mbssid_ie->profile;
	prof_subie_len = (int)mbssid_ie->profile->subie_len + TLV_HDR_LEN;

	while (profiles_total_len >= prof_subie_len) {
		profiles_cnt++;
		profiles_total_len -= prof_subie_len;
		profile_subie += prof_subie_len;
		prof_subie_len = profile_subie[TLV_LEN_OFF] + TLV_HDR_LEN;
	}

	return profiles_cnt;
}

int
wlc_multibssid_handling_eiscan(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	struct dot11_management_header *hdr, uint8 current_cnt, dot11_mbssid_ie_t *mbssid_ie)
{
	uint8 *prof_buf, prof_buf_len;
	uint16 fc, ft;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;
	/* Since HE Cap IE and HE Operation IE won't appear in
	 * nontransmitted BSSID profile,
	 * WLC_BSS3_HE bit setting should be recorded here
	 */
	bool he = bi->flags3 & WLC_BSS3_HE;

	ASSERT(mbssid_ie);

	prof_buf = wlc_multibssid_locate_profile(wlc, mbssid_ie, current_cnt, &prof_buf_len);

	/* prepare IE mgmt calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.scan.result = bi;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;

	fc = ltoh16(hdr->fc);
	ft = (fc == FC_BEACON) ? WLC_IEM_FC_SCAN_BCN : WLC_IEM_FC_SCAN_PRBRSP;

	/* parse IEs in this nontransmitted BSSID profile */
	(void)wlc_iem_parse_frame(wlc->iemi, cfg, ft, &upp, &pparm, prof_buf, prof_buf_len);
	/* WLC_BSS3_HE would be cleared after the above IE parsing
	 * restore it when it is set
	 */
	if (he) {
		bi->flags3 |= WLC_BSS3_HE;
	}

	return BCME_OK;
}

/* locate the nontransmitted BSSID profile
 * corresponding to current_cnt in this mbssid_ie
 */
static uint8 *
wlc_multibssid_locate_profile(wlc_info_t *wlc, dot11_mbssid_ie_t *mbssid_ie,
  uint8 current_cnt, uint8 *prof_buf_len)
{
	uint8 *prof_buf, i;
	int allprofiles_len;

	ASSERT(mbssid_ie);

	prof_buf = (uint8 *)&mbssid_ie->profile->moreie;
	*prof_buf_len = mbssid_ie->profile->subie_len;
	allprofiles_len = mbssid_ie->len - 1 - TLV_HDR_LEN;
	for (i = 0; i < current_cnt; i++) {
		allprofiles_len -= *prof_buf_len + TLV_HDR_LEN;
		prof_buf += *prof_buf_len;
		*prof_buf_len = prof_buf[TLV_LEN_OFF];
		prof_buf += TLV_HDR_LEN;
		ASSERT(allprofiles_len >= *prof_buf_len);
	}

	return prof_buf;
}

/* With a given multiple BSSID IE and current_cnt,
 * get ssid ie and nontransmitted BSSID for this nontransmitted BSSID profile
 */
bcm_tlv_t *
wlc_multibssid_recv_nontransmitted_ssid_bssid(wlc_info_t *wlc, struct dot11_management_header *hdr,
  dot11_mbssid_ie_t *mbssid_ie, uint8 current_cnt, struct ether_addr *cmpbssid)

{
	uint8 *prof_buf, prof_buf_len, lsb_bits, mbssid_index, mbssid_n;
	bcm_tlv_t *mbssid_index_ie, *ssid_ie;

	ASSERT(mbssid_ie);

	mbssid_n = mbssid_ie->maxbssid_indicator;

	prof_buf = wlc_multibssid_locate_profile(wlc, mbssid_ie, current_cnt, &prof_buf_len);

	/* get multiple BSSID index IE and SSID IE in this profile */
	mbssid_index_ie = bcm_parse_tlvs(prof_buf,
	        prof_buf_len, DOT11_MNG_MULTIPLE_BSSID_IDX_ID);
	ssid_ie = bcm_parse_tlvs(prof_buf, prof_buf_len, DOT11_MNG_SSID_ID);

	if (mbssid_index_ie) {
		/* get BSSID for nontransmitted BSSID profile */
		mbssid_index = mbssid_index_ie->data[0];
		lsb_bits =  (hdr->bssid.octet[ETHER_ADDR_LEN - 1] +
		             mbssid_index) & NBITMASK(mbssid_n);
		cmpbssid->octet[ETHER_ADDR_LEN - 1] =
		 (cmpbssid->octet[ETHER_ADDR_LEN - 1] & ~(NBITMASK(mbssid_n))) | lsb_bits;

		return ssid_ie;
	}

	return NULL;
}

int
wlc_multibssid_recv_preprocess_bcn(wlc_info_t *wlc, bool assoc_scan,
   struct dot11_management_header *hdr, uint8 *body, uint body_len,
   dot11_mbssid_ie_t *mbssid_ie, uint8 current_cnt, uint8 lastbyte)
{
	uint8 *prof_buf = NULL, prof_buf_len = 0;
	bcm_tlv_t *mbssid_index_ie;

	ASSERT(mbssid_ie);

	/* record the matched multiple BSSID information,
	 * for association scan only
	 */
	if (assoc_scan) {
		wlc->mbssid_i->maxbssid_indicator = mbssid_ie->maxbssid_indicator;
		wlc->mbssid_i->multibssid_index = 0;
	}

	if (current_cnt) {
		/* matched the nontransmitted BSSID case */
		prof_buf = wlc_multibssid_locate_profile(wlc, mbssid_ie,
		            current_cnt - 1, &prof_buf_len);

		mbssid_index_ie = bcm_parse_tlvs(prof_buf, prof_buf_len,
		                  DOT11_MNG_MULTIPLE_BSSID_IDX_ID);
		if (!mbssid_index_ie) {
			WL_SCAN_ERROR(("Wrong! No MBSSID index IE!\n"));
			return BCME_ERROR;
		}

		if (assoc_scan) {
			wlc->mbssid_i->multibssid_index = mbssid_index_ie->data[0];
			(void)memcpy(&wlc->mbssid_i->mbssid_transmitted,
			      &hdr->bssid, sizeof(struct ether_addr));
		}

		/* matched one of the nontransmitted BSSID profiles */
		/* integrate the matched nontransmitted BSSID profile
		 * into beacon and generate a new/legacy beacon frame
		 * without multiple BSSID IE
		 * hdr->bssid is still transmitted BSSID,
		 * nontransmitted BSSID will be derived
		 * from multiple BSSID index
		 */
		return wlc_multibssid_update_bcn(wlc, hdr,
		    body, body_len, prof_buf, prof_buf_len, lastbyte);
	} else { /* matched the transmitted BSSID case */
		return wlc_multibssid_update_bcn(wlc, hdr,
		    body, body_len, NULL, 0, 0);
	}
}

/* add IEs in nontransmitted BSSID profile but not in beacon main body to beacon */
static void
wlc_multibssid_addbcn_profile_ies(wlc_info_t *wlc, uint8 *prof_buf, int prof_buf_len,
	uint32 prof_ies_bitmap, uint8 *ies_recreated, uint *ies_recreated_len, uint body_len)
{
	uint tag_len;

	/* move to the bottom of current new beacon IEs */
	ies_recreated += *ies_recreated_len;
	/* check if some IEs only present in profile, not in beacon main IEs */
	while (prof_ies_bitmap) {
		tag_len = TLV_HDR_LEN + prof_buf[TLV_LEN_OFF];

		if (prof_buf_len < (int)tag_len) {
			WL_SCAN_ERROR(("bad nontransmitted BSSID profile:%d, %d\n",
			                prof_buf_len, tag_len));
			return;
		}

		if ((prof_buf[TLV_TAG_OFF] == DOT11_MNG_NONTRANS_BSSID_CAP_ID) ||
		    (prof_buf[TLV_TAG_OFF] == DOT11_MNG_MULTIPLE_BSSID_IDX_ID)) {
			goto next_ie;
		}
		if (prof_ies_bitmap & 1) {
			if (tag_len > (body_len - DOT11_BCN_PRB_LEN - *ies_recreated_len)) {
				WL_SCAN_ERROR(("No space adding profile ies to bcn: %d, %d\n",
				  tag_len, (body_len - DOT11_BCN_PRB_LEN - *ies_recreated_len)));
				return;
			}
			/* copy prof_ie to new beacon */
			(void)memcpy(ies_recreated, prof_buf, tag_len);
			ies_recreated += tag_len;
			*ies_recreated_len += tag_len;
		}
		/* move to the next IE in profile */
		prof_ies_bitmap >>= 1;
next_ie:
		prof_buf += tag_len;
		prof_buf_len -= tag_len;
	}
}

static uint32
wlc_multibssid_profie_bitmap(wlc_info_t *wlc, uint8 *prof_ies, int prof_ies_len)
{
	uint tag_len, bitmap = 1;
	wlc_iem_tag_t ie_tag;

	if (!prof_ies) {
	    /* no profile ies */
		return 0;
	}

	/* find how many ies in prof_buf */
	tag_len = TLV_HDR_LEN + prof_ies[TLV_LEN_OFF];
	while ((prof_ies_len > 0) && (prof_ies_len >= (int)tag_len)) {
		ie_tag = wlc_iem_get_id(prof_ies);
		if ((ie_tag != (wlc_iem_tag_t)DOT11_MNG_NONTRANS_BSSID_CAP_ID) &&
			(ie_tag != (wlc_iem_tag_t)DOT11_MNG_MULTIPLE_BSSID_IDX_ID)) {
			bitmap <<= 1;
		}

		/* next IE */
		prof_ies += tag_len;
		prof_ies_len -= (int)tag_len;
		tag_len = TLV_HDR_LEN + prof_ies[TLV_LEN_OFF];
	}

	if (!bitmap) {
		WL_SCAN_ERROR(("Can't support more than 32 tags\n"));
		return 0;
	}

	return (bitmap - 1);
}

/* overwrite some fileds/IEs in beacon frame body with
 * the specified transmitted/nontransmitted BSSID profile prof_buf
 */
static int
wlc_multibssid_update_bcn(wlc_info_t *wlc,
	struct dot11_management_header *hdr, uint8 *body, uint body_len,
	uint8 *prof_buf, uint8 prof_buf_len, uint8 new_bssid_lsb)
{
	uint8 *tags, *prof_ie = NULL;
	uint tag_len = 0, ies_recreated_len;
	int tags_len;
	wlc_iem_tag_t ie_tag;
	struct dot11_bcn_prb *bcn = (struct dot11_bcn_prb *)body;
	dot11_mbssid_cap_t *mbssid_cap_ie;
	uint8 *ies_recreated, *ies;
	uint32 prof_ies_bitmap;

	ies_recreated = (uint8 *)MALLOCZ(wlc->osh, body_len - DOT11_BCN_PRB_LEN);
	if (!ies_recreated) {
		WL_SCAN_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc),
			"wlc_multibssid_update_bcn",
			body_len - DOT11_BCN_PRB_LEN, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	/* update BSSID in header
	 * hdr is NULL when restructuring attached ies for wl_bss_info
	 */
	if (hdr && prof_buf) {
		hdr->bssid.octet[ETHER_ADDR_LEN - 1] = new_bssid_lsb;
		hdr->sa.octet[ETHER_ADDR_LEN - 1] = new_bssid_lsb;
	}

	if (prof_buf) {
		/* update fixed fields, capability only */
		mbssid_cap_ie = (dot11_mbssid_cap_t *)bcm_parse_tlvs(prof_buf,
		                prof_buf_len, DOT11_MNG_NONTRANS_BSSID_CAP_ID);
		ASSERT(mbssid_cap_ie);
		if (!mbssid_cap_ie) {
			WL_SCAN_ERROR(("Wrong! MBSSID profile has no cap IE!\n"));
			return BCME_ERROR;
		}
		bcn->capability = mbssid_cap_ie->capability;
	}
	/* update ies */
	tags = body + DOT11_BCN_PRB_LEN;
	tags_len = (int)(body_len - DOT11_BCN_PRB_LEN);

	ies_recreated_len = 0;
	ies = ies_recreated;
	prof_ies_bitmap = wlc_multibssid_profie_bitmap(wlc, prof_buf, prof_buf_len);
	if ((!prof_ies_bitmap) && (prof_buf)) {
		WL_SCAN_ERROR(("Invalid profile\n"));
		return BCME_BADARG;
	}

	for (; tags_len >= (int)(TLV_HDR_LEN + tags[TLV_LEN_OFF]);
	       tags += tag_len, tags_len -= (int)tag_len) {
		ie_tag = wlc_iem_get_id(tags);
		tag_len = TLV_HDR_LEN + tags[TLV_LEN_OFF];

		/* skip copying multiple BSSID IE */
		if (ie_tag == (wlc_iem_tag_t)DOT11_MNG_MULTIPLE_BSSID_ID) {
			continue;
		}

		if (prof_buf) {
			prof_ie = wlc_multibssid_match_bcnie_in_nontransprofile(wlc, ie_tag,
			            prof_buf, prof_buf_len, &prof_ies_bitmap);
		}
		if (prof_ie) { /* use the one in nontransmitted profile */
			uint8 prof_ie_len = TLV_HDR_LEN + prof_ie[TLV_LEN_OFF];
			if (prof_ie_len >
			    (body_len - DOT11_BCN_PRB_LEN - ies_recreated_len)) {
				WL_SCAN_ERROR(("No enough space in ies_recreated:%d, %d\n",
				  prof_ie_len,
				  (body_len - DOT11_BCN_PRB_LEN - (uint)(ies - ies_recreated))));
				return BCME_ERROR;
			}
			(void)memcpy(ies, prof_ie, prof_ie_len);
			ies += prof_ie_len;
			ies_recreated_len += prof_ie_len;
		} else { /* use the original */
			(void)memcpy(ies, tags, tag_len);
			ies += tag_len;
			ies_recreated_len += tag_len;
		}
	}

	if (prof_ies_bitmap) {
		/* some IEs in profile, but not in beacon main body */
		wlc_multibssid_addbcn_profile_ies(wlc, prof_buf, prof_buf_len,
		              prof_ies_bitmap, ies_recreated, &ies_recreated_len, body_len);
	}
	/* overwrite the old beacon */
	(void)memset(body + DOT11_BCN_PRB_LEN,
	          0, body_len - DOT11_BCN_PRB_LEN);
	(void)memcpy(body + DOT11_BCN_PRB_LEN, ies_recreated, ies_recreated_len);
	/* free dynamically allocated memory */
	MFREE(wlc->osh, ies_recreated, body_len - DOT11_BCN_PRB_LEN);

	return ies_recreated_len + DOT11_BCN_PRB_LEN;
}

/* check if one ie tag in beacon also shows
 * in the specified nontransmitted BSSID profile prof_buf
 */
static uint8 *
wlc_multibssid_match_bcnie_in_nontransprofile(wlc_info_t *wlc,
	wlc_iem_tag_t bcn_ie_tag, uint8 *prof_buf, uint8 prof_buf_len, uint32 *prof_ies_bitmap)
{
	uint8 *tags = prof_buf;
	int tags_len = (int)prof_buf_len;
	wlc_iem_tag_t ie_tag;
	uint8 tag_len, cnt = 0;

	while (tags_len >= (int)(TLV_HDR_LEN + tags[TLV_LEN_OFF])) {
		ie_tag = wlc_iem_get_id(tags);
		tag_len = TLV_HDR_LEN + tags[TLV_LEN_OFF];

		if (bcn_ie_tag == ie_tag) {
			*prof_ies_bitmap &= ~(1 << cnt);
			return tags;
		}

		/* next IE */
		tags += tag_len;
		tags_len -= (int)tag_len;
		if ((ie_tag != DOT11_MNG_NONTRANS_BSSID_CAP_ID) &&
			(ie_tag != DOT11_MNG_MULTIPLE_BSSID_IDX_ID)) {
			cnt++;
		}
	}

	return NULL;
}

static int
wlc_multibssid_convbcn_for_nontransbssid(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
  struct dot11_management_header *hdr, uint8 *body,
  uint body_len, uint8 bssid_lastbyte, uint8 prof_index)
{
	/* put nontransmitted BSSID profile into beacon and form a legacy one */
	uint8 *prof_buf;
	uint8 prof_buf_len, prof_total_len;
	bcm_tlv_t *mbssid_index_ie;
	dot11_mbssid_ie_t *mbssid_ie;
	uint8 *newbdy = body;
	uint newlen = 0;

	mbssid_ie = (dot11_mbssid_ie_t *)bcm_parse_tlvs(body + DOT11_BCN_PRB_LEN,
	             body_len - DOT11_BCN_PRB_LEN, DOT11_MNG_MULTIPLE_BSSID_ID);
	ASSERT(wlc_multibssid_valid_mbssidie(mbssid_ie));

	/* find the nontransmitted BSSID profile that matches
	 * the multiple BSSID index(prof_index) among all
	 * nontransmitted BSSID profiles in all multiple BSSID IEs
	 */
	 while (wlc_multibssid_valid_mbssidie(mbssid_ie)) {
		prof_total_len = mbssid_ie->len - 1 - TLV_HDR_LEN;
		prof_buf = (uint8 *)&mbssid_ie->profile->moreie;
		prof_buf_len = mbssid_ie->profile->subie_len;

		/* search all profiles in this mbssid IE */
		while (prof_total_len >= prof_buf_len) {
			mbssid_index_ie = bcm_parse_tlvs(prof_buf, prof_buf_len,
			                  DOT11_MNG_MULTIPLE_BSSID_IDX_ID);

			if (mbssid_index_ie &&
			    ((mbssid_index_ie->len == MULTIBSSID_IDX_IE_LEN1) ||
			     (mbssid_index_ie->len == MULTIBSSID_IDX_IE_LEN3)) &&
				(prof_index == mbssid_index_ie->data[0])) {
				/* found matched BSSID index
				 * so locate right prof_buf and prof_buf_len
				 * update beacon with IEs in this nontrans BSSID profile
				 */
				return wlc_multibssid_update_bcn(wlc, hdr, body, body_len,
				                   prof_buf, prof_buf_len, bssid_lastbyte);

			}

			if (prof_total_len > prof_buf_len + TLV_HDR_LEN) {
				/* move to the next profile */
				prof_total_len -= prof_buf_len + TLV_HDR_LEN;
				prof_buf += prof_buf_len;
				prof_buf_len = prof_buf[TLV_LEN_OFF];
				prof_buf += TLV_HDR_LEN;
			} else {
				/* no more valid profiles in this mbssid IE */
				break;
			}
		}

		/* look for the next mbssid IE */
		newbdy = (uint8 *)mbssid_ie + TLV_HDR_LEN + mbssid_ie->len;
		newlen = body_len - ((uint)((uint8 *)mbssid_ie - body) +
		                      TLV_HDR_LEN + mbssid_ie->len);
		ASSERT(newlen < (body_len - DOT11_BCN_PRB_LEN));
		mbssid_ie = (dot11_mbssid_ie_t *)bcm_parse_tlvs(newbdy,
		                  newlen, DOT11_MNG_MULTIPLE_BSSID_ID);
	}

	/* no matching multiple BSSID index found,
	 * don't convert this beacon
	 */
	return BCME_OK;
}

/* when associated to nontransmitted BSSID,
 * convert each beacon received from transmitted BSSID to
 * the updated regular beacon format to avoid issues in further processing
 */
void
wlc_multibssid_convertbcn_4associated_nontransbssid(wlc_info_t *wlc,
  struct dot11_management_header *hdr, uint16 fc, uint16 fk,
  const d11rxhdr_t *rxh, uint len_mpdu)
{
	wlc_bsscfg_t *cfg;
	uint body_len;
	uint8 *body;

	/* convert beacon and probe response only
	 * and only when multiple BSSID feature is enabled
	 */
	if (!wlc || !hdr || !rxh ||
	    !MULTIBSSID_ENAB(wlc->pub) || (fk != FC_BEACON)) {
		return;
	}

	/* convert only when associated to nontransmitted BSSID */
	cfg = wlc_bsscfg_find_by_current_mbssid(wlc, &hdr->bssid);
	if (!cfg) {
		return;
	}

	/* set body pointer to fixed field */
	body = (uint8 *)hdr + DOT11_A3_HDR_LEN;
	body_len = len_mpdu - DOT11_A3_HDR_LEN;
	(void)wlc_multibssid_convbcn_for_nontransbssid(wlc, cfg, hdr, body, body_len,
	      cfg->BSSID.octet[ETHER_ADDR_LEN - 1], cfg->current_bss->mbssid_index);

	/* convert bssid and sa in header */
	hdr->bssid.octet[ETHER_ADDR_LEN - 1] = cfg->BSSID.octet[ETHER_ADDR_LEN - 1];
	hdr->sa.octet[ETHER_ADDR_LEN - 1] = cfg->BSSID.octet[ETHER_ADDR_LEN - 1];
}

int
wlc_multibssid_duplicate_bi(wlc_info_t *wlc, wlc_bss_info_t *bi_dst, wlc_bss_info_t *bi_src)
{
		(void)memcpy(bi_dst, bi_src, sizeof(wlc_bss_info_t));
		bi_dst->bcn_prb = MALLOCZ(wlc->osh, bi_src->bcn_prb_len);
		if (!bi_dst->bcn_prb) {
			WL_SCAN_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc),
				"wlc_multibssid_duplicate_bi failed",
				bi_src->bcn_prb_len, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}

		(void)memcpy(bi_dst->bcn_prb, bi_src->bcn_prb, bi_src->bcn_prb_len);
		return BCME_OK;
}

void
wlc_multibssid_overwrite_bi(wlc_info_t *wlc,  wlc_bss_info_t *bi_dst, wlc_bss_info_t *bi_src)
{
	struct dot11_bcn_prb *bcn_prb;

	bcn_prb = bi_dst->bcn_prb;
	(void)memcpy(bi_dst, bi_src, sizeof(wlc_bss_info_t));
	bi_dst->bcn_prb = bcn_prb;
	(void)memcpy(bi_dst->bcn_prb,
	         bi_src->bcn_prb, bi_src->bcn_prb_len);
	bi_dst->bcn_prb_len = bi_src->bcn_prb_len;
}

void
wlc_multibssid_cpymbssid_setting(wlc_info_t *wlc, wlc_bss_info_t *BSS, wlc_bsscfg_t *cfg)
{
	if (!wlc->mbssid_i->maxbssid_indicator) {
		return;
	}

	BSS->max_mbssid_indicator = wlc->mbssid_i->maxbssid_indicator;
	BSS->mbssid_index = wlc->mbssid_i->multibssid_index;
	(void)memcpy(&BSS->mbssid_transmitted,
	         &wlc->mbssid_i->mbssid_transmitted, sizeof(struct ether_addr));

	/* ext_cap 22-th bit for multiple BSSID support */
	wlc_bsscfg_set_ext_cap(wlc->primary_bsscfg, DOT11_EXT_CAP_MBSSID,
		MULTIBSSID_ENAB(wlc->pub));

	/* reset the configuration in wlc->mbssid_i */
	wlc->mbssid_i->maxbssid_indicator = 0;
}

/* restore bi to the original,
 * update bi and bi->bcn_prb with IEs
 * in nontransmitted BSSID profile,
 */
void
wlc_multibssid_update_bi_to_nontransmitted(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
  wlc_bss_info_t *bi, wlc_bss_info_t *bi_original, struct dot11_management_header *hdr,
  uint8 current_cnt, dot11_mbssid_ie_t *mbssid_ie)
{
	int retval;

	/* reset to original bi */
	wlc_multibssid_overwrite_bi(wlc, bi, bi_original);
	/* update bi according to this nontransmitted BSSID profile */
	(void)wlc_multibssid_handling_eiscan(wlc, cfg, bi,
	                        hdr, current_cnt, mbssid_ie);
	/* update bcn frame body according to this nontransmitted BSSID profile */
	retval = wlc_multibssid_recv_preprocess_bcn(wlc, FALSE,
	             hdr, (uint8 *)bi->bcn_prb,  bi->bcn_prb_len,
	             mbssid_ie, current_cnt + 1, bi->BSSID.octet[ETHER_ADDR_LEN - 1]);
	if (retval < 0) {
		WL_SCAN_ERROR(("wlc_multibssid_recv_preprocess_bcn returned %d\n",
		               retval));
		bi->bcn_prb_len = 0;
	} else {
		bi->bcn_prb_len = (uint16)retval;
	}
}

void
wlc_multibssid_reset_max_mbssidindicator(wlc_info_t *wlc)
{
	wlc->mbssid_i->maxbssid_indicator = 0;
}

/* validate mbssid_ie returned from bcm_parse_tlvs */
bool
wlc_multibssid_valid_mbssidie(dot11_mbssid_ie_t *mbssid_ie)
{
	return (mbssid_ie && (mbssid_ie->len >= MULTIBSSD_IE_MIN_LEN));
}
