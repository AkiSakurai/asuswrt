/*
 * Association/Roam related routines
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_assoc.h 781321 2019-11-18 15:44:35Z $
 */

#ifndef __wlc_assoc_h__
#define __wlc_assoc_h__

/* assoc->state values */
#define AS_IDLE			0	/* Idle state */
#define AS_JOIN_INIT		1
#define AS_SCAN			2	/* Scan state */
#define AS_WAIT_IE		3	/* waiting for association ie */
#define AS_WAIT_IE_TIMEOUT	4	/* TimeOut waiting for assoc ie */
#define AS_JOIN_START		5	/* Join start state */
#define AS_SENT_FTREQ		6	/* Sent FT Req state */
#define AS_RECV_FTRES		7	/* Received FT Resp state */
#define AS_WAIT_TX_DRAIN	8	/* Waiting for tx packets to drain out */
#define AS_SENT_AUTH_1		9	/* Sent Authentication 1 state */
#define AS_SENT_AUTH_2		10	/* Sent Authentication 2 state */
#define AS_SENT_AUTH_3		11	/* Sent Authentication 3 state */
#define AS_SENT_ASSOC		12	/* Sent Association state */
#define AS_REASSOC_RETRY	13	/* Sent Assoc on DOT11_SC_REASSOC_FAIL */
#define AS_JOIN_ADOPT		14	/* Adopting BSS Params */
#define AS_WAIT_RCV_BCN		15	/* Waiting to receive beacon state */
#define AS_SYNC_RCV_BCN		16	/* Waiting Re-sync beacon state */
#define AS_WAIT_DISASSOC	17	/* Waiting to disassociate state */
#define AS_WAIT_PASSHASH	18	/* calculating passhash state */
#define AS_RECREATE_WAIT_RCV_BCN 19	/* wait for former BSS/IBSS bcn in assoc_recreate */
#define AS_ASSOC_VERIFY		20	/* allow former AP time to disassoc/deauth before PS mode */
#define AS_LOSS_ASSOC		21	/* Loss association */
#define AS_JOIN_CACHE_DELAY	22
#define AS_WAIT_FOR_AP_CSA	23	/* Wait for AP to send CSA to clients */
#define AS_WAIT_FOR_AP_CSA_ROAM_FAIL 24	/* Wait for AP to send CSA to clients when roam fails */
#define AS_MODE_SWITCH_START	25
#define AS_MODE_SWITCH_COMPLETE	26
#define AS_MODE_SWITCH_FAILED	27
#define AS_DISASSOC_TIMEOUT	28
#define AS_IBSS_CREATE		29	/* No IBSS peers found, STA will create IBSS */
#define AS_DFS_CAC_START	30	/* DFS Slave CAC begins */
#define AS_DFS_CAC_FAIL		31	/* DFS Slave detected radar during CAC state */
#define AS_DFS_ISM_INIT		32	/* DFS state machine notified to begin ISM */
#define AS_RETRY_ESS		33	/* Retry the entire join process */
#define AS_NEXT_ESS		34	/* Try the next BSSID using join process */
#define AS_RETRY_BSS		35	/* Retry assoc on DOT11_SC_ASSOC_TRY_LATER by AP */
#define AS_NEXT_BSS		36	/* Try the next join target */
#define AS_SCAN_ABORT		37
#define AS_START_EXT_AUTH	38	/* Trigger the external authentication */
#define AS_SENT_AUTH_UP		39	/* Send the Auth request 2 and 4 to upper layer */
#define AS_LAST_STATE		40

/* assoc->type values */
#define AS_NONE			0 /* No association */
#define AS_ASSOCIATION		1 /* Join association */
#define AS_ROAM			2 /* Roaming association */
#define AS_RECREATE		3 /* Re-Creating a prior association */
#define AS_VERIFY_TIMEOUT		4 /* Re-Creating Timedout */

#define AS_VALUE_NOT_SET	100 /* indicates uninitilized association state or type */

/* assoc->flags */
/* exclude reset AS_F_DO_CHANSWITCH calling assoc init as part of DFS channel switch reassoc */
#define AS_F_INIT_RESET		(AS_F_CACHED_RESULTS |\
					AS_F_CACHED_CHANNELS |\
					AS_F_SPEEDY_RECREATE |\
					AS_F_CACHED_ROAM)

#define AS_F_CACHED_RESULTS	0x0001	/* Assoc used cached results */
#define AS_F_CACHED_CHANNELS	0x0002	/* Assoc used cached results to create a channel list */
#define AS_F_SPEEDY_RECREATE	0x0004	/* Speedy association recreation */
#define AS_F_CACHED_ROAM	0x0008	/* Assoc used cached results when directed roaming */
#define AS_F_DO_CHANSWITCH	0x0010	/* Reassoc after DFS channel switch announcement */

typedef struct join_pref {
	uint32 score;
	uint32 rssi;
} join_pref_t;

#ifdef STA

extern void wlc_assoc_timer_add(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

extern int wlc_join(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const uint8 *SSID, int len,
	wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len);
extern void wlc_join_recreate(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

extern int wlc_join_cmd_proc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wlc_ssid_t *ssid, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len);

extern void wlc_join_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len);
extern int wlc_join_recreate_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len);

void wlc_join_done(wlc_bsscfg_t *cfg, struct ether_addr *bssid, int bss_type, int status);

void wlc_join_pref_reset_cond(wlc_bsscfg_t *cfg);

extern bool wlc_update_pref_score(void *join_pref_list, uint8 index,
	uint32 threshold, uint32 score);

extern int wlc_reassoc(wlc_bsscfg_t *cfg, wl_reassoc_params_t *reassoc_params);

extern int wlc_roam_scan(wlc_bsscfg_t *cfg, uint reason, chanspec_t *list, uint32 channum);
extern int wlc_roamscan_start(wlc_bsscfg_t *cfg, uint roam_reason);
extern void wlc_txrate_roam(wlc_info_t *wlc, struct scb *scb, tx_status_t *txs, bool pkt_sent,
	bool pkt_max_retries, uint8 ac);
extern void wlc_build_roam_cache(wlc_bsscfg_t *cfg, wlc_bss_list_t *candidates);
extern void wlc_roam_bcns_lost(wlc_bsscfg_t *cfg);
extern bool wlc_roam_scan_islazy(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool roam_scan_isactive);
extern bool wlc_lazy_roam_scan_suspend(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern bool wlc_lazy_roam_scan_sync_dtim(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_roam_handle_join(wlc_bsscfg_t *cfg);
extern void wlc_roam_handle_beacon_loss(wlc_bsscfg_t *cfg);
extern void wlc_roam_handle_missed_beacons(wlc_bsscfg_t *cfg, uint32 missed_beacons);
extern void wlc_roam_update_time_since_bcn(wlc_bsscfg_t *cfg, uint8 new_time);

extern int wlc_disassociate_client(wlc_bsscfg_t *cfg, bool send_disassociate);

extern int wlc_assoc_abort(wlc_bsscfg_t *cfg);

#if defined(WL_ASSOC_MGR)
/*
* Continue an association paused via assoc_mgr module.
* cfg - bsscfg the assoc was paused on
* addr - address of the scb that the assoc paused on
* Returns error if assoc was not paused or unable to restart
*/
extern int wlc_assoc_continue(wlc_bsscfg_t *cfg, struct ether_addr *addr);
#endif /* defined(WL_ASSOC_MGR) */

void wlc_restart_sta(wlc_info_t *wlc);
void wlc_resync_sta(wlc_info_t *wlc);
void wlc_resync_recv_bcn(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_bcn_prb *bcn);

extern void wlc_assoc_change_state(wlc_bsscfg_t *cfg, uint newstate);
extern void wlc_authresp_client(wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, uint8 *body, uint body_len, bool short_preamble);
extern void wlc_assocresp_client(wlc_bsscfg_t *cfg, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, uint body_len);
extern void wlc_process_assocresp_decision(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	assoc_decision_t * dc);

extern void wlc_sta_assoc_upd(wlc_bsscfg_t *cfg, bool state);

#ifdef ROBUST_DISASSOC_TX
extern void wlc_disassoc_tx(wlc_bsscfg_t *cfg, bool send_disassociate);
#endif /* ROBUST_DISASSOC_TX */

void wlc_link_monitor_watchdog(wlc_info_t *wlc);

extern wlc_bsscfg_t * wlc_find_assoc_in_progress(wlc_info_t *wlc, struct ether_addr *bssid);
#endif /* STA */

extern int wlc_mac_request_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req, uint scan_flags);

extern void wlc_roam_defaults(wlc_info_t *wlc, wlcband_t *band, int *roam_trigger, uint *rm_delta);

extern void wlc_disassoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint disassoc_reason, uint bss_type);
extern void wlc_deauth_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	const struct ether_addr *addr, uint deauth_reason, uint bss_type);

struct wlc_deauth_send_cbargs {
	struct ether_addr	ea;
	int8			_idx;
	void                    *pkt;
};
extern void wlc_send_deauth(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	struct ether_addr *da, struct ether_addr *bssid,
	struct ether_addr *sa, uint16 reason_code);
extern void wlc_deauth_sendcomplete_clean(wlc_info_t *wlc, struct scb *scb);

extern void wlc_disassoc_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint disassoc_reason, uint bss_type,
	uint8 *body, int body_len);
extern void wlc_deauth_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint deauth_reason, uint bss_type,
	uint8 *body, int body_len);
extern void wlc_assoc_update_dtim_count(wlc_bsscfg_t *cfg, uint16 dtim_count);
#ifdef WLPFN
void wl_notify_pfn(wlc_info_t *wlc);
#endif /* WLPFN */

extern void wlc_assoc_bcn_mon_off(wlc_bsscfg_t *cfg, bool off, uint user);

extern void wlc_join_adopt_ibss_params(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern int wlc_join_start_ibss(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int8 bsstype);
extern int wlc_join_start_prep(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

extern int wlc_assoc_iswpaenab(wlc_bsscfg_t *cfg, bool wpa);

extern void wlc_assoc_init(wlc_bsscfg_t *cfg, uint type);
extern void wlc_adopt_dtim_period(wlc_bsscfg_t *cfg, uint8 dtim_period);

extern uint32 wlc_bss_pref_score(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi, bool band_rssi_boost,
	uint32 *prssi);

extern int wlc_assoc_chanspec_sanitize(wlc_info_t *wlc, chanspec_list_t *list, int len,
	wlc_bsscfg_t *cfg);

#if defined(STA) && defined(AP)
extern void wlc_try_join_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params);
#endif /* STA && AP */

#ifdef WLSCANCACHE
bool wlc_assoc_cache_validate_timestamps(wlc_info_t *wlc, wlc_bss_list_t *bss_list);
#endif // endif
extern bool wlc_assoc_check_roam_candidate(wlc_bsscfg_t *cfg, wlc_bss_info_t *candidate_bi);

/** report assoc/roam state */
typedef struct {
	wlc_bsscfg_t *cfg;
	uint type;	/* See assoc->type values in wlc.h */
	uint state;	/* See assoc->state values in wlc.h */
} bss_assoc_state_data_t;
typedef void (*bss_assoc_state_fn_t)(void *arg, bss_assoc_state_data_t *notif_data);
int wlc_bss_assoc_state_register(wlc_info_t *wlc, bss_assoc_state_fn_t fn, void *arg);
int wlc_bss_assoc_state_unregister(wlc_info_t *wlc, bss_assoc_state_fn_t fn, void *arg);

/** report disassoc state */
typedef struct {
	wlc_bsscfg_t *cfg;
	uint type;	/* See DAN_TYPE_XXX */
	uint state;	/* See DAN_ST_XXX */
	const struct ether_addr *addr;
} bss_disassoc_notif_data_t;
typedef void (*bss_disassoc_notif_fn_t)(void *arg, bss_disassoc_notif_data_t *notif_data);
int wlc_bss_disassoc_notif_register(wlc_info_t *wlc, bss_disassoc_notif_fn_t fn, void *arg);
int wlc_bss_disassoc_notif_unregister(wlc_info_t *wlc, bss_disassoc_notif_fn_t fn, void *arg);

/* disassoc type */
#define DAN_TYPE_PEER_DEAUTH	0x01	/* originated from peer - not implemented */
#define DAN_TYPE_PEER_DISASSOC	0x02
#define DAN_TYPE_LOCAL_DEAUTH	0x11	/* originated from local */
#define DAN_TYPE_LOCAL_DISASSOC	0x12

/* disassoc state */
#define DAN_ST_DEAUTH_START	0x01	/* process started - not implemented */
#define DAN_ST_DEAUTH_CMPLT	0x10	/* process completed */
#define DAN_ST_DISASSOC_START	0x11
#define DAN_ST_DISASSOC_CMPLT	0x20

#ifdef STA
/* report time_since_bcn count */
/* not merge to assoc/roam satte report to not bother other module */
typedef struct {
	wlc_bsscfg_t *cfg;
	uint time_since_bcn; /* in milisecond */
} bss_time_since_bcn_notif_data_t;
typedef void (*bss_time_since_bcn_notif_fn_t)
	(void *arg, bss_time_since_bcn_notif_data_t *notif_data);
int wlc_bss_time_since_bcn_notif_register
	(wlc_assoc_info_t *as, bss_time_since_bcn_notif_fn_t fn, void *arg);
int wlc_bss_time_since_bcn_notif_unregister
	(wlc_assoc_info_t *as, bss_time_since_bcn_notif_fn_t fn, void *arg);
#endif /* STA */
/* return the <N>th join target from the current candidate, <N> is specified by 'offset' */
wlc_bss_info_t *wlc_assoc_get_join_target(wlc_info_t *wlc, int offset);
void wlc_assoc_next_join_target(wlc_info_t *wlc);
/* set the 'cfg' as the next join requestor */
void wlc_assoc_set_as_cfg(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
/* get the next join requestor */
wlc_bsscfg_t *wlc_assoc_get_as_cfg(wlc_info_t *wlc);
/* unregister packet call back for timed out pkt */
extern void wlc_assoc_unregister_pcb_on_timeout(wlc_info_t *wlc,
	wlc_bsscfg_t *cfg);

#ifdef STA
extern void wlc_join_attempt(wlc_bsscfg_t *cfg);
extern void wlc_join_attempt_select(wlc_bsscfg_t *cfg);
extern void wlc_join_bss_prep(wlc_bsscfg_t *cfg);
extern void wlc_join_bss_start(wlc_bsscfg_t *cfg);
extern void wlc_join_bss(wlc_bsscfg_t *cfg);
extern int wlc_join_attempt_info_get(wlc_bsscfg_t *cfg, uint8 *arg, int len);
extern int wlc_roam_cache_info_get(wlc_bsscfg_t *cfg, void *params,
	uint p_len, void *arg, uint len);
#endif /* STA */

#define AS_IN_PROGRESS_CFG(wlc)	wlc_assoc_get_as_cfg(wlc)
#define AS_IN_PROGRESS(wlc)	(AS_IN_PROGRESS_CFG(wlc) != NULL)
#define AS_IN_PROGRESS_WLC(wlc)	(AS_IN_PROGRESS(wlc) && \
				 BSS_MATCH_WLC(wlc, AS_IN_PROGRESS_CFG(wlc)))

/* ******** WORK-IN-PROGRESS ******** */

/* TODO: make these structure opaque */

/* module info */

/** per bsscfg association states */
struct wlc_assoc {
	struct wl_timer *timer;		/**< timer for auth/assoc request timeout */
	uint	type;			/**< roam, association, or recreation */
	uint	state;			/**< current state in assoc process */
	uint	flags;			/**< control flags for assoc */

	bool	preserved;		/**< Whether association was preserved */
	uint	recreate_bi_timeout;	/**< bcn intervals to wait to detect our former AP
					 * when performing an assoc_recreate
					 */
	uint	verify_timeout;		/**< ms to wait to allow an AP to respond to class 3
					 * data if it needs to disassoc/deauth
					 */
	uint8	retry_max;		/**< max. retries */
	uint8	ess_retries;		/**< number of ESS retries */
	uint8	bss_retries;		/**< number of BSS retries */

	uint16	capability;		/**< next (re)association request overrides */
	uint16	listen;
	struct ether_addr bssid;
	uint8	*ie;
	uint	ie_len;

	struct dot11_assoc_req *req;	/**< last (re)association request */
	uint	req_len;
	bool	req_is_reassoc;		/**< is a reassoc */
	struct dot11_assoc_resp *resp;	/**< last (re)association response */
	uint	resp_len;

	bool	rt;			/**< true if sta retry timer active */

	/* ROBUST_DISASSOC_TX */
	uint8	disassoc_tx_retry;		/**< disassoc tx retry number */
	bool block_disassoc_tx;

	struct ether_addr last_upd_bssid;	/**< last BSSID updated to the host */
	uint disassoc_txstatus;			/**< dissassoc packet acknowledgement */
	wlc_msch_req_handle_t	*req_msch_hdl;	/* hdl to msch request */
	uint16	dtim_count;			/* DTIM count */
	uint	state_rsdb;			/* preserved state in rsdb switch */
};

/* per bsscfg roam states */
#define ROAM_CACHELIST_SIZE		4
#define MAX_ROAM_CHANNEL		20
#define WLC_SRS_DEF_ROAM_CHAN_LIMIT	6	/* No. of cached channels for Split Roam Scan */

#define WL_RMC_REPORT_CMD_VERSION		1

/* Features that use downgraded RSDB scan */
typedef enum {
	SCAN_DOWNGRADED_AP_SCAN = 0x01,		/* Downgraded scan feature for AP active */
	SCAN_DOWNGRADED_P2P_DISC_SCAN = 0x02,	/* Downgraded scan feature for P2P Discovery */
	SCAN_DOWNGRADED_CH_PRUNE_ROAM = 0x10,	/* Enable channel pruning for ROAM SCAN */
	SCAN_DOWNGRADED_CH_PRUNE_ALL  = 0x20,	/* Enable channel pruning for any SCAN */
} downgraded_scan_features_t;

/* assoc scan state changes for pruning AP band channels */
typedef enum roam_prune_type {
	ROAM_PRUNE_NONE,
	ROAM_PRUNE_APBAND_CHANNELS, /* Do not scan any channel in AP band other than AP's channel */
	ROAM_PRUNE_NON_APBAND_CHANNELS /* reverse of ROAM_PRUNE_APBAND_CHANNELS */
} roam_prune_type_t;

struct wlc_roam {
	uint	bcn_timeout;		/**< seconds w/o bcns until loss of connection */
	bool	assocroam;		/**< roam to preferred assoc band in oid bcast scan */
	bool	off;			/**< disable roaming */
	uint8	time_since_bcn;		/**< second count since our last beacon from AP */
	uint8	minrate_txfail_cnt;	/**< tx fail count at min rate */
	uint8	minrate_txpass_cnt;	/**< number of consecutive frames at the min rate */
	uint	minrate_txfail_thresh;	/**< min rate tx fail threshold for roam */
	uint	minrate_txpass_thresh;	/**< roamscan threshold for being stuck at min rate */
	uint	txpass_cnt;		/**< turn off roaming if we get a better tx rate */
	uint	txpass_thresh;		/**< turn off roaming after x many packets */
	uint32	tsf_h;			/**< TSF high bits (to detect retrograde TSF) */
	uint32	tsf_l;			/**< TSF low bits (to detect retrograde TSF) */
	uint	scan_block;		/**< roam scan frequency mitigator */
	uint	ratebased_roam_block;	/**< limits mintxrate/txfail roaming frequency */
	uint	partialscan_period;	/**< user-specified roam scan period */
	uint	fullscan_period; 	/**< time between full roamscans */
	uint	reason;			/**< cache so we can report w/WLC_E_ROAM event */
	uint	original_reason;	/**< record the reason for precise reporting on link down */
	bool	active;			/**< RSSI based roam in progress */
	bool	cache_valid;		/**< RSSI roam cache is valid */
	uint	time_since_upd;		/**< How long since our update? */
	uint	fullscan_count;		/**< Count of full rssiroams */
	int	prev_rssi;		/**< Prior RSSI, used to invalidate cache */
	uint	cache_numentries;	/**< # of rssiroam APs in cache */
	bool	thrash_block_active;	/**< Some/all of our APs are unavaiable to reassoc */
	bool	motion;			/**< We are currently moving */
	int	RSSIref;		/**< trigger for motion detection */
	uint16	motion_dur;		/**< How long have we been moving? */
	uint16	motion_timeout;		/**< Time left using modifed values */
	uint8	motion_rssi_delta;	/**< Motion detect RSSI delta */
	bool	roam_scan_piggyback;	/**< Use periodic broadcast scans as roam scans */
	bool	piggyback_enab;		/**< Turn on/off roam scan piggybacking */
	struct {			/**< Roam cache info */
		chanspec_t chanspec;
		struct ether_addr BSSID;
		uint16 time_left_to_next_assoc;
		union {
			struct {
				uint8 reason;
				uint8 status;
			} roam_st;
			int16 rssi;
		} rssi_st;
		uint16 bss_load;
		uint32 time;
		uint32 fullscan_cnt;
	} cached_ap[ROAM_CACHELIST_SIZE];
	uint8	ap_environment;		/**< Auto-detect the AP density of the environment */
	bool	bcns_lost;		/**< indicate if the STA can see the AP */
	uint8	consec_roam_bcns_lost;	/**< counter to keep track of consecutive calls
					 * to wlc_roam_bcns_lost function
					 */
	uint8	roam_on_wowl; /* trigger roam scan (on bcn loss) for prim cfg from wowl watchdog */
	uint8   nfullscans;     /* Number of full scans before switching to partial scans */

	/* WLRCC: Roam Channel Cache */
	bool    rcc_mode;
	bool    rcc_valid;
	bool	roam_scan_started;
	bool	roam_fullscan;
	uint16	n_rcc_channels;
	chanspec_t *rcc_channels;
	uint16  max_roam_time_thresh;   /* maximum roam threshold time in ms */
	/* Roam hot channel cache */
	chanvec_t roam_chn_cache;	/**< split roam scan channel cache */
	bool	roam_chn_cache_locked;	/**< indicates cache was cfg'd manually */
	uint8	roam_chn_cache_limit;	/**< Max chan to use from cache for split roam scan */

	/* Split roam scan */
	int16	roam_trigger_aggr;	/**< aggressive trigger values set by host for roaming */
	uint16	roam_delta_aggr;	/**< aggressive roam mode - roam delta value */
	bool	multi_ap;		/**< TRUE when there are more than one AP for the SSID */
	uint8	split_roam_phase;	/**< tracks the phases of a split roam scan */
	bool	split_roam_scan;	/**< flag for enabling split roam scans */
	uint8	ci_delta;		/**< roam cache invalidate delta */
	uint	roam_type;		/**< Type of roam (standard, partial, split phase) */

	wl_reassoc_params_t *reassoc_param;
	uint8	reassoc_param_len;
	struct wl_timer *timer;		/**< timer to start roam after uatbtt */
	bool timer_active;		/**< Is roam timer active? */
	uint	prev_bcn_timeout;

	/* OPPORTUNISTIC_ROAM */
	bool	oppr_roam_off;		/**< disable opportunistic roam */

	int8	roam_rssi_boost_thresh;
	int8	roam_rssi_boost_delta;	/**< RSSI boost can be negative */
	int16	roam_prof_idx;		/**< current roaming profile (based upon band/RSSI) */

	uint16  prev_scan_home_time;
	uint32  link_monitor_last_update;	/**< last time roam scan counters decreased */

	uint	bcn_thresh;		/**< bcn intervals w/o bcn --> bcn lost event */
	uint	bcn_interval_cnt;	/**< count of bcn intervals since last bcn */
	bool    roam_bcnloss_off;      /* disable roaming for beacon loss */
	uint8	prune_type;	/* Pruning in the scan module for channels in our SoftAPs band. */

#ifdef BCMDBG
	uint8	tbtt_since_bcn;		/**< tbtt count since our last beacon from AP */
#endif // endif
};

extern void wlc_assoc_homech_req_update(wlc_bsscfg_t *bsscfg);
extern void wlc_assoc_homech_req_unregister(wlc_bsscfg_t *bsscfg);

/* attach/detach interface */
wlc_assoc_info_t *wlc_assoc_attach(wlc_info_t *wlc);
void wlc_assoc_detach(wlc_assoc_info_t *asi);

extern uint8 wlc_assoc_get_prune_type(wlc_info_t *wlc);
extern bool wlc_assoc_is_associating(wlc_bsscfg_t* bsscfg);
/* ******** WORK-IN-PROGRESS ******** */

extern int8 wlc_assoc_get_as_state(wlc_bsscfg_t *cfg);
extern int
wlc_ext_auth_tx_complete(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt,
        void *arg);
#endif /* __wlc_assoc_h__ */
