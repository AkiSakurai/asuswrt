/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * beamforming support
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
 * $Id: wlc_txbf.c 781066 2019-11-09 01:30:11Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [MacPhyBeamformingIF]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <osl.h>
#include <typedefs.h>
#include <802.11.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <siutils.h>
#include <wlioctl.h>
#include <wlc_cfg.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <d11.h>
#include <bcmendian.h>
#include <wlc_bsscfg.h>
#include <wlc_rate.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_vht.h>
#include <wlc_ht.h>
#include <wlc_bmac.h>
#include <wlc_txbf.h>
#include <wlc_stf.h>
#include <wlc_txc.h>
#include <wlc_keymgmt.h>
#include <wl_export.h>
#include <wlc_lq.h>
#include <wlc_ap.h>
#include <wlc_dump.h>
#include <phy_chanmgr_api.h>
#ifdef WL_MU_TX
#include <wlc_mutx.h>
#endif // endif
#include <wlc_scb_ratesel.h>
#include <wlc_vasip.h>
#ifdef WLVASIP
#include <d11vasip_code.h>
#endif // endif
#include <wlc_addrmatch.h>
#include <wlc_ratelinkmem.h>
#include <wlc_he.h>
#include <wlc_musched.h>
#include <wlc_macreq.h>
#include <wlc_txcfg.h>
#include <wlc_twt.h>

#ifdef WL_BEAMFORMING

#ifndef NOT_YET
#define NOT_YET 0
#endif // endif

/* iovar table */
enum wlc_txbf_iov {
	IOV_TXBF_BFR_CAP        = 1,
	IOV_TXBF_BFE_CAP        = 2,
	IOV_TXBF_TIMER          = 3,
	IOV_TXBF_TRIGGER        = 4,
	IOV_TXBF_RATESET        = 5,
	IOV_TXBF_MODE           = 6,
	IOV_TXBF_VIRTIF_DISABLE = 7,
	IOV_TXBF_BFE_NRX_OV     = 8,
	IOV_TXBF_IMP            = 9,
	IOV_TXBF_HT_ENABLE      = 10,
	IOV_TXBF_SCHED_TIMER    = 11,
	IOV_TXBF_SCHED_MSG      = 12,
	IOV_TXBF_MAX_RSSI       = 13,
	IOV_TXBF_MIN_RSSI       = 14,
	IOV_TXBF_PKT_THRE       = 15,
	IOV_TXBF_BFM_SPEXP      = 16,
	IOV_TXBF_BFR_SPEXP      = 17,
	IOV_TXBF_MUTIMER        = 18,
	IOV_TXBF_CFG            = 19,
	IOV_TXBF_BF_LAST
};

#define SU_BFR		0x1
#define SU_BFE		0x2
#define MU_BFR		0x4
#define MU_BFE		0x8
#define HE_SU_BFE	0x10
#define HE_MU_BFE	0x20
#define HE_CQI_BFE	0x40

typedef struct txbf_scb_info {
	scb_t *scb;
	wlc_txbf_info_t *txbf;
	uint32  vht_cap;
	bool	exp_en;    /* explicit */
	bool    imp_en;    /* implicit */
	uint8	shm_index; /* index for template & SHM blocks */
	int	amt_index;
	uint32  ht_txbf_cap;
	uint8	bfr_capable;
	uint8	bfe_capable;
	uint8	bfe_sts_cap; /* BFE STS Capability */
	bool    init_pending;
	uint16  imp_used;  /* implicit stats indiciated imp used */
	uint16  imp_bad;   /* implicit stats indicating bad */
	int	time_counter;	/* served/waiting time */
	int	serve_counter;	/* accumulated served time */
	uint32	last_tx_pkts;	/* tx pkt number at last schedule */
	bool	no_traffic;	/* no traffic since last schedule */

	uint32	flags;
	uint8	mx_bfiblk_idx;
	uint8	bfe_nrx; /* if HE, this is for bw80 */
	uint8	bfe_nrx_bw160;
	uint8	bfe_nrx_bw80p80;
	uint16	BFIConfig0; /* rev ge128 Linkmem info */
	uint16	BFIConfig1;
	uint16	BFRStat0;
	uint16	BFRStat1;
	uint8	bfe_sts_cap_bw160;
	he_phy_cap_t he_cap;
} txbf_scb_info_t;

typedef struct txbf_scb_cubby {
	txbf_scb_info_t *txbf_scb_info;
} txbf_scb_cubby_t;

#define  TXBFCFG_MAX_ENTRIES		19
#define  TXBFCFG_AUTOTXV_ADD		0
#define  TXBFCFG_AUTOTXV_DEL		1
#define  TXBFCFG_MUSND_PER		2
#define  TXBFCFG_STXV_MAXCNT		18

#define  TXBFCFG_HOSTFLAG_ENTRIES	3
#define  TXBFCFG_MAX_STRLEN		20

#if defined(BCMDBG)
static char txbfcfg_str_array[TXBFCFG_MAX_ENTRIES][TXBFCFG_MAX_STRLEN] = {
	"mtxv_autoadd",		/* host flag */
	"mtxv_autodel",		/* host flag */
	"dyn_musnd",		/* host flag */
	"musnd_timer",		/*  0  */
	"vht_sndage_thresh",	/*  1  */
	"hemm_sndage_thresh",	/*  2  */
	"hests_sndage_thresh",	/*  3  */
	"hesus_sndage_thresh",	/*  4  */
	"cqi_sndage_thresh",	/*  5  */
	"mtxvnew_thresh",	/*  6  */
	"musnd2grp_dis",	/*  7  */
	"mtxvidle_thresh",	/*  8  */
	"musnd_swthresh",	/*  9  */
	"vhttxvtmout_thresh",	/* 10  */
	"hemutmout_thresh",	/* 11  */
	"hesttmout_thresh",	/* 12  */
	"hesutmout_thresh",	/* 13  */
	"cqitmout_thresh",	/* 14  */
	"sutxvcnt_max",
};
#endif // endif

static uint16 txbfcfg_def_vals[TXBFCFG_MAX_ENTRIES] = {
	1, 1, 1,			/* host flags */
	0xc8,				/* musnd_timer */
	1, 1, 2, 4, 16,			/* thresholds for vht, hemm, hests, hesus, cqi */
	0,				/* ntxvnew */
	0,				/* musnd2grp_disable */
	20, 2,				/* thresholds for mtxv mtxvidle, musnd_sw */
	0, 0, 0, 0, 0,			/* timeout thresholds for vht, hemm, hests, hesus, cqi */
	16,				/* ntxv allocated for SU */
};

typedef struct txbf_cfg_tuple {
	uint16 val;
	uint16 offset;
} txbf_cfg_tuple_t;

typedef struct txbf_cfg_blk {
	txbf_cfg_tuple_t txbf_cfg[TXBFCFG_MAX_ENTRIES];
} txbf_cfg_blk_t;

struct wlc_txbf_info {
	wlc_info_t		*wlc;		/* pointer to main wlc structure */
	wlc_pub_t		*pub;		/* public common code handler */
	osl_t			*osh;		/* OSL handler */
	int	scb_handle; 	/* scb cubby handle to retrieve data from scb */
	uint8 bfr_capable;
	uint8 bfe_capable;
	uint8 mode; 	/* wl txbf 0: txbf is off
			 * wl txbf 1: txbf on/off depends on CLM and expected TXBF system gain
			 * wl txbf 2: txbf is forced on for all rates
			 */
	uint8 active;
	uint16 shm_idx_bmp;
	uint16	shm_base;
	uint16	sounding_period;
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint8	txbf_rate_mcs_bcm[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint16	txbf_rate_vht_bcm[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];	/* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_bcm[TXBF_RATE_OFDM_ALL]; /* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_cnt;
	uint8	txbf_rate_ofdm_cnt_bcm;
	uint32	max_link;
	uint8	applied2ovr; /* Status of TxBF on/off for override rspec if mode is 1 */
	bool virtif_disable; /* Disable Beamforming on non primary interfaces like P2P,TDLS */
	uint8  bfe_nrx_ov; /* number of bfe rx antenna override */
	uint8	imp; 	/* wl txbf_imp 0: Implicit txbf is off
			 * wl txbf_imp 1: Implicit txbf on/off depends on CLM and
			 * expected TXBF system gain
			 * wl txbf_imp 2: Impilict txbf is forced on all single stream rates
			 */
	uint32 flags;
	int8 max_txpwr_limit;
	bool    imp_nochk;  /* enable/disable detect bad implicit txbf results */
	uint32  max_link_ext;
	struct wl_timer *sched_timer;   /* timer for scheduling when number of active link > 7 */
	int sched_timer_interval;
	int sched_timer_enb;
	int sched_timer_added;
	int sched_msg;
	int max_rssi;
	int min_rssi;
	uint32  pkt_thre_sec;   /* threshold of pkt number per second for pkt based filter */
	uint32  pkt_thre_sched; /* threshold of pkt number per schedule cycle */
	scb_t *su_scbs[TXBF_MAX_LINK_EXT + 1];
	uint8 mu_max_links;
	uint16 mx_bfiblk_idx_bmp;
	uint16 mx_bfiblk_base;
	uint16 mu_sounding_period;
	scb_t *mu_scbs[TXBF_MU_MAX_LINKS];

#define IMPBF_REV_LT64_USR_IDX	6
#define IMPBF_REV_GE64_USR_IDX	7
	int8 impbf_usr_idx;
	bool ht_enable; /* enable 11N Exp Beamforming */
	/* special mode of bfm-based freq-domain spatial expansion */
	uint8 bfm_spexp;
	uint8 bfr_spexp;
	uint8 su_bfi_stidx; /* start index of SU SHM BFI links */
	txbf_cfg_blk_t *txbfcfg_blk_ptr;
};

#define TXBF_SCB_CUBBY(txbf, scb) (txbf_scb_cubby_t *)SCB_CUBBY(scb, (txbf->scb_handle))
#define TXBF_SCB_INFO(txbf, scb) (TXBF_SCB_CUBBY(txbf, scb))->txbf_scb_info

static const bcm_iovar_t txbf_iovars[] = {
	{"txbf", IOV_TXBF_MODE,
	(0), 0, IOVT_UINT8, 0
	},
	{"txbf_bfr_cap", IOV_TXBF_BFR_CAP,
	(IOVF_SET_DOWN), 0, IOVT_UINT8, 0
	},
	{"txbf_bfe_cap", IOV_TXBF_BFE_CAP,
	(IOVF_SET_DOWN), 0, IOVT_UINT8, 0
	},
	{"txbf_timer", IOV_TXBF_TIMER,
	(IOVF_SET_UP), 0, IOVT_INT32, 0
	},
	{"txbf_rateset", IOV_TXBF_RATESET,
	(0), 0, IOVT_BUFFER, sizeof(wl_txbf_rateset_t)
	},
	{"txbf_virtif_disable", IOV_TXBF_VIRTIF_DISABLE,
	(IOVF_SET_DOWN), 0, IOVT_BOOL, 0
	},
#if defined(WLTEST) || defined(WLPKTENG)
	{"txbf_bfe_nrx_ov", IOV_TXBF_BFE_NRX_OV,
	(0), 0, IOVT_INT32, 0
	},
#endif // endif
	{"txbf_imp", IOV_TXBF_IMP,
	(0), 0, IOVT_UINT8, 0
	},
	{"txbf_ht_enable", IOV_TXBF_HT_ENABLE,
	(IOVF_SET_DOWN), 0, IOVT_BOOL, 0
	},
#ifdef TXBF_MORE_LINKS
	{"txbf_sched_timer", IOV_TXBF_SCHED_TIMER,
	(0), 0, IOVT_UINT32, 0
	},
	{"txbf_sched_msg", IOV_TXBF_SCHED_MSG,
	(0), 0, IOVT_UINT32, 0
	},
	{"txbf_max_rssi", IOV_TXBF_MAX_RSSI,
	(0), 0, IOVT_INT32, 0
	},
	{"txbf_min_rssi", IOV_TXBF_MIN_RSSI,
	(0), 0, IOVT_INT32, 0
	},
	{"txbf_pkt_thre", IOV_TXBF_PKT_THRE,
	(0), 0, IOVT_UINT32, 0
	},
#endif	/* TXBF_MORE_LINKS */
	{"txbf_mutimer", IOV_TXBF_MUTIMER,
	(IOVF_SET_UP), 0, IOVT_INT32, 0
	},
	{"txbfcfg", IOV_TXBF_CFG,
	(IOVF_SET_DOWN), 0, IOVT_BUFFER, 0
	},
	{NULL, 0, 0, 0, 0, 0}
};

#define BF_SOUND_PERIOD_DFT	 (100 * 1000/4)	/* 100 ms, in 4us unit */
#define BF_SOUND_PERIOD_DISABLED 0xffff
#define BF_SOUND_PERIOD_MIN	 5	/* 5ms */
#define BF_SOUND_PERIOD_MAX	 128	/* 128ms */

#define BF_NDPA_TYPE_CWRTS	0x1d
#define BF_NDPA_TYPE_VHT	0x15

#define TXBF_BFE_MIMOCTL_VHT	0x8400
#define TXBF_BFE_MIMOCTL_HT	0x780
typedef enum {
	C_BFEMMCTL_VHT_NC_NBIT = 0,
	C_BFEMMCTL_VHT_NR_NBIT = 3,
	C_BFEMMCTL_VHT_BW_NBIT = 6,
	C_BFEMMCTL_VHT_TG_NBIT = 8,
	C_BFEMMCTL_VHT_CB_NBIT = 10,
	C_BFEMMCTL_VHT_FB_NBIT = 11, // feedback type(0: SU, 1: MU)
} eBfeMimoctlVhtBitDefinitions;

typedef enum {
	C_BFEMMCTL_HT_NC_NBIT = 0,
	C_BFEMMCTL_HT_NR_NBIT = 2,
	C_BFEMMCTL_HT_BW_NBIT = 4,
	C_BFEMMCTL_HT_TG_NBIT = 5
} eBfeMimoctlHtBitDefinitions;

// HE capabilities, first 10 bits are mimoctl content
typedef enum {
	C_BFECAPHE_NC_NBIT    = 0,
	C_BFECAPHE_NR_NBIT    = 3,
	C_BFECAPHE_BW_NBIT    = 6,
	C_BFECAPHE_TG_NBIT    = 8,
	C_BFECAPHE_CB_NBIT    = 9,
	C_BFECAPHE_CQIEN_NBIT = 13,
	C_BFECAPHE_MUEN_NBIT  = 14,
	C_BFECAPHE_SUEN_NBIT  = 15
} eBfeCapHeBitDefinitions;

#define C_BFECAP_HE	0x021b // default

// VHT capabilities, first 11 bits are mimoctl content
typedef enum {
	C_BFECAPVHT_NC_NBIT   = 0,
	C_BFECAPVHT_NR_NBIT   = 3,
	C_BFECAPVHT_BW_NBIT   = 6,
	C_BFECAPVHT_TG_NBIT   = 8,
	C_BFECAPVHT_CB_NBIT   = 10,
	C_BFECAPVHT_MUEN_NBIT = 14,
	C_BFECAPVHT_SUEN_NBIT = 15
} eBfeCapVhtBitDefinitions;

#define C_BFECAP_VHT	0x041b // default

// HT capabilities, first 11 bits are mimoctl content
typedef enum {
	C_BFECAPHT_NC_NBIT = 0,
	C_BFECAPHT_NR_NBIT = 2,
	C_BFECAPHT_BW_NBIT = 4,
	C_BFECAPHT_TG_NBIT = 5,
	C_BFECAPHT_CF_NBIT = 7,
	C_BFECAPHT_CB_NBIT = 9,
	C_BFECAPHT_EN_NBIT = 15
} eBfeCapHtBitDefinitions;

#define C_BFECAP_HT	0x000f

// C_LNK_BFICONFIG0_POS
typedef enum
{
	C_LNK_STY_NBIT         = 0,   // sounding type 0:ht, 1:vht, 2:he
	C_LNK_STY_LB           = 1,   // last bit
	C_LNK_BFRNC_NBIT       = 2,    // Nc- min:#oBFE RX, #mBFR STS- used in NDPA STA info
	C_LNK_BFRNC_LB         = 4,    // last bit
	C_LNK_BFRNDPS_NBIT     = 5,    // ndp NSS = min:#oBFR STS, #mBFR STS
	C_LNK_BFRNDPS_LB       = 7,    // last bit
	C_LNK_CQICAP_NBIT      = 8,    // CQI Cap
	C_LNK_MUCAP_NBIT       = 9,    // MU Cap
	C_LNK_TBCAP_NBIT       = 10,    // Trigger Based Cap
	C_LNK_PBWCAP_NBIT      = 11,    // Partial BW Cap
	C_LNK_MUNG_NBIT        = 12,   // MU Ng bfe capability
	C_LNK_DISAMB_NBIT      = 13,   // DisAmb bit, set to 1
	C_LNK_MUCB_NBIT        = 14,   // MU Cb bfe capability
	C_LNK_BFIVLD_NBIT      = 15,   // valid indication
} eLnkBfiCfg0BitsDefinitions;
#define C_LNK_STY_BMSK		0x0003
#define C_LNK_BFRNDPS_BSZ	(C_LNK_BFRNDPS_LB - C_LNK_BFRNDPS_NBIT + 1)
#define C_LNK_BFRNC_BMSK	0x1c
#define C_LNK_BFRNDPS_BMSK	0xe0

// C_LNK_BFICONFIG1_POS
typedef enum
{
	C_LNK_BFENR_NBIT       = 0,    // bfe Nr = min:bfe_sts, bfr_sts
	C_LNK_BFENR_LB         = 2,    // last bit
	C_LNK_MUCIDX_NBIT      = 9,    // muclient index	TODO: to remove!!!!
	C_LNK_MUCIDX_LB        = 12,   // last bit
	C_LNK_TXVLOCK_NBIT     = 15,   // Lock TXV alloc, place holder
} eLnkBfiCfg1BitsDefinitions;

// C_LNK_BFRSTAT0_POS
typedef enum
{
	// keep CM and SB together
	C_LNK_BFRCMASK_NBIT      = 0,	// Ucode stores BFM coremask
	C_LNK_BFRCMASK_LB        = 3,	// last bit
	C_LNK_BFRSB_NBIT         = 4,	// Subband, maintained in phyreg bfrconfig0
	C_LNK_BFRSB_LB           = 6,	// last bit
	C_LNK_BFRSU_NBIT         = 7,	// BFR is SU, either failed sounding or no VASIP group
	C_LNK_BFRLBW_NBIT        = 8,	// last_bw. Init'd and maintained by ucode.
	C_LNK_BFRLBW_LB          = 9,	// last bit
	C_LNK_BFRLPBW_NBIT       = 10,	// last phy bw
	C_LNK_BFRLPBW_LB         = 11,	// last bit
	//TODO:...
	C_LNK_FBTY_NBIT	         = 12,  // Feedback type in last ndpa 0:su, 1:mu 2:cqi
	C_LNK_FBTY_LB            = 13,  // last bit
	C_LNK_BFRSTY_NBIT        = 14,  // last sounding type completed, 0:HT 1:VHT 2:HE
	C_LNK_BFRSTY_LB          = 15,  // last bit
} eLnkBfiStat0BitsDefinitions;

// BFI block definitions
typedef enum
{
	C_BFI_IDX_POS  = 0,
	C_BFI_INFO_POS = 1,
	C_BFI_BLK_SZ = 2
} eBfiEntryDefinitions;

#define C_BFI_MTXV_POS C_BFI_INFO_POS
// definition for C_BFI_IDX_POS
typedef enum
{
	C_BFI_TXVIDX_NBIT       = 0,   // txv index, 0-127
	C_BFI_TXVIDX_LB         = 6,   // 7 bits
	C_BFI_TXVRDY_NBIT       = 7,   // sounding report ready
	C_BFI_STXVUSE_NBIT      = 8,   // txv idx is assigned by psm_r for SU
	C_BFI_CQIRDY_NBIT       = 9,   // cqi report ready
	// gap
	C_BFI_NDPATXCNT_NBIT    = 12,  // NDPA txcnt
	C_BFI_NDPATXCNT_LB      = 15,  // last bit
} eBfIIdxBitDefitions;

// for M_STXVM_BLK
typedef enum
{
	C_TXV_BFIIDX_NBIT       = 0,  // bfiidx, 0-255
	C_TXV_BFIIDX_LB         = 7,  // 8 bits
	C_TXV_EXPCNT_NBIT       = 8,  // expiry count for SU
	C_TXV_EXPCNT_LB         = 10, // 3 bits
} eTxvInfoDefinitions;

// definition for word-0/1 in txvm[.]
typedef enum
{
	// word-0
	C_TXVM0_BFIDX_NBIT	= 0,  // bfi idx
	C_TXVM0_BFIDX_LB	= 7,  // last bit
	C_TXVM0_FGIDX_NBIT	= 8,  // fifo group idx
	C_TXVM0_FGIDX_LB	= 11, // last bit
	C_TXVM0_BLROW_NBIT	= 12, // bflist entry/row idex: 0-31
	C_TXVM0_BLROW_LB	= 15, // last
	// word-1
	C_TXVM1_NDPPWR_NBIT	= 0,  // ndp txpwr
	C_TXVM1_NDPPWR_LB	= 7,  // ndp txpwr
	C_TXVM1_RPTRDY_NBIT	= 8,  // rpt ready
	C_TXVM1_SNDTMR_NBIT	= 9,  // sndtmr
	C_TXVM1_SNDTMR_LB	= 15, // last bit
	// word-2
	C_TXVM2_IDLE_NBIT	= 0,  // idle cnt
	C_TXVM2_IDLE_LB		= 7,  // last
} eMxTxvmDefintions;
#define TXBF_RPT_TYPE_COMPRESSED		0x2
#define TXBF_BFR_CONFIG0_BFR_START		0x1
#define TXBF_BFR_CONFIG0_FB_RPT_TYPE_SHIFT	1
#define TXBF_BFR_CONFIG0_FRAME_TYPE_SHIFT	3

#define TXBF_BFR_CONFIG0	(TXBF_BFR_CONFIG0_BFR_START | \
		(TXBF_RPT_TYPE_COMPRESSED << TXBF_BFR_CONFIG0_FB_RPT_TYPE_SHIFT))

#define TXBF_SCHED_TIMER_INTERVAL  1000

#define TXBF_MAX_RSSI  -60
#define TXBF_MIN_RSSI  -85

#define    TXBF_PKT_THRE_SEC   1

#define    TXBF_SCHED_MSG_SCB  1
#define    TXBF_SCHED_MSG_SWAP 2

#define BFI_BLK_SIZE(_rev)	((D11REV_GE(_rev, 128)) ? (2) : (16))

static int wlc_txbf_get_amt(wlc_info_t *wlc, scb_t* scb, int *amt_idx);
static void
wlc_txbf_bfr_init_ge128(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi);

static int
wlc_txbf_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);

static int wlc_txbf_up(void *context);
static int wlc_txbf_down(void *context);
static bool wlc_txbf_check_ofdm_rate(uint8 rate, uint8 *supported_rates, uint8 num_of_rates);

static uint8 wlc_txbf_set_shm_idx(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi);
static void wlc_txbf_bfr_chanspec_upd(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi);
static void wlc_txbf_bfr_init(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi);
static void wlc_txbf_bfe_init(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi);
static void wlc_txbf_bfe_init_rev128(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi);
#if defined(BCMDBG) || defined(WLTEST) || defined(DUMP_TXBF)
static void wlc_txbf_dump_stats(wlc_txbf_info_t *txbf, bcmstrbuf_t *b);
static int wlc_txbf_dump(void *ctx, bcmstrbuf_t *b);
static int wlc_txbf_dump_clr(void *ctx);
static void wlc_txbf_shm_dump(wlc_txbf_info_t *txbf, bcmstrbuf_t *b);
static void wlc_txbf_info_dump(wlc_txbf_info_t *txbf, bcmstrbuf_t *b);
static void wlc_txbf_dump_shm_bfi_blk(wlc_txbf_info_t *txbf, bcmstrbuf_t *b, uint8 idx);
static int wlc_txbf_dump_parse_args(wlc_info_t *wlc, uint8 bmp[], bool *dump_all, uint8* flags);
static void wlc_txbf_shmx_dump(wlc_txbf_info_t *txbf, bcmstrbuf_t *b);
static void wlc_txbf_dump_shmx_bfi_blk(wlc_txbf_info_t *txbf, bcmstrbuf_t *b, uint8 idx);
#else
#define wlc_txbf_shmx_dump(a, b) do {} while (0)
#define wlc_txbf_dump_shmx_bfi_blk(a, b, c) do {} while (0)
#endif // endif

static uint scb_txbf_secsz(void *context, scb_t *scb);
static int scb_txbf_init(void *context, scb_t *scb);
static void scb_txbf_deinit(void *context, scb_t *scb);
#ifdef WLRSDB
static int scb_txbf_update(void *context, scb_t *scb, wlc_bsscfg_t* new_cfg);
#endif /* WLRSDB */
static uint8 wlc_txbf_system_gain_acphy(uint8 bfr_ntx, uint8 bfe_nrx, uint8 std, uint8 mcs,
	uint8 nss, uint8 rate, uint8 bw, bool is_ldpc, bool is_imp);
static void wlc_txbf_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data);
static bool wlc_txbf_init_imp(wlc_txbf_info_t *txbf, scb_t *scb, txbf_scb_info_t *bfi);
static void wlc_txbf_impbf_updall(wlc_txbf_info_t *txbf);
static void  wlc_txbf_rateset_upd(wlc_txbf_info_t *txbf);

static int wlc_txbf_init_link_serve(wlc_txbf_info_t *txbf, scb_t *scb);
static void wlc_txbf_delete_link_serve(wlc_txbf_info_t *txbf, scb_t *scb);
static bool wlc_txbf_invalidate_bfridx(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi, uint16 idx);
static bool wlc_disable_bfe_for_smth(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg);
#ifdef TXBF_MORE_LINKS
static void wlc_txbf_sched_timer(void *arg);
static void wlc_txbf_schedule(wlc_txbf_info_t *txbf);
static int wlc_txbf_delete_link_ext(wlc_txbf_info_t *txbf, scb_t *scb);
static int wlc_txbf_init_link_ext(wlc_txbf_info_t *txbf, scb_t *scb);
#endif // endif

#ifdef WL_PSMX
static void wlc_txbf_bfridx_set_en_bit(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi, bool set);
static uint8 wlc_txbf_set_mx_bfiblk_idx(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi);
static bool wlc_txbf_mu_link_qualify(wlc_txbf_info_t *txbf, scb_t *scb);
static bool wlc_txbf_hemu_sounding_link_qualify(wlc_txbf_info_t *txbf, scb_t *scb);
static void wlc_txbf_mubfi_update(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi, bool set);
static void wlc_txbf_txbfcfg_write_shmx(wlc_txbf_info_t *txbf, int index);
#else
#define wlc_txbf_txbfcfg_write_shmx(a, b) do {} while (0)
#define wlc_txbf_hemu_sounding_link_qualify(a, b) FALSE
#define wlc_txbf_mu_link_qualify(a, b) FALSE
#endif // endif

static void wlc_txbf_init_exp_vht_ht_cap(wlc_txbf_info_t *txbf, scb_t *scb, txbf_scb_info_t *bfi);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

#ifdef WLRSDB
static int
scb_txbf_update(void *context, scb_t *scb, wlc_bsscfg_t* new_cfg)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	wlc_info_t *new_wlc = new_cfg->wlc;
	txbf_scb_cubby_t *txbf_scb_cubby = (txbf_scb_cubby_t *)TXBF_SCB_CUBBY(txbf, scb);
	txbf_scb_info_t *txbf_scb_info = txbf_scb_cubby->txbf_scb_info;
	txbf_scb_info->txbf = new_wlc->txbf;
	return TRUE;
}
#endif /* WLRSDB */

static uint
scb_txbf_secsz(void *context, scb_t *scb)
{
	/* also needed for internal SCB */
	return sizeof(txbf_scb_info_t);
}

static int
scb_txbf_init(void *context, scb_t *scb)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	txbf_scb_cubby_t *txbf_scb_cubby = (txbf_scb_cubby_t *)TXBF_SCB_CUBBY(txbf, scb);
	txbf_scb_info_t *bfi;

	ASSERT(txbf_scb_cubby);

	bfi = wlc_scb_sec_cubby_alloc(txbf->wlc, scb, scb_txbf_secsz(context, scb));
	ASSERT(bfi);

	bfi->txbf = txbf;
	bfi->scb = scb;
	txbf_scb_cubby->txbf_scb_info = bfi;
	bfi->init_pending = TRUE;

	if (!SCB_INTERNAL(scb)) {
		wlc_txbf_init_link(txbf, scb);
	}

	return BCME_OK;
}

static void
scb_txbf_deinit(void *context, scb_t *scb)
{

	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	txbf_scb_cubby_t *txbf_scb_cubby;
	txbf_scb_info_t *bfi;

	ASSERT(txbf);
	ASSERT(scb);

	txbf_scb_cubby = (txbf_scb_cubby_t *)TXBF_SCB_CUBBY(txbf, scb);
	if (!txbf_scb_cubby) {
		ASSERT(txbf_scb_cubby);
		return;
	}

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	if (!bfi) {
		return;
	}

	if (!SCB_INTERNAL(scb)) {
		wlc_txbf_delete_link(txbf, scb);
	}

	wlc_scb_sec_cubby_free(txbf->wlc, scb, bfi);
	txbf_scb_cubby->txbf_scb_info = NULL;
	return;
}

#ifdef TXBF_MORE_LINKS

/* find bfr(s) to serve next when link number is greater than maximum serve number
 */
static void
wlc_txbf_schedule(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc = txbf->wlc;
	scb_t *scb, *out_scb = NULL, *in_scb;
	txbf_scb_info_t *bfi;
	uint32 i, shm_idx;
	int in_idx, out_idx, max_cntr, rssi;

	ASSERT(txbf);

	for (i = 0; i <= txbf->max_link_ext; i++) {
		scb = txbf->su_scbs[i];

		if (scb == NULL)
			continue;

		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi) {
			ASSERT(bfi);
			return;
		}

		bfi->time_counter ++;

		if (i <= txbf->max_link)
			bfi->serve_counter ++;

#ifdef WLCNTSCB
		bfi->no_traffic = 1;

		if ((uint32)scb->scb_stats.tx_pkts - (uint32)bfi-> last_tx_pkts >=
			txbf->pkt_thre_sched)
			bfi->no_traffic = 0;

		bfi->last_tx_pkts = scb->scb_stats.tx_pkts;
#else
		bfi->no_traffic = 0;
#endif /* WLCNTSCB */
	}

	/* find a waiting scb to be swapped in
	 * pick the one waiting for the longest time
	 */

	in_idx = -1;
	max_cntr = -1;

	for (i = txbf->max_link + 1; i <= txbf->max_link_ext; i++) {
		scb = txbf->su_scbs[i];

		if (scb == NULL)
			continue;

		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

		rssi = wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb);

		/* scb filters, following scbs will NOT be swapped in
		 * (1) rssi too high or too low
		 * (2) no traffic since last schedule
		 */
		if (rssi > txbf->max_rssi || rssi < txbf->min_rssi || bfi->no_traffic) {
			if (txbf->sched_msg & TXBF_SCHED_MSG_SWAP) {
				WL_PRINT(("filterd, [%d] %d %d\n", i, rssi, bfi->no_traffic));
			}
			continue;
		}

		if (bfi->time_counter > max_cntr) {
			max_cntr = bfi->time_counter;
			in_idx = i;
		}
	}

	if (txbf->sched_msg & TXBF_SCHED_MSG_SWAP) {
		WL_PRINT(("-> in %d\n", in_idx));
	}

	/* no scb to be swapped in */
	if (in_idx == -1)
		return;

	out_idx = -1;

	/* check if there is empty shm just released by delete link */
	for (shm_idx = 0; shm_idx <= txbf->max_link; shm_idx++) {
		if ((isclr(&txbf->shm_idx_bmp, shm_idx)) &&
			(shm_idx != txbf->impbf_usr_idx)) {
			break;
		}
	}

	/* find a currently served scb to be swapped out
	 * (1) no traffic since last schedule
	 * (2) if no (1), then pick the one served for the longest time
	 */
	if (shm_idx == (txbf->max_link + 1)) {
		max_cntr = -1;

		for (i = 0; i <= txbf->max_link; i++) {
			scb = txbf->su_scbs[i];

			if (scb == NULL)
				continue;

			bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
			if (!bfi) {
				ASSERT(bfi);
				return;
			}

			if (bfi->no_traffic) {
				out_idx = i;
				break;
			}

			if (bfi->time_counter > max_cntr) {
				max_cntr = bfi->time_counter;
				out_idx = i;
			}
		}
	}

	if (txbf->sched_msg & TXBF_SCHED_MSG_SWAP) {
		WL_PRINT(("<- out %d\n", out_idx));
	}

	in_scb = txbf->su_scbs[in_idx];

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, in_scb);
	bfi->time_counter = 0;

	if (out_idx != -1) {
		out_scb = txbf->su_scbs[out_idx];
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, out_scb);
		bfi->time_counter = 0;
		wlc_txbf_delete_link_serve(txbf, out_scb);
	}

	wlc_txbf_delete_link_ext(txbf, in_scb);

	if (out_idx != -1) {
		wlc_txbf_init_link_ext(txbf, out_scb);
	}

	wlc_txbf_init_link_serve(txbf, in_scb);
}

static void
wlc_txbf_sched_timer(void *arg)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)arg;
	wlc_info_t *wlc = txbf->wlc;
	char eabuf[ETHER_ADDR_STR_LEN];
	scb_t *scb;
	txbf_scb_info_t *bfi;
	uint32 i;

	ASSERT(txbf);

	for (i = 0; i < txbf->max_link_ext; i++) {
		scb = txbf->su_scbs[i];

		if (scb == NULL) {
			continue;
		}

		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi) {
			ASSERT(bfi);
			return;
		}

		if (txbf->sched_msg & TXBF_SCHED_MSG_SCB) {
			uint32 tx_rate, tx_pkts;
#ifdef WLCNTSCB
			tx_rate = scb->scb_stats.tx_rate;
			tx_pkts = scb->scb_stats.tx_pkts;
#else
			tx_rate = 0;
			tx_pkts = 0;
#endif // endif
			BCM_REFERENCE(tx_rate);
			BCM_REFERENCE(tx_pkts);

			bcm_ether_ntoa(&scb->ea, eabuf);
			WL_PRINT(("[%2d] %08x %s, %d %d %d, %d %d, %08x %d, %d %d, %d %d\n",
				i, (int)(uintptr)scb, eabuf,

				bfi->exp_en,
				bfi->shm_index,
				bfi->amt_index,

			        wlc_lq_rssi_get(wlc, SCB_BSSCFG(scb), scb),
				bfi->no_traffic,
				tx_rate,
				tx_pkts,

				bfi->bfr_capable,
				bfi->bfe_capable,

				bfi->time_counter,
				bfi->serve_counter));
		}
	}

	wlc_txbf_schedule(txbf);
}

#endif	/* TXBF_MORE_LINKS */

const wl_txbf_rateset_t rs_pre64 = {
				{0xff, 0xff, 0, 0},      /* mcs */
				{0xff, 0xff, 0x7e, 0},   /* Broadcom-to-Broadcom mcs */
				{0x3ff, 0x3ff,    0, 0}, /* vht */
				{0x3ff, 0x3ff, 0x7e, 0}, /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},       /* ofdm */
				{0,0,0,0,0,0,0,0},       /* Broadcom-to-Broadcom ofdm */
				0,                       /* ofdm count */
				0,                       /* Broadcom-to-Broadcom ofdm count */
};

/* txbf rateset for corerev >= 64 */
const wl_txbf_rateset_t rs_ge64[] = {
				{
				{0xff, 0, 0, 0},        /* mcs */
				{0xff, 0xff, 0, 0},     /* Broadcom-to-Broadcom mcs */
				{0x3ff, 0,    0, 0},    /* vht */
				{0xfff, 0xff, 0, 0},    /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},      /* ofdm */
				{0,0,0,0,0,0,0,0},      /* Broadcom-to-Broadcom ofdm */
				0,                      /* ofdm count */
				0                       /* Broadcom-to-Broadcom ofdm count */
				},
				{
				{0xff, 0xff, 0, 0},      /* mcs */
				{0xff, 0xff, 0, 0},      /* Broadcom-to-Broadcom mcs */
				{0x3ff, 0x3ff, 0, 0},    /* vht */
				{0xfff, 0xfff, 0xff, 0}, /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},       /* ofdm */
				{0,0,0,0,0,0,0,0},       /* Broadcom-to-Broadcom ofdm */
				0,                       /* ofdm count */
				0                        /* Broadcom-to-Broadcom ofdm count */
				},
				{
				{0xff, 0xff, 0, 0},          /* mcs */
				{0xff, 0xff, 0, 0},          /* Broadcom-to-Broadcom mcs */
				{0x3ff, 0x3ff, 0xff, 0},     /* vht */
				{0xfff, 0xfff, 0x3ff, 0xff}, /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},           /* ofdm */
				{0,0,0,0,0,0,0,0},           /* Broadcom-to-Broadcom ofdm */
				0,                           /* ofdm count */
				0,                           /* Broadcom-to-Broadcom ofdm count */
				}
};

const wl_txbf_rateset_t rs_128[] = {
				{
				{0, 0, 0, 0},           /* mcs */
				{0, 0, 0, 0},           /* Broadcom-to-Broadcom mcs */
				{0xfff, 0, 0, 0},       /* vht */
				{0xfff, 0, 0, 0},       /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},      /* ofdm */
				{0,0,0,0,0,0,0,0},      /* Broadcom-to-Broadcom ofdm */
				0,                      /* ofdm count */
				0                       /* Broadcom-to-Broadcom ofdm count */
				},
				{
				{0, 0, 0, 0},            /* mcs */
				{0, 0, 0, 0},            /* Broadcom-to-Broadcom mcs */
				{0xfff, 0xfff, 0, 0},    /* vht */
				{0xfff, 0xfff, 0, 0},    /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},       /* ofdm */
				{0,0,0,0,0,0,0,0},       /* Broadcom-to-Broadcom ofdm */
				0,                       /* ofdm count */
				0                        /* Broadcom-to-Broadcom ofdm count */
				},
				{
				{0, 0, 0, 0},                /* mcs */
				{0, 0, 0, 0},                /* Broadcom-to-Broadcom mcs */
				{0xfff, 0xfff, 0x3ff, 0},    /* vht */
				{0xfff, 0xfff, 0x3ff, 0},    /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},           /* ofdm */
				{0,0,0,0,0,0,0,0},           /* Broadcom-to-Broadcom ofdm */
				0,                           /* ofdm count */
				0,                           /* Broadcom-to-Broadcom ofdm count */
				}
};

wlc_txbf_info_t *
BCMATTACHFN(wlc_txbf_attach)(wlc_info_t *wlc)
{
	wlc_txbf_info_t *txbf;
	int err, i;
	scb_cubby_params_t cubby_params;

	if (!(txbf = (wlc_txbf_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_txbf_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		wlc->pub->_txbf = FALSE;
		return NULL;
	}
	txbf->wlc = wlc;
	txbf->pub = wlc->pub;
	txbf->osh = wlc->osh;

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		wlc->pub->_txbf = FALSE;
		return txbf;
	} else {
		wlc->pub->_txbf = TRUE;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, txbf_iovars, "txbf",
		txbf, wlc_txbf_doiovar, NULL, wlc_txbf_up, wlc_txbf_down)) {
		WL_ERROR(("wl%d: txbf wlc_module_register() failed\n", wlc->pub->unit));
		MFREE(wlc->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
		return NULL;
	}
#if defined(BCMDBG) || defined(WLTEST) || defined(DUMP_TXBF)
	wlc_dump_add_fns(wlc->pub, "txbf", wlc_txbf_dump, wlc_txbf_dump_clr, wlc);
#endif // endif

	if (!wlc->pub->_txbf)
		return txbf;

	/* Reserve some space in SCB container */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = txbf;
	cubby_params.fn_init = scb_txbf_init;
	cubby_params.fn_deinit = scb_txbf_deinit;
	cubby_params.fn_secsz = scb_txbf_secsz;
#ifdef WLRSDB
	cubby_params.fn_update = scb_txbf_update;
#endif /* WLRSDB */

	txbf->scb_handle = wlc_scb_cubby_reserve_ext(wlc, sizeof(txbf_scb_cubby_t),
		&cubby_params);

	if (txbf->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve_ext() failed\n", wlc->pub->unit));
		MFREE(wlc->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
		return NULL;
	}

	/* Add client callback to the scb state notification list */
	if ((err = wlc_scb_state_upd_register(wlc, wlc_txbf_scb_state_upd_cb, txbf)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback %p\n",
			wlc->pub->unit, __FUNCTION__,
			OSL_OBFUSCATE_BUF(wlc_txbf_scb_state_upd_cb)));
		return NULL;
	}

	if (D11REV_LT(wlc->pub->corerev, 64)) {
		/* copy mcs rateset */
		bcopy(rs_pre64.txbf_rate_mcs, txbf->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
		bcopy(rs_pre64.txbf_rate_mcs_bcm, txbf->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
		/* copy vht rateset */
		for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
			txbf->txbf_rate_vht[i] = rs_pre64.txbf_rate_vht[i];
			txbf->txbf_rate_vht_bcm[i] = rs_pre64.txbf_rate_vht_bcm[i];
		}
		/* copy ofdm rateset */
		txbf->txbf_rate_ofdm_cnt = rs_pre64.txbf_rate_ofdm_cnt;
		bcopy(rs_pre64.txbf_rate_ofdm, txbf->txbf_rate_ofdm, rs_pre64.txbf_rate_ofdm_cnt);
		txbf->txbf_rate_ofdm_cnt_bcm = rs_pre64.txbf_rate_ofdm_cnt_bcm;
		bcopy(rs_pre64.txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_bcm,
			rs_pre64.txbf_rate_ofdm_cnt_bcm);
	} else {
		wlc_txbf_rateset_upd(txbf);
	}

#ifdef TXBF_MORE_LINKS
	if (!(txbf->sched_timer = wl_init_timer(wlc->wl, wlc_txbf_sched_timer, txbf, "txbf"))) {
		WL_ERROR(("%s: wl_init_timer for txbf timer failed\n", __FUNCTION__));
		return NULL;
	}

	txbf->sched_timer_interval = TXBF_SCHED_TIMER_INTERVAL;
	txbf->sched_timer_enb = 0;
	txbf->sched_timer_added = 0;
	txbf->sched_msg = 0;
#endif	/* TXBF_MORE_LINKS */

	txbf->txbfcfg_blk_ptr = NULL;
	if (D11REV_GT(wlc->pub->corerev, 128)) {
		if ((txbf->txbfcfg_blk_ptr = (txbf_cfg_blk_t *)MALLOCZ(wlc->osh,
			sizeof(txbf_cfg_blk_t)))) {
			txbf_cfg_tuple_t *txbf_cfg;
			/* Initilize txbf_cfg_blk */
			for (i = 0; i < TXBFCFG_MAX_ENTRIES; i++) {
				txbf_cfg = &txbf->txbfcfg_blk_ptr->txbf_cfg[i];
				if (i == TXBFCFG_STXV_MAXCNT) {
					txbf_cfg->offset = S_TXVFREE_BMP;
					txbf_cfg->val = txbfcfg_def_vals[i];
				} else {
					if (i >= TXBFCFG_HOSTFLAG_ENTRIES) {
					txbf_cfg->offset = MX_MUSND_PER(wlc) +
						(i - TXBFCFG_HOSTFLAG_ENTRIES) * 2;
					}
				}
				txbf_cfg->val = txbfcfg_def_vals[i];
			}
		} else {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes for txbfcfg_blk\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		}
	}

	wlc_txbf_init(txbf);
	return txbf;
}

void
BCMATTACHFN(wlc_txbf_detach)(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	if (!txbf)
		return;
	wlc = txbf->wlc;
	ASSERT(wlc);

#ifdef TXBF_MORE_LINKS
	if (txbf->sched_timer) {
		if (txbf->sched_timer_added)
			wl_del_timer(wlc->wl, txbf->sched_timer);
		wl_free_timer(wlc->wl, txbf->sched_timer);
	}
#endif	/* TXBF_MORE_LINKS */

	if (txbf->txbfcfg_blk_ptr) {
		MFREE(txbf->osh, (void *)txbf->txbfcfg_blk_ptr, sizeof(txbf_cfg_blk_t));
		txbf->txbfcfg_blk_ptr = NULL;
	}

	if (txbf->pub->_txbf == TRUE) {
		txbf->pub->_txbf = FALSE;
		wlc_scb_state_upd_unregister(wlc, wlc_txbf_scb_state_upd_cb, txbf);
	}
	wlc_module_unregister(txbf->pub, "txbf", txbf);
	MFREE(txbf->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
	return;
}

static int
wlc_txbf_up(void *context)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	wlc_info_t *wlc = txbf->wlc;
	int txchains = WLC_BITSCNT(wlc->stf->txchain);
	int rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	uint16 coremask0 = 0, coremask1 = 0, mask0;
	uint16 hecap = 0, vhtcap = 0, htcap = 0;
	wlc_pub_t *pub;
	pub = txbf->pub;
	/* Initialize TxBF links during big-hammer */
	if (pub->associated && TXBF_ENAB(pub)) {
		scb_t *scb;
		scb_iter_t scbiter;
		wlc_bsscfg_t *bsscfg;
		int i;

		FOREACH_BSS(wlc, i, bsscfg)
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				wlc_txbf_init_link(wlc->txbf, scb);
			}

	}

	txbf->shm_base = M_BFI_BLK(wlc) >> 1;
	wlc_write_shm(wlc, M_BFI_REFRESH_THR(wlc), txbf->sounding_period);

	if (PSMX_ENAB(pub)) {
		txbf->mx_bfiblk_base = MX_BFI_BLK(wlc) >> 1;
		if (D11REV_GE(pub->corerev, 128)) {
			txbf->shm_base = MX_BFI_BLK(wlc) >> 1;
		}
		wlc_txbf_mutimer_update(txbf, FALSE);
		wlc_txbf_bfi_init(txbf);

		if (txbf->txbfcfg_blk_ptr) {
			int i;
			for (i = 0; i < TXBFCFG_MAX_ENTRIES; i++) {
				wlc_txbf_txbfcfg_write_shmx(txbf, i);
			}
		}
	}
	if (txchains == 1) {
		txbf->active = 0;
		WL_TXBF(("wl%d: %s beamforming deactivated: txchains < 2!\n",
			pub->unit, __FUNCTION__));
	}

	wlc_write_shm(wlc, M_BFI_NRXC(wlc), rxchains - 1);

	/* ucode picks up the coremask from new shmem for all types of frames if bfm is
	 * set in txphyctl
	*/
	/* ?? not final but use this for now, need to find a good choice of two/three out of
	 * # txchains
	*/
	/* M_COREMASK_BFM: byte-0: for implicit, byte-1: for explicit nss-2 */
	/* M_COREMASK_BFM1: byte-0: for explicit nss-3, byte-1: for explicit nss-4 */

#ifdef TXBF_MORE_LINKS
	txbf->sched_timer_enb = 1;
	if ((!txbf->sched_timer_added) && (txbf->sched_timer_interval != 0)) {
		wl_add_timer(wlc->wl, txbf->sched_timer, txbf->sched_timer_interval, TRUE);
		txbf->sched_timer_added = 1;
	}
#endif /* TXBF_MORE_LINKS */

	if (D11REV_LT(pub->corerev, 64) || txbf->bfr_spexp == 0) {
		if (txchains == 2) {
			coremask0 = wlc->stf->txchain << 8;
			coremask1 = (wlc->stf->txchain << 8) | wlc->stf->txchain;
		} else if (txchains == 3) {
			mask0 = 0x8;
			if ((wlc->stf->txchain & mask0) == 0) {
				mask0 = 0x4;
			}
			coremask0 = (wlc->stf->txchain & ~mask0) << 8;
			coremask1 = (wlc->stf->txchain << 8) | wlc->stf->txchain;
		} else if (txchains >= 4) {
			coremask0 = 0x300;
			coremask1 = 0x7;
			coremask1 |= (wlc->stf->txchain << 8);
		}
	} else {
		coremask0 = wlc->stf->txchain << 8;
		coremask1 = (wlc->stf->txchain << 8) | wlc->stf->txchain;
	}

	coremask0 |=  wlc->stf->txchain;
	wlc_write_shm(wlc, M_COREMASK_BFM(wlc), coremask0);
	wlc_write_shm(wlc, M_COREMASK_BFM1(wlc), coremask1);

	if (HE_ENAB_BAND(pub, wlc->band->bandtype)) {
		hecap = C_BFECAP_HE;
		hecap = (rxchains - 1) << C_BFECAPHE_NC_NBIT;
		hecap |= (wlc_he_get_bfe_ndp_recvstreams(wlc->hei) << C_BFECAPHE_NR_NBIT);
		if (CHSPEC_IS40(wlc->chanspec)) {
			hecap |= (0x1 << C_BFECAPHE_BW_NBIT);
		} else if (CHSPEC_IS80(wlc->chanspec)) {
			hecap |= (0x2 << C_BFECAPHE_BW_NBIT);
		} else if (CHSPEC_IS160(wlc->chanspec)) {
			hecap |= (0x3 << C_BFECAPHE_BW_NBIT);
		}
		if (D11REV_GT(pub->corerev, 128)) {
			hecap |= (1 << C_BFECAPHE_CB_NBIT);
		}
		if (txbf->bfe_capable & TXBF_HE_CQI_BFE_CAP) {
			hecap |= (1 << C_BFECAPHE_CQIEN_NBIT);
		}
		if (txbf->bfe_capable & TXBF_HE_MU_BFE_CAP) {
			hecap |= (0x1 << C_BFECAPHE_MUEN_NBIT);
		}
		if (txbf->bfe_capable & TXBF_HE_SU_BFE_CAP) {
			hecap |= (0x1 << C_BFECAPHE_SUEN_NBIT);
		}

	}
	if (VHT_ENAB(pub)) {
		vhtcap = C_BFECAP_VHT;
		vhtcap = (rxchains - 1) << C_BFECAPVHT_NC_NBIT;
		vhtcap |= (wlc_phy_get_bfe_ndp_recvstreams((wlc_phy_t *)WLC_PI(wlc)) <<
			C_BFECAPVHT_NR_NBIT);
		if (CHSPEC_IS40(wlc->chanspec)) {
			vhtcap |= (0x1 << C_BFECAPVHT_BW_NBIT);
		} else if (CHSPEC_IS80(wlc->chanspec)) {
			vhtcap |= (0x2 << C_BFECAPVHT_BW_NBIT);
		} else if (CHSPEC_IS160(wlc->chanspec)) {
			vhtcap |= (0x3 << C_BFECAPVHT_BW_NBIT);
		}
		if (D11REV_GT(pub->corerev, 128)) {
			vhtcap |= (1 << C_BFECAPVHT_CB_NBIT);
		}
		if (txbf->bfe_capable & TXBF_MU_BFE_CAP) {
			vhtcap |= (0x1 << C_BFECAPVHT_MUEN_NBIT);
		}
		if (txbf->bfe_capable & TXBF_SU_BFE_CAP) {
			vhtcap |= (0x1 << C_BFECAPVHT_SUEN_NBIT);
		}
	}

	if (txbf->ht_enable) {
		htcap = C_BFECAP_HT;
		htcap = (rxchains - 1) << C_BFEMMCTL_HT_NC_NBIT;
		htcap |= HT_CAP_TXBF_CAP_C_BFR_ANT_DEF_VAL << C_BFECAPHT_NR_NBIT;

		if (CHSPEC_IS40(wlc->chanspec)) {
			htcap |= (0x1 << C_BFEMMCTL_HT_BW_NBIT);
		}

		if (txbf->bfe_capable & TXBF_SU_BFE_CAP) {
			htcap |= C_BFECAPHT_EN_NBIT;
		}
	}

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		wlc_write_shm(wlc, M_BFECAP_HE(wlc), hecap);
		wlc_write_shm(wlc, M_BFECAP_VHT(wlc), vhtcap);
		wlc_write_shm(wlc, M_BFECAP_HT(wlc), htcap);
	}

	WL_TXBF(("wl%d: %s bfr capable %d bfe capable %d beamforming mode %d\n",
		pub->unit, __FUNCTION__,
		(txbf->bfr_capable > TXBF_MU_BFR_CAP) ? TXBF_MU_BFR_CAP : txbf->bfr_capable,
		(txbf->bfe_capable > TXBF_MU_BFE_CAP) ? TXBF_MU_BFE_CAP : txbf->bfe_capable,
		txbf->mode));
	return BCME_OK;
}

void
wlc_txbf_impbf_upd(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;

	if (!txbf)
		return;

	wlc = txbf->wlc;

	if (D11REV_GT(wlc->pub->corerev, 40)) {
		bool is_caled = wlc_phy_is_txbfcal((wlc_phy_t *)WLC_PI(wlc));
		if (D11REV_IS(wlc->pub->corerev, 130)) {
			if (!wlc->pub->sih->otpflag && (CHIPREV(wlc->pub->sih->chiprev) <= 2)) {
				/* limit iTXBF to newly calibrated chips/boards */
				is_caled = FALSE;
			}
		}
		if (is_caled) {
			txbf->flags |= WLC_TXBF_FLAG_IMPBF;
		} else {
			txbf->flags &= ~WLC_TXBF_FLAG_IMPBF;
		}
	}
}

/* Returns TRUE if
 * a) Device is physically capable of acting as a VHTMU or HEMMU beamformer, and
 * b) the MU beamformer has been enabled by configuration.
 * Returns FALSE otherwise.
 */
bool
wlc_txbf_mutx_enabled(wlc_txbf_info_t *txbf, uint8 tx_type)
{
	wlc_info_t *wlc = txbf->wlc;
	ASSERT((tx_type == VHTMU) || (tx_type == HEMMU));
	if (WLC_PHY_AS_80P80(wlc, wlc->chanspec)) {
		return FALSE;
	}

	if (tx_type == VHTMU) {
		return ((txbf->bfr_capable & TXBF_MU_BFR_CAP) != 0);
	} else if (tx_type == HEMMU) {
		 return (HE_MMU_ENAB(wlc->pub) && ((txbf->bfr_capable & TXBF_HE_MU_BFR_CAP) != 0));
	}
	return FALSE;
}

/* Returns TRUE if
 * a) this device is physically capable of acting as an MU beamformee, and
 * b) the MU beamformee has been enabled by configuration.
 * Returns FALSE otherwise.
 */
bool
wlc_txbf_murx_capable(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc = txbf->wlc;
	return (((txbf->bfe_capable & TXBF_MU_BFE_CAP) != 0) &&
		!WLC_PHY_AS_80P80(wlc, wlc->chanspec));
}

void
wlc_txbf_mutimer_update(wlc_txbf_info_t *txbf, bool force_disable)
{
	wlc_info_t *wlc = txbf->wlc;

	BCM_REFERENCE(wlc);
	if (PSMX_ENAB(wlc->pub) && (D11REV_LT(wlc->pub->corerev, 128))) {
		if (MU_TX_ENAB(wlc) && !force_disable)
			wlc_write_shmx(wlc, MX_MUSND_PER(wlc), txbf->mu_sounding_period << 2);
		else
			wlc_write_shmx(wlc, MX_MUSND_PER(wlc), 0);
	}
}

static void
wlc_txbf_impbf_updall(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	txbf_scb_info_t *bfi;

	if (!txbf)
		return;

	wlc = txbf->wlc;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi) continue;
		wlc_txbf_init_imp(txbf, scb, bfi);
	}
	/* invalid tx header cache */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);
}

void
wlc_txbf_chanspec_upd(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	txbf_scb_info_t *bfi;

	if (!txbf) {
		return;
	}

	if (!txbf->pub->up) {
		return;
	}

	wlc = txbf->wlc;

	/* delete all txbf links if STA is moving to 160 MHz channel */
	if (WLC_PHY_AS_80P80(wlc, wlc->chanspec)) {
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
			if (!bfi) {
				continue;
			}
			/* delete explict txbf links */
			wlc_txbf_delete_link(txbf, scb);
			/* delete implicit txbf */
			wlc_txbf_init_imp(txbf, scb, bfi);
		}
	} else {
		WL_TXBF(("wl%d: %s: update chanspec 0x%04x home 0x%04x\n",
			wlc->pub->unit, __FUNCTION__, wlc->chanspec,
			wlc->home_chanspec));

		if (D11REV_GE(wlc->pub->corerev, 128)) {
			wlc_suspend_mac_and_wait(wlc);
			if (wlc->home_chanspec != wlc->chanspec) {
				wlc_mctrlx(wlc, MCTLX_FRCHAN, MCTLX_FRCHAN);
				wlc_mctrl(wlc, MCTL_FRCHAN, MCTL_FRCHAN);
			} else {
				wlc_mctrlx(wlc, MCTLX_FRCHAN, 0);
				wlc_mctrl(wlc, MCTL_FRCHAN, 0);
			}
			wlc_enable_mac(wlc);
		} else {
			FOREACHSCB(wlc->scbstate, &scbiter, scb) {
				bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
				if (!bfi) {
					continue;
				}
				wlc_txbf_bfr_chanspec_upd(txbf, bfi);
			}
		}
	}
	/* invalid tx header cache */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);
}

void
BCMATTACHFN(wlc_txbf_init)(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	uint16 corerev;

	ASSERT(txbf);
	if (!txbf) {
		return;
	}
	wlc = txbf->wlc;
	corerev = wlc->pub->corerev;

	txbf->shm_idx_bmp = 0;
	if (D11REV_GE(corerev, 40)) {
		if (WLC_BITSCNT(wlc->stf->hw_txchain) > 1) {
			txbf->bfr_capable = WLC_SU_BFR_CAP_PHY(wlc) ? TXBF_SU_BFR_CAP: 0;
			txbf->mode = TXBF_AUTO;

			if (D11REV_LT(corerev, 64)) {
				txbf->impbf_usr_idx = IMPBF_REV_LT64_USR_IDX;
			} else {
				txbf->impbf_usr_idx = IMPBF_REV_GE64_USR_IDX;
			}

			if (D11REV_GT(corerev, 40)) {
				txbf->imp = TXBF_AUTO;
			}
			if (D11REV_GE(corerev, 65)) {
				/* force txbf for all rates since the feature to enable transmit
				 * frames with TXBF considering regulatory per rate is still work in
				 * progress
				 */
				if (D11REV_IS(corerev, 130) || D11REV_IS(corerev, 131)) {
					txbf->bfr_capable |= (WLC_MU_BFR_CAP_PHY(wlc) &&
						(WLC_BITSCNT(wlc->stf->hw_txchain) >= 2))
						? TXBF_MU_BFR_CAP : 0;
				} else {
					txbf->bfr_capable |= (WLC_MU_BFR_CAP_PHY(wlc) &&
						(WLC_BITSCNT(wlc->stf->hw_txchain) >= 4))
						? TXBF_MU_BFR_CAP : 0;
				}
#ifdef WL_PSMX
				if (PSMX_ENAB(wlc->pub)) {
					txbf->mu_max_links = TXBF_MU_MAX_LINKS;
					txbf->mu_sounding_period = TXBF_MU_SOUND_PERIOD_DFT;
				}
#endif // endif
			}
#ifdef WL11AX
			txbf->bfr_capable |= WLC_HE_SU_BFR_CAP_PHY(wlc) ? TXBF_HE_SU_BFR_CAP : 0;
			txbf->bfr_capable |= (WLC_HE_SU_MU_BFR_CAP_PHY(wlc) &&
				(WLC_BITSCNT(wlc->stf->hw_txchain) >= 2)) ?
				TXBF_HE_SU_MU_BFR_CAP : 0;
			if (D11REV_GE(corerev, 129) && D11REV_LT(corerev, 131)) {
				txbf->bfr_capable |= TXBF_HE_CQI_BFR_CAP;
			}
#endif /* WL11AX */
		} else {
			txbf->bfr_capable = 0;
			txbf->mode = txbf->imp = TXBF_OFF;
		}

		txbf->bfe_capable = WLC_SU_BFE_CAP_PHY(wlc) ? TXBF_SU_BFE_CAP : 0;
		txbf->bfe_capable |= WLC_MU_BFE_CAP_PHY(wlc) ? TXBF_MU_BFE_CAP : 0;
		if (D11REV_GE(corerev, 128)) {
			txbf->bfe_capable |= WLC_HE_SU_BFE_CAP_PHY(wlc) ? TXBF_HE_SU_BFE_CAP : 0;
			txbf->bfe_capable |= WLC_HE_SU_MU_BFE_CAP_PHY(wlc) ?
				TXBF_HE_SU_MU_BFE_CAP : 0;
		}

		txbf->active = FALSE;
		txbf->sounding_period = BF_SOUND_PERIOD_DFT;
		txbf->max_link = TXBF_MAX_LINK;
		txbf->max_link_ext = TXBF_MAX_LINK_EXT;
		txbf->max_rssi = TXBF_MAX_RSSI;
		txbf->min_rssi = TXBF_MIN_RSSI;
		txbf->pkt_thre_sec = TXBF_PKT_THRE_SEC;
		txbf->pkt_thre_sched = txbf->pkt_thre_sec * txbf->sched_timer_interval / 1000;
	} else {
		txbf->bfr_capable = 0;
		txbf->bfe_capable = 0;
		txbf->mode = txbf->imp = TXBF_OFF;
		txbf->active = FALSE;
		txbf->max_link = 0;
	}
	txbf->applied2ovr = 0;
	txbf->virtif_disable = FALSE;
	txbf->ht_enable = FALSE;
	WL_TXBF(("wl%d: %s bfr capable %d bfe capable %d explicit txbf mode %s"
		"explicit txbf active %s Allow txbf on non primaryIF: %s\n",
		wlc->pub->unit, __FUNCTION__,
		((txbf->bfr_capable > TXBF_MU_BFR_CAP) ? TXBF_MU_BFR_CAP : txbf->bfr_capable),
		((txbf->bfe_capable > TXBF_MU_BFE_CAP) ? TXBF_MU_BFE_CAP : txbf->bfe_capable),
		txbf->mode == TXBF_ON ? "on" : (txbf->mode == TXBF_AUTO ? "auto" : "off"),
		(txbf->active ? "TRUE" : "FALSE"), (txbf->virtif_disable ? "TRUE" : "FALSE")));

	/* turn off bfm-based spatial expansion by default now */
	txbf->bfm_spexp = 0;
	/* turn off bfr spatial expansion (available for 4365C0 only) by default now */
	txbf->bfr_spexp = 0;
}

#ifdef WL_PSMX
/* initialize PHY user index in SHM BFI Block */
void
wlc_txbf_bfi_init(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	uint16 val, i;
	uint16 bfi_blk, bfridx_offset;
	uint8 su_bfi_stidx, su_usr_idx, max_usr_idx;

	wlc = txbf->wlc;
	if (!wlc->clk) {
		return;
	}

	txbf->su_bfi_stidx = 0;
	txbf->max_link = TXBF_MAX_LINK;
	txbf->impbf_usr_idx = IMPBF_REV_GE64_USR_IDX;

	if (!MU_TX_ENAB(txbf->wlc)) {
		return;
	}

	/* Adjust SU SHM bfi block start index, Max SU SHM index */
	max_usr_idx = txbf->impbf_usr_idx = TXBF_MAX_SU_USRIDX_GE64;
	su_bfi_stidx = txbf->su_bfi_stidx = wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU);
	/* MU takes one TXV memory slot, where as SU takes two, Set SU index to
	 * start from even slot after MU TXV
	 */
	su_usr_idx = ROUNDUP(su_bfi_stidx, 2);
	txbf->max_link = (su_bfi_stidx + ((max_usr_idx - su_usr_idx) >> 1));

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		return;
	}

	/* Set PHY user index in SHM for all SU BFI links */
	for (i = su_bfi_stidx; i <= txbf->max_link; i++) {
		bfi_blk = txbf->shm_base + i * BFI_BLK_SIZE(wlc->pub->corerev);
		bfridx_offset = shm_addr(bfi_blk, C_BFI_BFRIDX_POS);
		val = wlc_read_shm(wlc, bfridx_offset);
		val &= ~(C_BFRIDX_MASK);
		val |= ((su_usr_idx) & C_BFRIDX_MASK);
		wlc_write_shm(wlc, bfridx_offset, val);
		su_usr_idx = su_usr_idx + 2;
	}
}
#endif /* WL_PSMX */

void
wlc_txbf_link_upd(wlc_txbf_info_t *txbf, scb_t *scb)
{
	ASSERT(txbf);
	ASSERT(scb);

	if (!SCB_INTERNAL(scb) && SCB_ASSOCIATED(scb)) {
		/* Re-initial beamforming link, may change from SU to MU */
		wlc_txbf_delete_link(txbf, scb);
		wlc_txbf_init_link(txbf, scb);
	}
}

#ifdef WL_PSMX
/* Function to check if a partcilar HE client is qualified for mu sounding */
static bool
wlc_txbf_hemu_sounding_link_qualify(wlc_txbf_info_t *txbf, scb_t *scb)
{
	if ((!HE_DLMU_ENAB(txbf->pub) && !HE_MMU_ENAB(txbf->pub)) ||
		(wlc_he_get_omi_tx_nsts(txbf->wlc->hei, scb) > MUMAX_NSTS_ALLOWED)) {
		return FALSE;
	}

	return TRUE;
}

static bool
wlc_txbf_mu_link_qualify(wlc_txbf_info_t *txbf, scb_t *scb)
{
#ifdef WL_MU_TX
	int i;
	uint8 mu_count = 0;
	wlc_info_t *wlc = txbf->wlc;
	uint16 vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
	txbf_scb_info_t *bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(txbf);
	ASSERT(scb);

	/* skip admitting HEMU STA as VHTMU, if the STA is qualified as HEMU */
	if (HE_DLMU_ENAB(wlc->pub) &&
		(txbf->bfr_capable & TXBF_HE_MU_BFR_CAP) &&
		(bfi->bfe_capable & TXBF_HE_MU_BFE_CAP)) {
		return FALSE;
	}

	if (!MU_TX_ENAB(txbf->wlc) ||
		(!(vht_flags & SCB_MU_BEAMFORMEE)) ||
		wlc_mutx_sta_on_hold(wlc->mutx, scb) ||
		!wlc_mutx_sta_mu_link_permit(wlc->mutx, scb) ||
		bfi->bfe_nrx > MUMAX_NSTS_ALLOWED)
		return FALSE;

	/* count number of mu scb, except itself */
	for (i = 0; i < txbf->mu_max_links; i++) {
		if ((txbf->mu_scbs[i] != NULL) &&
			eacmp(&txbf->mu_scbs[i]->ea, &scb->ea) != 0) {
			mu_count++;
		}
	}

	if (mu_count && mu_count >= wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU))
		return FALSE;
#endif /* WL_MU_TX */

	return TRUE;
}
#ifdef WL_MU_TX
bool
wlc_txbf_is_mu_bfe(wlc_txbf_info_t *txbf, scb_t *scb)
{
	txbf_scb_info_t *bfi;

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	if (!bfi) {
		return FALSE;
	}
	if (SCB_HE_CAP(scb) &&
		!(bfi->bfe_capable & TXBF_HE_MU_BFE_CAP)) {
		return FALSE;
	}
	if (!SCB_HE_CAP(scb) &&
		SCB_VHT_CAP(scb) &&
		!(bfi->bfe_capable & TXBF_MU_BFE_CAP)) {
		return FALSE;
	}
	return TRUE;
}

uint8
wlc_txbf_get_free_su_bfr_links(wlc_txbf_info_t *txbf)
{
	uint8 i, cnt = 0, su_shm_idx = 0;

	su_shm_idx = txbf->su_bfi_stidx;

	for (i = su_shm_idx; i <= txbf->max_link; i++) {
		if (isclr(&txbf->shm_idx_bmp, i)) {
			if (i == (uint8)txbf->impbf_usr_idx) {
				continue;
			}
			cnt++;
		}
	}

	return cnt;
}
#endif /* WL_MU_TX */
#endif /* WL_PSMX */

int
wlc_txbf_mu_max_links_upd(wlc_txbf_info_t *txbf, uint8 num)
{
	if (txbf->pub->up)
		return BCME_NOTDOWN;

	if (num > TXBF_MU_MAX_LINKS)
		return BCME_RANGE;

	txbf->mu_max_links = num;
	return BCME_OK;
}

/*
 * initialize explicit beamforming
 * return BCME_OK if explicit is enabled, else
 * return BCME_ERROR
 */
static int
wlc_txbf_init_exp_ge128(wlc_txbf_info_t *txbf, scb_t *scb, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg;
	wlc_pub_t *pub;

	wlc = txbf->wlc;
	cfg = scb->bsscfg;
	pub = wlc->pub;
	BCM_REFERENCE(pub);
	if ((!(txbf->bfr_capable || txbf->bfe_capable)) ||
		WLC_PHY_AS_80P80(wlc, wlc->chanspec)|| (!wlc->clk)) {
		return BCME_OK;
	}

	if (txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg))) {
		return BCME_OK;
	}

	/* init default index to be invalid */
	bfi->amt_index = wlc_ratelinkmem_get_scb_link_index(wlc, scb);
	bfi->flags = 0;

#ifdef WL11AX
	if (SCB_HE_CAP(scb)) {
		uint16 he_flags;
		he_flags = wlc_he_get_scb_flags(wlc->hei, scb);
		if (he_flags & SCB_HE_SU_MU_BFE) {
			WL_TXBF(("wl%d %s: sta has HE BFE cap\n", pub->unit, __FUNCTION__));

			/* For HE, if SU_BFE is supported, then MU_BFE must be supported */
			bfi->bfe_capable = TXBF_HE_SU_MU_BFE_CAP;
			if ((txbf->bfr_capable & TXBF_HE_SU_BFR_CAP) &&
				(bfi->bfe_capable & TXBF_HE_SU_BFE_CAP)) {
				bfi->flags |= HE_SU_BFE;
			}
		} else {
			bfi->bfe_capable &= ~TXBF_HE_SU_MU_BFE_CAP;
		}

		if (he_flags & SCB_HE_CQI_BFE) {

			bfi->bfe_capable |= TXBF_HE_CQI_BFE_CAP;
			if (txbf->bfr_capable & TXBF_HE_CQI_BFR_CAP)
				bfi->flags |= HE_CQI_BFE;
			WL_TXBF(("wl%d %s: sta has HE CQI cap 0x%x\n", pub->unit,
				__FUNCTION__, bfi->flags));
		} else {
			bfi->bfe_capable &= ~TXBF_HE_CQI_BFE_CAP;
		}

		if (he_flags & (SCB_HE_SU_BFR | SCB_HE_MU_BFR)) {
			WL_TXBF(("wl%d %s: sta has HE %s BFR cap\n",
				pub->unit, __FUNCTION__,
				(he_flags & SCB_HE_MU_BFR) ? "MU":"SU"));

			ASSERT(he_flags & SCB_HE_SU_BFR);

			bfi->bfr_capable = TXBF_HE_SU_BFR_CAP;
			if (he_flags & SCB_HE_MU_BFR)
				bfi->bfr_capable |= TXBF_HE_MU_BFR_CAP;
		} else {
			bfi->bfr_capable &= ~TXBF_HE_SU_MU_BFR_CAP;
		}

		bfi->bfe_sts_cap = getbits((uint8 *)&bfi->he_cap, sizeof(bfi->he_cap),
			HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_IDX, HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_FSZ);
		bfi->bfe_sts_cap_bw160 = getbits((uint8 *)&bfi->he_cap, sizeof(bfi->he_cap),
			HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_IDX, HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_FSZ);
		bfi->bfe_nrx = HE_MAX_SS_SUPPORTED(scb->rateset.he_bw80_rx_mcs_nss);
		bfi->bfe_nrx_bw160 = HE_MAX_SS_SUPPORTED(scb->rateset.he_bw160_rx_mcs_nss);
		bfi->bfe_nrx_bw80p80 = HE_MAX_SS_SUPPORTED(scb->rateset.he_bw80p80_rx_mcs_nss);

		WL_TXBF(("wl%d %s he_flags 0x%x bfr_cap 0x%x bfe_cap 0x%x bfe_sts %d "
			"bfe_sts_160 %d\nbfe_nrx %d bfe_nrx_bw160 %d bfe_nrx_bw80p80 %d "
			"my bfr_cap 0x%x bfe_cap 0x%x\n",
			pub->unit, __FUNCTION__,
			he_flags, bfi->bfr_capable, bfi->bfe_capable,
			bfi->bfe_sts_cap, bfi->bfe_sts_cap_bw160,
			bfi->bfe_nrx, bfi->bfe_nrx_bw160, bfi->bfe_nrx_bw80p80,
			txbf->bfr_capable, txbf->bfe_capable));
	} else
#endif /* WL11AX */
		wlc_txbf_init_exp_vht_ht_cap(txbf, scb, bfi);

	if (!((txbf->bfr_capable && bfi->bfe_capable) ||
		(txbf->bfe_capable && bfi->bfr_capable &&
		(wlc_vht_get_cap_info(wlc->vhti) & VHT_CAP_INFO_SU_BEAMFMEE)))) {
		WL_TXBF(("wl%d %s "MACF" fail: my bfr/bfe_cap = %d %d, peer bfr/bfe_cap = %d %d\n",
			pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea),
			txbf->bfr_capable, txbf->bfe_capable,
			bfi->bfr_capable, bfi->bfe_capable));
		return BCME_OK;
	}

	if (!txbf->active && (WLC_BITSCNT(wlc->stf->txchain) == 1)) {
		WL_TXBF(("wl%d %s: can not activate beamforming with ntxchains = %d\n",
			pub->unit, __FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
	} else if (txbf->mode) {
		WL_TXBF(("wl%d %s: beamforming activated! ntxchains %d\n", pub->unit,
			__FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
		txbf->active = TRUE;
	}

	if ((txbf->bfr_capable & TXBF_HE_MU_BFR_CAP) &&
		(bfi->bfe_capable & TXBF_HE_MU_BFE_CAP) &&
		wlc_txbf_hemu_sounding_link_qualify(txbf, scb)) {
		bfi->flags |= (HE_MU_BFE | HE_SU_BFE);
	} else if ((txbf->bfr_capable & TXBF_MU_BFR_CAP) && (bfi->bfe_capable & TXBF_MU_BFE_CAP) &&
		wlc_txbf_mu_link_qualify(txbf, scb)) {
		bfi->flags = MU_BFE;
	}

	if (((txbf->bfr_capable & TXBF_SU_MU_BFR_CAP) && (bfi->bfe_capable & TXBF_SU_MU_BFE_CAP)) ||
		(txbf->bfr_capable & bfi->bfe_capable)) {
		wlc_txbf_bfr_init_ge128(txbf, bfi);
		bfi->exp_en = TRUE;
	}

	if (txbf->bfe_capable && bfi->bfr_capable &&
		((wlc_vht_get_cap_info(wlc->vhti) & VHT_CAP_INFO_SU_BEAMFMEE) ||
		(WLC_HE_SU_BFE_CAP_PHY(wlc) && (txbf->bfe_capable & TXBF_HE_SU_BFE_CAP)))) {
		wlc_txbf_bfe_init_rev128(txbf, bfi);
	}

	bfi->init_pending = FALSE;
	if (RATELINKMEM_ENAB(pub)) {
		wlc_ratelinkmem_upd_lmem_int(wlc, scb, TRUE /* clr_txbf_sts=1 in mreq */);
	}

	WL_TXBF(("wl%d: %s enables sta "MACF". exp_en %d amt %d\n",
		pub->unit, __FUNCTION__,  ETHER_TO_MACF(scb->ea),
		bfi->exp_en, bfi->amt_index));
	return BCME_OK;

}

/*
 * initialize explicit beamforming
 * return BCME_OK if explicit is enabled, else
 * return BCME_ERROR
 */
static int
wlc_txbf_init_exp(wlc_txbf_info_t *txbf, scb_t *scb, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	int amt_idx;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 val, shm_idx = BF_SHM_IDX_INV, shmx_idx = BF_SHM_IDX_INV;
	int status;
	wlc_bsscfg_t *cfg;

	ASSERT(txbf);
	wlc = txbf->wlc;

	ASSERT(wlc);
	ASSERT(bfi);
	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(shmx_idx);
	cfg = scb->bsscfg;

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		wlc_txbf_init_exp_ge128(txbf, scb, bfi);
		return BCME_OK;
	}
	/* init default index to be invalid */
	bfi->mx_bfiblk_idx = BF_SHM_IDX_INV; /* for mu sounding */
	bfi->shm_index = BF_SHM_IDX_INV; /* for (su bfr) and (bfe) */
	bfi->flags = 0;

	if ((!(txbf->bfr_capable || txbf->bfe_capable)) ||
		WLC_PHY_AS_80P80(wlc, wlc->chanspec)|| (!wlc->clk)) {
		return BCME_OK;
	}

	if (txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg)))
		return BCME_OK;

	wlc_txbf_init_exp_vht_ht_cap(txbf, scb, bfi);

	if (!((txbf->bfr_capable && bfi->bfe_capable) ||
		(txbf->bfe_capable && bfi->bfr_capable &&
		(wlc_vht_get_cap_info(wlc->vhti) & VHT_CAP_INFO_SU_BEAMFMEE)))) {
		WL_TXBF(("wl%d %s fail: my bfr/bfe_cap = %d %d, peer bfr/bfe_cap = %d %d\n",
			wlc->pub->unit, __FUNCTION__,
			txbf->bfr_capable, txbf->bfe_capable,
			bfi->bfr_capable, bfi->bfe_capable));
		return BCME_OK;
	}

#ifdef WL_PSMX
#ifdef WL_MUPKTENG
	if (wlc_mutx_pkteng_on(wlc->mutx))
		return BCME_OK;
#endif // endif
	if ((txbf->bfr_capable & TXBF_MU_BFR_CAP) && (bfi->bfe_capable & TXBF_MU_BFE_CAP) &&
		wlc_txbf_mu_link_qualify(txbf, scb)) {
		shmx_idx = wlc_txbf_set_mx_bfiblk_idx(txbf, bfi);
		if (shmx_idx != BF_SHM_IDX_INV) {
			bfi->flags = MU_BFE;
			if ((txbf->bfe_capable & TXBF_SU_BFE_CAP) &&
				(bfi->bfr_capable & TXBF_SU_BFR_CAP)) {
				shm_idx = wlc_txbf_set_shm_idx(txbf, bfi);
			}
		} else {
			WL_ERROR(("wl%d: %s failed to set shmx idx\n",
					wlc->pub->unit, __FUNCTION__));
			shm_idx = wlc_txbf_set_shm_idx(txbf, bfi);
		}
	}
	else
#endif /* WL_PSMX */
	{
		shm_idx = wlc_txbf_set_shm_idx(txbf, bfi);
	}

	if ((shm_idx == BF_SHM_IDX_INV) &&
#ifdef WL_PSMX
	(shmx_idx == BF_SHM_IDX_INV) &&
#endif // endif
	TRUE) {
		WL_ERROR(("wl%d: %s failed to set shm idx\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	if (((txbf->bfr_capable & TXBF_SU_MU_BFR_CAP) && (bfi->bfe_capable & TXBF_SU_MU_BFE_CAP)) ||
		(txbf->bfr_capable & bfi->bfe_capable)) {
		wlc_txbf_bfr_init(txbf, bfi);
		bfi->exp_en = TRUE;
		bfi->init_pending = FALSE;
	}

	if (!(txbf->bfe_capable && bfi->bfr_capable && (shm_idx != BF_SHM_IDX_INV) &&
		((wlc_vht_get_cap_info(wlc->vhti) & VHT_CAP_INFO_SU_BEAMFMEE)))) {
		goto init_done;
	}

	wlc_txbf_bfe_init(txbf, bfi);

	status = wlc_txbf_get_amt(wlc, scb, &amt_idx);
	if (status != BCME_OK) {
		return status;
	}

	val = wlc_read_amtinfo_by_idx(wlc, amt_idx);
	val &= ~((NBITMASK(C_ADDR_BFIIDX_LT128_BSZ)) << C_ADDR_BFIIDX_LT128_NBIT);
	val |= (NBITVAL(C_ADDR_BFCAP_LT128_NBIT) | shm_idx << C_ADDR_BFIIDX_LT128_NBIT);

	bfi->amt_index = amt_idx;
	wlc_write_amtinfo_by_idx(wlc, amt_idx, val);
	bfi->init_pending = FALSE;

init_done:
	WL_TXBF(("wl%d: %s enables sta %s. exp_en %d amt %d shm_idx %u shmx_idx %u\n",
		wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf),
		bfi->exp_en, bfi->amt_index, shm_idx, shmx_idx));
	return BCME_OK;

}

/*
 * initialize implicit beamforming
 */
static bool
wlc_txbf_init_imp(wlc_txbf_info_t *txbf, scb_t *scb, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg;

	wlc = txbf->wlc;
	cfg = scb->bsscfg;

	if ((txbf->flags & WLC_TXBF_FLAG_IMPBF) && txbf->imp &&
		!(txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg))) && !WLC_PHY_AS_80P80(wlc, wlc->chanspec) &&
		IS_PHYBW_IMPBF_SUP(wlc, wlc->chanspec) && !bfi->exp_en)
		bfi->imp_en = TRUE;
	else
		bfi->imp_en = FALSE;

	/* as iovar also calls this func, reset counters here */
	bfi->imp_used = 0;
	bfi->imp_bad = 0;
	return bfi->imp_en;
}

uint8
wlc_txbf_get_bfe_sts_cap(wlc_txbf_info_t *txbf, scb_t *scb)
{
	txbf_scb_info_t *bfi;

	bfi = TXBF_SCB_INFO(txbf, scb);
	if (bfi == NULL)
		return 0;

	/* bfe_sts_cap = 3: foursteams, 2: three streams,
	 * 1: two streams
	 */
	return bfi->bfe_sts_cap;
}

#if defined(WL_BEAMFORMING) && !defined(WLTXBF_DISABLED)
/* JIRA:SWMUMIMO-457
 * WAR for intel interop issue: Using bfm to do frequency-domain spatial expansion
 */
bool
wlc_txbf_bfmspexp_enable(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc = txbf->wlc;
	bool bfm_spexp;

	if ((wlc->band->phyrev == 32 || wlc->band->phyrev == 33) &&
	CHSPEC_IS20(wlc->chanspec) && txbf->bfm_spexp) {
		bfm_spexp = TRUE;
	} else {
		bfm_spexp = FALSE;
	}

	return bfm_spexp;
}
#endif /* WL_BEAMFORMING && !defined(WLTXBF_DISABLED) */

bool
wlc_txbf_bfrspexp_enable(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc = txbf->wlc;
	bool bfr_spexp;

	if (wlc->band->phyrev == 33 && txbf->bfr_spexp) {
		bfr_spexp = TRUE;
	} else {
		bfr_spexp = FALSE;
	}

	return bfr_spexp;
}

static bool
wlc_disable_bfe_for_smth(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg)
{
	bool disable_bfe;
	wlc_info_t *wlc = txbf->wlc;

	/* Disable bfe capability for AP that has TDCS turned on and nrx > 2
	 * because TXBF gain is usually smaller than TDCS gain. We cannot
	 * do the same disabling for STA, because we will need to support
	 * 3x3 and 4x4 MU-STA some time later.
	 */
	if (BSSCFG_AP(cfg) && wlc_phy_is_smth_en((wlc_phy_t *)WLC_PI(wlc)) &&
	    (wlc->band->phyrev >= 32) && (WLC_BITSCNT(wlc->stf->rxchain) > 2)) {
		disable_bfe = TRUE;
	} else {
		disable_bfe = FALSE;
	}

	return disable_bfe;
}

/*
 * initialize txbf for this link: turn on explicit if can
 * otherwise try implicit
 * return BCME_OK if any of the two is enabled
 */
static int
wlc_txbf_init_link_serve(wlc_txbf_info_t *txbf, scb_t *scb)
{
	txbf_scb_info_t *bfi;
	int ret;
	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	/* If the core is not up
	* defer it until core is up
	*/
	if (!txbf->wlc->pub->up) {
		return BCME_NOTUP;
	}

	ret = wlc_txbf_init_exp(txbf, scb, bfi);

	/* If exp txbf enabled, disable imp txbf */
	if (wlc_txbf_init_imp(txbf, scb, bfi)) {
		return BCME_OK;
	}

	return ret;
}

#ifdef TXBF_MORE_LINKS
static int
wlc_txbf_init_link_ext(wlc_txbf_info_t *txbf, scb_t *scb)
{
	uint32 i, idx = 0;
	txbf_scb_info_t *bfi;
	scb_t *psta_prim = wlc_ap_get_psta_prim(txbf->wlc->ap, scb);
	BCM_REFERENCE(bfi);

	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	for (i = txbf->max_link + 1; i <= txbf->max_link_ext; i ++) {
		if (txbf->su_scbs[i] == scb) {
			return BCME_OK;
		} else if ((psta_prim != NULL) && (txbf->su_scbs[i] == psta_prim)) {
			/*
			* PSTA Aware AP:STA's belong to same PSTA share a single
			* TxBF link.
			*/
			return BCME_OK;
		} else if (!idx && txbf->su_scbs[i] == NULL) {
			idx = i;
		}
	}
	if (idx) {
		txbf->su_scbs[idx] = scb;
		return BCME_OK;
	}

	return BCME_ERROR;
}
#endif /* TXBF_MORE_LINKS */

int
wlc_txbf_init_link(wlc_txbf_info_t *txbf, scb_t *scb)
{
	int r;
	txbf_scb_info_t *bfi;
	bool su_bfe;

	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);
	BCM_REFERENCE(bfi);
	BCM_REFERENCE(su_bfe);

	r = wlc_txbf_init_link_serve(txbf, scb);

#ifdef TXBF_MORE_LINKS
	su_bfe = (!(bfi->flags & MU_BFE) && (txbf->bfr_capable & TXBF_SU_BFR_CAP) &&
			(bfi->bfe_capable & TXBF_SU_BFE_CAP)) ? TRUE : FALSE;

	if ((!bfi->exp_en) && su_bfe)
		wlc_txbf_init_link_ext(txbf, scb);
#endif // endif
	return r;
}

#ifdef WL_PSMX
static uint8
wlc_txbf_set_mx_bfiblk_idx(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	uint8 mx_bfiblk_idx = BF_SHM_IDX_INV, i;
	bool found = FALSE;
	wlc_info_t *wlc;
	scb_t *scb = bfi->scb;

	BCM_REFERENCE(eabuf);
	wlc = txbf->wlc;

	if (WL_TXBF_ON()) {
		if (bfi->exp_en && bfi->shm_index != BF_SHM_IDX_INV) {
			WL_TXBF(("wl%d %s: scb already has user index %d\n",
				wlc->pub->unit, __FUNCTION__, bfi->shm_index));
		}
	}

	if (!txbf->active && (WLC_BITSCNT(wlc->stf->txchain) == 1)) {
		WL_TXBF(("wl%d %s: can not activate beamforming with ntxchains = %d\n",
			wlc->pub->unit, __FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
	} else if (txbf->mode) {
		WL_TXBF(("wl%d %s: beamforming actived! ntxchains %d\n", wlc->pub->unit,
			__FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
		txbf->active = TRUE;
	}

	/* find a free index */
	for (i = 0; i < txbf->mu_max_links; i++) {
		if (isclr(&txbf->mx_bfiblk_idx_bmp, i)) {
			if (!found) {
				mx_bfiblk_idx = i;
				found = TRUE;
			}
		} else if (eacmp(&txbf->mu_scbs[i]->ea, &scb->ea) == 0) {
			/* check if scb match for any existing entries */
			WL_TXBF(("wl%d %s: TxBF link for %s already exist\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf)));
			mx_bfiblk_idx = i;
			found = TRUE;
			break;
		}
	}

	if (!found) {
		WL_ERROR(("%d: %s fail to find a free user index\n",
			wlc->pub->unit, __FUNCTION__));
		return BF_SHM_IDX_INV;
	}

	bfi->mx_bfiblk_idx = mx_bfiblk_idx;
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub)) {
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
		wlc_txbf_mubfi_update(txbf, bfi, TRUE);
		wlc_bmac_enable_macx(wlc->hw);
	}
#endif // endif
	txbf->mu_scbs[mx_bfiblk_idx] = scb;

	WL_TXBF(("wl%d: %s add 0x%p %s shmx_index %d shm_bmp 0x%04x\n",
		wlc->pub->unit, __FUNCTION__, scb,
		bcm_ether_ntoa(&scb->ea, eabuf), mx_bfiblk_idx, txbf->mx_bfiblk_idx_bmp));

	return mx_bfiblk_idx;
}

static void
wlc_txbf_mubfi_update(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi, bool set)
{
	wlc_info_t *wlc;
	uint16 bfi_index;

	wlc = txbf->wlc;

	if (!wlc->clk) {
		return;
	}

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		return;
	}

	bfi_index = bfi->mx_bfiblk_idx;
	if (set) {
		setbit(&txbf->mx_bfiblk_idx_bmp, bfi_index);
	} else {
		clrbit(&txbf->mx_bfiblk_idx_bmp, bfi_index);
	}

}
#endif /* WL_PSMX */

static uint8
wlc_txbf_set_shm_idx(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	uint8 shm_idx = BF_SHM_IDX_INV, i;
	bool found = FALSE;
	scb_t *scb = bfi->scb;
	wlc_info_t *wlc;
	bool bfr, bfe;
	wlc_pub_t *pub = txbf->pub;
	uint8 su_shm_idx = 0;

	BCM_REFERENCE(eabuf);
	ASSERT(txbf);
	wlc = txbf->wlc;
	bfr = bfe = FALSE;

	if (MU_TX_ENAB(txbf->wlc)) {
		bfe = ((txbf->bfe_capable & TXBF_SU_BFE_CAP) &&
			(bfi->bfr_capable & TXBF_SU_BFR_CAP) &&
			(wlc_vht_get_cap_info(wlc->vhti) & VHT_CAP_INFO_SU_BEAMFMEE)) ||
			(WLC_HE_SU_BFE_CAP_PHY(wlc) &&
			(txbf->bfe_capable & TXBF_HE_SU_BFE_CAP)) ? TRUE : FALSE;
		bfr = ((!(bfi->flags & MU_BFE) && (txbf->bfr_capable & TXBF_SU_BFR_CAP) &&
			(bfi->bfe_capable & TXBF_SU_BFE_CAP)) ||
			((!(bfi->flags & HE_MU_BFE) && (txbf->bfr_capable & TXBF_HE_SU_BFR_CAP) &&
			(bfi->bfe_capable & TXBF_HE_SU_BFE_CAP)))) ? TRUE : FALSE;
		if (!bfr && !bfe) {
			return BF_SHM_IDX_INV;
		}

		if (bfr) {
			/* start from idx 'max_muclients' for su bfr link */
			su_shm_idx = txbf->su_bfi_stidx;
		} else {
			/* start from idx '0' for su bfe link */
			su_shm_idx = 0;
		}
	}

	if (bfi->exp_en) {
		WL_ERROR(("wl%d %s: scb already has user index %d\n", pub->unit, __FUNCTION__,
			bfi->shm_index));
	}

	if (!txbf->active && (WLC_BITSCNT(wlc->stf->txchain) == 1)) {
		WL_TXBF(("wl%d %s: can not activate beamforming with ntxchains = %d\n",
			pub->unit, __FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
	} else if (txbf->mode) {
		WL_TXBF(("wl%d %s: beamforming activated! ntxchains %d\n", pub->unit,
			__FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
		txbf->active = TRUE;
	}

	/* find a free index */
	for (i = su_shm_idx; i <= txbf->max_link; i++) {
		if ((txbf->shm_idx_bmp & (1 << i)) == 0) {
			if (i == (uint8)txbf->impbf_usr_idx) {
				continue;
			}

			if (!found) {
				shm_idx = i;
				found = TRUE;
			}
		} else {
			scb_t *psta_prim = wlc_ap_get_psta_prim(wlc->ap, scb);

			/* check if scb match to any exist entrys */
			if (eacmp(&txbf->su_scbs[i]->ea, &scb->ea) == 0) {
				WL_TXBF(("wl%d %s: TxBF link for %s already exist\n",
					pub->unit, __FUNCTION__,
					bcm_ether_ntoa(&scb->ea, eabuf)));
				bfi->shm_index = i;
				/* all PSTA connection use same txBF link */
				if (!(PSTA_ENAB(pub) && BSSCFG_PSTA(SCB_BSSCFG(scb)))) {
					txbf->su_scbs[i] = scb;
				}
				return i;
			} else if (txbf->su_scbs[i] == psta_prim) {
				/*
				* PSTA Aware AP:STA's belong to same PSTA share a single
				* TxBF link.
				*/
				bfi->shm_index = i;
				WL_TXBF(("wl%d %s: TxBF link for ProxySTA %s shm_index %d\n",
				pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf), bfi->shm_index));
				return bfi->shm_index;
			}
		}
	}

	if (!found) {
		WL_ERROR(("%d %s: fail to find a free user index\n", pub->unit, __FUNCTION__));
		return BF_SHM_IDX_INV;
	}

	bfi->shm_index = shm_idx;
	bfi->amt_index = pub->max_addrma_idx;
	setbit(&txbf->shm_idx_bmp, shm_idx);
	txbf->su_scbs[shm_idx] = scb;

	WL_TXBF(("wl%d %s: add 0x%p %s shm_idx %d shm_bmp 0x%04x\n",
		pub->unit, __FUNCTION__, scb,
		bcm_ether_ntoa(&scb->ea, eabuf), shm_idx, txbf->shm_idx_bmp));

	return shm_idx;
}

static void
wlc_txbf_delete_link_serve_ge128(wlc_txbf_info_t *txbf, scb_t *scb)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	wlc_info_t *wlc;
	txbf_scb_info_t *bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	uint16 vht_flags;

	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(vht_flags);

	ASSERT(txbf);
	wlc = txbf->wlc;
	ASSERT(wlc);

	WL_TXBF(("wl%d: %s %s\n",
		wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));

	if (!bfi) {
		WL_ERROR(("wl%d: %s failed for %s\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
		return;
	}

	vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);

	if (!(bfi->bfr_capable || bfi->bfe_capable)) {
		WL_TXBF(("wl%d: %s STA %s doesn't have TxBF cap %x\n", wlc->pub->unit,
			__FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf), vht_flags));
		return;
	}

	WL_TXBF(("wl%d: %s delete beamforming link %s exp_en %d amt_index %d\n",
		wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), bfi->exp_en,
		bfi->amt_index));

	bfi->exp_en = FALSE;
	bfi->init_pending = TRUE;
	bfi->BFIConfig0 &= ~(1 << C_LNK_BFIVLD_NBIT);

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}
}

static void
wlc_txbf_delete_link_serve(wlc_txbf_info_t *txbf, scb_t *scb)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	uint32 i = 0, j;
	wlc_info_t *wlc;
	txbf_scb_info_t *bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	uint16 vht_flags;
	scb_t *psta_prim;

	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(j);
	BCM_REFERENCE(vht_flags);

	ASSERT(txbf);
	wlc = txbf->wlc;
	ASSERT(wlc);

	WL_TXBF(("wl%d: %s %s\n",
		wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));

	if (!bfi) {
		WL_ERROR(("wl%d: %s failed for %s\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
		return;
	}

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		wlc_txbf_delete_link_serve_ge128(txbf, scb);
		return;
	}

	psta_prim = wlc_ap_get_psta_prim(wlc->ap, scb);

	vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);

#ifdef WL_PSMX
	for (j = 0; j < txbf->mu_max_links; j++) {
		if (isclr(&txbf->mx_bfiblk_idx_bmp, j))
			continue;
		if (txbf->mu_scbs[j] == scb) {
			break;
		}
	}
#endif // endif

	for (i = 0; i <= txbf->max_link; i++) {
		if (isclr(&txbf->shm_idx_bmp, i))
			continue;
		if ((txbf->su_scbs[i] == scb) || (txbf->su_scbs[i] == psta_prim)) {
			break;
		}
	}

	if ((i > txbf->max_link) &&
#ifdef WL_PSMX
	(j == txbf->mu_max_links) &&
#endif // endif
	TRUE) {
		return;
	}

	if (!(bfi->bfr_capable || bfi->bfe_capable)) {
		WL_ERROR(("wl%d: %s STA %s doesn't have TxBF cap %x\n", wlc->pub->unit,
			__FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf), vht_flags));
		return;
	}

	WL_TXBF(("wl%d: %s delete beamforming link %s shm_index %d amt_index %d\n",
		wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), bfi->shm_index,
		bfi->amt_index));

	if (!bfi->exp_en) {
		/* maybe it was disable due to txchain change, but link is still there */
		WL_ERROR(("wl%d: %s %s not enabled!\n", wlc->pub->unit, __FUNCTION__,
			bcm_ether_ntoa(&scb->ea, eabuf)));
	}

	bfi->exp_en = FALSE;
	bfi->init_pending = TRUE;
	if ((PSTA_ENAB(wlc->pub) && BSSCFG_PSTA(SCB_BSSCFG(scb))))
		return;

	if (wlc->pub->up) {
		uint16 val;
		if ((bfi->amt_index >= 0) && (bfi->amt_index < wlc->pub->max_addrma_idx) &&
			(txbf->su_scbs[i] == scb)) {
			val = wlc_read_amtinfo_by_idx(wlc, bfi->amt_index);
			val &= ~((NBITMASK(C_ADDR_BFIIDX_LT128_BSZ)) << C_ADDR_BFIIDX_LT128_NBIT);

			wlc_write_amtinfo_by_idx(wlc, bfi->amt_index, val);
		}
	}

	wlc_suspend_mac_and_wait(wlc);
	if ((i <= txbf->max_link) && (txbf->su_scbs[i] == scb)) {
		ASSERT(bfi->shm_index <= txbf->max_link);
		wlc_txbf_invalidate_bfridx(txbf, bfi, bfi->shm_index);
		clrbit(&txbf->shm_idx_bmp, i);
		txbf->su_scbs[i] = NULL;
		bfi->shm_index = BF_SHM_IDX_INV;
	}
	wlc_enable_mac(wlc);

#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub)) {
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
		if ((j < txbf->mu_max_links) && (txbf->mu_scbs[j] == scb)) {
			ASSERT(bfi->mx_bfiblk_idx < TXBF_MU_MAX_LINKS);
			wlc_txbf_invalidate_bfridx(txbf, bfi, bfi->mx_bfiblk_idx);
			wlc_txbf_mubfi_update(txbf, bfi, FALSE);
			txbf->mu_scbs[j] = NULL;
			bfi->mx_bfiblk_idx = BF_SHM_IDX_INV;
		}
		wlc_bmac_enable_macx(wlc->hw);
	}
#endif // endif

	if (!txbf->shm_idx_bmp &&
#ifdef WL_PSMX
	!txbf->mx_bfiblk_idx_bmp &&
#endif // endif
	TRUE) {
		txbf->active = FALSE;
		WL_TXBF(("wl%d: %s beamforming deactivated!\n", wlc->pub->unit, __FUNCTION__));
	}
}

#ifdef TXBF_MORE_LINKS
static int
wlc_txbf_delete_link_ext(wlc_txbf_info_t *txbf, scb_t *scb)
{
	uint32 i;

	for (i = txbf->max_link + 1; i <= txbf->max_link_ext; i++) {
		if (txbf->su_scbs[i] == scb) {
			txbf->su_scbs[i] = NULL;
			return BCME_OK;
		}
	}

	return BCME_ERROR;
}
#endif /* TXBF_MORE_LINKS */

void
wlc_txbf_delete_link(wlc_txbf_info_t *txbf, scb_t *scb)
{
#ifdef TXBF_MORE_LINKS
	if (wlc_txbf_delete_link_ext(txbf, scb) == BCME_OK)
		return;
#endif // endif
	wlc_txbf_delete_link_serve(txbf, scb);
}

void
wlc_txbf_delete_all_link(wlc_txbf_info_t *txbf)
{
	uint32 i;
	scb_t *scb = NULL;

	for (i = 0; i <= txbf->max_link; i++) {
		scb = txbf->su_scbs[i];
		if (scb) {
			wlc_txbf_delete_link(txbf, scb);
		}
	}
}

static
int wlc_txbf_down(void *context)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	uint32 i;

	ASSERT(txbf);
	for (i = 0; i <= txbf->max_link_ext; i++) {
		txbf->su_scbs[i] = NULL;
	}
	txbf->shm_idx_bmp = 0;
	txbf->active = FALSE;

#ifdef TXBF_MORE_LINKS
	if (txbf->sched_timer && txbf->sched_timer_added) {
		wl_del_timer(txbf->wlc->wl, txbf->sched_timer);
		txbf->sched_timer_added = 0;
	}
#endif	/* TXBF_MORE_LINKS */

	WL_TXBF(("wl%d: %s beamforming deactived!\n", txbf->wlc->pub->unit, __FUNCTION__));
	return BCME_OK;
}

static void
wlc_txbf_bfe_init_rev128(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	wlc_pub_t *pub;
	scb_t *scb = bfi->scb;
	uint8 bfr_nr, bfe_nsts = 0;

	wlc = txbf->wlc;
	pub = txbf->pub;
	ASSERT(wlc);

	// for linkmem
	if (SCB_HE_CAP(scb)) {
		bfr_nr = wlc_he_scb_get_bfr_nr(wlc->hei, scb);
	} else if (SCB_VHT_CAP(scb)) {
		bfr_nr = ((bfi->vht_cap & VHT_CAP_INFO_NUM_SOUNDING_DIM_MASK)
			>> VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT);
	} else if (SCB_HT_CAP(scb)) {
		bfr_nr = ((bfi->ht_txbf_cap & HT_CAP_TXBF_CAP_C_BFR_ANT_MASK)
			>> HT_CAP_TXBF_CAP_C_BFR_ANT_SHIFT);
	} else {
		bfr_nr = 1;
	}

	if (HE_ENAB_BAND(pub, wlc->band->bandtype)) {
		bfe_nsts = wlc_he_get_bfe_ndp_recvstreams(wlc->hei);
	} else if (VHT_ENAB(pub)) {
		bfe_nsts = wlc_phy_get_bfe_ndp_recvstreams((wlc_phy_t *)WLC_PI(wlc));
	} else if (txbf->ht_enable) {
		bfe_nsts = HT_CAP_TXBF_CAP_C_BFR_ANT_DEF_VAL;
	}

	setbits((uint8 *)&bfi->BFIConfig1, sizeof(bfi->BFIConfig1), C_LNK_BFENR_NBIT,
		NBITSZ(C_LNK_BFENR), MIN(bfe_nsts, bfr_nr));
}

static void
wlc_txbf_bfe_init(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	scb_t *scb = bfi->scb;
	uint16	bfi_blk, bfe_mimoctl;
	uint8 idx;
	bool isVHT = FALSE;
	int rxchains, bfr_nr;

	ASSERT(scb);

	wlc = txbf->wlc;
	ASSERT(wlc);

	idx = bfi->shm_index;
	bfi_blk = txbf->shm_base + idx * BFI_BLK_SIZE(wlc->pub->corerev);

	if (SCB_VHT_CAP(scb))
		isVHT = TRUE;

	rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	if (isVHT) {
		bfe_mimoctl = TXBF_BFE_MIMOCTL_VHT;
		bfr_nr = ((bfi->vht_cap & VHT_CAP_INFO_NUM_SOUNDING_DIM_MASK)
			>> VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT);
		bfe_mimoctl |= bfr_nr << C_BFEMMCTL_VHT_NR_NBIT;
		if (CHSPEC_IS160(wlc->chanspec) && isVHT) {
			bfe_mimoctl &= ~(1 << C_BFEMMCTL_VHT_CB_NBIT);
		}
		if (CHSPEC_IS40(wlc->chanspec)) {
			bfe_mimoctl |= (0x1 << C_BFEMMCTL_VHT_BW_NBIT);
		} else if (CHSPEC_IS80(wlc->chanspec)) {
			bfe_mimoctl |= (0x2 << C_BFEMMCTL_VHT_BW_NBIT);
		}
		bfe_mimoctl |= (rxchains - 1) << C_BFEMMCTL_VHT_NC_NBIT;
		if (MU_RX_ENAB(wlc)) {
			/* Bit-11 : Feedback type MU */
			bfe_mimoctl |= (0x1 << C_BFEMMCTL_VHT_FB_NBIT);
		} else
			bfe_mimoctl &= ~((0x1 << C_BFEMMCTL_VHT_FB_NBIT));
	} else {
		bfe_mimoctl = TXBF_BFE_MIMOCTL_HT;
		bfr_nr = ((bfi->ht_txbf_cap & HT_CAP_TXBF_CAP_C_BFR_ANT_MASK)
			>> HT_CAP_TXBF_CAP_C_BFR_ANT_SHIFT);
		bfe_mimoctl |= bfr_nr << C_BFEMMCTL_HT_NR_NBIT;

		if (CHSPEC_IS40(wlc->chanspec)) {
			bfe_mimoctl |= (0x1 << C_BFEMMCTL_HT_BW_NBIT);
		}
		bfe_mimoctl |= (rxchains - 1) << C_BFEMMCTL_HT_NC_NBIT;
	}

	wlc_suspend_mac_and_wait(wlc);

	wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BFE_MIMOCTL_POS),
		bfe_mimoctl);

	wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BFE_MYAID_POS), scb->bsscfg->AID);

	/*
	* If SCB is in ASSOCIATED state, then configure the BSSID in SHM. Note that when SCB is
	* ASSOCIATED and not AUTHORIZED, bsscfg->BSSID is still set to NULL. During this state
	* use BSSID from target BSS instead
	*/
	if (SCB_ASSOCIATED(scb)) {
		struct ether_addr *bssid;

		bssid = (ETHER_ISNULLADDR(&scb->bsscfg->BSSID) ? &scb->bsscfg->target_bss->BSSID :
			&scb->bsscfg->BSSID);
		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BSSID0_POS),
			((bssid->octet[1] << 8) | bssid->octet[0]));

		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BSSID1_POS),
			((bssid->octet[3] << 8) | bssid->octet[2]));

		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BSSID2_POS),
			((bssid->octet[5] << 8) | bssid->octet[4]));
	}
	wlc_enable_mac(wlc);
}

/* callback function upon evert chanspec change, corerev < 128 only */
static void
wlc_txbf_bfr_chanspec_upd(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	struct scb *scb = bfi->scb;
	uint16	bfi_blk, bfr_config0;
	bool isVHT = FALSE;
	uint8 bw;
#ifdef WL_PSMX
	uint16 val;
#endif /* WL_PSMX */

	ASSERT(scb);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		return;
	}
#ifdef WL_PSMX
	if ((bfi->flags & MU_BFE)) {
		if (bfi->mx_bfiblk_idx == BF_SHM_IDX_INV)
			return;
	} else
#endif /* WL_PSMX */
	{
		if (bfi->shm_index == BF_SHM_IDX_INV)
			return;
	}

	if (SCB_VHT_CAP(scb)) {
		isVHT = TRUE;
	}

	bfr_config0 = (TXBF_BFR_CONFIG0 | (isVHT << (TXBF_BFR_CONFIG0_FRAME_TYPE_SHIFT)));
	bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
	if (CHSPEC_IS80(wlc->chanspec)) {
		if (bw == BW_20MHZ) {
			bfr_config0 |= (((CHSPEC_CTL_SB(wlc->chanspec)
				>> WL_CHANSPEC_CTL_SB_SHIFT) & 0x3) << 10);
		} else if (bw == BW_40MHZ) {
			bfr_config0 |= (0x1 << 13);
			bfr_config0 |= (((CHSPEC_CTL_SB(wlc->chanspec)
				>> (WL_CHANSPEC_CTL_SB_SHIFT+1)) & 0x1) << 10);
		} else if (bw == BW_80MHZ) {
			bfr_config0 |= (0x2 << 13);
		}
	} else if (CHSPEC_IS40(wlc->chanspec)) {
		if (bw == BW_20MHZ) {
			bfr_config0 |= (((CHSPEC_CTL_SB(wlc->chanspec)
				>> WL_CHANSPEC_CTL_SB_SHIFT) & 0x1) << 10);
		} else if (bw == BW_40MHZ) {
			bfr_config0 |= (0x1 << 13);
		}
	}

#ifdef WL_PSMX
	if ((bfi->flags & MU_BFE)) {
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
		bfi_blk = txbf->mx_bfiblk_base +
				(bfi->mx_bfiblk_idx * BFI_BLK_SIZE(wlc->pub->corerev));
		/* Invalidate bfridx */
		val = wlc_read_shmx(wlc, shm_addr(bfi_blk, C_BFI_BFRIDX_POS));
		val &= (~(1 << C_BFRIDX_VLD_NBIT));
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_BFRIDX_POS), val);
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_BFR_CONFIG0_POS), bfr_config0);
		wlc_bmac_enable_macx(wlc->hw);
	} else
#endif /* WL_PSMX */
	{
		wlc_suspend_mac_and_wait(wlc);
		bfi_blk = txbf->shm_base + bfi->shm_index * BFI_BLK_SIZE(wlc->pub->corerev);
		wlc_txbf_invalidate_bfridx(txbf, bfi, shm_addr(bfi_blk, C_BFI_BFRIDX_POS));
		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BFR_CONFIG0_POS), bfr_config0);
		wlc_enable_mac(wlc);
	}
}

static void
wlc_txbf_bfr_init_ge128(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	wlc_pub_t *pub;
	scb_t *scb = bfi->scb;
	uint8 bw, bfi_mode = 0; /* 0=ht, 1=vht, 2=he */
	uint32 sb = 0;
	uint16 bfrcfg0 = 0, bfrcfg1 = 0, bfrStats = 0;
	uint8 bfe_nr = 0;

	ASSERT(scb);

	wlc = txbf->wlc;
	pub = txbf->pub;

	ASSERT(wlc);
	BCM_REFERENCE(pub);

	if (SCB_HE_CAP(scb)) {
		uint8 mu_cb, mu_ng;
		bfi_mode = 2;
		/* IE include declaration if low resolution is supported but since high resolution
		 * is prefered anyway we ignore IE and request high resolution
		 */
		mu_cb = 1;
		mu_ng = getbits((uint8 *)&bfi->he_cap, sizeof(bfi->he_cap),
			HE_PHY_MU_FEEDBACK_NG16_SUPPORT_IDX, HE_PHY_MU_FEEDBACK_NG16_SUPPORT_FSZ);

		bfrcfg0 |= (mu_cb << C_LNK_MUCB_NBIT);
		bfrcfg0 |= (mu_ng << C_LNK_MUNG_NBIT);
		bfrcfg0 |= (1 << C_LNK_DISAMB_NBIT);

	} else if (SCB_VHT_CAP(scb)) {
		bfi_mode = 1;
	}
	bfrcfg0 |= (bfi_mode << C_LNK_STY_NBIT);

	if ((bfi->flags & MU_BFE) || ((bfi->flags & HE_MU_BFE) && HE_MMU_ENAB(wlc->pub))) {
		/* update rate capabilities for this mu user */
		wlc_svmp_update_ratecap(wlc, scb, bfi->amt_index);
		setbit(&bfrcfg0, C_LNK_MUCAP_NBIT);
	}

	bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
	sb = CHSPEC_CTL_SB(wlc->chanspec);
	if (CHSPEC_IS80(wlc->chanspec)) {
		if (bw == BW_20MHZ) {
			sb = (sb >> WL_CHANSPEC_CTL_SB_SHIFT) & 0x3;
		} else if (bw == BW_40MHZ) {
			sb = (sb >> (WL_CHANSPEC_CTL_SB_SHIFT+1)) & 0x1;
		}
	} else if (CHSPEC_IS40(wlc->chanspec)) {
		if (bw == BW_20MHZ) {
			sb = (sb >> WL_CHANSPEC_CTL_SB_SHIFT) & 0x1;
		}
	}

	WL_TXBF(("wl%d: %s a %s-link: sta:"MACF" BSSIDX:%d\n",
		pub->unit, __FUNCTION__, SCB_HE_CAP(scb) ? "HE" :
		(SCB_VHT_CAP(scb) ? "VHT" : "HT"),
		ETHER_TO_MACF(scb->ea), WLC_BSSCFG_IDX(scb->bsscfg)));
#ifdef WL_PSMX
	if (PSMX_ENAB(pub)) {
		uint8 nc_idx = 0, ndps;
		uint8 bfe_nrx = 0, bfe_sts = 0, bfr_sts = 0, bfr_ntx = 0;

		bfr_ntx = (uint8) WLC_BITSCNT(wlc->stf->txchain) - 1;

		if ((bfi->flags & HE_MU_BFE)) {
			if (bw == BW_160MHZ) {
#ifdef WL11AX
				bfr_sts = wlc_he_get_bfr_ndp_sts(wlc->hei, TRUE);
#endif // endif
				bfe_sts = bfi->bfe_sts_cap_bw160;
				bfe_nrx = bfi->bfe_nrx_bw160 - 1;
			} else {
#ifdef WL11AX
				bfr_sts = wlc_he_get_bfr_ndp_sts(wlc->hei, FALSE);
#endif // endif
				bfe_sts = bfi->bfe_sts_cap;
				bfe_nrx = bfi->bfe_nrx - 1;
			}
			bfr_sts = MIN(bfr_ntx, bfr_sts);
		} else {
			bfe_nrx = bfi->bfe_nrx - 1;
			bfe_sts = bfi->bfe_sts_cap;
			bfr_sts = bfr_ntx;
		}

		bfrStats = ((bw -1) << C_LNK_BFRLBW_NBIT) | sb << C_LNK_BFRSB_NBIT;

		if ((bfi->flags & MU_BFE)) {
			nc_idx = MIN((uint8)WLC_BITSCNT(wlc->stf->txchain), (bfi->bfe_nrx)) - 1;
		} else if (bfi->flags & HE_MU_BFE) {
			nc_idx = MIN(bfr_sts, bfe_nrx);
		}
		bfrcfg0 |= (nc_idx << C_LNK_BFRNC_NBIT);

		ndps = MIN(bfr_sts, bfe_sts);
		bfrcfg0 |= (ndps << C_LNK_BFRNDPS_NBIT);

		bfrcfg0 |= (1 << C_LNK_BFIVLD_NBIT);

		WL_TXBF(("wl%d %s link bw %d my: bfr_sts %d peer: bfe_nrx %d bfe_sts %d result: "
			"nc_idx %d ndps %d flags:0x%x %x\n", pub->unit, __FUNCTION__,
			bw, bfr_sts, bfe_nrx, bfe_sts, nc_idx, ndps, bfi->flags, bfi->vht_cap));

	}
#endif /* WL_PSMX */

	bfi->BFIConfig0 = bfrcfg0;
	wlc_txbf_tbcap_update(wlc->txbf, scb);
	bfe_nr = getbits((uint8*)&bfi->BFIConfig1, sizeof(bfi->BFIConfig1), C_LNK_BFENR_NBIT,
		NBITSZ(C_LNK_BFENR));
	bfi->BFIConfig1 = bfrcfg1;
	bfi->BFIConfig1 |= bfe_nr << C_LNK_BFENR_NBIT;
	bfi->BFRStat0 = bfrStats;

#if defined(BCMDBG) || defined(WLTEST)
	if (WL_TXBF_ON()) {
		WL_TXBF(("wl%d: %s bficonfig0 0x%04x bficonfig1 0x%04x bfrStats0 0x%04x\n",
			pub->unit, __FUNCTION__, bfrcfg0, bfrcfg1, bfrStats));
	}
#endif // endif

	return;
}

static void
wlc_txbf_bfr_init(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc;
	scb_t *scb = bfi->scb;
	uint16	bfi_blk, bfrctl = 0, bfr_config0, ndpa_type, sta_info;
	bool isVHT = FALSE;
	uint8 nc_idx = 0;
	uint8 nsts_shift;
	uint16 corerev;

	ASSERT(scb);

	wlc = txbf->wlc;
	corerev = wlc->pub->corerev;
	ASSERT(wlc);

	BCM_REFERENCE(sta_info);
	BCM_REFERENCE(nc_idx);

	if (D11REV_GE(corerev, 128)) {
		wlc_txbf_bfr_init_ge128(txbf, bfi);
		return;
	}

	if (SCB_VHT_CAP(scb)) {
		isVHT = TRUE;
	}
	/* bfr_config0: compressed format only for 11n */
	bfr_config0 = (TXBF_BFR_CONFIG0 | (isVHT << (TXBF_BFR_CONFIG0_FRAME_TYPE_SHIFT)));

	if (bfi->flags & MU_BFE) {
		/* Update rate config for MU STAs */
		uint8 bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
		if (CHSPEC_IS80(wlc->chanspec)) {
			if (bw == BW_20MHZ) {
				bfr_config0 |= (((CHSPEC_CTL_SB(wlc->chanspec)
					>> WL_CHANSPEC_CTL_SB_SHIFT) & 0x3) << 10);
			} else if (bw == BW_40MHZ) {
				bfr_config0 |= (0x1 << 13);
				bfr_config0 |= (((CHSPEC_CTL_SB(wlc->chanspec)
					>> (WL_CHANSPEC_CTL_SB_SHIFT+1)) & 0x1) << 10);
			} else if (bw == BW_80MHZ) {
				bfr_config0 |= (0x2 << 13);
			}
		} else if (CHSPEC_IS40(wlc->chanspec)) {
			if (bw == BW_20MHZ) {
				bfr_config0 |= (((CHSPEC_CTL_SB(wlc->chanspec)
					>> WL_CHANSPEC_CTL_SB_SHIFT) & 0x1) << 10);
			} else if (bw == BW_40MHZ) {
				bfr_config0 |= (0x1 << 13);
			}
		}
		wlc_svmp_update_ratecap(wlc, scb, bfi->mx_bfiblk_idx);
	} else {
		if (CHSPEC_IS40(wlc->chanspec)) {
			bfr_config0 |= (0x1 << 13);
		} else if  (CHSPEC_IS80(wlc->chanspec)) {
			bfr_config0 |= (0x2 << 13);
		}
	}

	nsts_shift = C_BFI_BFRCTL_POS_NSTS_SHIFT;
	/* NDP streams and VHT/HT */
	if (WLC_BITSCNT(wlc->stf->txchain) == 4) {
		if (bfi->bfe_sts_cap == 3) {
			/* 4 streams */
			bfrctl |= (2 << nsts_shift);
		}  else if (bfi->bfe_sts_cap == 2) {
			/* 3 streams */
			bfrctl |= (1 << nsts_shift);
		}
	} else if (WLC_BITSCNT(wlc->stf->txchain) == 3) {
		if ((bfi->bfe_sts_cap == 3) || (bfi->bfe_sts_cap == 2)) {
			/* 3 streams */
			bfrctl |= (1 << nsts_shift);
		}
	}

	if (isVHT) {
		ndpa_type = BF_NDPA_TYPE_VHT;
		bfrctl |= (1 << C_BFI_BFRCTL_POS_NDP_TYPE_SHIFT);
	} else {
		ndpa_type = BF_NDPA_TYPE_CWRTS;
	}

	/* Used only for corerev < 64 */
	if (D11REV_LT(wlc->pub->corerev, 64) &&
		(scb->flags & SCB_BRCM)) {
		bfrctl |= (1 << C_BFI_BFRCTL_POS_MLBF_SHIFT);
	}

	WL_TXBF(("wl%d: %s a %s-link: sta:"MACF" BSSIDX:%d\n",
		wlc->pub->unit, __FUNCTION__, isVHT ? "VHT" : "HT",
		ETHER_TO_MACF(scb->ea), WLC_BSSCFG_IDX(scb->bsscfg)));
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub) && (bfi->flags & MU_BFE)) {
		int bss_mac5 = (scb->bsscfg->cur_etheraddr.octet[5]) & 0x3f;
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
		bfi_blk = txbf->mx_bfiblk_base +
			bfi->mx_bfiblk_idx * BFI_BLK_SIZE(wlc->pub->corerev);
		wlc_txbf_invalidate_bfridx(txbf, bfi, bfi->mx_bfiblk_idx);
		/* Secondary (non-default) BSS */
		if (scb->bsscfg != wlc_bsscfg_primary(wlc)) {
			ndpa_type  |= (1 << C_BFI_MUMBSS_NBIT);
		}
		bss_mac5 = bss_mac5 << C_BFI_BSSID_NBIT;
		ndpa_type |= bss_mac5;

		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_NDPA_FCTST_POS), ndpa_type);
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_BFRCTL_POS), bfrctl);
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_BFR_CONFIG0_POS), bfr_config0);
		/* Beamformee's mac addr bytes. Used by MU BFR. */
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_STA_ADDR_POS),
			((scb->ea.octet[1] << 8) | scb->ea.octet[0]));
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_STA_ADDR_POS + 1),
			((scb->ea.octet[3] << 8) | scb->ea.octet[2]));
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_STA_ADDR_POS + 2),
			((scb->ea.octet[5] << 8) | scb->ea.octet[4]));
#ifdef AP
		/* Update AID, FeedBack Type 1:MU, NC IDX */
		sta_info = scb->aid | 1 << C_STAINFO_FBT_NBIT;
		nc_idx = MIN((uint8)WLC_BITSCNT(wlc->stf->txchain), (bfi->bfe_nrx)) - 1;
		sta_info |= (nc_idx << C_STAINFO_NCIDX_NBIT);
		wlc_write_shmx(wlc, shm_addr(bfi_blk, C_BFI_STAINFO_POS), sta_info);
#endif // endif
		if (!SCB_PS(scb)) {
			wlc_txbf_bfridx_set_en_bit(txbf, bfi, TRUE);
		} else {
			/* This SCB is in PM, so enable sounding later by wlc_txbf_scb_ps_notify()
			 * when the SCB comes out of PM.
			 */
			WL_TXBF(("wl%d: %s SCB is in PM. Defer setting BFI_BFRIDX_EN\n",
				wlc->pub->unit, __FUNCTION__));
		}
#if defined(BCMDBG) || defined(WLTEST)
		if (WL_TXBF_ON()) {
			bcm_bprintf_bypass = TRUE;
			wlc_txbf_dump_shmx_bfi_blk(txbf, NULL, bfi->mx_bfiblk_idx);
			wlc_txbf_dump_shm_bfi_blk(txbf, NULL, bfi->mx_bfiblk_idx);
			bcm_bprintf_bypass = FALSE;
		}
#endif // endif
		wlc_bmac_enable_macx(wlc->hw);
	} else
#endif /* WL_PSMX */
	{
		wlc_suspend_mac_and_wait(wlc);
		bfi_blk = txbf->shm_base + bfi->shm_index * BFI_BLK_SIZE(wlc->pub->corerev);
		wlc_txbf_invalidate_bfridx(txbf, bfi, bfi->shm_index);
		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_NDPA_FCTST_POS), ndpa_type);
		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BFRCTL_POS), bfrctl);
		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BFR_CONFIG0_POS), bfr_config0);
#ifdef AP
		/* Update AID */
		sta_info = scb->aid;
		wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_STAINFO_POS), sta_info);
#endif // endif
#if defined(BCMDBG) || defined(WLTEST)
		if (WL_TXBF_ON()) {
			wlc_txbf_dump_shm_bfi_blk(txbf, NULL, bfi->shm_index);
		}
#endif // endif
		wlc_enable_mac(wlc);
	}

	return;
}

void
wlc_txbf_scb_state_upd(wlc_txbf_info_t *txbf, scb_t *scb, uint8* cap_ptr, uint cap_len, int8 cap_tp)
{
	wlc_bsscfg_t *bsscfg = NULL;
	wlc_info_t *wlc;
	txbf_scb_info_t *bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

	wlc = txbf->wlc;
	BCM_REFERENCE(wlc);
	bsscfg = scb->bsscfg;

	BCM_REFERENCE(bsscfg);

	ASSERT(bfi);
	if (!bfi) {
		WL_ERROR(("wl:%d %s failed\n", txbf->wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (SCB_HE_CAP(scb) && TXBF_CAP_IS_HE(cap_tp)) {
		bcopy(cap_ptr, (uint8 *) &bfi->he_cap, MIN(sizeof(bfi->he_cap), cap_len));
		if (!(bfi->bfe_sts_cap) && (D11REV_GE(wlc->pub->corerev, 128))) {
			bfi->bfe_sts_cap = getbits((uint8 *)&bfi->he_cap, sizeof(bfi->he_cap),
				HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_IDX,
				HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_FSZ);
		}
		if (!(bfi->bfe_sts_cap_bw160) && (D11REV_GE(wlc->pub->corerev, 128))) {
			bfi->bfe_sts_cap_bw160 = getbits((uint8 *)&bfi->he_cap, sizeof(bfi->he_cap),
				HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_IDX,
				HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_FSZ);
		}
	} else if (SCB_VHT_CAP(scb) && TXBF_CAP_IS_VHT(cap_tp)) {
		bcopy(cap_ptr, (uint8 *) &bfi->vht_cap, MIN(sizeof(bfi->vht_cap), cap_len));
		if (!SCB_HE_CAP(scb) && !(bfi->bfe_sts_cap) &&
			(D11REV_GE(wlc->pub->corerev, 128))) {
			bfi->bfe_sts_cap = ((bfi->vht_cap &
			  VHT_CAP_INFO_NUM_BMFMR_ANT_MASK) >> VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT);
		}
	} else if (SCB_HT_CAP(scb) && TXBF_CAP_IS_HT(cap_tp))
		bcopy(cap_ptr, (uint8 *) &bfi->ht_txbf_cap, MIN(sizeof(bfi->ht_txbf_cap), cap_len));

	if (BSSCFG_IBSS(bsscfg) ||
		BSSCFG_INFRA_STA(bsscfg) ||
#ifdef WLTDLS
	BSSCFG_IS_TDLS(bsscfg)) {
#else
	FALSE) {
#endif // endif
		if (!(txbf->bfr_capable || txbf->bfe_capable)) {
			return;
		}
#ifdef WL11AX
		if (SCB_HE_CAP(scb)) {
			uint16 he_flags;
			he_flags = wlc_he_get_scb_flags(wlc->hei, scb);
			if (he_flags && bfi->init_pending) {
				wlc_txbf_init_link(txbf, scb);
			}
		} else
#endif /* WL11AX */
#ifdef WL11AC
		if (SCB_VHT_CAP(scb)) {
			uint16 vht_flags;
			vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
			if ((vht_flags & (SCB_SU_BEAMFORMEE | SCB_MU_BEAMFORMEE |
				SCB_SU_BEAMFORMER | SCB_MU_BEAMFORMER)) &&
				bfi->init_pending) {
				wlc_txbf_init_link(txbf, scb);
			}
		} else
#endif /* WL11AC */
		if (SCB_HT_CAP(scb) &&
			bfi->init_pending &&
			(scb->flags3 & (SCB3_HT_BEAMFORMEE | SCB3_HT_BEAMFORMER)))
			wlc_txbf_init_link(txbf, scb);
	}

}

static void
wlc_txbf_scb_state_upd_cb(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)ctx;
	scb_t *scb;
	uint8 oldstate;

	ASSERT(notif_data != NULL);

	scb = notif_data->scb;
	ASSERT(scb != NULL);
	oldstate = notif_data->oldstate;

	/* when transitioning to ASSOCIATED state
	* intitialize txbf link for this SCB.
	*/
	if ((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb))
		wlc_txbf_delete_link(txbf, scb);
	else if (!(oldstate & ASSOCIATED) && SCB_ASSOCIATED(scb))
		wlc_txbf_init_link(txbf, scb);
}

void wlc_txbf_applied2ovr_upd(wlc_txbf_info_t *txbf, bool bfen)
{
	txbf->applied2ovr = bfen;
}

uint8 wlc_txbf_get_applied2ovr(wlc_txbf_info_t *txbf)
{
	return txbf->applied2ovr;
}

void
wlc_txbf_upd(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;
	int txchains;

	if (!txbf)
		return;

	wlc = txbf->wlc;
	ASSERT(wlc);

	txchains = WLC_BITSCNT(txbf->wlc->stf->txchain);

	if ((txbf->mode ||
		txbf->imp) &&
		(txchains > 1) &&
		(wlc->stf->allow_txbf == TRUE))
		wlc->allow_txbf = TRUE;
	else
		wlc->allow_txbf = FALSE;
}

void wlc_txbf_rxchain_upd(wlc_txbf_info_t *txbf)
{
	int rxchains;
	wlc_info_t *wlc;

	ASSERT(txbf);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (!txbf->mode) {
		return;
	}

	if (!wlc->clk) {
		return;
	}

	rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	wlc_write_shm(wlc, M_BFI_NRXC(wlc), rxchains - 1);

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry_all(wlc, NULL, FALSE,
			TRUE /* clr_txbf_stats=1 in mreq */);
	}
}

/* update txbf rateset based on number active txchains for corerev >= 64 */
static void  wlc_txbf_rateset_upd(wlc_txbf_info_t *txbf)
{
	int i, idx;
	wlc_info_t * wlc;
	const wl_txbf_rateset_t *rs;

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (WLC_BITSCNT(wlc->stf->txchain) == 1)
		return;

	idx = 0;
	if (WLC_BITSCNT(wlc->stf->txchain) == 4) {
		idx = 2;
	} else if (WLC_BITSCNT(wlc->stf->txchain) == 3) {
		idx = 1;
	}

	if (D11REV_GE(wlc->pub->corerev, 128))
		rs = rs_128;
	else
		rs = rs_ge64;

	/* copy mcs rateset */
	bcopy(rs[idx].txbf_rate_mcs, txbf->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
	bcopy(rs[idx].txbf_rate_mcs_bcm, txbf->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
	/* copy vht rateset */
	for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
		txbf->txbf_rate_vht[i] = rs[idx].txbf_rate_vht[i];
		txbf->txbf_rate_vht_bcm[i] = rs[idx].txbf_rate_vht_bcm[i];
	}

	/* copy ofdm rateset */
	txbf->txbf_rate_ofdm_cnt = rs[idx].txbf_rate_ofdm_cnt;
	bcopy(rs[idx].txbf_rate_ofdm, txbf->txbf_rate_ofdm, rs[idx].txbf_rate_ofdm_cnt);
	txbf->txbf_rate_ofdm_cnt_bcm = rs[idx].txbf_rate_ofdm_cnt_bcm;
	bcopy(rs[idx].txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_bcm,
		rs[idx].txbf_rate_ofdm_cnt_bcm);
}

/* Clear valid bit<8> in Phy cache index to trigger new sounding sequence */
static bool
wlc_txbf_invalidate_bfridx(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi, uint16 bfridx)
{
	wlc_info_t *wlc;
	uint16 val, addr, bfi_blk, offset;
	uint16 corerev;
	bool en = 0;
	uint8 en_nbit, vld_nbit;

	wlc = txbf->wlc;
	if (!wlc->clk) {
		return en;
	}

	corerev = wlc->pub->corerev;
	if (D11REV_GE(corerev, 128)) {
		return TRUE;
	}

	offset = C_BFI_BFRIDX_POS;
	vld_nbit = C_BFRIDX_VLD_NBIT;
	en_nbit = C_BFRIDX_EN_NBIT;
	bfi_blk = txbf->mx_bfiblk_base + bfridx * BFI_BLK_SIZE(corerev);
	BCM_REFERENCE(en_nbit);
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub) &&
		(bfi->flags & MU_BFE || bfi->flags & HE_MU_BFE)) {
		addr = shm_addr(bfi_blk, offset);
		val = wlc_read_shmx(wlc, addr);
		en = isset(&val, en_nbit);
		val &= (~(1 << vld_nbit| 1 << en_nbit));
		wlc_write_shmx(wlc, addr, val);
	} else
#endif /* WL_PSMX */
	{
		bfi_blk = txbf->shm_base + bfridx * BFI_BLK_SIZE(corerev);
		addr = shm_addr(bfi_blk, offset);
		val = wlc_read_shm(wlc, addr);
		clrbit(&val, vld_nbit);
		wlc_write_shm(wlc, addr, val);
	}
	return en;
}

#ifdef WL_PSMX
/* BFI block is enabled (has valid info) */
static void
wlc_txbf_bfridx_set_en_bit(wlc_txbf_info_t *txbf, txbf_scb_info_t *bfi, bool set)
{
	wlc_info_t *wlc;
	uint16 prev_val, val, addr, bfi_blk;
	uint16 corerev, bfr_idx, offset;
	uint8 en_nbit;

	wlc = txbf->wlc;

	if (!wlc->clk) {
		return;
	}

	if (!PSMX_ENAB(wlc->pub)) {
		return;
	}

	BCM_REFERENCE(prev_val);
	corerev = wlc->pub->corerev;

	/* Ucode handles the PM operation for sounding */
	if (D11REV_GE(corerev, 128)) {
		return;
	}

	bfr_idx = bfi->mx_bfiblk_idx;
	offset = C_BFI_BFRIDX_POS;
	en_nbit = C_BFRIDX_EN_NBIT;
	bfi_blk = txbf->mx_bfiblk_base + bfr_idx * BFI_BLK_SIZE(corerev);
	addr = shm_addr(bfi_blk, offset);
	prev_val = val = wlc_read_shmx(wlc, addr);
	if (set) {
		setbit(&val, en_nbit);
	} else {
		/* No attempt to clear is expected when it is already cleared */
		ASSERT((val & (1 << en_nbit)));
		clrbit(&val, en_nbit);
	}
	wlc_write_shmx(wlc, addr, val);

	WL_TXBF(("wl%d: %s:"MACF" bfr_idx:%d addr:%04x val:0x%04x prev:0x%04x\n", wlc->pub->unit,
		__FUNCTION__, ETHER_TO_MACF(bfi->scb->ea), bfr_idx, addr, val, prev_val));
}

void wlc_txbf_scb_ps_notify(wlc_txbf_info_t *txbf, scb_t *scb, bool ps_on)
{
	txbf_scb_info_t *bfi;
	bfi = TXBF_SCB_INFO(txbf, scb);

	if (bfi == NULL) {
		/* TXBF SCB cubby is deinited */
		return;
	}

	if (PSMX_ENAB(txbf->pub) && ((bfi->flags & MU_BFE) || (bfi->flags & MU_BFE))) {
		/* Disable/Enable MU sounding to the BFE when PS mode changes */
		wlc_txbf_bfridx_set_en_bit(txbf, bfi, (!ps_on));
	}

	WL_PS(("wl%d.%d: %s "MACF" ps:%s\n", txbf->wlc->pub->unit, WLC_BSSCFG_IDX(SCB_BSSCFG(scb)),
		__FUNCTION__, ETHER_TO_MACF(scb->ea), ps_on ? "ON":"OFF"));
}
#endif /* WL_PSMX */

void wlc_txbf_txchain_upd(wlc_txbf_info_t *txbf)
{
	int txchains = WLC_BITSCNT(txbf->wlc->stf->txchain);
	uint32 i;
	scb_t *scb;
	txbf_scb_info_t *bfi;
	scb_iter_t scbiter;
	wlc_info_t * wlc;
	uint16 coremask0 = 0, coremask1 = 0, mask0;
	wlc = txbf->wlc;
	ASSERT(wlc);

	wlc_txbf_upd(txbf);
	if (D11REV_GE(wlc->pub->corerev, 65)) {
		wlc_txbf_rateset_upd(txbf);
	}

	if (!wlc->clk)
		return;

	if (!txbf->mode && !txbf->imp)
		return;

	if (txchains < 2) {
		txbf->active = FALSE;
		WL_TXBF(("wl%d: %s beamforming deactived: ntxchains < 2!\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* ucode picks up the coremask from new shmem for all types of frames if bfm is
	 * set in txphyctl
	 */
	/* ?? not final but use this for now, need to find a good choice of two/three out of
	 * # txchains
	*/
	/* M_COREMASK_BFM: byte-0: for implicit, byte-1: for explicit nss-2 */
	/* M_COREMASK_BFM1: byte-0: for explicit nss-3, byte-1: for explicit nss-4 */

	if (D11REV_LT(wlc->pub->corerev, 64) || txbf->bfr_spexp == 0) {
		if (txchains == 2) {
			coremask0 = wlc->stf->txchain << 8;
			coremask1 = (wlc->stf->txchain << 8) | wlc->stf->txchain;
		} else if (txchains == 3) {
			mask0 = 0x8;
			if ((wlc->stf->txchain & mask0) == 0) {
				mask0 = 0x4;
			}
			coremask0 = (wlc->stf->txchain & ~mask0) << 8;
			coremask1 = (wlc->stf->txchain << 8) | wlc->stf->txchain;
		} else if (txchains >= 4) {
			coremask0 = 0x300;
			coremask1 = 0x7;
			coremask1 |= (wlc->stf->txchain << 8);
		}
	} else {
		coremask0 = wlc->stf->txchain << 8;
		coremask1 = (wlc->stf->txchain << 8) | wlc->stf->txchain;
	}

	coremask0 |=  wlc->stf->txchain;
	wlc_write_shm(wlc, M_COREMASK_BFM(wlc), coremask0);
	wlc_write_shm(wlc, M_COREMASK_BFM1(wlc), coremask1);

	if ((!txbf->active) && (txbf->shm_idx_bmp)) {
		txbf->active = TRUE;
		WL_TXBF(("wl%d: %s beamforming reactived! txchain %#x\n", wlc->pub->unit,
			__FUNCTION__, wlc->stf->txchain));
	}

	if (!wlc->pub->up)
		return;

	wlc_suspend_mac_and_wait(wlc);
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			uint8 nc_idx = 0, ndps, bw;
			uint8 bfe_nrx = 0, bfe_sts = 0, bfr_sts = 0, bfr_ntx = 0;
			bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
			if (!bfi) continue;

			bfr_ntx = (uint8) WLC_BITSCNT(wlc->stf->txchain) - 1;

			bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
			if ((bfi->flags & HE_MU_BFE)) {
				if (bw == BW_160MHZ) {
#ifdef WL11AX
					bfr_sts = wlc_he_get_bfr_ndp_sts(wlc->hei, TRUE);
#endif // endif
					bfe_sts = bfi->bfe_sts_cap_bw160;
					bfe_nrx = bfi->bfe_nrx_bw160 - 1;
				} else {
#ifdef WL11AX
					bfr_sts = wlc_he_get_bfr_ndp_sts(wlc->hei, FALSE);
#endif // endif
					bfe_sts = bfi->bfe_sts_cap;
					bfe_nrx = bfi->bfe_nrx - 1;
				}
				bfr_sts = MIN(bfr_ntx, bfr_sts);
			} else {
				bfe_nrx = bfi->bfe_nrx - 1;
				bfe_sts = bfi->bfe_sts_cap;
				bfr_sts = bfr_ntx;
			}

			if ((bfi->flags & MU_BFE)) {
				nc_idx = MIN((uint8)WLC_BITSCNT(wlc->stf->txchain),
						(bfi->bfe_nrx)) - 1;
			} else if (bfi->flags & HE_MU_BFE) {
				nc_idx = MIN(bfr_sts, bfe_nrx);
			}
			bfi->BFIConfig0 &= ~(C_LNK_BFRNC_BMSK | C_LNK_BFRNDPS_BMSK);
			bfi->BFIConfig0 |= (nc_idx << C_LNK_BFRNC_NBIT);

			ndps = MIN(bfr_sts, bfe_sts);
			bfi->BFIConfig0 |= (ndps << C_LNK_BFRNDPS_NBIT);
		}
	} else {
		if (txbf->shm_idx_bmp == 0)
			return;

		for (i = 0; i <= txbf->max_link; i++) {
			uint16 bfi_blk, bfrctl = 0;
			uint8 nsts_shift;

			if ((txbf->shm_idx_bmp & (1 << i)) == 0)
				continue;

			scb = txbf->su_scbs[i];
			bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

			bfi_blk = txbf->shm_base + i * BFI_BLK_SIZE(wlc->pub->corerev);

			nsts_shift = C_BFI_BFRCTL_POS_NSTS_SHIFT;
			/* NDP streams and VHT/HT */
			if (WLC_BITSCNT(wlc->stf->txchain) == 4) {
				if (bfi->bfe_sts_cap == 3) {
					/* 4 streams */
					bfrctl |= (2 << nsts_shift);
				}  else if (bfi->bfe_sts_cap == 2) {
					/* 3 streams */
					bfrctl |= (1 << nsts_shift);
				}
			} else if (WLC_BITSCNT(wlc->stf->txchain) == 3) {
				if ((bfi->bfe_sts_cap == 3) ||
					(bfi->bfe_sts_cap == 2)) {
					/* 3 streams */
					bfrctl |= (1 << nsts_shift);
				}
			}

			if (SCB_VHT_CAP(scb)) {
				bfrctl |= (1 << C_BFI_BFRCTL_POS_NDP_TYPE_SHIFT);
			}

			wlc_write_shm(wlc, shm_addr(bfi_blk, C_BFI_BFRCTL_POS), bfrctl);

			wlc_txbf_invalidate_bfridx(txbf, bfi, bfi->shm_index);
		}
	}

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry_all(wlc, NULL, FALSE,
			TRUE /* clr_txbf_stats=1 in mreq */);
	}

#ifdef WL_PSMX
	/* force evict all the txv's that are already assigned */
	wlc_macreq_deltxv(wlc);
#endif /* WL_PSMX */

	wlc_enable_mac(wlc);
	/* invalidate txcache since rates are changing */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);
}

void
wlc_txbf_sounding_clean_cache(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc = txbf->wlc;
	uint32 i;
	scb_t *scb;
	txbf_scb_info_t *bfi;
	bool en;

	BCM_REFERENCE(en);
	wlc_suspend_mac_and_wait(wlc);
	for (i = 0; i <= txbf->max_link; i++) {
		if (isclr(&txbf->shm_idx_bmp, i))
			continue;
		scb = txbf->su_scbs[i];
		if (!scb)
			continue;
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi)
			continue;
		en = wlc_txbf_invalidate_bfridx(txbf, bfi, i);
#ifdef WL_PSMX
		wlc_txbf_bfridx_set_en_bit(txbf, bfi, en);
#endif // endif
	}
	wlc_enable_mac(wlc);
#ifdef WL_PSMX
	if (!PSMX_ENAB(wlc->pub)) {
		return;
	}
	wlc_bmac_suspend_macx_and_wait(wlc->hw);
	for (i = 0; i < txbf->mu_max_links; i++) {
		if (isclr(&txbf->mx_bfiblk_idx_bmp, i))
				continue;
		scb = txbf->mu_scbs[i];
		if (!scb)
			continue;
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi)
			continue;
		en = wlc_txbf_invalidate_bfridx(txbf, bfi, i);
		wlc_txbf_bfridx_set_en_bit(txbf, bfi, en);
	}
	wlc_bmac_enable_macx(wlc->hw);
#endif /* WL_PSMX */
}

static bool wlc_txbf_check_ofdm_rate(uint8 rate, uint8 *supported_rates, uint8 num_of_rates)
{
	uint8 i;
	for (i = 0; i < num_of_rates; i++) {
		if (rate == (supported_rates[i] & RATE_MASK))
			return TRUE;
	}
	return FALSE;
}
void
wlc_txbf_txpower_target_max_upd(wlc_txbf_info_t *txbf, int8 max_txpwr_limit)
{
	if (!txbf)
		return;

	txbf->max_txpwr_limit = max_txpwr_limit;
}

/* Note: we haven't used bw and is_ldpc information yet */
static uint8 wlc_txbf_system_gain_acphy(uint8 bfr_ntx, uint8 bfe_nrx, uint8 std, uint8 mcs,
	uint8 nss, uint8 rate, uint8 bw, bool is_ldpc, bool is_imp)
{
	/* Input format
	 *   std:
	 *      0:  dot11ag
	 *      1:  11n
	 *      2:  11ac / he
	 *
	 *   rate:  phyrate of dot11ag format, e.g. 6/9/12/18/24/36/48/54 Mbps
	 *
	 *   bw:  20/40/80 MHz
	 *
	 * Output format:
	 *   expected TXBF system gain in 1/4dB step
	 */

	uint8 gain_3x1[12] = {0, 6, 16, 18, 18, 18, 18, 18, 18, 18, 0, 0};
	uint8 gain_3x2[24] = {0, 6, 14, 16, 16, 16, 16, 16, 16, 16, 0, 0,
	4, 4,  6,  6,  6,  6,  6,  6,  6,  6, 0, 0};
	uint8 gain_3x3[36] = {0, 6, 12, 14, 14, 14, 14, 14, 14, 14, 0, 0,
	4, 4,  6,  6,  6,  6,  6,  6,  6,  6, 0, 0,
	0, 2,  2,  6,  2,  6,  4,  0,  0,  0, 0, 0};
	uint8 gain_2x1[12] = {0, 6, 12, 12, 12, 12, 12, 12, 12, 12, 0, 0};
	uint8 gain_2x2[24] = {0, 6, 10, 10, 10, 10, 10, 10, 10, 10, 0, 0,
	0, 2,  2,  6,  2,  6,  4,  0,  0,  0, 0, 0};
	uint8 gain_imp_3x1[12] = {0, 4, 10, 12, 12, 12, 12, 12, 12, 12, 0, 0};
	uint8 gain_imp_3x2[12] = {0, 4,  4,  5,  5,  5,  5,  5,  5,  5, 0, 0};
	uint8 gain_imp_3x3[12] = {0, 0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0};
	uint8 gain_imp_2x1[12] = {0, 2,  4,  6,  8,  8,  8,  8,  8,  8, 0, 0};
	uint8 gain_imp_2x2[12] = {0, 0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0};
#ifdef WL_PSMX
	uint8 gain_4x4[48] = {0, 6, 12, 14, 14, 14, 14, 14, 14, 14, 14, 14,
	2, 6, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	2, 6, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5,
	4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0};
	uint8 gain_4x3[36] = {0, 6, 12, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	2, 6, 8, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	2, 6, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5};
	uint8 gain_4x2[24] = {0, 6, 16, 18, 18, 18, 18, 18, 18, 18, 18, 18,
	2, 6, 8, 11, 11, 11, 11, 11, 11, 11, 11, 11};
	uint8 gain_4x1[12] = {0, 8, 20, 22, 22, 22, 22, 22, 22, 22, 22, 22};

	uint8 gain_imp_4x4[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	uint8 gain_imp_4x3[12] = {0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
	uint8 gain_imp_4x2[12] = {0, 4, 4, 6, 6, 6, 6, 6, 6, 6, 6, 6};
	uint8 gain_imp_4x1[12] = {0, 8, 12, 16, 16, 16, 16, 16, 16, 16, 16, 16};

#endif /* WL_PSMX */
	uint8 *gain, idx = 0, expected_gain = 0;
#ifdef WL_PSMX
	gain = gain_4x4;
#else
	gain = gain_3x3;
#endif /* WL_PSMX */

	BCM_REFERENCE(bw);
	BCM_REFERENCE(is_ldpc);

	switch (std) {
	case 0: /* dot11ag */
		switch (rate) {
		case WLC_RATE_6M:
			idx = 0; break;
		case WLC_RATE_9M:
			idx = 0; break;
		case WLC_RATE_12M:
			idx = 1; break;
		case WLC_RATE_18M:
			idx = 2; break;
		case WLC_RATE_24M:
			idx = 3; break;
		case WLC_RATE_36M:
			idx = 4; break;
		case WLC_RATE_48M:
			idx = 5; break;
		case WLC_RATE_54M:
			idx = 6; break;
		}
		break;
	case 1: /* 11n */
		idx = (mcs % 8) + (mcs >> 3) * 12;
		break;
	case 2: /* 11ac / HE */
		idx = mcs + (nss - 1) * 12;
		break;
	}
#ifdef WL_PSMX
	if (bfr_ntx == 4 && bfe_nrx == 1) {
		gain = is_imp ? gain_imp_4x1 : gain_4x1;
	} else if (bfr_ntx == 4 && bfe_nrx == 2) {
		gain = is_imp ? gain_imp_4x2 : gain_4x2;
	} else if (bfr_ntx == 4 && bfe_nrx == 3) {
		gain = is_imp ? gain_imp_4x3 : gain_4x3;
	} else if (bfr_ntx == 4 && bfe_nrx == 4) {
		gain = is_imp ? gain_imp_4x4 : gain_4x4;
	}
	else
#endif // endif
	if (bfr_ntx == 3 && bfe_nrx == 1) {
		gain = is_imp ? gain_imp_3x1 : gain_3x1;
	} else if (bfr_ntx == 3 && bfe_nrx == 2) {
		gain = is_imp ? gain_imp_3x2 : gain_3x2;
	} else if (bfr_ntx == 3 && bfe_nrx == 3) {
		gain = is_imp ? gain_imp_3x3 : gain_3x3;
	} else if (bfr_ntx == 2 && bfe_nrx == 1) {
		gain = is_imp ? gain_imp_2x1 : gain_2x1;
	} else if (bfr_ntx == 2 && bfe_nrx == 2) {
		gain = is_imp ? gain_imp_2x2 : gain_2x2;
	} else 	{
		gain = is_imp ? gain_imp_3x3 : gain_3x3;
	}

	expected_gain = gain[idx];

	return expected_gain;
}

/* Txbf on/off based on CLM and expected TXBF system gain:
 * In some locales and some channels, TXBF on provides a worse performance than TXBF off.
 * This is mainly because the CLM power of TXBF on is much lower than TXBF off rates, and
 * offset the TXBF system gain leading to a negative net gain.
 * wlc_txbf_system_gain_acphy() function provides the expected TXBF system gain.
 * wlc_check_expected_txbf_system_gain() uses the above function and compare with the CLM power
 * back off of TXBF rates vs  non-TXBF rates, and make a decision to turn on/off TXBF for each
 * rate.
 */
bool wlc_txbf_bfen(wlc_txbf_info_t *txbf, scb_t *scb,
	ratespec_t rspec, txpwr204080_t* txpwrs, bool is_imp)
{
	wlc_info_t *wlc;
	uint8 ntx_txbf,  ntx_steer, bfe_nrx, std,  mcs,  nss_txbf,  rate,  bw, ntx;
	int8 pwr, pwr_txbf, gain, gain_comp, txbf_target_pwr;
	bool is_ldpc = FALSE;
	txbf_scb_info_t *bfi;
	ratespec_t txbf_rspec;
	bool rate_override;
	wlc = txbf->wlc;
	bfi = TXBF_SCB_INFO(txbf, scb);
	rate_override = RSPEC_ACTIVE(wlc->band->rspec_override);

	ASSERT(!(rspec & WL_RSPEC_TXBF));

	txbf_rspec = rspec;
	txbf_rspec &= ~(WL_RSPEC_TXEXP_MASK | WL_RSPEC_STBC);
	txbf_rspec |= WL_RSPEC_TXBF;

	if (is_imp || (D11REV_GE(wlc->pub->corerev, 128) && (rate_override))) {
		/* Ignore the spatial_expension policy if beamforming is enabled,
		 * and always use all currently enabled txcores
		 */
		ntx_steer = (uint8)WLC_BITSCNT(wlc->stf->txchain);
	} else {
		/* Number of txbf steering chains is MIN(#active txchains, #bfe sts + 1) */
		ntx_steer = MIN((uint8)WLC_BITSCNT(wlc->stf->txchain), (bfi->bfe_sts_cap + 1));
	}

	if (D11REV_LT(wlc->pub->corerev, 64) || txbf->bfr_spexp == 0) {
		ntx_txbf = ntx_steer;
	} else {
		ntx_txbf = (uint8)WLC_BITSCNT(wlc->stf->txchain);
	}

	/* fill TXEXP bits to indicate TXBF0,1 or 2 */
	txbf_rspec |= ((ntx_txbf - wlc_ratespec_nss(txbf_rspec)) << WL_RSPEC_TXEXP_SHIFT);
	nss_txbf = (uint8) wlc_ratespec_nss(txbf_rspec);

	/* bfe_sts_cap advertised incorrectly, turn off txbf */
	if (nss_txbf > ntx_txbf) {
		return FALSE;
	}

	bw = RSPEC_IS160MHZ(rspec) ? BW_160MHZ : (RSPEC_IS80MHZ(rspec) ? BW_80MHZ : (
		RSPEC_IS40MHZ(rspec) ?  BW_40MHZ : BW_20MHZ));

	/* bfe number of rx antenna override set? */
	if (txbf->bfe_nrx_ov) {
		bfe_nrx = txbf->bfe_nrx_ov;
	} else {
		if (D11REV_GE(wlc->pub->corerev, 128) && rate_override) {
			bfe_nrx = nss_txbf;
		} else if (SCB_HE_CAP(scb) && (bw == BW_160MHZ)) {
			bfe_nrx = bfi->bfe_nrx_bw160;
		} else {
			/* Legacy */
			bfe_nrx = bfi->bfe_nrx;
		}
	}

	if (ntx_steer < 2) {
		return FALSE;
	}

	/* Disable txbf for ntx_bfr = nrx_bfe = nss = 2 or 3 */
	if ((nss_txbf == 2 && ntx_steer == 2 && bfe_nrx == 2) ||
		(nss_txbf == 3 && ntx_steer == 3 && bfe_nrx == 3)) {
		return FALSE;
	}

	ntx = wlc_stf_txchain_get(wlc, rspec);

	is_ldpc = RSPEC_ISLDPC(txbf_rspec);
	/* retrieve power offset on per packet basis */
	pwr = txpwrs->pbw[bw - BW_20MHZ][TXBF_OFF_IDX];
	/* Sign Extend */
	pwr <<= 2;
	pwr >>= 2;
	/* convert power offset from 0.5 dB to 0.25 dB */
	pwr = (pwr * WLC_TXPWR_DB_FACTOR) / 2;

	/* gain compensation code */
	if ((ntx == 1) && (ntx_txbf == 2))
		gain_comp = 12;
	else if ((ntx == 1) && (ntx_txbf == 3))
		gain_comp = 19;
	else if ((ntx == 1) && (ntx_txbf == 4))
		gain_comp = 24;
	else if ((ntx == 2) && (ntx_txbf == 3))
		gain_comp = 7;
	else if ((ntx == 2) && (ntx_txbf == 4))
		gain_comp = 12;
	else if ((ntx == 3) && (ntx_txbf == 4))
		gain_comp = 5;
	else
		gain_comp = 0;

	pwr += gain_comp;

	if (RSPEC_ISHE(txbf_rspec)) {
		mcs = txbf_rspec & WL_RSPEC_HE_MCS_MASK;
		rate = 0;
		std = 2; /* HE */
	} else if (RSPEC_ISVHT(txbf_rspec)) {
		mcs = txbf_rspec & WL_RSPEC_VHT_MCS_MASK;
		rate = 0;
	        std = 2; /* 11AC */
	} else if (RSPEC_ISHT(txbf_rspec)) {
		mcs = txbf_rspec & WL_RSPEC_HT_MCS_MASK;
		rate = 0;
		std = 1; /* 11N */
	} else {
		ASSERT(RSPEC_ISLEGACY(txbf_rspec));
		rate = (uint8)RSPEC2RATE(txbf_rspec);
		mcs = 0;
		std = 0; /* Legacy */
	}

	gain = wlc_txbf_system_gain_acphy(ntx_steer, bfe_nrx, std, mcs, nss_txbf,
		rate, bw, is_ldpc, is_imp);

	/* retrieve txbf power offset on per packet basis */
	pwr_txbf = txpwrs->pbw[bw-BW_20MHZ][TXBF_ON_IDX];
	/* Sign Extend */
	pwr_txbf <<= 2;
	pwr_txbf >>= 2;
	/* convert power offset from 0.5 dB to 0.25 dB */
	pwr_txbf = (pwr_txbf * WLC_TXPWR_DB_FACTOR) / 2;

	txbf_target_pwr =  txbf->max_txpwr_limit - pwr_txbf;

	/* not use WL_TXBF to avoid per-packet chatty message */
	WL_INFORM(("%s: ntx %u ntx_txbf %u ntx_steer %u bfe_nrx %u std %u mcs %u nss_txbf %u"
		"  rate %u bw %u is_ldpc %u\n", __FUNCTION__, ntx, ntx_txbf, ntx_steer,
		bfe_nrx, std, mcs, nss_txbf, rate, bw, is_ldpc));

	WL_INFORM(("%s: %s TxBF: txbf_target_pwr %d qdB pwr %d qdB pwr_txbf %d qdB gain %d qdB"
		" gain_comp %d qdB\n", __FUNCTION__, ((txbf_target_pwr > 4) &&
		((pwr - pwr_txbf + gain) >= 0) ? "Enable" : "Disable"), txbf_target_pwr,
		(pwr - gain_comp), pwr_txbf, gain, gain_comp));

	/* Enable TXBF when effective power is greater than TXBF-off */
	if ((pwr - pwr_txbf + gain) >= 0) {
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			return TRUE;
		} else if ((txbf_target_pwr > ((BCM94360_MINTXPOWER * WLC_TXPWR_DB_FACTOR) + 1))) {
			/* Turn off TXBF when target power <= 1.25dBm on pre-128 chips */
			return TRUE;
		}
	}
	return FALSE;
}

static bool
wlc_txbf_imp_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, scb_t *scb,
	txpwr204080_t* txpwrs)
{
	wlc_info_t *wlc = txbf->wlc;
	txbf_scb_info_t *bfi;
	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	if (bfi->imp_en == FALSE || RSPEC_ISCCK(rspec) || RSPEC_ISHE(rspec) ||
		(wlc_ratespec_nss(rspec) > 1) ||
#ifdef WLTXBF_2G_DISABLED
		(BAND_2G(wlc->band->bandtype)) ||
#endif /* WLTXBF_2G_DISABLED */
		/* Only 43602 can beamform a dot11ag(OFDM) frame */
		(RSPEC_ISOFDM(rspec) && !(IS_LEGACY_IMPBF_SUP(wlc->pub->corerev))))
			return FALSE;

	ASSERT(!(bfi->exp_en && ((bfi->flags & MU_BFE) || (bfi->flags & HE_MU_BFE))));

	/* wl txbf_imp 2: imp txbf is forced on for all rates */
	if (wlc->txbf->imp == TXBF_ON) {
		return TRUE;
	} else
		return wlc_txbf_bfen(txbf, scb, rspec, txpwrs, TRUE);
}

static bool
wlc_txbf_exp_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, scb_t *scb, uint8 *shm_index,
	txpwr204080_t* txpwrs)
{
	uint nss, rate = 0;
	int is_brcm_sta = 0;
	bool is_valid = FALSE;
	wlc_info_t *wlc = txbf->wlc;
	bool rate_override = RSPEC_ACTIVE(wlc->band->rspec_override);

	if (!rate_override) {
		is_brcm_sta = (scb->flags & SCB_BRCM);
	}

	if (!D11REV_GE(wlc->pub->corerev, 128) ||
		(D11REV_GE(wlc->pub->corerev, 128) && (!rate_override))) {
		txbf_scb_info_t *bfi = TXBF_SCB_INFO(txbf, scb);
		ASSERT(bfi);
		if (!bfi) {
			WL_ERROR(("wl:%d %s empty txbf_scb_info 0x%p\n",
					wlc->pub->unit, __FUNCTION__, bfi));
			*shm_index = BF_SHM_IDX_INV;
			return FALSE;
		}

		if (!bfi->exp_en) {
			*shm_index = BF_SHM_IDX_INV;
			return FALSE;
		}

		// skip if sounding type is ht
		if (!txbf->ht_enable && (D11REV_GE(wlc->pub->corerev, 128) &&
			!(bfi->BFIConfig0 & C_LNK_STY_BMSK))) {
			return FALSE;
		}

		*shm_index = wlc_txbf_get_mubfi_idx(txbf, scb);
		if (*shm_index == BF_SHM_IDX_INV) {
			*shm_index = bfi->shm_index;
		}
	}

	if (RSPEC_ISCCK(rspec) || (txbf->mode == TXBF_OFF)) {
		return FALSE;
	}

	if (RSPEC_ISOFDM(rspec)) {
		rate = RSPEC2RATE(rspec);

		if (is_brcm_sta) {
			if (txbf->txbf_rate_ofdm_cnt_bcm == TXBF_RATE_OFDM_ALL)
				is_valid = TRUE;
			else if (txbf->txbf_rate_ofdm_cnt_bcm)
				is_valid = wlc_txbf_check_ofdm_rate((uint8)rate,
					txbf->txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_cnt_bcm);
		} else {
			if (txbf->txbf_rate_ofdm_cnt == TXBF_RATE_OFDM_ALL)
				is_valid = TRUE;
			else if (txbf->txbf_rate_ofdm_cnt)
				is_valid = wlc_txbf_check_ofdm_rate((uint8)rate,
					txbf->txbf_rate_ofdm, txbf->txbf_rate_ofdm_cnt);
		}
	}

	nss = wlc_ratespec_nss(rspec);
	ASSERT(nss >= 1);
	nss -= 1;
	if (RSPEC_ISVHT(rspec) || RSPEC_ISHE(rspec)) {
		rate = (rspec & (RSPEC_ISVHT(rspec) ? WL_RSPEC_VHT_MCS_MASK :
			WL_RSPEC_HE_MCS_MASK));
		if (is_brcm_sta)
			 is_valid = (((1 << rate) & txbf->txbf_rate_vht_bcm[nss]) != 0);
		else
			is_valid =  (((1 << rate) & txbf->txbf_rate_vht[nss]) != 0);

	} else if (RSPEC_ISHT(rspec)) {
		rate = (rspec & WL_RSPEC_HT_MCS_MASK) - (nss) * 8;

		if (is_brcm_sta)
			is_valid = (((1 << rate) & txbf->txbf_rate_mcs_bcm[nss]) != 0);
		else
			is_valid = (((1 << rate) & txbf->txbf_rate_mcs[nss]) != 0);
	}

	if (!is_valid) {
		WL_INFORM(("wl%d: %s Rate not present in txbf rateset rate:%d nss:%d\n",
			wlc->pub->unit, __FUNCTION__, rate, nss));
		return FALSE;
	}

	if (txbf->mode == TXBF_ON) {
		return TRUE;
	} else {
		return wlc_txbf_bfen(txbf, scb, rspec, txpwrs, FALSE);
	}
}

uint8
wlc_txbf_get_bfi_idx(wlc_txbf_info_t *txbf, scb_t *scb)
{
	txbf_scb_info_t *bfi;

	if (SCB_INTERNAL(scb)) {
		return BF_SHM_IDX_INV;
	}

	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);
	return bfi->shm_index;
}

#if defined(WLTEST) || defined(WLPKTENG)
int8
wlc_txbf_get_mu_txvidx(wlc_info_t *wlc, scb_t *scb)
{
	int8 txv_idx;
	wlc_txbf_info_t *txbf;
	txbf_scb_info_t *bfi;
	uint16 max_vhtmu_usrs = wlc_txcfg_max_clients_get(wlc->txcfg, VHTMU);

	txv_idx = BCME_NOTFOUND;
	txbf = wlc->txbf;
	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	if (bfi->flags & MU_BFE) {
		txv_idx = bfi->mx_bfiblk_idx;
	} else if (bfi->flags & HE_MU_BFE) {
		txv_idx = max_vhtmu_usrs + bfi->mx_bfiblk_idx;
	}

	return txv_idx;
}
#endif // endif

#ifdef WL_PSMX
uint8
wlc_txbf_get_mubfi_idx(wlc_txbf_info_t *txbf, scb_t *scb)
{
	txbf_scb_info_t *bfi;

	if (SCB_INTERNAL(scb)) {
		return BF_SHM_IDX_INV;
	}

	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	return bfi->mx_bfiblk_idx;
}
#endif /* WL_PSMX */

bool
wlc_txbf_sel(wlc_txbf_info_t *txbf, ratespec_t rspec, scb_t *scb, uint8 *shm_index,
	txpwr204080_t* txpwrs)
{
	bool ret = FALSE;
	wlc_info_t *wlc = txbf->wlc;

	ASSERT(!(rspec & WL_RSPEC_TXBF));

	if ((!txbf->ht_enable) && (RSPEC_ISHT(rspec))) {
		return ret;
	}

	ret = wlc_txbf_exp_sel(txbf, rspec, scb, shm_index, txpwrs);

	if ((D11REV_GE(wlc->pub->corerev, 128)) &&
		(RSPEC_ACTIVE(wlc->band->rspec_override)) && ret) {
		return ret;
	}
	if (*shm_index != BF_SHM_IDX_INV && ret)
		return ret;
	else
		ret = wlc_txbf_imp_sel(txbf, rspec, scb, txpwrs);

	return ret;
}

void
wlc_txbf_imp_txstatus(wlc_txbf_info_t *txbf, scb_t *scb, tx_status_t *txs)
{
	wlc_info_t *wlc = txbf->wlc;
	txbf_scb_info_t *bfi;

	if (SCB_INTERNAL(scb)) {
		return;
	}

	bfi = TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	if (!bfi->imp_en || txbf->imp_nochk)
		return;

	if (txs->status.s5 & TX_STATUS40_IMPBF_LOW_MASK)
		return;

	bfi->imp_used ++;
	if (txs->status.s5 & TX_STATUS40_IMPBF_BAD_MASK) {
		bfi->imp_bad ++;
	}

	if (bfi->imp_used >= 200 && ((bfi->imp_bad * 3) > bfi->imp_used)) {
		/* if estimated bad percentage > 1/3, disable implicit */
		bfi->imp_en = FALSE;
		if (WLC_TXC_ENAB(wlc))
			wlc_txc_inv_all(wlc->txc);
		WL_TXBF(("%s: disable implicit txbf. used %d bad %d\n",
			__FUNCTION__, bfi->imp_used, bfi->imp_bad));
	}

	if (bfi->imp_used >= 200) {
		WL_INFORM(("%s stats: used %d bad %d\n",
			__FUNCTION__, bfi->imp_used, bfi->imp_bad));
		/* reset it to avoid stale info. some rate might survive. */
		bfi->imp_used = 0;
		bfi->imp_bad = 0;
	}
	return;
}

void
wlc_txbf_fix_rspec_plcp(wlc_txbf_info_t *txbf, ratespec_t *prspec, uint8 *plcp,
	wl_tx_chains_t txbf_chains)
{
	uint nsts;
	ratespec_t txbf0_rspec = *prspec;
	BCM_REFERENCE(txbf);

	*prspec &= ~(WL_RSPEC_TXEXP_MASK | WL_RSPEC_STBC);
	/* Enable TxBF in rspec */
	*prspec |= WL_RSPEC_TXBF;
	/* fill TXEXP bits to indicate TXBF0,1,2 or 3 */
	nsts = wlc_ratespec_nsts(*prspec);
	*prspec |= ((txbf_chains - nsts) << WL_RSPEC_TXEXP_SHIFT);

	if (!RSPEC_ISSTBC(txbf0_rspec))
		return;

	if (RSPEC_ISHE(*prspec)) {
		uint32 plcp0;
		uint16 plcp1;

		/* Fill plcp0 & plcp1 */
		plcp0 = (plcp[3] << 24) | (plcp[2] << 16) | (plcp[1] << 8) | plcp[0];
		plcp1 = (plcp[5] << 8) | plcp[4];

		/* clear bit 35 stbc coding */
		plcp1 &= ~HE_SIGA_STBC;

		/* update NSTS */
		plcp0 &= ~HE_SIGA_NSTS_MASK;
		plcp0 |= ((nsts - 1) << HE_SIGA_NSTS_SHIFT) &
			HE_SIGA_NSTS_MASK;

		/* put plcp0 & plcp1 back in plcp */
		plcp[0] = (uint8)(plcp0);
		plcp[1] = (uint8)(plcp0 >> 8);
		plcp[2] = (uint8)(plcp0 >> 16);
		plcp[3] = (uint8)(plcp0 >> 24);
		plcp[4] = (uint8)(plcp1);
		plcp[5] = (uint8)(plcp1 >> 8);
	} else if (RSPEC_ISVHT(*prspec)) {
		uint16 plcp0 = 0;
			/* put plcp0 in plcp */
		plcp0 = (plcp[1] << 8) | (plcp[0]);

		/* clear bit 3 stbc coding */
		plcp0 &= ~VHT_SIGA1_STBC;
		/* update NSTS */
		plcp0 &= ~ VHT_SIGA1_NSTS_SHIFT_MASK_USER0;
		plcp0 |= ((uint16)((nsts - 1)) << VHT_SIGA1_NSTS_SHIFT);

		/* put plcp0 in plcp */
		plcp[0] = (uint8)plcp0;
		plcp[1] = (uint8)(plcp0 >> 8);

	} else if (RSPEC_ISHT(*prspec))
		plcp[3] &= ~PLCP3_STC_MASK;

	return;
}
#if defined(BCMDBG) || defined(WLTEST) || defined(DUMP_TXBF)
static void
wlc_txbf_shmx_dump(wlc_txbf_info_t *txbf, bcmstrbuf_t *b)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	uint8 i;
	scb_t *scb;
	txbf_scb_info_t *bfi;
	wlc_info_t *wlc = txbf->wlc;

	if (!PSMX_ENAB(wlc->pub))
		return;

	bcm_bprintf(b, "%d links with MU-Beamforming enabled\n",
		bcm_bitcount((uint8 *)&txbf->mx_bfiblk_idx_bmp, sizeof(txbf->mx_bfiblk_idx_bmp)));

	for (i = 0; i < txbf->mu_max_links; i++) {
		bool valid;
		uint16 bfr_idx = i;
		if (isclr(&txbf->mx_bfiblk_idx_bmp, i))
			continue;
		scb = txbf->mu_scbs[i];
		ASSERT(scb);
		if (!scb)
			continue;
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

		valid = (wlc_read_shmx(wlc,
			shm_addr(txbf->mx_bfiblk_base,
			bfr_idx * BFI_BLK_SIZE(wlc->pub->corerev)))
			& (1 << C_BFRIDX_VLD_NBIT)) ? TRUE : FALSE;

		bcm_bprintf(b, "%d: %s(aid 0x%x)\n", (i + 1),
			bcm_ether_ntoa(&scb->ea, eabuf), scb->aid);
		bcm_bprintf(b, "\tVHT cap: 0x%x ", bfi->vht_cap);
		bcm_bprintf(b, "exp_en: %s \n\t", bfi->exp_en ? "true" : "false");
		bcm_bprintf(b, "bfi_idx: %d amt_idx: %d",
			bfi->mx_bfiblk_idx, bfi->amt_index);

		bcm_bprintf(b, "\n\tLast sounding successful: %s\n", valid ? "true" : "false");
		if (bfi->vht_cap & VHT_CAP_INFO_MU_BEAMFMR) {
			bcm_bprintf(b, "\tmu bfr cap, bfe link %s\n",
				(txbf->bfe_capable ? "ON" : "OFF"));
		}

		if ((bfi->vht_cap & VHT_CAP_INFO_MU_BEAMFMEE) ||
			(bfi->bfe_capable & TXBF_HE_SU_BFE_CAP)) {
			bcm_bprintf(b, "\tmu bfe cap, bfr link %s, nsts %d nrx %d\n",
				(txbf->bfr_capable ? "ON" : "OFF"), bfi->bfe_sts_cap,
				bfi->bfe_nrx-1);
		}

		wlc_txbf_dump_shmx_bfi_blk(txbf, b, bfr_idx);
	}
}

#define TXBF_DUMP_ARGV_MAX	64
#define TXBF_DUMP_FLAG_RAWDUMP	0x1
#define TXBF_DUMP_FLAG_MU	0x2
#define TXBF_DUMP_FLAG_SU	0x4
#define TXBF_DUMP_FLAG_PARSED	0x8
#define TXBF_DUMP_FLAG_CQI	0x10
#define TXBF_MAX_ENTRIES	256
#define TXBF_DUMP_FLAG_TXV	0x10
#define TXBF_DUMP_FLAG_BFI	0x20
#define TXBF_SUTXV_MAX	16
#define TXBF_MUTXV_MAX	32
static void
wlc_txbf_info_dump(wlc_txbf_info_t *txbf, bcmstrbuf_t *b)
{
	uint8 entry_bitmap[TXBF_MAX_ENTRIES/NBBY + 1] = { 0 };
	bool dump_all = TRUE; /* default to dump all */
	wlc_info_t *wlc = txbf->wlc;
	uint8 flags = (TXBF_DUMP_FLAG_SU | TXBF_DUMP_FLAG_MU | TXBF_DUMP_FLAG_CQI);
	int i, j, entries = 0, valid;
	txbf_scb_info_t *bfi;
	uint16 txvm0 = 0, bfidx = 0, val0, val1;
	uint16 txvidx, txvm1 = 0, txvm2 = 0;
	uint16 txvuse_su_bmp, txvfree_bmp;
	uint32 cqi_txvuse_bmp, txvuse_bmp, txvbmp_blk0, txvbmp_blk1;
	scb_t *scb;
	d11linkmem_entry_t lnk;
	char txvbmp_str[][32] = {"inuse", "active", "new", "rptrdy", "tmout", "ingrp", "m2v", "su",
				"del", "musndrem"};

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return;
	}

	if (wlc->dump_args) {
		int err = wlc_txbf_dump_parse_args(wlc, entry_bitmap, &dump_all, &flags);
		if (err != BCME_OK) {
			bcm_bprintf(b, "txbf dump optional params (space separated): ");
			bcm_bprintf(b, "[-h -r -P -S -U -i<num1,num2,...> -I<start,finish>]\n");
			bcm_bprintf(b, "-h\t(this) help output\n");
			bcm_bprintf(b, "-i0,63\tSelect indices 0 and 63 (comma separated)\n");
			bcm_bprintf(b, "-I0,63\tSelect indices 0 through 63 (comma separated)\n");
			bcm_bprintf(b, "-r\tDump raw HW content only\n");
			bcm_bprintf(b, "-P\tDump parsed version of HW content\n");
			bcm_bprintf(b, "-R\tDump rateentry only (i.e. don't dump link entry)\n");
			bcm_bprintf(b, "-L\tDump linkentry only (i.e. don't dump rate entry)\n");
			bcm_bprintf(b, "-B0,10\tDump based on BFI indices 0 through 10\n");
			bcm_bprintf(b, "-b0,10\tDump based on BFI indices 0 and 10\n");
			bcm_bprintf(b, "-T0,10\tDump based on TXV indices 0 through 10\n");
			bcm_bprintf(b, "-t0,10\tDump based on TXV indices 0 and 10\n");
			bcm_bprintf(b, "-m\tDump based on MAC addresss)\n");
			return;
		}
	}

	/* Look-up can be either BFI or TXV, but not both */
	if (((flags & TXBF_DUMP_FLAG_TXV) && (flags & TXBF_DUMP_FLAG_BFI))) {
		bcm_bprintf(b, "Look-up can be either BFI or TXV, but not both\n");
		return;
	}

	/* txv related bitmap info for both SU and MU */
	txvuse_su_bmp = wlc_bmac_read_scr(wlc->hw, S_TXVUSE_BMP);
	txvfree_bmp = wlc_bmac_read_scr(wlc->hw, S_TXVFREE_BMP);

	if (flags & TXBF_DUMP_FLAG_CQI) {
		bcm_bprintf(b, "CQI txv bitmap:\n");
		for (i = 0; i < 10; i++) {
			j = i * 2;
			txvbmp_blk0 = wlc_read_shmx(wlc, MX_CQIBMP_BLK(wlc) + j*2);
			txvbmp_blk1 = wlc_read_shmx(wlc, MX_CQIBMP_BLK(wlc) + (j+1)*2);
			if (i == 0) {
				cqi_txvuse_bmp = txvbmp_blk0 | (txvbmp_blk1 << 16);
			}
			bcm_bprintf(b, "    %8s:\t%04x_%04x\n", txvbmp_str[i],
				txvbmp_blk0, txvbmp_blk1);
		}

		entries = TXBF_MUTXV_MAX;
	}
	if (flags & TXBF_DUMP_FLAG_MU) {
		bcm_bprintf(b, "MU txv bitmap:\n");
		for (i = 0; i < 10; i++) {
			j = i * 2;
			txvbmp_blk0 = wlc_read_shmx(wlc, MX_TXVBMP_BLK(wlc) + j*2);
			txvbmp_blk1 = wlc_read_shmx(wlc, MX_TXVBMP_BLK(wlc) + (j+1)*2);
			if (i == 0) {
				txvuse_bmp = txvbmp_blk0 | (txvbmp_blk1 << 16);
			}
			bcm_bprintf(b, "    %8s:\t%04x_%04x\n", txvbmp_str[i],
				txvbmp_blk0, txvbmp_blk1);
		}

		entries += TXBF_MUTXV_MAX;
	}

	if (flags & TXBF_DUMP_FLAG_SU) {
		bcm_bprintf(b, "SU txv bitmap: inuse %04x free %04x\n",
			txvuse_su_bmp, txvfree_bmp);
		entries += TXBF_SUTXV_MAX;
	}
	if (flags & TXBF_DUMP_FLAG_BFI) {
		entries = AMT_SIZE(wlc->pub->corerev);
	}

	for (i = 0; i < TXBF_MUTXV_MAX; i++) {
		txvidx = i;
		if (!isset(&cqi_txvuse_bmp, i)) {
			continue;
		}
		j = i*4;

		txvm0 = wlc_read_shmx(wlc, MX_CQIM_BLK(wlc) + j*2);
		bfidx = getbits((uint8*)&txvm0, sizeof(txvm0),
				C_TXVM0_BFIDX_NBIT, NBITSZ(C_TXVM0_BFIDX));
		txvm1 = wlc_read_shmx(wlc, MX_CQIM_BLK(wlc) + (j+1)*2);
		txvm2 = wlc_read_shmx(wlc, MX_CQIM_BLK(wlc) + (j+2)*2);

		scb = wlc_ratelinkmem_retrieve_cur_scb(wlc, bfidx);
		if (!scb) {
			continue;
		}

		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi) {
			continue;
		}

		bcm_bprintf(b, "%d: "MACF"(aid 0x%x) CQI\n", txvidx,
			ETHER_TO_MACF(scb->ea), scb->aid);

		if (bfi->bfr_capable) {
			/* scb as the bfr */
			bcm_bprintf(b, "    bfr cap 0x%x, myaddr as bfe %s\n",
				bfi->bfr_capable,
				(txbf->bfe_capable & bfi->bfr_capable) ? "ON" : "OFF");
		}

		if (bfi->bfe_capable) {
			int k = bfidx;
			k = bfidx * 2;
			/* scb as the bfe */
			bcm_bprintf(b, "    bfe cap 0x%x, myaddr as bfr %s, nsts %d nrx %d\n",
				bfi->bfe_capable,
				(txbf->bfr_capable & bfi->bfe_capable) ? "ON" : "OFF",
				bfi->bfe_sts_cap, bfi->bfe_nrx-1);

			val0 = (wlc_read_shmx(wlc, MX_BFI_BLK(wlc) + (k*2)));
			val1 = (wlc_read_shmx(wlc, MX_BFI_BLK(wlc) + ((k+1)*2)));
			valid = (val0 & (1 << C_BFI_CQIRDY_NBIT)) ? TRUE : FALSE;

			wlc_ratelinkmem_raw_lmem_read(wlc, bfidx, &lnk, AUTO);

			bcm_bprintf(b, "    bficfg0-1: %04x_%04x bfrstats0-2: %04x_%04x_%04x "
				"bfi_blk: %04x_%04x\n",
				lnk.BFIConfig0, lnk.BFIConfig1, lnk.BFRStat0, lnk.BFRStat1,
				lnk.BFRStat2, val0, val1);
			if (flags & TXBF_DUMP_FLAG_CQI) {
				bcm_bprintf(b, "    CQI txvm_info0-2: %04x_%04x_%04x",
					txvm0, txvm1, txvm2);
			}
			j = i*2;
			bcm_bprintf(b, " last stored mimoctl: %04x_%04x\n",
				wlc_read_shm(wlc, M_CQIMSTATS_BLK(wlc)+j*2),
				wlc_read_shm(wlc, M_CQIMSTATS_BLK(wlc)+(j+1)*2));
			bcm_bprintf(b, "    Last sounding successful: %s\n", valid ?
				"true" : "false");
		}
	}
	for (i = 0; i < entries; i++) {
		txvidx = i;
		if (flags & TXBF_DUMP_FLAG_MU) {
			if (i == (TXBF_MUTXV_MAX-1)) {
				flags &= ~TXBF_DUMP_FLAG_MU;
			}

			if (!isset(&txvuse_bmp, i)) {
				continue;
			}
			j = i*4;

			txvm0 = wlc_read_shmx(wlc, MX_TXVM_BLK(wlc) + j*2);
			bfidx = getbits((uint8*)&txvm0, sizeof(txvm0),
					C_TXVM0_BFIDX_NBIT, NBITSZ(C_TXVM0_BFIDX));
			txvm1 = wlc_read_shmx(wlc, MX_TXVM_BLK(wlc) + (j+1)*2);
			txvm2 = wlc_read_shmx(wlc, MX_TXVM_BLK(wlc) + (j+2)*2);
		} else if (flags & TXBF_DUMP_FLAG_SU) {
			txvidx = i;
			if (i >= (TXBF_MUTXV_MAX)) {
				txvidx = i - TXBF_MUTXV_MAX;
			}
			if (!isset(&txvuse_su_bmp, txvidx)) {
				continue;
			}

			txvm0 = wlc_read_shm(wlc, M_STXVM_BLK(wlc) + txvidx*2);
			bfidx = getbits((uint8*)&txvm0, sizeof(txvm0),
					C_TXV_BFIIDX_NBIT, NBITSZ(C_TXV_BFIIDX));
		}

		scb = wlc_ratelinkmem_retrieve_cur_scb(wlc, bfidx);
		if (!scb) {
			continue;
		}

		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi) {
			continue;
		}

		bcm_bprintf(b, "%d: "MACF"(aid 0x%x) %s\n", txvidx,
			ETHER_TO_MACF(scb->ea), scb->aid,
			flags & TXBF_DUMP_FLAG_MU ? "MU" : "SU");

		if (bfi->bfr_capable) {
			/* scb as the bfr */
			bcm_bprintf(b, "    bfr cap 0x%x, myaddr as bfe %s\n",
				bfi->bfr_capable,
				(txbf->bfe_capable & bfi->bfr_capable) ? "ON" : "OFF");
		}

		if (bfi->bfe_capable) {
			int k = bfidx;
			k = bfidx * 2;
			/* scb as the bfe */
			bcm_bprintf(b, "    bfe cap 0x%x, myaddr as bfr %s, nsts %d nrx %d\n",
				bfi->bfe_capable,
				(txbf->bfr_capable & bfi->bfe_capable) ? "ON" : "OFF",
				bfi->bfe_sts_cap, bfi->bfe_nrx-1);

			val0 = (wlc_read_shmx(wlc, MX_BFI_BLK(wlc) + (k*2)));
			val1 = (wlc_read_shmx(wlc, MX_BFI_BLK(wlc) + ((k+1)*2)));
			valid = (val0 & (1 << C_BFI_TXVRDY_NBIT)) ? TRUE : FALSE;

			wlc_ratelinkmem_raw_lmem_read(wlc, bfidx, &lnk, AUTO);

			bcm_bprintf(b, "    bficfg0-1: %04x_%04x bfrstats0-2: %04x_%04x_%04x "
				"bfi_blk: %04x_%04x\n",
				lnk.BFIConfig0, lnk.BFIConfig1, lnk.BFRStat0, lnk.BFRStat1,
				lnk.BFRStat2, val0, val1);
			if (flags & TXBF_DUMP_FLAG_MU) {
				bcm_bprintf(b, "    MU txvm_info0-2: %04x_%04x_%04x",
					txvm0, txvm1, txvm2);
			} else if (flags & TXBF_DUMP_FLAG_SU) {
				bcm_bprintf(b, "    SU txvm_info: %04x",
				wlc_read_shm(wlc, M_STXVM_BLK(wlc)+(i*2)));
				j = TXBF_MUTXV_MAX + i;
			}
			j = i*2;
			bcm_bprintf(b, " last stored mimoctl: %04x_%04x\n",
				wlc_read_shm(wlc, M_TXVMSTATS_BLK(wlc)+j*2),
				wlc_read_shm(wlc, M_TXVMSTATS_BLK(wlc)+(j+1)*2));
			bcm_bprintf(b, "    Last sounding successful: %s\n", valid ?
				"true" : "false");
		}
	}

	if (!(flags & TXBF_DUMP_FLAG_BFI)) {
		return;
	}

	for (i = 0; i < entries; i++) {
		scb = wlc_ratelinkmem_retrieve_cur_scb(wlc, i);
		if (!scb) {
			continue;
		}
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		if (!bfi) {
			continue;
		}

		val0 = (wlc_read_shmx(wlc, MX_BFI_BLK(wlc) + (i*2)));
		val1 = (wlc_read_shmx(wlc, MX_BFI_BLK(wlc) + ((i+1)*2)));
		txvidx = getbits((uint8*)&val0, sizeof(val0),
			C_BFI_TXVIDX_NBIT, NBITSZ(C_BFI_TXVIDX));

		bcm_bprintf(b, "%d: "MACF"(aid 0x%x) %s\n", i,
			ETHER_TO_MACF(scb->ea), scb->aid,
			flags & TXBF_DUMP_FLAG_MU ? "MU" : "SU");

		if (bfi->bfr_capable) {
			/* scb as the bfr */
			bcm_bprintf(b, "    bfr cap 0x%x, myaddr as bfe %s\n",
				bfi->bfr_capable,
				(txbf->bfe_capable & bfi->bfr_capable) ? "ON" : "OFF");
		}

		if (bfi->bfe_capable) {
			/* scb as the bfe */
			bcm_bprintf(b, "    bfe cap 0x%x, myaddr as bfr %s, nsts %d nrx %d\n",
				bfi->bfe_capable,
				(txbf->bfr_capable & bfi->bfe_capable) ? "ON" : "OFF",
				bfi->bfe_sts_cap, bfi->bfe_nrx-1);
			valid = (val0 & (1 << C_BFI_TXVRDY_NBIT)) ? TRUE : FALSE;
			wlc_ratelinkmem_raw_lmem_read(wlc, i, &lnk, AUTO);

			bcm_bprintf(b, "    bficfg0-1: %04x_0x%04x bfrstats0-2: %04x_%04x_%04x "
				"bfi_blk: %04x_%04x\n",
				bfi->BFIConfig0, bfi->BFIConfig1, lnk.BFRStat0, lnk.BFRStat1,
				lnk.BFRStat2, val0, val1);
			if (flags & TXBF_DUMP_FLAG_MU) {
				txvm0 = wlc_read_shmx(wlc, MX_TXVM_BLK(wlc) + txvidx*2);
				txvm1 = wlc_read_shmx(wlc, MX_TXVM_BLK(wlc) + (txvidx+1)*2);
				txvm2 = wlc_read_shmx(wlc, MX_TXVM_BLK(wlc) + (txvidx+2)*2);
				bcm_bprintf(b, "    MU txvm_info0-2: %04x_%04x_%04x",
					txvm0, txvm1, txvm2);
			} else if (flags & TXBF_DUMP_FLAG_SU) {
				bcm_bprintf(b, "    SU txvm_info: %04x",
				wlc_read_shm(wlc, M_STXVM_BLK(wlc)+((txvidx-TXBF_MUTXV_MAX)*2)));
			}
			j = txvidx*2;
			bcm_bprintf(b, " last stored mimoctl: %04x_%04x\n",
				wlc_read_shm(wlc, M_TXVMSTATS_BLK(wlc)+j*2),
				wlc_read_shm(wlc, M_TXVMSTATS_BLK(wlc)+(j+1)*2));
			bcm_bprintf(b, "    Last sounding successful: %s\n", valid ?
				"true" : "false");
		}
	}
}

static int
wlc_txbf_dump_parse_args(wlc_info_t *wlc, uint8 bmp[], bool *dump_all, uint8* flags)
{
	int err = BCME_OK;
	char *args = wlc->dump_args;
	char *p, **argv = NULL;
	uint argc = 0;
#if NOT_YET
	uint val, strt, end, i;
#endif // endif

	if (args == NULL || bmp == NULL || dump_all == NULL || flags == NULL) {
		err = BCME_BADARG;
		goto exit;
	}

	/* allocate argv */
	if ((argv = MALLOC(wlc->osh, sizeof(*argv) * TXBF_DUMP_ARGV_MAX)) == NULL) {
		WL_ERROR(("wl%d: %s: failed to allocate the argv buffer\n",
		          wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	/* get each token */
	p = bcmstrtok(&args, " ", 0);
	while (p && argc < TXBF_DUMP_ARGV_MAX-1) {
		argv[argc++] = p;
		p = bcmstrtok(&args, " ", 0);
	}
	argv[argc] = NULL;

	/* parse argv */
	argc = 0;
	while ((p = argv[argc++]) != NULL) {
		if (*p == '-') {
			switch (*++p) {
				case 'h':
					err = BCME_BADARG; /* invoke help */
					goto exit;
				case 'M':
					*flags |= TXBF_DUMP_FLAG_MU;
					break;
				case 'S':
					*flags |= TXBF_DUMP_FLAG_SU;
					break;
				case 'B':
					*flags |= TXBF_DUMP_FLAG_BFI;
					break;
#if NOT_YET
				case 'i':
					while (*++p != '\0') {
						val = bcm_strtoul(p, &p, 0);
						if (val >= TXBF_MAX_ENTRIES) {
							err = BCME_BADARG;
							goto exit;
						}
						setbit(bmp, val);
					}
					*dump_all = FALSE; /* selected dump(s) only */
					break;
				case 'I':
					/* -Ix,y inclusively prints indexes x through y */
					if (*(p+1) == '\0' || *(p+2) == '\0') {
						err = BCME_BADARG;
						goto exit;
					}
					strt = bcm_strtoul(++p, &p, 0);
					end = bcm_strtoul(++p, &p, 0);
					if (end >= strt && end < TXBF_MAX_ENTRIES) {
						for (i = strt; i <= end; i++) {
							setbit(bmp, i);
						}
					} else {
						err = BCME_BADARG;
						goto exit;
					}
					*dump_all = FALSE; /* selected dump(s) only */
					break;
#endif /* NOT_YET */
				default:
					err = BCME_BADARG;
					goto exit;
			}
		} else {
			err = BCME_BADARG;
			goto exit;
		}
	}

exit:
	if (argv) {
		MFREE(wlc->osh, argv, sizeof(*argv) * TXBF_DUMP_ARGV_MAX);
	}

	return err;
}

static void
wlc_txbf_shm_dump(wlc_txbf_info_t *txbf, bcmstrbuf_t *b)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	uint8 i = 0;
	scb_t *scb;
	txbf_scb_info_t *bfi;
	wlc_info_t *wlc = txbf->wlc;

	bcm_bprintf(b, "\n%d links with SU-Beamforming enabled\n",
		bcm_bitcount((uint8 *)&txbf->shm_idx_bmp, sizeof(txbf->shm_idx_bmp)));

	for (i = 0; i <= txbf->max_link; i++) {
		bool valid;
		uint16 bfr_idx = i;

		if (isclr(&txbf->shm_idx_bmp, i))
			continue;
		scb = txbf->su_scbs[i];
		ASSERT(scb);
		if (!scb)
			continue;
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

		valid = (wlc_read_shm(wlc,
			shm_addr(txbf->shm_base, bfr_idx * BFI_BLK_SIZE(wlc->pub->corerev)))
			& (1 << C_BFRIDX_VLD_NBIT)) ? TRUE : FALSE;

		bcm_bprintf(b, "%d: %s(aid 0x%x)\n", (i + 1),
			bcm_ether_ntoa(&scb->ea, eabuf), scb->aid);
		bcm_bprintf(b, "\tVHT cap: 0x%x ", bfi->vht_cap);
		bcm_bprintf(b, "exp_en: %s \n\t", bfi->exp_en ? "true" : "false");
		bcm_bprintf(b, "bfi_idx: %d amt_idx: %d",
			bfi->shm_index, bfi->amt_index);

		bcm_bprintf(b, "\n\tLast sounding successful: %s\n", valid ? "true" : "false");
		if (bfi->vht_cap & VHT_CAP_INFO_SU_BEAMFMR)
			bcm_bprintf(b, "\tsu bfr cap, bfe link %s\n",
				(txbf->bfe_capable && (wlc_vht_get_cap_info(wlc->vhti) &
				VHT_CAP_INFO_SU_BEAMFMEE))? "ON" : "OFF");
		if ((bfi->vht_cap & VHT_CAP_INFO_SU_BEAMFMEE) ||
			(bfi->bfe_capable & TXBF_HE_SU_BFE_CAP)) {
			bcm_bprintf(b, "\tsu bfe cap, bfr link %s, nsts %d nrx %d\n",
				((txbf->bfr_capable && !(bfi->flags & MU_BFE)) ?
				"ON" : "OFF"), bfi->bfe_sts_cap, bfi->bfe_nrx-1);
		}

		wlc_txbf_dump_shm_bfi_blk(txbf, b, bfr_idx);

	}
}

static int
wlc_txbf_dump_clr(void *ctx)
{
	wlc_info_t *wlc = ctx;

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	wlc_write_shm(wlc, M_TXNDPA_CNT(wlc), 0);
	wlc_write_shm(wlc, M_TXNDP_CNT(wlc), 0);
	wlc_write_shm(wlc, M_TXBFPOLL_CNT(wlc), 0);
	wlc_write_shm(wlc, M_TXSF_CNT(wlc), 0);
	wlc_write_shm(wlc, M_TXCWRTS_CNT(wlc), 0);
	wlc_write_shm(wlc, M_TXCWCTS_CNT(wlc), 0);
	wlc_write_shm(wlc, M_TXBFM_CNT(wlc), 0);
	wlc_write_shm(wlc, M_RXNDPAUCAST_CNT(wlc), 0);
	wlc_write_shm(wlc, M_RXNDPAMCAST_CNT(wlc), 0);
	wlc_write_shm(wlc, M_RXBFPOLLUCAST_CNT(wlc), 0);
	wlc_write_shm(wlc, M_BFERPTRDY_CNT(wlc), 0);
	wlc_write_shm(wlc, M_RXSFUCAST_CNT(wlc), 0);
	wlc_write_shm(wlc, M_RXCWRTSUCAST_CNT(wlc), 0);
	wlc_write_shm(wlc, M_RXCWCTSUCAST_CNT(wlc), 0);

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		wlc_write_shm(wlc, M_BFD_DONE_CNT(wlc), 0),
		wlc_write_shm(wlc, M_BFD_FAIL_CNT(wlc), 0);
		wlc_write_shm(wlc, M_TXVFULL_CNT(wlc), 0);
		wlc_write_shm(wlc, M_TXBRPTRIG_CNT(wlc), 0);
		wlc_write_shm(wlc, M_RXSFCQI_CNT(wlc), 0);
		wlc_write_shm(wlc, M_NDPAUSR_CNT(wlc), 0);
	}

	return BCME_OK;
}

#define TXBF_DUMP_LEN (16 * 1024)
static int
wlc_txbf_dump(void *ctx, bcmstrbuf_t *b)
{
	scb_t *scb;
	txbf_scb_info_t *txbf_scb_info;
	wlc_info_t *wlc = ctx;
	wlc_txbf_info_t *txbf = wlc->txbf;
	scb_iter_t scbiter;
	int ret = BCME_OK;
	uint16 coremask0, coremask1;
	char *buf = NULL;
	struct  bcmstrbuf bstr;

	if (!b) {
		buf = MALLOCZ(wlc->osh, TXBF_DUMP_LEN);
		bcm_binit(&bstr, buf, TXBF_DUMP_LEN);
		b = &bstr;
	}

	if (!wlc->clk) {
		ret = BCME_NOCLK;
		goto done;
	}

	if (!TXBF_ENAB(wlc->pub)) {
		bcm_bprintf(b, "Beamforming is not supported!\n");
	}

	bcm_bprintf(b, "Bfr capable: 0x%x Bfe capable: 0x%x\n"
		"Explicit txbf mode: %s, active: %s\n"
		"Allowed by country code: %s\n"
		"Allow txbf on non primaryIF: %s\n",
		txbf->bfr_capable, txbf->bfe_capable,
		(txbf->mode == TXBF_ON ? "on" : (txbf->mode == TXBF_AUTO ? "auto" : "off")),
		txbf->active ? "true" : "false", wlc->stf->allow_txbf ? "true" : "false",
		txbf->virtif_disable ? "false" : "true");
	coremask0 = wlc_read_shm(wlc, M_COREMASK_BFM(wlc));
	coremask1 = wlc_read_shm(wlc, M_COREMASK_BFM1(wlc));
	bcm_bprintf(b, "coremask0 0x%x coremask1 0x%x\n", coremask0, coremask1);

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		wlc_txbf_shm_dump(txbf, b);
		wlc_txbf_shmx_dump(txbf, b);
	} else {
		wlc_txbf_info_dump(txbf, b);
	}
	/*
	 * dump implicit info
	 */
	bcm_bprintf(b, "Implicit txbf mode: %s, calibr_flag: %d\n",
		(txbf->imp == TXBF_ON ? "on" : (txbf->imp == TXBF_AUTO ? "auto" : "off")),
		(txbf->flags & WLC_TXBF_FLAG_IMPBF) ? 1 : 0);
	if (((txbf->flags & WLC_TXBF_FLAG_IMPBF) && txbf->imp)) {
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			txbf_scb_info = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
			if (!txbf_scb_info) {
				continue;
			}
			if (!txbf_scb_info->imp_en) {
				continue;
			}
			bcm_bprintf(b, ""MACF"(aid 0x%x): imp_used %d imp_bad %d\n",
				ETHER_TO_MACF(scb->ea), scb->aid,
				txbf_scb_info->imp_used, txbf_scb_info->imp_bad);
		}
	}

	wlc_txbf_dump_stats(txbf, b);

done:
	if (buf) {
		MFREE(wlc->osh, buf, TXBF_DUMP_LEN);
	}

	return ret;
}

static void
wlc_txbf_dump_stats(wlc_txbf_info_t *txbf, bcmstrbuf_t *b)
{
	wlc_info_t *wlc = txbf->wlc;

	if (!wlc->clk)
		return;

	bcm_bprintf(b, "\nBeamforming Statistics:\n");
	bcm_bprintf(b, "txndpa %d txndp %d txbfpoll %d ",
		wlc_read_shm(wlc, M_TXNDPA_CNT(wlc)),
		wlc_read_shm(wlc, M_TXNDP_CNT(wlc)),
		wlc_read_shm(wlc, M_TXBFPOLL_CNT(wlc)));
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		bcm_bprintf(b, "txbrp %d ",
		wlc_read_shm(wlc, M_TXBRPTRIG_CNT(wlc)));
	}
	bcm_bprintf(b, "txsf %d txcwrts %d txcwcts %d"
		" txbfm %d\n",
		wlc_read_shm(wlc, M_TXSF_CNT(wlc)),
		wlc_read_shm(wlc, M_TXCWRTS_CNT(wlc)),
		wlc_read_shm(wlc, M_TXCWCTS_CNT(wlc)),
		wlc_read_shm(wlc, M_TXBFM_CNT(wlc)));
	bcm_bprintf(b, "rxndpa_u %d rxndpa_m %d rxbfpoll %d bferpt %d "
		"rxsf %d ",
		wlc_read_shm(wlc, M_RXNDPAUCAST_CNT(wlc)),
		wlc_read_shm(wlc, M_RXNDPAMCAST_CNT(wlc)),
		wlc_read_shm(wlc, M_RXBFPOLLUCAST_CNT(wlc)),
		wlc_read_shm(wlc, M_BFERPTRDY_CNT(wlc)),
		wlc_read_shm(wlc, M_RXSFUCAST_CNT(wlc)));

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		bcm_bprintf(b, "exprxsf %d rxcqi %d ",
			wlc_read_shm(wlc, M_NDPAUSR_CNT(wlc)),
			wlc_read_shm(wlc, M_RXSFCQI_CNT(wlc)));
	}
	bcm_bprintf(b, "rxcwrts %d rxcwcts %d\n",
		wlc_read_shm(wlc, M_RXCWRTSUCAST_CNT(wlc)),
		wlc_read_shm(wlc, M_RXCWCTSUCAST_CNT(wlc)));

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		bcm_bprintf(b, "bfd_dn %d bfd_fl %d\n",
		wlc_read_shm(wlc, M_BFD_DONE_CNT(wlc)),
		wlc_read_shm(wlc, M_BFD_FAIL_CNT(wlc)));
	}
}

static void
wlc_txbf_dump_shm_bfi_blk(wlc_txbf_info_t *txbf, bcmstrbuf_t *b, uint8 idx)
{
	uint32 offset;
	uint16 bfi_blk, val;
	char	*buf = NULL;
	struct  bcmstrbuf bstr;
	wlc_info_t *wlc = txbf->wlc;

	if (!wlc->clk)
		return;

	if (!b) {
		buf = MALLOCZ(wlc->osh, TXBF_DUMP_LEN);
		bcm_binit(&bstr, buf, TXBF_DUMP_LEN);
		b = &bstr;
	}

	val = wlc_read_amtinfo_by_idx(wlc, idx);
	if (val) {
		bcm_bprintf(b, "\tAMT_INFO_BLK[%d] = 0x%x\n", idx, val);
	}

	bfi_blk = (txbf->shm_base + idx * BFI_BLK_SIZE(wlc->pub->corerev));
	bcm_bprintf(b, "\tshm_blk %d:\n", idx);

	for (offset = 0; offset < BFI_BLK_SIZE(wlc->pub->corerev); offset += 4) {
		bcm_bprintf(b, "\toffset %2d: %04x %04x %04x %04x\n", offset,
		    wlc_read_shm(wlc, shm_addr(bfi_blk, offset)),
		    wlc_read_shm(wlc, shm_addr(bfi_blk, (offset + 1))),
		    wlc_read_shm(wlc, shm_addr(bfi_blk, (offset + 2))),
		    wlc_read_shm(wlc, shm_addr(bfi_blk, (offset + 3))));
	}

	if (buf) {
		MFREE(wlc->osh, buf, TXBF_DUMP_LEN);
	}
}

static void
wlc_txbf_dump_shmx_bfi_blk(wlc_txbf_info_t *txbf, bcmstrbuf_t *b, uint8 idx)
{
	uint32 offset;
	uint16 bfi_blk;
	wlc_info_t *wlc = txbf->wlc;
	char	*buf = NULL;
	struct  bcmstrbuf bstr;

	if (!wlc->clk) {
		return;
	}

	if (!PSMX_ENAB(wlc->pub)) {
		return;
	}

	if (!b) {
		buf = MALLOCZ(wlc->osh, TXBF_DUMP_LEN);
		bcm_binit(&bstr, buf, TXBF_DUMP_LEN);
		b = &bstr;
	}

	bfi_blk = (txbf->mx_bfiblk_base + idx * BFI_BLK_SIZE(wlc->pub->corerev));
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		bcm_bprintf(b, "shmx0 %04x shmx1 %04x \n",
			wlc_read_shmx(wlc, shm_addr(bfi_blk, C_BFI_IDX_POS)),
			wlc_read_shmx(wlc, shm_addr(bfi_blk, C_BFI_MTXV_POS)));
	} else {
		bcm_bprintf(b, "\tshmx_blk %d:\n", idx);
		for (offset = 0; offset < BFI_BLK_SIZE(wlc->pub->corerev); offset += 4) {
			bcm_bprintf(b, "\toffset %2d: %04x %04x %04x %04x\n", offset,
			    wlc_read_shmx(wlc, shm_addr(bfi_blk, offset)),
			    wlc_read_shmx(wlc, shm_addr(bfi_blk, (offset + 1))),
			    wlc_read_shmx(wlc, shm_addr(bfi_blk, (offset + 2))),
			    wlc_read_shmx(wlc, shm_addr(bfi_blk, (offset + 3))));
		}
	}

	if (buf) {
		MFREE(wlc->osh, buf, TXBF_DUMP_LEN);
	}

}
#endif // endif

/*
 * query keymgmt for the entry to use
 */
static int
wlc_txbf_get_amt(wlc_info_t *wlc, scb_t *scb, int *amt_idx)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	const struct ether_addr *ea;
	int err = BCME_OK;
	wlc_bsscfg_t *cfg;

	ASSERT(amt_idx != NULL && scb != NULL);
	ea = &scb->ea;
	cfg = scb->bsscfg;

	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(cfg);

	WL_TXBF(("wl%d %s: ea: %s\n", WLCWLUNIT(wlc), __FUNCTION__,
		bcm_ether_ntoa(ea, eabuf)));
	BCM_REFERENCE(ea);

	if (D11REV_LT(wlc->pub->corerev, 40))
		return BCME_UNSUPPORTED;

	if (!wlc->clk)
		return BCME_NOCLK;
#ifdef PSTA
	if (BSSCFG_STA(cfg) && PSTA_ENAB(wlc->pub)) {
		*amt_idx = AMT_IDX_BSSID;
		return BCME_OK;
	}
#endif // endif

#if defined(WET) || defined(WET_DONGLE)
	if (BSSCFG_STA(cfg) && (WET_ENAB(wlc) || WET_DONGLE_ENAB(wlc))) {
		*amt_idx = AMT_IDX_BSSID;
		return BCME_OK;
	}
#endif // endif
	/* keymgmt currently owns amt A2/TA amt allocations */
	*amt_idx = wlc_keymgmt_get_scb_amt_idx(wlc->keymgmt, scb);
	if (*amt_idx < 0)
		err = *amt_idx;

	WL_TXBF(("wl%d %s: amt entry %d status %d\n",
		WLCWLUNIT(wlc), __FUNCTION__, *amt_idx, err));
	return err;
}

void wlc_txfbf_update_amt_idx(wlc_txbf_info_t *txbf, int amt_idx, const struct ether_addr *addr)
{
	uint32 i, shm_idx;
	uint16 val;
	wlc_info_t *wlc;

	uint16 attr;
	char eabuf[ETHER_ADDR_STR_LEN];
	struct ether_addr tmp;
	scb_t *scb;
	txbf_scb_info_t *bfi;
	int32 idx;
	wlc_bsscfg_t *cfg;
	scb_t *psta_scb = NULL;
	scb_t *psta_prim = NULL;

	BCM_REFERENCE(eabuf);
	ASSERT(txbf);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (!TXBF_ENAB(wlc->pub)) {
		return;
	}

	if (txbf->shm_idx_bmp == 0)
		return;

	wlc_bmac_read_amt(wlc->hw, amt_idx, &tmp, &attr);
	if ((attr & ((AMT_ATTR_VALID) | (AMT_ATTR_A2)))
		!= ((AMT_ATTR_VALID) | (AMT_ATTR_A2))) {
		return;
	}

	/* PSTA AWARE AP: Look for PSTA SCB */
	FOREACH_BSS(wlc, idx, cfg) {
		psta_scb = wlc_scbfind(wlc, cfg, addr);
		if (psta_scb != NULL)
			break;
	}

	if (psta_scb != NULL) {
		psta_prim = wlc_ap_get_psta_prim(wlc->ap, psta_scb);
	}

	for (i = 0; i <= txbf->max_link; i++) {
		if (isclr(&txbf->shm_idx_bmp, i))
			continue;

		scb = txbf->su_scbs[i];
		ASSERT(scb);
		if (!scb)
			continue;
		bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
		ASSERT(bfi);
		if (!bfi) {
			WL_ERROR(("wl%d: %s update amt %x for %s failed\n",
				wlc->pub->unit, __FUNCTION__, amt_idx,
				bcm_ether_ntoa(addr, eabuf)));
			return;
		}

		if ((eacmp(&txbf->su_scbs[i]->ea, addr) == 0) ||
			((psta_scb != NULL) && (txbf->su_scbs[i] == psta_prim))) {
			shm_idx = bfi->shm_index;
			val = wlc_read_amtinfo_by_idx(wlc, amt_idx);
			val &= ~((NBITMASK(C_ADDR_BFIIDX_LT128_BSZ)) << C_ADDR_BFIIDX_LT128_NBIT);
			val |= (NBITVAL(C_ADDR_BFCAP_LT128_NBIT) |
				shm_idx << C_ADDR_BFIIDX_LT128_NBIT);
			wlc_write_amtinfo_by_idx(wlc, amt_idx, val);
			WL_TXBF(("wl%d: %s update amt idx %d %s for shm idx %d: val %#x\n",
				wlc->pub->unit, __FUNCTION__, amt_idx,
				bcm_ether_ntoa(addr, eabuf), shm_idx, val));
			return;
		}
	}
}

#if defined(BCMDBG)
static int
wlc_txbf_txbfcfg_get_dispatch(wlc_txbf_info_t *txbf, void *p,
	char *outbuf, int outlen)
{
	wlc_info_t *wlc = txbf->wlc;
	wl_txbfcfg_params_t txbfcfg_params;
	txbf_cfg_tuple_t *txbf_cfg;
	bcmstrbuf_t bstr;
	int ret;
	int i, start, end;

	ret = BCME_OK;
	bcopy(p, &txbfcfg_params, sizeof(wl_txbfcfg_params_t));
	bcm_binit(&bstr, outbuf, outlen);

	if (D11REV_LE(wlc->pub->corerev, 128)) {
		bcm_bprintf(&bstr, "txbfcfg is unsupported in %d\n", wlc->pub->corerev);
		return BCME_UNSUPPORTED;
	}

	if (!txbf->txbfcfg_blk_ptr) {
		bcm_bprintf(&bstr, "txbfcfg_blk is empty\n");
		return BCME_ERROR;
	}

	bcm_bprintf(&bstr, "idx %-20s  %s\t%s\t%s\n", "name", "val", "hex", "offset");

	if (txbfcfg_params.flags & WL_TXBFCFG_FLAGS_GETSEL_MASK &&
		txbfcfg_params.index < TXBFCFG_MAX_ENTRIES &&
		txbfcfg_params.index >= 0) {
		start = txbfcfg_params.index;
		end = txbfcfg_params.index + 1;
	} else {
		start = 0;
		end = TXBFCFG_MAX_ENTRIES;
	}

	for (i = start; i < end; i++) {
		txbf_cfg = &txbf->txbfcfg_blk_ptr->txbf_cfg[i];
		bcm_bprintf(&bstr, "%-02d  %-20s %4d\t0x%04x\t0x%x\n",
			i, txbfcfg_str_array[i], txbf_cfg->val,
			txbf_cfg->val, txbf_cfg->offset);
	}

	return ret;
}

static int
wlc_txbf_txbfcfg_set_dispatch(wlc_txbf_info_t *txbf, void *p)
{
	wlc_info_t *wlc = txbf->wlc;
	wl_txbfcfg_params_t *txbfcfg_params = (wl_txbfcfg_params_t *) p;
	txbf_cfg_tuple_t *txbf_cfg;

	if (D11REV_LE(wlc->pub->corerev, 128)) {
		return BCME_UNSUPPORTED;
	}

	if (!txbf->txbfcfg_blk_ptr) {
		return BCME_ERROR;
	}

	if ((txbfcfg_params->index >= TXBFCFG_MAX_ENTRIES) ||
		(txbfcfg_params->index < 0)) {
		return BCME_BADARG;
	}

	txbf_cfg = &txbf->txbfcfg_blk_ptr->txbf_cfg[txbfcfg_params->index];

	txbf_cfg->val = txbfcfg_params->val;

	wlc_txbf_txbfcfg_write_shmx(txbf, txbfcfg_params->index);

	return BCME_OK;
}
#endif // endif

#if defined(WL_PSMX)
static void
wlc_txbf_txbfcfg_write_shmx(wlc_txbf_info_t *txbf, int index)
{
	wlc_info_t *wlc = txbf->wlc;
	txbf_cfg_tuple_t *txbf_cfg;

	if (!PSMX_ENAB(wlc->pub) || !(wlc->clk)) {
		return;
	}

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return;
	}

	ASSERT((index < TXBFCFG_MAX_ENTRIES) && (index >= 0));

	txbf_cfg = &txbf->txbfcfg_blk_ptr->txbf_cfg[index];

	switch (index) {
	case TXBFCFG_AUTOTXV_ADD:
		wlc_bmac_mhf(wlc->hw, MXHF1, MXHF1_MTXVADD,
			txbf_cfg->val ? MXHF1_MTXVADD : 0, WLC_BAND_ALL);
		break;
	case TXBFCFG_AUTOTXV_DEL:
		wlc_bmac_mhf(wlc->hw, MXHF1, MXHF1_MTXVDEL,
			txbf_cfg->val ? MXHF1_MTXVDEL : 0, WLC_BAND_ALL);
	case TXBFCFG_MUSND_PER:
		wlc_bmac_mhf(wlc->hw, MXHF0, MXHF0_DYNSND,
			txbf_cfg->val ? MXHF0_DYNSND : 0, WLC_BAND_ALL);
		break;
	case TXBFCFG_STXV_MAXCNT: {
		uint16 bmsk = ((1 << txbf_cfg->val) - 1);
		wlc_bmac_write_scr(wlc->hw, txbf_cfg->offset, bmsk);
		break;
	}
	default:
		wlc_write_shmx(wlc, txbf_cfg->offset, txbf_cfg->val);
		break;
	}
}
#endif /* WL_PSMX */

/* Returns TRUE if ucode manages the fifo group index assignment and deletion
 * Returns FALSE otherwise.
 */
bool
wlc_txbf_autotxvcfg_get(wlc_txbf_info_t *txbf, bool add)
{
	bool autocfg = 0;
#if defined(WL_PSMX)
	wlc_info_t *wlc = txbf->wlc;
	txbf_cfg_tuple_t *txbf_cfg;

	if (!PSMX_ENAB(wlc->pub) || !(wlc->clk)) {
		return FALSE;
	}

	if (add) {
		txbf_cfg = &txbf->txbfcfg_blk_ptr->txbf_cfg[TXBFCFG_AUTOTXV_ADD];
		autocfg = txbf_cfg->val & MXHF1_MTXVADD;
	} else {
		txbf_cfg = &txbf->txbfcfg_blk_ptr->txbf_cfg[TXBFCFG_AUTOTXV_DEL];
		autocfg = txbf_cfg->val & MXHF1_MTXVDEL;
	}

#endif /* WL_PSMX */
	return autocfg;
}

static int
wlc_txbf_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, wlc_if_t *wlcif)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)hdl;
	wlc_info_t *wlc = txbf->wlc;
	wlc_pub_t *pub = wlc->pub;

	int32 int_val = 0;
	bool bool_val;
	uint32 *ret_uint_ptr;
	int32 *ret_int_ptr;
	int err = 0;
#if defined(WL_PSMX)
	uint16 uint16_val;
#endif // endif
	BCM_REFERENCE(alen);
	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlcif);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	ret_uint_ptr = (uint32 *)a;
	ret_int_ptr = (int32 *)a;

	if (!TXBF_ENAB(pub)) {
		return BCME_UNSUPPORTED;
	}

	switch (actionid) {
	case IOV_GVAL(IOV_TXBF_MODE):
		if (txbf->bfr_capable && txbf->mode)
			*ret_uint_ptr = txbf->mode;
		else
			*ret_uint_ptr = 0;
		break;

	case IOV_SVAL(IOV_TXBF_MODE):
		if (int_val > TXBF_ON) {
			err = BCME_BADOPTION;
			break;
		}
		if (txbf->bfr_capable || WLC_BITSCNT(wlc->stf->txchain) > 1) {
			txbf->mode = (uint8) int_val;
			if (txbf->mode == TXBF_OFF) {
				txbf->active = FALSE;
				WL_TXBF(("%s: TxBF inactive\n", __FUNCTION__));
			} else if (txbf->mode && (WLC_BITSCNT(wlc->stf->txchain) > 1) &&
				txbf->shm_idx_bmp) {
				txbf->active = TRUE;
				WL_TXBF(("%s: TxBF active\n", __FUNCTION__));
			}

			wlc_txbf_upd(txbf);
			/* invalidate txcache since rates are changing */
			if (WLC_TXC_ENAB(wlc))
				wlc_txc_inv_all(wlc->txc);
			if (RATELINKMEM_ENAB(pub) &&
				(wlc->band->rspec_override & WL_RSPEC_OVERRIDE_RATE)) {
				wlc_ratelinkmem_update_rate_entry(wlc,
						WLC_RLM_SPECIAL_RATE_SCB(wlc), NULL, 0);
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	case IOV_GVAL(IOV_TXBF_BFR_CAP):
		*ret_int_ptr = txbf->bfr_capable;
		break;

	case IOV_SVAL(IOV_TXBF_BFR_CAP):
		/* range check */
		if (D11REV_GE(pub->corerev, 128)) {
			int_val &= (TXBF_SU_MU_BFR_CAP | TXBF_HE_SU_MU_BFR_CAP
				| TXBF_HE_CQI_BFR_CAP);
		} else if (D11REV_GE(pub->corerev, 65)) {
			int_val &= TXBF_SU_MU_BFR_CAP;
		} else if (D11REV_GE(pub->corerev, 40)) {
			int_val &= TXBF_SU_BFR_CAP;
		} else {
			err = BCME_UNSUPPORTED;
			break;
		}

		txbf->bfr_capable = 0;
		if ((int_val & TXBF_SU_BFR_CAP) && WLC_SU_BFR_CAP_PHY(wlc)) {
			txbf->bfr_capable |= TXBF_SU_BFR_CAP;
		}

		if (D11REV_GE(pub->corerev, 65) && (int_val & TXBF_MU_BFR_CAP) &&
			WLC_MU_BFR_CAP_PHY(wlc)) {
			txbf->bfr_capable |= TXBF_SU_MU_BFR_CAP;
		}

		if (D11REV_GE(pub->corerev, 128) &&
			HE_ENAB_BAND(pub, wlc->band->bandtype)) {
			if ((int_val & TXBF_HE_SU_BFR_CAP) && WLC_HE_SU_BFR_CAP_PHY(wlc)) {
				txbf->bfr_capable |= TXBF_HE_SU_BFR_CAP;
			}
			if ((int_val & TXBF_HE_MU_BFR_CAP) &&
				WLC_HE_SU_MU_BFR_CAP_PHY(wlc) &&
				HE_DLMU_ENAB(pub)) {
				txbf->bfr_capable |= TXBF_HE_SU_MU_BFR_CAP;
			}
			if ((int_val & TXBF_HE_CQI_BFR_CAP) &&
				(BSSCFG_AP(wlc->cfg) &&
				HE_DLMU_ENAB(pub))) {
				txbf->bfr_capable |= TXBF_HE_CQI_BFR_CAP;
			}
		}
		break;

	case IOV_GVAL(IOV_TXBF_BFE_CAP):
		*ret_int_ptr = txbf->bfe_capable;
		break;

	case IOV_SVAL(IOV_TXBF_BFE_CAP):
		/* range check */
		if (D11REV_GE(pub->corerev, 128)) {
			int_val &= (TXBF_SU_BFE_CAP | TXBF_MU_BFE_CAP | TXBF_HE_SU_MU_BFE_CAP
				| TXBF_HE_CQI_BFE_CAP);
		} else if (D11REV_GE(pub->corerev, 65)) {
			int_val &= (TXBF_SU_BFE_CAP | TXBF_MU_BFE_CAP);
		} else if (D11REV_GE(pub->corerev, 40)) {
			int_val &= TXBF_SU_BFE_CAP;
		} else {
			err = BCME_UNSUPPORTED;
			break;
		}

		txbf->bfe_capable = 0;
		if ((int_val & TXBF_SU_BFE_CAP) && WLC_SU_BFE_CAP_PHY(wlc)) {
			txbf->bfe_capable |= TXBF_SU_BFE_CAP;
		}

		if (D11REV_GE(pub->corerev, 65) && (int_val & TXBF_MU_BFE_CAP) &&
			WLC_MU_BFE_CAP_PHY(wlc)) {
			txbf->bfe_capable |= (TXBF_SU_BFE_CAP | TXBF_MU_BFE_CAP);
		}
		if (D11REV_GE(pub->corerev, 128) &&
			HE_ENAB_BAND(pub, wlc->band->bandtype)) {
			if ((int_val & TXBF_HE_SU_BFE_CAP) && WLC_HE_SU_BFE_CAP_PHY(wlc)) {
				txbf->bfe_capable |= TXBF_HE_SU_BFE_CAP;
			}
			if ((int_val & TXBF_HE_MU_BFE_CAP) &&
				WLC_HE_SU_MU_BFE_CAP_PHY(wlc)) {
				txbf->bfe_capable |= TXBF_HE_SU_MU_BFE_CAP;
			}
			if ((int_val & TXBF_HE_CQI_BFE_CAP) &&
				(BSSCFG_STA(wlc->cfg) &&
				HE_DLMU_ENAB(pub))) {
				txbf->bfe_capable |= TXBF_HE_CQI_BFE_CAP;
			}
		}
		break;

	case IOV_GVAL(IOV_TXBF_TIMER):
		if (txbf->sounding_period == 0)
			*ret_uint_ptr = (uint32) -1; /* -1 auto */
		else if (txbf->sounding_period == BF_SOUND_PERIOD_DISABLED)
			*ret_uint_ptr = 0; /* 0: disabled */
		else
			*ret_uint_ptr = (uint32)txbf->sounding_period * 4/ 1000;
		break;

	case IOV_SVAL(IOV_TXBF_TIMER):
		if (int_val == -1) /* -1 auto */
			txbf->sounding_period = 0;
		else if (int_val == 0) /* 0: disabled */
			txbf->sounding_period = BF_SOUND_PERIOD_DISABLED;
		else if ((int_val < BF_SOUND_PERIOD_MIN) || (int_val > BF_SOUND_PERIOD_MAX))
			return BCME_BADARG;
		else
			txbf->sounding_period = (uint16)int_val * (1000 / 4);
		wlc_write_shm(wlc, M_BFI_REFRESH_THR(wlc), txbf->sounding_period);
		break;

	case IOV_GVAL(IOV_TXBF_RATESET): {
		int i;
		wl_txbf_rateset_t *ret_rs = (wl_txbf_rateset_t *)a;

		/* Copy MCS rateset */
		bcopy(txbf->txbf_rate_mcs, ret_rs->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
		bcopy(txbf->txbf_rate_mcs_bcm, ret_rs->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
		/* Copy VHT rateset */
		for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
			ret_rs->txbf_rate_vht[i] = txbf->txbf_rate_vht[i];
			ret_rs->txbf_rate_vht_bcm[i] = txbf->txbf_rate_vht_bcm[i];
		}
		/* Copy OFDM rateset */
		ret_rs->txbf_rate_ofdm_cnt = txbf->txbf_rate_ofdm_cnt;
		bcopy(txbf->txbf_rate_ofdm, ret_rs->txbf_rate_ofdm, txbf->txbf_rate_ofdm_cnt);
		ret_rs->txbf_rate_ofdm_cnt_bcm = txbf->txbf_rate_ofdm_cnt_bcm;
		bcopy(txbf->txbf_rate_ofdm_bcm, ret_rs->txbf_rate_ofdm_bcm,
			txbf->txbf_rate_ofdm_cnt_bcm);
		break;
	}

	case IOV_SVAL(IOV_TXBF_RATESET): {
		int i;
		wl_txbf_rateset_t *in_rs = (wl_txbf_rateset_t *)a;

		/* Copy MCS rateset */
		bcopy(in_rs->txbf_rate_mcs, txbf->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
		bcopy(in_rs->txbf_rate_mcs_bcm, txbf->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
		/* Copy VHT rateset */
		for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
			txbf->txbf_rate_vht[i] = in_rs->txbf_rate_vht[i];
			txbf->txbf_rate_vht_bcm[i] = in_rs->txbf_rate_vht_bcm[i];
		}
		/* Copy OFDM rateset */
		txbf->txbf_rate_ofdm_cnt = in_rs->txbf_rate_ofdm_cnt;
		bcopy(in_rs->txbf_rate_ofdm, txbf->txbf_rate_ofdm, in_rs->txbf_rate_ofdm_cnt);
		txbf->txbf_rate_ofdm_cnt_bcm = in_rs->txbf_rate_ofdm_cnt_bcm;
		bcopy(in_rs->txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_bcm,
			in_rs->txbf_rate_ofdm_cnt_bcm);
		break;
	}

	case IOV_GVAL(IOV_TXBF_VIRTIF_DISABLE):
		*ret_int_ptr = (int32)txbf->virtif_disable;
		break;

	case IOV_SVAL(IOV_TXBF_VIRTIF_DISABLE):
		if (D11REV_GE(pub->corerev, 40)) {
			txbf->virtif_disable = bool_val;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

#if defined(WLTEST) || defined(WLPKTENG)
	case IOV_GVAL(IOV_TXBF_BFE_NRX_OV):
		*ret_int_ptr = (int32)txbf->bfe_nrx_ov;
		break;

	case IOV_SVAL(IOV_TXBF_BFE_NRX_OV):
		if ((int_val < 0) || (int_val > 4))
			return BCME_BADARG;
		else
			txbf->bfe_nrx_ov = (uint8)int_val;
		break;
#endif // endif
	case IOV_GVAL(IOV_TXBF_IMP):
		*ret_uint_ptr = txbf->imp;
		break;

	case IOV_SVAL(IOV_TXBF_IMP):
		{
			uint8 imp = (uint8) int_val & 0x3;
			txbf->imp_nochk = (int_val & 0x4) ? TRUE : FALSE;

			if (txbf->imp != imp) {
				txbf->imp = imp;
				/* need to call to update imp_en in all scb */
				wlc_txbf_impbf_updall(txbf);
			}
		}
		break;
	case IOV_GVAL(IOV_TXBF_HT_ENABLE):
		*ret_int_ptr = (int32)txbf->ht_enable;
		break;

	case IOV_SVAL(IOV_TXBF_HT_ENABLE):
		if (D11REV_GE(pub->corerev, 40)) {
			txbf->ht_enable = bool_val;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
#ifdef TXBF_MORE_LINKS
	case IOV_GVAL(IOV_TXBF_SCHED_TIMER):
		*ret_uint_ptr = txbf->sched_timer_interval;
		break;

	case IOV_SVAL(IOV_TXBF_SCHED_TIMER):
		if (txbf->sched_timer_added && (txbf->sched_timer_interval != 0)) {
			wl_del_timer(wlc->wl, txbf->sched_timer);
			txbf->sched_timer_added = 0;
		}

		txbf->sched_timer_interval = int_val;
		txbf->pkt_thre_sched = txbf->pkt_thre_sec * txbf->sched_timer_interval / 1000;

		if (txbf->sched_timer_enb &&
			!txbf->sched_timer_added &&
			(txbf->sched_timer_interval != 0)) {
			wl_add_timer(wlc->wl, txbf->sched_timer, txbf->sched_timer_interval, TRUE);
			txbf->sched_timer_added = 1;
		}

		break;

	case IOV_GVAL(IOV_TXBF_SCHED_MSG):
		*ret_uint_ptr = txbf->sched_msg;
		break;

	case IOV_SVAL(IOV_TXBF_SCHED_MSG):
		txbf->sched_msg = int_val;
		break;

	case IOV_GVAL(IOV_TXBF_MAX_RSSI):
		*ret_int_ptr = txbf->max_rssi;
		break;

	case IOV_SVAL(IOV_TXBF_MAX_RSSI):
		txbf->max_rssi = int_val;
		break;

	case IOV_GVAL(IOV_TXBF_MIN_RSSI):
		*ret_int_ptr = txbf->min_rssi;
		break;

	case IOV_SVAL(IOV_TXBF_MIN_RSSI):
		txbf->min_rssi = int_val;
		break;

	case IOV_GVAL(IOV_TXBF_PKT_THRE):
		*ret_uint_ptr = txbf->pkt_thre_sec;
		break;

	case IOV_SVAL(IOV_TXBF_PKT_THRE):
		txbf->pkt_thre_sec = int_val;
		txbf->pkt_thre_sched = txbf->pkt_thre_sec * txbf->sched_timer_interval / 1000;
		break;
#endif /* TXBF_MORE_LINKS */

#if defined(WL_PSMX)
	case IOV_GVAL(IOV_TXBF_MUTIMER):
		if (txbf->mu_sounding_period == 0xffff)
			*ret_uint_ptr = (uint32) -1;
		else
			*ret_uint_ptr = (uint32)(txbf->mu_sounding_period);
		break;

	case IOV_SVAL(IOV_TXBF_MUTIMER):
		uint16_val = (uint16)int_val;
		ASSERT(PSMX_ENAB(pub));
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			wlc_macreq_txbf_mutimer(wlc, uint16_val);
		} else {
			if (uint16_val == 0xffff || uint16_val == 0) {
				/* do it once or stop */
				wlc_write_shmx(wlc, MX_MUSND_PER(wlc), uint16_val);
			} else if (txbf->mu_sounding_period != uint16_val) {
				/* Need to stop sounding first for ucode to load the new value */
				wlc_write_shmx(wlc, MX_MUSND_PER(wlc), 0);
				OSL_DELAY(10);
				wlc_write_shmx(wlc, MX_MUSND_PER(wlc), uint16_val << 2);
			}
		}

		txbf->mu_sounding_period = uint16_val;
		break;
#endif /* WL_PSMX */

#if defined(BCMDBG)
	case IOV_GVAL(IOV_TXBF_CFG):
		err = wlc_txbf_txbfcfg_get_dispatch(txbf, p, a, alen);
		break;

	case IOV_SVAL(IOV_TXBF_CFG):
		err = wlc_txbf_txbfcfg_set_dispatch(txbf, p);
		break;
#endif // endif
	default:
		WL_ERROR(("wl%d %s %x not supported\n", pub->unit, __FUNCTION__, actionid));
		return BCME_UNSUPPORTED;
	}
	return err;
}

void wlc_txbf_pkteng_tx_start(wlc_txbf_info_t *txbf, scb_t *scb)
{
	uint16 val, bfrctl = 0;
	wlc_info_t *wlc;
	txbf_scb_info_t *bfi;
	uint8 nsts_shift;

	wlc = txbf->wlc;
	ASSERT(wlc);
	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(bfi);
	if (!bfi || (WLC_BITSCNT(wlc->stf->txchain) == 1)) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}
	if (D11REV_GE(wlc->pub->corerev, 128)) {
		WL_ERROR(("rev ge129 Not supported\n"));
		return;
	}

	wlc_suspend_mac_and_wait(wlc);
	nsts_shift = C_BFI_BFRCTL_POS_NSTS_SHIFT;
	/* NDP streams and VHT/HT */
	if (WLC_BITSCNT(wlc->stf->txchain) == 4) {
		/* 4 streams */
		bfrctl = (2 << nsts_shift);
		bfi->bfe_sts_cap = 3;
	} else if (WLC_BITSCNT(wlc->stf->txchain) == 3) {
		/* 3 streams */
		bfrctl = (1 << nsts_shift);
		bfi->bfe_sts_cap = 2;
	} else if (WLC_BITSCNT(wlc->stf->txchain) == 2) {
		/* 2 streams */
		bfrctl = 0;
		bfi->bfe_sts_cap = 1;
	}
	bfrctl |= (wlc->stf->txchain << C_BFI_BFRCTL_POS_BFM_SHIFT);
	wlc_write_shm(wlc, shm_addr(txbf->shm_base, C_BFI_BFRCTL_POS), bfrctl);

	/* borrow shm block 0 for pkteng */
	val = wlc_read_shm(wlc, M_BFI_BLK(wlc));
	/* fake valid bit */
	val |= (1 << 8);
	/* use highest bw */
	val |= (3 << 12);
	wlc_write_shm(wlc, M_BFI_BLK(wlc), val);
	wlc_write_shm(wlc, M_BFI_REFRESH_THR(wlc), -1);

	wlc_enable_mac(wlc);

	bfi->shm_index = 0;
	bfi->exp_en = TRUE;

	if (!txbf->active) {
		txbf->active  = 1;
	}

}

void wlc_txbf_pkteng_tx_stop(wlc_txbf_info_t *txbf, scb_t *scb)
{
	wlc_info_t * wlc;
	uint16 val;
	txbf_scb_info_t *bfi;

	wlc = txbf->wlc;
	ASSERT(wlc);

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(bfi);
	if (!bfi) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		WL_ERROR(("rev ge129 not supported\n"));
		return;
	}

	bfi->exp_en = FALSE;
	bfi->bfe_sts_cap = 0;

	/* clear the valid bit */
	wlc_suspend_mac_and_wait(wlc);
	val = wlc_read_shm(wlc, shm_addr(txbf->shm_base, C_BFI_BFRIDX_POS));
	val &= (~(1 << C_BFRIDX_VLD_NBIT));
	wlc_write_shm(wlc, shm_addr(txbf->shm_base, C_BFI_BFRIDX_POS), val);
	wlc_enable_mac(wlc);

	if ((txbf->shm_idx_bmp == 0) && txbf->active) {
		txbf->active = 0;
	}
}
#ifdef WL11AC
void
wlc_txbf_vht_upd_bfr_bfe_cap(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, uint32 *cap)
{
	wlc_info_t *wlc;
	uint8 bfr, bfe;

	wlc = txbf->wlc;
	BCM_REFERENCE(wlc);
	if ((wlc->txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg))) || WLC_PHY_AS_80P80(wlc, wlc->chanspec)) {
		bfr = bfe = 0;
	} else {
		bfr = txbf->bfr_capable;
		bfe = wlc_disable_bfe_for_smth(txbf, cfg) ? 0: txbf->bfe_capable;
	}
	wlc_vht_upd_txbf_cap(wlc->vhti, bfr, bfe, cap);
}
#endif /* WL11AC */

void
wlc_txbf_ht_upd_bfr_bfe_cap(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, uint32 *cap)
{
	wlc_info_t *wlc;
	uint8 bfr, bfe;

	wlc = txbf->wlc;
	BCM_REFERENCE(wlc);
	if ((wlc->txbf->virtif_disable &&
		(cfg != wlc_bsscfg_primary((cfg)->wlc) &&
		!BSSCFG_PSTA(cfg))) ||
		!txbf->ht_enable)
		bfr = bfe = 0;
	else {
		bfr = txbf->bfr_capable;
		bfe = txbf->bfe_capable;
	}

	wlc_ht_upd_txbf_cap(cfg, bfr, bfe, cap);

}

#ifdef WL_MUPKTENG
int
wlc_txbf_mupkteng_addsta(wlc_txbf_info_t *txbf, scb_t *scb, uint8 idx, uint8 nrxchain)
{
	txbf_scb_info_t *bfi;
	int i;
	char eabuf[ETHER_ADDR_STR_LEN];
	BCM_REFERENCE(eabuf);

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	if (bfi == NULL) {
		WL_ERROR(("%s: bfi NULL\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (D11REV_GE(txbf->wlc->pub->corerev, 128)) {
		WL_ERROR(("rev ge129 not supported\n"));
		return BCME_UNSUPPORTED;
	}

	/* check if scb match for any existing entries */
	for (i = 0; i < txbf->mu_max_links; i++) {
		if ((txbf->mu_scbs[i] != NULL) &&
				eacmp(&txbf->mu_scbs[i]->ea, &scb->ea) == 0) {
			WL_ERROR(("%s: bfi block @idx %d already exists for client %s\n",
				__FUNCTION__, i, bcm_ether_ntoa(&scb->ea, eabuf)));
			return BCME_ERROR;
		}
	}

	/* find a free index */
	if (isset(&txbf->mx_bfiblk_idx_bmp, idx)) {
		WL_ERROR(("%s: bfi block @idx %d already used by another client %s\n",
				__FUNCTION__, idx, bcm_ether_ntoa(&txbf->mu_scbs[i]->ea, eabuf)));
		return BCME_ERROR;
	}

	bfi->mx_bfiblk_idx = idx;
	wlc_txbf_mubfi_update(txbf, bfi, TRUE);
	txbf->mu_scbs[idx] = scb;
	bfi->flags = MU_BFE;
	bfi->bfe_nrx = nrxchain;
	bfi->bfe_sts_cap = 3;
	wlc_txbf_bfr_init(txbf, bfi);

	WL_ERROR(("%s add 0x%p %s shmx_index %d shmx_bmp 0x%04x\n", __FUNCTION__, scb,
		bcm_ether_ntoa(&scb->ea, eabuf), idx, txbf->mx_bfiblk_idx_bmp));

	return BCME_OK;
}

int
wlc_txbf_mupkteng_clrsta(wlc_txbf_info_t *txbf, scb_t *scb)
{
	bool found = FALSE;
	txbf_scb_info_t *bfi;
	int i;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 idx;
	wlc_info_t *wlc;

	BCM_REFERENCE(eabuf);
	wlc = txbf->wlc;
	BCM_REFERENCE(wlc);

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);

	if (D11REV_GE(wlc->pub->corerev, 128)) {
		WL_ERROR(("rev ge129 not supported\n"));
		return BCME_UNSUPPORTED;
	}

	/* find a free index */
	for (i = 0; i < txbf->mu_max_links; i++) {
		if (txbf->mu_scbs[i] && (eacmp(&txbf->mu_scbs[i]->ea, &scb->ea) == 0)) {
			/* check if scb match for any existing entries */
			WL_ERROR(("wl%d: %s, bfi block exist for client %s\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf)));
			found = TRUE;
			break;
		}
	}

	if (!found) {
		WL_ERROR(("%d: %s bfi block doesn't exist for client %s\n", wlc->pub->unit,
			__FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
		return BCME_ERROR;
	}

	idx = bfi->mx_bfiblk_idx;
	wlc_txbf_invalidate_bfridx(txbf, bfi, idx);
	bfi->flags = 0;
	bfi->bfe_nrx = 0;
	bfi->bfe_sts_cap = 0;

	wlc_txbf_mubfi_update(txbf, bfi, FALSE);
	ASSERT(scb == txbf->mu_scbs[idx]);
	txbf->mu_scbs[idx] = NULL;
	bfi->mx_bfiblk_idx = BF_SHM_IDX_INV;

	WL_TXBF(("%s clear 0x%p %s shmx_index %d shmx_bmp 0x%04x\n", __FUNCTION__, scb,
		bcm_ether_ntoa(&scb->ea, eabuf), idx, txbf->mx_bfiblk_idx_bmp));

	return BCME_OK;
}
#endif /* WL_MUPKTENG */

void
wlc_txbf_fill_link_entry(wlc_txbf_info_t *txbf, wlc_bsscfg_t *cfg, scb_t *scb,
	d11linkmem_entry_t *link_entry)
{
	txbf_scb_info_t *bfi;

	ASSERT(link_entry != NULL);

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	if (bfi) {
		/* BFI link established / space for new BFI link */
		link_entry->BFIConfig0 = bfi->BFIConfig0;
		link_entry->BFIConfig1 = bfi->BFIConfig1;
		link_entry->BFRStat0 = bfi->BFRStat0;
	}
	/* TWT uses the BFIConfig1 field to specify TWT for a destination, call it now */
	wlc_twt_fill_link_entry(txbf->wlc->twti, scb, link_entry);
}

void
wlc_txbf_link_entry_dump(d11linkmem_entry_t *link_entry, bcmstrbuf_t *b)
{
	bcm_bprintf(b, "\t\tBFIConfig0:0x%04x BFIConfig1:0x%04x BFRStat0:0x%04x\n",
		link_entry->BFIConfig0, link_entry->BFIConfig1, link_entry->BFRStat0);
}

uint8
wlc_txbf_get_txbf_tx(wlc_txbf_info_t *txbf)
{
	return txbf->mode;
}

void
wlc_txbf_set_mu_sounding_period(wlc_txbf_info_t *txbf, uint16 val)
{
	txbf->mu_sounding_period = val;

	if (!txbf->wlc->clk) {
		return;
	}
	wlc_txbf_mutimer_update(txbf, FALSE);
}

uint16
wlc_txbf_get_mu_sounding_period(wlc_txbf_info_t *txbf)
{
	return txbf->mu_sounding_period;
}

/* This function is used for implicit beamforming calibration */
void wlc_forcesteer_gerev129(wlc_info_t *wlc, uint8 enable)
{
	wlc_txbf_info_t *txbf;
	scb_t *scb;
	txbf_scb_info_t *bfi;

	ASSERT(wlc);
	txbf = wlc->txbf;
	scb = (scb_t *)WLC_RLM_SPECIAL_RATE_SCB(wlc);
	ASSERT(txbf);
	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	ASSERT(bfi);
	if (!bfi || (WLC_BITSCNT(wlc->stf->txchain) == 1)) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}
#ifdef WL_PSMX
	if (D11REV_GE(wlc->pub->corerev, 129)) {
		uint8 txv_idx, snd_type;
		uint16 bfrcfg0, bfrStats, bw = 0;
		uint16 val, addr, bfridx, bfi_blk;

		/* for implicit cal, we put the report in index 127 */
		txv_idx = 0x7f;
		bfridx = WLC_RLM_SPECIAL_RATE_IDX;
		bfi_blk = txbf->mx_bfiblk_base + bfridx * BFI_BLK_SIZE(wlc->pub->corerev);
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
		addr = shm_addr(bfi_blk, C_BFI_IDX_POS);
		val = wlc_read_shmx(wlc, addr);
		/* set txv idx */
		val |= (txv_idx << C_BFI_TXVIDX_NBIT);
		/* set report ready */
		val = enable? val | (1 << C_BFI_TXVRDY_NBIT): val & ~(1 << C_BFI_TXVRDY_NBIT);
		wlc_write_shmx(wlc, addr, val);
		wlc_bmac_enable_macx(wlc->hw);

		bfi->exp_en = TRUE;

		/* sounding type is set to VHT */
		snd_type = 1;
		bfrcfg0 = (snd_type << C_LNK_STY_NBIT);
		bfi->BFIConfig0 = bfrcfg0;

		bw = (wlc->chanspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT;
		bw -= (WL_CHANSPEC_BW_20 >> WL_CHANSPEC_BW_SHIFT);
		bfrStats = (bw << C_LNK_BFRLBW_NBIT) | (0 << C_LNK_BFRSB_NBIT) |
			(wlc->stf->txchain << C_LNK_BFRCMASK_NBIT);
		bfi->BFRStat0 = bfrStats;
	}
#endif /* WL_PSMX */
	if (!txbf->active) {
		txbf->active  = 1;
	}

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_upd_lmem_int(wlc, scb, FALSE /* clr_txbf_sts=0 in mreq */);
	}
}

uint8
wlc_txbf_get_bfr_cap(wlc_txbf_info_t *txbf)
{
	return txbf->bfr_capable;
}

uint8
wlc_txbf_get_bfe_cap(wlc_txbf_info_t *txbf)
{
	return txbf->bfe_capable;
}

bool
wlc_txbf_is_hemmu_enab(wlc_txbf_info_t *txbf, scb_t *scb)
{
	txbf_scb_info_t *bfi;

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	if ((bfi->flags & HE_MU_BFE) && HE_MMU_ENAB(txbf->pub)) {
		return TRUE;
	} else {
		return FALSE;
	}
}

void
wlc_txbf_tbcap_update(wlc_txbf_info_t *txbf, scb_t *scb)
{
	txbf_scb_info_t *bfi;
	wlc_info_t *wlc = txbf->wlc;
	bool trig_su_fb;

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	if (!bfi || !(HE_DLMU_ENAB(txbf->pub)) || !SCB_HE_CAP(scb)) {
		return;
	}

	bfi->BFIConfig0 &= ~(1 << C_LNK_TBCAP_NBIT);
	bfi->BFIConfig0 &= ~(1 << C_LNK_MUCAP_NBIT);
	bfi->BFIConfig0 &= ~(1 << C_LNK_CQICAP_NBIT);

	if (SCB_DLOFDMA(scb)) {
		bfi->BFIConfig0 &= ~(1 << C_LNK_MUCAP_NBIT);

		trig_su_fb = getbits((uint8 *)&bfi->he_cap, sizeof(bfi->he_cap),
			HE_PHY_TRG_SU_BFM_FEEDBACK_IDX,
			HE_PHY_TRG_SU_BFM_FEEDBACK_FSZ);

		if (trig_su_fb) {
			bfi->BFIConfig0 |= (1 << C_LNK_TBCAP_NBIT);
		} else {
			bfi->BFIConfig0 &= ~(1 << C_LNK_TBCAP_NBIT);
		}

		if (bfi->flags & HE_CQI_BFE) {
			bfi->BFIConfig0 |= (1 << C_LNK_CQICAP_NBIT);
		} else {
			bfi->BFIConfig0 &= ~(1 << C_LNK_CQICAP_NBIT);
		}
	} else if (SCB_HEMMU(scb)) {
		bfi->BFIConfig0 |= (1 << C_LNK_TBCAP_NBIT);
		bfi->BFIConfig0 |= (1 << C_LNK_MUCAP_NBIT);
	}

	if (WLC_TXC_ENAB(wlc)) {
		wlc_txc_inv(wlc->txc, scb);
	}
	wlc_ratelinkmem_update_link_entry(wlc, scb);
}

#if defined(BCMDBG)
/* Returns true if the bficonfig is configured correctly according to mu_type */
int
wlc_txbf_tbcap_check(wlc_txbf_info_t *txbf, scb_t *scb, uint8 mu_type)
{
	txbf_scb_info_t *bfi;
	int ret = BCME_ERROR;

	bfi = (txbf_scb_info_t *)TXBF_SCB_INFO(txbf, scb);
	if (!bfi || !(HE_DLMU_ENAB(txbf->pub)) || !SCB_HE_CAP(scb)) {
		return ret;
	}

	/* for hemmu, tbcap and mucap must be set */
	if (mu_type == HEMMU) {
		if (!(bfi->BFIConfig0 & (1 << C_LNK_MUCAP_NBIT) &&
			(bfi->BFIConfig0 & (1 << C_LNK_TBCAP_NBIT)))) {
			return ret;
		}
	} else if (mu_type == DLOFDMA) {
		if ((bfi->BFIConfig0 & (1 << C_LNK_MUCAP_NBIT))) {
			return ret;
		}
	}

	return BCME_OK;
}
#endif // endif

static void
wlc_txbf_init_exp_vht_ht_cap(wlc_txbf_info_t *txbf, scb_t *scb, txbf_scb_info_t *bfi)
{
	wlc_info_t *wlc = txbf->wlc;
	wlc_pub_t *pub = wlc->pub;

	BCM_REFERENCE(pub);
#ifdef WL11AC
	if (SCB_VHT_CAP(scb)) {
		uint16 vht_flags;
		vht_flags = wlc_vht_get_scb_flags(wlc->vhti, scb);
		if (vht_flags & (SCB_SU_BEAMFORMEE | SCB_MU_BEAMFORMEE)) {
			WL_TXBF(("wl%d %s: sta has VHT %s BFE cap\n",
				pub->unit, __FUNCTION__,
				(vht_flags & SCB_MU_BEAMFORMEE) ? "MU":"SU"));

			ASSERT(vht_flags & SCB_SU_BEAMFORMEE);

			bfi->bfe_capable = TXBF_SU_BFE_CAP;
			if (vht_flags & SCB_MU_BEAMFORMEE)
				bfi->bfe_capable |= TXBF_MU_BFE_CAP;
		} else {
			bfi->bfe_capable &= ~(TXBF_SU_BFE_CAP | TXBF_MU_BFE_CAP);
		}

		if (vht_flags & (SCB_SU_BEAMFORMER | SCB_MU_BEAMFORMER)) {
			WL_TXBF(("wl%d %s: sta has VHT %s BFR cap\n",
				pub->unit, __FUNCTION__,
				(vht_flags & SCB_MU_BEAMFORMER) ? "MU":"SU"));

			ASSERT(vht_flags & SCB_SU_BEAMFORMER);

			bfi->bfr_capable = TXBF_SU_BFR_CAP;
			if (vht_flags & SCB_MU_BEAMFORMER)
				bfi->bfr_capable |= TXBF_MU_BFR_CAP;
		} else {
			bfi->bfr_capable &= ~(TXBF_SU_BFR_CAP | TXBF_MU_BFR_CAP);
		}

		bfi->bfe_sts_cap = ((bfi->vht_cap &
			VHT_CAP_INFO_NUM_BMFMR_ANT_MASK) >> VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT);

		bfi->bfe_nrx = VHT_MCS_SS_SUPPORTED(4, scb->rateset.vht_mcsmap) ? 4 :
				(VHT_MCS_SS_SUPPORTED(3, scb->rateset.vht_mcsmap) ? 3 :
				(VHT_MCS_SS_SUPPORTED(2, scb->rateset.vht_mcsmap) ? 2 : 1));

	} else
#endif /* WL11AC */
	 if (SCB_HT_CAP(scb) && txbf->ht_enable) {
		if (scb->flags3 & SCB3_HT_BEAMFORMEE) {
			WL_TXBF(("wl%d %s: sta has HT BFE cap\n", pub->unit, __FUNCTION__));
			bfi->bfe_capable = TXBF_SU_BFE_CAP;
		}
		if (scb->flags3 & SCB3_HT_BEAMFORMER) {
			WL_TXBF(("wl%d %s: sta has HT BFR cap\n", pub->unit, __FUNCTION__));
			bfi->bfr_capable = TXBF_SU_BFR_CAP;
		}
		bfi->bfe_sts_cap = ((bfi->ht_txbf_cap &
			HT_CAP_TXBF_CAP_C_BFR_ANT_MASK) >> HT_CAP_TXBF_CAP_C_BFR_ANT_SHIFT);
	}
}
#endif /* WL_BEAMFORMING */
