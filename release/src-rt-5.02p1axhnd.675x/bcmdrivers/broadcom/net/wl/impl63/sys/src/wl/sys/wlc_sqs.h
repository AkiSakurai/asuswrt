/*
 * Single stage Queuing and Scheduling module header file
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 */

#ifndef _WLC_SQS_H_
#define _WLC_SQS_H_

#define WLC_SQS_NOOP                    do { /* noop */ } while (0)

/** debug prints */
#define WLC_SQS_ERROR(args)             printf args
#ifdef SQS_DEBUG
#define WLC_SQS_DEBUG(args)             printf args
#else
#define WLC_SQS_DEBUG(args)             WLC_SQS_NOOP
#endif // endif

/** SCB SQS Cubby Accessors given a scb_sqs_t handle. Use these Accessors! */
#define SCB_SQS_FLOWID(hdl)             ((hdl)->flowid)
#define SCB_SQS_FLAGS(hdl)              ((hdl)->flags)
#define SCB_SQS_SCB(hdl)                ((hdl)->scb)

#define SCB_SQS_AMPDU_TX(hdl)		((hdl)->scb_ampdu_tx)
/* End of pull request set call back fn ptr */
typedef int (*eops_rqst_cb_t)(void *arg);

typedef struct {
	eops_rqst_cb_t cb;
	void *arg;
} eops_rqst_cb_info_t;

/* Packet pull call back fn pointer */
typedef uint16 (*pkt_pull_cb_t)(void *arg, uint16 ringid, uint16 request_cnt);

typedef struct {
	pkt_pull_cb_t cb;
	void *arg;
} pkt_pull_cb_info_t;

/* Flow ring status  call back fn pointer */
typedef bool (*flow_ring_status_cb_t)(void *arg, uint16 ringid);

typedef struct {
	flow_ring_status_cb_t cb;
	void *arg;
} flow_ring_status_cb_info_t;

/** Single stage Queuing and Scheduling module */
struct wlc_sqs_info {
	wlc_info_t	*wlc;			   /**< Back pointer to wl */
	pkt_pull_cb_info_t	pkt_pull_cb_info;  /**< Packet pull fn registered with BUS layer */
	eops_rqst_cb_info_t	eops_rqst_cb_info; /** < EOPS rqst call back function */
	flow_ring_status_cb_info_t flow_ring_status_cb_info; /** Flow ring status CB */
	uint16			v2r_intransit;	   /** < Outstanding V2R across all flows */
	uint16			eops_intransit;    /** < Outstanding EoPS Request cnt */
	int			scb_hdl;	   /** < SCB cubby hdl */
};

typedef struct sqs_virt_q {
	uint16 v_pkts;		/* current virtual Packets */
	uint16 v2r_pkts;	/* Current V2R count */
	uint16 v_pkts_max;	/* Debug - Max V pkts enqueud at any time */
	uint32 cum_pkts;	/* Debug - Cummulative pkts processed by SQS */
} sqs_virt_q_t;

typedef struct scb_sqs {
	void			*scb;			/**< SCB pointer */
	struct wlc_sqs_info*	sqs_info;		/**< Back ptr to sqs_info */
	sqs_virt_q_t		q[NUMPRIO];		/**< Virtual Q counters */
} scb_sqs_t;
#define WLC_SQS_SIZE	(sizeof(wlc_sqs_info_t))
#define SCB_SQS_SIZE	(sizeof(scb_sqs_t))

/* Utility Macros */
#define SCB_SQS_SCB(hdl)	((hdl)->scb)
#define SCB_SQS_INFO(hdl)	((hdl)->sqs_info)
#define SCB_SQS_Q(hdl, prio)	((hdl)->q[(prio)])
#define SCB_SQS_V_PKTS(hdl, prio)	(SCB_SQS_Q((hdl), (prio)).v_pkts)
#define SCB_SQS_V_PKTS_MAX(hdl, prio)	(SCB_SQS_Q((hdl), (prio)).v_pkts_max)
#define SCB_SQS_V2R_PKTS(hdl, prio)	(SCB_SQS_Q((hdl), (prio)).v2r_pkts)
#define SCB_SQS_CUM_V_PKTS(hdl, prio)	(SCB_SQS_Q((hdl), (prio)).cum_pkts)

#define WLC_SQS_WLC(sqs)      (((wlc_sqs_info_t *)sqs)->wlc)
#define WLC_SQS_PKTPULL_CB_INFO(sqs)	(((wlc_sqs_info_t *)sqs)->pkt_pull_cb_info)
#define WLC_SQS_PKTPULL_CB_FN(sqs)	((WLC_SQS_PKTPULL_CB_INFO(sqs)).cb)
#define WLC_SQS_PKTPULL_CB_ARG(sqs)	((WLC_SQS_PKTPULL_CB_INFO(sqs)).arg)

#define WLC_EOPS_RQST_CB_INFO(sqs)	(((wlc_sqs_info_t *)sqs)->eops_rqst_cb_info)
#define WLC_EOPS_RQST_CB_FN(sqs)	((WLC_EOPS_RQST_CB_INFO(sqs)).cb)
#define WLC_EOPS_RQST_CB_ARG(sqs)	((WLC_EOPS_RQST_CB_INFO(sqs)).arg)

#define WLC_SQS_FLRING_STS_CB_INFO(sqs)	(((wlc_sqs_info_t *)sqs)->flow_ring_status_cb_info)
#define WLC_SQS_FLRING_STS_CB_FN(sqs)	((WLC_SQS_FLRING_STS_CB_INFO(sqs)).cb)
#define WLC_SQS_FLRING_STS_CB_ARG(sqs)	((WLC_SQS_FLRING_STS_CB_INFO(sqs)).arg)

#define WLC_SQS_V2R_INTRANSIT(sqs)	(((wlc_sqs_info_t *)(sqs))->v2r_intransit)
#define WLC_SQS_EOPS_INTRANSIT(sqs)	(((wlc_sqs_info_t *)(sqs))->eops_intransit)

extern wlc_sqs_info_t *wlc_sqs_attach(wlc_info_t *wlc);
extern void wlc_sqs_detach(wlc_sqs_info_t *sqs_info);

/** Wireless SQS entry point. */
extern int wlc_sqs_sendup(uint16 sqs_flowid, uint8 prio, uint16 pkt_count);

extern uint16 wlc_sqs_vpkts(uint16 sqs_flowid, uint8 prio);
extern uint16 wlc_sqs_v2r_pkts(uint16 cfp_flowid, uint8 prio);
extern void wlc_sqs_v2r_enqueue(uint16 cfp_flowid, uint8 prio, uint16 pkt_count);
extern void wlc_sqs_v2r_dequeue(uint16 cfp_flowid, uint8 prio, uint16 pkt_count, bool sqs_force);
extern void wlc_sqs_vpkts_enqueue(uint16 cfp_flowid, uint8 prio, uint16 v_pkts);
extern void wlc_sqs_vpkts_rewind(uint16 cfp_flowid, uint8 prio, uint16 count);
extern void wlc_sqs_vpkts_forward(uint16 cfp_flowid, uint8 prio, uint16 count);

extern int wlc_sqs_pktq_plen(struct pktq *pktq, uint8 prio, bool amsdu_in_ampdu);
extern void *wlc_sqs_pktq_release(struct scb* scb, struct pktq *pktq, uint8 prio,
	int pkts_release, int *v2r_request, bool amsdu_in_ampdu, int max_pdu);
extern bool wlc_sqs_fifo_paused(uint8 prio);
extern void wlc_sqs_pull_packets_register(pkt_pull_cb_t cb, void* arg);
extern void wlc_sqs_flowring_status_register(flow_ring_status_cb_t cb, void* arg);
extern void wlc_sqs_taf_txeval_trigger(void);
extern void wlc_sqs_eops_rqst(void);
extern void wlc_sqs_eops_response(void);

#ifdef WLTAF
extern void * wlc_sqs_taf_get_handle(wlc_info_t* wlc);
void * wlc_sqs_taf_get_scb_info(void *sqsh, struct scb* scb);
void * wlc_sqs_taf_get_scb_tid_info(void *scb_h, int tid);
uint16 wlc_sqs_taf_get_scb_tid_pkts(void *scbh, void *tidh);
bool wlc_sqs_taf_release(void* sqsh, void* scbh, void* tidh, bool force,
	taf_scheduler_public_t* taf);
#endif /* WLTAF */

#endif /* _WLC_SQS_H_ */
