/*
 * Multiple BSSID implementation, header file
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
 * $Id: wlc_multibssid.h 791179 2020-09-18 08:37:39Z $
 */
void wlc_multibssid_config(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 attr);

int wlc_multibssid_recv_preprocess_bcn(wlc_info_t *wlc, bool assoc_scan,
  struct dot11_management_header *hdr, uint8 *body, uint body_len, dot11_mbssid_ie_t *mbssid_ie,
  uint8 current_cnt, uint8 lastbyte);

bcm_tlv_t *wlc_multibssid_recv_nontransmitted_ssid_bssid(wlc_info_t *wlc,
  struct dot11_management_header *hdr, dot11_mbssid_ie_t *mbssid_ie,
  uint8 current_cnt, struct ether_addr *cmpbssid);

uint8 wlc_multibssid_caltotal(wlc_info_t *wlc, wlc_bsscfg_t *cfg, dot11_mbssid_ie_t *mbssid_ie);

int wlc_multibssid_handling_eiscan(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	struct dot11_management_header *hdr, uint8 current_cnt, dot11_mbssid_ie_t *mbssid_ie);

void wlc_multibssid_convertbcn_4associated_nontransbssid(wlc_info_t *wlc,
  struct dot11_management_header *hdr, uint16 fc, uint16 fk,
  const d11rxhdr_t *rxh, uint len_mpdu);

int wlc_multibssid_duplicate_bi(wlc_info_t *wlc, wlc_bss_info_t *bi_dst, wlc_bss_info_t *bi_src);

void wlc_multibssid_overwrite_bi(wlc_info_t *wlc,  wlc_bss_info_t *bi_dst, wlc_bss_info_t *bi_src);
/* attach/detach interface */
wlc_multibssid_info_t *wlc_multibssid_attach(wlc_info_t *wlc);
void wlc_multibssid_detach(wlc_multibssid_info_t *mbssid_i);
void wlc_multibssid_cpymbssid_setting(wlc_info_t *wlc, wlc_bss_info_t *bi, wlc_bsscfg_t *cfg);
void wlc_multibssid_update_bi_to_nontransmitted(wlc_info_t *wlc,
 wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
wlc_bss_info_t *bi_original, struct dot11_management_header *hdr,
 uint8 current_cnt, dot11_mbssid_ie_t *mbssid_ie);
void wlc_multibssid_reset_max_mbssidindicator(wlc_info_t *wlc);

/* per wlc module info */
struct wlc_multibssid_info {
	wlc_info_t *wlc;
	struct ether_addr mbssid_transmitted;
	uint8 maxbssid_indicator;
	uint8 multibssid_index;
};

#define MAX_MAXBSSID_INDICATOR 8
/* Multiple BSSID IE should include at least:
 * 1 byte: MaxBSSID Indicator,
 */
#define MULTIBSSD_IE_MIN_LEN 1
/* Multiple BSSID-Index IE's length
 * can only be 1 or 3
 */
#define MULTIBSSID_IDX_IE_LEN1 1u
#define MULTIBSSID_IDX_IE_LEN3 3u

bool wlc_multibssid_valid_mbssidie(dot11_mbssid_ie_t *mbssid_ie);
