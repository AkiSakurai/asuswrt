/*
 * OBSS Protection support
 * Broadcom 802.11 Networking Device Driver
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
 * $Id$
 */

#ifndef _wlc_prot_obss_h_
#define _wlc_prot_obss_h_

/* Function: record secondary rssi histogram
 * three bins [hi, med, low] with
 * hi  : counting sec_rssi >= M_SECRSSI0_MIN (hi_thresh)
 * med : counting sec_rssi in [ M_SECRSSI1_MIN, M_SECRSSI0_MIN )
 * low : counting sec_rssi <= M_SECRSSI1_MIN (low_thresh)
 */
#define OBSS_SEC_RSSI_LIM0_DEFAULT				-50	/* in dBm */
#define OBSS_SEC_RSSI_LIM1_DEFAULT				-70	/* in dBm */
#define OBSS_INACTIVITY_PERIOD_DEFAULT				15	/* in seconds */
#define OBSS_DUR_THRESHOLD_DEFAULT				30	/* OBSS
* protection trigger for RX CRS Sec
*/

/* OBSS protection trigger for RX CRS Primary */
#define OBSS_DUR_RXCRS_PRI_THRESHOLD_DEFAULT			20

/* '70' is to check whether ibss is less than 70% of rxcrs primary */
#define OBSS_RATIO_RXCRS_PRI_VS_IBSS_DEFAULT			70

#define OBSS_BWSW_NO_ACTIVITY_CFM_PERIOD_DEFAULT		(6)	/* in seconds */
#define OBSS_BWSW_NO_ACTIVITY_CFM_PERIOD_INCR_DEFAULT		(5)	/* in seconds,
	* see explanation of obss_bwsw_no_activity_cfm_count_cfg
	*/
#define OBSS_BWSW_NO_ACTIVITY_MAX_INCR_DEFAULT			(30) /* in seconds,
* see explanation of obss_bwsw_no_activity_cfm_count_cfg
*/

#define OBSS_BWSW_ACTIVITY_CFM_PERIOD_DEFAULT			(3)	/* in seconds */
#define OBSS_BWSW_PSEUDO_SENSE_PERIOD_DEFAULT			(500) /* in msecs */
#define OBSS_BWSW_DUR_THRESHOLD_DEFAULT				15 /* OBSS DYN BWSW
* trigger for RX CRS Sec
*/
/* txop limit to trigger bw downgrade */
#define OBSS_TXOP_THRESHOLD_DEFAULT				12

/* module specific states */
struct wlc_prot_obss_info {
	bool protection;	/* TRUE if full phy bw CTS2SELF */
};

wlc_prot_obss_info_t *wlc_prot_obss_attach(wlc_info_t *wlc);
void wlc_prot_obss_detach(wlc_prot_obss_info_t *prot);

#ifdef WL_PROT_DYNBW
chanspec_t
wlc_prot_obss_ht_chanspec_override(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t *bsscfg, chanspec_t beacon_chanspec);

extern void
wlc_prot_obss_tx_bw_override(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t *bsscfg, uint32 *rspec_bw);
#endif /* WL_PROT_DYNBW */

void
wlc_prot_obss_beacon_chanspec_override(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t *bsscfg, chanspec_t *chanspec);

#ifdef WL_PROT_OBSS
#define WLC_PROT_OBSS_PROTECTION(prot)	((prot)->protection)
#else
#define WLC_PROT_OBSS_PROTECTION(prot)	(0)
#endif /* WL_PROT_OBSS */

extern void wlc_proto_obss_update_multiintf(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool wlc_prot_obss_interference_detected_for_bwsw_mif(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t * bsscfg);
extern void wlc_proto_obss_stats_init_multiintf(wlc_info_t *wlc, wlc_bsscfg_t *newcfg);
#endif /* _wlc_prot_obss_h_ */
