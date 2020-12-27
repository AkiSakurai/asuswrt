/*
 * OBSS Protection support
 * Broadcom 802.11 Networking Device Driver
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
#define OBSS_BWSW_PSEUDO_SENSE_PERIOD_DEFAULT			(2000) /* in msecs */
#define OBSS_BWSW_DUR_THRESHOLD_DEFAULT				15 /* OBSS DYN BWSW
* trigger for RX CRS Sec
*/
/* txop limit to trigger bw downgrade */
#define OBSS_TXOP_THRESHOLD_DEFAULT 				5

/* module specific states */
struct wlc_prot_obss_info {
	bool protection;	/* TRUE if full phy bw CTS2SELF */
};

wlc_prot_obss_info_t *wlc_prot_obss_attach(wlc_info_t *wlc);
void wlc_prot_obss_detach(wlc_prot_obss_info_t *prot);

bool
wlc_prot_obss_secondary_interference_detected(wlc_prot_obss_info_t *prot,
  uint8 obss_dur_threshold);

bool
wlc_prot_obss_primary_interference_detected(wlc_prot_obss_info_t *prot);

bool
wlc_prot_obss_interference_detected_for_bwsw(wlc_prot_obss_info_t *prot,
	wlc_bmac_obss_counts_t *delta_stats);

chanspec_t
wlc_prot_obss_ht_chanspec_override(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t *bsscfg, chanspec_t beacon_chanspec);

void
wlc_prot_obss_tx_bw_override(wlc_prot_obss_info_t *prot,
	wlc_bsscfg_t *bsscfg, uint32 *rspec_bw);

#ifdef WL_PROT_OBSS
#define WLC_PROT_OBSS_PROTECTION(prot)	((prot)->protection)
#else
#define WLC_PROT_OBSS_PROTECTION(prot)	(0)
#endif /* WL_PROT_OBSS */

#endif /* _wlc_prot_obss_h_ */
