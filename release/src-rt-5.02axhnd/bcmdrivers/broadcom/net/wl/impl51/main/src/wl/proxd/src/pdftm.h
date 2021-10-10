/*
 * Proxd FTM method support. See twiki FineTimingMeasurement.
 * This header is internal to proxd and specifies s/w interface to FTM
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
 * $Id: pdftm.h 778924 2019-09-13 19:33:40Z $
 */

#ifndef _pdftm_h_
#define _pdftm_h_

#include <typedefs.h>
#include <bcmutils.h>
#include <wlc_types.h>
#include <wlioctl.h>
#ifdef WLSLOTTED_BSS
#include <wlc_slotted_bss.h>
#endif /* WLSLOTTED_BSS */

typedef struct pdftm pdftm_t;
typedef struct pdftm_cmn pdftm_cmn_t;
typedef struct pdftm_session pdftm_session_t;
typedef struct pdburst_config pdburst_config_t;

/* convenience macros */
#define FTM_TU2MICRO(_tu) ((uint64)(_tu) << 10)
#define FTM_MICRO2TU(_tu) ((uint64)(_tu) >> 10)
#define FTM_TU2MILLI(_tu) ((uint32)FTM_TU2MICRO(_tu) / 1000)
#define FTM_MICRO2MILLI(_x) ((uint32)(_x) / 1000)
#define FTM_MICRO2SEC(_x) ((uint32)(_x) / 1000000)
#define FTM_INTVL2PSEC(_intvl) ((uint32)pdftm_intvl2psec(_intvl))
#define FTM_INTVL2NSEC(_intvl) ((uint32)pdftm_intvl2nsec(_intvl))
#define FTM_INTVL2USEC(_intvl) ((uint32)pdftm_intvl2usec(_intvl))
#define FTM_INTVL2MSEC(_intvl) (FTM_INTVL2USEC(_intvl) / 1000)
#define FTM_INTVL2SEC(_intvl) (FTM_INTVL2USEC(_intvl) / 1000000)
#define FTM_USECIN100MILLI(_usec) ((_usec) / 100000)
#define FTM_INIT_INTVL(_intvl, _val, _tmu) {\
	(_intvl)->intvl = (_val); (_intvl)->tmu = (_tmu); }

#define	BURST_DELAY_IMMEDIATE 3000  /* usec */

/* Constants for secure ranging */
#define FTM_TPK_MAX_LEN	16

#ifdef FTM_TPK_RI_PHY_LEN
#undef FTM_TPK_RI_PHY_LEN
#endif /* FTM_TPK_RI_PHY_LEN */

#ifdef FTM_TPK_RR_PHY_LEN
#undef FTM_TPK_RR_PHY_LEN
#endif /* FTM_TPK_RR_PHY_LEN */

#define FTM_TPK_RI_PHY_LEN  7	/* 6.5 */
#define FTM_TPK_RR_PHY_LEN  7

/* MF-BUF defines */
#define MF_BUF_MAX_LEN	512

/* begin vendor specific types */
#define FTM_VS_TPK_RI_LEN 8
#define FTM_VS_TPK_RR_LEN 8
#define FTM_VS_TPK_RI_LEN_SECURE_2_0 16
#define FTM_VS_TPK_RR_LEN_SECURE_2_0 16
#define FTM_VS_TPK_BUF_LEN 128
#define FTM_VS_TPK_RAND_LEN 32

enum {
	FTM_VS_F_NONE			= 0x0000,
	FTM_VS_F_VALID			= 0x0001,
	FTM_VS_F_TSPT1NS		= 0x0002,
	FTM_VS_F_SEC_RANGING_2_0	= 0x0004
};

/* vs req params  - FTM_VS_TLV_REQ_PARAMS- see 802.11.h */
enum {
	FTM_VS_REQ_F_NONE		= FTM_VS_F_NONE,
	FTM_VS_REQ_F_VALID		= FTM_VS_F_VALID,
	FTM_VS_REQ_F_SEQ_EN		= 0x0002,
	FTM_VS_REQ_F_VHTACK		= 0x0004,
	FTM_VS_REQ_F_SECURE		= 0x0008,
	FTM_VS_REQ_F_TSPT1NS		= 0x0010
};

struct ftm_vs_req_params {
	uint16	flags;
	uint8	totfrmcnt;	/* total frames to send */
	uint8	ftm_retries;	/* number of retries per ftm */
	uint32	chanspec;	/* chanspec_t */
};
typedef struct ftm_vs_req_params ftm_vs_req_params_t;

/* vs meas options/flags */
enum {
	FTM_VS_MEAS_F_NONE		= FTM_VS_F_NONE,
	FTM_VS_MEAS_F_VALID		= FTM_VS_F_VALID,
	FTM_VS_MEAS_F_TSPT1NS		= 0x2u	/* 0.1ns timestamps */
};

struct ftm_vs_meas_info {
	uint16			flags;
	wl_proxd_phy_error_t	phy_err;	/* phy error flag for target */
};
typedef struct ftm_vs_meas_info ftm_vs_meas_info_t;

/* vs seq params  - FTM_VS_TLV_SEQ_PARAMS- see 802.11.h */
enum {
	FTM_VS_SEQ_F_NONE	= FTM_VS_F_NONE,
	FTM_VS_SEQ_F_VALID	= FTM_VS_F_VALID,
	FTM_VS_SEQ_F_QPSK	= 0x0002		/* not yet */
};

struct ftm_vs_seq_params {
	uint16	flags;
	uint8	d[3];
	uint8	cores;		/* mask of cores to use */
};
typedef struct ftm_vs_seq_params ftm_vs_seq_params_t;

/* vs security params FTM_VS_TLV_SEC_PARAMS- see 802.11.h */
enum {
	FTM_VS_SEC_F_NONE		= FTM_VS_F_NONE,
	FTM_VS_SEC_F_VALID		= FTM_VS_F_VALID,
	FTM_VS_SEC_F_RANGING_2_0	= FTM_VS_F_SEC_RANGING_2_0
};

struct ftm_vs_sec_params {
	uint16	flags;
	uint8	rsvd;
	uint8	rlen;					/* length of ri or rr */
	uint8	ri[FTM_VS_TPK_RI_LEN_SECURE_2_0];	/* ri rr-must be together in that order */
	uint8	rr[FTM_VS_TPK_RR_LEN_SECURE_2_0];	/* must be of same length as ri */
};
typedef struct ftm_vs_sec_params ftm_vs_sec_params_t;

/* vs multi-frame buffer - may span ftm vs ies */
enum {
	FTM_VS_MF_BUF_F_NONE	= FTM_VS_F_NONE,
	FTM_VS_MF_BUF_F_VALID	= FTM_VS_F_VALID
};

struct ftm_vs_mf_buf {
	uint16 flags;
	uint16 offset;
	uint16 total;
	uint8  data[1]; /* variable */
};
typedef struct ftm_vs_mf_buf ftm_vs_mf_buf_t;

struct ftm_vs_timing_params {
	/* start the ranging sequence on the initiator after x usec,
	 * relative to the end of the ack tx
	 */
	uint16  start_seq_time;
	/*  Delta (us) from the start of the ranging
	 *  sequence sent by the target to the start of
	 *  the ranging sequence sent by the initiator
	 */
	uint16  delta_time_tx2rx;
};
typedef struct ftm_vs_timing_params ftm_vs_timing_params_t;

#ifdef WL_RANGE_SEQ
/* vendor specific info */
enum {
	FTM_VS_INFO_NONE        = 0x00,
	FTM_VS_INFO_VHTACK      = 0x01
};
typedef uint32 ftm_vs_info_t;
#endif /* WL_RANGE_SEQ */

#define FTM_VS_PARAMS_VALID(_params) (((_params)->flags & FTM_VS_F_VALID) != 0)
#define FTM_VS_PARAMS_VALID2(_params) (((_params) != NULL) && FTM_VS_PARAMS_VALID(_params))

#define FTM_VS_TX_MEAS_INFO(_bsi) ((_bsi != NULL) && \
	FTM_VS_PARAMS_VALID2(_bsi->vs_meas_info))

#ifdef WL_FTM_VS_MF_BUF
#define FTM_VS_TX_MF_BUF(_sn, _type, _bsi) ((_bsi != NULL) && \
	FTM_SESSION_SEQ_EN(sn) && ((_type) == PDBURST_FRAME_TYPE_MEAS) && \
	bsi->vs_mf_buf_data && bsi->vs_mf_buf_data_len && \
	FTM_VS_PARAMS_VALID2(bsi->vs_mf_buf))
#else
#define FTM_VS_TX_MF_BUF(_sn, _type, _bsi) FALSE
#endif /* WL_FTM_VS_MF_BUF */

#define FTM_SESSION_SEQ_EN(_sn) (\
	((_sn)->config->flags & WL_PROXD_SESSION_FLAG_SEQ_EN) != 0)

#ifdef WL_FTM_SECURITY
#define FTM_SESSION_ACTION_CAT(_sn) (\
	(((_sn)->config->flags & WL_PROXD_SESSION_FLAG_SECURE) != 0) ? \
	DOT11_ACTION_CAT_PDPA : DOT11_ACTION_CAT_PUBLIC)
#else
#define FTM_SESSION_ACTION_CAT(_sn) DOT11_ACTION_CAT_PUBLIC
#endif /* WL_FTM_SECURITY */

#define FTM_TX_OVRHD_BURST_DURATION_US 2000

/* end vendor specific types */

/* All prototypes return a BCME_ status unless otherwise specified */

/* create/allocate and return the ftm method */
pdftm_t* pdftm_attach(wlc_info_t *wlc, pdsvc_t *svc /* pdsvc_funcs_t * TBD */);

/* deallocate the ftm method, clear ftm */
void pdftm_detach(pdftm_t *ftm);

/* notification support */
enum {
	PDFTM_NOTIF_NONE		= 0,
	PDFTM_NOTIF_RSVD		= 1,
	PDFTM_NOTIF_EVENT_TYPE		= 2,
	PDFTM_NOTIF_AVAIL_UPDATE	= 3,
	PDFTM_NOTIF_BSS_UP		= 4,
	PDFTM_NOTIF_BSS_DOWN		= 5,
	PDFTM_NOTIF_MAX
};
typedef int pdftm_notify_t;

struct pdftm_notify_info {
	pdftm_session_t			*sn;
	const struct ether_addr		*addr;		/* peer address */
	wl_proxd_event_type_t		event_type;
	uint8				pad1[2];	/* pad event_type to 4 bytes */
	chanspec_t			chanspec;
	uint8				pad2[2];	/* pad chanspec to 4 bytes */
	ratespec_t			ratespec;
	const uint8			*data;
	uint				data_len;
	wlc_ftm_ranging_ctx_t		*rctx;
};
typedef struct pdftm_notify_info pdftm_notify_info_t;

/* prototypes */

/* notify (to ftm) */
void pdftm_notify(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	pdftm_notify_t notif, pdftm_notify_info_t *info);

/* iovar operations specific to ftm method */
int pdftm_get_iov(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len);

int pdftm_set_iov(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len);

bool pdftm_is_ftm_action(pdftm_t *ftm, const dot11_management_header_t *hdr,
	uint8 *body, uint body_len);

/* process a received action frame */
int pdftm_rx(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, dot11_management_header_t *hdr,
	uint8 *body, uint body_len,
	wlc_d11rxhdr_t *wrxh, ratespec_t rspec);

/* dump configuration and state */
void pdftm_dump(const pdftm_t *ftm, struct bcmstrbuf *b);

/* interval conversion support */
uint64 pdftm_intvl2psec(const wl_proxd_intvl_t *intvl);
uint64 pdftm_intvl2nsec(const wl_proxd_intvl_t *intvl);
uint64 pdftm_intvl2usec(const wl_proxd_intvl_t *intvl);

/* event (from ftm) support */

typedef struct pdftm_event_data pdftm_event_data_t;
typedef wlc_ftm_event_callback_t pdftm_event_callback_t;

/* get burst config for proxd_tune command. session defaults if sn is NULL */
const pdburst_config_t * pdftm_get_burst_config(pdftm_t *ftm,
	pdftm_session_t *sn, wl_proxd_session_flags_t *flags);

/* Vendor Specific support */

/* transmit a FTM Initiator-Report to peer-target */
int pdftm_vs_tx_initiator_rpt(pdftm_t *ftm, pdftm_session_t *sn);

/* check for FTM Vendor-Specific Action Frame (e.g. Initiator Report) */
bool pdftm_vs_is_ftm_action(pdftm_t *ftm, const dot11_management_header_t *hdr,
	uint8 *body, uint body_len);

/* handle receiving a FTM Vendor Specific Action Frame */
int pdftm_vs_rx_frame(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	dot11_management_header_t *hdr, uint8 *body, uint body_len,
	wlc_d11rxhdr_t *wrxh, ratespec_t rspec);

#ifdef WL_RANGE_SEQ
/* update the vendor specific info to the session */
int pdftm_vs_info_update(pdftm_session_t *sn, ftm_vs_info_t vsinfo);
#endif /* WL_RANGE_SEQ */

/* utils */
uint64 pdftm_div64(uint64 val, uint16 div);
#ifdef WLSLOTTED_BSS
int pdftm_ext_sched_cb(void *ctx, slotted_bss_notif_data_t *cb_data);
#endif /* WLSLOTTED_BSS */
#endif /* _pdftm_h_ */
