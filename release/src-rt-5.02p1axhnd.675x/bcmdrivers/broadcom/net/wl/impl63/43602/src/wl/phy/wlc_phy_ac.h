/*
 * ACPHY module header file
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
 * $Id: wlc_phy_ac.h 657469 2016-09-01 10:06:08Z $
 */

#ifndef _wlc_phy_ac_h_
#define _wlc_phy_ac_h_

#include <typedefs.h>
#include <wlc_phy_int.h>

/* Macro's ACREV0 and ACREV3 indicate an old(er) vs a new(er) 2069 radio */
#if ACCONF
#ifdef BCMCHIPID
#define  ACREV0 (BCMCHIPID == BCM4360_CHIP_ID || BCMCHIPID == BCM4352_CHIP_ID || \
		 BCMCHIPID == BCM43460_CHIP_ID || BCMCHIPID == BCM43526_CHIP_ID || \
		 BCM43602_CHIP(BCMCHIPID))
#define  ACREV3 BCM4350_CHIP(BCMCHIPID)
#else
extern int acphychipid;
#define ACREV0 (acphychipid == BCM4360_CHIP_ID || acphychipid == BCM4352_CHIP_ID ||\
		acphychipid == BCM43460_CHIP_ID || acphychipid == BCM43526_CHIP_ID || \
		BCM43602_CHIP(acphychipid))
#define ACREV3 BCM4350_CHIP(acphychipid)
#endif // endif
#else
#define ACREV0 0
#define ACREV3 0
#endif /* ACCONF != 0 */

#define ACPHY_GAIN_VS_TEMP_SLOPE_2G 7   /* units: db/100C */
#define ACPHY_GAIN_VS_TEMP_SLOPE_5G 7   /* units: db/100C */
#define ACPHY_SWCTRL_NVRAM_PARAMS 5
#define ACPHY_RSSIOFFSET_NVRAM_PARAMS 4
#define ACPHY_GAIN_DELTA_2G_PARAMS 2
#define ACPHY_TXPOWERCAP_PARAMS 3
#define ACPHY_TXPOWERCAP_MAX_QDB 127

/* The above variable had only 2 params (gain settings)
 * ELNA ON and ELNA OFF
 * With Olympic, we added 2 more gain settings for 2 Routs
 * Order of the variables is - ELNA_On, ELNA_Off, Rout_1, Rout_2
 * Both the routs should be with ELNA on, as
 * Elna_on - Elna_off offset is added to tr_loss and
 * ELNA_Off value is never used again.
 */
#define ACPHY_GAIN_DELTA_2G_PARAMS_EXT 4
#define ACPHY_GAIN_DELTA_ELNA_ON 0
#define ACPHY_GAIN_DELTA_ELNA_OFF 1
#define ACPHY_GAIN_DELTA_ROUT_1 2
#define ACPHY_GAIN_DELTA_ROUT_2 3

#define ACPHY_GAIN_DELTA_5G_PARAMS_EXT 4
#define ACPHY_GAIN_DELTA_5G_PARAMS 2
#define ACPHY_RCAL_OFFSET 0x10	/* otp offset for Wlan RCAL code */
#define ACPHY_RCAL_VAL_1X1 0xa  /* hard coded rcal_trim val for 1X1 chips */
#define ACPHY_RCAL_VAL_2X2 0x9  /* hard coded rcal_trim val for 2X2 chips */

#define ACPHY_NUM_CHAN_2G_20 14
#define ACPHY_NUM_CHAN_5G_20 25

/* ACPHY tables */
#define ACPHY_TBL_ID_MCS                          1
#define ACPHY_TBL_ID_TXEVMTBL                     2
#define ACPHY_TBL_ID_NVNOISESHAPINGTBL	          3
#define ACPHY_TBL_ID_NVRXEVMSHAPINGTBL	          4
#define ACPHY_TBL_ID_PHASETRACKTBL                5
#define ACPHY_TBL_ID_SQTHRESHOLD                  6
#define ACPHY_TBL_ID_RFSEQ                        7
#define ACPHY_TBL_ID_RFSEQEXT                     8
#define ACPHY_TBL_ID_ANTSWCTRLLUT                 9
#define ACPHY_TBL_ID_FEMCTRLLUT                  10
#define ACPHY_TBL_ID_GAINLIMIT                   11
#define ACPHY_TBL_ID_IQLOCAL                     12
#define ACPHY_TBL_ID_PAPR                        13
#define ACPHY_TBL_ID_SAMPLEPLAY                  14
#define ACPHY_TBL_ID_DUPSTRNTBL                  15
#define ACPHY_TBL_ID_BFMUSERINDEX                16
#define ACPHY_TBL_ID_BFECONFIG                   17
#define ACPHY_TBL_ID_BFEMATRIX                   18
#define ACPHY_TBL_ID_FASTCHSWITCH                19
#define ACPHY_TBL_ID_RFSEQBUNDLE                 20
#define ACPHY_TBL_ID_LNAROUT                     21
#define ACPHY_TBL_ID_MCDSNRVAL                   22
#define ACPHY_TBL_ID_BFRRPT                      23
#define ACPHY_TBL_ID_BFERPT                      24
#define ACPHY_TBL_ID_NVADJTBL                    25
#define ACPHY_TBL_ID_PHASETRACKTBL_1X1           26
#define ACPHY_TBL_ID_SCD_DBMTBL                  27
#define ACPHY_TBL_ID_DCD_DBMTBL                  28
#define ACPHY_TBL_ID_SLNAGAIN                    29
#define ACPHY_TBL_ID_BFECONFIG2X2TBL             30
#define ACPHY_TBL_ID_GAINCTRLBBMULTLUTS          32
#define ACPHY_TBL_ID_ESTPWRSHFTLUTS              33
#define ACPHY_TBL_ID_CHANNELSMOOTHING_1x1        34
#define ACPHY_TBL_ID_SGIADJUST                   35
#define ACPHY_TBL_ID_ESTPWRLUTS0                 64
#define ACPHY_TBL_ID_IQCOEFFLUTS0                65
#define ACPHY_TBL_ID_LOFTCOEFFLUTS0              66
#define ACPHY_TBL_ID_RFPWRLUTS0                  67
#define ACPHY_TBL_ID_GAIN0                       68
#define ACPHY_TBL_ID_GAINBITS0                   69
#define ACPHY_TBL_ID_RSSICLIPGAIN0               70
#define ACPHY_TBL_ID_EPSILON0                    71
#define ACPHY_TBL_ID_SCALAR0                     72
#define ACPHY_TBL_ID_CORE0CHANESTTBL             73
#define ACPHY_TBL_ID_SNOOPAGC                    80
#define ACPHY_TBL_ID_SNOOPPEAK                   81
#define ACPHY_TBL_ID_SNOOPCCKLMS                 82
#define ACPHY_TBL_ID_SNOOPLMS                    83
#define ACPHY_TBL_ID_SNOOPDCCMP                  84
#define ACPHY_TBL_ID_FDSS_MCSINFOTBL0            88
#define ACPHY_TBL_ID_FDSS_SCALEADJUSTFACTORSTBL0 89
#define ACPHY_TBL_ID_FDSS_BREAKPOINTSTBL0        90
#define ACPHY_TBL_ID_FDSS_SCALEFACTORSTBL0       91
#define ACPHY_TBL_ID_FDSS_SCALEFACTORSDELTATBL0  92
#define ACPHY_TBL_ID_PAPDLUTSELECT0              93
#define ACPHY_TBL_ID_GAINLIMIT0                  94
#define ACPHY_TBL_ID_ESTPWRLUTS1                 96
#define ACPHY_TBL_ID_IQCOEFFLUTS1                97
#define ACPHY_TBL_ID_LOFTCOEFFLUTS1              98
#define ACPHY_TBL_ID_RFPWRLUTS1                  99
#define ACPHY_TBL_ID_GAIN1                      100
#define ACPHY_TBL_ID_GAINBITS1                  101
#define ACPHY_TBL_ID_RSSICLIPGAIN1              102
#define ACPHY_TBL_ID_EPSILON1                   103
#define ACPHY_TBL_ID_SCALAR1                    104
#define ACPHY_TBL_ID_CORE1CHANESTTBL            105
#define ACPHY_TBL_ID_GAINLIMIT1                 126
#define ACPHY_TBL_ID_ESTPWRLUTS2                128
#define ACPHY_TBL_ID_IQCOEFFLUTS2               129
#define ACPHY_TBL_ID_LOFTCOEFFLUTS2             130
#define ACPHY_TBL_ID_RFPWRLUTS2                 131
#define ACPHY_TBL_ID_GAIN2                      132
#define ACPHY_TBL_ID_GAINBITS2                  133
#define ACPHY_TBL_ID_RSSICLIPGAIN2              134
#define ACPHY_TBL_ID_EPSILON2                   135
#define ACPHY_TBL_ID_SCALAR2                    136
#define ACPHY_TBL_ID_CORE2CHANESTTBL            137
#define ACPHY_TBL_ID_GAINLIMIT2                 158
#define ACPHY_NUM_DIG_FILT_COEFFS               15
#define ACPHY_TBL_LEN_NVNOISESHAPINGTBL         256
#define ACPHY_SPURWAR_NTONES_OFFSET              24 /* Starting offset for spurwar */
#define ACPHY_NV_NTONES_OFFSET                    0 /* Starting offset for nvshp */
#define ACPHY_SPURWAR_NTONES                      8 /* Numver of tones for spurwar */
#define ACPHY_NV_NTONES                          24 /* Numver of tones for nvshp */
/* Number of tones(spurwar+nvshp) to be written */
#define ACPHY_SPURWAR_NV_NTONES                  32

/* channel smoothing 1x1 and core0 chanest data formate */
#define UNPACK_FLOAT_AUTO_SCALE			1

#define CORE0CHANESTTBL_TABLE_WIDTH		32
#define CORE0CHANESTTBL_INTEGER_DATA_SIZE	14
#define CORE0CHANESTTBL_INTEGER_DATA_MASK	((1 << CORE0CHANESTTBL_INTEGER_DATA_SIZE) - 1)
#define CORE0CHANESTTBL_INTEGER_MAXVALUE	((CORE0CHANESTTBL_INTEGER_DATA_MASK+1)>>1)

#define CORE0CHANESTTBL_FLOAT_FORMAT		1
#define CORE0CHANESTTBL_REV0_DATA_SIZE		12
#define CORE0CHANESTTBL_REV0_EXP_SIZE		6
#define CORE0CHANESTTBL_REV2_DATA_SIZE		9
#define CORE0CHANESTTBL_REV2_EXP_SIZE		5

#define CHANNELSMOOTHING_FLOAT_FORMAT		0
#define CHANNELSMOOTHING_FLOAT_DATA_SIZE	11
#define CHANNELSMOOTHING_FLOAT_EXP_SIZE		8
#define CHANNELSMOOTHING_DATA_OFFSET		256

/* When the broadcast bit is set in the PHY reg address
 * it writes to the corresponding registers in all the cores
 */
#define ACPHY_REG_BROADCAST ((ACREV0 || ACREV3) ? 0x1000 : 0)

/* ACPHY RFSeq Commands */
#define ACPHY_RFSEQ_RX2TX		0x0
#define ACPHY_RFSEQ_TX2RX		0x1
#define ACPHY_RFSEQ_RESET2RX		0x2
#define ACPHY_RFSEQ_UPDATEGAINH		0x3
#define ACPHY_RFSEQ_UPDATEGAINL		0x4
#define ACPHY_RFSEQ_UPDATEGAINU		0x5

#define ACPHY_SPINWAIT_RFSEQ_STOP		1000
#define ACPHY_SPINWAIT_RFSEQ_FORCE		200000
#define ACPHY_SPINWAIT_RUNSAMPLE		1000
#define ACPHY_SPINWAIT_TXIQLO			20000
#define ACPHY_SPINWAIT_IQEST			10000

#define ACPHY_NUM_BW                    3
#define ACPHY_NUM_CHANS                 123
#define ACPHY_NUM_BW_2G                 2

#define ACPHY_ClassifierCtrl_classifierSel_MASK 0x7

/* Board types */
#define BCM94360MCI_SSID 0x61a
#define BCM94360CS_SSID  0x61b

/* gainctrl */
/* stages: elna, lna1, lna2, mix, bq0, bq1, dvga (tiny only) */
#define ACPHY_MAX_RX_GAIN_STAGES 7
#define ACPHY_INIT_GAIN 69
#define ACPHY_HI_GAIN 48
#define ACPHY_CRSMIN_DEFAULT 54
#define ACPHY_CRSMIN_GAINHI 32
#define ACPHY_MAX_LNA1_IDX 5
#define ACPHY_ILNA2G_MAX_LNA2_IDX 6
#define ACPHY_ELNA2G_MAX_LNA2_IDX 4
#define ACPHY_ELNA2G_MAX_LNA2_IDX_L 5
#define ACPHY_ILNA5G_MAX_LNA2_IDX 6
#define ACPHY_ELNA5G_MAX_LNA2_IDX 6

/* ACI (start) */
#define ACPHY_ACI_CHAN_LIST_SZ 3

#define ACPHY_ACI_MAX_DESENSE_BPHY_DB 24
#define ACPHY_ACI_MAX_DESENSE_OFDM_DB 48
#define ACPHY_ACI_COARSE_DESENSE_UP 4
#define ACPHY_ACI_COARSE_DESENSE_DN 4

#define ACPHY_ACI_NUM_MAX_GLITCH_AVG 2
#define ACPHY_ACI_WAIT_POST_MITIGATION 1
#define ACPHY_ACI_OFDM_HI_GLITCH_THRESH 600
#define ACPHY_ACI_OFDM_LO_GLITCH_THRESH 300
#define ACPHY_ACI_BPHY_HI_GLITCH_THRESH 300
#define ACPHY_ACI_BPHY_LO_GLITCH_THRESH 100
#define ACPHY_ACI_BORDER_GLITCH_SLEEP 12
#define ACPHY_ACI_MD_GLITCH_SLEEP 8
#define ACPHY_ACI_LO_GLITCH_SLEEP 4
#define ACPHY_ACI_GLITCH_BUFFER_SZ 4

/* hw aci */
#define ACPHY_HWACI_MAX_STATES 5       /* min 1 for default */
#define ACPHY_HWACI_NOACI_WAIT_TIME  8 /* sec */
#define ACPHY_HWACI_SLEEP_TIME 2       /* After a change, sleep for 2s for hwaci stats refresh */
/* ACI (end) */

/* AvVmid from NVRAM */
#define ACPHY_NUM_BANDS 5
#define ACPHY_AVVMID_NVRAM_PARAMS 2

/* epsdelta for NVRAM */
#define ACPHY_SIZE_EPSDELTA_ARRAY 10

/* hirssi elnabypass */
#define PHY_SW_HIRSSI_UCODE_CAP(pi)	ACMAJORREV_0((pi)->pubpi.phy_rev)
#define PHY_SW_HIRSSI_PERIOD      5    /* 5 second timeout */
#define PHY_SW_HIRSSI_OFF         (-1)
#define PHY_SW_HIRSSI_BYP_THR    (-13)
#define PHY_SW_HIRSSI_RES_THR    (-15)
#define PHY_SW_HIRSSI_W1_BYP_REG  ACPHY_W2W1ClipCnt3(rev)
#define PHY_SW_HIRSSI_W1_BYP_CNT  31
#define PHY_SW_HIRSSI_W1_RES_REG  ACPHY_W2W1ClipCnt1(rev)
#define PHY_SW_HIRSSI_W1_RES_CNT  31

#define ACPHY_TBL_ID_ESTPWRLUTS(core)	\
	(((core == 0) ? ACPHY_TBL_ID_ESTPWRLUTS0 : \
	((core == 1) ? ACPHY_TBL_ID_ESTPWRLUTS1 : ACPHY_TBL_ID_ESTPWRLUTS2)))

#define ACPHY_TBL_ID_CHANEST(core)	\
	(((core == 0) ? ACPHY_TBL_ID_CORE0CHANESTTBL : \
	((core == 1) ? ACPHY_TBL_ID_CORE1CHANESTTBL : ACPHY_TBL_ID_CORE2CHANESTTBL)))

/*
 * ACPHY_ENABLE_FCBS_HWACI  enables  HW ACI detection and
 * HW mitigation thru use of the FCBS
 * This allows for the AGC to be in two states ACI and Normal.
 * This mode of operation is not compatible with the pre-exisiting
 * schemes in particular SW based desense.
 * ACPHY_ENABLE_FCBS_HWACI also disables a selection of existing ACI
 * code.
 */
#ifndef WLC_DISABLE_ACI
#define ACPHY_ENABLE_FCBS_HWACI(pi) (ACMAJORREV_3((pi)->pubpi.phy_rev))
#else
#define ACPHY_ENABLE_FCBS_HWACI(pi) 0
#endif // endif

#ifndef WLC_DISABLE_ACI
/* This Macro is for conditioning HW ACI code on all variants after 4350 c0/c1/c2 */
#define ACHWACIREV(pi) \
		(ACMAJORREV_2((pi)->pubpi.phy_rev) && (ACMINORREV_0((pi)->pubpi.phy_rev) || \
		ACMINORREV_1((pi)->pubpi.phy_rev) || ACMINORREV_4((pi)->pubpi.phy_rev)))

#define AC4354REV(pi) \
		(ACMAJORREV_2((pi)->pubpi.phy_rev) && !ACMINORREV_0((pi)->pubpi.phy_rev) && \
		!ACMINORREV_1((pi)->pubpi.phy_rev) && !ACMINORREV_4((pi)->pubpi.phy_rev))

#else
#define ACHWACIREV(pi) 0
#define AC4354REV(pi) 0

#endif // endif

/*
 * ACPHY Core REV info and mapping to (Major/Minor)
 * http://confluence.broadcom.com/display/WLAN/ACPHY+Major+and+Minor+PHY+Revision+Mapping
 *
 * revid  | chip used | (major, minor) revision
 *  0     : 4360A0/A2 (3x3) | (0, 0)
 *  1     : 4360B0    (3x3) | (0, 1)
 *  2     : 4335A0    (1x1 + low-power improvment, no STBC/antdiv) | (1, 0)
 *  3     : 4350A0/B0 (2x2 + low-power 2x2) | (2, 0)
 *  4     : 4345TC    (1x1 + tiny radio)  | (3, 0)
 *  5     : 4335B0    (1x1, almost the same as rev2, txbf NDP stuck fix) | (1, 1)
 *  6     : 4335C0    (1x1, rev5 + bug fixes + stbc) | (1, 2)
 *  7     : 4345A0    (1x1 + tiny radio, rev4 improvment) | (3, 1)
 *  8     : 4350C0/C1 (2x2 + low-power 2x2) | (2, 1)
 *  9     : 43602A0   (3x3 + MDP for lower power, proprietary 256 QAM for 11n) | (5, 0)
 *  10    : 4349TC2A0
 *  11    : 43457A0   (Based on 4345A0 plus support A4WP (Wireless Charging)
 *  12    : 4349A0    (1x1, Reduced Area Tiny-2 Radio plus channel bonding 80+80 supported)
 *  13    : 4345B0/43457B0	(1x1 + tiny radio, phyrev 7 improvements)
 *  14    : 4350C2    (2x2 + low power) | (2, 4)
 *  15    : 4354A1/43569A0    (2x2 + low-power 2x2) | (2, 3)
 *  18    : 43602A1   (3x3 + MDP for lower power, proprietary 256 QAM for 11n) | (5, 1)
 */

/* Major Revs */
#define ACMAJORREV_5(phy_rev) \
	(ACREV_IS(phy_rev, 9) || ACREV_IS(phy_rev, 18))

#define ACMAJORREV_4(phy_rev) \
	(ACREV_IS(phy_rev, 12))

#define ACMAJORREV_3(phy_rev) \
	(ACREV_IS(phy_rev, 4) || ACREV_IS(phy_rev, 7) || ACREV_IS(phy_rev, 10) || \
	 ACREV_IS(phy_rev, 11) || ACREV_IS(phy_rev, 13))

#define ACMAJORREV_2(phy_rev) \
	(ACREV_IS(phy_rev, 3) || ACREV_IS(phy_rev, 8) || ACREV_IS(phy_rev, 14) || \
	 ACREV_IS(phy_rev, 15) || ACREV_IS(phy_rev, 17))

#define ACMAJORREV_1(phy_rev) \
	(ACREV_IS(phy_rev, 2) || ACREV_IS(phy_rev, 5) || ACREV_IS(phy_rev, 6))

#define ACMAJORREV_0(phy_rev) \
	(ACREV_IS(phy_rev, 0) || ACREV_IS(phy_rev, 1))

/* Minor Revs */
#define ACMINORREV_0(phy_rev) \
	(ACREV_IS(phy_rev, 0) || ACREV_IS(phy_rev, 2) || ACREV_IS(phy_rev, 3) || \
	 ACREV_IS(phy_rev, 4) || ACREV_IS(phy_rev, 9) || ACREV_IS(phy_rev, 10) || \
	 ACREV_IS(phy_rev, 12))

#define ACMINORREV_1(phy_rev) \
	(ACREV_IS(phy_rev, 1) || ACREV_IS(phy_rev, 5) || ACREV_IS(phy_rev, 7) || \
	 ACREV_IS(phy_rev, 8) || ACREV_IS(phy_rev, 18))

#define ACMINORREV_2(phy_rev) \
	(ACREV_IS(phy_rev, 6) || ACREV_IS(phy_rev, 11))

#define ACMINORREV_3(phy_rev) \
	(ACREV_IS(phy_rev, 13) || ACREV_IS(phy_rev, 15))

#define ACMINORREV_4(phy_rev) \
	(ACREV_IS(phy_rev, 14))

#define ACMINORREV_5(phy_rev) \
	(ACREV_IS(phy_rev, 17))

/* 2069 iPA or ePA radio */
/* Check Minor Radio Revid */
#define ACRADIO_2069_EPA_IS(radio_rev_id) \
	((RADIOREV(radio_rev_id) == 2) || (RADIOREV(radio_rev_id) == 3) || \
	 (RADIOREV(radio_rev_id) == 4) || (RADIOREV(radio_rev_id) == 7) || \
	 (RADIOREV(radio_rev_id) == 10) || (RADIOREV(radio_rev_id) == 11) || \
	 (RADIOREV(radio_rev_id) ==  8) || (RADIOREV(radio_rev_id) == 18) || \
	 (RADIOREV(radio_rev_id) == 24) || (RADIOREV(radio_rev_id) == 26) || \
	 (RADIOREV(radio_rev_id) == 34) || (RADIOREV(radio_rev_id) == 36) || \
	 (RADIOREV(radio_rev_id) == 42) || (RADIOREV(radio_rev_id) == 43) || \
	 (RADIOREV(radio_rev_id) == 45))

#ifndef ACPHY_HACK_PWR_STATUS
#define ACPHY_HACK_PWR_STATUS(rxs)	((ltoh16((rxs)->PhyRxStatus_1) & PRXS1_ACPHY_BIT_HACK) >> 3)
#endif // endif
/* Get Rx power on core 0 */
#ifndef ACPHY_RXPWR_ANT0
#define ACPHY_RXPWR_ANT0(rxs)	((ltoh16((rxs)->PhyRxStatus_2) & PRXS2_ACPHY_RXPWR_ANT0) >> 8)
#endif // endif
/* Get Rx power on core 1 */
#ifndef ACPHY_RXPWR_ANT1
#define ACPHY_RXPWR_ANT1(rxs)	(ltoh16((rxs)->PhyRxStatus_3) & PRXS3_ACPHY_RXPWR_ANT1)
#endif // endif
/* Get Rx power on core 2 */
#ifndef ACPHY_RXPWR_ANT2
#define ACPHY_RXPWR_ANT2(rxs)	((ltoh16((rxs)->PhyRxStatus_3) & PRXS3_ACPHY_RXPWR_ANT2) >> 8)
#endif // endif
#ifndef ACPHY_RXPWR_ANT4
#define ACPHY_RXPWR_ANT4(rxs)	(ltoh16((rxs)->PhyRxStatus_4) & PRXS3_ACPHY_RXPWR_ANT4)
#endif // endif

#ifdef ENABLE_FCBS
/* FCBS */
/* time to spinwait while waiting for the FCBS
 * switch trigger bit to go low after FCBS
 */
#define ACPHY_SPINWAIT_FCBS_SWITCH 2000
#define ACPHY_FCBS_PHYTBL16_LEN 400

/* PHY specific on-chip RAM offset of the FCBS cache */
#define FCBS_ACPHY_TMPLRAM_STARTADDR	0x1000

/* PHY specific shmem locations for specifying the length
 * of radio reg cache, phytbl cache, phyreg cache
 */
#define M_FCBS_ACPHY_RADIOREG			0x796
#define M_FCBS_ACPHY_PHYTBL16			0x798
#define M_FCBS_ACPHY_PHYTBL32			0x79a
#define M_FCBS_ACPHY_PHYREG				0x79c
#define M_FCBS_ACPHY_BPHYCTRL			0x79e
#define M_FCBS_ACPHY_TEMPLATE_PTR		0x7a0

typedef struct _acphy_fcbs_info {
	uint16 				phytbl16_buf_ChanA[ACPHY_FCBS_PHYTBL16_LEN];
	uint16 				phytbl16_buf_ChanB[ACPHY_FCBS_PHYTBL16_LEN];
} acphy_fcbs_info;

#endif /* ENABLE_FCBS */

/* 4335C0 LP Mode definitions */
/* "NORMAL_SETTINGS" --> VCO 2.5V + B0's tuning file changes */
/* "LOW_PWR_SETTINGS_1" --> VCO 2.5V + low power settings + tuning file changes */
/* "LOW_PWR_SETTINGS_2" --> VCO 1.35V + low power settings + tuning file changes */

typedef enum {
	ACPHY_LPMODE_NONE = -1,
	ACPHY_LPMODE_NORMAL_SETTINGS,
	ACPHY_LPMODE_LOW_PWR_SETTINGS_1,
	ACPHY_LPMODE_LOW_PWR_SETTINGS_2
} acphy_lp_modes_t;

#define ACPHY_VCO_2P5V	1
#define ACPHY_VCO_1P35V	0

/* Macro to enable clock gating changes in different cores */
#define SAMPLE_SYNC_CLK_BIT 	17

typedef enum {
	ACPHY_LP_CHIP_LVL_OPT,
	ACPHY_LP_PHY_LVL_OPT,
	ACPHY_LP_RADIO_LVL_OPT
} acphy_lp_opt_levels_t;

typedef struct _chan_info_common {
	uint16 chan;		/* channel number */
	uint16 freq;		/* in Mhz */
} chan_info_common_t;

typedef struct _chan_info_radio2069 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RFP_pll_vcocal5;
	uint16 RFP_pll_vcocal6;
	uint16 RFP_pll_vcocal2;
	uint16 RFP_pll_vcocal1;
	uint16 RFP_pll_vcocal11;
	uint16 RFP_pll_vcocal12;
	uint16 RFP_pll_frct2;
	uint16 RFP_pll_frct3;
	uint16 RFP_pll_vcocal10;
	uint16 RFP_pll_xtal3;
	uint16 RFP_pll_vco2;
	uint16 RF0_logen5g_cfg1;
	uint16 RFP_pll_vco8;
	uint16 RFP_pll_vco6;
	uint16 RFP_pll_vco3;
	uint16 RFP_pll_xtalldo1;
	uint16 RFP_pll_hvldo1;
	uint16 RFP_pll_hvldo2;
	uint16 RFP_pll_vco5;
	uint16 RFP_pll_vco4;
	uint16 RFP_pll_lf4;
	uint16 RFP_pll_lf5;
	uint16 RFP_pll_lf7;
	uint16 RFP_pll_lf2;
	uint16 RFP_pll_lf3;
	uint16 RFP_pll_cp4;
	uint16 RFP_pll_dsp1;
	uint16 RFP_pll_dsp2;
	uint16 RFP_pll_dsp3;
	uint16 RFP_pll_dsp4;
	uint16 RFP_pll_dsp6;
	uint16 RFP_pll_dsp7;
	uint16 RFP_pll_dsp8;
	uint16 RFP_pll_dsp9;
	uint16 RF0_logen2g_tune;
	uint16 RFX_lna2g_tune;
	uint16 RFX_txmix2g_cfg1;
	uint16 RFX_pga2g_cfg2;
	uint16 RFX_pad2g_tune;
	uint16 RF0_logen5g_tune1;
	uint16 RF0_logen5g_tune2;
	uint16 RFX_logen5g_rccr;
	uint16 RFX_lna5g_tune;
	uint16 RFX_txmix5g_cfg1;
	uint16 RFX_pga5g_cfg2;
	uint16 RFX_pad5g_tune;
	uint16 RFP_pll_cp5;
	uint16 RF0_afediv1;
	uint16 RF0_afediv2;
	uint16 RFX_adc_cfg5;

	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio2069_t;

typedef struct _chan_info_radio2069revGE16 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RFP_pll_vcocal5;
	uint16 RFP_pll_vcocal6;
	uint16 RFP_pll_vcocal2;
	uint16 RFP_pll_vcocal1;
	uint16 RFP_pll_vcocal11;
	uint16 RFP_pll_vcocal12;
	uint16 RFP_pll_frct2;
	uint16 RFP_pll_frct3;
	uint16 RFP_pll_vcocal10;
	uint16 RFP_pll_xtal3;
	uint16 RFP_pll_vco2;
	uint16 RFP_logen5g_cfg1;
	uint16 RFP_pll_vco8;
	uint16 RFP_pll_vco6;
	uint16 RFP_pll_vco3;
	uint16 RFP_pll_xtalldo1;
	uint16 RFP_pll_hvldo1;
	uint16 RFP_pll_hvldo2;
	uint16 RFP_pll_vco5;
	uint16 RFP_pll_vco4;
	uint16 RFP_pll_lf4;
	uint16 RFP_pll_lf5;
	uint16 RFP_pll_lf7;
	uint16 RFP_pll_lf2;
	uint16 RFP_pll_lf3;
	uint16 RFP_pll_cp4;
	uint16 RFP_pll_lf6;
	uint16 RFP_logen2g_tune;
	uint16 RF0_lna2g_tune;
	uint16 RF0_txmix2g_cfg1;
	uint16 RF0_pga2g_cfg2;
	uint16 RF0_pad2g_tune;
	uint16 RFP_logen5g_tune1;
	uint16 RFP_logen5g_tune2;
	uint16 RF0_logen5g_rccr;
	uint16 RF0_lna5g_tune;
	uint16 RF0_txmix5g_cfg1;
	uint16 RF0_pga5g_cfg2;
	uint16 RF0_pad5g_tune;
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio2069revGE16_t;

typedef struct _chan_info_radio2069revGE25 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RFP_pll_vcocal5;
	uint16 RFP_pll_vcocal6;
	uint16 RFP_pll_vcocal2;
	uint16 RFP_pll_vcocal1;
	uint16 RFP_pll_vcocal11;
	uint16 RFP_pll_vcocal12;
	uint16 RFP_pll_frct2;
	uint16 RFP_pll_frct3;
	uint16 RFP_pll_vcocal10;
	uint16 RFP_pll_xtal3;
	uint16 RFP_pll_cfg3;
	uint16 RFP_pll_vco2;
	uint16 RFP_logen5g_cfg1;
	uint16 RFP_pll_vco8;
	uint16 RFP_pll_vco6;
	uint16 RFP_pll_vco3;
	uint16 RFP_pll_xtalldo1;
	uint16 RFP_pll_hvldo1;
	uint16 RFP_pll_hvldo2;
	uint16 RFP_pll_vco5;
	uint16 RFP_pll_vco4;
	uint16 RFP_pll_lf4;
	uint16 RFP_pll_lf5;
	uint16 RFP_pll_lf7;
	uint16 RFP_pll_lf2;
	uint16 RFP_pll_lf3;
	uint16 RFP_pll_cp4;
	uint16 RFP_pll_lf6;
	uint16 RFP_logen2g_tune;
	uint16 RF0_lna2g_tune;
	uint16 RF0_txmix2g_cfg1;
	uint16 RF0_pga2g_cfg2;
	uint16 RF0_pad2g_tune;
	uint16 RFP_logen5g_tune1;
	uint16 RFP_logen5g_tune2;
	uint16 RF0_logen5g_rccr;
	uint16 RF0_lna5g_tune;
	uint16 RF0_txmix5g_cfg1;
	uint16 RF0_pga5g_cfg2;
	uint16 RF0_pad5g_tune;
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio2069revGE25_t;

typedef struct _chan_info_radio2069revGE25_52MHz {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RFP_pll_vcocal5;
	uint16 RFP_pll_vcocal6;
	uint16 RFP_pll_vcocal2;
	uint16 RFP_pll_vcocal1;
	uint16 RFP_pll_vcocal11;
	uint16 RFP_pll_vcocal12;
	uint16 RFP_pll_frct2;
	uint16 RFP_pll_frct3;
	uint16 RFP_pll_vcocal10;
	uint16 RFP_pll_xtal3;
	uint16 RFP_pll_vco2;
	uint16 RFP_logen5g_cfg1;
	uint16 RFP_pll_vco8;
	uint16 RFP_pll_vco6;
	uint16 RFP_pll_vco3;
	uint16 RFP_pll_xtalldo1;
	uint16 RFP_pll_hvldo1;
	uint16 RFP_pll_hvldo2;
	uint16 RFP_pll_vco5;
	uint16 RFP_pll_vco4;
	uint16 RFP_pll_lf4;
	uint16 RFP_pll_lf5;
	uint16 RFP_pll_lf7;
	uint16 RFP_pll_lf2;
	uint16 RFP_pll_lf3;
	uint16 RFP_pll_cp4;
	uint16 RFP_pll_lf6;
	uint16 RFP_logen2g_tune;
	uint16 RF0_lna2g_tune;
	uint16 RF0_txmix2g_cfg1;
	uint16 RF0_pga2g_cfg2;
	uint16 RF0_pad2g_tune;
	uint16 RFP_logen5g_tune1;
	uint16 RFP_logen5g_tune2;
	uint16 RF0_logen5g_rccr;
	uint16 RF0_lna5g_tune;
	uint16 RF0_txmix5g_cfg1;
	uint16 RF0_pga5g_cfg2;
	uint16 RF0_pad5g_tune;
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio2069revGE25_52MHz_t;

typedef struct _chan_info_radio2069revGE32 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RFP_pll_vcocal5;
	uint16 RFP_pll_vcocal6;
	uint16 RFP_pll_vcocal2;
	uint16 RFP_pll_vcocal1;
	uint16 RFP_pll_vcocal11;
	uint16 RFP_pll_vcocal12;
	uint16 RFP_pll_frct2;
	uint16 RFP_pll_frct3;
	uint16 RFP_pll_vcocal10;
	uint16 RFP_pll_xtal3;
	uint16 RFP_pll_vco2;
	uint16 RFP_logen5g_cfg1;
	uint16 RFP_pll_vco8;
	uint16 RFP_pll_vco6;
	uint16 RFP_pll_vco3;
	uint16 RFP_pll_xtalldo1;
	uint16 RFP_pll_hvldo1;
	uint16 RFP_pll_hvldo2;
	uint16 RFP_pll_vco5;
	uint16 RFP_pll_vco4;
	uint16 RFP_pll_lf4;
	uint16 RFP_pll_lf5;
	uint16 RFP_pll_lf7;
	uint16 RFP_pll_lf2;
	uint16 RFP_pll_lf3;
	uint16 RFP_pll_cp4;
	uint16 RFP_pll_lf6;
	uint16 RFP_pll_xtal4;
	uint16 RFP_logen2g_tune;
	uint16 RFX_lna2g_tune;
	uint16 RFX_txmix2g_cfg1;
	uint16 RFX_pga2g_cfg2;
	uint16 RFX_pad2g_tune;
	uint16 RFP_logen5g_tune1;
	uint16 RFP_logen5g_tune2;
	uint16 RFP_logen5g_idac1;
	uint16 RFX_lna5g_tune;
	uint16 RFX_txmix5g_cfg1;
	uint16 RFX_pga5g_cfg2;
	uint16 RFX_pad5g_tune;
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio2069revGE32_t;

typedef struct acphy_sfo_cfg {
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} acphy_sfo_cfg_t;

typedef struct _chan_info_rx_farrow {
#ifndef ACPHY_1X1_ONLY
	uint8 chan;            /* channel number */
	uint16 freq;           /* in Mhz */
	uint16 deltaphase_lo;
	uint16 deltaphase_hi;
	uint16 drift_period;
	uint16 farrow_ctrl;
#else /* ACPHY_1X1_ONLY */
	uint8 chan;            /* channel number */
	uint16 farrow_ctrl_20_40;
	uint32 deltaphase_20_40;
	uint16 farrow_ctrl_80;
	uint32 deltaphase_80;
#endif /* ACPHY_1X1_ONLY */
} chan_info_rx_farrow;

typedef struct _chan_info_tx_farrow {
#ifdef ACPHY_1X1_ONLY
	uint8 chan;            /* channel number */
	uint32 dac_resamp_fcw;
#else /* ACPHY_1X1_ONLY */
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */
	uint16 MuDelta_l;
	uint16 MuDelta_u;
	uint16 MuDeltaInit_l;
	uint16 MuDeltaInit_u;
#endif /* ACPHY_1X1_ONLY */
} chan_info_tx_farrow;

typedef struct _acphy_txcal_radioregs {
	bool   is_orig;
	uint16 iqcal_cfg1[PHY_CORE_MAX];
	uint16 pa2g_tssi[PHY_CORE_MAX];
	uint16 OVR20[PHY_CORE_MAX];
	uint16 OVR21[PHY_CORE_MAX];
	uint16 tx5g_tssi[PHY_CORE_MAX];
	uint16 iqcal_cfg2[PHY_CORE_MAX];
	uint16 iqcal_cfg3[PHY_CORE_MAX];
	uint16 auxpga_cfg1[PHY_CORE_MAX];
	uint16 iqcal_ovr1[PHY_CORE_MAX];
	uint16 tx_top_5g_ovr1[PHY_CORE_MAX];
	uint16 adc_cfg10[PHY_CORE_MAX];
	uint16 auxpga_ovr1[PHY_CORE_MAX];
	uint16 testbuf_ovr1[PHY_CORE_MAX];
	uint16 spare_cfg6[PHY_CORE_MAX];
	uint16 pa2g_cfg1[PHY_CORE_MAX];
	uint16 adc_ovr1[PHY_CORE_MAX];
	uint16 OVR14[PHY_CORE_MAX];
	uint16 pa5g_cfg1[PHY_CORE_MAX];
} acphy_txcal_radioregs_t;

typedef struct _acphy_txcal_phyregs {
	bool   is_orig;
	uint16 BBConfig;
	uint16 RxFeCtrl1;
	uint16 AfePuCtrl;

	uint16 RfctrlOverrideAfeCfg[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg1[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg2[PHY_CORE_MAX];
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlOverrideRxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreRxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideTxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreTxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGmult[PHY_CORE_MAX];
	uint16 RfctrlCoreRCDACBuf[PHY_CORE_MAX];
	uint16 RfctrlOverrideAuxTssi[PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi1[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
	uint16 RfseqCoreActv2059;
} acphy_txcal_phyregs_t;

typedef struct _acphy_rxcal_radioregs {
	bool   is_orig;
	uint16 RF_2069_TXRX2G_CAL_TX[PHY_CORE_MAX];
	uint16 RF_2069_TXRX5G_CAL_TX[PHY_CORE_MAX];
	uint16 RF_2069_TXRX2G_CAL_RX[PHY_CORE_MAX];
	uint16 RF_2069_TXRX5G_CAL_RX[PHY_CORE_MAX];
	uint16 RF_2069_RXRF2G_CFG2[PHY_CORE_MAX];
	uint16 RF_2069_RXRF5G_CFG2[PHY_CORE_MAX];
	uint16 RF_20691_RFRX_TOP_2G_OVR_EAST[PHY_CORE_MAX];
} acphy_rxcal_radioregs_t;

typedef struct _acphy_rxcal_phyregs {
	bool   is_orig;
	uint16 RfctrlOverrideTxPus [PHY_CORE_MAX];
	uint16 RfctrlCoreTxPus [PHY_CORE_MAX];
	uint16 RfctrlOverrideRxPus [PHY_CORE_MAX];
	uint16 RfctrlCoreRxPus [PHY_CORE_MAX];
	uint16 RfctrlOverrideGains [PHY_CORE_MAX];
	uint16 Dac_gain [PHY_CORE_MAX];
	uint16 forceFront [PHY_CORE_MAX];
	uint16 RfctrlCoreTXGAIN1 [PHY_CORE_MAX];
	uint16 RfctrlCoreTXGAIN2 [PHY_CORE_MAX];
	uint16 RfctrlCoreRXGAIN1 [PHY_CORE_MAX];
	uint16 RfctrlCoreRXGAIN2 [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGain [PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfCT [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGmult [PHY_CORE_MAX];
	uint16 RfctrlCoreRCDACBuf [PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfSwtch [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfSwtch [PHY_CORE_MAX];
	uint16 RfctrlOverrideAfeCfg [PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg1 [PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg2 [PHY_CORE_MAX];
	uint16 RfctrlOverrideLowPwrCfg [PHY_CORE_MAX];
	uint16 RfctrlCoreLowPwr [PHY_CORE_MAX];
	uint16 RfctrlOverrideAuxTssi [PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi1 [PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi2[PHY_CORE_MAX];
	uint16 RfctrlOverrideGlobalPus;
	uint16 RfctrlCoreGlobalPus;
	uint16 RxSdFeConfig1;
	uint16 RxSdFeConfig6;
	uint16 bbmult[PHY_CORE_MAX];
	uint16 rfseq_txgain[3 * PHY_CORE_NUM_4];
	uint16 RfseqCoreActv2059;
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
	uint16 AfePuCtrl;

	uint8 txpwridx[PHY_CORE_MAX];
	uint16 lbFarrowCtrl;
} acphy_rxcal_phyregs_t;

typedef struct _acphy_tempsense_phyregs {
	bool   is_orig;

	uint16 RxFeCtrl1;
	uint16 RxSdFeConfig1;
	uint16 RxSdFeConfig6;
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlOverrideAuxTssi[PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi1[PHY_CORE_MAX];
	uint16 RfctrlOverrideRxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreRxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideTxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreTxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlOverrideGains[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGain[PHY_CORE_MAX];
	uint16 RfctrlOverrideAfeCfg[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg1[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg2[PHY_CORE_MAX];
} acphy_tempsense_phyregs_t;

typedef struct _tempsense_radioregs {
	bool   is_orig;
	uint16 OVR18[PHY_CORE_MAX];
	uint16 OVR19[PHY_CORE_MAX];
	uint16 OVR5[PHY_CORE_MAX];
	uint16 OVR3[PHY_CORE_MAX];
	uint16 tempsense_cfg[PHY_CORE_MAX];
	uint16 testbuf_cfg1[PHY_CORE_MAX];
	uint16 auxpga_cfg1[PHY_CORE_MAX];
	uint16 auxpga_vmid[PHY_CORE_MAX];
} tempsense_radioregs_t;

typedef struct _tempsense_radioregs_tiny {
	uint16 tempsense_cfg[PHY_CORE_MAX];
	uint16 testbuf_cfg1[PHY_CORE_MAX];
	uint16 auxpga_cfg1[PHY_CORE_MAX];
	uint16 auxpga_vmid[PHY_CORE_MAX];
	uint16 tempsense_ovr1[PHY_CORE_MAX];
	uint16 testbuf_ovr1[PHY_CORE_MAX];
	uint16 auxpga_ovr1[PHY_CORE_MAX];
	uint16 tia_cfg9[PHY_CORE_MAX];
	uint16 adc_ovr1[PHY_CORE_MAX];
	uint16 adc_cfg10[PHY_CORE_MAX];
	uint16 tia_cfg5[PHY_CORE_MAX];
	uint16 rx_bb_2g_ovr_east[PHY_CORE_MAX];
	uint16 tia_cfg7[PHY_CORE_MAX];
} tempsense_radioregs_tiny_t;

typedef struct _acphy_tempsense_radioregs
{
	union {
		tempsense_radioregs_t acphy_tempsense_radioregs;
		tempsense_radioregs_tiny_t acphy_tempsense_radioregs_tiny;
	} u;
} acphy_tempsense_radioregs_t;

typedef struct acphy_rx_fdiqi_struct {
	int8 freq;
	int32 angle[PHY_CORE_MAX];
	int32 mag[PHY_CORE_MAX];
} acphy_rx_fdiqi_t;

typedef struct acphy_rx_fdiqi_ctl_struct {
	bool forced;
	uint16 forced_val;
	bool enabled;
	int32 slope[PHY_CORE_MAX];
	uint8 leakage_comp_mode;
} acphy_rx_fdiqi_ctl_t;

typedef struct acphy_iq_mismatch_struct {
	int32 angle;
	int32 mag;
	int32 sin_angle;
} acphy_iq_mismatch_t;

typedef struct {
	uint8 elna;
	uint8 trloss;
	uint8 elna_bypass_tr;
	uint8 lna1byp;
} acphy_fem_rxgains_t;

typedef struct {
	uint32 swctrlmap_2g[ACPHY_SWCTRL_NVRAM_PARAMS];
	uint32 swctrlmapext_2g[ACPHY_SWCTRL_NVRAM_PARAMS];
	uint32 swctrlmap_5g[ACPHY_SWCTRL_NVRAM_PARAMS];
	uint32 swctrlmapext_5g[ACPHY_SWCTRL_NVRAM_PARAMS];
	int8 txswctrlmap_2g;
	uint16 txswctrlmap_2g_mask;
	int8 txswctrlmap_5g;
	int8 init_val_2g;
	int8 init_val_5g;
} acphy_nvram_femctrl_t;

typedef struct {
	int8 rssi_corr_normal[PHY_CORE_MAX][ACPHY_NUM_BW_2G];
	int8 rssi_corr_normal_5g[PHY_CORE_MAX][ACPHY_RSSIOFFSET_NVRAM_PARAMS][ACPHY_NUM_BW];
	int8 rssi_corr_gain_delta_2g[PHY_CORE_MAX][ACPHY_GAIN_DELTA_2G_PARAMS][ACPHY_NUM_BW_2G];
	int8 rssi_corr_gain_delta_2g_sub[PHY_CORE_MAX][ACPHY_GAIN_DELTA_2G_PARAMS_EXT]
	[ACPHY_NUM_BW_2G][CH_2G_GROUP_NEW];
	int8 rssi_corr_gain_delta_5g[PHY_CORE_MAX][ACPHY_GAIN_DELTA_5G_PARAMS][ACPHY_NUM_BW]
	[CH_5G_4BAND];
	int8 rssi_corr_gain_delta_5g_sub[PHY_CORE_MAX][ACPHY_GAIN_DELTA_5G_PARAMS_EXT][ACPHY_NUM_BW]
	[CH_5G_4BAND];
	int8 rssi_tr_offset;
	int8 rssi_corr_pchan_2g_chan[PHY_CORE_MAX][ACPHY_NUM_BW_2G][ACPHY_NUM_CHAN_2G_20];
	int8 rssi_corr_pchan_2g_val[PHY_CORE_MAX][ACPHY_NUM_BW_2G][ACPHY_NUM_CHAN_2G_20];
	int8 rssi_corr_pchan_5g_chan[PHY_CORE_MAX][ACPHY_NUM_BW][ACPHY_NUM_CHAN_5G_20];
	int8 rssi_corr_pchan_5g_val[PHY_CORE_MAX][ACPHY_NUM_BW][ACPHY_NUM_CHAN_5G_20];
} acphy_rssioffset_t;

typedef struct {
	bool elna2g_present, elna5g_present;
	uint8 femctrl, femctrl_sub;
	uint8 dBpad;
	uint8 rfpll_5g;
	uint8 spur_war_enb_2g, spur_war_enb_5g;
	uint8 rcal_war;
	uint8 agc_cfg_2g;
	uint8 agc_cfg_5g;
	uint8 tssi_div_war;
	uint8 txgaintbl_id;
	uint8 gainboosta01;
	uint8 gainboost_core2;
	uint8 epa_on_during_txiqlocal;
	uint8 precal_tx_idx;
	uint8 bt_coex;
	uint8 femctrl_from_nvram;
	uint8 avvmid_from_nvram;
	uint8 vlin_en_from_nvram;
	uint8 ppr_bit_ext;
	uint8 rcal_otp_val_en;
	uint8 bbpll_spr_modes_dis;
	uint16 femctrlmask_2g, femctrlmask_5g;
	uint8 txgaintbl2g_blank;
	uint8 txgaintbl5g_blank;
	uint8 lpmode_2g;
	uint8 lpmode_5g;
	acphy_nvram_femctrl_t nvram_femctrl;
	uint8 phasetrack_max_alphabeta;
	uint8 dac_spur_improve;
	uint8 reduce_pa_turnontime;
	uint8 ltecoex_gaintbl_en;
	acphy_rssioffset_t  rssioffset;
	uint8 rssi_cal_freq_grp[14];
	acphy_fem_rxgains_t femrx_2g[PHY_CORE_MAX];
	acphy_fem_rxgains_t femrx_5g[PHY_CORE_MAX];
	acphy_fem_rxgains_t femrx_5gm[PHY_CORE_MAX];
	acphy_fem_rxgains_t femrx_5gh[PHY_CORE_MAX];
	int16 rxgain_tempadj_2g[PHY_CORE_MAX];
	int16 rxgain_tempadj_2g_sub[PHY_CORE_MAX][CH_2G_GROUP_NEW];
	int16 rxgain_tempadj_5gl[PHY_CORE_MAX];
	int16 rxgain_tempadj_5gml[PHY_CORE_MAX];
	int16 rxgain_tempadj_5gmu[PHY_CORE_MAX];
	int16 rxgain_tempadj_5gh[PHY_CORE_MAX];
	int16 rxgain_tempadj_2g_sub_elnaoff[PHY_CORE_MAX][CH_2G_GROUP_NEW];
	int16 rxgain_tempadj_5gl_elnaoff[PHY_CORE_MAX];
	int16 rxgain_tempadj_5gml_elnaoff[PHY_CORE_MAX];
	int16 rxgain_tempadj_5gmu_elnaoff[PHY_CORE_MAX];
	int16 rxgain_tempadj_5gh_elnaoff[PHY_CORE_MAX];
	bool  apply_trloss_comp_wrt_temp;
	bool  trloss_comp_wrt_temp_applied;
	uint  last_cal_time;
	int16 last_trloss_adj_temp;
	uint  trloss_adj_time_dur;
	int16  trloss_adj_temp_thresh;
	uint8 phy4350_ss_opt;
	int32 ed_thresh2g;
	int32 ed_thresh5g;
	int32 ed_thresh_default;
	uint8 agc_cfg2_2g;
	uint8 agc_cfg2_5g;
	uint32 dot11b_opts;
	uint8  tiny_maxrxgain[3];
	uint8 xtal_spur_suppress;
} acphy_srom_t;

typedef struct acphy_rxgainctrl_params {
	int8  gaintbl[ACPHY_MAX_RX_GAIN_STAGES][12];
	uint8 gainbitstbl[ACPHY_MAX_RX_GAIN_STAGES][12];
} acphy_rxgainctrl_t;

typedef struct acphy_lpfCT_phyregs {
	bool   is_orig;
	uint16 RfctrlOverrideLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT[PHY_CORE_MAX];
} acphy_lpfCT_phyregs_t;

typedef struct acphy_desense_values {
	uint8 clipgain_desense[4]; /* in dBs */
	uint8 ofdm_desense, bphy_desense;      /* in dBs */
	uint8 elna_bypass;
	uint8 lna1_bypass;
	uint8 lna1_tbl_desense, lna2_tbl_desense;   /* in ticks */
	uint8 lna1_gainlmt_desense, lna2_gainlmt_desense;   /* in ticks */
	uint8 lna1rout_gainlmt_desense, lna2rout_gainlmt_desense;
	uint8 lna1_rout, lna2_rout;
	uint8 mixer_setting_desense; /* 0 = no override */
	uint8 biq0_tbl_desense;	     /* in ticks */
	uint8 biqs_maxgain;		     /* CoreX_BiQuad_MaxGain, normal 24, 0 = no override */
	uint8 nf_hit_lna12;          /* (mostly to adjust nb/w1 clip for bt cases */
	bool on;
	bool forced;
}  acphy_desense_values_t;

typedef struct acphy_btcx_hybrid_mode_simul_rx {
	uint8 shared_core_lna1_idx;
	uint8 shared_core_lna2_max_idx;
	uint8 shared_core_elna_bypass;
	uint8 shared_core;
	uint8 mode;
	bool on;
}  acphy_btcx_hybrid_mode_simul_rx_t;

typedef struct desense_history {
	uint32 glitches[ACPHY_ACI_GLITCH_BUFFER_SZ];
	uint8 hi_glitch_dB;
	uint8 lo_glitch_dB;
	uint8 no_desense_change_time_cnt;
} desense_history_t;

typedef struct acphy_aci_params {
	/* array is indexed by chan/bw */
	uint8 chan;
	uint16 bw;
	uint64 last_updated;

	acphy_desense_values_t desense;
	int8 weakest_rssi;

	desense_history_t bphy_hist;
	desense_history_t ofdm_hist;
	uint8 glitch_buff_idx, glitch_upd_wait;

	uint8 hwaci_setup_state, hwaci_desense_state;
	uint8 hwaci_noaci_timer;
	uint8 hwaci_sleep;
	int  hwaci_desense_state_ovr;
	bool engine_called;
} acphy_aci_params_t;

typedef struct {
	uint16 sample_time;
	uint16 energy_thresh;
	uint16 detect_thresh;
	uint16 wait_period;
	uint8 sliding_window;
	uint8 samp_cluster;
	uint8 nb_lo_th;
	uint8 w3_lo_th;
	uint8 w3_md_th;
	uint8 w3_hi_th;
	uint8 w2;
	uint16 energy_thresh_w2;
	uint8 aci_present_th;
	uint8 aci_present_select;
} acphy_hwaci_setup_t;

typedef struct
{
	uint16 energy_thresh;
	uint8 lna1_pktg_lmt, lna2_pktg_lmt, lna1rout_pktg_lmt;
	uint8 w2_sel, w2_thresh, nb_thresh;
	uint16 energy_thresh_w2;
	uint8 lna2rout_pktg_lmt;
} acphy_hwaci_state_t;

typedef struct {
	/* Saving the default gain settings in these variables */
	uint8 lna1_gainlim_ofdm_2g_def[6];
	uint8 lna1_gainlim_cck_2g_def[6];
	uint8 lna1_rout_2g_def[6];
	uint8 lna1_gaindb_2g_def[6];
	uint8 lna1_gainbits_2g_def[6];

	uint8 lna2_gainlim_ofdm_2g_def[6];
	uint8 lna2_gainlim_cck_2g_def[6];
	uint8 lna2_rout_2g_def[6];
	uint8 lna2_gainbits_2g_def[6];
	uint8 lna2_gaindb_2g_def[6];

	uint8 lna1_gainlim_5g_def[6];
	uint8 lna1_rout_5g_def[6];
	uint8 lna1_gaindb_5g_def[6];
	uint8 lna1_gainbits_5g_def[6];

	uint8 lna2_gainlim_5g_def[6];
	uint8 lna2_gaindb_5g_def[6];
	uint8 lna2_gainbits_5g_def[6];

} acphy_hwaci_defgain_settings_t;

#ifdef PREASSOC_PWRCTRL
typedef struct phy_pwr_ctrl_save_acphy {
	bool status_idx_carry_2g[PHY_CORE_MAX];
	bool status_idx_carry_5g[PHY_CORE_MAX];
	uint8 status_idx_2g[PHY_CORE_MAX];
	uint8 status_idx_5g[PHY_CORE_MAX];
	uint16 last_chan_stored_2g;
	uint16 last_chan_stored_5g;
	int8   pwr_qdbm_2g[PHY_CORE_MAX];
	int8   pwr_qdbm_5g[PHY_CORE_MAX];
	bool   stored_not_restored_2g[PHY_CORE_MAX];
	bool   stored_not_restored_5g[PHY_CORE_MAX];

} phy_pwr_ctrl_s;
#endif // endif

/* Monitor for the Modified Entries - nvshapingtbl */
typedef struct _acphy_nshapetbl_mon {
	uint8 mod_flag;
	uint8 offset[ACPHY_SPURWAR_NV_NTONES];
} acphy_nshapetbl_mon_t;

typedef struct acphy_dssfB_values {
	uint16 channel;
	uint8 core;
	uint8 DSSFB_gain_th0_s1;
	uint8 DSSFB_gain_th1_s1;
	uint8 DSSFB_gain_th2_s1;
	uint8 DSSFB_gain_th0_s2;
	uint8 DSSFB_gain_th1_s2;
	uint8 DSSFB_gain_th2_s2;
	uint8 idepth_s1;
	uint8 idepth_s2;
	uint8 enabled_s1;
	uint8 enabled_s2;
	uint16 theta_i_s1;
	uint16 theta_q_s1;
	uint16 theta_i_s2;
	uint16 theta_q_s2;
	uint8 DSSFB_C_CTRL;
	bool on;

}  acphy_dssfB_values_t;

typedef struct acphy_dssf_values {
	uint16 channel;
	uint8 core;
	uint8 DSSF_gain_th0_s1;
	uint8 DSSF_gain_th1_s1;
	uint8 DSSF_gain_th2_s1;
	uint8 DSSF_gain_th0_s2;
	uint8 DSSF_gain_th1_s2;
	uint8 DSSF_gain_th2_s2;
	uint8 idepth_s1;
	uint8 idepth_s2;
	uint8 enabled_s1;
	uint8 enabled_s2;
	uint16 theta_i_s1;
	uint16 theta_q_s1;
	uint16 theta_i_s2;
	uint16 theta_q_s2;
	uint8 DSSF_C_CTRL;
	bool on;

}  acphy_dssf_values_t;

/**
 * Front End Control table contents for a single radio core. For some chips, the per-core tables are
 * not of equal size. A single table in PHYHW contains a 'subtable' per radio core. This structure
 * is not used in the case of 'sparse' tables.
 */
typedef struct acphy_fe_ctrl_table {
	uint8 *subtable;	/* containing values to copy into PHY hardware table */
	uint32 n_entries;	/* number of entries in the subtable */
	uint32 hw_offset;	/* index to write to in the HW table */
} acphy_fe_ctrl_table_t;

/**
 * PAPD temperature compensation
 */
typedef struct {
	int16  papd_ref_tssi[PHY_CORE_MAX]; /* TSSI reference at fullcal temperature */
	int8   papd_tempcomp[PHY_CORE_MAX]; /* Temperature dependent EPS offset */
	int16  last_tempcomp_temp;          /* Last temperature when tempcomp or fullcal was run */
	uint8  papdtempcomp_tempdelta;      /* Temperature delta to trigger tempcomp */
} acphy_cal_tempcomp_t;

struct phy_info_acphy {
	uint8  dac_mode;
	uint16 dac_rate;
	bool   logenmode43;
	uint16 bb_mult_save[PHY_CORE_MAX];
	uint8  bb_mult_save_valid;
	uint16 deaf_count;
	uint16 saved_bbconf;
	int8   txpwrindex[PHY_CORE_MAX]; 		/* index if hwpwrctrl if OFF */
	int8   phy_noise_all_core[PHY_CORE_MAX]; /* noise power in dB for all cores */
	int8   phy_noise_cache_crsmin[5][PHY_CORE_MAX]; /* cache of noise powers */
	int8   phy_noise_pwr_array[PHY_SIZE_NOISE_ARRAY][PHY_CORE_MAX];
	int8   phy_noise_counter;  /* Dummy variable for noise averaging */
	int8   phy_noise_in_crs_min[PHY_CORE_MAX]; /* noise power in dB for all cores */
	bool   trigger_crsmin_cal; /* crsmin cal to be triggered during next noise_sample_intr */
	uint8  phy_crs_th_from_crs_cal;
	uint8  phy_debug_crscal_counter;
	uint8  phy_debug_crscal_channel;
	acphy_txcal_radioregs_t ac_txcal_radioregs_orig;
	acphy_txcal_phyregs_t   ac_txcal_phyregs_orig;
	acphy_rxcal_radioregs_t ac_rxcal_radioregs_orig;
	acphy_rxcal_phyregs_t   ac_rxcal_phyregs_orig;
	acphy_tempsense_radioregs_t ac_tempsense_radioregs_orig;
	acphy_tempsense_phyregs_t   ac_tempsense_phyregs_orig;
	acphy_lpfCT_phyregs_t 	ac_lpfCT_phyregs_orig;
	uint32 pstart; /* sample collect fifo begins */
	uint32 pstop;  /* sample collect fifo ends */
	uint32 pfirst; /* sample collect trigger begins */
	uint32 plast;  /* sample collect trigger ends */
#ifdef BCMLTECOEX
	int8 ltecx_elna_bypass_status;
#endif // endif

#if defined(BCMDBG_RXCAL)
	phy_iq_est_t  rxcal_noise[PHY_CORE_MAX];
	phy_iq_est_t  rxcal_signal[PHY_CORE_MAX];
#endif // endif
	uint8 txpwridx_for_rxiqcal[PHY_CORE_MAX];

	bool init;
	bool init_done;
	int8 bt_sw_state;
	uint8 curr_band2g;
	uint8 band2g_init_done;
	uint8 band5g_init_done;
	uint8 prev_subband;
	uint8 curr_subband;
	int bbmult_comp;
	uint8 vlin_txidx;
	uint32 curr_bw;
	uint8  curr_spurmode;
	/* result of radio rccal */
	uint16 rccal_gmult;
	uint16 rccal_gmult_rc;
	uint8 rccal_dacbuf;
	uint16 rccal_adc_gmult;

	/* Flag for enabling auto crsminpower cal */
	bool crsmincal_enable;
	bool force_crsmincal;
	uint8  crsmincal_run;

	/* ACPHY FEM value from SROM */
	acphy_srom_t srom;

	/* pdet_range_id */
	uint8 srom_2g_pdrange_id;
	uint8 srom_5g_pdrange_id;

	/*  2 range tssi */
	bool srom_tworangetssi2g;
	bool srom_tworangetssi5g;

	/*  papr disable */
	bool srom_paprdis;

	/*  papd war enable and TH */
	int8 srom_papdwar;

	/*  dpd cal settings */
	uint8 srom_edpdcalset;

	/* epsdelta array */
	int16 srom_epsdelta2g[PHY_CORE_MAX][ACPHY_SIZE_EPSDELTA_ARRAY];

	/*  low range tssi */
	bool srom_lowpowerrange2g;
	bool srom_lowpowerrange5g;

	/*  iPa Pa gain override */
	uint8 srom_pagc2g;
	uint8 srom_pagc2g_ovr;
	uint8 srom_pagc5g;
	uint8 srom_pagc5g_ovr;

	txcal_coeffs_t txcal_cache[PHY_CORE_MAX];
	uint16	txcal_cache_cookie;
	uint8   radar_cal_active; /* to mask radar detect during cal's tone-play */

	int16 idle_tssi[PHY_CORE_MAX];
	int8  txpwr_offset[PHY_CORE_MAX];	/* qdBm signed offset for per-core tx pwr */
	uint8 txpwrindex_hw_save[PHY_CORE_MAX]; /* txpwr start index for hwpwrctrl */
	uint8 txpwrindex_cck_hw_save[PHY_CORE_MAX]; /* txpwr start index for hwpwrctrl */

	/* TSSI sleep enable */
	uint8 srom_tssisleep_en;

	/* PAPD related */

	bool    acphy_papd_kill_switch_en; /* flag to indicate if lna kill switch is enabled */
	bool    acphy_force_papd_cal;
	uint16  acphy_papd_tx_gain_at_last_cal[2]; /* Tx gain index at time of last papd cal */
	uint    acphy_papd_last_cal;     /* time of last papd cal */
	uint32  acphy_papd_recal_counter;
	uint8   papdmode;
	uint8	fastpapdgainctrl;
	   /* Tx gain pga index used during last papd cal
		* For REVs>=7, the PAD index is stored in the
		* 2G band and the PGA index is stored in the
		* 5G band
*/
	uint8   acphy_papd_cal_gain_index[2];
	bool    acphy_papdcomp;
	int16   acphy_papd_epsilon_offset[2];
	uint8	acphy_papd_skip;     /* skip papd calibration for IPA case */
	uint8   acphy_txpwr_idx_2G[3];    /* to store the power control txpwr index for 2G band */
	uint8   acphy_txpwr_idx_5G[3];    /* to store the power control txpwr index for 5G band */

	int8 	papd_lut0_cal_idx;  /* PAPD index for lut0 */
	int8 	papd_lut1_cal_idx;  /* PAPD index for lut1 */
	int8 	pacalidx_iovar;       /* to force papd cal index */

	/* rx gainctrl */
	acphy_fem_rxgains_t fem_rxgains[PHY_CORE_MAX];
	acphy_rxgainctrl_t rxgainctrl_params[PHY_CORE_MAX];
	uint8 rxgainctrl_stage_len[ACPHY_MAX_RX_GAIN_STAGES];
	int16 rxgainctrl_maxout_gains[ACPHY_MAX_RX_GAIN_STAGES];
	int8  lna2_complete_gaintbl[12];

	/* desense */
	acphy_desense_values_t curr_desense, zero_desense, total_desense;
	bool limit_desense_on_rssi;

#ifndef WLC_DISABLE_ACI
	/* aci (aci, cci, noise) */
	acphy_hwaci_setup_t hwaci_args;
	acphy_hwaci_state_t hwaci_states_2g[ACPHY_HWACI_MAX_STATES];
	acphy_hwaci_state_t hwaci_states_5g[ACPHY_HWACI_MAX_STATES];
	uint8 hwaci_max_states_2g, hwaci_max_states_5g;
#endif /* !WLC_DISABLE_ACI */
	bool hw_aci_status;
	acphy_aci_params_t aci_list2g[ACPHY_ACI_CHAN_LIST_SZ];
	acphy_aci_params_t aci_list5g[ACPHY_ACI_CHAN_LIST_SZ];
	acphy_aci_params_t *aci;

	/* bt */
	int32 btc_mode;
	acphy_desense_values_t bt_desense;
	acphy_btcx_hybrid_mode_simul_rx_t btcx_hybrid_mode_simul_rx;

#ifdef BCMLTECOEX
	int32 ltecx_mode;
	acphy_desense_values_t lte_desense;
	int32 ltecx_ed_thresh;
#endif // endif

	/* nvnoiseshapingtbl monitor */
	acphy_nshapetbl_mon_t nshapetbl_mon;

	acphy_rx_fdiqi_ctl_t fdiqi;

	int16 current_temperature;

	bool poll_adc_WAR;

	/* VLIN RELATED */
	uint8 vlinpwr2g_from_nvram;
	uint8 vlinpwr5g_from_nvram;
	uint16 vlinmask2g_from_nvram;
	uint16 vlinmask5g_from_nvram;

	#ifdef ENABLE_FCBS
	acphy_fcbs_info ac_fcbs;
	#endif /* ENABLE_FCBS */

	uint16 rfldo;
	uint8 acphy_lp_mode;	/* To select the low pweor mode */
	acphy_lp_modes_t lpmode_2g;
	acphy_lp_modes_t lpmode_5g;

	uint8 acphy_force_lpvco_2G;
	uint8 acphy_prev_lp_mode;
	uint8 acphy_lp_status;
	uint8 acphy_enable_smth;
	uint8 acphy_smth_dump_mode;
	uint8 acphy_4335_radio_pd_status;
	uint16 rxRfctrlCoreRxPus0, rxRfctrlOverrideRxPus0;
	uint16 afeRfctrlCoreAfeCfg10, afeRfctrlCoreAfeCfg20, afeRfctrlOverrideAfeCfg0;
	uint16 txRfctrlCoreTxPus0, txRfctrlOverrideTxPus0;
	uint16 radioRfctrlCmd, radioRfctrlCoreGlobalPus, radioRfctrlOverrideGlobalPus;
	int8 acphy_cck_dig_filt_type;
	uint16 AfePuCtrl;
	bool   ac_rxldpc_override;	/* LDPC override for RX, both band */

	bool rxiqcal_percore_2g, rxiqcal_percore_5g;

	uint16 clip1_th, edcrs_en;

	/* hirssi elna bypass */
	bool hirssi_en;
	uint16 hirssi_period, hirssi_byp_cnt, hirssi_res_cnt;
	int8 hirssi_byp_rssi, hirssi_res_rssi;
	bool hirssi_elnabyp2g_en, hirssi_elnabyp5g_en;
	int16 hirssi_timer2g, hirssi_timer5g;
	uint8 rssi_coresel;

#ifdef PREASSOC_PWRCTRL
	phy_pwr_ctrl_s pwr_ctrl_save;
#endif // endif
	/* dssf */
	acphy_dssf_values_t dssf;
	acphy_dssfB_values_t dssfB;

	/* target offset power (in qDb) */
	uint16 offset_targetpwr;
	/* Base index cache for band */
	int8	txpwr_idx_band_cache[PHY_CORE_MAX];
	int8	txpwr_idx_band_cck_cache[PHY_CORE_MAX];
	/* farrow tables */
	chan_info_tx_farrow(*tx_farrow)[ACPHY_NUM_CHANS];
	chan_info_rx_farrow(*rx_farrow)[ACPHY_NUM_CHANS];
	/* chan_tuning used only when RADIOMAJORREV(pi->pubpi.radiomajorrev)==2 */
	void *chan_tuning;
	uint32 chan_tuning_tbl_len;
	int8 pa_mode; /* Modes: High Efficiency, High Linearity */

	/* FEMvtrl table sparse representation: index,value */
	/* fectrl_sparse_table_len - length of sparse array */
	/* fectrl_table_len - hw table length */
	uint16 *fectrl_idx, *fectrl_val, fectrl_table_len, fectrl_sparse_table_len;
	uint8 fectrl_spl_entry_flag;
	acphy_fe_ctrl_table_t fectrl_c[PHY_CORE_MAX]; /* front end control table per core */
	bool mdgain_trtx_allowed;
	int8 rx_elna_bypass_gain_th;
	uint16 initGain_codeA, initGain_codeB;
	bool rxgaincal_rssical;	/* 0 = rxgain error cal and 1 = RSSI error cal */
	bool rssi_cal_rev; /* 0 = OLD ad 1 = NEW */
	bool rud_agc_enable;
	bool rssi_qdB_en; /* 0 = dqB reporting of RSSI is disabled */
	bool temp_comp_tr_loss;	/* 0 = Disable and 1 = Enable */
	int last_rssi;

	uint8 ant_swOvr_state_core0;
	uint8 ant_swOvr_state_core1;
	uint8 antdiv_rfswctrlpin_a0;
	uint8 antdiv_rfswctrlpin_a1;

#ifdef WL_PROXDETECT
	bool tof_active;
	bool tof_setup_done;
	bool tof_tx;
	uint16 tof_shm_ptr;
	uint16 tof_rfseq_bundle_offset;
	uint8  tof_core;
	bool tof_smth_forced;
	uint8 tof_rx_fdiqcomp_enable;
	uint8 tof_tx_fdiqcomp_enable;
	int8 tof_smth_enable;
	int8 tof_smth_dump_mode;
#endif // endif

	int8 paparambwver;

	uint16 *gaintbl_2g;
	uint16 *gaintbl_5g;

#if defined(WLC_TXPWRCAP)
	/* Tx Power cap vars */
	bool txpwrcap_cellstatus;
	wl_txpwrcap_tbl_t txpwrcap_tbl;
#endif /* WLC_TXPWRCAP */

#ifdef WLC_SW_DIVERSITY
	bool swdiv_enable;
	uint8 swdiv_gpio_num;
	wlc_swdiv_swctrl_t swdiv_swctrl_en;
	uint16 swdiv_swctrl_mask;
	uint16 swdiv_swctrl_ant0;
	uint16 swdiv_swctrl_ant1;
#endif /* WLC_SW_DIVERSITY */
#ifndef WLC_DISABLE_ACI
	acphy_hwaci_defgain_settings_t *def_gains;
#endif /* !WLC_DISABLE_ACI */
	int8 btswitch; /* bt switch state (-1: AUTO, 0: WL, 1: BT) */

	/* 5G Tx spur optimization */
	uint8 srom_txnospurmod5g;

	/* 2G Tx spur optimization */
	uint8 srom_txnospurmod2g;
	bool LTEJ_WAR_en; /* enable/disable LTEJ WAR for 4345 */
	uint16 pktabortctl; /* store phyreg(PktAbortCtrl) for dyn phy preemption */
	uint8 rx5ggainwar; /* gain boost WAR for 4350 Stella modules */
	uint8 ldo3p3_2g; /* Program LDO value via NVRAM */
	uint8 ldo3p3_5g; /* Program LDO value via NVRAM */

	/* phy_cal_xtal_spur */
	uint16 cal_avg_noise;
	uint16 cal_spur_lvl;

	/* Calibration temperature compensation */
	acphy_cal_tempcomp_t *cal_tempcomp;
#if defined(WLOLPC) || defined(BCMDBG) || defined(WLTEST)
	bool olpc_dbg_mode; /* Indicate OLPC cal stature during dbg mode */
#endif // endif
#if defined(BCMDBG_DUMP) || defined(BCMDBG)
	uint16 acphy_papd_mix_ovr[2];
	uint16 acphy_papd_attn_ovr[2];
	uint16 acphy_papd_pga_settled[2];
#endif /* (BCMDBG_DUMP) || (BCMDBG) */

#if (defined(WLTEST) || defined(WLPKTENG))
	/* Per Rate DPD related params */
	bool	perratedpd2g;
	bool	perratedpd5g;
#endif // endif
#if defined(WLTEST)
	uint16 rxstats[NUM_80211_RATES + 1];
#endif // endif
#if defined(WLTEST)
	/* Switch control override values */
	uint32 swctrlmap_entry;
	uint32 swctrlmap_mask;
#endif // endif
#if (defined(WLTEST) || defined(ACMAJORREV2_THROUGHPUT_OPT))
	/* Xtal LDO radio setting */
	uint8 xtalldo;
	bool en_xtalldowar_2069;
#endif // endif
};

#if     defined(WLOFFLD) || defined(BCM_OL_DEV)
int8 wlc_phy_noise_sample_acphy(wlc_phy_t *pih);
void wlc_phy_noise_reset_ma_acphy(wlc_phy_t *pih);
#endif /* defined(WLOFFLD) || defined(BCM_OL_DEV) */

/*
 * Masks for PA mode selection of linear
 * vs. high efficiency modes.
 */

#define PAMODE_HI_LIN_MASK		0x0000FFFF
#define PAMODE_HI_EFF_MASK		0xFFFF0000

typedef enum {
	PAMODE_HI_LIN = 0,
	PAMODE_HI_EFF
} acphy_swctrl_pa_modes_t;

typedef enum {
	ACPHY_TEMPSENSE_VBG = 0,
	ACPHY_TEMPSENSE_VBE = 1
} acphy_tempsense_cfg_opt_t;

uint8 wlc_phy_tssi2dbm_acphy(phy_info_t *pi, int32 tssi, int32 a1, int32 b0, int32 b1);
void wlc_phy_get_paparams_for_band_acphy(phy_info_t *pi, int16 *a1, int16 *b0, int16 *b1);

#ifdef WLC_TXCAL
uint16 wlc_phy_adjusted_tssi_acphy(phy_info_t *pi, uint8 core_num);
uint8 wlc_phy_apply_pwr_tssi_tble_chan_acphy(phy_info_t *pi);
uint8
wlc_phy_estpwrlut_intpol_acphy(phy_info_t *pi, uint8 channel,
        wl_txcal_power_tssi_t *pwr_tssi_lut_ch1, wl_txcal_power_tssi_t *pwr_tssi_lut_ch2);
void wlc_phy_set_olpc_anchor_acphy(phy_info_t *pi);
#endif // endif

#if defined(WLC_TXPWRCAP)
bool wlc_phy_txpwrcap_get_cellstatus_acphy(phy_info_t *pi);
void wlc_phy_txpwrcap_set_cellstatus_acphy(phy_info_t *pi, int mask, int value);
int wlc_phy_txpwrcap_tbl_get_acphy(phy_info_t *pi,
	wl_txpwrcap_tbl_t *txpwrcap_tbl);
int wlc_phy_txpwrcap_tbl_set_acphy(phy_info_t *pi,
	wl_txpwrcap_tbl_t *txpwrcap_tbl);
uint32 wlc_phy_get_txpwrcap_inuse_acphy(phy_info_t *pi);
#endif // endif

#ifdef WLC_SW_DIVERSITY
extern bool wlc_phy_swdiv_ant_set_acphy(phy_info_t *pi, uint8 new_ant);
extern bool wlc_phy_swdiv_ant_get_acphy(phy_info_t *pi, uint8 *cur_ant);
#endif // endif

#if defined(WLTEST) || defined(WLMEDIA_N2DBG) || defined(WLMEDIA_N2DEV) || \
	defined(DBG_PHY_IOV) || defined(WFD_PHY_LL_DEBUG)
extern void wlc_phy_noisecal_run_acphy(phy_info_t *pi);
#endif // endif

int32 acphy_get_rxgain_index(phy_info_t *pi, uint8 *index);
int32 acphy_set_rxgain_index(phy_info_t *pi, int index);

#ifdef PHY_XTAL_SPUR_CAL
#if defined(WLTEST)
int wlc_acphy_do_nmos_pmos(phy_info_t *pi, void *pparams);
#endif // endif
#endif /* PHY_XTAL_SPUR_CAL */
void wlc_phy_papd_tempcomp_trigger_acphy(phy_info_t *pi, int16 currtemp);

#if defined(AP) && defined(RADAR)
void wlc_phy_radar_detect_iir_war_acphy(phy_info_t *pi);
#endif // endif
#endif /* _wlc_phy_ac_h_ */
