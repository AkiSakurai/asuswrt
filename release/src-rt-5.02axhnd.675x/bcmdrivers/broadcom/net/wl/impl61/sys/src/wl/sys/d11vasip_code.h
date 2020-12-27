/*
 * VASIP init declarations for Broadcom 802.11
 * Networking Adapter Device Driver.
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
 * $Id: d11vasip_code.h 777548 2019-08-06 00:14:29Z $
 */

/* vasip code and inits */

#ifndef __D11VASIP_CODE_H__
#define __D11VASIP_CODE_H__

#include <typedefs.h>
#include <bcmdefs.h>

/* VASIP FW Version 6.1203 */
#define VASIP_FW_VER_MAJOR 6
#define VASIP_FW_VER_MINOR 1203

extern CONST uint32 d11vasipcode_major;
extern CONST uint32 d11vasipcode_minor;
enum {
	__vasip_map_bfd_log_buffer                       = 0,
	__vasip_map_bfds_mlbf_angle_buffer               = 1,
	__vasip_map_cmd_buffer                           = 2,
	__vasip_map_csi_rpt                              = 3,
	__vasip_map_delay_grouping_us                    = 4,
	__vasip_map_delay_precoding_us                   = 5,
	__vasip_map_err_code                             = 6,
	__vasip_map_err_code_group                       = 7,
	__vasip_map_err_code_precoder                    = 8,
	__vasip_map_err_count                            = 9,
	__vasip_map_fading_det_buf0                      = 10,
	__vasip_map_fading_det_buf1                      = 11,
	__vasip_map_grouping_forced                      = 12,
	__vasip_map_grouping_forced_mcs                  = 13,
	__vasip_map_grouping_mcs_check                   = 14,
	__vasip_map_grouping_mcs_delta                   = 15,
	__vasip_map_grouping_mcs_min                     = 16,
	__vasip_map_grouping_method                      = 17,
	__vasip_map_grouping_nss_comb_round_robin_ena    = 18,
	__vasip_map_grouping_number                      = 19,
	__vasip_map_grp_forced_buf                       = 20,
	__vasip_map_grp_mcs_tbl                          = 21,
	__vasip_map_grp_on_the_fly_buf                   = 22,
	__vasip_map_grp_sel_buf                          = 23,
	__vasip_map_imbf_rpt                             = 24,
	__vasip_map_interrupt                            = 25,
	__vasip_map_m2v_buf0                             = 26,
	__vasip_map_m2v_buf1                             = 27,
	__vasip_map_mcs_capping_enable                   = 28,
	__vasip_map_mcs_capping_threshold2               = 29,
	__vasip_map_mcs_capping_threshold3               = 30,
	__vasip_map_mcs_capping_threshold4               = 31,
	__vasip_map_mcs_map                              = 32,
	__vasip_map_mcs_overwrite_flag                   = 33,
	__vasip_map_mcs_rate                             = 34,
	__vasip_map_mi_maxmcs_in_const                   = 35,
	__vasip_map_mi_mcsnum_per_const                  = 36,
	__vasip_map_mi_mcs_recommend_enable              = 37,
	__vasip_map_mi_mcs_to_const                      = 38,
	__vasip_map_mu_mcs_max                           = 39,
	__vasip_map_overwrite_mcs                        = 40,
	__vasip_map_plugfest_en                          = 41,
	__vasip_map_precoder_buf                         = 42,
	__vasip_map_recommend_mcs                        = 43,
	__vasip_map_sgi_method                           = 44,
	__vasip_map_sgi_status                           = 45,
	__vasip_map_sgi_threshold_20M                    = 46,
	__vasip_map_sgi_threshold_40M                    = 47,
	__vasip_map_sgi_threshold_80M                    = 48,
	__vasip_map_sounding_update_buf                  = 49,
	__vasip_map_steering_mcs                         = 50,
	__vasip_map_txbf_ppr_tbl                         = 51,
	__vasip_map_v2m_buf_grp                          = 52,
	__vasip_map_v2m_buf_mvp                          = 53,
	__vasip_map_v2m_buf_on_the_fly                   = 54,
	__vasip_map_v2m_buf_snd                          = 55,
	__vasip_map_v2m_transfer_done                    = 56,
	vasipfw_symbol_count                             = 57
};

typedef struct {
	CONST uint32 *map;
	CONST uint16 *size;
	CONST uint32 *code;
	CONST uint32 *data;
	CONST uint code_size;
	CONST uint data_size;
	CONST uint ntx;
} vasip_fw_t;

extern CONST vasip_fw_t vasip_11ac_4x4_fw;

extern CONST vasip_fw_t vasip_11ac_3x3_fw;

extern CONST vasip_fw_t vasip_11ax_4x4_fw;

extern CONST vasip_fw_t vasip_airiq_11ac_fw;

extern CONST vasip_fw_t vasip_11ax_2x2_fw;

extern CONST vasip_fw_t vasip_11ax_3x3_fw;

#endif /* __D11VASIP_CODE_H__ */
