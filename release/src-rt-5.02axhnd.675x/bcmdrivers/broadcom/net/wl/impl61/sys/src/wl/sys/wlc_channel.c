/*
 * Common interface to channel definitions.
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
 * $Id: wlc_channel.c 778376 2019-08-29 13:25:29Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <802.11.h>
#include <wpa.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#ifdef BCMSUP_PSK
#include <eapol.h>
#include <bcmwpa.h>
#endif /* BCMSUP_PSK */
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_rsdb.h>
#include <wlc_bmac.h>
#include <wlc_phy_hal.h>
#include <phy_radar_api.h>
#include <phy_utils_api.h>
#include <phy_rxgcrs_api.h>
#include <phy_tpc_api.h>
#include <wl_export.h>
#include <wlc_stf.h>
#include <wlc_channel.h>
#ifdef WLC_TXCAL
#include <wlc_calload.h>
#endif // endif
#ifdef WL_SARLIMIT
#include <phy_tpc_api.h>
#include <wlc_sar_tbl.h>
#endif /* WL_SARLIMIT */
#include "wlc_clm_data.h"
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_dfs.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_reg.h>
#ifdef WLOLPC
#include <wlc_olpc_engine.h>
#endif /* WLOLPC */
#include <wlc_ht.h>
#include <wlc_vht.h>
#include <wlc_objregistry.h>
#include <wlc_event_utils.h>
#include <wlc_srvsdb.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#ifdef WLC_TXPWRCAP
#include <phy_txpwrcap_api.h>
#endif // endif
#ifdef DONGLEBUILD
#include <rte_trap.h>
#endif /* DONGLEBUILD */
#include <phy_radio_api.h>
#include <wlc_bsscfg.h>
#ifdef WL_MODESW
#include <wlc_modesw.h>
#endif /* WL_MODESW */
#include <bcmwifi_rclass.h>
#if defined(WL_AIR_IQ)
#include <wlc_scan.h>
#endif // endif

#define	INVCHANNEL		255	/* invalid channel */

#ifdef WLC_TXPWRCAP
#define WL_TXPWRCAP(x) WL_NONE(x)
static int wlc_channel_txcap_set_country(wlc_cm_info_t *wlc_cm);
static int wlc_channel_txcap_phy_update(wlc_cm_info_t *wlc_cm,
	wl_txpwrcap_tbl_t *txpwrcap_tbl, int* cellstatus);
static int wlc_channel_txcapver(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
#endif /* WLC_TXPWRCAP */

#define TXPWRCAP_NUM_SUBBANDS 5

#define BLOB_LITERAL "BLOB"
#define MAGIC_SEQ_LEN 4
#define PATRIM_MAGIC_SEQ 0x3D3F313D

typedef struct txcap_file_cc_group_info {
	uint8	num_cc;
	char	cc_list[1][2];
	/* int8 low_cap[num_subbands * num_antennas] */
	/* int8 high_cap[num_subbands * num_antennas] */
} txcap_file_cc_group_info_t;

#define TXCAP_FILE_HEADER_FLAG_WCI2_AND_HOST_MASK 0x1
typedef struct txcap_file_header {
	uint8	magic[4];
	uint16	version;
	uint16	flags;
	char	title[64];
	char	creation[64];
	uint8	num_subbands;
	uint8	num_antennas_per_core[TXPWRCAP_MAX_NUM_CORES];
	uint8	num_cc_groups;
	txcap_file_cc_group_info_t	cc_group_info;
} txcap_file_header_t;

typedef struct wlc_cm_band {
	uint16		locale_flags;		/* locale_info_t flags */
	chanvec_t	valid_channels;		/* List of valid channels in the country */
	chanvec_t	*radar_channels;	/* List of radar sensitive channels */
	struct wlc_channel_txchain_limits chain_limits;	/* per chain power limit */
	uint8		PAD[4];
} wlc_cm_band_t;

#define PPR_BUF_NUM 2
#define PPR_RU_BUF_NUM 2	/* UL-OFDMA, RU type PPR */

/* Pre-alloc buf for PPR */
typedef struct wlc_cm_ppr_buffer {
	int lock;	/* 0 Free, 1 Locked */
	int8 *txpwr;	/* buf pointer, for ppr_t or ppr_ru_t */
} wlc_cm_ppr_buffer_t;

typedef struct wlc_cm_data {
	char		srom_ccode[WLC_CNTRY_BUF_SZ];	/* Country Code in SROM */
	uint		srom_regrev;			/* Regulatory Rev for the SROM ccode */
	clm_country_t country;			/* Current country iterator for the CLM data */
	char		ccode[WLC_CNTRY_BUF_SZ];	/* current internal Country Code */
	uint		regrev;				/* current Regulatory Revision */
	char		country_abbrev[WLC_CNTRY_BUF_SZ];	/* current advertised ccode */
	wlc_cm_band_t	bandstate[MAXBANDS];	/* per-band state (one per phy/radio) */
	/* quiet channels currently for radar sensitivity or 11h support */
	chanvec_t	quiet_channels;		/* channels on which we cannot transmit */

	struct clm_data_header* clm_base_dataptr;
	int clm_base_data_len;

	/* List of radar sensitive channels for the current locale */
	chanvec_t locale_radar_channels;

	/* restricted channels */
	chanvec_t	restricted_channels;	/* copy of the global restricted channels of the */
						/* current local */
	bool		has_restricted_ch;

	/* regulatory class */
	rcvec_t		valid_rcvec;		/* List of valid regulatory class in the country */
	const rcinfo_t	*rcinfo_list[MAXRCLIST];	/* regulatory class info list */
	bool		sar_enable;		/* Use SAR as part of regulatory power calc */
#ifdef WL_SARLIMIT
	sar_limit_t	sarlimit;		/* sar limit per band/sub-band */
#endif // endif
	/* List of valid regulatory class in the country */
	chanvec_t	allowed_5g_20channels;	/* List of valid 20MHz channels in the country */
	chanvec_t	allowed_5g_4080channels;	/* List of valid 40 and 80MHz channels */

	uint32		clmload_status;		/* detailed clmload status */
	wlc_blob_info_t *clmload_wbi;
	uint32		txcapload_status;	/* detailed TX CAP load status */
	wlc_blob_info_t *txcapload_wbi;
	txcap_file_header_t		*txcap_download;
	uint32				txcap_download_size;
	txcap_file_cc_group_info_t	*current_country_cc_group_info;
	uint8				current_country_cc_group_info_index;
	uint32				txcap_high_cap_timeout;
	uint8				txcap_high_cap_active;
	struct wl_timer			*txcap_high_cap_timer;
	uint8				txcap_download_num_antennas;
	uint8				txcap_config[TXPWRCAP_NUM_SUBBANDS];
	uint8				txcap_state[TXPWRCAP_NUM_SUBBANDS];
	uint8				txcap_wci2_cell_status_last;
	uint8				txcap_cap_states_per_cc_group;
	wl_txpwrcap_tbl_t               txpwrcap_tbl;
	int                             cellstatus;
	const rcinfo_t *rcinfo_list_11ac[MAXRCLIST_REG];
#ifdef WL_GLOBAL_RCLASS
	uint8		cur_rclass;		/* current operating class, country or global */
#endif /* WL_GLOBAL_RCLASS */
	wlc_cm_ppr_buffer_t	ppr_buf[PPR_BUF_NUM];	/* Pre-alloc buf for PPR */
	wlc_cm_ppr_buffer_t	ppr_ru_buf[PPR_RU_BUF_NUM];	/* Pre-alloc buf for RU type PPR */
} wlc_cm_data_t;

struct wlc_cm_info {
	wlc_pub_t	*pub;
	wlc_info_t	*wlc;
	wlc_cm_data_t	*cm;
	dload_error_status_t blobload_status;		/* detailed blob load status */
};

typedef struct paoffset_srom {
	uint8 magic_seq[MAGIC_SEQ_LEN];
	uint16 hdr_len;		/* Header length including magic sequence */
	uint16 num_slices;	/* Number of slices.
				 * Will be 1 in case of MIMO and 2 in case of SDB
				 */
	uint16 slice0_offset;
	uint16 slice0_len;
	uint16 slice1_offset;
	uint16 slice1_len;
} paoffset_srom_t;

#ifdef WL_GLOBAL_RCLASS
static bool wlc_chk_rclass_support_5g(const rcinfo_t *rcinfo, uint8 *setBitBuff);
#endif /* WL_GLOBAL_RCLASS */

#if defined(BCMPCIEDEV_SROM_FORMAT) && defined(WLC_TXCAL)
static int wlc_srom_read_blobdata(struct wlc_info *wlc, wlc_cm_data_t *wlc_cm);
static dload_error_status_t wlc_handle_cal_dload_wrapper(struct wlc_info *wlc,
	wlc_blob_segment_t *segments, uint32 segment_count);
#endif // endif
static dload_error_status_t wlc_handle_clm_dload(wlc_info_t *wlc,
	wlc_blob_segment_t *segments, uint32 segment_count);

#ifdef WLC_TXPWRCAP
static dload_error_status_t wlc_handle_txcap_dload(wlc_info_t *wlc_cm,
	wlc_blob_segment_t *segments, uint32 segment_count);
#endif /* WLC_TXPWRCAP */
#ifdef WL_EXPORT_CURPOWER
static void
wlc_channel_clm_limits_to_ppr(const clm_power_limits_t *clm_pwr, ppr_t *ppr_pwr, wl_tx_bw_t bw);
static int
wlc_get_clm_power_limits(wlc_cm_info_t *wlc_cmi, wlc_clm_power_limits_req_t *arg,
	int arg_buf_len);
#endif /* WL_EXPORT_CURPOWER */

static void wlc_channels_init(wlc_cm_info_t *wlc_cm, clm_country_t country);
static void wlc_set_country_common(
	wlc_cm_info_t *wlc_cmi, const char* country_abbrev, const char* ccode, uint regrev,
	clm_country_t country);
static int wlc_country_aggregate_map(
	wlc_cm_info_t *wlc_cmi, const char *ccode, char *mapped_ccode, uint *mapped_regrev);
static clm_result_t wlc_countrycode_map(wlc_cm_info_t *wlc_cmi, const char *ccode,
	char *mapped_ccode, uint *mapped_regrev, clm_country_t *country);
static int wlc_channels_commit(wlc_cm_info_t *wlc_cmi);
static void wlc_chanspec_list(wlc_info_t *wlc, wl_uint32_list_t *list, chanspec_t chanspec_mask);
static bool wlc_japan_ccode(const char *ccode);
static bool wlc_us_ccode(const char *ccode);
static void wlc_regclass_vec_init(wlc_cm_info_t *wlc_cmi);
static void wlc_upd_restricted_chanspec_flag(wlc_cm_info_t *wlc_cmi);
static int wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cmi, ppr_t *txpwr);
static void wlc_channel_set_radar_chanvect(wlc_cm_data_t *wlc_cm, wlcband_t *band, uint16 flags);

/* IE mgmt callbacks */
#ifdef WLTDLS
static uint wlc_channel_tdls_calc_rc_ie_len(void *ctx, wlc_iem_calc_data_t *calc);
static int wlc_channel_tdls_write_rc_ie(void *ctx, wlc_iem_build_data_t *build);
#endif // endif

#if defined(WL_SARLIMIT) && defined(BCMDBG)
static void wlc_channel_sarlimit_dump(wlc_cm_info_t *wlc_cmi, sar_limit_t *sar);
#endif /* WL_SARLIMIT && BCMDBG */
static void wlc_channel_spurwar_locale(wlc_cm_info_t *wlc_cm, chanspec_t chanspec);

#if defined(BCMDBG)
static int wlc_channel_dump_reg_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_reg_local_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_srom_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_margin(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_locale(void *handle, struct bcmstrbuf *b);

static int wlc_channel_init_ccode(wlc_cm_info_t *wlc_cmi, char* country_abbrev, int ca_len);

static int wlc_dump_max_power_per_channel(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_2G_20M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_2G_40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_2G_20in40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_5G_20M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_5G_40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_5G_20in40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);

static int wlc_dump_country_aggregate_map(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b);
static int wlc_channel_supported_country_regrevs(void *handle, struct bcmstrbuf *b);

static bool wlc_channel_clm_chanspec_valid(wlc_cm_info_t *wlc_cmi, chanspec_t chspec);

const char fraction[4][4] = {"   ", ".25", ".5 ", ".75"};
#define QDB_FRAC(x)	(x) / WLC_TXPWR_DB_FACTOR, fraction[(x) % WLC_TXPWR_DB_FACTOR]
#define QDB_FRAC_TRUNC(x)	(x) / WLC_TXPWR_DB_FACTOR, \
	((x) % WLC_TXPWR_DB_FACTOR) ? fraction[(x) % WLC_TXPWR_DB_FACTOR] : ""
#endif // endif

#define	COPY_LIMITS(src, index, dst, cnt)	\
		bcopy(&src.limit[index], txpwr->dst, cnt)
#define	COPY_DSSS_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_CCK)
#define	COPY_OFDM_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_OFDM)
#define	COPY_MCS_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_MCS_1STREAM)
#ifdef WL11AC
#define	COPY_VHT_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_EXTRA_VHT)
#else
#define	COPY_VHT_LIMS(src, index, dst)
#endif // endif

#define CLM_DSSS_RATESET(src) ((const ppr_dsss_rateset_t*)&src->limit[WL_RATE_1X1_DSSS_1])
#define CLM_OFDM_1X1_RATESET(src) ((const ppr_ofdm_rateset_t*)&src->limit[WL_RATE_1X1_OFDM_6])
#define CLM_MCS_1X1_RATESET(src) ((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_1X1_MCS0])

#define CLM_DSSS_1X2_MULTI_RATESET(src) \
	((const ppr_dsss_rateset_t*)&src->limit[WL_RATE_1X2_DSSS_1])
#define CLM_OFDM_1X2_CDD_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src->limit[WL_RATE_1X2_CDD_OFDM_6])
#define CLM_MCS_1X2_CDD_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_1X2_CDD_MCS0])

#define CLM_DSSS_1X3_MULTI_RATESET(src) \
	((const ppr_dsss_rateset_t*)&src->limit[WL_RATE_1X3_DSSS_1])
#define CLM_OFDM_1X3_CDD_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src->limit[WL_RATE_1X3_CDD_OFDM_6])
#define CLM_MCS_1X3_CDD_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_1X3_CDD_MCS0])

#define CLM_DSSS_1X4_MULTI_RATESET(src) \
	((const ppr_dsss_rateset_t*)&src->limit[WL_RATE_1X4_DSSS_1])
#define CLM_OFDM_1X4_CDD_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src->limit[WL_RATE_1X4_CDD_OFDM_6])
#define CLM_MCS_1X4_CDD_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_1X4_CDD_MCS0])

#define CLM_MCS_2X2_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X2_SDM_MCS8])
#define CLM_MCS_2X2_STBC_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X2_STBC_MCS0])

#define CLM_MCS_2X3_STBC_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X3_STBC_MCS0])
#define CLM_MCS_2X3_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X3_SDM_MCS8])

#define CLM_MCS_2X4_STBC_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X4_STBC_MCS0])
#define CLM_MCS_2X4_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X4_SDM_MCS8])

#define CLM_MCS_3X3_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_3X3_SDM_MCS16])
#define CLM_MCS_3X4_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_3X4_SDM_MCS16])

#define CLM_MCS_4X4_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_4X4_SDM_MCS24])

#define CLM_OFDM_1X2_TXBF_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src->limit[WL_RATE_1X2_TXBF_OFDM_6])
#define CLM_MCS_1X2_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_1X2_TXBF_MCS0])
#define CLM_OFDM_1X3_TXBF_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src->limit[WL_RATE_1X3_TXBF_OFDM_6])
#define CLM_MCS_1X3_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_1X3_TXBF_MCS0])
#define CLM_OFDM_1X4_TXBF_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src->limit[WL_RATE_1X4_TXBF_OFDM_6])
#define CLM_MCS_1X4_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_1X4_TXBF_MCS0])
#define CLM_MCS_2X2_TXBF_RATESET(src) \
	((const ppr_ht_mcs_rateset_t*)&src->limit[WL_RATE_2X2_TXBF_SDM_MCS8])
#define CLM_MCS_2X3_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X3_TXBF_SDM_MCS8])
#define CLM_MCS_2X4_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_2X4_TXBF_SDM_MCS8])
#define CLM_MCS_3X3_TXBF_RATESET(src) \
	((const ppr_ht_mcs_rateset_t*)&src->limit[WL_RATE_3X3_TXBF_SDM_MCS16])
#define CLM_MCS_3X4_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src->limit[WL_RATE_3X4_TXBF_SDM_MCS16])
#define CLM_MCS_4X4_TXBF_RATESET(src) \
	((const ppr_ht_mcs_rateset_t*)&src->limit[WL_RATE_4X4_TXBF_SDM_MCS24])

#if defined WLTXPWR_CACHE && defined(WL11N)
static chanspec_t last_chanspec = 0;
#endif /* WLTXPWR_CACHE */

clm_result_t clm_aggregate_country_lookup(const ccode_t cc, unsigned int rev,
	clm_agg_country_t *agg);
clm_result_t clm_aggregate_country_map_lookup(const clm_agg_country_t agg,
	const ccode_t target_cc, unsigned int *rev);

static void
wlc_channel_read_bw_limits_to_ppr(wlc_cm_info_t *wlc_cmi, ppr_t *txpwr, clm_power_limits_t *limits,
	wl_tx_bw_t bw);

static void
wlc_channel_read_ub_bw_limits_to_ppr(wlc_cm_info_t *wlc_cmi, ppr_t *txpwr,
	clm_ru_power_limits_t *ru_limits, wl_tx_bw_t bw);

static void
wlc_channel_read_ru_bw_limits_to_ppr(wlc_cm_info_t *wlc_cmi, ppr_ru_t *ru_txpwr,
	clm_ru_power_limits_t *ru_limits);

static clm_result_t wlc_clm_power_limits(
	const clm_country_locales_t *locales, clm_band_t band,
	unsigned int chan, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, void *limits, bool ru);

/* QDB() macro takes a dB value and converts to a quarter dB value */
#ifdef QDB
#undef QDB
#endif // endif
#define QDB(n) ((n) * WLC_TXPWR_DB_FACTOR)

/* Regulatory Matrix Spreadsheet (CLM) MIMO v3.8.6.4
 * + CLM v4.1.3
 * + CLM v4.2.4
 * + CLM v4.3.1 (Item-1 only EU/9 and Q2/4).
 * + CLM v4.3.4_3x3 changes(Skip changes for a13/14).
 * + CLMv 4.5.3_3x3 changes for Item-5(Cisco Evora (change AP3500i to Evora)).
 * + CLMv 4.5.3_3x3 changes for Item-3(Create US/61 for BCM94331HM, based on US/53 power levels).
 * + CLMv 4.5.5 3x3 (changes from Create US/63 only)
 * + CLMv 4.4.4 3x3 changes(Create TR/4 (locales Bn7, 3tn), EU/12 (locales 3s, 3sn) for Airties.)
 */

/*
 * Some common channel sets
 */

/* All 2.4 GHz HW channels */
const chanvec_t chanvec_all_2G = {
	{0xfe, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* All 5 GHz HW channels */
const chanvec_t chanvec_all_5G = {
	/* 35,36,38/40,42,44,46/48,52/56,60 */
	{0x00, 0x00, 0x00, 0x00, 0x54, 0x55, 0x11, 0x11,
	/* 64/-/-/-/100/104,108/112,116/120,124 */
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,
#ifdef WL11AC
	/* /128,132/136,140/144,149/153,157/161,165/169,173... */
	0x11, 0x11, 0x21, 0x22, 0x22, 0x22, 0x00, 0x11,
#else
	/* /128,132/136,140/149/153,157/161,165... */
	0x11, 0x11, 0x20, 0x22, 0x22, 0x00, 0x00, 0x11,
#endif // endif
	0x11, 0x11, 0x11, 0x01}
};

/*
 * Radar channel sets
 */

#ifdef BAND5G
static const chanvec_t radar_set1 = { /* Channels 52 - 64, 100 - 144 */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,	/* 52 - 60 */
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,		/* 64, 100 - 124 */
	0x11, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 128 - 144 */
	0x00, 0x00, 0x00, 0x00}
};

/* Channels 149-165 are treated as radar channels as per new UK DFS requirements.
 * EN302 502 and EN301 893 Channel 144 is also available.
 */
static const chanvec_t radar_set_uk = { /* Channels 52 - 64, 100 - 144 */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,	/* 52 - 60 */
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,		/* 64, 100 - 124 */
	0x11, 0x11, 0x21, 0x22, 0x22, 0x00, 0x00, 0x00,		/* 128 - 144, 149 - 165 */
	0x00, 0x00, 0x00, 0x00}
};
#endif	/* BAND5G */

/*
 * Restricted channel sets
 */

#define WLC_REGCLASS_USA_2G_20MHZ	12
#define WLC_REGCLASS_EUR_2G_20MHZ	4
#define WLC_REGCLASS_JPN_2G_20MHZ	30
#define WLC_REGCLASS_JPN_2G_20MHZ_CH14	31
#define WLC_REGCLASS_GLOBAL_2G_20MHZ		81
#define WLC_REGCLASS_GLOBAL_2G_20MHZ_CH14	82

#ifdef BAND5G
/*
 * channel to Global regulatory class map
 */
static const rcinfo_t rcinfo_global_20 = {
	27,
	{
	{ 36, 115}, { 40, 115}, { 44, 115}, { 48, 115}, { 52, 118}, { 56, 118}, { 60, 118},
	{ 64, 118}, {100, 121}, {104, 121}, {108, 121}, {112, 121}, {116, 121}, {120, 121},
	{124, 121}, {128, 121}, {132, 121}, {136, 121}, {140, 121}, {144, 121}, {149, 125},
	{153, 125}, {157, 125}, {161, 125}, {165, 125}, {169, 125}, {173, 125},
	}
};
#endif /* BAND5G */

#ifdef WL11N
/* control channel at lower sb */
static const rcinfo_t rcinfo_global_40lower = {
	22,
	{
	{  1,  83}, {  2,  83}, {  3,  83}, {  4,  83}, {  5,  83}, {  6,  83}, {  7,  83},
	{  8,  83}, {  9,  83}, { 36, 116}, { 44, 116}, { 52, 119}, { 60, 119}, {100, 122},
	{108, 122}, {116, 122}, {124, 122}, {132, 122}, {140, 122}, {149, 126}, {157, 126},
	{165, 126}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
/* control channel at upper sb */
static const rcinfo_t rcinfo_global_40upper = {
	22,
	{
	{  5,  84}, {  6,  84}, {  7,  84}, {  8,  84}, {  9,  84}, { 10,  84}, { 11,  84},
	{ 12,  84}, { 13,  84}, { 40, 117}, { 48, 117}, { 56, 120}, { 64, 120}, {104, 123},
	{112, 123}, {120, 123}, {128, 123}, {136, 123}, {144, 123}, {153, 127}, {161, 127},
	{169, 127}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11N */

#ifdef WL11AC
/* center channel at 80MHZ */
static const rcinfo_t rcinfo_global_center_80 = {
	6,
	{
	{ 42, 128}, { 58, 128}, {106, 128}, {122, 128}, {138, 128}, {155, 128}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11AC */

#ifdef WL11AC_160
/* center channel at 160MHZ */
static const rcinfo_t rcinfo_global_center_160 = {
	2,
	{
	{ 50, 129}, {114, 129}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11AC_160 */

#ifdef BAND5G
/*
 * channel to regulatory class map for USA
 */
static const rcinfo_t rcinfo_us_20 = {
	25,
	{
	{ 36,   1}, { 40,   1}, { 44,   1}, { 48,   1}, { 52,   2}, { 56,   2}, { 60,   2},
	{ 64,   2}, {100,   4}, {104,   4}, {108,   4}, {112,   4}, {116,   4}, {120,   4},
	{124,   4}, {128,   4}, {132,   4}, {136,   4}, {140,   4}, {144,   4}, {149,   3},
	{153,   3}, {157,   3}, {161,   3}, {165,   5}, {  0,   0}, {  0,   0},
	}
};
#endif /* BAND5G */

#ifdef WL11N
/* control channel at lower sb */
static const rcinfo_t rcinfo_us_40lower = {
	19,
	{
	{  1,  32}, {  2,  32}, {  3,  32}, {  4,  32}, {  5,  32}, {  6,  32}, {  7,  32},
	{ 36,  22}, { 44,  22}, { 52,  23}, { 60,  23}, {100,  24}, {108,  24}, {116,  24},
	{124,  24}, {132,  24}, {140,  24}, {149,  25}, {157,  25}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
/* control channel at upper sb */
static const rcinfo_t rcinfo_us_40upper = {
	19,
	{
	{  5,  33}, {  6,  33}, {  7,  33}, {  8,  33}, {  9,  33}, { 10,  33}, { 11,  33},
	{ 40,  27}, { 48,  27}, { 56,  28}, { 64,  28}, {104,  29}, {112,  29}, {120,  29},
	{128,  29}, {136,  29}, {144,  29}, {153,  30}, {161,  30}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11N */

#ifdef WL11AC
/* center channel at 80MHZ */
static const rcinfo_t rcinfo_us_center_80 = {
	6,
	{
	{ 42, 128}, { 58, 128}, {106, 128}, {122, 128}, {138, 128}, {155, 128}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11AC */

#ifdef WL11AC_160
/* center channel at 160MHZ */
static const rcinfo_t rcinfo_us_center_160 = {
	2,
	{
	{ 50, 129}, {114, 129}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11AC_160 */

#ifdef BAND5G
/*
 * channel to regulatory class map for Europe
 */
static const rcinfo_t rcinfo_eu_20 = {
	25,
	{
	{ 36,   1}, { 40,   1}, { 44,   1}, { 48,   1}, { 52,   2}, { 56,   2}, { 60,   2},
	{ 64,   2}, {100,   3}, {104,   3}, {108,   3}, {112,   3}, {116,   3}, {120,   3},
	{124,   3}, {128,   3}, {132,   3}, {136,   3}, {140,   3}, {149,  17}, {153,  17},
	{157,  17}, {161,  17}, {165,  17}, {169,  17}, {  0,   0}, {  0,   0},
	}
};
#endif /* BAND5G */

#ifdef WL11N
static const rcinfo_t rcinfo_eu_40lower = {
	18,
	{
	{  1,  11}, {  2,  11}, {  3,  11}, {  4,  11}, {  5,  11}, {  6,  11}, {  7,  11},
	{  8,  11}, {  9,  11}, { 36,   5}, { 44,   5}, { 52,   6}, { 60,   6}, {100,   7},
	{108,   7}, {116,   7}, {124,   7}, {132,   7}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
static const rcinfo_t rcinfo_eu_40upper = {
	18,
	{
	{  5,  12}, {  6,  12}, {  7,  12}, {  8,  12}, {  9,  12}, { 10,  12}, { 11,  12},
	{ 12,  12}, { 13,  12}, { 40,   8}, { 48,   8}, { 56,   9}, { 64,   9}, {104,  10},
	{112,  10}, {120,  10}, {128,  10}, {136,  10}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11N */

#ifdef WL11AC
/* center channel at 80MHZ */
static const rcinfo_t rcinfo_eu_center_80 = {
	4,
	{
	{ 42, 128}, { 58, 128}, {106, 128}, {122, 128}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11AC */

#ifdef WL11AC_160
/* center channel at 160MHZ */
static const rcinfo_t rcinfo_eu_center_160 = {
	2,
	{
	{ 50, 129}, {114, 129}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11AC_160 */

#ifdef BAND5G
/*
 * channel to regulatory class map for Japan
 */
static const rcinfo_t rcinfo_jp_20 = {
	8,
	{
	{ 34,   1}, { 38,   1}, { 42,   1}, { 46,   1}, { 52,  32}, { 56,  32}, { 60,  32},
	{ 64,  32}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* BAND5G */

#ifdef WL11N
static const rcinfo_t rcinfo_jp_40 = {
	0,
	{
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif	/* WL11N */

#ifdef WL11AC
/* center channel at 80MHZ */
static const rcinfo_t rcinfo_jp_center_80 = {
	4,
	{
	{ 42, 128}, { 58, 128}, {106, 128}, {122, 128}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11Ac */

#ifdef WL11AC_160
/* center channel at 160MHZ */
static const rcinfo_t rcinfo_jp_center_160 = {
	2,
	{
	{ 50, 129}, {114, 129}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0},
	}
};
#endif /* WL11AC_160 */

/* Parameters of clm RU rates (802.11ax OFDMA) */
typedef struct wlc_cm_ru_rate_p {
	wl_tx_mode_t		mode;
	wl_tx_nss_t		nss;
	wl_tx_chains_t		chain;
} wlc_cm_ru_rate_p_t;

/* For 802.11ax, there are 19 tx modes,
 * ex. WL_RU_RATE_1X1_26SS1 to WL_RU_RATE_4X4_TXBF_26SS4 in clm_ru_rates_t
 */
static const wlc_cm_ru_rate_p_t ru_rate_tbl[] = {
	{WL_TX_MODE_NONE,	WL_TX_NSS_1,	WL_TX_CHAINS_1},	/* RATE_1X1_SS1 */
	{WL_TX_MODE_CDD,	WL_TX_NSS_1,	WL_TX_CHAINS_2},	/* RATE_1X2_SS1 */
	{WL_TX_MODE_NONE,	WL_TX_NSS_2,	WL_TX_CHAINS_2},	/* RATE_2X2_SS2 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_1,	WL_TX_CHAINS_2},	/* RATE_1X2_TXBF_SS1 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_2,	WL_TX_CHAINS_2},	/* RATE_2X2_TXBF_SS2 */
	{WL_TX_MODE_CDD,	WL_TX_NSS_1,	WL_TX_CHAINS_3},	/* RATE_1X3_SS1 */
	{WL_TX_MODE_NONE,	WL_TX_NSS_2,	WL_TX_CHAINS_3},	/* RATE_2X3_SS2 */
	{WL_TX_MODE_NONE,	WL_TX_NSS_3,	WL_TX_CHAINS_3},	/* RATE_3X3_SS3 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_1,	WL_TX_CHAINS_3},	/* RATE_1X3_TXBF_SS1 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_2,	WL_TX_CHAINS_3},	/* RATE_2X3_TXBF_SS2 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_3,	WL_TX_CHAINS_3},	/* RATE_3X3_TXBF_SS3 */
	{WL_TX_MODE_CDD,	WL_TX_NSS_1,	WL_TX_CHAINS_4},	/* RATE_1X4_SS1 */
	{WL_TX_MODE_NONE,	WL_TX_NSS_2,	WL_TX_CHAINS_4},	/* RATE_2X4_SS2 */
	{WL_TX_MODE_NONE,	WL_TX_NSS_3,	WL_TX_CHAINS_4},	/* RATE_3X4_SS3 */
	{WL_TX_MODE_NONE,	WL_TX_NSS_4,	WL_TX_CHAINS_4},	/* RATE_4X4_SS4 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_1,	WL_TX_CHAINS_4},	/* RATE_1X4_TXBF_SS1 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_2,	WL_TX_CHAINS_4},	/* RATE_2X4_TXBF_SS2 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_3,	WL_TX_CHAINS_4},	/* RATE_3X4_TXBF_SS3 */
	{WL_TX_MODE_TXBF,	WL_TX_NSS_4,	WL_TX_CHAINS_4},	/* RATE_4X4_TXBF_SS4 */
};

/* iovar table */
enum wlc_channel_iov {
	IOV_RCLASS				= 1, /* read rclass */
	IOV_CLMLOAD				= 2,
	IOV_CLMLOAD_STATUS			= 3,
	IOV_TXCAPLOAD				= 4,
	IOV_TXCAPLOAD_STATUS			= 5,
	IOV_TXCAPVER				= 6,
	IOV_TXCAPCONFIG 			= 7,
	IOV_TXCAPSTATE				= 8,
	IOV_TXCAPHIGHCAPTO			= 9,
	IOV_TXCAPDUMP				= 10,
	IOV_QUIETCHAN				= 11,
	IOV_CLM_POWER_LIMITS			= 12,
	IOV_DIS_CH_GRP				= 13,
	IOV_DIS_CH_GRP_CONF			= 14,
	IOV_DIS_CH_GRP_USER			= 15,
	IOV_IS_EDCRS_EU				= 16,
	IOV_LAST
};

static const bcm_iovar_t cm_iovars[] = {
	{"rclass", IOV_RCLASS, 0, 0, IOVT_UINT16, 0},
	{"clmload", IOV_CLMLOAD, IOVF_SET_DOWN, 0, IOVT_BUFFER, 0},
	{"clmload_status", IOV_CLMLOAD_STATUS, 0, 0, IOVT_UINT32, 0},
	{"txcapload", IOV_TXCAPLOAD, 0, 0, IOVT_BUFFER, 0},
	{"txcapload_status", IOV_TXCAPLOAD_STATUS, 0, 0, IOVT_UINT32, 0},
	{"txcapver", IOV_TXCAPVER, 0, 0, IOVT_BUFFER, 0},
	{"txcapconfig", IOV_TXCAPCONFIG, IOVF_SET_DOWN, 0, IOVT_BUFFER, sizeof(wl_txpwrcap_ctl_t)},
	{"txcapstate", IOV_TXCAPSTATE, 0, 0, IOVT_BUFFER, sizeof(wl_txpwrcap_ctl_t)},
	{"txcaphighcapto", IOV_TXCAPHIGHCAPTO, 0, 0, IOVT_UINT32, 0},
	{"txcapdump", IOV_TXCAPDUMP, 0, 0, IOVT_BUFFER, sizeof(wl_txpwrcap_dump_v3_t)},
#ifdef WL_EXPORT_CURPOWER
	{"clm_power_limits", IOV_CLM_POWER_LIMITS, 0, 0, IOVT_BUFFER, 0},
#endif /* WL_EXPORT_CURPOWER */
	{"dis_ch_grp", IOV_DIS_CH_GRP, 0, 0, IOVT_UINT32, 0},
	{"dis_ch_grp_conf", IOV_DIS_CH_GRP_CONF, 0, 0, IOVT_UINT32, 0},
	{"dis_ch_grp_user", IOV_DIS_CH_GRP_USER, (IOVF_SET_DOWN), 0, IOVT_UINT32, 0},
	{"is_edcrs_eu", IOV_IS_EDCRS_EU, 0, 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

clm_result_t
wlc_locale_get_channels(clm_country_locales_t *locales, clm_band_t band,
	chanvec_t *channels, chanvec_t *restricted)
{
	bzero(channels, sizeof(chanvec_t));
	bzero(restricted, sizeof(chanvec_t));

	return clm_country_channels(locales, band, (clm_channels_t *)channels,
		(clm_channels_t *)restricted);
}

clm_result_t wlc_get_flags(clm_country_locales_t *locales, clm_band_t band, uint16 *flags)
{
	unsigned long clm_flags = 0;

	clm_result_t result = clm_country_flags(locales, band, &clm_flags);

	*flags = 0;
	if (result == CLM_RESULT_OK) {
		switch (clm_flags & CLM_FLAG_DFS_MASK) {
		case CLM_FLAG_DFS_UK:
			*flags |= WLC_DFS_UK;
			break;
		case CLM_FLAG_DFS_EU:
			*flags |= WLC_DFS_EU;
			break;
		case CLM_FLAG_DFS_US:
			*flags |= WLC_DFS_FCC;
			break;
		case CLM_FLAG_DFS_NONE:
		default:
			break;
		}

		if (clm_flags & CLM_FLAG_EDCRS_EU) {
			*flags |= WLC_EDCRS_EU;
		}

		if (clm_flags & CLM_FLAG_FILTWAR1)
			*flags |= WLC_FILT_WAR;

		if (clm_flags & CLM_FLAG_TXBF)
			*flags |= WLC_TXBF;

		if (clm_flags & CLM_FLAG_NO_MIMO)
			*flags |= WLC_NO_MIMO;
		else {
			if (clm_flags & CLM_FLAG_NO_40MHZ)
				*flags |= WLC_NO_40MHZ;
			if (clm_flags & CLM_FLAG_NO_80MHZ)
				*flags |= WLC_NO_80MHZ;
			if (clm_flags & CLM_FLAG_NO_160MHZ)
				*flags |= WLC_NO_160MHZ;
		}

		if (clm_flags & CLM_FLAG_LO_GAIN_NBCAL)
			*flags |= WLC_LO_GAIN_NBCAL;

		if ((band == CLM_BAND_2G) && (clm_flags & CLM_FLAG_HAS_DSSS_EIRP))
			*flags |= WLC_EIRP;
		if ((band == CLM_BAND_5G) && (clm_flags & CLM_FLAG_HAS_OFDM_EIRP))
			*flags |= WLC_EIRP;
	}

	return result;
}

clm_result_t wlc_get_locale(clm_country_t country, clm_country_locales_t *locales)
{
	return clm_country_def(country, locales);
}

/* 20MHz channel info for 40MHz pairing support */

struct chan20_info {
	uint8	sb;
	uint8	adj_sbs;
};

/* indicates adjacent channels that are allowed for a 40 Mhz channel and
 * those that permitted by the HT
 */
const struct chan20_info chan20_info[] = {
	/* 11b/11g */
/* 0 */		{1,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 1 */		{2,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 2 */		{3,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 3 */		{4,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 4 */		{5,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 5 */		{6,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 6 */		{7,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 7 */		{8,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 8 */		{9,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 9 */		{10,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 10 */	{11,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 11 */	{12,	(CH_LOWER_SB)},
/* 12 */	{13,	(CH_LOWER_SB)},
/* 13 */	{14,	(CH_LOWER_SB)},

/* 11a japan high */
/* 14 */	{34,	(CH_UPPER_SB)},
/* 15 */	{38,	(CH_LOWER_SB)},
/* 16 */	{42,	(CH_LOWER_SB)},
/* 17 */	{46,	(CH_LOWER_SB)},

/* 11a usa low */
/* 18 */	{36,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 19 */	{40,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 20 */	{44,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 21 */	{48,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 22 */	{52,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 23 */	{56,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 24 */	{60,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 25 */	{64,	(CH_LOWER_SB | CH_EWA_VALID)},

/* 11a Europe */
/* 26 */	{100,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 27 */	{104,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 28 */	{108,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 29 */	{112,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 30 */	{116,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 31 */	{120,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 32 */	{124,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 33 */	{128,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 34 */	{132,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 35 */	{136,	(CH_LOWER_SB | CH_EWA_VALID)},

#ifdef WL11AC
/* 36 */	{140,   (CH_UPPER_SB | CH_EWA_VALID)},
/* 37 */	{144,   (CH_LOWER_SB)},

/* 11a usa high, ref5 only */
/* The 0x80 bit in pdiv means these are REF5, other entries are REF20 */
/* 38 */	{149,   (CH_UPPER_SB | CH_EWA_VALID)},
/* 39 */	{153,   (CH_LOWER_SB | CH_EWA_VALID)},
/* 40 */	{157,   (CH_UPPER_SB | CH_EWA_VALID)},
/* 41 */	{161,   (CH_LOWER_SB | CH_EWA_VALID)},
/* 42 */	{165,   (CH_UPPER_SB | CH_EWA_VALID)},

/* 11a japan */
/* 43 */	{184,   (CH_UPPER_SB)},
/* 44 */	{188,   (CH_LOWER_SB)},
/* 45 */	{192,   (CH_UPPER_SB)},
/* 46 */	{196,   (CH_LOWER_SB)},
/* 47 */	{200,   (CH_UPPER_SB)},
/* 48 */	{204,   (CH_LOWER_SB)},
/* 49 */	{208,   (CH_UPPER_SB)},
/* 50 */	{212,   (CH_LOWER_SB)},
/* 51 */	{216,   (CH_LOWER_SB)},
/* 52 */	{169,   (CH_LOWER_SB | CH_EWA_VALID)},
/* 53 */	{173,   (CH_LOWER_SB)},

};

#else

/* 36 */	{140,	(CH_LOWER_SB)},

/* 11a usa high, ref5 only */
/* The 0x80 bit in pdiv means these are REF5, other entries are REF20 */
/* 37 */	{149,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 38 */	{153,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 39 */	{157,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 40 */	{161,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 41 */	{165,	(CH_LOWER_SB)},

/* 11a japan */
/* 42 */	{184,	(CH_UPPER_SB)},
/* 43 */	{188,	(CH_LOWER_SB)},
/* 44 */	{192,	(CH_UPPER_SB)},
/* 45 */	{196,	(CH_LOWER_SB)},
/* 46 */	{200,	(CH_UPPER_SB)},
/* 47 */	{204,	(CH_LOWER_SB)},
/* 48 */	{208,	(CH_UPPER_SB)},
/* 49 */	{212,	(CH_LOWER_SB)},
/* 50 */	{216,	(CH_LOWER_SB)}
};
#endif /* WL11AC */

/* country code mapping for SPROM rev 1 */
static const char def_country[][WLC_CNTRY_BUF_SZ] = {
	"AU",   /* Worldwide */
	"TH",   /* Thailand */
	"IL",   /* Israel */
	"JO",   /* Jordan */
	"CN",   /* China */
	"JP",   /* Japan */
	"US",   /* USA */
	"DE",   /* Europe */
	"US",   /* US Low Band, use US */
	"JP",   /* Japan High Band, use Japan */
};

/* autocountry default country code list */
static const char def_autocountry[][WLC_CNTRY_BUF_SZ] = {
	"XY",
	"XA",
	"XB",
	"X0",
	"X1",
	"X2",
	"X3",
	"XS",
	"XV",
	"XT"
};

#ifdef CCODE_LOCKDOWN
static const char BCMATTACHDATA(lockdown_country)[][WLC_CNTRY_BUF_SZ] = {
	"US",
	"Q1",
	"Q2",
	"EU",
	"CA",
	"END",
};
#endif /* CCODE_LOCKDOWN */

static const char BCMATTACHDATA(rstr_ccode)[] = "ccode";
static const char BCMATTACHDATA(rstr_cc)[] = "cc";
static const char BCMATTACHDATA(rstr_regrev)[] = "regrev";

static bool
wlc_autocountry_lookup(char *cc)
{
	uint i;

	for (i = 0; i < ARRAYSIZE(def_autocountry); i++)
		if (!strcmp(def_autocountry[i], cc))
			return TRUE;

	return FALSE;
}

static bool
wlc_lookup_advertised_cc(char* ccode, const clm_country_t country)
{
	ccode_t advertised_cc;
	bool rv = FALSE;
	if (CLM_RESULT_OK == clm_country_advertised_cc(country, advertised_cc)) {
		memcpy(ccode, advertised_cc, 2);
		ccode[2] = '\0';
		rv = TRUE;
	}

	return rv;
}

static char * BCMRAMFN(wlc_channel_ccode_default)(void)
{
#if defined(WLTEST) && !defined(WLTEST_DISABLED)
	return "#a";
#else
	return "#n";
#endif // endif
}

#ifdef CCODE_LOCKDOWN
static bool
wlc_lockdown_country_lookup(char *cc)
{
	uint i = 0;

	while (strncmp(lockdown_country[i], "END", WLC_CNTRY_BUF_SZ)) {
		if (!strncmp(lockdown_country[i], cc, WLC_CNTRY_BUF_SZ))
			return TRUE;
		i++;
	}

	return FALSE;
}
#endif /* CCODE_LOCKDOWN */

static int
wlc_channel_init_ccode(wlc_cm_info_t *wlc_cmi, char* country_abbrev, int ca_len)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	int result = BCME_OK;
	clm_country_t country;

	result = wlc_country_lookup(wlc, country_abbrev, &country);

	/* default to US if country was not specified or not found */
	if (result != CLM_RESULT_OK) {
		strncpy(country_abbrev, "US", ca_len - 1);
		result = wlc_country_lookup(wlc, country_abbrev, &country);
	}

	/* Default to the NULL country(#n) which has no channels, if country US is not found */
	if (result != CLM_RESULT_OK) {
		strncpy(country_abbrev, wlc_channel_ccode_default(), ca_len - 1);
		result = wlc_country_lookup(wlc, country_abbrev, &country);
	}

	ASSERT(result == CLM_RESULT_OK);

	/* save default country for exiting 11d regulatory mode */
	wlc_cntry_set_default(wlc->cntry, country_abbrev);

	/* initialize autocountry_default to driver default */
	if (wlc_autocountry_lookup(country_abbrev))
		wlc_11d_set_autocountry_default(wlc->m11d, country_abbrev);
	else
		wlc_11d_set_autocountry_default(wlc->m11d, "XV");

	/* Calling set_countrycode() once does not generate any event, if called more than
	 * once generates COUNTRY_CODE_CHANGED event which will cause the driver to crash
	 * at startup since bsscfg structure is still not initialized.
	 */
	wlc_set_countrycode(wlc_cmi, country_abbrev);

	/* update edcrs_eu for the initial country setting */
	wlc->is_edcrs_eu = wlc_is_edcrs_eu(wlc);

	return result;
}

static int
wlc_cm_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_cm_info_t *wlc_cmi = (wlc_cm_info_t *)hdl;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	int err = BCME_OK;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	BCM_REFERENCE(len);
	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(val_size);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_RCLASS): {
		chanspec_t chspec = *(chanspec_t*)params;
		*ret_int_ptr = wlc_get_regclass(wlc_cmi, chspec);
		WL_INFORM(("chspec:%x rclass:%d\n", chspec, *ret_int_ptr));
		break;
	}
	case IOV_SVAL(IOV_CLMLOAD): {
		wl_dload_data_t dload_data;
		uint8 *bufptr;
		dload_error_status_t status;

		/* Make sure we have at least a dload data structure */
		if (p_len < sizeof(wl_dload_data_t)) {
			err =  BCME_ERROR;
			wlc_cm->clmload_status = DLOAD_STATUS_IOVAR_ERROR;
			break;
		}

		/* copy to stack so structure wl_data_data has any required processor alignment */
		memcpy(&dload_data, (wl_dload_data_t *)params, sizeof(wl_dload_data_t));

		WL_NONE(("%s: IOV_CLMLOAD flag %04x, type %04x, len %d, crc %08x\n",
			__FUNCTION__, dload_data.flag, dload_data.dload_type,
			dload_data.len, dload_data.crc));

		if (((dload_data.flag & DLOAD_FLAG_VER_MASK) >> DLOAD_FLAG_VER_SHIFT)
			!= DLOAD_HANDLER_VER) {
			err =  BCME_ERROR;
			wlc_cm->clmload_status = DLOAD_STATUS_IOVAR_ERROR;
			break;
		}

		bufptr = ((wl_dload_data_t *)(params))->data;

		status = wlc_blob_download(wlc_cm->clmload_wbi, dload_data.flag,
			bufptr, dload_data.len, wlc_handle_clm_dload);
		switch (status) {
			case DLOAD_STATUS_DOWNLOAD_SUCCESS:
			case DLOAD_STATUS_DOWNLOAD_IN_PROGRESS:
				err = BCME_OK;
				wlc_cm->clmload_status = status;
				break;
			default:
				err = BCME_ERROR;
				wlc_cm->clmload_status = status;
				break;
		}
		break;
	}
	case IOV_GVAL(IOV_CLMLOAD_STATUS): {
		*((uint32 *)arg) = wlc_cm->clmload_status;
		break;
	}
#ifdef WLC_TXPWRCAP
	case IOV_SVAL(IOV_TXCAPLOAD): {
		wl_dload_data_t dload_data;
		uint8 *bufptr;
		dload_error_status_t status;
		if (!WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		/* Make sure we have at least a dload data structure */
		if (p_len < sizeof(wl_dload_data_t)) {
			err =  BCME_ERROR;
			wlc_cm->txcapload_status = DLOAD_STATUS_IOVAR_ERROR;
			break;
		}
		/* copy to stack so structure wl_data_data has any required processor alignment */
		memcpy(&dload_data, (wl_dload_data_t *)params, sizeof(wl_dload_data_t));
		WL_NONE(("%s: IOV_TXCAPLOAD flag %04x, type %04x, len %d, crc %08x\n",
			__FUNCTION__, dload_data.flag, dload_data.dload_type,
			dload_data.len, dload_data.crc));
		if (((dload_data.flag & DLOAD_FLAG_VER_MASK) >> DLOAD_FLAG_VER_SHIFT)
			!= DLOAD_HANDLER_VER) {
			err =  BCME_ERROR;
			wlc_cm->txcapload_status = DLOAD_STATUS_IOVAR_ERROR;
			break;
		}
		bufptr = ((wl_dload_data_t *)(params))->data;
		status = wlc_blob_download(wlc_cm->txcapload_wbi, dload_data.flag,
			bufptr, dload_data.len, wlc_handle_txcap_dload);
		switch (status) {
			case DLOAD_STATUS_DOWNLOAD_SUCCESS:
			case DLOAD_STATUS_DOWNLOAD_IN_PROGRESS:
				err = BCME_OK;
				wlc_cm->txcapload_status = status;
				break;
			default:
				err = BCME_ERROR;
				wlc_cm->txcapload_status = status;
				break;
		}
		break;
	}
	case IOV_GVAL(IOV_TXCAPLOAD_STATUS): {
		if (WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			*((uint32 *)arg) = wlc_cm->txcapload_status;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_GVAL(IOV_TXCAPVER): {
		struct bcmstrbuf b;
		if (WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			bcm_binit(&b, arg, len);
			err = wlc_channel_txcapver(wlc_cmi, &b);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_GVAL(IOV_TXCAPCONFIG): {
		wl_txpwrcap_ctl_t *txcap_ctl = (wl_txpwrcap_ctl_t *)arg;
		if (WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			txcap_ctl->version = TXPWRCAPCTL_VERSION;
			memcpy(txcap_ctl->ctl, wlc_cm->txcap_config, TXPWRCAP_NUM_SUBBANDS);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_GVAL(IOV_TXCAPSTATE): {
		wl_txpwrcap_ctl_t *txcap_ctl = (wl_txpwrcap_ctl_t *)arg;
		if (WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			txcap_ctl->version = TXPWRCAPCTL_VERSION;
			memcpy(txcap_ctl->ctl, wlc_cm->txcap_state, TXPWRCAP_NUM_SUBBANDS);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_SVAL(IOV_TXCAPCONFIG): {
		wl_txpwrcap_ctl_t *txcap_ctl = (wl_txpwrcap_ctl_t *)arg;
		int i;
		if (!WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (txcap_ctl->version != 2) {
			err = BCME_VERSION;
			break;
		}

		/* Check that download has happened so we can do value range checks on values. */
		if (wlc_cm->txcap_download == NULL) {
			err = BCME_NOTREADY;
			break;
		}

		if (wlc_cm->txcap_cap_states_per_cc_group == 2) {
			for (i = 0; i < TXPWRCAP_NUM_SUBBANDS; i++) {
				if (txcap_ctl->ctl[i] > TXPWRCAPCONFIG_HOST) {
					err = BCME_BADARG;
					break;
				}
			}
		} else {
			for (i = 0; i < TXPWRCAP_NUM_SUBBANDS; i++) {
				if (txcap_ctl->ctl[i] > TXPWRCAPCONFIG_WCI2_AND_HOST) {
					err = BCME_BADARG;
					break;
				}
			}
		}

		if (err) break;
		memcpy(wlc_cm->txcap_config, txcap_ctl->ctl, TXPWRCAP_NUM_SUBBANDS);
		/* Reset txcap state for all sub-bands to the common low cap (first "row")
		 * when setting the txcap config.  Note txcapconfig is only allowed when
		 * the driver is down.
		 */
		memset(wlc_cm->txcap_state, TXPWRCAPSTATE_LOW_CAP, TXPWRCAP_NUM_SUBBANDS);
		break;
	}
	case IOV_SVAL(IOV_TXCAPSTATE): {
		wl_txpwrcap_ctl_t *txcap_ctl = (wl_txpwrcap_ctl_t *)arg;
		int i, idx = 0;
		wlc_info_t *wlc_cur = NULL;

		if (!WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (txcap_ctl->version != 2) {
			err = BCME_VERSION;
			break;
		}

		/* Check that download has happened so we can do value range checks on values. */
		if (wlc_cm->txcap_download == NULL) {
			err = BCME_NOTREADY;
			break;
		}

		for (i = 0; i < TXPWRCAP_NUM_SUBBANDS; i++) {
			if (wlc_cm->txcap_config[i] == TXPWRCAPCONFIG_WCI2_AND_HOST) {
				if (txcap_ctl->ctl[i] > TXPWRCAPSTATE_HOST_HIGH_WCI2_HIGH_CAP) {
					err = BCME_BADARG;
					break;
				}
			} else {
				if (txcap_ctl->ctl[i] > TXPWRCAPSTATE_HIGH_CAP) {
					err = BCME_BADARG;
					break;
				}
			}
		}
		if (err) break;
		memcpy(wlc_cm->txcap_state, txcap_ctl->ctl, TXPWRCAP_NUM_SUBBANDS);
		WL_TXPWRCAP(("%s: txcap_high_cap_timer deactivated, was %s\n", __FUNCTION__,
			wlc_cm->txcap_high_cap_active ? "active" : "deactive"));
		wl_del_timer(wlc_cmi->wlc->wl, wlc_cm->txcap_high_cap_timer);
		wlc_cm->txcap_high_cap_active = 0;
		WL_TXPWRCAP(("%s: txcap_high_cap_timer activated for %d seconds\n",
			__FUNCTION__,
			wlc_cm->txcap_high_cap_timeout));
		wlc_cm->txcap_high_cap_active = 1;
		wl_add_timer(wlc_cmi->wlc->wl,
			wlc_cm->txcap_high_cap_timer,
			1000 * wlc_cm->txcap_high_cap_timeout,
			FALSE);
		FOREACH_WLC(wlc_cmi->wlc->cmn, idx, wlc_cur) {
			wlc_cmi = wlc_cur->cmi;
			wlc_channel_txcap_phy_update(wlc_cmi, NULL, NULL);
		}
		break;
	}
	case IOV_GVAL(IOV_TXCAPHIGHCAPTO): {
		if (WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			*((uint32 *)arg) = wlc_cm->txcap_high_cap_timeout;
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_SVAL(IOV_TXCAPHIGHCAPTO): {
		if (WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			wlc_cm->txcap_high_cap_timeout = *((uint32 *)arg);
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_GVAL(IOV_TXCAPDUMP): {
		wl_txpwrcap_dump_v3_t *txcap_dump = (wl_txpwrcap_dump_v3_t *)arg;
		uint8 *p;
		uint8 num_subbands;
		uint8 num_antennas;
		if (!WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		bzero(txcap_dump, sizeof(wl_txpwrcap_dump_t));
		txcap_dump->version = 3;
		txcap_dump->current_country[0] = wlc_cm->country_abbrev[0];
		txcap_dump->current_country[1] = wlc_cm->country_abbrev[1];
		txcap_dump->current_channel = wf_chspec_ctlchan(wlc_cmi->wlc->chanspec);
		txcap_dump->high_cap_state_enabled = wlc_cm->txcap_high_cap_active;
		txcap_dump->wci2_cell_status_last = wlc_cm->txcap_wci2_cell_status_last;
		memcpy(txcap_dump->config, wlc_cm->txcap_config, TXPWRCAP_NUM_SUBBANDS);
		memcpy(txcap_dump->state, wlc_cm->txcap_state, TXPWRCAP_NUM_SUBBANDS);
		if (wlc_cm->txcap_download == NULL) {
			txcap_dump->download_present = 0;
		} else {
			txcap_dump->download_present = 1;
			num_subbands = wlc_cm->txcap_download->num_subbands,
			txcap_dump->num_subbands = num_subbands;
			memcpy(txcap_dump->num_antennas_per_core,
				wlc_cm->txcap_download->num_antennas_per_core,
				TXPWRCAP_MAX_NUM_CORES);
			num_antennas = wlc_cm->txcap_download_num_antennas;
			txcap_dump->num_antennas = num_antennas;
			txcap_dump->num_cc_groups = wlc_cm->txcap_download->num_cc_groups;
			txcap_dump->current_country_cc_group_info_index =
				wlc_cm->current_country_cc_group_info_index;
			txcap_dump->cap_states_per_cc_group = wlc_cm->txcap_cap_states_per_cc_group;
			p = (uint8 *)wlc_cm->current_country_cc_group_info;
			p += OFFSETOF(txcap_file_cc_group_info_t, cc_list) +
				2 * wlc_cm->current_country_cc_group_info->num_cc;
			memcpy(txcap_dump->host_low_wci2_low_cap, p, num_subbands * num_antennas);
			p += num_subbands * num_antennas;
			memcpy(txcap_dump->host_low_wci2_high_cap, p, num_subbands * num_antennas);
			if (wlc_cm->txcap_cap_states_per_cc_group == 4) {
				p += num_subbands * num_antennas;
				memcpy(txcap_dump->host_high_wci2_low_cap, p,
					num_subbands * num_antennas);
				p += num_subbands * num_antennas;
				memcpy(txcap_dump->host_high_wci2_high_cap, p,
					num_subbands * num_antennas);
			}
		}
		break;
	}
#endif /* WLC_TXPWRCAP */
#ifdef WL_EXPORT_CURPOWER
	case IOV_GVAL(IOV_CLM_POWER_LIMITS):
		err = wlc_get_clm_power_limits(wlc_cmi, (wlc_clm_power_limits_req_t *)arg, len);
		break;
#endif /* WL_EXPORT_CURPOWER */

	case IOV_GVAL(IOV_DIS_CH_GRP):
		*((uint32 *)arg) = wlc_cmi->wlc->pub->_dis_ch_grp_conf |
				wlc_cmi->wlc->pub->_dis_ch_grp_user;
		WL_INFORM(("wl%d: ch grp conf: 0x%08x, user: 0x%08x\n", wlc_cmi->wlc->pub->unit,
			wlc_cmi->wlc->pub->_dis_ch_grp_conf, wlc_cmi->wlc->pub->_dis_ch_grp_user));
		break;
	case IOV_GVAL(IOV_DIS_CH_GRP_CONF):
		*((uint32 *)arg) = wlc_cmi->wlc->pub->_dis_ch_grp_conf;
		WL_INFORM(("wl%d: ch grp conf: 0x%08x, user: 0x%08x\n", wlc_cmi->wlc->pub->unit,
			wlc_cmi->wlc->pub->_dis_ch_grp_conf, wlc_cmi->wlc->pub->_dis_ch_grp_user));
		break;
	case IOV_GVAL(IOV_DIS_CH_GRP_USER):
		*((uint32 *)arg) = wlc_cmi->wlc->pub->_dis_ch_grp_user;
		WL_INFORM(("wl%d: ch grp conf: 0x%08x, user: 0x%08x\n", wlc_cmi->wlc->pub->unit,
			wlc_cmi->wlc->pub->_dis_ch_grp_conf, wlc_cmi->wlc->pub->_dis_ch_grp_user));
		break;
	case IOV_SVAL(IOV_DIS_CH_GRP_USER):
		if (IS_DIS_CH_GRP_VALID(int_val | wlc_cmi->wlc->pub->_dis_ch_grp_conf)) {
			wlc_cmi->wlc->pub->_dis_ch_grp_user = (uint32) int_val;
		} else {
			WL_ERROR(("wl%d: ignoring invalid dis_ch_grp from user 0x%x (conf: 0x%x)\n",
					wlc_cmi->wlc->pub->unit, int_val,
					wlc_cmi->wlc->pub->_dis_ch_grp_conf));
			err = BCME_BADARG;
		}
		WL_INFORM(("wl%d: ch grp conf: 0x%08x, user: 0x%08x\n", wlc_cmi->wlc->pub->unit,
			wlc_cmi->wlc->pub->_dis_ch_grp_conf, wlc_cmi->wlc->pub->_dis_ch_grp_user));
		break;

	case IOV_GVAL(IOV_IS_EDCRS_EU):
		*ret_int_ptr = wlc_cmi->wlc->is_edcrs_eu;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#ifdef WLC_TXPWRCAP
static void
wlc_txcap_high_cap_timer(void *arg)
{
	wlc_cmn_info_t* cmn = (wlc_cmn_info_t *)arg;

	wlc_cm_info_t *wlc_cmi;
	wlc_cm_data_t *wlc_cm;

	int idx = 0;
	wlc_info_t *wlc_cur = NULL;

	ASSERT(cmn->wlc[0]);
	ASSERT(cmn->wlc[0]->cmi);
	ASSERT(cmn->wlc[0]->cmi->cm);

	wlc_cm = cmn->wlc[0]->cmi->cm;
	WL_TXPWRCAP(("%s: txcap_high_cap_timer expired\n", __FUNCTION__));
	wlc_cm->txcap_high_cap_active = 0;

	FOREACH_WLC(cmn, idx, wlc_cur) {
		wlc_cmi = wlc_cur->cmi;
		wlc_channel_txcap_phy_update(wlc_cmi, NULL, NULL);
	}
}
#endif /* WLC_TXPWRCAP */

#if defined(BCMPCIEDEV_SROM_FORMAT) && defined(WLC_TXCAL)
static int
BCMATTACHFN(wlc_srom_read_blobdata)(wlc_info_t *wlc, wlc_cm_data_t *wlc_cm1)
{
	uint16 *buf;
	int caldata_size, retval;
	wl_dload_data_t *dload_data;
	wlc_info_t *wlc1;
	wlc_cm_data_t *wlc_cm;
	wlc_pub_t *pub = wlc->pub;
	wlc_cm = (wlc_cm_data_t *) wlc_cm1;
	const uint32 patrim_magic = PATRIM_MAGIC_SEQ;

	if (!RSDB_ENAB(wlc->pub) || wlc->pub->unit == 1) {
		uint32 srom_size = get_srom_size(wlc->pub->sromrev);

		if (srom_size == 0) {
			caldata_size = sizeof(wl_dload_data_t);
		} else {
			caldata_size = sizeof(wl_dload_data_t) + (srom_size - 1);
		}

		dload_data = (wl_dload_data_t *) MALLOCZ(wlc->pub->osh, caldata_size);
		if (dload_data == NULL) {
			WL_ERROR(("MALLOC FAILURE \n"));
			return BCME_ERROR;
		}
		dload_data->flag = DL_BEGIN | DL_END;
		dload_data->dload_type = DL_TYPE_CLM;
		dload_data->len = caldata_size - OFFSETOF(wl_dload_data_t, data);
		dload_data->crc = 0;
		buf = (uint16*)dload_data->data;

		if (!_initvars_srom_pci_caldata(wlc->pub->sih, buf, wlc->pub->sromrev)) {
			if (memcmp(buf, BLOB_LITERAL, sizeof(BLOB_LITERAL) - 1) == 0) {
				retval = wlc_blob_download(wlc_cm->clmload_wbi, dload_data->flag,
					(uint8 *)buf, dload_data->len,
					wlc_handle_cal_dload_wrapper);
				wlc->cmi->blobload_status = retval;
				if (RSDB_ENAB(wlc->pub)) {
					wlc1 = wlc_rsdb_get_other_wlc(wlc);
					wlc1->cmi->blobload_status = retval;
				}
				WL_ERROR(("Blob download status: retval:%d\n", retval));
			} else if (memcmp(buf, &patrim_magic, sizeof(patrim_magic)) == 0) {
				wlc_info_t *wlc_iter;
				uint idx = 0;
				uint32 hnd_crc32, crc_offset = 0;

				paoffset_srom_t * paoffset = (paoffset_srom_t *)buf;
				if (paoffset->num_slices == 1 || paoffset->num_slices == 2) {
					if (paoffset->num_slices == 1)
						crc_offset = paoffset->slice0_offset +
						paoffset->slice0_len;
					else if (paoffset->num_slices == 2)
						crc_offset = paoffset->slice1_offset +
						paoffset->slice1_len;

					hnd_crc32 = hndcrc32((uint8*)buf, crc_offset, 0xffffffff);
					hnd_crc32 = hnd_crc32 ^ 0xffffffff;
					if (*(uint32*)(buf + (crc_offset/2)) == hnd_crc32) {
						FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
							if (idx == 0) {
								wlc_phy_read_patrim_srom(
								wlc_iter->pi,
								(int16 *)(((uint8 *)buf)
								+ paoffset->slice0_offset),
								paoffset->slice0_len);
							} else if (idx == 1) {
								if (paoffset->num_slices == 2) {
									wlc_phy_read_patrim_srom(
									wlc_iter->pi,
									(int16 *)(((uint8 *)buf)
									+ paoffset->slice1_offset),
									paoffset->slice1_len);
								}
							}
						}
					}
					else {
						WL_ERROR(("CRC mismatch !!!\n"));
					}
				}
				else {
					WL_ERROR(("Invalid number of slices\n"));
				}

			} else {
				WL_ERROR(("Cal data in SROM Not valid\n"));
			}
		}
		else {
				WL_ERROR(("caldata read failed\n"));
		}
		MFREE(pub->osh, dload_data, sizeof(wl_dload_data_t));
	}

	return BCME_OK;
}
#endif /* BCMPCIEDEV_SROM_FORMAT && WLC_TXCAL */

static int
wlc_channel_ppr_ru_prealloc(wlc_cm_info_t *wlc_cmi)
{
	int ppr_ru_buf_i;
	uint buf_size = ppr_ru_size();
	wlc_cm_ppr_buffer_t *ppr_ru_buf;

	for (ppr_ru_buf_i = 0; ppr_ru_buf_i < PPR_RU_BUF_NUM; ppr_ru_buf_i++) {
		ppr_ru_buf = &(wlc_cmi->cm->ppr_ru_buf[ppr_ru_buf_i]);
		ppr_ru_buf->lock = 0;
		if ((ppr_ru_buf->txpwr = (int8 *)MALLOCZ(wlc_cmi->wlc->osh, buf_size)) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
			return BCME_NOMEM;
		}
	}
	return BCME_OK;
}

static void
wlc_channel_ppr_ru_prealloc_free(wlc_cm_info_t *wlc_cmi)
{
	int ppr_ru_buf_i;
	wlc_cm_ppr_buffer_t *ppr_ru_buf;

	for (ppr_ru_buf_i = 0; ppr_ru_buf_i < PPR_RU_BUF_NUM; ppr_ru_buf_i++) {
		ppr_ru_buf = &(wlc_cmi->cm->ppr_ru_buf[ppr_ru_buf_i]);
		if (ppr_ru_buf->txpwr) {
			MFREE(wlc_cmi->wlc->osh, ppr_ru_buf->txpwr, ppr_ru_size());
		}
	}
}

/* Release prealloc RU PPR buf */
static void
wlc_channel_release_prealloc_ppr_ru_buf(wlc_cm_info_t *wlc_cmi, int ppr_ru_buf_i)
{
	ASSERT(wlc_cmi->cm->ppr_ru_buf[ppr_ru_buf_i].lock != 0);
	wlc_cmi->cm->ppr_ru_buf[ppr_ru_buf_i].lock = 0;
}

/* Acquire ru ppr memory from available prealloc RU PPR buf */
static int
wlc_channel_acquire_ppr_ru_from_prealloc_buf(wlc_cm_info_t *wlc_cmi, ppr_ru_t **ru_txpwr)
{
	int ppr_ru_buf_i;
	wlc_cm_ppr_buffer_t *ppr_ru_buf;

	for (ppr_ru_buf_i = 0; ppr_ru_buf_i < PPR_BUF_NUM; ppr_ru_buf_i++) {
		ppr_ru_buf = &(wlc_cmi->cm->ppr_ru_buf[ppr_ru_buf_i]);
		if (ppr_ru_buf->lock == 1) {
			continue;
		}
		if ((*ru_txpwr = ppr_ru_create_prealloc(ppr_ru_buf->txpwr, ppr_ru_size()))
			== NULL) {
			return BCME_NOMEM;
		}
		ppr_ru_buf->lock = 1;
		return ppr_ru_buf_i;
	}
	/* Prealloc RU PPR buf is not expected to be insufficient */
	WL_ERROR(("wl%d: %s: Out of Prealloc RU PPR buf\n", wlc_cmi->pub->unit, __FUNCTION__));
	ASSERT(0);
	return BCME_NOMEM;
}

static int
wlc_channel_ppr_prealloc(wlc_cm_info_t *wlc_cmi)
{
	int ppr_buf_i;
	uint buf_size = ppr_size(ppr_get_max_bw());
	wlc_cm_ppr_buffer_t *ppr_buf;

	for (ppr_buf_i = 0; ppr_buf_i < PPR_BUF_NUM; ppr_buf_i++) {
		ppr_buf = &(wlc_cmi->cm->ppr_buf[ppr_buf_i]);
		ppr_buf->lock = 0;
		if ((ppr_buf->txpwr = (int8 *)MALLOCZ(wlc_cmi->wlc->osh, buf_size)) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
			return BCME_NOMEM;
		}
	}
	return BCME_OK;
}

static void
wlc_channel_ppr_prealloc_free(wlc_cm_info_t *wlc_cmi)
{
	int ppr_buf_i;
	wlc_cm_ppr_buffer_t *ppr_buf;

	for (ppr_buf_i = 0; ppr_buf_i < PPR_BUF_NUM; ppr_buf_i++) {
		ppr_buf = &(wlc_cmi->cm->ppr_buf[ppr_buf_i]);
		if (ppr_buf->txpwr) {
			MFREE(wlc_cmi->wlc->osh, ppr_buf->txpwr, ppr_size(ppr_get_max_bw()));
		}
	}
}

/* Release prealloc PPR buf */
static void
wlc_channel_release_prealloc_ppr_buf(wlc_cm_info_t *wlc_cmi, int ppr_buf_i)
{
	ASSERT(wlc_cmi->cm->ppr_buf[ppr_buf_i].lock != 0);
	wlc_cmi->cm->ppr_buf[ppr_buf_i].lock = 0;
}

/* Acquire ppr memory from available prealloc PPR buf */
static int
wlc_channel_acquire_ppr_from_prealloc_buf(wlc_cm_info_t *wlc_cmi, ppr_t **txpwr, wl_tx_bw_t bw)
{
	int ppr_buf_i;
	wlc_cm_ppr_buffer_t *ppr_buf;

	for (ppr_buf_i = 0; ppr_buf_i < PPR_BUF_NUM; ppr_buf_i++) {
		ppr_buf = &(wlc_cmi->cm->ppr_buf[ppr_buf_i]);
		if (ppr_buf->lock == 1) {
			continue;
		}
		if ((*txpwr = ppr_create_prealloc(bw, ppr_buf->txpwr, ppr_size(bw))) == NULL) {
			return BCME_NOMEM;
		}
		ppr_buf->lock = 1;
		return ppr_buf_i;
	}
	/* Prealloc PPR buf is not expected to be insufficient */
	WL_ERROR(("wl%d: %s: Out of Prealloc PPR buf\n", wlc_cmi->pub->unit, __FUNCTION__));
	ASSERT(0);
	return BCME_NOMEM;
}

wlc_cm_info_t *
BCMATTACHFN(wlc_channel_mgr_attach)(wlc_info_t *wlc)
{
	clm_result_t result;
	wlc_cm_info_t *wlc_cmi;
	wlc_cm_data_t *wlc_cm;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	wlc_pub_t *pub = wlc->pub;
	int ref_cnt;
	WL_TRACE(("wl%d: wlc_channel_mgr_attach\n", wlc->pub->unit));

	if ((wlc_cmi = (wlc_cm_info_t *)MALLOCZ(pub->osh, sizeof(wlc_cm_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes", pub->unit,
			__FUNCTION__, MALLOCED(pub->osh)));
		return NULL;
	}

	wlc_cmi->pub = pub;
	wlc_cmi->wlc = wlc;
	/* XXX TEMPORARY HACK.  Since wlc's cmi is set by the caller with the
	 * return value/handle, we can't use it via wlc until we return.  Yet
	 * because are slowly morphy the code in steps, we do indeed use it
	 * from inside wlc_channel.  This won't happen eventually.  So the hack:
	 * set wlc'c cmi from inside right now!
	 */
	wlc->cmi = wlc_cmi;

#if defined(WLC_TXPWRCAP) && !defined(WLC_TXPWRCAP_DISABLED)
	wlc->pub->_txpwrcap = TRUE;
#endif // endif

	wlc_cm = (wlc_cm_data_t *) obj_registry_get(wlc->objr, OBJR_CLM_PTR);

	if (wlc_cm == NULL) {
		if ((wlc_cm = (wlc_cm_data_t *) MALLOCZ(wlc->pub->osh,
			sizeof(wlc_cm_data_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes", wlc->pub->unit,
				__FUNCTION__, MALLOCED(wlc->pub->osh)));
			MFREE(pub->osh, wlc_cmi, sizeof(wlc_cm_info_t));
			return NULL;
		}

		if ((wlc_cm->clmload_wbi = wlc_blob_attach(wlc)) == NULL) {
			MFREE(pub->osh, wlc_cm, sizeof(wlc_cm_data_t));
			MFREE(pub->osh, wlc_cmi, sizeof(wlc_cm_info_t));
			return NULL;
		}
#if defined(WLC_TXPWRCAP)
		if (WLTXPWRCAP_ENAB(wlc)) {
			if ((wlc_cm->txcapload_wbi = wlc_blob_attach(wlc)) == NULL) {
				MODULE_DETACH(wlc_cm->clmload_wbi, wlc_blob_detach);
				MFREE(pub->osh, wlc_cm, sizeof(wlc_cm_data_t));
				MFREE(pub->osh, wlc_cmi, sizeof(wlc_cm_info_t));
				return NULL;
			}
			memset(wlc_cm->txcap_config, TXPWRCAPCONFIG_WCI2, TXPWRCAP_NUM_SUBBANDS);
			memset(wlc_cm->txcap_state, TXPWRCAPSTATE_LOW_CAP, TXPWRCAP_NUM_SUBBANDS);
			if (!(wlc_cm->txcap_high_cap_timer = wl_init_timer(wlc->wl,
				wlc_txcap_high_cap_timer,
				obj_registry_get(wlc->objr, OBJR_WLC_CMN_INFO),
				"txcap_high_cap"))) {
				WL_ERROR(("wl%d: %s wl_init_timer for txcap_high_cap_timer"
						"failed\n",
						wlc->pub->unit, __FUNCTION__));
				MODULE_DETACH(wlc_cm->txcapload_wbi, wlc_blob_detach);
				MODULE_DETACH(wlc_cm->clmload_wbi, wlc_blob_detach);
				MFREE(pub->osh, wlc_cm, sizeof(wlc_cm_data_t));
				MFREE(pub->osh, wlc_cmi, sizeof(wlc_cm_info_t));
				return NULL;
			}
			/* Last cell status is unknown */
			wlc_cm->txcap_wci2_cell_status_last = 2;
		}
#endif /* WLC_TXPWRCAP */
		obj_registry_set(wlc->objr, OBJR_CLM_PTR, wlc_cm);
	}
	ref_cnt = obj_registry_ref(wlc->objr, OBJR_CLM_PTR);
	wlc_cmi->cm = wlc_cm;

	/* init the per chain limits to max power so they have not effect */
	memset(&wlc_cm->bandstate[BAND_2G_INDEX].chain_limits, WLC_TXPWR_MAX,
	       sizeof(struct wlc_channel_txchain_limits));
	memset(&wlc_cm->bandstate[BAND_5G_INDEX].chain_limits, WLC_TXPWR_MAX,
	       sizeof(struct wlc_channel_txchain_limits));

	/* get the SPROM country code or local, required to initialize channel set below */
	bzero(country_abbrev, WLC_CNTRY_BUF_SZ);
	if (wlc->pub->sromrev > 1) {
		/* get country code */
		const char *ccode = getvar(wlc->pub->vars, rstr_ccode);
		if (ccode) {
#ifndef ATE_BUILD
#ifndef OPENSRC_IOV_IOCTL
			int err;
#endif /* OPENSRC_IOV_IOCTL */
#endif /* ATE_BUILD */
			strncpy(country_abbrev, ccode, WLC_CNTRY_BUF_SZ - 1);
#ifndef ATE_BUILD
#ifndef OPENSRC_IOV_IOCTL
			err = wlc_cntry_external_to_internal(country_abbrev,
				sizeof(country_abbrev));
			if (err != BCME_OK) {
				/* XXX Gross Hack. In non-BCMDBG non-WLTEST mode,
				 * we want to reject country abbreviations ALL and RDR.
				 * So convert them to something really bogus, and
				 * rely on other code to reject them in favour
				 * of a default.
				 */
				strncpy(country_abbrev, "__", WLC_CNTRY_BUF_SZ - 1);
			}
#endif /* OPENSRC_IOV_IOCTL */
#endif /* ATE_BUILD */
		} else {
			/* Internally and in the CLM xml use "ww" for legacy ccode of zero,
			   i.e. '\0\0'.  Some very old hardware used a zero ccode in SROM
			   as an aggregate identifier (along with a regrev).  There never
			   was an actual country code of zero but just some aggregates.
			   When the CLM was re-designed to use xml, a country code of zero
			   was no longer legal.  Any xml references were changed to "ww",
			   which was already being used as the preferred alternate to
			   using zero.  Here at attach time we are translating a SROM/NVRAM
			   value of zero to srom ccode value of "ww".  The below code makes
			   it look just like the SROM/NVRAM had the value of "ww" for these
			   very old hardware.
			 */
			strncpy(country_abbrev, "ww", WLC_CNTRY_BUF_SZ - 1);
		}
	} else {
		uint locale_num;

		/* get locale */
		locale_num = (uint)getintvar(wlc->pub->vars, rstr_cc);
		/* get mapped country */
		if (locale_num < ARRAYSIZE(def_country))
			strncpy(country_abbrev, def_country[locale_num],
			        sizeof(country_abbrev) - 1);
	}

#if defined(BCMDBG) || defined(WLTEST) || defined(ATE_BUILD)
	/* convert "ALL/0" country code to #a/0 */
	if (!strncmp(country_abbrev, "ALL", WLC_CNTRY_BUF_SZ)) {
		strncpy(country_abbrev, "#a", sizeof(country_abbrev) - 1);
	}
#endif /* BCMDBG || WLTEST || ATE_BUILD */

	/* Pre-alloc buf for PPR to avoid frequent mem alloc/free whenever the chanspec change */
	if (wlc_channel_ppr_prealloc(wlc_cmi)) {
		WL_ERROR(("wl%d: %s wlc_channel_ppr_prealloc failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Pre-alloc buf for RU PPR to avoid frequent mem alloc/free whenever the chanspec change */
	if (wlc_channel_ppr_ru_prealloc(wlc_cmi)) {
		WL_ERROR(("wl%d: %s wlc_channel_ppr_ru_prealloc failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (ref_cnt > 1) {
		/* Since the whole of cm_data_t is shared,
		* we have no clm_init for second instance
		*/
		goto skip_clm_init;
	}

	strncpy(wlc_cm->srom_ccode, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
	wlc_cm->srom_regrev = getintvar(wlc->pub->vars, rstr_regrev);

	/* Correct SROM contents of an Apple board */
	if ((pub->sih->boardvendor == VENDOR_APPLE) &&
	    (pub->sih->boardtype == 0x93) &&
	    !strncmp(country_abbrev, "JP", WLC_CNTRY_BUF_SZ) &&
	    (wlc_cm->srom_regrev == 4)) {
		wlc_cm->srom_regrev = 6;
	}

	/* XXX PR118982 WAR to fix mis-programmed regrev in SROM for the X51A
	 * WAR apply for SKU "X0", X2" & "X3" modules; change regrev to 19.
	 */
	if ((pub->sih->boardvendor == VENDOR_APPLE) &&
	    (pub->sih->boardtype == BCM94360X51A)) {
		if ((!strncmp(country_abbrev, "X0", WLC_CNTRY_BUF_SZ)) ||
		    (!strncmp(country_abbrev, "X2", WLC_CNTRY_BUF_SZ)) ||
		    (!strncmp(country_abbrev, "X3", WLC_CNTRY_BUF_SZ)))
		        wlc_cm->srom_regrev = 19;
	}

	result = clm_init(&clm_header);
	ASSERT(result == CLM_RESULT_OK);

	/* these are initialised to zero until they point to malloced data */
	wlc_cm->clm_base_dataptr = NULL;
	wlc_cm->clm_base_data_len = 0;

skip_clm_init:
	result = wlc_channel_init_ccode(wlc_cmi, country_abbrev, sizeof(country_abbrev));

	BCM_REFERENCE(result);

#ifdef WLTDLS
	/* setupreq */
	if (TDLS_ENAB(wlc->pub)) {
		if (wlc_ier_add_build_fn(wlc->ier_tdls_srq, DOT11_MNG_REGCLASS_ID,
			wlc_channel_tdls_calc_rc_ie_len, wlc_channel_tdls_write_rc_ie, wlc_cmi)
			!= BCME_OK) {
			WL_ERROR(("wl%d: %s wlc_ier_add_build_fn failed, "
				"reg class in tdls setupreq\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		/* setupresp */
		if (wlc_ier_add_build_fn(wlc->ier_tdls_srs, DOT11_MNG_REGCLASS_ID,
			wlc_channel_tdls_calc_rc_ie_len, wlc_channel_tdls_write_rc_ie, wlc_cmi)
			!= BCME_OK) {
			WL_ERROR(("wl%d: %s wlc_ier_add_build_fn failed, "
				"reg class in tdls setupresp\n", wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
	}
#endif /* WLTDLS */

	/* register module */
	if (wlc_module_register(wlc->pub, cm_iovars, "cm", wlc_cmi, wlc_cm_doiovar,
	    NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n", wlc->pub->unit, __FUNCTION__));

		goto fail;
	}

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "locale", wlc_channel_dump_locale, wlc);
	wlc_dump_register(wlc->pub, "txpwr_reg",
	                  (dump_fn_t)wlc_channel_dump_reg_ppr, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "txpwr_local",
	                  (dump_fn_t)wlc_channel_dump_reg_local_ppr, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "txpwr_srom",
	                  (dump_fn_t)wlc_channel_dump_srom_ppr, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "txpwr_margin",
	                  (dump_fn_t)wlc_channel_dump_margin, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "country_regrevs",
	                  (dump_fn_t)wlc_channel_supported_country_regrevs, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "agg_map",
	                  (dump_fn_t)wlc_dump_country_aggregate_map, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "txpwr_reg_max",
	                  (dump_fn_t)wlc_dump_max_power_per_channel, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "clm_limits_2G_20M",
	                  (dump_fn_t)wlc_dump_clm_limits_2G_20M, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "clm_limits_2G_40M",
	                  (dump_fn_t)wlc_dump_clm_limits_2G_40M, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "clm_limits_2G_20in40M",
	                  (dump_fn_t)wlc_dump_clm_limits_2G_20in40M, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "clm_limits_5G_20M",
	                  (dump_fn_t)wlc_dump_clm_limits_5G_20M, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "clm_limits_5G_40M",
	                  (dump_fn_t)wlc_dump_clm_limits_5G_40M, (void *)wlc_cmi);
	wlc_dump_register(wlc->pub, "clm_limits_5G_20in40M",
	                  (dump_fn_t)wlc_dump_clm_limits_5G_20in40M, (void *)wlc_cmi);
#endif // endif
#if defined(BCMPCIEDEV_SROM_FORMAT) && defined(WLC_TXCAL)
	if (wlc_srom_read_blobdata(wlc, wlc_cm) == BCME_ERROR)
		goto fail;
#endif // endif
	return wlc_cmi;

	goto fail;

fail:	/* error handling */
	wlc->cmi = NULL;
	MODULE_DETACH(wlc_cmi, wlc_channel_mgr_detach);
	return NULL;
}

static dload_error_status_t
wlc_handle_clm_dload(wlc_info_t *wlc, wlc_blob_segment_t *segments, uint32 segment_count)
{
	wlc_info_t *wlc_cur = NULL;
	int idx = 0;
	wlc_cm_info_t *wlc_cmi = wlc->cmi;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	dload_error_status_t status = DLOAD_STATUS_DOWNLOAD_SUCCESS;
	clm_result_t clm_result = CLM_RESULT_OK;

	clm_country_t country;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	struct clm_data_header* clm_dataptr;
	int clm_data_len;
	uint32 chip;

	/* Make sure we have a chip id segment and clm data segemnt */
	if (segment_count < 2)
		return DLOAD_STATUS_CLM_BLOB_FORMAT;

	/* Check to see if chip id segment is correct length */
	if (segments[1].length != 4)
		return DLOAD_STATUS_CLM_BLOB_FORMAT;

	/* Check to see if chip id matches this chip's actual value */
	chip = load32_ua(segments[1].data);
	if (chip != CHIPID(wlc_cmi->pub->sih->chip))
		return DLOAD_STATUS_CLM_MISMATCH;

	/* Process actual clm data segment */
	clm_dataptr = (struct clm_data_header *)(segments[0].data);
	clm_data_len = segments[0].length;

	/* At this point forward we are responsible for freeing this data pointer */
	segments[0].data = NULL;

	/* Free any previously downloaded base data */
	if (wlc_cm->clm_base_dataptr != NULL) {
		MFREE(wlc_cmi->pub->osh, wlc_cm->clm_base_dataptr,
			wlc_cm->clm_base_data_len);
	}
	if (clm_dataptr != NULL) {
		WL_NONE(("wl%d: Pointing API at new base data: v%s\n",
			wlc_cmi->pub->unit, clm_dataptr->clm_version));
		clm_result = clm_init(clm_dataptr);
		if (clm_result != CLM_RESULT_OK) {
			WL_ERROR(("wl%d: %s: Error loading new base CLM"
				" data.\n",
				wlc_cmi->pub->unit, __FUNCTION__));
			status = DLOAD_STATUS_CLM_DATA_BAD;
			MFREE(wlc_cmi->pub->osh, clm_dataptr,
				clm_data_len);
		}
	}
	if ((clm_dataptr == NULL) || (clm_result != CLM_RESULT_OK)) {
		WL_NONE(("wl%d: %s: Reverting to base data.\n",
			wlc_cmi->pub->unit, __FUNCTION__));
		clm_init(&clm_header);
		wlc_cm->clm_base_data_len = 0;
		wlc_cm->clm_base_dataptr = NULL;
	} else {
		wlc_cm->clm_base_dataptr = clm_dataptr;
		wlc_cm->clm_base_data_len = clm_data_len;
	}

	if (wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country) ==
		CLM_RESULT_OK)
		wlc_cm->country = country;
	else
		wlc_cm->country = 0;

	bzero(country_abbrev, WLC_CNTRY_BUF_SZ);
	strncpy(country_abbrev, wlc_cm->srom_ccode, WLC_CNTRY_BUF_SZ - 1);

	FOREACH_WLC(wlc->cmn, idx, wlc_cur) {
		wlc_cmi = wlc_cur->cmi;
		wlc_channel_init_ccode(wlc_cmi, country_abbrev, sizeof(country_abbrev));
	}

	return status;
}

void
BCMATTACHFN(wlc_channel_mgr_detach)(wlc_cm_info_t *wlc_cmi)
{
	if (wlc_cmi) {
		wlc_info_t *wlc = wlc_cmi->wlc;
		wlc_pub_t *pub = wlc->pub;

		wlc_module_unregister(wlc->pub, "cm", wlc_cmi);

		if (obj_registry_unref(wlc->objr, OBJR_CLM_PTR) == 0) {
			wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

#ifdef WLC_TXPWRCAP
			if (wlc_cm->txcap_high_cap_timer) {
				wl_del_timer(wlc->wl, wlc_cm->txcap_high_cap_timer);
				wl_free_timer(wlc->wl, wlc_cm->txcap_high_cap_timer);
			}
			wlc_blob_detach(wlc_cm->txcapload_wbi);
#endif /* WLC_TXPWRCAP */

			wlc_blob_detach(wlc_cm->clmload_wbi);

			if (wlc_cm->clm_base_dataptr != NULL) {
				MFREE(pub->osh, wlc_cm->clm_base_dataptr,
					wlc_cm->clm_base_data_len);
			}

			wlc_channel_ppr_prealloc_free(wlc_cmi);
			wlc_channel_ppr_ru_prealloc_free(wlc_cmi);

			obj_registry_set(wlc->objr, OBJR_CLM_PTR, NULL);
			MFREE(pub->osh, wlc_cm, sizeof(wlc_cm_data_t));
		}
		MFREE(pub->osh, wlc_cmi, sizeof(wlc_cm_info_t));
	}
}

const char* wlc_channel_country_abbrev(wlc_cm_info_t *wlc_cmi)
{
	return wlc_cmi->cm->country_abbrev;
}

const char* wlc_channel_ccode(wlc_cm_info_t *wlc_cmi)
{
	return wlc_cmi->cm->ccode;
}

const char* wlc_channel_sromccode(wlc_cm_info_t *wlc_cmi)
{
	return wlc_cmi->cm->srom_ccode;
}

uint wlc_channel_regrev(wlc_cm_info_t *wlc_cmi)
{
	return wlc_cmi->cm->regrev;
}

uint16 wlc_channel_locale_flags(wlc_cm_info_t *wlc_cmi)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return wlc_cmi->cm->bandstate[wlc->band->bandunit].locale_flags;
}

uint16 wlc_channel_locale_flags_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit)
{
	return wlc_cmi->cm->bandstate[bandunit].locale_flags;
}

/*
 * return the first valid chanspec for the locale, if one is not found and hw_fallback is true
 * then return the first h/w supported chanspec.
 */
chanspec_t
wlc_default_chanspec(wlc_cm_info_t *wlc_cmi, bool hw_fallback)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	chanspec_t  chspec;

	chspec = wlc_create_chspec(wlc, 0);
	/* try to find a chanspec that's valid in this locale */
	if ((chspec = wlc_next_chanspec(wlc_cmi, chspec, CHAN_TYPE_ANY, 0)) == INVCHANSPEC)
		/* try to find a chanspec valid for this hardware */
		if (hw_fallback)
			chspec = phy_utils_chanspec_band_firstch(
				(phy_info_t *)WLC_PI(wlc),
				wlc->band->bandtype);
	return chspec;
}

/*
 * Return the next channel's chanspec.
 */
chanspec_t
wlc_next_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t cur_chanspec, int type, bool any_band)
{
	uint8 ch;
	uint8 cur_chan = CHSPEC_CHANNEL(cur_chanspec);
	chanspec_t chspec;

	/* 0 is an invalid chspec, routines trying to find the first available channel should
	 * now be using wlc_default_chanspec (above)
	 */
	ASSERT(cur_chanspec);

	/* current channel must be valid */
	if (cur_chan > MAXCHANNEL) {
		return ((chanspec_t)INVCHANSPEC);
	}
	/* Try all channels in current band */
	ch = cur_chan + 1;
	for (; ch <= MAXCHANNEL; ch++) {
		if (ch == MAXCHANNEL)
			ch = 0;
		if (ch == cur_chan)
			break;
		/* create the next channel spec */
		chspec = cur_chanspec & ~WL_CHANSPEC_CHAN_MASK;
		chspec |= ch;
		if (wlc_valid_chanspec(wlc_cmi, chspec)) {
			if ((type == CHAN_TYPE_ANY) ||
			(type == CHAN_TYPE_CHATTY && !wlc_quiet_chanspec(wlc_cmi, chspec)) ||
			(type == CHAN_TYPE_QUIET && wlc_quiet_chanspec(wlc_cmi, chspec)))
				return chspec;
		}
	}

	if (!any_band)
		return ((chanspec_t)INVCHANSPEC);

	/* Couldn't find any in current band, try other band */
	ch = cur_chan + 1;
	for (; ch <= MAXCHANNEL; ch++) {
		if (ch == MAXCHANNEL)
			ch = 0;
		if (ch == cur_chan)
			break;

		/* create the next channel spec */
		chspec = cur_chanspec & ~(WL_CHANSPEC_CHAN_MASK | WL_CHANSPEC_BAND_MASK);
		chspec |= ch;
		if (CHANNEL_BANDUNIT(wlc, ch) == BAND_2G_INDEX)
			chspec |= WL_CHANSPEC_BAND_2G;
		else
			chspec |= WL_CHANSPEC_BAND_5G;
		if (wlc_valid_chanspec_db(wlc_cmi, chspec)) {
			if ((type == CHAN_TYPE_ANY) ||
			(type == CHAN_TYPE_CHATTY && !wlc_quiet_chanspec(wlc_cmi, chspec)) ||
			(type == CHAN_TYPE_QUIET && wlc_quiet_chanspec(wlc_cmi, chspec)))
				return chspec;
		}
	}

	return ((chanspec_t)INVCHANSPEC);
}

#if defined(WL_RESTRICTED_APSTA)
/* This function evaluates if APSTA
* is allowed on the given chanspec.
*/
bool
wlc_channel_apsta_restriction(wlc_cm_info_t *wlc_cmi, chanspec_t cur_chspec, chanspec_t tar_chspec)
{
	bool ret = FALSE;
	if (CHSPEC_IS5G(cur_chspec) && CHSPEC_IS5G(tar_chspec) &&
		(wf_chspec_ctlchan(cur_chspec) != wf_chspec_ctlchan(tar_chspec))) {
		ret = TRUE;
	}
	return ret;
}
#endif /* WL_RESTRICTED_APSTA */

chanspec_t
wlc_default_chanspec_by_band(wlc_cm_info_t *wlc_cmi, uint bandunit)
{
	chanspec_t chanspec;
	uint start_channel;
	if (bandunit == BAND_2G_INDEX) {
		start_channel = BAND_2G_START_CHANNEL;
	} else {
		start_channel = BAND_5G_START_CHANNEL;
	}
	chanspec = CH20MHZ_CHSPEC(start_channel);
	if (!wlc_valid_chanspec_db(wlc_cmi, chanspec)) {
		chanspec = wlc_next_chanspec(wlc_cmi,
			chanspec, CHAN_TYPE_ANY, TRUE);
	}
	return chanspec;
}

/* return chanvec for a given country code and band */
bool
wlc_channel_get_chanvec(struct wlc_info *wlc, const char* country_abbrev,
	int bandtype, chanvec_t *channels)
{
	clm_band_t band;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_t country;
	clm_country_locales_t locale;
	chanvec_t unused;
	uint i;
	chanvec_t channels_5g20, channels_5g4080;

	result = wlc_country_lookup(wlc, country_abbrev, &country);
	if (result != CLM_RESULT_OK)
		return FALSE;

	result = wlc_get_locale(country, &locale);
	if (bandtype != WLC_BAND_2G && bandtype != WLC_BAND_5G)
		return FALSE;

	band = (bandtype == WLC_BAND_2G) ? CLM_BAND_2G : CLM_BAND_5G;

	wlc_locale_get_channels(&locale, band, channels, &unused);
	clm_valid_channels_5g(&locale, (clm_channels_t*)&channels_5g20,
	(clm_channels_t*)&channels_5g4080);

	/* don't mask 2GHz channels, but check 5G channels */
	for (i = (CH_MAX_2G_CHANNEL / 8) + 1; i < sizeof(chanvec_t); i++)
	channels->vec[i] &= channels_5g20.vec[i];
	return TRUE;
}

static int
wlc_valid_countrycode_channels(wlc_cm_info_t *wlc_cmi, char *mapped_ccode, uint *mapped_regrev,
	clm_country_t country)
{
	clm_country_locales_t locale;
	uint16 flags;
	wlc_info_t *wlc = wlc_cmi->wlc;
	uint i, j, chan;
	wlcband_t * band;
	chanvec_t sup_chan, temp_chan;
	wlc_cm_band_t band_state[MAXBANDS];
	clm_result_t result = wlc_get_locale(country, &locale);

	if (result != CLM_RESULT_OK) {
		return BCME_BADARG;
	}

	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {
		clm_band_t tmp_band;

		/* Skip channel init for 2G band on band locked 5G core */
		if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap) && !BAND_5G(band->bandtype)) {
			continue;
		}

		if (BAND_5G(band->bandtype)) {
			tmp_band = CLM_BAND_5G;
		} else {
			tmp_band = CLM_BAND_2G;
		}
		result = wlc_get_flags(&locale, tmp_band, &flags);
		band_state[band->bandunit].locale_flags = flags;

		wlc_locale_get_channels(&locale, tmp_band,
			&band_state[band->bandunit].valid_channels,
			&temp_chan);

		/* set the channel availability,
		 * masking out the channels that may not be supported on this phy
		 */
		phy_utils_chanspec_band_validch(
			(phy_info_t *)WLC_PI_BANDUNIT(wlc, band->bandunit),
			band->bandtype, &sup_chan);
		for (j = 0; j < sizeof(chanvec_t); j++) {
			band_state[band->bandunit].valid_channels.vec[j] &=
				sup_chan.vec[j];
		}
	}

	/* search for the existence of any valid channel */
	for (chan = 0; chan < MAXCHANNEL; chan++) {
		if (isset(band_state[wlc->band->bandunit].valid_channels.vec, chan)) {
			break;
		}
	}
	if (chan == MAXCHANNEL) {
		WL_ERROR(("wl%d: %s: no valid channel for %s/%d. nbands %d band %s bandlocked %d\n",
			wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev, NBANDS(wlc),
			BAND_5G(wlc->band->bandtype) ? "a" : "b", wlc->bandlocked));
		return BCME_BADARG;
	}

	if ((band_state[wlc->band->bandunit].locale_flags & WLC_NO_MIMO) && N_ENAB(wlc->pub)) {
		WL_ERROR(("wl%d: %s: no MCS rates for %s/%d. nbands %d band %s bandlocked %d,"
			" nmode need to be disabled.\n", wlc->pub->unit, __FUNCTION__,
			mapped_ccode, *mapped_regrev, NBANDS(wlc),
			BAND_5G(wlc->band->bandtype) ? "a" : "b", wlc->bandlocked));
		return BCME_BADARG;
	}
	return BCME_OK;
}

int
wlc_valid_countrycode(wlc_cm_info_t *wlc_cmi, const char *country_abbrev, const char *ccode,
	int regrev)
{
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_t country;
	char mapped_ccode[WLC_CNTRY_BUF_SZ];
	uint mapped_regrev = 0;
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	wlc_info_t *wlc_iter;
	int idx, ret;

	if (ccode[0] == '\0') {
		/* map the country code to a built-in country code, regrev, and country_info */
		result = wlc_countrycode_map(wlc_cmi, country_abbrev, mapped_ccode,
			&mapped_regrev, &country);
	} else {
		/* find the matching built-in country definition */
		result = wlc_country_lookup_direct(ccode, regrev, &country);
		strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ-1);
		mapped_ccode[WLC_CNTRY_BUF_SZ-1] = '\0';
		mapped_regrev = regrev;
	}

	if (result != CLM_RESULT_OK) {
		return BCME_BADARG;
	}

	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		ret = wlc_valid_countrycode_channels(wlc_iter->cmi, mapped_ccode,
			&mapped_regrev, country);
		if (ret != BCME_OK)
			return ret;
	}

	return BCME_OK;
}

/* set the driver's current country and regulatory information using a country code
 * as the source. Lookup built in country information found with the country code.
 */
int
wlc_set_countrycode(wlc_cm_info_t *wlc_cmi, const char* ccode)
{
	int retval;
	WL_NONE(("wl%d: %s: ccode \"%s\"\n", wlc_cmi->wlc->pub->unit, __FUNCTION__, ccode));
	retval = wlc_set_countrycode_rev(wlc_cmi, ccode, -1);
	if (retval == BCME_OK)
		wlc_phy_set_country(WLC_PI(wlc_cmi->wlc), wlc_channel_ccode(wlc_cmi));
	else
		wlc_phy_set_country(WLC_PI(wlc_cmi->wlc), NULL);
	return retval;
}

int
wlc_set_countrycode_rev(wlc_cm_info_t *wlc_cmi, const char* ccode, int regrev)
{
#ifdef BCMDBG
	wlc_info_t *wlc = wlc_cmi->wlc;
#endif // endif
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_t country;
	char mapped_ccode[WLC_CNTRY_BUF_SZ];
	uint mapped_regrev = 0;
	char country_abbrev[WLC_CNTRY_BUF_SZ] = { 0 };

	WL_NONE(("wl%d: %s: (country_abbrev \"%s\", ccode \"%s\", regrev %d) SPROM \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__,
	         country_abbrev, ccode, regrev, wlc_cmi->cm->srom_ccode, wlc_cmi->cm->srom_regrev));

	/* if regrev is -1, lookup the mapped country code,
	 * otherwise use the ccode and regrev directly
	 */
	if (regrev == -1) {
		/* map the country code to a built-in country code, regrev, and country_info */
		result = wlc_countrycode_map(wlc_cmi, ccode, mapped_ccode,
			&mapped_regrev, &country);
		if (result == CLM_RESULT_OK) {
			WL_NONE(("wl%d: %s: mapped to \"%s\"/%u\n",
				WLCWLUNIT(wlc), __FUNCTION__, ccode, mapped_regrev));
		} else {
			WL_NONE(("wl%d: %s: failed lookup\n",
				WLCWLUNIT(wlc), __FUNCTION__));
		}
	} else {
		/* find the matching built-in country definition */
		result = wlc_country_lookup_direct(ccode, regrev, &country);
		strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ-1);
		mapped_ccode[WLC_CNTRY_BUF_SZ-1] = '\0';
		mapped_regrev = regrev;
	}

	if (result != CLM_RESULT_OK)
		return BCME_BADARG;

	/* Set the driver state for the country.
	 * Getting the advertised country code from CLM.
	 * Else use the one comes from ccode.
	 */
	if (wlc_lookup_advertised_cc(country_abbrev, country)) {
		wlc_set_country_common(wlc_cmi, country_abbrev,
		mapped_ccode, mapped_regrev, country);
	} else {
		wlc_set_country_common(wlc_cmi, ccode, mapped_ccode, mapped_regrev, country);
	}

	return 0;
}

/* set the driver's newband with the channels */
void wlc_channels_init_ext(wlc_cm_info_t *wlc_cmi)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

	wlc_channels_init(wlc_cmi, wlc_cm->country);
}

/* set the driver's current country and regulatory information using a country code
 * as the source. Look up built in country information found with the country code.
 */
static void
wlc_set_country_common(wlc_cm_info_t *wlc_cmi,
                       const char* country_abbrev,
                       const char* ccode, uint regrev, clm_country_t country)
{
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	uint16 flags;

	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	wlc_cmn_info_t *wlc_cmn = wlc->cmn;
	char prev_country_abbrev[WLC_CNTRY_BUF_SZ];
	unsigned long clm_flags = 0;
	wlc_info_t *wlc_iter;
	int idx;

#if defined WLTXPWR_CACHE && defined(WL11N)
	wlc_phy_txpwr_cache_invalidate(phy_tpc_get_txpwr_cache(WLC_PI(wlc)));
#endif	/* WLTXPWR_CACHE */

	/* Ensure NUL string terminator before printing */
	wlc_cm->country_abbrev[WLC_CNTRY_BUF_SZ - 1] = '\0';
	wlc_cm->ccode[WLC_CNTRY_BUF_SZ - 1] = '\0';
	WL_REGULATORY(("wl%d: %s: country/abbrev/ccode/regrev "
			"from 0x%04x/%s/%s/%d to 0x%04x/%s/%s/%d\n",
			wlc->pub->unit, __FUNCTION__,
			wlc_cm->country, wlc_cm->country_abbrev, wlc_cm->ccode, wlc_cm->regrev,
			country, country_abbrev, ccode, regrev));

	if (wlc_cm->country == country && wlc_cm->regrev == regrev &&
			wlc_cm->country_abbrev[0] && wlc_cm->ccode[0] &&
			strncmp(wlc_cm->country_abbrev, country_abbrev, WLC_CNTRY_BUF_SZ) == 0 &&
			strncmp(wlc_cm->ccode, ccode, WLC_CNTRY_BUF_SZ) == 0) {
		WL_REGULATORY(("wl%d: %s: Avoid setting current country again.\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* save current country state */
	wlc_cm->country = country;

	bzero(&prev_country_abbrev, WLC_CNTRY_BUF_SZ);
	strncpy(prev_country_abbrev, wlc_cm->country_abbrev, WLC_CNTRY_BUF_SZ - 1);

	strncpy(wlc_cm->country_abbrev, country_abbrev, WLC_CNTRY_BUF_SZ-1);
	strncpy(wlc_cm->ccode, ccode, WLC_CNTRY_BUF_SZ-1);
	wlc_cm->regrev = regrev;

	result = wlc_get_locale(country, &locale);
	ASSERT(result == CLM_RESULT_OK);

	result = wlc_get_flags(&locale, CLM_BAND_2G, &flags);
	ASSERT(result == CLM_RESULT_OK);
	BCM_REFERENCE(result);

	result = clm_country_flags(&locale,  CLM_BAND_2G, &clm_flags);
	ASSERT(result == CLM_RESULT_OK);

	/* In rsdb mode, when country iovar is issued with both cores up, country
	 * data is updated as cm_data is shared btw wlc's but few of unshared structs
	 * like txpwr, stf,phy etc are set only for wlc-0. Although wlc_bmac_set_chanspec
	 * takes care of this in wl up path, we need to ensure we set the right txpwr limits
	 * for each core, when changing ccode through iovar or any call to this API
	 */
	FOREACH_WLC(wlc_cmn, idx, wlc_iter) {
		if (clm_flags & CLM_FLAG_EDCRS_EU) {
			wlc_phy_set_locale((phy_info_t *)WLC_PI(wlc_iter), REGION_EU);
		} else {
			wlc_phy_set_locale((phy_info_t *)WLC_PI(wlc_iter), REGION_OTHER);
		}

#ifdef WL_BEAMFORMING
		if (TXBF_ENAB(wlc_iter->pub)) {
			if (flags & WLC_TXBF) {
				wlc_stf_set_txbf(wlc_iter, TRUE);
			} else {
				wlc_stf_set_txbf(wlc_iter, FALSE);
			}
		}
#endif // endif

#ifdef WL11N
		/* disable/restore nmode based on country regulations */
		if ((flags & WLC_NO_MIMO) && ((NBANDS(wlc_iter) == 2) ||
				IS_SINGLEBAND_5G(wlc_iter->deviceid, wlc->phy_cap))) {
			result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);
			ASSERT(result == CLM_RESULT_OK);
		}
		if (flags & WLC_NO_MIMO) {
			wlc_set_nmode(wlc_iter->hti, OFF);
			wlc_iter->stf->no_cddstbc = TRUE;
		} else {
			wlc_iter->stf->no_cddstbc = FALSE;
			wlc_prot_n_mode_reset(wlc_iter->prot_n, FALSE);
		}

		wlc_stf_ss_update(wlc_iter, wlc_iter->bandstate[BAND_2G_INDEX]);
		wlc_stf_ss_update(wlc_iter, wlc_iter->bandstate[BAND_5G_INDEX]);
#endif /* WL11N */

#if defined(AP) && defined(RADAR)
		if (RADAR_ENAB(wlc->pub) && ((NBANDS(wlc_iter) == 2) ||
			IS_SINGLEBAND_5G(wlc_iter->deviceid, wlc_iter->phy_cap))) {
			phy_radar_detect_mode_t mode;
			result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);

			mode = ISDFS_UK(flags) ? RADAR_DETECT_MODE_UK :
				ISDFS_EU(flags) ? RADAR_DETECT_MODE_EU : RADAR_DETECT_MODE_FCC;
			phy_radar_detect_mode_set((phy_info_t *)WLC_PI(wlc_iter), mode);
		}
#endif /* AP && RADAR */

		/* Set caps before wlc_channels_init() so recalc target has correct cap values */
#ifdef WLC_TXPWRCAP
		if (WLTXPWRCAP_ENAB(wlc)) {
			wlc_channel_txcap_set_country(wlc_cmi);
			/* In case driver is up, but current channel didn't change as a
			 * result of a country change, we need to make sure we update the
			 * phy txcaps based on this new country's values.
			 */
			/* XXX BUG? There are paths in wlc_channel_set_chanspec()
			 * which call wlc_bmac_set_chanspec() without first calling
			 * wlc_channel_reg_limits() to computing new txpwr limits for
			 * a newly * adopted country or possibly new power constraints.
			 * This seems to be related to txpwr caching/channel change optimizations.
			 * Did this code consider these other reasons for txpwr limit changes
			 * besides a channel change?
			 */
			wlc_channel_txcap_phy_update(wlc_cmi, NULL, NULL);
		}
#endif /* WLC_TXPWRCAP */

		wlc_channels_init(wlc_iter->cmi, country);
	}

	/* Country code changed */
	if (strlen(prev_country_abbrev) > 1 &&
	    strncmp(wlc_cm->country_abbrev, prev_country_abbrev,
	            strlen(wlc_cm->country_abbrev)) != 0) {
		/* need to reset chan_blocked */
		if (WLDFS_ENAB(wlc->pub) && wlc->dfs)
			wlc_dfs_reset_all(wlc->dfs);
		/* need to reset afe_override */
		wlc_channel_spurwar_locale(wlc_cmi, wlc->chanspec);

		wlc_mac_event(wlc, WLC_E_COUNTRY_CODE_CHANGED, NULL,
		              0, 0, 0, wlc_cm->country_abbrev, strlen(wlc_cm->country_abbrev) + 1);
	}
	else {
		if ((!strncmp(wlc_cm->country_abbrev, "#a", sizeof("#a") - 1)) &&
			WLDFS_ENAB(wlc->pub) && wlc->dfs)
			wlc_dfs_reset_all(wlc->dfs);
	}
#ifdef WLOLPC
	if (OLPC_ENAB(wlc_cmi->wlc)) {
		WL_RATE(("olpc process: ccrev=%s regrev=%d\n", ccode, regrev));
		/* olpc needs to flush any stored chan info and cal if needed */
		wlc_olpc_eng_reset(wlc_cmi->wlc->olpc_info);
	}
#endif /* WLOLPC */

	wlc_ht_frameburst_limit(wlc->hti);
	return;
}

/* returns true if current country's CLM flag matches EDCRS_EU.
 * (Some countries like Brazil will return false here though using DFS as in EU)
 * EDCRS_EU flag is defined in CLM for countries that follow EU harmonized standards.
 */
bool
wlc_is_edcrs_eu(struct wlc_info *wlc)
{
	wlc_cm_info_t *wlc_cmi = wlc->cmi;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	clm_result_t result;
	clm_country_locales_t locale;
	clm_country_t country;
	uint16 flags;
	bool edcrs_eu_flag_set;

	result = wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country);

	if (result != CLM_RESULT_OK) {
		return FALSE;
	}

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK) {
		return FALSE;
	}

	result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);
	if (result != CLM_RESULT_OK) {
		return FALSE;
	}

	edcrs_eu_flag_set = ((flags & WLC_EDCRS_EU) == WLC_EDCRS_EU);

	WL_REGULATORY(("wl%d: %s: EDCRS_EU flag is %sset for country %s (flags:0x%02X)\n",
			wlc->pub->unit, __FUNCTION__,
			edcrs_eu_flag_set?"":"not ", wlc_cm->ccode, flags));
	return edcrs_eu_flag_set;
}

#ifdef RADAR
/* returns true if current CLM flag matches DFS_EU or DFS_UK for the operation mode/band;
 * (Some countries like Brazil will return true here though not in EU/EDCRS)
 */
bool
wlc_is_dfs_eu_uk(struct wlc_info *wlc)
{
	wlc_cm_info_t *wlc_cmi = wlc->cmi;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	clm_result_t result;
	clm_country_locales_t locale;
	clm_country_t country;
	uint16 flags;

	if (!((NBANDS(wlc) == 2) || IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) &&
#ifdef BGDFS_2G
			!BGDFS_2G_ENAB(wlc->pub) &&
#endif /* BGDFS_2G */
			TRUE)
		return FALSE;

	result = wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country);
	if (result != CLM_RESULT_OK)
		return FALSE;

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK)
		return FALSE;

	result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);
	if (result != CLM_RESULT_OK)
		return FALSE;

	return (ISDFS_EU(flags) || ISDFS_UK(flags));
}

bool
wlc_is_european_weather_radar_channel(struct wlc_info *wlc, chanspec_t chanspec)
{
	uint8 weather_sb[] = { 120, 124, 128 }; /* EU weather channel 20MHz sidebands */
	uint8 channel, idx, weather_len = sizeof(weather_sb)/sizeof(weather_sb[0]);

	if ((!wlc_valid_chanspec_db(wlc->cmi, chanspec) && wlc->band->bandtype != WLC_BAND_2G) ||
			!wlc_is_dfs_eu_uk(wlc)) {
		return FALSE;
	}

	FOREACH_20_SB(chanspec, channel) {
		for (idx = 0; idx < weather_len; idx++) {
			if (channel == weather_sb[idx]) {
				return TRUE;
			}
		}
	}

	return FALSE;
}
#endif /* RADAR */

/* Lookup a country info structure from a null terminated country code
 * The lookup is case sensitive.
 */
clm_result_t
wlc_country_lookup(struct wlc_info *wlc, const char* ccode, clm_country_t *country)
{
	clm_result_t result = CLM_RESULT_ERR;
	char mapped_ccode[WLC_CNTRY_BUF_SZ];
	uint mapped_regrev;

	WL_NONE(("wl%d: %s: ccode \"%s\", SPROM \"%s\"/%u\n",
	        wlc->pub->unit, __FUNCTION__, ccode,
		wlc->cmi->cm->srom_ccode, wlc->cmi->cm->srom_regrev));

	/* map the country code to a built-in country code, regrev, and country_info struct */
	result = wlc_countrycode_map(wlc->cmi, ccode, mapped_ccode, &mapped_regrev, country);

	if (result == CLM_RESULT_OK)
		WL_NONE(("wl%d: %s: mapped to \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, mapped_ccode, mapped_regrev));
	else
		WL_NONE(("wl%d: %s: failed lookup\n",
		         wlc->pub->unit, __FUNCTION__));

	return result;
}

static clm_result_t
wlc_countrycode_map(wlc_cm_info_t *wlc_cmi, const char *ccode,
	char *mapped_ccode, uint *mapped_regrev, clm_country_t *country)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	clm_result_t result = CLM_RESULT_ERR;
	uint srom_regrev = wlc_cmi->cm->srom_regrev;
	const char *srom_ccode = wlc_cmi->cm->srom_ccode;
	int mapped;

	BCM_REFERENCE(wlc);

	/* check for currently supported ccode size */
	if (strlen(ccode) > (WLC_CNTRY_BUF_SZ - 1)) {
		WL_ERROR(("wl%d: %s: ccode \"%s\" too long for match\n",
			WLCWLUNIT(wlc), __FUNCTION__, ccode));
		return CLM_RESULT_ERR;
	}

	/* default mapping is the given ccode and regrev 0 */
	strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ);
	*mapped_regrev = 0;

	/* If the desired country code matches the srom country code,
	 * then the mapped country is the srom regulatory rev.
	 * Otherwise look for an aggregate mapping.
	 */
	if (!strcmp(srom_ccode, ccode)) {
		WL_NONE(("wl%d: %s: srom ccode and ccode \"%s\" match\n",
		         wlc->pub->unit, __FUNCTION__, ccode));
		*mapped_regrev = srom_regrev;
		mapped = 0;
	} else {
		mapped = wlc_country_aggregate_map(wlc_cmi, ccode, mapped_ccode, mapped_regrev);
		if (mapped) {
			WL_NONE(("wl%d: %s: found aggregate mapping \"%s\"/%u\n",
			         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));
		}
	}

	WL_NONE(("wl%d: %s: searching for country using ccode/rev \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));

	/* find the matching built-in country definition */
	result = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev, country);

	/* if there is not an exact rev match, default to rev zero */
	if (result != CLM_RESULT_OK && *mapped_regrev != 0) {
		*mapped_regrev = 0;
		WL_NONE(("wl%d: %s: No country found, use base revision \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));
		result = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev, country);
	}

	if (result != CLM_RESULT_OK)
		WL_NONE(("wl%d: %s: No country found, failed lookup\n",
		         wlc->pub->unit, __FUNCTION__));

	return result;
}

clm_result_t
clm_aggregate_country_lookup(const ccode_t cc, unsigned int rev, clm_agg_country_t *agg)
{
	return clm_agg_country_lookup(cc, rev, agg);
}

clm_result_t
clm_aggregate_country_map_lookup(const clm_agg_country_t agg, const ccode_t target_cc,
	unsigned int *rev)
{
	return clm_agg_country_map_lookup(agg, target_cc, rev);
}

static int
wlc_country_aggregate_map(wlc_cm_info_t *wlc_cmi, const char *ccode,
                          char *mapped_ccode, uint *mapped_regrev)
{
#ifdef BCMDBG
	wlc_info_t *wlc = wlc_cmi->wlc;
#endif // endif
	clm_result_t result;
	clm_agg_country_t agg = 0;
	const char *srom_ccode = wlc_cmi->cm->srom_ccode;
	uint srom_regrev = wlc_cmi->cm->srom_regrev;

	/* Check for a match in the aggregate country list */
	WL_NONE(("wl%d: %s: searching for agg map for srom ccode/rev \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));

	result = clm_aggregate_country_lookup(srom_ccode, srom_regrev, &agg);

	if (result != CLM_RESULT_OK)
		WL_NONE(("wl%d: %s: no map for \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));
	else
		WL_NONE(("wl%d: %s: found map for \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));

	if (result == CLM_RESULT_OK) {
		result = clm_aggregate_country_map_lookup(agg, ccode, mapped_regrev);
		strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ);
	}

	return (result == CLM_RESULT_OK);
}

#if defined(BCMDBG)
static int
wlc_dump_country_aggregate_map(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	const char *cur_ccode = wlc_cmi->cm->ccode;
	uint cur_regrev = wlc_cmi->cm->regrev;
	clm_agg_country_t agg = 0;
	clm_result_t result;
	int agg_iter;

	/* Use "ww", WorldWide, for the lookup value for '\0\0' */
	if (cur_ccode[0] == '\0')
		cur_ccode = "ww";

	clm_iter_init(&agg_iter);
	if ((result = clm_aggregate_country_lookup(cur_ccode, cur_regrev, &agg)) == CLM_RESULT_OK) {
		clm_agg_map_t map_iter;
		ccode_t cc;
		unsigned int rev;

		bcm_bprintf(b, "Map for %s/%u ->\n", cur_ccode, cur_regrev);
		clm_iter_init(&map_iter);
		while ((result = clm_agg_map_iter(agg, &map_iter, cc, &rev)) == CLM_RESULT_OK) {
			bcm_bprintf(b, "%c%c/%u\n", cc[0], cc[1], rev);
		}
	} else {
		bcm_bprintf(b, "No lookaside table for %s/%u\n", cur_ccode, cur_regrev);
	}
	return 0;

}
#endif // endif

/* Lookup a country info structure from a null terminated country
 * abbreviation and regrev directly with no translation.
 */
clm_result_t
wlc_country_lookup_direct(const char* ccode, uint regrev, clm_country_t *country)
{
	return clm_country_lookup(ccode, regrev, country);
}

#if defined(STA) && defined(WL11D)
/* Lookup a country info structure considering only legal country codes as found in
 * a Country IE; two ascii alpha followed by " ", "I", or "O".
 * Do not match any user assigned application specifc codes that might be found
 * in the driver table.
 */
clm_result_t
wlc_country_lookup_ext(wlc_info_t *wlc, const char *ccode, clm_country_t *country)
{
	clm_result_t result = CLM_RESULT_NOT_FOUND;
	char country_str_lookup[WLC_CNTRY_BUF_SZ] = { 0 };

	/* only allow ascii alpha uppercase for the first 2 chars, and " ", "I", "O" for the 3rd */
	/* Also allow operating classes supported for dot11CountryString as per Annex E in spec */
	if (!((0x80 & ccode[0]) == 0 && bcm_isupper(ccode[0]) &&
	      (0x80 & ccode[1]) == 0 && bcm_isupper(ccode[1]) &&
	      (ccode[2] == ' ' || ccode[2] == 'I' || ccode[2] == 'O'||
	       ccode[2] == BCMWIFI_RCLASS_TYPE_US || ccode[2] == BCMWIFI_RCLASS_TYPE_EU ||
	       ccode[2] == BCMWIFI_RCLASS_TYPE_JP || ccode[2] == BCMWIFI_RCLASS_TYPE_GBL)))
		return result;

	/* for lookup in the driver table of country codes, only use the first
	 * 2 chars, ignore the 3rd character " ", "I", "O" qualifier
	 */
	country_str_lookup[0] = ccode[0];
	country_str_lookup[1] = ccode[1];

	/* do not match ISO 3166-1 user assigned country codes that may be in the driver table */
	if (!strcmp("AA", country_str_lookup) ||	/* AA */
	    !strcmp("ZZ", country_str_lookup) ||	/* ZZ */
	    country_str_lookup[0] == 'X' ||		/* XA - XZ */
	    (country_str_lookup[0] == 'Q' &&		/* QM - QZ */
	     (country_str_lookup[1] >= 'M' && country_str_lookup[1] <= 'Z')))
		return result;

	return wlc_country_lookup(wlc, country_str_lookup, country);
}
#endif /* STA && WL11D */

#ifdef BGDFS_2G
/* use this for basic check about DFS radar channels on a 2G band locked card
 * Returns TRUE if the chanspec is a DFS radar channel else returns FALSE
 */
bool
wlc_channel_2g_dfs_chan(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	clm_result_t result;
	clm_country_locales_t locale;
	clm_country_t country;
	const uint8 *vec = radar_set1.vec;
	uint16 flags;
	uint8 channel;

	result = wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d country look up failed\n", WLCWLUNIT(wlc_cmi->wlc)));
		return FALSE;
	}

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d getting country locale failed\n", WLCWLUNIT(wlc_cmi->wlc)));
		return FALSE;
	}

	result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d getting locale flags failed\n", WLCWLUNIT(wlc_cmi->wlc)));
		return FALSE;
	}

	if (ISDFS_UK(flags)) {
		vec = radar_set_uk.vec;
	}

	FOREACH_20_SB(chspec, channel) {
		if (isset(vec, channel)) {
			return TRUE;
		}
	}
	WL_REGULATORY(("wl%d chspec 0x%x is NOT a radar channel\n",
		WLCWLUNIT(wlc_cmi->wlc), chspec));
	return FALSE;
}
int
wlc_channel_set_phy_radar_detect(wlc_cm_info_t *wlc_cmi)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	clm_result_t result;
	clm_country_locales_t locale;
	clm_country_t country;
	phy_radar_detect_mode_t mode;
	uint16 flags;

	result = wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d country look up failed\n", WLCWLUNIT(wlc_cmi->wlc)));
		return BCME_ERROR;
	}

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d getting country locale failed\n", WLCWLUNIT(wlc_cmi->wlc)));
		return BCME_ERROR;
	}

	result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);
	if (result != CLM_RESULT_OK) {
		WL_REGULATORY(("wl%d getting locale flags failed\n", WLCWLUNIT(wlc_cmi->wlc)));
		return BCME_ERROR;
	}

	mode = ISDFS_UK(flags) ? RADAR_DETECT_MODE_UK :
			ISDFS_EU(flags) ? RADAR_DETECT_MODE_EU : RADAR_DETECT_MODE_FCC;

	phy_radar_detect_mode_set((phy_info_t *)WLC_PI(wlc_cmi->wlc), mode);

	WL_REGULATORY(("wl%d %s phy_radar_detect_mode_set with mode %d flags 0x%x\n",
		WLCWLUNIT(wlc_cmi->wlc), __FUNCTION__, mode, flags));
	return BCME_OK;
}

#endif /* BGDFS_2G */

static void
wlc_channel_set_radar_chanvect(wlc_cm_data_t *wlc_cm, wlcband_t *band, uint16 flags)
{
	uint j;
	const uint8 *vec = NULL;

	/* Return when No Flag for DFS TPC */
	if (!(flags & WLC_DFS_TPC)) {
		return;
	}

	if (ISDFS_UK(flags)) {
		vec = radar_set_uk.vec;
	} else if (flags & WLC_DFS_TPC) {
		vec = radar_set1.vec;
	}

	for (j = 0; j < sizeof(chanvec_t); j++) {
		wlc_cm->bandstate[band->bandunit].radar_channels->vec[j] =
			vec[j] &
			wlc_cm->bandstate[band->bandunit].
			valid_channels.vec[j];
	}
}

static void
wlc_channels_init(wlc_cm_info_t *wlc_cmi, clm_country_t country)
{
	clm_country_locales_t locale;
	uint16 flags;
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	uint i, j;
	wlcband_t * band;
	chanvec_t sup_chan, temp_chan;
	bool switch_to_global_opclass = FALSE;

#if defined(WL_GLOBAL_RCLASS)
	switch_to_global_opclass = TRUE;
#endif // endif

	bzero(&wlc_cm->restricted_channels, sizeof(chanvec_t));
	bzero(&wlc_cm->locale_radar_channels, sizeof(chanvec_t));
	bzero(&wlc_cm->allowed_5g_20channels, sizeof(chanvec_t));
	bzero(&wlc_cm->allowed_5g_4080channels, sizeof(chanvec_t));

	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {
		clm_result_t result = wlc_get_locale(country, &locale);
		clm_band_t tmp_band;

		/* Skip channel init for 2G band on band locked 5G core */
		if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap) && !BAND_5G(band->bandtype)) {
			continue;
		}

		if (result == CLM_RESULT_OK) {
			if (BAND_5G(band->bandtype)) {
				tmp_band = CLM_BAND_5G;
			} else {
				tmp_band = CLM_BAND_2G;
			}
			result = wlc_get_flags(&locale, tmp_band, &flags);
			wlc_cm->bandstate[band->bandunit].locale_flags = flags;

			wlc_locale_get_channels(&locale, tmp_band,
				&wlc_cm->bandstate[band->bandunit].valid_channels,
				&temp_chan);
			/* initialize restricted channels */
			for (j = 0; j < sizeof(chanvec_t); j++) {
				wlc_cm->restricted_channels.vec[j] |= temp_chan.vec[j];
			}
#ifdef BAND5G /* RADAR */
			wlc_cm->bandstate[band->bandunit].radar_channels =
				&wlc_cm->locale_radar_channels;
			if (BAND_5G(band->bandtype)) {
				wlc_channel_set_radar_chanvect(wlc_cm, band, flags);
			}

			if (BAND_5G(band->bandtype)) {
				clm_valid_channels_5g(&locale,
				(clm_channels_t*)&wlc_cm->allowed_5g_20channels,
				(clm_channels_t*)&wlc_cm->allowed_5g_4080channels);
			 }
#endif /* BAND5G */

			/* set the channel availability,
			 * masking out the channels that may not be supported on this phy
			 */
			phy_utils_chanspec_band_validch(
				(phy_info_t *)WLC_PI_BANDUNIT(wlc, band->bandunit),
				band->bandtype, &sup_chan);
			wlc_locale_get_channels(&locale, tmp_band,
				&wlc_cm->bandstate[band->bandunit].valid_channels,
				&temp_chan);
			for (j = 0; j < sizeof(chanvec_t); j++)
				wlc_cm->bandstate[band->bandunit].valid_channels.vec[j] &=
					sup_chan.vec[j];
		}
	}

	wlc_upd_restricted_chanspec_flag(wlc_cmi);
	wlc_quiet_channels_reset(wlc_cmi);
	wlc_channels_commit(wlc_cmi);
	wlc_update_rcinfo(wlc_cmi, switch_to_global_opclass);
}

/* Update the radio state (enable/disable) and tx power targets
 * based on a new set of channel/regulatory information
 */
static int
wlc_channels_commit(wlc_cm_info_t *wlc_cmi)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_phy_t *pi = WLC_PI(wlc);
	uint chan;
	ppr_t* txpwr = NULL;
	ppr_ru_t *ru_txpwr = NULL;
	int ppr_buf_i, ppr_ru_buf_i;
	int ret = BCME_OK;

	/* search for the existence of any valid channel */
	for (chan = 0; chan < MAXCHANNEL; chan++) {
		if (VALID_CHANNEL20_DB(wlc, chan)) {
			break;
		}
	}
	if (chan == MAXCHANNEL)
		chan = INVCHANNEL;

	/* based on the channel search above, set or clear WL_RADIO_COUNTRY_DISABLE */
	if (chan == INVCHANNEL) {
		/* country/locale with no valid channels, set the radio disable bit */
		mboolset(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
		WL_ERROR(("wl%d: %s: no valid channel for \"%s\" nbands %d bandlocked %d\n",
		          wlc->pub->unit, __FUNCTION__,
		          wlc_cmi->cm->country_abbrev, NBANDS(wlc), wlc->bandlocked));
		ret = BCME_BADCHAN;
	} else if (mboolisset(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE)) {
		/* country/locale with valid channel, clear the radio disable bit */
		mboolclr(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
	}

	if (wlc->pub->up && chan != INVCHANNEL) {
		/* recompute tx power for new country info */

		/* XXX REVISIT johnvb  What chanspec should we use when changing country
		 * and where do we get it if we don't create/set it ourselves, i.e.
		 * wlc's chanspec or the phy's chanspec?  Also see the REVISIT comment
		 * in wlc_channel_set_txpower_limit().
		 */

		/* Where do we get a good chanspec? wlc, phy, set it ourselves? */

		ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc_cmi, &txpwr,
			PPR_CHSPEC_BW(wlc->chanspec));
		if (ppr_buf_i < 0)
			return BCME_NOMEM;
		ppr_ru_buf_i = wlc_channel_acquire_ppr_ru_from_prealloc_buf(wlc_cmi, &ru_txpwr);
		if (ppr_ru_buf_i < 0) {
			wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);
			return BCME_NOMEM;
		}

		wlc_channel_reg_limits(wlc_cmi, wlc->chanspec, txpwr, ru_txpwr);
#ifdef WL11AX
		wlc_phy_set_ru_power_limits(WLC_PI(wlc), ru_txpwr);
#endif /* WL11AX */

		/* XXX REVISIT johvnb  When setting a new country is it OK to erase
		 * the current 11h/local constraint?  If we do need to maintain the 11h
		 * constraint, we could either cache it here in the channel code or just
		 * have the higher level, non wlc_channel.c code, reissue a call to
		 * wlc_channel_set_txpower_limit with the old constraint.  This requires
		 * us to identify which of the few wlc_[channel]_set_countrycode calls could
		 * have a previous constraint constraint active.  This raises the
		 * related question of how a constraint goes away?  Caching the constraint
		 * here in wlc_channel would be logical, but unfortunately it doesn't
		 * currently have its own structure/state.
		 */
		ppr_apply_max(txpwr, WLC_TXPWR_MAX);
		/* Where do we get a good chanspec? wlc, phy, set it ourselves? */
		wlc_phy_txpower_limit_set(pi, txpwr, wlc->chanspec);

		wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);
		wlc_channel_release_prealloc_ppr_ru_buf(wlc_cmi, ppr_ru_buf_i);
	}

	return ret;
}
chanvec_t *
wlc_quiet_chanvec_get(wlc_cm_info_t *wlc_cmi)
{
	return &wlc_cmi->cm->quiet_channels;
}

chanvec_t *
wlc_valid_chanvec_get(wlc_cm_info_t *wlc_cmi, uint bandunit)
{
	return &wlc_cmi->cm->bandstate[bandunit].valid_channels;
}

/* reset the quiet channels vector to the union of the restricted and radar channel sets */
void
wlc_quiet_channels_reset(wlc_cm_info_t *wlc_cmi)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
#ifdef BAND5G
	wlc_info_t *wlc = wlc_cmi->wlc;
	uint i;
	wlcband_t *band;
#endif /* BAND5G */

	/* initialize quiet channels for restricted channels */
	bcopy(&wlc_cm->restricted_channels, &wlc_cm->quiet_channels, sizeof(chanvec_t));

#ifdef BAND5G /* RADAR */
	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {
		/* initialize quiet channels for radar if we are in spectrum management mode */
		if (WL11H_ENAB(wlc)) {
			uint j;
			const chanvec_t *chanvec;

			chanvec = wlc_cm->bandstate[band->bandunit].radar_channels;
			for (j = 0; j < sizeof(chanvec_t); j++)
				wlc_cm->quiet_channels.vec[j] |= chanvec->vec[j];
		}
	}
#endif /* BAND5G */
}

bool
wlc_quiet_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	wlc_pub_t *pub = wlc_cmi->wlc->pub;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	uint8 channel;

	/* minimal bandwidth configuration check */
	if ((CHSPEC_IS40(chspec) && !(VHT_ENAB(pub) || N_ENAB(pub))) ||
			((CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec) || CHSPEC_IS80(chspec)) &&
			!VHT_ENAB(pub))) {
		return FALSE;
	}

	FOREACH_20_SB(chspec, channel) {
		if (isset(wlc_cm->quiet_channels.vec, channel)) {
			return TRUE;
		}
	}

	return FALSE;
}

/* Mark only the non-matching part of the chspec_exclude(subset of chspec) as passive */
void
wlc_set_quiet_chanspec_exclude(wlc_cm_info_t *wlc_cmi, chanspec_t chspec, chanspec_t chspec_exclude)
{
	uint8 channel, i, idx = 0, exclude_arr[8];
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	bool must_exclude;

	FOREACH_20_SB(chspec_exclude, channel) {
		exclude_arr[idx++] = channel;
	}

	FOREACH_20_SB(chspec, channel) {
		must_exclude = FALSE;
		for (i = 0; i < idx; ++i) {
			if (exclude_arr[i] == channel) {
				must_exclude = TRUE;
				break;
			}
		}

		if (!must_exclude && wlc_radar_chanspec(wlc_cmi, CH20MHZ_CHSPEC(channel))) {
			setbit(wlc_cm->quiet_channels.vec, channel);
			WL_REGULATORY(("%s: Setting quiet bit for channel %d of chanspec 0x%x \n",
				__FUNCTION__, channel, chspec));
		}
	}
}

void
wlc_set_quiet_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	wlc_pub_t *pub = wlc_cmi->wlc->pub;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	uint8 channel;

	/* minimal bandwidth configuration check */
	if ((CHSPEC_IS40(chspec) && !(VHT_ENAB(pub) || N_ENAB(pub))) ||
			((CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec) || CHSPEC_IS80(chspec)) &&
			!VHT_ENAB(pub))) {
		return;
	}

	FOREACH_20_SB(chspec, channel) {
		if (wlc_radar_chanspec(wlc_cmi, CHBW_CHSPEC(WL_CHANSPEC_BW_20, channel))) {
			setbit(wlc_cm->quiet_channels.vec, channel);
		}
	}
}

void
wlc_clr_quiet_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	wlc_pub_t *pub = wlc_cmi->wlc->pub;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	uint8 channel;

	if (!wlc_dfs_valid_ap_chanspec(wlc_cmi->wlc, chspec)) {
		WL_REGULATORY(("wl%d:%s:chspec[%x] is blocked, return \n",
			wlc_cmi->wlc->pub->unit, __FUNCTION__, chspec));
		return;
	}
	/* minimal bandwidth configuration check */
	if ((CHSPEC_IS40(chspec) && !(VHT_ENAB(pub) || N_ENAB(pub))) ||
			((CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec) || CHSPEC_IS80(chspec)) &&
			!VHT_ENAB(pub))) {
		return;
	}

	FOREACH_20_SB(chspec, channel) {
		clrbit(wlc_cm->quiet_channels.vec, channel);
	}
}

/* Is the channel valid for the current locale? (but don't consider channels not
 *   available due to bandlocking)
 */
bool
wlc_valid_channel2p5_db(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return (VALID_CHANNEL2P5(wlc, val) ||
		(!wlc->bandlocked && VALID_CHANNEL2P5_IN_BAND(wlc, OTHERBANDUNIT(wlc), val)));
}

/* Is the channel valid for the current locale? (but don't consider channels not
 *   available due to bandlocking)
 */
bool
wlc_valid_channel5_db(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return (VALID_CHANNEL5(wlc, val) ||
		(!wlc->bandlocked && VALID_CHANNEL5_IN_BAND(wlc, OTHERBANDUNIT(wlc), val)));
}

/* Is the channel valid for the current locale? (but don't consider channels not
 *   available due to bandlocking)
 */
bool
wlc_valid_channel10_db(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return (VALID_CHANNEL10(wlc, val) ||
		(!wlc->bandlocked && VALID_CHANNEL10_IN_BAND(wlc, OTHERBANDUNIT(wlc), val)));
}

/* Is the channel valid for the current locale? (but don't consider channels not
 *   available due to bandlocking)
 */
bool
wlc_valid_channel20_db(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return (VALID_CHANNEL20(wlc, val) ||
		(!wlc->bandlocked && VALID_CHANNEL20_IN_BAND(wlc, OTHERBANDUNIT(wlc), val)));
}

/* Is the channel valid for the current locale and specified band? */
bool
wlc_valid_channel2p5_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit, uint val)
{
	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and specified band? */
bool
wlc_valid_channel5_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit, uint val)
{
	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and specified band? */
bool
wlc_valid_channel10_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit, uint val)
{
	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and specified band? */
bool
wlc_valid_channel20_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit, uint val)
{
	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and current band? */
bool
wlc_valid_channel2p5(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[wlc->band->bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and current band? */
bool
wlc_valid_channel5(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[wlc->band->bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and current band? */
bool
wlc_valid_channel10(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[wlc->band->bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and current band? */
bool
wlc_valid_channel20(wlc_cm_info_t *wlc_cmi, uint val)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	return ((val < MAXCHANNEL) &&
		isset(wlc_cmi->cm->bandstate[wlc->band->bandunit].valid_channels.vec, val));
}

/* Is the 40 MHz allowed for the current locale and specified band? */
bool
wlc_valid_40chanspec_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

	return (((wlc_cm->bandstate[bandunit].locale_flags & (WLC_NO_MIMO | WLC_NO_40MHZ)) == 0) &&
		wlc->pub->phy_bw40_capable);
}

/* Is 80 MHz allowed for the current locale and specified band? */
bool
wlc_valid_80chanspec_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

	return (((wlc_cm->bandstate[bandunit].locale_flags & (WLC_NO_MIMO | WLC_NO_80MHZ)) == 0) &&
		wlc->pub->phy_bw80_capable);
}

/* Is chanspec allowed to use 80Mhz bandwidth for the current locale? */
bool
wlc_valid_80chanspec(struct wlc_info *wlc, chanspec_t chanspec)
{
	uint16 locale_flags;

	locale_flags = wlc_channel_locale_flags_in_band((wlc)->cmi, CHSPEC_WLCBANDUNIT(chanspec));

	return (VHT_ENAB_BAND((wlc)->pub, (CHSPEC2WLC_BAND(chanspec))) &&
	 !(locale_flags & WLC_NO_80MHZ) &&
	 WL_BW_CAP_80MHZ((wlc)->bandstate[BAND_5G_INDEX]->bw_cap) &&
	 wlc_valid_chanspec_db((wlc)->cmi, (chanspec)));
}

/* Is 80+80 MHz allowed for the current locale and specified band? */
bool
wlc_valid_8080chanspec_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

	return (((wlc_cm->bandstate[bandunit].locale_flags & (WLC_NO_MIMO | WLC_NO_160MHZ)) == 0) &&
	        WL_BW_CAP_160MHZ(wlc->bandstate[bandunit]->bw_cap));
}

/* Is chanspec allowed to use 80+80Mhz bandwidth for the current locale? */
bool
wlc_valid_8080chanspec(struct wlc_info *wlc, chanspec_t chanspec)
{
	uint16 locale_flags;

	locale_flags = wlc_channel_locale_flags_in_band((wlc)->cmi, CHSPEC_WLCBANDUNIT(chanspec));

	return (VHT_ENAB_BAND((wlc)->pub, (CHSPEC2WLC_BAND(chanspec))) &&
	 !(locale_flags & WLC_NO_160MHZ) &&
	 WL_BW_CAP_160MHZ((wlc)->bandstate[BAND_5G_INDEX]->bw_cap) &&
	 wlc_valid_chanspec_db((wlc)->cmi, (chanspec)));
}

/* Is 160 MHz allowed for the current locale and specified band? */
bool
wlc_valid_160chanspec_in_band(wlc_cm_info_t *wlc_cmi, uint bandunit)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

	return (((wlc_cm->bandstate[bandunit].locale_flags & (WLC_NO_MIMO | WLC_NO_160MHZ)) == 0) &&
		wlc->pub->phy_bw160_capable);
}

/* Is chanspec allowed to use 160Mhz bandwidth for the current locale? */
bool
wlc_valid_160chanspec(struct wlc_info *wlc, chanspec_t chanspec)
{
	uint16 locale_flags;

	locale_flags = wlc_channel_locale_flags_in_band((wlc)->cmi, CHSPEC_WLCBANDUNIT(chanspec));

	return (VHT_ENAB_BAND((wlc)->pub, (CHSPEC2WLC_BAND(chanspec))) &&
	 !(locale_flags & WLC_NO_160MHZ) &&
	 WL_BW_CAP_160MHZ((wlc)->bandstate[BAND_5G_INDEX]->bw_cap) &&
	 wlc_valid_chanspec_db((wlc)->cmi, (chanspec)));
}

static void
wlc_channel_txpower_limits(wlc_cm_info_t *wlc_cmi, ppr_t *txpwr)
{
	uint8 local_constraint;
	wlc_info_t *wlc = wlc_cmi->wlc;

	wlc_channel_reg_limits(wlc_cmi, wlc->chanspec, txpwr, NULL);

	local_constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);

	if (!AP_ONLY(wlc->pub)) {
		ppr_apply_constraint_total_tx(txpwr, local_constraint);
	}
}

static void
wlc_channel_spurwar_locale(wlc_cm_info_t *wlc_cm, chanspec_t chanspec)
{
#if ACCONF
	wlc_info_t *wlc = wlc_cm->wlc;
	int override;
	uint rev;
	bool isCN, isX2, isX51A, dBpad;

	isX51A = (wlc->pub->sih->boardtype == BCM94360X51A) ? TRUE : FALSE;
	dBpad = (wlc->pub->boardflags4 & BFL4_SROM12_4dBPAD) ? 1 : 0;
	if (!isX51A && !dBpad)
		return;

	isCN = bcmp("CN", wlc_channel_country_abbrev(wlc_cm), 2) ? FALSE : TRUE;
	isX2 = bcmp("X2", wlc_channel_country_abbrev(wlc_cm), 2) ? FALSE : TRUE;
	rev = wlc_channel_regrev(wlc_cm);

	if (D11REV_LE(wlc->pub->corerev, 40)) {
		wlc->stf->coremask_override = SPURWAR_OVERRIDE_OFF;
		return;
	}

	if (wlc_iovar_getint(wlc, "phy_afeoverride", &override) == BCME_OK) {
		override &= ~PHY_AFE_OVERRIDE_DRV;
		wlc->stf->coremask_override = SPURWAR_OVERRIDE_OFF;
		if (CHSPEC_IS5G(chanspec) &&
		    ((isCN && rev == 204) || (isX2 && rev == 204) || /* X87 module */
		     (isCN && rev == 242) || (isX2 && rev == 2242) || /* X238D module */
		     (isX51A && ((isCN && rev == 40) || (isX2 && rev == 19))))) { /* X51A module */
			override |= PHY_AFE_OVERRIDE_DRV;
			if (isX51A || CHSPEC_IS20(chanspec))
				wlc->stf->coremask_override = SPURWAR_OVERRIDE_X51A;
			else if (dBpad)
				wlc->stf->coremask_override = SPURWAR_OVERRIDE_DBPAD;
		}
		wlc_iovar_setint(wlc, "phy_afeoverride", override);
	}
#endif /* ACCONF */
}

void
wlc_channel_set_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chanspec)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_stf_t *stf = wlc->stf;
	wlc_bsscfg_t *bsscfg = wlc->cfg;
	int ppr_buf_i, ppr_ru_buf_i;
	ppr_t* txpwr = NULL;
	ppr_ru_t *ru_txpwr = NULL;
	int8 local_constraint_qdbm;
	wl_txpwrcap_tbl_t *txpwrcap_tbl_ptr = NULL;
	int *cellstatus_ptr = NULL;
	bool update_cap = FALSE;
#if defined WLTXPWR_CACHE && defined(WL11N)
	tx_pwr_cache_entry_t* cacheptr;
#endif // endif
	/* bw 160/80p80MHz for 4366 */
	if (WLC_PHY_160_HALF_NSS(wlc)) {
		if (WLC_PHY_AS_80P80(wlc, chanspec)) {
			/* For 160Mhz or 80+80Mhz, if phy is 80p80. Only 2x2 or 1x1 are avaiable.
			 * We also need to adjust the mcsmap, 4->2, 2->1.
			 * txstreams and rxstreams must be at least 2.
			 */
			ASSERT((stf->txstreams > 1) && (stf->rxstreams > 1));
			/* Number of tx and rx streams must be even to combine
			 * two 80 streams for 160Mhz.
			 */
			ASSERT((stf->txstreams & 1) == 0 && (stf->rxstreams & 1) == 0);
			if (stf->op_txstreams >= 2) {
				stf->op_txstreams = stf->txstreams >> 1;
				stf->op_rxstreams = stf->rxstreams >> 1;
				update_cap = TRUE;
			}
		}
#ifdef DYN160
		/* When DYN160 is not enabled, NSS is limited to 2 */
		else if (!DYN160_ACTIVE(wlc->pub) && WL_BW_CAP_160MHZ(wlc->band->bw_cap) &&
				BSSCFG_STA(bsscfg)) {
			if (stf->op_txstreams >= 2) {
				stf->op_txstreams = stf->txstreams >> 1;
				stf->op_rxstreams = stf->rxstreams >> 1;
				update_cap = TRUE;
			}
		}
		/* bw <= 80MHz with DYN160 */
		else if (DYN160_ACTIVE(wlc->pub) &&
				((stf->op_txstreams == (stf->txstreams / 2)) &&
				(stf->op_rxstreams == (stf->rxstreams / 2)))) {
			/* If previous chanspec is 160Mhz or 80+80Mhz,
			 * restore the original txstreams and rxstreams for 80Mhz, 40Mhz, and 20Mhz.
			 */
			stf->op_txstreams = stf->txstreams;
			stf->op_rxstreams = stf->rxstreams;
			update_cap = TRUE;
		}
#endif /* DYN160 */
		/* bw <= 80MHz for STA */
		else if (BSSCFG_STA(bsscfg) &&
				((stf->op_txstreams == (stf->txstreams / 2)) ||
				(stf->op_rxstreams == (stf->rxstreams / 2)))) {
			stf->op_txstreams = stf->txstreams;
			stf->op_rxstreams = stf->rxstreams;
			update_cap = TRUE;
		}
	}

	WL_RATE(("wl%d: ch 0x%04x, u:%d, "
		"htc 0x%x, hrc 0x%x, htcc 0x%x, hrcc 0x%x, tc 0x%x, rc 0x%x, "
		"ts 0x%x, rs 0x%x, ots 0x%x, ors 0x%x\n",
		WLCWLUNIT(wlc), chanspec, update_cap, stf->hw_txchain, stf->hw_rxchain,
		stf->hw_txchain_cap, stf->hw_rxchain_cap, stf->txchain, stf->rxchain,
		stf->txstreams, stf->rxstreams, stf->op_txstreams, stf->op_rxstreams));
	if (update_cap) {
		wlc_ht_stbc_tx_set(wlc->hti, wlc->band->band_stf_stbc_tx);
		WL_INFORM(("Update STBC TX for VHT and HT cap for chanspec(0x%x)\n", chanspec));
		wlc_default_rateset(wlc, &bsscfg->current_bss->rateset);
	}

#ifdef WLC_TXPWRCAP
	if (WLTXPWRCAP_ENAB(wlc_cmi->wlc)) {
		if (wlc_cmi->cm->txcap_download != NULL) {
			txpwrcap_tbl_ptr = &(wlc_cmi->cm->txpwrcap_tbl);
			cellstatus_ptr = &(wlc_cmi->cm->cellstatus);
			wlc_channel_txcap_phy_update(wlc_cmi, txpwrcap_tbl_ptr, cellstatus_ptr);
		}
	}
#endif /* WLC_TXPWRCAP */
#if defined WLTXPWR_CACHE && defined(WL11N)
	cacheptr = phy_tpc_get_txpwr_cache(WLC_PI(wlc));
	if (wlc_phy_txpwr_cache_is_cached(cacheptr, chanspec) != TRUE) {
		int result;
		chanspec_t kill_chan = 0;

		BCM_REFERENCE(result);

		if (last_chanspec != 0)
			kill_chan = wlc_phy_txpwr_cache_find_other_cached_chanspec(cacheptr,
				last_chanspec);

		if (kill_chan != 0) {
			wlc_phy_txpwr_cache_clear(wlc_cmi->pub->osh, cacheptr, kill_chan);
		}
		result = wlc_phy_txpwr_setup_entry(cacheptr, chanspec);
		ASSERT(result == BCME_OK);
	}
	last_chanspec = chanspec;

	if ((wlc_phy_get_cached_txchain_offsets(cacheptr, chanspec, 0) != WL_RATE_DISABLED) &&
		wlc_phy_txpwr_cache_is_cached(cacheptr, chanspec)) {
		wlc_channel_update_txchain_offsets(wlc_cmi, NULL);
		wlc_bmac_set_chanspec(wlc->hw, chanspec,
			(wlc_quiet_chanspec(wlc_cmi, chanspec) != 0), NULL,
			txpwrcap_tbl_ptr, cellstatus_ptr);

		return;
	}
#endif /* WLTXPWR_CACHE */
	WL_TSLOG(wlc, __FUNCTION__, TS_ENTER, 0);

	ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc_cmi, &txpwr,
		PPR_CHSPEC_BW(chanspec));
	if (ppr_buf_i < 0)
		return;
	ppr_ru_buf_i = wlc_channel_acquire_ppr_ru_from_prealloc_buf(wlc_cmi, &ru_txpwr);
	if (ppr_ru_buf_i < 0) {
		wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);
		return;
	}
#ifdef SRHWVSDB
	/* If txpwr shms are saved, no need to go through power init fns again */
	if (wlc_srvsdb_save_valid(wlc, chanspec)) {
		goto apply_chanspec;
	}
#endif /* SRHWVSDB */
	wlc_channel_spurwar_locale(wlc_cmi, chanspec);
	wlc_channel_reg_limits(wlc_cmi, chanspec, txpwr, ru_txpwr);
	/* For APs, need to wait until reg limits are set before retrieving constraint. */
	local_constraint_qdbm = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);
	if (!AP_ONLY(wlc->pub)) {
		ppr_apply_constraint_total_tx(txpwr, local_constraint_qdbm);
	}

#ifdef WL11AX
	wlc_phy_set_ru_power_limits(WLC_PI(wlc), ru_txpwr);
#endif /* WL11AX */
	wlc_channel_update_txchain_offsets(wlc_cmi, txpwr);

	WL_TSLOG(wlc, "After wlc_channel_update_txchain_offsets", 0, 0);

#ifdef SRHWVSDB
apply_chanspec:
#endif /* SRHWVSDB */
	wlc_bmac_set_chanspec(wlc->hw, chanspec,
		(wlc_quiet_chanspec(wlc_cmi, chanspec) != 0),
		txpwr, txpwrcap_tbl_ptr, cellstatus_ptr);
	wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);
	wlc_channel_release_prealloc_ppr_ru_buf(wlc_cmi, ppr_ru_buf_i);
	WL_TSLOG(wlc, __FUNCTION__, TS_EXIT, 0);

}

int
wlc_channel_set_txpower_limit(wlc_cm_info_t *wlc_cmi, uint8 local_constraint_qdbm)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	int ppr_buf_i, ppr_ru_buf_i;
	ppr_t *txpwr = NULL;
	ppr_ru_t *ru_txpwr = NULL;
	int8 tx_maxpwr;
	int8 txpwr_cap_min = wlc_tpc_get_pwr_cap_min(wlc->tpc);

	/* XXX REVISIT johnvb  Make sure wlc->chanspec updated first!  This is a hack.
	 * Since the purpose of wlc_channel is to encapsulate the operations related to
	 * country/regulatory including protecting/enforcing the restrictions in partial
	 * open source builds, using wlc's "upper" copy of chanspec breaks the design.
	 * While it would be possible to use the "lower" phy copy via wlc_phy_get_chanspec()
	 * this would cause an extra call/RPC in a BMAC dongle build.  The "correct"
	 * solution is making wlc_channel into a "object" with its own structure/state,
	 * attach, detach, etc.
	 */

	if (!wlc->clk)
		return BCME_NOCLK;

	ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc_cmi, &txpwr,
		PPR_CHSPEC_BW(wlc->chanspec));
	if (ppr_buf_i < 0)
		return BCME_NOMEM;
	ppr_ru_buf_i = wlc_channel_acquire_ppr_ru_from_prealloc_buf(wlc_cmi, &ru_txpwr);
	if (ppr_ru_buf_i < 0) {
		wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);
		return BCME_NOMEM;
	}

	wlc_channel_reg_limits(wlc_cmi, wlc->chanspec, txpwr, ru_txpwr);

	if (!AP_ONLY(wlc->pub)) {
		ppr_apply_constraint_total_tx(txpwr, local_constraint_qdbm);
	}

#ifdef WL11AX
	wlc_phy_set_ru_power_limits(WLC_PI(wlc), ru_txpwr);
#endif /* WL11AX */

	tx_maxpwr = ppr_get_max(txpwr);

	/* Validate the new txpwr */
	if ((tx_maxpwr < txpwr_cap_min) ||
		(txpwr_cap_min == WL_RATE_DISABLED && tx_maxpwr <
		 wlc_phy_maxtxpwr_lowlimit(WLC_PI(wlc)))) {

		wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);
		wlc_channel_release_prealloc_ppr_ru_buf(wlc_cmi, ppr_ru_buf_i);

		return BCME_RANGE;
	}

#if defined WLTXPWR_CACHE && defined(WL11N)
	wlc_phy_txpwr_cache_invalidate(phy_tpc_get_txpwr_cache(WLC_PI(wlc)));
#endif	/* WLTXPWR_CACHE */

	wlc_channel_update_txchain_offsets(wlc_cmi, txpwr);

	wlc_phy_txpower_limit_set(WLC_PI(wlc), txpwr, wlc->chanspec);

	wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);
	wlc_channel_release_prealloc_ppr_ru_buf(wlc_cmi, ppr_ru_buf_i);

	return 0;
}

clm_limits_type_t clm_chanspec_to_limits_type(chanspec_t chspec)
{
	clm_limits_type_t lt = CLM_LIMITS_TYPE_CHANNEL;

	if (CHSPEC_IS40(chspec)) {
		switch (CHSPEC_CTL_SB(chspec)) {
		case WL_CHANSPEC_CTL_SB_L:
			lt = CLM_LIMITS_TYPE_SUBCHAN_L;
			break;
		case WL_CHANSPEC_CTL_SB_U:
			lt = CLM_LIMITS_TYPE_SUBCHAN_U;
			break;
		default:
			ASSERT(0);
			break;
		}
	}
#ifdef WL11AC
	else if (CHSPEC_IS80(chspec) || CHSPEC_IS8080(chspec)) {
		switch (CHSPEC_CTL_SB(chspec)) {
		case WL_CHANSPEC_CTL_SB_LL:
			lt = CLM_LIMITS_TYPE_SUBCHAN_LL;
			break;
		case WL_CHANSPEC_CTL_SB_LU:
			lt = CLM_LIMITS_TYPE_SUBCHAN_LU;
			break;
		case WL_CHANSPEC_CTL_SB_UL:
			lt = CLM_LIMITS_TYPE_SUBCHAN_UL;
			break;
		case WL_CHANSPEC_CTL_SB_UU:
			lt = CLM_LIMITS_TYPE_SUBCHAN_UU;
			break;
		default:
			ASSERT(0);
			break;
		}
	} else if (CHSPEC_IS160(chspec)) {
		switch (CHSPEC_CTL_SB(chspec)) {
		case WL_CHANSPEC_CTL_SB_LLL:
			lt = CLM_LIMITS_TYPE_SUBCHAN_LLL;
			break;
		case WL_CHANSPEC_CTL_SB_LLU:
			lt = CLM_LIMITS_TYPE_SUBCHAN_LLU;
			break;
		case WL_CHANSPEC_CTL_SB_LUL:
			lt = CLM_LIMITS_TYPE_SUBCHAN_LUL;
			break;
		case WL_CHANSPEC_CTL_SB_LUU:
			lt = CLM_LIMITS_TYPE_SUBCHAN_LUU;
			break;
		case WL_CHANSPEC_CTL_SB_ULL:
			lt = CLM_LIMITS_TYPE_SUBCHAN_ULL;
			break;
		case WL_CHANSPEC_CTL_SB_ULU:
			lt = CLM_LIMITS_TYPE_SUBCHAN_ULU;
			break;
		case WL_CHANSPEC_CTL_SB_UUL:
			lt = CLM_LIMITS_TYPE_SUBCHAN_UUL;
			break;
		case WL_CHANSPEC_CTL_SB_UUU:
			lt = CLM_LIMITS_TYPE_SUBCHAN_UUU;
			break;
		default:
			ASSERT(0);
			break;
		}
	}
#endif /* WL11AC */
	return lt;
}

#ifdef WL11AC
/* Converts limits_type of control channel to the limits_type of
 * the larger-BW subchannel(s) enclosing it (e.g. 40in80, 40in160, 80in160)
 */
clm_limits_type_t clm_get_enclosing_subchan(clm_limits_type_t ctl_subchan, uint lvl)
{
	clm_limits_type_t lt = ctl_subchan;
	if (lvl == 1) {
		/* Get 40in80 given 20in80, 80in160 given 20in160, 40in8080 given 20in8080 */
		switch (ctl_subchan) {
			case CLM_LIMITS_TYPE_SUBCHAN_LL:
			case CLM_LIMITS_TYPE_SUBCHAN_LU:
			case CLM_LIMITS_TYPE_SUBCHAN_LLL:
			case CLM_LIMITS_TYPE_SUBCHAN_LLU:
			case CLM_LIMITS_TYPE_SUBCHAN_LUL:
			case CLM_LIMITS_TYPE_SUBCHAN_LUU:
				lt = CLM_LIMITS_TYPE_SUBCHAN_L;
				break;
			case CLM_LIMITS_TYPE_SUBCHAN_UL:
			case CLM_LIMITS_TYPE_SUBCHAN_UU:
			case CLM_LIMITS_TYPE_SUBCHAN_ULL:
			case CLM_LIMITS_TYPE_SUBCHAN_ULU:
			case CLM_LIMITS_TYPE_SUBCHAN_UUL:
			case CLM_LIMITS_TYPE_SUBCHAN_UUU:
				lt = CLM_LIMITS_TYPE_SUBCHAN_U;
				break;
			default:
				break;
		}
	} else if (lvl == 2) {
		/* Get 40in160 given 20in160 */
		switch (ctl_subchan) {
			case CLM_LIMITS_TYPE_SUBCHAN_LLL:
			case CLM_LIMITS_TYPE_SUBCHAN_LLU:
				lt = CLM_LIMITS_TYPE_SUBCHAN_LL;
				break;
			case CLM_LIMITS_TYPE_SUBCHAN_LUL:
			case CLM_LIMITS_TYPE_SUBCHAN_LUU:
				lt = CLM_LIMITS_TYPE_SUBCHAN_LU;
				break;
			case CLM_LIMITS_TYPE_SUBCHAN_ULL:
			case CLM_LIMITS_TYPE_SUBCHAN_ULU:
				lt = CLM_LIMITS_TYPE_SUBCHAN_UL;
				break;
			case CLM_LIMITS_TYPE_SUBCHAN_UUL:
			case CLM_LIMITS_TYPE_SUBCHAN_UUU:
				lt = CLM_LIMITS_TYPE_SUBCHAN_UU;
				break;
			default: break;
		}
	}
	return lt;
}
#endif /* WL11AC */

#ifdef WL_SARLIMIT
static void
wlc_channel_sarlimit_get_default(wlc_cm_info_t *wlc_cmi, sar_limit_t *sar)
{
/* XXX Make this an empty function if WL_SARLIMIT_DISABLED is defined.  This is an
 * unusual usage of the dongle SAR_ENAB() configuration define to remove the references
 * to wlc_sar_tbl[] and wlc_sar_tbl_len which are defined in the generated file
 * wlc_sar_tbl.c.  When SAR is disabled and thus not used, we don't want to ship the
 * SAR "compiler".  You can not undefine the primary feature flag, WL_SARLIMIT, because
 * it controls the code and data structure generation and must stay consistent with the
 * definition used to build a ROM, otherwise you would get many abandoned functions and
 * data structures resulting in a large increase in RAM usage.  To complete the removal
 * of the sar tool, WL_SARLIMIT_DISABLED is also used in a couple of makefiles to remove
 * the dependency and the pattern rule.
 */
#ifndef WL_SARLIMIT_DISABLED
	wlc_info_t *wlc = wlc_cmi->wlc;
	uint idx;

	for (idx = 0; idx < wlc_sar_tbl_len; idx++) {
		if (wlc_sar_tbl[idx].boardtype == wlc->pub->sih->boardtype) {
			memcpy((uint8 *)sar, (uint8 *)&(wlc_sar_tbl[idx].sar), sizeof(sar_limit_t));
			break;
		}
	}
#endif /* WL_SARLIMIT_DISABLED */
}

void
wlc_channel_sar_init(wlc_cm_info_t *wlc_cmi)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

	memset((uint8 *)wlc_cm->sarlimit.band2g,
	       wlc->bandstate[BAND_2G_INDEX]->sar,
	       WLC_TXCORE_MAX);
	memset((uint8 *)wlc_cm->sarlimit.band5g,
	       wlc->bandstate[BAND_5G_INDEX]->sar,
	       (WLC_TXCORE_MAX * WLC_SUBBAND_MAX));

	wlc_channel_sarlimit_get_default(wlc_cmi, &wlc_cm->sarlimit);
#ifdef BCMDBG
	wlc_channel_sarlimit_dump(wlc_cmi, &wlc_cm->sarlimit);
#endif /* BCMDBG */
}

#ifdef BCMDBG
void
wlc_channel_sarlimit_dump(wlc_cm_info_t *wlc_cmi, sar_limit_t *sar)
{
	int i;

	BCM_REFERENCE(wlc_cmi);

	WL_ERROR(("\t2G:    %2d%s %2d%s %2d%s %2d%s\n",
	          QDB_FRAC(sar->band2g[0]), QDB_FRAC(sar->band2g[1]),
	          QDB_FRAC(sar->band2g[2]), QDB_FRAC(sar->band2g[3])));
	for (i = 0; i < WLC_SUBBAND_MAX; i++) {
		WL_ERROR(("\t5G[%1d]  %2d%s %2d%s %2d%s %2d%s\n", i,
		          QDB_FRAC(sar->band5g[i][0]), QDB_FRAC(sar->band5g[i][1]),
		          QDB_FRAC(sar->band5g[i][2]), QDB_FRAC(sar->band5g[i][3])));
	}
}
#endif /* BCMDBG */
int
wlc_channel_sarlimit_get(wlc_cm_info_t *wlc_cmi, sar_limit_t *sar)
{
	memcpy((uint8 *)sar, (uint8 *)&wlc_cmi->cm->sarlimit, sizeof(sar_limit_t));
	return 0;
}

int
wlc_channel_sarlimit_set(wlc_cm_info_t *wlc_cmi, sar_limit_t *sar)
{
	memcpy((uint8 *)&wlc_cmi->cm->sarlimit, (uint8 *)sar, sizeof(sar_limit_t));
	return 0;
}

/* given chanspec and return the subband index */
static uint
wlc_channel_sarlimit_subband_idx(wlc_cm_info_t *wlc_cmi, chanspec_t chanspec)
{
	uint8 chan = CHSPEC_CHANNEL(chanspec);

	BCM_REFERENCE(wlc_cmi);

	if (chan < CHANNEL_5G_MID_START)
		return 0;
	else if (chan >= CHANNEL_5G_MID_START && chan < CHANNEL_5G_HIGH_START)
		return 1;
	else if (chan >= CHANNEL_5G_HIGH_START && chan < CHANNEL_5G_UPPER_START)
		return 2;
	else
		return 3;
}

/* Get the sar limit for the subband containing this channel */
void
wlc_channel_sarlimit_subband(wlc_cm_info_t *wlc_cmi, chanspec_t chanspec, uint8 *sar)
{
	int idx = 0;

	if (CHSPEC_IS5G(chanspec)) {
		idx = wlc_channel_sarlimit_subband_idx(wlc_cmi, chanspec);
		memcpy((uint8 *)sar, (uint8 *)wlc_cmi->cm->sarlimit.band5g[idx], WLC_TXCORE_MAX);
	} else {
		memcpy((uint8 *)sar, (uint8 *)wlc_cmi->cm->sarlimit.band2g, WLC_TXCORE_MAX);
	}
}
#endif /* WL_SARLIMIT */

bool
wlc_channel_sarenable_get(wlc_cm_info_t *wlc_cmi)
{
	return (wlc_cmi->cm->sar_enable);
}

void
wlc_channel_sarenable_set(wlc_cm_info_t *wlc_cmi, bool state)
{
	wlc_cmi->cm->sar_enable = state ? TRUE : FALSE;
}

void
wlc_channel_reg_limits(wlc_cm_info_t *wlc_cmi, chanspec_t chanspec, ppr_t *txpwr,
	ppr_ru_t *ru_txpwr)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	unsigned int chan;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	clm_power_limits_t limits;
	clm_ru_power_limits_t ru_limits;
	uint16 flags;
	clm_band_t bandtype;
	wlcband_t * band;
	int ant_gain;
	clm_limits_params_t lim_params;
#ifdef WL_SARLIMIT
	uint8 sarlimit[WLC_TXCORE_MAX];
#endif // endif
	bool valid = TRUE;
	uint lim_count = 0;
	clm_limits_type_t lim_types[5];
	wl_tx_bw_t lim_ppr_bw[5];
	uint i;

	ppr_clear(txpwr);
	if (ru_txpwr) {
		ppr_ru_clear(ru_txpwr);
	}
	BCM_REFERENCE(result);

	if (clm_limits_params_init(&lim_params) != CLM_RESULT_OK) {
		ASSERT(0);
#ifdef DONGLEBUILD
		HND_DIE();
#endif /* DONGLEBUILD */
	}
	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	bandtype = BAND_5G(band->bandtype) ? CLM_BAND_5G : CLM_BAND_2G;
	/* XXX: JIRA:SWWLAN-36509: Locale prioritazation feature: On
	* 2.4 GHz ONLY, do not pick up the 11d region for scanning or
	* association. Use autocountry_default.
	*/
	/* Lookup channel in autocountry_default if not in current country */
	valid = wlc_valid_chanspec_db(wlc_cmi, chanspec);
	if ((WLC_LOCALE_PRIORITIZATION_2G_ENABLED(wlc) &&
		bandtype == CLM_BAND_2G && !WLC_CNTRY_DEFAULT_ENAB(wlc)) ||
		!(valid)) {
		if (!valid || WLC_AUTOCOUNTRY_ENAB(wlc)) {
			const char *def = wlc_11d_get_autocountry_default(wlc->m11d);
			result = wlc_country_lookup(wlc, def, &country);
			if (result != CLM_RESULT_OK) {
				ASSERT(0);
#ifdef DONGLEBUILD
				HND_DIE();
#endif /* DONGLEBUILD */
			}
		} else {
			country = wlc_cmi->cm->country;
		}
	} else
		country = wlc_cmi->cm->country;

	chan = CHSPEC_CHANNEL(chanspec);
	ant_gain = band->antgain;
	lim_params.sar = WLC_TXPWR_MAX;
	band->sar = band->sar_cached;
	if (wlc_cmi->cm->sar_enable) {
#ifdef WL_SARLIMIT
		/* when WL_SARLIMIT is enabled, update band->sar = MAX(sarlimit[i]) */
		wlc_channel_sarlimit_subband(wlc_cmi, chanspec, sarlimit);
		if ((CHIPID(wlc->pub->sih->chip) != BCM4360_CHIP_ID) &&
		    (CHIPID(wlc->pub->sih->chip) != BCM4350_CHIP_ID) &&
		    !BCM43602_CHIP(wlc->pub->sih->chip)) {
			uint i;
			band->sar = 0;
			for (i = 0; i < WLC_BITSCNT(wlc->stf->hw_txchain); i++)
				band->sar = MAX(band->sar, sarlimit[i]);

			WL_NONE(("%s: in %s Band, SAR %d apply\n", __FUNCTION__,
				(BAND_5G(wlc->band->bandtype) ? "5G" : "2G"), band->sar));
		}
		/* Don't write sarlimit to registers when called for reporting purposes */
		if (chanspec == wlc->chanspec) {
			uint32 sar_lims = (uint32)(sarlimit[0] | sarlimit[1] << 8 |
			                           sarlimit[2] << 16 | sarlimit[3] << 24);
#ifdef WLTXPWR_CACHE
			tx_pwr_cache_entry_t* cacheptr = phy_tpc_get_txpwr_cache(WLC_PI(wlc));

			wlc_phy_set_cached_sar_lims(cacheptr, chanspec, sar_lims);
#endif	/* WLTXPWR_CACHE */
			if (wlc->pub->up && wlc->clk) {
				wlc_phy_sar_limit_set(WLC_PI_BANDUNIT(wlc,
					band->bandunit), sar_lims);
			}
		}
#endif /* WL_SARLIMIT */
		lim_params.sar = band->sar;
	}

#if defined(BCMDBG) || defined(WLTEST)
	if (strcmp(wlc_cmi->cm->country_abbrev, "#a") == 0) {
		band->sar = WLC_TXPWR_MAX;
		lim_params.sar = WLC_TXPWR_MAX;
#ifdef WL_SARLIMIT
		if (wlc->clk) {
			wlc_phy_sar_limit_set(WLC_PI_BANDUNIT(wlc, band->bandunit),
				((WLC_TXPWR_MAX & 0xff) | (WLC_TXPWR_MAX & 0xff) << 8 |
				(WLC_TXPWR_MAX & 0xff) << 16 | (WLC_TXPWR_MAX & 0xff) << 24));
		}
#endif /* WL_SARLIMIT */
	}
#endif /* BCMDBG || WLTEST */
	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK) {
		ASSERT(0);
#ifdef DONGLEBUILD
		HND_DIE();
#endif /* DONGLEBUILD */
	}

	result = wlc_get_flags(&locale, bandtype, &flags);
	if (result != CLM_RESULT_OK) {
		ASSERT(0);
#ifdef DONGLEBUILD
		HND_DIE();
#endif /* DONGLEBUILD */
	}

	wlc_bmac_filter_war_upd(wlc->hw, FALSE);

	wlc_bmac_lo_gain_nbcal_upd(wlc->hw, (flags & WLC_LO_GAIN_NBCAL) != 0);

	/* Need to set the txpwr_local_max to external reg max for
	 * this channel as per the locale selected for AP.
	 */
#ifdef AP
#ifdef WLTPC
	if (AP_ONLY(wlc->pub)) {
		uint8 ch = wf_chspec_ctlchan(wlc->chanspec);
		uint8 pwr = wlc_get_reg_max_power_for_channel(wlc->cmi, ch, TRUE);
		wlc_tpc_set_local_max(wlc->tpc, pwr);
	}
#endif /* WLTPC */
#endif /* AP */

	lim_types[0] = CLM_LIMITS_TYPE_CHANNEL;
	switch (CHSPEC_BW(chanspec)) {

	case WL_CHANSPEC_BW_20:
		lim_params.bw = CLM_BW_20;
		lim_count = 1;
		lim_ppr_bw[0] = WL_TX_BW_20;
		break;

	case WL_CHANSPEC_BW_40:
		lim_params.bw = CLM_BW_40;
		lim_count = 2;
		lim_types[1] = clm_chanspec_to_limits_type(chanspec);
		lim_ppr_bw[0] = WL_TX_BW_40;
		lim_ppr_bw[1] = WL_TX_BW_20IN40;
		break;

#ifdef WL11AC
	case WL_CHANSPEC_BW_80: {
		clm_limits_type_t ctl_limits_type =
			clm_chanspec_to_limits_type(chanspec);

		lim_params.bw = CLM_BW_80;
		lim_count = 3;
		lim_types[1] = clm_get_enclosing_subchan(ctl_limits_type, 1);
		lim_types[2] = ctl_limits_type;
		lim_ppr_bw[0] = WL_TX_BW_80;
		lim_ppr_bw[1] = WL_TX_BW_40IN80;
		lim_ppr_bw[2] = WL_TX_BW_20IN80;
		break;
	}

	case WL_CHANSPEC_BW_160: {
		clm_limits_type_t ctl_limits_type =
			clm_chanspec_to_limits_type(chanspec);

		lim_params.bw = CLM_BW_160;
		lim_count = 4;
		lim_types[1] = clm_get_enclosing_subchan(ctl_limits_type, 1);
		lim_types[2] = clm_get_enclosing_subchan(ctl_limits_type, 2);
		lim_types[3] = ctl_limits_type;
		lim_ppr_bw[0] = WL_TX_BW_160;
		lim_ppr_bw[1] = WL_TX_BW_80IN160;
		lim_ppr_bw[2] = WL_TX_BW_40IN160;
		lim_ppr_bw[3] = WL_TX_BW_20IN160;
		break;
	}

	case WL_CHANSPEC_BW_8080: {
		clm_limits_type_t ctl_limits_type =
			clm_chanspec_to_limits_type(chanspec);

		chan = wf_chspec_primary80_channel(chanspec);
		lim_params.other_80_80_chan = wf_chspec_secondary80_channel(chanspec);

		lim_params.bw = CLM_BW_80_80;
		lim_count = 5;
		lim_types[1] = clm_get_enclosing_subchan(ctl_limits_type, 1);
		lim_types[2] = clm_get_enclosing_subchan(ctl_limits_type, 2);
		lim_types[3] = ctl_limits_type;
		lim_types[4] = CLM_LIMITS_TYPE_CHANNEL;  /* special case: 8080chan2 */
		lim_ppr_bw[0] = WL_TX_BW_8080;
		lim_ppr_bw[1] = WL_TX_BW_80IN8080;
		lim_ppr_bw[2] = WL_TX_BW_40IN8080;
		lim_ppr_bw[3] = WL_TX_BW_20IN8080;
		lim_ppr_bw[4] = WL_TX_BW_8080CHAN2;
		break;
	}
#endif /* WL11AC */

	default:
		ASSERT(0);
		break;
	}

	/* Calculate limits for each (sub)channel */
	for (i = 0; i < lim_count; i++) {
#ifdef WL11AC
		/* For 8080CHAN2, just swap primary and secondary channel
		 * and calculate 80MHz limit
		 */
		if (lim_ppr_bw[i] == WL_TX_BW_8080CHAN2) {
			lim_params.other_80_80_chan = chan;
			chan = wf_chspec_secondary80_channel(chanspec);
		}
#endif /* WL11AC */

		if (wlc_clm_power_limits(&locale, bandtype, chan, ant_gain, lim_types[i],
			&lim_params, (void *)&limits, FALSE) == CLM_RESULT_OK) {
			wlc_channel_read_bw_limits_to_ppr(wlc_cmi, txpwr, &limits, lim_ppr_bw[i]);
		}
		if (wlc_clm_power_limits(&locale, bandtype, chan, ant_gain, lim_types[i],
			&lim_params, (void *)&ru_limits, TRUE) == CLM_RESULT_OK) {
			wlc_channel_read_ub_bw_limits_to_ppr(wlc_cmi, txpwr, &ru_limits,
				lim_ppr_bw[i]);
			if ((ru_txpwr != NULL) && (i == 0)) {
				wlc_channel_read_ru_bw_limits_to_ppr(wlc_cmi, ru_txpwr, &ru_limits);
			}

		}
	}
	WL_NONE(("Channel(chanspec) %d (0x%4.4x)\n", chan, chanspec));
	/* Convoluted WL debug conditional execution of function to avoid warnings. */
	WL_NONE(("%s", (phy_tpc_dump_txpower_limits(WLC_PI_BANDUNIT(wlc, band->bandunit),
			txpwr), "")));
}

#define CLM_HE_RATESET(src, index) ((const ppr_he_mcs_rateset_t *)&src->limit[index])

static void
wlc_channel_read_bw_limits_to_ppr(wlc_cm_info_t *wlc_cmi, ppr_t *txpwr, clm_power_limits_t *limits,
	wl_tx_bw_t bw)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	uint max_chains = PHYCORENUM(WLC_BITSCNT(wlc->stf->hw_txchain));
	/* HE_ENAB ensure hardware HE capable */
	bool he_cap = HE_ENAB(wlc->pub) && (wlc->pub->he_features != 0);

	BCM_REFERENCE(wlc);		/* SISO chip has fixed vals for PHYCORENUM and TXBF_ENAB */

	/* Port the values for this bandwidth */
	ppr_set_dsss(txpwr, bw, WL_TX_CHAINS_1,	CLM_DSSS_RATESET(limits));

	ppr_set_ofdm(txpwr, bw, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		CLM_OFDM_1X1_RATESET(limits));

#ifdef WL11N
	ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		CLM_MCS_1X1_RATESET(limits));

	/* Set HE power limit if HE capability is TRUE */
	if (he_cap) {
		ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			CLM_HE_RATESET(limits, WL_RATE_1X1_HE0SS1), WL_HE_RT_SU);
	}

	if (max_chains > 1) {
		ppr_set_dsss(txpwr, bw, WL_TX_CHAINS_2, CLM_DSSS_1X2_MULTI_RATESET(limits));

		ppr_set_ofdm(txpwr, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			CLM_OFDM_1X2_CDD_RATESET(limits));

		ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
			CLM_MCS_1X2_CDD_RATESET(limits));

		ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_STBC, WL_TX_CHAINS_2,
			CLM_MCS_2X2_STBC_RATESET(limits));

		ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_2,
			CLM_MCS_2X2_SDM_RATESET(limits));

		if (he_cap) {
			ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_HE_RATESET(limits, WL_RATE_1X2_HE0SS1), WL_HE_RT_SU);

			ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_2,
				CLM_HE_RATESET(limits, WL_RATE_2X2_HE0SS2), WL_HE_RT_SU);
		}

		if (max_chains > 2) {
			ppr_set_dsss(txpwr, bw, WL_TX_CHAINS_3, CLM_DSSS_1X3_MULTI_RATESET(limits));

			ppr_set_ofdm(txpwr, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_MCS_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_STBC, WL_TX_CHAINS_3,
				CLM_MCS_2X3_STBC_RATESET(limits));

			ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_3,
				CLM_MCS_2X3_SDM_RATESET(limits));

			ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_3,
				CLM_MCS_3X3_SDM_RATESET(limits));

			if (he_cap) {
				ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, CLM_HE_RATESET(limits, WL_RATE_1X3_HE0SS1),
					WL_HE_RT_SU);

				ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, CLM_HE_RATESET(limits, WL_RATE_2X3_HE0SS2),
					WL_HE_RT_SU);

				ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, CLM_HE_RATESET(limits, WL_RATE_3X3_HE0SS3),
					WL_HE_RT_SU);
			}

			if (max_chains > 3) {
				ppr_set_dsss(txpwr, bw, WL_TX_CHAINS_4,
					CLM_DSSS_1X4_MULTI_RATESET(limits));

				ppr_set_ofdm(txpwr, bw, WL_TX_MODE_CDD, WL_TX_CHAINS_4,
					CLM_OFDM_1X4_CDD_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_4, CLM_MCS_1X4_CDD_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_STBC,
					WL_TX_CHAINS_4, CLM_MCS_2X4_STBC_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE,
					WL_TX_CHAINS_4, CLM_MCS_2X4_SDM_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_NONE,
					WL_TX_CHAINS_4, CLM_MCS_3X4_SDM_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_4, WL_TX_MODE_NONE,
					WL_TX_CHAINS_4, CLM_MCS_4X4_SDM_RATESET(limits));

				if (he_cap) {
					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_1X4_HE0SS1),
						WL_HE_RT_SU);

					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_NONE,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_2X4_HE0SS2),
						WL_HE_RT_SU);

					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_NONE,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_3X4_HE0SS3),
						WL_HE_RT_SU);

					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_4, WL_TX_MODE_NONE,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_4X4_HE0SS4),
						WL_HE_RT_SU);
				}
			}
		}
	}
#if defined(WL_BEAMFORMING)
	if (TXBF_ENAB(wlc->pub) && (max_chains > 1)) {
		ppr_set_ofdm(txpwr, bw, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
			CLM_OFDM_1X2_TXBF_RATESET(limits));

		ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
			CLM_MCS_1X2_TXBF_RATESET(limits));

		ppr_set_ht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
			CLM_MCS_2X2_TXBF_RATESET(limits));

		if (he_cap) {
			ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_HE_RATESET(limits, WL_RATE_1X2_TXBF_HE0SS1), WL_HE_RT_SU);

			ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_HE_RATESET(limits, WL_RATE_2X2_TXBF_HE0SS2), WL_HE_RT_SU);
		}

		if (max_chains > 2) {
			ppr_set_ofdm(txpwr, bw, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_MCS_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_MCS_2X3_TXBF_RATESET(limits));

			ppr_set_ht_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_MCS_3X3_TXBF_RATESET(limits));

			if (he_cap) {
				ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_3,
					CLM_HE_RATESET(limits, WL_RATE_1X3_TXBF_HE0SS1),
					WL_HE_RT_SU);

				ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_3,
					CLM_HE_RATESET(limits, WL_RATE_2X3_TXBF_HE0SS2),
					WL_HE_RT_SU);

				ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_3,
					CLM_HE_RATESET(limits, WL_RATE_3X3_TXBF_HE0SS3),
					WL_HE_RT_SU);
			}

			if (max_chains > 3) {
				ppr_set_ofdm(txpwr, bw, WL_TX_MODE_TXBF, WL_TX_CHAINS_4,
					CLM_OFDM_1X4_TXBF_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_4, CLM_MCS_1X4_TXBF_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_4, CLM_MCS_2X4_TXBF_RATESET(limits));

				ppr_set_vht_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_4, CLM_MCS_3X4_TXBF_RATESET(limits));

				ppr_set_ht_mcs(txpwr, bw, WL_TX_NSS_4, WL_TX_MODE_TXBF,
					WL_TX_CHAINS_4, CLM_MCS_4X4_TXBF_RATESET(limits));
				if (he_cap) {
					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_1, WL_TX_MODE_TXBF,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_1X4_TXBF_HE0SS1),
						WL_HE_RT_SU);

					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_2, WL_TX_MODE_TXBF,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_2X4_TXBF_HE0SS2),
						WL_HE_RT_SU);

					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_3, WL_TX_MODE_TXBF,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_3X4_TXBF_HE0SS3),
						WL_HE_RT_SU);

					ppr_set_he_mcs(txpwr, bw, WL_TX_NSS_4, WL_TX_MODE_TXBF,
						WL_TX_CHAINS_4,
						CLM_HE_RATESET(limits, WL_RATE_4X4_TXBF_HE0SS4),
						WL_HE_RT_SU);
				}
			}
		}
	}
#endif /* defined(WL_BEAMFORMING) */
#endif /* WL11N */
}

#define CLM_HE_UB_RATESET(src, index) ((const int8)src->limit[index])

/* Read DL-OFDMA UB and LUB CLM power limits to pprpbw_t */
static void
wlc_channel_read_ub_bw_limits_to_ppr(wlc_cm_info_t *wlc_cmi, ppr_t *txpwr,
	clm_ru_power_limits_t *ru_limits, wl_tx_bw_t bw)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	uint max_chains = PHYCORENUM(WLC_BITSCNT(wlc->stf->hw_txchain));
	wl_he_rate_type_t type;
	clm_ru_rates_t rate;
	uint offset, type_index = 0;
	int8 power;
	int tbl_index;
	/* HE_ENAB ensure hardware HE capable */
	bool he_cap = HE_ENAB(wlc->pub) && (wlc->pub->he_features != 0);

	BCM_REFERENCE(wlc);

	/* Set HE power limit if HE capability is TRUE */
	if (he_cap == FALSE) {
		return;
	}
	/* Port the values for this bandwidth */
	for (type = WL_HE_RT_UB; type <= WL_HE_RT_LUB; type++) {
		for (rate = WL_RU_RATE_1X1_UBSS1; rate <= WL_RU_RATE_4X4_TXBF_UBSS4; rate++) {
			tbl_index = rate - WL_RU_RATE_1X1_UBSS1;
			if (ru_rate_tbl[tbl_index].chain <= max_chains) {
				offset = type_index * WL_RU_NUM_MODE;
				power = CLM_HE_UB_RATESET(ru_limits, (rate + offset));
				ppr_set_same_he_mcs(txpwr, bw, ru_rate_tbl[tbl_index].nss,
					ru_rate_tbl[tbl_index].mode, ru_rate_tbl[tbl_index].chain,
					power, type);
			}
		}
		type_index++;
	}
}

/* Read UL-OFDMA CLM power limits to pprpbw_ru_t */
static void
wlc_channel_read_ru_bw_limits_to_ppr(wlc_cm_info_t *wlc_cmi, ppr_ru_t *ru_txpwr,
	clm_ru_power_limits_t *ru_limits)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	uint max_chains = PHYCORENUM(WLC_BITSCNT(wlc->stf->hw_txchain));
	wl_he_rate_type_t type;
	clm_ru_rates_t rate;
	uint offset, type_index = 0;
	int8 power;
	/* HE_ENAB ensure hardware HE capable */
	bool he_cap = HE_ENAB(wlc->pub) && (wlc->pub->he_features != 0);

	BCM_REFERENCE(wlc);

	/* Set HE power limit if HE capability is TRUE */
	if (he_cap == FALSE) {
		return;
	}
	/* Port the values for this bandwidth */
	for (type = WL_HE_RT_RU26; type <= WL_HE_RT_RU996; type++) {
		for (rate = WL_RU_RATE_1X1_26SS1; rate <= WL_RU_RATE_4X4_TXBF_26SS4; rate++) {
			if (ru_rate_tbl[rate].chain <= max_chains) {
				offset = type_index * WL_RU_NUM_MODE;
				power = CLM_HE_UB_RATESET(ru_limits, (rate + offset));
				ppr_set_same_he_ru_mcs(ru_txpwr, type, ru_rate_tbl[rate].nss,
					ru_rate_tbl[rate].mode, ru_rate_tbl[rate].chain, power);
			}
		}
		type_index++;
		if (type == WL_HE_RT_RU106) {
			/* Type UB and LUB are located between RU106 and RU242,
			 * refer to clm_ru_rates_t
			 */
			type_index += 2;
		}
	}
}

static clm_result_t
wlc_clm_power_limits(
	const clm_country_locales_t *locales, clm_band_t band,
	unsigned int chan, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, void *limits, bool ru)
{
	if (ru) {
		return clm_ru_limits(locales, band, chan, ant_gain, limits_type, params,
			(clm_ru_power_limits_t *)limits);
	} else {
		return clm_limits(locales, band, chan, ant_gain, limits_type, params,
			(clm_power_limits_t *)limits);
	}
}

/* Returns TRUE if currently set country is Japan or variant */
bool
wlc_japan(struct wlc_info *wlc)
{
	return wlc_japan_ccode(wlc->cmi->cm->country_abbrev);
}

/* JP, J1 - J10 are Japan ccodes */
static bool
wlc_japan_ccode(const char *ccode)
{
	return (ccode[0] == 'J' &&
		(ccode[1] == 'P' || (ccode[1] >= '1' && ccode[1] <= '9')));
}

/* Q2 and Q1 are alternate USA ccode */
static bool
wlc_us_ccode(const char *ccode)
{
	return (!strncmp("US", ccode, WLC_CNTRY_BUF_SZ - 1) ||
		!strncmp("Q1", ccode, WLC_CNTRY_BUF_SZ - 1) ||
		!strncmp("Q2", ccode, WLC_CNTRY_BUF_SZ - 1) ||
		!strncmp("ALL", ccode, WLC_CNTRY_BUF_SZ - 1));
}

bool
BCMATTACHFN(wlc_is_ccode_lockdown)(wlc_info_t *wlc)
{
#ifdef CCODE_LOCKDOWN
	char *vars = wlc->pub->vars;
	char *s, *ccode;
	int len;
	char name[] = "ccode";

	len = strlen(name);

	if (!vars)
		return FALSE;

	/* look in vars[] */
	for (s = vars; s && *s;) {
		if ((bcmp(s, name, len) == 0) && (s[len] == '=')) {
			ccode = (&s[len+1]);
			if (wlc_lockdown_country_lookup(ccode))
				return TRUE;
		}
		while (*s++)
			;
	}
#endif /* CCODE_LOCKDOWN */
	return FALSE;
}

static void
wlc_regclass_vec_init(wlc_cm_info_t *wlc_cmi)
{
	uint8 i, idx;
	chanspec_t chanspec;
#ifdef WL11N
	wlc_info_t *wlc = wlc_cmi->wlc;
	bool saved_cap_40, saved_db_cap_40 = TRUE;
#endif // endif
#ifdef WL11AC
	bool saved_cap_80;
#endif // endif
#ifdef WL11AC_160
	bool saved_cap_160;
#endif // endif
	rcvec_t *rcvec = &wlc_cmi->cm->valid_rcvec;

#ifdef WL11N
	/* save 40 MHz cap */
	saved_cap_40 = WL_BW_CAP_40MHZ(wlc->band->bw_cap);
	wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	if (NBANDS(wlc) > 1) {
		saved_db_cap_40 = WL_BW_CAP_40MHZ(wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap);
		wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_40MHZ_BIT;
	}
#endif // endif
#ifdef WL11AC
	/* save 80 MHZ cap */
	saved_cap_80 = WL_BW_CAP_80MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap);
	wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_80MHZ_BIT;
#endif /* WL11AC */
#ifdef WL11AC_160
	/* save 160 MHZ cap */
	saved_cap_160 = WL_BW_CAP_160MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap);
	wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_160MHZ_BIT;
#endif /* WL11AC_160 */

	bzero(rcvec, MAXRCVEC);
	for (i = 0; i < MAXCHANNEL; i++) {
		chanspec = CH20MHZ_CHSPEC(i);
		if (wlc_valid_chanspec_db(wlc_cmi, chanspec)) {
			if ((idx = wlc_get_regclass(wlc_cmi, chanspec)))
				setbit((uint8 *)rcvec, idx);
		}
#if defined(WL11N) && !defined(WL11N_20MHZONLY)
		if (N_ENAB(wlc->pub)) {
			chanspec = CH40MHZ_CHSPEC(i, WL_CHANSPEC_CTL_SB_LOWER);
			if (wlc_valid_chanspec_db(wlc_cmi, chanspec)) {
				if ((idx = wlc_get_regclass(wlc_cmi, chanspec))) {
					setbit((uint8 *)rcvec, idx);
				}
			}
			chanspec = CH40MHZ_CHSPEC(i, WL_CHANSPEC_CTL_SB_UPPER);
			if (wlc_valid_chanspec_db(wlc_cmi, chanspec)) {
				if ((idx = wlc_get_regclass(wlc_cmi, chanspec))) {
					setbit((uint8 *)rcvec, idx);
				}
			}
		}
#endif /* defined(WL11N) && !defined(WL11N_20MHZONLY) */
#ifdef WL11AC
		if (VHT_ENAB(wlc->pub)) {
			chanspec = CH80MHZ_CHSPEC(i, WL_CHANSPEC_CTL_SB_LL);
			if (wlc_valid_chanspec(wlc_cmi, chanspec)) {
				if ((idx = wlc_get_regclass(wlc_cmi, chanspec)))
					setbit((uint8 *)rcvec, idx);
			}
		}
#endif // endif
	}
/* restore 160 MHZ cap */
#ifdef WL11AC_160
	if (saved_cap_160) {
		wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_160MHZ_BIT;
	} else {
		wlc->bandstate[BAND_5G_INDEX]->bw_cap &= ~WLC_BW_160MHZ_BIT;
	}
#endif /* WL11AC_160 */
/* restore 80 MHZ cap */
#ifdef WL11AC
	if (saved_cap_80) {
		wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_80MHZ_BIT;
	} else {
		wlc->bandstate[BAND_5G_INDEX]->bw_cap &= ~WLC_BW_80MHZ_BIT;
	}
#endif /* WL11AC */
#ifdef WL11N
	/* restore 40 MHz cap */
	if (saved_cap_40) {
		wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	} else {
		wlc->band->bw_cap &= ~WLC_BW_40MHZ_BIT;
	}

	if (NBANDS(wlc) > 1) {
		if (saved_db_cap_40) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_40MHZ_BIT;
		} else {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap &= ~WLC_BW_40MHZ_BIT;
		}
	}
#endif // endif
}

#ifdef WL11N
uint8
wlc_rclass_extch_get(wlc_cm_info_t *wlc_cmi, uint8 rclass)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	const rcinfo_t *rcinfo;
	uint8 i, extch = DOT11_EXT_CH_NONE;

	if (!isset(wlc_cm->valid_rcvec.vec, rclass)) {
		WL_ERROR(("wl%d: %s %d regulatory class not supported\n",
			wlc_cmi->wlc->pub->unit, wlc_cm->country_abbrev, rclass));
		return extch;
	}

	/* rcinfo consist of control channel at lower sb */
	rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40L];
	for (i = 0; rcinfo && i < rcinfo->len; i++) {
		if (rclass == rcinfo->rctbl[i].rclass) {
			/* ext channel is opposite of control channel */
			extch = DOT11_EXT_CH_UPPER;
			goto exit;
		}
	}

	/* rcinfo consist of control channel at upper sb */
	rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40U];
	for (i = 0; rcinfo && i < rcinfo->len; i++) {
		if (rclass == rcinfo->rctbl[i].rclass) {
			/* ext channel is opposite of control channel */
			extch = DOT11_EXT_CH_LOWER;
			break;
		}
	}
exit:
	WL_INFORM(("wl%d: %s regulatory class %d has ctl chan %s\n",
		wlc_cmi->wlc->pub->unit, wlc_cm->country_abbrev, rclass,
		((!extch) ? "NONE" : (((extch == DOT11_EXT_CH_LOWER) ? "LOWER" : "UPPER")))));

	return extch;
}
#endif /* WL11N */

#if defined(BCMDBG) || defined(WLTDLS) || defined(WL_MBO)
/* get the ordered list of supported reg class, with current reg class
 * as first element
 */
uint8
wlc_get_regclass_list(wlc_cm_info_t *wlc_cmi, uint8 *rclist, uint lsize,
	chanspec_t chspec, bool ie_order)
{
	uint8 i, cur_rc = 0, idx = 0;

	ASSERT(rclist != NULL);
	ASSERT(lsize > 1);

	if (ie_order) {
		cur_rc = wlc_get_regclass(wlc_cmi, chspec);
		if (!cur_rc) {
			return 0;
		}
		rclist[idx++] = cur_rc;	/* first element is current reg class */
	}

	for (i = 0; i < MAXREGCLASS && idx < lsize; i++) {
		if (isset(wlc_cmi->cm->valid_rcvec.vec, i))
			rclist[idx++] = i;
	}

	if (i < MAXREGCLASS && idx == lsize) {
		WL_ERROR(("wl%d: regulatory class list full %d\n", wlc_cmi->wlc->pub->unit, idx));
		ASSERT(0);
	}

	return idx;
}
#endif // endif

static uint8
wlc_get_2g_regclass(wlc_cm_info_t *wlc_cmi, uint8 chan)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
#ifdef WL_GLOBAL_RCLASS
	if (wlc_cm->cur_rclass == BCMWIFI_RCLASS_TYPE_GBL) {
		if (chan < 14) {
			return WLC_REGCLASS_GLOBAL_2G_20MHZ;
		} else {
			return WLC_REGCLASS_GLOBAL_2G_20MHZ_CH14;
		}
		BCM_REFERENCE(wlc_cm);
	}
#endif /* WL_GLOBAL_RCLASS */
	if (wlc_us_ccode(wlc_cm->country_abbrev))
		return WLC_REGCLASS_USA_2G_20MHZ;
	else if (wlc_japan_ccode(wlc_cm->country_abbrev)) {
		if (chan < 14)
			return WLC_REGCLASS_JPN_2G_20MHZ;
		else
			return WLC_REGCLASS_JPN_2G_20MHZ_CH14;
	} else
		return WLC_REGCLASS_EUR_2G_20MHZ;
}

uint8
wlc_get_regclass(wlc_cm_info_t *wlc_cmi, chanspec_t chanspec)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	const rcinfo_t *rcinfo = NULL;
	uint8 i;
	uint8 chan = CHSPEC_IS40(chanspec) ? wf_chspec_ctlchan(chanspec) : CHSPEC_CHANNEL(chanspec);

#ifdef WL11AC
	if (CHSPEC_IS80(chanspec)) {
		rcinfo = wlc_cm->rcinfo_list_11ac[WLC_RCLIST_80];
#ifdef WL11AC_160
	} else if (CHSPEC_IS160(chanspec)) {
		rcinfo = wlc_cm->rcinfo_list_11ac[WLC_RCLIST_160];
#endif /* WL11AC_160 */
	} else
#endif /* WL11AC */

#ifdef WL11N
	if (CHSPEC_IS40(chanspec)) {
		if (CHSPEC_SB_UPPER(chanspec))
			rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40U];
		else
			rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40L];
	} else
#endif /* WL11N */
	{
		if (CHSPEC_IS2G(chanspec))
			return (wlc_get_2g_regclass(wlc_cmi, chan));
		rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_20];
	}

	for (i = 0; rcinfo != NULL && i < rcinfo->len; i++) {
		if (chan == rcinfo->rctbl[i].chan)
			return (rcinfo->rctbl[i].rclass);
	}

	WL_ERROR(("wl%d: No regulatory class assigned for %s chanspec 0x%x\n",
		WLCWLUNIT(wlc_cmi->wlc), wlc_cm->country_abbrev, chanspec));

	return 0;
}

#if defined(BCMDBG)
void
wlc_dump_rclist(const char *name, uint8 *rclist, uint8 rclen, struct bcmstrbuf *b)
{
	uint i;

	if (!rclen)
		return;

	bcm_bprintf(b, "%s [ ", name ? name : "");
	for (i = 0; i < rclen; i++) {
		bcm_bprintf(b, "%d ", rclist[i]);
	}
	bcm_bprintf(b, "]");
	bcm_bprintf(b, "\n");

	return;
}

/* format a qdB value as integer and decimal fraction in a bcmstrbuf */
static void
wlc_channel_dump_qdb(struct bcmstrbuf *b, int qdb)
{
	if ((qdb >= 0) || (qdb % WLC_TXPWR_DB_FACTOR == 0))
		bcm_bprintf(b, "%2d%s", QDB_FRAC(qdb));
	else
		bcm_bprintf(b, "%2d%s",
			qdb / WLC_TXPWR_DB_FACTOR + 1,
			fraction[WLC_TXPWR_DB_FACTOR - (qdb % WLC_TXPWR_DB_FACTOR)]);
}

/* helper function for wlc_channel_dump_txppr() to print one set of power targets with label */
static void
wlc_channel_dump_pwr_range(struct bcmstrbuf *b, const char *label, int8 *ptr, uint count)
{
	uint i;

	bcm_bprintf(b, "%s ", label);
	for (i = 0; i < count; i++) {
		if (ptr[i] != WL_RATE_DISABLED) {
			wlc_channel_dump_qdb(b, ptr[i]);
			bcm_bprintf(b, " ");
		} else
			bcm_bprintf(b, "-     ");
	}
	bcm_bprintf(b, "\n");
}

/* helper function to print a target range line with the typical 8 targets */
static void
wlc_channel_dump_pwr_range8(struct bcmstrbuf *b, const char *label, int8* ptr)
{
	wlc_channel_dump_pwr_range(b, label, (int8*)ptr, 8);
}

#ifdef WL11AC

#define NUM_MCS_RATES WL_NUM_RATES_VHT
#define CHSPEC_TO_TX_BW(c)	(\
	CHSPEC_IS8080(c) ? WL_TX_BW_8080 : \
	(CHSPEC_IS160(c) ? WL_TX_BW_160 : \
	(CHSPEC_IS80(c) ? WL_TX_BW_80 : \
	(CHSPEC_IS40(c) ? WL_TX_BW_40 : WL_TX_BW_20))))

#else

#define NUM_MCS_RATES WL_NUM_RATES_MCS_1STREAM
#define CHSPEC_TO_TX_BW(c)	(CHSPEC_IS40(c) ? WL_TX_BW_40 : WL_TX_BW_20)

#endif // endif

/* helper function to print a target range line with the typical 8 targets */
static void
wlc_channel_dump_pwr_range_mcs(struct bcmstrbuf *b, const char *label, int8 *ptr)
{
	wlc_channel_dump_pwr_range(b, label, (int8*)ptr, NUM_MCS_RATES);
}

/* format the contents of a ppr_t structure for a bcmstrbuf */
static void
wlc_channel_dump_txppr(struct bcmstrbuf *b, ppr_t *txpwr, wl_tx_bw_t bw, wlc_info_t *wlc)
{
	ppr_dsss_rateset_t dsss_limits;
	ppr_ofdm_rateset_t ofdm_limits;
	ppr_vht_mcs_rateset_t mcs_limits;

	if (bw == WL_TX_BW_20) {
		bcm_bprintf(b, "\n20MHz:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_2, &dsss_limits);
			wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
				WL_RATESET_SZ_DSSS);
			ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_3, &dsss_limits);
				wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ",
					dsss_limits.pwr, WL_RATESET_SZ_DSSS);
				ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_4,
						&dsss_limits);
					wlc_channel_dump_pwr_range(b,  "DSSS_MULTI3       ",
						dsss_limits.pwr, WL_RATESET_SZ_DSSS);
					ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}
	} else if (bw == WL_TX_BW_40) {

		bcm_bprintf(b, "\n40MHz:\n");
		ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

		bcm_bprintf(b, "\n20in40MHz:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr, WL_TX_BW_20IN40, WL_TX_CHAINS_2, &dsss_limits);
			wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
				WL_RATESET_SZ_DSSS);
			ppr_get_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr, WL_TX_BW_20IN40, WL_TX_CHAINS_3, &dsss_limits);
				wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ",
					dsss_limits.pwr, WL_RATESET_SZ_DSSS);
				ppr_get_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr, WL_TX_BW_20IN40, WL_TX_CHAINS_4,
						&dsss_limits);
					wlc_channel_dump_pwr_range(b,  "DSSS_MULTI3       ",
						dsss_limits.pwr, WL_RATESET_SZ_DSSS);
					ppr_get_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

#ifdef WL11AC
	} else if (bw == WL_TX_BW_80) {
		bcm_bprintf(b, "\n80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_STBC,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}
		bcm_bprintf(b, "\n20in80MHz:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_20IN80, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);
		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr, WL_TX_BW_20IN80, WL_TX_CHAINS_2, &dsss_limits);
			wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
				WL_RATESET_SZ_DSSS);
			ppr_get_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr, WL_TX_BW_20IN80, WL_TX_CHAINS_3, &dsss_limits);
				wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ",
					dsss_limits.pwr, WL_RATESET_SZ_DSSS);
				ppr_get_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr, WL_TX_BW_20IN80, WL_TX_CHAINS_4,
						&dsss_limits);
					wlc_channel_dump_pwr_range(b,  "DSSS_MULTI3       ",
						dsss_limits.pwr, WL_RATESET_SZ_DSSS);
					ppr_get_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

		bcm_bprintf(b, "\n40in80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}
#endif /* WL11AC */

#ifdef WL11AC_160
	} else if (bw == WL_TX_BW_160) {
		bcm_bprintf(b, "\n160MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_160, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_160, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_160, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_2, WL_TX_MODE_STBC,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_2, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_3, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_160, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}
		bcm_bprintf(b, "\n20in160MHz:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_20IN160, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN160, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);
		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr, WL_TX_BW_20IN160, WL_TX_CHAINS_2, &dsss_limits);
			wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
				WL_RATESET_SZ_DSSS);
			ppr_get_ofdm(txpwr, WL_TX_BW_20IN160, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr, WL_TX_BW_20IN160, WL_TX_CHAINS_3,
					&dsss_limits);
				wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ",
					dsss_limits.pwr, WL_RATESET_SZ_DSSS);
				ppr_get_ofdm(txpwr, WL_TX_BW_20IN160, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr, WL_TX_BW_20IN160, WL_TX_CHAINS_4,
						&dsss_limits);
					wlc_channel_dump_pwr_range(b,  "DSSS_MULTI3       ",
						dsss_limits.pwr, WL_RATESET_SZ_DSSS);
					ppr_get_ofdm(txpwr, WL_TX_BW_20IN160, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

		bcm_bprintf(b, "\n40in160MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_40IN160, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_40IN160, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40IN160, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_40IN160, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

		bcm_bprintf(b, "\n80in160MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_80IN160, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_80IN160, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_80IN160, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_80IN160, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN160, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}
#endif /* WL11AC_160 */

#ifdef WL11AC_80P80
	} else if (bw == WL_TX_BW_8080) {
		bcm_bprintf(b, "\n80+80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_8080, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_8080, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_8080, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
					&ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_2, WL_TX_MODE_STBC,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_2, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_3, WL_TX_MODE_NONE,
					WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_8080, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}
		bcm_bprintf(b, "\n80+80MHz chan2:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_8080CHAN2, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_8080CHAN2, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);
		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_dsss(txpwr, WL_TX_BW_8080CHAN2, WL_TX_CHAINS_2, &dsss_limits);
			wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
				WL_RATESET_SZ_DSSS);
			ppr_get_ofdm(txpwr, WL_TX_BW_8080CHAN2, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_dsss(txpwr, WL_TX_BW_8080CHAN2, WL_TX_CHAINS_3,
					&dsss_limits);
				wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ",
					dsss_limits.pwr, WL_RATESET_SZ_DSSS);
				ppr_get_ofdm(txpwr, WL_TX_BW_8080CHAN2, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_dsss(txpwr, WL_TX_BW_8080CHAN2, WL_TX_CHAINS_4,
						&dsss_limits);
					wlc_channel_dump_pwr_range(b,  "DSSS_MULTI3       ",
						dsss_limits.pwr, WL_RATESET_SZ_DSSS);
					ppr_get_ofdm(txpwr, WL_TX_BW_8080CHAN2, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_8080CHAN2, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

		bcm_bprintf(b, "\n20in80+80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_20IN8080, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_20IN8080, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_20IN8080, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_20IN8080, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN8080, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

		bcm_bprintf(b, "\n40in80+80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_40IN8080, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_40IN8080, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_40IN8080, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_40IN8080, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN8080, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}

		bcm_bprintf(b, "\n80in80+80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_80IN8080, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ofdm(txpwr, WL_TX_BW_80IN8080, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				&ofdm_limits);
			wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

			ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
			ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, &mcs_limits);
			wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_get_ofdm(txpwr, WL_TX_BW_80IN8080, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3, &ofdm_limits);
				wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ",
					ofdm_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ",
					mcs_limits.pwr);
				ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3, &mcs_limits);
				wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
					mcs_limits.pwr);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_get_ofdm(txpwr, WL_TX_BW_80IN8080, WL_TX_MODE_CDD,
						WL_TX_CHAINS_4, &ofdm_limits);
					wlc_channel_dump_pwr_range8(b, "OFDM_CDD3         ",
						ofdm_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_1,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD3       ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_2,
						WL_TX_MODE_STBC, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP2",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_2,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP2    ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_3,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ",
						mcs_limits.pwr);
					ppr_get_vht_mcs(txpwr, WL_TX_BW_80IN8080, WL_TX_NSS_4,
						WL_TX_MODE_NONE, WL_TX_CHAINS_4, &mcs_limits);
					wlc_channel_dump_pwr_range_mcs(b, "MCS24_31          ",
						mcs_limits.pwr);
				}
			}
		}
#endif /* WL11AC_80P80 */
	}

	bcm_bprintf(b, "\n");
}
#endif // endif

/*
 * 	if (wlc->country_list_extended) all country listable.
 *	else J1 - J10 is excluded.
 */
static bool
wlc_country_listable(struct wlc_info *wlc, const char *countrystr)
{
	bool listable = TRUE;

	if (wlc->country_list_extended == FALSE) {
		if (countrystr[0] == 'J' &&
			(countrystr[1] >= '1' && countrystr[1] <= '9')) {
			listable = FALSE;
		}
	}

	return listable;
}

clm_country_t
wlc_get_country(struct wlc_info *wlc)
{
	return wlc->cmi->cm->country;
}

int
wlc_get_channels_in_country(struct wlc_info *wlc, void *arg)
{
	chanvec_t channels;
	wl_channels_in_country_t *cic = (wl_channels_in_country_t *)arg;
	chanvec_t sup_chan;
	uint count, need, i;

	if ((cic->band != WLC_BAND_5G) && (cic->band != WLC_BAND_2G)) {
		WL_ERROR(("Invalid band %d\n", cic->band));
		return BCME_BADBAND;
	}

	if ((NBANDS(wlc) == 1) && (cic->band != (uint)wlc->band->bandtype)) {
		WL_ERROR(("Invalid band %d for card\n", cic->band));
		return BCME_BADBAND;
	}

	if (wlc_channel_get_chanvec(wlc, cic->country_abbrev, cic->band, &channels) == FALSE) {
		WL_ERROR(("Invalid country %s\n", cic->country_abbrev));
		return BCME_NOTFOUND;
	}

	phy_utils_chanspec_band_validch((phy_info_t *)WLC_PI(wlc), cic->band, &sup_chan);
	for (i = 0; i < sizeof(chanvec_t); i++)
		sup_chan.vec[i] &= channels.vec[i];

	/* find all valid channels */
	for (count = 0, i = 0; i < sizeof(sup_chan.vec)*NBBY; i++) {
		if (isset(sup_chan.vec, i))
			count++;
	}

	need = sizeof(wl_channels_in_country_t) + count*sizeof(cic->channel[0]);

	if (need > cic->buflen) {
		/* too short, need this much */
		WL_ERROR(("WLC_GET_COUNTRY_LIST: Buffer size: Need %d Received %d\n",
			need, cic->buflen));
		cic->buflen = need;
		return BCME_BUFTOOSHORT;
	}

	for (count = 0, i = 0; i < sizeof(sup_chan.vec)*NBBY; i++) {
		if (isset(sup_chan.vec, i))
			cic->channel[count++] = i;
	}

	cic->count = count;
	return 0;
}

int
wlc_get_country_list(struct wlc_info *wlc, void *arg)
{
	chanvec_t channels;
	chanvec_t unused;
	wl_country_list_t *cl = (wl_country_list_t *)arg;
	clm_country_locales_t locale;
	chanvec_t sup_chan;
	uint need, chan_mask_idx, cc_idx;
	clm_country_t country_iter, mapped_country_iter;
	char countrystr[sizeof(ccode_t) + 1] = {0};
	char mapped_ccode[WLC_CNTRY_BUF_SZ];

	ccode_t cc;
	unsigned int regrev, mapped_regrev;

	if (cl->band_set == FALSE) {
		/* get for current band */
		cl->band = wlc->band->bandtype;
	}

	if ((cl->band != WLC_BAND_5G) && (cl->band != WLC_BAND_2G)) {
		WL_ERROR(("Invalid band %d\n", cl->band));
		return BCME_BADBAND;
	}

	if ((NBANDS(wlc) == 1) && (cl->band != (uint)wlc->band->bandtype)) {
		WL_INFORM(("Invalid band %d for card\n", cl->band));
		cl->count = 0;
		return 0;
	}

	phy_utils_chanspec_band_validch((phy_info_t *)WLC_PI(wlc), cl->band, &sup_chan);

	need = sizeof(wl_country_list_t);
	cl->count = 0;
	(void)clm_iter_init(&country_iter);
	while (clm_country_iter(&country_iter, cc, &regrev) == CLM_RESULT_OK) {
		memcpy(countrystr, cc, sizeof(ccode_t));
		if (!wlc_country_listable(wlc, countrystr)) {
			continue;
		}
		/* Checking if current CC already in list. Note that those CC that are not in list
		 * (because not eligible or because buffer too short) may be checked more than once
		 * and counted in 'need' more than once
		 */
		for (cc_idx = 0; cc_idx < cl->count; ++cc_idx) {
			if (!memcmp(&cl->country_abbrev[cc_idx*WLC_CNTRY_BUF_SZ], cc,
				sizeof(ccode_t)))
			{
				break;
			}
		}
		if (cc_idx < cl->count) {
			continue;
		}
		/* Checking if curent CC is supported (can be selected) */
		if (wlc_countrycode_map(wlc->cmi, countrystr, mapped_ccode, &mapped_regrev,
			&mapped_country_iter) != CLM_RESULT_OK)
		{
			continue;
		}
		/* Checking if region, corresponding to current CC, supports required band
		 */
		if ((wlc_get_locale(country_iter, &locale) != CLM_RESULT_OK) ||
			(wlc_locale_get_channels(&locale,
			(cl->band == WLC_BAND_5G) ? CLM_BAND_5G : CLM_BAND_2G, &channels, &unused)
			!= CLM_RESULT_OK))
		{
			continue;
		}
		for (chan_mask_idx = 0; chan_mask_idx < sizeof(sup_chan.vec); chan_mask_idx++) {
			if (sup_chan.vec[chan_mask_idx] & channels.vec[chan_mask_idx]) {
				break;
			}
		}
		if (chan_mask_idx == sizeof(sup_chan.vec)) {
			continue;
		}
		/* CC Passed all checks. If there is space in buffer - put it to buffer */
		need += WLC_CNTRY_BUF_SZ;
		if (need <= cl->buflen) {
			memcpy(&cl->country_abbrev[cl->count*WLC_CNTRY_BUF_SZ], cc,
				sizeof(ccode_t));
			cl->country_abbrev[cl->count*WLC_CNTRY_BUF_SZ + sizeof(ccode_t)] = 0;
			cl->count++;
		}
	}
	if (need > cl->buflen) {
		WL_ERROR(("WLC_GET_COUNTRY_LIST: Buffer size %d is too short, need %d\n",
			cl->buflen, need));
		cl->buflen = need;
		return BCME_BUFTOOSHORT;
	}
	return 0;
}

/* Get regulatory max power for a given channel in a given locale.
 * for external FALSE, it returns limit for brcm hw
 * ---- for 2.4GHz channel, it returns cck limit, not ofdm limit.
 * for external TRUE, it returns 802.11d Country Information Element -
 *	Maximum Transmit Power Level.
 */
int8
wlc_get_reg_max_power_for_channel_ex(wlc_cm_info_t *wlc_cmi, clm_country_locales_t *locales,
	int chan, bool external)
{
	int8 maxpwr = WL_RATE_DISABLED;
	wlc_info_t *wlc = wlc_cmi->wlc;

	if (external) {
		int int_limit;
		if (clm_regulatory_limit(locales, BAND_5G(wlc->band->bandtype) ?
			CLM_BAND_5G : CLM_BAND_2G, chan, &int_limit) == CLM_RESULT_OK) {
			maxpwr = (uint8)int_limit;
		}
	} else {
		clm_power_limits_t limits;
		clm_limits_params_t lim_params;

		if (clm_limits_params_init(&lim_params) == CLM_RESULT_OK) {
			if (clm_limits(locales,
				chan <= CH_MAX_2G_CHANNEL ? CLM_BAND_2G : CLM_BAND_5G,
				chan, 0, CLM_LIMITS_TYPE_CHANNEL, &lim_params,
				&limits) == CLM_RESULT_OK) {
				int i;

				for (i = 0; i < WL_NUMRATES; i++) {
					if (maxpwr < limits.limit[i])
						maxpwr = limits.limit[i];
				}
			}
		}
	}

	return (maxpwr);
}

/* Get regulatory max power for a given channel.
 * Current internal Country Code and current Regulatory Revision are used to lookup the country info
 * structure and from that the country locales structure. The clm functions used for this are
 * processor intensive. Avoid calling this function in a loop, try using
 * wlc_get_reg_max_power_for_channel_ex() instead.
 * for external FALSE, it returns limit for brcm hw
 * ---- for 2.4GHz channel, it returns cck limit, not ofdm limit.
 * for external TRUE, it returns 802.11d Country Information Element -
 *	Maximum Transmit Power Level.
 */
int8
wlc_get_reg_max_power_for_channel(wlc_cm_info_t *wlc_cmi, int chan, bool external)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	clm_country_locales_t locales;
	clm_country_t country = wlc_cm->country;
	clm_result_t result;

	if (country == CLM_ITER_NULL) {
		result = wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country);
		if (result != CLM_RESULT_OK) {
			wlc_cm->country = CLM_ITER_NULL;
			return WL_RATE_DISABLED;
		} else {
			wlc_cm->country = country;
		}
	}

	result = wlc_get_locale(country, &locales);
	if (result != CLM_RESULT_OK) {
		return WL_RATE_DISABLED;
	}

	return wlc_get_reg_max_power_for_channel_ex(wlc_cmi, &locales, chan, external);
}

#if defined(BCMDBG)
static int wlc_dump_max_power_per_channel(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = wlc_cmi->wlc;

	int8 ext_pwr = wlc_get_reg_max_power_for_channel(wlc_cmi,
		wf_chspec_ctlchan(wlc->chanspec), TRUE);
	/* int8 int_pwr = wlc_get_reg_max_power_for_channel(wlc_cm,
	   CHSPEC_CHANNEL(wlc->chanspec), FALSE);
	*/

	/* bcm_bprintf(b, "Reg Max Power: %d External: %d\n", int_pwr, ext_pwr); */
	bcm_bprintf(b, "Reg Max Power (External) %d\n", ext_pwr);
	return 0;
}

static int wlc_get_clm_limits(wlc_cm_info_t *wlc_cmi, clm_band_t bandtype,
	clm_bandwidth_t bw, clm_power_limits_t *limits,
	clm_power_limits_t *limits20in40)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	chanspec_t chanspec = wlc->chanspec;
	unsigned int chan;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	wlcband_t* band;
	int ant_gain;
	clm_limits_params_t lim_params;

	memset(limits, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));
	memset(limits20in40, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));

	country = wlc_cmi->cm->country;
	chan = CHSPEC_CHANNEL(chanspec);
	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	ant_gain = band->antgain;

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK)
		WL_ERROR(("wlc_get_locale failed\n"));

	if ((result == CLM_RESULT_OK) && (clm_limits_params_init(&lim_params) == CLM_RESULT_OK)) {
		lim_params.sar = band->sar;
		lim_params.bw = bw;

		result = clm_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, limits);
	}

	if ((result == CLM_RESULT_OK) && (bw == CLM_BW_40) && (limits20in40 != NULL))
		result = clm_limits(&locale, bandtype, chan, ant_gain,
			CHSPEC_SB_UPPER(chanspec) ?
			CLM_LIMITS_TYPE_SUBCHAN_U : CLM_LIMITS_TYPE_SUBCHAN_L,
			&lim_params, limits20in40);
	if (result != CLM_RESULT_OK)
		WL_ERROR(("clm_limits failed\n"));

	return 0;
}

static int wlc_dump_clm_limits_2G_20M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	clm_power_limits_t *limits;
	clm_power_limits_t *limits20in40;
	int i;

	if ((limits = (clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

	if ((limits20in40 =
		(clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
		return BCME_NOMEM;
	}

	wlc_get_clm_limits(wlc_cmi, CLM_BAND_2G, CLM_BW_20, limits, limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits->limit[i]);
	}

	MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
	MFREE(wlc_cmi->wlc->osh, limits20in40, sizeof(clm_power_limits_t));
	return 0;
}
static int wlc_dump_clm_limits_2G_40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	clm_power_limits_t *limits;
	clm_power_limits_t *limits20in40;
	int i;

	if ((limits = (clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

	if ((limits20in40 =
		(clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
		return BCME_NOMEM;
	}

	wlc_get_clm_limits(wlc_cmi, CLM_BAND_2G, CLM_BW_40, limits, limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits->limit[i]);
	}

	MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
	MFREE(wlc_cmi->wlc->osh, limits20in40, sizeof(clm_power_limits_t));
	return 0;
}
static int wlc_dump_clm_limits_2G_20in40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	clm_power_limits_t *limits;
	clm_power_limits_t *limits20in40;
	int i;

	if ((limits = (clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

	if ((limits20in40 =
		(clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
		return BCME_NOMEM;
	}

	wlc_get_clm_limits(wlc_cmi, CLM_BAND_2G, CLM_BW_40, limits, limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits20in40->limit[i]);
	}

	MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
	MFREE(wlc_cmi->wlc->osh, limits20in40, sizeof(clm_power_limits_t));
	return 0;
}
static int wlc_dump_clm_limits_5G_20M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	clm_power_limits_t *limits;
	clm_power_limits_t *limits20in40;
	int i;

	if ((limits = (clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

	if ((limits20in40 =
		(clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
		return BCME_NOMEM;
	}

	wlc_get_clm_limits(wlc_cmi, CLM_BAND_5G, CLM_BW_20, limits, limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits->limit[i]);
	}

	MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
	MFREE(wlc_cmi->wlc->osh, limits20in40, sizeof(clm_power_limits_t));
	return 0;
}
static int wlc_dump_clm_limits_5G_40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	clm_power_limits_t *limits;
	clm_power_limits_t *limits20in40;
	int i;

	if ((limits = (clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

	if ((limits20in40 =
		(clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
		return BCME_NOMEM;
	}

	wlc_get_clm_limits(wlc_cmi, CLM_BAND_5G, CLM_BW_40, limits, limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits->limit[i]);
	}

	MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
	MFREE(wlc_cmi->wlc->osh, limits20in40, sizeof(clm_power_limits_t));
	return 0;
}
static int wlc_dump_clm_limits_5G_20in40M(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	clm_power_limits_t *limits;
	clm_power_limits_t *limits20in40;
	int i;

	if ((limits = (clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}

	if ((limits20in40 =
		(clm_power_limits_t *)MALLOCZ(wlc_cmi->wlc->osh, sizeof(clm_power_limits_t)))
		== NULL) {
		WL_ERROR(("wl%d: %s: out of memory", wlc_cmi->pub->unit, __FUNCTION__));
		MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
		return BCME_NOMEM;
	}

	wlc_get_clm_limits(wlc_cmi, CLM_BAND_5G, CLM_BW_40, limits, limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits20in40->limit[i]);
	}

	MFREE(wlc_cmi->wlc->osh, limits, sizeof(clm_power_limits_t));
	MFREE(wlc_cmi->wlc->osh, limits20in40, sizeof(clm_power_limits_t));
	return 0;
}
#endif // endif

static bool
wlc_channel_defined_80MHz_channel(uint channel)
{
	int i;
	/* 80MHz channels in 5GHz band */
	static const uint8 defined_5g_80m_chans[] =
	        {42, 58, 106, 122, 138, 155};

	for (i = 0; i < (int)ARRAYSIZE(defined_5g_80m_chans); i++) {
		if (channel == defined_5g_80m_chans[i]) {
			return TRUE;
		}
	}
	return FALSE;
}

static bool
wlc_channel_clm_chanspec_valid(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
		wlc_cm_data_t	*wlc_cm = wlc_cmi->cm;

	if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec))
	{
		return (isset(wlc_cm->allowed_5g_4080channels.vec,
			wf_chspec_primary80_channel(chspec))&&
			isset(wlc_cm->allowed_5g_4080channels.vec,
			wf_chspec_secondary80_channel(chspec)));
	}
	return (CHSPEC_IS2G(chspec) ||
	 (CHSPEC_IS20(chspec) &&
	  isset(wlc_cm->allowed_5g_20channels.vec, CHSPEC_CHANNEL(chspec))) ||
	  isset(wlc_cm->allowed_5g_4080channels.vec, CHSPEC_CHANNEL(chspec)));
}

/*
 * Validate the chanspec for this locale, for 40MHz we need to also check that the sidebands
 * are valid 20MHz channels in this locale and they are also a legal HT combination
 */
static bool
wlc_valid_chanspec_ext(wlc_cm_info_t *wlc_cmi, chanspec_t chspec, bool dualband)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	uint8 channel = CHSPEC_CHANNEL(chspec);
	uint8 cmn_bwcap = WLC_BW_CAP_20MHZ;

	/* AirIQ uses chanspec 7/80, 14/80. Make exception */
#if defined(WL_AIR_IQ)
	if (wlc->scan->state & SCAN_STATE_PROHIBIT) {
		if (CHSPEC_IS2G(chspec) && CHSPEC_IS80(chspec) && ((CHSPEC_CHANNEL(chspec) == 7) ||
			(CHSPEC_CHANNEL(chspec) == 14))) {
			return TRUE;
		}
	}
#endif /* WL_AIR_IQ */

	/* check the chanspec */
	if (wf_chspec_malformed(chspec)) {
		WL_ERROR(("wl%d: malformed chanspec 0x%x\n", wlc->pub->unit, chspec));
		ASSERT(0);
		return FALSE;
	}

	if (RSDB_ENAB(wlc->pub)) {
		/* For dual MAC RSDB chips we will always use common bwcap of all cores to
		 * sanitize chanspec (Note that common bwcap is used only when bandstate is shared
		 * with other WLCs
		 */
		cmn_bwcap = wlc_get_cmn_bwcap(wlc,
				(CHSPEC_IS2G(chspec) ? WLC_BAND_2G: WLC_BAND_5G));
	} else {
		cmn_bwcap = (wlc->pub->phy_bw160_capable) ? WLC_BW_CAP_160MHZ :
			((wlc->pub->phy_bw80_capable) ? WLC_BW_CAP_80MHZ :
			((wlc->pub->phy_bw40_capable) ? WLC_BW_CAP_40MHZ : WLC_BW_CAP_20MHZ));
	}

	/* It can so happen that bandstate->bw_cap is updated in wlc_get_valid_chanspecs()
	 * before invoking this function.
	 * Sanitize cm_bwcap for non-RSDB(or chip that don`t share bandstate) cases based on
	 * PHY BW Capability
	 */
	if (!RSDB_ENAB(wlc->pub) || !RSDB_CMN_BANDSTATE_ENAB(wlc->pub)) {
		if (CHSPEC_IS40(chspec)) {
			if (WL_BW_CAP_40MHZ(cmn_bwcap) && !wlc->pub->phy_bw40_capable) {
				cmn_bwcap &= ~WLC_BW_40MHZ_BIT;
			}
		} else if (CHSPEC_IS80(chspec)) {
			if (WL_BW_CAP_80MHZ(cmn_bwcap) && !wlc->pub->phy_bw80_capable) {
				cmn_bwcap &= ~WLC_BW_80MHZ_BIT;
			}
		} else if (CHSPEC_IS160(chspec)) {
			if (WL_BW_CAP_160MHZ(cmn_bwcap) && !wlc->pub->phy_bw160_capable) {
				cmn_bwcap &= ~WLC_BW_160MHZ_BIT;
			}
		} else if (CHSPEC_IS8080(chspec)) {
			if (WL_BW_CAP_160MHZ(cmn_bwcap) && !wlc->pub->phy_bw8080_capable) {
				cmn_bwcap &= ~WLC_BW_160MHZ_BIT;
			}
		}
	}

	/* check channel range is in band range */
	if (CHANNEL_BANDUNIT(wlc_cmi->wlc, channel) != CHSPEC_WLCBANDUNIT(chspec))
		return FALSE;

	if (CHSPEC_IS5G(chspec) && IS_5G_CH_GRP_DISABLED(wlc, channel)) {
		return FALSE;
	}

	/* Check a 20Mhz channel */
	if (CHSPEC_IS20(chspec)) {
		if (dualband)
			return (VALID_CHANNEL20_DB(wlc_cmi->wlc, channel));
		else
			return (VALID_CHANNEL20(wlc_cmi->wlc, channel));
	} else if (CHSPEC_IS40(chspec)) { /* Check a 40Mhz channel */
		uint8 upper_sideband = 0, idx;
		uint8 num_ch20_entries = sizeof(chan20_info)/sizeof(struct chan20_info);

		if (!WL_BW_CAP_40MHZ(cmn_bwcap)) {
			return FALSE;
		}

		if (!VALID_40CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec)))
			return FALSE;

		if (dualband) {
			if (!VALID_CHANNEL20_DB(wlc, LOWER_20_SB(channel)) ||
			    !VALID_CHANNEL20_DB(wlc, UPPER_20_SB(channel)))
				return FALSE;
		} else {
			if (!VALID_CHANNEL20(wlc, LOWER_20_SB(channel)) ||
			    !VALID_CHANNEL20(wlc, UPPER_20_SB(channel)))
				return FALSE;
		}

		if (!wlc_channel_clm_chanspec_valid(wlc_cmi, chspec))
			return FALSE;

		/* find the lower sideband info in the sideband array */
		for (idx = 0; idx < num_ch20_entries; idx++) {
			if (chan20_info[idx].sb == LOWER_20_SB(channel))
				upper_sideband = chan20_info[idx].adj_sbs;
		}
		/* check that the lower sideband allows an upper sideband */
		if ((upper_sideband & (CH_UPPER_SB | CH_EWA_VALID)) == (CH_UPPER_SB | CH_EWA_VALID))
			return TRUE;
		return FALSE;
	} else if (CHSPEC_IS80(chspec)) { /* Check a 80MHz channel - only 5G band supports 80MHz */
		chanspec_t chspec40;

		/* Only 5G supports 80MHz
		 * Check the chanspec band with BAND_5G() instead of the more straightforward
		 * CHSPEC_IS5G() since BAND_5G() is conditionally compiled on BAND5G support. This
		 * check will turn into a constant check when compiling without BAND5G support.
		 */
		if (!BAND_5G(CHSPEC2WLC_BAND(chspec))) {
			return FALSE;
		}

		/* Make sure that the phy is 80MHz capable and that
		 * we are configured for 80MHz on the band
		 */
		if (!WL_BW_CAP_80MHZ(cmn_bwcap)) {
			return FALSE;
		}

		/* Ensure that vhtmode is enabled if applicable */
		if (!VHT_ENAB_BAND(wlc->pub, WLC_BAND_5G)) {
			return FALSE;
		}

		if (!VALID_80CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec)))
			return FALSE;

		/* Check that the 80MHz center channel is a defined channel */
		if (!wlc_channel_defined_80MHz_channel(channel)) {
			return FALSE;
		}

		if (!wlc_channel_clm_chanspec_valid(wlc_cmi, chspec))
			return FALSE;

		/* Make sure both 40 MHz side channels are valid
		 * Create a chanspec for each 40MHz side side band and check
		 */
		chspec40 = (chanspec_t)((channel - CH_20MHZ_APART) |
		                        WL_CHANSPEC_CTL_SB_L |
		                        WL_CHANSPEC_BW_40 |
		                        WL_CHANSPEC_BAND_5G);
		if (!wlc_valid_chanspec_ext(wlc_cmi, chspec40, dualband)) {
			WL_TMP(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
			        "failed valid check\n",
			        wlc->pub->unit, __FUNCTION__, chspec, chspec40));

			return FALSE;
		}

		chspec40 = (chanspec_t)((channel + CH_20MHZ_APART) |
		                        WL_CHANSPEC_CTL_SB_L |
		                        WL_CHANSPEC_BW_40 |
		                        WL_CHANSPEC_BAND_5G);
		if (!wlc_valid_chanspec_ext(wlc_cmi, chspec40, dualband)) {
			WL_TMP(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
			        "failed valid check\n",
			        wlc->pub->unit, __FUNCTION__, chspec, chspec40));

			return FALSE;
		}

		return TRUE;
	} else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		chanspec_t chspec80;

		/* Only 5G supports 80 +80 MHz
		 * Check the chanspec band with BAND_5G() instead of the more straightforward
		 * CHSPEC_IS5G() since BAND_5G() is conditionally compiled on BAND5G support. This
		 * check will turn into a constant check when compiling without BAND5G support.
		 */
		if (!BAND_5G(CHSPEC2WLC_BAND(chspec))) {
			return FALSE;
		}

		/* Make sure that the phy is 160/80+80 MHz capable and that
		 * we are configured for 160MHz on the band
		 */
		if (!WL_BW_CAP_160MHZ(cmn_bwcap)) {
			return FALSE;
		}

		/* Ensure that vhtmode is enabled if applicable */
		if (!VHT_ENAB_BAND(wlc->pub, WLC_BAND_5G)) {
			return FALSE;
		}

		if (CHSPEC_IS8080(chspec)) {
			if (!VALID_8080CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec)))
				return FALSE;
		}

		if (CHSPEC_IS160(chspec)) {
			if (!VALID_160CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec)))
				return FALSE;
		}

		chspec80 = (chanspec_t)(wf_chspec_primary80_channel(chspec) |
		                        WL_CHANSPEC_CTL_SB_L |
		                        WL_CHANSPEC_BW_80 |
		                        WL_CHANSPEC_BAND_5G);
		if (!wlc_valid_chanspec_ext(wlc_cmi, chspec80, dualband)) {
			WL_TMP(("wl%d: %s: 80 + 80 MHz: chanspec %0X -> chspec80 %0X "
			        "failed valid check\n",
			        wlc->pub->unit, __FUNCTION__, chspec, chspec80));

			return FALSE;
		}

		chspec80 = (chanspec_t)(wf_chspec_secondary80_channel(chspec) |
		                        WL_CHANSPEC_CTL_SB_L |
		                        WL_CHANSPEC_BW_80 |
		                        WL_CHANSPEC_BAND_5G);
		if (!wlc_valid_chanspec_ext(wlc_cmi, chspec80, dualband)) {
			WL_TMP(("wl%d: %s: 80 + 80 MHz: chanspec %0X -> chspec80 %0X "
			        "failed valid check\n",
			        wlc->pub->unit, __FUNCTION__, chspec, chspec80));

			return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}

bool
wlc_valid_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	return wlc_valid_chanspec_ext(wlc_cmi, chspec, FALSE);
}

bool
wlc_valid_chanspec_db(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	return wlc_valid_chanspec_ext(wlc_cmi, chspec, TRUE);
}

/*
 *  Fill in 'list' with validated chanspecs, looping through channels using the chanspec_mask.
 */
static void
wlc_chanspec_list(wlc_info_t *wlc, wl_uint32_list_t *list, chanspec_t chanspec_mask)
{
	uint8 channel;
	chanspec_t chanspec;

	for (channel = 0; channel < MAXCHANNEL; channel++) {
		chanspec = (chanspec_mask | channel);
		if (!wf_chspec_malformed(chanspec) &&
		    ((NBANDS(wlc) > 1) ? wlc_valid_chanspec_db(wlc->cmi, chanspec) :
		     wlc_valid_chanspec(wlc->cmi, chanspec))) {
			list->element[list->count] = chanspec;
			list->count++;
		}
	}
}

/*
 * Returns a list of valid chanspecs meeting the provided settings
 */
void
wlc_get_valid_chanspecs(wlc_cm_info_t *wlc_cmi, wl_uint32_list_t *list, uint bw, bool band2G,
                        const char *abbrev)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	chanspec_t chanspec;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_result_t flag_result = CLM_RESULT_ERR;
	uint16 flags;
	clm_country_locales_t locale;
	chanvec_t saved_valid_channels, saved_db_valid_channels, unused;
#ifdef WL11N
	uint16 saved_locale_flags = 0,  saved_db_locale_flags = 0;
	bool saved_cap_40 = TRUE, saved_db_cap_40 = TRUE;
#endif /* WL11N */
#ifdef WL11AC
	bool saved_cap_80;
	bool saved_cap_160;
#endif /* WL11AC */
	uint8 cmn_bwcap = WLC_BW_CAP_20MHZ;

	/* Check if this is a valid band for this card */
	if ((NBANDS(wlc) == 1) &&
	    (BAND_5G(wlc->band->bandtype) == band2G))
		return;

	if (RSDB_ENAB(wlc->pub)) {
		/* For RSDB chips we will always use common bw cap of all cores to evaluate and
		 * sanitize chanspec
		 * In non RSDB case, wlc->bandstate[BAND_XX_INDEX]->bw_cap is always updated from
		 * pub->phy_bwXX_capable. We will initialize common BW cap to bandstate->bw_cap
		 * in this scenario for any sanitization of chanspec
		 */
		cmn_bwcap = wlc_get_cmn_bwcap(wlc,
				(band2G ? WLC_BAND_2G: WLC_BAND_5G));
	} else {
		cmn_bwcap = (wlc->pub->phy_bw160_capable) ? WLC_BW_CAP_160MHZ :
			((wlc->pub->phy_bw80_capable) ? WLC_BW_CAP_80MHZ :
			((wlc->pub->phy_bw40_capable) ? WLC_BW_CAP_40MHZ : WLC_BW_CAP_20MHZ));
	}

	/* see if we need to look up country. Else, current locale */
	if (strcmp(abbrev, "")) {
		result = wlc_country_lookup(wlc, abbrev, &country);
		if (result != CLM_RESULT_OK) {
			WL_ERROR(("Invalid country \"%s\"\n", abbrev));
			return;
		}
		result = wlc_get_locale(country, &locale);

		flag_result = wlc_get_flags(&locale, band2G ? CLM_BAND_2G : CLM_BAND_5G, &flags);
		BCM_REFERENCE(flag_result);
	}

	/* Save current locales */
	if (result == CLM_RESULT_OK) {
		clm_band_t tmp_band = band2G ? CLM_BAND_2G : CLM_BAND_5G;
		bcopy(&wlc_cm->bandstate[wlc->band->bandunit].valid_channels,
			&saved_valid_channels, sizeof(chanvec_t));
		wlc_locale_get_channels(&locale, tmp_band,
			&wlc_cm->bandstate[wlc->band->bandunit].valid_channels, &unused);
		if (NBANDS(wlc) > 1) {
			bcopy(&wlc_cm->bandstate[OTHERBANDUNIT(wlc)].valid_channels,
			      &saved_db_valid_channels, sizeof(chanvec_t));
			wlc_locale_get_channels(&locale, tmp_band,
			      &wlc_cm->bandstate[OTHERBANDUNIT(wlc)].valid_channels, &unused);
		}
	}

#ifdef WL11N
	if (result == CLM_RESULT_OK) {
		saved_locale_flags = wlc_cm->bandstate[wlc->band->bandunit].locale_flags;
		wlc_cm->bandstate[wlc->band->bandunit].locale_flags = flags;

		if (NBANDS(wlc) > 1) {
			saved_db_locale_flags = wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags;
			wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags = flags;
		}
	}

	/* save 40 MHz cap */
	saved_cap_40 = WL_BW_CAP_40MHZ(wlc->band->bw_cap);
	wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	if (NBANDS(wlc) > 1) {
		saved_db_cap_40 = WL_BW_CAP_40MHZ(wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap);
		wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_40MHZ_BIT;
	}

#ifdef WL11AC
	/* save 80 MHz cap */
	saved_cap_80 = WL_BW_CAP_80MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap);
	wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_80MHZ_BIT;

	saved_cap_160 = WL_BW_CAP_160MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap);
	wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_160MHZ_BIT;

#endif /* WL11AC */

#endif /* WL11N */

	/* Go through 2G 20MHZ chanspecs */
	if (band2G && bw == WL_CHANSPEC_BW_20) {
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_20;
		wlc_chanspec_list(wlc, list, chanspec);
	}

	/* Go through 5G 20 MHZ chanspecs */
	if (!band2G && bw == WL_CHANSPEC_BW_20) {
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_20;
		wlc_chanspec_list(wlc, list, chanspec);
	}

#ifdef WL11N
	/* Go through 2G 40MHZ chanspecs only if N mode and PHY is capable of 40MHZ */
	if (band2G && bw == WL_CHANSPEC_BW_40 &&
		N_ENAB(wlc->pub) && WL_BW_CAP_40MHZ(cmn_bwcap)) {
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_UPPER;
		wlc_chanspec_list(wlc, list, chanspec);
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
		wlc_chanspec_list(wlc, list, chanspec);
	}

	/* Go through 5G 40MHZ chanspecs only if N mode and PHY is capable of 40MHZ  */
	if (!band2G && bw == WL_CHANSPEC_BW_40 &&
		N_ENAB(wlc->pub) && ((NBANDS(wlc) > 1) ||
		IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) &&
		WL_BW_CAP_40MHZ(cmn_bwcap)) {
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_UPPER;
		wlc_chanspec_list(wlc, list, chanspec);
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
		wlc_chanspec_list(wlc, list, chanspec);
	}

#ifdef WL11AC
	/* Go through 5G 80MHZ chanspecs only if VHT mode and PHY is capable of 80MHZ  */
	if (!band2G && bw == WL_CHANSPEC_BW_80 && VHT_ENAB_BAND(wlc->pub, WLC_BAND_5G) &&
		((NBANDS(wlc) > 1) || IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) &&
		WL_BW_CAP_80MHZ(cmn_bwcap)) {
		int i;
		uint16 ctl_sb[] = {
			WL_CHANSPEC_CTL_SB_LL,
			WL_CHANSPEC_CTL_SB_LU,
			WL_CHANSPEC_CTL_SB_UL,
			WL_CHANSPEC_CTL_SB_UU
		};

		for (i = 0; i < 4; i++) {
			chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_80 | ctl_sb[i];
			wlc_chanspec_list(wlc, list, chanspec);
		}
	}

	 /* Go through 5G 8080MHZ chanspecs only if VHT mode and PHY is capable of 8080MHZ  */
	if (!band2G && bw == WL_CHANSPEC_BW_8080 && VHT_ENAB_BAND(wlc->pub, WLC_BAND_5G) &&
		((NBANDS(wlc) > 1) || IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) &&
		WL_BW_CAP_160MHZ(cmn_bwcap)) {
		uint i;
		int j;
		uint16 ctl_sb[] = {
			WL_CHANSPEC_CTL_SB_LLL,
			WL_CHANSPEC_CTL_SB_LLU,
			WL_CHANSPEC_CTL_SB_LUL,
			WL_CHANSPEC_CTL_SB_LUU,
		};

		/* List of all valid channel ID combinations */
		uint8 chan_id[] = {
			0x20,
			0x02,
			0x30,
			0x03,
			0x40,
			0x04,
			0x50,
			0x05,
			0x21,
			0x12,
			0x31,
			0x13,
			0x41,
			0x14,
			0x51,
			0x15,
			0x42,
			0x24,
			0x52,
			0x25,
			0x53,
			0x35,
			0x54,
			0x45
		};

		for (i = 0; i < sizeof(chan_id)/sizeof(uint8); i++) {
			for (j = 0; j < 4; j++) {
				chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_8080
					| ctl_sb[j]| chan_id[i];
				if (!wf_chspec_malformed(chanspec) &&
					wf_chspec_valid(chanspec) &&
					((NBANDS(wlc) > 1) ?
					wlc_valid_chanspec_db(wlc->cmi, chanspec) :
					wlc_valid_chanspec(wlc->cmi, chanspec))) {
					list->element[list->count] = chanspec;
					list->count++;
				}
			}
		}
	}

	/* Go through 5G 160MHZ chanspecs only if VHT mode and PHY is capable of 160MHZ  */
	if (!band2G && bw == WL_CHANSPEC_BW_160 && VHT_ENAB_BAND(wlc->pub, WLC_BAND_5G) &&
		((NBANDS(wlc) > 1) || IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) &&
		WL_BW_CAP_160MHZ(cmn_bwcap)) {
		/* Valid 160 channels */
		uint8 chan[] = {50, 114};
		uint i;
		int j;
		uint16 ctl_sb[] = {
			WL_CHANSPEC_CTL_SB_LLL,
			WL_CHANSPEC_CTL_SB_LLU,
			WL_CHANSPEC_CTL_SB_LUL,
			WL_CHANSPEC_CTL_SB_LUU,
			WL_CHANSPEC_CTL_SB_ULL,
			WL_CHANSPEC_CTL_SB_ULU,
			WL_CHANSPEC_CTL_SB_UUL,
			WL_CHANSPEC_CTL_SB_UUU
		};

		for (i = 0; i < sizeof(chan)/sizeof(uint8); i++) {
			for (j = 0; j < 8; j++) {
				chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_160
					| ctl_sb[j]| chan[i];
				if (!wf_chspec_malformed(chanspec) &&
					((NBANDS(wlc) > 1) ?
					wlc_valid_chanspec_db(wlc->cmi, chanspec) :
					wlc_valid_chanspec(wlc->cmi, chanspec))) {
					list->element[list->count] = chanspec;
					list->count++;
				}
			}
		}
	}

	/* restore 80 MHz cap */
	if (saved_cap_80)
		wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_80MHZ_BIT;
	else
		wlc->bandstate[BAND_5G_INDEX]->bw_cap &= ~WLC_BW_80MHZ_BIT;

	if (saved_cap_160)
		wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_160MHZ_BIT;
	else
		wlc->bandstate[BAND_5G_INDEX]->bw_cap &= ~WLC_BW_160MHZ_BIT;

#endif /* WL11AC */

	/* restore 40 MHz cap */
	if (saved_cap_40)
		wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	else
		wlc->band->bw_cap &= ~WLC_BW_40MHZ_BIT;

	if (NBANDS(wlc) > 1) {
		if (saved_db_cap_40) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_40MHZ_BIT;
		} else {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap &= ~WLC_BW_40MHZ_BIT;
		}
	}

	if (result == CLM_RESULT_OK) {
		wlc_cm->bandstate[wlc->band->bandunit].locale_flags = saved_locale_flags;
		if ((NBANDS(wlc) > 1))
			wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags = saved_db_locale_flags;
	}
#endif /* WL11N */

	/* Restore the locales if switched */
	if (result == CLM_RESULT_OK) {
		bcopy(&saved_valid_channels,
			&wlc_cm->bandstate[wlc->band->bandunit].valid_channels,
			sizeof(chanvec_t));
		if ((NBANDS(wlc) > 1))
			bcopy(&saved_db_valid_channels,
			      &wlc_cm->bandstate[OTHERBANDUNIT(wlc)].valid_channels,
			      sizeof(chanvec_t));
	}
}

/*
 * API identifies passed chanspec present in the passed country.
 * Returns TRUE if chanspec queried to country match else FALSE
 * Caller should ensure to pass valid chanspec and country present in the CLM data
 */
bool
wlc_valid_chanspec_cntry(wlc_cm_info_t *wlc_cm, const char *country_abbrev,
		chanspec_t home_chanspec)
{
	uint i;
	wl_uint32_list_t *list;
	wlc_info_t *wlc = wlc_cm->wlc;
	char abbrev[WLC_CNTRY_BUF_SZ];
	chanspec_t chanspec = home_chanspec;
	bool found_chanspec = FALSE;
	bool ch2g = FALSE;
	uint bw = WL_CHANSPEC_BW_20;

	bzero(abbrev, WLC_CNTRY_BUF_SZ);
	list = (wl_uint32_list_t *)MALLOCZ(wlc->osh, (WL_NUMCHANSPECS+1) * sizeof(uint32));

	if (!list) {
		WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n", wlc->pub->unit,
		          __FUNCTION__, MALLOCED(wlc->osh)));
		return FALSE;
	}

	list->count = 0;

	strncpy(abbrev, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
	abbrev[WLC_CNTRY_BUF_SZ - 1] = '\0';

	/* chanspec is valid during associated case only. */
	if (CHSPEC_IS2G(chanspec) || (chanspec == 0)) {
		ch2g = TRUE;
	} else if (CHSPEC_IS5G(chanspec)) {
		ch2g = FALSE;
	}

	if (CHSPEC_IS20(chanspec) || (chanspec == 0)) {
		bw = WL_CHANSPEC_BW_20;
	} else if (CHSPEC_IS40(chanspec)) {
		bw = WL_CHANSPEC_BW_40;
	} else if (CHSPEC_IS80(chanspec)) {
		bw = WL_CHANSPEC_BW_80;
	}

	wlc_get_valid_chanspecs(wlc->cmi, list, bw, ch2g, abbrev);

	for (i = 0; i < list->count; i++) {
		chanspec_t chanspec_e = (chanspec_t) list->element[i];

		if (chanspec == chanspec_e) {
			found_chanspec = TRUE;
			WL_ERROR(("wl%d: %s: found chanspec 0x%04x\n",
			           wlc->pub->unit, __FUNCTION__, chanspec_e));
			break;
		}
	}

	if (list)
		MFREE(wlc->osh, list, (WL_NUMCHANSPECS+1) * sizeof(uint32));

	return found_chanspec;
}

/* query the channel list given a country and a regulatory class */
/* XXX The result list 'list' must be large enough to hold all possible channels available
 * recommend to allocate MAXCHANNEL worth of elements
 */
uint8
wlc_rclass_get_channel_list(wlc_cm_info_t *cmi, const char *abbrev, uint8 rclass,
	bool bw20, wl_uint32_list_t *list)
{
	const rcinfo_t *rcinfo = NULL;
	uint8 ch2g_start = 0, ch2g_end = 0;
	int i;

	BCM_REFERENCE(cmi);
	BCM_REFERENCE(bw20);

	if (wlc_us_ccode(abbrev)) {
		if (rclass == WLC_REGCLASS_USA_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 11;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_us_20;
#endif // endif
	} else if (wlc_japan_ccode(abbrev)) {
		if (rclass == WLC_REGCLASS_JPN_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 13;
		}
		else if (rclass == WLC_REGCLASS_JPN_2G_20MHZ_CH14) {
			ch2g_start = 14;
			ch2g_end = 14;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_jp_20;
#endif // endif
	} else {
		if (rclass == WLC_REGCLASS_EUR_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 13;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_eu_20;
#endif // endif
	}

	list->count = 0;
	if (rcinfo == NULL) {
		for (i = ch2g_start; i <= ch2g_end; i ++)
			list->element[list->count ++] = i;
	}
	else {
		for (i = 0; i < rcinfo->len; i ++) {
			if (rclass == rcinfo->rctbl[i].rclass)
				list->element[list->count ++] = rcinfo->rctbl[i].chan;
		}
	}

	return (uint8)list->count;
}

/* Returns the chanspec for given country,rcalss and control channel */
chanspec_t
wlc_channel_rclass_get_chanspec(const char *abbrev, uint8 rclass, uint8 channel)
{
	uint bw = 0;

	/* Global/US/EU/JP and China Rclass is same for 80MHz,
	 * 160MHz, and 80p80
	*/
	if (rclass == 128) {
		bw = WL_CHANSPEC_BW_80;
	} else if (rclass == 129) {
		bw = WL_CHANSPEC_BW_160;
	} else if (rclass == 130) {
		bw = WL_CHANSPEC_BW_8080;
	} else if (wlc_us_ccode(abbrev)) {
		/* For Country code US
		 * rclass 22-33 are 40MHz rclass
		 */
		if (rclass >= 22 && rclass <= 33) {
			bw = WL_CHANSPEC_BW_40;
		} else {
			bw = WL_CHANSPEC_BW_20;
		}
	} else if (wlc_japan_ccode(abbrev)) {
		/* For country code japan
		 * rclass 36-57 are 40 MHz rclass
		*/
		if (rclass >= 36 && rclass <= 57) {
			bw = WL_CHANSPEC_BW_40;
		} else {
			bw = WL_CHANSPEC_BW_20;
		}
	} else {
		/* For country code EU
		 * rclass 5-12 are 40MHz rclass
		*/
		if (rclass >= 5 && rclass <= 12) {
			bw = WL_CHANSPEC_BW_40;
		} else {
			bw = WL_CHANSPEC_BW_20;
		}
	}
	return wf_channel2chspec(channel, bw);
}

/* Return true if the channel is a valid channel that is radar sensitive
 * in the current country/locale
 */
bool
wlc_radar_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
#ifdef BAND5G /* RADAR */
	const chanvec_t *radar_channels;
	uint8 channel;

	/* The radar_channels chanvec may be a superset of valid channels,
	 * so be sure to check for a valid channel first.
	 */
	if (!chspec || !wlc_valid_chanspec_db(wlc_cmi, chspec) || !CHSPEC_IS5G(chspec)) {
		return FALSE;
	}

	radar_channels = wlc_cmi->cm->bandstate[BAND_5G_INDEX].radar_channels;

	FOREACH_20_SB(chspec, channel) {
		if (isset(radar_channels->vec, channel)) {
			return TRUE;
		}
	}

#endif	/* BAND5G */
	return FALSE;
}

/* Return true if the channel is a valid channel that is radar sensitive
 * in the current country/locale
 */
bool
wlc_restricted_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	chanvec_t *restricted_channels;
	uint8 channel;

	/* The restriced_channels chanvec may be a superset of valid channels,
	 * so be sure to check for a valid channel first.
	 */

	if (!chspec || !wlc_valid_chanspec_db(wlc_cmi, chspec)) {
		return FALSE;
	}

	restricted_channels = &wlc_cmi->cm->restricted_channels;

	FOREACH_20_SB(chspec, channel) {
		if (isset(restricted_channels->vec, channel)) {
			return TRUE;
		}
	}

	return FALSE;
}

static bool
is_clm_restricted_chanspec(chanspec_t chspec, chanvec_t *restricted_chanvec)
{
	uint8 lower_sb, upper_sb;
	uint8 channel = CHSPEC_CHANNEL(chspec);
	if (CHSPEC_IS20(chspec)) {
		lower_sb = upper_sb = channel;
	} else if (CHSPEC_IS40(chspec)) {
		lower_sb = LOWER_20_SB(channel);
		upper_sb = UPPER_20_SB(channel);
	} else if (CHSPEC_IS80(chspec)) {
		lower_sb = LOWER_20_SB(LOWER_40_SB(channel));
		upper_sb = UPPER_20_SB(UPPER_40_SB(channel));
	} else {
		return FALSE;
	}
	for (; lower_sb <= upper_sb; lower_sb += CH_20MHZ_APART) {
		if (isset(restricted_chanvec->vec, lower_sb)) {
			return TRUE;
		}
	}
	return FALSE;
}

bool
wlc_channel_clm_restricted_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	clm_country_t country;
	clm_country_locales_t locales;
	chanvec_t unused_chanvec, restricted_chanvec;
	if (!chspec || !wlc_valid_chanspec_db(wlc_cm, chspec) ||
		(wlc_country_lookup_direct(wlc_cm->cm->ccode, wlc_cm->cm->regrev, &country) !=
		CLM_RESULT_OK) ||
		(wlc_get_locale(country, &locales) != CLM_RESULT_OK) ||
		(wlc_locale_get_channels(&locales, CHSPEC_IS2G(chspec) ? CLM_BAND_2G : CLM_BAND_5G,
		&unused_chanvec, &restricted_chanvec) != CLM_RESULT_OK))
	{
		return FALSE;
	}
	if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		return is_clm_restricted_chanspec(wf_chspec_primary80_channel(chspec),
			&restricted_chanvec) ||
			is_clm_restricted_chanspec(wf_chspec_secondary80_channel(chspec),
			&restricted_chanvec);
	}
	return is_clm_restricted_chanspec(chspec, &restricted_chanvec);
}

void
wlc_clr_restricted_chanspec(wlc_cm_info_t *wlc_cmi, chanspec_t chspec)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	uint8 channel;

	FOREACH_20_SB(chspec, channel) {
		clrbit(wlc_cm->restricted_channels.vec, channel);
	}
	wlc_upd_restricted_chanspec_flag(wlc_cmi);
}

static void
wlc_upd_restricted_chanspec_flag(wlc_cm_info_t *wlc_cmi)
{
	uint j;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;

	for (j = 0; j < (int)sizeof(chanvec_t); j++)
		if (wlc_cm->restricted_channels.vec[j]) {
			wlc_cm->has_restricted_ch = TRUE;
			return;
		}

	wlc_cm->has_restricted_ch = FALSE;
}

bool
wlc_has_restricted_chanspec(wlc_cm_info_t *wlc_cmi)
{
	return wlc_cmi->cm->has_restricted_ch;
}

#if defined(BCMDBG)

const bcm_bit_desc_t fc_flags[] = {
	{WLC_EIRP, "EIRP"},
	{WLC_DFS_TPC, "DFS/TPC"},
	{WLC_NO_80MHZ, "No 80MHz"},
	{WLC_NO_40MHZ, "No 40MHz"},
	{WLC_NO_MIMO, "No MIMO"},
	{WLC_RADAR_TYPE_EU, "EU_RADAR"},
	{WLC_FILT_WAR, "FILT_WAR"},
	{WLC_TXBF, "TxBF"},
	{0, NULL}
};

/* FTODO need to add 80mhz to this function */
static int
wlc_channel_dump_locale(void *handle, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t*)handle;
	wlc_cm_info_t* wlc_cmi = wlc->cmi;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	ppr_t *txpwr;

	char max_ofdm_str[32];
	char max_ht20_str[32];
	char max_ht40_str[32];
	char max_cck_str[32];
	int chan, i;
	int restricted;
	int radar = 0;
	int max_cck, max_ofdm;
	int max_ht20 = 0, max_ht40 = 0;
	char flagstr[64];
	uint8 rclist[MAXRCLISTSIZE], rclen;
	chanspec_t chanspec;
	int quiet;
	ppr_dsss_rateset_t dsss_limits;
	ppr_ofdm_rateset_t ofdm_limits;
#ifdef WL11N
	ppr_ht_mcs_rateset_t mcs_limits;
#endif // endif

	clm_country_locales_t locales;
	clm_country_t country;

	clm_result_t result = wlc_country_lookup_direct(wlc_cm->ccode,
		wlc_cm->regrev, &country);

	if (result != CLM_RESULT_OK) {
		return -1;
	}

	if ((txpwr = ppr_create(wlc->pub->osh, ppr_get_max_bw())) == NULL) {
		return BCME_ERROR;
	}

	bcm_bprintf(b, "srom_ccode \"%s\" srom_regrev %u\n",
	            wlc_cm->srom_ccode, wlc_cm->srom_regrev);

	result = wlc_get_locale(country, &locales);
	if (result != CLM_RESULT_OK) {
		ppr_delete(wlc->pub->osh, txpwr);
		return -1;
	}

	if (NBANDS(wlc) > 1) {
		uint16 flags;
		wlc_get_flags(&locales, CLM_BAND_2G, &flags);
		bcm_format_flags(fc_flags, flags, flagstr, 64);
		bcm_bprintf(b, "2G Flags: %s\n", flagstr);
		wlc_get_flags(&locales, CLM_BAND_5G, &flags);
		bcm_format_flags(fc_flags, flags, flagstr, 64);
		bcm_bprintf(b, "5G Flags: %s\n", flagstr);
	} else {
		uint16 flags;
		if (BAND_2G(wlc->band->bandtype))
			wlc_get_flags(&locales, CLM_BAND_2G, &flags);
		else
			wlc_get_flags(&locales, CLM_BAND_5G, &flags);
		bcm_format_flags(fc_flags, flags, flagstr, 64);
		bcm_bprintf(b, "%dG Flags: %s\n", BAND_2G(wlc->band->bandtype)?2:5, flagstr);
	}

	if (N_ENAB(wlc->pub))
		bcm_bprintf(b, "  Ch Rdr/reS DSSS  OFDM   HT    20/40\n");
	else
		bcm_bprintf(b, "  Ch Rdr/reS DSSS  OFDM\n");

	for (chan = 0; chan < MAXCHANNEL; chan++) {
		chanspec = CH20MHZ_CHSPEC(chan);
		if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
			chanspec = CH40MHZ_CHSPEC(chan, WL_CHANSPEC_CTL_SB_LOWER);
			if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
				chanspec = CH80MHZ_CHSPEC(chan, WL_CHANSPEC_CTL_SB_LOWER);
				if (!wlc_valid_chanspec_db(wlc->cmi, chanspec))
					continue;
			}
		}

		radar = wlc_radar_chanspec(wlc->cmi, chanspec);
		restricted = wlc_restricted_chanspec(wlc->cmi, chanspec);
		quiet = wlc_quiet_chanspec(wlc->cmi, chanspec);

		wlc_channel_reg_limits(wlc->cmi, chanspec, txpwr, NULL);

		ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_limits);
		max_cck = dsss_limits.pwr[0];

		ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		max_ofdm = ofdm_limits.pwr[0];

#if defined(WL11N)
		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_get_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
					WL_TX_CHAINS_2, &mcs_limits);
			max_ht20 = mcs_limits.pwr[0];

			ppr_get_ht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
			max_ht40 = mcs_limits.pwr[0];
		}
#endif /* WL11N */

		if (CHSPEC_IS2G(chanspec))
			if (max_cck != WL_RATE_DISABLED)
			snprintf(max_cck_str, sizeof(max_cck_str),
			         "%2d%s", QDB_FRAC(max_cck));
			else
				strncpy(max_cck_str, "-    ", sizeof(max_cck_str));

		else
			strncpy(max_cck_str, "     ", sizeof(max_cck_str));

		if (max_ofdm != WL_RATE_DISABLED)
			snprintf(max_ofdm_str, sizeof(max_ofdm_str),
		         "%2d%s", QDB_FRAC(max_ofdm));
		else
			strncpy(max_ofdm_str, "-    ", sizeof(max_ofdm_str));

		if (N_ENAB(wlc->pub)) {

			if (max_ht20 != WL_RATE_DISABLED)
				snprintf(max_ht20_str, sizeof(max_ht20_str),
			         "%2d%s", QDB_FRAC(max_ht20));
			else
				strncpy(max_ht20_str, "-    ", sizeof(max_ht20_str));

			if (max_ht40 != WL_RATE_DISABLED)
				snprintf(max_ht40_str, sizeof(max_ht40_str),
			         "%2d%s", QDB_FRAC(max_ht40));
			else
				strncpy(max_ht40_str, "-    ", sizeof(max_ht40_str));

			bcm_bprintf(b, "%s%3d %s%s%s     %s %s  %s %s\n",
			            (CHSPEC_IS40(chanspec)?">":" "), chan,
			            (radar ? "R" : "-"), (restricted ? "S" : "-"),
			            (quiet ? "Q" : "-"),
			            max_cck_str, max_ofdm_str,
			            max_ht20_str, max_ht40_str);
		}
		else
			bcm_bprintf(b, "%s%3d %s%s%s     %s %s\n",
			            (CHSPEC_IS40(chanspec)?">":" "), chan,
			            (radar ? "R" : "-"), (restricted ? "S" : "-"),
			            (quiet ? "Q" : "-"),
			            max_cck_str, max_ofdm_str);
	}

	bzero(rclist, MAXRCLISTSIZE);
	chanspec = wlc->pub->associated ?
	        wlc->home_chanspec : WLC_BAND_PI_RADIO_CHANSPEC;
	rclen = wlc_get_regclass_list(wlc->cmi, rclist, MAXRCLISTSIZE, chanspec, FALSE);
	if (rclen) {
		bcm_bprintf(b, "supported regulatory class:\n");
		for (i = 0; i < rclen; i++)
			bcm_bprintf(b, "%d ", rclist[i]);
		bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "has_restricted_ch %s\n", wlc_cm->has_restricted_ch ? "TRUE" : "FALSE");

	ppr_delete(wlc->pub->osh, txpwr);
	return 0;
}
#endif // endif

static void
wlc_channel_margin_summary_mapfn(void *context, uint8 *a, uint8 *b)
{
	uint8 margin;
	uint8 *pmin = (uint8*)context;

	if (*a > *b)
		margin = *a - *b;
	else
		margin = 0;

	*pmin = MIN(*pmin, margin);
}

/* Map the given function with its context value over the power targets
 * appropriate for the given band and bandwidth in two txppr structs.
 * If the band is 2G, DSSS/CCK rates will be included.
 * If the bandwidth is 20MHz, only 20MHz targets are included.
 * If the bandwidth is 40MHz, both 40MHz and 20in40 targets are included.
 */
static void
wlc_channel_map_txppr_binary(ppr_mapfn_t fn, void* context, uint bandtype, uint bw,
	ppr_t *a, ppr_t *b, wlc_info_t *wlc)
{
	if (bw == WL_CHANSPEC_BW_20) {
		if (bandtype == WL_CHANSPEC_BAND_2G)
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20, WL_TX_CHAINS_1);
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1);
	}

#ifdef WL11N
	/* map over 20MHz rates for 20MHz channels */
	if (bw == WL_CHANSPEC_BW_20) {
		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			if (bandtype == WL_CHANSPEC_BAND_2G) {
				ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20, WL_TX_CHAINS_2);
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					ppr_map_vec_dsss(fn, context, a, b,
						WL_TX_BW_20, WL_TX_CHAINS_3);
				}
			}
		}
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_1,
			WL_TX_MODE_NONE, WL_TX_CHAINS_1);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_1,
				WL_TX_MODE_CDD, WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2,
				WL_TX_MODE_STBC, WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2,
				WL_TX_MODE_NONE, WL_TX_CHAINS_2);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3);
				ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3);

				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3);
				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3);

				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20,
						WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_4);
					ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20,
						WL_TX_NSS_2, WL_TX_MODE_STBC, WL_TX_CHAINS_4);
					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20,
						WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20,
						WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20,
						WL_TX_NSS_4, WL_TX_MODE_NONE, WL_TX_CHAINS_4);
				}
			}
		}
	} else
	/* map over 40MHz and 20in40 rates for 40MHz channels */
	{
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_40, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_1,
				WL_TX_MODE_CDD, WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2,
				WL_TX_MODE_STBC, WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2,
				WL_TX_MODE_NONE, WL_TX_CHAINS_2);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_40, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3);
				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3);

				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3);
				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3);

				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_40,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4);
					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40,
						WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40,
						WL_TX_NSS_2, WL_TX_MODE_STBC, WL_TX_CHAINS_4);
					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40,
						WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40,
						WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40,
						WL_TX_NSS_4, WL_TX_MODE_NONE, WL_TX_CHAINS_4);
				}
			}
		}
		/* 20in40 legacy */
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_CHAINS_1);
			if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
				ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20IN40,
					WL_TX_CHAINS_2);
				if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
					ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20IN40,
						WL_TX_CHAINS_3);
					if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
						ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20IN40,
							WL_TX_CHAINS_4);
					}
				}
			}
		}

		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1);

		/* 20in40 HT */
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1);

		if (PHYCORENUM(wlc->stf->op_txstreams) > 1) {
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_1,
				WL_TX_MODE_CDD,	WL_TX_CHAINS_2);
			ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2,
				WL_TX_MODE_STBC, WL_TX_CHAINS_2);
			ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2,
				WL_TX_MODE_NONE, WL_TX_CHAINS_2);

			if (PHYCORENUM(wlc->stf->op_txstreams) > 2) {
				ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_MODE_CDD,
					WL_TX_CHAINS_3);
				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_1,
					WL_TX_MODE_CDD, WL_TX_CHAINS_3);

				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2,
					WL_TX_MODE_STBC, WL_TX_CHAINS_3);
				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3);
				ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_3,
					WL_TX_MODE_NONE, WL_TX_CHAINS_3);

				if (PHYCORENUM(wlc->stf->op_txstreams) > 3) {
					ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20IN40,
						WL_TX_MODE_CDD, WL_TX_CHAINS_4);
					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40,
						WL_TX_NSS_1, WL_TX_MODE_CDD, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40,
						WL_TX_NSS_2, WL_TX_MODE_STBC, WL_TX_CHAINS_4);
					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40,
						WL_TX_NSS_2, WL_TX_MODE_NONE, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40,
						WL_TX_NSS_3, WL_TX_MODE_NONE, WL_TX_CHAINS_4);

					ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40,
						WL_TX_NSS_4, WL_TX_MODE_NONE, WL_TX_CHAINS_4);
				}
			}
		}
	}
#endif /* WL11N */
}

/* calculate the offset from each per-rate power target in txpwr to the supplied
 * limit (or zero if txpwr[x] is less than limit[x]), and return the smallest
 * offset of relevant rates for bandtype/bw.
 */
static uint8
wlc_channel_txpwr_margin(ppr_t *txpwr, ppr_t *limit, uint bandtype, uint bw, wlc_info_t *wlc)
{
	uint8 margin = 0xff;

	wlc_channel_map_txppr_binary(wlc_channel_margin_summary_mapfn, &margin,
	                             bandtype, bw, txpwr, limit, wlc);
	return margin;
}

/* return a ppr_t struct with the phy srom limits for the given channel */
static void
wlc_channel_srom_limits(wlc_cm_info_t *wlc_cmi, chanspec_t chanspec,
	ppr_t *srommin, ppr_t *srommax)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_phy_t *pi = WLC_PI(wlc);
	uint8 min_srom;

	if (srommin != NULL)
		ppr_clear(srommin);
	if (srommax != NULL)
		ppr_clear(srommax);

	if (!PHYTYPE_HT_CAP(wlc_cmi->wlc->band))
		return;

	wlc_phy_txpower_sromlimit(pi, chanspec, &min_srom, srommax, 0);
	if (srommin != NULL)
		ppr_set_cmn_val(srommin, min_srom);
}

/* Set a per-chain power limit for the given band
 * Per-chain offsets will be used to make sure the max target power does not exceed
 * the per-chain power limit
 */
int
wlc_channel_band_chain_limit(wlc_cm_info_t *wlc_cmi, uint bandtype,
                             struct wlc_channel_txchain_limits *lim)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	int ppr_buf_i;
	ppr_t* txpwr = NULL;
	int bandunit = (bandtype == WLC_BAND_2G) ? BAND_2G_INDEX : BAND_5G_INDEX;

	if (!PHYTYPE_HT_CAP(wlc_cmi->wlc->band))
		return BCME_UNSUPPORTED;

	wlc_cmi->cm->bandstate[bandunit].chain_limits = *lim;

	if (CHSPEC_WLCBANDUNIT(wlc->chanspec) != bandunit)
		return 0;

#if defined(WLTXPWR_CACHE) && defined(WL11N)
		wlc_phy_txpwr_cache_invalidate(phy_tpc_get_txpwr_cache(WLC_PI(wlc)));
#endif // endif

	ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc_cmi, &txpwr,
		PPR_CHSPEC_BW(wlc->chanspec));
	if (ppr_buf_i < 0)
		return BCME_NOMEM;

	/* update the current tx chain offset if we just updated this band's limits */
	wlc_channel_txpower_limits(wlc_cmi, txpwr);
	wlc_channel_update_txchain_offsets(wlc_cmi, txpwr);

	wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);

	return 0;
}

/* update the per-chain tx power offset given the current power targets to implement
 * the correct per-chain tx power limit
 */
static int
wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cmi, ppr_t *txpwr)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	struct wlc_channel_txchain_limits *lim;
	wl_txchain_pwr_offsets_t offsets;
	chanspec_t chanspec;
	int i, err;
	int max_pwr;
	int band, bw;
	int limits_present = FALSE;
	uint8 delta, margin, err_margin;
	wl_txchain_pwr_offsets_t cur_offsets;
#ifdef BCMDBG
	char chanbuf[CHANSPEC_STR_LEN];
#endif // endif
#if defined WLTXPWR_CACHE && defined(WL11N)
	tx_pwr_cache_entry_t* cacheptr = phy_tpc_get_txpwr_cache(WLC_PI(wlc));
#endif // endif
	int ppr_buf_i;
	ppr_t* srompwr = NULL;

	if (!PHYTYPE_HT_CAP(wlc->band)) {
		return BCME_UNSUPPORTED;
	}

	chanspec = wlc->chanspec;

#if defined WLTXPWR_CACHE && defined(WL11N)
	if ((wlc_phy_txpwr_cache_is_cached(cacheptr, chanspec) == TRUE) &&
		(wlc_phy_get_cached_txchain_offsets(cacheptr, chanspec, 0) != WL_RATE_DISABLED)) {

		for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
			offsets.offset[i] =
				wlc_phy_get_cached_txchain_offsets(cacheptr, chanspec, i);
		}

	/* always set, at least for the moment */
		err = wlc_stf_txchain_pwr_offset_set(wlc, &offsets);

		return err;
	}
#endif /* WLTXPWR_CACHE && WL11N */

	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);

	ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc_cmi, &srompwr,
		PPR_CHSPEC_BW(chanspec));
	if (ppr_buf_i < 0)
		return BCME_NOMEM;
	/* initialize the offsets to a default of no offset */
	memset(&offsets, 0, sizeof(wl_txchain_pwr_offsets_t));

	lim = &wlc_cmi->cm->bandstate[CHSPEC_WLCBANDUNIT(chanspec)].chain_limits;

	/* see if there are any chain limits specified */
	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
		if (lim->chain_limit[i] < WLC_TXPWR_MAX) {
			limits_present = TRUE;
			break;
		}
	}

	/* if there are no limits, we do not need to do any calculations */
	if (limits_present) {

#ifdef WLTXPWR_CACHE
		ASSERT(txpwr != NULL);
#endif // endif
		/* find the max power target for this channel and impose
		 * a txpwr delta per chain to meet the specified chain limits
		 * Bound the delta by the tx power margin
		 */

		/* get the srom min powers */
		wlc_channel_srom_limits(wlc_cmi, wlc->chanspec, srompwr, NULL);

		/* find the dB margin we can use to adjust tx power */
		margin = wlc_channel_txpwr_margin(txpwr, srompwr, band, bw, wlc);

		/* reduce the margin by the error margin 1.5dB backoff */
		err_margin = 6;	/* 1.5 dB in qdBm */
		margin = (margin >= err_margin) ? margin - err_margin : 0;

		/* get the srom max powers */
		wlc_channel_srom_limits(wlc_cmi, wlc->chanspec, NULL, srompwr);

		/* combine the srom limits with the given regulatory limits
		 * to find the actual channel max
		 */
		/* wlc_channel_txpwr_vec_combine_min(srompwr, txpwr); */
		ppr_apply_vector_ceiling(srompwr, txpwr);

		/* max_pwr = (int)wlc_channel_txpwr_max(srompwr, band, bw, wlc); */
		max_pwr = (int)ppr_get_max_for_bw(srompwr, PPR_CHSPEC_BW(chanspec));
		WL_NONE(("wl%d: %s: channel %s max_pwr %d margin %d\n",
		         wlc->pub->unit, __FUNCTION__,
		         wf_chspec_ntoa(wlc->chanspec, chanbuf), max_pwr, margin));

		/* for each chain, calculate an offset that keeps the max tx power target
		 * no greater than the chain limit
		 */
		for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
			WL_NONE(("wl%d: %s: chain_limit[%d] %d",
			         wlc->pub->unit, __FUNCTION__,
			         i, lim->chain_limit[i]));
			if (lim->chain_limit[i] < max_pwr) {
				delta = max_pwr - lim->chain_limit[i];

				WL_NONE((" desired delta -%u lim delta -%u",
				         delta, MIN(delta, margin)));

				/* limit to the margin allowed for our adjustmets */
				delta = MIN(delta, margin);

				offsets.offset[i] = -delta;
			}
			WL_NONE(("\n"));
		}
	} else {
		WL_NONE(("wl%d: %s skipping limit calculation since limits are MAX\n",
		         wlc->pub->unit, __FUNCTION__));
	}

#if defined WLTXPWR_CACHE && defined(WL11N)
	if (wlc_phy_txpwr_cache_is_cached(cacheptr, chanspec) == TRUE) {
		for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
			wlc_phy_set_cached_txchain_offsets(cacheptr, chanspec, i,
				offsets.offset[i]);
		}
	}
#endif // endif
	err = wlc_iovar_op(wlc, "txchain_pwr_offset", NULL, 0,
	                   &cur_offsets, sizeof(wl_txchain_pwr_offsets_t), IOV_GET, NULL);

	if (!err && bcmp(&cur_offsets.offset, &offsets.offset, WL_NUM_TXCHAIN_MAX)) {

		err = wlc_iovar_op(wlc, "txchain_pwr_offset", NULL, 0,
			&offsets, sizeof(wl_txchain_pwr_offsets_t), IOV_SET, NULL);
	}

	if (err) {
		WL_ERROR(("wl%d: txchain_pwr_offset failed: error %d\n",
			WLCWLUNIT(wlc), err));
	}

	wlc_channel_release_prealloc_ppr_buf(wlc_cmi, ppr_buf_i);

	return err;
}

#if defined(BCMDBG)
static int
wlc_channel_dump_reg_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cmi = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cmi->wlc;
	ppr_t* txpwr;
	char chanbuf[CHANSPEC_STR_LEN];
	int ant_gain;
	int sar;

	wlcband_t* band = wlc->band;

	ant_gain = band->antgain;
	sar = band->sar;

	if ((txpwr = ppr_create(wlc_cmi->pub->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
		return BCME_ERROR;
	}
	wlc_channel_reg_limits(wlc_cmi, wlc->chanspec, txpwr, NULL);

	bcm_bprintf(b, "Regulatory Limits for channel %s (SAR:",
		wf_chspec_ntoa(wlc->chanspec, chanbuf));
	if (sar == WLC_TXPWR_MAX)
		bcm_bprintf(b, " -  ");
	else
		bcm_bprintf(b, "%2d%s", QDB_FRAC_TRUNC(sar));
	bcm_bprintf(b, " AntGain: %2d%s)\n", QDB_FRAC_TRUNC(ant_gain));
	wlc_channel_dump_txppr(b, txpwr, CHSPEC_TO_TX_BW(wlc->chanspec), wlc);

	ppr_delete(wlc_cmi->pub->osh, txpwr);
	return 0;
}

/* dump of regulatory power with local constraint factored in for the current channel */
static int
wlc_channel_dump_reg_local_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cmi = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cmi->wlc;
	ppr_t* txpwr;
	char chanbuf[CHANSPEC_STR_LEN];

	if ((txpwr = ppr_create(wlc_cmi->pub->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
		return BCME_ERROR;
	}

	wlc_channel_txpower_limits(wlc_cmi, txpwr);

	bcm_bprintf(b, "Regulatory Limits with constraint for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, txpwr, CHSPEC_TO_TX_BW(wlc->chanspec), wlc);

	ppr_delete(wlc_cmi->pub->osh, txpwr);
	return 0;
}

/* dump of srom per-rate max/min values for the current channel */
static int
wlc_channel_dump_srom_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cmi = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cmi->wlc;
	ppr_t* srompwr;
	char chanbuf[CHANSPEC_STR_LEN];

	if ((srompwr = ppr_create(wlc_cmi->pub->osh, ppr_get_max_bw())) == NULL) {
		return BCME_ERROR;
	}

	wlc_channel_srom_limits(wlc_cmi, wlc->chanspec, NULL, srompwr);

	bcm_bprintf(b, "PHY/SROM Max Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, srompwr, CHSPEC_TO_TX_BW(wlc->chanspec), wlc);

	wlc_channel_srom_limits(wlc_cmi, wlc->chanspec, srompwr, NULL);

	bcm_bprintf(b, "PHY/SROM Min Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, srompwr, CHSPEC_TO_TX_BW(wlc->chanspec), wlc);

	ppr_delete(wlc_cmi->pub->osh, srompwr);
	return 0;
}

static void
wlc_channel_margin_calc_mapfn(void *ignore, uint8 *a, uint8 *b)
{
	BCM_REFERENCE(ignore);

	if (*a > *b)
		*a = *a - *b;
	else
		*a = 0;
}

/* dumps dB margin between a rate an the lowest allowable power target, and
 * summarize the min of the margins for the current channel
 */
static int
wlc_channel_dump_margin(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cmi = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cmi->wlc;
	ppr_t* txpwr;
	ppr_t* srommin;
	chanspec_t chanspec;
	int band, bw;
	uint8 margin;
	char chanbuf[CHANSPEC_STR_LEN];

	chanspec = wlc->chanspec;
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);

	if ((txpwr = ppr_create(wlc_cmi->pub->osh, PPR_CHSPEC_BW(chanspec))) == NULL) {
		return 0;
	}
	if ((srommin = ppr_create(wlc_cmi->pub->osh, PPR_CHSPEC_BW(chanspec))) == NULL) {
		ppr_delete(wlc_cmi->pub->osh, txpwr);
		return 0;
	}

	wlc_channel_txpower_limits(wlc_cmi, txpwr);

	/* get the srom min powers */
	wlc_channel_srom_limits(wlc_cmi, wlc->chanspec, srommin, NULL);

	/* find the dB margin we can use to adjust tx power */
	margin = wlc_channel_txpwr_margin(txpwr, srommin, band, bw, wlc);

	/* calulate the per-rate margins */
	wlc_channel_map_txppr_binary(wlc_channel_margin_calc_mapfn, NULL,
	                             band, bw, txpwr, srommin, wlc);

	bcm_bprintf(b, "Power margin for channel %s, min = %u\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf), margin);
	wlc_channel_dump_txppr(b, txpwr, CHSPEC_TO_TX_BW(wlc->chanspec), wlc);

	ppr_delete(wlc_cmi->pub->osh, srommin);
	ppr_delete(wlc_cmi->pub->osh, txpwr);
	return 0;
}

static int
wlc_channel_supported_country_regrevs(void *handle, struct bcmstrbuf *b)
{
	int iter;
	ccode_t cc;
	unsigned int regrev;

	BCM_REFERENCE(handle);

	if (clm_iter_init(&iter) == CLM_RESULT_OK) {
		while (clm_country_iter((clm_country_t*)&iter, cc, &regrev) == CLM_RESULT_OK) {
			bcm_bprintf(b, "%c%c/%u\n", cc[0], cc[1], regrev);

		}
	}
	return 0;
}

#endif // endif

void
wlc_dump_clmver(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	const struct clm_data_header* clm_base_headerptr = wlc_cm->clm_base_dataptr;
	const char* verstrptr;
	const char* useridstrptr;

	if (clm_base_headerptr == NULL)
		clm_base_headerptr = &clm_header;

	bcm_bprintf(b, "API: %d.%d\nData: %s\nCompiler: %s\n%s\n",
		clm_base_headerptr->format_major, clm_base_headerptr->format_minor,
		clm_base_headerptr->clm_version, clm_base_headerptr->compiler_version,
		clm_base_headerptr->generator_version);
	verstrptr = clm_get_base_app_version_string();
	if (verstrptr != NULL)
		bcm_bprintf(b, "Customization: %s\n", verstrptr);
	useridstrptr = clm_get_string(CLM_STRING_TYPE_USER_STRING, CLM_STRING_SOURCE_BASE);
	if (useridstrptr != NULL)
		bcm_bprintf(b, "Creation: %s\n", useridstrptr);
}

void wlc_channel_update_txpwr_limit(wlc_info_t *wlc)
{
	int ppr_buf_i, ppr_ru_buf_i;
	ppr_t *txpwr = NULL;
	ppr_ru_t *ru_txpwr = NULL;

	ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc->cmi, &txpwr,
		PPR_CHSPEC_BW(wlc->chanspec));
	if (ppr_buf_i < 0)
		return;
	ppr_ru_buf_i = wlc_channel_acquire_ppr_ru_from_prealloc_buf(wlc->cmi, &ru_txpwr);
	if (ppr_ru_buf_i < 0) {
		wlc_channel_release_prealloc_ppr_buf(wlc->cmi, ppr_buf_i);
		return;
	}

	wlc_channel_reg_limits(wlc->cmi, wlc->chanspec, txpwr, NULL);
#ifdef WL11AX
	wlc_phy_set_ru_power_limits(WLC_PI(wlc), ru_txpwr);
#endif /* WL11AX */
	wlc_phy_txpower_limit_set(WLC_PI(wlc), txpwr, wlc->chanspec);

	wlc_channel_release_prealloc_ppr_buf(wlc->cmi, ppr_buf_i);
	wlc_channel_release_prealloc_ppr_ru_buf(wlc->cmi, ppr_ru_buf_i);
}

#ifdef WLTDLS
/* Regulatory Class IE in TDLS Setup frames */
static uint
wlc_channel_tdls_calc_rc_ie_len(void *ctx, wlc_iem_calc_data_t *calc)
{
	wlc_cm_info_t *cmi = (wlc_cm_info_t *)ctx;
	wlc_iem_ft_cbparm_t *ftcbparm = calc->cbparm->ft;
	chanspec_t chanspec = ftcbparm->tdls.chspec;
	uint8 rclen;		/* regulatory class length */
	uint8 rclist[32];	/* regulatory class list */

	if (!isset(ftcbparm->tdls.cap, DOT11_TDLS_CAP_CH_SW)) {
		return 0;
	}

	/* XXX need to resolve if AP & STA req to support
	 * regulatory class IE in beacon/probe so that
	 * associated STA in a BSS able to make a join
	 * decision
	 */
	rclen = wlc_get_regclass_list(cmi, rclist, MAXRCLISTSIZE, chanspec, TRUE);

	return TLV_HDR_LEN + rclen;
}

static int
wlc_channel_tdls_write_rc_ie(void *ctx, wlc_iem_build_data_t *build)
{
	wlc_cm_info_t *cmi = (wlc_cm_info_t *)ctx;
	wlc_iem_ft_cbparm_t *ftcbparm = build->cbparm->ft;
	chanspec_t chanspec = ftcbparm->tdls.chspec;
	uint8 rclen;		/* regulatory class length */
	uint8 rclist[32];	/* regulatory class list */

	if (!isset(ftcbparm->tdls.cap, DOT11_TDLS_CAP_CH_SW)) {
		return BCME_OK;
	}

	/* XXX need to resolve if AP & STA req to support
	 * regulatory class IE in beacon/probe so that
	 * associated STA in a BSS able to make a join
	 * decision
	 */
	rclen = wlc_get_regclass_list(cmi, rclist, MAXRCLISTSIZE, chanspec, TRUE);

	bcm_write_tlv_safe(DOT11_MNG_REGCLASS_ID, rclist, rclen,
		build->buf, build->buf_len);

	return BCME_OK;
}
#endif /* WLTDLS */

void
wlc_channel_tx_power_target_min_max(struct wlc_info *wlc,
	chanspec_t chanspec, int *min_pwr, int *max_pwr)
{
	int cur_min = 0xFFFF, cur_max = 0;
	wlc_phy_t *pi = WLC_PI(wlc);
	int txpwr_ppr_buf_i, srommax_ppr_buf_i;
	ppr_t *txpwr = NULL;
	ppr_t *srommax = NULL;
	int8 min_srom;
	chanspec_t channel = wf_chspec_ctlchan(chanspec) |
		CHSPEC_BAND(chanspec) | WL_CHANSPEC_BW_20;

	txpwr_ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc->cmi, &txpwr,
		PPR_CHSPEC_BW(channel));
	if (txpwr_ppr_buf_i < 0)
		return;

	srommax_ppr_buf_i = wlc_channel_acquire_ppr_from_prealloc_buf(wlc->cmi, &srommax,
		PPR_CHSPEC_BW(channel));
	if (srommax_ppr_buf_i < 0) {
		wlc_channel_release_prealloc_ppr_buf(wlc->cmi, txpwr_ppr_buf_i);
		return;
	}
	/* use the control channel to get the regulatory limits and srom max/min */
	wlc_channel_reg_limits(wlc->cmi, channel, txpwr, NULL);

	wlc_phy_txpower_sromlimit(pi, channel, (uint8*)&min_srom, srommax, 0);

	/* bound the regulatory limit by srom min/max */
	ppr_apply_vector_ceiling(txpwr, srommax);
	ppr_apply_min(txpwr, min_srom);

	WL_NONE(("min_srom %d\n", min_srom));

	cur_min = ppr_get_min(txpwr, min_srom);
	cur_max = ppr_get_max(txpwr);

	*min_pwr = (int)cur_min;
	*max_pwr = (int)cur_max;

	wlc_channel_release_prealloc_ppr_buf(wlc->cmi, txpwr_ppr_buf_i);
	wlc_channel_release_prealloc_ppr_buf(wlc->cmi, srommax_ppr_buf_i);
}

int8
wlc_channel_max_tx_power_for_band(struct wlc_info *wlc, int band, int8 *min)
{
	int chan;
	chanspec_t chspec;
	int chspec_band;
	int8 max = 0, tmpmin = 0x7F;
	int chan_min;
	int chan_max = 0;
	wlc_cm_info_t *cmi = wlc->cmi;

	chspec_band = (band == WLC_BAND_2G) ? WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G;

	for (chan = 0; chan < MAXCHANNEL; chan++) {
		chspec = chan | WL_CHANSPEC_BW_20 | chspec_band;

		if (!wlc_valid_chanspec_db(cmi, chspec))
			continue;

		wlc_channel_tx_power_target_min_max(wlc, chspec, &chan_min, &chan_max);

		max = MAX(chan_max, max);
		tmpmin = MIN(chan_min, tmpmin);
	}
	*min = tmpmin;
	return max;
}

void
wlc_channel_set_tx_power(struct wlc_info *wlc,
	int band, int num_chains, int *txcpd_power_offset, int *tx_chain_offset)
{
	int i;
	int8 band_max, band_min = 0;
	int8 offset;
	struct wlc_channel_txchain_limits lim;
	wlc_cm_info_t *cmi = wlc->cmi;
	bool offset_diff = FALSE;

	/* init limits to max value for int8 to not impose any limit */
	memset(&lim, 0x7f, sizeof(struct wlc_channel_txchain_limits));

	/* find max target power for the band */
	band_max = wlc_channel_max_tx_power_for_band(wlc, band, &band_min);

	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
		if (i < num_chains) {
			offset =
			(int8) (*(txcpd_power_offset + i) * WLC_TXPWR_DB_FACTOR);
		}
		else
			offset = 0;

		if (offset != 0)
			lim.chain_limit[i] = band_max + offset;

		/* check if the new offsets are equal to the previous offsets */
		if (*(tx_chain_offset + i) != *(txcpd_power_offset + i)) {
			offset_diff = TRUE;
		}
	}

	if (offset_diff == TRUE) {
		wlc_channel_band_chain_limit(cmi, band, &lim);
	}
}

void
wlc_channel_apply_band_chain_limits(struct wlc_info *wlc,
	int band, int8 band_max, int num_chains, int *txcpd_power_offset, int *tx_chain_offset)
{
	int i;
	int8 offset;
	struct wlc_channel_txchain_limits lim;
	wlc_cm_info_t *cmi = wlc->cmi;
	bool offset_diff = FALSE;

	/* init limits to max value for int8 to not impose any limit */
	memset(&lim, 0x7f, sizeof(struct wlc_channel_txchain_limits));

	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
		if (i < num_chains) {
			offset = (int8) (*(txcpd_power_offset + i) * WLC_TXPWR_DB_FACTOR);
		}
		else
			offset = 0;

		if (offset != 0)
			lim.chain_limit[i] = band_max + offset;

		/* check if the new offsets are equal to the previous offsets */
		if (*(tx_chain_offset + i) != *(txcpd_power_offset + i)) {
			offset_diff = TRUE;
		}
	}

	if (offset_diff == TRUE) {
		wlc_channel_band_chain_limit(cmi, band, &lim);
	}
}

#ifdef WLC_TXPWRCAP
static int
wlc_channel_txcap_set_country(wlc_cm_info_t *wlc_cmi)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	int err = BCME_OK;
	BCM_REFERENCE(wlc);
	WL_TXPWRCAP(("%s: country %c%c, channel %d\n", __FUNCTION__,
		wlc_cm->country_abbrev[0], wlc_cm->country_abbrev[1],
		wf_chspec_ctlchan(wlc->chanspec)));
	if (wlc_cm->txcap_download == NULL) {
		WL_TXPWRCAP(("%s: No txcap download installed\n", __FUNCTION__));
		goto exit;
	}
	/* Search in txcap table for the current country and subband to find limits for antennas */
	{
		uint8 *p;
		txcap_file_header_t *txcap_file_header;
		uint8 num_subbands;
		uint8 num_cc_groups;
		uint8 num_antennas;
		txcap_file_cc_group_info_t *cc_group_info;
		uint8 num_cc;
		uint8 *cc = NULL; /* WAR for wrong compiler might not be defined warning */
		int i, j;
		p = (uint8 *)wlc_cm->txcap_download;
		txcap_file_header = (txcap_file_header_t *)p;
		num_subbands = txcap_file_header->num_subbands;
		num_antennas = 0;
		for (i = 0; i < TXPWRCAP_MAX_NUM_CORES; i++) {
			num_antennas += txcap_file_header->num_antennas_per_core[i];
		}
		num_cc_groups = txcap_file_header->num_cc_groups;
		p += OFFSETOF(txcap_file_header_t, cc_group_info);
		for (i = 0; i < num_cc_groups; i++) {
			cc_group_info = (txcap_file_cc_group_info_t *)p;
			num_cc = cc_group_info->num_cc;
			p += 1;
			WL_TXPWRCAP(("%s: checking cc group %d, num_cc %d\n",
				__FUNCTION__, i, num_cc));
			for (j = 0; j < num_cc; j++) {
				cc = (uint8 *)p;
				p += 2;
				if (!memcmp(cc, "*", 1) || !memcmp(cc, wlc_cm->country_abbrev, 2)) {
					WL_TXPWRCAP(("%s: Matched entry %d in this cc group %d\n",
						__FUNCTION__, j, i));
					wlc_cm->current_country_cc_group_info = cc_group_info;
					wlc_cm->current_country_cc_group_info_index = i;
					wlc_cm->txcap_download_num_antennas = num_antennas;
					goto exit;
				}
			}
			if (wlc_cm->txcap_cap_states_per_cc_group == 2) {
				p += 2*num_subbands*num_antennas;
			} else {
				p += 4*num_subbands*num_antennas;
			}
		}
		WL_TXPWRCAP(("%s: Country not found! Clearing current_country_cc_group_info\n",
			__FUNCTION__));
		wlc_cm->current_country_cc_group_info = NULL;
	}
exit:
	return err;
}

static int
wlc_channel_txcap_phy_update(wlc_cm_info_t *wlc_cmi,
	wl_txpwrcap_tbl_t *txpwrcap_tbl, int* cellstatus)
{
	wlc_info_t *wlc = wlc_cmi->wlc;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	int err = BCME_OK;
	wl_txpwrcap_tbl_t wl_txpwrcap_tbl;
	uint8 *p;
	int8 *pwrcap_cell_on;
	int8 *pwrcap_cell_off;
	int8 *pwrcap;
	uint8 num_subbands;
	uint8 num_antennas;
	int new_cell_status;
	WL_TXPWRCAP(("%s: country %c%c, channel %d\n", __FUNCTION__,
		wlc_cm->country_abbrev[0], wlc_cm->country_abbrev[1],
		wf_chspec_ctlchan(wlc->chanspec)));
	if (wlc_cm->current_country_cc_group_info) {
		uint8 channel;
		uint8 subband = 0;
		num_subbands =  wlc_cm->txcap_download->num_subbands,
		num_antennas = wlc_cm->txcap_download_num_antennas;
		memcpy(wl_txpwrcap_tbl.num_antennas_per_core,
			wlc_cm->txcap_download->num_antennas_per_core,
			sizeof(wl_txpwrcap_tbl.num_antennas_per_core));
		channel = wf_chspec_ctlchan(wlc->chanspec);
		if (channel <= 14) {
			subband = 0;
		} else if (channel <= 48) {
			subband = 1;
		} else if (channel <= 64) {
			subband = 2;
		} else if (channel <= 144) {
			subband = 3;
		} else {
			subband = 4;
		}
		WL_TXPWRCAP(("Mapping channel %d to subband %d\n", channel, subband));
		p = (uint8 *)wlc_cm->current_country_cc_group_info;
		p += OFFSETOF(txcap_file_cc_group_info_t, cc_list) +
			2 * wlc_cm->current_country_cc_group_info->num_cc;
		pwrcap_cell_on = (int8 *)(p + subband*num_antennas);
		p += num_subbands*num_antennas;
		pwrcap_cell_off = (int8 *)(p + subband*num_antennas);
		if (wlc_cm->txcap_config[subband] == TXPWRCAPCONFIG_WCI2_AND_HOST) {
			int8 *pwrcap_cell_on_2;
			int8 *pwrcap_cell_off_2;
			int ant;
			pwrcap_cell_on_2 = (int8 *)
				(pwrcap_cell_on + 2 * (num_subbands*num_antennas));
			pwrcap_cell_off_2 = (int8 *)
				(pwrcap_cell_off + 2 * (num_subbands*num_antennas));
			if (wlc_cm->txcap_wci2_cell_status_last & 2) {
				if (wlc_cm->txcap_high_cap_active &&
					(wlc_cm->txcap_state[subband] ==
					TXPWRCAPSTATE_HOST_LOW_WCI2_HIGH_CAP ||
					wlc_cm->txcap_state[subband] ==
					TXPWRCAPSTATE_HOST_HIGH_WCI2_HIGH_CAP)) {
					new_cell_status = 0;
				} else {
					new_cell_status = 1;
				}
			} else if (wlc_cm->txcap_wci2_cell_status_last & 1) {
				new_cell_status = 1;
			} else {
				new_cell_status = 0;
			}
			/* XXX If no recent host state update then use worst case WCI2 on/off
			 * values from head or body.
			 */
			if (!wlc_cm->txcap_high_cap_active) {
				for (ant = 0; ant < num_antennas; ant++) {
					wl_txpwrcap_tbl.pwrcap_cell_on[ant] =
					MIN(pwrcap_cell_on[ant], pwrcap_cell_on_2[ant]);
					wl_txpwrcap_tbl.pwrcap_cell_off[ant] =
					MIN(pwrcap_cell_off[ant], pwrcap_cell_off_2[ant]);
				}
			} else if (wlc_cm->txcap_state[subband] ==
				TXPWRCAPSTATE_HOST_LOW_WCI2_LOW_CAP ||
				wlc_cm->txcap_state[subband] ==
				TXPWRCAPSTATE_HOST_LOW_WCI2_HIGH_CAP) {
				memcpy(&wl_txpwrcap_tbl.pwrcap_cell_on, pwrcap_cell_on,
					num_antennas);
				memcpy(&wl_txpwrcap_tbl.pwrcap_cell_off, pwrcap_cell_off,
					num_antennas);
				} else { /* TXPWRCAPSTATE_HOST_HIGH WCI2_LOW_CAP or WCI2_HIGH_CAP */
					memcpy(&wl_txpwrcap_tbl.pwrcap_cell_on, pwrcap_cell_on_2,
						num_antennas);
					memcpy(&wl_txpwrcap_tbl.pwrcap_cell_off, pwrcap_cell_off_2,
						num_antennas);
				}
			} else
		if (wlc_cm->txcap_config[subband] == TXPWRCAPCONFIG_WCI2) {
			if (wlc_cm->txcap_wci2_cell_status_last & 2) {
				if (wlc_cm->txcap_high_cap_active &&
					wlc_cm->txcap_state[subband] == TXPWRCAPSTATE_HIGH_CAP) {
					new_cell_status = 0;
				} else {
					new_cell_status = 1;
				}
			} else if (wlc_cm->txcap_wci2_cell_status_last & 1) {
				new_cell_status = 1;
			} else {
				new_cell_status = 0;
			}
			memcpy(&wl_txpwrcap_tbl.pwrcap_cell_on, pwrcap_cell_on, num_antennas);
			memcpy(&wl_txpwrcap_tbl.pwrcap_cell_off, pwrcap_cell_off, num_antennas);
		} else { /* TXPWRCAPCONFIG_HOST */
			/* Use the same values for cell on and off.  You might think you would pass
			 * both the cell on and off values as normal and rely on the new cell status
			 * value to pick the host selected/forced value.  But because the PSM uses
			 * the cell on (i.e. low cap) values as a 'fail safe' as it wakes from sleep
			 * you have to pass the same values.  In this way you defeat the 'fail-safe'
			 * which is what you want if you are in host mode and WCI2 is not relevant.
			 *
			 * Technically the duplication is only needed for a new cell status of
			 * off.  If the new cell status is on then the PSM is going to use the
			 * cell on values, both for fail-safe and othewise.  The * cell off values
			 * will not get used by the PSM.
			 */
			if (wlc_cm->txcap_high_cap_active &&
				wlc_cm->txcap_state[subband] == TXPWRCAPSTATE_HIGH_CAP) {
				new_cell_status = 0;
				pwrcap = pwrcap_cell_off;
			} else {
				new_cell_status = 1;
				pwrcap = pwrcap_cell_on;
			}
			memcpy(&wl_txpwrcap_tbl.pwrcap_cell_on, pwrcap, num_antennas);
			memcpy(&wl_txpwrcap_tbl.pwrcap_cell_off, pwrcap, num_antennas);
		}
		if (txpwrcap_tbl) {
			memcpy(txpwrcap_tbl, &wl_txpwrcap_tbl, sizeof(wl_txpwrcap_tbl_t));
		} else {
			err = wlc_phy_txpwrcap_tbl_set(WLC_PI(wlc), &wl_txpwrcap_tbl);
		}

		if (cellstatus) {
			*cellstatus = new_cell_status;
		}
		else {
			WL_TXPWRCAP(("%s: setting phy cell status to %s\n",
				__FUNCTION__, new_cell_status ? "ON" : "OFF"));
			wlc_phyhal_txpwrcap_set_cellstatus(WLC_PI(wlc), new_cell_status);
		}
	} else {
	}
	return err;
}

static dload_error_status_t
wlc_handle_txcap_dload(wlc_info_t *wlc, wlc_blob_segment_t *segments, uint32 segment_count)
{
	wlc_cm_info_t *wlc_cmi = wlc->cmi;
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	dload_error_status_t status = DLOAD_STATUS_DOWNLOAD_SUCCESS;
	txcap_file_header_t *txcap_dataptr;
	int txcap_data_len, idx = 0;
	uint32 chip;
	wlc_info_t *wlc_cur = NULL;
	/* Make sure we have a chip id segment and txcap data segemnt */
	if (segment_count < 2) {
		return DLOAD_STATUS_TXCAP_BLOB_FORMAT;
	}
	/* Check to see if chip id segment is correct length */
	if (segments[1].length != 4) {
		return DLOAD_STATUS_TXCAP_BLOB_FORMAT;
	}
	/* Check to see if chip id matches this chip's actual value */
	chip = load32_ua(segments[1].data);
	/* There are cases that chip id is updated for modules but not ref board, we need to check
	 * both here
	 */
	if ((chip != SI_CHIPID(wlc_cmi->pub->sih)) && (chip != wlc_cmi->pub->sih->chip)) {
		return DLOAD_STATUS_TXCAP_MISMATCH;
	}
	/* Process actual clm data segment */
	txcap_dataptr = (txcap_file_header_t *)(segments[0].data);
	txcap_data_len = segments[0].length;
	/* Check that the header is big enough to access fixed fields */
	if (txcap_data_len < sizeof(txcap_file_header_t))  {
		return DLOAD_STATUS_TXCAP_DATA_BAD;
	}
	/* Check for magic string at the beginning */
	if (memcmp(txcap_dataptr->magic, "TXCP", 4) != 0) {
		return DLOAD_STATUS_TXCAP_DATA_BAD;
	}
	/* Check for version 2.  That is all we support, i.e. no backward compatiblity */
	if (txcap_dataptr->version != 2) {
		return DLOAD_STATUS_TXCAP_DATA_BAD;
	} else {
		if (txcap_dataptr->flags & TXCAP_FILE_HEADER_FLAG_WCI2_AND_HOST_MASK) {
			wlc_cm->txcap_cap_states_per_cc_group = 4;
		} else {
			wlc_cm->txcap_cap_states_per_cc_group = 2;
		}
	}
	/* At this point forward we are responsible for freeing this data pointer */
	segments[0].data = NULL;
	/* Save the txcap download after freeing any previous txcap download */
	if (wlc_cm->txcap_download != NULL) {
		MFREE(wlc_cmi->pub->osh, wlc_cm->txcap_download, wlc_cm->txcap_download_size);
	}
	wlc_cm->txcap_download = txcap_dataptr;
	wlc_cm->txcap_download_size = txcap_data_len;
	/* XXX JAVB REVIST - what else should be clean up on new download: timer?, ???
	 * note txcapload requires/enforces driver is down
	 */
	memset(wlc_cm->txcap_config, TXPWRCAPCONFIG_WCI2, TXPWRCAP_NUM_SUBBANDS);
	memset(wlc_cm->txcap_state, TXPWRCAPSTATE_LOW_CAP, TXPWRCAP_NUM_SUBBANDS);

	wlc_channel_txcap_set_country(wlc_cmi);
	FOREACH_WLC(wlc_cmi->wlc->cmn, idx, wlc_cur) {
		wlc_cmi = wlc_cur->cmi;
		wlc_channel_txcap_phy_update(wlc_cmi, NULL, NULL);
	}
	return status;
}

static int
wlc_channel_txcapver(wlc_cm_info_t *wlc_cmi, struct bcmstrbuf *b)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	if (wlc_cm->txcap_download == NULL) {
		bcm_bprintf(b, "No txcap file downloaded\n");
	} else {
		 /* Print the downloaded txcap binary file structure */
		uint8 *p;
		txcap_file_header_t *txcap_file_header;
		p = (uint8 *)wlc_cm->txcap_download;
		txcap_file_header = (txcap_file_header_t *)p;
		bcm_bprintf(b, "Data Title: %s\n", txcap_file_header->title);
		bcm_bprintf(b, "Data Creation: %s\n", txcap_file_header->creation);
	}
	return BCME_OK;
}

void
wlc_channel_txcap_cellstatus_cb(wlc_cm_info_t *wlc_cmi, int cellstatus)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	WL_TXPWRCAP(("%s: ucode reporting cellstatus %s\n", __FUNCTION__,
		(cellstatus & 2) ? "UNKNOWN" :
		(cellstatus & 1) ? "ON" : "OFF"));
	wlc_cm->txcap_wci2_cell_status_last = (uint8)cellstatus;
	/* Special processing to allow cell status/diversity to work when there is
	 * no txcap file downloaded.
	 */
	if (wlc_cm->txcap_download == NULL) {
		int new_cell_status;
		if (cellstatus & 2) {
			/* Unknown cell status is translated to a cell status of 0/off
			 * which means that diversity is enabled.  This allows diversity
			 * if there is no txcap file downloaded.
			 */
			new_cell_status = 0;
		} else {
			new_cell_status = cellstatus & 1;
		}
		WL_TXPWRCAP(("%s: setting phy cell status to %s\n",
			__FUNCTION__, new_cell_status ? "ON" : "OFF"));
		wlc_phyhal_txpwrcap_set_cellstatus(
			WLC_PI(wlc_cmi->wlc),
			new_cell_status);
	} else {
		wlc_channel_txcap_phy_update(wlc_cmi, NULL, NULL);
	}
}
#endif /* WLC_TXPWRCAP */

#ifdef WL_EXPORT_CURPOWER
/* Converts power limits in CLM format (clm_power_limits_t) to PPR format (ppr_t) */
static void
wlc_channel_clm_limits_to_ppr(const clm_power_limits_t *clm_pwr, ppr_t *ppr_pwr, wl_tx_bw_t bw)
{
	typedef enum rate_type {
		RT_DSSS,
		RT_OFDM,
		RT_HT,
		RT_VHT
	} rate_type_t;
	/* Rate group - corresponds to one ppr_..._rateset */
	typedef struct rate_group {
		rate_type_t rt;		/* Rate type */
		int clm_rate;		/* First CLM rate (WL_RATE_...) */
		wl_tx_chains_t chains;  /* Number of expanded chains */
		wl_tx_nss_t nss;	/* Number of unexpanded chains */
		wl_tx_mode_t mode;	/* Expansion method */
	} rate_group_t;
#if WL_NUMRATES == 100
	/* Original rate definitions - no VHT rates, i.e. VHT rateset has HT length */
	#define VHT_RT RT_HT
#else
	/* Modern rate definitions, VHT rateset (almost?) always has VHT length */
	#define VHT_RT RT_VHT
	#define HAS_VHT_RATES
#endif /* WL_NUMRATES == 100 */
#if WL_NUMRATES <= 178
	/* VHT TXBF0 rateset has HT length */
	#define VHT_TXBF0_RT RT_HT
#else
	/* VHT TXBF0 rateset has VHT length */
	#define VHT_TXBF0_RT RT_VHT
#endif /* WL_NUMRATES <= 178 */
#if WL_NUMRATES >= 178
	#define HAS_TXBF
#endif /* WL_NUMRATES >= 178 */
#if WL_NUMRATES >= 336
	#define HAS_4TX
#endif /* WL_NUMRATES >= 336 */
/* In subsequent macros 'chains' is number of expanded chains, mcs is 802.11n MCS index of first
 * rate in rateset, nss is number of unexpanded chains
 */
#define RATE_GROUP_DSSS(chains) {RT_DSSS, WL_RATE_1X##chains##_DSSS_1, WL_TX_CHAINS_##chains, \
	WL_TX_NSS_1, WL_TX_MODE_NONE}
#define RATE_GROUP_OFDM() {RT_OFDM, WL_RATE_1X1_OFDM_6, WL_TX_CHAINS_1, WL_TX_NSS_1, \
	WL_TX_MODE_NONE}
#define RATE_GROUP_OFDM_EXP(chains, exp) {RT_OFDM, WL_RATE_1X##chains##_##exp##_OFDM_6, \
	WL_TX_CHAINS_##chains, WL_TX_NSS_1, WL_TX_MODE_##exp}
#define RATE_GROUP_VHTSS1() {VHT_RT, WL_RATE_1X1_MCS0, WL_TX_CHAINS_1, WL_TX_NSS_1, \
	WL_TX_MODE_NONE}
#define RATE_GROUP_VHT_SS1_CDD(chains) {VHT_RT, WL_RATE_1X##chains##_CDD_MCS0, \
	WL_TX_CHAINS_##chains, WL_TX_NSS_1, WL_TX_MODE_CDD}
#define RATE_GROUP_VHT_SS1_STBC(chains) {VHT_RT, WL_RATE_2X##chains##_STBC_MCS0, \
	WL_TX_CHAINS_##chains, WL_TX_NSS_2, WL_TX_MODE_STBC}
#define RATE_GROUP_VHT_SS1_TXBF(chains) {VHT_RT, WL_RATE_1X##chains##_TXBF_MCS0, \
	WL_TX_CHAINS_##chains, WL_TX_NSS_1, WL_TX_MODE_TXBF}
#define RATE_GROUP_VHT(chains, mcs, nss) {VHT_RT, WL_RATE_##nss##X##chains##_SDM_##mcs, \
	WL_TX_CHAINS_##chains, WL_TX_NSS_##nss, WL_TX_MODE_NONE}
#define RATE_GROUP_VHT_TXBF0(chains, mcs) {VHT_TXBF0_RT, \
	WL_RATE_##chains##X##chains##_SDM_##mcs, WL_TX_CHAINS_##chains, WL_TX_NSS_##chains, \
	WL_TX_MODE_TXBF}
#define RATE_GROUP_VHT_TXBF(chains, mcs, nss) {RT_VHT, WL_RATE_##nss##X##chains##_SDM_##mcs, \
	WL_TX_CHAINS_##chains, WL_TX_NSS_##nss, WL_TX_MODE_TXBF}
	static const rate_group_t rate_groups[] = {
		RATE_GROUP_DSSS(1),
		RATE_GROUP_OFDM(),
		RATE_GROUP_VHTSS1(),

		RATE_GROUP_DSSS(2),
		RATE_GROUP_OFDM_EXP(2, CDD),
		RATE_GROUP_VHT_SS1_CDD(2),
		RATE_GROUP_VHT_SS1_STBC(2),
		RATE_GROUP_VHT(2, MCS8, 2),

		RATE_GROUP_DSSS(3),
		RATE_GROUP_OFDM_EXP(3, CDD),
		RATE_GROUP_VHT_SS1_CDD(3),
		RATE_GROUP_VHT_SS1_STBC(3),
		RATE_GROUP_VHT(3, MCS8, 2),
		RATE_GROUP_VHT(3, MCS16, 3),

#ifdef HAS_TXBF
		RATE_GROUP_OFDM_EXP(2, TXBF),
		RATE_GROUP_VHT_SS1_TXBF(2),
		RATE_GROUP_VHT_TXBF0(2, MCS8),

		RATE_GROUP_OFDM_EXP(3, TXBF),
		RATE_GROUP_VHT_SS1_TXBF(3),
		RATE_GROUP_VHT_TXBF(3, MCS8, 2),
		RATE_GROUP_VHT_TXBF0(3, MCS16),
#ifdef HAS_4TX
		RATE_GROUP_DSSS(4),
		RATE_GROUP_OFDM_EXP(4, CDD),
		RATE_GROUP_VHT_SS1_CDD(4),
		RATE_GROUP_VHT_SS1_STBC(4),
		RATE_GROUP_VHT(4, MCS16, 3),
		RATE_GROUP_VHT(4, MCS24, 4),
		RATE_GROUP_VHT(4, MCS8, 2),
		RATE_GROUP_OFDM_EXP(4, TXBF),
		RATE_GROUP_VHT_SS1_TXBF(4),
		RATE_GROUP_VHT_TXBF(4, MCS8, 2),
		RATE_GROUP_VHT_TXBF(4, MCS16, 3),
		RATE_GROUP_VHT_TXBF0(4, MCS24),
#endif /* HAS_4TX */
#endif /* HAS_TXBF */
	};
	size_t i;
	for (i = 0; i < ARRAYSIZE(rate_groups); ++i) {
		const clm_power_t *clm_rateset = clm_pwr->limit + rate_groups[i].clm_rate;
		switch (rate_groups[i].rt) {
		case RT_DSSS:
			ppr_set_dsss(ppr_pwr, bw, rate_groups[i].chains,
				(const ppr_dsss_rateset_t *)clm_rateset);
			break;
		case RT_OFDM:
			ppr_set_ofdm(ppr_pwr, bw, rate_groups[i].mode, rate_groups[i].chains,
				(const ppr_ofdm_rateset_t *)clm_rateset);
			break;
		case RT_HT:
			ppr_set_ht_mcs(ppr_pwr, bw, rate_groups[i].nss, rate_groups[i].mode,
				rate_groups[i].chains, (const ppr_ht_mcs_rateset_t *)clm_rateset);
			break;
		case RT_VHT:
			ppr_set_vht_mcs(ppr_pwr, bw, rate_groups[i].nss, rate_groups[i].mode,
				rate_groups[i].chains, (const ppr_vht_mcs_rateset_t *)clm_rateset);
			break;
		}
	}
#undef VHT_RT
#undef VHT_TXBF0_RT
#ifdef HAS_4TX
	#undef HAS_4TX
#endif /* HAS_4TX */
#ifdef HAS_VHT_RATES
	#undef HAS_VHT_RATES
#endif /* HAS_VHT_RATES */
#undef RATE_GROUP_DSSS
#undef RATE_GROUP_OFDM
#undef RATE_GROUP_OFDM_EXP
#undef RATE_GROUP_VHTSS1
#undef RATE_GROUP_VHT_SS1_CDD
#undef RATE_GROUP_VHT_SS1_STBC
#undef RATE_GROUP_VHT_SS1_TXBF
#undef RATE_GROUP_VHT
#undef RATE_GROUP_VHT_TXBF0
#undef RATE_GROUP_VHT_TXBF
}

/* Enum translation element - translates one value */
typedef struct wlc_enum_translation {
	uint from, to;
} wlc_enum_translation_t;

/* Enum translation. Returns 'to' from enum translation element with given 'from' (if found) or
 * 'def_value'
 */
static uint
wlc_translate_enum(const wlc_enum_translation_t *translation, size_t len, uint from, uint def_value)
{
	while (len--) {
		if (translation++->from == from) {
			return translation[-1].to;
		}
	}
	return def_value;
}

static int
wlc_get_clm_power_limits(wlc_cm_info_t *wlc_cmi, wlc_clm_power_limits_req_t *arg, int arg_buf_len)
{
	clm_country_t country;
	clm_country_locales_t locales;
	clm_limits_params_t limits_params;
	chanspec_t chanspec;
	uint channel;
	wl_tx_bw_t tx_bw;
	clm_band_t band;
	uint regrev;
	unsigned long country_flags;
	clm_power_limits_t power_limits;
	ppr_t *tx_power_ppr;
	int err = BCME_OK;
	wlcband_t *band_state;
	char unused_mapped_ccode[WLC_CNTRY_BUF_SZ];
	/* WL_CHANSPEC_BW_... -> clm_bandwidth_t translation */
	static const wlc_enum_translation_t bw_translation[] = {
		{WL_CHANSPEC_BW_20, CLM_BW_20},
		{WL_CHANSPEC_BW_40, CLM_BW_40},
#ifdef WL11AC
#ifdef WL_CHANSPEC_BW_80
		{WL_CHANSPEC_BW_80, CLM_BW_80},
#endif /* WL_CHANSPEC_BW_80: */
#ifdef WL_CHANSPEC_BW_160
		{WL_CHANSPEC_BW_160, CLM_BW_160},
#endif /* WL_CHANSPEC_BW_160: */
#ifdef WL_CHANSPEC_BW_8080
		{WL_CHANSPEC_BW_8080, CLM_BW_80_80},
#endif /* WL_CHANSPEC_BW_8080 */
#endif /* WL11AC */
	};
	/* WL_CHANSPEC_BW_... -> wl_tx_bw_t translation */
	static const wlc_enum_translation_t tx_bw_translation[] = {
		{WL_CHANSPEC_BW_20, WL_TX_BW_20},
		{WL_CHANSPEC_BW_40, WL_TX_BW_40},
#ifdef WL11AC
		{WL_CHANSPEC_BW_80, WL_TX_BW_80},
		{WL_CHANSPEC_BW_160, WL_TX_BW_160},
#endif /* WL11AC */
	};
	/* WL_CHANSPEC_BAND_... -> wl_band_t translation */
	static const wlc_enum_translation_t band_translation[] = {
		{WL_CHANSPEC_BAND_2G, CLM_BAND_2G}, {WL_CHANSPEC_BAND_5G, CLM_BAND_5G},
	};

	if (arg->version != 1) {
		return BCME_VERSION;
	}
	/* If `buflen` longer than actual buffer length - it is wrong */
	if ((size_t)arg->buflen > (size_t)arg_buf_len) {
		WL_ERROR(("\"clm_power_limits\" iovar: invalid buflen: %d\n",
			arg->buflen));
		return BCME_BADARG;
	}

	/* Checking validity of some parameters */
	chanspec = (chanspec_t)arg->chanspec;
	if (!wf_chspec_valid(chanspec)) {
		WL_ERROR(("\"clm_power_limits\" iovar: invalid chanspec: 0x%04X\n",
			chanspec));
		return BCME_BADARG;
	}
	if (arg->antenna_idx >= MIN(WLC_TXCORE_MAX, WL_TX_CHAINS_MAX)) {
		WL_ERROR(("\"clm_power_limits\" iovar: invalid antenna index: %d\n",
			arg->antenna_idx));
		return BCME_BADARG;
	}
	arg->output_flags = 0;
	/* Finding region iterator and regrev of CLM region */
	if (wlc_countrycode_map(wlc_cmi, arg->cc, unused_mapped_ccode, &regrev, &country) !=
		CLM_RESULT_OK)
	{
		/* Found none - region with given CC is unavailable */
		return BCME_NOTFOUND;
	}
	/* Now determine type of region (WLC_CLM_POWER_LIMITS_OUTPUT_FLAG_... flag to assign) */
	if (!bcmp(arg->cc, wlc_cmi->cm->srom_ccode, sizeof(ccode_t))) {
		/* CC same as in SROM - worlwide region */
		arg->output_flags |= WLC_CLM_POWER_LIMITS_OUTPUT_FLAG_WORLDWIDE_LIMITS;
	} else if (regrev != 0) {
		/* Nonzero regrev - product-specific region */
		arg->output_flags |= WLC_CLM_POWER_LIMITS_OUTPUT_FLAG_PRODUCT_LIMITS;
	} else {
		clm_country_t c;
		ccode_t cc;
		uint r;
		/* Zero regrev - product-specific in regervless configuration, country-default
		 * otherwise
		 */
		/* Assume product-specific and try to find nonzero regrev in CLM regions */
		arg->output_flags |= WLC_CLM_POWER_LIMITS_OUTPUT_FLAG_PRODUCT_LIMITS;
		(void)clm_iter_init(&c);
		while (clm_country_iter(&c, cc, &r) == CLM_RESULT_OK) {
			/* CLM data has nonzero regrevs - hence zero regrev means country-default */
			if (r) {
				arg->output_flags |=
					WLC_CLM_POWER_LIMITS_OUTPUT_FLAG_DEFAULT_COUNTRY_LIMITS;
				break;
			}
		}
	}
	if (clm_country_def(country, &locales) != CLM_RESULT_OK) {
		/* If, somehow, region not found despite there is iterator pointing to it - abandon
		*/
		return BCME_NOTFOUND;
	}
	/* CLM region found - obtain its flags */
	clm_country_flags(&locales, CLM_BAND_2G, &country_flags);
	arg->clm_country_flags_2g = (uint32)country_flags;
	clm_country_flags(&locales, CLM_BAND_5G, &country_flags);
	arg->clm_country_flags_5g = (uint32)country_flags;

	/* Preparing parameters for CLM limits retrieval */
	(void)clm_limits_params_init(&limits_params);
	limits_params.antenna_idx = arg->antenna_idx;
	/* If SAR limits is needed and present - obtain it */
	if ((!(arg->input_flags & WLC_CLM_POWER_LIMITS_INPUT_FLAG_NO_SAR)) &&
		wlc_channel_sarenable_get(wlc_cmi))
	{
		uint8 sarlimit[WLC_TXCORE_MAX] = {0};
		wlc_channel_sarlimit_subband(wlc_cmi, chanspec, sarlimit);
		limits_params.sar = sarlimit[arg->antenna_idx];
	}
	if ((limits_params.bw = (clm_bandwidth_t)wlc_translate_enum(bw_translation,
		ARRAYSIZE(bw_translation), CHSPEC_BW(chanspec), ~0)) == (clm_bandwidth_t)~0)
	{
		WL_ERROR(("\"clm_power_limits\" iovar: unsupported bandwidth\n"));
		return BCME_BADARG;
	}
	channel = CHSPEC_CHANNEL(chanspec);
#ifdef WL11AC
	if (limits_params.bw == CLM_BW_80_80) {
		channel = wf_chspec_primary80_channel(chanspec);
		limits_params.other_80_80_chan = wf_chspec_secondary80_channel(chanspec);
	}
#endif /* WL11AC */
	if ((band = (clm_band_t)wlc_translate_enum(band_translation, ARRAYSIZE(band_translation),
		CHSPEC_BAND(chanspec), ~0)) == (clm_band_t)~0)
	{
		WL_ERROR(("\"clm_power_limits\" iovar: unsupported band\n"));
		return BCME_BADARG;
	}
	band_state = wlc_cmi->wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	/* clm_limits() parameters computed - obtain CLM limits */
	switch (clm_limits(&locales, band, channel, band_state->antgain,
		(clm_limits_type_t)arg->clm_subchannel, &limits_params, &power_limits))
	{
	case CLM_RESULT_OK:
		/* Limits obtained */
		break;
	case CLM_RESULT_NOT_FOUND:
		/* Limits not obtained because channel not supported */
		return BCME_BADCHAN;
	default:
		/* Error in input parameters */
		return BCME_ERROR;
	}
	/* CLM parameters obtained - converting them to PPR format */
	if ((tx_bw = (wl_tx_bw_t)wlc_translate_enum(tx_bw_translation,
		ARRAYSIZE(tx_bw_translation), CHSPEC_BW(chanspec), ~0)) == (wl_tx_bw_t)~0)
	{
		WL_ERROR(("\"clm_power_limits\" iovar: unsupported bandwidth\n"));
		return BCME_BADARG;
	}
	tx_power_ppr = ppr_create(wlc_cmi->wlc->osh, tx_bw);
	if (!tx_power_ppr) {
		return BCME_NOMEM;
	}
	wlc_channel_clm_limits_to_ppr(&power_limits, tx_power_ppr, tx_bw);
	/* CLM limits converted to PPR format */
	/* If board limits shall be applied - retrieving and applying them */
	if (!(arg->input_flags & WLC_CLM_POWER_LIMITS_INPUT_FLAG_NO_BOARD)) {
		ppr_t *board_limits_ppr = ppr_create(wlc_cmi->wlc->osh, tx_bw);
		if (!board_limits_ppr) {
			err = BCME_NOMEM;
			goto cleanup;
		}
		wlc_phy_txpower_sromlimit(WLC_PI_BANDUNIT(wlc_cmi->wlc,
			CHSPEC_WLCBANDUNIT(chanspec)), chanspec, NULL, board_limits_ppr,
			(uint8)arg->antenna_idx);
		ppr_apply_vector_ceiling(tx_power_ppr, board_limits_ppr);
		ppr_delete(wlc_cmi->wlc->osh, board_limits_ppr);
	}
	/* Limits computed - now serializing them in TLV format to iovar buffer */
	arg->ppr_tlv_size = ppr_get_tlv_size(tx_power_ppr, tx_bw, WL_TX_CHAINS_MAX);
	if ((OFFSETOF(wlc_clm_power_limits_req_t, ppr_tlv) + arg->ppr_tlv_size) > arg->buflen) {
		err = BCME_BUFTOOSHORT;
		goto cleanup;
	}
	ppr_convert_to_tlv(tx_power_ppr, tx_bw, (uint8 *)arg ->ppr_tlv, arg->ppr_tlv_size,
		WL_TX_CHAINS_MAX);
	/* Limits serialized to iovar buffer */
cleanup:
	/* Freeing PPR buffer */
	if (tx_power_ppr) {
		ppr_delete(wlc_cmi->wlc->osh, tx_power_ppr);
	}
	return err;
}
#endif /* WL_EXPORT_CURPOWER */

#if defined(BCMPCIEDEV_SROM_FORMAT) && defined(WLC_TXCAL)
static dload_error_status_t
wlc_handle_cal_dload_wrapper(wlc_info_t *wlc, wlc_blob_segment_t *segments,
	uint32 segment_count)
{
	return wlc_handle_cal_dload(wlc, segments, segment_count);
}
#endif /* WLC_TXCAL */

#ifdef WL_AP_CHAN_CHANGE_EVENT
void
wlc_channel_send_chan_event(wlc_info_t *wlc, wl_chan_change_reason_t reason,
	chanspec_t chanspec)
{
	wl_event_change_chan_t evt_data;
	ASSERT(wlc != NULL);

	memset(&evt_data, 0, sizeof(evt_data));

	evt_data.version = WL_CHAN_CHANGE_EVENT_VER_1;
	evt_data.length = WL_CHAN_CHANGE_EVENT_LEN_VER_1;
	evt_data.target_chanspec = chanspec;
	evt_data.reason = reason;

	wlc_bss_mac_event(wlc, wlc->cfg, WLC_E_AP_CHAN_CHANGE, NULL, 0,
		0, 0, (void *)&evt_data, sizeof(evt_data));
	WL_TRACE(("wl%d: CHAN Change event to ch 0x%02x\n", WLCWLUNIT(wlc),
		evt_data.target_chanspec));
}
#endif /* WL_CHAN_CHANGE_EVENT */

/* stating from LSB of each octet in bmp of length len, get the position of the n th valid bit
 *	bmp	- bitmap array of octets
 *	len	- length (number of octets in bmp)
 *	n	- n th valid bit's position is required (n must be between 1 and len * 8)
 * Returns the position of the n th valid bit, stating from LSB of each octet in bmp of length len,
 *	returns -1 if not found or on error
 */
static int32 wlc_channel_get_nth_valid_bit_position(uint8 *bmp, int32 len, int32 n)
{
	int32 i = 0;

	if (bmp == NULL || n < 1 || n > len * NBBY) {
		return -1;
	}
	while (i < len * NBBY && n > 0) {
		if (isset(bmp, i)) {
			if (--n == 0) return i;
		}
		i++;
	}

	return -1;
}

/* Returns a valid random DFS/Non-DFS channel or 0 on error */
chanspec_t
wlc_channel_5gchanspec_rand(wlc_info_t *wlc, bool radar_detected)
{
	chanvec_t ch_vec;
	chanspec_t def_chspec = wlc->default_bss->chanspec, chspec, first_valid, rand_chspec;
	uint16 def_sb = CHSPEC_CTL_SB(def_chspec), def_bw = CHSPEC_BW(def_chspec),
	       bw_full, bw_sub, ch1_count, ch1, bw_idx;
#if defined(WL11AC_80P80)
	uint16 ch2_count, ch2;
#endif /* WL11AC_80P80 */
	uint32 rand_tsf = wlc->clk ? R_REG(wlc->osh, D11_TSF_RANDOM(wlc)) : 0;
	int16 rand_idx;
#if defined(BCMDBG)
	char chanbuf[CHANSPEC_STR_LEN];
#endif /* BCMDBG */
	/* list of bandwidth in decreasing order of magnitude */
	uint16 bw_list[] = { WL_CHANSPEC_BW_160, WL_CHANSPEC_BW_80,
			WL_CHANSPEC_BW_40, WL_CHANSPEC_BW_20 };
	/* length of the list of bandwidth */
	uint16 bw_list_len = sizeof(bw_list) / sizeof(bw_list[0]);

	first_valid = rand_chspec = 0;
	/* get to the first acceptable bw */
	for (bw_idx = 0; bw_idx < bw_list_len && bw_list[bw_idx] != def_bw; bw_idx++) {
		/* NO OP */;
	}

	if (bw_idx == bw_list_len) {
		WL_REGULATORY(("radar: %d, def_chspec: 0x%04x, bw_list[%d]: 0x%x, def_bw: 0x%x\n",
				radar_detected, def_chspec, bw_idx,
				(bw_idx < bw_list_len ? bw_list[bw_idx] : -1),
				def_bw));
		return 0;
	}

	while (bw_idx < bw_list_len && rand_chspec == 0) {
		bw_full = bw_sub = bw_list[bw_idx++];
		if (!wlc_is_valid_bw(wlc, wlc->cfg, BAND_5G_INDEX, bw_full)) {
			continue; /* skip unsupported bandwidth */
		}
#if defined(WL11AC_80P80)
		if (bw_full == WL_CHANSPEC_BW_8080) {
			/* for 80p80, look for two 80MHz channels */
			bw_sub = WL_CHANSPEC_BW_80;
		}
#endif /* WL11AC_80P80 */
		memset(&ch_vec, 0, sizeof(ch_vec));
		ch1_count = 0;
		for (ch1 = CH_MIN_5G_CHANNEL; ch1 < MAXCHANNEL; ch1++) {
			chspec = CHBW_CHSPEC(bw_sub, ch1);
			if (!wlc_valid_chanspec_db(wlc->cmi, chspec) ||
				wlc_restricted_chanspec(wlc->cmi, chspec) ||
				!wlc_dfs_valid_ap_chanspec(wlc, chspec)) {
				continue; /* skip invalid chspec */
			}
			if (first_valid == 0) {
				first_valid = chspec; /* valid but still CAC might be required */
			}
			/* just detected radar? Avoid DFS ch unless (pre)cleared eg. in EDCRS_EU */
			if (radar_detected && wlc_quiet_chanspec(wlc->cmi, chspec)) {
				continue; /* CAC is required; skip unavailable DFS radar channels */
			}
			setbit(ch_vec.vec, ch1);
			ch1_count++;
		}
		if (ch1_count == 0) {
			continue; /* no valid channels of given bandwidth found */
		}
		rand_idx = rand_tsf % ch1_count;
#if defined(WL11AC_80P80)
		if (bw_full == WL_CHANSPEC_BW_8080) {
			if (ch1_count < 2) {
				continue; /* at least two 80MHz ch are required for 80p80 */
			}
			/* reduce rand_idx to make way for ch2; if count is 2, just use both 80 */
			rand_idx = ch1_count == 2 ? 0 : (rand_idx > 1 ? rand_idx -1 : rand_idx);
		}
		ch2_count = ch1_count - rand_idx -1; /* count of valid channels left for ch2 */
#endif /* WL11AC_80P80 */
		/* choose 'rand_idx'th channel */
		ch1 = (uint8) (WL_CHANSPEC_CHAN_MASK & wlc_channel_get_nth_valid_bit_position(
					ch_vec.vec, sizeof(ch_vec.vec), rand_idx + 1));
		if (ch1 == 0 || ch1 == MAXCHANNEL) {
			continue; /* failed to find an usable channel */
		}
		rand_chspec = CHBW_CHSPEC(bw_full, ch1);
#if defined(WL11AC_80P80)
		if (bw_full == WL_CHANSPEC_BW_8080) {
			uint8 pri20 = ch1 - CH_20MHZ_APART - CH_10MHZ_APART; /* zeroth side band */
			rand_idx += rand_tsf % ch2_count;
			ch2 = (uint8) (WL_CHANSPEC_CHAN_MASK &
				wlc_channel_get_nth_valid_bit_position(ch_vec.vec,
					sizeof(ch_vec.vec), rand_idx + 1));
			if (ch2 == 0 || ch2 == MAXCHANNEL) {
				continue; /* second valid channel of 80MHz bw not found */
			}
			rand_chspec = wf_chspec_get8080_chspec(pri20, ch1, ch2);
		}
#endif /* WL11AC_80P80 */

		if (bw_full == def_bw) { /* if the bandwidth matches default BSS channel's */
			rand_chspec |= def_sb; /* we can use the default BSS sideband */
		}
	}

	if (rand_chspec == 0 && first_valid != 0) {
		rand_chspec = first_valid;
	}

	if (rand_chspec == 0) {
		return 0;
	}

#if defined(BCMDBG)
	WL_REGULATORY(("wl%d: %s: dfs selected random chanspec %s (%04x)\n", wlc->pub->unit,
			__FUNCTION__, wf_chspec_ntoa(rand_chspec, chanbuf), rand_chspec));
#endif // endif

	ASSERT(wlc_valid_chanspec_db(wlc->cmi, rand_chspec));

	return rand_chspec;
}

/*
 * Return a valid 2g chanspec of current BW. If none are found, returns 0.
 */
chanspec_t
wlc_channel_next_2gchanspec(wlc_cm_info_t *wlc_cmi, chanspec_t cur_chanspec)
{
	chanspec_t chspec;
	uint16 chan;
	uint16 cur_bw = CHSPEC_BW_GE(cur_chanspec, WL_CHANSPEC_BW_80) ? WL_CHANSPEC_BW_40 :
			CHSPEC_BW(cur_chanspec);

	for (chan = 1; chan <= CH_MAX_2G_CHANNEL; chan++) {
		chspec = CHBW_CHSPEC(cur_bw, chan);
		if (wlc_valid_chanspec(wlc_cmi, chspec)) {
			return chspec;
		}
	}
	return 0;
}

#ifdef WL_GLOBAL_RCLASS
/* check client's 2g/5g band and global operating support capability */
uint8
wlc_sta_supports_global_rclass(uint8 *rclass_bitmap)
{
	uint8 global_opclass_support = 0;

	/* MAXRCLIST = 3 for 20, 40 lower and 40 upper RCINFO
	 * MACRCLIST_REG = 2 for 80 and 160 RCINFO
	 */
	const rcinfo_t *rcinfo[MAXRCLIST + MAXRCLIST_REG] = {NULL};

	/* As of now check for Global opclass support, if present update
	 * and return.
	 * TODO: check for country specific opclass if present and update
	 * country specific opclass info, may be useful later
	 */
	/* check for 2G band */
	if (isset(rclass_bitmap, WLC_REGCLASS_GLOBAL_2G_20MHZ) ||
		isset(rclass_bitmap, WLC_REGCLASS_GLOBAL_2G_20MHZ_CH14)) {

		global_opclass_support |= (0X01 << WLC_BAND_2G);
	}
#ifdef BAND5G
	rcinfo[0] =  &rcinfo_global_center_160;
	rcinfo[1] =  &rcinfo_global_center_80;
	rcinfo[2] =  &rcinfo_global_40upper;
	rcinfo[3] =  &rcinfo_global_40lower;
	rcinfo[4] =  &rcinfo_global_20;

	if ((wlc_chk_rclass_support_5g(rcinfo[0], rclass_bitmap)) ||
		(wlc_chk_rclass_support_5g(rcinfo[1], rclass_bitmap)) ||
		(wlc_chk_rclass_support_5g(rcinfo[2], rclass_bitmap)) ||
		(wlc_chk_rclass_support_5g(rcinfo[3], rclass_bitmap)) ||
		(wlc_chk_rclass_support_5g(rcinfo[4], rclass_bitmap))) {

		global_opclass_support |= (0X01 << WLC_BAND_5G);
	}
	return global_opclass_support;
#endif /* BAND5G */
}

/* query the channel list given a country and a regulatory class */
/* XXX The result list 'list' must be large enough to hold all possible channels available
 * recommend to allocate MAXCHANNEL worth of elements
 */
static bool
wlc_chk_rclass_support_5g(const rcinfo_t *rcinfo, uint8 *setBitBuff)
{
	int count = 0;
	int i = 0;
	bool rclass_support = FALSE;

	if (!rcinfo) {
		return rclass_support;
	}

	/* check for 5G band */
	count = rcinfo->len;
	for (i = 0; i < count; i++) {
		if (isset(setBitBuff, rcinfo->rctbl[i].rclass)) {
			rclass_support = TRUE;
			break;
		}
	}
	return rclass_support;
}
int
wlc_channel_get_cur_rclass(struct wlc_info *wlc)
{
	return wlc->cmi->cm->cur_rclass;
}

/* Set Only BCMWIFI_RCLASS_TYPE_GBL or BCMWIFI_RCLASS_TYPE_NONE,
 * rest country specific rclass values are not required
 */
void
wlc_channel_set_cur_rclass(struct wlc_info *wlc, uint8 cur_rclass)
{
	wlc->cmi->cm->cur_rclass = cur_rclass;
}
#endif /* WL_GLOBAL_RCLASS */
void
wlc_update_rcinfo(wlc_cm_info_t *wlc_cmi, bool switch_to_global_opclass)
{
	wlc_cm_data_t *wlc_cm = wlc_cmi->cm;
	wlc_pub_t *pub = wlc_cmi->pub;
	BCM_REFERENCE(pub);

	if (switch_to_global_opclass) {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_global_20;
#endif // endif
#ifdef WL11N
		if (N_ENAB(pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_global_40lower;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_global_40upper;
		}
#endif // endif
#ifdef WL11AC
		if (VHT_ENAB(pub)) {
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_80] = &rcinfo_global_center_80;
#ifdef WL11AC_160
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_160] = &rcinfo_global_center_160;
#endif // endif
		}
#endif /* WL11AC */
	} else if (wlc_us_ccode(wlc_cm->country_abbrev)) {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_us_20;
#endif // endif
#ifdef WL11N
		if (N_ENAB(pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_us_40lower;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_us_40upper;
		}
#endif // endif
#ifdef WL11AC
		if (VHT_ENAB(pub)) {
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_80] = &rcinfo_us_center_80;
#ifdef WL11AC_160
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_160] = &rcinfo_us_center_160;
#endif // endif
		}
#endif /* WL11AC */
	} else if (wlc_japan_ccode(wlc_cm->country_abbrev)) {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_jp_20;
#endif // endif
#ifdef WL11N
		if (N_ENAB(pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_jp_40;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_jp_40;
		}
#endif // endif
#ifdef WL11AC
		if (VHT_ENAB(pub)) {
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_80] = &rcinfo_jp_center_80;
#ifdef WL11AC_160
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_160] = &rcinfo_jp_center_160;
#endif // endif
		}
#endif /* WL11AC */
	} else {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_eu_20;
#endif // endif
#ifdef WL11N
		if (N_ENAB(pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_eu_40lower;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_eu_40upper;
		}
#endif // endif
#ifdef WL11AC
		if (VHT_ENAB(pub)) {
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_80] = &rcinfo_eu_center_80;
#ifdef WL11AC_160
			wlc_cm->rcinfo_list_11ac[WLC_RCLIST_160] = &rcinfo_eu_center_160;
#endif // endif
		}
#endif /* WL11AC */
	}
	wlc_regclass_vec_init(wlc_cmi);
}

uint8 *
wlc_write_wide_bw_chan_ie(chanspec_t chspec, uint8 *cp, int buflen)
{
	dot11_wide_bw_chan_ie_t *wide_bw_chan_ie;
	uint8 center_chan;

	/* perform buffer length check. */
	/* if not big enough, return buffer untouched */
	BUFLEN_CHECK_AND_RETURN((TLV_HDR_LEN + DOT11_WIDE_BW_IE_LEN), buflen, cp);

	wide_bw_chan_ie = (dot11_wide_bw_chan_ie_t *) cp;
	wide_bw_chan_ie->id = DOT11_NGBR_WIDE_BW_CHAN_SE_ID;
	wide_bw_chan_ie->len = DOT11_WIDE_BW_IE_LEN;

	if (CHSPEC_IS40(chspec))
		wide_bw_chan_ie->channel_width = WIDE_BW_CHAN_WIDTH_40;
	else if (CHSPEC_IS80(chspec))
		wide_bw_chan_ie->channel_width = WIDE_BW_CHAN_WIDTH_80;
	else if (CHSPEC_IS160(chspec))
		wide_bw_chan_ie->channel_width = WIDE_BW_CHAN_WIDTH_160;
	else if (CHSPEC_IS8080(chspec))
		wide_bw_chan_ie->channel_width = WIDE_BW_CHAN_WIDTH_80_80;
	else
		wide_bw_chan_ie->channel_width = WIDE_BW_CHAN_WIDTH_20;

	if (CHSPEC_IS8080(chspec)) {
		wide_bw_chan_ie->center_frequency_segment_0 =
			wf_chspec_primary80_channel(chspec);
		wide_bw_chan_ie->center_frequency_segment_1 =
			wf_chspec_secondary80_channel(chspec);
	}
	else {
		center_chan = CHSPEC_CHANNEL(chspec) >> WL_CHANSPEC_CHAN_SHIFT;
		wide_bw_chan_ie->center_frequency_segment_0 = center_chan;
		wide_bw_chan_ie->center_frequency_segment_1 = 0;
	}

	cp += (TLV_HDR_LEN + DOT11_WIDE_BW_IE_LEN);

	return cp;
}
