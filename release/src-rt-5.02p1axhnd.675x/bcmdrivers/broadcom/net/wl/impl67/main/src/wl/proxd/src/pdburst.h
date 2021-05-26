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
 * $Id: pdburst.h 787663 2020-06-08 15:59:47Z $
 */

#ifndef _pdburst_h_
#define _pdburst_h_

#include <typedefs.h>
#include <wlc_types.h>

#include "pdsvc.h"
#include "pdftm.h"

#define PDBURST_UNIT(_burstp) WLCWLUNIT((_burstp)->wlc)

typedef struct pdburst pdburst_t;
typedef void   *pdburst_method_ctx_t;

/* parameters from protocol or local configuration */
struct  pdburst_config {
	struct ether_addr	peer_mac;
	chanspec_t		chanspec;
	uint16			rsvd;			/* chanspec expansion */
	uint32			tx_power;
	ratespec_t		ratespec;		/* override */
	wl_proxd_intvl_t	duration;		/* activity */
	wl_proxd_intvl_t	ftm_sep;
	wl_proxd_intvl_t	timeout;		/* for peer response */
	uint16			num_ftm;
	uint8			ftm_retries;
	uint8			ftm_req_retries;	/* request retries */
	struct ether_addr	cur_ether_addr;		/* source address for Tx */
	struct ether_addr	bssid;
};

struct pdburst_params {
	wl_proxd_method_t		method;
	wl_proxd_session_flags_t	flags;
	wlc_bsscfg_t			*bsscfg;
	const pdburst_config_t		*config;	/* valid for session lifetime */
	/* incoming req info, if any (on target), req points to unprocessed body */
	const uint8			*req;
	uint16				req_len;
	const wlc_d11rxhdr_t		*req_wrxh;
	ratespec_t			req_rspec;
	uint64				start_tsf;
	uint8				dialog;
};
typedef struct pdburst_params pdburst_params_t;

typedef uint64 pdburst_ts_t; /* pico seconds */
struct pdburst_tsinfo {
	uint8			tx_id;		/* id for this tx i.e. dialog */
	uint8			ts_id;		/* timestamps for this id i.e. follow up */
	pdburst_ts_t		tod;		/* measurement for frame tx */
	pdburst_ts_t		toa;		/* measurement for frame ack rx */
	pdburst_ts_t		tod_err;
	pdburst_ts_t		toa_err;
};
typedef struct pdburst_tsinfo pdburst_tsinfo_t;

/* callback support */

enum {
	PDBURST_FRAME_TYPE_REQ		= 1,
	PDBURST_FRAME_TYPE_MEAS		= 2,
	PDBURST_FRAME_TYPE_MEAS_END	= 3
};
typedef int8 pdburst_frame_type_t;
extern pdburst_frame_type_t pdburst_get_ftype(uint8 tof_type);

/* all callbacks return BCME_ status codes unless otherwise specified */

/* pdburst_get_frame_len_t provides support for obtaining the length of body
 * to allocate for a request or measurement frame. This callback is used
 * by burst manager to allocate the frame, taking into account any vendor
 * specific attributes it needs. Allocated frame is of type dot11_fc_kind that
 * is returned.
 */
typedef int (*pdburst_get_frame_len_t)(pdburst_method_ctx_t ctx,
	pdburst_frame_type_t type, uint16 *dot11_fc_kind);

/* pdburst_prep_tx_t is used to populate the body for the request or a measurement frame
 * with body_len containing the used length. burst manager may add vendor ies
 * before transmitting the request or measurement frame. The callback may
 * populate fields of the packet header. the type of packet management frame depends
 * on the method using the burst manager.
 */
typedef int (*pdburst_prep_tx_t)(pdburst_method_ctx_t ctx, pdburst_frame_type_t type, void *pkt,
	uint8 *body, uint body_max, uint *body_len, const pdburst_tsinfo_t *tsinfo);

/* pdburst_tx_done_t is used to indicate tx has been done along with its status
 * BCME_OK on sucessful tx, BCME_TXFAIL if failed (after retries), and
 * including WL_PROXD_E_NOACK if no ack was received.
 */
typedef int (*pdburst_tx_done_t)(pdburst_method_ctx_t ctx, wl_proxd_status_t status);

/* pdburst_measdone_t used to indicate rtt sample information derived from one
 * exchange. this method may be NULL if no rtt dample detail is desired. this
 * applies only to the initiator.
 */
typedef int (*pdburst_meas_done_t)(pdburst_method_ctx_t ctx,
	const wl_proxd_rtt_sample_t *rtt);

struct pdburst_results {
	wl_proxd_status_t	status;
	wl_proxd_result_flags_t	flags;
	wl_proxd_rtt_sample_t	avg_rtt;
	uint32			avg_dist;
	pd_distu_t		dist_unit;
	uint16			num_valid_rtt;
	uint16			sd_rtt;
	uint16			num_rtt;
	uint16			num_meas;
	wl_proxd_rtt_sample_t	rtt[1];		/* variable */
};
typedef struct pdburst_results pdburst_results_t;

/* pdburst_meas_done_t is used to indicate the completion status of a burst, along with
 * results if the burst completed successfully and the session is initiated from
 * the local device. avg_dist is indicated in terms of distance units.
 */
typedef int (*pdburst_done_t)(pdburst_method_ctx_t ctx, const pdburst_results_t *res);

/* pdburst_sn_info_t is used to get/set session related info. in out wrt proto */
enum {
	PDBURST_SESSION_FLAGS_LAST_BURST	= 0x00000001,	/* last burst - out */
	PDBURST_SESSION_FLAGS_MBURST_FOLLOWUP	= 0x00000002,	/* follow up across bursts- out */
	PDBURST_SESSION_FLAGS_SEQ_EN		= 0x00000004,	/* bcm toast - in/out */
	PDBURST_SESSION_FLAGS_VHTACK		= 0x00000008,	/* bcm in/out */
	PDBURST_SESSION_FLAGS_SEQ_START		= 0x00000010,	/* ltf start w/ - bcm in */
	PDBURST_SESSION_FLAGS_SECURE		= 0x00000020,	/* secure - bcm out */
	PDBURST_SESSION_FLAGS_QPSK		= 0x00000040,	/* qpsk/seq - bcm out */
	PDBURST_SESSION_FLAGS_TSPT1NS		= 0x00000080,	/* 0.1ns ts resolution - bcm out */
};
typedef uint16 pdburst_session_flags_t;

struct pdburst_session_info {
	pdburst_session_flags_t		flags;

	/* note: vs attributes valid only during prep_tx/rx callbacks */
	ftm_vs_req_params_t		*vs_req_params;
	ftm_vs_seq_params_t		*vs_seq_params;
	ftm_vs_sec_params_t		*vs_sec_params;
	ftm_vs_meas_info_t		*vs_meas_info;

	ftm_vs_mf_buf_t			*vs_mf_buf;		/* in/out */
	uint8				*vs_mf_buf_data;	/* mf buf data for in/out */
	uint16				vs_mf_buf_data_len;	/* mf buf len for in/out */
	uint16				vs_mf_buf_data_max;	/* allocation limit for rx */
	ftm_vs_timing_params_t		*vs_timing_params;
	ftm_vs_mf_buf_t			*vs_mf_stats_buf_hdr;	/* stats buffer in/out */
	uint8				*vs_mf_stats_buf_data;	/* mf stats buf data for in/out */
	uint16				vs_mf_stats_buf_data_len; /* mf stats buf len in/out */
	uint16				vs_mf_stats_buf_data_max; /* allocation limit for rx */
};
typedef struct pdburst_session_info pdburst_session_info_t;

typedef int (*pdburst_get_session_info_t)(
	pdburst_method_ctx_t ctx, pdburst_session_info_t *infop);
typedef int (*pdburst_set_session_info_t)(
	pdburst_method_ctx_t ctx, const pdburst_session_info_t *infop);

typedef int (*pdburst_vs_get_frame_len_t)(pdburst_method_ctx_t ctx,
	pdburst_frame_type_t type, const pdburst_session_info_t *bsi);

typedef int (*pdburst_vs_prep_tx_t)(pdburst_method_ctx_t ctx,
	pdburst_frame_type_t type, uint8 *body, uint body_max, uint *body_len,
	pdburst_session_info_t *bsi);

typedef int (*pdburst_vs_rx_t)(pdburst_method_ctx_t ctx, pdburst_frame_type_t type,
	const uint8 *body, uint body_len, pdburst_session_info_t *bsi);

#define CBDECL(X) pdburst_## X ## _t X
struct pdburst_callbacks {
	CBDECL(get_frame_len);
	CBDECL(prep_tx);
	CBDECL(tx_done);
	CBDECL(meas_done);
	CBDECL(done);
	CBDECL(get_session_info);
	CBDECL(set_session_info);
	CBDECL(vs_get_frame_len);
	CBDECL(vs_prep_tx);
	CBDECL(vs_rx);
};
#undef CBDECL
typedef struct pdburst_callbacks pdburst_callbacks_t;

extern uint8 ftm_vs_get_tof_txcnt(void *pdburstp);

void pdftm_set_burst_deferred(pdftm_t *ftm, pdftm_session_t *sn);

/* create a burst */
pdburst_t* pdburst_create(wlc_info_t *wlc, void *ctx, const pdburst_callbacks_t *callbacks);

/* initialize the burst, with specified parameters - params not guaranteed to be valid
 * when burst starts, but config is  (for efficiency). re-initialize burst parameters
 * during switch.
 */
int pdburst_init(pdburst_t *burst, const pdburst_params_t *params);

/* start or resume burst. callbacks must be asynchronous to this call, in the case
 * of initiator a trigger must be transmitted using prep_tx callback at
 * burst_tsf specified in the parameters. On target, the channel switch is performed
 * and the burst is suspended. when the trigger comes from the initiator
 * this function is called again to resume.
 */
int pdburst_start(pdburst_t *burst);

/* suspend a burst e.g. caller may request delay */
int pdburst_suspend(pdburst_t *burst);

/* receive a measurement frame - body and body_len indicate remaining length after
 * method has processed the frame. timestamps from peer are provided in tsinfo. tod/toa
 * may overflow 48-bits (e.g. ftm not-continuous bit in error)
 */
int pdburst_rx(pdburst_t *burst, wlc_bsscfg_t *bsscfg,
	const dot11_management_header_t *hdr,
	const uint8 *body, int body_len,
	const wlc_d11rxhdr_t *wrxh, ratespec_t rspec,
	const pdburst_tsinfo_t *tsinfo);

/* destroy a burst */
void pdburst_destroy(pdburst_t **burst);

/* cancel an existing burst */
void pdburst_cancel(pdburst_t* burstp, wl_proxd_status_t reason);

/* dump a burst */
void pdburst_dump(const pdburst_t *burst, struct bcmstrbuf *b);

/* note on collect, debugging
 * pdburst may implement debugging/collect support by adding appropriate vendor
 * attributes to tx and processing vendor attributes from rx
 */

void pdburst_process_tx_rx_status(wlc_info_t *wlc, tx_status_t *txs,
	d11rxhdr_t *rxh, struct ether_addr *peer);

/* this definition is used for visibility with respect to PCB registration */
extern void pdburst_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg);

extern int ftm_vs_unpack_mf_stats_buf(pdftm_session_t *sn, const uint8* buf, uint len,
	pdburst_session_info_t *bsi);
extern uint8*  ftm_vs_init_mf_stats_buf(void * burstp);
extern void ftm_vs_update_mf_stats_len(void *burstp, uint16 len);

#endif /* _pdburst_h_ */
