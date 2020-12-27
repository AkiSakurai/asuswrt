/*
 * Common (OS-independent) portion of
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_pdtof.c 467163 2014-04-02 17:41:32Z $
 */

/*
 * TOF based proximity detection implementation for Broadcom 802.11 Networking Driver
*/

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <wlc_hrt.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scan.h>
#include <wl_export.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <hndpmu.h>
#include <wlc_pcb.h>

#include <wlc_pdsvc.h>
#include <wlc_pddefs.h>
#include <wlc_pdmthd.h>

#if defined(linux) && defined(__KERNEL__)
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/types.h>
#endif

/* #define TOF_COLLECT */

/* #define TOF_DEBUG_TIME */
/* #define TOF_DEBUG_TIME2 */
/* #define TOF_DEBUG_FTM */
/* #define TOF_STDDEV_CHECK */
/* #define TOF_PROFILE */

#ifdef TOF_PROFILE
#define TOF_DEBUG_TIME2
#endif

#ifdef TOF_DEBUG_FTM
#define MDBG(x)  printf x /* dbg messages for non-timing sensetive code  */
#else
#define MDBG(x)
#endif

#ifdef atoi
#undef atoi
#endif
#define	atoi bcm_atoi

#define	TOF_VER			1
#define	TOF_DEFAULT_INTERVAL	200
#define	TOF_DEFAULT_DURATION	200
#define	TOF_DEFAULT_TIMEOUT	20
#define	TOF_REQ_RETRY_CNT	6

/*
	FTM frame period for target, it's schip specific
	and can be configured via "proxd_ftmperiod" iovar,
	the following were calculated for 4335b0:
*/
#define FTM_PERIOD_80		940
#define FTM_PERIOD_40		460
#define FTM_PERIOD_20		90

#define TOF_FTM_DEFAULT_PERIOD	FTM_PERIOD_80

#define TOF_DEFAULT_GD_ADJ	1
#define TOF_DEFAULT_SW_ADJ	1
#define TOF_DEFAULT_HW_ADJ	0
#define TOF_DEFAULT_VHTACK	1

#define	TOF_DEFAULT_THRESHOLD_SCALE_20M	5
#define	TOF_DEFAULT_THRESHOLD_LOG2_20M	6
#define	TOF_DEFAULT_THRESHOLD_SCALE	3
#define	TOF_DEFAULT_THRESHOLD_LOG2	7

#define	TOF_INITIATOR_DT_MAX		-1
#define	TOF_INITIATOR_DT_MIN		-1
#define	TOF_TARGET_DT_MAX		-1
#define	TOF_TARGET_DT_MIN		-1

#define	TOF_RTT_MAXVAL		2000
#define	TOF_TS_RANGE		300

#define	TOF_SEQ_SHIFT		12
#define	TOF_SEQ_MASK		0xf000
#define	TOF_VHT_ACK_SHIFT	8
#define	TOF_CMD_MASK		0xff
#define	TOF_RSP_MASK		0xff
#define	TOF_TX			16
#define	TOF_RX			1
#define	TOF_RESET		0
#define	TOF_SUCCESS		0x88

/* All these parameters are related to CLK */
#define	TOF_SHIFT		15
#define	TOF_TICK_TO_NS(val, Tq)	(((uint64)(val)*(Tq)+ (1 << (TOF_SHIFT-1))) >> TOF_SHIFT)
#define	TOF_NS_PS		1000
/* Distance =  (C (speed of light) * Delta Time	(1 ns)) /2 = (3 * 10^8)* (10 ^ -9) /2
 * = 0.15 m
 * For 1 ns equals 0.15 meter, to keep the accuray, times 15 then divide by 100.
 * Distance expressed as Q4 number(i.e 16 => 1m)
 * We prefer to round down
*/
#define	TOF_NS_TO_16THMETER(val)	((((val)*15)<<4)/100)

#define	MAX_DEBUG_COUNT		32

typedef enum {
	C_FRAMETYPE_DOT11B	= 0,
	C_FRAMETYPE_DOT11A	= 1,
	C_FRAMETYPE_DOT11N	= 2,
	C_FRAMETYPE_DOT11AC	= 3
} eFrameTypeDefinitions;

#define	PDSVC_PAYLOAD_LEN(payloadp)	((payloadp)->len)
#define	PDSVC_PAYLOAD(payloadp)		((payloadp)->data)

#include<packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct {
	uint8		rev;		/* protocol extension revision */
	uint32		ftm_count;	/* number of ftm frames to send */
	uint16		retry_count;	/* target ftm retries */
	uint32		ftm_period;	/* delay form ACK for FTM[n-1] to tx of next FTM[n]  */
	uint16		ftm_rate;	/* legacy rates desired ftm dot11 rate for FTM's  */
	uint16		ftm_vht_rate;	/* vht rates */
	chanspec_t	chanspec;	/* desired ofdm bw is in there (20, 40, 80 MHz) */
	/* add more parameters as needed, schk must be the last one  */
	/*
	 * e.g.  to share proxd results with target AP,(via ftm_trigger stop frame)
	 * uint16 median rtd;
	 * uint16 avg rtd;
	 * uint16 std deviation;
	 */
	uint8		schk;		/*  sanity check signature  */
} BWL_POST_PACKED_STRUCT tof_rq_params_t;
typedef BWL_PRE_PACKED_STRUCT struct {
	uint8		rev;		/* protocol extension revision */
	bool		valid;		/* data is valid or not */
	uint8		frmcnt;		/* valid frame counts */
	uint8		reason;		/* reason code */
	uint32		distance;	/* distance in 1/16 meter */
	uint32		sdrtt;		/* standard deviation of RTT */
	int32		var3;		/* difference between gd and adj */
	uint8		schk;		/*  sanity check signature  */
} BWL_POST_PACKED_STRUCT tof_rqend_params_t;
#define	FTM_PARAMS_MAGIC	0x5a
#define	VS_IE_ID		0xDD

typedef BWL_PRE_PACKED_STRUCT struct {
	uint8	id;		/* IE ID: 0xDD */
	uint8	len;		/* IE length */
	uint8	OUI[3];		/* BROADCOM OUI  00:10:18 */
	union {
		tof_rq_params_t		rqparams;	/* request parameters */
		tof_rqend_params_t	rqendparams;	/* request end parameters. */
	} params;
} BWL_POST_PACKED_STRUCT proxd_ie_t;

/* initiator , AKA receiving station  */
#define	FTM_11V_EXT_MIN		8	/*  min sz of 11v IE expanded trig frame [cat+act+trig+id+len+oui[3]] */
typedef BWL_PRE_PACKED_STRUCT struct tof_trig_frm {
	uint8		category;	/* current method */
	uint8		action;		/* action field */
	uint8		trigger;	/* 1 - start, 0 - stop */
	proxd_ie_t	ftm_ie;		/* PROXD IE */
} BWL_POST_PACKED_STRUCT tof_trig_frm_t;

/* Declarations of static functions */
typedef	BWL_PRE_PACKED_STRUCT struct tof_measure {
	uint8	category;	/* category */
	uint8	action;		/* action */
	uint8	token;		/* dialog token	*/
	uint8	follow_token;	/* follow up token */
	uint8	TOD[6];		/* tx measurement frame timestamp */
	uint8	TOA[6];		/* rx ACK  timestamp */
	uint16	max_TOD;	/* Max TOD error */
	uint16	max_TOA;	/* Max TOA error */
} BWL_POST_PACKED_STRUCT tof_measure_frm_t;

#ifdef TOF_COLLECT
/* Unprotected Wireless Network Management (UWNM) action types */
#define	DOT11_UWNM_ACTION_TIMING_COLLECT	255

/* Declarations of static functions */
typedef	BWL_PRE_PACKED_STRUCT struct tof_collect {
	uint8	category;	/* category */
	uint8	action;		/* action */
	uint8	type;		/* packet type */
	uint8	index;		/* data index: 0 is header, others is data */
	uint16	length;		/* data length */
	uint8	data[1];	/* collect data */
} BWL_POST_PACKED_STRUCT tof_collect_frm_t;
#endif /* TOF_COLLECT */
#include<packed_section_end.h>

struct wlc_pdtof_sm;
typedef	struct wlc_pdtof_sm		pdtof_sm_t;

typedef	wl_proxd_params_rssi_method_t	rssi_cfg_t;

typedef struct tofts {
	uint32	t1;
	uint32	t2;
	uint32	t3;
	uint32	t4;
	int32	gd;
	int32	adj;
	uint32	rspec;
	int8	rssi;
	bool	discard;
} tofts_t;

typedef	struct tof_tslist {
	uint16	tscnt;
	tofts_t	*tslist;
} tof_tslist_t;

/* TOF method data object */
typedef	struct pdsvc_tofobj	{
	pdmthd_if_t	tofif;
	/* callbacks to	service	tx and notify functions	*/
	pdsvc_funcs_t			*svc_funcs;
	wlc_bsscfg_t			*bsscfg;
	uint32				signature;
	wlc_info_t			*wlc;
	pdtof_sm_t			*sm;
	uint16				txcnt;
	uint16				rxcnt;
	uint16				measurecnt;
	uint16				frame_type_cnt[FRAME_TYPE_NUM];
	uint16				adj_type_cnt[TOF_ADJ_TYPE_NUM];
	uint32				dbgstatus;
	bool				scanestarted;
	bool				smstoped;
	struct wl_timer			*timer;
	wlc_hrt_to_t			*duration_timer;
	wlc_hrt_to_t			*ftm_tx_timer;
	bool				ftm_tx_timer_active;
	uint32				ftm_period;
	bool				duration_timer_active;
	bool				timeractive;
	tof_tslist_t			tof_tslist;
	uint64				tx_t1;
	uint64				tx_t4;
	pdsvc_payload_t			*payloadp;
	int32				distance;
	uint32				meanrtt;
	uint32				modertt;
	uint32				medianrtt;
	uint32				sdrtt;
	int8				clb_data_2g[2];
	int8				clb_data_5g[12];
	bool				apply_rssi_clb;
	int8				avgrssi;
	uint16				tofcmd;
	uint64				Tq;
	uint16				Q;
	uint8				phyver;
	uint8				frmcnt;
	uint32				oldavbrx;
	uint32				oldavbtx;
	uint				chipnum;
	uint				chiprev;
	uint16				shmemptr;
	struct ether_addr		allow_mac;
	bool				caldone;
	int32				var3;
	struct ether_addr		initiatormac;
	wl_proxd_params_tof_method_t	*paramp;
	wl_proxd_params_tof_tune_t	*tunep;
#ifdef TOF_COLLECT
	bool				remote_request;
	bool				remote_collect;
	int16				remote_cnt;
	int16				collect_cnt;
	int16				collect_size;
	wl_proxd_collect_header_t	*collect_header;
	wl_proxd_collect_data_t 	*collect;
	int16				debug_cnt;
	wl_proxd_debug_data_t		*debug_info;
#endif /* TOF_COLLECT */
} pdtof_obj_t;

/* RSSI Proximity state machine parameters */
struct wlc_pdtof_sm	{
	uint8			tof_mode;
	uint8			tof_txcnt;
	uint8			tof_rxcnt;
	uint8			tof_state;
	struct ether_addr	tof_peerea;
	struct ether_addr	tof_selfea;
	pdtof_obj_t		*tof_obj;
	uint8			tof_reason;
	uint8			tof_legacypeer;
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
};

enum tof_type {
	TOF_TYPE_REQ_END		= 0,
	TOF_TYPE_REQ_START		= 1,
	TOF_TYPE_MEASURE_END		= 2,
	TOF_TYPE_MEASURE		= 3,
#ifdef TOF_COLLECT
	TOF_TYPE_COLLECT_REQ_END	= 4,
	TOF_TYPE_COLLECT_REQ_START	= 5,
	TOF_TYPE_COLLECT_DATA_END	= 6,
	TOF_TYPE_COLLECT_DATA		= 7,
#endif /* TOF_COLLECT */
	TOF_TYPE_LAST			= 8
};

enum tof_event {
	TOF_EVENT_WAKEUP	= 0,
	TOF_EVENT_RXACT		= 1,
	TOF_EVENT_TMO		= 2,
	TOF_EVENT_ACKED		= 3,
	TOF_EVENT_NOACK		= 4,
	TOF_EVENT_WAITMTMO	= 5,
#ifdef TOF_COLLECT
	TOF_EVENT_COLLECT_REQ	= 6,
#endif /* TOF_COLLECT */
	TOF_EVENT_LAST		= 7
};

enum tof_ret {
	TOF_RET_SLEEP	= 0,
	TOF_RET_ALIVE	= 1,
	TOF_RET_IGNORE	= 2
};

typedef struct wlc_pdtof_data {
	uint8			tof_type;
	int8			tof_rssi;
	uint32			tof_rspec;
	struct ether_addr	tof_srcea;
	struct ether_addr	tof_dstea;
} wlc_pdtof_data_t;

#ifdef TOF_DEBUG_TIME2
static uint32 tsf_start, tsf_hi, tsf_scanstart,	tsf_txreq, tsf_rxack, tsf_rxm, tsf_tmo,	tsf_lastm;
#endif

static void tof_ftm_req_init(pdtof_obj_t* tofobj, tof_trig_frm_t *tof_hdr, uint8 type);
static int wlc_pdtof_send(pdtof_sm_t *sm, struct ether_addr	*da, uint8 type);

static int wlc_pdtof_sm(pdtof_sm_t *sm,	int	event, uint8 *param, int paramlen,
	wlc_pdtof_data_t *datap);
static void wlc_pdtof_measure(pdtof_obj_t *tofobj, int cmd);
#ifdef TOF_COLLECT
static int wlc_pdtof_collection(pdmthd_if_t	*svcif,	wl_proxd_collect_query_t *query,
	void *buff, int len, uint16 *reqLen);
#endif /* TOF_COLLECT */
static int wlc_pdtof_confirmed(pdtof_sm_t *sm, uint8 reason);

/*
# -- two nvram.txt variables used for rssi reading adjustment --
2ghz :(ofdm, cck)
rssicorrnorm_c0=0,-3
5ghz 4 subbands:, {b1(20,40,80), ...b4()} cal values per each subband U-NII-x
rssicorrnorm5g_c0=0,1,-1,0,0,-2,-1,-1,-3,-2,-1,-3
*/
#ifdef TOF_DEBUG
#define	CHDBG(x)	printf	x
#else
#define	CHDBG(x)
#endif
#define TOF_PRINTF(x)	printf x

#define core0_k_tof_H_bits	10
#define chnsm_k_tof_H_bits	24

static int chan2band5g_idx(uint8 chan);
static int init_rssi_clb_data(pdtof_obj_t *tofobj);
static int adjust_rssi(pdtof_obj_t *tofobj);

#if defined(TOF_DEBUG_FTM)
#if defined(linux) && defined(__KERNEL__)
static uint32 get_usts(wlc_info_t *wlc)
{
	static struct timeval ts0;
	struct timeval ts1;
	static uint64 ts64_0 = 0, ts64_1 = 0;
	static bool  onetime = TRUE;
	if (onetime) {
		do_gettimeofday(&ts0);
		ts64_0 = ts0.tv_sec * 1000000 + ts0.tv_usec;
		ts64_1 = ts64_0;
		onetime = FALSE;
	} else {
		do_gettimeofday(&ts1);
		ts64_1 = ts1.tv_sec * 1000000 + ts1.tv_usec;
	}
	return (uint32)(ts64_1 - ts64_0);
}
#else
static uint32 get_usts(wlc_info_t *wlc)
{
	uint32 tsval, tsf_hi;
	wlc_read_tsf(wlc, &tsval, &tsf_hi);
	return tsval;
}
#endif /* linux */
#endif /* TOF_DEBUG_FTM */

/* get K value */
static uint32 wlc_pdtof_get_kval(pdtof_obj_t *tofobj, bool initiator)
{
	uint32 k;

	if (tofobj->sm->tof_legacypeer == TOF_LEGACY_AP)
	{
		if (initiator)
			initiator = FALSE;
		else
			return 0;
	}

	if (initiator) {
		k = tofobj->tunep->Ki;
		if (!k)
			wlc_phy_tof_kvalue(tofobj->wlc->band->pi, tofobj->paramp->chanspec,
				&k, NULL);
	}
	else {
		k = tofobj->tunep->Kt;
		if (!k)
			wlc_phy_tof_kvalue(tofobj->wlc->band->pi, tofobj->paramp->chanspec,
				NULL, &k);
	}
	return k;
}

/* get log2 and scale value */
static void wlc_pdtof_get_log2_scale(pdtof_obj_t *tofobj, int16 *log2p, int16 *scalep)
{
	int16 val;

	val = tofobj->tunep->N_log2;
	if (!val) {
		if (CHSPEC_IS80(tofobj->paramp->chanspec) || CHSPEC_IS40(tofobj->paramp->chanspec))
			val = TOF_DEFAULT_THRESHOLD_LOG2;
		else
			val = TOF_DEFAULT_THRESHOLD_LOG2_20M;
	}
	if (log2p)
		*log2p = val;

	val = tofobj->tunep->N_scale;
	if (!val) {
		if (CHSPEC_IS80(tofobj->paramp->chanspec) || CHSPEC_IS40(tofobj->paramp->chanspec))
			val = TOF_DEFAULT_THRESHOLD_SCALE;
		else
			val = TOF_DEFAULT_THRESHOLD_SCALE_20M;
	}
	if (scalep)
		*scalep = val;
}

/* Enable VHT ACK  */
static void wlc_pdtof_tof_init_vht(pdtof_obj_t *tofobj, int len_bytes)
{
	pdtof_sm_t *sm = tofobj->sm;
	chanspec_t chanspec = tofobj->paramp->chanspec;
	int	dot11_bw, dbps, vht_sig_a1, vht_sig_a2, vht_sig_b1;
	int	frame_len, ampdu_len, ampdu_len_wrds;
	int	n_sym, vht_length, vht_pad;
	int	n_ampdu_delim, n_ampdu_eof;

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

	n_sym = ((8 * ampdu_len + 22) + dbps - 1) / dbps;
	vht_length = (n_sym * dbps - 22) / 8;
	vht_pad = (vht_length > ampdu_len_wrds)? vht_length - ampdu_len_wrds : 0;

#ifdef TOF_DEBUG
	printf("LEN	%d => AMPDU	LEN	%d => NSYMS	%d => %d bytes\n",
		len_bytes, ampdu_len, n_sym, vht_length);
#endif
	n_ampdu_delim = vht_pad >> 2;
	n_ampdu_eof = vht_pad & 3;
#ifdef TOF_DEBUG
	printf("n_ampdu_delim =	%d,	n_ampdu_eof	= %d\n", n_ampdu_delim, n_ampdu_eof);
#endif
	if ((n_ampdu_delim == 0) && (ampdu_len & 3) && (vht_length > ampdu_len_wrds)) {
		int adj = 4 - (ampdu_len & 3);
		n_ampdu_eof += (adj & 3);
#ifdef TOF_DEBUG
		printf("Adjust:	adj	= %d, n_ampdu_eof =	%d\n", adj, n_ampdu_eof);
#endif
	}

	sm->phyctl0 = (dot11_bw << 14) | 7 | (1 << 6);
	sm->phyctl1 = 0;
	sm->phyctl2 = 0;
	sm->lsig = 0xb | (((n_sym + 5)*3 - 3) << 5);
	sm->vhta0 = (vht_sig_a1 & 0xffff);
	sm->vhta1 = ((vht_sig_a2 & 0xff) << 8) | ((vht_sig_a1 >> 16) & 0xff);
	sm->vhta2 = (vht_sig_a2 >> 8) & 0xffff;
	sm->vhtb0 = ((ampdu_len + 3)/4) & 0xffff;
	sm->vhtb1 = vht_sig_b1;
	sm->ampductl = (n_ampdu_eof << 8) | (1 << 1);
	sm->ampdudlim = n_ampdu_delim;
	sm->ampdulen = (frame_len << 4) | 1;
}

/* Setup VHT rate */
static void wlc_pdtof_tof_setup_vht(pdtof_obj_t *tofobj)
{
	pdtof_sm_t *sm = tofobj->sm;
	wlc_info_t *wlc	= tofobj->wlc;

	if (wlc_read_shm(tofobj->wlc, tofobj->shmemptr + M_TOF_PHYCTL0) != sm->phyctl0) {
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_PHYCTL0, sm->phyctl0);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_PHYCTL1, sm->phyctl1);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_PHYCTL2, sm->phyctl2);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_LSIG, sm->lsig);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_VHTA0, sm->vhta0);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_VHTA1, sm->vhta1);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_VHTA2, sm->vhta2);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_VHTB0, sm->vhtb0);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_VHTB1, sm->vhtb1);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_AMPDU_CTL, sm->ampductl);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_AMPDU_DLIM, sm->ampdudlim);
		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_AMPDU_LEN, sm->ampdulen);
	}
}

/* Called when entering/exiting tof measurement mode */
static void wlc_pdtof_hw(pdtof_obj_t *tofobj, bool enter)
{
	wlc_info_t *wlc = tofobj->wlc;

	wlc_phy_tof(wlc->band->pi, enter, tofobj->tunep->hw_adj);
}

/* group delay and adj calculation */
static int
wlc_pdtof_rtd_adj(pdtof_obj_t *tofobj, int frame_type, int frame_bw, bool sw_adj_en, bool hw_adj_en,
	int32 *gd, int32 *adj, bool initiator)
{
	wlc_info_t *wlc	= tofobj->wlc;
	uint32 *tbl_value;
	struct tof_rtd_adj_params params;
	int adj_err, nfft, ret;
	int16 log2, scale;
#ifdef RSSI_REFINE
	int32 *T = NULL;
#endif
#ifdef TOF_COLLECT
	wl_proxd_collect_data_t *collect = tofobj->collect;
	int n_out = 0;
#ifdef RSSI_REFINE
	int max, max_index = -1, peak_offset, new_index, scalar, imp_average = 0, t = 0;
#endif
#endif /* TOF_COLLECT */

	nfft = (NFFT_BASE << frame_bw); /* allocate max value */
#ifdef RSSI_REFINE
	tbl_value = (uint32 *)MALLOC(wlc->osh, 5 * nfft * sizeof(uint32));
#else
	tbl_value = (uint32 *)MALLOC(wlc->osh, 2 * nfft * sizeof(uint32));
#endif
	if (!tbl_value)
		return BCME_NOMEM;

	params.bw = (BANDWIDTH_BASE << frame_bw);
	wlc_pdtof_get_log2_scale(tofobj, &log2, &scale);
	params.thresh_log2 = log2;
	params.thresh_scale = scale;
	params.w_len = tofobj->tunep->w_len[frame_bw];
	params.w_offset = tofobj->tunep->w_offset[frame_bw];
	params.gd_ns = 0;
	params.adj_ns = 0;
	params.H = (int32*)tbl_value;
#ifdef RSSI_REFINE
	params.p_A = params.H + nfft;
	T = params.p_A + nfft;
#else
	params.p_A = NULL;
#endif
	if (params.w_len > nfft)
		params.w_len = nfft;

	*gd = *adj = 0;

	if (hw_adj_en) {
#ifdef TOF_COLLECT
		if (collect)
		adj_err = wlc_phy_chan_mag_sqr_impulse_response(wlc->band->pi, frame_type,
			params.w_len, params.w_offset, chnsm_k_tof_H_bits,
			params.H, (int *)&params.gd_ns, collect->H, tofobj->shmemptr);
		else
#endif
		adj_err = wlc_phy_chan_mag_sqr_impulse_response(wlc->band->pi, frame_type,
			params.w_len, params.w_offset, chnsm_k_tof_H_bits,
			params.H, (int *)&params.gd_ns, NULL, tofobj->shmemptr);

		if (adj_err == BCME_OK) {
#ifdef TOF_COLLECT
			n_out = params.w_len;
#endif
		} else {
			hw_adj_en = FALSE;
		}
	}

	if (!hw_adj_en && sw_adj_en) {
#ifdef TOF_COLLECT
		if (collect)
			adj_err = wlc_phy_chan_freq_response(wlc->band->pi, nfft,
				core0_k_tof_H_bits, params.H, NULL, collect->H);
		else
#endif
		adj_err = wlc_phy_chan_freq_response(wlc->band->pi, nfft,
			core0_k_tof_H_bits, params.H, NULL, NULL);

		if (adj_err == BCME_OK) {
#ifdef TOF_COLLECT
			n_out = nfft;
#endif
		} else {
			sw_adj_en = FALSE;
		}
	}

	wlc_pdtof_measure(tofobj, initiator? TOF_RX : TOF_RESET);

	if (hw_adj_en) {
		tofobj->adj_type_cnt[1]++;
		params.w_ext = params.H;
	} else if (sw_adj_en) {
		tofobj->adj_type_cnt[0]++;
		params.w_ext = NULL;
	}

	if (hw_adj_en || sw_adj_en) {
		if ((ret = tof_rtd_adj(wlc, &params)) != BCME_OK)
		{
#ifdef RSSI_REFINE
			MFREE(wlc->osh, tbl_value, 5 * nfft * sizeof(uint32));
#else
			MFREE(wlc->osh, tbl_value, 2 * nfft * sizeof(uint32));
#endif
			return ret;
		}
	}
	*gd = params.gd_ns;
	*adj = params.adj_ns;

#ifdef TOF_COLLECT
	if (collect) {
		collect->nfft =  n_out;
		collect->type = (hw_adj_en ? 1 : 0);
		collect->gd_adj_ns = *gd;
		collect->gd_h_adj_ns = *adj;
#ifdef RSSI_REFINE
		/* find max */
		max = -1;
		for (i = 0; i < nfft; i++) {
			if (params.p_A[i] > max) {
				max = params.p_A[i];
				max_index = i;
			}
		}

		/* center impulse response at 31 (roll), scale each variable */
		peak_offset = (nfft/2)-max_index;
		scalar = (1<<TOF_MAXSCALE)/max;
		for (i = 0; i < nfft; i++) {
			new_index = (i - 1 + peak_offset + nfft) % nfft;
			T[new_index] = params.p_A[i];
			imp_average += T[new_index]; /* calculate sum of impulse response */
			T[new_index] *= scalar; /* scale */
			/* check C code rolled properly */
			collect->rssi_bias.imp_resp[new_index] = T[new_index];
		}

		collect->rssi_bias.threshold[1] = find_crossing(T, max, nfft, 2);
		collect->rssi_bias.threshold[3] = find_crossing(T, max, nfft, 3);
		collect->rssi_bias.threshold[5] = find_crossing(T, max, nfft, 4);
		collect->rssi_bias.threshold[7] = find_crossing(T, max, nfft, 5);
		collect->rssi_bias.threshold[9] = find_crossing(T, max, nfft, 6);

		/* reverse array so now center of impulse response is at 32 */
		for (i = 0; i < nfft/2; i++) {
			t = T[i];
			T[i] = T[nfft - i - 1];
			T[nfft-i-1] = t;
		}

		collect->rssi_bias.version = 1;
		collect->rssi_bias.peak_offset = (peak_offset+nfft)%nfft;
		collect->rssi_bias.threshold[0] = find_crossing(T, max, nfft, 2);
		collect->rssi_bias.threshold[2] = find_crossing(T, max, nfft, 3);
		collect->rssi_bias.threshold[4] = find_crossing(T, max, nfft, 4);
		collect->rssi_bias.threshold[6] = find_crossing(T, max, nfft, 5);
		collect->rssi_bias.threshold[8] = find_crossing(T, max, nfft, 6);

		collect->rssi_bias.threshold[10] = scalar;
		/* peak to average */
		collect->rssi_bias.bias = (max >> 4) * 100 /((imp_average >> 4) / nfft);
#endif /* RSSI_REFINE */
	}
#endif /* TOF_COLLECT */
#ifdef RSSI_REFINE
	MFREE(wlc->osh, tbl_value, 5 * nfft * sizeof(uint32));
#else
	MFREE(wlc->osh, tbl_value, 2 * nfft * sizeof(uint32));
#endif
	return BCME_OK;
}

/* get the 5g index into clb_data_5g[] array by channel */
static int chan2band5g_idx(uint8 chan)
{
	if ((chan >= 36) && (chan <= 48)) {
		/* U_NII-1 */
		return 0;
	} else if ((chan >= 52) && (chan <= 64)) {
		/* U_NII-2 */
		return 3;
	} else if ((chan >= 100) && (chan <= 140)) {
		/* U_NII-2e	*/
		return 6;
	} else if ((chan >= 149) && (chan <= 165)) {
		/* U_NII-3 */
		return 9;
	} else {
		WL_ERROR(("channel is not UNII !\n"));
		return 0; /* invalid 5ghz chan */
	}
}

/* get CLB data from nvram */
static int init_rssi_clb_data(pdtof_obj_t *tofobj)
{
	int i;
	wlc_info_t *wlc = tofobj->wlc;
	char *clb_var2g = getvar(wlc->pub->vars, "rssicorrnorm_c0");
	char *clb_var5g = getvar(wlc->pub->vars, "rssicorrnorm5g_c0");

	if ((!clb_var2g) || (!clb_var5g)) {
		WL_ERROR(("warning: nvram RSSI calib data is not found\n"));
		tofobj->apply_rssi_clb = FALSE;
		return -1;
	}

	tofobj->clb_data_2g[0] = atoi(bcmstrtok(&clb_var2g, ",", NULL));
	tofobj->clb_data_2g[1] = atoi(bcmstrtok(&clb_var2g, ",", NULL));

	CHDBG(("clb_data_2g[0]=%d\n", tofobj->clb_data_2g[0]));
	CHDBG(("clb_data_2g[1]=%d\n", tofobj->clb_data_2g[1]));

	for	(i = 0; i < 12; i++) {
		tofobj->clb_data_5g[i] = atoi(bcmstrtok(&clb_var5g, ",", NULL));
		CHDBG(("clb_data_5g[%d]=%d\n", i, tofobj->clb_data_5g[i]));
	}
	/* to disable rssi adjustment , comment the nvram vars in nvram.txt */
	tofobj->apply_rssi_clb = TRUE;
	return 0;
}

/* adjust rssi reading accoeding too claibr table from nvram.txt */
static int adjust_rssi(pdtof_obj_t *tofobj)
{
	chanspec_t chspec = tofobj->paramp->chanspec;
	uint8 chan = CHSPEC_CHANNEL(chspec);
	uint16 bw = CHSPEC_BW(chspec);
	int UNII_idx;
	int8 rssi_adj;

	if (!tofobj->apply_rssi_clb)
		return 0;

	if (CHSPEC_IS5G(chspec)) {

		/* turn	bw into	an offset within UNii tripplet */
		if (bw == WL_CHANSPEC_BW_20) {
			bw = 0;
		} else if (bw == WL_CHANSPEC_BW_40) {
			bw = 1;
		} else if (bw == WL_CHANSPEC_BW_80) {
			bw = 2;
		} else {
			WL_ERROR(("unhandled chanspec BW, use 20\n"));
			bw = 0;
		}

		UNII_idx = chan2band5g_idx(chan);
		CHDBG(("chan is 5g, UNII_idx:%d, bw_idx:%d\n", UNII_idx, bw));

		rssi_adj = tofobj->clb_data_5g[UNII_idx + bw];
	} else if (CHSPEC_IS2G(chspec)) {
		rssi_adj = tofobj->clb_data_2g[0]; /* ofdm only	*/
		CHDBG(("chan is	2g\n"));
	} else {
		WL_ERROR(("Uknown chanspec\n"));
		return 0;
	}
	CHDBG(("rssi_adj_val:%d\n", rssi_adj));
	return rssi_adj;
}

/* store 6 bytes value */
static void htol48_ua_store(uint64 val, uint8 *bytes)
{
	bytes[0] = val & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = (val >> 16) & 0xff;
	bytes[3] = (val >> 24) & 0xff;
	bytes[4] = (val >> 32) & 0xff;
	bytes[5] = (val >> 48) & 0xff;
}

/* convert avb time stamp */
static uint32 wlc_pdtof_ts_value(uint8 *ts)
{
	return (uint32)ts[0] + ((uint32)ts[1] << 8) +
		((uint32)ts[2] << 16) + ((uint32)ts[3] << 24);
}

/* transmit command to ucode */
static bool wlc_pdtof_cmd(wlc_info_t *wlc, uint shmemptr, uint16 cmd)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	d11regs_t *regs = wlc_hw->regs;
	int i = 0;

	/* Wait until last command completes */
	while ((R_REG(wlc_hw->osh, &regs->maccommand) & MCMD_TOF) && (i < 400)) {
		OSL_DELAY(1);
		i++;
	}

	if (R_REG(wlc_hw->osh, &regs->maccommand) & MCMD_TOF) {
		WL_ERROR(("TOF ucode cmd timeout\n"));
		return FALSE;
	}

	wlc_write_shm(wlc, shmemptr + M_TOF_CMD, cmd);

	OR_REG(wlc->osh, &wlc->regs->maccommand, MCMD_TOF);

	return TRUE;
}

/* send measurement cmd to ucode */
static void wlc_pdtof_measure(pdtof_obj_t *tofobj, int cmd)
{
	wlc_info_t *wlc = tofobj->wlc;
	uint16 tof_cmd = TOF_RESET;

	tofobj->tofcmd = 0xdead;
	if (wlc_pdtof_cmd(wlc, tofobj->shmemptr, tof_cmd)) {
		wlc_phy_tof_cmd(wlc->band->pi, FALSE);
		if (cmd == TOF_RX) {
			tof_cmd = TOF_RX;
			if (tofobj->tunep->vhtack)
				tof_cmd |= (1 << TOF_VHT_ACK_SHIFT);
			if (wlc_pdtof_cmd(wlc, tofobj->shmemptr, tof_cmd))
				tofobj->tofcmd = tof_cmd;
		}
	}
	else
		wlc_phy_tof_cmd(wlc->band->pi, FALSE);
}

/* Get AVB time stamp */
static void wlc_pdtof_avbtime(pdtof_obj_t *tofobj, uint32 *tx, uint32 *rx)
{
	wlc_info_t *wlc = tofobj->wlc;
#ifdef TOF_COLLECT
	wl_proxd_collect_data_t	*collect = tofobj->collect;
	wl_proxd_debug_data_t *debug = (tofobj->debug_cnt > 0 &&
		tofobj->debug_cnt <= MAX_DEBUG_COUNT)?
		&tofobj->debug_info[tofobj->debug_cnt - 1] : NULL;
#endif /* TOF_COLLECT */

	wlc_get_avb_timestamp(wlc->hw, tx, rx);

#ifdef TOF_COLLECT
	if (collect) {
		collect->tof_avb_rxl = (uint16)(*rx & 0xffff);
		collect->tof_avb_rxh = (uint16)((*rx >> 16) & 0xffff);
		collect->tof_avb_txl = (uint16)(*tx & 0xffff);
		collect->tof_avb_txh = (uint16)((*tx >> 16) & 0xffff);

		if (debug) {
			debug->tof_avb_rxl = collect->tof_avb_rxl;
			debug->tof_avb_rxh = collect->tof_avb_rxh;
			debug->tof_avb_txl = collect->tof_avb_txl;
			debug->tof_avb_txh = collect->tof_avb_txh;
		}
	}
#endif /* TOF_COLLECT */
}

/* Get measurement results */
static int wlc_pdtof_measure_results(pdtof_obj_t *tofobj, uint64 *tx, uint64 *rx,
	int32 *gd, int32 *adj, uint16 id, bool acked, int8 *rssip, bool *discard, bool initiator)
{
	wlc_info_t *wlc = tofobj->wlc;
	uint16 rspcmd, i;
	uint32 avbrx, avbtx;
	int32 h_adj = 0;
#ifdef TOF_COLLECT
	wl_proxd_collect_data_t *collect = tofobj->collect;
	wl_proxd_debug_data_t *debug = (tofobj->debug_cnt > 0 &&
		tofobj->debug_cnt <= MAX_DEBUG_COUNT)?
		&tofobj->debug_info[tofobj->debug_cnt - 1] : NULL;
#endif /* TOF_COLLECT */

	for (i = 0; i < 200; i++) {
		rspcmd = wlc_read_shm(wlc, tofobj->shmemptr + M_TOF_RSP);
		if (((rspcmd & TOF_RSP_MASK) == TOF_SUCCESS) || !acked)
			break;
		OSL_DELAY(1);
	}

#ifdef TOF_COLLECT
	if (collect) {
		collect->tof_cmd = tofobj->tofcmd;
		collect->tof_rsp = rspcmd;
		collect->tof_id = id;
		collect->tof_status0 = 0;
		collect->tof_status2 = 0;
		collect->tof_chsm0 = wlc_read_shm(wlc, tofobj->shmemptr + M_TOF_CHNSM_0);
		collect->nfft = 0;

		if (debug) {
			debug->tof_cmd = tofobj->tofcmd;
			debug->tof_rsp = rspcmd;
			debug->tof_id = collect->tof_id;
			debug->tof_status0 = collect->tof_status0;
			debug->tof_status2 = collect->tof_status2;
			debug->tof_chsm0 = collect->tof_chsm0;

			debug->tof_phyctl0 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_PHYCTL0);
			debug->tof_phyctl1 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_PHYCTL1);
			debug->tof_phyctl2 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_PHYCTL2);
			debug->tof_lsig	= wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_LSIG);
			debug->tof_vhta0 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_VHTA0);
			debug->tof_vhta1 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_VHTA1);
			debug->tof_vhta2 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_VHTA2);
			debug->tof_vhtb0 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_VHTB0);
			debug->tof_vhtb1 = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_VHTB1);
			debug->tof_apmductl = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_AMPDU_CTL);
			debug->tof_apmdudlim = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_AMPDU_DLIM);
			debug->tof_apmdulen = wlc_read_shm(tofobj->wlc,
				tofobj->shmemptr + M_TOF_AMPDU_LEN);
		}
	}
#endif /* TOF_COLLECT */

	wlc_pdtof_avbtime(tofobj, &avbtx, &avbrx);

	if ((rspcmd & TOF_RSP_MASK) == TOF_SUCCESS) {
		uint32 delta;
		int frame_type, frame_bw, expected_frame_bw;
		bool hw_adj, sw_adj;

		if (avbrx == tofobj->oldavbrx && avbtx == tofobj->oldavbtx) {
			uint32 clkst, macctrl1;
			wlc_get_avb_timer_reg(wlc->hw, &clkst, &macctrl1);
			wlc_enable_avb_timer(wlc->hw, TRUE);
			printf("Clkst %x Macctrl1 %x Restart AVB timer\n", clkst, macctrl1);
		} else {
			tofobj->oldavbrx = avbrx;
			tofobj->oldavbtx = avbtx;
		}

		if (wlc_phy_tof_info(wlc->band->pi, &frame_type, &frame_bw, rssip) == BCME_OK) {

		tofobj->frame_type_cnt[frame_type]++;
			if (CHSPEC_IS80(tofobj->paramp->chanspec))
				expected_frame_bw = TOF_BW_80MHZ_INDEX;
			else if (CHSPEC_IS40(tofobj->paramp->chanspec))
				expected_frame_bw = TOF_BW_40MHZ_INDEX;
			else
				expected_frame_bw = TOF_BW_20MHZ_INDEX;

			if (WLCISACPHY(wlc->band) && (tofobj->chipnum == BCM4360_CHIP_ID ||
					tofobj->chipnum == BCM43460_CHIP_ID) ) {
				frame_bw = expected_frame_bw;
			}
		} else {
			frame_type = -1;
				frame_bw = -1;
			expected_frame_bw = 0;
		}

		if ((frame_type < 1) || (frame_bw != expected_frame_bw)) {
			TOF_PRINTF(("id 0x%x frame_type %d frame_bw %d expected_frame_bw %d\n",
				id, frame_type, frame_bw, expected_frame_bw));
			if (discard)
				*discard = TRUE;
			wlc_pdtof_measure(tofobj, initiator? TOF_RX : TOF_RESET);
			return BCME_ERROR;
		}

#ifdef TOF_DEBUG
		printf("AVB(%d): tx	%u rx %u id	%x\n", i, avbtx, avbrx, id);
#endif

		if (acked && tofobj->tunep->gdadj) {
			sw_adj = tofobj->tunep->sw_adj;
			hw_adj = tofobj->tunep->hw_adj;
		} else {
			sw_adj = FALSE;
			hw_adj = FALSE;
		}

		if (hw_adj || sw_adj) {
			int ret;
			ret = wlc_pdtof_rtd_adj(tofobj, frame_type, frame_bw, sw_adj,
				hw_adj, gd, adj, initiator);
			if (ret)
				return ret;
		} else {
			*gd = *adj = 0;
			wlc_pdtof_measure(tofobj, initiator? TOF_RX : TOF_RESET);
		}
		h_adj = *adj;

		if (initiator) {
			/* avbrx = t2, avbtx = t3, t3 should be bigger than t2 */
			if (avbtx < avbrx) {
				/* Timestamp overwraped */
				avbrx = 0xFFFFFFFF - avbrx;
				avbtx += avbrx;
				avbrx = 0;
			}

			delta = TOF_TICK_TO_NS(avbtx-avbrx, tofobj->Tq);
			*rx = TOF_TICK_TO_NS(avbrx, tofobj->Tq);
			*tx = *rx + delta;
			*rx += h_adj;
		} else {
			/* avbtx = t1, avbrx = t4, t4 should be bigger */
			uint32 kt = wlc_pdtof_get_kval(tofobj, FALSE);
			if (avbrx < avbtx) {
				/* Timestamp overwraped	*/
				avbtx = 0xFFFFFFFF - avbtx;
				avbrx += avbtx;
				avbtx = 0;
			}
			/* T4 = T1(avb timer) + Delta(avbrx-avbtx) + h_adj - K (target k value) */
			/* Tq is a shift factor to keep avb timer integer calculation accuracy */
			delta = TOF_TICK_TO_NS(avbrx-avbtx, tofobj->Tq);
			*tx = TOF_TICK_TO_NS(avbtx, tofobj->Tq);
			if (delta + h_adj > kt)
				*rx = *tx + delta + h_adj - kt;
			else {
				*rx = *tx;
				WL_ERROR(("K %d is bigger than delta %d\n", kt,
					delta + h_adj));
			}
		}

		if (discard) {
			if (tofobj->tunep->minDT > 0 && delta < tofobj->tunep->minDT)
				*discard = TRUE;
			else if (tofobj->tunep->maxDT > 0 && delta > tofobj->tunep->maxDT)
				*discard = TRUE;
		}

		return BCME_OK;
	} else {
		wlc_pdtof_measure(tofobj, initiator? TOF_RX : TOF_RESET);

#ifdef TOF_DEBUG
		printf("AVB Failed id %d rspcmd %x tofcmd %x\n", id, rspcmd, tofobj->tofcmd);
#endif /* TOF_DEBUG	*/
	}
	return BCME_ERROR;
}

/* reset TOF state */
static void wlc_pdtof_reset(pdtof_sm_t *sm, uint8 mode, uint8 reason)
{
	pdtof_obj_t *tofobj = sm->tof_obj;

	if (reason != TOF_REASON_INITIAL) {
		wlc_pdtof_measure(tofobj, TOF_RESET);
		wlc_pdtof_hw(tofobj, FALSE);
	}

	sm->tof_mode = mode;
	sm->tof_state = TOF_STATE_IDLE;
	sm->tof_txcnt = 0;
	sm->tof_rxcnt = 0;
	sm->tof_reason = reason;
	sm->tof_legacypeer = TOF_LEGACY_UNKNOWN;
	sm->tof_txvht = FALSE;
	sm->tof_rxvht = FALSE;
	if (reason != TOF_REASON_NOACK && reason != TOF_REASON_TIMEOUT)
		sm->tof_retrycnt = 0;
	if (mode == WL_PROXD_MODE_INITIATOR) {
		bcopy(&tofobj->paramp->tgt_mac, &sm->tof_peerea, ETHER_ADDR_LEN);
		if (tofobj->timeractive) {
			tofobj->timeractive = FALSE;
			wl_del_timer(tofobj->wlc->wl, tofobj->timer);
		}
		tofobj->tof_tslist.tscnt = 0;
		tofobj->frmcnt = 0;
	} else {
		if (tofobj->ftm_tx_timer_active) {
			wlc_hrt_del_timeout(tofobj->ftm_tx_timer);
			tofobj->ftm_tx_timer_active = FALSE;
		}
		bcopy(&ether_null, &sm->tof_peerea, ETHER_ADDR_LEN);
	}
}

/* We are on proximity detection channel now. */
static void
wlc_pdtof_excursion_prep_done(wlc_info_t *wlc, void *arg, uint *dwell)
{
	pdtof_obj_t *tofobj = (pdtof_obj_t *)arg;

	ASSERT(wlc != NULL);
	ASSERT(tofobj != NULL);

	if (tofobj->smstoped)
		return;

#ifdef TOF_DEBUG_TIME2
	wlc_read_tsf(tofobj->wlc, &tsf_scanstart, &tsf_hi);
	tsf_scanstart -= tsf_start;
#endif

	/* Initiate	find here */
#ifdef TOF_COLLECT
	if (tofobj->remote_request)
		(void) wlc_pdtof_sm(tofobj->sm, TOF_EVENT_COLLECT_REQ, NULL, 0, NULL);
	else
#endif /* TOF_COLLECT */
	(void) wlc_pdtof_sm(tofobj->sm, TOF_EVENT_WAKEUP, NULL, 0, NULL);
}

/* scan engine is done */
static void
wlc_pdtof_excursion_complete(void *ctx,	int status, wlc_bsscfg_t *cfg)
{
	pdtof_obj_t *tofobj = (pdtof_obj_t *)ctx;

	ASSERT(tofobj != NULL);
	ASSERT(cfg != NULL);
	ASSERT(tofobj->wlc != NULL);

	/* proximity duration time expired */
	if (tofobj->scanestarted) {
		tofobj->scanestarted = FALSE;
	}

	if (tofobj->smstoped)
		return;

#ifdef TOF_DEBUG
	WL_PRINT(("Tmo State=%d	Tx=%d Rx=%d	Ttx=%d Trx=%d Reason=%d\n",
		tofobj->sm->tof_state, tofobj->sm->tof_txcnt, tofobj->sm->tof_rxcnt,
		tofobj->txcnt, tofobj->rxcnt, tofobj->sm->tof_reason));
#endif
	ASSERT(tofobj->paramp->interval >= tofobj->paramp->duration);

#ifdef TOF_DEBUG_TIME2
	wlc_read_tsf(tofobj->wlc, &tsf_tmo, &tsf_hi);
	tsf_tmo -= tsf_start;
#endif

	if (tofobj->sm->tof_state != TOF_STATE_ICONFIRM)
		wlc_pdtof_sm(tofobj->sm, TOF_EVENT_TMO, NULL, 0, NULL);
}

/* TOF stay power on using bsscfg state flag to stop mpc */
static void wlc_pdtof_pwron(wlc_bsscfg_t *cfg, bool up)
{
	if (cfg && cfg->assoc) {
		if (up)
			cfg->assoc->state = AS_SCAN;
		else
			cfg->assoc->state = AS_IDLE;
	}
}

/* TOF timeout function */
static void
wlc_pdtof_duration_expired_cb(void *ctx)
{
	pdtof_obj_t *tofobj = (pdtof_obj_t *)ctx;
	wlc_bsscfg_t *cfg;

	ASSERT(tofobj != NULL);

	cfg = tofobj->bsscfg;

	wlc_hrt_del_timeout(tofobj->duration_timer);
	wlc_gptimer_wake_upd(tofobj->wlc, WLC_GPTIMER_AWAKE_PROXD, FALSE);
	tofobj->duration_timer_active = FALSE;
	if (!(tofobj->tunep->flags & WL_PROXD_FLAG_NOCHANSWT)) {
		wlc_pdtof_pwron(tofobj->bsscfg, FALSE);
	}
	wlc_pdtof_excursion_complete(ctx, 0, cfg);
}

/* abort the scan engine */
static void
wlc_pdtof_excursion_abort(pdtof_obj_t* tofobj)
{
	ASSERT(tofobj != NULL);

	/* stop scan engine if it is running */
	if (tofobj->scanestarted) {
		wlc_scan_abort_ex(tofobj->wlc->scan, tofobj->bsscfg, WLC_E_STATUS_ABORT);
		tofobj->scanestarted = FALSE;
	}

	/* stop HRT duration timer if it is running */
	if (tofobj->duration_timer_active) {
		wlc_hrt_del_timeout(tofobj->duration_timer);
		wlc_gptimer_wake_upd(tofobj->wlc, WLC_GPTIMER_AWAKE_PROXD, FALSE);
		tofobj->duration_timer_active =	FALSE;
	}
}

/* start the scan engine to begin TOF */
static int
wlc_pdtof_excursion_start(pdtof_obj_t *tofobj, chanspec_t chanspec, uint16 duration)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *cfg;
	struct ether_addr bssid;
	wlc_ssid_t ssid;
	int err;

	ASSERT(tofobj != NULL);
	ASSERT(tofobj->wlc != NULL);
	ASSERT(tofobj->bsscfg != NULL);

	wlc = tofobj->wlc;
	cfg = tofobj->bsscfg;

	/* abort excursion in case it is overlapping to	on-going duration.
	 * it is not expected to happen though.
	*/
	wlc_pdtof_excursion_abort(tofobj);


	/* check if the proxd chanspec is the same with the current channel.
	   use scan engine only in case the proxd needs to go off-channel excursion.
	*/
	if (chanspec == WLC_BAND_PI_RADIO_CHANSPEC ||
		(tofobj->tunep->flags & WL_PROXD_FLAG_NOCHANSWT)) {

		if (!(tofobj->tunep->flags & WL_PROXD_FLAG_NOCHANSWT)) {
			wlc_pdtof_pwron(tofobj->bsscfg, TRUE);
			wlc_radio_mpc_upd(tofobj->wlc);
		}

		/* start hrt timer for duration	*/
		wlc_hrt_add_timeout(tofobj->duration_timer, tofobj->paramp->duration * 1000,
			wlc_pdtof_duration_expired_cb, (void *)tofobj);

		tofobj->duration_timer_active =	TRUE;

		wlc_gptimer_wake_upd(wlc, WLC_GPTIMER_AWAKE_PROXD, TRUE);

		wlc_pdtof_excursion_prep_done(wlc, (void *)tofobj, NULL);

	} else {
		/* match our BSSID */
		bcopy(&cfg->BSSID, &bssid, ETHER_ADDR_LEN);

		/* We don't care about SSID */
		ssid.SSID_len = 0;

		/* initiate an excursion */
		/* scan request parameters
			wlc,
			bss_type = DOT11_BSSTYPE_ANY,
			bssid =	&bssid,
			nssid =	1,
			ssids =	&ssid,
			scan_type =	-1,	// Shouldn't be	DOT11_SCANTYPE_PASSIVE
					// to get actcb_fn_t callback
			nprobes	= 1,	// Shoud be	1 otherwise duration is split
					// with	number of nprobes
			active_time	= duration,
			passive_time = 0,
			home_time =	0,
			chanspec_list =	&chanspec,
			chanspec_num = 1,
			chanspec_start = 0,
			save_prb = FALSE,
			scancb_fn_t	fn = wlc_pdtof_excursion_complete,
			arg	= tofobj,
			macreq = WLC_ACTION_ACTFRAME,
			scan_flags = WL_SCANFLAGS_SWTCHAN,
			bsscfg = cfg,
			actcb_fn_t act_cb =	wlc_pdtof_excursion_prep_done,
			act_cb_arg = tofobj
*/
		err = wlc_scan_request_ex(wlc, DOT11_BSSTYPE_ANY, &bssid, 1, &ssid,
			-1, 1, duration, 0, 0,
			&chanspec, 1, 0, FALSE,	wlc_pdtof_excursion_complete, tofobj,
			WLC_ACTION_ACTFRAME, WL_SCANFLAGS_SWTCHAN, cfg,
			wlc_pdtof_excursion_prep_done, tofobj);

		if (err != BCME_OK) {
			WL_ERROR(("Scan	Engine Error %d\n", err));
			return err;
		}

		tofobj->scanestarted = TRUE;
	}

	return BCME_OK;
}

/* wl proxd_status funtion */
static int wlc_pdtof_get_status(pdmthd_if_t* svcif, bool *is_active,
	wl_proxd_status_iovar_t *iop)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;

	ASSERT(svcif != NULL);
	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	if (tofobj->sm) {
		if (is_active != NULL) {
			*is_active = (tofobj->smstoped ? FALSE :
				tofobj->sm->tof_state != TOF_STATE_ICONFIRM);
		}
		if (iop) {
			iop->state = tofobj->sm->tof_state;
			iop->mode = tofobj->sm->tof_mode;
			iop->distance = tofobj->distance;
			iop->txcnt = tofobj->txcnt;
			iop->rxcnt = tofobj->rxcnt;
			bcopy((void*)tofobj->frame_type_cnt,
				(void*)iop->frame_type_cnt, sizeof(iop->frame_type_cnt));
			bcopy((void*)tofobj->adj_type_cnt,
				(void*)iop->adj_type_cnt, sizeof(iop->adj_type_cnt));
			iop->dbgstatus = tofobj->dbgstatus;
			iop->avg_rssi = tofobj->avgrssi;
			iop->low_rssi = tofobj->frmcnt;
			iop->hi_rssi = tofobj->sm->tof_legacypeer;
			iop->reason = tofobj->sm->tof_reason;
			bcopy(&tofobj->sm->tof_peerea, &iop->peer, ETHER_ADDR_LEN);
		}
		return BCME_OK;
	}

	return BCME_ERROR;
}

/* config the TOF parameters */
static int wlc_pdtof_config(pdmthd_if_t* svcif, uint8 mode, wlc_bsscfg_t *bsscfg)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;

	ASSERT(svcif != NULL);
	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	if (tofobj->sm) {
		tofobj->sm->tof_mode = mode;
		tofobj->bsscfg = bsscfg;
		if (bsscfg)
			bcopy(&bsscfg->cur_etheraddr, &tofobj->sm->tof_selfea, ETHER_ADDR_LEN);
		else
			bzero(&tofobj->sm->tof_selfea, ETHER_ADDR_LEN);
	}
	return 0;
}

/* activate the scan engine */
static void wlc_pdtof_activate_pm(pdtof_obj_t* tofobj)
{
	wlc_info_t *wlc;

	wlc = tofobj->wlc;

	ASSERT(wlc != NULL);

	tofobj->txcnt =	0;
	tofobj->rxcnt =	0;

	/* start excursion for proximity duration */
	wlc_pdtof_excursion_start(tofobj, tofobj->paramp->chanspec,
		tofobj->paramp->duration);

	if (!(tofobj->tunep->flags & WL_PROXD_FLAG_NOCHANSWT)) {
		/* Enable AVB timer in case it is turned off when CLK is off */
		wlc_enable_avb_timer(wlc->hw, TRUE);
		/*use SCAN engine SCAN_INPROESS to make
		 * wlc_is_non_delay_mpc() to return FALSE
		 */
		wlc_radio_mpc_upd(wlc);

	}
}

/* deactivate the scan engine */
static void wlc_pdtof_deactivate_pm(pdtof_obj_t* tofobj)
{

}

/* wl proxd_find/stop function */
static int wlc_pdtof_start(pdmthd_if_t* svcif, bool start)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;
	pdtof_sm_t *sm;
	wlc_info_t *wlc;

	ASSERT(tofobj != NULL);
	ASSERT(tofobj->sm != NULL);
	ASSERT(tofobj->wlc != NULL);

	sm = tofobj->sm;
	wlc = tofobj->wlc;

	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	if (start) {
		tof_tslist_t *listp = &tofobj->tof_tslist;

#ifdef TOF_DEBUG_TIME2
		wlc_read_tsf(tofobj->wlc, &tsf_start, &tsf_hi);
		tsf_scanstart = tsf_txreq = tsf_rxack =
		tsf_rxm = tsf_tmo = tsf_lastm = 0;
#endif
		tofobj->smstoped = FALSE;
		tofobj->distance = 0;
		tofobj->meanrtt = 0;
		tofobj->modertt = 0;
		tofobj->medianrtt = 0;
		tofobj->sdrtt = 0;
		tofobj->dbgstatus = 0;
		bzero((void*)tofobj->frame_type_cnt, sizeof(tofobj->frame_type_cnt));
		bzero((void*)tofobj->adj_type_cnt, sizeof(tofobj->adj_type_cnt));

		wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_INITIAL);

#ifdef TOF_COLLECT
		if (tofobj->collect && tofobj->collect_size != tofobj->paramp->ftm_cnt) {
			MFREE(wlc->osh,	tofobj->collect, (tofobj->collect_size + 1) *
				sizeof(wl_proxd_collect_data_t));

			tofobj->collect_size = 0;
			tofobj->collect	= MALLOC(wlc->osh, (tofobj->paramp->ftm_cnt + 1) *
				sizeof(wl_proxd_collect_data_t));
			if (tofobj->collect)
				tofobj->collect_size = tofobj->paramp->ftm_cnt;
			else
				goto err;
		}

		tofobj->debug_cnt = 0;
		tofobj->collect_cnt = 0;
		if (tofobj->collect) {
			bzero(tofobj->collect, (tofobj->collect_size + 1) *
				sizeof(wl_proxd_collect_data_t));
		}
#endif /* TOF_COLLECT */

		if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
			bcopy(&tofobj->paramp->tgt_mac, &sm->tof_peerea, ETHER_ADDR_LEN);
			if (ETHER_ISNULLADDR(&tofobj->paramp->tgt_mac)) {
				tofobj->smstoped = TRUE;
				return BCME_BADADDR;
			}

			if (!listp->tslist || tofobj->measurecnt != tofobj->paramp->ftm_cnt) {
				/* The measure counts changed */
				if (listp->tslist)
					MFREE(wlc->osh, listp->tslist, tofobj->measurecnt *
						sizeof(tofts_t));

				listp->tslist = MALLOC(wlc->osh, tofobj->paramp->ftm_cnt *
					sizeof(tofts_t));
				if (listp->tslist)
					tofobj->measurecnt = tofobj->paramp->ftm_cnt;
				else
					goto err;
			}

			listp->tscnt = 0;
			if (listp->tslist)
				bzero(listp->tslist, tofobj->measurecnt * sizeof(tofts_t));
			wlc_pdtof_activate_pm(tofobj);
			MDBG(("TS:%d Started\n", get_usts(tofobj->wlc)));
		} else {
			tofobj->measurecnt = tofobj->paramp->ftm_cnt;
			tofobj->tx_t1 = 0;
			tofobj->tx_t4 = 0;
		}
	} else if (!tofobj->smstoped) {
		if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
			wlc_pdtof_confirmed(sm, TOF_REASON_ABORT);
		}
		else
			wlc_pdtof_hw(tofobj, FALSE);
		tofobj->smstoped = TRUE;
	}

	return BCME_OK;

err:
	tofobj->measurecnt = 0;
	tofobj->smstoped = TRUE;
	WL_ERROR(("MALLOC failed %s\n",	__FUNCTION__));
	return BCME_NOMEM;
}

/* TOF release function */
static int wlc_pdtof_release(pdmthd_if_t *svcif)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;
	int ret = 0;

	ASSERT(svcif != NULL);

	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);
	ASSIGN_SIGNATURE(tofobj, 0);

	if (tofobj->duration_timer != NULL)
		wlc_hrt_free_timeout(tofobj->duration_timer);

	if (tofobj->ftm_tx_timer != NULL)
		wlc_hrt_free_timeout(tofobj->ftm_tx_timer);

	if (tofobj->sm)
		MFREE(tofobj->wlc->osh, tofobj->sm, sizeof(pdtof_sm_t));

	if (tofobj->tof_tslist.tslist)
		MFREE(tofobj->wlc->osh, tofobj->tof_tslist.tslist, tofobj->measurecnt *
			sizeof(tofts_t));

	if (tofobj->timer)
		wl_free_timer(tofobj->wlc->wl, tofobj->timer);

	if (tofobj->tunep) {
		ret = tofobj->tunep->flags & WL_PROXD_FLAG_NETRUAL;
		MFREE(tofobj->wlc->osh, tofobj->tunep, sizeof(wl_proxd_params_tof_tune_t));
	}

	if (tofobj->paramp)
		MFREE(tofobj->wlc->osh, tofobj->paramp, sizeof(wl_proxd_params_tof_method_t));

#ifdef TOF_COLLECT
	if (tofobj->collect_header)
		MFREE(tofobj->wlc->osh, tofobj->collect_header, sizeof(wl_proxd_collect_header_t));

	if (tofobj->collect)
		MFREE(tofobj->wlc->osh, tofobj->collect, (tofobj->collect_size + 1) *
			sizeof(wl_proxd_collect_data_t));

	if (tofobj->debug_info)
		MFREE(tofobj->wlc->osh, tofobj->debug_info, MAX_DEBUG_COUNT *
			sizeof(wl_proxd_debug_data_t));
#endif /* TOF_COLLECT */

	wlc_enable_avb_timer(tofobj->wlc->hw, FALSE);

	MFREE(tofobj->wlc->osh, tofobj, sizeof(pdtof_obj_t));
	return ret;
}

/* update TOF parameters from rxed TOF request frame */
static void update_tof_params_from_ftm(pdtof_obj_t* tofobj, tof_trig_frm_t *ftm_rq)
{
	tof_rq_params_t *params = &ftm_rq->ftm_ie.params.rqparams;
	MDBG(("rcvd FTM_RQ, period:%d, ftm_cnt:%d, tx_rate:%x, vht_rate:%x\n",
		ntoh32(params->ftm_period), ntoh32(params->ftm_count),
		ntoh16(params->ftm_rate), ntoh16(params->ftm_vht_rate)));
	tofobj->ftm_period = ntoh32(params->ftm_period);
	tofobj->paramp->ftm_cnt = ntoh32(params->ftm_count);
	tofobj->measurecnt = tofobj->paramp->ftm_cnt;
	tofobj->paramp->retry_cnt = ntoh16(params->retry_count);
	tofobj->paramp->tx_rate = ntoh16(params->ftm_rate);
	tofobj->paramp->vht_rate = ntoh16(params->ftm_vht_rate);
	tofobj->paramp->chanspec = ntoh16(params->chanspec);
}

static int wlc_pdtof_rx_endpkt(pdtof_obj_t *tofobj, struct ether_addr *sa, uint8* param)
{
	tof_trig_frm_t *hdr = (tof_trig_frm_t *)param;
	wl_proxd_event_data_t *evp, event;
	int ret = 0;

	if (tofobj->smstoped)
		return ret;

	if (tofobj->timeractive && !bcmp(&tofobj->initiatormac, sa, ETHER_ADDR_LEN)) {
		tofobj->timeractive = FALSE;
		wl_del_timer(tofobj->wlc->wl, tofobj->timer);
	}

	if ((tofobj->tunep->flags & WL_PROXD_FLAG_TARGET_REPORT) &&
		hdr->ftm_ie.params.rqendparams.valid &&
		tofobj->svc_funcs && tofobj->svc_funcs->notify) {
		evp = &event;
		bzero(evp, sizeof(*evp));
		evp->ver = hton16(TOF_VER);
		evp->mode = hton16(tofobj->sm->tof_mode);
		evp->method = hton16(PROXD_TOF_METHOD);
		bcopy(sa, &evp->peer_mac, ETHER_ADDR_LEN);
		evp->ftm_unit = hton16(TOF_NS_PS);
		evp->ftm_cnt = 0;

		evp->err_code = hdr->ftm_ie.params.rqendparams.reason;
		evp->distance = hdr->ftm_ie.params.rqendparams.distance;
		evp->validfrmcnt = hdr->ftm_ie.params.rqendparams.frmcnt;
		evp->validfrmcnt = hton16(evp->validfrmcnt);
		evp->sdrtt = hdr->ftm_ie.params.rqendparams.sdrtt;
		evp->var3 = hdr->ftm_ie.params.rqendparams.var3;
		if (evp->err_code == TOF_REASON_OK)
			ret = (*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr, sa,
				WLC_E_STATUS_SUCCESS, WLC_E_PROXD_COMPLETED,
				(void *)evp, sizeof(*evp));
		else
			ret = (*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr, sa,
				WLC_E_STATUS_FAIL, WLC_E_PROXD_ERROR, (void *)evp, sizeof(*evp));
	}

	return ret;
}

/* Process rxed action frames */
static int wlc_pdtof_process_action_frame(pdmthd_if_t* svcif, struct ether_addr *sa,
	struct ether_addr *da, wlc_d11rxhdr_t *wrxh, uint8 *body, int body_len, uint32 rspec)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;
	pdtof_sm_t *sm;
	wlc_info_t *wlc;
	int ret;
	wlc_pdtof_data_t data;
	d11rxhdr_t *rxh;

	ASSERT(tofobj != NULL);
	ASSERT(tofobj->sm != NULL);
	ASSERT(tofobj->wlc != NULL);

	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	if (tofobj->smstoped)
		return 0;
#ifdef	TOF_DEBUG
	printf("rx frame %d %d %d %d\n", *body, *(body+1), *(body+2), *(body+3));
#endif
	if (*body != DOT11_ACTION_CAT_WNM && *body != DOT11_ACTION_CAT_UWNM)
		return BCME_ERROR;

	rxh = &wrxh->rxhdr;

	if (*(body+1) == DOT11_WNM_ACTION_TMNG_MEASUR_REQ)
	{
		tof_trig_frm_t *ftm_rq = (tof_trig_frm_t *)body;
		MDBG(("TS:%d, Rx FTM REQ: cat:%x, act:%d, trig:%d\n",
			get_usts(tofobj->wlc), ftm_rq->category, ftm_rq->action, ftm_rq->trigger));

		/* check if its a broadcom's extended ftm_rq frame  */
		if ((body_len >= FTM_11V_EXT_MIN) &&
			(ftm_rq->ftm_ie.id == 0xdd) &&
			(ftm_rq->ftm_ie.OUI[0] == BRCM_OUI[0]) &&
			(ftm_rq->ftm_ie.OUI[1] == BRCM_OUI[1]) &&
			(ftm_rq->ftm_ie.OUI[2] == BRCM_OUI[2]) &&
			/*  extra check for the frm format rev  */
			(ftm_rq->ftm_ie.params.rqparams.schk == (FTM_PROTO_REV ^ FTM_PARAMS_MAGIC) ||
			ftm_rq->ftm_ie.params.rqendparams.schk == (FTM_PROTO_REV ^ FTM_PARAMS_MAGIC))) {
				if (ftm_rq->trigger) {
				/* correct ftm_req with additional params */
				update_tof_params_from_ftm(tofobj, ftm_rq);
		} else {
					/* parse event results */
					wlc_pdtof_rx_endpkt(tofobj, sa, body);
				}
		} else {
			WL_ERROR(("ERROR: Not a BCM extended FTM_REQ\n"));
		}

		if (*(body+2) == TOF_TYPE_REQ_END)
			data.tof_type = TOF_TYPE_REQ_END;
		else if (*(body+2) == TOF_TYPE_REQ_START) {
#ifdef TOF_COLLECT
			tofobj->debug_cnt = 0;
#endif /* TOF_COLLECT */
			data.tof_type = TOF_TYPE_REQ_START;
		} else
			return BCME_ERROR;
	} else if ((*(body+1) == DOT11_UWNM_ACTION_TIMING_MEASUREMENT) && (rxh->RxStatus1 &  htol16(RXS_TOFINFO))) {
		tof_measure_frm_t *tof_hdr = (tof_measure_frm_t *)body;
		if (tof_hdr->token)
			data.tof_type = TOF_TYPE_MEASURE;
		else
			data.tof_type = TOF_TYPE_MEASURE_END;
	}
#ifdef TOF_COLLECT
	else if (*(body+1) == DOT11_UWNM_ACTION_TIMING_COLLECT) {
		tof_collect_frm_t *tof_hdr = (tof_collect_frm_t*)body;
		data.tof_type = tof_hdr->type;
	}
#endif /* TOF_COLLECT */
	else
		return BCME_ERROR;

	sm = tofobj->sm;
	wlc = tofobj->wlc;

	bcopy(da, &data.tof_dstea, ETHER_ADDR_LEN);
	bcopy(sa, &data.tof_srcea, ETHER_ADDR_LEN);
	data.tof_rssi = wlc_lq_rssi_pktrxh_cal(wlc, wrxh);
	data.tof_rspec = rspec;

	++tofobj->rxcnt;

#ifdef TOF_COLLECT
	if (tofobj->debug_info && tofobj->debug_cnt < MAX_DEBUG_COUNT) {
		wl_proxd_debug_data_t *debug = &tofobj->debug_info[tofobj->debug_cnt];

		bzero(debug, sizeof(wl_proxd_debug_data_t));
		debug->count = (uint8)tofobj->rxcnt;
		debug->stage = (uint8)sm->tof_state;
		debug->received = (uint8)TRUE;
		debug->paket_type = (uint8)data.tof_type;
		debug->category = (uint8)body[0];
		debug->action = (uint8)body[1];
		debug->token = (uint8)body[2];
		debug->follow_token = (uint8)body[3];
		debug->index = debug->follow_token + 1;
		tofobj->debug_cnt++;
	}
#endif /* TOF_COLLECT */

	if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT &&
		sm->tof_state != TOF_STATE_IDLE && sm->tof_state != TOF_STATE_ICONFIRM)
		sm->tof_rxvht = TRUE;

	ret = wlc_pdtof_sm(sm, TOF_EVENT_RXACT, body, body_len, &data);
	if (ret == TOF_RET_ALIVE) {
		if (tofobj->duration_timer_active) {
			wlc_hrt_del_timeout(tofobj->duration_timer);
			wlc_hrt_add_timeout(tofobj->duration_timer, tofobj->paramp->timeout*1000,
				wlc_pdtof_duration_expired_cb, (void *)tofobj);
		}
	} else if (ret == TOF_RET_SLEEP) {
		if (tofobj->scanestarted) {
			wlc_scan_abort_ex(tofobj->wlc->scan, tofobj->bsscfg, WLC_E_STATUS_ABORT);
			tofobj->scanestarted = FALSE;
		}
	}

	return 0;
}

/* action frame tx complete callback */
static void	wlc_pdtof_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	pdtof_obj_t *tofobj = (pdtof_obj_t *)arg;

	if (tofobj->smstoped)
		return;
	if (!(txstatus & TX_STATUS_ACK_RCV)) {
		WL_ERROR(("ACK was lost txstat:0x%x, pkt:%d, retry:%d\n",
			txstatus, tofobj->sm->tof_txcnt, tofobj->sm->tof_retrycnt));

		if (tofobj->ftm_tx_timer_active) {
			wlc_hrt_del_timeout(tofobj->ftm_tx_timer);
			tofobj->ftm_tx_timer_active = FALSE;
			WL_ERROR(("%s:ERROR: ftm[%d] OnHrtTimer TX is pending, cancelled\n",
				__FUNCTION__, tofobj->sm->tof_txcnt));
		}

		wlc_pdtof_sm(tofobj->sm, TOF_EVENT_NOACK, NULL, 0, NULL);
	} else {
		wlc_pdtof_sm(tofobj->sm, TOF_EVENT_ACKED, NULL, 0, NULL);
	}
}

/* initialize brcm vendor specific fields in the ftm request frame  */
static void tof_ftm_req_init(pdtof_obj_t* tofobj, tof_trig_frm_t *tof_hdr, uint8 type)
{
	tof_hdr->category = DOT11_ACTION_CAT_WNM;
	tof_hdr->action = DOT11_WNM_ACTION_TMNG_MEASUR_REQ;

	tof_hdr->ftm_ie.id = VS_IE_ID;
	tof_hdr->ftm_ie.len = sizeof(tof_rq_params_t)+ 3; /* vend data + oui len */
	bcopy(BRCM_OUI, tof_hdr->ftm_ie.OUI, DOT11_OUI_LEN);
	tof_hdr->ftm_ie.params.rqparams.rev = FTM_PROTO_REV;

	if (type == TOF_TYPE_REQ_START) {
	/* T period during which intr's phy will be blind, depends on b/w */
		tof_hdr->ftm_ie.params.rqparams.ftm_period = hton32(tofobj->ftm_period);
		tof_hdr->ftm_ie.params.rqparams.chanspec = hton16(tofobj->paramp->chanspec);
		tof_hdr->ftm_ie.params.rqparams.ftm_count = hton32(tofobj->paramp->ftm_cnt);
		tof_hdr->ftm_ie.params.rqparams.retry_count = hton16(tofobj->paramp->retry_cnt);
		tof_hdr->ftm_ie.params.rqparams.ftm_rate  = hton16(tofobj->paramp->tx_rate);
		tof_hdr->ftm_ie.params.rqparams.ftm_vht_rate  = hton16(tofobj->paramp->vht_rate);
		tof_hdr->ftm_ie.params.rqparams.chanspec = hton16(tofobj->paramp->chanspec);
	/* sanity check signature at the end of the req param structure */
		tof_hdr->ftm_ie.params.rqparams.schk = (FTM_PROTO_REV ^ FTM_PARAMS_MAGIC);
	}
	else if (tofobj->tunep->flags & WL_PROXD_FLAG_INITIATOR_REPORT) {
		/* Request end frame report distance to target */
		tof_hdr->ftm_ie.params.rqendparams.valid = TRUE;
		tof_hdr->ftm_ie.params.rqendparams.frmcnt = tofobj->frmcnt;
		tof_hdr->ftm_ie.params.rqendparams.distance = hton32(tofobj->distance);
		tof_hdr->ftm_ie.params.rqendparams.sdrtt = hton32(tofobj->sdrtt);
		tof_hdr->ftm_ie.params.rqendparams.var3 = hton32(tofobj->var3);
		tof_hdr->ftm_ie.params.rqendparams.reason = tofobj->sm->tof_reason;
		tof_hdr->ftm_ie.params.rqendparams.schk = (FTM_PROTO_REV ^ FTM_PARAMS_MAGIC);
	}
}

/* TOF action frame send function */
static int wlc_pdtof_send(pdtof_sm_t *sm, struct ether_addr	*da, uint8 type)
{
	wlc_info_t *wlc;
	pdtof_obj_t* tofobj = sm->tof_obj;
	wl_action_frame_t *af;
	ratespec_t rate_override;
	uint16 durid = 60;
	pkcb_fn_t fn = NULL;
	int ret = BCME_ERROR;
#ifdef TOF_DEBUG
	char eabuf[32];

	bcm_ether_ntoa(da, eabuf);
	printf("send type %d, %s %d\n", type, eabuf, sm->tof_txcnt);
#endif
#ifdef TOF_PROFILE
	wlc_read_tsf(tofobj->wlc, &tsf_lastm, &tsf_hi);
	tsf_lastm -= tsf_start;
#endif
	if (!tofobj->svc_funcs || !tofobj->svc_funcs->txaf)
		return BCME_ERROR;

	if (type == TOF_TYPE_MEASURE) {
		rate_override = (ratespec_t)(tofobj->paramp->tx_rate |
			tofobj->paramp->vht_rate << 16);
	} else
		rate_override = PROXD_DEFAULT_TX_RATE;

	if ((rate_override & WL_RSPEC_BW_MASK) == WL_RSPEC_BW_UNSPECIFIED) {
		if (CHSPEC_IS80(tofobj->paramp->chanspec))
			rate_override |= WL_RSPEC_BW_80MHZ;
		else if (CHSPEC_IS40(tofobj->paramp->chanspec))
			rate_override |= WL_RSPEC_BW_40MHZ;
	}

	if (RSPEC_ISVHT(rate_override))
		sm->tof_txvht = TRUE;

	wlc = tofobj->wlc;
	if ((af = (wl_action_frame_t *)MALLOC(wlc->osh, WL_WIFI_AF_PARAMS_SIZE)) == NULL)
		return BCME_ERROR;

	bzero(af, WL_WIFI_AF_PARAMS_SIZE);
	if (da)
		bcopy(da, &af->da, ETHER_ADDR_LEN);

	af->packetId = tofobj->txcnt;
	/* COPY	required TLVS will be added	later */
	if (type == TOF_TYPE_REQ_START || type == TOF_TYPE_REQ_END)
	{
		tof_trig_frm_t *tof_hdr = (tof_trig_frm_t *)af->data;
		tof_ftm_req_init(tofobj, tof_hdr, type);  /* initialize frame fields */
		af->len = sizeof(tof_trig_frm_t);
		tof_hdr->trigger = type;
		fn = wlc_pdtof_tx_complete;
#ifdef TOF_PROFILE
		printf("EVENT = %d, TIME = 0x%0x\n", (type+10), tsf_lastm);
#endif
	} else if (type == TOF_TYPE_MEASURE_END || type == TOF_TYPE_MEASURE) {
		tof_measure_frm_t *tof_hdr = (tof_measure_frm_t *)af->data;
		int nextFrames = tofobj->measurecnt - sm->tof_txcnt;

		if (tofobj->tunep->totalfrmcnt)
			nextFrames = tofobj->tunep->totalfrmcnt - sm->tof_txpktcnt;

		if (tofobj->sm->tof_legacypeer == TOF_LEGACY_AP)
			nextFrames--;

		af->len	= sizeof(tof_measure_frm_t);
		durid = wlc_compute_frame_dur(wlc, rate_override, WLC_LONG_PREAMBLE, 0);
		if (nextFrames > 0 && tofobj->tunep->gdadj && tofobj->tunep->rsv_media)
			durid += tofobj->tunep->rsv_media;

		wlc_write_shm(wlc, tofobj->shmemptr + M_TOF_DOT11DUR, durid);

		tof_hdr->category = DOT11_ACTION_CAT_UWNM;
		tof_hdr->action = DOT11_UWNM_ACTION_TIMING_MEASUREMENT;
		if (type == TOF_TYPE_MEASURE) {
			tof_hdr->token = sm->tof_txcnt+1;
			/* mark measurement pkt with special packet ID to identify it later */
			af->packetId |= PROXD_FTM_PACKET_TAG;
		}
		else
			tof_hdr->token = 0;

		tof_hdr->follow_token = sm->tof_txcnt;
		tof_hdr->max_TOD = 0;
		tof_hdr->max_TOA = 0;
		htol48_ua_store(tofobj->tx_t1, tof_hdr->TOD);
		htol48_ua_store(tofobj->tx_t4, tof_hdr->TOA);
		fn = wlc_pdtof_tx_complete;

		wlc_pdtof_measure(tofobj, TOF_RESET);
#ifdef TOF_PROFILE
		printf("EVENT = %d, TOKEN=%d FOLLOW_TOKEN=%d TIME = 0x%0x\n", (type+10),
			tof_hdr->token, tof_hdr->follow_token, tsf_lastm);
#endif
	}
#ifdef TOF_COLLECT
	else if	(type == TOF_TYPE_COLLECT_REQ_START || type == TOF_TYPE_COLLECT_REQ_END) {
		tof_collect_frm_t *tof_hdr = (tof_collect_frm_t	*)af->data;

		af->len = sizeof(tof_collect_frm_t);
		tof_hdr->category = DOT11_ACTION_CAT_UWNM;
		tof_hdr->action = DOT11_UWNM_ACTION_TIMING_COLLECT;
		tof_hdr->type = type;
		fn = wlc_pdtof_tx_complete;
	} else if (type == TOF_TYPE_COLLECT_DATA_END || type == TOF_TYPE_COLLECT_DATA) {
		tof_collect_frm_t *tof_hdr = (tof_collect_frm_t *)af->data;
		wl_proxd_collect_query_t query;

		memset(&query, 0, sizeof(query));
		query.method = htol32(PROXD_TOF_METHOD);

		if (tofobj->remote_cnt == 0) {
			query.request = PROXD_COLLECT_QUERY_HEADER;
			ret = wlc_pdtof_collection((pdmthd_if_t *)tofobj, &query,
				tof_hdr->data, ACTION_FRAME_SIZE, &tof_hdr->length);
		} else {
			if (!tofobj->collect ||
				tofobj->remote_cnt > tofobj->collect_cnt ||
				tofobj->remote_cnt > tofobj->collect_size) {
				MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
				return BCME_ERROR;
			}

			query.request = PROXD_COLLECT_QUERY_DATA;
			query.index = tofobj->remote_cnt - 1;
			ret = wlc_pdtof_collection((pdmthd_if_t *)tofobj, &query,
				tof_hdr->data, ACTION_FRAME_SIZE, &tof_hdr->length);
		}

		if (ret != BCME_OK || tof_hdr->length <= 0) {
			MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
			return BCME_ERROR;
		}

		af->len	= sizeof(tof_collect_frm_t) + tof_hdr->length - 1;
		tof_hdr->category = DOT11_ACTION_CAT_UWNM;
		tof_hdr->action = DOT11_UWNM_ACTION_TIMING_COLLECT;
		if (type == TOF_TYPE_COLLECT_DATA && tofobj->collect &&
			tofobj->remote_cnt < tofobj->collect_cnt)
			tof_hdr->type = TOF_TYPE_COLLECT_DATA;
		else
			tof_hdr->type = TOF_TYPE_COLLECT_DATA_END;
		tof_hdr->index = tofobj->remote_cnt;
		fn = wlc_pdtof_tx_complete;
	}
#endif /* TOF_COLLECT */
	else {
		WL_ERROR(("Unknown TOF pkt type	%d\n", type));
		MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
		return BCME_ERROR;
	}

#ifdef TOF_COLLECT
	if (tofobj->debug_info && tofobj->debug_cnt < MAX_DEBUG_COUNT) {
		wl_proxd_debug_data_t *debug = &tofobj->debug_info[tofobj->debug_cnt];

		bzero(debug, sizeof(wl_proxd_debug_data_t));
		debug->count = (uint8)tofobj->txcnt + 1;
		debug->stage = (uint8)sm->tof_state;
		debug->received = (uint8)FALSE;
		debug->paket_type = (uint8)type;
		debug->category = (uint8)af->data[0];
		debug->action = (uint8)af->data[1];
		debug->token = (uint8)af->data[2];
		debug->follow_token = (uint8)af->data[3];
		debug->index = debug->follow_token + 1;
		tofobj->debug_cnt++;
	}
#endif /* TOF_COLLECT */

	if ((ret = (*tofobj->svc_funcs->txaf)(wlc->pdsvc_info, af, rate_override,
		fn, &sm->tof_selfea)) == BCME_OK) {
		tofobj->txcnt++;
	}
	else
		WL_ERROR(("%s failed %d\n", __FUNCTION__, ret));

	MFREE(wlc->osh, af, WL_WIFI_AF_PARAMS_SIZE);
	return ret;
}

/* calcualte the distance based on measure results */
static int wlc_analyze_results(pdtof_obj_t*	tofobj, tof_tslist_t *listp,
	uint32 *meanrtt, uint32 *modertt, uint32 *medianrtt, uint32 *sdrtt,
	int32 *gdavg, int32 *adjavg, int32 *difavg)
{
	tofts_t *toftsp;
	uint32 *list, sigma;
	int16 i, cnt, newcnt;
	uint32 mean, median, k;
	int32 delta;
#if defined(TOF_DEBUG_TIME) || defined(TOF_STDDEV_CHECK)
	int32 rawmean = 0;
#endif
	if (listp && listp->tscnt && listp->tslist) {
		list = (uint32 *)MALLOC(tofobj->wlc->osh, sizeof(uint32)*listp->tscnt);
		if (list == NULL) {
			WL_ERROR(("TOF alloc memory failed\n"));
			return -1;
		}
		toftsp = &listp->tslist[0];
		k = wlc_pdtof_get_kval(tofobj, TRUE);
		for (i = 0, cnt = 0; i < listp->tscnt; i++, toftsp++) {
			if (toftsp->t1 < toftsp->t4 && (toftsp->t2 < toftsp->t3 ||
				(tofobj->sm->tof_legacypeer == TOF_LEGACY_AP &&
				toftsp->t2 == 0 && toftsp->t3 == 0)) &&
				!toftsp->discard) {
				list[cnt] = (toftsp->t4-toftsp->t1) - (toftsp->t3-toftsp->t2);
				if (list[cnt] < k + TOF_RTT_MAXVAL && list[cnt] + 100 > k) {
					/* only use valid RTT, discard RTT < -100  or RTT > 2000 */
					cnt++;
					if (gdavg) *gdavg += toftsp->gd;
					if (adjavg) *adjavg += toftsp->adj;
					if (difavg) *difavg += (toftsp->gd - toftsp->adj);
				}
#ifdef TOF_DEBUG_TIME
				else
					printf("Discard AVBERR (%d)Delta %d\n", i, list[cnt]);
#endif
			}
#ifdef TOF_DEBUG_TIME
			else
				printf("Discard AVBERR (%d)\n", i);
#endif
		}

		sigma = wlc_pdsvc_deviation(list, cnt, 1);
		if (cnt) {
			if (gdavg) *gdavg /= cnt;
			if (adjavg) *adjavg /= cnt;
			if (difavg) *difavg /= cnt;
		}

#ifdef TOF_STDDEV_CHECK
		if (CHSPEC_IS80(tofobj->paramp->chanspec)) {
			uint32 sigma1;
			/* 80M Hz do Filtering */
			while (sigma >= 150 && cnt > 2) {
				int maxidx = 0, minidx = 0;
				uint32 total;
				/* remove crazy outlays */
				i = 0;
				min = 0xffffffff;
				max = 0;
				total = 0;
				while (i < cnt) {
					if (list[i] > max) {
						max = list[i];
						maxidx = i;
					}
					if (list[i] < min) {
						min = list[i];
						minidx = i;
					}
					total += list[i];
					i++;
				}
				max = (total-max) / (cnt-1);
				min = (total-min) / (cnt-1);
				total = (total-max-min) / (cnt-2);
				if (max - total < total - min) {
					/* remove max */
#ifdef TOF_DEBUG_TIME
					printf("Discard Sigma Delta %d\n", list[maxidx]);
#endif
					list[maxidx] = list[cnt-1];
				} else {
					/* remove min */
#ifdef TOF_DEBUG_TIME
					printf("Discard Sigma Delta %d\n", list[minidx]);
#endif
					list[minidx] = list[cnt-1];
				}
				cnt--;
				sigma = wlc_pdsvc_deviation(list, cnt, 1);
			}

			sigma1 = TOF_TS_RANGE;

			rawmean = wlc_pdsvc_average(list, cnt);
#ifdef TOF_DEBUG_TIME
			printf("Raw	mean %d, sigma %d\n", rawmean, sigma);
#endif
			/* Remove outlayers */
			i = 0;
			newcnt = cnt;
			while (i < newcnt) {
				if ((list[i] > rawmean + sigma1) ||
					(list[i] < rawmean - sigma1)) {
#ifdef TOF_DEBUG_TIME
					printf("Discard OutOfRange %d\n", list[i]);
#endif
					list[i] = list[newcnt-1];
					newcnt--;
				} else
					i++;
			}
		} else {
			newcnt = cnt;
		}
#else
		newcnt = cnt;
#endif /* TOF_STDDEV_CHECK */
		tofobj->frmcnt = newcnt;
		mean = wlc_pdsvc_average(list, newcnt);
		delta = mean;
#if defined(TOF_DEBUG_TIME) || defined(TOF_STDDEV_CHECK)
		rawmean = mean;
#endif
#ifdef TOF_STDDEV_CHECK
		median = wlc_pdsvc_median(list, newcnt);
#else
		median = 0;
#endif
#ifdef TOF_DEBUG_TIME
		printf("cnt	%d mean	%d median %d	k %d delta %d "
			"RTT %d	deviation %d.%d	avgrssi	%d\n", newcnt, mean,
			median, k, delta, delta-k, sigma/10, sigma%10, tofobj->avgrssi);
		printf("RawMeanNs%d	AvgNs %d MedianNs	%d Ns %d\n", rawmean-k,
			mean-k, median-k, delta-k);
		rawmean -= k;
		if (rawmean <= 0)
			rawmean = 0;
		else
			rawmean = TOF_NS_TO_16THMETER(rawmean);
		printf("RawMeanDistance %d\n", rawmean);
#endif /* TOF_DEBUG_TIME */
		MFREE(tofobj->wlc->osh, list, sizeof(uint32)*listp->tscnt);

		if (meanrtt) *meanrtt = mean;
		if (modertt) *modertt = 0;
		if (medianrtt) *medianrtt = median;
		if (sdrtt) *sdrtt = sigma;

		delta -= k;
		if (delta <= 0)
			delta = 0;
		else
			delta = TOF_NS_TO_16THMETER(delta);

		return delta;
	}
	return -1;
}

/* generate TOF events */
static void wlc_pdtof_event(pdtof_sm_t *sm, uint8 eventtype)
{
	pdtof_obj_t* tofobj = sm->tof_obj;
	wl_proxd_event_data_t *evp, event;

	if (tofobj->smstoped)
		return;

	if (tofobj->svc_funcs && tofobj->svc_funcs->notify) {
		if (tofobj->tunep->flags & WL_PROXD_FLAG_NETRUAL) {
			if (eventtype == WLC_E_PROXD_STOP &&
				(tofobj->tunep->flags & WL_PROXD_FLAG_REPORT_FAILURE)) {
				bcopy(&sm->tof_peerea, &tofobj->initiatormac, ETHER_ADDR_LEN);
				if (tofobj->timeractive) {
					wl_del_timer(tofobj->wlc->wl, tofobj->timer);
				}
				wl_add_timer(tofobj->wlc->wl, tofobj->timer, 2, FALSE);
				tofobj->timeractive = TRUE;
			}
			return ;
		}
		evp = &event;
		bzero(evp, sizeof(*evp));
		evp->ver = hton16(TOF_VER);
		evp->mode = hton16(sm->tof_mode);
		evp->method = hton16(PROXD_TOF_METHOD);
		evp->err_code = sm->tof_reason;
		bcopy(&sm->tof_peerea, &evp->peer_mac, ETHER_ADDR_LEN);
		evp->ftm_unit = hton16(TOF_NS_PS);
		evp->ftm_cnt = 0;
		(*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr, &sm->tof_peerea,
			WLC_E_STATUS_SUCCESS, eventtype, (void *)evp, sizeof(*evp));
	}
}

/* TOF get report results and state machine goes to CONFIRM state */
static void wlc_pdtof_report_done(pdtof_sm_t *sm, uint8 reason)
{
	pdtof_obj_t* tofobj = sm->tof_obj;

	sm->tof_state = TOF_STATE_ICONFIRM;
	tofobj->dbgstatus = (reason == TOF_REASON_OK);

	wlc_pdtof_excursion_abort(tofobj);
	wlc_pdtof_deactivate_pm(tofobj);

	if (tofobj->tunep->flags & WL_PROXD_FLAG_NETRUAL) {
		wlc_pdtof_reset(sm, WL_PROXD_MODE_TARGET, TOF_REASON_INITIAL);
	}

	MDBG(("TS:%d should be in pwr_down now\n", get_usts(tofobj->wlc)));

}
/* TOF get final results and state machine goes to CONFIRM state */
static int wlc_pdtof_confirmed(pdtof_sm_t *sm, uint8 reason)
{
	pdtof_obj_t* tofobj = sm->tof_obj;
	tof_tslist_t *listp = &tofobj->tof_tslist;
	int i, ret = 0;
	int32 rssi;
	wl_proxd_event_data_t *evp;
	int distance;
	uint32 meanrtt = 0, modertt = 0, medianrtt = 0, sdrtt = 0;
	ftm_sample_t *ftmp;
	int tofret = TOF_RET_SLEEP;
#ifdef TOF_DEBUG_TIME2
	uint32 t, t1;
#endif

	wlc_pdtof_measure(tofobj, TOF_RESET);
	if (tofobj->smstoped)
		return tofret;

#ifdef TOF_DEBUG_TIME2
	wlc_read_tsf(tofobj->wlc, &t, &tsf_hi);
	t = t - tsf_start;
#endif

	if (tofobj->tunep->flags & WL_PROXD_FLAG_INITIATOR_REPORT)
		sm->tof_state = TOF_STATE_IREPORT;
	else {
		sm->tof_state = TOF_STATE_ICONFIRM;
		wlc_pdtof_excursion_abort(tofobj);
		wlc_pdtof_deactivate_pm(tofobj);
	}
	sm->tof_reason = reason;
	wlc_pdtof_hw(tofobj, FALSE);

#ifdef TOF_DEBUG_TIME
	MDBG(("TS:%d CONFIRMED: reason is: ", get_usts(tofobj->wlc)));
	switch (reason) {
		case TOF_REASON_OK:
			printf("OK\n");
			break;
		case TOF_REASON_REQEND:
			printf("REQEND\n");
			break;
		case TOF_REASON_TIMEOUT:
			printf("TIMEOUT\n");
			break;
		case TOF_REASON_NOACK:
			printf("NOACK\n");
			break;
		case TOF_REASON_INVALIDAVB:
			printf("INVALIDAVB\n");
			break;
		case TOF_REASON_ABORT:
			printf("ABORT\n");
			break;
		default:
			printf("ERROR\n");
			break;
	}
#endif /* TOF_DEBUG_TIME */
	if (tofobj->timeractive) {
		tofobj->timeractive = FALSE;
		wl_del_timer(tofobj->wlc->wl, tofobj->timer);
	}

	if (tofobj->svc_funcs && tofobj->svc_funcs->notify) {

		int len = OFFSETOF(wl_proxd_event_data_t, ftm_buff);

		if ((tofobj->tunep->flags & WL_PROXD_FLAG_INITIATOR_RPTRTT) &&
#ifdef TOF_COLLECT
			!tofobj->remote_request &&
#endif /* TOF_COLLECT */
			(reason == TOF_REASON_OK || reason == TOF_REASON_TIMEOUT ||
			reason == TOF_REASON_NOACK))
			len += sizeof(ftm_sample_t)*listp->tscnt;

		evp = (wl_proxd_event_data_t *)MALLOC(tofobj->wlc->osh, len);
		if (evp == NULL) {
			WL_ERROR(("TOF confirm alloc memory failed\n"));
			return tofret;
		}

		evp->ver = hton16(TOF_VER);
		evp->mode = hton16(sm->tof_mode);
		evp->method = hton16(PROXD_TOF_METHOD);
		evp->err_code = reason;
		evp->TOF_type = ((sm->tof_legacypeer == TOF_LEGACY_AP) ?
			TOF_TYPE_ONE_WAY : TOF_TYPE_TWO_WAY);
		evp->OFDM_frame_type = (sm->tof_rxvht || sm->tof_txvht)?
			TOF_FRAME_RATE_VHT : TOF_FRAME_RATE_LEGACY;
		evp->gdcalcresult = (tofobj->adj_type_cnt[TOF_ADJ_SOFTWARE] >
			tofobj->adj_type_cnt[TOF_ADJ_HARDWARE]) ?
			TOF_ADJ_SOFTWARE : TOF_ADJ_HARDWARE;
		evp->bandwidth = CHSPEC_IS80(tofobj->paramp->chanspec)? TOF_BW_80MHZ :
			(CHSPEC_IS40(tofobj->paramp->chanspec)? TOF_BW_40MHZ :
			(CHSPEC_IS20(tofobj->paramp->chanspec)? TOF_BW_20MHZ :
			TOF_BW_10MHZ));
		bcopy(&sm->tof_peerea, &evp->peer_mac, ETHER_ADDR_LEN);
		evp->ftm_unit = hton16(TOF_NS_PS);
		if ((tofobj->tunep->flags & WL_PROXD_FLAG_INITIATOR_RPTRTT) && listp->tscnt > 0 &&
#ifdef TOF_COLLECT
			!tofobj->remote_request &&
#endif /* TOF_COLLECT */
			(reason == TOF_REASON_OK || reason == TOF_REASON_TIMEOUT ||
			reason == TOF_REASON_NOACK)) {
			evp->ftm_cnt = hton16(listp->tscnt);
			ftmp = &evp->ftm_buff[0];
		} else {
			evp->ftm_cnt = 0;
			ftmp = NULL;
		}

#ifdef TOF_COLLECT
		if (tofobj->remote_request) {
			if (reason == TOF_REASON_OK)
				(*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr,
					&sm->tof_peerea, WLC_E_STATUS_SUCCESS,
					WLC_E_PROXD_COLLECT_COMPLETED, (void *)evp, len);
			else
				(*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr,
					&sm->tof_peerea, WLC_E_STATUS_FAIL,
					WLC_E_PROXD_COLLECT_ERROR, (void *)evp, len);
			tofobj->remote_request = FALSE;
		} else {
#endif /* TOF_COLLECT */

		for (i = 0, rssi = 0; i < listp->tscnt; i++) {
			uint32 delta;
#ifdef TOF_DEBUG_TIME
			uint32 rspec = listp->tslist[i].rspec;
			char rate_buf[20];
			if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_RATE)
				sprintf(rate_buf, "rate	%d Mbps", (rspec & WL_RSPEC_RATE_MASK)/2);
			else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HT)
				sprintf(rate_buf, "ht mcs %d", (rspec & WL_RSPEC_RATE_MASK));
			else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT)
				sprintf(rate_buf, "vht mcs %dx%d",
					(rspec & WL_RSPEC_VHT_MCS_MASK),
					(rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT);
			else
				sprintf(rate_buf, "unknown");
#endif
			/* get rssi calib adjustment value, will be 0 if not enabled */
			listp->tslist[i].rssi += adjust_rssi(tofobj);

			rssi += listp->tslist[i].rssi;
			delta = (listp->tslist[i].t4-listp->tslist[i].t1) -
				(listp->tslist[i].t3-listp->tslist[i].t2);
#ifdef TOF_DEBUG_TIME
			printf("(%d)T1 %u T2 %u	T3 %u T4 %u	Delta %d "
				"rssi %d %s	gd %d hadj %d dif %d%s\n", i,
				listp->tslist[i].t1, listp->tslist[i].t2, listp->tslist[i].t3,
				listp->tslist[i].t4, delta, listp->tslist[i].rssi, rate_buf,
				listp->tslist[i].gd, listp->tslist[i].adj,
				listp->tslist[i].gd - listp->tslist[i].adj,
				listp->tslist[i].discard? "	discard" : "");
#endif
			if (ftmp) {
				ftmp->value = hton32(delta);
				ftmp->rssi = listp->tslist[i].rssi;
				ftmp++;
			}
		}

		if (listp->tscnt) {
			rssi = rssi * (-10)/listp->tscnt;
			if ((rssi%10) >= 5)
				rssi = -rssi/10 - 1;
			else
				rssi = -rssi/10;
		} else
			rssi = 0;
		evp->avg_rssi = hton16(rssi);
		tofobj->avgrssi = rssi;

#ifdef TOF_DEBUG_TIME
		printf("Sample cnt %d avg rssi %d\n", listp->tscnt, rssi);
#endif
		evp->var1 = evp->var2 = evp->var3 = 0;
		distance = wlc_analyze_results(tofobj, listp,
			&meanrtt, &modertt, &medianrtt, &sdrtt,
			&evp->var1, &evp->var2, &tofobj->var3);
		evp->var1 = hton32(evp->var1);
		evp->var2 = hton32(evp->var2);
		evp->var3 = hton32(tofobj->var3);
		evp->validfrmcnt = hton16(tofobj->frmcnt);
		if (tofobj->frmcnt == 0 && sm->tof_reason == TOF_REASON_OK) {
			sm->tof_reason = TOF_REASON_INVALIDAVB;
			evp->err_code = TOF_REASON_INVALIDAVB;
		}
#ifdef TOF_DEBUG_TIME
		printf("Distance %d.%d meter\n", (distance>>4), (((distance & 0xf) * 1000)>>4));
#endif
		tofobj->distance = distance;
		tofobj->meanrtt = meanrtt;
		tofobj->modertt = modertt;
		tofobj->medianrtt = medianrtt;
		tofobj->sdrtt = sdrtt;

		evp->distance = ntoh32(distance);
		evp->meanrtt = ntoh32(meanrtt);
		evp->modertt = ntoh32(modertt);
		evp->medianrtt = ntoh32(medianrtt);
		evp->sdrtt = ntoh32(sdrtt); /* standard deviation */
		evp->peer_router_info = NULL;

		if (tofobj->tunep->flags & WL_PROXD_FLAG_INITIATOR_REPORT)
		{
			wlc_pdtof_send(sm, &sm->tof_peerea, TOF_TYPE_REQ_END);
			tofret = TOF_RET_ALIVE;
		}
#ifdef TOF_DEBUG_TIME2
		wlc_read_tsf(tofobj->wlc, &t1, &tsf_hi);
		t1 = t1 -tsf_start;
		printf("Scan %d Txreq %d Rxack %d 1stM %d ",
			tsf_scanstart, tsf_txreq, tsf_rxack, tsf_rxm);
		printf("lastM	%d Confirm %d Event	%d tmo %d\n",
			tsf_lastm, t, t1, tsf_tmo);
#endif

		if (reason == TOF_REASON_OK)
			ret = (*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr,
				&sm->tof_peerea, WLC_E_STATUS_SUCCESS, WLC_E_PROXD_COMPLETED,
				(void *)evp, len);
		else
			ret = (*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr,
				&sm->tof_peerea, WLC_E_STATUS_FAIL, WLC_E_PROXD_ERROR,
				(void *)evp, len);

#ifdef TOF_COLLECT
		}
#endif /* TOF_COLLECT */
		MFREE(tofobj->wlc->osh, evp, len);
		if ((tofobj->tunep->flags & WL_PROXD_FLAG_NETRUAL) && !ret &&
			!(tofobj->tunep->flags & WL_PROXD_FLAG_INITIATOR_REPORT)) {
			wlc_pdtof_reset(sm, WL_PROXD_MODE_TARGET, TOF_REASON_INITIAL);
		}
	}
	return tofret;
}

#ifdef TOF_COLLECT
/* TOF collects debug data */
static void wlc_pdtof_collect_data(pdtof_obj_t* tofobj, int index)
{
	if (tofobj->collect && index < tofobj->collect_size) {
		wl_proxd_collect_data_t *collect = tofobj->collect;
		collect->index = ++index;
		bcopy(collect, collect + index, sizeof(wl_proxd_collect_data_t) -
			(256 - collect->nfft) * sizeof(uint32));
		tofobj->collect_cnt = index;
		tofobj->remote_collect = FALSE;
	}
}

/* initiator gets collect debug data */
static int wlc_pdtof_initiator_get_collect_data(pdtof_obj_t* tofobj, tof_collect_frm_t *tof_hdr)
{
	if (tof_hdr->index == 0 && tofobj->collect_header) {
		bcopy(tof_hdr->data, tofobj->collect_header, tof_hdr->length);
		tofobj->collect_cnt = 0;
		tofobj->remote_collect = TRUE;
	} else if (tofobj->collect && tof_hdr->index <= tofobj->collect_size) {
		bcopy(tof_hdr->data, tofobj->collect + tof_hdr->index, tof_hdr->length);
		tofobj->collect_cnt = tof_hdr->index;
		tofobj->remote_collect = TRUE;
	}

	return (!tofobj->collect || tof_hdr->index >= tofobj->collect_size);
}
#endif /* TOF_COLLECT */

/* initiator gets AVB time stamp */
static int wlc_pdtof_initiator_get_ts(pdtof_sm_t *sm, tof_measure_frm_t *protp,
	int rssi, uint32 rspec, uint8 type)
{
	pdtof_obj_t* tofobj = sm->tof_obj;
	tof_tslist_t *listp = &tofobj->tof_tslist;
	tofts_t * list = listp->tslist;
	uint64 t3, t2;
	int32 gd, adj;
	bool discard = FALSE;

	if (type == TOF_TYPE_MEASURE && wlc_pdtof_measure_results(tofobj, &t3, &t2, &gd, &adj,
		(uint16)protp->follow_token, TRUE, NULL, &discard, TRUE)) {
		t2 = 0;
		t3 = 0;
		gd = 0;
		adj = 0;
		discard = TRUE;
	}

	if (protp->follow_token <= tofobj->measurecnt && list) {
		/* Get t2, t3 */
		if (protp->follow_token < tofobj->measurecnt && type == TOF_TYPE_MEASURE) {
#ifdef TOF_COLLECT
			wlc_pdtof_collect_data(tofobj, protp->follow_token);
#endif /* TOF_COLLECT */
			list[protp->follow_token].t3 = (uint32)t3;
			list[protp->follow_token].t2 = (uint32)t2;
			list[protp->follow_token].gd = gd;
			list[protp->follow_token].adj = adj;
			list[protp->follow_token].rssi = rssi;
			list[protp->follow_token].rspec = rspec;
			list[protp->follow_token].discard = discard;
		}
		if (protp->follow_token) {
			uint32 delta;
			list[protp->follow_token-1].t1 = wlc_pdtof_ts_value(protp->TOD);
			list[protp->follow_token-1].t4 = wlc_pdtof_ts_value(protp->TOA);
			delta = list[protp->follow_token-1].t4 - list[protp->follow_token-1].t1;
			list[protp->follow_token-1].t1 /= 10;
			list[protp->follow_token-1].t4 = list[protp->follow_token-1].t1 +
				(delta + 5)/10;
			if (protp->follow_token > listp->tscnt)
				listp->tscnt = protp->follow_token;
		}
	}

	return BCME_OK;
}

/* target gets time stamp */
static int wlc_pdtof_target_get_ts(pdtof_sm_t *sm, bool acked)
{
	pdtof_obj_t* tofobj = sm->tof_obj;
	uint64 t1, t4;
	int32 gd, adj;
	int8 rssi;
	bool discard = FALSE;
	uint16 id;

	if (sm->tof_legacypeer == TOF_LEGACY_AP) {
		tof_tslist_t *listp = &tofobj->tof_tslist;
		id = listp->tscnt;
	} else {
		id = sm->tof_txcnt;
	}

	if (wlc_pdtof_measure_results(tofobj, &t1, &t4, &gd, &adj, id, acked, &rssi,
		&discard, FALSE)) {
		t1 = 0;
		t4 = 0;
		gd = 0;
		adj = 0;
		discard = TRUE;
	}

#ifdef TOF_COLLECT
	if (sm->tof_legacypeer != TOF_LEGACY_AP) {
		wlc_pdtof_collect_data(tofobj, sm->tof_txcnt);
	}
#endif /* TOF_COLLECT */

	if (!acked || discard)
		return BCME_ERROR;

	if (sm->tof_legacypeer == TOF_LEGACY_AP) {
		tof_tslist_t *listp = &tofobj->tof_tslist;
		tofts_t * list = listp->tslist;

#ifdef TOF_COLLECT
		wlc_pdtof_collect_data(tofobj, tofobj->collect_cnt);
#endif /* TOF_COLLECT */
		if (list && listp->tscnt < tofobj->measurecnt) {
			list[listp->tscnt].t1 = (uint32)t1;
			list[listp->tscnt].t4 = (uint32)t4;
			list[listp->tscnt].gd = gd;
			list[listp->tscnt].adj = adj;
			list[listp->tscnt].rssi = rssi;
			list[listp->tscnt].rspec = (tofobj->paramp->tx_rate |
				tofobj->paramp->vht_rate << 16);
			list[listp->tscnt].discard = discard;
			listp->tscnt++;
		}
		tofobj->tx_t1 = t1;
		tofobj->tx_t4 = t4;
	} else {
		/* convert into	10th Ns	according to the spec */
		tofobj->tx_t1 = t1*10;
		tofobj->tx_t4 = t4*10;
	}
	return BCME_OK;
}

/* TOF timeout function */
static void wlc_pdtof_timer(void *arg)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t *)arg;

	tofobj->timeractive = FALSE;
	if (tofobj->smstoped)
		return;

	if(tofobj->sm->tof_mode == WL_PROXD_MODE_INITIATOR)
		wlc_pdtof_sm(tofobj->sm, TOF_EVENT_WAITMTMO, NULL, 0, NULL);
	else {
		/* target report failing rxed initiator's distance report */
		if (tofobj->svc_funcs && tofobj->svc_funcs->notify) {
			(*tofobj->svc_funcs->notify)(tofobj->svc_funcs->notifyptr,
				&tofobj->initiatormac, WLC_E_STATUS_FAIL, WLC_E_PROXD_STOP,
				NULL, 0);
		}
	}
}

/* accessor for method's configurable parameters */
static int wlc_pdtof_access_params(pdmthd_if_t *svcif, void *pbuf, int len, bool write)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;
	int	err = 0;

	ASSERT(pbuf != NULL);

	if (write) {
		/*	service	is setting the params  */
		bcopy(pbuf, tofobj->paramp, sizeof(wl_proxd_params_tof_method_t));
	} else {
		/* service is reading params */
		bcopy(tofobj->paramp, pbuf, sizeof(wl_proxd_params_tof_method_t));
	}
	return err;
}

#ifdef TOF_COLLECT
/* wl proxd_collect function */
static int wlc_pdtof_collection(pdmthd_if_t *svcif, wl_proxd_collect_query_t *query,
	void *buff, int len, uint16 *reqLen)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;
	pdtof_sm_t *sm;
	wlc_info_t *wlc;

	ASSERT(tofobj != NULL);
	ASSERT(tofobj->sm != NULL);
	ASSERT(tofobj->wlc != NULL);
	ASSERT(buff != NULL);

	sm = tofobj->sm;
	wlc = tofobj->wlc;

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
				reply->status = (int8)((tofobj->collect	!= NULL)?
					((tofobj->debug_info != NULL)? 2 : 1) : 0);
				reply->mode = sm->tof_mode;
				reply->busy = ((sm->tof_mode == WL_PROXD_MODE_TARGET) ?
					(sm->tof_state > TOF_STATE_IDLE) : TRUE) &&
					(sm->tof_state < TOF_STATE_ICONFIRM);
				reply->remote = tofobj->remote_collect;
			} else {
				if (query->status) {
					if (!tofobj->collect) {
						tofobj->collect = MALLOC(wlc->osh,
							(tofobj->measurecnt + 1) *
							sizeof(wl_proxd_collect_data_t));
						if (!tofobj->collect) {
							WL_ERROR(("MALLOC failed %s\n",
								__FUNCTION__));
							return BCME_NOMEM;
						}

						tofobj->collect_cnt = 0;
						tofobj->collect_size = tofobj->measurecnt;

						tofobj->collect_header = MALLOC(wlc->osh,
							sizeof(wl_proxd_collect_header_t));
						if (!tofobj->collect_header) {
							MFREE(wlc->osh, tofobj->collect,
								(tofobj->collect_size + 1) *
								sizeof(wl_proxd_collect_data_t));
							tofobj->collect = NULL;

							WL_ERROR(("MALLOC failed %s\n",
								__FUNCTION__));
							return BCME_NOMEM;
						}
					}

					if (query->status == 1) {
						if (tofobj->debug_info) {
							MFREE(wlc->osh, tofobj->debug_info,
								MAX_DEBUG_COUNT *
								sizeof(wl_proxd_debug_data_t));

							tofobj->debug_info = NULL;
							tofobj->debug_cnt = 0;
						}
					} else if (!tofobj->debug_info) {
						tofobj->debug_info = MALLOC(wlc->osh,
							MAX_DEBUG_COUNT *
							sizeof(wl_proxd_debug_data_t));
						if (!tofobj->debug_info) {
							WL_ERROR(("MALLOC failed %s\n",
								__FUNCTION__));
							return BCME_NOMEM;
						}

						tofobj->debug_cnt = 0;
					}
				} else {
					if (tofobj->collect) {
						MFREE(wlc->osh, tofobj->collect,
							(tofobj->collect_size + 1) *
							sizeof(wl_proxd_collect_data_t));
						tofobj->collect = NULL;

						if (tofobj->collect_header) {
							MFREE(wlc->osh, tofobj->collect_header,
								sizeof(wl_proxd_collect_header_t));
							tofobj->collect_header = NULL;
						}

						tofobj->collect_cnt = 0;
						tofobj->collect_size = 0;
					}

					if (tofobj->debug_info) {
						MFREE(wlc->osh, tofobj->debug_info,
							MAX_DEBUG_COUNT *
							sizeof(wl_proxd_debug_data_t));

						tofobj->debug_info = NULL;
						tofobj->debug_cnt = 0;
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
			if (tofobj->remote_collect && tofobj->collect_header) {
				bcopy(tofobj->collect_header, reply,
					sizeof(wl_proxd_collect_header_t));
			} else {
				bzero(reply, sizeof(wl_proxd_collect_header_t));

				reply->total_frames = (uint16)tofobj->collect_cnt;
				if (CHSPEC_IS80(tofobj->paramp->chanspec)) {
					reply->nfft = 256;
					reply->bandwidth = 80;
				} else if (CHSPEC_IS40(tofobj->paramp->chanspec)) {
					reply->nfft = 128;
					reply->bandwidth = 40;
				} else if (CHSPEC_IS20(tofobj->paramp->chanspec)) {
					reply->nfft = 64;
					reply->bandwidth = 20;
				} else {
					reply->nfft = 0;
					reply->bandwidth = 10;
				}
				reply->channel = CHSPEC_CHANNEL(tofobj->paramp->chanspec);
				reply->chanspec = tofobj->paramp->chanspec;
				reply->fpfactor = tofobj->Tq;
				reply->fpfactor_shift = tofobj->Q;
				wlc_pdtof_get_log2_scale(tofobj,&reply->thresh_log2,
					&reply->thresh_scale);
				bcopy((void*)tofobj->tunep->w_offset, (void*)reply->window_offset,
					TOF_BW_NUM*sizeof(int16));
				bcopy((void*)tofobj->tunep->w_len, (void*)reply->window_len,
					TOF_BW_NUM*sizeof(int16));
				if (sm->tof_mode == WL_PROXD_MODE_INITIATOR)
					reply->kvalue = wlc_pdtof_get_kval(tofobj, TRUE);
				else
					reply->kvalue = wlc_pdtof_get_kval(tofobj, FALSE);
				reply->distance = tofobj->distance;
				reply->meanrtt = tofobj->meanrtt;
				reply->modertt = tofobj->modertt;
				reply->medianrtt = tofobj->medianrtt;
				reply->sdrtt = tofobj->sdrtt;
				si_pmu_fvco_pllreg(wlc->hw->sih, NULL, &reply->clkdivisor);
				reply->clkdivisor &= PMU1_PLL0_PC1_M1DIV_MASK;
				reply->chipnum = tofobj->chipnum;
				reply->chiprev = tofobj->chiprev;
				reply->phyver = wlc->band->phyrev;

				bcopy(&sm->tof_selfea, &reply->loaclMacAddr, ETHER_ADDR_LEN);
				if (ETHER_ISNULLADDR(&sm->tof_peerea))
					bcopy(&tofobj->paramp->tgt_mac, &reply->remoteMacAddr,
						ETHER_ADDR_LEN);
				else
					bcopy(&sm->tof_peerea, &reply->remoteMacAddr,
						ETHER_ADDR_LEN);
			}
			break;
		}

		case PROXD_COLLECT_QUERY_DATA:
		{
			wl_proxd_collect_data_t *reply;
			wl_proxd_collect_data_t *collect;
			int size;

			if (!tofobj->collect)
				return BCME_ERROR;

			if (query->index >= (uint16)tofobj->collect_cnt ||
				query->index >= (uint16)tofobj->collect_size)
				return BCME_RANGE;

			collect = tofobj->collect + query->index + 1;
			size = sizeof(wl_proxd_collect_data_t) -
				(256 - collect->nfft)* sizeof(uint32);

			*reqLen = (uint16)size;
			if (len < size)
				return BCME_BUFTOOSHORT;

			reply = (wl_proxd_collect_data_t *)buff;
			bcopy(collect, reply, size);
			break;
		}

		case PROXD_COLLECT_QUERY_DEBUG:
		{
			wl_proxd_debug_data_t *reply;
			wl_proxd_debug_data_t *debug;
			int size;

			if (!tofobj->debug_info)
				return BCME_ERROR;

			if (query->index >= (uint16)tofobj->debug_cnt ||
				query->index >= (uint16)MAX_DEBUG_COUNT)
				return BCME_RANGE;

			debug = &tofobj->debug_info[query->index];
			size = sizeof(wl_proxd_debug_data_t);

			*reqLen = size;
			if (len < size)
				return BCME_BUFTOOSHORT;

			reply = (wl_proxd_debug_data_t *)buff;
			bcopy(debug, reply, size);
			break;
		}

		case PROXD_COLLECT_REMOTE_REQUEST:
		{
			if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
				if (sm->tof_state != TOF_STATE_IDLE &&
					sm->tof_state != TOF_STATE_ICONFIRM) {
					return BCME_BUSY;
				}

				if (!tofobj->collect) {
					return BCME_ERROR;
				}

				tofobj->collect_cnt = 0;
				bzero(tofobj->collect, (tofobj->collect_size + 1) *
					sizeof(wl_proxd_collect_data_t));

				tofobj->remote_request = TRUE;
				wlc_pdtof_start((pdmthd_if_t*)tofobj, TRUE);
			} else
				return  BCME_UNSUPPORTED;
			break;
		}

		default:
			return BCME_UNSUPPORTED;
	}

	return BCME_OK;
}
#endif /* TOF_COLLECT */

/* delay certain time before txing the next measurement packet */
static void wlc_pdtof_ftm_tx_timer(void *ctx)
{
	pdtof_obj_t *tofobj = (pdtof_obj_t *)ctx;
	pdtof_sm_t *sm = tofobj->sm;
	uint8 type = TOF_TYPE_MEASURE;

	ASSERT(tofobj != NULL);

	if (!tofobj->caldone && !tofobj->smstoped) {
		/* Wait for calculation done */
		wlc_hrt_add_timeout(tofobj->ftm_tx_timer, 100, wlc_pdtof_ftm_tx_timer, ctx);
		return;
	}
	wlc_hrt_del_timeout(tofobj->ftm_tx_timer);
	tofobj->ftm_tx_timer_active = FALSE;
	if(!AP_ENAB(tofobj->wlc->pub))
		wlc_gptimer_wake_upd(tofobj->wlc, WLC_GPTIMER_AWAKE_PROXD, FALSE);
	sm->tof_txpktcnt++;
	if (sm->tof_txcnt >= tofobj->measurecnt || (tofobj->tunep->totalfrmcnt &&
		sm->tof_txpktcnt >= tofobj->tunep->totalfrmcnt))
		type = TOF_TYPE_MEASURE_END;

	if (wlc_pdtof_send(sm, &tofobj->sm->tof_peerea, type) != BCME_OK) {
		WL_ERROR(("%s: ERROR in pdtof_send\n", __FUNCTION__));
	}
	MDBG(("%s: TS:%d, tx ftm:%d, retry_cnt:%d\n", __FUNCTION__,
		get_usts(tofobj->wlc), sm->tof_txcnt, sm->tof_retrycnt));
}

/* Use one way RTT */
static void wlc_pdtof_start_oneway(pdtof_sm_t *sm)
{
	sm->tof_state = TOF_STATE_ILEGACY;
	sm->tof_legacypeer = TOF_LEGACY_AP;
	sm->tof_txcnt = 0;
	sm->tof_txpktcnt = 1;
	wlc_pdtof_send(sm, &sm->tof_peerea, TOF_TYPE_MEASURE);
}

/* TOF state machine */
static int wlc_pdtof_sm(pdtof_sm_t *sm, int event, uint8 *param, int paramlen,
	wlc_pdtof_data_t *datap)
{
	tof_measure_frm_t *protp = (tof_measure_frm_t *)param;
	pdtof_obj_t* tofobj = sm->tof_obj;
	wlc_info_t *wlc = tofobj->wlc;
#ifdef TOF_DEBUG
	char eabuf[32];
#endif
	int ret = TOF_RET_IGNORE;

	ASSERT(event < TOF_EVENT_LAST);
	ASSERT(sm != NULL);
#ifdef TOF_PROFILE
	wlc_read_tsf(tofobj->wlc, &tsf_lastm, &tsf_hi);
	tsf_lastm -= tsf_start;
	if (protp)
		printf("EVENT = %d, TOKEN=%d FOLLOW_TOKEN=%d TIME = 0x%0x\n", event, protp->token,
			protp->follow_token, tsf_lastm);
	else
		printf("EVENT = %d, TIME = 0x%0x\n", event, tsf_lastm);
#endif

	if (sm->tof_mode == WL_PROXD_MODE_DISABLE)
		return TOF_RET_SLEEP;

	if (event == TOF_EVENT_TMO && sm->tof_state != TOF_STATE_ICONFIRM) {
		wlc_pdtof_confirmed(sm,	TOF_REASON_TIMEOUT);
		return TOF_RET_SLEEP;
	}

	if (event == TOF_EVENT_RXACT) {
		ASSERT(param != NULL);
#ifdef TOF_COLLECT
		ASSERT(paramlen >= sizeof(tof_collect_frm_t)); /* must be at least this len */
#endif
		ASSERT(datap != NULL);

		if (sm->tof_mode == WL_PROXD_MODE_TARGET) {
			if (bcmp(&ether_bcast, &tofobj->allow_mac, ETHER_ADDR_LEN) &&
				bcmp(&datap->tof_srcea, &tofobj->allow_mac, ETHER_ADDR_LEN)) {
				return TOF_RET_IGNORE;
			}
		}

		if (sm->tof_state != TOF_STATE_IDLE &&
			sm->tof_state != TOF_STATE_ICONFIRM) {
			if (bcmp(&datap->tof_srcea, &sm->tof_peerea, ETHER_ADDR_LEN) ||
				bcmp(&datap->tof_dstea, &sm->tof_selfea, ETHER_ADDR_LEN)) {
				return TOF_RET_IGNORE;
			}
		}
	}

	switch (sm->tof_state) {
		case TOF_STATE_IDLE:
			if (event == TOF_EVENT_WAKEUP) {
				if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
#ifdef TOF_DEBUG_TIME2
					wlc_read_tsf(tofobj->wlc, &tsf_txreq, &tsf_hi);
					tsf_txreq -= tsf_start;
#endif
					wlc_pdtof_hw(tofobj, TRUE);
					/* Only needed on side which sends acks */
					/* Response is ack wrapped in ctl frame so its 10 + ACK CF +
					 * HT Ctrl = 16 bytes
					 */
					wlc_pdtof_tof_init_vht(tofobj, 16);
					wlc_pdtof_tof_setup_vht(tofobj);

					wlc_pdtof_event(sm, WLC_E_PROXD_START);
					wlc_pdtof_send(sm, &sm->tof_peerea, TOF_TYPE_REQ_START);
					wlc_pdtof_measure(tofobj, TOF_RX);
					sm->tof_state = TOF_STATE_IWAITM;
					ret = TOF_RET_ALIVE;
				} else if (sm->tof_mode == WL_PROXD_MODE_TARGET) {
					ret = TOF_RET_ALIVE;
				} else {
					ret = TOF_RET_SLEEP;
					WL_ERROR(("Invalid mode	%d\n", sm->tof_mode));
				}
			}
#ifdef TOF_COLLECT
			else if (event == TOF_EVENT_COLLECT_REQ) {
				if (sm->tof_mode == WL_PROXD_MODE_INITIATOR) {
					wlc_pdtof_event(sm, WLC_E_PROXD_COLLECT_START);
					wlc_pdtof_send(sm, &sm->tof_peerea,
						TOF_TYPE_COLLECT_REQ_START);
					sm->tof_state = TOF_STATE_IWAITCL;
					sm->tof_retrycnt = 0;
					ret = TOF_RET_ALIVE;
				} else {
					ret = TOF_RET_SLEEP;
					WL_ERROR(("Invalid mode %d\n", sm->tof_mode));
				}
			}
#endif /* TOF_COLLECT */
			else if (event == TOF_EVENT_RXACT) {
				if (datap->tof_type == TOF_TYPE_REQ_START) {
					/* Rxed	measure	request	packet */
					if (sm->tof_mode == WL_PROXD_MODE_TARGET) {
						wlc_bsscfg_t *bsscfg;

						wlc_pdtof_hw(tofobj, TRUE);

						ret = TOF_RET_ALIVE;
						bcopy(&datap->tof_srcea, &sm->tof_peerea,
							ETHER_ADDR_LEN);
						bcopy(&datap->tof_dstea, &sm->tof_selfea,
							ETHER_ADDR_LEN);

						if (tofobj->bsscfg)
							bsscfg = tofobj->bsscfg;
						else
							bsscfg = wlc_bsscfg_find_by_hwaddr(wlc,
								&sm->tof_selfea);
						if (bsscfg && !ETHER_ISNULLADDR(&bsscfg->BSSID) &&
							bsscfg->current_bss->chanspec != 0) {
							tofobj->paramp->chanspec =
								bsscfg->current_bss->chanspec;
						}

						sm->tof_state = TOF_STATE_TWAITM;
						MDBG(("TS:%d TGT FTM[0] \n", get_usts(wlc)));
						sm->tof_txpktcnt = 1;
						wlc_pdtof_send(sm, &sm->tof_peerea,
							TOF_TYPE_MEASURE);
						wlc_pdtof_event(sm, WLC_E_PROXD_START);
					} else {
						ret = TOF_RET_IGNORE;
					}
				}
#ifdef TOF_COLLECT
				else if (datap->tof_type == TOF_TYPE_COLLECT_REQ_START) {
					/* Rxed collect request packet */
					if (sm->tof_mode == WL_PROXD_MODE_TARGET) {
						ret = TOF_RET_ALIVE;
						bcopy(&datap->tof_srcea, &sm->tof_peerea,
							ETHER_ADDR_LEN);
						bcopy(&datap->tof_dstea, &sm->tof_selfea,
							ETHER_ADDR_LEN);

						sm->tof_state = TOF_STATE_TWAITCL;
						sm->tof_retrycnt = 0;
						tofobj->remote_cnt = 0;
						wlc_pdtof_send(sm, &sm->tof_peerea,
							TOF_TYPE_COLLECT_DATA);
						wlc_pdtof_event(sm, WLC_E_PROXD_COLLECT_START);
					} else {
						ret = TOF_RET_IGNORE;
					}
				}
#endif /* TOF_COLLECT */
				else {
					ret = TOF_RET_IGNORE;
				}
			}
			break;

		case TOF_STATE_IWAITM:
			ret = TOF_RET_ALIVE;
			if (event == TOF_EVENT_RXACT) {
				/* Target Pairing State	*/
				if (datap->tof_type == TOF_TYPE_MEASURE ||
					datap->tof_type == TOF_TYPE_MEASURE_END) {
					/* Rxed	measure	packet */
					if (sm->tof_legacypeer == TOF_LEGACY_UNKNOWN) {
						/* First Measurement Packet	*/
#ifdef TOF_DEBUG_TIME2
						wlc_read_tsf(tofobj->wlc, &tsf_rxm, &tsf_hi);
						tsf_rxm	-= tsf_start;
#endif
						sm->tof_legacypeer = TOF_NONLEGACY_AP;
					}
					if (tofobj->timeractive) {
						wl_del_timer(wlc->wl, tofobj->timer);
						if (datap->tof_type == TOF_TYPE_MEASURE)
							wl_add_timer(wlc->wl, tofobj->timer,
								tofobj->paramp->timeout, FALSE);
						else
							tofobj->timeractive = FALSE;
					}
					++sm->tof_rxcnt;
					wlc_pdtof_initiator_get_ts(sm, protp, datap->tof_rssi,
						datap->tof_rspec, datap->tof_type);
					if (datap->tof_type == TOF_TYPE_MEASURE) {
#ifdef TOF_DEBUG_TIME2
						wlc_read_tsf(tofobj->wlc, &tsf_lastm, &tsf_hi);
						tsf_lastm -= tsf_start;
#endif
						if (protp->follow_token >= tofobj->measurecnt) {
							wlc_pdtof_send(sm, &datap->tof_srcea,
								TOF_TYPE_REQ_END);
							ret = wlc_pdtof_confirmed(sm,
								TOF_REASON_OK);
							MDBG(("Initr: got last MES frame, rxcnt:%d,"
								"ftoken:%d mescnt:%d\n",
								sm->tof_rxcnt, protp->follow_token,
								tofobj->measurecnt));
						}
					} else {
#ifdef TOF_DEBUG_TIME2
						wlc_read_tsf(tofobj->wlc, &tsf_lastm, &tsf_hi);
						tsf_lastm -= tsf_start;
#endif
						MDBG(("Initr: got MES_END frm, rcvcnt:%d,"
							" ftoken:%d mescnt:%d \n",
							sm->tof_rxcnt, protp->follow_token,
							tofobj->measurecnt));

						ret = wlc_pdtof_confirmed(sm, TOF_REASON_OK);
					}
				} else {
					WL_ERROR(("Initiator(%d) got unexpected	type %d\n",
						sm->tof_state, datap->tof_type));
					ret = TOF_RET_IGNORE;
				}
			} else if	(event == TOF_EVENT_NOACK) {
				/* REQ is NOT acked	*/
				++sm->tof_retrycnt;
				if (sm->tof_retrycnt >= TOF_REQ_RETRY_CNT) {
					wlc_pdtof_confirmed(sm, TOF_REASON_NOACK);
					ret = TOF_RET_SLEEP;
				} else {
					wlc_pdtof_send(sm, &sm->tof_peerea, TOF_TYPE_REQ_START);
					wlc_pdtof_measure(tofobj, TOF_RX);
					ret = TOF_RET_ALIVE;
				}
			} else if (event == TOF_EVENT_ACKED) {
				sm->tof_retrycnt = 0;
				if (tofobj->tunep->flags & WL_PROXD_FLAG_ONEWAY) {
					wlc_pdtof_start_oneway(sm);
				} else if (tofobj->paramp->timeout) {
					wl_add_timer(wlc->wl, tofobj->timer,
						tofobj->paramp->timeout, FALSE);
					tofobj->timeractive = TRUE;
					if (tofobj->tunep->flags & WL_PROXD_FLAG_NETRUAL) {
						/* AWDL disallow one way TOF */
						sm->tof_legacypeer = TOF_NONLEGACY_AP;
					}
				}
#ifdef TOF_DEBUG_TIME2
				wlc_read_tsf(tofobj->wlc, &tsf_rxack, &tsf_hi);
				tsf_rxack -= tsf_start;
#endif
			} else if (event == TOF_EVENT_WAITMTMO) {
				/* Wait	for	measurement	pkt	timeout	*/
				if (sm->tof_legacypeer == TOF_LEGACY_UNKNOWN) {
					wlc_pdtof_start_oneway(sm);
				} else if (sm->tof_legacypeer == TOF_NONLEGACY_AP) {
					/* AP stoped txing measurement */
					ret = wlc_pdtof_confirmed(sm, TOF_REASON_TIMEOUT);
				}
			}
			break;

		case TOF_STATE_ILEGACY:
			if (event == TOF_EVENT_ACKED || event == TOF_EVENT_NOACK) {
				if (event == TOF_EVENT_ACKED) {
					if (wlc_pdtof_target_get_ts(sm, TRUE) == BCME_OK) {
						sm->tof_retrycnt = 0;
						sm->tof_txcnt++;
					} else
						sm->tof_retrycnt++;
				} else {
					sm->tof_retrycnt++;
					wlc_pdtof_target_get_ts(sm, FALSE);
				}

				if (sm->tof_retrycnt > tofobj->paramp->retry_cnt) {
					wlc_pdtof_confirmed(sm, TOF_REASON_NOACK);
					ret = TOF_RET_SLEEP;
				} else if (sm->tof_txcnt >= tofobj->measurecnt) {
					wlc_pdtof_confirmed(sm, TOF_REASON_OK);
					ret = TOF_RET_SLEEP;
				} else if (sm->tof_txpktcnt >= tofobj->tunep->totalfrmcnt &&
					tofobj->tunep->totalfrmcnt) {
					wlc_pdtof_confirmed(sm, TOF_REASON_ABORT);
					ret = TOF_RET_SLEEP;
				} else {
					sm->tof_txpktcnt++;
					wlc_pdtof_send(sm, &sm->tof_peerea, TOF_TYPE_MEASURE);
					ret = TOF_RET_ALIVE;
				}
			}
			break;

		case TOF_STATE_TWAITM:
			if (event == TOF_EVENT_ACKED || event == TOF_EVENT_NOACK) {
				if (sm->tof_txcnt <= tofobj->measurecnt) {
					if (tofobj->ftm_tx_timer_active) {
						wlc_hrt_del_timeout(tofobj->ftm_tx_timer);
						tofobj->ftm_tx_timer_active = FALSE;

						WL_ERROR(("%s:ERROR:ftm[%d] OnHrtTimer"
							" TX is pending, cancelled\n",
							__FUNCTION__,
							tofobj->sm->tof_txcnt));
					}
					wlc_hrt_add_timeout(tofobj->ftm_tx_timer,
						tofobj->ftm_period,
						wlc_pdtof_ftm_tx_timer, (void *)tofobj);

					tofobj->ftm_tx_timer_active = TRUE;
					tofobj->caldone = FALSE;
					if(!AP_ENAB(wlc->pub))
						wlc_gptimer_wake_upd(wlc,
							WLC_GPTIMER_AWAKE_PROXD, TRUE);
					MDBG(("TS:%d, scheduled TX ftm[%d] retry:%d\n",
						get_usts(tofobj->wlc),
						sm->tof_txcnt, sm->tof_retrycnt));
				}
				if (event == TOF_EVENT_ACKED) {
#ifdef TOF_PROFILE
					wlc_read_tsf(tofobj->wlc, &tsf_lastm, &tsf_hi);
					tsf_lastm -= tsf_start;
					printf("TIME_BEFORE_GET_TS = 0x%0x\n", tsf_lastm);
#endif
					if (sm->tof_txcnt >= tofobj->measurecnt ||
						(tofobj->tunep->totalfrmcnt && sm->tof_txpktcnt >=
						tofobj->tunep->totalfrmcnt))
						/* last measurement frame */
						sm->tof_txcnt++;
					else if (wlc_pdtof_target_get_ts(sm, TRUE) == BCME_OK) {
						sm->tof_retrycnt = 0;
						sm->tof_txcnt++;
					} else {
						MDBG(("get_tgt_ts failed, do retry\n"));
						sm->tof_retrycnt++;
					}
#ifdef TOF_PROFILE
					wlc_read_tsf(tofobj->wlc, &tsf_lastm, &tsf_hi);
					tsf_lastm -= tsf_start;
					printf("TIME_AFTER_GET_TS = 0x%0x\n", tsf_lastm);
#endif
				} else {
					sm->tof_retrycnt++;
					OSL_DELAY(1);
					wlc_pdtof_target_get_ts(sm, FALSE);
				}
				tofobj->caldone = TRUE;
				if (sm->tof_retrycnt > tofobj->paramp->retry_cnt) {

					WL_ERROR(("Too many retries, ftm:%d, stopped\n",
						sm->tof_txcnt));
					sm->tof_retrycnt = 0;
					wlc_pdtof_event(sm, WLC_E_PROXD_STOP);
					wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_NOACK);
					ret = TOF_RET_SLEEP;
				} else if (sm->tof_txcnt > tofobj->measurecnt) {
					MDBG((">TARGET %d ftms completed OK\n",
						sm->tof_txcnt));
					sm->tof_retrycnt = 0;
					wlc_pdtof_event(sm,	WLC_E_PROXD_STOP);
					wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_OK);
					ret = TOF_RET_SLEEP;
				} else if (sm->tof_txpktcnt >= tofobj->tunep->totalfrmcnt &&
					tofobj->tunep->totalfrmcnt) {
					sm->tof_retrycnt = 0;
					wlc_pdtof_event(sm, WLC_E_PROXD_STOP);
					wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_ABORT);
					ret = TOF_RET_SLEEP;
				} else {
					ret = TOF_RET_ALIVE;
				}
			} else if (event == TOF_EVENT_RXACT) {
				if (datap->tof_type == TOF_TYPE_REQ_END) {
					/* Rxed measure request end packet */
					wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_REQEND);
					ret = TOF_RET_SLEEP;
				} else if (datap->tof_type == TOF_TYPE_REQ_START) {
					/* Rxed	start because client resets	*/
					sm->tof_txcnt = 0;
					sm->tof_retrycnt = 0;
					MDBG(("Intiator restarts \n"));
					wlc_pdtof_send(sm, &sm->tof_peerea, TOF_TYPE_MEASURE);
					wlc_pdtof_event(sm, WLC_E_PROXD_START);
					ret = TOF_RET_ALIVE;
				} else {
					MDBG(("Unknown TOF pkt type:%d\n", datap->tof_type));
				}
			}
			break;

#ifdef TOF_COLLECT
		case TOF_STATE_IWAITCL:
			ret = TOF_RET_ALIVE;
			if (event == TOF_EVENT_RXACT) {
				/* Target Pairing State */
				if (datap->tof_type == TOF_TYPE_COLLECT_DATA ||
					datap->tof_type == TOF_TYPE_COLLECT_DATA_END) {
					bool endRx = FALSE;

					if (tofobj->timeractive) {
						wl_del_timer(wlc->wl, tofobj->timer);
						if (datap->tof_type == TOF_TYPE_COLLECT_DATA)
							wl_add_timer(wlc->wl, tofobj->timer,
								tofobj->paramp->timeout, FALSE);
						else
							tofobj->timeractive = FALSE;
					}
					++sm->tof_rxcnt;
					endRx = wlc_pdtof_initiator_get_collect_data(tofobj,
						(tof_collect_frm_t *)protp);
					if (datap->tof_type == TOF_TYPE_COLLECT_DATA) {
#ifdef TOF_DEBUG_TIME2
						wlc_read_tsf(tofobj->wlc, &tsf_lastm, &tsf_hi);
						tsf_lastm -= tsf_start;
#endif
						if (endRx) {
							wlc_pdtof_send(sm, &datap->tof_srcea,
								TOF_TYPE_COLLECT_REQ_END);
							wlc_pdtof_confirmed(sm, TOF_REASON_OK);
							ret = TOF_RET_SLEEP;
						}
					} else {
						wlc_pdtof_confirmed(sm, TOF_REASON_OK);
						ret = TOF_RET_SLEEP;
					}
				} else {
					WL_ERROR(("Initiator(%d) got unexpected type %d\n",
						sm->tof_state, datap->tof_type));
					ret = TOF_RET_IGNORE;
				}
			} else if (event == TOF_EVENT_NOACK) {
				/* REQ is NOT acked */
				++sm->tof_retrycnt;
				if (sm->tof_retrycnt >= TOF_REQ_RETRY_CNT) {
					wlc_pdtof_confirmed(sm, TOF_REASON_NOACK);
					ret = TOF_RET_SLEEP;
				} else {
					wlc_pdtof_send(sm, &sm->tof_peerea,
						TOF_TYPE_COLLECT_REQ_START);
					ret = TOF_RET_ALIVE;
				}
			} else if (event == TOF_EVENT_ACKED) {
				sm->tof_retrycnt = 0;
				if (tofobj->paramp->timeout) {
					wl_add_timer(wlc->wl, tofobj->timer,
						tofobj->paramp->timeout, FALSE);
					tofobj->timeractive = TRUE;
				}
#ifdef TOF_DEBUG_TIME2
				wlc_read_tsf(tofobj->wlc, &tsf_rxack, &tsf_hi);
				tsf_rxack -= tsf_start;
#endif
			} else if (event == TOF_EVENT_WAITMTMO) {
				wlc_pdtof_confirmed(sm, TOF_REASON_TIMEOUT);
				ret = TOF_RET_SLEEP;
			}
			break;

		case TOF_STATE_TWAITCL:
			if (event == TOF_EVENT_ACKED || event == TOF_EVENT_NOACK) {
				if (event == TOF_EVENT_ACKED) {
					sm->tof_retrycnt = 0;
					sm->tof_txcnt++;
					tofobj->remote_cnt++;
				} else {
					sm->tof_retrycnt++;
				}

				if (sm->tof_retrycnt > tofobj->paramp->retry_cnt) {
					wlc_pdtof_event(sm, WLC_E_PROXD_COLLECT_STOP);
					sm->tof_retrycnt = 0;
					wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_NOACK);
					ret = TOF_RET_SLEEP;
				} else if (tofobj->remote_cnt > tofobj->collect_cnt) {
					wlc_pdtof_event(sm, WLC_E_PROXD_COLLECT_STOP);
					wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_OK);
					ret = TOF_RET_SLEEP;
				} else {
					wlc_pdtof_send(sm, &sm->tof_peerea, (tofobj->collect &&
						tofobj->remote_cnt < tofobj->collect_cnt)?
						TOF_TYPE_COLLECT_DATA : TOF_TYPE_COLLECT_DATA_END);
					ret = TOF_RET_ALIVE;
				}
			} else if (event == TOF_EVENT_RXACT) {
				if (datap->tof_type == TOF_TYPE_COLLECT_REQ_END) {
					/* Rxed collect request end packet */
					wlc_pdtof_reset(sm, sm->tof_mode, TOF_REASON_REQEND);
					ret = TOF_RET_SLEEP;
				} else if (datap->tof_type == TOF_TYPE_COLLECT_REQ_START) {
					/* Rxed start because client resets */
					sm->tof_txcnt = 0;
					sm->tof_retrycnt = 0;
					tofobj->remote_cnt = 0;
					wlc_pdtof_send(sm, &sm->tof_peerea, TOF_TYPE_COLLECT_DATA);
					wlc_pdtof_event(sm, WLC_E_PROXD_START);
					ret = TOF_RET_ALIVE;
				}
			}
			break;
#endif /* TOF_COLLECT */
		case TOF_STATE_IREPORT:
			if (event == TOF_EVENT_ACKED) {
				wlc_pdtof_report_done(sm, TOF_REASON_OK);
			} else {
				wlc_pdtof_report_done(sm, TOF_REASON_NOACK);
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

#if defined(TOF_DEBUG) /* do post print otherwise ts avb may fail */
	printf("<0> %s: Mode %d	Event %d State %d Peer %s Pktcnt %d	%d\n",
		__FUNCTION__, sm->tof_mode, event, sm->tof_state,
		bcm_ether_ntoa(&sm->tof_peerea, eabuf), sm->tof_txcnt,
		sm->tof_rxcnt);
#endif

	return ret;
}

/* create TOF method, TOF intialization */
pdmthd_if_t* wlc_pdtof_create_method(wlc_info_t *wlc, uint16 mode, pdsvc_funcs_t* funcsp,
	struct ether_addr *selfmac, pdsvc_payload_t *payload)
{
	pdtof_obj_t* tofobj;
	wl_proxd_params_tof_method_t *paramp;
	wl_proxd_params_tof_tune_t *tunep;

	ASSERT(wlc != NULL);

	tofobj = (pdtof_obj_t*)MALLOC(wlc->osh, sizeof(pdtof_obj_t));
	if (tofobj != NULL) {
		bzero(tofobj, sizeof(pdtof_obj_t));
		ASSIGN_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);
		tofobj->wlc = wlc;

		tofobj->tunep = MALLOC(wlc->osh, sizeof(wl_proxd_params_tof_tune_t));
		if (tofobj->tunep) {
			bzero(tofobj->tunep, sizeof(wl_proxd_params_tof_tune_t));
		} else {
			WL_ERROR(("malloc tune struct failed\n"));
			goto err;
		}
		tofobj->paramp = MALLOC(wlc->osh, sizeof(wl_proxd_params_tof_method_t));
		if (tofobj->paramp) {
			bzero(tofobj->paramp, sizeof(wl_proxd_params_tof_method_t));
		} else {
			WL_ERROR(("malloc param struct failed\n"));
			goto err;
		}

		tofobj->chipnum = CHIPID(wlc->pub->sih->chip);
		tofobj->chiprev = wlc->pub->sih->chiprev;

		tofobj->tofif.mconfig = wlc_pdtof_config;
		tofobj->tofif.mstart = wlc_pdtof_start;
		tofobj->tofif.mpushaf = wlc_pdtof_process_action_frame;
		tofobj->tofif.mstatus = wlc_pdtof_get_status;
		tofobj->tofif.mrelease = wlc_pdtof_release;
		tofobj->tofif.rw_params = wlc_pdtof_access_params;
#ifdef TOF_COLLECT
		tofobj->tofif.collect = wlc_pdtof_collection;
#else
		tofobj->tofif.collect = NULL;
#endif /* TOF_COLLECT */
		tofobj->tofif.params_ptr = (wl_proxd_params_common_t *)tofobj->paramp;
		tofobj->svc_funcs = funcsp;
		tofobj->payloadp = payload;
		tofobj->Q = TOF_SHIFT;
		tofobj->phyver = wlc->band->phyrev;

		paramp = tofobj->paramp;
		paramp->chanspec = PROXD_DEFAULT_CHANSPEC;
		paramp->tx_power = PROXD_DEFAULT_TX_POWER;
		paramp->tx_rate = PROXD_DEFAULT_TX_RATE;
		paramp->timeout = TOF_DEFAULT_TIMEOUT;
		paramp->ftm_cnt = PROXD_DEFAULT_FTM_COUNT;
		paramp->interval = TOF_DEFAULT_INTERVAL;
		paramp->duration = TOF_DEFAULT_DURATION;
		paramp->retry_cnt = PROXD_DEFAULT_RETRY_CNT;

		tunep = tofobj->tunep;
		tunep->gdadj = TOF_DEFAULT_GD_ADJ;
		tunep->sw_adj = TOF_DEFAULT_SW_ADJ;
		tunep->hw_adj = TOF_DEFAULT_HW_ADJ;
		tunep->vhtack = TOF_DEFAULT_VHTACK;
		tofobj->Tq = (funcsp->clock_factor)(wlc->pdsvc_info, tofobj->Q,
			&tunep->Ki, &tunep->Kt);

		/* 80MHz */
		tunep->w_len[TOF_BW_80MHZ_INDEX] = 32;
		tunep->w_offset[TOF_BW_80MHZ_INDEX] = 10;
		/* 40MHz */
		tunep->w_len[TOF_BW_40MHZ_INDEX] = 16;
		tunep->w_offset[TOF_BW_40MHZ_INDEX] = 8;
		/* 20MHz */
		tunep->w_len[TOF_BW_20MHZ_INDEX] = 8;
		tunep->w_offset[TOF_BW_20MHZ_INDEX] = 4;

		if (mode == WL_PROXD_MODE_INITIATOR) {
			tunep->minDT = TOF_INITIATOR_DT_MIN;
			tunep->maxDT = TOF_INITIATOR_DT_MAX;
		} else {
			tunep->minDT = TOF_TARGET_DT_MIN;
			tunep->maxDT = TOF_TARGET_DT_MAX;
		}

		tofobj->sm = MALLOC(wlc->osh, sizeof(pdtof_sm_t));
		if (tofobj->sm) {
			bzero(tofobj->sm, sizeof(pdtof_sm_t));
			tofobj->sm->tof_mode = mode;
			if (selfmac)
				bcopy(selfmac, &tofobj->sm->tof_selfea, ETHER_ADDR_LEN);
			tofobj->sm->tof_obj = tofobj;
		} else {
			WL_ERROR(("Create tofpd obj failed\n"));
			goto err;
		}

		if ((tofobj->duration_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
			WL_ERROR(("wl%d: %s: wlc_hrt_alloc_timeout failed\n",
				wlc->pub->unit, __FUNCTION__));
			goto err;
		}

		if ((tofobj->ftm_tx_timer = wlc_hrt_alloc_timeout(wlc->hrti)) == NULL) {
			WL_ERROR(("wl%d: %s: ftm_tx_timer hrt tmr alloc failed \n",
				wlc->pub->unit, __FUNCTION__));
			goto err;
		}
		tofobj->ftm_tx_timer_active = FALSE;
		tofobj->ftm_period = TOF_FTM_DEFAULT_PERIOD;
		bcopy(&ether_bcast, &tofobj->allow_mac, ETHER_ADDR_LEN);

		if (!(tofobj->timer = wl_init_timer(wlc->wl, wlc_pdtof_timer,
			tofobj, "pdtof"))) {
			WL_ERROR(("Create pdtof timer failed\n"));
			goto err;
		}

		/* Reset state machine */
		wlc_pdtof_reset(tofobj->sm, tofobj->sm->tof_mode, TOF_REASON_INITIAL);
		tofobj->smstoped = TRUE;

		/* Get TOF shared memory address */
		tofobj->shmemptr = wlc_read_shm(wlc, M_TOF_PTR)*2;
		wlc_enable_avb_timer(wlc->hw, TRUE);

		/* initialize rssi calibration table */
		init_rssi_clb_data(tofobj);
	} else {
		WL_ERROR(("wl:%d %s MALLOC failed malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->pub->osh)));
	}

	return ((pdmthd_if_t *)tofobj);
err:
	if (tofobj)	{
		if (tofobj->duration_timer != NULL)
			wlc_hrt_free_timeout(tofobj->duration_timer);

		if (tofobj->sm)
			MFREE(wlc->osh,	tofobj->sm, sizeof(pdtof_sm_t));

		if (tofobj->tunep)
			MFREE(wlc->osh,	tofobj->tunep, sizeof(wl_proxd_params_tof_tune_t));

		if (tofobj->paramp)
			MFREE(wlc->osh,	tofobj->paramp, sizeof(wl_proxd_params_tof_method_t));

		MFREE(wlc->osh,	tofobj,	sizeof(pdtof_obj_t));
	}

	return NULL;
}

/* wl proxd_tune get command */
int
wlc_pdtof_get_tune(pdmthd_if_t* svcif, void *pbuf, int len)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;
	wl_proxd_params_tof_tune_t *tunep = pbuf;

	ASSERT(tofobj != NULL);
	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	if (len < sizeof(wl_proxd_params_tof_tune_t))
		return BCME_BUFTOOSHORT;

	bcopy(tofobj->tunep, pbuf, sizeof(wl_proxd_params_tof_tune_t));
	if (!tunep->Ki)
		tunep->Ki = wlc_pdtof_get_kval(tofobj, TRUE);
	if (!tunep->Kt)
		tunep->Kt = wlc_pdtof_get_kval(tofobj, FALSE);
	if (!tunep->N_log2)
		wlc_pdtof_get_log2_scale(tofobj, &tunep->N_log2, NULL);
	if (!tunep->N_scale)
		wlc_pdtof_get_log2_scale(tofobj, NULL, &tunep->N_scale);
	return BCME_OK;
}

/* wl proxd_tune set command */
int
wlc_pdtof_set_tune(pdmthd_if_t* svcif, void *pbuf, int len)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;
	wl_proxd_params_tof_tune_t *tunep = pbuf;
	int16 log2, scale;

	ASSERT(tofobj != NULL);
	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	if (len < sizeof(wl_proxd_params_tof_tune_t))
		return BCME_BUFTOOSHORT;

	if (tunep->Ki == wlc_pdtof_get_kval(tofobj, TRUE))
		tunep->Ki = 0;
	if (tunep->Kt == wlc_pdtof_get_kval(tofobj, FALSE))
		tunep->Kt = 0;

	wlc_pdtof_get_log2_scale(tofobj, &log2, &scale);
	if (tunep->N_scale == scale)
		tunep->N_scale = 0;
	if (tunep->N_log2 == log2)
		tunep->N_log2 = 0;
	bcopy(pbuf, tofobj->tunep, sizeof(wl_proxd_params_tof_tune_t));
	return BCME_OK;
}

/* wl proxd_ftmperiod get command */
int
wlc_pdtof_get_ftmperiod(pdmthd_if_t* svcif)
{
	int val = -1;
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;

	ASSERT(tofobj != NULL);
	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	val = (int)(tofobj->ftm_period);
	return val;
}

/* wl proxd_ftmperiod set command */
int
wlc_pdtof_set_ftmperiod(pdmthd_if_t* svcif, uint32 val)
{
	int err = BCME_UNSUPPORTED;
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;

	ASSERT(tofobj != NULL);
	CHECK_SIGNATURE(tofobj, PDSVC_TOF_MTHD_SIGNATURE);

	tofobj->ftm_period = val;
	err = BCME_OK;
	return err;
}

int wlc_pdtof_allowmac(pdmthd_if_t* svcif, struct ether_addr *addr)
{
	pdtof_obj_t* tofobj = (pdtof_obj_t*)svcif;

	bcopy(addr, &tofobj->allow_mac, ETHER_ADDR_LEN);

	return 0;
}
