/*
 * ACPHY RSSI Compute module interface
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
 * $Id: phy_ac_rssi.h 785600 2020-03-31 18:18:10Z $
 */

#ifndef _phy_ac_rssi_h_
#define _phy_ac_rssi_h_

#include <phy_api.h>
#include <phy_ac.h>
#include <phy_rssi.h>
#include <phy_type_rssi.h>

/* forward declaration */
typedef struct phy_ac_rssi_info phy_ac_rssi_info_t;

#define MAX_NUM_ULOFDMA_USERS 16

typedef struct {
	uint8 num_users;
	uint8 mu_fstr_error;
	uint8 mu_error_code;
	uint8 user_mac_idx[MAX_NUM_ULOFDMA_USERS];
	int16 user_rssi[MAX_NUM_ULOFDMA_USERS];
	uint8 user_snr[MAX_NUM_ULOFDMA_USERS];
	int16 user_freq_error[MAX_NUM_ULOFDMA_USERS];
	int16 ul_pwr_ctrl;
	uint32 timestamp;
	uint16 txtrig_cnt;
	uint16 rxofdma_cnt;
	uint16 rxofdma_cnt_last;
	uint8 ulofdma_rssi_cal_disable;
} phy_ulofdma_per_user_rxstats_t;

typedef struct {
	bool   pos_freq;
	uint16 freq_offset_l, freq_offset_h;
	uint32 freq_offset_total;
	uint32 tracked_freq_offsetHz;
	uint32 pll_wild_base_offset;
} phy_tracked_freq_offset_t;

typedef struct phy_ac_rssi_data {
	/* this data is shared between rssi and misc */
	bool	rxgaincal_rssical;	/* 0 = rxgain error cal and 1 = RSSI error cal */
	/* this data is shared between rssi, misc and rxgcrs */
	bool	rssi_cal_rev;		/* 0 = OLD and 1 = NEW */
	/* this data is only shared to phy_ac_rssi_iov */
	bool	rssi_qdB_en; /* 0 = dqB reporting of RSSI is disabled */
	/* this data is for API */
	bool	subbandintf_en; /* 0 = subband interference measurement is disabled */
	phy_ulofdma_per_user_rxstats_t per_user_stats;
	phy_tracked_freq_offset_t tracked_freq_stats;
} phy_ac_rssi_data_t;

/* register/unregister ACPHY specific implementations to/from common */
phy_ac_rssi_info_t *phy_ac_rssi_register_impl(phy_info_t *pi,
	phy_ac_info_t *aci, phy_rssi_info_t *ri);
void phy_ac_rssi_unregister_impl(phy_ac_rssi_info_t *info);

/* inter-module data API */
phy_ac_rssi_data_t *phy_ac_rssi_get_data(phy_ac_rssi_info_t *rssii);

/* intra-module data API */
/* this is accessed by rxgcrs module */
int phy_ac_rssi_set_cal_rev(phy_ac_rssi_info_t *ri, bool set_val);
/* this is accessed only by the iov file */
int phy_ac_rssi_set_qdb_en(phy_ac_rssi_info_t *ri, bool set_val);
int phy_ac_rssi_set_cal_rxgain(phy_ac_rssi_info_t *ri, bool set_val);

/* subband RSSI setup */
extern void phy_ac_rssi_subbandrssi_setup(phy_info_t *pi, uint8 mode);

/* subband intf enable API */
extern int phy_ac_rssi_subbandintf_enable(phy_info_t *pi, bool set_val);

/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
void phy_ac_rssi_init_gain_err(phy_ac_rssi_info_t *info);

extern int16 phy_ac_rssi_compute_compensation(phy_type_rssi_ctx_t *ctx,
	int16 *rxpwr_core, bool db_qdb);
uint8 wlc_phy_rssi_get_chan_freq_range_acphy(phy_info_t *pi, uint8 core_segment_mapping,
	uint8 core);
int phy_ac_rssi_set_qdb_en(phy_ac_rssi_info_t *ri, bool set_val);
int phy_ac_rssi_get_qdb_en(phy_ac_rssi_info_t *ri, int32 *ret_val);

#endif /* _phy_ac_rssi_h_ */
