/**
 * @file
 * @brief
 * PHY module Power-per-rate API. Provides interface functions and definitions for
 * ppr structure for use containing regulatory and board limits and tx power targets.
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
 * $Id: $
 */

/**
 * @file
 * @brief
 * Sometimes not all rates should reach the same max power, in order to comply with FCC. Different
 * rates (even different sub-band bands) may want different max power.
 *
 * real_max_power[antenna, rate] = max_power[antenna] - power_offset[rate]
 *
 * Both regulatory restrictions (by wlc_channel.c) and SPROM restrictions (SPROm is read by phy
 * code) determine this power offset.
 */

#include <typedefs.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <wlc_ppr.h>
#include <bcmutils.h>

#ifndef BCMDRIVER

#ifndef WL_BEAMFORMING
#define WL_BEAMFORMING /* enable TxBF definitions for utility code */
#endif // endif

#ifndef WL11AC_160
#define WL11AC_160 /* Enable WL11AC_160 for utility code */
#endif // endif

#ifndef WL11AC_80P80
#define WL11AC_80P80 /* Enable WL11AC_80P80 for utility code */
#endif // endif

#ifndef bcopy
#include <string.h>
#include <stdlib.h>
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#endif // endif

#ifndef ASSERT
#define ASSERT(exp)	do {} while (0)
#endif // endif
#endif /* BCMDRIVER */

/* ppr local TXBF_ENAB() macro because wlc->pub struct is not accessible */
#ifdef WL_BEAMFORMING
#if defined(WLTXBF_DISABLED)
#define PPR_TXBF_ENAB()	(0)
#else
#define PPR_TXBF_ENAB()	(1)
#endif // endif
#else
#define PPR_TXBF_ENAB()	(0)
#endif /* WL_BEAMFORMING */

typedef enum ppr_tlv_id {
	PPR_RGRP_DSSS_ID = 1,
	PPR_RGRP_OFDM_ID,
	PPR_RGRP_MCS_ID,
	PPR_RGRP_HE_ID,
	PPR_RGRP_HE_UB_ID,
	PPR_RGRP_HE_LUB_ID,
	PPR_RGRP_HE_RU_ID
} ppr_tlv_id_t;

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define PPR_SERIALIZATION_VER 3
#define PPR_TLV_VER (PPR_SERIALIZATION_VER + 1)

/** ppr deserialization header */
typedef BWL_PRE_PACKED_STRUCT struct ppr_deser_header {
	uint8  version;
	uint8  bw;
	uint16 per_band_size;
	uint32 flags;
	uint16 chain3size; /* ppr data size of 3 Tx chains, needed in deserialisation process */
} BWL_POST_PACKED_STRUCT ppr_deser_header_t;

typedef BWL_PRE_PACKED_STRUCT struct ppr_ser_mem_flag {
	uint32 magic_word;
	uint32 flag;
} BWL_POST_PACKED_STRUCT ppr_ser_mem_flag_t;

#define WLC_TXPWR_DB_FACTOR 4 /* conversion for phy txpwr cacluations that use .25 dB units */

/* QDB() macro takes a dB value and converts to a quarter dB value */
#ifdef QDB
#undef QDB
#endif // endif
#define QDB(n) ((n) * WLC_TXPWR_DB_FACTOR)

#define PPR_HDR_CUR_BW_MASK		0x000000FF
#define PPR_HDR_ALLOC_BW_MASK		0x0000FF00
#define PPR_HDR_FLAG_MASK		0xFFFF0000
#define PPR_HDR_ALLOC_BW_SHIFT		8

/* PPR flags, start defining from left to right, in case bandwidth needs more bits */
#define PPR_HDR_FLAG_PREALLOC		0x80000000 /* ppr structure is on a pre-allocaed memory */

/* Flag bits in serialization/deserialization */
#define PPR_MAX_TX_CHAIN_MASK	0x0000007 /* mask of Tx chains */
#define PPR_SER_MEM_WORD	0xBEEFC0FF /* magic word indicates serialization start */

/* size of serialization header */
#define SER_HDR_LEN    sizeof(ppr_deser_header_t)

/** Per band tx powers */
typedef BWL_PRE_PACKED_STRUCT struct pprpb {
	/* start of 20MHz tx power limits */
	int8 p_1x1dsss[WL_RATESET_SZ_DSSS];			/* Legacy CCK/DSSS */
	int8 p_1x1ofdm[WL_RATESET_SZ_OFDM];		/* 20 MHz Legacy OFDM transmission */
	int8 p_1x1vhtss1[WL_RATESET_SZ_VHT_MCS_P];	/* 10HT/12VHT pwrs from 1x1mcs0 */
	int8 p_1x1_hess1[WL_RATESET_SZ_HE_MCS];		/* HE SU type */
	int8 p_1x1_hess1_ub[WL_RATESET_SZ_HE_MCS];	/* HE UB type */
	int8 p_1x1_hess1_lub[WL_RATESET_SZ_HE_MCS];	/* HE LUB type */
#if (PPR_MAX_TX_CHAINS > 1)
	int8 p_1x2dsss[WL_RATESET_SZ_DSSS];			/* Legacy CCK/DSSS */
	int8 p_1x2cdd_ofdm[WL_RATESET_SZ_OFDM];		/* 20 MHz Legacy OFDM CDD transmission */
	int8 p_1x2cdd_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 1x2cdd_mcs0 */
	int8 p_2x2stbc_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x2stbc_mcs0 */
	int8 p_2x2vhtss2[WL_RATESET_SZ_VHT_MCS_P];	/* 10HT/12VHT pwrs from 2x2sdm_mcs8 */
	int8 p_1x2cdd_hess1[WL_RATESET_SZ_HE_MCS];
	int8 p_1x2cdd_hess1_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x2cdd_hess1_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x2_hess2[WL_RATESET_SZ_HE_MCS];
	int8 p_2x2_hess2_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x2_hess2_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x2txbf_ofdm[WL_RATESET_SZ_OFDM];	/* 20 MHz Legacy OFDM TXBF transmission */
	int8 p_1x2txbf_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 1x2txbf_mcs0 */
	int8 p_2x2txbf_vhtss2[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x2txbf_mcs8 */
	int8 p_1x2txbf_hess1[WL_RATESET_SZ_HE_MCS];
	int8 p_1x2txbf_hess1_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x2txbf_hess1_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x2txbf_hess2[WL_RATESET_SZ_HE_MCS];
	int8 p_2x2txbf_hess2_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x2txbf_hess2_lub[WL_RATESET_SZ_HE_MCS];
#if (PPR_MAX_TX_CHAINS > 2)
	int8 p_1x3dsss[WL_RATESET_SZ_DSSS];			/* Legacy CCK/DSSS */
	int8 p_1x3cdd_ofdm[WL_RATESET_SZ_OFDM];		/* 20 MHz Legacy OFDM CDD transmission */
	int8 p_1x3cdd_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 1x3cdd_mcs0 */
	int8 p_2x3stbc_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x3stbc_mcs0 */
	int8 p_2x3vhtss2[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x3sdm_mcs8 spexp1 */
	int8 p_3x3vhtss3[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 3x3sdm_mcs16 */
	int8 p_1x3cdd_hess1[WL_RATESET_SZ_HE_MCS];
	int8 p_1x3cdd_hess1_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x3cdd_hess1_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x3_hess2[WL_RATESET_SZ_HE_MCS];
	int8 p_2x3_hess2_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x3_hess2_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x3_hess3[WL_RATESET_SZ_HE_MCS];
	int8 p_3x3_hess3_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x3_hess3_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x3txbf_ofdm[WL_RATESET_SZ_OFDM];	/* 20 MHz Legacy OFDM TXBF transmission */
	int8 p_1x3txbf_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 1x3txbf_mcs0 */
	int8 p_2x3txbf_vhtss2[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x3txbf_mcs8 */
	int8 p_3x3txbf_vhtss3[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 3x3txbf_mcs16 */
	int8 p_1x3txbf_hess1[WL_RATESET_SZ_HE_MCS];
	int8 p_1x3txbf_hess1_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x3txbf_hess1_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x3txbf_hess2[WL_RATESET_SZ_HE_MCS];
	int8 p_2x3txbf_hess2_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x3txbf_hess2_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x3txbf_hess3[WL_RATESET_SZ_HE_MCS];
	int8 p_3x3txbf_hess3_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x3txbf_hess3_lub[WL_RATESET_SZ_HE_MCS];
#if (PPR_MAX_TX_CHAINS > 3)
	int8 p_1x4dsss[WL_RATESET_SZ_DSSS];			/* Legacy CCK/DSSS */
	int8 p_1x4cdd_ofdm[WL_RATESET_SZ_OFDM];		/* 20 MHz Legacy OFDM CDD transmission */
	int8 p_1x4cdd_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 1x4cdd_mcs0 */
	int8 p_2x4stbc_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x4stbc_mcs0 */
	int8 p_2x4vhtss2[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x4sdm_mcs8 spexp1 */
	int8 p_3x4vhtss3[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 3x4 ht_vht */
	int8 p_4x4vhtss4[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 4x4 */
	int8 p_1x4cdd_hess1[WL_RATESET_SZ_HE_MCS];
	int8 p_1x4cdd_hess1_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x4cdd_hess1_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x4_hess2[WL_RATESET_SZ_HE_MCS];
	int8 p_2x4_hess2_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x4_hess2_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x4_hess3[WL_RATESET_SZ_HE_MCS];
	int8 p_3x4_hess3_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x4_hess3_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_4x4_hess4[WL_RATESET_SZ_HE_MCS];
	int8 p_4x4_hess4_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_4x4_hess4_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x4txbf_ofdm[WL_RATESET_SZ_OFDM];	/* 20 MHz Legacy OFDM TXBF transmission */
	int8 p_1x4txbf_vhtss1[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 1x4txbf_mcs0 */
	int8 p_2x4txbf_vhtss2[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 2x4txbf_mcs8 */
	int8 p_3x4txbf_vhtss3[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 3x4txbf_mcs16 */
	int8 p_4x4txbf_vhtss4[WL_RATESET_SZ_VHT_MCS_P]; /* 10HT/12VHT pwrs from 4x4txbf_mcs16 */
	int8 p_1x4txbf_hess1[WL_RATESET_SZ_HE_MCS];
	int8 p_1x4txbf_hess1_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_1x4txbf_hess1_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x4txbf_hess2[WL_RATESET_SZ_HE_MCS];
	int8 p_2x4txbf_hess2_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_2x4txbf_hess2_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x4txbf_hess3[WL_RATESET_SZ_HE_MCS];
	int8 p_3x4txbf_hess3_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_3x4txbf_hess3_lub[WL_RATESET_SZ_HE_MCS];
	int8 p_4x4txbf_hess4[WL_RATESET_SZ_HE_MCS];
	int8 p_4x4txbf_hess4_ub[WL_RATESET_SZ_HE_MCS];
	int8 p_4x4txbf_hess4_lub[WL_RATESET_SZ_HE_MCS];
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
} BWL_POST_PACKED_STRUCT pprpbw_t;

/** Per band ru tx powers */
typedef struct pprpb_ru {
	int8 p_1x1hess1[WL_RATESET_SZ_HE_MCS];		/* 12 pwrs from 1x1 he mcs0 */
#if (PPR_MAX_TX_CHAINS > 1)
	int8 p_1x2cdd_hess1[WL_RATESET_SZ_HE_MCS];	/* 12 pwrs from he 1x2cdd_mcs0 */
	int8 p_2x2hess2[WL_RATESET_SZ_HE_MCS];		/* 12 HE pwrs from 2x2sdm_mcs0 */
	int8 p_1x2txbf_hess1[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 1x2txbf_mcs0 */
	int8 p_2x2txbf_hess2[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 2x2txbf_mcs0 */
#if (PPR_MAX_TX_CHAINS > 2)
	int8 p_1x3cdd_hess1[WL_RATESET_SZ_HE_MCS];	/* 12HE pwrs from 1x3cdd_mcs0 */
	int8 p_2x3hess2[WL_RATESET_SZ_HE_MCS];		/* 12HE pwrs from 2x3sdm_mcs0 spexp1 */
	int8 p_3x3hess3[WL_RATESET_SZ_HE_MCS];		/* 12HE pwrs from 3x3sdm_mcs0 */
	int8 p_1x3txbf_hess1[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 1x3txbf_mcs0 */
	int8 p_2x3txbf_hess2[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 2x3txbf_mcs0 */
	int8 p_3x3txbf_hess3[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 3x3txbf_mcs0 */
#if (PPR_MAX_TX_CHAINS > 3)
	int8 p_1x4cdd_hess1[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 1x4cdd_mcs0 */
	int8 p_2x4hess2[WL_RATESET_SZ_HE_MCS];		/* 12 HE pwrs from 2x4sdm_mcs0 spexp1 */
	int8 p_3x4hess3[WL_RATESET_SZ_HE_MCS];		/* 12 HE pwrs from 3x4 mcs0 */
	int8 p_4x4hess4[WL_RATESET_SZ_HE_MCS];		/* 12 HE pwrs from 4x4 mcs0 */
	int8 p_1x4txbf_hess1[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 1x4txbf_mcs0 */
	int8 p_2x4txbf_hess2[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 2x4txbf_mcs0 */
	int8 p_3x4txbf_hess3[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 3x4txbf_mcs0 */
	int8 p_4x4txbf_hess4[WL_RATESET_SZ_HE_MCS];	/* 12 HE pwrs from 4x4txbf_mcs0 */
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
} pprpbw_ru_t;

#define PPR_CHAIN1_FIRST OFFSETOF(pprpbw_t, p_1x1dsss)
#define PPR_CHAIN1_FIRST_MCS OFFSETOF(pprpbw_t, p_1x1vhtss1)
#define PPR_CHAIN1_END   (OFFSETOF(pprpbw_t, p_1x1_hess1_lub) + \
	sizeof(((pprpbw_t *)0)->p_1x1_hess1_lub))
#define PPR_CHAIN1_SIZE  PPR_CHAIN1_END
#define PPR_CHAIN1_MCS_END (OFFSETOF(pprpbw_t, p_1x1_hess1_lub) + \
	sizeof(((pprpbw_t *)0)->p_1x1_hess1_lub))
#define PPR_CHAIN1_MCS_SIZE  (PPR_CHAIN1_MCS_END - PPR_CHAIN1_FIRST_MCS)
/* 1 tx mode, 1x1_he. 3 types, SU, UB and LUB */
#define PPR_CHAIN1_HE_SIZE (3 * WL_RATESET_SZ_HE_MCS)
#if (PPR_MAX_TX_CHAINS > 1)
#define PPR_CHAIN2_FIRST OFFSETOF(pprpbw_t, p_1x2dsss)
#define PPR_CHAIN2_FIRST_MCS OFFSETOF(pprpbw_t, p_1x2cdd_vhtss1)
#define PPR_CHAIN2_END   (OFFSETOF(pprpbw_t, p_2x2txbf_hess2_lub) + \
	sizeof(((pprpbw_t *)0)->p_2x2txbf_hess2_lub))
#define PPR_CHAIN2_SIZE  (PPR_CHAIN2_END - PPR_CHAIN2_FIRST)
#define PPR_CHAIN2_MCS_END (OFFSETOF(pprpbw_t, p_2x2_hess2_lub) + \
	sizeof(((pprpbw_t *)0)->p_2x2_hess2_lub))
#define PPR_CHAIN2_MCS_SIZE  (PPR_CHAIN2_MCS_END - PPR_CHAIN2_FIRST_MCS)
/* 2 tx modes, 1x2cdd_he and 2x2_he. 3 types, SU, UB and LUB */
#define PPR_CHAIN2_HE_SIZE (2 * 3 * WL_RATESET_SZ_HE_MCS)
#define PPR_BF_CHAIN2_FIRST OFFSETOF(pprpbw_t, p_1x2txbf_ofdm)
#define PPR_BF_CHAIN2_FIRST_MCS OFFSETOF(pprpbw_t, p_1x2txbf_vhtss1)
#define PPR_BF_CHAIN2_END   (OFFSETOF(pprpbw_t, p_2x2txbf_hess2_lub) + \
	sizeof(((pprpbw_t *)0)->p_2x2txbf_hess2_lub))
#define PPR_BF_CHAIN2_SIZE  (PPR_BF_CHAIN2_END - PPR_BF_CHAIN2_FIRST)
#define PPR_BF_CHAIN2_MCS_SIZE  (PPR_BF_CHAIN2_END - PPR_BF_CHAIN2_FIRST_MCS)
/* 2 tx modes, 1x2txbf_he and 2x2txbf_he. 3 types, SU, UB and LUB */
#define PPR_BF_CHAIN2_HE_SIZE (2 * 3 * WL_RATESET_SZ_HE_MCS)
#if (PPR_MAX_TX_CHAINS > 2)
#define PPR_CHAIN3_FIRST OFFSETOF(pprpbw_t, p_1x3dsss)
#define PPR_CHAIN3_FIRST_MCS OFFSETOF(pprpbw_t, p_1x3cdd_vhtss1)
#define PPR_CHAIN3_END   (OFFSETOF(pprpbw_t, p_3x3txbf_hess3_lub) + \
	sizeof(((pprpbw_t *)0)->p_3x3txbf_hess3_lub))
#define PPR_CHAIN3_SIZE  (PPR_CHAIN3_END - PPR_CHAIN3_FIRST)
#define PPR_CHAIN3_MCS_END (OFFSETOF(pprpbw_t, p_3x3_hess3_lub) + \
	sizeof(((pprpbw_t *)0)->p_3x3_hess3_lub))
#define PPR_CHAIN3_MCS_SIZE  (PPR_CHAIN3_MCS_END - PPR_CHAIN3_FIRST_MCS)
/* 3 tx modes, 1x3cdd_he, 2x3_he and 3x3_he. 3 types, SU, UB and LUB */
#define PPR_CHAIN3_HE_SIZE (3 * 3 * WL_RATESET_SZ_HE_MCS)
#define PPR_BF_CHAIN3_FIRST OFFSETOF(pprpbw_t, p_1x3txbf_ofdm)
#define PPR_BF_CHAIN3_FIRST_MCS OFFSETOF(pprpbw_t, p_1x3txbf_vhtss1)
#define PPR_BF_CHAIN3_END   (OFFSETOF(pprpbw_t, p_3x3txbf_hess3_lub) + \
	sizeof(((pprpbw_t *)0)->p_3x3txbf_hess3_lub))
#define PPR_BF_CHAIN3_SIZE  (PPR_BF_CHAIN3_END - PPR_BF_CHAIN3_FIRST)
#define PPR_BF_CHAIN3_MCS_SIZE  (PPR_BF_CHAIN3_END - PPR_BF_CHAIN3_FIRST_MCS)
/* 3 tx modes, 1x3txbf_he, 2x3txbf_he and 3x3txbf_he. 3 types, SU, UB and LUB */
#define PPR_BF_CHAIN3_HE_SIZE (3 * 3 * WL_RATESET_SZ_HE_MCS)
#if (PPR_MAX_TX_CHAINS > 3)
#define PPR_CHAIN4_FIRST OFFSETOF(pprpbw_t, p_1x4dsss)
#define PPR_CHAIN4_FIRST_MCS OFFSETOF(pprpbw_t, p_1x4cdd_vhtss1)
#define PPR_CHAIN4_END   (OFFSETOF(pprpbw_t, p_4x4txbf_hess4_lub) + \
	sizeof(((pprpbw_t *)0)->p_4x4txbf_hess4_lub))
#define PPR_CHAIN4_SIZE  (PPR_CHAIN4_END - PPR_CHAIN4_FIRST)
#define PPR_CHAIN4_MCS_END (OFFSETOF(pprpbw_t, p_4x4_hess4_lub) + \
	sizeof(((pprpbw_t *)0)->p_4x4_hess4_lub))
#define PPR_CHAIN4_MCS_SIZE  (PPR_CHAIN4_MCS_END - PPR_CHAIN4_FIRST_MCS)
/* 4 tx modes, 1x4cdd_he, 2x4_he, 3x4_he and 4x4_he. 3 types, SU, UB and LUB */
#define PPR_CHAIN4_HE_SIZE (4 * 3 * WL_RATESET_SZ_HE_MCS)
#define PPR_BF_CHAIN4_FIRST OFFSETOF(pprpbw_t, p_1x4txbf_ofdm)
#define PPR_BF_CHAIN4_FIRST_MCS OFFSETOF(pprpbw_t, p_1x4txbf_vhtss1)
#define PPR_BF_CHAIN4_END   (OFFSETOF(pprpbw_t, p_4x4txbf_hess4_lub) + \
	sizeof(((pprpbw_t *)0)->p_4x4txbf_hess4_lub))
#define PPR_BF_CHAIN4_SIZE  (PPR_BF_CHAIN4_END - PPR_BF_CHAIN4_FIRST)
#define PPR_BF_CHAIN4_MCS_SIZE  (PPR_BF_CHAIN4_END - PPR_BF_CHAIN4_FIRST_MCS)
/* 4 tx modes, 1x4txbf_he, 2x4txbf_he, 3x4txbf_he and 4x4txbf_he. 3 types, SU, UB and LUB */
#define PPR_BF_CHAIN4_HE_SIZE (4 * 3 * WL_RATESET_SZ_HE_MCS)
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */

/* Maximum supported bandwidth */
#if defined(WL11AC_80P80)
#define PPR_BW_MAX WL_TX_BW_8080
#define MAX_PPR_SIZE (sizeof(ppr_bw_8080_t) + sizeof(wl_tx_bw_t))
#elif defined(WL11AC_160)
#define PPR_BW_MAX WL_TX_BW_160
#define MAX_PPR_SIZE (sizeof(ppr_bw_160_t) + sizeof(wl_tx_bw_t))
#else
#define PPR_BW_MAX WL_TX_BW_80
#define MAX_PPR_SIZE (sizeof(ppr_bw_80_t) + sizeof(wl_tx_bw_t))
#endif // endif

/* If a bw is ULB */
#define IS_ULB_BW(bw) ((bw == WL_TX_BW_2P5) || (bw == WL_TX_BW_5) || (bw == WL_TX_BW_10))

/** Structure to contain ppr values for a 20MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_20 {
	/* 20MHz tx power limits */
	pprpbw_t b20;
} BWL_POST_PACKED_STRUCT ppr_bw_20_t;

/** Structure to contain ppr values for a 40MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_40 {
	/* 40MHz tx power limits */
	pprpbw_t b40;
	/* 20in40MHz tx power limits */
	pprpbw_t b20in40;
} BWL_POST_PACKED_STRUCT ppr_bw_40_t;

/** Structure to contain ppr values for an 80MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_80 {
	/* 80MHz tx power limits */
	pprpbw_t b80;
	/* 20in80MHz tx power limits */
	pprpbw_t b20in80;
	/* 40in80MHz tx power limits */
	pprpbw_t b40in80;
} BWL_POST_PACKED_STRUCT ppr_bw_80_t;

/** Structure to contain ppr values for a 160MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_160 {
	/* 160MHz tx power limits */
	pprpbw_t b160;
	/* 20in160MHz tx power limits */
	pprpbw_t b20in160;
	/* 40in160MHz tx power limits */
	pprpbw_t b40in160;
	/* 80in160MHz tx power limits */
	pprpbw_t b80in160;
} BWL_POST_PACKED_STRUCT ppr_bw_160_t;

/** Structure to contain ppr values for an 80+80MHz channel */
typedef BWL_PRE_PACKED_STRUCT struct ppr_bw_8080 {
	/* 80+80MHz chan1 tx power limits */
	pprpbw_t b80ch1;
	/* 80+80MHz chan2 tx power limits */
	pprpbw_t b80ch2;
	/* 80in80+80MHz (chan1 subband) tx power limits */
	pprpbw_t b80in8080;
	/* 20in80+80MHz (chan1 subband) tx power limits */
	pprpbw_t b20in8080;
	/* 40in80+80MHz (chan1 subband) tx power limits */
	pprpbw_t b40in8080;
} BWL_POST_PACKED_STRUCT ppr_bw_8080_t;

/**
 * This is the initial implementation of the structure we're hiding. It is sized to contain only
 * the set of powers it requires, so the union is not necessarily the size of the largest member.
 */
BWL_PRE_PACKED_STRUCT struct ppr {
	uint32 hdr;

	BWL_PRE_PACKED_STRUCT union {
		ppr_bw_20_t ch20;
		ppr_bw_40_t ch40;
		ppr_bw_80_t ch80;
		ppr_bw_160_t ch160;
		ppr_bw_8080_t ch8080;
	} ppr_bw;
} BWL_POST_PACKED_STRUCT;

/* RU26, 52, 106, 242, 484, 996 */
#define NUM_RU 6
/* ppr size of struct ppr_ru */
#define SIZE_RU_PPR (sizeof(pprpbw_ru_t) * NUM_RU)
struct ppr_ru {
	uint32 hdr;
	/* ru26 tx power limits */
	pprpbw_ru_t ru26;
	/* ru52 tx power limits */
	pprpbw_ru_t ru52;
	/* ru106 tx power limits */
	pprpbw_ru_t ru106;
	/* ru242 tx power limits */
	pprpbw_ru_t ru242;
	/* ru484 tx power limits */
	pprpbw_ru_t ru484;
	/* ru996 tx power limits */
	pprpbw_ru_t ru996;
};

typedef BWL_PRE_PACKED_STRUCT struct ppr_dsss_tlv {
	uint8 bw;
	uint8 chains;
	uint8 pwr[];
} BWL_POST_PACKED_STRUCT ppr_dsss_tlv_t;

typedef BWL_PRE_PACKED_STRUCT struct ppr_ofdm_tlv {
	uint8 bw;
	uint8 chains;
	uint8 mode;
	uint8 pwr[];
} BWL_POST_PACKED_STRUCT ppr_ofdm_tlv_t;

typedef BWL_PRE_PACKED_STRUCT struct ppr_mcs_tlv {
	uint8 bw;
	uint8 chains;
	uint8 mode;
	uint8 nss;
	uint8 pwr[];
} BWL_POST_PACKED_STRUCT ppr_mcs_tlv_t;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

static wl_tx_bw_t ppr_get_cur_bw(const ppr_t* p)
{
	return ((p->hdr) & PPR_HDR_CUR_BW_MASK);
}

#ifdef BCMDRIVER
static wl_tx_bw_t ppr_get_alloc_bw(uint32 hdr)
{
	return (((hdr) & PPR_HDR_ALLOC_BW_MASK) >> PPR_HDR_ALLOC_BW_SHIFT);
}
#endif // endif

#if defined(BCMDBG_ASSERT) && defined(BCMDRIVER)
static bool ppr_is_valid_bw(wl_tx_bw_t bw)
{
	bool ret = FALSE;
	if ((bw == WL_TX_BW_20) || (bw == WL_TX_BW_40) || (bw == WL_TX_BW_80) ||
		(bw == WL_TX_BW_160) || (bw == WL_TX_BW_8080) || (bw == PPR_BW_MAX) ||
		IS_ULB_BW(bw)) {
		ret = TRUE;
	}

	return ret;
}
#endif // endif

/** Returns a flag of ppr conditions (chains, txbf etc.) */
static uint32 ppr_get_flag(void)
{
	uint32 flag = 0;
	flag  |= PPR_MAX_TX_CHAINS & PPR_MAX_TX_CHAIN_MASK;
	return flag;
}

static uint16 ppr_ser_size_per_band(uint32 flags)
{
	uint16 ret = PPR_CHAIN1_SIZE; /* at least 1 chain rates should be there */
	uint8 chain   = flags & PPR_MAX_TX_CHAIN_MASK;
	BCM_REFERENCE(chain);
#if (PPR_MAX_TX_CHAINS > 1)
	if (chain > 1) {
		ret += PPR_CHAIN2_SIZE;
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (chain > 2) {
		ret += PPR_CHAIN3_SIZE;
	}
#if (PPR_MAX_TX_CHAINS > 3)
	if (chain > 3) {
		ret += PPR_CHAIN4_SIZE;
	}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
	return ret;
}

/** Returns the number of bands for a specific bandwidth bw */
static uint ppr_bands_by_bw(wl_tx_bw_t bw)
{
	switch (bw) {
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
		return sizeof(ppr_bw_20_t)/sizeof(pprpbw_t);
	case WL_TX_BW_40:
		return sizeof(ppr_bw_40_t)/sizeof(pprpbw_t);
	case WL_TX_BW_80:
		return sizeof(ppr_bw_80_t)/sizeof(pprpbw_t);
	case WL_TX_BW_160:
		return sizeof(ppr_bw_160_t)/sizeof(pprpbw_t);
	case WL_TX_BW_8080:
		return sizeof(ppr_bw_8080_t)/sizeof(pprpbw_t);
	default:
		ASSERT(0);
		return 0;
	}
}

/** Return the required serialization size based on the flag field. */
static uint ppr_ser_size_by_flag(uint32 flag, wl_tx_bw_t bw)
{
	return ppr_ser_size_per_band(flag) * ppr_bands_by_bw(bw);
}

#define COPY_PPR_TOBUF(x, y) do { bcopy(&pprbuf[x], *buf, y); \
	*buf += y; ret += y; } while (0);

/** Serialize ppr data of a bandwidth into the given buffer */
static uint ppr_serialize_block(const uint8* pprbuf, uint8** buf, uint32 serflag)
{
	uint ret = 0;
#if (PPR_MAX_TX_CHAINS > 1)
	uint chain   = serflag & PPR_MAX_TX_CHAIN_MASK; /* chain number in serialized block */
#endif // endif

	COPY_PPR_TOBUF(PPR_CHAIN1_FIRST, PPR_CHAIN1_SIZE);
#if (PPR_MAX_TX_CHAINS > 1)
	if (chain > 1) {
		COPY_PPR_TOBUF(PPR_CHAIN2_FIRST, PPR_CHAIN2_SIZE);
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (chain > 2) {
		COPY_PPR_TOBUF(PPR_CHAIN3_FIRST, PPR_CHAIN3_SIZE);
	}
#if (PPR_MAX_TX_CHAINS > 3)
	if (chain > 3) {
		COPY_PPR_TOBUF(PPR_CHAIN4_FIRST, PPR_CHAIN4_SIZE);
	}
#endif /* (PPR_MAX_TX_CHAINS > 2) */
#endif /* (PPR_MAX_TX_CHAINS > 2) */
#endif /* (PPR_MAX_TX_CHAINS > 1) */
	return ret;
}

/** Serialize ppr data of each bandwidth into the given buffer, returns bytes copied */
static uint ppr_serialize_data(const ppr_t *pprptr, uint8* buf, uint32 serflag)
{
	uint i;
	uint bands;
	const uint8* pprbuf;

	uint ret = sizeof(ppr_deser_header_t);
	ppr_deser_header_t* header = (ppr_deser_header_t*)buf;
	ASSERT(pprptr && buf);
	header->version = PPR_SERIALIZATION_VER;
	header->bw      = (uint8)ppr_get_cur_bw(pprptr);
	header->flags   = HTON32(ppr_get_flag());
	header->per_band_size	= HTON16(ppr_ser_size_per_band(serflag));

	buf += sizeof(*header);

	bands = ppr_bands_by_bw(header->bw);
	pprbuf = (const uint8*)&pprptr->ppr_bw;
	for (i = 0; i < bands; i++) {
		ret += ppr_serialize_block(pprbuf, &buf, serflag);
		pprbuf += sizeof(pprpbw_t); /* Jump to next band */
	}

	return ret;
}

/** Copy serialized ppr data of a bandwidth */
static void
ppr_copy_serdata(uint8* pobuf, const uint8** inbuf, uint32 flag, uint16 per_band_size,
	uint16 ppr_ver)
{
	uint chain   = flag & PPR_MAX_TX_CHAIN_MASK;
	uint16 len, chain1_size;

	BCM_REFERENCE(chain);
	/* HE rate is supported from ppr_ver = 1 */
	if (ppr_ver >= 1) {
		chain1_size = PPR_CHAIN1_SIZE;
	} else {
		chain1_size = PPR_CHAIN1_SIZE - PPR_CHAIN1_HE_SIZE;
	}
	bcopy(*inbuf, pobuf, chain1_size);
	*inbuf += chain1_size;
	len = chain1_size;
#if (PPR_MAX_TX_CHAINS > 1)
	if (chain > 1) {
		uint16 chain2_size;

		if (ppr_ver >= 1) {
			chain2_size = PPR_CHAIN2_SIZE;
		} else {
			chain2_size = PPR_CHAIN2_SIZE -
				(PPR_CHAIN2_HE_SIZE + PPR_BF_CHAIN2_HE_SIZE);
		}
		bcopy(*inbuf, &pobuf[PPR_CHAIN2_FIRST], chain2_size);
		*inbuf += chain2_size;
		len += chain2_size;
	}
#if (PPR_MAX_TX_CHAINS > 2)
	if (chain > 2) {
		uint16 chain3_size;

		if (ppr_ver >= 1) {
			chain3_size = PPR_CHAIN3_SIZE;
		} else {
			chain3_size = PPR_CHAIN3_SIZE -
				(PPR_CHAIN3_HE_SIZE + PPR_BF_CHAIN3_HE_SIZE);
		}
		bcopy(*inbuf, &pobuf[PPR_CHAIN3_FIRST], chain3_size);
		*inbuf += chain3_size;
		len += chain3_size;
	}
#if (PPR_MAX_TX_CHAINS > 3)
	if (chain > 3) {
		uint16 chain4_size;

		if (ppr_ver >= 1) {
			chain4_size = PPR_CHAIN4_SIZE;
		} else {
			chain4_size = PPR_CHAIN4_SIZE -
				(PPR_CHAIN4_HE_SIZE + PPR_BF_CHAIN4_HE_SIZE);
		}
		bcopy(*inbuf, &pobuf[PPR_CHAIN4_FIRST], chain4_size);
		*inbuf += chain4_size;
		len += chain4_size;
	}
#endif /* (PPR_MAX_TX_CHAINS > 3) */
#endif /* (PPR_MAX_TX_CHAINS > 2) */
#endif /* (PPR_MAX_TX_CHAINS > 1) */
	if (len < per_band_size) {
		 *inbuf += (per_band_size - len);
	}
}

/* Deserialize data into a ppr_t structure */
static void
ppr_deser_cpy(ppr_t* pptr, const uint8* inbuf, uint32 flag, wl_tx_bw_t bw, uint16 per_band_size,
	uint16 ppr_ver)
{
	uint i;
	uint bands;
	uint8* pobuf;

	ppr_set_ch_bw(pptr, bw);
	bands = ppr_bands_by_bw(bw);
	pobuf = (uint8*)&pptr->ppr_bw;
	for (i = 0; i < bands; i++) {
		ppr_copy_serdata(pobuf, &inbuf, flag, per_band_size, ppr_ver);
		pobuf += sizeof(pprpbw_t); /* Jump to next band */
	}
}

/** Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_20(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	if (bw == WL_TX_BW_20 || IS_ULB_BW(bw))
		pwrs = &p->ppr_bw.ch20.b20;
	/* else */
	/*   ASSERT(0); */
	return pwrs;
}

/** Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_40(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_40:
		pwrs = &p->ppr_bw.ch40.b40;
	break;
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:

	case WL_TX_BW_20IN40:
		pwrs = &p->ppr_bw.ch40.b20in40;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}

/** Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_80(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_80:
		pwrs = &p->ppr_bw.ch80.b80;
	break;
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
	case WL_TX_BW_20IN40:
	case WL_TX_BW_20IN80:
		pwrs = &p->ppr_bw.ch80.b20in80;
	break;
	case WL_TX_BW_40:
	case WL_TX_BW_40IN80:
		pwrs = &p->ppr_bw.ch80.b40in80;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}

/** Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_160(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_160:
	case WL_TX_BW_8080:		// stf function doesn't care much if we're 160 or 80p80
		pwrs = &p->ppr_bw.ch160.b160;
	break;
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
	case WL_TX_BW_20IN40:
	case WL_TX_BW_20IN80:
	case WL_TX_BW_20IN160:
		pwrs = &p->ppr_bw.ch160.b20in160;
	break;
	case WL_TX_BW_40:
	case WL_TX_BW_40IN80:
	case WL_TX_BW_40IN160:
		pwrs = &p->ppr_bw.ch160.b40in160;
	break;
	case WL_TX_BW_80:
	case WL_TX_BW_80IN160:
		pwrs = &p->ppr_bw.ch160.b80in160;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}

/** Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers_8080(ppr_t* p, wl_tx_bw_t bw)
{
	pprpbw_t* pwrs = NULL;

	switch (bw) {
	case WL_TX_BW_160:		// stf function doesn't care much if we're 160 or 80p80
	case WL_TX_BW_8080:
		pwrs = &p->ppr_bw.ch8080.b80ch1;
	break;
	case WL_TX_BW_8080CHAN2:
		pwrs = &p->ppr_bw.ch8080.b80ch2;
	break;
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
	case WL_TX_BW_20IN40:
	case WL_TX_BW_20IN80:
	case WL_TX_BW_20IN160:
	case WL_TX_BW_20IN8080:
		pwrs = &p->ppr_bw.ch8080.b20in8080;
	break;
	case WL_TX_BW_40:
	case WL_TX_BW_40IN80:
	case WL_TX_BW_40IN160:
	case WL_TX_BW_40IN8080:
		pwrs = &p->ppr_bw.ch8080.b40in8080;
	break;
	case WL_TX_BW_80:
	case WL_TX_BW_80IN160:
	case WL_TX_BW_80IN8080:
		pwrs = &p->ppr_bw.ch8080.b80in8080;
	break;
	default:
		/* ASSERT(0); */
	break;
	}
	return pwrs;
}

typedef pprpbw_t* (*wlc_ppr_get_bw_pwrs_fn_t)(ppr_t* p, wl_tx_bw_t bw);

typedef struct {
	wl_tx_bw_t ch_bw;		/* Bandwidth of the channel for which powers are stored */
	/* Function to retrieve the powers for the requested bandwidth */
	wlc_ppr_get_bw_pwrs_fn_t fn;
} wlc_ppr_get_bw_pwrs_pair_t;

static const wlc_ppr_get_bw_pwrs_pair_t ppr_get_bw_pwrs_fn[] = {
	{WL_TX_BW_20, ppr_get_bw_powers_20},
	{WL_TX_BW_40, ppr_get_bw_powers_40},
	{WL_TX_BW_80, ppr_get_bw_powers_80},
	{WL_TX_BW_160, ppr_get_bw_powers_160},
	{WL_TX_BW_8080, ppr_get_bw_powers_8080},
	{WL_TX_BW_2P5, ppr_get_bw_powers_20},
	{WL_TX_BW_5, ppr_get_bw_powers_20},
	{WL_TX_BW_10, ppr_get_bw_powers_20},
};

/** Get a pointer to the power values for a given channel bandwidth */
static pprpbw_t* ppr_get_bw_powers(ppr_t* p, wl_tx_bw_t bw)
{
	uint32 i;

	if (p == NULL) {
		return NULL;
	}

	for (i = 0; i < (int)ARRAYSIZE(ppr_get_bw_pwrs_fn); i++) {
		if (ppr_get_bw_pwrs_fn[i].ch_bw == ppr_get_cur_bw(p))
			return ppr_get_bw_pwrs_fn[i].fn(p, bw);
	}

	ASSERT(0);
	return NULL;
}

/** Get a pointer to the power values for a given ru size */
static pprpbw_ru_t* ppr_get_ru_powers(ppr_ru_t* p, wl_he_rate_type_t type)
{
	if (p == NULL) {
		return NULL;
	}

	switch (type) {
	case WL_HE_RT_RU26:
		return &p->ru26;
	case WL_HE_RT_RU52:
		return &p->ru52;
	case WL_HE_RT_RU106:
		return &p->ru106;
	case WL_HE_RT_RU242:
		return &p->ru242;
	case WL_HE_RT_RU484:
		return &p->ru484;
	case WL_HE_RT_RU996:
		return &p->ru996;
	default:
		return NULL;
	}
}

/**
 * Rate group power finder functions: ppr_get_xxx_group()
 * To preserve the opacity of the PPR struct, even inside the API we try to limit knowledge of
 * its details. Almost all API functions work on the powers for individual rate groups, rather than
 * directly accessing the struct. Once the section of the structure corresponding to the bandwidth
 * has been identified using ppr_get_bw_powers(), the ppr_get_xxx_group() functions use knowledge
 * of the number of spatial streams, the number of tx chains, and the expansion mode to return a
 * pointer to the required group of power values.
 */

/** Get a pointer to the power values for the given dsss rate group for a given channel bandwidth */
static int8* ppr_get_dsss_group(pprpbw_t* bw_pwrs, wl_tx_chains_t tx_chains)
{
	int8* group_pwrs = NULL;

	switch (tx_chains) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
#if (PPR_MAX_TX_CHAINS > 3)
	case WL_TX_CHAINS_4:
		group_pwrs = bw_pwrs->p_1x4dsss;
		break;
#endif /* PPR_MAX_TX_CHAINS > 3 */
	case WL_TX_CHAINS_3:
		group_pwrs = bw_pwrs->p_1x3dsss;
		break;
#endif /* PPR_MAX_TX_CHAINS > 2 */
	case WL_TX_CHAINS_2:
		group_pwrs = bw_pwrs->p_1x2dsss;
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_CHAINS_1:
		group_pwrs = bw_pwrs->p_1x1dsss;
		break;
	default:
		ASSERT(0);
		break;
	}
	return group_pwrs;
}

/** Get a pointer to the power values for the given ofdm rate group for a given channel bandwidth */
static int8* ppr_get_ofdm_group(pprpbw_t* bw_pwrs, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	int8* group_pwrs = NULL;
	BCM_REFERENCE(mode);
	switch (tx_chains) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
#if (PPR_MAX_TX_CHAINS > 3)
	case WL_TX_CHAINS_4:
		if (mode == WL_TX_MODE_TXBF) {
			group_pwrs = bw_pwrs->p_1x4txbf_ofdm;
		} else {
			group_pwrs = bw_pwrs->p_1x4cdd_ofdm;
		}
		break;
#endif /* PPR_MAX_TX_CHAINS > 3 */
	case WL_TX_CHAINS_3:
		if (mode == WL_TX_MODE_TXBF)
			group_pwrs = bw_pwrs->p_1x3txbf_ofdm;
		else
			group_pwrs = bw_pwrs->p_1x3cdd_ofdm;
		break;
#endif /* PPR_MAX_TX_CHAINS > 2 */
	case WL_TX_CHAINS_2:
		if (mode == WL_TX_MODE_TXBF)
			group_pwrs = bw_pwrs->p_1x2txbf_ofdm;
		else
			group_pwrs = bw_pwrs->p_1x2cdd_ofdm;
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_CHAINS_1:
		group_pwrs = bw_pwrs->p_1x1ofdm;
		break;
	default:
		ASSERT(0);
		break;
	}
	return group_pwrs;
}

/**
 * Tables to provide access to HT/VHT rate group powers. This avoids an ugly nested switch with
 * messy conditional compilation.
 *
 * Access to a given table entry is via table[chains - Nss][mode], except for the Nss3 table, which
 * only has one row, so it can be indexed directly by table[mode].
 *
 * Separate tables are provided for each of Nss1, Nss2 and Nss3 because they are all different
 * sizes. A combined table would be very sparse, and this arrangement also simplifies the
 * conditional compilation.
 *
 * Each row represents a given number of chains, so there's no need for a zero row. Because
 * chains >= Nss is always true, there is no one-chain row for Nss2 and there are no one- or
 * two-chain rows for Nss3. With the tables correctly sized, we can index the rows
 * using [chains - Nss].
 *
 * Then, inside each row, we index by mode:
 * WL_TX_MODE_NONE, WL_TX_MODE_STBC, WL_TX_MODE_CDD, WL_TX_MODE_TXBF.
 */

#define OFFSNONE (-1)

static const int mcs_groups_nss1[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_t, p_1x1vhtss1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x2cdd_vhtss1),
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x3cdd_vhtss1),
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x4cdd_vhtss1),
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
};

/** mcs group with TXBF data */
static const int mcs_groups_nss1_txbf[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_t, p_1x1vhtss1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x2cdd_vhtss1),
	OFFSETOF(pprpbw_t, p_1x2txbf_vhtss1)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x3cdd_vhtss1),
	OFFSETOF(pprpbw_t, p_1x3txbf_vhtss1)},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x4cdd_vhtss1),
	OFFSETOF(pprpbw_t, p_1x4txbf_vhtss1)}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
};

#if (PPR_MAX_TX_CHAINS > 1)
static const int mcs_groups_nss2[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_t, p_2x2vhtss2),
	OFFSETOF(pprpbw_t, p_2x2stbc_vhtss1),
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_t, p_2x3vhtss2),
	OFFSETOF(pprpbw_t, p_2x3stbc_vhtss1),
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSETOF(pprpbw_t, p_2x4vhtss2),
	OFFSETOF(pprpbw_t, p_2x4stbc_vhtss1),
	OFFSNONE,
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
};

/** mcs group with TXBF data */
static const int mcs_groups_nss2_txbf[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_t, p_2x2vhtss2),
	OFFSETOF(pprpbw_t, p_2x2stbc_vhtss1),
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x2txbf_vhtss2)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_t, p_2x3vhtss2),
	OFFSETOF(pprpbw_t, p_2x3stbc_vhtss1),
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x3txbf_vhtss2)},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSETOF(pprpbw_t, p_2x4vhtss2),
	OFFSETOF(pprpbw_t, p_2x4stbc_vhtss1),
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x4txbf_vhtss2)}
#endif /* PPR_MAX_TX_CHAINS > 3  */
#endif /* PPR_MAX_TX_CHAINS > 2 */
};
#if (PPR_MAX_TX_CHAINS > 2)
static const int mcs_groups_nss3[PPR_MAX_TX_CHAINS - 2][WL_NUM_TX_MODES] = {
/* 3 chains */
	{OFFSETOF(pprpbw_t, p_3x3vhtss3),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	{OFFSETOF(pprpbw_t, p_3x4vhtss3),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3  */
};

/** mcs group with TXBF data */
static const int mcs_groups_nss3_txbf[PPR_MAX_TX_CHAINS - 2][WL_NUM_TX_MODES] = {
/* 3 chains */
	{OFFSETOF(pprpbw_t, p_3x3vhtss3),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_3x3txbf_vhtss3)},
#if (PPR_MAX_TX_CHAINS > 3)
	{OFFSETOF(pprpbw_t, p_3x4vhtss3),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_3x4txbf_vhtss3)}
#endif /* PPR_MAX_TX_CHAINS > 3  */
};
#if (PPR_MAX_TX_CHAINS > 3)
static const int mcs_groups_nss4[WL_NUM_TX_MODES] = {
/* 4 chains only */
	OFFSETOF(pprpbw_t, p_4x4vhtss4),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE,
};
/** mcs group with TXBF data */
static const int mcs_groups_nss4_txbf[WL_NUM_TX_MODES] = {
/* 4 chains only */
	OFFSETOF(pprpbw_t, p_4x4vhtss4),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_4x4txbf_vhtss4)
};
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */

/** Get a pointer to the power values for the given rate group for a given channel bandwidth */
static int8* ppr_get_mcs_group(pprpbw_t* bw_pwrs, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	int8* group_pwrs = NULL;
	int offset;

	switch (Nss) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
#if (PPR_MAX_TX_CHAINS > 3)
	case WL_TX_NSS_4:
		if (tx_chains == WL_TX_CHAINS_4) {
			if (mode == WL_TX_MODE_TXBF && PPR_TXBF_ENAB()) {
				 offset = mcs_groups_nss4_txbf[mode];
			} else {
				offset = mcs_groups_nss4[mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 3 */
	case WL_TX_NSS_3:
		if ((tx_chains >= WL_TX_CHAINS_3) && (tx_chains <= PPR_MAX_TX_CHAINS)) {
			if (PPR_TXBF_ENAB()) {
				offset = mcs_groups_nss3_txbf[tx_chains - Nss][mode];
			} else {
				offset = mcs_groups_nss3[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 2 */
	case WL_TX_NSS_2:
		if ((tx_chains >= WL_TX_CHAINS_2) && (tx_chains <= PPR_MAX_TX_CHAINS)) {
			if (PPR_TXBF_ENAB()) {
				offset = mcs_groups_nss2_txbf[tx_chains - Nss][mode];
			} else {
				offset = mcs_groups_nss2[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_NSS_1:
		if (tx_chains <= PPR_MAX_TX_CHAINS) {
			if (PPR_TXBF_ENAB()) {
				offset = mcs_groups_nss1_txbf[tx_chains - Nss][mode];
			} else {
				offset = mcs_groups_nss1[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
	default:
#ifdef BCMQT
		printf("%s: %d: WL_TX_CHAINS_4 not supported yet, ignoring for now!!\n",
			__FUNCTION__, __LINE__);
#else
		ASSERT(0);
#endif // endif
		break;
	}
	return group_pwrs;
}

static const int he_mcs_groups_nss1[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_t, p_1x1_hess1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x2cdd_hess1),
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x3cdd_hess1),
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x4cdd_hess1),
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
};

/** mcs group with TXBF data */
static const int he_mcs_groups_nss1_txbf[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_t, p_1x1_hess1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x2cdd_hess1),
	OFFSETOF(pprpbw_t, p_1x2txbf_hess1)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x3cdd_hess1),
	OFFSETOF(pprpbw_t, p_1x3txbf_hess1)},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_1x4cdd_hess1),
	OFFSETOF(pprpbw_t, p_1x4txbf_hess1)}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
};

#if (PPR_MAX_TX_CHAINS > 1)
static const int he_mcs_groups_nss2[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_t, p_2x2_hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_t, p_2x3_hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSETOF(pprpbw_t, p_2x4_hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
};

/** mcs group with TXBF data */
static const int he_mcs_groups_nss2_txbf[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_t, p_2x2_hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x2txbf_hess2)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_t, p_2x3_hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x3txbf_hess2)},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSETOF(pprpbw_t, p_2x4_hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_2x4txbf_hess2)}
#endif /* PPR_MAX_TX_CHAINS > 3  */
#endif /* PPR_MAX_TX_CHAINS > 2 */
};
#if (PPR_MAX_TX_CHAINS > 2)
static const int he_mcs_groups_nss3[PPR_MAX_TX_CHAINS - 2][WL_NUM_TX_MODES] = {
/* 3 chains */
	{OFFSETOF(pprpbw_t, p_3x3_hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	{OFFSETOF(pprpbw_t, p_3x4_hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3  */
};

/** mcs group with TXBF data */
static const int he_mcs_groups_nss3_txbf[PPR_MAX_TX_CHAINS - 2][WL_NUM_TX_MODES] = {
/* 3 chains */
	{OFFSETOF(pprpbw_t, p_3x3_hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_3x3txbf_hess3)},
#if (PPR_MAX_TX_CHAINS > 3)
	{OFFSETOF(pprpbw_t, p_3x4_hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_3x4txbf_hess3)}
#endif /* PPR_MAX_TX_CHAINS > 3  */
};
#if (PPR_MAX_TX_CHAINS > 3)
static const int he_mcs_groups_nss4[WL_NUM_TX_MODES] = {
/* 4 chains only */
	OFFSETOF(pprpbw_t, p_4x4_hess4),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE,
};
/** mcs group with TXBF data */
static const int he_mcs_groups_nss4_txbf[WL_NUM_TX_MODES] = {
/* 4 chains only */
	OFFSETOF(pprpbw_t, p_4x4_hess4),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_t, p_4x4txbf_hess4)
};
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */

/** Get a pointer to the power values for the given rate group for a given channel bandwidth */
static int8 *ppr_get_he_mcs_group(pprpbw_t* bw_pwrs, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	int8 *pwr = NULL;
	int offset;

	switch (Nss) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
#if (PPR_MAX_TX_CHAINS > 3)
	case WL_TX_NSS_4:
		if (tx_chains == WL_TX_CHAINS_4) {
			if (mode == WL_TX_MODE_TXBF && PPR_TXBF_ENAB()) {
				 offset = he_mcs_groups_nss4_txbf[mode];
			} else {
				offset = he_mcs_groups_nss4[mode];
			}
			if (offset != OFFSNONE) {
				pwr = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 3 */
	case WL_TX_NSS_3:
		if ((tx_chains >= WL_TX_CHAINS_3) && (tx_chains <= PPR_MAX_TX_CHAINS)) {
			if (PPR_TXBF_ENAB()) {
				offset = he_mcs_groups_nss3_txbf[tx_chains - Nss][mode];
			} else {
				offset = he_mcs_groups_nss3[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				pwr = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 2 */
	case WL_TX_NSS_2:
		if ((tx_chains >= WL_TX_CHAINS_2) && (tx_chains <= PPR_MAX_TX_CHAINS)) {
			if (PPR_TXBF_ENAB()) {
				offset = he_mcs_groups_nss2_txbf[tx_chains - Nss][mode];
			} else {
				offset = he_mcs_groups_nss2[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				pwr = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_NSS_1:
		if (tx_chains <= PPR_MAX_TX_CHAINS) {
			if (PPR_TXBF_ENAB()) {
				offset = he_mcs_groups_nss1_txbf[tx_chains - Nss][mode];
			} else {
				offset = he_mcs_groups_nss1[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				pwr = (int8*)bw_pwrs + offset;
			}
		}
		else
			ASSERT(0);
		break;
	default:
		ASSERT(0);
		break;
	}
	return pwr;
}

/** Get the HE MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_get_he_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, ppr_he_mcs_rateset_t *he, wl_he_rate_type_t type)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;
	int offset = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_he_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			switch (type) {
			case WL_HE_RT_SU:
				offset = 0;
				break;
			case WL_HE_RT_UB:
				offset = WL_RATESET_SZ_HE_MCS;
				break;
			case WL_HE_RT_LUB:
				offset = 2 * WL_RATESET_SZ_HE_MCS;
				break;
			default:
				PPR_ERROR(("%s: Wrong type %d. BW:%d Nss:%d mode:%d TX_Chain:%d\n",
					__FUNCTION__, type, bw, Nss, mode, tx_chains));
				ASSERT(0);
				memset(he->pwr, (int8)WL_RATE_DISABLED, sizeof(*he));
				return cnt;
			}
			powers += offset;
			bcopy(powers, he->pwr, sizeof(*he));
			cnt = sizeof(*he);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, he));
		memset(he->pwr, (int8)WL_RATE_DISABLED, sizeof(*he));
	}
	return cnt;
}

int ppr_set_he_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const ppr_he_mcs_rateset_t *he, wl_he_rate_type_t type)
{
	pprpbw_t *bw_pwrs;
	int8 *power;
	int cnt = 0;
	int offset = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		power = (int8 *)ppr_get_he_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (power != NULL) {
			switch (type) {
			case WL_HE_RT_SU:
				offset = 0;
				break;
			case WL_HE_RT_UB:
				offset = WL_RATESET_SZ_HE_MCS;
				break;
			case WL_HE_RT_LUB:
				offset = 2 * WL_RATESET_SZ_HE_MCS;
				break;
			default:
				PPR_ERROR(("%s: Wrong type %d. BW:%d Nss:%d mode:%d TX_Chain:%d\n",
					__FUNCTION__, type, bw, Nss, mode, tx_chains));
				ASSERT(0);
				return cnt;
			}
			power += offset;
			bcopy(he->pwr, power, sizeof(*he));
			cnt = sizeof(*he);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, he));
	}
	return cnt;

}

int ppr_set_same_he_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const int8 power, wl_he_rate_type_t type)
{
	pprpbw_t *bw_pwrs;
	int8 *dest_group;
	int cnt = 0, offset = 0, i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8 *)ppr_get_he_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (dest_group != NULL) {
			switch (type) {
			case WL_HE_RT_SU:
				offset = 0;
				break;
			case WL_HE_RT_UB:
				offset = WL_RATESET_SZ_HE_MCS;
				break;
			case WL_HE_RT_LUB:
				offset = 2 * WL_RATESET_SZ_HE_MCS;
				break;
			default:
				PPR_ERROR(("%s: Wrong type %d. BW:%d Nss:%d mode:%d TX_Chain:%d\n",
					__FUNCTION__, type, bw, Nss, mode, tx_chains));
				ASSERT(0);
				return cnt;
			}
			dest_group += offset;
			cnt = sizeof(ppr_he_mcs_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains));
	}
	return cnt;
}

static const int he_ru_mcs_groups_nss1[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_ru_t, p_1x1hess1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_1x2cdd_hess1),
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_1x3cdd_hess1),
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_1x4cdd_hess1),
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
};

/** he ru mcs group with TXBF data */
static const int he_ru_mcs_groups_nss1_txbf[PPR_MAX_TX_CHAINS][WL_NUM_TX_MODES] = {
	/* WL_TX_MODE_NONE
	   WL_TX_MODE_STBC
	   WL_TX_MODE_CDD
	   WL_TX_MODE_TXBF
	*/
	/* 1 chain */
	{OFFSETOF(pprpbw_ru_t, p_1x1hess1),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 1)
	/* 2 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_1x2cdd_hess1),
	OFFSETOF(pprpbw_ru_t, p_1x2txbf_hess1)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_1x3cdd_hess1),
	OFFSETOF(pprpbw_ru_t, p_1x3txbf_hess1)},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_1x4cdd_hess1),
	OFFSETOF(pprpbw_ru_t, p_1x4txbf_hess1)}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */
};

#if (PPR_MAX_TX_CHAINS > 1)
static const int he_ru_mcs_groups_nss2[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_ru_t, p_2x2hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_ru_t, p_2x3hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSETOF(pprpbw_ru_t, p_2x4hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
};

/** he ru mcs group with TXBF data */
static const int he_ru_mcs_groups_nss2_txbf[PPR_MAX_TX_CHAINS - 1][WL_NUM_TX_MODES] = {
	/* 2 chain */
	{OFFSETOF(pprpbw_ru_t, p_2x2hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_2x2txbf_hess2)},
#if (PPR_MAX_TX_CHAINS > 2)
	/* 3 chain */
	{OFFSETOF(pprpbw_ru_t, p_2x3hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_2x3txbf_hess2)},
#if (PPR_MAX_TX_CHAINS > 3)
	/* 4 chain */
	{OFFSETOF(pprpbw_ru_t, p_2x4hess2),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_2x4txbf_hess2)}
#endif /* PPR_MAX_TX_CHAINS > 3  */
#endif /* PPR_MAX_TX_CHAINS > 2 */
};

#if (PPR_MAX_TX_CHAINS > 2)
static const int he_ru_mcs_groups_nss3[PPR_MAX_TX_CHAINS - 2][WL_NUM_TX_MODES] = {
/* 3 chains */
	{OFFSETOF(pprpbw_ru_t, p_3x3hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE},
#if (PPR_MAX_TX_CHAINS > 3)
	{OFFSETOF(pprpbw_ru_t, p_3x4hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE}
#endif /* PPR_MAX_TX_CHAINS > 3  */
};

/** he ru mcs group with TXBF data */
static const int he_ru_mcs_groups_nss3_txbf[PPR_MAX_TX_CHAINS - 2][WL_NUM_TX_MODES] = {
/* 3 chains */
	{OFFSETOF(pprpbw_ru_t, p_3x3hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_3x3txbf_hess3)},
#if (PPR_MAX_TX_CHAINS > 3)
	{OFFSETOF(pprpbw_ru_t, p_3x4hess3),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_3x4txbf_hess3)}
#endif /* PPR_MAX_TX_CHAINS > 3  */
};

#if (PPR_MAX_TX_CHAINS > 3)
static const int he_ru_mcs_groups_nss4[WL_NUM_TX_MODES] = {
/* 4 chains only */
	OFFSETOF(pprpbw_ru_t, p_4x4hess4),
	OFFSNONE,
	OFFSNONE,
	OFFSNONE,
};

/** he ru mcs group with TXBF data */
static const int he_ru_mcs_groups_nss4_txbf[WL_NUM_TX_MODES] = {
/* 4 chains only */
	OFFSETOF(pprpbw_ru_t, p_4x4hess4),
	OFFSNONE,
	OFFSNONE,
	OFFSETOF(pprpbw_ru_t, p_4x4txbf_hess4)
};
#endif /* PPR_MAX_TX_CHAINS > 3 */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#endif /* PPR_MAX_TX_CHAINS > 1 */

/** Get a pointer to the ru mcs power values for the given rate group for a given bandwidth */
static int8* ppr_ru_get_mcs_group(pprpbw_ru_t* bw_pwrs, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	int8* group_pwrs = NULL;
	int offset;

	if (mode >= WL_NUM_TX_MODES) {
		ASSERT(0);
		return NULL;
	}

	switch (Nss) {
#if (PPR_MAX_TX_CHAINS > 1)
#if (PPR_MAX_TX_CHAINS > 2)
#if (PPR_MAX_TX_CHAINS > 3)
	case WL_TX_NSS_4:
		if (tx_chains == WL_TX_CHAINS_4) {
			if (mode == WL_TX_MODE_TXBF && PPR_TXBF_ENAB()) {
				offset = he_ru_mcs_groups_nss4_txbf[mode];
			} else {
				offset = he_ru_mcs_groups_nss4[mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		} else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 3 */
	case WL_TX_NSS_3:
		if ((tx_chains >= WL_TX_CHAINS_3) && (tx_chains <= PPR_MAX_TX_CHAINS)) {
			if (PPR_TXBF_ENAB()) {
				offset = he_ru_mcs_groups_nss3_txbf[tx_chains - Nss][mode];
			} else {
				offset = he_ru_mcs_groups_nss3[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		} else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 2 */
	case WL_TX_NSS_2:
		if ((tx_chains >= WL_TX_CHAINS_2) && (tx_chains <= PPR_MAX_TX_CHAINS)) {
			if (PPR_TXBF_ENAB()) {
				offset = he_ru_mcs_groups_nss2_txbf[tx_chains - Nss][mode];
			} else {
				offset = he_ru_mcs_groups_nss2[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		} else
			ASSERT(0);
		break;
#endif /* PPR_MAX_TX_CHAINS > 1 */
	case WL_TX_NSS_1:
		if (tx_chains <= PPR_MAX_TX_CHAINS) {
			if (PPR_TXBF_ENAB()) {
				offset = he_ru_mcs_groups_nss1_txbf[tx_chains - Nss][mode];
			} else {
				offset = he_ru_mcs_groups_nss1[tx_chains - Nss][mode];
			}
			if (offset != OFFSNONE) {
				group_pwrs = (int8*)bw_pwrs + offset;
			}
		} else
			ASSERT(0);
		break;
	default:
		ASSERT(0);
		break;
	}
	return group_pwrs;
}

/* Get the HE RU MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_get_he_ru_mcs(ppr_ru_t* ppr_ru_ptr, wl_he_rate_type_t type, wl_tx_nss_t Nss,
	wl_tx_mode_t mode, wl_tx_chains_t tx_chains, ppr_he_mcs_rateset_t* mcs)
{
	pprpbw_ru_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(ppr_ru_ptr);
	bw_pwrs = ppr_get_ru_powers(ppr_ru_ptr, type);
	if (bw_pwrs != NULL) {
		powers = ppr_ru_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(powers, mcs->pwr, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("ppr_get_he_ru_mcs: Failed ppr_ru_t:%p ru type:%d Nss:%d"
			" mode:%d TX_Chain:%d rateset:%p\n",
			ppr_ru_ptr, type, Nss, mode, tx_chains, mcs));
		memset(mcs->pwr, (int8)WL_RATE_DISABLED, sizeof(*mcs));
	}
	return cnt;

}

/* Set the HE RU MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_he_ru_mcs(ppr_ru_t* ppr_ru_ptr, wl_he_rate_type_t type, wl_tx_nss_t Nss,
	wl_tx_mode_t mode, wl_tx_chains_t tx_chains, const ppr_he_mcs_rateset_t* mcs)
{
	pprpbw_ru_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_ru_powers(ppr_ru_ptr, type);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_ru_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(mcs->pwr, powers, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("ppr_set_he_ru_mcs: Failed ppr_ru_t:%p ru type:%d Nss:%d mode:%d"
			" TX_Chain:%d rateset:%p\n",
			ppr_ru_ptr, type, Nss, mode, tx_chains, mcs));
	}
	return cnt;

}

/* Set the HE RU MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_same_he_ru_mcs(ppr_ru_t* ppr_ru_ptr, wl_he_rate_type_t type, wl_tx_nss_t Nss,
	wl_tx_mode_t mode, wl_tx_chains_t tx_chains, const int8 power)
{
	pprpbw_ru_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_ru_powers(ppr_ru_ptr, type);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_ru_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_he_mcs_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("ppr_set_same_he_mcs: Failed ppr_ru_t:%p ru type:%d Nss:%d mode:%d"
			" TX_Chain:%d power:%d\n",
			ppr_ru_ptr, type, Nss, mode, tx_chains, power));
	}
	return cnt;
}

/** Size routine for user alloc/dealloc */
static uint32 ppr_pwrs_size(uint32 hdr)
{
	uint32 size;

	switch ((hdr & PPR_HDR_CUR_BW_MASK)) {
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
		size = sizeof(ppr_bw_20_t);
	break;
	case WL_TX_BW_40:
		size = sizeof(ppr_bw_40_t);
	break;
	case WL_TX_BW_80:
		size = sizeof(ppr_bw_80_t);
	break;
#ifdef WL11AC_160
	case WL_TX_BW_160:
		size = sizeof(ppr_bw_160_t);
	break;
#endif // endif
#ifdef WL11AC_80P80
	case WL_TX_BW_8080:
		size = sizeof(ppr_bw_8080_t);
	break;
#endif // endif
	default:
		ASSERT(0);
		size = 0;
	break;
	}
	return size;
}

/** Initialization routine */
void ppr_init(ppr_t *pprptr, wl_tx_bw_t bw)
{
	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}
	memset(pprptr, (int8)WL_RATE_DISABLED, ppr_size(bw));
	pprptr->hdr = 0;
	pprptr->hdr |= (bw & PPR_HDR_CUR_BW_MASK);
	pprptr->hdr |= ((bw << PPR_HDR_ALLOC_BW_SHIFT) & PPR_HDR_ALLOC_BW_MASK);
}

/** Reinitialization routine for opaque PPR struct */
void ppr_clear(ppr_t *pprptr)
{
	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}
	memset((uchar*)&pprptr->ppr_bw, (int8)WL_RATE_DISABLED,
		ppr_pwrs_size(pprptr->hdr));
}

void ppr_ru_set_same_value(ppr_ru_t* ppr_ru_ptr, int8 val)
{
	memset(ppr_ru_ptr, val, sizeof(ppr_ru_t));
	ppr_ru_ptr->hdr = 0;
}

/* Initialization routine for opaque PPR RU struct */
void ppr_ru_clear(ppr_ru_t* ppr_ru_ptr)
{
	memset(ppr_ru_ptr, (int8)WL_RATE_DISABLED, sizeof(ppr_ru_t));
	ppr_ru_ptr->hdr = 0;
}

uint32 ppr_ru_size(void)
{
	return sizeof(ppr_ru_t);
}

/** Size routine for user alloc/dealloc */
uint32 ppr_size(wl_tx_bw_t bw)
{
	uint32 ret = ppr_pwrs_size(bw) + sizeof(wl_tx_bw_t);
	ASSERT(ret <= MAX_PPR_SIZE);
	return ret;
}

/** Size routine for user serialization alloc */
uint32 ppr_ser_size(const ppr_t *pprptr)
{
	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return 0;
	}
	return ppr_pwrs_size(pprptr->hdr) + SER_HDR_LEN;	/* struct size plus headers */
}

/** Size routine for user serialization alloc */
uint32 ppr_ser_size_by_bw(wl_tx_bw_t bw)
{
	return ppr_pwrs_size(bw) + SER_HDR_LEN;
}

/** Constructor routine for opaque PPR struct */
ppr_t* ppr_create(osl_t *osh, wl_tx_bw_t bw)
{
	ppr_t* pprptr;

	ASSERT(ppr_is_valid_bw(bw));
#ifndef BCMDRIVER
	BCM_REFERENCE(osh);
	if ((pprptr = (ppr_t*)malloc((uint)ppr_size(bw))) != NULL) {
#else
	if ((pprptr = (ppr_t*)MALLOC_NOPERSIST(osh, (uint)ppr_size(bw))) != NULL) {
#endif // endif
		ppr_init(pprptr, bw);
	} else {
		PPR_ERROR(("%s: MALLOC(%d) failed\n", __FUNCTION__, (int)ppr_size(bw)));
	}
	return pprptr;
}

/* Constructor routine for opaque PPR struct on pre-alloc memory */
ppr_t* ppr_create_prealloc(wl_tx_bw_t bw, int8 *buf, uint len)
{
	ppr_t* pprptr = NULL;
	ASSERT(ppr_is_valid_bw(bw));

	if (ppr_size(bw) <= len) {
		pprptr = (ppr_t*) buf;
		ppr_init(pprptr, bw);
		pprptr->hdr |= PPR_HDR_FLAG_PREALLOC;
	} else {
		PPR_ERROR(("%s: Insufficient mem(%d), need %d\n", __FUNCTION__, len,
				(int)ppr_size(bw)));
		ASSERT(0);
	}
	return pprptr;
}

/* Constructor routine for opaque PPR RU struct */
ppr_ru_t* ppr_ru_create(osl_t* osh)
{
	ppr_ru_t* ppr_ru_ptr;

#ifndef BCMDRIVER
	BCM_REFERENCE(osh);
	if ((ppr_ru_ptr = (ppr_ru_t*)malloc(sizeof(ppr_ru_t))) != NULL) {
#else
	if ((ppr_ru_ptr = (ppr_ru_t*)MALLOC_NOPERSIST(osh, sizeof(ppr_ru_t))) != NULL) {
#endif // endif
		ppr_ru_clear(ppr_ru_ptr);
	} else {
		PPR_ERROR(("ppr_ru_create: MALLOC(%d) failed\n", (uint32)sizeof(ppr_ru_t)));
	}
	return ppr_ru_ptr;
}

/* Constructor routine for opaque PPR RU struct on pre-alloc memory */
ppr_ru_t* ppr_ru_create_prealloc(int8 *buf, uint len)
{
	ppr_ru_t* ppr_ru_ptr = NULL;

	if (sizeof(ppr_ru_t) <= len) {
		ppr_ru_ptr = (ppr_ru_t*) buf;
		ppr_ru_clear(ppr_ru_ptr);
		ppr_ru_ptr->hdr |= PPR_HDR_FLAG_PREALLOC;
	} else {
		PPR_ERROR(("ppr_create_ru_prealloc: Insufficient mem(%d), need %d\n", len,
				(uint32)sizeof(ppr_ru_t)));
		ASSERT(0);
	}
	return ppr_ru_ptr;
}

/* Destructor routine for opaque PPR RU struct */
void ppr_ru_delete(osl_t* osh, ppr_ru_t* ppr_ru_ptr)
{
	if (ppr_ru_ptr->hdr & PPR_HDR_FLAG_PREALLOC)
		return;

#ifndef BCMDRIVER
	BCM_REFERENCE(osh);
	free(ppr_ru_ptr);
#else
	MFREE(osh, ppr_ru_ptr, sizeof(*ppr_ru_ptr));
#endif // endif
}

/**
 * Init flags in the memory block for serialization, the serializer will check
 * the flag to decide which ppr to be copied
 */
int ppr_init_ser_mem_by_bw(uint8* pbuf, wl_tx_bw_t bw, uint32 len)
{
	ppr_ser_mem_flag_t *pmflag;

	if (pbuf == NULL || ppr_ser_size_by_bw(bw) > len)
		return BCME_BADARG;

	pmflag = (ppr_ser_mem_flag_t *)pbuf;
	pmflag->magic_word = HTON32(PPR_SER_MEM_WORD);
	pmflag->flag   = HTON32(ppr_get_flag());

	/* init the memory */
	memset(pbuf + sizeof(*pmflag), (uint8)WL_RATE_DISABLED, len-sizeof(*pmflag));
	return BCME_OK;
}

int ppr_init_ser_mem(uint8* pbuf, ppr_t * ppr, uint32 len)
{
	return ppr_init_ser_mem_by_bw(pbuf, ppr->hdr, len);
}

/** Destructor routine for opaque PPR struct */
void ppr_delete(osl_t *osh, ppr_t *pprptr)
{
	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}
	ASSERT(ppr_is_valid_bw(ppr_get_cur_bw(pprptr)));

	if (pprptr->hdr & PPR_HDR_FLAG_PREALLOC)
		return;

#ifndef BCMDRIVER
	BCM_REFERENCE(osh);
	free(pprptr);
#else
	MFREE(osh, pprptr, (uint)ppr_size(ppr_get_alloc_bw(pprptr->hdr)));
#endif // endif
}

/* Update the bw for the given opaque PPR struct
 * This function is used when an opaque PPR struct has been allocated
 * with enough space for the given bandwidth.
 * USE WITH CAUTION
 */
void ppr_set_ch_bw(ppr_t *pprptr, wl_tx_bw_t bw)
{
	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}
	ASSERT(ppr_size(ppr_get_alloc_bw(pprptr->hdr)) >= ppr_size(bw));
	pprptr->hdr &= ~PPR_HDR_CUR_BW_MASK;
	pprptr->hdr |= (bw & PPR_HDR_CUR_BW_MASK);
}

/** Type routine for inferring opaque structure size */
wl_tx_bw_t ppr_get_ch_bw(const ppr_t *pprptr)
{
	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return 0;
	}
	return ppr_get_cur_bw(pprptr);
}

/** Type routine to get ppr supported maximum bw */
wl_tx_bw_t ppr_get_max_bw(void)
{
	return PPR_BW_MAX;
}

/** Get the dsss values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_get_dsss(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_chains_t tx_chains,
	ppr_dsss_rateset_t* dsss)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_dsss_group(bw_pwrs, tx_chains);
		if (powers != NULL) {
			bcopy(powers, dsss->pwr, sizeof(*dsss));
			cnt = sizeof(*dsss);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, tx_chains, dsss));
		memset(dsss->pwr, (int8)WL_RATE_DISABLED, sizeof(*dsss));
	}
	return cnt;
}

/** Get the ofdm values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_get_ofdm(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains,
	ppr_ofdm_rateset_t* ofdm)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_ofdm_group(bw_pwrs, mode, tx_chains);
		if (powers != NULL) {
			bcopy(powers, ofdm->pwr, sizeof(*ofdm));
			cnt = sizeof(*ofdm);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, mode, tx_chains, ofdm));
		memset(ofdm->pwr, (int8)WL_RATE_DISABLED, sizeof(*ofdm));
	}
	return cnt;
}

/**
 * Get the HT MCS values for the group specified by Nss, with the given bw and tx chains. Function
 * is not called by ACPHY code, but even in case of ACPHY, function is called by wlc_channel.c.
 */
int ppr_get_ht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, ppr_ht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(powers, mcs->pwr, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, mcs));
		memset(mcs->pwr, (int8)WL_RATE_DISABLED, sizeof(*mcs));
	}

	return cnt;
}

/** Get the VHT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_get_vht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, ppr_vht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int cnt = 0;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(powers, mcs->pwr, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, mcs));
		memset(mcs->pwr, (int8)WL_RATE_DISABLED, sizeof(*mcs));
	}
	return cnt;
}

#define TXPPR_TXPWR_MAX 0x7f             /* WLC_TXPWR_MAX */

/**
 * Get the minimum power for a VHT MCS rate specified by Nss, with the given bw and tx chains.
 * Disabled rates are ignored
 */
int ppr_get_vht_mcs_min(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
 wl_tx_chains_t tx_chains, int8* mcs_min)
{
	pprpbw_t* bw_pwrs;
	const int8* powers;
	int result = BCME_ERROR;
	uint i = 0;

	*mcs_min = TXPPR_TXPWR_MAX;

	ASSERT(pprptr);
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			for (i = 0; i < sizeof(ppr_vht_mcs_rateset_t); i++) {
				/* ignore disabled rates! */
				if (powers[i] != WL_RATE_DISABLED)
					*mcs_min = MIN(*mcs_min, powers[i]);
			}
			result = BCME_OK;
		}
	}
	return result;
}

/* Routines to set target powers per rate in a group */

/** Set the dsss values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_dsss(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_chains_t tx_chains,
	const ppr_dsss_rateset_t* dsss)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_dsss_group(bw_pwrs, tx_chains);
		if (powers != NULL) {
			bcopy(dsss->pwr, powers, sizeof(*dsss));
			cnt = sizeof(*dsss);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, tx_chains, dsss));
	}
	return cnt;
}

/** Set the ofdm values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_ofdm(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains,
	const ppr_ofdm_rateset_t* ofdm)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_ofdm_group(bw_pwrs, mode, tx_chains);
		if (powers != NULL) {
			bcopy(ofdm->pwr, powers, sizeof(*ofdm));
			cnt = sizeof(*ofdm);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, mode, tx_chains, ofdm));
	}
	return cnt;
}

/**
 * Set the HT MCS values for the group specified by Nss, with the given bw and tx chains. Function
 * is not called by ACPHY code, but even in case of ACPHY, function is called by wlc_channel.c.
 */
int ppr_set_ht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const ppr_ht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(mcs->pwr, powers, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, mcs));
	}
	return cnt;
}

/** Set the VHT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_vht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const ppr_vht_mcs_rateset_t* mcs)
{
	pprpbw_t* bw_pwrs;
	int8* powers;
	int cnt = 0;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		powers = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (powers != NULL) {
			bcopy(mcs->pwr, powers, sizeof(*mcs));
			cnt = sizeof(*mcs);
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d rateset:%p\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, mcs));
	}
	return cnt;
}

/* Routines to set rate groups to a single target value */

/** Set the dsss values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_same_dsss(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_chains_t tx_chains, const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_dsss_group(bw_pwrs, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_dsss_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d TX_Chain:%d power:%d\n",
			__FUNCTION__, pprptr, bw, tx_chains, power));
	}
	return cnt;
}

/** Set the ofdm values for the given number of tx_chains and 20, 20in40, etc. */
int ppr_set_same_ofdm(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains,
	const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_ofdm_group(bw_pwrs, mode, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_ofdm_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d mode:%d TX_Chain:%d power:%d\n",
			__FUNCTION__, pprptr, bw, mode, tx_chains, power));
	}
	return cnt;
}

/** Set the HT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_same_ht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_ht_mcs_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d power:%d\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, power));
	}
	return cnt;
}

/** Set the HT MCS values for the group specified by Nss, with the given bw and tx chains */
int ppr_set_same_vht_mcs(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains, const int8 power)
{
	pprpbw_t* bw_pwrs;
	int8* dest_group;
	int cnt = 0;
	int i;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		dest_group = (int8*)ppr_get_mcs_group(bw_pwrs, Nss, mode, tx_chains);
		if (dest_group != NULL) {
			cnt = sizeof(ppr_vht_mcs_rateset_t);
			for (i = 0; i < cnt; i++)
				*dest_group++ = power;
		}
	}
	if (cnt == 0) {
		PPR_ERROR(("%s: Failed ppr_t:%p BW:%d Nss:%d mode:%d TX_Chain:%d power:%d\n",
			__FUNCTION__, pprptr, bw, Nss, mode, tx_chains, power));
	}
	return cnt;
}

/* Helper routines to operate on the entire ppr set */

/** Ensure no rate limit is greater than the cap */
uint ppr_apply_max(ppr_t *pprptr, int8 maxval)
{
	uint i = 0;
	int8 *rptr;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return i;
	}

	rptr = (int8*)&pprptr->ppr_bw;
	size = ppr_pwrs_size(pprptr->hdr);

	for (i = 0; i < size; i++, rptr++) {
		*rptr = MIN(*rptr, maxval);
	}
	return i;
}

/** number of non proprietary 802.11n single stream MCS'es excluding MCS32 */
#define N_HT_SS_MCS	8

/**
 * Before the OLPC calibration (needs to send out some training frames to converge TX Power control)
 * rates need to be disabled that cannot be controlled to with the accuracy needed to avoid
 * regulatory violations. Once the calibration completes a newer reduced threshold is set to enable
 * all valid rates.
 *
 * Check for any power in the rate group at or below the threshold. If any is found, set the entire
 * group to WL_RATE_DISABLED. An exception is made for HT 87 and 88 and VHT 8 and 9 which will not
 * cause the entire group to be disabled if they are disabled or below the threshold.
 */
static void ppr_force_disabled_group(int8* powers, int8 threshold, uint len)
{
	uint i;

	for (i = 0; (i < len) && (i < N_HT_SS_MCS); i++) {
		/* if we find a below-threshold rate in the set... */
		if (powers[i] < threshold) {
			/* disable the entire rate set and return */
			for (i = 0; i < len; i++) {
				powers[i] = WL_RATE_DISABLED;
			}
			return;
		}
	}
	/* VHT 8 and 11 can be disabled separately */
	for (; i < len; i++) {
		if (powers[i] < threshold) {
			powers[i] = WL_RATE_DISABLED;
		}
	}
}

/**
 * Before the OLPC calibration (needs to send out some training frames to converge TX Power control)
 * rates need to be disabled that cannot be controlled to with the accuracy needed to avoid
 * regulatory violations. Once the calibration completes a newer reduced threshold is set to enable
 * all valid rates.
 *
 * Make low power rates (below the threshold) explicitly disabled. If one rate in a group is
 * disabled, disable the whole group.
 */
int ppr_force_disabled(ppr_t* pprptr, int8 threshold)
{
	wl_tx_bw_t bw;

	for (bw = WL_TX_BW_20; bw <= WL_TX_BW_80; bw++) {
		pprpbw_t* bw_pwrs = ppr_get_bw_powers(pprptr, bw);

		if (bw_pwrs != NULL) {
			int8* powers;
			int8* mcs_powers;

			powers = (int8*)ppr_get_dsss_group(bw_pwrs, WL_TX_CHAINS_1);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_DSSS);

			powers = (int8*)ppr_get_ofdm_group(bw_pwrs,
				WL_TX_MODE_NONE, WL_TX_CHAINS_1);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
				WL_TX_MODE_NONE, WL_TX_CHAINS_1);
			ASSERT(mcs_powers);
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN1_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS_P) {
				ppr_force_disabled_group(powers, threshold,
					WL_RATESET_SZ_VHT_MCS_P);
			}

#if (PPR_MAX_TX_CHAINS > 1)
			powers = (int8*)ppr_get_dsss_group(bw_pwrs, WL_TX_CHAINS_2);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_DSSS);

			powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_CDD, WL_TX_CHAINS_2);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2);
			ASSERT(mcs_powers);
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN2_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS_P) {
				ppr_force_disabled_group(powers, threshold,
					WL_RATESET_SZ_VHT_MCS_P);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_2);
				ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_2);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN2_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS_P) {
					ppr_force_disabled_group(powers, threshold,
						WL_RATESET_SZ_VHT_MCS_P);
				}
			}
#endif /* WL_BEAMFORMING */

#if (PPR_MAX_TX_CHAINS > 2)
			powers = (int8*)ppr_get_dsss_group(bw_pwrs, WL_TX_CHAINS_3);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_DSSS);

			powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_CDD, WL_TX_CHAINS_3);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3);
			ASSERT(mcs_powers);
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN3_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS_P) {
				ppr_force_disabled_group(powers, threshold,
					WL_RATESET_SZ_VHT_MCS_P);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_3);
				ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_3);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN3_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS_P) {
					ppr_force_disabled_group(powers, threshold,
						WL_RATESET_SZ_VHT_MCS_P);
				}
			}
#endif /* WL_BEAMFORMING */
#if (PPR_MAX_TX_CHAINS > 3)
			powers = (int8*)ppr_get_dsss_group(bw_pwrs, WL_TX_CHAINS_4);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_DSSS);

			powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_CDD, WL_TX_CHAINS_4);
			ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_4);
			ASSERT(mcs_powers);
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN4_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS_P) {
				ppr_force_disabled_group(powers, threshold,
					WL_RATESET_SZ_VHT_MCS_P);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				powers = (int8*)ppr_get_ofdm_group(bw_pwrs, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_4);
				ppr_force_disabled_group(powers, threshold, WL_RATESET_SZ_OFDM);

				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_4);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN4_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS_P) {
					ppr_force_disabled_group(powers, threshold,
						WL_RATESET_SZ_VHT_MCS_P);
				}
			}
#endif /* WL_BEAMFORMING */
#endif /* (PPR_MAX_TX_CHAINS > 3) */
#endif /* (PPR_MAX_TX_CHAINS > 2) */
#endif /* (PPR_MAX_TX_CHAINS > 1) */
		}
	}
	return BCME_OK;
}

#if (PPR_MAX_TX_CHAINS > 1)
/**
 * LCN20 PHY doesn't support VHT rates, but it seems some of its regulatory data does. We need
 * to explicitly disable the VHT rates so there's no temptation to use them.
 */
static void ppr_force_disabled_vht_in_group(int8* powers, uint len)
{
	uint i;

	for (i = N_HT_SS_MCS; (i < len); i++) {
		powers[i] = WL_RATE_DISABLED;
	}
}
#endif /* (PPR_MAX_TX_CHAINS > 1) */

/**
 * LCN20 PHY doesn't support VHT MIMO rates, but it seems some of its regulatory data does. We need
 * to explicitly disable the VHT MIMO rates so there's no temptation to use them.
 */
int
ppr_disable_vht_mimo_rates(ppr_t* pprptr)
{
#if (PPR_MAX_TX_CHAINS > 1)
	wl_tx_bw_t bw;

	for (bw = WL_TX_BW_20; bw <= WL_TX_BW_80; bw++) {
		pprpbw_t* bw_pwrs = ppr_get_bw_powers(pprptr, bw);

		if (bw_pwrs != NULL) {
			int8* mcs_powers;
			int8* powers;

			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2);
			ASSERT(mcs_powers);
			/* skip CDD */
			/* mcs_powers += WL_RATESET_SZ_VHT_MCS; */
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN2_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS) {
				ppr_force_disabled_vht_in_group(powers, WL_RATESET_SZ_VHT_MCS);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_2);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN2_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS) {
					ppr_force_disabled_vht_in_group(powers,
						WL_RATESET_SZ_VHT_MCS);
				}
			}
#endif /* WL_BEAMFORMING */

#if (PPR_MAX_TX_CHAINS > 2)
			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3);
			ASSERT(mcs_powers);
			/* skip CDD */
			/* mcs_powers += WL_RATESET_SZ_VHT_MCS; */
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN3_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS) {
				ppr_force_disabled_vht_in_group(powers, WL_RATESET_SZ_VHT_MCS);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_3);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN3_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS) {
					ppr_force_disabled_vht_in_group(powers,
						WL_RATESET_SZ_VHT_MCS);
				}
			}
#endif /* WL_BEAMFORMING */

#if (PPR_MAX_TX_CHAINS > 3)
			mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_4);
			ASSERT(mcs_powers);
			/* skip CDD */
			/* mcs_powers += WL_RATESET_SZ_VHT_MCS; */
			for (powers = mcs_powers; powers < &mcs_powers[PPR_CHAIN4_MCS_SIZE];
				powers += WL_RATESET_SZ_VHT_MCS) {
				ppr_force_disabled_vht_in_group(powers, WL_RATESET_SZ_VHT_MCS);
			}

#ifdef WL_BEAMFORMING
			if (PPR_TXBF_ENAB()) {
				mcs_powers = (int8*)ppr_get_mcs_group(bw_pwrs, WL_TX_NSS_1,
					WL_TX_MODE_TXBF, WL_TX_CHAINS_4);
				ASSERT(mcs_powers);
				for (powers = mcs_powers;
					powers < &mcs_powers[PPR_BF_CHAIN4_MCS_SIZE];
					powers += WL_RATESET_SZ_VHT_MCS) {
					ppr_force_disabled_vht_in_group(powers,
						WL_RATESET_SZ_VHT_MCS);
				}
			}
#endif /* WL_BEAMFORMING */
#endif /* (PPR_MAX_TX_CHAINS > 3) */
#endif /* (PPR_MAX_TX_CHAINS > 2) */
		}
	}
#endif /* (PPR_MAX_TX_CHAINS > 1) */
	return BCME_OK;
}

#if (PPR_MAX_TX_CHAINS > 1)
#define APPLY_CONSTRAINT(x, y, max) do {		\
		ret += (y - x);							\
		for (i = x; i < y; i++)					\
			pprbuf[i] = MIN(pprbuf[i], max);	\
	} while (0);

/** Apply appropriate single-, two- and three-chain constraints across the appropriate ppr block */
/* Also apply constraints on 4 chains */
static uint ppr_apply_constraint_to_block(int8* pprbuf, int8 constraint)
{
	uint ret = 0;
	uint i = 0;
	int8 constraint_2chain = constraint - QDB(3);
#if (PPR_MAX_TX_CHAINS > 2)
	int8 constraint_3chain = constraint - (QDB(4) + 3); /* - 4.75dBm */
#endif /* PPR_MAX_TX_CHAINS > 2 */
#if (PPR_MAX_TX_CHAINS > 3)
	int8 constraint_4chain = constraint - QDB(6); /* - 6.00dB */
#endif /* PPR_MAX_TX_CHAINS > 3 */

	APPLY_CONSTRAINT(PPR_CHAIN1_FIRST, PPR_CHAIN1_END, constraint);
	APPLY_CONSTRAINT(PPR_CHAIN2_FIRST, PPR_CHAIN2_END, constraint_2chain);
#if (PPR_MAX_TX_CHAINS > 2)
	APPLY_CONSTRAINT(PPR_CHAIN3_FIRST, PPR_CHAIN3_END, constraint_3chain);
#endif /* PPR_MAX_TX_CHAINS > 2 */
#if (PPR_MAX_TX_CHAINS > 3)
	APPLY_CONSTRAINT(PPR_CHAIN4_FIRST, PPR_CHAIN4_END, constraint_4chain);
#endif /* PPR_MAX_TX_CHAINS > 3 */
	return ret;
}
#endif /* (PPR_MAX_TX_CHAINS > 1) */

/**
 * Reduce total transmitted power to level of constraint.
 * For two chain rates, the per-antenna power must be halved.
 * For three chain rates, it must be a third of the constraint.
 */
uint ppr_apply_constraint_total_tx(ppr_t* pprptr, int8 constraint)
{
	uint ret = 0;

#if (PPR_MAX_TX_CHAINS > 1)
	int8* pprbuf;
	ASSERT(pprptr);

#if 1
	ret = ppr_apply_max(pprptr, constraint);
	return ret;
#endif

	/**
	 * TXPPR_TXPWR_MAX implies no constrains applied
	 * so skip applying constrains for multiple chains
	 */
	if (constraint == TXPPR_TXPWR_MAX)
		return ret;

	switch (ppr_get_cur_bw(pprptr)) {
	case WL_TX_BW_20:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch20.b20;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
	case WL_TX_BW_40:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch40.b40;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch40.b20in40;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
	case WL_TX_BW_80:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch80.b80;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch80.b20in80;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch80.b40in80;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
#ifdef WL11AC_160
	case WL_TX_BW_160:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch160.b160;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch160.b20in160;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch160.b40in160;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch160.b80in160;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
#endif /* WL11AC_160 */
#ifdef WL11AC_80P80
	case WL_TX_BW_8080:
		{
			pprbuf = (int8*)&pprptr->ppr_bw.ch8080.b80ch1;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch8080.b80ch2;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch8080.b80in8080;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch8080.b20in8080;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
			pprbuf = (int8*)&pprptr->ppr_bw.ch8080.b40in8080;
			ret += ppr_apply_constraint_to_block(pprbuf, constraint);
		}
		break;
#endif /* WL11AC_80P80 */
	default:
		ASSERT(0);
	}

#else
	ASSERT(pprptr);
	ret = ppr_apply_max(pprptr, constraint);
#endif /* PPR_MAX_TX_CHAINS > 1 */
	return ret;
}

/** Ensure no rate limit is lower than the specified minimum */
uint ppr_apply_min(ppr_t *pprptr, int8 minval)
{
	uint i = 0;
	int8 *rptr;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return i;
	}

	rptr = (int8*)&pprptr->ppr_bw;
	size = ppr_pwrs_size(pprptr->hdr);

	for (i = 0; i < size; i++, rptr++) {
		*rptr = MAX(*rptr, minval);
	}
	return i;
}

/** Ensure no rate limit in this ppr set is greater than the corresponding limit in ppr_cap */
uint ppr_apply_vector_ceiling(ppr_t *pprptr, const ppr_t* ppr_cap)
{
	uint i = 0, size;
	int8 *rptr;
	const int8 *capptr;

	if ((pprptr == NULL) || (ppr_cap == NULL)) {
		PPR_ERROR(("%s: ppr pointer is NULL\n", __FUNCTION__));
		ASSERT(0);
		return i;
	}

	rptr = (int8 *)&pprptr->ppr_bw;
	capptr = (const int8*)&ppr_cap->ppr_bw;

	if (ppr_get_cur_bw(pprptr) == ppr_get_cur_bw(ppr_cap)) {
		size = ppr_pwrs_size(pprptr->hdr);
		for (i = 0; i < size; i++, rptr++, capptr++) {
			*rptr = MIN(*rptr, *capptr);
		}
	}
	return i;
}

/** Ensure no rate limit in this ppr set is lower than the corresponding limit in ppr_min */
uint ppr_apply_vector_floor(ppr_t *pprptr, const ppr_t *ppr_min)
{
	uint i = 0, size;
	int8 *rptr;
	const int8 *minptr;

	if ((pprptr == NULL) || (ppr_min == NULL)) {
		PPR_ERROR(("%s: ppr pointer is NULL\n", __FUNCTION__));
		ASSERT(0);
		return i;
	}

	rptr = (int8 *)&pprptr->ppr_bw;
	minptr = (const int8 *)&ppr_min->ppr_bw;

	if (ppr_get_cur_bw(pprptr) == ppr_get_cur_bw(ppr_min)) {
		size = ppr_pwrs_size(pprptr->hdr);
		for (i = 0; i < size; i++, rptr++, minptr++) {
			*rptr = MAX((uint8)*rptr, (uint8)*minptr);
		}
	}
	return i;
}

/** Get the maximum power in the ppr set */
int8 ppr_get_max(ppr_t *pprptr)
{
	uint i;
	int8 *rptr;
	int8 maxval = WL_RATE_DISABLED;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr does not exist.\n", __FUNCTION__));
		ASSERT(0);
		return maxval;
	}

	size = ppr_pwrs_size(pprptr->hdr);
	rptr = (int8 *)&pprptr->ppr_bw;
	maxval = *rptr++;

	for (i = 1; i < size; i++, rptr++) {
		maxval = MAX(maxval, *rptr);
	}
	return maxval;
}

/* Get the maximum power in the ppr_ru set */
int8 ppr_ru_get_max(ppr_ru_t *pprptr)
{
	uint i;
	int8 *rptr;
	int8 maxval = WL_RATE_DISABLED;
	uint size = SIZE_RU_PPR;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr does not exist.\n", __FUNCTION__));
		ASSERT(0);
		return maxval;
	}

	rptr = (int8 *)&pprptr->ru26;
	maxval = *rptr++;

	for (i = 1; i < size; i++, rptr++) {
		maxval = MAX(maxval, *rptr);
	}
	return maxval;
}

/* Get the minimum power in the ppr_r set, excluding disallowed
 * rates and (possibly) powers set to the minimum for the phy
 */
int8 ppr_ru_get_min(ppr_ru_t* pprptr, int8 floor)
{
	uint i;
	int8 *rptr;
	int8 minval = WL_RATE_DISABLED;
	uint size = SIZE_RU_PPR;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr does not exist.\n", __FUNCTION__));
		ASSERT(0);
		return minval;
	}

	rptr = (int8 *)&pprptr->ru26;

	for (i = 0; (i < size) && ((minval == WL_RATE_DISABLED) || (minval == floor));
		i++, rptr++) {
		minval = *rptr;
	}
	for (; i < size; i++, rptr++) {
		if ((*rptr != WL_RATE_DISABLED) && (*rptr != floor))
			minval = MIN(minval, *rptr);
	}
	return minval;
}

/* Get the maximum power in the dsss ppr set */
int8 ppr_get_dsss_max(ppr_t* pprptr, wl_tx_bw_t bw, wl_tx_chains_t tx_chains)
{
	ppr_dsss_rateset_t dsss;
	uint8 rate;
	int8 maxval = 0;
	ppr_get_dsss(pprptr, bw, tx_chains, &dsss);
	maxval = dsss.pwr[0];
	for (rate = 1; rate < WL_RATESET_SZ_DSSS; rate++) {
		maxval = MAX(maxval, dsss.pwr[rate]);
	}
	return maxval;
}
/**
 * Get the minimum power in the ppr set, excluding disallowed
 * rates and (possibly) powers set to the minimum for the phy
 */
int8 ppr_get_min(ppr_t *pprptr, int8 floor)
{
	uint i;
	int8 *rptr;
	int8 minval = WL_RATE_DISABLED;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr does not exist.\n", __FUNCTION__));
		ASSERT(0);
		return minval;
	}

	rptr = (int8 *)&pprptr->ppr_bw;
	size = ppr_pwrs_size(pprptr->hdr);

	for (i = 0; (i < size) && ((minval == WL_RATE_DISABLED) || (minval == floor));
		i++, rptr++) {
		minval = *rptr;
	}
	for (; i < size; i++, rptr++) {
		if ((*rptr != WL_RATE_DISABLED) && (*rptr != floor))
			minval = MIN(minval, *rptr);
	}
	return minval;
}

/** Get the maximum power for a given bandwidth in the ppr set */
int8 ppr_get_max_for_bw(ppr_t* pprptr, wl_tx_bw_t bw)
{
	uint i;
	uint size = sizeof(pprpbw_t);
	const pprpbw_t* bw_pwrs;
	const int8* rptr;
	int8 maxval;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		rptr = (const int8*)bw_pwrs;
		maxval = *rptr++;

		for (i = 1; i < size; i++, rptr++) {
			maxval = MAX(maxval, *rptr);
		}
	} else {
		maxval = WL_RATE_DISABLED;
	}
	return maxval;
}

/** Get the minimum power for a given bandwidth  in the ppr set */
int8 ppr_get_min_for_bw(ppr_t* pprptr, wl_tx_bw_t bw)
{
	uint i;
	uint size = sizeof(pprpbw_t);
	const pprpbw_t* bw_pwrs;
	const int8* rptr;
	int8 minval;

	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	if (bw_pwrs != NULL) {
		rptr = (const int8*)bw_pwrs;
		minval = *rptr++;

		for (i = 1; i < size; i++, rptr++) {
			minval = MIN(minval, *rptr);
		}
	} else
		minval = WL_RATE_DISABLED;
	return minval;
}

/** Map the given function with its context value over the two power vectors */
void
ppr_map_vec_dsss(ppr_mapfn_t fn, void* context, ppr_t* pprptr1, ppr_t* pprptr2,
	wl_tx_bw_t bw, wl_tx_chains_t tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	ASSERT(pprptr1);
	ASSERT(pprptr2);

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_dsss_group(bw_pwrs1, tx_chains);
		powers2 = (int8*)ppr_get_dsss_group(bw_pwrs2, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_DSSS; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}

/** Map the given function with its context value over the two power vectors */
void
ppr_map_vec_ofdm(ppr_mapfn_t fn, void* context, ppr_t* pprptr1, ppr_t* pprptr2,
	wl_tx_bw_t bw, wl_tx_mode_t mode, wl_tx_chains_t tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_ofdm_group(bw_pwrs1, mode, tx_chains);
		powers2 = (int8*)ppr_get_ofdm_group(bw_pwrs2, mode, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_OFDM; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}

/** Map the given function with its context value over the two power vectors */
void
ppr_map_vec_ht_mcs(ppr_mapfn_t fn, void* context, ppr_t* pprptr1,
	ppr_t* pprptr2, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode,
	wl_tx_chains_t tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_mcs_group(bw_pwrs1, Nss, mode, tx_chains);
		powers2 = (int8*)ppr_get_mcs_group(bw_pwrs2, Nss, mode, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_HT_MCS; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}

/** Map the given function with its context value over the two power vectors */
void
ppr_map_vec_vht_mcs(ppr_mapfn_t fn, void* context, ppr_t* pprptr1,
	ppr_t* pprptr2, wl_tx_bw_t bw, wl_tx_nss_t Nss, wl_tx_mode_t mode, wl_tx_chains_t
	tx_chains)
{
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* powers1;
	int8* powers2;
	uint i;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);
	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		powers1 = (int8*)ppr_get_mcs_group(bw_pwrs1, Nss, mode, tx_chains);
		powers2 = (int8*)ppr_get_mcs_group(bw_pwrs2, Nss, mode, tx_chains);
		if ((powers1 != NULL) && (powers2 != NULL)) {
			for (i = 0; i < WL_RATESET_SZ_VHT_MCS; i++)
				(fn)(context, (uint8*)powers1++, (uint8*)powers2++);
		}
	}
}

/* Map the given function with its context value over the two power vectors */
bool
ppr_ru_map_vec_all(ppr_mapfn_t fn, void *context, ppr_ru_t *pprptr1, ppr_ru_t *pprptr2)
{
	uint i;
	int8 *rptr1;
	int8 *rptr2;
	uint size = SIZE_RU_PPR;
	bool ret = TRUE;

	if ((pprptr1 != NULL) && (pprptr2 != NULL)) {
		rptr1 = (int8 *)&pprptr1->ru26;
		rptr2 = (int8 *)&pprptr2->ru26;
		for (i = 0; i < size; i++, rptr1++, rptr2++) {
			(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
		}
	} else {
		PPR_ERROR(("%s: pprptr does not exist.\n", __FUNCTION__));
		ret = FALSE;
	}
	return ret;
}

static bool
ppr_map_vec_per_bw(ppr_mapfn_t fn, void* context, ppr_t* pprptr1, ppr_t* pprptr2, wl_tx_bw_t bw)
{
	uint i, size;
	pprpbw_t* bw_pwrs1;
	pprpbw_t* bw_pwrs2;
	int8* rptr1;
	int8* rptr2;
	bool ret = TRUE;

	bw_pwrs1 = ppr_get_bw_powers(pprptr1, bw);
	bw_pwrs2 = ppr_get_bw_powers(pprptr2, bw);

	if ((bw_pwrs1 != NULL) && (bw_pwrs2 != NULL)) {
		rptr1 = (int8*)bw_pwrs1;
		rptr2 = (int8*)bw_pwrs2;
		size = sizeof(pprpbw_t);
		for (i = 0; i < size; i++, rptr1++, rptr2++) {
			(fn)(context, (uint8*)rptr1, (uint8*)rptr2);
		}
	} else {
		ret = FALSE;
	}
	return ret;
}

/** Map the given function with its context value over the two power vectors */
void
ppr_map_vec_all(ppr_mapfn_t fn, void* context, ppr_t* pprptr1, ppr_t* pprptr2)
{
	wl_tx_bw_t bw1, bw2;

	bw1 = ppr_get_cur_bw(pprptr1);
	bw2 = ppr_get_cur_bw(pprptr2);
	if (bw1 != bw2) {
		PPR_ERROR(("%s: BW mismatch. BW1 %d BW2 %d.\n", __FUNCTION__, bw1, bw2));
		ASSERT(0);
		return;
	}
	switch (bw1) {
	case WL_TX_BW_20:
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_20);
		break;
	case WL_TX_BW_40:
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_20IN40);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_40);
		break;
	case WL_TX_BW_80:
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_20IN80);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_40IN80);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_80);
		break;
#ifdef WL11AC_160
	case WL_TX_BW_160:
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_20IN160);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_40IN160);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_80IN160);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_160);
		break;
#endif /* WL11AC_160 */
#ifdef WL11AC_80P80
	case WL_TX_BW_8080:
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_20IN8080);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_40IN8080);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_80IN8080);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_8080);
		ppr_map_vec_per_bw(fn, context, pprptr1, pprptr2, WL_TX_BW_8080CHAN2);
		break;
#endif /* WL11AC_80P80 */
	default:
		PPR_ERROR(("%s: Incorrect BW (%d).\n", __FUNCTION__, bw1));
		ASSERT(0);
		break;
	}
}

/** Set PPR struct to a certain power level */
void
ppr_set_cmn_val(ppr_t *pprptr, int8 val)
{
	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}
	memset((uchar*)&pprptr->ppr_bw, val, ppr_pwrs_size(pprptr->hdr));
}

void
ppr_copy_ru_struct(ppr_ru_t *s, ppr_ru_t *d)
{
	memcpy(d, s, sizeof(ppr_ru_t));
}

/** Make an identical copy of a ppr structure (for ppr_bw==all case) */
void
ppr_copy_struct(ppr_t* pprptr_s, ppr_t* pprptr_d)
{
	int8* rptr_s = (int8*)&pprptr_s->ppr_bw;
	int8* rptr_d = (int8*)&pprptr_d->ppr_bw;
	/* ASSERT(ppr_pwrs_size(pprptr_d->hdr) >= ppr_pwrs_size(pprptr_s->hdr)); */

	if (ppr_get_cur_bw(pprptr_s) ==
		ppr_get_cur_bw(pprptr_d))
		memcpy(rptr_d, rptr_s, ppr_pwrs_size(pprptr_s->hdr));
	else {
		const pprpbw_t* src_bw_pwrs;
		pprpbw_t* dest_bw_pwrs;

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_40);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_40);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20IN40);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20IN40);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_80);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_80);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20IN80);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20IN80);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_40IN80);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_40IN80);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_160);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_160);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20IN160);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20IN160);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_40IN160);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_40IN160);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_80IN160);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_80IN160);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_8080);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_8080);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_8080CHAN2);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_8080CHAN2);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_20IN8080);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_20IN8080);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_40IN8080);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_40IN8080);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));

		src_bw_pwrs = ppr_get_bw_powers(pprptr_s, WL_TX_BW_80IN8080);
		dest_bw_pwrs = ppr_get_bw_powers(pprptr_d, WL_TX_BW_80IN8080);
		if (src_bw_pwrs && dest_bw_pwrs)
			bcopy((const uint8*)src_bw_pwrs, (uint8*)dest_bw_pwrs,
				sizeof(*src_bw_pwrs));
	}
}

/** Subtract each power from a common value and re-store */
void
ppr_cmn_val_minus(ppr_t *pprptr, int8 val)
{
	uint i;
	int8 *rptr;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	rptr = (int8 *)&pprptr->ppr_bw;
	size = ppr_pwrs_size(pprptr->hdr);

	for (i = 0; i < size; i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr = val - *rptr;
	}
}

/* Subtract a common value from each power and re-store */
void
ppr_ru_minus_cmn_val(ppr_ru_t *ru_pprptr, int8 val)
{
	uint i;
	uint size;
	int8 *rptr;

	if (ru_pprptr == NULL) {
		PPR_ERROR(("%s: ru_pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	size = SIZE_RU_PPR;
	rptr = (int8 *)&ru_pprptr->ru26;

	for (i = 0; i < size; i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr = *rptr - val;
	}
}

/** Subtract a common value from each power and re-store */
void
ppr_minus_cmn_val(ppr_t *pprptr, int8 val)
{
	uint i;
	int8 *rptr;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	rptr = (int8 *)&pprptr->ppr_bw;
	size = ppr_pwrs_size(pprptr->hdr);

	for (i = 0; i < size; i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr = *rptr - val;
	}
}

/** Add a common value to each power and re-store */
void
ppr_plus_cmn_val(ppr_t *pprptr, int8 val)
{
	uint i;
	int8 *rptr;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	rptr = (int8 *)&pprptr->ppr_bw;
	size = ppr_pwrs_size(pprptr->hdr);

	for (i = 0; i < size; i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr += val;
	}
}

/** Multiply by a percentage */
void
ppr_multiply_percentage(ppr_t *pprptr, uint8 val)
{
	uint i;
	int8 *rptr;
	uint size;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	rptr = (int8 *)&pprptr->ppr_bw;
	size = ppr_pwrs_size(pprptr->hdr);

	for (i = 0; i < size; i++, rptr++) {
		if (*rptr != (int8)WL_RATE_DISABLED)
			*rptr = (*rptr * val) / 100;
	}
}

/**
 * Compare two ppr ru variables s1 and s2, save the min value of each
 * contents to variable d
 */
void
ppr_ru_compare_min(ppr_ru_t *d, ppr_ru_t *s1, ppr_ru_t *s2)
{
	uint i;
	uint size = SIZE_RU_PPR;
	int8 *rptrd;
	int8 *rptr1;
	int8 *rptr2;

	if ((d == NULL) || (s1 == NULL) || (s2 == NULL)) {
		PPR_ERROR(("%s: ppr pointer is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	rptrd = (int8 *)&d->ru26;
	rptr1 = (int8 *)&s1->ru26;
	rptr2 = (int8 *)&s2->ru26;

	for (i = 0; i < size; i++, rptr1++, rptr2++, rptrd++) {
		*rptrd = MIN(*rptr1, *rptr2);
	}
}

/* Calculate new power offset (tx_pwr_max - *rptr_target)
 * Updtae to txpwr_offset (MAX(*rptr_offset, (tx_pwr_max - *rptr_target)))
 */
void
ppr_ru_update_txpwr_offset(ppr_ru_t *txpwr_offset, ppr_ru_t *txpwr_target, int8 tx_pwr_max)
{
	uint i;
	uint size = SIZE_RU_PPR;
	int8 *rptr_offset;
	int8 *rptr_target;

	if ((txpwr_offset == NULL) || (txpwr_target == NULL)) {
		PPR_ERROR(("%s: ppr pointer is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	rptr_offset = (int8 *)&txpwr_offset->ru26;
	rptr_target = (int8 *)&txpwr_target->ru26;

	for (i = 0; i < size; i++, rptr_offset++, rptr_target++) {
		if (*rptr_target != (int8)WL_RATE_DISABLED) {
			*rptr_offset = MAX(*rptr_offset, (tx_pwr_max - *rptr_target));
		}
	}
}

/* Compare each power in pprptr with a common value, save the min value of each
 * contents to pprptr
 */
void
ppr_ru_compare_min_cmn_val(ppr_ru_t *pprptr, int8 val)
{
	uint i;
	int8 *rptr;
	uint size = SIZE_RU_PPR;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return;
	}

	rptr = (int8 *)&pprptr->ru26;

	for (i = 0; i < size; i++, rptr++) {
		*rptr = MIN(*rptr, val);
	}
}

/**
 * Compare two ppr variables p1 and p2, save the min value of each
 * contents to variable p1
 */
void
ppr_compare_min(ppr_t* p1, ppr_t* p2)
{
	uint i;
	int8* rptr1 = NULL;
	int8* rptr2 = NULL;
	uint32 pprsize = 0;

	if (ppr_get_cur_bw(p1) == ppr_get_cur_bw(p2)) {
		rptr1 = (int8*)&p1->ppr_bw;
		rptr2 = (int8*)&p2->ppr_bw;
		pprsize = ppr_pwrs_size(p1->hdr);
	}

	for (i = 0; i < pprsize; i++, rptr1++, rptr2++) {
		*rptr1 = MIN(*rptr1, *rptr2);
	}
}

/**
 * Compare two ppr variables p1 and p2, save the max. value of each
 * contents to variable p1
 */
void
ppr_compare_max(ppr_t* p1, ppr_t* p2)
{
	uint i;
	int8* rptr1 = NULL;
	int8* rptr2 = NULL;
	uint32 pprsize = 0;

	if (ppr_get_cur_bw(p1) == ppr_get_cur_bw(p2)) {
		rptr1 = (int8*)&p1->ppr_bw;
		rptr2 = (int8*)&p2->ppr_bw;
		pprsize = ppr_pwrs_size(p1->hdr);
	}

	for (i = 0; i < pprsize; i++, rptr1++, rptr2++) {
		*rptr1 = MAX(*rptr1, *rptr2);
	}
}

/**
 * Serialize the contents of the opaque ppr struct.
 * Writes number of bytes copied, zero on error.
 * Returns error code, BCME_OK if successful.
 */
int
ppr_serialize(const ppr_t *pprptr, uint8 *buf, uint buflen, uint *bytes_copied)
{
	int err = BCME_OK;

	if (pprptr == NULL) {
		PPR_ERROR(("%s: pprptr is NULL\n", __FUNCTION__));
		ASSERT(0);
		return BCME_ERROR;
	}

	if (buflen <= sizeof(ppr_ser_mem_flag_t)) {
		err = BCME_BUFTOOSHORT;
	} else {
		ppr_ser_mem_flag_t *smem_flag = (ppr_ser_mem_flag_t *)buf;
		uint32 flag = NTOH32(smem_flag->flag);

		/* check if memory contains a valid flag, if not, use current
		 * condition (num of chains, txbf etc.) to serialize data.
		 */
		if (NTOH32(smem_flag->magic_word) != PPR_SER_MEM_WORD) {
			flag = ppr_get_flag();
		}

		if (buflen >= ppr_ser_size_by_flag(flag, ppr_get_cur_bw(pprptr))) {
			*bytes_copied = ppr_serialize_data(pprptr, buf, flag);
		} else {
			err = BCME_BUFTOOSHORT;
		}
	}
	return err;
}

/**
 * Deserialize the contents of a buffer into an opaque ppr struct.
 * Creates an opaque structure referenced by *pptrptr, NULL on error.
 * Returns error code, BCME_OK if successful.
 */
int
ppr_deserialize_create(osl_t *osh, const uint8* buf, uint buflen, ppr_t** pprptr,
	uint16 ppr_ver)
{
	const uint8* bptr = buf;
	int err = BCME_OK;
	ppr_t* lpprptr = NULL;

	if ((buflen > SER_HDR_LEN) && (bptr != NULL) && (*bptr == PPR_SERIALIZATION_VER)) {
		const ppr_deser_header_t * ser_head = (const ppr_deser_header_t *)bptr;
		wl_tx_bw_t ch_bw = ser_head->bw;
		/* struct size plus header */
		uint32 ser_size = ppr_pwrs_size((ch_bw)) + SER_HDR_LEN;

		if ((lpprptr = ppr_create(osh, ch_bw)) != NULL) {
			uint32 flags = NTOH32(ser_head->flags);
			uint16 per_band_size = NTOH16(ser_head->per_band_size);
			/* set the data with default value before deserialize */
			ppr_set_cmn_val(lpprptr, WL_RATE_DISABLED);

			ppr_deser_cpy(lpprptr, bptr + sizeof(*ser_head), flags, ch_bw,
				per_band_size, ppr_ver);
		} else if (buflen < ser_size) {
			err = BCME_BUFTOOSHORT;
		} else {
			err = BCME_NOMEM;
		}
	} else if (buflen <= SER_HDR_LEN) {
		err = BCME_BUFTOOSHORT;
	} else if (bptr == NULL) {
		err = BCME_BADARG;
	} else {
		err = BCME_VERSION;
	}
	*pprptr = lpprptr;
	return err;
}

/**
 * Deserialize the contents of a buffer into an opaque ppr struct.
 * Creates an opaque structure referenced by *pptrptr, NULL on error.
 * Returns error code, BCME_OK if successful.
 */
int
ppr_deserialize(ppr_t* pprptr, const uint8* buf, uint buflen, uint16 ppr_ver)
{
	const uint8* bptr = buf;
	int err = BCME_OK;
	ASSERT(pprptr);
	if ((buflen > SER_HDR_LEN) && (bptr != NULL) && (*bptr == PPR_SERIALIZATION_VER)) {
		const ppr_deser_header_t * ser_head = (const ppr_deser_header_t *)bptr;
		wl_tx_bw_t ch_bw = ser_head->bw;

		if (ch_bw == ppr_get_cur_bw(pprptr)) {
			uint32 flags = NTOH32(ser_head->flags);
			uint16 per_band_size = NTOH16(ser_head->per_band_size);
			ppr_set_cmn_val(pprptr, WL_RATE_DISABLED);
			ppr_deser_cpy(pprptr, bptr + sizeof(*ser_head), flags, ch_bw,
				per_band_size, ppr_ver);
		} else {
			err = BCME_BADARG;
		}
	} else if (buflen <= SER_HDR_LEN) {
		err = BCME_BUFTOOSHORT;
	} else if (bptr == NULL) {
		err = BCME_BADARG;
	} else {
		err = BCME_VERSION;
	}
	return err;
}

/* Get transmit channel bandwidths from chanspec */
wl_tx_bw_t ppr_chanspec_bw(chanspec_t chspec)
{
	uint chspec_bw = CHSPEC_BW(chspec);
	wl_tx_bw_t ret = WL_TX_BW_20;
	switch (chspec_bw) {
	case WL_CHANSPEC_BW_2P5:
	case WL_CHANSPEC_BW_5:
	case WL_CHANSPEC_BW_10:
	case WL_CHANSPEC_BW_20:
		ret = WL_TX_BW_20;
		break;
	case WL_CHANSPEC_BW_40:
		ret = WL_TX_BW_40;
		break;
	case WL_CHANSPEC_BW_80:
		ret = WL_TX_BW_80;
		break;
	case WL_CHANSPEC_BW_160:
		ret = WL_TX_BW_160;
		break;
	case WL_CHANSPEC_BW_8080:
		ret = WL_TX_BW_8080;
		break;
	default :
		ASSERT(0);
	}
	return ret;
}

#if defined(WL_EXPORT_CURPOWER) || !defined(BCMDRIVER)

#define DSSS_TLV_LEN (sizeof(ppr_dsss_tlv_t) + sizeof(ppr_dsss_rateset_t))
#define OFDM_TLV_LEN (sizeof(ppr_ofdm_tlv_t) + sizeof(ppr_ofdm_rateset_t))
#define MCS_TLV_LEN (sizeof(ppr_mcs_tlv_t) + sizeof(ppr_vht_mcs_rateset_t))
#define HE_TLV_LEN (sizeof(ppr_mcs_tlv_t) + sizeof(ppr_he_mcs_rateset_t))
#define PPR_TLV_VER_SIZE (sizeof(int8))

/* Fill DSSS TLV data into the given buffer */
static uint32
ppr_to_dssstlv_per_band(ppr_t* pprptr, wl_tx_bw_t bw, uint8 **to_tlv_buf, uint32 tlv_buf_len,
	wl_tx_chains_t max_chain)
{
	uint32 len = 0;
	wl_tx_chains_t chain;

	pprpbw_t* bw_pwrs;
	bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	for (chain = WL_TX_CHAINS_1; chain <= max_chain; chain++) {
		bcm_tlv_t *tlv_hdr = (bcm_tlv_t *)(*to_tlv_buf);
		ppr_dsss_tlv_t *dsss_tlv = (ppr_dsss_tlv_t *)tlv_hdr->data;
		if ((len + DSSS_TLV_LEN + BCM_TLV_HDR_SIZE) <= tlv_buf_len) {
			const int8* powers;
			bool found = FALSE;

			if (bw_pwrs != NULL) {
				powers = ppr_get_dsss_group(bw_pwrs, chain);
				if (powers) {
					memcpy(dsss_tlv->pwr, powers, sizeof(ppr_dsss_rateset_t));
					found = TRUE;
				}
			}

			if (found) {
				tlv_hdr->id = PPR_RGRP_DSSS_ID;
				tlv_hdr->len = DSSS_TLV_LEN;
				dsss_tlv->bw = bw;
				dsss_tlv->chains = (uint8)chain;
				len += (DSSS_TLV_LEN + BCM_TLV_HDR_SIZE);
				*to_tlv_buf += (DSSS_TLV_LEN + BCM_TLV_HDR_SIZE);
			}
		} else {
			break;
		}
	}
	return len;
}

/* Fill OFDM TLV data into the given buffer */
static uint32
ppr_to_ofdmtlv_per_band(ppr_t* pprptr, wl_tx_bw_t bw, uint8 **to_tlv_buf, uint32 tlv_buf_len,
	wl_tx_chains_t max_chain)
{
	uint32 len = 0;
	wl_tx_chains_t chain;
	wl_tx_mode_t mode;
	pprpbw_t* bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	for (chain = WL_TX_CHAINS_1; chain <= max_chain; chain++) {
		for (mode = WL_TX_MODE_NONE; mode < WL_NUM_TX_MODES; mode ++) {
			bcm_tlv_t *tlv_hdr = (bcm_tlv_t *)(*to_tlv_buf);
			ppr_ofdm_tlv_t *ofdm_tlv = (ppr_ofdm_tlv_t *)tlv_hdr->data;
			const int8* powers;
			bool found = FALSE;
			if ((len + OFDM_TLV_LEN + BCM_TLV_HDR_SIZE) <= tlv_buf_len) {
				if (bw_pwrs != NULL) {
					powers = ppr_get_ofdm_group(bw_pwrs, mode, chain);
					if (powers != NULL) {
						memcpy(ofdm_tlv->pwr, powers,
							sizeof(ppr_ofdm_rateset_t));
						found = TRUE;
					}
				}
				if (found) {
					tlv_hdr->id = PPR_RGRP_OFDM_ID;
					tlv_hdr->len = OFDM_TLV_LEN;
					ofdm_tlv->bw = bw;
					ofdm_tlv->chains = (uint8)chain;
					ofdm_tlv->mode = mode;

					len += (OFDM_TLV_LEN + BCM_TLV_HDR_SIZE);
					*to_tlv_buf += (OFDM_TLV_LEN + BCM_TLV_HDR_SIZE);
				}
			} else {
				return len;
			}
		}

	}
	return len;
}

/* Fill MCS TLV data into the given buffer */
static uint32
ppr_to_mcstlv_per_band(ppr_t* pprptr, wl_tx_bw_t bw, uint8 **to_tlv_buf, uint32 tlv_buf_len,
	wl_tx_chains_t max_chain)
{
	uint32 len = 0;
	wl_tx_chains_t chain;
	wl_tx_mode_t mode;
	uint8 nss;
	pprpbw_t* bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	for (chain = WL_TX_CHAINS_1; chain <= max_chain; chain++) {
		for (nss = WL_TX_NSS_1; nss <= chain; nss++) {
			for (mode = WL_TX_MODE_NONE; mode < WL_NUM_TX_MODES; mode ++) {
				bcm_tlv_t *tlv_hdr = (bcm_tlv_t *)(*to_tlv_buf);
				ppr_mcs_tlv_t *mcs_tlv = (ppr_mcs_tlv_t *)tlv_hdr->data;
				if ((len + MCS_TLV_LEN + BCM_TLV_HDR_SIZE) <= tlv_buf_len) {
					const int8* powers;
					bool found = FALSE;
					if (bw_pwrs != NULL) {
						powers = ppr_get_mcs_group(bw_pwrs, nss, mode,
							chain);
						if (powers != NULL) {
							memcpy(mcs_tlv->pwr, powers,
								sizeof(ppr_vht_mcs_rateset_t));
							found = TRUE;
						}
					}

					if (found) {
						tlv_hdr->id = PPR_RGRP_MCS_ID;
						tlv_hdr->len = MCS_TLV_LEN;
						mcs_tlv->bw = bw;
						mcs_tlv->chains = (uint8)chain;
						mcs_tlv->mode = mode;
						mcs_tlv->nss = nss;
						len += (MCS_TLV_LEN + BCM_TLV_HDR_SIZE);
						*to_tlv_buf += (MCS_TLV_LEN + BCM_TLV_HDR_SIZE);
					}
				} else {
					return len;
				}
			}
		}
	}
	return len;
}

/* Fill HE TLV data into the given buffer */
static uint32
ppr_to_hetlv_per_band(ppr_t* pprptr, wl_tx_bw_t bw, uint8 **to_tlv_buf, uint32 tlv_buf_len,
	wl_tx_chains_t max_chain, wl_he_rate_type_t type)
{
	uint32 len = 0;
	wl_tx_chains_t chain;
	wl_tx_mode_t mode;
	uint8 nss;
	pprpbw_t* he_pwrs = ppr_get_bw_powers(pprptr, bw);
	for (chain = WL_TX_CHAINS_1; chain <= max_chain; chain++) {
		for (nss = WL_TX_NSS_1; nss <= chain; nss++) {
			for (mode = WL_TX_MODE_NONE; mode < WL_NUM_TX_MODES; mode ++) {
				bcm_tlv_t *tlv_hdr = (bcm_tlv_t *)(*to_tlv_buf);
				ppr_mcs_tlv_t *he_tlv = (ppr_mcs_tlv_t *)tlv_hdr->data;

				if (mode == WL_TX_MODE_STBC)
					continue;

				if ((len + HE_TLV_LEN + BCM_TLV_HDR_SIZE) <= tlv_buf_len) {
					const int8* powers;
					uint offset = 0, id = 0;
					bool found = FALSE;
					if (he_pwrs != NULL) {
						powers = ppr_get_he_mcs_group(he_pwrs, nss, mode,
							chain);
						if (powers != NULL) {
							switch (type) {
							case WL_HE_RT_SU:
								id = PPR_RGRP_HE_ID;
								offset = 0;
								break;
							case WL_HE_RT_UB:
								id = PPR_RGRP_HE_UB_ID;
								offset = WL_RATESET_SZ_HE_MCS;
								break;
							case WL_HE_RT_LUB:
								id = PPR_RGRP_HE_LUB_ID;
								offset = 2 * WL_RATESET_SZ_HE_MCS;
								break;
							default:
								PPR_ERROR(("%s: Wrong type %d.\n",
									__FUNCTION__, type));
								ASSERT(0);
								return len;
							}
							powers += offset;
							memcpy(he_tlv->pwr, powers,
								sizeof(ppr_he_mcs_rateset_t));
							found = TRUE;
						}
					}

					if (found) {
						tlv_hdr->id = id;
						tlv_hdr->len = HE_TLV_LEN;
						he_tlv->bw = bw;
						he_tlv->chains = (uint8)chain;
						he_tlv->mode = mode;
						he_tlv->nss = nss;
						len += (HE_TLV_LEN + BCM_TLV_HDR_SIZE);
						*to_tlv_buf += (HE_TLV_LEN + BCM_TLV_HDR_SIZE);
					}
				} else {
					PPR_ERROR(("%s: Buffer insufficient. (tlv_buf_len %d)\n",
						__FUNCTION__, tlv_buf_len));
					return len;
				}
			}
		}
	}
	return len;
}

/* Fill HE RU MCS TLV data into the given buffer */
static uint32
ppr_to_he_ru_mcstlv_per_band(ppr_ru_t* ppr_ru_ptr, wl_he_rate_type_t size, uint8 **to_tlv_buf,
	uint32 tlv_buf_len, wl_tx_chains_t max_chain)
{
	uint32 len = 0;
	wl_tx_chains_t chain;
	wl_tx_mode_t mode;
	uint8 nss;

	pprpbw_ru_t* ru_pwrs = ppr_get_ru_powers(ppr_ru_ptr, size);
	for (chain = WL_TX_CHAINS_1; chain <= max_chain; chain++) {
		for (nss = WL_TX_NSS_1; nss <= chain; nss++) {
			for (mode = WL_TX_MODE_NONE; mode < WL_NUM_TX_MODES; mode ++) {
				bcm_tlv_t *tlv_hdr = (bcm_tlv_t *)(*to_tlv_buf);
				ppr_mcs_tlv_t *mcs_tlv = (ppr_mcs_tlv_t *)tlv_hdr->data;
				if (len + HE_TLV_LEN + BCM_TLV_HDR_SIZE <= tlv_buf_len) {
					const int8* powers;
					bool found = FALSE;
					if (ru_pwrs != NULL) {
						powers = ppr_ru_get_mcs_group(ru_pwrs, nss, mode,
							chain);
						if (powers != NULL) {
							memcpy(mcs_tlv->pwr, powers,
								sizeof(ppr_he_mcs_rateset_t));
							found = TRUE;
						}
					}

					if (found) {
						tlv_hdr->id = PPR_RGRP_HE_RU_ID;
						tlv_hdr->len = HE_TLV_LEN;
						mcs_tlv->bw = size;
						mcs_tlv->chains = (uint8)chain;
						mcs_tlv->mode = mode;
						mcs_tlv->nss = nss;
						len += (HE_TLV_LEN + BCM_TLV_HDR_SIZE);
						*to_tlv_buf += (HE_TLV_LEN + BCM_TLV_HDR_SIZE);
					}
				} else {
					return len;
				}
			}
		}
	}
	return len;
}

/* Convert ppr_ru structure to TLV data */
/* No TLV ver is set in the header */
void ppr_ru_convert_to_tlv(ppr_ru_t* ppr_ru_ptr, uint8 *to_tlv_buf, uint32 tlv_buf_len,
	wl_tx_chains_t max_chain)
{
	wl_he_rate_type_t type;

	ASSERT(ppr_ru_ptr && to_tlv_buf);

	if (tlv_buf_len < sizeof(ppr_ru_t)) {
		ASSERT(0);
		return;
	}

	to_tlv_buf[0] = PPR_TLV_VER;
	to_tlv_buf++;

	for (type = WL_HE_RT_RU26; type <= WL_HE_RT_RU996; type++) {
		tlv_buf_len -= ppr_to_he_ru_mcstlv_per_band(ppr_ru_ptr, type, &to_tlv_buf,
			tlv_buf_len, max_chain);
	}
}

/* Convert ppr structure to TLV data */
void ppr_convert_to_tlv(ppr_t* pprptr, wl_tx_bw_t bw, uint8 *to_tlv_buf, uint32 tlv_buf_len,
	wl_tx_chains_t max_chain)
{
	uint32 len = tlv_buf_len;
	wl_tx_bw_t check_bw;
	ASSERT(pprptr && to_tlv_buf);
	to_tlv_buf[0] = PPR_TLV_VER;
	to_tlv_buf++;
	switch (bw) {
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
		len -= ppr_to_dssstlv_per_band(pprptr, bw, &to_tlv_buf, len, max_chain);
		len -= ppr_to_ofdmtlv_per_band(pprptr, bw, &to_tlv_buf, len, max_chain);
		len -= ppr_to_mcstlv_per_band(pprptr, bw, &to_tlv_buf, len, max_chain);
		len -= ppr_to_hetlv_per_band(pprptr, bw, &to_tlv_buf, len, max_chain, WL_HE_RT_SU);
		len -= ppr_to_hetlv_per_band(pprptr, bw, &to_tlv_buf, len, max_chain, WL_HE_RT_UB);
		len -= ppr_to_hetlv_per_band(pprptr, bw, &to_tlv_buf, len, max_chain, WL_HE_RT_LUB);
		break;
	case WL_TX_BW_40:
		len -= ppr_to_dssstlv_per_band(pprptr, WL_TX_BW_20IN40, &to_tlv_buf, len,
			max_chain);
		len -= ppr_to_ofdmtlv_per_band(pprptr, WL_TX_BW_20IN40, &to_tlv_buf, len,
			max_chain);
		len -= ppr_to_mcstlv_per_band(pprptr, WL_TX_BW_20IN40, &to_tlv_buf, len,
			max_chain);
		len -= ppr_to_hetlv_per_band(pprptr, WL_TX_BW_20IN40, &to_tlv_buf, len,
			max_chain, WL_HE_RT_SU);
		len -= ppr_to_hetlv_per_band(pprptr, WL_TX_BW_20IN40, &to_tlv_buf, len,
			max_chain, WL_HE_RT_UB);
		len -= ppr_to_hetlv_per_band(pprptr, WL_TX_BW_20IN40, &to_tlv_buf, len,
			max_chain, WL_HE_RT_LUB);
		len -= ppr_to_ofdmtlv_per_band(pprptr, WL_TX_BW_40, &to_tlv_buf, len, max_chain);
		len -= ppr_to_mcstlv_per_band(pprptr, WL_TX_BW_40, &to_tlv_buf, len, max_chain);
		len -= ppr_to_hetlv_per_band(pprptr, WL_TX_BW_40, &to_tlv_buf, len, max_chain,
			WL_HE_RT_SU);
		len -= ppr_to_hetlv_per_band(pprptr, WL_TX_BW_40, &to_tlv_buf, len, max_chain,
			WL_HE_RT_UB);
		len -= ppr_to_hetlv_per_band(pprptr, WL_TX_BW_40, &to_tlv_buf, len, max_chain,
			WL_HE_RT_LUB);
		break;
	case WL_TX_BW_80:
		for (check_bw = WL_TX_BW_80; check_bw <= WL_TX_BW_40IN80; check_bw++) {
			if (check_bw == WL_TX_BW_20IN40)
				continue;
			if (check_bw == WL_TX_BW_20IN80) {
				len -= ppr_to_dssstlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
					max_chain);
			}
			len -= ppr_to_ofdmtlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain);
			len -= ppr_to_mcstlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain);
			len -= ppr_to_hetlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain, WL_HE_RT_SU);
			len -= ppr_to_hetlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain, WL_HE_RT_UB);
			len -= ppr_to_hetlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain, WL_HE_RT_LUB);
		}
		break;
	case WL_TX_BW_160:
		for (check_bw = WL_TX_BW_160; check_bw <= WL_TX_BW_80IN160; check_bw++) {
			if (check_bw == WL_TX_BW_20IN160) {
				len -= ppr_to_dssstlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
					max_chain);
			}
			len -= ppr_to_ofdmtlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain);
			len -= ppr_to_mcstlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain);
			len -= ppr_to_hetlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain, WL_HE_RT_SU);
			len -= ppr_to_hetlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain, WL_HE_RT_UB);
			len -= ppr_to_hetlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain, WL_HE_RT_LUB);
		}
		break;
	case WL_TX_BW_8080:
		for (check_bw = WL_TX_BW_8080; check_bw <= WL_TX_BW_80IN8080; check_bw++) {
			if (check_bw == WL_TX_BW_20IN8080) {
				len -= ppr_to_dssstlv_per_band(pprptr, WL_TX_BW_8080, &to_tlv_buf,
					len, max_chain);
			}
			len -= ppr_to_ofdmtlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain);
			len -= ppr_to_mcstlv_per_band(pprptr, check_bw, &to_tlv_buf, len,
				max_chain);
		}
		break;
	default:
		break;
	};
}

/* Convert TLV data to ppr structure */
int ppr_convert_from_tlv(ppr_t* pprptr, uint8 *from_tlv_buf, uint32 tlv_buf_len)
{
	uint8 ser_ver = from_tlv_buf[0];
	bcm_tlv_t *elt = (bcm_tlv_t *)(&from_tlv_buf[1]);
	int ret = BCME_OK;
	if (ser_ver != PPR_TLV_VER) {
		ret = BCME_VERSION;
	} else {
		do {
			switch (elt->id) {
			case PPR_RGRP_DSSS_ID:
				{
					ppr_dsss_tlv_t *dsss_tlv = (ppr_dsss_tlv_t *)elt->data;
					ppr_set_dsss(pprptr, dsss_tlv->bw, dsss_tlv->chains,
						(ppr_dsss_rateset_t*)dsss_tlv->pwr);
				}
				break;
			case PPR_RGRP_OFDM_ID:
				{
					ppr_ofdm_tlv_t *ofdm_tlv = (ppr_ofdm_tlv_t *)elt->data;
					ppr_set_ofdm(pprptr, ofdm_tlv->bw, ofdm_tlv->mode,
						ofdm_tlv->chains,
						(ppr_ofdm_rateset_t*)(ofdm_tlv->pwr));
				}
				break;
			case PPR_RGRP_MCS_ID:
				{
					ppr_mcs_tlv_t *mcs_tlv = (ppr_mcs_tlv_t *)elt->data;
					ppr_set_vht_mcs(pprptr, mcs_tlv->bw, mcs_tlv->nss,
						mcs_tlv->mode, mcs_tlv->chains,
						(ppr_vht_mcs_rateset_t*)mcs_tlv->pwr);
				}
				break;
			case PPR_RGRP_HE_ID:
				{
					ppr_mcs_tlv_t *he_tlv = (ppr_mcs_tlv_t *)elt->data;
					ppr_set_he_mcs(pprptr, he_tlv->bw, he_tlv->nss,
						he_tlv->mode, he_tlv->chains,
						(ppr_he_mcs_rateset_t *)he_tlv->pwr, WL_HE_RT_SU);
				}
				break;
			case PPR_RGRP_HE_UB_ID:
				{
					ppr_mcs_tlv_t *he_tlv = (ppr_mcs_tlv_t *)elt->data;
					ppr_set_he_mcs(pprptr, he_tlv->bw, he_tlv->nss,
						he_tlv->mode, he_tlv->chains,
						(ppr_he_mcs_rateset_t *)he_tlv->pwr, WL_HE_RT_UB);
				}
				break;
			case PPR_RGRP_HE_LUB_ID:
				{
					ppr_mcs_tlv_t *he_tlv = (ppr_mcs_tlv_t *)elt->data;
					ppr_set_he_mcs(pprptr, he_tlv->bw, he_tlv->nss,
						he_tlv->mode, he_tlv->chains,
						(ppr_he_mcs_rateset_t *)he_tlv->pwr, WL_HE_RT_LUB);
				}
				break;
			default:
				ASSERT(0);
				break;
			}

		} while ((elt = bcm_next_tlv(elt, (int *)&tlv_buf_len)) != NULL);
	}
	return ret;
}

/* Convert TLV data to ppr_ru structure */
int ppr_ru_convert_from_tlv(ppr_ru_t* ppr_ru_ptr, uint8 *from_tlv_buf, uint32 tlv_buf_len)
{
	uint8 ser_ver = from_tlv_buf[0];
	bcm_tlv_t *elt = (bcm_tlv_t *)(&from_tlv_buf[1]);
	int ret = BCME_OK;
	if (ser_ver != PPR_TLV_VER) {
		ret = BCME_VERSION;
	} else {
		do {
			switch (elt->id) {
			case PPR_RGRP_HE_RU_ID:
				{
					ppr_mcs_tlv_t *mcs_tlv = (ppr_mcs_tlv_t *)elt->data;
					if (elt->len != (sizeof(ppr_he_mcs_rateset_t) +
						sizeof(ppr_mcs_tlv_t))) {
						return BCME_BADLEN;
					}

					if (mcs_tlv->nss > WL_TX_NSS_4 ||
						mcs_tlv->mode >= WL_NUM_TX_MODES ||
						mcs_tlv->chains > PPR_MAX_TX_CHAINS) {
						return BCME_ERROR;
					}

					ppr_set_he_ru_mcs(ppr_ru_ptr, mcs_tlv->bw, mcs_tlv->nss,
						mcs_tlv->mode, mcs_tlv->chains,
						(ppr_he_mcs_rateset_t*)mcs_tlv->pwr);
				}
				break;
			default:
				ASSERT(0);
				break;
			}

		} while ((elt = bcm_next_tlv(elt, (int *)&tlv_buf_len)) != NULL);
	}
	return ret;
}

/* Get the required buffer size for DSSS TLV data */
static uint32 ppr_get_tlv_size_dsss(uint32 max_chain)
{
	uint32 size = 0;
	uint32 chain;
	for (chain = 1; chain <= max_chain; chain++) {
		size += (DSSS_TLV_LEN + BCM_TLV_HDR_SIZE);
	}
	return size;
}

/* Get the required buffer size for MCS/OFDM TLV data */
static uint32 ppr_get_ofdmmcs_size(ppr_t* pprptr, uint32 max_chain, wl_tx_bw_t bw)
{
	uint32 size = 0;
	uint32 chain;
	wl_tx_mode_t mode;
	uint32 nss;
	const int8* powers;
	pprpbw_t* bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	for (chain = 1; chain <= max_chain; chain++) {
		for (mode = WL_TX_MODE_NONE; mode < WL_NUM_TX_MODES; mode ++) {
			if (bw_pwrs != NULL) {
				powers = ppr_get_ofdm_group(bw_pwrs, mode, chain);
				if (powers != NULL) {
					size += (OFDM_TLV_LEN + BCM_TLV_HDR_SIZE);
				}
				for (nss = WL_TX_NSS_1; nss <= chain; nss++) {
					powers = ppr_get_mcs_group(bw_pwrs, nss, mode,
						chain);
					if (powers != NULL) {
						size += (MCS_TLV_LEN + BCM_TLV_HDR_SIZE);
					}
				}
			}
		}
	}
	return size;
}

/* Get the required buffer size for HE TLV data */
static uint32 ppr_get_he_size(ppr_t* pprptr, uint32 max_chain, wl_tx_bw_t bw)
{
	uint32 size = 0;
	uint32 chain;
	wl_tx_mode_t mode;
	uint32 nss;
	const int8* powers;
	pprpbw_t* bw_pwrs = ppr_get_bw_powers(pprptr, bw);
	for (chain = 1; chain <= max_chain; chain++) {
		for (mode = WL_TX_MODE_NONE; mode < WL_NUM_TX_MODES; mode ++) {
			if (mode == WL_TX_MODE_STBC)
				continue;
			if (bw_pwrs != NULL) {
				for (nss = WL_TX_NSS_1; nss <= chain; nss++) {
					powers = ppr_get_he_mcs_group(bw_pwrs, nss, mode,
						chain);
					if (powers != NULL) {
						size += (HE_TLV_LEN + BCM_TLV_HDR_SIZE);
					}
				}
			}
		}
	}
	return size;
}

/* Get the required buffer size for he ru rates */
uint32 ppr_ru_get_size(ppr_ru_t* ppr_ru_ptr, uint32 max_chain)
{
	uint32 size = PPR_TLV_VER_SIZE;
	wl_he_rate_type_t type;
	wl_tx_chains_t chain;
	wl_tx_mode_t mode;
	uint8 nss;
	pprpbw_ru_t* ru_pwrs;
	int8 *pwrs;

	for (type = WL_HE_RT_RU26; type <= WL_HE_RT_RU996; type++) {
		ru_pwrs = ppr_get_ru_powers(ppr_ru_ptr, type);
		if (ru_pwrs != NULL) {
			for (chain = WL_TX_CHAINS_1; (uint)chain <= (uint)max_chain; chain++) {
				for (nss = WL_TX_NSS_1; nss <= chain; nss++) {
					for (mode = WL_TX_MODE_NONE; mode < WL_NUM_TX_MODES;
						mode++) {
						pwrs = ppr_ru_get_mcs_group(ru_pwrs, nss, mode,
							chain);
						if (pwrs != NULL) {
							size += (HE_TLV_LEN + BCM_TLV_HDR_SIZE);
						}
					}
				}
			}
		}
	}

	return size;
}

/* Get the total HE (Type SU, UB, LUB) TLV buffer size
 * for the given ppr data of bandwidth and max chains
 */
uint32 ppr_get_he_tlv_size(ppr_t* pprptr, wl_tx_bw_t bw, uint32 max_chain)
{
	uint32 he_size = ppr_get_he_size(pprptr, max_chain, bw);
	uint32 ret = 0;
	switch (bw) {
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
		ret += 3*he_size;	/* Type SU, UB, LUB */
		break;
	case WL_TX_BW_40:
		/* 20IN40 and 40 */
		ret += 2*3*he_size;
		break;
	case WL_TX_BW_80:
		/* 20IN80, 40IN80,80 */
		ret += 3*3*he_size;
		break;
	case WL_TX_BW_160:
		/* 20IN160, 40IN160, 80IN160, 160 */
		ret += 4*3*he_size;
		break;
	case WL_TX_BW_8080:
		break;
	default:
		ret = 0;
		ASSERT(0);
		break;
	};

	return ret;
}

/* Get the total TLV buffer size for the given ppr data of bandwidth and max chains */
uint32 ppr_get_tlv_size(ppr_t* pprptr, wl_tx_bw_t bw, uint32 max_chain)
{
	uint32 dsss_size = ppr_get_tlv_size_dsss(max_chain);
	uint32 mcs_ofdm_size = ppr_get_ofdmmcs_size(pprptr, max_chain, bw);
	uint32 he_size = ppr_get_he_size(pprptr, max_chain, bw);
	uint32 ret = PPR_TLV_VER_SIZE;
	switch (bw) {
	case WL_TX_BW_2P5:
	case WL_TX_BW_5:
	case WL_TX_BW_10:
	case WL_TX_BW_20:
		ret += (dsss_size + mcs_ofdm_size);
		ret += 3*he_size;	/* Type SU, UB, LUB */
		break;
	case WL_TX_BW_40:
		/* 20IN40 and 40 */
		ret += (dsss_size + 2*mcs_ofdm_size);
		ret += 2*3*he_size;
		break;
	case WL_TX_BW_80:
		/* 20IN80, 40IN80,80 */
		ret += (dsss_size + 3*mcs_ofdm_size);
		ret += 3*3*he_size;
		break;
	case WL_TX_BW_160:
		/* 20IN160, 40IN160, 80IN160, 160 */
		ret += (dsss_size + 4*mcs_ofdm_size);
		ret += 4*3*he_size;
		break;
	case WL_TX_BW_8080:
		/* 20IN8080, 40IN8080, 80IN8080, 8080, 8080CHAN2 */
		ret += (dsss_size + 5*mcs_ofdm_size);
		break;
	default:
		ret = 0;
		ASSERT(0);
		break;
	};

	return ret;
}

/* Get current PPR TLV version */
uint32 ppr_get_tlv_ver(void)
{
	return PPR_TLV_VER;
}

#endif /* WL_EXPORT_CURPOWER || !BCMDRIVER */

#ifdef WLTXPWR_CACHE

#define MAX_TXPWR_CACHE_ENTRIES 2
#define TXPWR_ALL_INVALID	 0xff
#define TXPWR_CACHE_TXPWR_MAX 0x7f             /* WLC_TXPWR_MAX; */
#define TXPWR_CACHE_SARLIMS_MAX ((TXPWR_CACHE_TXPWR_MAX << 24) \
	| (TXPWR_CACHE_TXPWR_MAX << 16) | (TXPWR_CACHE_TXPWR_MAX << 8) \
	| TXPWR_CACHE_TXPWR_MAX)

/** transmit power cache */
struct tx_pwr_cache_entry {
	chanspec_t chanspec;
	ppr_t* cache_pwrs[TXPWR_CACHE_NUM_TYPES];
	uint8 tx_pwr_max[PPR_MAX_TX_CHAINS];
	uint8 tx_pwr_min[PPR_MAX_TX_CHAINS];
	int8 txchain_offsets[PPR_MAX_TX_CHAINS];
	uint8 data_invalid_flags;
	uint8 stf_tx_max_offset;
#ifdef WL_SARLIMIT
	uint32 sar_lims;
#endif // endif
	int8 tx_pwr_max_boardlim[PPR_MAX_TX_CHAINS];
};

/* Work around to pass 4345a0 ROM validation
 * 16 bytes are the size of the padded struct tx_pwr_cache_entry_t in ROM
 */
#ifdef TXPWR_CACHE_IN_ROM
uint8 txpwr_cache[MAX_TXPWR_CACHE_ENTRIES*16] = {0};
#endif // endif

#ifndef WLTXPWR_CACHE_PHY_ONLY
static int stf_tx_pwr_min = TXPWR_CACHE_TXPWR_MAX;
#endif // endif

static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec);
static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_diff_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec);
static void wlc_phy_txpwr_cache_clear_entry(osl_t *osh, tx_pwr_cache_entry_t* entryptr);
static tx_pwr_cache_entry_t* tx_pwr_cache_get(tx_pwr_cache_entry_t* cacheptr, uint32 idx);

static tx_pwr_cache_entry_t*
BCMRAMFN(tx_pwr_cache_get)(tx_pwr_cache_entry_t* tx_pwr_cache, uint32 idx)
{
	return (&tx_pwr_cache[idx]);
}

/** Find a cache entry for the specified chanspec. */
static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec)
{
	uint i;
	tx_pwr_cache_entry_t* entryptr = NULL;

	for (i = 0; i < (MAX_TXPWR_CACHE_ENTRIES); i++) {
		entryptr = tx_pwr_cache_get(cacheptr, i);
		if (entryptr->chanspec == chanspec) {
			return entryptr;
		}
	}
	return NULL;
}

/** Find a cache entry that's NOT for the specified chanspec. */
static tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_get_diff_entry(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec)
{
	uint i;
	tx_pwr_cache_entry_t* entryptr = NULL;

	for (i = 0; i < (MAX_TXPWR_CACHE_ENTRIES) && (entryptr == NULL); i++) {
		if (tx_pwr_cache_get(cacheptr, i)->chanspec != chanspec) {
			entryptr = tx_pwr_cache_get(cacheptr, i);
		}
	}
	return entryptr;
}

/** Clear a specific cache entry. Delete any ppr_t structs and clear the pointers. */
static void wlc_phy_txpwr_cache_clear_entry(osl_t *osh, tx_pwr_cache_entry_t* entryptr)
{
	uint i;

	entryptr->chanspec = 0;

	ASSERT(entryptr != NULL);
	for (i = 0; i < TXPWR_CACHE_NUM_TYPES; i++) {
		if (entryptr->cache_pwrs[i] != NULL) {
			ppr_delete(osh, entryptr->cache_pwrs[i]);
			entryptr->cache_pwrs[i] = NULL;
		}
	}
	/*
	 * Don't bother with max, min and txchain_offsets, as they need to be
	 * initialised when the entry is setup for a new chanspec
	 */
}

/**
 * Get a ppr_t struct of a given type from the cache for the specified chanspec.
 * Don't return the pointer if the cached data is invalid.
 */
bool wlc_phy_get_cached_pwr(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint pwr_type,
	ppr_t* pprptr)
{
	bool ret = FALSE;
	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if ((entryptr != NULL) &&
			((entryptr->data_invalid_flags & (0x01 << pwr_type)) == 0)) {
			ppr_copy_struct(entryptr->cache_pwrs[pwr_type], pprptr);
			ret = TRUE;
		}
	}

	return ret;
}

int wlc_phy_set_cached_pwr(osl_t* osh, tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint pwr_type, ppr_t* pwrptr)
{
	int result = BCME_NOTFOUND;

	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			ppr_copy_struct(pwrptr, entryptr->cache_pwrs[pwr_type]);
			entryptr->data_invalid_flags &= ~(0x01 << pwr_type); /* now valid */
			result = BCME_OK;
		}
	}
	return result;
}

void wlc_phy_txpwr_cache_clear(osl_t* osh, tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec)
{
	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);
	if (entryptr != NULL) {
		entryptr->chanspec = 0;
		entryptr->data_invalid_flags = TXPWR_ALL_INVALID;
	}
}

void wlc_phy_txpwr_cache_close(osl_t* osh, tx_pwr_cache_entry_t* cacheptr)
{
	uint i;

	for (i = 0; i < MAX_TXPWR_CACHE_ENTRIES; i++) {
		tx_pwr_cache_entry_t* entryptr = tx_pwr_cache_get(cacheptr, i);
		wlc_phy_txpwr_cache_clear_entry(osh, entryptr);
	}
	MFREE(osh, cacheptr, (uint)sizeof(*cacheptr) * MAX_TXPWR_CACHE_ENTRIES);
}

tx_pwr_cache_entry_t* wlc_phy_txpwr_cache_create(osl_t* osh)
{
	int i, j;
	tx_pwr_cache_entry_t* cacheptr =
		(tx_pwr_cache_entry_t*)MALLOC(osh,
		(uint)sizeof(tx_pwr_cache_entry_t) * MAX_TXPWR_CACHE_ENTRIES);
	if (cacheptr != NULL) {
		memset(cacheptr, 0, (uint)sizeof(tx_pwr_cache_entry_t) * MAX_TXPWR_CACHE_ENTRIES);
		/* Allocate memory for each entry */
		for (i = 0; i < MAX_TXPWR_CACHE_ENTRIES; i++) {
			tx_pwr_cache_entry_t* entryptr = tx_pwr_cache_get(cacheptr, i);
			entryptr->data_invalid_flags = TXPWR_ALL_INVALID;
			for (j = 0; j < TXPWR_CACHE_NUM_TYPES; j++) {
				entryptr->cache_pwrs[j] = ppr_create(osh, ppr_get_max_bw());
				if (!entryptr->cache_pwrs[j]) {
					wlc_phy_txpwr_cache_close(osh, cacheptr);
					return NULL;
				}
			}
		}
	}
	return cacheptr;
}

/*
 * Get a ppr_t struct of a given type from the cache for the specified chanspec.
 * Return the pointer even if the cached data is invalid.
 */
ppr_t* wlc_phy_get_cached_ppr_ptr(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint pwr_type)
{
	ppr_t* pwrptr = NULL;

	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			pwrptr = entryptr->cache_pwrs[pwr_type];
	}

	return pwrptr;
}

/** Indicate if we have cached a particular ppr_t struct for any chanspec. */
bool wlc_phy_is_pwr_cached(tx_pwr_cache_entry_t* cacheptr, uint pwr_type, ppr_t* pwrptr)
{
	bool result = FALSE;
	uint i;

	if (pwr_type < TXPWR_CACHE_NUM_TYPES) {
		for (i = 0; (i < MAX_TXPWR_CACHE_ENTRIES) && (result == FALSE); i++) {
			if (tx_pwr_cache_get(cacheptr, i)->cache_pwrs[pwr_type] == pwrptr) {
				result = TRUE;
			}
		}
	}
	return result;
}

/* Get the maximum boardlim for the specified core and chanspec. */
int8 wlc_phy_get_cached_boardlim(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core)
{
	uint8 board_lim = WL_RATE_DISABLED;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			board_lim = entryptr->tx_pwr_max_boardlim[core];
	}

	return board_lim;
}

/* Set the maximum boardlim for the specified core and chanspec. */
int wlc_phy_set_cached_boardlim(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core,
	int8 board_lim)
{
	int result = BCME_NOTFOUND;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			entryptr->tx_pwr_max_boardlim[core] = board_lim;
			result = BCME_OK;
		}
	}
	return result;
}

/** Get the maximum power for the specified core and chanspec. */
uint8 wlc_phy_get_cached_pwr_max(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core)
{
	uint8 max_pwr = WL_RATE_DISABLED;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			max_pwr = entryptr->tx_pwr_max[core];
	}

	return max_pwr;
}

/** Set the maximum power for the specified core and chanspec. */
int wlc_phy_set_cached_pwr_max(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core,
	uint8 max_pwr)
{
	int result = BCME_NOTFOUND;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			entryptr->tx_pwr_max[core] = max_pwr;
			result = BCME_OK;
		}
	}
	return result;
}

/** Get the minimum power for the specified core and chanspec. */
uint8 wlc_phy_get_cached_pwr_min(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core)
{
	uint8 min_pwr = WL_RATE_DISABLED;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			min_pwr = entryptr->tx_pwr_min[core];
	}

	return min_pwr;
}

/** Set the minimum power for the specified core and chanspec. */
int wlc_phy_set_cached_pwr_min(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec, uint core,
	uint8 min_pwr)
{
	int result = BCME_NOTFOUND;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			entryptr->tx_pwr_min[core] = min_pwr;
			result = BCME_OK;
		}
	}
	return result;
}

/** Get the txchain offsets for the specified chanspec. */
int8 wlc_phy_get_cached_txchain_offsets(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint core)
{
	uint8 offset = WL_RATE_DISABLED;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL)
			offset = entryptr->txchain_offsets[core];
	}

	return offset;
}

/** Set the txchain offsets for the specified chanspec. */
int wlc_phy_set_cached_txchain_offsets(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint core, int8 offset)
{
	int result = BCME_NOTFOUND;

	if (core < PPR_MAX_TX_CHAINS) {
		tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

		if (entryptr != NULL) {
			entryptr->txchain_offsets[core] = offset;
			result = BCME_OK;
		}
	}
	return result;
}

/** Indicate if we have a cache entry for the specified chanspec. */
bool wlc_phy_txpwr_cache_is_cached(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	bool result = FALSE;

	if (wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec)) {
		result = TRUE;
	}
	return result;
}

/** Find a cache entry that's NOT for the specified chanspec. Return the chanspec. */
chanspec_t wlc_phy_txpwr_cache_find_other_cached_chanspec(tx_pwr_cache_entry_t* cacheptr,
	chanspec_t chanspec)
{
	chanspec_t chan = 0;

	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_diff_entry(cacheptr, chanspec);
	if (entryptr != NULL) {
		chan = entryptr->chanspec;
	}
	return chan;
}

/** Invalidate all cached data. */
void wlc_phy_txpwr_cache_invalidate(tx_pwr_cache_entry_t* cacheptr)
{
	uint j;

	for (j = 0; j < MAX_TXPWR_CACHE_ENTRIES; j++) {
		tx_pwr_cache_entry_t* entryptr = tx_pwr_cache_get(cacheptr, j);
		if (entryptr->chanspec != 0) {
			uint i;
			entryptr->data_invalid_flags = TXPWR_ALL_INVALID;
			for (i = 0; i < PPR_MAX_TX_CHAINS; i++) {
				entryptr->tx_pwr_min[i] = TXPWR_CACHE_TXPWR_MAX;
				entryptr->tx_pwr_max[i] = WL_RATE_DISABLED;
				entryptr->txchain_offsets[i] = WL_RATE_DISABLED;
			}
			entryptr->stf_tx_max_offset = TXPWR_CACHE_TXPWR_MAX;
#ifdef WL_SARLIMIT
			entryptr->sar_lims = TXPWR_CACHE_SARLIMS_MAX;
#endif // endif
		}
	}
}

/** Find an empty cache entry and initialise it. */
int wlc_phy_txpwr_setup_entry(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	int result = BCME_NOTFOUND;

	/* find an empty entry */
	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, 0);
	if (entryptr != NULL) {
		uint i;

		entryptr->chanspec = chanspec;
		for (i = 0; i < PPR_MAX_TX_CHAINS; i++) {
			entryptr->tx_pwr_min[i] = TXPWR_CACHE_TXPWR_MAX; /* WLC_TXPWR_MAX; */
			entryptr->tx_pwr_max[i] = WL_RATE_DISABLED;
			entryptr->txchain_offsets[i] = WL_RATE_DISABLED;
		}
		entryptr->stf_tx_max_offset = TXPWR_CACHE_TXPWR_MAX;
#ifdef WL_SARLIMIT
		entryptr->sar_lims = TXPWR_CACHE_SARLIMS_MAX;
#endif // endif
		result = BCME_OK;
	}
	return result;
}

#ifdef WL_SARLIMIT
void wlc_phy_set_cached_sar_lims(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint32 sar_lims)
{
	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if (entryptr != NULL) {
		entryptr->sar_lims = sar_lims;
	}
}

uint32 wlc_phy_get_cached_sar_lims(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	uint32 sar_lims = TXPWR_CACHE_SARLIMS_MAX;

	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if (entryptr != NULL)
		sar_lims = entryptr->sar_lims;

	return sar_lims;
}
#endif /* WL_SARLIMIT */

#ifndef WLTXPWR_CACHE_PHY_ONLY

/** Get the cached minimum Tx power */
int wlc_phy_get_cached_stf_pwr_min_dbm(tx_pwr_cache_entry_t* cacheptr)
{
	return stf_tx_pwr_min;
}

/** set the cached minimum Tx power */
void wlc_phy_set_cached_stf_pwr_min_dbm(tx_pwr_cache_entry_t* cacheptr, int min_pwr)
{
	stf_tx_pwr_min = min_pwr;
}

void wlc_phy_set_cached_stf_max_offset(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec,
	uint8 max_offset)
{
	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if (entryptr != NULL) {
		entryptr->stf_tx_max_offset = max_offset;
	}
}

uint8 wlc_phy_get_cached_stf_max_offset(tx_pwr_cache_entry_t* cacheptr, chanspec_t chanspec)
{
	uint8 max_offset = TXPWR_CACHE_TXPWR_MAX;

	tx_pwr_cache_entry_t* entryptr = wlc_phy_txpwr_cache_get_entry(cacheptr, chanspec);

	if (entryptr != NULL)
		max_offset = entryptr->stf_tx_max_offset;

	return max_offset;

}
#endif /* WLTXPWR_CACHE_PHY_ONLY */
#endif /* WLTXPWR_CACHE */
