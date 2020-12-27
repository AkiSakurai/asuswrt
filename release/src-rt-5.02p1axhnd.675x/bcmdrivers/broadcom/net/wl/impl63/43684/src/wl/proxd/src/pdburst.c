/*
 * Proxd internal interface - burst manager
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
 * $Id: pdburst.c 780098 2019-10-15 13:37:23Z $
 */

#include <wlc_cfg.h>

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <802.11.h>
#include <bcmevent.h>
#include <wlioctl.h>
#include <bcmwifi_channels.h>

#include <osl.h>
#include <wl_dbg.h>
#include <siutils.h>

#include <wlc_pub.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_hrt.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scan_utils.h>
#include <wl_export.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <hndpmu.h>
#include <wlc_pcb.h>
#include <wlc_lq.h>

#include <wlc_pdsvc.h>
#include <wlc_pddefs.h>
#include <wlc_pdmthd.h>

#include "pdsvc.h"
#include "pdftm.h"
#include "pdburst.h"
#include "pdftmpvt.h"

#include <phy_rxgcrs_api.h>
#include <phy_tof_api.h>

#ifdef WL_RANGE_SEQ
#include <phy_ac_misc.h>
#endif /* WL_RANGE_SEQ */

/* #define TOF_DEBUG_UCODE 1 */
/* #define TOF_DEBUG_TIME */
/* #define TOF_DEBUG_TIME2 */
/* #define TOF_PROFILE */
/* #define TOF_KVALUE_CAL */

#define TOF_DFREE_SCAN		1
#define TOF_DFREE_TXDONE	2
#define TOF_DFREE_PWR		4
#define TOF_DFREE_INSVCCB	8
#define TOF_RX_PROCESS_MASK	1

#if defined(TOF_DBG) || defined(TOF_COLLECT)
#include <sbchipc.h>
#endif /* TOF_DBG || TOF_COLLECT */

#ifdef TOF_PROFILE
#define TOF_DEBUG_TIME2
#endif // endif

#define TOF_VER			1
#ifdef TOF_DEBUG_FTM
#ifdef EVENT_LOG_COMPILE
#define MDBG(x) do { \
	if (EVENT_LOG_IS_LOG_ON(EVENT_LOG_TAG_PROXD_INFO)) \
		EVENT_LOG_COMPACT_CAST_PAREN_ARGS(EVENT_LOG_TAG_PROXD_INFO, x); \
	} while (0)
#else /* EVENT_LOG_COMPILE */
#define MDBG(x)  printf x /* dbg messages for non-timing sensetive code  */
#endif /* EVENT_LOG_COMPILE */
#else
#define MDBG(x)
#endif /* TOF_DEBUG_FTM */

#ifdef WL_RANGE_SEQ
#ifdef FTM_SESSION_CHANSWT
#define BURST_NOCHANSWT(flags)		((flags) & WL_PROXD_SESSION_FLAG_NOCHANSWT)
#else
#define BURST_NOCHANSWT(flags)		TRUE
#endif /* FTM_SESSION_CHANSWT */
#endif /* WL_RANGE_SEQ */

#define BURST_HZ_PICO 1000 /* nano second resolution */
#define BURST_ERR_PICO (BURST_HZ_PICO >> 1) /* 0.5 nano sec */

struct pdburst_sm;
typedef struct pdburst_sm	pdburst_sm_t;

typedef struct ftmts {
	uint32	t1;
	uint32	t2;
	uint32	t3;
	uint32	t4;
	int32	gd;
	int32	adj;
	uint32	rspec;
	wl_proxd_rssi_t	rssi;
	bool	discard;
	uint8	tx_id;
	wl_proxd_snr_t  snr;
	wl_proxd_bitflips_t  bitflips;
	wl_proxd_phy_error_t tof_phy_error;
} ftmts_t;

typedef	struct tof_tslist {
	uint16	tscnt;
	ftmts_t	*tslist;
} tof_tslist_t;

struct pdburst;
typedef struct pdburst	pdburst_t;
#ifdef WL_RANGE_SEQ
#define MAX_COLLECT_COUNT	2
#else
#define MAX_COLLECT_COUNT	5
#endif /* WL_RANGE_SEQ */

typedef struct pdburst_collect {
	bool				remote_request;
	bool				remote_collect;
	int16				remote_cnt;
	int16				collect_cnt;
	int16				collect_size;
	uint32				*collect_buf;
	pdburst_t *			pdburstp;
	wl_proxd_collect_header_t	*collect_header;
	wl_proxd_collect_data_t		*collect;
	wl_proxd_collect_info_t		collect_info;
	uint32				*chan;
	uint8				ri_rr[FTM_TPK_RI_RR_LEN_SECURE_2_0];
	pdburst_config_t		*configp;
	wl_proxd_collect_method_t	collect_method;
} pdburst_collect_t;

#ifdef TOF_COLLECT
static pdburst_collect_t *pdburst_collect = NULL;
#endif /* TOF_COLLECT */

enum  {
	TOF_SEQ_NONE		= 0,
	TOF_SEQ_STARTED		= 1,
	TOF_SEQ_DONE		= 2,
	TOF_SEQ_LAST		= 3
};

typedef int8 pdburst_seq_state_t;

/* TOF method data object */
struct pdburst {
	void				*ctx; /* session ctx */
	const pdburst_callbacks_t	*svc_funcs;
	wlc_bsscfg_t			*bsscfg;
	wlc_info_t			*wlc;
	pdburst_sm_t			*sm;
	uint8				txcnt;
	uint8				rxcnt;
	uint8				measurecnt;
	uint8				totalfrmcnt;
	uint16				frame_type_cnt[FRAME_TYPE_NUM];
	uint8				adj_type_cnt[TOF_ADJ_TYPE_NUM];
	struct wl_timer			*timer;
	wlc_hrt_to_t			*duration_timer;
	wlc_hrt_to_t			*ftm_tx_timer;
	bool				ftm_tx_timer_active;
	bool				duration_timer_active;
	bool				timeractive;
	bool				caldone;
	uint16				tofcmd;
	tof_tslist_t			tof_tslist;
	uint64				tx_t1;
	uint64				tx_t4;
	int32				distance;
	uint32				meanrtt;
	uint64				Tq;
	uint32				oldavbrx;
	uint32				oldavbtx;
	uint32				chipnum;
	uint32				chiprev;
	uint16				shmemptr;
	struct ether_addr		allow_mac;
	int32				var3;
	wlc_bsscfg_t			*scanbsscfg;
	uint8				noscanengine;
	bool				lastburst;
	pdburst_config_t		*configp;
	wl_proxd_params_tof_tune_t	*tunep;
	uint8				phyver;
	uint8				frmcnt;
	int8				avgrssi;
	uint8				scanestarted;
	uint8				smstoped;
	bool				destroyed;
	uint8				delayfree;
	bool				seq_en;
	int32				seq_len;
	uint32				flags;
	uint16				sdrtt;
	ftmts_t				*lasttsp; /* previous burst time stamps */
	bool				seq_started;
	pdburst_collect_t		*collectp;
	tof_pbuf_t			tof_pbuf[TOF_PROFILE_BUF_SIZE];
	bool				channel_dumped;
	uint16				result_flags;

	/* num measurement frames recv from peer (ignoring retries) */
	uint8				num_meas;
	uint8				core;
	pdburst_seq_state_t		seq_state;
	uint8				*mf_buf;
	uint16				mf_buf_len;
	uint8				*mf_stats_buf;
	uint16				mf_stats_buf_len;
	bool				randmac_in_use;
	uint32				ftm_rx_rspec; /* valid only only initiator */
	uint32				ack_rx_rspec;
	uint64				phy_deaf_start;
	uint8				pad[3];
};

#define BURST_SEQ_EN(_bp) ((_bp)->seq_en != 0)
#define BURST_IS_VHTACK(_bp) (((_bp)->flags & WL_PROXD_SESSION_FLAG_VHTACK) != 0)

#ifdef WL_RANGE_SEQ
#define BURST_SEQ_EN_SET(_bp, v) ((_bp)->seq_en = (v))
#define BURST_FLAG_SEQ_EN(_bp) (((_bp)->flags & WL_PROXD_SESSION_FLAG_SEQ_EN) != 0)
#endif /* WL_RANGE_SEQ */

/* RSSI Proximity state machine parameters */
struct pdburst_sm {
	uint8			tof_mode;
	uint8			tof_txcnt;
	uint8			tof_rxcnt;
	uint8			tof_state;
	struct ether_addr	tof_peerea;
	struct ether_addr	tof_selfea;
	pdburst_t		*tof_obj;
	wl_proxd_status_t	tof_reason;
	uint8			tof_retrycnt;
	uint8			tof_txpktcnt;
	bool			tof_txvht;
	bool			tof_rxvht;
	uint16			phyctl0;
	uint16			phyctl1;
	uint16			phyctl2;
	uint16			lsig;
	uint16			vhta0;
	uint16			vhta1;
	uint16			vhta2;
	uint16			vhtb0;
	uint16			vhtb1;
	uint16			ampductl;
	uint16			ampdudlim;
	uint16			ampdulen;
	uint8			tof_legacypeer;
	uint8			tof_followup;
	uint8			tof_dialog;
	uint32			toa; /* timestamp of arrival of FTM or ACK */
	uint32			tod; /* timestamp of departure of FTM or ACK */
};

enum tof_type {
	TOF_TYPE_REQ_END		= 0,
	TOF_TYPE_REQ_START		= 1,
	TOF_TYPE_MEASURE_END		= 2,
	TOF_TYPE_MEASURE		= 3,
	TOF_TYPE_COLLECT_REQ_END	= 4,
	TOF_TYPE_COLLECT_REQ_START	= 5,
	TOF_TYPE_COLLECT_DATA_END	= 6,
	TOF_TYPE_COLLECT_DATA		= 7,
	TOF_TYPE_LAST			= 8
};

enum tof_event {
	TOF_EVENT_WAKEUP	= 0,
	TOF_EVENT_RXACT		= 1,
	TOF_EVENT_TMO		= 2,
	TOF_EVENT_ACKED		= 3,
	TOF_EVENT_NOACK		= 4,
	TOF_EVENT_WAITMTMO	= 5,
	TOF_EVENT_COLLECT_REQ	= 6,
	TOF_EVENT_LAST		= 7
};

enum tof_ret {
	TOF_RET_SLEEP	= 0,
	TOF_RET_ALIVE	= 1,
	TOF_RET_IGNORE	= 2,
	TOF_RET_END		= 3
};

typedef struct pdburst_data {
	uint8			tof_type;
	int8			tof_rssi;
	uint32			tof_rspec;
	struct ether_addr	tof_srcea;
	struct ether_addr	tof_dstea;
} pdburst_data_t;

#ifdef TOF_COLLECT_REMOTE

#include<packed_section_start.h>

/* Declarations of collect debug header */
typedef	BWL_PRE_PACKED_STRUCT pdburst_collect_frm {
	uint8	category;
	uint8	OUI[3];
	uint8	type;		/* BRCM_FTM_VS_AF_TYPE */
	uint8	subtype;	/* BRCM_FTM_VS_COLLECT_SUBTYPE */
	uint8	tof_type;	/* packet type */
	uint8	index;		/* data index: 0 is header, others is data */
	uint16	length;		/* data length */
	uint8	data[1];	/* collect data */
} BWL_POST_PACKED_STRUCT pdburst_collect_frm_t;

#include<packed_section_end.h>

#endif /* TOF_COLLECT_REMOTE */

#define PROTOCB(_burst, _func, _args) (((_burst)->svc_funcs && \
	(_burst)->svc_funcs->_func) ? (*(_burst)->svc_funcs->_func)_args :\
	BCME_UNSUPPORTED)

#ifdef TOF_DEBUG_TIME2
static uint32 tsf_start, tsf_hi, tsf_scanstart, tsf_txreq, tsf_rxack;
static uint32 tsf_rxm, tsf_tmo, tsf_lastm, tsf_confirm;
#endif // endif

static int pdburst_send(pdburst_sm_t *sm, struct ether_addr *da, uint8 type);

static int pdburst_sm(pdburst_sm_t *sm, int event, const pdburst_tsinfo_t *protp,
	int paramlen, pdburst_data_t *datap);
static void pdburst_measure(pdburst_t *tofobj, int cmd);
static int pdburst_confirmed(pdburst_sm_t *sm, wl_proxd_status_t reason);
static int pdburst_target_done(pdburst_sm_t *sm, wl_proxd_status_t reason);
static void
pdburst_mf_stats_init_event(
	wl_proxd_event_t * event,
	wl_proxd_session_id_t sid);
static void pdburst_send_mf_stats_event(
	pdburst_t * burstp);
#if defined(TOF_COLLECT) || defined(TOF_COLLECT_REMOTE)
static void
pdburst_collect_prep_header(pdburst_collect_t *collectp,
	wl_proxd_collect_header_t *header);
#endif /* TOF_COLLECT || TOF_COLLECT_REMOTE */
#ifdef TOF_COLLECT
static int pdburst_collect_event_log(pdburst_collect_t *collect);
static int pdburst_collect_generic_event(pdburst_t *burstp);
static int pdburst_collect_event(pdburst_t *burstp);
#endif /* TOF_COLLECT */

#ifdef TOF_COLLECT_INLINE
typedef enum tof_collect_inline_type
{
	TOF_COLLECT_INLINE_HEADER = 1,
	TOF_COLLECT_INLINE_FRAME_INFO = 2,
	TOF_COLLECT_INLINE_FRAME_INFO_CHAN_DATA = 3,
	TOF_COLLECT_INLINE_RESULTS = 4,
	TOF_COLLECT_INLINE_MAX
} tof_collect_inline_type_t;

#define TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD	512U /* bytes */

#define TOF_COLLECT_INLINE_HEADER_INFO_VER		TOF_COLLECT_INLINE_HEADER_INFO_VER_1
#define TOF_COLLECT_INLINE_RESULTS_VER		TOF_COLLECT_INLINE_RESULTS_VER_1
#define TOF_COLLECT_INLINE_FRAME_INFO_VER		TOF_COLLECT_INLINE_FRAME_INFO_VER_2

static int pdburst_collect_inline_results(pdburst_t *pburst, pdburst_results_t *res);
static int pdburst_collect_inline_header_info_event(pdburst_t *pburst);
static wl_proxd_event_t *pdburst_collect_inline_event_alloc(pdburst_t *pburst, uint8 type);
static uint32 pdburst_collect_get_buf_size(uint8 type);
static int pdburst_collect_inline_frame_info_data_events(pdburst_t *pburst, ftmts_t * list,
	uint8 frame_cnt);
static int pdburst_collect_inline_frame_info_chan_data(pdburst_t *pburst,
	uint32 *chan_data, uint32 data_size);
#endif /* TOF_COLLECT_INLINE */

#ifdef WL_PROXD_UCODE_TSYNC
static void pdburst_clear_ucode_ack_block(wlc_info_t *wlc);
#endif /* WL_PROXD_UCODE_TSYNC */

/* convert tof frame type to pdburst frame type */
pdburst_frame_type_t pdburst_get_ftype(uint8 type)
{
	pdburst_frame_type_t ftype;
	ASSERT(type <= TOF_TYPE_MEASURE);
	if (type > TOF_TYPE_MEASURE) {
		WL_ERROR(("%s()- Unknown tof type %d\n", __FUNCTION__, type));
	}

	if (type == TOF_TYPE_REQ_START || type == TOF_TYPE_REQ_END) {
		ftype = PDBURST_FRAME_TYPE_REQ;
	} else {
		ftype = PDBURST_FRAME_TYPE_MEAS; /* both TOF_TYPE_MEASURE/MEASURE_END */
	}
	return ftype;
}

/* API to get the chanspec from OTA rspec & configured chanspec
* Basically...configured BW and over the air frame/ack BW can be different.
* so we update the BW of configured chanspec with OTA ones
*/
static chanspec_t
pdburst_get_chspec_from_rspec(pdburst_t *burstp, ratespec_t rspec)
{
	chanspec_t config_chanspec = burstp->configp->chanspec;
	chanspec_t chanspec = 0;

	if ((rspec & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_80MHZ) {
		chanspec = (config_chanspec & ~(WL_CHANSPEC_BW_MASK)) |
			WL_CHANSPEC_BW_80;
	} else if ((rspec & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_40MHZ) {
		chanspec = (config_chanspec & ~(WL_CHANSPEC_BW_MASK)) |
			WL_CHANSPEC_BW_40;
	} else if ((rspec & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_20MHZ) {
		chanspec = (config_chanspec & ~(WL_CHANSPEC_BW_MASK)) |
			WL_CHANSPEC_BW_20;
	}

	return chanspec;
}

#ifdef WL_PROXD_UCODE_TSYNC
/* API to derive ACK rspec from TXStatus ack block..
* Refer below confluence for details
* http://confluence.broadcom.com/display/WLAN/NewUcodeInterfaceForProxdFeature
*/
static ratespec_t
pdburst_get_tgt_ackrspec(pdburst_t *burstp, uint16 ucodeack)
{
	uint8 bw, type, leg_rate;
	ratespec_t ackrspec = 0;
	ucodeack = (ucodeack & FTM_TXSTATUS_ACK_RSPEC_BLOCK_MASK);
	bw = (ucodeack >> FTM_TXSTATUS_ACK_RSPEC_BW_SHIFT) &
		FTM_TXSTATUS_ACK_RSPEC_BW_MASK;

	if (bw == FTM_TXSTATUS_ACK_RSPEC_BW_20) {
		ackrspec |= WL_RSPEC_BW_20MHZ;
	} else if (bw == FTM_TXSTATUS_ACK_RSPEC_BW_40) {
		ackrspec |= WL_RSPEC_BW_40MHZ;
	} else if (bw == FTM_TXSTATUS_ACK_RSPEC_BW_80) {
		ackrspec |= WL_RSPEC_BW_80MHZ;
	} else if (bw == FTM_TXSTATUS_ACK_RSPEC_BW_160) {
		ackrspec |= WL_RSPEC_BW_160MHZ;
	}

	type = (ucodeack >> FTM_TXSTATUS_ACK_RSPEC_TYPE_SHIFT) &
		FTM_TXSTATUS_ACK_RSPEC_TYPE_MASK;

	leg_rate = (ucodeack >> FTM_TXSTATUS_ACK_RSPEC_RATE_SHIFT) &
		FTM_TXSTATUS_ACK_RSPEC_RATE_MASK;
#ifdef TOF_DEBUG_UCODE
	FTM_ERR(("\n ucodeack 0x%04x bw 0x%02x type 0x%02x leg_rate 0x%02x\n",
		ucodeack, bw, type, leg_rate));
#endif // endif
	if (type == FTM_TXSTATUS_ACK_RSPEC_TYPE_LEG) {
		if (FTM_TXSTATUS_ACK_RSPEC_RATE_6M(leg_rate)) {
			ackrspec |= WLC_RATE_6M;
		} else { /* any non-6mps is fine */
			ackrspec |= WLC_RATE_24M;
		}
	} else if (type == FTM_TXSTATUS_ACK_RSPEC_TYPE_HT) {
		/* TODO..add non-mcs0 support */
		ackrspec |= HT_RSPEC(0);
	} else if (type == FTM_TXSTATUS_ACK_RSPEC_TYPE_VHT) {
		ackrspec |= VHT_RSPEC(0, FTM_NSS);
	} else if (type == FTM_TXSTATUS_ACK_RSPEC_TYPE_HE) {
		ASSERT(0);
	} else if (type == FTM_TXSTATUS_ACK_RSPEC_TYPE_CCK) {
		ASSERT(0);
	}

#ifdef TOF_DEBUG_UCODE
	FTM_ERR(("\n ackrspec 0x%08x\n", ackrspec));
#endif // endif

	return ackrspec;
}
#endif /* WL_PROXD_UCODE_TSYNC */

/* API to get subband index */
static uint8
pdburst_get_subband_idx(pdburst_t *burstp, ratespec_t rspec)
{
	wlc_info_t *wlc = burstp->wlc;
	uint8 idx = 0;
	chanspec_t chanspec = pdburst_get_chspec_from_rspec(burstp, rspec);
	if (CHSPEC_BW(chanspec) !=
		CHSPEC_BW(WLC_BAND_PI_RADIO_CHANSPEC)) {
		/* initiator and target bandwidth is different */
		if (CHSPEC_IS80(WLC_BAND_PI_RADIO_CHANSPEC)) {
			/* target is 80 */
			if (CHSPEC_IS40(chanspec)) {
				idx = WL_PROXD_80M_40M;
			} else {
				idx = WL_PROXD_80M_20M;
			}
		} else if (CHSPEC_IS40(WLC_BAND_PI_RADIO_CHANSPEC)) {
			if (CHSPEC_IS20(chanspec)) {
				idx = WL_PROXD_40M_20M;
			}
		}
	}

	return idx;
}

/* get K value */
static uint32
pdburst_get_kval(pdburst_t *burstp, bool initiator, bool seq_en)
{
	wlc_info_t *wlc = burstp->wlc;
	uint32 k;
	ratespec_t ftm_ratespec, ackrspec;
	chanspec_t ftm_chanspec, ackchanspec;
	uint8 bw_idx = 0;
	uint16 rate_idx = 0;

	/* FTM config values */
	ftm_ratespec = burstp->configp->ratespec;
	ftm_chanspec = burstp->configp->chanspec;

	FTM_TRACE(("Radio-chanpec %x, burst-config-chanspec %x\n",
		WLC_BAND_PI_RADIO_CHANSPEC, ftm_chanspec));

	/* overwrite with actual OTA values & derive ack ratespec/chanspec */
	if (initiator) {
		if ((burstp->flags & WL_PROXD_SESSION_FLAG_ONE_WAY) ||
			(burstp->sm->tof_legacypeer == TOF_LEGACY_AP)) {
			return 0;
		}
		ftm_ratespec = burstp->ftm_rx_rspec;
		ftm_chanspec = pdburst_get_chspec_from_rspec(burstp, ftm_ratespec);
		if (BURST_IS_VHTACK(burstp) && RSPEC_ISVHT(ftm_ratespec)) {
			ackrspec = VHT_RSPEC(0, FTM_NSS) | (ftm_ratespec & WL_RSPEC_BW_MASK);
		} else if (RSPEC_ISHT(ftm_ratespec)) {
			ackrspec = HT_RSPEC(0) | (ftm_ratespec & WL_RSPEC_BW_MASK);
		} else {
			/* We will transmit ACK same as the FTM frame and upto 24Mbps */
			uint8 leg_ack_rate = (ftm_ratespec & WL_RSPEC_LEGACY_RATE_MASK);
			if (leg_ack_rate > WLC_RATE_24M) {
				leg_ack_rate = WLC_RATE_24M;
			}
			ackrspec = leg_ack_rate | (ftm_ratespec & WL_RSPEC_BW_MASK);
		}
		k = burstp->tunep->Ki;
		bw_idx = pdburst_get_subband_idx(burstp, ftm_ratespec);
	} else {
		ackchanspec = pdburst_get_chspec_from_rspec(burstp,
			burstp->ack_rx_rspec);
		ackrspec = burstp->ack_rx_rspec;
		k = burstp->tunep->Kt;
		bw_idx = pdburst_get_subband_idx(burstp, burstp->ack_rx_rspec);
	}

	rate_idx = proxd_get_ratespec_idx(ftm_ratespec, ackrspec);
	if (!k) {
		if (initiator) {
			wlc_phy_kvalue(WLC_PI(wlc), WLC_BAND_PI_RADIO_CHANSPEC,
				rate_idx, &k, NULL, (seq_en ? WL_PROXD_SEQEN : 0) | bw_idx);
			TOF_PRINTF(("ftm-cspec:%x k:%d ftm-rspec:%x idx:%d rate-idx:%d\n",
				ftm_chanspec, k, ftm_ratespec, bw_idx, rate_idx));
		} else {
			wlc_phy_kvalue(WLC_PI(wlc), WLC_BAND_PI_RADIO_CHANSPEC,
				rate_idx, NULL, &k, (seq_en ? WL_PROXD_SEQEN : 0) | bw_idx);
			TOF_PRINTF(("ack-cspec:%x k:%d ack-rspec:%x idx:%d rate-idx:%d\n",
				ackchanspec, k, ackrspec, bw_idx, rate_idx));
		}
	} else {
		if (initiator) {
			TOF_PRINTF(("ftm-cspec:%x k:%d ftm-rspec:%x idx:%d rate-idx:%d\n",
				ftm_chanspec, k, ftm_ratespec, bw_idx, rate_idx));
		} else {
			TOF_PRINTF(("ack-cspec:%x k:%d ack-rspec:%x idx:%d rate-idx:%d\n",
				ackchanspec, k, ackrspec, bw_idx, rate_idx));
		}
	}

	return k;
}

/* Get total frame count */
static uint8 pdburst_total_framecnt(pdburst_t *burstp)
{
	if (BURST_SEQ_EN(burstp))
		return 0;
	if (burstp->tunep->totalfrmcnt)
		return burstp->tunep->totalfrmcnt;
	if (burstp->totalfrmcnt)
		return burstp->totalfrmcnt;
	return 0;
}

/* Enable VHT ACK  */
static void pdburst_tof_init_vht(pdburst_t *burstp, int len_bytes)
{
	pdburst_sm_t *sm = burstp->sm;
	chanspec_t chanspec = burstp->configp->chanspec;
	int	dot11_bw, dbps, vht_sig_a1, vht_sig_a2, vht_sig_b1;
	int	frame_len, ampdu_len, ampdu_len_wrds;
	int	n_sym, vht_length, vht_pad;
	int	n_ampdu_delim, n_ampdu_eof;

#ifdef TOF_DEBUG_UCODE
	TOF_PRINTF(("%s: Setting up SHM values for TOF VHT ACK\n", __FUNCTION__));
#endif // endif
	if (CHSPEC_IS80(chanspec)) {
		dot11_bw = 2;
		dbps = 117;
		vht_sig_b1 = 3 << 5;
	} else if (CHSPEC_IS40(chanspec)) {
		dot11_bw = 1;
		dbps = 54;
		vht_sig_b1 = 3 << 3;
	} else if (CHSPEC_IS20(chanspec)) {
		dot11_bw = 0;
		dbps = 26;
		vht_sig_b1 = 7 << 1;
	} else
		return;

	vht_sig_a1 = dot11_bw | (1<<2) | (63 << 4) | (1<<23);
	vht_sig_a2 = (1 << 9);

	frame_len = len_bytes + 4;
	ampdu_len = len_bytes + 8;
	ampdu_len_wrds = 4 * ((ampdu_len + 3) / 4);
#ifdef TOF_DEBUG_UCODE
	TOF_PRINTF(("vht_sig_a1 0x%08x, vht_sig_a2 0x%08x, frame_len %d, words %d\n",
		vht_sig_a1, vht_sig_a2, frame_len, ampdu_len_wrds));
#endif // endif

	n_sym = ((8 * ampdu_len + 22) + dbps - 1) / dbps;
	vht_length = (n_sym * dbps - 22) / 8;
	vht_pad = (vht_length > ampdu_len_wrds)? vht_length - ampdu_len_wrds : 0;

#ifdef TOF_DEBUG_UCODE
	TOF_PRINTF(("LEN %d => AMPDU LEN %d => NSYMS %d => %d bytes => %d pad\n",
		len_bytes, ampdu_len, n_sym, vht_length, vht_pad));
#endif // endif
	n_ampdu_delim = vht_pad >> 2;
	n_ampdu_eof = vht_pad & 3;
#ifdef TOF_DEBUG_UCODE
	TOF_PRINTF(("n_ampdu_delim =%d, n_ampdu_eof = %d\n", n_ampdu_delim, n_ampdu_eof));
#endif // endif
	if ((n_ampdu_delim == 0) && (ampdu_len & 3) && (vht_length > ampdu_len_wrds)) {
		int adj = 4 - (ampdu_len & 3);
		n_ampdu_eof += (adj & 3);
#ifdef TOF_DEBUG_UCODE
		TOF_PRINTF(("Adjust: adj = %d, n_ampdu_eof = %d\n", adj, n_ampdu_eof));
#endif // endif
	}

	if (D11REV_GE(burstp->wlc->pub->corerev, 80)) {
		sm->phyctl0 = PHY_TXC_FT_VHT | D11_REV80_PHY_TXC_NON_SOUNDING;
		sm->phyctl1 = dot11_bw;
		sm->phyctl2 = (1 << (burstp->core));
	} else
	{
		sm->phyctl0 = (dot11_bw << 14) | 7 | (1 << (burstp->core + 6));
		sm->phyctl1 = 0;
		sm->phyctl2 = 0;
	}
	sm->lsig = 0xb | (((n_sym + 5)*3 - 3) << 5);
	sm->vhta0 = (vht_sig_a1 & 0xffff);
	sm->vhta1 = ((vht_sig_a2 & 0xff) << 8) | ((vht_sig_a1 >> 16) & 0xff);
	sm->vhta2 = (vht_sig_a2 >> 8) & 0xffff;
	sm->vhtb0 = ((ampdu_len + 3)/4) & 0xffff;
	sm->vhtb1 = vht_sig_b1;
	sm->ampductl = (n_ampdu_eof << 8) | (1 << 1);
	sm->ampdudlim = n_ampdu_delim;
	sm->ampdulen = (frame_len << 4) | 1;

#ifdef TOF_DEBUG_UCODE
	TOF_PRINTF(("phyctl0: 0x%04x, phyctl1: 0x%04x, phyctl2: 0x%04x, lsig: 0x%04x\n",
		sm->phyctl0, sm->phyctl1, sm->phyctl2, sm->lsig));
	TOF_PRINTF(("vhta0: 0x%04x, vhta1: 0x%04x, vhta2: 0x%04x\n",
		sm->vhta0, sm->vhta1, sm->vhta2));
	TOF_PRINTF(("vhtb0: 0x%04x, vhtb1: 0x%04x\n",
		sm->vhtb0, sm->vhtb1));
	TOF_PRINTF(("ampductl: 0x%04x, ampdudlim: 0x%04x, ampdulen: 0x%04x\n",
		sm->ampductl, sm->ampdudlim, sm->ampdulen));
#endif // endif
}

static void pdburst_tof_setup_ht(pdburst_t *burstp)
{
}

/* Setup VHT rate */
static void pdburst_tof_setup_vht(pdburst_t *burstp)
{
	pdburst_sm_t *sm = burstp->sm;
	wlc_info_t *wlc	= burstp->wlc;
	if (wlc_read_shm(wlc, burstp->shmemptr + M_TOF_PHYCTL0_OFFSET(wlc)) != sm->phyctl0) {
#ifdef TOF_DEBUG_UCODE
		TOF_PRINTF(("%s: Writing SHM values for TOF VHT ACK\n", __FUNCTION__));
#endif // endif
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_PHYCTL0_OFFSET(wlc), sm->phyctl0);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_PHYCTL1_OFFSET(wlc), sm->phyctl1);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_PHYCTL2_OFFSET(wlc), sm->phyctl2);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_LSIG_OFFSET(wlc), sm->lsig);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_VHTA0_OFFSET(wlc), sm->vhta0);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_VHTA1_OFFSET(wlc), sm->vhta1);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_VHTA2_OFFSET(wlc), sm->vhta2);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_VHTB0_OFFSET(wlc), sm->vhtb0);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_VHTB1_OFFSET(wlc), sm->vhtb1);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_AMPDU_CTL_OFFSET(wlc), sm->ampductl);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_AMPDU_DLIM_OFFSET(wlc), sm->ampdudlim);
		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_AMPDU_LEN_OFFSET(wlc), sm->ampdulen);
	}
}

#ifdef TOF_SEQ_DBG
int gTofSeqCnt = 0;
#endif // endif

/* Called when entering/exiting tof measurement mode */
static void pdburst_hw(pdburst_t *burstp, bool enter, bool tx)
{
	wlc_info_t *wlc = burstp->wlc;

#ifdef TOF_SEQ_DBG
	gTofSeqCnt = 0;
#endif // endif

#ifdef WL_PROXD_SEQ
	if (enter) {
		if (BURST_SEQ_EN(burstp)) {
			phy_tof_seq_params(WLC_PI(wlc), TRUE);
			phy_tof_seq_upd_dly(WLC_PI(wlc), tx, (int)burstp->core, FALSE);
		}
	}
#endif // endif

	wlc_phy_tof(WLC_PI(wlc), enter, tx, burstp->tunep->hw_adj, BURST_SEQ_EN(burstp),
		(int)burstp->core, burstp->tunep->emu_delay);
}

/* adjustment to timestamp */
static int
pdburst_rtd_adj(pdburst_t *burstp, int frame_type, int frame_bw, int cfo,
	bool sw_adj_en, bool hw_adj_en, bool seq_en,
	int32 *gd, int32 *adj, bool initiator, uint8 ts_id)
{
	wlc_info_t *wlc = burstp->wlc;
	uint32 *tbl_value, chan_size = 0;
	struct tof_rtd_adj_params params;
	int adj_err = BCME_OK, nfft, nbuffer, nlen, nlen_1;

	int32* tmpH = NULL;
	uint32* tmpR = NULL;
	int status = BCME_OK, bandtype = wlc->band->bandtype;

	uint8* ri_rr = NULL;
#ifdef TOF_COLLECT
	int n_out = 0;
#ifdef RSSI_REFINE
	int max, max_index = -1, peak_offset, new_index, scalar, imp_average = 0, t = 0;
#endif // endif
#endif /* TOF_COLLECT */
	uint32 *chan_data = NULL;
	uint32 chan_data_len = 0;

	uint32* p_collect_data = NULL;
	nlen_1 = 0;
	BCM_REFERENCE(adj_err);

#if defined(WL_RANGE_SEQ) && defined(TOF_COLLECT) && defined(RSSI_REFINE)
	BCM_REFERENCE(max);
	BCM_REFERENCE(max_index);
	BCM_REFERENCE(peak_offset);
	BCM_REFERENCE(new_index);
	BCM_REFERENCE(scalar);
	BCM_REFERENCE(imp_average);
	BCM_REFERENCE(t);
#endif /* WL_RANGE_SEQ && TOF_COLLECT && RSSI_REFINE */

#ifdef TOF_DEBUG
	TOF_PRINTF(("func %s burstp %p frame_type %x bw %x cfo %x sw_adj %x hw_adj %x\n",
		__FUNCTION__, OSL_OBFUSCATE_BUF(burstp),  frame_type, frame_bw,  cfo,
			sw_adj_en,	hw_adj_en));
	TOF_PRINTF(("pdburst_rtd_adj() seq_started %d num_meas %d tof_txcnt %d "
		"seq_en %x gd %x adj %x initiator %x frame_type %d\n",
		burstp->seq_started, burstp->num_meas, burstp->sm->tof_txcnt,
		seq_en, *gd,  *adj,  initiator, frame_type));
#endif // endif
	if (BURST_SEQ_EN(burstp) &&
		(!burstp->seq_started) &&
		((burstp->num_meas % TOF_DEFAULT_FTMCNT_SEQ == 1) || /* initiator */
		!(burstp->sm->tof_txcnt % TOF_DEFAULT_FTMCNT_SEQ  ))) { /* target */
		burstp->seq_started = TRUE;
		burstp->result_flags |= WL_PROXD_LTFSEQ_STARTED;
		pdburst_measure(burstp, initiator? TOF_RX : TOF_RESET);
		return BCME_OK;
	}

	if (burstp->configp->chanspec == WLC_BAND_PI_RADIO_CHANSPEC)
		/* initiator and target use same chanspec */
		params.subband = PRXS_SUBBAND_20LL;
	else
		params.subband = ((uint32)frame_bw) >> 16;
	frame_bw = frame_bw & 0xffff;

	if (CHSPEC_IS80(burstp->configp->chanspec))
#ifdef TOF_SEQ_20_IN_80MHz
		nfft = ((NFFT_BASE * 2) << frame_bw);
#else
		nfft = (NFFT_BASE << frame_bw);
#endif // endif
	else {
		if (BURST_SEQ_EN(burstp))
			nfft = ((NFFT_BASE * 2) << frame_bw);
		else
			nfft = (NFFT_BASE << frame_bw);
	}

	if (CHSPEC_IS80(WLC_BAND_PI_RADIO_CHANSPEC))
		nlen = 256;
	else if (CHSPEC_IS40(WLC_BAND_PI_RADIO_CHANSPEC))
		nlen = 128;
	else {
#ifdef TOF_SEQ_20MHz_BW_512IFFT
		if (BURST_SEQ_EN(burstp)) {
			nlen = nfft;
			nlen_1 = 384;
		}
		else
#endif // endif
		nlen = 64;
	}

	/* Applicable only to 4375 & !SEQ */
	if (!BURST_SEQ_EN(burstp) && D11REV_IS(wlc->pub->corerev, 82) &&
		D11MINORREV_IS(wlc->pub->corerev, 2)) {
		uint8 bw_idx;
		if (initiator) {
			bw_idx = pdburst_get_subband_idx(burstp, burstp->ftm_rx_rspec);
		} else {
			bw_idx = pdburst_get_subband_idx(burstp, burstp->ack_rx_rspec);
		}
		if (bw_idx == WL_PROXD_80M_40M) {
			nlen = 128;
		} else if (bw_idx == WL_PROXD_80M_20M ||
			bw_idx == WL_PROXD_40M_20M) {
			nlen = 64;
		}
	}
	nbuffer = nlen + 16;

	tbl_value = (uint32 *)MALLOC(wlc->osh, 2 * (nbuffer + nlen_1) * sizeof(uint32));
	if (!tbl_value) {
		return BCME_NOMEM;
	}
#ifdef TOF_COLLECT_INLINE
	/* chan_data allocation status is checked in the place
	 * where it is used because of multiple collection paths
	 */
	chan_data_len = 2 * nlen * sizeof(uint32);
	chan_data = (uint32 *)MALLOCZ(wlc->osh, chan_data_len);
#endif /* TOF_COLLECT_INLINE */
	params.bw = (BANDWIDTH_BASE << frame_bw);
	params.w_len = burstp->tunep->w_len[frame_bw];
	params.w_offset = burstp->tunep->w_offset[frame_bw];
	params.gd_ns = 0;
	params.adj_ns = 0;
	params.H = (int32*)tbl_value;
	params.w_ext = NULL;
	params.gd_shift = !BURST_SEQ_EN(burstp);
	params.p_A = NULL;
	if (CHIPID(wlc->pub->sih->chip) == 0 /* BCM43014_CHIP_ID */) {
		params.tdcs_en = TRUE;
	} else {
		/* Using chanest memory for AUX slice */
		params.tdcs_en = BAND_5G(bandtype) ? TOF_DEFAULT_TDCS_EN : FALSE;
	}

	params.longwin_en = TOF_DEFAULT_LONGWIN_EN;
	if (params.longwin_en) {
		params.w_offset = params.w_len << 1;
	}

	if (params.w_len > nfft)
		params.w_len = nfft;

	*gd = *adj = 0;

#ifdef TOF_COLLECT
	if (burstp->collectp) {
		p_collect_data = burstp->collectp->collect_buf;
		ri_rr = burstp->collectp->ri_rr;
	}
#endif // endif

	if (seq_en) {
#ifdef WL_RANGE_SEQ
		if (frame_bw == TOF_BW_20MHZ_INDEX) {
			if (CHSPEC_IS5G(WLC_BAND_PI_RADIO_CHANSPEC)) {
#else
		if ((frame_bw == TOF_BW_20MHZ_INDEX) &&
			(CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC) > 14)) {
#endif /* WL_RANGE_SEQ */
			params.thresh_scale[0] = burstp->tunep->seq_5g20.N_tx_scale;
			params.thresh_log2[0] = burstp->tunep->seq_5g20.N_tx_log2;
			params.thresh_scale[1] = burstp->tunep->seq_5g20.N_rx_scale;
			params.thresh_log2[1] = burstp->tunep->seq_5g20.N_rx_log2;
			params.w_len = burstp->tunep->seq_5g20.w_len;
			params.w_offset = burstp->tunep->seq_5g20.w_offset;
			} else {
#ifdef WL_RANGE_SEQ
				params.thresh_scale[0] = burstp->tunep->seq_2g20.N_tx_scale;
				params.thresh_log2[0] = burstp->tunep->seq_2g20.N_tx_log2;
				params.thresh_scale[1] = burstp->tunep->seq_2g20.N_rx_scale;
				params.thresh_log2[1] = burstp->tunep->seq_2g20.N_rx_log2;
				params.w_len = burstp->tunep->seq_2g20.w_len;
				params.w_offset = burstp->tunep->seq_2g20.w_offset;
			}
		} else {
#endif /* WL_RANGE_SEQ */
			int i;
			for (i = 0; i <  2; i++) {
				params.thresh_scale[i] = burstp->tunep->N_scale[i + TOF_BW_NUM];
				params.thresh_log2[i] = burstp->tunep->N_log2[i + TOF_BW_NUM];
			}
		}

		chan_size = (K_TOF_COLLECT_CHAN_SIZE >> (2 - frame_bw));
		TOF_PRINTF(("frame_bw = %d, chan_size = %d.\n", frame_bw, chan_size));
		if (!burstp->collectp) {
			TOF_PRINTF(("Allocating temp mem ----->\n"));
			tmpR = (uint32*)MALLOCZ(burstp->wlc->osh,
					(PHY_CORE_MAX + 1) * chan_size * sizeof(uint32));
			if (tmpR == NULL) {
				TOF_PRINTF(("*** Malloc error for channel estimates.*** \n"));
				return BCME_NOMEM;
			}
		} else {
			tmpR = (uint32 *) (burstp->collectp->chan);
		}
		if (tmpR != NULL) {
			adj_err = phy_tof_chan_freq_response(WLC_PI(burstp->wlc),
				chan_size, CORE0_K_TOF_H_BITS, tmpH, NULL, tmpR,
				TRUE, 1, TRUE);
			burstp->channel_dumped = TRUE;
			if (adj_err == BCME_NOCLK) {
				TOF_PRINTF(("wlc_hw clk %d\n", burstp->wlc->hw->clk));
			}
		}

		/*
		int i = 0;
		TOF_PRINTF(("%p\n", (void *) OSL_OBFUSCATE_BUF(tmpR)));
		TOF_PRINTF(("%p\n", (void *) OSL_OBFUSCATE_BUF(burstp->collectp->chan)));
		for(i = 0; i < 8; i++) {
			TOF_PRINTF(("(%p: %u) ",
				(void *) (OSL_OBFUSCATE_BUF(tmpR) + i), *(tmpR + i)));
		}
		TOF_PRINTF(("\n"));
		*/
		if (!burstp->collectp) {
			MFREE(burstp->wlc->osh, tmpR,
				(PHY_CORE_MAX + 1) * chan_size * sizeof(uint32));
		}

#ifdef TOF_DBG_SEQ_PHY_SEC
		TOF_PRINTF(("%s : channel dump \n", __FUNCTION__));
#endif /* TOF_DBG_SEQ_PHY_SEC */
		if (adj_err) {
			TOF_PRINTF(("error = %d\n", adj_err));
		}
		hw_adj_en = FALSE;
		sw_adj_en = FALSE;
	} else {
		if (frame_bw == TOF_BW_20MHZ_INDEX && !RSPEC_ISVHT(burstp->configp->ratespec) &&
			CHSPEC_IS2G(WLC_BAND_PI_RADIO_CHANSPEC)) {
			/* 2g/20M channels without VHT rate */
			params.thresh_log2[1] = burstp->tunep->N_log2_2g;
			params.thresh_scale[1] = burstp->tunep->N_scale_2g;
		} else {
			params.thresh_log2[1] = burstp->tunep->N_log2[frame_bw];
			params.thresh_scale[1] = burstp->tunep->N_scale[frame_bw];
		}
		if (hw_adj_en) {
			adj_err = wlc_phy_chan_mag_sqr_impulse_response(WLC_PI(wlc), frame_type,
				params.w_len, params.w_offset, CHNSM_K_TOF_H_BITS, params.H,
				(int *)&params.gd_ns, p_collect_data, burstp->shmemptr);
			if (adj_err == BCME_OK) {
				params.w_ext = params.H;
#ifdef TOF_COLLECT
				n_out = params.w_len + K_TOF_COLLECT_H_PAD;
#endif // endif
			} else {
				hw_adj_en = FALSE;
			}
		}

		if (!hw_adj_en && sw_adj_en) {
			adj_err = phy_tof_chan_freq_response(WLC_PI(wlc), nlen,
				CORE0_K_TOF_H_BITS, params.H, NULL, p_collect_data,
				TRUE, 1, FALSE);
			if (adj_err == BCME_OK) {
#ifdef TOF_COLLECT
				n_out = nlen;
#endif // endif
			} else {
				sw_adj_en = FALSE;
			}
		}
	}

#ifdef TOF_DBG_SEQ
		/* In debug mode, this prevents the Sequence Triggering again and */
		/* overwriting the sample capture buffer on the reception of next frame */
		burstp->seq_started = FALSE;
		/*
			if(burstp->wlc->clk)
				wlc_write_shm(burstp->wlc, burstp->shmemptr + M_TOF_UCODE_SET, 0);
		*/
#endif // endif

	pdburst_measure(burstp, initiator? TOF_RX : TOF_RESET);

	if (burstp->seq_en) {
		int adj1;
		adj1 = (int)pdburst_get_kval(burstp, initiator, TRUE);

		/* retreive bit flip and SNR threshold from tunep */
		params.bitflip_thresh = burstp->tunep->bitflip_thresh;
		params.snr_thresh = burstp->tunep->snr_thresh;

		status = wlc_phy_seq_ts(WLC_PI(wlc), nbuffer, tbl_value,
			(initiator ? 0 : 1), cfo, adj1, (void*)&params,
			&params.adj_ns, &burstp->seq_len, p_collect_data,
			ri_rr, burstp->tunep->smooth_win_en);
		if (status) {
			seq_en = FALSE;
		}
		if (!initiator && burstp->seq_state == TOF_SEQ_DONE) {
			burstp->seq_started = FALSE;
		}
#ifdef TOF_COLLECT
		n_out = 2*(nbuffer + K_TOF_COLLECT_H_PAD);
#endif // endif
	} else if (hw_adj_en || sw_adj_en) {
		if (tof_rtd_adj(wlc, &params, chan_data, &chan_data_len) != BCME_OK) {
#ifdef TOF_DEBUG_TIME
			TOF_PRINTF(("$$$ tof_rtd_adj failed $$$\n"));
#endif // endif
			hw_adj_en = FALSE;
			sw_adj_en = FALSE;
		}
	}

	if (seq_en)
		burstp->adj_type_cnt[TOF_ADJ_SEQ]++;
	else if (hw_adj_en)
		burstp->adj_type_cnt[TOF_ADJ_HARDWARE]++;
	else if (sw_adj_en)
		burstp->adj_type_cnt[TOF_ADJ_SOFTWARE]++;

	*gd = params.gd_ns;
	*adj = params.adj_ns;

#ifdef TOF_COLLECT
	if (p_collect_data) {
		burstp->collectp->collect_info.nfft =  n_out;
		if (seq_en)
			burstp->collectp->collect_info.type = TOF_ADJ_SEQ;
		else if (hw_adj_en)
			burstp->collectp->collect_info.type = TOF_ADJ_HARDWARE;
		else if (sw_adj_en)
			burstp->collectp->collect_info.type = TOF_ADJ_SOFTWARE;
		burstp->collectp->collect_info.gd_adj_ns = *gd;
		burstp->collectp->collect_info.gd_h_adj_ns = *adj;
	}
#endif /* TOF_COLLECT */
#ifdef TOF_COLLECT_INLINE
	if (chan_data && chan_data_len)
		pdburst_collect_inline_frame_info_chan_data(burstp, chan_data, chan_data_len);
	if (chan_data)
		MFREE(wlc->osh, chan_data, chan_data_len);
#endif /* TOF_COLLECT_INLINE */
	MFREE(wlc->osh, tbl_value, 2 * (nbuffer + nlen_1) * sizeof(uint32));
	return (hw_adj_en || sw_adj_en || seq_en) ? BCME_OK : BCME_ERROR;
}

/* transmit command to ucode */
static bool pdburst_cmd(wlc_info_t *wlc, uint shmemptr, uint16 cmd)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	d11regs_t *regs = wlc_hw->regs;
	int i = 0;

	/* Wait until last command completes */
	while ((R_REG(wlc_hw->osh, D11_MACCOMMAND_ALTBASE(regs, wlc->regoffsets)) & MCMD_TOF) &&
		(i < TOF_MCMD_TIMEOUT)) {
		OSL_DELAY(1);
		i++;
	}

	if (R_REG(wlc_hw->osh, D11_MACCOMMAND_ALTBASE(regs, wlc->regoffsets)) & MCMD_TOF) {
		FTM_ERR(("TOF ucode cmd timeout; maccommand: 0x%p\n",
			D11_MACCOMMAND_ALTBASE(regs, wlc->regoffsets)));
		return FALSE;
	}

	wlc_write_shm(wlc, shmemptr + M_TOF_CMD_OFFSET(wlc), cmd);

	W_REG(wlc_hw->osh, D11_MACCOMMAND_ALTBASE(regs, wlc->regoffsets), MCMD_TOF);

#ifdef TOF_DEBUG
	TOF_PRINTF(("cmd = %x c1f %x c7f %x\n", cmd, wlc_read_shm(wlc, (0xc1f * 2)),
		wlc_read_shm(wlc, (0xc7f * 2))));
	for (i = 0; i < 10; i++) {
		TOF_PRINTF(("TOF_DBG%d %02x  \n", i+1, wlc_read_shm(wlc, ((0x888 + i) * 2))));
	}
#endif // endif

	if (cmd & TOF_RX) {
		/* Inform ucode to process FTM meas frames on initiator */
		wlc_write_shm(wlc, shmemptr + M_TOF_FLAGS_OFFSET(wlc), 1);
	}
	else {
		wlc_write_shm(wlc, shmemptr + M_TOF_FLAGS_OFFSET(wlc), 0);
	}

	return TRUE;
}

/* send measurement cmd to ucode */
static void pdburst_measure(pdburst_t *burstp, int cmd)
{
	wlc_info_t *wlc = burstp->wlc;
	uint16 tof_cmd = TOF_RESET;
#ifdef TOF_DEBUG_TIME
	uint64 phy_deaf_end;
	uint32 ftm_sep;
	uint32 diff;
#endif /* TOF_DEBUG_TIME */

	ASSERT(wlc->hw != NULL);
	if (wlc->hw == NULL) {
		return;
	}

	if ((!wlc->hw->clk) || (wlc->pub->hw_off)) {
		WL_ERROR(("pdburst_measure() clk %d , hw_off %d\n",
			wlc->hw->clk, wlc->pub->hw_off));
		return;
	}

#ifdef WL_RANGE_SEQ
	if (BURST_SEQ_EN(burstp) && burstp->seq_started)
#else
	if (BURST_SEQ_EN(burstp) /* && burstp->seq_started */)
#endif // endif
		tof_cmd |= (1 << TOF_SEQ_SHIFT);

	burstp->tofcmd = 0xdead;
	if (pdburst_cmd(wlc, burstp->shmemptr, tof_cmd)) {
	  phy_tof_cmd(WLC_PI(wlc), BURST_SEQ_EN(burstp), burstp->tunep->emu_delay);
#ifdef WL_RANGE_SEQ
		burstp->tofcmd = tof_cmd;
#endif /* WL_RANGE_SEQ */
#if defined(TOF_SEQ_DBG)
		if (BURST_SEQ_EN(burstp)) {
			gTofSeqCnt++;
			if (gTofSeqCnt > 1)
				cmd = 0;
		}
#endif // endif
		if (cmd == TOF_RX) {
			tof_cmd |= TOF_RX;
			if ((burstp->configp != NULL) && !BURST_SEQ_EN(burstp)) {
				if (RSPEC_ISVHT(burstp->configp->ratespec) &&
					BURST_IS_VHTACK(burstp)) {
					tof_cmd |= (1 << TOF_VHT_ACK_SHIFT);
				}
			}
			if (pdburst_cmd(wlc, burstp->shmemptr, tof_cmd))
				burstp->tofcmd = tof_cmd;
		}
	}
	else {
	  phy_tof_cmd(WLC_PI(wlc), FALSE, 0);
	}

#ifdef TOF_DEBUG_TIME
	/* phy is undefed with phy_tof_cmd() above */
	phy_deaf_end = OSL_SYSUPTIME_US();

	ftm_sep = burstp->configp ? FTM_INTVL2USEC(&burstp->configp->ftm_sep) : 0;
	diff = phy_deaf_end - burstp->phy_deaf_start;
	if (ftm_sep && burstp->phy_deaf_start &&
		(burstp->sm->tof_mode == WL_PROXD_MODE_INITIATOR)) {
		if (diff > ftm_sep) {
			WL_ERROR(("ftp-phydeaf: phy is deaf for(%uus) for more than "
				"sep intv(%dus)\n", diff, ftm_sep));
		}
		TOF_PRINTF(("ftm-phydeaf: phy is deaf form %u to %u(%uus), sep intv(%dus)\n",
			(uint32)burstp->phy_deaf_start, (uint32)phy_deaf_end, diff, ftm_sep));
	}

	/* reset phy deaf start time */
	burstp->phy_deaf_start = 0;
#endif /* TOF_DEBUG_TIME */
}

/* Get AVB time stamp */
static void pdburst_avbtime(pdburst_t *burstp, uint32 *tx, uint32 *rx)
{
	wlc_info_t *wlc = burstp->wlc;

	if (PROXD_ENAB_UCODE_TSYNC(wlc->pub)) {
		pdburst_sm_t *sm = burstp->sm;
		*tx = sm->tod;
		*rx = sm->toa;
	} else {
		wlc_get_avb_timestamp(wlc->hw, tx, rx);
	}
}

/* Get measurement results */
static int pdburst_measure_results(pdburst_t *burstp, uint64 *tx, uint64 *rx,
	int32 *gd, int32 *adj, uint16 id, bool acked,
	wlc_phy_tof_info_t *tof_info, bool *discard, bool initiator)
{
	wlc_info_t *wlc = burstp->wlc;
	uint16 rspcmd, i;
	uint32 avbrx, avbtx;
	int32 h_adj = 0;
	int ret = BCME_OK;

	for (i = 0; i < 200; i++) {
		rspcmd = wlc_read_shm(wlc, burstp->shmemptr + M_TOF_RSP_OFFSET(wlc));
		if (((rspcmd & TOF_RSP_MASK) == TOF_SUCCESS) || !acked)
			break;
		OSL_DELAY(1);
	}

	if (!BURST_SEQ_EN(burstp)) {
		pdburst_avbtime(burstp, &avbtx, &avbrx);
	}

#ifdef TOF_COLLECT
	if (burstp->collectp && burstp->collectp->collect) {
		burstp->collectp->collect_info.tof_cmd = burstp->tofcmd;
		burstp->collectp->collect_info.tof_rsp = rspcmd;
		burstp->collectp->collect_info.tof_id = id;
		burstp->collectp->collect_info.nfft = 0;
		burstp->collectp->collect_info.type = TOF_ADJ_NONE;
		if (!BURST_SEQ_EN(burstp)) {
			burstp->collectp->collect_info.tof_avb_rxl = (uint16)(avbrx & 0xffff);
			burstp->collectp->collect_info.tof_avb_rxh =
				(uint16)((avbrx >> 16) & 0xffff);
			burstp->collectp->collect_info.tof_avb_txl = (uint16)(avbtx & 0xffff);
			burstp->collectp->collect_info.tof_avb_txh =
				(uint16)((avbtx >> 16) & 0xffff);
		}
	}
#endif /* TOF_COLLECT */

	if ((rspcmd & TOF_RSP_MASK) == TOF_SUCCESS) {

		uint32 delta, subband;
		int expected_frame_bw = 0, frame_type = 0;
		wlc_phy_tof_info_type_t temp_mask;

		if (!BURST_SEQ_EN(burstp)) {
			if (avbrx == burstp->oldavbrx && avbtx == burstp->oldavbtx) {
				uint32 clkst, macctrl1;
				wlc_get_avb_timer_reg(wlc->hw, &clkst, &macctrl1);
				wlc_enable_avb_timer(wlc->hw, TRUE);
				TOF_PRINTF(("Clkst %x Macctrl1 %x Restart AVB timer\n",
					clkst, macctrl1));
			} else {
				burstp->oldavbrx = avbrx;
				burstp->oldavbtx = avbtx;
			}
		}

		temp_mask = (WLC_PHY_TOF_INFO_TYPE_FRAME_TYPE |
			WLC_PHY_TOF_INFO_TYPE_FRAME_BW | WLC_PHY_TOF_INFO_TYPE_CFO |
			WLC_PHY_TOF_INFO_TYPE_RSSI);
		if (wlc_phy_tof_info(WLC_PI(wlc), tof_info, temp_mask, burstp->core)
			== BCME_OK) {
			ratespec_t rspec;
			if (tof_info->info_mask & WLC_PHY_TOF_INFO_TYPE_FRAME_BW) {
				subband = ((uint32)(tof_info->frame_bw)) & 0xffff0000;
				tof_info->frame_bw = tof_info->frame_bw & 0xffff;
			}
			else {
				subband = 0;
				tof_info->frame_bw = -1;
			}
			if (!BURST_SEQ_EN(burstp)) {
				/* On the initiator side, for the legacy case, use the BW  */
				/* from the rxstatus itself, as legacy header doesn't have */
				/* bw information and rspec always hold 20MHz bandwidth  */
				if (initiator) {
					rspec = burstp->ftm_rx_rspec;
					/* Update the rspec BW information for legacy */
					if ((rspec & WL_RSPEC_ENCODING_MASK)
						== WL_RSPEC_ENCODE_RATE) {
						if (tof_info->frame_bw == TOF_BW_80MHZ_INDEX) {
							rspec = (rspec & (~(WL_RSPEC_BW_MASK)))
							| WL_RSPEC_BW_80MHZ;
						} else if (tof_info->frame_bw ==
							TOF_BW_40MHZ_INDEX) {
							rspec = (rspec & (~(WL_RSPEC_BW_MASK)))
							| WL_RSPEC_BW_40MHZ;
						} else {
							rspec = (rspec & (~(WL_RSPEC_BW_MASK)))
							| WL_RSPEC_BW_20MHZ;
						}
					}
					burstp->ftm_rx_rspec = rspec;
				} else {
					rspec = burstp->ack_rx_rspec;
				}

				if ((rspec & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_80MHZ) {
					tof_info->frame_bw = TOF_BW_80MHZ_INDEX;
				} else if ((rspec & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_40MHZ) {
					tof_info->frame_bw = TOF_BW_40MHZ_INDEX;
				} else if ((rspec & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_20MHZ) {
					tof_info->frame_bw = TOF_BW_20MHZ_INDEX;
				}

				if ((rspec & WL_RSPEC_ENCODING_MASK) ==
					WL_RSPEC_ENCODE_VHT) {
					tof_info->frame_type = FRAME_TYPE_11AC;
				} else if ((rspec & WL_RSPEC_ENCODING_MASK) ==
					WL_RSPEC_ENCODE_HT) {
					tof_info->frame_type = FRAME_TYPE_11N;
				} else if ((rspec & WL_RSPEC_ENCODING_MASK) ==
					WL_RSPEC_ENCODE_RATE) {
					tof_info->frame_type = FRAME_TYPE_OFDM;
				}
			}
			burstp->frame_type_cnt[frame_type]++;
			/*
			 *FIXME!!!!!!
			 *This needs to be revised sp for non-bcm AP's. Logic should be
			 *that frame bw is == expected bw. Expected bw may need to be
			 *negotiated w/ AP. For now expected bw should be bw used to tx
			 *frame. This is NOT NECESSARILY same as phy bw as 20MHz frames
			 *can be transmitted while phy is in 80MHz mode
			 *
			 *AVB rx timer is asserted at different point for 11g v 11ac
			 *frames so this needs to be adjusted here.
			 */
			if (CHSPEC_IS80(burstp->configp->chanspec))
				expected_frame_bw = TOF_BW_80MHZ_INDEX;
			else if (CHSPEC_IS40(burstp->configp->chanspec))
				expected_frame_bw = TOF_BW_40MHZ_INDEX;
			else
				expected_frame_bw = TOF_BW_20MHZ_INDEX;

			if (WLCISACPHY(wlc->band) && (burstp->chipnum == BCM4360_CHIP_ID)) {
				TOF_PRINTF(("band %p chipnum %x frame_bw %x expected_frame_bw %x\n",
				 OSL_OBFUSCATE_BUF(wlc->band), burstp->chipnum, tof_info->frame_bw,
				 expected_frame_bw));
				tof_info->frame_bw = expected_frame_bw;
			}
#ifdef TOF_COLLECT
			if (burstp->collectp && burstp->collectp->collect) {
				 burstp->collectp->collect_info.tof_frame_type
					= (uint8)(tof_info->frame_type);
				burstp->collectp->collect_info.tof_frame_bw =
					(uint8)(tof_info->frame_bw);
				burstp->collectp->collect_info.tof_rssi =
					(int8)(tof_info->rssi);
				burstp->collectp->collect_info.tof_cfo =
					(int32)(tof_info->cfo);
			}
#endif // endif
		} else {
			tof_info->info_mask = WLC_PHY_TOF_INFO_TYPE_NONE;
			tof_info->frame_type = -1;
			tof_info->frame_bw = -1;
			expected_frame_bw = 0;
			subband = 0;
		}

		if (!BURST_SEQ_EN(burstp)) {
			TOF_PRINTF(("AVB(%d): tx %u rx %u id %x\n", i, avbtx, avbrx, id));
		}

#ifdef TOF_PROFILE
		wlc_read_tsf(burstp->wlc, &tsf_lastm, &tsf_hi);
		tsf_lastm -= tsf_start;
		TOF_PRINTF(("BEFORE_PROCESSING TIME = 0x%0x\n", tsf_lastm));
#endif // endif

		if (BURST_SEQ_EN(burstp) || burstp->tunep->hw_adj || burstp->tunep->sw_adj) {
			ret = pdburst_rtd_adj(burstp, tof_info->frame_type,
				(tof_info->frame_bw | subband), tof_info->cfo,
				burstp->tunep->sw_adj, burstp->tunep->hw_adj,
				BURST_SEQ_EN(burstp), gd, adj, initiator, id);
			if (ret) {
				return BCME_ERROR;
			}
		} else {
			*gd = *adj = 0;
			pdburst_measure(burstp, initiator? TOF_RX : TOF_RESET);
			burstp->adj_type_cnt[TOF_ADJ_NONE]++;
		}
		h_adj = *adj;

#ifdef TOF_PROFILE
		wlc_read_tsf(burstp->wlc, &tsf_lastm, &tsf_hi);
		tsf_lastm -= tsf_start;
		TOF_PRINTF(("AFTER_PROCESSING TIME = 0x%0x\n", tsf_lastm));
#endif // endif

		if (BURST_SEQ_EN(burstp)) {
			if (initiator) {
				*tx = h_adj;
				*rx = 0;
			} else {
				*tx = 0;
				*rx = h_adj;
			}
		} else {
			if (initiator) {
				uint32 ki = pdburst_get_kval(burstp, TRUE, FALSE);
				delta = (uint32)(TOF_TICK_TO_NS(((avbtx-avbrx) & 0xffff),
					burstp->Tq));
				*rx = TOF_TICK_TO_NS(avbrx, burstp->Tq);
				*tx = *rx + delta;
				*rx = (*rx + h_adj) - ki;
#ifdef TOF_KVALUE_CAL
				TOF_PRINTF(("dTraw %d\n", (delta - h_adj)));
#endif // endif
			} else {
				/*
					T4 = T1(avb timer) + Delta(avbrx-avbtx)
						+ h_adj - K (target k value)
					Tq is a shift factor to keep avb timer integer
					calculation accuracy
				*/
				uint32 kt = pdburst_get_kval(burstp, FALSE, FALSE);
				delta = TOF_TICK_TO_NS(((avbrx-avbtx) & 0xffff), burstp->Tq);
				*tx = TOF_TICK_TO_NS(avbtx, burstp->Tq);
#ifdef TOF_KVALUE_CAL
				TOF_PRINTF(("dTraw %d\n", (delta + h_adj)));
#endif // endif
				if (delta + h_adj > kt)
					*rx = *tx + delta + h_adj - kt;
				else {
					*rx = *tx;
					FTM_ERR(("K %d is bigger than delta %d\n", kt,
						delta + h_adj));
				}
			}
		}

		return BCME_OK;
	} else {
		pdburst_measure(burstp, initiator? TOF_RX : TOF_RESET);
		TOF_PRINTF(("AVB Failed id %d rspcmd %x tofcmd %x\n", id, rspcmd, burstp->tofcmd));
	}
	return BCME_ERROR;
}

/* reset TOF state */
static void pdburst_reset(pdburst_sm_t *sm, uint8 mode, wl_proxd_status_t reason)
{
	pdburst_t *burstp = sm->tof_obj;

	if (reason != WL_PROXD_E_NOTSTARTED) {
		pdburst_measure(burstp, TOF_RESET);
		pdburst_hw(burstp, FALSE, FALSE);
#ifdef WL_RANGE_SEQ
		BURST_SEQ_EN_SET(burstp, FALSE);
#else
		burstp->seq_en = FALSE;
#endif // endif
	}

	sm->tof_mode = mode;
	sm->tof_state = TOF_STATE_IDLE;
	sm->tof_txcnt = 0;
	sm->tof_rxcnt = 0;
	sm->tof_reason = reason;
	sm->tof_legacypeer = TOF_LEGACY_UNKNOWN;
	sm->tof_txvht = FALSE;
	sm->tof_rxvht = FALSE;

	if (reason != WL_PROXD_E_NOACK && reason != WL_PROXD_E_TIMEOUT)
		sm->tof_retrycnt = 0;

	if (burstp->ftm_tx_timer_active)
	{
		wlc_hrt_del_timeout(burstp->ftm_tx_timer);
		burstp->ftm_tx_timer_active = FALSE;
	}

	if (mode == WL_PROXD_MODE_INITIATOR) {
		bcopy(&burstp->configp->peer_mac, &sm->tof_peerea, ETHER_ADDR_LEN);
		if (burstp->timeractive) {
			burstp->timeractive = FALSE;
			wl_del_timer(burstp->wlc->wl, burstp->timer);
		}
		burstp->tof_tslist.tscnt = 0;
		burstp->frmcnt = 0;
	} else {
		bcopy(&ether_null, &sm->tof_peerea, ETHER_ADDR_LEN);
	}
}

static void
pdburst_excursion_complete(void *ctx,	int status, wlc_bsscfg_t *cfg)
{

	pdburst_t *burstp = (pdburst_t *)ctx;
#ifdef TOF_DEBUG_TIME
	TOF_PRINTF(("$$$$$  %s\n", __FUNCTION__));
#endif // endif
	ASSERT(burstp != NULL);
	ASSERT(cfg != NULL);
	ASSERT(burstp->wlc != NULL);

	/* proximity duration time expired */
	if (burstp->scanestarted) {
		burstp->scanestarted = FALSE;
	}

	burstp->delayfree &= ~TOF_DFREE_SCAN;
	if (!burstp->delayfree && burstp->destroyed) {
#ifdef TOF_DEBUG_TIME
		TOF_PRINTF(("delayfree burstp %p scan complete\n", OSL_OBFUSCATE_BUF(burstp)));
#endif // endif
		MFREE(burstp->wlc->osh, burstp, sizeof(pdburst_t));
		return;
	}

	if (burstp->smstoped)
		return;

#ifdef TOF_DEBUG_TIME2
	wlc_read_tsf(burstp->wlc, &tsf_tmo, &tsf_hi);
	tsf_tmo -= tsf_start;
#endif // endif

	if (burstp->sm->tof_state != TOF_STATE_ICONFIRM)
		pdburst_sm(burstp->sm, TOF_EVENT_TMO, NULL, 0, NULL);
}

/* TOF stay power on using bsscfg state flag to stop mpc */
static void pdburst_pwron(pdburst_t* burstp, bool up)
{
	proxd_power(burstp->wlc, PROXD_PWR_ID_BURST, up);
}

/* TOF initiator timeout function */
static void
pdburst_duration_expired_cb(void *ctx)
{
	pdburst_t *burstp = (pdburst_t *)ctx;
	wlc_bsscfg_t *cfg;

	ASSERT(burstp != NULL);

	cfg = burstp->bsscfg;

	wlc_hrt_del_timeout(burstp->duration_timer);
	burstp->duration_timer_active = FALSE;
	pdburst_excursion_complete(ctx, 0, cfg);
}

/* TOF target timeout function */
static void
pdburst_duration_expired_target(void *ctx)
{
	pdburst_t *burstp = (pdburst_t *)ctx;
	pdburst_sm_t *sm;
#ifdef TOF_DEBUG_TIME
	uint64 curtsf = 0;
#endif // endif

	ASSERT(burstp != NULL);
	if (burstp) {
#ifdef TOF_DEBUG_TIME
		FTM_GET_TSF(wlc_ftm_get_handle(burstp->wlc), curtsf);
		TOF_PRINTF(("*** %s: BURST EXPIRED TIME = %u.%u \n", __FUNCTION__,
			FTM_LOG_TSF_ARG(curtsf)));
#endif // endif
		sm = burstp->sm;
		burstp->duration_timer_active = FALSE;
		if (sm) {
			pdburst_reset(sm, sm->tof_mode, WL_PROXD_E_TIMEOUT);
			pdburst_target_done(sm, WL_PROXD_E_TIMEOUT);
		}
	}
}

/* activate the scan engine */
static void pdburst_activate_pm(pdburst_t* burstp)
{
	wlc_info_t *wlc;

	wlc = burstp->wlc;

	ASSERT(wlc != NULL);

	BCM_REFERENCE(wlc);

	burstp->txcnt = 0;
	burstp->rxcnt = 0;
}

/* deactivate the scan engine */
static void pdburst_deactivate_pm(pdburst_t* burstp)
{
}

static uint8 pdburst_get_ftm_cnt(pdburst_t* burstp)
{

	if (burstp->flags & WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP) {
		if (burstp->configp->num_ftm > 1) {
			if (burstp->tx_t1)
				return (burstp->configp->num_ftm);
			else
				return (burstp->configp->num_ftm - 1);
		}
	} else {
		if (burstp->configp->num_ftm > 1)
			return (burstp->configp->num_ftm - 1);
	}

	if (BURST_SEQ_EN(burstp))
		return burstp->tunep->ftm_cnt[TOF_BW_SEQTX_INDEX];

	if (CHSPEC_IS80(burstp->configp->chanspec))
		return burstp->tunep->ftm_cnt[TOF_BW_80MHZ_INDEX];

	if (CHSPEC_IS40(burstp->configp->chanspec))
		return burstp->tunep->ftm_cnt[TOF_BW_40MHZ_INDEX];

	return burstp->tunep->ftm_cnt[TOF_BW_20MHZ_INDEX];
}

/* update TOF parameters from rxed frame */
static int
pdburst_rx_tof_params(pdburst_t *burstp, pdburst_frame_type_t ftype,
	const uint8 *body, int body_len, ratespec_t rspec)
{
	int err = BCME_OK;
	pdburst_session_info_t bsi;
	ftm_vs_req_params_t req_params;
	ftm_vs_seq_params_t seq_params;
	ftm_vs_sec_params_t sec_params;
	ftm_vs_meas_info_t meas_info;
	uint8 ri_rr[FTM_TPK_RI_RR_LEN_SECURE_2_0];
	ftm_vs_timing_params_t timing_params;
	wlc_phy_tof_secure_2_0_t  tof_sec_params;
	uint16 ri_rr_len = 0;

	bzero((wlc_phy_tof_secure_2_0_t *)&tof_sec_params, sizeof(tof_sec_params));
	bzero(ri_rr, sizeof(ri_rr));

	burstp->measurecnt = burstp->configp->num_ftm - 1;
	/* burstp->configp->ratespec = rspec; */

	bzero(&bsi, sizeof(bsi));
	/* initialize local variables with 0s */
	bzero(&req_params, sizeof(req_params));
	bzero(&seq_params, sizeof(seq_params));
	bzero(&sec_params, sizeof(sec_params));
	bzero(&meas_info, sizeof(meas_info));
	bzero(&timing_params, sizeof(timing_params));

	tof_sec_params.start_seq_time =
		TIMING_TLV_START_SEQ_TIME;
	tof_sec_params.delta_time_tx2rx =
		TIMING_TLV_DELTA_TIME_TX2RX;

	switch (ftype) {
	case PDBURST_FRAME_TYPE_REQ:
		bsi.vs_req_params = &req_params;
		bsi.vs_seq_params = &seq_params;
		break;
	case PDBURST_FRAME_TYPE_MEAS:
		bsi.vs_sec_params = &sec_params;
		bsi.vs_timing_params = &timing_params;
		bsi.vs_meas_info = &meas_info;
		break;
	default:
		err = BCME_UNSUPPORTED;
		goto done;
	}

	err = PROTOCB(burstp, vs_rx, (burstp->ctx, ftype, body, body_len, &bsi));
	if (err != BCME_OK) {
		/* clear vendor options if peer is not bcm */
		if (err == WL_PROXD_E_NOT_BCM) {
			pdburst_session_info_t info;
			bzero(&info, sizeof(info));
			err = PROTOCB(burstp, set_session_info, (burstp->ctx, &info));
			burstp->flags &= ~(WL_PROXD_SESSION_FLAG_SEQ_EN |
				WL_PROXD_SESSION_FLAG_VHTACK | WL_PROXD_SESSION_FLAG_SECURE);
		}
		goto done;
	}

	/* session is already updated in vs rx. update burst */
	if (FTM_VS_PARAMS_VALID(&req_params)) {
		burstp->seq_en = (req_params.flags & FTM_VS_REQ_F_SEQ_EN) ? TRUE : FALSE;
		burstp->totalfrmcnt = req_params.totfrmcnt;
		burstp->flags |= (req_params.flags & FTM_VS_REQ_F_VHTACK) ?
			WL_PROXD_SESSION_FLAG_VHTACK : 0;
		burstp->flags |= (req_params.flags & FTM_VS_REQ_F_SECURE) ?
			WL_PROXD_SESSION_FLAG_SECURE : 0;
	}

	/* Further TLVs not present/required for AVB */
	if (!BURST_SEQ_EN(burstp))
		goto done;

	if (burstp->flags & WL_PROXD_SESSION_FLAG_SECURE) {
		if (!BURST_SEQ_EN(burstp) || !FTM_VS_PARAMS_VALID(&sec_params))
			goto done;
		if (bsi.vs_sec_params->flags &
			FTM_VS_SEC_F_RANGING_2_0) {
			memcpy(ri_rr, bsi.vs_sec_params->ri,
				FTM_TPK_RI_PHY_LEN_SECURE_2_0);
			memcpy((ri_rr +
				FTM_TPK_RI_PHY_LEN_SECURE_2_0),
				bsi.vs_sec_params->rr,
				FTM_TPK_RR_PHY_LEN_SECURE_2_0);
			ri_rr_len = FTM_TPK_RI_RR_LEN_SECURE_2_0;
			tof_sec_params.start_seq_time =
			bsi.vs_timing_params->start_seq_time;
			tof_sec_params.delta_time_tx2rx =
			bsi.vs_timing_params->delta_time_tx2rx;
		} else if ((bsi.vs_sec_params->flags &
			FTM_VS_SEC_F_VALID)
			== FTM_VS_SEC_F_VALID) {
			memcpy(ri_rr, bsi.vs_sec_params->ri, FTM_VS_TPK_RI_LEN);
			memcpy((ri_rr + FTM_VS_TPK_RI_LEN), bsi.vs_sec_params->rr,
				(FTM_TPK_RI_RR_LEN - FTM_VS_TPK_RI_LEN));
			ri_rr_len = FTM_TPK_RI_RR_LEN;
		}
		err = phy_tof_set_ri_rr(WLC_PI(burstp->wlc), (uint8 *)ri_rr, ri_rr_len,
			burstp->core, TRUE, TRUE, tof_sec_params);
		prhex("phy_ri", (uint8 *)bsi.vs_sec_params->ri,
			((bsi.vs_sec_params->flags &
			FTM_VS_SEC_F_RANGING_2_0) ?
			FTM_VS_TPK_RI_LEN_SECURE_2_0 :
			FTM_VS_TPK_RI_LEN));
		prhex("phy_rr", (uint8 *)bsi.vs_sec_params->rr,
			((bsi.vs_sec_params->flags &
			FTM_VS_SEC_F_RANGING_2_0) ?
			FTM_VS_TPK_RR_LEN_SECURE_2_0 :
			FTM_VS_TPK_RR_LEN));

	} else {
		err = phy_tof_set_ri_rr(WLC_PI(burstp->wlc), NULL, FTM_TPK_RI_RR_LEN,
				burstp->core, TRUE, FALSE, tof_sec_params);
	}

done:
	return err;
}

static uint8 pdburst_rx_measurecnt(pdburst_t* burstp, const pdburst_tsinfo_t *protp)
{
	tof_tslist_t *listp = &burstp->tof_tslist;
	ftmts_t * list = listp->tslist;

	if (protp && list && protp->ts_id > 0) {
		/* Not the first measurement packet */
#ifdef TOF_DEBUG_TIME
		if (burstp->flags & WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP)
			TOF_PRINTF(("Comp txid %d tsid %d\n", list[listp->tscnt].tx_id,
				protp->ts_id));
#endif /* TOF_DEBUG_TIME */
		if (list[listp->tscnt].tx_id == protp->ts_id)
			return listp->tscnt + 1;
		/* This is retransmission */
		return listp->tscnt;
	}

	return 0;
}

static pdburst_seq_state_t
get_next_seq_state(int num_meas)
{
	return (pdburst_seq_state_t)(num_meas % TOF_SEQ_LAST);
}

/* Process rxed action frames */
static int pdburst_process_rx_frame(pdburst_t* burstp, const wlc_d11rxhdr_t *wrxh,
	const uint8 *body, int body_len, uint32 rspec, const pdburst_tsinfo_t *tsinfo)
{
	pdburst_sm_t *sm;
	wlc_info_t *wlc;
	int ret;
	pdburst_data_t data;
	const d11rxhdr_t *rxh;
	uint32 chan_size;
	int32 *tmpH;
	uint32* tmpR;
	int32 adj_err;
	uint8 mes_cnt;

	ASSERT(burstp != NULL);
	ASSERT(burstp->sm != NULL);
	ASSERT(burstp->wlc != NULL);

#ifndef TOF_COLLECT_REMOTE
	if (burstp->smstoped || !burstp->ctx || !burstp->svc_funcs)
		return 0;
#endif /* TOF_COLLECT */

	rxh = &wrxh->rxhdr;
	sm = burstp->sm;
	wlc = burstp->wlc;

#ifdef TOF_DEBUG_TIME
	if (burstp->sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
		/* log action farme rx time to compute tottal deaf interval
		 * (ucode already made phy deaf on receivig FTM measurement frame).
		 */
		burstp->phy_deaf_start = OSL_SYSUPTIME_US();
	}

	if (tsinfo) {
		TOF_PRINTF(("pdburst_process_rx_frame: measurement dialog %d "
			"followup %d rxStatus1 0x%x, MuRate 0x%x\n", tsinfo->tx_id, tsinfo->ts_id,
			D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, RxStatus1),
			D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate)));
	} else {
		TOF_PRINTF(("pdburst_process_rx_frame: request\n"));
	}
#endif /* TOF_DEBUG_TIME */

	if (!tsinfo)
	{
		/* FTM Request */
#ifdef TOF_COLLECT_REMOTE
		if (*body == DOT11_ACTION_CAT_VS && *(body+5) == BRCM_FTM_VS_COLLECT_SUBTYPE) {
			pdburst_collect_frm_t *tof_hdr = (pdburst_collect_frm_t *)body;
			data.tof_type = tof_hdr->tof_type;
			tsinfo = (const pdburst_tsinfo_t *)body;
		}
		else
#endif /* TOF_COLLECT */
		{
			data.tof_type = TOF_TYPE_REQ_START;
#ifndef WL_RANGE_SEQ
			if (burstp->sm->tof_mode == WL_PROXD_MODE_TARGET) {
				burstp->seq_state = TOF_SEQ_NONE;
			}
#endif /* !WL_RANGE_SEQ */
		}
	} else if ((D11REV_GE(wlc->pub->corerev, 128) && (D11RXHDR_ACCESS_VAL(rxh,
		wlc->pub->corerev, MuRate) & htol16(RXS_TOFINFO))) ||
		(D11REV_LT(wlc->pub->corerev, 128) && (D11RXHDR_ACCESS_VAL(rxh,
			wlc->pub->corerev, RxStatus1) & htol16(RXS_TOFINFO)))) {
		/* FTM measurement packet */
		if ((tsinfo->ts_id == 0) && (burstp->seq_en)) {
			uint8 frame_bw;
			if (CHSPEC_IS80(burstp->configp->chanspec)) {
				frame_bw = 2;
			} else {
				frame_bw = 0;
			}
			TOF_PRINTF(("frame_bw = %d\n", frame_bw));
			chan_size = (K_TOF_COLLECT_CHAN_SIZE) >> (2 - frame_bw);
			tmpH = NULL;
			if (!burstp->collectp) {
				TOF_PRINTF(("Allocating temp mem ----->\n"));
				tmpR = (uint32*)MALLOCZ(burstp->wlc->osh,
						(PHY_CORE_MAX+1)*chan_size* sizeof(uint32));
				if (tmpR == NULL) {
					TOF_PRINTF(("*** Malloc error for channel estimates."
								"*** \n"));
					return BCME_NOMEM;
				}
			} else {
				tmpR = (uint32 *) (burstp->collectp->chan);
			}

			adj_err = phy_tof_chan_freq_response(WLC_PI(burstp->wlc),
					chan_size, CORE0_K_TOF_H_BITS, tmpH, NULL, tmpR,
					FALSE, 1, TRUE);
			burstp->channel_dumped = TRUE;

			if (burstp->collectp) {
				burstp->collectp->collect_info.num_max_cores =
					phy_tof_num_cores(WLC_PI(burstp->wlc));
			}

			if ((tmpR != NULL) && !burstp->collectp) {
				MFREE(burstp->wlc->osh, tmpR,
						(PHY_CORE_MAX+1)*chan_size*sizeof(uint32));
			}
			TOF_PRINTF(("Channel dump : adj_err -> %d\n", adj_err));
			if (burstp->flags & WL_PROXD_SESSION_FLAG_SECURE) {
				if (burstp->tunep->core == 0xff) {
					phy_tof_core_select(WLC_PI(burstp->wlc),
							burstp->tunep->acs_gdv_thresh,
							burstp->tunep->acs_gdmm_thresh,
							burstp->tunep->acs_rssi_thresh,
							burstp->tunep->acs_delta_rssi_thresh,
							&(burstp->core),
							burstp->tunep->core_mask);
				}
			}
		}
		if (BURST_SEQ_EN(burstp)) {
			burstp->seq_state = get_next_seq_state(burstp->num_meas);
		}
		mes_cnt = pdburst_rx_measurecnt(burstp, tsinfo);
		burstp->num_meas = mes_cnt + 1; /* because meas_cnt starts from 0 */
#ifdef TOF_DEBUG_TIME
		TOF_PRINTF(("%s: num_meas %d mes_cnt %d measurecnt %d tx_id %d ts_id %d \n",
			__FUNCTION__, burstp->num_meas, mes_cnt,
		burstp->measurecnt, tsinfo->tx_id, tsinfo->ts_id));
#endif // endif
		if (tsinfo->tx_id) {
			if (mes_cnt >= burstp->measurecnt) {
				data.tof_type = TOF_TYPE_MEASURE_END;
			} else {
				data.tof_type = TOF_TYPE_MEASURE;
			}
		} else {
			data.tof_type = TOF_TYPE_MEASURE_END;
		}
#ifdef ROM_COMPAT_LQRSSI
		data.tof_rssi = wlc_lq_recv_rssi_compute(wlc, wrxh);
#else
		data.tof_rssi = wrxh->rssi;
#endif /* ROM_COMPAT_LQRSSI */
		if (BURST_SEQ_EN(burstp)) {
			if (burstp->sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
				if (burstp->num_meas % TOF_DEFAULT_FTMCNT_SEQ == 1) {
					/* call the vs_rx cb */
#ifdef WL_RANGE_SEQ
					ret = pdburst_rx_tof_params(burstp,
							PDBURST_FRAME_TYPE_MEAS,
							body, body_len, rspec);
#endif /* WL_RANGE_SEQ */
					/* It was found that the phy's classsifier register
					** that the phy is deaf at time, on the reception of a ftm
					** measurement frame # 2. Since the driver control's the
					** undeaf'ing of the phy, this change was made
					*/
#ifdef WL_RANGE_SEQ
					proxd_undeaf_phy(burstp->wlc, TRUE);
					if (ret != BCME_OK) {
						return ret;
					}
#endif /* WL_RANGE_SEQ */
				} else if (burstp->num_meas % TOF_DEFAULT_FTMCNT_SEQ == 0) {
					/* changes from another RB
					** needs to be added
					*/
					burstp->measurecnt = burstp->configp->num_ftm - 1;
					/* 3rd measurement frame from target */
#ifdef WL_RANGE_SEQ
					ret = pdburst_rx_tof_params(burstp,
						PDBURST_FRAME_TYPE_MEAS_END,
						body, body_len, rspec);
					if (ret != BCME_OK) {
						FTM_ERR(("pdburst_rx_tof_params:"
							" Returned error %d\n", ret));
						return ret;
					}
#else
					if (FTM_BSSCFG_SECURE(wlc_ftm_get_handle(wlc),
						burstp->bsscfg)) {
					/* call the vs_rx cb to process the mf-buf or other TLVs */
					}
#endif /* WL_RANGE_SEQ */
				}
			}
		}
	} else {
		FTM_ERR(("rxh->RxStatus1 %x\n",
			D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, RxStatus1)));
		return 0;
	}

	bcopy(&burstp->configp->peer_mac, &data.tof_srcea, ETHER_ADDR_LEN);
	data.tof_rspec = rspec;
	++burstp->rxcnt;

	if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT &&
		sm->tof_state != TOF_STATE_IDLE && sm->tof_state != TOF_STATE_ICONFIRM)
		sm->tof_rxvht = TRUE;

	ret = pdburst_sm(sm, TOF_EVENT_RXACT, tsinfo, sizeof(*tsinfo), &data);
	if (sm->tof_mode == WL_PROXD_MODE_TARGET)
		return 0;

	if (ret == TOF_RET_ALIVE) {
		if (burstp->duration_timer_active) {
			wlc_hrt_del_timeout(burstp->duration_timer);
			wlc_hrt_add_timeout(burstp->duration_timer,
				FTM_INTVL2USEC(&burstp->configp->timeout),
				pdburst_duration_expired_cb, (void *)burstp);
		}
	} else if (ret == TOF_RET_SLEEP) {
		if (burstp->scanestarted) {
			wlc_scan_abort_ex(wlc->scan, burstp->scanbsscfg,
				WLC_E_STATUS_ABORT);
			burstp->scanestarted = FALSE;
		}
	}

	return 0;
}

/* action frame tx complete callback */
void pdburst_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	pdburst_t *burstp = (pdburst_t *)arg;
	pdburst_sm_t *sm;
	int err = BCME_OK;

	if (!burstp)
		return;

#ifdef TOF_DEBUG_TIME
	TOF_PRINTF(("pdburst_tx_complete: burstp %p, %s \n",
		OSL_OBFUSCATE_BUF(burstp),
		((txstatus & TX_STATUS_ACK_RCV)?"ACK recvd":"NO ACK")));
#endif // endif

	if (burstp->smstoped && (burstp->delayfree & TOF_DFREE_TXDONE)) {
		/* pkt txed but not processed because of burst timeout */
		proxd_undeaf_phy(wlc, (txstatus & TX_STATUS_ACK_RCV));
		if (FTM_BSSCFG_SECURE(wlc_ftm_get_handle(burstp->wlc), burstp->bsscfg)) {
			phy_rxgcrs_sel_classifier((phy_info_t *) WLC_PI(burstp->wlc),
				TOF_CLASSIFIER_BPHY_ON_OFDM_ON);
		}
	}
	burstp->delayfree &= ~TOF_DFREE_TXDONE;
	if (!burstp->delayfree && burstp->destroyed) {
#ifdef TOF_DEBUG_TIME
		TOF_PRINTF(("delayfree burstp %p after tx_complete\n", OSL_OBFUSCATE_BUF(burstp)));
#endif // endif
		MFREE(burstp->wlc->osh, burstp, sizeof(pdburst_t));
		return;
	}

#ifndef TOF_COLLECT_REMOTE
	if (burstp->smstoped || !burstp->ctx || !burstp->svc_funcs)
		return;
#endif /* TOF_COLLECT_REMOTE */

	ASSERT(burstp->sm != NULL);

	sm = burstp->sm;
	if (sm == NULL) {
		ASSERT((burstp->ctx != NULL) && burstp->ftm_tx_timer_active);
		return;
	}
	err = (txstatus & TX_STATUS_ACK_RCV) ? BCME_OK : WL_PROXD_E_NOACK;

	(void)PROTOCB(burstp, tx_done, (burstp->ctx, err));

	if (err != BCME_OK) {
		FTM_ERR(("pdburst_tx_complete: ACK was lost txstat:0x%x, pkt:%d, retry:%d\n",
			txstatus, burstp->sm->tof_txcnt, burstp->sm->tof_retrycnt));

		if ((burstp->ftm_tx_timer_active) && (sm->tof_mode == WL_PROXD_MODE_TARGET)) {
			wlc_hrt_del_timeout(burstp->ftm_tx_timer);
			burstp->ftm_tx_timer_active = FALSE;
			FTM_ERR(("%s:ERROR: ftm[%d] OnHrtTimer TX is pending, cancelled\n",
				__FUNCTION__, burstp->sm->tof_txcnt));
		}
		/* reset ucode state */
		pdburst_measure(burstp, TOF_RESET);
		pdburst_sm(burstp->sm, TOF_EVENT_NOACK, NULL, 0, NULL);
	} else {
		pdburst_sm(burstp->sm, TOF_EVENT_ACKED, NULL, 0, NULL);
	}
}

static int
pdburst_init_bsi(pdburst_t *burstp, pdburst_frame_type_t ftype,
	ftm_vs_req_params_t *req, ftm_vs_seq_params_t *seq,
	ftm_vs_sec_params_t *sec, ftm_vs_mf_buf_t *mf,
	ftm_vs_meas_info_t *meas_info,
	ftm_vs_timing_params_t *timing_params,
	pdburst_session_info_t *bsi)
{
	int err = BCME_OK;
	wlc_phy_tof_info_t tof_info;

	BCM_REFERENCE(mf);

	bzero(bsi, sizeof(*bsi));

	if (ftype == PDBURST_FRAME_TYPE_REQ) {
		bsi->vs_req_params = req;
		bzero(req, sizeof(*req));
		req->flags = FTM_VS_F_VALID;
		req->totfrmcnt = pdburst_total_framecnt(burstp);
		/* other req params are generated or taken from session */
	}

	if (ftype == PDBURST_FRAME_TYPE_MEAS) {
		/* meas info */
		bzero(meas_info, sizeof(*meas_info));
		bsi->vs_meas_info = meas_info;
		meas_info->flags = FTM_VS_MEAS_F_VALID;
		if (get_next_seq_state(burstp->sm->tof_txcnt) == TOF_SEQ_DONE) {
			/* read and set phy error code here */
			bzero(&tof_info, sizeof(tof_info));
			err = wlc_phy_tof_info(WLC_PI(burstp->wlc), &tof_info,
				WLC_PHY_TOF_INFO_TYPE_PHYERROR, burstp->core);
			if (err != BCME_OK)
				goto done;
			bsi->vs_meas_info->phy_err = tof_info.tof_phy_error;
		}
	}

	if (BURST_SEQ_EN(burstp)) {
		bsi->vs_seq_params = seq;
		bzero(seq, sizeof(*seq));
		seq->flags = FTM_VS_F_VALID;
	}

	if (burstp->flags & WL_PROXD_SESSION_FLAG_SECURE) {
		bsi->vs_sec_params = sec;
		bzero(sec, sizeof(*sec));
		sec->flags = FTM_VS_F_VALID;
		/* other sec params are generated or taken from session */
		bsi->vs_timing_params = timing_params;
	}

	/* MF buf goes in last measure frame of each measurement */
	if (get_next_seq_state(burstp->sm->tof_txcnt) == TOF_SEQ_DONE) {
		bsi->vs_mf_buf_data = burstp->mf_buf;
		bsi->vs_mf_buf_data_max = burstp->mf_buf_len;
		bsi->vs_mf_buf_data_len = burstp->mf_buf_len;
		bsi->vs_mf_buf = mf;
		mf->flags = FTM_VS_F_VALID;
	}

	BCM_REFERENCE(mf);
done:
	return err;
}

/* TOF action frame send function */
static int pdburst_send(pdburst_sm_t *sm, struct ether_addr *da, uint8 type)
{
	wlc_info_t *wlc;
	pdburst_t* burstp = sm->tof_obj;
	ratespec_t rate_override;
	uint16 durid = 60;
	pkcb_fn_t fn = NULL;
	int ret = BCME_ERROR;
	uint16 fc_type;
	pdburst_tsinfo_t tsinfo;
	wlc_bsscfg_t *bsscfg;
	uint8* pbody;
	wlc_pkttag_t *pkttag;
	void *pkt = NULL;
	uint pkt_len;
	uint ftm_len, vs_ie_len = 0;
	pdburst_frame_type_t ftype;
	int txstatus = BCME_OK;
	pdburst_session_info_t bsi;
	ftm_vs_req_params_t req_params;
	ftm_vs_seq_params_t seq_params;
	ftm_vs_sec_params_t sec_params;
	ftm_vs_meas_info_t meas_info;
	ftm_vs_mf_buf_t mf_buf;
	ftm_vs_timing_params_t timing_params;
#ifdef TOF_DEBUG_TIME
	char eabuf[32];
	bcm_ether_ntoa(da, eabuf);
	TOF_PRINTF(("send type %d, %s %d %d\n", type, eabuf, sm->tof_dialog, sm->tof_followup));
#endif // endif
	wlcband_t *band = NULL;

	bzero(&req_params, sizeof(req_params));
	bzero(&seq_params, sizeof(seq_params));
	bzero(&sec_params, sizeof(sec_params));
	bzero(&meas_info, sizeof(meas_info));
	bzero(&mf_buf, sizeof(mf_buf));
	bzero(&timing_params, sizeof(timing_params));

	wlc = burstp->wlc;

#if defined(TOF_PROFILE)
	wlc_read_tsf(wlc, &tsf_lastm, &tsf_hi);
	tsf_lastm -= tsf_start;
#endif // endif
	if (!burstp->svc_funcs) {
		ret =  WL_PROXD_E_PROTO;
		goto done;
	}

	if (burstp->bsscfg) {
		bsscfg = burstp->bsscfg;
	} else {
		if (!(bsscfg = wlc_bsscfg_find_by_hwaddr(wlc, &sm->tof_selfea))) {
#ifdef TOF_DEBUG_TIME
			FTM_ERR(("%s()wl%d: Can't find BSSCFG from matching selfea " MACF "\n",
				__FUNCTION__, wlc->pub->unit,
				ETHERP_TO_MACF(&sm->tof_selfea)));
#endif // endif
			return BCME_ERROR;
		}
	}

	if (!BURST_SEQ_EN(burstp) && (type == TOF_TYPE_MEASURE || type == TOF_TYPE_MEASURE_END)) {
#ifdef TOF_DEBUG_TIME
		TOF_PRINTF(("*** rate spec 0x%08x\n", burstp->configp->ratespec));
#endif // endif
		band = wlc->bandstate[CHSPEC_BANDUNIT(burstp->configp->chanspec)];
		if (RSPEC_ACTIVE(band->rspec_override)) {
			rate_override = band->rspec_override;
		} else {
			rate_override = burstp->configp->ratespec;
		}
	} else {
		rate_override = PROXD_DEFAULT_TX_RATE;
		/* rate_override = 0x02030009; */
	}

	if ((rate_override & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_UNSPECIFIED) {
		if (CHSPEC_IS80(burstp->configp->chanspec))
			rate_override |= WL_RSPEC_BW_80MHZ;
		else if (CHSPEC_IS40(burstp->configp->chanspec))
			rate_override |= WL_RSPEC_BW_40MHZ;
		else if (CHSPEC_IS20(burstp->configp->chanspec))
			rate_override |= WL_RSPEC_BW_20MHZ;
	}

	if (RSPEC_ISVHT(rate_override))
		sm->tof_txvht = TRUE;

	ftype = pdburst_get_ftype(type);
	if (ftype == PDBURST_FRAME_TYPE_REQ) {
		/*   about to send FTM request, set the req flag here.
		 **  Also initialize the num_meas to 0 here.
		*/
		burstp->result_flags |= WL_PROXD_REQUEST_SENT;
		burstp->num_meas = 0;
		burstp->seq_state = TOF_SEQ_NONE;
	}
	else if (ftype != PDBURST_FRAME_TYPE_MEAS) {
		FTM_ERR(("Unknown TOF pkt type	%d\n", type));
		ret = WL_PROXD_E_FRAME_TYPE;
		goto done;
	}
	/* avoid vs_ie for AVB responder */
	if (BURST_SEQ_EN(burstp) || (ftype == PDBURST_FRAME_TYPE_REQ)) {
		ret = pdburst_init_bsi(burstp, ftype, &req_params, &seq_params,
			&sec_params, &mf_buf, &meas_info, &timing_params, &bsi);
		if (ret != BCME_OK)
			goto done;
	}

	ftm_len = PROTOCB(burstp, get_frame_len, (burstp->ctx, ftype, &fc_type));
	/* avoid vs_ie for AVB responder */
	if (BURST_SEQ_EN(burstp) || (ftype == PDBURST_FRAME_TYPE_REQ)) {
		vs_ie_len +=
			PROTOCB(burstp, vs_get_frame_len, (burstp->ctx, ftype, &bsi));
	}

	/* get allocation of action frame */
	pkt = proxd_alloc_action_frame(wlc->pdsvc_info, bsscfg, da,
		&burstp->configp->cur_ether_addr, &burstp->configp->bssid,
		ftm_len + vs_ie_len,
		&pbody, (burstp->flags & WL_PROXD_SESSION_FLAG_SECURE) ?
		DOT11_ACTION_CAT_PDPA: DOT11_ACTION_CAT_PUBLIC, 0);
	if (pkt == NULL) {
		ret = BCME_NOMEM;
		goto done;
	}

	pkttag = WLPKTTAG(pkt);
	if (sm->tof_retrycnt && !CHSPEC_IS5G(burstp->configp->chanspec)) {
		pkttag->flags |= WLF_USERTS;
	}
	pkttag->shared.packetid = burstp->configp->chanspec;
	WLPKTTAGBSSCFGSET(pkt, bsscfg->_idx);

	bzero(&tsinfo, sizeof(tsinfo));
	if (type == TOF_TYPE_REQ_START || type == TOF_TYPE_REQ_END)
	{
		fn = pdburst_tx_complete;

#ifdef TOF_PROFILE
		TOF_PRINTF(("EVENT = %d, TIME = 0x%0x\n", (type+10), tsf_lastm));
#endif // endif
		wlc_update_shm(burstp->wlc, burstp->shmemptr + M_TOF_FLAGS_OFFSET(wlc),
			1 << TOF_RX_FTM_NBIT, 1 << TOF_RX_FTM_NBIT);
	} else if (type == TOF_TYPE_MEASURE_END || type == TOF_TYPE_MEASURE) {
		int nextFrames;

		fn = pdburst_tx_complete;
		nextFrames = pdburst_total_framecnt(burstp);

		if (nextFrames)
			nextFrames = nextFrames- sm->tof_txpktcnt;
		else
			nextFrames = burstp->measurecnt - sm->tof_txcnt;

		if (burstp->sm->tof_legacypeer == TOF_LEGACY_AP)
			nextFrames--;

		durid = wlc_compute_frame_dur(wlc, CHSPEC_BANDTYPE(bsscfg->current_bss->chanspec),
				rate_override, WLC_LONG_PREAMBLE, 0);
		if (nextFrames > 0 && burstp->tunep->rsv_media)
			durid += burstp->tunep->rsv_media;

		wlc_write_shm(wlc, burstp->shmemptr + M_TOF_DOT11DUR_OFFSET(wlc), durid);

		/* mark measurement pkt with special packet ID to identify it later */
		pkttag->shared.packetid |= (PROXD_FTM_PACKET_TAG |
			PROXD_MEASUREMENT_PKTID);
		burstp->delayfree |= TOF_DFREE_TXDONE;

		tsinfo.toa_err = BURST_ERR_PICO;
		tsinfo.tod_err = BURST_ERR_PICO;
		tsinfo.toa = burstp->tx_t4;
		tsinfo.tod = burstp->tx_t1;
		tsinfo.tx_id = sm->tof_dialog;
		tsinfo.ts_id = sm->tof_followup;

		if (type == TOF_TYPE_MEASURE || type == TOF_TYPE_MEASURE_END) {
			burstp->seq_state = get_next_seq_state(burstp->sm->tof_txcnt);
		}
		pdburst_measure(burstp, TOF_RESET);
#ifdef TOF_PROFILE
		TOF_PRINTF(("EVENT = %d, TOKEN=%d FOLLOW_TOKEN=%d TIME = 0x%0x\n", (type+10),
			tsinfo.tx_id, tsinfo.ts_id, tsf_lastm));
#endif // endif
	}

	ret = PROTOCB(burstp, prep_tx, (burstp->ctx, ftype, pkt, pbody,
		(ftm_len + vs_ie_len), &ftm_len, &tsinfo));
	if (ret != BCME_OK) {
		goto done;
	}
	/* avoid vs_ie for AVB responder */
	if (BURST_SEQ_EN(burstp) || (ftype == PDBURST_FRAME_TYPE_REQ)) {
		ret = PROTOCB(burstp, vs_prep_tx, (burstp->ctx,
			ftype, pbody + ftm_len, vs_ie_len, &vs_ie_len, &bsi));
		if (ret != BCME_OK) {
			goto done;
		}
	}
	if (fn) {
		wlc_pcb_fn_register(wlc->pcb, fn, burstp, pkt);
	}

	if (burstp->scanestarted)
		txstatus = WL_PROXD_E_SCAN_INPROCESS;

	/* adjust packet length based on data actually added */
	pkt_len = PKTLEN(wlc->osh, pkt);
	PKTSETLEN(wlc->osh, pkt, pkt_len);

	if (proxd_tx(wlc->pdsvc_info, pkt, bsscfg, rate_override, txstatus)) {
		burstp->txcnt++;
	} else {
		ret = BCME_TXFAIL;
		pkt = NULL; /* will be freed by proxd_tx... */
	}

done:
	if (ret != BCME_OK) {
		if (pkt)
			PKTFREE(wlc->osh, pkt, FALSE);
		FTM_ERR(("wl%d: %s: status %d\n",
			wlc->pub->unit, __FUNCTION__, ret));
	}

	return ret;
}

static bool
pdburst_ts_valid(tof_tslist_t *listp, int16 meas_idx, int32 dT, int32 k)
{
	bool ret = TRUE;

	/* Filtering wrong/out-of-bound measurements */
	/* Just ignore upper bound for K-value calibration */
	if (((listp->tslist[meas_idx].t3 - listp->tslist[meas_idx].t2) == 0) ||
		((listp->tslist[meas_idx].t4 - listp->tslist[meas_idx].t1) == 0) ||
		((k != 0) && (dT > TOF_RTT_MAXVAL)) ||
		(dT <= TOF_RTT_MINVAL)) {
		ret = FALSE;
	}

	return ret;
}

#ifdef WL_PROXD_OUTLIER_FILTERING
/* Median Absolute Deviatin (MAD) Scaling Factor */
/* 1.25 (5/4) -> 78% CDF point, lower value more percentage */
#define MAD_SCALE 5u
#define MAD_SHIFT 2u
#define MAD_RANGE 5u
/* Bound is decided as (Median - (MAD_RANGE*MAD)) to (Median + (MAD_RANGE*MAD)) */

static int32
pdburst_filter_outliers(pdburst_t* burstp, int32 *arr, int16 *n)
{
	int i, j;
	int32 median, mad;
	int32 *local_arr;
	int32 lower_bound, upper_bound;
	int16 cnt = *n;

	if (cnt > 2) {
		local_arr = (int32 *)MALLOCZ(burstp->wlc->osh, sizeof(int32) * cnt);
		/* If memory not allocated, just return the average value without */
		/* any outlier filtering */
		if (local_arr != NULL) {
			/* Sort array in ascending order */
			wlc_pdsvc_sortasc(arr, (uint16)cnt);
			/* Median of the set */
			median = wlc_pdsvc_median(arr, (uint16)cnt);
			/* Absolute deviation from median */
			for (i = 0; i < cnt; i++) {
				local_arr[i] = ABS((signed)(arr[i] - median));
			}
			/* Sort the absolute deviation and then find the median */
			/* Also known as Median Absolute Deviation (MAD) */
			/* MAD = b * Median(abs(xi - Mi)) */
			wlc_pdsvc_sortasc(local_arr, (uint16)cnt);
			mad = wlc_pdsvc_median(local_arr, (uint16)cnt);
			mad = MAD_SCALE * mad >> MAD_SHIFT;
			if (mad == 0) {
				mad = 1;
			}
			lower_bound = median - (MAD_RANGE * mad);
			upper_bound = median + (MAD_RANGE * mad);
			TOF_PRINTF(("Mad=%d, Lower/Upper Limit =%d/%d\n", mad, lower_bound,
				upper_bound));
			/* Filter any outliers out of these bounds */
			j = 0;
			for (i = 0; i < cnt; i++) {
				if (arr[i] >= lower_bound && arr[i] <= upper_bound) {
					local_arr[j] = arr[i];
					j++;
				}
			}
			/* Copy the filtered measurements and free the local_arr */
			memcpy(arr, local_arr, sizeof(int32) * j);
			MFREE(burstp->wlc->osh, local_arr, sizeof(int32) * cnt);
			*n = j;
			cnt = *n;
		}
	}

	return wlc_pdsvc_average(arr, cnt);
}
#endif /* WL_PROXD_OUTLIER_FILTERING */

/* calculate the distance based on measure results */
static int pdburst_analyze_results(pdburst_t* burstp, tof_tslist_t *listp,
	pdburst_results_t *res, int32 *difavg, wl_proxd_status_t reason)
{
	ftmts_t *toftsp;
	int32 *list;
	uint32 sigma;
	int16 i, cnt;
	uint32 k, d;
	int32 mean = 0;
	wl_proxd_rtt_sample_t *ftmp = &res->rtt[0];
	wl_proxd_snr_t tof_target_snr = 0;
	wl_proxd_bitflips_t tof_target_bitflips = 0;

	if (BURST_SEQ_EN(burstp)) {
		k = 0;
		d = 10; /* to use timestamp stored in 0.1ns units */
	} else {
		k = pdburst_get_kval(burstp, TRUE, FALSE);
		d = 1; /* to use timestamp stored in 1ns units */
	}

	if (listp && listp->tscnt && listp->tslist && ftmp) {
		list = (int32 *)MALLOCZ(burstp->wlc->osh, sizeof(uint32)*listp->tscnt);
		if (list == NULL) {
			return BCME_NOMEM;
		}
		toftsp = &listp->tslist[0];
		res->avg_rtt.rssi = 0;
		for (i = 0, cnt = 0; i < listp->tscnt; i++, toftsp++, ftmp++) {
			bool valid_ts = TRUE;
			int32 dT = 0;
			uint32 rspec = listp->tslist[i].rspec;
			char rate_buf[20];
			tof_target_snr = 0;
			tof_target_bitflips = 0;
			if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_RATE)
				sprintf(rate_buf, "rate %d Mbps", (rspec & WL_RSPEC_RATE_MASK)/2);
			else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HT)
				sprintf(rate_buf, "ht mcs %d", (rspec & WL_RSPEC_RATE_MASK));
			else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT)
				sprintf(rate_buf, "vht mcs %dx%d", (rspec & WL_RSPEC_VHT_MCS_MASK),
					(rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT);
			else
				sprintf(rate_buf, "unknown");

			if (burstp->seq_en) {
				dT = ((listp->tslist[i].t4-listp->tslist[i].t1) % burstp->seq_len) -
					(listp->tslist[i].t3-listp->tslist[i].t2);
			} else {
				dT = (listp->tslist[i].t4-listp->tslist[i].t1) -
					(listp->tslist[i].t3-listp->tslist[i].t2);

			}

			ftmp->version = WL_PROXD_RTT_SAMPLE_VERSION_2;
			ftmp->length = sizeof(*ftmp) - OFFSETOF(wl_proxd_rtt_sample_t, id);
			ftmp->id = listp->tslist[i].tx_id;
			FTM_INIT_INTVL(&ftmp->rtt, dT, WL_PROXD_TMU_NANO_SEC);
			ftmp->rssi = listp->tslist[i].rssi;
			/* Add rssi to average rssi */
			res->avg_rtt.rssi += ftmp->rssi;
			ftmp->ratespec = listp->tslist[i].rspec;
			ftmp->snr = toftsp->snr;
			ftmp->bitflips = toftsp->bitflips;
			ftmp->status = reason;
			ftmp->tof_phy_error = toftsp->tof_phy_error;
			ftmp->tof_tgt_phy_error = ftm_vs_tgt_snr_bitfips(burstp->ctx,
				burstp->tunep->snr_thresh, burstp->tunep->bitflip_thresh,
				&tof_target_snr, &tof_target_bitflips);
			ftmp->tof_tgt_snr = tof_target_snr;
			ftmp->tof_tgt_bitflips = tof_target_bitflips;
			ftmp->flags |= (listp->tslist[i].discard? WL_PROXD_RTT_SAMPLE_DISCARD : 0);
			ftmp->coreid = burstp->core;
			ftmp->chanspec = burstp->configp->chanspec;
			FTM_TRACE(("%s (%d)T1 %u T2 %u T3 %u T4 %u Delta %d "
				"rssi %d gd %d hadj %d dif "
				"%d%s tof_phy_error %x \n", rate_buf, ftmp->id,
				listp->tslist[i].t1, listp->tslist[i].t2, listp->tslist[i].t3,
				listp->tslist[i].t4, dT, listp->tslist[i].rssi,
				listp->tslist[i].gd, listp->tslist[i].adj,
				listp->tslist[i].gd - listp->tslist[i].adj,
				listp->tslist[i].discard? "	discard" : "",
				ftmp->tof_phy_error));
			FTM_TRACE(("snr %d bitflips %d tof_phy_error %x tof_tgt_phy_error %x "
				"tof_target_snr = %d tof_target_bitflips = %d\n",
				ftmp->snr, ftmp->bitflips,
				ftmp->tof_phy_error, ftmp->tof_tgt_phy_error, ftmp->tof_tgt_snr,
				ftmp->tof_tgt_bitflips));
			/* For sample number 0,3,6, etc(SETUP frames), skip valid ts updation */
			if (BURST_SEQ_EN(burstp)) {
				if ((i % TOF_DEFAULT_FTMCNT_SEQ == 0) ||
					(i % TOF_DEFAULT_FTMCNT_SEQ == 2))
				{
					toftsp->discard = TRUE;
					ftmp->flags |= WL_PROXD_RTT_SAMPLE_DISCARD;
					continue;
				}
				if (toftsp->discard)
					continue;

				if (dT < 0) {
					if (dT < -burstp->seq_len/2) {
						dT += burstp->seq_len;
					}
				/*
					if 2 devices are close than dT could become
					negative but then will wrap around
				*/
				} else if (dT > (burstp->seq_len >> 1)) {
					dT -= burstp->seq_len;
				}
				if  (((burstp->seq_state == TOF_SEQ_NONE) ||
					(burstp->seq_state == TOF_SEQ_DONE))) {
					toftsp->discard = TRUE;
					ftmp->flags |= WL_PROXD_RTT_SAMPLE_DISCARD;
					continue;
				}
			} else {
				/* Check for valid measurements for Non-SEQ */
				valid_ts = pdburst_ts_valid(listp, i, dT, k);
			}
			if (valid_ts) {
				list[cnt] = dT;
				ftmp->distance = dT;
				cnt++;
				if (difavg) {
					*difavg += (toftsp->gd - toftsp->adj);
				}
			} else {
				TOF_PRINTF(("Discard AVBERR(%d) Delta %d\n", ftmp->id, dT));
				toftsp->discard = TRUE;
				ftmp->flags |= WL_PROXD_RTT_SAMPLE_DISCARD;
				/* Update the status for this sample */
				ftmp->status =  WL_PROXD_E_INVALIDMEAS;
			}
		}
		TOF_PRINTF(("Valid TS Cnt %d\n", cnt));
		if (listp->tscnt) {
			res->avg_rtt.rssi = res->avg_rtt.rssi * (-10)/listp->tscnt;
			if ((res->avg_rtt.rssi%10) >= 5)
				res->avg_rtt.rssi = -res->avg_rtt.rssi/10 - 1;
			else
				res->avg_rtt.rssi = -res->avg_rtt.rssi/10;
		} else {
			res->avg_rtt.rssi = 0;
		}
		res->avg_rtt.ratespec = listp->tslist[0].rspec;
		res->avg_rtt.chanspec = burstp->configp->chanspec;
		res->avg_rtt.version = WL_PROXD_RTT_SAMPLE_VERSION_2;
		res->avg_rtt.length = sizeof(wl_proxd_rtt_sample_t) -
			OFFSETOF(wl_proxd_rtt_sample_t, id);
		burstp->avgrssi = res->avg_rtt.rssi;

#ifdef WL_PROXD_OUTLIER_FILTERING
		mean = pdburst_filter_outliers(burstp, list, &cnt);
		TOF_PRINTF(("Filt. Valid TS Cnt %d\n", cnt));
#else
		mean = wlc_pdsvc_average(list, cnt);
#endif /* WL_PROXD_OUTLIER_FILTERING */
		/* Compute SD on the filtered valid measurements */
		sigma = wlc_pdsvc_deviation(list, mean, cnt, 1);

		burstp->meanrtt = mean;
		burstp->sdrtt = sigma;
		burstp->frmcnt = cnt;

		FTM_INIT_INTVL(&res->avg_rtt.rtt, mean, WL_PROXD_TMU_NANO_SEC);
		res->sd_rtt = sigma;
		res->num_valid_rtt = cnt;

		FTM_TRACE(("cnt %d mean %d k %d RTT %d deviation %d.%d avgrssi %d\n",
			cnt, mean, k, mean, sigma/10, sigma%10, burstp->avgrssi));

		if (cnt && difavg) {
			*difavg /= cnt;
			TOF_PRINTF(("Average Dif %d\n", *difavg));
		}
		if (!BURST_SEQ_EN(burstp)) {
			if (mean > 0) {
				mean = TOF_NS_TO_16THMETER(mean, d);
			} else {
				mean = 0;
			}
		} else {
			mean = TOF_NS_TO_16THMETER(mean-(int32)k, d);
		}
		MFREE(burstp->wlc->osh, list, sizeof(uint32)*listp->tscnt);

		return mean;
	}
	return -1;
}

#ifdef TOF_COLLECT_REMOTE
/* generate TOF events */
static void pdburst_event(pdburst_sm_t *sm, uint8 eventtype)
{
	pdburst_t* burstp = sm->tof_obj;

	if (burstp->collectp && eventtype == WLC_E_PROXD_START) {
		burstp->collectp->collect_cnt = 0;
	}
}
#endif /* TOF_COLLECT_REMOTE */

/* TOF get report results and state machine goes to CONFIRM state */
static void pdburst_report_done(pdburst_sm_t *sm, wl_proxd_status_t reason)
{
	pdburst_t* burstp = sm->tof_obj;

	sm->tof_state = TOF_STATE_ICONFIRM;

	pdburst_deactivate_pm(burstp);

	if (burstp->flags & WL_PROXD_SESSION_FLAG_NETRUAL) {
		pdburst_reset(sm, WL_PROXD_MODE_TARGET, WL_PROXD_E_NOTSTARTED);
	}

	MDBG(("TS:%d should be in pwr_down now\n", get_usts(burstp->wlc)));

}

static int pdburst_target_done(pdburst_sm_t *sm, wl_proxd_status_t reason)
{
	pdburst_t* burstp = sm->tof_obj;

#ifdef TOF_DEBUG_TIME
	switch (reason) {
		case WL_PROXD_E_OK:
			TOF_PRINTF(("OK\n"));
			break;
		case WL_PROXD_E_TIMEOUT:
			TOF_PRINTF(("TIMEOUT\n"));
			break;
		case WL_PROXD_E_NOACK:
			TOF_PRINTF(("NOACK\n"));
			break;
		case WL_PROXD_E_INVALIDMEAS:
			TOF_PRINTF(("INVALIDMEAS\n"));
			break;
		case WL_PROXD_E_CANCELED:
			TOF_PRINTF(("ABORT\n"));
			break;
		default:
			TOF_PRINTF(("ERROR\n"));
			break;
	}
#endif /* TOF_DEBUG_TIME */

#ifdef WL_RANGE_SEQ
	if (!burstp) {
		ASSERT(0);
		/* No point proceeding further if burstp is invalid */
		OSL_SYS_HALT();
	}
#endif /* WL_RANGE_SEQ */

	phy_tof_cmd(WLC_PI(burstp->wlc), FALSE, 0);
	if (FTM_BSSCFG_SECURE(wlc_ftm_get_handle(burstp->wlc), burstp->bsscfg)) {
		phy_rxgcrs_sel_classifier((phy_info_t *) WLC_PI(burstp->wlc),
			TOF_CLASSIFIER_BPHY_ON_OFDM_ON);
	}

	if (burstp->ftm_tx_timer_active) {
		wlc_hrt_del_timeout(burstp->ftm_tx_timer);
		burstp->ftm_tx_timer_active = FALSE;
	}

	/* stop HRT duration timer if it is running */
	if (burstp->duration_timer_active) {
		wlc_hrt_del_timeout(burstp->duration_timer);
		burstp->duration_timer_active = FALSE;
	}

	if (burstp->collectp != NULL && burstp->collectp->configp != NULL) {
		memcpy(burstp->collectp->configp, burstp->configp, sizeof(*burstp->configp));
	}
#ifdef TOF_COLLECT
#ifdef WL_RANGE_SEQ
	if (burstp->collectp) {
#else
	if (burstp && burstp->collectp) {
#endif // endif
		(void)pdburst_collect_event(burstp);
#ifdef WL_RANGE_SEQ
		burstp->collectp->collect_cnt = 0;
#endif // endif
	}
#endif /* TOF_COLLECT */

	/*
	*In burst_done call sequence, pdburst_destroy is called
	*in case of single burst or last burst which will FREE the
	*burstp struct. So any access to burstp
	*should be done before burst_done
	*/
	burstp->delayfree |= TOF_DFREE_INSVCCB;
	(void)PROTOCB(burstp, done, (burstp->ctx, NULL));
	burstp->delayfree &= ~TOF_DFREE_INSVCCB;
	if (burstp->destroyed && !burstp->delayfree) {
		MFREE(burstp->wlc->osh, burstp, sizeof(pdburst_t));
	} else {
		burstp->smstoped = TRUE;
	}
	return TOF_RET_SLEEP;
}

static int pdburst_result(pdburst_t* burstp, wl_proxd_status_t reason)
{
	tof_tslist_t *listp = &burstp->tof_tslist;
	int32 dif = 0;
	int distance;
	pdburst_results_t *res;
	int len = OFFSETOF(pdburst_results_t, rtt);
#ifdef TOF_DEBUG_TIME2
	uint32 t;
#endif // endif

	len += sizeof(wl_proxd_rtt_sample_t)*listp->tscnt;

	res = (pdburst_results_t *)MALLOCZ(burstp->wlc->osh, len);
	if (res == NULL) {
		//FTM_LOG_STATUS(burstp->wlc->pdsvc_info->ftm, BCME_NOMEM,
		//	(("wl%d: %s: TOF confirm alloc memory failed\n",
		//	PDBURST_UNIT(burstp), __FUNCTION__)));
		return BCME_NOMEM;
	}

	distance = pdburst_analyze_results(burstp, listp, res, &dif, reason);
	if (distance == -1)
		res->dist_unit = PD_DIST_UNKNOWN;
	else
		res->dist_unit = PD_DIST_1BY16M;

	res->num_rtt = listp->tscnt;
	res->status = reason;
	if (!res->num_rtt && reason == WL_PROXD_E_NOACK)
		res->flags = WL_PROXD_RESULT_FLAG_FATAL;
	else {
		res->flags = WL_PRXOD_RESULT_FLAG_NONE;
		if (BURST_IS_VHTACK(burstp) && RSPEC_ISVHT(burstp->configp->ratespec)) {
			res->flags |= WL_PROXD_RESULT_FLAG_VHTACK; /* with vhtack */
		}
	}

	res->flags |= burstp->result_flags;
	res->num_meas = burstp->num_meas;

	if (burstp->frmcnt == 0 && burstp->sm->tof_reason == WL_PROXD_E_OK) {
		burstp->sm->tof_reason = WL_PROXD_E_INVALIDMEAS;
		res->status = WL_PROXD_E_INVALIDMEAS;
	}
#ifdef TOF_DEBUG_TIME
	if (distance == -1)
		TOF_PRINTF(("Distance -1 meter\n"));
	else
		TOF_PRINTF(("Distance %d.%d meter\n", (distance>>4), ((distance & 0xf) * 625)));
#endif // endif
	burstp->distance = distance;
	res->avg_dist = distance;

#ifdef TOF_DEBUG_TIME2
	wlc_read_tsf(burstp->wlc, &t, &tsf_hi);
	t = t -tsf_start;
	TOF_PRINTF(("Scan %d Txreq %d Rxack %d 1stM %d ", tsf_scanstart, tsf_txreq,
		tsf_rxack, tsf_rxm));
	TOF_PRINTF(("lastM %d Confirm %d Event %d tmo %d\n", tsf_lastm, tsf_confirm, t, tsf_tmo));
#endif // endif

	if (!(burstp->delayfree & TOF_DFREE_TXDONE)) {
		/* De-register callback at the end of every burst */
		wlc_pcb_fn_find(burstp->wlc->pcb, pdburst_tx_complete, burstp, TRUE);
	}

#ifdef TOF_COLLECT_INLINE
	pdburst_collect_inline_results(burstp, res);
#endif /* TOF_COLLECT_INLINE */
	burstp->delayfree |= TOF_DFREE_INSVCCB;
	(void)PROTOCB(burstp, done, (burstp->ctx, res));
	burstp->delayfree &= ~TOF_DFREE_INSVCCB;
	MFREE(burstp->wlc->osh, res, len);

	return BCME_OK;
}

void pdburst_cancel(pdburst_t* burstp, wl_proxd_status_t reason)
{
	ASSERT(burstp != NULL);
	(void)pdburst_confirmed(burstp->sm, reason);
}

/* TOF get final results and state machine goes to CONFIRM state */
static int pdburst_confirmed(pdburst_sm_t *sm, wl_proxd_status_t reason)
{
	pdburst_t* burstp = sm->tof_obj;
#ifdef WL_RANGE_SEQ
	wlc_info_t *wlc;
#else
	wlc_info_t *wlc = burstp->wlc;
#endif // endif
	int tofret = TOF_RET_END;

#ifdef WL_RANGE_SEQ
	if (!burstp) {
		ASSERT(0);
		/* No point proceeding further if burstp is invalid */
		OSL_SYS_HALT();
	}
	wlc = burstp->wlc;
#endif // endif
	pdburst_measure(burstp, TOF_RESET);
	if (burstp->smstoped)
		return tofret;

#ifdef TOF_DEBUG_TIME2
	wlc_read_tsf(burstp->wlc, &tsf_confirm, &tsf_hi);
	tsf_confirm = tsf_confirm - tsf_start;
#endif // endif

	sm->tof_state = TOF_STATE_ICONFIRM;
	if (burstp->collectp != NULL && burstp->collectp->configp != NULL) {
		memcpy(burstp->collectp->configp, burstp->configp, sizeof(*burstp->configp));
	}

	pdburst_deactivate_pm(burstp);

	sm->tof_reason = reason;
	pdburst_hw(burstp, FALSE, FALSE);

#ifdef TOF_DEBUG_TIME
	switch (reason) {
		case WL_PROXD_E_OK:
			TOF_PRINTF(("OK\n"));
			break;
		case WL_PROXD_E_TIMEOUT:
			TOF_PRINTF(("TIMEOUT\n"));
			break;
		case WL_PROXD_E_NOACK:
			TOF_PRINTF(("NOACK\n"));
			break;
		case WL_PROXD_E_INVALIDMEAS:
			TOF_PRINTF(("INVALIDMEAS\n"));
			break;
		case WL_PROXD_E_CANCELED:
			TOF_PRINTF(("ABORT\n"));
			break;
		default:
			TOF_PRINTF(("ERROR\n"));
			break;
	}
#endif /* TOF_DEBUG_TIME */
	if (burstp->timeractive) {
		burstp->timeractive = FALSE;
		wl_del_timer(burstp->wlc->wl, burstp->timer);
	}

	pdburst_send_mf_stats_event(burstp);

#ifdef TOF_COLLECT_REMOTE
	if (burstp->collectp && burstp->collectp->remote_request) {
		/* Remote collect data section is done */
		burstp->collectp->remote_request = FALSE;
		return tofret;
	}
#endif /* TOF_COLLECT_REMOTE */

#ifdef TOF_COLLECT
#ifdef WL_RANGE_SEQ
	if (burstp->collectp) {
#else
	if (burstp && burstp->collectp) {
#endif // endif
		(void)pdburst_collect_event(burstp);
#ifdef WL_RANGE_SEQ
		burstp->collectp->collect_cnt = 0;
#endif // endif
	}
#endif /* TOF_COLLECT */

	if (burstp->svc_funcs && burstp->svc_funcs->done && burstp->configp) {
		(void)pdburst_result(burstp, reason);
		pdburst_pwron(burstp, FALSE);
		ftm_vs_reset_target_side_data(burstp->ctx);
		if (burstp->destroyed && !burstp->delayfree) {
#ifdef TOF_DEBUG_TIME
			TOF_PRINTF(("delayfree burstp %p confirmed\n", OSL_OBFUSCATE_BUF(burstp)));
#endif // endif
			MFREE(wlc->osh, burstp, sizeof(pdburst_t));
		}
	}
	return tofret;
}

#ifdef TOF_COLLECT
static int pdburst_collect_mem(wlc_info_t *wlc, pdburst_collect_t **collect, bool alloc)
{

	int i;
	pdburst_collect_t *collectp = *collect;

	if (alloc) {
		collectp = MALLOCZ(wlc->osh, sizeof(pdburst_collect_t));
		if (!collectp) {
			FTM_ERR(("MALLOC failed %s\n", __FUNCTION__));
			*collect = NULL;
			return BCME_NOMEM;
		}
		collectp->collect_cnt = 0;
		collectp->collect_size = MAX_COLLECT_COUNT;
		collectp->collect = MALLOCZ(wlc->osh, collectp->collect_size *
			sizeof(wl_proxd_collect_data_t));
		if (collectp->collect) {
			/* initialize wl_proxd_collect_data_t version and length */
			for (i = 0; i < collectp->collect_size; i++) {
				collectp->collect[i].version = WL_PROXD_COLLECT_DATA_VERSION_3;
				collectp->collect[i].len = sizeof(wl_proxd_collect_data_t) -
					OFFSETOF(wl_proxd_collect_data_t, info);
			}
		}
#ifdef WL_RANGE_SEQ
		collectp->collect_buf = MALLOCZ(wlc->osh, K_TOF_COLLECT_HRAW_SIZE_20MHZ *
#else
		collectp->collect_buf = MALLOCZ(wlc->osh, K_TOF_COLLECT_HRAW_SIZE_80MHZ *
#endif // endif
			sizeof(uint32));
		collectp->collect_header = MALLOCZ(wlc->osh, sizeof(wl_proxd_collect_header_t));
		collectp->chan = MALLOCZ(wlc->osh,
				(PHY_CORE_MAX + 1) * K_TOF_COLLECT_CHAN_SIZE * sizeof(uint32));
		collectp->configp = MALLOCZ(wlc->osh, sizeof(*collectp->configp));
#ifdef WL_RANGE_SEQ
		collectp->collect_method |= WL_PROXD_COLLECT_METHOD_TYPE_EVENT;
#endif /* WL_RANGE_SEQ */
	}

	if (!alloc || !collectp->collect || !collectp->collect_buf ||
		!collectp->collect_header || !collectp->chan || !collectp->configp) {
		if (collectp->collect) {
			MFREE(wlc->osh, collectp->collect, (collectp->collect_size) *
				sizeof(wl_proxd_collect_data_t));
		}
		if (collectp->collect_buf) {
			MFREE(wlc->osh, collectp->collect_buf,
#ifdef WL_RANGE_SEQ
				K_TOF_COLLECT_HRAW_SIZE_20MHZ * sizeof(uint32));
#else
				K_TOF_COLLECT_HRAW_SIZE_80MHZ * sizeof(uint32));
#endif /* WL_RANGE_SEQ */
		}
		if (collectp->collect_header) {
			MFREE(wlc->osh, collectp->collect_header,
				sizeof(wl_proxd_collect_header_t));
		}
		if (collectp->chan) {
			MFREE(wlc->osh, collectp->chan,
				(PHY_CORE_MAX + 1) * K_TOF_COLLECT_CHAN_SIZE * sizeof(uint32));
		}

		if (collectp->configp) {
			MFREE(wlc->osh, collectp->configp,
				sizeof(*collectp->configp));
		}
		collectp->collect_size = 0;
#ifdef WL_RANGE_SEQ
		MFREE(wlc->osh, collectp, sizeof(pdburst_collect_t));
#endif // endif
		if (alloc) {
			FTM_ERR(("%s: MALLOC failed\n", __FUNCTION__));
		}
		*collect = NULL;
		return BCME_NOMEM;
	}

	*collect = collectp;
	return BCME_OK;
}
#endif /* TOF_COLLECT */
#ifdef TOF_COLLECT_REMOTE
static int pdburst_collect_send(pdburst_sm_t *sm, struct ether_addr *da, uint8 type)
{
	wlc_info_t *wlc;
	pdburst_t* burstp = sm->tof_obj;
	pdburst_collect_t *collectp = burstp->collectp;
	ratespec_t rate_override;
	pkcb_fn_t fn = NULL;
	int ret = BCME_ERROR;
	wlc_bsscfg_t *bsscfg;
	uint8* pbody;
	void *pkt;
	int len;
	pdburst_collect_frm_t *tof_hdr;
	wl_action_frame_t *af;
	TOF_PRINTF(("send type %d, " MACF " %d %d\n", type, ETHERP_TO_MACF(da), sm->tof_dialog,
		sm->tof_followup));

	if (!burstp->svc_funcs || !collectp)
		return BCME_ERROR;

	wlc = burstp->wlc;

	if (burstp->bsscfg) {
		bsscfg = burstp->bsscfg;
	} else {
		bsscfg = wlc_bsscfg_find_by_hwaddr(wlc, &sm->tof_selfea);
	}

	rate_override = PROXD_DEFAULT_TX_RATE;

	if ((rate_override & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_UNSPECIFIED) {
		if (CHSPEC_IS80(burstp->configp->chanspec))
			rate_override |= WL_RSPEC_BW_80MHZ;
		else if (CHSPEC_IS40(burstp->configp->chanspec))
			rate_override |= WL_RSPEC_BW_40MHZ;
		else if (CHSPEC_IS20(burstp->configp->chanspec))
			rate_override |= WL_RSPEC_BW_20MHZ;
	}

	if ((af = (wl_action_frame_t *)MALLOCZ(wlc->osh, WL_WIFI_AF_PARAMS_SIZE)) == NULL)
		return BCME_NOMEM;

	tof_hdr = (pdburst_collect_frm_t *)af->data;
	len = sizeof(pdburst_collect_frm_t);
	if (type == TOF_TYPE_COLLECT_DATA_END || type == TOF_TYPE_COLLECT_DATA) {
		wl_proxd_collect_query_t query;

		bzero(&query, sizeof(query));
		query.method = htol32(PROXD_TOF_METHOD);

		if (collectp->remote_cnt == 0) {
			query.request = PROXD_COLLECT_QUERY_HEADER;
			ret = pdburst_collection(burstp->wlc, collectp, &query,
				tof_hdr->data, ACTION_FRAME_SIZE, &tof_hdr->length);
		} else {
			if (!collectp->collect ||
				collectp->remote_cnt > collectp->collect_cnt ||
				collectp->remote_cnt > collectp->collect_size) {
				MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
				return BCME_ERROR;
			}

			query.request = PROXD_COLLECT_QUERY_DATA;
			query.index = collectp->remote_cnt - 1;
			ret = pdburst_collection(burstp->wlc, collectp, &query,
				tof_hdr->data, ACTION_FRAME_SIZE, &tof_hdr->length);
		}

		if (ret != BCME_OK || tof_hdr->length <= 0) {
			MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
			return BCME_ERROR;
		}
		len	+= tof_hdr->length - 1;
	}

	/* get allocation of action frame */
	if ((pkt = proxd_alloc_action_frame(wlc->pdsvc_info, bsscfg, da,
		&burstp->configp->cur_ether_addr, &burstp->configp->bssid, len,
		&pbody, DOT11_ACTION_CAT_VS, BRCM_FTM_VS_AF_TYPE)) == NULL) {
		return BCME_NOMEM;
	}

	/* copy action frame payload */
	tof_hdr->tof_type = type;
	tof_hdr->category = DOT11_ACTION_CAT_VS;
	memcpy(tof_hdr->OUI, BRCM_PROP_OUI, DOT11_OUI_LEN);
	tof_hdr->type = BRCM_FTM_VS_AF_TYPE;
	tof_hdr->subtype = BRCM_FTM_VS_COLLECT_SUBTYPE;
	if (type == TOF_TYPE_COLLECT_DATA_END || type == TOF_TYPE_COLLECT_DATA) {
		if (type == TOF_TYPE_COLLECT_DATA && collectp->collect &&
			collectp->remote_cnt < collectp->collect_cnt)
			tof_hdr->tof_type = TOF_TYPE_COLLECT_DATA;
		else
			tof_hdr->tof_type = TOF_TYPE_COLLECT_DATA_END;
		tof_hdr->index = collectp->remote_cnt;
	}
	bcopy(af->data, pbody, len);

	fn = pdburst_tx_complete;
	wlc_pcb_fn_register(wlc->pcb, fn, burstp, pkt);

	if (proxd_tx(wlc->pdsvc_info, pkt, bsscfg, rate_override, 0)) {
		burstp->txcnt++;
		ret = BCME_OK;
	} else {
		ret = BCME_TXFAIL;
		FTM_ERR(("%s tx failed\n", __FUNCTION__));
	}

	MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
	return ret;
}
#endif /* TOF_COLLECT_REMOTE */

#ifdef TOF_COLLECT
/* TOF collects debug data */
static void pdburst_collect_data(void* collect, int index, bool isTwenty)
{
	pdburst_collect_t * collectp = collect;
	int collect_h_size = 0;
	if (collectp) {
#ifdef WL_RANGE_SEQ
		pdburst_t *pdburstp = collectp->pdburstp;
		wl_proxd_collect_data_t *p_collect, *p_collect_end;
		uint32 *p_buf = collectp->collect_buf;
		int n = 0, n_total = collectp->collect_info.nfft;
#endif /* WL_RANGE_SEQ */

		if (isTwenty) {
			collect_h_size = K_TOF_COLLECT_H_SIZE_20MHZ;
		} else {
			collect_h_size = K_TOF_COLLECT_H_SIZE_80MHZ;
		}

#ifdef WL_RANGE_SEQ
		if (pdburstp && BURST_SEQ_EN(pdburstp) && !index) {
			/* Ignore toast first measurement */
			return;
		}
#else
		wl_proxd_collect_data_t *p_collect, *p_collect_z, *p_collect_end;
		uint32 *p_buf = collectp->collect_buf;
		int n = 0, n_total = collectp->collect_info.nfft;
#endif /* WL_RANGE_SEQ */
		/* Only advance log if id changed */
		p_collect = collectp->collect + collectp->collect_cnt;
#ifndef WL_RANGE_SEQ
		if (collectp->collect_cnt > 0) {
			p_collect_z = p_collect - 1;
			while ((p_collect_z != collectp->collect) && (p_collect_z->info.index)) {
				p_collect_z--;
			}
			if (p_collect_z->info.tof_id == collectp->collect_info.tof_id)
				p_collect = p_collect_z;
		}
#endif /* !WL_RANGE_SEQ */
		p_collect_end = collectp->collect + collectp->collect_size;
		collectp->collect_cnt = (p_collect - collectp->collect);

		collectp->collect_info.index = 0;

#ifdef WL_RANGE_SEQ
		/* The p_collect->H allocated here is for 20MHz only.
		 * The collect_h_size could be more for 80MHz.
		 */
		if (isTwenty) {
#endif // endif
		while (p_collect < p_collect_end) {
			collectp->collect_info.nfft = n_total - n;
			if (collectp->collect_info.nfft > collect_h_size)
				collectp->collect_info.nfft = collect_h_size;
			bcopy((void*)&collectp->collect_info, (void*)&p_collect->info,
				sizeof(wl_proxd_collect_info_t));
			bcopy((void*)(p_buf + n), (void*)&p_collect->H,
				collect_h_size*sizeof(uint32));
			p_collect++;
			collectp->collect_info.index++;
			n += collect_h_size;
			if (n >= n_total)
				break;
		}
#ifdef WL_RANGE_SEQ
		}
#endif // endif

#if defined(TOF_COLLECT)
		memcpy((void *)&(p_collect - 1)->ri_rr, (void *)&collectp->ri_rr,
			FTM_TPK_RI_RR_LEN_SECURE_2_0);
#ifndef WL_PROXD_LOG_OPT
		prhex(NULL, (void *)&collectp->ri_rr,
				FTM_TPK_RI_RR_LEN_SECURE_2_0);
#endif /* WL_PROXD_LOG_OPT */
#endif /* TOF_COLLECT */

#ifndef WL_RANGE_SEQ
		uint32 chan_size = (K_TOF_COLLECT_CHAN_SIZE) >>
			(2 - collectp->collect_info.tof_frame_bw);
		memcpy((void *)&(p_collect - 1)->chan, (void *)&collectp->chan,
			(collectp->collect_info.num_max_cores + 1)*chan_size*sizeof(uint32));
#ifdef TOF_DBG
		prhex("TOF_COLLECT ri_rr",
			(uint8 *)&((p_collect - 1)->ri_rr[0]), FTM_TPK_RI_RR_LEN_SECURE_2_0);
#endif /* TOF_DBG */
#endif /* !WL_RANGE_SEQ */
		collectp->collect_cnt += collectp->collect_info.index;
		collectp->remote_collect = FALSE;
	}
}
#endif /* TOF_COLLECT */

#ifdef TOF_COLLECT_REMOTE
/* initiator gets collect debug data */
static int pdburst_initiator_get_collect_data(pdburst_collect_t * collectp,
	pdburst_collect_frm_t *tof_hdr)
{
	/* TOF_PRINTF(("index %d collect size %d\n", tof_hdr->index, collectp->collect_size)); */

	if (tof_hdr->index == 0 && collectp->collect_header) {
		bcopy(tof_hdr->data, collectp->collect_header, tof_hdr->length);
		collectp->collect_cnt = 0;
		collectp->remote_collect = TRUE;
	} else if (collectp->collect && (tof_hdr->index <= collectp->collect_size)) {
		bcopy(tof_hdr->data, collectp->collect + (tof_hdr->index - 1), tof_hdr->length);
		collectp->collect_cnt = tof_hdr->index;
		collectp->remote_collect = TRUE;
	}

	return (!collectp->collect || tof_hdr->index >= collectp->collect_size);
}
#endif /* TOF_COLLECT_REMOTE */

#if defined(TOF_COLLECT) || defined(TOF_COLLECT_REMOTE)
/* wl proxd_collect function */
int pdburst_collection(wlc_info_t *wlc, void *collectptr, wl_proxd_collect_query_t *query,
	void *buff, int len, uint16 *reqLen)
{
	pdburst_collect_t* collectp = collectptr;
	pdburst_t *burstp = NULL;
	pdburst_sm_t *sm = NULL;

	ASSERT(buff != NULL);

	/* collectp == NULL is IOVAR call */
	if (!pdburst_collect && query->request != PROXD_COLLECT_GET_STATUS &&
		query->request != PROXD_COLLECT_SET_STATUS) {
		return BCME_NOTREADY;
	}

	if (!collectp)
		collectp = pdburst_collect;

	if (collectp) {
		burstp = collectp->pdburstp;
		if (burstp)
			sm = burstp->sm;
	}

	switch (query->request) {
		case PROXD_COLLECT_GET_STATUS:
		case PROXD_COLLECT_SET_STATUS:
		{
			wl_proxd_collect_query_t *reply;

			*reqLen = sizeof(wl_proxd_collect_query_t);
			if (len < sizeof(wl_proxd_collect_query_t))
				return BCME_BUFTOOSHORT;

			reply = (wl_proxd_collect_query_t *)buff;
			bzero(reply, sizeof(wl_proxd_collect_query_t));

			if (query->request == PROXD_COLLECT_GET_STATUS) {
				if (pdburst_collect) {
					reply->status = 1;
					reply->remote = pdburst_collect->remote_collect;
					TOF_PRINTF(("status 1\n"));
				} else {
					reply->status = 0;
					reply->remote = FALSE;
				}
				if (!sm) {
					reply->mode = WL_PROXD_MODE_INITIATOR;
					reply->busy = FALSE;
				} else {
					reply->mode = sm->tof_mode;
					reply->busy = ((sm->tof_mode == WL_PROXD_MODE_TARGET) ?
						(sm->tof_state > TOF_STATE_IDLE) : TRUE) &&
						(sm->tof_state < TOF_STATE_ICONFIRM);
				}
			} else {
				if (query->status) {
					if (!pdburst_collect) {
						if (pdburst_collect_mem(wlc, &pdburst_collect,
							TRUE)) {
							FTM_ERR(("MALLOC failed %s\n",
								__FUNCTION__));
							return BCME_NOMEM;
						}
						/* store the bitmask */
						pdburst_collect->collect_method = query->status;
					}
				} else {
					if (pdburst_collect) {
						pdburst_collect_mem(wlc, &pdburst_collect, FALSE);
					}
				}
			}
			break;
		}

		case PROXD_COLLECT_QUERY_HEADER:
		{
			wl_proxd_collect_header_t *reply;

			*reqLen = sizeof(wl_proxd_collect_header_t);
			if (len < sizeof(wl_proxd_collect_header_t))
				return BCME_BUFTOOSHORT;

			reply = (wl_proxd_collect_header_t *)buff;

			if (!burstp || !collectp) {
				bzero(reply, sizeof(wl_proxd_collect_header_t));
				return BCME_OK;
			}

			if (collectp->remote_collect && collectp->collect_header) {
				bcopy(collectp->collect_header, reply,
					sizeof(wl_proxd_collect_header_t));
			} else {
				bzero(reply, sizeof(wl_proxd_collect_header_t));
				pdburst_collect_prep_header(collectp, reply);
			}
			break;
		}

		case PROXD_COLLECT_QUERY_DATA:
		{
			wl_proxd_collect_data_t *reply;
			wl_proxd_collect_data_t *collect;
			int size;
			int collect_h_size;

			if (!burstp) {
				ASSERT(0);
				return BCME_ERROR;
			}

			if (CHSPEC_IS20(burstp->configp->chanspec)) {
				collect_h_size = K_TOF_COLLECT_H_SIZE_20MHZ;
			} else {
				collect_h_size = K_TOF_COLLECT_H_SIZE_80MHZ;
			}

			if (!collectp->collect)
				return BCME_ERROR;

			if (query->index >= (uint16)collectp->collect_cnt ||
				query->index >= (uint16)collectp->collect_size)
				return BCME_RANGE;

			collect = collectp->collect + query->index;
			size = sizeof(wl_proxd_collect_data_t) -
				(collect_h_size - collect->info.nfft)* sizeof(uint32);

			*reqLen = (uint16)size;
			if (len < size)
				return BCME_BUFTOOSHORT;

			reply = (wl_proxd_collect_data_t *)buff;
			bcopy(collect, reply, size);
			break;
		}

		case PROXD_COLLECT_QUERY_DEBUG:
			return BCME_ERROR;

#ifdef TOF_COLLECT_REMOTE
		case PROXD_COLLECT_REMOTE_REQUEST:
		{
			if (!burstp)
				return BCME_NOTREADY;
			if (burstp->sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
				if (burstp->sm->tof_state != TOF_STATE_IDLE &&
					burstp->sm->tof_state != TOF_STATE_ICONFIRM) {
					return BCME_BUSY;
				}

				if (!collectp->collect) {
					return BCME_ERROR;
				}

				collectp->collect_cnt = 0;
				bzero(collectp->collect, (collectp->collect_size) *
					sizeof(wl_proxd_collect_data_t));

				collectp->remote_request = TRUE;
				pdburst_start(burstp);
			} else
				return  BCME_UNSUPPORTED;
			break;
		}
#endif /* TOF_COLLECT_REMOTE */

		case PROXD_COLLECT_DONE:
			if (burstp) {
				burstp->collectp = NULL;
				pdburst_destroy(&burstp);
				collectp->pdburstp = NULL;
			}
#ifndef TOF_COLLECT_REMOTE
			collectp->collect_cnt = 0;
			bzero(collectp->collect, (collectp->collect_size) *
				sizeof(wl_proxd_collect_data_t));
#endif /* !TOF_COLLECT_REMOTE */
			break;

		default:
			return BCME_UNSUPPORTED;
	}

	return BCME_OK;
}
#endif /* TOF_COLLECT || TOF_COLLECT_REMOTE */

#ifdef TOF_COLLECT_REMOTE
static int pdburst_sm_initiator_collect_wait(pdburst_sm_t *sm, int event, pdburst_data_t *datap,
	pdburst_tsinfo_t *protp)
{
	pdburst_t* burstp = sm->tof_obj;
	wlc_info_t *wlc = burstp->wlc;
	int ret = TOF_RET_ALIVE;

	switch (event) {
		case TOF_EVENT_RXACT:
			/* Initiator Receive FTMs */
			if (datap->tof_type == TOF_TYPE_COLLECT_DATA ||
				datap->tof_type == TOF_TYPE_COLLECT_DATA_END) {
				bool endRx = FALSE;

				if (burstp->timeractive) {
					wl_del_timer(wlc->wl, burstp->timer);
					if (datap->tof_type == TOF_TYPE_COLLECT_DATA)
						wl_add_timer(wlc->wl, burstp->timer,
							FTM_INTVL2MSEC(&burstp->configp->timeout),
							FALSE);
					else
						burstp->timeractive = FALSE;
				}
				++sm->tof_rxcnt;
				endRx = pdburst_initiator_get_collect_data(burstp->collectp,
					(pdburst_collect_frm_t *)protp);
				if (datap->tof_type == TOF_TYPE_COLLECT_DATA) {
					if (endRx) {
						pdburst_collect_send(sm, &datap->tof_srcea,
							TOF_TYPE_COLLECT_REQ_END);
						pdburst_confirmed(sm, WL_PROXD_E_OK);
						ret = TOF_RET_SLEEP;
					}
				} else {
					pdburst_confirmed(sm, WL_PROXD_E_OK);
					ret = TOF_RET_SLEEP;
				}
			} else {
				FTM_ERR(("Initiator(%d) got unexpected type %d\n",
					sm->tof_state, datap->tof_type));
				ret = TOF_RET_IGNORE;
			}
			break;

		case TOF_EVENT_NOACK:
			/* REQ is NOT acked */
			++sm->tof_retrycnt;
			if (sm->tof_retrycnt > burstp->configp->ftm_req_retries) {
				pdburst_confirmed(sm, WL_PROXD_E_NOACK);
				ret = TOF_RET_SLEEP;
			} else {
				pdburst_collect_send(sm, &sm->tof_peerea,
					TOF_TYPE_COLLECT_REQ_START);
			}
			break;

		case TOF_EVENT_ACKED:
			sm->tof_retrycnt = 0;
			if (FTM_INTVL2MSEC(&burstp->configp->timeout)) {
				wl_add_timer(wlc->wl, burstp->timer,
					FTM_INTVL2MSEC(&burstp->configp->timeout), FALSE);
				burstp->timeractive = TRUE;
			}
			break;

		case TOF_EVENT_WAITMTMO:
			pdburst_confirmed(sm, WL_PROXD_E_TIMEOUT);
			ret = TOF_RET_SLEEP;
			break;

		default:
			ret = TOF_RET_IGNORE;
			break;
	}
	return ret;
}

static int pdburst_sm_target_collect_wait(pdburst_sm_t *sm, int event)
{
	pdburst_t* burstp = sm->tof_obj;
	int ret = TOF_RET_ALIVE;

	if (event == TOF_EVENT_ACKED || event == TOF_EVENT_NOACK) {
		if (event == TOF_EVENT_ACKED) {
			sm->tof_retrycnt = 0;
			sm->tof_txcnt++;
			sm->tof_followup = sm->tof_dialog;
			burstp->collectp->remote_cnt++;
		} else {
			sm->tof_retrycnt++;
		}

		if (sm->tof_retrycnt > burstp->configp->ftm_retries ||
			burstp->collectp->remote_cnt > burstp->collectp->collect_cnt) {
			/* Remote collect is done, free buffer */
			burstp->collectp = NULL;
			pdburst_destroy(&burstp);
			ret = TOF_RET_SLEEP;
		} else {
			pdburst_collect_send(sm, &sm->tof_peerea,
				(burstp->collectp->collect && burstp->collectp->remote_cnt <
					burstp->collectp->collect_cnt)?
					TOF_TYPE_COLLECT_DATA : TOF_TYPE_COLLECT_DATA_END);
			ret = TOF_RET_ALIVE;
		}
	}
	return ret;
}
#endif /* TOF_COLLECT_REMOTE */

static bool pdburst_initiator_measure_frame(pdburst_t* burstp, uint8 type)
{
	if (type == TOF_TYPE_MEASURE)
		return TRUE;
	if (burstp->lastburst)
		return FALSE;

	if (burstp->flags & WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP)
		return TRUE;
	return FALSE;
}

static void pdburst_initiator_save_last_frame(pdburst_t* burstp, uint8 type, uint64 t2,
	uint64 t3, int32 gd, int32 adj, int8 rssi, ratespec_t rspec, bool discard, uint8 txid)
{
	if (!(burstp->flags & WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP))
		return;

	if (burstp->lasttsp && !burstp->lastburst) {
		/* Save last frame measurement t2/t3 for next burst */
		burstp->tx_t1 = t2;
		burstp->lasttsp->t1 = 0;
		burstp->lasttsp->t4 = 0;
		burstp->lasttsp->t3 = (uint32)t3;
		burstp->lasttsp->t2 = (uint32)t2;
		burstp->lasttsp->gd = gd;
		burstp->lasttsp->adj = adj;
		burstp->lasttsp->rssi = rssi;
		burstp->lasttsp->rspec = rspec;
		burstp->lasttsp->discard = discard;
		burstp->lasttsp->tx_id = txid;
#ifdef TOF_DEBUG_TIME
		TOF_PRINTF(("Save txid %d t2 %d\n", txid, burstp->lasttsp->t2));
#endif // endif
	}
}

static void pdburst_initiator_restore_last_frame(pdburst_t* burstp, ftmts_t *tsp)
{
	if (burstp->flags & WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP) {
		if (burstp->lasttsp && tsp) {
			memcpy(tsp, burstp->lasttsp, sizeof(ftmts_t));
#ifdef TOF_DEBUG_TIME
			TOF_PRINTF(("Restore timestamp txid %d\n", tsp->tx_id));
#endif /* TOF_DEBUG_TIME */
		}
	}
}

/* initiator gets AVB time stamp */
static int pdburst_initiator_get_ts(pdburst_sm_t *sm, const pdburst_tsinfo_t *protp,
	int rssi, uint32 rspec, uint8 type)
{
	pdburst_t* burstp = sm->tof_obj;
	tof_tslist_t *listp = &burstp->tof_tslist;
	ftmts_t * list = listp->tslist;
	uint64 t3, t2;
	int32 gd, adj;
	bool discard = FALSE;
	uint8 measurecnt;
	wlc_phy_tof_info_t tof_info;
	wlc_phy_tof_info_type_t temp_mask;

	bzero(&tof_info, sizeof(tof_info));

	burstp->ftm_rx_rspec = rspec;

	measurecnt = pdburst_rx_measurecnt(burstp, protp);
#ifdef TOF_DEBUG_TIME
	TOF_PRINTF(("%s burstp->seq_started %d seq_en %d num_meas %d measurecnt %d  "
	"type %d ts_id %d tx_id %d seq_state %d\n",
	__FUNCTION__, burstp->seq_started, burstp->seq_en,
	 burstp->num_meas, measurecnt, type, protp->ts_id, protp->tx_id, burstp->seq_state));
#endif // endif
	if (pdburst_initiator_measure_frame(burstp, type)) {
		if (pdburst_measure_results(burstp, &t3, &t2, &gd, &adj,
			(uint16)protp->ts_id, TRUE, &tof_info, &discard, TRUE)) {
			t2 = 0;
			t3 = 0;
			gd = 0;
			adj = 0;
			discard = TRUE;
		}
	}

	if (BURST_SEQ_EN(burstp) && (burstp->seq_state == TOF_SEQ_DONE)) {
		burstp->seq_started = FALSE;
		WL_ERROR(("seq_started is set to %d\n", burstp->seq_started));
		proxd_undeaf_phy(burstp->wlc, TRUE);
		pdburst_measure(burstp, TOF_RX);
	}

	temp_mask =  (WLC_PHY_TOF_INFO_TYPE_SNR | WLC_PHY_TOF_INFO_TYPE_BITFLIPS);
	wlc_phy_tof_info(WLC_PI(burstp->wlc), &tof_info, temp_mask, burstp->core);

	if (BURST_SEQ_EN(burstp) && !(burstp->num_meas % TOF_DEFAULT_FTMCNT_SEQ)) {
		burstp->seq_started = FALSE;
		TOF_PRINTF(("seq_started is set to %d\n", burstp->seq_started));
		proxd_undeaf_phy(burstp->wlc, TRUE);
		pdburst_measure(burstp, TOF_RX);
	}
	if (measurecnt <= burstp->measurecnt && list) {
		/* Get t2, t3 */
		if (measurecnt < burstp->measurecnt && type == TOF_TYPE_MEASURE) {
#ifdef TOF_COLLECT
			pdburst_collect_data(burstp->collectp, protp->ts_id,
					CHSPEC_IS20(burstp->configp->chanspec));
#endif /* TOF_COLLECT */
			list[measurecnt].t3 = (uint32)t3;
			list[measurecnt].t2 = (uint32)t2;
			list[measurecnt].gd = gd;
			list[measurecnt].adj = adj;
			list[measurecnt].rssi = rssi;
			list[measurecnt].rspec = rspec;
			list[measurecnt].discard = discard;
			list[measurecnt].tx_id = protp->tx_id;
			if (tof_info.info_mask & WLC_PHY_TOF_INFO_TYPE_RSSI) {
				list[measurecnt].rssi = tof_info.rssi;
			}
			if (tof_info.info_mask & WLC_PHY_TOF_INFO_TYPE_SNR) {
				list[measurecnt].snr = tof_info.snr;
			}
			if (tof_info.info_mask & WLC_PHY_TOF_INFO_TYPE_BITFLIPS) {
				list[measurecnt].bitflips = tof_info.bitflips;
			}

			if (tof_info.info_mask & WLC_PHY_TOF_INFO_TYPE_PHYERROR) {
				list[measurecnt].tof_phy_error = tof_info.tof_phy_error;
			}
		} else {
			/* last measurement */
			pdburst_initiator_save_last_frame(burstp, type, t2, t3, gd, adj, rssi,
				rspec, discard, protp->tx_id);
		}
		if (measurecnt) {
			/* convert to nano sec - resolution here. 0.5 ns error */
			list[measurecnt-1].t1 = (uint32)pdftm_div64(protp->tod, BURST_HZ_PICO);
			list[measurecnt-1].t4 = (uint32)pdftm_div64((protp->toa +
				BURST_ERR_PICO), BURST_HZ_PICO);
			if (measurecnt > listp->tscnt)
				listp->tscnt = measurecnt;
		}
	}
#ifdef TOF_COLLECT_INLINE
	/* At initiator, log previous measurement as current T1 and T4 are not available yet. */
	if (measurecnt <= burstp->measurecnt && list && measurecnt) {
		pdburst_collect_inline_frame_info_data_events(burstp,
			&list[measurecnt-1], measurecnt-1);
	}
#endif /* TOF_COLLECT_INLINE */

	return BCME_OK;
}

static bool pdburst_target_timestamp_last_frame(pdburst_t * burstp, uint8 totalfcnt)
{
	bool ret = TRUE;
	if (burstp->sm->tof_txcnt >= burstp->measurecnt ||
		(totalfcnt && burstp->sm->tof_txpktcnt >= totalfcnt)) {
		/* last measurement frame */
		if (!(burstp->flags & WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP) || burstp->lastburst) {
			burstp->sm->tof_retrycnt = 0;
			burstp->sm->tof_txcnt++;
			burstp->sm->tof_followup = burstp->sm->tof_dialog;
			ret = FALSE;
		}
	}
	return ret;
}

/* target gets time stamp */
static int pdburst_target_get_ts(pdburst_sm_t *sm, bool acked)
{
	pdburst_t* burstp = sm->tof_obj;
	uint64 t1, t4;
	int32 gd, adj;
	bool discard = FALSE;
	uint16 id;
	wlc_phy_tof_info_t tof_info;
	wlc_phy_tof_info_type_t tof_info_mask;

	bzero(&tof_info, sizeof(tof_info));

	if (sm->tof_legacypeer == TOF_LEGACY_AP) {
		tof_tslist_t *listp = &burstp->tof_tslist;
		id = listp->tscnt;
	} else {
		id = sm->tof_txcnt;
	}

	if (pdburst_measure_results(burstp, &t1, &t4, &gd, &adj, id, acked,
		&tof_info, &discard, FALSE)) {
		t1 = 0;
		t4 = 0;
		gd = 0;
		adj = 0;
		discard = TRUE;
	}

	tof_info_mask =  (WLC_PHY_TOF_INFO_TYPE_SNR | WLC_PHY_TOF_INFO_TYPE_BITFLIPS
					| WLC_PHY_TOF_INFO_TYPE_RSSI);
	wlc_phy_tof_info(WLC_PI(burstp->wlc), &tof_info, tof_info_mask, burstp->core);

#ifdef TOF_COLLECT
	if (sm->tof_legacypeer != TOF_LEGACY_AP) {
		pdburst_collect_data(burstp->collectp, sm->tof_followup,
				CHSPEC_IS20(burstp->configp->chanspec));
	}
#endif /* TOF_COLLECT */

	if (!acked || discard)
		return BCME_ERROR;

	if (sm->tof_legacypeer == TOF_LEGACY_AP) {
		tof_tslist_t *listp = &burstp->tof_tslist;
		ftmts_t * list = listp->tslist;

#ifdef TOF_COLLECT
		pdburst_collect_data(burstp->collectp, burstp->collectp->collect_cnt,
				CHSPEC_IS20(burstp->configp->chanspec));
#endif /* TOF_COLLECT */
		if (list && listp->tscnt < burstp->measurecnt) {
			list[listp->tscnt].t1 = (uint32)t1;
			list[listp->tscnt].t4 = (uint32)t4;
			list[listp->tscnt].gd = gd;
			list[listp->tscnt].adj = adj;
			if (tof_info.info_mask & WLC_PHY_TOF_INFO_TYPE_RSSI)
				list[listp->tscnt].rssi = tof_info.rssi;
			if (tof_info.info_mask & WLC_PHY_TOF_INFO_TYPE_SNR)
				list[listp->tscnt].snr = tof_info.snr;
			if (tof_info.info_mask & WLC_PHY_TOF_INFO_TYPE_BITFLIPS)
				list[listp->tscnt].bitflips = tof_info.bitflips;
			list[listp->tscnt].rspec = burstp->configp->ratespec;
			list[listp->tscnt].discard = discard;
			list[listp->tscnt].tx_id = sm->tof_dialog;
			listp->tscnt++;
		}
		burstp->tx_t1 = t1;
		burstp->tx_t4 = t4;
	} else {
		/* convert into PICO second */
		burstp->tx_t1 = t1 * BURST_HZ_PICO;
		burstp->tx_t4 = t4 * BURST_HZ_PICO;
	}
#ifdef TOF_COLLECT_INLINE
	{
		ftmts_t list_lcl;
		bzero(&list_lcl, sizeof(list_lcl));
		list_lcl.t2 = 0;
		list_lcl.t3 = 0;
		list_lcl.t1 = (uint32)t1;
		list_lcl.t4 = (uint32)t4;
		list_lcl.gd = gd;
		list_lcl.adj = adj;
		list_lcl.rssi = tof_info.rssi;
		pdburst_collect_inline_frame_info_data_events(burstp, &list_lcl, id);
	}
#endif /* TOF_COLLECT_INLINE */
	return BCME_OK;
}

/* TOF timeout function */
static void pdburst_timer(void *arg)
{
	pdburst_t* burstp = (pdburst_t *)arg;

	burstp->timeractive = FALSE;
	if (burstp->smstoped)
		return;

	if (burstp->sm->tof_mode == WL_PROXD_MODE_INITIATOR)
		pdburst_sm(burstp->sm, TOF_EVENT_WAITMTMO, NULL, 0, NULL);
}

/* delay certain time before txing the next measurement packet */
static void pdburst_ftm_tx_timer(void *ctx)
{
	pdburst_t *burstp = (pdburst_t *)ctx;
	pdburst_sm_t *sm = burstp->sm;
	uint8 type = TOF_TYPE_MEASURE;
	int totalfcnt;

	ASSERT(burstp != NULL);
	if (sm->tof_mode == WL_PROXD_MODE_INITIATOR)
	{
		burstp->ftm_tx_timer_active = FALSE;
		pdburst_send(sm, &sm->tof_peerea, TOF_TYPE_REQ_START);
		pdburst_measure(burstp, TOF_RX);
		return;
	}
	if (!burstp->caldone && !burstp->smstoped) {
		/* Wait for calculation done */
		wlc_hrt_add_timeout(burstp->ftm_tx_timer, 100, pdburst_ftm_tx_timer, ctx);
		return;
	}
	wlc_hrt_del_timeout(burstp->ftm_tx_timer);
	burstp->ftm_tx_timer_active = FALSE;
	sm->tof_txpktcnt++;
	if (!(++sm->tof_dialog))
		sm->tof_dialog = 1;
	totalfcnt = pdburst_total_framecnt(burstp);
	if (sm->tof_txcnt >= burstp->measurecnt ||
		(totalfcnt && sm->tof_txpktcnt >= totalfcnt)) {
		type = TOF_TYPE_MEASURE_END;

		/* Last burst measurement dialog = 0 */
		if (burstp->lastburst) {
			sm->tof_dialog = 0;
		}
	}

	if (pdburst_send(sm, &burstp->sm->tof_peerea, type) != BCME_OK) {
		FTM_ERR(("%s: ERROR in pdburst_send\n", __FUNCTION__));
	}
}

/* Use one way RTT */
static void pdburst_start_oneway(pdburst_t *burstp, pdburst_sm_t *sm)
{
	sm->tof_state = TOF_STATE_ILEGACY;
	sm->tof_legacypeer = TOF_LEGACY_AP;
	sm->tof_txcnt = 0;
	sm->tof_txpktcnt = 1;
	++sm->tof_dialog;
	if (!sm->tof_dialog)
		sm->tof_dialog = 1;
	pdburst_send(sm, &sm->tof_peerea, TOF_TYPE_MEASURE);
}

static void pdburst_decide_oneway(pdburst_t *burstp, pdburst_sm_t *sm)
{
	/* Disable one-way RTT when it is set as auto-mode */
	sm->tof_legacypeer = TOF_NONLEGACY_AP;
}
static int pdburst_sm_legacy(pdburst_sm_t *sm, int event)
{
	pdburst_t* burstp = sm->tof_obj;
	int ret = TOF_RET_SLEEP;
	uint8 totalfcnt;

	if (event == TOF_EVENT_ACKED || event == TOF_EVENT_NOACK) {
		if (event == TOF_EVENT_ACKED) {
			if (pdburst_target_get_ts(sm, TRUE) == BCME_OK) {
				sm->tof_retrycnt = 0;
				sm->tof_txcnt++;
				sm->tof_followup = sm->tof_dialog;
			} else
				sm->tof_retrycnt++;
		} else {
			sm->tof_retrycnt++;
			pdburst_target_get_ts(sm, FALSE);
		}

		totalfcnt = pdburst_total_framecnt(burstp);
		if (sm->tof_retrycnt > burstp->configp->ftm_retries) {
			pdburst_confirmed(sm, WL_PROXD_E_NOACK);
		} else if (sm->tof_txcnt >= burstp->measurecnt) {
			pdburst_confirmed(sm, WL_PROXD_E_OK);
		} else if (sm->tof_txpktcnt >= totalfcnt && totalfcnt) {
			pdburst_confirmed(sm, WL_PROXD_E_CANCELED);
		} else {
			sm->tof_txpktcnt++;
			sm->tof_dialog++;
			if (!sm->tof_dialog)
				sm->tof_dialog = 1;
			pdburst_send(sm, &sm->tof_peerea, TOF_TYPE_MEASURE);
			ret = TOF_RET_ALIVE;
		}
	}
	return ret;
}

static int pdburst_sm_target_wait(pdburst_sm_t *sm, int event, pdburst_data_t *datap)
{
	pdburst_t* burstp = sm->tof_obj;
	int ret = TOF_RET_ALIVE;
	uint8 totalfcnt;

	switch (event) {
		case TOF_EVENT_ACKED:
		case TOF_EVENT_NOACK:
			if (sm->tof_txcnt <= burstp->measurecnt) {
				if (burstp->ftm_tx_timer_active) {
					wlc_hrt_del_timeout(burstp->ftm_tx_timer);
					burstp->ftm_tx_timer_active = FALSE;
				}
				wlc_hrt_add_timeout(burstp->ftm_tx_timer,
						FTM_INTVL2USEC(&burstp->configp->ftm_sep),
						pdburst_ftm_tx_timer, (void *)burstp);
				burstp->ftm_tx_timer_active = TRUE;
				burstp->caldone = FALSE;
			}
			totalfcnt = pdburst_total_framecnt(burstp);
			if (event == TOF_EVENT_ACKED) {
#if defined(TOF_PROFILE)
				wlc_read_tsf(burstp->wlc, &tsf_lastm, &tsf_hi);
				tsf_lastm -= tsf_start;
				TOF_PRINTF(("TIME_BEFORE_GET_TS = 0x%0x\n", tsf_lastm));
#endif // endif
				if (pdburst_target_timestamp_last_frame(burstp, totalfcnt) &&
					pdburst_target_get_ts(sm, TRUE) == BCME_OK) {
					sm->tof_retrycnt = 0;
					sm->tof_txcnt++;
					sm->tof_followup = sm->tof_dialog;
				} else {
					sm->tof_retrycnt++;
				}
#if defined(TOF_PROFILE)
				wlc_read_tsf(burstp->wlc, &tsf_lastm, &tsf_hi);
				tsf_lastm -= tsf_start;
				TOF_PRINTF(("TIME_AFTER_GET_TS = 0x%0x\n", tsf_lastm));
#endif // endif
			} else {
				sm->tof_retrycnt++;
				OSL_DELAY(1);
				pdburst_target_get_ts(sm, FALSE);
			}
			burstp->caldone = TRUE;
			if (sm->tof_retrycnt > burstp->configp->ftm_retries) {
				FTM_ERR(("Too many retries, ftm:%d, stopped\n", sm->tof_txcnt));
				sm->tof_retrycnt = 0;
				pdburst_reset(sm, sm->tof_mode, WL_PROXD_E_NOACK);
				ret = pdburst_target_done(sm, WL_PROXD_E_NOACK);
			} else if (sm->tof_txcnt > burstp->measurecnt) {
				if (event == TOF_EVENT_ACKED) {
					/*  TARGET ftms completed OK */
					sm->tof_retrycnt = 0;
					pdburst_reset(sm, sm->tof_mode, WL_PROXD_E_OK);
					ret = pdburst_target_done(sm, WL_PROXD_E_OK);
				}
			} else if (sm->tof_txpktcnt >= totalfcnt && totalfcnt) {
				sm->tof_retrycnt = 0;
				pdburst_reset(sm, sm->tof_mode, WL_PROXD_E_CANCELED);
				ret = pdburst_target_done(sm, WL_PROXD_E_CANCELED);
			}
			break;

		case TOF_EVENT_RXACT:
			/* target received FTMR */
			if (datap->tof_type == TOF_TYPE_REQ_START) {
				/* Rxed start because client resets */
				sm->tof_txcnt = 0;
				sm->tof_retrycnt = 0;
				pdburst_send(sm, &sm->tof_peerea, TOF_TYPE_MEASURE);
			} else {
				FTM_ERR(("Unknown TOF pkt type:%d\n", datap->tof_type));
				ret = TOF_RET_IGNORE;
			}
			break;

		default:
			ret = TOF_RET_IGNORE;
			break;
	}
	return ret;
}

static int pdburst_sm_initiator_wait(pdburst_sm_t *sm, int event, pdburst_data_t *datap,
	const pdburst_tsinfo_t *protp)
{
	pdburst_t* burstp = sm->tof_obj;
	wlc_info_t *wlc = burstp->wlc;
	int ret = TOF_RET_ALIVE;

	switch (event) {
		case TOF_EVENT_RXACT:
			/* Initiator Rxed Packet */
			if (datap->tof_type == TOF_TYPE_MEASURE ||
				datap->tof_type == TOF_TYPE_MEASURE_END) {
				/* Rxed measure packet */
				if (sm->tof_legacypeer == TOF_LEGACY_UNKNOWN) {
					/* First Measurement Packet */
#ifdef TOF_DEBUG_TIME2
					wlc_read_tsf(burstp->wlc, &tsf_rxm, &tsf_hi);
					tsf_rxm -= tsf_start;
#endif // endif
					sm->tof_legacypeer = TOF_NONLEGACY_AP;
				}
#ifdef TOF_DEBUG_TIME2
				wlc_read_tsf(burstp->wlc, &tsf_lastm, &tsf_hi);
				tsf_lastm -= tsf_start;
#endif // endif
				if (burstp->timeractive) {
					wl_del_timer(wlc->wl, burstp->timer);
					if (datap->tof_type == TOF_TYPE_MEASURE)
						wl_add_timer(wlc->wl, burstp->timer,
							FTM_INTVL2MSEC(&burstp->configp->timeout),
							FALSE);
					else
						burstp->timeractive = FALSE;
				}
				++sm->tof_rxcnt;
				pdburst_initiator_get_ts(sm, protp, datap->tof_rssi,
					datap->tof_rspec, datap->tof_type);
				if (datap->tof_type == TOF_TYPE_MEASURE) {
					if (pdburst_rx_measurecnt(burstp, protp) >=
						burstp->measurecnt) {
						ret = pdburst_confirmed(sm, WL_PROXD_E_OK);
					}
				} else {
					ret = pdburst_confirmed(sm, WL_PROXD_E_OK);
				}
			} else {
				FTM_ERR(("Initiator(%d) got unexpected type %d\n",
					sm->tof_state, datap->tof_type));
				ret = TOF_RET_IGNORE;
			}
			break;

		case TOF_EVENT_NOACK:
			/* REQ is NOT acked */
			++sm->tof_retrycnt;
			if (sm->tof_retrycnt > burstp->configp->ftm_req_retries) {
				pdburst_confirmed(sm, WL_PROXD_E_NOACK);
				ret = TOF_RET_SLEEP;
			} else {
				if (burstp->flags & WL_PROXD_SESSION_FLAG_SEQ_EN) {
					wlc_hrt_add_timeout(burstp->ftm_tx_timer,
						TOF_REQ_START_RETRY_DUR, pdburst_ftm_tx_timer,
						(void *)burstp);
					burstp->ftm_tx_timer_active = TRUE;
					ret = TOF_RET_SLEEP;
				} else {
					pdburst_send(sm, &sm->tof_peerea, TOF_TYPE_REQ_START);
					pdburst_measure(burstp, TOF_RX);
				}
			}
			break;

		case TOF_EVENT_ACKED:
			/* initiator rxed FTMR ack */
			burstp->result_flags |= WL_PROXD_REQUEST_ACKED;
			sm->tof_retrycnt = 0;
			if (burstp->flags & WL_PROXD_SESSION_FLAG_ONE_WAY) {
				pdburst_start_oneway(burstp, sm);
			} else if (FTM_INTVL2MSEC(&burstp->configp->timeout)) {
				wl_add_timer(wlc->wl, burstp->timer,
					FTM_INTVL2MSEC(&burstp->configp->timeout), FALSE);
				burstp->timeractive = TRUE;
				pdburst_decide_oneway(burstp, sm);
			}
			if (burstp->ftm_tx_timer_active) {
				wlc_hrt_del_timeout(burstp->ftm_tx_timer);
				burstp->ftm_tx_timer_active = FALSE;
			}
#ifdef TOF_DEBUG_TIME2
			wlc_read_tsf(burstp->wlc, &tsf_rxack, &tsf_hi);
			tsf_rxack -= tsf_start;
#endif // endif
			break;

		case TOF_EVENT_WAITMTMO:
			/* Wait for measurement pkt timeout */
			if (sm->tof_legacypeer == TOF_LEGACY_UNKNOWN) {
				pdburst_start_oneway(burstp, sm);
			} else if (sm->tof_legacypeer == TOF_NONLEGACY_AP) {
				/* AP stoped txing measurement */
				ret = pdburst_confirmed(sm, WL_PROXD_E_TIMEOUT);
			}
			break;

		default:
			ret = TOF_RET_IGNORE;
			break;
	}
	return ret;
}

static int pdburst_sm_idle(pdburst_sm_t *sm, int event, pdburst_data_t *datap)
{
	pdburst_t* burstp = sm->tof_obj;
	int ret = TOF_RET_ALIVE;

	switch (event) {
		case TOF_EVENT_WAKEUP:
			if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
#ifdef TOF_DEBUG_TIME2
				wlc_read_tsf(burstp->wlc, &tsf_txreq, &tsf_hi);
				tsf_txreq -= tsf_start;
#endif // endif
				if (burstp->flags & WL_PROXD_SESSION_FLAG_SEQ_EN) {
					burstp->seq_en = TRUE;
				} else {
					burstp->seq_en = FALSE;
				}
				burstp->measurecnt = pdburst_get_ftm_cnt(burstp);
				pdburst_hw(burstp, TRUE, FALSE);
				if (!BURST_SEQ_EN(burstp)) {
					/* Only needed on side which sends acks */
					/* Response is ack wrapped in ctl frame 16 bytes */
					pdburst_tof_init_vht(burstp, 16);
					pdburst_tof_setup_vht(burstp);
					if (RSPEC_ISHT(burstp->configp->ratespec)) {
						pdburst_tof_setup_ht(burstp);
					}
				} else {
					phy_tof_setup_ack_core(WLC_PI(burstp->wlc), burstp->core);
				}
				pdburst_send(sm, &sm->tof_peerea, TOF_TYPE_REQ_START);
				pdburst_measure(burstp, TOF_RX);
				sm->tof_state = TOF_STATE_IWAITM;
			} else if (sm->tof_mode != WL_PROXD_MODE_TARGET) {
				FTM_ERR(("Invalid mode %d\n", sm->tof_mode));
				ret = TOF_RET_SLEEP;
			}
			break;
#ifdef TOF_COLLECT_REMOTE
		case TOF_EVENT_COLLECT_REQ:
			if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
				pdburst_event(sm, WLC_E_PROXD_COLLECT_START);
				pdburst_collect_send(sm, &sm->tof_peerea,
					TOF_TYPE_COLLECT_REQ_START);
				sm->tof_state = TOF_STATE_IWAITCL;
				sm->tof_retrycnt = 0;
				ret = TOF_RET_ALIVE;
			} else {
				ret = TOF_RET_SLEEP;
				FTM_ERR(("Invalid mode %d\n", sm->tof_mode));
			}
			break;
#endif /* TOF_COLLECT_REMOTE */
		case TOF_EVENT_RXACT:
			if (datap->tof_type == TOF_TYPE_REQ_START) {
				/* Rxed measure request packet */
				if (sm->tof_mode == WL_PROXD_MODE_TARGET) {
					pdburst_hw(burstp, TRUE, TRUE);
					bcopy(&datap->tof_srcea, &sm->tof_peerea, ETHER_ADDR_LEN);
					sm->tof_state = TOF_STATE_TWAITM;
					sm->tof_txpktcnt = 1;
					sm->tof_dialog++;
					if (!sm->tof_dialog)
						sm->tof_dialog = 1;
					pdburst_send(sm, &sm->tof_peerea, TOF_TYPE_MEASURE);
				} else {
					ret = TOF_RET_IGNORE;
				}
			}
#ifdef TOF_COLLECT_REMOTE
			else if (datap->tof_type == TOF_TYPE_COLLECT_REQ_START) {
				/* Rxed collect request packet */
				if (sm->tof_mode == WL_PROXD_MODE_TARGET && burstp->collectp) {
					bcopy(&datap->tof_srcea, &sm->tof_peerea, ETHER_ADDR_LEN);
					bcopy(&datap->tof_dstea, &sm->tof_selfea, ETHER_ADDR_LEN);
					sm->tof_state = TOF_STATE_TWAITCL;
					sm->tof_retrycnt = 0;
					burstp->collectp->remote_cnt = 0;
					pdburst_collect_send(sm, &sm->tof_peerea,
						TOF_TYPE_COLLECT_DATA);
					pdburst_event(sm, WLC_E_PROXD_COLLECT_START);
				} else {
					ret = TOF_RET_IGNORE;
				}
			}
#endif /* TOF_COLLECT_REMOTE */
			else {
				ret = TOF_RET_IGNORE;
			}
			break;
		default:
			ret = TOF_RET_IGNORE;
			break;
	}
	return ret;
}

/* TOF state machine */
static int pdburst_sm(pdburst_sm_t *sm, int event, const pdburst_tsinfo_t *protp,
	int paramlen, pdburst_data_t *datap)
{
	pdburst_t* burstp = sm->tof_obj;
	int ret = TOF_RET_IGNORE;

	ASSERT(event < TOF_EVENT_LAST);
	ASSERT(sm != NULL);
#if defined(TOF_PROFILE)
	wlc_read_tsf(burstp->wlc, &tsf_lastm, &tsf_hi);
	tsf_lastm -= tsf_start;

	if (protp) {
#ifdef TOF_PROFILE
		TOF_PRINTF(("EVENT = %d, TOKEN=%d FOLLOW_TOKEN=%d TIME = 0x%0x\n",
			event, protp->tx_id, protp->ts_id, tsf_lastm));
#endif // endif

	} else {
#ifdef TOF_PROFILE
		TOF_PRINTF(("EVENT = %d, TIME = 0x%0x\n", event, tsf_lastm));
#endif // endif
	}
#endif /* TOF_PROFILE */

	if (sm->tof_mode == WL_PROXD_MODE_DISABLE)
		return TOF_RET_SLEEP;

	if (event == TOF_EVENT_TMO && sm->tof_state != TOF_STATE_ICONFIRM) {
		pdburst_confirmed(sm, WL_PROXD_E_TIMEOUT);
		return TOF_RET_SLEEP;
	}

	if (event == TOF_EVENT_RXACT) {
		ASSERT(datap != NULL);

		if (sm->tof_mode == WL_PROXD_MODE_TARGET) {
			if (bcmp(&ether_bcast, &burstp->allow_mac, ETHER_ADDR_LEN) &&
				bcmp(&datap->tof_srcea, &burstp->allow_mac, ETHER_ADDR_LEN)) {
				return TOF_RET_IGNORE;
			}
		}
	}

	switch (sm->tof_state) {
		case TOF_STATE_IDLE:
			ret = pdburst_sm_idle(sm, event, datap);
			break;

		case TOF_STATE_IWAITM:
			ret = pdburst_sm_initiator_wait(sm, event, datap, protp);
			break;

		case TOF_STATE_ILEGACY:
			ret = pdburst_sm_legacy(sm, event);
			break;

		case TOF_STATE_TWAITM:
			ret = pdburst_sm_target_wait(sm, event, datap);
			break;

#ifdef TOF_COLLECT_REMOTE
		case TOF_STATE_IWAITCL:
			ret = pdburst_sm_initiator_collect_wait(sm, event, datap, protp);
			break;

		case TOF_STATE_TWAITCL:
			ret = pdburst_sm_target_collect_wait(sm, event);
			break;
#endif /* TOF_COLLECT_REMOTE */
		case TOF_STATE_IREPORT:
			if (event == TOF_EVENT_ACKED) {
				pdburst_report_done(sm, WL_PROXD_E_OK);
			} else {
				pdburst_report_done(sm, WL_PROXD_E_NOACK);
			}
			ret = TOF_RET_SLEEP;
			break;

		case TOF_STATE_ICONFIRM:
			if (event == TOF_EVENT_ACKED) {
				ret = TOF_RET_SLEEP;
			} else if (event == TOF_EVENT_NOACK) {
				ret = TOF_RET_SLEEP;
			}
			break;

		default:
			ASSERT(0);
			break;
	}

	return ret;
}

static int pdburst_init_tslist(pdburst_t * burstp, tof_tslist_t *listp, int list_cnt)
{
	if (!listp->tslist || (burstp->measurecnt != list_cnt)) {
		/* The measure counts changed */
		if (listp->tslist)
			MFREE(burstp->wlc->osh, listp->tslist, burstp->measurecnt *
				sizeof(ftmts_t));
		listp->tslist = MALLOCZ(burstp->wlc->osh, list_cnt * sizeof(ftmts_t));
		if (listp->tslist)
			burstp->measurecnt = list_cnt;
		else {
			burstp->measurecnt = 0;
			return BCME_NOMEM;
		}
		if (!burstp->lasttsp) {
			burstp->lasttsp = MALLOCZ(burstp->wlc->osh, sizeof(ftmts_t));
			if (!burstp->lasttsp)
			{
				MFREE(burstp->wlc->osh, listp->tslist, list_cnt * sizeof(ftmts_t));
				return BCME_NOMEM;
			}
		} else {
			pdburst_initiator_restore_last_frame(burstp, listp->tslist);
		}
	}
	listp->tscnt = 0;
	return BCME_OK;
}

static int pdburst_target_init(pdburst_t * burstp, const pdburst_params_t *params)
{
	int err = BCME_OK;
	if (!burstp->mf_buf) {
		if (!(burstp->mf_buf = MALLOCZ(burstp->wlc->osh, MF_BUF_MAX_LEN))) {
			err = BCME_NOMEM;
			goto done;
		}
	}
	burstp->mf_buf_len = MF_BUF_MAX_LEN;

	pdburst_reset(burstp->sm, WL_PROXD_MODE_TARGET, WL_PROXD_E_NOTSTARTED);
	burstp->sm->tof_dialog = params->dialog;
	if (burstp->flags & WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP) {
		if (burstp->tx_t1) {
			burstp->sm->tof_followup = params->dialog;
#ifdef TOF_DEBUG_TIME
			TOF_PRINTF(("followup token %d\n", burstp->sm->tof_followup));
#endif // endif
		}
	} else {
		burstp->tx_t1 = 0;
		burstp->tx_t4 = 0;
		burstp->sm->tof_followup = 0;
	}

	if (params->req) {
		err = pdburst_rx_tof_params(burstp, PDBURST_FRAME_TYPE_REQ,
			params->req, params->req_len, params->req_rspec);
		if (err != BCME_OK)
			goto done;

		proxd_update_tunep_values(burstp->tunep, burstp->configp->chanspec,
			BURST_IS_VHTACK(burstp));
	}

done:
	return err;
}

static int pdburst_get_session_info(pdburst_t * burstp, const pdburst_params_t *params,
	pdburst_session_info_t *info)
{
	int err;

	err = PROTOCB(burstp, get_session_info, (burstp->ctx, info));
	if (err != BCME_OK) {
		goto done;
	}

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	burstp->configp = (pdburst_config_t *)params->config;
	GCC_DIAGNOSTIC_POP();

	burstp->bsscfg = params->bsscfg;
	burstp->flags = params->flags;
	burstp->lastburst = (info->flags & PDBURST_SESSION_FLAGS_LAST_BURST);
	if (info->flags & PDBURST_SESSION_FLAGS_MBURST_FOLLOWUP) {
		burstp->flags |= WL_PROXD_SESSION_FLAG_MBURST_FOLLOWUP;
	}
	/* note: seq, vht ack and secure flags are always in sync */
done:
	return err;
}

/* external interface */

pdburst_t*
pdburst_create(wlc_info_t *wlc, void *ctx, const pdburst_callbacks_t *callbacks)
{
	pdburst_t * burstp;

	ASSERT(wlc != NULL);
#ifdef TOF_DEBUG_TIME
	TOF_PRINTF(("pdburst_create\n"));
#endif // endif
	burstp = MALLOCZ(wlc->osh, sizeof(pdburst_t));
	if (burstp != NULL) {
		burstp->wlc = wlc;
		burstp->ctx = ctx;
		burstp->chipnum = CHIPID(wlc->pub->sih->chip);
		burstp->chiprev = wlc->pub->sih->chiprev;

		burstp->svc_funcs = callbacks;
		burstp->phyver = wlc->band->phyrev;
		burstp->tunep = proxd_get_tunep(wlc, &burstp->Tq);

		burstp->sm = MALLOCZ(wlc->osh, sizeof(pdburst_sm_t));
		if (burstp->sm) {
			burstp->sm->tof_obj = burstp;
		} else {
			FTM_ERR(("Create tofpd obj failed\n"));
			goto err;
		}

		if ((burstp->duration_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
			FTM_ERR(("wl%d: %s: wlc_hrt_alloc_timeout failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto err;
		}

		if ((burstp->ftm_tx_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
			FTM_ERR(("wl%d: %s: ftm_tx_timer hrt tmr alloc failed \n",
				wlc->pub->unit, __FUNCTION__));
			goto err;
		}
		burstp->ftm_tx_timer_active = FALSE;
		bcopy(&ether_bcast, &burstp->allow_mac, ETHER_ADDR_LEN);

		if (!(burstp->timer = wl_init_timer(wlc->wl, pdburst_timer,
			burstp, "pdburst"))) {
			FTM_ERR(("Create pdburst timer failed\n"));
			goto err;
		}

		/* Reset state machine */
		burstp->smstoped = TRUE;
		/* Get TOF shared memory address */
		burstp->shmemptr = wlc_read_shm(wlc, M_TOF_BLK_PTR(wlc)) << 1;
#ifdef TOF_COLLECT
		if (pdburst_collect) {
			ASSERT(burstp != pdburst_collect->pdburstp);
			if (pdburst_collect->pdburstp) {
				/* When we create new burst, free the previous one if it is not
				** collected yet. Then link collect data to the new burst.
				** collected yet. Then link collect data to the new burst.
				*/
				pdburst_collect->pdburstp->collectp = NULL;
				pdburst_destroy(&pdburst_collect->pdburstp);
			}
			burstp->collectp = pdburst_collect;
			pdburst_collect->pdburstp = burstp;
			burstp->tunep->minDT = -1;
			burstp->tunep->maxDT = -1;
		}
#endif  /* TOF_COLLECT */
		burstp->core = (burstp->tunep->core == 255) ? 0 : burstp->tunep->core;

	} else {
		FTM_ERR(("wl:%d %s MALLOC failed malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->pub->osh)));
	}

	return (burstp);
err:
	if (burstp) {
		if (burstp->duration_timer != NULL) {
			wlc_hrt_free_timeout(burstp->duration_timer);
			burstp->duration_timer = NULL;
		}

		if (burstp->ftm_tx_timer != NULL) {
			wlc_hrt_free_timeout(burstp->ftm_tx_timer);
			burstp->ftm_tx_timer = NULL;
		}

		if (burstp->sm) {
			MFREE(wlc->osh, burstp->sm, sizeof(pdburst_sm_t));
			burstp->sm = NULL;
		}

		if (burstp->timer) {
			wl_free_timer(wlc->wl, burstp->timer);
			burstp->timer = NULL;
		}

		MFREE(wlc->osh, burstp, sizeof(pdburst_t));
	}

	return NULL;
}

static int
pdburst_resolve_initator_mac_addr(pdburst_t *burstp)
{
	int err = BCME_OK;
	struct ether_addr *mac = &burstp->configp->cur_ether_addr;
	/* resolve selfea in case of initiator
	* Appliable only in case of primary bsscg
	* NAN or AWDL should config cur_ether_addr
	*/
	if (BSSCFG_SLOTTED_BSS(burstp->bsscfg)) {
		if (ETHER_ISNULLADDR(mac)) {
			ASSERT(0);
			err = BCME_BADADDR;
		}
		goto done;
	}

	ASSERT(burstp->sm->tof_mode == WL_PROXD_MODE_INITIATOR);
	if (ETHER_ISNULLADDR(mac)) {
		struct ether_addr *rand_mac;
		rand_mac = wlc_proxd_get_randmac(burstp->wlc->pdsvc_info, burstp->bsscfg);
		if (rand_mac) {
			memcpy(mac, rand_mac, ETHER_ADDR_LEN);
			burstp->randmac_in_use = TRUE;
		} else {
			/* in case WL_RANDMAC is undefined */
			memcpy(mac, &burstp->bsscfg->cur_etheraddr, ETHER_ADDR_LEN);
			err = BCME_OK;
		}
	}
done:
	return err;
}

int
pdburst_init(pdburst_t *burstp, const pdburst_params_t *params)
{
	pdburst_session_info_t info;
	int err = BCME_OK;

	if (!burstp) {
		err = BCME_BADARG;
		goto done;
	}

	/* note: do this first */
	err = pdburst_get_session_info(burstp, params, &info);
	if (err != BCME_OK)
		goto done;

	memcpy(&burstp->sm->tof_selfea, burstp->bsscfg ? &burstp->bsscfg->cur_etheraddr :
		&ether_null, ETHER_ADDR_LEN);

	if (burstp->flags & WL_PROXD_SESSION_FLAG_INITIATOR) {
		tof_tslist_t *listp = &burstp->tof_tslist;
		uint8 list_cnt = pdburst_get_ftm_cnt(burstp);

		burstp->sm->tof_mode = WL_PROXD_MODE_INITIATOR;
		proxd_update_tunep_values(burstp->tunep, burstp->configp->chanspec,
			BURST_IS_VHTACK(burstp));
		if (pdburst_init_tslist(burstp, listp, list_cnt)) {
			burstp->smstoped = TRUE;
			err = BCME_NOMEM;
			goto done;
		}
	} else if (burstp->flags & WL_PROXD_SESSION_FLAG_TARGET) {
		err = pdburst_target_init(burstp, params);
	} else {
			err = BCME_UNSUPPORTED;
			goto done;
	}

	if (BURST_IS_VHTACK(burstp)) {
		info.flags |= PDBURST_SESSION_FLAGS_VHTACK;
		err = PROTOCB(burstp, set_session_info, (burstp->ctx, &info));
	}

done:
	return err;
}

int
pdburst_start(pdburst_t *burstp)
{
	pdburst_sm_t *sm;
	int err = BCME_OK;
#ifdef TOF_DEBUG_TIME
	wlc_info_t *wlc;
	uint64 curtsf;
#endif // endif

	ASSERT(burstp != NULL);
	ASSERT(burstp->sm != NULL);
	ASSERT(burstp->wlc != NULL);

	sm = burstp->sm;

#ifdef TOF_DEBUG_TIME
	wlc = burstp->wlc;
	TOF_PRINTF(("%s mode %d\n", __FUNCTION__, sm->tof_mode));
#endif // endif
#ifdef TOF_DEBUG_TIME2
	wlc_read_tsf(burstp->wlc, &tsf_start, &tsf_hi);
	tsf_scanstart = tsf_txreq = tsf_rxack =
	tsf_rxm = tsf_tmo = tsf_lastm = 0;
#endif // endif
	burstp->smstoped = FALSE;
	burstp->distance = 0;
	burstp->meanrtt = 0;
	burstp->sdrtt = 0;
	bzero((void*)burstp->frame_type_cnt, sizeof(burstp->frame_type_cnt));
	bzero((void*)burstp->adj_type_cnt, sizeof(burstp->adj_type_cnt));

#ifdef TOF_COLLECT_DEBUG
	burstp->debug_cnt = 0;
#endif // endif
#ifdef TOF_COLLECT_INLINE
	pdburst_collect_inline_header_info_event(burstp);
#endif /* TOF_COLLECT_INLINE */

	if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
		err = pdburst_resolve_initator_mac_addr(burstp);
		if (err != BCME_OK)
			goto done;
		pdburst_reset(sm, sm->tof_mode, WL_PROXD_E_NOTSTARTED);
		bcopy(&burstp->configp->peer_mac, &sm->tof_peerea, ETHER_ADDR_LEN);
		if (ETHER_ISNULLADDR(&burstp->configp->peer_mac)) {
			burstp->smstoped = TRUE;
			return BCME_BADADDR;
		}
		pdburst_activate_pm(burstp);
		(void) pdburst_sm(burstp->sm, TOF_EVENT_WAKEUP, NULL, 0, NULL);
	} else {
		/* start hrt timer for duration */
		wlc_hrt_add_timeout(burstp->duration_timer,
			(FTM_INTVL2USEC(&burstp->configp->duration) -
				FTM_TX_OVRHD_BURST_DURATION_US),
			pdburst_duration_expired_target, (void *)burstp);
#ifdef TOF_DEBUG_TIME
		FTM_GET_TSF(wlc_ftm_get_handle(wlc), curtsf);
		TOF_PRINTF(("%s: BURST START TIME = %u.%u \n",
			__FUNCTION__, FTM_LOG_TSF_ARG(curtsf)));
#endif // endif
		burstp->duration_timer_active = TRUE;
		pdburst_process_rx_frame(burstp, NULL, NULL, 0, burstp->configp->ratespec, NULL);
	}
done:
#ifdef TOF_DEBUG_UCODE
	TOF_PRINTF(("%s err %d\n", __FUNCTION__, err));
#endif // endif
	return err;
}

int
pdburst_suspend(pdburst_t *burstp)
{
	pdburst_sm_t *sm = NULL;

#ifdef TOF_DEBUG_TIME
	TOF_PRINTF(("%s\n", __FUNCTION__));
#endif // endif

	if (burstp)
		sm = burstp->sm;

	if (sm && !burstp->smstoped) {
		if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
			pdburst_confirmed(sm, WL_PROXD_E_CANCELED);
			burstp->smstoped = TRUE;
			pdburst_pwron(burstp, FALSE);
		} else {
			pdburst_hw(burstp, FALSE, FALSE);
			pdburst_target_done(sm, WL_PROXD_E_CANCELED);
		}
	}

	return BCME_OK;
}

int
pdburst_rx(pdburst_t *burstp, wlc_bsscfg_t *bsscfg, const dot11_management_header_t *hdr,
	const uint8 *body, int body_len, const wlc_d11rxhdr_t *wrxh, ratespec_t rspec,
	const pdburst_tsinfo_t *tsinfo)
{
	/* This is rxed measurement packet */
	if (burstp) {
		burstp->bsscfg = bsscfg;

		return pdburst_process_rx_frame(burstp, wrxh, body, body_len, rspec, tsinfo);
	}
#ifdef TOF_COLLECT_REMOTE
	else {
		if (pdburst_collect && pdburst_collect->pdburstp)
			return pdburst_process_rx_frame(pdburst_collect->pdburstp, wrxh, body,
				body_len, rspec, tsinfo);
		return BCME_DATA_NOTFOUND;
	}
#endif /* TOF_COLLECT_REMOTE */
	return BCME_OK;
}

void
pdburst_destroy(pdburst_t **in_burst)
{
	/* remove create method */
	pdburst_t *burstp;
#ifdef TOF_DEBUG_TIME
	TOF_PRINTF(("%s\n", __FUNCTION__));
#endif // endif

	if (!in_burst)
		goto done;

	burstp = *in_burst;
	*in_burst = NULL;

	if (burstp) {
		burstp->smstoped = TRUE;
		burstp->ctx = NULL;
		burstp->configp = NULL;
		burstp->svc_funcs = NULL;
#ifdef TOF_COLLECT
		if (burstp->collectp) {
			if (burstp->collectp->collect_method &
				WL_PROXD_COLLECT_METHOD_TYPE_IOVAR) {
				if (pdburst_collect ||
					(burstp->sm->tof_state == TOF_STATE_ICONFIRM)) {
					/* wait collect to finish, then destroy */
					if (FTM_BSSCFG_SECURE(wlc_ftm_get_handle(burstp->wlc),
						burstp->bsscfg)) {
						phy_rxgcrs_sel_classifier(
							(phy_info_t *) WLC_PI(burstp->wlc),
							TOF_CLASSIFIER_BPHY_ON_OFDM_ON);
					}
					goto done;
				}
			} else {
				/* Non-IOVAR so no need to retain collect buffers */
				pdburst_collect_mem(burstp->wlc, &pdburst_collect, FALSE);
				if (FTM_BSSCFG_SECURE(wlc_ftm_get_handle(burstp->wlc),
					burstp->bsscfg)) {
					phy_rxgcrs_sel_classifier(
						(phy_info_t *) WLC_PI(burstp->wlc),
						TOF_CLASSIFIER_BPHY_ON_OFDM_ON);
				}
				burstp->collectp = NULL;
			}
		}
#endif /* TOF_COLLECT */
		if (burstp->randmac_in_use) {
			wlc_proxd_release_randmac(burstp->wlc->pdsvc_info, burstp->bsscfg);
			burstp->randmac_in_use = FALSE;
		}
#ifdef WL_PROXD_UCODE_TSYNC
		if (PROXD_ENAB_UCODE_TSYNC(burstp->wlc->pub)) {
			pdburst_clear_ucode_ack_block(burstp->wlc);
		}
#endif /* WL_PROXD_UCODE_TSYNC */
		pdburst_deactivate_pm(burstp);
		pdburst_measure(burstp, TOF_RESET);
		phy_tof_cmd(WLC_PI(burstp->wlc), FALSE, 0);
		pdburst_hw(burstp, FALSE, FALSE);
		if (FTM_BSSCFG_SECURE(wlc_ftm_get_handle(burstp->wlc),
			burstp->bsscfg)) {
			phy_rxgcrs_sel_classifier((phy_info_t *) WLC_PI(burstp->wlc),
				TOF_CLASSIFIER_BPHY_ON_OFDM_ON);
		}
#ifdef WL_RANGE_SEQ
		if (!BURST_NOCHANSWT(burstp->flags) && burstp->noscanengine)
#endif // endif
		pdburst_pwron(burstp, FALSE);

		if (burstp->timeractive) {
			burstp->timeractive = FALSE;
			wl_del_timer(burstp->wlc->wl, burstp->timer);
		}
		if (burstp->duration_timer != NULL) {
			wlc_hrt_free_timeout(burstp->duration_timer);
			burstp->duration_timer_active = FALSE;
			burstp->duration_timer = NULL;
		}

		if (burstp->ftm_tx_timer != NULL) {
			wlc_hrt_free_timeout(burstp->ftm_tx_timer);
			burstp->ftm_tx_timer_active = FALSE;
			burstp->ftm_tx_timer = NULL;
		}

		if (burstp->sm) {
			MFREE(burstp->wlc->osh, burstp->sm, sizeof(pdburst_sm_t));
			burstp->sm = NULL;
		}

		if (burstp->timer) {
			wl_free_timer(burstp->wlc->wl, burstp->timer);
			burstp->timer = NULL;
		}

		if (burstp->tof_tslist.tslist) {
			MFREE(burstp->wlc->osh, burstp->tof_tslist.tslist,
				burstp->measurecnt * sizeof(ftmts_t));
			burstp->tof_tslist.tslist = NULL;
		}

		if (burstp->lasttsp) {
			MFREE(burstp->wlc->osh, burstp->lasttsp, sizeof(ftmts_t));
			burstp->lasttsp = NULL;
		}

		if (burstp->mf_buf) {
			MFREE(burstp->wlc->osh, burstp->mf_buf, burstp->mf_buf_len);
			burstp->mf_buf = NULL;
			burstp->mf_buf_len = 0;
		}

		if (burstp->mf_stats_buf) {
			MFREE(burstp->wlc->osh, burstp->mf_stats_buf, burstp->mf_stats_buf_len);
			burstp->mf_stats_buf = NULL;
			burstp->mf_stats_buf_len = 0;
		}
		if (!(burstp->delayfree & TOF_DFREE_TXDONE)) {
			/* No measurement frame is pending, remove callback */
			wlc_pcb_fn_find(burstp->wlc->pcb, pdburst_tx_complete, burstp, TRUE);
		}
		/* mark burst first as destroyed - even when freeing */
		burstp->destroyed = TRUE;
		if (!burstp->delayfree)
			MFREE(burstp->wlc->osh, burstp, sizeof(pdburst_t));
	}

done:
	return;
}

void
pdburst_dump(const pdburst_t *burst, struct bcmstrbuf *b)
{
}
#if defined(TOF_COLLECT) || defined(TOF_COLLECT_REMOTE)
static void
pdburst_collect_prep_header(pdburst_collect_t *collectp,
	wl_proxd_collect_header_t *header)
{
	ratespec_t ackrspec;
	pdburst_t *burstp;
	wlc_info_t *wlc;

	if (!collectp || !header)
		return;

	if (!collectp->pdburstp)
		return;

	burstp = collectp->pdburstp;
	wlc = burstp->wlc;
	header->total_frames = (uint16)collectp->collect_cnt;
	if (CHSPEC_IS80(collectp->configp->chanspec)) {
		header->nfft = 256;
		header->bandwidth = 80;
	} else if (CHSPEC_IS40(collectp->configp->chanspec)) {
		header->nfft = 128;
		header->bandwidth = 40;
	} else if (CHSPEC_IS20(collectp->configp->chanspec)) {
		header->nfft = 64;
		header->bandwidth = 20;
	} else {
		header->nfft = 0;
		header->bandwidth = 10;
	}

	header->channel = CHSPEC_CHANNEL(collectp->configp->chanspec);
	header->chanspec = burstp->configp->chanspec;
	header->fpfactor = burstp->Tq;
	header->fpfactor_shift = TOF_SHIFT;
	memcpy((void*)&header->params, (void*)burstp->tunep,
		sizeof(wl_proxd_params_tof_tune_t));
	if (!BURST_IS_VHTACK(burstp) ||
		!RSPEC_ISVHT(burstp->configp->ratespec)) {
		/* no vhtack */
		ackrspec = LEGACY_RSPEC(PROXD_DEFAULT_TX_RATE);
	} else {
		ackrspec = LEGACY_RSPEC(PROXD_DEFAULT_TX_RATE) |
			WL_RSPEC_ENCODE_VHT;
	}
	wlc_phy_kvalue(WLC_PI(wlc), burstp->configp->chanspec,
		proxd_get_ratespec_idx(burstp->configp->ratespec,
			ackrspec),
		&header->params.Ki, &header->params.Kt,
		((collectp->collect_info.type == TOF_ADJ_SEQ) ?
			WL_PROXD_SEQEN : 0));
	header->distance = burstp->distance;
	header->meanrtt = burstp->meanrtt;
	header->modertt = 0;
	header->medianrtt = 0;
	header->sdrtt = burstp->sdrtt;
	si_pmu_fvco_pllreg(wlc->hw->sih, NULL, &header->clkdivisor);
	header->clkdivisor &= PMU1_PLL0_PC1_M1DIV_MASK;
	header->chipnum = burstp->chipnum;
	header->chiprev = burstp->chiprev;
	header->phyver = wlc->band->phyrev;

	memcpy(&header->localMacAddr, &burstp->sm->tof_selfea,
		ETHER_ADDR_LEN);
	if (ETHER_ISNULLADDR(&burstp->sm->tof_peerea))
		memcpy(&header->remoteMacAddr, &collectp->configp->peer_mac,
			ETHER_ADDR_LEN);
	else
		memcpy(&header->remoteMacAddr, &burstp->sm->tof_peerea,
			ETHER_ADDR_LEN);
}
#endif /* TOF_COLLECT || TOF_COLLECT_REMOTE */

#if defined(TOF_COLLECT) || defined(TOF_COLLECT_INLINE)
static void
pdburst_collect_init_event(wl_proxd_event_t *event,
	wl_proxd_session_id_t sid)
{
	event->version = htol16(WL_PROXD_API_VERSION);
	event->len = htol16(OFFSETOF(wl_proxd_event_t, tlvs));
	event->type = htol16(WL_PROXD_EVENT_COLLECT);
	event->method = htol16(WL_PROXD_METHOD_FTM);
	event->sid = htol16(sid);
	bzero(event->pad, sizeof(event->pad));
}
#endif // endif

#ifdef TOF_COLLECT_INLINE

/* Get buffer size for a type of pdburst event */
static uint32
pdburst_collect_get_buf_size(uint8 type)
{
	uint32 size = sizeof(wl_proxd_event_t) + sizeof(wl_proxd_tlv_t);
	switch (type) {
		case TOF_COLLECT_INLINE_HEADER :
			size += sizeof(wl_proxd_collect_inline_header_info_t);
			break;
		case TOF_COLLECT_INLINE_FRAME_INFO:
			size += sizeof(wl_proxd_collect_inline_frame_info_t);
			break;
		case TOF_COLLECT_INLINE_FRAME_INFO_CHAN_DATA:
			size += TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD;
			break;
		case TOF_COLLECT_INLINE_RESULTS:
			size += sizeof(wl_proxd_collect_inline_results_info_t);
			break;
		default:
			size = 0;
			break;
	}
	return size;
}

/* Allocate event buffer for different type of log buffers */
static wl_proxd_event_t *
pdburst_collect_inline_event_alloc(pdburst_t *pburst, uint8 type)
{
	wl_proxd_event_t *event = NULL;
	wl_proxd_session_id_t sid;
	uint32 buf_size = pdburst_collect_get_buf_size(type);

	if (buf_size == 0)
		goto fail;

	/* Allocate event buffer */
	if ((event = (wl_proxd_event_t *)MALLOCZ(pburst->wlc->osh, buf_size)) == NULL) {
		FTM_ERR(("wl:%d %s MALLOC failed malloced %d bytes\n", pburst->wlc->pub->unit,
			__FUNCTION__, MALLOCED(pburst->wlc->pub->osh)));
		goto fail;
	}

	/* This need to be changed */
	sid = (pburst->ctx != NULL)? ((pdftm_session_t *)pburst->ctx)->sid:
		WL_PROXD_SESSION_ID_GLOBAL;

	pdburst_collect_init_event(event, sid);

fail:
	return event;
}

static void
pburst_collect_inline_event_free(pdburst_t *pburst, uint8 type, wl_proxd_event_t *event)
{
	uint32 buf_size;

	if (event != NULL) {
		buf_size = pdburst_collect_get_buf_size(type);

		MFREE(pburst->wlc->osh, event, buf_size);
	}
}

/* Send measurement parameter information - Send during start of measurement */
static int
pdburst_collect_inline_header_info_event(pdburst_t *burstp)
{
	int ret = BCME_OK;
	wl_proxd_tlv_t *tlv;
	wl_proxd_collect_inline_header_info_t *header;

	wl_proxd_event_t *event = pdburst_collect_inline_event_alloc(burstp,
		TOF_COLLECT_INLINE_HEADER);

	if (event == NULL) {
		ret = BCME_NOMEM;
		goto fail;
	}

	tlv = (wl_proxd_tlv_t *)event->tlvs;
	header = (wl_proxd_collect_inline_header_info_t *)tlv->data;

	/* Prep for info */
	tlv->id = WL_PROXD_TLV_ID_COLLECT_INLINE_HEADER;
	tlv->len = sizeof(wl_proxd_collect_inline_header_info_t);
	event->pad[0] = 0;
	event->pad[1] = 0;
	event->len += OFFSETOF(wl_proxd_tlv_t, data) +
		sizeof(wl_proxd_collect_inline_header_info_t);

	/* Create header */
	header->version = TOF_COLLECT_INLINE_HEADER_INFO_VER;
	header->chanspec = burstp->configp->chanspec;
	header->ratespec = burstp->configp->ratespec;
	header->num_ftm = burstp->configp->num_ftm;
	eacopy(&burstp->configp->peer_mac, &header->peer_mac);
	bcm_ether_privacy_mask(&header->peer_mac);
	eacopy(&burstp->configp->cur_ether_addr, &header->cur_ether_addr);
	bcm_ether_privacy_mask(&header->cur_ether_addr);

	EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)(event), event->len);
fail:
	if (event) {
		pburst_collect_inline_event_free(burstp, TOF_COLLECT_INLINE_HEADER, event);
	}
	return ret;
}

/* Send measurement output information */
static int
pdburst_collect_inline_results(pdburst_t *burstp, pdburst_results_t *res)
{
	int ret = BCME_OK;
	wl_proxd_tlv_t *tlv;
	wl_proxd_collect_inline_results_info_t *results;

	wl_proxd_event_t *event = pdburst_collect_inline_event_alloc(burstp,
		TOF_COLLECT_INLINE_RESULTS);

	if (event == NULL) {
		ret = BCME_NOMEM;
		goto fail;
	}

	tlv = (wl_proxd_tlv_t *)event->tlvs;
	results = (wl_proxd_collect_inline_results_info_t *)tlv->data;

	tlv->id = WL_PROXD_TLV_ID_COLLECT_INLINE_RESULTS;
	tlv->len = sizeof(wl_proxd_collect_inline_results_info_t);
	event->pad[0] = 0;
	event->pad[1] = 0;
	event->len += OFFSETOF(wl_proxd_tlv_t, data) +
		sizeof(wl_proxd_collect_inline_results_info_t);

	/* Create header */
	results->version = TOF_COLLECT_INLINE_RESULTS_VER;
	results->meanrtt = burstp->meanrtt;
	results->distance = res->avg_dist;
	results->num_rtt = res->num_rtt;
	results->status = res->status;
	results->ratespec = res->avg_rtt.ratespec;
	results->pad1 = 0;
	results->pad2 = 0;

	EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)(event), event->len);
fail:
	if (event) {
		pburst_collect_inline_event_free(burstp, TOF_COLLECT_INLINE_RESULTS, event);
	}
	return ret;
}

/* Send a transaction specific information */
static int
pdburst_collect_inline_frame_info_data_events(pdburst_t *pburst, ftmts_t * list, uint8 frame_cnt)
{
	int ret = BCME_OK;
	wl_proxd_tlv_t *tlv;
	wl_proxd_collect_inline_frame_info_t *frame_info;
	wl_proxd_event_t *event = pdburst_collect_inline_event_alloc(pburst,
		TOF_COLLECT_INLINE_FRAME_INFO);

	if (event == NULL) {
		ret = BCME_NOMEM;
		goto fail;
	}

	tlv = (wl_proxd_tlv_t *)event->tlvs;
	frame_info = (wl_proxd_collect_inline_frame_info_t *)tlv->data;

	/* Send frame specific results */
	event->pad[0] = frame_cnt;
	event->pad[1] = 0;
	tlv->id = WL_PROXD_TLV_ID_COLLECT_INLINE_FRAME_INFO;
	tlv->len = sizeof(*frame_info);
	event->len +=  OFFSETOF(wl_proxd_tlv_t, data) + sizeof(*frame_info);

	/* Create frame specific information from rtt_collect */
	frame_info->version = TOF_COLLECT_INLINE_FRAME_INFO_VER;
	frame_info->pad1 = 0;

	frame_info->gd = list->gd;
	frame_info->hadj = list->adj;
	frame_info->rssi = list->rssi;
	frame_info->T[0] = list->t1;
	frame_info->T[1] = list->t2;
	frame_info->T[2] = list->t3;
	frame_info->T[3] = list->t4;

	frame_info->pad[0] = 0;
	frame_info->pad[1] = 0;
	frame_info->pad[2] = 0;

	EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)event,
		event->len);
fail:
	if (event) {
		pburst_collect_inline_event_free(pburst, TOF_COLLECT_INLINE_FRAME_INFO, event);
	}
	return ret;
}

static int
pdburst_collect_inline_frame_info_chan_data(pdburst_t *pburst, uint32 *chan_data,
	uint32 data_size)
{
	int ret = BCME_OK;
	int frag_i, num_frags;
	uint16 frag_size = 0, offset = 0, pending_data_sz = 0;
	wl_proxd_tlv_t *tlv;
	wl_proxd_event_t *event;

	event = pdburst_collect_inline_event_alloc(pburst, TOF_COLLECT_INLINE_FRAME_INFO_CHAN_DATA);

	if (event == NULL) {
		ret = BCME_NOMEM;
		goto fail;
	}

	tlv = (wl_proxd_tlv_t *)event->tlvs;

	num_frags = data_size / TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD;
	pending_data_sz = data_size % TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD;

	if (pending_data_sz)
		num_frags++;

	event->pad[0] = num_frags;
	tlv->id = WL_PROXD_TLV_ID_COLLECT_INLINE_FRAME_DATA;

	/* Send frame data */
	for (frag_i = 0; frag_i < num_frags; frag_i++) {
		bzero(tlv->data, TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD);
		/* frag_size is in number of words (4-bytes) */
		if ((frag_i != (num_frags - 1)) || (pending_data_sz == 0))
			frag_size = TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD;
		else
			frag_size = pending_data_sz;
		offset = frag_i * TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD;
		event->pad[1] = frag_i;
		tlv->len = frag_size;
		event->len = OFFSETOF(wl_proxd_event_t, tlvs) +
			OFFSETOF(wl_proxd_tlv_t, data) + frag_size;
		/* frag_size will be alwasys TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD */
		(void)memcpy_s(tlv->data, TOF_COLLECT_INLINE_EVENT_LOG_MAX_PAYLOAD,
			(((uint8 *)chan_data) + offset), frag_size);
		EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)event,
			event->len);
	}
fail:
	if (event) {
		pburst_collect_inline_event_free(pburst,
			TOF_COLLECT_INLINE_FRAME_INFO_CHAN_DATA, event);
	}
	return ret;
}

#endif /* TOF_COLLECT_INLINE */

#ifdef TOF_COLLECT
static int pdburst_collect_event(pdburst_t *burstp)
{
	int ret = BCME_OK;
	if (burstp && burstp->collectp) {
		if (burstp->collectp->collect_method & WL_PROXD_COLLECT_METHOD_TYPE_EVENT) {
			ret = pdburst_collect_generic_event(burstp);
		}
		if (burstp->collectp->collect_method & WL_PROXD_COLLECT_METHOD_TYPE_EVENT_LOG) {
			ret = pdburst_collect_event_log(burstp->collectp);
		}
	}
	return ret;
}

static int pdburst_collect_generic_event(pdburst_t *burstp)
{
	int i;
	int ret = BCME_OK;
	pdftm_session_t *sn = NULL;
	wl_proxd_session_id_t sid = 0;
	pdburst_collect_t *collectp = NULL;
	uint16 event_len = 0;
	uint16 tlv_len = 0;
	uint16 buf_size = 0;
	uint8 *event_buf = NULL;
	wl_proxd_event_t *event = NULL;
	wl_proxd_tlv_t *tlv = NULL;
	wl_proxd_collect_event_data_t *cop = NULL;
	wl_proxd_collect_data_t *collect = NULL;
	uint32 *outp = NULL;
#ifdef WL_PROXD_SEQ
	wlc_phy_tof_info_t tof_info;
#endif /* WL_PROXD_SEQ */

	if (!burstp || !burstp->collectp) {
		return BCME_NOTREADY;
	}

	collectp = burstp->collectp;
	if (!collectp->collect_cnt) {
		return BCME_NOTREADY;
	}

	if (!CHSPEC_IS20(collectp->configp->chanspec)) {
		/* only supports 20MHZ */
		return BCME_UNSUPPORTED;
	}

	buf_size = sizeof(wl_proxd_event_t) + sizeof(wl_proxd_tlv_t) +
		sizeof(wl_proxd_collect_event_data_t);
	if ((event_buf = (uint8 *)MALLOCZ(burstp->wlc->osh, buf_size)) == NULL) {
		FTM_ERR(("wl:%d %s MALLOC failed malloced %d bytes\n", burstp->wlc->pub->unit,
			__FUNCTION__, MALLOCED(burstp->wlc->pub->osh)));
		return BCME_NOMEM;
	}

	event = (wl_proxd_event_t *)event_buf;
	tlv = (wl_proxd_tlv_t *)event->tlvs;
	cop = (wl_proxd_collect_event_data_t *)tlv->data;

	sn = (pdftm_session_t *)burstp->ctx;
	sid = (sn != NULL)? sn->sid : WL_PROXD_SESSION_ID_GLOBAL;

	/* calculate lengths */
	tlv_len = sizeof(wl_proxd_collect_event_data_t);
	event_len = OFFSETOF(wl_proxd_event_t, tlvs) + OFFSETOF(wl_proxd_tlv_t, data) + tlv_len;

	/* initialize event/tlv header info */
	bzero(event_buf, sizeof(event_buf));
	pdburst_collect_init_event(event, sid);
	event->type = WL_PROXD_EVENT_COLLECT;
	event->len = event_len;
	tlv->id = WL_PROXD_TLV_ID_COLLECT_DATA;
	tlv->len = tlv_len;
	int collect_h_size;

	if (CHSPEC_IS20(burstp->configp->chanspec)) {
		collect_h_size = K_TOF_COLLECT_H_SIZE_20MHZ;
#ifndef WL_RANGE_SEQ
	} else {
		collect_h_size = K_TOF_COLLECT_H_SIZE_80MHZ;
	}
#endif /* !WL_RANGE_SEQ */
	if (collectp->collect_cnt && collectp->collect_cnt <= collectp->collect_size) {
		for (i = 0; i < collectp->collect_cnt; i++) {
			collect = collectp->collect + i;
			if (collect) {
#ifdef WL_RANGE_SEQ
				outp = (!i)? cop->H_LB : cop->H_RX;
				(void)memcpy_s(outp, collect_h_size * sizeof(uint32),
					collect->H, (collect_h_size * sizeof(uint32)));
#else
				outp = (i)? cop->H_LB : cop->H_RX;
				memcpy(outp, collect->H,
					(collect_h_size * sizeof(uint32)));
#endif // endif
			}
		}
	}
#ifdef WL_RANGE_SEQ
	} else {
		collect_h_size = K_TOF_COLLECT_H_SIZE_80MHZ;
	}
	(void)memcpy_s(cop->ri_rr, sizeof(collectp->ri_rr),
		collectp->ri_rr, sizeof(collectp->ri_rr));
#else
	memcpy(cop->ri_rr, collectp->ri_rr, sizeof(collectp->ri_rr));
#endif /* WL_RANGE_SEQ */
	/* query phy for phy error mask */
#ifdef WL_PROXD_SEQ
	bzero(&tof_info, sizeof(tof_info));
	wlc_phy_tof_info(WLC_PI(burstp->wlc), &tof_info,
		WLC_PHY_TOF_INFO_TYPE_ALL, burstp->core);
	cop->phy_err_mask = tof_info.tof_phy_error;
#else /* WL_PROXD_SEQ */
	cop->phy_err_mask = 0;
#endif /* WL_PROXD_SEQ */

	cop->version = WL_PROXD_COLLECT_EVENT_DATA_VERSION_MAX;
#ifdef WL_RANGE_SEQ
	cop->length = sizeof(*cop) - OFFSETOF(wl_proxd_collect_event_data_t, H_LB);
#else
	cop->length = sizeof(*cop) - OFFSETOF(*cop, H_LB);
#endif /* WL_RANGE_SEQ */
	proxd_send_event(burstp->wlc->pdsvc_info, burstp->bsscfg, WL_PROXD_E_OK,
		&burstp->sm->tof_selfea, event, event->len);

#ifdef WL_RANGE_SEQ
	MFREE(collectp->pdburstp->wlc->osh, event_buf, buf_size);
#else
	MFREE(burstp->wlc->osh, event_buf, buf_size);
#endif // endif
	return ret;
}

static int
pdburst_collect_gen_info_data_events(pdburst_collect_t *collectp)
{
	int ret = BCME_OK;
#ifdef EVENT_LOG_COMPILE
	int frame_i, frag_i, num_frags;
	uint16 frag_size;
	uint8 offset = 0;
	wl_proxd_session_id_t sid;
	uint16 sample_size, buf_size;
	uint8 *event_buf = NULL;
	wl_proxd_event_t *event = NULL;
	wl_proxd_tlv_t *tlv = NULL;
	wlc_info_t *wlc = NULL;

	if (!collectp || !collectp->pdburstp || !collectp->configp)
		return BCME_NOTREADY;

	wlc = collectp->pdburstp->wlc;

	if (!CHSPEC_IS20(collectp->configp->chanspec)) {
		/* only supports 20MHZ */
		return BCME_UNSUPPORTED;
	}

	/* determine buffer size */
	sample_size = K_TOF_COLLECT_H_SIZE_20MHZ;
	num_frags = (sample_size / EVENT_LOG_MAX_RECORD_PAYLOAD_SIZE) + 1;
	buf_size = sizeof(wl_proxd_event_t) + sizeof(wl_proxd_tlv_t) +
		((sample_size/num_frags + 1) * sizeof(uint32));

	/* event_buf is reused for info, data, chan_data, and ri_rr */
	if ((event_buf = (uint8 *)MALLOCZ(wlc->osh, buf_size)) == NULL) {
		FTM_ERR(("wl:%d %s MALLOC failed malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->pub->osh)));
		return BCME_NOMEM;
	}

	event = (wl_proxd_event_t *)event_buf;
	tlv = (wl_proxd_tlv_t *)event->tlvs;

	sid = (pdftm_session_t *)(collectp->pdburstp->ctx) != NULL ?
		((pdftm_session_t *)(collectp->pdburstp->ctx))->sid :
		WL_PROXD_SESSION_ID_GLOBAL;

	for (frame_i = 0; frame_i < collectp->collect_cnt; frame_i++) {
		bzero(event_buf, buf_size);
		/* Prep for info */
		pdburst_collect_init_event(event, sid);
		tlv->id = WL_PROXD_TLV_ID_COLLECT_INFO;
		tlv->len = sizeof(wl_proxd_collect_info_t);
		event->pad[0] = frame_i;
		event->len += OFFSETOF(wl_proxd_tlv_t, data) + sizeof(wl_proxd_collect_info_t);
		memcpy(tlv->data, &((collectp->collect + frame_i)->info),
			sizeof(wl_proxd_collect_info_t));
		EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)(event), event->len);

		/* Prep for data */
		for (frag_i = 0; frag_i < num_frags; frag_i++) {
			bzero(event_buf, buf_size);
			/* frag_size is in number of words (4-bytes) */
			if (num_frags > 1) {
				frag_size = (frag_i) ? (sample_size/num_frags) :
						((sample_size/num_frags) + 1);
			} else {
				frag_size = (sample_size/num_frags);
			}
			offset = frag_i * ((sample_size/num_frags) + 1);
			pdburst_collect_init_event(event, sid);
			event->pad[0] = frame_i;
			event->pad[1] = frag_i;
			tlv->id = WL_PROXD_TLV_ID_COLLECT_DATA;
			tlv->len = frag_size;	/* number of words */
			event->len += OFFSETOF(wl_proxd_tlv_t, data) + (tlv->len * sizeof(uint32));
			memcpy(tlv->data, ((collectp->collect + frame_i)->H + offset),
				(tlv->len * sizeof(uint32)));
			EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)event,
				event->len);
		}
	}

	/* Prep for chan data */
	/* send in fragments with size of K_TOF_COLLECT_SC_20MHz (64 words) */
	num_frags = K_TOF_COLLECT_CHAN_SIZE / K_TOF_COLLECT_SC_20MHZ;
	for (frag_i = 0; frag_i < num_frags; frag_i++) {
		bzero(event_buf, buf_size);
		pdburst_collect_init_event(event, sid);
		tlv->id = WL_PROXD_TLV_ID_COLLECT_CHAN_DATA;
		tlv->len = K_TOF_COLLECT_SC_20MHZ;	/* number of words (4-bytes) */
		event->pad[0] = frag_i;
		event->pad[1] = num_frags;
		event->len += OFFSETOF(wl_proxd_tlv_t, data) + (tlv->len * sizeof(uint32));

		memcpy(tlv->data, ((collectp->chan) + (frag_i * tlv->len)),
			(tlv->len * sizeof(uint32)));
		EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)event,
			event->len);
	}

	/* Prep for rirr */
	bzero(event_buf, buf_size);
	pdburst_collect_init_event(event, sid);
	tlv->id = WL_PROXD_TLV_ID_RI_RR;
	tlv->len = FTM_TPK_RI_RR_LEN;	/* in bytes */
	event->len += OFFSETOF(wl_proxd_tlv_t, data) + tlv->len;
	memcpy(tlv->data, collectp->ri_rr, FTM_TPK_RI_RR_LEN);
	EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)event,
		event->len);

	MFREE(burstp->wlc->osh, event_buf, buf_size);
#endif /* EVENT_LOG_COMPILE */
	return ret;
}

static int
pdburst_collect_event_log(pdburst_collect_t *collectp)
{
	int ret = BCME_OK;
#ifdef EVENT_LOG_COMPILE
	wl_proxd_session_id_t sid;
	uint8 event_buf[sizeof(wl_proxd_event_t) + sizeof(wl_proxd_tlv_t) +
		sizeof(wl_proxd_collect_header_t)];
	wl_proxd_event_t *event = (wl_proxd_event_t *)event_buf;
	wl_proxd_tlv_t *tlv = (wl_proxd_tlv_t *)event->tlvs;
	wl_proxd_collect_header_t *header = (wl_proxd_collect_header_t *)tlv->data;

	if (!collectp) {
		return BCME_NOTREADY;
	}

	/* total_frames is 0, skip */
	if (!collectp->collect_cnt) {
		return BCME_NOTREADY;
	}

	sid = (pdftm_session_t *)(collectp->pdburstp->ctx) != NULL ?
		((pdftm_session_t *)(collectp->pdburstp->ctx))->sid :
		WL_PROXD_SESSION_ID_GLOBAL;

	/* prepare the header data */
	pdburst_collect_prep_header(collectp, header);

	pdburst_collect_init_event(event, sid);
	tlv->id = WL_PROXD_TLV_ID_COLLECT_HEADER;
	tlv->len = sizeof(wl_proxd_collect_header_t);
	event->len += OFFSETOF(wl_proxd_tlv_t, data) + tlv->len;
	EVENT_LOG_BUFFER(EVENT_LOG_TAG_PROXD_SAMPLE_COLLECT, (void *)event, event->len);

	ret = pdburst_collect_gen_info_data_events(collectp);
#endif /* EVENT_LOG_COMPILE */
	return ret;
}
#endif /* TOF_COLLECT */

uint8 ftm_vs_get_tof_txcnt(void *burst)
{
	pdburst_t *pdburstp = (pdburst_t *)burst;
	if ((pdburstp) && (pdburstp->sm)) {
		TOF_PRINTF(("ftm_vs_get_tof_txcnt(), tof_txcnt %d\n", pdburstp->sm->tof_txcnt));
		return pdburstp->sm->tof_txcnt;
	}
	return 0;
}

uint8 *
ftm_vs_init_mf_stats_buf(void *burstp)
{
	pdburst_t *pdburstp = (pdburst_t *)burstp;
	ASSERT(pdburstp != NULL);
	if (!pdburstp->mf_stats_buf) {
		if (!(pdburstp->mf_stats_buf =
			MALLOCZ(pdburstp->wlc->osh, MF_BUF_MAX_LEN))) {
			WL_ERROR(("ftm_vs_init_mf_stats_buf()"
				"MALLOC of mf_stats_buf data failed\n"));
			return NULL;
		}
		pdburstp->mf_stats_buf_len = MF_BUF_MAX_LEN;
	}
	return (uint8*)pdburstp->mf_stats_buf;
}

static void
pdburst_mf_stats_init_event(wl_proxd_event_t *event,
	wl_proxd_session_id_t sid)
{
	event->version = htol16(WL_PROXD_API_VERSION);
	event->len = htol16(OFFSETOF(wl_proxd_event_t, tlvs));
	event->type = htol16(WL_PROXD_EVENT_MF_STATS);
	event->method = htol16(WL_PROXD_METHOD_FTM);
	event->sid = htol16(sid);
	bzero(event->pad, sizeof(event->pad));
}

void pdburst_send_mf_stats_event(pdburst_t *burstp)
{
	pdftm_session_t *sn;
	wl_proxd_session_id_t sid;
	uint16 buf_len  = 0, buf_size = 0;
	uint8 *event_buf = NULL;
	wl_proxd_event_t *event = NULL;
	wl_proxd_tlv_t *tlv = NULL;

	ASSERT(burstp != NULL);
	if (!burstp) {
		return;
	}

	sn = (pdftm_session_t *)burstp->ctx;
	sid = (sn != NULL)? sn->sid : WL_PROXD_SESSION_ID_GLOBAL;

	buf_len = burstp->mf_stats_buf_len;
	buf_size = sizeof(wl_proxd_event_t) + sizeof(wl_proxd_tlv_t) + buf_len;

	event_buf = (uint8 *)MALLOCZ(burstp->wlc->osh, buf_size);
	if (!event_buf) {
		WL_ERROR(("MALLOC failed %s\n", __FUNCTION__));
		return;
	}

	event = (wl_proxd_event_t *)event_buf;
	pdburst_mf_stats_init_event(event, sid);
	event->len = OFFSETOF(wl_proxd_event_t, tlvs) + OFFSETOF(wl_proxd_tlv_t, data) + buf_len;

	tlv = (wl_proxd_tlv_t *)event->tlvs;
	tlv->id = WL_PROXD_TLV_ID_MF_STATS_DATA;
	tlv->len = buf_len;

	memcpy(tlv->data, burstp->mf_stats_buf, buf_len);
	proxd_send_event(burstp->wlc->pdsvc_info, burstp->bsscfg, WL_PROXD_E_OK,
		&burstp->sm->tof_selfea, event, event->len);

	MFREE(burstp->wlc->osh, event_buf, buf_size);
	return;
}

void ftm_vs_update_mf_stats_len(void *burstp, uint16 len)
{
	pdburst_t *pdburstp = (pdburst_t *)burstp;
	ASSERT(pdburstp != NULL);
	if ((pdburstp) && (pdburstp->mf_stats_buf)) {
		pdburstp->mf_stats_buf_len = len;
	}
}
#ifdef WL_PROXD_UCODE_TSYNC
/* For SHM definitions refer below confluence
* http://confluence.broadcom.com/display/WLAN/NewUcodeInterfaceForProxdFeature
*/
#define FTM_TIMESTAMP_SHIFT		16
#define TOF_AVB_ACK_BLOCK_SIZE		6
#define TXS_ACK_INDEX_SHIFT		3
#define RXH_ACK_INDEX_SHIFT		8
#define RXH_ACK_INDEX_SHIFT_REV_GE80	12
#define ACK_BLOCK_SIZE			3
#define NUM_UCODE_ACK_TS_BLKS		4

void
pdburst_process_tx_rx_status(wlc_info_t *wlc, tx_status_t *txs,
	d11rxhdr_t *rxh, struct ether_addr *peer)
{
	uint32 ftm_avb, ack_avb;
	uint32 off_set;
	uint16 ack_avb_blk[ACK_BLOCK_SIZE];
	int i;
	uint8 index;
	pdburst_sm_t *sm = NULL;
	pdburst_t *p = NULL;
	uint8 rxh_ack_shift;

	if (txs) {
		ftm_avb = txs->status.ack_map2;
		index = (txs->status.s5 >> TXS_ACK_INDEX_SHIFT) & 0x07;
	} else {
		if (D11REV_GE(wlc->pub->corerev, 128)) {
			/*
			Note: for non-FTM frames, AvbRxTimeH is reused for uplink
			rate info on AX chips
			*/
			ftm_avb = (D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, AvbRxTimeL) |
				(D11RXHDR_GE128_ACCESS_VAL(rxh, UlRtInfo) <<
				FTM_TIMESTAMP_SHIFT));
		} else {
			ftm_avb = (D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, AvbRxTimeL) |
				(D11RXHDR_LT80_ACCESS_VAL(rxh, AvbRxTimeH) <<
				FTM_TIMESTAMP_SHIFT));
		}
		if (D11REV_GE(wlc->pub->corerev, 80)) {
			rxh_ack_shift = RXH_ACK_INDEX_SHIFT_REV_GE80;
		} else {
			rxh_ack_shift = RXH_ACK_INDEX_SHIFT;
		}
		index = (D11RXHDR_ACCESS_VAL(rxh, wlc->pub->corerev, MuRate) >>
			rxh_ack_shift) & 0x07;
	}
	FTM_TRACE(("\n AVB FTMTS : %u tof_avb_block index %d\n",
		ftm_avb, index));

	if (D11REV_GE(wlc->pub->corerev, 80)) {
		if (index == 0x04) { /* invalid index 0x04..check */
			TOF_PRINTF(("\n No available block, so ack ts willnot be valid..ignore\n"));
			return;
		}
	} else if (index == 0x0F) { /* invalid index 0x0F..check */
		TOF_PRINTF(("\n No available block, so ack ts willnot be valid..ignore\n"));
		return;
	}
	off_set = index * TOF_AVB_ACK_BLOCK_SIZE + M_TOF_AVB_BLK(wlc->hw);

	/* Wait until ack time stamps are updated/valid */
	i = 0;
	while (!((wlc_bmac_read_shm(wlc->hw, off_set)) & 0x01) &&
		(i < TOF_ACK_TS_TIMEOUT)) {
		OSL_DELAY(1);
		i++;
	}

	for (i = 0; i < ACK_BLOCK_SIZE; i++) {
		ack_avb_blk[i] = wlc_bmac_read_shm(wlc->hw, off_set + 2*i);
	}

	/* IMP: reset the first word in ACK block so that UCODE
	* can reuse it. Note that we have only 4 ACK blocks
	*/
	wlc_bmac_write_shm(wlc->hw, off_set, 0x0000);

	if (txs && !(txs->status.was_acked)) {
		TOF_PRINTF(("\npacket not acked, so ack ts willnot be valid..ignore\n"));
		return;
	}

	FTM_TRACE(("\n avb1 0x%04x avb2 0x%04x avb3 0x%04x\n", ack_avb_blk[0],
		ack_avb_blk[1], ack_avb_blk[2]));

	if (!(ack_avb_blk[0] & 0x8000)) {
		TOF_PRINTF(("\n ack field not valid \n"));
		wlc_bmac_write_shm(wlc->hw, M_TOF_AVB_ACK_ON(wlc->hw), 0);
		return;
	}

	FTM_TRACE(("\n avb1 %u avb2 %u avb3 %u\n", ack_avb_blk[0],
		ack_avb_blk[1], ack_avb_blk[2]));

	p = (pdburst_t *)pdftm_session_get_burstp(wlc, peer,
		(txs != NULL) ? WL_PROXD_SESSION_FLAG_TARGET :
		WL_PROXD_SESSION_FLAG_INITIATOR);
	if (!p) { /* TODO: debug this */
		TOF_PRINTF(("\n%s:burst/ssn not created or already freed\n", __FUNCTION__));
		return;
	}

	sm = p->sm;
	if (!sm) {
		TOF_PRINTF(("\ntxs: Invalid state machine..some thing wrong\n"));
		return;
	}
	ack_avb = ((ack_avb_blk[1]) & 0xFFFF) |
		((ack_avb_blk[2] & 0xFFFF) << FTM_TIMESTAMP_SHIFT);
	if (txs) {
		sm->tod = ftm_avb;
		sm->toa = ack_avb;
		p->ack_rx_rspec = pdburst_get_tgt_ackrspec(p, ack_avb_blk[0]);
	} else {
		sm->tod = ack_avb;
		sm->toa = ftm_avb;
	}
	FTM_TRACE(("\n AVB FTMTS : %u ACKTS : %u tof_avb_block index %d\n",
		ftm_avb, ack_avb, index));
}

/* Called when destroying the burst.
* It can happen by the time a FTM measurement is received in
* ucode, burst might be stopping. This case randmac will clear
* the multi_mac and ATM entries which results in tossing the
* received frame. So clear/reset all ACK SHM blocks
*/
static void
pdburst_clear_ucode_ack_block(wlc_info_t *wlc)
{
	uint8 index = 0;
	uint16 off_set = 0;
	for (index = 0; index < NUM_UCODE_ACK_TS_BLKS; index++) {
		off_set = index * TOF_AVB_ACK_BLOCK_SIZE +
			M_TOF_AVB_BLK(wlc->hw);
		wlc_bmac_write_shm(wlc->hw, off_set, 0x0000);
	}
}
#endif /* WL_PROXD_UCODE_TSYNC */
