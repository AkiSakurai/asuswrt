/*
 * BSS Config related declarations and exported functions for
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_bsscfg.h 775473 2019-05-31 08:04:47Z $
 */
#ifndef _WLC_BSSCFG_H_
#define _WLC_BSSCFG_H_

#include <typedefs.h>
#include <wlc_types.h>
#include <bcmwifi_channels.h>
#include <wlc_pub.h>
#include <wlc_rate.h>

/* Check if a particular BSS config is AP or STA */
#if defined(AP) && !defined(STA)
#define BSSCFG_AP(cfg)		(1)
#define BSSCFG_STA(cfg)		(0)
#define BSSCFG_INFRA_STA(cfg)	(0)
#elif !defined(AP) && defined(STA)
#define BSSCFG_AP(cfg)		(0)
#define BSSCFG_STA(cfg)		(1)
#define BSSCFG_INFRA_STA(cfg)	((BSSCFG_STA((cfg))) && ((cfg)->BSS))
#else
#define BSSCFG_AP(cfg)		((cfg)->_ap)
#define BSSCFG_STA(cfg)		(!(BSSCFG_AP((cfg))))
#define BSSCFG_INFRA_STA(cfg)	((BSSCFG_STA((cfg))) && ((cfg)->BSS))
#endif /* defined(AP) && !defined(STA) */

#define BSSCFG_INFRA_STA_OR_AP(cfg)	(BSSCFG_INFRA_STA(cfg) || BSSCFG_AP(cfg))

#ifdef STA
#define BSSCFG_IBSS(cfg)	(/* BSSCFG_STA(cfg) && */!(cfg)->BSS)
#else
#define BSSCFG_IBSS(cfg)	(0)
#endif // endif

#ifdef PSTA
#define BSSCFG_PSTA(cfg)	((cfg)->_psta)
#else /* PSTA */
#define BSSCFG_PSTA(cfg)	(0)
#endif /* PSTA */

#ifdef WLSLOTTED_BSS
#define BSSCFG_SLOTTED_BSS(cfg)    ((cfg) && ((cfg)->type == BSSCFG_TYPE_SLOTTED_BSS))
#else
#define BSSCFG_SLOTTED_BSS(cfg) 0
#endif // endif
#ifdef WL_CLIENT_SAE
#define BSSCFG_EXT_AUTH(cfg) (((cfg)->auth == WL_AUTH_SAE_KEY) && (BSSCFG_STA(cfg)))
#else
#define BSSCFG_EXT_AUTH(cfg) FALSE
#endif /* WL_CLIENT_SAE */
#define BSSCFG_NAN_MGMT(cfg)	FALSE
#define BSSCFG_NAN_DATA(cfg) FALSE
#define BSSCFG_NAN(cfg) (BSSCFG_NAN_MGMT(cfg) || BSSCFG_NAN_DATA(cfg))
#ifdef ACKSUPR_MAC_FILTER
#define BSSCFG_ACKSUPR(cfg) (BSSCFG_AP(cfg) && cfg->acksupr_mac_filter)
#endif /* ACKSUPR_MAC_FILTER */

#ifdef WLP2P
#define BSSCFG_P2PDISC(cfg)	(cfg->subtype == BSSCFG_P2P_DISC && cfg->type == BSSCFG_TYPE_P2P)
#else
#define BSSCFG_P2PDISC(cfg)	FALSE
#endif /* WLP2P */

#define BSSCFG_AS_STA(cfg) (BSSCFG_STA(cfg) && cfg->associated)
#define BSSCFG_AP_UP(cfg) (BSSCFG_AP(cfg) && cfg->associated)
/* Check if a BSSCFG is primary or not */
#define BSSCFG_IS_PRIMARY(cfg)    ((cfg) == wlc_bsscfg_primary((cfg)->wlc))

/*
 * XXX We need to include the interface definition before the bsscfg before they
 * hardly tied. Should we do more either dynamic allocation or parallel array
 * referencing ?
 */
/* forward declarations */
struct scb;
struct tdls_bss;
struct bssload_bss;

#include <wlioctl.h>
#include <bcm_notif_pub.h>

#define WLC_BSSCFG_RATESET_DEFAULT	0
#define WLC_BSSCFG_RATESET_OFDM		1
#define WLC_BSSCFG_RATESET_MAX		WLC_BSSCFG_RATESET_OFDM

#ifdef WLRSDB
#define BSS_MATCH_WLC(_wlc, cfg) ((_wlc) == ((cfg)->wlc))
#else
#define BSS_MATCH_WLC(_wlc, cfg) TRUE
#endif // endif

/* Iterator for all bsscfgs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_BSS(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if (((cfg = (wlc)->bsscfg[idx]) != NULL) && \
			BSS_MATCH_WLC((wlc), cfg))

/* Iterator for all ibss bsscfgs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_IBSS(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if (((cfg = (wlc)->bsscfg[idx]) != NULL) && (!cfg->BSS) &&\
			BSS_MATCH_WLC((wlc), cfg))

/* Iterator for "associated" bsscfgs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_AS_BSS(wlc, idx, cfg) \
	for (idx = 0; cfg = NULL, (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((wlc)->bsscfg[idx] != NULL && \
		    (wlc)->bsscfg[idx]->associated && \
		    (cfg = (wlc)->bsscfg[idx]) != NULL && \
			BSS_MATCH_WLC((wlc), cfg))

/* Iterator for "ap" bsscfgs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_AP(wlc, idx, cfg) \
	for (idx = 0; cfg = NULL, (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((wlc)->bsscfg[idx] != NULL && BSSCFG_AP((wlc)->bsscfg[idx]) && \
		    (cfg = (wlc)->bsscfg[idx]) != NULL && \
			BSS_MATCH_WLC((wlc), cfg))

/* Iterator for "up" AP bsscfgs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_UP_AP(wlc, idx, cfg) \
	for (idx = 0; cfg = NULL, (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((wlc)->bsscfg[idx] != NULL && BSSCFG_AP((wlc)->bsscfg[idx]) && \
		    (wlc)->bsscfg[idx]->up && \
		    ((cfg = (wlc)->bsscfg[idx]) != NULL) && \
			BSS_MATCH_WLC((wlc), cfg))

/* Iterator for "sta" bsscfgs:	(wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
/* Iterator for STA bss configs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_STA(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((cfg = (wlc)->bsscfg[idx]) && BSSCFG_STA(cfg) && \
			BSS_MATCH_WLC((wlc), cfg))

/* Iterator for "associated" STA bss configs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_AS_STA(wlc, idx, cfg) \
	for (idx = 0; cfg = NULL, (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((wlc)->bsscfg[idx] != NULL && BSSCFG_STA((wlc)->bsscfg[idx]) && \
		    (wlc)->bsscfg[idx]->associated && \
		    ((cfg = (wlc)->bsscfg[idx]) != NULL) && \
			BSS_MATCH_WLC((wlc), cfg))

/* As above for all up PSTA BSS configs */
#define FOREACH_UP_PSTA(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((cfg = (wlc)->bsscfg[idx]) && BSSCFG_PSTA(cfg) && cfg->up && \
			BSS_MATCH_WLC((wlc), cfg))

/* As above for all PSTA BSS configs */
#define FOREACH_PSTA(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((cfg = (wlc)->bsscfg[idx]) && BSSCFG_PSTA(cfg) && \
			BSS_MATCH_WLC((wlc), cfg))

#define FOREACH_ALL_WLC_BSS(wlc, idx, cfg) \
	for (idx = 0; idx < WLC_MAXBSSCFG; ++idx) \
		if ((cfg = (wlc)->bsscfg[idx]) != NULL)

#define WLC_IS_CURRENT_BSSID(cfg, bssid) \
	(!bcmp((char*)(bssid), (char*)&((cfg)->BSSID), ETHER_ADDR_LEN))
#define WLC_IS_CURRENT_SSID(cfg, ssid, len) \
	(((len) == (cfg)->SSID_len) && \
	 !bcmp((char*)(ssid), (char*)&((cfg)->SSID), (len)))

/* Iterator for all bsscfgs across wlc:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOR_ALL_UP_BSS(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if (((cfg = (wlc)->bsscfg[idx]) != NULL) && \
			(wlc)->bsscfg[idx]->up)

#ifdef WLP2P
#define FOREACH_P2P_BSS(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if (((cfg = (wlc)->bsscfg[idx]) != NULL) && \
			BSS_MATCH_WLC((wlc), cfg) && \
			BSS_P2P_ENAB((wlc), cfg))
#endif /* WLP2P */

/* Iterator for all bsscfgs across wlc:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOR_ALL_UP_AP_BSS(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if (((cfg = (wlc)->bsscfg[idx]) != NULL) && \
			(wlc)->bsscfg[idx]->up && cfg->_ap)

#ifdef DWDS
/* DWDS STA loopback list */
#define DWDS_SA_HASH_SZ		(32)
#define DWDS_SA_EXPIRE_TIME	(10)		/* 10 secs */
#define DWDS_SA_HASH(ea)	((ea[5] + ea[4] + ea[3] + ea[2] + ea[1]) & (DWDS_SA_HASH_SZ - 1))

typedef struct dwds_sa {
	struct ether_addr sa;
	uint32 last_used;
	struct dwds_sa *next;
} dwds_sa_t;
#endif /* DWDS */

#define AP_BSS_UP_COUNT(wlc) wlc_ap_bss_up_count(wlc)

#define IS_PRIMARY_BSSCFG_IDX(idx)	((idx) == 0)

/* connection states */
/* XXX TODO: move states in wlc_assoc_t & wlc_roam_t that are used to detect/monitor
 * beacon lost and/or shared by both BSS and IBSS to here so that later on we can
 * optimize the memory usage by not allocating wlc_roam_t if possible...
 */
typedef struct {
	mbool	ign_bcn_lost_det;	/**< skip bcn lost detection */
} wlc_cxn_t;

/* ign_bcn_lost_det requestor */
#define BCN_MON_REQ_NAN		(1<<0)

#ifdef WL_TRAFFIC_THRESH
typedef struct wlc_trf_data {
	uint16 cur;
	uint16 count;
	uint16 *num_data;
} wlc_trf_data_t;
typedef struct wlc_intfer_params {
	uint16 num_secs;
	uint16 thresh;
} wlc_intfer_params_t;
#endif /* WL_TRAFFIC_THRESH */

/**
 * Stores information about the relation between the local entity and a BSS. Information about the
 * BSS that is not specific for this local entity is stored in struct wlc_bss_info instead.
 */
struct wlc_bsscfg {
	wlc_info_t	*wlc;		/**< wlc to which this bsscfg belongs to. */
	bool		up;		/**< is this configuration up operational */
	bool		enable;		/**< is this configuration enabled */
	bool		_ap;		/**< is this configuration an AP */
	bool		_psta;		/**< is this configuration a PSTA */
	bool		associated;	/**< is BSS in ASSOCIATED state */
	struct wlc_if	*wlcif;		/**< virtual interface, NULL for primary bsscfg */
	bool		BSS;		/**< infrastructure or adhoc */
	bool		dtim_programmed;
	uint8		SSID_len;	/**< the length of SSID */
	uint8		SSID[DOT11_MAX_SSID_LEN];	/**< SSID string */
	bool		closednet_nobcnssid;	/**< hide ssid info in beacon */
	bool		closednet_nobcprbresp;	/**< Don't respond to broadcast probe requests */
	uint8		ap_isolate;	/**< ap isolate bitmap 0: disabled, 1: all 2: mulicast */
	struct scb      *bcmc_scb;      /**< common bcmc_scb to transmit broadcast frames */

	int8		_idx;           /**< index of this bsscfg assigned in wlc_bsscfg_alloc() */

	/* Multicast filter list */
	bool		allmulti;		/**< enable all multicasts */
	uint		nmulticast;		/**< # enabled multicast addresses */
	struct ether_addr	*multicast; 	/**< ptr to list of multicast addresses */

	/* security */
	uint32		wsec;		/**< wireless security bitvec */
	int16		auth;		/**< 802.11 authentication: Open, Shared Key, WPA */
	int16		openshared;	/**< try Open auth first, then Shared Key */
	bool		wsec_restrict;	/**< drop unencrypted packets if wsec is enabled */
	bool		eap_restrict;	/**< restrict data until 802.1X auth succeeds */
	uint32		WPA_auth;	/**< WPA: authenticated key management */
	bool		wsec_portopen;	/**< indicates keys are plumbed */
	bool		is_WPS_enrollee; /* To allow a join to an encrypted AP while wsec is 0
					  * for the purposes of geting WPS credentials
					  */
	/* MBSS || WLP2P */
	uint32		maxassoc;	/**< Max associations for this bss */

	struct ether_addr	BSSID;		/**< BSSID (associated) */
	struct ether_addr	cur_etheraddr;	/**< h/w address */
	uint16                  bcmc_fid;	/**< the last BCMC FID queued to TX_BCMC_FIFO */
	uint16                  bcmc_fid_shm;	/**< the last BCMC FID written to shared mem */

	uint32		flags;		/**< WLC_BSSCFG flags; see below */
	bsscfg_type_t	type;		/**< bsscfg type */
	bsscfg_subtype_t subtype; /* bit field indicating subtypes supported by this cfg */

	/* STA */
	/* Association parameters. Used to limit the scan in join process. Saved before
	 * starting a join process and freed after finishing the join process regardless
	 * if the join is succeeded or failed.
	 */
	wl_join_assoc_params_t	*assoc_params;
	uint16			assoc_params_len;

	uint16			AID;		/**< association id for each bss */

	struct ether_addr	prev_BSSID;	/**< MAC addr of last associated AP (BSS) */

	int		auth_atmptd;	/**< auth type (open/shared) attempted */

	wlc_bss_info_t	*target_bss;	/**< BSS parms during tran. to ASSOCIATED state */
	wlc_bss_info_t	*current_bss;	/**< BSS parms in ASSOCIATED state */

	wlc_assoc_t	*assoc;		/**< association mangement */
	wlc_roam_t	*roam;		/**< roam states */
	wlc_link_qual_t	*link;		/**< link quality monitor */
	wlc_pm_st_t	*pm;		/**< power management */

	/* join targets sorting preference */
	wlc_join_pref_t *join_pref;
	/* Give RSSI score of APs in preferred band a boost
	 * to make them fare better instead of always preferring
	 * the band. This is maintained separately from regular
	 * join pref as no order can be imposed on this preference
	 */
	struct {
		uint8 band;
		int8 rssi;
	} join_pref_rssi_delta;
	struct ether_addr join_bssid;
	int8	PLCPHdr_override;	/**< 802.11b Preamble Type override */

	/* 'unique' ID of this bsscfg, assigned at bsscfg allocation */
	uint16		ID;

	uint		txrspecidx;		/**< index into tx rate circular buffer */
	ratespec_t     	txrspec[NTXRATE][2];	/**< circular buffer of prev MPDUs tx rates */

	/* STA */
	/* Scan parameters. Used to modify the scan parameters in join process.
	 * Saved before starting a join process and freed after finishing the join
	 * regardless if the join is succeeded or failed.
	 */
	wl_join_scan_params_t	*scan_params;

	/* WME */
	wlc_wme_t	*wme;			/**< WME config and ACP used locally */

	/* WLTDLS */
	/* Tunnel Direct Link Setup */
	struct tdls_bss	*tdls;
	uint8		tdls_cap;	/* TDLS fast data path */

	/* Broadcom proprietary information element */
	uint8		*brcm_ie;

	uint8		ext_cap_len;	/* extend capability length */
	uint8		*ext_cap;	/* extend capability */

	uint8		oper_mode; /* operating mode notification value */
	bool		oper_mode_enabled; /* operating mode is enabled */

	wlc_cxn_t *cxn;

	wl_scan_params_t *roam_scan_params; /* customize roam scans */

	bool _dwds;	/**< Dynamic WDS */

	bool		mcast_regen_enable;	/**< Multicast Regeneration is enabled or not */
	bool		wmf_enable;		/**< WMF is enabled or not */
	bool		wmf_ucast_igmp;		/**< 1 to enable, 0 by default */

	bool		is_awdl;
	int8 rateset;				/**< bss rateset */

	/* ps_pretend_retry_limit is for normal ps pretend, where ps pretend is activated
	 * on the very first failure so preventing all packet loss. When zero, this feature
	 * is completely inoperative. The ps_pretend_retry_limit sets the maximum number
	 * of successive attempts to enter ps pretend when a packet is having difficulty
	 * to be delivered.
	 */
	uint8   ps_pretend_retry_limit;

	/* ps_pretend_threshold sets the number of successive failed TX status that
	 * must occur in order to enter the 'threshold' ps pretend. When zero, the 'threshold'
	 * ps pretend is completely inoperative.
	 * This threshold means packets are always lost, they begin to be saved
	 * once the threshold has been met.
	 */
	uint8	ps_pretend_threshold;

	struct bssload_bss	*bssload;	/**< BSS load reporting */

	uint32 flags2;				/**< bsscfg flags */

#ifdef ACKSUPR_MAC_FILTER
	bool acksupr_mac_filter; /* enable or disable acksupr */
#endif /* ACKSUPR_MAC_FILTER */
	uint8 ibss_up_pending;	/* TRUE, if ibss bringup is pending in case of modesw */

	bool mcast_filter_nosta;	/* Drop mcast frames on BSS if no STAs associated */
	uint8   bcn_to_dly;             /* delay link down until roam complete */
	uint8 nss_sup;			/* max nss capability of the associated AP */
	bool disable_ap_up;	/* for APSTA, prevent AP cfg UP, till STA not up and
				 * associated to upstream AP
				 */
#ifdef WL_GLOBAL_RCLASS
	uint16			scb_without_gbl_rclass;	/* number of associated sta support global
							 * regulatory class
							 */
#endif /* WL_GLOBAL_RCLASS */
#ifdef MULTIAP
	uint8			*multiap_ie;	/* Multi-AP ie */
	bool			_map;		/* Multi-AP */
	uint8			map_attr;	/* Multi-AP attribute values */
#endif	/* MULTIAP */
#if defined(WLATF) && defined(WLATF_PERC)
	uint8			bssperc;	/* ATF percentage per bsscfg */
	uint8			sched_bssperc;	/* Schedule context ATF percentage per bsscfg */
#endif // endif
	/* ====== !!! ADD NEW FIELDS ABOVE HERE !!! ====== */
#ifdef WL_TRAFFIC_THRESH
	bool traffic_thresh_enab;
	uint16 trf_cfg_enable;
	wlc_intfer_params_t trf_cfg_params[WL_TRF_MAX_QUEUE];
	wlc_trf_data_t trf_cfg_data[WL_TRF_MAX_QUEUE];
	uint8 trf_scb_enable;			/* per_scb bit map of enabled access category */
	wlc_intfer_params_t trf_scb_params[WL_TRF_MAX_QUEUE];	/* per_scb settings for txfail */
#endif /* WL_TRAFFIC_THRESH */
#ifdef DWDS
	bool		dwds_loopback_filter;
	dwds_sa_t	*sa_list[DWDS_SA_HASH_SZ]; /* list of clients attached to the DWDS STA */
#endif /* DWDS */
	int8 far_sta_rssi;	/* far_sta_rssi value */
	uint8 edca_update_count; /* edca update counter, shared between HE muedca and QoS cfg */

	uint32 mem_bytes; /* bytes of memory allocated for this bsscfg */

	uint8 block_as_retry; /* block assoc retry in wlc_watchdog */

#ifdef BCMDBG
	/* ====== LEAVE THESE AT THE END ====== */
	/* Rapid PM transition */
	wlc_hrt_to_t *rpmt_timer;
	uint32	rpmt_1_prd;
	uint32	rpmt_0_prd;
	uint8	rpmt_n_st;
	void *last;	/* last field marker */
#endif // endif
};
/* wlc_bsscfg_t flags */
#define WLC_BSSCFG_PRESERVE     0x1		/**< preserve STA association on driver down/up */
#define WLC_BSSCFG_WME_DISABLE	0x2		/**< Do not advertise/use WME for this BSS */
#define WLC_BSSCFG_PS_OFF_TRANS	0x4		/**< BSS is in transition to PS-OFF */
#define WLC_BSSCFG_SW_BCN	0x8		/**< The BSS is generating beacons in SW */
#define WLC_BSSCFG_SW_PRB	0x10		/**< The BSS is generating probe responses in SW */
#define WLC_BSSCFG_HW_BCN	0x20		/**< The BSS is generating beacons in HW */
#define WLC_BSSCFG_HW_PRB	0x40		/**< The BSS is generating probe responses in HW */
#define WLC_BSSCFG_RSDB_IF	0x80		/**< The bsscfg pair created for connections
						 * like awdl/nan
						 */
#define WLC_BSSCFG_WME_ASSOC	0x100		/**< This Infra STA has WME association */
#define WLC_BSSCFG_RSDB_CLONE	0x200		/**< RSDB BSSCFG clone indication, during move */
#define WLC_BSSCFG_NOBCMC	0x400		/**< The BSS has no broadcast/multicast traffic */
#define WLC_BSSCFG_NOIF		0x800		/**< The BSS has no OS presentation */
#define WLC_BSSCFG_11N_DISABLE	0x1000		/**< Do not advertise .11n IEs for this BSS */
#define WLC_BSSCFG_PSINFO	0x2000		/**< The BSS supports psinfo */
#define WLC_BSSCFG_11H_DISABLE	0x4000		/**< Do not follow .11h rules for this BSS */
#define WLC_BSSCFG_NATIVEIF	0x8000		/**< The BSS uses native OS if */
#define WLC_BSSCFG_P2P_DISC	0x10000		/**< The BSS is for p2p discovery */
#define WLC_BSSCFG_TX_SUPR	0x20000		/**< The BSS is in absence mode */
#define WLC_BSSCFG_SRADAR_ENAB	0x40000		/**< follow special radar rules for soft/ext ap */
#define WLC_BSSCFG_DYNBCN	0x80000		/**< Do not beacon if no client is associated */
#define WLC_BSSCFG_AP_NORADAR_CHAN  0x00400000	/**< disallow ap to start on radar channel */
#define WLC_BSSCFG_ASSOC_RECR	0x00800000	/* cfg created by assoc recreate */
#define WLC_BSSCFG_USR_RADAR_CHAN_SELECT	0x01000000	/**< User will perform radar
								 * random chan select.
								 */
#define WLC_BSSCFG_BSSLOAD_DISABLE  0x02000000	/**< Do not enable BSSLOAD for this BSS */
#define WLC_BSSCFG_VHT_DISABLE	    0x04000000	/**< Do not advertise VHT IEs for this BSS */
#define WLC_BSSCFG_HE_DISABLE	    0x08000000	/**< Do not advertise HE IEs for this BSS */
#define WLC_BSSCFG_TX_SUPR_ENAB	0x20000000	/**< The BSS supports absence supprssion re-que */
#define WLC_BSSCFG_NO_AUTHENTICATOR 0x40000000 /* The BSS has no in-driver authenticator */
#define WLC_BSSCFG_ALLOW_FTOVERDS	0x80000000	/**< Use of FBT Over-the-DS is allowed */

#define WLC_BSSCFG_FL2_RSDB_LINKED	0x8	/* Other core's BSSCFG is linked to it */
#define WLC_BSSCFG_FL2_MODESW_BWSW	0x10	/**< The BSS is for ModeSW bandwidth flag */
#define WLC_BSSCFG_FL2_MFP_CAPABLE	0x20	/**< The BSS is for MFP capable flag */
#define WLC_BSSCFG_FL2_MFP_REQUIRED	0x40	/**< The BSS is for MFP required flag */
#define WLC_BSSCFG_FL2_FBT_1X		0x80	/**< The BSS is for FBT 802.1X */
#define WLC_BSSCFG_FL2_FBT_PSK		0x100	/**< The BSS is for FBT PSK */
#define WLC_BSSCFG_FL2_RSDB_NOT_ACTIVE	0x400	/* To indicate an inactive primary cfg */
#define WLC_BSSCFG_FL2_TBOW_ACTIVE	0x800	/* The flag is for TBOW Active on the BSS */
#define WLC_BSSCFG_FL2_CSA_IN_PROGRESS	0x1000	/* The BSS is doing CSA */
#define WLC_BSSCFG_FL2_RESERVED1	0x4000	/* Available/Free for use */
#define WLC_BSSCFG_FL2_NO_KM		0x8000	/* The BSS is not support Key Management (NO_KM) */
#define WLC_BSSCFG_FL2_AKM_REQUIRES_MFP 0x10000  /* current AKM forces MFP required */
#define WLC_BSSCFG_FL2_SPLIT_ASSOC_REQ	0x20000	/* The BSS split assoc req into two parts */
#define WLC_BSSCFG_FL2_SPLIT_ASSOC_RESP	0x40000	/* The BSS split assoc resp into two parts */

#define BSSCFG_HAS_PSINFO(cfg)	(((cfg)->flags & WLC_BSSCFG_PSINFO) != 0)
#define BSSCFG_SET_PSINFO(cfg)	((cfg)->flags |= WLC_BSSCFG_PSINFO)

#if defined(WLRSDB) /* ensure BSS_MATCH() for RSDB */
#define WLC_BSSCFG(wlc, idx) \
	(((idx) < WLC_MAXBSSCFG && ((int)(idx)) >= 0) ? \
	(((wlc)->bsscfg[idx] && BSS_MATCH_WLC((wlc), (wlc)->bsscfg[idx])) ? \
	(wlc)->bsscfg[idx]: NULL) :NULL)
#else
#define WLC_BSSCFG(wlc, idx) \
	(((idx) < WLC_MAXBSSCFG && ((int)(idx)) >= 0) ? ((wlc)->bsscfg[idx]) : NULL)
#endif /* WLRSDB */

#define HWBCN_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_HW_BCN) != 0)
#define HWPRB_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_HW_PRB) != 0)
#define SOFTBCN_ENAB(cfg)	(((cfg)->flags & WLC_BSSCFG_SW_BCN) != 0)
#define SOFTPRB_ENAB(cfg)	(((cfg)->flags & WLC_BSSCFG_SW_PRB) != 0)
#define DYNBCN_ENAB(cfg)	(((cfg)->flags & WLC_BSSCFG_DYNBCN) != 0)

#define BSSCFG_HAS_NOIF(cfg)	(((cfg)->flags & WLC_BSSCFG_NOIF) != 0)
#define BSSCFG_HAS_NATIVEIF(cfg)	(((cfg)->flags & WLC_BSSCFG_NATIVEIF) != 0)
#define BSSCFG_IS_RSDB_CLONE(cfg)	(((cfg)->flags & WLC_BSSCFG_RSDB_CLONE) != 0)
#define BSSCFG_IS_RSDB_IF(cfg)		(((cfg)->flags & WLC_BSSCFG_RSDB_IF) != 0)
#define BSSCFG_SET_RSDB_IF(cfg)		((cfg)->flags |= WLC_BSSCFG_RSDB_IF)

#define BSSCFG_IS_ASSOC_RECREATED(cfg)	(((cfg)->flags & WLC_BSSCFG_ASSOC_RECR) != 0)
#define BSSCFG_SET_ASSOC_RECREATED(cfg)	((cfg)->flags |= WLC_BSSCFG_ASSOC_RECR)

#ifdef WLTDLS
#define BSSCFG_IS_TDLS(cfg)	((cfg)->type ==  BSSCFG_TYPE_TDLS)
#else
#define BSSCFG_IS_TDLS(cfg)	FALSE
#endif // endif

#ifdef MFP
#define BSSCFG_IS_MFP_CAPABLE(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_MFP_CAPABLE) != 0)
#define BSSCFG_IS_MFP_REQUIRED(cfg) (((cfg)->flags2 & (WLC_BSSCFG_FL2_MFP_REQUIRED | \
				WLC_BSSCFG_FL2_AKM_REQUIRES_MFP)) != 0)
#define BSSCFG_SET_MFP_CAPABLE(cfg) ((cfg->flags2) |= WLC_BSSCFG_FL2_MFP_CAPABLE)
#define BSSCFG_SET_AKM_REQUIRES_MFP(cfg) ((cfg->flags2) |= WLC_BSSCFG_FL2_AKM_REQUIRES_MFP)
#define BSSCFG_CLEAR_AKM_REQUIRES_MFP(cfg) ((cfg->flags2) &= ~WLC_BSSCFG_FL2_AKM_REQUIRES_MFP)
#else
#define BSSCFG_IS_MFP_CAPABLE(cfg)	FALSE
#define BSSCFG_IS_MFP_REQUIRED(cfg)	FALSE
#endif // endif

#ifdef WLFBT
#define WLC_BSSCFG_FL2_FBT_MASK	(WLC_BSSCFG_FL2_FBT_1X | WLC_BSSCFG_FL2_FBT_PSK)
#define BSSCFG_IS_FBT(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_FBT_MASK) != 0)
#define BSSCFG_IS_FBT_1X(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_FBT_1X) != 0)
#define BSSCFG_IS_FBT_PSK(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_FBT_PSK) != 0)
#else
#define BSSCFG_IS_FBT(cfg)	FALSE
#define BSSCFG_IS_FBT_1X(cfg)	FALSE
#define BSSCFG_IS_FBT_PSK(cfg)	FALSE
#endif // endif

#ifdef WMF
#define WMF_ENAB(cfg)	((cfg)->wmf_enable)
#else
#define WMF_ENAB(cfg)	FALSE
#endif // endif

#ifdef DWDS
#define DWDS_ENAB(cfg)	((cfg)->_dwds)
#else
#define DWDS_ENAB(cfg)	FALSE
#endif // endif

#define SPLIT_ASSOC_REQ(cfg)	((cfg)->flags2 & WLC_BSSCFG_FL2_SPLIT_ASSOC_REQ)
#define SPLIT_ASSOC_RESP(cfg)	((cfg)->flags2 & WLC_BSSCFG_FL2_SPLIT_ASSOC_RESP)

#ifdef MULTIAP
#define MAP_ENAB(cfg)	((cfg)->_map)
#else
#define MAP_ENAB(cfg)	FALSE
#endif	/* MULTIAP */

#ifdef MCAST_REGEN
#define MCAST_REGEN_ENAB(cfg)	((cfg)->mcast_regen_enable)
#else
#define MCAST_REGEN_ENAB(cfg)	FALSE
#endif // endif

#define AP_80211_RAW_ENAB(cfg) FALSE
#define AP_80211_RAW_QOS(cfg) FALSE
#define AP_80211_RAW_NOCRYPTO(cfg) FALSE

#ifdef WL_MODESW
#define BSSCFG_IS_MODESW_BWSW(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_MODESW_BWSW) != 0)
#else
#define BSSCFG_IS_MODESW_BWSW(cfg)	FALSE
#endif // endif

#ifdef WL_TBOW
#define BSSCFG_IS_TBOW_ACTIVE(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_TBOW_ACTIVE) != 0)
#define BSSCFG_SET_TBOW_ACTIVE(cfg)	((cfg)->flags2 |= WLC_BSSCFG_FL2_TBOW_ACTIVE)
#define BSSCFG_RESET_TBOW_ACTIVE(cfg)	((cfg)->flags2 &= ~WLC_BSSCFG_FL2_TBOW_ACTIVE)
#endif /* WL_TBOW */

#define BSSCFG_BCMC_SCB(cfg)		((cfg)->bcmc_scb)
#define BSSCFG_HAS_BCMC_SCB(cfg)	(BSSCFG_BCMC_SCB(cfg) != NULL)

#define BSSCFG_IS_CSA_IN_PROGRESS(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_CSA_IN_PROGRESS) != 0)
#define BSSCFG_SET_CSA_IN_PROGRESS(cfg)	((cfg)->flags2 |= WLC_BSSCFG_FL2_CSA_IN_PROGRESS)
#define BSSCFG_CLR_CSA_IN_PROGRESS(cfg)	((cfg)->flags2 &= ~WLC_BSSCFG_FL2_CSA_IN_PROGRESS)

/* TDLS capabilities */
#ifdef WLTDLS
#define BSS_TDLS_UAPSD_BUF_STA		0x1
#define BSS_TDLS_PEER_PSM		0x2
#define BSS_TDLS_CH_SW			0x4
#define BSS_TDLS_SUPPORT		0x8
#define BSS_TDLS_PROHIBIT		0x10
#define BSS_TDLS_CH_SW_PROHIBIT		0x20
#define BSS_TDLS_BUFFER_STA(cfg)	(((cfg)->tdls_cap & BSS_TDLS_UAPSD_BUF_STA) != 0)
#else
#define BSS_TDLS_BUFFER_STA(cfg)	FALSE
#endif // endif

#define IS_BSSCFG_LINKED(cfg)	((cfg)->flags2 & WLC_BSSCFG_FL2_RSDB_LINKED)
#define BSSCFG_IS_NO_KM(cfg)	(((cfg)->flags2 & WLC_BSSCFG_FL2_NO_KM) != 0)

#define MULTIMAC_ENTRY_INVALID -1

extern bsscfg_module_t *wlc_bsscfg_attach(wlc_info_t *wlc);
extern void wlc_bsscfg_detach(bsscfg_module_t *bcmh);

/** bsscfg cubby callback functions */
typedef int (*bsscfg_cubby_init_t)(void *ctx, wlc_bsscfg_t *cfg);
typedef void (*bsscfg_cubby_deinit_t)(void *ctx, wlc_bsscfg_t *cfg);
typedef int (*bsscfg_cubby_config_get_t)(void *ctx, wlc_bsscfg_t *cfg, uint8 *data, int *len);
typedef int (*bsscfg_cubby_config_set_t)(void *ctx, wlc_bsscfg_t *cfg, const uint8 *data, int len);
typedef void (*bsscfg_cubby_dump_t)(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
typedef void (*bsscfg_cubby_datapath_log_dump_t)(void *ctx, wlc_bsscfg_t *cfg, int tag);

typedef struct bsscfg_cubby_params {
	void *context;
	bsscfg_cubby_init_t fn_init;
	bsscfg_cubby_deinit_t fn_deinit;
	bsscfg_cubby_dump_t fn_dump;
#ifdef WL_DATAPATH_LOG_DUMP
	bsscfg_cubby_datapath_log_dump_t fn_data_log_dump;
#endif /* WL_DATAPATH_LOG_DUMP */
#ifdef WLRSDB
	bsscfg_cubby_config_get_t fn_get;
	bsscfg_cubby_config_set_t fn_set;
	uint config_size;
#endif /* WL_RSDB */
} bsscfg_cubby_params_t;

/**
 * This function allocates an opaque cubby of the requested size in the bsscfg container.
 * The callback functions fn_init/fn_deinit are called when a bsscfg is allocated/freed.
 * It returns a handle that can be used in macro BSSCFG_CUBBY to retrieve the cubby.
 * Function returns a negative number on failure
 */
#ifdef BCMDBG

int wlc_bsscfg_cubby_reserve(wlc_info_t *wlc, uint size,
	bsscfg_cubby_init_t fn_init, bsscfg_cubby_deinit_t fn_deinit,
	bsscfg_cubby_dump_t fn_dump, void *ctx,
	const char *func);
int wlc_bsscfg_cubby_reserve_ext(wlc_info_t *wlc, uint size, bsscfg_cubby_params_t *params,
	const char *func);

/* Macro defines to automatically supply the function name parameter */
#define wlc_bsscfg_cubby_reserve(wlc, size, fn_init, fn_deinit, fn_dump, ctx) \
	wlc_bsscfg_cubby_reserve(wlc, size, fn_init, fn_deinit, fn_dump, ctx, __FUNCTION__)
#define wlc_bsscfg_cubby_reserve_ext(wlc, size, params) \
	wlc_bsscfg_cubby_reserve_ext(wlc, size, params, __FUNCTION__)

#else /* BCMDBG */

int wlc_bsscfg_cubby_reserve(wlc_info_t *wlc, uint size,
	bsscfg_cubby_init_t fn_init, bsscfg_cubby_deinit_t fn_deinit,
	bsscfg_cubby_dump_t fn_dump, void *ctx);
int wlc_bsscfg_cubby_reserve_ext(wlc_info_t *wlc, uint size, bsscfg_cubby_params_t *params);

#endif /* BCMDBG */

/* macro to retrieve the pointer to module specific opaque data in bsscfg container */
#define BSSCFG_CUBBY(bsscfg, handle) \
	(void *)((bsscfg) == NULL ? NULL : (uint8 *)(bsscfg) + (handle))
#define CONST_BSSCFG_CUBBY(bsscfg, handle) \
	(const void *)((bsscfg) == NULL ? NULL : (const uint8 *)(bsscfg) + (handle))

/* **** State change event is replacing up/down event **** */

/** bsscfg state change event data. */
typedef struct bsscfg_state_upd_data_t
{
	/* BSSCFG instance data. */
	wlc_bsscfg_t *cfg;

	/* states before change.
	 * new states are in wlc_bsscfg_t.
	 */
	bool old_enable;
	bool old_up;

	/* Each client of the notification can increment this value. It is used
	 * to indicate the number of pending asynchronous callbacks in the driver
	 * down path.
	 */
	int callbacks_pending;
} bsscfg_state_upd_data_t;

/** bsscfg state change event callback function. */
typedef void (*bsscfg_state_upd_fn_t)(void *ctx, bsscfg_state_upd_data_t *evt_data);

/**
 * wlc_bsscfg_state_upd_register()
 *
 * This function registers a callback that will be invoked when a bsscfg
 * changes its state (enable/disable/up/down).
 *
 * Parameters
 *    wlc       Common driver context.
 *    fn        Callback function  to invoke on state change events.
 *    arg       Client data that will provided as param to the callback.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
extern int wlc_bsscfg_state_upd_register(wlc_info_t *wlc, bsscfg_state_upd_fn_t fn, void *arg);

/**
 * wlc_bsscfg_state_upd_unregister()
 *
 * This function unregisters a bsscfg state change event callback.
 *
 * Parameters
 *    wlc       Common driver context.
 *    fn        Callback function that was previously registered.
 *    arg       Client data that was previously registerd.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
extern int wlc_bsscfg_state_upd_unregister(wlc_info_t *wlc, bsscfg_state_upd_fn_t fn, void *arg);

/* **** Deprecated: up/down event is being replaced by state change event **** */

/** bsscfg up/down event data. */
typedef struct bsscfg_up_down_event_data_t
{
	/* BSSCFG instance data. */
	wlc_bsscfg_t	*bsscfg;

	/* TRUE for up event, FALSE for down event. */
	bool		up;

	/* Each client of the notification can increment this value. It is used
	 * to indicate the number of pending asynchronous callbacks in the driver
	 * down path.
	 */
	int callbacks_pending;
} bsscfg_up_down_event_data_t;

/** bsscfg up/down event callback function. */
typedef void (*bsscfg_up_down_fn_t)(void *ctx, bsscfg_up_down_event_data_t *evt_data);

/**
 * wlc_bsscfg_updown_register()
 *
 * This function registers a callback that will be invoked when either a bsscfg
 * up or down event occurs.
 *
 * Parameters
 *    wlc       Common driver context.
 *    callback  Callback function  to invoke on up/down events.
 *    arg       Client specified data that will provided as param to the callback.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
extern int wlc_bsscfg_updown_register(wlc_info_t *wlc, bsscfg_up_down_fn_t fn, void *arg);

/**
 * wlc_bsscfg_updown_unregister()
 *
 * This function unregisters a bsscfg up/down event callback.
 *
 * Parameters
 *    wlc       Common driver context.
 *    callback  Callback function that was previously registered.
 *    arg       Client specified data that was previously registerd.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
extern int wlc_bsscfg_updown_unregister(wlc_info_t *wlc, bsscfg_up_down_fn_t fn, void *arg);

/**
 * wlc_bsscfg_iface_register()
 *
 * This function registers a callback that will be invoked while creating an interface.
 *
 * Parameters
 *    wlc       Common driver context.
 *    if_type	Type of interface to be created.
 *    callback  Invoke respective interface creation callback function.
 *    ctx       Context of the respective callback function.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
typedef wlc_bsscfg_t *(*iface_create_hdlr_fn)(void *module_ctx, wl_interface_create_t *in,
		wl_interface_info_t *out, int *err);
typedef int32 (*iface_remove_hdlr_fn)(wlc_info_t *wlc, wlc_if_t *wlcif);
extern int32 wlc_bsscfg_iface_register(wlc_info_t *wlc, wl_interface_type_t iftype,
		iface_create_hdlr_fn create_cb, iface_remove_hdlr_fn remove_cb, void *ctx);
/**
 * wlc_bsscfg_iface_unregister()
 *
 * This function unregisters a callback that will be invoked while creating an interface.
 *
 * Parameters
 *    wlc       Common driver context.
 *    if_type	Type of interface to be created.
 * Returns:
 *    BCME_OK on success, else BCME_xxx error code.
 */
extern int32 wlc_bsscfg_iface_unregister(wlc_info_t *wlc, wl_interface_type_t iftype);
int32 wlc_if_remove_ap_sta(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

/** bcn/prbresp template update notification */
typedef struct {
	wlc_bsscfg_t *cfg;
	int type;	/**< FC_BEACON, or FC_PROBE_RESP */
} bss_tplt_upd_data_t;
typedef void (*bss_tplt_upd_fn_t)(void *arg, bss_tplt_upd_data_t *notif_data);
extern int wlc_bss_tplt_upd_register(wlc_info_t *wlc, bss_tplt_upd_fn_t fn, void *arg);
extern int wlc_bss_tplt_upd_unregister(wlc_info_t *wlc, bss_tplt_upd_fn_t fn, void *arg);
extern void wlc_bss_tplt_upd_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int type);

/** ibss mute request notification */
typedef struct {
	wlc_bsscfg_t *cfg;
	bool mute;
} ibss_mute_upd_data_t;
typedef void (*ibss_mute_upd_fn_t)(void *arg, ibss_mute_upd_data_t *notif_data);
extern int wlc_ibss_mute_upd_register(wlc_info_t *wlc, ibss_mute_upd_fn_t fn, void *arg);
extern int wlc_ibss_mute_upd_unregister(wlc_info_t *wlc, ibss_mute_upd_fn_t fn, void *arg);
extern void wlc_ibss_mute_upd_notif(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool mute);
#ifdef WL_TRAFFIC_THRESH
void wlc_bsscfg_traffic_thresh_set_defaults(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
int wlc_bsscfg_traffic_thresh_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
void wlc_bsscfg_traffic_thresh_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif // endif

/**
 * query bss specific pretbtt:
 * return in bss_pretbtt_query_data_t something bigger than 'min'
 * if 'min' doesn't meet your need
 */
typedef struct {
	wlc_bsscfg_t *cfg;
	uint pretbtt;
} bss_pretbtt_query_data_t;
typedef void (*bss_pretbtt_query_fn_t)(void *arg, bss_pretbtt_query_data_t *notif_data);
extern int wlc_bss_pretbtt_query_register(wlc_info_t *wlc,
	bss_pretbtt_query_fn_t fn, void *arg);
extern int wlc_bss_pretbtt_query_unregister(wlc_info_t *wlc,
	bss_pretbtt_query_fn_t fn, void *arg);
extern uint wlc_bss_pretbtt_query(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint min);

/* APIs */
typedef struct wlc_bsscfg_type {
	uint8 type;
	uint8 subtype;
} wlc_bsscfg_type_t;

/* XXX Remove the macro WLC_BSSCFG_CREATE_PARAM
 * XXX when all the branches moves to latest interface_create version
 */
#define WLC_BSSCFG_CREATE_PARAM
typedef struct wlc_bsscfg_create_param {
	wlc_bsscfg_type_t		*type;
	wl_interface_create_t		*if_buf;
	uint32				bsscfg_flag;
	int8				bsscfg_idx;
} wlc_bsscfg_create_param_t;

wlc_bsscfg_t*
wlc_bsscfg_alloc_iface_create(wlc_info_t *wlc, wlc_bsscfg_create_param_t *iface_param, int32 *err);

/* NDIS compatibility macro */
#define WLC_BSSCFG_TYPE_T

extern wlc_bsscfg_t *wlc_bsscfg_primary(wlc_info_t *wlc);
extern int wlc_bsscfg_primary_init(wlc_info_t *wlc);
extern wlc_bsscfg_t *wlc_bsscfg_alloc(wlc_info_t *wlc, int idx, wlc_bsscfg_type_t *type,
	uint flags, uint32 flags2, struct ether_addr *ea);
extern int wlc_bsscfg_reset(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_bsscfg_type_t *type,
	uint flags, uint32 flags2, struct ether_addr *ea);
extern int wlc_bsscfg_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern int wlc_bsscfg_reinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_bsscfg_type_t *type,
	uint flags);
extern void wlc_bsscfg_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_bsscfg_free(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_bsscfg_bcmcscbfree(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern int wlc_bsscfg_disable(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_down(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_enable(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_get_free_idx(wlc_info_t *wlc);
extern wlc_bsscfg_t *wlc_bsscfg_find(wlc_info_t *wlc, int idx, int *perr);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_hwaddr(wlc_info_t *wlc, struct ether_addr *hwaddr);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_bssid(wlc_info_t *wlc, const struct ether_addr *bssid);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_hwaddr_bssid(wlc_info_t *wlc,
	const struct ether_addr *hwaddr, const struct ether_addr *bssid);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_target_bssid(wlc_info_t *wlc,
	const struct ether_addr *bssid);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_ssid(wlc_info_t *wlc, uint8 *ssid, int ssid_len);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_wlcif(wlc_info_t *wlc, wlc_if_t *wlcif);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_ID(wlc_info_t *wlc, uint16 id);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_unique_hwaddr(wlc_info_t *wlc,
	const struct ether_addr *hwaddr);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_unique_bssid(wlc_info_t *wlc, struct ether_addr *bssid);
wlc_bsscfg_t *wlc_bsscfg_find_by_unique_target_bssid(wlc_info_t *wlc,
	const struct ether_addr *bssid);
extern int wlc_ap_bss_up_count(wlc_info_t *wlc);
#ifdef STA
extern int wlc_bsscfg_assoc_params_set(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len);
extern int wlc_bsscfg_assoc_params_reset(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#define wlc_bsscfg_assoc_params(bsscfg)	(bsscfg)->assoc_params
extern int wlc_bsscfg_scan_params_set(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wl_join_scan_params_t *scan_params);
extern void wlc_bsscfg_scan_params_reset(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#define wlc_bsscfg_scan_params(bsscfg)	(bsscfg)->scan_params
#endif // endif
extern void wlc_bsscfg_SSID_set(wlc_bsscfg_t *bsscfg, const uint8 *SSID, int len);
extern void wlc_bsscfg_ID_assign(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

extern int wlc_bsscfg_rateset_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	uint8 rates, uint8 bw, uint8 mcsallow);

#ifdef STA
int wlc_bsscfg_type_bi2cfg(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi,
	wlc_bsscfg_type_t *type);
#endif // endif
int wlc_bsscfg_type_fixup(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_bsscfg_type_t *type, bool min_reinit);

#define WLC_BCMCSCB_GET(wlc, bsscfg) (((bsscfg)->flags & WLC_BSSCFG_NOBCMC) ? \
	NULL : ((bsscfg)->bcmc_scb))

#define WLC_BSSCFG_INVALID_IFIDX	-1
#define WLC_BSSCFG_INVALID_IDX		-1
#define WLC_BSSCFG_IDX(bsscfg)		((bsscfg)? (bsscfg)->_idx : WLC_BSSCFG_INVALID_IDX)

extern int wlc_bsscfg_set_ext_cap(wlc_bsscfg_t *bsscfg, uint32 bit, bool val);
extern bool wlc_bsscfg_test_ext_cap(wlc_bsscfg_t *bsscfg, uint32 bit);

#define WLC_BSSCFG_AUTH(cfg) ((cfg)->auth)

/* Extend WME_ENAB to per-BSS */
#define BSS_WME_ENAB(wlc, cfg) \
	(WME_ENAB((wlc)->pub) && !((cfg)->flags & WLC_BSSCFG_WME_DISABLE))
#define BSS_WME_AS(wlc, cfg) \
	(BSS_WME_ENAB(wlc, cfg) && ((cfg)->flags & WLC_BSSCFG_WME_ASSOC) != 0)

/* Extend N_ENAB to per-BSS */
#define BSS_N_ENAB(wlc, cfg) \
	(N_ENAB((wlc)->pub) && !((cfg)->flags & WLC_BSSCFG_11N_DISABLE))

/* Extend VHT_ENAB to per-BSS */
#define BSS_VHT_ENAB(wlc, cfg) \
	(VHT_ENAB_BAND((wlc)->pub, (wlc)->band->bandtype) && \
		 !((cfg)->flags & WLC_BSSCFG_VHT_DISABLE))

/* Extend HE_ENAB to per-BSS. */
#define BSS_HE_ENAB(wlc, cfg) (HE_ENAB((wlc)->pub) && (((cfg)->flags & WLC_BSSCFG_HE_DISABLE) == 0))
/* Extend HE_ENAB to per-BSS & per-band. */
#define BSS_HE_ENAB_BAND(wlc, bandtype, cfg) \
	(HE_ENAB_BAND((wlc)->pub, bandtype) && BSS_HE_ENAB(wlc, cfg))

/* Extend WL11H_ENAB to per-BSS */
#define BSS_WL11H_ENAB(wlc, cfg) \
	(WL11H_ENAB(wlc) && !((cfg)->flags & WLC_BSSCFG_11H_DISABLE))

/* Extend WLBSSLOAD_ENAB to per-BSS */
#define BSS_WLBSSLOAD_ENAB(wlc, cfg) \
	(WLBSSLOAD_ENAB((wlc)->pub) && !((cfg)->flags & WLC_BSSCFG_BSSLOAD_DISABLE))

/* Extend P2P_ENAB to per-BSS */
#ifdef WLP2P
#define BSS_P2P_ENAB(wlc, cfg) \
	(P2P_ENAB((wlc)->pub) && (cfg) && ((cfg)->type == BSSCFG_TYPE_P2P))
#define BSS_P2P_DISC_ENAB(wlc, cfg) \
	(P2P_ENAB((wlc)->pub) && (cfg) && ((cfg)->flags & WLC_BSSCFG_P2P_DISC) != 0)
#else
#define BSS_P2P_ENAB(wlc, cfg) FALSE
#define BSS_P2P_DISC_ENAB(wlc, cfg) FALSE
#endif /* !WLP2P */

/* handy bsscfg type macros */
#ifdef WLP2P
#define P2P_DEV(wlc, cfg)	(BSS_P2P_DISC_ENAB(wlc, cfg))
#define P2P_GO(wlc, cfg)	(BSSCFG_AP(cfg) && BSS_P2P_ENAB(wlc, cfg))
#define P2P_CLIENT(wlc, cfg)	(BSSCFG_STA(cfg) && BSS_P2P_ENAB(wlc, cfg) && \
				 !BSS_P2P_DISC_ENAB(wlc, cfg))
#define P2P_IF(wlc, cfg)	(P2P_GO(wlc, cfg) || P2P_CLIENT(wlc, cfg))
#else
#define P2P_DEV(wlc, cfg)	FALSE
#define P2P_GO(wlc, cfg)	FALSE
#define P2P_CLIENT(wlc, cfg)	FALSE
#define P2P_IF(wlc, cfg)	FALSE
#endif /* !WLP2P */

#ifdef WL_BSSCFG_TX_SUPR
#define BSS_TX_SUPR_ENAB(cfg) ((cfg)->flags & WLC_BSSCFG_TX_SUPR_ENAB)
#define BSS_TX_SUPR(cfg) ((cfg)->flags & WLC_BSSCFG_TX_SUPR)
#else
#define BSS_TX_SUPR_ENAB(cfg)	FALSE
#define BSS_TX_SUPR(cfg)	0
#endif // endif

#ifdef WLTDLS
/* Extend TDLS_ENAB to per-BSS */
#define BSS_TDLS_ENAB(wlc, cfg) \
	(TDLS_ENAB((wlc)->pub) && (cfg) && ((cfg)->type ==  BSSCFG_TYPE_TDLS))
#else
#define BSS_TDLS_ENAB(wlc, cfg) 	FALSE
#endif // endif

#ifdef WL_PROXDETECT
#define BSS_PROXD_ENAB(wlc, cfg) \
	(PROXD_ENAB((wlc)->pub) && (cfg) && ((cfg)->type == BSSCFG_TYPE_PROXD))
#else
#define BSS_PROXD_ENAB(wlc, cfg) 	FALSE
#endif // endif

#ifdef WLAWDL
/* Extend AWDL_ENAB to per-BSS */
#define BSS_AWDL_ENAB(wlc, cfg) \
	(AWDL_ENAB((wlc)->pub) && (cfg) && (cfg)->is_awdl)
#else
#define BSS_AWDL_ENAB(wlc, cfg) 	FALSE
#endif // endif

/* Macros related to Multi-BSS. */
#if defined(MBSS)
/* Define as all bits less than and including the msb shall be one's */
#define MBSS_BCN_ENAB(wlc, cfg)		(MBSS_ENAB((wlc)->pub) &&	\
					 BSSCFG_AP(cfg) &&		\
					 ((cfg)->flags & (WLC_BSSCFG_SW_BCN | \
					                  WLC_BSSCFG_HW_BCN | \
					                  0)))
#define MBSS_PRB_ENAB(wlc, cfg)		(MBSS_ENAB((wlc)->pub) &&	\
					 BSSCFG_AP(cfg) &&		\
					 ((cfg)->flags & (WLC_BSSCFG_SW_PRB | \
					                  WLC_BSSCFG_HW_PRB | \
					                  0)))

/**
 * BC/MC FID macros.  Only valid under MBSS
 *
 *    BCMC_FID_QUEUED	   Are any packets enqueued on the BC/MC fifo?
 */

/* XXX If we can guarantee no race condition reading SHM FID on dotxstatus of last
 * XXX fid, then we could eliminate recording fid_shm in the driver
 */

#define BCMC_PKTS_QUEUED(bsscfg) \
	(((bsscfg)->bcmc_fid_shm != INVALIDFID) || ((bsscfg)->bcmc_fid != INVALIDFID))
#else
#define MBSS_BCN_ENAB(wlc, cfg)  0
#define MBSS_PRB_ENAB(wlc, cfg)  0
#define BCMC_PKTS_QUEUED(cfg)	 0
#endif /* defined(MBSS) */

#define BSS_11H_SRADAR_ENAB(wlc, cfg)		(WL11H_ENAB(wlc) && BSSCFG_SRADAR_ENAB(cfg))
#define BSSCFG_SRADAR_ENAB(cfg)			((cfg)->flags & WLC_BSSCFG_SRADAR_ENAB)
#define BSS_11H_AP_NORADAR_CHAN_ENAB(wlc, cfg) \
	(WL11H_ENAB(wlc) && BSSCFG_AP_NORADAR_CHAN_ENAB(cfg))
#define BSSCFG_AP_NORADAR_CHAN_ENAB(cfg)	((cfg)->flags & WLC_BSSCFG_AP_NORADAR_CHAN)
#define BSSCFG_AP_USR_RADAR_CHAN_SELECT(cfg)    ((cfg)->flags & WLC_BSSCFG_USR_RADAR_CHAN_SELECT)

#ifdef MBO_AP
#define BSS_MBO_ENAB(wlc, bsscfg)	wlc_bsscfg_is_mbo_enabled((wlc), (bsscfg))
extern bool wlc_bsscfg_is_mbo_enabled(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#else
#define BSS_MBO_ENAB(wlc, bsscfg)	0
#endif /* MBO_AP */

#define BLOCK_AS_RESET 1

extern chanspec_t wlc_get_home_chanspec(struct wlc_bsscfg *cfg);
extern wlc_bss_info_t *wlc_get_current_bss(struct wlc_bsscfg *cfg);

extern bool wlc_bsscfg_mcastfilter(wlc_bsscfg_t *cfg, struct ether_addr *da);
extern int wlc_bss_count(wlc_info_t *wlc);
#ifdef WLRSDB
int wlc_bsscfg_configure_from_bsscfg(wlc_bsscfg_t *from_cfg, wlc_bsscfg_t *to_cfg);
#endif // endif

#ifdef BCMDBG_TXSTUCK
extern void wlc_bsscfg_print_txstuck(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG_TXSTUCK */

/* additional IEs passed part of notif data */
typedef struct {
	ht_cap_ie_t *cap_ie;
	void *vht_cap_ie_p;
	bcm_tlv_t *sup_rates;
	bcm_tlv_t *ext_rates;
} bcn_notif_data_ext_t;

/* on-channel beacon reception notification */
typedef struct {
	wlc_bsscfg_t  *cfg;
	wlc_d11rxhdr_t *wrxh;
	uint8 *plcp;
	struct dot11_management_header *hdr;
	uint8 *body;
	int bcn_len;
	struct scb *scb;
	bcn_notif_data_ext_t	*data_ext;
} bss_rx_bcn_notif_data_t;
typedef void (*bss_rx_bcn_notif_fn_t)(void *arg, bss_rx_bcn_notif_data_t *data);
int wlc_bss_rx_bcn_register(wlc_info_t *wlc, bss_rx_bcn_notif_fn_t fn, void *arg);
int wlc_bss_rx_bcn_unregister(wlc_info_t *wlc, bss_rx_bcn_notif_fn_t fn, void *arg);
void wlc_bss_rx_bcn_signal(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct scb *scb,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_management_header *hdr,
	uint8 *body, int bcn_len, bcn_notif_data_ext_t *data_ext);

extern bool wlc_bss_get_mimo_cap(wlc_bss_info_t *bi);
extern bool wlc_bss_get_80p80_cap(wlc_bss_info_t *bi);
extern void wlc_bsscfg_type_get(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	wlc_bsscfg_type_t *type);
void wlc_bsscfg_datapath_log_dump(wlc_bsscfg_t *bsscfg, int tag);

wlc_info_t *wlc_bsscfg_get_wlc_from_wlcif(wlc_info_t *wlc, wlc_if_t *wlcif);
struct ether_addr *wlc_bsscfg_get_ether_addr(wlc_info_t *wlc, wlc_if_t *wlcif);

extern bool wlc_bsscfg_is_associated(wlc_bsscfg_t* bsscfg);

extern bool wlc_bsscfg_mfp_supported(wlc_bsscfg_t *bsscfg);
wlc_bsscfg_t* wlc_iface_create_generic_bsscfg(wlc_info_t *wlc, wl_interface_create_t *if_buf,
	wlc_bsscfg_type_t *type, int32 *err);

extern struct wlc_if* wlcif_get_by_ifindex(wlc_info_t *wlc, int if_idx);

int
wlc_bsscfg_upd_multmac_tbl(wlc_bsscfg_t *cfg, struct ether_addr *ea,
	struct ether_addr *bssid);
int
wlc_bsscfg_delete_multmac_entry(wlc_bsscfg_t *cfg, struct ether_addr *ea);
int
wlc_bsscfg_get_multmac_entry_idx(wlc_bsscfg_t *cfg, const struct ether_addr *ea);
struct ether_addr *
wlc_bsscfg_multmac_etheraddr(wlc_bsscfg_t *cfg, int idx);
struct ether_addr *
wlc_bsscfg_multmac_bssid(wlc_bsscfg_t *cfg, int idx);
int wlc_set_iface_info(wlc_bsscfg_t *cfg, wl_interface_info_t *wl_info,
		wl_interface_create_t *if_buf);
int wlc_bsscfg_bcmc_scb_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
void wlc_bsscfg_notif_signal(wlc_bsscfg_t *cfg, bsscfg_state_upd_data_t *st_data);
#ifdef WL_GLOBAL_RCLASS
extern void wlc_bsscfg_update_rclass(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg);
extern bool wlc_cur_opclass_global(wlc_info_t* wlc, wlc_bsscfg_t* bsscfg);
#endif /* WL_GLOBAL_RCLASS */
bool wlc_is_ap_interface_up(wlc_info_t *wlc);
#endif	/* _WLC_BSSCFG_H_ */
