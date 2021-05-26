/*
 * BT Coex module
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
 * $Id$
 */

/**
 * @file
 * @brief
 * XXX Twiki: [BTCoexistenceHardware] [SoftwareApplicationNotes] [UcodeBTCoExistence]
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi_channels.h>
#include <bcmdevs.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_btcx.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_ap.h>
#include <wlc_stf.h>
#include <wlc_ampdu_cmn.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif // endif
#include <wlc_hw_priv.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#ifdef WLAIBSS
#include <wlc_aibss.h>
#endif // endif

static int wlc_btc_mode_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_wire_set(wlc_info_t *wlc, int int_val);
static int wlc_btc_flags_idx_set(wlc_info_t *wlc, int int_val, int int_val2);
static int wlc_btc_flags_idx_get(wlc_info_t *wlc, int int_val);
static void wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable);
static void wlc_btc_rssi_threshold_get(wlc_info_t *wlc);
static int wlc_btc_wlc_up(void *ctx);
static int wlc_btc_wlc_down(void *ctx);
int wlc_btcx_desense(wlc_btc_info_t *btc, int band);
#ifdef WL_BTCDYN
static int wlc_btc_dynctl_profile_set(wlc_info_t *wlc, void *parambuf);
static int wlc_btc_dynctl_profile_get(wlc_info_t *wlc, void *resbuf);
static int wlc_btc_dynctl_status_get(wlc_info_t *wlc, void *resbuf);
static int wlc_btc_dynctl_sim_get(wlc_info_t *wlc, void *resbuf);
static int wlc_btc_dynctl_sim_set(wlc_info_t *wlc, void *parambuf);
#endif /* WL_BTCDYN */
static int8 wlc_btc_get_btrssi(wlc_btc_info_t *btc);
static void wlc_btc_reset_btrssi(wlc_btc_info_t *btc);
static int wlc_btc_siso_ack_get(wlc_info_t *wlc);
static uint16 wlc_btc_sisoack_read_shm(wlc_info_t *wlc);
static void wlc_btc_sisoack_write_shm(wlc_info_t *wlc, uint16 sisoack);

#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
static void wlc_btc_pm_adjust(wlc_info_t *wlc,  bool bt_active);
#endif // endif
static int wlc_btc_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_btcx_watchdog(void *arg);

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
static int wlc_dump_btcx(wlc_info_t *wlc, struct bcmstrbuf *b);
static int wlc_clr_btcxdump(wlc_info_t *wlc);
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST */

#ifdef WLAIBSS
static uint wlc_btcx_aibss_ie_len(void *ctx, wlc_iem_calc_data_t *calc);
static int wlc_btcx_aibss_write_ie(void *ctx, wlc_iem_build_data_t *build);
static int wlc_btcx_aibss_parse_ie(void *ctx, wlc_iem_parse_data_t *parse);
static void wlc_btcx_strobe_enable(wlc_btc_info_t *btc, bool on);
static int wlc_btcx_scb_init(void *ctx, struct scb *scb);
static void wlc_btcx_scb_deinit(void *ctx, struct scb *scb);
static void wlc_btcx_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt);
static void wlc_btcx_aibss_set_prot(wlc_btc_info_t *btc, bool prot_on, bool cmn_cts2self);
static void wlc_btcx_aibss_check_state(wlc_btc_info_t *btc);
static void wlc_btcx_aibss_read_bt_sync_state(wlc_btc_info_t *btc);
static void wlc_btcx_aibss_get_local_bt_state(wlc_btc_info_t *btc);
static void wlc_btcx_aibss_update_agg_state(wlc_btc_info_t *btc);
static void wlc_btc_aibss_get_state(wlc_btc_info_t *btc, uint8 action);
static void wlc_btcx_aibss_calc_tsf(wlc_btc_info_t *btc, uint32 *new_tsf_l,
	uint32 mod_factor, uint16 add_delta);
static void wlc_btcx_aibss_update_agg_size(wlc_btc_info_t *btc);
static void wlc_btcx_aibss_chk_clk_sync(wlc_btc_info_t *btc);

/* Ucode Rev in which Oxygen support was added */
#define BTCX_OXYGEN_SUPPORT_UCODE_REV	0x3c40000 /* BOM  964.0 */

/* Actions supported by wlc_btc_aibss_get_state () */
#define CHECK_AIBSS_STALE_BEACON	0x1
#define CHECK_AIBSS_BT_SYNC_STATE	0x2

/* Periods, intervals related to BT */
#define BT_ESCO_PERIOD			7500 /* us */
#define BT_SCO_PERIOD			3750 /* us */
#define BT_STROBE_INTERVAL		30000 /* us */
#define BT_ESCO_OFF_DELAY_CNT		30 /* Delay reporting eSCO Off count */
#define BT_STALE_BEACON_CHK_PERIOD	5 /* watchdog period: ~ 1 sec */

#define BT_AIBSS_RX_AGG_SIZE		16

/* BT-WLAN information (via GCI) related bits */
#define BT_IS_SLAVE			0x8000
#define BT_CLK_SYNC_STS			0x40
#define BT_CLK_SYNC_STS_SHIFT_BITS	6

/* defines related to BT info */
#define BT_INFO_TASK_TYPE_SCO		0x1
#define BT_INFO_TASK_TYPE_ESCO		0x2
#define BT_INFO_BT_CLK_SYNCED		0x4

/* BTCX info structure for Oxygen */
typedef struct {
	uint32 btinfo;
	uint32 bcn_rx_cnt;
} btcx_oxygen_info_t;

#endif /* WLAIBSS */

#define BT_AMPDU_THRESH		10000	/* if BT period < this threshold, turn off ampdu */
#define	BT_AMPDU_THRESH_ABORTCNT	20
/* if BT has constant critical or high priority requests */

#define IS_4356Z_BTCX(phy_rev, radiorev, xtalfreq, wlc) \
	((ACREV_IS(phy_rev, 8) || ACREV_IS(phy_rev, 15) || ACREV_IS(phy_rev, 17)) && \
	((radiorev == 0x27) || (radiorev == 0x29)) && \
	(xtalfreq == 37400) && WLCISACPHY(wlc->band) && \
	!(wlc->pub->boardflags & BFL_SROM11_EXTLNA) && \
	BAND_2G(wlc->band->bandtype))

enum {
	IOV_BTC_MODE,           /* BT Coexistence mode */
	IOV_BTC_WIRE,           /* BTC number of wires */
	IOV_BTC_STUCK_WAR,       /* BTC stuck WAR */
	IOV_BTC_FLAGS,          /* BT Coex ucode flags */
	IOV_BTC_PARAMS,         /* BT Coex shared memory parameters */
	IOV_BTCX_CLEAR_DUMP,    /* clear btcx stats */
	IOV_BTC_SISO_ACK,       /* Specify SISO ACK antenna, disabled when 0 */
	IOV_BTC_RXGAIN_THRESH,  /* Specify restage rxgain thresholds */
	IOV_BTC_RXGAIN_FORCE,   /* Force for BTC restage rxgain */
	IOV_BTC_RXGAIN_LEVEL,    /* Set the BTC restage rxgain level */
#ifdef WL_BTCDYN
	IOV_BTC_DYNCTL,			/* get/set coex dynctl profile */
	IOV_BTC_DYNCTL_STATUS,  /* get dynctl status: dsns, btpwr, wlrssi, btc_mode, etc */
	IOV_BTC_DYNCTL_SIM,		/* en/dis & config dynctl algo simulation mode */
#endif // endif
#if defined(BCMDBG)
#ifdef WLAIBSS
	IOV_BTC_SET_STROBE,	/* Get/Set BT Strobe */
#endif // endif
	IOV_BTC_BTRSSI_AVG,     /* avererage btrssi (clear all btrssi history when 0) */
	IOV_BTC_BTRSSI_THRESH,  /* btrssi threshold to implicitly switch btc mode */
	IOV_BTC_BTRSSI_HYST,    /* hysteresis for btrssi */
	IOV_BTC_WLRSSI_THRESH,   /* wlrssi threshold to implicitly switch btc mode */
	IOV_BTC_WLRSSI_HYST,    /* hysteresis for wlrssi */
	IOV_BTC_DYAGG          /* force on/off dynamic tx aggregation */
#endif /* BCMDBG */
};

const bcm_iovar_t btc_iovars[] = {
	{"btc_mode", IOV_BTC_MODE, 0, IOVT_UINT32, 0},
	{"btc_stuck_war", IOV_BTC_STUCK_WAR, 0, IOVT_BOOL, 0 },
	{"btc_flags", IOV_BTC_FLAGS, (IOVF_SET_UP | IOVF_GET_UP), IOVT_BUFFER, 0 },
	{"btc_params", IOV_BTC_PARAMS, 0, IOVT_BUFFER, 0 },
#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
	{"btcx_clear_dump", IOV_BTCX_CLEAR_DUMP, (IOVF_SET_CLK), IOVT_VOID, 0 },
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST */
	{"btc_siso_ack", IOV_BTC_SISO_ACK, 0, IOVT_INT16, 0
	},
	{"btc_rxgain_thresh", IOV_BTC_RXGAIN_THRESH, 0, IOVT_UINT32, 0
	},
	{"btc_rxgain_force", IOV_BTC_RXGAIN_FORCE, 0, IOVT_UINT32, 0
	},
	{"btc_rxgain_level", IOV_BTC_RXGAIN_LEVEL, 0, IOVT_UINT32, 0
	},
#ifdef WL_BTCDYN
	/* set dynctl profile, get status , etc */
	{"btc_dynctl", IOV_BTC_DYNCTL, 0, IOVT_BUFFER, 0
	},
	/* set dynctl status */
	{"btc_dynctl_status", IOV_BTC_DYNCTL_STATUS, 0, IOVT_BUFFER, 0
	},
	/* enable & configure dynctl simulation mode (aka dryrun) */
	{"btc_dynctl_sim", IOV_BTC_DYNCTL_SIM, 0, IOVT_BUFFER, 0
	},
#endif // endif
#if defined(BCMDBG)
#ifdef WLAIBSS
	{"btc_set_strobe", IOV_BTC_SET_STROBE, 0, IOVT_UINT32, 0
	},
#endif // endif
	{"btc_btrssi_avg", IOV_BTC_BTRSSI_AVG, 0, IOVT_INT8, 0},
	{"btc_btrssi_thresh", IOV_BTC_BTRSSI_THRESH, 0, IOVT_INT8, 0},
	{"btc_btrssi_hyst", IOV_BTC_BTRSSI_HYST, 0, IOVT_UINT8, 0},
	{"btc_wlrssi_thresh", IOV_BTC_WLRSSI_THRESH, 0, IOVT_INT8, 0},
	{"btc_wlrssi_hyst", IOV_BTC_WLRSSI_HYST, 0, IOVT_UINT8, 0},
	{"btc_dyagg", IOV_BTC_DYAGG, 0, IOVT_INT8, 0},
#endif /* BCMDBG */
	{NULL, 0, 0, 0, 0}
};

#ifdef WL_BTCDYN
/* dynamic tx power control based on RSSI for hybrid coex */
#define DYN_PWR_MAX_STEPS	8
#define DYN_PWR_DFLT_STEPS	6
#define DYN_PWR_DFLT_QDBM	127

/*  btcdyn nvram variables to initialize the profile  */
static const char BCMATTACHDATA(rstr_btcdyn_flags)[] = "btcdyn_flags";
static const char BCMATTACHDATA(rstr_btcdyn_dflt_dsns_level)[] = "btcdyn_dflt_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_low_dsns_level)[] = "btcdyn_low_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_mid_dsns_level)[] = "btcdyn_mid_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_high_dsns_level)[] = "btcdyn_high_dsns_level";
static const char BCMATTACHDATA(rstr_btcdyn_default_btc_mode)[] = "btcdyn_default_btc_mode";
static const char BCMATTACHDATA(rstr_btcdyn_msw_rows)[] = "btcdyn_msw_rows";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_rows)[] = "btcdyn_dsns_rows";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row0)[] = "btcdyn_msw_row0";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row1)[] = "btcdyn_msw_row1";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row2)[] = "btcdyn_msw_row2";
static const char BCMATTACHDATA(rstr_btcdyn_msw_row3)[] = "btcdyn_msw_row3";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row0)[] = "btcdyn_dsns_row0";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row1)[] = "btcdyn_dsns_row1";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row2)[] = "btcdyn_dsns_row2";
static const char BCMATTACHDATA(rstr_btcdyn_dsns_row3)[] = "btcdyn_dsns_row3";
static const char BCMATTACHDATA(rstr_btcdyn_btrssi_hyster)[] = "btcdyn_btrssi_hyster";
static const char BCMATTACHDATA(rstr_btcdyn_coex_two_ant)[] = "btcdyn_coex_two_ant";
static const char BCMATTACHDATA(rstr_btcdyn_wlpwr_thresh)[] = "btcdyn_wlpwr_thresh";
static const char BCMATTACHDATA(rstr_btcdyn_wlpwr_val)[] = "btcdyn_wlpwr_val";
#endif /* WL_BTCDYN */

/* BT RSSI threshold for implict mode switching */
static const char BCMATTACHDATA(rstr_prot_btrssi_thresh)[] = "prot_btrssi_thresh";
/* siso ack setting for hybrid mode */
static const char BCMATTACHDATA(rstr_btc_siso_ack)[] = "btc_siso_ack";

/* BTC stuff */

struct wlc_btc_info {
	wlc_info_t *wlc;
	uint16  bth_period;             /* bt coex period. read from shm. */
	bool    bth_active;             /* bt active session */
	uint8   prot_rssi_thresh;       /* rssi threshold for forcing protection */
	uint8   ampdutx_rssi_thresh;    /* rssi threshold to turn off ampdutx */
	uint8   ampdurx_rssi_thresh;    /* rssi threshold to turn off ampdurx */
	uint8   high_threshold;         /* threshold to switch to btc_mode 4 */
	uint8   low_threshold;          /* threshold to switch to btc_mode 1 */
	uint8   host_requested_pm;      /* saved pm state specified by host */
	uint8   mode_overridden;        /* override btc_mode for long range */
	/* cached value for btc in high driver to avoid frequent RPC calls */
	int     mode;
	int     wire;
	/* SISO ACK antenna (bits 0-7 specify core mask) */
	/* for btcdyn, bits 8-11: max siso ack power (dBm); bits 12-15: current power */
	int     siso_ack;
	int     restage_rxgain_level;
	int     restage_rxgain_force;
	int     restage_rxgain_active;
	uint8   restage_rxgain_on_rssi_thresh;  /* rssi threshold to turn on rxgain restaging */
	uint8   restage_rxgain_off_rssi_thresh; /* rssi threshold to turn off rxgain restaging */
	uint16  agg_off_bm;
	bool    siso_ack_ovr;           /* siso_ack set 0: automatically 1: by iovar */
#ifdef WL_BTCDYN
	/* BTCOEX extension: adds dynamic desense & modes witching feature */
	uint16	bt_pwr_shm;	/* last raw/per task bt_pwr read from ucode */
	int8	bt_pwr;		/* current bt power  */
	int8	wl_rssi;	/* last wl rssi */
	uint8	cur_dsns; /* current desense level */
	dctl_prof_t *dprof;	/* current board dynctl profile  */
	btcx_dynctl_calc_t desense_fn;  /* calculate desense level  */
	btcx_dynctl_calc_t mswitch_fn;  /* calculate mode switch */
	/*  stimuli for dynctl dry runs(fake BT & WL activity) */
	int8	sim_btpwr;
	int8	sim_wlrssi;
	int8	sim_btrssi;
	/* current mode switching hysteresis */
	int8 msw_btrssi_hyster;	/* from bt rssi */
	bool coex_two_ant;		/* special WAR  for Samsung 5g COEX h/w issue */
	bool	dynctl_sim_on;  /* enable/disable simulation mode */
	uint32	prev_btpwr_ts;	/* timestamp of last call to btc_dynctl() */
	int8	prev_btpwr;		/* prev btpwr reading to filter out false invalids */
#endif	/* WL_BTCDYN */

	int8	btrssi[BTC_BTRSSI_SIZE]; /* array of recent BT RSSI values */
	int16	btrssi_sum;	/*  bt rssi MA sum */
	uint8	btrssi_cnt;	/* number of btrssi samples */
	uint8	btrssi_idx;	/* index to bt_rssi sample array */
	int8	bt_rssi;	/* averaged bt rssi */
	uint16	bt_shm_addr;
	uint8	run_cnt;

#ifdef	WLAIBSS
	/* AIBSS related */
	int32	scb_handle;	/* SCB CUBBY OFFSET */
	uint32	prev_tsf_l;
	uint32	prev_tsf_h;
	uint32	last_btinfo;
	uint32  local_btinfo;
	uint8	bt_out_of_sync_cnt;
	uint8	esco_off_cnt;
	bool	strobe_enabled;
	bool	strobe_on;	/* strobe to BT for Oxygen */
	bool	local_bt_in_sync; /* Sync status of local BT */
	bool	other_bt_in_sync;
	bool	local_bt_is_master;
	bool	sco_prot_on;
	bool	other_esco_present;
	bool	rx_agg_change;
	bool	rx_agg_modified;
	bool	acl_grant_set;
#endif /* WLAIBSS */
	int	flags;
	int	abort_prev_cnt;
	int	abort_curr_cnt;
	uint8   btrssi_hyst;             /* btrssi hysteresis */
	uint8   wlrssi_hyst;             /* wlrssi hysteresis */
	bool    simrx_slna;              /* simultaneous rx with shared fem/lna */
	int8    dyagg;                   /* dynamic tx agg (1: on, 0: off, -1: auto) */
#ifdef WL_BTCDYN
	/* dynamic tx power adjustment based on RSSI for hybrid coex */
	uint8 wlpwr_steps;
	int8 *wlpwr_thresh;
	int8 *wlpwr_val;
#endif	/* WL_BTCDYN */
	int8	prot_btrssi_thresh; /* used by implicit mode switching */
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>
#ifdef WL_BTCDYN
/* dynamic BTCOEX wl densense & coex mode switching */

/*
	MAC --> notify BTCOEX c band & chanspec has changed
	note: the code in this function needs to be as small as possible
	it is on realtime switching band/channel path
*/
void wlc_btcx_chspec_change_notify(wlc_info_t *wlc, chanspec_t chanspec, bool switchband)
{
#ifdef	DYNCTL_DBG
	chanspec_t old_chanspec = wlc->chanspec;
#endif /* DYNCTL_DBG */
	wlc_btc_info_t *btc = wlc->btch;
	bool bt_active;

	BTCDBG(("%s:wl%d: old chspec:0x%x new chspec:0x%x, bandsw:%d\n",
		__FUNCTION__, wlc->pub->unit,
		old_chanspec, chanspec, switchband));

	/*
	  note new bt activity detection uses wlc_btcx_get_btpwr() func call,
	  but to keep thE code small we'll use original Karthik's detection
	*/
	bt_active = btc->bth_active;

	if (btc->coex_two_ant && bt_active) {
		/*  BT is active & the board is 2 ant coex design *
		    WAR# we need to force wl to enforce spatioal policy for wl
		    to operate on core0 if wl is sitching to 5g channel
		*/
		 if (CHSPEC_IS5G(chanspec)) {
			/*	use coremask to override spatioal policy
				for ofdm & b rates -> single stream, core0 operation only
			*/
			BTCDBG(("BT is active, WL _/-> 5g, "
				"current txcore_mask[cck,ofdm,nsts1,nsts2\n]"
				":%0x,%0x,%0x,%0x\n",
				wlc->stf->txcore[0][1],
				wlc->stf->txcore[1][1],
				wlc->stf->txcore[2][1],
				wlc->stf->txcore[3][1]));

			wlc->stf->txcore_override[NSTS2_IDX] = 0x01;
			wlc->stf->txcore_override[NSTS1_IDX] = 0x01;
			wlc->stf->txcore_override[OFDM_IDX]  = 0x1;
			wlc->stf->txcore_override[CCK_IDX] = 0x1;

		 } else {
			/* new chan is 2g, reset txcore override */
			uint8 i;
			BTCDBG(("BT is active wl_/-> 2G, remove coremask override\n"));
			for (i = 0; i < MAX_CORE_IDX-1; i++) {
				wlc->stf->txcore_override[i] = 0;
			}
		 }
	}
}

/* extract the bitfield, check if invalid */
static void btcx_extract_pwr(int16 btpwr, uint8 shft, int8 *pwr8)
{
	int8 tmp;
	*pwr8 = BT_INVALID_TX_PWR;

	if ((tmp = (btpwr >> shft) & SHM_BTC_MASK_TXPWR)) {
		*pwr8 = (tmp * BT_TX_PWR_STEP) + BT_TX_PWR_OFFSET;
	}
}

#ifdef DBG_BTPWR_HOLES
typedef struct pwrs {
	int8 pwr;
	uint32 ts;
} pwrs_t;
static void btcdyn_detect_btpwrhole(wlc_btc_info_t *btc, int8 cur_pwr)
{
	static pwrs_t pwr_smpl[4] = {{-127, 0}, {-127, 0}, {-127, 0}, {-127, 0}};
	static uint8 pwr_idx = 0;
	int32 cur_ts;

	cur_ts =  OSL_SYSUPTIME();
	pwr_smpl[pwr_idx].pwr = cur_pwr;
	pwr_smpl[pwr_idx].ts = cur_ts;

	/* detect a hole (an abnormality) in PWR sampling sequence */
	if ((pwr_smpl[pwr_idx].pwr != BT_INVALID_TX_PWR) &&
		(pwr_smpl[(pwr_idx-1) & 0x3].pwr == BT_INVALID_TX_PWR) &&
		(pwr_smpl[(pwr_idx-2) & 0x3].pwr != BT_INVALID_TX_PWR)) {

		DYNCTL_ERROR(("BTPWR hole at T-1:%d, delta from T-2:%d\n"
			" btpwr:[t, t-1, t-2]:%d,%d,%d\n", pwr_smpl[(pwr_idx-1) & 0x3].ts,
			(pwr_smpl[(pwr_idx-1) & 0x3].ts - pwr_smpl[(pwr_idx-2) & 0x3].ts),
			pwr_smpl[pwr_idx].pwr, pwr_smpl[(pwr_idx-1) & 0x3].pwr,
			pwr_smpl[(pwr_idx-2) & 0x3].pwr));

	}
	pwr_idx = (pwr_idx + 1) & 0x3;
}
#endif /* DBG_BTPWR_HOLES */

/*
	checks for BT power of each active task, converts to dBm and
	returns the highest power level if there is > 1 task detected.
*/
static int8 wlc_btcx_get_btpwr(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;

	int8 pwr_sco, pwr_a2dp, pwr_sniff, pwr_acl;
	int8 result_pwr = BT_INVALID_TX_PWR;
	uint16 txpwr_shm;
	uint16 btcx_blk_ptr = wlc->hw->btc->bt_shm_addr;
	int8 pwr_tmp;

	/* read btpwr  */
	txpwr_shm = wlc_read_shm(wlc, btcx_blk_ptr + M_BTCX_BT_TXPWR);
	btc->bt_pwr_shm = txpwr_shm; /* keep a copy for dbg & status */

	/* clear the shm after read, ucode will refresh with a new value  */
	wlc_write_shm(wlc,  btcx_blk_ptr + M_BTCX_BT_TXPWR, 0);

	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_SCO, &pwr_sco);
	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_A2DP, &pwr_a2dp);
	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_SNIFF, &pwr_sniff);
	btcx_extract_pwr(txpwr_shm, SHM_BTC_SHFT_TXPWR_ACL, &pwr_acl);

	/*
	  although rare, both a2dp & sco may be active,
	  pick the highest one. if both are invalid, check sniff
	*/
	if (pwr_sco != BT_INVALID_TX_PWR ||
		pwr_a2dp != BT_INVALID_TX_PWR) {

		BTCDBG(("%s: shmem_val:%x, BT tasks pwr: SCO:%d, A2DP:%d, SNIFF:%d\n",
			__FUNCTION__, txpwr_shm, pwr_sco, pwr_a2dp, pwr_sniff));

		result_pwr = pwr_sco;
		if (pwr_a2dp > pwr_sco)
			result_pwr = pwr_a2dp;

	} else if (pwr_acl != BT_INVALID_TX_PWR) {
		result_pwr = pwr_acl;
	} else if (pwr_sniff != BT_INVALID_TX_PWR) {
		result_pwr = pwr_sniff;
	}

#ifdef DBG_BTPWR_HOLES
	btcdyn_detect_btpwrhole(btc, result_pwr);
#endif // endif

	/* protect from single invalid pwr reading ("pwr hole") */
	if (result_pwr == BT_INVALID_TX_PWR) {
		BTCDBG(("cur btpwr invalid, use prev value:%d\n",
			btc->prev_btpwr));
		pwr_tmp = btc->prev_btpwr;
	} else {
		pwr_tmp = result_pwr;
	}

	btc->prev_btpwr = result_pwr;
	result_pwr = pwr_tmp;
	return result_pwr;
}

/*
	At a given BT TX PWR level (typically > 7dbm)
	there is a certain WL RSSI level range at which WL performance in Hybrid
	COEX mode is actually lower than in Full TDM.
	The algorithm below selects the right mode based on tabulated data points.
	The profile table is specific for each board and needs to be calibrated
	for every new board + BT+WIFI antenna design.
*/
static uint8 btcx_dflt_mode_switch(wlc_info_t *wlc, int8 wl_rssi, int8 bt_pwr, int8 bt_rssi)
{
	wlc_btc_info_t *btc = wlc->btch;
	dctl_prof_t *profile = btc->dprof;
	uint8 row, new_mode;
	new_mode = profile->default_btc_mode;

#ifdef BCMLTECOEX
	/* No mode switch is required if ltecx is ON and Not in desense mode */
	if (wlc_ltecx_get_lte_status(wlc) &&
		!wlc->hw->ltecx_hw->mws_elna_bypass) {
		return WL_BTC_FULLTDM;
	}
#endif // endif

	/* no active BT task when bt_pwr is invalid */
	if	(bt_pwr == BT_INVALID_TX_PWR) {
		return new_mode;
	}

	/* keep current coex mode  if: */
	if ((btc->bt_rssi == BT_RSSI_INVALID) ||
		(btc->wl_rssi == WLC_RSSI_INVALID)) {
		return btc->mode;
	}

	for (row = 0; row < profile->msw_rows; row++) {

		if ((bt_pwr >= profile->msw_data[row].bt_pwr) &&
			(bt_rssi < profile->msw_data[row].bt_rssi +
			btc->msw_btrssi_hyster) &&
			(wl_rssi > profile->msw_data[row].wl_rssi_low) &&
			(wl_rssi < profile->msw_data[row].wl_rssi_high)) {
			/* new1: fallback mode is now per {btpwr + bt_rssi + wl_rssi range} */
				new_mode = profile->msw_data[row].mode;
			break;
		}
	}

	if (new_mode != profile->default_btc_mode) {
		/* the new mode is a downgrade from the default one,
		 for wl & bt signal conditions have deteriorated.
		 Apply hysteresis to stay in this mode until
		 the conditions get better by >= hyster values
		*/
		/*  positive for bt rssi  */
		btc->msw_btrssi_hyster = profile->msw_btrssi_hyster;
	} else {
		/* in or has switched to default, turn off h offsets */
		btc->msw_btrssi_hyster = 0;
	}

	return new_mode;
}

/*
* calculates new desense level using
* current btcmode, bt_pwr, wl_rssi,* and board profile data points
*/
static uint8 btcx_dflt_get_desense_level(wlc_info_t *wlc, int8 wl_rssi, int8 bt_pwr, int8 bt_rssi)
{
	wlc_btc_info_t *btc = wlc->btch;
	dctl_prof_t *profile = btc->dprof;
	uint8 row, new_level;

	new_level = profile->dflt_dsns_level;

	/* BT "No tasks" -> use default desense */
	if	(bt_pwr == BT_INVALID_TX_PWR) {
		return new_level;
	}

	/*  keep current desense level if: */
	if ((btc->bt_rssi == BT_RSSI_INVALID) ||
		(btc->wl_rssi == WLC_RSSI_INVALID)) {
		return btc->cur_dsns;
	}

	for (row = 0; row < profile->dsns_rows; row++) {
		if (btc->mode == profile->dsns_data[row].mode &&
			bt_pwr >= profile->dsns_data[row].bt_pwr) {

			if (wl_rssi > profile->dsns_data[row].wl_rssi_high) {
				new_level = profile->high_dsns_level;
			}
			else if (wl_rssi > profile->dsns_data[row].wl_rssi_low) {

				new_level = profile->mid_dsns_level;
			} else {
				new_level = profile->low_dsns_level;
			}
			break;
		}
	}
	return  new_level;
}

/*
* Dynamic Tx power control
*/
static void
btcx_dyn_txpwr_ctrl(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc = wlc->btch;
	int cur_txpwr, new_txpwr, ackPwr;
	uint16 sisoAck;
	uint8 idx;

	/*  keep current tx power if: */
	if (btc->wl_rssi == WLC_RSSI_INVALID ||
		btc->wlpwr_steps == 0) {
		return;
	}

	/* get current tx power */
	wlc_iovar_getint(wlc, "qtxpower", &cur_txpwr);

	/* restore default pwr if BT is not active or not in hybrid or parallel coex */
	if (!btc->bth_active ||
		!(btc->mode == WL_BTC_HYBRID || btc->mode == WL_BTC_PARALLEL)) {
		if (cur_txpwr != DYN_PWR_DFLT_QDBM) {
			wlc_iovar_setint(wlc, "qtxpower", DYN_PWR_DFLT_QDBM);
		}
		return;
	}

	/* find new target pwr based on RSSI */
	for (idx = 0; idx < btc->wlpwr_steps - 1; idx++) {
		if (btc->wl_rssi > btc->wlpwr_thresh[idx]) break;
	}
	new_txpwr = btc->wlpwr_val[idx] * 4; /* convert to quarter dBm */

	if (btc->mode == WL_BTC_PARALLEL) {
		/* apply new Tx power for all packets */
		if (new_txpwr != cur_txpwr) {
			wlc_iovar_setint(wlc, "qtxpower", new_txpwr);
		}
	} else if (btc->mode == WL_BTC_HYBRID) {
		if (btc->siso_ack & BTC_SISOACK_CORES_MASK) {
			/* siso ack used, adjust siso ack pwr */
			if (cur_txpwr == DYN_PWR_DFLT_QDBM) {
				/* if current tgt pwr is unknown, set to max pwr by user */
				cur_txpwr = btc->wlpwr_val[btc->wlpwr_steps - 1] * 4;
				wlc_iovar_setint(wlc, "qtxpower", cur_txpwr);
			}
			/* check max siso ack pwr in quarter dBm */
			ackPwr = (btc->siso_ack & BTC_SISOACK_MAXPWR_FLD) >>
				(BTC_SISOACK_MAXPWR_POS - 2);
			if (ackPwr > 0 && new_txpwr > ackPwr) {
				new_txpwr = ackPwr;
			}
			/* change if new siso ack pwr is not higher than current tgt pwr */
			if (new_txpwr <= cur_txpwr) {
				btc->siso_ack = new_txpwr << (BTC_SISOACK_CURPWR_POS - 2) |
					(btc->siso_ack & (~BTC_SISOACK_CURPWR_FLD));
				/* get siso ack power offset in s5.1 or s4.1 format */
				ackPwr = (cur_txpwr - new_txpwr) >> 1;
				sisoAck = (uint16)(ackPwr << 8 |
					(btc->siso_ack & BTC_SISOACK_CORES_MASK));
				/* update siso ack */
				if (sisoAck != wlc_btc_sisoack_read_shm(wlc)) {
					wlc_btc_sisoack_write_shm(wlc, sisoAck);
				}
			}
		} else {
			/* siso ack not used; set pwr for all pkts */
			if (new_txpwr != cur_txpwr) {
				wlc_iovar_setint(wlc, "qtxpower", new_txpwr);
			}
		}
	}
}

/* set external desense handler */
int btcx_set_ext_desense_calc(wlc_info_t *wlc, btcx_dynctl_calc_t fn)
{
	wlc_btc_info_t *btch = wlc->btch;

	ASSERT(fn);
	btch->desense_fn = fn;
	return BCME_OK;
}

/* set external mode switch handler */
int btcx_set_ext_mswitch_calc(wlc_info_t *wlc, btcx_dynctl_calc_t fn)
{
	wlc_btc_info_t *btch = wlc->btch;

	ASSERT(fn);
	btch->mswitch_fn = fn;
	return BCME_OK;
}

/* wrapper for real/ or dry run */
static int8 btcx_get_wl_rssi(wlc_btc_info_t *btc)
{
	return btc->wlc->cfg->link->rssi;
}

/*
	dynamic COEX CTL (desense & switching) called from btc_wtacdog()
*/
static void wlc_btcx_dynctl(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;
	uint8 btc_mode = wlc->btch->mode;
	dctl_prof_t *ctl_prof = btc->dprof;
	uint16 btcx_blk_ptr = wlc->hw->btc->bt_shm_addr;
	uint32 cur_ts = OSL_SYSUPTIME();

	/* protection against too frequent calls from btc watchdog context */
	if ((cur_ts - btc->prev_btpwr_ts) < DYNCTL_MIN_PERIOD) {
		btc->prev_btpwr_ts = cur_ts;
		return;
	}
	btc->prev_btpwr_ts = cur_ts;

	btc->bt_pwr = wlc_btcx_get_btpwr(btc);
	btc->wl_rssi = btcx_get_wl_rssi(btc);
	btc->bt_rssi = wlc_btc_get_btrssi(btc);

	if (btc->dynctl_sim_on) {
	/* simulation mode is on  */
		btc->bt_pwr = btc->sim_btpwr;
		btc->wl_rssi = btc->sim_wlrssi;
		btc->bt_rssi = btc->sim_btrssi;
	}

	if (IS_MSWITCH_ON(ctl_prof)) {
		uint8 new_mode;

		ASSERT(btc->mswitch_fn);
		new_mode = btc->mswitch_fn(wlc, btc->wl_rssi, btc->bt_pwr, btc->bt_rssi);

		if (new_mode != btc_mode) {
			wlc_btc_mode_set(wlc, new_mode);

			BTCDBG(("%s mswitch mode:%d -> mode:%d,"
				" bt_pwr:%d, wl_rssi:%d, cur_dsns:%d, BT hstr[rssi:%d]\n",
				__FUNCTION__, btc_mode, new_mode, btc->bt_pwr,
				btc->wl_rssi, btc->cur_dsns,
				ctl_prof->msw_btrssi_hyster));

			if ((wlc->hw->boardflags & BFL_FEM_BT) == 0 && /* dLNA chip */
				btcx_blk_ptr != 0) {
				/* update btcx_host_flags  based on btc_mode */
				if (new_mode == WL_BTC_FULLTDM) {
					wlc_bmac_update_shm(wlc->hw,
						btcx_blk_ptr + M_BTCX_HOST_FLAGS,
						BTCX_HFLG_DLNA_TDM_VAL, BTCX_HFLG_DLNA_MASK);
				} else {
					/* mainly for hybrid and parallel */
					wlc_bmac_update_shm(wlc->hw,
						btcx_blk_ptr + M_BTCX_HOST_FLAGS,
						BTCX_HFLG_DLNA_DFLT_VAL, BTCX_HFLG_DLNA_MASK);
				}
			}
		}
	}

	/* enable protection after mode switching */
	wlc_enable_btc_ps_protection(wlc, wlc->cfg, TRUE);

	/* check if we need to switch the desense level */
	if (IS_DESENSE_ON(ctl_prof)) {
		uint8 new_level;

		ASSERT(btc->desense_fn);
		new_level = btc->desense_fn(wlc, btc->wl_rssi, btc->bt_pwr, btc->bt_rssi);

		if (new_level != btc->cur_dsns) {
			/* apply new desense level */
			if ((wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				new_level)) == BCME_OK) {

				BTCDBG(("%s: set new desense:%d, prev was:%d btcmode:%d,"
					" bt_pwr:%d, wl_rssi:%d\n",
					__FUNCTION__, new_level, btc->cur_dsns, btc->mode,
					btc->bt_pwr, btc->wl_rssi));

				btc->cur_dsns = new_level;
			} else
				WL_ERROR(("%s desense apply error\n",
					__FUNCTION__));
		}
	}

	/* dynamic tx power control */
	if (IS_PWRCTRL_ON(ctl_prof)) {
		btcx_dyn_txpwr_ctrl(wlc);
	}
}

static int wlc_btc_dynctl_profile_set(wlc_info_t *wlc, void *parambuf)
{
	wlc_btc_info_t *btc = wlc->btch;

	bcopy(parambuf, btc->dprof, sizeof(dctl_prof_t));
	return BCME_OK;
}

static int wlc_btc_dynctl_profile_get(wlc_info_t *wlc, void *resbuf)
{
	wlc_btc_info_t *btc = wlc->btch;

	bcopy(btc->dprof, resbuf, sizeof(dctl_prof_t));
	return BCME_OK;
}

/* get dynctl status iovar handler */
static int wlc_btc_dynctl_status_get(wlc_info_t *wlc, void *resbuf)
{
	wlc_btc_info_t *btc = wlc->btch;
	dynctl_status_t dynctl_status;

	/* agg. stats into local stats var */
	dynctl_status.sim_on = btc->dynctl_sim_on;
	dynctl_status.bt_pwr_shm  = btc->bt_pwr_shm;
	dynctl_status.bt_pwr = btc->bt_pwr;
	dynctl_status.bt_rssi = btc->bt_rssi;
	dynctl_status.wl_rssi = btc->wl_rssi;
	dynctl_status.dsns_level = btc->cur_dsns;
	dynctl_status.btc_mode = btc->mode;

	/* return it */
	bcopy(&dynctl_status, resbuf, sizeof(dynctl_status_t));
	return BCME_OK;
}

/*   get dynctl sim parameters */
static int wlc_btc_dynctl_sim_get(wlc_info_t *wlc, void *resbuf)
{
	dynctl_sim_t sim;
	wlc_btc_info_t *btc = wlc->btch;

	sim.sim_on = btc->dynctl_sim_on;
	sim.btpwr = btc->sim_btpwr;
	sim.btrssi = btc->sim_btrssi;
	sim.wlrssi = btc->sim_wlrssi;

	bcopy(&sim, resbuf, sizeof(dynctl_sim_t));
	return BCME_OK;
}

/*   set dynctl sim parameters */
static int wlc_btc_dynctl_sim_set(wlc_info_t *wlc, void *parambuf)
{
	dynctl_sim_t sim;

	wlc_btc_info_t *btc = wlc->btch;
	bcopy(parambuf, &sim, sizeof(dynctl_sim_t));

	btc->dynctl_sim_on = sim.sim_on;
	btc->sim_btpwr = sim.btpwr;
	btc->sim_btrssi = sim.btrssi;
	btc->sim_wlrssi = sim.wlrssi;

	return BCME_OK;
}

#endif /* WL_BTCDYN desense &  mode switching */

#ifdef WL_BTCDYN
/*
* initialize one row btc_thr_data_t * from a named nvram var
*/
static int
BCMATTACHFN(wlc_btc_dynctl_init_trow)(wlc_btc_info_t *btc,
	btc_thr_data_t *trow, const char *varname, uint16 xpected_sz)
{
	wlc_info_t *wlc = btc->wlc;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 j = 0;

	/* read mode switching table */
	int array_sz = getintvararraysize(wlc_hw->vars, varname);
	if (!array_sz)
		return 0; /* var is not present */

	/* mk sure num of items in the var is OK */
	if (array_sz != xpected_sz)
		return -1;

	trow->mode = getintvararray(wlc_hw->vars, varname, j++);
	trow->bt_pwr = getintvararray(wlc_hw->vars, varname, j++);
	trow->bt_rssi = getintvararray(wlc_hw->vars, varname, j++);
	trow->wl_rssi_high = getintvararray(wlc_hw->vars, varname, j++);
	trow->wl_rssi_low = getintvararray(wlc_hw->vars, varname, j++);
	return 1;
}
#endif /* WL_BTCDYN */

/* Read bt rssi from shm and do moving average. Return true if bt rssi < thresh. */
static int8 wlc_btc_get_btrssi(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc = btc->wlc;
	uint16 btrssi_shm, btcx_blk_ptr;
	int16 btrssi_avg = 0;

/* avoid WIN32 NIC driver compile errors */
#ifdef WLC_LOW
	btcx_blk_ptr = wlc->hw->btc->bt_shm_addr;
#else
	btcx_blk_ptr = 2 * wlc_bmac_read_shm(wlc->hw, M_BTCX_BLK_PTR);
#endif // endif

	if (!btc->bth_active || btcx_blk_ptr == 0)
		return BT_RSSI_INVALID;

	/* read btrssi idx from shm */
	btrssi_shm = wlc_bmac_read_shm(wlc->hw, btcx_blk_ptr + M_BTCX_RSSI);

	if (btrssi_shm) {
		int8 cur_rssi, old_rssi;
		/* clear shm because ucode keeps max btrssi idx */
		wlc_bmac_write_shm(wlc->hw, btcx_blk_ptr + M_BTCX_RSSI, 0);

		/* actual bt rssi = -1 x (btrssi_shm x 5 + 10) */
		cur_rssi = (-1) * (int8)(btrssi_shm * BT_RSSI_STEP + BT_RSSI_OFFSET);

		/* after BT on let accumulate > BTC_BTRSSI_SIZE */
		if (btc->btrssi_cnt < BTC_BTRSSI_SIZE)
			btc->btrssi_cnt++;

		/* accumulate & calc moving average */
		old_rssi = btc->btrssi[btc->btrssi_idx];
		btc->btrssi[btc->btrssi_idx] = cur_rssi;
		/* sum = -old one, +new  */
		btc->btrssi_sum = btc->btrssi_sum - old_rssi + cur_rssi;
		ASSERT(btc->btrssi_cnt);
		btrssi_avg = btc->btrssi_sum / btc->btrssi_cnt;

		btc->btrssi_idx = MODINC_POW2(btc->btrssi_idx, BTC_BTRSSI_SIZE);
	}

	if (btc->btrssi_cnt < BTC_BTRSSI_MIN_NUM)
		return BT_RSSI_INVALID;

	return (int8)btrssi_avg;
}

wlc_btc_info_t *
BCMATTACHFN(wlc_btc_attach)(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc;
#if defined(WLC_LOW) || defined(WL_BTCDYN)
	wlc_hw_info_t *wlc_hw = wlc->hw;
#endif // endif

	if ((btc = (wlc_btc_info_t*)
		MALLOCZ(wlc->osh, sizeof(wlc_btc_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	btc->wlc = wlc;

#ifdef	WLAIBSS
	/* reserve cubby in the scb container to monitor per SCB BT info */
	if ((btc->scb_handle = wlc_scb_cubby_reserve(wlc,
		sizeof(btcx_oxygen_info_t *), wlc_btcx_scb_init, wlc_btcx_scb_deinit,
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
		NULL, btc, 0)) < 0) {
#else
		NULL, btc)) < 0) {
#endif // endif
		WL_ERROR(("wl%d: %s: btc scb cubby err\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_btcx_bss_updn, btc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bss updown err\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WLAIBSS */

	/* register module */
	if (wlc_module_register(wlc->pub, btc_iovars, "btc", btc, wlc_btc_doiovar,
		wlc_btcx_watchdog, wlc_btc_wlc_up, wlc_btc_wlc_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: btc register err\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef	WLAIBSS
	/* IE mgmt callbacks for AIBSS */
	/* bcn */
	if (wlc_iem_vs_add_build_fn(wlc->iemi, FC_BEACON, WLC_IEM_VS_IE_PRIO_BRCM_BTCX,
	                            wlc_btcx_aibss_ie_len, wlc_btcx_aibss_write_ie,
	                            btc) != BCME_OK) {
		WL_ERROR(("wl%d: %s btc ie add err\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_iem_vs_add_parse_fn(wlc->iemi, FC_BEACON, WLC_IEM_VS_IE_PRIO_BRCM_BTCX,
	                            wlc_btcx_aibss_parse_ie, btc) != BCME_OK) {
		WL_ERROR(("wl%d: %s btc parse err\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WLAIBSS */

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
	/* register dump stats for btcx */
	wlc_dump_register(wlc->pub, "btcx", (dump_fn_t)wlc_dump_btcx, (void *)wlc);
#endif /* BCMDBG || BCMDBG_DUMP */

#ifdef WL_BTCDYN
	if ((btc->dprof = MALLOCZ(wlc->osh, sizeof(dctl_prof_t))) != NULL) {

		/*  default desnense & mode switching tables (2 rows each) */
		btc_thr_data_t dflt_msw_data[2] = {
			{1, 12, -73 -30, -90},
			{1, 8, -73, -30, -60},
		};
		/*  default desense datatable  */
		btc_thr_data_t dflt_dsns_data[2] = {
			{5, 4, 0, -55, -65},
			{5, -16, 0, -50, -65}
		};

		/* defult tx power control table for hybrid coex */
		int8 dflt_wlpwr_thresh[DYN_PWR_DFLT_STEPS-1] = {-30, -40, -50, -60, -70};
		int8 dflt_wlpwr_val[DYN_PWR_DFLT_STEPS] = {3, 4, 6, 8, 10, 12};

		/* allocate memory for tx power control */
		btc->wlpwr_val = MALLOCZ(wlc->osh, sizeof(int8)*DYN_PWR_MAX_STEPS);
		btc->wlpwr_thresh = MALLOCZ(wlc->osh, sizeof(int8)*(DYN_PWR_MAX_STEPS-1));
		if (btc->wlpwr_val == NULL || btc->wlpwr_thresh == NULL) {
			WL_ERROR(("wl%d: %s: MALLOC for wlpwr failed, %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}

		/* dynctl profile struct ver */
		btc->dprof->version = DCTL_PROFILE_VER;
		/* default WL desense & mode switch handlers  */
		btc->desense_fn = btcx_dflt_get_desense_level;
		btc->mswitch_fn = btcx_dflt_mode_switch;
		btc->bt_rssi = BT_RSSI_INVALID;
		btc->dprof->msw_btrssi_hyster = BTCDYN_DFLT_BTRSSI_HYSTER;

		/*
		 * try loading btcdyn profile from nvram,
		 * use "btcdyn_flags" var as a presense indication
		 */
		if (getvar(wlc_hw->vars, rstr_btcdyn_flags) != NULL) {

			uint16 i;

			/* read int params 1st */
			btc->dprof->flags = getintvar(wlc_hw->vars, rstr_btcdyn_flags);
			btc->dprof->dflt_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_dflt_dsns_level);
			btc->dprof->low_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_low_dsns_level);
			btc->dprof->mid_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_mid_dsns_level);
			btc->dprof->high_dsns_level =
				getintvar(wlc_hw->vars, rstr_btcdyn_high_dsns_level);
			btc->dprof->default_btc_mode =
				getintvar(wlc_hw->vars, rstr_btcdyn_default_btc_mode);
			btc->dprof->msw_btrssi_hyster =
				getintvar(wlc_hw->vars, rstr_btcdyn_btrssi_hyster);

			/* if (1) the board is using two antenna btcoex */
			btc->coex_two_ant =
				getintvar(wlc_hw->vars, rstr_btcdyn_coex_two_ant);

			/* params for dynamic tx power control */
			btc->wlpwr_steps =
				getintvararraysize(wlc_hw->vars, rstr_btcdyn_wlpwr_val);
			if (btc->wlpwr_steps > DYN_PWR_MAX_STEPS) {
				btc->wlpwr_steps = 0;
			}
			if (btc->wlpwr_steps > 0 &&
				((btc->wlpwr_steps - 1) !=
				getintvararraysize(wlc_hw->vars, rstr_btcdyn_wlpwr_thresh))) {
				btc->wlpwr_steps = 0;
			}

			if (btc->wlpwr_steps == 0) {
				/* use default tx power table for hybrid coex */
				btc->wlpwr_steps = DYN_PWR_DFLT_STEPS;
				bcopy(dflt_wlpwr_thresh, btc->wlpwr_thresh,
					sizeof(dflt_wlpwr_thresh));
				bcopy(dflt_wlpwr_val, btc->wlpwr_val,
					sizeof(dflt_wlpwr_val));
			} else {
				/* use nvram settings */
				int i;
				btc->wlpwr_val[0] =
				getintvararray(wlc_hw->vars, rstr_btcdyn_wlpwr_val, 0);
				for (i = 1; i < btc->wlpwr_steps; i++) {
					btc->wlpwr_val[i] =
					getintvararray(wlc_hw->vars, rstr_btcdyn_wlpwr_val, i);
					btc->wlpwr_thresh[i-1] =
					getintvararray(wlc_hw->vars, rstr_btcdyn_wlpwr_thresh, i-1);
				}
			}

			/* these two are used for data array sz check */
			btc->dprof->msw_rows =
				getintvar(wlc_hw->vars, rstr_btcdyn_msw_rows);
			btc->dprof->dsns_rows =
				getintvar(wlc_hw->vars, rstr_btcdyn_dsns_rows);

			/* sanity check on btcdyn nvram table sz */
			if ((btc->dprof->msw_rows > DCTL_TROWS_MAX) ||
				(((btc->dprof->flags & DCTL_FLAGS_MSWITCH) == 0) !=
				(btc->dprof->msw_rows == 0))) {
				BTCDBG(("btcdyn invalid mode switch config\n"));
				goto rst2_dflt;
			}
			if ((btc->dprof->dsns_rows > DCTL_TROWS_MAX) ||
				(((btc->dprof->flags & DCTL_FLAGS_DESENSE) == 0) !=
				(btc->dprof->dsns_rows == 0))) {
				BTCDBG(("btcdyn invalid dynamic desense config\n"));
				goto rst2_dflt;
			}

			/*  initialize up to 4 rows in msw table */
			i = wlc_btc_dynctl_init_trow(btc, &btc->dprof->msw_data[0],
				rstr_btcdyn_msw_row0, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &btc->dprof->msw_data[1],
				rstr_btcdyn_msw_row1, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &btc->dprof->msw_data[2],
				rstr_btcdyn_msw_row2, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &btc->dprof->msw_data[3],
				rstr_btcdyn_msw_row3, sizeof(btc_thr_data_t));

			/* number of initialized table rows must match to specified in nvram */
			if (i != btc->dprof->msw_rows) {
				BTCDBG(("btcdyn incorrect nr of mode switch rows (%d)\n", i));
				goto rst2_dflt;
			}

			/*  initialize up to 4 rows in desense sw table */
			i = wlc_btc_dynctl_init_trow(btc, &btc->dprof->dsns_data[0],
				rstr_btcdyn_dsns_row0, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &btc->dprof->dsns_data[1],
				rstr_btcdyn_dsns_row1, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &btc->dprof->dsns_data[2],
				rstr_btcdyn_dsns_row2, sizeof(btc_thr_data_t));
			i += wlc_btc_dynctl_init_trow(btc, &btc->dprof->dsns_data[3],
				rstr_btcdyn_dsns_row3, sizeof(btc_thr_data_t));

			/* number of initialized table rows must match to specified in nvram */
			if (i != btc->dprof->dsns_rows) {
				BTCDBG(("btcdyn incorrect nr of dynamic desense rows (%d)\n", i));
				goto rst2_dflt;
			}

			BTCDBG(("btcdyn profile has been loaded from nvram - Ok\n"));
		} else {
			rst2_dflt:
			WL_ERROR(("wl%d: %s: nvram.txt: missing or bad btcdyn profile vars."
				" do init from default\n", wlc->pub->unit, __FUNCTION__));

			/* enable dyn desense, mode switching and tx pwr control */
			btc->dprof->flags = (DCTL_FLAGS_DYNCTL |
				DCTL_FLAGS_DESENSE |
				DCTL_FLAGS_MSWITCH |
				DCTL_FLAGS_PWRCTRL);
			/* initialize default profile */
			btc->dprof->dflt_dsns_level = DESENSE_OFF;
			btc->dprof->low_dsns_level = DESENSE_OFF;
			btc->dprof->mid_dsns_level = DFLT_DESENSE_MID;
			btc->dprof->high_dsns_level = DFLT_DESENSE_HIGH;
			btc->dprof->default_btc_mode = WL_BTC_HYBRID;
			btc->dprof->msw_rows = DCTL_TROWS;
			btc->dprof->dsns_rows = DCTL_TROWS;
			btc->sim_btpwr = BT_INVALID_TX_PWR;
			btc->sim_wlrssi = WLC_RSSI_INVALID;

			/*  sanity check for the table sizes */
			ASSERT(sizeof(dflt_msw_data) <=
				(DCTL_TROWS_MAX * sizeof(btc_thr_data_t)));
			bcopy(dflt_msw_data, btc->dprof->msw_data, sizeof(dflt_msw_data));
			ASSERT(sizeof(dflt_dsns_data) <=
				(DCTL_TROWS_MAX * sizeof(btc_thr_data_t)));
			bcopy(dflt_dsns_data,
				btc->dprof->dsns_data, sizeof(dflt_dsns_data));

			/* default tx power table for hybrid coex */
			btc->wlpwr_steps = DYN_PWR_DFLT_STEPS;
			bcopy(dflt_wlpwr_thresh, btc->wlpwr_thresh, sizeof(dflt_wlpwr_thresh));
			bcopy(dflt_wlpwr_val, btc->wlpwr_val, sizeof(dflt_wlpwr_val));
		}
		/* set btc_mode to default value */
		wlc_hw->btc->mode = btc->dprof->default_btc_mode;
	} else {
		WL_ERROR(("wl%d: %s: MALLOC for btc->dprof failed, %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
	}
#endif /* WL_BTCDYN */

	wlc_btc_reset_btrssi(btc);

#ifdef WLC_LOW /* avoid WIN32 NIC driver compile error */
	if (getvar(wlc_hw->vars, rstr_prot_btrssi_thresh) != NULL) {
		btc->prot_btrssi_thresh =
			(int8)getintvar(wlc_hw->vars, rstr_prot_btrssi_thresh);
	} else {
		btc->prot_btrssi_thresh = BTC_BTRSSI_THRESH;
	}
#else
	btc->prot_btrssi_thresh = BTC_BTRSSI_THRESH;
#endif // endif

	btc->btrssi_hyst = BTC_BTRSSI_HYST;
	btc->wlrssi_hyst = BTC_WLRSSI_HYST;
	btc->simrx_slna = FALSE;
	btc->dyagg = AUTO;

#ifdef WLC_LOW /* avoid WIN32 NIC driver compile error */
	btc->siso_ack =	getintvar(wlc_hw->vars, rstr_btc_siso_ack);
#endif // endif

	return btc;
	/* error handling */
fail:
	wlc_btc_detach(btc);
	return NULL;
}

/* BTCX Wl up callback */
static int
wlc_btc_wlc_up(void *ctx)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_info_t *wlc = btc->wlc;

	if (!btc->bt_shm_addr)
		btc->bt_shm_addr = 2 * wlc_bmac_read_shm(wlc->hw, M_BTCX_BLK_PTR);

	if (AIBSS_ENAB(wlc->pub)) {
		/* Enable dynamic Tx aggregation in AIBSS mode */
		wlc_btc_hflg(wlc, ON, BTCX_HFLG_DYAGG);
	}
	return BCME_OK;
}

/* BTCX Wl down callback */
static int
wlc_btc_wlc_down(void *ctx)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_info_t *wlc = btc->wlc;

	btc->bt_shm_addr = 0;
	if (AIBSS_ENAB(wlc->pub)) {
		/* Disable dynamic Tx aggregation if AIBSS goes down */
		wlc_btc_hflg(wlc, OFF, BTCX_HFLG_DYAGG);
	}
	return BCME_OK;
}

#ifdef	WLAIBSS
/* bsscfg up/down */
static void
wlc_btcx_bss_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_bsscfg_t *cfg = (evt ? evt->bsscfg : NULL);

	BCM_REFERENCE(btc);
	BCM_REFERENCE(cfg);
	if (!AIBSS_ENAB(btc->wlc->pub) || (!cfg) || !BSSCFG_IBSS(cfg))
		return;
	if (btc->wlc->ucode_rev < BTCX_OXYGEN_SUPPORT_UCODE_REV) {
		WL_ERROR(("Oxygen Not supported in Ucode Ver: 0x%x\n", btc->wlc->ucode_rev));
		return;
	}
	/* Enable/Disable BT Strobe */
	wlc_btcx_strobe_enable(btc, evt->up);
}
/* scb cubby */
static int
wlc_btcx_scb_init(void *ctx, struct scb *scb)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	btcx_oxygen_info_t **pbt_info = (btcx_oxygen_info_t **)SCB_CUBBY(scb, btc->scb_handle);
	btcx_oxygen_info_t *bt_info;

	if (SCB_INTERNAL(scb) || !AIBSS_ENAB(btc->wlc->pub)) {
		return BCME_OK;
	}

	if ((bt_info = MALLOCZ(btc->wlc->osh, sizeof(btcx_oxygen_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: no mem\n", btc->wlc->pub->unit, __FUNCTION__));
		return BCME_NOMEM;
	}
	*pbt_info = bt_info;

	return BCME_OK;
}

static void
wlc_btcx_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	btcx_oxygen_info_t **pbt_info = (btcx_oxygen_info_t **)SCB_CUBBY(scb, btc->scb_handle);
	btcx_oxygen_info_t *bt_info = *pbt_info;

	if (bt_info) {
		MFREE(btc->wlc->osh, bt_info, sizeof(btcx_oxygen_info_t));
		*pbt_info = NULL;
	}
}

/* Return the length of the BTCX IE in beacon */
static uint
wlc_btcx_aibss_ie_len(void *ctx, wlc_iem_calc_data_t *calc)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;

	/* To avoid compiler warning when AIBSS_ENAB is '0' */
	BCM_REFERENCE(btc);

	if (AIBSS_ENAB(btc->wlc->pub)) {
		return (sizeof(btc_brcm_prop_ie_t));
	} else {
		return (0);
	}
}

/*
The following function builds the BTCX IE. This gets called when
beacon is being built
*/
static int
wlc_btcx_aibss_write_ie(void *ctx, wlc_iem_build_data_t *build)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	btc_brcm_prop_ie_t *btc_brcm_prop_ie = (btc_brcm_prop_ie_t *)build->buf;

	if (((build->ft != FC_BEACON) || !AIBSS_ENAB(btc->wlc->pub)) ||
		(build->buf_len < sizeof(btc_brcm_prop_ie_t))) {
		return 0;
	}

	btc_brcm_prop_ie->id = DOT11_MNG_PROPR_ID;
	btc_brcm_prop_ie->len = BRCM_BTC_INFO_TYPE_LEN;
	bcopy(BRCM_PROP_OUI, &btc_brcm_prop_ie->oui[0], DOT11_OUI_LEN);
	btc_brcm_prop_ie->type = BTC_INFO_BRCM_PROP_IE_TYPE;

	/* Read out the latest BT state */
	wlc_btcx_aibss_get_local_bt_state(btc);

	/* Copy the BT status */
	btc_brcm_prop_ie->info = btc->local_btinfo;
	return BCME_OK;
}

/*
This function gets called during beacon parse when the BTCX IE is
present in the beacon
*/
static int
wlc_btcx_aibss_parse_ie(void *ctx, wlc_iem_parse_data_t *parse)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;

	if (!btc) {
		return BCME_ERROR;
	}

	if ((parse->ie == NULL) || !BSSCFG_IBSS(parse->cfg) || !AIBSS_ENAB(btc->wlc->pub)) {
		return BCME_OK;
	}

	switch (parse->ft) {
	case FC_BEACON:
		/* Beacon contains the Oxygen IE */
		if (parse->pparm->ft->bcn.scb != NULL) {
			btcx_oxygen_info_t **pbt_info =
				SCB_CUBBY((parse->pparm->ft->bcn.scb), (btc->scb_handle));
			btcx_oxygen_info_t *bt_info = *pbt_info;

			btc_brcm_prop_ie_t *btc_brcm_prop_ie =
				(btc_brcm_prop_ie_t *)parse->ie;

#if defined(BCMDBG)
			if (bt_info->btinfo != btc_brcm_prop_ie->info) {
				char eabuf[32];

				bcm_ether_ntoa(&parse->pparm->ft->bcn.scb->ea, eabuf);
				WL_INFORM(("wl%d: Bcn: eth: %s BT Info chg: 0x%x 0x%x\n",
					btc->wlc->pub->unit, eabuf, bt_info->btinfo,
					btc_brcm_prop_ie->info));
			}
#endif // endif
			bt_info->btinfo = btc_brcm_prop_ie->info;
			bt_info->bcn_rx_cnt++;
			wlc_btcx_aibss_check_state(btc);
		}

		break;
	}

	return BCME_OK;
}

/* Check BT status in all devices in the IBSS and decide on the protection scheme to use */
static void
wlc_btcx_aibss_check_state(wlc_btc_info_t *btc)
{
	bool prot_on = FALSE, cmn_cts2self = FALSE;

	/* AIBSS_ENAB is checked by the caller */
	/* Update the local BT state */
	wlc_btcx_aibss_get_local_bt_state(btc);
	/* Check BT sync state in all the other devices in this IBSS */
	wlc_btc_aibss_get_state(btc, CHECK_AIBSS_BT_SYNC_STATE);
	/* Update aggregation state */
	wlc_btcx_aibss_update_agg_state(btc);

	/*
	If BT info in local device (currently only SCO or eSCO indication and BT sync status)
	has changed, update beacon
	*/
	if (btc->local_btinfo != btc->last_btinfo) {
		int i;
		wlc_bsscfg_t *cfg;

#if defined(BCMDBG)
		WL_ERROR(("wl%d: BTCX: Local BT Info chg: 0x%x to 0x%x prd: %d sync: %d\n",
			btc->wlc->pub->unit, btc->last_btinfo, btc->local_btinfo,
			btc->bth_period, btc->local_bt_in_sync));
#endif // endif
		/* Update Beacon since BT status has changed */
		FOREACH_BSS(btc->wlc, i, cfg) {
			if (cfg->associated && BSSCFG_IBSS(cfg)) {
				wlc_bss_update_beacon(btc->wlc, cfg);
			}
		}
		btc->last_btinfo = btc->local_btinfo;
	}

	if (!(btc->local_btinfo & BT_INFO_TASK_TYPE_ESCO)) {
		/* Local device does not have eSCO on */
		if (btc->other_esco_present && btc->other_bt_in_sync) {
			/* At least one other device in the IBSS has eSCO and
			   clock is synced at all devices
			*/
			prot_on = cmn_cts2self = TRUE;
		}
		/* else default values */
	} else {
		/* Local device has eSCO */
		/* prot_on is the default value */
		cmn_cts2self = btc->other_bt_in_sync;
	}
	wlc_btcx_aibss_set_prot(btc, prot_on, cmn_cts2self);
}

/* Use a low pass filter to weed out any spurious reads of BT sync state */
static void
wlc_btcx_aibss_read_bt_sync_state(wlc_btc_info_t *btc)
{
	uint16 eci_m;
	uint8 tmp;

#define BTCX_OUT_OF_SYNC_CNT 5
	/* First check if the local device is the master. If not, sync state
	   state does not apply
	*/
	eci_m = wlc_bmac_read_eci_data_reg(btc->wlc->hw, 2);

	if ((!AIBSS_ENAB(btc->wlc->pub)) || (eci_m & BT_IS_SLAVE)) {
		btc->local_bt_in_sync = FALSE;
		btc->local_bt_is_master = FALSE;
		return;
	}

	btc->local_bt_is_master = TRUE;
	eci_m = wlc_bmac_read_eci_data_reg(btc->wlc->hw, 0);
	tmp = ((eci_m & BT_CLK_SYNC_STS) >> BT_CLK_SYNC_STS_SHIFT_BITS) & 0x1;
	if (!tmp) {
		btc->bt_out_of_sync_cnt++;
		if (btc->bt_out_of_sync_cnt <= BTCX_OUT_OF_SYNC_CNT) {
			tmp = btc->local_bt_in_sync;
		} else {
			btc->bt_out_of_sync_cnt = BTCX_OUT_OF_SYNC_CNT;
		}
	}
	else
		btc->bt_out_of_sync_cnt = 0;
	btc->local_bt_in_sync = tmp;
}

/* This function enables/disables BT strobe */
void
wlc_btcx_strobe_enable(wlc_btc_info_t *btc, bool on)
{
	if (BT3P_HW_COEX(btc->wlc) && BAND_2G(btc->wlc->band->bandtype)) {
		btc->strobe_enabled = on;
	} else {
		btc->strobe_enabled = FALSE;
	}
}

/* This function calculates the mod of the current TSF with the input factor */
static void
wlc_btcx_aibss_calc_tsf(wlc_btc_info_t *btc, uint32 *new_tsf_l,
	uint32 mod_factor, uint16 add_delta)
{
	uint32 tsf_l, tsf_h;
	uint32 new_tsf_h;
	uint32 temp;

	/* Read current TSF */
	wlc_read_tsf(btc->wlc, &tsf_l, &tsf_h);
	/* Divide by factor */
	bcm_uint64_divide(&temp, tsf_h, tsf_l, mod_factor);
	/* Multiply by factor and add factor * 2 */
	bcm_uint64_multiple_add(&new_tsf_h, new_tsf_l, temp, mod_factor, mod_factor * 2);
	/* Add delta */
	wlc_uint64_add(&new_tsf_h, new_tsf_l, 0, add_delta);
}

/*
This function enables/disables Oxygen protection (if local device does not have
eSCO or if BT clock is not synchronized in one or more devices in the IBSS)
*/
static void
wlc_btcx_aibss_set_prot(wlc_btc_info_t *btc, bool prot_on, bool cmn_cts2self)
{
	if (AIBSS_ENAB(btc->wlc->pub) && BT3P_HW_COEX(btc->wlc) &&
		BAND_2G(btc->wlc->band->bandtype) && btc->strobe_on) {
		uint16 btcx_config;

		btcx_config = wlc_read_shm(btc->wlc, btc->bt_shm_addr + M_BTCX_CONFIG);
		if (!btc->sco_prot_on && prot_on) {
			uint32 new_tsf_l;

			/* Read current TSF, divide and multiply by BT_ESCO_PERIOD
			and add 2 * BT_ESCO_PERIOD (15000).
			Setup the time for pretend eSCO protection:
			   Add 6175 (6175 ms = BT_ESCO_PERIOD (7500) - 750(to send CTS2SELF) -
			   625 (eSCO Tx slot)
			*/
			wlc_btcx_aibss_calc_tsf(btc, &new_tsf_l, BT_ESCO_PERIOD, 6175);

			/* Write out the eSCO protection start time in SHM */
			wlc_write_shm(btc->wlc, M_BTCX_IBSS_TSF_ESCO_L, (new_tsf_l & 0xFFFF));
			btcx_config |= C_BTCX_CONFIG_SCO_PROT;
			btc->sco_prot_on = TRUE;
		} else if (btc->sco_prot_on && !prot_on) {
			btcx_config &= ~C_BTCX_CONFIG_SCO_PROT;
			btc->sco_prot_on = FALSE;
		}
		if (cmn_cts2self)
			btcx_config |= C_BTCX_CFG_CMN_CTS2SELF;
		else
			btcx_config &= ~C_BTCX_CFG_CMN_CTS2SELF;
		wlc_write_shm(btc->wlc, btc->bt_shm_addr + M_BTCX_CONFIG, btcx_config);
	}
}

/* Enable/disable strobing to BT */
static void
wlc_btcx_aibss_set_strobe(wlc_btc_info_t *btc, int strobe_on)
{
	if (AIBSS_ENAB(btc->wlc->pub) && BT3P_HW_COEX(btc->wlc) &&
		BAND_2G(btc->wlc->band->bandtype)) {
		uint32 new_tsf_l = 0;
		uint16 btcx_config;
		uint32 strobe_val = 0;
		uint32 secimcr = 0x89;

		btcx_config = wlc_read_shm(btc->wlc, btc->bt_shm_addr + M_BTCX_CONFIG);
		if (!btc->strobe_on && strobe_on) {

			/* Enable WCI-2 interface for strobing */
			if (!si_btcx_wci2_init(btc->wlc->hw->sih))
				return;
			/* Read current TSF, divide by BT_STROBE_INTERVAL,
			multiply by BT_STROBE_INTERVAL and add (2 * BT_STROBE_INTERVAL)
			*/
			wlc_btcx_aibss_calc_tsf(btc, &new_tsf_l, BT_STROBE_INTERVAL, 0);
#if defined(BCMDBG)
			WL_INFORM(("wl%d: STROBE: mul: %d old 0x%x 0x%x new 0x%x 0x%x\n",
				btc->wlc->pub->unit, temp, tsf_h, tsf_l, new_tsf_h, new_tsf_l));
#endif // endif

			/* Read out current TSF to check for any TSF jumps in the watchdog
			   routine
			*/
			wlc_read_tsf(btc->wlc, &btc->prev_tsf_l, &btc->prev_tsf_h);

			/* Enable ucode */
			btcx_config |= C_BTCX_CONFIG_BT_STROBE;

			strobe_val = GCI_WL_STROBE_BIT_MASK;
			secimcr = 0x99;
		} else if (btc->strobe_on && !strobe_on) {
			/* Disable WCI-2 interface */
			si_gci_reset(btc->wlc->hw->sih);
#if defined(BCMDBG)
			WL_INFORM(("wl%d: BTCX STROBE: Off\n", btc->wlc->pub->unit));
#endif // endif

			/* Disable strobing in ucode */
			btcx_config &= ~(C_BTCX_CONFIG_BT_STROBE | C_BTCX_CONFIG_SCO_PROT);
			btc->sco_prot_on = FALSE;
		} else
			return;
		/* Write out the starting strobe TS into SHM */
		wlc_write_shm(btc->wlc, M_BTCX_IBSS_TSF_L, (new_tsf_l & 0xFFFF));
		wlc_write_shm(btc->wlc, M_BTCX_IBSS_TSF_ML, (new_tsf_l >> 16) & 0xFFFF);

		btc->strobe_on = strobe_on;

		wlc_write_shm(btc->wlc, btc->bt_shm_addr + M_BTCX_CONFIG, btcx_config);

		/* Indicate to BT that strobe is on/off */
		si_gci_direct(btc->wlc->pub->sih, OFFSETOF(chipcregs_t, gci_control_1),
			GCI_WL_STROBE_BIT_MASK, strobe_val);
		si_gci_direct(btc->wlc->pub->sih, OFFSETOF(chipcregs_t, gci_output[1]),
			GCI_WL_STROBE_BIT_MASK, strobe_val);

		/* Baud rate: loop back mode on/off */
		si_gci_direct(btc->wlc->pub->sih, OFFSETOF(chipcregs_t, gci_secimcr),
			0xFF, secimcr);
	}
}

/*
This function checks to see if the TSF has jumped and if so stop and restart strobe.
To restart a flag is set so that any pending protection state completes before the
strobe is started again. This function also checks BT sync state and if not synced
grants ACL requests to speed up the synchronization process.
*/

static void
wlc_btcx_aibss_chk_clk_sync(wlc_btc_info_t *btc)
{
	uint16 pri_map_lo;

	pri_map_lo = wlc_read_shm(btc->wlc, btc->bt_shm_addr + M_BTCX_PRI_MAP_LO);

	/* Check to see if TSF has jumped (because of sync to the network TSF). If it
	   has jumped, disable and enable strobing
	*/
	if (btc->strobe_enabled) {
		if (btc->strobe_on) {
			uint32 tsf_l, tsf_h;

			if ((btc->prev_tsf_l == 0) && (btc->prev_tsf_h == 0))
				wlc_read_tsf(btc->wlc, &btc->prev_tsf_l, &btc->prev_tsf_h);

			wlc_read_tsf(btc->wlc, &tsf_l, &tsf_h);

			if (wlc_uint64_lt(btc->prev_tsf_h, btc->prev_tsf_l, tsf_h,
				tsf_l)) {
				uint32 temp_tsf_l = tsf_l, temp_tsf_h = tsf_h;

				wlc_uint64_sub(&temp_tsf_h, &temp_tsf_l, btc->prev_tsf_h,
					btc->prev_tsf_l);
				if (temp_tsf_h || (temp_tsf_l > 1100000)) {
					wlc_btcx_aibss_set_strobe(btc, FALSE);
				}
			}
			btc->prev_tsf_l = tsf_l;
			btc->prev_tsf_h = tsf_h;
		} else {
			wlc_btcx_aibss_set_strobe(btc, TRUE);
		}
		/* Check to see if BT clock is synchronized (only if eSCO is on) */
		if ((btc->strobe_on) && (btc->local_btinfo & BT_INFO_TASK_TYPE_ESCO)) {

			/* Sync state applies only if the device is master */
			if (btc->local_bt_is_master) {

				if (!(btc->local_btinfo & BT_INFO_BT_CLK_SYNCED)) {
					/* If not synchronized, grant ACL requests to speed up
					   synchronization (as denied requests prevent master
					   from clock dragging)
					*/
					if (!(pri_map_lo & 0x2)) {
						pri_map_lo |= 0x2;
						wlc_write_shm(btc->wlc, btc->bt_shm_addr +
							M_BTCX_PRI_MAP_LO, pri_map_lo);
						btc->acl_grant_set = TRUE;
#if defined(BCMDBG)
						WL_INFORM(("wl%d: BT Not in Sync\n",
							btc->wlc->pub->unit));
#endif // endif
					}
				} else {
					if (btc->acl_grant_set) {
						pri_map_lo &= ~0x2;
						wlc_write_shm(btc->wlc, btc->bt_shm_addr +
							M_BTCX_PRI_MAP_LO, pri_map_lo);
						btc->acl_grant_set = FALSE;
#if defined(BCMDBG)
						WL_INFORM(("wl%d: BT in Sync\n",
							btc->wlc->pub->unit));
#endif // endif
					}
				}
				return;
			}
		}
	} else {
		wlc_btcx_aibss_set_strobe(btc, FALSE);
	}

	/* If Strobe is off or eSCO is off, disable acl grants if enabled */
	if (btc->acl_grant_set) {
		pri_map_lo &= ~0x2;
		wlc_write_shm(btc->wlc, btc->bt_shm_addr +
			M_BTCX_PRI_MAP_LO, pri_map_lo);
		btc->acl_grant_set = FALSE;
	}
}

/* Check AIBSS state (by traversing through the list of all devices) as per input
request. Currently supports 2 actions:
- Check for BT sync status
- Check for stale beacons
*/
static void
wlc_btc_aibss_get_state(wlc_btc_info_t *btc, uint8 action)
{
	/* Check BT status in all the other devices in this IBSS */
	if (AIBSS_ENAB(btc->wlc->pub)) {
		int i;
		wlc_bsscfg_t *cfg;
		wlc_info_t *wlc = btc->wlc;

		btc->other_bt_in_sync = TRUE;
		wlc->btch->other_esco_present =  FALSE;
		FOREACH_AS_STA(wlc, i, cfg) {
			if (cfg->associated && BSSCFG_IBSS(cfg)) {
				struct scb_iter scbiter;
				struct scb *scb;

				FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
					btcx_oxygen_info_t **pbt_info =
						SCB_CUBBY((scb), (wlc->btch->scb_handle));
					btcx_oxygen_info_t *bt_info = *pbt_info;

					if (action & CHECK_AIBSS_STALE_BEACON) {
						if (!bt_info->bcn_rx_cnt) {
							/* No beacons received since the last check,
							   clear out the BT info.
							*/
							bt_info->btinfo = 0;
						}
						bt_info->bcn_rx_cnt = 0;
					}
					if (action & CHECK_AIBSS_BT_SYNC_STATE) {
						if (bt_info->btinfo & BT_INFO_TASK_TYPE_ESCO) {
							/* Atleast one device with BT eSCO on */
							wlc->btch->other_esco_present =  TRUE;
							if (!(bt_info->btinfo &
								BT_INFO_BT_CLK_SYNCED)) {
								/* Atleast one BT is not synced */
								btc->other_bt_in_sync = FALSE;
							}
						}
					}
				}
			}
		}
	}
}

/* Get the latest local BT state */
static void
wlc_btcx_aibss_get_local_bt_state(wlc_btc_info_t *btc)
{
	if (AIBSS_ENAB(btc->wlc->pub)) {
		wlc_btcx_read_btc_params(btc->wlc);
		wlc_btcx_aibss_read_bt_sync_state(btc);

		btc->local_btinfo = 0;
		if (btc->bth_period >= BT_ESCO_PERIOD) {
			btc->local_btinfo = BT_INFO_TASK_TYPE_ESCO;
			if (btc->local_bt_in_sync)
				btc->local_btinfo |= BT_INFO_BT_CLK_SYNCED;
		}
	}
}

/*
Update Rx aggregation state based on if eSCO is enabled/disabled in the
entire AIBSS.
*/
static void
wlc_btcx_aibss_update_agg_state(wlc_btc_info_t *btc)
{
#if defined(BCMDBG)
	bool prev_rx_agg = btc->rx_agg_change;
#endif // endif

	if (AIBSS_ENAB(btc->wlc->pub)) {
		if ((!btc->other_esco_present) && !(btc->local_btinfo & BT_INFO_TASK_TYPE_ESCO)) {
			btc->esco_off_cnt++;
			if (btc->esco_off_cnt >= BT_ESCO_OFF_DELAY_CNT) {
				btc->esco_off_cnt = BT_ESCO_OFF_DELAY_CNT;
				btc->rx_agg_change = FALSE;
#if defined(BCMDBG)
				if (prev_rx_agg != btc->rx_agg_change) {
					WL_ERROR(("BTCX: Rx Agg revert cnt: %d esco: %d\n",
						btc->esco_off_cnt, btc->other_esco_present));
				}
#endif // endif
				return;
			}
		} else {
			btc->esco_off_cnt = 0;
		}
		btc->rx_agg_change = TRUE;
#if defined(BCMDBG)
		if (prev_rx_agg != btc->rx_agg_change) {
			WL_ERROR(("BTCX: Rx Agg chg cnt: %d esco: %d\n",
				btc->esco_off_cnt, btc->other_esco_present));
		}
#endif // endif
	} else {
		btc->esco_off_cnt = BT_ESCO_OFF_DELAY_CNT;
		btc->rx_agg_change = FALSE;
	}
}

/* Update Rx aggregation size if needed as follows:
	- Turn off Rx agg
	- Turn it back on
	- AMPDU Rx will call wlc_btcx_get_ba_rx_wsize (from wlc_ampdu_recv_addba_req_resp())
		to get the BTCX size
*/
static void
wlc_btcx_aibss_update_agg_size(wlc_btc_info_t *btc)
{
	if (AIBSS_ENAB(btc->wlc->pub)) {
		if (btc->rx_agg_change && !btc->rx_agg_modified) {
			/* Agg change needed */
			btc->rx_agg_modified = TRUE;
		} else if (!btc->rx_agg_change && btc->rx_agg_modified) {
			/* Agg change needs to be reverted */
			btc->rx_agg_modified = FALSE;
		} else
			return;
	} else {
		return;
	}

	/* If agg size needs to be modified or reverted, turn it off and back on */
	wlc_ampdu_agg_state_upd_ovrd_band(btc->wlc, OFF,
		WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
	wlc_ampdu_agg_state_upd_ovrd_band(btc->wlc, ON,
		WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
}
#endif /* WLAIBSS */

/*
This function is called by AMPDU Rx to get modified Rx aggregation size.
*/
uint8 wlc_btcx_get_ba_rx_wsize(wlc_info_t *wlc)
{
#ifdef WLAIBSS
	if ((AIBSS_ENAB(wlc->pub)) && wlc->btch->rx_agg_change)
		return (BT_AIBSS_RX_AGG_SIZE);
	else
#endif // endif
		return (0);
}

void
BCMATTACHFN(wlc_btc_detach)(wlc_btc_info_t *btc)
{
	wlc_info_t *wlc;

	if (btc == NULL)
		return;

	wlc = btc->wlc;
	wlc_module_unregister(wlc->pub, "btc", btc);

#ifdef WL_BTCDYN
	if (btc->dprof) {
		MFREE(wlc->osh, btc->dprof, sizeof(dctl_prof_t));
		btc->dprof = NULL;
	}
	if (btc->wlpwr_val) {
		MFREE(wlc->osh, btc->wlpwr_val, sizeof(int8)*DYN_PWR_MAX_STEPS);
		btc->wlpwr_val = NULL;
	}
	if (btc->wlpwr_thresh) {
		MFREE(wlc->osh, btc->wlpwr_thresh, sizeof(int8)*(DYN_PWR_MAX_STEPS-1));
		btc->wlpwr_thresh = NULL;
	}

#endif /* WL_BTCDYN */
	MFREE(wlc->osh, btc, sizeof(wlc_btc_info_t));
}

static int
wlc_btc_mode_set(wlc_info_t *wlc, int int_val)
{
	int err = wlc_bmac_btc_mode_set(wlc->hw, int_val);
	wlc->btch->mode = wlc_bmac_btc_mode_get(wlc->hw);

	return err;
}

int
wlc_btc_mode_get(wlc_info_t *wlc)
{
	return wlc->btch->mode;
}

int
wlc_btcx_desense(wlc_btc_info_t *btc, int band)
{
	int i;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int btc_mode = wlc_btc_mode_get(wlc);

	/* Dynamic restaging of rxgain for BTCoex */
	if (!SCAN_IN_PROGRESS(wlc->scan) &&
	    btc->bth_active &&
	    (wlc->cfg->link->rssi != WLC_RSSI_INVALID)) {
		if (!btc->restage_rxgain_active &&
		    ((BAND_5G(wlc->band->bandtype) &&
		      ((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_5G_MASK) == BTC_RXGAIN_FORCE_5G_ON)) ||
		     (BAND_2G(wlc->band->bandtype) &&
		      ((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_2G_MASK) == BTC_RXGAIN_FORCE_2G_ON) &&
		      (!btc->restage_rxgain_on_rssi_thresh ||
		       (btc_mode == WL_BTC_DISABLE) ||
		       (btc->restage_rxgain_on_rssi_thresh &&
			(btc_mode == WL_BTC_HYBRID) &&
			(-wlc->cfg->link->rssi < btc->restage_rxgain_on_rssi_thresh)))))) {
			if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				btc->restage_rxgain_level)) == BCME_OK) {
				btc->restage_rxgain_active = 1;
			}
			WL_ASSOC(("wl%d: BTC restage rxgain (%x) ON: RSSI %d Thresh -%d, bt %d, "
				"(err %d)\n",
				wlc->pub->unit, wlc->stf->rxchain, wlc->cfg->link->rssi,
				btc->restage_rxgain_on_rssi_thresh,
				btc->bth_active, i));
		}
		else if (btc->restage_rxgain_active &&
			((BAND_5G(wlc->band->bandtype) &&
			((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_5G_MASK) == BTC_RXGAIN_FORCE_OFF)) ||
			(BAND_2G(wlc->band->bandtype) &&
			(((btc->restage_rxgain_force &
			BTC_RXGAIN_FORCE_2G_MASK) == BTC_RXGAIN_FORCE_OFF) ||
			(btc->restage_rxgain_off_rssi_thresh &&
			(btc_mode == WL_BTC_HYBRID) &&
			(-wlc->cfg->link->rssi > btc->restage_rxgain_off_rssi_thresh)))))) {
			  if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0)) == BCME_OK) {
				btc->restage_rxgain_active = 0;
			  }
			  WL_ASSOC(("wl%d: BTC restage rxgain (%x) OFF: RSSI %d Thresh -%d, bt %d, "
				"(err %d)\n",
				wlc->pub->unit, wlc->stf->rxchain, wlc->cfg->link->rssi,
				btc->restage_rxgain_off_rssi_thresh,
				btc->bth_active, i));
		}
	} else if (btc->restage_rxgain_active) {
		if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0)) == BCME_OK) {
			btc->restage_rxgain_active = 0;
		}
		WL_ASSOC(("wl%d: BTC restage rxgain (%x) OFF: RSSI %d bt %d (err %d)\n",
			wlc->pub->unit, wlc->stf->rxchain, wlc->cfg->link->rssi,
			btc->bth_active, i));
	}

	return BCME_OK;
}

int
wlc_btc_siso_ack_set(wlc_info_t *wlc, int16 siso_ack, bool force)
{
	wlc_btc_info_t *btch = wlc->btch;
	uint16 sisoack_shm;
	uint16	sisoack_txpwr = 0;
	uint16	btparam_txpwr = 0;
	bool	sisoack_flag = FALSE;

	if (!btch)
		return BCME_ERROR;

	if (force) {

		if (siso_ack == AUTO) {
			btch->siso_ack_ovr = FALSE;
		} else {
			/* sanity check forced value */
			if (siso_ack && !(siso_ack & TXCOREMASK)) {
				return BCME_BADARG;
			}
			btch->siso_ack = siso_ack;
			btch->siso_ack_ovr = TRUE;
		}
	}

	if (!btch->siso_ack_ovr) {
		/* no override, set siso_ack according to btc_mode/chipids/boardflag etc. */
		if (siso_ack == AUTO) {
			siso_ack = 0;
#ifdef WLC_LOW
			if (wlc->pub->boardflags & BFL_FEM_BT) {
				/* check boardflag: antenna shared w BT */
				/* futher check srom, nvram */
				if (wlc->hw->btc->btcx_aa == 0x3) { /* two antenna */
					if (wlc->pub->boardflags2 &
					    BFL2_BT_SHARE_ANT0) { /* core 0 shared */
						siso_ack = 0x2;
					} else {
						siso_ack = 0x1;
					}
				} else if (wlc->hw->btc->btcx_aa == 0x7) { /* three antenna */
					; /* not supported yet */
				}
			} else
#endif /* WLC_LOW */
			{ /* For discrete chips this BFL_FEM_BT flag may not be set */
				if (btch->mode == WL_BTC_HYBRID)
					siso_ack = TXCORE0_MASK;
				else
					siso_ack = AUTO;
			}
		}
		btch->siso_ack = siso_ack;
	}
	btparam_txpwr = wlc_btc_params_get(wlc, BTC_PARAMS_FW_START_IDX + BTC_FW_SISO_ACK_TX_PWR);
	if (btparam_txpwr < 255) {
		sisoack_txpwr = btparam_txpwr << 8;
		sisoack_flag = TRUE;
	}

	/* update siso_ack shm */
	sisoack_shm = wlc_btc_sisoack_read_shm(wlc);
	if (sisoack_flag) {
		sisoack_shm = (sisoack_txpwr & BTC_SISOACK_TXPWR_MASK);
	} else {
		sisoack_shm &= BTC_SISOACK_TXPWR_MASK; /* txpwr offset set by phy */
	}
	sisoack_shm = sisoack_shm | (btch->siso_ack & BTC_SISOACK_CORES_MASK);
	wlc_btc_sisoack_write_shm(wlc, sisoack_shm);

	return BCME_OK;
}

int
wlc_btc_siso_ack_get(wlc_info_t *wlc)
{
	return wlc->btch->siso_ack;
}

uint16
wlc_btc_sisoack_read_shm(wlc_info_t *wlc)
{
	uint16 sisoack = 0;

	if (wlc->clk) {
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			sisoack = wlc_bmac_read_shm(wlc->hw, M_COREMASK_BTRESP);
		} else {
			sisoack = wlc_bmac_read_shm(wlc->hw, M_COREMASK_BTRESP_PRE40);
		}
	}

	return sisoack;
}

void
wlc_btc_sisoack_write_shm(wlc_info_t *wlc, uint16 sisoack)
{
	if (wlc->clk) {
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			wlc_bmac_write_shm(wlc->hw, M_COREMASK_BTRESP, sisoack);
		} else {
			wlc_bmac_write_shm(wlc->hw, M_COREMASK_BTRESP_PRE40, sisoack);
		}
	}
}

#ifdef BCMDBG
void
wlc_btc_dump_status(wlc_info_t *wlc,  struct bcmstrbuf *b)
{
	uint16 mhf = wlc_bmac_mhf_get(wlc->hw, MHF3, WLC_BAND_AUTO);
#ifdef WLC_LOW
	uint16 btc_hf = wlc_bmac_read_shm(wlc->hw, wlc->hw->btc->bt_shm_addr + M_BTCX_HOST_FLAGS);
#endif // endif
	bcm_bprintf(b, "Current BTCX status: \n");

	if (mhf & MHF3_BTCX_ACTIVE_PROT) {
		if (mhf & MHF3_BTCX_PS_PROTECT) {
			bcm_bprintf(b, "\t protection with ps null\n");
		} else {
			bcm_bprintf(b, "\t protection with CTSA\n");
		}
	}

#ifdef WLC_LOW
	if (btc_hf & BTCX_HFLG_ANT2WL) {
		bcm_bprintf(b, "\t prisel always to WL\n");
	}
	if (btc_hf & BTCX_HFLG_DYAGG) {
		bcm_bprintf(b, "\t dynamic tx aggregation\n");
	}
#endif /* WLC_LOW */

	if (wlc->btch->simrx_slna) {
		bcm_bprintf(b, "\t simultanoues rx with shared fem/lna\n");
	}

	if (wlc->btch->siso_ack) {
		bcm_bprintf(b, "\t siso ack\n");
	}
}
#endif /* BCMDBG */

static void wlc_btc_reset_btrssi(wlc_btc_info_t *btc)
{
	memset(btc->btrssi, 0, sizeof(int8)*BTC_BTRSSI_SIZE);
	btc->btrssi_cnt = 0;
	btc->btrssi_idx = 0;
	btc->btrssi_sum = 0;
}

static int
wlc_btc_wire_set(wlc_info_t *wlc, int int_val)
{
	int err;
	err = wlc_bmac_btc_wire_set(wlc->hw, int_val);
	wlc->btch->wire = wlc_bmac_btc_wire_get(wlc->hw);
	return err;
}

int
wlc_btc_wire_get(wlc_info_t *wlc)
{
	return wlc->btch->wire;
}

void wlc_btc_mode_sync(wlc_info_t *wlc)
{
	wlc->btch->mode = wlc_bmac_btc_mode_get(wlc->hw);
	wlc->btch->wire = wlc_bmac_btc_wire_get(wlc->hw);
}

uint8 wlc_btc_save_host_requested_pm(wlc_info_t *wlc, uint8 val)
{
	return (wlc->btch->host_requested_pm = val);
}

bool wlc_btc_get_bth_active(wlc_info_t *wlc)
{
	return wlc->btch->bth_active;
}

uint16 wlc_btc_get_bth_period(wlc_info_t *wlc)
{
	return wlc->btch->bth_period;
}

static int
wlc_btc_flags_idx_set(wlc_info_t *wlc, int int_val, int int_val2)
{
	return wlc_bmac_btc_flags_idx_set(wlc->hw, int_val, int_val2);
}

static int
wlc_btc_flags_idx_get(wlc_info_t *wlc, int int_val)
{
	return wlc_bmac_btc_flags_idx_get(wlc->hw, int_val);
}

int
wlc_btc_params_set(wlc_info_t *wlc, int int_val, int int_val2)
{
	return wlc_bmac_btc_params_set(wlc->hw, int_val, int_val2);
}

int
wlc_btc_params_get(wlc_info_t *wlc, int int_val)
{
	return wlc_bmac_btc_params_get(wlc->hw, int_val);
}

static void
wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable)
{
	wlc_bmac_btc_stuck_war50943(wlc->hw, enable);
}

static void
wlc_btc_rssi_threshold_get(wlc_info_t *wlc)
{
	wlc_bmac_btc_rssi_threshold_get(wlc->hw,
		&wlc->btch->prot_rssi_thresh,
		&wlc->btch->high_threshold,
		&wlc->btch->low_threshold);
}

void  wlc_btc_4313_gpioctrl_init(wlc_info_t *wlc)
{
	if (CHIPID(wlc->pub->sih->chip) == BCM4313_CHIP_ID) {
	/* ePA 4313 brds */
		if (wlc->pub->boardflags & BFL_FEM) {
			if (wlc->pub->boardrev >= 0x1250 && (wlc->pub->boardflags & BFL_FEM_BT)) {
				wlc_mhf(wlc, MHF5, MHF5_4313_BTCX_GPIOCTRL, MHF5_4313_BTCX_GPIOCTRL,
					WLC_BAND_ALL);
			} else
				wlc_mhf(wlc, MHF4, MHF4_EXTPA_ENABLE,
				MHF4_EXTPA_ENABLE, WLC_BAND_ALL);
			/* iPA 4313 brds */
			} else {
				if (wlc->pub->boardflags & BFL_FEM_BT)
					wlc_mhf(wlc, MHF5, MHF5_4313_BTCX_GPIOCTRL,
						MHF5_4313_BTCX_GPIOCTRL, WLC_BAND_ALL);
			}
	}
}

uint
wlc_btc_frag_threshold(wlc_info_t *wlc, struct scb *scb)
{
	ratespec_t rspec;
	uint rate, thresh;
	wlc_bsscfg_t *cfg;

	/* Make sure period is known */
	if (wlc->btch->bth_period == 0)
		return 0;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* if BT SCO is ongoing, packet length should not exceed 1/2 of SCO period */
	rspec = wlc_get_rspec_history(cfg);
	rate = RSPEC2KBPS(rspec);

	/*  use one half of the duration as threshold.  convert from usec to bytes */
	/* thresh = (bt_period * rate) / 1000 / 8 / 2  */
	thresh = (wlc->btch->bth_period * rate) >> 14;

	if (thresh < DOT11_MIN_FRAG_LEN)
		thresh = DOT11_MIN_FRAG_LEN;
	return thresh;
}

#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
static void
wlc_btc_pm_adjust(wlc_info_t *wlc,  bool bt_active)
{
	wlc_bsscfg_t *cfg = wlc->cfg;
	/* only bt is not active, set PM to host requested mode */
	if (wlc->btch->host_requested_pm != PM_FORCE_OFF) {
		if (bt_active) {
				if (PM_OFF == wlc->btch->host_requested_pm &&
				cfg->pm->PM != PM_FAST)
				wlc_set_pm_mode(wlc, PM_FAST, cfg);
		} else {
			if (wlc->btch->host_requested_pm != cfg->pm->PM)
				wlc_set_pm_mode(wlc, wlc->btch->host_requested_pm, cfg);
		}
	}
}
#endif /* STA */

/* XXX FIXME
 * BTCX settings are now global but we may later on need to change it for multiple BSS
 * hence pass the bsscfg as a parm.
 */
void
wlc_enable_btc_ps_protection(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool protect)
{
	int btc_mode = wlc_btc_mode_get(wlc);
	/*
	BTCX mode is set in wlc_mchan_set_btcx_prot_mode() in wlc_mchan.c when
	MCHAN is active
	*/
	if (MCHAN_ACTIVE(wlc->pub))
		return;

	if ((wlc_btc_wire_get(wlc) >= WL_BTC_3WIRE) && btc_mode) {
		wlc_bsscfg_t *pc;
		wlc_btc_info_t *btc = wlc->btch;
		int btc_flags = wlc_bmac_btc_flags_get(wlc->hw);
		uint16 protections = 0;
		uint16 active = 0;
		uint16 ps = 0;
		uint16 simrsp = 0;
		int8 wlrssi_th, btrssi_th;
		bool act_prot = (wlc_bmac_mhf_get(wlc->hw, MHF3, WLC_BAND_AUTO) &
		    MHF3_BTCX_ACTIVE_PROT);

		pc = wlc->cfg;
		BCM_REFERENCE(pc);

		/* if radio is disable, driver may be down, quit here */
		if (wlc->pub->radio_disabled || !wlc->pub->up)
			return;

#if defined(STA) && !defined(BCMNODOWN)
		/* ??? if ismpc, driver should be in down state if up/down is allowed */
		if (wlc->mpc && wlc_ismpc(wlc))
			return;
#endif // endif

#ifdef WL_BTCDYN
		if (!IS_DYNCTL_ON(wlc->btch->dprof)) {
#endif // endif

			if (btc_flags & WL_BTC_FLAG_SIM_RSP) {
				simrsp = MHF3_BTCX_SIM_RSP;
			}

			/* prot_rssi_thresh is negate. e.g., 70 -> -70 dBm */
			wlrssi_th = (int8)(btc->prot_rssi_thresh * -1);
			if (wlrssi_th && act_prot) {
				wlrssi_th += btc->wlrssi_hyst;
			}

			btrssi_th = wlc->btch->prot_btrssi_thresh;
			if (btrssi_th && act_prot) {
				btrssi_th += btc->btrssi_hyst;
			}

			btc->bt_rssi = wlc_btc_get_btrssi(btc);

			/* enable implicit tdm when either wl/bt rssi below threshold(s) */
			if ((wlrssi_th && wlc->cfg->link->rssi < wlrssi_th) ||
			    (btrssi_th && btc->bt_rssi < btrssi_th))
			{
				active = MHF3_BTCX_ACTIVE_PROT;
				if (CHIPID(wlc->pub->sih->chip) == BCM4350_CHIP_ID) {
					ps = MHF3_BTCX_PS_PROTECT;
					simrsp = 0;
				}
			}

			/* enable simrx (w sLNA) when both wl/bt rssi above thresholds */
			if (btc_mode == WL_BTC_HYBRID &&
			    (wlrssi_th && wlc->cfg->link->rssi &&
			    wlc->cfg->link->rssi >= wlrssi_th) &&
			    (btrssi_th && btc->bt_rssi && btc->bt_rssi >= btrssi_th)) {
				btc->simrx_slna = TRUE;
			}
#ifdef WL_BTCDYN
		}
#endif // endif
		/* check explicit tdm flags */
		if (btc_flags & WL_BTC_FLAG_ACTIVE_PROT) {
			active = MHF3_BTCX_ACTIVE_PROT;
		}
		if (btc_flags & WL_BTC_FLAG_PS_PROTECT) {
			ps = MHF3_BTCX_PS_PROTECT;
		}
		BCM_REFERENCE(ps);

#ifdef STA
		/* Enable PS protection when the primary bsscfg is associated as
		 * an infra STA and is the only connection
		 */
		if (BSSCFG_STA(pc) && pc->current_bss->infra &&
		    WLC_BSS_CONNECTED(pc) && wlc->stas_connected == 1 &&
		    (wlc->aps_associated == 0 || wlc_ap_stas_associated(wlc->ap) == 0)) {
			/* when WPA/PSK security is enabled wait until WLC_PORTOPEN() is TRUE */
			if (pc->WPA_auth == WPA_AUTH_DISABLED || !WSEC_ENABLED(pc->wsec) ||
			    WLC_PORTOPEN(pc))
				protections = active | ps;
			/* XXX temporarily disable protections in between
			 * association is done and key is plumbed???
			 */
			else
				protections = 0;
		}
		/*
		Enable PS protection if there is only one BSS
		associated as STA and there are no APs. All else
		enable CTS2SELF.
		*/
		else if (wlc->stas_connected > 0)
		{
			if ((wlc->stas_connected == 1) && (wlc->aps_associated == 0) &&
				!BSSCFG_IBSS(pc))
				protections = active | ps;
			else
				protections = active;
		}
		else
#endif /* STA */
#ifdef AP
		if (wlc->aps_associated > 0 && wlc_ap_stas_associated(wlc->ap) > 0)
			protections = active;
		/* No protection */
		else
#endif /* AP */
			protections = 0;

		protections |= simrsp;

		wlc_mhf(wlc, MHF3, MHF3_BTCX_ACTIVE_PROT | MHF3_BTCX_PS_PROTECT | MHF3_BTCX_SIM_RSP,
		    protections, WLC_BAND_2G);

		/* disable siso ack in tdm (IOVAR can override) */
		if ((CHIPID(wlc->pub->sih->chip) == BCM4350_CHIP_ID) ||
			(CHIPID(wlc->pub->sih->chip) == BCM43602_CHIP_ID)) {
			if (protections & MHF3_BTCX_ACTIVE_PROT) {
				wlc_btc_siso_ack_set(wlc, 0, FALSE);
			} else {
				wlc_btc_siso_ack_set(wlc, AUTO, FALSE);
			}
		}

#ifdef WLMCNX
		/*
		For non-VSDB the only time we turn on PS protection is when there is only
		one STA associated - primary or GC. In this case, set the BSS index in
		designated SHM location as well.
		*/
		if ((MCNX_ENAB(wlc->pub)) && (protections & ps)) {
			uint idx;
			wlc_bsscfg_t *cfg;
			int bss_idx;

			FOREACH_AS_STA(wlc, idx, cfg) {
				if (!cfg->BSS)
					continue;
				bss_idx = wlc_mcnx_BSS_idx(wlc->mcnx, cfg);
				wlc_mcnx_shm_bss_idx_set(wlc->mcnx, bss_idx);
				break;
			}
		}
#endif /* WLMCNX */
	}
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP) || defined(WLTEST)
static int wlc_dump_btcx(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint8 idx, offset;
	uint16 hi, lo;
	uint32 buff[C_BTCX_DBGBLK_SZ/2];
	uint16 base = D11REV_LT(wlc->pub->corerev, 40) ?
	M_BTCX_DBGBLK: M_BTCX_DBGBLK_11AC;

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	for (idx = 0; idx < C_BTCX_DBGBLK_SZ; idx += 2) {
		offset = idx*2;
		lo = wlc_bmac_read_shm(wlc->hw, base+offset);
		hi = wlc_bmac_read_shm(wlc->hw, base+offset+2);
		buff[idx>>1] = (hi<<16) | lo;
	}

	bcm_bprintf(b, "nrfact: %u, ntxconf: %u (%u%%), txconf_durn(us): %u\n",
		buff[0], buff[1], buff[0] ? (buff[1]*100)/buff[0]: 0, buff[2]);
	return 0;
}

static int wlc_clr_btcxdump(wlc_info_t *wlc)
{
	uint16 base = D11REV_LT(wlc->pub->corerev, 40) ?
	M_BTCX_DBGBLK: M_BTCX_DBGBLK_11AC;

	if (!wlc->clk) {
	return BCME_NOCLK;
	}

	wlc_bmac_set_shm(wlc->hw, base, 0, C_BTCX_DBGBLK_SZ*2);
	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST */

/* Read relevant BTC params to determine if aggregation has to be enabled/disabled */
void
wlc_btcx_read_btc_params(wlc_info_t *wlc)
{
	wlc_btc_info_t *btc = wlc->btch;
	if (BT3P_HW_COEX(wlc)) {
		wlc_bmac_btc_period_get(wlc->hw, &btc->bth_period,
			&btc->bth_active, &btc->agg_off_bm);
		if (!btc->bth_active && !btc->btrssi_cnt) {
			wlc_btc_reset_btrssi(btc);
		}
	}
}

static int
wlc_btcx_watchdog(void *arg)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int btc_mode = wlc_btc_mode_get(wlc);
	int abort_delta_cnt = 0;
	int last_a2dp = 0;

#ifdef BCMLTECOEX
	uint16 ltecx_rx_reaggr_off;
#endif /* BCMLTECOEX */
	wlc->btch->run_cnt++;

#if defined(WLC_LOW) && defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB(wlc->pub)) {
		/* Indicate MCHAN status to BT */
		if (MCHAN_ACTIVE(wlc->pub)) {
			/* Indicate to BT that multi-channel is enabled */
			si_gci_direct(wlc->pub->sih, OFFSETOF(chipcregs_t, gci_control_1),
					GCI_WL_MCHAN_BIT_MASK, GCI_WL_MCHAN_BIT_MASK);
			si_gci_direct(wlc->pub->sih, OFFSETOF(chipcregs_t, gci_output[1]),
					GCI_WL_MCHAN_BIT_MASK, GCI_WL_MCHAN_BIT_MASK);
		} else {
			/* Indicate to BT that multi-channel is disabled */
			si_gci_direct(wlc->pub->sih, OFFSETOF(chipcregs_t, gci_control_1),
					GCI_WL_MCHAN_BIT_MASK, 0);
			si_gci_direct(wlc->pub->sih, OFFSETOF(chipcregs_t, gci_output[1]),
					GCI_WL_MCHAN_BIT_MASK, 0);
		}
	}
#endif /* WLC_LOW && BCMPCIEDEV */

#ifndef WL_BTCDYN
	/* simrx_slan will be set to TRUE if wl/bt rssi above thresholds */
	wlc->btch->simrx_slna = FALSE;
#endif // endif

	/* update critical BT state, only for 2G band */
	if (btc_mode && BAND_2G(wlc->band->bandtype)) {
#if defined(STA) && defined(BTCX_PM0_IDLE_WAR)
		wlc_btc_pm_adjust(wlc, wlc->btch->bth_active);
#endif /* STA */

#ifdef WL_BTCDYN
		if (IS_DYNCTL_ON(btc->dprof)) {
			/* new dynamic btcoex algo */
			wlc_btcx_dynctl(btc);
		} else {
#endif /* WL_BTCDYN */
			/* legacy mode switching */
			wlc_btc_rssi_threshold_get(wlc);

			/* if rssi too low, switch to TDM */
			if (wlc->btch->low_threshold &&
				-wlc->cfg->link->rssi >
				wlc->btch->low_threshold) {
				if (!IS_BTCX_FULLTDM(btc_mode)) {
					wlc->btch->mode_overridden = (uint8)btc_mode;
					wlc_btc_mode_set(wlc, WL_BTC_FULLTDM);
				}
			} else if (wlc->btch->high_threshold &&
				-wlc->cfg->link->rssi <
				wlc->btch->high_threshold) {
				if (btc_mode != WL_BTC_PARALLEL) {
					wlc->btch->mode_overridden = (uint8)btc_mode;
					wlc_btc_mode_set(wlc, WL_BTC_PARALLEL);
				}
			} else {
				if (wlc->btch->mode_overridden) {
					wlc_btc_mode_set(wlc, wlc->btch->mode_overridden);
					wlc->btch->mode_overridden = 0;
				}
			}

			/* enable protection in ucode */
			wlc_enable_btc_ps_protection(wlc, wlc->cfg, TRUE);
#ifdef WL_BTCDYN
		}
#endif // endif
	}

#if defined(WLC_LOW) && !defined(WL_BTCDYN)
	wlc_phy_btc_simrx(wlc->hw->band->pi, wlc->btch->simrx_slna);
#endif // endif

#if defined(BCMECICOEX) && defined(WL11N)
	if (CHIPID(wlc->pub->sih->chip) == BCM4331_CHIP_ID &&
		(wlc->pub->boardflags & BFL_FEM_BT)) {
		/* for X28, middle chain has to be disabeld when bt is active */
		/* to assure smooth rate policy */
		if ((btc_mode == WL_BTC_LITE || btc_mode == WL_BTC_HYBRID) &&
			BAND_2G(wlc->band->bandtype) && wlc->btch->bth_active) {
			if (btc_mode == WL_BTC_LITE)
				wlc_stf_txchain_set(wlc, 0x5, TRUE,
					WLC_TXCHAIN_ID_BTCOEX);
		} else {
			wlc_stf_txchain_set(wlc, wlc->stf->hw_txchain,
				FALSE, WLC_TXCHAIN_ID_BTCOEX);
		}
	}
#endif // endif

#if defined(WLAIBSS)
	if (AIBSS_ENAB(wlc->pub)) {
		/* Check and enable/disable Oxygen protection as needed */
		/* Check for any stale beacons every 5 seconds */
		if ((wlc->btch->run_cnt % BT_STALE_BEACON_CHK_PERIOD) == 0)
			wlc_btc_aibss_get_state(btc, CHECK_AIBSS_STALE_BEACON);
		wlc_btcx_aibss_check_state(btc);
		wlc_btcx_aibss_chk_clk_sync(btc);
	}
#endif // endif

#ifdef WLAMPDU
	if (BT3P_HW_COEX(wlc) && BAND_2G(wlc->band->bandtype)) {
		if (btc_mode && btc_mode != WL_BTC_PARALLEL) {
			/* condition on 4356z 2.4G TDM mode only, check for busy BT traffic */
			if (IS_4356Z_BTCX(wlc->band->phyrev, wlc->band->radiorev,
			    wlc->hw->xtalfreq, wlc)) {
				wlc->btch->abort_prev_cnt = wlc->btch->abort_curr_cnt;
				wlc->btch->abort_curr_cnt =
					wlc_btc_params_get(wlc, M_BTCX_ABORT_CNT/2);
				abort_delta_cnt =
					wlc->btch->abort_curr_cnt -
					wlc->btch->abort_prev_cnt;
				last_a2dp = wlc_btc_params_get(wlc, M_BTCX_LAST_A2DP/2);
			}
			/* Make sure STA is on the home channel to avoid changing AMPDU
			 * state during scanning
			 */

			if (AMPDU_ENAB(wlc->pub) && !SCAN_IN_PROGRESS(wlc->scan) &&
			    wlc->pub->associated) {
				bool dyagg = FALSE; /* dyn tx agg turned off by default */
#ifdef BCMLTECOEX
				if (BCMLTECOEX_ENAB(wlc->pub))	{
					if (wlc->hw->ltecx_hw->mws_lterx_prot) {
						if (wlc_ltecx_get_lte_status(wlc) &&
							!wlc->hw->ltecx_hw->mws_elna_bypass)
						{
							wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
								WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
							wlc->hw->ltecx_hw->mws_rx_aggr_off = TRUE;
						}
						if (wlc_ltecx_get_lte_status(wlc))
							wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
								WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
					}
				}
#endif /* BCMLTECOEX */
				/* process all bt related disabling/enabling here */
				if ((wlc->btch->bth_period &&
				    wlc->btch->bth_period < BT_AMPDU_THRESH) ||
				    wlc->btch->agg_off_bm ||
#ifdef WLAIBSS
				    btc->rx_agg_change ||
#endif // endif
				    (IS_4356Z_BTCX(wlc->band->phyrev, wlc->band->radiorev,
						wlc->hw->xtalfreq, wlc) &&
					(last_a2dp != 0) &&
					(abort_delta_cnt > BT_AMPDU_THRESH_ABORTCNT)) ||
				(FALSE)) {
					/* shutoff one rxchain to avoid steep rate drop */
					if ((btc_mode == WL_BTC_HYBRID) &&
						(CHIPID(wlc->pub->sih->chip) == BCM43225_CHIP_ID)) {
						wlc_stf_rxchain_set(wlc, 1, TRUE);
						wlc->stf->rxchain_restore_delay = 0;
					}
					if (IS_BTCX_FULLTDM(btc_mode)) {
#ifdef WLAIBSS
						if ((AIBSS_ENAB(wlc->pub)) && btc->rx_agg_change) {
							wlc_btcx_aibss_update_agg_size(btc);
						} else
#endif /* WLAIBSS */
						{
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
							WLC_BAND_2G, AMPDU_UPD_ALL);
						}
					} else {
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
							WLC_BAND_2G, AMPDU_UPD_ALL);
						if (btc_mode == WL_BTC_HYBRID &&
						    D11REV_GE(wlc->pub->corerev, 48)) {
							dyagg = TRUE; /* enable dyn tx agg */
						}
					}
				} else {
#ifdef BCMLTECOEX
					if (BCMLTECOEX_ENAB(wlc->pub))	{
						if (((!wlc_ltecx_get_lte_status(wlc) ||
							wlc->hw->ltecx_hw->mws_elna_bypass)	&&
							wlc->hw->ltecx_hw->mws_rx_aggr_off) &&
							!wlc->btch->bth_active) {
							ltecx_rx_reaggr_off = wlc_read_shm(wlc,
								wlc->hw->btc->bt_shm_addr
								+ M_LTECX_RX_REAGGR);
							if (!ltecx_rx_reaggr_off) {
								/* Resume Rx aggregation per
								  * SWWLAN-32809
								  */
								wlc_ampdu_agg_state_upd_ovrd_band
								(wlc, ON, WLC_BAND_2G,
								AMPDU_UPD_RX_ONLY);
								wlc->hw->ltecx_hw->mws_rx_aggr_off =
									FALSE;
							}
						}
						if (!wlc_ltecx_get_lte_status(wlc))
							wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
								WLC_BAND_2G, AMPDU_UPD_ALL);
					}
					else
#endif /* BCMLTECOEX */
					{
#ifdef WLAIBSS
						if (AIBSS_ENAB(wlc->pub)) {
							wlc_btcx_aibss_update_agg_size(btc);
						} else
#endif /* WLAIBSS */
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
							WLC_BAND_2G, AMPDU_UPD_ALL);
					}
					if ((btc_mode == WL_BTC_HYBRID) &&
						(CHIPID(wlc->pub->sih->chip) == BCM43225_CHIP_ID) &&
						(++wlc->stf->rxchain_restore_delay > 5)) {
						/* restore rxchain. */
						wlc_stf_rxchain_set(wlc,
							wlc->stf->hw_rxchain, TRUE);
						wlc->stf->rxchain_restore_delay = 0;
					}
				}
				if (wlc->btch->dyagg != AUTO) {
					dyagg = wlc->btch->dyagg; /* IOVAR override */
				}
				wlc_btc_hflg(wlc, dyagg, BTCX_HFLG_DYAGG);
			}
		} else {
#ifdef BCMLTECOEX
			if (BCMLTECOEX_ENAB(wlc->pub))	{
				if (wlc_ltecx_get_lte_status(wlc) &&
					!wlc->hw->ltecx_hw->mws_elna_bypass) {
					wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
						WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
					wlc->hw->ltecx_hw->mws_rx_aggr_off = TRUE;
				} else if (wlc->hw->ltecx_hw->mws_rx_aggr_off &&
				!wlc->btch->bth_active) {
					ltecx_rx_reaggr_off = wlc_read_shm(wlc,
						wlc->hw->btc->bt_shm_addr + M_LTECX_RX_REAGGR);
					if (!ltecx_rx_reaggr_off) {
						/* Resume Rx aggregation per SWWLAN-32809 */
						wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
							WLC_BAND_2G, AMPDU_UPD_RX_ONLY);
						wlc->hw->ltecx_hw->mws_rx_aggr_off = FALSE;
					}
				}
				/* Don't resume tx aggr while LTECX is active */
				if (wlc_ltecx_get_lte_status(wlc))
					wlc_ampdu_agg_state_upd_ovrd_band(wlc, OFF,
						WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
				else
					wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
						WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
			}
			else
#endif /* BCMLTECOEX */
			{
				/* Dynamic BTC mode requires this */
				wlc_ampdu_agg_state_upd_ovrd_band(wlc, ON,
					WLC_BAND_2G, AMPDU_UPD_TX_ONLY);
			}
		}
	}
#endif /* WLAMPDU */

#ifdef WL_BTCDYN
	if (!IS_DYNCTL_ON(btc->dprof)) {
		/* legacy desnse coex  */
		wlc_btcx_desense(btc, wlc->band->bandtype);
	}
#else
	wlc_btcx_desense(btc, wlc->band->bandtype);
#endif // endif

	if (wlc->clk && (wlc->pub->sih->boardvendor == VENDOR_APPLE) &&
	    ((CHIPID(wlc->pub->sih->chip) == BCM4331_CHIP_ID) ||
	     BCM43602_CHIP(wlc->pub->sih->chip) ||
	     (CHIPID(wlc->pub->sih->chip) == BCM4360_CHIP_ID))) {
		wlc_write_shm(wlc, M_COREMASK_BTRESP, (uint16)btc->siso_ack);
	}

	return BCME_OK;
}

/* handle BTC related iovars */
static int
wlc_btc_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_btc_info_t *btc = (wlc_btc_info_t *)ctx;
	wlc_info_t *wlc = (wlc_info_t *)btc->wlc;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {

	case IOV_SVAL(IOV_BTC_FLAGS):
		err = wlc_btc_flags_idx_set(wlc, int_val, int_val2);
		break;

	case IOV_GVAL(IOV_BTC_FLAGS): {
		*ret_int_ptr = wlc_btc_flags_idx_get(wlc, int_val);
		break;
		}

	case IOV_SVAL(IOV_BTC_PARAMS):
		err = wlc_btc_params_set(wlc, int_val, int_val2);
		break;

	case IOV_GVAL(IOV_BTC_PARAMS):
		*ret_int_ptr = wlc_btc_params_get(wlc, int_val);
		break;

	case IOV_SVAL(IOV_BTC_MODE):
		err = wlc_btc_mode_set(wlc, int_val);
		break;

	case IOV_GVAL(IOV_BTC_MODE):
		*ret_int_ptr = wlc_btc_mode_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_WIRE):
		err = wlc_btc_wire_set(wlc, int_val);
		break;

	case IOV_GVAL(IOV_BTC_WIRE):
		*ret_int_ptr = wlc_btc_wire_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_STUCK_WAR):
		wlc_btc_stuck_war50943(wlc, bool_val);
		break;

#if defined(BCMDBG) || defined(BCMDBG_DUMP)|| defined(WLTEST)
	case IOV_SVAL(IOV_BTCX_CLEAR_DUMP):
		err = wlc_clr_btcxdump(wlc);
		break;
#endif /* BCMDBG || BCMDBG_DUMP || WLTEST */

	case IOV_GVAL(IOV_BTC_SISO_ACK):
		*ret_int_ptr = wlc_btc_siso_ack_get(wlc);
		break;

	case IOV_SVAL(IOV_BTC_SISO_ACK):
		wlc_btc_siso_ack_set(wlc, (int16)int_val, TRUE);
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_THRESH):
		*ret_int_ptr = ((uint32)btc->restage_rxgain_on_rssi_thresh |
			((uint32)btc->restage_rxgain_off_rssi_thresh << 8));
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_THRESH):
		if (int_val == 0) {
			err = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0);
			if (err == BCME_OK) {
				btc->restage_rxgain_on_rssi_thresh = 0;
				btc->restage_rxgain_off_rssi_thresh = 0;
				btc->restage_rxgain_active = 0;
				WL_ASSOC(("wl%d: BTC restage rxgain disabled\n", wlc->pub->unit));
			} else {
				err = BCME_NOTREADY;
			}
		} else {
			btc->restage_rxgain_on_rssi_thresh = (uint8)(int_val & 0xFF);
			btc->restage_rxgain_off_rssi_thresh = (uint8)((int_val >> 8) & 0xFF);
			WL_ASSOC(("wl%d: BTC restage rxgain enabled\n", wlc->pub->unit));
		}
		WL_ASSOC(("wl%d: BTC restage rxgain thresh ON: -%d, OFF -%d\n",
			wlc->pub->unit,
			btc->restage_rxgain_on_rssi_thresh,
			btc->restage_rxgain_off_rssi_thresh));
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_FORCE):
		*ret_int_ptr = btc->restage_rxgain_force;
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_FORCE):
		btc->restage_rxgain_force = int_val;
		break;

	case IOV_GVAL(IOV_BTC_RXGAIN_LEVEL):
		*ret_int_ptr = btc->restage_rxgain_level;
		break;

	case IOV_SVAL(IOV_BTC_RXGAIN_LEVEL):
		btc->restage_rxgain_level = int_val;
		if (btc->restage_rxgain_active) {
			if ((err = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain",
				btc->restage_rxgain_level)) != BCME_OK) {
				/* Need to apply new level on next update */
				btc->restage_rxgain_active = 0;
				err = BCME_NOTREADY;
			}
			WL_ASSOC(("wl%d: set BTC rxgain level %d (active %d)\n",
				wlc->pub->unit,
				btc->restage_rxgain_level,
				btc->restage_rxgain_active));
		}
		break;
#ifdef	WL_BTCDYN
	case IOV_SVAL(IOV_BTC_DYNCTL):
		err = wlc_btc_dynctl_profile_set(wlc, params);
		break;
	case IOV_GVAL(IOV_BTC_DYNCTL):
		err = wlc_btc_dynctl_profile_get(wlc, arg);
		break;

	case IOV_GVAL(IOV_BTC_DYNCTL_STATUS):
		err = wlc_btc_dynctl_status_get(wlc, arg);
		break;

	case IOV_GVAL(IOV_BTC_DYNCTL_SIM):
		err = wlc_btc_dynctl_sim_get(wlc, arg);
		break;
	case IOV_SVAL(IOV_BTC_DYNCTL_SIM):
		err = wlc_btc_dynctl_sim_set(wlc, arg);
		break;
#endif /* WL_BTCDYN */

#if defined(BCMDBG)
#ifdef WLAIBSS
	case IOV_SVAL(IOV_BTC_SET_STROBE):
		if (AIBSS_ENAB(wlc->pub)) {
			wlc_btcx_strobe_enable(btc, int_val);
		}
		break;

	case IOV_GVAL(IOV_BTC_SET_STROBE):
		*ret_int_ptr = btc->strobe_enabled;
		break;
#endif /* WLAIBSS */

	case IOV_GVAL(IOV_BTC_WLRSSI_THRESH):
		/* prot_rssi_thresh is negate (i.e., 70 -> -70dBm) */
		*ret_int_ptr = (int8)(btc->prot_rssi_thresh * -1);
		break;

	case IOV_SVAL(IOV_BTC_WLRSSI_THRESH):
		err = wlc_btc_params_set(wlc, (M_BTCX_PROT_RSSI_THRESH >> 1), int_val * -1);
		break;

	case IOV_GVAL(IOV_BTC_BTRSSI_THRESH):
		*ret_int_ptr = btc->prot_btrssi_thresh;
		break;

	case IOV_SVAL(IOV_BTC_BTRSSI_THRESH):
		btc->prot_btrssi_thresh = (int8)int_val;
		break;

	case IOV_GVAL(IOV_BTC_WLRSSI_HYST):
		*ret_int_ptr = btc->wlrssi_hyst;
		break;

	case IOV_SVAL(IOV_BTC_WLRSSI_HYST):
		btc->wlrssi_hyst = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_BTC_BTRSSI_HYST):
		*ret_int_ptr = btc->btrssi_hyst;
		break;

	case IOV_SVAL(IOV_BTC_BTRSSI_HYST):
		btc->btrssi_hyst = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_BTC_BTRSSI_AVG):
		*ret_int_ptr = btc->bt_rssi;
		break;

	case IOV_SVAL(IOV_BTC_BTRSSI_AVG):
		if (int_val == 0) {
			wlc_btc_reset_btrssi(wlc->btch);
		}
		break;

	case IOV_GVAL(IOV_BTC_DYAGG):
		*ret_int_ptr = btc->dyagg;
		break;

	case IOV_SVAL(IOV_BTC_DYAGG):
		btc->dyagg = (int8)int_val;
		break;
#endif /* BCMDBG */

	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}

/* E.g., To set BTCX_HFLG_SKIPLMP, wlc_btc_hflg(wlc, 1, BTCX_HFLG_SKIPLMP) */
void
wlc_btc_hflg(wlc_info_t *wlc, bool set, uint16 val)
{
	uint16 btc_blk_ptr, btc_hflg;

	if (!wlc->clk)
		return;

	btc_blk_ptr = 2 * wlc_bmac_read_shm(wlc->hw, M_BTCX_BLK_PTR);
	btc_hflg = wlc_bmac_read_shm(wlc->hw, btc_blk_ptr + M_BTCX_HOST_FLAGS);

	if (set)
		btc_hflg |= val;
	else
		btc_hflg &= ~val;

	wlc_bmac_write_shm(wlc->hw, btc_blk_ptr + M_BTCX_HOST_FLAGS, btc_hflg);
}

/* TODO: Move the following LTECOEX code to a new file: wlc_ltecx.c */
#ifdef BCMLTECOEX

enum {
	IOV_LTECX_MWS_COEX_BITMAP,
	IOV_LTECX_MWS_WLANRX_PROT,
	IOV_LTECX_WCI2_TXIND,
	IOV_LTECX_MWS_WLANRXPRI_THRESH,
	IOV_LTECX_WCI2_CONFIG,
	IOV_LTECX_MWS_PARAMS,
	IOV_LTECX_MWS_FRAME_CONFIG,
	IOV_LTECX_MWS_COEX_BAUDRATE,
	IOV_LTECX_MWS_SCANJOIN_PROT,
	IOV_LTECX_MWS_LTETX_ADV_PROT,
	IOV_LTECX_MWS_LTERX_PROT,
	IOV_LTECX_MWS_ELNA_RSSI_THRESH,
	IOV_LTECX_MWS_IM3_PROT,
	IOV_LTECX_MWS_WIFI_SENSITIVITY,
	IOV_LTECX_MWS_DEBUG_MSG,
	IOV_LTECX_MWS_DEBUG_MODE,
	IOV_LTECX_WCI2_LOOPBACK
};

/* LTE coex iovars */
const bcm_iovar_t ltecx_iovars[] = {
	{"mws_coex_bitmap", IOV_LTECX_MWS_COEX_BITMAP,
	(0), IOVT_UINT16, 0
	},
	{"mws_wlanrx_prot", IOV_LTECX_MWS_WLANRX_PROT,
	(0), IOVT_UINT16, 0
	},
	{"wci2_txind", IOV_LTECX_WCI2_TXIND,
	(0), IOVT_UINT16, 0
	},
	{"mws_wlanrxpri_thresh", IOV_LTECX_MWS_WLANRXPRI_THRESH,
	(0), IOVT_UINT16, 0
	},
	{"mws_params", IOV_LTECX_MWS_PARAMS,
	(0),
	IOVT_BUFFER, sizeof(mws_params_t)
	},
	{"wci2_config", IOV_LTECX_WCI2_CONFIG,
	(0),
	IOVT_BUFFER, sizeof(wci2_config_t)
	},
	{"mws_frame_config", IOV_LTECX_MWS_FRAME_CONFIG,
	(0),
	IOVT_UINT16, 0
	},
	{"mws_baudrate", IOV_LTECX_MWS_COEX_BAUDRATE,
	(IOVF_SET_DOWN), IOVT_UINT16, 0
	},
	{"mws_scanjoin_prot", IOV_LTECX_MWS_SCANJOIN_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_ltetx_adv", IOV_LTECX_MWS_LTETX_ADV_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_lterx_prot", IOV_LTECX_MWS_LTERX_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_im3_prot", IOV_LTECX_MWS_IM3_PROT,
	(0), IOVT_UINT16, 0
	},
	{"mws_wifi_sensitivity", IOV_LTECX_MWS_WIFI_SENSITIVITY,
	(0), IOVT_INT32, 0
	},
	{"mws_debug_msg", IOV_LTECX_MWS_DEBUG_MSG,
	(IOVF_SET_UP | IOVF_GET_UP),
	IOVT_BUFFER, sizeof(mws_wci2_msg_t)
	},
	{"mws_debug_mode", IOV_LTECX_MWS_DEBUG_MODE,
	(IOVF_SET_UP | IOVF_GET_UP), IOVT_INT32, 0
	},
	{"mws_elna_rssi_thresh", IOV_LTECX_MWS_ELNA_RSSI_THRESH,
	(0), IOVT_INT32, 0
	},
	{"wci2_loopback", IOV_LTECX_WCI2_LOOPBACK,
	(IOVF_SET_UP | IOVF_GET_UP),
	IOVT_BUFFER, sizeof(wci2_loopback_t)
	},
	{NULL, 0, 0, 0, 0}
};

static int wlc_ltecx_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_ltecx_watchdog(void *arg);

static const char BCMATTACHDATA(rstr_ltecx)[]                    = "ltecx";
static const char BCMATTACHDATA(rstr_ltecx_rssi_thresh_lmt)[]    = "ltecx_rssi_thresh_lmt";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_mode)[]         = "ltecx_20mhz_mode";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_mode)[]         = "ltecx_10mhz_mode";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2390_rssi_th)[] = "ltecx_20mhz_2390_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2385_rssi_th)[] = "ltecx_20mhz_2385_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2380_rssi_th)[] = "ltecx_20mhz_2380_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2375_rssi_th)[] = "ltecx_20mhz_2375_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_20mhz_2370_rssi_th)[] = "ltecx_20mhz_2370_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2395_rssi_th)[] = "ltecx_10mhz_2395_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2390_rssi_th)[] = "ltecx_10mhz_2390_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2385_rssi_th)[] = "ltecx_10mhz_2385_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2380_rssi_th)[] = "ltecx_10mhz_2380_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2375_rssi_th)[] = "ltecx_10mhz_2375_rssi_th";
static const char BCMATTACHDATA(rstr_ltecx_10mhz_2370_rssi_th)[] = "ltecx_10mhz_2370_rssi_th";
static const char BCMATTACHDATA(rstr_ltecxmux)[]                 = "ltecxmux";
/* ROM auto patching work-around. Explicitly disable LTECX IOVAR patching, which is not supported
 * when the BTCX and LTECX IOVAR tables/handlers co-exist in the same source file. The following
 * macros prevent the BTCX IOVAR patch table/handler from being incorrectly registered with
 * the LTECX module.
 */
#undef  ROM_AUTO_IOCTL_PATCH_IOVARS
#define ROM_AUTO_IOCTL_PATCH_IOVARS    NULL
#undef  ROM_AUTO_IOCTL_PATCH_DOIOVAR
#define ROM_AUTO_IOCTL_PATCH_DOIOVAR   NULL

wlc_ltecx_info_t *
BCMATTACHFN(wlc_ltecx_attach)(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	wlc_ltecx_info_t *ltecx;
	uint16 i, array_size;
	/* ltecxmux
	 * #nibble 0: Mode (WCI2/GPIO/...)
	 * #nibble 1: FNSEL(WCI2) / LTECX PIN Configuration
	 * #nibble 2: UART1 GPIO#(WCI2) / Frame Sync GPIO number
	 * #nibble 3: UART2 GPIO#(WCI2) / LTE Rx GPIO number
	 * #nibble 4: LTE Tx GPIO number
	 * #nibble 5: WLAN Priority GPIO number
	 */

	if ((ltecx = (wlc_ltecx_info_t *)
		MALLOCZ(wlc->osh, sizeof(wlc_ltecx_info_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	ltecx->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, ltecx_iovars, rstr_ltecx, ltecx, wlc_ltecx_doiovar,
		wlc_ltecx_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_rssi_thresh_lmt) != NULL) {
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram =
				(uint8)getintvar(wlc_hw->vars, rstr_ltecx_rssi_thresh_lmt);
	}
	else {
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram = LTE_RSSI_THRESH_LMT;
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_mode) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars, rstr_ltecx_20mhz_mode);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_mode, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_mode) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars, rstr_ltecx_10mhz_mode);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_mode, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2390_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2390_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2390][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2390_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2385_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2385_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2385][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2385_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2380_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2380_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2380][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2380_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2375_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2375_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2375][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2375_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_20mhz_2370_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_20mhz_2370_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[LTECX_NVRAM_20M_RSSI_2370][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_20mhz_2370_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2395_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2395_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2395][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2395_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2390_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2390_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2390][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2390_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2385_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2385_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2385][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2385_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2380_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2380_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2380][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2380_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2375_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2375_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2375][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2375_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecx_10mhz_2370_rssi_th) != NULL) {
		array_size = (uint32)getintvararraysize(wlc_hw->vars,
			rstr_ltecx_10mhz_2370_rssi_th);
		for (i = 0; i < array_size; i++)
			wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[LTECX_NVRAM_10M_RSSI_2370][i]
			= (uint32)getintvararray(wlc_hw->vars, rstr_ltecx_10mhz_2370_rssi_th, i);
	}
	if (getvar(wlc_hw->vars, rstr_ltecxmux) != NULL) {
		wlc_hw->ltecx_hw->ltecxmux = (uint32)getintvar(wlc_hw->vars, rstr_ltecxmux);
	}

	wlc->pub->_ltecx = ((wlc_hw->boardflags & BFL_LTECOEX) != 0);
	wlc->pub->_ltecxgci = (wlc_hw->sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT) &&
		(wlc_hw->machwcap & MCAP_BTCX);
	return ltecx;

fail:
	wlc_ltecx_detach(ltecx);
	return NULL;
}

void
BCMATTACHFN(wlc_ltecx_detach)(wlc_ltecx_info_t *ltecx)
{
	wlc_info_t *wlc;

	if (ltecx == NULL)
		return;

	wlc = ltecx->wlc;
	wlc->pub->_ltecx = FALSE;
	wlc_module_unregister(wlc->pub, rstr_ltecx, ltecx);
	MFREE(wlc->osh, ltecx, sizeof(wlc_ltecx_info_t));
}

void
wlc_ltecx_init(wlc_hw_info_t *wlc_hw)
{
	bool ucode_up = FALSE;

	if (wlc_hw->up)
		ucode_up = (R_REG(wlc_hw->osh, &wlc_hw->regs->maccontrol) & MCTL_EN_MAC);

	/* Enable LTE coex in ucode */
	if (wlc_hw->up && ucode_up)
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
	/* Clear history */
	wlc_hw->ltecx_hw->ltecx_enabled			= 0;
	wlc_hw->ltecx_hw->mws_wlanrx_prot_prev	= 0;
	wlc_hw->ltecx_hw->mws_lterx_prot_prev	= 0;
	wlc_hw->ltecx_hw->ltetx_adv_prev		= 0;
	wlc_hw->ltecx_hw->adv_tout_prev			= 0;
	wlc_hw->ltecx_hw->scanjoin_prot_prev	= 0;
	wlc_hw->ltecx_hw->mws_ltecx_txind_prev	= 0;
	wlc_hw->ltecx_hw->mws_wlan_rx_ack_prev	= 0;
	wlc_hw->ltecx_hw->mws_rx_aggr_off		= 0;
	wlc_hw->ltecx_hw->mws_wifi_sensi_prev	= 0;
	wlc_hw->ltecx_hw->mws_im3_prot_prev		= 0;
	wlc_hw->ltecx_hw->mws_wlanrxpri_thresh = 2;
	/* If INVALID, set baud_rate to default */
	if (wlc_hw->ltecx_hw->baud_rate == LTECX_WCI2_INVALID_BAUD)
		wlc_hw->ltecx_hw->baud_rate = LTECX_WCI2_DEFAULT_BAUD;

	/* update look-ahead, baud rate and tx_indication from NVRAM variable */
	if (wlc_hw->ltecx_hw->ltecx_flags) {
		wlc_hw->ltecx_hw->ltetx_adv = (wlc_hw->ltecx_hw->ltecx_flags
					    & LTECX_LOOKAHEAD_MASK)>> LTECX_LOOKAHEAD_SHIFT;
		wlc_hw->ltecx_hw->baud_rate = (wlc_hw->ltecx_hw->ltecx_flags
					    & LTECX_BAUDRATE_MASK)>> LTECX_BAUDRATE_SHIFT;
		wlc_hw->ltecx_hw->mws_ltecx_txind = (wlc_hw->ltecx_hw->ltecx_flags
						  & LTECX_TX_IND_MASK)>> LTECX_TX_IND_SHIFT;
	}

	/* update ltecx parameters based on nvram and lte freq */
	wlc_ltecx_update_status(wlc_hw->wlc);
	wlc_ltecx_check_chmap(wlc_hw->wlc);
	/* update elna status based on the rssi_threshold */
	wlc_ltecx_update_wl_rssi_thresh(wlc_hw->wlc);
	/* Set WLAN Rx prot mode in ucode */
	wlc_ltecx_set_wlanrx_prot(wlc_hw);
	/* Allow WLAN TX during LTE TX based on eLNA bypass status */
	wlc_ltecx_update_wlanrx_ack(wlc_hw->wlc);
	/* update look ahead and protection
	 * advance duration
	 */
	wlc_ltecx_update_ltetx_adv(wlc_hw->wlc);
	/* update lterx protection type */
	wlc_ltecx_update_lterx_prot(wlc_hw->wlc);
	/* update scanjoin protection */
	wlc_ltecx_scanjoin_prot(wlc_hw->wlc);
	/* update ltetx indication */
	wlc_ltetx_indication(wlc_hw->wlc);
	/* update wlanrxpri_thresh */
	wlc_ltecx_set_wlanrxpri_thresh(wlc_hw->wlc);
	if (wlc_hw->up && ucode_up)
		wlc_bmac_enable_mac(wlc_hw);
	/* update ltecx interface information */
	wlc_ltecx_host_interface(wlc_hw->wlc);

}

static int
wlc_ltecx_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_ltecx_info_t *ltecx = (wlc_ltecx_info_t *)ctx;
	wlc_info_t *wlc = (wlc_info_t *)ltecx->wlc;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	int err = 0;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_LTECX_MWS_COEX_BITMAP):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->ltecx_chmap;
		else
			err = BCME_BUFTOOSHORT;
	    break;
	case IOV_SVAL(IOV_LTECX_MWS_COEX_BITMAP):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint16 bitmap;
			bool ucode_up = FALSE;
			bcopy((char *)params, (char *)&bitmap, sizeof(uint16));

			wlc->hw->ltecx_hw->ltecx_chmap = bitmap;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);

			/* Enable LTE coex in ucode */
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_check_chmap(wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WLANRX_PROT):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->mws_wlanrx_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WLANRX_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof (uint16)) {
			uint16 val;
			bool ucode_up = FALSE;

			bcopy((char *)params, (char *)&val, sizeof(uint16));
			if (val > C_LTECX_HOST_PROT_TYPE_AUTO) {
				err = BCME_BADARG;
				break;
			}

			wlc->hw->ltecx_hw->mws_wlanrx_prot = val;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);

			/* Enable wlan rx protection in ucode */
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_set_wlanrx_prot(wlc->hw);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_TXIND):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->mws_ltecx_txind = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltetx_indication(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_WCI2_TXIND):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->mws_ltecx_txind;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WLANRXPRI_THRESH):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->mws_wlanrxpri_thresh;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WLANRXPRI_THRESH):
		if (p_len >= sizeof (uint16))
		{
			uint16 val;
			bcopy((char *)params, (char *)&val, sizeof(uint16));

			if ((uint16)val > 11) {
				err = BCME_BADARG;
				break;
			}
			wlc->hw->ltecx_hw->mws_wlanrxpri_thresh = val;
			wlc_ltecx_set_wlanrxpri_thresh(wlc->hw->wlc);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_CONFIG):
		bcopy((char *)params, &wlc->hw->ltecx_hw->wci2_config,
		sizeof(wci2_config_t));
		break;
	case IOV_GVAL(IOV_LTECX_WCI2_CONFIG):
		bcopy(&wlc->hw->ltecx_hw->wci2_config, (char *)arg,
		sizeof(wci2_config_t));
		break;
	case IOV_SVAL(IOV_LTECX_MWS_PARAMS):
		bcopy((char *)params, &wlc->hw->ltecx_hw->mws_params,
		sizeof(mws_params_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_PARAMS):
		bcopy(&wlc->hw->ltecx_hw->mws_params, (char *)arg,
		sizeof(mws_params_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_FRAME_CONFIG):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->mws_frame_config;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_FRAME_CONFIG):
		if (len >= sizeof(uint16))
			/* 16-bit value */
			wlc->hw->ltecx_hw->mws_frame_config = (uint16)int_val;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_COEX_BAUDRATE):
		if (len >= sizeof(uint16))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->baud_rate;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_COEX_BAUDRATE):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint16 baudrate;
			bcopy((char *)params, (char *)&baudrate, sizeof(uint16));
			if ((baudrate == 2) || (baudrate == 3))
				wlc->hw->ltecx_hw->baud_rate = baudrate;
			else
				err = BCME_BADARG;
		}
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_SCANJOIN_PROT):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->scanjoin_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_SCANJOIN_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->scanjoin_prot = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_scanjoin_prot(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_LTETX_ADV_PROT):
		if (len >= sizeof(uint16))
			*ret_int_ptr = wlc->hw->ltecx_hw->ltetx_adv;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_LTETX_ADV_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint16 lookahead_dur;
			bool ucode_up = FALSE;

			bcopy((uint16 *)params, &lookahead_dur, sizeof(uint16));
			wlc->hw->ltecx_hw->ltetx_adv = lookahead_dur;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_ltetx_adv(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_LTERX_PROT):
		if (len >= sizeof(uint16))
		*ret_int_ptr =  wlc->hw->ltecx_hw->mws_lterx_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_LTERX_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->mws_lterx_prot = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_lterx_prot(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_ELNA_RSSI_THRESH):
		if (len >= sizeof(int32))
			*ret_int_ptr = wlc->hw->ltecx_hw->mws_elna_rssi_thresh;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_ELNA_RSSI_THRESH):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(int32)) {
			int32 rssi_thresh;
			bool ucode_up = FALSE;

			bcopy((int32 *)params, &rssi_thresh, sizeof(int32));
			wlc->hw->ltecx_hw->mws_elna_rssi_thresh = rssi_thresh;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_wl_rssi_thresh(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_IM3_PROT):
		if (len >= sizeof(uint16))
		*ret_int_ptr =  wlc->hw->ltecx_hw->mws_im3_prot;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_IM3_PROT):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(uint16)) {
			uint8 w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(uint8));
			wlc->hw->ltecx_hw->mws_im3_prot = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_ltecx_update_im3_prot(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_WIFI_SENSITIVITY):
		if (len >= sizeof(int))
			*ret_int_ptr = (int) wlc->hw->ltecx_hw->mws_ltecx_wifi_sensitivity;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_WIFI_SENSITIVITY):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(int)) {
			int wifi_sensitivity;
			bool ucode_up = FALSE;

			bcopy((char *)params, (char *)&wifi_sensitivity, sizeof(int));
			wlc->hw->ltecx_hw->mws_ltecx_wifi_sensitivity = wifi_sensitivity;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
				wlc_ltecx_wifi_sensitivity(wlc->hw->wlc);
			if (wlc->hw->up && ucode_up)
				wlc_bmac_enable_mac(wlc->hw);

		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_DEBUG_MSG):
		bcopy((char *)params, &wlc->hw->ltecx_hw->mws_wci2_msg,
		sizeof(mws_wci2_msg_t));
		bool ucode_up = FALSE;
		if (wlc->hw->up)
			ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
				MCTL_EN_MAC);
		if (wlc->hw->up && ucode_up) {
			wlc_bmac_suspend_mac_and_wait(wlc->hw);
				wlc_ltecx_update_debug_msg(wlc->hw->wlc);
				wlc_bmac_enable_mac(wlc->hw);
		} else
			err = BCME_NOTUP;
		break;
	case IOV_GVAL(IOV_LTECX_MWS_DEBUG_MSG):
		bcopy(&wlc->hw->ltecx_hw->mws_wci2_msg, (char *)arg,
		sizeof(mws_wci2_msg_t));
		break;
	case IOV_GVAL(IOV_LTECX_MWS_DEBUG_MODE):
		if (len >= sizeof(int))
		*ret_int_ptr =  wlc->hw->ltecx_hw->mws_debug_mode;
		else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_MWS_DEBUG_MODE):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(int)) {
			int w;
			bool ucode_up = FALSE;

			bcopy((uint8 *)params, &w, sizeof(int));
			wlc->hw->ltecx_hw->mws_debug_mode = w;
			if (wlc->hw->up)
				ucode_up = (R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) &
					MCTL_EN_MAC);
			if (wlc->hw->up && ucode_up) {
				wlc_bmac_suspend_mac_and_wait(wlc->hw);
				wlc_ltecx_update_debug_mode(wlc->hw->wlc);
				wlc_bmac_enable_mac(wlc->hw);
			} else
				err = BCME_NOTUP;
		} else
			err = BCME_BUFTOOSHORT;
		break;
	case IOV_SVAL(IOV_LTECX_WCI2_LOOPBACK):
		if (!BCMLTECOEX_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
		} else if (p_len >= sizeof(wci2_loopback_t)) {
			wci2_loopback_t wci2_loopback;
			uint16 w = 0;
			bcopy((char *)params, (char *)(&wci2_loopback), sizeof(wci2_loopback));
			if (wci2_loopback.loopback_type) {
				/* prevent setting of incompatible loopback mode */
				w = wlc_bmac_read_shm(wlc->hw,
					wlc->hw->btc->bt_shm_addr + M_LTECX_FLAGS);
				if ((w & LTECX_FLAGS_LPBK_MASK) &&
					((w & LTECX_FLAGS_LPBK_MASK) !=
					wci2_loopback.loopback_type)) {
					err = BCME_BADARG;
					break;
				}
				w |= wci2_loopback.loopback_type;
				if (wci2_loopback.loopback_type != LTECX_FLAGS_LPBK_OFF) {
					/* Reuse CRTI code to start test */
					wlc->hw->ltecx_hw->mws_debug_mode = 1;
					/* Init counters */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_WCI2_LPBK_NBYTES_RX,
						htol16(0));
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_WCI2_LPBK_NBYTES_TX,
						htol16(0));
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_WCI2_LPBK_NBYTES_ERR,
						htol16(0));
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_MSG,
						htol16(0));
					/* CRTI_REPEATS=0 presumes Olympic Rx loopback test */
					/* Initialized for Olympic Tx loopback test further below */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_REPEATS,
						htol16(0));
					/* Suppress scans during lpbk */
					wlc->hw->wlc->scan->state |= SCAN_STATE_SUPPRESS;
				}
				if (wci2_loopback.loopback_type == LTECX_FLAGS_LPBKSRC_MASK) {
					/* Set bit15 of CRTI_MSG to distinguish Olympic Tx
					 * loopback test from RIM test
					 */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_MSG,
						0x8000 | wci2_loopback.packet);
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_INTERVAL,
						20); /* TODO: hardcoded for now */
					wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
						M_LTECX_CRTI_REPEATS,
						htol16(wci2_loopback.repeat_ct));
				}
			}
			else {
				/* Lpbk disabled. Reenable scans */
				wlc->hw->wlc->scan->state &= ~SCAN_STATE_SUPPRESS;
				/* CRTI ucode used to start test - now stop test */
				wlc->hw->ltecx_hw->mws_debug_mode = 0;
			}
			wlc_bmac_write_shm(wlc->hw, wlc->hw->btc->bt_shm_addr + M_LTECX_FLAGS, w);
		} else
			err = BCME_BUFTOOSHORT;
	    break;
	case IOV_GVAL(IOV_LTECX_WCI2_LOOPBACK):
	{
		wci2_loopback_rsp_t rsp;
		rsp.nbytes_rx = (int)wlc_bmac_read_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
			M_LTECX_WCI2_LPBK_NBYTES_RX);
		rsp.nbytes_tx = (int)wlc_bmac_read_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
			M_LTECX_WCI2_LPBK_NBYTES_TX);
		rsp.nbytes_err = (int)wlc_bmac_read_shm(wlc->hw, wlc->hw->btc->bt_shm_addr +
			M_LTECX_WCI2_LPBK_NBYTES_ERR);
		bcopy(&rsp, (char *)arg, sizeof(wci2_loopback_rsp_t));
	}
		break;

	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}

static int
wlc_ltecx_watchdog(void *arg)
{
	wlc_ltecx_info_t *ltecx = (wlc_ltecx_info_t *)arg;
	wlc_info_t *wlc = (wlc_info_t *)ltecx->wlc;
	if (BCMLTECOEX_ENAB(wlc->pub)) {
		/* update ltecx parameters based on nvram and lte freq */
		wlc_ltecx_update_status(wlc);
		/* enable/disable ltecx based on channel map */
		wlc_ltecx_check_chmap(wlc);
		/* update elna status based on rssi_thresh
		 * First update the elna status and
		 * update the other flags
		 */
		wlc_ltecx_update_wl_rssi_thresh(wlc);
		/* update protection type */
		wlc_ltecx_set_wlanrx_prot(wlc->hw);
		/* Allow WLAN TX during LTE TX based on eLNA bypass status */
		wlc_ltecx_update_wlanrx_ack(wlc);
		/* update look ahead and protection
		 * advance duration
		 */
		wlc_ltecx_update_ltetx_adv(wlc);
		/* update lterx protection type */
		wlc_ltecx_update_lterx_prot(wlc);
		/* update im3 protection type */
		wlc_ltecx_update_im3_prot(wlc);
		/* update scanjoin protection */
		wlc_ltecx_scanjoin_prot(wlc);
		/* update ltetx indication */
		wlc_ltetx_indication(wlc);
		/* update wlanrxpri_thresh */
		wlc_ltecx_set_wlanrxpri_thresh(wlc);
		/* Check RSSI with WIFI Sensitivity */
		wlc_ltecx_wifi_sensitivity(wlc);
		wlc_ltecx_update_debug_mode(wlc);
	}
	return BCME_OK;
}

void wlc_ltecx_check_chmap(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_hflags, ltecx_state;
	uint16 ltecx_en;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	ltecx_en = 0;	/* IMP: Initialize to disable */
	/* Decide if ltecx algo needs to be enabled */
	if (BAND_2G(wlc->band->bandtype)) {
		/* enable ltecx algo as per ltecx_chmap */
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if (wlc_hw->ltecx_hw->ltecx_chmap & (1 << (chan - 1))) {
			ltecx_en	= 1;
		}
	}

	if (wlc_hw->btc->bt_shm_addr == NULL)
		return;

	ltecx_state = wlc_bmac_read_shm(wlc_hw,
		wlc_hw->btc->bt_shm_addr + M_LTECX_STATE);

	/* Update ucode ltecx flags */
	if (wlc_hw->ltecx_hw->ltecx_enabled != ltecx_en)	{
		if (wlc_hw->btc->bt_shm_addr) {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (ltecx_en == 1) {
				ltecx_hflags |= (1<<C_LTECX_HOST_COEX_EN);
			}
			else  {
				ltecx_hflags &= ~(1<<C_LTECX_HOST_COEX_EN);
				ltecx_state  |= (1<<C_LTECX_ST_IDLE);
				wlc_bmac_write_shm(wlc_hw,
					wlc_hw->btc->bt_shm_addr + M_LTECX_STATE, ltecx_state);
			}
			wlc_bmac_write_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->ltecx_enabled = ltecx_en;
		}
	}
	wlc_hw->ltecx_hw->ltecx_idle = (ltecx_state >> C_LTECX_ST_IDLE) & 0x1;
}

void wlc_ltecx_set_wlanrx_prot(wlc_hw_info_t *wlc_hw)
{
	uint16 prot, shm_val;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	prot = wlc_hw->ltecx_hw->mws_wlanrx_prot;
	/* Set protection to NONE if !2G, or 2G but eLNA bypass */
	if (!BAND_2G(wlc_hw->wlc->band->bandtype) || (wlc_hw->ltecx_hw->mws_elna_bypass)) {
		prot = C_LTECX_MWS_WLANRX_PROT_NONE;
	}
	if (prot != wlc_hw->ltecx_hw->mws_wlanrx_prot_prev)	{
		if (wlc_hw->btc->bt_shm_addr) {
			shm_val = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			/* Clear all protection type bits  */
			shm_val = (shm_val &
				~((1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP)
				|(1 << C_LTECX_HOST_PROT_TYPE_PM_CTS)
				|(1 << C_LTECX_HOST_PROT_TYPE_AUTO)));
			/* Set appropriate protection type bit */
			if (prot == C_LTECX_MWS_WLANRX_PROT_NONE) {
				shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
			} else if (prot == C_LTECX_MWS_WLANRX_PROT_CTS) {
				shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_PM_CTS);
			} else if (prot == C_LTECX_MWS_WLANRX_PROT_PM) {
				shm_val = shm_val | (0 << C_LTECX_HOST_PROT_TYPE_PM_CTS);
			} else if (prot == C_LTECX_MWS_WLANRX_PROT_AUTO) {
				shm_val = shm_val | (1 << C_LTECX_HOST_PROT_TYPE_AUTO);
			}
			wlc_bmac_write_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, shm_val);
			wlc_hw->ltecx_hw->mws_wlanrx_prot_prev = prot;
		}
	}
}

void wlc_ltecx_update_ltetx_adv(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 adv_tout = 0;
	uint16 prot_type;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	/* Update ltetx_adv and adv_tout for 2G band only */
	if (BAND_2G(wlc->band->bandtype))	{
		if (wlc_hw->btc->bt_shm_addr)	{
			if (wlc_hw->ltecx_hw->ltetx_adv != wlc_hw->ltecx_hw->ltetx_adv_prev)	{
				wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
					M_LTECX_TX_LOOKAHEAD_DUR, wlc_hw->ltecx_hw->ltetx_adv);
				wlc_hw->ltecx_hw->ltetx_adv_prev = wlc_hw->ltecx_hw->ltetx_adv;
			}
			/* NOTE: C_LTECX_HOST_PROT_TYPE_CTS may be changed by ucode in AUTO mode */
			prot_type = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
		    if (prot_type & (1 << C_LTECX_HOST_PROT_TYPE_CTS)) {
				if (wlc_hw->ltecx_hw->ltetx_adv >= 500)
				    adv_tout = wlc_hw->ltecx_hw->ltetx_adv - 500;
				else
				    adv_tout = wlc_hw->ltecx_hw->ltetx_adv;
			} else {
			    if (wlc_hw->ltecx_hw->ltetx_adv >= 800)
					adv_tout = wlc_hw->ltecx_hw->ltetx_adv - 800;
				else
					adv_tout = wlc_hw->ltecx_hw->ltetx_adv;
			}
			if (adv_tout != wlc_hw->ltecx_hw->adv_tout_prev)	{
				wlc_bmac_write_shm(wlc_hw,
					wlc_hw->btc->bt_shm_addr + M_LTECX_PROT_ADV_TIME, adv_tout);
				wlc_hw->ltecx_hw->adv_tout_prev = adv_tout;
			}
		}
	}
}

void
wlc_ltecx_update_lterx_prot(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 lterx_prot, ltecx_hflags;

	if (!si_iscoreup (wlc_hw->sih))
		return;

	lterx_prot = 0;
	if (BAND_2G(wlc->band->bandtype))	{
		lterx_prot	= wlc_hw->ltecx_hw->mws_lterx_prot;
	}
	if (lterx_prot != wlc_hw->ltecx_hw->mws_lterx_prot_prev)	{
		if (wlc_hw->btc->bt_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (lterx_prot == 0)
				ltecx_hflags |= (1 << C_LTECX_HOST_TX_ALWAYS);
			else
				ltecx_hflags &= ~(1 << C_LTECX_HOST_TX_ALWAYS);
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->mws_lterx_prot_prev = lterx_prot;
		}
	}
}

void wlc_ltecx_scanjoin_prot(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 scanjoin_prot, ltecx_hflags;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	scanjoin_prot = 0;
	if (BAND_2G(wlc->band->bandtype)) {
		scanjoin_prot = wlc_hw->ltecx_hw->scanjoin_prot;
	}
	if (scanjoin_prot != wlc_hw->ltecx_hw->scanjoin_prot_prev)	{
		if (wlc_hw->btc->bt_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (scanjoin_prot == 0)
				ltecx_hflags &= ~(1 << C_LTECX_HOST_SCANJOIN_PROT);
			else
				ltecx_hflags |= (1 << C_LTECX_HOST_SCANJOIN_PROT);
			wlc_bmac_write_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->scanjoin_prot_prev = scanjoin_prot;
		}
	}
}

void wlc_ltetx_indication(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_txind, ltecx_hflags;

	if (!si_iscoreup(wlc_hw->sih))
		return;

	ltecx_txind = 0;
	if (BAND_2G(wlc->band->bandtype)) {
		ltecx_txind = wlc_hw->ltecx_hw->mws_ltecx_txind;
	}
	if (ltecx_txind != wlc_hw->ltecx_hw->mws_ltecx_txind_prev)	{
		if (wlc_hw->btc->bt_shm_addr)	{
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS);
			if (ltecx_txind == 0)
				ltecx_hflags &= ~(1 << C_LTECX_HOST_TXIND);
			else
				ltecx_hflags |= (1 << C_LTECX_HOST_TXIND);
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
					+ M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->mws_ltecx_txind_prev = ltecx_txind;
		}
	}
}

void wlc_ltecx_host_interface(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_mode, ltecx_hflags;

	ltecx_mode = wlc_hw->ltecx_hw->ltecxmux &
		LTECX_MUX_MODE_MASK >> LTECX_MUX_MODE_SHIFT;

	ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
		wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);

	if (BCMLTECOEXGCI_ENAB(wlc_hw->wlc->pub))
	{
		if (ltecx_mode == LTECX_MUX_MODE_GPIO) {
			/* Enabling ERCX */
			si_ercx_init(wlc_hw->sih, wlc_hw->ltecx_hw->ltecxmux);
			ltecx_hflags |= (1 << C_LTECX_HOST_INTERFACE);
		} else {
			/* Enable LTECX WCI-2 UART interface */
			si_wci2_init(wlc_hw->sih, wlc_hw->ltecx_hw->baud_rate,
				wlc_hw->ltecx_hw->ltecxmux);
			ltecx_hflags &= ~(1 << C_LTECX_HOST_INTERFACE);
		}
	}
	else {
		/* Enable LTECX ERCX interface */
		si_ercx_init(wlc_hw->sih, wlc_hw->ltecx_hw->ltecxmux);
		ltecx_hflags |= (1 << C_LTECX_HOST_INTERFACE);
	}
	/* Run Time Interface Config */
	wlc_bmac_write_shm(wlc_hw,
		wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS, ltecx_hflags);

}

void wlc_ltecx_set_wlanrxpri_thresh(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 thrsh, shm_val;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	thrsh = wlc_hw->ltecx_hw->mws_wlanrxpri_thresh;
	shm_val = wlc_bmac_read_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				    + M_LTECX_RXPRI_THRESH);
	/* Set to NONE if !2G */
	if (!BAND_2G(wlc->band->bandtype)) {
		shm_val = 0;
	}
	else {
		shm_val = thrsh;
	}
	wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr + M_LTECX_RXPRI_THRESH, shm_val);
}

int wlc_ltecx_get_lte_status(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (wlc_hw->ltecx_hw->ltecx_enabled && !wlc_hw->ltecx_hw->ltecx_idle)
		return 1;
	else
		return 0;
}
int wlc_ltecx_get_lte_map(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (BAND_2G(wlc->band->bandtype) && (BCMLTECOEX_ENAB(wlc->pub))) {
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if ((wlc_hw->ltecx_hw->ltecx_enabled) &&
			(wlc_hw->ltecx_hw->ltecx_chmap & (1 << (chan - 1))))
			return 1;
	}
	return 0;
}
void wlc_ltecx_update_wl_rssi_thresh(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	int ltecx_rssi_thresh, thresh_lmt;
	if (!si_iscoreup (wlc_hw->sih))
		return;
	if (BAND_2G(wlc->band->bandtype)) {
		if (wlc_hw->btc->bt_shm_addr && wlc_ltecx_get_lte_status(wlc) &&
			(wlc->cfg->link->rssi != WLC_RSSI_INVALID)) {
			ltecx_rssi_thresh = wlc_hw->ltecx_hw->mws_elna_rssi_thresh;
			/* mws_ltecx_rssi_thresh_lmt will be zero the very first time
			 * to make the init time decision if we are entering the hysterisis from
			 * lower RSSI side or higher RSSI side (w.r.t. RSSI thresh)
			 */
			thresh_lmt = wlc_hw->ltecx_hw->mws_ltecx_rssi_thresh_lmt;
			if (wlc->cfg->link->rssi >= (ltecx_rssi_thresh + thresh_lmt)) {
				wlc_hw->ltecx_hw->mws_elna_bypass = TRUE;
				wlc_hw->ltecx_hw->mws_ltecx_rssi_thresh_lmt =
					wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram;
			} else if (wlc->cfg->link->rssi < (ltecx_rssi_thresh - thresh_lmt)) {
				wlc_hw->ltecx_hw->mws_elna_bypass = FALSE;
				wlc_hw->ltecx_hw->mws_ltecx_rssi_thresh_lmt =
					wlc_hw->ltecx_hw->ltecx_rssi_thresh_lmt_nvram;
			}
		} else
			wlc_hw->ltecx_hw->mws_elna_bypass = FALSE;
	}
}

void
wlc_ltecx_update_wlanrx_ack(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 wlan_rx_ack, ltecx_hflags;

	if (!si_iscoreup (wlc_hw->sih))
		return;

	wlan_rx_ack = 0;
	if (BAND_2G(wlc->band->bandtype))   {
		wlan_rx_ack  = wlc_hw->ltecx_hw->mws_elna_bypass;
	}
	if (wlan_rx_ack != wlc_hw->ltecx_hw->mws_wlan_rx_ack_prev)    {
		if (wlc_hw->btc->bt_shm_addr)   {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
			wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (wlan_rx_ack == 1)
				ltecx_hflags |= (1 << C_LTECX_HOST_RX_ACK);
			else
				ltecx_hflags &= ~(1 << C_LTECX_HOST_RX_ACK);
			wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS, ltecx_hflags);
			wlc_hw->ltecx_hw->mws_wlan_rx_ack_prev = wlan_rx_ack;
		}
	}
}
int wlc_ltecx_chk_elna_bypass_mode(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (BAND_2G(wlc->band->bandtype) && (BCMLTECOEX_ENAB(wlc->pub))) {
		chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);
		if ((wlc_hw->ltecx_hw->ltecx_enabled) &&
			(wlc_hw->ltecx_hw->ltecx_chmap & (1 << (chan - 1)))) {
			if (wlc_hw->ltecx_hw->mws_elna_bypass == TRUE)
				return 1;
			else
				return 0;
		}
	}
	return 0;
}

void wlc_ltecx_update_status(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	int i, ch, index, lte_freq_half_bw = 0, freq, coex_chmap = 0, freq_index = 0;
	int	wlanrx_prot_min_ch = 0, lterx_prot_min_ch = 0, scanjoin_prot_min_ch = 0;
	int wlanrx_prot_info = 0, lterx_prot_info = 0, scan_join_prot_info = 0;
	chanspec_t chan = CHSPEC_CHANNEL(wlc->chanspec);

	if (!wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT] &&
		!wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT] &&
		!wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_LTERX_PROT] &&
		!wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_LTERX_PROT])
		return;

	if ((wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw !=
		wlc_hw->ltecx_hw->lte_channel_bw_prev)||
		(wlc_hw->ltecx_hw->mws_params.mws_tx_center_freq !=
		wlc_hw->ltecx_hw->lte_center_freq_prev))
	{
		wlc_hw->ltecx_hw->lte_channel_bw_prev =
			wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw;
		wlc_hw->ltecx_hw->lte_center_freq_prev =
			wlc_hw->ltecx_hw->mws_params.mws_tx_center_freq;

		if (wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw == LTE_CHANNEL_BW_20MHZ)	{
			wlanrx_prot_info =
				wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT];
			lterx_prot_info =
				wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_LTERX_PROT];
			scan_join_prot_info =
				wlc_hw->ltecx_hw->ltecx_20mhz_modes_prot[LTECX_NVRAM_SCANJOIN_PROT];
			lte_freq_half_bw = LTE_20MHZ_INIT_STEP;
		} else if (wlc_hw->ltecx_hw->mws_params.mws_tx_channel_bw == LTE_CHANNEL_BW_10MHZ) {
			wlanrx_prot_info =
				wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_WLANRX_PROT];
			lterx_prot_info =
				wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_LTERX_PROT];
			scan_join_prot_info =
				wlc_hw->ltecx_hw->ltecx_10mhz_modes_prot[LTECX_NVRAM_SCANJOIN_PROT];
			lte_freq_half_bw = LTE_10MHZ_INIT_STEP;
		} else
			return;

		for (freq = (LTE_BAND40_MAX_FREQ - lte_freq_half_bw);
					freq > LTE_BAND40_MIN_FREQ;
						freq = freq- LTE_FREQ_STEP_SIZE) {
			if ((wlc_hw->ltecx_hw->mws_params.mws_tx_center_freq +
				LTE_MAX_FREQ_DEVIATION) >= freq)
				break;
			freq_index ++;
		}
		if (freq_index < LTE_FREQ_STEP_MAX) {
			wlanrx_prot_min_ch =  (wlanrx_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
			lterx_prot_min_ch =  (lterx_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
			scanjoin_prot_min_ch =  (scan_join_prot_info >>
				(freq_index * LTECX_NVRAM_GET_PROT_MASK)) & LTECX_MIN_CH_MASK;
		}
		wlc_hw->ltecx_hw->mws_wlanrx_prot_min_ch = wlanrx_prot_min_ch;
		wlc_hw->ltecx_hw->mws_lterx_prot_min_ch = lterx_prot_min_ch;
		wlc_hw->ltecx_hw->mws_scanjoin_prot_min_ch = scanjoin_prot_min_ch;
		wlc_hw->ltecx_hw->mws_lte_freq_index = freq_index;

		ch = (wlanrx_prot_min_ch >= lterx_prot_min_ch)
					? wlanrx_prot_min_ch: lterx_prot_min_ch;
		for (i = 0; i < ch; i++)
			coex_chmap |= (1<<i);
		/* update coex_bitmap */
		wlc_hw->ltecx_hw->ltecx_chmap = coex_chmap;
	}

	wlanrx_prot_min_ch = wlc_hw->ltecx_hw->mws_wlanrx_prot_min_ch;
	lterx_prot_min_ch = wlc_hw->ltecx_hw->mws_lterx_prot_min_ch;
	scanjoin_prot_min_ch = wlc_hw->ltecx_hw->mws_scanjoin_prot_min_ch;

	/* update wlanrx protection */
	if (wlanrx_prot_min_ch && chan <= wlanrx_prot_min_ch)
		wlc_hw->ltecx_hw->mws_wlanrx_prot = 1;
	else
		wlc_hw->ltecx_hw->mws_wlanrx_prot = 0;
	/* update lterx protection */
	if (lterx_prot_min_ch && chan <= lterx_prot_min_ch)
		wlc_hw->ltecx_hw->mws_lterx_prot = 1;
	else
		wlc_hw->ltecx_hw->mws_lterx_prot = 0;
	/* update scanjoin protection */
	if (scanjoin_prot_min_ch && chan <= scanjoin_prot_min_ch)
		wlc_hw->ltecx_hw->scanjoin_prot = 1;
	else
		wlc_hw->ltecx_hw->scanjoin_prot = 0;
	/* update wl rssi threshold */
	index = wlc_hw->ltecx_hw->mws_lte_freq_index;
	if (wlc_hw->ltecx_hw->lte_channel_bw_prev == LTE_CHANNEL_BW_20MHZ) {
		if (index < LTECX_NVRAM_MAX_RSSI_PARAMS)
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh =
				wlc_hw->ltecx_hw->ltecx_rssi_thresh_20mhz_bw[index][chan-1];
		else
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh = 0;
	} else if (wlc_hw->ltecx_hw->lte_channel_bw_prev == LTE_CHANNEL_BW_10MHZ) {
		if (index < LTECX_NVRAM_MAX_RSSI_PARAMS)
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh =
				wlc_hw->ltecx_hw->ltecx_rssi_thresh_10mhz_bw[index][chan-1];
		else
			wlc_hw->ltecx_hw->mws_elna_rssi_thresh = 0;
	}
}

void
wlc_ltecx_update_im3_prot(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_hflags = 0;

	if (!si_iscoreup (wlc_hw->sih))
		return;

	if (BAND_2G(wlc->band->bandtype) && (wlc_hw->btc->bt_shm_addr) &&
		(wlc_hw->ltecx_hw->mws_im3_prot != wlc_hw->ltecx_hw->mws_im3_prot_prev)) {
			ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
			if (wlc_hw->ltecx_hw->mws_im3_prot)
				ltecx_hflags |= (1 << C_LTECX_HOST_PROT_TXRX);
			else
				ltecx_hflags &= ~(1 << C_LTECX_HOST_PROT_TXRX);
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
			+ M_LTECX_HOST_FLAGS, ltecx_hflags);
		wlc_hw->ltecx_hw->mws_im3_prot_prev = wlc_hw->ltecx_hw->mws_im3_prot;
	}
}

void
wlc_ltecx_wifi_sensitivity(wlc_info_t * wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	int ltecx_hflags;

	if (!si_iscoreup (wlc_hw->sih))
	return;

	if (BAND_2G(wlc->band->bandtype)) {
		if (wlc_hw->btc->bt_shm_addr && wlc_hw->ltecx_hw->mws_wlanrx_prot) {

			if (wlc->cfg->link->rssi > wlc_hw->ltecx_hw->mws_ltecx_wifi_sensitivity)
				wlc_hw->ltecx_hw->mws_wifi_sensi = 1;
			else
				wlc_hw->ltecx_hw->mws_wifi_sensi = 0;

			if (wlc_hw->ltecx_hw->mws_wifi_sensi !=
				wlc_hw->ltecx_hw->mws_wifi_sensi_prev) {
				ltecx_hflags = wlc_bmac_read_shm(wlc_hw,
				wlc_hw->btc->bt_shm_addr + M_LTECX_HOST_FLAGS);
				if (wlc_hw->ltecx_hw->mws_wifi_sensi)
					ltecx_hflags |= (1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
				else
					ltecx_hflags &= ~(1 << C_LTECX_HOST_PROT_TYPE_NONE_TMP);
				wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr
				+ M_LTECX_HOST_FLAGS, ltecx_hflags);
				wlc_hw->ltecx_hw->mws_wifi_sensi_prev =
					wlc_hw->ltecx_hw->mws_wifi_sensi;
			}
		}
	}
}

void wlc_ltecx_update_debug_msg(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	if (BAND_2G(wlc->band->bandtype) && (wlc_hw->btc->bt_shm_addr)) {
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
			M_LTECX_CRTI_MSG, wlc_hw->ltecx_hw->mws_wci2_msg.mws_wci2_data);
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
			M_LTECX_CRTI_INTERVAL, wlc_hw->ltecx_hw->mws_wci2_msg.mws_wci2_interval);
		wlc_bmac_write_shm(wlc_hw, wlc_hw->btc->bt_shm_addr +
			M_LTECX_CRTI_REPEATS, wlc_hw->ltecx_hw->mws_wci2_msg.mws_wci2_repeat);
	}
}

#define GCI_INTMASK_RXFIFO_NOTEMPTY 0x4000
void wlc_ltecx_update_debug_mode(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16 ltecx_state;
	if (!si_iscoreup(wlc_hw->sih))
		return;

	if (BAND_2G(wlc->band->bandtype) && (wlc_hw->btc->bt_shm_addr)) {
		ltecx_state = wlc_bmac_read_shm(wlc_hw,
			wlc_hw->btc->bt_shm_addr + M_LTECX_STATE);
		if (wlc_hw->ltecx_hw->mws_debug_mode) {
			/* Disable Inband interrupt for FrmSync */
			si_gci_indirect(wlc_hw->sih, 0x00010,
			OFFSETOF(chipcregs_t, gci_inbandeventintmask),
			0x00000001, 0x00000000);
			/* Enable Inband interrupt for Aux Valid bit */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_intmask),
				GCI_INTMASK_RXFIFO_NOTEMPTY, GCI_INTMASK_RXFIFO_NOTEMPTY);
			/* Route Rx-data through RXFIFO */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_rxfifo_common_ctrl),
				ALLONES_32, 0xFF00);
			ltecx_state |= (1 << C_LTECX_ST_CRTI_DEBUG_MODE_TMP);
		} else {
			/* Enable Inband interrupt for FrmSync */
			si_gci_indirect(wlc_hw->sih, 0x00010,
			OFFSETOF(chipcregs_t, gci_inbandeventintmask),
			0x00000001, 0x00000001);
			/* Disable Inband interrupt for Aux Valid bit */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_intmask),
				GCI_INTMASK_RXFIFO_NOTEMPTY, 0);
			/* Route Rx-data through AUX register */
			si_gci_direct(wlc_hw->sih, OFFSETOF(chipcregs_t, gci_rxfifo_common_ctrl),
				ALLONES_32, 0xFF);
			ltecx_state &= ~(1 << C_LTECX_ST_CRTI_DEBUG_MODE_TMP);
		}
		wlc_bmac_write_shm(wlc_hw,
			wlc_hw->btc->bt_shm_addr + M_LTECX_STATE, ltecx_state);
	}
}
/* LTE coex definitions */

void
wlc_ltecx_assoc_in_prog(wlc_info_t *wlc, int val)
{
	uint16 bt_shm_addr = 2 * wlc_bmac_read_shm(wlc->hw, M_BTCX_BLK_PTR);
	uint16 ltecxHostFlag = wlc_bmac_read_shm(wlc->hw, bt_shm_addr + M_LTECX_HOST_FLAGS);
	if (val == TRUE)
		ltecxHostFlag |= (1 << C_LTECX_HOST_ASSOC_PROG);
	else
		ltecxHostFlag &= (~(1 << C_LTECX_HOST_ASSOC_PROG));
	wlc_bmac_write_shm(wlc->hw, bt_shm_addr + M_LTECX_HOST_FLAGS, ltecxHostFlag);
}

#endif /* BCMLTECOEX */
