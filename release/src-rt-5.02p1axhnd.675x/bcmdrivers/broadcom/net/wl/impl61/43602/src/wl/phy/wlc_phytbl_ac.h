/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THIS IS A GENERATED FILE - DO NOT EDIT
 * Generated on Tue Aug 16 16:22:54 PDT 2011
 *
 * Copyright(c) 2007 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id: wlc_phytbl_ac.h 659247 2016-09-13 11:46:51Z $
 */
/* FILE-CSTYLED */

#include <wlc_phy_ac.h>

typedef phytbl_info_t acphytbl_info_t;

#define NUM_ROWS_CHAN_TUNING_2069 77

extern CONST acphytbl_info_t acphytbl_info_rev0[];
extern CONST uint32 acphytbl_info_sz_rev0;
extern CONST acphytbl_info_t acphytbl_info_rev2[];
extern CONST uint32 acphytbl_info_sz_rev2;
extern CONST acphytbl_info_t acphytbl_info_rev6[];
extern CONST uint32 acphytbl_info_sz_rev6;
extern CONST acphytbl_info_t acphytbl_info_rev3[];
extern CONST uint32 acphytbl_info_sz_rev3;
extern CONST acphytbl_info_t acphytbl_info_rev9[];
extern CONST uint32 acphytbl_info_sz_rev9;
extern chan_info_radio2069_t chan_tuning_2069rev3[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069_t chan_tuning_2069rev4[NUM_ROWS_CHAN_TUNING_2069];
extern CONST chan_info_radio2069_t chan_tuning_2069rev7[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_16_17[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_16_17_40[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_18[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_18_40[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_GE16_lp[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_23_2Glp_5Gnonlp[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_GE16_2Glp_5Gnonlp[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_GE16_40_lp[NUM_ROWS_CHAN_TUNING_2069];
extern BCMATTACHDATA(chan_info_radio2069revGE32_t) chan_tuning_2069_rev33_37[NUM_ROWS_CHAN_TUNING_2069];
extern BCMATTACHDATA(chan_info_radio2069revGE32_t) chan_tuning_2069_rev39_40[NUM_ROWS_CHAN_TUNING_2069];
extern BCMATTACHDATA(chan_info_radio2069revGE32_t) chan_tuning_2069_rev33_37_40[NUM_ROWS_CHAN_TUNING_2069];
extern BCMATTACHDATA(chan_info_radio2069revGE32_t) chan_tuning_2069_rev36[NUM_ROWS_CHAN_TUNING_2069];
extern BCMATTACHDATA(chan_info_radio2069revGE32_t) chan_tuning_2069_rev36_40[NUM_ROWS_CHAN_TUNING_2069];
extern BCMATTACHDATA(chan_info_radio2069revGE32_t) chan_tuning_2069_rev39[NUM_ROWS_CHAN_TUNING_2069];

extern chan_info_radio2069revGE16_t chan_tuning_2069rev_GE_18_40MHz_lp[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE16_t chan_tuning_2069rev_GE_18_lp[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE25_t chan_tuning_2069rev_GE_25[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE25_t chan_tuning_2069rev_GE_25_40MHz_lp[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE25_t chan_tuning_2069rev_GE_25_lp[NUM_ROWS_CHAN_TUNING_2069];
extern chan_info_radio2069revGE25_t chan_tuning_2069rev_GE_25_40MHz[NUM_ROWS_CHAN_TUNING_2069];
extern BCMATTACHDATA(chan_info_radio2069revGE25_52MHz_t) chan_tuning_2069rev_GE_25_52MHz[NUM_ROWS_CHAN_TUNING_2069];

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(BCMDBG_PHYDUMP)
#if defined(DBG_PHY_IOV) || defined(BCMDBG_PHYDUMP)
extern radio_20xx_dumpregs_t dumpregs_2069_rev0[];
extern radio_20xx_dumpregs_t dumpregs_2069_rev16[];
extern radio_20xx_dumpregs_t dumpregs_2069_rev17[];
extern radio_20xx_dumpregs_t dumpregs_2069_rev25[];
extern radio_20xx_dumpregs_t dumpregs_2069_rev32[];
#endif // endif
#endif // endif
extern radio_20xx_prefregs_t prefregs_2069_rev3[];
extern radio_20xx_prefregs_t prefregs_2069_rev4[];
extern radio_20xx_prefregs_t prefregs_2069_rev16[];
extern radio_20xx_prefregs_t prefregs_2069_rev17[];
extern radio_20xx_prefregs_t prefregs_2069_rev18[];
extern radio_20xx_prefregs_t prefregs_2069_rev23[];
extern radio_20xx_prefregs_t prefregs_2069_rev24[];
extern radio_20xx_prefregs_t prefregs_2069_rev25[];
extern radio_20xx_prefregs_t prefregs_2069_rev26[];
extern radio_20xx_prefregs_t prefregs_2069_rev33_37[];
extern radio_20xx_prefregs_t prefregs_2069_rev36[];
extern radio_20xx_prefregs_t prefregs_2069_rev39[];
extern uint16 ovr_regs_2069_rev2[];
extern uint16 ovr_regs_2069_rev16[];
extern uint16 ovr_regs_2069_rev32[];
#ifndef ACPHY_1X1_ONLY
extern chan_info_rx_farrow BCMATTACHDATA(rx_farrow_tbl)[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
extern chan_info_tx_farrow BCMATTACHDATA(tx_farrow_dac1_tbl)[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
extern chan_info_tx_farrow BCMATTACHDATA(tx_farrow_dac2_tbl)[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
extern chan_info_tx_farrow BCMATTACHDATA(tx_farrow_dac3_tbl)[ACPHY_NUM_BW][ACPHY_NUM_CHANS];
#else /* ACPHY_1X1_ONLY */
extern chan_info_rx_farrow BCMATTACHDATA(rx_farrow_tbl[1][ACPHY_NUM_CHANS]);
extern chan_info_tx_farrow BCMATTACHDATA(tx_farrow_dac1_tbl[1][ACPHY_NUM_CHANS]);
#endif /* ACPHY_1X1_ONLY */
extern uint16 acphy_txgain_epa_2g_2069rev0[];
extern uint16 acphy_txgain_epa_5g_2069rev0[];
extern uint16 acphy_txgain_ipa_2g_2069rev0[];
extern uint16 acphy_txgain_ipa_5g_2069rev0[];

extern uint16 acphy_txgain_ipa_2g_2069rev16[];

extern uint16 acphy_txgain_epa_2g_2069rev17[];
extern uint16 acphy_txgain_epa_5g_2069rev17[];
extern uint16 acphy_txgain_ipa_2g_2069rev17[];
extern uint16 acphy_txgain_ipa_5g_2069rev17[];
extern uint16 acphy_txgain_epa_2g_2069rev18[];
extern uint16 acphy_txgain_epa_5g_2069rev18[];

extern uint16 acphy_txgain_epa_2g_2069rev4[];
extern CONST uint16 acphy_txgain_epa_2g_2069rev7[];
extern uint16 acphy_txgain_epa_2g_2069rev4_id1[];
extern uint16 acphy_txgain_epa_5g_2069rev4[];
extern CONST uint16 acphy_txgain_epa_5g_2069rev7[];
extern uint16 acphy_txgain_epa_2g_2069rev16[];
extern uint16 acphy_txgain_epa_5g_2069rev16[];
extern uint16 acphy_txgain_ipa_2g_2069rev18[];
extern uint16 acphy_txgain_ipa_5g_2069rev16[];
extern uint16 acphy_txgain_ipa_2g_2069rev25[];
extern uint16 acphy_txgain_ipa_2g_2069rev25_bcm43162[];
extern uint16 acphy_txgain_ipa_5g_2069rev25[];
extern uint16 acphy_txgain_ipa_5g_2069rev18[];
extern uint16 acphy_txgain_epa_2g_2069rev33_35_36_37[];
extern uint16 acphy_txgain_epa_5g_2069rev33_35_36[];
extern uint16 acphy_txgain_epa_5g_2069rev37_38[];
extern uint16 acphy_txgain_epa_2g_2069rev34[];
extern uint16 acphy_txgain_epa_5g_2069rev34[];
extern uint16 acphy_txgain_ipa_2g_2069rev33_37[];
extern uint16 acphy_txgain_ipa_5g_2069rev33_37[];
extern uint16 acphy_txgain_ipa_2g_2069rev39[];
extern uint16 acphy_txgain_ipa_5g_2069rev39[];
extern CONST uint32 acphy_txv_for_spexp[];
