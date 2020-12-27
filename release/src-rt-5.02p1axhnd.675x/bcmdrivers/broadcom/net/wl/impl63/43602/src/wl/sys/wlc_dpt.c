/*
 * DPT (Direct Packet Transfer between 2 STAs) source file
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
 * $Id: wlc_dpt.c 593242 2015-10-15 20:31:28Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifndef WLDPT
#error "WLDPT is not defined"
#endif	/* WLDPT */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_ie_misc_hndlrs.h>
#include <wlc_phy_hal.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_sup.h>
#include <wlc_auth.h>
#include <wlc_dpt.h>
#include <wlc_scan.h>
#include <wlc_ampdu.h>
#include <wlc_frmutil.h>
#include <wl_export.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#ifdef WL_BCN_COALESCING
#include <wlc_bcn_clsg.h>
#endif /* WL_BCN_COALESCING */

/* iovar table */
enum {
	IOV_DPT,		/* enable/disable dpt */
	IOV_DPT_DISCOVERY,	/* DPT discovery mode: manual|auto */
	IOV_DPT_DISCOVERABLE,	/* DPT device discoverable: true|false */
	IOV_DPT_DENY,		/* create dpt deny entry for given macaddr */
	IOV_DPT_ENDPOINT,	/* create/mod/del dpt manual endpoint */
	IOV_DPT_PATHSEL,	/* set default path selection */
	IOV_DPT_PROBE_FREQ,	/* minimum probe freq (in secs) */
	IOV_DPT_MIN_RSSI,	/* minimum RSSI needed for DPT link */
	IOV_DPT_MAX_SESSIONS,	/* max # of dpt connections */
	IOV_DPT_WSEC,		/* encryption on DPT link; none or AES */
	IOV_DPT_WPA_AUTH,	/* auth on DPT link; WPA2PSK only */
	IOV_DPT_PMK,		/* pre shared key for DPT link */
	IOV_DPT_LIST,		/* list of active DPT stations and the status */
	IOV_DPT_FNAME,		/* friendly name for DPT link */
};

static const bcm_iovar_t dpt_iovars[] = {
	{"dpt", IOV_DPT, 0, IOVT_BOOL, 0},
	{"dpt_discovery", IOV_DPT_DISCOVERY, 0, IOVT_UINT32, 0},
	{"dpt_discoverable", IOV_DPT_DISCOVERABLE, 0, IOVT_BOOL, 0},
	{"dpt_pathsel", IOV_DPT_PATHSEL, 0, IOVT_UINT32, 0},
	{"dpt_deny", IOV_DPT_DENY, 0, IOVT_BUFFER, sizeof(dpt_iovar_t)},
	{"dpt_endpoint", IOV_DPT_ENDPOINT, 0, IOVT_BUFFER, sizeof(dpt_iovar_t)},
	{"dpt_probe_freq", IOV_DPT_PROBE_FREQ, 0, IOVT_UINT32, 0},
	{"dpt_min_rssi", IOV_DPT_MIN_RSSI, 0, IOVT_INT32, 0},
	{"dpt_max_sessions", IOV_DPT_MAX_SESSIONS, 0, IOVT_UINT32, 0},
	{"dpt_wsec", IOV_DPT_WSEC, 0, IOVT_UINT32, 0},
	{"dpt_wpa_auth", IOV_DPT_WPA_AUTH, 0, IOVT_UINT32, 0},
	{"dpt_pmk", IOV_DPT_PMK, 0, IOVT_BUFFER, sizeof(wsec_pmk_t)},
	{"dpt_list", IOV_DPT_LIST, 0, IOVT_BUFFER, 0},
	{"dpt_fname", IOV_DPT_FNAME, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0}
};

#define DPT_PROBE_FREQ		15	/* max probe sending freq (in secs) */
#define DPT_HEARTBEAT_FREQ	3	/* max heartbeat freq (in secs) */
#define DPT_PATHSEL_FREQ	60	/* max path selection freq (in secs) */
#define	DPT_HB_TIMEOUT		15	/* heart beat timeout */
#define DPT_SCB_AGE_THRESHOLD	10	/* scb age threshold for active reclaim */
#define DPT_SCB_USE_TIMEOUT	180	/* time (secs) with no data after which scb is freed */
#define DPT_RSSI_THRESHOLD	-80	/* rssi threshold for dpt link */
#define DPT_LOOKASIDE_SIZE	10	/* max dpt lookaside entries */
#define	DPT_LS_PENDING_TIMEOUT	3	/* time (in sec) for which to wait */

/* flags for DPT scb */
#define DPT_SCB_ACTIVE		0x0001	/* DPT link active and used */
#define DPT_SCB_MANUAL		0x0002	/* DPT was created in manual mode */
#define DPT_SCB_CON_PENDING	0x0004	/* DPT connection in progress */
#define DPT_SCB_CONNECTED	0x0008	/* DPT connection with peer exists */
#define DPT_SCB_SP_PENDING	0x0010	/* DPT link pending sp notification */
#define DPT_SCB_SUSPENDED	0x0020	/* DPT link suspended */
#define DPT_SCB_LS_PENDING	0x0040	/* DPT link pending ls notification */
#define DPT_SCB_PEER_SUSPENDED	0x0080	/* DPT link suspended */
#define DPT_SCB_PORT_OPEN	0x0100	/* DPT link ready for data */

#define DPT_SCBFLAGS_STR \
	"active", \
	"manual", \
	"con_pending", \
	"connected", \
	"sp_pending", \
	"suspended", \
	"ls_pending", \
	"peer_suspended", \
	"port_open", \

/* DPT security mode */
#define	DPT_SEC_NONE		0	/* no security */
#define	DPT_SEC_SUPPLICANT	1	/* supplicant mode */
#define	DPT_SEC_AUTHENTICATOR	2	/* authenticator mode */

/* flags for lookaside entries */
#define DPT_LOOKASIDE_INUSE 	0x1	/* entry in use */
#define DPT_LOOKASIDE_PERM 	0x2	/* persistent entry */

#define DPT_PATHSEL_STR \
	"auto", \
	"appath", \
	"direct"

/* dpt related stats */
typedef struct wlc_dpt_cnt {
	uint32 txprbreq;	/* dpt probe req sent */
	uint32 txprbresp;	/* dpt probe resp sent */
	uint32 rxdptprbreq;	/* dpt probe req recd */
	uint32 rxdptprbresp;	/* dpt probe resp recd */
	uint32 rxallprbreq;	/* all probe req recd */
	uint32 rxallprbresp;	/* all probe resp recd */
	uint32 discard;		/* frames discarded due to full buffer */
	uint32 ubuffered;	/* frames buffered by DPT txmod */
	uint32 buf_reinserted;	/* frames reinserted */
} wlc_dpt_cnt_t;

/* structure to store information about a dpt peer that has not yet
 * responded
 */
typedef struct dpt_lookaside {
	struct ether_addr ea;	/* mac address of dpt peer */
	uint8 prb_sent;		/* time at which last probe was sent */
	uint8 flags;		/* flags for the connection */
} dpt_lookaside_t;

/* DPT module specific state */
struct dpt_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	int scb_handle;		/* scb cubby handle to retrieve data from scb */
	uint16 cap;		/* DPT capability (advertised in ie) */
	uint8 pathsel;		/* path selection: auto|ap|direct */
	uint8 discovery;	/* dpt discovery mode: manual|auto|scan */
	uint8 discoverable;	/* dpt device discoverable */
	int16 rssi_threshold;	/* rssi threshold for dpt link */
	uint8 prb_freq;		/* max probe frequency */
	uint8 hb_freq;		/* max heart beat frequency */
	uint8 pathsel_freq;	/* max path selection change frequency */
	uint8 token;		/* id for the dpt messages */
	uint8 ls_pending;	/* num of peers from which waiting for LS ack */
	uint8 po_pending;	/* num of peers waiting for port to be opened */
	uint max_sessions;	/* max number of dpt sessions */
	uint cur_sessions;	/* cur number of dpt sessions */
	uint scb_age_threshold;	/* scb age threshold (in sec) */
	dpt_lookaside_t lookaside[DPT_LOOKASIDE_SIZE];	/* lookaside entries */
#ifdef WLCNT
	wlc_dpt_cnt_t *cnt;	/* counters/stats */
#endif /* WLCNT */
};

/* scb cubby structure */
typedef struct scb_dpt {
	uint16 flags;		/* flags for the connection */
	uint path_switch;	/* time at which pathsel caused path switch */
	uint hb_sent;		/* time at which last heart beat was sent */
	uint hb_rcvd;		/* time at which last heart beat was recd */
	uint used;		/* time at which used by dpt data */
	uint8 sec_mode;		/* none or supplicant or authenticator */
	uint8 pathsel;		/* path selection mode */
	uint8 con_token;	/* connection request token */
	uint8 sp_token;		/* pending active token for switch path */
	uint8 ls_token;		/* link suspend token */
	uint8 ls_pending_time;	/* remaining time for ls_pending state */
	bool free_me;		/* free scb at next watchdog */
	uint16 peer_ies_len;	/* length of saved peer ies */
	uint8 *peer_ies;	/* peer ies */
	uint8 fnlen;		/* length of friendly name */
	uint8 fname[DPT_FNAME_LEN+1];	/* friendly name */
	wlc_bsscfg_t *parent;	/* Infra STA bsscfg */
	struct pktq ubufq;	/* buffered queue */
} scb_dpt_t;

/* actual pointer in the cubby */
typedef struct scb_dpt_cubby {
	scb_dpt_t *cubby;
} scb_dpt_cubby_t;

#define SCB_DPT_CUBBY(dpt, scb) (scb_dpt_cubby_t *)SCB_CUBBY((scb), (dpt)->scb_handle)
#define SCB_DPT(dpt, scb) (SCB_DPT_CUBBY(dpt, scb))->cubby

/* DPT types in the DPT ie TLVs */
#define DPT_IE_TYPE_FNAME	0	/* friendly name of DPT device */
#define DPT_IE_TYPE_BSSID	1	/* BSSID of parent BSS */

/* DPT Capabilities */
#define DPT_CAP_IN_NETWORK	0x01	/* capable of in-network connections */
#define DPT_CAP_OUT_OF_NETWORK	0x02	/* capable of out-of--network connections */

#define DPT_PACKET_VERSION	1	/* Initial version */

/* different types of DPT protocol frames */
#define DPT_TYPE_CONNECT	0	/* connect frame req/rep/discon */
#define DPT_TYPE_SWITCH_PATH	1	/* switch path frame */
#define DPT_TYPE_LINK_STATE	2	/* link state frame susp/resume */
#define DPT_TYPE_HEART_BEAT	3	/* heart beat frame */

/* subtypes of CONNECT frames */
#define DPT_ST_CONNECT_REQ	0	/* connect request */
#define DPT_ST_CONNECT_RESP	1	/* connect resp */
#define DPT_ST_DISCONNECT	2	/* disconnect req */

/* subtypes of SWITCH_PATH frames (sent by initiator peer) */
#define DPT_ST_SP_DIRECT_TO_AP	0	/* Direct to AP Path switch */
#define DPT_ST_SP_AP_TO_DIRECT	1	/* AP Path to Direct switch */
#define DPT_ST_SP_ACK		2	/* Ack for switch path */

/* subtypes of LINK_STATE frames (sent by responder peer) */
#define DPT_ST_LS_SUSPEND	0	/* suspend link */
#define DPT_ST_LS_RESUME	1	/* resume tx on link */
#define DPT_ST_LS_ACK		2	/* Ack for LS frame */

/* subtypes of heart beat; well, only 1 subtype */
/* XXX: Future HB may not be a Data frame. May be sent as a beacon or
 * could be configurable
 */
#define DPT_ST_HB		0	/* heart beat */

/* status of DPT messages */
#define DPT_PKT_STATUS_SUCCESS	0	/* Req successful */
#define DPT_PKT_STATUS_FAILED	1	/* Req failed */

#ifdef BCMDBG
const char *dpt_type_str[] = { "CON", "SWITCH_PATH", "LINK_STATE", "HEART_BEAT" };
const char *dpt_st_con_str[] = { "CONNECT_REQ", "CONNECT_RESP", "DISCONNECT" };
const char *dpt_st_sp_str[] = { "DIRECT_TO_AP", "AP_TO_DIRECT", "ACK" };
const char *dpt_st_ls_str[] = { "SUSPEND", "RESUME", "ACK" };
const char *dpt_st_hb_str[] = { "HB" };
#endif // endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* fixed part of DPT packet header; variable length IEs will follow this  */
BWL_PRE_PACKED_STRUCT struct dpt_packet_s {
	uint8	ver;		/* protocol version */
	uint8	token;		/* for matching responses */
	uint8	type;		/* types of frame */
	uint8	subtype;	/* subtype */
	uint8	status;		/* status of req */
	uint8	rsvd[3];	/* for growth */
} BWL_POST_PACKED_STRUCT;
typedef struct dpt_packet_s dpt_packet_t;

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#define	DPT_PACKET_LEN		8	/* fixed length of DPT pkt hdr */

/* local prototypes */
static int scb_dpt_init(void *context, struct scb *scb);
static void scb_dpt_deinit(void *context, struct scb *scb);
static int wlc_dpt_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_dpt_watchdog(void *hdl);
static int wlc_dpt_down(void *hdl);
static void wlc_dpt_buffer_upper(void *hdl, struct scb *scb, void *p, uint prec);
static uint wlc_dpt_txpktcnt_upper(void *hdl);
static void wlc_dpt_send_prbreq(dpt_info_t *dpt, wlc_bsscfg_t *parent, struct ether_addr *dst);
static void wlc_dpt_send_prbresp(dpt_info_t *dpt, wlc_bsscfg_t *parent, struct ether_addr *dst);
static struct scb *wlc_dpt_scb_create(dpt_info_t *dpt, wlc_bsscfg_t *parent,
	struct ether_addr *ea, struct ether_addr *bssid);
static brcm_prop_ie_t *wlc_dpt_get_dpt_ie(uint8 *tlvs, int tlvs_len);
static void wlc_dpt_set_sec_mode(dpt_info_t *dpt, struct scb *scb);
static void wlc_dpt_update_path(dpt_info_t *dpt, struct scb *scb, bool force);
static void wlc_dpt_age_scb(dpt_info_t *dpt);
static dpt_lookaside_t *wlc_dpt_lookaside_find(dpt_info_t *dpt,
	struct ether_addr *ea);
static dpt_lookaside_t *wlc_dpt_lookaside_create(dpt_info_t *dpt,
	struct ether_addr *ea);
static void wlc_dpt_disconnect(dpt_info_t *dpt, struct scb *scb, bool force);
static void wlc_dpt_send_pkt(dpt_info_t *dpt, wlc_bsscfg_t *parent,
	struct ether_addr *sa, struct ether_addr *da, uint8 *data, uint dlen, uint32 flags);
static void wlc_dpt_send_connreq(dpt_info_t *dpt, struct scb *scb, uint8 token);
static void wlc_dpt_send_connresp(dpt_info_t *dpt, wlc_bsscfg_t *parent,
	struct ether_addr *sa, struct ether_addr *da, uint8 token, uint8 status);
static void wlc_dpt_send_hb(dpt_info_t *dpt, struct scb *scb);
static void wlc_dpt_send_sp(dpt_info_t *dpt, struct scb *scb, uint8 subtype,
	uint8 token);
static void wlc_dpt_send_ls(dpt_info_t *dpt, struct scb *scb, uint8 subtype,
	uint8 token);
static void wlc_dpt_rcv_connreq(dpt_info_t *dpt, struct wlc_frminfo *f,
	uint8 *data, uint dlen);
static void wlc_dpt_rcv_connresp(dpt_info_t *dpt, struct scb *scb, uint8 *data,
	uint dlen);
static void wlc_dpt_rcv_sp_ack(dpt_info_t *dpt, struct scb *scb, uint8 token);
static int wlc_dpt_join(dpt_info_t *dpt, wlc_bss_info_t *bi, struct scb *scb,
	uint8 *body, int body_len);
static struct scb *wlc_dpt_scbfindband_all(wlc_info_t *wlc, const struct ether_addr *ea,
	int bandunit);
#ifdef BCMDBG
static int wlc_dpt_dump(dpt_info_t *dpt, struct bcmstrbuf *b);
#endif /* BCMDBG */
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void wlc_dpt_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b);
#else
#define wlc_dpt_scb_dump NULL
#endif // endif

static int wlc_dpt_write_ie(dpt_info_t *dpt, wlc_bsscfg_t *bsscfg, uint8 *cp, int buflen);

/* IE mgmt */
static uint wlc_dpt_calc_dpt_ie_len(void *ctx, wlc_iem_calc_data_t *data);
static int wlc_dpt_write_dpt_ie(void *ctx, wlc_iem_build_data_t *data);
static int wlc_dpt_prq_parse_dpt_ie(void *ctx, wlc_iem_parse_data_t *data);
static int wlc_dpt_prs_parse_dpt_ie(void *ctx, wlc_iem_parse_data_t *data);

/* functions for the upper DPT txmod; This is used to buffer frames at the top
 * when the path switch (AP to direct) is sent to peer. Peer has to ack
 * this frame. After the ack is received the frames in this txmod are released.
 * This helps prevent reordering of frames during path switch
 */
static txmod_fns_t BCMATTACHDATA(dpt_txmod_upper_fns) = {
	wlc_dpt_buffer_upper,
	wlc_dpt_txpktcnt_upper,
	NULL,
	NULL
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

dpt_info_t *
BCMATTACHFN(wlc_dpt_attach)(wlc_info_t *wlc)
{
	dpt_info_t *dpt;
	uint16 prbfstbmp = FT2BMP(FC_PROBE_REQ) | FT2BMP(FC_PROBE_RESP);

	/* sanity checks */
	ASSERT(wlc->pub->tunables->maxdpt > 0);
	ASSERT((wlc->pub->tunables->maxdpt + WLC_MAXBSSCFG)
	       <= wlc->pub->tunables->maxscb);
	ASSERT((wlc->pub->tunables->maxdpt + 1) <= WLC_MAXBSSCFG);
	ASSERT(ISPOWEROF2(NTXRATE));
	ASSERT(sizeof(dpt_packet_t) == DPT_PACKET_LEN);

	if (!(dpt = (dpt_info_t *)MALLOC(wlc->osh, sizeof(dpt_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)dpt, sizeof(dpt_info_t));

#ifdef WLCNT
	if (!(dpt->cnt = (wlc_dpt_cnt_t *)MALLOC(wlc->osh, sizeof(wlc_dpt_cnt_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero((char *)dpt->cnt, sizeof(wlc_dpt_cnt_t));
#endif /* WLCNT */

	dpt->wlc = wlc;
	dpt->cap = DPT_CAP_IN_NETWORK;
	dpt->pathsel = DPT_PATHSEL_AUTO;
	dpt->discovery = DPT_DISCOVERY_MANUAL | DPT_DISCOVERY_AUTO;
	dpt->discoverable = TRUE;
	dpt->prb_freq = DPT_PROBE_FREQ;
	dpt->hb_freq = DPT_HEARTBEAT_FREQ;
	dpt->pathsel_freq = DPT_PATHSEL_FREQ;
	dpt->max_sessions = wlc->pub->tunables->maxdpt;
	dpt->scb_age_threshold = DPT_SCB_AGE_THRESHOLD;
	dpt->rssi_threshold = DPT_RSSI_THRESHOLD;

	/* reserve cubby in the scb container for per-scb private data */
	dpt->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(scb_dpt_cubby_t),
#if defined(BCM_HOST_MEM_RESTORE) && defined(BCM_HOST_MEM_SCB)
		scb_dpt_init, scb_dpt_deinit, wlc_dpt_scb_dump, (void *)dpt, 0);
#else
		scb_dpt_init, scb_dpt_deinit, wlc_dpt_scb_dump, (void *)dpt);
#endif // endif
	if (dpt->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		goto fail;
	}

	/* register IE mgmt callback */
	/* calc/build */
	/* prbreq/prbrsp */
	if (wlc_iem_vs_add_build_fn_mft(wlc->iemi, prbfstbmp, WLC_IEM_VS_IE_PRIO_BRCM_DPT,
	      wlc_dpt_calc_dpt_ie_len, wlc_dpt_write_dpt_ie, dpt) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_build_fn failed, prb ie\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* parse */
	/* prbreq */
	if (wlc_iem_vs_add_parse_fn(wlc->iemi, FC_PROBE_REQ, WLC_IEM_VS_IE_PRIO_BRCM_DPT,
	                            wlc_dpt_prq_parse_dpt_ie, dpt) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, prb req\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	/* prbresp */
	if (wlc_iem_vs_add_parse_fn(wlc->iemi, FC_PROBE_RESP, WLC_IEM_VS_IE_PRIO_BRCM_DPT,
	                            wlc_dpt_prs_parse_dpt_ie, dpt) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, prb resp\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, dpt_iovars, "dpt", dpt, wlc_dpt_doiovar,
	                        wlc_dpt_watchdog, NULL, wlc_dpt_down)) {
		WL_ERROR(("wl%d: dpt wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	wlc_txmod_fn_register(wlc, TXMOD_DPT, dpt, dpt_txmod_upper_fns);
#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "dpt", (dump_fn_t)wlc_dpt_dump, (void *)dpt);
#endif // endif
	/* try to set dpt to the default value */
	wlc_dpt_set(dpt, wlc->pub->_dpt);

	return dpt;

fail:
	MFREE(wlc->osh, dpt, sizeof(dpt_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_dpt_detach)(dpt_info_t *dpt)
{
	if (!dpt)
		return;

	/* sanity */
	ASSERT(dpt->ls_pending == 0);
	ASSERT(dpt->po_pending == 0);

	wlc_module_unregister(dpt->wlc->pub, "dpt", dpt);
#ifdef WLCNT
	if (dpt->cnt)
		MFREE(dpt->wlc->osh, dpt->cnt, sizeof(wlc_dpt_cnt_t));
#endif /* WLCNT */
	MFREE(dpt->wlc->osh, dpt, sizeof(dpt_info_t));
}

/* scb cubby init fn */
static int
scb_dpt_init(void *context, struct scb *scb)
{
	return 0;
}

/* Global scb lookup */
static struct scb *
wlc_dpt_scbfindband_all(wlc_info_t *wlc, const struct ether_addr *ea, int bandunit)
{
	int32 idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	FOREACH_BSS(wlc, idx, cfg) {
		scb = wlc_scbfindband(wlc, cfg, ea, bandunit);
		if (scb != NULL)
			break;
	}

	return scb;
}

/* scb cubby deinit fn */
static void
scb_dpt_deinit(void *context, struct scb *scb)
{
	dpt_info_t *dpt = (dpt_info_t *)context;
	wlc_info_t *wlc = dpt->wlc;
	wlc_bsscfg_t *bsscfg = scb->bsscfg;
	scb_dpt_cubby_t *scb_dpt_cubby;
	scb_dpt_t *scb_dpt;

	if (bsscfg && BSSCFG_IS_DPT(bsscfg)) {
		wlc_bsscfg_t *parent;
		wlc_dpt_t *dc;

		WL_DPT(("wl%d: scb_dpt_deinit: enter\n", wlc->pub->unit));

		/* XXX JQL decouple with the bsscfg first, otherwise the scb
		 * will be freed along with the bsscfg.
		 */
		scb->bsscfg = NULL;

		/* cleanup the auth */
		/* XXX DPT manages auth/sup itself and doesn't follow
		 * AP/authenticator and STA/supplicant rule therefore
		 * freeing any auth itself.
		 * When there is a need to support IBSS 4-way handshake
		 * in the driver move this few lines to wlc_bsscfg.c.
		 */
		if (BCMAUTH_PSK_ENAB(wlc->pub) && bsscfg->authenticator) {
			wlc_authenticator_detach(bsscfg->authenticator);
			bsscfg->authenticator = NULL;
		}

		/* XXX: we are freeing bsscfg here. There is a possible race
		 * in future if we ever use bsscfg in dotxstatus
		 */

		wlc_bsscfg_free(wlc, bsscfg);

		/* free the cubby structure */
		scb_dpt_cubby = SCB_DPT_CUBBY(dpt, scb);
		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt != NULL);

		parent = scb_dpt->parent;
		ASSERT(parent != NULL);

		dc = parent->dpt;

		if (pktq_len(&scb_dpt->ubufq)) {
			WL_ERROR(("wl%d: flushing %d frames from ubufq\n",
				wlc->pub->unit, pktq_len(&scb_dpt->ubufq)));
			pktq_flush(wlc->osh, &scb_dpt->ubufq, TRUE, NULL, 0);
		}

		if (scb_dpt->peer_ies) {
			MFREE(wlc->osh, scb_dpt->peer_ies, scb_dpt->peer_ies_len);
			scb_dpt->peer_ies = NULL;
		}

		if (scb_dpt->flags & DPT_SCB_LS_PENDING) {
			dc->ls_pending--;
			dpt->ls_pending--;
			ASSERT(dpt->ls_pending <= wlc->pub->tunables->maxdpt);
		}

		if ((scb_dpt->flags & DPT_SCB_CONNECTED) &&
		    !(scb_dpt->flags & DPT_SCB_PORT_OPEN)) {
			dc->po_pending--;
			dpt->po_pending--;
			ASSERT(dpt->po_pending <= wlc->pub->tunables->maxdpt);
		}

		dpt->cur_sessions--;
		MFREE(wlc->osh, scb_dpt_cubby->cubby, sizeof(scb_dpt_t));
		scb_dpt_cubby->cubby = NULL;

		wlc_set_ps_ctrl(parent);
	}
}

/* frees all the buffers and cleanup everything on down */
static int
wlc_dpt_down(void *hdl)
{
	dpt_info_t *dpt = (dpt_info_t *)hdl;
	wlc_info_t *wlc = dpt->wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	int i;

	/* cleanup the lookaside entries */
	for (i = 0; i < DPT_LOOKASIDE_SIZE; i++)
		bzero(&dpt->lookaside[i], sizeof(dpt_lookaside_t));

	/* free all dpt scbs; deinit function frees up the bsscfgs */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (BSSCFG_IS_DPT(scb->bsscfg))
			wlc_dpt_disconnect(dpt, scb, TRUE);
	}
	return 0;
}

void
wlc_dpt_cleanup(dpt_info_t *dpt, wlc_bsscfg_t *parent)
{
	wlc_info_t *wlc = dpt->wlc;
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (BSSCFG_IS_DPT(scb->bsscfg) &&
		    wlc_dpt_get_parent_bsscfg(wlc, scb) == parent) {
			dpt_lookaside_t *dla = wlc_dpt_lookaside_find(dpt, &scb->ea);
			if (dla != NULL)
				bzero(dla, sizeof(dpt_lookaside_t));
			wlc_dpt_disconnect(dpt, scb, TRUE);
		}
	}
}

/* DPT buffering function called through txmod */
static void
wlc_dpt_buffer_upper(void *hdl, struct scb *scb, void *p, uint prec)
{
	dpt_info_t *dpt = (dpt_info_t *)hdl;
	wlc_info_t *wlc = dpt->wlc;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);

	ASSERT(scb_dpt != NULL);

	if (pktq_full(&scb_dpt->ubufq)) {
		WL_ERROR(("wl%d: %s: ubufq full; Discarding\n",
			dpt->wlc->pub->unit, __FUNCTION__));

		PKTFREE(wlc->osh, p, TRUE);
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		WLCNTINCR(dpt->cnt->discard);
		return;
	}

	pktenq(&scb_dpt->ubufq, p);
	WLCNTINCR(dpt->cnt->ubuffered);
}

/* Return the buffered packets held by DPT */
static uint
wlc_dpt_txpktcnt_upper(void *hdl)
{
	dpt_info_t *dpt = (dpt_info_t *)hdl;
	wlc_info_t *wlc = dpt->wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	int pktcnt = 0;
	scb_dpt_t *scb_dpt;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (BSSCFG_IS_DPT(scb->bsscfg)) {
			scb_dpt = SCB_DPT(dpt, scb);
			ASSERT(scb_dpt);
			pktcnt += pktq_len(&scb_dpt->ubufq);
		}
	}

	return pktcnt;
}

static int
wlc_dpt_watchdog(void *hdl)
{
	int i;
	dpt_info_t *dpt = (dpt_info_t *)hdl;
	wlc_info_t *wlc = dpt->wlc;
	scb_dpt_t *scb_dpt;
	struct scb *scb;
	struct scb_iter scbiter;

	for (i = 0; i < DPT_LOOKASIDE_SIZE; i++) {
		if (!(dpt->lookaside[i].flags & DPT_LOOKASIDE_PERM)) {
			if (dpt->lookaside[i].prb_sent > 0)
				dpt->lookaside[i].prb_sent--;
		}
	}

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!BSSCFG_IS_DPT(scb->bsscfg))
			continue;

		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt);

		if (scb_dpt->free_me) {
			wlc_scbfree(wlc, scb);
			continue;
		}

		/* age out the scb if no data traffic for a long time */
		if ((wlc->pub->now - scb_dpt->used) >= DPT_SCB_USE_TIMEOUT) {
			wlc_dpt_disconnect(dpt, scb, FALSE);
			continue;
		}

		/* cleanup if port not opened for a long time */
		if (!(scb_dpt->flags & DPT_SCB_PORT_OPEN) &&
		    !(scb_dpt->flags & DPT_SCB_MANUAL) &&
		    ((wlc->pub->now - scb_dpt->used) > DPT_HB_TIMEOUT)) {
			wlc_dpt_disconnect(dpt, scb, FALSE);
			continue;
		}

		/* send periodic heart beats if port is open */
		if ((wlc->pub->now - scb_dpt->hb_sent) >= dpt->hb_freq) {
			if ((scb_dpt->flags & DPT_SCB_PORT_OPEN) &&
			    !(scb_dpt->flags & DPT_SCB_SUSPENDED)) {
				scb_dpt->hb_sent = wlc->pub->now;
				wlc_dpt_send_hb(dpt, scb);
			}
		}

		/* resend connection request */
		if (scb_dpt->flags & DPT_SCB_CON_PENDING) {
			wlc_dpt_send_connreq(dpt, scb, scb_dpt->con_token);
		}

		/* resend path switch notification */
		if (scb_dpt->flags & DPT_SCB_SP_PENDING) {
			scb_dpt->sp_token = dpt->token++;
			wlc_dpt_send_sp(dpt, scb, DPT_ST_SP_AP_TO_DIRECT,
				scb_dpt->sp_token);
		}

		/* resend link suspend notification */
		if (scb_dpt->flags & DPT_SCB_LS_PENDING) {
			scb_dpt->ls_pending_time--;
			if (scb_dpt->ls_pending_time) {
				scb_dpt->ls_token = dpt->token++;
				wlc_dpt_send_ls(dpt, scb,
					DPT_ST_LS_SUSPEND, scb_dpt->ls_token);
			} else {
				wlc_dpt_disconnect(dpt, scb, FALSE);
				continue;
			}
		}

		/* check if we haven't rcvd heart beat for a long time */
		/* XXX: if HB gets converted to beacon format, then dont
		 * check for suspended
		 */
		if  ((scb_dpt->flags & DPT_SCB_PORT_OPEN) &&
		     !(scb_dpt->flags & DPT_SCB_PEER_SUSPENDED)) {
			if ((wlc->pub->now - scb_dpt->hb_rcvd) > DPT_HB_TIMEOUT) {
				WL_DPT(("wl%d: HB not recd for %d secs\n",
					dpt->wlc->pub->unit, DPT_HB_TIMEOUT));
				wlc_dpt_disconnect(dpt, scb, FALSE);
				continue;
			}
		}

		/* periodic path update; freq controlled in fn itself */
		wlc_dpt_update_path(dpt, scb, FALSE);
	}
	return 0;
}

/* handle DPT related iovars */
static int
wlc_dpt_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	dpt_info_t *dpt = (dpt_info_t *)hdl;
	wlc_info_t *wlc = dpt->wlc;
	wlc_pub_t *pub = wlc->pub;
	struct scb *scb;
	scb_dpt_t *scb_dpt;
	struct scb_iter scbiter;
	dpt_lookaside_t *dla;
	int32 int_val = 0;
	bool bool_val;
	int err = 0;
	wlc_bsscfg_t *bsscfg;
	wlc_dpt_t *dc;

	ASSERT(dpt == wlc->dpt);

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	dc = bsscfg->dpt;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {
	case IOV_GVAL(IOV_DPT):
		*((uint32*)a) = DPT_ENAB(pub);
		break;

	case IOV_SVAL(IOV_DPT):
		return wlc_dpt_set(dpt, bool_val);

	case IOV_GVAL(IOV_DPT_DISCOVERY):
		*((uint32*)a) = (uint32)dpt->discovery;
		break;

	case IOV_SVAL(IOV_DPT_DISCOVERY):
		if (int_val & ~(DPT_DISCOVERY_MANUAL | DPT_DISCOVERY_AUTO)) {
			err = BCME_RANGE;
			break;
		}

		dpt->discovery = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_DPT_DISCOVERABLE):
		*((uint32*)a) = (uint32)dpt->discoverable;
		break;

	case IOV_SVAL(IOV_DPT_DISCOVERABLE):
		dpt->discoverable = (uint8)int_val;
		break;

	case IOV_SVAL(IOV_DPT_DENY):
	{
		dpt_iovar_t *info = (dpt_iovar_t *) a;

		if ((info->mode != DPT_DENY_LIST_ADD) &&
		    (info->mode != DPT_DENY_LIST_REMOVE)) {
			WL_ERROR(("wl%d: invalid mode\n", pub->unit));
			err = BCME_BADARG;
			break;
		}

		if (info->mode == DPT_DENY_LIST_ADD) {
			dla = wlc_dpt_lookaside_find(dpt, &info->ea);
			if (!dla)
				dla = wlc_dpt_lookaside_create(dpt, &info->ea);
			if (!dla) {
				WL_ERROR(("wl%d: could not create lookaside entry\n",
					pub->unit));
				err = BCME_BADARG;
				break;
			}
			dla->flags |= DPT_LOOKASIDE_PERM;
		} else {
			dla = wlc_dpt_lookaside_find(dpt, &info->ea);
			if (!dla || !(dla->flags & DPT_LOOKASIDE_PERM)) {
				WL_ERROR(("wl%d: deny entry not found\n",
					pub->unit));
				err = BCME_BADARG;
				break;
			}
			bzero(dla, sizeof(dpt_lookaside_t));
		}

		/* disconnect if entry added for an existing dpt connection */
		scb = wlc_dpt_scbfindband_all(wlc, &info->ea,
		                              CHSPEC_WLCBANDUNIT(wlc->home_chanspec));
		if (scb && BSSCFG_IS_DPT(scb->bsscfg) &&
		    (info->mode == DPT_DENY_LIST_ADD)) {
			wlc_dpt_disconnect(dpt, scb, TRUE);
		}
		break;
	}

	case IOV_SVAL(IOV_DPT_ENDPOINT):
	{
		dpt_iovar_t *info = (dpt_iovar_t *) a;

		if ((info->mode != DPT_MANUAL_EP_CREATE) &&
		    (info->mode != DPT_MANUAL_EP_DELETE)) {
			WL_ERROR(("wl%d: invalid mode\n", pub->unit));
			err = BCME_BADARG;
			break;
		}

		if (!(dpt->discovery & DPT_DISCOVERY_MANUAL)) {
			WL_ERROR(("wl%d: discovery mode is not manual\n",
				pub->unit));
			err = BCME_BADARG;
			break;
		}

		if (info->mode == DPT_MANUAL_EP_CREATE) {

			scb = wlc_dpt_scbfindband_all(wlc, &info->ea,
			                              CHSPEC_WLCBANDUNIT(wlc->home_chanspec));

			if (!scb || !BSSCFG_IS_DPT(scb->bsscfg)) {
				/* create a new dpt scb */
				scb = wlc_dpt_scb_create(dpt, bsscfg, &info->ea, NULL);

				if (!scb) {
					WL_ERROR(("wl%d: could not create scb\n",
						pub->unit));
					err = BCME_BADARG;
					break;
				}
			}

			scb_dpt = SCB_DPT(dpt, scb);
			ASSERT(scb_dpt != NULL);

			scb_dpt->flags |= DPT_SCB_MANUAL;

			/* send probe request */
			wlc_dpt_send_prbreq(dpt, scb_dpt->parent, &scb->ea);

		} else if (info->mode == DPT_MANUAL_EP_DELETE) {
			scb = wlc_dpt_scbfindband_all(wlc, &info->ea,
			                              CHSPEC_WLCBANDUNIT(wlc->home_chanspec));

			if (!scb || !BSSCFG_IS_DPT(scb->bsscfg)) {
				WL_ERROR(("wl%d: could not find scb\n",
					pub->unit));
				err = BCME_BADARG;
				break;
			}

			scb_dpt = SCB_DPT(dpt, scb);
			ASSERT(scb_dpt != NULL);

			scb_dpt->flags &= ~DPT_SCB_MANUAL;
			wlc_dpt_disconnect(dpt, scb, FALSE);
		} else
			ASSERT(0);

		break;
	}

	case IOV_GVAL(IOV_DPT_PATHSEL):
		*((uint32*)a) = (uint32)dpt->pathsel;
		break;

	case IOV_SVAL(IOV_DPT_PATHSEL):
		dpt->pathsel =  (uint8)int_val;
		break;

	case IOV_GVAL(IOV_DPT_PROBE_FREQ):
		*((uint32*)a) = (uint32)dpt->prb_freq;
		break;

	case IOV_SVAL(IOV_DPT_PROBE_FREQ):
		dpt->prb_freq = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_DPT_MIN_RSSI):
		*((int32*)a) = dpt->rssi_threshold;
		break;

	case IOV_SVAL(IOV_DPT_MIN_RSSI):
		dpt->rssi_threshold = (int16)int_val;
		break;

	case IOV_GVAL(IOV_DPT_MAX_SESSIONS):
		*((uint32*)a) = (uint32)dpt->max_sessions;
		break;

	case IOV_SVAL(IOV_DPT_MAX_SESSIONS):
		if ((int_val > pub->tunables->maxdpt) || ((uint)int_val < dpt->cur_sessions)) {
			err = BCME_RANGE;
			break;
		}
		dpt->max_sessions =  (uint)int_val;
		break;

	case IOV_GVAL(IOV_DPT_WSEC):
		*((uint32*)a) = dc->wsec;
		break;

	case IOV_SVAL(IOV_DPT_WSEC):
		if ((int_val != 0) && (int_val != AES_ENABLED)) {
			WL_ERROR(("wl%d: invalid wsec %d\n", pub->unit, int_val));
			err = BCME_UNSUPPORTED;
			break;
		}

		dc->wsec = int_val;
		break;

	case IOV_GVAL(IOV_DPT_FNAME):
	{
		dpt_fname_t *fname = (dpt_fname_t *) a;

		if ((uint)alen < sizeof(dpt_fname_t)) {
			WL_ERROR(("wl%d: input buffer of len %d too small\n",
				pub->unit, alen));
			err = BCME_BUFTOOSHORT;
		}

		fname->len = dc->fnlen;
		memcpy(fname->name, dc->fname, fname->len);
		fname->name[fname->len] = '\0';
		break;
	}

	case IOV_SVAL(IOV_DPT_FNAME):
	{
		dpt_fname_t *fname = (dpt_fname_t *) a;

		if (fname->len >= DPT_FNAME_LEN) {
			WL_ERROR(("wl%d: invalid fname length %d\n",
				pub->unit, fname->len));
			err = BCME_BADARG;
			break;
		}

		WL_DPT(("wl%d: setting fname as %s len %d\n",
			pub->unit, fname->name, fname->len));

		dc->fnlen = fname->len;
		memcpy(dc->fname, fname->name, fname->len);
		dc->fname[fname->len] = '\0';
		break;
	}

	case IOV_GVAL(IOV_DPT_WPA_AUTH):
		*((uint32*)a) = (uint32)dc->WPA_auth;
		break;

	case IOV_SVAL(IOV_DPT_WPA_AUTH):
		if ((int_val != WPA_AUTH_DISABLED) && (int_val != BRCM_AUTH_DPT)) {
			WL_ERROR(("wl%d: invalid wpa_auth %d (should be 0 or 0x%x)\n",
				pub->unit, int_val, BRCM_AUTH_DPT));
			err = BCME_UNSUPPORTED;
			break;
		}

		dc->WPA_auth = (uint16)int_val;
		break;

	case IOV_SVAL(IOV_DPT_PMK):
	{
		wsec_pmk_t *pmk = (wsec_pmk_t *) a;

		if ((pmk->key_len < WSEC_MIN_PSK_LEN) || (pmk->key_len >= WSEC_MAX_PSK_LEN)) {
			WL_ERROR(("wl%d: invalid key len %d (should be between 8 and 64)\n",
				pub->unit, pmk->key_len));
			err = BCME_UNSUPPORTED;
			break;
		}

		memcpy(&dc->pmk, pmk, sizeof(wsec_pmk_t));

		if ((pmk->flags & WSEC_PASSPHRASE) && (pmk->key_len < WSEC_MAX_PSK_LEN)) {
			/* null terminate, just for printing purposes */
			pmk->key[pmk->key_len] = '\0';
			WL_DPT(("wl%d: setting pmk to be %s len %d\n",
				pub->unit, pmk->key, pmk->key_len));
		}

		break;
	}

	case IOV_GVAL(IOV_DPT_LIST):
	{
		dpt_list_t *list = (dpt_list_t *) a;
		uint8 i = 0;

		if ((uint)alen < (sizeof(uint32) + (dpt->cur_sessions * sizeof(dpt_status_t)))) {
			WL_ERROR(("wl%d: input buffer of len %d too small\n",
				pub->unit, alen));
			err = BCME_BUFTOOSHORT;
		}

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (!BSSCFG_IS_DPT(scb->bsscfg))
				continue;

			scb_dpt = SCB_DPT(dpt, scb);
			ASSERT(scb_dpt);

			list->status[i].status = DPT_STATUS_ACTIVE;
			list->status[i].fnlen = scb_dpt->fnlen;
			bcopy(scb_dpt->fname, list->status[i].name, scb_dpt->fnlen);
			list->status[i].rssi = wlc_scb_rssi(scb);
			/* retrieve the sta info */
			wlc_sta_info(wlc, scb->bsscfg,
			             &scb->ea, &list->status[i].sta, sizeof(sta_info_t));
			i++;
		}
		list->num = i;
		break;
	}

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

int
wlc_dpt_set(dpt_info_t *dpt, bool on)
{
	wlc_info_t *wlc = dpt->wlc;

	wlc->pub->_dpt = FALSE;

	if (on) {
		if (!wlc_dpt_cap(dpt)) {
			WL_ERROR(("wl%d: device not dpt capable\n",
				wlc->pub->unit));
			return BCME_UNSUPPORTED;
		}
		if (TDLS_ENAB(wlc->pub)) {
			WL_ERROR(("wl%d: TDLS is enalbed, turn off TDLS first.\n",
				wlc->pub->unit));
			return BCME_NOTREADY;
		}
		wlc->pub->_dpt = on;
	} else
		wlc_dpt_down(dpt);

	wlc_enable_probe_req(wlc, PROBE_REQ_DPT_MASK, on ? PROBE_REQ_DPT_MASK : 0);
#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(wlc->bc, BCN_CLSG_DPT_MASK, on ? BCN_CLSG_DPT_MASK : 0);
#endif /* WL_BCN_COALESCING */
	return 0;
}

bool
wlc_dpt_cap(dpt_info_t *dpt)
{
	if (N_ENAB(dpt->wlc->pub))
		return TRUE;
	else
		return FALSE;
}

struct scb *
wlc_dpt_query(dpt_info_t *dpt, wlc_bsscfg_t *parent, void *p, struct ether_addr *ea)
{
	wlc_info_t *wlc = dpt->wlc;
	wlc_pub_t *pub = wlc->pub;
	struct scb *scb;
	scb_dpt_t *scb_dpt = NULL;
	dpt_lookaside_t *dla;

	ASSERT(DPT_ENAB(pub));

	/* For phase 1, have to be associated in a BSS */
	if (wlc->ibss_bsscfgs > 0)
		return NULL;

	/* No mcast/bcast traffic on dpt link */
	if (ETHER_ISMULTI(ea))
		return NULL;

	scb = wlc_dpt_scbfindband_all(wlc, ea,
	                              CHSPEC_WLCBANDUNIT(wlc->home_chanspec));

	if (scb && BSSCFG_IS_DPT(scb->bsscfg)) {
		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt);
		/* Make sure that both direct and ap path are not set */
		ASSERT((WLPKTTAG(p)->flags & (WLF_DPT_DIRECT | WLF_DPT_APPATH)) !=
			(WLF_DPT_DIRECT | WLF_DPT_APPATH));
		if (((scb_dpt->flags & DPT_SCB_ACTIVE) &&
		     !(WLPKTTAG(p)->flags & WLF_DPT_APPATH)) ||
		    ((scb_dpt->flags & DPT_SCB_CONNECTED) &&
		     (WLPKTTAG(p)->flags & WLF_DPT_DIRECT))) {
			if (!(WLPKTTAG(p)->flags & WLF_DPT_TYPE))
				scb_dpt->used = pub->now;
			return scb;
		}
		return NULL;
	}

	/* return if auto discovery not enabled */
	if (!(dpt->discovery & DPT_DISCOVERY_AUTO))
		return NULL;

	dla = wlc_dpt_lookaside_find(dpt, ea);

	if (dla) {
		/* check if we need to send probe */
		if (!dla->prb_sent) {
			wlc_dpt_send_prbreq(dpt, parent, &dla->ea);
			dla->prb_sent = dpt->prb_freq;
		}

		return NULL;
	}

	/* create lookaside entry and send probe */
	dla = wlc_dpt_lookaside_create(dpt, ea);
	if (dla)
		wlc_dpt_send_prbreq(dpt, parent, &dla->ea);

	return NULL;
}

static void
wlc_dpt_send_prbreq(dpt_info_t *dpt, wlc_bsscfg_t *parent, struct ether_addr *dst)
{
	wlc_info_t *wlc = dpt->wlc;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	if (!wlc->pub->up)
		return;

	WL_DPT(("wl%d: wlc_dpt_send_prbreq: at %d to %s\n",
		wlc->pub->unit, wlc->pub->now, bcm_ether_ntoa(dst, eabuf)));

	wlc_sendprobe(wlc, parent, (const uint8 *)"", 0, dst, &ether_bcast, 0, NULL, 0);

	WLCNTINCR(dpt->cnt->txprbreq);
}

static void
wlc_dpt_prb_body(wlc_info_t *wlc, wlc_bsscfg_t *parent, wlc_bsscfg_t *bsscfg,
	uint8 *pbody, int *len)
{
	int plen = *len;
	int ielen;

	wlc_bcn_prb_body(wlc, FC_PROBE_RESP, bsscfg, pbody, len, FALSE);

	if (!BSSCFG_IS_DPT(bsscfg))
		return;

	ielen = wlc_dpt_write_ie(wlc->dpt, parent, pbody + *len, plen - *len);
	*len = *len + ielen;
}

static wlc_bsscfg_t *
wlc_dpt_bsscfg_create(wlc_info_t *wlc, wlc_bsscfg_t *parent)
{
	int idx;
	wlc_bsscfg_t *bsscfg;
	wlc_dpt_t *dc;

	if ((idx = wlc_bsscfg_get_free_idx(wlc)) == -1) {
		WL_ERROR(("wl%d: cannot find free index for bsscfg\n", wlc->pub->unit));
		return NULL;
	}

	if ((bsscfg = wlc_bsscfg_alloc(wlc, idx, WLC_BSSCFG_NOIF | WLC_BSSCFG_NOBCMC,
	                               &parent->cur_etheraddr, FALSE)) == NULL) {
		WL_ERROR(("wl%d: %s: failed to allocate bsscfg\n", wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	wlc_bsscfg_dpt_init(wlc, bsscfg);

	dc = parent->dpt;

	bsscfg->WPA_auth = dc->WPA_auth;
	bsscfg->wsec = dc->wsec;
	bsscfg->wsec_restrict = TRUE;
	/* restrict access until authorized */
	if (bsscfg->WPA_auth)
		bsscfg->eap_restrict = TRUE;

	return bsscfg;
}

static void
wlc_dpt_send_prbresp(dpt_info_t *dpt, wlc_bsscfg_t *parent, struct ether_addr *dst)
{
	wlc_info_t *wlc = dpt->wlc;
	void *p;
	uint8 *pbody;
	int len;
	wlc_bsscfg_t *bsscfg = NULL;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	if (!wlc->pub->up)
		return;

	ASSERT(parent != NULL);

	/* create a dummy bsscfg structure to be passed to wlc_bcn_prb_body() */
	bsscfg = wlc_dpt_bsscfg_create(wlc, parent);
	if (bsscfg == NULL) {
		WL_ERROR(("wl%d: cannot malloc bsscfg\n", wlc->pub->unit));
		goto done;
	}

	/* create a random bssid */
	wlc_getrand(wlc, bsscfg->BSSID.octet, ETHER_ADDR_LEN);

	/* Set MAC addr to unicast and "locally administered" */
	ETHER_SET_LOCALADDR(&bsscfg->BSSID);
	ETHER_SET_UNICAST(&bsscfg->BSSID);

	len = wlc->pub->bcn_tmpl_len;

	if ((p = wlc_frame_get_mgmt(wlc, FC_PROBE_RESP, dst, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, len, &pbody)) == NULL) {
		goto done;
	}

	wlc_dpt_prb_body(wlc, parent, bsscfg, pbody, &len);
	PKTSETLEN(wlc->osh, p, len + DOT11_MGMT_HDR_LEN);

	WL_DPT(("wl%d: wlc_dpt_send_prbresp: at %d to %s\n",
		wlc->pub->unit, wlc->pub->now, bcm_ether_ntoa(dst, eabuf)));

	WLCNTINCR(dpt->cnt->txprbresp);

	wlc_sendmgmt(wlc, p, wlc->active_queue, NULL);

done:
	if (bsscfg != NULL)
		wlc_bsscfg_free(wlc, bsscfg);
}

wlc_bsscfg_t *
wlc_dpt_get_parent_bsscfg(wlc_info_t *wlc, struct scb *scb)
{
	scb_dpt_t *scb_dpt;

	ASSERT(scb != NULL);

	scb_dpt = SCB_DPT(wlc->dpt, scb);
	ASSERT(scb_dpt != NULL);

	return scb_dpt->parent;
}

static wlc_bsscfg_t *
wlc_dpt_find_parent_bsscfg(wlc_info_t *wlc, struct scb *scb,
	brcm_prop_ie_t *dpt_ie, struct ether_addr *da)
{
	wlc_bsscfg_t *parent = NULL;
	uint8 *tlvs;
	uint8 tlvs_len;
	bcm_tlv_t *bssid;

	if (scb != NULL)
		return wlc_dpt_get_parent_bsscfg(wlc, scb);

	tlvs = (uint8 *)&dpt_ie[1];
	tlvs_len = dpt_ie->len - BRCM_PROP_IE_LEN;
	bssid = bcm_parse_tlvs(tlvs, tlvs_len, DPT_IE_TYPE_BSSID);
	if (bssid == NULL)
		goto exit;

	/* XXX: is BSSID match only okay? do we need to check if it is associated
	 * and a BSS?
	 */
	parent = wlc_bsscfg_find_by_bssid(wlc, (struct ether_addr *)bssid->data);
	if (parent == NULL)
		goto exit;

	/* FIXME: Need to find a way to identify the BSS of the incoming dpt req(s)
	 * when they don't have BSSID info in the DPT IE i.e. older version of dpt.
	 * For now just use the first BSS found matching the DA.
	 */
exit:
	if (parent == NULL) {
		int idx;
		wlc_bsscfg_t *cfg;
		FOREACH_AS_STA(wlc, idx, cfg) {
			if (!cfg->BSS)
				continue;
			if (bcmp(da, &cfg->cur_etheraddr, ETHER_ADDR_LEN) != 0)
				continue;
			parent = cfg;
			break;
		}
	}

	return parent;
}

static void
wlc_dpt_recv_process_prbreq(dpt_info_t *dpt, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	wlc_info_t *wlc = dpt->wlc;
	dpt_lookaside_t *dla;
	brcm_prop_ie_t *dpt_ie;
	struct scb *scb;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	scb_dpt_t *scb_dpt = NULL;
	wlc_bsscfg_t *parent;

	WLCNTINCR(dpt->cnt->rxallprbreq);

	/* For phase 1, have to be associated; IBSS not yet tested */
	if (wlc->ibss_bsscfgs > 0)
		return;

	/* return if it does not have a dpt ie */
	if ((dpt_ie = wlc_dpt_get_dpt_ie(body, body_len)) == NULL)
		return;

	WL_DPT(("wl%d: wlc_dpt_process_prbreq: recd DPT prbreq from %s\n",
		wlc->pub->unit, bcm_ether_ntoa(&hdr->sa, eabuf)));

	WLCNTINCR(dpt->cnt->rxdptprbreq);

	/* return if peer is on deny list */
	dla = wlc_dpt_lookaside_find(dpt, &hdr->sa);
	if (dla && (dla->flags & DPT_LOOKASIDE_PERM))
		return;

	/* return if no matching capability */
	if ((ltoh16(dpt_ie->cap) & dpt->cap) == 0)
		return;

	scb = wlc_dpt_scbfindband_all(wlc, &hdr->sa, CHSPEC_WLCBANDUNIT(wlc->home_chanspec));
	if (scb != NULL) {
		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt != NULL);
	}

	/* return if we are not discoverable and not manual */
	if (!dpt->discoverable &&
	    (!scb || !(scb_dpt->flags & DPT_SCB_MANUAL)))
		return;

	/* try all associated BSS if not directed to us */
	parent = wlc_dpt_find_parent_bsscfg(wlc, scb, dpt_ie, &hdr->da);
	if (parent == NULL) {
		WL_ERROR(("wl%d: %s: unable to find parent bsscfg\n",
		          wlc->pub->unit, __FUNCTION__));
		return;
	}

	wlc_dpt_send_prbresp(dpt, parent, &hdr->sa);
}

static void
wlc_dpt_recv_process_prbresp(dpt_info_t *dpt, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	wlc_info_t *wlc = dpt->wlc;
	struct scb *scb;
	scb_dpt_t *scb_dpt = NULL;
	dpt_lookaside_t *dla;
	brcm_prop_ie_t *dpt_ie;
	uint len = body_len - sizeof(struct dot11_bcn_prb);
	uint8 *parse = (uint8*)body + sizeof(struct dot11_bcn_prb);
	bool on_home_channel;
	uint8 rx_channel;
	bcm_tlv_t *ds_params;
	wlc_bss_info_t bi;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
	wlc_bsscfg_t *parent;

	if (body_len < sizeof(struct dot11_bcn_prb)) {
		WL_INFORM(("wl%d:%s: malformed ie discard\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	WLCNTINCR(dpt->cnt->rxallprbresp);

	/* For phase 1, have to be associated; IBSS not yet tested */
	if (wlc->ibss_bsscfgs > 0)
		return;

	/* make sure the probe response is in our home channel */
	rx_channel = (uint8)WLC_RX_CHANNEL(&wrxh->rxhdr);
	ds_params = bcm_parse_tlvs(parse, len, DOT11_MNG_DS_PARMS_ID);
	if (!ds_params || ds_params->len < 1) {
		/* if no channel info, use the channel on which we received the beacon */
		/* For 40MHz, rx_channel can be either the center or side channel */
		on_home_channel =
			(CHSPEC_CHANNEL(wlc->home_chanspec) == rx_channel) ||
			(wf_chspec_ctlchan(wlc->home_chanspec) == rx_channel);
	} else {
		on_home_channel =
			(wf_chspec_ctlchan(wlc->home_chanspec) == ds_params->data[0]);
	}

	if (!on_home_channel)
		return;

	dpt_ie = wlc_dpt_get_dpt_ie(parse, len);

	/* do not disconnect if probe resp does not have DPT IE */
	if ((dpt_ie == NULL) || ((ltoh16(dpt_ie->cap) & dpt->cap) == 0))
		return;

	WLCNTINCR(dpt->cnt->rxdptprbresp);
	WL_DPT(("wl%d: wlc_dpt_process_prbresp: recd DPT prbresp from %s\n",
		wlc->pub->unit, bcm_ether_ntoa(&hdr->sa, eabuf)));

	scb = wlc_dpt_scbfindband_all(wlc, &hdr->sa,
	                              CHSPEC_WLCBANDUNIT(wlc->home_chanspec));
	if (scb != NULL) {
		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt != NULL);
	}

	/* return if auto discovery not on and scb is not manual */
	if (!(dpt->discovery & DPT_DISCOVERY_AUTO) &&
	    (!scb || !(scb_dpt->flags & DPT_SCB_MANUAL)))
		return;

	/* nothing to do if we are already connected or pending */
	if (scb && (scb_dpt->flags & (DPT_SCB_CONNECTED | DPT_SCB_CON_PENDING)))
		return;

	/* use wlc_parse_bcn_prb() to interpret the ie list in the dpt prb resp */
	if (wlc_recv_parse_bcn_prb(wlc, wrxh, &hdr->bssid, FALSE, body, body_len, &bi)) {
		WL_ERROR(("wl%d: wlc_parse_bcn_prb() failed\n", wlc->pub->unit));
		return;
	}

	/* return if not directed to us */
	parent = wlc_dpt_find_parent_bsscfg(wlc, scb, dpt_ie, &hdr->da);
	if (parent == NULL) {
		WL_ERROR(("wl%d: %s: unable to find parent bsscfg\n",
		          wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* validate that the correct cipher and akm are specified */
	if (parent->dpt->WPA_auth == BRCM_AUTH_DPT) {
		if ((bi.wpa2.multicast != WPA_CIPHER_NONE) ||
		    (bi.wpa2.ucount != 1) ||
		    (bi.wpa2.unicast[0] != WPA_CIPHER_AES_CCM) ||
		    (bi.wpa2.acount != 1) ||
		    (bi.wpa2.auth[0] != BRCM_AKM_DPT)) {
			WL_ERROR(("wl%d: invalid cipher/akm\n", wlc->pub->unit));
			return;
		}
	}

	/* At this point all sanity checks are done and we are ready to
	 * create a DPT connection, so allocate a scb and start the join
	 */

	if (!scb || !(scb_dpt->flags & DPT_SCB_MANUAL)) {
		dla = wlc_dpt_lookaside_find(dpt, &hdr->sa);
		/* scb and dla not there; is an unexpected response; return */
		if (!dla)
			return;

		/* create a dpt scb */
		scb = wlc_dpt_scb_create(dpt, parent, &hdr->sa, &hdr->bssid);
		if (!scb) {
			WL_ERROR(("wl%d: %s: scb not created\n",
				wlc->pub->unit, __FUNCTION__));
			return;
		}
	}

	wlc_dpt_join(dpt, &bi, scb, body, body_len);

	scb_dpt = SCB_DPT(dpt, scb);
	ASSERT(scb_dpt != NULL);

	scb_dpt->flags |= DPT_SCB_CON_PENDING;

	scb_dpt->con_token = dpt->token++;
	wlc_dpt_send_connreq(dpt, scb, scb_dpt->con_token);

	return;
}

static void
wlc_dpt_rcv_connreq(dpt_info_t *dpt, wlc_frminfo_t *f, uint8 *data, uint dlen)
{
	wlc_info_t *wlc = dpt->wlc;
	dpt_packet_t *dpkt = (dpt_packet_t *)data;
	uint8 *body;
	uint body_len;
	struct ether_addr *bssid;
	uint len;
	uint8 *parse;
	brcm_prop_ie_t *dpt_ie;
	struct scb *scb;
	scb_dpt_t *scb_dpt = NULL;
	dpt_lookaside_t *dla;
	wlc_bss_info_t bi;
	wlc_bsscfg_t *parent;

	/* silently ignore malformed frame */
	if (dlen < (sizeof(dpt_packet_t) + sizeof(struct dot11_bcn_prb) + ETHER_ADDR_LEN)) {
		WL_ERROR(("wl%d: %s: truncated pkt\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	bssid = (struct ether_addr *)&dpkt[1];
	body = data + ETHER_ADDR_LEN + sizeof(dpt_packet_t);
	body_len = dlen - ETHER_ADDR_LEN - sizeof(dpt_packet_t);
	parse = body + sizeof(struct dot11_bcn_prb);
	len = body_len - sizeof(struct dot11_bcn_prb);

	dpt_ie = wlc_dpt_get_dpt_ie(parse, len);

	/* silently ignore if dpt ie not found */
	if (dpt_ie == NULL) {
		WL_ERROR(("wl%d: %s: unexp connreq with no dpt ie\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	scb = wlc_dpt_scbfindband_all(wlc, f->sa, CHSPEC_WLCBANDUNIT(wlc->home_chanspec));
	if (scb != NULL) {
		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt != NULL);
	}

	/* return if not directed to us */
	parent = wlc_dpt_find_parent_bsscfg(wlc, scb, dpt_ie, f->da);
	if (parent == NULL) {
		WL_ERROR(("wl%d: %s: unable to find parent bsscfg\n",
		          wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* return (after rejecting conn req) if peer is on deny list */
	dla = wlc_dpt_lookaside_find(dpt, f->sa);
	if (dla && (dla->flags & DPT_LOOKASIDE_PERM)) {
		wlc_dpt_send_connresp(dpt, parent, &parent->cur_etheraddr, f->sa,
			dpkt->token, DPT_PKT_STATUS_FAILED);
		return;
	}

	if ((ltoh16(dpt_ie->cap) & dpt->cap) == 0) {
		wlc_dpt_send_connresp(dpt, parent, &parent->cur_etheraddr, f->sa,
			dpkt->token, DPT_PKT_STATUS_FAILED);
		WL_ERROR(("wl%d: %s: connreq with mismatch cap\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* resend conn resp if connected with the same token */
	if (scb && (scb_dpt->flags & DPT_SCB_CONNECTED)) {
		if (dpkt->token == scb_dpt->con_token)
			wlc_dpt_send_connresp(dpt, parent,
				&scb->bsscfg->cur_etheraddr, &scb->ea,
				scb_dpt->con_token, DPT_PKT_STATUS_SUCCESS);
		return;
	}

	/* higher mac address throws away connreq if it is waiting for connresp */
	if (scb && (scb_dpt->flags & DPT_SCB_CON_PENDING)) {
		if (memcmp(&parent->cur_etheraddr.octet[0],
		           &scb->ea.octet[0], ETHER_ADDR_LEN) > 0) {
			WL_ERROR(("wl%d: %s: discarding since pending\n",
				wlc->pub->unit, __FUNCTION__));
			return;
		}
	}

	/* use wlc_parse_bcn_prb() to interpret the ie list in the dpt prb resp */
	if (wlc_recv_parse_bcn_prb(wlc, f->wrxh, bssid, FALSE, body, body_len, &bi)) {
		WL_ERROR(("wl%d: wlc_parse_bcn_prb() failed\n", wlc->pub->unit));
		return;
	}

	/* validate that the correct cipher and akm are specified */
	if (parent->dpt->WPA_auth == BRCM_AUTH_DPT) {
		if ((bi.wpa2.multicast != WPA_CIPHER_NONE) ||
		    (bi.wpa2.ucount != 1) ||
		    (bi.wpa2.unicast[0] != WPA_CIPHER_AES_CCM) ||
		    (bi.wpa2.acount != 1) ||
		    (bi.wpa2.auth[0] != BRCM_AKM_DPT)) {
			WL_ERROR(("wl%d: invalid cipher/akm\n", wlc->pub->unit));
			return;
		}
	}

	/* At this point all sanity checks are done and we are ready to
	 * create a DPT connection, so allocate a scb and start the join
	 */

	if (!scb || !(scb_dpt->flags & DPT_SCB_MANUAL)) {
		scb = wlc_dpt_scb_create(dpt, parent, f->sa, bssid);
		if (!scb) {
			WL_ERROR(("wl%d: %s: scb not created\n",
				wlc->pub->unit, __FUNCTION__));
			return;
		}
	} else {
		/* update bssid with the new one in connreq */
		bcopy(bssid, &scb->bsscfg->BSSID, ETHER_ADDR_LEN);
	}

	wlc_dpt_join(dpt, &bi, scb, body, body_len);

	scb_dpt = SCB_DPT(dpt, scb);
	ASSERT(scb_dpt != NULL);

	scb_dpt->flags &= ~DPT_SCB_CON_PENDING;
	scb_dpt->flags |= DPT_SCB_CONNECTED;
	scb_dpt->hb_rcvd = dpt->wlc->pub->now;
	scb_dpt->con_token = dpkt->token;

	wlc_dpt_send_connresp(dpt, parent, &scb->bsscfg->cur_etheraddr, &scb->ea,
		scb_dpt->con_token, DPT_PKT_STATUS_SUCCESS);

	/* evaluate if we should be supplicant, authenticator or none */
	wlc_dpt_set_sec_mode(dpt, scb);

	wlc_dpt_update_path(dpt, scb, TRUE);

	return;
}

static void
wlc_dpt_rcv_connresp(dpt_info_t *dpt, struct scb *scb, uint8 *data, uint dlen)
{
	dpt_packet_t *dpkt = (dpt_packet_t *)data;
	scb_dpt_t *scb_dpt;

	scb_dpt = SCB_DPT(dpt, scb);
	ASSERT(scb_dpt != NULL);

	if (scb_dpt->flags & DPT_SCB_CONNECTED) {
		WL_ERROR(("wl%d: %s: already connected (ignore it)\n",
			dpt->wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (!(scb_dpt->flags & DPT_SCB_CON_PENDING)) {
		WL_ERROR(("wl%d: %s: unexp resp(ignore it)\n",
			dpt->wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (dpkt->status != DPT_PKT_STATUS_SUCCESS) {
		WL_ERROR(("wl%d: %s: connection req failed with status %d\n",
			dpt->wlc->pub->unit, __FUNCTION__, dpkt->status));
		return;
	}

	scb_dpt->flags &= ~DPT_SCB_CON_PENDING;
	scb_dpt->flags |= DPT_SCB_CONNECTED;
	scb_dpt->hb_rcvd = dpt->wlc->pub->now;

	/* evaluate if we should be supplicant, authenticator or none */
	wlc_dpt_set_sec_mode(dpt, scb);

	wlc_dpt_update_path(dpt, scb, TRUE);
}

static int
wlc_dpt_join(dpt_info_t *dpt, wlc_bss_info_t *bi, struct scb *scb, uint8 *body,
	int body_len)
{
	wlc_info_t *wlc = dpt->wlc;
	wlc_pub_t *pub = wlc->pub;
	uint len = body_len - sizeof(struct dot11_bcn_prb);
	uint8 *parse = (uint8*)body + sizeof(struct dot11_bcn_prb);
	bcm_tlv_t *fname_ie;
	brcm_ie_t *brcm_ie;
	brcm_prop_ie_t *dpt_ie;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);

	ASSERT(scb_dpt != NULL);

	/* save the peer ies */
	if (scb_dpt->peer_ies)
		MFREE(pub->osh, scb_dpt->peer_ies, scb_dpt->peer_ies_len);
	if ((scb_dpt->peer_ies = MALLOC(pub->osh, len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			pub->unit, __FUNCTION__, MALLOCED(pub->osh)));
		scb_dpt->peer_ies_len = 0;
	} else {
		scb_dpt->peer_ies_len = (uint16)len;
		bcopy(parse, scb_dpt->peer_ies, len);
	}

	if (bi->RSSI != WLC_RSSI_INVALID) {
		scb->rssi_window[scb->rssi_index] = bi->RSSI;
		scb->rssi_index = MODINC_POW2(scb->rssi_index, MA_WINDOW_SZ);
	}

	/* Fill up scb with brcm ie details */
	brcm_ie = (brcm_ie_t *)bcm_find_vendor_ie(parse, len, BRCM_OUI, NULL, 0);
	wlc_process_brcm_ie(wlc, scb, brcm_ie);

	/* fill up dpt related info */
	dpt_ie = wlc_dpt_get_dpt_ie(parse, len);
	ASSERT(dpt_ie);

	fname_ie = bcm_parse_tlvs(&dpt_ie[1], dpt_ie->len - BRCM_PROP_IE_LEN, DPT_IE_TYPE_FNAME);
	if (fname_ie && (fname_ie->len <= DPT_FNAME_LEN)) {
		scb_dpt->fnlen = fname_ie->len;
		bcopy(fname_ie->data, scb_dpt->fname, fname_ie->len);
		scb_dpt->fname[scb_dpt->fnlen] = '\0';
	}

	if (N_ENAB(pub) && (bi->flags & WLC_BSS_HT)) {
		ht_cap_ie_t *cap_ie;
		ht_add_ie_t *add_ie;
		obss_params_t *obss_ie = NULL;

		/* extract ht cap and additional ie */
		cap_ie = wlc_read_ht_cap_ies(wlc, parse, len);
		add_ie = wlc_read_ht_add_ies(wlc, parse, len);
		if (COEX_ENAB(pub))
			obss_ie = wlc_ht_read_obss_scanparams_ie(wlc, parse, len);

		wlc_ht_update_scbstate(wlc, scb, cap_ie, add_ie, obss_ie);
	} else if ((scb->flags & SCB_HTCAP) &&
	           ((bi->flags & WLC_BSS_HT) != WLC_BSS_HT))
		wlc_ht_update_scbstate(wlc, scb, NULL, NULL, NULL);

	/* Set WME bits */
	scb->flags &= ~SCB_WMECAP;
	if (WME_ENAB(pub)) {
		wlc_qosinfo_update(scb, 0, TRUE);     /* Clear Qos Info by default */
		if (wlc_find_wme_ie(parse, len) != NULL)
			scb->flags |= SCB_WMECAP;
	}

	wlc_rateset_filter(&bi->rateset, &scb->rateset, FALSE, WLC_RATES_CCK_OFDM,
	                   RATE_MASK, wlc_get_mcsallow(wlc, NULL));

	wlc_scb_ratesel_init(wlc, scb);

	return 0;
}

void
wlc_dpt_wpa_passhash_done(dpt_info_t *dpt, struct ether_addr *ea)
{
	wlc_info_t *wlc = dpt->wlc;
	bool stat;
	uint8 *self_ies = NULL;
	int self_ies_len;
	struct scb *scb;
	scb_dpt_t *scb_dpt;
	wlc_bsscfg_t *bsscfg;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* defined(BCMDBG) || defined(BCMDBG_ERR) */

	scb = wlc_dpt_scbfindband_all(wlc, ea, CHSPEC_WLCBANDUNIT(wlc->home_chanspec));
	if (!scb || !BSSCFG_IS_DPT(scb->bsscfg)) {
		WL_ERROR(("wl%d: %s: dpt scb not found for ea %s\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(ea, eabuf)));
		return;
	}

	bsscfg = scb->bsscfg;

	scb_dpt = SCB_DPT(dpt, scb);
	ASSERT(scb_dpt != NULL);

	self_ies_len = wlc->pub->bcn_tmpl_len;
	if ((self_ies = (uint8 *)MALLOC(wlc->osh, self_ies_len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return;
	}

	wlc_dpt_prb_body(wlc, scb_dpt->parent, bsscfg, self_ies, &self_ies_len);

	if (scb_dpt->sec_mode == DPT_SEC_SUPPLICANT) {
		stat = wlc_set_sup(wlc->idsup, bsscfg, SUP_WPAPSK,
			(uint8 *)self_ies + sizeof(struct dot11_bcn_prb),
			self_ies_len - sizeof(struct dot11_bcn_prb),
			scb_dpt->peer_ies, scb_dpt->peer_ies_len);

		if (!stat)
			WL_ERROR(("wl%d: wlc_set_sup() failed with %d\n",
				wlc->pub->unit, stat));
		else
			WL_DPT(("wl%d: wlc_set_sup() successful\n",
				wlc->pub->unit));
	} else if (BCMAUTH_PSK_ENAB(wlc->pub) && (scb_dpt->sec_mode == DPT_SEC_AUTHENTICATOR)) {
		stat = wlc_set_auth(bsscfg->authenticator, AUTH_WPAPSK, scb_dpt->peer_ies,
		                    scb_dpt->peer_ies_len,
			(uint8 *)self_ies + sizeof(struct dot11_bcn_prb),
		                    self_ies_len - sizeof(struct dot11_bcn_prb), scb);

		if (!stat)
			WL_ERROR(("wl%d: wlc_set_auth() failed with %d\n",
				wlc->pub->unit, stat));
		else
			WL_DPT(("wl%d: wlc_set_auth() successful\n",
				wlc->pub->unit));
	} else
		ASSERT(0);

	if (self_ies) {
		MFREE(wlc->osh, (void *)self_ies, self_ies_len);
	}
}

static struct scb *
wlc_dpt_scb_create(dpt_info_t *dpt, wlc_bsscfg_t *parent,
	struct ether_addr *ea, struct ether_addr *bssid)
{
	wlc_info_t *wlc = dpt->wlc;
	wlc_bsscfg_t *bsscfg = NULL;
	struct scb *scb = NULL;
	scb_dpt_cubby_t *scb_dpt_cubby;
	dpt_lookaside_t *dla;
	scb_dpt_t *scb_dpt = NULL;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	ASSERT(parent != NULL);

	scb = wlc_dpt_scbfindband_all(wlc, ea, CHSPEC_WLCBANDUNIT(wlc->home_chanspec));

	if (!scb) {
		if (dpt->cur_sessions >= dpt->max_sessions) {
			wlc_dpt_age_scb(dpt);
			/* check again to see if aging helped */
			if (dpt->cur_sessions >= dpt->max_sessions) {
				WL_DPT(("wl%d: wlc_dpt_scb_lookup: could NOT age out immediately\n",
					wlc->pub->unit));
				return NULL;
			}
		}

		bsscfg = wlc_dpt_bsscfg_create(wlc, parent);
		if (bsscfg == NULL) {
			WL_ERROR(("wl%d: cannot malloc bsscfg\n", wlc->pub->unit));
			return NULL;
		}

		scb = wlc_scblookupband(wlc, bsscfg, ea,
		                        CHSPEC_WLCBANDUNIT(wlc->home_chanspec));
		if (!scb) {
			WL_ERROR(("wl%d: %s: out of scbs\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}

		WL_DPT(("wl%d: Created a new DPT scb with ea %s\n",
			wlc->pub->unit, bcm_ether_ntoa(ea, eabuf)));
	} else {
		/* cleanup up the existing info on the scb */
		scb_dpt_deinit(dpt, scb);

		WL_DPT(("wl%d: Reusing DPT scb with ea %s\n",
			wlc->pub->unit, bcm_ether_ntoa(ea, eabuf)));

		bsscfg = wlc_dpt_bsscfg_create(wlc, parent);
		if (bsscfg == NULL) {
			WL_ERROR(("wl%d: cannot malloc bsscfg\n", wlc->pub->unit));
			goto fail;
		}
	}

	wlc_scb_rssi_init(scb, dpt->rssi_threshold);

	/* malloc the scb cubby data */
	scb_dpt = (scb_dpt_t *)MALLOC(wlc->osh, sizeof(scb_dpt_t));
	if (!scb_dpt) {
		WL_ERROR(("wl%d: cannot malloc scb_dpt\n", wlc->pub->unit));
		goto fail;
	}

	bzero((char *)scb_dpt, sizeof(scb_dpt_t));

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wlc->pub)) {
		bsscfg->packet_filter = WLC_ACCEPT_DIRECTED;
	}
#endif // endif

	if (bssid)
		bcopy(bssid, &bsscfg->BSSID, ETHER_ADDR_LEN);
	else {
		/* create a random bssid */
		wlc_getrand(wlc, &bsscfg->BSSID.octet[0], ETHER_ADDR_LEN);

		/* Set MAC addr to unicast and "locally administered" */
		ETHER_SET_LOCALADDR(&bsscfg->BSSID);
		ETHER_SET_UNICAST(&bsscfg->BSSID);
	}

	wlc_scb_set_bsscfg(scb, bsscfg);

	scb_dpt_cubby = SCB_DPT_CUBBY(dpt, scb);
	scb_dpt_cubby->cubby = scb_dpt;
	scb_dpt->pathsel = dpt->pathsel;
	scb_dpt->used = wlc->pub->now;
	scb_dpt->hb_sent = wlc->pub->now;
	scb_dpt->parent = parent;

	pktq_init(&scb_dpt->ubufq, 1, PKTQ_LEN_DEFAULT);

	/* cleanup the lookaside entry if it exists */
	dla = wlc_dpt_lookaside_find(dpt, ea);
	if (dla)
		bzero(dla, sizeof(dpt_lookaside_t));

	dpt->cur_sessions++;

	return scb;

fail:
	if (scb != NULL)
		wlc_scbfree(wlc, scb);
	if (scb_dpt != NULL)
		MFREE(wlc->osh, scb_dpt, sizeof(scb_dpt_t));
	return NULL;
}

static int
wlc_dpt_write_ie(dpt_info_t *dpt, wlc_bsscfg_t *bsscfg, uint8 *cp, int buflen)
{
	brcm_prop_ie_t *dpt_ie = (brcm_prop_ie_t *)cp;
	uint8 *tlvs;
	uint8 tlvs_len;
	wlc_dpt_t *dc;

	if (BSSCFG_IS_DPT(bsscfg))
		return 0;

	ASSERT(bsscfg != NULL);

	dc = bsscfg->dpt;

	/* length check */
	tlvs_len = TLV_HDR_LEN + dc->fnlen + TLV_HDR_LEN + ETHER_ADDR_LEN;
	/* if buffer too small, return untouched buffer */
	BUFLEN_CHECK_AND_RETURN((TLV_HDR_LEN + BRCM_PROP_IE_LEN + tlvs_len), buflen, 0);

	dpt_ie->id = DOT11_MNG_PROPR_ID;
	dpt_ie->len = BRCM_PROP_IE_LEN + tlvs_len;
	bcopy(BRCM_PROP_OUI, &dpt_ie->oui[0], DOT11_OUI_LEN);
	dpt_ie->type = DPT_IE_TYPE;
	dpt_ie->cap = htol16(dpt->cap);

	tlvs = (uint8 *)&dpt_ie[1];
	bcm_write_tlv(DPT_IE_TYPE_FNAME, dc->fname, dc->fnlen, tlvs);

	tlvs += TLV_HDR_LEN + dc->fnlen;
	bcm_write_tlv(DPT_IE_TYPE_BSSID, (uint8 *)&bsscfg->BSSID, ETHER_ADDR_LEN, tlvs);

	return (TLV_HDR_LEN + dpt_ie->len);
}

static brcm_prop_ie_t *
wlc_dpt_get_dpt_ie(uint8 *tlvs, int tlvs_len)
{
	uint8 *ie;
	brcm_prop_ie_t *dpt_ie = NULL;

	while ((ie = (uint8 *)bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_PROPR_ID))) {
		dpt_ie = (brcm_prop_ie_t *)ie;
		if ((dpt_ie->len >= BRCM_PROP_IE_LEN) &&
		    (!bcmp(&dpt_ie->oui[0], BRCM_PROP_OUI, DOT11_OUI_LEN)) &&
		    (dpt_ie->type == DPT_IE_TYPE))
			break;

		/* point to the next ie */
		ie += ie[TLV_LEN_OFF] + TLV_HDR_LEN;
		tlvs_len -= (int)(ie - tlvs);
		tlvs = ie;
	}

	/* validate that the dpt ie is properly formed */
	if (ie) {
		tlvs = (uint8 *)&dpt_ie[1];
		tlvs_len = dpt_ie->len - BRCM_PROP_IE_LEN;
		/* validate that the fname is there */
		if (bcm_parse_tlvs(tlvs, tlvs_len, DPT_IE_TYPE_FNAME) == NULL)
			return NULL;
		return dpt_ie;
	}

	return NULL;
}

#define wlc_dpt_set_pmstate(cfg) wlc_set_pmstate(cfg, (cfg)->pm->PMenabled)

static void
wlc_dpt_set_sec_mode(dpt_info_t *dpt, struct scb *scb)
{
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);
	wlc_bsscfg_t *bsscfg = scb->bsscfg;
	wlc_bsscfg_t *parent;
	wlc_dpt_t *dc;
	int bcmerror;
	int set_sup;

	ASSERT(memcmp(&bsscfg->cur_etheraddr, &scb->ea, ETHER_ADDR_LEN));

	/* cleanup the pre-existing sup/auth */
	bcmerror = wlc_iovar_op(dpt->wlc, "sup_wpa", NULL, 0, &set_sup, sizeof(int),
		IOV_GET, bsscfg->wlcif);
	if (set_sup) {
		set_sup = 0;
		bcmerror = wlc_iovar_op(dpt->wlc, "sup_wpa", NULL, 0, &set_sup, sizeof(int),
			IOV_SET, bsscfg->wlcif);
	}
	if (BCMAUTH_PSK_ENAB(dpt->wlc->pub) && bsscfg->authenticator) {
		wlc_authenticator_detach(bsscfg->authenticator);
		bsscfg->authenticator = NULL;
	}

	ASSERT(scb_dpt != NULL);

	scb_dpt->sec_mode = DPT_SEC_NONE;
	if (bsscfg->WPA_auth == WPA_AUTH_DISABLED) {
		scb_dpt->flags |= DPT_SCB_PORT_OPEN;
		return;
	}

	parent = scb_dpt->parent;
	ASSERT(parent != NULL);

	dc = parent->dpt;

	dc->po_pending++;
	dpt->po_pending++;
	ASSERT(dpt->po_pending <= dpt->wlc->pub->tunables->maxdpt);
	wlc_dpt_set_pmstate(parent);

	/* the smaller ether addr is supplicant, higher is authenticator */
	if (memcmp(&bsscfg->cur_etheraddr.octet[0], &scb->ea.octet[0], ETHER_ADDR_LEN) < 0) {
		set_sup = 1;
		WL_DPT(("wl%d: wlc_dpt_set_sec_mode: Setting up as Supplicant\n",
			dpt->wlc->pub->unit));
		scb_dpt->sec_mode = DPT_SEC_SUPPLICANT;

		bcmerror = wlc_iovar_op(dpt->wlc, "sup_wpa", NULL, 0, &set_sup, sizeof(int),
			IOV_SET, bsscfg->wlcif);

		if (bcmerror != BCME_OK) {
			WL_ERROR(("wl%d: %s: unable to attach sup\n",
				dpt->wlc->pub->unit, __FUNCTION__));
			return;
		}

		wlc_sup_set_ea(dpt->wlc->idsup, bsscfg, &scb->ea);
		wlc_sup_set_pmk(dpt->wlc->idsup, bsscfg, &dc->pmk, FALSE);

		wlc_sup_set_ssid(dpt->wlc->idsup, bsscfg, (uchar *)&bsscfg->BSSID, ETHER_ADDR_LEN);

	} else if (bcmp(&bsscfg->cur_etheraddr.octet[0], &scb->ea.octet[0], ETHER_ADDR_LEN) > 0) {
		WL_DPT(("wl%d: wlc_dpt_set_sec_mode: Setting up as Authenticator\n",
			dpt->wlc->pub->unit));
		scb_dpt->sec_mode = DPT_SEC_AUTHENTICATOR;

		bsscfg->authenticator = wlc_authenticator_attach(dpt->wlc, bsscfg);

		if (bsscfg->authenticator == NULL) {
			WL_ERROR(("wl%d: %s: unable to attach auth\n",
				dpt->wlc->pub->unit, __FUNCTION__));
			return;
		}

		wlc_auth_set_pmk(bsscfg->authenticator, &dc->pmk);
		wlc_auth_join_complete(bsscfg->authenticator, &scb->ea, TRUE);
	} else
		ASSERT(0);

}

void
wlc_dpt_port_open(dpt_info_t *dpt, struct ether_addr *ea)
{
	wlc_info_t *wlc = dpt->wlc;
	struct scb *scb;
	scb_dpt_t *scb_dpt;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* defined(BCMDBG) || defined(BCMDBG_ERR) */
	wlc_bsscfg_t *parent;

	scb = wlc_dpt_scbfindband_all(wlc, ea, CHSPEC_WLCBANDUNIT(wlc->home_chanspec));

	if (!scb || !BSSCFG_IS_DPT(scb->bsscfg)) {
		WL_ERROR(("wl%d: %s: dpt scb not found for ea %s\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(ea, eabuf)));
		return;
	}

	/* Authorize scb for data */
	wlc_ioctl(wlc, WLC_SCB_AUTHORIZE, ea, ETHER_ADDR_LEN, NULL);

	scb_dpt = SCB_DPT(dpt, scb);
	ASSERT(scb_dpt != NULL);

	parent = scb_dpt->parent;
	ASSERT(parent != NULL);

	scb_dpt->flags |= DPT_SCB_PORT_OPEN;
	parent->dpt->po_pending--;
	dpt->po_pending--;
	ASSERT(dpt->po_pending <= wlc->pub->tunables->maxdpt);
	wlc_dpt_set_pmstate(parent);

	wlc_dpt_update_path(dpt, scb, TRUE);
}

static void
wlc_dpt_update_path(dpt_info_t *dpt, struct scb *scb, bool force)
{
	wlc_info_t *wlc = dpt->wlc;
	bool old, new;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);
	void *p;
	wlc_bsscfg_t *parent;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	ASSERT(scb_dpt);

	/* no need to go forward if we recently switched; prevents flip-flop */
	if (!force &&
	    ((wlc->pub->now - scb_dpt->path_switch) < dpt->pathsel_freq))
		return;

	old = (scb_dpt->flags & DPT_SCB_ACTIVE);
	new = FALSE;

	/* DPT allowed if
	 * - If DPT connection established
	 * - and link is not suspended
	 * - and scb is authorized if security is ON
	 * - and either path selection chooses direct path or is forced direct path
	 */
	if ((scb_dpt->flags & DPT_SCB_PORT_OPEN) &&
	    !(scb_dpt->flags & DPT_SCB_SUSPENDED)) {
		if ((scb_dpt->pathsel == DPT_PATHSEL_DIRECT) ||
		    ((scb_dpt->pathsel == DPT_PATHSEL_AUTO) &&
		     (wlc_scb_rssi(scb) > dpt->rssi_threshold)))
			new = TRUE;
	}

	if (old != new) {
		scb_dpt->path_switch = wlc->pub->now;
		if (new) {

			ASSERT(!(scb_dpt->flags & DPT_SCB_SP_PENDING));

			scb_dpt->flags |= DPT_SCB_SP_PENDING;

			/* mark as active */
			scb_dpt->flags |= DPT_SCB_ACTIVE;

			/* Add the DPT to the txpath for this SCB */
			wlc_txmod_config(wlc, scb, TXMOD_DPT);

			/* send switch path notification */
			scb_dpt->sp_token = dpt->token++;
			wlc_dpt_send_sp(dpt, scb, DPT_ST_SP_AP_TO_DIRECT,
				scb_dpt->sp_token);

			WL_ASSOC_OR_DPT(("wl%d: DPT link with %s is pending active\n",
				wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
		} else {
			scb_dpt->flags &= ~DPT_SCB_ACTIVE;
			WL_ASSOC_OR_DPT(("wl%d: DPT link with %s is inactive\n",
				wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));

			if (scb_dpt->flags & DPT_SCB_SP_PENDING) {

				scb_dpt->flags &= ~DPT_SCB_SP_PENDING;

				/* Unconfigure the DPT from the txpath */
				wlc_txmod_unconfig(wlc, scb, TXMOD_DPT);

				/* release all the packets from the buffered queues */
				while ((p = pktq_deq(&scb_dpt->ubufq, NULL))) {
					parent = wlc_dpt_get_parent_bsscfg(wlc, scb);
					wlc_tdls_reinsert_pkt(wlc, parent, scb, p);
					WLCNTINCR(dpt->cnt->buf_reinserted);
				}
			}
		}
	}
}

void
wlc_dpt_used(dpt_info_t *dpt, struct scb *scb)
{
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);
	ASSERT(scb_dpt);
	scb_dpt->used = dpt->wlc->pub->now;
}

bool
wlc_dpt_pm_pending(dpt_info_t *dpt, wlc_bsscfg_t *parent)
{
	wlc_pm_st_t *pm = parent->pm;
	wlc_dpt_t *dc = parent->dpt;

	if (pm->PMenabled &&
	    (dc->ls_pending != 0 || dc->po_pending != 0))
		return TRUE;

	return FALSE;
}

/* sends dpt action frame to all dpt peers updating their DPT link state */
void
wlc_dpt_update_pm_all(dpt_info_t *dpt, wlc_bsscfg_t *parent, bool state)
{
	wlc_info_t *wlc = dpt->wlc;
	struct scb *scb;
	scb_dpt_t *scb_dpt;
	struct scb_iter scbiter;

	ASSERT(parent != NULL);

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_dpt_t *dc;

		if (!BSSCFG_IS_DPT(scb->bsscfg))
			continue;

		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt != NULL);

		/* no need to inform if port not open */
		if (!(scb_dpt->flags & DPT_SCB_PORT_OPEN))
			continue;

		if (parent != scb_dpt->parent)
			continue;

		dc = parent->dpt;

		if (state) {
			/* continue if peer already suspended or pending suspension */
			if ((scb_dpt->flags & DPT_SCB_PEER_SUSPENDED) ||
			    (scb_dpt->flags & DPT_SCB_LS_PENDING))
				continue;

			scb_dpt->flags |= DPT_SCB_LS_PENDING;
			scb_dpt->ls_pending_time = DPT_LS_PENDING_TIMEOUT;
			dc->ls_pending++;
			dpt->ls_pending++;
			ASSERT(dpt->ls_pending <= wlc->pub->tunables->maxdpt);
			scb_dpt->ls_token = dpt->token++;
			wlc_dpt_send_ls(dpt, scb, DPT_ST_LS_SUSPEND, scb_dpt->ls_token);
		} else {
			wlc_dpt_send_ls(dpt, scb, DPT_ST_LS_RESUME, dpt->token++);
			if (scb_dpt->flags & DPT_SCB_LS_PENDING) {
				scb_dpt->flags &= ~DPT_SCB_LS_PENDING;
				dc->ls_pending--;
				dpt->ls_pending--;
				ASSERT(dpt->ls_pending <= dpt->wlc->pub->tunables->maxdpt);
			}
			scb_dpt->flags &= ~DPT_SCB_PEER_SUSPENDED;
		}
	}

	wlc_set_ps_ctrl(parent);
}

static void
wlc_dpt_age_scb(dpt_info_t *dpt)
{
	wlc_info_t *wlc = dpt->wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	scb_dpt_t *scb_dpt;
	uint old_activeage = 0, old_disage = 0, age;
	struct scb *old_activescb = NULL, *old_disscb = NULL;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (BSSCFG_IS_DPT(scb->bsscfg)) {
			scb_dpt = SCB_DPT(dpt, scb);
			ASSERT(scb_dpt);
			if (scb_dpt->flags & DPT_SCB_MANUAL)
				continue;
			age = wlc->pub->now - scb_dpt->used;
			if (scb_dpt->flags & DPT_SCB_ACTIVE) {
				if (age >= old_activeage) {
					old_activeage = age;
					old_activescb = scb;
				}
			} else {
				if (age >= old_disage) {
					old_disage = age;
					old_disscb = scb;
				}
			}
		}
	}

	/* free oldest disabled scb first */
	if (old_disscb) {
		WL_DPT(("wl%d: wlc_dpt_age_scb: freeing dis scb (%s) age %d\n",
			wlc->pub->unit, bcm_ether_ntoa(&old_disscb->ea, eabuf), old_disage));
		wlc_dpt_disconnect(dpt, old_disscb, FALSE);
		return;
	}

	/* try to free oldest active scb if it is not too young */
	if (old_activescb) {
		if (old_activeage < dpt->scb_age_threshold) {
			WL_DPT(("wl%d: wlc_dpt_age_scb: cannot free young scb (%s) age %d\n",
				wlc->pub->unit, bcm_ether_ntoa(&old_activescb->ea, eabuf),
				old_activeage));
			return;
		}

		WL_DPT(("wl%d: wlc_dpt_age_scb: freeing active scb (%s) age %d\n",
			wlc->pub->unit, bcm_ether_ntoa(&old_activescb->ea, eabuf),
			old_activeage));
		wlc_dpt_disconnect(dpt, old_activescb, FALSE);
	}
}

/* function to find the lookaside entry with the matching ea */
static dpt_lookaside_t *
wlc_dpt_lookaside_find(dpt_info_t *dpt, struct ether_addr *ea)
{
	int i;
	dpt_lookaside_t *dla;

	for (i = 0; i < DPT_LOOKASIDE_SIZE; i++) {
		dla = &dpt->lookaside[i];
		if (!bcmp(&dla->ea, ea, ETHER_ADDR_LEN))
			return dla;
	}

	return NULL;
}

/* function to create the lookaside entry with the matching ea
 * Entry should not exist before calling this function
 */
static dpt_lookaside_t *
wlc_dpt_lookaside_create(dpt_info_t *dpt, struct ether_addr *ea)
{
	int i;

	ASSERT(!wlc_dpt_lookaside_find(dpt, ea));

	/* get next free entry */
	for (i = 0; i < DPT_LOOKASIDE_SIZE; i++) {
		if (!(dpt->lookaside[i].flags & DPT_LOOKASIDE_INUSE))
			break;
	}

	/* XXX: return for now if no free entry found;
	 * either age or increase size of lookaside
	 */
	if (i == DPT_LOOKASIDE_SIZE)
		return NULL;

	bcopy(ea, &dpt->lookaside[i].ea, ETHER_ADDR_LEN);
	dpt->lookaside[i].flags = DPT_LOOKASIDE_INUSE;
	dpt->lookaside[i].prb_sent = dpt->prb_freq;

	return &dpt->lookaside[i];
}

void
wlc_dpt_free_scb(dpt_info_t *dpt, struct scb *scb)
{
	scb_dpt_t *scb_dpt;
	wlc_bsscfg_t *parent;
	wlc_dpt_t *dc;

	scb_dpt = SCB_DPT(dpt, scb);
	ASSERT(scb_dpt != NULL);

	parent = scb_dpt->parent;
	ASSERT(parent != NULL);

	dc = parent->dpt;

	if (scb_dpt->flags & DPT_SCB_LS_PENDING) {
		dc->ls_pending--;
		dpt->ls_pending--;
		ASSERT(dpt->ls_pending <= dpt->wlc->pub->tunables->maxdpt);
	}

	if ((scb_dpt->flags & DPT_SCB_CONNECTED) &&
	    !(scb_dpt->flags & DPT_SCB_PORT_OPEN)) {
		dc->po_pending--;
		dpt->po_pending--;
		ASSERT(dpt->po_pending <= dpt->wlc->pub->tunables->maxdpt);
	}

	/* clear all flags except manual */
	scb_dpt->flags &= DPT_SCB_MANUAL;

	/* mark scb for deletion if it was not manual. Leads to race conditions
	 * if we delete here
	 */
	if (!(scb_dpt->flags & DPT_SCB_MANUAL))
		scb_dpt->free_me = TRUE;

	wlc_dpt_set_pmstate(parent);
}

static void
wlc_dpt_disconnect(dpt_info_t *dpt, struct scb *scb, bool free_scb)
{
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);
	dpt_packet_t dpkt;

	ASSERT(scb_dpt != NULL);

	if (scb_dpt->flags & DPT_SCB_CONNECTED) {
		bzero(&dpkt, sizeof(dpkt));

		dpkt.ver = DPT_PACKET_VERSION;
		dpkt.type = DPT_TYPE_CONNECT;
		dpkt.subtype = DPT_ST_DISCONNECT;
		dpkt.token = dpt->token++;
		dpkt.status = DPT_PKT_STATUS_SUCCESS;

		wlc_dpt_send_pkt(dpt, scb_dpt->parent, &scb->bsscfg->cur_etheraddr, &scb->ea,
			(uint8 *)&dpkt, sizeof(dpkt), 0);
	}

	wlc_dpt_free_scb(dpt, scb);
}

static void
wlc_dpt_send_connreq(dpt_info_t *dpt, struct scb *scb, uint8 token)
{
	uint8 *data;
	dpt_packet_t *dpkt;
	uint8 *pbody;
	int len, bcn_tmpl_len;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);
	wlc_info_t *wlc = dpt->wlc;

	ASSERT(scb_dpt != NULL);
	ASSERT(wlc);

	bcn_tmpl_len = wlc->pub->bcn_tmpl_len;
	if ((data = (uint8 *)MALLOC(wlc->osh, bcn_tmpl_len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return;
	}

	dpkt = (dpt_packet_t *)data;
	bzero(dpkt, sizeof(dpt_packet_t));

	dpkt->ver = DPT_PACKET_VERSION;
	dpkt->type = DPT_TYPE_CONNECT;
	dpkt->subtype = DPT_ST_CONNECT_REQ;
	dpkt->token = token;

	bcopy(&scb->bsscfg->BSSID, &dpkt[1], ETHER_ADDR_LEN);
	pbody = data + sizeof(dpt_packet_t) + ETHER_ADDR_LEN;
	len = bcn_tmpl_len - sizeof(dpt_packet_t) - ETHER_ADDR_LEN;
	wlc_dpt_prb_body(dpt->wlc, scb_dpt->parent, scb->bsscfg, pbody, &len);

	wlc_dpt_send_pkt(dpt, scb_dpt->parent, &scb->bsscfg->cur_etheraddr, &scb->ea,
		data, len + sizeof(dpt_packet_t) + ETHER_ADDR_LEN, WLF_DPT_APPATH);

	if (data) {
		MFREE(wlc->osh, (void *)data, bcn_tmpl_len);
	}
}

static void
wlc_dpt_send_connresp(dpt_info_t *dpt, wlc_bsscfg_t *parent,
	struct ether_addr *sa, struct ether_addr *da,
	uint8 token, uint8 status)
{
	dpt_packet_t dpkt;

	bzero(&dpkt, sizeof(dpkt));

	dpkt.ver = DPT_PACKET_VERSION;
	dpkt.type = DPT_TYPE_CONNECT;
	dpkt.subtype = DPT_ST_CONNECT_RESP;
	dpkt.token = token;
	dpkt.status = status;

	wlc_dpt_send_pkt(dpt, parent, sa, da, (uint8 *)&dpkt, sizeof(dpkt), WLF_DPT_APPATH);
}

static void
wlc_dpt_send_ls(dpt_info_t *dpt, struct scb *scb, uint8 subtype, uint8 token)
{
	dpt_packet_t dpkt;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);

	ASSERT(scb_dpt != NULL);

	bzero(&dpkt, sizeof(dpkt));

	dpkt.ver = DPT_PACKET_VERSION;
	dpkt.type = DPT_TYPE_LINK_STATE;
	dpkt.subtype = subtype;
	dpkt.token = token;

	wlc_dpt_send_pkt(dpt, scb_dpt->parent, &scb->bsscfg->cur_etheraddr, &scb->ea,
		(uint8 *)&dpkt, sizeof(dpkt), 0);
}

static void
wlc_dpt_send_hb(dpt_info_t *dpt, struct scb *scb)
{
	uint8 *data;
	dpt_packet_t *dpkt;
	uint8 *pbody;
	int len, bcn_tmpl_len;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);
	wlc_info_t *wlc = dpt->wlc;

	ASSERT(scb_dpt != NULL);
	ASSERT(wlc);

	bcn_tmpl_len = wlc->pub->bcn_tmpl_len;

	if ((data = (uint8 *)MALLOC(wlc->osh, bcn_tmpl_len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, %d bytes malloced\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return;
	}

	dpkt = (dpt_packet_t *)data;
	bzero(dpkt, sizeof(dpt_packet_t));

	dpkt->ver = DPT_PACKET_VERSION;
	dpkt->type = DPT_TYPE_HEART_BEAT;
	dpkt->subtype = DPT_ST_HB;
	dpkt->token = dpt->token++;

	pbody = data + sizeof(dpt_packet_t);
	len = bcn_tmpl_len - sizeof(dpt_packet_t);
	wlc_dpt_prb_body(dpt->wlc, scb_dpt->parent, scb->bsscfg, pbody, &len);

	wlc_dpt_send_pkt(dpt, scb_dpt->parent, &scb->bsscfg->cur_etheraddr, &scb->ea,
		data, len + sizeof(dpt_packet_t), WLF_DPT_DIRECT);

	if (data) {
		MFREE(wlc->osh, (void *)data, bcn_tmpl_len);
	}
}

static void
wlc_dpt_send_sp(dpt_info_t *dpt, struct scb *scb, uint8 subtype, uint8 token)
{
	dpt_packet_t dpkt;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);

	ASSERT(scb_dpt != NULL);

	bzero(&dpkt, sizeof(dpkt));
	dpkt.ver = DPT_PACKET_VERSION;
	dpkt.type = DPT_TYPE_SWITCH_PATH;
	dpkt.subtype = subtype;
	dpkt.token = token;

	wlc_dpt_send_pkt(dpt, scb_dpt->parent, &scb->bsscfg->cur_etheraddr, &scb->ea,
		(uint8 *)&dpkt, sizeof(dpkt), WLF_DPT_APPATH);
}

static void
wlc_dpt_send_pkt(dpt_info_t *dpt, wlc_bsscfg_t *parent,
	struct ether_addr *sa, struct ether_addr *da,
	uint8 *data, uint len, uint32 flags)
{
	wlc_info_t *wlc = dpt->wlc;
	osl_t *osh = wlc->osh;
	dpt_packet_t *dpkt = (dpt_packet_t *)data;
	struct ether_header *eh;
	bcmeth_hdr_t *bcmhdr;
	void *p;
	uint alloc_len;

	ASSERT(parent != NULL);

	WL_DPT(("wl%d: Sending DPT frame with type %s subtype %s token %d\n",
		wlc->pub->unit, dpt_type_str[dpkt->type],
		(dpkt->type == DPT_TYPE_CONNECT) ?
		dpt_st_con_str[dpkt->subtype] :
		(dpkt->type == DPT_TYPE_SWITCH_PATH) ?
		dpt_st_sp_str[dpkt->subtype] :
		(dpkt->type == DPT_TYPE_LINK_STATE) ?
		dpt_st_ls_str[dpkt->subtype] : dpt_st_hb_str[dpkt->subtype],
		dpkt->token));

	alloc_len = sizeof(struct ether_header) + sizeof(bcmeth_hdr_t) + len;

	/* alloc new frame buffer */
	if ((p = PKTGET(osh, (TXOFF + alloc_len), TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: pktget error for len %d\n",
			wlc->pub->unit, __FUNCTION__, (int)TXOFF + alloc_len));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return;
	}

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, alloc_len);

	/* init ether_header */
	eh = (struct ether_header *) PKTDATA(osh, p);
	bcopy(da, eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy(sa, eh->ether_shost, ETHER_ADDR_LEN);
	eh->ether_type = hton16(ETHER_TYPE_BRCM);

	/* add bcmeth_hdr_t fields */
	bcmhdr = (bcmeth_hdr_t *) &eh[1];
	bcmhdr->subtype = hton16(BCMILCP_SUBTYPE_VENDOR_LONG);
	bcmhdr->length = hton16(BCMILCP_BCM_SUBTYPEHDR_MINLENGTH + (uint16)len);
	bcmhdr->version = BCMILCP_BCM_SUBTYPEHDR_VERSION;
	bcopy(BRCM_OUI, &bcmhdr->oui[0], DOT11_OUI_LEN);
	bcmhdr->usr_subtype =  hton16(BCMILCP_BCM_SUBTYPE_DPT);

	bcopy(dpkt, (char *)&bcmhdr[1], len);

	WLPKTTAG(p)->flags |= (WLF_DPT_TYPE | flags);
	wlc_sendpkt(wlc, p, parent->wlcif);
}

static void
wlc_dpt_rcv_sp_ack(dpt_info_t *dpt, struct scb *scb, uint8 token)
{
	wlc_info_t *wlc = dpt->wlc;
	scb_dpt_t *scb_dpt = SCB_DPT(dpt, scb);
	void *p;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif

	ASSERT(scb_dpt != NULL);

	if (scb_dpt->flags & DPT_SCB_SP_PENDING) {

		ASSERT(scb_dpt->flags & DPT_SCB_ACTIVE);

		if (scb_dpt->sp_token != token) {
			WL_DPT(("wl%d: Recd token %d waiting for %d\n",
				wlc->pub->unit, token, scb_dpt->sp_token));
			return;
		}

		/* release all the packets from the buffered queue */
		while ((p = pktq_deq(&scb_dpt->ubufq, NULL))) {
			SCB_TX_NEXT(TXMOD_DPT, scb, p, WLC_PRIO_TO_PREC(PKTPRIO(p)));
		}

		/* Unconfigure the DPT from the txpath */
		wlc_txmod_unconfig(wlc, scb, TXMOD_DPT);

		scb_dpt->flags &= ~DPT_SCB_SP_PENDING;

		WL_ASSOC_OR_DPT(("wl%d: DPT link with %s is active\n",
			wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
	}
}

/* Returns true if packet consumed */
bool
wlc_dpt_rcv_pkt(dpt_info_t *dpt, struct wlc_frminfo *f)
{
	wlc_info_t *wlc = dpt->wlc;
	osl_t *osh = wlc->osh;
	struct scb *scb;
	dpt_packet_t *dpkt;
	scb_dpt_t *scb_dpt;
	uint plen, ilen;
	uchar *pdata;
	bcmeth_hdr_t *bcmhdr;
#if defined(BCMDBG) || defined(BCMDBG_ERR)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* defined(BCMDBG) || defined(BCMDBG_ERR) */

	pdata = PKTDATA(osh, f->p);
	plen = PKTLEN(osh, f->p);

#ifdef EXT_STA
	if (WLEXTSTA_ENAB(wlc->pub))
		ilen = DOT11_A3_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN;
	else
#endif // endif
	ilen = ETHER_HDR_LEN;

	/* validate that it has the bcmeth_hdr at the minimum */
	if (plen < (ilen + sizeof(bcmeth_hdr_t)))
		return FALSE;

	bcmhdr = (bcmeth_hdr_t *)(pdata + ilen);

	/* validate that it is a DPT protocol data frame */
	if ((ntoh16(bcmhdr->subtype) != BCMILCP_SUBTYPE_VENDOR_LONG) ||
	    bcmp(BRCM_OUI, &bcmhdr->oui[0], DOT11_OUI_LEN) ||
	    (ntoh16(bcmhdr->usr_subtype) != BCMILCP_BCM_SUBTYPE_DPT)) {
		WL_ERROR(("wl%d: %s: not a dpt data frame\n",
			wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}

	plen -= (ilen + sizeof(bcmeth_hdr_t));
	pdata += ilen + sizeof(bcmeth_hdr_t);

	if (plen < sizeof(dpt_packet_t)) {
		WL_ERROR(("wl%d: recd truncated dpt content\n", wlc->pub->unit));
		goto toss;
	}

	dpkt = (dpt_packet_t *)pdata;

	WL_DPT(("wl%d: Recd DPT frame with type %s subtype %s token %d\n",
		wlc->pub->unit, dpt_type_str[dpkt->type],
		(dpkt->type == DPT_TYPE_CONNECT) ?
		dpt_st_con_str[dpkt->subtype] :
		(dpkt->type == DPT_TYPE_SWITCH_PATH) ?
		dpt_st_sp_str[dpkt->subtype] :
		(dpkt->type == DPT_TYPE_LINK_STATE) ?
		dpt_st_ls_str[dpkt->subtype] : dpt_st_hb_str[dpkt->subtype],
		dpkt->token));

	scb = wlc_dpt_scbfindband_all(wlc, f->sa, CHSPEC_WLCBANDUNIT(wlc->home_chanspec));

	/* validate that a DPT scb exists except for a conn req */
	if ((dpkt->type != DPT_TYPE_CONNECT) || (dpkt->subtype != DPT_ST_CONNECT_REQ)) {
		if (!scb || !BSSCFG_IS_DPT(scb->bsscfg)) {
			WL_ERROR(("wl%d: %s: dpt scb not found for ea %s\n",
				wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(f->sa, eabuf)));
			return FALSE;
		}
	}

	switch (dpkt->type) {
	case DPT_TYPE_CONNECT:
		switch (dpkt->subtype) {
		case DPT_ST_CONNECT_REQ:
			wlc_dpt_rcv_connreq(dpt, f, pdata, plen);
			break;
		case DPT_ST_CONNECT_RESP:
			wlc_dpt_rcv_connresp(dpt, scb, pdata, plen);
			break;
		case DPT_ST_DISCONNECT:
			wlc_dpt_free_scb(dpt, scb);
			break;
		default:
			WL_ERROR(("wl%d: unexpected DPT subtype\n", wlc->pub->unit));
		}
		break;
	case DPT_TYPE_SWITCH_PATH:
		switch (dpkt->subtype) {
		case DPT_ST_SP_AP_TO_DIRECT:
			wlc_dpt_send_sp(dpt, scb, DPT_ST_SP_ACK, dpkt->token);
			break;
		case DPT_ST_SP_ACK:
			wlc_dpt_rcv_sp_ack(dpt, scb, dpkt->token);
			break;
		default:
			WL_ERROR(("wl%d: unexpected DPT subtype\n", wlc->pub->unit));
		}
		break;
	case DPT_TYPE_LINK_STATE:
		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt != NULL);

		switch (dpkt->subtype) {
		case DPT_ST_LS_SUSPEND:
			wlc_dpt_send_ls(dpt, scb, DPT_ST_LS_ACK, dpkt->token);
			if (!(scb_dpt->flags & DPT_SCB_SUSPENDED)) {
				scb_dpt->flags |= DPT_SCB_SUSPENDED;
				wlc_dpt_update_path(dpt, scb, TRUE);
			}
			break;
		case DPT_ST_LS_RESUME:
			if (scb_dpt->flags & DPT_SCB_SUSPENDED) {
				scb_dpt->flags &= ~DPT_SCB_SUSPENDED;
				wlc_dpt_update_path(dpt, scb, TRUE);
			}
			break;
		case DPT_ST_LS_ACK:
			if (scb_dpt->flags & DPT_SCB_LS_PENDING) {
				wlc_bsscfg_t *parent;
				wlc_dpt_t *dc;

				if (scb_dpt->ls_token != dpkt->token) {
					WL_DPT(("wl%d: Recd token %d waiting for %d\n",
						wlc->pub->unit, dpkt->token, scb_dpt->ls_token));
					break;
				}

				parent = scb_dpt->parent;
				ASSERT(parent != NULL);

				dc = parent->dpt;

				scb_dpt->flags &= ~DPT_SCB_LS_PENDING;
				dc->ls_pending--;
				dpt->ls_pending--;
				ASSERT(dpt->ls_pending <= wlc->pub->tunables->maxdpt);
				scb_dpt->flags |= DPT_SCB_PEER_SUSPENDED;
				if (dc->ls_pending != 0)
					break;
				wlc_dpt_set_pmstate(parent);
			}
			break;
		default:
			WL_ERROR(("wl%d: unexpected DPT subtype\n", wlc->pub->unit));
		}
		break;
	case DPT_TYPE_HEART_BEAT:
		switch (dpkt->subtype) {
		case DPT_ST_HB:
			scb_dpt = SCB_DPT(dpt, scb);
			ASSERT(scb_dpt != NULL);

			if (scb_dpt->flags & DPT_SCB_PORT_OPEN)
				scb_dpt->hb_rcvd = wlc->pub->now;
			break;
		default:
			WL_ERROR(("wl%d: unexpected DPT subtype\n", wlc->pub->unit));
		}
		break;
	default:
		WL_ERROR(("wl%d: unexpected DPT type\n", wlc->pub->unit));
	}

toss:
	PKTFREE(osh, f->p, FALSE);
	return TRUE;
}

static uint
wlc_dpt_calc_dpt_ie_len(void *ctx, wlc_iem_calc_data_t *data)
{
	wlc_bsscfg_t *cfg = data->cfg;

	if (BSSCFG_IS_DPT(cfg)) {
		wlc_dpt_t *dc = cfg->dpt;

		return TLV_HDR_LEN + BRCM_PROP_IE_LEN +
		        TLV_HDR_LEN + dc->fnlen +
		        TLV_HDR_LEN + ETHER_ADDR_LEN;
	}

	return 0;
}

static int
wlc_dpt_write_dpt_ie(void *ctx, wlc_iem_build_data_t *data)
{
	dpt_info_t *dpt = (dpt_info_t *)ctx;
	wlc_bsscfg_t *cfg = data->cfg;

	if (BSSCFG_IS_DPT(cfg))
		return wlc_dpt_write_ie(dpt, cfg, data->buf, data->buf_len);

	return BCME_OK;
}

static int
wlc_dpt_prq_parse_dpt_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	dpt_info_t *dpt = (dpt_info_t *)ctx;
	wlc_info_t *wlc = dpt->wlc;
	wlc_iem_pparm_t *pparm = data->pparm;

	if (!DPT_ENAB(wlc->pub))
		return BCME_OK;

	if (data->ie == NULL)
		return BCME_OK;

	wlc_dpt_recv_process_prbreq(dpt, pparm->wrxh, pparm->plcp,
	                            (struct dot11_management_header *)pparm->hdr,
	                            data->ie, data->ie_len);

	return BCME_OK;
}

static int
wlc_dpt_prs_parse_dpt_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	dpt_info_t *dpt = (dpt_info_t *)ctx;
	wlc_info_t *wlc = dpt->wlc;
	wlc_iem_pparm_t *pparm = data->pparm;

	if (!DPT_ENAB(wlc->pub))
		return BCME_OK;

	if (data->ie == NULL)
		return BCME_OK;

	wlc_dpt_recv_process_prbresp(dpt, pparm->wrxh, pparm->plcp,
	                             (struct dot11_management_header *)pparm->hdr,
	                             data->ie, data->ie_len);

	return BCME_OK;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
const char *dpt_pathsel_str[] = { DPT_PATHSEL_STR };
const char *dpt_scbflags_str[] = { DPT_SCBFLAGS_STR };
const int dpt_scbflags_cnt = sizeof(dpt_scbflags_str)/sizeof(dpt_scbflags_str[0]);

static void
wlc_dpt_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b)
{
	dpt_info_t *dpt = (dpt_info_t *)context;
	wlc_info_t *wlc = dpt->wlc;
	scb_dpt_t *scb_dpt;
	sta_info_t sta;
	int i;

	if (BSSCFG_IS_DPT(scb->bsscfg)) {
		scb_dpt = SCB_DPT(dpt, scb);
		ASSERT(scb_dpt);

		wlc_sta_info(wlc, scb->bsscfg, &scb->ea, &sta, sizeof(sta_info_t));
		bcm_bprintf(b, "    %s(%d): pathsel %s rx %d tx %d used %d rssi %d flags 0x%x:",
		            scb_dpt->fname, scb_dpt->fnlen,
		            dpt_pathsel_str[scb_dpt->pathsel],
		            sta.rx_ucast_pkts, sta.tx_pkts, scb_dpt->used,
		            wlc_scb_rssi(scb), scb_dpt->flags);
		for (i = 0; i < dpt_scbflags_cnt; i++) {
			if (scb_dpt->flags & (1 << i))
				bcm_bprintf(b, " %s", dpt_scbflags_str[i]);
		}
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "    parent bsscfg: %d\n", WLC_BSSCFG_IDX(scb_dpt->parent));
	}
}
#endif /* BCMDBG || BCMDBG_DUMP */

#ifdef BCMDBG
static int
wlc_dpt_dump(dpt_info_t *dpt, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = dpt->wlc;
	wlc_dpt_cnt_t *cnt = dpt->cnt;
	struct scb *scb;
	struct scb_iter scbiter;
	char eabuf[ETHER_ADDR_STR_LEN];
	int i;

	bcm_bprintf(b, "dpt %d now %d cur_sessions %d max_sessions %d "
		"discovery %d discoverable %d\n",
		wlc->pub->_dpt, wlc->pub->now, dpt->cur_sessions, dpt->max_sessions,
		dpt->discovery, dpt->discoverable);
	bcm_bprintf(b, "pathsel %s prb_freq %d hb_freq %d pathsel_freq %d "
		"ls_pending %d po_pending %d\n",
		dpt_pathsel_str[dpt->pathsel], dpt->prb_freq,
		dpt->hb_freq, dpt->pathsel_freq, dpt->ls_pending, dpt->po_pending);
	bcm_bprintf(b, "discard %d ubuffered %d buf_reins %d\n",
		cnt->discard, cnt->ubuffered, cnt->buf_reinserted);
	bcm_bprintf(b, "txprbreq %d rxdptprbreq %d rxallprbreq %d\n",
		cnt->txprbreq, cnt->rxdptprbreq, cnt->rxallprbreq);
	bcm_bprintf(b, "txprbresp %d rxdptprbresp %d rxallprbresp %d\n",
		cnt->txprbresp, cnt->rxdptprbresp, cnt->rxallprbresp);

	bcm_bprintf(b, "DPT lookaside list is\n");
	for (i = 0; i < DPT_LOOKASIDE_SIZE; i++) {
		if (dpt->lookaside[i].flags & DPT_LOOKASIDE_INUSE) {
			bcm_bprintf(b, "%s prb_sent %d %s\n",
				bcm_ether_ntoa(&dpt->lookaside[i].ea, eabuf),
				dpt->lookaside[i].prb_sent,
				(dpt->lookaside[i].flags & DPT_LOOKASIDE_PERM) ?
					"perm" : "");
		}
	}

	bcm_bprintf(b, "DPT stations are\n");
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_dpt_scb_dump(dpt, scb, b);
	}

	return 0;
}
#endif /* BCMDBG */
