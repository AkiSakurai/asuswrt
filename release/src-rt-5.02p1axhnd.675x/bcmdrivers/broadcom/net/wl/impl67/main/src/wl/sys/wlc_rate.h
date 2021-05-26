/*
 * Common OS-independent driver header for rate management.
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
 * $Id: wlc_rate.h 783272 2020-01-21 09:05:26Z $
 */

#ifndef _WLC_RATE_H_
#define _WLC_RATE_H_

#include <typedefs.h>
#include <hndd11.h>
#include <wlioctl.h>
#include <wlc_types.h>
#include <bcmutils.h>
#include <802.11ax.h>
#include <bcmwifi_rspec.h>
#include <bcmwifi_rates.h>

typedef struct wlc_rateset {
	uint32	count;			/**< number of (legacy) rates in rates[] */
	uint8	rates[WLC_NUMRATES];	/**< rates in 500kbps units w/hi bit set if basic */
	uint8	mcs[MCSSET_LEN];	/**< supported HT mcs index bit map */
	uint16  vht_mcsmap;		/**< supported VHT mcs nss bit map */
	uint16  vht_mcsmap_prop;	/**< supported prop VHT mcs nss bit map */
	/* The format of the mcs_nss maps below is defined by the HE standard, as an 'HE-MCS map'.
	 * Two bits per stream: d[1:0] is for stream #0, d[3:2] for stream #1, etc.
	 * 'b00: mandatory minimum support of MCS 0 through 7
	 * 'b01: support of MCS 0 through 8
	 * 'b10: support of MCS 0 through 9
	 * 'b11: no HE MCS'es supported
	 */
	uint16  he_bw80_tx_mcs_nss;	/**< supported HE Tx mcs nss set for BW <=80Mhz */
	uint16  he_bw80_rx_mcs_nss;	/**< supported HE Rx mcs nss set for BW <=80Mhz */
	uint16  he_bw160_tx_mcs_nss;	/**< supported HE Tx mcs nss set for the BW 160Mhz */
	uint16  he_bw160_rx_mcs_nss;	/**< supported HE Rx mcs nss set for the BW 160Mhz */
	uint16  he_bw80p80_tx_mcs_nss;	/**< supported HE Tx mcs nss set for the BW 80p80Mhz */
	uint16  he_bw80p80_rx_mcs_nss;	/**< supported HE Rx mcs nss set for the BW 80p80Mhz */
} wlc_rateset_t;

extern const struct wlc_rateset cck_ofdm_mimo_rates;
extern const struct wlc_rateset ofdm_mimo_rates;
extern const struct wlc_rateset cck_ofdm_40bw_mimo_rates;
extern const struct wlc_rateset ofdm_40bw_mimo_rates;
extern const struct wlc_rateset cck_ofdm_rates;
extern const struct wlc_rateset ofdm_rates;
extern const struct wlc_rateset cck_rates;
extern const struct wlc_rateset gphy_legacy_rates;
extern const struct wlc_rateset wlc_lrs_rates;
extern const struct wlc_rateset rate_limit_1_2;

typedef struct mcs_info {
	uint32 phy_rate_20;	/**< phy rate in kbps [20Mhz] */
	uint32 phy_rate_40;	/**< phy rate in kbps [40Mhz] */
	uint32 phy_rate_20_sgi;	/**< phy rate in kbps [20Mhz] with SGI */
	uint32 phy_rate_40_sgi;	/**< phy rate in kbps [40Mhz] with SGI */
	uint8  tx_phy_ctl3;	/**< phy ctl byte 3, code rate, modulation type, # of streams */
	uint8  leg_ofdm;	/**< matching legacy ofdm rate in 500bkps */
} mcs_info_t;

#if defined(WLPROPRIETARY_11N_RATES) /* Broadcom proprietary rate support for 11n */

#define WLC_MAXMCS	102	/**< max valid mcs index */

#else /* WLPROPRIETARY_11N_RATES */

#define WLC_MAXMCS	32	/**< max valid mcs index */

#endif /* WLPROPRIETARY_11N_RATES */

#define N_11N_MCS_VALUES	33	/**< Number of standardized MCS values in 802.11n (HT) */
#define MCS_TABLE_SIZE		N_11N_MCS_VALUES

#define PROP11N_2_VHT_MCS(rspec) (((rspec) & 0x1) ? 8 : 9) /* odd propr HT rates use 3/4 coding */

extern const mcs_info_t mcs_table[];

#define MCS_INVALID	0xFF
#define MCS_CR_MASK	0x07	/**< Code Rate bit mask */
#define MCS_MOD_MASK	0x38	/**< Modulation bit shift */
#define MCS_MOD_SHIFT	3	/**< MOdulation bit shift */
#define MCS_TXS_MASK   	0xc0	/**< num tx streams - 2 bit mask */
#define MCS_TXS_SHIFT	6	/**< num tx streams - 2 bit shift */
#define MCS_CR(_mcs)	(mcs_table[_mcs].tx_phy_ctl3 & MCS_CR_MASK)
#define MCS_MOD(_mcs)	((mcs_table[_mcs].tx_phy_ctl3 & MCS_MOD_MASK) >> MCS_MOD_SHIFT)
#define MCS_TXS(_mcs)	((mcs_table[_mcs].tx_phy_ctl3 & MCS_TXS_MASK) >> MCS_TXS_SHIFT)
#define MCS_RATE(_mcs, _is40, _sgi)	(_sgi ? \
	(_is40 ? mcs_table[_mcs].phy_rate_40_sgi : mcs_table[_mcs].phy_rate_20_sgi) : \
	(_is40 ? mcs_table[_mcs].phy_rate_40 : mcs_table[_mcs].phy_rate_20))

#if defined(WLPROPRIETARY_11N_RATES) /* Broadcom proprietary rate support for 11n */
#define VALID_MCS(_mcs)	(((_mcs) < N_11N_MCS_VALUES) || IS_PROPRIETARY_11N_MCS(_mcs))
#else
#define VALID_MCS(_mcs)	(((_mcs) < N_11N_MCS_VALUES))
#endif // endif

/* Macros to use with the legacy rates in rateset */
#define	WLC_RATE_FLAG	0x80	/**< basic rate flag */
#define	RATE_MASK	0x7f	/* Rate value mask w/o basic rate flag */
#define	RATE_MASK_FULL	0xff	/* Rate value mask with basic rate flag */

#define WLC_STD_MAX_VHT_MCS	9	/**< 11ac std VHT MCS 0-9 */

#define IS_PROPRIETARY_VHT_MCS_10_11(mcs) ((mcs) == 10 || (mcs) == 11)

/* mcsmap with MCS0-9 for Nss = 4 */
#define VHT_CAP_MCS_MAP_0_9_NSS4 \
	        ((VHT_CAP_MCS_MAP_0_9 << VHT_MCS_MAP_GET_SS_IDX(1)) | \
	         (VHT_CAP_MCS_MAP_0_9 << VHT_MCS_MAP_GET_SS_IDX(2)) | \
	         (VHT_CAP_MCS_MAP_0_9 << VHT_MCS_MAP_GET_SS_IDX(3)) | \
	         (VHT_CAP_MCS_MAP_0_9 << VHT_MCS_MAP_GET_SS_IDX(4)))

#define VHT_PROP_MCS_MAP_10_11_NSS4 \
	        ((VHT_PROP_MCS_MAP_10_11 << VHT_MCS_MAP_GET_SS_IDX(1)) | \
	         (VHT_PROP_MCS_MAP_10_11 << VHT_MCS_MAP_GET_SS_IDX(2)) | \
	         (VHT_PROP_MCS_MAP_10_11 << VHT_MCS_MAP_GET_SS_IDX(3)) | \
	         (VHT_PROP_MCS_MAP_10_11 << VHT_MCS_MAP_GET_SS_IDX(4)))

#define MAX_HT_RATES	8	/* max no. of ht rates supported (0-7) */
#define MAX_VHT_RATES	12	/* max no. of vht rates supported (0-9 and prop 10-11) */
#define MAX_HE_RATES	12	/* max no. of HE rates supported */

#define NTXRATE		64	/**< # tx MPDUs rate is reported for */
#define NVITXRATE	32	/**< # Video tx MPDUs rate is reported for */

/* bw define used by rate modules */
#define BW_20MHZ	(WL_RSPEC_BW_20MHZ >> WL_RSPEC_BW_SHIFT)
#define BW_40MHZ	(WL_RSPEC_BW_40MHZ >> WL_RSPEC_BW_SHIFT)
#define BW_80MHZ	(WL_RSPEC_BW_80MHZ >> WL_RSPEC_BW_SHIFT)
#define BW_160MHZ	(WL_RSPEC_BW_160MHZ >> WL_RSPEC_BW_SHIFT)

#define BW_MAXMHZ	BW_160MHZ
#define BW_MINMHZ	BW_20MHZ

#define RSPEC2BW(rspec)	((WL_RSPEC_BW_MASK & (rspec)) >> WL_RSPEC_BW_SHIFT)

#define RSPEC2ENCODING(rspec) (((rspec) & WL_RSPEC_ENCODING_MASK) >> WL_RSPEC_ENCODING_SHIFT)

#define RSPEC_ACTIVE(rspec) \
	(!RSPEC_ISLEGACY(rspec) || \
	 ((rspec) & (WL_RSPEC_OVERRIDE_RATE | WL_RSPEC_OVERRIDE_MODE | WL_RSPEC_LEGACY_RATE_MASK)))

#define PLCP3_ISLDPC(plcp)	(((plcp) & PLCP3_LDPC) != 0)
#define PLCP3_ISSGI(plcp)	(((plcp) & PLCP3_SGI) != 0)
#define PLCP3_ISSTBC(plcp)	(((plcp) & PLCP3_STC_MASK) != 0)
#define PLCP3_STC_MASK          0x30
#define PLCP3_STC_SHIFT         4
#define PLCP3_LDPC              0x40
#define PLCP3_SGI               0x80

#define VHT_PLCP3_ISSGI(plcp)	(((plcp) & VHT_SIGA2_GI_SHORT) != 0)
#define VHT_PLCP0_ISSTBC(plcp)	(((plcp) & VHT_SIGA1_STBC) != 0)
#define VHT_PLCP0_BW(plcp)	((plcp) & VHT_SIGA1_BW_MASK)

#define RATE_ISOFDM(r)	RATE_INFO_RATE_ISOFDM((r) & RATE_MASK)
#define RATE_ISCCK(r)	RATE_INFO_RATE_ISCCK((r) & RATE_MASK)

#define HIGHEST_SINGLE_STREAM_MCS	7 /* MCS values greater than this enable multiple streams */

#if defined(WLPROPRIETARY_11N_RATES) /* Broadcom proprietary rate support for 11n */
#define IS_SINGLE_STREAM(mcs)	(GET_11N_MCS_NSS(mcs) == 1)
#else
#define IS_SINGLE_STREAM(mcs)	(((mcs) <= HIGHEST_SINGLE_STREAM_MCS) || ((mcs) == 32))
#endif	/* WLPROPRIETARY_11N_RATES */

/* create ratespecs */

#define HT_RSPEC_BW(mcs, bw) \
	(HT_RSPEC(mcs) | (((bw) << WL_RSPEC_BW_SHIFT) & WL_RSPEC_BW_MASK))
#define VHT_RSPEC_BW(mcs, nss, bw) \
	(VHT_RSPEC(mcs, nss) | (((bw) << WL_RSPEC_BW_SHIFT) & WL_RSPEC_BW_MASK))

/* Rates specified in wlc_rateset_filter() */
#define WLC_RATES_CCK_OFDM	0
#define WLC_RATES_CCK		1
#define WLC_RATES_OFDM		2

/* mcsallow flags in wlc_rateset_filter() and wlc_filter_rateset */
#define WLC_MCS_ALLOW_HT			0x1
#define WLC_MCS_ALLOW_VHT			0x2
#define WLC_MCS_ALLOW_PROP_HT			0x4 /**< Broadcom proprietary 11n rates */
#define WLC_MCS_ALLOW_1024QAM			0x8 /**< Proprietary VHT rates */
#define WLC_MCS_ALLOW_HE			0x10

/* per-bw sgi enable */
#define SGI_BW20		1
#define SGI_BW40		2
#define SGI_BW80		4
#define SGI_BW160		8

#define WLC_RSPEC_OVERRIDE_ANY(wlc)	(\
	wlc->bandstate[BAND_2G_INDEX]->rspec_override || \
	wlc->bandstate[BAND_2G_INDEX]->mrspec_override || \
	wlc->bandstate[BAND_5G_INDEX]->rspec_override || \
	wlc->bandstate[BAND_5G_INDEX]->mrspec_override || \
	wlc->bandstate[BAND_6G_INDEX]->rspec_override || \
	wlc->bandstate[BAND_6G_INDEX]->mrspec_override)

/* ************ HT definitions ************* */
#define WLC_RSPEC_HT_MCS_MASK		0x07	/**< HT MCS value mask in rate field */
#define WLC_RSPEC_HT_PROP_MCS_MASK	0x7F	/**< (>4) bits are required for prop MCS */
#define WLC_RSPEC_HT_NSS_MASK		0x78	/**< HT Nss value mask in rate field */
#define WLC_RSPEC_HT_NSS_SHIFT		3	/**< HT Nss value shift in rate field */

/* ************* HE definitions. ************* */

/*
 * Note - HE Rates BITMAP is defined in wlioctl.h
 *
 * - WL_HE_CAP_MCS_0_7_MAP	0x00ff
 * - WL_HE_CAP_MCS_0_9_MAP	0x03ff
 * - WL_HE_CAP_MCS_0_11_MAP	0x0fff
 */
/* HE Rates BITMAP */

#define IS_HE_MCS_10_11(mcs) ((mcs) == 10 || (mcs) == 11)

/* Map the mcs code to mcs bitmap */
#define HE_MAX_MCS_TO_MCS_MAP(max_mcs) \
	((max_mcs == HE_CAP_MAX_MCS_0_7) ? WL_HE_CAP_MCS_0_7_MAP : \
	 (max_mcs == HE_CAP_MAX_MCS_0_9) ? WL_HE_CAP_MCS_0_9_MAP : \
	 (max_mcs == HE_CAP_MAX_MCS_0_11) ? WL_HE_CAP_MCS_0_11_MAP : \
	 0)

#define HE_MCS_MAP_TO_MAX_MCS(mcs_map) \
	((mcs_map == WL_HE_CAP_MCS_0_7_MAP) ? HE_CAP_MAX_MCS_0_7 : \
	 (mcs_map == WL_HE_CAP_MCS_0_9_MAP) ? HE_CAP_MAX_MCS_0_9 : \
	 (mcs_map == WL_HE_CAP_MCS_0_11_MAP) ? HE_CAP_MAX_MCS_0_11 : \
	 HE_CAP_MAX_MCS_NONE)

/**
 * Note : Spec defined codes (IEEE Draft P802.11ax D1.4 Text following figure 9-589cn-Rx HE-MCS Map
 * and Tx HE-MCS Map subfields and Basic HE-MCS And NSS Set field) are present in 802.11ax.h as :
 * - HE_CAP_MAX_MCS_0_7    =  0
 * - HE_CAP_MAX_MCS_0_9    =  1
 * - HE_CAP_MAX_MCS_0_11   =  2
 * - HE_CAP_MAX_MCS_NONE   =  3
 */

/**
 * Store mcs map for all NSS in a compact form:
 *
 * bit[0:1] mcs code for NSS 1
 * bit[2:3] mcs code for NSS 2
 * ...
 * bit[14:15] mcs code for NSS 8
 *
 * 2 bits are used for encoding each MCS per NSS map (HE MAX MCS MAP is 16 bits)
 */

/**
 * User defined Code for indicating that none of the HE MCS' are supported. This user
 * defined code shall only be used within the driver for book keeping and other activities.
 */
#define HE_CAP_MAX_MCS_NONE_ALL (\
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(1)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(2)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(3)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(4)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(5)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(6)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(7)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(8)))

/* mcsmap with MCS0-11 for Nss = 4 */
#define HE_CAP_MAX_MCS_MAP_0_11_NSS4 (\
		(HE_CAP_MAX_MCS_0_11 << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(1)) | \
		(HE_CAP_MAX_MCS_0_11 << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(2)) | \
		(HE_CAP_MAX_MCS_0_11 << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(3)) | \
		(HE_CAP_MAX_MCS_0_11 << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(4)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(5)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(6)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(7)) | \
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(8)))

/* max no. of HE rates supported (mcs 0-11) */
#define MIN_HE_MCS		0
#define MAX_HE_MCS		11
#define MAX_HE_RATES		12

#define HE_MAX_MCS_TO_INDEX(max_mcs) \
	((max_mcs == HE_CAP_MAX_MCS_0_11) ? 11 : \
	(max_mcs == HE_CAP_MAX_MCS_0_9) ? 9 : \
	(max_mcs == HE_CAP_MAX_MCS_0_7) ? 7 : \
	7)
#define HE_MCS_MAP_SET_MCS_PER_SS(nss, mcs, mcsmap) \
	do { \
	 (mcsmap) &= (~(HE_CAP_MAX_MCS_MASK << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(nss))); \
	 (mcsmap) |= (((mcs) & HE_CAP_MAX_MCS_MASK) << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(nss)); \
	} while (0)

#define HE_CAP_MAX_MCS_NSS_GET_SS_IDX(nss) (((nss) - 1) * HE_CAP_MAX_MCS_SIZE)

#define HE_MCS_SS_SUPPORTED(nss, mcsMap) \
		 (HE_CAP_MAX_MCS_NSS_GET_MCS((nss), (mcsMap)) != HE_CAP_MAX_MCS_NONE)

/* Get the max ss supported from the mcs map */
#define HE_MAX_SS_SUPPORTED(mcsMap) \
	HE_MCS_SS_SUPPORTED(8, mcsMap) ? 8 : \
	HE_MCS_SS_SUPPORTED(7, mcsMap) ? 7 : \
	HE_MCS_SS_SUPPORTED(6, mcsMap) ? 6 : \
	HE_MCS_SS_SUPPORTED(5, mcsMap) ? 5 : \
	HE_MCS_SS_SUPPORTED(4, mcsMap) ? 4 : \
	HE_MCS_SS_SUPPORTED(3, mcsMap) ? 3 : \
	HE_MCS_SS_SUPPORTED(2, mcsMap) ? 2 : \
	HE_MCS_SS_SUPPORTED(1, mcsMap) ? 1 : 0
/* ratespec utils */

/* Number of spatial streams, Nss, specified by the ratespec */
extern uint wlc_ratespec_nss(ratespec_t rspec);
/* Modulation without spatial stream info */
extern uint8 wlc_ratespec_mcs(ratespec_t rspec);

/* Number of space time streams, Nss + Nstbc expansion, specified by the ratespec */
extern uint wlc_ratespec_nsts(ratespec_t rspec);

/* Number of Tx chains, NTx, specified by the ratespec */
extern uint wlc_ratespec_ntx(ratespec_t rspec);

/* bw in Mhz, specified by the ratespec */
extern uint8 wlc_ratespec_bw(ratespec_t rspec);

extern uint8 wlc_rate_get_single_stream_mcs(uint mcs);
extern void wlc_rate_clear_prop_11n_mcses(uint8 mcs_bitmap[]);
#define RSPEC_REFERENCE_RATE(rspec) wlc_rate_rspec_reference_rate(rspec)
#define WLC_RATE_500K_TO_KBPS(rate) ((rate) * 500)      /* convert 500kbps to kbps */

/* Calculate the reference rate for response frame basic rate calculation */
extern uint wlc_rate_rspec_reference_rate(ratespec_t rspec);

/* Legacy reference rate for HT/VHT rates */
extern uint wlc_rate_ht_basic_reference(uint ht_mcs);
extern uint wlc_rate_vht_basic_reference(uint vht_mcs);
extern uint wlc_rate_he_basic_reference(uint he_mcs);

/* sanitize, and sort a rateset with the basic bit(s) preserved, validate rateset */
extern bool wlc_rate_hwrs_filter_sort_validate(struct wlc_rateset *rs,
                                               const struct wlc_rateset *hw_rs,
                                               bool check_brate, uint8 txstreams);
/* copy rateset src to dst as-is (no masking or sorting) */
extern void wlc_rateset_copy(const struct wlc_rateset *src, struct wlc_rateset *dst);

/* would be nice to have these documented ... */
extern ratespec_t wlc_recv_compute_rspec(uint corerev, d11rxhdr_t *rxh, uint8 *plcp);
extern void wlc_rateset_filter(struct wlc_rateset *src, struct wlc_rateset *dst,
	bool basic_only, uint8 rates, uint xmask, uint8 mcsallow);
extern void wlc_rateset_ofdm_fixup(struct wlc_rateset *rs);
extern void wlc_rateset_default(wlc_info_t *wlc, struct wlc_rateset *rs_tgt,
	const struct wlc_rateset *rs_hw, uint phy_type, int bandtype, bool cck_only, uint rate_mask,
	uint8 mcsallow, uint8 bw, uint8 txstreams);
extern int16 wlc_rate_legacy_phyctl(uint rate);

extern int wlc_dump_rateset(const char *name, struct wlc_rateset *rateset, struct bcmstrbuf *b);
extern void wlc_dump_rspec(void *context, ratespec_t rspec, struct bcmstrbuf *b);

extern int wlc_dump_mcsset(const char *name, uchar *mcs, struct bcmstrbuf *b);
extern void wlc_dump_vht_mcsmap(const char *name, uint16 mcsmap, struct bcmstrbuf *b);
extern void wlc_dump_vht_mcsmap_prop(const char *name, uint16 mcsmap, struct bcmstrbuf *b);
extern void wlc_dump_he_rateset(const char *name, wlc_rateset_t *rateset, struct bcmstrbuf *b);

void wlc_rateset_dump(wlc_rateset_t *rs, struct bcmstrbuf *b);

extern ratespec_t wlc_get_highest_rate(struct wlc_rateset *rateset, uint8 bw, bool sgi,
	bool vht_ldpc, uint8 vht_ratemask, uint8 txstreams);

extern void wlc_rateset_ht_mcs_upd(struct wlc_rateset *rs, uint8 txstreams);
extern void wlc_rateset_ht_mcs_clear(struct wlc_rateset *rateset);
extern void wlc_rateset_ht_mcs_build(struct wlc_rateset *rateset, uint8 txstreams);
extern void wlc_rateset_ht_bw_mcs_filter(struct wlc_rateset *rateset, uint8 bw);
extern uint8 wlc_rate_ht_to_nonht(uint8 mcs);
extern uint16 wlc_get_valid_vht_mcsmap(uint8 mcscode, uint8 prop_mcscode, uint8 bw, bool ldpc,
	uint8 nss, uint8 ratemask);
#ifdef WL11AC
extern void wlc_rateset_vhtmcs_upd(struct wlc_rateset *rs, uint8 nstreams);
extern void wlc_rateset_vhtmcs_build(struct wlc_rateset *rateset, uint8 txstreams);
extern void wlc_rateset_prop_vhtmcs_upd(wlc_rateset_t *rs, uint8 nstreams);
extern void wlc_rateset_prop_vhtmcs_build(struct wlc_rateset *rateset, uint8 txstreams);
extern uint16 wlc_rateset_vht_filter_mcsmap(uint16 vht_mcsmap, uint8 nstreams, uint8 mcsallow);
#ifdef WL11AX
extern void wlc_rateset_hemcs_build(wlc_rateset_t *rateset, uint8 nstreams);
extern uint16 wlc_he_rateset_intersection(uint16 set1, uint16 set2);
ratespec_t BCMFASTPATH wlc_hetb_rspec(uint corerev, d11rxhdr_t *rxh, uint8 *plcp);
#endif /* WL11AX */
extern void wlc_rateset_he_none_all(wlc_rateset_t *rs);
extern void wlc_rateset_he_cp(wlc_rateset_t *rs_dst, const wlc_rateset_t *rs_src);
#endif /* WL11AC */

#endif	/* _WLC_RATE_H_ */
