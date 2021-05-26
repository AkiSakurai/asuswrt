/*
 * WLC LTE Coex module API definition
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_ltecx.h 781093 2019-11-11 15:21:20Z $
 */

#ifndef _wlc_ltecx_h_
#define _wlc_ltecx_h_

/* LTECX - WCI2 Default baud rate */
#define LTECX_WCI2_INVALID_BAUD		0
#define LTECX_WCI2_DEFAULT_BAUD		3

/* ltecxflg interface mask */
#define	LTECX_LOOKAHEAD_MASK	0x00FFF
#define	LTECX_BAUDRATE_MASK		0x0F000
#define	LTECX_TX_IND_MASK		0x10000

/* LTE B40 parameters */
#define LTECX_NVRAM_PARAM_MAX			3
#define LTECX_NVRAM_WLANRX_PROT			0
#define LTECX_NVRAM_LTERX_PROT			1
#define LTECX_NVRAM_SCANJOIN_PROT		2
#define LTECX_NVRAM_RSSI_THRESH_20MHZ	5
#define LTECX_NVRAM_RSSI_THRESH_10MHZ	6
#define LTECX_NVRAM_MAX_CHANNELS		13
#define LTECX_NVRAM_GET_PROT_MASK		4
#define LTECX_NVRAM_20M_RSSI_2390		0
#define LTECX_NVRAM_20M_RSSI_2385		1
#define LTECX_NVRAM_20M_RSSI_2380		2
#define LTECX_NVRAM_20M_RSSI_2375		3
#define LTECX_NVRAM_20M_RSSI_2370		4
#define LTECX_NVRAM_10M_RSSI_2395		0
#define LTECX_NVRAM_10M_RSSI_2390		1
#define LTECX_NVRAM_10M_RSSI_2385		2
#define LTECX_NVRAM_10M_RSSI_2380		3
#define LTECX_NVRAM_10M_RSSI_2375		4
#define LTECX_NVRAM_10M_RSSI_2370		5

#define LTE_CHANNEL_BW_20MHZ	20000
#define LTE_CHANNEL_BW_10MHZ	10000
#define LTE_BAND40_MAX_FREQ		2400
#define LTE_BAND40_MIN_FREQ		2300
#define LTE_20MHZ_INIT_STEP		10
#define LTE_10MHZ_INIT_STEP		5
#define LTE_RSSI_THRESH_LMT		2
#define LTE_FREQ_STEP_SIZE		5
#define LTE_FREQ_STEP_MAX		8
#define LTE_MAX_FREQ_DEVIATION	2
#define LTECX_LOOKAHEAD_SHIFT	0
#define LTECX_BAUDRATE_SHIFT	12
#define LTECX_TX_IND_SHIFT		16

#define LTECX_MIN_CH_MASK		0xF

#define LTECX_FRAME_DOWNLINK_TYPE		1
#define LTECX_FRAME_GAURDPERIOD_TYPE	2
#define LTECX_FRAME_UPLINK_TYPE			3
#define LTECX_FRAME_SYNC_MASK			0x1
#define LTECX_ECI_SLICE_12			0xC
#define BTCX_ECI_EVENT_POLARITY			(1 << 8)
#define SECI_ULCR_LBC_POS			4
#define LTECX_CELLSTATUS_MASK			3
#define LTECX_CELLSTATUS_UKNOWN			2
#define LTECX_CELLSTATUS_ON			1
#define LTECX_CELLANT_TX_MASK			1
#define LTECX_DO_NOISE_EST_POS			12
#define LTECX_TXNOISE_CNT			2
#define LTECX_WCI2_TST_VALID			15
#define UL_ANT_MASK_ANT0			0x400
#define UL_ANT_MASK_ANT1			0x200
#define UL_ANT_MASK_ANT2			0x100
#define UL_ANT_MASK_ANT3			0x80
#define MWS_ANTMAP_UL_CORE0			0x600
#define DL_ANT_MASK_ANT0			0x8
#define DL_ANT_MASK_ANT1			0x4
#define DL_ANT_MASK_ANT2			0x2
#define MWS_ANTMAP_DL_CORE0			0xc
#define MWS_ANTMAP_DL_CORE1			0x2
#define MWS_ANTMAP_DL_ANTMODE_MASK		0x40
#define MWS_ANTMAP_UL_ANTSEL_MASK		0x1800
#define MWS_ANTMAP_UL_ANTMODE_MASK		0x2000
#define MWS_ANTMAP_DL_ANTSEL_MASK		0x30
#define MWS_OCLMAP_DEFAULT_2G			0x1fff
#define MWS_OCLMAP_DEFAULT_5G_LO		0xffff
#define MWS_OCLMAP_DEFAULT_5G_MID		0xffff
#define MWS_OCLMAP_DEFAULT_5G_HIGH		0x1f

typedef enum shm_ltecx_hflags_e {
	C_LTECX_HOST_COEX_EN	= 0,	/* 1: Enable Lte Coex */
	C_LTECX_HOST_RX_ALWAYS,			/* 1: WLAN Rx not affected by LTE Tx */
	C_LTECX_HOST_TX_NEGEDGE,		/* 1: LTE_Tx lookahead de-asserts
									 *  at actual LTE_Tx end
									 */
	C_LTECX_HOST_PROT_TXRX,			/* 1: Enable LTE simultaneous TxRx protection */
	C_LTECX_HOST_TX_ALWAYS	= 4,	/* 1: WLAN Tx does not affect LTE Rx */
	C_LTECX_HOST_ASSOC_PROG,		/* 1: Association in progress */
	C_LTECX_HOST_ASSOC_STATE,		/* 1: Client STA associated */
	C_LTECX_HOST_PROT_TYPE_NONE_TMP,	/* bit updated by firmware */
	C_LTECX_HOST_PROT_TYPE_PM_CTS = 8,	/* bit updated by firmware */
	C_LTECX_HOST_PROT_TYPE_NONE,		/* bit updated by ucode */
	C_LTECX_HOST_PROT_TYPE_CTS,		/* 0: Use PM packets, 1: Use CTS2SELF */
	C_LTECX_HOST_PROT_TYPE_AUTO,
	C_LTECX_HOST_RX_ACK	= 12,		/* 0: Cant receive Ack during LTE_Tx */
	C_LTECX_HOST_TXIND,
	C_LTECX_HOST_SCANJOIN_PROT,
	C_LTECX_HOST_INTERFACE = 15		/* 0: WCI2, 1: ERCX Interface */
} shm_ltecx_hflags_t;

typedef enum {
	C_LTECX_ST_PROT_REQ	= 0,		/* 1: LTECX Protection Requested */
	C_LTECX_ST_IDLE,				/* 1: LTE is idle */
	C_LTECX_ST_ACTUAL_TX,			/* 1: LTE Tx On */
	C_LTECX_ST_TX_PREV,				/* Previous LTE Tx (with lookahead) */
	C_LTECX_ST_WLAN_PRIO = 4,		/* 1: WLAN in critical */
	C_LTECX_ST_PRQ_ACTIVE,			/* Probe request sent */
	C_LTECX_ST_PROT_PND,			/* 1: LTECX Protection Pending */
	C_LTECX_ST_PROT_REQ_CTS,		/* 1: LTECX Protection Requested CTS2SELF */
	C_LTECX_ST_RESEND_GCI_BITS = 8,	/* 1: Indicate the status to the MWS. */
	C_LTECX_ST_TYPE3_INFINITE_STATE,	/* 1: TYPE 3 MSG with infinite duration. */
	C_LTECX_ST_AVAILABLE1,
	C_LTECX_ST_AVAILABLE2,
	C_LTECX_ST_TX_IND = 12,
	C_LTECX_ST_LTE_ACTIVE		/* Needed for TxPwrCap selection */
} shm_ltecx_state_t;

/* LTE Flags bits */
typedef enum {
	C_LTECX_FLAGS_LPBKSRC	= 0,
	C_LTECX_FLAGS_LPBKSINK	= 1,
	C_LTECX_FLAGS_TXIND	= 2,
	C_LTECX_FLAGS_SCAN_PROG	= 3,
	C_LTECX_FLAGS_RXBREAK	= 4,
	C_LTECX_FLAGS_WCI2_4TXPWRCAP	= 5,
	C_LTECX_FLAGS_MWS_TYPE7_CELL_TX_ANT	= 6,
	C_LTECX_FLAGS_TSCOEX_EN = 7,
	C_LTECX_FLAGS_FS_INTR = 8,
	C_LTECX_FLAGS_CRTI_DEBUG_MODE = 9	/* 1: CRTI DEBUG MODE Enabled */
} shm_ltecx_flags_t;

/* LTE coex definitions */
typedef enum mws_wlanrx_prot_e {
	C_LTECX_MWS_WLANRX_PROT_NONE	= 0,
	C_LTECX_MWS_WLANRX_PROT_CTS,
	C_LTECX_MWS_WLANRX_PROT_PM,
	C_LTECX_MWS_WLANRX_PROT_AUTO
} mws_wlanrx_prot_t;

typedef enum {
	C_LTECX_DATA_TYPE_INT16,
	C_LTECX_DATA_TYPE_UINT32
} ltecx_arr_datatype_t;

#define LTECX_FLAGS_LPBKSRC_MASK (1 << C_LTECX_FLAGS_LPBKSRC)
#define LTECX_FLAGS_LPBKSINK_MASK (1 << C_LTECX_FLAGS_LPBKSINK)
#define LTECX_FLAGS_LPBK_MASK ((LTECX_FLAGS_LPBKSRC_MASK) | (LTECX_FLAGS_LPBKSINK_MASK))

/* LTE coex data structures */
typedef struct {
	uint8 loopback_type;
	uint8 packet;
	uint16 repeat_ct;
} wci2_loopback_t;

typedef struct {
	uint16 nbytes_tx;
	uint16 nbytes_rx;
	uint16 nbytes_err;
} wci2_loopback_rsp_t;

typedef struct wlc_ltecx_cmn_info {
	int16		ltecx_rssi_thresh_20mhz[LTECX_NVRAM_RSSI_THRESH_20MHZ]
				[LTECX_NVRAM_MAX_CHANNELS]; /* elna rssi threshold for 20MHz BW */
	int16		ltecx_rssi_thresh_10mhz[LTECX_NVRAM_RSSI_THRESH_10MHZ]
				[LTECX_NVRAM_MAX_CHANNELS]; /* elna rssi threshold for 10MHz BW */
	uint32		ltecx_20mhz_modes[LTECX_NVRAM_PARAM_MAX];
					/* wlanrx_prot, lterx_prot, scanjoin_prot */
	uint32		ltecx_10mhz_modes[LTECX_NVRAM_PARAM_MAX];
					/* wlanrx_prot, lterx_prot, scanjoin_prot */
	mws_params_t	mws_params;
	wci2_config_t	wci2_config;
	mws_frame_config_t mws_frame_config;
	mws_scanreq_bms_t	*mws_scanreq_bms;
} wlc_ltecx_cmn_info_t;

typedef struct {
	uint16 wlan_txmap2g;
	uint16 wlan_txmap5g;
	uint16 wlan_rxmap2g;
	uint16 wlan_rxmap5g;
} mws_wlan_ant_map_t;

struct wlc_ltecx_info {
	wlc_info_t	*wlc;
	bool		ltecx_enabled;	/* LTECX enabled/disabled in ucode */
	bool		ltecx_idle;		/* LTE signalling IDLE */
	bool		mws_lterx_prot;
	bool		mws_lterx_prot_prev;	/* To detect change in mws_lterx_prot */
	bool		mws_im3_prot;
	bool		mws_im3_prot_prev;	/* To detect change in mws_lterx_prot */
	bool		mws_ltecx_txind;
	bool		mws_ltecx_txind_prev; /* To detect change in mws_ltecx_txind */
	bool		mws_wlan_rx_ack_prev; /* To detect change in rx_ack bit */
	bool		mws_rx_aggr_off;	/* 1: Rx Aggregation disabled by LTECX */
	bool		mws_elna_bypass;		/* 1: elna bypassed 0: elna enabled */
	uint8		mws_wlanrx_prot;
	uint8		mws_wlanrx_prot_prev;	/* Previous protection mode */
	uint8		baud_rate;		/* SECI uart baud rate */
	uint8		ltecx_rssi_thresh_lmt_nvram;
	uint8		mws_ltecx_rssi_thresh_lmt; /* rssi threshold hysteresis loop limit */
	uint8		mws_wlanrx_prot_min_ch;
	uint8		mws_lterx_prot_min_ch;
	uint8		mws_scanjoin_prot_min_ch;
	uint8		mws_lte_freq_index;
	uint16		mws_ltetx_dur_prev;    /* Previous Lte Tx duration */
	uint16		ltecx_chmap;	/* per-ch ltecx bm (iovar "mws_coex_bitmap") */
	uint16		ltetx_adv;
	uint16		ltetx_adv_prev;	/* To detect change in ltetx_adv */
	uint16		adv_tout_prev;
	uint16		scanjoin_prot;
	uint16		scanjoin_prot_prev; /* To detect change in scanjoin_prot */
	uint16		lte_center_freq_prev;
	uint16		lte_channel_bw_prev;
	uint16		mws_debug_mode;
	uint16		mws_debug_mode_prev; /* Used to optimize shmem access */
	uint16		ltecx_shm_addr;
	int16		mws_wifi_sensi_prev;
	int16		mws_ltecx_wifi_sensitivity;
	int16		mws_elna_rssi_thresh; /* elna bypass RSSI threshold */
	uint32		ltecx_flags;
	uint32		ltecxmux;	/* LTECX Configuration */
	uint32		ltecxpadnum;
	uint32		ltecxfnsel;
	uint32		ltecxgcigpio;
	mws_wci2_msg_t	mws_wci2_msg;
	uint32		xtalfreq;
	uint16		ltecx_ts_chmap; /* per channel time sharing feature for 4350 */
	mws_ant_map_t	mws_antmap;
	bool		mws_rxbreak_dis;
	uint32		antmap_in_use_not_used;
	bool		tx_aggr_off;
	wlc_ltecx_cmn_info_t *cmn;
	uint16      mws_noise_meas;
	mws_wlan_ant_map_t      antmap_in_use;
	uint16		mws_wlanrxpri_thresh;
	wl_mws_ocl_override_t   mws_oclmap;
	bool            ocl_iovar_set;
};

struct mws_scanreq_bms {
	uint32 bm_2g[16];
	uint32 bm_5g_lo[16];
	uint32 bm_5g_mid[16];
	uint32 bm_5g_hi[16];
};

#ifdef BCMLTECOEX
/* LTE coex functions */
extern wlc_ltecx_info_t *wlc_ltecx_attach(wlc_info_t *wlc);
extern void wlc_ltecx_detach(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_init(wlc_ltecx_info_t *ltecx);

extern void wlc_ltecx_update_all_states(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_frame_config(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_check_chmap(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_check_tscoex_chmap(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_set_wlanrx_prot(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_ltetx_adv(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_lterx_prot(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_im3_prot(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_scanjoin_prot(wlc_ltecx_info_t *ltecx);
extern void wlc_ltetx_indication(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_set_wlanrxpri_thresh(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_set_noise_meas(wlc_ltecx_info_t *ltecx);
extern bool wlc_ltecx_get_lte_status(wlc_ltecx_info_t *ltecx);
extern bool wlc_ltecx_turnoff_rx_aggr(wlc_ltecx_info_t *ltecx);
extern bool wlc_ltecx_turnoff_tx_aggr(wlc_ltecx_info_t *ltecx);
extern bool wlc_ltecx_get_lte_map(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_wl_rssi_thresh(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_wlanrx_ack(wlc_ltecx_info_t *ltecx);
extern int wlc_ltecx_chk_elna_bypass_mode(wlc_ltecx_info_t * ltecx);
extern void wlc_ltecx_update_status(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_wifi_sensitivity(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_debug_msg(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_debug_mode(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_interface_active(wlc_hw_info_t *wlc_hw);
extern void wlc_ltecx_interface_disable(wlc_hw_info_t *wlc_hw);
extern bool wlc_ltecx_rx_agg_off(wlc_info_t *wlc);
extern bool wlc_ltecx_tx_agg_off(wlc_info_t *wlc);
extern void wlc_ltecx_read_scanreq_bm(wlc_ltecx_info_t *ltecx,
       mws_scanreq_params_t* mws_scanreq_params, int Lte_idx);
extern void wlc_ltecx_update_scanreq_bm(wlc_ltecx_info_t *ltecx,
       mws_scanreq_params_t *mws_scanreq_params);
extern void wlc_ltecx_update_scanreq_bm_channel(wlc_ltecx_info_t *ltecx,
       chanspec_t chanspec);
extern void wlc_ltecx_ant_update(wlc_ltecx_info_t *ltecx);
extern void wlc_ltecx_update_seci_rxbreak(wlc_ltecx_info_t *ltecx);
#ifdef OCL
void wlc_lte_ocl_update(wlc_ltecx_info_t *ltecx, int cellstatus);
void wlc_stf_ocl_lte(wlc_info_t *wlc, bool disable);
#endif // endif

extern void wlc_ltecx_update_wci2_config(wlc_ltecx_info_t *ltecx);
#endif /* BCMLTECOEX */
extern bool wlc_ltecx_rx_agg_off(wlc_info_t *wlc);
extern bool wlc_ltecx_tx_agg_off(wlc_info_t *wlc);

#endif /* _wlc_ltecx_h_ */
