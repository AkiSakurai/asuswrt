/*
 * Common [OS-independent] rate management
 * 802.11 Networking Adapter Device Driver.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_rate_def.c 467328 2014-04-03 01:23:40Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <proto/802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>

/* Rate info per rate: It tells whether a rate is ofdm or not and its phy_rate value */
const uint8 rate_info[WLC_MAXRATE + 1] = {
	/*  0     1     2     3     4     5     6     7     8     9 */
/*   0 */ 0x00, 0x00, 0x0a, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  10 */ 0x00, 0x37, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8f, 0x00,
/*  20 */ 0x00, 0x00, 0x6e, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  30 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8e, 0x00, 0x00, 0x00,
/*  40 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x00,
/*  50 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  60 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  70 */ 0x00, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  80 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  90 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
/* 100 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8c
};

/* rates are in units of Kbps */
const mcs_info_t mcs_table[MCS_TABLE_SIZE] = {
	/* MCS  0: SS 1, MOD: BPSK,  CR 1/2 */
	{6500,   13500,  CEIL(6500*10, 9),   CEIL(13500*10, 9),  0x00, WLC_RATE_6M},
	/* MCS  1: SS 1, MOD: QPSK,  CR 1/2 */
	{13000,  27000,  CEIL(13000*10, 9),  CEIL(27000*10, 9),  0x08, WLC_RATE_12M},
	/* MCS  2: SS 1, MOD: QPSK,  CR 3/4 */
	{19500,  40500,  CEIL(19500*10, 9),  CEIL(40500*10, 9),  0x0A, WLC_RATE_18M},
	/* MCS  3: SS 1, MOD: 16QAM, CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9),  0x10, WLC_RATE_24M},
	/* MCS  4: SS 1, MOD: 16QAM, CR 3/4 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x12, WLC_RATE_36M},
	/* MCS  5: SS 1, MOD: 64QAM, CR 2/3 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0x19, WLC_RATE_48M},
	/* MCS  6: SS 1, MOD: 64QAM, CR 3/4 */
	{58500,  121500, CEIL(58500*10, 9),  CEIL(121500*10, 9), 0x1A, WLC_RATE_54M},
	/* MCS  7: SS 1, MOD: 64QAM, CR 5/6 */
	{65000,  135000, CEIL(65000*10, 9),  CEIL(135000*10, 9), 0x1C, WLC_RATE_54M},
	/* MCS  8: SS 2, MOD: BPSK,  CR 1/2 */
	{13000,  27000,  CEIL(13000*10, 9),  CEIL(27000*10, 9),  0x40, WLC_RATE_6M},
	/* MCS  9: SS 2, MOD: QPSK,  CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9),  0x48, WLC_RATE_12M},
	/* MCS 10: SS 2, MOD: QPSK,  CR 3/4 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x4A, WLC_RATE_18M},
	/* MCS 11: SS 2, MOD: 16QAM, CR 1/2 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0x50, WLC_RATE_24M},
	/* MCS 12: SS 2, MOD: 16QAM, CR 3/4 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0x52, WLC_RATE_36M},
	/* MCS 13: SS 2, MOD: 64QAM, CR 2/3 */
	{104000, 216000, CEIL(104000*10, 9), CEIL(216000*10, 9), 0x59, WLC_RATE_48M},
	/* MCS 14: SS 2, MOD: 64QAM, CR 3/4 */
	{117000, 243000, CEIL(117000*10, 9), CEIL(243000*10, 9), 0x5A, WLC_RATE_54M},
	/* MCS 15: SS 2, MOD: 64QAM, CR 5/6 */
	{130000, 270000, CEIL(130000*10, 9), CEIL(270000*10, 9), 0x5C, WLC_RATE_54M},
	/* MCS 16: SS 3, MOD: BPSK,  CR 1/2 */
	{19500,  40500,  CEIL(19500*10, 9),  CEIL(40500*10, 9),  0x80, WLC_RATE_6M},
	/* MCS 17: SS 3, MOD: QPSK,  CR 1/2 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x88, WLC_RATE_12M},
	/* MCS 18: SS 3, MOD: QPSK,  CR 3/4 */
	{58500,  121500, CEIL(58500*10, 9),  CEIL(121500*10, 9), 0x8A, WLC_RATE_18M},
	/* MCS 19: SS 3, MOD: 16QAM, CR 1/2 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0x90, WLC_RATE_24M},
	/* MCS 20: SS 3, MOD: 16QAM, CR 3/4 */
	{117000, 243000, CEIL(117000*10, 9), CEIL(243000*10, 9), 0x92, WLC_RATE_36M},
	/* MCS 21: SS 3, MOD: 64QAM, CR 2/3 */
	{156000, 324000, CEIL(156000*10, 9), CEIL(324000*10, 9), 0x99, WLC_RATE_48M},
	/* MCS 22: SS 3, MOD: 64QAM, CR 3/4 */
	{175500, 364500, CEIL(175500*10, 9), CEIL(364500*10, 9), 0x9A, WLC_RATE_54M},
	/* MCS 23: SS 3, MOD: 64QAM, CR 5/6 */
	{195000, 405000, CEIL(195000*10, 9), CEIL(405000*10, 9), 0x9B, WLC_RATE_54M},
	/* MCS 24: SS 4, MOD: BPSK,  CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9), 0xC0, WLC_RATE_6M},
	/* MCS 25: SS 4, MOD: QPSK,  CR 1/2 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0xC8, WLC_RATE_12M},
	/* MCS 26: SS 4, MOD: QPSK,  CR 3/4 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0xCA, WLC_RATE_18M},
	/* MCS 27: SS 4, MOD: 16QAM, CR 1/2 */
	{104000, 216000, CEIL(104000*10, 9), CEIL(216000*10, 9), 0xD0, WLC_RATE_24M},
	/* MCS 28: SS 4, MOD: 16QAM, CR 3/4 */
	{156000, 324000, CEIL(156000*10, 9), CEIL(324000*10, 9), 0xD2, WLC_RATE_36M},
	/* MCS 29: SS 4, MOD: 64QAM, CR 2/3 */
	{208000, 432000, CEIL(208000*10, 9), CEIL(432000*10, 9), 0xD9, WLC_RATE_48M},
	/* MCS 30: SS 4, MOD: 64QAM, CR 3/4 */
	{234000, 486000, CEIL(234000*10, 9), CEIL(486000*10, 9), 0xDA, WLC_RATE_54M},
	/* MCS 31: SS 4, MOD: 64QAM, CR 5/6 */
	{260000, 540000, CEIL(260000*10, 9), CEIL(540000*10, 9), 0xDB, WLC_RATE_54M},
	/* MCS 32: SS 1, MOD: BPSK,  CR 1/2 */
	{0,      6000,   0, CEIL(6000*10, 9), 0x00, WLC_RATE_6M},
};

/* Hardware rates (also encodes default basic rates) */
const wlc_rateset_t cck_ofdm_mimo_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

const wlc_rateset_t ofdm_mimo_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{       0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

/* Default ratesets that include MCS32 for 40BW channels */
const wlc_rateset_t cck_ofdm_40bw_mimo_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

const wlc_rateset_t ofdm_40bw_mimo_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

const wlc_rateset_t cck_ofdm_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

const wlc_rateset_t gphy_legacy_rates = {
	4,
	{ /*	1b,   2b,   5.5b,  11b Mbps */
		0x82, 0x84, 0x8b, 0x96
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

const wlc_rateset_t ofdm_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

const wlc_rateset_t cck_rates = {
	4,
	{ /*	1b,   2b,   5.5,  11 Mbps */
		0x82, 0x84, 0x0b, 0x16
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

/* set of rates in supported rates elt when splitting rates between it and the Extended Rates elt */
const wlc_rateset_t wlc_lrs_rates = {
	8,
	{ /*	1,    2,    5.5,  11,   18,   24,   36,   54 Mbps */
		0x02, 0x04, 0x0b, 0x16, 0x24, 0x30, 0x48, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};


/* Rateset including only 1 and 2 Mbps, without basic rate annotation.
 * This is used for a wlc_rate_hwrs_filter_sort_validate param when limiting a rateset for
 * Japan Channel 14 regulatory compliance using early radio revs
 */
const wlc_rateset_t rate_limit_1_2 = {
	2,
	{ /*	1,    2  Mbps */
		0x02, 0x04
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};
