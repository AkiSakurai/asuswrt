/*
 * CLM API functions.
 * $ Copyright Broadcom $
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_clm.c 823386 2019-06-04 00:14:32Z fs936724 $
 */

/* Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags through this file.
 */
#include <wlc_cfg.h>

#include <bcmwifi_rates.h>
#include <typedefs.h>


#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include "wlc_clm.h"
#include "wlc_clm_data.h"

/******************************
* MODULE MACROS AND CONSTANTS *
*******************************
*/

/* BLOB format version major number */
#define FORMAT_VERSION_MAJOR 24

/* BLOB format version minor number */
#define FORMAT_VERSION_MINOR 0

/* Minimum supported binary BLOB format's major version */
#define FORMAT_MIN_COMPAT_MAJOR 7

#if (FORMAT_VERSION_MAJOR != CLM_FORMAT_VERSION_MAJOR) || \
		(FORMAT_VERSION_MINOR != CLM_FORMAT_VERSION_MINOR)
#error CLM data format version mismatch between wlc_clm.c and wlc_clm_data.h
#endif

#ifndef NULL
	/** Null pointer */
	#define NULL 0
#endif

/** Boolean type definition for use inside this module */
typedef int MY_BOOL;

#ifndef TRUE
	/** TRUE for MY_BOOL */
	#define TRUE 1
#endif

#ifndef FALSE
	/** FALSE for MY_BOOL */
	#define FALSE 0
#endif

#ifndef OFFSETOF
	/** Offset of given field in given structure */
#ifdef _WIN64
	#define OFFSETOF(s, m) (unsigned long long)&(((s *)0)->m)
#else
	#define OFFSETOF(s, m) (unsigned long)&(((s *)0)->m)
#endif /* _WIN64 */
#endif /* OFFSETOF */

#ifndef ARRAYSIZE
	/** Number of elements in given array */
	#define ARRAYSIZE(x) (unsigned)(sizeof(x)/sizeof(x[0]))
#endif

#if WL_NUMRATES >= 178
	/** Defined if bcmwifi_rates.h contains TXBF rates */
	#define CLM_TXBF_RATES_SUPPORTED
#endif

#ifndef BCMRAMFN
	#define BCMRAMFN(x) x
#endif /* BCMRAMFN */

/** CLM data source IDs */
typedef enum data_source_id {
	/** Incremental CLM data. Placed first so we look there before base
	 * data
	 */
	DS_INC,

	/** Main CLM data */
	DS_MAIN,

	/** Number of CLM data source IDs */
	DS_NUM
} data_source_id_t;

/** Indices in base_ht_loc_data[] vector used by some function and containing
 * data pertinent to base and HT locales
 */
typedef enum base_ht_id {
	/** Index for base locale */
	BH_BASE,

	/** Index for HT locale */
	BH_HT,

	/** Number of indices (length of base_ht_loc_data vector) */
	BH_NUM
} base_ht_id_t;

/** Module constants */
enum clm_internal_const {
	/** MASKS THAT DEFINE ITERATOR CONTENTS */

	/** Pointed data is in main CLM data source */
	ITER_DS_MAIN = 0x40000000,

	/** Pointed data is in incremental CLM data source */
	ITER_DS_INC = 0x00000000,

	/** Mask of data source field of iterator */
	ITER_DS_MASK = 0x40000000,

	/** Mask of index field of iterator */
	ITER_IDX_MASK = 0x3FFFFFFF,

	/** Number of MCS/OFDM rates, differing only by modulation */
	NUM_MCS_MODS = 8,

	/** Number of DSSS rates, differing only by modulation */
	NUM_DSSS_MODS = 4,

	/** Channel number stride at 20MHz */
	CHAN_STRIDE = 4,

	/** Mask of count field in subchannel path descriptor */
	SUB_CHAN_PATH_COUNT_MASK = 0xF0,

	/** Offset of count field in subchannel path descriptor */
	SUB_CHAN_PATH_COUNT_OFFSET = 4,

	/** Prefill constant for power limits used in clm_limits() */
	UNSPECIFIED_POWER = CLM_DISABLED_POWER + 1,

	/** clm_country_locales::computed_flags: country flags taken from main
	 * data
	 */
	COUNTRY_FLAGS_DS_MAIN = (unsigned char)DS_MAIN,

	/** clm_country_locales::computed_flags: country flags taken from
	 * incremental data
	 */
	COUNTRY_FLAGS_DS_INC = (unsigned char)DS_INC,

	/** clm_country_locales::computed_flags: mask for country flags source
	 * field
	 */
	COUNTRY_FLAGS_DS_MASK = (unsigned char)(DS_NUM - 1),

#if WL_NUMRATES >= 336
	/** Base value for rates in extended 4TX rate set */
	BASE_EXT4_RATE = WL_RATE_1X4_DSSS_1,
#endif /* WL_NUMRATES >= 336 */

	/** Base value for rates in extended rate set */
	BASE_EXT_RATE = WL_RATE_1X3_DSSS_1,

	/** Shift value for hash function that computes index of TX mode for
	 * HE0 rates
	 */
	SU_TX_MODE_HASH_SHIFT = 4,

	/** Mask valie for hash function that computes index of TX mode index
	 * for HE0 rates
	 */
	SU_TX_MODE_HASH_MASK = 0x3F
};

/** Rate types */
enum clm_rate_type {
	/** DSSS (802.11b) rate */
	RT_DSSS,

	/** OFDM (802.11a/g) rate */
	RT_OFDM,

	/** MCS (802.11n/ac) rate */
	RT_MCS,

	/** HE (802.11ax) SU rate */
	RT_SU
};

/** RU rate traits with respect to channel bandwidth (elements of ru_rate_bw[])
 */
typedef enum clm_ru_rate_bw {
	/** UL with minimum bandwidth of 20MHz */
	RRB_20,
	/** UL with minimum bandwidth of 40MHz */
	RRB_40,
	/** UL with minimum bandwidth of 80MHz */
	RRB_80,
	/** DL  */
	RRB_DL
} clm_ru_rate_bw_t;

/** Format of CC/rev representation in aggregations and advertisings */
typedef enum clm_ccrev_format {
	/** As clm_cc_rev_t */
	CCREV_FORMAT_CC_REV,

	/** As 8-bit index to region table */
	CCREV_FORMAT_CC_IDX8,

	/** As 16-bit index to region table */
	CCREV_FORMAT_CC_IDX16
} clm_ccrev_format_t;

/** Internal type for regrevs */
typedef unsigned short regrev_t;

/** CLM data set descriptor */
typedef struct data_dsc {
	/** Pointer to registry (TOC) structure of CLM data */
	const clm_registry_t *data;

	/** Relocation factor of CLM DATA set: value that shall be added to
	 * pointer contained in CLM data set to get a true pointer (e.g. 0 if
	 * data is not relocated)
	 */
	uintptr relocation;

	/** Valid channel comb sets (per band, per bandwidth). Empty for
	 * 80+80
	 */
	clm_channel_comb_set_t valid_channel_combs[CLM_BAND_NUM][CLM_BW_NUM];

	/** 40+MHz combs stored in BLOB - obsoleted */
	MY_BOOL has_high_bw_combs_obsoleted;

	/** Index within clm_country_rev_definition_cd10_t::extra of byte with
	 * bits 9-16 of rev. -1 for 8-bit revs
	 */
	int reg_rev16_idx;

	/** Length of region record in bytes */
	int country_rev_rec_len;

	/** Address of header for version strings */
	const clm_data_header_t *header;

	/** True if BLOB capable of containing 160MHz data - obsoleted */
	MY_BOOL has_160mhz_obsoleted;

	/** Obsoleted */
	const clm_channel_range_t *chan_ranges_bw_obsoleted[CLM_BW_NUM];

	/** Per-band-bandwidth base addresses of rate set definitions */
	const unsigned char *rate_sets_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** Index within clm_country_rev_definition_cdXX_t::extra of byte with
	 * bits 9-10 of locale index. -1 for 8-bit locale indices
	 */
	int reg_loc10_idx;

	/** Index within clm_country_rev_definition_cdXX_t::extra of byte with
	 * bits 11-12 of locale index. -1 for 8 and 10 bit locale indices
	 */
	int reg_loc12_idx;

	/** Index within clm_country_rev_definition_cdXX_t::extra of byte with
	 * region flags. -1 if region has no flags
	 */
	int reg_flags_idx;

	/** CC/revs representation in aggregations */
	clm_ccrev_format_t ccrev_format;

	/** Per-band-bandwidth base addresses of extended rate set definitions */
	const unsigned char *ext_rate_sets_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** True if BLOB contains ULB channels - obsoleted */
	MY_BOOL has_ulb_obsoleted;

	/** If BLOB has regrev remaps - pointer to remap set structure */
	const clm_regrev_cc_remap_set_t *regrev_remap;

	/** 'flags' from clm_data_registry or 0 if there is no flags there */
	unsigned int registry_flags;

	/** Index within clm_country_rev_definition_cdXX_t::extra second of
	 * byte with region flags. -1 if region has no second byte of flags
	 */
	int reg_flags_2_idx;

	/** 4-bit subchannel index (for backward compatibility) */
	MY_BOOL scr_idx_4;

	/** 'flags2' from clm_data_registry or 0 if there is no flags there */
	unsigned int registry_flags2;

	/** Index within clm_country_rev_definition_cdXX_t::extra of byte with
	* bits 13-14 of locale index. -1 for 8, 10 and 12 bit locale indices
	*/
	int reg_loc14_idx;

	/** Obsoleted */
	const unsigned char *he_rate_sets_obsoleted;

	/** Descriptors of HE rates, used in BLOB */
	const clm_he_rate_dsc_t *he_rate_dscs;

	/** Per-band-bandwidth base addresses of extended 4TX rate set
	 * definitions
	 */
	const unsigned char *ext4_rate_sets_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** Per-band-bandwidth base addresses of OFDMA rate set definitions */
	const unsigned char *he_rate_sets_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** Per-band-bandwidth base addresses of channel range descriptors */
	const clm_channel_range_t *chan_ranges_band_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** Index within clm_country_rev_definition_cdXX_t::extra of byte with
	 * bits 9-12 of 6GHz locale indices. -1 for 8 bit locale indices
	 */
	int reg_loc12_6g_idx;

	/** Index within clm_country_rev_definition_cdXX_t::extra of byte with
	 * bits 13-16 of 6GHz locale indices. -1 for 8 and 12 bit locale indices
	 */
	int reg_loc16_6g_idx;

	/** Mask that covers valid bits in locale index - used for
	 * interpretation of special indices (CLM_LOC_... constants, that are
	 * truncated in BLOB)
	 */
	int loc_idx_mask;

	/** Per-band-bandwidth indices of rate set definitions */
	const unsigned short *rate_sets_indices_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** Per-band-bandwidth indices of ext rate set definitions */
	const unsigned short *ext_rate_sets_indices_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** Per-band-bandwidth indices of ext4 rate set definitions */
	const unsigned short *ext4_rate_sets_indices_bw[CLM_BAND_NUM][CLM_BW_NUM];

	/** Per-band-bandwidth indices of HE/RU rate set definitions */
	const unsigned short *he_rate_sets_indices_bw[CLM_BAND_NUM][CLM_BW_NUM];
} data_dsc_t;

/** Addresses of locale-related data */
typedef struct locale_data {
	/** Locale definition */
	const unsigned char *def_ptr;

	/** Per-bandwidth base addresses of channel range descriptors */
	const clm_channel_range_t * const *chan_ranges_bw;

	/** Per-bandwidth base addresses of rate set definitions */
	const unsigned char * const *rate_sets_bw;

	/** Base address of valid channel sets definitions */
	const unsigned char *valid_channels;

	/** Base address of restricted sets definitions */
	const unsigned char *restricted_channels;

	/** Per-bandwidth channel combs */
	const clm_channel_comb_set_t *combs[CLM_BW_NUM];

	/** 80MHz subchannel rules - obsoleted */
	clm_sub_chan_region_rules_80_t sub_chan_channel_rules_80_obsoleted;

	/** 160MHz subchannel rules - obsoleted */
	clm_sub_chan_region_rules_160_t sub_chan_channel_rules_160_obsoleted;

	/** Per-bandwidth base addresses of extended rate set definitions */
	const unsigned char * const *ext_rate_sets_bw;

	/** Obsoleted */
	const unsigned char *he_rate_sets;

	/** Per-bandwidth base addresses of extended 4TX rate set definitions */
	const unsigned char * const *ext4_rate_sets_bw;

	/** Per-bandwidth base addresses of OFDMA rate set definitions */
	const unsigned char * const *he_rate_sets_bw;

	/** Per-bandwidth indices of rate set definitions */
	const unsigned short * const *rate_sets_indices_bw;

	/** Per-bandwidth indices of ext rate set definitions */
	const unsigned short * const *ext_rate_sets_indices_bw;

	/** Per-bandwidth indices of ext4 rate set definitions */
	const unsigned short * const *ext4_rate_sets_indices_bw;

	/** Per-bandwidth indices of HE/RU rate set definitions */
	const unsigned short * const *he_rate_sets_indices_bw;
} locale_data_t;

/** Addresses of region (country)-related data. Replaces previous, completely
 * obsoleted layout
 */
typedef struct country_data_v2 {
	/** 40MHz subchannel rules */
	clm_sub_chan_region_rules_t sub_chan_channel_rules_40[CLM_BAND_NUM];

	/** 80MHz subchannel rules */
	clm_sub_chan_region_rules_t sub_chan_channel_rules_80[CLM_BAND_NUM];

	/** 160MHz subchannel rules */
	clm_sub_chan_region_rules_t sub_chan_channel_rules_160[CLM_BAND_NUM];

	/** Power increments (offsets) for 40MHz subchannel rules (or NULL) */
	const signed char *sub_chan_increments_40[CLM_BAND_NUM];

	/** Power increments (offsets) for 80MHz subchannel rules (or NULL) */
	const signed char *sub_chan_increments_80[CLM_BAND_NUM];

	/** Power increments (offsets) for 160MHz subchannel rules (or NULL) */
	const signed char *sub_chan_increments_160[CLM_BAND_NUM];

	/** Per-band-bandwidth base addresses of channel range descriptors.
	 * References correspondent field in data_dsc_t. Used in subchannel
	 * rules' computation where, in case of incremental BLOB use, locale
	 * subchannel ranges can't be used (as they may come from different
	 * data source)
	 */
	const clm_channel_range_t *(*chan_ranges_band_bw)[CLM_BW_NUM];
} country_data_v2_t;

/** Information about aggregation */
typedef struct aggregate_data {
	/** Default region */
	clm_cc_rev4_t def_reg;

	/** Number of regions */
	int num_regions;

	/** Pointer to vector of regions in BLOB-specific format */
	const void *regions;
} aggregate_data_t;

/** Locale type descriptor */
typedef struct loc_type_dsc {
	/** Band */
	clm_band_t band;

	/* Locale flavor (base/HT) */
	base_ht_id_t flavor;

	/** Offset of locale definition field in clm_country_locales_t */
	unsigned int def_field_offset;

	/** Offset of locales' definition field in clm_registry_t */
	unsigned int loc_def_field_offset;
} loc_type_dsc_t;

/** Descriptors of main and incremental data sources */
static data_dsc_t data_sources[] = {
	{ NULL, 0, {{{0, 0}}}, 0, 0, 0, NULL, 0, {NULL}, {{NULL}}, 0, 0, 0,
	(clm_ccrev_format_t)0, {{NULL}}, 0, NULL, 0, 0, 0, 0, 0, NULL, NULL,
	{{NULL}}, {{NULL}}, {{NULL}}, 0, 0, 0, {{NULL}}, {{NULL}}, {{NULL}},
	{{NULL}} },
	{ NULL, 0, {{{0, 0}}}, 0, 0, 0, NULL, 0, {NULL}, {{NULL}}, 0, 0, 0,
	(clm_ccrev_format_t)0, {{NULL}}, 0, NULL, 0, 0, 0, 0, 0, NULL, NULL,
	{{NULL}}, {{NULL}}, {{NULL}}, 0, 0, 0, {{NULL}}, {{NULL}}, {{NULL}},
	{{NULL}} }
};


/** Rate type by rate index. Values are from enum clm_rate_type, compressed to
 * 2-bits
 */
static unsigned char rate_type[(WL_NUMRATES + 3)/4];

/* Gives rate type for given rate index. Use of named constants make this
 * expression too ugly
 */
#define RATE_TYPE(rate_idx)	\
	((get_rate_type()[(unsigned)(rate_idx) >> 2] >> (((rate_idx) & 3) << 1)) & 3)

/* Sets rate type for given rate index */
#define SET_RATE_TYPE(rate_idx, rt) \
	get_rate_type()[(unsigned)(rate_idx) >> 2] &= ~(3 << (((rate_idx) & 3) << 1)); \
	get_rate_type()[(unsigned)(rate_idx) >> 2] |= rt << (((rate_idx) & 3) << 1)


/** Valid 40M channels of 2.4G band */
static const struct clm_channel_comb valid_channel_combs_2g_40m[] = {
	{  3,  11, 1}, /* 3 - 11 with step of 1 */
};

/** Set of 40M 2.4G valid channel combs */
static const struct clm_channel_comb_set valid_channel_2g_40m_set = {
	1, valid_channel_combs_2g_40m
};

/** Valid 40M channels of 5G band */
static const struct clm_channel_comb valid_channel_combs_5g_40m[] = {
	{ 38,  62, 8}, /* 38 - 62 with step of 8 */
	{102, 142, 8}, /* 102 - 142 with step of 8 */
	{151, 159, 8}, /* 151 - 159 with step of 8 */
};

/** Set of 40M 5G valid channel combs */
static const struct clm_channel_comb_set valid_channel_5g_40m_set = {
	3, valid_channel_combs_5g_40m
};

/** Valid 80M channels of 5G band */
static const struct clm_channel_comb valid_channel_combs_5g_80m[] = {
	{ 42,  58, 16}, /* 42 - 58 with step of 16 */
	{106, 138, 16}, /* 106 - 138 with step of 16 */
	{155, 155, 16}, /* 155 - 155 with step of 16 */
};

/** Set of 80M 5G valid channel combs */
static const struct clm_channel_comb_set valid_channel_5g_80m_set = {
	3, valid_channel_combs_5g_80m
};

/** Valid 160M channels of 5G band */
static const struct clm_channel_comb valid_channel_combs_5g_160m[] = {
	{ 50,  50, 32}, /* 50 - 50 with step of 32 */
	{114, 114, 32}, /* 114 - 114 with step of 32 */
};

/** Set of 160M 5G valid channel combs */
static const struct clm_channel_comb_set valid_channel_5g_160m_set = {
	2, valid_channel_combs_5g_160m
};

/* This is a hack to accommodate PHOENIX2 builds which don't know about the VHT
 * rates
 */
#ifndef WL_RATESET_SZ_VHT_MCS
#define CLM_NO_VHT_RATES
#endif


/** Maps CLM_DATA_FLAG_WIDTH_...  to clm_bandwidth_t */
static const unsigned char bw_width_to_idx_ac[CLM_DATA_FLAG_WIDTH_MASK + 1] = {
	CLM_BW_20, CLM_BW_40, 0, 0, 0, 0, 0, 0, CLM_BW_80, CLM_BW_160,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, CLM_BW_80_80, 0};

static const unsigned char* bw_width_to_idx;

#if defined(WL_RU_NUMRATES)
/** SU base (modulation index 0) rate indices in same order, as in
 * he_rate_descriptors[] and in clm_ru_rates_t for a particular rate type
 */
static const unsigned int su_base_rates[] = {
	WL_RATE_1X1_HE0SS1, WL_RATE_1X2_HE0SS1, WL_RATE_2X2_HE0SS2, WL_RATE_1X2_TXBF_HE0SS1,
	WL_RATE_2X2_TXBF_HE0SS2, WL_RATE_1X3_HE0SS1, WL_RATE_2X3_HE0SS2, WL_RATE_3X3_HE0SS3,
	WL_RATE_1X3_TXBF_HE0SS1, WL_RATE_2X3_TXBF_HE0SS2, WL_RATE_3X3_TXBF_HE0SS3,
	WL_RATE_1X4_HE0SS1, WL_RATE_2X4_HE0SS2, WL_RATE_3X4_HE0SS3, WL_RATE_4X4_HE0SS4,
	WL_RATE_1X4_TXBF_HE0SS1, WL_RATE_2X4_TXBF_HE0SS2, WL_RATE_3X4_TXBF_HE0SS3,
	WL_RATE_4X4_TXBF_HE0SS4,
};

/** Hash table for computation that determines HE0 rate's TX mode index - i.e.
 * that specifies mapping inverse to specified by su_base_rates[]
 */
static unsigned char he_tx_mode_hash[SU_TX_MODE_HASH_MASK + 1];

/** Traits of RU rates with respect to bandwidth. 2 bits per rate, values are
 * RRB_... constants
 */
static unsigned char ru_rate_bw[(WL_RU_NUMRATES + 3) / 4];

/** Properties of various transmission modes for a particular rate type.
 * Descriptors follow in same order as items in su_base_rates[] and in
 * clm_ru_rates_t for a particular rate type. Items have zero 'rate_type'
 * field, because they described all HE rate types
 */
static const clm_he_rate_dsc_t he_rate_descriptors[] = {
	{0, 1, 1, 0},				/* 1X1 */
	{0, 1, 2, 0},				/* 1X2 */
	{0, 2, 2, 0},				/* 2X2 */
	{0, 1, 2, CLM_HE_RATE_FLAG_TXBF},	/* 1X2_TXBF */
	{0, 2, 2, CLM_HE_RATE_FLAG_TXBF},	/* 2X2_TXBF */
	{0, 1, 3, 0},				/* 1X3 */
	{0, 2, 3, 0},				/* 2X3 */
	{0, 3, 3, 0},				/* 3X3 */
	{0, 1, 3, CLM_HE_RATE_FLAG_TXBF},	/* 1X3_TXBF */
	{0, 2, 3, CLM_HE_RATE_FLAG_TXBF},	/* 2X3_TXBF */
	{0, 3, 3, CLM_HE_RATE_FLAG_TXBF},	/* 3X3_TXBF */
	{0, 1, 4, 0},				/* 1X4 */
	{0, 2, 4, 0},				/* 2X4 */
	{0, 3, 4, 0},				/* 3X4 */
	{0, 4, 4, 0},				/* 4X4 */
	{0, 1, 4, CLM_HE_RATE_FLAG_TXBF},	/* 1X4_TXBF */
	{0, 2, 4, CLM_HE_RATE_FLAG_TXBF},	/* 2X4_TXBF */
	{0, 3, 4, CLM_HE_RATE_FLAG_TXBF},	/* 3X4_TXBF */
	{0, 4, 4, CLM_HE_RATE_FLAG_TXBF},	/* 4X4_TXBF */
};
#elif defined(WL_NUM_HE_RT)
/** Maps shifted CLM_DATA_FLAG2_OUTER_BW_... constanst to bandwidths */
static const clm_bandwidth_t outer_bw_to_bw[] = {
	CLM_BW_NUM, CLM_BW_40,
	CLM_BW_80, CLM_BW_160
};

/** RU rate types that corresponds to HE on various bandwidths */
static const wl_he_rate_type_t he_bw_to_ru[] = {
	WL_HE_RT_RU242, WL_HE_RT_RU484,
	WL_HE_RT_RU996, (wl_he_rate_type_t)WL_NUM_HE_RT
};

/** Maps WL_HE_RT_... constants to their minimum bandwidths. UB/LUB mapped to
 * CLM_BW_NUM to designate that they are not allowed on subchannels
 */
static const clm_bandwidth_t min_ru_bw[] = {
	CLM_BW_20, CLM_BW_20, CLM_BW_20, CLM_BW_20, CLM_BW_NUM, CLM_BW_NUM, CLM_BW_20, CLM_BW_40,
	CLM_BW_80
};
#endif /* WL_NUM_HE_RT */

/** Maps limit types to descriptors of paths from main channel to correspondent
 * subchannel. Each descriptor is a byte. High nibble contains number of steps
 * in path, bits in low nibble describe steps. 0 - select lower subchannel,
 * 1 - select upper subchannel. Least significant bit corresponds to last step
 */
static const unsigned char subchan_paths[] = {
	0x00, /* CHANNEL */
	0x10, /* SUBCHAN_L */
	0x11, /* SUBCHAN_U */
	0x20, /* SUBCHAN_LL */
	0x21, /* SUBCHAN_LU */
	0x22, /* SUBCHAN_UL */
	0x23, /* SUBCHAN_UU */
	0x30, /* SUBCHAN_LLL */
	0x31, /* SUBCHAN_LLU */
	0x32, /* SUBCHAN_LUL */
	0x33, /* SUBCHAN_LUU */
	0x34, /* SUBCHAN_ULL */
	0x35, /* SUBCHAN_ULU */
	0x36, /* SUBCHAN_UUL */
	0x37, /* SUBCHAN_UUU */
};

/** Offsets of per-antenna power limits inside TX power record */
static const int antenna_power_offsets[] = {
	CLM_LOC_DSC_POWER_IDX, CLM_LOC_DSC_POWER1_IDX, CLM_LOC_DSC_POWER2_IDX,
	CLM_LOC_DSC_POWER3_IDX
};

/** Locale type descriptors, ordered by locale type indices
 * (CLM_LOC_IDX_BASE_... constants)
 */
static const loc_type_dsc_t loc_type_dscs[] = {
	{CLM_BAND_2G, BH_BASE, OFFSETOF(clm_country_locales_t, locale_2G),
	OFFSETOF(clm_registry_t, locales[CLM_LOC_IDX_BASE_2G])},
	{CLM_BAND_5G, BH_BASE, OFFSETOF(clm_country_locales_t, locale_5G),
	OFFSETOF(clm_registry_t, locales[CLM_LOC_IDX_BASE_5G])},
	{CLM_BAND_2G, BH_HT, OFFSETOF(clm_country_locales_t, locale_2G_HT),
	OFFSETOF(clm_registry_t, locales[CLM_LOC_IDX_HT_2G])},
	{CLM_BAND_5G, BH_HT, OFFSETOF(clm_country_locales_t, locale_5G_HT),
	OFFSETOF(clm_registry_t, locales[CLM_LOC_IDX_HT_5G])},
#ifdef WL_BAND6G
	{CLM_BAND_6G, BH_BASE, OFFSETOF(clm_country_locales_t, locale_6G),
	OFFSETOF(clm_registry_t, locales_6g_base)},
	{CLM_BAND_6G, BH_HT, OFFSETOF(clm_country_locales_t, locale_6G_HT),
	OFFSETOF(clm_registry_t, locales_6g_ht)},
#endif /* WL_BAND6G */
};

/** Composes locale type indices (CLM_LOC_IDX_BASE_... constants) out of band
 * and flavor
 */
static const unsigned int compose_loc_type[CLM_BAND_NUM][BH_NUM] = {
	{CLM_LOC_IDX_BASE_2G, CLM_LOC_IDX_HT_2G},
	{CLM_LOC_IDX_BASE_5G, CLM_LOC_IDX_HT_5G},
#ifdef WL_BAND6G
	{CLM_LOC_IDX_BASE_6G, CLM_LOC_IDX_HT_6G},
#endif /* WL_BAND6G */
};


/****************************
* MODULE INTERNAL FUNCTIONS *
*****************************
*/

/** Removes bits set in mask from value, shifting right
 * \param[in] value Value to remove bits from
 * \param[in] mask Mask with bits to remove
 * Returns value with bits in mask removed
 */
static unsigned int
remove_extra_bits(unsigned int value, unsigned int mask)
{
	while (mask) {
		/* m has mask's lowest set bit and all lower bits set */
		unsigned int m = mask ^ (mask - 1);
		/* Clearing mask's lowest 1 bit */
		mask &= ~m;
		/* m has bits to left of former mask's lowest set bit set */
		m >>= 1;
		/* Removing mask's (former) lowest set bit from value */
		value = (value & m) | ((value >> 1) & ~m);
		/* Removing mask's (former) lowest set bit from mask */
		mask >>= 1;
	}
	return value;
}

/** Returns value of field with given name in given (main of incremental) CLM
 * data set. Interface to get_data that converts field name to offset of field
 * inside CLM data registry
 * \param[in] ds_id CLM data set identifier
 * \param[in] field Name of field in struct clm_registry
 * \return Field value as (const void *) pointer. NULL if given data source was
 * not set
 */
#define GET_DATA(ds_id, field) get_data(ds_id, OFFSETOF(clm_registry_t, field))

/* Accessor function to avoid data_sources structure from getting into ROM.
 * Don't have this function in ROM.
 */
static data_dsc_t *
BCMRAMFN(get_data_sources)(int ds_idx)
{
	return &data_sources[ds_idx];
}

/* Accessor function to avoid rate_type structure from getting into ROM.
 * Don't have this function in ROM.
 */
static unsigned char *
BCMRAMFN(get_rate_type)(void)
{
	return rate_type;
}

#ifdef WL_RU_NUMRATES
/** Returns TX mode index for given HE0 rate
 * \param[in] he0_rate HE0 rate index from clm_rates_t
 * \return TX mode of this rate (its index in su_base_rates)
 */
static unsigned int
BCMRAMFN(get_he_tx_mode)(unsigned int he0_rate)
{
	return he_tx_mode_hash[(he0_rate >> SU_TX_MODE_HASH_SHIFT) & SU_TX_MODE_HASH_MASK];
}

/** Sets TX mode for given HE0 rate in he_tx_mode_hash
 * \param[in] he0_rate HE0 rate index from clm_rates_t
 * \param[in] mode Mode (index of this rate in su_base_rates)
 */
static void
BCMRAMFN(set_he_tx_mode)(unsigned int he0_rate, unsigned int mode)
{
	he_tx_mode_hash[(he0_rate >> SU_TX_MODE_HASH_SHIFT) & SU_TX_MODE_HASH_MASK] =
		(unsigned char)mode;
}

/** Returns minimum bandwidth for given RU rate, BW_NUM for non-RU OFDMA rates */
static clm_bandwidth_t
BCMRAMFN(get_ru_rate_min_bw)(unsigned int ru_rate)
{
	static const clm_bandwidth_t rrb_to_bw[] = {
		CLM_BW_20, CLM_BW_40,
		CLM_BW_80,
		CLM_BW_NUM
	};
	return rrb_to_bw[(ru_rate_bw[ru_rate >> 2] >> (((ru_rate) & 3) << 1)) & 3];
}

/** Sets bandwidth trate (RRB_... constant) for given RU rate
 * \param[in] ru_rate RU rate index (from clm_ru_rates_t)
 * \param[in] rrb RRB_... constant corresponding to given RU rate
 */
static void
BCMRAMFN(set_ru_rate_bw_type)(unsigned int ru_rate, clm_ru_rate_bw_t rrb)
{
	ru_rate_bw[ru_rate >> 2] &= (unsigned char)(~(3 << (((ru_rate) & 3) << 1)));
	ru_rate_bw[ru_rate >> 2] |= (unsigned char)(rrb << (((ru_rate) & 3) << 1));
}
#endif /* WL_RU_NUMRATES */

/** Returns value of field with given offset in given (main or incremental) CLM
 * data set
 * \param[in] ds_idx CLM data set identifier
 * \param[in] field_offset Offset of field in struct clm_registry
 * \return Field value as (const void *) pointer. NULL if given data source was
 * not set
 */
static const void *
get_data(int ds_idx, unsigned long field_offset)
{
	data_dsc_t *ds = get_data_sources(ds_idx);
	const uintptr paddr = (uintptr)ds->data;
	const char **pp = (const char **)(paddr + field_offset);

	return (ds->data && *pp) ? (*pp + ds->relocation) : NULL;
}

/** Converts given pointer value, fetched from some (possibly relocated) CLM
 * data structure to its 'true' value
 * Note that GET_DATA() return values are already converted to 'true' values
 * \param[in] ds_idx Identifier of CLM data set that contained given pointer
 * \param[in] ptr Pointer to convert
 * \return 'True' (unrelocated) pointer value
 */
static const void *
relocate_ptr(int ds_idx, const void *ptr)
{
	return ptr ? ((const char *)ptr + get_data_sources(ds_idx)->relocation) : NULL;
}

/** Returns address of data item, stored in one of ..._set_t structures
 * \param[in] ds_idx Identifier of CLM data set
 * \param[in] field_offset Offset of address of set field in clm_registry_t
 * \param[in] num_offset Offset of 'num' field in ..._set_t structure
 * \param[in] set_offset Offset of 'set' field in ..._set_t structure
 * \param[in] rec_len Length of record referenced by 'set' field
 * \param[in] idx Index of requested field
 * \param[out] num Optional output parameter - value of 'num' field of ..._set
    structure. -1 if structure not found
 * \return Address of idx's field of vector, referenced by set field or NULL
 */
static const void *
get_item(int ds_idx, unsigned long field_offset, unsigned long num_offset,
	unsigned long set_offset, int rec_len, int idx, int *num)
{
	const void *data = get_data(ds_idx, field_offset);
	if (data) {
		int n = *(const int *)((const char *)data + num_offset);
		if (num) {
			*num = n;
		}
		return (idx < n)
				? ((const char *)relocate_ptr(ds_idx,
				*(const void * const *)((const char *)data + set_offset)) +
				idx * rec_len)
				: NULL;
	}
	if (num) {
		*num = -1;
	}
	return NULL;
}

/** Removes syntax sugar around get_item() call
 * \param[in] ds_id Identifier of CLM data set
 * \param[in] registry_field name of field in clm_registry_t
 * \param[in] set_struct ..._set_t structure
 * \param[in] record_len Length of record, referenced by 'set' field
 * \param[in] idx Index of record
 * \param[out] num Optional output parameter - value of 'num' field of -1
 * \return Address of record or 0
 */
#define GET_ITEM(ds_id, registry_field, set_struct, record_len, idx, num_value) \
	get_item(ds_id, OFFSETOF(clm_registry_t, registry_field), \
	OFFSETOF(set_struct, num), OFFSETOF(set_struct, set), record_len, idx, num_value)

/** Retrieves CLM data set identifier and index, contained in given iterator
 * Applicable to previously 'packed' iterator. Not applicable to 'just
 * initialized' iterators
 * \param[in] iter Iterator to unpack
 * \param[out] ds_id CLM data set identifier retrieved from iterator
 * \param[out] idx Index retrieved from iterator
 */
static void
iter_unpack(int iter, data_source_id_t *ds_id, int *idx)
{
	--iter;
	if (ds_id) {
		*ds_id = ((iter & ITER_DS_MASK) == ITER_DS_MAIN) ? DS_MAIN : DS_INC;
	}
	if (idx) {
		*idx = iter & ITER_IDX_MASK;
	}
}

/** Creates (packs) iterator out of CLM data set identifier and index
 * \param[out] iter Resulted iterator
 * \param[in] ds_id CLM data set identifier to put to iterator
 * \param[in] idx Index to put to iterator
 */
static void
iter_pack(int *iter, data_source_id_t ds_id, int idx)
{
	if (iter) {
		*iter = (((ds_id == DS_MAIN) ? ITER_DS_MAIN : ITER_DS_INC) |
			(idx & ITER_IDX_MASK)) + 1;
	}
}

/** Traversal of byte string sequence
 * \param[in] byte_string_seq Byte string sequence to traverse
 * \param[in] idx Index of string to find
 * \param[in] sequence_index NULL or sequence index, containing offsets of strings
 * \return Address of idx's string in a sequence
 */
static const unsigned char *
get_byte_string(const unsigned char *byte_string_seq, int idx,
	const unsigned short *sequence_index)
{
	if (sequence_index) {
		return byte_string_seq + sequence_index[idx];
	}
	while (idx--) {
		byte_string_seq += *byte_string_seq + 1;
	}
	return byte_string_seq;
}

/** Skips base locale header, optionally retrieving some data from it
 * \param[in] loc_def Address of base locale definition/header
 * \param[out, optional] base_flags_ptr Base locale flags
 * \param[out, optional] psd_limits_ptr Address of first PSD limits record
 * (receives NULL if locale does not have them)
 * \return Address past base locale header
 */
static const unsigned char *
skip_base_header(const unsigned char *loc_def, unsigned char *base_flags_ptr,
	const unsigned char **psd_limits_ptr)
{
	unsigned char flags = loc_def[CLM_LOC_DSC_FLAGS_IDX];
	if (base_flags_ptr) {
		*base_flags_ptr = flags;
	}
	loc_def += CLM_LOC_DSC_BASE_HDR_LEN;
	loc_def += 1 + CLM_LOC_DSC_PUB_REC_LEN * (int)(*loc_def);
	if (psd_limits_ptr) {
		*psd_limits_ptr = (flags & CLM_DATA_FLAG_PSD_LIMITS) ? loc_def : NULL;
	}
	if (flags & CLM_DATA_FLAG_PSD_LIMITS) {
		do {
			flags = *loc_def++;
			loc_def += 1 + CLM_LOC_DSC_PSD_REC_LEN * (int)(*loc_def);
		} while (flags & CLM_DATA_FLAG_MORE);
	}
	return loc_def;
}

/** Looks up byte string that contains locale definition, precomputes
 * locale-related data
 * \param[in] locales Region locales
 * \param[in] loc_type Type of locale to retrieve (CLM_LOC_IDX_...)
 * \param[out] loc_data Locale-related data. If locale not found all fields are
 * zeroed
 * \return TRUE in case of success, FALSE if region locale definitions
 * structure contents is invalid
 */
static MY_BOOL
get_loc_def(const clm_country_locales_t *locales, int loc_type,
	locale_data_t *loc_data)
{
	data_source_id_t ds_id;
	int bw_idx;
	const loc_type_dsc_t *ltd;
	clm_band_t band;
	const unsigned char *loc_def = NULL;
	data_dsc_t *ds;

	bzero(loc_data, sizeof(*loc_data));
	if ((unsigned int)loc_type >= ARRAYSIZE(loc_type_dscs)) {
		return FALSE;
	}
	ltd = &loc_type_dscs[loc_type];
	band = ltd->band;
	loc_def = *(const unsigned char * const *)((const char *)locales + ltd->def_field_offset);
	if (!loc_def) {
		return TRUE;
	}
	ds_id = (locales->main_loc_data_bitmask & (1 << loc_type)) ? DS_MAIN : DS_INC;
	ds = get_data_sources(ds_id);
	loc_data->def_ptr = loc_def;
	loc_data->chan_ranges_bw = ds->chan_ranges_band_bw[band];
	loc_data->rate_sets_bw = ds->rate_sets_bw[band];
	loc_data->rate_sets_indices_bw = ds->rate_sets_indices_bw[band];
	loc_data->ext_rate_sets_bw = ds->ext_rate_sets_bw[band];
	loc_data->ext_rate_sets_indices_bw = ds->ext_rate_sets_indices_bw[band];
	loc_data->valid_channels = (const unsigned char *)GET_DATA(ds_id, locale_valid_channels);
	loc_data->restricted_channels =
		(const unsigned char *)GET_DATA(ds_id, restricted_channels);
	for (bw_idx = 0; bw_idx < CLM_BW_NUM; ++bw_idx) {
		loc_data->combs[bw_idx] = &ds->valid_channel_combs[band][bw_idx];
	}
	loc_data->ext4_rate_sets_bw = ds->ext4_rate_sets_bw[band];
	loc_data->ext4_rate_sets_indices_bw = ds->ext4_rate_sets_indices_bw[band];
	loc_data->he_rate_sets_bw = ds->he_rate_sets_bw[band];
	loc_data->he_rate_sets_indices_bw = ds->he_rate_sets_indices_bw[band];
	return TRUE;
}

/** Retrieving helper data on base and HT locale on given band
 * Sets def_ptr to point to TX power data
 * \param[in] locales Region locales
 * \param[in] band Band to retrieve information for
 * \param[out, optional] base_flags If nonnull - container for BLOB base locale
 * flags
 * \return TRUE if retrieval was successful
 */
static MY_BOOL
fill_base_ht_loc_data(const clm_country_locales_t *locales, clm_band_t band,
	locale_data_t base_ht_loc_data[BH_NUM], unsigned char *base_flags)
{
	if (base_flags) {
		*base_flags = 0;
	}
	/* Computing helper information on base locale for given bandwidth */
	if (!get_loc_def(locales, compose_loc_type[band][BH_BASE], &(base_ht_loc_data[BH_BASE]))) {
		return FALSE;
	}
	if (base_ht_loc_data[BH_BASE].def_ptr) {
		/* Advance local definition pointer from regulatory info to
		 * power targets info, optionally retrieving base flags
		 */
		base_ht_loc_data[BH_BASE].def_ptr = skip_base_header(
			base_ht_loc_data[BH_BASE].def_ptr, base_flags, NULL);
	}
	return get_loc_def(locales, compose_loc_type[band][BH_HT], &(base_ht_loc_data[BH_HT]));
}

/** Fills field or group of fields in given per-band-bandwidth array of pointers
 * \param[out] dst_array Array being filled. Its true signature is:
 * 'const SOMETYPE *somearray[CLM_BAND_NUM][CLM_BW_NUM]'
 * \param[in] band Band of given piece of data
 * \param[in] bw Bandwidth of given piece of data
 * \param[in] ds_id Data source of given piece of data
 * \param[in] field_offset Piece of data. Pointer field within clm_registry_t,
 * specified by its offset within the structure
 * \param[in] enabled FALSE means that piece of data shall be ignored
 * \param[in] per_band_bw TRUE means that piece of data is per-band-bandwidth
 * field. Should be used to initialize just one field in target array
 * \param[in] per_bw Evaluated if 'per_band_bw' is FALSE. TRUE means that piece
 * is per-bandwidth field (i.e. if 'band' parameter is 5G, then value shall be
 * used for all bands for given bandwidths, if 'band' is not 5GHz, value shall
 * be ignored). FALSE means that value sshal be used for all bands and
 * bandwidths if 'band' is 5GHz and 'bw' is 20MHz, ignored otherwise
 */
static void
fill_band_bw_field(void *dst_array, clm_band_t band, clm_bandwidth_t bw,
	data_source_id_t ds_id, unsigned int field_offset, MY_BOOL enabled,
	MY_BOOL per_band_bw, MY_BOOL per_bw)
{
	const void *(*dst)[CLM_BW_NUM] = (const void *(*)[CLM_BW_NUM])dst_array;
	const void *ptr;
	int band_idx, bw_idx;
	if (!enabled) {
		return;
	}
	if (!(per_band_bw || (per_bw && (band == CLM_BAND_5G)) ||
		((band == CLM_BAND_5G) && (bw == CLM_BW_20))))
	{
		return;
	}
#ifdef WL_BAND6G
	if ((band == CLM_BAND_6G) &&
		!(get_data_sources(ds_id)->registry_flags2 & CLM_REGISTRY_FLAG2_6GHZ))
	{
		return;
	}
#endif /* WL_BAND6G */
	ptr = get_data(ds_id, field_offset);
	if (per_band_bw) {
		dst[band][bw] = ptr;
	} else if (per_bw && (band == CLM_BAND_5G)) {
		for (band_idx = 0; band_idx < (int)CLM_BAND_NUM; ++band_idx) {
			if ((band_idx != (int)CLM_BAND_2G) || (bw <= (int)CLM_BW_40)) {
				dst[band_idx][bw] = ptr;
			}
		}
	} else {
		for (band_idx = 0; band_idx < (int)CLM_BAND_NUM; ++band_idx) {
			for (bw_idx = 0; bw_idx < (int)CLM_BW_NUM; ++bw_idx) {
				if ((band_idx != (int)CLM_BAND_2G) || (bw_idx <= (int)CLM_BW_40)) {
					dst[band_idx][bw_idx] = ptr;
				}
			}
		}
	}
}

/** Tries to fill valid_channel_combs using given CLM data source
 * This function takes information about 20MHz channels from BLOB, 40MHz
 * channels from valid_channel_...g_40m_set structures hardcoded in this module
 * to save BLOB space
 * \param[in] ds_id Identifier of CLM data to get information from
 */
static void
try_fill_valid_channel_combs(data_source_id_t ds_id)
{
	static const struct band_bw_field {
		clm_band_t band;
		clm_bandwidth_t bw;
		unsigned int field_offset;
	} fields [] = {
		{CLM_BAND_2G, CLM_BW_20,
		OFFSETOF(clm_registry_t, valid_channels_2g_20m)},
		{CLM_BAND_2G, CLM_BW_40,
		OFFSETOF(clm_registry_t, valid_channels_2g_40m)},
		{CLM_BAND_5G, CLM_BW_20,
		OFFSETOF(clm_registry_t, valid_channels_5g_20m)},
		{CLM_BAND_5G, CLM_BW_40,
		OFFSETOF(clm_registry_t, valid_channels_5g_40m)},
		{CLM_BAND_5G, CLM_BW_80,
		OFFSETOF(clm_registry_t, valid_channels_5g_80m)},
		{CLM_BAND_5G, CLM_BW_160,
		OFFSETOF(clm_registry_t, valid_channels_5g_160m)},
#ifdef WL_BAND6G
		{CLM_BAND_6G, CLM_BW_20,
		OFFSETOF(clm_registry_t, valid_channels_6g_20m)},
		{CLM_BAND_6G, CLM_BW_40,
		OFFSETOF(clm_registry_t, valid_channels_6g_40m)},
		{CLM_BAND_6G, CLM_BW_80,
		OFFSETOF(clm_registry_t, valid_channels_6g_80m)},
		{CLM_BAND_6G, CLM_BW_160,
		OFFSETOF(clm_registry_t, valid_channels_6g_160m)},
#endif /* WL_BAND6G */
	};
	const clm_channel_comb_set_t *combs[CLM_BAND_NUM][CLM_BW_NUM];
	int band, bw;
	const struct band_bw_field *fd;
	data_dsc_t *ds = get_data_sources(ds_id);
	if (!ds->data) {
		/* Can't obtain data - make all combs empty */
		for (band = 0; band < CLM_BAND_NUM; ++band) {
			for (bw = 0; bw < CLM_BW_NUM; ++bw) {
				ds->valid_channel_combs[band][bw].num = 0;
			}
		}
		return;
	}
	/* Fill combs[][] with references to combs that will be retrieved from
	 * BLOB
	 */
	bzero((void *)combs, sizeof(combs));
	for (fd = fields; fd < (fields + ARRAYSIZE(fields)); ++fd) {
		fill_band_bw_field((void *)combs, fd->band, fd->bw, ds_id, fd->field_offset,
			(fd->bw == CLM_BW_20) ||
			(ds->registry_flags & CLM_REGISTRY_FLAG_HIGH_BW_COMBS),
			TRUE, FALSE);
	}
	/* Transferring combs from BLOB to valid_channel_combs[][] */
	for (band = 0; band < CLM_BAND_NUM; ++band) {
		for (bw = 0; bw < CLM_BW_NUM; ++bw) {
			const clm_channel_comb_set_t *comb_set = combs[band][bw];
			clm_channel_comb_set_t *dest_comb_set =
				&ds->valid_channel_combs[band][bw];
			if (!comb_set || !comb_set->num) {
				/* Incremental comb set empty or absent?
				 * Borrowing from base
				 */
				if (ds_id == DS_INC) {
					*dest_comb_set =
						get_data_sources(DS_MAIN)->
						valid_channel_combs[band][bw];
				}
				continue;
			}
			/* Nonempty comb set - first copy all fields... */
			*dest_comb_set = *comb_set;
			/* ... then relocate pointer (to comb vector) */
			dest_comb_set->set =
				(const clm_channel_comb_t *)relocate_ptr(ds_id, comb_set->set);
			/* Base comb set empty? Share from incremental */
			if ((ds_id == DS_INC) &&
				!get_data_sources(DS_MAIN)->valid_channel_combs[band][bw].num)
			{
				get_data_sources(DS_MAIN)->valid_channel_combs[band][bw] =
					*dest_comb_set;
			}
		}
	}
	if (!(ds->registry_flags & CLM_REGISTRY_FLAG_HIGH_BW_COMBS)) {
		/* No 40+MHz combs in BLOB => using hardcoded ones */
		ds->valid_channel_combs[CLM_BAND_2G][CLM_BW_40] = valid_channel_2g_40m_set;
		ds->valid_channel_combs[CLM_BAND_5G][CLM_BW_40] = valid_channel_5g_40m_set;
		ds->valid_channel_combs[CLM_BAND_5G][CLM_BW_80] = valid_channel_5g_80m_set;
		ds->valid_channel_combs[CLM_BAND_5G][CLM_BW_160] = valid_channel_5g_160m_set;
	}
}

/** In given comb set looks up for comb that contains given channel
 * \param[in] channel Channel to find comb for
 * \param[in] combs Comb set to find comb in
 * \return NULL or address of given comb
 */
static const clm_channel_comb_t *
get_comb(int channel, const clm_channel_comb_set_t *combs)
{
	const clm_channel_comb_t *ret, *comb_end;
	/* Fail on nonempty comb set with NULL set pointer (ClmCompiler bug)  */
	ASSERT((combs->set != NULL) || (combs->num == 0));
	for (ret = combs->set, comb_end = ret + combs->num; ret != comb_end; ++ret) {
		if ((channel >= ret->first_channel) && (channel <= ret->last_channel) &&
		   (((channel - ret->first_channel) % ret->stride) == 0))
		{
			return ret;
		}
	}
	return NULL;
}

/** Among combs whose first channel is greater than given looks up one with
 * minimum first channel
 * \param[in] channel Channel to find comb for
 * \param[in] combs Comb set to find comb in
 * \return Address of found comb, NULL if all combs have first channel smaller
 * than given
 */
static const clm_channel_comb_t *
get_next_comb(int channel, const clm_channel_comb_set_t *combs)
{
	const clm_channel_comb_t *ret, *comb, *comb_end;
	/* Fail on nonempty comb set with NULL set pointer (ClmCompiler bug)  */
	ASSERT((combs->set != NULL) || (combs->num == 0));
	for (ret = NULL, comb = combs->set, comb_end = comb + combs->num; comb != comb_end;
		++comb)
	{
		if (comb->first_channel <= channel) {
			continue;
		}
		if (!ret || (comb->first_channel < ret->first_channel)) {
			ret = comb;
		}
	}
	return ret;
}

/** Fills channel set structure from given source
 * \param[out] channels Channel set to fill
 * \param[in] channel_defs Byte string sequence that contains channel set
 * definitions
 * \param[in] def_idx Index of byte string that contains required channel set
 * definition
 * \param[in] ranges Vector of channel ranges
 * \param[in] combs Set of combs of valid channel numbers
 */
static void
get_channels(clm_channels_t *channels, const unsigned char *channel_defs,
	unsigned char def_idx, const clm_channel_range_t *ranges,
	const clm_channel_comb_set_t *combs)
{
	int num_ranges;

	if (!channels) {
		return;
	}
	bzero(channels->bitvec, sizeof(channels->bitvec));
	if (!channel_defs) {
		return;
	}
	if (def_idx == CLM_RESTRICTED_SET_NONE) {
		return;
	}
	channel_defs = get_byte_string(channel_defs, def_idx, NULL);
	num_ranges = *channel_defs++;
	if (!num_ranges) {
		return;
	}
	/* Fail on nonempty range set with NULL range pointer (ClmCompiler bug)  */
	ASSERT(ranges != NULL);
	/* Fail on empty comb set with nonempty channel range set (ClmCompiler bug) */
	ASSERT(combs != NULL);
	while (num_ranges--) {
		unsigned char r = *channel_defs++;
		if (r == CLM_RANGE_ALL_CHANNELS) {
			const clm_channel_comb_t *comb = combs->set;
			int num_combs;
			/* Fail on nonempty comb set with NULL set pointer (ClmCompiler bug)  */
			ASSERT((comb != NULL) || (combs->num == 0));
			for (num_combs = combs->num; num_combs--; ++comb) {
				int chan;
				for (chan = comb->first_channel; chan <= comb->last_channel;
					chan += comb->stride) {
					channels->bitvec[chan / 8] |=
						(unsigned char)(1 << (chan % 8));
				}
			}
		} else {
			int chan = ranges[r].start, end = ranges[r].end;
			const clm_channel_comb_t *comb = get_comb(chan, combs);
			if (!comb) {
				continue;
			}
			for (;;) {
				channels->bitvec[chan / 8] |= (unsigned char)(1 << (chan % 8));
				if (chan >= end) {
					break;
				}
				if (chan < comb->last_channel) {
					chan += comb->stride;
					continue;
				}
				comb = get_next_comb(chan, combs);
				if (!comb || (comb->first_channel > end)) {
					break;
				}
				chan = comb->first_channel;
			}
		}
	}
}

/** True if given channel belongs to given range and belongs to comb that
 * represent this range
 * \param[in] channel Channel in question
 * \param[in] range Range in question
 * \param[in] combs Comb set
 * \param[in] other_in_pair Other channel in 80+80 channel pair. Used when
 * combs parameter is zero-length
 * \return True if given channel belongs to given range and belong to comb that
 * represent this range
 */
static MY_BOOL
channel_in_range(int channel, const clm_channel_range_t *range,
	const clm_channel_comb_set_t *combs, int other_in_pair)
{
	const clm_channel_comb_t *comb;
	/* Fail on NULL comb set descriptor pointer (ClmCompiler bug)  */
	ASSERT(combs != NULL);
	if (!combs->num) {
		return (channel == range->start) && (other_in_pair == range->end);
	}
	if ((channel < range->start) || (channel > range->end)) {
		return FALSE;
	}
	comb = get_comb(range->start, combs);
	while (comb && (comb->last_channel < channel)) {
		comb = get_next_comb(comb->last_channel, combs);
	}
	return comb && (comb->first_channel <= channel) &&
		!((channel - comb->first_channel) % comb->stride);
}

#if defined(WL_RU_NUMRATES) || defined(WL_NUM_HE_RT)
/** Checks if two numerical ranges intersect by at least given length */
static MY_BOOL ranges_intersect(int l1, int r1, int l2, int r2, int min_intersection)
{
	return (((r1 < r2) ? r1 : r2) - ((l1 > l2) ? l1 : l2)) >= min_intersection;
}
#endif /* defined(WL_RU_NUMRATES) || defined(WL_NUM_HE_RT) */

#if defined(WL_NUM_HE_RT) && !defined(WL_RU_NUMRATES)
/** Returns half-step for nonintersecting channels of given bandwidth in channel
 * number units (e.g. 2 for CLM_BW_20)
 */
static int
half_step(clm_bandwidth_t bandwidth)
{
	if (bandwidth == CLM_BW_80_80) {
		bandwidth = CLM_BW_80;
	}
	return (CHAN_STRIDE << (bandwidth - CLM_BW_20)) / 2;
}
#endif /* defined(WL_NUM_HE_RT) && !defined(WL_RU_NUMRATES) */


/** Fills rate_type[] */
static void
fill_rate_types(void)
{
	/** Rate range descriptor */
	static const struct {
		/** First rate in range */
		int start;

		/* Number of rates in range */
		int length;

		/* Rate type for range */
		enum clm_rate_type rt;
	} rate_ranges[] = {
		{0,                      WL_NUMRATES,        RT_MCS},
		{WL_RATE_1X1_DSSS_1,     WL_RATESET_SZ_DSSS, RT_DSSS},
		{WL_RATE_1X2_DSSS_1,     WL_RATESET_SZ_DSSS, RT_DSSS},
		{WL_RATE_1X3_DSSS_1,     WL_RATESET_SZ_DSSS, RT_DSSS},
		{WL_RATE_1X1_OFDM_6,     WL_RATESET_SZ_OFDM, RT_OFDM},
		{WL_RATE_1X2_CDD_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
		{WL_RATE_1X3_CDD_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
#ifdef CLM_TXBF_RATES_SUPPORTED
		{WL_RATE_1X2_TXBF_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
		{WL_RATE_1X3_TXBF_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
#endif /* LM_TXBF_RATES_SUPPORTED */
#if WL_NUMRATES >= 336
		{WL_RATE_1X4_DSSS_1,     WL_RATESET_SZ_DSSS, RT_DSSS},
		{WL_RATE_1X4_CDD_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
#ifdef CLM_TXBF_RATES_SUPPORTED
		{WL_RATE_1X4_TXBF_OFDM_6, WL_RATESET_SZ_OFDM, RT_OFDM},
#endif /* CLM_TXBF_RATES_SUPPORTED */
#endif /* WL_NUMRATES >= 336 */
	};
	int range_idx;
	for (range_idx = 0; range_idx < (int)ARRAYSIZE(rate_ranges); ++range_idx) {
		int rate_idx = rate_ranges[range_idx].start;
		int end = rate_idx + rate_ranges[range_idx].length;
		enum clm_rate_type rt = rate_ranges[range_idx].rt;
		do {
			SET_RATE_TYPE(rate_idx, rt);
		} while (++rate_idx < end);
	}
#ifdef WL_RU_NUMRATES
	{
		int su_mode_idx;
		for (su_mode_idx = 0; su_mode_idx < (int)ARRAYSIZE(su_base_rates); ++su_mode_idx) {
			unsigned int su_base_code = su_base_rates[su_mode_idx];
			unsigned int ri;
			for (ri = su_base_code; ri < (su_base_code + WL_RATESET_SZ_HE_MCS); ++ri) {
				SET_RATE_TYPE(ri, RT_SU);
			}
		}
	}
#endif /* WL_RU_NUMRATES */
}

#ifdef WL_RU_NUMRATES
/** Retrieves a parameters of given RU rate
 * \param[in] ru_rate RU rate in question
 * \param[out] rate_dsc Pointer to clm_he_rate_dsc_t, containing all rate
 * parameters, except rate type
 * \param[out] rate_type Rate type of given rate
 */
static void
get_ru_rate_info(clm_ru_rates_t ru_rate, const clm_he_rate_dsc_t **ru_rate_dsc,
	wl_he_rate_type_t *ru_rate_type)
{
	*ru_rate_dsc = he_rate_descriptors + (ru_rate % ARRAYSIZE(he_rate_descriptors));
	*ru_rate_type = (wl_he_rate_type_t)(ru_rate / ARRAYSIZE(he_rate_descriptors) + 1);
}

/** Fills-in RU(OFDMA) rates - related data structures */
static void
fill_ru_rate_info(void)
{
	static const struct {
		clm_ru_rates_t start;
		clm_ru_rate_bw_t rrb;
	} rate_ranges[] = {
		{WL_RU_RATE_1X1_26SS1, RRB_20},
		{WL_RU_RATE_1X1_52SS1, RRB_20},
		{WL_RU_RATE_1X1_106SS1, RRB_20},
		{WL_RU_RATE_1X1_UBSS1, RRB_DL},
		{WL_RU_RATE_1X1_LUBSS1, RRB_DL},
		{WL_RU_RATE_1X1_242SS1, RRB_20},
		{WL_RU_RATE_1X1_484SS1, RRB_40},
		{WL_RU_RATE_1X1_996SS1, RRB_80},
	};
	unsigned int range_idx, mode_idx;
	ASSERT(ARRAYSIZE(he_rate_descriptors) == ARRAYSIZE(su_base_rates));
	for (range_idx = 0; range_idx < ARRAYSIZE(rate_ranges); ++range_idx) {
		for (mode_idx = 0; mode_idx < ARRAYSIZE(he_rate_descriptors); ++mode_idx) {
			set_ru_rate_bw_type(rate_ranges[range_idx].start + mode_idx,
				rate_ranges[range_idx].rrb);
		}
	}
	for (mode_idx = 0; mode_idx < ARRAYSIZE(su_base_rates); ++mode_idx) {
		/* Prefilling hash for collision checking */
		set_he_tx_mode(su_base_rates[mode_idx], 0xFF);
	}
	for (mode_idx = 0; mode_idx < ARRAYSIZE(su_base_rates); ++mode_idx) {
		unsigned int he0_rate = su_base_rates[mode_idx];
		/* Checking for hash collision. If it happened - hash constants
		 * (SU_TX_MODE_HASH_...) must be changed
		 */
		ASSERT(get_he_tx_mode(he0_rate) == 0xFF);
		/* This check is used to identify HE0 rates during
		 * clm_he_limit() and clm_ru_limits(). If this condition will
		 * cease to be met after next rate code rehash - something need
		 * to be done
		 */
		ASSERT((RATE_TYPE(he0_rate) == RT_SU) && (RATE_TYPE(he0_rate - 1) != RT_SU));
		set_he_tx_mode(he0_rate, mode_idx);
	}
}
#endif /*  WL_RU_NUMRATES */


/** True if BLOB format with given major version supported
 * Made separate function to avoid ROM invalidation (albeit it is always wrong idea to put
 * CLM to ROM)
 * \param[in] BLOB major version
 * Returns TRUE if given major BLOB format version supported
 */
static MY_BOOL
is_blob_version_supported(int format_major)
{
	return (format_major <= FORMAT_VERSION_MAJOR) &&
		(format_major >= FORMAT_MIN_COMPAT_MAJOR);
}

/** Verifies that BLOB's flags and flags2 field are consistent with environment
 * in which ClmAPI was compiled
 * It is expected that when this function is called data_dsc_t::flags and
 * data_dsc_t::flags2 fields are already filled in
 * \param[in] ds_id Identifier of CLM data set
 * Returns CLM_RESULT_OK if no problems were found, CLM_RESULT_ERR otherwise
 */
static clm_result_t
check_data_flags_compatibility(data_source_id_t ds_id)
{
	data_dsc_t *ds = get_data_sources(ds_id);
	unsigned int registry_flags = ds->registry_flags;
	unsigned int registry_flags2 = ds->registry_flags2;
	if ((registry_flags & CLM_REGISTRY_FLAG_NUM_RATES_MASK) &&
		((registry_flags & CLM_REGISTRY_FLAG_NUM_RATES_MASK) !=
		((WL_NUMRATES << CLM_REGISTRY_FLAG_NUM_RATES_SHIFT) &
		CLM_REGISTRY_FLAG_NUM_RATES_MASK)))
	{
		/* BLOB was compiled for WL_NUMRATES different from one, ClmAPI
		 * was compiled for
		 */
		return CLM_RESULT_ERR;
	}
	/* Unknown flags present. May never happen for regularly released
	 * ClmAPI sources, but can for ClmAPI with patched-in features from
	 * more recent BLOB formats
	 */
	if ((registry_flags & ~(unsigned int)CLM_REGISTRY_FLAG_ALL) ||
		(registry_flags2 & ~(unsigned int)CLM_REGISTRY_FLAG2_ALL))
	{
		/* BLOB contains flags ClmAPI is unaware of */
		return CLM_RESULT_ERR;
	}
#ifdef WL_RU_NUMRATES
	if ((registry_flags2 & CLM_REGISTRY_FLAG2_HE_RU_FIXED) &&
		((registry_flags2 & CLM_REGISTRY_FLAG2_NUM_RU_RATES_MASK) !=
		((WL_RU_NUMRATES << CLM_REGISTRY_FLAG2_NUM_RU_RATES_SHIFT) &
		CLM_REGISTRY_FLAG2_NUM_RU_RATES_MASK)))
	{
		/* Number of RU rates doesn't match one that BLOB was compiled
		 * for
		 */
		return CLM_RESULT_ERR;
	}
#endif /* WL_RU_NUMRATES */
	return CLM_RESULT_OK;
}

/** Fills-in fields in data_dsc_t that related to layout of region record
 * \param[in] ds_id Identifier of CLM data set
 * Returns CLM_RESULT_OK if no problems were found, CLM_RESULT_ERR otherwise
 */
static clm_result_t
fill_region_record_data_fields(data_source_id_t ds_id)
{
	data_dsc_t *ds = get_data_sources(ds_id);
	unsigned int registry_flags = ds->registry_flags;
	unsigned int registry_flags2 = ds->registry_flags2;
	ds->reg_rev16_idx = -1;
	ds->reg_loc10_idx = -1;
	ds->reg_loc12_idx = -1;
	ds->reg_loc14_idx = -1;
	ds->reg_loc12_6g_idx = -1;
	ds->reg_loc16_6g_idx = -1;
	ds->reg_flags_idx = -1;
	ds->reg_flags_2_idx = -1;
	ds->loc_idx_mask = 0xFF;
	if (registry_flags & CLM_REGISTRY_FLAG_CD_REGIONS) {
		int idx = 0;
		int extra_loc_idx_bytes =
			(registry_flags & CLM_REGISTRY_FLAG_CD_LOC_IDX_BYTES_MASK) >>
			CLM_REGISTRY_FLAG_CD_LOC_IDX_BYTES_SHIFT;
		/* Indicates format used by shims that extends
		 * clm_country_rev_definition10_fl_t but do not use index-based
		 * implementation of CC/rev references
		 */
		MY_BOOL loc12_flag_swap =
			!!(registry_flags & CLM_REGISTRY_FLAG_REGION_LOC_12_FLAG_SWAP);
		if (registry_flags2 & CLM_REGISTRY_FLAG2_6GHZ) {
			idx += 2;
		}
		ds->loc_idx_mask = (1 << (8 + 2 * extra_loc_idx_bytes)) - 1;
		ds->reg_rev16_idx =
			(registry_flags & CLM_REGISTRY_FLAG_CD_16_BIT_REV) ? idx++ : -1;
		ds->reg_loc10_idx = (extra_loc_idx_bytes-- > 0) ? idx++ : -1;
		if (loc12_flag_swap) {
			ds->reg_flags_idx = idx++;
			ds->reg_loc12_idx = idx++;
			ds->loc_idx_mask = 0xFFF;
		} else {
			ds->reg_loc12_idx = (extra_loc_idx_bytes-- > 0) ? idx++ : -1;
			ds->reg_loc14_idx = (extra_loc_idx_bytes-- > 0) ? idx++ : -1;
			ds->reg_flags_idx = idx++;
			if (registry_flags2 & CLM_REGISTRY_FLAG2_6GHZ) {
				if (ds->reg_loc10_idx >= 0) {
					ds->reg_loc12_6g_idx = idx++;
				}
				if (ds->reg_loc14_idx >= 0) {
					ds->reg_loc16_6g_idx = idx++;
				}
			}
		}
		ds->country_rev_rec_len =
			OFFSETOF(clm_country_rev_definition_cd10_t, extra) + idx;
		ds->ccrev_format =
			loc12_flag_swap ? CCREV_FORMAT_CC_REV :
			((registry_flags & CLM_REGISTRY_FLAG_CD_16_BIT_REGION_INDEX)
			? CCREV_FORMAT_CC_IDX16 : CCREV_FORMAT_CC_IDX8);
	} else if (registry_flags & CLM_REGISTRY_FLAG_COUNTRY_10_FL) {
		ds->loc_idx_mask = 0x3FF;
		ds->reg_loc10_idx = 0;
		ds->reg_flags_idx = 1;
		ds->country_rev_rec_len = sizeof(clm_country_rev_definition10_fl_t);
		ds->ccrev_format = CCREV_FORMAT_CC_REV;
	} else {
		ds->country_rev_rec_len = sizeof(clm_country_rev_definition_t);
		ds->ccrev_format = CCREV_FORMAT_CC_REV;
	}
	if (registry_flags & CLM_REGISTRY_FLAG_REGION_FLAG_2) {
		ds->reg_flags_2_idx = ds->country_rev_rec_len++ -
			OFFSETOF(clm_country_rev_definition_cd10_t, extra);
		ds->scr_idx_4 = FALSE;
	}
	return CLM_RESULT_OK;
}

/** Fills-in fields in data_dsc_t that points to various collections inside BLOB
 * \param[in] ds_id Identifier of CLM data set
 * Returns CLM_RESULT_OK if no problems were found, CLM_RESULT_ERR otherwise
 */
static clm_result_t
fill_pointer_data_fields(data_source_id_t ds_id)
{
	static const struct band_bw_fields_dsc {
		clm_band_t band;
		clm_bandwidth_t bw;
		unsigned int main_rates_field_offset;
		unsigned int ext_rates_field_offset;
		unsigned int ext4_rates_field_offset;
		unsigned int he_rates_field_offset;
		unsigned int main_rates_indices_field_offset;
		unsigned int ext_rates_indices_field_offset;
		unsigned int ext4_rates_indices_field_offset;
		unsigned int he_rates_indices_field_offset;
		unsigned int channel_ranges_field_offset;
	} band_bw_fields[] = {
		{CLM_BAND_2G, CLM_BW_20,
		OFFSETOF(clm_registry_t, locale_rate_sets_2g_20m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_2g_20m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_2g_20m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_2g_20m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_2g_20m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_2g_20m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_2g_20m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_2g_20m),
		OFFSETOF(clm_registry_t, channel_ranges_2g_20m)},
		{CLM_BAND_2G, CLM_BW_40,
		OFFSETOF(clm_registry_t, locale_rate_sets_2g_40m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_2g_40m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_2g_40m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_2g_40m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_2g_40m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_2g_40m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_2g_40m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_2g_40m),
		OFFSETOF(clm_registry_t, channel_ranges_2g_40m)},
		{CLM_BAND_5G, CLM_BW_20,
		OFFSETOF(clm_registry_t, locale_rate_sets_5g_20m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_5g_20m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_5g_20m),
		OFFSETOF(clm_registry_t, locale_rate_sets_he),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_5g_20m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_5g_20m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_5g_20m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_5g_20m),
		OFFSETOF(clm_registry_t, channel_ranges_20m)},
		{CLM_BAND_5G, CLM_BW_40,
		OFFSETOF(clm_registry_t, locale_rate_sets_5g_40m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_5g_40m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_5g_40m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_5g_40m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_5g_40m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_5g_40m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_5g_40m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_5g_40m),
		OFFSETOF(clm_registry_t, channel_ranges_40m)},
		{CLM_BAND_5G, CLM_BW_80,
		OFFSETOF(clm_registry_t, locale_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, channel_ranges_80m)},
		{CLM_BAND_5G, CLM_BW_80_80,
		OFFSETOF(clm_registry_t, locale_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_5g_80m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_5g_80m),
		OFFSETOF(clm_registry_t, channel_ranges_80m)},
		{CLM_BAND_5G, CLM_BW_160,
		OFFSETOF(clm_registry_t, locale_rate_sets_5g_160m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_5g_160m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_5g_160m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_5g_160m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_5g_160m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_5g_160m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_5g_160m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_5g_160m),
		OFFSETOF(clm_registry_t, channel_ranges_160m)},
#ifdef WL_BAND6G
		{CLM_BAND_6G, CLM_BW_20,
		OFFSETOF(clm_registry_t, locale_rate_sets_6g_20m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_6g_20m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_6g_20m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_6g_20m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_6g_20m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_6g_20m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_6g_20m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_6g_20m),
		OFFSETOF(clm_registry_t, channel_ranges_6g_20m)},
		{CLM_BAND_6G, CLM_BW_40,
		OFFSETOF(clm_registry_t, locale_rate_sets_6g_40m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_6g_40m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_6g_40m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_6g_40m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_6g_40m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_6g_40m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_6g_40m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_6g_40m),
		OFFSETOF(clm_registry_t, channel_ranges_6g_40m)},
		{CLM_BAND_6G, CLM_BW_80,
		OFFSETOF(clm_registry_t, locale_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, channel_ranges_6g_80m)},
		{CLM_BAND_6G, CLM_BW_80_80,
		OFFSETOF(clm_registry_t, locale_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_6g_80m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_6g_80m),
		OFFSETOF(clm_registry_t, channel_ranges_6g_80m)},
		{CLM_BAND_6G, CLM_BW_160,
		OFFSETOF(clm_registry_t, locale_rate_sets_6g_160m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_6g_160m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_6g_160m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_6g_160m),
		OFFSETOF(clm_registry_t, locale_rate_sets_index_6g_160m),
		OFFSETOF(clm_registry_t, locale_ext_rate_sets_index_6g_160m),
		OFFSETOF(clm_registry_t, locale_ext4_rate_sets_index_6g_160m),
		OFFSETOF(clm_registry_t, locale_he_rate_sets_index_6g_160m),
		OFFSETOF(clm_registry_t, channel_ranges_6g_160m)}
#endif /* WL_BAND6G */
	};
	data_dsc_t *ds = get_data_sources(ds_id);
	int registry_flags = ds->registry_flags;
	int registry_flags2 = ds->registry_flags2;
	const struct band_bw_fields_dsc *fd;
	if (registry_flags & CLM_REGISTRY_FLAG_REGREV_REMAP) {
		ds->regrev_remap = (const clm_regrev_cc_remap_set_t *)GET_DATA(ds_id, regrev_remap);
	}
	for (fd = band_bw_fields; fd < (band_bw_fields + ARRAYSIZE(band_bw_fields)); ++fd) {
		fill_band_bw_field((void *)ds->chan_ranges_band_bw, fd->band, fd->bw, ds_id,
			fd->channel_ranges_field_offset, TRUE,
			(registry_flags2 & CLM_REGISTRY_FLAG2_PER_BAND_BW_RANGES) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BW_RS) != 0);
		fill_band_bw_field((void *)ds->rate_sets_bw, fd->band, fd->bw, ds_id,
			fd->main_rates_field_offset, TRUE,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BAND_RATES) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BW_RS) != 0);
		fill_band_bw_field((void *)ds->ext_rate_sets_bw, fd->band, fd->bw, ds_id,
			fd->ext_rates_field_offset,
			(registry_flags & CLM_REGISTRY_FLAG_EXT_RATE_SETS) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BAND_RATES) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BW_RS) != 0);
		fill_band_bw_field((void *)ds->ext4_rate_sets_bw, fd->band, fd->bw, ds_id,
			fd->ext4_rates_field_offset,
			(registry_flags2 & CLM_REGISTRY_FLAG2_EXT4) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BAND_RATES) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BW_RS) != 0);
		fill_band_bw_field((void *)ds->he_rate_sets_bw, fd->band, fd->bw, ds_id,
			fd->he_rates_field_offset,
			(registry_flags2 & CLM_REGISTRY_FLAG2_HE_LIMITS) != 0,
			(registry_flags2 & CLM_REGISTRY_FLAG2_RU_SETS_PER_BW_BAND) != 0,
			FALSE);
		fill_band_bw_field((void *)ds->rate_sets_indices_bw, fd->band, fd->bw, ds_id,
			fd->main_rates_indices_field_offset,
			(registry_flags2 & CLM_REGISTRY_FLAG2_RATE_SET_INDEX) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BAND_RATES) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BW_RS) != 0);
		fill_band_bw_field((void *)ds->ext_rate_sets_indices_bw, fd->band, fd->bw, ds_id,
			fd->ext_rates_indices_field_offset,
			(registry_flags & CLM_REGISTRY_FLAG_EXT_RATE_SETS) &&
			(registry_flags2 & CLM_REGISTRY_FLAG2_RATE_SET_INDEX),
			(registry_flags & CLM_REGISTRY_FLAG_PER_BAND_RATES) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BW_RS) != 0);
		fill_band_bw_field((void *)ds->ext4_rate_sets_indices_bw, fd->band, fd->bw, ds_id,
			fd->ext4_rates_indices_field_offset,
			(registry_flags2 & CLM_REGISTRY_FLAG2_EXT4) &&
			(registry_flags2 & CLM_REGISTRY_FLAG2_RATE_SET_INDEX),
			(registry_flags & CLM_REGISTRY_FLAG_PER_BAND_RATES) != 0,
			(registry_flags & CLM_REGISTRY_FLAG_PER_BW_RS) != 0);
		fill_band_bw_field((void *)ds->he_rate_sets_indices_bw, fd->band, fd->bw, ds_id,
			fd->he_rates_indices_field_offset,
			(registry_flags2 & CLM_REGISTRY_FLAG2_HE_LIMITS) &&
			(registry_flags2 & CLM_REGISTRY_FLAG2_RATE_SET_INDEX),
			(registry_flags2 & CLM_REGISTRY_FLAG2_RU_SETS_PER_BW_BAND) != 0,
			FALSE);
	}
	if (registry_flags2 & CLM_REGISTRY_FLAG2_HE_LIMITS) {
#if defined(WL_RU_NUMRATES)
		if (!(registry_flags2 & CLM_REGISTRY_FLAG2_HE_RU_FIXED)) {
			/* Old-style HE limits not supported with new bcmwifi_rates.h */
			return CLM_RESULT_ERR;
		}
#elif defined(WL_NUM_HE_RT)
		if ((registry_flags2 & (CLM_REGISTRY_FLAG2_HE_RU_FIXED |
			CLM_REGISTRY_FLAG2_HE_SU_ORDINARY)) || (ds_id != DS_MAIN))
		{
			/* New-style HE-limits and incremental operation
			 * not supported with old bcmwifi_rates.h
			 */
			return CLM_RESULT_ERR;
		}
		ds->he_rate_dscs = (const clm_he_rate_dsc_t *)GET_DATA(ds_id, he_rates);
#else
		/* HE limits not supported with very old style bcmwifi_rates.h */
		return CLM_RESULT_ERR;
#endif /* WL_NUM_HE_RT */
	}
	return CLM_RESULT_OK;
}

/** Initializes given CLM data source
 * \param[in] header Header of CLM data
 * \param[in] ds_id Identifier of CLM data set
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if data address is
 * zero or if CLM data tag is absent at given address or if major number of CLM
 * data format version is not supported by CLM API
 */
static clm_result_t
clm_data_source_init(const clm_data_header_t *header, data_source_id_t ds_id)
{
	data_dsc_t *ds = get_data_sources(ds_id);
	if (header) {
		MY_BOOL has_registry_flags = TRUE;
		clm_result_t err;
		if (strncmp(header->header_tag, CLM_HEADER_TAG, sizeof(header->header_tag))) {
			return CLM_RESULT_ERR;
		}
		if ((header->format_major == 5) && (header->format_minor == 1)) {
			has_registry_flags = FALSE;
		} else if (!is_blob_version_supported(header->format_major)) {
			return CLM_RESULT_ERR;
		}
		ds->scr_idx_4 = header->format_major <= 17;
		ds->relocation = (uintptr)((const char*)header -(const char*)header->self_pointer);
		ds->data = (const clm_registry_t*)relocate_ptr(ds_id, header->data);
		ds->registry_flags = has_registry_flags ? ds->data->flags : 0;
		ds->registry_flags2 = (ds->registry_flags & CLM_REGISTRY_FLAG_REGISTRY_FLAGS2)
			? ds->data->flags2 : 0;
		ds->header = header;

		err = check_data_flags_compatibility(ds_id);
		if (err != CLM_RESULT_OK) {
			return err;
		}

		err = fill_region_record_data_fields(ds_id);
		if (err != CLM_RESULT_OK) {
			return err;
		}

		err = fill_pointer_data_fields(ds_id);
		if (err != CLM_RESULT_OK) {
			return err;
		}


	} else {
		ds->relocation = 0;
		ds->data = NULL;
	}
	try_fill_valid_channel_combs(ds_id);
	if (ds_id == DS_MAIN) {
		try_fill_valid_channel_combs(DS_INC);
	}
	fill_rate_types();
#ifdef WL_RU_NUMRATES
	fill_ru_rate_info();
#endif /* WL_RU_NUMRATES */
	return CLM_RESULT_OK;
}

/** True if two given CC/Revs are equal
 * \param[in] cc_rev1 First CC/Rev
 * \param[in] cc_rev2 Second CC/Rev
 * \return True if two given CC/Revs are equal
 */
static MY_BOOL
cc_rev_equal(const clm_cc_rev4_t *cc_rev1, const clm_cc_rev4_t *cc_rev2)
{
	return (cc_rev1->cc[0] == cc_rev2->cc[0]) && (cc_rev1->cc[1] == cc_rev2->cc[1]) &&
		(cc_rev1->rev == cc_rev2->rev);
}

/** True if two given CCs are equal
 * \param[in] cc1 First CC
 * \param[in] cc2 Second CC
 * \return True if two given CCs are equal
 */
static MY_BOOL
cc_equal(const char *cc1, const char *cc2)
{
	return (cc1[0] == cc2[0]) && (cc1[1] == cc2[1]);
}

/** Copies CC
 * \param[out] to Destination CC
 * \param[in] from Source CC
 */
static void
copy_cc(char *to, const char *from)
{
	to[0] = from[0];
	to[1] = from[1];
}

/** Returns country definition by index
 * \param[in] ds_id Data set to look in
 * \param[in] idx Country definition index
 * \param[out] num_countries Optional output parameter - number of regions
 * \return Country definition address, NULL if data set contains no countries
 * or index is out of range
 */
static const clm_country_rev_definition_cd10_t *
get_country_def_by_idx(data_source_id_t ds_id, int idx, int *num_countries)
{
	return (const clm_country_rev_definition_cd10_t *)
		GET_ITEM(ds_id, countries, clm_country_rev_definition_set_t,
		get_data_sources(ds_id)->country_rev_rec_len, idx, num_countries);
}

/** Performs 8->16 regrev remap
 * This function assumes that remap is needed - i.e. CLM data set uses 8 bit
 * regrevs and has CLM_REGISTRY_FLAG_REGREV_REMAP flag set
 * \param[in] ds_id Data set from which CC/rev was retrieved
 * \param[in,out] ccrev CC/rev to remap
 */
static void
remap_regrev(data_source_id_t ds_id, clm_cc_rev4_t *ccrev)
{
	const clm_regrev_cc_remap_set_t *regrev_remap_set = get_data_sources(ds_id)->regrev_remap;
	const clm_regrev_cc_remap_t *cc_remap;
	const clm_regrev_cc_remap_t *cc_remap_end;

	unsigned int num_regrevs;
	const clm_regrev_regrev_remap_t *regrev_regrev_remap;
	/* Fail if function called for a blob that does not define remaps */
	ASSERT(regrev_remap_set != NULL);
	cc_remap = (const clm_regrev_cc_remap_t *)relocate_ptr(ds_id, regrev_remap_set->cc_remaps);
	cc_remap_end = cc_remap + regrev_remap_set->num;
	/* Fail on nonempty remapped CCs' set with NULL set pointer (ClmCompiler bug)  */
	ASSERT((cc_remap != NULL) || (cc_remap_end == cc_remap));
	for (; cc_remap < cc_remap_end;	++cc_remap)
	{
		if (cc_equal(ccrev->cc, cc_remap->cc)) {
			break;
		}
	}
	if (cc_remap >= cc_remap_end) {
		return;
	}
	/* Fail on empty remap descriptors' set when there is a reference into it
	 * (ClmCompiler bug)
	 */
	ASSERT(regrev_remap_set->regrev_remaps != NULL);
	regrev_regrev_remap = (const clm_regrev_regrev_remap_t *)relocate_ptr(ds_id,
		regrev_remap_set->regrev_remaps) + cc_remap->index;
	for (num_regrevs = cc_remap[1].index - cc_remap->index; num_regrevs--;
		++regrev_regrev_remap)
	{
		if (regrev_regrev_remap->r8 == ccrev->rev) {
			ccrev->rev = (unsigned int)regrev_regrev_remap->r16l +
				((unsigned int)regrev_regrev_remap->r16h << 8);
			break;
		}
	}
}

/** Reads CC/rev from region (country) record
 * \param[in] ds_id Identifier of CLM data set
 * \param[in] country Region record to read from
 * \param[out] result Buffer for result
 */
static void
get_country_ccrev(data_source_id_t ds_id,
	const clm_country_rev_definition_cd10_t *country,
	clm_cc_rev4_t *result)
{
	data_dsc_t *ds = get_data_sources(ds_id);
	copy_cc(result->cc, country->cc_rev.cc);
	result->rev = country->cc_rev.rev;
	if (ds->reg_rev16_idx >= 0) {
		result->rev += ((unsigned int)country->extra[ds->reg_rev16_idx]) << 8;
	} else if (result->rev == (CLM_DELETED_MAPPING & 0xFF)) {
		result->rev = CLM_DELETED_MAPPING;
	} else if (ds->regrev_remap) {
		remap_regrev(ds_id, result);
	}
}

/** Reads CC/rev from given memory address
 * CC/rev in memory may be in form of clm_cc_rev4_t, 8-bit index, 16-bit index
 * \param[in] ds_id Identifier of CLM data set
 * \param[out] result Buffer for result
 * \param[in] raw_ccrev Address of raw CC/rev or vector of CC/revs
 * \param[in] raw_ccrev_idx Index in vector of CC/revs
 */
static void
get_ccrev(data_source_id_t ds_id, clm_cc_rev4_t *result, const void *raw_ccrev,
	int raw_ccrev_idx)
{
	data_dsc_t *ds = get_data_sources(ds_id);
	const clm_cc_rev_t *plain_ccrev = NULL;
	/* Determining storage format */
	switch (ds->ccrev_format) {
	case CCREV_FORMAT_CC_REV:
		/* Stored as plain clm_cc_rev_t */
		plain_ccrev = (const clm_cc_rev_t *)raw_ccrev + raw_ccrev_idx;
		break;
	case CCREV_FORMAT_CC_IDX8:
	case CCREV_FORMAT_CC_IDX16:
		{
			/* Stored as 8-bit or 16-bit index */
			int idx = (ds->ccrev_format == CCREV_FORMAT_CC_IDX8)
				? *((const unsigned char *)raw_ccrev + raw_ccrev_idx)
				: *((const unsigned short *)raw_ccrev + raw_ccrev_idx);
			int num_countries;
			const clm_country_rev_definition_cd10_t *country =
				get_country_def_by_idx(ds_id, idx, &num_countries);
			/* Index to region table or to extra_ccrevs? */
			if (country) {
				/* Index to region table */
				get_country_ccrev(ds_id, country, result);
			} else {
				/* Index to extra_ccrev */
				const void *ccrev =
					GET_ITEM(ds_id, extra_ccrevs, clm_cc_rev_set_t,
					(ds->reg_rev16_idx >= 0) ? sizeof(clm_cc_rev4_t)
					: sizeof(clm_cc_rev_t),
					idx - num_countries, NULL);
				/* Fail on empty extra CC/rev despite there is
				 * a reference into it (ClmCompiler bug)
				 */
				ASSERT(ccrev != NULL);
				/* What format extra_ccrev has? */
				if (ds->reg_rev16_idx >= 0) {
					*result = *(const clm_cc_rev4_t *)ccrev;
				} else {
					/* clm_cc_rev_t structures (8-bit
					 * rev)
					 */
					plain_ccrev = (const clm_cc_rev_t *)ccrev;
				}
			}
		}
		break;
	}
	if (plain_ccrev) {
		copy_cc(result->cc, plain_ccrev->cc);
		result->rev = plain_ccrev->rev;
		if (result->rev == (CLM_DELETED_MAPPING & 0xFF)) {
			result->rev = CLM_DELETED_MAPPING;
		} else if (ds->regrev_remap) {
			remap_regrev(ds_id, result);
		}
	}
}

/** Retrieves aggregate data by aggregate index
 * \param[in] ds_id Identifier of CLM data set
 * \param[in] idx Aggregate index
 * \param[out] result Buffer for result
 * \param[out] num_aggregates Optional output parameter - number of aggregates
    in data set
 * \return TRUE if index is in valid range
 */
static MY_BOOL
get_aggregate_by_idx(data_source_id_t ds_id, int idx, aggregate_data_t *result,
	int *num_aggregates)
{
	MY_BOOL maps_indices =
		get_data_sources(ds_id)->ccrev_format != CCREV_FORMAT_CC_REV;
	const void *p =
		GET_ITEM(ds_id, aggregates, clm_aggregate_cc_set_t,
		maps_indices ? sizeof(clm_aggregate_cc16_t) : sizeof(clm_aggregate_cc_t),
		idx, num_aggregates);
	if (!p) {
		return FALSE;
	}
	if (maps_indices) {
		result->def_reg = ((const clm_aggregate_cc16_t *)p)->def_reg;
		result->num_regions = ((const clm_aggregate_cc16_t *)p)->num_regions;
		result->regions = relocate_ptr(ds_id, ((const clm_aggregate_cc16_t *)p)->regions);
	} else {
		get_ccrev(ds_id, &result->def_reg,
			&((const clm_aggregate_cc_t *)p)->def_reg, 0);
		result->num_regions = ((const clm_aggregate_cc_t *)p)->num_regions;
		result->regions = relocate_ptr(ds_id, ((const clm_aggregate_cc_t *)p)->regions);
	}
	return TRUE;
}

/** Looks for given aggregation in given data set
 * \param[in] ds_id Identifier of CLM data set
 * \param[in] cc_rev Aggregation's default region CC/rev
 * \param[out] idx Optional output parameter - index of aggregation in set
 * \param[out] result Output parameter - buffer for aggregate data
 * \return TRUE if found
 */
static MY_BOOL
get_aggregate(data_source_id_t ds_id, const clm_cc_rev4_t *cc_rev, int *idx,
	aggregate_data_t *result)
{
	int i;
	/* Making copy because *cc_rev may be part of *result */
	clm_cc_rev4_t target = *cc_rev;
	for (i = 0; get_aggregate_by_idx(ds_id, i, result, NULL); ++i) {
		if (cc_rev_equal(&result->def_reg, &target)) {
			if (idx) {
				*idx = i;
			}
			return TRUE;
		}
	}
	return FALSE;
}

/** Looks for mapping with given CC in given aggregation
 * \param[in] ds_id Identifier of CLM data set aggregation belongs to
 * \param[in] agg Aggregation to look in
 * \param[in] cc CC to look for
 * \param[out] result Optional buffer for resulted mapping
 * \return TRUE if found
 */
static MY_BOOL
get_mapping(data_source_id_t ds_id, const aggregate_data_t *agg,
	const ccode_t cc, clm_cc_rev4_t *result)
{
	const unsigned char *mappings =	agg ? (const unsigned char *)agg->regions : NULL;
	clm_cc_rev4_t ccrev_buf;
	int i;
	if (!mappings) {
		return FALSE;
	}
	if (!result) {
		result = &ccrev_buf;
	}
	for (i = 0; i < agg->num_regions; ++i) {
		get_ccrev(ds_id, result, mappings, i);
		if (cc_equal(cc, result->cc)) {
			return TRUE;
		}
	}
	return FALSE;
}

/** Reads locale index from region record
 * \param[in] ds_id Identifier of CLM data set region record belongs to
 * \param[in] country_definition Region definition record
 * \param[in] loc_type Locale type
 * \return Locale index or one of CLM_LOC_... special indices
 */
static int
loc_idx(data_source_id_t ds_id,
	const clm_country_rev_definition_cd10_t *country_definition,
	int loc_type)
{
	int ret = country_definition->locales[loc_type];
	data_dsc_t *ds = get_data_sources(ds_id);
#ifdef WL_BAND6G
	if (loc_type_dscs[loc_type].band == CLM_BAND_6G) {
		if (!(ds->registry_flags2 & CLM_REGISTRY_FLAG2_6GHZ)) {
			return CLM_LOC_NONE;
		}
		if (ds->reg_loc12_6g_idx >= 0) {
			ret |= ((int)country_definition->extra[ds->reg_loc12_6g_idx] <<
				((CLM_LOC_IDX_NUM - loc_type + 2) * 4)) & 0xF00;
			if (ds->reg_loc16_6g_idx >= 0) {
				ret |= ((int)country_definition->extra[ds->reg_loc16_6g_idx] <<
					((CLM_LOC_IDX_NUM - loc_type + 2) * 4 + 4)) & 0xF000;
			}
		}
		ret &= ds->loc_idx_mask;
	} else
#endif /* WL_BAND6G */
	{
		if (ds->reg_loc10_idx >= 0) {
			ret |= ((int)country_definition->extra[ds->reg_loc10_idx] <<
				((CLM_LOC_IDX_NUM - loc_type)*2)) & 0x300;
			if (ds->reg_loc12_idx >= 0) {
				ret |= ((int)country_definition->extra[ds->reg_loc12_idx] <<
					((CLM_LOC_IDX_NUM - loc_type) * 2 + 2)) & 0xC00;
				if (ds->reg_loc14_idx >= 0) {
					ret |= ((int)country_definition->extra[ds->reg_loc14_idx] <<
						((CLM_LOC_IDX_NUM - loc_type) * 2 + 4)) & 0x3000;
				}
			}
		}
	}
	if (ret == (CLM_LOC_NONE & ds->loc_idx_mask)) {
		ret = CLM_LOC_NONE;
	} else if (ret == (CLM_LOC_SAME & ds->loc_idx_mask)) {
		ret = CLM_LOC_SAME;
	} else if (ret == (CLM_LOC_DELETED & ds->loc_idx_mask)) {
		ret = CLM_LOC_DELETED;
	}
	return ret;
}

/** True if given country definition marked as deleted
 * \param[in] ds_id Identifier of CLM data set country definition belongs to
 * \param[in] country_definition Country definition structure
 * \return True if given country definition marked as deleted
 */
static MY_BOOL
country_deleted(data_source_id_t ds_id,
	const clm_country_rev_definition_cd10_t *country_definition)
{
	return loc_idx(ds_id, country_definition, 0) == CLM_LOC_DELETED;
}

/** Looks up for definition of given country (region) in given CLM data set
 * \param[in] ds_id Data set id to look in
 * \param[in] cc_rev Region CC/rev to look for
 * \param[out] idx Optional output parameter: index of found country definition
 * \return Address of country definition or NULL
 */
static const clm_country_rev_definition_cd10_t *
get_country_def(data_source_id_t ds_id, const clm_cc_rev4_t *cc_rev, int *idx)
{
	int i;
	const clm_country_rev_definition_cd10_t *ret;
	for (i = 0; (ret = get_country_def_by_idx(ds_id, i, NULL)) != NULL; ++i) {
		clm_cc_rev4_t region_ccrev;
		get_country_ccrev(ds_id, ret, &region_ccrev);
		if (cc_rev_equal(&region_ccrev, cc_rev)) {
			if (idx) {
				*idx = i;
			}
			return ret;
		}
	}
	return NULL;
}

/** Finds subchannel rule for given main channel and fills channel table for it
 * \param[out] actual_table Table to fill. Includes channel numbers only for
 * bandwidths included in subchannel rule
 * \param[out] power_inc Power increment to apply
 * \param[in] full_table Full subchannel table to take channel numbers from
 * \param[in] limits_type Limits type (subchannel ID)
 * \param[in] channel Main channel
 * \param[in] ranges Channel ranges' table
 * \param[in] comb_set Comb set for main channel's bandwidth
 * \param[in] main_rules Array of main channel subchannel rules (each rule
 * pertinent to range of main channels)
 * \param[in] num_main_rules Number of main channel subchannel rules
 * \param[in] num_subchannels Number of subchannels in rule
 * (CLM_DATA_SUB_CHAN_MAX_... constant)
 * \param[in] registry_flags Registry flags for data set that contains
 * subchannel rules
 */
static void
fill_actual_subchan_table(unsigned char actual_table[CLM_BW_NUM],
	clm_power_t *power_inc, unsigned char full_table[CLM_BW_NUM],
	int limits_type, int channel, const clm_channel_range_t *ranges,
	const clm_channel_comb_set_t *comb_set,
	const clm_sub_chan_region_rules_t *region_rules,
	const signed char *increments,
	int num_subchannels, unsigned int registry_flags)
{
	/* Rule pointer as character pointer (to simplify address
	 * arithmetic)
	 */
	const unsigned char *r;
	unsigned bw_data_len = (registry_flags & CLM_REGISTRY_FLAG_SUBCHAN_RULES_INC)
		? sizeof(clm_sub_chan_rule_inc_t) : sizeof(unsigned char);
	unsigned chan_rule_len = CLM_SUB_CHAN_RULES_IDX + (bw_data_len * num_subchannels);
	int rule_index = 0;

	/* Fail nonempty subchannel rules when there are no channel range
	 * descriptors for main channel bandwidth (ClmCompiler bug)
	 */
	ASSERT((region_rules->num == 0) || (ranges != NULL));

	/* Loop over subchannel rules */
	for (r = (const unsigned char *)region_rules->channel_rules;
			rule_index < region_rules->num; r += chan_rule_len, ++rule_index)
	{
		/* Did we find rule for range that contains given main
		 * channel?
		 */
		if (channel_in_range(channel, ranges + r[CLM_SUB_CHAN_RANGE_IDX], comb_set, 0)) {
			/* Rule found - now let's fill the table */

			/* Loop index, actual type is clm_bandwidth_t */
			int bw_idx;
			/* Subchannel rule (cell in 'Rules' page) */
			const clm_sub_chan_rule_inc_t *sub_chan_rule =
				(const clm_sub_chan_rule_inc_t *)
				(r + CLM_SUB_CHAN_RULES_IDX +
				(limits_type - 1) * bw_data_len);
			/* Probing all possible bandwidths */
			for (bw_idx = 0; bw_idx < CLM_BW_NUM; ++bw_idx) {
				/* If bandwidth included to rule */
				if ((1 << (bw_idx - CLM_BW_20)) & sub_chan_rule->bw) {
					/* Copy channel number for this
					 * bandwidth from full table
					 */
					actual_table[bw_idx] = full_table[bw_idx];
				}
			}
			*power_inc = (registry_flags & CLM_REGISTRY_FLAG_SUBCHAN_RULES_INC) ?
				sub_chan_rule->inc :
				(increments ? increments[rule_index * num_subchannels
				+ (limits_type - 1)] : 0);
			return; /* All done, no need to look for more rules */
		}
	}
}

/** Returns active channel bandwidth (same as params->bw, except that 80 instead of 80+80 */
static clm_bandwidth_t active_channel_bandwidth(const clm_limits_params_t *params)
{
	return (params->bw == CLM_BW_80_80) ? CLM_BW_80 : params->bw;
}

clm_result_t clm_init(const struct clm_data_header *header)
{
	bw_width_to_idx = bw_width_to_idx_ac;
	return clm_data_source_init(header, DS_MAIN);
}

clm_result_t clm_set_inc_data(const struct clm_data_header *header)
{
	return clm_data_source_init(header, DS_INC);
}

clm_result_t clm_iter_init(int *iter)
{
	if (iter) {
		*iter = CLM_ITER_NULL;
		return CLM_RESULT_OK;
	}
	return CLM_RESULT_ERR;
}

clm_result_t clm_limits_params_init(struct clm_limits_params *params)
{
	if (!params) {
		return CLM_RESULT_ERR;
	}
	params->bw = CLM_BW_20;
	params->antenna_idx = 0;
	params->sar = 0x7F;
	params->other_80_80_chan = 0;
	return CLM_RESULT_OK;
}

clm_result_t clm_country_iter(clm_country_t *country, ccode_t cc, unsigned int *rev)
{
	data_source_id_t ds_id;
	int idx;
	clm_result_t ret = CLM_RESULT_OK;
	if (!country || !cc || !rev) {
		return CLM_RESULT_ERR;
	}
	if (*country == CLM_ITER_NULL) {
		ds_id = DS_INC;
		idx = 0;
	} else {
		iter_unpack(*country, &ds_id, &idx);
		++idx;
	}
	for (;;) {
		int num_countries;
		const clm_country_rev_definition_cd10_t *country_definition =
			get_country_def_by_idx(ds_id, idx, &num_countries);
		clm_cc_rev4_t country_ccrev;
		if (!country_definition) {
			if (ds_id == DS_INC) {
				ds_id = DS_MAIN;
				idx = 0;
				continue;
			} else {
				ret = CLM_RESULT_NOT_FOUND;
				idx = (num_countries >= 0) ? num_countries : 0;
				break;
			}
		}
		get_country_ccrev(ds_id, country_definition, &country_ccrev);
		if (country_deleted(ds_id, country_definition)) {
			++idx;
			continue;
		}
		if ((ds_id == DS_MAIN) && get_data_sources(DS_INC)->data) {
			int i, num_inc_countries;
			const clm_country_rev_definition_cd10_t *inc_country_definition;
			for (i = 0;
				(inc_country_definition =
				get_country_def_by_idx(DS_INC, i, &num_inc_countries)) != NULL;
				++i)
			{
				clm_cc_rev4_t inc_country_ccrev;
				get_country_ccrev(DS_INC, inc_country_definition,
					&inc_country_ccrev);
				if (cc_rev_equal(&country_ccrev, &inc_country_ccrev)) {
					break;
				}
			}
			if (i < num_inc_countries) {
				++idx;
				continue;
			}
		}
		copy_cc(cc, country_ccrev.cc);
		*rev = country_ccrev.rev;
		break;
	}
	iter_pack(country, ds_id, idx);
	return ret;
}

clm_result_t clm_country_lookup(const ccode_t cc, unsigned int rev, clm_country_t *country)
{
	int ds_idx;
	clm_cc_rev4_t cc_rev;
	if (!cc || !country) {
		return CLM_RESULT_ERR;
	}
	copy_cc(cc_rev.cc, cc);
	cc_rev.rev = (regrev_t)rev;
	for (ds_idx = 0; ds_idx < DS_NUM; ++ds_idx) {
		int idx;
		const clm_country_rev_definition_cd10_t *country_definition =
			get_country_def((data_source_id_t)ds_idx, &cc_rev, &idx);
		if (!country_definition) {
			continue;
		}
		if (country_deleted((data_source_id_t)ds_idx, country_definition)) {
			return CLM_RESULT_NOT_FOUND;
		}
		iter_pack(country, (data_source_id_t)ds_idx, idx);
		return CLM_RESULT_OK;
	}
	return CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_country_def(const clm_country_t country, clm_country_locales_t *locales)
{
	data_source_id_t ds_id;
	int idx;
	unsigned int loc_type;
	const clm_country_rev_definition_cd10_t *country_definition;
	const clm_country_rev_definition_cd10_t *main_country_definition = NULL;
	int flags_idx;
	if (!locales) {
		return CLM_RESULT_ERR;
	}
	iter_unpack(country, &ds_id, &idx);
	country_definition = get_country_def_by_idx(ds_id, idx, NULL);
	if (!country_definition) {
		return CLM_RESULT_NOT_FOUND;
	}
	locales->main_loc_data_bitmask = 0;
	for (loc_type = 0; loc_type < ARRAYSIZE(loc_type_dscs); ++loc_type) {
		data_source_id_t locale_ds_id = ds_id;
		const unsigned char *loc_def;
		int locale_idx = loc_idx(locale_ds_id, country_definition, loc_type);
		if (locale_idx == CLM_LOC_SAME) {
			if (!main_country_definition) {
				clm_cc_rev4_t country_ccrev;
				get_country_ccrev(ds_id, country_definition, &country_ccrev);
				main_country_definition =
					get_country_def(DS_MAIN, &country_ccrev, NULL);
			}
			locale_ds_id = DS_MAIN;
			locale_idx = main_country_definition
				? loc_idx(locale_ds_id, main_country_definition, loc_type)
				: CLM_LOC_NONE;
		}
		if (locale_idx == CLM_LOC_NONE) {
			loc_def = NULL;
		} else {
			MY_BOOL is_base = loc_type_dscs[loc_type].flavor == BH_BASE;
			loc_def = (const unsigned char *)get_data(locale_ds_id,
				loc_type_dscs[loc_type].loc_def_field_offset);
			while (locale_idx--) {
				int tx_rec_len;
				if (is_base) {
					loc_def = skip_base_header(loc_def, NULL, NULL);
				}
				for (;;) {
					unsigned char flags = *loc_def++;
					if (flags & CLM_DATA_FLAG_FLAG2) {
						loc_def++;
					}
					tx_rec_len = CLM_LOC_DSC_TX_REC_LEN +
						((flags &
						CLM_DATA_FLAG_PER_ANT_MASK) >>
						CLM_DATA_FLAG_PER_ANT_SHIFT);
					if (!(flags & CLM_DATA_FLAG_MORE)) {
						break;
					}
					loc_def += 1 + tx_rec_len * (int)(*loc_def);
				}
				loc_def += 1 + tx_rec_len * (int)(*loc_def);
			}
		}
		*(const unsigned char **)
			((char *)locales + loc_type_dscs[loc_type].def_field_offset) = loc_def;
		if (locale_ds_id == DS_MAIN) {
			locales->main_loc_data_bitmask |= (1 << loc_type);
		}
	}
	flags_idx = get_data_sources(ds_id)->reg_flags_idx;
	locales->country_flags = (flags_idx >= 0) ? country_definition->extra[flags_idx] : 0;
	flags_idx = get_data_sources(ds_id)->reg_flags_2_idx;
	locales->country_flags_2 = (flags_idx >= 0) ? country_definition->extra[flags_idx] : 0;
	locales->computed_flags = (unsigned char)ds_id;
	return CLM_RESULT_OK;
}

clm_result_t clm_country_channels(const clm_country_locales_t *locales, clm_band_t band,
	clm_channels_t *valid_channels, clm_channels_t *restricted_channels)
{
	clm_channels_t dummy_valid_channels;
	locale_data_t loc_data;

	if (!locales || ((unsigned)band >= (unsigned)CLM_BAND_NUM)) {
		return CLM_RESULT_ERR;
	}
	if (!restricted_channels && !valid_channels) {
		return CLM_RESULT_OK;
	}
	if (!valid_channels) {
		valid_channels = &dummy_valid_channels;
	}
	if (!get_loc_def(locales, compose_loc_type[band][BH_BASE], &loc_data)) {
		return CLM_RESULT_ERR;
	}
	if (loc_data.def_ptr) {
		get_channels(valid_channels, loc_data.valid_channels,
			loc_data.def_ptr[CLM_LOC_DSC_CHANNELS_IDX],
			loc_data.chan_ranges_bw[CLM_BW_20],
			loc_data.combs[CLM_BW_20]);
		get_channels(restricted_channels, loc_data.restricted_channels,
			loc_data.def_ptr[CLM_LOC_DSC_RESTRICTED_IDX],
			loc_data.chan_ranges_bw[CLM_BW_20],
			loc_data.combs[CLM_BW_20]);
		if (restricted_channels) {
			int i;
			for (i = 0; i < (int)ARRAYSIZE(restricted_channels->bitvec); ++i) {
				restricted_channels->bitvec[i] &= valid_channels->bitvec[i];
			}
		}
	} else {
		get_channels(valid_channels, NULL, 0, NULL, NULL);
		get_channels(restricted_channels, NULL, 0, NULL, NULL);
	}
	return CLM_RESULT_OK;
}

clm_result_t clm_country_flags(const clm_country_locales_t *locales, clm_band_t band,
	unsigned long *ret_flags)
{
	int base_ht_idx;
	locale_data_t base_ht_loc_data[BH_NUM];
	unsigned char base_flags;
	if (!locales || !ret_flags || ((unsigned)band >= (unsigned)CLM_BAND_NUM)) {
		return CLM_RESULT_ERR;
	}
	*ret_flags = (unsigned long)(CLM_FLAG_DFS_NONE | CLM_FLAG_NO_40MHZ | CLM_FLAG_NO_80MHZ |
		CLM_FLAG_NO_80_80MHZ | CLM_FLAG_NO_160MHZ | CLM_FLAG_NO_MIMO);
	if (!fill_base_ht_loc_data(locales, band, base_ht_loc_data, &base_flags)) {
		return CLM_RESULT_ERR;
	}
	switch (base_flags & CLM_DATA_FLAG_DFS_MASK) {
	case CLM_DATA_FLAG_DFS_NONE:
		*ret_flags |= CLM_FLAG_DFS_NONE;
		break;
	case CLM_DATA_FLAG_DFS_EU:
		*ret_flags |= CLM_FLAG_DFS_EU;
		break;
	case CLM_DATA_FLAG_DFS_US:
		*ret_flags |= CLM_FLAG_DFS_US;
		break;
	case CLM_DATA_FLAG_DFS_TW:
		*ret_flags |= CLM_FLAG_DFS_TW;
		break;
	case CLM_DATA_FLAG_DFS_UK:
		*ret_flags |= CLM_FLAG_DFS_UK;
		break;
	case CLM_DATA_FLAG_DFS_JP:
		*ret_flags |= CLM_FLAG_DFS_JP;
		break;
	}
	if (base_flags & CLM_DATA_FLAG_FILTWAR1) {
		*ret_flags |= CLM_FLAG_FILTWAR1;
	}
	for (base_ht_idx = 0; base_ht_idx < (int)ARRAYSIZE(base_ht_loc_data); ++base_ht_idx) {
		unsigned char flags, flags2;
		const unsigned char *tx_rec = base_ht_loc_data[base_ht_idx].def_ptr;

		if (!tx_rec) {
			continue;
		}
		do {
			int num_rec, tx_rec_len;
			MY_BOOL eirp;
			unsigned char bw_idx;
			unsigned long bw_flag_mask = 0;
			const unsigned char *rate_sets = NULL;
			const unsigned short *rate_sets_index = NULL;
			unsigned int base_rate = 0;

			flags = *tx_rec++;
			flags2 = (flags & CLM_DATA_FLAG_FLAG2) ? *tx_rec++ : 0;
			tx_rec_len = CLM_LOC_DSC_TX_REC_LEN + ((flags &
				CLM_DATA_FLAG_PER_ANT_MASK) >> CLM_DATA_FLAG_PER_ANT_SHIFT);
			num_rec = (int)*tx_rec++;
			if (flags2 & (CLM_DATA_FLAG2_WIDTH_EXT | CLM_DATA_FLAG2_OUTER_BW_MASK)) {
				tx_rec += tx_rec_len * num_rec;
				continue;
			}
			bw_idx = bw_width_to_idx[flags & CLM_DATA_FLAG_WIDTH_MASK];
			if (bw_idx == CLM_BW_40) {
				bw_flag_mask = CLM_FLAG_NO_40MHZ;
			} else if (bw_idx == CLM_BW_80) {
				bw_flag_mask = CLM_FLAG_NO_80MHZ;
			} else if (bw_idx == CLM_BW_80_80) {
				bw_flag_mask = CLM_FLAG_NO_80_80MHZ;
			} else if (bw_idx == CLM_BW_160) {
				bw_flag_mask = CLM_FLAG_NO_160MHZ;
			}
			if (tx_rec_len != CLM_LOC_DSC_TX_REC_LEN) {
				*ret_flags |= CLM_FLAG_PER_ANTENNA;
			}
			eirp = (flags & CLM_DATA_FLAG_MEAS_MASK) == CLM_DATA_FLAG_MEAS_EIRP;
			switch (flags2 & CLM_DATA_FLAG2_RATE_TYPE_MASK) {
			case CLM_DATA_FLAG2_RATE_TYPE_HE:
				*ret_flags |= CLM_FLAG_HE;
				tx_rec += num_rec * tx_rec_len;
				continue;
			case CLM_DATA_FLAG2_RATE_TYPE_EXT:
				rate_sets = base_ht_loc_data[base_ht_idx].ext_rate_sets_bw[bw_idx];
				rate_sets_index =
					base_ht_loc_data[base_ht_idx].
					ext_rate_sets_indices_bw[bw_idx];
				base_rate = BASE_EXT_RATE;
				break;
			case CLM_DATA_FLAG2_RATE_TYPE_EXT4:
#if WL_NUMRATES >= 336
				rate_sets = base_ht_loc_data[base_ht_idx].ext4_rate_sets_bw[bw_idx];
				rate_sets_index =
					base_ht_loc_data[base_ht_idx].
					ext4_rate_sets_indices_bw[bw_idx];
				base_rate = BASE_EXT4_RATE;
#else /* WL_NUMRATES >= 336 */
				ASSERT(!"EXT4 rate may not happen when bcmwifi_rates.h is <=3TX");
#endif /* else (WL_NUMRATES >= 336) */
				break;
			default:
				rate_sets = base_ht_loc_data[base_ht_idx].rate_sets_bw[bw_idx];
				rate_sets_index =
					base_ht_loc_data[base_ht_idx].rate_sets_indices_bw[bw_idx];
				base_rate = 0;
				break;
			}
			/* Fail on absent rate set table for a bandwidth when TX limit table is
			 * nonempty (ClmCompiler bug)
			 */
			ASSERT((rate_sets != NULL) || (num_rec == 0));
			for (; num_rec--; tx_rec += tx_rec_len) {
				const unsigned char *rates =
					get_byte_string(rate_sets, tx_rec[CLM_LOC_DSC_RATE_IDX],
					rate_sets_index);
				int num_rates = *rates++;
				/* Check for a non-disabled power before
				 * clearing NO_bw flag
				 */
				if ((unsigned char)CLM_DISABLED_POWER ==
					(unsigned char)tx_rec[CLM_LOC_DSC_POWER_IDX]) {
					continue;
				}
				if (bw_flag_mask) {
					*ret_flags &= ~bw_flag_mask;
					/* clearing once should be enough */
					bw_flag_mask = 0;
				}
				while (num_rates--) {
					unsigned int rate_idx = base_rate + (*rates++);
					switch (RATE_TYPE(rate_idx)) {
					case RT_DSSS:
						if (eirp) {
							*ret_flags |= CLM_FLAG_HAS_DSSS_EIRP;
						}
						break;
					case RT_OFDM:
						if (eirp) {
							*ret_flags |= CLM_FLAG_HAS_OFDM_EIRP;
						}
						break;
					case RT_MCS:
						*ret_flags &= ~CLM_FLAG_NO_MIMO;
						break;
					case RT_SU:
						*ret_flags &= ~CLM_FLAG_NO_MIMO;
						*ret_flags |= CLM_FLAG_HE;
						break;
					}
				}
			}
		} while (flags & CLM_DATA_FLAG_MORE);
	}
	if (locales->country_flags & CLM_DATA_FLAG_REG_TXBF) {
		*ret_flags |= CLM_FLAG_TXBF;
	}
	if (locales->country_flags & CLM_DATA_FLAG_REG_DEF_FOR_CC) {
		*ret_flags |= CLM_FLAG_DEFAULT_FOR_CC;
	}
	if (locales->country_flags & CLM_DATA_FLAG_REG_EDCRS_EU) {
		*ret_flags |= CLM_FLAG_EDCRS_EU;
	}
	if (locales->country_flags_2 & CLM_DATA_FLAG_2_REG_LO_GAIN_NBCAL) {
		*ret_flags |= CLM_FLAG_LO_GAIN_NBCAL;
	}
	if (locales->country_flags_2 & CLM_DATA_FLAG_2_REG_CHSPRWAR2) {
		*ret_flags |= CLM_FLAG_CHSPRWAR2;
	}
	if (base_flags & CLM_DATA_FLAG_PSD_LIMITS) {
		*ret_flags |= CLM_FLAG_PSD;
	}
	if (locales->country_flags & CLM_DATA_FLAG_REG_RED_EU) {
		*ret_flags |= CLM_FLAG_RED_EU;
	}
	return CLM_RESULT_OK;
}

clm_result_t clm_country_advertised_cc(const clm_country_t country, ccode_t advertised_cc)
{
	data_source_id_t ds_id;
	int idx, ds_idx;
	const clm_country_rev_definition_cd10_t *country_def;
	clm_cc_rev4_t cc_rev;

	if (!advertised_cc) {
		return CLM_RESULT_ERR;
	}
	iter_unpack(country, &ds_id, &idx);
	country_def = get_country_def_by_idx(ds_id, idx, NULL);
	if (!country_def) {
		return CLM_RESULT_ERR;
	}
	get_country_ccrev(ds_id, country_def, &cc_rev);
	for (ds_idx = 0; ds_idx < DS_NUM; ++ds_idx) {
		int adv_cc_idx;
		const clm_advertised_cc_t *adv_cc;
		for (adv_cc_idx = 0;
			(adv_cc = (const clm_advertised_cc_t *)GET_ITEM(ds_idx,
			advertised_ccs, clm_advertised_cc_set_t,
			sizeof(clm_advertised_cc_t), adv_cc_idx, NULL)) != NULL;
			++adv_cc_idx)
		{
			int num_aliases = adv_cc->num_aliases;
			int alias_idx;
			const void *alias = (const void *)relocate_ptr(ds_idx, adv_cc->aliases);
			if (num_aliases == CLM_DELETED_NUM) {
				continue;
			}
			if ((ds_idx == DS_MAIN) && get_data_sources(DS_INC)->data) {
				int inc_adv_cc_idx;
				const clm_advertised_cc_t *inc_adv_cc;
				for (inc_adv_cc_idx = 0;
					(inc_adv_cc =
					(const clm_advertised_cc_t *)GET_ITEM(DS_INC,
					advertised_ccs, clm_advertised_cc_set_t,
					sizeof(clm_advertised_cc_t), inc_adv_cc_idx,
					NULL)) != NULL;
					++inc_adv_cc_idx)
				{
					if (cc_equal(adv_cc->cc, inc_adv_cc->cc)) {
						break;
					}
				}
				if (inc_adv_cc) {
					continue;
				}
			}
			for (alias_idx = 0; alias_idx < num_aliases; ++alias_idx) {
				clm_cc_rev4_t alias_ccrev;
				get_ccrev((data_source_id_t)ds_idx, &alias_ccrev, alias, alias_idx);
				if (cc_rev_equal(&alias_ccrev, &cc_rev)) {
					copy_cc(advertised_cc, adv_cc->cc);
					return CLM_RESULT_OK;
				}
			}
		}
	}
	copy_cc(advertised_cc, cc_rev.cc);
	return CLM_RESULT_OK;
}

#if !defined(WLC_CLMAPI_PRE7) || defined(BCMROMBUILD)

/** Precomputes country (region) related data
 * \param[in] locales Region locales
 * \param[out] loc_data Country-related data
 */
static void
get_country_data(const clm_country_locales_t *locales,
	country_data_v2_t *country_data)
{
	data_source_id_t ds_id =
		(data_source_id_t)(locales->computed_flags & COUNTRY_FLAGS_DS_MASK);
	data_dsc_t *ds = get_data_sources(ds_id);
	/* Index of region subchannel rules */
	int rules_idx;
	bzero(country_data, sizeof(*country_data));
	/* Computing subchannel rules index */
	if (ds->scr_idx_4) {
		/* Deprecated 4-bit noncontiguous index field in first region
		 * flag byte
		 */
		rules_idx = remove_extra_bits(
			locales->country_flags & CLM_DATA_FLAG_REG_SC_RULES_MASK_4,
			CLM_DATA_FLAG_REG_SC_RULES_EXTRA_BITS_4);
	} else if (ds->registry_flags & CLM_REGISTRY_FLAG_REGION_FLAG_2) {
		/* New 3+5=8-bit index located in lower 3 and lower 5 bits of
		 * first and second region flag bytes
		 */
		rules_idx = (locales->country_flags & CLM_DATA_FLAG_REG_SC_RULES_MASK) |
			((locales->country_flags_2 & CLM_DATA_FLAG_2_REG_SC_RULES_MASK)
			<< CLM_DATA_FLAG_REG_SC_RULES_MASK_WIDTH);
	} else {
		/* Original 3-bit index */
		rules_idx = locales->country_flags & CLM_DATA_FLAG_REG_SC_RULES_MASK;
	}
	/* Making index 0-based */
	rules_idx -= CLM_SUB_CHAN_IDX_BASE;
	if (rules_idx >= 0) {
		/* Information for shoveling region rules from BLOB to
		 * country_data (band/bandwidth specific)
		 */
		static const struct rules_dsc {
			/* Registry flags ('registry_flags' field) that must be
			 * set for field to be in BLOB
			 */
			unsigned int registry_flags;

			/* Registry flags ('registry_flags2' field) that must be
			 * set for field to be in BLOB
			 */
			unsigned int registry_flags2;

			/* Offset to pointer to clm_sub_chan_rules_set_t
			 * structure in BLOB
			 */
			unsigned int registry_offset;

			/* Offset to destination rules field in country_data */
			unsigned int country_data_rules_offset;

			/* Offset to destination increment field in
			 * country_data
			 */
			unsigned country_data_increments_offset;
		} rules_dscs[] = {
			{CLM_REGISTRY_FLAG_SUB_CHAN_RULES | CLM_REGISTRY_FLAG_SUBCHAN_RULES_40, 0,
			OFFSETOF(clm_registry_t, sub_chan_rules_2g_40m),
			OFFSETOF(country_data_v2_t, sub_chan_channel_rules_40[CLM_BAND_2G]),
			OFFSETOF(country_data_v2_t, sub_chan_increments_40[CLM_BAND_2G])},
			{CLM_REGISTRY_FLAG_SUB_CHAN_RULES | CLM_REGISTRY_FLAG_SUBCHAN_RULES_40, 0,
			OFFSETOF(clm_registry_t, sub_chan_rules_5g_40m),
			OFFSETOF(country_data_v2_t, sub_chan_channel_rules_40[CLM_BAND_5G]),
			OFFSETOF(country_data_v2_t, sub_chan_increments_40[CLM_BAND_5G])},
			{CLM_REGISTRY_FLAG_SUB_CHAN_RULES, 0,
			OFFSETOF(clm_registry_t, sub_chan_rules_80),
			OFFSETOF(country_data_v2_t, sub_chan_channel_rules_80[CLM_BAND_5G]),
			OFFSETOF(country_data_v2_t, sub_chan_increments_80[CLM_BAND_5G])},
			{CLM_REGISTRY_FLAG_SUB_CHAN_RULES | CLM_REGISTRY_FLAG_160MHZ, 0,
			OFFSETOF(clm_registry_t, sub_chan_rules_160),
			OFFSETOF(country_data_v2_t, sub_chan_channel_rules_160[CLM_BAND_5G]),
			OFFSETOF(country_data_v2_t, sub_chan_increments_160[CLM_BAND_5G])},
#ifdef WL_BAND6G
			{CLM_REGISTRY_FLAG_SUB_CHAN_RULES | CLM_REGISTRY_FLAG_SUBCHAN_RULES_40,
			CLM_REGISTRY_FLAG2_6GHZ,
			OFFSETOF(clm_registry_t, sub_chan_rules_6g_40),
			OFFSETOF(country_data_v2_t, sub_chan_channel_rules_40[CLM_BAND_6G]),
			OFFSETOF(country_data_v2_t, sub_chan_increments_40[CLM_BAND_6G])},
			{CLM_REGISTRY_FLAG_SUB_CHAN_RULES, CLM_REGISTRY_FLAG2_6GHZ,
			OFFSETOF(clm_registry_t, sub_chan_rules_6g_80),
			OFFSETOF(country_data_v2_t, sub_chan_channel_rules_80[CLM_BAND_6G]),
			OFFSETOF(country_data_v2_t, sub_chan_increments_80[CLM_BAND_6G])},
			{CLM_REGISTRY_FLAG_SUB_CHAN_RULES | CLM_REGISTRY_FLAG_160MHZ,
			CLM_REGISTRY_FLAG2_6GHZ,
			OFFSETOF(clm_registry_t, sub_chan_rules_6g_160),
			OFFSETOF(country_data_v2_t, sub_chan_channel_rules_160[CLM_BAND_6G]),
			OFFSETOF(country_data_v2_t, sub_chan_increments_160[CLM_BAND_6G])},
#endif /* WL_BAND6G */
		};
		/* Offset in BLOB to this region's rules from
		 * clm_sub_chan_rules_set_t::region_rules
		 */
		unsigned int region_rules_offset = rules_idx *
			((ds->registry_flags & CLM_REGISTRY_FLAG_SUBCHAN_RULES_INC_SEPARATE) ?
			sizeof(clm_sub_chan_region_rules_inc_t) :
			sizeof(clm_sub_chan_region_rules_t));
		/* Index in rules_dscs. Corresponds to band/bandwidth */
		uint ri;
		for (ri = 0; ri < ARRAYSIZE(rules_dscs); ++ri) {
			/* Address of current rules_dsc structure */
			const struct rules_dsc *rds = rules_dscs + ri;
			/* Address of sub_chan_channel_rules_XX field in
			 * destination country_data_t structure
			 */
			clm_sub_chan_region_rules_t *country_data_rules_field =
			(clm_sub_chan_region_rules_t *)((unsigned char *)country_data +
			rds->country_data_rules_offset);
			/* Address of sub_chan_increments_XX field in
			 * destination country_data_t structure
			 */
			const unsigned char **country_data_increments_field =
				(const unsigned char **)((unsigned char *)country_data +
				rds->country_data_increments_offset);
			/* Address of top-level clm_sub_chan_rules_set_t for
			 * current band/bandwidth in BLOB
			 */
			const clm_sub_chan_rules_set_t *blob_rule_set;
			/* Address of this region's
			 * clm_sub_chan_region_rules_inc_t structure in BLOB
			 */
			const clm_sub_chan_region_rules_inc_t *blob_region_rules;
			/* Can BLOB have subchannel rules for current
			 * band/bandwidth?
			 */
			if (((ds->registry_flags & rds->registry_flags) != rds->registry_flags) ||
				((ds->registry_flags2 & rds->registry_flags2) !=
				rds->registry_flags2))
			{
				continue;
			}
			blob_rule_set = (const clm_sub_chan_rules_set_t *)get_data(ds_id,
				rds->registry_offset);
			/* Does BLOB actually have subchannel rules for current
			 * band/bandwidth?
			 */
			if (!blob_rule_set || (rules_idx >= blob_rule_set->num)) {
				continue;
			}
			blob_region_rules = (const clm_sub_chan_region_rules_inc_t *)
				((const unsigned char *)relocate_ptr(ds_id,
				blob_rule_set->region_rules) + region_rules_offset);

			country_data_rules_field->num = blob_region_rules->num;
			country_data_rules_field->channel_rules =
				relocate_ptr(ds_id, blob_region_rules->channel_rules);

			*country_data_increments_field = (ds->registry_flags &
				CLM_REGISTRY_FLAG_SUBCHAN_RULES_INC_SEPARATE) ?
				(const unsigned char *)relocate_ptr(ds_id,
				blob_region_rules->increments) : NULL;
		}
	}
	country_data->chan_ranges_band_bw = ds->chan_ranges_band_bw;
}

/** Fills subchannel table that maps bandwidth to subchannel numbers
 * \param[out] subchannels Subchannel table being filled
 * \param[in] channel Main channel number
 * \param[in] bw Main channel bandwidth (actual type is clm_bandwidth_t)
 * \param[in] limits_type Limits type (subchannel ID)
 * \return TRUE if bandwidth/limits type combination is valid
 */
static MY_BOOL
fill_full_subchan_table(unsigned char subchannels[CLM_BW_NUM], int channel,
int bw, clm_limits_type_t limits_type)
{
	/* Descriptor of path to given subchannel */
	unsigned char path = subchan_paths[limits_type];

	/* Mask that corresponds to current step in path */
	unsigned char path_mask =
		1 << ((path & SUB_CHAN_PATH_COUNT_MASK) >> SUB_CHAN_PATH_COUNT_OFFSET);

	/* Channel number stride for current bandwidth */
	int stride = (CHAN_STRIDE << (bw - CLM_BW_20)) / 2;

	/* Emptying the map */
	bzero(subchannels, sizeof(unsigned char) * CLM_BW_NUM);
	for (;;) {
		/* Setting channel number for current bandwidth */
		subchannels[bw--] = (unsigned char)channel;

		/* Rest will related to previous (halved) bandwidth, i.e. to
		 * subchannel
		 */

		/* Is path over? */
		if ((path_mask >>= 1) == 0) {
			return TRUE; /* Yes - success */
		}
		/* Path not over but we passed through minimum bandwidth? */
		if (bw < 0) {
			return FALSE; /* Yes, failure */
		}

		/* Halving channel stride */
		stride >>= 1;

		/* Selecting subchannel number according to path */
		if (path & path_mask) {
			channel += stride;
		} else {
			channel -= stride;
		}
	}
}

/* Preliminary observations.
 * Every power in *limits is a minimum over values from several sources:
 * - For main (full) channel - over per-channel-range limits from limits that
 *   contain this channel. There can be legitimately more than one such limit:
 *   e.g. one EIRP and another Conducted (this was legal up to certain moment,
 *   not legal now but may become legal again in future)
 * - For subchannels it might be minimum over several enclosing channels (e.g.
 *   for 20-in-80 it may be minimum over corresponding 20MHz main (full)
 *   channel and 40MHz enclosing main (full) channel). Notice that all
 *   enclosing channels have different channel numbers (e.g. for 36LL it is
 *   40MHz enclosing channel 38 and 80MHz enclosing channel 42)
 * - 2.4GHz 20-in-40 channels also take power targets for DSSS rates from 20MHz
 *   channel data (even though other limits are taken from enclosing 40MHz
 *   channel)
 *
 * So in general resulting limit is a minimum of up to 3 channels (one per
 * bandwidth) and these channels have different numbers!
 * 'bw_to_chan' vector contains mapping from bandwidths to channel numbers.
 * Bandwidths not used in limit computation are mapped to 0.
 * 20-in-40 DSSS case is served by 'channel_dsss' variable, that when nonzero
 * contains number of channel where from DSSS limits shall be taken.
 *
 * 'chan_offsets' mapping is initially computed to derive all these channel
 * numbers from main channel number, main channel bandwidth and power limit
 * type (i.e. subchannel ID)  is computed. Also computation of chan_offsets is
 * used to determine if bandwidth/limits_type pair is valid.
*/
/** Processing normal (non-OFDMA) rate limits of single TX rate group
 * In parameter names below, 'tx_' prefix means that it is characteristic of
 * given rate group
 * \param[in,out] per_rate_limits Per-rate power limits being computed
 * \param[in]tx_rec_len Length of records in group
 * \param[in]tx_num_rec Number of recortds in group
 * \param[in]tx_flags First flag byte for group
 * \param[in]tx_bw Bandwidth for channels in group
 * \param[in]tx_power_idx Index of power byte in records of group
 * \param[in]tx_base_rate Base rate for records in group
 * \param[in]tx_channel_for_bw Channel in groups' bandwidth that corresponds to
 * channel in question (i.e. channel in question itself or containing channel
 * in group)
 * \param[in]tx_comb_set_for_bw Comb set for bandwidth of channels in group
 * \param[in]tx_rate_sets List of rate sets for channels in group
 * \param[in]tx_rate_sets_index NULL or index or rate sets for channels in group
 * \param[in]tx_channel_rangesList of channel ranges for channels in group
 * \param[out]valid_channel True if any powers were found
 * \param[in]params clm_[ru_]limits() parameters
 * \param[in]ant_gain Antenns gain
 * \param[in]power_inc Power increment from subchannel rule
 * \param[in]comb_sets Comb sets for all bandwidths
 * \param[in]channel_dsss Channel for DSSS rates of 20-in-40 power targets (0
 * if irrelevant)
 */
static void
process_tx_rate_group(clm_power_t *per_rate_limits, const unsigned char *tx_rec,
	int tx_rec_len, int tx_num_rec, unsigned char tx_flags,
	clm_bandwidth_t tx_bw, int tx_power_idx,
	unsigned int tx_base_rate, int tx_channel_for_bw,
	const clm_channel_comb_set_t *tx_comb_set_for_bw,
	const unsigned char *tx_rate_sets, const unsigned short *tx_rate_sets_index,
	const clm_channel_range_t *tx_channel_ranges,
	MY_BOOL *valid_channel, const clm_limits_params_t *params, int ant_gain,
	clm_power_t power_inc, const clm_channel_comb_set_t *const* comb_sets,
	int channel_dsss)
{
	/* Nothing interesting can be found in this group */
	if ((!(tx_channel_for_bw || (channel_dsss && (tx_bw == CLM_BW_20)))) ||
		((tx_power_idx >= tx_rec_len) && *valid_channel))
	{
		return;
	}
	for (; tx_num_rec--; tx_rec += tx_rec_len)	{
		/* Channel range for current transmission power record */
		const clm_channel_range_t *range =
			tx_channel_ranges + tx_rec[CLM_LOC_DSC_RANGE_IDX];

		/* Power targets for current transmission power record
		 * - original and incremented per subchannel rule
		 */
		clm_power_t qdbm, qdbm_inc;

		/* Per-antenna record without a limit for given antenna index?
		 */
		if (tx_power_idx >= tx_rec_len) {
			/* At least check if channel is valid */
			if (!*valid_channel && tx_channel_for_bw &&
				channel_in_range(tx_channel_for_bw, range, tx_comb_set_for_bw,
				params->other_80_80_chan) &&
				((unsigned char)tx_rec[CLM_LOC_DSC_POWER_IDX] !=
				(unsigned char)CLM_DISABLED_POWER))
			{
				*valid_channel = TRUE;
				/* Return to skip the rest of group */
				return;
			}
		}
		qdbm_inc = qdbm = (clm_power_t)tx_rec[tx_power_idx];
		if ((unsigned char)qdbm != (unsigned char)CLM_DISABLED_POWER) {
			if ((tx_flags & CLM_DATA_FLAG_MEAS_MASK) == CLM_DATA_FLAG_MEAS_EIRP) {
				qdbm -= (clm_power_t)ant_gain;
				qdbm_inc = qdbm;
			}
			qdbm_inc += power_inc;
			/* Apply SAR limit */
			qdbm = (clm_power_t)((qdbm > params->sar) ? params->sar : qdbm);
			qdbm_inc = (clm_power_t)((qdbm_inc > params->sar)
				? params->sar : qdbm_inc);
		}

		/* If this record related to channel for this bandwidth? */
		if (tx_channel_for_bw &&
			channel_in_range(tx_channel_for_bw, range, tx_comb_set_for_bw,
			params->other_80_80_chan))
		{
			/* Rate indices  for current records' rate set */
			const unsigned char *rates = get_byte_string(tx_rate_sets,
				tx_rec[CLM_LOC_DSC_RATE_IDX], tx_rate_sets_index);
			int num_rates = *rates++;
			/* Loop over this TX power record's rate indices */
			while (num_rates--) {
				unsigned int rate_idx = *rates++ + tx_base_rate;
				clm_power_t *pp = per_rate_limits + rate_idx;
				/* Chosing minimum (if CLF_SCR_MIN) or latest power */
				if ((!channel_dsss || (RATE_TYPE(rate_idx) != RT_DSSS)) &&
					((*pp ==
					(clm_power_t)(unsigned char)UNSPECIFIED_POWER) ||
					((*pp > qdbm_inc) &&
					(*pp !=
					(clm_power_t)(unsigned char)CLM_DISABLED_POWER))))
				{
					*pp = qdbm_inc;
				}
			}
			if ((unsigned char)qdbm_inc != (unsigned char)CLM_DISABLED_POWER) {
				*valid_channel = TRUE;
			}
		}
		/* If this rule related to 20-in-something DSSS channel? */
		if (channel_dsss && (tx_bw == CLM_BW_20) &&
			channel_in_range(channel_dsss, range, comb_sets[CLM_BW_20], 0))
		{
			/* Same as above */
			const unsigned char *rates = get_byte_string(tx_rate_sets,
				tx_rec[CLM_LOC_DSC_RATE_IDX], tx_rate_sets_index);
			int num_rates = *rates++;
			while (num_rates--) {
				unsigned int rate_idx = *rates++ + tx_base_rate;
				clm_power_t *pp = per_rate_limits + rate_idx;
				if (RATE_TYPE(rate_idx) == RT_DSSS) {
					*pp = qdbm;
				}
			}
		}
	}
}

#ifdef WL_RU_NUMRATES
/** Processing OFDMA rate limits of single TX rate group
 * In parameter names below, 'tx_' prefix means that it is characteristic of
 * given rate group
 * \param[in,out] per_rate_limits Per-rate power limits being computed
 * \param[in]tx_rec_len Length of records in group
 * \param[in]tx_num_rec Number of recortds in group
 * \param[in]tx_flags First flag byte for group
 * \param[in]tx_flags2 Second flag byte for group
 * \param[in]tx_bw Bandwidth for channels in group
 * \param[in]tx_power_idx Index of power byte in records of group
 * \param[in]tx_base_rate Base rate for records in group
 * \param[in]tx_channel_for_bw Channel in groups' bandwidth that corresponds to
 * channel in question (i.e. channel in question itself or containing channel
 * in group)
 * \param[in]tx_comb_set_for_bw Comb set for bandwidth of channels in group
 * \param[in]tx_rate_sets List of rate sets for channels in group
 * \param[in]tx_rate_sets_index NULL or index or rate sets for channels in group
 * \param[in]tx_channel_ranges List of channel ranges for channels in group
 * \param[out]has_he_limit True if ther eis HE0 power for main channel
 * \param[in]params clm_[ru_]limits() parameters
 * \param[in]ant_gain Antenns gain
 * \param[in]limits_type Limits type (channel/subchannel)
 * \param[in] bw_to_chan Maps bandwidths (elements of clm_bw_t enum) from
 * subchannel bandwidth to whole channel bandwidth to correspondent channel
 * numbers, all other bandwidths - to zero
 */
static void
process_tx_ru_rate_group(clm_power_t *per_rate_limits, const unsigned char *tx_rec,
	int tx_rec_len, int tx_num_rec, unsigned char tx_flags,
	unsigned char tx_flags2, clm_bandwidth_t tx_bw, int tx_power_idx,
	unsigned int tx_base_rate, int tx_channel_for_bw,
	const clm_channel_comb_set_t *tx_comb_set_for_bw,
	const unsigned char *tx_rate_sets, const unsigned short *tx_rate_sets_index,
	const clm_channel_range_t *tx_channel_ranges,
	MY_BOOL *has_he_limit, const clm_limits_params_t *params, int ant_gain,
	clm_limits_type_t limits_type, unsigned char bw_to_chan[CLM_BW_NUM])
{
	/** For each bandwidth specifies first (SS1, unexpanded) RU rate with such
	 * minimum bandwidth that is equivalent to some HE0 rate
	 */
	static const clm_ru_rates_t he_bw_to_ru_rate[CLM_BW_NUM] = {
		WL_RU_RATE_1X1_242SS1, WL_RU_RATE_1X1_484SS1,
		WL_RU_RATE_1X1_996SS1, (clm_ru_rates_t)WL_RU_NUMRATES, WL_RU_RATE_1X1_996SS1
	};
	/** Maps shifted CLM_DATA_FLAG2_OUTER_BW_... values to bandwidths.
	 * CLM_DATA_FLAG2_OUTER_BW_SAME mapped to 20MHz, because its checkked
	 * against subchannel table and here test shall just pass (with 20 it
	 * will pass for sure)
	 */
	static const clm_bandwidth_t outer_bw_to_bw[] = {
		CLM_BW_20, CLM_BW_40,
		CLM_BW_80, CLM_BW_160
	};
	clm_bandwidth_t whole_channel_bw = active_channel_bandwidth(params);
	clm_bandwidth_t subchannel_bw = active_channel_bandwidth(params) -
		((subchan_paths[limits_type] & SUB_CHAN_PATH_COUNT_MASK) >>
		SUB_CHAN_PATH_COUNT_OFFSET);
	int half_subchan_width = (CHAN_STRIDE << (subchannel_bw - CLM_BW_20)) / 2;
	/* Bounds of (sub) channel in terms of channel numbers */
	int left_bound = (int)bw_to_chan[subchannel_bw] - half_subchan_width;
	int right_bound = (int)bw_to_chan[subchannel_bw] + half_subchan_width;
	/** Half-width of channels in group in terms of channel numbers */
	int tx_half_width =
		(CHAN_STRIDE << (((tx_bw == CLM_BW_80_80) ? CLM_BW_80 : tx_bw) - CLM_BW_20)) / 2;
	int min_intersection_width = (tx_half_width < half_subchan_width) ?
		(2 * tx_half_width) : 2 * (half_subchan_width);

	if ((!tx_channel_for_bw && (tx_bw > params->bw)) || (tx_power_idx >= tx_rec_len)) {
		/* Nothing interesting can be found in this group */
		return;
	}
	/* Checking if it is RU subchannel record group from higher bandwidth
	 * channel - skip it if so
	 */
	if (((tx_flags2 & CLM_DATA_FLAG2_RATE_TYPE_MASK) == CLM_DATA_FLAG2_RATE_TYPE_HE) &&
		(outer_bw_to_bw[(tx_flags2 & CLM_DATA_FLAG2_OUTER_BW_MASK) >>
		CLM_DATA_FLAG2_OUTER_BW_SHIFT] > whole_channel_bw))
	{
		return;
	}
	for (; tx_num_rec--; tx_rec += tx_rec_len)	{
		/* Channel range for current transmission power record */
		const clm_channel_range_t *range =
			tx_channel_ranges + tx_rec[CLM_LOC_DSC_RANGE_IDX];
		/* Range end channel number */
		int range_end = (tx_bw == CLM_BW_80_80) ? (int)range->start : (int)range->end;

		/* Power targets for current transmission power record */
		clm_power_t qdbm = (clm_power_t)tx_rec[tx_power_idx];
		if ((unsigned char)qdbm != (unsigned char)CLM_DISABLED_POWER) {
			if ((tx_flags & CLM_DATA_FLAG_MEAS_MASK) == CLM_DATA_FLAG_MEAS_EIRP) {
				qdbm -= (clm_power_t)ant_gain;
			}
			/* Apply SAR limit */
			qdbm = (clm_power_t)((qdbm > params->sar) ? params->sar : qdbm);
		}

		/* Record may be for range that is wider than or same width as
		 * (sub)channel - in this case 'tx_channel_for_bw' is nonzero
		 * and test by subchannel table performed, or it may be narrower
		 * than (sub) channel - in this case intersection test is
		 * performed
		 */
		if (tx_channel_for_bw
			? channel_in_range(tx_channel_for_bw, range, tx_comb_set_for_bw,
			params->other_80_80_chan)
			: ranges_intersect(left_bound, right_bound,
				(int)range->start - tx_half_width, range_end + tx_half_width,
				min_intersection_width))
		{
			/* Rate indices  for current records' rate set */
			const unsigned char *rates = get_byte_string(tx_rate_sets,
				tx_rec[CLM_LOC_DSC_RATE_IDX], tx_rate_sets_index);
			int num_rates = *rates++;
			/* Loop over this TX power record's rate indices */
			while (num_rates--) {
				unsigned int rate_idx = *rates++ + tx_base_rate;
				clm_power_t *pp;
				clm_bandwidth_t ru_min_bw;
				if ((tx_flags2 & CLM_DATA_FLAG2_RATE_TYPE_MASK) ==
					CLM_DATA_FLAG2_RATE_TYPE_HE)
				{
					/* OFDMA rate. What kind? */
					ru_min_bw = get_ru_rate_min_bw(rate_idx);
					if (ru_min_bw == CLM_BW_NUM) {
						/* UB/LUB. Ignored for subchannels */
						if ((limits_type == CLM_LIMITS_TYPE_CHANNEL) &&
							(tx_bw == params->bw))
						{
							per_rate_limits[rate_idx] = qdbm;
						}
						continue;
					}
				} else {
					/* SU rate. Only HE0 rates are of interest */
					unsigned int he_tx_mode;
					if ((RATE_TYPE(rate_idx) != RT_SU) ||
						(RATE_TYPE(rate_idx - 1) == RT_SU))
					{
						/* Not a HE0 rate */
						continue;
					}
					if (tx_bw == params->bw) {
						/* HE0 rate for whole channel - this
						 * makes RU limits legitimate
						 */
						*has_he_limit = TRUE;
					}
					/* Computing equivalent RU rate. It is
					 * narrow enough for sure, as it passed
					 * subchannel number test
					 */
					he_tx_mode = get_he_tx_mode(rate_idx);
					rate_idx = he_bw_to_ru_rate[tx_bw];
					if (rate_idx == WL_RU_NUMRATES) {
						/* No such RU rate (e.g. for
						 * HE0 at 160MHz) - skip
						 */
						continue;
					}
					rate_idx += he_tx_mode;
					ru_min_bw = get_ru_rate_min_bw(rate_idx);
				}
				if (ru_min_bw > subchannel_bw) {
					/* RU rate wider than (sub)channel - skip */
					continue;
				}
				pp = per_rate_limits + rate_idx;
				/* If limit for rate was not yet specified or if
				 * recod's channel range completely covers the
				 * channel - overwriting limit for rate (assuming
				 * that rate groups in BLOB sorted appropriately),
				 * otherwise writing minimum value
				 */
				if ((*pp == (clm_power_t)(unsigned char)UNSPECIFIED_POWER) ||
					((((int)range->start - tx_half_width) <= left_bound) &&
					((range_end + tx_half_width) >= right_bound)) ||
					((*pp > qdbm) && (*pp !=
					(clm_power_t)(unsigned char)CLM_DISABLED_POWER)))
				{
					*pp = qdbm;
				}
			}
		}
	}
}
#endif /* WL_RU_NUMRATES */

/** Compute power limits for one locale (base or HT)
 * This function is formerly a part of clm_limits(), that become separate
 * function in process of functions' size limiting action. Thus the above
 * comment is actually related to both clm_limits() (that prepares various
 * parameters and does the postprocessing) and compute_locale_limits() (that
 * performs actual computation of power limits)
 * \param[out] per_rate_limits Per-rate power limits being computed
 * \param[out] valid_channel TRUE if there is at least one enabled power limit
 * \param[in] params Miscellaneous power computation parameters, passed to
 * clm_limits()
 * \param[in] ant_gain Antenna gain in dBi (for use in EIRP limits computation)
 * \param[in] limits_type Subchannel to get limits for
 * \param[in] loc_data Locale-specific helper data, precomputed in clm_limits()
 * \param[in] channel_dsss Channel for DSSS rates of 20-in-40 power targets (0
 * if irrelevant)
 * \param[in] power_inc  Power increment from subchannel rule
 * \param[in] bw_to_chan Maps bandwidths (elements of clm_bw_t enum) used in
 * subchannel rule to channel numbers. Bandwidths not used in subchannel rule
 * mapped to zero
 * \param[in] ru_limits TRUE means that RU limits (for rates defined in
 * clm_ru_rates_t) shall be retrieved, FALSE means that normal limits (for
 * rates defined in clm_ru_rates_t) shall be retrieved
 */
static void
compute_locale_limits(clm_power_t *per_rate_limits,
	MY_BOOL *valid_channel, const clm_limits_params_t *params, int ant_gain,
	clm_limits_type_t limits_type, const locale_data_t *loc_data,
	int channel_dsss, clm_power_t power_inc,
	unsigned char bw_to_chan[CLM_BW_NUM], MY_BOOL ru_limits)
{
	/** Transmission power records' sequence for current locale */
	const unsigned char *tx_rec = loc_data->def_ptr;

	/** Channel combs for given band - vector indexed by channel bandwidth
	 */
	const clm_channel_comb_set_t *const* comb_sets = loc_data->combs;

	/** CLM_DATA_FLAG_ flags for current group of TX power records */
	unsigned char tx_flags, tx_flags2;

	/* Loop over all groups of TX power records */
	do {
		/** Number of records in group */
		int tx_num_rec;

		/** Bandwidth of records in group */
		clm_bandwidth_t tx_bw;

		/** Channel combs for bandwidth used in group.
		 * NULL for 80+80 channel
		 */
		const clm_channel_comb_set_t *tx_comb_set_for_bw;

		/** Channel number to look for bandwidth used in this group */
		int tx_channel_for_bw;

		/** Length of TX power records in current group */
		int tx_rec_len;

		/** Index of TX power inside TX power record */
		int tx_power_idx;

		/** Base address for channel ranges for bandwidth used in
		 * current group
		 */
		const clm_channel_range_t *tx_channel_ranges;

		/** Sequence of rate sets' definition for bw used in this group
		 */
		const unsigned char *tx_rate_sets = NULL;

		/** Index of sequence of rate sets' definition for bw used in
		 * this group
		 */
		const unsigned short *tx_rate_sets_index = NULL;

		/* Base value for rates in current rate set */
		unsigned int tx_base_rate = 0;

		tx_flags = *tx_rec++;
		tx_flags2 = (tx_flags & CLM_DATA_FLAG_FLAG2) ? *tx_rec++ : 0;
		tx_rec_len = CLM_LOC_DSC_TX_REC_LEN +
			((tx_flags & CLM_DATA_FLAG_PER_ANT_MASK) >> CLM_DATA_FLAG_PER_ANT_SHIFT);
		tx_num_rec = (int)*tx_rec++;
		if (tx_num_rec && (!(tx_flags2 & CLM_DATA_FLAG2_WIDTH_EXT)) &&
			((ru_limits ||
			((tx_flags2 & CLM_DATA_FLAG2_RATE_TYPE_MASK)
			!= CLM_DATA_FLAG2_RATE_TYPE_HE))))
		{
			tx_bw = (clm_bandwidth_t)bw_width_to_idx[tx_flags &
				CLM_DATA_FLAG_WIDTH_MASK];
			tx_comb_set_for_bw = comb_sets[tx_bw];
			tx_channel_for_bw = bw_to_chan[tx_bw];
			tx_power_idx = (tx_rec_len == CLM_LOC_DSC_TX_REC_LEN)
				? CLM_LOC_DSC_POWER_IDX
				: antenna_power_offsets[params->antenna_idx];
			tx_channel_ranges = loc_data->chan_ranges_bw[tx_bw];
			switch (tx_flags2 & CLM_DATA_FLAG2_RATE_TYPE_MASK) {
			case CLM_DATA_FLAG2_RATE_TYPE_MAIN:
				tx_base_rate = 0;
				tx_rate_sets = loc_data->rate_sets_bw[tx_bw];
				tx_rate_sets_index = loc_data->rate_sets_indices_bw[tx_bw];
				break;
			case CLM_DATA_FLAG2_RATE_TYPE_EXT:
				tx_base_rate = BASE_EXT_RATE;
				tx_rate_sets = loc_data->ext_rate_sets_bw[tx_bw];
				tx_rate_sets_index = loc_data->ext_rate_sets_indices_bw[tx_bw];
				break;
			case CLM_DATA_FLAG2_RATE_TYPE_EXT4:
#if WL_NUMRATES >= 336
				tx_base_rate = BASE_EXT4_RATE;
				tx_rate_sets = loc_data->ext4_rate_sets_bw[tx_bw];
				tx_rate_sets_index = loc_data->ext4_rate_sets_indices_bw[tx_bw];
#else /* WL_NUMRATES >= 336 */
				ASSERT(!"EXT4 rate may not happen when bcmwifi_rates.h is <=3TX");
#endif /* else (WL_NUMRATES >= 336) */
				break;
			case CLM_DATA_FLAG2_RATE_TYPE_HE:
				tx_base_rate = 0;
				tx_rate_sets = loc_data->he_rate_sets_bw[tx_bw];
				tx_rate_sets_index = loc_data->he_rate_sets_indices_bw[tx_bw];
				break;
			}
			/* Fail on absent rate set table for a bandwidth when
			 * TX limit table is nonempty (ClmCompiler bug)
			 */
			ASSERT(tx_rate_sets != NULL);
			/* Fail on absent channel range table for a bandwidth
			 * when TX limit table is nonempty(ClmCompiler bug)
			 */
			ASSERT(tx_channel_ranges != NULL);
#ifdef WL_RU_NUMRATES
			if (ru_limits) {
				process_tx_ru_rate_group(per_rate_limits, tx_rec, tx_rec_len,
					tx_num_rec, tx_flags, tx_flags2, tx_bw, tx_power_idx,
					tx_base_rate, tx_channel_for_bw, tx_comb_set_for_bw,
					tx_rate_sets, tx_rate_sets_index, tx_channel_ranges,
					valid_channel, params, ant_gain, limits_type, bw_to_chan);
			} else
#else /* WL_RU_NUMRATES */
			(void)limits_type;
#endif /* else WL_RU_NUMRATES */
			{
				process_tx_rate_group(per_rate_limits, tx_rec, tx_rec_len,
					tx_num_rec, tx_flags, tx_bw, tx_power_idx, tx_base_rate,
					tx_channel_for_bw, tx_comb_set_for_bw, tx_rate_sets,
					tx_rate_sets_index, tx_channel_ranges, valid_channel,
					params, ant_gain, power_inc, comb_sets, channel_dsss);
			}
		}
		tx_rec += tx_num_rec * tx_rec_len;
	} while (tx_flags & CLM_DATA_FLAG_MORE);
}

/** Common part of clm_limits() and clm_ru_limits
 * \param[in] locales Country (region) locales' information
 * \param[in] band Frequency band
 * \param[in] channel Channel number (main channel if subchannel limits output
 * is required)
 * \param[in] ant_gain Antenna gain in quarter dBm (used if limit is given in
 * EIRP terms)
 * \param[in] limits_type Subchannel to get limits for
 * \param[in] params Other parameters
 * \param[out] per_rate_limits Buffer for per-rate limits
 * \param[in] num_limits Length of buffer for per-rate limits
 * \param[in] ru_limits TRUE means that RU limits (for rates defined in
 * clm_ru_rates_t) shall be retrieved, FALSE means that normal limits (for
 * rates defined in clm_ru_rates_t) shall be retrieved
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` is
 * null or contains invalid information, or if any other input parameter
 * (except channel) has invalid value, CLM_RESULT_NOT_FOUND if channel has
 * invalid value
 */
static clm_result_t
compute_clm_limits(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, clm_power_t *per_rate_limits,
	unsigned int num_limits, MY_BOOL ru_limits)
{
	/** Locale characteristics for base and HT locales of given band */
	locale_data_t base_ht_loc_data[BH_NUM];

	/** Loop variable. Points first to base than to HT locale */
	int base_ht_idx;

	/** Channel for DSSS rates of 20-in-40 power targets (0 if
	 * irrelevant)
	 */
	int channel_dsss = 0;

	/** Become true if at least one power target was found */
	MY_BOOL valid_channel = FALSE;

	/** Maps bandwidths to subchannel numbers */
	unsigned char subchannels[CLM_BW_NUM];

	/** Country (region) precomputed data */
	country_data_v2_t country_data;

	/** Per-bandwidth channel comb sets taken from same data source as
	 * country definition
	 */
	const clm_channel_comb_set_t *country_comb_sets;

	/* Data set descriptor */
	data_dsc_t *ds;

	/* Simple validity check */
	if (!locales || !per_rate_limits || !num_limits || (num_limits > WL_NUMRATES) ||
		((unsigned)band >= (unsigned)CLM_BAND_NUM) || !params ||
		((unsigned)params->bw >= (unsigned)CLM_BW_NUM) ||
		((unsigned)limits_type >= CLM_LIMITS_TYPE_NUM) ||
		((unsigned)params->antenna_idx >= WL_TX_CHAINS_MAX))
	{
		return CLM_RESULT_ERR;
	}

	/* Fills bandwidth->channel number map */
	if (!fill_full_subchan_table(subchannels, channel, active_channel_bandwidth(params),
		limits_type))
	{
		/** bandwidth/limits_type pair is invalid */
		return CLM_RESULT_ERR;
	}
	if (params->bw == CLM_BW_80_80) {
		subchannels[CLM_BW_80_80] = subchannels[CLM_BW_80];
	}

	ds = get_data_sources((data_source_id_t)(locales->computed_flags & COUNTRY_FLAGS_DS_MASK));

	if ((band == CLM_BAND_2G) && (params->bw != CLM_BW_20) && subchannels[CLM_BW_20] &&
		!ru_limits)
	{
		/* 20-in-something, 2.4GHz. Channel to take DSSS limits from */
		channel_dsss = subchannels[CLM_BW_20];
	}
	memset(per_rate_limits, (unsigned char)UNSPECIFIED_POWER, num_limits);

	/* Computing helper information on locales */
	if (!fill_base_ht_loc_data(locales, band, base_ht_loc_data, NULL)) {
		return CLM_RESULT_ERR;
	}

	/* Obtaining precomputed country data */
	get_country_data(locales, &country_data);

	/* Obtaining comb sets pertinent to data source that contains
	* country
	*/
	country_comb_sets = ds->valid_channel_combs[band];

	/* For base then HT locales do */
	for (base_ht_idx = 0; base_ht_idx < (int)ARRAYSIZE(base_ht_loc_data); ++base_ht_idx) {
		/* Precomputed helper data for current locale */
		const locale_data_t *loc_data = base_ht_loc_data + base_ht_idx;

		/* Same as subchannels, but only has nonzeros for bandwidths,
		 * used by current subchannel rule
		 */
		unsigned char bw_to_chan[CLM_BW_NUM];

		/* Power increment from subchannel rule */
		clm_power_t power_inc = 0;

		/* No transmission power records or lookin for HE0 or HE limits
		 * and locale is base - nothing to do for this locale
		 */
		if ((!loc_data->def_ptr) || (ru_limits && (base_ht_idx == BH_BASE))) {
			continue;
		}

		/* Now computing 'bw_to_chan' - bandwidth to channel map that
		 * determines which channels will be used for limit computation
		 * RU limits require full subchannel table
		 */
		/* Preset to 'no bandwidths' */
		bzero(bw_to_chan, sizeof(bw_to_chan));
		if (!ru_limits) {
			if (limits_type == CLM_LIMITS_TYPE_CHANNEL) {
				/* Main channel case - bandwidth specified as
				 * parameter, channel specified as parameter
				 */
				bw_to_chan[params->bw] = (unsigned char)channel;
			} else if (params->bw == CLM_BW_40) {
				clm_sub_chan_region_rules_t *rules_40 =
					&country_data.sub_chan_channel_rules_40[band];
				if (rules_40->channel_rules) {
					/* Explicit 20-in-40 rules are defined */
					fill_actual_subchan_table(bw_to_chan, &power_inc,
						subchannels, limits_type, channel,
						country_data.chan_ranges_band_bw[band][params->bw],
						&country_comb_sets[CLM_BW_40],
						rules_40,
						country_data.sub_chan_increments_40[band],
						CLM_DATA_SUB_CHAN_MAX_40, ds->registry_flags);
				} else {
					/* Default. Use 20-in-40 'limit from 40MHz' rule */
					bw_to_chan[CLM_BW_40] = (unsigned char)channel;
				}
			} else if (params->bw == CLM_BW_80) {
				fill_actual_subchan_table(bw_to_chan, &power_inc, subchannels,
					limits_type, channel,
					country_data.chan_ranges_band_bw[band][params->bw],
					&country_comb_sets[CLM_BW_80],
					&country_data.sub_chan_channel_rules_80[band],
					country_data.sub_chan_increments_80[band],
					CLM_DATA_SUB_CHAN_MAX_80, ds->registry_flags);
			} else if (params->bw == CLM_BW_80_80) {
				fill_actual_subchan_table(bw_to_chan, &power_inc, subchannels,
					limits_type, channel,
					country_data.chan_ranges_band_bw[band][CLM_BW_80],
					&country_comb_sets[CLM_BW_80],
					&country_data.sub_chan_channel_rules_80[band],
					country_data.sub_chan_increments_80[band],
					CLM_DATA_SUB_CHAN_MAX_80, ds->registry_flags);
				bw_to_chan[CLM_BW_80_80] = bw_to_chan[CLM_BW_80];
				bw_to_chan[CLM_BW_80] = 0;
			} else if (params->bw == CLM_BW_160) {
				fill_actual_subchan_table(bw_to_chan, &power_inc, subchannels,
					limits_type, channel,
					country_data.chan_ranges_band_bw[band][params->bw],
					&country_comb_sets[CLM_BW_160],
					&country_data.sub_chan_channel_rules_160[band],
					country_data.sub_chan_increments_160[band],
					CLM_DATA_SUB_CHAN_MAX_160, ds->registry_flags);
			}
			/* bw_to_chan computed */
		}
		compute_locale_limits(per_rate_limits, &valid_channel, params, ant_gain,
			limits_type, loc_data, channel_dsss, power_inc,
			ru_limits ? subchannels : bw_to_chan, ru_limits);
	}
	if (valid_channel) {
		/* Converting CLM_DISABLED_POWER and UNSPECIFIED_POWER to
		 * WL_RATE_DISABLED
		 */
		clm_power_t *pp = per_rate_limits, *end = pp + num_limits;
		/* Subchannel with complex subchannel rule with all rates
		 * disabled might be falsely detected as valid. For default
		 * (zero) antenna assume that channel is invalid and will look
		 * for refutation
		 */
		if (params->antenna_idx == 0) {
			valid_channel = FALSE;
		}
		do {
			if ((*pp == (clm_power_t)(unsigned char)CLM_DISABLED_POWER) ||
				(*pp == (clm_power_t)(unsigned char)UNSPECIFIED_POWER))
			{
				*pp = (clm_power_t)WL_RATE_DISABLED;
			} else {
				/* Found enabled rate - channel is valid */
				valid_channel = TRUE;
			}
		} while (++pp < end);
	} else if (ru_limits) {
		memset(per_rate_limits, (unsigned char)WL_RATE_DISABLED, num_limits);
	}
	return valid_channel ? CLM_RESULT_OK : CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_limits(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, clm_power_limits_t *limits)
{
	if (!limits) {
		return CLM_RESULT_ERR;
	}
	return compute_clm_limits(locales, band, channel, ant_gain, limits_type, params,
		limits->limit, ARRAYSIZE(limits->limit), FALSE);
}

#ifdef WL_RU_NUMRATES
clm_result_t clm_ru_limits(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, clm_ru_power_limits_t *limits)
{
	if (!limits) {
		return CLM_RESULT_ERR;
	}
	return compute_clm_limits(locales, band, channel, ant_gain, limits_type, params,
		limits->limit, ARRAYSIZE(limits->limit), TRUE);
}
#endif /* WL_RU_NUMRATES */

#if defined(WL_RU_NUMRATES) || defined(WL_NUM_HE_RT)
clm_result_t clm_he_limit_params_init(struct clm_he_limit_params *params)
{
	if (!params) {
		return CLM_RESULT_ERR;
	}
	params->he_rate_type = WL_HE_RT_SU;
	params->tx_mode = WL_TX_MODE_NONE;
	params->nss = 1;
	params->chains = 1;
	return CLM_RESULT_OK;
}
#endif /* defined(WL_RU_NUMRATES) || defined(WL_NUM_HE_RT) */

#if defined(WL_RU_NUMRATES)
clm_result_t clm_available_he_limits(const clm_country_locales_t *locales, clm_band_t band,
	clm_bandwidth_t bandwidth, unsigned int channel,
	clm_limits_type_t limits_type, clm_available_he_limits_result_t *result)
{
	/* Buffers for all kinds of rates' mass retrieval we may need. Unnion
	 * because they'll be used sequentiuially (if at all)
	 */
	union {
		clm_power_t ru_limits[WL_RU_NUMRATES];
		clm_power_t su_limits[WL_NUMRATES];
	} limits;
	clm_limits_params_t limit_params;
	clm_result_t err;
	unsigned int rt_idx;
	if (!result || !locales) {
		return CLM_RESULT_ERR;
	}
	bzero(result, sizeof(*result));
	if (bandwidth == CLM_BW_80_80) {
		return CLM_RESULT_NOT_FOUND;
	}
	err = clm_limits_params_init(&limit_params);
	if (err != CLM_RESULT_OK) {
		return err;
	}
	limit_params.bw = bandwidth;
	/* First retrieve HE rates - they are part of normal rates */
	err = compute_clm_limits(locales, band, channel, 0, limits_type, &limit_params,
		limits.su_limits, ARRAYSIZE(limits.su_limits), FALSE);
	if (err != CLM_RESULT_OK) {
		return err;
	}
	/* ... and looking for SU limits among them */
	for (rt_idx = 0; rt_idx < ARRAYSIZE(su_base_rates); ++rt_idx) {
		const clm_he_rate_dsc_t *su_rate_dsc = he_rate_descriptors + rt_idx;
		wl_tx_mode_t tx_mode;
		if (limits.su_limits[su_base_rates[rt_idx]] == (clm_power_t)WL_RATE_DISABLED) {
			continue;
		}
		result->rate_type_mask |= (1 << WL_HE_RT_SU);
		result->nss_mask |= (1 << su_rate_dsc->nss);
		result->chains_mask |= (1 << su_rate_dsc->chains);
		if (su_rate_dsc->flags & CLM_HE_RATE_FLAG_TXBF) {
			tx_mode = WL_TX_MODE_TXBF;
		} else if ((su_rate_dsc->nss == 1) && (su_rate_dsc->chains > 1)) {
			tx_mode = WL_TX_MODE_CDD;
		} else {
			tx_mode = WL_TX_MODE_NONE;
		}
		result->tx_mode_mask |= 1 << tx_mode;
	}
	/* Now retrieving RU limits ... */
	err = compute_clm_limits(locales, band, channel, 0, limits_type, &limit_params,
		limits.ru_limits, ARRAYSIZE(limits.ru_limits), TRUE);
	if (err == CLM_RESULT_ERR) {
		return err;
	}
	if (err != CLM_RESULT_OK) {
		return result->rate_type_mask ? CLM_RESULT_OK : CLM_RESULT_NOT_FOUND;
	}
	/* ... and looking which RU are defined */
	for (rt_idx = 0; rt_idx < WL_RU_NUMRATES; ++rt_idx) {
		const clm_he_rate_dsc_t *he_rate_dsc;
		wl_he_rate_type_t ru_rate_type;
		wl_tx_mode_t tx_mode;
		if (limits.ru_limits[rt_idx] == (clm_power_t)WL_RATE_DISABLED) {
			continue;
		}
		/* Fail on absent HE rate table when there is enabled HE rate (ClmCompiler bug) */
		get_ru_rate_info((clm_ru_rates_t)rt_idx, &he_rate_dsc, &ru_rate_type);
		result->rate_type_mask |= (1 << ru_rate_type);
		result->nss_mask |= (1 << he_rate_dsc->nss);
		result->chains_mask |= (1 << he_rate_dsc->chains);
		if (he_rate_dsc->flags & CLM_HE_RATE_FLAG_TXBF) {
			tx_mode = WL_TX_MODE_TXBF;
		} else if ((he_rate_dsc->nss == 1) && (he_rate_dsc->chains > 1)) {
			tx_mode = WL_TX_MODE_CDD;
		} else {
			tx_mode = WL_TX_MODE_NONE;
		}
		result->tx_mode_mask |= 1 << tx_mode;
	}
	return CLM_RESULT_OK;
}

clm_result_t clm_he_limit(const clm_country_locales_t *locales, clm_band_t band,
	clm_bandwidth_t bandwidth, unsigned int channel, int ant_gain,
	clm_limits_type_t limits_type, const clm_he_limit_params_t *params,
	clm_he_limit_result_t *result)
{
	union {
		clm_power_t ru_limits[WL_RU_NUMRATES];
		clm_power_t su_limits[WL_NUMRATES];
	} limits;
	clm_limits_params_t limit_params;
	clm_result_t err;
	unsigned int rt_idx;
	/* Checking validity of parameters that will not be checked in compute_clm_limits() */
	if (!params || !result ||
		((unsigned int)params->he_rate_type >= (unsigned int)WL_NUM_HE_RT) ||
		((unsigned int)(params->chains - 1) >= (unsigned int)WL_TX_CHAINS_MAX) ||
		((unsigned int)(params->nss - 1) >= (unsigned int)WL_TX_CHAINS_MAX) ||
		(params->nss > params->chains) ||
		((unsigned int)params->tx_mode >= (unsigned int)WL_NUM_TX_MODES) ||
		(params->tx_mode == WL_TX_MODE_STBC) ||
		((params->chains == 1) && (params->tx_mode != WL_TX_MODE_NONE)) ||
		((params->tx_mode == WL_TX_MODE_CDD) && (params->nss != 1)))
	{
		return CLM_RESULT_ERR;
	}
	result->limit = (clm_power_t)WL_RATE_DISABLED;
	err = clm_limits_params_init(&limit_params);
	if (err != CLM_RESULT_OK) {
		return err;
	}
	limit_params.bw = bandwidth;
	err = compute_clm_limits(locales, band, channel, ant_gain, limits_type, &limit_params,
		(clm_power_t *)&limits,
		(params->he_rate_type == WL_HE_RT_SU) ? WL_NUMRATES : WL_RU_NUMRATES,
		(params->he_rate_type != WL_HE_RT_SU));
	if (err != CLM_RESULT_OK) {
		return err;
	}
	/* Choosing HE rate descriptor (in he_rate_descriptors[]) that matches
	 * all rate parameters except rate type
	 */
	for (rt_idx = 0; rt_idx < ARRAYSIZE(he_rate_descriptors); ++rt_idx) {
		const clm_he_rate_dsc_t *he_rate_dsc = he_rate_descriptors + rt_idx;
		if ((params->nss == he_rate_dsc->nss) &&
			(params->chains == he_rate_dsc->chains) &&
			((params->tx_mode == WL_TX_MODE_TXBF) ==
			((he_rate_dsc->flags & CLM_HE_RATE_FLAG_TXBF) != 0)))
		{
			break;
		}
	}
	if (rt_idx == ARRAYSIZE(he_rate_descriptors)) {
		/* Rate with requested parameters not found - exiting */
		return CLM_RESULT_NOT_FOUND;
	}
	if (params->he_rate_type == WL_HE_RT_SU) {
		/* For SU rates - rate index in limits.su_limits
		 * contained in su_base_rates[]
		 */
		result->limit = limits.su_limits[su_base_rates[rt_idx]];
	} else {
		/* For RU rates rate index in limits.ru_limits computed
		 * by combining rate type and index within rate type,
		 * found above
		 */
		result->limit = limits.ru_limits[rt_idx + (params->he_rate_type - 1) *
			ARRAYSIZE(he_rate_descriptors)];
	}
	return (result->limit == (clm_power_t)WL_RATE_DISABLED) ? CLM_RESULT_NOT_FOUND :
		CLM_RESULT_OK;
}

clm_result_t clm_get_ru_rate_params(clm_ru_rates_t ru_rate, clm_he_limit_params_t *params)
{
	const clm_he_rate_dsc_t *he_rate_dsc;
	if (!params || ((unsigned int)ru_rate >= WL_RU_NUMRATES)) {
		return CLM_RESULT_ERR;
	}
	get_ru_rate_info(ru_rate, &he_rate_dsc, &(params->he_rate_type));
	params->chains = (unsigned int)he_rate_dsc->chains;
	params->nss = (unsigned int)he_rate_dsc->nss;
	if (he_rate_dsc->flags & CLM_HE_RATE_FLAG_TXBF) {
		params->tx_mode = WL_TX_MODE_TXBF;
	} else if ((he_rate_dsc->nss == 1) && (he_rate_dsc->chains > 1)) {
		params->tx_mode = WL_TX_MODE_CDD;
	} else {
		params->tx_mode = WL_TX_MODE_NONE;
	}
	return CLM_RESULT_OK;
}
#elif defined(WL_NUM_HE_RT)

/** Scans TX power limits and calls given callback for HE limits, defined for
 * given channel. Helper function for clm_available_he_limits() and
 * clm_he_limit()
 * \param[in] locales Country (region) locales' information
 * \param[in] band Band of channel in question
 * \param[in] bandwidth Bandwidth of channel in question (of main channel if
 * subchannel is requested). If CLM_BW_NUM then neitehr channel nor bandwidth
 * is checked (i.e. 'fnc' is called for all HE TX power records
 * \param[in] channel Channel number of channel in question (main channel if
 * subchannel is requested). Ignored if 'bandwidth' is CLM_BW_NUM
 * \param[in] limits_type Subchannel in question. Ignored if 'bandwidth' is
 * CLM_BW_NUM
 * \param[in] min_bw
 * \param[in] use_subchan_rules
 * \param[in] ignore_ru_subchannels
 * \param[in] fnc Function to call for TX power record with HE limits of channel
 * in question. Arguments of this function: 'limit' - power limit for current
 * TX record, 'tx_flags' - address of first flag byte of TX records group,
 * 'rates' - sequence of HE rate indices, 'num_rates' - number of rate indices,
 * 'rate_dscs' - array of HE rate descriptors, 'chan_bw' - Bandwidth of channel
 * to which current TX power record belongs to, 'is_main_bw' - TRUE if TX power
 * record related to main channel bandwidth, FALSE if to narrower bandwidth,
 * 'full_coverage' - TRUE if TX record completely covers requested subchannel,
 * 'arg' - argument, passed to scan_he_limits(). Last return value of this
 * function is used as return value of scan_he_limits()
 * \param[in, out] arg - Argument, passed to callback function
 * Returms CLM_RESULT_ERR if some parameters (except channel) were obviously
 * wrong, CLM_RESULT_OK otherwise
 */
static clm_result_t
scan_he_limits(const clm_country_locales_t *locales, clm_band_t band,
	clm_bandwidth_t bandwidth, unsigned int channel, clm_limits_type_t limits_type,
	clm_bandwidth_t min_bw, MY_BOOL use_subchan_rules, MY_BOOL ignore_ru_subchannels,
	void (*fnc) (clm_power_t /* limit */, const unsigned char * /* tx_flags */,
		const unsigned char * /* rates */, unsigned int /* num_rates */,
		const clm_he_rate_dsc_t * /* rate_dscs */, clm_bandwidth_t /* chan_bw */,
		MY_BOOL /* is_main_bw */, MY_BOOL /* full_coverage */, void * /* arg */),
	void *arg)
{
	data_dsc_t *ds;
	locale_data_t ht_loc_data; /* Helper information about locale being traversed */
	/* Mapping of bandwidths (from subchannel up to to main channel) to channel numbers */
	unsigned char subchannels[CLM_BW_NUM];
	unsigned char bw_to_chan[CLM_BW_NUM]; /* actually used subset of 'subchannels' */
	clm_power_t power_inc = 0; /* Power increment from subchannel rule */
	const unsigned char *tx_rec; /* Pointer in TX power records' group */
	clm_bandwidth_t subchannel_bw; /* Bandwidth of requested (sub)channel */
	int half_subchan_width; /* Half width of requested (sub)channel in channel numbers */
	int left_bound, right_bound; /* Bounds of (sub) channel in terms of channel numbers */

	/* Parameter sanity check, filling in 'ht_loc_data' */
	if (!locales || !fnc || ((unsigned)band >= (unsigned)CLM_BAND_NUM) ||
		((unsigned)bandwidth >= (unsigned)CLM_BW_NUM) ||
		((unsigned)limits_type >= CLM_LIMITS_TYPE_NUM) ||
		!get_loc_def(locales, compose_loc_type[band][BH_HT], &ht_loc_data))
	{
		return CLM_RESULT_ERR;
	}
	ds = get_data_sources((data_source_id_t)(locales->computed_flags & COUNTRY_FLAGS_DS_MASK));
	/* 80+80 channel is specified by its 'active' 80MHz subchannel - treat it as 80MHz channel
	*/
	if (bandwidth == CLM_BW_80_80) {
		bandwidth = CLM_BW_80;
	}
	tx_rec = ht_loc_data.def_ptr;
	if (!tx_rec) {
		return CLM_RESULT_NOT_FOUND; /* Locale has no TX power records */
	}
	if (!fill_full_subchan_table(subchannels, (int)channel, (int)bandwidth,
		limits_type))
	{
		/* Filling full subchannel table for given main channel */
		return CLM_RESULT_ERR;
	}
	if (use_subchan_rules) {
		bzero(bw_to_chan, sizeof(bw_to_chan));
		/* Computing 'bw_to_chan' and 'power_inc' */
		if (limits_type == CLM_LIMITS_TYPE_CHANNEL) {
			/* Main channel case - given channel is the only subchannel */
			bw_to_chan[bandwidth] = (unsigned char)channel;
		} else {
			country_data_v2_t country_data;
			const clm_sub_chan_region_rules_t *sub_chan_rules = NULL;
			const signed char *sub_chan_increments = NULL;
			int num_subchannels = 0;
			get_country_data(locales, &country_data);
			switch (bandwidth) {
			case CLM_BW_40:
				sub_chan_rules = &country_data.sub_chan_channel_rules_40[band];
				sub_chan_increments = country_data.sub_chan_increments_40[band];
				num_subchannels = CLM_DATA_SUB_CHAN_MAX_40;
				break;
			case CLM_BW_80:
				sub_chan_rules = &country_data.sub_chan_channel_rules_80[band];
				sub_chan_increments = country_data.sub_chan_increments_80[band];
				num_subchannels = CLM_DATA_SUB_CHAN_MAX_80;
				break;
			case CLM_BW_160:
				sub_chan_rules = &country_data.sub_chan_channel_rules_160[band];
				sub_chan_increments = country_data.sub_chan_increments_160[band];
				num_subchannels = CLM_DATA_SUB_CHAN_MAX_160;
				break;
			default:
				break;
			}
			if ((bandwidth == CLM_BW_40) && !sub_chan_rules->channel_rules) {
				/* Default is to use 40 for 20-in-40 */
				bw_to_chan[CLM_BW_40] = (unsigned char)channel;
			} else if (num_subchannels) {
				fill_actual_subchan_table(bw_to_chan, &power_inc,
					subchannels, limits_type, channel,
					country_data.chan_ranges_band_bw[band][bandwidth],
					&(ds->valid_channel_combs[band][bandwidth]),
					sub_chan_rules, sub_chan_increments,
					num_subchannels, ds->registry_flags);
			}
		}
	} else {
		(void)memcpy_s(bw_to_chan, sizeof(bw_to_chan), subchannels, sizeof(subchannels));
	}
	subchannel_bw = bandwidth -
		((subchan_paths[limits_type] & SUB_CHAN_PATH_COUNT_MASK) >>
		SUB_CHAN_PATH_COUNT_OFFSET);
	half_subchan_width = half_step(subchannel_bw);
	left_bound = (int)bw_to_chan[subchannel_bw] - half_subchan_width;
	right_bound = (int)bw_to_chan[subchannel_bw] + half_subchan_width;
	for (;;) {
		const unsigned char *tx_flags = tx_rec;
		unsigned char flags = *tx_rec++;
		unsigned char flags2 = (flags & CLM_DATA_FLAG_FLAG2) ? *tx_rec++ : 0;
		int tx_rec_len = CLM_LOC_DSC_TX_REC_LEN +
			((flags & CLM_DATA_FLAG_PER_ANT_MASK) >> CLM_DATA_FLAG_PER_ANT_SHIFT);
		int num_rec = (int)*tx_rec++;
		clm_bandwidth_t pg_bw =
			(clm_bandwidth_t)bw_width_to_idx[flags & CLM_DATA_FLAG_WIDTH_MASK];
		clm_bandwidth_t outer_bw =
			(flags2 & CLM_DATA_FLAG2_OUTER_BW_MASK)
			? outer_bw_to_bw[(flags2 & CLM_DATA_FLAG2_OUTER_BW_MASK) >>
			CLM_DATA_FLAG2_OUTER_BW_SHIFT]
			: pg_bw;
		const clm_channel_comb_set_t *comb_set_for_bw = ht_loc_data.combs[pg_bw];
		int channel_for_bw = bw_to_chan[pg_bw];
		const clm_channel_range_t *ranges_for_bw = ht_loc_data.chan_ranges_bw[pg_bw];

		if ((flags2 & CLM_DATA_FLAG2_WIDTH_EXT) ||
			((flags2 & CLM_DATA_FLAG2_RATE_TYPE_MASK) != CLM_DATA_FLAG2_RATE_TYPE_HE) ||
			(ignore_ru_subchannels && (flags2 & CLM_DATA_FLAG2_OUTER_BW_MASK)) ||
			((flags2 & CLM_DATA_FLAG2_OUTER_BW_MASK) &&
			(outer_bw_to_bw[(flags2 & CLM_DATA_FLAG2_OUTER_BW_MASK) >>
			CLM_DATA_FLAG2_OUTER_BW_SHIFT] > bandwidth)) ||
			(!channel_for_bw && ((pg_bw >= subchannel_bw) || (pg_bw < min_bw))))
		{
			tx_rec += tx_rec_len * num_rec;
		} else {
			int half_tx_width = half_step(pg_bw);
			int min_intersection_width = (half_tx_width < half_subchan_width) ?
				(2 * half_tx_width) : (2 * half_subchan_width);
			for (; num_rec--; tx_rec += tx_rec_len) { /* Loop over TX records group */
				const unsigned char *rates; /* Rate indices  for current rate set */
				clm_power_t power; /* Power for current record */
				const clm_channel_range_t *range =
					ranges_for_bw + tx_rec[CLM_LOC_DSC_RANGE_IDX];
				int range_end = (pg_bw == CLM_BW_80_80) ? (int)range->start :
					(int)range->end;
				if (!(channel_for_bw
					? channel_in_range(channel_for_bw, range, comb_set_for_bw,
					0)
					: ranges_intersect(left_bound, right_bound,
					(int)range->start - half_tx_width,
					range_end + half_tx_width, min_intersection_width)))
				{
					continue; /* Channel not in range - skip this record */
				}
				rates = get_byte_string(ht_loc_data.he_rate_sets_bw[pg_bw],
					tx_rec[CLM_LOC_DSC_RATE_IDX],
					ht_loc_data.he_rate_sets_indices_bw[pg_bw]);
				power = (clm_power_t)tx_rec[CLM_LOC_DSC_POWER_IDX];
				if ((unsigned char)power  != (unsigned char)CLM_DISABLED_POWER) {
					power += power_inc;
				}
				fnc(power, tx_flags, rates + 1, *rates, ds->he_rate_dscs, outer_bw,
					outer_bw == bandwidth,
					(((int)range->start - half_tx_width) <= left_bound) &&
					((range_end + half_tx_width) >= right_bound), arg);
			}
		}
		if (!(flags & CLM_DATA_FLAG_MORE)) {
			break;
		}
	}
	return CLM_RESULT_OK;
}

/** scan_he_limits() callback for clm_available_he_limits()
 * \param[in] limit HE power limit (ignored)
 * \param[in] tx_flags Flags of group of HE power records (ignored)
 * \param[in] rates HE rate indices
 * \param[in] num_rates Number of HE rate indices
 * \param[in] rate_dscs HE rate descriptors
 * \param[in] chan_bw Bandwidth of channel to which current TX power record
 * belongs to
 * \param[in] is_main_bw TRUE if TX power record related to main channel
 * bandwidth, FALSE if to narrower bandwidth
 * \param[in] full_coverage - TRUE if TX record completely covers requested
 * subchannel
 * \param[in] arg 'result' parameter, passed to clm_available_he_limits()
 * Returns CLM_RESULT_OK
 */
static void
available_he_limits_scan_fnc(clm_power_t limit, const unsigned char *tx_flags,
	const unsigned char *rates, unsigned int num_rates,
	const clm_he_rate_dsc_t *rate_dscs, clm_bandwidth_t chan_bw,
	MY_BOOL is_main_bw, MY_BOOL full_coverage, void *arg)
{
	clm_available_he_limits_result_t *result = (clm_available_he_limits_result_t *)arg;
	(void)limit;
	(void)tx_flags;
	(void)full_coverage;
	while (num_rates--) {
		const clm_he_rate_dsc_t *rate_dsc = &rate_dscs[*rates++];
		wl_tx_mode_t tx_mode;
		if (rate_dsc->flags & CLM_HE_RATE_FLAG_TXBF) {
			tx_mode = WL_TX_MODE_TXBF;
		} else if ((rate_dsc->nss == 1) && (rate_dsc->chains > 1)) {
			tx_mode = WL_TX_MODE_CDD;
		} else {
			tx_mode = WL_TX_MODE_NONE;
		}
		result->tx_mode_mask |= (1 << tx_mode);
		result->rate_type_mask |= (1 << rate_dsc->rate_type);
		result->nss_mask |= (1 << rate_dsc->nss);
		result->chains_mask |= (1 << rate_dsc->chains);
		if (rate_dsc->rate_type == WL_HE_RT_SU) {
			wl_he_rate_type_t ru_equivalent = he_bw_to_ru[chan_bw];
			if (ru_equivalent != WL_NUM_HE_RT) {
				result->rate_type_mask |= (1 << ru_equivalent);
			}
			if (is_main_bw) {
				result->rate_type_mask |= (1 << WL_HE_RT_SU);
			}
		} else {
			result->rate_type_mask |= (1 << rate_dsc->rate_type);
		}
	}
}

clm_result_t
clm_available_he_limits(const clm_country_locales_t *locales, clm_band_t band,
	clm_bandwidth_t bandwidth, unsigned int channel,
	clm_limits_type_t limits_type, clm_available_he_limits_result_t *result)
{
	clm_result_t ret;
	if (!result) {
		return CLM_RESULT_ERR;
	}
	bzero(result, sizeof(*result));
	ret = scan_he_limits(locales, band, bandwidth, channel, limits_type,
		CLM_BW_20, FALSE, FALSE, available_he_limits_scan_fnc, result);
	if (ret == CLM_RESULT_OK) {
		if (!(result->rate_type_mask & (1 << WL_HE_RT_SU))) {
			bzero(result, sizeof(*result));
			ret = CLM_RESULT_NOT_FOUND;
		} else if (limits_type != CLM_LIMITS_TYPE_CHANNEL) {
			result->rate_type_mask &= ~(1 << WL_HE_RT_UB);
			result->rate_type_mask &= ~(1 << WL_HE_RT_LUB);
		}
	}
	return ret;
}

/** Argument structure for he_limit_scan_fnc */
typedef struct he_limit_scan_arg {
	/** clm_he_limit() 'params' - describes rate to look for */
	const clm_he_limit_params_t *params;
	/** clm_he_limit() result */
	clm_he_limit_result_t *result;
	/* Antenna gain */
	clm_power_t ant_gain;

	/** Ignore coverage and bandwidth, always compute minimum limit */
	MY_BOOL ignore_coverage;

	/**  Maximum outer bandwidth (CLM spreadsheet page, used for limit
	 * computation - may be different fromchannel range bandwidth for
	 * power, specified for subchannels) used in limit computation so far.
	 * Initialized to -2, set to -1 for RU242-996 limit derived from HE
	 * limit
	 */
	int max_outer_bw;

	/** TRUE if at highest outer bandwidth so far the whole (sub)channel
	 * was covered. FALSE means that limit shall be computed as minimum of
	 * all pertinent limits
	 */
	MY_BOOL full_coverage;

	/** HE limit found on main bandwidth */
	MY_BOOL he_found;
} he_limit_scan_arg_t;

/** scan_he_limits() callback for clm_he_limit()
 * \param[in] limit HE power limit
 * \param[in] tx_flags Flags of group of HE power records
 * \param[in] rates HE rate indices
 * \param[in] num_rates Number of HE rate indices
 * \param[in] rate_dscs HE rate descriptors
 * \param[in] chan_bw Bandwidth of channel to which current TX power record
 * belongs to
 * \param[in] is_main_bw TRUE if TX power record related to main channel
 * bandwidth, FALSE if to narrower bandwidth
 * \param[in] full_coverage - TRUE if TX record completely covers requested
 * subchannel
 * \param[in] arg Address of he_limit_scan_arg_t structure
 */
static void
he_limit_scan_fnc(clm_power_t limit, const unsigned char *tx_flags,
	const unsigned char *rates, unsigned int num_rates,
	const clm_he_rate_dsc_t *rate_dscs, clm_bandwidth_t chan_bw,
	MY_BOOL is_main_bw, MY_BOOL full_coverage, void *arg)
{
	he_limit_scan_arg_t *hlsa = (he_limit_scan_arg_t *)arg;
	const clm_he_limit_params_t *params = hlsa->params;
	clm_he_limit_result_t *result = hlsa->result;
	if (((*tx_flags & CLM_DATA_FLAG_MEAS_MASK) == CLM_DATA_FLAG_MEAS_EIRP) &&
		(limit != (clm_power_t)(unsigned)CLM_DISABLED_POWER))
	{
		limit -= hlsa->ant_gain;
	}
	while (num_rates--) {
		const clm_he_rate_dsc_t *rate_dsc = &rate_dscs[*rates++];
		wl_he_rate_type_t ru_equivalent = (wl_he_rate_type_t)WL_NUM_HE_RT;
		int outer_bw = (int)chan_bw;
		if (rate_dsc->rate_type == WL_HE_RT_SU) {
			if (is_main_bw) {
				hlsa->he_found = TRUE;
			}
			ru_equivalent = he_bw_to_ru[chan_bw];
			outer_bw = -1;
		}
		if (((params->he_rate_type == (wl_he_rate_type_t)rate_dsc->rate_type) ||
			(params->he_rate_type == ru_equivalent)) &&
			(params->nss == rate_dsc->nss) && (params->chains == rate_dsc->chains) &&
			((params->tx_mode == WL_TX_MODE_TXBF) ==
			((rate_dsc->flags & CLM_HE_RATE_FLAG_TXBF) != 0)))
		{
			if (!hlsa->ignore_coverage) {
				if (hlsa->full_coverage && (hlsa->max_outer_bw > outer_bw)) {
					continue;
				}
				if ((hlsa->max_outer_bw == outer_bw) ||
					((hlsa->max_outer_bw > outer_bw) && !hlsa->full_coverage))
				{
					full_coverage = FALSE;
				}
			}
			if ((full_coverage && !hlsa->ignore_coverage) ||
				(result->limit == (clm_power_t)(unsigned)UNSPECIFIED_POWER) ||
				((result->limit != (clm_power_t)(unsigned)CLM_DISABLED_POWER) &&
				(limit < result->limit)))
			{
				result->limit = limit;
			}
			if (!hlsa->ignore_coverage) {
				if (full_coverage || (outer_bw > hlsa->max_outer_bw)) {
					hlsa->max_outer_bw = outer_bw;
				}
				hlsa->full_coverage = full_coverage;
			}
		}
	}
}

clm_result_t
clm_he_limit(const clm_country_locales_t *locales, clm_band_t band,
	clm_bandwidth_t bandwidth, unsigned int channel, int ant_gain,
	clm_limits_type_t limits_type, const clm_he_limit_params_t *params,
	clm_he_limit_result_t *result)
{
	he_limit_scan_arg_t arg;
	clm_result_t ret;
	clm_bandwidth_t subchannel_bw, ru_bw;
	MY_BOOL is_ru_ul;
	if (!params || !result || ((unsigned)bandwidth >= (unsigned)CLM_BW_NUM) ||
		((unsigned int)params->he_rate_type >= (unsigned int)WL_NUM_HE_RT) ||
		((unsigned int)(params->chains - 1) >= (unsigned int)WL_TX_CHAINS_MAX) ||
		((unsigned int)(params->nss - 1) >= (unsigned int)WL_TX_CHAINS_MAX) ||
		(params->nss > params->chains) ||
		((unsigned int)params->tx_mode >= (unsigned int)WL_NUM_TX_MODES))
	{
		return CLM_RESULT_ERR;
	}
	subchannel_bw = ((bandwidth == CLM_BW_80_80) ? CLM_BW_80 : bandwidth) -
		(int)((subchan_paths[limits_type] & SUB_CHAN_PATH_COUNT_MASK) >>
		SUB_CHAN_PATH_COUNT_OFFSET);
	ru_bw = min_ru_bw[params->he_rate_type];
	is_ru_ul = (ru_bw != CLM_BW_NUM) && (params->he_rate_type != WL_HE_RT_SU);
	if ((ru_bw == CLM_BW_NUM) ? (limits_type != CLM_LIMITS_TYPE_CHANNEL)
		: (ru_bw > subchannel_bw))
	{
		result->limit = (clm_power_t)WL_RATE_DISABLED;
		return CLM_RESULT_NOT_FOUND;
	}
	arg.params = params;
	arg.result = result;
	arg.ant_gain = (clm_power_t)ant_gain;
	arg.max_outer_bw = -2;
	arg.full_coverage = FALSE;
	arg.ignore_coverage = !is_ru_ul;
	arg.he_found = FALSE;
	result->limit = (clm_power_t)(unsigned)UNSPECIFIED_POWER;
	ret = scan_he_limits(locales, band, bandwidth, channel, limits_type,
		is_ru_ul ? ru_bw : subchannel_bw, (params->he_rate_type == WL_HE_RT_SU),
		!is_ru_ul, he_limit_scan_fnc, &arg);
	if ((result->limit == (clm_power_t)(unsigned)UNSPECIFIED_POWER) ||
		((params->he_rate_type != WL_HE_RT_SU) && !arg.he_found))
	{
		result->limit = (clm_power_t)WL_RATE_DISABLED;
		ret = CLM_RESULT_NOT_FOUND;
	}
	return ret;
}
#endif /* WL_NUM_HE_RT */

/** Retrieves information about channels with valid power limits for locales of
 * some region
 * \param[in] locales Country (region) locales' information
 * \param[out] valid_channels Valid 5GHz channels
 * \return CLM_RESULT_OK in case of success, CLM_RESULT_ERR if `locales` is
 * null or contains invalid information, CLM_RESULT_NOT_FOUND if channel has
 * invalid value
 */
clm_result_t clm_valid_channels_5g(const clm_country_locales_t *locales,
	clm_channels_t *channels20, clm_channels_t *channels4080)
{
	/** Locale characteristics for base and HT locales of given band */
	locale_data_t base_ht_loc_data[BH_NUM];
	/** Loop variable */
	int base_ht_idx;

	/* Check pointers' validity */
	if (!locales || !channels20 || !channels4080) {
		return CLM_RESULT_ERR;
	}
	/* Clearing output parameters */
	bzero(channels20, sizeof(*channels20));
	bzero(channels4080, sizeof(*channels4080));

	/* Computing helper information on locales */
	if (!fill_base_ht_loc_data(locales, CLM_BAND_5G, base_ht_loc_data, NULL)) {
		return CLM_RESULT_ERR;
	}

	/* For base then HT locales do */
	for (base_ht_idx = 0; base_ht_idx < (int)ARRAYSIZE(base_ht_loc_data); ++base_ht_idx) {
		/** Precomputed helper data for current locale */
		const locale_data_t *loc_data = base_ht_loc_data + base_ht_idx;

		/** Channel combs for given band - vector indexed by channel
		 * bandwidth
		 */
		const clm_channel_comb_set_t *const* comb_sets = loc_data->combs;

		/** Transmission power records' sequence for current locale */
		const unsigned char *tx_rec = loc_data->def_ptr;

		/** CLM_DATA_FLAG_ flags for current group */
		unsigned char flags, flags2;

		/* No transmission power records - nothing to do for this
		 * locale
		 */
		if (!tx_rec) {
			continue;
		}

		/* Loop over all TX records' groups */
		do {
			/** Number of records in group */
			int num_rec;
			/* Bandwidth of records in group */
			clm_bandwidth_t pg_bw;
			/* Channel combs for bandwidth used in group */
			const clm_channel_comb_set_t *comb_set_for_bw;
			/* Length of TX power records in current group */
			int tx_rec_len;
			/* Vector of channel ranges' definition */
			const clm_channel_range_t *ranges;
			clm_channels_t *channels;

			flags = *tx_rec++;
			flags2 = (flags & CLM_DATA_FLAG_FLAG2) ? *tx_rec++ : 0;
			tx_rec_len = CLM_LOC_DSC_TX_REC_LEN +
				((flags & CLM_DATA_FLAG_PER_ANT_MASK) >>
				CLM_DATA_FLAG_PER_ANT_SHIFT);
			num_rec = (int)*tx_rec++;
			if (flags2 & (CLM_DATA_FLAG2_WIDTH_EXT | CLM_DATA_FLAG2_OUTER_BW_MASK)) {
				tx_rec += num_rec * tx_rec_len;
				continue;
			}
			pg_bw = (clm_bandwidth_t)bw_width_to_idx[flags & CLM_DATA_FLAG_WIDTH_MASK];
			if (pg_bw == CLM_BW_80_80) {
				tx_rec += num_rec * tx_rec_len;
				continue;
			}
			if (pg_bw == CLM_BW_20)
				channels = channels20;
			else
				channels = channels4080;

			ranges = loc_data->chan_ranges_bw[pg_bw];
		        /* Fail on absent range table when there is nonempty
			 * limits table for current bandwidth (ClmCompiler bug)
			 */
			ASSERT((ranges != NULL) || (num_rec == 0));

			/* Loop over all records in group */

			comb_set_for_bw = comb_sets[pg_bw];

			for (; num_rec--; tx_rec += tx_rec_len)
			{

				/* Channel range for current transmission power
				 * record
				 */
				const clm_channel_range_t *range = ranges +
					tx_rec[CLM_LOC_DSC_RANGE_IDX];

				int num_combs;
				const clm_channel_comb_set_t *combs = loc_data->combs[pg_bw];
				const clm_channel_comb_t *comb;
				/* Fail on absent combs table for present bandwidth
				 * (ClmCompiler bug)
				 */
				ASSERT(combs != NULL);
				comb = combs->set;
				/* Fail on NULL pointer to nonempty comb set (ClmCompiler bug) */
				ASSERT((comb != NULL) || (combs->num == 0));

				/* Check for a non-disabled power before
				 * clearing NO_bw flag
				 */
				if ((unsigned char)CLM_DISABLED_POWER ==
					(unsigned char)tx_rec[CLM_LOC_DSC_POWER_IDX])
				{
					continue;
				}

				for (num_combs = comb_set_for_bw->num; num_combs--; ++comb) {
					int chan;
					for (chan = comb->first_channel; chan <= comb->last_channel;
						chan += comb->stride) {
						if (chan && channel_in_range(chan, range,
							comb_set_for_bw, 0))
						{
							channels->bitvec[chan / 8] |=
								(unsigned char)
								(1 << (chan % 8));
						}
					}
				}
			}
		} while (flags & CLM_DATA_FLAG_MORE);
	}
	return CLM_RESULT_OK;
}

clm_result_t clm_channels_params_init(clm_channels_params_t *params)
{
	if (!params) {
		return CLM_RESULT_ERR;
	}
	params->bw = CLM_BW_20;
	params->this_80_80 = 0;
	params->add = 0;
	return CLM_RESULT_OK;
}

clm_result_t clm_valid_channels(const clm_country_locales_t *locales, clm_band_t band,
	const clm_channels_params_t *params, clm_channels_t *channels)
{
	/** Locale characteristics for base and HT locales of given band */
	locale_data_t base_ht_loc_data[BH_NUM];
	/** Index for looping over base and HT locales */
	int base_ht_idx;
	/** Return value */
	clm_result_t ret = CLM_RESULT_NOT_FOUND;

	/* Check parameters' validity */
	if (!locales || !channels || !params ||
		((unsigned int)band >= (unsigned int)CLM_BAND_NUM) ||
		((unsigned)params->bw >= (unsigned int)CLM_BW_NUM) ||
		((params->this_80_80 != 0) != (params->bw == CLM_BW_80_80)))
	{
		return CLM_RESULT_ERR;
	}

	if (params->add == 0) {
		/* Clearing output parameters */
		bzero(channels, sizeof(*channels));
	}

	/* Computing helper information on locales */
	if (!fill_base_ht_loc_data(locales, band, base_ht_loc_data, NULL)) {
		return CLM_RESULT_ERR;
	}

	/* For base then HT locales do */
	for (base_ht_idx = 0; base_ht_idx < (int)ARRAYSIZE(base_ht_loc_data); ++base_ht_idx) {
		/** Precomputed helper data for current locale */
		const locale_data_t *loc_data = base_ht_loc_data + base_ht_idx;

		/** Channel combs for given band - vector indexed by channel
		 * bandwidth
		 */
		const clm_channel_comb_set_t *const* comb_sets = loc_data->combs;

		/** Transmission power records' sequence for current locale */
		const unsigned char *tx_rec = loc_data->def_ptr;

		/** CLM_DATA_FLAG_ flags for current group */
		unsigned char flags, flags2;

		/* No transmission power records - nothing to do for this
		 * locale
		 */
		if (!tx_rec) {
			continue;
		}

		/* Loop over all TX record groups */
		do {
			/** Number of records in group */
			int num_rec;
			/** Bandwidth of records in subsgroupequence */
			clm_bandwidth_t pg_bw;
			/** Channel combs for bandwidth used in group */
			const clm_channel_comb_set_t *comb_set_for_bw;
			/** Length of TX power records in current group */
			int tx_rec_len;
			/** Vector of channel ranges' definition */
			const clm_channel_range_t *ranges;

			flags = *tx_rec++;
			flags2 = (flags & CLM_DATA_FLAG_FLAG2) ? *tx_rec++ : 0;
			num_rec = (int)*tx_rec++;
			tx_rec_len = CLM_LOC_DSC_TX_REC_LEN +
				((flags & CLM_DATA_FLAG_PER_ANT_MASK) >>
				CLM_DATA_FLAG_PER_ANT_SHIFT);
			if (flags2 & (CLM_DATA_FLAG2_WIDTH_EXT | CLM_DATA_FLAG2_OUTER_BW_MASK)) {
				tx_rec += num_rec * tx_rec_len;
				continue;
			}
			pg_bw = (clm_bandwidth_t)bw_width_to_idx[flags & CLM_DATA_FLAG_WIDTH_MASK];
			/* If not bandwidth we are looking for - skip to next
			 * group of TX records
			 */
			if (pg_bw != params->bw) {
				tx_rec += num_rec * tx_rec_len;
				continue;
			}

			ranges = loc_data->chan_ranges_bw[pg_bw];
			/* Fail on absent range set for nonempty limit table for
			 * current bandwidth (ClmCompiler bug)
			 */
			ASSERT((ranges != NULL) || (num_rec == 0));
			comb_set_for_bw = comb_sets[pg_bw];
			/* Fail on NULL pointer to nonempty comb set (ClmCompiler bug) */
			ASSERT((comb_set_for_bw->set != NULL) || (comb_set_for_bw->num == 0));
			/* Loop over all records in subgroup */
			for (; num_rec--; tx_rec += tx_rec_len)
			{
				/** Channel range for current transmission power
				 * record
				 */
				const clm_channel_range_t *range = ranges +
					tx_rec[CLM_LOC_DSC_RANGE_IDX];

				int num_combs;
				const clm_channel_comb_t *comb;

				/* Skip disabled power record */
				if ((unsigned char)CLM_DISABLED_POWER ==
					(unsigned char)tx_rec[CLM_LOC_DSC_POWER_IDX])
				{
					continue;
				}
				/* 80+80 ranges are special - they are channel
				 * pairs
				 */
				if (params->this_80_80) {
					if (range->start == params->this_80_80) {
						channels->bitvec[range->end / 8] |=
							(unsigned char)
							(1 << (range->end % 8));
						ret = CLM_RESULT_OK;
					}
					continue;
				}
				/* Normal enabled range - loop over all
				 * channels in it
				 */
				for (num_combs = comb_set_for_bw->num, comb = comb_set_for_bw->set;
					num_combs--; ++comb)
				{
					unsigned char chan;
					if ((range->end < comb->first_channel) ||
						(range->start > comb->last_channel))
					{
						continue;
					}
					for (chan = comb->first_channel;
						chan <= comb->last_channel;
						chan += comb->stride)
					{
						if (channel_in_range(chan, range,
							comb_set_for_bw, 0))
						{
							channels->bitvec[chan / 8] |=
								(unsigned char)
								(1 << (chan % 8));
							ret = CLM_RESULT_OK;
						}
					}
				}
			}
		} while (flags & CLM_DATA_FLAG_MORE);
	}
	return ret;
}
#endif /* !WLC_CLMAPI_PRE7 || BCMROMBUILD */

/** Common preamble to clm_regulatory_limit() and clm_psd_limit() - validates
 * parameters and retrieves information, pertinent to base locales
 * \param[in] locales Country (region) locales' information
 * \param[in] band Frequency band
 * \param[in] bandwidth Channel bandwidth
 * \param[out] loc_def_ptr Address of base locale for given band
 * \param[out] ranges_ptr 20MHz channel ranges for given band/bandwidth
 * \param[out] comb_set_ptr 20MHz channel combs for given band/bandwidth
 * \return True if input parameters are valid and CLM data for given band exist
 */
static MY_BOOL
get_base_loc_info(const clm_country_locales_t *locales, clm_band_t band,
	clm_bandwidth_t bandwidth, const unsigned char **loc_def_ptr,
	const clm_channel_range_t **ranges_ptr,
	const clm_channel_comb_set_t **comb_set_ptr)
{
	locale_data_t loc_data;
	if (!locales || ((unsigned)band >= (unsigned)CLM_BAND_NUM) ||
		((unsigned)bandwidth >= (unsigned)CLM_BW_NUM))
	{
		return FALSE;
	}
	if (!get_loc_def(locales, compose_loc_type[band][BH_BASE], &loc_data)) {
		return FALSE;
	}
	if (!loc_data.def_ptr) {
		return FALSE;
	}
	*loc_def_ptr = loc_data.def_ptr;
	*ranges_ptr = loc_data.chan_ranges_bw[bandwidth];
	*comb_set_ptr = loc_data.combs[bandwidth];
	return TRUE;
}

clm_result_t clm_regulatory_limit(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, int *limit)
{
	int num_rec;
	const unsigned char *pub_rec = NULL;
	const clm_channel_range_t *ranges = NULL;
	const clm_channel_comb_set_t *comb_set = NULL;
	if (!get_base_loc_info(locales, band, CLM_BW_20, &pub_rec, &ranges, &comb_set) || !limit) {
		return CLM_RESULT_ERR;
	}
	pub_rec += CLM_LOC_DSC_BASE_HDR_LEN;
	/* Fail on absent range set for nonempty limit table (ClmCompiler bug) */
	ASSERT((ranges != NULL) || (*pub_rec == 0));
	for (num_rec = *pub_rec++; num_rec--; pub_rec += CLM_LOC_DSC_PUB_REC_LEN) {
		if (channel_in_range(channel, ranges + pub_rec[CLM_LOC_DSC_RANGE_IDX], comb_set, 0))
		{
			*limit = (int)((const signed char *)pub_rec)[CLM_LOC_DSC_POWER_IDX];
			return CLM_RESULT_OK;
		}
	}
	return CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_psd_limit_params_init(clm_psd_limit_params_t *params)
{
	if (!params) {
		return CLM_RESULT_ERR;
	}
	params->bw = CLM_BW_20;
	return CLM_RESULT_OK;
}

clm_result_t clm_psd_limit(const clm_country_locales_t *locales, clm_band_t band,
	unsigned int channel, int ant_gain, const clm_psd_limit_params_t *params,
	clm_power_t *psd_limit)
{
	unsigned char psd_flags;
	const unsigned char *psd_limits = NULL;
	const clm_channel_range_t *ranges = NULL;
	const clm_channel_comb_set_t *comb_set = NULL;
	if (!params) {
		return CLM_RESULT_ERR;
	}
	if (!params ||
		!get_base_loc_info(locales, band, params->bw, &psd_limits, &ranges, &comb_set) ||
		!psd_limit)
	{
		return CLM_RESULT_ERR;
	}
	skip_base_header(psd_limits, NULL, &psd_limits);
	if (!psd_limits) {
		return CLM_RESULT_NOT_FOUND;
	}
	do {
		int num_rec;
		psd_flags = *psd_limits++;
		num_rec = *psd_limits++;
		if (bw_width_to_idx[psd_flags & CLM_DATA_FLAG_WIDTH_MASK] != params->bw) {
			psd_limits += CLM_LOC_DSC_PSD_REC_LEN * num_rec;
			continue;
		}
		/* Fail on absent range set for nonempty limit table (ClmCompiler bug) */
		ASSERT((ranges != NULL) || (num_rec == 0));
		for (; num_rec--; psd_limits += CLM_LOC_DSC_PSD_REC_LEN) {
			if (channel_in_range(channel, ranges + psd_limits[CLM_LOC_DSC_RANGE_IDX],
				comb_set, 0))
			{
				*psd_limit = (clm_power_t)((const signed char *)psd_limits)
					[CLM_LOC_DSC_POWER_IDX];
				if ((psd_flags & CLM_DATA_FLAG_MEAS_MASK) ==
					CLM_DATA_FLAG_MEAS_EIRP)
				{
					*psd_limit -= (clm_power_t)ant_gain;
				}
				return CLM_RESULT_OK;
			}
		}
	} while (psd_flags & CLM_DATA_FLAG_MORE);
	return CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_agg_country_iter(clm_agg_country_t *agg, ccode_t cc, unsigned int *rev)
{
	data_source_id_t ds_id;
	int idx;
	clm_result_t ret = CLM_RESULT_OK;
	if (!agg || !cc || !rev) {
		return CLM_RESULT_ERR;
	}
	if (*agg == CLM_ITER_NULL) {
		ds_id = DS_INC;
		idx = 0;
	} else {
		iter_unpack(*agg, &ds_id, &idx);
		++idx;
	}
	for (;;) {
		int num_aggregates;
		aggregate_data_t aggregate_data, inc_aggregate_data;
		if (!get_aggregate_by_idx(ds_id, idx, &aggregate_data, &num_aggregates)) {
			if (ds_id == DS_INC) {
				ds_id = DS_MAIN;
				idx = 0;
				continue;
			} else {
				ret = CLM_RESULT_NOT_FOUND;
				idx = (num_aggregates >= 0) ? num_aggregates : 0;
				break;
			}
		}
		if (aggregate_data.num_regions == CLM_DELETED_NUM) {
			++idx;
			continue;
		}
		if ((ds_id == DS_MAIN) && get_aggregate(DS_INC,
			&aggregate_data.def_reg, NULL, &inc_aggregate_data)) {
			++idx;
			continue;
		}
		copy_cc(cc, aggregate_data.def_reg.cc);
		*rev = aggregate_data.def_reg.rev;
		break;
	}
	iter_pack(agg, ds_id, idx);
	return ret;
}

clm_result_t clm_agg_map_iter(const clm_agg_country_t agg, clm_agg_map_t *map, ccode_t cc,
	unsigned int *rev)
{
	data_source_id_t ds_id, mapping_ds_id;
	int agg_idx, mapping_idx;
	aggregate_data_t aggregate_data;
	clm_cc_rev4_t mapping = {"", 0};

	if (!map || !cc || !rev) {
		return CLM_RESULT_ERR;
	}
	iter_unpack(agg, &ds_id, &agg_idx);
	get_aggregate_by_idx(ds_id, agg_idx, &aggregate_data, NULL);
	if (*map == CLM_ITER_NULL) {
		mapping_idx = 0;
		mapping_ds_id = ds_id;
	} else {
		iter_unpack(*map, &mapping_ds_id, &mapping_idx);
		++mapping_idx;
	}
	for (;;) {
		aggregate_data_t cur_agg_data;
		MY_BOOL found = TRUE;
		if (mapping_ds_id == ds_id) {
			cur_agg_data = aggregate_data;
		} else {
			found = get_aggregate(mapping_ds_id, &aggregate_data.def_reg, NULL,
				&cur_agg_data);
		}
		if (found) {
			if (mapping_idx < cur_agg_data.num_regions) {
				get_ccrev(mapping_ds_id, &mapping, cur_agg_data.regions,
					mapping_idx);
			} else {
				found = FALSE;
			}
		}
		if (!found) {
			if (mapping_ds_id == DS_MAIN) {
				iter_pack(map, mapping_ds_id, mapping_idx);
				return CLM_RESULT_NOT_FOUND;
			}
			mapping_ds_id = DS_MAIN;
			mapping_idx = 0;
			continue;
		}
		if (mapping.rev == CLM_DELETED_MAPPING) {
			++mapping_idx;
			continue;
		}
		if ((ds_id == DS_INC) && (mapping_ds_id == DS_MAIN) &&
			get_mapping(DS_INC, &aggregate_data, mapping.cc, NULL))
		{
			++mapping_idx;
			continue;
		}
		copy_cc(cc, mapping.cc);
		*rev = mapping.rev;
		break;
	}
	iter_pack(map, mapping_ds_id, mapping_idx);
	return CLM_RESULT_OK;
}

clm_result_t clm_agg_country_lookup(const ccode_t cc, unsigned int rev,
	clm_agg_country_t *agg)
{
	int ds_idx;
	clm_cc_rev4_t cc_rev;
	if (!cc || !agg) {
		return CLM_RESULT_ERR;
	}
	copy_cc(cc_rev.cc, cc);
	cc_rev.rev = (regrev_t)rev;
	for (ds_idx = 0; ds_idx < DS_NUM; ds_idx++) {
		int agg_idx;
		aggregate_data_t agg_data;
		if (get_aggregate((data_source_id_t)ds_idx, &cc_rev, &agg_idx, &agg_data)) {
			if (agg_data.num_regions == CLM_DELETED_NUM) {
				return CLM_RESULT_NOT_FOUND;
			}
			iter_pack(agg, (data_source_id_t)ds_idx, agg_idx);
			return CLM_RESULT_OK;
		}
	}
	return CLM_RESULT_NOT_FOUND;
}

clm_result_t clm_agg_country_map_lookup(const clm_agg_country_t agg,
	const ccode_t target_cc, unsigned int *rev)
{
	data_source_id_t ds_id;
	int aggregate_idx;
	aggregate_data_t aggregate_data;
	clm_cc_rev4_t mapping;

	if (!target_cc || !rev) {
		return CLM_RESULT_ERR;
	}
	iter_unpack(agg, &ds_id, &aggregate_idx);
	get_aggregate_by_idx(ds_id, aggregate_idx, &aggregate_data, NULL);
	for (;;) {
		if (get_mapping(ds_id, &aggregate_data, target_cc, &mapping)) {
			if (mapping.rev == CLM_DELETED_MAPPING) {
				return CLM_RESULT_NOT_FOUND;
			}
			*rev = mapping.rev;
			return CLM_RESULT_OK;
		}
		if (ds_id == DS_MAIN) {
			return CLM_RESULT_NOT_FOUND;
		}
		ds_id = DS_MAIN;
		if (!get_aggregate(DS_MAIN, &aggregate_data.def_reg, NULL, &aggregate_data)) {
			return CLM_RESULT_NOT_FOUND;
		}
	}
}

const char* clm_get_base_app_version_string(void)
{
	return clm_get_string(CLM_STRING_TYPE_APPS_VERSION,
		CLM_STRING_SOURCE_BASE);
}


const char* clm_get_inc_app_version_string(void)
{
	return clm_get_string(CLM_STRING_TYPE_APPS_VERSION,
		CLM_STRING_SOURCE_INCREMENTAL);
}

const char* clm_get_string(clm_string_type_t string_type,
	clm_string_source_t string_source)
{
	data_source_id_t ds_id =
		((string_source == CLM_STRING_SOURCE_BASE) ||
		((string_source == CLM_STRING_SOURCE_EFFECTIVE) &&
		!get_data_sources(DS_INC)->data))
		? DS_MAIN : DS_INC;
	data_dsc_t *ds = get_data_sources(ds_id);
	const clm_data_header_t *header = ds->header;
	const char* ret = NULL;
	const char* def_value = "";
	/* field_len value shall be long enough to compare with default value.
	 * As default value is "", 1 is enough
	 */
	unsigned field_len = 1;
	/* NULL if data source is invalid or absent */
	if (((unsigned)string_source >= (unsigned)CLM_STRING_SOURCE_NUM) || !ds->data) {
		return NULL;
	}
	switch (string_type) {
	case CLM_STRING_TYPE_DATA_VERSION:
		ret = (ds->registry_flags2 & CLM_REGISTRY_FLAG2_DATA_VERSION_STRINGS)
			? (const char *)GET_DATA(ds_id, clm_version) : header->clm_version;
		break;
	case CLM_STRING_TYPE_COMPILER_VERSION:
		ret = header->compiler_version;
		break;
	case CLM_STRING_TYPE_GENERATOR_VERSION:
		ret = header->generator_version;
		break;
	case CLM_STRING_TYPE_APPS_VERSION:
		if (ds->data->flags & CLM_REGISTRY_FLAG_APPS_VERSION) {
			ret = (ds->registry_flags2 & CLM_REGISTRY_FLAG2_DATA_VERSION_STRINGS)
				? (const char *)GET_DATA(ds_id, apps_version)
				: header->apps_version;
			def_value = CLM_APPS_VERSION_NONE_TAG;
			field_len = sizeof(header->apps_version);
		}
		break;
	case CLM_STRING_TYPE_USER_STRING:
		if (ds->data->flags & CLM_REGISTRY_FLAG_USER_STRING) {
			ret = (const char*)relocate_ptr(ds_id, ds->data->user_string);
		}
		break;
	default:
		return NULL;
	}
	/* If string equals default value - will return NULL */
	if (ret && !strncmp(ret, def_value, field_len)) {
		ret = NULL;
	}
	return ret;
}
