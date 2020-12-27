/*
 * Radio 20707 table definition header file
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
 * $Id$
 */

#ifndef _WLC_PHYTBL_20707_H_
#define _WLC_PHYTBL_20707_H_

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>

#include "wlc_phy_int.h"
#include "phy_ac_rxgcrs.h"

typedef struct _chan_info_radio20707_rffe_2G {
	/* 2G tuning data */
	uint8 RFP0_logen_reg1_logen_mix_ctune;
	uint8 RF0_rx5g_reg1_rxdb_lna_tune;
	uint8 RF1_rx5g_reg1_rxdb_lna_tune;
	uint8 RF2_rx5g_reg1_rxdb_lna_tune;
	uint8 RF0_logen_core_reg3_logen_lc_ctune;
	uint8 RF1_logen_core_reg3_logen_lc_ctune;
	uint8 RF2_logen_core_reg3_logen_lc_ctune;
	uint8 RF0_tx2g_mix_reg4_tx2g_mx_tune;
	uint8 RF1_tx2g_mix_reg4_tx2g_mx_tune;
	uint8 RF2_tx2g_mix_reg4_tx2g_mx_tune;
	uint8 RF0_tx2g_pad_reg3_tx2g_pad_tune;
	uint8 RF1_tx2g_pad_reg3_tx2g_pad_tune;
	uint8 RF2_tx2g_pad_reg3_tx2g_pad_tune;
	uint8 RFP0_logen_reg0_logen_div3en;
} chan_info_radio20707_rffe_2G_t;

typedef struct _chan_info_radio20707_rffe_5G {
	/* 5G tuning data */
	uint8 RFP0_logen_reg1_logen_mix_ctune;
	uint8 RF0_logen_core_reg3_logen_lc_ctune;
	uint8 RF1_logen_core_reg3_logen_lc_ctune;
	uint8 RF2_logen_core_reg3_logen_lc_ctune;
	uint8 RF0_rx5g_reg1_rxdb_lna_tune;
	uint8 RF1_rx5g_reg1_rxdb_lna_tune;
	uint8 RF2_rx5g_reg1_rxdb_lna_tune;
	uint8 RF0_tx5g_mix_reg4_tx5g_mx_tune;
	uint8 RF1_tx5g_mix_reg4_tx5g_mx_tune;
	uint8 RF2_tx5g_mix_reg4_tx5g_mx_tune;
	uint8 RF0_tx5g_pad_reg3_tx5g_pad_tune;
	uint8 RF1_tx5g_pad_reg3_tx5g_pad_tune;
	uint8 RF2_tx5g_pad_reg3_tx5g_pad_tune;
	uint8 RFP0_logen_reg0_logen_div3en;
} chan_info_radio20707_rffe_5G_t;

typedef struct _chan_info_radio20707_rffe_6G {
	/* 5G tuning data */
	uint8 RFP0_logen_reg1_logen_mix_ctune;
	uint8 RF0_logen_core_reg3_logen_lc_ctune;
	uint8 RF1_logen_core_reg3_logen_lc_ctune;
	uint8 RF2_logen_core_reg3_logen_lc_ctune;
	uint8 RF0_rx5g_reg1_rxdb_lna_tune;
	uint8 RF1_rx5g_reg1_rxdb_lna_tune;
	uint8 RF2_rx5g_reg1_rxdb_lna_tune;
	uint8 RF0_tx5g_mix_reg4_tx5g_mx_tune;
	uint8 RF1_tx5g_mix_reg4_tx5g_mx_tune;
	uint8 RF2_tx5g_mix_reg4_tx5g_mx_tune;
	uint8 RF0_tx5g_pad_reg3_tx5g_pad_tune;
	uint8 RF1_tx5g_pad_reg3_tx5g_pad_tune;
	uint8 RF2_tx5g_pad_reg3_tx5g_pad_tune;
	uint8 RFP0_logen_reg0_logen_div3en;
} chan_info_radio20707_rffe_6G_t;

typedef struct _chan_info_radio20707_rffe {
	uint16 channel;
	uint16 freq;
	union {
		/* In this union, make sure the largest struct is at the top. */
		chan_info_radio20707_rffe_6G_t val_6G;
		chan_info_radio20707_rffe_5G_t val_5G;
		chan_info_radio20707_rffe_2G_t val_2G;
	} u;
} chan_info_radio20707_rffe_t;

extern const chan_info_radio20707_rffe_t chan_tune_20707_rev0_2g[];
extern const chan_info_radio20707_rffe_t chan_tune_20707_rev0_5g[];
extern const chan_info_radio20707_rffe_t chan_tune_20707_rev0_6g[];
extern const chan_info_radio20707_rffe_t chan_tune_20707_rev1_2g[];
extern const chan_info_radio20707_rffe_t chan_tune_20707_rev1_ipa_5g[];
extern const chan_info_radio20707_rffe_t chan_tune_20707_rev1_epa_5g[];
extern const chan_info_radio20707_rffe_t chan_tune_20707_rev1_6g[];

extern const uint16 chan_tune_20707_rev0_length_2g;
extern const uint16 chan_tune_20707_rev0_length_5g;
extern const uint16 chan_tune_20707_rev0_length_6g;
extern const uint16 chan_tune_20707_rev1_length_2g;
extern const uint16 chan_tune_20707_rev1_length_ipa_5g;
extern const uint16 chan_tune_20707_rev1_length_epa_5g;
extern const uint16 chan_tune_20707_rev1_length_6g;

#if (defined(BCMDBG) && defined(DBG_PHY_IOV)) || defined(BCMDBG_PHYDUMP)
extern const radio_20xx_dumpregs_t dumpregs_20707_rev0[];
extern const radio_20xx_dumpregs_t dumpregs_20707_rev1[];
#endif // endif

/* Radio referred values tables */
extern const radio_20xx_prefregs_t prefregs_20707_rev0[];
extern const radio_20xx_prefregs_t prefregs_20707_rev1[];

extern int8 BCMATTACHDATA(lna12_gain_tbl_2g_20707rX_ilna)[2][N_LNA12_GAINS];
extern int8 BCMATTACHDATA(lna12_gain_tbl_5g_20707rX_ilna)[2][N_LNA12_GAINS];
extern int8 BCMATTACHDATA(lna12_gainbits_tbl_2g_20707rX_ilna)[2][N_LNA12_GAINS];
extern int8 BCMATTACHDATA(lna12_gainbits_tbl_5g_20707rX_ilna)[2][N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_rout_map_2g_20707rX_ilna)[N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_rout_map_5g_20707rX_ilna)[N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_gain_map_2g_20707rX_ilna)[N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_gain_map_5g_20707rX_ilna)[N_LNA12_GAINS];

extern int8 BCMATTACHDATA(lna12_gain_tbl_2g_20707rX_elna)[2][N_LNA12_GAINS];
extern int8 BCMATTACHDATA(lna12_gain_tbl_5g_20707rX_elna)[2][N_LNA12_GAINS];
extern int8 BCMATTACHDATA(lna12_gainbits_tbl_2g_20707rX_elna)[2][N_LNA12_GAINS];
extern int8 BCMATTACHDATA(lna12_gainbits_tbl_5g_20707rX_elna)[2][N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_rout_map_2g_20707rX_elna)[N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_rout_map_5g_20707rX_elna)[N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_gain_map_2g_20707rX_elna)[N_LNA12_GAINS];
extern uint8 BCMATTACHDATA(lna1_gain_map_5g_20707rX_elna)[N_LNA12_GAINS];

extern int8 BCMATTACHDATA(gainlimit_tbl_20707rX)[RXGAIN_CONF_ELEMENTS][MAX_RX_GAINS_PER_ELEM];
extern int8 BCMATTACHDATA(tia_gain_tbl_20707rX)[N_TIA_GAINS];
extern int8 BCMATTACHDATA(tia_gainbits_tbl_20707rX)[N_TIA_GAINS];
extern int8 BCMATTACHDATA(biq01_gain_tbl_20707rX)[2][N_BIQ01_GAINS];
extern int8 BCMATTACHDATA(biq01_gainbits_tbl_20707rX)[2][N_BIQ01_GAINS];

extern uint16 BCMATTACHDATA(nap_lo_th_adj_maj51)[5];
extern uint16 BCMATTACHDATA(nap_hi_th_adj_maj51)[5];

#endif	/* _WLC_PHYTBL_20707_H_ */
