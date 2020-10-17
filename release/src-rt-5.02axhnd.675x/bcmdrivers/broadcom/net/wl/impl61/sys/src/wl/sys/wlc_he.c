/*
 * 802.11ax HE (High Efficiency) STA signaling and d11 h/w manipulation.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_he.c 780087 2019-10-15 09:59:59Z $
 */

#ifdef WL11AX

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <802.11.h>
#include <802.11ax.h>
#include <wl_dbg.h>
#include <wlc_types.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_dump.h>
#include <wlc_iocv_cmd.h>
#include <wlc_bsscfg.h>
#include <wlc_rate.h>
#include <wlc_rspec.h>
#include <wlc_scb.h>
#include <wlc_musched.h>
#include <wlc_he.h>
#include <wlc_assoc.h>
#include <phy_hecap_api.h>
#include <wlc_ap.h>
#include <wlc_dbg.h>
#include <wlc_stf.h>
#include <wlc_ratelinkmem.h>
#include <wlc_pcb.h>
#include <wlc_txbf.h>
#include <wlc_vht.h>
#include <wlc_scb_ratesel.h>
#include <wlc_apps.h>
#include <bcmdevs.h>
#ifdef TESTBED_AP_11AX
#include <wlc_test.h>
#endif /* TESTBED_AP_11AX */
#include <wlc_mutx.h>

#define HE_PPET_MAX_RUCOUNT	3	/* 242, 484 and 996. 2x996 is not supported */
#define HE_PPET_MAX_RUBITMAP	0x7	/* 0111 = 0x7 => 242, 484 and 996. 2x996 is not supported */
#define HE_MAX_PPET_SIZE	(HE_PPE_THRESH_LEN(HE_CAP_MCS_MAP_NSS_MAX, HE_PPET_MAX_RUCOUNT))
#define HE_MAX_HTC_CODES	16

#define HE_NEXT_HTC_IDX(idx)	(((idx) + 1) == HE_MAX_HTC_CODES ? 0 : (idx) + 1)
#define HE_INFO_STR_SZ		128

/* variable length PPE Thresholds field description */
typedef struct wlc_he_ppet {
	uint8 ppe_ths_len;
	uint8 ppe_ths[HE_MAX_PPET_SIZE];
} wlc_he_ppet_t;

/* module info */
struct wlc_he_info {
	wlc_info_t *wlc;
	int scbh;
	int cfgh;

	/* HE capabilities of this node being advertised in outgoing mgmt frames.
	 * Do not modify this while processing incoming mgmt frames.
	 */
	he_mac_cap_t def_mac_cap;	/* default HE MAC capabilities of this node */
	he_phy_cap_t def_phy_cap;	/* default HE PHY capabilities of this node */
	wlc_he_rateset_t he_rateset;	/* Supported MCS per NSS per BW for tx/rx */
	uint he_ratesetsz;		/* size of rateset for OP and CAP ie */
	wlc_he_ppet_t ppe_ths;		/* PPE Thresholds of this node */
	uint8 he_format;		/* range extension support */
	bool bsscolor_disable;		/* BSS coloring disabled? */
	uint8 bsscolor;			/* BSS color to be used in AP mode */
	uint8 update_bsscolor_counter;	/* BSS color running update counter, beacon countdowns */
	uint8 bsscolor_counter;		/* BSS color initial beacon counter */
	uint8 new_bsscolor;		/* New BSS color to be used */
	wlc_rateset_t rateset_filter;	/* externally configured rateset limit. */
	uint8 dynfrag;			/* Current dynamic fragment level */
	uint8 ppet;			/* PPET supported as specified by PHY */
	uint8 ppet_override;		/* PPET mode, auto or override to 0, 8 or 16 us */
	bool bsr_supported;		/* Testbed AP configurable. A-BSR support */
	bool testbed_mode;		/* Tebtbed AP active */
	bool muedca_blocked;		/* Testbed mode default is no muedca */
};

/* debug */
#define WL_HE_ERR(x)	WL_ERROR(x)
#ifdef BCMDBG
#define WL_HE_INFO(x)	WL_INFORM(x)
#else
#define WL_HE_INFO(x)
#endif // endif

typedef struct {
	uint32 codes[HE_MAX_HTC_CODES];
	he_htc_cb_fn_t cb_fn[HE_MAX_HTC_CODES];
	uint8 rd_idx;		/* read index in codes to be transmitted */
	uint8 wr_idx;		/* write index in codes to be transmitted */
	uint8 outstanding;	/* currently HTC code being transmitted? */
} scb_he_htc_t;

/* scb cubby */
/* TODO: compress the structure as much as possible */
typedef struct {
	uint16 flags;		/* converted HE flags */
	he_mac_cap_t mac_cap;	/* capabilities info in the HE Cap IE */
	he_phy_cap_t phy_cap;	/* HE PHY capabilities of this node */
	bool ppet_valid;	/* ppet information in ppe_nss is valid */
	uint32 ppe_nss[HE_CAP_MCS_MAP_NSS_MAX]; /* PPE Thresholds of this node */
	uint32 min_ppe_nss;
	uint8 trig_mac_pad_dur; /* in usec */
	uint8 multi_tid_agg_num;
	scb_he_htc_t htc;	/* Tx HTC information */
	uint16 bw80_tx_mcs_nss;	/* HE Tx mcs nss set BW 80Mhz, supported/capabilities */
	uint16 bw80_rx_mcs_nss;	/* HE Rx mcs nss set BW 80Mhz, supported/capabilities  */
	uint16 bw160_tx_mcs_nss; /* HE Tx mcs nss set BW 160Mhz, supported/capabilities */
	uint16 bw160_rx_mcs_nss; /* HE Rx mcs nss set BW 160Mhz, supported/capabilities */
	uint16 bw80p80_tx_mcs_nss; /* HE Tx mcs nss set BW 80p80Mhz, supported/capabilities */
	uint16 bw80p80_rx_mcs_nss; /* HE Tx mcs nss set BW 80p80Mhz, supported/capabilities */
	uint16 omi_pmq;		/* OMI code as received through PMQ */
	uint16 omi_htc;		/* OMI code as received by HTC+ */
	uint16 omi_lm;		/* OMI code to be stored in linkmem */
	uint8 qnull_retries;	/* counters for qos null retries */
} scb_he_t;

/** per BSS info */
typedef struct {
	he_mu_ac_param_t sta_mu_edca[AC_COUNT];	/* EDCA Parameter set be used by STAs */
	uint16 rts_thresh;		/* AP config for duration of RTS in usec */
	uint8 pe_duration;		/* Default PE Duration */
	uint8 edca_update_count;	/* EDCA Parameter Set Update Count, for STA mode only */
	uint8 bsscolor;			/* BSS color to be used in STA mode */
	bool partial_bsscolor_ind;	/* Partial Bsscolor indication */
	bool bsscolor_disable;		/* BSS coloring disabled? for Rx */
} bss_he_info_t;

/* cubby access macros */
#define SCB_HE_CUBBY(hei, scb)		(scb_he_t **)SCB_CUBBY(scb, (hei)->scbh)
#define SCB_HE(hei, scb)		*SCB_HE_CUBBY(hei, scb)

/* handy macros to access bsscfg cubby & data */
#define BSS_HE_INFO_LOC(hei, cfg)	(bss_he_info_t **)BSSCFG_CUBBY(cfg, (hei)->cfgh)
#define BSS_HE_INFO(hei, cfg)		*BSS_HE_INFO_LOC(hei, cfg)

/* features mask */
#define WL_HE_MAX_ALLOWED_FEATURES	(WL_HE_FEATURES_5G | WL_HE_FEATURES_2G | \
					WL_HE_FEATURES_DLOMU | WL_HE_FEATURES_ULOMU | \
					WL_HE_FEATURES_DLMMU)
/* default features */
static uint32
WL_HE_FEATURES_DEFAULT(wlc_info_t *wlc)
{
	uint32 ret = (WL_HE_FEATURES_5G | WL_HE_FEATURES_2G | WL_HE_FEATURES_DLOMU |
	              WL_HE_FEATURES_ULOMU);
	if (!D11REV_IS(wlc->pub->corerev, 130)) { /* deemed not mature enough on this twig */
		ret |= WL_HE_FEATURES_DLMMU;
	}

	return ret;
}

/* Max. MSDU fragments */
#define WLC_HE_MAX_MSDU_FRAG_ENC	5 /* Max. MSDU fragments receivable by
					* HE STA to be 2^(HE_MAC_MSDU_FRAG_ENC) -1
					*/
/* Max. TID in an AMPDU */
#define WLC_HE_MAX_TID_IN_AMPDU		1

#ifndef HE_DEFAULT_PE_DURATION
#define HE_DEFAULT_PE_DURATION		4	/* in units of 4us */
#endif // endif

#define HE_BW_RX_RASETSZ		2
#define HE_BW_TX_RASETSZ		2
#define HE_BW_RASETSZ			(HE_BW_TX_RASETSZ + HE_BW_RX_RASETSZ)
#define HE_BW80_RX_RATESET_OFFSET	0
#define HE_BW80_TX_RATESET_OFFSET	2
#define HE_BW160_RX_RATESET_OFFSET	4
#define HE_BW160_TX_RATESET_OFFSET	6
#define HE_BW80P80_RX_RATESET_OFFSET	8
#define HE_BW80P80_TX_RATESET_OFFSET	10
#define HE_MIN_HE_RATESETSZ		HE_BW_RASETSZ

#define HE_BSSCOLOR_APIDX		HE_BSSCOLOR_IDX1
#define HE_BSSCOLOR_STAIDX		HE_BSSCOLOR_IDX0
#define HE_BSSCOLOR_MAX_VALUE		0x3f

#define HE_RTS_DUR_THRESHOLD_32USEC_SHIFT	5 /* convert usec <-> 32usec unit */

#define HE_INITIAL_BSSCOLOR_UPDATE_CNT	10

#define HE_MUEDCA_DEFAULT_TIMER		8
#define HE_MUEDCA_INVALID_UPDATE_COUNT	0xff

#define HE_LOWEST_QAM1024_MCS_IDX	10

#define HE_QNULL_RETRY_LIMIT		3

/* Macro to expand a ppet (ppet8 + ppet16 combo) to all 3 used RUs consuming a total of 24 bits */
#define HE_EXPAND_PPET_ALL_RU(ppet) \
(((ppet) << (4 * HE_PPE_THRESH_NSS_RU_FSZ)) | ((ppet) << (2 * HE_PPE_THRESH_NSS_RU_FSZ)) | (ppet))

/* The HT control field in non-wds is located at (FC|DUR|A1|A2|A3|SEQ|QOS|HTC) 2+2+6+6+6+2+2=26 */
#define HTC_CONTROL_OFFSET	26
#define HTC_SIZEOFA4		6
#define HTC_IDENTITY_MASK	0x3		/* See 9.2.4.6 */
#define HTC_IDENTITY_HE		0x3
#define HTC_CODE_SIZE		32		/* HTC code is 32 bits */
#define HTC_VARIANT_SZ		2		/* HTC code variant is 2 bits. */
#define HTC_CONTROL_ID_SZ	4		/* HTC code HE control ID is 4 bits. */

#define HTC_CONTROL_ID_TRS	0		/* HTC Control ID Triggered response scheduling */
#define HTC_CONTROL_TRS_SZ	26
#define HTC_CONTROL_ID_OM	1		/* HTC Control ID Operating mode */
#define HTC_CONTROL_OM_SZ	12
#define HTC_CONTROL_ID_HLA	2		/* HTC Control ID HE link adaptation */
#define HTC_CONTROL_HLA_SZ	26
#define HTC_CONTROL_ID_BSR	3		/* HTC Control ID Buffer status report */
#define HTC_CONTROL_BSR_SZ	26
#define HTC_CONTROL_ID_UPH	4		/* HTC Control ID UL power headroom */
#define HTC_CONTROL_UPH_SZ	8
#define HTC_CONTROL_ID_BQR	5		/* HTC Control ID Bandwidth query report */
#define HTC_CONTROL_BQR_SZ	10
#define HTC_CONTROL_ID_CAS	6		/* HTC Control ID Command and status */
#define HTC_CONTROL_CAS_SZ	8
/* See 9.2.4.6a.2 OM Control: */
#define HTC_OM_CONTROL_RX_NSS_MASK			0x007
#define HTC_OM_CONTROL_RX_NSS_OFFSET			0
#define HTC_OM_CONTROL_CHANNEL_WIDTH_MASK		0x018
#define HTC_OM_CONTROL_CHANNEL_WIDTH_OFFSET		3
#define HTC_OM_CONTROL_UL_MU_DISABLE_MASK		0x020
#define HTC_OM_CONTROL_UL_MU_DISABLE_OFFSET		5
#define HTC_OM_CONTROL_TX_NSTS_MASK			0x1C0
#define HTC_OM_CONTROL_TX_NSTS_OFFSET			6
#define HTC_OM_CONTROL_ER_SU_DISABLE_MASK		0x200
#define HTC_OM_CONTROL_ER_SU_DISABLE_OFFSET		9
#define HTC_OM_CONTROL_DL_MUMIMO_RESOUND_MASK		0x400
#define HTC_OM_CONTROL_DL_MUMIMO_RESOUND_OFFSET		10
#define HTC_OM_CONTROL_UL_MU_DATA_DISABLE_MASK		0x800
#define HTC_OM_CONTROL_UL_MU_DATA_DISABLE_OFFSET	11

#define HE_OMI_ENCODE(rx, tx, bw, er_su, dl_res, ul_dis, ul_data_dis) \
	(((rx) << HTC_OM_CONTROL_RX_NSS_OFFSET) | \
	((tx) << HTC_OM_CONTROL_TX_NSTS_OFFSET) | \
	((bw) << HTC_OM_CONTROL_CHANNEL_WIDTH_OFFSET) | \
	((er_su) << HTC_OM_CONTROL_ER_SU_DISABLE_OFFSET) | \
	((dl_res) << HTC_OM_CONTROL_DL_MUMIMO_RESOUND_OFFSET) | \
	((ul_dis) << HTC_OM_CONTROL_UL_MU_DISABLE_OFFSET) | \
	((ul_data_dis) << HTC_OM_CONTROL_UL_MU_DATA_DISABLE_OFFSET))

#define HE_TXRATE_UPDATE_MASK	(HTC_OM_CONTROL_RX_NSS_MASK | HTC_OM_CONTROL_CHANNEL_WIDTH_MASK)

#define HTC_OM_CONTROL_CHANNEL_WIDTH(omi)	(((omi) & HTC_OM_CONTROL_CHANNEL_WIDTH_MASK) \
	>> HTC_OM_CONTROL_CHANNEL_WIDTH_OFFSET)
#define HTC_OM_CONTROL_RX_NSS(omi)		(((omi) & HTC_OM_CONTROL_RX_NSS_MASK) \
	>> HTC_OM_CONTROL_RX_NSS_OFFSET)
#define HTC_OM_CONTROL_TX_NSTS(omi)		(((omi) & HTC_OM_CONTROL_TX_NSTS_MASK) \
	>> HTC_OM_CONTROL_TX_NSTS_OFFSET)
#define HTC_OM_CONTROL_ER_SU_DISABLE(omi)	(((omi) & HTC_OM_CONTROL_ER_SU_DISABLE_MASK) \
	>> HTC_OM_CONTROL_ER_SU_DISABLE_OFFSET)
#define HTC_OM_CONTROL_DL_MUMIMO_RESOUND(omi)	(((omi) & HTC_OM_CONTROL_DL_MUMIMO_RESOUND_MASK) \
	>> HTC_OM_CONTROL_DL_MUMIMO_RESOUND_OFFSET)
#define HTC_OM_CONTROL_UL_MU_DISABLE(omi)	(((omi) & HTC_OM_CONTROL_UL_MU_DISABLE_MASK) \
	>> HTC_OM_CONTROL_UL_MU_DISABLE_OFFSET)
#define HTC_OM_CONTROL_UL_MU_DATA_DISABLE(omi)	(((omi) & HTC_OM_CONTROL_UL_MU_DATA_DISABLE_MASK) \
	>> HTC_OM_CONTROL_UL_MU_DATA_DISABLE_OFFSET)

/* default mu edca AIFSN setting */
#define HE_MUEDCA_AC_BE_ACI	(EDCF_AC_BE_ACI_STA & ~EDCF_AIFSN_MASK) /* 0x00 */
#define HE_MUEDCA_AC_BK_ACI	(EDCF_AC_BK_ACI_STA & ~EDCF_AIFSN_MASK) /* 0x20 */
#define HE_MUEDCA_AC_VI_ACI	(EDCF_AC_VI_ACI_STA & ~EDCF_AIFSN_MASK) /* 0x40 */
#define HE_MUEDCA_AC_VO_ACI	(EDCF_AC_VO_ACI_STA & ~EDCF_AIFSN_MASK) /* 0x60 */

static const he_mu_ac_param_t default_mu_edca_sta[AC_COUNT] = {
	{ HE_MUEDCA_AC_BE_ACI, EDCF_AC_BE_ECW_STA, HE_MUEDCA_DEFAULT_TIMER },
	{ HE_MUEDCA_AC_BK_ACI, EDCF_AC_BK_ECW_STA, HE_MUEDCA_DEFAULT_TIMER },
	{ HE_MUEDCA_AC_VI_ACI, EDCF_AC_VI_ECW_STA, HE_MUEDCA_DEFAULT_TIMER },
	{ HE_MUEDCA_AC_VO_ACI, EDCF_AC_VO_ECW_STA, HE_MUEDCA_DEFAULT_TIMER }
};

/* local declarations */

/* capabilities */
static void wlc_he_ap_maccap_init(wlc_he_info_t *hei, he_mac_cap_t *ap_mac_cap);
static void wlc_he_sta_maccap_init(wlc_he_info_t *hei, he_mac_cap_t *sta_mac_cap);
static void wlc_he_maccap_init(wlc_he_info_t *hei);
static void wlc_he_ap_phycap_init(wlc_he_info_t *hei, he_phy_cap_t *ap_phy_cap);
static void wlc_he_sta_phycap_init(wlc_he_info_t *hei, he_phy_cap_t *sta_phy_cap);

/* wlc module */
static int wlc_he_wlc_up(void *ctx);
static int wlc_he_doiovar(void *context, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif);
#if defined(BCMDBG)
static int wlc_he_dump(void *ctx, struct bcmstrbuf *b);
#endif // endif
static void BCMFASTPATH wlc_he_htc_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs);

/* bsscfg module */
static int wlc_he_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_he_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
#if defined(BCMDBG)
static void wlc_he_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#endif // endif
static void wlc_he_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *evt);
static void wlc_he_assoc_state_change_cb(void *ctx, bss_assoc_state_data_t *notif_data);

/* scb cubby */
static int wlc_he_scb_init(void *ctx, scb_t *scb);
static void wlc_he_scb_deinit(void *ctx, scb_t *scb);
static uint wlc_he_scb_secsz(void *, scb_t *);
#if defined(BCMDBG)
static void wlc_he_scb_dump(void *ctx, scb_t *scb, struct bcmstrbuf *b);
#endif // endif

/* IE mgmt */
static uint wlc_he_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_he_write_cap_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_he_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_he_write_op_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_he_calc_muedca_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_he_write_muedca_ie(void *ctx, wlc_iem_build_data_t *data);
static uint wlc_he_calc_color_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_he_write_color_ie(void *ctx, wlc_iem_build_data_t *data);
#ifdef TESTBED_AP_11AX
static uint wlc_he_calc_sr_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_he_write_sr_ie(void *ctx, wlc_iem_build_data_t *data);
#endif /* TESTBED_AP_11AX */
static void wlc_he_parse_cap_ie(wlc_he_info_t *hei, scb_t *scb, he_cap_ie_t *cap);
static int wlc_he_assoc_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_he_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_he_parse_muedca_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_he_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data);

/* misc */
static bool wlc_he_hw_cap(wlc_info_t *wlc);
static uint8 wlc_he_get_dynfrag(wlc_info_t *wlc);
static void wlc_he_configure_bsscolor_phy(wlc_he_info_t *hei, wlc_bsscfg_t *cfg);
#ifdef STA
static void wlc_he_sendqosnull(wlc_info_t *wlc, wlc_bsscfg_t *cfg, scb_t *scb);
#endif /* STA */

#ifdef HERTDBG
static void wlc_he_print_rateset(wlc_he_rateset_t *he_rateset);
#endif /* HERTDBG */

/* iovar table */
enum {
	IOV_HE = 0,
	IOV_LAST
};

static const bcm_iovar_t he_iovars[] = {
	{"he", IOV_HE, IOVF_RSDB_SET, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

static int wlc_he_cmd_enab(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_features(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_partial_bsscolor(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_bsscolor(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_rtsdurthresh(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_peduration(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_muedca(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_dynfrag(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_ppet(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_htc(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_axmode(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_omi(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
#ifdef TESTBED_AP_11AX
static int wlc_he_cmd_bsr(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
static int wlc_he_cmd_testbed(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif);
#endif /* TESTBED_AP_11AX */

static int
wlc_he_cmd_cap(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;

	*result = wlc_he_hw_cap(wlc);

	*rlen = sizeof(*result);

	return BCME_OK;
}

uint8 wlc_get_heformat(wlc_info_t *wlc)
{
	return wlc->hei->he_format;
}

uint8 wlc_get_hebsscolor(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bss_he_info_t *bhei;

	bhei = BSS_HE_INFO(wlc->hei, cfg);

	return bhei->bsscolor;
}

static int
wlc_he_cmd_range_ext(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;

	if (set) {
		if (!wlc_he_hw_cap(wlc)) {
			return BCME_UNSUPPORTED;
		}
		if ((*params >= HE_FORMAT_MAX) || (*params == HE_106_TONE_RANGE_EXT)) {
			return BCME_RANGE;
		}

		/* bit mask 0x00 --> range extension disabled
		 * bit mask 0x01 --> 106 tone range extension is supported
		 * bit mask 0x10 --> 242 tone range extension is supported
		 * bit mask 0x11 --> 106 and 242 tone range extension is supported
		 * 4369A0 only supports 242 tone so use h format 1 to represent 242 when
		 * IOVAR issue wl he range ext 2
		 */
		if (*params == HE_242_TONE_RANGE_EXT) {
			hei->he_format = HE_FORMAT_RANGE_EXT;
		}
		else {
			hei->he_format = HE_FORMAT_SU;
		}
	}
	else {

		*result = hei->he_format;

		/* HE format HE_FORMAT_RANGE_EXT represents the  242 tone
		 * hence return HE_242_TONE_RANGE_EXT
		 */
		if (*result == HE_FORMAT_RANGE_EXT)
			*result = HE_242_TONE_RANGE_EXT;

		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

/*  HE cmds  */
static const wlc_iov_cmd_t he_cmds[] = {
	{ WL_HE_CMD_ENAB, IOVF_RSDB_SET | IOVF_SET_DOWN, IOVT_UINT8, wlc_he_cmd_enab },
	{ WL_HE_CMD_FEATURES, IOVF_SET_DOWN, IOVT_UINT32, wlc_he_cmd_features },
	{ WL_HE_CMD_BSSCOLOR, 0, IOVT_BUFFER, wlc_he_cmd_bsscolor },
	{ WL_HE_CMD_PARTIAL_BSSCOLOR, 0, IOVT_UINT8, wlc_he_cmd_partial_bsscolor },
	{ WL_HE_CMD_CAP, 0, IOVT_UINT8, wlc_he_cmd_cap },
	{ WL_HE_CMD_RANGE_EXT, 0, IOVT_UINT8, wlc_he_cmd_range_ext },
	{ WL_HE_CMD_RTSDURTHRESH, 0, IOVT_UINT16, wlc_he_cmd_rtsdurthresh },
	{ WL_HE_CMD_PEDURATION, 0, IOVT_UINT8, wlc_he_cmd_peduration },
	{ WL_HE_CMD_MUEDCA, 0, IOVT_BUFFER, wlc_he_cmd_muedca },
	{ WL_HE_CMD_DYNFRAG, 0, IOVT_UINT8, wlc_he_cmd_dynfrag },
	{ WL_HE_CMD_PPET, 0, IOVT_UINT8, wlc_he_cmd_ppet },
	{ WL_HE_CMD_HTC, 0, IOVT_UINT32, wlc_he_cmd_htc },
	{ WL_HE_CMD_AXMODE, 0, IOVT_BOOL, wlc_he_cmd_axmode },
	{ WL_HE_CMD_OMI, 0, IOVT_BUFFER, wlc_he_cmd_omi },
#ifdef TESTBED_AP_11AX
	{ WL_HE_CMD_BSR_SUPPORT, IOVF_SET_DOWN, IOVT_BOOL, wlc_he_cmd_bsr },
	{ WL_HE_CMD_TESTBED, IOVF_SET_DOWN, IOVT_BOOL, wlc_he_cmd_testbed },
#endif /* TESTBED_AP_11AX */
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
*/
#include <wlc_patch.h>

/* ======== attach/detach ======== */

wlc_he_info_t *
BCMATTACHFN(wlc_he_attach)(wlc_info_t *wlc)
{
	uint16 build_capfstbmp =
		FT2BMP(FC_BEACON) |
		FT2BMP(FC_PROBE_RESP) |
#ifdef STA
		FT2BMP(FC_ASSOC_REQ) |
		FT2BMP(FC_REASSOC_REQ) |
#endif // endif
#ifdef AP
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
#endif // endif
		FT2BMP(FC_PROBE_REQ) |
		0;
	uint16 build_opfstbmp =
		FT2BMP(FC_BEACON) |
		FT2BMP(FC_PROBE_RESP) |
#ifdef AP
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
#endif // endif
		0;
	uint16 build_muedcafstbmp = (
#ifdef AP
		FT2BMP(FC_BEACON) |
		FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
#endif /* AP */
		0);
	uint16 build_colorfstbmp = (
#ifdef AP
		FT2BMP(FC_BEACON) |
		FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
#endif /* AP */
		0);
#ifdef TESTBED_AP_11AX
	uint16 build_spatialreusefstbmp = (
		FT2BMP(FC_BEACON) |
		FT2BMP(FC_PROBE_RESP) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP));
#endif /* TESTBED_AP_11AX */

	uint16 parse_capfstbmp =
#ifdef AP
		FT2BMP(FC_ASSOC_REQ) |
		FT2BMP(FC_REASSOC_REQ) |
#endif // endif
#ifdef STA
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
#endif // endif
		FT2BMP(FC_PROBE_REQ) |
		0;
	uint16 parse_opfstbmp =
#ifdef STA
		FT2BMP(FC_BEACON) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
#endif // endif
		0;
	uint16 parse_muedcafstbmp = (
#ifdef STA
		FT2BMP(FC_BEACON) |
		FT2BMP(FC_ASSOC_RESP) |
		FT2BMP(FC_REASSOC_RESP) |
#endif /* STA */
		0);
	uint16 scanfstbmp =
		FT2BMP(WLC_IEM_FC_SCAN_BCN) |
		FT2BMP(WLC_IEM_FC_SCAN_PRBRSP |
		0);
	wlc_he_info_t *hei;
	scb_cubby_params_t cubby_params;
	bsscfg_cubby_params_t cfg_cubby_params;
	uint8 random;

	/* allocate private module info */
	if ((hei = MALLOCZ(wlc->osh, sizeof(*hei))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	hei->wlc = wlc;

	if (!wlc_he_hw_cap(wlc) ||
#ifdef BCM_HE_DISABLE
		TRUE ||
#endif /* BCM_HE_DISABLE */
		!N_ENAB(wlc->pub)) {
		WL_INFORM(("wl%d: %s: HE functionality disabled \n", wlc->pub->unit, __FUNCTION__));
		wlc->pub->_he_enab = FALSE;
		return hei;
	}

	/* reserve some space in scb for private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = hei;
	cubby_params.fn_init = wlc_he_scb_init;
	cubby_params.fn_deinit = wlc_he_scb_deinit;
	cubby_params.fn_secsz = wlc_he_scb_secsz;
#if defined(BCMDBG)
	cubby_params.fn_dump = wlc_he_scb_dump;
#endif // endif

	if ((hei->scbh = wlc_scb_cubby_reserve_ext(wlc, sizeof(scb_he_t *), &cubby_params)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve_ext() failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	/* register IE mgmt callbacks - calc/build */

	/* bcn/prbreq/prbrsp/assocreq/reassocreq/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, build_capfstbmp, DOT11_MNG_HE_CAP_ID,
			wlc_he_calc_cap_ie_len, wlc_he_write_cap_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, he cap ie\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, build_opfstbmp, DOT11_MNG_HE_OP_ID,
			wlc_he_calc_op_ie_len, wlc_he_write_op_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, he op ie\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, build_muedcafstbmp, DOT11_MNG_MU_EDCA_ID,
			wlc_he_calc_muedca_ie_len, wlc_he_write_muedca_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, he mu-edca ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, build_colorfstbmp, DOT11_MNG_COLOR_CHANGE_ID,
			wlc_he_calc_color_ie_len, wlc_he_write_color_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, he color change ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#ifdef TESTBED_AP_11AX
	/* bcn/prbrsp/assocresp/reassocresp */
	if (wlc_iem_add_build_fn_mft(wlc->iemi, build_spatialreusefstbmp, DOT11_MNG_SRPS_ID,
			wlc_he_calc_sr_ie_len, wlc_he_write_sr_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_build_fn failed, he spatial reuse ie\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* TESTBED_AP_11AX */

	/* register IE mgmt callbacks - parse */

	/* assocreq/reassocreq/assocresp/reassocresp */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, parse_capfstbmp, DOT11_MNG_HE_CAP_ID,
		wlc_he_assoc_parse_cap_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, he cap ie\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
	/* bcn/assocresp/reassocresp */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, parse_opfstbmp, DOT11_MNG_HE_OP_ID,
		wlc_he_parse_op_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, he op ie\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
	/* bcn/assocresp/reassocresp */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, parse_muedcafstbmp, DOT11_MNG_MU_EDCA_ID,
		wlc_he_parse_muedca_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, he mu-edca ie\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}
	/* bcn/prbrsp in scan */
	if (wlc_iem_add_parse_fn_mft(wlc->iemi, scanfstbmp, DOT11_MNG_HE_CAP_ID,
		wlc_he_scan_parse_cap_ie, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_iem_add_parse_fn failed, cap ie in scan\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	/* register packet class callback, used by HTC+ */
	if (wlc_pcb_fn_set(wlc->pcb, 0, WLF2_PCB1_HTC, wlc_he_htc_pkt_freed) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_pcb_fn_set() failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve space for bss data */
	bzero(&cfg_cubby_params, sizeof(cfg_cubby_params));

	cfg_cubby_params.context = hei;
	cfg_cubby_params.fn_init = wlc_he_bss_init;
	cfg_cubby_params.fn_deinit = wlc_he_bss_deinit;
#if defined(BCMDBG)
	cfg_cubby_params.fn_dump = wlc_he_bss_dump;
#endif // endif

	hei->cfgh = wlc_bsscfg_cubby_reserve_ext(wlc, sizeof(bss_he_info_t), &cfg_cubby_params);
	if (hei->cfgh < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve_ext failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	/* bsscfg state change callback */
	if (wlc_bsscfg_state_upd_register(wlc, wlc_he_bsscfg_state_upd, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	/* register assoc state change callback */
	if (wlc_bss_assoc_state_register(wlc, wlc_he_assoc_state_change_cb, hei) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bss_assoc_state_register failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, he_iovars, "he", hei, wlc_he_doiovar, NULL,
		wlc_he_wlc_up, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(BCMDBG)
	/* debug dump */
	wlc_dump_register(wlc->pub, "he", wlc_he_dump, hei);
#endif // endif

	wlc->pub->_he_enab = TRUE;

	WLC_HE_FEATURES_SET(wlc->pub, WL_HE_FEATURES_DEFAULT(wlc));

	/* update FIFOs because we enable MU */
	wlc_hw_update_nfifo(wlc->hw);

	/* Initialize coloring number */
	wlc_getrand(wlc, &random, 1);
	hei->bsscolor = (random % HE_BSSCOLOR_MAX_VALUE) + 1;
	hei->bsscolor_disable = FALSE;
	hei->bsscolor_counter = HE_INITIAL_BSSCOLOR_UPDATE_CNT;

	hei->dynfrag = HE_MAC_FRAG_VHT_MPDU;

	/* Default mode at startup for PPET override is auto */
	hei->ppet_override = WL_HE_PPET_AUTO;

#ifdef TESTBED_AP_11AX
	/* By default BSR support is cleared for testbed AP. */
	hei->bsr_supported = FALSE;
#else
	/* By default BSR support is set. */
	hei->bsr_supported = TRUE;
#endif /* TESTBED_AP_11AX */

	return hei;

fail:
	wlc_he_detach(hei);
	return NULL;
}

void
BCMATTACHFN(wlc_he_detach)(wlc_he_info_t *hei)
{
	wlc_info_t *wlc;

	if (hei == NULL) {
		return;
	}

	wlc = hei->wlc;

	wlc_module_unregister(wlc->pub, "he", hei);

	MFREE(wlc->osh, hei, sizeof(*hei));
}

static void
wlc_he_cp_rateset_to_wlc(wlc_he_rateset_t *he_rateset, wlc_rateset_t *rateset)
{
	rateset->he_bw80_tx_mcs_nss = he_rateset->bw80_tx_mcs_nss;
	rateset->he_bw80_rx_mcs_nss = he_rateset->bw80_rx_mcs_nss;
	rateset->he_bw160_tx_mcs_nss = he_rateset->bw160_tx_mcs_nss;
	rateset->he_bw160_rx_mcs_nss = he_rateset->bw160_rx_mcs_nss;
	rateset->he_bw80p80_tx_mcs_nss = he_rateset->bw80p80_tx_mcs_nss;
	rateset->he_bw80p80_rx_mcs_nss = he_rateset->bw80p80_rx_mcs_nss;
}

static void
wlc_he_cp_he_rateset_to_bi_sup(wlc_bss_info_t *bi, wlc_rateset_t *rateset)
{
	bi->he_sup_bw80_tx_mcs = rateset->he_bw80_tx_mcs_nss;
	bi->he_sup_bw80_rx_mcs = rateset->he_bw80_rx_mcs_nss;
	bi->he_sup_bw160_tx_mcs = rateset->he_bw160_tx_mcs_nss;
	bi->he_sup_bw160_rx_mcs = rateset->he_bw160_rx_mcs_nss;
	bi->he_sup_bw80p80_tx_mcs = rateset->he_bw80p80_tx_mcs_nss;
	bi->he_sup_bw80p80_rx_mcs = rateset->he_bw80p80_rx_mcs_nss;
}

static void
wlc_he_init_bi_rateset_to_none(wlc_bss_info_t *bi)
{
	bi->he_neg_bw80_tx_mcs = HE_CAP_MAX_MCS_NONE_ALL;
	bi->he_neg_bw80_rx_mcs = HE_CAP_MAX_MCS_NONE_ALL;
	bi->he_neg_bw160_tx_mcs = HE_CAP_MAX_MCS_NONE_ALL;
	bi->he_neg_bw160_rx_mcs = HE_CAP_MAX_MCS_NONE_ALL;
	bi->he_neg_bw80p80_tx_mcs = HE_CAP_MAX_MCS_NONE_ALL;
	bi->he_neg_bw80p80_rx_mcs = HE_CAP_MAX_MCS_NONE_ALL;
}

static void
wlc_he_remove_1024qam(wlc_info_t *wlc, uint8 nstreams, uint16 *he_mcs_nss)
{
	uint8 nss, mcs_code;
	for (nss = 1; nss <= nstreams; nss++) {
		mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, *he_mcs_nss);
		if (mcs_code == HE_CAP_MAX_MCS_0_11) {
			HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_0_9, *he_mcs_nss);
		}
	}
}

/*
 * Intersect HE Cap mcsmaps with that of peer's and
 * update rateset with BW, Tx and Rx limitations
 */
static void
wlc_he_intersect_txrxmcsmaps(wlc_he_info_t *hei, scb_t *scb, bool bw_160,
		bool isomi, uint txlimit, uint rxlimit)
{
	scb_he_t *sh;
	wlc_rateset_t *scb_rs;
	uint nss;
	uint16 txmcsmap80, rxmcsmap80, txmcsmap160, rxmcsmap160, txmcsmap80p80, rxmcsmap80p80;

	sh = SCB_HE(hei, scb);
	ASSERT(sh != NULL);

	/* First copy the ratesets, apply possible BW limitations */
	scb_rs = &scb->rateset;

	/* Update tx and rx mcs maps based on intersection of Our cap with SCB's.
	 * Intersection of our Tx and SCB's Rx to get our Tx; Our Rx and SCB's Tx to get our Rx.
	 */
	txmcsmap80 = wlc_he_rateset_intersection(hei->he_rateset.bw80_tx_mcs_nss,
			sh->bw80_rx_mcs_nss);
	rxmcsmap80 = wlc_he_rateset_intersection(hei->he_rateset.bw80_rx_mcs_nss,
			sh->bw80_tx_mcs_nss);
	if (bw_160) {
		txmcsmap160 = wlc_he_rateset_intersection(hei->he_rateset.bw160_tx_mcs_nss,
				sh->bw160_rx_mcs_nss);
		rxmcsmap160 = wlc_he_rateset_intersection(hei->he_rateset.bw160_rx_mcs_nss,
				sh->bw160_tx_mcs_nss);
		txmcsmap80p80 = wlc_he_rateset_intersection(hei->he_rateset.bw80p80_tx_mcs_nss,
				sh->bw80p80_rx_mcs_nss);
		rxmcsmap80p80 = wlc_he_rateset_intersection(hei->he_rateset.bw80p80_rx_mcs_nss,
				sh->bw80p80_tx_mcs_nss);
	} else {
		txmcsmap160 = HE_CAP_MAX_MCS_NONE_ALL;
		rxmcsmap160 = HE_CAP_MAX_MCS_NONE_ALL;
		txmcsmap80p80 = HE_CAP_MAX_MCS_NONE_ALL;
		rxmcsmap80p80 = HE_CAP_MAX_MCS_NONE_ALL;
	}

	/* Now apply NSS txlimit to get final tx rateset */
	for (nss = txlimit + 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, txmcsmap80);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, txmcsmap160);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, txmcsmap80p80);
	}

	if (!isomi) {
		/* Now apply the rx chain limits */
		for (nss = rxlimit + 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
			HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rxmcsmap80);
			HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rxmcsmap160);
			HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rxmcsmap80p80);
		}
	}
	scb_rs->he_bw80_tx_mcs_nss = txmcsmap80;
	scb_rs->he_bw80_rx_mcs_nss = rxmcsmap80;
	scb_rs->he_bw160_tx_mcs_nss = txmcsmap160;
	scb_rs->he_bw160_rx_mcs_nss = rxmcsmap160;
	scb_rs->he_bw80p80_tx_mcs_nss = txmcsmap80p80;
	scb_rs->he_bw80p80_rx_mcs_nss = rxmcsmap80p80;

	WL_RATE(("wl%d: %s txmcsmap80 0x%x rxmcsmap80 0x%x txmcsmap160 0x%x rxmcsmap160 0x%x"
			" txmcsmap80p80 0x%x rxmcsmap80p80 0x%x\n",
			hei->wlc->pub->unit, __FUNCTION__, txmcsmap80, rxmcsmap80, txmcsmap160,
			rxmcsmap160, txmcsmap80p80, rxmcsmap80p80));
}

/**
 * initialize the given rateset with HE defaults based upon band type.
 */
static void
wlc_he_init_rateset(wlc_he_info_t *hei, wlc_rateset_t *rateset, uint8 band_idx, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc =  hei->wlc;
	uint nss;
	uint rxstreams = wlc_stf_rxstreams_get(wlc);

	BCM_REFERENCE(cfg);

	if (!HE_ENAB_BAND(wlc->pub, wlc->bandstate[band_idx]->bandtype)) {
		wlc_rateset_he_none_all(rateset);
		return;
	}

	wlc_he_cp_rateset_to_wlc(&hei->he_rateset, rateset);
	/* In the 2G band only 80Mhz rateset is possible/allowed */
	if (band_idx == BAND_2G_INDEX) {
		rateset->he_bw160_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
		rateset->he_bw160_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
		rateset->he_bw80p80_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
		rateset->he_bw80p80_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	}
	/* Now apply the rx and tx chain limits */
	for (nss = rxstreams + 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80_rx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw160_rx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80p80_rx_mcs_nss);
	}
	for (nss = WLC_BITSCNT(wlc->stf->txchain) + 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80_tx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw160_tx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80p80_tx_mcs_nss);
	}
	/* Apply possible external limit (wl rateset) */
	rateset->he_bw80_tx_mcs_nss = wlc_he_rateset_intersection(rateset->he_bw80_tx_mcs_nss,
		hei->rateset_filter.he_bw80_tx_mcs_nss);
	rateset->he_bw80_rx_mcs_nss = wlc_he_rateset_intersection(rateset->he_bw80_rx_mcs_nss,
		hei->rateset_filter.he_bw80_rx_mcs_nss);
	rateset->he_bw160_tx_mcs_nss = wlc_he_rateset_intersection(rateset->he_bw160_tx_mcs_nss,
		hei->rateset_filter.he_bw160_tx_mcs_nss);
	rateset->he_bw160_rx_mcs_nss = wlc_he_rateset_intersection(rateset->he_bw160_rx_mcs_nss,
		hei->rateset_filter.he_bw160_rx_mcs_nss);
	rateset->he_bw80p80_tx_mcs_nss = wlc_he_rateset_intersection(rateset->he_bw80p80_tx_mcs_nss,
		hei->rateset_filter.he_bw80p80_tx_mcs_nss);
	rateset->he_bw80p80_rx_mcs_nss = wlc_he_rateset_intersection(rateset->he_bw80p80_rx_mcs_nss,
		hei->rateset_filter.he_bw80p80_rx_mcs_nss);
}

/**
 * Apply OMI settings on rateset from scb hei and update the scb rateset. OMI params are based upon
 * received params: BW is of type DOT11_OPER_MODE_.. omi_rxnss inidicates the remote Rx caps and
 * determines our tx nss, where 0 means 1 nss and max is 7 (indicating 8 nss).
 *
 */
static void
wlc_he_apply_omi(wlc_info_t *wlc, scb_t *scb, uint8 omi_bw, uint8 omi_rxnss, bool update_vht)
{
	wlc_he_info_t *hei = wlc->hei;
	uint8 oper_mode;

	/* Intersect and update rateset */
	wlc_he_intersect_txrxmcsmaps(hei, scb, (omi_bw == DOT11_OPER_MODE_160MHZ) ? TRUE : FALSE,
		TRUE, omi_rxnss + 1, 0);

	/* This is all what is needed within HE. The BW limitation of 20 or 40 in the final
	 * rate table will be determined by wlc_scb_ratesel_init. The BW limitation is stored
	 * in (VHT) oper_mode info (even if received through HTC) and gets applied by that setting
	 */
	if (update_vht) {
		/* Determine matching oper_mode for VHT and lower rates. */
		oper_mode = DOT11_D8_OPER_MODE(0, omi_rxnss + 1, 0,
			(omi_bw == DOT11_OPER_MODE_160MHZ) ?  1 : 0,
			(omi_bw == DOT11_OPER_MODE_160MHZ) ? DOT11_OPER_MODE_80MHZ : omi_bw);
		wlc_vht_update_scb_oper_mode(wlc->vhti, scb, oper_mode);
	} else {
		wlc_scb_ratesel_init(wlc, scb);
	}
}

/**
 * Returns whether or not the capabilities should be limited to 80Mhz operation. This depends on
 * a number of settings from wlc and bsscfg. Return true if limitation should be applied.
 */
static bool
wlc_he_bw80_limited(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	if (BAND_2G(wlc->band->bandtype)) {
		return TRUE;
	}
	/* No 160MHz and 80p80 capability advertisement if
	 * - the locale does not allow 160MHz or
	 * - the band is not configured to support it or
	 * - chanspec was not 160MHz or 80+80Mhz for AP/IBSS
	 */
	if ((wlc_channel_locale_flags(wlc->cmi) & WLC_NO_160MHZ) ||
	    (!WL_BW_CAP_160MHZ(wlc->band->bw_cap))) {
		return TRUE;
	}
	if ((cfg != NULL) && (!cfg->BSS) && !BSSCFG_IS_TDLS(cfg)) {
		return TRUE;
	}

	return FALSE;
}

/**
 * initialize the HE rates for the def_rateset and hw_rateset.
 */
static void
wlc_he_default_ratesets(wlc_he_info_t *hei, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc =  hei->wlc;
	wlc_rateset_t *rateset;
	uint8 band_idx;

	for (band_idx = 0; band_idx < NBANDS(wlc); band_idx++) {
		if (IS_SINGLEBAND_5G(wlc->deviceid, wlc->phy_cap)) {
			band_idx = BAND_5G_INDEX;
		}
		rateset = &wlc->bandstate[band_idx]->defrateset;
		wlc_he_init_rateset(hei, rateset, band_idx, cfg);
		rateset = &wlc->bandstate[band_idx]->hw_rateset;
		wlc_he_init_rateset(hei, rateset, band_idx, cfg);
	}
}

/**
 * Update the mcs maps, as an external influence (like tx/rx chain config) may require so.
 */
void
wlc_he_update_mcs_cap(wlc_he_info_t *hei)
{
	wlc_info_t *wlc =  hei->wlc;
	uint8 idx;
	wlc_bsscfg_t *cfg;

	wlc_he_default_ratesets(hei, NULL);
	FOREACH_UP_AP(wlc, idx, cfg) {
		wlc_bss_info_t *bi = cfg->current_bss;
		wlc_he_init_rateset(hei, &bi->rateset, wlc->band->bandunit, cfg);
		wlc_he_cp_he_rateset_to_bi_sup(bi,
			&wlc->bandstate[wlc->band->bandunit]->hw_rateset);
	}
	/* update beacon/probe resp for AP */
	if (wlc->pub->up && AP_ENAB(wlc->pub) && wlc->pub->associated) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

/**
 * Set a rateset limit. This will be applied when calculating rateset supported.
 */
void
wlc_he_set_rateset_filter(wlc_he_info_t *hei, wlc_rateset_t *rateset)
{
	hei->rateset_filter = *rateset;
}

/**
 * Retreieve the default/unfiltered by HW supported rateset
 */
void
wlc_he_default_rateset(wlc_he_info_t *hei, wlc_rateset_t *rateset)
{
	wlc_he_cp_rateset_to_wlc(&hei->he_rateset, rateset);
}

/**
 * Initialize supported HE capabilities, this function is called from wlc.c from wlc_attach. It
 * should initialize all HE capabilites supported by the device. Stored in HE information store
 * in the wlc struct.
 */
int
BCMATTACHFN(wlc_he_init_defaults)(wlc_he_info_t *hei)
{
	wlc_info_t *wlc;
	uint32 channel_width_set;

	wlc = hei->wlc;

	/* No limit on external configured rateset */
	hei->rateset_filter.he_bw80_tx_mcs_nss = HE_CAP_MAX_MCS_MAP_0_11_NSS4;
	hei->rateset_filter.he_bw80_rx_mcs_nss = HE_CAP_MAX_MCS_MAP_0_11_NSS4;
	hei->rateset_filter.he_bw160_tx_mcs_nss = HE_CAP_MAX_MCS_MAP_0_11_NSS4;
	hei->rateset_filter.he_bw160_rx_mcs_nss = HE_CAP_MAX_MCS_MAP_0_11_NSS4;
	hei->rateset_filter.he_bw80p80_tx_mcs_nss = HE_CAP_MAX_MCS_MAP_0_11_NSS4;
	hei->rateset_filter.he_bw80p80_rx_mcs_nss = HE_CAP_MAX_MCS_MAP_0_11_NSS4;

	/* Populate HE MCS-NSS-BW cap rateset */
	phy_hecap_get_rateset((phy_info_t *)WLC_PI(wlc), &hei->he_rateset);

	/* Remove 1024QAM cap if needed */
	if (BCM1024QAM_DSAB(wlc)) {
		wlc_he_remove_1024qam(wlc, wlc->stf->txstreams,
			&hei->he_rateset.bw80_tx_mcs_nss);
		wlc_he_remove_1024qam(wlc, wlc->stf->txstreams,
			&hei->he_rateset.bw160_tx_mcs_nss);
		wlc_he_remove_1024qam(wlc, wlc->stf->txstreams,
			&hei->he_rateset.bw80p80_tx_mcs_nss);
		wlc_he_remove_1024qam(wlc, wlc->stf->rxstreams,
			&hei->he_rateset.bw80_rx_mcs_nss);
		wlc_he_remove_1024qam(wlc, wlc->stf->rxstreams,
			&hei->he_rateset.bw160_rx_mcs_nss);
		wlc_he_remove_1024qam(wlc, wlc->stf->rxstreams,
			&hei->he_rateset.bw80p80_rx_mcs_nss);
	}

#ifdef HERTDBG
	wlc_he_print_rateset(&hei->he_rateset);
#endif /* HERTDBG */

	/* Initialize band-specific default HE rateset */
	wlc_he_default_ratesets(hei, NULL);

	/* Initialize MAC Capabilities */
	wlc_he_maccap_init(hei);

	hei->ppet = phy_hecap_get_ppet((phy_info_t *)WLC_PI(wlc));

	/* Initialize PHY Capabilities */
	phy_hecap_fill_phycap_info((phy_info_t *)WLC_PI(wlc), &hei->def_phy_cap);

	/* Determine the rateset size based upon PHY capabilities. Min is 4 */
	hei->he_ratesetsz = HE_MIN_HE_RATESETSZ; /* BW 80 TxRx always encoded */
	/* From std: The Rx HE-MCS Map 160 MHz subfield is present if B2 of the Channel Width Set
	 * subfield of the HE PHY Capabilities Information field is set to 1
	 */
	channel_width_set = getbits((uint8*)&hei->def_phy_cap, sizeof(hei->def_phy_cap),
		HE_PHY_CH_WIDTH_SET_IDX, HE_PHY_CH_WIDTH_SET_FSZ);
	if (channel_width_set & HE_PHY_CH_WIDTH_5G_160)
		hei->he_ratesetsz += HE_BW_RASETSZ; /* BW 160 TxRx encoded */
	/* From std: The Rx HE-MCS Map 80+80 MHz subfield is present if B3 of the Channel Width Set
	 * subfield of the HE PHY Capabilities Information field is set to 1
	 */
	if (channel_width_set & HE_PHY_CH_WIDTH_5G_80P80) {
		/* 80p80 is only allowed if 160 is supported: */
		ASSERT(hei->he_ratesetsz == (HE_BW_RASETSZ + HE_BW_RASETSZ));
		hei->he_ratesetsz += HE_BW_RASETSZ; /* BW 80p80 TxRx encoded */
	}

	return BCME_OK;
}

static void
wlc_he_fill_ppe_thresholds(wlc_he_info_t *hei)
{
	wlc_info_t *wlc = hei->wlc;
	uint8 max_nss_supported;
	uint8 ppet;
	wlc_he_ppet_t *ppe_ths;
	uint nss, ru_index;
	uint8 ppet8;
	uint8 ppet16;

	ppe_ths = &hei->ppe_ths;

	/* Take the override value if set */
	if (hei->ppet_override == WL_HE_PPET_AUTO) {
		ppet = hei->ppet;
	} else {
		ppet = hei->ppet_override;
	}

	if (ppet == WL_HE_PPET_0US) {
		ppe_ths->ppe_ths_len = 0;
		return;
	}

	/* take rxchain to specify remote what ppet values per nss to use. */
	max_nss_supported = wlc_stf_rxstreams_get(wlc);

	ppe_ths->ppe_ths_len = HE_PPE_THRESH_LEN(max_nss_supported, HE_PPET_MAX_RUCOUNT);
	memset(ppe_ths->ppe_ths, 0, ppe_ths->ppe_ths_len);

	/* Store total nss (-1) in IE */
	setbits(ppe_ths->ppe_ths, ppe_ths->ppe_ths_len, HE_NSSM1_IDX, HE_NSSM1_LEN,
		max_nss_supported - 1);
	/* Store ru bitmask */
	setbits(ppe_ths->ppe_ths, ppe_ths->ppe_ths_len, HE_RU_INDEX_MASK_IDX, HE_RU_INDEX_MASK_LEN,
		HE_PPET_MAX_RUBITMAP);

	if (ppet == WL_HE_PPET_8US) {
		ppet8 = HE_CONST_IDX_BPSK;
		ppet16 = HE_CONST_IDX_NONE;
	} else { /* WL_HE_PPET_16US */
		ppet8 = HE_CONST_IDX_NONE;
		ppet16 = HE_CONST_IDX_BPSK;
	}

	for (nss = 0; nss < max_nss_supported; nss++) {
		for (ru_index = 0; ru_index < HE_PPET_MAX_RUCOUNT; ru_index++) {
			setbits(ppe_ths->ppe_ths, ppe_ths->ppe_ths_len,
				HE_PPET16_BIT_OFFSET(HE_PPET_MAX_RUCOUNT, nss, ru_index),
				HE_PPE_THRESH_NSS_RU_FSZ, ppet16);
			setbits(ppe_ths->ppe_ths, ppe_ths->ppe_ths_len,
				HE_PPET8_BIT_OFFSET(HE_PPET_MAX_RUCOUNT, nss, ru_index),
				HE_PPE_THRESH_NSS_RU_FSZ, ppet8);
		}
	}
}

/* get current cap info this node uses */
uint8
wlc_he_get_bfe_ndp_recvstreams(wlc_he_info_t *hei)
{
	uint8 bfe_sts = 0;

	bfe_sts = getbits((uint8*)&hei->def_phy_cap, sizeof(hei->def_phy_cap),
		HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_IDX, HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_FSZ);

	return bfe_sts;
}

/* get my bfr sts */
uint8
wlc_he_get_bfr_ndp_sts(wlc_he_info_t *hei, bool is_bw160)
{
	uint8 bfr_sts;

	if (is_bw160) {
		bfr_sts = getbits((uint8*)&hei->def_phy_cap, sizeof(hei->def_phy_cap),
			HE_PHY_SOUND_DIM_ABOVE80MHZ_IDX, HE_PHY_SOUND_DIM_ABOVE80MHZ_FSZ);
	} else {
		bfr_sts = getbits((uint8*)&hei->def_phy_cap, sizeof(hei->def_phy_cap),
			HE_PHY_SOUND_DIM_BELOW80MHZ_IDX, HE_PHY_SOUND_DIM_BELOW80MHZ_FSZ);
	}

	return bfr_sts;
}

static void
wlc_he_ap_maccap_init(wlc_he_info_t *hei, he_mac_cap_t *ap_mac_cap)
{
	wlc_info_t *wlc = hei->wlc;

	memcpy(ap_mac_cap, &hei->def_mac_cap, sizeof(*ap_mac_cap));

	/* b3-b4: Fragmentation Support field */
	setbits((uint8 *)ap_mac_cap, sizeof(*ap_mac_cap), HE_MAC_FRAG_SUPPORT_IDX,
		HE_MAC_FRAG_SUPPORT_FSZ, wlc_he_get_dynfrag(wlc));

	/* b10-b11: Trigger frame mac padding duration is 0 for AP */

	/* b19: AP can receive A-BSR support */
	if (hei->bsr_supported) {
		setbit((uint8 *)ap_mac_cap, HE_MAC_A_BSR_IDX);
	}

	/* b24: Group Addressed Multi-STA Block-Ack in DL MU Support, is 0 for AP */

	/* b25: OM Control Support, always 1 for AP */
	setbit((uint8 *)ap_mac_cap, HE_MAC_OM_CONTROL_SUPPORT_IDX);

	/* b26: OFDMA RA Support */

	/* b26: AP supports sending Trigger to allocate random RUs */

	/* b31: Rx Control Frame to MultiBSS is 0 for AP. */

	/* b34: BQR receive Support. */

	/* b44: OM Ctrl UL MU Data Disable RX, supported. */
	setbit((uint8 *)ap_mac_cap, HE_MAC_OM_UL_MU_DATA_DISABLE_IDX);
}

static void
wlc_he_sta_maccap_init(wlc_he_info_t *hei, he_mac_cap_t *sta_mac_cap)
{
	wlc_info_t *wlc = hei->wlc;

	memcpy(sta_mac_cap, &hei->def_mac_cap, sizeof(*sta_mac_cap));

	/* b3-b4: Fragmentation Support field */
	setbits((uint8 *)sta_mac_cap, sizeof(*sta_mac_cap), HE_MAC_FRAG_SUPPORT_IDX,
		HE_MAC_FRAG_SUPPORT_FSZ, wlc_he_get_dynfrag(wlc));

	/* b10-b11: Trigger frame mac padding duration, set to max for moment, not 100% sure what
	 * was found as safe value, initially 16usec was not even enough...
	 */
	setbits((uint8 *)sta_mac_cap, sizeof(*sta_mac_cap), HE_MAC_TRG_PAD_DUR_IDX,
		HE_MAC_TRG_PAD_DUR_IDX, HE_MAC_TRG_PAD_DUR_16);

	/* b19: STA supports sending A-control with AMPDU BSR  */

	/* b24: Group Addressed Multi-STA Block-Ack in DL MU Support, not supported in a0 */

	/* b25: OM Control Support */
	setbit((uint8 *)sta_mac_cap, HE_MAC_OM_CONTROL_SUPPORT_IDX);

	/* b26: OFDMA RA Support */

	/* b31: Rx Control Frame to MultiBSS. */

	/* b34: BQR transmit Support. */

	/* b36: NDP Feedback Report Support */

	/* b38: A-MSDU In A-MPDU Support */
}

static void
BCMATTACHFN(wlc_he_maccap_init)(wlc_he_info_t *hei)
{
	wlc_info_t *wlc = hei->wlc;
	he_mac_cap_t *mac_cap = &hei->def_mac_cap;
	uint8 frag_cap;
	bzero(mac_cap, sizeof(*mac_cap));

	/* Initialize common HE MAC capabilities for STA & AP */

	/* b0: +HTC-HE Support */
	if (D11REV_GE(wlc->pub->corerev, 129)) {
		setbit((uint8 *)mac_cap, HE_MAC_HTC_HE_SUPPORT_IDX);
	}

	frag_cap = wlc_he_get_dynfrag(wlc);
	/* b3-b4: Fragmentation Support field */
	setbits((uint8 *)mac_cap, sizeof(*mac_cap), HE_MAC_FRAG_SUPPORT_IDX,
		HE_MAC_FRAG_SUPPORT_FSZ, frag_cap);

	if ((frag_cap == HE_MAC_FRAG_ONE_PER_AMPDU) || (frag_cap == HE_MAC_FRAG_MULTI_PER_AMPDU)) {
		/* b5-b7: Max. Number of fragmented MSDUs */
		setbits((uint8 *)mac_cap, sizeof(*mac_cap), HE_MAC_MAX_MSDU_FRAGS_IDX,
			HE_MAC_MAX_MSDU_FRAGS_FSZ, WLC_HE_MAX_MSDU_FRAG_ENC);

		/* bit 8-9: Min. payload size of first fragment */
		setbits((uint8 *)mac_cap, sizeof(*mac_cap), HE_MAC_MIN_FRAG_SIZE_IDX,
			HE_MAC_MIN_FRAG_SIZE_FSZ, HE_MAC_MINFRAG_NO_RESTRICT);
	}

	/* bit 12-14: Max. TIDs that can be aggregated in an AMPDU, 684a0 does not support */
	if (!D11REV_IS(wlc->pub->corerev, 128))
		setbits((uint8 *)mac_cap, sizeof(*mac_cap), HE_MAC_MULTI_TID_AGG_IDX,
			HE_MAC_MULTI_TID_AGG_FSZ, (WLC_HE_MAX_TID_IN_AMPDU - 1));

	/* bit 15-16: HE Link Adaptation Capable: not supported for a0 */

	/* b17: Support Rx. of multi-TID A-MPDU under all ack context */

	/* b18: UMRS support */

	/* b20: See above, TWT support related. */

	/* b21: Supports Rx. of Multi-STA BA with 32-bit BlockAck bitmap */

	/* b22: STA supports participating in an MU Cascading sequence */

	/* b23: Ack-enabled Multi-TID Aggregation Support */

	/* b27-28: Max AMPDU Length Exponent. Setting it to 2 as this is supported by all HW */
	setbits((uint8 *)mac_cap, sizeof(*mac_cap), HE_MAC_MAX_AMPDU_LEN_EXP_IDX,
		HE_MAC_MAX_AMPDU_LEN_EXP_FSZ, 2);

	/* b29: A-MSDU Fragmentation Support. */
	if ((frag_cap == HE_MAC_FRAG_ONE_PER_AMPDU) || (frag_cap == HE_MAC_FRAG_MULTI_PER_AMPDU)) {
		setbit((uint8 *)mac_cap, HE_MAC_AMSDU_FRAG_SUPPORT_IDX);
	}

	/* b30: Flexible TWT Schedule Support. */

	/* b32: BSRP A-MPDU Aggregation. */

	/* b33: QTP Support. */

	/* b35: SR Responder. */

	/* b37: OPS Support */

	/* b38: A-MSDU In A-MPDU Support */

	/* b42: HE Subchannel Selective Transmission Support - RSDB (TWT) Dont Support */
}

static void
wlc_he_ap_phycap_init(wlc_he_info_t *hei, he_phy_cap_t *ap_phy_cap)
{
	memcpy(ap_phy_cap, &hei->def_phy_cap, sizeof(*ap_phy_cap));

	/* b12: device class. Reserved for AP */
	clrbit((uint8 *)ap_phy_cap, HE_PHY_DEVICE_CLASS_IDX);

	/* b22-b23: Only Full BW UL MU-MIMO Rx. */

	/* b56-b58: TXBF feedback with Trigger frame */

	/* b60: DL MU-MIMO on Partial BW. Reserved for AP */
	clrbit((uint8 *)ap_phy_cap, HE_PHY_DL_MU_MIMO_PART_BW_IDX);

}

static void
wlc_he_sta_phycap_init(wlc_he_info_t *hei, he_phy_cap_t *sta_phy_cap)
{
	memcpy(sta_phy_cap, &hei->def_phy_cap, sizeof(*sta_phy_cap));

	/* b22-b23: Only Full BW UL MU-MIMO Tx. */

	/* b33: MU Beamformer. valid only for AP */
	clrbit((uint8 *)sta_phy_cap, HE_PHY_MU_BEAMFORMER_IDX);

	/* b56-b58: TXBF feedback with Trigger frame */

}

/* ======== iovar dispatch ======== */

/* FIXME: integrate with iovar subcommand infrastructure
 * which is being designed/implemented...
 */

static int
wlc_he_cmd_enab(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;

	if (set) {
		if (!wlc_he_hw_cap(wlc) || !N_ENAB(wlc->pub)) {
			return BCME_UNSUPPORTED;
		}
		wlc->pub->_he_enab = *(uint8 *)params;
		if (!wlc->pub->_he_enab) {
			wlc_twt_disable(wlc);
		}
	} else {
		*result = wlc->pub->_he_enab;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

static int
wlc_he_cmd_features(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen,
	bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;

	if (set) {
		uint32 features = *(uint32 *)params;
		bool update_fifos = FALSE;

		if (*(int32 *)params == -1) {
			/* Reset to default value */
			features = WL_HE_FEATURES_DEFAULT(wlc);
		}
		if (features & (~WL_HE_MAX_ALLOWED_FEATURES)) {
			return BCME_UNSUPPORTED;
		}
		if (((features & (WL_HE_FEATURES_ULOMU | WL_HE_FEATURES_DLOMU)) != 0) &&
			((features & (WL_HE_FEATURES_5G | WL_HE_FEATURES_2G)) == 0)) {
			return BCME_EPERM; /* not a valid combination */
		}
#if !defined(BCMDBG) && !defined(WLTEST)
		if (((features & WL_HE_FEATURES_ULOMU) != 0) &&
			((features & WL_HE_FEATURES_DLOMU) == 0)) {
			return BCME_EPERM; /* not a valid combination */
		}
#endif // endif
		update_fifos = ((features & WL_HE_FEATURES_DLOMU) !=
			(wlc->pub->he_features & WL_HE_FEATURES_DLOMU));
		wlc->pub->he_features = features;
		if (update_fifos) {
			/* The number of active TX FIFOs may have changed */
			wlc_hw_update_nfifo(wlc->hw);
		}
	} else {
		*(uint32 *)result = wlc->pub->he_features;
		*rlen = sizeof(*(uint32 *)result);
	}

	return BCME_OK;
}

static int
wlc_he_configure_new_bsscolor(wlc_he_info_t *hei, uint8 new_color, uint8 disabled,
	uint8 switch_count)
{
	wlc_info_t *wlc = hei->wlc;
	int idx;
	wlc_bsscfg_t *bsscfg;
	bss_he_info_t *bhei;

	if ((hei->update_bsscolor_counter) && (switch_count))
		return BCME_BUSY;

	/* Special case, when switch_count is set to 0 then force update of color at once. This
	 * allows for testing colors in all modes (pkteng).
	 */
	if (switch_count == 0) {
		hei->bsscolor_disable = disabled;
		hei->new_bsscolor = new_color;
		hei->bsscolor = new_color;
		wlc_he_configure_bsscolor_phy(hei, NULL);
		FOREACH_BSS(wlc, idx, bsscfg) {
			bhei = BSS_HE_INFO(hei, bsscfg);
			bhei->bsscolor = new_color;
			bhei->bsscolor_disable = disabled;
			if (BSSCFG_AP(bsscfg)) {
				wlc_bss_update_beacon(wlc, bsscfg);
				wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
			}
			if (RATELINKMEM_ENAB(wlc->pub)) {
				wlc_ratelinkmem_update_link_entry_all(wlc, bsscfg, FALSE,
					FALSE /* clr_txbf_stats=0 in mreq */);
			}
		}
	} else {
		hei->bsscolor_counter = switch_count;
		if (disabled) {
			hei->bsscolor_disable = TRUE;
			wlc_he_configure_bsscolor_phy(hei, NULL);
			wlc_update_beacon(hei->wlc);
			wlc_update_probe_resp(hei->wlc, TRUE);
		} else {
			hei->update_bsscolor_counter = switch_count;
			hei->bsscolor_disable = FALSE;
			hei->new_bsscolor = new_color;
		}
	}
	return BCME_OK;
}

/** called on './wl he bsscolor' */
static int
wlc_he_cmd_bsscolor(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg;
	bss_he_info_t *bhei;
	wl_he_bsscolor_t *bsscolor;
	int ret_code = BCME_OK;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	if (set) {
		bsscolor = (wl_he_bsscolor_t *)params;
		if (!BSSCFG_AP(cfg) && bsscolor->switch_count) {
			ret_code = BCME_NOTAP;
		} else if (!wlc_he_hw_cap(wlc)) {
			ret_code = BCME_UNSUPPORTED;
		} else if (bsscolor->color > HE_BSSCOLOR_MAX_VALUE) {
			ret_code = BCME_BADARG;
		} else if ((bsscolor->color != hei->bsscolor) ||
			(bsscolor->disabled != hei->bsscolor_disable) ||
			(bsscolor->switch_count == 0)) {
			ret_code = wlc_he_configure_new_bsscolor(hei, bsscolor->color,
				bsscolor->disabled, bsscolor->switch_count);
		}
	} else {
		bsscolor = (wl_he_bsscolor_t *)result;
		if (BSSCFG_AP(cfg)) {
			bsscolor->color = hei->bsscolor;
			bsscolor->disabled = hei->bsscolor_disable;
		} else {
			bhei = BSS_HE_INFO(hei, cfg);
			bsscolor->color = bhei->bsscolor;
			bsscolor->disabled = bhei->bsscolor_disable;
		}
		bsscolor->switch_count = hei->bsscolor_counter;
		*rlen = sizeof(*bsscolor);
	}

	return ret_code;
}

/** called on './wl he partialbsscolor' */
static int
wlc_he_cmd_partial_bsscolor(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg;
	bss_he_info_t *bhei;
	bool new_val = (*(uint8 *)params != 0);

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);
	bhei = BSS_HE_INFO(hei, cfg);

	if (set) {
		if (!BSSCFG_AP(cfg)) {
			return BCME_NOTAP;
		}
		if (!wlc_he_hw_cap(wlc)) {
			return BCME_UNSUPPORTED;
		}
		if (new_val != bhei->partial_bsscolor_ind) {
			bhei->partial_bsscolor_ind = new_val;
			/* update beacon and probe response templates */
			if (cfg->up) {
				wlc_bss_update_beacon(wlc, cfg);
				wlc_bss_update_probe_resp(wlc, cfg, TRUE);
			}
		}
	} else {
		*result = bhei->partial_bsscolor_ind;
		*rlen = sizeof(*result);
	}
	return BCME_OK;
}

static int
wlc_he_cmd_rtsdurthresh(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg;
	bss_he_info_t *bhei;
	uint16 new_val = *(uint16 *)params;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);
	bhei = BSS_HE_INFO(hei, cfg);

	if (set) {
		if (!BSSCFG_AP(cfg)) {
			return BCME_NOTAP;
		}
		if (!wlc_he_hw_cap(wlc)) {
			return BCME_UNSUPPORTED;
		}
		if (new_val != bhei->rts_thresh) {
			if ((new_val >> HE_RTS_DUR_THRESHOLD_32USEC_SHIFT) >=
				HE_RTS_THRES_DISABLED) {
				new_val = BCM_UINT16_MAX;
			}
			bhei->rts_thresh = new_val;
			/* update beacon and probe response templates */
			if (cfg->up) {
				wlc_bss_update_beacon(wlc, cfg);
				wlc_bss_update_probe_resp(wlc, cfg, TRUE);
			}
		}
	} else {
		*(uint16 *)result = bhei->rts_thresh;
		*rlen = sizeof(*(uint16*)result);
	}

	return BCME_OK;
}

static int
wlc_he_cmd_peduration(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg;
	bss_he_info_t *bhei;
	uint8 new_val = *(uint8 *)params;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);
	bhei = BSS_HE_INFO(hei, cfg);

	if (set) {
		if (!BSSCFG_AP(cfg)) {
			return BCME_NOTAP;
		}
		if (!wlc_he_hw_cap(wlc)) {
			return BCME_UNSUPPORTED;
		}
		if (new_val != bhei->pe_duration) {
			bhei->pe_duration = new_val;
			/* update beacon and probe response templates */
			if (cfg->up) {
				wlc_bss_update_beacon(wlc, cfg);
				wlc_bss_update_probe_resp(wlc, cfg, TRUE);
			}
		}
	} else {
		*result = bhei->pe_duration;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

static int
wlc_he_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	int err = BCME_OK;

	BCM_REFERENCE(vsize);

	switch (actionid) {
	case IOV_GVAL(IOV_HE):
		err = wlc_iocv_iov_cmd_proc(wlc, ctx, he_cmds, ARRAYSIZE(he_cmds),
			FALSE, params, plen, arg, alen, wlcif);
		break;
	case IOV_SVAL(IOV_HE):
		err = wlc_iocv_iov_cmd_proc(wlc, ctx, he_cmds, ARRAYSIZE(he_cmds),
			TRUE, params, plen, arg, alen, wlcif);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* ======== wlc module hooks ========= */

/* wlc up/init callback */
static int
wlc_he_wlc_up(void *ctx)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	phy_info_t *pi = (phy_info_t *)WLC_PI(wlc);
	wlc_bsscolor_t bsscolor;

	/* Tell the phy to not use any color filtering for STA idx */
	bsscolor.disable = TRUE;
	bsscolor.color = 0;
	bsscolor.index = HE_BSSCOLOR_STAIDX;
	phy_hecap_write_bsscolor(pi, &bsscolor);

	return BCME_OK;
}

#if defined(BCMDBG)
/* debug dump */
static int
wlc_he_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;

	bcm_bprintf(b, "HE Enab: %d Features: 0x%x\n", HE_ENAB(wlc->pub), wlc->pub->he_features);

	return BCME_OK;
}
#endif // endif

/* ======== bsscfg module hooks ======== */

/* bsscfg cubby */
static int
wlc_he_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_he_info_t *hei = (wlc_he_info_t *)ctx;
	wlc_info_t *wlc = hei->wlc;
	bss_he_info_t **pbhei = BSS_HE_INFO_LOC(hei, cfg);
	bss_he_info_t *bhei = BSS_HE_INFO(hei, cfg);

	if ((bhei = MALLOCZ(wlc->osh, sizeof(*bhei))) == NULL) {
		WL_ERROR(("wl%d: %s: mem alloc failed, allocated %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}

	*pbhei = bhei;

	bhei->pe_duration = HE_DEFAULT_PE_DURATION;
	bhei->rts_thresh = 0;		 /* always enabled */
	bhei->edca_update_count = HE_MUEDCA_INVALID_UPDATE_COUNT;
	memcpy(&bhei->sta_mu_edca, &default_mu_edca_sta, sizeof(bhei->sta_mu_edca));
	return BCME_OK;
}

static void
wlc_he_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_he_info_t *hei = (wlc_he_info_t *)ctx;
	wlc_info_t *wlc = hei->wlc;
	bss_he_info_t **pbhei = BSS_HE_INFO_LOC(hei, cfg);
	bss_he_info_t *bhei = BSS_HE_INFO(hei, cfg);

	if (bhei == NULL) {
		return;
	}

	MFREE(wlc->osh, bhei, sizeof(*bhei));
	*pbhei = NULL;
}

#if defined(BCMDBG)
static void
wlc_he_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_he_info_t *hei = (wlc_he_info_t *)ctx;
	bss_he_info_t *bhei = BSS_HE_INFO(hei, cfg);

	bcm_bprintf(b, "Bsscolor %d (%s)\n", bhei->bsscolor, bhei->bsscolor_disable ? "disabled" :
		"enabled");
}
#endif // endif

/**
 * bsscfg state change callback, will be invoked when a bsscfg changes its state (enable/disable/
 * up/down).
 */
static void
wlc_he_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *evt)
{
	wlc_he_info_t *hei = (wlc_he_info_t *)ctx;
	wlc_info_t *wlc =  hei->wlc;
	wlc_bsscfg_t *cfg = evt->cfg;

	if (cfg->up) {
		wlc_bss_info_t *bi = cfg->current_bss;

		/* TWT requester support in extended capabilities. */
		if (wlc_twt_req_cap(wlc, cfg)) {
			wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_TWT_REQUESTER, TRUE);
		} else {
			wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_TWT_REQUESTER, FALSE);
		}

		/* TWT responder support in extended capabilities. */
		if (wlc_twt_resp_cap(wlc, cfg)) {
			wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_TWT_RESPONDER, TRUE);
		} else {
			wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_TWT_RESPONDER, FALSE);
		}

		/* Initialize band-specific default HE rateset */
		wlc_he_default_ratesets(hei, cfg);

		if (BSSCFG_AP(cfg) || BSSCFG_IBSS(cfg)) {
			bi->flags3 &= ~(WLC_BSS3_HE);

			if (BSS_HE_ENAB_BAND(wlc, wlc->band->bandtype, cfg)) {
				bi->flags3 |= WLC_BSS3_HE;
			}
		}

		/* calculate PPET based on nss */
		wlc_he_fill_ppe_thresholds(hei);

		if (BSSCFG_AP(cfg)) {
			wlc_he_init_rateset(hei, &bi->rateset, wlc->band->bandunit, cfg);
			wlc_he_cp_he_rateset_to_bi_sup(bi,
				&wlc->bandstate[wlc->band->bandunit]->hw_rateset);
			wlc_he_init_bi_rateset_to_none(bi);
			wlc_he_configure_bsscolor_phy(hei, NULL);
		}
	}
}

#ifdef STA
/* PCB function for the QoS Null frame */
static void
wlc_he_send_qosnull_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	scb_he_t *he_scb;

	bsscfg = (wlc_bsscfg_t*) arg;
	ASSERT(bsscfg != NULL);

	if (!(scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID))) {
		return;
	}

	if (!(he_scb = SCB_HE(wlc->hei, scb))) {
		return;
	}

	if (txstatus & TX_STATUS_ACK_RCV) {
		/* acked */
		he_scb->qnull_retries = 0;
	} else if (he_scb->qnull_retries++ < HE_QNULL_RETRY_LIMIT) {
		/* retry qos null */
		wlc_he_sendqosnull(wlc, bsscfg, scb);
	} else {
		/* reach retry limit */
		WL_ERROR(("wl%d: %s: Fails to send qos-null frames. Reach retry limit %d\n",
			wlc->pub->unit, __FUNCTION__, he_scb->qnull_retries));
		he_scb->qnull_retries = 0;
	}
}

static int
wlc_he_sendqosnull_cb(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt, void *data)
{
	wlc_pcb_fn_register(wlc->pcb, wlc_he_send_qosnull_complete, cfg, pkt);
	return BCME_OK;
}

static void
wlc_he_sendqosnull(wlc_info_t *wlc, wlc_bsscfg_t *cfg, scb_t *scb)
{
	wlc_he_info_t *hei = wlc->hei;
	scb_he_t *he_scb;
	uint32 htc;

	ASSERT(scb);
	ASSERT(cfg);
	if (SCB_HE_CAP(scb) && BSSCFG_STA(cfg)) {
		he_scb = SCB_HE(hei, scb);
		htc = HE_OMI_ENCODE(HTC_OM_CONTROL_TX_NSTS(he_scb->omi_lm),
			HTC_OM_CONTROL_RX_NSS(he_scb->omi_lm),
			HTC_OM_CONTROL_CHANNEL_WIDTH(he_scb->omi_lm), 0, 0, 1, 0);
		htc = (htc << HTC_CONTROL_ID_SZ) | HTC_CONTROL_ID_OM;
		htc = (htc << HTC_VARIANT_SZ) | HTC_IDENTITY_HE;
		wlc_he_htc_send_code(wlc, scb, htc, NULL);
		wlc_sendnulldata(wlc, cfg, &cfg->BSSID, 0, 0,
			PRIO_8021D_BE, wlc_he_sendqosnull_cb, NULL);
	}
}
#endif /* STA */

/**
 * assoc state change callback, will be invoked when a link association state changes
 */
static void
wlc_he_assoc_state_change_cb(void *ctx, bss_assoc_state_data_t *notif_data)
{
	wlc_he_info_t *hei = (wlc_he_info_t *)ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg = notif_data->cfg;

	if (!HE_ENAB(wlc->pub)) {
		return;
	}
	if (notif_data->type == AS_ASSOCIATION ||
		notif_data->type == AS_ROAM) {

		if (notif_data->state == AS_JOIN_ADOPT) {
			/* Make sure phy gets updated with staid info, color config function */
			wlc_he_configure_bsscolor_phy(hei, cfg);

#ifdef STA

			/* XXX: Chips with corerev>=128 do not support UL OFDMA as a STA. The temp
			 * WAR is to send a QoS-null with UL_MU_DIS in OMI for now. Remove
			 * following when it has UL OFDMA supported.
			 * XXX: Update this condition when corerev 130 and 131 has UL OFDMA as STA
			 * supported
			 */
			if (D11REV_GE(wlc->pub->corerev, 128)) {
				scb_t *scb;
				scb_he_t *he_scb;
				if (!(scb = wlc_scbfind(wlc, cfg, &cfg->BSSID))) {
					return;
				}
				he_scb = SCB_HE(hei, scb);
				if (he_scb) {
					he_scb->qnull_retries = 0;
					wlc_he_sendqosnull(wlc, cfg, scb);
				}
			}
#endif /* STA */
		}
	}
}

/* ======== scb cubby ======== */

static int
wlc_he_scb_init(void *ctx, scb_t *scb)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	scb_he_t **psh = SCB_HE_CUBBY(hei, scb);
	scb_he_t *sh = SCB_HE(hei, scb);

	ASSERT(sh == NULL);

	*psh = wlc_scb_sec_cubby_alloc(wlc, scb, sizeof(*sh));

	return BCME_OK;
}

static void
wlc_he_scb_deinit(void *ctx, scb_t *scb)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	scb_he_t **psh = SCB_HE_CUBBY(hei, scb);
	scb_he_t *sh = SCB_HE(hei, scb);

	/* Memory not allocated for SCB return */
	if (!sh) {
		return;
	}

	wlc_scb_sec_cubby_free(wlc, scb, sh);
	*psh = NULL;
}

static uint
wlc_he_scb_secsz(void *ctx, scb_t *scb)
{
	scb_he_t *sh;
	return sizeof(*sh);
}

#if defined(BCMDBG)
static const bcm_bit_desc_t scb_mac_cap[] =
{
	{HE_MAC_TWT_REQ_SUPPORT_IDX, "TWTReq"},
	{HE_MAC_TWT_RESP_SUPPORT_IDX, "TWTResp"},
	{HE_MAC_UL_MU_RESP_SCHED_IDX, "UL-MU-Resp"},
	{HE_MAC_A_BSR_IDX, "A-BSR"}
};

static const bcm_bit_desc_t scb_phy_cap[] =
{
	{HE_PHY_DEVICE_CLASS_IDX, "Class-A"},
	{HE_PHY_LDPC_PYLD_IDX, "LDPC"},
	{HE_PHY_SU_BEAMFORMER_IDX, "SU-BFR"},
	{HE_PHY_SU_BEAMFORMEE_IDX, "SU-BFE"},
	{HE_PHY_MU_BEAMFORMER_IDX, "MU-BFR"},
	{HE_PHY_TX_1024_QAM_LESS_242_TONE_RU_IDX, "Tx1024Q"},
	{HE_PHY_RX_1024_QAM_LESS_242_TONE_RU_IDX, "Rx1024Q"}
};

static void
wlc_he_scb_dump(void *ctx, scb_t *scb, struct bcmstrbuf *b)
{
	char heinfostr[HE_INFO_STR_SZ];
	wlc_he_info_t *hei = ctx;
	scb_he_t *sh = SCB_HE(hei, scb);
	wlc_rateset_t *rateset;

	if (sh == NULL) {
		return;
	}

	if (!SCB_HE_CAP(scb)) {
		bcm_bprintf(b, "     NOT HE capable");
		return;
	}

	bcm_format_octets(scb_mac_cap, ARRAYSIZE(scb_mac_cap), (const uint8 *)&sh->mac_cap,
		sizeof(sh->mac_cap), heinfostr, sizeof(heinfostr));
	bcm_bprintf(b, "     he_mac_cap: %s ", heinfostr);
	bcm_bprhex(b, NULL, TRUE, (const uint8 *)&sh->mac_cap, sizeof(sh->mac_cap));
	bcm_format_octets(scb_phy_cap, ARRAYSIZE(scb_phy_cap), (const uint8 *)&sh->phy_cap,
		sizeof(sh->phy_cap), heinfostr, sizeof(heinfostr));
	bcm_bprintf(b, "     he_phy_cap: %s ", heinfostr);
	bcm_bprhex(b, NULL, TRUE, (const uint8 *)&sh->phy_cap, sizeof(sh->phy_cap));
	rateset = &scb->rateset;
	bcm_bprintf(b, "     Tx 80=0x%04x, Rx 80=0x%04x, Tx 160=0x%04x, Rx 160=0x%04x\n",
		rateset->he_bw80_tx_mcs_nss, rateset->he_bw80_rx_mcs_nss,
		rateset->he_bw160_tx_mcs_nss, rateset->he_bw160_rx_mcs_nss);
}
#endif // endif

/* ======== IE mgmt ======== */

/* figure out the length of the value in the TLV i.e. the length without the TLV header. */
static uint8
_wlc_he_calc_cap_ie_len(wlc_he_info_t *hei, wlc_bsscfg_t *cfg)
{
	/* The length of the HE cap IE is dependent on the bandwidth. By the time the defaults
	 * get determined it is not possible to know for which band. This is how it works:
	 * The minimum support is bw80, on 2.4G that is also the max. On 5G the max is
	 * determined by the capabilities of device which is precalculated.
	 */
	if (wlc_he_bw80_limited(hei->wlc, cfg)) {
		return (sizeof(he_cap_ie_t) - TLV_HDR_LEN + HE_MIN_HE_RATESETSZ +
			hei->ppe_ths.ppe_ths_len);
	} else {
		return (sizeof(he_cap_ie_t) - TLV_HDR_LEN + hei->he_ratesetsz +
			hei->ppe_ths.ppe_ths_len);
	}
}

static uint
wlc_he_max_mcs_idx(uint16 mcs_nss_set)
{
	uint8 nss;
	uint8 mcs_code;
	uint max_mcs_idx = 0;

	for (nss = 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		mcs_code = HE_CAP_MAX_MCS_NSS_GET_MCS(nss, mcs_nss_set);

		if (mcs_code == HE_CAP_MAX_MCS_NONE) {
			/* continue to next stream */
			continue;
		}
		if (HE_MAX_MCS_TO_INDEX(mcs_code) > max_mcs_idx) {
			max_mcs_idx = HE_MAX_MCS_TO_INDEX(mcs_code);
		}
	}
	return max_mcs_idx;
}

/* HE Cap IE */
static uint
wlc_he_calc_cap_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_he_info_t *hei = (wlc_he_info_t *)ctx;

	if (!data->cbparm->he)
		return 0;

	return TLV_HDR_LEN + _wlc_he_calc_cap_ie_len(hei, data->cfg);
}

static int
wlc_he_write_cap_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_he_info_t *hei = (wlc_he_info_t *)ctx;
	wlc_info_t *wlc = hei->wlc;
	he_cap_ie_t *cap = (he_cap_ie_t *)data->buf;
	wlc_bsscfg_t *cfg = data->cfg;
	wlc_rateset_t *rateset;
	uint32 channel_width_set;
	uint offset;
	uint8 *bw_info;
	uint8 *ptr;
	uint mcs_tmp;
	uint max_mcs_rx;
	uint max_mcs_tx;

	/* sanity check */
	STATIC_ASSERT(sizeof(hei->def_mac_cap) == SIZE_OF(he_cap_ie_t, mac_cap));
	STATIC_ASSERT(sizeof(hei->def_phy_cap) == SIZE_OF(he_cap_ie_t, phy_cap));

	/* IE header */
	cap->id = DOT11_MNG_ID_EXT_ID;
	cap->id_ext = EXT_MNG_HE_CAP_ID;
	cap->len = _wlc_he_calc_cap_ie_len(hei, cfg);
	offset = sizeof(cap->id) + sizeof(cap->id_ext) + sizeof(cap->len);

	/* Initializing default HE MAC & PHY Capabilities
	 * Set other bits to add new capabilities
	 */
	if (BSSCFG_AP(cfg)) {
		wlc_he_ap_maccap_init(hei, &cap->mac_cap);
		wlc_he_ap_phycap_init(hei, &cap->phy_cap);
	} else {
		wlc_he_sta_maccap_init(hei, &cap->mac_cap);
		wlc_he_sta_phycap_init(hei, &cap->phy_cap);
	}

	/* TWT requester support ? */
	if (wlc_twt_req_cap(wlc, cfg)) {
		setbit((uint8 *)&cap->mac_cap, HE_MAC_TWT_REQ_SUPPORT_IDX);
	}

	/* TWT responder support ? */
	if (wlc_twt_resp_cap(wlc, cfg)) {
		setbit((uint8 *)&cap->mac_cap, HE_MAC_TWT_RESP_SUPPORT_IDX);
	}

	/* Broadcast TWT support ? */
	if (wlc_twt_bcast_cap(wlc, cfg)) {
		setbit((uint8 *)&cap->mac_cap, HE_MAC_BCAST_TWT_SUPPORT_IDX);
	}

	setbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
		HE_PHY_SOUND_DIM_BELOW80MHZ_IDX, HE_PHY_SOUND_DIM_BELOW80MHZ_FSZ,
		wlc->stf->op_txstreams - 1);

	/* Init bfe and bfr cap */
	setbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
		HE_PHY_SU_BEAMFORMEE_IDX, HE_PHY_SU_BEAMFORMEE_FSZ, 0);
	setbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
		HE_PHY_SU_BEAMFORMER_IDX, HE_PHY_SU_BEAMFORMER_FSZ, 0);
	setbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
		HE_PHY_MU_BEAMFORMER_IDX, HE_PHY_MU_BEAMFORMER_FSZ, 0);
#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub)) {
		if (wlc_txbf_get_bfe_cap(wlc->txbf) & TXBF_HE_SU_BFE_CAP) {
			setbit((uint8*)&cap->phy_cap, HE_PHY_SU_BEAMFORMEE_IDX);
		} else {
			clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
				HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_IDX,
				HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_FSZ);
			clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
				HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_IDX,
				HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_FSZ);
			clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
				HE_PHY_MAX_NC_IDX, HE_PHY_MAX_NC_FSZ);
		}
		if (wlc_txbf_get_bfr_cap(wlc->txbf) & TXBF_HE_SU_BFR_CAP) {
			setbit((uint8*)&cap->phy_cap, HE_PHY_SU_BEAMFORMER_IDX);
		} else {
			clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
				HE_PHY_SOUND_DIM_BELOW80MHZ_IDX,
				HE_PHY_SOUND_DIM_BELOW80MHZ_FSZ);
			clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
				HE_PHY_SOUND_DIM_ABOVE80MHZ_IDX,
				HE_PHY_SOUND_DIM_ABOVE80MHZ_FSZ);
		}
		if (wlc_txbf_get_bfr_cap(wlc->txbf) & TXBF_HE_MU_BFR_CAP) {
			setbit((uint8*)&cap->phy_cap, HE_PHY_MU_BEAMFORMER_IDX);
		}
	} else
#endif /* WL_BEAMFORMING */
	{
		clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_IDX, HE_PHY_BEAMFORMEE_STS_BELOW80MHZ_FSZ);
		clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_IDX, HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_FSZ);
		clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_SOUND_DIM_BELOW80MHZ_IDX, HE_PHY_SOUND_DIM_BELOW80MHZ_FSZ);
		clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_SOUND_DIM_ABOVE80MHZ_IDX, HE_PHY_SOUND_DIM_ABOVE80MHZ_FSZ);
		clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_MAX_NC_IDX, HE_PHY_MAX_NC_FSZ);
	}
	if (BAND_2G(wlc->band->bandtype) || wlc_he_bw80_limited(wlc, cfg)) {
		clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_IDX, HE_PHY_BEAMFORMEE_STS_ABOVE80MHZ_FSZ);
		clrbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_SOUND_DIM_ABOVE80MHZ_IDX, HE_PHY_SOUND_DIM_ABOVE80MHZ_FSZ);
	}

	/* Phy capabilities b1-b7 Channel Width Set is dependent on band */
	channel_width_set = getbits((uint8 *)&cap->phy_cap, sizeof(cap->phy_cap),
		HE_PHY_CH_WIDTH_SET_IDX, HE_PHY_CH_WIDTH_SET_FSZ);
	if (BAND_2G(wlc->band->bandtype)) {
		channel_width_set &= ~(HE_PHY_CH_WIDTH_5G_80 | HE_PHY_CH_WIDTH_5G_160 |
			HE_PHY_CH_WIDTH_5G_80P80 | HE_PHY_CH_WIDTH_5G_242TONE);
		if (!(WL_BW_CAP_40MHZ(wlc->band->bw_cap))) {
			channel_width_set &= ~(HE_PHY_CH_WIDTH_2G_40);
		}
	} else {
		channel_width_set &= ~(HE_PHY_CH_WIDTH_2G_40 | HE_PHY_CH_WIDTH_2G_242TONE);
		if (wlc_he_bw80_limited(wlc, cfg)) {
			channel_width_set &= ~(HE_PHY_CH_WIDTH_5G_160 | HE_PHY_CH_WIDTH_5G_80P80);
			/* If limited to 20Mhz only then report this (if we are non-AP). */
			if (BSSCFG_STA(cfg) && (WL_BW_CAP_20MHZ(wlc->band->bw_cap)) &&
				!(WL_BW_CAP_40MHZ(wlc->band->bw_cap))) {
				channel_width_set &= ~(HE_PHY_CH_WIDTH_5G_80);
			}
		}
	}
	setbits((uint8 *)&cap->phy_cap, sizeof(cap->phy_cap), HE_PHY_CH_WIDTH_SET_IDX,
		HE_PHY_CH_WIDTH_SET_FSZ, channel_width_set);

	offset += (uint)(sizeof(he_mac_cap_t) + sizeof(he_phy_cap_t));

	/* HE TX-RX MCS-NSS support */
	rateset = &wlc->bandstate[wlc->band->bandunit]->hw_rateset;
	ptr = (uint8 *)cap + offset;
	bw_info = ptr;
	setbits(bw_info, HE_BW_RASETSZ, HE_BW80_RX_RATESET_OFFSET * NBBY,
		HE_BW_RX_RASETSZ * NBBY, rateset->he_bw80_rx_mcs_nss);
	setbits(bw_info, HE_BW_RASETSZ, HE_BW80_TX_RATESET_OFFSET * NBBY,
		HE_BW_TX_RASETSZ * NBBY, rateset->he_bw80_tx_mcs_nss);
	max_mcs_rx = wlc_he_max_mcs_idx(rateset->he_bw80_rx_mcs_nss);
	max_mcs_tx = wlc_he_max_mcs_idx(rateset->he_bw80_tx_mcs_nss);
	ptr += HE_BW_RASETSZ;
	if (channel_width_set & HE_PHY_CH_WIDTH_5G_160) {
		setbits(bw_info, 2 * HE_BW_RASETSZ, HE_BW160_RX_RATESET_OFFSET * NBBY,
			HE_BW_RX_RASETSZ * NBBY, rateset->he_bw160_rx_mcs_nss);
		setbits(bw_info, 2 * HE_BW_RASETSZ, HE_BW160_TX_RATESET_OFFSET * NBBY,
			HE_BW_TX_RASETSZ * NBBY, rateset->he_bw160_tx_mcs_nss);
		ptr += HE_BW_RASETSZ;
		mcs_tmp = wlc_he_max_mcs_idx(rateset->he_bw160_rx_mcs_nss);
		if (mcs_tmp > max_mcs_rx) {
			max_mcs_rx = mcs_tmp;
		}
		mcs_tmp = wlc_he_max_mcs_idx(rateset->he_bw160_tx_mcs_nss);
		if (mcs_tmp > max_mcs_tx) {
			max_mcs_tx = mcs_tmp;
		}
	}
	if (channel_width_set & HE_PHY_CH_WIDTH_5G_80P80) {
		setbits(bw_info, HE_BW_RASETSZ, HE_BW80P80_RX_RATESET_OFFSET * NBBY,
			HE_BW_RX_RASETSZ * NBBY, rateset->he_bw80p80_rx_mcs_nss);
		setbits(bw_info, HE_BW_RASETSZ, HE_BW80P80_TX_RATESET_OFFSET * NBBY,
			HE_BW_TX_RASETSZ * NBBY, rateset->he_bw80p80_tx_mcs_nss);
		ptr += HE_BW_RASETSZ;
		mcs_tmp = wlc_he_max_mcs_idx(rateset->he_bw80p80_rx_mcs_nss);
		if (mcs_tmp > max_mcs_rx) {
			max_mcs_rx = mcs_tmp;
		}
		mcs_tmp = wlc_he_max_mcs_idx(rateset->he_bw80p80_tx_mcs_nss);
		if (mcs_tmp > max_mcs_tx) {
			max_mcs_tx = mcs_tmp;
		}
	}

	if (max_mcs_rx < HE_LOWEST_QAM1024_MCS_IDX) {
		clrbits((uint8 *)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_RX_1024_QAM_LESS_242_TONE_RU_IDX,
			HE_PHY_RX_1024_QAM_LESS_242_TONE_RU_FSZ);
	}
	if (max_mcs_tx < HE_LOWEST_QAM1024_MCS_IDX) {
		clrbits((uint8 *)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_TX_1024_QAM_LESS_242_TONE_RU_IDX,
			HE_PHY_TX_1024_QAM_LESS_242_TONE_RU_FSZ);
	}

	/* Initialize default HE PPE Thresholds field */
	if (hei->ppe_ths.ppe_ths_len > 0) {
		setbit((uint8*)&cap->phy_cap, HE_PHY_PPE_THRESH_PRESENT_IDX);
		memcpy(ptr, hei->ppe_ths.ppe_ths, hei->ppe_ths.ppe_ths_len);
	}

#ifdef TESTBED_AP_11AX
	/* When AP is set for testbed mode then some features are to be disabled by default */
	if (hei->testbed_mode) {
		clrbit((uint8 *)&cap->mac_cap, HE_MAC_OM_UL_MU_DATA_DISABLE_IDX);

		clrbit((uint8 *)&cap->phy_cap, HE_PHY_SU_CODEBOOK_SUPPORT_IDX);
		clrbit((uint8 *)&cap->phy_cap, HE_PHY_MU_CODEBOOK_SUPPORT_IDX);
		clrbit((uint8 *)&cap->phy_cap, HE_PHY_SU_FEEDBACK_NG16_SUPPORT_IDX);
		clrbit((uint8 *)&cap->phy_cap, HE_PHY_UL_MU_PYLD_IDX);
		clrbit((uint8 *)&cap->phy_cap, HE_PHY_NDP_4x_LTF_3_2_GI_RX_IDX);

		clrbits((uint8 *)&cap->mac_cap, sizeof(cap->mac_cap), HE_MAC_MAX_AMPDU_LEN_EXP_IDX,
			HE_MAC_MAX_AMPDU_LEN_EXP_FSZ);
	}
#endif /* TESTBED_AP_11AX */

	return BCME_OK;
}

static int
wlc_he_parse_cap_ppe_thresh(wlc_he_info_t *hei, scb_he_t *sh, uint8 *cap_ptr, uint length)
{
	wlc_info_t *wlc = hei->wlc;
	uint nss, ru, num_ru, nss_idx;
	uint ru_r_idx, ru_w_idx;
	uint32 current_ppet16;
	uint32 current_ppet8;
	uint32 ppet16;
	uint32 ppet8;

	BCM_REFERENCE(wlc);

	/* cap_ptr points to PPE field, read nss(m1) and ru bitmap */
	nss = getbits(cap_ptr, 1, HE_NSSM1_IDX, HE_NSSM1_LEN) + 1;
	ru = getbits(cap_ptr, 1, HE_RU_INDEX_MASK_IDX, HE_RU_INDEX_MASK_LEN);
	num_ru = bcm_bitcount((uint8*) &ru, 1);
	if (HE_PPE_THRESH_LEN(nss, num_ru) > length) {
		WL_HE_INFO(("wl%d: %s: Invalid PPE threshold Cap IE len %d, %d\n",
			wlc->pub->unit, __FUNCTION__, HE_PPE_THRESH_LEN(nss, num_ru), length));
		return BCME_ERROR;
	} else if (nss > HE_CAP_MCS_MAP_NSS_MAX) {
		WL_HE_INFO(("wl%d: %s: Invalid PPE_NUM_NSS %d\n", wlc->pub->unit, __FUNCTION__,
			nss));
		return BCME_ERROR;
	} else if (num_ru > HE_MAX_RU_COUNT) {
		WL_HE_INFO(("wl%d: %s: Invalid PPE_NUM_RU %d\n", wlc->pub->unit, __FUNCTION__, ru));
		return BCME_ERROR;
	}

	/* JIRA SWWLAN-178834 has pdf attached explaining format to store info. We store info in
	 * the format such that it can be used by ucode and only requires a copy. JIRA SWWLAN-194366
	 * provides additional information on how to parse the PPET values.
	 */
	sh->min_ppe_nss = -1;
	for (nss_idx = 0; nss_idx < nss; nss_idx++) {
		for (ru_r_idx = 0, ru_w_idx = 0; ru_w_idx < HE_MAX_RU_COUNT; ru_w_idx++) {
			if ((ru & NBITVAL(ru_w_idx)) != 0) {
				/* determine if the supplied value by remote sta indicates
				 * higher ppet value. This is quite difficult due to way the
				 * ppet values are encoded, see specification and JIRA
				 */
				current_ppet16 = sh->ppe_nss[nss_idx] >>
					(ru_w_idx * HE_PPE_THRESH_NSS_RU_FSZ * 2);
				current_ppet16 &= HE_PPE_THRESH_NSS_RU_MASK;
				current_ppet8 = sh->ppe_nss[nss_idx] >>
					((ru_w_idx * 2) + 1) * HE_PPE_THRESH_NSS_RU_FSZ;
				current_ppet8 &= HE_PPE_THRESH_NSS_RU_MASK;
				/* combined read of PPET16 & PPET8 */
				ppet16 = getbits(cap_ptr, length,
					HE_PPET16_BIT_OFFSET(num_ru, nss_idx, ru_r_idx),
					2 * HE_PPE_THRESH_NSS_RU_FSZ);
				ru_r_idx++;
				ppet8 = ppet16 >> HE_PPE_THRESH_NSS_RU_FSZ;
				ppet16 &= HE_PPE_THRESH_NSS_RU_MASK;
				if (ppet16 < current_ppet16) {
					current_ppet16 = ppet16;
				}
				if (ppet8 < current_ppet8) {
					current_ppet8 = ppet8;
				}
				if ((current_ppet8 >= current_ppet16) &&
				    (current_ppet8 != HE_CONST_IDX_NONE)) {
					/* invalid combo created. Move ppet8 index below ppet16
					 * or if ppet16 = 0 then set ppet8 to none
					 */
					if (current_ppet16 == HE_CONST_IDX_BPSK) {
						current_ppet8 = HE_CONST_IDX_NONE;
					} else {
						current_ppet8 = current_ppet16 - 1;
					}

				}
				sh->ppe_nss[nss_idx] &= ~(((HE_PPE_THRESH_NSS_RU_MASK <<
					HE_PPE_THRESH_NSS_RU_FSZ) | HE_PPE_THRESH_NSS_RU_MASK) <<
					(ru_w_idx * HE_PPE_THRESH_NSS_RU_FSZ * 2));
				sh->ppe_nss[nss_idx] |= (((current_ppet8 <<
					HE_PPE_THRESH_NSS_RU_FSZ) | current_ppet16) <<
					(ru_w_idx * HE_PPE_THRESH_NSS_RU_FSZ * 2));

				if (sh->min_ppe_nss > sh->ppe_nss[nss_idx]) {
					sh->min_ppe_nss = sh->ppe_nss[nss_idx];
				}
			}
		}
		ASSERT(ru_r_idx == num_ru);
	}

	return BCME_OK;
}

/* Extract he rateset from capabilities ie */
static int
wlc_he_parse_rateset_cap_ie(wlc_info_t *wlc, he_cap_ie_t *cap, wlc_he_rateset_t *he_rateset)
{
	uint min_ie_size = sizeof(*cap) - TLV_HDR_LEN + HE_MIN_HE_RATESETSZ;
	uint32 channel_width_set;
	uint8 *bw_info;

	he_rateset->bw80_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	he_rateset->bw80_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	he_rateset->bw160_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	he_rateset->bw160_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	he_rateset->bw80p80_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	he_rateset->bw80p80_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;

	if (cap->len < min_ie_size) {
		return BCME_ERROR;
	}
	channel_width_set = getbits((uint8*)&cap->phy_cap, sizeof(cap->phy_cap),
		HE_PHY_CH_WIDTH_SET_IDX, HE_PHY_CH_WIDTH_SET_FSZ);
	bw_info = (uint8*)cap->phy_cap + HE_PHY_CAP_INFO_SIZE;

	he_rateset->bw80_rx_mcs_nss = getbits(bw_info, HE_BW_RASETSZ,
		HE_BW80_RX_RATESET_OFFSET * NBBY, HE_BW_RX_RASETSZ * NBBY);
	he_rateset->bw80_tx_mcs_nss = getbits(bw_info, HE_BW_RASETSZ,
		HE_BW80_TX_RATESET_OFFSET * NBBY, HE_BW_TX_RASETSZ * NBBY);

	if ((!(channel_width_set & HE_PHY_CH_WIDTH_5G_160)) || (BAND_2G(wlc->band->bandtype))) {
		return BCME_OK;
	}
	min_ie_size += HE_BW_RASETSZ;
	if (cap->len < min_ie_size) {
		return BCME_ERROR;
	}

	he_rateset->bw160_rx_mcs_nss = getbits(bw_info, 2 * HE_BW_RASETSZ,
		HE_BW160_RX_RATESET_OFFSET * NBBY, HE_BW_RX_RASETSZ * NBBY);
	he_rateset->bw160_tx_mcs_nss = getbits(bw_info, 2 * HE_BW_RASETSZ,
		HE_BW160_TX_RATESET_OFFSET * NBBY, HE_BW_TX_RASETSZ * NBBY);

	if (!(channel_width_set & HE_PHY_CH_WIDTH_5G_80P80)) {
		return BCME_OK;
	}
	min_ie_size += HE_BW_RASETSZ;
	if (cap->len < min_ie_size) {
		return BCME_ERROR;
	}

	he_rateset->bw80p80_rx_mcs_nss = getbits(bw_info, 3 * HE_BW_RASETSZ,
		HE_BW80P80_RX_RATESET_OFFSET * NBBY, HE_BW_RX_RASETSZ * NBBY);
	he_rateset->bw80p80_tx_mcs_nss = getbits(bw_info, 3 * HE_BW_RASETSZ,
		HE_BW80P80_TX_RATESET_OFFSET * NBBY, HE_BW_TX_RASETSZ * NBBY);

	return BCME_OK;
}

/** called on reception of a DOT11_MNG_HE_CAP_ID ie to process it */
static void
wlc_he_parse_cap_ie(wlc_he_info_t *hei, scb_t *scb, he_cap_ie_t *cap)
{
	scb_he_t *sh;
	wlc_info_t *wlc = hei->wlc;
	const uint fixed_ie_size = sizeof(*cap) - TLV_HDR_LEN + HE_MIN_HE_RATESETSZ;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	wlc_bss_info_t *bi = cfg->target_bss;
	wlc_he_rateset_t he_rateset;
	wlc_rateset_t *rateset;
	wlc_rateset_t *our_rateset;
	uint32 channel_width_set;
	uint mcs_nss_setsz;
	uint8 *data;
	uint32 default_ppe_nss;
	uint nss;
	uint16 omi_rx_nss;
	uint16 omi_bw;
	uint16 omi_tx_nss;
	uint8 oper_mode;
	uint legacy_omi_bw;
	uint bw;
	uint rxstreams = wlc_stf_rxstreams_get(wlc);

	/* Clear HE flag */
	scb->flags2 &= ~SCB2_HECAP;

	sh = SCB_HE(hei, scb);
	ASSERT(sh != NULL);

	bzero(&sh->mac_cap, sizeof(sh->mac_cap));
	bzero(&sh->phy_cap, sizeof(sh->phy_cap));

	if (cap == NULL) {
		return;
	}

	if (cap->len < fixed_ie_size) {
		WL_HE_INFO(("wl%d: %s: Invalid CAP IE len %d\n", wlc->pub->unit, __FUNCTION__,
			cap->len));
		return;
	}

	memcpy(&sh->mac_cap, &cap->mac_cap, sizeof(sh->mac_cap));
	memcpy(&sh->phy_cap, &cap->phy_cap, sizeof(sh->phy_cap));

	/* Set HE capability if HE MAC CAP IE is not empty */
	SCB_SET_HE_CAP(scb);

	/* HE HTC support */
	if (D11REV_GE(wlc->pub->corerev, 129)) {
		if (getbits((uint8 *)&sh->mac_cap, sizeof(sh->mac_cap), HE_MAC_HTC_HE_SUPPORT_IDX,
			HE_MAC_HTC_HE_SUPPORT_FSZ)) {
			sh->flags |= SCB_HE_HTC_CAP;
		}
	}

	if (TWT_ENAB(wlc->pub)) {
		wlc_twt_scb_set_cap(wlc->twti, scb, WLC_TWT_CAP_BCAST_TWT_SUPPORT,
			isset((uint8 *)&sh->mac_cap, HE_MAC_BCAST_TWT_SUPPORT_IDX));
		wlc_twt_scb_set_cap(wlc->twti, scb, WLC_TWT_CAP_TWT_RESP_SUPPORT,
			isset((uint8 *)&sh->mac_cap, HE_MAC_TWT_RESP_SUPPORT_IDX));
	}

	sh->trig_mac_pad_dur = (uint8)getbits((uint8 *)&sh->mac_cap, sizeof(sh->mac_cap),
		HE_MAC_TRG_PAD_DUR_IDX, HE_MAC_TRG_PAD_DUR_FSZ);
	sh->trig_mac_pad_dur <<= 3; /* convert from 8 usec unit to usec */

	sh->multi_tid_agg_num = (uint8)getbits((uint8 *)&sh->mac_cap, sizeof(sh->mac_cap),
		HE_MAC_MULTI_TID_AGG_IDX, HE_MAC_MULTI_TID_AGG_FSZ) + 1;

	/* LDPC coding in payload */
	if (getbits((uint8 *)&sh->phy_cap, sizeof(sh->phy_cap),
		HE_PHY_LDPC_PYLD_IDX, HE_PHY_LDPC_PYLD_FSZ)) {
		sh->flags |= SCB_HE_LDPCCAP;
	}

	/* Check if the peer can Rx 1024 QAM in <242 tone RU */
	if (getbits((uint8*)&hei->def_phy_cap, sizeof(hei->def_phy_cap),
		HE_PHY_TX_1024_QAM_LESS_242_TONE_RU_IDX,
		HE_PHY_TX_1024_QAM_LESS_242_TONE_RU_FSZ) &&
		getbits((uint8 *)&sh->phy_cap, sizeof(sh->phy_cap),
			HE_PHY_RX_1024_QAM_LESS_242_TONE_RU_IDX,
			HE_PHY_RX_1024_QAM_LESS_242_TONE_RU_FSZ)) {
		sh->flags |= SCB_HE_DL_QAM1024_LT242;
	}
	/* Check if the peer can Tx 1024 QAM in <242 tone RU */
	if (getbits((uint8*)&hei->def_phy_cap, sizeof(hei->def_phy_cap),
		HE_PHY_RX_1024_QAM_LESS_242_TONE_RU_IDX,
		HE_PHY_RX_1024_QAM_LESS_242_TONE_RU_FSZ) &&
		getbits((uint8 *)&sh->phy_cap, sizeof(sh->phy_cap),
			HE_PHY_TX_1024_QAM_LESS_242_TONE_RU_IDX,
			HE_PHY_TX_1024_QAM_LESS_242_TONE_RU_FSZ)) {
		sh->flags |= SCB_HE_UL_QAM1024_LT242;
	}

	/* Check device class */
	if (getbits((uint8 *)&sh->phy_cap, sizeof(sh->phy_cap),
		HE_PHY_DEVICE_CLASS_IDX, HE_PHY_DEVICE_CLASS_FSZ)) {
		sh->flags |= SCB_HE_DEVICE_CLASS;
	}

	/* Check if remote is BSR capable */
	if (isset((uint8 *)&sh->mac_cap, HE_MAC_A_BSR_IDX)) {
		sh->flags |= SCB_HE_BSR_CAPABLE;
	}

	/**
	 * TX-RX HE-MCS support:
	 */
	if (wlc_he_parse_rateset_cap_ie(wlc, cap, &he_rateset)) {
		WL_HE_INFO(("wl%d: %s: Parsing CAP IE HE rateset failed\n", wlc->pub->unit,
			__FUNCTION__));
		return;
	}

	/* Two reasons for getting here: assoc_req or assoc_resp. The req needs intersection
	 * but the response should in theory be good. However, it is easier and safer to intersect.
	 */
	rateset = &scb->rateset;
	our_rateset = &wlc->bandstate[wlc->band->bandunit]->hw_rateset;
	/* In rateset it is stored from 'our' perspective. So our tx becomes intersection of
	 * our tx capability and remote rx capability.
	 */
	rateset->he_bw80_tx_mcs_nss = wlc_he_rateset_intersection(he_rateset.bw80_rx_mcs_nss,
		our_rateset->he_bw80_tx_mcs_nss);
	rateset->he_bw80_rx_mcs_nss = wlc_he_rateset_intersection(he_rateset.bw80_tx_mcs_nss,
		our_rateset->he_bw80_rx_mcs_nss);
	rateset->he_bw160_tx_mcs_nss = wlc_he_rateset_intersection(he_rateset.bw160_rx_mcs_nss,
		our_rateset->he_bw160_tx_mcs_nss);
	rateset->he_bw160_rx_mcs_nss = wlc_he_rateset_intersection(he_rateset.bw160_tx_mcs_nss,
		our_rateset->he_bw160_rx_mcs_nss);
	rateset->he_bw80p80_tx_mcs_nss = wlc_he_rateset_intersection(he_rateset.bw80p80_rx_mcs_nss,
		our_rateset->he_bw80p80_tx_mcs_nss);
	rateset->he_bw80p80_rx_mcs_nss = wlc_he_rateset_intersection(he_rateset.bw80p80_tx_mcs_nss,
		our_rateset->he_bw80p80_rx_mcs_nss);
	/* Apply BW limitation */
	if (wlc_he_bw80_limited(wlc, cfg)) {
		rateset->he_bw160_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
		rateset->he_bw160_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
		rateset->he_bw80p80_tx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
		rateset->he_bw80p80_rx_mcs_nss = HE_CAP_MAX_MCS_NONE_ALL;
	}
	/* Now apply the rx and tx chain limits */
	for (nss = rxstreams + 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80_rx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw160_rx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80p80_rx_mcs_nss);
	}
	for (nss = WLC_BITSCNT(wlc->stf->txchain) + 1; nss <= HE_CAP_MCS_MAP_NSS_MAX; nss++) {
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80_tx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw160_tx_mcs_nss);
		HE_MCS_MAP_SET_MCS_PER_SS(nss, HE_CAP_MAX_MCS_NONE, rateset->he_bw80p80_tx_mcs_nss);
	}

#ifdef HERTDBG
	WL_PRINT(("HE-Rate Cap IE mcs map\n"));
	WL_PRINT(("BW80    Remote Rx: %04x, Our Tx: %04x, intersect: %04x\n",
		he_rateset.bw80_rx_mcs_nss, hei->he_rateset.bw80_tx_mcs_nss,
		rateset->he_bw80_tx_mcs_nss));
	WL_PRINT(("BW80    Remote Tx: %04x, Our Rx: %04x, intersect: %04x\n",
		he_rateset.bw80_tx_mcs_nss, hei->he_rateset.bw80_rx_mcs_nss,
		rateset->he_bw80_rx_mcs_nss));
	WL_PRINT(("BW160   Remote Rx: %04x, Our Tx: %04x, intersect: %04x\n",
		he_rateset.bw160_rx_mcs_nss, hei->he_rateset.bw160_tx_mcs_nss,
		rateset->he_bw160_tx_mcs_nss));
	WL_PRINT(("BW160   Remote Tx: %04x, Our Rx: %04x, intersect: %04x\n",
		he_rateset.bw160_tx_mcs_nss, hei->he_rateset.bw160_rx_mcs_nss,
		rateset->he_bw160_rx_mcs_nss));
	WL_PRINT(("BW80P80 Remote Rx: %04x, Our Tx: %04x, intersect: %04x\n",
		he_rateset.bw80p80_rx_mcs_nss, hei->he_rateset.bw80p80_tx_mcs_nss,
		rateset->he_bw80p80_tx_mcs_nss));
	WL_PRINT(("BW80P80 Remote Tx: %04x, Our Rx: %04x, intersect: %04x\n",
		he_rateset.bw80p80_tx_mcs_nss, hei->he_rateset.bw80p80_rx_mcs_nss,
		rateset->he_bw80p80_rx_mcs_nss));
#endif /* HERTDBG */

	/* Capability of the BSS */
	if (BSSCFG_STA(cfg)) {
		bi->he_neg_bw80_tx_mcs = rateset->he_bw80_tx_mcs_nss;
		bi->he_neg_bw80_rx_mcs = rateset->he_bw80_rx_mcs_nss;
		bi->he_neg_bw160_tx_mcs = rateset->he_bw160_tx_mcs_nss;
		bi->he_neg_bw160_rx_mcs = rateset->he_bw160_rx_mcs_nss;
		bi->he_neg_bw80p80_tx_mcs = rateset->he_bw80p80_tx_mcs_nss;
		bi->he_neg_bw80p80_rx_mcs = rateset->he_bw80p80_rx_mcs_nss;
		wlc_he_cp_he_rateset_to_bi_sup(bi,
			&wlc->bandstate[wlc->band->bandunit]->hw_rateset);
		wlc_rateset_he_cp(&bi->rateset, rateset);
	}
	/* Copy the MCS sets for all BWs to sh. They are needed to be able to apply OMI */
	sh->bw80_tx_mcs_nss = he_rateset.bw80_tx_mcs_nss;
	sh->bw80_rx_mcs_nss = he_rateset.bw80_rx_mcs_nss;
	sh->bw160_tx_mcs_nss = he_rateset.bw160_tx_mcs_nss;
	sh->bw160_rx_mcs_nss = he_rateset.bw160_rx_mcs_nss;
	sh->bw80p80_tx_mcs_nss = he_rateset.bw80p80_tx_mcs_nss;
	sh->bw80p80_rx_mcs_nss = he_rateset.bw80p80_rx_mcs_nss;

	/* Initialize OMI values with capabilities, so no limit gets accidently applied */
	if (getbits((uint8 *)&sh->mac_cap, sizeof(sh->mac_cap),
		HE_MAC_OM_CONTROL_SUPPORT_IDX, HE_MAC_OM_CONTROL_SUPPORT_FSZ)) {
		sh->flags |= SCB_HE_OMI;
		if (getbits((uint8 *)&sh->mac_cap, sizeof(sh->mac_cap),
			HE_MAC_OM_UL_MU_DATA_DISABLE_IDX, HE_MAC_OM_UL_MU_DATA_DISABLE_FSZ)) {
			sh->flags |= SCB_HE_OMI_UL_MU_DATA_DIS;
		}
	}

	channel_width_set = getbits((uint8*)&sh->phy_cap, sizeof(sh->phy_cap),
		HE_PHY_CH_WIDTH_SET_IDX, HE_PHY_CH_WIDTH_SET_FSZ);

	if (channel_width_set & (HE_PHY_CH_WIDTH_2G_242TONE | HE_PHY_CH_WIDTH_5G_242TONE)) {
		sh->flags |= SCB_HE_DL_242TONE;
	}
	/* For BW take the rx set. The Tx set BW limitation may be different but will
	 * get applied through rate table, it is unclear at this point if ucode needs
	 * BW limitation for Tx?
	 */
	if ((sh->bw80p80_rx_mcs_nss != HE_CAP_MAX_MCS_NONE_ALL) ||
	    (sh->bw160_rx_mcs_nss != HE_CAP_MAX_MCS_NONE_ALL)) {
		omi_bw = DOT11_OPER_MODE_160MHZ;
	} else {
		/* Max is 80, but in 2G it is 40 or 20, use chanspec to determine */
		omi_bw = DOT11_OPER_MODE_80MHZ;
		if (BAND_2G(wlc->band->bandtype)) {
			if ((CHSPEC_IS40(wlc->chanspec)) &&
			    (channel_width_set & HE_PHY_CH_WIDTH_2G_40)) {
				omi_bw = DOT11_OPER_MODE_40MHZ;
			} else {
				omi_bw = DOT11_OPER_MODE_20MHZ;
			}
		} else {
			/* 5G can be limited to 20 (or 40) only. If STA, apply BW_CAP. If AP
			 * then observe channel_width and bw_cap and chanspec.
			 */
			if (BSSCFG_AP(cfg)) {
				if (CHSPEC_IS20(wlc->chanspec) ||
					!(channel_width_set & HE_PHY_CH_WIDTH_5G_80)) {
					omi_bw = DOT11_OPER_MODE_20MHZ;
					sh->flags |= SCB_HE_5G_20MHZ_ONLY;
				} else if (CHSPEC_IS40(wlc->chanspec)) {
					omi_bw = DOT11_OPER_MODE_40MHZ;
				}
			} else {
				if (CHSPEC_IS20(wlc->chanspec)) {
					omi_bw = DOT11_OPER_MODE_20MHZ;
				} else if (CHSPEC_IS40(wlc->chanspec)) {
					omi_bw = DOT11_OPER_MODE_40MHZ;
				}
			}
		}
	}

	/* Determine omi rx nss, derive from tx mcs nss. Omi rx is remote rx, which is our tx */
	omi_rx_nss = HE_MAX_SS_SUPPORTED(sh->bw80_tx_mcs_nss);
	if ((HE_MAX_SS_SUPPORTED(sh->bw160_tx_mcs_nss)) > omi_rx_nss)
		omi_rx_nss = HE_MAX_SS_SUPPORTED(sh->bw160_tx_mcs_nss);
	if ((HE_MAX_SS_SUPPORTED(sh->bw80p80_tx_mcs_nss)) > omi_rx_nss)
		omi_rx_nss = HE_MAX_SS_SUPPORTED(sh->bw80p80_tx_mcs_nss);
	omi_rx_nss -= 1;
	omi_tx_nss = HE_MAX_SS_SUPPORTED(sh->bw80_rx_mcs_nss);
	if ((HE_MAX_SS_SUPPORTED(sh->bw160_rx_mcs_nss)) > omi_tx_nss)
		omi_tx_nss = HE_MAX_SS_SUPPORTED(sh->bw160_rx_mcs_nss);
	if ((HE_MAX_SS_SUPPORTED(sh->bw80p80_rx_mcs_nss)) > omi_tx_nss)
		omi_tx_nss = HE_MAX_SS_SUPPORTED(sh->bw80p80_rx_mcs_nss);
	omi_tx_nss -= 1;

	bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
	if (bw == BW_160MHZ) {
		legacy_omi_bw = DOT11_OPER_MODE_160MHZ;
	} else if (bw == BW_80MHZ) {
		legacy_omi_bw = DOT11_OPER_MODE_80MHZ;
	} else if (bw == BW_40MHZ) {
		legacy_omi_bw = DOT11_OPER_MODE_40MHZ;
	} else {
		legacy_omi_bw = DOT11_OPER_MODE_20MHZ;
	}
	if (legacy_omi_bw <= omi_bw) {
		omi_bw = legacy_omi_bw;
	} else {
		/* Apply the current settings in oper mode. This will make sure the determined BW
		 * will be applied by scb_ratesel
		 */
		oper_mode = DOT11_D8_OPER_MODE(0, omi_rx_nss + 1, 0,
			(omi_bw == DOT11_OPER_MODE_160MHZ) ?  1 : 0,
			(omi_bw == DOT11_OPER_MODE_160MHZ) ? DOT11_OPER_MODE_80MHZ : omi_bw);
		wlc_vht_update_scb_oper_mode(wlc->vhti, scb, oper_mode);
	}
	/* OMI HE ctrl frame, LSB -> MSB: RxNss (3b) | BW (2b) | UL_MU_dis (1b) | TxNSS (3b) |
	 * ER SU Disable (1b) | DL MU_MIMO re-sound (1b) | UL MU Data Dsiable. Note: not all
	 * fields are written. Encoding is copying HTC omi, use those defines!
	 */
	sh->omi_htc = sh->omi_lm = HE_OMI_ENCODE(omi_rx_nss, omi_tx_nss, omi_bw, 0, 0, 0, 0);
	/* pmqdata updates only lower 5bits of RXNSS | BW */
	sh->omi_pmq = (sh->omi_htc & HE_TXRATE_UPDATE_MASK);

	/* Parse PPET values only once. */
	if (sh->ppet_valid == FALSE) {
		/* Only parse PPET information once, if not available set it to 0 us */
		sh->ppet_valid = TRUE;
		/* Default the ppet values with our own limitation, then parse IE (if available)
		 * to adjust to possible higher value.
		 */
		if (hei->ppet == WL_HE_PPET_0US) {
			default_ppe_nss = (HE_CONST_IDX_NONE << HE_PPE_THRESH_NSS_RU_FSZ) |
				HE_CONST_IDX_NONE;
		} else if (hei->ppet == WL_HE_PPET_8US) {
			default_ppe_nss = (HE_CONST_IDX_BPSK << HE_PPE_THRESH_NSS_RU_FSZ) |
				HE_CONST_IDX_NONE;
		} else { /* 16 usec */
			default_ppe_nss = (HE_CONST_IDX_NONE << HE_PPE_THRESH_NSS_RU_FSZ) |
				HE_CONST_IDX_BPSK;
		}
		default_ppe_nss = HE_EXPAND_PPET_ALL_RU(default_ppe_nss);
		for (nss = 0; nss < ARRAYSIZE(sh->ppe_nss); nss++) {
			sh->ppe_nss[nss] = default_ppe_nss;
		}
		/* First check if PPET IE is available */
		if (isset(&sh->phy_cap, HE_PHY_PPE_THRESH_PRESENT_IDX)) {
			/* need to skip var supported HE-MCS and NSS Set field */
			mcs_nss_setsz = 0;
			if ((channel_width_set & HE_PHY_CH_WIDTH_5G_160) &&
			    BAND_5G(wlc->band->bandtype))
				mcs_nss_setsz += HE_BW_RASETSZ;
			if ((channel_width_set & HE_PHY_CH_WIDTH_5G_80P80) &&
			    BAND_5G(wlc->band->bandtype))
				mcs_nss_setsz += HE_BW_RASETSZ;

			if (cap->len <= fixed_ie_size + mcs_nss_setsz) {
				WL_HE_INFO(("wl%d: %s: Invalid CAP IE len (PPE) %d\n",
					wlc->pub->unit, __FUNCTION__, cap->len));
			} else {
				data = (uint8*)cap->phy_cap + HE_PHY_CAP_INFO_SIZE +
					HE_MIN_HE_RATESETSZ;
				wlc_he_parse_cap_ppe_thresh(hei, sh, data + mcs_nss_setsz,
					cap->len - (fixed_ie_size + mcs_nss_setsz));
			}
		}
	}

#ifdef WL_BEAMFORMING
	/* beamforming caps */
	if (TXBF_ENAB(wlc->pub)) {
		uint8 su_bfe, su_bfr, mu_bfr;
		WL_HE_INFO(("wl%d %s ie %d len %d id_ext %d\n", wlc->pub->unit, __FUNCTION__,
			cap->id, cap->len, cap->id_ext));

		su_bfr = getbits((uint8 *)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_SU_BEAMFORMER_IDX, HE_PHY_SU_BEAMFORMER_FSZ);
		su_bfe = getbits((uint8 *)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_SU_BEAMFORMEE_IDX, HE_PHY_SU_BEAMFORMEE_FSZ);
		mu_bfr = getbits((uint8 *)&cap->phy_cap, sizeof(cap->phy_cap),
			HE_PHY_MU_BEAMFORMER_IDX, HE_PHY_MU_BEAMFORMER_FSZ);

		/* peer should be SU beamformer as well to become MU beamformer */
		if (su_bfr) {
			sh->flags |= SCB_HE_SU_BFR;
			if (mu_bfr)
				sh->flags |= SCB_HE_MU_BFR;
		}

		/* non-AP STA has to support SU_BFE. For HE, SU_BFE=1 implies MU_BFE=1 */
		if (su_bfe) {
			 sh->flags |= SCB_HE_SU_MU_BFE;
		}
		wlc_txbf_scb_state_upd(hei->wlc->txbf, scb, (uint8 *) &cap->phy_cap,
			sizeof(cap->phy_cap), TXBF_CAP_TYPE_HE);
	}
#endif /*  WL_BEAMFORMING */

	/* Trigger ratelinkmem update to make sure all values get programmed */
	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_upd_lmem_int(wlc, scb, TRUE /* clr_txbf_sts=1 in mreq */);
	}
}

/** called on reception of assocreq/reassocreq/assocresp/reassocresp */
static int
wlc_he_assoc_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_he_info_t *hei = ctx;

	BCM_REFERENCE(hei);

	switch (data->ft) {
#ifdef AP
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ: {
		wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
		scb_t *scb = ftpparm->assocreq.scb;

		ASSERT(scb != NULL);

		wlc_he_parse_cap_ie(hei, scb, (he_cap_ie_t *)data->ie);

		ftpparm->assocreq.he_cap_ie = data->ie;
		break;
	}
#endif /* AP */
#ifdef STA
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP: {
		wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
		scb_t *scb = ftpparm->assocresp.scb;

		ASSERT(scb != NULL);

		wlc_he_parse_cap_ie(hei, scb, (he_cap_ie_t *)data->ie);
		break;
	}
#endif /* STA */
	default:
		break;
	}

	return BCME_OK;
}

static int
wlc_he_scan_parse_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	he_cap_ie_t *cap;
	wlc_iem_ft_pparm_t *ftpparm;
	wlc_bss_info_t *bi;
	const uint fixed_ie_size = sizeof(*cap) - TLV_HDR_LEN + HE_MIN_HE_RATESETSZ;
	wlc_he_rateset_t he_rateset;
	wlc_rateset_t rateset;
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;

	BCM_REFERENCE(wlc);

	ftpparm = data->pparm->ft;
	bi = ftpparm->scan.result;

	bi->flags3 &= ~(WLC_BSS3_HE);

	if ((cap = (he_cap_ie_t *)data->ie) == NULL) {
		return BCME_OK;
	}

	if (cap->len < fixed_ie_size) {
		WL_HE_INFO(("wl%d: %s: Invalid CAP IE len %d\n", wlc->pub->unit, __FUNCTION__,
			cap->len));
		return BCME_OK;
	}

	if (wlc_he_parse_rateset_cap_ie(wlc, cap, &he_rateset)) {
		WL_HE_INFO(("wl%d: %s: Parsing CAP IE HE rateset failed\n", wlc->pub->unit,
			__FUNCTION__));
		return BCME_OK;
	}

	wlc_he_cp_rateset_to_wlc(&he_rateset, &rateset);
	wlc_he_cp_he_rateset_to_bi_sup(bi, &rateset);
	wlc_he_init_bi_rateset_to_none(bi);

	/* Mark the BSS as HE capable */
	bi->flags3 |= WLC_BSS3_HE;

	return BCME_OK;
}

/* HE OP IE */
static uint
wlc_he_calc_op_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	if (!data->cbparm->he)
		return 0;
	if (!BSSCFG_AP(data->cfg))
		return 0;

	return sizeof(he_op_ie_t);
}

/** AP centric, called on transmit of bcn/prbrsp/assocresp/reassocresp */
static int
wlc_he_write_op_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg = data->cfg;
	he_op_ie_t *op = (he_op_ie_t *)data->buf;
	bss_he_info_t *bhei = BSS_HE_INFO(hei, cfg);
	uint16 mcs_nss;

	BCM_REFERENCE(wlc);

	bzero(op, sizeof(*op));

	ASSERT(BSSCFG_AP(cfg));

	/* IE header */
	op->id = DOT11_MNG_ID_EXT_ID;
	op->id_ext = EXT_MNG_HE_OP_ID;
	op->len = sizeof(*op) - TLV_HDR_LEN;

	/* BSS Color value */
	setbits((uint8 *)&op->color, sizeof(op->color), HE_OP_BSS_COLOR_IDX, HE_OP_BSS_COLOR_FSZ,
		hei->bsscolor);
	/* The color is updated when the "global" color has changed or when color disable field
	 * gets cleared, e.g. disable goes from TRUE => FALSE. The latter requires color update as
	 * well, as this this the place where the color gets programmed to phy, but only when
	 * bsscolor+counter reaches 1 which is the moment the switch occurs.
	 */
	if ((bhei->bsscolor != hei->bsscolor) ||
	    ((bhei->bsscolor_disable) && (!hei->bsscolor_disable) &&
	     (hei->update_bsscolor_counter == 1))) {
		bhei->bsscolor = hei->bsscolor;
		/* find all scbs for this bsscfg and get ratelinkmem updated */
		if (RATELINKMEM_ENAB(wlc->pub)) {
			wlc_ratelinkmem_update_link_entry_all(wlc, cfg, FALSE,
				FALSE /* clr_txbf_stats=0 in mreq */);
		}
		/* Configure phy rx filter */
		bhei->bsscolor_disable = hei->bsscolor_disable;
		wlc_he_configure_bsscolor_phy(hei, cfg);
	}
	/* BSS Color disabled */
	bhei->bsscolor_disable = hei->bsscolor_disable;
	if ((hei->bsscolor_disable) || (hei->update_bsscolor_counter > 1)) {
		setbit(&op->color, HE_OP_BSS_COLOR_DISABLED_IDX);
	}

	/* Default PE Duration */
	setbits((uint8 *)&op->parms, sizeof(op->parms), HE_OP_DEF_PE_DUR_IDX, HE_OP_DEF_PE_DUR_FSZ,
		bhei->pe_duration);

	/* HE duration based RTS Threshold */
	setbits((uint8 *)&op->parms, sizeof(op->parms), HE_OP_HE_DUR_RTS_THRESH_IDX,
		HE_OP_HE_DUR_RTS_THRESH_FSZ,
		(bhei->rts_thresh >> HE_RTS_DUR_THRESHOLD_32USEC_SHIFT));

	/* Partial BSS Color bit */
	setbits((uint8 *)&op->color, sizeof(op->color), HE_OP_BSS_COLOR_PARTIAL_IDX,
		HE_OP_BSS_COLOR_PARTIAL_FSZ, bhei->partial_bsscolor_ind);

	/**
	 * XXX: Basic HE MCS NSS set
	 * Implementation needed. intersection of all HE capable STAs associated to this
	 * AP and all MBSS APs of the mcs nss capabilities. Odly it doesnt define the tx or rx
	 * rate. It doesnt seem to be of much importance for AP. Initially setting to AP cap
	 * should suffice. Can be updated later for higher complexity. Needs to be stored in HEI
	 * and upated by handlers for station add/remove, so it wont have to be calculated every
	 * beacon.
	 */
	/**
	 * During PF1 Intel found our default not acceptable. We used he_bw80_tx_mcs_nss, and they
	 * saw that as minimum and couldnt fullfill that. So for now we set it to really bare
	 * minium, which is NSS1*MCS7
	 */
	mcs_nss =
		(HE_CAP_MAX_MCS_0_7 << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(1)) |
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(2)) |
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(3)) |
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(4)) |
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(5)) |
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(6)) |
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(7)) |
		(HE_CAP_MAX_MCS_NONE << HE_CAP_MAX_MCS_NSS_GET_SS_IDX(8));
	op->basic_mcs_nss_set = htol16(mcs_nss);

	return BCME_OK;
}

/** called on reception of a DOT11_MNG_HE_OP_ID ie to process it */
static int
wlc_he_parse_op_ie(void *ctx, wlc_iem_parse_data_t *data)
{
#ifdef STA
	wlc_he_info_t *hei = ctx;
	scb_t *scb;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg;
	bss_he_info_t *bhei;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	he_op_ie_t *op_ie;
	uint16 he_RTSThresh;
	uint8 bsscolor;
	bool bsscolor_disable;
	bool trigger_ratelinkmem;
	uint8 pe_duration;

	if (!HE_ENAB(wlc->pub)) {
		return BCME_OK;
	}

	if (data->ie == NULL) {
		return BCME_OK;
	}

	switch (data->ft) {
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		scb = ftpparm->assocresp.scb;
		break;

	case FC_BEACON:
		scb = ftpparm->bcn.scb;
		break;

	default:
		return BCME_OK;
	}

	ASSERT(scb != NULL);
	if (!SCB_HE_CAP(scb)) {
		return BCME_OK;
	}

	cfg = SCB_BSSCFG(scb);
	bhei = BSS_HE_INFO(hei, cfg);
	op_ie = (he_op_ie_t *)data->ie;

	if (op_ie->len < sizeof(*op_ie) - TLV_HDR_LEN) {
		WL_HE_INFO(("wl%d: %s: Invalid OP IE len %d\n", wlc->pub->unit, __FUNCTION__,
			op_ie->len));
		return BCME_OK;
	}

	/* bss color change should only happen for initial parse, otherwise AP needs to follow
	 * special procedure. So for now if color changes, just accept it and program it to PHY
	 */
	bsscolor = (uint8)getbits((uint8 *)&op_ie->color, sizeof(op_ie->color), HE_OP_BSS_COLOR_IDX,
		HE_OP_BSS_COLOR_FSZ);
	/* Disable BSScolor indication */
	bsscolor_disable = (isset((uint8 *)&op_ie->color, HE_OP_BSS_COLOR_DISABLED_IDX)) ? TRUE :
		FALSE;

	/* Dealing with color changes is difficult. There is an color disable bit to
	 * indicate if the coloring is used at all. Problem is that this bit is set when
	 * color change happens and if the a color change IE is present as well then the
	 * color should still be used. So here is a little "trick" to deal with all this:
	 * We start with color 0 and disabled true. When disabled is recvd, then only color
	 * is updated, and ratelinkmem is triggered to consume the new color. So tx will
	 * follow the color. When enabled is recvd true then color is updated, ratelinkmem
	 * is triggered, and phy is updated. When disabled is recvd and color was enabled
	 * then we just disable color on the phy side. So basically it is not that hard. The
	 * recvd color is used for tx. Disabled/enabled is used to program color (or not) to
	 * phy.
	 */
	trigger_ratelinkmem = FALSE;
	if (bhei->bsscolor != bsscolor) {
		bhei->bsscolor = bsscolor;
		/* trigger ratelinkmem */
		trigger_ratelinkmem = TRUE;
		/* Set bhei->bsscolor_disable to !bsscolor_disable to trigger update */
		bhei->bsscolor_disable = !bsscolor_disable;
	}
	if (bhei->bsscolor_disable != bsscolor_disable) {
		bhei->bsscolor_disable = bsscolor_disable;
		wlc_he_configure_bsscolor_phy(hei, cfg);
	}

	/* Default PE Duration */
	pe_duration = (uint8)getbits((uint8 *)&op_ie->parms, sizeof(op_ie->parms),
		HE_OP_DEF_PE_DUR_IDX, HE_OP_DEF_PE_DUR_FSZ);
	if (bhei->pe_duration != pe_duration) {
		bhei->pe_duration = pe_duration;
		phy_hecap_write_pe_dur((phy_info_t *)WLC_PI(wlc), pe_duration);
	}

	/* TWT required */
	if (TWT_ENAB(wlc->pub)) {
		wlc_twt_set_twt_required(wlc->twti, cfg,
			(isset((uint8 *)&op_ie->parms, HE_OP_TWT_REQD_IDX)) ? 1 : 0);
		/* This means we have to join a BCAST TWT or create individual TWT
		 * if TWT is supported.
		 */
	}

	/* HE Duration based RTS Threshold */
	he_RTSThresh = (uint16)getbits((uint8 *)&op_ie->parms, sizeof(op_ie->parms),
		HE_OP_HE_DUR_RTS_THRESH_IDX, HE_OP_HE_DUR_RTS_THRESH_FSZ);
	if (he_RTSThresh >= HE_RTS_THRES_DISABLED) {
		/* max value means mode is disabled */
		he_RTSThresh = BCM_UINT16_MAX; /* set max duration */
	} else {
		he_RTSThresh <<= HE_RTS_DUR_THRESHOLD_32USEC_SHIFT; /* convert to usec */
	}
	if (bhei->rts_thresh != he_RTSThresh) {
		bhei->rts_thresh = he_RTSThresh;
		trigger_ratelinkmem = TRUE;
	}

	/**
	 * Basic HE MCS-NSS set, this information is of no use at the moment. It defines the
	 * intersection of all STAs associated with AP. or intersection of all STAs in case of
	 * IBSS, latter seems rather impossible. Since intersection can only go down, and on
	 * case of removal of one the limited stas it is almost impossible to determine that
	 * intersection can be recalculated. It requires tracking of capabilities of all STAs
	 * which are part of BSS and continous calculation of intersection. IBSS not supported at
	 * this point.
	 */

	if (trigger_ratelinkmem) {
		if (RATELINKMEM_ENAB(wlc->pub)) {
			wlc_ratelinkmem_update_link_entry(wlc, scb);
		}
	}

#else
	BCM_REFERENCE(ctx);
	BCM_REFERENCE(data);
#endif /* STA */
	return BCME_OK;
}

/* ======== misc ========= */

/* is the hardware HE capable */
static bool
wlc_he_hw_cap(wlc_info_t *wlc)
{
	return WLC_HE_CAP_PHY(wlc);
}

/* update scb using the cap and op contents */
/* Note - capie and opie are in raw format i.e. LSB. */
void
wlc_he_update_scb_state(wlc_he_info_t *hei, int bandtype, scb_t *scb,
	he_cap_ie_t *capie, he_op_ie_t *opie)
{
	wlc_he_parse_cap_ie(hei, scb, capie);
}

static uint8
wlc_he_get_dynfrag(wlc_info_t *wlc)
{
	return wlc->hei->dynfrag;
}

uint16
wlc_he_get_peer_caps(wlc_he_info_t *hei, scb_t *scb)
{
	scb_he_t *sh =
		(scb_he_t *)SCB_HE(hei, scb);
	if (!sh) {
		return 0;
	} else {
		return sh->flags;
	}
}

uint8
wlc_he_scb_get_bfr_nr(wlc_he_info_t *hei, scb_t *scb)
{
	scb_he_t *sh =
		(scb_he_t *)SCB_HE(hei, scb);
	if (!sh) {
		return 0;
	} else {
		return getbits((uint8*)&sh->phy_cap, sizeof(sh->phy_cap),
			HE_PHY_SOUND_DIM_BELOW80MHZ_IDX, HE_PHY_SOUND_DIM_BELOW80MHZ_FSZ);
	}
}

static void
wlc_he_configure_bsscolor_phy(wlc_he_info_t *hei, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = hei->wlc;
	bss_he_info_t *bhei;
	phy_info_t *pi = (phy_info_t *)WLC_PI(wlc);
	wlc_bsscolor_t bsscolor;

	if (cfg == NULL) {
		bsscolor.index = HE_BSSCOLOR_APIDX;
		/* Disable BSSColor filtering */
		bsscolor.disable = TRUE;
		/* Use some valid number for color */
		bsscolor.color = 1;
	} else {
		bhei = BSS_HE_INFO(hei, cfg);
		/* Disable BSSColor filtering ? */
		bsscolor.disable = bhei->bsscolor_disable;
		bsscolor.color = bhei->bsscolor;
		if (BSSCFG_AP(cfg)) {
			bsscolor.index = HE_BSSCOLOR_APIDX;
		} else {
			bsscolor.index = HE_BSSCOLOR_STAIDX;
			/* Write AID as STAID */
			bsscolor.staid[STAID0] = HE_STAID_BSS_BCAST;
			bsscolor.staid[STAID1] = HE_STAID_MBSS_BCAST;
			bsscolor.staid[STAID2] = HE_STAID_BSS_BCAST;
			bsscolor.staid[STAID3] = (cfg->AID & HE_STAID_MASK);
		}
	}

	phy_hecap_write_bsscolor(pi, &bsscolor);
}

void
wlc_he_ap_tbtt(wlc_he_info_t *hei)
{
	if (hei->update_bsscolor_counter == 0) {
		return;
	}
	hei->update_bsscolor_counter--;
	if (hei->update_bsscolor_counter == 1) {
		hei->bsscolor = hei->new_bsscolor;
	}
	wlc_update_beacon(hei->wlc);
	wlc_update_probe_resp(hei->wlc, TRUE);
}

bool
wlc_he_partial_bss_color_get(wlc_he_info_t *hei, wlc_bsscfg_t *cfg, uint8 *color)
{
	bss_he_info_t *bhei;

	ASSERT(cfg != NULL);
	bhei = BSS_HE_INFO(hei, cfg);

	if (bhei != NULL) {
		if (color != NULL) {
			*color = bhei->bsscolor;
		}
		return bhei->partial_bsscolor_ind;
	} else {
		return FALSE;
	}
}

static uint wlc_he_calc_muedca_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_he_info_t *hei = ctx;

	if (!data->cbparm->he) {
		return 0;
	}
	if (!BSSCFG_AP(data->cfg)) {
		return 0;
	}
	if (hei->muedca_blocked) {
		return 0;
	}

	return sizeof(he_muedca_ie_t);
}

static int wlc_he_write_muedca_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_he_info_t *hei = ctx;
	wlc_bsscfg_t *cfg = data->cfg;
	bss_he_info_t *bhei = BSS_HE_INFO(hei, cfg);
	he_muedca_ie_t *muedca_ie;
	uint8 aci;

	if (hei->muedca_blocked) {
		return BCME_OK;
	}

	ASSERT(BSSCFG_AP(cfg));

	muedca_ie = (he_muedca_ie_t *)data->buf;

	muedca_ie->id = DOT11_MNG_ID_EXT_ID;
	muedca_ie->id_ext = EXT_MNG_MU_EDCA_ID;
	muedca_ie->len = sizeof(*muedca_ie) - TLV_HDR_LEN;

	muedca_ie->mu_qos_info = cfg->edca_update_count;

	for (aci = AC_BE; aci < AC_COUNT; aci++) {
		muedca_ie->ac_param[aci].aci_aifsn = bhei->sta_mu_edca[aci].aci_aifsn;
		muedca_ie->ac_param[aci].ecw_min_max = bhei->sta_mu_edca[aci].ecw_min_max;
		muedca_ie->ac_param[aci].muedca_timer = bhei->sta_mu_edca[aci].muedca_timer;
	}

	return BCME_OK;
}

/* color IE functions. color IE is added when the bss color is changing. A down counter in the IE
 * is to be updated for every beacond, and once reached 0 the color is to be 'used' and on the next
 * beacon the color IE Is to be removed. This is from the specification, explaining difference of
 * counter values between Beacons and non - Beacons:
 * A value of 0 indicates that the switch occurs at the current TBTT if the element is carried in a
 * Beacon frame or the next TBTT following the frame that carried the element if the frame is not a
 * Beacon frame.
 * The internal counter used folows a slightly different usage: 0 Means no update is going on at
 * all. 1 means, last beacon with color IE and and on next beacon the color IE will be removed.
 * On 1 also the color is updated. Other values means that the color change is approaching.
 * The reason for this method is that the color IE needs removal. So on the beacon where the color
 * change happens the color IE should be sent out with counter value 0, on the next beacon the
 * color IE should not be there anymore. To make this easy for software we use an off by one
 * downcounter.
 */
static uint
wlc_he_calc_color_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_he_info_t *hei = ctx;

	if (!data->cbparm->he)
		return 0;
	if ((data->ft == FC_BEACON) && (hei->update_bsscolor_counter == 0))
		return 0;
	if ((data->ft != FC_BEACON) && (hei->update_bsscolor_counter < 2))
		return 0;

	return sizeof(he_colorchange_ie_t);
}

static int
wlc_he_write_color_ie(void *ctx, wlc_iem_build_data_t *data)
{
	wlc_he_info_t *hei = ctx;
	he_colorchange_ie_t *colorchange_ie;

	colorchange_ie = (he_colorchange_ie_t *)data->buf;

	colorchange_ie->id = DOT11_MNG_ID_EXT_ID;
	colorchange_ie->id_ext = EXT_MNG_COLOR_CHANGE_ID;
	colorchange_ie->len = sizeof(*colorchange_ie) - TLV_HDR_LEN;
	/* If this is for Beacon frame then use counter - 1, otherwise use counter - 2 */
	if (data->ft == FC_BEACON) {
		colorchange_ie->countdown = hei->update_bsscolor_counter - 1;
	} else {
		colorchange_ie->countdown = hei->update_bsscolor_counter - 2;
	}
	colorchange_ie->newcolor = hei->new_bsscolor;

	return BCME_OK;
}

#ifdef TESTBED_AP_11AX
/**
 * Spatial reuse IE. Currently there is no limitation configured to spatial reuser, nor are
 * there any specific parameters specified. The IE is here to support Testbed AP. This is added
 * to support testcase 5.65.1
 */
static uint wlc_he_calc_sr_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	if (!data->cbparm->he)
		return 0;

	return sizeof(he_spatial_reuse_ie_t);
}

static int wlc_he_write_sr_ie(void *ctx, wlc_iem_build_data_t *data)
{
	he_spatial_reuse_ie_t *spatial_reuse_ie;

	spatial_reuse_ie = (he_spatial_reuse_ie_t *)data->buf;

	spatial_reuse_ie->id = DOT11_MNG_ID_EXT_ID;
	spatial_reuse_ie->id_ext = EXT_MNG_SRPS_ID;
	spatial_reuse_ie->len = sizeof(*spatial_reuse_ie) - TLV_HDR_LEN;
	spatial_reuse_ie->sr_control = 0;	/* No SR limitations specified */

	return BCME_OK;

}
#endif /* TESTBED_AP_11AX */

static int
wlc_he_parse_muedca_ie(void *ctx, wlc_iem_parse_data_t *data)
{
#ifdef STA
	wlc_he_info_t *hei = ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	scb_t *scb = NULL;
	wlc_bsscfg_t *bsscfg;
	bss_he_info_t *bhei;
	he_muedca_ie_t *muedca_ie;
	uint8 edca_update_count;

	if (data->ie == NULL) {
		return BCME_OK;
	}

	switch (data->ft) {
	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		scb = ftpparm->assocresp.scb;
		break;

	case FC_BEACON:
		scb = ftpparm->bcn.scb;
		break;

	default:
		return BCME_OK;
	}

	ASSERT(scb != NULL);
	if (!SCB_HE_CAP(scb)) {
		return BCME_OK;
	}

	bsscfg = scb->bsscfg;
	bhei = BSS_HE_INFO(hei, bsscfg);
	muedca_ie = (he_muedca_ie_t *)data->ie;
	edca_update_count =
		(muedca_ie->mu_qos_info & WME_QI_AP_COUNT_MASK) >> WME_QI_AP_COUNT_SHIFT;
	if (edca_update_count != bhei->edca_update_count) {
		bhei->edca_update_count = edca_update_count;

		/* Now inform Ucode about the new MU EDCA params */
	}
#endif /* STA */

	return BCME_OK;
}

static int
wlc_he_cmd_muedca(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg;
	wl_he_muedca_v1_t *muedca;
	uint8 aci;
	bss_he_info_t *bhei;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);
	bhei = BSS_HE_INFO(hei, cfg);

	if (set) {
		muedca = (wl_he_muedca_v1_t *)params;
		if (muedca->version != WL_HE_VER_1)
			return BCME_UNSUPPORTED;

		for (aci = AC_BE; aci < AC_COUNT; aci++) {
			bhei->sta_mu_edca[aci].aci_aifsn = muedca->ac_param_sta[aci].aci_aifsn;
			bhei->sta_mu_edca[aci].ecw_min_max = muedca->ac_param_sta[aci].ecw_min_max;
			bhei->sta_mu_edca[aci].muedca_timer =
				muedca->ac_param_sta[aci].muedca_timer;
		}
		cfg->edca_update_count++;
		cfg->edca_update_count &= WME_QI_AP_COUNT_MASK;
		hei->muedca_blocked = FALSE;
		if (BSSCFG_AP(cfg)) {
			if (wlc->pub->up) {
				wlc_update_beacon(wlc);
				wlc_update_probe_resp(wlc, TRUE);
			}
		}
	} else {
		muedca = (wl_he_muedca_v1_t *)result;
		muedca->version = WL_HE_VER_1;
		muedca->length = sizeof(*muedca) - 4;

		for (aci = AC_BE; aci < AC_COUNT; aci++) {
			muedca->ac_param_sta[aci].aci_aifsn = bhei->sta_mu_edca[aci].aci_aifsn;
			muedca->ac_param_sta[aci].ecw_min_max = bhei->sta_mu_edca[aci].ecw_min_max;
			muedca->ac_param_sta[aci].muedca_timer =
				bhei->sta_mu_edca[aci].muedca_timer;
		}
		*rlen = sizeof(*muedca);
	}

	return BCME_OK;

}

/** called on './wl he dynfrag' */
static int
wlc_he_cmd_dynfrag(void *ctx, uint8 *params, uint16 plen, uint8 *result,
	uint16 *rlen, bool set, wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	ASSERT(cfg != NULL);
	BCM_REFERENCE(cfg);

	if (set) {
		uint8 newfrag = *params;

		if (newfrag > HE_MAC_FRAG_VHT_MPDU) {
			return BCME_BADARG;
		}

		if (hei->dynfrag != newfrag) {
			hei->dynfrag = newfrag;
			if (BSSCFG_AP(cfg) && wlc->pub->up) {
				wlc_update_beacon(wlc);
				wlc_update_probe_resp(wlc, TRUE);
			}
		}
	} else {
		*result = hei->dynfrag;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

/** called on './wl he ppet' */
static int
wlc_he_cmd_ppet(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen, bool set,
		wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	ASSERT(cfg != NULL);
	BCM_REFERENCE(cfg);

	if (set) {
		uint8 newppet = *params;

		if (hei->ppet_override != newppet) {
			hei->ppet_override = newppet;
			wlc_he_fill_ppe_thresholds(hei);
			/* Update the beacons to override the new value */
			if (BSSCFG_AP(cfg) && wlc->pub->up) {
				wlc_update_beacon(wlc);
				wlc_update_probe_resp(wlc, TRUE);
			}
			/* trigger ratelinkmem for all scbs to get the override configured */
			if (RATELINKMEM_ENAB(wlc->pub)) {
				wlc_ratelinkmem_update_link_entry_all(wlc, cfg, FALSE,
					FALSE /* clr_txbf_stats=0 in mreq */);
			}
		}
	} else {
		*result = hei->ppet_override;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

/** called on './wl he axmode' */
static int
wlc_he_cmd_axmode(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen, bool set,
		wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	ASSERT(cfg != NULL);
	BCM_REFERENCE(cfg);

	if (set) {
		uint8 newaxmode = *params;

		if (cfg->up) {
			return BCME_NOTDOWN;
		}
		if (newaxmode != 0) {
			cfg->flags &= ~WLC_BSSCFG_HE_DISABLE;
		} else
			cfg->flags |= WLC_BSSCFG_HE_DISABLE;

	} else {
		*result = ((cfg->flags & WLC_BSSCFG_HE_DISABLE) != 0 ?
				FALSE : TRUE);
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

void
wlc_he_set_ldpc_cap(wlc_he_info_t *hei, bool enab)
{
	if (enab) {
		setbit(&hei->def_phy_cap, HE_PHY_LDPC_PYLD_IDX);
	} else {
		clrbit(&hei->def_phy_cap, HE_PHY_LDPC_PYLD_IDX);
	}
}

/** Compute and fill the link mem entry (Rev 128) content, called by wlc_tx_fill_link_entry. */
void
wlc_he_fill_link_entry(wlc_he_info_t *hei, wlc_bsscfg_t *cfg, scb_t *scb,
	d11linkmem_entry_t* link_entry)
{
	scb_he_t *sh;
	bss_he_info_t *bhei;
	uint32 ppet_nss;

	ASSERT(link_entry != NULL);
	ASSERT(scb != NULL);
	ASSERT(cfg != NULL);
	ASSERT(SCB_HE_CAP(scb));

	bhei = BSS_HE_INFO(hei, cfg);
	if (!bhei) {
		return;
	}
	sh = SCB_HE(hei, scb);
	/* sh might already freed in wlc_he_scb_deinit */
	if (!sh) {
		return;
	}

	link_entry->RtsDurThresh = htol16(bhei->rts_thresh);

	if (sh->ppet_valid) {
		/* If there is a ppet override configured then we apply it here */
		if (hei->ppet_override == WL_HE_PPET_AUTO) {
			ppet_nss = sh->min_ppe_nss;
		} else {
			if (hei->ppet_override == WL_HE_PPET_0US) {
				ppet_nss = HE_EXPAND_PPET_ALL_RU((HE_CONST_IDX_NONE <<
					HE_PPE_THRESH_NSS_RU_FSZ) | HE_CONST_IDX_NONE);
			} else if (hei->ppet_override == WL_HE_PPET_8US) {
				ppet_nss = HE_EXPAND_PPET_ALL_RU((HE_CONST_IDX_BPSK <<
					HE_PPE_THRESH_NSS_RU_FSZ) | HE_CONST_IDX_NONE);
			} else { /* 16 us */
				ppet_nss = HE_EXPAND_PPET_ALL_RU((HE_CONST_IDX_NONE <<
					HE_PPE_THRESH_NSS_RU_FSZ) | HE_CONST_IDX_BPSK);
			}
		}
		/* PPE thresholds valid, fill all */
		link_entry->PPET_AmpMinDur &= ~D11_REV128_PPETX_MASK;
		link_entry->PPET_AmpMinDur |= htol32(ppet_nss);
	}
	 /* no Multi-TID yet */
	link_entry->MultiTIDAggBitmap = 0;
	link_entry->MultiTIDAggNum = sh->multi_tid_agg_num;

	link_entry->BssColor_valid |= bhei->bsscolor & D11_REV128_COLOR_MASK;

	link_entry->OMI = sh->omi_lm;

	link_entry->OMI |= ((sh->flags & SCB_HE_DL_QAM1024_LT242 ? 1 : 0) <<
		C_LTX_OMI_DLQ1024_NBIT);
	link_entry->OMI |= ((sh->flags & SCB_HE_UL_QAM1024_LT242 ? 1 : 0) <<
		C_LTX_OMI_ULQ1024_NBIT);

	link_entry->OMI |= ((((sh->flags & SCB_HE_BSR_CAPABLE) && (hei->bsr_supported)) ? 1 : 0) <<
		C_LTX_OMI_BSR_NBIT);
	/* Set ldpc cap bit */
	link_entry->OMI |= (((SCB_HE_CAP(scb) && SCB_HE_LDPC_CAP(hei, scb)) ||
		(SCB_VHT_CAP(scb) && SCB_VHT_LDPC_CAP(hei->wlc->vhti, scb))) ? 1 : 0) <<
		C_LTX_OMI_LDPC_NBIT;
	/* Initiate the OMISTAT field using the lower 12 bits of OMI filed */
	link_entry->OMIStat = (link_entry->OMI & D11_LTX_OMISTAT_MASK);
}

#ifdef HERTDBG
static void wlc_he_print_rateset(wlc_he_rateset_t *he_rateset)
{
	/* Print the MCS cap map for all bandwidths */
	WL_PRINT(("HE-Rate rateset:\n"));
	WL_PRINT(("\t80Mhz     Tx:%04x Rx:%04x\n", he_rateset->bw80_tx_mcs_nss,
		he_rateset->bw80_tx_mcs_nss));
	WL_PRINT(("\t80p80Mhz  Tx:%04x Rx:%04x\n", he_rateset->bw80p80_tx_mcs_nss,
		he_rateset->bw80p80_tx_mcs_nss));
	WL_PRINT(("\t160Mhz    Tx:%04x Rx:%04x\n", he_rateset->bw160_tx_mcs_nss,
		he_rateset->bw160_tx_mcs_nss));
}
#endif /* HERTDBG */

uint16
wlc_he_get_scb_flags(wlc_he_info_t *hei, struct scb *scb)
{
	scb_he_t *he_scb;

	he_scb = SCB_HE(hei, scb);
	if (he_scb) {
		return he_scb->flags;
	} else {
		return 0;
	}
}

/* Function to check if a given scb can do ul ofdma data */
bool
wlc_he_get_ulmu_allow(wlc_he_info_t *hei, struct scb *scb)
{
	scb_he_t *he_scb;
	bool ret = FALSE;

	he_scb = SCB_HE(hei, scb);
	if (he_scb && SCB_HE_CAP(scb) &&
		(!HTC_OM_CONTROL_UL_MU_DISABLE(he_scb->omi_htc) &&
		!HTC_OM_CONTROL_UL_MU_DATA_DISABLE(he_scb->omi_htc))) {
		ret = TRUE;
	}
	return ret;
}

uint8
wlc_he_get_omi_tx_nsts(wlc_he_info_t *hei, scb_t *scb)
{
	scb_he_t *sh;
	sh = SCB_HE(hei, scb);
	return HTC_OM_CONTROL_TX_NSTS(sh->omi_htc);
}

/**
 * Process omi_data as received with HTC. The data will only contain the actual omi_data (12 bits)
 * and be loaded in omi_data, already CPU translated, use mask/shift operations to extract
 * individual fields. Store the received function and see if it matches against last received PMQ
 * version. If so then we can start applying it. Otherwise store it and wait for tx drain complete
 * trigger.
 */
static void
wlc_he_htc_process_omi(wlc_info_t* wlc, scb_t *scb, uint32 omi_data)
{
	scb_he_t *he_scb;
	bool rate_change, nss_change, ulmu_dis_change, bw_change;
	uint8 ps_omi, link_bw, new_omi_bw;

	he_scb = SCB_HE(wlc->hei, scb);

	if (he_scb->omi_htc == omi_data) {
		/* Nothing to update */
		return;
	}

	link_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
	rate_change = ((omi_data & HE_TXRATE_UPDATE_MASK) !=
		(he_scb->omi_htc & HE_TXRATE_UPDATE_MASK));

	nss_change = ((omi_data & HTC_OM_CONTROL_TX_NSTS_MASK) !=
		(he_scb->omi_htc & HTC_OM_CONTROL_TX_NSTS_MASK));

	ulmu_dis_change = (((omi_data & HTC_OM_CONTROL_UL_MU_DISABLE_MASK) !=
		(he_scb->omi_htc & HTC_OM_CONTROL_UL_MU_DISABLE_MASK)) ||
		((omi_data & HTC_OM_CONTROL_UL_MU_DATA_DISABLE_MASK) !=
		(he_scb->omi_htc & HTC_OM_CONTROL_UL_MU_DATA_DISABLE_MASK)));

	WL_PS(("wl%d.%d %s: omi_pmq: 0x%x omi_htc: 0x%x -> 0x%x\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__,
		he_scb->omi_pmq, he_scb->omi_htc, omi_data));

	bw_change = ((omi_data & HTC_OM_CONTROL_CHANNEL_WIDTH_MASK) !=
		(he_scb->omi_htc & HTC_OM_CONTROL_CHANNEL_WIDTH_MASK));

	/* Store original received htc omi before updating channel width,
	 * because omi_htc has to be equal to omi_pmq to get SCB out of PS.
	 */
	he_scb->omi_htc = (uint16)omi_data;

	if (bw_change) {
		uint8 omi_bw, chanspec_bw;
		wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
		/* make sure the new omi_bw does not exceed chanspec's bw */
		omi_bw = HTC_OM_CONTROL_CHANNEL_WIDTH(omi_data);
		if (CHSPEC_IS160(cfg->current_bss->chanspec)) {
			chanspec_bw = DOT11_OPER_MODE_160MHZ;
		} else if (CHSPEC_IS8080(cfg->current_bss->chanspec)) {
			chanspec_bw = DOT11_OPER_MODE_8080MHZ;
		} else if (CHSPEC_IS80(cfg->current_bss->chanspec)) {
			chanspec_bw = DOT11_OPER_MODE_80MHZ;
		} else if (CHSPEC_IS40(cfg->current_bss->chanspec)) {
			chanspec_bw = DOT11_OPER_MODE_40MHZ;
		} else {
			chanspec_bw = DOT11_OPER_MODE_20MHZ;
		}
		new_omi_bw = MIN(omi_bw, chanspec_bw);

		if (omi_bw != new_omi_bw) {
			/* Override the omi_data with proper bw info */
			omi_data &= ~HTC_OM_CONTROL_CHANNEL_WIDTH_MASK;
			omi_data |= ((new_omi_bw << HTC_OM_CONTROL_CHANNEL_WIDTH_OFFSET)
				& HTC_OM_CONTROL_CHANNEL_WIDTH_MASK);

			WL_ERROR(("wl%d.%d %s: STA "MACF" recv omi_bw %d new_omi_bw %d "
				"chanspec_bw %d chanspec 0x%x raw omi 0x%x\n",
				wlc->pub->unit,	WLC_BSSCFG_IDX(cfg), __FUNCTION__,
				ETHER_TO_MACF(scb->ea), omi_bw, new_omi_bw,
				chanspec_bw, cfg->current_bss->chanspec, omi_data));
		}

#ifdef TESTBED_AP_11AX
		/* Update packet engine configuration. PKT engine calls are safe since if not
		 * programmed, then command will be ignored.
		 */
		update_pkt_eng_ulbw(wlc, (scb->aid & DOT11_AID_MASK),
			HTC_OM_CONTROL_CHANNEL_WIDTH(omi_data));
#endif /* TESTBED_AP_11AX */
		wlc_mutx_hemmu_omibw_upd(wlc->mutx, scb, link_bw, new_omi_bw);
	}

	he_scb->omi_lm = (uint16)omi_data;

	if (rate_change) {
		uint8 bw, rx_nss;
		bw = HTC_OM_CONTROL_CHANNEL_WIDTH(omi_data);
		rx_nss = HTC_OM_CONTROL_RX_NSS(omi_data);
		wlc_he_apply_omi(wlc, scb, bw, rx_nss, TRUE);
	}

	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}

	if (!BSSCFG_AP(SCB_BSSCFG(scb))) {
		/* rest of functionality is only applicable for AP mode, for
		 * STA simply exit here
		 */
		return;
	}
	if (nss_change) {
		if (HE_ULMU_ENAB(wlc->pub) && SCB_HE_CAP(scb) &&
			SCB_ULOFDMA(scb)) {
			uint8 tx_nss;
			tx_nss = HTC_OM_CONTROL_TX_NSTS(omi_data);
			wlc_musched_upd_ul_nss(wlc->musched, scb, tx_nss);
		}
#ifdef TESTBED_AP_11AX
		update_pkt_eng_ulnss(wlc, (scb->aid & DOT11_AID_MASK),
			HTC_OM_CONTROL_TX_NSTS(omi_data));
#endif /* TESTBED_AP_11AX */
	}

	if (ulmu_dis_change) {
		if (wlc_he_get_ulmu_allow(wlc->hei, scb) == FALSE) {
			WL_MUTX(("wl%d: %s: change ul ofdma STA "MACF" admission to FALSE\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
			wlc_musched_admit_ulclients(wlc, scb, FALSE);
		} /* Else let the msched watchdog trigger the UL MU admission */
#ifdef TESTBED_AP_11AX
		update_pkt_eng_ulstate(wlc, !HTC_OM_CONTROL_UL_MU_DISABLE(omi_data));
#endif /* TESTBED_AP_11AX */
		wlc_mutx_he_eval_and_admit_clients(wlc->mutx, scb, TRUE);
	}

	/* PS ON if omi_htc != omi_pmq.
	 *	- Nothing will be done if already in PS
	 * PS OFF if omi_htc == omi_pmq.
	 *	- Nothing will be done if already out of PS.
	 *	- If still draining txfifo, PS off will be pending
	 */
	if ((he_scb->omi_htc & HE_TXRATE_UPDATE_MASK) == he_scb->omi_pmq) {
		ps_omi = PS_SWITCH_OFF;
		wlc_apps_ps_requester(wlc, scb, 0, PS_SWITCH_OMI);
	} else {
		ps_omi = PS_SWITCH_OMI;
		wlc_apps_ps_requester(wlc, scb, PS_SWITCH_OMI, 0);
	}
	WL_PS(("wl%d.%d %s: ps_omi 0x%x\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__, ps_omi));
	wlc_apps_process_ps_switch(wlc, scb, ps_omi);
}

/**
 * This function is to be called by VHT module when AF frame containing oper_mode info.
 */
void
wlc_he_recv_af_oper_mode(wlc_info_t* wlc, scb_t *scb, uint8 oper_mode)
{
	scb_he_t *he_scb;
	uint8 omi_bw;
	uint8 omi_rx_nss;
	uint8 bw;
	uint8 rx_nss;

	/* Ignore info if the Rx NSS Type is 1 (no usefull information for us) */
	if (DOT11_OPER_MODE_RXNSS_TYPE(oper_mode)) {
		return;
	}

	omi_bw = DOT11_OPER_MODE_160_8080(oper_mode) ?
		DOT11_OPER_MODE_160MHZ : DOT11_OPER_MODE_CHANNEL_WIDTH(oper_mode);
	omi_rx_nss = DOT11_OPER_MODE_RXNSS(oper_mode) - 1;

	/* track that we recevied oper_mode so we next time we receive HT OMI we for sure apply */
	he_scb = SCB_HE(wlc->hei, scb);

	bw = HTC_OM_CONTROL_CHANNEL_WIDTH(he_scb->omi_lm);
	rx_nss = HTC_OM_CONTROL_RX_NSS(he_scb->omi_lm);

	if ((omi_bw != bw) || (omi_rx_nss != rx_nss)) {
		wlc_he_apply_omi(wlc, scb, omi_bw, omi_rx_nss, FALSE);
		/* Tx nss is not defined in legacy operaring mode AF, dont update, but if the ucode
		 * really needs the field then we should program max nss based upon capabilities
		 * or use the rx value (need to discuss)
		 */
		he_scb->omi_lm &= ~HE_TXRATE_UPDATE_MASK;
		he_scb->omi_lm |= omi_bw << HTC_OM_CONTROL_CHANNEL_WIDTH_OFFSET;
		he_scb->omi_lm |= omi_rx_nss << HTC_OM_CONTROL_RX_NSS_OFFSET;
		if (RATELINKMEM_ENAB(wlc->pub)) {
			wlc_ratelinkmem_update_link_entry(wlc, scb);
		}
	}
}

/**
 * Parse a received HTC code. The HTC code to be parsed should include the Variant bits, but the
 * variant has to be HE (will not be verified by this function), and it should be stored as it was
 * was received over the air, such that getbit(s) can be used to extract the information.
 */
static void
wlc_he_parse_htcode(wlc_info_t* wlc, scb_t *scb, uint8 *htc_code)
{
	uint32 read_idx;
	uint8 control_id;
	uint32 info_sz;
	uint32 control_data;

	/* Skip variant which is two bits */
	read_idx = HTC_VARIANT_SZ;

	/* Loop over all the encoded control IDs */
	while (read_idx < (HTC_CODE_SIZE - HTC_CONTROL_ID_SZ)) {
		control_id = getbits(htc_code, HTC_CODE_SIZE, read_idx, HTC_CONTROL_ID_SZ);
		read_idx += HTC_CONTROL_ID_SZ;
		switch (control_id) {
		case HTC_CONTROL_ID_TRS :
			/* Note: if this is the first HTC code then it is valid TRS, but if it
			 * is second one then it cant fit (TRS size is 26), but in that case it
			 * is supposed to be padding. Padding is made up by all 0. Length
			 * validation will pick up padding, so we set length TRS_SZ, and keep
			 * checks as simple as possible, since it will work correctly.
			 */
			info_sz = HTC_CONTROL_TRS_SZ;
			break;
		case HTC_CONTROL_ID_OM :
			info_sz = HTC_CONTROL_OM_SZ;
			break;
		case HTC_CONTROL_ID_HLA :
			info_sz = HTC_CONTROL_HLA_SZ;
			break;
		case HTC_CONTROL_ID_BSR :
			info_sz = HTC_CONTROL_BSR_SZ;
			break;
		case HTC_CONTROL_ID_UPH :
			info_sz = HTC_CONTROL_UPH_SZ;
			break;
		case HTC_CONTROL_ID_BQR :
			info_sz = HTC_CONTROL_BQR_SZ;
			break;
		case HTC_CONTROL_ID_CAS :
			info_sz = HTC_CONTROL_CAS_SZ;
			break;
		default:
			/* Unhandled code. Problem: we cannot continue parsing as the the info
			 * size is unknown. Set sz to 0, so parsing gets skipped
			 */
			info_sz = 0;
			break;
		}
		/* Verify the info_sz to be valid (not too large) */
		if ((info_sz > (HTC_CODE_SIZE - read_idx)) || (info_sz == 0)) {
			break;
		}
		control_data = getbits(htc_code, HTC_CODE_SIZE, read_idx, info_sz);
		read_idx += info_sz;
		/* Now parse the different control IDs for which we have support */
		switch (control_id) {
		case HTC_CONTROL_ID_OM :
			wlc_he_htc_process_omi(wlc, scb, control_data);
			break;
		default:
			WL_TRACE(("Unhandled HTC %d, data 0x%03x\n", control_id, control_data));
			break;
		}

	}
}

/**
 * any received AMSDU/AMPDU/MPDU should call this function so HTC information can
 * extracted from header and data can be used.
 */
void
wlc_he_htc_recv(wlc_info_t* wlc, scb_t *scb, d11rxhdr_t *rxh, struct dot11_header *hdr)
{
	uint16 fc;
	uint8 *htc;
	uint32 offset;
	scb_he_t *he_scb;

	if (!HE_ENAB(wlc->pub))
		return;

	he_scb = SCB_HE(wlc->hei, scb);
	if (!he_scb) {
		return;
	}

	/* Determine if header includes HTC field. See 9.2.4.1.10. It needs to be HT or higher
	 * type frame, then it it has to be QOS frame, if both conditions true then fc.order bit
	 * specifies whether or not the HTC field is included/appended.
	 */
	fc = ltoh16(hdr->fc);
	if ((D11PPDU_FT(rxh, wlc->pub->corerev) < FT_HT) ||
	    !(FC_SUBTYPE_ANY_QOS(FC_SUBTYPE(fc))) || !(fc & FC_ORDER)) {
		return;
	}
	/* Check if we are dealing with HE HTC code */
	offset = HTC_CONTROL_OFFSET;
	if ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS)) {
		offset += HTC_SIZEOFA4;
	}
	htc = (uint8 *)hdr;
	htc += offset;
	if ((htc[0] & HTC_IDENTITY_MASK) == HTC_IDENTITY_HE) {
		/* Yes, it is a HE HTC code. process it but avoid processing 'empty codes' */
		if (htc[0] != 0xff) {
			wlc_he_parse_htcode(wlc, scb, htc);
		}
	}
}

/**
 * if there is a HTC+ code to transmit return true; Note: *htc_code will never be 0 in that case
 */
bool
wlc_he_htc_tx(wlc_info_t* wlc, scb_t *scb, void *pkt, uint32 *htc_code)
{
	scb_he_htc_t *htc;
	scb_he_t *he_scb;
	uint8 *data;

	if (!HE_ENAB(wlc->pub))
		return FALSE;

	he_scb = SCB_HE(wlc->hei, scb);
	if (!he_scb || !(he_scb->flags & SCB_HE_HTC_CAP)) {
		return FALSE;
	}
	htc = &he_scb->htc;

	if (htc->rd_idx == htc->wr_idx) {
		return FALSE;
	}

	if (htc->outstanding) {
		return FALSE;
	}

	data = (uint8 *)htc_code;
	data[0] = (htc->codes[htc->rd_idx] | HTC_IDENTITY_HE) & 0xff;
	data[1] = (htc->codes[htc->rd_idx] >> 8) & 0xff;
	data[2] = (htc->codes[htc->rd_idx] >> 16) & 0xff;
	data[3] = (htc->codes[htc->rd_idx] >> 24) & 0xff;

	htc->outstanding++;
	WLF2_PCB1_REG(pkt, WLF2_PCB1_HTC);

	return TRUE;
}

/**
 * Called upon packet free, when packet contains HTC code. If success then allow to go to next
 * code (if any in array of codes to transmit). If no success, then set code to be retried on
 * next packet.
 */
static void BCMFASTPATH
wlc_he_htc_pkt_freed(wlc_info_t *wlc, void *pkt, uint txs)
{
	struct scb *scb;
	scb_he_t *he_scb;
	scb_he_htc_t *htc;

	ASSERT(HE_ENAB(wlc->pub));

	/* no packet */
	if (!pkt)
		return;

	if ((scb = WLPKTTAGSCBGET(pkt)) == NULL)
		return;

	he_scb = SCB_HE(wlc->hei, scb);

	if (!he_scb) {
		return;
	}
	htc = &he_scb->htc;

	if ((txs & TX_STATUS_MASK) == TX_STATUS_ACK_RCV) {
		ASSERT(htc->rd_idx != htc->wr_idx);
		ASSERT(htc->outstanding == 1);
		if (htc->cb_fn[htc->rd_idx]) {
			htc->cb_fn[htc->rd_idx](wlc, scb, htc->codes[htc->rd_idx]);
		}
		htc->rd_idx = HE_NEXT_HTC_IDX(htc->rd_idx);
	}
	htc->outstanding--;
}

/**
 * Add a htc code to queue of htc codes to be transmitted.
 */
void
wlc_he_htc_send_code(wlc_info_t *wlc, scb_t *scb, uint32 htc_code, he_htc_cb_fn_t cb_fn)
{
	scb_he_t *he_scb;
	scb_he_htc_t *htc;

	ASSERT(HE_ENAB(wlc->pub));

	he_scb = SCB_HE(wlc->hei, scb);

	if (!he_scb || !(he_scb->flags & SCB_HE_HTC_CAP)) {
		return;
	}

	htc = &he_scb->htc;

	/* Check if there is rooom to send next HTC code */
	if (HE_NEXT_HTC_IDX(htc->wr_idx) != htc->rd_idx) {
		htc->codes[htc->wr_idx] = htc_code;
		htc->cb_fn[htc->wr_idx] = cb_fn;
		htc->wr_idx = HE_NEXT_HTC_IDX(htc->wr_idx);
	} else {
		WL_ERROR(("%s No more space to store HTC code\n", __FUNCTION__));
	}
}

static int
wlc_he_cmd_htc(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen, bool set,
	wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	scb_iter_t scbiter;
	scb_t *scb;

	if (set) {
		uint32 htc_code = *(uint32 *)params;

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			wlc_he_htc_send_code(wlc, scb, htc_code, NULL);
		}
	} else {
		return BCME_UNSUPPORTED;
	}

	return BCME_OK;
}

static void
wlc_he_tx_omi_callback(wlc_info_t *wlc, struct scb *scb, uint32 htc_omi)
{
	scb_he_t *he_scb;
	uint16 omi;
	uint8 tx_nsts;
	uint8 bw;
	uint8 rx_nss;

	he_scb = SCB_HE(wlc->hei, scb);

	omi = (htc_omi >> (HTC_CONTROL_ID_SZ + HTC_VARIANT_SZ));
	tx_nsts = HTC_OM_CONTROL_TX_NSTS(omi);
	rx_nss = HTC_OM_CONTROL_RX_NSS(omi);
	bw = HTC_OM_CONTROL_CHANNEL_WIDTH(omi);

	/* Update linkmem with "inverted" data */
	he_scb->omi_lm = HE_OMI_ENCODE(tx_nsts, rx_nss, bw, HTC_OM_CONTROL_ER_SU_DISABLE(omi),
		HTC_OM_CONTROL_DL_MUMIMO_RESOUND(omi), HTC_OM_CONTROL_UL_MU_DISABLE(omi),
		HTC_OM_CONTROL_UL_MU_DATA_DISABLE(omi));
	/* Update Tx rate table */
	wlc_he_apply_omi(wlc, scb, bw, tx_nsts, TRUE);
	if (RATELINKMEM_ENAB(wlc->pub)) {
		wlc_ratelinkmem_update_link_entry(wlc, scb);
	}
}

static int
wlc_he_cmd_omi(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen, bool set,
	wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;
	wlc_info_t *wlc = hei->wlc;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	wl_he_omi_t *omi = (wl_he_omi_t *)params;
	struct ether_addr *addr;
	scb_t *scb;
	scb_he_t *he_scb;
	uint32 htc;
	bool use_htc_callback;
	uint8 lm_tx_nsts;
	uint8 lm_bw;
	int ul_mu_data_disable;

	/* ignore the user supplied peer address for infra STA and use BSSID instead */
	addr = (BSSCFG_STA(cfg) && cfg->BSS) ? &cfg->BSSID : &omi->peer;
	/* lookup the scb */
	scb = wlc_scbfind(wlc, cfg, addr);
	if (!scb) {
		return BCME_BADARG;
	}

	he_scb = SCB_HE(wlc->hei, scb);

	if (!he_scb) {
		return BCME_UNSUPPORTED;
	}

	if (set) {
#ifdef TESTBED_AP_11AX
		/* for testbed AP there is an override needed for only txnsts. This should not
		 * be communicated to remote, only result in generation of new txrate tables
		 * with new txnsts (and possibly bw, but test does not require that).
		 */
		if (omi->tx_override) {
			he_scb->omi_lm = HE_OMI_ENCODE(omi->tx_nsts, omi->rx_nss,
				omi->channel_width, omi->er_su_disable,
				omi->dl_mumimo_resound, omi->ul_mu_disable,
				omi->ul_mu_data_disable);
			wlc_he_apply_omi(wlc, scb, omi->channel_width, omi->tx_nsts, TRUE);
			if (RATELINKMEM_ENAB(wlc->pub)) {
				wlc_ratelinkmem_update_link_entry(wlc, scb);
			}
			return BCME_OK;
		}
#endif /* TESTBED_AP_11AX */
		if (!(he_scb->flags & SCB_HE_OMI)) {
			return BCME_UNSUPPORTED;
		}
		if (he_scb->flags & SCB_HE_OMI_UL_MU_DATA_DIS) {
			ul_mu_data_disable = omi->ul_mu_data_disable;
		} else {
			ul_mu_data_disable = 0;
		}
		/* The paramaters for tx and rx are about to be updated. If tx params get
		 * updated then all tx frames be suppressed, htc transmission (using null frame),
		 * then tx rateset update, linkmem update and finally tx unblock. Found improved
		 * method. Limit the rateset to min of current and new (if needed). send htc, once
		 * sent (callback) update rateset to new set. This will make sure packet will
		 * always be sent with "valid" tx set and gives best tput and easiest handling.
		 */
		use_htc_callback = TRUE;
		lm_tx_nsts = HTC_OM_CONTROL_RX_NSS(he_scb->omi_lm);
		lm_bw = HTC_OM_CONTROL_CHANNEL_WIDTH(he_scb->omi_lm);
		if ((lm_tx_nsts > omi->tx_nsts) || (lm_bw > omi->channel_width)) {
			/* Update Tx rate table to temporarily minium value. If this value
			 * is the same as what is configured then dont set callback for htc
			 * but do all the updating now and be done with it.
			 */
			if ((lm_tx_nsts >= omi->tx_nsts) && (lm_bw >= omi->channel_width)) {
				/* Update linkmem */
				he_scb->omi_lm = HE_OMI_ENCODE(omi->tx_nsts, omi->rx_nss,
					omi->channel_width, omi->er_su_disable,
					omi->dl_mumimo_resound, omi->ul_mu_disable,
					ul_mu_data_disable);
				wlc_he_apply_omi(wlc, scb, omi->channel_width, omi->tx_nsts, TRUE);
				if (RATELINKMEM_ENAB(wlc->pub)) {
					wlc_ratelinkmem_update_link_entry(wlc, scb);
				}
				use_htc_callback = FALSE;
			} else {
				wlc_he_apply_omi(wlc, scb,
					lm_bw > omi->channel_width ? omi->channel_width : lm_bw,
					lm_tx_nsts > omi->tx_nsts ? omi->tx_nsts : lm_tx_nsts,
					TRUE);
			}
		}
		/* Create HTC code, note: it will go out new rateset, which is wrong. We should
		 * use a null frame at bw20 and 1x1 to be safe. TBD!
		 */
		htc = HE_OMI_ENCODE(omi->rx_nss, omi->tx_nsts, omi->channel_width,
			omi->er_su_disable, omi->dl_mumimo_resound, omi->ul_mu_disable,
			ul_mu_data_disable);
		htc = (htc << HTC_CONTROL_ID_SZ) | HTC_CONTROL_ID_OM;
		htc = (htc << HTC_VARIANT_SZ) | HTC_IDENTITY_HE;
		wlc_he_htc_send_code(wlc, scb, htc,
			use_htc_callback ? wlc_he_tx_omi_callback : NULL);
		/* Send NULL data frame to make sure HTC gets send... */
		wlc_sendnulldata(wlc, cfg, &scb->ea, 0, 0, PRIO_8021D_BE, NULL, NULL);
	} else {
		omi = (wl_he_omi_t *)result;
		memcpy(&omi->peer, addr, sizeof(omi->peer));
		omi->rx_nss = HTC_OM_CONTROL_TX_NSTS(he_scb->omi_lm);
		omi->channel_width = HTC_OM_CONTROL_CHANNEL_WIDTH(he_scb->omi_lm);
		omi->tx_nsts = HTC_OM_CONTROL_RX_NSS(he_scb->omi_lm);
		omi->er_su_disable = HTC_OM_CONTROL_ER_SU_DISABLE(he_scb->omi_lm);
		omi->dl_mumimo_resound = HTC_OM_CONTROL_DL_MUMIMO_RESOUND(he_scb->omi_lm);
		omi->ul_mu_disable = HTC_OM_CONTROL_UL_MU_DISABLE(he_scb->omi_lm);
		omi->ul_mu_data_disable = HTC_OM_CONTROL_UL_MU_DATA_DISABLE(he_scb->omi_lm);
		omi->tx_override = 0;
	}

	return BCME_OK;
}

/**
 * wlc_he_omi_pmq_code is to be called when a PMQ interrupt with OMI information is received.
 */
bool
wlc_he_omi_pmq_code(wlc_info_t *wlc, scb_t *scb, uint8 rx_nss, uint8 bw)
{
	scb_he_t *he_scb;

	if (!HE_ENAB(wlc->pub)) {
		return FALSE;
	}

	he_scb = SCB_HE(wlc->hei, scb);
	if (!he_scb) {
		return FALSE;
	}

	he_scb->omi_pmq = HE_OMI_ENCODE(rx_nss, 0, bw, 0, 0, 0, 0);

	WL_PS(("wl%d.%d %s: omi_htc 0x%x omi_pmq 0x%x\n", wlc->pub->unit,
		WLC_BSSCFG_IDX(SCB_BSSCFG(scb)), __FUNCTION__,
		he_scb->omi_htc, he_scb->omi_pmq));

	/* return whether to suppress */
	return ((he_scb->omi_htc & HE_TXRATE_UPDATE_MASK) != he_scb->omi_pmq);
}

#ifdef TESTBED_AP_11AX
static int
wlc_he_cmd_bsr(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen, bool set,
	wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;

	if (set) {
		hei->bsr_supported = *params;
	} else {
		*result = hei->bsr_supported;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}

static int
wlc_he_cmd_testbed(void *ctx, uint8 *params, uint16 plen, uint8 *result, uint16 *rlen, bool set,
	wlc_if_t *wlcif)
{
	wlc_he_info_t *hei = ctx;

	if (set) {
		hei->testbed_mode = (*params) ? TRUE : FALSE;
		hei->muedca_blocked = (*params) ? TRUE : FALSE;
	} else {
		*result = hei->testbed_mode;
		*rlen = sizeof(*result);
	}

	return BCME_OK;
}
#endif /* TESTBED_AP_11AX */

/*
 * Update the HE rateset of SCB based on the max NSS.
 */
void
wlc_he_upd_scb_rateset_mcs(wlc_he_info_t *hei, struct scb *scb, uint8 link_bw)
{
	wlc_info_t *wlc =  hei->wlc;
	wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
	uint txstreams, rxstreams, peer_nss, omi_rx_nss;
	scb_he_t *he_scb;

	/* For Tx chain limit, use min of op_txsreams and Peer's NSS from OMN;
	 * If Peer is in OMN, use the RXNSS of OMN as the Tx chains.
	 */
	if (SCB_VHT_CAP(scb)) {
		peer_nss = wlc_vht_get_scb_oper_nss(wlc->vhti, scb);
	} else if (SCB_HE_CAP(scb)) {
		he_scb = SCB_HE(hei, scb);
		ASSERT(he_scb != NULL);

		peer_nss = HE_MAX_SS_SUPPORTED(he_scb->bw80_rx_mcs_nss);
		if (link_bw == BW_160MHZ) {
			peer_nss = HE_MAX_SS_SUPPORTED(he_scb->bw160_rx_mcs_nss);
		}
		omi_rx_nss = HTC_OM_CONTROL_RX_NSS(he_scb->omi_lm) + 1;
		peer_nss = MIN(peer_nss, omi_rx_nss);
	} else {
		return;
	}

	txstreams = MIN(wlc->stf->op_txstreams, peer_nss);
	rxstreams = MIN(wlc->stf->op_rxstreams, wlc_stf_rxstreams_get(wlc));

	/* Intersect and update rateset */
	wlc_he_intersect_txrxmcsmaps(hei, scb, !wlc_he_bw80_limited(wlc, cfg),
		FALSE, txstreams, rxstreams);
}

bool
wlc_he_is_nonbrcm_160sta(wlc_he_info_t *hei, scb_t *scb)
{
	uint32 channel_width_set;
	scb_he_t *sh;

	sh = SCB_HE(hei, scb);
	ASSERT(sh != NULL);

	channel_width_set = getbits((uint8*)&sh->phy_cap, sizeof(sh->phy_cap),
		HE_PHY_CH_WIDTH_SET_IDX, HE_PHY_CH_WIDTH_SET_FSZ);

	if (!SCB_IS_BRCM(scb) &&
		(channel_width_set & (HE_PHY_CH_WIDTH_2G_40 | HE_PHY_CH_WIDTH_5G_80 |
			HE_PHY_CH_WIDTH_5G_160))) {
		WL_INFORM(("%s : "MACF" likely non brcm bw160 sta\n",
			__FUNCTION__, ETHER_TO_MACF(scb->ea)));
		return TRUE;
	}

	return FALSE;
}

#endif /* WL11AX */
