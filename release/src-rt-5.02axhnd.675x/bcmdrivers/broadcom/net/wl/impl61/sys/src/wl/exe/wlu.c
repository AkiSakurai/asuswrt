/*
 * Common code for wl command-line swiss-army-knife utility
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
 * $Id: wlu.c 779909 2019-10-09 21:49:41Z $
 */

#include <typedefs.h>
#include <awd.h>
#include <wlioctl.h>
#include <wlioctl_utils.h>

#if !defined(TARGETOS_nucleus)
#define CLMDOWNLOAD
#endif // endif

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <typedefs.h>
#include <epivers.h>
#include <ethernet.h>
#include <802.11.h>
#include <802.1d.h>
#include <802.11e.h>
#include <802.11ax.h>
#include <wpa.h>
#include <bcmip.h>
#include <wps.h>

#include <bcmwifi_rates.h>
#include <bcmwifi_rspec.h>
#include <rte_ioctl.h>

#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmwifi_channels.h>
#include <bcmsrom_fmt.h>
#include <bcmsrom_tbl.h>
#include "wlu_common.h"
#include "wlu.h"
#include <bcmcdc.h>
#if defined(linux)
#ifndef TARGETENV_android
#include <unistd.h>
#endif // endif
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <time.h>
#include <sched.h>
#define TIME_STR_SZ 100 /*  string buffer size for timestamp formatting */
#endif /* linux */
#if defined(WLBSSLOAD_REPORT) && defined(linux)
#include <sys/time.h>
#endif   /* defined(WLBSSLOAD_REPORT) && defined(linux) */

#include <inttypes.h>
#include <miniopt.h>
#include <errno.h>

#if defined SERDOWNLOAD || defined CLMDOWNLOAD
#include <sys/stat.h>
#include <trxhdr.h>
#ifdef SERDOWNLOAD
#include <usbrdl.h>
#endif // endif
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#endif /* SERDOWNLOAD || defined CLMDOWNLOAD */

#define CCA_MESH_MAX 8

#if LCNCONF || SSLPNCONF
#define MAX_CHUNK_LEN 1456  /* 8 * 7 * 26 */
#else
#define MAX_CHUNK_LEN 1408 /* 8 * 8 * 22 */
#endif // endif

#include <bcm_mpool_pub.h>
#include <bcmipv6.h>

#define EVENT_LOG_DUMPER
#include <event_log_tag.h>

#include <sdiovar.h>

#include "wlu_subcounters.h"
#include "wluc_otp.h"
#define CCA_MESH_MAX 8
#define WL_OP_NONE	(0)
#define WL_OP_D2H	(1)
#define WL_OP_H2D	(2)

/* For backwards compatibility, the absense of the define 'NO_FILESYSTEM_SUPPORT'
 * implies that a filesystem is supported.
 */
#if !defined(BWL_NO_FILESYSTEM_SUPPORT)
#define BWL_FILESYSTEM_SUPPORT
#endif // endif

const char blob_magic_string[] = {'B', 'L', 'O', 'B'};

cmd_func_t wl_int, wl_buf;

const ofdm_rates_t ofdm_rates[] = { /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
					0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c };

/* ifdef protection since wlc_event_names[] only used under ifdef linux */
#ifdef linux

#define NAME_ENTRY(x) {x, #x}

static const wlu_name_entry_t wlc_event_names[] = {
	NAME_ENTRY(WLC_E_SET_SSID),
	NAME_ENTRY(WLC_E_JOIN),
	NAME_ENTRY(WLC_E_START),
	NAME_ENTRY(WLC_E_AUTH),
	NAME_ENTRY(WLC_E_AUTH_IND),
	NAME_ENTRY(WLC_E_DEAUTH),
	NAME_ENTRY(WLC_E_DEAUTH_IND),
	NAME_ENTRY(WLC_E_ASSOC),
	NAME_ENTRY(WLC_E_ASSOC_IND),
	NAME_ENTRY(WLC_E_REASSOC),
	NAME_ENTRY(WLC_E_REASSOC_IND),
	NAME_ENTRY(WLC_E_DISASSOC),
	NAME_ENTRY(WLC_E_DISASSOC_IND),
	NAME_ENTRY(WLC_E_QUIET_START),
	NAME_ENTRY(WLC_E_QUIET_END),
	NAME_ENTRY(WLC_E_BEACON_RX),
	NAME_ENTRY(WLC_E_LINK),
	NAME_ENTRY(WLC_E_MIC_ERROR),
	NAME_ENTRY(WLC_E_ROAM),
	NAME_ENTRY(WLC_E_TXFAIL),
	NAME_ENTRY(WLC_E_PMKID_CACHE),
	NAME_ENTRY(WLC_E_RETROGRADE_TSF),
	NAME_ENTRY(WLC_E_PRUNE),
	NAME_ENTRY(WLC_E_AUTOAUTH),
	NAME_ENTRY(WLC_E_EAPOL_MSG),
	NAME_ENTRY(WLC_E_SCAN_COMPLETE),
	NAME_ENTRY(WLC_E_ADDTS_IND),
	NAME_ENTRY(WLC_E_DELTS_IND),
	NAME_ENTRY(WLC_E_BCNSENT_IND),
	NAME_ENTRY(WLC_E_BCNRX_MSG),
	NAME_ENTRY(WLC_E_BCNLOST_MSG),
	NAME_ENTRY(WLC_E_ROAM_PREP),
	NAME_ENTRY(WLC_E_PFN_NET_FOUND),
	NAME_ENTRY(WLC_E_PFN_NET_LOST),
	NAME_ENTRY(WLC_E_RESET_COMPLETE),
	NAME_ENTRY(WLC_E_JOIN_START),
	NAME_ENTRY(WLC_E_ROAM_START),
	NAME_ENTRY(WLC_E_ASSOC_START),
	NAME_ENTRY(WLC_E_IBSS_ASSOC),
	NAME_ENTRY(WLC_E_RADIO),
	NAME_ENTRY(WLC_E_PSM_WATCHDOG),
	NAME_ENTRY(WLC_E_PROBREQ_MSG),
	NAME_ENTRY(WLC_E_SCAN_CONFIRM_IND),
	NAME_ENTRY(WLC_E_PSK_SUP),
	NAME_ENTRY(WLC_E_COUNTRY_CODE_CHANGED),
	NAME_ENTRY(WLC_E_EXCEEDED_MEDIUM_TIME),
	NAME_ENTRY(WLC_E_ICV_ERROR),
	NAME_ENTRY(WLC_E_UNICAST_DECODE_ERROR),
	NAME_ENTRY(WLC_E_MULTICAST_DECODE_ERROR),
	NAME_ENTRY(WLC_E_TRACE),
	NAME_ENTRY(WLC_E_IF),
	NAME_ENTRY(WLC_E_P2P_DISC_LISTEN_COMPLETE),
	NAME_ENTRY(WLC_E_RSSI),
	NAME_ENTRY(WLC_E_PFN_SCAN_COMPLETE),
	NAME_ENTRY(WLC_E_ACTION_FRAME),
	NAME_ENTRY(WLC_E_ACTION_FRAME_COMPLETE),
	NAME_ENTRY(WLC_E_PRE_ASSOC_IND),
	NAME_ENTRY(WLC_E_PRE_REASSOC_IND),
	NAME_ENTRY(WLC_E_AP_STARTED),
	NAME_ENTRY(WLC_E_DFS_AP_STOP),
	NAME_ENTRY(WLC_E_DFS_AP_RESUME),
	NAME_ENTRY(WLC_E_WAI_STA_EVENT),
	NAME_ENTRY(WLC_E_WAI_MSG),
	NAME_ENTRY(WLC_E_ESCAN_RESULT),
	NAME_ENTRY(WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE),
	NAME_ENTRY(WLC_E_PROBRESP_MSG),
	NAME_ENTRY(WLC_E_P2P_PROBREQ_MSG),
	NAME_ENTRY(WLC_E_DCS_REQUEST),
	NAME_ENTRY(WLC_E_FIFO_CREDIT_MAP),
	NAME_ENTRY(WLC_E_ACTION_FRAME_RX),
	NAME_ENTRY(WLC_E_WAKE_EVENT),
	NAME_ENTRY(WLC_E_RM_COMPLETE),
	NAME_ENTRY(WLC_E_HTSFSYNC),
	NAME_ENTRY(WLC_E_OVERLAY_REQ),
	NAME_ENTRY(WLC_E_CSA_COMPLETE_IND),
	NAME_ENTRY(WLC_E_EXCESS_PM_WAKE_EVENT),
	NAME_ENTRY(WLC_E_PFN_SCAN_NONE),
	NAME_ENTRY(WLC_E_PFN_SCAN_ALLGONE),
	NAME_ENTRY(WLC_E_GTK_PLUMBED),
	NAME_ENTRY(WLC_E_ASSOC_REQ_IE),
	NAME_ENTRY(WLC_E_ASSOC_RESP_IE),
	{ 0, NULL}
};
#endif /* linux */

static cmd_func_t wlu_dump, wlu_dump_clr, wlu_mempool, wlu_srdump, wlu_srvar;
static cmd_func_t wl_rate, wl_rate_mrate, wl_bss_max;
static cmd_func_t wl_channel, wl_chanspec, wl_rclass, wl_dfs_ap_move, wl_sc_chan;
static cmd_func_t wl_phy_vcore;
static cmd_func_t wl_dfs_max_safe_tx;
static cmd_func_t wl_radio, wl_version, wl_list, wl_band, wl_bandlist, wl_phylist;
static cmd_func_t wl_cfp_dump;
static cmd_func_t wl_amsdu_tid;
static cmd_func_t wlc_traffic_thresh;
static cmd_func_t wl_join, wl_txpwr, wl_txpwr_cap, wl_country;
static cmd_func_t wl_out, wl_txpwr1, wl_country_ie_override, wl_echo;
static cmd_func_t wl_radar_status, wl_clear_radar_status;
static cmd_func_t wl_radar_sc_status;
static cmd_func_t wl_radar_subband_status;
static cmd_func_t wl_get_pktcnt, wl_upgrade;
static cmd_func_t wl_default_rateset;
static cmd_func_t wl_rateset, wl_txbf_rateset;
static cmd_func_t wl_dfs_status;
static cmd_func_t wl_dfs_status_all;
static cmd_func_t wl_get_txpwr_limit;
static cmd_func_t wl_get_txpwr_target_max, wl_get_chanspec_txpwr_max;
static cmd_func_t wl_get_txpwr_adj_est;
static cmd_func_t wl_chan_info;
static cmd_func_t wl_wme_ac_req, wl_add_ie, wl_del_ie, _wl_list_ie;
static cmd_func_t wl_wme_apsd_sta, wl_wme_dp, wl_lifetime;
static cmd_func_t wl_rand;
static cmd_func_t wl_awd_data_info, wl_wme_counters;
static cmd_func_t wl_eventbitvec, wl_bitvecext;
static cmd_func_t wl_auto_channel_sel;
static cmd_func_t wl_msglevel, wl_plcphdr, wl_macreg, wl_band_elm;
static cmd_func_t wl_rateparam, wl_status, wl_spect;
static cmd_func_t wl_sup_rateset, wl_scan, wl_send_csa, wl_iscan, wl_escan;
static cmd_func_t wl_roamparms, wl_roam_prof;
#ifdef WLRCC
static cmd_func_t wl_roamchannels;
#endif /* WLRCC */
static cmd_func_t wl_dump_chanlist, wl_measure_req, wl_send_quiet;
static cmd_func_t wl_pm_mute_tx;
static cmd_func_t wl_dump_chanspecs, wl_dump_chanspecs_defset;
static cmd_func_t wl_wsec;
static cmd_func_t wl_channels_in_country;
static cmd_func_t wl_wpa_auth, wl_deauth_rc, wl_bssid, wl_smfstats;
static cmd_func_t wl_set_pmk;
static cmd_func_t wl_rm_request, wl_rm_report;
static cmd_func_t wl_join_pref, wl_assoc_pref;
static cmd_func_t wl_dump_networks, wl_revinfo, wl_iov_pktqlog_params;
static cmd_func_t wl_varstr;
static cmd_func_t wl_roam_cache;

#if defined(linux)
static cmd_func_t wl_escan_event_check;
static cmd_func_t wl_escanresults;
static cmd_func_t wl_event_check;
#endif   /* linux */

static cmd_func_t wl_reassoc;

static cmd_func_t wl_overlay;
static cmd_func_t wl_pmkid_info;

static void wl_rate_histo_print(wl_mac_ratehisto_res_t *rate_histo_res);
static cmd_func_t wl_rate_histo;
static cmd_func_t wl_mac_rate_histo;
static cmd_func_t wl_rate_histo_report;
static cmd_func_t wme_tx_params;
static cmd_func_t wme_maxbw_params;

static cmd_func_t wl_actframe;
static cmd_func_t wl_antsel;
static cmd_func_t wl_txfifo_sz;

static cmd_func_t wl_mcast_ar;

static cmd_func_t wl_pwrstats;
static cmd_func_t wl_memuse;

static cmd_func_t wl_hmaptest;
static cmd_func_t wl_hmap;

int wl_seq_batch_in_client(bool enable);

static cmd_func_t wl_antgain;
static cmd_func_t wl_srchmem;
static cmd_func_t wl_ptk_start;

#ifdef CLMDOWNLOAD
static cmd_func_t wl_clmload;
static cmd_func_t wl_txcapload;
static cmd_func_t wl_txcapctl;
static cmd_func_t wl_txcapdump;
#endif /* CLMDOWNLOAD */
static cmd_func_t wl_calload;
static cmd_func_t wl_caldump;

#ifdef RWL_WIFI
/* Function added to support RWL_WIFI Transport */
static cmd_func_t wl_wifiserver;
#endif // endif

static cmd_func_t wl_cca_get_stats;
static cmd_func_t wl_cca_stats_mesh;
static cmd_func_t wl_txdelay_params;
static cmd_func_t wl_intfer_params;
static cmd_func_t wl_rpmt;
static cmd_func_t wl_sarlimit;
static cmd_func_t wl_ie;
static cmd_func_t wl_ccode_info;

#ifdef SERDOWNLOAD
static cmd_func_t dhd_upload;
int debug = 0;
#endif // endif

static cmd_func_t wl_staprio;
static cmd_func_t wl_stamon_sta_config;
static cmd_func_t wl_monitor_promisc_level;
static cmd_func_t wl_monitor_config;
static cmd_func_t wl_bcnlenhist;

#ifdef SR_DEBUG
static cmd_func_t wl_dump_pmu;
#endif /* SR_DEBUG */

static cmd_func_t wl_bss_peer_info;
static cmd_func_t wl_setiproute;
static cmd_func_t wl_desired_bssid;
static cmd_func_t wl_interface_remove_action;
static cmd_func_t wl_macdbg_pmac;
static cmd_func_t wl_svmp_mem;
static cmd_func_t wl_mu_rate;
static cmd_func_t wl_mu_group;
static cmd_func_t wl_ded_setup;
static cmd_func_t wl_mu_policy;
static cmd_func_t wl_muinfo;
static cmd_func_t wl_muclients;
#if defined(BCMDBG)
static cmd_func_t wl_svmp_sampcol;
#endif // endif
static cmd_func_t wl_macregx;
static cmd_func_t wl_scanmac;
cmd_func_t wl_hostip;
cmd_func_t wl_hostipv6_extended;
static cmd_func_t wl_hc;
static cmd_func_t wl_wake_timer;
static cmd_func_t wl_disassoc;

#if defined(BCMDBG) || defined(BCMDBG_MU)
static cmd_func_t wl_mutx_ac;
static cmd_func_t wl_macreq_cmd;
#endif // endif

#ifdef BCMDBG
static cmd_func_t wl_wrr_handler;
#endif // endif

static cmd_func_t wl_idauth;
static int wl_idauth_config_set(void *wl, uint16 category, char **argv);
static int wl_idauth_config_get(void *wl, uint16 category, char **argv);
static int wl_idauth_dump_counters(void *wl, uint16 category, char **argv);
static int wl_idauth_dump_peer_info(void *wl, uint16 category, char **argv);

static int wlu_dump_phytbls(void *wl, char *dump_buf);
static int wl_parse_rateset(void *wl, wl_rateset_args_u_t* rs, int rslen, int rsver, char **argv);
static void dump_networks(char *buf);
static void wl_dump_wpa_rsn_ies(uint8* cp, uint len);
static void wl_rsn_ie_dump(bcm_tlv_t *ie);
static void bcm_print_vs_ie(uint8 *parse, int len);
static const char *capmode2str(uint16 capability);
static bool wlu_is_wpa_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len);
static void wl_dump_wps(uint8* cp, uint len);
static uint8 *wlu_parse_tlvs(uint8 *tlv_buf, int buflen, uint key);
static void bcm_wps_version(uint8 *wps_ie);
static void bcm_is_wps_configured(uint8 *wps_ie);
static bool bcm_is_wps_ie(uint8 *ie, uint8 **tlvs, uint32 *tlvs_len);
static bcm_tlv_t *bcm_find_vs_ie(uint8 *parse, int len, uint8 *oui, uint8 oui_len, uint8 oui_type);
static bool bcm_vs_ie_match(uint8 *ie, uint8 *oui, int oui_len, uint8 type);
static void wl_dump_ext_cap(uint8* cp, uint len);
static void wl_ext_cap_ie_dump(bcm_tlv_t* ext_cap_ie);
static void free_cca_array(cca_congest_channel_req_t **favg, int favg_chan_elts);
static int wl_print_dfs_status(wl_dfs_status_t *dfs_status);
static int wl_print_dfs_sub_status(wl_dfs_sub_status_t *sub);
static int wl_print_dfs_status_all(wl_dfs_status_all_t *dfs_status_all);
static int wl_parse_txbf_rateset(wl_txbf_rateset_t *rs, char **argv);
static void wl_print_txbf_mcsset(char *mcsset, char *prefix);
static void wl_print_txbf_vhtmcsset(uint16 *mcsset, char *prefix);

static cmd_func_t wl_power_sel_params;
#if defined(BCMDBG)
static cmd_func_t wl_dump_modesw_dyn_bwsw;
#endif // endif

int wlu_get(void *wl, int cmd, void *buf, int len);
int wlu_set(void *wl, int cmd, void *buf, int len);

static cmd_func_t wl_modesw_timecal;
static cmd_func_t wl_atm_staperc;
static cmd_func_t wl_hwareg;

typedef struct wl_config_iovar_s wl_config_iovar_t;
typedef struct nv_s nv_t;

/* 802.11i/WPA RSN IE parsing utilities */
typedef struct {
	uint16 version;
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *akm;
	uint8 *capabilities;
} rsn_parse_info_t;

static int wl_rsn_ie_parse_info(uint8* buf, uint len, rsn_parse_info_t *rsn);
static uint wl_rsn_ie_decode_cntrs(uint cntr_field);
typedef void (wl_config_print_func_t)(wl_config_iovar_t *config_iovar,
	wl_config_t *config);
static void wl_bcm_config_print(wl_config_iovar_t *cfg_iovar, wl_config_t *cfg);
static int wl_parse_assoc_params(char **argv, wl_assoc_params_t *params, bool *prescanned);
static int wl_join_prescanned(void *wl, wl_join_params_t *join_params, uint *join_params_size);
#define wl_parse_reassoc_params(argv, params) wl_parse_assoc_params(argv, \
						(wl_assoc_params_t *)(params), NULL)

static uint16 wl_qdbm_to_mw(int8 qdbm);
static int8 wl_mw_to_qdbm(uint16 mw);

static void wl_printrate(int val);
static int wl_get_iscan(void *wl, char *buf, uint buf_len);
int wlu_var_setbuf(void *wl, const char *iovar, void *param, int param_len);
int wlu_iovar_get(void *wl, const char *iovar, void *outbuf, int len);
int wlu_iovar_set(void *wl, const char *iovar, void *param, int paramlen);
int wlu_iovar_getint(void *wl, const char *iovar, int *pval);
int wlu_iovar_setint(void *wl, const char *iovar, int val);
static int wl_vndr_ie(void *wl, const char *command, uint32 pktflag_ok, char **argv);
static void wl_join_pref_print_ie(bcm_tlv_t *ie);
static void wl_join_pref_print_akm(uint8* suite);
static void wl_join_pref_print_cipher_suite(uint8* suite);

static int wl_assertlog(void *wl, cmd_t *cmd, char **argv);
static int wl_tsf(void *wl, cmd_t *cmd, char **argv);

static cmd_func_t wl_taf_def;
static cmd_func_t wl_scb_bs_data;
static cmd_func_t wl_scb_rx_report;

static int wl_dfs_channel_forced(void *wl, cmd_t *cmd, char **argv);
static int wl_event_log_set_init(void *wl, cmd_t *cmd, char **argv);
static int wl_event_log_set_expand(void *wl, cmd_t *cmd, char **argv);
static int wl_event_log_set_shrink(void *wl, cmd_t *cmd, char **argv);
static int wl_event_log_tag_control(void *wl, cmd_t *cmd, char **argv);
static int wl_event_log_get(void *wl, cmd_t *cmd, char **argv);
static int wl_sleep_ret_ext(void *wl, cmd_t *cmd, char **argv);

static cmd_func_t wl_pcie_bus_throughput_params;

static cmd_func_t wl_phy_txpwrcap_tbl;
static cmd_func_t wl_sim_pm;
static cmd_func_t wl_utrace_capture;
static cmd_func_t wl_eventaggr;

#ifdef ATE_BUILD
static cmd_func_t wl_gpaio;
#endif // endif

static cmd_func_t wl_musched_cmd;
static cmd_func_t wl_umusched_cmd;
static cmd_func_t wl_txbfcfg_cmd;

static cmd_func_t wl_srcfg_cmd;

static cmd_func_t wl_nd_ra_limit_intv;

#if defined(BCMDBG)
static cmd_func_t wl_cca_chan_util_interval;
static cmd_func_t wl_cca_chan_util_dur;
static cmd_func_t wl_cca_chan_util;
static cmd_func_t wl_cca_rx_util;
static cmd_func_t wl_cca_tx_util;
static cmd_func_t wl_cca_raw_stats;
static cmd_func_t wl_cca_delta_stats;
#endif // endif

static void dlystat_dump(txdelay_stats_t *txdelay_stats);
static int wl_dlystats(void *wl, cmd_t *cmd, char **argv);
static int wl_dlystats_clear(void *wl, cmd_t *cmd, char **argv);

static int wl_get_tcmstbl_entry(void *wl, cmd_t *cmd, char **argv);

char *ver2str(unsigned int vms, unsigned int vls);

static int wl_rmc_report_version_get(void *wl, uint16 category, char **argv);
static int wl_rmc_report_data_get(void *wl, uint16 category, char **argv);

#define NUM_CHANSPECS_LIST_SIZE	110 /* chanspecs list size passed to driver */

/* constants to be used to parse dynamic ed setup
 * for more info see wlc_phy_int.h
 */
#define DYN_ED_MAX_SED 100
#define DYN_ED_MIN_SED 0
#define DYN_ED_MAX_TH -20
#define DYN_ED_MIN_TH -75

/* IOCtl version read from targeted driver */
static int ioctl_version;

/* 64 bits aligned allocation */
static union {
	char bufdata[WLC_IOCTL_MAXLEN];
	uint64 alignme;
} bufstruct_wlu;
static char *buf = (char*) &bufstruct_wlu.bufdata;

/* integer output format, default to signed integer */
static uint8 int_fmt;

/*
 * Country names and abbreviations from ISO 3166
 */
typedef struct {
	const char *name;	/* Long name */
	const char *abbrev;	/* Abbreviation */
} cntry_name_t;
cntry_name_t cntry_names[];	/* At end of this file */

struct nv_s {
	char *name;
	uint32 value;
};

struct wl_config_iovar_s {
	char *iovar_name;
	wl_config_print_func_t *pfunc;
	nv_t params[BCM_CONFIG_ARRAY_SIZE];
};

typedef struct {
	uint value;
	const char *string;
} monitor_promisc_level_msg_t;

#define WL_SCAN_PARAMS_SSID_MAX 10

#define RATE_2G_USAGE							\
"\tEither \"auto\", or a simple CCK/DSSS/OFDM rate value:\n"		\
"\t1 2 5.5 11 6 9 12 18 24 36 48 54\n\n"				\
"\tOr options to specify legacy, HT, or VHT rate:\n"			\
"\t-r R, --rate=R        : legacy rate (CCK, DSSS, OFDM)\n"		\
"\t-h M, --ht=M          : HT MCS index [0-23]\n"			\
"\t-v M[xS], --vht=M[xS] : VHT MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. 5x2 is MCS=5, Nss=2\n" \
"\t-c cM[sS]             : VHT (c notation) MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. c5s2 is MCS=5, Nss=2\n" \
"\t-e M[xS], --he=M[xS]  : HE rate M [0-11],\n" \
"\t-s S, --ss=S          : VHT Nss [1-8], number of spatial streams, default 1.\n" \
"\t                      : Only used with -v/--vht when MxS format is not used\n" \
"\t-x T, --exp=T         : Tx Expansion, number of tx chains (NTx) beyond the minimum\n" \
"\t                      : required for the space-time-streams, exp = NTx - Nsts\n" \
"\t--stbc                : Use STBC expansion, otherwise no STBC\n"	\
"\t-l, --ldpc            : Use LDPC encoding, otherwise no LDPC\n"	\
"\t-g, --sgi             : Guard interval. Different values for HT/VHT\n" \
"\t                      : Use Short Guard Interval otherwise standard GI\n" \
"\t-i, --hegi            : Guard interval. Different values for HE\n" \
"\t                      : For HE, cp_ltf combination allowed values (0,1,2,3)\n" \
"\t-b, --bandwidth       : transmit bandwidth MHz; 2.5, 5, 10, 20, 40, 80, 160"

#define RATE_5G_USAGE							\
"\tEither \"auto\", or a simple OFDM rate value:\n"			\
"\t6 9 12 18 24 36 48 54\n\n"						\
"\tOr options to specify legacy OFDM, HT, or VHT rate:\n"		\
"\t-r R, --rate=R        : legacy OFDM rate\n"				\
"\t-h M, --ht=M          : HT MCS index [0-23]\n"			\
"\t-v M[xS], --vht=M[xS] : VHT MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. 5x2 is MCS=5, Nss=2\n" \
"\t-c cM[sS]             : VHT (c notation) MCS index M [0-9],\n"			\
"\t                      : and optionally Nss S [1-8], eg. c5s2 is MCS=5, Nss=2\n" \
"\t-e M[xS], --he=M[xS]  : HE rate M [0-11],\n"		\
"\t-s S, --ss=S          : VHT Nss [1-8], number of spatial streams, default 1.\n" \
"\t                      : Only used with -v/--vht when MxS format is not used\n" \
"\t-x T, --exp=T         : Tx Expansion, number of tx chains (NTx) beyond the minimum\n" \
"\t                      : required for the space-time-streams, exp = NTx - Nsts\n" \
"\t--stbc                : Use STBC expansion, otherwise no STBC\n"	\
"\t-l, --ldpc            : Use LDPC encoding, otherwise no LDPC\n"	\
"\t-g, --sgi             : Guard interval. Different values for HT/VHT\n" \
"\t                      : Use Short Guard Interval otherwise standard GI\n" \
"\t-i, --hegi            : Guard interval. Different values for HE\n" \
"\t                      : For HE cp_ltf combination allowed values (0,1,2,3)\n" \
"\t-b, --bandwidth       : transmit bandwidth MHz; 2.5, 5, 10, 20, 40, 80, 160"

#define TXBF_RATESET_USAGE						\
"Get rateset consisting of OFDM, HT and VHT rates, and Broadcom-to-Broadcom\n" \
"\tgroup of OFDM, HT and VHT rates by issuing command with no arguments.\n" \
"\tOFDM rates printed are in Mbps, and each Basic rate in OFDM list is marked\n" \
"\tby (b) behind it.  Example: full list of OFDM rates:\n" \
"\t\t6(b) 9 12(b) 18 24(b) 36 48 54\n" \
"\twhere 6, 12 and 24 are Basic rates.\n" \
"\n" \
"\tSet synopsis:\n" \
"\t\twl txbf_rateset < [ofdm_rate_list] [options ...] >\n" \
"\tOFDM rate specification does not need to mark Basic rates because Basic\n" \
"\trates are automatically recognized.\n" \
"\tOptions are processed in order; thus redundant instances of an option will\n" \
"\tresult in only the last instance taking effect for that option.\n" \
"\tOptions:\n" \
"\t-m <MCS_bitmask> ...\n" \
"\t\tSet HT rates by bitmask bytes, each ranges from 00 through ff, where\n" \
"\t\tthe least significant bit is MCS0.\n" \
"\t\tExample: '-m 3f 01' specifies HT rates MCS0 - MCS5 and MCS8.\n" \
"\n" \
"\t-v <VHT_bitmask> ...\n" \
"\t\tSet VHT rates for each supported count of spatial streams.\n" \
"\t\tExample: '-v 3ff 1ff ff' specifies VHT rates: MCS0 - MCS9 for 1 stream,\n" \
"\t\tMCS0 - MCS8 for 2 streams, and MCS0 - MCS7 for 3 streams.\n" \
"\n" \
"\t-b\n" \
"\t\tSet for Broadcom-to-Broadcom group of rates.  Otherwise without\n" \
"\t\tthe -b option, the standard group of rates are set accordingly.\n"

#define MONITOR_PROMISC_LEVEL_USAGE	\
"\tUsage: wl monitor_promisc_level [<bitmap> | <+|-name>]\n" \
"\tbitmap values and corresponding name are the following:\n" \
"\tArgs:\n" \
"\t\tbit:0:promisc: " \
"When set, address filter accepts all received frames." \
"When cleared, the address filter accepts only those frames " \
"that match the BSSID or local MAC address\n" \
"\t\tbit:1:ctrl: " \
"When set, the RX filter accepts all received control frames " \
"that are accepted by the address filter. " \
"When cleared, the RX filter rejects all control frames other " \
"than PS poll frames." \
"\t\tbit:3:fcs: " \
"When set, the RX filter forwards received frames with FCS " \
"errors to the driver." \
"When cleared, frames with FCS errors are discarded.\n\n" \
"\tExample: wl monitor_promisc_level +promisc\n" \
"\tExample: wl monitor_promisc_level 0x2\n" \
"\tExample: wl monitor_promisc_level 0"

#define MONITOR_CONFIG_USAGE							\
"\tUsage: wl monitor_config [-h] [-aid aids] [-vht_aid aids] [-he_aid aids]\n" \
"\t       [-gid gid] [-bc bsscolor] [-bssbmp bss_bitmap]\n" \
"\tArgs:\n" \
"\t-aid <val>     : set all aids to be <val> for VHT and HE\n"  \
"\t-aid <val1,val2,val3,val4> : set aids for both VHT and HE\n" \
"\t-vht_aid <val> : set all aids to <val> for VHT\n" \
"\t-vht_aid <val1,val2,val3,val4> : set aids for VHT\n"   \
"\t-he_aid <val>  : set all aids to <val> for HE\n"   \
"\t-gid <val>     : set groupid for VHT\n" \
"\t-bc <val>      : set bsscolor. If not given, disable bss color filtering\n"  \
"\t-bssbmp <val>  : (internal) specify he bss bitmap. If not given, default to 3\n" \
"\t-hetb <addr>   : Enable HETB PPDU sniffing\n" \
"\t-h             : print this help message\n" \
"\tExample: wl monitor_config -he_aid 1,2,3,4\n" \

/* the default behavior is batching in driver,
 * to indicate client batching, users should specify --interactive and --clientbatch
 */
static bool batch_in_client;

/* The wl_config_iovar_list structure is used to define config iovars. Config iovars can be in
 * either an auto mode or in an override mode. If it is in auto mode, the status of the iovar
 * is determined automatically. In override mode, the status is passed as a parameter to the
 * iovar. If a new config iovar is getting added, it can either reuse the last entry in the
 * list if parameters match, or add a new entry. If you are adding a new entry, make sure it is
 * added before the last entry.
 * In each row, the last entry of name-value params must have NULL
 */
wl_config_iovar_t wl_config_iovar_list[] = {
	{ "rsdb_mode", wl_bcm_config_print, {{"auto", -1}, {"mimo", 0}, {"rsdb", 1}, {"80p80", 2},
	{NULL, 0}}},
	/* XXX: If you are adding a new config iovar entry to this array, keep it above this line
	 */
	{ NULL, wl_bcm_config_print, {{"auto", -1}, {"off", 0}, {"disable", 0}, {"on", 1},
	{"enable", 1}, {NULL, 0}}},
};

/* If the new command needs to be part of 'wc.exe' tool used for WMM,
 * be sure to modify wc_cmds[] array as well
 *
 * If you add a command, please update wlu_cmd.c cmd2cat to categorize the command.
 */
cmd_t wl_cmds[] = {
	{ "debug_crash", wlu_reg2args, WLC_GET_VAR, WLC_SET_VAR,
	"debug crash - TYPE DELAYED"},
	{ "ver", wl_version, -1, -1,
	"get version information" },
	{ "cmds", wl_list, -1, -1,
	"generate a short list of available commands"},
	{ "ioctl_echo",	wl_echo, -1, WLC_ECHO,
	"check ioctl functionality" },
	{ "up",	wl_void, -1, WLC_UP,
	"reinitialize and mark adapter up (operational)" },
	{ "down", wl_void, -1, WLC_DOWN,
	"reset and mark adapter down (disabled)" },
	{ "out", wl_out, -1, WLC_OUT,
	"mark adapter down but do not reset hardware(disabled)\n"
	"\tOn dualband cards, cards must be bandlocked before use."},
	{ "clk", wl_int, WLC_GET_CLK, WLC_SET_CLK,
	"set board clock state. return error for set_clk attempt if the driver is not down\n"
	"\t0: clock off\n"
	"\t1: clock on" },
	{ "reboot", wl_void, -1, WLC_REBOOT,
	"Reboot platform"},
	{ "radio", wl_radio, WLC_GET_RADIO, WLC_SET_RADIO,
	"Set the radio on or off.\n"
	"\t\"on\" or \"off\"" },
	{ "dump", wlu_dump, WLC_GET_VAR, -1,
	"Give suboption \"list\" to list various suboptions" },
	{ "dump_clear", wlu_dump_clr, -1, WLC_SET_VAR,
	"Clear a specific category of counters/stats.\n"
	"\tUsage: dump_clear <category-name>\n"},
	{ "srclear", wlu_srwrite, -1, WLC_SET_SROM,
	"Clears first 'len' bytes of the srom, len in decimal or hex\n"
	"\tUsage: srclear <len>" },
	{ "srdump", wlu_srdump, WLC_GET_SROM, -1,
	"print contents of SPROM to stdout" },
	{ "srwrite", wlu_srwrite, -1, WLC_SET_SROM,
	"Write the srom: srwrite byteoffset value" },
	{ "cisconvert", wlu_srvar, -1, -1,
	"Print CIS tuple for given name=value pair" },
	{ "rdvar", wlu_srvar, WLC_GET_SROM, -1,
	"Read a named variable to the srom" },
	{ "wrvar", wlu_srvar, WLC_GET_SROM, WLC_SET_SROM,
	"Write a named variable to the srom" },
	{ "nvram_dump", wl_nvdump, WLC_NVRAM_DUMP, -1,
	"print nvram variables to stdout" },
	{ "nvset", wl_nvset, -1, WLC_NVRAM_SET,
	"set an nvram variable\n"
	"\tname=value (no spaces around \'=\')" },
	{ "nvget", wl_nvget, WLC_NVRAM_GET, -1,
	"get the value of an nvram variable" },
	{ "nvram_get", wl_nvget, WLC_NVRAM_GET, -1,
	"get the value of an nvram variable" },
	{ "revinfo", wl_revinfo, WLC_GET_REVINFO, -1,
	"get hardware revision information" },
	{ "msglevel", wl_msglevel, WLC_GET_VAR, WLC_SET_VAR,
	"set driver console debugging message bitvector\n"
	"\ttype \'wl msglevel ?\' for values" },
	{ "PM", wl_int, WLC_GET_PM, WLC_SET_PM,
	"set driver power management mode:\n"
	"\t0: CAM (constantly awake)\n"
	"\t1: PS  (power-save)\n"
	"\t2: FAST PS mode" },
	{ "wake", wl_int, WLC_GET_WAKE, WLC_SET_WAKE,
	"set driver power-save mode sleep state:\n"
	"\t0: core-managed\n"
	"\t1: awake" },
	{ "promisc", wl_int, WLC_GET_PROMISC, WLC_SET_PROMISC,
	"set promiscuous mode ethernet address reception\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "monitor", wl_int, WLC_GET_MONITOR, WLC_SET_MONITOR,
	"set monitor mode\n"
	"\t0 - disable\n"
	"\t1 - enable active monitor mode (interface still operates)" },
	{ "frag", wl_print_deprecate, -1, -1, "Deprecated. Use fragthresh." },
	{ "rts", wl_print_deprecate, -1, -1, "Deprecated. Use rtsthresh." },
	{ "cwmin", wl_int, WLC_GET_CWMIN, WLC_SET_CWMIN,
	"Set the cwmin.  (integer [1, 255])" },
	{ "cwmax", wl_int, WLC_GET_CWMAX, WLC_SET_CWMAX,
	"Set the cwmax.  (integer [256, 2047])" },
	{ "srl", wl_int, WLC_GET_SRL, WLC_SET_SRL,
	"Set the short retry limit.  (integer [1, 255])" },
	{ "lrl", wl_int, WLC_GET_LRL, WLC_SET_LRL,
	"Set the long retry limit.  (integer [1, 255])" },
	{ "rate", wl_rate_mrate, WLC_GET_RATE, -1,
	"force a fixed rate:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "mrate", wl_rate_mrate, -1, -1, /* Deprecated. Use "bg_mrate" or "a_mrate" */
	"force a fixed multicast rate:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "a_rate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed rate for the A PHY:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "a_mrate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed multicast rate for the A PHY:\n"
	"\tvalid values for 802.11a are (6, 9, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "bg_rate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed rate for the B/G PHY:\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "bg_mrate", wl_phy_rate, WLC_GET_VAR, WLC_SET_VAR,
	"force a fixed multicast rate for the B/G PHY:\n"
	"\tvalid values for 802.11b are (1, 2, 5.5, 11)\n"
	"\tvalid values for 802.11g are (1, 2, 5.5, 6, 9, 11, 12, 18, 24, 36, 48, 54)\n"
	"\t-1 (default) means automatically determine the best rate" },
	{ "2g_rate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for data frames in the 2.4G band:\n\n"
	RATE_2G_USAGE
	},
	{ "2g_mrate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for mulitcast/broadcast data frames in the 2.4G band:\n\n"
	RATE_2G_USAGE
	},
	{ "5g_rate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for data frames in the 5G band:\n\n"
	RATE_5G_USAGE
	},
	{ "5g_mrate", wl_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force a fixed rate for mulitcast/broadcast data frames in the 5G band:\n\n"
	RATE_5G_USAGE
	},
	{ "infra", wl_int, WLC_GET_INFRA, WLC_SET_INFRA,
	"Configure BSS type or query Infra mode.\n"
	"Configure BSS type for next BSS Start or Join operation:\n"
	"\t0 (IBSS)\n"
	"\t1 (Infra BSS)\n"
	"\t2 (Any BSS, for Join only)\n"
	"\t3 (Mesh BSS)\n"
	"Query the Infra mode of the current BSS:\n"
	"\t0 (IBSS)\n"
	"\t1 (Infra BSS)\n"
	"Note: use \"wl infra_configuration\" to query the configuration.\n"},
	{ "bssid", wl_bssid, WLC_GET_BSSID, -1,
	"Get the BSSID value, error if STA and not associated"},
	{ "bssmax", wl_bss_max, WLC_GET_VAR, -1,
	"get number of BSSes " },
	{ "channel", wl_channel, WLC_GET_CHANNEL, WLC_SET_CHANNEL,
	"Set the channel:\n"
	"\tvalid channels for 802.11b/g (2.4GHz band) are 1 through 14\n"
	"\tvalid channels for 802.11a  (5 GHz band) are:\n"
	"\t\t36, 40, 44, 48, 52, 56, 60, 64,\n"
	"\t\t100, 104, 108, 112, 116,120, 124, 128, 132, 136, 140, 144,\n"
	"\t\t149, 153, 157, 161,\n"
	"\t\t184, 188, 192, 196, 200, 204, 208, 212, 216"},
	{ "clmver", wl_var_getandprintstr, WLC_GET_VAR, -1,
	"Get version information for CLM data and tools"},
	{ "clm_data_ver", wl_var_getandprintstr, WLC_GET_VAR, -1,
	"get CLM data version information" },
	{ "roam_channels_in_cache", wl_dump_chanspecs, WLC_GET_VAR, -1,
	"Get a list of channels in roam cache" },
	{ "roam_channels_in_hotlist", wl_dump_chanspecs, WLC_GET_VAR, -1,
	"Get a list of channels in roam hot channel list" },
	{ "roam_cache", wl_roam_cache, WLC_GET_VAR, -1,
	"Usage: wl -i eth1 roam_cache <subcmd> \n"
	"\tsubcmd types:\n"
	"\t ver - roam_cache version\n"
	"\t data - roam cache data\n"
	},
	{ "chanspecs", wl_dump_chanspecs, WLC_GET_VAR, -1,
	"Get all the valid chanspecs (default: all within current locale):\n"
	"\t-b band (5(a) or 2(b/g))\n"
	"\t-w bandwidth, 20, 40, 80, 160, or 80+80\n"
	"\t[-c country_abbrev]"
	},
	{ "chanspecs_defset", wl_dump_chanspecs_defset, WLC_GET_VAR, -1,
	"Get default chanspecs for current driver settings (default: all within current locale)"
	},
	{ "chanspec", wl_chanspec, WLC_GET_VAR, WLC_SET_VAR,
	"Set current or configured channel:\n"
	"\t20MHz : [2g|5g]<channel>[/20]\n"
	"\t40MHz : [2g|5g]<channel>/40[u,l]\n"
	"\t80MHz :    [5g]<channel>/80\n"
	"\t160MHz:    [5g]<channel>/160\n"
	"\t80+80MHz:  [5g]<channel>/80+80/<1st80channel>-<2nd80channel>\n"
	"\toptional band 2g or 5g, default to 2g if channel <= 14\n"
	"\tchannel number (0-200)\n"
	"\tbandwidth, 2.5, 5, 10, 20, 40, 80, 160, or 80+80 default 20\n"
	"\tprimary sideband for 40MHz on 2g, l=lower, u=upper\n"
	"\t<1st80Channel>, <2nd80Channel> Required for 80+80, otherwise not allowed. "
	"These fields specify the center channel of the first and second 80MHz band.\n"
	"OR Set channel with legacy format:\n"
	"\t-c channel number (0-224)\n"
	"\t-b band (5(a) or 2(b/g))\n"
	"\t-w bandwidth 20 or 40\n"
	"\t-s ctl sideband, -1=lower, 0=none, 1=upper"},
	{ "sc_chan", wl_sc_chan, WLC_GET_VAR, WLC_SET_VAR,
	"Set current or configured channel:\n"
	"\t20MHz : [2g|5g]<channel>[/20]\n"
	"\t40MHz : [2g|5g]<channel>/40[u,l]\n"
	"\t80MHz :    [5g]<channel>/80\n"
	"\toptional band 2g or 5g, default to 2g if channel <= 14\n"
	"\tchannel number (0-200)\n"
	"\tbandwidth, 2.5, 5, 10, 20, 40, or 80, default 20\n"
	"\tprimary sideband for 40MHz on 2g, l=lower, u=upper\n"
	"OR Set channel with legacy format:\n"
	"\t-c channel number (0-224)\n"
	"\t-b band (5(a) or 2(b/g))\n"
	"\t-w bandwidth 20 or 40\n"
	"\t-s ctl sideband, -1=lower, 0=none, 1=upper"},
	{ "phy_vcore", wl_phy_vcore, WLC_GET_VAR, -1,
	"get virtual core related capabilities\n"},
	{ "rclass", wl_rclass, WLC_GET_VAR, -1,
	"Get operation class:\n"
	"\t chanspec \n"},
	{ "dfs_channel_forced", wl_dfs_channel_forced, WLC_GET_VAR, WLC_SET_VAR,
	"Set <channel>[a,b][n][u,l]\n"
	"\tchannel number (0-224)\n"
	"\tband a=5G, b=2G, default to 2G if channel <= 14\n"
	"\tbandwidth, n=10, non for 20 & 40\n"
	"\tctl sideband, l=lower, u=upper\n"
	"Set channel list using -l option \n"
	"\twl dfs_channel_forced {-l <chanspec list> | 0}\n"
	"\t20MHz : <channel>[/20]\n"
	"\t40MHz : <channel>{{l|u}|/40}\n"
	"\t80MHz : <channel>/80\n"
	"\tChannels specified using '-l' option should be\n"
	"seperated by ','/' ' and should be prefixed with '+'/'-'\n"
	"Deletes existing configuration when '0' specified"},
	{ "txpwr", wl_txpwr, -1, -1, /* Deprecated. Use "txpwr1" */
	"Set tx power in milliwatts.  Range [1, 84]." },
	{ "txpwr1", wl_txpwr1, WLC_GET_VAR, WLC_SET_VAR,
	"Set tx power in in various units. Choose one of (default: dBm): \n"
	"\t-d dBm units (range: -32 - 31)\n"
	"\t-q quarter dBm units (range: -128 - 127)\n"
	"\t-m milliwatt units\n"
	"Can be combined with:\n"
	"\t-o turn on override to disable regulatory and other limitations\n"
	"Use wl txpwr -1 to restore defaults"},
	{ "txpwrlimit", wl_get_txpwr_limit, WLC_CURRENT_PWR, -1,
	"Return current tx power limit" },
	{ "txpwr_cap", wl_txpwr_cap, WLC_GET_VAR, WLC_SET_VAR,
	"Set per-core tx power cap in qdBm\n"
	"Use wl txpwr_cap 0 to disable the limitation"},
	{ "ucflags", wl_reg, WLC_GET_UCFLAGS, WLC_SET_UCFLAGS,
	"Get/Set ucode flags 1, 2, 3(16 bits each)\n"
	"\toffset [ value ] [ band ]" },
	{ "shmem", wl_reg, WLC_GET_SHMEM, WLC_SET_SHMEM,
	"Get/Set a shared memory location:\n"
	"\toffset [ value ] [band ]" },
	{ "macreg", wl_macreg, WLC_R_REG, WLC_W_REG,
	"Get/Set any mac registers(include IHR and SB):\n"
	"\tmacreg offset size[2,4] [ value ] [ band ]" },
	{ "shmemx", wl_macregx, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set a shared memory location of PSMX:\n"
	"\toffset [ value ] [band ]" },
	{ "macregx", wl_macregx, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set any mac registers(include IHR and SB) of PSMX:\n"
	"\tmacreg offset size[2,4] [ value ] [ band ]" },
	{ "shmem1", wl_macregx, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set a shared memory location of PSMR1:\n"
	"\toffset [ value ] [band ]" },
	{ "macreg1", wl_macregx, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set any mac registers(include IHR and SB) of PSMR1:\n"
	"\tmacreg1 offset size[2,4] [ value ] [ band ]" },
	{ "ucantdiv", wl_int, WLC_GET_UCANTDIV, WLC_SET_UCANTDIV,
	"Enable/disable ucode antenna diversity (1/0 or on/off)" },
	{ "actframe", wl_actframe, -1, WLC_SET_VAR,
	"Send a Vendor specific Action frame to a channel\n"
	"\tusage: wl actframe <Dest Mac Addr> <data> channel dwell-time <BSSID>" },
	{ "antdiv", wl_int, WLC_GET_ANTDIV, WLC_SET_ANTDIV,
	"Set antenna diversity for rx\n"
	"\t0 - force use of antenna 0\n"
	"\t1 - force use of antenna 1\n"
	"\t3 - automatic selection of antenna diversity" },
	{ "txant", wl_int, WLC_GET_TXANT, WLC_SET_TXANT,
	"Set the transmit antenna\n"
	"\t0 - force use of antenna 0\n"
	"\t1 - force use of antenna 1\n"
	"\t3 - use the RX antenna selection that was in force during\n"
	"\t    the most recently received good PLCP header" },
	{ "plcphdr", wl_plcphdr, WLC_GET_PLCPHDR, WLC_SET_PLCPHDR,
	"Set the plcp header.\n"
	"\t\"long\" or \"auto\" or \"debug\"" },
	{ "phytype", wl_int, WLC_GET_PHYTYPE, -1,
	"Get phy type" },
	{ "rateparam", wl_rateparam, -1, WLC_SET_RATE_PARAMS,
	"set driver rate selection tunables\n"
	"\targ 1: tunable id\n"
	"\targ 2: tunable value" },
	{ "wsec_restrict", wl_bsscfg_int, WLC_GET_WEP_RESTRICT, WLC_SET_WEP_RESTRICT,
	"Drop unencrypted packets if WSEC is enabled\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "eap", wl_int, WLC_GET_EAP_RESTRICT, WLC_SET_EAP_RESTRICT,
	"restrict traffic to 802.1X packets until 802.1X authorization succeeds\n"
	"\t0 - disable\n"
	"\t1 - enable" },
	{ "cur_etheraddr", wlu_cur_etheraddr, -1, -1,
	"Get/set the current hw address" },
	{ "perm_etheraddr", wl_iov_mac, -1, -1,
	"Get the permanent address from NVRAM" },
	{ "authorize", wl_macaddr, -1, WLC_SCB_AUTHORIZE,
	"restrict traffic to 802.1X packets until 802.1X authorization succeeds" },
	{ "deauthorize", wl_macaddr, -1, WLC_SCB_DEAUTHORIZE,
	"do not restrict traffic to 802.1X packets until 802.1X authorization succeeds" },
	{ "deauthenticate", wl_deauth_rc, -1, WLC_SCB_DEAUTHENTICATE_FOR_REASON,
	"deauthenticate a STA from the AP with optional reason code (AP ONLY)" },
	{ "wsec", wl_wsec, WLC_GET_WSEC, WLC_SET_WSEC,
	"wireless security bit vector\n"
	"\t1 - WEP enabled\n"
	"\t2 - TKIP enabled\n"
	"\t4 - AES enabled\n"
	"\t8 - WSEC in software\n"
	"\t0x80 - FIPS enabled\n"
	"\t0x100 - WAPI enabled"
	},
	{ "auth", wl_bsscfg_int, WLC_GET_AUTH, WLC_SET_AUTH,
	"set/get 802.11 authentication type. 0 = OpenSystem, 1= SharedKey, 3=Open/Shared" },
	{ "wpa_auth", wl_wpa_auth, WLC_GET_WPA_AUTH, WLC_SET_WPA_AUTH,
	"Bitvector of WPA authorization modes:\n"
	"\t1	WPA-NONE\n"
	"\t2	WPA-802.1X/WPA-Professional\n"
	"\t4	WPA-PSK/WPA-Personal\n"
	"\t64	WPA2-802.1X/WPA2-Professional\n"
	"\t128	WPA2-PSK/WPA2-Personal\n"
	"\t0	disable WPA"
	},
	{ "wpa_cap", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get 802.11i RSN capabilities" },
	{ "set_pmk", wl_set_pmk, -1, WLC_SET_WSEC_PMK,
	"Set passphrase for PMK in driver-resident supplicant." },
	{ "scan", wl_scan, -1, WLC_SCAN,
	"Initiate a scan.\n" SCAN_USAGE
	},
	{ "roamscan_parms", wl_roamparms, WLC_GET_VAR, WLC_SET_VAR,
	"set/get roam scan parameters\n"
	"Use standard scan params syntax below,"
	"but only active/passive/home times, nprobes, type,"
	"and channels are used.\n"
	"All other values are silently discarded.\n"
	SCAN_USAGE
	},
#ifdef WLRCC
	{ "roamscan_channels", wl_roamchannels, WLC_GET_VAR, -1,
	"get roam channels\n"
	},
#endif /* WLRCC */
	{ "roam_prof", wl_roam_prof, WLC_GET_VAR, WLC_SET_VAR,
	"get/set roaming profiles (need to specify band)\n"
	"\tUsage: wl roam_prof_2g a|b|2g|5g flags rssi_upper rssi_lower delta, boost_thresh "
	"boot_delta nfscan fullperiod initperiod backoff maxperiod\n"
	},
	{ "iscan_s", wl_iscan, -1, WLC_SET_VAR,
	"Initiate an incremental scan.\n" SCAN_USAGE
	},
	{ "iscan_c", wl_iscan, -1, WLC_SET_VAR,
	"Continue an incremental scan.\n" SCAN_USAGE
	},
	{ "escan", wl_escan, -1, WLC_SET_VAR,
	"Start an escan.\n" SCAN_USAGE
	},
	{ "escanabort", wl_escan, -1, WLC_SET_VAR,
	"Abort an escan.\n" SCAN_USAGE
	},
	{ "regulatory", wl_int, WLC_GET_REGULATORY, WLC_SET_REGULATORY,
	"Get/Set regulatory domain mode (802.11d). Driver must be down." },
	{ "spect", wl_spect, WLC_GET_SPECT_MANAGMENT, WLC_SET_SPECT_MANAGMENT,
	"Get/Set 802.11h Spectrum Management mode.\n"
	"\t0 - Off\n"
	"\t1 - Loose interpretation of 11h spec - may join non-11h APs\n"
	"\t2 - Strict interpretation of 11h spec - may not join non-11h APs\n"
	"\t3 - Disable 11h and enable 11d\n"
	"\t4 - Loose interpretation of 11h+d spec - may join non-11h APs"
	},
	{ "scanabort", wl_var_void, -1, WLC_SET_VAR,
	"Abort a scan." },
	{ "scanresults", wl_dump_networks, WLC_SCAN_RESULTS, -1,
	"Return results from last scan." },
	{ "iscanresults", wl_dump_networks, WLC_GET_VAR, -1,
	"Return results from last iscan. Specify a buflen (max 8188)\n"
	"\tto artificially limit the size of the results buffer.\n"
	"\tiscanresults [buflen]"},
	{ "assoc",  wl_status, -1, -1,
	"Print information about current network association.\n"
	"\t(also known as \"status\")" },
	{ "status", wl_status, -1, -1,
	"Print information about current network association.\n"
	"\t(also known as \"assoc\")" },
	{ "disassoc", wl_disassoc, -1, WLC_DISASSOC,
	"Disassociate from the current BSS/IBSS." },
	{ "chanlist", wl_print_deprecate, WLC_GET_VALID_CHANNELS, -1,
	"Deprecated. Use channels." },
	{ "channels", wl_dump_chanlist, WLC_GET_VALID_CHANNELS, -1,
	"Return valid channels for the current settings." },
	{ "channels_in_country", wl_channels_in_country, WLC_GET_CHANNELS_IN_COUNTRY, -1,
	"Return valid channels for the country specified.\n"
	"\tArg 1 is the country abbreviation\n"
	"\tArg 2 is the band(a or b)"},
	{ "txpwr_adj_est", wl_get_txpwr_adj_est, WLC_GET_VAR, -1,
	"Return current adjusted est power settings.\n" },
	{ "txpwr_target_max", wl_get_txpwr_target_max, WLC_GET_VAR, -1,
	"Return current max tx target power settings.\n" },
	{ "chanspec_txpwr_max", wl_get_chanspec_txpwr_max, WLC_GET_VAR, -1,
	"Return valid chanspecs with max tx power settings.\n"
	"\t-b band (5(a) or 2(b/g))\n"
	"\t-w bandwidth, 20, 40, 80, 160 or 8080\n"
	},
	{ "cur_rateset", wl_rateset, WLC_GET_VAR, -1,
	"Returns the rateset currently in use, (b) indicates basic rate.\n"
	"It is the BSS rateset if the bsscfg is of AP or IBSS; \n"
	"it is the link rateset if the bsscfg is of STA; "
	"it is the default rateset otherwise.\n"
	},
	{ "rateset", wl_rateset, WLC_GET_RATESET, WLC_SET_RATESET,
	"Returns or sets the supported and basic rateset, (b) indicates basic\n"
	"\tWith no args, returns the rateset. Args are\n"
	"\trateset \"default\" | \"all\" | <arbitrary rateset> [-m|-v|-e <list of mcs masks>]\n"
	"\t\tdefault - driver defaults\n"
	"\t\tall - all rates are basic rates\n"
	"\t\tarbitrary rateset - list of rates\n"
	"\tList of rates are in Mbps and each rate is optionally followed\n"
	"\tby \"(b)\" or \"b\" for a Basic rate. Example: 1(b) 2b 5.5 11\n"
	"\tAt least one rate must be Basic for a legal rateset.\n\n"
	"\t-m  sets HT rates (bitmasks, 00-ff). Least significant bit is MCS0.\n"
	"\t    example: 'rateset -m 0x3f 0x01' limits rates to MCS0-MCS5 and MCS8\n\n"
	"\t-v  sets VHT MCS values for each supported count of spatial streams.\n"
	"\t    example: 'rateset -v 3ff 1ff ff fff' limits vht rates to MCS 0-9 for 1 stream,\n"
	"\t      MCS 0-8 for 2 streams, MCS 0-7 for 3 streams, and MCS 0-11 for 4 streams.\n\n"
	"\t-e  sets HE MCS values for each supported count of spatial streams.\n"
	"\t    example: 'rateset -e 3ff 3ff ff fff' limits HE rates to MCS 0-9 for 1 stream,\n"
	"\t      MCS 0-9 for 2 streams, MCS 0-7 for 3 streams, and MCS 0-11 for 4 streams."
	},
	{"txbf_rateset", wl_txbf_rateset, WLC_GET_TXBF_RATESET, WLC_SET_TXBF_RATESET,
	TXBF_RATESET_USAGE
	},
	{ "default_rateset", wl_default_rateset, WLC_GET_VAR, -1,
	"Returns supported rateset of given phy.\n"
	"\tYou have to insert following Args\n"
	"\t\tArg 1. Phy Type: have to be one of the following: "
	"[a, b, g, n, lp, ssn, ht, lcn, lcn40, ac]\n"
	"\t\tArg 2. Band Type: 2 for 2.4G or 5 for 5G\n"
	"\t\tArg 3. CCK Only: 1 for CCK Only or 0 for CCK and OFDM rates\n"
	"\t\tArg 4. Basic Rates: 1 for all rates WITH basic rates or "
	"0 for all rates WITHOUT basic rates\n"
	"\t\tArg 5. MCS Rates: 1 for all rates WITH MCS rates or "
	"0 for all rates WITHOUT MCS rates\n"
	"\t\tArg 6. Bandwidth: have to be one of the following: [10, 20, 40, 80, 160]\n"
	"\t\tArg 7. TX/RX Stream: \"tx\" for TX streams or \"rx\" for RX streams\n"
	"\t\tExample: PHY: AC, Band 2.4G, CCK rates only, With Basec rates, "
	"WithOut MCS rates, BW: 40 and TX streams\n"
	"\t\tInput: default_rateset ac 2 0 1 0 40 tx\n"
	},
	{ "roam_trigger", wl_band_elm, WLC_GET_ROAM_TRIGGER, WLC_SET_ROAM_TRIGGER,
	"Get or Set the roam trigger RSSI threshold:\n"
	"\tGet: roam_trigger [a|b]\n"
	"\tSet: roam_trigger <integer> [a|b|all]\n"
	"\tinteger -   0: default\n"
	"\t            1: optimize bandwidth\n"
	"\t            2: optimize distance\n"
	"\t    [-1, -99]: dBm trigger value"},
	{ "roam_delta",	wl_band_elm, WLC_GET_ROAM_DELTA, WLC_SET_ROAM_DELTA,
	"Set the roam candidate qualification delta. roam_delta [integer [, a/b]]" },
	{ "roam_scan_period", wl_int, WLC_GET_ROAM_SCAN_PERIOD, WLC_SET_ROAM_SCAN_PERIOD,
	"Set the roam candidate qualification delta.  (integer)" },
	{ "suprates", wl_sup_rateset, WLC_GET_SUP_RATESET_OVERRIDE, WLC_SET_SUP_RATESET_OVERRIDE,
	"Returns or sets the 11g override for the supported rateset\n"
	"\tWith no args, returns the rateset. Args are a list of rates,\n"
	"\tor 0 or -1 to specify an empty rateset to clear the override.\n"
	"\tList of rates are in Mbps, example: 1 2 5.5 11"},
	{ "prb_resp_timeout", wl_int, WLC_GET_PRB_RESP_TIMEOUT, WLC_SET_PRB_RESP_TIMEOUT,
	"Get/Set probe response timeout"},
	{ "channel_qa", wl_int, WLC_GET_CHANNEL_QA, -1,
	"Get last channel quality measurment"},
	{ "channel_qa_start", wl_void, -1, WLC_START_CHANNEL_QA,
	"Start a channel quality measurment"},
	{ "ccode_info", wl_ccode_info, WLC_GET_VAR, -1,
	"Get Country Code Info"},
	{ "country", wl_country, WLC_GET_COUNTRY, WLC_SET_COUNTRY,
	"Select Country Code for driver operational region\n"
	"\tFor simple country setting: wl country <country>\n"
	"\tWhere <country> is either a long name or country code from ISO 3166; "
	"for example \"Germany\" or \"DE\"\n"
	"\n\tFor a specific built-in country definition: "
	"wl country <built-in> [<advertised-country>]\n"
	"\tWhere <built-in> is a country country code followed by '/' and "
	"regulatory revision number.\n"
	"\tFor example, \"US/3\".\n"
	"\tAnd where <advertised-country> is either a long name or country code from ISO 3166.\n"
	"\tIf <advertised-country> is omitted, it will be the same as the built-in country code.\n"
	"\n\tUse 'wl country list [band(a or b)]' for the list of supported countries"},
	{ "country_ie_override", wl_country_ie_override, WLC_GET_VAR, WLC_SET_VAR,
	"To set/get country ie"},
	{ "autocountry_default", wl_varstr, WLC_GET_VAR, WLC_SET_VAR,
	"Select Country Code for use with Auto Contry Discovery"},
	{ "join", wl_join, -1, -1,
	"Join a specified network SSID.\n"
	"\tUsage: join <ssid> [key <0-3>:xxxxx] [imode bss|ibss] "
	"[amode open|shared|openshared|wpa|wpapsk|wpa2|wpa2psk|wpanone|ftpsk] [options]\n"
	"\tOptions:\n"
	"\t-b MAC, --bssid=MAC \tBSSID (xx:xx:xx:xx:xx:xx) to scan and join\n"
	"\t-c CL, --chanspecs=CL \tchanspecs (comma or space separated list)\n"
	"\tprescanned \tuses channel and bssid list from scanresults\n"
	"\t-p, -passive: force passive assoc scan (useful for P2P)"},
	{ "ssid", wl_ssid, WLC_GET_SSID, WLC_SET_SSID,
	"Set or get a configuration's SSID.\n"
	"\twl ssid [-C num]|[--cfg=num] [<ssid>]\n"
	"\tIf the configuration index 'num' is not given, configuraion #0 is assumed and\n"
	"\tsetting will initiate an assoication attempt if in infrastructure mode,\n"
	"\tor join/creation of an IBSS if in IBSS mode,\n"
	"\tor creation of a BSS if in AP mode."},
	{ "mac", wl_maclist, WLC_GET_MACLIST, WLC_SET_MACLIST,
	"Set or get the list of source MAC address matches.\n"
	"\twl mac xx:xx:xx:xx:xx:xx [xx:xx:xx:xx:xx:xx ...]\n"
	"\tTo Clear the list: wl mac none" },
	{ "macmode", wl_int, WLC_GET_MACMODE, WLC_SET_MACMODE,
	"Set the mode of the MAC list.\n"
	"\t0 - Disable MAC address matching.\n"
	"\t1 - Deny association to stations on the MAC list.\n"
	"\t2 - Allow association to stations on the MAC list."},
	{ "band", wl_band, WLC_GET_BAND, WLC_SET_BAND,
	"Returns or sets the current band\n"
	"\tauto - auto switch between available bands (default)\n"
	"\ta - force use of 802.11a band\n"
	"\tb - force use of 802.11b band" },
	{ "bands", wl_bandlist, WLC_GET_BANDLIST, -1,
	"Return the list of available 802.11 bands" },
	{ "phylist", wl_phylist, WLC_GET_PHYLIST, -1,
	"Return the list of available phytypes" },
	{ "shortslot", wl_int, WLC_GET_SHORTSLOT, -1,
	"Get current 11g Short Slot Timing mode. (0=long, 1=short)" },
	{ "shortslot_override", wl_int, WLC_GET_SHORTSLOT_OVERRIDE, WLC_SET_SHORTSLOT_OVERRIDE,
	"Get/Set 11g Short Slot Timing mode override. (-1=auto, 0=long, 1=short)" },
	{ "pktcnt", wl_get_pktcnt, WLC_GET_PKTCNTS, -1,
	"Get the summary of good and bad packets." },
	{ "upgrade", wl_upgrade, -1, WLC_UPGRADE,
	"Upgrade the firmware on an embedded device" },
	{ "gmode", wl_gmode, WLC_GET_GMODE, WLC_SET_GMODE,
	"Set the 54g Mode (LegacyB|Auto||GOnly|BDeferred|Performance|LRS)" },
	{ "gmode_protection", wl_int, WLC_GET_GMODE_PROTECTION, -1,
	"Get G protection mode. (0=disabled, 1=enabled)" },
	{ "gmode_protection_control", wl_int, WLC_GET_PROTECTION_CONTROL,
	WLC_SET_PROTECTION_CONTROL,
	"Get/Set 11g protection mode control alg."
	"(0=always off, 1=monitor local association, 2=monitor overlapping BSS)" },
	{ "gmode_protection_override", wl_int, WLC_GET_GMODE_PROTECTION_OVERRIDE,
	WLC_SET_GMODE_PROTECTION_OVERRIDE,
	"Get/Set 11g protection mode override. (-1=auto, 0=disable, 1=enable)" },
	{ "protection_control", wl_int, WLC_GET_PROTECTION_CONTROL,
	WLC_SET_PROTECTION_CONTROL,
	"Get/Set protection mode control alg."
	"(0=always off, 1=monitor local association, 2=monitor overlapping BSS)" },
	{ "legacy_erp", wl_int, WLC_GET_LEGACY_ERP, WLC_SET_LEGACY_ERP,
	"Get/Set 11g legacy ERP inclusion (0=disable, 1=enable)" },
	{ "isup", wl_int, WLC_GET_UP, -1,
	"Get driver operational state (0=down, 1=up)"},
	{ "rssi", wl_rssi, WLC_GET_RSSI, -1,
	"Get the current RSSI val, for an AP you must specify the mac addr of the STA" },
	{ "fasttimer", wl_print_deprecate, -1, -1,
	"Deprecated. Use fast_timer."},
	{ "slowtimer", wl_print_deprecate, -1, -1,
	"Deprecated. Use slow_timer."},
	{ "glacialtimer", wl_print_deprecate, -1, -1,
	"Deprecated. Use glacial_timer."},
	{ "dfs_status", wl_dfs_status, WLC_GET_VAR, -1,
	"Get dfs status"},
	{ "dfs_status_all", wl_dfs_status_all, WLC_GET_VAR, -1,
	"Get dfs status of multiple cores or parallel radar scans"},
	{ "radar_status", wl_radar_status, WLC_GET_VAR, -1,
	"Get radar detection status"},
	{ "clear_radar_status", wl_clear_radar_status, -1, WLC_SET_VAR,
	"Clear radar detection status"},
	{ "radar_sc_status", wl_radar_sc_status, WLC_GET_VAR, WLC_SET_VAR,
	"Get/clear sc radar detection status"},
	{ "radar_subband_status", wl_radar_subband_status, WLC_GET_VAR, WLC_SET_VAR,
	"Get/clear subband radar detection status"},
	{ "pwr_percent", wl_int, WLC_GET_PWROUT_PERCENTAGE, WLC_SET_PWROUT_PERCENTAGE,
	"Get/Set power output percentage"},
	{ "toe", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/Disable tcpip offload feature"},
	{ "arpoe", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/Disable arp agent offload feature"},
	{ "wet", wl_int, WLC_GET_WET, WLC_SET_WET,
	"Get/Set wireless ethernet bridging mode"},
	{ "bi", wl_int, WLC_GET_BCNPRD, WLC_SET_BCNPRD,
	"Get/Set the beacon period (bi=beacon interval)"},
	{ "dtim", wl_int, WLC_GET_DTIMPRD, WLC_SET_DTIMPRD,
	"Get/Set DTIM"},
	{ "measure_req", wl_measure_req, -1, WLC_MEASURE_REQUEST,
	"Send an 802.11h measurement request.\n"
	"\tUsage: wl measure_req <type> <target MAC addr>\n"
	"\tMeasurement types are: TPC, Basic, CCA, RPI\n"
	"\tTarget MAC addr format is xx:xx:xx:xx:xx:xx"},
	{ "quiet", wl_send_quiet, -1, WLC_SEND_QUIET,
	"Send an 802.11h quiet command.\n"
	"\tUsage: wl quiet <TBTTs until start>, <duration (in TUs)>, <offset (in TUs)>"},
	{ "pm_mute_tx", wl_pm_mute_tx, -1, WLC_SET_VAR,
	"Sets parameters for power save mode with muted transmission path. Usage:\n"
	"\twl pm_mute_tx 1 <deadline>\t: attempts to enable mode as soon as\n"
	"\t\t\t  timer of <deadline> (milliseconds) expires.\n"
	"\twl pm_mute_tx 0\t: disables mode\n" },
	{ "csa", wl_send_csa, -1, WLC_SET_VAR,
	"Send an 802.11h channel switch anouncement with chanspec:\n"
	"\t<mode> <count> <channel>[a,b][n][u,l][frame type]\n"
	"\tmode (0 or 1)\n"
	"\tcount (0-254)\n"
	"\tchannel format:\n"
	"\t20MHz : [2g|5g]<channel>[/20]\n"
	"\t40MHz : [2g|5g]<channel>/40[u,l]\n"
	"\t80MHz :    [5g]<channel>/80\n"
	"\toptional band 2g or 5g, default to 2g if channel <= 14\n"
	"\tchannel number (0-200)\n"
	"\tbandwidth, 20, 40, or 80, default 20\n"
	"\tprimary sideband for 40MHz on 2g, l=lower, u=upper\n"
	"\tcsa frame type(optional), default is broadcast if not specified, u=unicast"},
	{ "constraint", wl_int, -1, WLC_SEND_PWR_CONSTRAINT,
	"Send an 802.11h Power Constraint IE\n"
	"\tUsage: wl constraint 1-255 db"},
	{ "rm_req", wl_rm_request, -1, WLC_SET_VAR,
	"Request a radio measurement of type basic, cca, or rpi\n"
	"\tspecify a series of measurement types each followed by options.\n"
	"\texample: wl rm_req cca -c 1 -d 50 cca -c 6 cca -c 11\n"
	"\tOptions:\n"
	"\t-t n  numeric token id for measurement set or measurement\n"
	"\t-c n  channel\n"
	"\t-d n  duration in TUs (1024 us)\n"
	"\t-p    parallel flag, measurement starts at the same time as previous\n"
	"\n"
	"\tEach measurement specified uses the same channel and duration as the\n"
	"\tprevious unless a new channel or duration is specified."},
	{ "rm_rep", wl_rm_report, WLC_GET_VAR, -1,
	"Get current radio measurement report"},
#if defined(BCMDBG)
	{ "cca_chan_util_interval", wl_cca_chan_util_interval, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: \n\t wl cca_chan_util_interval [-d num msecs] to config measurement interval\n"
	"\t wl cca_chan_util_interval to query for current measurement interval" },
	{ "cca_chan_util_dur", wl_cca_chan_util_dur, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: \n\t wl cca_chan_util_dur [-d num TUs] to config measurement duration\n"
	"\t wl cca_chan_util_dur to query for current measurement dur\n"
	"\t measurement duration is in TUs (1 TU = 1024 usecs). Default 1000 TUs or 1.024 secs" },
	{ "cca_chan_util", wl_cca_chan_util, WLC_GET_VAR, -1,
	"Usage: \n\t wl cca_chan_util [-v verbose] Returns current channel busy percentage\n" },
	{ "cca_rx_util", wl_cca_rx_util, WLC_GET_VAR, -1,
	"Usage: \n\t wl cca_rx_util Returns current channel Rx busy percentage\n" },
	{ "cca_tx_util", wl_cca_tx_util, WLC_GET_VAR, -1,
	"Usage: \n\t wl cca_tx_util Returns current channel Tx busy percentage\n" },
	{ "cca_stats_raw", wl_cca_raw_stats, WLC_GET_VAR, -1,
	"Usage: \n\t wl cca_stats_raw Returns raw CCA counters\n" },
	{ "cca_stats_delta", wl_cca_delta_stats, WLC_GET_VAR, -1,
	"Usage: \n\t wl cca_stats_delta Returns CCA delta counters\n" },
#endif // endif
	{ "join_pref", wl_join_pref, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get join target preferences."},
	{ "assoc_pref", wl_assoc_pref, WLC_GET_ASSOC_PREFER, WLC_SET_ASSOC_PREFER,
	"Set/Get association preference.\n"
	"Usage: wl assoc_pref [auto|a|b|g]"},
	{ "wme", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set WME (Wireless Multimedia Extensions) mode (0=off, 1=on, -1=auto)"},
	{ "wme_ac", wl_wme_ac_req, WLC_GET_VAR, WLC_SET_VAR,
	"wl wme_ac ap|sta [be|bk|vi|vo [ecwmax|ecwmin|txop|aifsn|acm <value>] ...]"},
	{ "wme_apsd", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set APSD (Automatic Power Save Delivery) mode on AP (0=off, 1=on)" },
	{ "wme_apsd_sta", wl_wme_apsd_sta, WLC_GET_VAR, WLC_SET_VAR,
	"Set APSD parameters on STA. Driver must be down.\n"
	"Usage: wl wme_apsd_sta <max_sp_len> <be> <bk> <vi> <vo>\n"
	"   <max_sp_len>: number of frames per USP: 0 (all), 2, 4, or 6\n"
	"   <xx>: value 0 to disable, 1 to enable U-APSD per AC" },
	{ "wme_dp", wl_wme_dp, WLC_GET_VAR, WLC_SET_VAR,
	"Set AC queue discard policy.\n"
	"Usage: wl wme_dp <be> <bk> <vi> <vo>\n"
	"   <xx>: value 0 for newest-first, 1 for oldest-first" },
	{ "wme_counters", wl_wme_counters, WLC_GET_VAR, -1,
	"print WMM stats" },
	{ "wme_clear_counters", wl_var_void, -1, WLC_SET_VAR,
	"clear WMM counters"},
	{ "wme_tx_params", wme_tx_params, -1, -1,
	"wl wme_tx_params [be|bk|vi|vo [short|sfb|long|lfb|max_rate <value>] ...]"},
	{ "wme_maxbw_params", wme_maxbw_params, WLC_GET_VAR, WLC_SET_VAR,
	"wl wme_maxbw_params [be|bk|vi|vo <value> ....]"},
	{ "lifetime", wl_lifetime, WLC_GET_VAR, WLC_SET_VAR,
	"Set Lifetime parameter (milliseconds) for each ac.\n"
	"wl lifetime be|bk|vi|vo [<value>]"},
	{ "reinit", wl_void, -1, WLC_INIT,
	"Reinitialize device"},
	{ "sta_info", wl_sta_info, WLC_GET_VAR, -1,
	"wl sta_info <xx:xx:xx:xx:xx:xx>"},
	{ "sta_report", wl_sta_report, WLC_GET_VAR, -1,
	"wl sta_report <xx:xx:xx:xx:xx:xx>"},
	{"sta_supp_chan", wl_sta_supp_chan, WLC_GET_VAR, -1,
	"wl sta_supp_chan <xx:xx:xx:xx:xx:xx>"},
	{ "staprio", wl_staprio, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get sta priority \n"
	"Usage: wl staprio <xx:xx:xx:xx:xx:xx> <prio> \n"
	"<prio>: 0~3"},
	{ "pktq_stats", wl_iov_pktqlog_params, WLC_GET_VAR, -1,
	"Dumps packet queue log info for [C] common, [A] AMPDU, [N] NAR or [P] power save queues\n"
	"A:, N: or P: are used to prefix a MAC address (a colon : separator is necessary),\n"
	"or else C: is used alone. The '+' option after the colon gives more details.\n"
	"Up to 4 parameters may be given, the common queue is default when no parameters\n"
	"are supplied\n"
	"Use '/<PREC>' as suffix to restrict to certain prec indices; multiple /<PREC>/<PREC>/..."
	"can be used\n"
	"Also, '//' as a suffix to the MAC address or 'C://' will enable automatic logging of\n"
	"all prec as they are seen.\n"
	"Full automatic operation is also possible with the shorthand\n"
	"'A:' (or 'A://'), 'P:' (or 'P://') etc which scans through all known addresses for\n"
	"those parameters that take a MAC address.\n"
	"wl pktq_stats [C:[+]]|[A:[+]|P:[+]|N:[+]<xx:xx:xx:xx:xx:xx>][/<PREC>[/<PREC>]][//]..." },
	{ "bs_data", wl_scb_bs_data, WLC_GET_VAR, -1,
	"Display per station band steering data\n"
	"usage: bs_data [options]\n"
	"  options are:\n"
	"    -comma    Use commas to separate values rather than blanks.\n"
	"    -tab      Use <TAB> to separate values rather than blanks.\n"
	"    -raw      Display raw values as received from driver.\n"
	"    -noidle   Do not display idle stations\n"
	"    -nooverall  Do not display total of all stations\n"
	"    -noreset  Do not reset counters after reading" },
	{ "rx_report", wl_scb_rx_report, WLC_GET_VAR, -1,
	"Display per station live data about rx datapath\n"
	"usage: rx_report [options]\n"
	"  options are:\n"
	"    -sta xx:xx:xx:xx:xx:xx  only display specific mac addr.\n"
	"    -comma      Use commas to separate values rather than blanks.\n"
	"    -tab        Use <TAB> to separate values rather than blanks.\n"
	"    -raw        Display raw values as received from driver.\n"
	"    -noidle     Do not display idle stations\n"
	"    -nooverall  Do not display total of all stations\n"
	"    -noreset    Do not reset counters after reading" },
	{ "cap", wl_var_getandprintstr, WLC_GET_VAR, -1, "driver capabilities"},
	{ "malloc_dump", wl_print_deprecate, -1, -1, "Deprecated. Folded under 'wl dump malloc"},
	{ "chan_info", wl_chan_info, WLC_GET_VAR, WLC_SET_VAR, "channel info"
	},
	{ "add_ie", wl_add_ie, -1, WLC_SET_VAR,
	"Add a vendor proprietary IE to 802.11 management packets\n"
	"Usage: wl add_ie <pktflag> length OUI hexdata\n"
	"<pktflag>: Bit 0 - Beacons\n"
	"           Bit 1 - Probe Rsp\n"
	"           Bit 2 - Assoc/Reassoc Rsp\n"
	"           Bit 3 - Auth Rsp\n"
	"           Bit 4 - Probe Req\n"
	"           Bit 5 - Assoc/Reassoc Req\n"
	"           Bit 7 - Auth Req\n"
	"Example: wl add_ie 3 10 00:90:4C 0101050c121a03\n"
	"         to add this IE to beacons and probe responses" },
	{ "del_ie", wl_del_ie, -1, WLC_SET_VAR,
	"Delete a vendor proprietary IE from 802.11 management packets\n"
	"Usage: wl del_ie <pktflag> length OUI hexdata\n"
	"<pktflag>: Bit 0 - Beacons\n"
	"           Bit 1 - Probe Rsp\n"
	"           Bit 2 - Assoc/Reassoc Rsp\n"
	"           Bit 3 - Auth Rsp\n"
	"           Bit 4 - Probe Req\n"
	"           Bit 5 - Assoc/Reassoc Req\n"
	"           Bit 7 - Auth Req\n"
	"Example: wl del_ie 3 10 00:90:4C 0101050c121a03" },
	{ "list_ie", _wl_list_ie, WLC_GET_VAR, -1,
	"Dump the list of vendor proprietary IEs" },
	{ "rand", wl_rand, WLC_GET_VAR, -1,
	"Get a 2-byte Random Number from the MAC's PRNG\n"
	"Usage: wl rand"},
	{ "bcmerrorstr", wl_var_getandprintstr, WLC_GET_VAR, -1, "errorstring"},
	{ "freqtrack", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set Frequency Tracking Mode (0=Auto, 1=On, 2=OFF)"},
	{ "eventing", wl_eventbitvec, WLC_GET_VAR, WLC_SET_VAR,
	"set/get hex filter bitmask for MAC event reporting up to application layer"},
	{ "event_msgs_ext", wl_bitvecext, WLC_GET_VAR, WLC_SET_VAR,
	"set/get bit arbitrary size hex filter bitmask for MAC"	},
	{ "event_msgs", wl_eventbitvec, WLC_GET_VAR, WLC_SET_VAR,
	"set/get hex filter bitmask for MAC event reporting via packet indications"},
	{ "counters", wl_counters, WLC_GET_VAR, -1,
	"Return driver counter values. \n"
	"\t wl counters [options]. Options:\n"
	"\t --nz      : only non zero counters\n"
	"\t --err     : only error/warning related counters\n"
	"\t --rx      : only rx specific counters\n"
	"\t --tx\n"
	"\t --ctrl    : only ctrl/mgmt related counters\n"
	"\t --ucode   : only ucode generated counters\n"
	"\t --ucast\n"
	"\t --mcast   : only mcast+bcast related counters\n"
	"\t --sec     : security: only tkip/aes/ related counters\n"
	"\t --ampdu --rx     : combine options to narrow down selection\n"
	"\t --ampdu --invert : use --invert to invert the selection"},
	{ "subcounters", wl_subcounters, WLC_GET_VAR, WLC_SET_VAR,
	"Return driver counter values of requested counters. \n"
	"\t wl subcounters <version> <counters list> - To list requested counters\n"
	"\t wl subcounters - To get the counters version FW is using\n"
	"\t wl subcounters <version> - List supported counters in given version"},
	{ "if_counters", wl_if_counters, WLC_GET_VAR, -1,
	"Return driver counter values for current interface." },
	{ "reset_cnts", wl_clear_counters, WLC_GET_VAR, -1,
	"Clear driver counter values" },
	{ "wlc_ver", wl_wlc_ver, WLC_GET_VAR, -1,
	"returns wlc interface version" },
	{ "delta_stats_interval", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the delta statistics interval in seconds (0 to disable)"},
	{ "delta_stats", wl_delta_stats, WLC_GET_VAR, -1,
	"get the delta statistics for the last interval" },
	{ "swdiv_stats", wl_swdiv_stats, WLC_GET_VAR, -1,
	"Returns swdiv stats"},
	{ "rxfifo_counters", wl_rxfifo_counters, WLC_GET_VAR, -1,
	"Returns rxfifo counters"},
	{ "assoc_info", wl_assoc_info, WLC_GET_VAR, -1,
	"Returns the assoc req and resp information [STA only]" },
	{ "awd_data_info", wl_awd_data_info, WLC_GET_VAR, -1,
	"Returns information by given TYPE:\n"
	"\t AWD_DATA_JOIN_INFO (0)"},
	{ "autochannel", wl_auto_channel_sel, WLC_GET_CHANNEL_SEL, WLC_START_CHANNEL_SEL,
	"auto channel selection: \n"
	"\t1 to issue a channel scanning;\n"
	"\t2 to set chanspec based on the channel scan result;\n"
	"\twithout argument to only show the chanspec selected; \n"
	"\tssid must set to null before this process, RF must be up"},
	{ "csscantimer", wl_int, WLC_GET_CS_SCAN_TIMER, WLC_SET_CS_SCAN_TIMER,
	"auto channel scan timer in minutes (0 to disable)" },
	{ "closed", wl_int, WLC_GET_CLOSED, WLC_SET_CLOSED,
	"hides the network from active scans, 0 or 1.\n"
	"\t0 is open, 1 is hide" },
	{ "pmkid_info", wl_pmkid_info, WLC_GET_VAR, WLC_SET_VAR,
	"Returns the pmkid table" },
	{ "probresp_mac_filter", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get MAC filter based Probe response mode. \n"
	"\t0 - Disable MAC filter based Probe response mode.\n"
	"\t1 - Enable MAC filter based Probe response mode.\n"
	"\tNo parameter - Returns the current setting."},
	{ "eap_restrict", wl_bsscfg_int, WLC_GET_VAR, WLC_SET_VAR,
	"set/get EAP restriction"},
	{ "reset_d11cnts", wl_var_void, -1, WLC_SET_VAR,
	"reset 802.11 MIB counters"},
	{ "staname", wl_varstr, WLC_GET_VAR, WLC_SET_VAR,
	"get/set station name: \n"
	"\tMaximum name length is 15 bytes"},
	{ "apname", wl_varstr, WLC_GET_VAR, -1,
	"get AP name"},
	{ "overlay", wl_overlay, WLC_GET_VAR, WLC_SET_VAR,
	"overlay virt_addr phy_addr size"},
	{ "antgain", wl_antgain, WLC_GET_VAR, WLC_SET_VAR,
	"Set temp ag0/1 value\n"
	"usage: wl antgain ag0=0x1 ag1=0x2"
	},
	{ "phy_antsel", wl_antsel, WLC_GET_VAR, -1,
	"get/set antenna configuration \n"
	"\tset: -1(AUTO), 0xAB(fixed antenna selection)\n"
	"\t\twhere A and B is the antenna numbers used for RF chain 1 and 0 respectively\n"
	"\tquery: <utx>[AUTO] <urx>[AUTO] <dtx>[AUTO] <drx>[AUTO]\n"
	"\t\twhere utx = TX unicast antenna configuration\n"
	"\t\t\turx = RX unicast antenna configuration\n"
	"\t\t\tdtx = TX default (non-unicast) antenna configuration\n"
	"\t\t\tdrx = RX default (non-unicast) antenna configuration"
	},
	{ "txfifo_sz", wl_txfifo_sz, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the txfifo size; usage: wl txfifo_sz <fifonum> <size_in_bytes>" },
#if defined(linux)
	{ "escan_event_check", wl_escan_event_check, -1, -1,
	"Listen and prints the escan events from the dongle\n"
	"\tescan_event_check syntax is: escan_event_check ifname flag\n"
	"\tflag 1 = sync_id info, 2 = bss info, 4 = state + bss info [default], "
	"8 = TLV check for IEs"},
	{ "escanresults", wl_escanresults, -1, WLC_SET_VAR,
	"Start escan and display results.\n" SCAN_USAGE
	},
	{ "wl_event_check", wl_event_check, -1, -1,
	"listen and display brcm events\n"
	"\tusage:wl -i <ifname> wl_event_check"},
#endif   /* linux */

	{ "hs20_ie", wl_hs20_ie, -1, WLC_SET_VAR,
	"set hotspot 2.0 indication IE\n"
	"\tusage: wl hs20_ie <length> <hexdata>\n"
	},
	{"rate_histo", wl_rate_histo, -1, WLC_GET_VAR,
	"Get rate hostrogram"
	},
	{ "wme_apsd_trigger", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Set Periodic APSD Trigger Frame Timer timeout in ms (0=off)"},
	{ "wme_autotrigger", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/Disable sending of APSD Trigger frame when all ac are delivery enabled"},
	{ "reassoc", wl_reassoc, -1, WLC_REASSOC,
	"Initiate a (re)association request.\n"
	"\tUsage: wl reassoc <bssid> [options]\n"
	"\tOptions:\n"
	"\t-c CL, --chanspecs=CL \tchanspecs (comma or space separated list)"},
	{ "send_nulldata", wl_iov_mac, -1, -1,
	"Sed a null frame to the specified hw address" },
	{ "srchmem", wl_srchmem, WLC_GET_VAR, WLC_SET_VAR,
	"g/set ucode srch engine memory"},
	{"ptk_start", wl_ptk_start, -1, WLC_SET_VAR,
	"Send specified \"wl_ptk_start \" params .\n"
	"\tUsage: wl ptk_start <sec type> <awdl peer_addr> <pmk> <role>\n"},
#ifdef CLMDOWNLOAD
	{ "clmload", wl_clmload, -1, WLC_SET_VAR,
	"Download CLM data into a driver.  Driver must be down.\n"
	"\tUsage: wl clmload <clm blob file name>\n"
	"\t  Note obsolete syntax 'wl clmload 0/1 <clm blob file name>' is still accepted\n"
	"\t  but the download type 0/1 is no longer applicable and is ignored.  Incremental\n"
	"\t  CLM download is no longer supported.  Also reverting to the original built-in\n"
	"\t  CLM is no longer supported.  (This syntax was 'wl clmload 0/1')"},
	{ "txcapload", wl_txcapload, -1, WLC_SET_VAR,
	"Download txcap data into a driver.  Driver must be down.\n"
	"\tUsage: wl txcapload <txcap file name>\n"},
	{ "txcapver", wl_var_getandprintstr, WLC_GET_VAR, -1,
	"List curent txcap downloaded information"},
	{ "txcapconfig", wl_txcapctl, WLC_GET_VAR, WLC_SET_VAR,
	"Set or get the txcap config setting for the low/high cap selection for each subband"},
	{ "txcapstate", wl_txcapctl, WLC_GET_VAR, WLC_SET_VAR,
	"Set or get the txcap state setting for the low/high cap selection for each subband"},
	{ "txcapdump", wl_txcapdump, WLC_GET_VAR, -1,
	"Get txcap dump information.  Intended for design verification/debugging as opposed to\n"
	"\tproduction usage and as such may change with little (or no) notice."},
#endif /* CLMDOWNLOAD */
	{ "calload", wl_calload, -1, WLC_SET_VAR,
	"Download CAL data into a driver.  Driver must be down.\n"
	"\tUsage: wl calload <cal file name> to download existing calibration data file\n"},
	{ "caldump", wl_caldump, WLC_GET_VAR, -1,
	"Dump calibration data and save it with calibration storage format.\n"
	"\tUsage: wl caldump <cal file name> to dump current calibration info to file\n"},
	{ "calload_chkver", wl_buf, -1, WLC_SET_VAR,
	"Confirm TxCal load with provided version string"},
	{ "bmac_reboot", wl_var_void, -1, WLC_SET_VAR,
	"Reboot BMAC"},
#ifdef RWL_WIFI
	{ "findserver", wl_wifiserver, -1, -1,
	"Used to find the remote server with proper mac address given by the user,this "
	"cmd is specific to wifi protocol."},
#endif // endif
	{ "assertlog", wl_assertlog, WLC_GET_VAR, -1,
	"get external assert logs\n"
	"\tUsage: wl assertlog"},
	{ "assert_type", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the asset_bypass flag; usage: wl assert_type <1/0> (On/Off)"
	},
	{"cca_get_stats", wl_cca_get_stats, WLC_GET_VAR, -1,
	"Usage: wl cca_stats [-c channel] [-s num seconds][-n]\n"
	"\t -c channel: Optional. specify channel. 0 = All channels. Default = current channel \n"
	"\t -n: no analysis of results\n"
	"\t -s num_seconds: Optional. Default = 10, Max = 60\n"
	"\t -i: list individual measurements in addition to the averages\n"
	"\t -curband: Only recommend channels on current band"
	},
	{"cca_stats_mesh", wl_cca_stats_mesh, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: wl cca_stats_mesh [-c channel] [-s num seconds][-h]\n"
	"\t\t or\n"
	"\t  wl cca_stats_mesh [enable /disable]\n"
	"\t -c channel: Optional. specify channel. 0 = All channels. Default = current channel \n"
	"\t -s num_seconds: Optional. Default = 10, Max = 60:"
	" Feature is not supported in dongle builds\n"
	"\t -m < MAC_ID>: MAC_ID is added to the amt table for cca_stats\n"
	"\t -d < MAC_ID>: MAC_ID is deleted from the amt table if present\n"
	"\t mac_table : prints the list of active MAC_IDs used in collecting mesh_stats\n"
	"\t -da : all cca_stats collection will be deleted\n"
	"\t -h  : prints this message"
	},
	{ "smfstats", wl_smfstats, WLC_GET_VAR, WLC_SET_VAR,
	"get/clear selected management frame (smf) stats"
	"\twl smfstats [-C num]|[--cfg=num] [auth]|[assoc]|[reassoc]|[clear]\n"
	"\tclear - to clear the stats" },
#ifdef RWL_DONGLE
	{ "dongleset", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Enable uart driver"
	},
#endif // endif
	{ "manfinfo", wl_var_getandprintstr, WLC_GET_VAR, -1,
	"show chip package info in OTP"},
	{ "pm_dur", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Retrieve accumulated PM duration information (GET only)\n"
	},
	{ "mpc_dur", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Retrieve accumulated MPC duration information in ms (GET) or clear accumulator (SET)\n"
	"\tUsage: wl mpc_dur <any-number-to-clear>"},
	{"txdelay_params", wl_txdelay_params, WLC_GET_VAR, -1,
	"get chanim stats \n"
	"\t Usage: wl txdelay_params ratio cnt period tune"
	},
	{"intfer_params", wl_intfer_params, WLC_GET_VAR, WLC_SET_VAR,
	"set/get intfer params \n"
	"\tUsage: wl intfer_params period (in sec) cnt(0~4) txfail_thresh tcptxfail_thresh\n"
	"\tperiod=0: disable Driver monitor txfail"
	},
	{"dlystats", wl_dlystats, WLC_GET_VAR, -1,
	"dump delay statistics\n"
	"\tUsage: wl dlystats [xx:xx:xx:xx:xx:xx] (optional mac address)\n"
	"\tIf mac_addr is not specified, dump all of scbs"
	},
	{"dlystats_clear", wl_dlystats_clear, -1, WLC_SET_VAR,
	"clear delay stats \n"
	"\t Usage: wl dlystats_clear"
	},
	{ "dngl_wd", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"enable or disable dongle keep alive watchdog timer\n"
	"\tUsage: wl dngl_wd 0\\1 (to turn off\\on)"},
	{ "tsf", wl_tsf, WLC_GET_VAR, WLC_SET_VAR,
	"set/get tsf register\n"
	"\tUsage: wl tsf [<high> <low>]"},
	{ "mac_rate_histo", wl_mac_rate_histo, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: wl mac_rate_histo <mac address> <access category> <num_pkts>\n"
	"\t(MAC address e.g. 00:11:20:11:33:33)\n"
	"\t(Access Category(AC) - 0x10:for entire MAC or 0x4:for video AC for this MAC)\n"
	"\t(num_pkts (optional) - Number of packets to average - max 64 for AC 0x10,"
	" max 32 for AC 0x4)"
	},
	{ "rate_histo_report", wl_rate_histo_report, WLC_GET_VAR, -1,
	"Usage: wl rate_histo_report [mac address]\n"
	"\tClient device MAC address (eg. 11:22:33:44:55:66) is required when used on an AP"
	},
#ifdef SERDOWNLOAD
	{ "rwl_download", rwl_download, -1, WLC_SET_VAR,
	"rwl_download  <firmware> <nvram>\n"
	"\trwl_download transfer firmware and nvram via remote interface\n"
	},
	{ "init", dhd_init, WLC_GET_VAR, WLC_SET_VAR,
	"init <chip_id>\n"
	"\tInitialize the chip.\n"
	"\tCurrently only 4325, 4329, 43291, 4330a1 and 4330 (b1) are supported"
	},
	{ "download", dhd_download, WLC_GET_VAR, WLC_SET_VAR,
	"download  <binfile> <varsfile>\n"
	"\tdownload file to dongle ram and start CPU\n"
	"\tvars file will replace vars parsed from the CIS"
	},
	{ "upload", dhd_upload, WLC_GET_VAR, WLC_SET_VAR,
	"upload <file> \n"
	"\tupload the entire memory and save it to the file"
	},
#endif /* SERDOWNLOAD */
	{ "rpmt", wl_rpmt, -1, WLC_SET_VAR, "rpmt <pm1-to> <pm0-to>"},
	{ "ie", wl_ie, WLC_GET_VAR, WLC_SET_VAR,
	"set/get IE\n"
	"\tUsage: For set: wl ie type length hexdata\n"
	"\t     For get: wl ie type" },
	{ "mempool", wlu_mempool, WLC_GET_VAR, -1,
	"Get memory pool statistics" },
#ifdef SR_DEBUG
	{ "sr_dump_pmu", wl_dump_pmu, WLC_GET_VAR, WLC_SET_VAR,
	"Dump value of PMU registers"},
	{ "sr_pmu_keep_on", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Keep resource on"},
	{ "sr_power_island", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Keep power islands on/off.\n"
	"Usage: For get:wl sr_power_island\n"
	"       For set:wl sr_power_island 0x????\n"
	"        where ?-> 0 power_island off\n"
	"              ?-> 1 power_island on\n"
	"              eg: wl sr_power_island 0x1101"},
#endif /* SR_DEBUG */
	{ "antdiv_bcnloss", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"0	- Disable Rx antenna flip feature based on consecutive beacon loss\n"
	"\tX - beacon loss count after which Rx ant will be flipped\n"
	"\tUsage: wl antdiv_bcnloss <beaconloss_count>\n"
	},
	{ "lpc_params", wl_power_sel_params, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get Link Power Control params\n"
	"\tUsage: wl lpc_params <rate_stab_thresh> <pwr_stab_thresh>\n"
	"\t\t<lpc_exp_time> <pwrup_slow_step>\n"
	"\t\t<pwrup_fast_step> <pwrdn_slow_step>\n"},
	{ "nar_clear_dump", wl_var_void, -1, WLC_SET_VAR,
	"Clear non-aggregated regulation counters"},
	{ "sar_limit", wl_sarlimit, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get sar_limit\n"
	"\tusage: (set) sar_limit <2Gcore0 2Gcore1 2Gcore2 2Gcore3 5G[0]core0 5G[0]core1...>\n"
	"\t       (get) sar_limit, return sar limit table\n"
	"\tunit: all input/output values are absolute and in unit of qdbm\n"
	},
	{ "event_log_set_init", wl_event_log_set_init, -1, WLC_SET_VAR,
	"Initialize an event log set\n"
	"\tUsage: wl event_log_set_init <set> <size>\n"},
	{ "event_log_set_expand", wl_event_log_set_expand, -1, WLC_SET_VAR,
	"Increase the size of an event log set\n"
	"\tUsage: wl event_log_set_expand <set> <size>\n"},
	{ "event_log_set_shrink", wl_event_log_set_shrink, -1, WLC_SET_VAR,
	"Decrease the size of an event log set\n"
	"\tUsage: wl event_log_set_shrink <set> <[size]>\n"},
	{ "event_log_tag_control", wl_event_log_tag_control, -1, WLC_SET_VAR,
	"Modify the state of an event log tag\n"
	"\tUsage: wl event_log_tag_control <tag> <set> <flags>\n"},
	{ "event_log_get", wl_event_log_get, -1, WLC_SET_VAR,
	"\t usage: wl event_log_get [-f <set_num>] [-g <set_num> -s <buf_size>]\n"
	"\t -f: flush a log buffer of specified set being written  to by trigerring logtrace\n"
	"\t -g: get a log buffer of a specified set that is full but not delivered to host yet\n"
	"\t     Store the contents in a buffer of size specified with -s option\n"
	"\t Note: Only -f option is currently supported\n"
	},
	{ "rmc_ar", wl_mcast_ar, WLC_GET_VAR, WLC_SET_VAR,
	"Set active receiver to the one that matches the provided mac address\n"
	"If there is no match among current RMC receivers, it will return fail\n"
	"If mac address is set to all 0 (00:00:00:00:00:00), auto selection mode is enabled\n"
	"and the transmitter will choose the active receiver automatically by RSSI\n"
	"\tusage: wl rmc_ar [mac address]\n"
	"Get the device mac that is set to be the active receiver for this transmitter\n"
	"\tusage: wl rmc_ar\n"
	},

	{ "pm2_sleep_ret_ext", wl_sleep_ret_ext, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set Dynamic Fast Return To Sleep params"},
	{ "sta_monitor", wl_stamon_sta_config, WLC_GET_VAR, WLC_SET_VAR,
	"wl sta_monitor [<add> <xx:xx:xx:xx:xx:xx>[<chanspec> <off-channel time>]] "
	"[<del|stats> <xx:xx:xx:xx:xx:xx>] [<monitor_time> <timer value in ms>]"
	" [<enable|disable|counters|reset_cnts>]"},
	{ "monitor_promisc_level", wl_monitor_promisc_level, WLC_GET_VAR, WLC_SET_VAR,
	"Set a bitmap of different MAC promiscuous level of monitor mode.\n\n"
	MONITOR_PROMISC_LEVEL_USAGE},
	{ "taf", wl_taf_def, WLC_GET_VAR, WLC_SET_VAR,
	"wl taf <MAC> [<scheduler_id> [<priority>]]\n"
	"wl taf <scheduler_id> [coeff [<coeff>]|dump|list]\n"
	"wl taf enable [0|1]|order [0|1]|bypass [0|1]|high [<val>]|low [<val>]|force [<val>]|list"
	},
	{"bcnlenhist", wl_bcnlenhist, WLC_GET_VAR, -1,
	"Usage: wl bcnlenhist [0]"},
	{ "bss_peer_info", wl_bss_peer_info, WLC_GET_VAR, -1,
	"Get BSS peer info of all the peer's in the indivudual interface\n"
	"\tIf a non-zero MAC address is specified, gets the peer info of the PEER alone\n"
	"\tUsage: wl bss_peer_info [MAC address]"},
#if defined(BCMDBG)
	{ "dump_modesw_dyn_bwsw", wl_dump_modesw_dyn_bwsw, WLC_GET_VAR, -1,
	"Usage : wl dump_modesw_dyn_bwsw" },
#endif // endif
	{ "pwrstats", wl_pwrstats, WLC_GET_VAR, -1,
	"Get power usage statistics\n"
	"Usage: wl pwrstats [<type>] ..."},
	{ "memuse", wl_memuse, WLC_GET_VAR, -1,
	"Get memory usage statistics\n"
	"Usage: wl memuse"},
	{ "drift_stats_reset", wl_var_void, -1, WLC_SET_VAR,
	"Reset drift statistics"},
	{ "ibss_route_tbl", wl_setiproute, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set ibss route table\n"
	"\tUsage: wl ibss_route_tbl num_entries [{ip_addr1, mac_addr1}, ...]"},
	{ "ip_route_table", wl_setiproute, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set ip route table\n"
	"\tUsage: wl ip_route_tbl num_entries [{ip_addr1, mac_addr1}, ...]"},
	{ "rsdb_mode", wl_bcm_config, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get the RSDB mode. Possible values auto(-1), mimo(0), rsdb(1), 80p80(2)"},
	{ "desired_bssid", wl_desired_bssid, WLC_GET_DESIRED_BSSID, WLC_SET_DESIRED_BSSID,
	"Set or get the desired BSS ID value\n"
	"\tUsage: wl desired_bssid [BSSID]"},
	{ "ht_features", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"disable/enable/force proprietary 11n rates support. Interface must be down." },
	{ "modesw_timecal", wl_modesw_timecal, WLC_GET_VAR, WLC_SET_VAR,
	"Usage: \n\t wl modesw_timecal 0~1 for disable /enable \n"
	"\t wl modesw_timecal to get Time statistics" },
	{ "atim", wl_int, WLC_GET_ATIM, WLC_SET_ATIM,
	"Set/Get the current ATIM window size" },
	{ "pcie_bus_tput", wl_pcie_bus_throughput_params, WLC_GET_VAR, WLC_SET_VAR,
	"Measure the pcie bus througput\n"
	"Usage: wl pcie_bus_tput -n 64\n"},
	{ "interface_create", wl_interface_create_action, -1, WLC_GET_VAR,
	"create an interface on a WLC instance that receives the IOVAR\n"
	"\tUsage: wl interface_create <interface_type> [-m <MAC-address>] [-b <BSSID>] "
	"[-f if_index]\n"
	"[Optional] MAC-address: xx:xx:xx:xx:xx:xx\n"
	"[Optional] BSSID: yy:yy:yy:yy:yy:yy\n"
	"[Optional] if_index: Interface index to be used in FW"
	},
	{ "interface_remove", wl_interface_remove_action, -1, WLC_SET_VAR,
	"Deletes the interface on which this command is received\n"
	"\tUsage:\n\t wl interface_remove\n"
	"\t wl -i <interface_name> interface_remove \n"
	"\t wl interface_remove -C <bss_cfg_index> \n"
	},
	{"phy_txpwrcap_tbl", wl_phy_txpwrcap_tbl, WLC_GET_VAR, WLC_SET_VAR,
	"Get the stored txpwr cap table:\n"
	"\t\twl phy_txpwrcap_tbl\n"
	"\tSet the txpwr cap table:\n"
	"\t\twl phy_txpwrcap_tbl <Na0> <Na1> <Na2> <Na3> <Na4> <Na5> <Na6> <Na7>"
	" <Cap_cell_ON> <Cap_cell_OFF>\n"
	"\t\tNaX: Number of Antennas on Core X\n"
	"\t\tCap_cell_ON: Pwr caps for valid antennas on all cores for Cell On status\n"
	"\t\tCap_cell_OFF: Pwr caps for valid antennas on all cores for Cell Off status\n"
	"\t\tPwr Caps are in qdBm, int8 format. Cap_cell_OFF values are optional.\n"},
#ifdef ATE_BUILD
	{ "gpaio", wl_gpaio, NULL, WLC_SET_VAR,
	"Configure the GPAIO using different options as follows:\n\n"
	"\tgpaio pmu_afeldo\n\n"
	"\tgpaio pmu_txldo\n\n"
	"\tgpaio pmu_vcoldo\n\n"
	"\tgpaio pmu_lnaldo\n\n"
	"\tgpaio pmu_adcldo\n\n"
	"\tgpaio clear\n\n"},
#endif // endif
	{ "dfs_ap_move", wl_dfs_ap_move, WLC_GET_VAR, WLC_SET_VAR,
	"Move the AP interface to dfs channel specified:\n"
	"\t Default: Get the dfs scan status\n"
	"\t -1: Abort recent AP move request (if in progress)\n"
	"\t -2: Stunt recent AP move request (if in progress)\n"
	"\t20MHz : [5g]<channel>[/20]\n"
	"\t40MHz : [5g]<channel>/40[u,l]\n"
	"\t80MHz :    [5g]<channel>/80\n"
	"\tchannel number (36-200)\n"
	"\tbandwidth, 20, 40, or 80, default 20\n"
	"\tprimary sideband for 40MHz on 2g, l=lower, u=upper\n"
	"OR Set channel with legacy format:\n"
	"\t-c channel number (36-224)\n"
	"\t-w bandwidth 20 or 40\n"
	"\t-s ctl sideband, -1=lower, 0=none, 1=upper"},
	{"pmac", wl_macdbg_pmac, WLC_GET_VAR, -1,
	"Get mac obj values such as of SHM and IHR\n"
	"\tusage: wl pmac <type> <addresses up to 16> -s <step size>"
	" -n <num> -b <bitmap> -w <write val> -r\n"
	"<addresses> : a single address, or multiple, upto 16 addresses, in Decimal or Hex\n"
	"<step size> = 0 or 2 or 4\n"
	"<num> : how many to print\n"
	"<bitmap> : 32-bit value\n"
	"<w_val> : write value to the registers\n"
	"-r option is used to specify internal address:"},
	{"msched", wl_musched_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"msched   mu scheduler control commands\n"
	"         Usage: wl msched [command] [cmd options] [-ul] [-dl]\n"
	"Available commands and options:\n"
	"  wl msched policy [0|1|2|-1] - set/get policy 0:disable 1:fixed 2:trivial -1:auto\n"
	"  wl msched rucfg [-r row] [-c col] ru1 ru2 ...ru8 - set rucfg table\n"
	"    e.g., rucfg -r 2 67 67s, where 67s indicates RU67 in secondary 80MHz\n"
	"  wl msched rucfg [-r row] [-c col] ru1,ru2,...ru8 - set rucfg table option 2\n"
	"  wl msched rucfg [-r row] [-c col] ru1-ru8 - set rucfg table option 3\n"
	"  wl msched rucfg [-r row] [-c col] get rucfg table\n"
	"  wl msched rucfg_ack [-r row] [-c col] set/get rucfg table for hetb ack\n"
	"  wl msched lowat [-b bw] val - set/get low water mark value\n"
	"  wl msched maxn [-b bw] val - set/get max N value\n"
	"  wl msched tmout val - set/get schedule timeoutN value\n"
	"  wl msched ackpolicy val - set/get ackpolicy 0:serial 1:trig_in_amp 2:mu_bar\n"
	"  wl msched snd val - set/get sounding frame sequence 0:hetb 1:hesu 2:bfp\n"
	"  wl msched mindlusers - set/get min dlofdma capable users required to enable dlofdma\n"
	"  wl msched maxofdmausers - set/get max dl ofdma users\n"
	"  wl msched murts - set/get if use MU-RTS\n"
	"  wl msched help - print this help message\n"},
	{"umsched", wl_umusched_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"umsched   ul ofdma mu scheduler control commands\n"
	"         Usage: wl umsched [command] [cmd options] [-u <mac addr>]\n"
	"Available commands and options:\n"
	"  wl umsched                         - query ul ofdma scheduler info\n"
	"  wl umsched mctl <val>              - mac-control info\n"
	"  wl umsched txcnt <val>             - txmode info\n"
	"  wl umsched rclmt <val>             - retry counter limit\n"
	"  wl umsched interval <val>          - polling internval\n"
	"  wl umsched burst <val>             - burst size/number\n"
	"  wl umsched maxdur <val>            - max duration\n"
	"  wl umsched cpltf <val>             - cpltf in trigger. 1, 2 are valid (default 2)\n"
	"  wl umsched nltf <val>              - Set target minimum encoded nltf (default 0)\n"
	"  wl umsched mcs <val> [-u <addr>]   - fix MCS for all or a given user\n"
	"  wl umsched nss <val> [-u <addr>]   - fix NSS for all or a given user\n"
	"  wl umsched trssi <val> [-u <addr>] - fix target rssi for all or a given user\n"
	"  wl umsched maxn [-b bw] <val>      - max number of users in trigger frame (per BW)\n"
	"  wl umsched maxclients <val>        - max number of admitted ul ofdma clients.\n"
	"  wl umsched fb <val>                - 1: turn on ul ofdma frameburst; 0: turn off\n"
	"  wl msched help - print this help message\n"},
	{"txbfcfg", wl_txbfcfg_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"txbfcfg  txbf configuration commands\n"
	"         Usage: wl txbfcfg [idx] [val]\n"},
	{"monitor_config", wl_monitor_config, WLC_GET_VAR, WLC_SET_VAR,
	"Configure monitor parameters:\n\n"
	 MONITOR_CONFIG_USAGE
	},
	{ "vasip_counters_clear", wl_var_void, -1, WLC_SET_VAR,
	"clear vasip counters"},
	{ "vasip_error_clear", wl_var_void, -1, WLC_SET_VAR,
	"clear error counters"},
	{"svmp_mem", wl_svmp_mem, WLC_GET_VAR, WLC_SET_VAR,
	"Usage for set mode: wl svmp_mem <offset> <len> <value>\n"
	"\tUsage for read mode: wl svmp_mem <offset> <len> [<-b B> <-c C> <-m M>]\n"
	"\t  where <len> for number of 16-bit entries and <value> is 16-bit value\n"
	"\tSet mode will set SVMP (starting from offset for len) with the given value\n"
	"\tRead mode will read SVMP (starting from offset for len) and"
	" with <-b B> or <-c C> for print-layout options\n"
	"\t  '-b': bitwidth, only valid for B = 16 or 32 (default=32)\n"
	"\t  '-c': number of columns (entries) in a row, only valid for C = 1~16 (default=1)\n"
	"\t  '-m': display mode, where M=0 for hex without 0x, M=1 for hex with 0x,"
	" M=2 for decimal, M=3 for complex (default=0)"},
	{"mu_rate", wl_mu_rate, WLC_GET_VAR, WLC_SET_VAR,
	"Force the tranmission rate for each user, rate0 is for user0; rate1 is for user1...\n"
	"Usage: wl mu_rate { [auto | -1] | [[rate0] [rate1] [rate2] [rate3]]\n"
	"no input: read current MU-MIMO rate.\n"
	"auto or -1: turn on auto rate.\n"},
	{"mu_group", wl_mu_group, WLC_GET_VAR, WLC_SET_VAR,
	"Force the group recommendation result or set parameters for VASIP group recomendation\n"
	"\tUsage: no parameters means getting configs\n"
	"\t  'wl mu_group [-g P0 [P01 P02 ... P04] [... -g Px [Px1 Px2 ... Px4]]] [-f F]'\n"
	"\t  'wl mu_group [-g -1] [-m M] [-n N]'\n"
	"\t  Combination of '-g 0 XXX' with '-m M' or '-n N' is invalid\n"
	"\t       Example1: wl mu_group -g 0 0x016 0x209 0x309 -g 1 0x009 0x217 -g 2 0x115 0x308\n"
	"\t       Example2: wl mu_group -g 0 0x007 0x109 0x209 0x308 -f 0\n"
	"\t       Example3: wl mu_group -g -1\n"
	"\t       Example4: wl mu_group -g -1 -m 1 -n 4\n"
	"\t       Example5: wl mu_group -m 1 -n 4 (only valid under auto grouping)\n"
	"\t  -g: Force group recommendation (x<=7, up to 8 options)\n"
	"\t       P0=-1 means VASIP group recommendation (not-forced mode, default)\n"
	"\t       P0~Px are expected to be 0~x in forced mode\n"
	"\t       Pxy: three nibbles for (user_id<<8 + (user_nss-1)<<4 + user_mcs)\n"
	"\t  -f: Force MCS and only valid with '-g 0 XXX'\n"
	"\t       F=0: auto MCS from VASIP MCS recommendation\n"
	"\t       F=1: forced MCS according to '-g' argument (default when froced grouping)\n"
	"\t  -m: Method for VASIP group recommendation, M>=0\n"
	"\t       M=0: old method: 1 group for all admitted users with GID=9\n"
	"\t       M>0: new method: M=1 for N best THPT groups\n"
	"\t  -n: Number of groups reported to MAC for VASIP group recommendation, N=1~15"},
	{"dy_ed_setup", wl_ded_setup, WLC_GET_VAR, WLC_SET_VAR,
	"Configure the Dynamic ED threshold settings\n"
	"\tUsage: no parameters means getting configs\n"
	"\t  'wl dy_ed_setup [-win W] [-seddis D][-maxsed M] [-minsed N] [-maxth H] [-minth L]"
	" [-inc I] [-dec D]'\n"
	"\t       Example1: wl dy_ed_setup -win 2\n"
	"\t       Example2: wl dy_ed_setup -minth -78\n"
	"\t  -win: Configure the monitoring window length (in sec)\n"
	"\t  -seddis: Configure the SED_DISABLE (in %). SED higher than"
	" this will turn off the feature\n"
	"\t  -maxsed: Configure the max SED (in %)."
	" SED higher than this will trigger threshold change (if feature is enabled)\n"
	"\t  -minsed: Configure the min SED (in %). SED lower than this"
	" will trigger threshold change (if feature is enabled)\n"
	"\t  -maxth: Configure the maximum allowed threshold for dynamic ED (in dbm)\n"
	"\t  -minth: Configure the minimum allowed threshold for dynamic ED (in dbm)\n"
	"\t  -inc: Configure the step size for threshold increase (dB)\n"
	"\t  -dec: Configure the step size for threshold decrease (dB)\n\n"
	"\t  Note that the acceptable range for maxth and minth should satisfy"
	" -75<= minth <=maxth <=-20. Otherwise the configuration will be rejected. \n"
	},
	{"max_muclients", wl_muclients, WLC_GET_VAR, WLC_SET_VAR,
	"Configure the number of admitted users per technology\n"
	"\tUsage: Displays current config when no input\n"
	"\t  'wl max_muclients [-T <value>]\n"
	"\t       T=vhtmu: Configure max admitted users for VHT MU-MIMO\n"
	"\t       T=hemmu: Configure max admitted users for HE MU-MIMO\n"
	"\t       T=dlofdma: Configure max admitted users for HE-OFDMA\n"
	},
	{"mu_policy", wl_mu_policy, WLC_GET_VAR, WLC_SET_VAR,
	"Configure the MU admission control policies\n"
	"\tUsage: no parameters means getting configs\n"
	"\t  'wl mu_policy [-sched_timer T] [-pfmon P] [-pfmon_gpos G] [-samebw B]"
	" [-nrx R] [-max_muclients C]'\n"
	"\t       Example1: wl mu_policy -sched_timer 60 -pfmon 1 -pfmon_gpos 0 -samebw 0 -nrx 1\n"
	"\t       Example2: wl mu_policy -sched_timer 0\n"
	"\t       Example3: wl mu_policy -pfmon 0\n"
	"\t       Example4: wl mu_policy -nrx 2\n"
	"\t       Example5: wl mu_policy -max_muclients 4\n"
	"\t  -sched_timer: Configure the timer interval for the score based MU client scheduler\n"
	"\t       T=0 means the scheduler is disabled\n"
	"\t       T>0 means the timer duration in seconds (default 60)\n"
	"\t  -pfmon: Configure the perfomance monitors (mutxcnt and gpos)'\n"
	"\t       P=0: Disable the perfomance monitors\n"
	"\t       P=1: Enable the perfomance monitors and black lists\n"
	"\t  -pfmon_gpos: Configure the gpos performance monitor\n"
	"\t       G=0: Disable the gpos performance monitor\n"
	"\t       G=1: Enable the gpos performance monitor\n"
	"\t  -samebw: Configure the BW check at admission control\n"
	"\t       B=0: Allow clients with different BW to be admitted\n"
	"\t       B=1: Only clients with the same BW can be admitted\n"
	"\t  -nrx: Configure the max nrx (number of RX streams) of the clients\n"
	"\t       R=1: Only 1x1 MU STAs can be admitted\n"
	"\t       R=2: Both 1x1 and 2x2 MU STAs can be admitted\n"
	"\t  -max_muclients: Configure the max number of clients\n"
	"\t       C: Can be a value between 2~8"},
	{"muinfo", wl_muinfo, WLC_GET_VAR, -1,
	"Get MU-MIMO state information and statistics.\n"
	"\tUsage: wl muinfo <option>\n"
	"\t\t-v: more detail information\n"},
#if defined(BCMDBG)
	{ "svmp_sampcol", wl_svmp_sampcol, WLC_GET_VAR, WLC_SET_VAR,
	"No parameters means getting configs\n"
	"\tOptional parameters for BCM4365 are:\n"
	"\t-e enable capture, 0: enable, 1: disable,"
	    "without this field means setting configs only\n"
	"\t-t trigger mode, 0: pkt-proc transition trigger (need 2 additional states setting),"
	    "1: force immediate trigger, 2: radar-det trigger\n"
	"\t-w waiting counter in sample (default: 0)\n"
	"\t-l capture length in sample (default: 128)\n"
	"\t-s source select, set phy1_mux and rx1_mux\n"
	"\t\tphy1_mux, 0: gpioOut, 1: fftOut, 2: dbgHx, 3: rx1mux\n"
	"\t\trx1_mux, 4: farrowOut, 6: dcFilterOut, 7: rxFilterOut, 8: aciFilterOut\n"
	"\t-d dual capture mode, use ACPHY sample_collect HW (default: off)\n"
	"\t\tdata_mux, 4: farrowOut, 5: iqCompOut, 6: dcFilterOut, 7: rxFilterOut\n"
	"\t-r samplerate, 0: 1xBW, 1: 2xBW (only farrowOut supports 2xBW)\n"
	"\t-p memory packing selection, set packing_mode, packing_order, packing_format,"
	    "single-core selection (default 1 0 0 0)\n"
	"\t\tpacking_mode, 0: for dual capture, 1: 4-core, 2: first 2-core, 3: single-core\n"
	"\t\tpacking_order, 0: big endian, 1: little endian\n"
	"\t\tpacking_format, 0: IQswap of cfix, 1: VASIP cfix format,"
	    "4365A0/B0 only supports IQswap of cfix\n"
	"\t\tsingle-core selection, 0~3 (4365A0/B0 only forces to core0)\n"
	"\t-a SVMP buffer address in word size, set addr_start and addr_end"
	    "(default 0x25800 0x26800)\n"
	"\t-i send interrupt to VASIP when capture done (default 1)\n"},
#endif // endif
	{ "scanmac", wl_scanmac, WLC_GET_VAR, WLC_SET_VAR,
	"Configure scan MAC using subcommands:\n"
	"\tscanmac enable <0|1>\n"
	"\tscanmac bsscfg\n"
	"\tscanmac config <mac> <random_mask> <scan_bitmap>\n"},
	{"mkeep_alive", wl_mkeep_alive, WLC_GET_VAR, WLC_SET_VAR,
	"Send specified \"mkeep-alive\" packet periodically.\n"
	"Usage: wl mkeep_alive <index0-7> [[<immediate>] <period> <packet>]\n"
	"\tindex: 0 - 7 (for set or get).\n"
	"\timmediate: Send first packet immediately.\n"
	"\tperiod: Re-transmission period in msecs, 0 to disable.\n"
	"\tpacket: Hex packet contents to transmit. The packet contents should\n"
	"\t  include the entire ethernet packet (ethernet header, IP header, UDP\n"
	"\t  header, and UDP payload) specified in network byte order. If no\n"
	"\t  packet is specified, a nulldata frame will be sent instead.\n\n"
	"E.g. Send keep alive packet every 30 seconds using id-1:\n"
	"\twl mkeep_alive 1 30000 0x0014a54b164f000f66f45b7e08004500001e000040004011c"
	"52a0a8830700a88302513c413c4000a00000a0d\n" },
	{ "tcmstbl", wl_get_tcmstbl_entry, WLC_GET_VAR, -1,
	"Returns the total power in hdBm and the Tx core Mask for the combination: \n"
	"\t-c core number (0-7) default 0\n"
	"\t-s cell status (0 or 1) default 0\n"
	"\t-a antenna map (each nibble holds one core, core0 LSB) default 0x0 \n"},
	{ "dfs_max_safe_tx", wl_dfs_max_safe_tx, WLC_GET_VAR, WLC_SET_VAR,
	"Thresholds for tx traffic on main core. On crossing this threshold, scan core \n"
	"background DFS scan results may be discarded depending on dfs_txblank_check_mode.\n"
	"\t Default: Get the dfs max safe tx thresholds for non-adjacent (and adjacent) cases.\n"},
	{ "dfs_txblank_check_mode", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"Control if tx on main core must be considered at end of background CAC on scan core.\n"
	"\t Default: Get the dfs txblank check mode.\n"},
	{ "nd_ra_limit_intv", wl_nd_ra_limit_intv, WLC_GET_VAR, WLC_SET_VAR,
	"Get / Set IPv6 RA rate limit interval\n"
	"\tUsage(Set): nd_ra_limit_intv -t <type> -p <percentage(<100)> -m <fixed time>\n"
	"\t          : nd_ra_limit_intv -t 0 -p XX -m YYY\n"
	"\t          : nd_ra_limit_intv -t 1 -m ZZZ\n"
	"\tUsage(Get): nd_ra_limit_intv\n"
	"\t<type> 0: percentage of lifetime, 1: fixed time\n"},
	{"sim_PM", wl_sim_pm, WLC_GET_VAR, WLC_SET_VAR,
	"Set the fw into a simulated PM in idle associated mode\n"
	"\t[0 / 1] - Disable / Enable\n\t-c - Cycle time in TUs (max 10,000)\n\t-u Up time in TUs "
	"(max 1000)\n"},
	{ "wowl_arp_hostip", wl_hostip, WLC_GET_VAR, WLC_SET_VAR,
	"Add a host-ip address or display them"},
	{ "arp_hostip_clear", wl_var_void, -1, WLC_SET_VAR,
	"Clear all host-ip addresses"},
	{ "hc", wl_hc, WLC_GET_VAR, WLC_SET_VAR,
	"Health Check command group\n"
	"\tusage: wl hc tx/rx/scan <attribute> [value]\n"
	"\tGet or Set tx/rx/scan attribute values"},
	{"fbt_r0kh_id", wl_varstr, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set R0 Key Holder Idenitifer for an interface\n"
	"\tUsage: wl fbt_r0kh_id <string>\n"
	"String: Maximum 48 byte R0 Key Holder ID\n"},
	{"fbt_r1kh_id", wl_iov_mac, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set 802.11r R1 Key Holder Idenitifer for an interface\n"
	"\tUsage: wl fbt_r1kh_id <mac-address>\n"
	"MAC-address: xx:xx:xx:xx:xx:xx\n"},
	{"fbt_auth_resp", wl_varstr, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set fbt auth response for an interface\n"
	"\tUsage: wl fbt_auth_resp <string>\n"
	"String: Maximum 48 byte FBT auth response\n"},
	{ "icmp_hostip", wl_hostip, WLC_GET_VAR, WLC_SET_VAR,
	"Add a host-ip address or display current address set"},
	{ "icmp_hostipv6", wl_hostipv6_extended, WLC_GET_VAR, WLC_SET_VAR,
	"Set host-ipv6 address or display current addresses set for icmp offloads\n"
	"\tUse NULL ipv6 address - :: to flush all addresses\n"
	"Usage:\n\tSET: wl icmp_hostipv6 <ipv6addr1> <ipv6addr2>\n"
	"\tGET: wl icmp_hostipv6"},
	{"wake_timer", wl_wake_timer, WLC_GET_VAR, WLC_SET_VAR,
	"\tSend wake event to host periodically\n"
	"\tUsage : wl wake_timer <period> <limit>\n"
	"\t<period>: value in ms\n"
	"\t<limit>: value to specify num of events\n"
	"\t\t 0 : disable\n"
	"\t\t 0xFFFF : Infinite\n"
	"\t\t non-zero value: num of events\n"},
	{ "idauth", wl_idauth, WLC_GET_VAR, WLC_SET_VAR,
	"IDAUTH command group\n"
	"\tusage: wl idauth config -a 0/1 -b 1000 -c 3 -e 1000 -g 1000 -m 10\n"
	"\t\t-a: authentication offload enable/disable \n"
	"\t\t-c: EAPOL retry count\n"
	"\t\t-e: EAPOL retry Interval\n"
	"\t\t-g: GTK rotation interval\n"
	"\t\t-m: MIC fail count for blacklist\n"
	"\t\t-b: Blacklist age\n"
	"\tusage: wl idauth peer_info\n"
	"\tusage: wl idauth counters\n"},
	{ "utrace_capture", wl_utrace_capture, WLC_GET_VAR, -1,
	"Read the template RAM for ucode trace data.\n"},
	{"event_aggr", wl_eventaggr, WLC_GET_VAR, WLC_SET_VAR,
	"Configure event aggregation parameters\n"
	"Usage: wl event_aggr <flags> <bufsize> <flush_timeout> <num_events_flush>"},
	{ "bus:hmap", wl_hmap, WLC_GET_VAR, WLC_SET_VAR,
	"Enable / Disable HMAP\n" "Usage: wl hmap [0/1] ..."},
	{ "bus:hmaptest", wl_hmaptest, WLC_GET_VAR, WLC_SET_VAR,
	"Trigger hmaptest with parameters\n"
	"\tUsage: wl hmaptest -l 1400 -a 0/1/2 -w 0/1 -i 0/1\n"
	"\tUsage: wl hmaptest -s 1\n"
	"\t\t-l: length of transfer max 1920. default [512]\n"
	"\t\t-a: accesstype 0=ARM 1=M2M 2=D11 default [ARM]\n"
	"\t\t-w: write 1=write 0=read default[read]\n"
	"\t\t-i: invalid 1=invalid 0=valid default[valid]\n"
	"\t\t-s: stop HMAPTEST value [1]\n"},
	{ "struct_ver", wl_struct_ver, WLC_GET_VAR, -1,
	"returns structure versions" },
	{"atm_staperc", wl_atm_staperc, WLC_GET_VAR, WLC_SET_VAR,
	"<mac_addr> [num_percent]\n"},
	{ "hwareg", wl_hwareg, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set any HWA registers:\n"
	"\tUsage: wl hwareg <type> <offset> [value]\n"
	"\t       type: <top> <cmn> <txbm> <tx> <txdma> <txs> <rxbm> <rx> <cpl> <dma>\n"
	"\t       usage: wl hwareg top 0x104\n"
	"\t       usage: wl hwareg top 0x104 4\n"},
	{ "cfp_dump", wl_cfp_dump, WLC_GET_VAR, -1,
	"Dump stats for a particular STA\n"
	"wl cfp_dump <xx:xx:xx:xx:xx:xx>\n"},
	{ "amsdu_tid", wl_amsdu_tid, WLC_GET_VAR, WLC_SET_VAR,
	"enable/disable per-tid amsdu; usage: wl amsdu_tid <tid> [0/1]" },
	{ "traffic_thresh", wlc_traffic_thresh, WLC_GET_VAR, WLC_SET_VAR,
	"Set txfail thresholds for type of traffic in number of seconds\n"
	"\tUsage:  wl traffic_thresh ap/sta -c <type> -s <num_secs> "
	"-t <threshold>\n"
	"\t\twl traffic_thresh ap/sta -c <type> -e 1/0\n"
	"\t\twl traffic_thresh ap/sta -dump/-h\n"
	"\t\twl traffic_thresh mac -m <MACID> -c <type> -e 1/0\n"
	"\t\twl traffic_thresh mac -dump -m <MACID>\n"
	"\t\twl traffic_thresh intf 1/0 \n"
	"\t\twl traffic_thresh bss 1/0 \n"
	"\t	type:  traffic access category\n"
	"\t	MACID: specific mac whose threshold flags can be enabled or disabled\n"
	"\t		using this command will reset the corresponding MACs stats\n"
	"\t	num_secs: window for the fail threshold, default is 5 sec\n"
	"\t	threshold: packets failures, default is 0xffff\n"
	"\t	ap: mode to change ap configuration\n"
	"\t	sta: mode to change sta configurations\n"
	"\t	mac: mode to enable and disable feature per sta\n"
	"\t	intf: mode to enable and disable feature per interface\n"
	"\t	bss: mode to enable and disable feature per bsscfg\n"
	"\t	dump: will dump the whole config settings, doesnot have parameters\n"
	"\t	help: prints this message\n"},
	{ "sgi_tx", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"set Guard Interval(GI) for transmitting HT/VHT and HE frame:\n"
	"\t-1: AUTO\n"
	"\t 0: SGI OFF for HT/VHT frame, 2x LTF/1.6us GI for HE frame\n"
	"\t 1: SGI ON for HT/VHT frame, 2x LTF/0.8us GI for HE frame\n"
	"\t 2: SGI ON for HT/VHT frame, 1x LTF/0.8us GI for HE frame\n"
	"\t 3: SGI ON for HT/VHT frame, 2x LTF/0.8us GI for HE frame\n"
	"\t 4: SGI ON for HT/VHT frame, 2x LTF/1.6us GI for HE frame\n"
	"\t 5: SGI ON for HT/VHT frame, 4x LTF/3.2us GI for HE frame\n"},
#if defined(BCMDBG) || defined(BCMDBG_MU)
	{ "mutx_ac", wl_mutx_ac, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get per AC MUTX enable flag.\n"
	"Usage: wl -i <interface_name> mutx_ac 0, 1, vo|vi|bk|be [<mu_type>]\n"
	"\t<mu_type> : A subset of following types. Space seperated.\n"
	"\t\tvhtmu heofdma hemu\n"
	"\t\tTo disable all types of MU use \"none\" or \"0\" as mu_type.\n"
	"\t\tTo disable all types of MU for all ACs: mutx_ac 0.\n"
	"\t\tTo enable all types of MU for all ACs: mutx_ac 1.\n"
	"\t\tdefault: MUTX is enabled for all types and AC.\n"},
	{"macreq", wl_macreq_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"macreq   command request send to MAC\n"
	"         Usage: wl macreq [command] [options]\n"
	"Available commands and options:\n"
	"  wl macreq [-a|addmtxv] -l1,6 - Add link indices 4 and 6 (comma separated)\n"
	"  wl macreq [-a|addmtxv] -L1,6 - Add link indices 1 through 6 (comma separated)\n"
	"  wl macreq [-d|delmtxv] -l1,6 - Delete link indices 4 and 6 (comma separated)\n"
	"  wl macreq [-d|delmtxv] -L1,6 - Delete link indices 1 through 6 (comma separated)\n"
	"  wl macreq [-a|addmtxv] -t1,6 - Add link indices 4 and 6 (comma separated)\n"
	"  wl macreq [-a|addmtxv] -T1,6 - Add txv indices 1 through 6 (comma separated)\n"
	"  wl macreq [-d|delmtxv] -t1,6 - Delete txv indices 4 and 6 (comma separated)\n"
	"  wl macreq [-d|delmtxv] -T1,6 - Delete txv indices 1 through 6 (comma separated)\n"
	"  wl macreq [-d|delmtxv] - Delete all\n"
	"  wl macreq [-u|updlmem] -l1,6 - Update link indices 4 and 6 (comma separated)\n"
	"  wl macreq [-u|updlmem] -L1,6 - Update link indices 1 through 6 (comma separated)\n"
	"  wl macreq [-dl|delmem] -l1,6 - Delete linkmem indices 4 and 6 (comma separated)\n"
	"  wl macreq [-dl|delmem] -L1,6 - Delete linkmem indices 1 through 6 (comma separated)\n"
	"  wl macreq [updmtmr] <val> - value in msec\n"
	"  wl macreq help - print this help message\n"},
#endif /* defined(BCMDBG) || defined(BCMDBG_MU) */
#ifdef BCMDBG
	{ "wrr_weight", wl_wrr_handler, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get Global Weighted Roundrobin weights\n"
	"Usage: wl -i <interface_name> wrr_weight SU VHTMU HEOFDMA\n"
	"\tSU : Num SU AMPDUs per service round.\n"
	"\tVHTMU : Num VHTMU PPDUs per service round.\n"
	"\tHEOFDMA : Num HEOFDMA PPDUs perservice round\n"},
	{ "wrr_weight_max", wl_wrr_handler, WLC_GET_VAR, WLC_SET_VAR,
	"Set/Get Global Weighted Roundrobin maximum weights\n"
	"Usage: wl -i <interface_name> wrr_weight_max SU VHTMU HEOFDMA\n"
	"\tSU : Max SU AMPDUs per service round.\n"
	"\tVHTMU : Max VHTMU PPDUs per service round.\n"
	"\tHEOFDMA : Max HEOFDMA PPDUs perservice round\n"},
#endif // endif
	{"sr_config", wl_srcfg_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"sr_config   spatial Reuse control commands\n"
	"Usage: wl sr_config [command] [value] \n"
	"Available commands and options:\n"
	"wl sr_config options [0/1]: 0: Disable SR, 1: Enable SR\n"
	"wl sr_config nsrg_pdmin [value]: get/set NON SRG OBSS PDMIN value\n"
	"wl sr_config nsrg_pdmax [value]: get/set NON SRG OBSS PDMAX value\n"
	"wl sr_config srg_pdmin [value]: get/set SRG OBSS PDMIN value\n"
	"wl sr_config srg_pdmax [value]: get/set SRG OBSS PDMAX value\n"
	"wl sr_config txpwrref [value]: get/set tx power reference value\n"
	"wl sr_config nsrg_txpwrref0 [value]: get/set NON SRG tx power reference0 value\n"
	"wl sr_config srg_txpwrref0 [value]: get/set NON SRG tx power reference0 value\n"
	"wl sr_config srg_obsscolorbmp0 [value]: get/set SRG OBSS Color Bitmap0 value\n"
	"wl sr_config srg_obsscolorbmp1 [value]: get/set SRG OBSS Color Bitmap1 value\n"
	"wl sr_config srg_obsscolorbmp2 [value]: get/set SRG OBSS Color Bitmap2 value\n"
	"wl sr_config srg_obsscolorbmp3 [value]: get/set SRG OBSS Color Bitmap3 value\n"
	"  wl sr_config help - print this help message\n"},
	{"dtrace_ea", wl_iov_mac, WLC_GET_VAR, WLC_SET_VAR,
	"Get/set the dtrace mac address for whitelist filter"},
	{ "block_he", wl_varint, WLC_GET_VAR, WLC_SET_VAR,
	"block he clients\n"
	"\t0 - disable\n"
	"\t1 - enable blocking he clients based on he field in the request\n"
	"\t2 - enable blocking he clients based on he field in the request and block mac list\n" },
	{ NULL, NULL, 0, 0, NULL }
};

cmd_t wl_varcmd = {"var", wl_varint, -1, -1, "unrecognized name, type -h for help"};

#ifdef WC_TOOL
/* Include any commands for wc tool used for WMM
 * These need to be only the command names from port_cmds and wl_cmds array
 */
static const char *wc_cmds[] = {
	"ver", "cmds", "up", "down",
	"gmode", "listen", "wme", "wme_ac", "wme_apsd",
	"wme_apsd_sta", "wme_dp"
};
#else
static const char **wc_cmds = NULL;
#endif /* WC_TOOL */

static const char *dfs_cacstate_str[WL_DFS_CACSTATES] = {
	"IDLE",
	"PRE-ISM Channel Availability Check(CAC)",
	"In-Service Monitoring(ISM)",
	"Channel Switching Announcement(CSA)",
	"POST-ISM Channel Availability Check",
	"PRE-ISM Ouf Of Channels(OOC)",
	"POST-ISM Out Of Channels(OOC)"
};

#define MAX_MODULES	256
static cmd_t* module_cmds[MAX_MODULES];
static int module_count = 0;

/* register commands for a module */
void
wl_module_cmds_register(cmd_t *cmds)
{
	if (cmds == NULL)
		return;

	if (module_count < MAX_MODULES) {
		module_cmds[module_count] = cmds;
		module_count++;
	}
	else
		fprintf(stderr, "err - module count over %d\n", MAX_MODULES);
}

/* common function to find a command */
cmd_t *
wlu_find_cmd(char *name)
{
	int i;
	cmd_t *cmd = NULL;

	/* search cmd in modules */
	for (i = 0; i < module_count; i++) {

		/* search cmd in one cmd table */
		for (cmd = module_cmds[i]; cmd->name; cmd++) {
			/* stop if we find a matching name */
			if (!strcmp(cmd->name, name)) {
				break;
			}
		}

		/* if a match was found, break out of module loop */
		if (cmd->name != NULL) {
			break;
		}
	}

	return (cmd->name != NULL) ? cmd : NULL;
}

/* return global ioctl_version */
int
wl_get_ioctl_version(void)
{
	return ioctl_version;
}

/* return the address of bufstruct_wlu, global buf */
char *
wl_get_buf(void)
{
	return buf;
}

/* initialize stuff needed before processing the command */
void
wl_cmd_init(void)
{
	int_fmt = INT_FMT_DEC;
	g_wlc_idx = -1;
}

void
wlu_init(void)
{
	/* Init global variables at run-time, not as part of the declaration.
	 * This is required to support init/de-init of the driver. Initialization
	 * of globals as part of the declaration results in non-deterministic
	 * behaviour since the value of the globals may be different on the
	 * first time that the driver is initialized vs subsequent initializations.
	 */
	int_fmt = INT_FMT_DEC;
	g_wlc_idx = -1;
	batch_in_client = FALSE;
	init_cmd_batchingmode();

	/* register general wl commands */
	wl_module_cmds_register(wl_cmds);

	/* add wluc module init here */
	wluc_common_module_init();
	wluc_phy_module_init();
	wluc_wnm_module_init();
	wluc_cac_module_init();
	wluc_rmc_module_init();
	wluc_rrm_module_init();
	wluc_wowl_module_init();
	wluc_pkt_filter_module_init();
	wluc_mfp_module_init();
	wluc_ota_module_init();
	wluc_bssload_module_init();
	wluc_stf_module_init();
	wluc_offloads_module_init();
	wluc_tpc_module_init();
	wluc_toe_module_init();
	wluc_arpoe_module_init();
	wluc_keep_alive_module_init();
	wluc_ap_module_init();
	wluc_omn_module_init();
	wluc_ampdu_module_init();
	wluc_ampdu_cmn_module_init();
	wluc_bmac_module_init();
	wluc_ht_module_init();
	wluc_wds_module_init();
	wluc_keymgmt_module_init();
	wluc_scan_module_init();
	wluc_obss_module_init();
	wluc_prot_obss_module_init();
	wluc_lq_module_init();
	wluc_seq_cmds_module_init();
	wluc_btcx_module_init();
	wluc_led_module_init();
	wluc_interfere_module_init();
	wluc_ltecx_module_init();
	wluc_mbo_module_init();
#ifdef WLNDOE
	wluc_ndoe_module_init();
#endif /* WLNDOE */
#ifdef WLPFN
	wluc_pfn_module_init();
#endif /* WLPFN */
#ifdef BT_WIFI_HANDOVER
	wluc_tbow_module_init();
#endif /* BT_WIFI_HANDOVER */
#ifdef WLP2P
	wluc_p2p_module_init();
#endif /* WLP2PO */
#ifdef WLTDLS
	wluc_tdls_module_init();
#endif /* WLTDLS */
#ifdef WL_PROXDETECT
	wluc_proxd_module_init();
#endif /* WL_PROXDETECT */
#ifdef WLP2PO
	wluc_p2po_module_init();
#endif /* WLP2PO */
#ifdef WLANQPO
	wluc_anqpo_module_init();
#endif /* WLANQPO */
#ifdef WL_BTCDYN
	wluc_btcdyn_module_init();
#endif /* WL_BTCDYN */
#ifdef WL_MSCH
	wluc_msch_module_init();
#endif /* WL_MSCH */
#ifdef WLBDO
	wluc_bdo_module_init();
#endif /* WLBDO */
#ifdef WL_RANDMAC
	wluc_randmac_module_init();
#endif /* WL_RANDMAC */
#ifdef WLRSDB
	wluc_rsdb_module_init();
#endif // endif
	wluc_he_module_init();
	wluc_twt_module_init();
#ifdef WL_MBO
	wluc_mbo_module_init();
#endif /* WL_MBO */
#ifdef ECOUNTERS
	wluc_ecounters_module_init();
#endif // endif
#ifdef WL_OCE
	wluc_oce_module_init();
#endif /* WL_OCE */
#ifdef WL_ESP
	wluc_esp_module_init();
#endif /* WL_ESP */
#ifdef WLADPS
	wluc_adps_module_init();
#endif // endif
#ifdef WL_LEAKY_AP_STATS
	wluc_leakyapstats_module_init();
#endif /* WL_LEAKY_AP_STATS */
	wluc_otp_module_init();
#ifdef WLSLOTTED_BSS
	wluc_slot_bss_module_init();
#endif /* WLSLOTTED_BSS */
#ifdef WL_RPSNOA
	wluc_rpsnoa_module_init();
#endif // endif
#ifdef WLBAM
	wluc_bam_module_init();
#endif // endif
#ifdef WL_PWROPT
	wluc_pwropt_module_init();
#endif // endif
	wluc_heb_module_init();
#ifdef WL_BTL
	wluc_btl_module_init();
#endif	/* WL_BTL */
#ifdef WL_TVPM
	wluc_tvpm_module_init();
#endif	/* WL_TVPM */
#ifdef WL_TDMTX
	wluc_tdmtx_module_init();
#endif // endif
}

int wlc_ver_major(void *wl)
{
	int err;
	wl_wlc_version_t wlc_ver;

	memset(&wlc_ver, 0, sizeof(wlc_ver));
	err = wlu_iovar_get(wl, "wlc_ver", &wlc_ver, sizeof(wl_wlc_version_t));
	if (err != BCME_OK) {
		printf("Unable to get wlc_ver iovar %d\n", err);
		/* old firmware may not support it */
		return 0;
	}
	return wlc_ver.wlc_ver_major;
}

int
wl_check(void *wl)
{
	int ret;
	int val;

	if ((ret = wlu_get(wl, WLC_GET_MAGIC, &val, sizeof(int))) < 0)
		return ret;

	/* Detect if IOCTL swapping is necessary */
	if (val == (int)bcmswap32(WLC_IOCTL_MAGIC))
	{
		val = bcmswap32(val);
		g_swap = TRUE;
	}
	if (val != WLC_IOCTL_MAGIC)
		return -1;
	if ((ret = wlu_get(wl, WLC_GET_VERSION, &val, sizeof(int))) < 0)
		return ret;
	ioctl_version = dtoh32(val);
	if (ioctl_version != WLC_IOCTL_VERSION &&
	    ioctl_version != 1) {
		fprintf(stderr, "Version mismatch, please upgrade. Got %d, expected %d or 1\n",
		        ioctl_version, WLC_IOCTL_VERSION);
		return -1;
	}
	return 0;
}

/**
 * Parse/validate the command line on a call-by-call basis. There are 'common' options (such as
 * '-i') and 'per platform' options (meaning: options that are not valid for all platforms, such
 * as --interactive). When this function encounters a:
 * - 'common' option: it will parse it (= store result in either a function parameter or global
 *   variable) and return. Argv then points at the argument following the common option.
 *       -i -a -d -u -x
 *       --clientbatch --es -W --wlc
 *       -h --help
 * - 'platform' option: it will not parse it, and not advance argv.
 *
 * @param pargv    updated upon return if the first argument is an option, remains intact otherwise.
 * @param pifname  NON-NULL if interface name was encountered during this pass
 * @param phelp    TRUE if '-h' was supplied, *argv then points at the optional help subject.
 */
int
wl_option(char ***pargv, char **pifname, int *phelp)
{
	char *ifname = NULL;
	int help = FALSE;
	int status = CMD_OPT;
	char **argv = *pargv;

	if (*argv != NULL) {
		/* select different adapter */
		if (!strcmp(*argv, "-a") || !strcmp(*argv, "-i")) {
			char *opt = *argv++;
			ifname = *argv;
			if (!ifname) {
				fprintf(stderr,
					"error: expected interface name after option %s\n", opt);
				status = CMD_ERR;
			}
		}
		/* integer output format */
		else if (!strcmp(*argv, "-d"))
			int_fmt = INT_FMT_DEC;
		else if (!strcmp(*argv, "-u"))
			int_fmt = INT_FMT_UINT;
		else if (!strcmp(*argv, "-x"))
			int_fmt = INT_FMT_HEX;

		/* command usage */
		else if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help"))
			help = TRUE;

		else if (!strcmp(*argv, "--clientbatch")) {
			wl_seq_batch_in_client(TRUE);
		}
		/* To handle endian mis-matches between wl utility and wl driver */
		else if (!strcmp(*argv, "--es")) {
			g_swap = TRUE;
		}
		else if (!stricmp(*argv, "-W") || !strcmp(*argv, "--wlc")) {
			char *opt = *argv++;
			char *endptr = NULL;
			if (*argv) {
				g_wlc_idx = strtol(*argv, &endptr, 0);
			}
			if (endptr == *argv) {
				fprintf(stderr,
					"error: expected wlc integer index after option %s\n", opt);
				status = CMD_ERR;
				/* just to ensure that we trigger error */
				argv--;
			}
		}
		else {
			/* no 'common' option encountered */
			status = CMD_WL;
		}
		if (status == CMD_OPT) {
			/* advance argv past the common option */
			argv ++;
		}
	}

	*phelp = help;
	*pifname = ifname;
	*pargv = argv;

	return status;
} /* wl_option */

/* Dump out short list of commands */
static int
wl_list(void *wl, cmd_t *garb, char **argv)
{
	cmd_t *cmd;
	int nrows, i, len;
	char *list_buf;
	int letter, col, row, pad;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(garb);
	UNUSED_PARAMETER(argv);

	nrows = 0;
	for (i = 0; i < module_count; i++) {
		for (cmd = module_cmds[i]; cmd->name; cmd++)
		/* Check for wc_cmd */
		if (wc_cmd_check(cmd->name))
		    nrows++;
	}

	nrows /= 4;
	nrows++;

	len = nrows * 80 + 2;
	list_buf = malloc(len);
	if (list_buf == NULL) {
		fprintf(stderr, "Failed to allocate buffer of %d bytes\n", len);
		return BCME_NOMEM;
	}
	for (i = 0; i < len; i++)
		*(list_buf+i) = 0;

	row = col = 0;
	for (letter = 'a'; letter < 'z'; letter++) {
		for (i = 0; i < module_count; i++) {
			for (cmd = module_cmds[i]; cmd->name; cmd++) {
				/* Check for wc_cmd */
				if (!wc_cmd_check(cmd->name))
					continue;
				if (cmd->name[0] == letter || cmd->name[0] == letter - 0x20) {
					strcat(list_buf+row*80, cmd->name);
					pad = 18 * (col + 1) - strlen(list_buf+row*80);
					if (pad < 1)
						pad = 1;
					for (; pad; pad--)
						strcat(list_buf+row*80, " ");
					row++;
					if (row == nrows) {
						col++; row = 0;
					}
				}
			}
		}
	}
	for (row = 0; row < nrows; row++)
		printf("%s\n", list_buf+row*80);

	printf("\n");

	free(list_buf);
	return (0);
}

void
wl_cmds_usage(FILE *fid, cmd_t *port_cmds)
{
	cmd_t *port_cmd;
	cmd_t *cmd;
	int i;

	/* print usage of port commands */
	for (port_cmd = port_cmds; port_cmd && port_cmd->name; port_cmd++)
		/* Check for wc_cmd */
		if (wc_cmd_check(port_cmd->name))
			wl_cmd_usage(fid, port_cmd);

	/* print usage of common commands without port counterparts */
	for (i = 0; i < module_count; i++) {
		for (cmd = module_cmds[i]; cmd->name; cmd++) {
			/* search if port counterpart exists */
			for (port_cmd = port_cmds; port_cmd && port_cmd->name; port_cmd++)
				if (!strcmp(port_cmd->name, cmd->name))
					break;
			/* Also, check for this being a wc_cmd */
			if ((!port_cmd || !port_cmd->name) && (wc_cmd_check(cmd->name)))
				wl_cmd_usage(fid, cmd);
		}
	}
}

void
wl_usage(FILE *fid, cmd_t *port_cmds)
{
	fprintf(fid, "Usage: %s [-a|i <adapter>]"
		" [-h] [-d|u|x] [-w|--wlc <index>] <command> [arguments]\n", wlu_av0);

	fprintf(fid, "\n");
	fprintf(fid, "  -h        this message and command descriptions\n");
	fprintf(fid, "  -h [cmd]  command description for cmd\n");
	fprintf(fid, "  -a, -i    adapter name or number\n");
	fprintf(fid, "  -d        output format signed integer\n");
	fprintf(fid, "  -u        output format unsigned integer\n");
	fprintf(fid, "  -x        output format hexdecimal\n");
	fprintf(fid, "  -w <idx>  index of WLC for RSDB only\n");
	fprintf(fid, "\n");

	wl_cmds_usage(fid, port_cmds);
}

void
wl_printint(int val)
{
	switch (int_fmt) {
	case INT_FMT_UINT:
		printf("%u\n", val);
		break;
	case INT_FMT_HEX:
		printf("0x%x\n", val);
		break;
	case INT_FMT_DEC:
	default:
		printf("%d\n", val);
		break;
	}
}

int
wl_int(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr = NULL;

	if (!*++argv) {
		if (cmd->get == -1)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;

		val = dtoh32(val);
		wl_printint(val);
	} else {
		if (cmd->set == -1)
			return -1;
		if (!stricmp(*argv, "on"))
			val = 1;
		else if (!stricmp(*argv, "off"))
			val = 0;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}
		}

		val = htod32(val);
		ret = wlu_set(wl, cmd->set, &val, sizeof(int));
	}

	return ret;
}

int wl_buf(void *wl, cmd_t *cmd, char **argv)
{
	char* data;
	int err;
	int len;
	int i;

	strcpy(buf, cmd->name);

	/* set */
	if (argv[1]) {
		len = strlen(buf);
		data = argv[1];
		for (i = len + 1, len += 1 + strlen(data) / 2;
		    (i < len) && (i < (int)WLC_IOCTL_SMLEN); i ++) {
			char hex[] = "XX";
			hex[0] = *data++;
			hex[1] = *data++;
			buf[i] = (uint8)strtoul(hex, NULL, 16);
		}
		err = wlu_set(wl, WLC_SET_VAR, buf, i);
	}
	/* get */
	else if (!(err = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_SMLEN))) {
		len = dtoh32(*(int *)buf);
		if (len > WLC_IOCTL_SMLEN) {
			data = buf + sizeof(int);
			for (i = 0; i < len; i ++)
				printf("%02x", (uint8)(data[i]));
			printf("\n");
		} else {
			err = BCME_BUFTOOLONG;
		}
	}

	return err;
}

/* Return a new chanspec given a legacy chanspec
 * Returns INVCHANSPEC on error
 */
static chanspec_t
wl_chspec_from_legacy(chanspec_t legacy_chspec)
{
	chanspec_t chspec;

	/* get the channel number */
	chspec = LCHSPEC_CHANNEL(legacy_chspec);

	/* convert the band */
	if (LCHSPEC_IS2G(legacy_chspec)) {
		chspec |= WL_CHANSPEC_BAND_2G;
	} else {
		chspec |= WL_CHANSPEC_BAND_5G;
	}

	/* convert the bw and sideband */
	if (LCHSPEC_IS20(legacy_chspec)) {
		chspec |= WL_CHANSPEC_BW_20;
	} else {
		chspec |= WL_CHANSPEC_BW_40;
		if (LCHSPEC_CTL_SB(legacy_chspec) == WL_LCHANSPEC_CTL_SB_LOWER) {
			chspec |= WL_CHANSPEC_CTL_SB_L;
		} else {
			chspec |= WL_CHANSPEC_CTL_SB_U;
		}
	}

	if (wf_chspec_malformed(chspec)) {
		fprintf(stderr, "wl_chspec_from_legacy: output chanspec (0x%04X) malformed\n",
		        chspec);
		return INVCHANSPEC;
	}

	return chspec;
}

/* Return a legacy chanspec given a new chanspec
 * Returns INVCHANSPEC on error
 */
chanspec_t
wl_chspec_to_legacy(chanspec_t chspec)
{
	chanspec_t lchspec;

	if (wf_chspec_malformed(chspec)) {
		fprintf(stderr, "wl_chspec_to_legacy: input chanspec (0x%04X) malformed\n",
		        chspec);
		return INVCHANSPEC;
	}

	/* get the channel number */
	lchspec = CHSPEC_CHANNEL(chspec);

	/* convert the band */
	if (CHSPEC_IS2G(chspec)) {
		lchspec |= WL_LCHANSPEC_BAND_2G;
	} else {
		lchspec |= WL_LCHANSPEC_BAND_5G;
	}

	/* convert the bw and sideband */
	if (CHSPEC_IS20(chspec)) {
		lchspec |= WL_LCHANSPEC_BW_20;
		lchspec |= WL_LCHANSPEC_CTL_SB_NONE;
	} else if (CHSPEC_IS40(chspec)) {
		lchspec |= WL_LCHANSPEC_BW_40;
		if (CHSPEC_CTL_SB(chspec) == WL_CHANSPEC_CTL_SB_L) {
			lchspec |= WL_LCHANSPEC_CTL_SB_LOWER;
		} else {
			lchspec |= WL_LCHANSPEC_CTL_SB_UPPER;
		}
	} else {
		/* cannot express the bandwidth */
		char chanbuf[CHANSPEC_STR_LEN];
		fprintf(stderr,
		        "wl_chspec_to_legacy: unable to convert chanspec %s (0x%04X) "
		        "to pre-11ac format\n",
		        wf_chspec_ntoa(chspec, chanbuf), chspec);
		return INVCHANSPEC;
	}

	return lchspec;
}

/* given a chanspec value, do the endian and chanspec version conversion to
 * a chanspec_t value
 * Returns INVCHANSPEC on error
 */
chanspec_t
wl_chspec_to_driver(chanspec_t chanspec)
{
	if (ioctl_version == 1) {
		chanspec = wl_chspec_to_legacy(chanspec);
		if (chanspec == INVCHANSPEC) {
			return chanspec;
		}
	}
	chanspec = htodchanspec(chanspec);

	return chanspec;
}

/* given a chanspec value, do the endian and chanspec version conversion to
 * a chanspec_t value in a 32 bit integer
 * Returns INVCHANSPEC on error
 */
uint32
wl_chspec32_to_driver(chanspec_t chanspec)
{
	uint32 val;

	if (ioctl_version == 1) {
		chanspec = wl_chspec_to_legacy(chanspec);
		if (chanspec == INVCHANSPEC) {
			return chanspec;
		}
	}
	val = htod32((uint32)chanspec);

	return val;
}

/* given a chanspec value from the driver, do the endian and chanspec version conversion to
 * a chanspec_t value
 * Returns INVCHANSPEC on error
 */
chanspec_t
wl_chspec_from_driver(chanspec_t chanspec)
{
	chanspec = dtohchanspec(chanspec);
	if (ioctl_version == 1) {
		chanspec = wl_chspec_from_legacy(chanspec);
	}
	return chanspec;
}

/* given a chanspec value from the driver in a 32 bit integer, do the endian and
 * chanspec version conversion to a chanspec_t value
 * Returns INVCHANSPEC on error
 */
chanspec_t
wl_chspec32_from_driver(uint32 chanspec32)
{
	chanspec_t chanspec;

	chanspec = (chanspec_t)dtoh32(chanspec32);

	if (ioctl_version == 1) {
		chanspec = wl_chspec_from_legacy(chanspec);
	}
	return chanspec;
}

#ifdef CLMDOWNLOAD
/*
Generic interface for downloading required data onto the dongle
*/
int
download2dongle(void *wl, char *iovar, uint16 flag, uint16 dload_type,
	unsigned char *dload_buf, int len)
{
	struct wl_dload_data *dload_ptr = (struct wl_dload_data *)dload_buf;
	int err = 0;
	int dload_data_offset;

	dload_data_offset = OFFSETOF(wl_dload_data_t, data);
	dload_ptr->flag = (DLOAD_HANDLER_VER << DLOAD_FLAG_VER_SHIFT) | flag;
	dload_ptr->dload_type = dload_type;
	dload_ptr->len = htod32(len - dload_data_offset);
	dload_ptr->crc = 0;
	len = len + 8 - (len%8);

	err = wlu_iovar_setbuf(wl, iovar,
		dload_buf, len, buf, WLC_IOCTL_MEDLEN);
	return err;
}

int
dload_clm(void *wl, uint32 datalen, unsigned char *org_buf, int ds_id)
{
	int num_chunks, chunk_len, cumulative_len = 0;
	int size2alloc;
	unsigned char *new_buf;
	wl_clm_dload_info_t *clm_info_ptr;
	int err = 0, clm_info_offset, chunk_offset;

	clm_info_offset = OFFSETOF(wl_dload_data_t, data);
	chunk_offset = OFFSETOF(wl_clm_dload_info_t, data_chunk);

	num_chunks = datalen/MAX_CHUNK_LEN;
	if (datalen % MAX_CHUNK_LEN != 0)
		num_chunks++;
	size2alloc = clm_info_offset + chunk_offset + MAX_CHUNK_LEN;

	if ((new_buf = (unsigned char *)malloc(size2alloc)) != NULL) {
		memset(new_buf, 0, size2alloc);
		clm_info_ptr = (wl_clm_dload_info_t*)((uint8 *)new_buf + clm_info_offset);
		clm_info_ptr->num_chunks = num_chunks;
		clm_info_ptr->clm_total_len = datalen;
		clm_info_ptr->ds_id = ds_id;
		do {
			if (datalen >= MAX_CHUNK_LEN)
				chunk_len = MAX_CHUNK_LEN;
			else
				chunk_len = datalen;
			memset(new_buf + clm_info_offset + chunk_offset, 0,
				size2alloc - clm_info_offset - chunk_offset);
			clm_info_ptr->chunk_len = htod32(chunk_len);

			memcpy(&clm_info_ptr->data_chunk[0], org_buf + cumulative_len, chunk_len);
			clm_info_ptr->chunk_offset = cumulative_len;
			cumulative_len += chunk_len;

			err = download2dongle(wl, "generic_dload", 0, DL_TYPE_CLM, new_buf,
				chunk_len + clm_info_offset + chunk_offset);

			datalen = datalen - chunk_len;
		} while ((datalen > 0) && (err == 0));
		free(new_buf);
	} else {
		err = BCME_NOMEM;
	}

	return err;
}

int
dload_blob(void *wl, char *iovar, unsigned char *org_buf, uint32 datalen)
{
	int chunk_len, cumulative_len = 0;
	int size2alloc;
	unsigned char *new_buf;
	int err = 0, data_offset;
	uint16 dl_flag = DL_BEGIN;

	data_offset = OFFSETOF(wl_dload_data_t, data);
	size2alloc = data_offset + MAX_CHUNK_LEN;

	if ((new_buf = (unsigned char *)malloc(size2alloc)) != NULL) {
		memset(new_buf, 0, size2alloc);

		do {
			if (datalen >= MAX_CHUNK_LEN)
				chunk_len = MAX_CHUNK_LEN;
			else
				chunk_len = datalen;

			memcpy(new_buf + data_offset, org_buf + cumulative_len, chunk_len);
			cumulative_len += chunk_len;

			if (datalen - chunk_len == 0)
				dl_flag |= DL_END;

			err = download2dongle(wl, iovar, dl_flag, DL_TYPE_CLM,
				new_buf, data_offset + chunk_len);
			dl_flag &= ~DL_BEGIN;

			datalen = datalen - chunk_len;
		} while ((datalen > 0) && (err == 0));

		free(new_buf);
	} else {
		err = BCME_NOMEM;
	}

	return err;
}

int
dload_clm_blob(void *wl, unsigned char *org_buf, uint32 datalen)
{
	int chunk_len, cumulative_len = 0;
	int size2alloc;
	unsigned char *new_buf;
	int err = 0, data_offset;
	uint16 dl_flag = DL_BEGIN;

	data_offset = OFFSETOF(wl_dload_data_t, data);
	size2alloc = data_offset + MAX_CHUNK_LEN;

	if ((new_buf = (unsigned char *)malloc(size2alloc)) != NULL) {
		memset(new_buf, 0, size2alloc);

		do {
			if (datalen >= MAX_CHUNK_LEN)
				chunk_len = MAX_CHUNK_LEN;
			else
				chunk_len = datalen;

			memcpy(new_buf + data_offset, org_buf + cumulative_len, chunk_len);
			cumulative_len += chunk_len;

			if (datalen - chunk_len == 0)
				dl_flag |= DL_END;

			err = download2dongle(wl, "clmload", dl_flag, DL_TYPE_CLM,
				new_buf, data_offset + chunk_len);
			dl_flag &= ~DL_BEGIN;

			datalen = datalen - chunk_len;
		} while ((datalen > 0) && (err == 0));

		free(new_buf);
	} else {
		err = BCME_NOMEM;
	}

	return err;
}

#define CLM_INPUT_FILE_MIN_LEN 32
int
process_clm_data(void *wl, char *clmfn, int ds_id)
{
	int ret = 0;

	FILE *fp = NULL;

	unsigned int clm_filelen;
	unsigned long status = 0;
	unsigned char *new_buf = NULL;
	uint32 clm_data_len;
	unsigned char *new_ptr;
	int filelen = 0;
	const char clm_magic_string[] = {'C', 'L', 'M', ' ', 'D', 'A', 'T', 'A'};
	const char blob_magic_string[] = {'B', 'L', 'O', 'B'};

	/* Open the clm download file */
	if (!(fp = fopen(clmfn, "rb"))) {
		fprintf(stderr, "unable to open input file %s\n", clmfn);
		ret = BCME_BADARG;
		goto error;
	}

	/* fstat is a linux/unix, not a stdio function. This is replaced everywhere with function
	 * fsize() which calls fseek(fp, 0L, SEEK_END), ftell(fp), fseek(fp, 0L, SEEK_SET) sequence
	 * to determine the file size.
	 */

	if ((filelen = fsize(fp)) < 0) {
		fprintf(stderr, "fseek or ftell on input file %s return error %s\n",
			clmfn, strerror(errno));
		ret = BCME_ERROR;
		goto error;
	}
	clm_filelen = (uint)filelen;

	if (clm_filelen == 0) {
		fprintf(stderr, "input file %s is empty (i.e. zero length)\n", clmfn);
		ret = BCME_ERROR;
		goto error;
	}

	if ((new_buf = malloc(clm_filelen)) == NULL) {
		fprintf(stderr, "unable to allocate %u bytes based on input file size!\n",
			clm_filelen);
		ret = BCME_NOMEM;
		goto error;
	}

	/* We can read a CLM binary file or CLM blob file.  The CLM binary file has
	 * been obsoleted in favor of the "blob" format that allows additional data
	 * to included in a modular/independent way.  The blob format is downloaded
	 * via the new 'clmload' iovar.  The binary format uses the legacy and now obsolete
	 * 'generic_dload' iovar.
	 *
	 * Newer driver builds no longer support the generic_dload iovar but this wl
	 * command utility will support either format/iovar combination based on the
	 * download file's magic string for some transition time.
	 */

	status = fread(new_buf, 1, clm_filelen, fp);

	/* Basic sanity check on size. Make sure there is enough for any magic string plus
	 * a little more for good measure.
	 */
	if (status < CLM_INPUT_FILE_MIN_LEN) {
		fprintf(stderr, "size of input file %s is less than %d bytes."
			"  This can't be a CLM file!\n", clmfn, CLM_INPUT_FILE_MIN_LEN);
		ret = BCME_ERROR;
		goto error;
	} else if (status != clm_filelen) {
		fprintf(stderr, "read of input file %s wasn't good based on fstat size %u\n",
			clmfn, clm_filelen);
		ret = BCME_ERROR;
		goto error;
	}

	/* CLM blob or binary format file? */
	if (memcmp(new_buf, blob_magic_string, sizeof(blob_magic_string)) == 0) {
		/* CLM blob file? They start with magic string 'BLOB' */
		printf("Downloading CLM blob format file %s\n", clmfn);
		ret = dload_clm_blob(wl, new_buf, clm_filelen);
	} else if (memcmp(new_buf, clm_magic_string, sizeof(clm_magic_string)) == 0) {
		/* pure CLM binary file?  CLM binary files start with 'CLM DATA' */
		clm_data_len = clm_filelen;
		new_ptr = new_buf;
		printf("Downloading legacy, obsolete CLM binary format file %s as a %s CLM\n",
			clmfn, ds_id ? "incremental":"base");
		ret = dload_clm(wl, clm_data_len, new_ptr, ds_id);
	} else {
		fprintf(stderr, "input file is missing CLM binary or CLM blob magic string\n");
		ret = -1;
		goto error;
	}
error:
	if (new_buf)
		free(new_buf);
	if (fp)
		fclose(fp);

	return ret;
}

#define TXCAP_INPUT_FILE_MIN_LEN 32

int
process_txcap_data(void *wl, char *txcapfn)
{
	int ret = 0;

	FILE *fp = NULL;

	unsigned int txcap_filelen;
	unsigned long status = 0;
	unsigned char *new_buf = NULL;
	int filelen = 0;

	/* Open the txcap download file */
	if (!(fp = fopen(txcapfn, "rb"))) {
		fprintf(stderr, "unable to open input file %s\n", txcapfn);
		ret = -EINVAL;
		goto error;
	}

	/* fstat is a linux/unix, not a stdio function. This is replaced everywhere with function
	 * fsize() which calls fseek(fp, 0L, SEEK_END), ftell(fp), fseek(fp, 0L, SEEK_SET) sequence
	 * to determine the file size.
	 */

	if ((filelen = fsize(fp)) < 0) {
		fprintf(stderr, "fseek or ftell on input file %s return error %s\n",
			txcapfn, strerror(errno));
		ret = -EINVAL;
		goto error;
	}
	txcap_filelen = (uint)filelen;

	if (txcap_filelen == 0) {
		fprintf(stderr, "input file %s is empty (i.e. zero length)\n", txcapfn);
		ret = -EINVAL;
		goto error;
	}

	if ((new_buf = malloc(txcap_filelen)) == NULL) {
		fprintf(stderr, "unable to allocate %u bytes based on input file size!\n",
			txcap_filelen);
		ret = -ENOMEM;
		goto error;
	}

	status = fread(new_buf, 1, txcap_filelen, fp);

	if (status != txcap_filelen) {
		fprintf(stderr, "read of input file %s wasn't good based on fstat size %u\n",
			txcapfn, txcap_filelen);
		ret = -EINVAL;
		goto error;
	}

	/* Basic sanity check on size. Make sure there is enough for any magic string plus
	 * a little more for good measure.
	 */
	if (status < TXCAP_INPUT_FILE_MIN_LEN) {
		fprintf(stderr, "size of input file %s is less than %d bytes."
			"  This can't be a txcap file!\n", txcapfn, TXCAP_INPUT_FILE_MIN_LEN);
		ret = -EINVAL;
		goto error;
	} else if (status != txcap_filelen) {
		fprintf(stderr, "read of input file %s wasn't good based on fstat size %u\n",
			txcapfn, txcap_filelen);
		ret = -EINVAL;
		goto error;
	}

	/* txcap package? */
	if (memcmp(new_buf, blob_magic_string, sizeof(blob_magic_string)) == 0) {
		/* MSF packaged file? They start with magic string 'BLOB' */
		printf("Downloading txcap package format file %s\n", txcapfn);
		ret = dload_blob(wl, "txcapload", new_buf, txcap_filelen);
	} else {
		fprintf(stderr, "input file is missing txcap package magic string\n");
		ret = -1;
		goto error;
	}

error:
	if (new_buf)
		free(new_buf);
	if (fp)
		fclose(fp);

	return ret;
}

static int
wl_clmload(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char *arg1;
	char *arg2;
	char* fname;

	BCM_REFERENCE(cmd);

	/* argv is pointing at the clmload command name to start */
	if ((arg1 = *++argv) == NULL) {
		fprintf(stderr, "too few arguments (none)\n");
		return BCME_USAGE_ERROR;
	} else if ((arg2 = *++argv) == NULL) {
		/* one argument - use it, arg1, as the file name */
		fname = arg1;
	} else if ((*++argv) == NULL) {
		/* two arguments - use the last, arg2, as the filename
		 * while ignoring the first argument.  We no longer
		 * support downloading anything but a base CLM,
		 */
		fname = arg2;
	} else {
		fprintf(stderr, "too mang arguments (3 or more)\n");
		return BCME_USAGE_ERROR;
	}

	ret = process_clm_data(wl, fname, 0);

	return ret;
}
static int
wl_txcapload(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char *arg1;
	char* fname;

	BCM_REFERENCE(cmd);

	/* argv is pointing at the clmload command name to start */
	if ((arg1 = *++argv) == NULL) {
		/* too few arguments (none) */
		return -1;
	} else if (*++argv == NULL) {
		/* one argument - use it, arg1, as the file name */
		fname = arg1;
	} else {
		/* too mang arguments (2 or more) */
		return -1;
	}

	ret = process_txcap_data(wl, fname);

	return ret;
}

static int
wl_txcapctl(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	void *ptr = NULL;
	wl_txpwrcap_ctl_t *txcap_ctl_ptr;
	wl_txpwrcap_ctl_t txcap_ctl;
	int i;

	if (!(argv[1])) { /* Get */
		if ((err = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;
		txcap_ctl_ptr =  ptr;
		printf("%d %d %d %d %d\n",
			txcap_ctl_ptr->ctl[0],
			txcap_ctl_ptr->ctl[1],
			txcap_ctl_ptr->ctl[2],
			txcap_ctl_ptr->ctl[3],
			txcap_ctl_ptr->ctl[4]);
	} else { /* Set */
		for (i = 0; i < 5; i++) {
			if (*++argv) {
				txcap_ctl.ctl[i] = strtol(*argv, NULL, 0);
			} else {
				return BCME_USAGE_ERROR;
			}
		}
		if (argv[1]) {
			return BCME_USAGE_ERROR;
		}
		else {
			(void)txcap_ctl;
			txcap_ctl.version = TXPWRCAPCTL_VERSION;
			if ((err = wlu_var_setbuf(wl, cmd->name, &txcap_ctl, sizeof(txcap_ctl)))
				< 0)
				return BCME_ERROR;
		}
	}
	return err;
}

static int
wl_txcapdump(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	void *ptr = NULL;
	uint8 txcap_dump_version;
	int i;

	if (!(argv[1])) { /* Get */
		if ((err = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, &ptr)) < 0)
			 return err;
		txcap_dump_version = *((uint8 *)ptr);
		if (txcap_dump_version == 2) {
			wl_txpwrcap_dump_t *txcap_dump_ptr =  ptr;
			printf("current country %c%c, channel(control) %d\n",
				txcap_dump_ptr->current_country[0],
				txcap_dump_ptr->current_country[1],
				txcap_dump_ptr->current_channel);
			printf("low/high cap config: ");
			for (i = 0; i < TXPWRCAP_NUM_SUBBANDS; i++) {
				printf(" %d", txcap_dump_ptr->config[i]);
			}
			printf("\n");
			printf("low/high cap state:  ");
			for (i = 0; i < TXPWRCAP_NUM_SUBBANDS; i++) {
				printf(" %d", txcap_dump_ptr->state[i]);
			}
			printf("\n");
			printf("high cap state enabled %d\n",
				txcap_dump_ptr->high_cap_state_enabled);
			printf("wci2 cell status last %d\n",
				txcap_dump_ptr->wci2_cell_status_last);
			if (!txcap_dump_ptr->download_present) {
				printf("txcap download not present\n");
			} else {
				printf("subbands %d, antennas %d, antennas per core",
					txcap_dump_ptr->num_subbands, txcap_dump_ptr->num_antennas);
				for (i = 0; i < TXPWRCAP_MAX_NUM_CORES; i++) {
					printf(" %d", txcap_dump_ptr->num_antennas_per_core[i]);
				}
				printf(", cc groups %d\n", txcap_dump_ptr->num_cc_groups);
				printf("current cc group info index %d\n",
					txcap_dump_ptr->current_country_cc_group_info_index);
				printf(" tx caps (decimal qdbm, per subband, per antenna,"
				       " e.g. sb0_a0, sb0_a1, sb1_a0, sb1_a1, ...)\n");
				printf("  low cap:  ");
				for (i = 0;
					i < txcap_dump_ptr->num_subbands *
						txcap_dump_ptr->num_antennas;
					i++) {
					printf("%4d ", txcap_dump_ptr->low_cap[i]);
				}
				printf("\n");
				printf("  high cap: ");
				for (i = 0;
					i < txcap_dump_ptr->num_subbands *
						txcap_dump_ptr->num_antennas;
					i++) {
					printf("%4d ", txcap_dump_ptr->high_cap[i]);
				}
				printf("\n");
			}
		} else if (txcap_dump_version == 3) {
			wl_txpwrcap_dump_v3_t *txcap_dump_ptr =  ptr;
			printf("current country %c%c, channel(control) %d\n",
				txcap_dump_ptr->current_country[0],
				txcap_dump_ptr->current_country[1],
				txcap_dump_ptr->current_channel);
			printf("low/high cap config: ");
			for (i = 0; i < TXPWRCAP_NUM_SUBBANDS; i++) {
				printf(" %d", txcap_dump_ptr->config[i]);
			}
			printf("\n");
			printf("low/high cap state:  ");
			for (i = 0; i < TXPWRCAP_NUM_SUBBANDS; i++) {
				printf(" %d", txcap_dump_ptr->state[i]);
			}
			printf("\n");
			printf("high cap state enabled %d\n",
				txcap_dump_ptr->high_cap_state_enabled);
			printf("wci2 cell status last %d\n",
				txcap_dump_ptr->wci2_cell_status_last);
			if (!txcap_dump_ptr->download_present) {
				printf("txcap download not present\n");
			} else {
				printf("subbands %d, antennas %d, antennas per core",
					txcap_dump_ptr->num_subbands, txcap_dump_ptr->num_antennas);
				for (i = 0; i < TXPWRCAP_MAX_NUM_CORES; i++) {
					printf(" %d", txcap_dump_ptr->num_antennas_per_core[i]);
				}
				printf(", cc groups %d\n", txcap_dump_ptr->num_cc_groups);
				printf("current cc group info index %d\n",
					txcap_dump_ptr->current_country_cc_group_info_index);
				printf(" tx caps (decimal qdbm, per subband, per antenna,"
				       " e.g. sb0_a0, sb0_a1, sb1_a0, sb1_a1, ...)\n");
				if (txcap_dump_ptr->cap_states_per_cc_group == 2) {
					printf("  low  cap: ");
					for (i = 0;
						i < txcap_dump_ptr->num_subbands *
							txcap_dump_ptr->num_antennas;
						i++) {
						printf("%4d ",
							txcap_dump_ptr->host_low_wci2_low_cap[i]);
					}
					printf("\n");
					printf("  high cap: ");
					for (i = 0;
						i < txcap_dump_ptr->num_subbands *
							txcap_dump_ptr->num_antennas;
						i++) {
						printf("%4d ",
							txcap_dump_ptr->host_low_wci2_high_cap[i]);
					}
					printf("\n");
				} else {
					printf("  host low  wci2 low  cap: ");
					for (i = 0;
						i < txcap_dump_ptr->num_subbands *
							txcap_dump_ptr->num_antennas;
						i++) {
						printf("%4d ",
							txcap_dump_ptr->host_low_wci2_low_cap[i]);
					}
					printf("\n");
					printf("  host low  wci2 high cap: ");
					for (i = 0;
						i < txcap_dump_ptr->num_subbands *
							txcap_dump_ptr->num_antennas;
						i++) {
						printf("%4d ",
							txcap_dump_ptr->host_low_wci2_high_cap[i]);
					}
					printf("\n");
					printf("  host high wci2 low  cap: ");
					for (i = 0;
						i < txcap_dump_ptr->num_subbands *
							txcap_dump_ptr->num_antennas;
						i++) {
						printf("%4d ",
							txcap_dump_ptr->host_high_wci2_low_cap[i]);
					}
					printf("\n");
					printf("  host high wci2 high cap: ");
					for (i = 0;
						i < txcap_dump_ptr->num_subbands *
							txcap_dump_ptr->num_antennas;
						i++) {
						printf("%4d ",
							txcap_dump_ptr->host_high_wci2_high_cap[i]);
					}
					printf("\n");
				}
			}
		} else {
			err = BCME_USAGE_ERROR;
		}
	} else { /* Set */
		err = BCME_USAGE_ERROR;
	}
	return err;
}

#endif /* CLMDOWNLOAD */

#define CAL_INPUT_FILE_MIN_LEN 32

int
process_cal_data(void *wl, char *calfn)
{
	int ret = 0;
	FILE *fp = NULL;
	unsigned int cal_filelen;
	unsigned long status = 0;
	unsigned char *new_buf = NULL;
	int filelen = 0;

	/* Open the CAL download file */
	if (!(fp = fopen(calfn, "rb"))) {
		fprintf(stderr, "unable to open input file %s\n", calfn);
		ret = -EINVAL;
		goto error;
	}

	/* fstat is a linux/unix, not a stdio function. This is replaced everywhere with function
	 * fsize() which calls fseek(fp, 0L, SEEK_END), ftell(fp), fseek(fp, 0L, SEEK_SET) sequence
	 * to determine the file size.
	 */

	if ((filelen = fsize(fp)) < 0) {
		fprintf(stderr, "fseek or ftell on input file %s return error %s\n",
			calfn, strerror(errno));
		ret = -EINVAL;
		goto error;
	}
	cal_filelen = (uint)filelen;

	if (cal_filelen == 0) {
		fprintf(stderr, "input file %s is empty (i.e. zero length)\n", calfn);
		ret = -EINVAL;
		goto error;
	}

	if ((new_buf = malloc(cal_filelen)) == NULL) {
		fprintf(stderr, "unable to allocate %u bytes based on input file size!\n",
			cal_filelen);
		ret = -ENOMEM;
		goto error;
	}

	status = fread(new_buf, 1, cal_filelen, fp);

	if (status != cal_filelen) {
		fprintf(stderr, "read of input file %s wasn't good based on fstat size %u\n",
			calfn, cal_filelen);
		ret = -EINVAL;
		goto error;
	}

	/* Basic sanity check on size. Make sure there is enough for any magic string plus
	 * a little more for good measure.
	 */
	if (status < CAL_INPUT_FILE_MIN_LEN) {
		fprintf(stderr, "size of input file %s is less than %d bytes."
			"  This can't be a cal file!\n", calfn, CAL_INPUT_FILE_MIN_LEN);
		ret = -EINVAL;
		goto error;
	} else if (status != cal_filelen) {
		fprintf(stderr, "read of input file %s wasn't good based on fstat size %u\n",
			calfn, cal_filelen);
		ret = -EINVAL;
		goto error;
	}

	/* Calibration package? */
	if (memcmp(new_buf, blob_magic_string, sizeof(blob_magic_string)) == 0) {
		/* MSF packaged file? They start with magic string 'BLOB' */
		printf("Downloading calibration package format file %s\n", calfn);
		ret = dload_blob(wl, "calload", new_buf, cal_filelen);
	} else {
		fprintf(stderr, "input file is missing calibration package magic string\n");
		ret = -1;
		goto error;
	}

error:
	if (new_buf)
		free(new_buf);
	if (fp)
		fclose(fp);

	return ret;
}

static int
wl_calload(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char* fname;

	BCM_REFERENCE(cmd);

	/* argv is pointing at the calload command name to start */
	if ((fname = *++argv) == NULL) {
		/* too few arguments (none) */
		return -EINVAL;
	}

	ret = process_cal_data(wl, fname);

	return ret;
}

int
process_cal_dump(void *wl, char *fname_txcal, char *fname_rxcal)
{
	int ret = 0;
	unsigned long status = 0;
	FILE *fblobp = NULL;
	void *ptr = NULL;
	uint32 dump_sz_total;
	uint16 dump_sz_txcal, dump_sz_rxcal;

	/* Read back TX calibration information */
	if ((ret = wlu_var_getbuf_med(wl, "caldump", NULL, 0, &ptr)) < 0)
		goto error;

	dump_sz_total = dtoh32(*(int *)ptr);
	printf("caldump: total dump size %d bytes\n", dump_sz_total);
	if (dump_sz_total > (WLC_IOCTL_MEDLEN - 4)) {
		fprintf(stderr, "caldump: total dump size too large\n");
		ret = -EINVAL;
		goto error;
	}
	ptr = (void *)((uint8 *)ptr + sizeof(dump_sz_total));

	/* The TxCal data starts first. Its size is indicated in first two bytes
	 * (excluding size field itself - an uint16 field)
	 */
	dump_sz_txcal = dtoh16(*(uint16 *)ptr) + sizeof(dump_sz_txcal);
	printf("\tTxCal dump size %d bytes\n", dump_sz_txcal);
	if (dump_sz_total < (uint32)dump_sz_txcal) {
		fprintf(stderr, "caldump: total dump size too small for TxCal\n");
		ret = -EINVAL;
		goto error;
	}

	/* Open the txcal bin file */
	if (!(fblobp = fopen(fname_txcal, "wb"))) {
		fprintf(stderr, "unable to open TxCal output file %s\n", fname_txcal);
		perror(fname_txcal);
		ret = -EINVAL;
		goto error;
	}

	status = (unsigned long)fwrite((uint8 *)ptr, 1, dump_sz_txcal, fblobp);
	if (status != dump_sz_txcal) {
		fprintf(stderr, "unable to complete TxCal output (write %lu out of %u bytes)\n",
			status, dump_sz_txcal);
		ret = -EINVAL;
		goto error;
	}
	ptr = (void *)((uint8 *)ptr + dump_sz_txcal);
	fclose(fblobp);
	fblobp = NULL;

	/* The RxCal data starts after TxCal. Its size is indicated in first two bytes
	 * (but here, its size is derived from dump_sz_total and dump_sz_txcal))
	 */

	/* For backward comatible where only TxCal file is given, we just dump TxCal
	 * and return. If the RXCal file name is "NULL", we just return without any error
	 */
	if (fname_rxcal == NULL) {
		fprintf(stderr, "can't find RxCal output file\n");
		return ret;
	}
	dump_sz_rxcal = (uint16)(dump_sz_total - (uint32)dump_sz_txcal);
	printf("\tRxCal dump size %d bytes\n", dump_sz_rxcal);
	if (dump_sz_rxcal == 0) {
		fprintf(stderr, "no RxCal data available\n");
		ret = -EINVAL;
		goto error;
	}

	/* Open the rxcal bin file */
	if (!(fblobp = fopen(fname_rxcal, "wb"))) {
		fprintf(stderr, "unable to open RxCal output file %s\n", fname_rxcal);
		perror(fname_rxcal);
		ret = -EINVAL;
		goto error;
	}

	status = (unsigned long)fwrite((uint8 *)ptr, 1, dump_sz_rxcal, fblobp);
	if (status != dump_sz_rxcal) {
		fprintf(stderr, "unable to complete RxCal output (write %lu out of %u bytes)\n",
			status, dump_sz_rxcal);
		ret = -EINVAL;
		goto error;
	}

error:
	if (fblobp)
		fclose(fblobp);
	return ret;
}

static int
wl_caldump(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char *fname_txcal, *fname_rxcal;

	BCM_REFERENCE(cmd);

	/* argv is pointing at the command name to start */
	if ((fname_txcal = *++argv) == NULL) {
		/* too few arguments (none) */
		return -EINVAL;
	}
	printf("caldump: dump TxCal to file %s\n", fname_txcal);

	if ((fname_rxcal = *++argv) != NULL)
		printf("caldump: dump RxCal to file %s\n", fname_rxcal);

	ret = process_cal_dump(wl, fname_txcal, fname_rxcal);

	return ret;
}

/* Set or Get a BSS Configuration idexed integer, with an optional config index argument:
 *	wl xxx [-C N]|[--cfg=N] <integer>
 *
 * Option:
 *	-C N
 *	--cfg==N
 *	--config==N
 *	--configuration==N
 *		specify the config index N
 * If cfg index is not given, the "bsscfg:" iovar prefix is not used.
 * If cfg index is not given, and the cmd->set value is not WLC_SET_VAR, then
 * this funtion will support a legacy IOCTL wl_int() call for the cmd->set and
 * cmd->get ioctl commands.
 */
int
wl_bsscfg_int(void *wl, cmd_t *cmd, char **argv)
{
	char *endptr = NULL;
	char *val_name;
	int bsscfg_idx = 0;
	int val = 0;
	int consumed;
	int ret;

	val_name = *argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, val_name, &bsscfg_idx, &consumed)) != 0)
		return ret;

	/* handle a bsscfg int with a legacy ioctl */
	if (consumed == 0 && cmd->set != WLC_SET_VAR) {
		/* back up to the orig command and run as an ioctl int */
		argv--;
		return wl_int(wl, cmd, argv);
	}

	argv += consumed;

	if (!*argv) {
		/* This is a GET */
		if (cmd->get == -1)
			return -1;

		if (consumed == 0)
			ret = wlu_iovar_getint(wl, val_name, &val);
		else
			ret = wl_bssiovar_getint(wl, val_name, bsscfg_idx, &val);

		if (ret < 0)
			return ret;

		wl_printint(val);
	} else {
		/* This is a SET */
		if (cmd->set == -1)
			return -1;

		if (!stricmp(*argv, "on"))
			val = 1;
		else if (!stricmp(*argv, "off"))
			val = 0;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}
		}

		if (consumed == 0)
			ret = wlu_iovar_setint(wl, val_name, val);
		else
			ret = wl_bssiovar_setint(wl, val_name, bsscfg_idx, val);
	}

	return ret;
}

static int
wl_overlay(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int argc;
	char *endptr = NULL;
	void *ptr = NULL;
	int param[3];

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc < 1 || argc > 3) {
		printf("required args: virt_addr phy_addr size\n");
		return BCME_USAGE_ERROR;
	}

	param[0] = strtol(argv[1], &endptr, 0);
	if (*endptr != '\0' || (param[0] & ~0x003FFE00) != 0) {
		printf("Invaild virtual address: %s\n", argv[1]);
		return BCME_BADARG;
	}

	if (argc == 1) {
		if ((ret = wlu_var_getbuf(wl, cmd->name, param, sizeof(int), &ptr)) >= 0) {
			wl_hexdump((uchar *)ptr, 512);
		}
		return (ret);
	}

	param[1] = strtol(argv[2], &endptr, 0);
	if (*endptr != '\0' || (param[1] & ~0x003FFE00) != 0) {
		printf("Invaild physical Address: %s\n", argv[2]);
		return BCME_BADARG;
	}

	if (argc == 3) {
		param[2] = strtol(argv[3], &endptr, 0);
		if (*endptr != '\0' || param[2] < 0 || param[2] > 7) {
			printf("Invaild size: %s\n", argv[3]);
			return BCME_BADARG;
		}
	} else {
		param[2] = 0;
	}

	printf("Setting virtual Address 0x%x to physical Address 0x%x, size is %d\n",
		param[0], param[1], param[2]);
	ret = wlu_var_setbuf(wl, cmd->name, param, sizeof(param));

	return (ret);
}

static int
wl_macreg(void *wl, cmd_t *cmd, char **argv)
{
	int reg;
	int size;
	uint32 val;
	int ret;
	char *endptr = NULL;
	rw_reg_t rwt;
	uint argc;

	val = 0;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* required arg: reg offset */
	if (argc < 1)
		return BCME_USAGE_ERROR;

	reg = strtol(argv[0], &endptr, 0);
	if (*endptr != '\0')
		return BCME_USAGE_ERROR;

	/* required arg: reg size */
	if (argc < 2)
		return BCME_USAGE_ERROR;

	size = strtol(argv[1], &endptr, 0);
	if (*endptr != '\0')
		return BCME_USAGE_ERROR;

	rwt.band = WLC_BAND_AUTO;

	/* Third arg: new value or band */
	if (argc >= 3) {
		if (!stricmp(argv[2], "a"))
			rwt.band = WLC_BAND_5G;
		else if (!stricmp(argv[2], "b"))
			rwt.band = WLC_BAND_2G;
		else {
			val = strtoul(argv[2], &endptr, 0);
			if (*endptr != '\0')
				return BCME_USAGE_ERROR;
		}

	}

	/* Fourth arg: band */
	if (argc >= 4) {
		if (!stricmp(argv[3], "a"))
			rwt.band = WLC_BAND_5G;
		else if (!stricmp(argv[3], "b"))
			rwt.band = WLC_BAND_2G;
		else
			return BCME_USAGE_ERROR;
	}

	if ((argc == 2) || ((argc == 3) && (rwt.band != WLC_BAND_AUTO))) {
		rwt.band = htod32(rwt.band);
		rwt.byteoff = htod32(reg);
		rwt.size = htod32(size);
		if ((ret = wlu_get(wl, cmd->get, &rwt, sizeof(rw_reg_t))) < 0)
			return (ret);
		printf("0x%04x\n", dtoh32(rwt.val));
	}
	else {
		rwt.band = htod32(rwt.band);
		rwt.byteoff = htod32(reg);
		rwt.size = htod32(size);
		rwt.val = htod32(val);
		ret = wlu_set(wl, cmd->set, &rwt, sizeof(rw_reg_t));
	}

	return (ret);
}

static int
wl_macregx(void *wl, cmd_t *cmd, char **argv)
{
	int reg;
	int size = 2;
	int err;
	char *p;
	rw_reg_t rwt;

	/* eat command name */
	argv++;

	/* required arg: reg offset */
	if ((p = *argv) == NULL) {
		err = BCME_USAGE_ERROR;
		goto exit;
	}
	reg = strtol(p, NULL, 0);
	argv++;

	/* this is shared with macreg1 which is for psmr1 */
	if (strncmp(cmd->name, "macreg", 6) == 0) {
		/* required arg: size of the register */
		if ((p = *argv) == NULL) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		size = strtol(p, NULL, 0);
		argv++;
	}

	rwt.byteoff = htod32(reg);
	rwt.size = htod32(size);
	rwt.band = WLC_BAND_AUTO;

	if ((p = *argv) == NULL) {
		uint32 val;
		/* GET cmd */
		if ((err = wlu_iovar_getbuf(wl, cmd->name, &rwt,
			sizeof(rwt), buf, WLC_IOCTL_SMLEN) < 0)) {
			goto exit;
		}
		val = *((uint32 *)buf);

		if (size == 4) {
			printf("0x%08x\n", dtoh32(val));
		} else {
			printf("0x%04x\n", (uint16)dtoh32(val));
		}
	} else {
		/* SET cmd */
		/* required arg: set value */
		if ((p = *argv) == NULL) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		rwt.val = htod32(strtol(p, NULL, 0));
		argv++;

		if ((err = wlu_iovar_set(wl, cmd->name, &rwt, sizeof(rwt))) < 0) {
			printf("Error setting variable %s\n", cmd->name);
			return err;
		}
	}
exit:
	return (err);
}

/*
 * get or get a band specific variable
 * the band can be a/b/all or omitted. "all"(set only)
 * means all supported bands. blank means current band
 */
static int
wl_band_elm(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct {
		int val;
		int band;
	} x;
	char *endptr = NULL;
	uint argc;

	/* eat command name */
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	x.val = 0;
	x.band = WLC_BAND_AUTO;

	/* First arg: value or band */
	if (argc >= 1) {
		if (!stricmp(argv[0], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[0], "b"))
			x.band = WLC_BAND_2G;
		else if (!stricmp(argv[0], "all"))
			x.band = WLC_BAND_ALL;
		else {
			x.val = strtol(argv[0], &endptr, 0);
			if (*endptr != '\0')
				return BCME_USAGE_ERROR;
		}
	}

	/* Second arg: band */
	if (argc >= 2) {
		if (!stricmp(argv[1], "a"))
			x.band = WLC_BAND_5G;
		else if (!stricmp(argv[1], "b"))
			x.band = WLC_BAND_2G;
		else if (!stricmp(argv[1], "all"))
			x.band = WLC_BAND_ALL;
		else
			return BCME_USAGE_ERROR;
	}

	/* issue the get or set ioctl */
	if ((argc == 0) || ((argc == 1) && (x.band != WLC_BAND_AUTO))) {
		if (x.band == WLC_BAND_ALL) {
			printf("band option \"all\" is for set only, not get\n");
			return BCME_USAGE_ERROR;
		}

		x.band = htod32(x.band);
		if ((ret = wlu_get(wl, cmd->get, &x, sizeof(x))) < 0)
			return (ret);

		printf("%s is 0x%04x(%d)\n", cmd->name, (uint16)(dtoh32(x.val)), dtoh32(x.val));
	} else {
		x.band = htod32(x.band);
		x.val = htod32(x.val);
		ret = wlu_set(wl, cmd->set, &x, sizeof(x));
	}

	return (ret);
}

/* Disassoc */
static int
wl_disassoc(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr ea;

	if (cmd->set < 0)
		return -1;

	if (!*++argv) {
		 return wlu_set(wl, cmd->set, NULL, 0);
	} else {
		if (!wl_ether_atoe(*argv, &ea))
			return BCME_USAGE_ERROR;
		return wlu_set(wl, cmd->set, &ea, ETHER_ADDR_LEN);
	}
}

static void
wl_txq_prec_dump(wl_iov_pktq_log_t* iov_pktq_log, bool hide_unknown, bool is_aqm)
{
#define PREC_DUMPV(v4, v5)  ((iov_pktq_log->version == 4) ? (v4) : (v5))

#define v4hstubL            "prec:   rqstd,  stored,selfsave,   saved,fulldrop, dropped," \
	                    "sacrficd, retried, rtsfail,rtrydrop, psretry,supprssd,   " \
	                    "acked,utlisatn,q length,Data Mbits/s,Phy Mbits/s,Rate Mbits/s"

#define v4hstubS            "prec:   rqstd,  stored, dropped, retried, rtsfail,rtrydrop, " \
	                    "psretry,   acked,utlisatn,q length,Data Mbits/s,Phy Mbits/s"

#define v4fstubL            "  %02u: %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, " \
	                    "%7u, %7u, %7u, %7u, %8.2f,   %8.2f,    %8.2f"

#define v4fstubS            "  %02u: %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, %7u, " \
	                    "%8.2f,   %8.2f"

#define v4fstubL_aqm        "  %02u: %7u, %7u, %7u, %7u, %7u, %7u, %7u,       -,       -, " \
	                    "%7u, %7u, %7u, %7u, %7u, %7u, %8.2f,          -,           -"

#define v4fstubS_aqm        "  %02u: %7u, %7u, %7u,       -,       -, %7u, %7u, %7u, %7u, " \
	                    "%7u, %8.2f,          -"

	const char* v4headingsL = v4hstubL" (+v%d.)\n";
	const char* v4headingsS = v4hstubS" (+v%d.)\n";
	const char* v5headingsL = v4hstubL", %%air, %%effcy  (v%d)\n";
	const char* v5headingsS = v4hstubS", %%air, %%effcy  (v%d)\n";

	const char* v4formL =     v4fstubL"\n";
	const char* v4formS =     v4fstubS"\n";
	const char* v4formL_aqm = v4fstubL_aqm"\n";
	const char* v4formS_aqm = v4fstubS_aqm"\n";

	const char* v5formL =     v4fstubL",  %6.1f,  %5.1f\n";
	const char* v5formS =     v4fstubS",  %6.1f,  %5.1f\n";
	const char* v5formL_aqm = v4fstubL_aqm",       -,      -\n";
	const char* v5formS_aqm = v4fstubS_aqm",       -,      -\n";

	char*  headings;
	uint8  index;
	uint8  prec;
	uint32 prec_mask = 0;
	char   marker[4] = "[X]";
	pktq_log_format_v05_t* logv05 = NULL;
	pktq_log_format_v04_t* logv04 = NULL;

	if (iov_pktq_log->version == 0x04) {
		logv04 = &iov_pktq_log->pktq_log.v04;
	}
	else if (iov_pktq_log->version == 0x05) {
		logv05 = &iov_pktq_log->pktq_log.v05;
	}
	else {
		fprintf(stderr, "Unknown/unsupported binary format (%x)\n",
		        iov_pktq_log->version);
		return;
	}

	headings = PREC_DUMPV(&logv04->headings[0], &logv05->headings[0]);

	for (index = 0; index < (uint8)iov_pktq_log->params.num_addrs; index++) {

		char* heading_start;
		char* heading_end;
		uint32 num_prec = 0;

		prec_mask = PREC_DUMPV(logv04->counter_info[index],
		                       logv05->counter_info[index]);
		num_prec = PREC_DUMPV(logv04->num_prec[index],
		                      logv05->num_prec[index]);

		/* test for 'unknown' data; unknown means either that
		 * the queue is invalid or else that the logging
		 * is not active at all.
		 */
		if (((prec_mask & 0xFFFF) == 0) && hide_unknown) {
			continue;
		}

		if ((num_prec == 0) && hide_unknown) {
			continue;
		}

		/* search for string marker - the marker is of the form
		   "[<index>]" where index is a single ascii numeral
		*/
		marker[1] = '0' + index;
		heading_start = strstr(headings, marker);

		/* The driver may pass back an optional character
		 * string for additional info
		 */
		if (heading_start != NULL) {

			heading_start += strlen(marker);
			heading_end = strstr(heading_start, marker);

			if (heading_end == NULL) {
				heading_end = heading_start + strlen(heading_start);
			}
			while (heading_start < heading_end) {
				fputc(*heading_start++, stdout);
			}
		}

		/* Note that this is zero if the data is invalid */
		if (!num_prec) {
			fprintf(stdout, "Parameter %c:%s not valid\n",
			        iov_pktq_log->params.addr_type[index] != 0 ?
			        iov_pktq_log->params.addr_type[index] & 0x7F : ' ',
			        wl_ether_etoa(&iov_pktq_log->params.ea[index]));
			continue;
		}

		/* check for short form or long form (top bit set) */
		fprintf(stdout,
		        iov_pktq_log->params.addr_type[index] & 0x80 ?
		        PREC_DUMPV(v4headingsL, v5headingsL) :
		        PREC_DUMPV(v4headingsS, v5headingsS),
		        iov_pktq_log->version);

		for (prec = 0; prec < num_prec; prec++) {
			float tput = 0.0;
			float txrate_succ = 0.0;
			float txrate_main = 0.0;
			pktq_log_counters_v05_t counters;
			uint32 try_count = 0;
			float airuse = 0.0;
			float efficiency = 0.0;

			if (!(prec_mask & (1 << prec))) {
				continue;
			}

			if (iov_pktq_log->version == 5) {
				counters = logv05->counters[index][prec];
			}
			else {
				/* the following is a trick - it is possible because
				 * V4 and V5 are both common except that V5 has extra fields
				 * at the end
				*/
				memcpy(&counters, &logv04->counters[index][prec],
				       sizeof(pktq_log_counters_v04_t));
				counters.airtime = 0;
			}

			txrate_succ = (float)((float)counters.txrate_succ * 0.5);

			if (counters.time_delta != 0) {
				/* convert bytes to bits */
				tput = (float)counters.throughput;
				tput *= 8.0;

				if (counters.airtime) {
					efficiency = (float)(100.0 * tput /
							(float)counters.airtime);
				}

				/* converts to rate of bits per us,
				   because time_delta is in micro-seconds
				*/
				tput /= (float)counters.time_delta;

				/* Calculate % airtime */
				airuse = (float)((float)counters.airtime * 100.0 /
						(float)counters.time_delta);
			}

			if (!(is_aqm && (prec & 1))) {
				uint32 acked = counters.acked;

				try_count = counters.acked + counters.retry;

				if (is_aqm && (prec_mask & (1 << (prec + 1)))) {
					pktq_log_counters_v05_t hi;

					if (iov_pktq_log->version == 5) {
						hi = logv05->counters[index][prec + 1];
					}
					else {
						/* the following is a trick - it is possible
						 * fields V4 and V5 are both common except
						 * that V5 has extra fields at the end
						 */
						memcpy(&hi, &logv04->counters[index][prec + 1],
						       sizeof(pktq_log_counters_v04_t));
					}

					acked += hi.acked;
					try_count += hi.acked + hi.retry;

					if (counters.airtime) {
						float t = (float)hi.throughput;
						t /= (float)counters.airtime;
						efficiency += (float)(100.0 * 8.0 * t);
					}
				}
				if (acked) {
					txrate_succ /= (float) acked;

					if (counters.txrate_succ) {
						efficiency /= txrate_succ;
					}
					else {
						efficiency = 0;
					}
				}
				else {
					txrate_succ = 0;
					efficiency = 0;
				}
			}

			if (try_count) {
				txrate_main = (float)((float)counters.txrate_main * 0.5);
				txrate_main /= (float)try_count;
			}

			if (iov_pktq_log->params.addr_type[index] & 0x80) {
				/* long form */
				if (is_aqm && (prec & 1)) {
					/* aqm format for hi-prec */
					fprintf(stdout, PREC_DUMPV(v4formL_aqm, v5formL_aqm),
					        prec,
					        counters.requested,
					        counters.stored,
					        counters.selfsaved,
					        counters.saved,
					        counters.full_dropped,
					        counters.dropped,
					        counters.sacrificed,
					        counters.retry_drop,
					        counters.ps_retry,
					        counters.suppress,
					        counters.acked,
					        counters.max_used,
					        counters.queue_capacity,
					        tput);
				}
				else {
					fprintf(stdout, PREC_DUMPV(v4formL, v5formL),
					        prec,
					        counters.requested,
					        counters.stored,
					        counters.selfsaved,
					        counters.saved,
					        counters.full_dropped,
					        counters.dropped,
					        counters.sacrificed,
					        counters.retry,
					        counters.rtsfail,
					        counters.retry_drop,
					        counters.ps_retry,
					        counters.suppress,
					        counters.acked,
					        counters.max_used,
					        counters.queue_capacity,
					        tput, txrate_succ,
					        txrate_main,
					        airuse, efficiency);
				}
			}
			else {
				/* short form */
				if (is_aqm && (prec & 1)) {
					/* aqm format for hi-prec */
					fprintf(stdout, PREC_DUMPV(v4formS_aqm, v5formS_aqm),
					        prec,
					        counters.requested,
					        counters.stored,
					        counters.dropped,
					        counters.retry_drop,
					        counters.ps_retry,
					        counters.acked,
					        counters.max_used,
					        counters.queue_capacity,
					        tput);
				}
				else {
					fprintf(stdout, PREC_DUMPV(v4formS, v5formS),
					        prec,
					        counters.requested,
					        counters.stored,
					        counters.dropped,
					        counters.retry,
					        counters.rtsfail,
					        counters.retry_drop,
					        counters.ps_retry,
					        counters.acked,
					        counters.max_used,
					        counters.queue_capacity,
					        tput, txrate_succ,
					        airuse, efficiency);
				}
			}
		}
		fputs("\n", stdout);

		if (iov_pktq_log->version == 5 &&
		       (logv05->pspretend_time_delta[index] != (uint32)-1)) {

			fprintf(stdout, "Total time in ps pretend state is %d milliseconds\n\n",
			        (logv05->pspretend_time_delta[index] + 500)/1000);
		}
	}
}

static int
wl_scb_bs_data(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 flag_bits = 0;
	int argn;
	enum { DISPLAY_COOKED, DISPLAY_RAW } display_mode = DISPLAY_COOKED;
	iov_bs_data_struct_t *data = (iov_bs_data_struct_t *)buf;
	char sep = ' ';
	bool skip_idle = FALSE;
	float total_throughput = 0.0;
	bool display_overall = TRUE;
	uint32 time_delta = 0;
	uint32 cumul_airtime = 0;

	UNUSED_PARAMETER(cmd);	/* cmd->name should match argv[0] ? */

	if (!argv[0]) {
		fprintf(stderr, "%s: argv[0] missing\n", __FUNCTION__);
		return BCME_BADARG;
	}

	for (argn = 1; argv[argn]; ++argn) {
		if (!strcmp(argv[argn], "-noreset")) {	/* do not reset counters after reading */
			flag_bits |= SCB_BS_DATA_FLAG_NO_RESET;
		} else
		if (!strcmp(argv[argn], "-raw")) {	/* Display raw counters */
			display_mode = DISPLAY_RAW;
		} else
		if (!strcmp(argv[argn], "-tab")) {	/* Tab separator */
			sep = '\t';
		} else
		if (!strcmp(argv[argn], "-comma")) {	/* Comma separator */
			sep = ',';
		} else
		if (!strcmp(argv[argn], "-noidle")) {	/* Skip idle stations */
			skip_idle = TRUE;
		} else
		if (!strcmp(argv[argn], "-nooverall")) {	/* Skip overall summary  */
			display_overall = FALSE;
		} else
		if (!strcmp(argv[argn], "-help") || !strcmp(argv[argn], "-h")) {
			/* Display usage, do not complain about unknown option. */
			return BCME_USAGE_ERROR;
		} else {
			fprintf(stderr, "%s: unknown option: %s\n", argv[0], argv[argn]);
			return BCME_USAGE_ERROR;
		}
	}

	flag_bits = htod32(flag_bits);
	err = wlu_iovar_getbuf(wl, argv[0], &flag_bits, sizeof(flag_bits), buf, WLC_IOCTL_MAXLEN);
	if (err) {
		return (err);
	}

	data->structure_version = dtoh16(data->structure_version);
	if (data->structure_version != SCB_BS_DATA_STRUCT_VERSION) {
		fprintf(stderr, "wlu / wl driver mismatch, expect V%d format, got %d.\n",
			SCB_BS_DATA_STRUCT_VERSION, data->structure_version);
		return BCME_IOCTL_ERROR;
	}

	data->structure_count = dtoh16(data->structure_count);
	if (data->structure_count == 0) {
		printf("No stations are currently associated.\n");
		return BCME_OK;
	}

	/* Display Column headers - mac address always, then, depending on display mode */

	printf("%17s%c", "Station Address", sep);
	switch (display_mode) {
	case DISPLAY_RAW:
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"retry_drop", "rtsfail", "retry", "txrate_main",
			"txrate_succ", "acked", "throughput", "time_delta", "airtime");
		break;
	case DISPLAY_COOKED:
		printf("%10s%c%10s%c%10s%c%10s%c%10s\n", "PHY Mbps", sep, "Data Mbps", sep,
			"Air Use", sep, "Data Use", sep, "Retries");
		break;
	}

	/* Convert returned counters to host byte order, and sum up total throughput */

	for (argn = 0; argn < data->structure_count; ++argn) {

		iov_bs_data_record_t *rec;
		iov_bs_data_counters_t *ctr;
		float data_rate;

		rec = &data->structure_record[argn];
		ctr = &rec->station_counters;

#define DEVICE_TO_HOST(xyzzy) ctr->xyzzy = dtoh32(ctr->xyzzy)
		DEVICE_TO_HOST(retry_drop);
		DEVICE_TO_HOST(rtsfail);
		DEVICE_TO_HOST(retry);
		DEVICE_TO_HOST(txrate_main);
		DEVICE_TO_HOST(txrate_succ);
		DEVICE_TO_HOST(acked);
		DEVICE_TO_HOST(throughput);
		DEVICE_TO_HOST(time_delta);
		DEVICE_TO_HOST(airtime);
#undef DEVICE_TO_HOST

		/* Save delta_time for total_airtime calculation */
		if (ctr->time_delta)
			time_delta = ctr->time_delta;

		/* Calculate data rate in bits per second, rather than bytes per second */
		data_rate = (ctr->time_delta) ?
			(float)((float)ctr->throughput * 8.0 / (float)ctr->time_delta) : 0.0;

		total_throughput += data_rate;
		cumul_airtime += ctr->airtime;
	}

	for (argn = 0; argn < data->structure_count; ++argn) {
		iov_bs_data_record_t *rec;
		iov_bs_data_counters_t *ctr;
		rec = &data->structure_record[argn];
		ctr = &rec->station_counters;

		if (skip_idle && (ctr->acked == 0)) continue;

		printf("%17s%c", wl_ether_etoa(&rec->station_address), sep);
		switch (display_mode) {
		case DISPLAY_RAW:
			printf("%9d %9d %9d %9d %9d %9d %9d %9d %9d\n",
				ctr->retry_drop, ctr->rtsfail, ctr->retry,
				ctr->txrate_main, ctr->txrate_succ, ctr->acked,
				ctr->throughput, ctr->time_delta, ctr->airtime);
			break;
		case DISPLAY_COOKED:
			{
			float data_rate;
			float phy_rate;
			float use, air, rtr;

			/* Calculate PHY rate */
			phy_rate = (ctr->acked) ?
				(float)((float)ctr->txrate_succ * 0.5 / (float)ctr->acked) : 0.0;

			/* Calculate Data rate */
			data_rate = (ctr->time_delta) ?
				(float)((float)ctr->throughput * 8.0 / (float)ctr->time_delta) :
				0.0;

			/* Calculate use percentage amongst throughput from all stations */
			use = (total_throughput) ? (float)(data_rate / total_throughput * 100.0) :
				0.0;

			/* Calculate % airtime */
			air = (ctr->time_delta) ? (float)((float)ctr->airtime * 100.0 /
			          (float) ctr->time_delta) : 0.0;

			/* Calculate retry percentage */
			rtr = (ctr->acked) ? (float)((float)ctr->retry / (float)ctr->acked * 100) :
				0.0;

			printf("%10.1f%c%10.1f%c%9.1f%%%c%9.1f%%%c%9.1f%%\n",
				phy_rate, sep, data_rate, sep, air, sep, use, sep, rtr);
			}
			break;
		}
	}

	/* Add total summary */
	if (display_overall && (display_mode == DISPLAY_COOKED)) {
		float total_air = 0;

		/* Calculate % total airtime */
		if (time_delta) {
			total_air = (float)((float)cumul_airtime * 100.0 /
				(float) time_delta);
		}

		printf("        (overall)%c", sep);
		printf("%10.1s%c%10.1f%c%9.1f%%%c%9.1s%c%9.1s\n",
			"-", sep, total_throughput, sep, total_air, sep, "-", sep, "-");

	}

	return BCME_OK;
}

static void
wl_scb_rx_report_printf(char *s, float f, uint64 rxmpdu)
{
	if (rxmpdu == 0)
		sprintf(s, "%5.5s", "-");
	else if (10*(int)f == (int)(f*10.0))
		sprintf(s, "%5.0f", f);
	else
		sprintf(s, "%5.1f", f);
}

static int
wl_scb_rx_report(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int32 flag_bits = 0;
	int argn, tid;
	enum { DISPLAY_COOKED, DISPLAY_RAW } display_mode = DISPLAY_COOKED;
	iov_rx_report_struct_t *data = (iov_rx_report_struct_t *)buf;
	char sep = ' ';
	bool skip_idle = FALSE;
	bool mac_only = FALSE;
	bool display_overall = TRUE;
	struct ether_addr ea;
	iov_rx_report_counters_t allc;
	iov_rx_report_record_t *rec;
	iov_rx_report_counters_t *ctr;
	char st_nss[10];
	char st_mcs[10];
	char st_bw[10];

	UNUSED_PARAMETER(cmd);	/* cmd->name should match argv[0] ? */

	if (!argv[0]) {
		fprintf(stderr, "%s: argv[0] missing\n", __FUNCTION__);
		return BCME_BADARG;
	}

	for (argn = 1; argv[argn]; ++argn) {
		if (!strcmp(argv[argn], "-noreset")) {	/* do not reset counters after reading */
			flag_bits |= SCB_RX_REPORT_DATA_FLAG_NO_RESET;
		} else
		if (!strcmp(argv[argn], "-raw")) {	/* Display raw counters */
			display_mode = DISPLAY_RAW;
		} else
		if (!strcmp(argv[argn], "-tab")) {	/* Tab separator */
			sep = '\t';
		} else
		if (!strcmp(argv[argn], "-comma")) {	/* Comma separator */
			sep = ',';
		} else
		if (!strcmp(argv[argn], "-noidle")) {	/* Skip idle stations */
			skip_idle = TRUE;
		} else
		if (!strcmp(argv[argn], "-nooverall")) {	/* Skip overall summary  */
			display_overall = FALSE;
		} else
		if (!strcmp(argv[argn], "-sta")) {	/* Only display given station */
			if (!wl_ether_atoe(argv[++argn], &ea))
				fprintf(stderr, "-sta: invalid mac addr: %s\n", argv[argn]);
			else {
				mac_only = TRUE;
			}
		} else
		if (!strcmp(argv[argn], "-help") || !strcmp(argv[argn], "-h")) {
			/* Display usage, do not complain about unknown option. */
			return BCME_USAGE_ERROR;
		} else {
			fprintf(stderr, "%s: unknown option: %s\n", argv[0], argv[argn]);
			return BCME_USAGE_ERROR;
		}
	}

	flag_bits = htod32(flag_bits);
	err = wlu_iovar_getbuf(wl, argv[0], &flag_bits, sizeof(flag_bits), buf, WLC_IOCTL_MAXLEN);
	if (err) {
		return (err);
	}

	data->structure_version = dtoh16(data->structure_version);
	if (data->structure_version != SCB_RX_REPORT_DATA_STRUCT_VERSION) {
		fprintf(stderr, "wlu / wl driver mismatch, expect V%d format, got %d.\n",
			SCB_RX_REPORT_DATA_STRUCT_VERSION, data->structure_version);
		return BCME_IOCTL_ERROR;
	}

	data->structure_count = dtoh16(data->structure_count);
	if (data->structure_count == 0) {
		printf("No stations are currently associated.\n");
		return BCME_OK;
	}

	/* Display Column headers - mac address always, then, depending on display mode */

	printf("%25s%c", "Station Address (rssi)", sep);
	switch (display_mode) {
	case DISPLAY_RAW:
		printf("%8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s"
			"%8.8s %8.8s %8.8s %8.8s %8.8s %8.8s\n",
			"tid", "rxampdu", "rxmpdu", "rxbyte", "rxphyrate",
			"rxholes", "dup", "oow", "rtries", "rxbw",
			"rxmcs", "rxnss", "dtime");
		break;
	case DISPLAY_COOKED:
		printf("%4s%c%7s%c%8s%c"
			"%10s%c%10s%c%5s%c%5s%c"
			"%5s%c%5s%c%5s%c"
			"%5s%c%5s%c%6s\n",
			"tid", sep, "ampdu", sep, "mpdu", sep,
			"Data Mbps", sep, "PHY Mbps", sep, "bw", sep, "mcs", sep,
			"Nss", sep, "oow", sep, "holes", sep,
			"dup", sep, "rtries", sep, "air");
		break;
	}

	/* Convert returned counters to host byte order, and sum up all stations */
	memset(&allc, 0, sizeof(allc));
	for (argn = 0; argn < data->structure_count; ++argn) {
		rec = &data->structure_record[argn];
		for (tid = 0; tid < NUMPRIO; tid++) {
			iov_rx_report_counters_t *ctr;
			//float data_rate;

			ctr = &rec->station_counters[tid];

			/* Only consider valid tid */
			if (!(rec->station_flags & (1<<tid))) continue;

#define DEVICE_TO_HOST64(xyzzy) ctr->xyzzy = dtoh64(ctr->xyzzy); allc.xyzzy += ctr->xyzzy;
#define DEVICE_TO_HOST32(xyzzy) ctr->xyzzy = dtoh32(ctr->xyzzy); allc.xyzzy += ctr->xyzzy;
			DEVICE_TO_HOST64(rxbyte);
			DEVICE_TO_HOST64(rxphyrate);
			DEVICE_TO_HOST32(rxampdu);
			DEVICE_TO_HOST32(rxmpdu);
			DEVICE_TO_HOST32(rxholes);
			DEVICE_TO_HOST32(rxdup);
			DEVICE_TO_HOST32(rxoow);
			DEVICE_TO_HOST32(rxretried);
			DEVICE_TO_HOST32(rxbw);
			DEVICE_TO_HOST32(rxnss);
			DEVICE_TO_HOST32(rxmcs);
#undef DEVICE_TO_HOST32
#undef DEVICE_TO_HOST64

		}
	}

	for (argn = 0; argn < data->structure_count; ++argn) {
		iov_rx_report_record_t *rec;
		bool first_time_header = TRUE;

		rec = &data->structure_record[argn];

		if (mac_only && memcmp(&rec->station_address, &ea, ETHER_ADDR_LEN)) continue;

		for (tid = 0; tid < NUMPRIO; tid++) {
			ctr = &rec->station_counters[tid];

			/* Only consider valid tid */
			if (!(rec->station_flags & (1<<tid))) continue;

			/* Optionnal skip tid with no activity */
			if (skip_idle && (ctr->rxmpdu == 0)) continue;

			if (first_time_header) {
				printf("%17s (%ddBm)%c",
					wl_ether_etoa(&rec->station_address),
					rec->rssi, sep);
				first_time_header = FALSE;
			} else {
				printf("                          %c", sep);
			}

			switch (display_mode) {
			case DISPLAY_RAW:
				printf("%7u %8u %8u %8llu "
					"%8llu %8u %8u "
					"%7u %8u %8u "
					"%8u %8u %8u\n",
					tid, ctr->rxampdu, ctr->rxmpdu, ctr->rxbyte,
					ctr->rxphyrate, ctr->rxholes, ctr->rxdup,
					ctr->rxoow, ctr->rxretried, ctr->rxbw,
					ctr->rxmcs, ctr->rxnss, rec->time_delta);
				break;
			case DISPLAY_COOKED:
				{
				float bw = 0, nss = 0, speed = 0;
				float phyrate = 0, mcs = 0, ratio = 0;

				if (ctr->rxmpdu) {
					bw = ctr->rxbw/ctr->rxmpdu;
					phyrate = (ctr->rxphyrate/1000.0)/ctr->rxmpdu;
					mcs = ctr->rxmcs*1.0/ctr->rxmpdu;
					nss = ctr->rxnss*1.0/ctr->rxmpdu;

					if (rec->time_delta) {
						speed = (ctr->rxbyte*8.0) / rec->time_delta;
						ratio = speed * 100.0 / phyrate;
					}
				}

				wl_scb_rx_report_printf(st_nss, nss, ctr->rxmpdu);
				wl_scb_rx_report_printf(st_mcs, mcs, ctr->rxmpdu);
				wl_scb_rx_report_printf(st_bw, bw, ctr->rxmpdu);

				printf("%3d%c%7d%c%8d%c"
					"%10.1f%c%10.1f%c%s%c%s%c%s%c"
					"%5d%c%5d%c%5d%c%5d%c"
					"%6.0f%%\n",
					tid, sep, ctr->rxampdu, sep, ctr->rxmpdu, sep,
					speed, sep, phyrate, sep,
					st_bw, sep, st_mcs, sep, st_nss, sep,
					ctr->rxoow, sep, ctr->rxholes, sep,
					ctr->rxdup, sep, ctr->rxretried, sep,
					ratio);
				}
				break;
			}
		}
	}

	/* Add total summary */
	if (display_overall && (display_mode == DISPLAY_COOKED)) {
		ctr = &allc;

		if (ctr->rxmpdu) {
			float phyrate = 0, ratio = 0, speed = 0;
			phyrate = (ctr->rxphyrate/1000.0)/ctr->rxmpdu;

			if (rec->time_delta) {
				speed = (ctr->rxbyte*8.0) / rec->time_delta;
				ratio = speed * 100.0 / phyrate;
			}

			printf("                 (overall)%c", sep);
			printf("%3c%c%7d%c%8d%c"
				"%10.1f%c%10c%c%5c%c%5c%c%5c%c"
				"%5c%c%5c%c%5c%c%5c%c"
				"%6.0f%%\n",
				'-', sep, ctr->rxampdu, sep, ctr->rxmpdu, sep,
				speed, sep, '-', sep, '-', sep, '-', sep, '-', sep,
				'-', sep, '-', sep,
				'-', sep, '-', sep,
				ratio);
		}
	}

	return BCME_OK;
}

/* IO variables that take MAC addresses (with optional single letter prefix)
 * and output a string buffer
 */
static int
wl_iov_pktqlog_params(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	char** macaddrs = argv + 1;

	wl_iov_mac_full_params_t*  full_params = (wl_iov_mac_full_params_t*)buf;
	wl_iov_mac_params_t*       params = &full_params->params;
	wl_iov_mac_extra_params_t* extra_params = &full_params->extra_params;

	wl_iov_mac_params_t       loop_params;
	wl_iov_mac_extra_params_t loop_extra_params;
	uint32   index;
	bool  loop_assoclist = FALSE;
	struct maclist* maclist = NULL;
	wlc_rev_info_t revinfo;
	uint32 corerev;

	if (cmd->get < 0)
		return -1;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	corerev = dtoh32(revinfo.corerev);

	memset(full_params, 0, sizeof(*full_params));
	memset(&loop_params, 0, sizeof(loop_params));
	memset(&loop_extra_params, 0, sizeof(loop_extra_params));

	/* only pass up to WL_IOV_MAC_PARAM_LEN parameters */
	while (params->num_addrs < WL_IOV_MAC_PARAM_LEN && *macaddrs) {
		bool    full_auto = FALSE;
		char*   ptr = *macaddrs;
		uint32  bitmask;

		/* is there a prefix character? */
		if (ptr[1] == ':') {
			params->addr_type[params->num_addrs] = toupper((int)(ptr[0]));

			/* move ptr to skip over prefix */
			ptr += 2;

			/* is there the 'long form' option ? */
			if (ptr[0] == '+') {
				/* check for + additional info option, set top bit */
				params->addr_type[params->num_addrs]  |= 0x80;
				ptr++;
			}
			if ((ptr[0] == 0) || (ptr[0] == '/' || ptr[0] == ',')) {
				/* this is the fully automatic mode */
				full_auto = TRUE;
			}
		}

		/* the prefix C: denotes no given MAC address (to refer to "common") */
		if ((params->addr_type[params->num_addrs] & 0x7F) == 'C')  {
			full_auto = FALSE;
		}
		else if (full_auto) {
			loop_assoclist = TRUE;
			loop_params.addr_type[loop_params.num_addrs] =
			                     params->addr_type[params->num_addrs];
		}
		else if (wl_ether_atoe(ptr, &params->ea[params->num_addrs])) {
			 /* length of MAC addr string excl end char */
			ptr += (ETHER_ADDR_STR_LEN - 1);
		}
		else {
			params->addr_type[params->num_addrs] = 0;
			printf("Bad parameter '%s'\n", *macaddrs);
			++macaddrs;
			continue;
		}

		bitmask = 0;

		while (ptr && (ptr[0] == ',' || ptr[0] == '/') &&
		         ((ptr[1] >= '0' && ptr[1] <= '9') ||
		         ptr[1] == '/' || ptr[1] == ',')) {

			uint8 prec;
			char* endptr = 0;

			if (ptr[1] == '/' || ptr[1] == ',') {
				/* this is the 'auto' setting */
				bitmask |= PKTQ_LOG_AUTO;
				ptr += 2;
			}
			else {
				ptr++;

				prec = (uint8)strtoul(ptr, &endptr, 10);

				if (prec <= 15) {
					bitmask |= (1 << prec);
				}
				else {
					printf("Bad precedence %d (will be ignored)\n",
					       prec);
				}
				ptr = endptr;
			}

		}

		if (bitmask == 0) {
			/* PKTQ_LOG_DEF_PREC is ignored in V4, it is used to indicate no prec was
			 * selected
			 */
			bitmask = 0xFFFF | PKTQ_LOG_DEF_PREC;
		}

		if (full_auto) {
			loop_extra_params.addr_info[loop_params.num_addrs] = bitmask;
			loop_params.num_addrs++;
		}
		else {
			extra_params->addr_info[params->num_addrs] = bitmask;
			params->num_addrs ++;
		}
		++macaddrs;
	}

	while (*macaddrs) {
		printf("Ignoring excess parameter '%s' (maximum number of params is %d)\n",
		       *macaddrs, WL_IOV_MAC_PARAM_LEN);
		++macaddrs;
	}

	/* if no valid params found, pass default prefix 'C' with no mac address */
	if (params->num_addrs == 0 && !loop_assoclist)
	{
		params->addr_type[0] = 'C';
		extra_params->addr_info[0] = 0xFFFF;
		params->num_addrs = 1;
	}

	if (params->num_addrs) {
		/* set a "version" indication (ie extra_params present) */
		params->num_addrs |= (4 << 8);

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, full_params,
		                            sizeof(*full_params),
		                            buf, WLC_IOCTL_MAXLEN)) < 0) {
			fprintf(stderr, "Error getting variable %s\n", argv[0]);
			return ret;
		}

		wl_txq_prec_dump((wl_iov_pktq_log_t*)buf, FALSE, corerev >= 40);

	}
	if (!loop_assoclist) {
		return 0;
	}

	maclist = malloc(WLC_IOCTL_MEDLEN);

	if (!maclist) {
		fprintf(stderr, "unable to allocate memory\n");
		return -ENOMEM;
	}
	maclist->count = htod32((WLC_IOCTL_MEDLEN - sizeof(int)) / ETHER_ADDR_LEN);

	if ((ret = wlu_get(wl, WLC_GET_ASSOCLIST, maclist, WLC_IOCTL_MEDLEN)) < 0) {
		fprintf(stderr, "Cannot get assoclist\n");
		free(maclist);
		return ret;
	}
	maclist->count = dtoh32(maclist->count);

	if (maclist->count == 0) {
		fprintf(stderr, "No available addresses in assoclist for automatic operation\n");
		free(maclist);
		return 0;
	}

	for (index = 0; index < loop_params.num_addrs; index++) {
		uint32 ea_index = 0;

		while (ea_index < maclist->count) {

			memset(full_params, 0, sizeof(*full_params));

			while ((params->num_addrs < WL_IOV_MAC_PARAM_LEN) &&
			       (ea_index < maclist->count)) {

				params->addr_type[params->num_addrs] = loop_params.addr_type[index];
				params->ea[params->num_addrs] = maclist->ea[ea_index];
				extra_params->addr_info[params->num_addrs] =
					loop_extra_params.addr_info[index] | PKTQ_LOG_AUTO;

				params->num_addrs++;
				ea_index++;
			}

			/* set a "version" indication (ie extra_params present) */
			params->num_addrs |= (4 << 8);

			if ((ret = wlu_iovar_getbuf(wl, cmd->name, full_params,
			                            sizeof(*full_params),
			                            buf, WLC_IOCTL_MAXLEN)) < 0) {
				fprintf(stderr, "Error getting %s\n", argv[0]);
				free(maclist);
				return ret;
			}

			wl_txq_prec_dump((wl_iov_pktq_log_t*)buf, TRUE, corerev >= 40);
		}
	}
	free(maclist);
	return 0;
}

static int
wlu_dump_phytbls(void *wl, char *dump_buf)
{
	/* The IOVAR's used in iteration */
	const char str_phytbl[] = "phytbl";
	const char str_phytbl_page[] = "phydump_page";
	const char str_phytbl_entry[] = "phydump_entry";
	int phytbl_page = 0;
	int phytbl_entry = 0;
	int ret;

	/* Initialize page and entry */
	ret = wlu_iovar_setint(wl, str_phytbl_page, phytbl_page);
	if (ret != BCME_OK) {
		fprintf(stderr, "IOVAR %s is not supported by firmware\n", str_phytbl_page);
	} else {
		ret = wlu_iovar_setint(wl, str_phytbl_entry, phytbl_entry);
		if (ret != BCME_OK) {
			fprintf(stderr, "IOVAR %s is not supported by firmware\n",
					str_phytbl_entry);
		}
		else {
			/* Dump the whole table by iterating str_phytbl IOVAR */
			do {
				strncpy(dump_buf, str_phytbl, sizeof(str_phytbl));
				if ((ret = wlu_iovar_getbuf(wl, "dump",
						dump_buf, (int)strlen(dump_buf),
						dump_buf, WL_DUMP_BUF_LEN)) >= 0) {
					fputs(dump_buf, stdout);
					memset(dump_buf, 0, WL_DUMP_BUF_LEN);
					if (((ret = wlu_iovar_getint(wl, str_phytbl_page,
							&phytbl_page)) >= 0) &&
							(phytbl_page == 0)) {
						ret = wlu_iovar_getint(wl, str_phytbl_entry,
								&phytbl_entry);
					}
				}
			/* Iteration ends when both page and entry wrap around */
			} while ((ret >= BCME_OK) && ((phytbl_page != 0) || (phytbl_entry != 0)));
		}
	}
	return ret;
}

static int
wlu_dump(void *wl, cmd_t *cmd, char **argv)
{
	int ret, err, bcmerr;
	char *dump_buf;

	/* This suboption is only known to wl, IOVAR list won't show it */
	const char str_phytbls[] = "phytbls";

	if (cmd->get < 0)
		return -1;

	dump_buf = malloc(WL_DUMP_BUF_LEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WL_DUMP_BUF_LEN);
		return BCME_NOMEM;
	}
	memset(dump_buf, 0, WL_DUMP_BUF_LEN);

	/* skip the command name */
	argv++;

	/* XXX: Remove "if (*argv == NULL)" conditional code after 5/20/06
	 * when there is no need to run wl command over old driver
	 */
	/* If no args given, get the subset of 'wl dump all'
	 * Otherwise, if args are given, they are the dump section names.
	 */
	if (*argv == NULL) {
		/* query for the 'dump' without any argument */
		ret = wlu_iovar_getbuf(wl, "dump", NULL, 0, dump_buf, WL_DUMP_BUF_LEN);

		/* if the query is successful, continue on and print the result. */

		/* if the query fails, check for a legacy driver that does not support
		 * the "dump" iovar, and instead issue a WLC_DUMP ioctl.
		 */
		if (ret) {
			err = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
			if (!err && (bcmerr == BCME_UNSUPPORTED)) {
				ret = wlu_get(wl, WLC_DUMP, dump_buf, WL_DUMP_BUF_LEN);
				if (ret) {
					fprintf(stderr, "dump: error on query of WLC_DUMP\n");
				}
			} else {
				fprintf(stderr, "dump: error on query of dump list\n");
			}
		}
	} else if (!strncmp(*argv, str_phytbls, sizeof(str_phytbls))) {
		ret = wlu_dump_phytbls(wl, dump_buf);
	} else {
		/* create the dump section name list */
		while (*argv) {
			/* add space delimiter if this is not the first section name */
			if (dump_buf[0] != '\0')
				strcat(dump_buf, " ");

			strcat(dump_buf, *argv);

			argv++;
		}

		/* This is a "space" added at end of last argument */
		strcat(dump_buf, " ");

		ret = wlu_iovar_getbuf(wl, "dump", dump_buf, (int)strlen(dump_buf),
				dump_buf, WL_DUMP_BUF_LEN);
	}

	if (ret >= BCME_OK) {
		ret = BCME_OK;
		fputs(dump_buf, stdout);
	}

	free(dump_buf);

	return ret;
}

static int
wlu_dump_clr(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	/* skip the command name */
	argv++;

	/* call dump_clear for each category */
	while (*argv != NULL) {
		memset(buf, 0, WLC_IOCTL_MAXLEN);

		ret = wlu_iovar_setbuf(wl, cmd->name, *argv, strlen(*argv),
		                       buf, WLC_IOCTL_MAXLEN);
		if (ret != BCME_OK) {
			fprintf(stderr, "%s: error on %s\n", cmd->name, *argv);
			return ret;
		}
		argv++;
	}

	return BCME_OK;
}

static int
wl_staprio(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_USAGE_ERROR;
	wl_staprio_cfg_t staprio_cfg;
	char 	*endptr = NULL;

	if (!*++argv) return -1;

	/* get link mac address */
	if (!wl_ether_atoe(*argv++, &staprio_cfg.ea))
		goto error;

	if (argv[0]) {
		staprio_cfg.prio = (uint8)strtol(argv[0], &endptr, 0);
		if (*endptr != '\0')
			goto error;

		if (staprio_cfg.prio > 3) {
			printf("prio %d out of range [0, 3]\n", staprio_cfg.prio);
			goto error;
		}
		else {
			printf("Set SCB prio: 0x%x\n", staprio_cfg.prio);
			ret = wlu_iovar_setbuf(wl, cmd->name, (void *) &staprio_cfg,
				sizeof(wl_staprio_cfg_t), buf, WLC_IOCTL_MEDLEN);
		}
	}
	else {
		if ((ret = wlu_iovar_getbuf(wl, cmd->name, (void *) &staprio_cfg,
			sizeof(wl_staprio_cfg_t), buf, WLC_IOCTL_MEDLEN)) >= 0) {
			printf("SCB prio: 0x%x\n", ((wl_staprio_cfg_t *)buf)->prio);
		}
	}

error:
	return ret;
}

static int
wlu_srdump(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i, nw, nb = 0;
	uint16 *words = (uint16 *)&buf[8];

	srom_rw_t   *srt;

	/*
	 * Avoid wl utility to driver compatibility issues by reading a 'safe' amount of words from
	 * SPROM to determine the SPROM version that the driver supports, once the version is known
	 * the full SPROM contents can be read. At the moment sromrev12 is the largest.
	 */
	nw = MAX(MAX(SROM10_SIGN, SROM11_SIGN), SROM11_SIGN)  + 1;

	srt = (srom_rw_t *)buf;
	srt->byteoff = htod32(0);
	srt->nbytes = htod32(2 * nw);

	if (cmd->get < 0)
		return -1;
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	if (words[SROM11_SIGN] == SROM15_SIGNATURE) {
		nw = SROM15_WORDS;
	} else if (words[SROM11_SIGN] == SROM13_SIGNATURE) {
		nw = SROM13_WORDS;
	} else if (words[SROM11_SIGN] == SROM12_SIGNATURE) {
		nw = SROM12_WORDS;
	} else if (words[SROM11_SIGN] == SROM11_SIGNATURE) {
		nw = SROM11_WORDS;
	} else if (words[SROM10_SIGN] == SROM10_SIGNATURE) {
		nw = SROM10_WORDS;
	} else if (words[SROM16_SIGN] == SROM16_SIGNATURE) {
		nw = SROM16_WORDS;
	} else if (words[SROM17_SIGN] == SROM17_SIGNATURE) {
		nw = SROM17_WORDS;
	} else if (words[SROM18_SIGN] == SROM18_SIGNATURE) {
		nw = SROM18_WORDS;
	} else {
		nw = SROM4_WORDS;
		if ((words[SROM4_SIGN] != SROM4_SIGNATURE) &&
			(words[SROM8_SIGN] != SROM4_SIGNATURE))
			nw = SROM_WORDS;
	}

	/* allow reading a larger (or any other-size one) if specified */
	if (*++argv != NULL) {
		nb = (int)strtol(*argv, NULL, 0);
		if (nb & 1) {
			printf("Byte count %d is odd\n", nb);
			return BCME_BADARG;
		}
		nw = nb / 2;
	}

	/* Since the SROM version known at this point, the entire SPROM contents can be read */
	srt->nbytes = htod32(2 * nw);
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	for (i = 0; i < nw; i++) {
		if ((i % 8) == 0)
			printf("\n  srom[%03d]:  ", i);
		printf("0x%04x  ", words[i]);
	}
	printf("\n");

	return 0;
}

#if	defined(linux) || defined(MACOSX) || defined(_CFE_) || defined(DONGLEBUILD) || \
	defined(BWL_NO_INTERNAL_STDLIB_SUPPORT)
/* linux, MacOS: ffs is in the standard C library */
/* CFE, DONGLEBUILD: Not needed, the code below is ifdef out */
#else
static int
ffs(int i)
{
	int j;

	if (i != 0)
		for (j = 0; j < 32; j++)
			if (i & (1 << j))
				return j + 1;
	return 0;
}
#endif	/* linux, MACOSX, CFE, DONGLEBUILD */

#if	!defined(_CFE_) && !defined(DONGLEBUILD)

/* VX wants prototypes even for static functions. */
static int newtuple(char *b, int *cnt, uint8 tag, const cis_tuple_t *srv);
static int parsecis(char *b, char **argv, int sromrev);
static const sromvar_t *srvlookup(const sromvar_t *tab, char *name, int nlen, int sromrev);

static int
newtuple(char *b, int *cnt, uint8 tag, const cis_tuple_t *srv)
{
	memset(b, 0, srv->len + 2);

	b[0] = tag;
	b[1] = (char)srv->len;
	b[2] = (char)srv->tag;

	if (cnt)
		*cnt += 3;
	return 0;
}

/**
 * When programming OTP or SROM, driver expects to receive a CIS from the wl utility.
 * This function converts a caller supplied string (in **argv) containing nvram variables pairs into
 * a CIS (in *b). Caller can dictate the binary CIS contents  by using nvram string 'RAW=...' or
 * 'RAW1=...'. Function will only create tuples for values in caller supplied nvram string.
 */
static int
parsecis(char *b, char **argv, int sromrev)
{
	/* built-in list of known tuples and nvram var(s) associated with a specific tuple */
	const cis_tuple_t *srv = cis_hnbuvars;
	char	*cpar = NULL, *p = NULL;
	char	*par;
	char	delimit[2] = " \0";
	int	cnt = 0, i = 0;
	uint 	sromrev_mask = 0xffffffff;

	if (sromrev > 0 && sromrev <= 31) {
		sromrev_mask = 1 << sromrev;
	} else {
		printf("Invalid sromrev %d.\n", sromrev);
		return BCME_BADARG;
	}

	/* Walk through built-in list of tuples, create append buffer */
	while (srv->tag != 0xFF) {
		uint val = 0;

		/* Skip srv if not supported in sromrev */
		if (!(sromrev_mask & srv->revmask)) {
			srv++;
			continue;
		}

		/* Special cases (Raw Data / macaddr / ccode / fem) */
		if (srv->tag == OTP_RAW) {
			if ((p = find_pattern(argv, "RAW", &val))) {
				p += (strlen("RAW") + 1);	/* RAW= */
				for (;;) {
					b[cnt++] = (unsigned char) strtoul(p, &p, 16);
					if (!*p++)
						break;
				}
			}
		} else if (srv->tag == OTP_RAW1) {
			if ((p = find_pattern(argv, "RAW1", NULL))) {
				for (;;) {
					b[cnt++] = (unsigned char) strtoul(p, &p, 16);
					if (!*p++)
						break;
				}
			}
		} else if (srv->tag == OTP_VERS_1) {
			uint l1 = 1, l2 = 1;
			char *p2 = NULL;

			if ((p = find_pattern(argv, "manf", NULL)))
				l1 += strlen(p);

			if ((p2 = find_pattern(argv, "productname", NULL)))
				l2 += strlen(p2);

			if ((p != NULL) | (p2 != NULL)) {
				b[cnt++] = CISTPL_VERS_1;
				b[cnt++] = 2 + l1 + l2;
				b[cnt++] = 8;
				b[cnt++] = 0;
				if (p) {
					char *q = p;
					/* Replace '_' by space */
					while ((q = strchr(q, '_')))
						*q = ' ';
					memcpy(&b[cnt], p, l1);
				} else
					b[cnt] = '\0';
				cnt += l1;

				if (p2) {
					char *q = p2;
					/* Replace '_' by space */
					while ((q = strchr(q, '_')))
						*q = ' ';
					memcpy(&b[cnt], p2, l2);
				} else
					b[cnt] = '\0';
				cnt += l2;
			}
		} else if (srv->tag == OTP_MANFID) {
			bool found = FALSE;
			uint manfid = 0, prodid = 0;

			if ((p = find_pattern(argv, "manfid", &manfid)))
				found = TRUE;

			if ((p = find_pattern(argv, "prodid", &prodid)))
				found = TRUE;

			if (found) {
				b[cnt++] = CISTPL_MANFID;
				b[cnt++] = srv->len;
				b[cnt++] = (uint8)(manfid & 0xff);
				b[cnt++] = (uint8)((manfid >> 8) & 0xff);
				b[cnt++] = (uint8)(prodid & 0xff);
				b[cnt++] = (uint8)((prodid >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_MACADDR) {
			if ((p = find_pattern(argv, "macaddr", NULL))) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				if (!wl_ether_atoe(p, (struct ether_addr*)&b[cnt]))
					printf("Argument does not look like a MAC "
					"address: %s\n", p);
				cnt += sizeof(struct ether_addr);
			}
		} else if (srv->tag == HNBU_MACADDR2) {
			if ((p = find_pattern(argv, "macaddr2", NULL))) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				if (!wl_ether_atoe(p, (struct ether_addr*)&b[cnt])) {
					printf("Argument does not look like a MAC "
						"address: %s\n", p);
				}
				cnt += sizeof(struct ether_addr);
			}
		} else if (srv->tag == HNBU_CCODE) {
			bool found = FALSE;
			char tmp[3] = "\0\0\0";

			if ((p = find_pattern(argv, "ccode", NULL))) {
				found = TRUE;
				tmp[0] = *p++;
				tmp[1] = *p++;
			}
			if ((p = find_pattern(argv, "cctl", &val))) {
				found = TRUE;
				tmp[2] = (uint8)val;
			}
			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 3);
				cnt += 3;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_RSSISMBXA2G) {
			bool found = FALSE;
			char tmp[2] = "\0\0";

			if ((p = find_pattern(argv, "rssismf2g", &val))) {
				found = TRUE;
				tmp[0] |= val & 0xf;
			}
			if ((p = find_pattern(argv, "rssismc2g", &val))) {
				found = TRUE;
				tmp[0] |= (val & 0xf) << 4;
			}
			if ((p = find_pattern(argv, "rssisav2g", &val))) {
				found = TRUE;
				tmp[1] |= val & 0x7;
			}
			if ((p = find_pattern(argv, "bxa2g", &val))) {
				found = TRUE;
				tmp[1] |= (val & 0x3) << 3;
			}
			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 2);
				cnt += 2;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_RSSISMBXA5G) {
			bool found = FALSE;
			char tmp[2] = "\0\0";

			if ((p = find_pattern(argv, "rssismf5g", &val))) {
				found = TRUE;
				tmp[0] |= val & 0xf;
			}
			if ((p = find_pattern(argv, "rssismc5g", &val))) {
				found = TRUE;
				tmp[0] |= (val & 0xf) << 4;
			}
			if ((p = find_pattern(argv, "rssisav5g", &val))) {
				found = TRUE;
				tmp[1] |= val & 0x7;
			}
			if ((p = find_pattern(argv, "bxa5g", &val))) {
				found = TRUE;
				tmp[1] |= (val & 0x3) << 3;
			}
			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 2);
				cnt += 2;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_FEM) {
			bool	found = FALSE;
			uint16	tmp2g = 0, tmp5g = 0;

			if ((p = find_pattern(argv, "antswctl2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_ANTSWLUT_SHIFT) &
					SROM8_FEM_ANTSWLUT_MASK);
			}
			if ((p = find_pattern(argv, "triso2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_TR_ISO_SHIFT) &
					SROM8_FEM_TR_ISO_MASK);
			}
			if ((p = find_pattern(argv, "pdetrange2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_PDET_RANGE_SHIFT) &
					SROM8_FEM_PDET_RANGE_MASK);
			}
			if ((p = find_pattern(argv, "extpagain2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_EXTPA_GAIN_SHIFT) &
					SROM8_FEM_EXTPA_GAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssipos2g", &val))) {
				found = TRUE;
				tmp2g |= ((val << SROM8_FEM_TSSIPOS_SHIFT) &
					SROM8_FEM_TSSIPOS_MASK);
			}
			if ((p = find_pattern(argv, "antswctl5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_ANTSWLUT_SHIFT) &
					SROM8_FEM_ANTSWLUT_MASK);
			}
			if ((p = find_pattern(argv, "triso5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_TR_ISO_SHIFT) &
					SROM8_FEM_TR_ISO_MASK);
			}
			if ((p = find_pattern(argv, "pdetrange5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_PDET_RANGE_SHIFT) &
					SROM8_FEM_PDET_RANGE_MASK);
			}
			if ((p = find_pattern(argv, "extpagain5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_EXTPA_GAIN_SHIFT) &
					SROM8_FEM_EXTPA_GAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssipos5g", &val))) {
				found = TRUE;
				tmp5g |= ((val << SROM8_FEM_TSSIPOS_SHIFT) &
					SROM8_FEM_TSSIPOS_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(tmp2g & 0xff);
				b[cnt++] = (uint8)((tmp2g >> 8) & 0xff);
				b[cnt++] = (uint8)(tmp5g & 0xff);
				b[cnt++] = (uint8)((tmp5g >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_UUID) {

			char *uuidstr = NULL;
			char nibble[3] = {0, 0, 0};

			if ((uuidstr = find_pattern(argv, "uuid", NULL)) != NULL) {

				/* uuid format 12345678-1234-5678-1234-567812345678 */

				if (strlen(uuidstr) == 36) {
					newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
					while (*uuidstr != '\0') {
						if (*uuidstr == '-') {
							uuidstr++;
							continue;
						}
						nibble[0] = *uuidstr++;
						nibble[1] = *uuidstr++;
						b[cnt ++] = (char)strtoul(nibble, NULL, 16);
					}
				}
			}

		} else if (srv->tag == HNBU_TEMPTHRESH) {
			bool found = FALSE;
			char tmp[6] = "\0\0\0\0\0\0";

			if ((p = find_pattern(argv, "tempthresh", &val))) {
				found = TRUE;
				tmp[0] = val;
			}
			if ((p = find_pattern(argv, "temps_period", &val))) {
				found = TRUE;
				tmp[1] |= ((val << SROM11_TEMPS_PERIOD_SHIFT) &
					SROM11_TEMPS_PERIOD_MASK);
			}
			if ((p = find_pattern(argv, "temps_hysteresis", &val))) {
				found = TRUE;
				tmp[1] |= ((val << SROM11_TEMPS_HYSTERESIS_SHIFT) &
					SROM11_TEMPS_HYSTERESIS_MASK);
			}
			if ((p = find_pattern(argv, "tempoffset", &val))) {
				found = TRUE;
				tmp[2] = val;
			}
			if ((p = find_pattern(argv, "tempsense_slope", &val))) {
				found = TRUE;
				tmp[3] = val;
			}
			if ((p = find_pattern(argv, "tempcorrx", &val))) {
				found = TRUE;
				tmp[4] |= ((val << SROM11_TEMPCORRX_SHIFT) &
					SROM11_TEMPCORRX_MASK);
			}
			if ((p = find_pattern(argv, "tempsense_option", &val))) {
				found = TRUE;
				tmp[4] |= ((val << SROM11_TEMPSENSE_OPTION_SHIFT) &
					SROM11_TEMPSENSE_OPTION_MASK);
			}
			if ((p = find_pattern(argv, "phycal_tempdelta", &val))) {
				found = TRUE;
				tmp[5] = val;
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				memcpy(&b[cnt], tmp, 6);
				cnt += 6;	/* contents filled already */
			}
		} else if (srv->tag == HNBU_FEM_CFG) {
			bool found = FALSE;
			uint16	fem_cfg1 = 0, fem_cfg2 = 0;

			if ((p = find_pattern(argv, "femctrl", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_FEMCTRL_SHIFT) &
					SROM11_FEMCTRL_MASK);
			}
			if ((p = find_pattern(argv, "papdcap2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_PAPDCAP_SHIFT) &
					SROM11_PAPDCAP_MASK);
			}
			if ((p = find_pattern(argv, "tworangetssi2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_TWORANGETSSI_SHIFT) &
					SROM11_TWORANGETSSI_MASK);
			}
			if ((p = find_pattern(argv, "pdgain2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_PDGAIN_SHIFT) &
					SROM11_PDGAIN_MASK);
			}
			if ((p = find_pattern(argv, "epagain2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_EPAGAIN_SHIFT) &
					SROM11_EPAGAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssiposslope2g", &val))) {
				found = TRUE;
				fem_cfg1 |= ((val << SROM11_TSSIPOSSLOPE_SHIFT) &
					SROM11_TSSIPOSSLOPE_MASK);
			}
			if ((p = find_pattern(argv, "gainctrlsph", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_GAINCTRLSPH_SHIFT) &
					SROM11_GAINCTRLSPH_MASK);
			}
			if ((p = find_pattern(argv, "papdcap5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_PAPDCAP_SHIFT) &
					SROM11_PAPDCAP_MASK);
			}
			if ((p = find_pattern(argv, "tworangetssi5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_TWORANGETSSI_SHIFT) &
					SROM11_TWORANGETSSI_MASK);
			}
			if ((p = find_pattern(argv, "pdgain5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_PDGAIN_SHIFT) &
					SROM11_PDGAIN_MASK);
			}
			if ((p = find_pattern(argv, "epagain5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_EPAGAIN_SHIFT) &
					SROM11_EPAGAIN_MASK);
			}
			if ((p = find_pattern(argv, "tssiposslope5g", &val))) {
				found = TRUE;
				fem_cfg2 |= ((val << SROM11_TSSIPOSSLOPE_SHIFT) &
					SROM11_TSSIPOSSLOPE_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(fem_cfg1 & 0xff);
				b[cnt++] = (uint8)((fem_cfg1 >> 8) & 0xff);
				b[cnt++] = (uint8)(fem_cfg2 & 0xff);
				b[cnt++] = (uint8)((fem_cfg2 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_ACRXGAINS_C0) {
			bool found = FALSE;
			uint16	rxgains = 0, rxgains1 = 0;

			if ((p = find_pattern(argv, "rxgains5gtrelnabypa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gtrisoa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRISOA_SHIFT) &
					SROM11_RXGAINS5GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gelnagaina0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrelnabypa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS2GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrisoa0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRISOA_SHIFT) &
					SROM11_RXGAINS2GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gelnagaina0", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GELNAGAINA_SHIFT) &
					SROM11_RXGAINS2GELNAGAINA_MASK);
			}

			if ((p = find_pattern(argv, "rxgains5ghtrelnabypa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GHTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghtrisoa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRISOA_SHIFT) &
					SROM11_RXGAINS5GHTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghelnagaina0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GHELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrelnabypa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GMTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrisoa0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRISOA_SHIFT) &
					SROM11_RXGAINS5GMTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmelnagaina0", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GMELNAGAINA_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(rxgains & 0xff);
				b[cnt++] = (uint8)((rxgains >> 8) & 0xff);
				b[cnt++] = (uint8)(rxgains1 & 0xff);
				b[cnt++] = (uint8)((rxgains1 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_ACRXGAINS_C1) {
			bool found = FALSE;
			uint16	rxgains = 0, rxgains1 = 0;

			if ((p = find_pattern(argv, "rxgains5gtrelnabypa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gtrisoa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRISOA_SHIFT) &
					SROM11_RXGAINS5GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gelnagaina1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrelnabypa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS2GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrisoa1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRISOA_SHIFT) &
					SROM11_RXGAINS2GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gelnagaina1", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GELNAGAINA_SHIFT) &
					SROM11_RXGAINS2GELNAGAINA_MASK);
			}

			if ((p = find_pattern(argv, "rxgains5ghtrelnabypa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GHTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghtrisoa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRISOA_SHIFT) &
					SROM11_RXGAINS5GHTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghelnagaina1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GHELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrelnabypa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GMTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrisoa1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRISOA_SHIFT) &
					SROM11_RXGAINS5GMTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmelnagaina1", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GMELNAGAINA_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(rxgains & 0xff);
				b[cnt++] = (uint8)((rxgains >> 8) & 0xff);
				b[cnt++] = (uint8)(rxgains1 & 0xff);
				b[cnt++] = (uint8)((rxgains1 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_ACRXGAINS_C2) {
			bool found = FALSE;
			uint16	rxgains = 0, rxgains1 = 0;

			if ((p = find_pattern(argv, "rxgains5gtrelnabypa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gtrisoa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GTRISOA_SHIFT) &
					SROM11_RXGAINS5GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gelnagaina2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS5GELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrelnabypa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRELNABYPA_SHIFT) &
					SROM11_RXGAINS2GTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gtrisoa2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GTRISOA_SHIFT) &
					SROM11_RXGAINS2GTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains2gelnagaina2", &val))) {
				found = TRUE;
				rxgains |= ((val << SROM11_RXGAINS2GELNAGAINA_SHIFT) &
					SROM11_RXGAINS2GELNAGAINA_MASK);
			}

			if ((p = find_pattern(argv, "rxgains5ghtrelnabypa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GHTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghtrisoa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHTRISOA_SHIFT) &
					SROM11_RXGAINS5GHTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5ghelnagaina2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GHELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GHELNAGAINA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrelnabypa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRELNABYPA_SHIFT) &
					SROM11_RXGAINS5GMTRELNABYPA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmtrisoa2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMTRISOA_SHIFT) &
					SROM11_RXGAINS5GMTRISOA_MASK);
			}
			if ((p = find_pattern(argv, "rxgains5gmelnagaina2", &val))) {
				found = TRUE;
				rxgains1 |= ((val << SROM11_RXGAINS5GMELNAGAINA_SHIFT) &
					SROM11_RXGAINS5GMELNAGAINA_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(rxgains & 0xff);
				b[cnt++] = (uint8)((rxgains >> 8) & 0xff);
				b[cnt++] = (uint8)(rxgains1 & 0xff);
				b[cnt++] = (uint8)((rxgains1 >> 8) & 0xff);
			}
		} else if (srv->tag == HNBU_PDOFF_2G) {
			bool	found = FALSE;
			uint16	tmppdoff2g = 0;

			if ((p = find_pattern(argv, "pdoffset2g40ma0", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_A0_SHIFT) &
					SROM11_PDOFF_2G_40M_A0_MASK);
			}

			if ((p = find_pattern(argv, "pdoffset2g40ma1", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_A1_SHIFT) &
					SROM11_PDOFF_2G_40M_A1_MASK);
			}

			if ((p = find_pattern(argv, "pdoffset2g40ma2", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_A2_SHIFT) &
					SROM11_PDOFF_2G_40M_A2_MASK);
			}

			if ((p = find_pattern(argv, "pdoffset2g40mvalid", &val))) {
				found = TRUE;
				tmppdoff2g |= ((val << SROM11_PDOFF_2G_40M_VALID_SHIFT) &
					SROM11_PDOFF_2G_40M_VALID_MASK);
			}

			if (found) {
				newtuple(&b[cnt], &cnt, CISTPL_BRCM_HNBU, srv);
				b[cnt++] = (uint8)(tmppdoff2g & 0xff);
				b[cnt++] = (uint8)((tmppdoff2g >> 8) & 0xff);
			}
		} else {	/* All other tuples */
			int	found = FALSE, varlen = 0;
			char	*cur = &b[cnt];
			uint	newtp = TRUE;

			/* example srv->params contents: "1aa2g 1aa5g" */
			par = malloc(strlen(srv->params)+1);
			if (!par)
				return BCME_NOMEM;

			/* Walk through each parameters in one tuple */
			strcpy(par, srv->params);

			cpar = strtok (par, delimit);	/* current param */
			while (cpar) {
				int	array_sz = 1;
				val = 0;

				/* Fill the CIS tuple to b but don't commit cnt yet */
				if (newtp) {
					newtuple(cur, NULL, CISTPL_BRCM_HNBU, srv);
					cur += 3;
					newtp = FALSE;
				}

				/* the first byte of each parameter indicates its length */
				varlen = (*cpar++) - '0';

				/* parse array size if any */
				if (*cpar == '*') {
					array_sz = 0;
					while (((*++cpar) >= '0') && (*cpar <= '9'))
						array_sz = (array_sz * 10) + *cpar - '0';
				}

				/* Find the parameter in the input argument list */
				if ((p = find_pattern(argv, cpar, &val)))
					found = TRUE;
				else
					val = 0;

				while (found && array_sz--) {
					*cur++ = (uint8)(val & 0xff);
					if (varlen >= 2)
						*cur++ = (uint8)((val >> 8) & 0xff);
					if (varlen >= 4) {
						*cur++ = (uint8)((val >> 16) & 0xff);
						*cur++ = (uint8)((val >> 24) & 0xff);
					}

					/* skip the "," if more array elements */
					if (p && array_sz) {
						char *q = NULL;

						p = strstr (p, ",");	/* current param */
						if (p) {
							p++;
							val = strtoul(p, &q, strncmp(p, "0x", 2) ?
								10 : 16);
						} else {
							printf("Input array size error!");
							free(par);
							return BCME_BADARG;
						}
					}
				}

				/* move to the next parameter string */
				cpar = strtok(NULL, delimit);
			}
			free(par);

			/* commit the tuple if its valid */
			if (found)
				cnt += (cur - &b[cnt]);
		}

		srv++;
	}

	printf("sromrev %d buffer size %d bytes:\n", sromrev, cnt);
	for (i = 0; i < cnt; i++) {
		printf("0x%.02x ", b[i] & 0xff);
		if (i%8 == 7)	printf("\n");
	}
	printf("\n");

	return cnt;
}

static const sromvar_t *
srvlookup(const sromvar_t *tab, char *name, int nlen, int sromrev)
{
	uint32 srrmask;
	const sromvar_t *srv = tab;

	srrmask = 1 << sromrev;

	while (srv->name) {
		if ((strncmp(name, srv->name, nlen) == 0) &&
		    ((srrmask & srv->revmask) != 0))
		    break;
		while (srv->flags & SRFL_MORE)
			srv++;
		srv++;
	}

	return srv;
}
#endif	/* !(_CFE_ || DONGLEBUILD) */

/** read/write caller supplied NVRAM variables in OTP or SROM */
static int
wlu_srvar(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	int ret, nw, nlen, ro, co, wr, sromrev, shift = 0;
	bool otp = FALSE;
	uint32 val32 = 0;
	char *name, *p, *newval;
	const sromvar_t *srv;
	uint16 w, *words = (uint16 *)&buf[8];
	srom_rw_t   *srt;
	struct ether_addr ea;

	ro = !strcmp(*argv, "rdvar");
	wr = !strcmp(*argv, "wrvar");
	co = !strcmp(*argv, "cisconvert");

	if (!*++argv)
		return BCME_USAGE_ERROR;

	/* Query the driver on where the cis comes from: OTP or SROM */
	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "cis_source");
	ret = wl_get(wl, WLC_GET_VAR, buf, strlen(buf)+1);
	if (ret < 0) {
		; /* printf("Error %x: cannot get cis_source\n", ret); */
	}

	/* XXX If driver supports cissource and it's from OTP, consider it as CIS
	 * Not support SROM format OTP yet
	 */
	if (buf[0] == WLC_CIS_OTP)
		otp = TRUE;
	if (otp && ro) {
		/* read caller supplied nvram variable from driver */
		wl_nvget(wl, cmd, --argv);
		return ret;
	}

	/*
	 * Before OTP can be written, the caller supplied nvram string has to be converted into a
	 * list of CIS tuples. This CIS format is SROM rev dependent.
	 */
	if ((otp && wr) || co) {
		int cnt = 0, err = 0;
		uint sromrev = 8;
		void *p = NULL;

		/* Read all nvram variables from driver and retrieve srom revision from that */
		if ((err = wlu_var_getbuf(wl, "nvram_dump", NULL, 0, &p)) < 0) {
			err = wlu_get(wl, WLC_NVRAM_DUMP, &buf[0], WLC_IOCTL_MAXLEN);
		}

		if (err) {
			printf("Fail to get sromrev from nvram file!\n");
			return err;
		}

		if ((p = strstr(p, "sromrev"))) {
			char *q = NULL;

			p = (void*)((char*)p + 8);
			/* for OTP, its either srom rev 10 or 16 */
			sromrev = strtoul(p, &q, strncmp(p, "0x", 2) ? 10 : 16);
		} else {
			printf("sromrev not defined in nvram file!\n");
			return BCME_ERROR;
		}

		/* convert caller supplied nvram string (in argv) into a list of tuples (in buf) */
		if ((cnt = parsecis(buf, argv, sromrev)) <= 0) {
			printf("CIS parse failure!\n");
			return BCME_ERROR;
		}

		/* leave an empty srom_rw_t at the front for backward
		 * compatibility
		 */
		if (!co) {
			/*
			 * Pass the CIS containing caller supplied nvram vars to driver so driver
			 * can write OTP. Driver decides which OTP region (hardware,software) will
			 * be written, depending on chip type and bus type.
			 */
			ret = wlu_iovar_set(wl, "cisvar", buf, cnt); /* IOV_BMAC_CISVAR */
		}
		return ret;
	}

	/* First read the srom and find out the sromrev */
	srt = (srom_rw_t *)buf;
	srt->byteoff = htod32(0);
	srt->nbytes = htod32(2 * SROM4_WORDS);

	if (cmd->get < 0)
		return -1;
	if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return ret;

	if (words[SROM18_SIGN] == SROM18_SIGNATURE) {
		sromrev = 18;

		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * SROM18_WORDS);

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0) {
			return ret;
		}
	} else if (words[SROM16_SIGN] == SROM16_SIGNATURE) {
		sromrev = 16;

		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * SROM16_WORDS);

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0) {
			return ret;
		}
	} else if (words[SROM11_SIGN] == SROM11_SIGNATURE) {
		sromrev = 11;

		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * SROM11_WORDS);

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;

	} else if (words[SROM11_SIGN] == SROM12_SIGNATURE) {
		sromrev = 12;

		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * SROM12_WORDS);

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
	} else if (words[SROM11_SIGN] == SROM13_SIGNATURE) {
		sromrev = 13;

		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * SROM13_WORDS);

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
	} else if (words[SROM11_SIGN] == SROM15_SIGNATURE) {
		sromrev = 15;
		srt->byteoff = htod32(0);
		srt->nbytes = htod32(2 * SROM15_WORDS);

		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;
	} else {
		if ((words[SROM4_SIGN] != SROM4_SIGNATURE) &&
		    (words[SROM8_SIGN] != SROM4_SIGNATURE))
			nw = SROM_CRCREV;
		else
			nw = SROM4_CRCREV;
		sromrev = words[nw] & 0xff;
	}

	if ((sromrev < 2) || (sromrev > SROM_MAXREV)) {
		return BCME_ERROR;
	}

	nw = 0;
	while ((name = *argv++) != NULL) {
		int off, off_base;
		/* sromrev >= 18, for 2G20CCK and BW160 PA offset */
		uint bw160_shift, bw160_antoff;

		newval = strchr(name, '=');
		if (newval)
			*newval++ = '\0';
		nlen = strlen(name);
		if ((nlen == 0) || (nlen > 32)) {
			printf("Bad variable name: %s\n", name);
			continue;
		}
		off = 0;
		bw160_shift = bw160_antoff = 0;
		if (sromrev == 16) {
			srv = srvlookup(pci_srom16vars, name, nlen + 1, sromrev);
		} else if (sromrev == 15) {
			srv = srvlookup(pci_srom15vars, name, nlen + 1, sromrev);
		} else {
			srv = srvlookup(pci_sromvars, name, nlen + 1, sromrev);
		}

		if (srv->name == NULL) {
			int path;
			if (sromrev >= 15 && sromrev < 18) {
				printf("Variable %s does not exist in sromrev"
					" %d \n", name, sromrev);
				return BCME_ERROR;
			}
			srv = srvlookup(perpath_pci_sromvars, name, nlen - 1, sromrev);
			path = name[nlen - 1] - '0';
			if ((srv->name == NULL) || (path < 0) || (path >= MAX_PATH_SROM)) {
				printf("Variable %s does not exist in sromrev %d\n",
				       name, sromrev);
				continue;
			}
			if (sromrev == 18) {
				if (path == 0) {
					off = SROM18_PATH0;
					if (srv->off > LEGACY_PA_OFFSET_END) {
						bw160_shift = 0;
					}
				} else if (path == 1) {
					off = SROM18_PATH1;
					if (srv->off > LEGACY_PA_OFFSET_END) {
						bw160_shift = SROM18_PATH1 - SROM18_PATH0;
					}
				} else if (path == 2) {
					off = SROM18_PATH2;
					if (srv->off > LEGACY_PA_OFFSET_END) {
						bw160_shift = SROM18_PATH2 - SROM18_PATH0;
					}
				} else if (path == 3) {
					off = SROM18_PATH3;
					if (srv->off > LEGACY_PA_OFFSET_END) {
						bw160_shift = SROM18_PATH3 - SROM18_PATH0;
					}
				}
				if (srv->off > LEGACY_PA_OFFSET_END) {
					bw160_antoff = (path * 20);
				}
			} else if (sromrev == 13) {
				if (path == 0) {
					off = SROM13_PATH0;
				} else if (path == 1) {
					off = SROM13_PATH1;
				} else if (path == 2) {
					off = SROM13_PATH2;
				} else if (path == 3) {
					off = SROM13_PATH3;
				}
			} else if (sromrev == 12) {
				if (path == 0) {
					off = SROM12_PATH0;
				} else if (path == 1) {
					off = SROM12_PATH1;
				} else if (path == 2) {
					off = SROM12_PATH2;
				}
			} else if (sromrev == 11) {
				if (path == 0) {
					off = SROM11_PATH0;
				} else if (path == 1) {
					off = SROM11_PATH1;
				} else if (path == 2) {
					off = SROM11_PATH2;
				}
			} else if (sromrev >= 8) {
				if (path == 0) {
					off = SROM8_PATH0;
				} else if (path == 1) {
					off = SROM8_PATH1;
				} else if (path == 2) {
					off = SROM8_PATH2;
				} else if (path == 3) {
					off = SROM8_PATH3;
				}
			} else
				off = (path == 0) ? SROM4_PATH0 : SROM4_PATH1;
		} else {
			if (sromrev == 18) {
				/*
				 * SROM18 is extension version of SROM13. PCIE portion of SROM18
				 * increase to 112 words from 64 words, so the body portion of
				 * SROM18 need to be offset with (112 - 64).
				 */
				if (srv->off < SROM13_PCIE_WORDS) {
					if (srv->off == SROM_DEVID_PCIE) {
						/* Adjust devid's offset for sromrev 18 */
						off = SROM18_PCIE_INC;
					} else {
						off = 0;
					}
				} else {
					/* Adjust srom body portion's offset for sromrev 18 */
					off = SROM18_PCIE_INC;
				}
			} else {
				off = 0;
			}
		}
		off_base = off;
		off = off_base + srv->off - bw160_shift + bw160_antoff;

		if (ro) {
			/* This code is cheating a bit: it knows that SRFL_ETHADDR means three
			 * whole words, and SRFL_MORE means 2 whole words (i.e. the masks for
			 * them are all 0xffff).
			 */
			if (srv->flags & SRFL_ETHADDR) {
				w = words[off];
				ea.octet[0] = w >> 8;
				ea.octet[1] = w & 0xff;
				w = words[off + 1];
				ea.octet[2] = w >> 8;
				ea.octet[3] = w & 0xff;
				w = words[off + 2];
				ea.octet[4] = w >> 8;
				ea.octet[5] = w & 0xff;
			} else if (srv->flags & SRFL_MORE) {
				val32 = words[off];
				val32 |= words[srv[1].off] << 16;
			} else {
				shift = ffs(srv->mask) - 1;
				val32 = (words[off] & srv->mask) >> shift;
			}

			/* OK, print it */
			if (srv->flags & SRFL_ETHADDR)
				printf("%s=%s", name, wl_ether_etoa(&ea));
			else if (srv->flags & SRFL_PRHEX)
				printf("%s=0x%x", name, val32);
			else if (srv->flags & SRFL_PRSIGN)
				printf("%s=%d", name, val32);
			else
				printf("%s=%u", name, val32);

			if (srv->flags & SRFL_ARRAY) {
				do {
					srv ++;
					off = off_base + srv->off - bw160_shift + bw160_antoff;

					if (srv->name == NULL)
						break;

					shift = ffs(srv->mask) - 1;
					val32 = (words[off] & srv->mask) >> shift;

					if (srv->flags & SRFL_PRHEX)
						printf(",0x%x", val32);
					else if (srv->flags & SRFL_PRSIGN)
						printf(",%d", val32);
					else
						printf(",%u", val32);
				} while (srv->flags & SRFL_ARRAY);
			}
			printf("\n");

		} else {	/* wr */

			/* Make the change in the image we read */
			if (!newval) {
				printf("wrvar missing value to write for variable %s\n", name);
				ro = 1;
				break;
			}

			/* Cheating again as above */
			if (srv->flags & SRFL_ETHADDR) {
				if (!wl_ether_atoe(newval, &ea)) {
					printf("Argument does not look like a MAC address: %s\n",
						newval);
					ret = BCME_USAGE_ERROR;
					ro = 1;
					break;
				}
				words[off] = (ea.octet[0] << 8) | ea.octet[1];
				words[off + 1] = (ea.octet[2] << 8) | ea.octet[3];
				words[off + 2] = (ea.octet[4] << 8) | ea.octet[5];
				off += 2;
			} else {
				val32 = strtoul(newval, &p, 0);
				if (p == newval) {
					printf("Bad value: %s for variable %s\n", newval, name);
					ro = 1;
					break;
				}

				if (srv->flags & SRFL_MORE) {
					words[off] = val32 & 0xffff;
					words[off + 1] = val32 >> 16;
					off++;
				} else {
					shift = ffs(srv->mask) - 1;
					words[off] = (((val32 << shift) & srv->mask) |
						(words[off] & ~srv->mask));

					if (srv->flags & SRFL_ARRAY) {
						do {
							srv ++;
							off = off_base + srv->off
								- bw160_shift + bw160_antoff;

							if (srv->name == NULL)
								break;

							newval = p + 1;
							val32 = strtoul(newval, &p, 0);
							if (p == newval) {
								printf(
								 "Bad value: %s for variable %s\n",
								 newval, name);
								ro = 1;
								break;
							}

							shift = ffs(srv->mask) - 1;
							words[off] =
							 (((val32 << shift) & srv->mask) |
							 (words[off] & ~srv->mask));
						} while (srv->flags & SRFL_ARRAY);
					}
				}
			}

			if (off > nw)
				nw = off;
		}
	}

	if (!ro) {
		/* Now write all the changes */
		nw++;
		srt->byteoff = 0;
		srt->nbytes = htod32(2 * nw);
		ret = wlu_set(wl, cmd->set, buf, (2 * nw) + 8);
		if (ret < 0)
			printf("Error %d writing the srom\n", ret);
	}

	return ret;
#endif /* _CFE_ */
}

/* All 32bits are used. Please populate wl_msgs2[] with further entries */
static dbg_msg_t wl_msgs[] = {
	{WL_ERROR_VAL,		"error"},
	{WL_ERROR_VAL,		"err"},
	{WL_TRACE_VAL,		"trace"},
	{WL_PRHDRS_VAL,		"prhdrs"},
	{WL_PRPKT_VAL,		"prpkt"},
	{WL_INFORM_VAL,		"inform"},
	{WL_INFORM_VAL,		"info"},
	{WL_INFORM_VAL,		"inf"},
	{WL_TMP_VAL,		"tmp"},
	{WL_OID_VAL,		"oid"},
	{WL_RATE_VAL,		"rate"},
	{WL_ASSOC_VAL,		"assoc"},
	{WL_ASSOC_VAL,		"as"},
	{WL_PRUSR_VAL,		"prusr"},
	{WL_PS_VAL,		"ps"},
	{WL_MODE_SWITCH_VAL,	"modesw"},
	{WL_G_IOV_VAL,		"g_iov"},
	{WL_DUAL_VAL,		"dual"},
	{WL_WSEC_VAL,		"wsec"},
	{WL_WSEC_DUMP_VAL,	"wsec_dump"},
	{WL_LOG_VAL,		"log"},
	{WL_BCNTRIM_VAL,	"bcntrim"},
	{WL_PFN_VAL,		"pfn"},
	{WL_REGULATORY_VAL,	"regulatory"},
	{WL_TAF_VAL,		"taf"},
	{WL_MPC_VAL,		"mpc"},
	{WL_APSTA_VAL,		"apsta"},
	{WL_DFS_VAL,		"dfs"},
	{WL_MUMIMO_VAL,		"mumimo"},
	{WL_MUMIMO_VAL,		"mu"},
	{WL_MBSS_VAL,		"mbss"},
	{WL_CAC_VAL,		"cac"},
	{WL_AMSDU_VAL,		"amsdu"},
	{WL_AMPDU_VAL,		"ampdu"},
	{WL_FFPLD_VAL,		"ffpld"},
	{WL_TWT_VAL,		"twt"},
	{0,			NULL}
};

/* msglevels which use wl_msg_level2 should go here */
static dbg_msg_t wl_msgs2[] = {
	{WL_SCAN_VAL,	"scan"},
	{WL_WOWL_VAL,	"wowl"},
	{WL_COEX_VAL,	"coex"},
	{WL_RTDC_VAL,	"rtdc"},
	{WL_PROTO_VAL,	"proto"},
	{WL_CHANINT_VAL,	"chanim"},
	{WL_WMF_VAL, "wmf"},
#ifdef WLP2P
	{WL_P2P_VAL,	"p2p"},
#endif // endif
	{WL_ITFR_VAL,	"itfr"},
#ifdef WLMCHAN
	{WL_MCHAN_VAL,	"mchan"},
#endif // endif
#ifdef WLTDLS
	{WL_TDLS_VAL, "tdls"},
#endif // endif
	{WL_PSTA_VAL,	"psta"},
	{WL_MCNX_VAL,	"mcnx"},
	{WL_PROT_VAL,	"prot"},
	{WL_TBTT_VAL,	"tbtt"},
	{WL_TIMESTAMP_VAL, "time"},
	{WL_PWRSEL_VAL, "lpc"},
	{WL_TSO_VAL,	"tso"},
	{WL_MAC_VAL,	"mac"},
	{WL_MQ_VAL,	"mq"},
	{WL_TIMESTAMP_VAL, "chanlog"},
#ifdef WLP2PO
	{WL_P2PO_VAL, "p2po"},
#endif // endif
	{WL_CFG80211_VAL, "cfg80211"},
	{WL_WNM_VAL, "wnm"},
	{WL_TXBF_VAL, "txbf"},
	{WL_PCIE_VAL, "pcie"},
	{WL_S_IOV_VAL,	"s_iov"},
	{WL_NATOE_VAL,	"natoe"},
#ifdef WL_AIR_IQ
	{WL_AIRIQ_VAL,  "airiq"},
#endif // endif
	{0,		NULL}
};

static int
wl_msglevel(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint hval = 0, len, val = 0, found, last_val = 0, msglevel = 0, msglevel2_add = 0;
	uint msglevel2_del = 0, msglevel_add = 0, msglevel_del = 0, supported = 1;
	char *endptr = NULL;
	dbg_msg_t *dbg_msg = wl_msgs, *dbg_msg2 = wl_msgs2;
	void *ptr = NULL;
	struct wl_msglevel2 msglevel64, *reply;
	const char *cmdname = "msglevel";
	char c[32] = "0x";

	UNUSED_PARAMETER(cmd);

	/* but preseve older IOCTL call for older drivers */
	if ((ret = wlu_var_getbuf_sm(wl, cmdname, &msglevel64, sizeof(msglevel64), &ptr) < 0)) {
		if ((ret = wlu_get(wl, WLC_GET_MSGLEVEL, &msglevel, sizeof(int))) < 0)
			return (ret);
		supported = 0;
		msglevel = dtoh32(msglevel);
		if (!*++argv) {
			printf("0x%x ", msglevel);
			for (i = 0; (val = dbg_msg[i].value); i++) {
			if ((msglevel & val) && (val != last_val))
				printf(" %s", dbg_msg[i].string);
			last_val = val;
			}
		printf("\n");
		return (0);
		}
		while (*argv) {
			char *s = *argv;
			if (*s == '+' || *s == '-')
				s++;
			else
				msglevel_del = ~0; /* make the whole list absolute */
			val = strtoul(s, &endptr, 0);
			if (val == 0xFFFFFFFF) {
				fprintf(stderr,
					"Bits >32 are not supported on this driver version\n");
				val = 1;
			}
			/* not an integer if not all the string was parsed by strtoul */
			if (*endptr != '\0') {
				for (i = 0; (val = dbg_msg[i].value); i++) {
					if (stricmp(dbg_msg[i].string, s) == 0) {
						break;
					}
				}
				if (!val) {
					goto usage;
				}
			}
			if (**argv == '-')
				msglevel_del |= val;
			else
				msglevel_add |= val;
			++argv;
		}
		msglevel &= ~msglevel_del;
		msglevel |= msglevel_add;
		msglevel = htod32(msglevel);
		return (wlu_set(wl, WLC_SET_MSGLEVEL, &msglevel, sizeof(int)));
	} else { /* 64bit message level */
		reply = (struct wl_msglevel2 *)ptr;
		reply->low = dtoh32(reply->low);
		reply->high = dtoh32(reply->high);
		if (!*++argv) {
			if (reply->high != 0)
				printf("0x%x%08x", reply->high, reply->low);
			else
				printf("0x%x ", reply->low);
			for (i = 0; (val = dbg_msg2[i].value); i++) {
				if (((reply->high & val)) && (val != last_val))
					printf(" %s", dbg_msg2[i].string);
				last_val = val;
				}
			last_val = 0;
			for (i = 0; (val = dbg_msg[i].value); i++) {
				if (((reply->low & val)) && (val != last_val))
					printf(" %s", dbg_msg[i].string);
				last_val = val;
			}
			printf("\n");
			return (0);
		}
		while (*argv) {
			char* s = *argv;
			char t[32];
			found = 0;
			if (*s == '+' || *s == '-')
				s++;
			else {
				msglevel_del = ~0;	/* make the whole list absolute */
				msglevel2_del = ~0;
			}
			val = strtoul(s, &endptr, 0);
			if (val == 0xFFFFFFFF){ /* Assume >32 bit hex passed in */
				if (!(*s == '0' && *(s+1) == 'x')) {
					fprintf(stderr,
					"Msg bits >32 take only numerical input in hex\n");
					val = 0;
				} else {
					len = strlen(s);
					hval = strtoul(strncpy(t, s, len-8), &endptr, 0);
					*endptr = 0;
					s = s+strlen(t);
					s = strcat(c, s);
					val = strtoul(s, &endptr, 0);
					if (hval == 0xFFFFFFFF) {
						fprintf(stderr, "Invalid entry for msglevel\n");
						hval = 0;
						val = 0;
					}
				}
			}
			if (*endptr != '\0') {
				for (i = 0; (val = dbg_msg[i].value); i++) {
					if (stricmp(dbg_msg[i].string, s) == 0) {
						found = 1;
						break;
					}
				}
				if (!found) {
				for (i = 0; (hval = dbg_msg2[i].value); i++)
					if (stricmp(dbg_msg2[i].string, s) == 0)
						break;
				}
				if (!val && !hval)
				      goto usage;
		      }
		      if (**argv == '-') {
				msglevel_del |= val;
				if (!found)
					msglevel2_del |= hval;
		      }
		      else {
				msglevel_add |= val;
				if (!found)
					msglevel2_add |= hval;
		      }
		      ++argv;
		}
		reply->low &= ~msglevel_del;
		reply->high &= ~msglevel2_del;
		reply->low |= msglevel_add;
		reply->high |= msglevel2_add;
		reply->low = htod32(reply->low);
		reply->high = htod32(reply->high);
		msglevel64.low = reply->low;
		msglevel64.high = reply->high;
		return (wlu_var_setbuf(wl, cmdname, &msglevel64, sizeof(msglevel64)));
	}

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");

	for (i = 0; (val = dbg_msg[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, dbg_msg[i].string);
		else
			fprintf(stderr, ", %s", dbg_msg[i].string);
		last_val = val;
	}
	if (supported) {
		for (i = 0; (val = dbg_msg2[i].value); i++) {
			if (val != last_val)
				fprintf(stderr, "\n0x%x00000000 %s", val, dbg_msg2[i].string);
			else
				fprintf(stderr, ", %s", dbg_msg2[i].string);
			last_val = val;
		}
	}
	fprintf(stderr, "\n");
	return 0;
}

struct d11_mcs_rate_info {
	uint8 constellation_bits;
	uint8 coding_q;
	uint8 coding_d;
};

static const struct d11_mcs_rate_info wlu_mcs_info[] = {
	{ 1, 1, 2 }, /* MCS  0: MOD: BPSK,   CR 1/2 */
	{ 2, 1, 2 }, /* MCS  1: MOD: QPSK,   CR 1/2 */
	{ 2, 3, 4 }, /* MCS  2: MOD: QPSK,   CR 3/4 */
	{ 4, 1, 2 }, /* MCS  3: MOD: 16QAM,  CR 1/2 */
	{ 4, 3, 4 }, /* MCS  4: MOD: 16QAM,  CR 3/4 */
	{ 6, 2, 3 }, /* MCS  5: MOD: 64QAM,  CR 2/3 */
	{ 6, 3, 4 }, /* MCS  6: MOD: 64QAM,  CR 3/4 */
	{ 6, 5, 6 }, /* MCS  7: MOD: 64QAM,  CR 5/6 */
	{ 8, 3, 4 }, /* MCS  8: MOD: 256QAM, CR 3/4 */
	{ 8, 5, 6 }  /* MCS  9: MOD: 256QAM, CR 5/6 */
};

static uint
wl_mcs2rate(uint mcs, uint nss, uint bw, int sgi)
{
	const int ksps = 250; /* kilo symbols per sec, 4 us sym */
	const int Nsd_20MHz = 52;
	const int Nsd_40MHz = 108;
	const int Nsd_80MHz = 234;
	const int Nsd_160MHz = 468;
	uint rate;

	if (mcs == 32) {
		/* just return fixed values for mcs32 instead of trying to parametrize */
		rate = (sgi == 0) ? 6000 : 6700;
	} else if (mcs <= 9) {
		/* This calculation works for 11n HT and 11ac VHT if the HT mcs values
		 * are decomposed into a base MCS = MCS % 8, and Nss = 1 + MCS / 8.
		 * That is, HT MCS 23 is a base MCS = 7, Nss = 3
		 */

		/* find the number of complex numbers per symbol */
		if (bw == 20) {
			rate = Nsd_20MHz;
		} else if (bw == 40) {
			rate = Nsd_40MHz;
		} else if (bw == 80) {
			rate = Nsd_80MHz;
		} else if (bw == 160) {
			rate = Nsd_160MHz;
		} else {
			rate = 1;
		}

		/* multiply by bits per number from the constellation in use */
		rate = rate * wlu_mcs_info[mcs].constellation_bits;

		/* adjust for the number of spatial streams */
		rate = rate * nss;

		/* adjust for the coding rate given as a quotient and divisor */
		rate = (rate * wlu_mcs_info[mcs].coding_q) / wlu_mcs_info[mcs].coding_d;

		/* multiply by Kilo symbols per sec to get Kbps */
		rate = rate * ksps;

		/* adjust the symbols per sec for SGI
		 * symbol duration is 4 us without SGI, and 3.6 us with SGI,
		 * so ratio is 10 / 9
		 */
		if (sgi) {
			/* add 4 for rounding of division by 9 */
			rate = ((rate * 10) + 4) / 9;
		}
	} else {
		rate = 0;
	}

	return rate;
}

/* take a wl_ratespec arg and return phy rate in 500Kbps units */
static int
wl_ratespec2rate(uint32 rspec)
{
	const char* fn_name = "wl_ratespec2rate";
	int rate = -1;
	int sgi = ((rspec & WL_RSPEC_SGI) != 0);

	if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_RATE) {
		rate = (rspec & WL_RSPEC_RATE_MASK);
	} else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HT) {
		uint mcs = (rspec & WL_RSPEC_RATE_MASK);

		if (mcs > 32) {
			fprintf(stderr, "%s: MCS %u out of range (>32) in ratespec 0x%X\n",
				fn_name, mcs, rspec);
		} else if (mcs == 32) {
			rate = wl_mcs2rate(mcs, 1, 40, sgi) / 500;
		} else {
			uint nss = 1 + (mcs / 8);
			mcs = mcs % 8;

			rate = wl_mcs2rate(mcs, nss, 20, sgi) / 500;
		}
	} else if ((rspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT) {
		uint mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
		uint nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		rate = wl_mcs2rate(mcs, nss, 20, sgi) / 500;
	} else {
		fprintf(stderr, "%s: expected rate encoding in ratespec 0x%X\n",
			fn_name, (uint)rspec);
	}

	return rate;
}

/* take rate arg in units of 500Kbits/s and print it in units of Mbit/s */
static void
wl_printrate(int val)
{
	char rate_buf[32];

	printf("%s\n", rate_int2string(rate_buf, val));
}

/* convert rate string in Mbit/s format, like "11", "5.5", to internal 500 Kbit/s units */
int
rate_string2int(char *s)
{
	if (!stricmp(s, "-1"))
		return (0);
	if (!stricmp(s, "5.5"))
		return (11);
	return (atoi(s) * 2);
}

/* convert rate internal 500 Kbits/s units to string in Mbits/s format, like "11", "5.5" */
char*
rate_int2string(char *rate_buf, int val)
{
	if ((val == -1) || (val == 0))
		sprintf(rate_buf, "auto");
	else
		sprintf(rate_buf, "%d%s Mbps", (val / 2), (val & 1) ? ".5" : "");
	return (rate_buf);
}

/*
 * Format a ratespec for output of any of the wl_rate() iovars
 */
char*
wl_rate_print(char *rate_buf, uint32 rspec)
{
	uint encode, rate, txexp, bw_val;
	const char* stbc;
	const char* ldpc;
	const char* bw;
	uint8 valid_encding = FALSE;

	encode = (rspec & WL_RSPEC_ENCODING_MASK);
	rate = (rspec & WL_RSPEC_RATE_MASK);
	txexp = (rspec & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT;
	bw_val = (rspec & WL_RSPEC_BW_MASK);
	stbc  = ((rspec & WL_RSPEC_STBC) != 0) ? " stbc" : "";
	ldpc  = ((rspec & WL_RSPEC_LDPC) != 0) ? " ldpc" : "";

	if (bw_val == WL_RSPEC_BW_UNSPECIFIED) {
		bw = "auto";
	} else if (bw_val == WL_RSPEC_BW_20MHZ) {
		bw = "20";
	} else if (bw_val == WL_RSPEC_BW_40MHZ) {
		bw = "40";
	} else if (bw_val == WL_RSPEC_BW_80MHZ) {
		bw = "80";
	} else if (bw_val == WL_RSPEC_BW_160MHZ) {
		bw = "160";
	} else if (bw_val == WL_RSPEC_BW_10MHZ) {
		bw = "10";
	} else if (bw_val == WL_RSPEC_BW_5MHZ) {
		bw = "5";
	} else if (bw_val == WL_RSPEC_BW_2P5MHZ) {
		bw = "2.5";
	} else {
		bw = "???";
	}

	if ((rspec & ~WL_RSPEC_TXEXP_MASK) == 0) { /* Ignore TxExpansion for NULL rspec check */
		valid_encding = TRUE;
		sprintf(rate_buf, "auto");
	} else if (encode == WL_RSPEC_ENCODE_HE) {
		const char* gi_ltf[] = {" 1xLTF GI 0.8us", " 2xLTF GI 0.8us",
			" 2xLTF GI 1.6us", " 4xLTF GI 3.2us"};
		uint8 gi_int = RSPEC_HE_LTF_GI(rspec);
		uint mcs = (rspec & WL_RSPEC_HE_MCS_MASK);
		uint Nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;

		valid_encding = TRUE;

		sprintf(rate_buf, "he mcs %d Nss %d Tx Exp %d BW %s%s%s%s",
			mcs, Nss, txexp, bw, stbc, ldpc, gi_ltf[gi_int]);
	} else {
		const char* sgi;
		sgi = ((rspec & WL_RSPEC_SGI)  != 0) ? " sgi"  : "";
		if (encode == WL_RSPEC_ENCODE_RATE) {

			valid_encding = TRUE;

			sprintf(rate_buf, "rate %d%s Mbps Tx Exp %d",
				rate/2, (rate % 2)?".5":"", txexp);
		} else if (encode == WL_RSPEC_ENCODE_HT) {

			valid_encding = TRUE;

			sprintf(rate_buf, "ht mcs %d Tx Exp %d BW %s%s%s%s",
				rate, txexp, bw, stbc, ldpc, sgi);
		} else if (encode == WL_RSPEC_ENCODE_VHT) {
			uint mcs = (rspec & WL_RSPEC_VHT_MCS_MASK);
			uint Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

			valid_encding = TRUE;

			sprintf(rate_buf, "vht mcs %d Nss %d Tx Exp %d BW %s%s%s%s",
				mcs, Nss, txexp, bw, stbc, ldpc, sgi);
		}
	}

	if (!valid_encding) {
		sprintf(rate_buf, "<unknown encoding for ratespec 0x%08X>", rspec);
	}

	return rate_buf;
}

/* handles both "rate" and "mrate", which makes the flow a bit complex */
static int
wl_rate_mrate(void *wl, cmd_t *cmd, char **argv)
{
	const char* fn_name = "wl_rate_mrate";
	int error;
	int val;
	int band;
	int list[3];
	char aname[sizeof("5g_mrate") + 1];
	char bgname[sizeof("2g_mrate") + 1];
	char *name;

	sprintf(aname, "5g_%s", cmd->name);
	sprintf(bgname, "2g_%s", cmd->name);

	if ((error = wlu_get(wl, WLC_GET_BAND, &band, sizeof(uint))) < 0)
		return error;
	band = dtoh32(band);

	if ((error = wlu_get(wl, WLC_GET_BANDLIST, list, sizeof(list))) < 0)
		return error;
	list[0] = dtoh32(list[0]);
	list[1] = dtoh32(list[1]);
	list[2] = dtoh32(list[2]);

	if (!list[0])
		return BCME_ERROR;
	else if (list[0] > 2)
		list[0] = 2;

	/* toss the command name from the args */
	argv++;

	if ((!strcmp(cmd->name, "rate"))) {
		/* it is "rate" */
		if (!*argv) {
			/* it is "rate" get. handle it here */
			/* WLC_GET_RATE processing */
			if ((error = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
				return error;

			val = dtoh32(val);
			wl_printrate(val);
			return 0;
		}
	}

	switch (band) {
	case WLC_BAND_AUTO :
		if (list[0] > 1) {
			fprintf(stderr,
			        "%s: driver band must be locked to %s %s, use %s/%s instead\n",
			        fn_name, (*argv ? "set" : "get"), cmd->name,
			        bgname, aname);
			return BCME_BADARG;
		} else if (list[1] == WLC_BAND_5G)
			name = (char *)aname;
		else if (list[1] == WLC_BAND_2G)
			name = (char *)bgname;
		else
			return BCME_ERROR;

		break;

	case WLC_BAND_5G :
		name = (char *)aname;
		break;

	case WLC_BAND_2G :
		name = (char *)bgname;
		break;

	default :
		return BCME_ERROR;
	}

	if (!*argv) {
		/* it is "mrate" get */
		if ((error = wlu_iovar_getint(wl, name, &val) < 0))
			return error;

		val = dtoh32(val);

		if (ioctl_version == 1) {
			wl_printrate(val);
		} else {
			wl_printrate(wl_ratespec2rate((uint32)val));
		}
	} else {
		/* create the ioctl value based on the major ioctl version */
		if (ioctl_version == 1) {
			/* for ver=1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are just given as 500 Kbps units
			 */
			val = rate_string2int(*argv);
		} else {
			/* for ver>1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are a wl_ratespec of a legacy rate.
			 */
			val = WL_RSPEC_ENCODE_RATE | rate_string2int(*argv);
		}

		val = htod32(val);

		error = wlu_iovar_setint(wl, name, val);
	}

	return error;
}

/* parse the -v/--vht or -c argument for the wl_rate() command.
 * return FALSE if the arg does not look like MxS or cMsS, where M and S are single digits
 * return TRUE if the arg does look like MxS or cMsS, setting mcsp to M, and nssp to S
 */
static int
wl_parse_he_vht_spec(const char* cp, int* mcsp, int* nssp)
{
	char *startp, *endp;
	char c;
	int mcs, nss;
	char sx;

	if (cp == NULL || cp[0] == '\0') {
		return FALSE;
	}

	if (cp[0] == 'c') {
		startp = (char*)cp + 1;
		sx = 's';
	}
	else {
		startp = (char*)cp;
		sx = 'x';
	}

	mcs = (int)strtol(startp, &endp, 10);
	/* verify MCS 0-11, and next char is 's' or 'x' */
	/* HE MCS is 0-11, VHT MCS 0-9 and prop MCS 10-11 */
	if (mcs < 0 || mcs > 11 || endp[0] != sx) {
		return FALSE;
	}

	/* grab the char after the 'x'/'s' and convert to value */
	c = endp[1];
	nss = 0;
	if (isdigit((int)c)) {
		nss = c - '0';
	}

	/* consume trailing space after digit */
	cp = &endp[2];
	while (isspace((int)(*cp))) {
		cp++;
	}

	/* check for trailing garbage after digit */
	if (cp[0] != '\0') {
		return FALSE;
	}

	if (nss < 1 || nss > 8) {
		return FALSE;
	}

	*mcsp = mcs;
	*nssp = nss;

	return TRUE;
}

static int
wl_rate(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t mo;
	char *option_name = NULL;
	char* endp;
	const int option_name_len = 64;
	const char* iov_name;
	const char* fn_name = "wl_rate";
	bool options = FALSE;
	bool auto_set = FALSE;
	bool legacy_set = FALSE, ht_set = FALSE, vht_set = FALSE, he_set = FALSE;
	int rate, mcs, Nss, tx_exp, bw;
	bool stbc, ldpc, sgi;
	uint8 hegi;
	int err, opt_err;
	uint32 rspec = 0;

	/* set default values */
	rate = 0;
	mcs = 0;
	Nss = 0;
	tx_exp = 0;
	stbc = FALSE;
	ldpc = FALSE;
	sgi = FALSE;
	hegi = 0xFF;
	bw = 0;

	/* save the command name */
	iov_name = *argv++;

	if (ioctl_version == 1) {
		fprintf(stderr,
			"cmd %s not supported in this version of the driver, ioctl version 1.\n",
			iov_name);
		return BCME_USAGE_ERROR;
	}

	/* process a GET */
	if (!*argv) {
		uint32 val = 0;
		char * rate_str;
		const int rate_str_len = 64;

		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_getint(wl, iov_name, (int*)&val)) < 0)
			return err;

		rate_str = malloc(rate_str_len);
		if (rate_str == NULL) {
			fprintf(stderr, "%s: malloc failure for rate string buffer.\n", fn_name);
			return BCME_NOMEM;
		}
		memset(rate_str, 0, rate_str_len);

		wl_rate_print(rate_str, val);

		printf("%s\n", rate_str);

		free(rate_str);

		return 0;
	}

	/* process a SET */

	/* initialze to common error for the miniopt processing */
	err = BCME_USAGE_ERROR;

	/* alloc option name for error messages */
	option_name = malloc(option_name_len);
	if (option_name == NULL) {
		fprintf(stderr, "%s: malloc failure for option_name buffer.\n", fn_name);
		return BCME_NOMEM;
	}
	memset(option_name, 0, option_name_len);

	miniopt_init(&mo, fn_name, "lg", TRUE);
	while ((opt_err = miniopt(&mo, argv)) != -1) {
		if (opt_err == 1) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		argv += mo.consumed;

		/* track whether or not the command line is just a single positional
		 * parameter of "auto" or a legacy rate, or is using options
		 */
		if (!mo.positional) {
			options = TRUE;	/* command line is using options */
		}
		if (mo.positional) {
			/* command line is using a positional parameter,
			 * complain if options are also being used
			 */
			if (options) {
				fprintf(stderr,
				        "%s: cannot mix positional args and options. "
				        "Got positional arg \"%s\" after options.\n",
				        fn_name, mo.valstr);
				goto exit;
			}
			/* complain if there are any more parameters since there should only
			 * be one positional param, "auto" or a legacy rate.
			 */
			if (*argv != NULL) {
				fprintf(stderr,
				        "%s: unexpected parameter \"%s\" after rate value.\n",
				        fn_name, *argv);
				goto exit;
			}
			/* test for "auto" to clear the rate override */
			if (!strcmp(mo.valstr, "auto")) {
				auto_set = TRUE;
			} else {
				/* pretend there was a '-r' option */
				mo.opt = 'r';
			}
		}

		/* format the option name for error messages */
		if (mo.key[0] != '\0') {
			/* would like to do the following, but snprintf() is not availble in
			 * all target builds. Fails in win_mfgtest_wl build.
			 *
			 * snprintf(option_name, option_name_len, "--%s", mo.key);
			 * option_name[option_name_len - 1] = '\0';
			 */
			size_t key_len;

			key_len = strlen(mo.key);
			/* limit key_len to space in str buffer minus the '--' and null */
			key_len = MIN((uint)(option_name_len - 3), key_len);

			memcpy(option_name, "--", 2);
			memcpy(option_name + 2, mo.key, key_len);
			option_name[2 + key_len] = '\0';
		} else {
			sprintf(option_name, "-%c", mo.opt);
		}

		/* Option: -r or --rate */
		if (mo.opt == 'r' ||
		    !strcmp(mo.key, "rate")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			/* special case check for "-r 5.5" */
			if (!strcmp(mo.valstr, "5.5")) {
				rate = 11;
			} else if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			} else {
				rate = mo.val*2;
			}

			legacy_set = TRUE;
		}
		/* Option: -h or --ht */
		if (mo.opt == 'h' ||
			!strcmp(mo.key, "ht")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}

			mcs = mo.val;
			ht_set = TRUE;
		}
		/* Option: -v or --vht */
		if (mo.opt == 'v' ||
			!strcmp(mo.key, "vht")) {

			if (mo.valstr == NULL || mo.valstr[0] == 'c') {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			mcs = (int)strtol(mo.valstr, &endp, 10);
			if (*endp == '\0') {
				mcs = mo.val;
				vht_set = TRUE;
			} else if (wl_parse_he_vht_spec(mo.valstr, &mcs, &Nss)) {
				vht_set = TRUE;
			} else {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
		}

		/* Option: -e or --he */
		if (mo.opt == 'e' || !strcmp(mo.key, "he")) {
			if (mo.valstr == NULL || mo.valstr[0] == 'c') {
				fprintf(stderr, "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			mcs = (int)strtol(mo.valstr, &endp, 10);
			if (*endp == '\0') {
				mcs = mo.val;
				he_set = TRUE;
			} else if (wl_parse_he_vht_spec(mo.valstr, &mcs, &Nss)) {
				he_set = TRUE;
			} else {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
		}

		/* Option: -c (system team's c notiation: c<MCS>s<Nss>) */
		if (mo.opt == 'c') {
			if (mo.valstr == NULL || mo.valstr[0] != 'c') {
				fprintf(stderr,
				        "%s: expected a value start with c for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			mcs = (int)strtol(mo.valstr + 1, &endp, 10);
			if (*endp == '\0') {
				vht_set = TRUE;
			} else if (wl_parse_he_vht_spec(mo.valstr, &mcs, &Nss)) {
				vht_set = TRUE;
			} else {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
		}
		/* Option: -s or --ss */
		if (mo.opt == 's' ||
		    !strcmp(mo.key, "ss")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
			Nss = mo.val;
		}
		/* Option: -x or --exp */
		if (mo.opt == 'x' ||
		    !strcmp(mo.key, "exp")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}
			tx_exp = mo.val;
			if (tx_exp < 0 || tx_exp > 3) {
				fprintf(stderr,
				        "%s: tx expansion %d out of range [0-3]\n",
				        fn_name, tx_exp);
				err = BCME_RANGE;
				goto exit;
			}
		}
		/* Option: --stbc */
		if (!strcmp(mo.key, "stbc")) {
			if (mo.valstr != NULL) {
				fprintf(stderr,
				        "%s: unexpected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}
			stbc = TRUE;
		}
		/* Option: -l or --ldpc */
		if (mo.opt == 'l' ||
		    !strcmp(mo.key, "ldpc")) {
			if (mo.valstr != NULL) {
				fprintf(stderr,
				        "%s: unexpected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}
			ldpc = TRUE;
		}

		/* Option: -g or --sgi */
		if (mo.opt == 'g' || !strcmp(mo.key, "sgi")) {
			if (!vht_set && !ht_set) {
				fprintf(stderr, ":use -g option only in vht/ht \n");
					goto exit;
			}
			if (mo.valstr != NULL) {
				fprintf(stderr, "%s: unexpected a value for %s option\n",
					fn_name, option_name);
				goto exit;
			}
			sgi = TRUE;
		}

		if (mo.opt == 'i' || !strcmp(mo.key, "hegi")) {
			if (!he_set) {
				fprintf(stderr, ":use -i option only in he \n");
				goto exit;
			}

			if (mo.valstr == NULL || mo.valstr[0] == 'c') {
				fprintf(stderr, "%s: expected a value for %s option\n",
					fn_name, option_name);
				goto exit;
			}

			mo.val = (int)strtol(mo.valstr, &endp, 10);
			if (*endp == '\0') {
				if (mo.val < 4) hegi = mo.val;
				else {
					fprintf(stderr, "%s: could not parse \"%s\", "
						"as a value for %s option\n",
						fn_name, mo.valstr, option_name);
					goto exit;
				}
			}
		}

		/* Option: -b or --bandwidth */
		if (mo.opt == 'b' ||
		    !strcmp(mo.key, "bandwidth")) {
			if (mo.valstr == NULL) {
				fprintf(stderr,
				        "%s: expected a value for %s option\n",
				        fn_name, option_name);
				goto exit;
			}

			if (!mo.good_int && strcmp(mo.valstr, "2.5")) {
				fprintf(stderr,
				        "%s: could not parse \"%s\" as a value for %s option\n",
				        fn_name, mo.valstr, option_name);
				goto exit;
			}

			if (mo.val == 2) {
				if (strcmp(mo.valstr, "2.5")) {
					fprintf(stderr,
						"%s: unexpected bandwidth specified \"%s\", "
						"expected 2.5, 5, 10, 20, 40, 80, or 160\n",
						fn_name, mo.valstr);
					goto exit;
				}
				bw = WL_RSPEC_BW_2P5MHZ;
			} else if (mo.val == 5) {
				bw = WL_RSPEC_BW_5MHZ;
			} else if (mo.val == 10) {
				bw = WL_RSPEC_BW_10MHZ;
			} else if (mo.val == 20) {
				bw = WL_RSPEC_BW_20MHZ;
			} else if (mo.val == 40) {
				bw = WL_RSPEC_BW_40MHZ;
			} else if (mo.val == 80) {
				bw = WL_RSPEC_BW_80MHZ;
			} else if (mo.val == 160) {
				bw = WL_RSPEC_BW_160MHZ;
			} else {
				fprintf(stderr,
				        "%s: unexpected bandwidth specified \"%s\", "
				        "expected 2.5, 5, 10, 20, 40, 80, or 160\n",
				        fn_name, mo.valstr);
				goto exit;
			}
		}
	}

	/*
	 * check for necessary use of one of -r/-h/-v or auto
	 */

	if (!auto_set && !legacy_set && !ht_set && !vht_set && !he_set) {
		fprintf(stderr, "%s: must specify one of \"auto\", legacy rate -r/--rate, "
		"HT (11n) rate -h/--ht, VHT (11ac) rate -v/--vht or HE (11ax) rate -e/--he \n",
		fn_name);
		goto exit;
	}

	/*
	 * check for incompatible use of -r/-h/-v
	 */

	if (legacy_set && ht_set) {
		fprintf(stderr, "%s: cannot use legacy rate -r/--rate and "
		        "HT rate -h/--ht at the same time\n",
		        fn_name);
		goto exit;
	}

	if (legacy_set && vht_set) {
		fprintf(stderr, "%s: cannot use legacy rate -r/--rate and "
		        "HT rate -v/--vht at the same time\n",
		        fn_name);
		goto exit;
	}

	if (legacy_set && he_set) {
		fprintf(stderr, "%s: cannot use legacy rate -r/--rate and "
		        "HE rate -e/--he at the same time\n",
		        fn_name);
		goto exit;
	}

	if (ht_set && (vht_set || he_set)) {
		fprintf(stderr, "%s: cannot use HT rate -h/--ht, "
		        "VHT rate -v/--vht or HE rate -e/--he at the same time\n",
		        fn_name);
		goto exit;
	}

	/* Nss can only be used with VHT and HE */
	if ((!he_set && !vht_set) && Nss != 0) {
		fprintf(stderr, "%s: cannot use -s/--ss option with non VHT and non HE rate\n",
		        fn_name);
		goto exit;
	}

	/* STBC, LDPC, SGI can only be used with HT/VHT/HE rates */
	if ((stbc || ldpc || sgi || (hegi && (hegi != 0xFF))) && !(ht_set || vht_set || he_set)) {
		fprintf(stderr, "%s: cannot use STBC/LDPC/SGI options with non HT/VHT/HE rates\n",
		        fn_name);
		goto exit;
	}

	/* set the ratespec encoding type and basic rate value */
	if (auto_set) {
		rspec = 0;
	} else if (legacy_set) {
		rspec = WL_RSPEC_ENCODE_RATE;	/* 11abg */
		rspec |= rate;
	} else if (ht_set) {
		rspec = WL_RSPEC_ENCODE_HT;	/* 11n HT */
		rspec |= mcs;
	} else if (vht_set) {
		rspec = WL_RSPEC_ENCODE_VHT;	/* 11ac VHT */
		if (Nss == 0) {
			Nss = 1; /* default Nss = 1 if --ss option not given */
		}
		rspec |= (Nss << WL_RSPEC_VHT_NSS_SHIFT) | mcs;
	} else if (he_set) {
		rspec = WL_RSPEC_ENCODE_HE;	/* 11ax HE */
		if (Nss == 0) Nss = 1; /* default Nss = 1 if --ss option not given */
		rspec |= (Nss << WL_RSPEC_HE_NSS_SHIFT) | mcs;
	} else {
		fprintf(stderr,
				"%s: Invalid rate set for \"%s\" as a value for %s option\n",
				fn_name, mo.valstr, option_name);
				goto exit;
	}

	/* set the other rspec fields */
	rspec |= (tx_exp << WL_RSPEC_TXEXP_SHIFT);
	rspec |= bw;
	rspec |= (stbc ? WL_RSPEC_STBC : 0);
	rspec |= (ldpc ? WL_RSPEC_LDPC : 0);
	rspec |= (sgi  ? WL_RSPEC_SGI  : 0);
	if (he_set) {
		rspec |= ((hegi != 0xFF) ? HE_GI_TO_RSPEC(hegi) : HE_GI_TO_RSPEC(1));
		if (hegi == 0)
			fprintf(stderr,	"%s: Warning HE CP/LTF 0 may not be supported\n", fn_name);
		if (!ldpc && ((bw == WL_RSPEC_BW_160MHZ) || (bw == WL_RSPEC_BW_80MHZ) ||
			(bw == WL_RSPEC_BW_40MHZ) || (bw == 0)))
			fprintf(stderr,	"%s: Warning LDPC is required for HE > 20MHz\n", fn_name);
	}
	err = wlu_iovar_setint(wl, iov_name, (int)rspec);

exit:
	if (option_name != NULL) {
		free(option_name);
	}

	return err;
} /* wl_rate */

static int
wl_bss_max(void *wl, cmd_t *cmd, char **argv)
{
	int val = 1;
	int error;

	UNUSED_PARAMETER(argv);

	/* Get the CAP variable; search for mbss4 or mbss16 */
	strcpy(buf, "cap");
	if ((error = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MEDLEN)) < 0)
		return (error);

	buf[WLC_IOCTL_MEDLEN] = '\0';
	if (strstr(buf, "mbss16"))
		val = 16;
	else if (strstr(buf, "mbss8"))
		val = 8;
	else if (strstr(buf, "mbss4"))
		val = 4;

	printf("%d\n", val);
	return (0);
}

int
wl_phy_rate(void *wl, cmd_t *cmd, char **argv)
{
	int val, error;
	const char *name = cmd->name;
	static const struct {
		const char* orig;
		const char* new;
	} aliases[] = {
		{"bg_rate",  "2g_rate"},
		{"bg_mrate", "2g_mrate"},
		{"a_rate",   "5g_rate"},
		{"a_mrate",  "5g_mrate"},
	};

	/* toss the command name from the args */
	argv++;

	/* if we are not using the legacy ioctl driver, translate the
	 * bg_* prefix to 2g_* and a_* to 5g_* iovars
	 */
	if (ioctl_version > 1) {
		int i;
		for (i = 0; i < 4; i++) {
			if (!strcmp(name, aliases[i].orig)) {
				name = aliases[i].new;
				break;
			}
		}
	}

	if (!*argv) {
		error = wlu_iovar_getint(wl, name, &val);
		if (error < 0)
			return (error);

		val = dtoh32(val);
		if (ioctl_version > 1) {
			wl_printrate(wl_ratespec2rate((uint32)val));
		} else {
			wl_printrate(val);
		}
	} else {
		/* create the ioctl value based on the major ioctl version */
		if (ioctl_version == 1) {
			/* for ver=1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are just given as 500 Kbps units
			 */
			val = rate_string2int(*argv);
		} else {
			/* for ver>1 ioctl interfaces, override values for 2g_(m)rate/5g_(m)rate
			 * are a wl_ratespec of a legacy rate.
			 */
			val = WL_RSPEC_ENCODE_RATE | rate_string2int(*argv);
		}

		val = htod32(val);
		error = wlu_iovar_setint(wl, name, val);
	}

	return error;
}

static int
wl_awd_data_info(void *wl, cmd_t *cmd, char **argv)
{
	awd_data_v1_t *awd_data;
	int ret, len = 0;
	uint32 type = 0xff;

	if (*++argv == NULL) {
		fprintf(stderr, "no arguments to wl_awd_data_info\n");
		return BCME_USAGE_ERROR;
	}

	/* AWD_DATA_JOIN_INFO (0) */
	if (!strcmp(*argv, "0")) {
		type = AWD_DATA_JOIN_INFO;
	}

	printf("AWD DATA INFO TYPE %d\n", type);
	/* get the generic association information */
	if ((ret = wlu_iovar_getbuf(wl, cmd->name, &type,
			sizeof(uint32), buf, WLC_IOCTL_SMLEN)) < 0) {
		return ret;
	}

	if (type == AWD_DATA_JOIN_INFO) {
		awd_data = (awd_data_v1_t *)buf;

		while (len < awd_data->length) {
			awd_tag_data_v1_t *awd_tag_data =
				(awd_tag_data_v1_t *)(awd_data->data + len);

			switch (awd_tag_data->tagid) {
			case AWD_DATA_TAG_JOIN_CLASSIFICATION_INFO:
			{
				join_classification_info_v1_t *join_tlv =
					(join_classification_info_v1_t *)(awd_tag_data->data);
				printf("\tJOIN_CLASSIFICATION_INFO::\n");
				printf("\t\tassoc_type: %d\n", join_tlv->assoc_type);
				printf("\t\tassoc_state: %d\n", join_tlv->assoc_state);
				printf("\t\twsec: 0x%x\n", join_tlv->wsec);
				printf("\t\twpa_auth: 0x%x\n", join_tlv->wpa_auth);
				printf("\t\twpa_state: 0x%x\n", join_tlv->wpa_state);
				printf("\t\twsec_portopen: %d\n", join_tlv->wsec_portopen);
				printf("\t\ttotal attempts num: %d\n",
					join_tlv->total_attempts_num);
				printf("\t\tnum of targets: %d\n", join_tlv->num_of_targets);
				printf("\t\ttime to join: %d\n", join_tlv->time_to_join);
			}
			break;
			case AWD_DATA_TAG_JOIN_TARGET_CLASSIFICATION_INFO:
			{
				join_target_classification_info_v1_t *trgt_join_tlv =
				(join_target_classification_info_v1_t *)(awd_tag_data->data);
				printf("\tJOIN_TARGET_CLASSIFICATION_INFO::\n");
				printf("\t\trssi: %d\n", trgt_join_tlv->rssi);
				printf("\t\tcca: %d\n", trgt_join_tlv->cca);
				printf("\t\tchannel: %d\n", trgt_join_tlv->channel);
				printf("\t\tnum of attempts: %d\n",
					trgt_join_tlv->num_of_attempts);
				printf("\t\tBSSID: %x:%x:%x\n", trgt_join_tlv->oui[0],
					trgt_join_tlv->oui[1], trgt_join_tlv->oui[2]);
				printf("\t\tjoin time duration: %d\n",
					trgt_join_tlv->time_duration);
			}
			break;
			case AWD_DATA_TAG_ASSOC_STATE:
			{
				join_assoc_state_v1_t *assoc_state_tlv =
					(join_assoc_state_v1_t *)(awd_tag_data->data);
				printf("\tASSOC_STATE::\n");
				printf("\t\tassoc_state: %d\n", assoc_state_tlv->assoc_state);
			}
			break;
			case AWD_DATA_TAG_CHANNEL:
			{
				join_channel_v1_t *channel_tlv =
					(join_channel_v1_t *)(awd_tag_data->data);
				printf("\tCHANNEL::\n");
				printf("\t\tchannel: %d\n", channel_tlv->channel);
			}
			break;
			case AWD_DATA_TAG_TOTAL_NUM_OF_JOIN_ATTEMPTS:
			{
				join_total_attempts_num_v1_t *num_attemps_tlv =
					(join_total_attempts_num_v1_t *)(awd_tag_data->data);
				printf("\tTOTAL_NUM_OF_JOIN_ATTEMPTS::\n");
				printf("\t\ttotal attempts num: %d\n",
					num_attemps_tlv->total_attempts_num);
			}
			break;
			default:
				printf("\tUndefined TLV Tag %d\n", awd_tag_data->tagid);
			}

			len += awd_tag_data->length + sizeof(awd_tag_data_v1_t);
		}
		printf("\n");
	}
	return 0;
}

static int
wl_pmkid_info(void *wl, cmd_t *cmd, char**argv)
{
	int i, j, ret;
	pmkid_list_t *pmkid_info;

	if (!*++argv) {
		strcpy(buf, cmd->name);
		if ((ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN)) < 0)
			return ret;

		pmkid_info = (pmkid_list_t *)buf;
		pmkid_info->npmkid = dtoh32(pmkid_info->npmkid);
		printf("\npmkid entries : %d\n", pmkid_info->npmkid);

		for (i = 0; i < (int)pmkid_info->npmkid; i++) {
			printf("\tPMKID[%d]: %s =",
				i, wl_ether_etoa(&pmkid_info->pmkid[i].BSSID));
			for (j = 0; j < WPA2_PMKID_LEN; j++)
				printf("%02x ", pmkid_info->pmkid[i].PMKID[j]);
			printf("\n");
		}
	}
	else {
#ifdef test_pmkid_info
		char eaddr[6] = {0x0, 0x0, 0x1, 0x2, 0x3, 0x5};
		char eaddr1[6] = {0xa, 0xb, 0xc, 0xd, 0xe, 0xf};
		char id[WPA2_PMKID_LEN], id1[WPA2_PMKID_LEN];
		int i, len = (sizeof(uint32) + 2*(sizeof(pmkid_t)));

		/* check that the set uses to "test" cmd */
		if (strcmp(*argv, "test")) {
			printf("\t wl pmkid_info only supports `test` a test specific set\n");
			return BCME_USAGE_ERROR;
		}
		if ((pmkid_info = (pmkid_list_t *)malloc(len)) == NULL)	{
			printf("\tfailed to allocate buffer\n");
			return BCME_NOMEM;
		}

		printf("\toverwriting pmkid table with test pattern\n");
		for (i = 0; i < (int)sizeof(id); i++) {
			id[i] = i;
			id1[i] = (i*2);
		}

		/* "test" - creates two PMKID entries and sets the table to that */
		pmkid_info->npmkid = htod32(2);
		memcpy(&pmkid_info->pmkid[0].BSSID.octet[0], &eaddr[0], ETHER_ADDR_LEN);
		memcpy(&pmkid_info->pmkid[0].PMKID[0], &id[0], WPA2_PMKID_LEN);
		memcpy(&pmkid_info->pmkid[1].BSSID.octet[0], &eaddr1[0], ETHER_ADDR_LEN);
		memcpy(&pmkid_info->pmkid[1].PMKID[0], &id1[0], WPA2_PMKID_LEN);

		ret = wlu_var_setbuf(wl, cmd->name, pmkid_info, len);

		free(pmkid_info);

		return ret;
#else
		printf("\tset cmd ignored\n");
#endif /* test_pmkid_info */
	}

	return 0;
}

/* init fields including version before issueing a get iovar */
static void
wl_rateset_init_fields(wl_rateset_args_u_t* rs, int rsver)
{
	switch (rsver) {
		case RATESET_ARGS_V1:
			/* NO version support. */
			break;
		case RATESET_ARGS_V2:
			rs->rsv2.version = rsver;
			break;
		/* add new versions here */
		default:
			/* nothing needed here */
			break;
	}
}

static int
wl_rateset(void *wl, cmd_t *cmd, char **argv)
{
	wl_rateset_args_u_t rs, defrs;
	int error;
	uint i;
	int rslen = 0, rsver = 0;
	uint32 *rscount = NULL;
	uint8 *rsrates = NULL;
	uint8 *rsmcs = NULL;
	uint16 *rsvht_mcs = NULL;
	uint16 *rshe_mcs = NULL;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if ((error = wl_get_rateset_args_info(wl, &rslen, &rsver)) < 0)
		return (error);
	wl_rateset_init_fields(&rs, rsver);
	wl_rateset_init_fields(&defrs, rsver);

	if (*argv == NULL) {
		/* get current rateset */
			if ((error = wlu_iovar_get(wl, "cur_rateset", &rs, rslen)) < 0)
				return (error);

		wl_rateset_get_fields(&rs, rsver, &rscount, &rsrates, &rsmcs, &rsvht_mcs,
		                      &rshe_mcs);
		dump_rateset(rsrates, dtoh32(*rscount));
		printf("\n");
		wl_print_mcsset((char *)rsmcs);
		wl_print_vhtmcsset((uint16 *)rsvht_mcs);
		if (rsver >= RATESET_ARGS_V2)
			wl_print_hemcsset((uint16 *)rshe_mcs);
	} else {
		/* get default rateset and mcsset */
			if ((error = wlu_iovar_get(wl, "rateset", &defrs, rslen)) < 0)
				return (error);

		wl_rateset_get_fields(&defrs, rsver, &rscount, &rsrates, NULL, NULL, NULL);

		*rscount = dtoh32(*rscount);

		if (!stricmp(*argv, "all")) {
			for (i = 0; i < *rscount; i++)
				rsrates[i] |= 0x80;
			*rscount = htod32(*rscount);

				error = wlu_iovar_set(wl, "rateset", &defrs, rslen);
		} else if (!stricmp(*argv, "default")) {
			*rscount = htod32(*rscount);
				error = wlu_iovar_set(wl, "rateset", &defrs, rslen);
		} else {	/* arbitrary list */
			error = wl_parse_rateset(wl, &defrs, rslen, rsver, argv);
			if (!error) {
				wl_rateset_get_fields(&defrs, rsver, &rscount, &rsrates, NULL,
					NULL, NULL);

				/* check for common error of no basic rates */
				for (i = 0; i < *rscount; i++) {
					if (rsrates[i] & 0x80)
						break;
				}
				if (i < *rscount) {
					*rscount = htod32(*rscount);
					error = wlu_iovar_set(wl, "rateset", &defrs, rslen);

				} else {
					error = BCME_BADARG;
					fprintf(stderr,
					    "Bad Args: at least one rate must be marked Basic\n");
				}
			}
		}

	}
	return (error);
}

static int
wl_default_rateset(void *wl, cmd_t *cmd, char **argv)
{
	int error = 0;
	wl_rates_info_t rates_info;

	UNUSED_PARAMETER(cmd);

	memset((char*)&rates_info.rs_tgt, 0, sizeof(wl_rateset_t));

	argv++;
	/* not enough params */
	if (*argv == NULL) {
		fprintf(stderr, "Can't parse phy type\n");
		return BCME_USAGE_ERROR;
	}

	/* phy_type */
	if (!stricmp(*argv, "a"))
		rates_info.phy_type = 0;
	else if (!stricmp(*argv, "b"))
		rates_info.phy_type = 2;
	else if (!stricmp(*argv, "g"))
		rates_info.phy_type = 2;
	else if (!stricmp(*argv, "n"))
		rates_info.phy_type = 4;
	else if (!stricmp(*argv, "lp"))
		rates_info.phy_type = 5;
	else if (!stricmp(*argv, "ssn"))
		rates_info.phy_type = 6;
	else if (!stricmp(*argv, "ht"))
		rates_info.phy_type = 7;
	else if (!stricmp(*argv, "lcn"))
		rates_info.phy_type = 8;
	else if (!stricmp(*argv, "lcn40"))
		rates_info.phy_type = 10;
	else if (!stricmp(*argv, "ac"))
		rates_info.phy_type = 11;
	else {
		fprintf(stderr, "Wrong phy type: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	argv++;
	/* not enough params */
	if (*argv == NULL) {
		fprintf(stderr, "Can't parse band type\n");
		return BCME_USAGE_ERROR;
	}

	/* band type */
	if (!stricmp(*argv, "5"))
		rates_info.bandtype = WLC_BAND_5G;
	else if (!stricmp(*argv, "2"))
		rates_info.bandtype = WLC_BAND_2G;
	else {
		fprintf(stderr, "Wrong band type: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	argv++;
	/* not enough params */
	if (*argv == NULL) {
		fprintf(stderr, "Can't parse cck\n");
		return BCME_USAGE_ERROR;
	}

	/* cck only */
	if (!stricmp(*argv, "0"))
		rates_info.cck_only = FALSE;
	else if (!stricmp(*argv, "1"))
		rates_info.cck_only = TRUE;
	else {
		fprintf(stderr, "Wrong cck: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	argv++;
	/* not enough params */
	if (*argv == NULL) {
		fprintf(stderr, "Can't parse basic rates\n");
		return BCME_USAGE_ERROR;
	}

	/* rate_mask */
	if (!stricmp(*argv, "0"))
		rates_info.rate_mask = 0x7f;
	else if (!stricmp(*argv, "1"))
		rates_info.rate_mask = 0xff;
	else {
		fprintf(stderr, "Wrong basic rates: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	argv++;
	/* not enough params */
	if (*argv == NULL) {
		fprintf(stderr, "Can't parse mcs\n");
		return BCME_USAGE_ERROR;
	}

	/* mcs */
	if (!stricmp(*argv, "0"))
		rates_info.mcsallow = FALSE;
	else if (!stricmp(*argv, "1"))
		rates_info.mcsallow = TRUE;
	else {
		fprintf(stderr, "Wrong mcs: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	argv++;
	/* not enough params */
	if (*argv == NULL) {
		fprintf(stderr, "Can't parse bandwidth\n");
		return BCME_USAGE_ERROR;
	}

	/* channel bandwidth */
	if (!stricmp(*argv, "10"))
		rates_info.bw = 10;
	else if (!stricmp(*argv, "20"))
		rates_info.bw = 20;
	else if (!stricmp(*argv, "40"))
		rates_info.bw = 40;
	else if (!stricmp(*argv, "80"))
		rates_info.bw = 80;
	else if (!stricmp(*argv, "160"))
		rates_info.bw = 160;
	else {
		fprintf(stderr, "Wrong bandwidth: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	argv++;
	/* not enough params */
	if (*argv == NULL) {
		fprintf(stderr, "Can't parse tx/rx streams\n");
		return BCME_USAGE_ERROR;
	}

	/* mcs */
	if (!stricmp(*argv, "tx")) {
		int txstreams;
		if ((error = wlu_iovar_getint(wl, "txstreams", &txstreams)) < 0) {
			fprintf(stderr, "Can't get tx streams\n");
			return BCME_USAGE_ERROR;
		}
		rates_info.txstreams = txstreams;
	}
	else if (!stricmp(*argv, "rx")) {
		int rxstreams;
		if ((error = wlu_iovar_getint(wl, "rxstreams", &rxstreams)) < 0) {
			fprintf(stderr, "Can't get rx streams\n");
			return BCME_USAGE_ERROR;
		}
		rates_info.txstreams = rxstreams;
	}
	else {
		fprintf(stderr, "Wrong tx/rx streams: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* get default rates */
	if ((error = wlu_iovar_getbuf(wl, "default_rateset", NULL, 0, &rates_info,
		sizeof(wl_rates_info_t)))) {
		fprintf(stderr, "default_rateset failed\n");
		return (error);
	}

	dump_rateset(rates_info.rs_tgt.rates, dtoh32(rates_info.rs_tgt.count));

	return (error);
}

static int
wl_txbf_rateset(void *wl, cmd_t *cmd, char **argv)
{
	wl_txbf_rateset_t rs;
	int error = 0;

	argv++;

	if ((error = wlu_iovar_get(wl, cmd->name, &rs, sizeof(rs))) < 0)
		return (error);
	/* Get txbf rateset */
	if (*argv == NULL) {
		printf("    OFDM: ");
		dump_rateset(rs.txbf_rate_ofdm, dtoh32(rs.txbf_rate_ofdm_cnt));
		printf("\n");
		wl_print_txbf_mcsset((char *)rs.txbf_rate_mcs, "   ");
		wl_print_txbf_vhtmcsset((uint16 *)rs.txbf_rate_vht, "   ");

		printf("BCM OFDM: ");
		dump_rateset(rs.txbf_rate_ofdm_bcm, dtoh32(rs.txbf_rate_ofdm_cnt_bcm));
		printf("\n");
		wl_print_txbf_mcsset((char *)rs.txbf_rate_mcs_bcm, "BCM");
		wl_print_txbf_vhtmcsset((uint16 *)rs.txbf_rate_vht_bcm, "BCM");
	} else {	/* Set txbf rateset */
		error = wl_parse_txbf_rateset(&rs, argv);
		if (!error) {
			rs.txbf_rate_ofdm_cnt     = htod32(rs.txbf_rate_ofdm_cnt);
			rs.txbf_rate_ofdm_cnt_bcm = htod32(rs.txbf_rate_ofdm_cnt_bcm);
			error = wlu_iovar_set(wl, cmd->name, &rs, sizeof(rs));
		}
	}
	return (error);
}

static int
wl_sup_rateset(void *wl, cmd_t *cmd, char **argv)
{
	wl_rateset_args_u_t rs;
	bool got_basic;
	int error;
	uint i;
	int rslen = 0, rsver = 0;
	uint32 *rscount = NULL;
	uint8 *rsrates = NULL;
	uint8 *rsmcs = NULL;
	uint16 *rsvht_mcs = NULL;

	error = 0;
	memset((char*)&rs, 0, sizeof(wl_rateset_args_u_t));

	argv++;

	if ((error = wl_get_rateset_args_info(wl, &rslen, &rsver)) < 0)
		return (error);
	wl_rateset_get_fields(&rs, rsver, &rscount, &rsrates, &rsmcs, &rsvht_mcs, NULL);

	if (*argv == NULL) {
		/* get rateset */
		if ((error = wlu_get(wl, cmd->get, &rs, rslen)) < 0)
			return (error);

		dump_rateset(rsrates, dtoh32(*rscount));
		printf("\n");
	} else {
		if (!stricmp(*argv, "-1") || !stricmp(*argv, "0")) {
			/* set an empty rateset */
			error = wlu_set(wl, cmd->set, &rs, rslen);
		}
		else {	/* set the specified rateset */
			wl_parse_rateset(wl, &rs, rslen, rsver, argv);
			/* check for common error of including a basic rate */
			got_basic = FALSE;
			for (i = 0; i < *rscount; i++) {
				if (rsrates[i] & 0x80) {
					rsrates[i] &= 0x7F;
					got_basic = TRUE;
				}
			}
			if (got_basic) {
				fprintf(stderr,
				"Warning: Basic rate attribute ignored for \"%s\" command\n",
				cmd->name);
			}
			*rscount = htod32(*rscount);
			error = wlu_set(wl, cmd->set, &rs, rslen);
		}

	}
	return (error);
}

/*
 * Parse the rateset command arguments into the passed wl_rateset_args_t structure.
 *
 * Returns 0 on success, or an appropriate error code (BCME_USAGE_ERROR, BCME_BADARG).
 *
 */
static int
wl_parse_rateset(void *wl, wl_rateset_args_u_t* rs, int rslen, int rsver, char **argv)
{
	char* endp = NULL;
	char* arg;
	int r;
	int mcs_index = 0;
	uint32 mcs_mask;
	int error = 0;
	wl_rateset_args_u_t cur_rs;
	bool mcs_args, vht_args, he_args;

	uint32 *rscount = NULL;
	uint8 *rsrates = NULL;
	uint8 *rsmcs = NULL;
	uint16 *rsvht_mcs = NULL;
	uint16 *rshe_mcs = NULL;

	uint32 *cur_count = NULL;
	uint8 *cur_rsrates = NULL;
	uint8 *cur_mcs = NULL;
	uint16 *cur_vht_mcs = NULL;
	uint16 *cur_he_mcs = NULL;

	mcs_args = vht_args = he_args = FALSE;

	memset(rs, 0, rslen);

	/* init version/length fields */
	if (rsver == RATESET_ARGS_V2) {
		rs->rsv2.version = rsver;
		rs->rsv2.len = sizeof(rs->rsv2);
	}

	wl_rateset_get_fields(rs, rsver, &rscount, &rsrates, &rsmcs, &rsvht_mcs, &rshe_mcs);
	wl_rateset_get_fields(&cur_rs, rsver, &cur_count, &cur_rsrates, &cur_mcs, &cur_vht_mcs,
		&cur_he_mcs);

	while ((arg = *argv++) != NULL) {
		/* mcs rates */
		if (!stricmp(arg, "-m")) {
			mcs_args = TRUE;
			break;
		}

		/* vht rates */
		if (!stricmp(arg, "-v")) {
			vht_args = TRUE;
			break;
		}

		/* he rates */
		if (!stricmp(arg, "-e")) {
			he_args = TRUE;
			break;
		}

		/* Parse legacy rates */

		if (*rscount >= WL_MAXRATES_IN_SET) {
			fprintf(stderr,
			"parsing \"%s\", too many rates specified, max is %d rates\n",
			arg, WL_MAXRATES_IN_SET);
			error = BCME_USAGE_ERROR;
			break;
		}

		/* convert the rate number to a 500kbps rate by multiplying by 2 */
		r = (int)(strtoul(arg, &endp, 0) * 2);
		if (endp == arg) {
			fprintf(stderr, "unable to convert the rate parameter \"%s\"\n", arg);
			error = BCME_USAGE_ERROR;
			break;
		}

		/* parse a .5 specially */
		if (!strncmp(endp, ".5", 2)) {
			r += 1;
			endp += 2;
		}

		/* strip trailing space */
		while (isspace((int)endp[0]))
			endp++;

		/* check for a basic rate specifier */
		if (!stricmp(endp, "b") || !stricmp(endp, "(b)")) {
			r |= 0x80;
		} else if (endp[0] != '\0') {
			fprintf(stderr,
				"unable to convert trailing characters"
				" \"%s\" in the rate parameter \"%s\"\n",
				endp, arg);
			error = BCME_USAGE_ERROR;
			break;
		}

		/* no legacy rates specified */
		if ((*rscount == 0) && (r == 0)) {
			fprintf(stderr, "empty legacy rateset not supported\n");
			error = BCME_USAGE_ERROR;
			break;
		}

		rsrates[*rscount] = r;
		*rscount = *rscount + 1;
	}

	if (error)
		return error;

	if (!mcs_args && !vht_args && !he_args && !*rscount)
		return BCME_USAGE_ERROR; /* Cannot happen, really */

	/*
	 * If one of the rate sets was not specified, keep its current setting.
	 */

	error = wlu_iovar_get(wl, "cur_rateset", &cur_rs, rslen);
	if (error)
		return error;

	if (!*rscount) { /* No legacy rates specified -- keep what we have */
		*rscount = *cur_count;
		memcpy(rsrates, cur_rsrates, *rscount);
	}

	if (!mcs_args) { /* No MCS rates specified */
		memcpy(rsmcs, cur_mcs, MCSSET_LEN);
	}

	if (!vht_args) { /* No VHT rates specified, keep current values */
		memcpy(rsvht_mcs, cur_vht_mcs,
		       VHT_CAP_MCS_MAP_NSS_MAX * sizeof(rsvht_mcs[0]));
	}

	if (rsver == RATESET_ARGS_V2) {
		if (!he_args) { /* No HE rates specified, keep current values */
			memcpy(rshe_mcs, cur_he_mcs,
			       HE_CAP_MCS_MAP_NSS_MAX * sizeof(rshe_mcs[0]));
		}
	}

	/* If no more args, return. */

	if (!arg) {
		return error;
	}

	/* Parse mcs or VHT rateset values */

	while ((arg = *argv++) != NULL) {

		if (mcs_args) {

			if (mcs_index >= MCSSET_LEN) {
				fprintf(stderr, "parsing \"%s\", too many mcs rates "
				        "specified, max is %d rates\n", arg, MCSSET_LEN);
				error = BCME_USAGE_ERROR;
				break;
			}

			mcs_mask = strtoul(arg, &endp, 16);

			if (endp == arg) {
				fprintf(stderr, "unable to convert the mcs parameter \"%s\"\n",
				        arg);
				error = BCME_BADARG;
				break;
			}

			/* clear the mcs rates */
			if (mcs_mask == 0) {
				memset(rsmcs, 0, MCSSET_LEN);
				break;
			}

			/* copy the mcs rates bitmap octets */
			rsmcs[mcs_index++] = mcs_mask;

		} else if (vht_args) { /* vht_args */

			/*
			 * Specified as rate masks for Nss=0, Nss=1, etc.
			 */
			if (mcs_index >= VHT_CAP_MCS_MAP_NSS_MAX) {
				fprintf(stderr,
					"Error: Too many VHT rate masks specified, max %d\n",
					VHT_CAP_MCS_MAP_NSS_MAX);
				error = BCME_USAGE_ERROR;
				break;
			}

			mcs_mask = strtoul( arg, &endp, 16 ); /* Base 16 for consistency with -m */

			if ((*arg == '\0') || (*endp != '\0')) {
				fprintf(stderr, "Error converting VHT rate mask value '%s'\n", arg);
				error = BCME_USAGE_ERROR;
				break;
			}

			/*
			 * Can only specify 0, 0xff, 0x1ff, 0x3ff because of the way the rates
			 * are encoded in the driver (0-3).
			 */
			if ((mcs_mask != 0x0000) &&   /* vht disabled */
			    (mcs_mask != VHT_CAP_MCS_0_7_RATEMAP) &&   /* vht mcs0-7 */
			    (mcs_mask != VHT_CAP_MCS_0_8_RATEMAP) &&   /* vht mcs0-8 */
			    (mcs_mask != VHT_CAP_MCS_0_9_RATEMAP) &&   /* vht mcs0-9 */
			    (mcs_mask != (VHT_CAP_MCS_0_9_RATEMAP |
			                  VHT_PROP_MCS_10_11_RATEMAP))) {   /* vht mcs0-11 */
				fprintf(stderr, "Error: VHT rate mask must be 0 (disabled),"
				        " 0xff (MCS0-7), 0x1ff (MCS0-8), 0x3ff (MCS0-9) or "
				        "0xfff (MCS0-11).\n");
				error = BCME_BADARG;
				break;
			}

			rsvht_mcs[mcs_index++] = mcs_mask;
		} else { /* he_args */
			/*
			 * Specified as rate masks for Nss=0, Nss=1, etc.
			 */
			if (mcs_index >= WL_HE_CAP_MCS_MAP_NSS_MAX) {
				fprintf(stderr,
					"Error: Too many HE rate masks specified, max %d\n",
					WL_HE_CAP_MCS_MAP_NSS_MAX);
				error = BCME_USAGE_ERROR;
				break;
			}

			mcs_mask = strtoul( arg, &endp, 16 ); /* Base 16 for consistency with -x */

			if ((*arg == '\0') || (*endp != '\0')) {
				fprintf(stderr, "Error converting VHT rate mask value '%s'\n", arg);
				error = BCME_USAGE_ERROR;
				break;
			}

			/*
			 * Can only specify 0, 0xff, 0x3ff, 0xfff because of the way the rates
			 * are encoded in the driver (0-3).
			 */
			if ((mcs_mask != 0x0000) &&   /* he disabled */
			    (mcs_mask != WL_HE_CAP_MCS_0_7_MAP) &&   /* he mcs0-7 */
			    (mcs_mask != WL_HE_CAP_MCS_0_9_MAP) &&   /* he mcs0-9 */
			    (mcs_mask != WL_HE_CAP_MCS_0_11_MAP)) {  /* he mcs0-11 */
				fprintf(stderr, "Error: HE rate mask must be 0 (disabled),"
				        " 0xff (MCS0-7), 0x3ff (MCS0-9) or 0xfff (MCS0-11).\n");
				error = BCME_BADARG;
				break;
			}

			rshe_mcs[mcs_index++] = mcs_mask;
		}
	}

	return error;
}

static int
wl_parse_txbf_rateset(wl_txbf_rateset_t *rs, char **argv)
{
#define TXBF_RATE_OFDM	0
#define TXBF_RATE_MCS	1
#define TXBF_RATE_VHT	2
	char* endp = NULL;
	char* arg;
	int i;
	int error = 0;
	int rate_type = TXBF_RATE_OFDM;
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];
	uint8	txbf_rate_mcs_cnt = 0;
	uint8	mcs_mask;
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];
	uint8	txbf_rate_vht_cnt = 0;
	uint16	vht_mask;
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];
	uint8	txbf_rate_ofdm_cnt;
	int r;
	bool specified_ofdm = FALSE;
	bool specified_mcs  = FALSE;
	bool specified_vht  = FALSE;
	bool specified_bcm  = FALSE;

	/* First see if ofdm list */
	arg = *argv;
	if (stricmp(arg, "-m") != 0 && stricmp(arg, "-v") != 0) {
		rate_type = TXBF_RATE_OFDM;
		specified_ofdm = TRUE;
		txbf_rate_ofdm_cnt = 0;
	}

	while ((arg = *argv++) != NULL) {
		/* mcs rates */
		if (!stricmp(arg, "-m")) {
			rate_type = TXBF_RATE_MCS;
			specified_mcs = TRUE;
			txbf_rate_mcs_cnt = 0;
			continue;
		}
		/* vht rates */
		if (!stricmp(arg, "-v")) {
			rate_type = TXBF_RATE_VHT;
			specified_vht = TRUE;
			txbf_rate_vht_cnt = 0;
			continue;
		}
		/* Broadcom-to-Broadcom group */
		if (!stricmp(arg, "-b")) {
			specified_bcm = TRUE;
			continue;
		}

		if (rate_type == TXBF_RATE_OFDM) {
			/* Check if too many specified ofdm rates */
			if (txbf_rate_ofdm_cnt >= TXBF_RATE_OFDM_ALL) {
				fprintf(stderr, "ERR: more than max. of %d ofdm rates\n",
					TXBF_RATE_OFDM_ALL);
				return (BCME_USAGE_ERROR);
			}
			/* Convert user typed number to a 500kbps rate by multiplying by 2 */
			r = (int)(strtoul(arg, &endp, 0) * 2);
			if (endp == arg) {
				fprintf(stderr, "ERR: failed to convert %s\n", arg);
				return (BCME_USAGE_ERROR);
			}
			/* Check if valid ofdm rate */
			for (i = 0; i < (int)sizeof(ofdm_rates); i++) {
				/* match: it is a valid ofdm rate */
				if (r == (ofdm_rates[i] & OFDM_RATE_MASK)) {
					r = ofdm_rates[i]; /* will pick up basic rate bit too */
					break;
				}
			}
			if (i >= (int)sizeof(ofdm_rates)) {
				fprintf(stderr, "ERR: %s is an invalid ofdm rate\n", arg);
				return (BCME_USAGE_ERROR);
			}

			/* Strip trailing space */
			while (isspace((int)endp[0]))
				endp++;
			/* No ofdm rate specified */
			if ((rs->txbf_rate_ofdm_cnt == 0) && (r == 0)) {
				fprintf(stderr, "ERR: no ofdm rate specified\n");
				return (BCME_USAGE_ERROR);
			}
			txbf_rate_ofdm[txbf_rate_ofdm_cnt++] = r;
		} else if (rate_type == TXBF_RATE_MCS) {
			/* Check if exceeding max */
			if (txbf_rate_mcs_cnt >= TXBF_RATE_MCS_ALL) {
				fprintf(stderr, "ERR: exceed max. %d bitmask bytes; parsing %s\n",
					TXBF_RATE_MCS_ALL, arg);
				return (BCME_BADARG);
			}
			mcs_mask = (uint8)strtoul(arg, &endp, 16);
			if (endp == arg) {
				fprintf(stderr, "ERR: failed to convert %s\n", arg);
				return (BCME_USAGE_ERROR);
			}
			txbf_rate_mcs[txbf_rate_mcs_cnt++] = mcs_mask;
		} else if (rate_type == TXBF_RATE_VHT) {
			/* Check if exceeding max */
			if (txbf_rate_vht_cnt >= TXBF_RATE_VHT_ALL) {
				fprintf(stderr, "ERR: exceed max. %d bitmasks; parsing %s\n",
					TXBF_RATE_VHT_ALL, arg);
				return (BCME_BADARG);
			}
			vht_mask = (uint16)strtoul(arg, &endp, 16);
			if (endp == arg) {
				fprintf(stderr, "ERR: failed to convert %s\n", arg);
				return (BCME_USAGE_ERROR);
			}

			/* Validate bitmask value */
			if (vht_mask > 0x0fff) {
				fprintf(stderr, "ERR: vht bitmask must be 0 (disabled)"
						" or up to a maximum of 0xfff (MCS0-11).\n");
				return (BCME_BADARG);
			}
			txbf_rate_vht[txbf_rate_vht_cnt++] = vht_mask;
		}

	} /* while */

	if (specified_mcs && !specified_bcm) {
		for (i = 0; i < txbf_rate_mcs_cnt; i++)
			rs->txbf_rate_mcs[i] = txbf_rate_mcs[i];
		if (txbf_rate_mcs_cnt)
			for (i = txbf_rate_mcs_cnt; i < TXBF_RATE_MCS_ALL; i++)	/* clear trailer */
				rs->txbf_rate_mcs[i] = 0;
	}

	if (specified_vht && !specified_bcm) {
		for (i = 0; i < txbf_rate_vht_cnt; i++)
			rs->txbf_rate_vht[i] = txbf_rate_vht[i];
		if (txbf_rate_vht_cnt)
			for (i = txbf_rate_vht_cnt; i < TXBF_RATE_VHT_ALL; i++)	/* clear trailer */
				rs->txbf_rate_vht[i] = 0;
	}

	if (specified_ofdm && !specified_bcm) {
		for (i = 0; i < txbf_rate_ofdm_cnt; i++)
			rs->txbf_rate_ofdm[i] = txbf_rate_ofdm[i];
		rs->txbf_rate_ofdm_cnt = txbf_rate_ofdm_cnt;
	}

	if (specified_mcs && specified_bcm) {
		for (i = 0; i < txbf_rate_mcs_cnt; i++)
			rs->txbf_rate_mcs_bcm[i] = txbf_rate_mcs[i];
		if (txbf_rate_mcs_cnt)
			for (i = txbf_rate_mcs_cnt; i < TXBF_RATE_MCS_ALL; i++)	/* clear trailer */
				rs->txbf_rate_mcs_bcm[i] = 0;
	}

	if (specified_vht && specified_bcm) {
		for (i = 0; i < txbf_rate_vht_cnt; i++)
			rs->txbf_rate_vht_bcm[i] = txbf_rate_vht[i];
		if (txbf_rate_vht_cnt)
			for (i = txbf_rate_vht_cnt; i < TXBF_RATE_VHT_ALL; i++)	/* clear trailer */
				rs->txbf_rate_vht_bcm[i] = 0;
	}

	if (specified_ofdm && specified_bcm) {
		for (i = 0; i < txbf_rate_ofdm_cnt; i++)
			rs->txbf_rate_ofdm_bcm[i] = txbf_rate_ofdm[i];
		rs->txbf_rate_ofdm_cnt_bcm = txbf_rate_ofdm_cnt;
	}

	return (error);
}

/*
 * Get or Set LPC Params
 *	wl lpc_params \
 *		<rate_stab_thresh> <pwr_stab_thresh> <lpc_exp_time> <pwrup_slow_step>
 *           <pwrup_fast_step> <pwrdn_slow_step>
 */
static int
wl_power_sel_params(void *wl, cmd_t *cmd, char **argv)
{
	int err, argc;
	bool lpc_params_use_powersel_params;
	lpc_params_t lpc_params;

	UNUSED_PARAMETER(cmd);

	lpc_params_use_powersel_params = (wlc_ver_major(wl) <= 5);

	/* handle old powersel_params_t format support */
	if (lpc_params_use_powersel_params) {
		powersel_params_t pwrsel_params;

		argv++;

		if (*argv == NULL) {
			/* get current powersel params */
			if ((err = wlu_iovar_get(wl, cmd->name, (void *) &pwrsel_params,
				(sizeof(powersel_params_t)))) < 0)
				return (err);

			printf("- Link Power Control parameters -\n");
			printf("tp_ratio_thresh\t\t= %d\nrate_stab_thresh\t= %d\n",
				pwrsel_params.tp_ratio_thresh, pwrsel_params.rate_stab_thresh);
			printf("pwr_stab_thresh\t\t= %d\npwr_sel_exp_time\t= %d\n",
				pwrsel_params.pwr_stab_thresh, pwrsel_params.pwr_sel_exp_time);
		} else {
			char *endptr;
			/* Validate num of entries */
			for (argc = 0; argv[argc]; argc++);
			if (argc != 4)
				return BCME_USAGE_ERROR;

			argc = 0;
			pwrsel_params.tp_ratio_thresh = strtol(argv[argc], &endptr, 0);
			argc++;
			pwrsel_params.rate_stab_thresh = strtol(argv[argc], &endptr, 0);
			argc++;
			pwrsel_params.pwr_stab_thresh = strtol(argv[argc], &endptr, 0);
			argc++;
			pwrsel_params.pwr_sel_exp_time = strtol(argv[argc], &endptr, 0);

			/* Set powersel params */
			err = wlu_iovar_set(wl, cmd->name, (void *) &pwrsel_params,
				(sizeof(powersel_params_t)));
		}
		return err;
	}

	/* handle new lpc_params_t format support */
	argv++;

	if (*argv == NULL) {
		/* get current powersel params */
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) &lpc_params,
			(sizeof(lpc_params_t)))) < 0)
			return (err);

		printf("- Link Power Control parameters (ver%d) -\n",
			lpc_params.version);
		printf("rate_stab_thresh\t= %d\npwr_stab_thresh\t\t= %d\n",
			lpc_params.rate_stab_thresh, lpc_params.pwr_stab_thresh);
		printf("lpc_exp_time\t\t= %d\npwrup_slow_step\t\t= %d\n",
			lpc_params.lpc_exp_time, lpc_params.pwrup_slow_step);
		printf("pwrup_fast_step\t\t= %d\npwrdn_slow_step\t\t= %d\n",
			lpc_params.pwrup_fast_step, lpc_params.pwrdn_slow_step);
	} else {
		char *endptr;
		/* Validate num of entries */
		for (argc = 0; argv[argc]; argc++);
		if (argc != 6)
			return BCME_USAGE_ERROR;

		lpc_params.version = WL_LPC_PARAMS_CURRENT_VERSION;
		lpc_params.length = sizeof(lpc_params);

		argc = 0;
		lpc_params.rate_stab_thresh = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwr_stab_thresh = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.lpc_exp_time = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwrup_slow_step = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwrup_fast_step = strtol(argv[argc], &endptr, 0);
		argc++;
		lpc_params.pwrdn_slow_step = strtol(argv[argc], &endptr, 0);

		/* Set powersel params */
		err = wlu_iovar_set(wl, cmd->name, (void *) &lpc_params,
			(sizeof(lpc_params_t)));
	}

	return err;
}

static int
wl_channel(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	channel_info_t ci;

	if (!*++argv) {
		memset(&ci, 0, sizeof(ci));
		if ((ret = wlu_get(wl, cmd->get, &ci, sizeof(channel_info_t))) < 0)
			return ret;
		ci.hw_channel = dtoh32(ci.hw_channel);
		ci.scan_channel = dtoh32(ci.scan_channel);
		ci.target_channel = dtoh32(ci.target_channel);
		if (ci.scan_channel) {
			printf("Scan in progress.\n");
			printf("current scan channel\t%d\n", ci.scan_channel);
		} else {
			printf("No scan in progress.\n");
		}
		printf("current mac channel\t%d\n", ci.hw_channel);
		printf("target channel\t%d\n", ci.target_channel);
		return 0;
	} else {
		ci.target_channel = htod32(atoi(*argv));
		ret =  wlu_set(wl, cmd->set, &ci.target_channel, sizeof(int));
		return ret;
	}
}

static int
wl_chanspec(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_chanspec";
	bool band_set = FALSE, ch_set = FALSE, bw_set = FALSE, ctl_sb_set = FALSE;
	int err, opt_err;
	uint32 val = 0;
	chanspec_t chanspec = 0;

	/* toss the command name */
	argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_getint(wl, cmd->name, (int*)&val)) < 0)
			return err;

		chanspec = wl_chspec32_from_driver(val);
		wf_chspec_ntoa(chanspec, buf);
		printf("%s (0x%x)\n", buf, chanspec);
		return 0;
	}

	chanspec = wf_chspec_aton(*argv);
	if (chanspec != 0) {
		val = wl_chspec32_to_driver(chanspec);
		if (val != INVCHANSPEC) {
			err = wlu_iovar_setint(wl, cmd->name, val);
		} else {
			err = BCME_USAGE_ERROR;
		}
	} else {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'c') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" the channel\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val > 224) {
					fprintf(stderr, "%s: invalid channel %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				chanspec |= to.val;
				ch_set = TRUE;
			}
			if (to.opt == 'b') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for band\n",
						fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 5) && (to.val != 2)) {
					fprintf(stderr,
						"%s: invalid band %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 5)
					chanspec |= WL_CHANSPEC_BAND_5G;
				else
					chanspec |= WL_CHANSPEC_BAND_2G;
				band_set = TRUE;
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" bandwidth\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 20) && (to.val != 40) && (to.val != 80) &&
					(to.val != 160)) {
					fprintf(stderr,
						"%s: invalid bandwidth %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 20)
					chanspec |= WL_CHANSPEC_BW_20;
				else if (to.val == 40)
					chanspec |= WL_CHANSPEC_BW_40;
				else if (to.val == 80)
					chanspec |= WL_CHANSPEC_BW_80;
				else
					chanspec |= WL_CHANSPEC_BW_160;
				bw_set = TRUE;
			}
			if (to.opt == 's') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" ctl sideband\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 1) && (to.val != 0) && (to.val != -1)) {
					fprintf(stderr,
						"%s: invalid ctl sideband %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == -1)
					chanspec |= WL_CHANSPEC_CTL_SB_LOWER;
				else if (to.val == 1)
					chanspec |= WL_CHANSPEC_CTL_SB_UPPER;
				ctl_sb_set = TRUE;
			}
		}

		/* set ctl sb to 20 if not set and 20mhz is selected */
		if (!ctl_sb_set && CHSPEC_IS20(chanspec)) {
			ctl_sb_set = TRUE;
		}

		if (ch_set && band_set && bw_set && ctl_sb_set) {
			val = wl_chspec32_to_driver(chanspec);
			if (val != INVCHANSPEC) {
				err = wlu_iovar_setint(wl, cmd->name, val);
			} else {
				err = BCME_USAGE_ERROR;
			}
		} else {
			if (!ch_set)
				fprintf(stderr, "%s: you need to set a channel,"
					" '-c <ch>'\n", fn_name);
			if (!band_set)
				fprintf(stderr, "%s: you need to set a band,"
					" '-b <5|2>'\n", fn_name);
			if (!bw_set)
				fprintf(stderr, "%s: you need to set a bandwidth,"
					" '-w <20|40>'\n", fn_name);
			if (!ctl_sb_set)
				fprintf(stderr, "%s: you need to set a ctl sideband,"
					  " '-s <-1|0|1>'\n", fn_name);
			err = BCME_USAGE_ERROR;
		}
	}

	if (!err)
		printf("Chanspec set to 0x%x\n", chanspec);

exit:
	return err;
}

static int
wl_sc_chan(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_sc_chan";
	bool band_set = FALSE, ch_set = FALSE, bw_set = FALSE, ctl_sb_set = FALSE;
	int err, opt_err;
	uint32 val = 0;
	chanspec_t chanspec = 0;

	/* toss the command name */
	argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_getint(wl, cmd->name, (int*)&val)) < 0)
			return err;

		chanspec = wl_chspec32_from_driver(val);
		wf_chspec_ntoa(chanspec, buf);
		printf("%s (0x%x)\n", buf, chanspec);
		return 0;
	}

	chanspec = wf_chspec_aton(*argv);
	if (chanspec != 0) {
		val = wl_chspec32_to_driver(chanspec);
		if (val != INVCHANSPEC) {
			err = wlu_iovar_setint(wl, cmd->name, val);
		} else {
			err = BCME_USAGE_ERROR;
		}
	} else {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'c') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" the channel\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val > 224) {
					fprintf(stderr, "%s: invalid channel %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				chanspec |= to.val;
				ch_set = TRUE;
			}
			if (to.opt == 'b') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for band\n",
						fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 5) && (to.val != 2)) {
					fprintf(stderr,
						"%s: invalid band %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 5)
					chanspec |= WL_CHANSPEC_BAND_5G;
				else
					chanspec |= WL_CHANSPEC_BAND_2G;
				band_set = TRUE;
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" bandwidth\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 20) && (to.val != 40) && (to.val != 80)) {
					fprintf(stderr,
						"%s: invalid bandwidth %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 20)
					chanspec |= WL_CHANSPEC_BW_20;
				else if (to.val == 40)
					chanspec |= WL_CHANSPEC_BW_40;
				else
					chanspec |= WL_CHANSPEC_BW_80;
				bw_set = TRUE;
			}
			if (to.opt == 's') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" ctl sideband\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 1) && (to.val != 0) && (to.val != -1)) {
					fprintf(stderr,
						"%s: invalid ctl sideband %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == -1)
					chanspec |= WL_CHANSPEC_CTL_SB_LOWER;
				else if (to.val == 1)
					chanspec |= WL_CHANSPEC_CTL_SB_UPPER;
				ctl_sb_set = TRUE;
			}
		}

		/* set ctl sb to 20 if not set and 20mhz is selected */
		if (!ctl_sb_set && CHSPEC_IS20(chanspec)) {
			ctl_sb_set = TRUE;
		}

		if (ch_set && band_set && bw_set && ctl_sb_set) {
			val = wl_chspec32_to_driver(chanspec);
			if (val != INVCHANSPEC) {
				err = wlu_iovar_setint(wl, cmd->name, val);
			} else {
				err = BCME_USAGE_ERROR;
			}
		} else {
			if (!ch_set)
				fprintf(stderr, "%s: you need to set a channel,"
					" '-c <ch>'\n", fn_name);
			if (!band_set)
				fprintf(stderr, "%s: you need to set a band,"
					" '-b <5|2>'\n", fn_name);
			if (!bw_set)
				fprintf(stderr, "%s: you need to set a bandwidth,"
					" '-w <20|40>'\n", fn_name);
			if (!ctl_sb_set)
				fprintf(stderr, "%s: you need to set a ctl sideband,"
					  " '-s <-1|0|1>'\n", fn_name);
			err = BCME_USAGE_ERROR;
		}
	}

	if (!err)
		printf("SC Chanspec set to 0x%x\n", chanspec);

exit:
	return err;
}

static int
wl_phy_vcore(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint32 val = 0;
	uint8 bw80p80_cap  = 0;
	uint8 ncore = 0;
	uint8 hw_swmask_txchain = 0;
	uint8 hw_swmask_rxchain = 0;
	bool tx_80p80_valid = FALSE;
	bool rx_80p80_valid = FALSE;

	/* toss the command name */
	argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
		if ((err = wlu_iovar_getint(wl, cmd->name, (int*)&val)) < 0)
			return err;
		bw80p80_cap = (val >> 12) & 0x1;
		/*
		*	assume maximum core number is 4
		*	bit extension is needed for future > 4-core design
		*/
		ncore = (val >> 8) & 0xf;
		hw_swmask_txchain  = (val >> 4) & 0xf;
		hw_swmask_rxchain  = val & 0xf;
		printf("REAL160:NO\n");
		//FIXME if real160 is supported in the future

		if (bw80p80_cap == 1) {
			if (ncore == 4) {
				/* 80p80 for 4-core, 4365 */
				tx_80p80_valid |= hw_swmask_txchain == 0xf;
				tx_80p80_valid |= hw_swmask_txchain == 0xa;
				tx_80p80_valid |= hw_swmask_txchain == 0x5;
				rx_80p80_valid |= hw_swmask_rxchain == 0xf;
				rx_80p80_valid |= hw_swmask_rxchain == 0xa;
				rx_80p80_valid |= hw_swmask_rxchain == 0x5;
				if (!(tx_80p80_valid) || !(rx_80p80_valid)) {
					printf("80p80:NO\n");
				} else {
					printf("80p80:YES\n");
					if (hw_swmask_txchain == 0xf) {
						printf("tx_vcore=%d txchain= %d\n", 1, 5);
						printf("tx_vcore=%d txchain=%d\n", 2, 10);
						printf("tx_vcore=%d txchain=%d\n", 3, 15);
					} else if (hw_swmask_txchain == 0xa) {
						printf("tx_vcore=%d txchain=%d\n", 2, 10);
					} else {
						printf("tx_vcore=%d txchain= %d\n", 1, 5);
					}
					if (hw_swmask_rxchain == 0xf) {
						printf("rx_vcore=%d rxchain= %d\n", 1, 5);
						printf("rx_vcore=%d rxchain=%d\n", 2, 10);
						printf("rx_vcore=%d rxchain=%d\n", 3, 15);
					} else if (hw_swmask_rxchain == 0xa) {
						printf("rx_vcore=%d rxchain=%d\n", 2, 10);
					} else {
						printf("rx_vcore=%d rxchain= %d\n", 1, 5);
					}
				}
			} else if (ncore == 2) {
				/* 80p80 for 2-core, 4349 */
				tx_80p80_valid |= hw_swmask_txchain == 0x3;
				rx_80p80_valid |= hw_swmask_rxchain == 0x3;

				if (!(tx_80p80_valid) || !(rx_80p80_valid)) {
					printf("80p80:NO\n");
				} else {
					printf("80p80:YES\n");
					printf("tx_vcore=%d txchain= %d\n", 1, 3);
					printf("rx_vcore=%d rxchain= %d\n", 1, 3);
				}
			} else {
				printf("!!!Number of cores should be 2 or 4!!!\n");
			}
		} else {
			printf("80p80:NO\n");
		}
		return 0;
	} else {
		return BCME_UNSUPPORTED;
	}
}

static int
wl_dfs_ap_move(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	void *ptr;
	const char* fn_name = "wl_dfs_ap_move";
	bool band_set = FALSE, ch_set = FALSE, bw_set = FALSE, ctl_sb_set = FALSE;
	int err, opt_err;
	uint32 val = 0;
	chanspec_t chanspec = 0;
	int arg1;
	struct wl_dfs_ap_move_status_v2 *status_v2;

	char chanbuf[CHANSPEC_STR_LEN];
	const char *dfs_state_str[DFS_SCAN_S_MAX] = {
		"Radar Free On Channel",
		"Radar Found On Channel",
		"Radar Scan In Progress",
		"Radar Scan Aborted",
		"RSDB Mode switch in Progress For Scan"
	};

	/* toss the command name */
	argv++;

	/* GET */
	if (!*argv) {
		if (cmd->get < 0)
			return -1;

		if ((err = wlu_var_getbuf_sm(wl, cmd->name, NULL, 0, &ptr))) {
			printf("err=%d \n", err);
			return err;
		}

		status_v2 = (struct wl_dfs_ap_move_status_v2*)ptr;

		if (status_v2->version != WL_DFS_AP_MOVE_VERSION) {
			err = BCME_UNSUPPORTED;
			printf("err=%d version=%d\n", err, status_v2->version);
			return err;
		}

		printf("version=%d, move status=%d\n", status_v2->version,
			status_v2->move_status);

		if (status_v2->move_status != (int8) DFS_SCAN_S_IDLE) {
			chanspec = wl_chspec32_from_driver(status_v2->chanspec);
			if (chanspec != 0 && chanspec != INVCHANSPEC) {
				wf_chspec_ntoa(chanspec, chanbuf);
				printf("AP Target Chanspec %s (0x%x)\n", chanbuf, chanspec);
			}
			printf("%s\n", dfs_state_str[status_v2->move_status]);
			err = wl_print_dfs_status_all(&status_v2->scan_status);

		} else {
			printf("dfs AP move in IDLE state\n");
			err = wl_print_dfs_status_all(&status_v2->scan_status);
		}

		return err;
	}

	/* SET */
	arg1 = atoi(*argv);
	chanspec = wf_chspec_aton(*argv);

	if (arg1 == WL_DFS_AP_MOVE_ABORT || arg1 == WL_DFS_AP_MOVE_STUNT) {
		err = wlu_iovar_setint(wl, cmd->name, arg1);
	} else if (chanspec != 0) {
		val = wl_chspec32_to_driver(chanspec);
		if (val != INVCHANSPEC) {
			err = wlu_iovar_setint(wl, cmd->name, val);
		} else {
			err = BCME_USAGE_ERROR;
		}
	} else {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'c') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" the channel\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val > 224) {
					fprintf(stderr, "%s: invalid channel %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				chanspec |= to.val;
				ch_set = TRUE;
			}
			if (to.opt == 'b') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for band\n",
						fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val != 5) {
					fprintf(stderr,
						"%s: invalid band %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				chanspec |= WL_CHANSPEC_BAND_5G;
				band_set = TRUE;
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" bandwidth\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 20) && (to.val != 40) && (to.val != 80)) {
					fprintf(stderr,
						"%s: invalid bandwidth %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 20)
					chanspec |= WL_CHANSPEC_BW_20;
				else if (to.val == 40)
					chanspec |= WL_CHANSPEC_BW_40;
				else
					chanspec |= WL_CHANSPEC_BW_80;
				bw_set = TRUE;
			}
			if (to.opt == 's') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for"
						" ctl sideband\n", fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 1) && (to.val != 0) && (to.val != -1)) {
					fprintf(stderr,
						"%s: invalid ctl sideband %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == -1)
					chanspec |= WL_CHANSPEC_CTL_SB_LOWER;
				else if (to.val == 1)
					chanspec |= WL_CHANSPEC_CTL_SB_UPPER;
				ctl_sb_set = TRUE;
			}
		}

		/* set ctl sb to 20 if not set and 20mhz is selected */
		if (!ctl_sb_set && CHSPEC_IS20(chanspec)) {
			ctl_sb_set = TRUE;
		}

		if (ch_set && band_set && bw_set && ctl_sb_set) {
			val = wl_chspec32_to_driver(chanspec);
			if (val != INVCHANSPEC) {
				err = wlu_iovar_setint(wl, cmd->name, val);
			} else {
				err = BCME_USAGE_ERROR;
			}
		} else {
			if (!ch_set)
				fprintf(stderr, "%s: you need to set a channel,"
					" '-c <ch>'\n", fn_name);
			if (!band_set)
				fprintf(stderr, "%s: you need to set a band,"
					" '-b <5|2>'\n", fn_name);
			if (!bw_set)
				fprintf(stderr, "%s: you need to set a bandwidth,"
					" '-w <20|40>'\n", fn_name);
			if (!ctl_sb_set)
				fprintf(stderr, "%s: you need to set a ctl sideband,"
					  " '-s <-1|0|1>'\n", fn_name);
			err = BCME_USAGE_ERROR;
		}
	}

exit:
	return err;
}

static int
wl_dfs_max_safe_tx(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint argc, num_vals = 0;
	uint32 *pvals, vals[3];
	char *endptr = NULL;

	/* pop command name */
	argv++;

	/* arg count */
	for (argc = 0; argc < 4 && argv[argc]; argc++); /* empty loop */

	/* get case */
	if (argc < 1) {
		if (cmd->get < 0) {
			return -1;
		}

		if ((err = wlu_var_getbuf_sm(wl, cmd->name, NULL, 0, (void**)&pvals)) || !pvals) {
			printf("err=%d \n", err);
			return err;
		}

		pvals[0] = dtoh32(pvals[0]);
		pvals[1] = dtoh32(pvals[1]);
		pvals[2] = dtoh32(pvals[2]);
		printf("non-adjacent=%u%%, adjacent=%u%%, eu-weather=%u%%\n",
				pvals[0], pvals[1], pvals[2]);

		return err;
	}

	/* set case */
	if (cmd->get < 0) {
		return -1;
	}
	vals[num_vals++] = htod32(strtol(argv[0], &endptr, 0));
	if (argc > 1) {
		vals[num_vals++] = htod32(strtol(argv[1], &endptr, 0));
	}
	if (argc > 2) {
		vals[num_vals++] = htod32(strtol(argv[2], &endptr, 0));
	}

	if ((err = wlu_var_setbuf_sm(wl, cmd->name, vals, sizeof(*vals)*(num_vals))) != BCME_OK) {
		printf("err=%d\n", err);
		return err;
	}

	return err;
}

static int
wl_rclass(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_USAGE_ERROR;
	chanspec_t chanspec = 0;
	chanspec_t chspec_drv = 0;
	void *ptr;

	/* toss the command name */
	argv++;

	if (*argv) {
		chanspec = wf_chspec_aton(*argv);
		if (chanspec != 0) {
			chspec_drv = wl_chspec_to_driver(chanspec);
			err = wlu_var_getbuf(wl, cmd->name, &chspec_drv, sizeof(chanspec_t), &ptr);

			if (err)
				printf("Read rclass fails: chanspec:0x%x\n", chanspec);
			else
				printf("rclass=0x%x\n", *((int *)ptr));
		}
	}

	return err;
}

static int
wl_plcphdr(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;
		val = dtoh32(val);
		if (val == WLC_PLCP_AUTO)
			printf("long");
		else if (val == WLC_PLCP_SHORT)
			printf("auto");
		else if (val == WLC_PLCP_LONG)
			printf("debug");
		else
			printf("unknown");
		printf("\n");
		return 0;
	} else {
		if (!stricmp(*argv, "long"))
			val = WLC_PLCP_AUTO;
		else if (!stricmp(*argv, "auto"))
			val = WLC_PLCP_SHORT;
		else if (!stricmp(*argv, "debug"))
			val = WLC_PLCP_LONG;
		else
			return BCME_USAGE_ERROR;
		val = htod32(val);
		return wlu_set(wl, cmd->set, &val, sizeof(int));
	}
}

/* WLC_GET_RADIO and WLC_SET_RADIO in driver operate on radio_disabled which
 * is opposite of "wl radio [1|0]".  So invert for user.
 * In addition, display WL_RADIO_SW_DISABLE and WL_RADIO_HW_DISABLE bits.
 */
static int
wl_radio(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint val;
	char *endptr = NULL;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
			return ret;
		val = dtoh32(val);
		printf("0x%04x\n", val);
		return 0;
	} else {
		if (cmd->set < 0)
			return -1;
		if (!stricmp(*argv, "on"))
			val = WL_RADIO_SW_DISABLE << 16;
		else if (!stricmp(*argv, "off"))
			val = WL_RADIO_SW_DISABLE << 16 | WL_RADIO_SW_DISABLE;
		else {
			val = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}

			/* raw bits setting, add the mask if not provided */
			if ((val >> 16) == 0) {
				val |= val << 16;
			}
		}
		val = htod32(val);
		return wlu_set(wl, cmd->set, &val, sizeof(int));
	}
}

char *
ver2str(unsigned int vms, unsigned int vls)
{
	static char verstr[100];
	unsigned int maj, year, month, day, build;

	maj = (vms >> 16) & 0xFFFF;
	if (maj > 1000) {
		/* it is probably a date... */
		year = (vms >> 16) & 0xFFFF;
		month = vms & 0xFFFF;
		day = (vls >> 16) & 0xFFFF;
		build = vls & 0xFFFF;
		sprintf(verstr, "%d/%d/%d build %d",
			month, day, year, build);
	} else {
		/* it is a tagged release. */
		sprintf(verstr, "%d.%d RC%d.%d",
			(vms>>16)&0xFFFF, vms&0xFFFF,
			(vls>>16)&0xFFFF, vls&0xFFFF);
	}
	return verstr;
}
#define TRF_DEFAULT_NUMSECS 5
#define TRF_DEFAULT_THRESH  250
static int
wlc_traffic_thresh(void *wl, cmd_t *cmd, char **argv)
{
	wl_traffic_thresh_t req;
	char *val_p, *param;
	/*
	 * default values
	 */
	req.type = WL_TRF_DU;
	req.num_secs = TRF_DEFAULT_NUMSECS;
	req.thresh = TRF_DEFAULT_THRESH;
	param = *argv++;
	/* Parse args */
	if (!(param = *argv++))
		return BCME_USAGE_ERROR;
	if (!stricmp(param, "ap")) {
		req.mode = WL_TRF_MODE_AP;
	} else if (!stricmp(param, "sta")) {
		req.mode = WL_TRF_MODE_STA;
	} else if (!stricmp(param, "mac")) {
		req.mode = WL_TRF_MODE_MAC;
	} else if (!stricmp(param, "intf")) {
		req.mode = WL_TRF_MODE_WL_FEATURE;
		val_p = *argv++;
		req.enable = (int)strtoul(val_p, NULL, 0);
		return (wlu_var_setbuf(wl, cmd->name, &req, sizeof(req)));
	} else if (!stricmp(param, "bss")) {
		req.mode = WL_TRF_MODE_CFG_FEATURE;
		val_p = *argv++;
		req.enable = (int)strtoul(val_p, NULL, 0);
		return (wlu_var_setbuf(wl, cmd->name, &req, sizeof(req)));
	} else {
		return BCME_USAGE_ERROR;
	}

	param = *argv++;
	if (param == NULL) {
		return BCME_USAGE_ERROR;
	}
	if (!stricmp(param, "-dump")) {
		void *dump_buf = malloc(sizeof(struct get_traffic_thresh));
		struct get_traffic_thresh *res = (struct get_traffic_thresh *)dump_buf;
		wl_traffic_thresh_t *data;
		int i;
		if (dump_buf == NULL) {
			printf("NO memory\n");
			return BCME_NORESOURCE;
		}
		req.type = WL_TRF_DU;
		if (req.mode == WL_TRF_MODE_MAC) {
			while ((param = *argv++) != NULL) {
				if ((val_p = *argv++) == NULL) {
					printf("Need value for parameter:%s\n", param);
					free(dump_buf);
					return BCME_USAGE_ERROR;
				}
				if (stricmp(param, "-m") == 0) {
					printf("req.mac %s\n", val_p);
					if (!wl_ether_atoe(val_p, &req.mac)) {
						free(dump_buf);
						return BCME_USAGE_ERROR;
					}
				}
			}
		}
		if (wlu_iovar_getbuf(wl, cmd->name, &req, sizeof(req),
				dump_buf, sizeof(struct get_traffic_thresh))) {
			free(dump_buf);
			return BCME_USAGE_ERROR;
		}
		data = res->data;
		printf("feature enabled for intf = %d\n", res->intf_enab);
		printf("feature enabled for bss = %d\n", res->bss_enab);
		if (res->mode == WL_TRF_MODE_AP) {
			printf("ap auto enable flags = 0x%x\n", res->auto_enable);
		} else if (res->mode == WL_TRF_MODE_STA) {
			printf("sta auto enable flags = 0x%x\n", res->auto_enable);
		} else if (res->mode == WL_TRF_MODE_MAC) {
			printf("scb  enable_flags = 0x%x\n", res->auto_enable);
			free(dump_buf);
			return BCME_OK;
		} else {
			free(dump_buf);
			return BCME_USAGE_ERROR;
		}
		for (i = 0; i < WL_TRF_MAX_QUEUE; i++) {
			printf("type:%d, thresh:%d, num_secs:%d\n",
					i, data[i].thresh, data[i].num_secs);
		}
		free(dump_buf);
		return BCME_OK;
	} else if (!stricmp(param, "-h")) {
		return BCME_USAGE_ERROR;
	}

	do {
		if ((val_p = *argv++) == NULL) {
			printf("Need value for parameter:%s\n", param);
			return BCME_USAGE_ERROR;
		}
		if (!stricmp(param, "-c")) {
			if (!stricmp(val_p, "bk")) {
				req.type = ((req.type & 0xf0) | WL_TRF_BK);
			} else if (!stricmp(val_p, "be")) {
				req.type = ((req.type & 0xf0) | WL_TRF_BE);
			} else if (!stricmp(val_p, "vi")) {
				req.type = ((req.type & 0xf0) | WL_TRF_VI);
			} else if (!stricmp(val_p, "vo")) {
				req.type = ((req.type & 0xf0) | WL_TRF_VO);
			} else if (!stricmp(val_p, "to")) {
				req.type = ((req.type & 0xf0) | WL_TRF_TO);
			} else {
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(param, "-s")) {
			req.num_secs = (int)strtoul(val_p, NULL, 0);
			if ((req.mode == WL_TRF_MODE_STA) &&
					(req.num_secs >= WL_TRF_MAX_SECS)) {
				printf("max allowed num_secs is %d\n",
						WL_TRF_MAX_SECS);
				req.num_secs = WL_TRF_MAX_SECS;
			}
		} else if (!stricmp(param, "-t")) {
			req.thresh = (int)strtoul(val_p, NULL, 0);
		} else if (!stricmp(param, "-e")) {
			req.type |= WL_TRF_AE;
			req.enable = (int)strtoul(val_p, NULL, 0);
		} else if (stricmp(param, "-m") == 0) {
			req.type |= WL_TRF_PM;
			if (!wl_ether_atoe(val_p, &req.mac)) {
				return BCME_USAGE_ERROR;
			}
		} else {
			printf("parameter %s not supported\n", param);
			return BCME_USAGE_ERROR;
		}
	} while ((param = *argv++) != NULL);
	return (wlu_var_setbuf(wl, cmd->name, &req, sizeof(req)));
}

static int
wl_cfp_dump(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	scb_val_t scb_val;
	char *dump_buf;

	if (!*++argv)
		return BCME_USAGE_ERROR;

	dump_buf = malloc(WLC_IOCTL_MEDLEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WLC_IOCTL_MEDLEN);
		return BCME_NOMEM;
	}
	memset(dump_buf, 0, WLC_IOCTL_MEDLEN);

	if (!wl_ether_atoe(*argv, &scb_val.ea))
		return BCME_USAGE_ERROR;

	/* query for 'cfp_dump_id' */
	ret = wlu_iovar_getbuf(wl, cmd->name, &scb_val,
		sizeof(scb_val), dump_buf, WLC_IOCTL_MEDLEN);

	fputs(dump_buf, stdout);

	free(dump_buf);

	return ret;
}

static int
wl_amsdu_tid(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "amsdu_tid";
	struct amsdu_tid_control astc, *reply;
	uint8 tid;
	int err;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);
	if ((param = *++argv) == NULL)
		return BCME_USAGE_ERROR;

	tid = atoi(param);
	if (tid > MAXPRIO)
		return BCME_USAGE_ERROR;
	astc.tid = tid;

	if ((param = *++argv)) {
		astc.enable = atoi(param);
		err = wlu_var_setbuf(wl, cmdname, &astc, sizeof(astc));
	} else {
		if ((err = wlu_var_getbuf_sm(wl, cmdname, &astc, sizeof(astc), &ptr) < 0))
			return err;
		reply = (struct amsdu_tid_control *)ptr;
		printf("AMSDU for tid %d: %d\n", tid, reply->enable);
	}
	return err;
}
static int
wl_version(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int bcmerr = 0;
	char *p = NULL;
	char *dump_buf;
	int err;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	printf("Broadcom BCA: %s\n",
		ver2str(((EPI_MAJOR_VERSION) << 16) | EPI_MINOR_VERSION,
		(EPI_RC_NUMBER << 16) | EPI_INCREMENTAL_NUMBER));

	dump_buf = malloc(WLC_IOCTL_SMLEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WLC_IOCTL_SMLEN);
		return BCME_NOMEM;
	}
	memset(dump_buf, 0, WLC_IOCTL_SMLEN);

	/* query for 'ver' to get version info */
	ret = wlu_iovar_get(wl, "ver", dump_buf, WLC_IOCTL_SMLEN);

	/* if the query is successful, continue on and print the result. */

	/* if the query fails, check for a legacy driver that does not support
	 * the "dump" iovar, and instead issue a WLC_DUMP ioctl.
	 */
	if (ret) {
		err = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
		if (!err && (bcmerr == BCME_UNSUPPORTED)) {
			ret = wlu_get(wl, WLC_DUMP, dump_buf, WLC_IOCTL_SMLEN);
		}
	}

	if (ret) {
		fprintf(stderr, "Error %d on query of driver dump\n", (int)ret);
		free(dump_buf);
		return ret;
	}

	/* keep only the first line from the dump buf output */
	p = strchr(dump_buf, '\n');
	if (p)
		*p = '\0';
	printf("%s\n", dump_buf);

	free(dump_buf);

	return 0;
}

static int
wl_rateparam(void *wl, cmd_t *cmd, char **argv)
{
	int val[2];

	if (!*++argv)
		return BCME_USAGE_ERROR;
	val[0] = htod32(atoi(*argv));
	if (!*++argv)
		return BCME_USAGE_ERROR;
	val[1] = htod32(atoi(*argv));
	return wlu_set(wl, cmd->set, val, sizeof(val));
}

static int
wl_roamparms(void *wl, cmd_t *cmd, char **argv)
{
	int params_size;
	wl_scan_params_t *params;
	int err = 0;

	params_size = WL_MAX_ROAMSCAN_DATSZ;
	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	if (!(argv[1])) {
#ifdef BCMDBG
		printf("GET roam scan params\n");
#endif // endif
	/* no data to copy here for a get */
	err = wlu_iovar_getbuf(wl, "roamscan_parms", params, 0,
		buf, WLC_IOCTL_MEDLEN);
	if (err) {
		fprintf(stderr, "Error retrieving roamscan params: %d\n", err);
		goto done;
	}

#ifdef BCMDBG
		prhex(NULL, (void *)buf, 64);
#endif // endif
		memset(params, 0, params_size);
		memcpy(params, buf, params_size);

		printf("Roam Scan Parameters:\n");
		printf("scan_type:    %d\n", dtoh32(params->scan_type));
		printf("nprobes:      %d\n", dtoh32(params->nprobes));
		printf("active_time:  %d\n", dtoh32(params->active_time));
		printf("passive_time: %d\n", dtoh32(params->passive_time));
		printf("home_time:    %d\n", dtoh32(params->home_time));

		/* print out the channels, if any */
		if (params->channel_num) {
			uint32 i;
			uint32 chcount = dtoh32(params->channel_num);
			printf("Channels:\n");
			for (i = 0; i < chcount; i++)
				printf("Channel number 0x%x\n", dtoh16(params->channel_list[i]));
		}
		/* No ssids to print out, ever */

		goto done;
	}

	printf("Setting Roam Scan parameters \n");

	err = wl_scan_prep(wl, cmd, argv, params, &params_size);

	if (err)
		goto done;

	printf("params_size %d\n", params_size);
	err = wlu_iovar_setbuf(wl, "roamscan_parms", params, params_size, buf, WLC_IOCTL_MEDLEN);

done:
	free(params);
	return err;
}

#ifdef WLRCC
static int
wl_roamchannels(void *wl, cmd_t *cmd, char **argv)
{
	int params_size;
	wl_roam_channel_list_t *params;
	int i, chcount, chspec;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	params_size = sizeof(*params);
	params = (wl_roam_channel_list_t *)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan channels\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	if (!(argv[1])) {
		/* no data to copy here for a get */
		err = wlu_iovar_getbuf(wl, "roamscan_channels", params, 0,
			buf, WLC_IOCTL_MEDLEN);
		if (err) {
			fprintf(stderr, "Error retrieving roamscan channels: %d\n", err);
			goto done;
		}

		memcpy(params, buf, params_size);

		chcount = dtoh32(params->n);
		printf("Roam Scan Channels: %d\n", chcount);
		for (i = 0; i < chcount; i++) {
			chspec = dtoh16(params->channels[i]);
			wf_chspec_ntoa(chspec, buf);
			printf("Channel: %s (0x%x)\n", buf, chspec);
		}
		goto done;
	}

	printf("Setting Roam Scan channels not supported!!!\n");

done:
	free(params);
	return err;
}
#endif /* WLRCC */

static int
wl_roam_prof(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint i;
	void *ptr = NULL;
	uint16	wl_rp_ver = WL_ROAM_PROF_VER_0;
	uint wl_roam_prof_size = sizeof(wl_roam_prof_v1_t);
	union {
		wl_roam_prof_band_v1_t v1;
		wl_roam_prof_band_v2_t v2;
	} rp;

	/* try to check wlc_ver first
	 * for older FW (WLC interface revision <= 5), wl_roam_prof ver 0 is used;
	 * for newer FW (WLC interface revision > 5), wl_roam_prof ver 1 is used;
	 */
	if (wlc_ver_major(wl) > 5) {
		wl_rp_ver = WL_ROAM_PROF_VER_1;
		wl_roam_prof_size = sizeof(wl_roam_prof_v2_t);
	}

	/* fill in some common fields among all roam_prof verisions */
	++argv;
	rp.v1.ver = wl_rp_ver;
	if (*argv && (!strcmp(*argv, "b") || !strcmp(*argv, "2g"))) {
		rp.v1.band = WLC_BAND_2G;
	} else if (*argv && (!strcmp(*argv, "a") || !strcmp(*argv, "5g"))) {
		rp.v1.band = WLC_BAND_5G;
	} else
		return -1;	/* Missing band */

	/* roam_prof version get */
	rp.v1.len = 0;
	if ((ret = wlu_var_getbuf(wl, cmd->name, &rp, 8, &ptr)) < 0)
		return ret;

	memcpy(&rp, ptr, sizeof(rp));

	if (dtoh16(rp.v1.ver) > WL_ROAM_PROF_VER_1) {
		printf("\tIncorrect version: expected up to %d; got %d\n",
				WL_ROAM_PROF_VER_1, dtoh16(rp.v1.ver));
		return BCME_VERSION;
	}
	if ((rp.v1.len % wl_roam_prof_size) != 0) {
		printf("\tbad length (=%d) in return data\n", rp.v1.len);
		return BCME_BADLEN;
	}

	if (!*++argv) {
		for (i = 0; i < WL_MAX_ROAM_PROF_BRACKETS; i++) {
			if ((i * wl_roam_prof_size) > rp.v1.len) break;

			if (rp.v1.ver > WL_ROAM_PROF_VER_0) {
				/* The full scan period must be non-zero for valid roam profile */
				if (rp.v2.roam_prof[i].fullscan_period == 0) break;

				printf("flag:%02x RSSI[%d,%d] delta:%d(%s) boost:%d.by.%d "
				       "nfscan:%d period(full:%ds partial:%ds.x%d.%ds) "
				       "CU(trigger:%d%% duration:%ds)\n",
				       rp.v2.roam_prof[i].roam_flags,
				       rp.v2.roam_prof[i].roam_trigger,
				       rp.v2.roam_prof[i].rssi_lower,
				       rp.v2.roam_prof[i].roam_delta,
				       rp.v2.roam_prof[i].channel_usage ? "%" : "dBm",
				       rp.v2.roam_prof[i].rssi_boost_thresh,
				       rp.v2.roam_prof[i].rssi_boost_delta,
				       rp.v2.roam_prof[i].nfscan,
				       rp.v2.roam_prof[i].fullscan_period,
				       rp.v2.roam_prof[i].init_scan_period,
				       rp.v2.roam_prof[i].backoff_multiplier,
				       rp.v2.roam_prof[i].max_scan_period,
				       rp.v2.roam_prof[i].channel_usage,
				       rp.v2.roam_prof[i].cu_avg_calc_dur);
			} else {
				/* The full scan period must be non-zero for valid roam profile */
				if (rp.v1.roam_prof[i].fullscan_period == 0) break;

				printf("flag:%02x RSSI[%d,%d] delta:%d(dBm) boost:%d.by.%d "
				       "nfscan:%d period(full:%ds partial:%ds.x%d.%ds)\n",
				       rp.v1.roam_prof[i].roam_flags,
				       rp.v1.roam_prof[i].roam_trigger,
				       rp.v1.roam_prof[i].rssi_lower,
				       rp.v1.roam_prof[i].roam_delta,
				       rp.v1.roam_prof[i].rssi_boost_thresh,
				       rp.v1.roam_prof[i].rssi_boost_delta,
				       rp.v1.roam_prof[i].nfscan,
				       rp.v1.roam_prof[i].fullscan_period,
				       rp.v1.roam_prof[i].init_scan_period,
				       rp.v1.roam_prof[i].backoff_multiplier,
				       rp.v1.roam_prof[i].max_scan_period);
			}
		}
	} else {
		/* set */
		memset(&rp.v1.roam_prof[0], 0, wl_roam_prof_size * WL_MAX_ROAM_PROF_BRACKETS);
		for (i = 0; i < WL_MAX_ROAM_PROF_BRACKETS; i++) {
			if (rp.v1.ver > WL_ROAM_PROF_VER_0) {
				if (!*argv) break;
				rp.v2.roam_prof[i].roam_flags = atoi(*argv++);

				if (!*argv) return -1;
				rp.v2.roam_prof[i].roam_trigger = atoi(*argv++);

				if (!*argv) return -1;
				rp.v2.roam_prof[i].rssi_lower = atoi(*argv++);

				if (!*argv) return -1;
				rp.v2.roam_prof[i].roam_delta = atoi(*argv++);

				if (!*argv) return -1;
				rp.v2.roam_prof[i].rssi_boost_thresh = atoi(*argv++);

				if (!*argv) return -1;
				rp.v2.roam_prof[i].rssi_boost_delta = atoi(*argv++);

				if (!*argv) return -1;
				rp.v2.roam_prof[i].nfscan = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v2.roam_prof[i].fullscan_period = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v2.roam_prof[i].init_scan_period = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v2.roam_prof[i].backoff_multiplier = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v2.roam_prof[i].max_scan_period = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v2.roam_prof[i].channel_usage = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v2.roam_prof[i].cu_avg_calc_dur = htod16(atoi(*argv++));

				/* If channel usage is not used, set cu_avg_calc_dur to default */
				if (rp.v2.roam_prof[i].channel_usage == WL_CU_PERCENTAGE_DISABLE)
					rp.v2.roam_prof[i].cu_avg_calc_dur =
						WL_CU_CALC_DURATION_DEFAULT;
			} else {
				if (!*argv) break;
				rp.v1.roam_prof[i].roam_flags = atoi(*argv++);

				if (!*argv) return -1;
				rp.v1.roam_prof[i].roam_trigger = atoi(*argv++);

				if (!*argv) return -1;
				rp.v1.roam_prof[i].rssi_lower = atoi(*argv++);

				if (!*argv) return -1;
				rp.v1.roam_prof[i].roam_delta = atoi(*argv++);

				if (!*argv) return -1;
				rp.v1.roam_prof[i].rssi_boost_thresh = atoi(*argv++);

				if (!*argv) return -1;
				rp.v1.roam_prof[i].rssi_boost_delta = atoi(*argv++);

				if (!*argv) return -1;
				rp.v1.roam_prof[i].nfscan = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v1.roam_prof[i].fullscan_period = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v1.roam_prof[i].init_scan_period = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v1.roam_prof[i].backoff_multiplier = htod16(atoi(*argv++));

				if (!*argv) return -1;
				rp.v1.roam_prof[i].max_scan_period = htod16(atoi(*argv++));
			}
		}

		if (i == 0) {
			return -1;
		}

		if (*argv) {
			/* too many parameters */
			return -1;
		}

		rp.v1.len = wl_roam_prof_size * i;
		ret = wlu_var_setbuf(wl, cmd->name, &rp, 8 + rp.v1.len);
	}
	return ret;
}

static int
wl_scan(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(uint16);
	wl_scan_params_t *params;
	int err = 0;

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, params, &params_size);

	if (!err) {

		err = wlu_set(wl, cmd->set, params, params_size);
	}

	free(params);
	return err;
}

static int
wl_escan(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_escan_params_t, params)) +
	    (WL_NUMCHANNELS * sizeof(uint16));
	wl_escan_params_t *params;
	int err = 0;
	uint16 action = WL_SCAN_ACTION_START;

	if (!stricmp(*argv, "escan"))
		/* start an escan */
		action = WL_SCAN_ACTION_START;
	else if (!stricmp(*argv, "escanabort"))
		/* abort an escan */
		action = WL_SCAN_ACTION_ABORT;
	else {
		printf("unknown escan command: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_escan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, &params->params, &params_size);

	if (!err) {
		params->version = htod32(ESCAN_REQ_VERSION);
		params->action = htod16(action);

#if defined(linux)
		srand((unsigned)time(NULL));
		params->sync_id = htod16(rand() & 0xffff);
#else
		params->sync_id = htod16(4321);
#endif /* #if defined(linux) */

		params_size += OFFSETOF(wl_escan_params_t, params);
		err = wlu_iovar_setbuf(wl, "escan", params, params_size, buf, WLC_IOCTL_MAXLEN);
	}

	free(params);
	return err;
}

static int
wl_iscan(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_iscan_params_t, params)) +
	    (WL_NUMCHANNELS * sizeof(uint16));
	wl_iscan_params_t *params;
	int err = 0;
	uint16 action = WL_SCAN_ACTION_START;
	char **p;
	uint16 iscan_duration = 0;

	if (!stricmp(*argv, "iscan_s"))
		action = WL_SCAN_ACTION_START;
	else if (!stricmp(*argv, "iscan_c"))
		action = WL_SCAN_ACTION_CONTINUE;
	else {
		printf("unknown iscan command: %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* look for iscan_duration parameter */
	p = argv;
	while (*p != NULL) {
		if (!strcmp(*p, "-d") || !strncmp(*p, "--duration=", 11)) {
			char *valptr;
			int val;
			char *endptr;
			if (!strcmp(*p, "-d"))
				valptr = *(++p);
			else
				valptr = *p + 11;
			val = (int)strtol(valptr, &endptr, 0);
			if (*endptr != '\0') {
				fprintf(stderr,
					"could not parse \"%s\" as an int for duration\n",
					valptr);
				err = -1;
				goto exit;
			}
			iscan_duration = (uint16) val;
			break;
		}
		++p;
	}

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_iscan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, &params->params, &params_size);

	if (!err) {
		params->version = htod32(ISCAN_REQ_VERSION);
		params->action = htod16(action);
		params->scan_duration = htod16(iscan_duration);
		params_size += OFFSETOF(wl_iscan_params_t, params);
		err = wlu_iovar_setbuf(wl, "iscan", params, params_size, buf, WLC_IOCTL_MAXLEN);
	}

	free(params);
exit:
	return err;
}

static int
wl_parse_assoc_params(char **argv, wl_assoc_params_t *params, bool *prescanned)
{
	int err = BCME_OK;
	char *p, *eq, *valstr;
	char opt;
	bool opt_end;
	int keylen;
	char key[64];
	int i;
	bool bssid_set = FALSE;
	bool ch_set = FALSE;

	opt_end = FALSE;

	while ((p = *argv) != NULL) {
		argv++;
		memset(key, 0, sizeof(key));
		opt = '\0';
		valstr = NULL;

		if (opt_end) {
			valstr = p;
		}
		else if (!strcmp(p, "--")) {
			opt_end = TRUE;
			continue;
		}
		else if (!strcmp(p, "prescanned")) {
			if (prescanned)
				*prescanned = TRUE;
			continue;
		}
		else if (!strncmp(p, "--", 2)) {
			eq = strchr(p, '=');
			if (eq == NULL) {
				fprintf(stderr, "wl_parse_assoc_params: missing \" = \" in "
				        "long param \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			keylen = eq - (p + 2);
			if (keylen > 63)
				keylen = 63;
			memcpy(key, p + 2, keylen);

			valstr = eq + 1;
			if (*valstr == '\0') {
				fprintf(stderr, "wl_parse_assoc_params: missing value after "
				        "\" = \" in long param \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		else if (!strncmp(p, "-", 1)) {
			opt = p[1];
			if (strlen(p) > 2) {
				fprintf(stderr, "wl_parse_assoc_params: only single char options, "
				        "error on param \"%s\"\n", p);
				err = BCME_BADARG;
				goto exit;
			}
			if (*argv == NULL) {
				fprintf(stderr, "wl_parse_assoc_params: missing value parameter "
				        "after \"%s\"\n", p);
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			valstr = *argv++;
		} else {
			valstr = p;
		}

		/* handle -o v or --option=val */
		if (opt == 'b' || !stricmp(key, "bssid")) {
			if (!wl_ether_atoe(valstr, &params->bssid)) {
				fprintf(stderr, "could not parse as an ethernet MAC address\n");
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			bssid_set = TRUE;
		}
		else if (opt == 'c' || !strcmp(key, "chanspecs")) {
			params->chanspec_num =
			     wl_parse_chanspec_list(valstr, params->chanspec_list, WL_NUMCHANNELS);
			if (params->chanspec_num == -1) {
				fprintf(stderr, "error parsing chanspec list arg\n");
				err = BCME_BADARG;
				goto exit;
			}
			ch_set = TRUE;
		}
	}

	if (prescanned && *prescanned && (ch_set || bssid_set)) {
		fprintf(stderr, "cannot use bssid/channel options with prescan option\n");
		err = BCME_BADARG;
		goto exit;
	}

	/* prepare the chanspec using the channel number and the user provided options */
	for (i = 0; i < params->chanspec_num; i++) {
		chanspec_t chanspec = wl_chspec_to_driver(params->chanspec_list[i]);
		if (chanspec == INVCHANSPEC) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		params->chanspec_list[i] = chanspec;
	}
	params->chanspec_num = htod32(params->chanspec_num);

exit:
	return err;
}

/* wl reassoc <bssid>
 * Options:
 * -c CL, --chanspecs=CL, where CL is a comma or space separated list of chanspecs
 */
static int
wl_reassoc(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = WL_REASSOC_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(chanspec_t);
	wl_reassoc_params_t *params;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	if (*++argv == NULL) {
		fprintf(stderr, "no arguments to wl_reassoc\n");
		return BCME_USAGE_ERROR;
	}

	params = (wl_reassoc_params_t *)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	if (!wl_ether_atoe(*argv, &params->bssid)) {
		fprintf(stderr, "could not parse %s as an Ethernet MAC address\n", *argv);
		err = BCME_USAGE_ERROR;
		goto exit;
	}
	/* default to plain old ioctl */
	params_size = ETHER_ADDR_LEN;

	if (*++argv != NULL) {
		if ((err = wl_parse_reassoc_params(argv, params)) != BCME_OK) {
			fprintf(stderr, "could not parse reassociation parameters\n");
			goto exit;
		}
		params_size = WL_REASSOC_PARAMS_FIXED_SIZE +
		        dtoh32(params->chanspec_num) * sizeof(chanspec_t);
	}

	err = wlu_set(wl, WLC_REASSOC, params, params_size);

exit:
	free(params);
	return err;
}

/* Pretty print the BSS list */
static void
dump_networks(char *network_buf)
{
	wl_scan_results_v2_t *list = (wl_scan_results_v2_t*)network_buf;
	wl_bss_info_v109_1_t *bi;
	uint i;

	if (list->count == 0)
		return;
	else if (list->version != WL_BSS_INFO_VERSION &&
	         list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	         list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		return;
	}

	bi = (wl_bss_info_v109_1_t*)list->bss_info;
	for (i = 0; i < list->count; i++, bi = (wl_bss_info_v109_1_t*)((int8*)bi +
		dtoh32(bi->length))) {
		dump_bss_info(bi);
	}
}

static int
wl_bcnlenhist(void *wl, cmd_t *cmd, char **argv)
{
	wlc_bcn_len_hist_t *bcnlenhist = NULL;
	uint32 *bcns_len = NULL;
	char* dump_buf = NULL;
	uint32 counter = 0;
	int	index = 0;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	dump_buf = malloc(WLC_IOCTL_SMLEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WLC_IOCTL_SMLEN);
		return -1;
	}
	memset(dump_buf, 0, WLC_IOCTL_SMLEN);

	if (argv[1])
		err = wlu_iovar_getbuf(wl, "bcnlenhist", argv[1], 1, dump_buf, WLC_IOCTL_SMLEN);
	else
		err = wlu_iovar_getbuf(wl, "bcnlenhist", NULL, 0, dump_buf, WLC_IOCTL_SMLEN);

	if (BCME_OK == err) {
		bcnlenhist = (wlc_bcn_len_hist_t *)dump_buf;

		index = bcnlenhist->cur_index;
		counter = bcnlenhist->ringbuff_len;
		bcns_len = bcnlenhist->bcnlen_ring;

		index--;
		printf("LAST %d BEACON LENGTH's:  ", counter);
		for (; counter--; index--) {
			if (index < 0)
				index = bcnlenhist->ringbuff_len - 1;
			printf("%d  ", bcns_len[index]);
		}

		printf("\nMAX BCNLEN: %d\n", bcnlenhist->max_bcnlen);

		if (bcnlenhist->min_bcnlen == (int)0x7fffffff)
			printf("MIN BCNLEN: 0\n\n");
		else
			printf("MIN BCNLEN: %d\n\n", bcnlenhist->min_bcnlen);
	}

	free(dump_buf);

	return err;
}

static int
wl_dump_networks(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	char *dump_buf, *dump_buf_orig;
	uint32 status = 0;
	bool iscan = FALSE;

	dump_buf_orig = dump_buf = malloc(WL_DUMP_BUF_LEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WL_DUMP_BUF_LEN);
		return BCME_NOMEM;
	}

	iscan = (cmd->get != WLC_SCAN_RESULTS);
	if (iscan) {
		int buflen = 1920;	/* usually fits about 10 BSS infos */

		if (*(++argv)) {
			char *endptr = NULL;
			buflen = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		ret = wl_get_iscan(wl, dump_buf, buflen);
	} else
		ret = wl_get_scan(wl, WLC_SCAN_RESULTS, dump_buf, WL_DUMP_BUF_LEN);

	if (ret == 0) {
		if (iscan) {
			status = dtoh32(((wl_iscan_results_v2_t *)dump_buf)->status);
			dump_buf += OFFSETOF(wl_iscan_results_v2_t, results);
		}
		dump_networks(dump_buf);
		if (iscan) {
			switch (status) {
			case WL_SCAN_RESULTS_PARTIAL:
				printf("iscanresults incomplete\n");
				break;
			case WL_SCAN_RESULTS_SUCCESS:
				printf("iscanresults complete\n");
				break;
			case WL_SCAN_RESULTS_PENDING:
				printf("iscanresults pending\n");
				break;
			case WL_SCAN_RESULTS_ABORTED:
				printf("iscanresults aborted\n");
				break;
			default:
				printf("iscanresults returned unknown status %d\n", status);
				break;
			}
		}
	}

exit:
	free(dump_buf_orig);
	return ret;
}

static int
wl_dump_chanlist(void *wl, cmd_t *cmd, char **argv)
{
	uint32 chan_buf[WL_NUMCHANNELS + 1];
	wl_uint32_list_t *list;
	int ret;
	uint i;

	UNUSED_PARAMETER(argv);

	list = (wl_uint32_list_t *)(void *)chan_buf;
	list->count = htod32(WL_NUMCHANNELS);
	ret = wlu_get(wl, cmd->get, chan_buf, sizeof(chan_buf));
	if (ret < 0)
		return ret;

	for (i = 0; i < dtoh32(list->count); i++)
		printf("%d ", dtoh32(list->element[i]));
	printf("\n");
	return ret;
}

/* Dump chanspecs based on the driver's current configuration of band, band-width & locale. */
static int
wl_dump_chanspecs_defset(void *wl, cmd_t *cmd, char **argv)
{
	const char* fn_name = "wl_dump_chanspecs_defset";
	wl_uint32_list_t *list;
	int ret, buflen;
	chanspec_t c = 0;
	uint i;
	int err;
	int num_chanspecs = 0;
	char chspec_str[CHANSPEC_STR_LEN];

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(buf, 0, WLC_IOCTL_MAXLEN);

	strcpy(buf, "chanspecs_defset");
	buflen = strlen(buf) + 1;

	/* toss the command name */
	argv++;

	/* Validate arguments if any */
	if (*argv) {
		fprintf(stderr,
		    "%s: This IOVAR doesn't take any arguments.\n", fn_name);
		err = BCME_USAGE_ERROR;
		goto exit;
	}

	num_chanspecs += NUM_CHANSPECS_LIST_SIZE;

	/* Add list */
	list = (wl_uint32_list_t *)(buf + buflen);
	list->count = htod32(num_chanspecs);
	buflen += sizeof(uint32)*(num_chanspecs + 1);

	/* if buflen is greater then WLC_IOCTL_MAXLEN return error */
	ret = BCME_IOCTL_ERROR;
	while (buflen <= WLC_IOCTL_MAXLEN) {
		int bcmerr;

		ret = wlu_get(wl, WLC_GET_VAR, &buf[0], buflen);
		if (ret == 0)
			break;

		/* If the error is not buffer too short break */
		ret = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
		if (ret < 0 || bcmerr != BCME_BUFTOOSHORT)
			break;

		/* If the error is buffer too short, increment the num_chanspecs */
		num_chanspecs += NUM_CHANSPECS_LIST_SIZE;
		list->count = htod32(num_chanspecs);
		/* increase the buffer length by the space for the chanspecs added */
		buflen += sizeof(uint32)*(NUM_CHANSPECS_LIST_SIZE);
	}

	/* Incase of error return */
	if (ret) {
		err = BCME_IOCTL_ERROR;
		goto exit;
	}

	list = (wl_uint32_list_t *)buf;
	for (i = 0; i < dtoh32(list->count); i++) {
		c = (chanspec_t)dtoh32(list->element[i]);
		wf_chspec_ntoa(c, chspec_str);
		printf("%s (0x%04x)\n", chspec_str, c);
	}
	printf("\n");
	return ret;

	exit:
		return err;
}

static int
wl_dump_chanspecs(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_dump_chanspecs";
	wl_uint32_list_t *list;
	chanspec_t c = 0, *chanspec;
	int ret, buflen;
	uint i;
	int err, opt_err;
	bool band_set = FALSE, bw_set = FALSE, is80p80 = FALSE;
	char abbrev[WLC_CNTRY_BUF_SZ] = ""; /* default.. current locale */
	char chspec_str[CHANSPEC_STR_LEN];
	char *country_abbrev;
	int num_chanspecs = 0;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(buf, 0, WLC_IOCTL_MAXLEN);

	/* multiple commands are using this API to dump a channel list:
	 * chanspecs
	 * roam_channels_in_cache
	 * roam_channels_in_hotlist
	 */
	strcpy(buf, cmd->name);
	buflen = strlen(buf) + 1;

	/* toss the command name */
	argv++;

	/* Validate arguments if any */
	if (*argv) {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'b') {
				if (!to.good_int) {
					fprintf(stderr,
					        "%s: could not parse \"%s\" as an int for band\n",
					        fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 5) && (to.val != 2)) {
					fprintf(stderr,
					        "%s: invalid band %d\n",
					        fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 5)
					c |= WL_CHANSPEC_BAND_5G;
				else
					c |= WL_CHANSPEC_BAND_2G;
				band_set = TRUE;
			}
			if (to.opt == 'w') {
				/* For 80+80 case, using flag is80p80 for it */
				if (!strcmp(to.valstr, "80+80"))  {
					is80p80 = TRUE;
				}
				if ((!to.good_int) && (!is80p80)) {
					fprintf(stderr,
					        "%s: could not parse \"%s\" as an int for"
					        " bandwidth\n",
					        fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 20) && (to.val != 40) && (to.val != 80) &&
					(to.val != 160) && (!is80p80)) {
					fprintf(stderr,
					        "%s: invalid bandwidth %d\n",
					        fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 20)
					c |= WL_CHANSPEC_BW_20;
				else if (to.val == 40)
					c |= WL_CHANSPEC_BW_40;
				else {
					if (ioctl_version == 1) {
						fprintf(stderr,
						        "%s: bandwidth 80 MHz is not supported by "
						        "this version driver.\n",
						        fn_name);
						err = BCME_USAGE_ERROR;
						goto exit;

					}
					if (is80p80)
						c |= WL_CHANSPEC_BW_8080;
					else if (to.val == 160)
						c |= WL_CHANSPEC_BW_160;
					else
						c |= WL_CHANSPEC_BW_80;
				}

				bw_set = TRUE;
			}
			if (to.opt == 'c') {
				if (!to.valstr) {
					fprintf(stderr,
					        "%s: please provide country abbrev \n", fn_name);
					err = BCME_USAGE_ERROR;
					goto exit;
				}
				strncpy(abbrev, to.valstr, WLC_CNTRY_BUF_SZ - 1);
				abbrev[WLC_CNTRY_BUF_SZ - 1] = '\0';
			}
		}
		if (!bw_set || !band_set) {
			if (!band_set)
				fprintf(stderr, "%s: you need to set a band, '-b <5|2>'\n",
				        fn_name);
			if (!bw_set)
				fprintf(stderr,
				        "%s: you need to set a bandwidth, "
				        "'-w <20|40|80|160|80+80>'\n",
				        fn_name);
			err = BCME_USAGE_ERROR;
			goto exit;
		}
	}

	/* convert chanspec to legacy if needed */
	if (c != 0) {
		c = wl_chspec_to_driver(c);
		if (c == INVCHANSPEC) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
	}

	/* Add chanspec argument */
	chanspec = (chanspec_t *) (buf + buflen);
	*chanspec = c;
	buflen += (sizeof(chanspec_t));

	/* Add country abbrev */
	country_abbrev = buf + buflen;
	strncpy(country_abbrev, abbrev, WLC_CNTRY_BUF_SZ);
	buflen += WLC_CNTRY_BUF_SZ;

	num_chanspecs += NUM_CHANSPECS_LIST_SIZE;

	/* Add list */
	list = (wl_uint32_list_t *)(buf + buflen);
	list->count = htod32(num_chanspecs);
	buflen += sizeof(uint32)*(num_chanspecs + 1);

	/* if buflen is greater then WLC_IOCTL_MAXLEN return error */
	ret = BCME_IOCTL_ERROR;
	while (buflen <= WLC_IOCTL_MAXLEN) {
		int bcmerr;

		ret = wlu_get(wl, WLC_GET_VAR, &buf[0], buflen);
		if (ret == 0)
			break;

		/* If the error is not buffer too short break */
		ret = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
		if (ret < 0 || bcmerr != BCME_BUFTOOSHORT)
			break;

		/* reconstruct the argument */
		strcpy(buf, cmd->name);

		/* Add chanspec argument */
		*chanspec = c;

		/* Add country abbrev */
		strncpy(country_abbrev, abbrev, WLC_CNTRY_BUF_SZ);

		/* If the error is buffer too short, increment the num_chanspecs */
		num_chanspecs += NUM_CHANSPECS_LIST_SIZE;
		list->count = htod32(num_chanspecs);
		/* increase the buffer length by the space for the chanspecs added */
		buflen += sizeof(uint32)*(NUM_CHANSPECS_LIST_SIZE);
	}

	/* Incase of error return */
	if (ret) {
		err = BCME_IOCTL_ERROR;
		goto exit;
	}

	list = (wl_uint32_list_t *)buf;
	for (i = 0; i < dtoh32(list->count); i++) {
		c = wl_chspec32_from_driver(list->element[i]);
		wf_chspec_ntoa(c, chspec_str);
		printf("%s (0x%04x)\n", chspec_str, c);
	}
	printf("\n");
	return ret;

exit:
	return err;
}

static int
wl_rmc_report_version_get(void *wl, uint16 category, char **argv)
{
	int ret = 0;
	bcm_xtlv_t *rmc_rpt_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 rmc_rpt_len, rmc_rpt_id;
	uint16 val_len, val_id;
	void *val_tlv_ptr;
	uint16 len_of_tlvs;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;

	if (*argv) {
		printf("error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* init the wrapper bcn_report category XTLV in the IO buffer and copy to it */
	rmc_rpt_tlv = (bcm_xtlv_t *)&buf[0];
	rmc_rpt_tlv->id = htol16(category);
	rmc_rpt_tlv->len = 0;
	bcm_xtlv_pack_xtlv(rmc_rpt_tlv, category, 0, 0, no_pad);

	/* issue the 'roam_cache' get with the one-item id list */
	ret = wlu_iovar_getbuf(wl, "roam_cache", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN);
	if (ret) {
		return ret;
	}

	rmc_rpt_len = ltoh16(rmc_rpt_tlv->len);
	rmc_rpt_id = ltoh16(rmc_rpt_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(rmc_rpt_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("XTLV specifies length %u too long for iobuffer len %u\n",
			rmc_rpt_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the rmc_report xtlv container */
	if (rmc_rpt_id != category) {
		printf("return category %u did not match %u\n", rmc_rpt_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (rmc_rpt_len < BCM_XTLV_HDR_SIZE) {
		printf("return XTLV container specifies length %u too small to hold any values\n",
				rmc_rpt_len);
		return BCME_ERROR;
	}
	val_xtlv = (bcm_xtlv_t*)rmc_rpt_tlv->data;
	val_len = ltoh16(val_xtlv->len);
	val_id = ltoh16(val_xtlv->id);

	/* make sure the value xtlv is valid xtlv container */
	if (!bcm_valid_xtlv(val_xtlv, rmc_rpt_len, no_pad)) {
		printf("return XTLV specifies length %u too long for containing xtlv len %u\n",
				val_len, rmc_rpt_len);
		return BCME_ERROR;
	}

	if (val_len == 0) {
		printf("roam_cache version get: return value XTLV (id:%u) empty\n", val_id);
		return BCME_ERROR;
	}

	/* print as words if multiple of word size */
	val_tlv_ptr = rmc_rpt_tlv->data;
	len_of_tlvs = rmc_rpt_tlv->len;
	while (len_of_tlvs && len_of_tlvs > BCM_XTLV_HDR_SIZE) {
		val_xtlv = (bcm_xtlv_t *)val_tlv_ptr;
		switch (val_xtlv->id) {
			case WL_RMC_RPT_XTLV_VER:
				printf("version:%d\n", *((uint8*)val_xtlv->data));
				break;
			default:
				break;
		}
		val_tlv_ptr = (uint8 *)val_tlv_ptr + bcm_xtlv_size(val_xtlv,
				BCM_XTLV_OPTION_ALIGN32);
		len_of_tlvs -= bcm_xtlv_size(val_xtlv, BCM_XTLV_OPTION_ALIGN32);
	}
	return (ret);
}

static int
wl_rmc_report_data_get(void *wl, uint16 category, char **argv)
{
	int ret = 0;
	bcm_xtlv_t *rmc_rpt_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 rmc_rpt_len, rmc_rpt_id;
	uint16 val_len, val_id;
	void *val_tlv_ptr;
	uint16 len_of_tlvs;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;
	uint idx;
	const char* reason_name;
	const char* status_name;

	static struct {
		uint event;
		const char *event_name;
	} reason_names[] = {
		{WLC_E_REASON_INITIAL_ASSOC, "INITIAL ASSOCIATION"},
		{WLC_E_REASON_LOW_RSSI, "LOW_RSSI"},
		{WLC_E_REASON_DEAUTH, "RECEIVED DEAUTHENTICATION"},
		{WLC_E_REASON_DISASSOC, "RECEIVED DISASSOCATION"},
		{WLC_E_REASON_BCNS_LOST, "BEACONS LOST"},
		{WLC_E_REASON_FAST_ROAM_FAILED, "FAST ROAM FAILED"},
		{WLC_E_REASON_DIRECTED_ROAM, "DIRECTED ROAM"},
		{WLC_E_REASON_TSPEC_REJECTED, "TSPEC REJECTED"},
		{WLC_E_REASON_BETTER_AP, "BETTER AP FOUND"},
		{WLC_E_REASON_MINTXRATE, "STUCK AT MIN TX RATE"},
		{WLC_E_REASON_BSSTRANS_REQ, "REQUESTED ROAM"},
		{WLC_E_REASON_TXFAIL, "TOO MANY TXFAILURES"}
	};

	static struct {
		uint event;
		const char *event_name;
	} status_names[] = {
		{WLC_E_STATUS_SUCCESS, "operation was successful"},
		{WLC_E_STATUS_FAIL, "operation failed"},
		{WLC_E_STATUS_TIMEOUT, "operation timed out"},
		{WLC_E_STATUS_NO_NETWORKS, "failed due to no matching network found"},
		{WLC_E_STATUS_ABORT, "operation was aborted"},
		{WLC_E_STATUS_NO_ACK, "protocol failure: packet not ack'd"},
		{WLC_E_STATUS_UNSOLICITED, "AUTH or ASSOC packet was unsolicited"},
		{WLC_E_STATUS_ATTEMPT, "attempt to assoc to an auto auth configuration"},
		{WLC_E_STATUS_PARTIAL, "scan results are incomplete"},
		{WLC_E_STATUS_NEWSCAN, "scan aborted by another scan"},
		{WLC_E_STATUS_NEWASSOC, "scan aborted due to assoc in progress"},
		{WLC_E_STATUS_11HQUIET, "802.11h quiet period started"},
		{WLC_E_STATUS_SUPPRESS, "user disabled scanning"},
		{WLC_E_STATUS_NOCHANS, "no allowable channels to scan"},
		{WLC_E_STATUS_CS_ABORT, "abort channel select"},
		{WLC_E_STATUS_ERROR, "request failed due to error"},
		{WLC_E_STATUS_INVALID, "Invalid status code"}
	};

	if (*argv) {
		printf("error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}
	/* init the wrapper roam_cache category XTLV in the IO buffer and copy to it */
	rmc_rpt_tlv = (bcm_xtlv_t *)&buf[0];
	rmc_rpt_tlv->id = htol16(category);
	rmc_rpt_tlv->len = 0;
	bcm_xtlv_pack_xtlv(rmc_rpt_tlv, category, 0, 0, no_pad);

	/* issue the 'roam_cache' get with the one-item id list */
	ret = wlu_iovar_getbuf(wl, "roam_cache", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN);
	if (ret) {
		return ret;
	}

	rmc_rpt_len = ltoh16(rmc_rpt_tlv->len);
	rmc_rpt_id = ltoh16(rmc_rpt_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(rmc_rpt_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("XTLV specifies length %u too long for iobuffer len %u\n",
			rmc_rpt_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the bcn_report xtlv container */
	if (rmc_rpt_id != category) {
		printf("return category %u did not match %u\n", rmc_rpt_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (rmc_rpt_len < BCM_XTLV_HDR_SIZE) {
		printf("return XTLV container specifies length %u too small to hold any values\n",
				rmc_rpt_len);
		return BCME_ERROR;
	}
	val_xtlv = (bcm_xtlv_t*)rmc_rpt_tlv->data;
	val_len = ltoh16(val_xtlv->len);
	val_id = ltoh16(val_xtlv->id);

	/* make sure the value xtlv is valid xtlv container */
	if (!bcm_valid_xtlv(val_xtlv, rmc_rpt_len, no_pad)) {
		printf("return XTLV specifies length %u too long for containing xtlv len %u\n",
				val_len, rmc_rpt_len);
		return BCME_ERROR;
	}

	if (val_len == 0) {
		printf("roam_cache data get: return value XTLV (id:%u) empty\n", val_id);
		return BCME_ERROR;
	}

	/* print as words if multiple of word size */
	val_tlv_ptr = rmc_rpt_tlv->data;
	len_of_tlvs = rmc_rpt_tlv->len;
	while (len_of_tlvs && len_of_tlvs > BCM_XTLV_HDR_SIZE) {
		val_xtlv = (bcm_xtlv_t *)val_tlv_ptr;
		switch (val_xtlv->id) {
			case WL_RMC_RPT_XTLV_BSS_INFO:
			{
				rmc_bss_info_v1_t *bss_info = (rmc_bss_info_v1_t *)(val_xtlv->data);
				printf("\t Current BSS INFO:\n");
				printf("\t\tRSSI: %d\n", bss_info->rssi);
				printf("\t\tNumber of full scans performed on current BSS: %d\n",
					bss_info->fullscan_count);
				for (idx = 0; idx < ARRAYSIZE(reason_names); idx++) {
					if (reason_names[idx].event == bss_info->reason)
						reason_name = reason_names[idx].event_name;
				}
				printf("\t\tReason code for last full scan: %s(%d)\n",
					reason_name, bss_info->reason);
				printf("\t\tDelta between current time and last full scan: %d\n",
					bss_info->time_full_scan);
				for (idx = 0; idx < ARRAYSIZE(status_names); idx++) {
					if (status_names[idx].event == bss_info->status)
						status_name = status_names[idx].event_name;
				}
				printf("\t\tLast status code for not roaming: %s(%d)\n",
					status_name, bss_info->status);
			}
				break;

			case WL_RMC_RPT_XTLV_CANDIDATE_INFO:
			{
				rmc_candidate_info_v1_t *candidate_info =
					(rmc_candidate_info_v1_t *)(val_xtlv->data);
				printf("\t Candidate INFO:\n");
				printf("\t\tBSSID: %s\n",
					wl_ether_etoa((const struct ether_addr *)
						&candidate_info->bssid));
				printf("\t\tRSSI: %d\n", candidate_info->rssi);
				printf("\t\tChannel: %d\n", candidate_info->ctl_channel);
				printf("\t\tDelta between current time and last seen time: %d\n",
					candidate_info->time_last_seen);
				printf("\t\tBSS load: %d\n", candidate_info->bss_load);
			}
				break;

			default:
				break;
		}
		val_tlv_ptr = (uint8 *)val_tlv_ptr + bcm_xtlv_size(val_xtlv,
				BCM_XTLV_OPTION_ALIGN32);
		len_of_tlvs -= bcm_xtlv_size(val_xtlv, BCM_XTLV_OPTION_ALIGN32);
	}
	return (ret);
}

static int
wl_roam_cache(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char *sub_cmd_name;

	UNUSED_PARAMETER(cmd);

	/* Skip the command name */
	argv++;

	sub_cmd_name = *argv++;

	if (sub_cmd_name == NULL) {
		/* usage */
		printf("roam_cache: missing category\n");
		return BCME_USAGE_ERROR;
	}

	if (!strcmp(sub_cmd_name, "data")) {
		/* GET roam cache report data */
		ret = wl_rmc_report_data_get(wl, WL_RMC_RPT_CMD_DATA, argv);
	} else if (!strcmp(sub_cmd_name, "ver")) {
		/* GET roam cache report ver */
		ret = wl_rmc_report_version_get(wl, WL_RMC_RPT_CMD_VER, argv);
	} else {
		/* usage */
		printf("unknown roam_cache sub-commands \"%s\"\n", sub_cmd_name);
		return BCME_USAGE_ERROR;
	}
	return ret;
}

static int
wl_channels_in_country(void *wl, cmd_t *cmd, char **argv)
{
	wl_channels_in_country_t *cic;
	int ret;
	uint i, len;

	cic = (wl_channels_in_country_t *)buf;
	cic->buflen = WLC_IOCTL_MAXLEN;
	cic->count = 0;

	/* country abbrev must follow */
	if (!*++argv) {
		fprintf(stderr, "missing country abbrev\n");
		return BCME_USAGE_ERROR;
	}

	len = strlen(*argv);
	if ((len > 3) || (len < 2)) {
		fprintf(stderr, "invalid country abbrev: %s\n", *argv);
		return BCME_BADARG;
	}

	strcpy(cic->country_abbrev, *argv);

	/* band must follow */
	if (!*++argv) {
		fprintf(stderr, "missing band\n");
		return BCME_USAGE_ERROR;
	}

	if (!stricmp(*argv, "a"))
		cic->band = WLC_BAND_5G;
	else if (!stricmp(*argv, "b"))
		cic->band = WLC_BAND_2G;
	else {
		fprintf(stderr, "unsupported band: %s\n", *argv);
		return BCME_UNSUPPORTED;
	}

	cic->buflen = htod32(cic->buflen);
	cic->band = htod32(cic->band);
	cic->count = htod32(cic->count);
	ret = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN);
	if (ret < 0)
		return ret;

	for (i = 0; i < dtoh32(cic->count); i++)
		printf("%d ", dtoh32(cic->channel[i]));
	printf("\n");

	return ret;
}

int
wl_get_scan(void *wl, int opc, char *scan_buf, uint buf_len)
{
	wl_scan_results_v2_t *list = (wl_scan_results_v2_t*)scan_buf;
	int ret;

	list->buflen = htod32(buf_len);
	ret = wlu_get(wl, opc, scan_buf, buf_len);
	if (ret < 0)
		return ret;
	ret = 0;

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);
	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
	} else if (list->version != WL_BSS_INFO_VERSION &&
	        list->version != LEGACY2_WL_BSS_INFO_VERSION &&
	        list->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			list->version, WL_BSS_INFO_VERSION);
		list->buflen = 0;
		list->count = 0;
	}

	return ret;
}

static int
wl_get_iscan(void *wl, char *scan_buf, uint buf_len)
{
	wl_iscan_results_v2_t list;
	wl_scan_results_v2_t *results;
	int ret;

	memset(&list, '\0', sizeof(list));
	list.results.buflen = htod32(buf_len);
	ret = wlu_iovar_getbuf(wl, "iscanresults", &list, WL_ISCAN_RESULTS_FIXED_SIZE,
	                      scan_buf, WLC_IOCTL_MAXLEN);

	if (ret < 0)
		return ret;

	ret = 0;

	results = &((wl_iscan_results_v2_t*)scan_buf)->results;
	results->buflen = dtoh32(results->buflen);
	results->version = dtoh32(results->version);
	results->count = dtoh32(results->count);
	if (results->buflen == 0) {
		printf("wl_get_iscan buflen 0\n");
		results->version = 0;
		results->count = 0;
	} else if (results->version != WL_BSS_INFO_VERSION &&
	        results->version != LEGACY2_WL_BSS_INFO_VERSION &&
	        results->version != LEGACY_WL_BSS_INFO_VERSION) {
		fprintf(stderr, "Sorry, your driver has bss_info_version %d "
			"but this program supports only version %d.\n",
			results->version, WL_BSS_INFO_VERSION);
		results->buflen = 0;
		results->count = 0;
	}

	return ret;
}

static int
wl_spect(void *wl, cmd_t *cmd, char **argv)
{
	int ret, spect;
	char *endptr = NULL;

	if (!*++argv) {
		if ((ret = wlu_get(wl, cmd->get, &spect, sizeof(spect))) < 0) {
			return ret;
		}

		spect = dtoh32(spect);
		switch (spect) {
		case SPECT_MNGMT_OFF:
			printf("Off\n");
			break;

		case SPECT_MNGMT_LOOSE_11H:
			printf("Loose interpretation of 11h spec - may join non 11h AP.\n");
			break;

		case SPECT_MNGMT_STRICT_11H:
			printf("Strict interpretation of 11h spec - may not join non 11h AP.\n");
			break;

		case SPECT_MNGMT_STRICT_11D:
			printf("802.11d mode\n");
			break;

		case SPECT_MNGMT_LOOSE_11H_D:
			printf("Loose interpretation of 11h+d spec - may join non-11h APs\n");
			break;

		default:
			printf("invalid value 0x%x\n", spect);
			return BCME_BADARG;
		}
		return (0);
	} else {
		spect = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return BCME_USAGE_ERROR;

		if (spect < SPECT_MNGMT_OFF || spect > SPECT_MNGMT_LOOSE_11H_D)
			return BCME_BADARG;

		spect = htod32(spect);
		return wlu_set(wl, cmd->set, &spect, sizeof(spect));
	}
}

static int
wl_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	struct ether_addr bssid;
	wlc_ssid_t ssid;
	char ssidbuf[SSID_FMT_BUF_LEN];
	wl_bss_info_v109_1_t *bi;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if ((ret = wlu_get(wl, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN)) == 0) {
		/* The adapter is associated. */
		*(uint32*)buf = htod32(WLC_IOCTL_MAXLEN);
		if ((ret = wlu_get(wl, WLC_GET_BSS_INFO, buf, WLC_IOCTL_MAXLEN)) < 0)
			return ret;

		bi = (wl_bss_info_v109_1_t*)(buf + 4);
		if (dtoh32(bi->version) == WL_BSS_INFO_VERSION ||
			dtoh32(bi->version) == LEGACY2_WL_BSS_INFO_VERSION ||
			dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
			dump_bss_info(bi);
		} else {
			fprintf(stderr, "Sorry, your driver has bss_info_version %d "
				"but this program supports only versions "
				"%d, %d, and %d.\n",
				bi->version, WL_BSS_INFO_VERSION,
				LEGACY2_WL_BSS_INFO_VERSION,
				LEGACY_WL_BSS_INFO_VERSION);
		}
	} else {
		printf("Not associated. Last associated with ");

		if ((ret = wlu_get(wl, WLC_GET_SSID, &ssid, sizeof(wlc_ssid_t))) < 0) {
			printf("\n");
			return ret;
		}

		wl_format_ssid(ssidbuf, ssid.SSID, dtoh32(ssid.SSID_len));
		printf("SSID: \"%s\"\n", ssidbuf);
	}

	return 0;
}

static int
wl_deauth_rc(void *wl, cmd_t *cmd, char **argv)
{
	scb_val_t scb_val;
	int ret;

	if (!*++argv) {
		fprintf(stderr, "STA MAC not specified, deauth all\n");
		ret = wlu_set(wl, WLC_SCB_DEAUTHENTICATE, (void *)&ether_bcast,
			ETHER_ADDR_LEN);
		return ret;

	} else if (!wl_ether_atoe(*argv, &scb_val.ea)) {
		fprintf(stderr, "Malformed STA MAC parameter\n");
		ret = BCME_USAGE_ERROR;

	} else if (!*++argv) {
		/* No reason code furnished, so driver will use its default */
		ret = wlu_set(wl, WLC_SCB_DEAUTHENTICATE, &scb_val.ea,
			ETHER_ADDR_LEN);

	} else {
		scb_val.val = htod32((uint32)strtoul(*argv, NULL, 0));
		ret = wlu_set(wl, cmd->set, &scb_val, sizeof(scb_val));
	}
	return ret;
}

static int
wl_wpa_auth(void *wl, cmd_t *cmd, char **argv)
{
	int bsscfg_idx = 0;
	int consumed;
	int wpa_auth = 0;
	int ret = 0;
	int i;
	static struct {
		int val;
		const char *name;
	} auth_mode[] =
		  /* Keep the numeric values in the staticly initialized
		   * help string consistent.  Unfortunately, there isn't
		   * an automatic way for that.
		   */
		{{WPA_AUTH_NONE,	"WPA-NONE"},
		 {WPA_AUTH_UNSPECIFIED,	"WPA-802.1x"},
		 {WPA_AUTH_PSK,		"WPA-PSK"},
		 {WPA2_AUTH_UNSPECIFIED, "WPA2-802.1x"},
		 {WPA2_AUTH_PSK,	"WPA2-PSK"},
		 {WPA2_AUTH_1X_SHA256,	"1X-SHA256"},
		 {WPA2_AUTH_FT,		"FT"},
		 {WPA2_AUTH_PSK_SHA256,	"PSK-SHA256"},
		 {WPA3_AUTH_SAE_PSK,	"SAE"},
		 {WPA3_AUTH_SAE_FBT,	"SAE-FT"},
		 {WPA_AUTH_DISABLED,	"disabled"}};

	/* skip the command name */
	argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, cmd->name, &bsscfg_idx, &consumed)) != 0)
		return ret;

	argv += consumed;

	if (!*argv) {
		/* no arg, so this is a GET. */

		if (!consumed)
			ret = wlu_iovar_getint(wl, "wpa_auth", &wpa_auth);
		else
			ret = wl_bssiovar_getint(wl, "wpa_auth", bsscfg_idx, &wpa_auth);

		if (ret < 0)
			return ret;

		/* Show all AKM suites enabled */
		printf("0x%x", wpa_auth);

		if (wpa_auth == WPA_AUTH_DISABLED)
			printf(" Disabled");

		for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++) {
			if (wpa_auth & auth_mode[i].val)
				printf(" %s", auth_mode[i].name);
		}

		printf("\n");
		return ret;

	} else {
		/* there's an arg, so this is a SET. */
		ret = 1;

		/* Validate the user input range */
		if (isdigit((int)*argv[0])) {
			unsigned int range = 0;

			/* param is a number; look for value in the list */
			wpa_auth = strtoul(*argv, NULL, 0);

			/* Validate the user input range */

			for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++)
				range |= auth_mode[i].val;

			range = (~range) & 0xFFFF;

			if (range & wpa_auth) {
				ret = 1;
				goto usage;
			} else {
				ret = 0;
			}

		} else {

			int arg_count = 0;
			char** p_argv;
			int j = 0;
			unsigned int range = 0;

			wpa_auth = 0;
			p_argv = argv;

			for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++)
				range |= auth_mode[i].val;

			range = (~range) & (0xFFFF);

			while (*p_argv) {
				arg_count++;
				p_argv++;
				}

			p_argv = argv;

			for (j = 0; j < arg_count; j++) {
				bool found = FALSE;

				argv = p_argv + j;

				/* treat param as string to be matched in list */
				for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++) {
					if (!stricmp(auth_mode[i].name, *argv)) {

						found = TRUE;
						wpa_auth |= auth_mode[i].val;
						ret = 0;

						/* traverse the list */
						argv++;
						if (!*argv)
							break;
					}
				}

				if ((found == FALSE) || (range & wpa_auth))
					goto usage;
			}

		}
		if (ret)
			fprintf(stderr, "%s is not a valid WPA auth mode\n", *argv);
		else {
			if (!consumed)
				ret = wlu_iovar_setint(wl, "wpa_auth", wpa_auth);
			else
				ret = wl_bssiovar_setint(wl, "wpa_auth", bsscfg_idx, wpa_auth);
		}
	}

	return ret;

usage:
	fprintf(stderr, "Inavlid user argument.\n");
	fprintf(stderr, "Values may be a bitvector or list of names from the set.\n");

	for (i = 0; i < (int)ARRAYSIZE(auth_mode); i++) {
		fprintf(stderr, "\n0x%04x  %s", auth_mode[i].val, auth_mode[i].name);
	}

	printf("\n");
	return ret;
}

static int
wl_set_pmk(void *wl, cmd_t *cmd, char **argv)
{
	wsec_pmk_t psk;
	size_t key_len;

	if (!*++argv) {
		return BCME_USAGE_ERROR;
	}
	key_len = strlen(*argv);
	if (key_len < WSEC_MIN_PSK_LEN || key_len > WSEC_MAX_PSK_LEN) {
		fprintf(stderr, "passphrase must be between %d and %d characters long\n",
		       WSEC_MIN_PSK_LEN, WSEC_MAX_PSK_LEN);
		return BCME_BADARG;
	}
	psk.key_len = htod16((ushort) key_len);
	psk.flags = htod16(WSEC_PASSPHRASE);
	memcpy(psk.key, *argv, key_len);
	return wlu_set(wl, cmd->set, &psk, sizeof(psk));
}

static int
wl_wsec(void *wl, cmd_t *cmd, char **argv)
{
	int wsec;
	int bsscfg_idx = 0;
	int consumed;
	char *endptr = NULL;
	int error;

	UNUSED_PARAMETER(cmd);

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "wsec", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	if (!*argv) {
		/* This is a GET */
		if (consumed == 0) {
			error = wlu_get(wl, WLC_GET_WSEC, &wsec, sizeof(uint32));
			wsec = dtoh32(wsec);
		}
		else
			error = wl_bssiovar_getint(wl, "wsec", bsscfg_idx, &wsec);

		if (!error)
			wl_printint(wsec);
	} else {
		/* This is a SET */
		if (!stricmp(*argv, "off"))
			wsec = 0;
		else {
			wsec = strtol(*argv, &endptr, 0);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}
		}

		if (consumed == 0) {
			wsec = htod32(wsec);
			error = wlu_set(wl, WLC_SET_WSEC, &wsec, sizeof(uint32));
		}
		else
			error = wl_bssiovar_setint(wl, "wsec", bsscfg_idx, wsec);
	}

	return error;
}

static int
wl_get_chanspec_txpwr_max(void *wl, cmd_t *cmd, char **argv)
{
	int i, err;
	char chspec_str[CHANSPEC_STR_LEN];

	wl_chanspec_txpwr_max_t *chanspec_txpwr;
	wl_chanspec_txpwr_max_t params;

	miniopt_t to;
	chanspec_t chanspec = 0;
	int opt_err;
	bool band_set = FALSE, bw_set = FALSE;

	/* toss the command name */
	argv++;

	/* Validate arguments if any */
	if (*argv) {
		miniopt_init(&to, __FUNCTION__, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'b') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse [%s] as band\n",
						__FUNCTION__, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val != 5) && (to.val != 2)) {
					fprintf(stderr, "%s: invalid band %d\n",
						__FUNCTION__, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 5)
					chanspec |= WL_CHANSPEC_BAND_5G;
				else
					chanspec |= WL_CHANSPEC_BAND_2G;
				band_set = TRUE;
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse [%s] as bandwidth\n",
						__FUNCTION__, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val == 20)
					chanspec |= WL_CHANSPEC_BW_20;
				else if (to.val == 40)
					chanspec |= WL_CHANSPEC_BW_40;
				else if (to.val == 80)
					chanspec |= WL_CHANSPEC_BW_80;
				else if (to.val == 160)
					chanspec |= WL_CHANSPEC_BW_160;
				else if (to.val == 8080)
					chanspec |= WL_CHANSPEC_BW_8080;
				else {
					fprintf(stderr, "%s: invalid bandwidth %d\n",
					         __FUNCTION__, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				bw_set = TRUE;
			}
		}
		if (!bw_set || !band_set) {
			if (!band_set)
				fprintf(stderr, "%s: you need to set a band, '-b <5|2>'\n",
				        __FUNCTION__);
			if (!bw_set)
				fprintf(stderr,
				        "%s: you need to set a bandwidth, '-w <20|40|80>'\n",
				        __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto exit;
		}
	}

	/* convert chanspec to legacy if needed */
	if (chanspec != 0) {
		chanspec = wl_chspec_to_driver(chanspec);
		if (chanspec == INVCHANSPEC) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
	}

	memset(&params, 0, WL_CHANSPEC_TXPWR_MAX_LEN);
	params.ver = WL_CHANSPEC_TXPWR_MAX_VER;
	params.len = WL_CHANSPEC_TXPWR_MAX_LEN;
	params.count = 1;
	params.txpwr[0].chanspec = chanspec;

	if ((err = wlu_iovar_getbuf(wl, cmd->name, &params, sizeof(params),
		buf, WLC_IOCTL_MAXLEN)) < 0) {
		return err;
	}

	chanspec_txpwr = (wl_chanspec_txpwr_max_t *)buf;

	if (chanspec_txpwr->ver != WL_CHANSPEC_TXPWR_MAX_VER) {
		fprintf(stderr, "Error: version [%d] mismatch Driver version:%d\n",
			WL_CHANSPEC_TXPWR_MAX_VER, chanspec_txpwr->ver);
		return err;
	}

	for (i = 0; i < (int)(dtoh32(chanspec_txpwr->count)); i++) {
		chanspec = wl_chspec32_from_driver(chanspec_txpwr->txpwr[i].chanspec);
		wf_chspec_ntoa(chanspec, chspec_str);

		printf("%s\t(0x%04x)\t%2d.%02d(dbm)\n",
			chspec_str, chanspec,
			DIV_QUO(chanspec_txpwr->txpwr[i].txpwr_max, 4),
			DIV_REM(chanspec_txpwr->txpwr[i].txpwr_max, 4));
	}
	printf("\n");

exit:
	return err;
}

static int wl_get_txpwr_adj_est(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	txpwr_adj_est_t adj_est_pwr;
	int i;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_iovar_get(wl, cmd->name, (void *)&adj_est_pwr,  sizeof(adj_est_pwr))) < 0) {
		fprintf(stderr, "Error: est power failed. Make sure interface is up.\n");
		return err;
	}

	if (adj_est_pwr.version != TXPWR_ADJ_EST_VERSION) {
		fprintf(stderr, "Error: version [%d] mismatch Driver version:%d\n",
			TXPWR_ADJ_EST_VERSION, adj_est_pwr.version);
		return err;
	}

	printf("Last adjusted est. power (chanspec:0x%x):\t", adj_est_pwr.chanspec);
	for (i = 0; i < adj_est_pwr.rf_cores; i++)
		printf("%2d.%02d  ",
		       DIV_QUO(adj_est_pwr.estpwr[i], 4),
		       DIV_REM(adj_est_pwr.estpwr[i], 4));
	printf("\n");

	return err;
}

static int wl_get_txpwr_target_max(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	txpwr_target_max_t target_pwr;
	int i;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_iovar_get(wl, cmd->name, (void *)&target_pwr,  sizeof(target_pwr))) < 0) {
		fprintf(stderr, "Error: txpwr_target failed. Make sure interface is up.\n");
		return err;
	}

	if (target_pwr.version != TXPWR_TARGET_VERSION) {
		fprintf(stderr, "Error: version [%d] mismatch Driver version:%d\n",
			TXPWR_TARGET_VERSION, target_pwr.version);
		return err;
	}

	printf("Maximum Tx Power Target (chanspec:0x%x):\t", target_pwr.chanspec);
	for (i = 0; i < target_pwr.rf_cores; i++)
		printf("%2d.%02d  ",
		       DIV_QUO(target_pwr.txpwr[i], 4),
		       DIV_REM(target_pwr.txpwr[i], 4));
	printf("\n");

	return err;
}

#define WL_JOIN_PARAMS_MAX WL_JOIN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(chanspec_t)

/* when prescanned option is specified */
static int
wl_join_prescanned(void *wl, wl_join_params_t *join_params, uint *join_params_size)
{
	/* load with prescanned channels and bssids */
	int ret, err = 0;
	char *destbuf;
	wl_scan_results_v2_t *scanres = NULL;
	wl_bss_info_v109_1_t *bi;
	uint i, cnt, bssid_cnt, bi_len;

	if ((destbuf = malloc(WL_DUMP_BUF_LEN)) == NULL) {
		fprintf(stderr, "Failed to allocate %d-byte buffer for scanresults\n",
			WL_DUMP_BUF_LEN);
		err = BCME_NOMEM;
		goto pexit;
	}

	if ((ret = wl_get_scan(wl, WLC_SCAN_RESULTS, destbuf, WL_DUMP_BUF_LEN)) != 0) {
		fprintf(stderr, "failed to fetch scan results, err %d\n", ret);
		err = ret;
		goto pexit;
	}

	scanres = (wl_scan_results_v2_t *)destbuf;
	if (scanres->version != WL_BSS_INFO_VERSION) {
		fprintf(stderr, "scan parsing failed (expect version %d, got %d)\n",
			WL_BSS_INFO_VERSION, scanres->version);
		err = -1;
		goto pexit;
	}

	/* find matching ssids to fill the channel list */
	for (cnt = i = 0, bi = (wl_bss_info_v109_1_t*)scanres->bss_info; i < scanres->count;
		 i++, bi = (wl_bss_info_v109_1_t*)((int8*)bi + bi_len)) {
		bi_len = dtoh32(bi->length);
		if ((bi->SSID_len != join_params->ssid.SSID_len) ||
			memcmp(bi->SSID, join_params->ssid.SSID,
			join_params->ssid.SSID_len)) {
			continue;
		} else {
			dump_bss_info(bi);
			printf("--------------------------------\n");
		}

		memcpy(&join_params->params.chanspec_list[cnt],
			&bi->chanspec, sizeof(chanspec_t));
		cnt++;
	}
	bssid_cnt = (uint16)cnt;

	/* append the corresponding bssids */
	destbuf = (char*)&join_params->params.chanspec_list[cnt];
	*join_params_size = destbuf - (char*)join_params;
	*join_params_size += (cnt * sizeof(struct ether_addr));

	if (*join_params_size > WL_JOIN_PARAMS_MAX) {
		fprintf(stderr, "Can't fit bssids for all %d APs found\n", cnt);
		err = -1;
		goto pexit;
	}

	for (cnt = i = 0, bi = (wl_bss_info_v109_1_t*)scanres->bss_info;
		 (i < scanres->count) && (cnt < bssid_cnt);
		 i++, bi = (wl_bss_info_v109_1_t*)((int8*)bi + bi_len)) {
		bi_len = dtoh32(bi->length);
		if ((bi->SSID_len != join_params->ssid.SSID_len) ||
			memcmp(bi->SSID, join_params->ssid.SSID,
			join_params->ssid.SSID_len)) {
			continue;
		}

		memcpy(destbuf, &bi->BSSID, sizeof(struct ether_addr));
		destbuf += sizeof(struct ether_addr);
		cnt++;
	}

	if (cnt != bssid_cnt) {
		fprintf(stderr, "Mismatched channel and bssid counts!\n");
		err = -1;
		goto pexit;
	}

	if (cnt == 0) {
		printf("No matches found, issuing normal join.\n");
	} else {
		printf("Passing %d channel/bssid pairs.\n", cnt);
	}
	join_params->params.bssid_cnt = htod16(bssid_cnt);

pexit:
	if (scanres)
		free((char*)scanres);
	else
		free(destbuf);
	return err;
}

/* wl join <ssid> [key <0-3>:xxxxx]
 *                [imode bss|ibss]
 *                [amode open|shared|openshared|wpa|wpapsk|wpa2|wpa2psk|wpanone|ftpsk]
 *                [options]
 * Options:
 * -b MAC, --bssid=MAC, where MAC is in xx:xx:xx:xx:xx:xx format
 * -c CL, --chanspecs=CL, where CL is a comma or space separated list of chanspecs
 * -p, -passive: uses  join iovar instead of SET_SSID ioctl to force passive assoc scan
 */

static int
wl_join(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK, idx = 0;
	wl_join_params_t *join_params;
	uint join_params_size;
	wl_wsec_key_t key;
	int wsec = 0, auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	char* cmd_name;
	bool prescanned = FALSE;
	int passive = 0;

	UNUSED_PARAMETER(cmd);

	cmd_name = *argv++;

	/* allocate the max storage */
	join_params_size = WL_JOIN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(chanspec_t);
	if ((join_params = malloc(join_params_size)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for assoc params\n", join_params_size);
		return BCME_NOMEM;
	}
	memset(join_params, 0, join_params_size);
	memcpy(&join_params->params.bssid, &ether_bcast, ETHER_ADDR_LEN);

	/* verify that SSID was specified and is a valid length */
	if (!*argv || (strlen(*argv) > DOT11_MAX_SSID_LEN)) {
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	join_params->ssid.SSID_len = strlen(*argv);
	memcpy(join_params->ssid.SSID, *argv, join_params->ssid.SSID_len);

	/* default to plain old ioctl */
	join_params_size = sizeof(wlc_ssid_t);

	/* get infrastructure mode */
	if (wlu_iovar_getint(wl, "infra_configuration", &infra) < 0) {
		fprintf(stderr, "iovar infra_configuration GET failed, ret %d\n", ret);
		goto exit;
	}
	infra = dtoh32(infra);

	/* get authentication mode */
	if ((ret = wlu_get(wl, WLC_GET_AUTH, &auth, sizeof(int))) < 0) {
		fprintf(stderr, "ioctl WLC_GET_AUTH failed, ret %d\n", ret);
		goto exit;
	}
	auth = dtoh32(auth);

	/* get current wsec */
	if (wlu_iovar_getint(wl, "wsec", &wsec) < 0) {
		wsec = 0;
	}
	wsec = dtoh32(wsec);

	/* get WPA_auth mode */
	if ((ret = wlu_get(wl, WLC_GET_WPA_AUTH, &wpa_auth, sizeof(wpa_auth))) < 0) {
		fprintf(stderr, "ioctl WLC_GET_WPA_AUTH failed, ret %d\n", ret);
		goto exit;
	}
	wpa_auth = dtoh32(wpa_auth);

	while (*++argv) {
		if (!stricmp(*argv, "wepkey") || !stricmp(*argv, "wep") || !stricmp(*argv, "key")) {
			/* specified wep key */
			memset(&key, 0, sizeof(key));
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			/* WEP index specified */
			if (*(argv[0]+1) == ':') {
				idx = *argv[0] - 0x30;
				if (idx < 0 || idx > 3) {
					fprintf(stderr, "Invalid key index %d specified\n", idx);
					ret = BCME_BADARG;
					goto exit;
				}
				argv[0] += 2; /* colon + digit */
			}
			key.index = idx;

			if (parse_wep(argv, &key, FALSE)) {
				ret = BCME_BADARG;
				goto exit;
			}

			key.index = htod32(key.index);
			key.len = htod32(key.len);
			key.algo = htod32(key.algo);
			key.flags = htod32(key.flags);

			if ((ret = wlu_set(wl, WLC_SET_KEY, &key, sizeof(wl_wsec_key_t))) < 0) {
				goto exit;
			}
			wsec |= WEP_ENABLED;
		}
		/* specified infrastructure mode */
		else if (!stricmp(*argv, "imode") ||
		         !stricmp(*argv, "infra") ||
		         !stricmp(*argv, "mode")) {
			if (!*++argv) {
				fprintf(stderr, "%s %s: expected argument after \"infra\" keyword "
				        "but command line ended.\n", wlu_av0, cmd_name);
				ret = BCME_USAGE_ERROR;
				goto exit;
			} else if (!stricmp(*argv, "ibss") ||
			           !stricmp(*argv, "adhoc") ||
			           !stricmp(*argv, "ad-hoc")) {
				infra = WL_BSSTYPE_INDEP;
			} else if (!stricmp(*argv, "bss") ||
			           !stricmp(*argv, "managed") ||
			           !strnicmp(*argv, "infra", 5)) {
				infra = WL_BSSTYPE_INFRA;
			} else {
				fprintf(stderr, "%s %s: unrecongnized parameter \"%s\" after "
				        "\"infra\" keyword\n", wlu_av0, cmd_name, *argv);
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		/* specified authentication mode */
		else if (!stricmp(*argv, "amode") || !strnicmp(*argv, "auth", 4)) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			auth = WL_AUTH_OPEN_SYSTEM;
			wpa_auth = WPA_AUTH_DISABLED;
			if (!stricmp(*argv, "open"))
				auth = WL_AUTH_OPEN_SYSTEM;
			else if (!stricmp(*argv, "shared"))
				auth = WL_AUTH_SHARED_KEY;
			else if (!stricmp(*argv, "openshared"))
				auth = WL_AUTH_OPEN_SHARED;
			else if (!stricmp(*argv, "wpanone"))
				wpa_auth = WPA_AUTH_NONE;
			else if (!stricmp(*argv, "wpa"))
				wpa_auth = WPA_AUTH_UNSPECIFIED;
			else if (!stricmp(*argv, "wpapsk"))
				 wpa_auth = WPA_AUTH_PSK;
			else if (!stricmp(*argv, "wpa2"))
				wpa_auth = WPA2_AUTH_UNSPECIFIED;
			else if (!stricmp(*argv, "wpa2psk"))
				wpa_auth = WPA2_AUTH_PSK;
			else if (!stricmp(*argv, "ftpsk"))
				wpa_auth = WPA2_AUTH_PSK | WPA2_AUTH_FT;
			else if (!stricmp(*argv, "wpa2-sha256"))
				wpa_auth = WPA2_AUTH_1X_SHA256;
			else if (!stricmp(*argv, "wpa2psk-sha256"))
				wpa_auth = WPA2_AUTH_PSK_SHA256;
			else {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
		}
		else if (!stricmp(*argv, "-passive") || !stricmp(*argv, "-p")) {
			/* Use extended join iovar to assoc_scan passively */
			passive = 1;
		}
		/* optional assoc params */
		else if ((ret = wl_parse_assoc_params(argv, &join_params->params,
				&prescanned)) == BCME_OK) {
			join_params_size = WL_JOIN_PARAMS_FIXED_SIZE +
				dtoh32(join_params->params.chanspec_num) * sizeof(chanspec_t);
			break;
		}
		else {
			fprintf(stderr, "%s %s: unable to parse parameter \"%s\"\n",
				wlu_av0, cmd_name, *argv);
			goto exit;
		}
	}

	/* set infrastructure mode */
	infra = htod32(infra);
	if ((ret = wlu_set(wl, WLC_SET_INFRA, &infra, sizeof(int))) < 0) {
		fprintf(stderr, "ioctl WLC_SET_INFRA failed, ret %d\n", ret);
		goto exit;
	}

	/* set authentication mode */
	auth = htod32(auth);
	if ((ret = wlu_set(wl, WLC_SET_AUTH, &auth, sizeof(int))) < 0) {
		fprintf(stderr, "ioctl WLC_SET_AUTH failed, ret %d\n", ret);
		goto exit;
	}

	/* set wsec mode */
	wsec = htod32(wsec);
	if ((ret = wlu_iovar_setint(wl, "wsec", wsec)) < 0) {
		fprintf(stderr, "iovar wsec SET failed, ret %d\n", ret);
		goto exit;
	}

	/* set WPA_auth mode */
	wpa_auth = htod32(wpa_auth);
	if ((ret = wlu_set(wl, WLC_SET_WPA_AUTH, &wpa_auth, sizeof(wpa_auth))) < 0) {
		fprintf(stderr, "ioctl WLC_SET_WPA_AUTH failed, ret %d\n", ret);
		goto exit;
	}

	if (passive) {
		wl_extjoin_params_t *extjoin_params;
		int extjoin_size;
		int i;
		printf("Using passive assoc scan\n");
		extjoin_size = WL_EXTJOIN_PARAMS_FIXED_SIZE +
			join_params->params.chanspec_num * sizeof(chanspec_t);
		if ((extjoin_params = malloc(extjoin_size)) == NULL) {
			fprintf(stderr, "Error allocating %d bytes for extjoin \n", extjoin_size);
			ret = BCME_NOMEM;
			goto exit;
		}

		/* Copy assoc params from legacy struct into extended struct */
		memset(extjoin_params, 0, extjoin_size);
		memcpy(&extjoin_params->ssid.SSID, &join_params->ssid.SSID, DOT11_MAX_SSID_LEN);
		extjoin_params->ssid.SSID_len = htod32(join_params->ssid.SSID_len);

		memcpy(&extjoin_params->assoc.bssid, &join_params->params.bssid, ETHER_ADDR_LEN);
		extjoin_params->assoc.chanspec_num = join_params->params.chanspec_num;
		for (i = 0; i < join_params->params.chanspec_num; i++) {
			extjoin_params->assoc.chanspec_list[i] =
				join_params->params.chanspec_list[i];
		}

		extjoin_params->scan.scan_type = WL_SCANFLAGS_PASSIVE;
		extjoin_params->scan.nprobes = -1;
		extjoin_params->scan.active_time = -1;
		extjoin_params->scan.passive_time = -1;
		extjoin_params->scan.home_time = -1;

		ret = wlu_var_setbuf(wl, "join", extjoin_params, extjoin_size);
		free(extjoin_params);
	} else {
		/* join parameters starts with the ssid */
		join_params->ssid.SSID_len = htod32(join_params->ssid.SSID_len);

		if (prescanned) {
			if ((ret = wl_join_prescanned(wl, join_params, &join_params_size)) < 0)
				goto exit;
		}

		ret = wlu_set(wl, WLC_SET_SSID, join_params, join_params_size);
	}

exit:
	free(join_params);
	return ret;
}

/* Set or Get the "bssid" iovar, with an optional config index argument:
 *	wl bssid [-C N]|[--cfg=N] bssid
 *
 * Option:
 *	-C N
 *	--cfg=N
 *	--config=N
 *	--configuration=N
 *		specify the config index N
 * If cfg index not given on a set, the WLC_SET_BSSID ioctl will be used
 */
static int
wl_bssid(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr ea;
	int bsscfg_idx = 0;
	int consumed;
	int error;

	UNUSED_PARAMETER(cmd);

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "bssid", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	if (*argv == NULL) {
		if (consumed == 0) {
			/* no config index, use WLC_GET_BSSID on the interface */
			error = wlu_get(wl, WLC_GET_BSSID, &ea, ETHER_ADDR_LEN);
		} else {
			/* use "bssid" iovar since a config option was given */
			error = wlu_bssiovar_get(wl, "bssid", bsscfg_idx, &ea, ETHER_ADDR_LEN);
		}
		if (error < 0)
			return error;
		printf("%s\n", wl_ether_etoa(&ea));

	} else {

		if (!wl_ether_atoe(*argv, &ea))
			return BCME_USAGE_ERROR;

		if (consumed == 0) {
			/* no config index given, use WLC_SET_BSSID */
			error = wlu_set(wl, WLC_SET_BSSID, &ea, ETHER_ADDR_LEN);
		} else {
			/* use "bssid" iovar since a config option was given */
			error = wl_bssiovar_set(wl, "bssid", bsscfg_idx, &ea, ETHER_ADDR_LEN);
		}
	}
	return error;
}

static const char*
wl_smfs_map_type(uint8 type)
{
	static const struct {uint8 type; char name[32];} type_names[] = {
		{SMFS_TYPE_AUTH, "Authentication_Request"},
		{SMFS_TYPE_ASSOC, "Association_Request"},
		{SMFS_TYPE_REASSOC, "Reassociation_Request"},
		{SMFS_TYPE_DISASSOC_TX, "Disassociation_Request_TX"},
		{SMFS_TYPE_DISASSOC_RX, "Disassociation_Request_RX"},
		{SMFS_TYPE_DEAUTH_TX, "Deauthentication_Request_TX"},
		{SMFS_TYPE_DEAUTH_RX, "Deauthentication_Request_RX"}
	};

	const char *tname = "UNKNOWN";
	uint i;

	for (i = 0; i < ARRAYSIZE(type_names); i++) {
		if (type_names[i].type == type)
		    tname = type_names[i].name;
	}
	return tname;
}

static int
wl_disp_smfs(char *inbuf)
{
	static const char *codename[] = {"Status_code", "Reason_code"};
	wl_smf_stats_t *smf_stats;
	wl_smfs_elem_t *elemt = NULL;
	const char *namebuf;
	uint32 version;
	int count;

	smf_stats = (wl_smf_stats_t *) inbuf;
	namebuf = wl_smfs_map_type(smf_stats->type);

	version = dtoh32(smf_stats->version);
	if (version != SMFS_VERSION) {
		fprintf(stderr, "Sorry, your driver has smfs_version %d "
			"but this program supports only version %d.\n",
			version, SMFS_VERSION);
		return -1;
	}

	printf("Frame type: %s\n", namebuf);
	printf("\tIgnored Count: %d\n", dtoh32(smf_stats->ignored_cnt));
	printf("\tMalformed Count: %d\n", dtoh32(smf_stats->malformed_cnt));

	count = dtoh32(smf_stats->count_total);

	if (count) {
		namebuf = codename[dtoh32(smf_stats->codetype)];
		printf("\tSuccessful/Failed Count:\n");
		elemt = &smf_stats->elem[0];
	}

	while (count) {
		printf("\t\t%s %d Count: %d\n",  namebuf, dtoh16(elemt->code),
		  dtoh32(elemt->count));
		elemt ++;
		count --;
	}

	return 0;

}

/*
 * Check for the smfstats parameters. One of defined parameters can be passed in.
 */
static int
wl_smfs_option(char **argv, int* idx, int *consumed, int* clear)
{
	int err = 0;
	char *p;
	char const * smfs_opt[] = {"auth", "assoc", "reassoc", "disassoc_tx",
							   "disassoc_rx", "deauth_tx", "deauth_rx"};
	char const * clear_opt = "clear";
	int i;
	char const * cur_opt;

	if (*argv == NULL) {
		goto exit;
	}

	p = *argv++;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		cur_opt = smfs_opt[i];
		if (!strcmp(p, cur_opt)) {
			*idx = i;
			*consumed += 1;
			goto exit;
		}
	}

	if (!strcmp(p, clear_opt))
		*clear = 1;

exit:
	return err;
}

/* Get or Clear (set)  the "smfstats" iovar, with an optional config index argument:
 *	wl smfstats [-C N]|[--cfg=N] 0
 *
 * Option:
 *	-C N
 *	--cfg=N
 *	--config=N
 *	--configuration=N
 *		specify the config index N
 * If cfg index not given on a set, the WLC_SET_SMF_STATS ioctl will be used
 */
static int
wl_smfstats(void *wl, cmd_t *cmd, char **argv)
{
	int bsscfg_idx = 0;
	int cfg_consumed = 0, smfs_consumed = 0;
	int err;
	int i, val;
	int smf_index = 0;
	int smfs_clear = 0;

	BCM_REFERENCE(cmd);

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((err = wl_cfg_option(argv, "smfstats", &bsscfg_idx, &cfg_consumed)) != 0)
		return err;

	argv += cfg_consumed;

	if ((err = wl_smfs_option(argv, &smf_index, &smfs_consumed, &smfs_clear)) != 0)
		return err;

	if (!smfs_clear) {
		if (cfg_consumed == 0) {
			if (smfs_consumed) {
				err = wlu_iovar_getbuf(wl, "smfstats", &smf_index, sizeof(int),
				   buf, WLC_IOCTL_SMLEN);
				if (!err)
					err = wl_disp_smfs(buf);
			}
			else {
				for (i = 0; i < SMFS_TYPE_MAX; i++) {
					smf_index = i;
					err = wlu_iovar_getbuf(wl, "smfstats", &smf_index,
					   sizeof(int), buf, WLC_IOCTL_SMLEN);
					if (!err)
						err = wl_disp_smfs(buf);
				}
			}
		} else {
			/* use "stats" iovar since a config option was given */
			if (smfs_consumed) {
				err = wl_bssiovar_getbuf(wl, "smfstats", bsscfg_idx, &smf_index,
				  sizeof(int), buf, WLC_IOCTL_SMLEN);
				if (!err)
					err = wl_disp_smfs(buf);
			}
			else {
				for (i = 0; i < SMFS_TYPE_MAX; i++) {
					smf_index = i;
					err = wl_bssiovar_getbuf(wl, "smfstats", bsscfg_idx,
						&smf_index, sizeof(int), buf, WLC_IOCTL_SMLEN);
					if (!err)
						err = wl_disp_smfs(buf);
				}
			}
		}
		if (err < 0)
			return err;
	} else {
		val = 0;

		if (cfg_consumed == 0)
			err = wlu_iovar_setint(wl, "smfstats", val);
		else
			err = wl_bssiovar_setint(wl, "smfstats", bsscfg_idx, val);

	}
	return err;
}

/* Quarter dBm units to mW
 * Table starts at QDBM_OFFSET, so the first entry is mW for qdBm=153
 * Table is offset so the last entry is largest mW value that fits in
 * a uint16.
 */

#define QDBM_OFFSET 153 /* QDBM_OFFSET */
#define QDBM_TABLE_LEN 40 /* QDBM_TABLE_LEN */

/* Smallest mW value that will round up to the first table entry, QDBM_OFFSET.
 * Value is ( mW(QDBM_OFFSET - 1) + mW(QDBM_OFFSET) ) / 2
 */
#define QDBM_TABLE_LOW_BOUND 6493 /* QDBM_TABLE_LOW_BOUND */

/* Largest mW value that will round down to the last table entry,
 * QDBM_OFFSET + QDBM_TABLE_LEN-1.
 * Value is ( mW(QDBM_OFFSET + QDBM_TABLE_LEN - 1) + mW(QDBM_OFFSET + QDBM_TABLE_LEN) ) / 2.
 */
#define QDBM_TABLE_HIGH_BOUND 64938 /* QDBM_TABLE_HIGH_BOUND */

static const uint16 nqdBm_to_mW_map[QDBM_TABLE_LEN] = {
/* qdBm:        +0	+1	+2	+3	+4	+5	+6	+7	*/
/* 153: */      6683,	7079,	7499,	7943,	8414,	8913,	9441,	10000,
/* 161: */      10593,	11220,	11885,	12589,	13335,	14125,	14962,	15849,
/* 169: */      16788,	17783,	18836,	19953,	21135,	22387,	23714,	25119,
/* 177: */      26607,	28184,	29854,	31623,	33497,	35481,	37584,	39811,
/* 185: */      42170,	44668,	47315,	50119,	53088,	56234,	59566,	63096
};

static uint16
wl_qdbm_to_mw(int8 qdbm)
{
	uint factor = 1;
	int idx = qdbm - QDBM_OFFSET;

	if (idx >= QDBM_TABLE_LEN) {
		/* clamp to max uint16 mW value */
		return 0xFFFF;
	}

	/* scale the qdBm index up to the range of the table 0-40
	 * where an offset of 40 qdBm equals a factor of 10 mW.
	 */
	while (idx < 0) {
		idx += 40;
		factor *= 10;
	}

	/* return the mW value scaled down to the correct factor of 10,
	 * adding in factor/2 to get proper rounding.
	 */
	return ((nqdBm_to_mW_map[idx] + factor/2) / factor);
}

static int8
wl_mw_to_qdbm(uint16 mw)
{
	uint8 qdbm;
	int offset;
	uint mw_uint = mw;
	uint boundary;

	/* handle boundary case */
	if (mw_uint <= 1)
		return WL_RATE_DISABLED;

	offset = QDBM_OFFSET;

	/* move mw into the range of the table */
	while (mw_uint < QDBM_TABLE_LOW_BOUND) {
		mw_uint *= 10;
		offset -= 40;
	}

	for (qdbm = 0; qdbm < QDBM_TABLE_LEN-1; qdbm++) {
		boundary = nqdBm_to_mW_map[qdbm] +
			(nqdBm_to_mW_map[qdbm+1] - nqdBm_to_mW_map[qdbm])/2;
		if (mw_uint < boundary) break;
	}

	qdbm += (int8)offset;

	return (qdbm);
}

#define WLC_TXPWR_DB_FACTOR 4
#define UNIT_MW		1 /* UNIT_MW */
#define UNIT_QDBM	2 /* UNIT_QDBM */
#define UNIT_DBM	3 /* UNIT_DBM */

static int
wl_txpwr1(void *wl, cmd_t *cmd, char **argv)
{
	int ret, val, new_val = 0, unit;
	const char *name = "qtxpower";
	bool override = FALSE;
	int8 temp_val;

	if (!*++argv) {
		int divquo, divrem;
		bool neg;

		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_iovar_getint(wl, name, &val)) < 0)
			return ret;

		override = ((val & WL_TXPWR_OVERRIDE) != 0);
		val &= ~WL_TXPWR_OVERRIDE;
		temp_val = (int8)(val & 0xff);

		divquo = DIV_QUO(temp_val, 4);
		divrem = DIV_REM(temp_val, 4);
		neg = (divrem < 0) || (divquo < 0);
		divrem = ABS(divrem);
		divquo = ABS(divquo);

		printf("TxPower is %d qdbm, %s%d.%d dbm, %d mW  Override is %s\n",
		       temp_val, neg ? "-" : "", divquo, divrem,
		       (temp_val < 4) ? 0 : wl_qdbm_to_mw(temp_val),
		       override ? "On" : "Off");
		return 0;
	} else {
		/* for set */
		bool unit_set = FALSE;
		unit = UNIT_DBM;	/* default units */

		/* override can be used in combo with any unit */
		if (!strcmp(*argv, "-o")) {
				override = TRUE;
			if (!*++argv)
				return BCME_USAGE_ERROR;
		}

		if (!strcmp(*argv, "-d")) {
				unit = UNIT_DBM;
				unit_set = TRUE;
			argv++;
		}
		else if (!strcmp(*argv, "-q")) {
				unit = UNIT_QDBM;
				unit_set = TRUE;
			argv++;
		}
		else if (!strcmp(*argv, "-m")) {
				unit = UNIT_MW;
				unit_set = TRUE;
			argv++;
		}

		if (!*argv)
			return BCME_USAGE_ERROR;

		/* override can be used in combo with any unit */
		if (!strcmp(*argv, "-o")) {
			override = TRUE;
			argv++;
		}

		if (!*argv)
			return BCME_USAGE_ERROR;

		val = atoi(*argv);

		if (!unit_set) {
			if (val == -1) {
				val = WLC_TXPWR_MAX;		/* Max val of 127 qdbm */
					unit = UNIT_QDBM;
			} else if (val <= 0) {
				return BCME_BADARG;
			}
		}

		if ((val <= 0) && (unit == UNIT_MW)) {
			return BCME_BADARG;
		}

		switch (unit) {
		case UNIT_MW:
			new_val = (uint8)wl_mw_to_qdbm((uint16)MIN(val, 0xffff));
			break;
		case UNIT_DBM:
			if (val > (WLC_TXPWR_MAX / WLC_TXPWR_DB_FACTOR))
				return BCME_BADARG;
			val *= WLC_TXPWR_DB_FACTOR;
			if (val < WL_RATE_DISABLED)
				val = WL_RATE_DISABLED;
			temp_val = (int8)val;
			new_val = (uint8)temp_val; /* need to keep sign bit in low byte */
			break;
		case UNIT_QDBM:
			if (val > WLC_TXPWR_MAX)
				return BCME_BADARG;
			if (val < WL_RATE_DISABLED)
				val = WL_RATE_DISABLED;
			temp_val = val;
			new_val = (uint8)temp_val; /* need to keep sign bit in low byte */
			break;
		}

		if (override)
			new_val |= WL_TXPWR_OVERRIDE;

		return wlu_iovar_setint(wl, name, new_val);
	}
}

static int
wl_txpwr(void *wl, cmd_t *cmd, char **argv)
{
	int error;
	uint32 val;
	char *endptr = NULL;
	uint32 override;
	const char *name = "qtxpower";

	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		if ((error = wlu_iovar_getint(wl, name, (int *)&val)) < 0)
			return error;

		/* Report power in mw with WL_TXPWR_OVERRIDE
		 * bit indicating the status
		 */
		override = ((val & WL_TXPWR_OVERRIDE) != 0);
		val &= ~WL_TXPWR_OVERRIDE;
		printf("%d.%d dBm = %d mw.  %s\n", DIV_QUO(val, 4), DIV_REM(val, 4),
			wl_qdbm_to_mw((uint8)(MIN(val, 0xff))), (override ? "(Override ON)" : ""));
		return 0;
	} else {
		if (!strcmp(*argv, "-u")) {
			override = 0;
			argv++;
		} else
			override = WL_TXPWR_OVERRIDE;

		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			return BCME_USAGE_ERROR;
		}

		val = wl_mw_to_qdbm((uint16)MIN(val, 0xffff));

		/* wl command input power will override current power set if told so */
		val |= override;

		return wlu_iovar_setint(wl, name, val);
	}
}

static int
wl_get_txpwr_limit(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint8 val_qdbm;
	uint16 val_mw;
	tx_power_legacy_t power;

	UNUSED_PARAMETER(argv);

	ret = wlu_get(wl, cmd->get, &power, sizeof(power));
	if (ret < 0)
		return ret;

	val_qdbm = MIN(power.txpwr_band_max[0], power.txpwr_local_max);
	val_mw = wl_qdbm_to_mw((uint8)(MIN(val_qdbm, 0xff)));

	printf("%d mW (%d.%d dBm)\n", val_mw, DIV_QUO(val_qdbm, 4), DIV_REM(val_qdbm, 4));

	return ret;
}

static int
wl_txpwr_cap(void *wl, cmd_t *cmd, char **argv)
{
	uint i;
	int ret;
	uint32 txpwrcap[4] = { 0 };
	int8 idx[4] = { 0 };
	uint argc;
	char *endptr;

	wlc_rev_info_t revinfo;
	uint32 corerev;

	if (cmd->get < 0)
		return -1;
	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	corerev = dtoh32(revinfo.corerev);
	if (corerev < 128) {
		printf("error\n");
		return BCME_UNSUPPORTED;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	for (i = 0; i < 4; i++) {
		if (argc > i) {
			txpwrcap[i] = (uint32)strtol(argv[1 + i], &endptr, 0);
			if (*endptr != '\0') {
				printf("error\n");
				return BCME_USAGE_ERROR;
			}
		}
	}

	if (argc == 0) {
		if ((ret = wlu_iovar_getint(wl, cmd->name, (int*)&txpwrcap[0])) < 0) {
			return (ret);
		}
		txpwrcap[0] = dtoh32(txpwrcap[0]);
		idx[0] = (int8)(txpwrcap[0] & 0xff);
		idx[1] = (int8)((txpwrcap[0] >> 8) & 0xff);
		idx[2] = (int8)((txpwrcap[0] >> 16) & 0xff);
		idx[3] = (int8)((txpwrcap[0] >> 24) & 0xff);
		printf("txpwr_cap for core{0...3}: %d %d %d %d\n", idx[0], idx[1],
		       idx[2], idx[3]);
	} else {
		for (i = 0; i < 4; i++) {
			if (txpwrcap[i] > 127) {
				printf("exceed the limit\n");
				return BCME_USAGE_ERROR;
			}
		}
		ret = wlu_iovar_setbuf(wl, cmd->name, txpwrcap, sizeof(txpwrcap),
			buf, WLC_IOCTL_MAXLEN);
	}

	return ret;
}

#define	IOCTLECHO_MAX_SIZE	1000

#if IOCTLECHO_MAX_SIZE + 2 > WLC_IOCTL_MAXLEN
#error INVALID IOCTLECHO_MAX_SIZE
#endif // endif

#ifndef ATE_BUILD
static int
wl_echo(void *wl, cmd_t *cmd, char **argv)
{
char *endptr;
uint len = 0;
uint  i;
int	ret;

	UNUSED_PARAMETER(cmd);

	if (argv[1])
		len = strtoul(argv[1], &endptr, 0);

	if (len > IOCTLECHO_MAX_SIZE) {
		printf("maximum allowed size is : %d\n", IOCTLECHO_MAX_SIZE);
		len = IOCTLECHO_MAX_SIZE;
	}

	memset(buf, 0, len+2);

	for (i = 0; i < len; i++)
		buf[2+i] = (char)i;

	((ushort*)buf)[0] = len;

	if ((ret = wlu_get(wl, WLC_ECHO, buf, len+2)) < 0)
		return ret;

	if (((ushort*)buf)[0] != len)	{
		printf("error read size is different then write size\n");
		return -1;
	}

	for (i = 0; i < ((ushort*)buf)[0]; i++)
		if (buf[2+i] != (char)i) {
			printf("error read data is different then write data\n");
			return -1;
		}

	printf("write buffer and read buffer are identical\n");

	return 0;

}
#endif /* !ATE_BUILD */

static int
wl_out(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	return wlu_set(wl, WLC_OUT, NULL, 0);
}

static int
wl_band(void *wl, cmd_t *cmd, char **argv)
{
	uint band;
	int error;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if (*argv == NULL) {	/* get current band */
		if ((error = wlu_get(wl, WLC_GET_BAND, &band, sizeof(uint))) < 0)
			return (error);
		band = dtoh32(band);

		if (band == WLC_BAND_AUTO)
			printf("auto\n");
		else if (band == WLC_BAND_5G)
			printf("a\n");
		else if (band == WLC_BAND_2G)
			printf("b\n");
		else {
			printf("unrecognized band value %d\n", band);
			error = BCME_ERROR;
		}
	} else {		/* set the band */
		if (!stricmp(*argv, "auto"))
			band = WLC_BAND_AUTO;
		else if (!stricmp(*argv, "a"))
			band = WLC_BAND_5G;
		else if (!stricmp(*argv, "b"))
			band = WLC_BAND_2G;
		else {
			printf("unsupported band: %s\n", *argv);
			return BCME_UNSUPPORTED;
		}

		band = htod32(band);
		error = wlu_set(wl, WLC_SET_BAND, &band, sizeof(uint));

	}
	return (error);
}

static int
wl_bandlist(void *wl, cmd_t *cmd, char **argv)
{
	uint list[3];
	int error;
	uint i;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if ((error = wlu_get(wl, WLC_GET_BANDLIST, list, sizeof(list))) < 0)
		return (error);
	list[0] = dtoh32(list[0]);
	list[1] = dtoh32(list[1]);
	list[2] = dtoh32(list[2]);

	/* list[0] is count, followed by 'count' bands */

	if (list[0] > 2)
		list[0] = 2;

	for (i = 1; i <= list[0]; i++)
		if (list[i] == WLC_BAND_5G)
			printf("a ");
		else if (list[i] == WLC_BAND_2G)
			printf("b ");
		else
			printf("? ");
	printf("\n");

	return (0);
}

static int
wl_phylist(void *wl, cmd_t *cmd, char **argv)
{
	char phylist_buf[128];
	int error;
	char *cp;

	UNUSED_PARAMETER(cmd);

	error = 0;

	argv++;

	if ((error = wlu_get(wl, WLC_GET_PHYLIST, phylist_buf, sizeof(phylist_buf))) < 0)
		return (error);

	cp = phylist_buf;

	for (; *cp; cp++)
		printf("%c ", *cp);
	printf("\n");

	return (0);
}

#ifdef linux
#define UPGRADE_BUFSIZE	512 /* upgrade buffer size */
#else
#define UPGRADE_BUFSIZE	1024 /* upgrade buffer size */
#endif /* linux */

static int
wl_upgrade(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif	defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return 0;
#else
	FILE *fp;
	int ret = 0;
	struct {
		uint32 offset;
		char buf[UPGRADE_BUFSIZE];
	} block;
	uint32 offset;
	uint len;

	if (!*++argv)
		return BCME_USAGE_ERROR;

	if (!(fp = fopen(*argv, "rb"))) {
		fprintf(stderr, "%s: No such file or directory\n", *argv);
		return BCME_BADARG;
	}

	printf("Programming %s...", *argv);
	fflush(stdout);
	offset = 0;
	block.offset = htod32(offset);
	while ((len = fread(block.buf, 1, sizeof(block.buf), fp))) {
		if ((ret = wlu_set(wl, cmd->set, &block, 4 + len)) < 0)
			break;
		offset += len;
		block.offset = htod32(offset);
		printf(".");
		fflush(stdout);
	}

	if (ferror(fp)) {
		ret = ferror(fp);
		printf("\nerror reading %s\n", *argv);
	} else {
		long status = WLC_UPGRADE_PENDING;
		int retries;

		printf("\nCommitting image to flash...\n");
		while (status == WLC_UPGRADE_PENDING) {
			retries = 10;
retry:
			if ((ret = wlu_get(wl, WLC_UPGRADE_STATUS,
				&status, sizeof(status))) < 0) {
				/* the first attempt to get status will
				 * likely fail due to dev reset
				 */
				if (retries--)
					goto retry;
				break;
			}
			status = dtoh32(status);
		}
		if (status == WLC_UPGRADE_SUCCESS)
			printf("\nDone\n\nSuccessfully downloaded %d bytes\n", block.offset);
		else
			fprintf(stderr, "\n*** UPGRADE FAILED! *** (status %ld)\n", status);
	}

	fclose(fp);
	return ret;
#endif   /* BWL_FILESYSTEM_SUPPORT */
}

static int
wl_get_pktcnt(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	get_pktcnt_t pktcnt;

	UNUSED_PARAMETER(argv);

	memset(&pktcnt, 0, sizeof(pktcnt));
	if ((ret = wlu_get(wl, cmd->get, &pktcnt, sizeof(pktcnt))) < 0)
		return ret;

	printf("Receive: good packet %d, bad packet %d, othercast good packet %d\n",
		dtoh32(pktcnt.rx_good_pkt), dtoh32(pktcnt.rx_bad_pkt),
		dtoh32(pktcnt.rx_ocast_good_pkt));
	printf("Transmit: good packet %d, bad packet %d\n",
		dtoh32(pktcnt.tx_good_pkt), dtoh32(pktcnt.tx_bad_pkt));

	return ret;
}

static cntry_name_t *
wlc_cntry_name_to_country(char *long_name)
{
	cntry_name_t *cntry;
	for (cntry = cntry_names; cntry->name &&
		stricmp(long_name, cntry->name); cntry++);
	return (!cntry->name ? NULL : cntry);
}

static cntry_name_t *
wlc_cntry_abbrev_to_country(const char *abbrev)
{
	cntry_name_t *cntry;
	if (!*abbrev || strlen(abbrev) > 3 || strlen(abbrev) < 2)
		return (NULL);
	for (cntry = cntry_names; cntry->name &&
		strnicmp(abbrev, cntry->abbrev, strlen(abbrev)); cntry++);
	return (!cntry->name ? NULL : cntry);
}

static int
wl_parse_country_spec(const char *spec, char *ccode, int *regrev)
{
	char *revstr;
	char *endptr = NULL;
	int ccode_len;
	int rev = -1;

	revstr = strchr(spec, '/');

	if (revstr) {
		rev = strtol(revstr + 1, &endptr, 10);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			fprintf(stderr,
				"Could not parse \"%s\" as a regulatory revision "
				"in the country string \"%s\"\n",
				revstr + 1, spec);
			return BCME_USAGE_ERROR;
		}
	}

	if (revstr)
		ccode_len = (int)(uintptr)(revstr - spec);
	else
		ccode_len = (int)strlen(spec);

	if (ccode_len > 3) {
		fprintf(stderr,
			"Could not parse a 2-3 char country code "
			"in the country string \"%s\"\n",
			spec);
		return BCME_USAGE_ERROR;
	}

	memcpy(ccode, spec, ccode_len);
	ccode[ccode_len] = '\0';
	*regrev = rev;

	return 0;
}

int
wl_country(void *wl, cmd_t *cmd, char **argv)
{
	cntry_name_t *cntry;
	wl_country_t cspec = {{0}, 0, {0}};
	int argc = 0;
	int err;
	int bcmerr = 1;
	int interr = 1;

	/* skip the command name */
	argv++;

	/* find the arg count */
	while (argv[argc])
		argc++;

	/* check arg list count */
	if (argc > 2) {
		fprintf(stderr, "Too many arguments (%d) for command %s\n", argc, cmd->name);
		return BCME_USAGE_ERROR;
	}

	buf[0] = 0;
	if (argc == 0) {
		const char* name = "<unknown>";

		/* first try the country iovar */
		err = wlu_iovar_get(wl, "country", &cspec, sizeof(cspec));

		if (!err) {
			cntry = wlc_cntry_abbrev_to_country(cspec.country_abbrev);
			if (cntry)
				name = cntry->name;
			cspec.rev = dtoh32(cspec.rev);

			printf("%s (%s/%d) %s\n",
			       cspec.country_abbrev, cspec.ccode, cspec.rev, name);

			return 0;
		}

		/* if there was an error other than BCME_UNSUPPORTED, fail now */
		interr = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
		if (!interr && (bcmerr != BCME_UNSUPPORTED))
			return err;

		/* if the "country" iovar is unsupported, try the WLC_SET_COUNTRY ioctl */
		if ((err = wlu_get(wl, cmd->get, &buf[0], WLC_IOCTL_SMLEN)) < 0)
			return err;
		if (strlen(buf) == 0) {
			printf("No country set\n");
			return 0;

		}
		cntry = wlc_cntry_abbrev_to_country(buf);
		if (cntry != NULL)
			name = cntry->name;

		printf("%s () %s\n", buf, name);
		return 0;
	}

	if (!stricmp(*argv, "list")) {
		uint i;
		const char* abbrev;
		wl_country_list_t *cl = (wl_country_list_t *)buf;

		cl->buflen = WLC_IOCTL_MAXLEN;
		cl->count = 0;

		/* band may follow */
		if (*++argv) {
			cl->band_set = TRUE;
			if (!stricmp(*argv, "a"))
				cl->band = WLC_BAND_5G;
			else if (!stricmp(*argv, "b") || !stricmp(*argv, "g"))
				cl->band = WLC_BAND_2G;
			else {
				printf("unsupported band: %s\n", *argv);
				return BCME_UNSUPPORTED;
			}
		} else {
			cl->band_set = FALSE;
		}

		cl->buflen = htod32(cl->buflen);
		cl->band_set = htod32(cl->band_set);
		cl->band = htod32(cl->band);
		cl->count = htod32(cl->count);
		err = wlu_get(wl, WLC_GET_COUNTRY_LIST, buf, WLC_IOCTL_MAXLEN);
		if (err < 0)
			return err;

		printf("Supported countries: country code and long name\n");
		for (i = 0; i < dtoh32(cl->count); i++) {
			abbrev = &cl->country_abbrev[i*WLC_CNTRY_BUF_SZ];
			cntry = wlc_cntry_abbrev_to_country(abbrev);
			printf("%s\t%s\n", abbrev, cntry ? cntry->name : "");
		}
		return 0;
	}

	memset(&cspec, 0, sizeof(cspec));
	cspec.rev = -1;

	if (argc == 1) {
		/* check for the first arg being a country name, e.g. "United States",
		 * or country spec, "US/1", or just a country code, "US"
		 */
		if ((cntry = wlc_cntry_name_to_country(argv[0])) != NULL) {
			/* arg matched a country name */
			memcpy(cspec.country_abbrev, cntry->abbrev, WLC_CNTRY_BUF_SZ);
			err = 0;
		} else {
			/* parse a country spec, e.g. "US/1", or a country code.
			 * cspec.rev will be -1 if not specified.
			 */
			err = wl_parse_country_spec(argv[0], cspec.country_abbrev, &cspec.rev);
		}

		if (err) {
			fprintf(stderr,
				"Argument \"%s\" could not be parsed as a country name, "
				"country code, or country code and regulatory revision.\n",
				argv[0]);
			return BCME_USAGE_ERROR;
		}

		/* if the arg was a country spec, then fill out ccdoe and rev,
		 * and leave country_abbrev defaulted to the ccode
		 */
		if (cspec.rev != -1)
			memcpy(cspec.ccode, cspec.country_abbrev, WLC_CNTRY_BUF_SZ);
	} else {
		/* for two args, the first needs to be a country code or country spec */
		err = wl_parse_country_spec(argv[0], cspec.ccode, &cspec.rev);
		if (err) {
			fprintf(stderr,
				"Argument 1 \"%s\" could not be parsed as a country code, or "
				"country code and regulatory revision.\n",
				argv[0]);
			return BCME_USAGE_ERROR;
		}

		/* the second arg needs to be a country name or country code */
		if ((cntry = wlc_cntry_name_to_country(argv[1])) != NULL) {
			/* arg matched a country name */
			memcpy(cspec.country_abbrev, cntry->abbrev, WLC_CNTRY_BUF_SZ);
		} else {
			int rev;
			err = wl_parse_country_spec(argv[1], cspec.country_abbrev, &rev);
			if (rev != -1) {
				fprintf(stderr,
					"Argument \"%s\" had a revision. Arg 2 must be "
					"a country name or country code without a revision\n",
					argv[1]);
				return BCME_USAGE_ERROR;
			}
		}

		if (err) {
			fprintf(stderr,
				"Argument 2 \"%s\" could not be parsed as "
				"a country name or country code\n",
				argv[1]);
			return BCME_USAGE_ERROR;
		}
	}

	/* first try the country iovar */
	if (cspec.rev == -1 && cspec.ccode[0] == '\0')
		err = wlu_iovar_set(wl, "country", &cspec, WLC_CNTRY_BUF_SZ);
	else {
		cspec.rev = htod32(cspec.rev);
		err = wlu_iovar_set(wl, "country", &cspec, sizeof(cspec));
	}

	if (err == 0)
		return 0;

	/* if there was an error other than BCME_UNSUPPORTED, fail now */
	interr = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
	if (!interr && (bcmerr != BCME_UNSUPPORTED))
		return err;

	/* if the "country" iovar is unsupported, try the WLC_SET_COUNTRY ioctl if possible */

	if (cspec.rev != -1 || cspec.ccode[0] != '\0') {
		fprintf(stderr,
			"Driver does not support full country spec interface, "
			"only a country name or code may be sepcified\n");
		return err;
	}

	/* use the legacy ioctl */
	err = wlu_set(wl, WLC_SET_COUNTRY, cspec.country_abbrev, WLC_CNTRY_BUF_SZ);

	return err;
}

int
wl_country_ie_override(void *wl, cmd_t *cmd, char **argv)
{
	int argc = 0;
	int error, i;

	/* skip the command name */
	argv++;

	/* find the arg count */
	while (argv[argc])
		argc++;

	if (argc == 0) {
		void *ptr;
		bcm_tlv_t *ie;

		if ((error = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return (error);

		ie = (bcm_tlv_t *)ptr;

		printf("ie tag:0x%x ie len:0x%x ie data:", ie->id, ie->len);

		for (i = 0; i < ie->len; i++)
			printf("0x%x ", ie->data[i]);

		printf("\n");

		return error;

	} else {
		/* Set */
		char *endptr = NULL;
		uchar *valsp;
		int8 ie_len, pad = 0;

		/* retrieve the ie len in advance to check for padding */
		ie_len = (int8)strtol(*(argv + 1), NULL, 0);
		if (ie_len & 1) {
			fprintf(stderr, "country ie len is odd(%d), padding by 1 octet\n", ie_len);
			pad = 1;
		}

		valsp = (uchar*)malloc(argc + pad);
		if (valsp == NULL) {
			fprintf(stderr, "Error allocating %d bytes country ie\n", argc);
			return BCME_NOMEM;
		}
		memset(valsp, 0, argc + pad);

		for (i = 0; i < argc; i++, argv++) {

			valsp[i] = (uchar)strtol(*argv, &endptr, 0);

			/* make sure all the value string was parsed by strtol */
			if (*endptr != '\0') {
				free(valsp);
				return BCME_USAGE_ERROR;
			}
		}

		/* update ie len if padded */
		if (pad) {
			valsp[1] += 1;
			valsp[ie_len + TLV_HDR_LEN] = 0;
		}

		error = wlu_var_setbuf(wl, cmd->name, valsp, argc + pad);

		free(valsp);

		return error;
	}
}

static int
wl_actframe(void *wl, cmd_t *cmd, char **argv)
{
	wl_action_frame_t * action_frame;
	wl_af_params_t * af_params;
	struct ether_addr ea;
	int argc;
	int err = 0;

	UNUSED_PARAMETER(cmd);

	if (!argv[1] || !argv[2]) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if ((af_params = (wl_af_params_t *) malloc(WL_WIFI_AF_PARAMS_SIZE)) == NULL) {
		printf("wl_actframe: unable to allocate frame \n");
		return BCME_NOMEM;
	}
	af_params->channel = 0;
	af_params->dwell_time = -1;
	action_frame = &af_params->action_frame;

	/* Add the packet Id */
	action_frame->packetId = (uint32)(uintptr)action_frame;

	/* convert the ea string into an ea struct */
	if (!wl_ether_atoe(argv[1], &ea)) {
		free(af_params);
		printf(" ERROR: no valid ether addr provided\n");
		return BCME_USAGE_ERROR;
	}
	memcpy(&action_frame->da, (char*)&ea, ETHER_ADDR_LEN);
	/* set default BSSID */
	memcpy(&af_params->BSSID, (char*)&ea, ETHER_ADDR_LEN);

	/* add the length */
	if (argv[2]) {
		action_frame->len  = htod16(strlen(argv[2])) / 2;
	}

	/* add the channel */
	if (argc > 3 && argv[3]) {
		af_params->channel = htod32(atoi(argv[3]));
	}

	/* add the dwell_time */
	if (argc > 4 && argv[4]) {
		af_params->dwell_time = htod32(atoi(argv[4]));
	}

	/* add the BSSID */
	if (argc > 5 && argv[5]) {
		if (!wl_ether_atoe(argv[5], &ea)) {
			free(af_params);
			printf(" ERROR: no valid ether addr provided\n");
			return BCME_USAGE_ERROR;
		}
		memcpy(&af_params->BSSID, (char*)&ea, ETHER_ADDR_LEN);
	}

	if ((err = get_ie_data ((uchar *)argv[2],
		&action_frame->data[0],
		action_frame->len))) {
		free(af_params);
		fprintf(stderr, "Error parsing data arg\n");
		return err;
	}
	err = wlu_var_setbuf(wl, "actframe", af_params, WL_WIFI_AF_PARAMS_SIZE);

	free(af_params);

	return (err);

}

static int
wl_dfs_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	void *ptr;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return ret;

	ret = wl_print_dfs_status(ptr);

	return ret;
}

static int
wl_print_dfs_status(wl_dfs_status_t *dfs_status)
{
	char chanspec_str[CHANSPEC_STR_LEN];

	dfs_status->state = dtoh32(dfs_status->state);
	dfs_status->duration = dtoh32(dfs_status->duration);
	dfs_status->chanspec_cleared = wl_chspec_from_driver(dfs_status->chanspec_cleared);

	if (dfs_status->state >= WL_DFS_CACSTATES) {
		printf("Unknown dfs state %d.\n", dfs_status->state);
		return -1;
	}

	printf("state %s time elapsed %dms radar channel cleared by dfs ",
		dfs_cacstate_str[dfs_status->state], dfs_status->duration);

	if (dfs_status->chanspec_cleared) {
		printf("channel %s (0x%04X)\n",
		       wf_chspec_ntoa(dfs_status->chanspec_cleared, chanspec_str),
		       dfs_status->chanspec_cleared);
	}
	else {
		printf("none\n");
	}
	return 0;
}

static int
wl_dfs_status_all(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	void *ptr;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return ret;

	ret = wl_print_dfs_status_all(ptr);

	return ret;
}

/* prints dfs status from one subset */
static int
wl_print_dfs_sub_status(wl_dfs_sub_status_t *sub)
{
	char chanspec_str[CHANSPEC_STR_LEN];

	sub->state = dtoh32(sub->state);
	sub->duration = dtoh32(sub->duration);
	sub->chanspec = wl_chspec_from_driver(sub->chanspec);
	sub->chanspec_last_cleared = wl_chspec_from_driver(sub->chanspec_last_cleared);
	sub->sub_type = dtoh16(sub->sub_type);

	// state
	if (sub->state >= WL_DFS_CACSTATES) {
		printf("Unknown dfs state %d.\n", sub->state);
		return -1;
	}
	printf("state: %s, time elapsed: %dms, chanspec: ",
			dfs_cacstate_str[sub->state], sub->duration);
	// chanspec
	if (sub->chanspec) {
		printf("%s (0x%04X), chanspec last cleared: ",
				wf_chspec_ntoa(sub->chanspec, chanspec_str),
				sub->chanspec);
	} else {
		printf("none, chanspec last cleared: ");
	}
	// chanspec last cleared
	if (sub->chanspec_last_cleared) {
		printf("%s (0x%04X), ",
				wf_chspec_ntoa(sub->chanspec_last_cleared, chanspec_str),
				sub->chanspec_last_cleared);
	} else {
		printf("none, ");
	}
	// sub type
	printf("sub type: 0x%02x\n", sub->sub_type);

	return 0;
}

/* prints dfs status of all subsets received */
static int
wl_print_dfs_status_all(wl_dfs_status_all_t *dfs_status_all)
{
	int count;
	int err;
	if (dfs_status_all == NULL) {
		return BCME_ERROR;
	}

	dfs_status_all->version = dtoh16(dfs_status_all->version);
	dfs_status_all->num_sub_status = dtoh16(dfs_status_all->num_sub_status);

	if (dfs_status_all->version != WL_DFS_STATUS_ALL_VERSION) {
		err = BCME_UNSUPPORTED;
		printf("err=%d version=%d\n", err, dfs_status_all->version);
		return err;
	}

	printf("version: %d, num_sub_status: %d\n",
			dfs_status_all->version, dfs_status_all->num_sub_status);

	for (count = 0; count < dfs_status_all->num_sub_status; ++count) {
		printf("@%d: ", count);
		if ((err = wl_print_dfs_sub_status(&dfs_status_all->dfs_sub_status[count]))
				!= BCME_OK) {
			return err;
		}
	}

	return BCME_OK;
}

static int
wl_radar_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint i;
	wl_radar_status_t ra;
	char chanspec_str[CHANSPEC_STR_LEN];
	static const struct {
		uint32 radar_type;
		const char *radar_type_name;
	} radar_names[] = {
		{0, "NONE"},
		{1, "ETSI_0"},
		{2, "ETSI_1"},
		{3, "ETSI_2"},
		{4, "ETSI_3"},
		{5, "ETSI_4"},
		{6, "ETSI_5-S2"},
		{7, "ETSI_5-S3"},
		{8, "ETSI_6-S2"},
		{9, "ETSI_6-S3"},
		{10, "FCC-0"},
		{11, "FCC-1"},
		{12, "FCC-2"},
		{13, "FCC-3"},
		{14, "FCC-4"},
		{15, "FCC-5"},
		{16, "FCC-6"},
		{17, "JP1-1"},
		{18, "JP1-2"},
		{19, "JP2-1-1"},
		{20, "JP2-1-2"},
		{21, "JP2-2-1"},
		{22, "JP2-2-2"},
		{23, "JP2-2-3"},
		{24, "JP2-2-4"},
		{25, "JP3"},
		{26, "JP4"},
		{27, "KN1"},
		{28, "KN2"},
		{29, "KN3"},
		{30, "KN4"},
		{31, "UK1"},
		{32, "UK2"},
		{255, "UNCLASSIFIED"},
	};

	char radar_type_str[24];

	UNUSED_PARAMETER(argv);
	if ((ret = wlu_iovar_get(wl, cmd->name, &ra, sizeof(ra))) < 0)
		return ret;
	ra.ch = wl_chspec_from_driver(ra.ch);

	if (ra.detected == FALSE) {
		printf("NO RADAR DETECTED \n");
	} else {
		  for (i = 0; i < ARRAYSIZE(radar_names); i++) {
			if (radar_names[i].radar_type == ra.radartype)
				snprintf(radar_type_str, sizeof(radar_type_str),
					"%s", radar_names[i].radar_type_name);
		}

		  if (ra.pretended == TRUE) {
		    printf("DFS: NONE ########## RADAR DETECTED  ON CHAN  %s \n",
		    wf_chspec_ntoa(ra.ch, chanspec_str));
		  } else {
		    if (ra.radartype == 8) {
		      printf("DFS: FCC-5 ########## RADAR DETECTED  ON CHAN %s \n",
		      wf_chspec_ntoa(ra.ch, chanspec_str));
		      printf(" ########## lp_csect_single = %d, Time from last detection = %u, ",
		      ra.lp_csect_single, ra.timefromL);
		      printf(" = %dmin %dsec AT %dMS \n ",
		      ra.timefromL/60, ra.timefromL%60, ra.timenow);
		    } else {
		      printf("DFS: %s ########## RADAR DETECTED  ON CHAN %s \n",
		      radar_type_str, wf_chspec_ntoa(ra.ch, chanspec_str));
		      printf(" ########## detected_pulse_index= %d, nconseq_pulses = %d, ",
		      ra.detected_pulse_index, ra.nconsecq_pulses);
		      printf(" Time from last detection = %u, = %dmin %dsec AT %dMS \n",
		      ra.timefromL, ra.timefromL/60, ra.timefromL%60, ra.timenow);
		      printf("Pruned Intv: ");
		      for (i = 0; i < 10; i++) {
			printf("%d-%d ", ra.intv[i], i);
		      }
		      printf("\n");

		      printf("Pruned PW:  ");
		      for (i = 0; i < 10; i++) {
			printf("%i-%d ", ra.pw[i], i);
		      }
		      printf("\n");

		      printf("Pruned FM:  ");
		      for (i = 0; i < 10; i++) {
			printf("%i-%d ", ra.fm[i], i);
		      }
		      printf("\n");
#ifdef BCMDBG
		      printf("Pruned FC:  ");
		      for (i = 0; i < 10; i++) {
		        printf("%i-%d ", ra.fc[i], i);
		      }
		      printf("\n");

		      printf("Pruned Chirp:  ");
		      for (i = 0; i < 10; i++) {
		        printf("%i-%d ", ra.chirp[i], i);
		      }
		      printf("\n");
#endif // endif
		    }
		  }
	}
		return ret;
}

static int
wl_radar_sc_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	uint i;
	wl_radar_status_t ra;
	static const struct {
		uint32 radar_type;
		const char *radar_type_name;
	} radar_names[] = {
		{0, "NONE"},
		{1, "ETSI_0"},
		{2, "ETSI_1"},
		{3, "ETSI_2"},
		{4, "ETSI_3"},
		{5, "ETSI_4"},
		{6, "ETSI_5-S2"},
		{7, "ETSI_5-S3"},
		{8, "ETSI_6-S2"},
		{9, "ETSI_6-S3"},
		{10, "FCC-0"},
		{11, "FCC-1"},
		{12, "FCC-2"},
		{13, "FCC-3"},
		{14, "FCC-4"},
		{15, "FCC-5"},
		{16, "FCC-6"},
		{17, "JP1-1"},
		{18, "JP1-2"},
		{19, "JP2-1-1"},
		{20, "JP2-1-2"},
		{21, "JP2-2-1"},
		{22, "JP2-2-2"},
		{23, "JP2-2-3"},
		{24, "JP2-2-4"},
		{25, "JP3"},
		{26, "JP4"},
		{27, "KN1"},
		{28, "KN2"},
		{29, "KN3"},
		{30, "KN4"},
		{31, "UK1"},
		{32, "UK2"},
		{255, "UNCLASSIFIED"},
	};

	char radar_type_str[24];

	/* Skip the command name */
	argv++;

	if (*argv == NULL) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &ra, sizeof(ra))) < 0)
			return ret;

		if (ra.detected == FALSE) {
			printf("NO RADAR_SC DETECTED \n");
		} else {
			  for (i = 0; i < ARRAYSIZE(radar_names); i++) {
				if (radar_names[i].radar_type == ra.radartype)
					snprintf(radar_type_str, sizeof(radar_type_str),
						"%s", radar_names[i].radar_type_name);
			}

			  if (ra.pretended == TRUE) {
			    printf("DFS: NONE ########## RADAR_SC DETECTED  ON SC CHAN \n");
			  } else {
			    if (ra.radartype == 8) {
			      printf("DFS: FCC-5 ########## RADAR_SC DETECTED  ON SC CHAN \n");
			      printf(" ########## lp_csect_single");
					printf("= %d, Time from last detection = %u, ",
						ra.lp_csect_single, ra.timefromL);
			      printf(" = %dmin %dsec AT %dMS \n ",
			      ra.timefromL/60, ra.timefromL%60, ra.timenow);
			    } else {
			      printf("DFS: %s ########## RADAR_SC DETECTED  ON SC CHAN \n",
			      radar_type_str);
			      printf(" ########## detected_pulse_index= %d, nconseq_pulses = %d, ",
			      ra.detected_pulse_index, ra.nconsecq_pulses);
			      printf(" Time from last detection = %u, = %dmin %dsec AT %dMS \n",
			      ra.timefromL, ra.timefromL/60, ra.timefromL%60, ra.timenow);
			      printf("Pruned Intv: ");
			      for (i = 0; i < 10; i++) {
				printf("%d-%d ", ra.intv[i], i);
			      }
			      printf("\n");

			      printf("Pruned PW:  ");
			      for (i = 0; i < 10; i++) {
				printf("%i-%d ", ra.pw[i], i);
			      }
			      printf("\n");

			      printf("Pruned FM:  ");
			      for (i = 0; i < 10; i++) {
				printf("%i-%d ", ra.fm[i], i);
			      }
			      printf("\n");
#ifdef BCMDBG
			      printf("Pruned FC:  ");
			      for (i = 0; i < 10; i++) {
				printf("%i-%d ", ra.fc[i], i);
			      }
			      printf("\n");

			      printf("Pruned Chirp:  ");
			      for (i = 0; i < 10; i++) {
			        printf("%i-%d ", ra.chirp[i], i);
			      }
			      printf("\n");
#endif // endif
			    }
			  }
		}

	} else {
		ret = atoi(*argv);
		if (ret == 0) {
			printf("Clear SC Radar Status \n");
			ra.detected = FALSE;
			return wlu_var_setbuf(wl, "clear_radar_sc_status",
				&ra, sizeof(wl_radar_status_t));
		}
	}

	return ret;
}

static int
wl_clear_radar_status(void *wl, cmd_t *cmd, char **argv)
{
	wl_radar_status_t ra;

	UNUSED_PARAMETER(argv);
	printf("Clear Radar Status \n");

		ra.detected = FALSE;
		return wlu_var_setbuf(wl, cmd->name, &ra, sizeof(wl_radar_status_t));
}

static int
wl_radar_subband_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	int val;

	/* Skip the command name */
	argv++;

	if (*argv == NULL) {
		if ((ret = wlu_iovar_getint(wl, cmd->name, &val)) < 0)
			return ret;
		if (val == 0)
			printf("NO RADAR SUBBAND DETECTED \n");
		else
			printf("%d(0x%x) \n", val, val);
		return 0;
	} else {
		val = atoi(*argv);
		if (val == 0) {
			printf("Clear Radar Subband Status \n");
			return wlu_iovar_setint(wl, cmd->name, val);
		} else {
			printf("Unsupported Argument \n");
			return 0;
		}
	}
}

static int
wl_measure_req(void *wl, cmd_t *cmd, char **argv)
{
	uint32 val;
	struct ether_addr ea;

	if (!*++argv) {
		printf("error: missing arguments\n");
		return BCME_USAGE_ERROR;
	}

	if (!stricmp(*argv, "tpc"))
		val = WLC_MEASURE_TPC;
	else if (!stricmp(*argv, "basic"))
		val = WLC_MEASURE_CHANNEL_BASIC;
	else if (!stricmp(*argv, "cca"))
		val = WLC_MEASURE_CHANNEL_CCA;
	else if (!stricmp(*argv, "rpi"))
		val = WLC_MEASURE_CHANNEL_RPI;
	else {
		printf("error: unknown measurement type %s\n", *argv);
		return BCME_USAGE_ERROR;
	}
	argv++;

	if (!*argv) {
		printf("error: missing target address\n");
		return BCME_USAGE_ERROR;
	}

	if (!wl_ether_atoe(*argv, &ea)) {
		printf("error: could not parse MAC address %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	val = htod32(val);
	memcpy(&buf[0], &val, sizeof(uint32));
	memcpy(&buf[4], ea.octet, ETHER_ADDR_LEN);

	return wlu_set(wl, cmd->set, buf, sizeof(uint32) + ETHER_ADDR_LEN);
}

static int
wl_send_quiet(void *wl, cmd_t *cmd, char **argv)
{
	dot11_quiet_t quiet;

	if (!*++argv) {
		printf("error: missing arguments\n");
		return BCME_USAGE_ERROR;
	}
	/* Order is count, duration, offset */
	quiet.count = atoi(*argv);
	if (!*++argv) {
		printf("error: missing arguments\n");
		return BCME_USAGE_ERROR;
	}
	quiet.duration = atoi(*argv);
	if (!*++argv) {
		printf("error: missing arguments\n");
		return BCME_USAGE_ERROR;
	}
	quiet.offset = atoi(*argv);
	quiet.period = 0;

	quiet.duration = htod16(quiet.duration);
	quiet.offset = htod16(quiet.offset);
	return (wlu_set(wl, cmd->set, &quiet, sizeof(quiet)));
}

static int
wl_pm_mute_tx(void *wl, cmd_t *cmd, char **argv)
{
	wl_pm_mute_tx_t var;

	var.version = htod16(WL_PM_MUTE_TX_VER);
	var.len = htod16(sizeof(wl_pm_mute_tx_t));

	if (!*++argv) {
		goto missing_args;
	}

	var.enable = atoi(*argv);

	if (var.enable) {
		if (!*++argv) {
			goto missing_args;
		}
		var.deadline = htod16(atoi(*argv));
	}

	return (wlu_var_setbuf(wl, cmd->name, &var, var.len));

missing_args:
	return -1;
}

static int
wl_send_csa(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_chan_switch_t csa_arg;

	/* Order is mode, count channel */
	if (!*++argv) {
		printf("error: missing arguments\n");
		return BCME_USAGE_ERROR;
	}
	csa_arg.mode = atoi(*argv) ? 1 : 0;
	if (!*++argv) {
		printf("error: missing count\n");
		return BCME_USAGE_ERROR;
	}
	csa_arg.count = atoi(*argv);
	if (!*++argv) {
		printf("error: missing channel\n");
		return BCME_USAGE_ERROR;
	}
	csa_arg.reg = 0;

	if ((csa_arg.chspec = wf_chspec_aton(*argv))) {
		csa_arg.chspec = wl_chspec_to_driver(csa_arg.chspec);
		if (csa_arg.chspec == INVCHANSPEC) {
			return BCME_USAGE_ERROR;
		}
		/* csa action frame type */
		if (*++argv) {
			if (strcmp(*argv, "u") == 0)
				csa_arg.frame_type = CSA_UNICAST_ACTION_FRAME;
			else {
				printf("error: invalid frame type: %s\n", *argv);
				return BCME_USAGE_ERROR;
			}
		} else
			csa_arg.frame_type = CSA_BROADCAST_ACTION_FRAME;

		err = wlu_var_setbuf(wl, cmd->name, &csa_arg, sizeof(csa_arg));
	} else {
		printf("Error: bad parameters \"%s\"\n", *argv);
		return BCME_BADARG;
	}

	return err;
}

/* Get buffer of minimal size equal to the sizeof iovar name + param_len */
int
wlu_var_getbuf_minimal(void *wl, const char *iovar, void *param, int param_len, void **bufptr)
{
	int len;

	/* include the null */
	len = strlen(iovar) + 1;

	memset(buf, 0, (len + param_len));
	strcpy(buf, iovar);

	if (param_len)
		memcpy(&buf[len], param, param_len);

	*bufptr = buf;

	return wlu_get(wl, WLC_GET_VAR, &buf[0], (len + param_len));
}

int
wlu_var_setbuf_med(void *wl, const char *iovar, void *param, int param_len)
{
	int len;

	memset(buf, 0, WLC_IOCTL_MEDLEN);
	strcpy(buf, iovar);

	/* include the null */
	len = strlen(iovar) + 1;

	if (param_len)
		memcpy(&buf[len], param, param_len);

	len += param_len;

	return wlu_set(wl, WLC_SET_VAR, &buf[0], WLC_IOCTL_MEDLEN);
}

#ifdef NOTYET
static int
wl_wlciovar_mkbuf(const char *iovar, int wlcidx, void *param,
	int paramlen, void *bufptr, int buflen, int *perr)
{
	const char *prefix = "wlc:";
	return wl_prefixiovar_mkbuf(iovar, prefix, wlcidx,  param, paramlen, bufptr, buflen, perr);
}
#endif // endif

static int
wl_chan_info(void *wl, cmd_t *cmd, char **argv)
{
	uint bitmap;
	uint8 channel;
	uint32 chanspec_arg;
	int buflen, err = BCME_OK, first, last, minutes;
	char *param;
	bool all;
	if (!*++argv) {
		first = 0;
		last = MAXCHANNEL;
		all = TRUE;
	} else {
		last = first = atoi(*argv);
		if (last <= 0) {
			printf(" Usage: %s [channel | All ]\n", cmd->name);
			return BCME_USAGE_ERROR;
		}
		all = FALSE;
	}

	for (; first <= last; first++) {
		channel = first;
		chanspec_arg = CH20MHZ_CHSPEC(channel);

		strcpy(buf, "per_chan_info");
		buflen = strlen(buf) + 1;
		param = (char *)(buf + buflen);
		/* there should be no problem if converting to a legacy chanspec
		 * since chanspec_arg is created as 20MHz
		 */
		chanspec_arg = wl_chspec32_to_driver(chanspec_arg);
		memcpy(param, (char*)&chanspec_arg, sizeof(chanspec_arg));

		if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
			return err;

		bitmap = dtoh32(*(uint *)buf);
		minutes = (bitmap >> 24) & 0xff;

		if (!(bitmap & WL_CHAN_VALID_HW)) {
			if (!all)
				printf("Invalid Channel\n");
			continue;
		}

		if (!(bitmap & WL_CHAN_VALID_SW)) {
			if (!all)
				printf("Not supported in current locale\n");
			continue;
		}

		printf("Channel %d\t", channel);

		if (bitmap & WL_CHAN_BAND_5G)
			printf("A Band");
		else
			printf("B Band");

		if (bitmap & WL_CHAN_RADAR) {
			printf(", RADAR Sensitive");
		}
		if (bitmap & WL_CHAN_RESTRICTED) {
			printf(", Restricted");
		}
		if (bitmap & WL_CHAN_PASSIVE) {
			printf(", Passive");
		}
		if (bitmap & WL_CHAN_CLM_RESTRICTED) {
			printf(", CLM Restrict");
		}
		if (bitmap & WL_CHAN_INACTIVE) {
			printf(", Temporarily Out of Service for %d minutes", minutes);
		}
		printf("\n");
	}

	return (0);
}

int
wl_sta_report(void *wl, cmd_t *cmd, char **argv)
{
	sta_report_t *sta_report;
	struct ether_addr ea;
	char *param;
	int buflen, err;

	strcpy(buf, *argv);

	/* convert the ea string into an ea struct */
	if (!*++argv || !wl_ether_atoe(*argv, &ea)) {
		printf(" ERROR: no valid ether addr provided\n");
		return BCME_USAGE_ERROR;
	}

	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&ea, ETHER_ADDR_LEN);

	if ((err = wlu_get(wl, cmd->get, buf, sizeof(sta_report_t))) < 0) {
		return err;
	}

	sta_report = (sta_report_t *)buf;
	sta_report->ver = dtoh16(sta_report->ver);

	/* Report unrecognized version */
	if (sta_report->ver > WL_STA_REPORT_VER) {
		printf(" ERROR: unknown driver station info version %d\n", sta_report->ver);
		return BCME_ERROR;
	}

	sta_report->len = dtoh16(sta_report->len);
	sta_report->tx_rspec = dtoh32(sta_report->tx_rspec);
	sta_report->rx_rspec = dtoh32(sta_report->rx_rspec);
	printf("[VER %d] STA %s:\n", sta_report->ver, *argv);
	printf("\ttx nrate : ");
	wl_nrate_print(sta_report->tx_rspec, ioctl_version);
	printf("\trx nrate : ");
	wl_nrate_print(sta_report->rx_rspec, ioctl_version);

	return BCME_OK;
}

static int
wl_revinfo(void *wl, cmd_t *cmd, char **argv)
{
	char b[8];
	int err;
	wlc_rev_info_t revinfo;
	uint32 rev, rev2, drvrev_major;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(&revinfo, 0, sizeof(revinfo));

	if ((err = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return err;

	printf("vendorid 0x%x\n", dtoh32(revinfo.vendorid));
	printf("deviceid 0x%x\n", dtoh32(revinfo.deviceid));
	printf("radiorev 0x%x\n", dtoh32(revinfo.radiorev));
	printf("chipnum 0x%x\n", dtoh32(revinfo.chipnum));
	printf("chiprev 0x%x\n", dtoh32(revinfo.chiprev));
	printf("chippackage 0x%x\n", dtoh32(revinfo.chippkg));
	printf("corerev %d.%d\n", dtoh32(revinfo.corerev), dtoh32(revinfo.coreminorrev));
	printf("boardid 0x%x\n", dtoh32(revinfo.boardid));
	printf("boardvendor 0x%x\n", dtoh32(revinfo.boardvendor));
	printf("boardrev %s\n", bcm_brev_str(dtoh32(revinfo.boardrev), b));
	if (revinfo.driverrev) {
		rev = dtoh32(revinfo.driverrev);
		drvrev_major = (rev >> 24) & 0xFF;
		printf("driverrev %d.%d.%d.%d\n",
			(rev >> 24) & 0xFF,
			(rev >> 16) & 0xFF,
			(rev >> 8) & 0xFF,
			rev & 0xFF);
	} else {
		drvrev_major = dtoh32(revinfo.drvrev_major);
		printf("driverrev %d.%d.%d.%d\n",
			dtoh32(revinfo.drvrev_major),
			dtoh32(revinfo.drvrev_minor),
			dtoh32(revinfo.drvrev_rc),
			dtoh32(revinfo.drvrev_rc_inc));
	}
	rev = dtoh32(revinfo.ucoderev);
	rev2 = dtoh32(revinfo.ucoderev2);
	if (rev2 == 0) {
		printf("ucoderev %d.%d\n", (rev >> 16) & 0xFFFF, rev & 0xFFFF);
	} else {
		printf("ucoderev %d.%d.r%d\n", (rev >> 16) & 0xFFFF, rev & 0xFFFF, rev2);
	}
	printf("bus 0x%x\n", dtoh32(revinfo.bus));
	printf("phytype 0x%x\n", dtoh32(revinfo.phytype));
	printf("phyrev %d.%d\n", dtoh32(revinfo.phyrev), dtoh32(revinfo.phyminorrev));
	printf("anarev 0x%x\n", dtoh32(revinfo.anarev));
	printf("nvramrev %d\n", dtoh32(revinfo.nvramrev));

	if (drvrev_major >= 17) {
		/* XXX: Print below only when wl firmware is from KUDU 17.10,
		 * because it's possible to issue "wl revinfo" with wl utility
		 * from KUDU 17.10 but wl firmware is from non-KUDU.
		 */
		printf("otpflag 0x%x\n", dtoh32(revinfo.otpflag));
	}

	return 0;
}

static int
wl_rm_request(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	const char* fn_name = "wl_rm_request";
	wl_rm_req_t *rm_ptr;
	wl_rm_req_t rm;
	wl_rm_req_elt_t req;
	int buflen = 0;
	int err, opt_err;
	int type;
	bool in_measure = FALSE;

	UNUSED_PARAMETER(cmd);

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	memset(&rm, 0, WL_RM_REQ_FIXED_LEN);
	memset(&req, 0, sizeof(wl_rm_req_elt_t));

	strcpy(buf, "rm_req");
	buflen = strlen(buf) + 1;

	rm_ptr = (wl_rm_req_t*)(buf + buflen);
	buflen += WL_RM_REQ_FIXED_LEN;

	/* toss the command name */
	argv++;

	miniopt_init(&to, fn_name, "p", FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		argv += to.consumed;

		if (to.opt == 't') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for the token\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}

			if (!in_measure)
				rm.token = to.val;
			else
				req.token = to.val;
		}
		if (to.opt == 'c') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for channel\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}

			req.chanspec = CH20MHZ_CHSPEC(to.val);
		}
		if (to.opt == 'd') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for duration\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			req.dur = to.val;
		}

		if (to.opt == 'p') {
			req.flags = WL_RM_FLAG_PARALLEL;
		}

		if (to.positional) {
			if (!strcmp(to.valstr, "basic")) {
				type = WL_RM_TYPE_BASIC;
			} else if (!strcmp(to.valstr, "cca")) {
				type = WL_RM_TYPE_CCA;
			} else if (!strcmp(to.valstr, "rpi")) {
				type = WL_RM_TYPE_RPI;
			} else {
				fprintf(stderr,
					"%s: could not parse \"%s\" as a measurement type\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			/* complete the previous measurement */
			if (in_measure) {
				req.chanspec = wl_chspec_to_driver(req.chanspec);
				req.token = htod32(req.token);
				req.tsf_h = htod32(req.tsf_h);
				req.tsf_l = htod32(req.tsf_l);
				req.dur = htod32(req.dur);
				memcpy(buf + buflen, &req, sizeof(wl_rm_req_elt_t));
				buflen += sizeof(wl_rm_req_elt_t);
				rm.count++;
				req.chanspec = wl_chspec_from_driver(req.chanspec);
				req.token = dtoh32(req.token);
				req.tsf_h = dtoh32(req.tsf_h);
				req.tsf_l = dtoh32(req.tsf_l);
				req.dur = dtoh32(req.dur);
				/* measure to measure default param update */
				req.token++;	/* each measure gets a new token */
				req.flags = 0;	/* measure flags are cleared between measures */
			}
			in_measure = TRUE;
			req.type = (int8)type;
		}
	}

	/* complete the last measurement */
	if (in_measure) {
		req.chanspec = wl_chspec_to_driver(req.chanspec);
		req.token = htod32(req.token);
		req.tsf_h = htod32(req.tsf_h);
		req.tsf_l = htod32(req.tsf_l);
		req.dur = htod32(req.dur);
		memcpy(buf + buflen, &req, sizeof(wl_rm_req_elt_t));
		buflen += sizeof(wl_rm_req_elt_t);
		rm.count++;
	}

	if (rm.count == 0) {
		fprintf(stderr, "%s: no measurement requests specified\n",
			fn_name);
		err = BCME_USAGE_ERROR;
		goto exit;
	}

	rm.token = htod32(rm.token);
	rm.count = htod32(rm.count);
	memcpy(rm_ptr, &rm, WL_RM_REQ_FIXED_LEN);

	err = wlu_set(wl, WLC_SET_VAR, &buf[0], buflen);

exit:
	return err;
}

static int
wl_rm_report(void *wl, cmd_t *cmd, char **argv)
{
	wl_rm_rep_t *rep_set;
	wl_rm_rep_elt_t rep;
	char extra[128];
	char* p;
	const char* name;
	uint8* data;
	int err, bin;
	uint32 val;
	uint16 channel;
	bool aband;
	int len;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	strcpy(buf, "rm_rep");

	if ((err = wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
		return err;

	rep_set = (wl_rm_rep_t *)buf;
	rep_set->token = dtoh32(rep_set->token);
	rep_set->len = dtoh32(rep_set->len);

	printf("Measurement Report: token %d, length %d\n", rep_set->token, rep_set->len);

	len = rep_set->len;
	data = (uint8*)rep_set->rep;
	for (; len > 0; (len -= rep.len), (data += rep.len)) {
		if (len >= WL_RM_REP_ELT_FIXED_LEN)
			memcpy(&rep, data, WL_RM_REP_ELT_FIXED_LEN);
		else
			break;

		rep.chanspec = wl_chspec_from_driver(rep.chanspec);
		rep.token = dtoh32(rep.token);
		rep.tsf_h = dtoh32(rep.tsf_h);
		rep.tsf_l = dtoh32(rep.tsf_l);
		rep.dur = dtoh32(rep.dur);
		rep.len = dtoh32(rep.len);

		data += WL_RM_REP_ELT_FIXED_LEN;
		len -= WL_RM_REP_ELT_FIXED_LEN;

		if (rep.type == WL_RM_TYPE_BASIC)
			name = "Basic";
		else if (rep.type == WL_RM_TYPE_CCA)
			name = "CCA";
		else if (rep.type == WL_RM_TYPE_RPI)
			name = "RPI";
		else
			name = NULL;

		if (name)
			printf("\nReport   : %s\n", name);
		else
			printf("\nReport   : %d <unknown>\n", rep.type);

		p = extra;
		if (rep.flags & WL_RM_FLAG_PARALLEL) {
			if (p != extra)
				p += sprintf(p, " | ");
			p += sprintf(p, "Parallel");
		}
		if (rep.flags & WL_RM_FLAG_LATE) {
			if (p != extra)
				p += sprintf(p, " | ");
			p += sprintf(p, "Late");
		}
		if (rep.flags & WL_RM_FLAG_INCAPABLE) {
			if (p != extra)
				p += sprintf(p, " | ");
			p += sprintf(p, "Incapable");
		}
		if (rep.flags & WL_RM_FLAG_REFUSED) {
			if (p != extra)
				p += sprintf(p, " | ");
			p += sprintf(p, "Refused");
		}

		if (p != extra) {
			printf("flags    : 0x%02x (%s)\n", rep.flags, extra);
		} else {
			printf("flags    : 0x%02x\n", rep.flags);
		}
		printf("token    : %4d\n", rep.token);

		if (rep.flags & (WL_RM_FLAG_LATE |
			WL_RM_FLAG_INCAPABLE |
			WL_RM_FLAG_REFUSED)) {
			continue;
		}

		channel = CHSPEC_CHANNEL(rep.chanspec);
		aband = CHSPEC_IS5G(rep.chanspec);

		printf("channel  : %4d %s\n", channel,
		       aband ? "(a)":"(b)");
		printf("start tsf: 0x%x:%08x\n", rep.tsf_h, rep.tsf_l);
		printf("duration : %4d TU\n", rep.dur);

		if (len < (int)rep.len) {
			printf("Error: partial report element, %d report bytes "
			       "remain, element claims %d\n",
			       len, rep.len);
			break;
		}

		if (rep.type == WL_RM_TYPE_BASIC) {
			if (rep.len >= 4) {
				memcpy(&val, data, sizeof(uint32));
				val = dtoh32(val);
				printf("Basic bits: 0x%08x\n", val);
			}
		} else if (rep.type == WL_RM_TYPE_CCA) {
			if (rep.len >= 4) {
				memcpy(&val, data, sizeof(uint32));
				val = dtoh32(val);
				printf("Carrier Fraction: %d / 255\n", val);
			}
		} else if (rep.type == WL_RM_TYPE_RPI) {
			if (rep.len >= sizeof(wl_rm_rpi_rep_t)) {
				wl_rm_rpi_rep_t rpi_rep;
				int8 min = -128;
				int8 max;

				memcpy(&rpi_rep, data, sizeof(wl_rm_rpi_rep_t));

				for (bin = 0; bin < 8; bin++) {
					max = rpi_rep.rpi_max[bin];
					if (bin == 0)
						printf("       Power <= %3d: ",
						       max);
					else if (bin < 7)
						printf(" %3d < Power <= %3d: ",
						       min, max);
					else
						printf(" %3d < Power       : ",
						       min);
					min = max;
					printf("%3d\n", rpi_rep.rpi[bin]);
				}
			}
		}
	}

	return err;
}

static int
wl_join_pref(void *wl, cmd_t *cmd, char **argv)
{
	char* data;
	int err;
	int len;
	int remaining_bytes;
	int i;
	bcm_tlv_t *ie;

	UNUSED_PARAMETER(cmd);

	strcpy(buf, "join_pref");

	/* set */
	if (argv[1]) {
		len = strlen(buf);
		data = argv[1];
		for (i = len + 1, len += 1 + strlen(data) / 2;
		    (i < len) && (i < (int)WLC_IOCTL_MAXLEN); i ++) {
			char hex[] = "XX";
			hex[0] = *data++;
			hex[1] = *data++;
			buf[i] = (uint8)strtoul(hex, NULL, 16);
		}
		err = wlu_set(wl, WLC_SET_VAR, buf, i);
	}
	/* get */
	else if (!(err = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MAXLEN))) {
		len = dtoh32(*(int *)buf);
		data = buf + sizeof(int);
		for (i = 0; i < len; i ++)
			printf("%02x", (uint8)(data[i]));
		printf("\n");
		/* pretty print the join pref elements */
		remaining_bytes = len;
		ie = (bcm_tlv_t*)data;
		if (!bcm_valid_tlv(ie, remaining_bytes))
		    ie = NULL;
		while (ie) {
			wl_join_pref_print_ie(ie);
			ie = bcm_next_tlv(ie, &remaining_bytes);
		}
	}
	return err;
}

static void
wl_join_pref_print_ie(bcm_tlv_t *ie)
{
	int i;
	uint8 band;
	uint8 count;
	int suite_len;
	uint8 *suite;
	int data_bytes;

	switch (ie->id) {
	case WL_JOIN_PREF_RSSI:
		printf("Pref RSSI\n");
		if (ie->len > 2)
			printf("\t<%d extra bytes in pref data>\n", ie->len);
		break;
	case WL_JOIN_PREF_BAND:
		printf("Pref BAND: ");
		if (ie->len < 2) {
			printf("len = %d <band pref data truncated>\n", ie->len);
			break;
		}

		band = ie->data[1];
		if (band == WLC_BAND_AUTO)
			printf("0x%x AUTO (no preference)\n", band);
		else if (band == WLC_BAND_5G)
			printf("0x%x 5 GHz\n", band);
		else if (band == WLC_BAND_2G)
			printf("0x%x 2.4 GHz\n", band);
		else if (band == WLJP_BAND_ASSOC_PREF)
			printf("0x%x Use ASSOC_PREFER value\n", band);
		else
			printf("0x%x\n", band);

		if (ie->len > 2)
			printf("\t<%d extra bytes in pref data>\n", ie->len - 1);

		break;
	case WL_JOIN_PREF_WPA:
		printf("Pref WPA: ");
		if (ie->len < 2) {
			printf("len = %d <WPA pref data truncated>\n", ie->len);
			break;
		}
		count = ie->data[1];
		printf("%d ACP Specs\n", count);

		data_bytes = ie->len - 2;
		suite_len = 4; /* WPA Suite Selector length, OUI + type */
		suite = ie->data + 2;

		for (i = 0; i < (int)count; i++) {
			if (data_bytes < 3 * suite_len)
				break;
			printf("\t");
			/* AKM Suite */
			wl_join_pref_print_akm(suite);
			printf(",");

			suite = suite + suite_len;
			/* Unicast Cipher Suite */
			printf("U:");
			wl_join_pref_print_cipher_suite(suite);
			printf(",");

			suite = suite + suite_len;
			/* Multicast Cipher Suite */
			printf("M:");
			if (!memcmp(suite, WL_WPA_ACP_MCS_ANY, suite_len))
				printf("Any");
			else
				wl_join_pref_print_cipher_suite(suite);
			printf("\n");

			suite = suite + suite_len;
			data_bytes -= 3 * suite_len;
		}

		if (i != count)
			printf("\t<expected %d more specs, %d bytes>\n",
			count - i, suite_len * (count - i));
		if (data_bytes > 0)
			printf("\t<%d extra bytes>\n", data_bytes);
		break;
	case WL_JOIN_PREF_RSSI_DELTA:
		printf("RSSI Delta for Pref BAND: ");
		if (ie->len < 2) {
			printf("len = %d <rssi delta pref data truncated>\n", ie->len);
			break;
		}

		band = ie->data[1];
		if (band == WLC_BAND_AUTO)
			printf("0x%x AUTO (no preference)\n", band);
		else if (band == WLC_BAND_5G)
			printf("0x%x 5 GHz\n", band);
		else if (band == WLC_BAND_2G)
			printf("0x%x 2.4 GHz\n", band);
		else
			printf("0x%x\n", band);

		printf("RSSI boost %ddb\n", ie->data[0]);

		break;
	default:
		printf("Pref 0x%x: len = %d\n", ie->id, ie->len);
		for (i = 0; i < ie->len; i++)
			printf("%02x", ie->data[i]);
		printf("\n");
		break;

	}

}

static void
wl_join_pref_print_akm(uint8* suite)
{
	uint8 type = suite[3];
	const char *oui_name;

	if (!memcmp(suite, WPA_OUI, 3))
		oui_name = "WPA";
	else if (!memcmp(suite, WPA2_OUI, 3))
		oui_name = "WPA2";
	else
		oui_name = NULL;

	if (oui_name) {
		if (type == RSN_AKM_NONE)
			printf("%s-NONE", oui_name);
		else if (type == RSN_AKM_UNSPECIFIED)
			printf("%s", oui_name);
		else if (type == RSN_AKM_UNSPECIFIED)
			printf("%s-PSK", oui_name);
		else
			printf("%s/0x%x", oui_name, type);
	} else {
		printf("0x%02x%02x%02x/0x%02x", suite[0], suite[1], suite[2], suite[3]);
	}
}

static void
wl_join_pref_print_cipher_suite(uint8* suite)
{
	uint8 type = suite[3];
	const char *oui_name;

	if (!memcmp(suite, WPA_OUI, 3))
		oui_name = "WPA";
	else if (!memcmp(suite, WPA2_OUI, 3))
		oui_name = "WPA2";
	else
		oui_name = NULL;

	if (oui_name) {
		if (type == WPA_CIPHER_NONE)
			printf("%s/NONE", oui_name);
		else if (type == WPA_CIPHER_WEP_40)
			printf("%s/WEP40", oui_name);
		else if (type == WPA_CIPHER_TKIP)
			printf("%s/TKIP", oui_name);
		else if (type == WPA_CIPHER_AES_CCM)
			printf("%s/AES", oui_name);
		else if (type == WPA_CIPHER_WEP_104)
			printf("%s/WEP104", oui_name);
		else
			printf("%s/0x%x", oui_name, type);
	} else {
		printf("0x%02x%02x%02x/0x%02x", suite[0], suite[1], suite[2], suite[3]);
	}
}

static int
wl_assoc_pref(void *wl, cmd_t *cmd, char **argv)
{
	uint assoc_pref;
	int err;

	/* set */
	if (argv[1]) {
		if (!strcmp(argv[1], "auto") || !strcmp(argv[1], "0"))
			assoc_pref = WLC_BAND_AUTO;
		else if (!strcmp(argv[1], "a") || !strcmp(argv[1], "1"))
			assoc_pref = WLC_BAND_5G;
		else if (!strcmp(argv[1], "b") || !strcmp(argv[1], "g") || !strcmp(argv[1], "2"))
			assoc_pref = WLC_BAND_2G;
		else
			return BCME_USAGE_ERROR;
		assoc_pref = htod32(assoc_pref);
		err = wlu_set(wl, cmd->set, &assoc_pref, sizeof(assoc_pref));
	}
	/* get */
	else if (!(err = wlu_get(wl, cmd->get, &assoc_pref, sizeof(assoc_pref)))) {
		assoc_pref = dtoh32(assoc_pref);
		switch (assoc_pref) {
		case WLC_BAND_AUTO:
			printf("auto\n");
			break;
		case WLC_BAND_5G:
			printf("a\n");
			break;
		case WLC_BAND_2G:
			printf("b/g\n");
			break;
		}
	}
	return err;
}

static const char ac_names[AC_COUNT][6] = {"AC_BE", "AC_BK", "AC_VI", "AC_VO"};

/*
 * Get or set WME per-AC transmit parameters
 */
static int
wme_tx_params(void *wl, cmd_t *cmd, char **argv)
{
	char *val_p, *ac_str, *param;
	int buflen;
	int aci;
	wme_tx_params_t cur_params[AC_COUNT], new_params[AC_COUNT];
	int err;
	int val;

	UNUSED_PARAMETER(cmd);

	argv++;

	buflen = WLC_IOCTL_MAXLEN;

	/*
	 * Get current acparams, using buf as an input buffer.
	 * Return data is array of 4 ACs of wme params.
	 */

	strcpy(buf, "wme_tx_params");
	if ((err = wlu_get(wl, WLC_GET_VAR, &buf[0], buflen)) < 0) {
		return err;
	}
	memcpy(&cur_params, buf, WL_WME_TX_PARAMS_IO_BYTES);

	if ((ac_str = *argv++) == NULL) {
		printf("WME TX params: \n");
		for (aci = 0; aci < AC_COUNT; aci++) {
			printf("%s: short %d. sfb %d. long %d. lfb %d. max %d\n", ac_names[aci],
				cur_params[aci].short_retry,
				cur_params[aci].short_fallback,
				cur_params[aci].long_retry,
				cur_params[aci].long_fallback,
				cur_params[aci].max_rate);
		}
	} else {
		int chk_lim;
		if (strcmp(ac_str, "be") == 0) {
			aci = AC_BE;
		} else if (strcmp(ac_str, "bk") == 0) {
			aci = AC_BK;
		} else if (strcmp(ac_str, "vi") == 0) {
			aci = AC_VI;
		} else if (strcmp(ac_str, "vo") == 0) {
			aci = AC_VO;
		} else {
			printf("Unknown access class: %s\n", ac_str);
			return BCME_USAGE_ERROR;
		}

		/* Preload new values with current values */
		memcpy(&new_params, &cur_params, sizeof(new_params));
		while ((param = *argv++) != NULL) {
			if ((val_p = *argv++) == NULL) {
				printf("Need value following %s\n", param);
				return BCME_USAGE_ERROR;
			}
			chk_lim = 15;
			val = (int)strtoul(val_p, NULL, 0);
			/* All values must fit in uint8 */
			if (!strcmp(param, "short")) {
				new_params[aci].short_retry = (uint8)val;
			} else if (!strcmp(param, "sfb")) {
				new_params[aci].short_fallback = (uint8)val;
			} else if (!strcmp(param, "long")) {
				new_params[aci].long_retry = (uint8)val;
			} else if (!strcmp(param, "lfb")) {
				new_params[aci].long_fallback = (uint8)val;
			} else if ((!strcmp(param, "max_rate")) || (!strcmp(param, "max")) ||
				(!strcmp(param, "rate"))) {
				chk_lim = 255;
				new_params[aci].max_rate = (uint8)val;
			} else {
				printf("Unknown parameter: %s\n", param);
				return BCME_USAGE_ERROR;
			}
			if (val > chk_lim) {
				printf("Value for %s must be < %d\n", param, chk_lim + 1);
				return BCME_USAGE_ERROR;
			}
		}
		strcpy(buf, "wme_tx_params");
		memcpy(buf + strlen(buf) + 1, new_params, WL_WME_TX_PARAMS_IO_BYTES);
		err = wlu_set(wl, WLC_SET_VAR, &buf[0], buflen);
	}

	return err;
}

/*
 * Get or Set WME Access Class (AC) parameters
 *	wl wme_ac ap|sta [be|bk|vi|vo [ecwmax|ecwmin|txop|aifsn|acm <value>] ...]
 * Without args past ap|sta, print current values
 */
static int
wl_wme_ac_req(void *wl, cmd_t *cmd, char **argv)
{
	const char *iovar_name;
	int err;
	edcf_acparam_t acparam_cur[AC_COUNT], acparam_new[AC_COUNT], *acp;
	char *ac_str, *param, *val;
	bool acm;
	int aci, aifsn, ecwmin, ecwmax, txop;

	UNUSED_PARAMETER(cmd);

	argv++;

	if ((param = *argv++) == NULL)
		return BCME_USAGE_ERROR;

	if (!strcmp(param, "ap"))
		iovar_name = "wme_ac_ap";
	else if (!strcmp(param, "sta"))
		iovar_name = "wme_ac_sta";
	else
		return BCME_USAGE_ERROR;

	/*
	 * Get current acparams into an array of 4 ACs of wme params.
	 */
	err = wlu_iovar_get(wl, iovar_name, &acparam_cur, sizeof(acparam_cur));
	if (err < 0)
		return err;

	if ((ac_str = *argv++) == NULL) {
		printf("AC Parameters\n");

		for (aci = 0; aci < AC_COUNT; aci++) {
			acp = &acparam_cur[aci];
			acp->TXOP = dtoh16(acp->TXOP);
			if (((acp->ACI & EDCF_ACI_MASK) >> EDCF_ACI_SHIFT) != aci)
				printf("Warning: AC params out of order\n");
			acm = (acp->ACI & EDCF_ACM_MASK) ? 1 : 0;
			aifsn = acp->ACI & EDCF_AIFSN_MASK;
			ecwmin = acp->ECW & EDCF_ECWMIN_MASK;
			ecwmax = (acp->ECW & EDCF_ECWMAX_MASK) >> EDCF_ECWMAX_SHIFT;
			txop = acp->TXOP;
			printf("%s: raw: ACI 0x%x ECW 0x%x TXOP 0x%x\n",
			       ac_names[aci],
			       acp->ACI, acp->ECW, acp->TXOP);
			printf("       dec: aci %d acm %d aifsn %d "
			       "ecwmin %d ecwmax %d txop 0x%x\n",
			       aci, acm, aifsn, ecwmin, ecwmax, txop);
			/* CWmin = 2^(ECWmin) - 1 */
			/* CWmax = 2^(ECWmax) - 1 */
			/* TXOP = number of 32 us units */
			printf("       eff: CWmin %d CWmax %d TXop %dusec\n",
			       EDCF_ECW2CW(ecwmin), EDCF_ECW2CW(ecwmax), EDCF_TXOP2USEC(txop));
		}

		err = 0;
	} else {
		if (strcmp(ac_str, "be") == 0)
			aci = AC_BE;
		else if (strcmp(ac_str, "bk") == 0)
			aci = AC_BK;
		else if (strcmp(ac_str, "vi") == 0)
			aci = AC_VI;
		else if (strcmp(ac_str, "vo") == 0)
			aci = AC_VO;
		else
			return BCME_USAGE_ERROR;

		/* Preload new values with current values */
		memcpy(&acparam_new, &acparam_cur, sizeof(acparam_new));

		acp = &acparam_new[aci];

		while ((param = *argv++) != NULL) {
			if ((val = *argv++) == NULL)
				return BCME_USAGE_ERROR;

			if (!strcmp(param, "acm")) {
				if (!stricmp(val, "on") || !stricmp(val, "1"))
					acp->ACI |= EDCF_ACM_MASK;
				else if (!stricmp(val, "off") || !stricmp(val, "0"))
					acp->ACI &= ~EDCF_ACM_MASK;
				else {
					fprintf(stderr, "acm value must be 1|0\n");
					return BCME_USAGE_ERROR;
				}
			} else if (!strcmp(param, "aifsn")) {
				aifsn = (int)strtol(val, NULL, 0);
				if (aifsn >= EDCF_AIFSN_MIN && aifsn <= EDCF_AIFSN_MAX)
					acp->ACI =
					        (acp->ACI & ~EDCF_AIFSN_MASK) |
					        (aifsn & EDCF_AIFSN_MASK);
				else {
					fprintf(stderr, "aifsn %d out of range (%d-%d)\n",
					        aifsn, EDCF_AIFSN_MIN, EDCF_AIFSN_MAX);
					return BCME_USAGE_ERROR;
				}
			} else if (!strcmp(param, "ecwmax")) {
				ecwmax = (int)strtol(val, NULL, 0);
				if (ecwmax >= EDCF_ECW_MIN && ecwmax <= EDCF_ECW_MAX)
					acp->ECW =
					        ((ecwmax << EDCF_ECWMAX_SHIFT) & EDCF_ECWMAX_MASK) |
					        (acp->ECW & EDCF_ECWMIN_MASK);
				else {
					fprintf(stderr, "ecwmax %d out of range (%d-%d)\n",
					        ecwmax, EDCF_ECW_MIN, EDCF_ECW_MAX);
					return BCME_USAGE_ERROR;
				}
			} else if (!strcmp(param, "ecwmin")) {
				ecwmin = (int)strtol(val, NULL, 0);
				if (ecwmin >= EDCF_ECW_MIN && ecwmin <= EDCF_ECW_MAX)
					acp->ECW =
					        ((acp->ECW & EDCF_ECWMAX_MASK) |
					         (ecwmin & EDCF_ECWMIN_MASK));
				else {
					fprintf(stderr, "ecwmin %d out of range (%d-%d)\n",
					        ecwmin, EDCF_ECW_MIN, EDCF_ECW_MAX);
					return BCME_USAGE_ERROR;
				}
			} else if (!strcmp(param, "txop")) {
				txop = (int)strtol(val, NULL, 0);
				if (txop >= EDCF_TXOP_MIN && txop <= EDCF_TXOP_MAX)
					acp->TXOP = htod16(txop);
				else {
					fprintf(stderr, "txop %d out of range (%d-%d)\n",
					        txop, EDCF_TXOP_MIN, EDCF_TXOP_MAX);
					return BCME_USAGE_ERROR;
				}
			} else {
				fprintf(stderr, "unexpected param %s\n", param);
				return BCME_USAGE_ERROR;
			}
		}

		/*
		 * Now set the new acparams
		 * NOTE: only one of the four ACs can be set at a time.
		 */
		err = wlu_iovar_set(wl, iovar_name, acp, sizeof(edcf_acparam_t));
	}

	return err;
}

/*
 * Get or Set WME APSD control parameters
 *	wl wme_apsd_sta <max_sp_len> <be> <bk> <vi> <vo>
 *	  <max_sp_len> is 0 (all), 2, 4, or 6
 *        <be>, <bk>, <vi>, <vo> are each 0 or 1 for APSD enable
 *  with no args, print current values
 */
static int
wl_wme_apsd_sta(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int buflen;
	char *param;
	int qosinfo;
	int msp, max_sp_len, be, bk, vi, vo;

	/* Display current params if no args, else set params */

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "wme_qosinfo");
	buflen = WLC_IOCTL_MAXLEN;

	param = *++argv;

	if (param == NULL) {
		if ((err = wlu_get(wl, cmd->get, &buf[0], buflen)) < 0)
			return err;

		memcpy(&qosinfo, buf, sizeof(qosinfo));
		qosinfo = dtoh32(qosinfo);

		msp = (qosinfo & WME_QI_STA_MAXSPLEN_MASK) >> WME_QI_STA_MAXSPLEN_SHIFT;
		be = (qosinfo & WME_QI_STA_APSD_BE_MASK) >> WME_QI_STA_APSD_BE_SHIFT;
		bk = (qosinfo & WME_QI_STA_APSD_BK_MASK) >> WME_QI_STA_APSD_BK_SHIFT;
		vi = (qosinfo & WME_QI_STA_APSD_VI_MASK) >> WME_QI_STA_APSD_VI_SHIFT;
		vo = (qosinfo & WME_QI_STA_APSD_VO_MASK) >> WME_QI_STA_APSD_VO_SHIFT;

		max_sp_len = msp * 2;

		printf("Max SP Length = %d, APSD: BE=%d BK=%d VI=%d VO=%d\n",
		       max_sp_len, be, bk, vi, vo);
	} else {
		max_sp_len = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return BCME_USAGE_ERROR;
		be = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return BCME_USAGE_ERROR;
		bk = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return BCME_USAGE_ERROR;
		vi = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return BCME_USAGE_ERROR;
		vo = (int)strtol(param, 0, 0);

		if (((be | bk | vi | vo) & ~1) | (max_sp_len & ~6)) {
			printf("%s: Invalid parameter\n", cmd->name);
			return BCME_BADARG;
		}

		msp = max_sp_len / 2;

		qosinfo = (msp << WME_QI_STA_MAXSPLEN_SHIFT) & WME_QI_STA_MAXSPLEN_MASK;
		qosinfo |= (be << WME_QI_STA_APSD_BE_SHIFT) & WME_QI_STA_APSD_BE_MASK;
		qosinfo |= (bk << WME_QI_STA_APSD_BK_SHIFT) & WME_QI_STA_APSD_BK_MASK;
		qosinfo |= (vi << WME_QI_STA_APSD_VI_SHIFT) & WME_QI_STA_APSD_VI_MASK;
		qosinfo |= (vo << WME_QI_STA_APSD_VO_SHIFT) & WME_QI_STA_APSD_VO_MASK;

		qosinfo = htod32(qosinfo);
		memcpy(&buf[strlen(buf) + 1], &qosinfo, sizeof(qosinfo));

		err = wlu_set(wl, cmd->set, &buf[0], buflen);
	}

	return err;
}

/*
 * Get or Set WME discard policy
 *	wl wme_dp <be> <bk> <vi> <vo>
 *        <be>, <bk>, <vi>, <vo> are each 0/1 for discard newest/oldest first
 *  with no args, print current values
 */
static int
wl_wme_dp(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int buflen;
	char *param;
	int dp;
	int be, bk, vi, vo;

	/* Display current params if no args, else set params */

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, "wme_dp");
	buflen = WLC_IOCTL_MAXLEN;

	param = *++argv;

	if (param == NULL) {
		if ((err = wlu_get(wl, cmd->get, &buf[0], buflen)) < 0)
			return err;

		memcpy(&dp, buf, sizeof(dp));
		dp = dtoh32(dp);

		be = (dp >> AC_BE) & 1;
		bk = (dp >> AC_BK) & 1;
		vi = (dp >> AC_VI) & 1;
		vo = (dp >> AC_VO) & 1;

		printf("Discard oldest first: BE=%d BK=%d VI=%d VO=%d\n", be, bk, vi, vo);
	} else {
		be = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return BCME_USAGE_ERROR;
		bk = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return BCME_USAGE_ERROR;
		vi = (int)strtol(param, 0, 0);
		if ((param = *++argv) == NULL)
			return BCME_USAGE_ERROR;
		vo = (int)strtol(param, 0, 0);

		if ((be | bk | vi | vo) & ~1) {
			printf("%s: Invalid parameter\n", cmd->name);
			return BCME_BADARG;
		}

		dp = (be << AC_BE) | (bk << AC_BK) | (vi << AC_VI) | (vo << AC_VO);

		dp = htod32(dp);
		memcpy(&buf[strlen(buf) + 1], &dp, sizeof(dp));

		err = wlu_set(wl, cmd->set, &buf[0], buflen);
	}

	return err;
}

/*
 * Get or Set WME lifetime parameter
 *	"wl lifetime be|bk|vi|vo [<value>]"},
 *  with no args, print current values
 */
static int
wl_lifetime(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint8 ac;
	char *param, *val;
	const char *cmdname = "lifetime";
	wl_lifetime_t lifetime, *reply;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return BCME_USAGE_ERROR;

	if (strcmp(param, "be") == 0)
		ac = AC_BE;
	else if (strcmp(param, "bk") == 0)
		ac = AC_BK;
	else if (strcmp(param, "vi") == 0)
		ac = AC_VI;
	else if (strcmp(param, "vo") == 0)
		ac = AC_VO;
	else {
		fprintf(stderr, "unexpected param %s\n", param);
		return BCME_USAGE_ERROR;
	}

	if ((val = *++argv) == NULL) {
		lifetime.ac = htod32(ac);
		if ((err = wlu_var_getbuf(wl, cmdname, &lifetime, sizeof(lifetime),
		                         &ptr)) < 0)
			return err;
		reply = (wl_lifetime_t *) ptr;
		reply->ac = dtoh32(reply->ac);
		reply->lifetime = dtoh32(reply->lifetime);
		printf("Lifetime for access class '%s' is %dms\n", param, reply->lifetime);
	}
	else {
		lifetime.ac = htod32(ac);
		lifetime.lifetime = htod32((uint)strtol(val, 0, 0));
		err = wlu_var_setbuf(wl, cmdname, &lifetime, sizeof(lifetime));
	}

	return err;
}

#define VNDR_IE_OK_FLAGS \
	(VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG | VNDR_IE_ASSOCRSP_FLAG | \
	 VNDR_IE_AUTHRSP_FLAG | VNDR_IE_PRBREQ_FLAG | VNDR_IE_ASSOCREQ_FLAG | \
	 VNDR_IE_AUTHREQ_FLAG |VNDR_IE_DISASSOC_FLAG)

static int
wl_add_ie(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);

	return (wl_vndr_ie(wl, "add", VNDR_IE_OK_FLAGS, argv));
}

static int
wl_del_ie(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);

	return (wl_vndr_ie(wl, "del", VNDR_IE_OK_FLAGS, argv));
}

static int
wl_vndr_ie(void *wl, const char *command, uint32 pktflag_ok, char **argv)
{
	vndr_ie_setbuf_t *ie_setbuf;
	int buflen;
	int err = 0;
	int ret;
	int bsscfg_idx = 0;
	int consumed = 0;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv + 1, argv[0], &bsscfg_idx, &consumed)) != 0)
		return ret;
	if (consumed)
		argv = argv + consumed;
	else
		bsscfg_idx = -1;

	if ((err = wl_mk_ie_setbuf(command, pktflag_ok, argv, &ie_setbuf, &buflen)) != 0)
		return err;

	if (bsscfg_idx == -1)
		err = wlu_var_setbuf(wl, "vndr_ie", ie_setbuf, buflen);
	else
		err = wlu_bssiovar_setbuf(wl, "vndr_ie", bsscfg_idx,
			ie_setbuf, buflen, buf, WLC_IOCTL_MAXLEN);

	free(ie_setbuf);

	return (err);
}

static int
_wl_list_ie(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	const char *old = cmd->name;

	cmd->name = "vndr_ie";
	err = wl_list_ie(wl, cmd, argv);
	cmd->name = old;

	return err;
}

static int
wl_rand(void *wl, cmd_t *cmd, char **argv)
{
	char *randbuf;
	uint16 randnum;
	int err;
	void *ptr;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	randbuf = (char *)ptr;
	memcpy(&randnum, randbuf, sizeof(uint16));
	printf("%d\n", randnum);

	return (0);
}

static int
wl_wme_counters(void *wl, cmd_t *cmd, char **argv)
{
	char *statsbuf;
	wl_wme_cnt_t cnt;
	int err;
	void *ptr;
	char *pbuf = buf;
	uint ac;
	int ap_mode = 0;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf_sm (wl, cmd->name, NULL, 0, &ptr)))
		return (err);

	statsbuf = (char *)ptr;
	memcpy(&cnt, statsbuf, sizeof(cnt));
	cnt.version = dtoh16(cnt.version);
	cnt.length = dtoh16(cnt.length);

	if (cnt.version != WL_WME_CNT_VERSION) {
		printf("\tIncorrect version of counters struct: expected %d; got %d\n",
			WL_WME_CNT_VERSION, cnt.version);
		return -1;
	}

	if ((err = wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))) < 0) {
		return err;
	}
	ap_mode = dtoh32(ap_mode);

	/* summary stat counter line */
	for (ac = AC_BE; ac < AC_COUNT; ac++) {
		pbuf += sprintf(pbuf, "\n%s: tx frames: %u bytes: %u failed frames: %u "
		                "failed bytes: %u\n",
		                ac_names[ac], dtoh32(cnt.tx[ac].packets), dtoh32(cnt.tx[ac].bytes),
		                dtoh32(cnt.tx_failed[ac].packets), dtoh32(cnt.tx_failed[ac].bytes));
		pbuf += sprintf(pbuf, "       rx frames: %u bytes: %u failed frames: %u "
		                "failed bytes: %u\n", dtoh32(cnt.rx[ac].packets),
		                dtoh32(cnt.rx[ac].bytes), dtoh32(cnt.rx_failed[ac].packets),
		                dtoh32(cnt.rx_failed[ac].bytes));

		if (ap_mode) {
			pbuf += sprintf(pbuf, "       foward frames: %u bytes: %u \n",
			                dtoh32(cnt.forward[ac].packets),
			                dtoh32(cnt.forward[ac].bytes));
		}

		pbuf += sprintf(pbuf, "       tx frames time expired: %u \n",
		                dtoh32(cnt.tx_expired[ac].packets));
	}
	pbuf += sprintf(pbuf, "\n");
	fputs(buf, stdout);
	return (0);
}

int
hexstrtobitvec(const char *cp, uchar *bitvec, int veclen)
{
	uchar value = 0;
	int nibble;		/* index of current hex-format nibble to process */
	int even;		/* 1 if even number of nibbles, 0 if odd number */
	int i = 0;

	if (cp[0] == '0' && cp[1] == 'x')
		cp += 2;

	memset(bitvec, '\0', veclen);
	nibble = strlen(cp);
	if (!nibble)
		return -1;
	even = ((nibble % 2) == 0);

	/* convert from right to left (lsb is rightmost byte) */
	--nibble;
	while (nibble >= 0 && i < veclen && (isxdigit((int)cp[nibble]) &&
		(value = isdigit((int)cp[nibble]) ? cp[nibble]-'0' :
		(islower((int)cp[nibble]) ? toupper((int)cp[nibble]) : cp[nibble])-'A'+10) < 16)) {
		if (even == ((nibble+1) % 2)) {
			bitvec[i] += value*16;
			++i;
		} else
			bitvec[i] = value;
		--nibble;
	}

	return ((nibble == -1 && i <= veclen) ? 0 : -1);
}

static int
wl_bitvecext(void *wl, cmd_t *cmd, char **argv)
{
	int err, bcmerr;
	eventmsgs_ext_t *eventmask_msg;
	uint8 masksize;
	err = 0;

	bcmerr = 1;

	/* set */
	if (argv[1]) {
		uint8 send_iovar_datasize;
		/* send user mask size up to WL_EVENTING_MASK_MAX_LEN */
		masksize = MIN((strlen(argv[1])/2), WL_EVENTING_MASK_MAX_LEN);
		send_iovar_datasize = masksize + EVENTMSGS_EXT_STRUCT_SIZE;
		eventmask_msg = (eventmsgs_ext_t*)malloc(send_iovar_datasize);
		if (eventmask_msg == NULL) {
			fprintf(stderr, "fail to allocate event_msgs"
				"structure of %d bytes\n", send_iovar_datasize);
			return BCME_NOMEM;
		}
		memset((void*)eventmask_msg, 0, send_iovar_datasize);
		eventmask_msg->len = masksize;
		eventmask_msg->command = EVENTMSGS_SET_MASK;
		eventmask_msg->ver = EVENTMSGS_VER;
		if (!(err = hexstrtobitvec(argv[1], eventmask_msg->mask, eventmask_msg->len))) {
			err = wlu_var_setbuf(wl, cmd->name, (void*)eventmask_msg,
				send_iovar_datasize);
			if (err) {
				int getint_error = 0;
				getint_error = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
				if ((!getint_error) && (bcmerr == BCME_UNSUPPORTED)) {
					uchar bitvec[WL_EVENTING_MASK_LEN];
					printf("old firmware support only 128 events"
						"setting only the first 128 events\n");
					memset(bitvec, '\0', WL_EVENTING_MASK_LEN);
					if (!(err = hexstrtobitvec(argv[1], bitvec,
						WL_EVENTING_MASK_LEN)))
							err = wlu_var_setbuf(wl, "event_msgs",
								bitvec, WL_EVENTING_MASK_LEN);
				}
			}
		}
		else {
			fprintf(stderr, "Invalid mask %d\n", eventmask_msg->len);
		}
	}
	/* get */
	else {
		void *ptr;
		char *mask;
		int i;
		bool skipzeros;
		eventmsgs_ext_t *eventmask_msg_in;

		skipzeros = TRUE;
		/* input structure have no mask */
		eventmask_msg = (eventmsgs_ext_t*)malloc(EVENTMSGS_EXT_STRUCT_SIZE);
		if (eventmask_msg == NULL) {
		/*
		* XXX: commented out: build failure on 64 bits Linux platform
		*	fprintf(stderr, "fail to allocate event_msgs"
		*		"structure of %d bytes\n", EVENTMSGS_EXT_STRUCT_SIZE);
		*/
			return BCME_NOMEM;
		}
		memset((void*)eventmask_msg, 0,	EVENTMSGS_EXT_STRUCT_SIZE);
		/* command only used for set */
		eventmask_msg->command = EVENTMSGS_NONE;
		/* max read mask size is WL_EVENTING_MASK_MAX_LEN */
		eventmask_msg->len = WL_EVENTING_MASK_MAX_LEN;
		eventmask_msg->ver = EVENTMSGS_VER;
		memset(buf, '\0', WL_EVENTINT_MAX_GET_SIZE);
		if (!(err = wlu_var_getbuf_sm(wl, cmd->name, (void*)eventmask_msg,
			EVENTMSGS_EXT_STRUCT_SIZE, &ptr))) {
				eventmask_msg_in = (eventmsgs_ext_t*)ptr;
				mask = (char *)eventmask_msg_in->mask;
				masksize = eventmask_msg_in->len;
		}
		if (err) {
			int getint_error = 0;
			getint_error = wlu_iovar_getint(wl, "bcmerror", &bcmerr);
			if ((!getint_error) && (bcmerr == BCME_UNSUPPORTED)) {
				printf("old firmware support only 128 events"
					"getting only the first 128 events\n");
				if (!(err = wlu_var_getbuf_sm(wl, "event_msgs", NULL, 0, &ptr))) {
					mask = (char *)ptr;
					masksize = WL_EVENTING_MASK_LEN;
				}
			}
		}
		if (!err) {
			printf("0x");
			for (i = masksize - 1; i >= 0; i--) {
				if (mask[i] || (i == 0))
					skipzeros = FALSE;
				if (skipzeros)
					continue;
				printf("%02x", mask[i] & 0xff);
			}
			printf("\n");
		}
	}
	free(eventmask_msg);
	return (err);
}

static int
wl_eventbitvec(void *wl, cmd_t *cmd, char **argv)
{
	char *vbuf;
	int err;
	uchar bitvec[WL_EVENTING_MASK_LEN];
	bool skipzeros;
	int i;

	err = 0;
	skipzeros = TRUE;

	/* set */
	if (argv[1]) {
		memset(bitvec, '\0', sizeof(bitvec));
		if (!(err = hexstrtobitvec(argv[1], bitvec, sizeof(bitvec))))
			err = wlu_var_setbuf(wl, cmd->name, bitvec, sizeof(bitvec));
		else
			return BCME_BADARG;
	}
	/* get */
	else {
		void *ptr;

		memset(buf, '\0', WLC_IOCTL_MAXLEN);
		if (!(err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr))) {
			vbuf = (char *)ptr;
			printf("0x");
			for (i = (sizeof(bitvec) - 1); i >= 0; i--) {
				if (vbuf[i] || (i == 0))
					skipzeros = FALSE;
				if (skipzeros)
					continue;
				printf("%02x", vbuf[i] & 0xff);
			}
			printf("\n");
		}
	}

	return (err);
}

static int
wl_auto_channel_sel(void *wl, cmd_t *cmd, char **argv)
{
	/*
	* The following condition(s) must be met when Auto Channel Selection
	* is enabled.
	*  - the I/F is up (change radio channel requires it is up?)
	*  - the AP must not be associated (setting SSID to empty should
	*    make sure it for us)
	*/
	int chosen = 0;
	wl_uint32_list_t request;
	int ret = 0;

	if (!*++argv) {
		ret = wlu_get(wl, cmd->get, &chosen, sizeof(chosen));
		chosen = wl_chspec32_from_driver(chosen);
		if (ret >= 0 && chosen != 0) {
			wf_chspec_ntoa((chanspec_t)chosen, buf);
			printf("%s (0x%x)\n", buf, chosen);
			return 0;
		}
		else {
			if (chosen == 0)
				printf("invalid chanspec (0x%x)\n", chosen);
		}
	} else {
		if (atoi(*argv) == 1) {
			request.count = htod32(0);
			ret = wlu_set(wl, cmd->set, &request, sizeof(request));
		} else if (atoi(*argv) == 2) {
			ret = wlu_get(wl, cmd->get, &chosen, sizeof(chosen));
			if (ret >= 0 && chosen != 0)
				ret = wlu_iovar_setint(wl, "chanspec", (int)chosen);
		} else {
			ret = BCME_BADARG;
		}
	}
	return ret;
}

static int
wl_varstr(void *wl, cmd_t *cmd, char **argv)
{
	int error;
	char *str;

	if (!*++argv) {
		void *ptr;

		if ((error = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return (error);

		str = (char *)ptr;
		printf("%s\n", str);
		return (0);
	} else {
		str = *argv;
		/* str length include NULL */
		return wlu_var_setbuf(wl, cmd->name, str, (strlen(str)+1));
	}
}

/* Return TRUE if it's one of the wc cmds. If WC_TOOL is not defined,
 * it'll return TRUE by default so all the commands are allowed.
 */
bool wc_cmd_check(const char *cmd_name)
{
	uint j;
	if (wc_cmds == NULL)
		return TRUE;

	for (j = 0; j < ARRAYSIZE(wc_cmds); j++)
		if (strcmp(wc_cmds[j], cmd_name) == 0)
			return TRUE;
	return FALSE;
}

/* get/set max bandwidth for each access category in ap */
static int
wme_maxbw_params(void *wl, cmd_t *cmd, char **argv)
{
	wme_max_bandwidth_t cur_params, new_params;
	char *val_p, *ac_str, *param;
	int buflen;
	int aci;
	int err;
	int val;
	int ap_mode = 0;

	argv++;

	if ((err = wlu_get(wl, WLC_GET_AP, &ap_mode, sizeof(ap_mode))) < 0)
		return err;

	if (!ap_mode) {
		printf("%s: AP only\n", cmd->name);
		return -1;
	}

	buflen = WLC_IOCTL_MAXLEN;

	/* get the current max bandwidth, using buf as an input buffer. */
	strcpy(buf, "wme_maxbw_params");
	if ((err = wlu_get(wl, WLC_GET_VAR, &buf[0], buflen)) < 0) {
		return err;
	}

	/* cache the current values */
	memcpy(&cur_params, buf, sizeof(wme_max_bandwidth_t));

	if ((ac_str = *argv) == NULL) {
		printf("WME bandwidth limit: \n");
		for (aci = 0; aci < AC_COUNT; aci++) {
			printf("%s: bandwidth limit %d\n", ac_names[aci],
				cur_params.ac[aci]);
		}
	} else {
		/* preload new values with current values */
		memcpy(&new_params, &cur_params, sizeof(new_params));
		while ((param = *argv++) != NULL) {
			if ((val_p = *argv++) == NULL) {
				printf("Need value following %s\n", param);
				return BCME_USAGE_ERROR;
			}

			val = (int)strtoul(val_p, NULL, 0);

			if (!strcmp(param, "be")) {
				new_params.ac[AC_BE] = (uint32)val;
			} else if (!strcmp(param, "bk")) {
				new_params.ac[AC_BK] = (uint32)val;
			} else if (!strcmp(param, "vi")) {
				new_params.ac[AC_VI] = (uint32)val;
			} else if (!strcmp(param, "vo")) {
				new_params.ac[AC_VO] = (uint32)val;
			} else {
				printf("Unknown access category: %s\n", param);
				return BCME_USAGE_ERROR;
			}
		}

		strcpy(buf, "wme_maxbw_params");
		memcpy(buf + strlen(buf) + 1, &new_params, sizeof(wme_max_bandwidth_t));
		err = wlu_set(wl, WLC_SET_VAR, &buf[0], buflen);

	}

	return err;
}

static int
wl_antsel(void *wl, cmd_t *cmd, char **argv)
{
	const char *ant_sel = "fixed";
	char *val_name;
	wlc_antselcfg_t val = {{0}, 0};
	int err, argc, i;
	char *endptr = NULL;
	uint32 txchain_bitmap = 0;
	uint16 antsel_mask = 0;

	/* toss the command name */
	val_name = *argv++;

	if (!*argv) {
		if (cmd->get < 0)
			return -1;
	    if ((err = wlu_iovar_get(wl, "txchain", &txchain_bitmap, sizeof(txchain_bitmap))) < 0)
				return err;

		/* iterate over max 4 chains */
		for (i = 0; i < 4; i ++) {
			if (!(txchain_bitmap & (1<<i)))
				antsel_mask |=  (0xF << i * 4);
		}

		if ((err = wlu_iovar_get(wl, val_name, &val, sizeof(wlc_antselcfg_t))) < 0)
			return err;

		printf("C3C2C1C0: ");
		for (i = ANT_SELCFG_TX_UNICAST; i < ANT_SELCFG_MAX; i++) {
			if (val.ant_config[i] & ANT_SELCFG_AUTO)
				ant_sel = "auto";
			printf("0x%04X %s ",
			antsel_mask | (val.ant_config[i] & ANT_SELCFG_MASK), ant_sel);
		}
		printf("\n");
	} else {
		/* arg count */
		for (argc = 0; argv[argc]; argc++);

		if ((argc >= 2 && argc <= 3) || argc > ANT_SELCFG_MAX) {
			printf("invalid %d args\n", argc);
			return BCME_USAGE_ERROR;
		}

		val.ant_config[ANT_SELCFG_TX_UNICAST] = (uint8)strtol(*argv++, &endptr, 0);
		printf("UTX 0x%02x\n", val.ant_config[ANT_SELCFG_TX_UNICAST]);
		if (*endptr != '\0') {
			printf("Invaild UTX parameter: %s\n", *argv);
			return BCME_USAGE_ERROR;
		}
		if (argc == 1) {
			val.ant_config[ANT_SELCFG_RX_UNICAST] =
				val.ant_config[ANT_SELCFG_TX_UNICAST];
			val.ant_config[ANT_SELCFG_TX_DEF] = val.ant_config[ANT_SELCFG_TX_UNICAST];
			val.ant_config[ANT_SELCFG_RX_DEF] = val.ant_config[ANT_SELCFG_TX_UNICAST];
		} else {
			val.ant_config[ANT_SELCFG_RX_UNICAST] = (uint8)strtol(*argv++, &endptr, 0);
			printf("URX 0x%02x\n", val.ant_config[ANT_SELCFG_RX_UNICAST]);
			if (*endptr != '\0') {
				printf("Invaild URX parameter: %s\n", *argv);
				return BCME_USAGE_ERROR;
			}
			val.ant_config[ANT_SELCFG_TX_DEF] = (uint8)strtol(*argv++, &endptr, 0);
			printf("DTX 0x%02x\n", val.ant_config[ANT_SELCFG_TX_DEF]);
			if (*endptr != '\0') {
				printf("Invaild DTX parameter: %s\n", *argv);
				return BCME_USAGE_ERROR;
			}
			val.ant_config[ANT_SELCFG_RX_DEF] = (uint8)strtol(*argv++, &endptr, 0);
			printf("DRX 0x%02x\n", val.ant_config[ANT_SELCFG_RX_DEF]);
			if (*endptr != '\0') {
				printf("Invaild DRX parameter: %s\n", *argv);
				return BCME_USAGE_ERROR;
			}
		}
		err = wlu_iovar_set(wl, val_name, &val, sizeof(wlc_antselcfg_t));
	}
	return err;
}

static int
wl_txfifo_sz(void *wl, cmd_t *cmd, char **argv)
{
	char *param;
	const char *cmdname = "txfifo_sz";
	wl_txfifo_sz_t ts, *reply;
	uint fifo;
	int err;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return BCME_USAGE_ERROR;

	fifo = atoi(param);
	if (fifo > NFIFO_LEGACY)
		return BCME_USAGE_ERROR;
	ts.fifo = fifo;
	ts.magic = WL_TXFIFO_SZ_MAGIC;

	if ((param = *++argv)) {
		ts.size = atoi(param);
		err = wlu_var_setbuf(wl, cmdname, &ts, sizeof(ts));
	} else {
		if ((err = wlu_var_getbuf_sm(wl, cmdname, &ts, sizeof(ts), &ptr) < 0))
			return err;
		reply = (wl_txfifo_sz_t *)ptr;
		printf("fifo %d size %d\n", fifo, reply->size);
	}
	return err;
}

#ifdef linux

static int
wl_escan_event_check(void *wl, cmd_t *cmd, char **argv)
{
	int					fd, err, i, octets;
	struct sockaddr_ll	sll;
	struct ifreq		ifr;
	char				ifnames[IFNAMSIZ] = {"eth1"};
	uint8               print_flag = 4;
	bcm_event_t			* event;
	uint32              status;
	char*				data;
	int					event_type;
	uint8	event_inds_mask[WL_EVENTING_MASK_LEN];	/* event bit mask */

	wl_escan_result_v2_t* escan_data;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		if (strlen(*argv) >= IFNAMSIZ) {
			printf("Interface name %s too long\n", *argv);
			return -1;
		}
		strncpy(ifnames, *argv, (IFNAMSIZ - 1));
		if (*++argv)
			print_flag = atoi(*argv);
	} else {
		strncpy(ifnames, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifnames, (IFNAMSIZ - 1));

	memset(event_inds_mask, '\0', WL_EVENTING_MASK_LEN);

	if ((err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		return (err);
	event_inds_mask[WLC_E_ESCAN_RESULT / 8] |= 1 << (WLC_E_ESCAN_RESULT % 8);
	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		return (err);

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		return -1;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		close(fd);
		return -1;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		close(fd);
		return -1;
	}

	data = (char*)malloc(ESCAN_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			ESCAN_EVENTS_BUFFER_SIZE);
		close(fd);
		return BCME_NOMEM;
	}

	while (1) {
		octets = recv(fd, data, ESCAN_EVENTS_BUFFER_SIZE, 0);
		event = (bcm_event_t *)data;

		event_type = ntoh32(event->event.event_type);

		if ((event_type == WLC_E_ESCAN_RESULT) && (octets > 0)) {
			escan_data = (wl_escan_result_v2_t*)&data[sizeof(bcm_event_t)];
			status = ntoh32(event->event.status);

			if (print_flag & 1)
				printf("WLC_E_ESCAN_RESULT, (sync_id,status) = (%d,%d)\n",
				escan_data->sync_id, status);

			if (print_flag & 2)
				for (i = 0; i < escan_data->bss_count; i++) {
					dump_bss_info(&escan_data->bss_info[i]);
				}

			if (print_flag & 4) {
				if (status == WLC_E_STATUS_PARTIAL) {
					printf("sync_id: %d, WLC_E_STATUS_PARTIAL\n",
					escan_data->sync_id);
					for (i = 0; i < escan_data->bss_count; i++) {
						dump_bss_info(&escan_data->bss_info[i]);
					}
				}
				if (status == WLC_E_STATUS_SUCCESS)
					printf("sync_id: %d, WLC_E_STATUS_SUCCESS => SCAN_DONE\n",
					escan_data->sync_id);
				if ((status != WLC_E_STATUS_SUCCESS) &&
					(status != WLC_E_STATUS_PARTIAL))
					printf("sync_id: %d, status:%d, misc. error/abort\n",
					escan_data->sync_id, status);
			}

			if (print_flag & 8) {
				wl_bss_info_v109_t *bi = (wl_bss_info_v109_t*)escan_data->bss_info;
				int    remainder = bi->ie_length;
				int    processed = sizeof(wl_escan_result_v2_t);
				uint8* iebuf = &((uint8*)escan_data)[
					sizeof(wl_escan_result_v2_t)];

				if (status != WLC_E_STATUS_PARTIAL)
					continue;

				printf("MOREINFO: (sync_id,buflen,ielen) = (%d,%d,%d)\n",
					escan_data->sync_id,
					escan_data->buflen,
					bi->ie_length);

				/* do a tlv sanity check */
				while (remainder > 0) {
					processed += 1 + 1 + iebuf[1];
					remainder -= 1 + 1 + iebuf[1];
					iebuf     += 1 + 1 + iebuf[1];
				}
				if (processed >= ESCAN_EVENTS_BUFFER_SIZE)
					break;

				if (remainder != 0) {
					printf("ERROR: IE tlv sanity check failed for "
						"(ssid,sync_id,buflen,ielen,remainder) = "
						"(%s,%d,%d,%d,%d)\n",
						bi->SSID,
						escan_data->sync_id, escan_data->buflen,
						bi->ie_length,
						remainder);
					iebuf = &((uint8*)escan_data)[
						sizeof(wl_escan_result_v2_t)];
					if ((escan_data->buflen - sizeof(wl_escan_result_v2_t))
						> 0) {
						for (i = 0; i < (int)(escan_data->buflen -
							sizeof(wl_escan_result_v2_t)); i++) {
							printf("%02x ", iebuf[i]);
						}
						printf("\n");
					}
				}
			}
		}
	}

	/* if we ever reach here */
	free(data);
	close(fd);

	return (0);
}

static int
wl_escanresults(void *wl, cmd_t *cmd, char **argv)
{
	int params_size = (WL_SCAN_PARAMS_FIXED_SIZE + OFFSETOF(wl_escan_params_t, params)) +
	    (WL_NUMCHANNELS * sizeof(uint16));
	wl_escan_params_t *params;
	int fd, err, octets;
	int err2 = BCME_OK;
	struct sockaddr_ll sll;
	struct ifreq ifr;
	bcm_event_t *event;
	uint32 status;
	char *data;
	int event_type;
	uint8 event_inds_mask[WL_EVENTING_MASK_LEN];	/* event bit mask */
	bool revert_event_bit = FALSE;
	wl_escan_result_v2_t *escan_data;
	struct escan_bss *escan_bss_head = NULL;
	struct escan_bss *escan_bss_tail = NULL;
	struct escan_bss *result;

	fd_set rfds;
	struct timeval tv;
	int retval;

	params_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_escan_params_t*)malloc(params_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", params_size);
		return BCME_NOMEM;
	}
	memset(params, 0, params_size);

	err = wl_scan_prep(wl, cmd, argv, &params->params, &params_size);
	if (err)
		goto exit2;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));

	memset(event_inds_mask, '\0', WL_EVENTING_MASK_LEN);

	/* Read the event mask from driver and unmask the event WLC_E_ESCAN_RESULT */
	if ((err = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		goto exit2;

	if (isclr(event_inds_mask, WLC_E_ESCAN_RESULT)) {
		setbit(event_inds_mask, WLC_E_ESCAN_RESULT);
		if ((err = wlu_iovar_set(wl, "event_msgs",
		                         &event_inds_mask, WL_EVENTING_MASK_LEN)))
			goto exit2;
		revert_event_bit = TRUE;
	}

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		err = -1;
		goto exit2;
	}

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);

	/* bind the socket first before starting escan so we won't miss any event */
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		goto exit2fd;
	}

	params->version = htod32(ESCAN_REQ_VERSION);
	params->action = htod16(WL_SCAN_ACTION_START);

	srand((unsigned)time(NULL));
	params->sync_id = htod16(rand() & 0xffff);

	params_size += OFFSETOF(wl_escan_params_t, params);
	err = wlu_iovar_setbuf(wl, "escan", params, params_size, buf, WLC_IOCTL_MAXLEN);
	if (err != 0)
		goto exit2fd;

	data = (char*)malloc(ESCAN_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			ESCAN_EVENTS_BUFFER_SIZE);
		err = BCME_NOMEM;
		goto exit2fd;
	}

	/* Set scan timeout */
	tv.tv_sec = WL_EVENT_TIMEOUT;
	tv.tv_usec = 0;

	/* receive scan result */
	while ((retval = select(fd+1, &rfds, NULL, NULL, &tv)) > 0) {
		octets = recv(fd, data, ESCAN_EVENTS_BUFFER_SIZE, 0);
		event = (bcm_event_t *)data;
		event_type = ntoh32(event->event.event_type);

		if ((event_type == WLC_E_ESCAN_RESULT) && (octets > 0)) {
			escan_data = (wl_escan_result_v2_t*)&data[sizeof(bcm_event_t)];
			status = ntoh32(event->event.status);

			if (status == WLC_E_STATUS_PARTIAL) {
				wl_bss_info_v109_1_t *bi = (wl_bss_info_v109_1_t*)
					&escan_data->bss_info[0];
				wl_bss_info_v109_1_t *bss;

				/* check if we've received info of same BSSID */
				for (result = escan_bss_head; result; result = result->next) {
					bss = (wl_bss_info_v109_1_t*)result->bss;

					if (wlu_bcmp(bi->BSSID.octet, bss->BSSID.octet,
						ETHER_ADDR_LEN) ||
						CHSPEC_BAND(bi->chanspec) !=
						CHSPEC_BAND(bss->chanspec) ||
						bi->SSID_len != bss->SSID_len ||
						wlu_bcmp(bi->SSID, bss->SSID, bi->SSID_len))
						continue;

					/* We've already got this BSS. Update RSSI if necessary */
					/* Prefer valid RSSI */
					if (bi->RSSI == WLC_RSSI_INVALID)
						break;
					else if (bss->RSSI == WLC_RSSI_INVALID)
						goto escan_update;

					/* Prefer on-channel RSSI */
					if (!(bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) &&
					     (bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL))
						break;
					else if ((bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) &&
						!(bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL))
						goto escan_update;

					/* Prefer probe response RSSI */
					if ((bi->flags & WL_BSS_FLAGS_FROM_BEACON) &&
					    !(bss->flags & WL_BSS_FLAGS_FROM_BEACON))
						break;
					else if (!(bi->flags & WL_BSS_FLAGS_FROM_BEACON) &&
					          (bss->flags & WL_BSS_FLAGS_FROM_BEACON))
						goto escan_update;

					/* Prefer better RSSI */
					if (bi->RSSI <= bss->RSSI)
						break;

escan_update:				/* Update known entry */
					bss->RSSI = bi->RSSI;
					bss->SNR = bi->SNR;
					bss->phy_noise = bi->phy_noise;
					bss->flags = bi->flags;
					break;
				}

				if (!result) {
					/* New BSS. Allocate memory and save it */
					struct escan_bss *ebss = malloc(
						OFFSETOF(struct escan_bss, bss)	+ bi->length);

					if (!ebss) {
						perror("can't allocate memory for bss");
						goto exit1;
					}

					ebss->next = NULL;
					memcpy(&ebss->bss, bi, bi->length);
					if (escan_bss_tail) {
						escan_bss_tail->next = ebss;
					}
					else {
						escan_bss_head = ebss;
					}
					escan_bss_tail = ebss;
				}
			}
			else if (status == WLC_E_STATUS_SUCCESS) {
				/* Escan finished. Let's go dump the results. */
				break;
			}
			else {
				printf("sync_id: %d, status:%d, misc. error/abort\n",
					escan_data->sync_id, status);
				goto exit1;
			}
		}
	}

	if (retval > 0) {
		/* print scan results */
		for (result = escan_bss_head; result; result = result->next) {
			dump_bss_info(result->bss);
		}
	} else if (retval == 0) {
		printf(" Scan timeout! \n");
	} else {
		printf(" Receive scan results failed!\n");
	}

exit1:
	/* free scan results */
	result = escan_bss_head;
	while (result) {
		struct escan_bss *tmp = result->next;
		free(result);
		result = tmp;
	}

	free(data);
exit2fd:
	close(fd);
exit2:
	free(params);

	/* Revert the event bit if appropriate */
	if (revert_event_bit &&
	    !(err2 = wlu_iovar_get(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN))) {
		clrbit(event_inds_mask, WLC_E_ESCAN_RESULT);
		err2 = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN);
	}

	if (err2) {
		fprintf(stderr, "Failed to revert event mask, error %d\n", err2);
	}
	return err ? err : err2;
}

/*
* Description:
* Primary job of this routine is to open a socket and listening
* on this socket for BRCM events and display it on console
*/

#define WL_EVENTS_BUFFER_SIZE 2048
static int
wl_event_check(void *wl, cmd_t *cmd, char **argv)
{
	int fd, err, octets;
	struct sockaddr_ll	sll;
	struct ifreq		ifr;
	char ifnames[IFNAMSIZ] = {"eth1"};
	bcm_event_t * event;
	uint32 status;
	char* data;
	uint event_type;
	char time_str[TIME_STR_SZ];
	struct tm  ts;
	struct timespec tspec;
	uint policy;
	pid_t pid;
	struct sched_param sp;
	uint max;
	uint ret;
	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		if (strlen(*argv) >= IFNAMSIZ) {
			printf("Interface name %s too long\n", *argv);
			return -1;
		}
		strncpy(ifnames, *argv, (IFNAMSIZ - 1));
	} else if (wl) {
		strncpy(ifnames, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifnames, (IFNAMSIZ - 1));

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		return -1;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		close(fd);
		return -1;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot bind %d\n", err);
		close(fd);
		return -1;
	}

	data = (char*)malloc(WL_EVENTS_BUFFER_SIZE);

	if (data == NULL) {
		printf("Cannot not allocate %d bytes for events receive buffer\n",
			ESCAN_EVENTS_BUFFER_SIZE);
		close(fd);
		return BCME_NOMEM;
	}
	/* make this process as rtprocess with highest priority */

	/* get our scheduling policy */
	pid = getpid();
	policy = sched_getscheduler(pid);
	switch (policy) {
	case SCHED_OTHER:
	{
		max = sched_get_priority_max(SCHED_RR);
		sp.sched_priority = max;
		/* set the process scheduling also to RR */
		sched_setscheduler(pid, SCHED_RR, &sp);
		break;
	}
	case SCHED_RR:
	{
		max = sched_get_priority_max(SCHED_RR);
		sp.sched_priority = max;
		/* set the priority to max */
		sched_setparam(pid, &sp);
	break;
	}
	case SCHED_FIFO:
	{
		max = sched_get_priority_max(SCHED_RR);
		sp.sched_priority = max;
		sched_setscheduler(pid, SCHED_RR, &sp);
		break;
	}
	case -1:
		perror("sched_getscheduler fail!!");
		break;
	default:
		printf("Unknown policy:%u!!Will not change policy to max!!\n", policy);
		break;
	}

	while (1) {
		octets = recv(fd, data, WL_EVENTS_BUFFER_SIZE, 0);
		UNUSED_PARAMETER(octets);
		event = (bcm_event_t *)data;
		event_type = ntoh32(event->event.event_type);
		/* get the  RTC  time in sec/usec */
		ret = clock_gettime(CLOCK_REALTIME, &tspec);
		if (ret) {
			printf("clock_gettime error:%d\n", ret);
			free(data);
			close(fd);
			return -1;
		}
		/* convert it in year-month-day-hour-min-sec */
		ts = *localtime(&tspec.tv_sec);
		strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &ts);
		/* print first line of events */
		printf("%s:%09d %-8.16s %d:%s:", time_str, (uint)tspec.tv_nsec, ifnames,
			event_type, wlu_lookup_name(wlc_event_names,
			event_type));

		/* event subtype interpretation */
		switch (event_type)
		{
			case WLC_E_SET_SSID:
			{
				status = ntoh32(event->event.status);
				if (status == WLC_E_STATUS_SUCCESS) {
					uint32 ssid_len = ntoh32(event->event.datalen);
					char ssid[SSID_FMT_BUF_LEN];
					wl_format_ssid(ssid, (uint8*)(data+sizeof(bcm_event_t)),
					ssid_len);
					printf("SSID->\"%s\"", ssid);
				}
				else {
					printf("SET_SSID_FAIL:%d", status);
				}
				break;
			}

			case WLC_E_JOIN:
			{
				status = ntoh32(event->event.status);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("JOIN_SUCCESS:");
					printf("%02X:%02X:%02X:%02X:%02X:%02X",
					event->event.addr.octet[0], event->event.addr.octet[1],
					event->event.addr.octet[2], event->event.addr.octet[3],
					event->event.addr.octet[4], event->event.addr.octet[5]);
				}
				else {
					printf("JOIN_FAIL:%d", status);
				}
				break;
			}
			case WLC_E_AUTH:
			{
				status = ntoh32(event->event.status);
				uint32 reason = ntoh32(event->event.reason);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("AUTH_REQ_SUCCESS:%d", reason);
				}
				else {
					printf("AUTH_FAIL:%d", status);
				}
				break;
			}
			case WLC_E_AUTH_IND:
			{
				status = ntoh32(event->event.status);
				uint32 reason = ntoh32(event->event.reason);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("AUTH_IND:%d", reason);
				}
				else {
					printf("AUTH_IND_FAIL:%d", status);
				}
				break;
			}
			case WLC_E_DEAUTH:
			{
				status = ntoh32(event->event.status);
				uint32 reason = ntoh32(event->event.reason);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("DEAUTH_REQ:%d", reason);
				}
				else {
					printf("DEAUTH_REQ_FAIL:%d", status);
				}
				break;
			}
			case WLC_E_DEAUTH_IND:
			{
				status = ntoh32(event->event.status);
				uint32 reason = ntoh32(event->event.reason);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("DEAUTH_IND:%d", reason);
				}
				else {
					printf("DEAUTH_IND:%d", status);
				}
				break;
			}
			case WLC_E_ASSOC:
			{
				status = ntoh32(event->event.status);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("ASSOCRESP_SUCCESS");
				}
				else {
					printf("ASSOCRESP_FAIL:%d", status);
				}
				break;
			}
			case WLC_E_DISASSOC:
			{
				status = ntoh32(event->event.status);
				uint32 reason = ntoh32(event->event.reason);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("DISASSOC_REQ:%d", reason);
				}
				else {
					printf("DISASSOC_REQ_FAIL:%d", status);
				}
			break;
			}
			case WLC_E_DISASSOC_IND:
			{
				status = ntoh32(event->event.status);
				uint32 reason = ntoh32(event->event.reason);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("DISASSOC_IND:%d", reason);
				}
				else {
					printf("DISASSOC_IND_FAIL:%d", status);
				}
				break;
			}
			case WLC_E_SCAN_COMPLETE:
			{
				status = ntoh32(event->event.status);
				if (status == WLC_E_STATUS_SUCCESS) {
					printf("SCAN_SUCCESS");
				}
				else {
					printf("SCAN_FAIL:%d", status);
				}
				break;
			}
			case WLC_E_LINK:
			{
				uint16 flag = ntoh16(event->event.flags);
				if (flag) {
					printf("LINK_UP");
				}
				else {
				printf("LINK_DOWN:%d", ntoh32(event->event.reason));
				}
				break;
			}
			default:
			printf(" ");
			break;
		}
		printf("\n");
		fflush(stdout);
	}

	/* if we ever reach here */
	free(data);
	close(fd);
	return (0);
}

#endif   /* linux */

#define MAX_HOST_ICMP_IPV6     4

int
wl_hostipv6_extended(void *wl, cmd_t *cmd, char **argv)
{

	int ret;
	uint16 *ip_addr;
	wl_icmp_ipv6_cfg_t *cfg;

	if (!*++argv) {
		/* Get */
		void *ptr = NULL;
		uint32 i, k;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		cfg = (wl_icmp_ipv6_cfg_t *)ptr;

		if (cfg->version != WL_ICMP_IPV6_CFG_VERSION) {
			printf("Version mismatch %d, expected %d\n", cfg->version,
				WL_ICMP_IPV6_CFG_VERSION);
			return BCME_VERSION;
		}
		/* Check length */
		if (cfg->length != WL_ICMP_CFG_IPV6_LEN(cfg->num_ipv6)) {
			printf("Length mismatch %d, calculated length %d\n", cfg->length,
				(int)WL_ICMP_CFG_IPV6_LEN(cfg->num_ipv6));
			return BCME_BADARG;
		}
		printf("Total %d Host ipv6 addresses\n", cfg->num_ipv6);
		for (k = 0; k < cfg->num_ipv6; k++) {
			ip_addr = (uint16*)cfg->host_ipv6[k].addr;
			/* Print ipv6 Addr */
			for (i = 0; i < 8; i++) {
				printf("%x", ntoh16(ip_addr[i]));
				if (i < 7)
					printf(":");
			}
			printf("\r\n");
		}
	} else {
		int argc, buf_size;
		struct ipv6_addr ipa_set[MAX_HOST_ICMP_IPV6], null_ipa;

		/* Set */
		memset(null_ipa.addr, 0, IPV6_ADDR_LEN);
		for (argc = 0; argc < MAX_HOST_ICMP_IPV6 && argv[argc]; argc++) {
			if (!wl_atoipv6(argv[argc], &ipa_set[argc]))
				return BCME_USAGE_ERROR;
			if (!memcmp(null_ipa.addr, ipa_set->addr, IPV6_ADDR_LEN)) {
				argc = 0;
				break;
			}
		}
		buf_size = WL_ICMP_CFG_IPV6_LEN(argc);
		cfg = (wl_icmp_ipv6_cfg_t *)malloc(buf_size);
		if (!cfg) {
			printf("%s: Unable to allocate %d bytes!\n", __FUNCTION__, buf_size);
			return BCME_NOMEM;
		}
		memset(cfg, 0, buf_size);
		cfg->version = WL_ICMP_IPV6_CFG_VERSION;
		cfg->fixed_length = WL_ICMP_CFG_IPV6_FIXED_LEN;
		cfg->length = buf_size;
		cfg->flags = 0;
		if (!argc) {
			cfg->flags |= WL_ICMP_IPV6_CLEAR_ALL;
		}
		cfg->num_ipv6 = argc;
		memcpy(cfg->host_ipv6, ipa_set,
			cfg->num_ipv6 * sizeof(struct ipv6_addr));
		ret = wlu_var_setbuf(wl, cmd->name, cfg, buf_size);
		free(cfg);
	}
	return ret;
}

static int
wl_mcast_ar(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	uint argc;
	wl_rmc_entry_t *reply = NULL;
	wl_rmc_entry_t rmc_entry;
	void *ptr = NULL;

	memset(&rmc_entry, 0, sizeof(wl_rmc_entry_t));
	if (!*++argv) {
		/* Get and display activer receiver address */
		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0) {
			return ret;
		}

		reply = (wl_rmc_entry_t*)ptr;

		printf("%s\n", wl_ether_etoa(&reply->addr));
		return 0;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* Set activer receiver's mac address */
	if (argc > 0 && argv[0]) {
		printf(" %s \n", argv[0]);
		if (!wl_ether_atoe(argv[0], &rmc_entry.addr)) {
			if (strlen(argv[0]) == 1 && atoi(argv[0]) == 0) {
				memset(&rmc_entry, 0, sizeof(wl_rmc_entry_t));
			} else {
				printf("Invalid argument, Please enter mac address\n"
					"or enter \"0\" for auto ar selection\n");
				return -1;
			}
		}
	} else {
		printf("Too few arguments\n");
		return -1;
	}

	ret = wlu_var_setbuf(wl, cmd->name, &rmc_entry, sizeof(wl_rmc_entry_t));

	return ret;
}

static void
wl_rate_histo_print(wl_mac_ratehisto_res_t *rate_histo_res)
{
	uint i, nss;

	printf("Rates\n");
	for (i = 0; i < (DOT11_RATE_MAX + 1); i++) {
		if (rate_histo_res->rate[i]) {
			if (DIV_REM(i, 2))
				printf("%.2d\t%d.%d Mbit/s\n",
					rate_histo_res->rate[i], DIV_QUO(i, 2), DIV_REM(i, 2)/10);
			else
				printf("%.2d\t%d Mbit/s\n",
					rate_histo_res->rate[i], DIV_QUO(i, 2));
		}
	}

	printf("MCS indexes:\n");
	for (i = 0; i < (WL_RATESET_SZ_HT_MCS * WL_TX_CHAINS_MAX); i++) {
		if (rate_histo_res->mcs[i]) {
			printf("%d\tMCS %d\n", rate_histo_res->mcs[i], i);
		}
	}

	printf("VHT indexes:\n");
	for (nss = 0; nss < WL_TX_CHAINS_MAX; nss++) {
		for (i = 0; i < WL_RATESET_SZ_VHT_MCS; i++) {
			if (rate_histo_res->vht[i][nss]) {
				printf("%d\tVHT %d Nss %d\n", rate_histo_res->vht[i][nss], i,
					nss + 1);
			}
		}
	}

	return;
}

static int
wl_rate_histo(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err;
	wl_mac_ratehisto_res_t *rate_histo_res;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;

	rate_histo_res = (wl_mac_ratehisto_res_t *)ptr;

	wl_rate_histo_print(rate_histo_res);

	return 0;
}

static int
wl_mac_rate_histo(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr ea;
	int buflen, err;
	wl_mac_ratehisto_cmd_t *rate_histo_cmd;
	wl_mac_ratehisto_res_t *rate_histo_res;

	if (!*++argv || !wl_ether_atoe(*argv, &ea))
		return BCME_USAGE_ERROR;

	strcpy(buf, "mac_rate_histo");
	buflen = strlen(buf) + 1;
	rate_histo_cmd = (wl_mac_ratehisto_cmd_t *)(buf + buflen);
	memcpy((char*)&rate_histo_cmd->ea, (char*)&ea, ETHER_ADDR_LEN);

	if (*++argv)
	{
		/* The access category is obtained and checked for validity */
		rate_histo_cmd->ac_cat = (uint8)strtol(*argv, NULL, 0);
		if (!(rate_histo_cmd->ac_cat == 0x10 || rate_histo_cmd->ac_cat == 0x4)) {
			printf("Only Access Category 0x10 and 0x4 is supported\n");
			return BCME_BADARG;
		}

		if (*++argv) {
			/* The number of pkts to avg is obtained and checked for valid range */
			rate_histo_cmd->num_pkts = (uint8)strtol(*argv, NULL, 10);
		} else {
			/* Set default value as maximum of all access categories
			 * so that it is set to the max value below
			 */
			rate_histo_cmd->num_pkts = 64;
		}

		if (rate_histo_cmd->ac_cat == 0x10 && rate_histo_cmd->num_pkts > 64) {
			rate_histo_cmd->num_pkts = 64;
		} else if (rate_histo_cmd->ac_cat == 0x4 && rate_histo_cmd->num_pkts > 32) {
			rate_histo_cmd->num_pkts = 32;
		}
	} else {
		return BCME_USAGE_ERROR;
	}

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return err;

	rate_histo_res = (wl_mac_ratehisto_res_t *)buf;

	wl_rate_histo_print(rate_histo_res);

	printf("First TSF Timestamp: %08x%08x\n", rate_histo_res->tsf_timer[0][1],
		rate_histo_res->tsf_timer[0][0]);
	printf("Last TSF Timestamp : %08x%08x\n", rate_histo_res->tsf_timer[1][1],
		rate_histo_res->tsf_timer[1][0]);

	return 0;
}

static int
wl_sarlimit(void *wl, cmd_t *cmd, char **argv)
{
	uint i;
	int ret;
	sar_limit_t sar;
	uint argc;
	char *endptr;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc != 0 && argc != sizeof(sar_limit_t)) {
		printf("Error: Input %d SAR values, need total %d SAR values\n",
		       argc, (int)sizeof(sar_limit_t));
		return BCME_USAGE_ERROR;
	}

	if (argc == 0) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &sar, sizeof(sar_limit_t))) < 0) {
			return (ret);
		}
		printf("\t2G:    %4d %4d %4d %4d\n",
		       sar.band2g[0], sar.band2g[1], sar.band2g[2], sar.band2g[3]);
		for (i = 0; i < WLC_SUBBAND_MAX; i++) {
			printf("\t5G[%1d]  %4d %4d %4d %4d\n", i,
			       sar.band5g[i][0], sar.band5g[i][1],
			       sar.band5g[i][2], sar.band5g[i][3]);
		}
	} else {
		uint8 *ptr = (uint8 *)&sar;
		memset(ptr, WLC_TXPWR_MAX, sizeof(sar_limit_t));
		for (i = 0; i < argc; i++) {
			ptr[i] = (uint8)(strtol(argv[1 + i], &endptr, 0));
			if (*endptr != '\0') {
				printf("error\n");
				return BCME_USAGE_ERROR;
			}
		}
		printf("\t2G:    %4d %4d %4d %4d\n",
		       sar.band2g[0], sar.band2g[1], sar.band2g[2], sar.band2g[3]);
		for (i = 0; i < WLC_SUBBAND_MAX; i++) {
			printf("\t5G[%1d]  %4d %4d %4d %4d\n", i,
			       sar.band5g[i][0], sar.band5g[i][1],
			       sar.band5g[i][2], sar.band5g[i][3]);
		}
		ret = wlu_iovar_set(wl, cmd->name, &sar, sizeof(sar_limit_t));
	}

	return ret;
}

#ifdef SR_DEBUG
/* Displays PMU related info on screen */
static int
wl_dump_pmu(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	pmu_reg_t *pmu_var;
	int err;
	uint i;
	uint32 pmu_chip_ctl_reg;
	uint32 pmu_chip_reg_reg;
	uint32 pmu_chip_pll_reg;
	uint32 pmu_chip_res_reg;
	UNUSED_PARAMETER(argv);
	if ((err = wlu_var_getbuf_med (wl, cmd->name, NULL, 0, &ptr)))
		return (err);
	pmu_var = (pmu_reg_t *)ptr;
	printf("PMU Control          : 0x%08x\n", pmu_var->pmu_control);
	printf("PMU Capabilities     : 0x%08x\n", pmu_var->pmu_capabilities);
	printf("PMU Status           : 0x%08x\n", pmu_var->pmu_status);
	printf("Resource State       : 0x%08x\n", pmu_var->res_state);
	printf("Resurce Pending      : 0x%08x\n", pmu_var->res_pending);
	printf("PMU Timer            : 0x%08x\n", pmu_var->pmu_timer1);
	printf("Minimum Resource Mask: 0x%08x\n", pmu_var->min_res_mask);
	printf("Maximum Resource Mask: 0x%08x\n", pmu_var->max_res_mask);
	/* Displays values of the 5 PMU Chip Control Registers */
	pmu_chip_ctl_reg = (pmu_var->pmu_capabilities & 0xf8000000);
	pmu_chip_ctl_reg = pmu_chip_ctl_reg >> 27;
	for (i = 0; i < pmu_chip_ctl_reg; i++) {
		printf("PMU ChipControl[%d]   : 0x%08x\n", i, pmu_var->pmu_chipcontrol1[i]);
	}
	/* Displays values of the 6 PMU Reg Control Registers */
	pmu_chip_reg_reg = (pmu_var->pmu_capabilities & 0x07c00000);
	pmu_chip_reg_reg = pmu_chip_reg_reg >> 22;
	for (i = 0; i < pmu_chip_reg_reg; i++) {
		printf("PMU RegControl[%d]    : 0x%08x\n", i, pmu_var->pmu_regcontrol[i]);
	}
	/* Displays values of the 6 PMU Pll Control Registers */
	pmu_chip_pll_reg = (pmu_var->pmu_capabilities & 0x003e0000);
	pmu_chip_pll_reg = pmu_chip_pll_reg >> 17;
	for (i = 0; i < pmu_chip_pll_reg; i++) {
		printf("PMU PllControl[%d]    : 0x%08x\n", i, pmu_var->pmu_pllcontrol[i]);
	}
	/* Displays values of the 31 PMU Resource Up/Down Timer */
	pmu_chip_res_reg = (pmu_var->pmu_capabilities & 0x00001f00);
	pmu_chip_res_reg = pmu_chip_res_reg >> 8;
	for (i = 0; i < pmu_chip_res_reg; i++) {
		printf("PMU Resource Up/Down Timer[%d]    : 0x%08x\n", i,
			pmu_var->pmu_rsrc_up_down_timer[i]);
	}
	/* Displays values of the 31 PMU Resource Dependancy Mask */
	pmu_chip_res_reg = (pmu_var->pmu_capabilities & 0x00001f00);
	pmu_chip_res_reg = pmu_chip_res_reg >> 8;
	for (i = 0; i < pmu_chip_res_reg; i++) {
		printf("PMU Resource Dependancy Mask[%d]    : 0x%08x\n", i,
			pmu_var->rsrc_dep_mask[i]);
	}
	return 0;
}
#endif /* #ifdef SR_DEBUG */

static int
wl_antgain(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	int	err = 0;
	uint	val;
	uint8	ag[2];
	uint8	*rag;
	void	*ptr;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {	/* write maxpower */
		if (find_pattern(argv, "ag0", &val))
			ag[0] = val & 0xff;
		else {
			printf("Missing ag0\n");
			return BCME_USAGE_ERROR;
		}

		if (find_pattern(argv, "ag1", &val))
			ag[1] = val & 0xff;
		else {
			printf("Missing ag1\n");
			return BCME_USAGE_ERROR;
		}

		if ((err = wlu_var_setbuf(wl, "antgain", &ag, 2 * sizeof(uint8)) < 0)) {
			printf("wl_antgain: fail to set\n");
		}
	} else {
		if ((err = wlu_var_getbuf(wl, "antgain", NULL, 0, &ptr) < 0)) {
			printf("wl_antgain: fail to get antgain\n");
			return err;
		}
		rag = (uint8*)ptr;
		printf("ag0=%x\n", rag[0]);
		printf("ag1=%x\n", rag[1]);
	}

	return err;
#endif /* _CFE_ */
}

#define MAX_PWR_STAT_TYPES	32

static int
wl_hmaptest(void *wl, cmd_t *cmd, char **argv)
{
	int opt_err;
	miniopt_t to;
	pcie_hmaptest_t hmaptest_params;
	int ret = USAGE_ERROR;
	bool stop = FALSE;

	BCM_REFERENCE(cmd);
	memset(&hmaptest_params, 0, sizeof(hmaptest_params));
	if (!*++argv) {
		printf("HMAPTEST: get iovar not supported use -s to stop hmaptest\r\n");
		return BCME_UNSUPPORTED;
	} else {
		miniopt_init(&to, __FUNCTION__, NULL, FALSE);
		hmaptest_params.xfer_len = 512;
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				return BCME_USAGE_ERROR;
			}
			argv += to.consumed;
			if (to.opt == 'l') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse \"%s\" as an int for"
						" length \n", __FUNCTION__, to.valstr);
					goto error;
				}
				hmaptest_params.xfer_len = htol32(to.val);
			}
			if (to.opt == 'a') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse \"%s\" as an int for"
						" accesstype \n", __FUNCTION__, to.valstr);
					goto error;
				}
				hmaptest_params.accesstype = htol32(to.val);
				if (hmaptest_params.accesstype >= HMAPTEST_ACCESS_NONE) {
					fprintf(stderr, "%s: improper accesstype %d exit\n",
					__FUNCTION__, hmaptest_params.accesstype);
				}
			}
			if (to.opt == 'w') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse \"%s\" as an int for"
						" write \n", __FUNCTION__, to.valstr);
					goto error;
				}
				hmaptest_params.is_write = htol32(to.val);
			}
			if (to.opt == 'i') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse \"%s\" as an int for"
						" invalid \n", __FUNCTION__, to.valstr);
					goto error;
				}
				hmaptest_params.is_invalid = htol32(to.val);
			}
			if (to.opt == 's') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse \"%s\" as an int for"
						" stop \n", __FUNCTION__, to.valstr);
					goto error;
				}
				if (to.val)
					stop = TRUE;
				else {
					fprintf(stderr, "%s: %d is not a valid value for stop\n",
						__FUNCTION__, to.val);
					goto error;
				}
			}
		}
		if (stop) {
			memset(&hmaptest_params, 0, sizeof(hmaptest_params));
			hmaptest_params.accesstype = HMAPTEST_ACCESS_NONE;
		}
		hmaptest_params.version = htol16(PCIE_HMAPTEST_VERSION);
		hmaptest_params.length = htol16(sizeof(hmaptest_params));
		ret = wlu_iovar_set(wl, cmd->name, (void *) &hmaptest_params,
		        sizeof(hmaptest_params));
	}

	printf("%d\n", ret);
	return ret;
error:
	return BCME_ERROR;

}

static int
wl_hmap(void *wl, cmd_t *cmd, char **argv)
{
	pcie_hmap_t *hmap_params;
	int ret = USAGE_ERROR;
	uint i;
	int len = sizeof(pcie_hmap_t) + ((PCIE_MAX_HMAP_WINDOWS - 1) * sizeof(hmapwindow_t));

	BCM_REFERENCE(cmd);
	hmap_params = (pcie_hmap_t *)malloc(len);
	if (hmap_params == NULL) {
		fprintf(stderr, "malloc failed to allocate %d bytes\n", len);
		return BCME_NOMEM;
	}

	memset(hmap_params, 0, len);
	if (!*++argv) {
		/* get and print hmap vio regs in hmap get iovar */
		if ((ret = wlu_iovar_get(wl, cmd->name, hmap_params, len)) < 0)
		        goto error;

		printf("HMAP: status = %d\n", ltoh32(hmap_params->enable));
		printf("HMAP: Error Registers\r\n");
		printf("HMAP: Violation Addr Lo = %08x\r\n",
			ltoh32(hmap_params->hmap_violationaddr_lo));
		printf("HMAP: Violation Addr Hi = %08x\r\n",
			ltoh32(hmap_params->hmap_violationaddr_hi));
		printf("HMAP: Violation Info = %08x\r\n", ltoh32(hmap_params->hmap_violation_info));
		printf("HMAP: Window Config = %08x\r\n", ltoh32(hmap_params->window_config));

		for (i = 0; i < ltoh32(hmap_params->nwindows); i++) {
			printf("hmap: window %d address lower=%08x upper=%08x length=%08x\n",
				i, ltoh32(hmap_params->hwindows[i].baseaddr_lo),
				ltoh32(hmap_params->hwindows[i].baseaddr_hi),
				ltoh32(hmap_params->hwindows[i].windowlength));
		}

	} else {
		char *endptr;
		if (*argv == NULL) {
			ret = BCME_USAGE_ERROR;
			goto error;
		}

		hmap_params->enable = (uint32)strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "%s: %s: error parsing \"%s\" as an integer\n",
			        wlu_av0, cmd->name, *argv);
			ret = BCME_USAGE_ERROR;
			goto error;
		}
		hmap_params->version = htol16(PCIE_HMAP_VERSION);
		hmap_params->length = htol16(sizeof(hmap_params));

		ret = wlu_iovar_set(wl, cmd->name, (void *)hmap_params,
		        sizeof(*hmap_params));
	}
error:
	printf("%d\n", ret);
	free(hmap_params);
	return ret;

}

static int
wl_pwrstats(void *wl, cmd_t *cmd, char **argv)
{
	wl_pwrstats_query_t *p_query;
	wl_pwrstats_t	*p_pwrstats;
	void	*ptr = NULL;
	int	rc = 0;
	uint	i, argc, len, taglen;
	uint16	type;

	/* Count <type> args and allocate buffer */
	for (argv++, argc = 0; argv[argc]; argc++)
		;
	if (argc > MAX_PWR_STAT_TYPES) {
		fprintf(stderr, "Currently limited to %d types in one query\n",
		        MAX_PWR_STAT_TYPES);
		return -1;
	}

	len = OFFSETOF(wl_pwrstats_query_t, type) + argc * sizeof(uint16);
	p_query = (wl_pwrstats_query_t *)malloc(len);
	if (p_query == NULL) {
		fprintf(stderr, "malloc failed to allocate %d bytes\n", len);
		return -1;
	}

	/* Build a list of types */
	p_query->length = argc;
	for (i = 0; i < argc; i++, argv++) {
		char *endptr;
		p_query->type[i] = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Type '%s' (arg %d) not a number?\n", *argv, i);
			free(p_query);
			return -1;
		}
	}

	/* Now issue the get with the query as parameter */
	rc = wlu_var_getbuf(wl, cmd->name, p_query, len, &ptr);
	free(p_query);
	if (rc < 0)
		return rc;

	p_pwrstats = (wl_pwrstats_t *)ptr;

	if (dtoh16(p_pwrstats->version) != WL_PWRSTATS_VERSION) {
		printf("Power stats version mismatch\n");
		return BCME_ERROR;
	}

	/* Basic information */
	printf("Version: %d, Length %d bytes\n",
	       dtoh16(p_pwrstats->version), dtoh16(p_pwrstats->length));

	/* Run down tags displaying content */
	len = dtoh16(p_pwrstats->length) - WL_PWR_STATS_HDRLEN;
	for (ptr = p_pwrstats->data; len >= 2 * sizeof(uint16); *(uint8**)&ptr += taglen) {
		/* Grab tag/len words */
		type = dtoh16(((uint16*)ptr)[0]);
		taglen = dtoh16(((uint16*)ptr)[1]);

		if ((taglen < 2 * sizeof(uint16)) || (taglen > len)) {
			fprintf(stderr, "Bad len %d for tag %d, remaining len %d\n",
			        taglen, type, len);
			rc = BCME_ERROR;
			break;
		}

		if (taglen & 0xF000) {
			fprintf(stderr, "Resrved bits in len %d for tag %d, remaining len %d\n",
			        taglen, type, len);
			rc = BCME_ERROR;
			break;
		}

		/* Tag-specific display */
		switch (type) {
		case WL_PWRSTATS_TYPE_SLICE_INDEX:
		{
			wl_pwr_slice_index_t stats;

			if (taglen < sizeof(wl_pwr_slice_index_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_slice_index_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&stats, ptr, taglen);
			printf("Slice index: %d\n", stats.slice_index);

		}
		break;
		case WL_PWRSTATS_TYPE_PHY:
		{
			wl_pwr_phy_stats_t stats;

			if (taglen < sizeof(wl_pwr_phy_stats_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_phy_stats_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&stats, ptr, taglen);
			printf("PHY:\n"
			       "  TX Duration: %u\n"
			       "  RX Duration: %u\n",
			       dtoh32(stats.tx_dur),
			       dtoh32(stats.rx_dur));
		}
		break;

		case WL_PWRSTATS_TYPE_SCAN:
		{
			wl_pwr_scan_stats_t stats;

			if (taglen < sizeof(wl_pwr_scan_stats_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_scan_stats_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&stats, ptr, taglen);
			printf("SCAN:\n"
			       "  User-Scan:\tCount: %u\tDuration: %u\n"
			       "  Assoc-Scan:\tCount: %u\tDuration: %u\n"
			       "  Roam-Scan:\tCount: %u\tDuration: %u\n"
			       "  PNO-Scan:\tCount: %u\tDuration: %u\n"
			       "  Other-Scan:\tCount: %u\tDuration: %u\n",
			       dtoh32(stats.user_scans.count),
			       dtoh32(stats.user_scans.dur),
			       dtoh32(stats.assoc_scans.count),
			       dtoh32(stats.assoc_scans.dur),
			       dtoh32(stats.roam_scans.count),
			       dtoh32(stats.roam_scans.dur),
			       dtoh32(stats.pno_scans[0].count),
			       dtoh32(stats.pno_scans[0].dur),
			       dtoh32(stats.other_scans.count),
			       dtoh32(stats.other_scans.dur));
		}
		break;

		case WL_PWRSTATS_TYPE_USB_HSIC:
		{
			wl_pwr_usb_hsic_stats_t stats;

			if (taglen < sizeof(wl_pwr_usb_hsic_stats_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_usb_hsic_stats_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&stats, ptr, taglen);
			printf("HSIC:\n"
			       "  Suspend count: %u\n"
			       "  Resume count: %u\n"
			       "  Disconnect count: %u\n"
			       "  Reconnect count: %u\n"
			       "  Active duration: %u\n"
			       "  Suspend duration: %u\n"
			       "  Disconnect duration:%u\n",
			       dtoh32(stats.hsic.suspend_ct),
			       dtoh32(stats.hsic.resume_ct),
			       dtoh32(stats.hsic.disconnect_ct),
			       dtoh32(stats.hsic.reconnect_ct),
			       dtoh32(stats.hsic.active_dur),
			       dtoh32(stats.hsic.suspend_dur),
			       dtoh32(stats.hsic.disconnect_dur));
		}
		break;

		case WL_PWRSTATS_TYPE_PCIE:
		{
			wl_pwr_pcie_stats_t stats;

			if (taglen < sizeof(wl_pwr_pcie_stats_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_pcie_stats_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&stats, ptr, taglen);
			if (dtoh32(stats.pcie.l0_cnt) == 0) {
				printf("link stats are not supported for this pcie core\n");
			}
			printf("PCIE:\n"
			       "  D3 Suspend count: %u\n"
			       "  D0 Resume count: %u\n"
			       "  PERST# assert count: %u\n"
			       "  PERST# deassert count: %u\n"
			       "  Active duration: %u ms\n"
			       "  D3 Suspend duration: %u ms\n"
			       "  PERST# duration:%u ms\n"
			       "  l0 cnt:%u dur:%u usecs\n"
			       "  l1 cnt:%u dur:%u usecs\n"
			       "  l1_1 cnt:%u dur:%u usecs\n"
			       "  l1_2 cnt:%u dur:%u usecs\n"
			       "  l2 cnt:%u dur:%u usecs\n"
			       "  LTR_ACTIVE Count %u Duration: %u ms\n"
			       "  LTR_SLEEP Count %u Duration: %u ms\n"
			       "  DeepSleep Count %u Duration: %u ms\n",
			       dtoh32(stats.pcie.d3_suspend_ct),
			       dtoh32(stats.pcie.d0_resume_ct),
			       dtoh32(stats.pcie.perst_assrt_ct),
			       dtoh32(stats.pcie.perst_deassrt_ct),
			       dtoh32(stats.pcie.active_dur),
			       dtoh32(stats.pcie.d3_suspend_dur),
			       dtoh32(stats.pcie.perst_dur),
			       dtoh32(stats.pcie.l0_cnt),
			       dtoh32(stats.pcie.l0_usecs),
			       dtoh32(stats.pcie.l1_cnt),
			       dtoh32(stats.pcie.l1_usecs),
			       dtoh32(stats.pcie.l1_1_cnt),
			       dtoh32(stats.pcie.l1_1_usecs),
			       dtoh32(stats.pcie.l1_2_cnt),
			       dtoh32(stats.pcie.l1_2_usecs),
			       dtoh32(stats.pcie.l2_cnt),
			       dtoh32(stats.pcie.l2_usecs),
			       dtoh32(stats.pcie.ltr_active_ct),
			       dtoh32(stats.pcie.ltr_active_dur),
			       dtoh32(stats.pcie.ltr_sleep_ct),
			       dtoh32(stats.pcie.ltr_sleep_dur),
			       dtoh32(stats.pcie.deepsleep_count),
			       dtoh32(stats.pcie.deepsleep_dur));

			printf("\nIPC Stats\n");
			printf("  Submissions:\tCount: %u\th2d drbl: %u\t",
				dtoh32(stats.pcie.num_submissions),
				dtoh32(stats.pcie.num_h2d_doorbell));
			if (stats.pcie.num_h2d_doorbell)
				printf("Avg per h2d drbl: %d.%d\n",
					DIV_QUO(dtoh32(stats.pcie.num_submissions),
					dtoh32(stats.pcie.num_h2d_doorbell)),
					DIV_REM(dtoh32(stats.pcie.num_submissions),
					dtoh32(stats.pcie.num_h2d_doorbell)));
			else
				printf("  Avg per h2d drbl: 0.0\n");

			printf("  Completions:\tCount: %u\td2h drbl: %u\t",
				dtoh32(stats.pcie.num_completions),
				dtoh32(stats.pcie.num_d2h_doorbell));
			if (stats.pcie.num_d2h_doorbell)
				printf("Avg per d2h drbl: %d.%d\n",
					DIV_QUO(dtoh32(stats.pcie.num_completions),
					dtoh32(stats.pcie.num_d2h_doorbell)),
					DIV_REM(dtoh32(stats.pcie.num_completions),
					dtoh32(stats.pcie.num_d2h_doorbell)));
			else
				printf("Avg per d2h drbl: 0.0\n");

			printf("   Rx Cmplt:\tCount: %u\trx_cmplt drbl: %u\t",
				dtoh32(stats.pcie.num_rxcmplt),
				dtoh32(stats.pcie.num_rxcmplt_drbl));
			if (stats.pcie.num_rxcmplt_drbl)
				printf("Avg per rx_cmplt drbl: %d.%d\n",
					DIV_QUO(dtoh32(stats.pcie.num_rxcmplt),
					dtoh32(stats.pcie.num_rxcmplt_drbl)),
					DIV_REM(dtoh32(stats.pcie.num_rxcmplt),
					dtoh32(stats.pcie.num_rxcmplt_drbl)));
			else
				printf("Avg per rx_cmplt drbl: 0.0\n");

			printf("   Tx Cmplt:\tCount: %u\ttx_cmplt drbl: %u\t",
				dtoh32(stats.pcie.num_txstatus),
				dtoh32(stats.pcie.num_txstatus_drbl));
			if (stats.pcie.num_txstatus_drbl)
				printf("Avg per tx_cmplt drbl: %d.%d\n",
					DIV_QUO(dtoh32(stats.pcie.num_txstatus),
					dtoh32(stats.pcie.num_txstatus_drbl)),
					DIV_REM(dtoh32(stats.pcie.num_txstatus),
					dtoh32(stats.pcie.num_txstatus_drbl)));
			else
				printf("Avg per tx_cmplt drbl: 0.0\n");

		}
		break;

		case WL_PWRSTATS_TYPE_SDIO:
		{
			wl_pwr_sdio_stats_t stats;

			if (taglen < sizeof(wl_pwr_sdio_stats_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_sdio_stats_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&stats, ptr, taglen);
			printf("SDIO:\n"
				"  active duration: %u\n"
				"  wakehost count: %u\n"
				"  intr count: data: %u\tmailbox: %u\terror: %u\n"
				"  ds_wake ON: count: %u\t duration: %u\n"
				"  ds_wake OFF: count: %u\t duration: %u\n"
				"  ds_d0 count: %u\t duration: %u\n"
				"  ds_d3 count: %u\t duration: %u\n"
				"  DEV_WAKE count: ASSRT: %u\tDASST: %u\n"
				"  ds mb rx count: DSACK: %u\tDSNACK: %u\tD3INFORM: %u\n"
				"  ds mb tx count: DSREQ: %u\tDSEXIT: %u\tD3ACK: %u\tD3EXIT: %u\n",

				dtoh32(stats.sdio.active_dur),
				dtoh32(stats.sdio.wakehost_cnt),

				dtoh32(stats.sdio.data_intr_cnt),
				dtoh32(stats.sdio.mb_intr_cnt),
				dtoh32(stats.sdio.error_intr_cnt),

				dtoh32(stats.sdio.ds_wake_on_cnt),
				dtoh32(stats.sdio.ds_wake_on_dur),
				dtoh32(stats.sdio.ds_wake_off_cnt),
				dtoh32(stats.sdio.ds_wake_off_dur),

				dtoh32(stats.sdio.ds_d0_cnt),
				dtoh32(stats.sdio.ds_d0_dur),
				dtoh32(stats.sdio.ds_d3_cnt),
				dtoh32(stats.sdio.ds_d3_dur),

				dtoh32(stats.sdio.ds_dw_assrt_cnt),
				dtoh32(stats.sdio.ds_dw_dassrt_cnt),

				dtoh32(stats.sdio.ds_rx_dsack_cnt),
				dtoh32(stats.sdio.ds_rx_dsnack_cnt),
				dtoh32(stats.sdio.ds_rx_d3inform_cnt),
				dtoh32(stats.sdio.ds_tx_dsreq_cnt),
				dtoh32(stats.sdio.ds_tx_dsexit_cnt),
				dtoh32(stats.sdio.ds_tx_d3ack_cnt),
				dtoh32(stats.sdio.ds_tx_d3exit_cnt));

		}
		break;

		case WL_PWRSTATS_TYPE_PM_AWAKE1:
		{
			wl_pwr_pm_awake_stats_v1_t stats;
			bool skip = FALSE;
			uint32 dur_time, bits;
			uint endidx;

			if (taglen < sizeof(wl_pwr_pm_awake_stats_v1_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_pm_awake_stats_v1_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&stats, ptr, taglen);
			printf("PM WAKE:\n"
			       "  Current Time: %u\n"
			       "  HW MACC: 0x%08x\n"
			       "  SW MACC: 0x%08x\n"
			       "  PM Dur: %u\n"
			       "  MPC Dur: %u\n"
			       "  TSF Drift (Last/Min/Max/Avg/Cnt): %d/%d/%d/%u/%u\n"
			       "  Frts (end_cnt/dur): %u/%u\n",
			       dtoh32(stats.awake_data.curr_time),
			       dtoh32(stats.awake_data.hw_macc),
			       dtoh32(stats.awake_data.sw_macc),
			       dtoh32(stats.awake_data.pm_dur),
			       dtoh32(stats.awake_data.mpc_dur),
			       dtoh32(stats.awake_data.last_drift),
			       dtoh32(stats.awake_data.min_drift),
			       dtoh32(stats.awake_data.max_drift),
			       dtoh32(stats.awake_data.avg_drift),
			       dtoh32(stats.awake_data.drift_cnt),
			       dtoh32(stats.frts_end_cnt),
			       dtoh32(stats.frts_time));

			printf("\n");

			i = endidx = stats.awake_data.pmwake_idx;
			if (endidx > WLC_STA_AWAKE_STATES_MAX_V1) {
				fprintf(stderr, "Unexpected idx %d > %d\n",
				        endidx, WLC_STA_AWAKE_STATES_MAX_V1);
				rc = BCME_ERROR;
				break;
			}

			if (endidx == 0)
				endidx = WLC_STA_AWAKE_STATES_MAX_V1;

			do {
				if (i >= WLC_STA_AWAKE_STATES_MAX_V1)
					i = 0;

				dur_time = dtoh32(stats.awake_data.pm_state[i].timestamp);
				bits = dtoh32(stats.awake_data.pm_state[i].reason);
				if (dur_time == 0 && bits == 0)
					continue;

				printf("  State: %2d  reason: 0x%04x  time: %u\n",
				       i, bits, dur_time);
			} while (++i != endidx);
			printf("\n");

			for (i = 0; i < WLC_PMD_EVENT_MAX_V1; i++) {
				dur_time = dtoh32(stats.awake_data.pmd_event_wake_dur[i]);
				if (dur_time == 0) {
					if (i != 0)
						skip = TRUE;
					continue;
				}
				if (skip) {
					printf("  ---\n");
					skip = FALSE;
				}
				printf("  Event: %2d Wake-Duration: %u\n", i, dur_time);
			}
		}
		break;

		case WL_PWRSTATS_TYPE_PM_AWAKE2:
		{
			wl_pwr_pm_awake_stats_v2_t *stats = (wl_pwr_pm_awake_stats_v2_t *)ptr;
			bool skip = FALSE;
			uint32 dur_time, bits;
			uint endidx;

			if (taglen < sizeof(wl_pwr_pm_awake_stats_v2_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_pm_awake_stats_v2_t));
				rc = BCME_ERROR;
				break;
			}

			printf("PM WAKE:\n"
			       "  Current Time: %u\n"
			       "  HW MACC: 0x%08x\n"
			       "  SW MACC: 0x%08x\n"
			       "  PM Dur: %u\n"
			       "  MPC Dur: %u\n"
			       "  TSF Drift (Last/Min/Max/Avg/Cnt): %d/%d/%d/%u/%u\n"
			       "  Frts (end_cnt/dur): %u/%u\n"
			       "  Flags: %d, ASLEEP=%s\n",
			       dtoh32(stats->awake_data.curr_time),
			       dtoh32(stats->awake_data.hw_macc),
			       dtoh32(stats->awake_data.sw_macc),
			       dtoh32(stats->awake_data.pm_dur),
			       dtoh32(stats->awake_data.mpc_dur),
			       dtoh32(stats->awake_data.last_drift),
			       dtoh32(stats->awake_data.min_drift),
			       dtoh32(stats->awake_data.max_drift),
			       dtoh32(stats->awake_data.avg_drift),
			       dtoh32(stats->awake_data.drift_cnt),
			       dtoh32(stats->awake_data.frts_end_cnt),
			       dtoh32(stats->awake_data.frts_time),
			       stats->awake_data.flags,
			       ((stats->awake_data.flags &
			       WL_PWR_PM_AWAKE_STATS_WAKE_MASK) == WL_PWR_PM_AWAKE_STATS_ASLEEP) ?
			       "True":"False");

			printf("\n");
			i = endidx = stats->awake_data.pmwake_idx;
			if (endidx > stats->awake_data.pm_state_len) {
				fprintf(stderr, "Unexpected idx %d > %d\n",
				        endidx, stats->awake_data.pm_state_len);
				rc = BCME_ERROR;
				break;
			}

			if (endidx == 0)
				endidx = stats->awake_data.pm_state_len;

			do {
				wlc_pm_debug_t *pm_state = (wlc_pm_debug_t *)
					(((uint8 *)&stats->awake_data) +
					stats->awake_data.pm_state_offset);
				if (i >= stats->awake_data.pm_state_len)
					i = 0;

				dur_time = dtoh32(pm_state[i].timestamp);
				bits = dtoh32(pm_state[i].reason);
				if (dur_time == 0 && bits == 0)
					continue;

				printf("  State: %2d  reason: 0x%04x  time: %u\n",
				       i, bits, dur_time);
			} while (++i != endidx);
			printf("\n");

			for (i = 0; i < stats->awake_data.pmd_event_wake_dur_len; i++) {
				uint32 *pmd_event_wake_dur = (uint32 *)
					(((uint8 *)&stats->awake_data) +
					stats->awake_data.pmd_event_wake_dur_offset);
				dur_time = dtoh32(pmd_event_wake_dur[i]);
				if (dur_time == 0) {
					if (i != 0)
						skip = TRUE;
					continue;
				}
				if (skip) {
					printf("  ---\n");
					skip = FALSE;
				}
				printf("  Event: %2d Wake-Duration: %u\n", i, dur_time);
			}
		}
		break;

		case WL_PWRSTATS_TYPE_CONNECTION:
		{
			wl_pwr_connect_stats_t connect_stats;

			if (taglen < sizeof(wl_pwr_connect_stats_t)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen, (int)sizeof(wl_pwr_connect_stats_t));
				rc = BCME_ERROR;
				break;
			}

			memcpy(&connect_stats, ptr, taglen);
			printf("Connect:\n"
					"  Count: %u\n"
					"  Duration: %u\n",
					dtoh32(connect_stats.count),
					dtoh32(connect_stats.dur));
		}
		break;

		case WL_PWRSTATS_TYPE_MIMO_PS_METRICS:
		{
			wl_mimo_meas_metrics_t stats;
			bool ocl_stats = FALSE;

			if (taglen < STRUCT_SIZE_THROUGH(&stats, total_tx_time_3chain)) {
				fprintf(stderr, "Short len for %d: %d < %d\n",
				        type, taglen,
				        (int)STRUCT_SIZE_THROUGH(&stats, total_tx_time_3chain));
				rc = BCME_ERROR;
				break;
			} else if (taglen >= STRUCT_SIZE_THROUGH(&stats, total_rx_time_ocl)) {
				ocl_stats = TRUE;
			}

			memcpy(&stats, ptr, taglen);
			printf("MIMO ps measurement metrics:\n");
			if (ocl_stats) {
				printf("  MIMO  Idle duration:\t%10u (OCL Idle: %10u)\n"
					   "  OCL   Idle duration:\t%10u\n",
					   dtoh32(stats.total_idle_time_mimo),
					   dtoh32(stats.total_idle_time_ocl),
					   dtoh32(stats.total_idle_time_ocl));
			} else {
				printf("  MIMO  Idle duration:\t%10u\n",
				       dtoh32(stats.total_idle_time_mimo));
			}
			printf("  SISO  Idle duration:\t%10u\n",
			       dtoh32(stats.total_idle_time_siso));
			if (ocl_stats) {
				printf("  MIMO  Rx duration:\t%10u (OCL Rx: %10u)\n"
					   "  OCL   Rx duration:\t%10u\n",
					   dtoh32(stats.total_rx_time_mimo),
					   dtoh32(stats.total_rx_time_ocl),
					   dtoh32(stats.total_rx_time_ocl));
			} else {
				printf("  MIMO  Rx duration:\t%10u\n",
				       dtoh32(stats.total_rx_time_mimo));
			}
			printf("  SISO  RX duration:\t%10u\n"
				   "  TX 1-chain duration:\t%10u\n"
				   "  TX 2-chain duration:\t%10u\n"
				   "  TX 3-chain duration:\t%10u\n",
				   dtoh32(stats.total_rx_time_siso),
				   dtoh32(stats.total_tx_time_1chain),
				   dtoh32(stats.total_tx_time_2chain),
				   dtoh32(stats.total_tx_time_3chain));
		}
		break;
		default:
			printf("Skipping uknown %d-byte tag %d\n", taglen, type);
			break;
		}

		printf("\n");
		if (rc)
			break;

		/* Adjust length to account for padding, but don't exceed total len */
		taglen = (ROUNDUP(taglen, 4) > len) ? len : ROUNDUP(taglen, 4);
		len -= taglen;
	}

	if (len && (len < 2 * sizeof(uint16))) {
		fprintf(stderr, "Invalid length remaining %d\n", len);
		rc = BCME_ERROR;
	}

	return (rc);
}

/*
* Function to output heap details for wl memuse command
*/

static int
wl_memuse(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	memuse_info_t *mu;
	wlc_rev_info_t revinfo;

	memset(&revinfo, 0, sizeof(revinfo));
	if ((err = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return err;

	if (revinfo.driverrev && dtoh32(revinfo.driverrev) < DINGO_FW_REV) {
		/* memuse implementation for FW branch version < 9.0 (before Dingo) */
		wl_varstr(wl, cmd, argv);
	}
	else {
		/* memuse implementation for FW branch version > 9.0 (Dingo and above) */
		memset(buf, 0, sizeof(memuse_info_t));
		strcpy(buf, cmd->name);

		if ((err = wlu_get(wl, WLC_GET_VAR, &buf[0], sizeof(memuse_info_t))) < 0) {
			return err;
		}

		mu = (memuse_info_t *)buf;

		if (mu->ver == RTE_MEMUSEINFO_VER && mu->len >= sizeof(memuse_info_t)) {
			printf("Heap Total: %d(%dK)\n", mu->arena_size, KB(mu->arena_size));
			printf("Free: %d(%dK), LWM: %d(%dK)\n",
			       mu->arena_free, KB(mu->arena_free),
			       mu->free_lwm, KB(mu->free_lwm));
			printf("In use: %d(%dK), HWM: %d(%dK)\n",
			       mu->inuse_size, KB(mu->inuse_size),
			       mu->inuse_hwm, KB(mu->inuse_hwm));
			printf("Malloc failure count: %d\n", mu->mf_count);
		} else {
			printf("Heap Total: %d(%dK), Heap Free: %d(%dK)\n",
				mu->arena_size, KB(mu->arena_size),
				mu->arena_free, KB(mu->arena_free));
		}
	}
	return (0);
}

/* this is the batched command packet size. now for remoteWL, we set it to 512 bytes */
#define MEMBLOCK (512 - 32) /* allow 32 bytes for overhead (header, alignment, etc) */

int wl_seq_batch_in_client(bool enable)
{
	batch_in_client = enable;
	return 0;
}

int
wl_seq_start(void *wl, cmd_t *cmd, char **argv)
{
	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if (!batch_in_client) {
		return wlu_iovar_setbuf(wl, "seq_start", NULL, 0, buf, WLC_IOCTL_MAXLEN);
	}
	else {
		if (cmd_batching_mode) {
			printf("calling seq_start() when it's already in batching mode\n");
			clean_up_cmd_list();
			cmd_batching_mode = FALSE;
			return BCME_USAGE_ERROR;
		}
		else {
			cmd_batching_mode = TRUE;
			cmd_pkt_list_num = 0;

			cmd_list.head = NULL;
			cmd_list.tail = NULL;
		}
	}

	return 0;
}

int
wl_seq_stop(void *wl, cmd_t *cmd, char **argv)
{
	char *bufp;
	int ret = 0;
	int seq_list_len;
	int len;
	wl_seq_cmd_pkt_t *next_cmd;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if (!batch_in_client) {
		return wlu_iovar_setbuf(wl, "seq_stop", NULL, 0, buf, WLC_IOCTL_MAXLEN);
	}
	else {
		if (!cmd_batching_mode) {
			printf("calling seq_stop when it's already out of batching mode\n");
			return BCME_USAGE_ERROR;
		}
		cmd_batching_mode = FALSE;

		next_cmd = cmd_list.head;

		/* dump batched commands to the DUT */

		if (next_cmd == NULL) {
			printf("no command batched\n");
			return BCME_USAGE_ERROR;
		}

		ret = wlu_iovar_setbuf(wl, "seq_start", NULL, 0, buf, WLC_IOCTL_MAXLEN);
		if (ret) {
			printf("failed to send seq_start\n");
			goto fail;
		}

		while (next_cmd != NULL) {
			bufp = buf;
			memset(bufp, 0, WLC_IOCTL_MAXLEN);

			strcpy(bufp, "seq_list");
			bufp += (strlen("seq_list") + 1);

			seq_list_len = bufp - buf;

			while ((seq_list_len < MEMBLOCK) && (next_cmd != NULL)) {
				/* note, driver handles the IOCTL buffer 32-bit alignment */
				len = next_cmd->cmd_header.len;
				len += (seq_list_len + sizeof(wl_seq_cmd_ioctl_t));

				if (len < MEMBLOCK) {
					memcpy(bufp, &(next_cmd->cmd_header),
						sizeof(wl_seq_cmd_ioctl_t));
					bufp += sizeof(wl_seq_cmd_ioctl_t);
					memcpy(bufp, next_cmd->data, next_cmd->cmd_header.len);
					bufp += next_cmd->cmd_header.len;
					seq_list_len = len;

					next_cmd = next_cmd->next;
				}
				else
					break;
			}

			ret = wl_set(wl, WLC_SET_VAR, &buf[0], seq_list_len);

			if (ret) {
				printf("failed to send seq_list\n");
				goto fail;
			}
		}

		ret = wlu_iovar_setbuf(wl, "seq_stop", NULL, 0, buf, WLC_IOCTL_MAXLEN);
		if (ret) {
			printf("failed to send seq_stop\n");
		}

	fail:
		clean_up_cmd_list();
		return ret;
	}
}

#ifdef RWL_WIFI
/* Function added to support RWL_WIFI Transport
* Used to find the remote server with proper mac address given by
* the user,this cmd is specific to RWL_WIFIi protocol
*/
static int wl_wifiserver(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	if ((ret = wlu_iovar_set(wl, cmd->name, *argv, strlen(*argv))) < 0) {
		printf("Error finding the remote server  %s\n", argv[0]);
		return ret;
	}
	return ret;
}
#endif // endif

static int
wl_srchmem(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	struct args {
		int reg;
		uint32 ssidlen;
		uint8 ssid[DOT11_MAX_SSID_LEN];
	} x;
	char *endptr;
	uint argc;
	char *iovar;

	UNUSED_PARAMETER(cmd);

	memset(&x, 0, sizeof(x));

	/* save command name */
	iovar = argv[0];
	argv++;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	/* required arg: reg offset */
	if (argc < 1)
		return BCME_USAGE_ERROR;

	x.reg = strtol(argv[0], &endptr, 0);
	if (*endptr != '\0' || x.reg > 15)
		return BCME_USAGE_ERROR;

	if (argc > 2)
		return BCME_USAGE_ERROR;

	if (argc == 2) {
		uint32 len;

		len = strlen(argv[1]);
		if (len > sizeof(x.ssid)) {
			printf("ssid too long\n");
			return BCME_BADARG;
		}
		memcpy(x.ssid, argv[1], len);
		x.ssidlen = len;
	}

	/* issue the get or set ioctl */
	if (argc == 1) {
		x.reg = htod32(x.reg);

		ret = wlu_iovar_getbuf(wl, iovar, &x, sizeof(x), buf, WLC_IOCTL_SMLEN);
		if (ret < 0) {
			printf("get returned error 0x%x\n", ret);
			return (ret);
		}

		wl_hexdump((uchar *)buf, sizeof(x.ssidlen) + sizeof(x.ssid));
	} else {
		x.reg = htod32(x.reg);
		x.ssidlen = htod32(x.ssidlen);

		ret = wlu_iovar_set(wl, iovar, &x, sizeof(x));
		if (ret < 0) {
			printf("set returned error 0x%x\n", ret);
			return (ret);
		}
	}

	return (ret);
}

cntry_name_t cntry_names[] = {

{"AFGHANISTAN",		"AF"},
{"ALBANIA",		"AL"},
{"ALGERIA",		"DZ"},
{"AMERICAN SAMOA",	"AS"},
{"ANDORRA",		"AD"},
{"ANGOLA",		"AO"},
{"ANGUILLA",		"AI"},
{"ANTARCTICA",		"AQ"},
{"ANTIGUA AND BARBUDA",	"AG"},
{"ARGENTINA",		"AR"},
{"ARMENIA",		"AM"},
{"ARUBA",		"AW"},
{"ASCENSION ISLAND",	"AC"},
{"AUSTRALIA",		"AU"},
{"AUSTRIA",		"AT"},
{"AZERBAIJAN",		"AZ"},
{"BAHAMAS",		"BS"},
{"BAHRAIN",		"BH"},
{"BANGLADESH",		"BD"},
{"BARBADOS",		"BB"},
{"BELARUS",		"BY"},
{"BELGIUM",		"BE"},
{"BELIZE",		"BZ"},
{"BENIN",		"BJ"},
{"BERMUDA",		"BM"},
{"BHUTAN",		"BT"},
{"BOLIVIA",		"BO"},
{"BONAIRE SAINT EUSTATIUS AND SABA",		"BQ"},
{"BOSNIA AND HERZEGOVINA",		"BA"},
{"BOTSWANA",		"BW"},
{"BOUVET ISLAND",	"BV"},
{"BRAZIL",		"BR"},
{"BRITISH INDIAN OCEAN TERRITORY",		"IO"},
{"BRUNEI DARUSSALAM",	"BN"},
{"BULGARIA",		"BG"},
{"BURKINA FASO",	"BF"},
{"BURUNDI",		"BI"},
{"CAMBODIA",		"KH"},
{"CAMEROON",		"CM"},
{"CANADA",		"CA"},
{"CAPE VERDE",		"CV"},
{"CAYMAN ISLANDS",	"KY"},
{"CENTRAL AFRICAN REPUBLIC",		"CF"},
{"CHAD",		"TD"},
{"CHILE",		"CL"},
{"CHINA",		"CN"},
{"CHRISTMAS ISLAND",	"CX"},
{"CLIPPERTON ISLAND",	"CP"},
{"COCOS (KEELING) ISLANDS",		"CC"},
{"COLOMBIA",		"CO"},
{"COMOROS",		"KM"},
{"CONGO",		"CG"},
{"CONGO, THE DEMOCRATIC REPUBLIC OF THE",		"CD"},
{"COOK ISLANDS",	"CK"},
{"COSTA RICA",		"CR"},
{"COTE D'IVOIRE",	"CI"},
{"CROATIA",		"HR"},
{"CUBA",		"CU"},
{"CURACAO",		"CW"},
{"CYPRUS",		"CY"},
{"CZECH REPUBLIC",	"CZ"},
{"DENMARK",		"DK"},
{"DJIBOUTI",		"DJ"},
{"DOMINICA",		"DM"},
{"DOMINICAN REPUBLIC",	"DO"},
{"ECUADOR",		"EC"},
{"EGYPT",		"EG"},
{"EL SALVADOR",		"SV"},
{"EQUATORIAL GUINEA",	"GQ"},
{"ERITREA",		"ER"},
{"ESTONIA",		"EE"},
{"ETHIOPIA",		"ET"},
{"FALKLAND ISLANDS (MALVINAS)",		"FK"},
{"FAROE ISLANDS",	"FO"},
{"FIJI",		"FJ"},
{"FINLAND",		"FI"},
{"FRANCE",		"FR"},
{"FRENCH GUIANA",	"GF"},
{"FRENCH POLYNESIA",	"PF"},
{"FRENCH SOUTHERN TERRITORIES",		"TF"},
{"GABON",		"GA"},
{"GAMBIA",		"GM"},
{"GEORGIA",		"GE"},
{"GERMANY",		"DE"},
{"GHANA",		"GH"},
{"GIBRALTAR",		"GI"},
{"GREECE",		"GR"},
{"GREENLAND",		"GL"},
{"GRENADA",		"GD"},
{"GUADELOUPE",		"GP"},
{"GUAM",		"GU"},
{"GUATEMALA",		"GT"},
{"GUERNSEY",		"GG"},
{"GUINEA",		"GN"},
{"GUINEA-BISSAU",	"GW"},
{"GUYANA",		"GY"},
{"HAITI",		"HT"},
{"HEARD ISLAND AND MCDONALD ISLANDS",		"HM"},
{"HOLY SEE (VATICAN CITY STATE)",		"VA"},
{"HONDURAS",		"HN"},
{"HONG KONG",		"HK"},
{"HUNGARY",		"HU"},
{"ICELAND",		"IS"},
{"INDIA",		"IN"},
{"INDONESIA",		"ID"},
{"IRAN, ISLAMIC REPUBLIC OF",		"IR"},
{"IRAQ",		"IQ"},
{"IRELAND",		"IE"},
{"ISRAEL",		"IL"},
{"ITALY",		"IT"},
{"JAMAICA",		"JM"},
{"JAPAN",		"JP"},
{"JERSEY",		"JE"},
{"JORDAN",		"JO"},
{"KAZAKHSTAN",		"KZ"},
{"KENYA",		"KE"},
{"KIRIBATI",		"KI"},
{"KOREA, DEMOCRATIC PEOPLE'S REPUBLIC OF",		"KP"},
{"KOREA, REPUBLIC OF",	"KR"},
{"KUWAIT",		"KW"},
{"KYRGYZSTAN",		"KG"},
{"LAO PEOPLE'S DEMOCRATIC REPUBLIC",		"LA"},
{"LATVIA",		"LV"},
{"LEBANON",		"LB"},
{"LESOTHO",		"LS"},
{"LIBERIA",		"LR"},
{"LIBYAN ARAB JAMAHIRIYA",		"LY"},
{"LIECHTENSTEIN",	"LI"},
{"LITHUANIA",		"LT"},
{"LUXEMBOURG",		"LU"},
{"MACAO",		"MO"},
{"MACEDONIA, THE FORMER YUGOSLAV REPUBLIC OF",		"MK"},
{"MADAGASCAR",		"MG"},
{"MALAWI",		"MW"},
{"MALAYSIA",		"MY"},
{"MALDIVES",		"MV"},
{"MALI",		"ML"},
{"MALTA",		"MT"},
{"MAN, ISLE OF",	"IM"},
{"MARSHALL ISLANDS",	"MH"},
{"MARTINIQUE",		"MQ"},
{"MAURITANIA",		"MR"},
{"MAURITIUS",		"MU"},
{"MAYOTTE",		"YT"},
{"MEXICO",		"MX"},
{"MICRONESIA, FEDERATED STATES OF",		"FM"},
{"MOLDOVA, REPUBLIC OF",		"MD"},
{"MONACO",		"MC"},
{"MONGOLIA",		"MN"},
{"MONTENEGRO",		"ME"},
{"MONTSERRAT",		"MS"},
{"MOROCCO",		"MA"},
{"MOZAMBIQUE",		"MZ"},
{"MYANMAR",		"MM"},
{"NAMIBIA",		"NA"},
{"NAURU",		"NR"},
{"NEPAL",		"NP"},
{"NETHERLANDS",		"NL"},
{"NETHERLANDS ANTILLES",		"AN"},
{"NEW CALEDONIA",	"NC"},
{"NEW ZEALAND",		"NZ"},
{"NICARAGUA",		"NI"},
{"NIGER",		"NE"},
{"NIGERIA",		"NG"},
{"NIUE",		"NU"},
{"NORFOLK ISLAND",		"NF"},
{"NORTHERN MARIANA ISLANDS",		"MP"},
{"NORWAY",		"NO"},
{"OMAN",		"OM"},
{"PAKISTAN",		"PK"},
{"PALAU",		"PW"},
{"PALESTINIAN TERRITORY, OCCUPIED",		"PS"},
{"PANAMA",		"PA"},
{"PAPUA NEW GUINEA",	"PG"},
{"PARAGUAY",		"PY"},
{"PERU",		"PE"},
{"PHILIPPINES",		"PH"},
{"PITCAIRN",		"PN"},
{"POLAND",		"PL"},
{"PORTUGAL",		"PT"},
{"PUERTO RICO",		"PR"},
{"QATAR",		"QA"},
{"Q1",		"Q1"},
{"REUNION",		"RE"},
{"ROMANIA",		"RO"},
{"RUSSIAN FEDERATION",	"RU"},
{"RWANDA",		"RW"},
{"SAINT HELENA",	"SH"},
{"SAINT KITTS AND NEVIS",		"KN"},
{"SAINT LUCIA",		"LC"},
{"SAINT PIERRE AND MIQUELON",		"PM"},
{"SAINT VINCENT AND THE GRENADINES",		"VC"},
{"SAMOA",		"WS"},
{"SAN MARINO",		"SM"},
{"SAINT MARTIN",	"MF"},
{"SAO TOME AND PRINCIPE",		"ST"},
{"SAUDI ARABIA",	"SA"},
{"SENEGAL",		"SN"},
{"SERBIA",		"RS"},
{"SEYCHELLES",		"SC"},
{"SIERRA LEONE",	"SL"},
{"SINGAPORE",		"SG"},
{"SLOVAKIA",		"SK"},
{"SLOVENIA",		"SI"},
{"SOLOMON ISLANDS",	"SB"},
{"SOMALIA",		"SO"},
{"SOUTH AFRICA",	"ZA"},
{"SOUTH GEORGIA AND THE SOUTH SANDWICH ISLANDS",		"GS"},
{"SPAIN",		"ES"},
{"SRI LANKA",		"LK"},
{"SUDAN",		"SD"},
{"SURINAME",		"SR"},
{"SVALBARD AND JAN MAYEN",		"SJ"},
{"SWAZILAND",		"SZ"},
{"SWEDEN",		"SE"},
{"SWITZERLAND",		"CH"},
{"SYRIAN ARAB REPUBLIC",		"SY"},
{"TAIWAN, REPUBLIC OF CHINA",		"TW"},
{"TAJIKISTAN",		"TJ"},
{"TANZANIA, UNITED REPUBLIC OF",		"TZ"},
{"THAILAND",		"TH"},
{"TIMOR-LESTE (EAST TIMOR)",		"TL"},
{"TOGO",		"TG"},
{"TOKELAU",		"TK"},
{"TONGA",		"TO"},
{"TRINIDAD AND TOBAGO",	"TT"},
{"TRISTAN DA CUNHA",	"TA"},
{"TUNISIA",		"TN"},
{"TURKEY",		"TR"},
{"TURKMENISTAN",	"TM"},
{"TURKS AND CAICOS ISLANDS",		"TC"},
{"TUVALU",		"TV"},
{"UGANDA",		"UG"},
{"UKRAINE",		"UA"},
{"UNITED ARAB EMIRATES",		"AE"},
{"UNITED KINGDOM",	"GB"},
{"UNITED STATES",	"US"},
{"UNITED STATES MINOR OUTLYING ISLANDS",		"UM"},
{"URUGUAY",		"UY"},
{"UZBEKISTAN",		"UZ"},
{"VANUATU",		"VU"},
{"VENEZUELA",		"VE"},
{"VIET NAM",		"VN"},
{"VIRGIN ISLANDS, BRITISH",		"VG"},
{"VIRGIN ISLANDS, U.S.",		"VI"},
{"WALLIS AND FUTUNA",	"WF"},
{"WESTERN SAHARA",	"EH"},
{"YEMEN",		"YE"},
{"YUGOSLAVIA",		"YU"},
{"ZAMBIA",		"ZM"},
{"ZIMBABWE",		"ZW"},
{"RADAR CHANNELS",	"RDR"},
{"ALL CHANNELS",	"ALL"},
{NULL,			NULL}
};

static int
wl_ptk_start(void *wl, cmd_t *cmd, char **argv)
{
	const char *str;
	int buf_len;
	int str_len;
	int rc;
	int count;
	wl_ptk_start_iov_t *ptk_start_args;

	UNUSED_PARAMETER(cmd);

	count = ARGCNT(argv);

	/*
	** Set the attributes.
	*/
	if (count < 5) {
		fprintf(stderr, "<sec-type> <peer_addr> <pmk> <role> expected\n");
		rc = -1;
		goto exit;
	}

	str = "ptk_start";
	str_len = strlen(str);
	strncpy(buf, str, str_len);
	buf[ str_len ] = '\0';

	ptk_start_args = (wl_ptk_start_iov_t *) (buf + str_len + 1);
	ptk_start_args->sec_type = strtoul(*(argv + 1), NULL, 0); /* WL_PTK_START_SEC_TYPE_PMK */
	if (!wl_ether_atoe(*(argv + 2), &ptk_start_args->peer_addr)) {
		fprintf(stderr, "awdl ranging config mac addr err\n");
		rc = -1;
		goto exit;
	}
	ptk_start_args->tlvs[0].id = WL_PTK_START_TLV_PMK;
	ptk_start_args->tlvs[0].len = strlen(*(argv+3));
	strncpy((char*)ptk_start_args->tlvs[0].data, *(argv+3), ptk_start_args->tlvs[0].len);
	printf("PMK[len = %d]: %s\n", ptk_start_args->tlvs[0].len,
	(char*)ptk_start_args->tlvs[0].data);
	ptk_start_args->role = strtoul(*(argv + 4), NULL, 0);   /* WL_PTK_START_SEC_TYPE_PMK */

	buf_len = str_len + 1 + sizeof(wl_ptk_start_iov_t) +  ptk_start_args->tlvs[0].len;
	rc = wlu_set(wl,
		WLC_SET_VAR,
		buf,
		buf_len);
exit:
	return (rc);
}

static void
wl_print_txbf_mcsset(char *mcsset, char *prefix)
{
	int i;

	printf("%s MCS : [ ", prefix);
	for (i = 0; i < (TXBF_RATE_MCS_ALL * 8); i++)
		if (isset(mcsset, i))
			printf("%d ", i);
	printf("]\n");
}

static void
wl_print_txbf_vhtmcsset(uint16 *mcsset, char *prefix)
{
	int i, j;

	for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
		if (mcsset[i]) {
			if (i == 0)
				printf("%s VHT : ", prefix);
			else
				printf("        : ");
			for (j = 0; j <= 11; j++)
				if (isbitset(mcsset[i], j))
					printf("%dx%d ", j, i + 1);
			printf("\n");
		} else {
			break;
		}
	}
}

static int
wl_assertlog(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err;
	int i;
	char *log_p = NULL;
	assertlog_results_t *results;
	void *ptr = NULL;

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc > 1)
		return BCME_USAGE_ERROR;

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;

	results = (assertlog_results_t *)buf;

	printf("get external assert logs: %d\n", results->num);
	if (!results->num)
		return 0;

	if (results->version != ASSERTLOG_CUR_VER) {
		printf("Version mismatch: version = 0x%x, expected 0x%x\n",
			results->version, ASSERTLOG_CUR_VER);
		return 0;
	}

	log_p = (char *)&results->logs[0];

	printf("id: \ttime(ms) \tstring\n");
	for (i = 0; i < (int)results->num; i++) {
		printf("%d: \t%d \t%s", i, ((assert_record_t *)log_p)->time,
			((assert_record_t *)log_p)->str);
		log_p += results->record_len;
	}

	return 0;
}

static const char *
cca_level(int score, int med, int hi)
{
	if (score < med)
		return ("Low");
	if (score >= med && score < hi)
		return ("Medium");
	if (score >= hi)
		return ("High");
	return NULL;
}

static const char *cca_errors[] = {
	"No error",
	"Preferred band",
	"Dwell Duration too low",
	"Channel prefs",
	"Interference too high",
	"Only 1 channel inoput"
};
static void
free_cca_array(cca_congest_channel_req_t **favg, int favg_chan_elts)
{
	int i;

	if (favg == NULL)
		{
		return;
		}
	for (i = 0; i < favg_chan_elts; i++) {
		if (favg[i] != NULL) {
			free(favg[i]);
			favg[i] = NULL;
		}
	}
	if (favg != NULL) {
		free(favg);
		favg = NULL;
	}
}
static int
wl_cca_get_stats(void *wl, cmd_t *cmd, char **argv)
{
	cca_congest_channel_req_t *results;
	cca_congest_channel_req_t req;
	cca_congest_t *chptr;
	cca_congest_channel_req_t **avg = NULL, **new_avg = NULL; /* Max num of channels */
	void *ptr = NULL;
	char *param, *val_p;
	int base, limit, i, channel, err = 0;
	int ibss_per, obss_per, inter_per, val;
	const char *ibss_lvl = NULL;
	const char *obss_lvl = NULL;
	const char *inter_lvl = NULL;
	int tmp_channel;
	chanspec_t new_chanspec, cur_chanspec;
	bool do_individ = FALSE;
	bool do_analyze = TRUE;
	bool curband = FALSE;
	int avg_chan_idx = 0, avg_chan_elts = 40;
	uint32 flags;
	int j;

	req.num_secs = 10;
	tmp_channel = 0xff;

	argv++;

	/* Parse args */
	while ((param = *argv++) != NULL) {
		if (stricmp(param, "-n") == 0) {
			do_analyze = FALSE;
			continue;
		}
		if (stricmp(param, "-i") == 0) {
			do_individ = TRUE;
			continue;
		}
		if (stricmp(param, "-curband") == 0) {
			curband = TRUE;
			continue;
		}

		if ((val_p = *argv++) == NULL) {
			printf("Need value following %s\n", param);
			return BCME_USAGE_ERROR;
		}
		if (stricmp(param, "-c") == 0) {
			tmp_channel = (int)strtoul(val_p, NULL, 0);
		}

		if (stricmp(param, "-cs") == 0) {
			if ((new_chanspec = wf_chspec_aton(val_p)))
				tmp_channel = wf_chspec_ctlchan(new_chanspec);
		}

		if (stricmp(param, "-s") == 0) {
			req.num_secs = (uint16)strtoul(val_p, NULL, 0);
			if (req.num_secs == 0 || req.num_secs > MAX_CCA_SECS) {
				printf("%d: Num of seconds must be <= %d\n",
					req.num_secs, MAX_CCA_SECS);
				return BCME_USAGE_ERROR;
			}
		}
	}

	if (tmp_channel == 0) {
		/* do all channels */
		base = 1; limit = MAXCHANNEL;
	} else {
		/* Use current channel as default if none specified */
		if (tmp_channel == 0xff) {
			if ((err = wlu_iovar_getint(wl, "chanspec", (int*)&val)) < 0) {
				printf("CCA: Can't get currrent chanspec\n");
				return err;
			}
			cur_chanspec = wl_chspec32_from_driver(val);
			tmp_channel = wf_chspec_ctlchan(cur_chanspec);
			printf("Using channel %d\n", tmp_channel);
		}
		base = limit = tmp_channel;
	}

	avg = (cca_congest_channel_req_t **)
		calloc(1, sizeof(cca_congest_channel_req_t*) * avg_chan_elts);
	if (avg == NULL) {
		printf("unable to allocate memory\n");
		return BCME_NOMEM;
	}

	req.num_secs = htod16(req.num_secs);

	for (channel = base; channel <= limit; channel++) {

		/* Get stats for each channel */
		req.chanspec = CH20MHZ_CHSPEC(channel);
		req.chanspec = wl_chspec_to_driver(req.chanspec);
		if ((err = wlu_var_getbuf(wl, cmd->name, &req, sizeof(req), &ptr)) < 0) {
		goto func_exit;
		}

		results = (cca_congest_channel_req_t *)ptr;
		results->chanspec = wl_chspec_from_driver(results->chanspec);
		if (results->chanspec == 0 || results->num_secs == 0)
			continue;

		if (results->num_secs > MAX_CCA_SECS) {
			printf("Bogus num of seconds returned %d\n", results->num_secs);
			err = -1;
			goto func_exit;
		}

		/* Summarize and save summary for this channel */
		if (avg_chan_idx >= avg_chan_elts) {
			new_avg = (cca_congest_channel_req_t **)calloc
			(1, sizeof(cca_congest_channel_req_t*) * (avg_chan_elts + 10));

			if (new_avg == NULL) {
				printf("unable to allocate memory\n");
				err = BCME_NOMEM;
				goto func_exit;
			}
			memcpy(new_avg, avg, avg_chan_elts);
			free_cca_array(avg, avg_chan_elts);
			avg_chan_elts += 10;
			avg = new_avg;
			new_avg = NULL;
		}

		avg[avg_chan_idx] = (cca_congest_channel_req_t *)
			malloc(sizeof(cca_congest_channel_req_t));
		if (avg[avg_chan_idx] == NULL) {
			printf("unable to allocate memory\n");
			err = BCME_NOMEM;
			goto func_exit;
		}
		cca_per_chan_summary(results, avg[avg_chan_idx], 1);
		if (avg[avg_chan_idx]->num_secs)
			avg_chan_idx++;

		/* printf stats for each second of each channel */
		if (do_individ) {
			if (channel == base)
				printf("chan dur      ibss           obss"
					"           interf       time\n");
			for (i = 0; i < results->num_secs; i++) {
				chptr = &results->secs[i];
				if (chptr->duration) {
					/* Percentages */
					ibss_per = chptr->congest_ibss * 100 /chptr->duration;
					obss_per = chptr->congest_obss * 100 /chptr->duration;
					inter_per = chptr->interference * 100 /chptr->duration;
					/* Levels */
					ibss_lvl = cca_level(ibss_per, IBSS_MED, IBSS_HI);
					obss_lvl = cca_level(obss_per, OBSS_MED, OBSS_HI);
					inter_lvl = cca_level(inter_per, INTERFER_MED, INTERFER_HI);

				printf("%-3u %4d %4u %2d%% %-6s %4u %2d%% %-6s %4u %2d%% %-6s %d\n",
					CHSPEC_CHANNEL(results->chanspec),
					chptr->duration,
					chptr->congest_ibss, ibss_per, ibss_lvl,
					chptr->congest_obss, obss_per, obss_lvl,
					chptr->interference, inter_per, inter_lvl,
					chptr->timestamp);
				}
			}
		}
	}

	/* Print summary stats of each channel */
	printf("Summaries:\n");
	printf("chan dur      ibss           obss             interf     num seconds\n");
	for (j = 0; j < avg_chan_idx; j++) {
		/* Percentages */
		ibss_per = avg[j]->secs[0].congest_ibss;
		obss_per = avg[j]->secs[0].congest_obss;
		inter_per = avg[j]->secs[0].interference;
		/* Levels */
		ibss_lvl = cca_level(ibss_per, IBSS_MED, IBSS_HI);
		obss_lvl = cca_level(obss_per, OBSS_MED, OBSS_HI);
		inter_lvl = cca_level(inter_per, INTERFER_MED, INTERFER_HI);

		if (avg[j]->num_secs) {
			printf("%-3u %4d %4s %2d%% %-6s %4s %2d%% %-6s %4s %2d%% %-6s %d\n",
				CHSPEC_CHANNEL(avg[j]->chanspec),
				avg[j]->secs[0].duration,
				"", avg[j]->secs[0].congest_ibss, ibss_lvl,
				"", avg[j]->secs[0].congest_obss, obss_lvl,
				"", avg[j]->secs[0].interference, inter_lvl,
				avg[j]->num_secs);
		}
	}

	if (!do_analyze) {
		goto func_exit;
	}

	if ((err = wlu_iovar_getint(wl, "chanspec", (int *)&val)) < 0) {
		printf("CCA: Can't get currrent chanspec\n");
		goto func_exit;
	}
	cur_chanspec = wl_chspec32_from_driver(val);
	flags = 0;
	if (curband) {
		if (CHSPEC_IS5G(cur_chanspec))
			flags |= CCA_FLAG_5G_ONLY;
		if (CHSPEC_IS2G(cur_chanspec))
			flags |= CCA_FLAG_2G_ONLY;
	}

	if ((err = cca_analyze(avg,  avg_chan_idx, flags, &new_chanspec)) != 0) {
		if (err > 0) {
			printf("Cannot find a good channel due to: %s\n", cca_errors[err]);
			err = BCME_ERROR;
		}
		goto func_exit;
	}
	printf("Recommended channel: %d\n", wf_chspec_ctlchan(new_chanspec));

	func_exit:
	free_cca_array(avg, avg_chan_elts);

	return err;
}
static int
wl_cca_stats_mesh(void *wl, cmd_t *cmd, char **argv)
{
	cca_mesh_req_t *res;
	cca_mesh_req_t req;
	cca_congest_t *chptr;
	cca_congest_channel_req_t *results;
	cca_congest_channel_req_t *request = &req.data;
	void *ptr = NULL;
	char *param, *val_p;
	int base, limit, i, channel, err = 0;
	int txnode_per, rxnode_per, xxobss_per, inter_per, val;
	int tmp_channel;
	chanspec_t new_chanspec, cur_chanspec;

	/*
	 * default values
	 */
	req.cmd_type = CCA_MESH_GET;
	request->num_secs = 10;
	tmp_channel = 0xff;

	argv++;

	/* Parse args */
	while ((param = *argv++) != NULL) {
		if (stricmp(param, "-h") == 0) {
			return BCME_USAGE_ERROR;
		}
		if (stricmp(param, "-da") == 0) {
			req.cmd_type = CCA_MESH_DEL_ALL;
			err = wlu_var_setbuf(wl, cmd->name, &req, sizeof(req));
			return err;
		}
		if (stricmp(param, "enable") == 0) {
			req.cmd_type = CCA_MESH_ENABLE;
			err = wlu_var_setbuf(wl, cmd->name, &req, sizeof(req));
			return err;
		}
		if (stricmp(param, "disable") == 0) {
			req.cmd_type = CCA_MESH_DISABLE;
			err = wlu_var_setbuf(wl, cmd->name, &req, sizeof(req));
			return err;
		}
		if (stricmp(param, "mac_table") == 0) {
			req.cmd_type = CCA_MESH_DUMP;
			err = wlu_var_getbuf(wl, cmd->name, &req, sizeof(req), &ptr);
			if (!err) {
				res = (cca_mesh_req_t *)ptr;
				struct ether_addr *ptr_ea = (struct ether_addr *)&res->data;
				for (i = 0; i < CCA_MESH_MAX; i++) {
					printf("MAC %d: %s\n", i, wl_ether_etoa(ptr_ea));
					ptr_ea++;
				}
			}
			return err;
		}
		if ((val_p = *argv++) == NULL) {
			printf("Need value following %s\n", param);
			return BCME_USAGE_ERROR;
		}
		if (stricmp(param, "-m") == 0) {
			req.cmd_type = CCA_MESH_SET;
			if (!wl_ether_atoe(val_p, &req.mac)) {
				return BCME_USAGE_ERROR;
			}
			err = wlu_var_setbuf(wl, cmd->name, &req, sizeof(req));
			return err;
		}
		if (stricmp(param, "-d") == 0) {
			req.cmd_type = CCA_MESH_DEL;
			if (!wl_ether_atoe(val_p, &req.mac)) {
				return BCME_USAGE_ERROR;
			}
			err = wlu_var_setbuf(wl, cmd->name, &req, sizeof(req));
			return err;
		}
		if (stricmp(param, "-c") == 0) {
			tmp_channel = (int)strtoul(val_p, NULL, 0);
		}

		if (stricmp(param, "-cs") == 0) {
			if ((new_chanspec = wf_chspec_aton(val_p)))
				tmp_channel = wf_chspec_ctlchan(new_chanspec);
		}

		if (stricmp(param, "-s") == 0) {
			request->num_secs = (int)strtoul(val_p, NULL, 0);
			if (request->num_secs == 0 || request->num_secs > MAX_CCA_SECS) {
				printf("%d: Num of seconds must be <= %d\n",
					request->num_secs, MAX_CCA_SECS);
				return BCME_USAGE_ERROR;
			}
		}
	}

	if (tmp_channel == 0) {
		/* do all channels */
		base = 1; limit = MAXCHANNEL;
	} else {
		/* Use current channel as default if none specified */
		if (tmp_channel == 0xff) {
			if ((err = wlu_iovar_getint(wl, "chanspec", (int*)&val)) < 0) {
				printf("CCA: Can't get currrent chanspec\n");
				return err;
			}
			cur_chanspec = wl_chspec32_from_driver(val);
			tmp_channel = wf_chspec_ctlchan(cur_chanspec);
			printf("Using channel %d\n", tmp_channel);
		}
		base = limit = tmp_channel;
	}

	for (channel = base; channel <= limit; channel++) {

		/* Get stats for each channel */
		request->chanspec = CH20MHZ_CHSPEC(channel);
		request->chanspec = wl_chspec_to_driver(request->chanspec);
		if ((err = wlu_var_getbuf(wl, cmd->name, &req, sizeof(req), &ptr)) < 0) {
			goto func_exit;
		}
		res = (cca_mesh_req_t *)ptr;
		results = (cca_congest_channel_req_t *)(&res->data);
		results->chanspec = wl_chspec_from_driver(results->chanspec);
		if (results->chanspec == 0 || results->num_secs == 0)
			continue;

		if (results->num_secs > MAX_CCA_SECS) {
			printf("Bogus num of seconds returned %d\n", results->num_secs);
			err = -1;
			goto func_exit;
		}

		printf("CCA_CAP MAC count = %d\n", res->count);
		printf("CCA_STATS_MESH_VER = %d\n", res->ver);
		if (channel == base) {
			char buf[512], *pbuf;
			int i;
			pbuf = buf;
			pbuf += sprintf(pbuf, "chan dur    ");
			for (i = 0; i < CCA_MESH_MAX; i++) {
				pbuf +=	sprintf(pbuf, "tx%d      rx%d     ", i, i);
			}
			pbuf += sprintf(pbuf, "obss   interf  time\n");
			fputs(buf, stdout);
		}
		for (i = 0; i < results->num_secs; i++) {
			int j;
			chptr = &results->secs[i];
			if (chptr->duration) {
				printf("%-3u %4d ", CHSPEC_CHANNEL(results->chanspec),
						chptr->duration);
				for (j = 0; j < CCA_MESH_MAX; j++) {
					/* Percentages */
					txnode_per = (chptr->txnode[j] * 100)
						/chptr->duration;
					rxnode_per = (chptr->rxnode[j] * 100)
						/chptr->duration;

					printf("%4u %2d%% %4u %2d%%",
							chptr->txnode[j], txnode_per,
							chptr->rxnode[j], rxnode_per);
				}
				xxobss_per = (chptr->xxobss * 100) /chptr->duration;
				inter_per = (chptr->interference * 100) /chptr->duration;
				printf("%4u %2d%% %4u %2d%% %d\n",
						chptr->xxobss, xxobss_per,
						chptr->interference, inter_per,
						chptr->timestamp);
			}
		}

	}

	func_exit:

	return err;
}

static int
wl_txdelay_params(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	txdelay_params_t param;
	int argc;

	argv++;

	if (*argv == NULL) {
		/* get current txdelay params */
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) &param,
			(sizeof(txdelay_params_t)))) < 0)
			return (err);

		printf("Txdelay params: ratio[%d] cnt[%d] period[%d] tune[%d]\n",
			param.ratio, param.cnt,	param.period, param.tune);
	}
	else {
		char *endptr;
		/* Validate num of entries */
		for (argc = 0; argv[argc]; argc++);
		if (argc != 4)
			return BCME_USAGE_ERROR;

		argc = 0;
		param.ratio = strtol(argv[argc], &endptr, 0);
		argc++;
		param.cnt = strtol(argv[argc], &endptr, 0);
		argc++;
		param.period = strtol(argv[argc], &endptr, 0);
		argc++;
		param.tune = strtol(argv[argc], &endptr, 0);

		/* Set txdelay params */
		err = wlu_iovar_set(wl, cmd->name, (void *) &param,
			(sizeof(txdelay_params_t)));
	}
	return (err);
}

static int
wl_intfer_params(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_intfer_params_t param;
	int		argc;
	char *endptr = NULL;

	argv++;

	if (*argv == NULL) {
		/* get current txdelay params */
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) &param,
				(sizeof(wl_intfer_params_t)))) < 0) {
			goto error;
		}

		if (param.version != INTFER_VERSION) {
			printf("Interference params structure version (%d) is not the "
					"version (%d) supported by this tool",
					INTFER_VERSION, param.version);
			err = BCME_USAGE_ERROR;
		} else {
			printf("Intference params: period[%x] cnt[%x] txfail_thresh[%x]"
					" tcptxfail_thresh[%x]\n", param.period, param.cnt,
					param.txfail_thresh, param.tcptxfail_thresh);
		}
	} else {
		/* Validate num of entries */
		err = BCME_USAGE_ERROR;

		for (argc = 0; argv[argc]; argc++);
		if (argc != 4) {
			goto error;
		}

		param.period = (uint8)strtol(argv[0], &endptr, 0);
		if (*endptr != '\0') {
			goto error;
		}

		param.cnt = (uint8)strtol(argv[1], &endptr, 0);
		if (*endptr != '\0') {
			goto error;
		}

		param.txfail_thresh = (uint8)strtol(argv[2], &endptr, 0);
		if (*endptr != '\0') {
			goto error;
		}

		param.tcptxfail_thresh = (uint8)strtol(argv[3], &endptr, 0);
		if (*endptr != '\0') {
			goto error;
		}

		/* Set intfer params */
		param.version = INTFER_VERSION;
		err = wlu_iovar_set(wl, cmd->name, (void *) &param,
				(sizeof(wl_intfer_params_t)));
	}
error:
	return (err);
}

static void
dlystat_dump(txdelay_stats_t *txdelay_stats)
{
	const char *const aci_names[] = { "AC_BE", "AC_BK", "AC_VI", "AC_VO"};
	uint32 i, last, s;
	uint ac;
	uint32 sum;
	uint32 max_val, total = 0;
	scb_delay_stats_t *delay_stats;
	struct ether_addr *ea;
	uint32 dlystat_cnt = txdelay_stats->scb_cnt;

	if (txdelay_stats->full_result == TXDELAY_STATS_PARTIAL_RESULT) {
		printf("\tThis is partial results for SCBs\n"
				"\tcaused by insuffient IOCTL buffer\n"
				"\tIf you want to get full SCBs\n"
				"\tPlease use iteration for each of specific mac_address\n"
				"\twhich can be got from other iovars such as assoclist\n");
	}

	for (s = 0; s < dlystat_cnt; s++) {
		ea = &txdelay_stats->scb_delay_stats[s].ea;
		printf("[SCB%d]\n\t%s\n", s, wl_ether_etoa(ea));
		/* Report Latency and packet loss statistics (for each AC) */
		for (ac = 0; ac < AC_COUNT; ac++) {

			delay_stats = txdelay_stats->scb_delay_stats[s].dlystats + ac;
			for (i = 0, total = 0, sum = 0; i < SCB_RETRY_SHORT_DEF; i++) {
				total += delay_stats->txmpdu_cnt[i];
				sum += delay_stats->delay_sum[i];
			}
			if (total) {
				printf("%s:  PLoss %d/%d (%d%%)\n",
						aci_names[ac], delay_stats->txmpdu_lost, total,
						(delay_stats->txmpdu_lost * 100) / total);
				printf("\tDelay stats in usec (avg/min/max): %d %d %d\n",
						CEIL(sum, total), delay_stats->delay_min,
						delay_stats->delay_max);
				printf("\tTxcnt Hist    :");
				for (i = 0; i < SCB_RETRY_SHORT_DEF; i++) {
					printf("  %d", delay_stats->txmpdu_cnt[i]);
				}
				printf("\n\tDelay vs Txcnt:");
				for (i = 0; i < SCB_RETRY_SHORT_DEF; i++) {
					if (delay_stats->txmpdu_cnt[i])
						printf("  %d", delay_stats->delay_sum[i] /
								delay_stats->txmpdu_cnt[i]);
					else
						printf("  -1");
				}
				printf("\n");

				for (i = 0, max_val = 0, last = 0; i < WLPKTDLY_HIST_NBINS; i++) {
					max_val += delay_stats->delay_hist[i];
					if (delay_stats->delay_hist[i]) last = i;
				}
				last = 8 * (last/8 + 1) - 1;
				if (max_val) {
					printf("\tDelay Hist \n\t\t:");
					for (i = 0; i <= last; i++) {
						printf(" %d(%d%%)", delay_stats->
								delay_hist[i],
								(delay_stats->delay_hist[i] * 100)
								/ max_val);
						if ((i % 8) == 7 && i != last)
							printf("\n\t\t:");
					}
				}
				printf("\n");
			}
		}
		printf("\n");
	}
}

static int
wl_dlystats(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	txdelay_stats_t *dlystats_param;
	txdelay_stats_t *delay_stats;

	dlystats_param = (txdelay_stats_t *)malloc(sizeof(*dlystats_param));

	if (dlystats_param == NULL) {
		return BCME_NOMEM;
	}

	memset(dlystats_param, 0, sizeof(*dlystats_param));
	dlystats_param->version = TXDELAY_STATS_VERSION;

	if (*++argv) {
		if (!wl_ether_atoe(*argv, &dlystats_param->scb_delay_stats[0].ea)) {
			ret = BCME_USAGE_ERROR;
			goto EXIT;
		}
		dlystats_param->scb_cnt = 1;
	} else {
		dlystats_param->scb_cnt = MAX_TXDELAY_STATS_SCBS;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, dlystats_param, sizeof(*dlystats_param),
			buf, WLC_IOCTL_MAXLEN);

	if (ret) {
		if (ret == BCME_VERSION) {
			printf("txdelay_stats_t version is mismatched\n");
		}
		goto EXIT;
	}

	delay_stats = (txdelay_stats_t *)buf;
	dlystat_dump(delay_stats);

EXIT:
	free(dlystats_param);
	return ret;
}

static int
wl_dlystats_clear(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;

	UNUSED_PARAMETER(argv);

	err = wlu_iovar_set(wl, cmd->name, buf, 0);

	return err;
}

static int
wl_rpmt(void *wl, cmd_t *cmd, char **argv)
{
	int count;
	int len;
	char *endptr;
	uint32 val;

	argv ++;

	count = ARGCNT(argv);
	if (count != 2) {
		return BCME_USAGE_ERROR;
	}

	strcpy(buf, cmd->name);
	len = strlen(buf) + 1;

	val = htod32(strtoul(argv[0], &endptr, 0));
	memcpy(&buf[len], &val, sizeof(uint32));
	len += sizeof(uint32);
	val = htod32(strtoul(argv[1], &endptr, 0));
	memcpy(&buf[len], &val, sizeof(uint32));
	len += sizeof(uint32);

	return wlu_set(wl, WLC_SET_VAR, buf, len);
}

static int
wl_tsf(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "tsf";
	struct tsf {
		uint32 low;
		uint32 high;
	} tsf_buf;
	char *endptr;
	int err;

	UNUSED_PARAMETER(cmd);

	/* toss the command name */
	argv++;

	if (*argv == NULL) {
		/* get */
		err = wlu_iovar_get(wl, cmdname, &tsf_buf, sizeof(tsf_buf));
		if (err)
			return err;
		printf("0x%08X 0x%08X\n", htod32(tsf_buf.high), htod32(tsf_buf.low));
	} else {
		/* set */
		if (argv[1] == NULL)
			return BCME_USAGE_ERROR;

		tsf_buf.high = (uint32)strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "%s: %s: error parsing \"%s\" as an integer\n",
			        wlu_av0, cmdname, *argv);
			return BCME_USAGE_ERROR;
		}

		argv++;
		tsf_buf.low = (uint32)strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "%s: %s: error parsing \"%s\" as an integer\n",
			        wlu_av0, cmdname, *argv);
			return BCME_USAGE_ERROR;
		}

		tsf_buf.low = dtoh32(tsf_buf.low);
		tsf_buf.high = dtoh32(tsf_buf.high);

		err = wlu_iovar_set(wl, cmdname, &tsf_buf, sizeof(tsf_buf));
		if (err)
			return err;
	}

	return err;
}

static int wl_event_log_set_init(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "event_log_set_init";
	wl_el_set_params_t pars;
	int argc;
	int err;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 3) {
		return BCME_USAGE_ERROR;
	}

	memset(&pars, 0, sizeof(wl_el_set_params_t));
	pars.set = atoi(argv[1]);
	pars.size = htod32(atoi(argv[2]));

	err = wlu_iovar_set(wl, cmdname, &pars, sizeof(wl_el_set_params_t));

	return err;
}

static int wl_event_log_set_expand(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "event_log_set_expand";
	wl_el_set_params_t pars;
	int argc;
	int err;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 3) {
		return BCME_USAGE_ERROR;
	}

	memset(&pars, 0, sizeof(wl_el_set_params_t));
	pars.set = atoi(argv[1]);
	pars.size = htod32(atoi(argv[2]));

	err = wlu_iovar_set(wl, cmdname, &pars, sizeof(wl_el_set_params_t));

	return err;
}

static int wl_event_log_set_shrink(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "event_log_set_shrink";
	wl_el_set_params_t pars;
	int argc;
	int err;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if ((argc < 2) || (argc > 3)) {
		return BCME_USAGE_ERROR;
	}

	memset(&pars, 0, sizeof(wl_el_set_params_t));
	pars.set = atoi(argv[1]);
	if (argc == 3)
		pars.size = htod32(atoi(argv[2]));

	err = wlu_iovar_set(wl, cmdname, &pars, sizeof(wl_el_set_params_t));

	return err;
}

static int wl_event_log_tag_control(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "event_log_tag_control";
	wl_el_tag_params_t pars;
	int argc;
	int err;
	int flags = 0;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argc = 0; argv[argc]; argc++)
		;

	if (argc < 4) {
		return BCME_USAGE_ERROR;
	}

	argv++;

	memset(&pars, 0, sizeof(wl_el_tag_params_t));
	pars.tag = htod16(atoi(*argv++));
	pars.set = atoi(*argv++);

	while (*argv) {
		if (!strcmp(*argv, "LOG")) {
			flags |= EVENT_LOG_TAG_FLAG_LOG;
		} else if (!strcmp(*argv, "PRINT")) {
			flags |= EVENT_LOG_TAG_FLAG_PRINT;
		} else if (!strcmp(*argv, "NONE")) {
			flags |= EVENT_LOG_TAG_FLAG_NONE;
		} else {
			return BCME_USAGE_ERROR;
		}
		argv++;
	}

	pars.flags = flags;

	err = wlu_iovar_set(wl, cmdname, &pars, sizeof(pars));

	return err;
}

static int wl_event_log_get(void *wl, cmd_t *cmd, char **argv)
{
	const char *cmdname = "event_log_get";
	char *set_arg = NULL;
	wl_el_set_params_t pars;
	int argc;
	int err = BCME_OK;

	UNUSED_PARAMETER(cmd);

	/* arg count */
	for (argv++, argc = 0; argv[argc]; argc++);

	memset(&pars, 0, sizeof(wl_el_set_params_t));

	/* Look for -f */
	if ((*argv) && !strcmp(*argv, "-f")) {
		set_arg = *++argv;
		if (!set_arg)
		{
			fprintf(stderr, "Missing argument to -f option\n");
			return BCME_USAGE_ERROR;
		}
		/* Ignore other arguments */
		pars.set = atoi(set_arg);
		pars.size = 0;

		/* cause a flush of an event log buffer associated with this particular
		 * set. This flush is caused by trigerring logtrace
		 */
		err = wlu_iovar_set(wl, cmdname, &pars, sizeof(wl_el_set_params_t));
	}
	else
	{
		fprintf(stderr, "Missing -f option. Currently -g <set> -s <buf_len> "
			"options are not supported\n");
		err = BCME_USAGE_ERROR;
	}
	return err;
}

#ifdef SERDOWNLOAD

static int dhd_rwl_download(void *dhd, char *fwname, char *command);

int
rwl_download(void *dhd, cmd_t *cmd, char **argv)
{
	char *fname = NULL;
	char *vname = NULL;
	int ret = 0;
	UNUSED_PARAMETER(cmd);
	if (!*++argv) {
		fprintf(stderr, "Require dongle image filename \n");
		ret = -1;
		goto exit;
	}
	else {
		fname = *argv;
		if (debug)
			printf("dongle image file is %s\n", fname);
	}
	if (!*++argv) {
		fprintf(stderr, "vars filename missing, assuming no var file\n");
		ret = -1;
		goto exit;
	}
	else {
		vname = *argv;
		if (debug)
			printf("dongle var file is %s\n", vname);
	}
	ret = dhd_rwl_download(dhd, fname, "download");
	printf("ret = %d\n", ret);

	if (ret == 0)
		ret = dhd_rwl_download(dhd, vname, "nvdownload");
exit:
	return ret;
}

static int
dhd_rwl_download(void *dhd, char *fwfile, char *command)
{
	unsigned char *bufp  = NULL;
	int len = 0, length = 0;
	int ret = 0;
	char *p;
	unsigned char *buff = NULL;
	int remained_bytes;
	uint32 start = 0;
	FILE *fp = NULL;
	struct stat finfo;
	unsigned long status;

	/* Open the firmware file */
	if (!(fp = fopen(fwfile, "rb"))) {
		perror(fwfile);
		ret = -1;
		goto exit;
	}

	if (stat(fwfile, &finfo)) {
		printf("dhd_rwl_download: %s: %s\n", fwfile, strerror(errno));
		ret = -1;
		goto exit;
	}

	length = finfo.st_size;

	if (length <= 0) {
		ret = -1;
		goto exit;
	}

	if ((bufp = malloc(length +4)) == NULL) {
		printf("dhd_rwl_download: Unable to allowcate %d bytes!\n", len);
		ret = -1;
		goto exit;
	}

	/* Read the firmware file into the buffer */
	status  = fread(bufp, sizeof(uint8), length, fp);

	/* close the firmware file */
	fclose(fp);
	fp = NULL;

	if ((int)status < length) {
		printf("dhd_rwl_download: Short read in %s!\n", fwfile);
		ret = -1;
		goto exit;
	}

	printf("Starting %s, total file length is %d\n", command, length);

	/* do the download reset */
	if ((ret = wlu_iovar_setint(dhd, command, TRUE))) {
		fprintf(stderr, "dhd_rwl_download: failed to put dongle to download mode\n");
		ret = -1;
		goto exit;
	}

	buff = bufp;
	remained_bytes = len = length;

	while (remained_bytes > 0) {
		printf(".");
		p = buf;
		memset(p, 0, WLC_IOCTL_MAXLEN);
		strcpy(p, "membytes");
		p += strlen("membytes") + 1;

		memcpy(p, &start, sizeof(int));
		p += sizeof(int);

		if (remained_bytes >= MEMBLOCK)
			len = MEMBLOCK;
		else
			len = remained_bytes;

		memcpy(p, &len, sizeof(int));
		p += sizeof(int);

		memcpy(p, buff, len);
		p += len;

		if (debug) {
			printf("sending %d bytes block: \n", (int)(p - buf));
		}

		ret = wlu_set(dhd, WLC_SET_VAR, &buf[0], (int) (p - buf));

		if (ret) {
			fprintf(stderr, "%s: error %d on writing %d membytes at 0x%08x\n",
				"wl_set()", ret, len, start);
			goto exit;
		}

		start += len;
		buff += len;
		remained_bytes -= len;
	}
	printf("\n");

	/* start running the downloaded code, download complete */
	if ((ret = wlu_iovar_setint(dhd, command, FALSE))) {
		fprintf(stderr, "dhd_rwl_download: failed to take dongle out of download mode\n");
		goto exit;
	}

exit:
	if (bufp)
		free(bufp);

	if (fp)
		fclose(fp);

	return ret;
}

/* Check that strlen("membytes")+1 + 2*sizeof(int32) + MEMBLOCK <= WLC_IOCTL_MAXLEN */
#if (MEMBLOCK + 17 > WLC_IOCTL_MAXLEN)
#error MEMBLOCK/WLC_IOCTL_MAXLEN sizing
#endif // endif

static int dhd_hsic_download(void *dhd, char *fwname, char *nvname);
static int ReadFiles(char *fwfile, char *nvfile, unsigned char ** buffer);
static int check_file(unsigned char *headers);

static char* chip_select = "none";

int
dhd_init(void *dhd, cmd_t *cmd, char **argv)
{
	int ret = -1;
	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		fprintf(stderr, "Error: Missing require chip ID"
			"<4325,  4329, 43291, 4330a1, 4330 or hsic>\n");
		ret = BCME_USAGE_ERROR;
	}
	else if (strcmp(*argv, "4325") && strcmp(*argv, "4329") &&
		strcmp(*argv, "43291") && strcmp(*argv, "4330") &&
		strcmp(*argv, "4330a1") && strcmp(*argv, "hsic")) {
		fprintf(stderr, "Error: Unsupported chip ID %s\n", *argv);
		ret = BCME_BADARG;
	}
	else if ((ret = wlu_iovar_setbuf(dhd, "init", *argv, strlen(*argv) + 1,
		buf, WLC_IOCTL_MAXLEN))) {
		fprintf(stderr, "Error: %s: failed to initialize the dongle \n",
		        "dhd_init()");
	}
	else
		ret = 0;

	if (ret == 0) {
		if (!strcmp(*argv, "4325"))  {
			fprintf(stdout, "4325 is the selected chip id\n");
			chip_select = "4325";
		} else if (!strcmp(*argv, "4329"))  {
			fprintf(stdout, "4329 is the selected chip id\n");
			chip_select = "4329";
	        } else if (!strcmp(*argv, "43291"))  {
			fprintf(stdout, "43291 is the selected chip id\n");
		        chip_select = "43291";
		} else if (!strcmp(*argv, "4330"))  {
			fprintf(stdout, "4330b0 is the selected chip id\n");
		        chip_select = "4330b0";
		} else if (!strcmp(*argv, "4330a1"))  {
			fprintf(stdout, "4330a1 is the selected chip id\n");
		        chip_select = "4330a1";
		} else if (!strcmp(*argv, "hsic"))  {
			fprintf(stdout, "hsic interface is selected\n");
		        chip_select = "hsic";
		} else
			chip_select = "none";
	}

	return ret;
}

int
dhd_download(void *dhd, cmd_t *cmd, char **argv)
{
	char *fname = NULL;
	char *vname = NULL;
	uint32 start = 0;
	uint32 last4bytes;
	int ret = 0;
	uint file_size;
	int ram_size = 0, var_size, var_words, nvram_len, remained_bytes;
	FILE *fp = NULL;
	struct stat finfo;
	char *bufp;
	int len;
	uint8 memblock[MEMBLOCK];
	uint8 varbuf[WLC_IOCTL_MAXLEN];

	UNUSED_PARAMETER(cmd);

	if (!strcmp(chip_select, "none")) {
		fprintf(stderr, "chip init must be called before firmware download. \n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	if (!strcmp(chip_select, "4325")) {
		fprintf(stdout, "using 4325 ram_info\n");
		ram_size = RAM_SIZE_4325;
	} else if (!strcmp(chip_select, "4329")) {
		fprintf(stdout, "using 4329 ram_info\n");
		ram_size = RAM_SIZE_4329;
	} else if (!strcmp(chip_select, "43291")) {
		fprintf(stdout, "using 43291 ram_info\n");
		ram_size = RAM_SIZE_43291;
	} else if (!strcmp(chip_select, "4330b0")) {
		fprintf(stdout, "using 4330 b0 ram_info\n");
		ram_size = RAM_SIZE_4330_b0;
	} else if (!strcmp(chip_select, "4330a1")) {
		fprintf(stdout, "using 4330a1 ram_info\n");
		ram_size = RAM_SIZE_4330_a1;
	} else if (!strcmp(chip_select, "hsic")) {
		fprintf(stdout, "using hsic interface\n");
	} else {
		fprintf(stderr, "Error: unknown chip\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	if (!*++argv) {
		fprintf(stderr, "Require dongle image filename \n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}
	else {
		fname = *argv;
		if (debug)
			printf("dongle image file is %s\n", fname);
	}

	if (!*++argv) {
		fprintf(stderr, "vars filename missing, assuming no var file\n");
		ret = BCME_USAGE_ERROR;
		goto exit;

	}
	else {
		vname = *argv;
		if (debug)
			printf("dongle var file is %s\n", vname);
	}

	/* do the download on hsic */
	/* merge the firmware and the nvram */
	if (!strcmp(chip_select, "hsic")) {
		/* the hsic firwmare and download code */
		ret = dhd_hsic_download(dhd, fname, vname);
		return ret;
	}

	if (!(fp = fopen(fname, "rb"))) {
		perror(fname);
		ret = BCME_BADARG;
		goto exit;
	}

	if (stat(fname, &finfo)) {
		perror(fname);
		ret = -1;
		goto exit;
	}
	file_size = finfo.st_size;
	if (debug) {
		printf("%s file_size=%d\n", fname, file_size);
	}

	/* do the download reset if not suppressed */
	if ((ret = wlu_iovar_setint(dhd, "download", TRUE))) {
		fprintf(stderr, "%s: failed to put dongle in download mode\n",
				"dhd_iovar_setint()");
		goto exit;
	}

	memset(memblock, 0, MEMBLOCK);

	printf("downloading %s, file_size=%d\n", fname, file_size);

	/* read the file and push blocks down to memory */
	while ((len = fread(memblock, sizeof(uint8), MEMBLOCK, fp))) {
		if (len < MEMBLOCK && !feof(fp)) {
			fprintf(stderr, "%s: error reading file %s\n", "fread()", fname);
			ret = -1;
			goto exit;
		}

		if (debug) {
			printf("memblock=\n%s\n", memblock);
		}

		bufp = buf;
		memset(bufp, 0, WLC_IOCTL_MAXLEN);
		strcpy(bufp, "membytes");
		bufp += strlen("membytes") + 1;
		memcpy(bufp, &start, sizeof(int));
		bufp += sizeof(int);
		memcpy(bufp, &len, sizeof(int));
		bufp += sizeof(int);
		memcpy(bufp, memblock, len);

		ret = wl_set(dhd, WLC_SET_VAR, &buf[0], (bufp - buf + len));

		if (ret) {
			fprintf(stderr, "%s: error %d on writing %d membytes at 0x%08x\n",
			        "wl_set()", ret, len, start);
			goto exit;
		}

		start += len;
		memset(memblock, 0, MEMBLOCK);
	}

	if (!feof(fp)) {
		fprintf(stderr, "%s: error reading file %s\n", "feof()", fname);
		ret = -1;
		goto exit;
	}
	fclose(fp);
	fp = NULL;

	if (vname) {
	/* download the vars file if specified */
	/* read in the file */
		if (!(fp = fopen(vname, "rb"))) {
			perror(vname);
			ret = BCME_BADARG;
			goto exit;
		}

		if (stat(vname, &finfo)) {
			perror(vname);
			ret = -1;
			goto exit;
		}
		file_size = finfo.st_size;

		printf("downloading %s, file_size=%d\n", vname, file_size);

		memset(varbuf, 0, WLC_IOCTL_MAXLEN);

		/* read the file and push blocks down to memory */
		if (fread(varbuf, 1, file_size, fp) != file_size) {
			perror(fname);
			ret = -1;
			goto exit;
		}

		fclose(fp);
		fp = NULL;

		if (debug) {
			printf("the original varbuf=%s\n", varbuf);
		}

		/* convert linefeeds to nuls */
		nvram_len = process_nvram_vars((char*)&varbuf, file_size);
		if (debug) {
			printf("after process_nvram_vars(), %s nvram_len=%d\n%s\n",
			vname, nvram_len, varbuf);
		}
		bufp = (char*)&varbuf + nvram_len;
		*bufp++ = 0;

		var_size = ROUNDUP(nvram_len + 1, 4);
		/* calculate start address */
		start = ram_size - var_size - 4;

		if (debug) {
			printf("var_size=%d, start=0x%0X\n", var_size, start);
		}

		/* need to send the last 4 bytes. */
		var_words = var_size / 4;
		last4bytes = (~var_words << 16) | (var_words & 0x0000FFFF);
		last4bytes = htol32(last4bytes);

		if (debug) {
			printf("last4bytes=0x%0X\n", last4bytes);
		}

		bufp = (char*)&varbuf + var_size;
		memcpy(bufp, &last4bytes, 4);

		/* send down var_size+4 bytes with each time "membytes" MEMBLOCK bytes */
		bufp = (char*)&varbuf;
		remained_bytes = var_size + 4;

		while (remained_bytes > 0) {
			char *p;

			p = buf;
			memset(p, 0, WLC_IOCTL_MAXLEN);

			strcpy(p, "membytes");
			p += strlen("membytes") + 1;

			memcpy(p, &start, sizeof(int));
			p += sizeof(int);

			if (remained_bytes >= MEMBLOCK) {
				len = MEMBLOCK;
			}
			else
				len = remained_bytes;

			memcpy(p, &len, sizeof(int));
			p += sizeof(int);

			memcpy(p, bufp, len);
			p += len;

			if (debug) {
				printf("sending %d bytes block:\n", (int)(p - buf));
				printf("%s\n", buf);
			}

			ret = wl_set(dhd, WLC_SET_VAR, &buf[0], (p - buf));

			if (ret) {
				fprintf(stderr, "%s: error %d on writing %d membytes at 0x%08x\n",
						"wl_set()", ret, len, start);
				goto exit;
			}

			start += len;
			bufp += len;
			remained_bytes -= len;
		}
	}

	/* start running the downloaded code if not suppressed */
	if ((ret = wlu_iovar_setint(dhd, "download", FALSE))) {
		fprintf(stderr, "%s: failed to take dongle out of download mode\n",
				"dhd_iovar_setint()");
		goto exit;
	}

exit:
	if (fp)
		fclose(fp);

	return ret;
}

int
dhd_upload(void *dhd, cmd_t *cmd, char **argv)
{
	char *fname = NULL;
	uint32 start = 0;
	uint32 size;
	int ram_size;
	FILE *fp = NULL;
	uint len;
	int ret = 0;

	UNUSED_PARAMETER(cmd);

	if (!strcmp(chip_select, "none")) {
		fprintf(stderr, "chip init must be called before firmware download. \n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	if (!strcmp(chip_select, "4325")) {
		fprintf(stdout, "using 4325 ram_info\n");
		ram_size = RAM_SIZE_4325;
	} else if (!strcmp(chip_select, "4329")) {
		fprintf(stdout, "using 4329 ram_info\n");
		ram_size = RAM_SIZE_4329;
	} else if (!strcmp(chip_select, "43291")) {
		fprintf(stdout, "using 43291 ram_info\n");
		ram_size = RAM_SIZE_43291;
	} else if (!strcmp(chip_select, "4330b0")) {
		fprintf(stdout, "using 4330 b0 ram_info\n");
		ram_size = RAM_SIZE_4330_b0;
	} else if (!strcmp(chip_select, "4330a1")) {
		fprintf(stdout, "using 4330 a1 ram_info\n");
		ram_size = RAM_SIZE_4330_a1;
	} else {
		fprintf(stderr, "Error: unknown chip\n");
		ret = BCME_USAGE_ERROR;
		goto exit;
	}

	argv++;

	if (debug) {
		printf("argv=%s\n", *argv);
	}

	fname = *argv;

	/* validate arguments */
	if (!fname) {
		fprintf(stderr, "filename required\n");
		ret = BCME_BADARG;
		goto exit;
	}

	if (!(fp = fopen(fname, "wb"))) {
		perror(fname);
		ret = BCME_BADARG;
		goto exit;
	}

	/* default size to full RAM */
	size = ram_size - start;

	/* read memory and write to file */
	while (size) {
		char *ptr;
		int params[2];

		len = MIN(MEMBLOCK, size);

		params[0] = start;
		params[1] = len;
		ret = wlu_iovar_getbuf(dhd, "membytes", params, 2 * sizeof(int),
		(void**)&ptr, MEMBLOCK);
		if (ret) {
			fprintf(stderr, "dhd_upload(): failed reading %d membytes from 0x%08x\n",
			        len, start);
			break;
		}

		if (fwrite(ptr, sizeof(*ptr), len, fp) != len) {
			fprintf(stderr, "dhd_upload(): error writing to file %s\n", fname);
			ret = -1;
			break;
		}

		start += len;
		size -= len;
	}

	fclose(fp);
exit:
	return ret;
}

static int
dhd_hsic_download(void *dhd, char *fwfile, char *nvfile)
{
	unsigned char *bufp  = NULL;
	int len = 0, length = 0;
	int ret = 0;
	char *p;
	unsigned char *buff = NULL;
	int remained_bytes;
	uint32 start = 0;

	/* read and merge fw and nvram files */
	length = ReadFiles(fwfile, nvfile, &bufp);
	if (length <= 0) {
		ret = -1;
		goto exit;
	}

	printf("Starting download, total file length is %d\n", length);

	/* do the download reset */
	if ((ret = wlu_iovar_setint(dhd, "download", TRUE))) {
		fprintf(stderr, "dhd_hsic_download: failed to put dongle to download mode\n");
		goto exit;
	}

	buff = bufp;
	remained_bytes = len = length;

	while (remained_bytes > 0) {
		printf(".");
		p = buf;
		memset(p, 0, WLC_IOCTL_MAXLEN);
		strcpy(p, "membytes");
		p += strlen("membytes") + 1;

		memcpy(p, &start, sizeof(int));
		p += sizeof(int);

		if (remained_bytes >= MEMBLOCK)
			len = MEMBLOCK;
		else
			len = remained_bytes;

		memcpy(p, &len, sizeof(int));
		p += sizeof(int);

		memcpy(p, buff, len);
		p += len;

		if (debug) {
			printf("sending %d bytes block: \n", (int)(p - buf));
		}

		ret = wlu_set(dhd, WLC_SET_VAR, &buf[0], (p - buf));

		if (ret) {
			fprintf(stderr, "%s: error %d on writing %d membytes at 0x%08x\n",
				"wl_set()", ret, len, start);
			goto exit;
		}

		start += len;
		buff += len;
		remained_bytes -= len;
	}
	printf("\n");

	/* start running the downloaded code, download complete */
	if ((ret = wlu_iovar_setint(dhd, "download", FALSE))) {
		fprintf(stderr, "dhd_hsic_download: failed to take dongle out of download mode\n");
		goto exit;
	}

exit:
	if (bufp)
		free(bufp);

	return ret;
}

static int ReadFiles(char *fname, char *vname, unsigned char ** buffer)
{
	FILE *fp = NULL;
	FILE *fp1 = NULL;
	struct stat finfo;
	uint8 *buf = NULL;
	int len, fwlen, actual_len, nvlen = 0;
	struct trx_header *hdr;
	unsigned long status;
	unsigned int pad;
	unsigned int i;
	int ret = -1;

	/* Open the firmware file */
	if (!(fp = fopen(fname, "rb"))) {
		perror(fname);
		ret = BCME_BADARG;
		goto exit;
	}

	if (stat(fname, &finfo)) {
		printf("dhd_download: %s: %s\n", fname, strerror(errno));
		ret = -1;
		goto exit;
	}
	len = fwlen = finfo.st_size;

	/* Open nvram file */
	if (!(fp1 = fopen(vname, "rb"))) {
		perror(fname);
		ret = BCME_BADARG;
		goto exit;
	}

	if (stat(vname, &finfo)) {
		printf("dhd_download: %s: %s\n", vname, strerror(errno));
		ret = -1;
		goto exit;
	}
	nvlen = finfo.st_size;
	len += nvlen;

	if ((buf = malloc(len +4)) == NULL) {
		printf("dhd_download: Unable to allowcate %d bytes!\n", len);
		ret = BCME_NOMEM;
		goto exit;
	}

	/* Read the firmware file into the buffer */
	status  = fread(buf, sizeof(uint8), fwlen, fp);

	/* close the firmware file */
	fclose(fp);

	if ((int)status < fwlen) {
		printf("dhd_download: Short read in %s!\n", fname);
		ret = -1;
		goto exit;
	}

	/* Validating the format /length etc of the file */
	if ((actual_len = check_file(buf)) <= 0) {
		printf("dhd_download: Failed input file check on %s!\n", fname);
		ret = -1;
		goto exit;
	}

	/* Read the nvram file into the buffer */
	status  = fread(buf + actual_len, sizeof(uint8), nvlen, fp1);

	/* close the nvram file */
	fclose(fp1);

	if ((int)status < nvlen) {
		printf("dhd_download: Short read in %s!\n", vname);
		ret = -1;
		goto exit;
	}

	/* porcess nvram vars if user specifics a text file instead of binary */
	nvlen = process_nvram_vars((char*) &buf[actual_len], (unsigned int) nvlen);

	if (nvlen % 4) {
		pad = 4 - (nvlen % 4);
		for (i = 0; i < pad; i ++)
			buf[actual_len + nvlen + i] = 0;
		nvlen += pad;
	}

	/* fix up len to be actual len + nvram len */
	len = actual_len + nvlen;
	/* update trx header with added nvram bytes */
	hdr = (struct trx_header *) buf;
	hdr->len = htol32(len);
	/* pass the actual fw len */
	hdr->offsets[TRX_OFFSETS_NVM_LEN_IDX] = htol32(nvlen);
	/* caculate CRC over header */
	hdr->crc32 = hndcrc32((uint8 *) &hdr->flag_version,
		sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
		CRC32_INIT_VALUE);

	/* Calculate CRC over data */
	for (i = sizeof(struct trx_header); i < (unsigned int)len; ++i)
		hdr->crc32 = hndcrc32((uint8 *)&buf[i], 1, hdr->crc32);
	hdr->crc32 = htol32(hdr->crc32);

	*buffer  = buf;
	return len;

exit:
	if (buf)
		free(buf);

	return ret;
}

static int
check_file(unsigned char *headers)
{
	struct trx_header *trx;
	int actual_len = -1;

	/* Extract trx header */
	trx = (struct trx_header *)headers;
	if ((ltoh32(trx->magic)) != TRX_MAGIC) {
		printf("check_file: Error: trx bad hdr %x!\n", ltoh32(trx->magic));
		return -1;
	}

	if (ltoh32(trx->flag_version) & TRX_UNCOMP_IMAGE) {
		actual_len = ltoh32(trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]) +
			sizeof(struct trx_header);
		return actual_len;
	}
	return actual_len;
}
#endif /* SERDOWNLOAD */

static int
wlu_mempool(void *wl, cmd_t *cmd, char **argv)
{
	void               *ptr;
	int                ret;
	int                i;
	wl_mempool_stats_t *stats;
	bcm_mp_stats_t     *mp_stats;

	UNUSED_PARAMETER(argv);

	if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return ret;

	stats = (wl_mempool_stats_t *) ptr;
	mp_stats = stats->s;

	printf("%-8s %8s %8s %8s %8s %8s\n", "Name", "SZ", "Max", "Curr", "HiWater", "Failed");
	for (i = 0; i < stats->num; i++) {
		printf("%-8s %8d %8d %8d %8d %8d\n", mp_stats->name, (int) mp_stats->objsz,
		       mp_stats->nobj, mp_stats->num_alloc, mp_stats->high_water,
		       mp_stats->failed_alloc);

		mp_stats++;
	}

	return (0);
}

static int
wl_ie(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr;
	int err;
	uchar *data;
	int bsscfg_idx = 0;
	int consumed = 0;
	int iecount;
	ie_setbuf_t *ie_setbuf;
	ie_getbuf_t param;
	uchar datalen, type, count, col;

	/* parse a bsscfg_idx option if present */
	if ((err = wl_cfg_option(argv + 1, argv[0], &bsscfg_idx, &consumed)) != 0)
		return err;
	if (consumed)
		argv = argv + consumed;
	else
		bsscfg_idx = -1;

	if (!*++argv) {
		fprintf(stderr, "missing parameter type\n");
		return BCME_USAGE_ERROR;
	}
	/* get IE type */
	type = (uchar)atoi(argv[0]);

	if (!*++argv) {
		param.id = type;
		ptr = buf;
		if (bsscfg_idx == -1)
			err = wlu_var_getbuf(wl, cmd->name, &param, sizeof(param), &ptr);
		else
			err = wl_bssiovar_getbuf(wl, cmd->name, bsscfg_idx, &param, sizeof(param),
			buf, WLC_IOCTL_MAXLEN);
		if (err == 0) {
			data = (uchar *)ptr;
			datalen = data[1]+2;
			printf("%s len %d\n", cmd->name, datalen);
			printf("%s Data:\n", cmd->name);
			for (count = 0; (count < datalen);) {
				for (col = 0; (col < MAX_DATA_COLS) &&
					(count < datalen); col++, count++) {
					printf("%02x", *data++);
				}
				printf("\n");
			}
		}
		else {
			fprintf(stderr, "Error %d getting IOVar\n", err);
		}
		return err;
	}

	/* get IE length */
	datalen = (uchar)atoi(argv[0]);

	if (datalen > 0) {
		if (!argv[1]) {
			fprintf(stderr, "Data bytes should be specified for IE of length %d\n",
				datalen);
			return BCME_USAGE_ERROR;
		}
		else {
			/* Ensure each data byte is 2 characters long */
			if ((int)strlen (argv[1]) < (datalen * 2)) {
				fprintf(stderr, "Please specify all the data bytes for this IE\n");
				return BCME_BADARG;
			}
		}
	}

	if ((datalen == 0) && (argv[1] != NULL))
		fprintf(stderr, "Ignoring data bytes for IE of length %d\n", datalen);

	count = sizeof(ie_setbuf_t) + datalen - 1;
	data = malloc(count);
	if (data == NULL) {
		fprintf(stderr, "memory alloc failure\n");
		return BCME_NOMEM;
	}

	ie_setbuf = (ie_setbuf_t *) data;
	/* Copy the ie SET command ("add") to the buffer */
	strncpy(ie_setbuf->cmd, "add", VNDR_IE_CMD_LEN - 1);
	ie_setbuf->cmd[VNDR_IE_CMD_LEN - 1] = '\0';

	/* Buffer contains only 1 IE */
	iecount = htod32(1);
	memcpy((void *)&ie_setbuf->ie_buffer.iecount, &iecount, sizeof(int));

	/* Now, add the IE to the buffer */
	ie_setbuf->ie_buffer.ie_list[0].ie_data.id = type;
	ie_setbuf->ie_buffer.ie_list[0].ie_data.len = datalen;

	if (datalen > 0) {
		if ((err = get_ie_data ((uchar *)argv[1],
		          (uchar *)&ie_setbuf->ie_buffer.ie_list[0].ie_data.data[0],
		          datalen))) {
			free(data);
			fprintf(stderr, "Error parsing data arg\n");
			return err;
		}
	}

	if (bsscfg_idx == -1)
		err = wlu_var_setbuf(wl, cmd->name, data, count);
	else
		err = wlu_bssiovar_setbuf(wl, cmd->name, bsscfg_idx,
			data, count, buf, WLC_IOCTL_MAXLEN);

	free(data);
	return (err);
}

/* Restore the ignored warnings status */

static int
wl_sleep_ret_ext(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int argc, i;
	uint32 val;
	char *endptr = NULL;
	wl_pm2_sleep_ret_ext_t sleep_ret_ext;
	wl_pm2_sleep_ret_ext_t* sleep_ret_ext_ptr;

	/* Skip the command name */
	UNUSED_PARAMETER(cmd);
	argv++;

	/* If no arguments are given, print the existing settings */
	argc = ARGCNT(argv);
	if (argc == 0) {
		char *logic_str;

		/* Get and print the values */
		if ((ret = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, (void*) &sleep_ret_ext_ptr)))
			return ret;

		if (sleep_ret_ext_ptr->logic == WL_DFRTS_LOGIC_OFF)
			logic_str = "DISABLED";
		else if (sleep_ret_ext_ptr->logic == WL_DFRTS_LOGIC_OR)
			logic_str = "OR";
		else if (sleep_ret_ext_ptr->logic == WL_DFRTS_LOGIC_AND)
			logic_str = "AND";
		else
			logic_str = "ERROR";

		printf("logic: %d (%s)\n",
		       sleep_ret_ext_ptr->logic, logic_str);
		if (sleep_ret_ext_ptr->logic != WL_DFRTS_LOGIC_OFF) {
			printf("low_ms: %d\n", sleep_ret_ext_ptr->low_ms);
			printf("high_ms: %d\n", sleep_ret_ext_ptr->high_ms);
			printf("rx_pkts_threshold: %d\n",
				sleep_ret_ext_ptr->rx_pkts_threshold);
			printf("tx_pkts_threshold: %d\n",
			       sleep_ret_ext_ptr->tx_pkts_threshold);
			printf("txrx_pkts_threshold: %d\n",
			       sleep_ret_ext_ptr->txrx_pkts_threshold);
			printf("rx_bytes_threshold: %d\n",
			       sleep_ret_ext_ptr->rx_bytes_threshold);
			printf("tx_bytes_threshold: %d\n",
			       sleep_ret_ext_ptr->tx_bytes_threshold);
			printf("txrx_bytes_threshold: %d\n",
			       sleep_ret_ext_ptr->txrx_bytes_threshold);
		}
		return 0;
	}

	memset(&sleep_ret_ext, 0, sizeof(wl_pm2_sleep_ret_ext_t));
	i = 0;

	/* Get the first 'logic' argument. */
	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	if (val != WL_DFRTS_LOGIC_OFF && val != WL_DFRTS_LOGIC_OR &&
		val != WL_DFRTS_LOGIC_AND) {
		printf("Invalid logic value %u\n", val);
		goto usage;
	}
	sleep_ret_ext.logic = val;
	++i;

	/* If logic is 0 (disable) then no more arguments are needed */
	if (sleep_ret_ext.logic == 0)
		goto set;

	if (argc < 9)
		goto usage;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.low_ms = val;
	++i;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.high_ms = val;
	++i;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.rx_pkts_threshold = val;
	++i;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.tx_pkts_threshold = val;
	++i;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.txrx_pkts_threshold = val;
	++i;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.rx_bytes_threshold = val;
	++i;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.tx_bytes_threshold = val;
	++i;

	val = strtoul(argv[i], &endptr, 0);
	if (*endptr != '\0')
		goto usage;
	sleep_ret_ext.txrx_bytes_threshold = val;
	++i;

	if (i != argc)
		goto usage;

set:
	return wlu_var_setbuf(wl, cmd->name, &sleep_ret_ext,
		sizeof(wl_pm2_sleep_ret_ext_t));

usage:
	printf("Usage: %s [logic] [<low_ms> <high_ms>"
		" <rxP> <txP> <txrxP> <rxB> <txB> <txrxB>\n", wlu_av0);
	printf("Parameters:\n");
	printf("logic   : 0=disable, 1=OR, 2=AND all non-zero switching thresholds.\n");
	printf("low_ms  : Low pm2_sleep_ret value.\n");
	printf("high_ms : High pm2_sleep_ret value.\n");
	printf("rxP     : Switching threshold in # of rx packets.\n");
	printf("          eg. Switch from the low to high FRTS value if rxP or\n");
	printf("          more packets are received in a PM2 radio wake period.\n");
	printf("          0 means ignore this threshold.\n");
	printf("txP     : Switching threshold in # of tx packets.\n");
	printf("txrxP   : Switching threshold in # of combined tx+rx packets.\n");
	printf("rxB     : Switching threshold in # of rx bytes.\n");
	printf("txB     : Switching threshold in # of tx bytes.\n");
	printf("txrxB   : Switching threshold in # of combined tx+rx bytes.\n");
	return -1;
}

static int wl_stamon_sta_config(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	wlc_stamon_sta_config_t stamon_cfg;
	struct maclist *maclist = (struct maclist *) buf;
	uint i, max = (WLC_IOCTL_MAXLEN - sizeof(int)) / ETHER_ADDR_LEN;
	struct ether_addr *ea;
	int monitor_time;
	char *endptr;

	memset(&stamon_cfg, 0, sizeof(wlc_stamon_sta_config_t));
	if (!*++argv) {
		maclist->count = htod32(max);
		err = wlu_iovar_get(wl, cmd->name, maclist,
			WLC_IOCTL_MAXLEN);
		maclist->count = dtoh32(maclist->count);
		if (!err) {
			for (i = 0, ea = maclist->ea; i < maclist->count && i < max; i++, ea++)
				printf("%s\n", wl_ether_etoa(ea));
		}
	} else {
		stamon_cfg.version = htod16(STAMON_STACONFIG_VER);
		stamon_cfg.length = htod16(STAMON_STACONFIG_LENGTH);

		if (!stricmp(*argv, "add")) {
			stamon_cfg.cmd = htodenum(STAMON_CFG_CMD_ADD);
			if (!*++argv || !wl_ether_atoe(*argv, &stamon_cfg.ea)) {
				printf(" ERROR: no valid ether addr provided\n");
				return -1;
			}
			argv++;
			if (*argv) {
				stamon_cfg.chanspec = wf_chspec_aton(*argv);
				stamon_cfg.chanspec = wl_chspec_to_driver(stamon_cfg.chanspec);
				if (!stamon_cfg.chanspec) {
					printf(" ERROR: no valid chanspec provided\n");
					return -1;
				}
				argv++;
				stamon_cfg.offchan_time = (*argv) ? strtoul(*argv, NULL, 0) : 0;
				stamon_cfg.offchan_time = htod32(stamon_cfg.offchan_time);

				if (!stamon_cfg.offchan_time) {
					printf(" ERROR: no valid off-channel time provided\n");
					return -1;
				}
			}
		} else if (!stricmp(*argv, "del")) {
			stamon_cfg.cmd = htodenum(STAMON_CFG_CMD_DEL);
			if (!*++argv || !wl_ether_atoe(*argv, &stamon_cfg.ea)) {
				printf(" ERROR: no valid ether addr provided\n");
				err = -1;
			}
		} else if (!stricmp(*argv, "monitor_time")) {
			stamon_cfg.cmd = htodenum(STAMON_CFG_CMD_SET_MONTIME);
			if (*++(argv) != NULL) {
				monitor_time = strtoul(*argv, &endptr, 0);

				if ((monitor_time < 0) || (*endptr != '\0')) {
					printf(" ERROR: Invalid monitor_time provided\n");
					err = BCME_BADARG;
				} else {
					stamon_cfg.monitor_time = htod32((uint32)monitor_time);
				}
			} else {
				printf("Incorrect number of arguments\n");
				err = BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "enable"))
			stamon_cfg.cmd = htodenum(STAMON_CFG_CMD_ENB);
		else if (!stricmp(*argv, "disable"))
			stamon_cfg.cmd = htodenum(STAMON_CFG_CMD_DSB);
		else if (!stricmp(*argv, "counters")) {
			const char *cmdname = "sta_monitor_cnt";
			void *ptr = NULL;
			stamon_cnts_t *cntrs = NULL;

			if ((err = wlu_var_getbuf_sm(wl, cmdname, NULL, 0, &ptr))) {
				return err;
			}
			cntrs = (stamon_cnts_t*)ptr;
			if (dtoh16(cntrs->version) == STAMON_CNTR_VER) {
				printf("stamon_cnts: data %u, mngt %u, cntl %u\n",
				dtoh32(cntrs->rxstamondata), dtoh32(cntrs->rxstamonmngt),
				dtoh32(cntrs->rxstamoncntl));
			} else {
				err = BCME_VERSION;
			}
			return err;
		} else if (!stricmp(*argv, "reset_cnts"))
			stamon_cfg.cmd = htodenum(STAMON_CFG_CMD_RSTCNT);
		else if (!stricmp(*argv, "stats")) {
			stamon_info_t *pbuf;

			stamon_cfg.cmd = htodenum(STAMON_CFG_CMD_GET_STATS);
			if (!*++argv || !wl_ether_atoe(*argv, &stamon_cfg.ea)) {
				printf(" ERROR: no valid ether addr provided\n");
				err = -1;
			}

			memset(buf, 0, WLC_IOCTL_MAXLEN);
			if (!err) {
				err = wlu_iovar_getbuf(wl, cmd->name, &stamon_cfg,
					sizeof(wlc_stamon_sta_config_t), buf, WLC_IOCTL_MAXLEN);
			}
			pbuf = (stamon_info_t*)buf;
			if (!err) {
				pbuf->version = dtoh32(pbuf->version);
				pbuf->count = dtoh32(pbuf->count);
				if (pbuf->count != 0) {
					/* Success case: Found the mac in STAMON and
					 * read the stats
					 */
					printf(""MACF" : RSSI :%d \n",
						ETHER_TO_MACF(pbuf->sta_data[0].ea),
						dtoh32(pbuf->sta_data[0].rssi));
				} else if (pbuf->count == 0) {
					/* Failed case, Didn't Found the Mac
					 * read the stats
					 */
					printf("STAMON not monitoring "MACF" \n",
						ETHER_TO_MACF(stamon_cfg.ea));
				}
			}

			return err;
		} else {
			printf("error: unknown operation option%s\n", *argv);
			err = -1;
		}

		if (!err)
			err = wlu_iovar_set(wl, cmd->name,
				&stamon_cfg, sizeof(wlc_stamon_sta_config_t));

	}

	return err;
}

static monitor_promisc_level_msg_t wl_monpromisc_level_msgs[] = {
	{WL_MONPROMISC_PROMISC,	"promisc"},
	{WL_MONPROMISC_CTRL, "ctrl"},
	{WL_MONPROMISC_FCS, "fcs"},
	{0,		NULL}
};

#define PERCENT(val, total) ((total) > 0 ? ((val)*100/(total)) : (0))

static void
wl_rate_histo_map2str(uint8 type, uint32 index, uint32 val, uint32 percent,
	bool recent, char *out_str)
{
	const uint8 legacy_mbps[] = {1, 2, 5, 6, 9, 11, 12, 18, 24, 36, 48, 54};
	const uint8 legacy_mbps_len = sizeof(legacy_mbps) / sizeof(legacy_mbps[0]);
	char str[16];

	switch (type) {
		case WL_HISTO_RATE_TYPE_VHT:
			/* VHT: Map index to MCSxNSS
			 *     MCS = index % 12
			 *     NSS = (index / 12) + 1
			 */
			sprintf(str, "%dx%d", WL_HISTO_RATE_DECODE_VHT_MCS(index),
				WL_HISTO_RATE_DECODE_VHT_NSS(index));
			break;
		case WL_HISTO_RATE_TYPE_LEGACY:
			/* Legacy: Map index to rate range in Mbps
			 *     range start = index * 4 Mbps
			 *     range end   = ((index + 1 ) * 4) - 1 Mbps
			 */
			sprintf(str, "%dMbps", (index < legacy_mbps_len ? legacy_mbps[index] : 0));
			break;
		case WL_HISTO_RATE_TYPE_HT:
			/* HT: Map index is MCS index too */
		default:
			sprintf(str, "%d", index);
			break;
	}
	sprintf(out_str, "%4s: %d(%d%%)%c ", str, val, percent,
		(recent ? '*' : ' '));
}

/* print rate histogram map */
static int
wl_rate_histo_map_print(wl_rate_histo_map1_t *map)
{
	uint32 i, j, lim, tot = 0, totLeg = 0;
	uint32 totVHT = 0, totVHTBW[3] = {0}, totVHTGE8 = 0, totVHTNSS[4] = {0};
	const char *type_str[WL_HISTO_RATE_NUM_TYPES] = {"Legacy", "HT/11n", "VHT/11ac"};
	char out_str[32], *bar = "====================";
	uint8 bwIdx, nssIdx, rtype, percent, brk;
	bool vhtRateUsed = (map->recent_type == WL_HISTO_RATE_TYPE_VHT);

	if (map == NULL) {
		return BCME_ERROR;
	}
	for (i = 0; i < WL_HISTO_MAP1_ARR_LEN; i++) {
		if (map->arr[i] == 0) {
			continue;
		}
		tot += map->arr[i];
		if (i < WL_NUM_LEGACY_RATES) {
			totLeg += map->arr[i];
		} else {
			bwIdx = (i - WL_NUM_LEGACY_RATES) / WL_NUM_VHT_RATES;
			nssIdx = ((i - WL_NUM_LEGACY_RATES) % WL_NUM_VHT_RATES) / 12; /* NSS -1 */
			if (bwIdx < 3) {
				totVHTBW[bwIdx] += map->arr[i];
				totVHT += map->arr[i];
				if (((i - WL_NUM_LEGACY_RATES) % 12) >= 8) {
					vhtRateUsed = TRUE;
					totVHTGE8 += map->arr[i];
				}
			}
			if (nssIdx < 4) {
				totVHTNSS[nssIdx] += map->arr[i];
			}
		}
	}
	if (tot == 0) {
		printf("total:%d, duration: %ds\n", tot, map->seconds);
		return BCME_OK;
	} else {
		printf("total:%d, recent rate type: %s, duration: %ds\n", tot,
			(map->recent_type < WL_HISTO_RATE_NUM_TYPES ?
			type_str[map->recent_type] : "Unknown"),
			map->seconds);
		printf("\ttotal Legacy/non: %u/%u, non-Legacy @NSS1/2/3/4: %u/%u/%u/%u, "
			"non-Legacy @20/40/80MHz: %u/%u/%u, VHT@MCS 9-11: %d\n",
			totLeg, totVHT,
			totVHTNSS[0], totVHTNSS[1], totVHTNSS[2], totVHTNSS[3],
			totVHTBW[0], totVHTBW[1], totVHTBW[2], totVHTGE8);
	}

	if (totLeg > 0) {
		brk = 6;
		lim = WL_NUM_LEGACY_RATES;
		for (i = 0; i < lim; i++) {
			percent = PERCENT(map->arr[i], tot);
			wl_rate_histo_map2str(WL_HISTO_RATE_TYPE_LEGACY,
				i, map->arr[i], percent, (i == map->recent_index), out_str);
			printf("%-16s ", out_str);
			if (((uint32)(i % brk)) == ((uint32)(brk - 1)) ||
				((uint32)i) == (uint32)(lim - 1)) {
				printf("\n");
			}
		}
	}

	if (totVHT == 0) {
		return BCME_OK;
	}

	brk = vhtRateUsed ? 12 : 8;
	lim = WL_NUM_VHT_RATES;
	for (bwIdx = 0; bwIdx < 3; bwIdx++) {
		if (totVHTBW[bwIdx] == 0) {
			continue; /* none on this bw index */
		}
		i = WL_NUM_LEGACY_RATES + bwIdx * WL_NUM_VHT_RATES;
		printf("Bandwidth: %dMHz\n", WL_HISTO_INDEX2BW(i));
		for (j = 0; j < lim; j++, i++) {
			/* could skip since none at specific nss
			 * if (j/12 >= 4 || totVHTNSS[j/12] == 0) {
			 *	continue;
			 *}
			 */
			/* skip since none at vht mcs > 7 */
			if (!vhtRateUsed && (j % 12) > 7) {
				continue;
			}
			percent = PERCENT(map->arr[i], tot);
			wl_rate_histo_map2str(WL_HISTO_RATE_TYPE_VHT, j, map->arr[i],
				percent, (i == map->recent_index), out_str);
			printf("%-16s ", out_str);
			if (((uint32)(j % 12)) == ((uint32)(brk - 1)) ||
				((uint32)j) == (uint32)(lim - 1)) {
				printf("\n");
			}
		}
	}
	printf("\n");

	for (i = 0; i < WL_HISTO_MAP1_ARR_LEN; i++) {
		if (map->arr[i] == 0) {
			continue;
		}
		percent = PERCENT(map->arr[i], tot);
		if (i < WL_NUM_LEGACY_RATES) {
			rtype = WL_HISTO_RATE_TYPE_LEGACY;
			j = i;
		} else {
			rtype = WL_HISTO_RATE_TYPE_VHT;
			j = (i - WL_NUM_LEGACY_RATES) % WL_NUM_VHT_RATES;
		}
		wl_rate_histo_map2str(rtype, j, map->arr[i], percent,
			(i == map->recent_index), out_str);
		printf("@%3dMHz %-20s |%.*s\n", WL_HISTO_INDEX2BW(i), out_str, (percent + 2)/5,
			bar);
	}
	return BCME_OK;
}

/* print rate histogram report */
static int
wl_rate_histo_report_print(wl_rate_histo_report_t *rpt)
{
	wl_rate_histo_maps1_t * maps;
	const size_t rpt_len = sizeof(*rpt) + sizeof(*maps);

	if (rpt->ver != WL_HISTO_VER_1) {
		printf("Unsupported version: %d\n", rpt->ver);
		return BCME_UNSUPPORTED;
	}
	printf("Version: %d\n", rpt->ver);

	if (rpt->type != WL_HISTO_TYPE_RATE_MAP1) {
		printf("Unsupported histogram type: %d\n", rpt->type);
		return BCME_UNSUPPORTED;
	}
	printf("Report type: %d\n", rpt->type);

	if (rpt->length < rpt_len) {
		printf("Report buffer length too short: %d < %d\n", (int) rpt->length,
			(int) rpt_len);
		return BCME_BUFTOOSHORT;
	}

	if (rpt->length < (rpt->fixed_len + sizeof(*maps))) {
		printf("Report map buffer length too short: %d < %d = %d+%d\n", rpt->length,
			(int) (rpt->fixed_len + sizeof(*maps)),
			(int) rpt->fixed_len, (int) sizeof(*maps));
		return BCME_BUFTOOSHORT;
	}

	printf("Peer MAC %s\n", wl_ether_etoa(&rpt->ea));
	maps = WL_HISTO_DATA(rpt);
	printf("\nRx ");
	wl_rate_histo_map_print(&maps->rx);
	printf("\nTx ");
	wl_rate_histo_map_print(&maps->tx);
	printf("\n");

	return BCME_OK;
}

/* get rate histogram report */
static int
wl_rate_histo_report(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int rpt_len = sizeof(wl_rate_histo_report_t) + sizeof(wl_rate_histo_maps1_t);
	wl_rate_histo_report_t *rpt = (wl_rate_histo_report_t *) buf;

	memset(rpt, 0, sizeof(*rpt));
	rpt->ver = WL_HISTO_VER_1;
	rpt->type = WL_HISTO_TYPE_RATE_MAP1;
	rpt->length = rpt_len;
	rpt->fixed_len = WL_HISTO_VER_1_FIXED_LEN;

	if (*++argv && !wl_ether_atoe(*argv, &rpt->ea)) {
		printf("Error parsing (optional parameter) MAC address\n");
		return BCME_USAGE_ERROR;
	}

	err = wlu_iovar_getbuf(wl, cmd->name, rpt, sizeof(*rpt), rpt, WLC_IOCTL_MAXLEN);

	if (err == BCME_OK) {
		err = wl_rate_histo_report_print(rpt);
	}

	return err;
}

static int
wl_monitor_promisc_level(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint val = 0, last_val = 0;
	uint promiscbitmap = 0, promiscbitmap_add = 0, promiscbitmap_del = 0;
	char *endptr;
	const char *cmdname = "monitor_promisc_level";

	UNUSED_PARAMETER(cmd);
	if ((ret = wlu_iovar_getint(wl, cmdname, (int *)&promiscbitmap) < 0)) {
		return ret;
	}
	promiscbitmap = dtoh32(promiscbitmap);
	if (!*++argv) {
		printf("0x%x ", promiscbitmap);
		for (i = 0; (val = wl_monpromisc_level_msgs[i].value); i++) {
			if ((promiscbitmap & val) && (val != last_val))
				printf(" %s", wl_monpromisc_level_msgs[i].string);
			last_val = val;
		}
		printf("\n");
		return (0);
	}
	while (*argv) {
		char *s = *argv;
		if (*s == '+' || *s == '-')
			s++;
		else
			promiscbitmap_del = ~0; /* make the whole list absolute */
		val = strtoul(s, &endptr, 0);
		if (val == 0xFFFFFFFF) {
			fprintf(stderr,
				"Bits >32 are not supported on this driver version\n");
			val = 1;
		}
		/* not an integer if not all the string was parsed by strtoul */
		if (*endptr != '\0') {
			for (i = 0; (val = wl_monpromisc_level_msgs[i].value); i++) {
				if (stricmp(wl_monpromisc_level_msgs[i].string, s) == 0) {
					break;
				}
			}
			if (!val) {
				goto usage;
			}
		}
		if (**argv == '-')
			promiscbitmap_del |= val;
		else
			promiscbitmap_add |= val;
		++argv;
	}
	promiscbitmap &= ~promiscbitmap_del;
	promiscbitmap |= promiscbitmap_add;
	promiscbitmap = htod32(promiscbitmap);
	return (wlu_iovar_setint(wl, cmdname, (int)promiscbitmap));

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");
	for (i = 0; (val = wl_monpromisc_level_msgs[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, wl_monpromisc_level_msgs[i].string);
		else
			fprintf(stderr, ", %s", wl_monpromisc_level_msgs[i].string);
		last_val = val;
	}
	fprintf(stderr, "\n");
	return 0;
}

static int
wl_bss_peer_info(void *wl, cmd_t *cmd, char **argv)
{
	bss_peer_list_info_t *info;
	bss_peer_info_t *peer_info;
	bss_peer_info_param_t param;
	int err, i;
	void *ptr;

	memset(&param, 0, sizeof(bss_peer_info_param_t));
	param.version = htod16(BSS_PEER_INFO_PARAM_CUR_VER);

	if (*++argv) {
		if (!wl_ether_atoe(*argv, &param.ea)) {
			printf(" ERROR: no valid ether addr provided\n");
			return -1;
		}
	}

	if ((err = wlu_var_getbuf_med(wl, cmd->name, &param, sizeof(bss_peer_info_param_t),
		&ptr)) < 0)
		return err;

	info = (bss_peer_list_info_t*)ptr;

	if ((dtoh16(info->version) != BSS_PEER_LIST_INFO_CUR_VER) ||
		(dtoh16(info->bss_peer_info_len) != sizeof(bss_peer_info_t))) {
		printf("BSS peer info version/structure size mismatch driver %d firmware %d \r\n",
			BSS_PEER_LIST_INFO_CUR_VER, dtoh16(info->version));
		return -1;
	}

	if (WLC_IOCTL_MEDLEN < (BSS_PEER_LIST_INFO_FIXED_LEN +
		(dtoh32(info->count) * sizeof(bss_peer_info_t)))) {
		printf("ERROR : peer list received exceed the buffer size\r\n");
	}

	for (i = 0; i < (int)dtoh32(info->count); i++) {
		peer_info = &info->peer_info[i];
		peer_info->rateset.count = dtoh32(peer_info->rateset.count);
		printf("PEER%d: MAC: %s: RSSI %d TxRate %d kbps RxRate %d kbps age : %ds\r\n",
			i, wl_ether_etoa(&peer_info->ea), peer_info->rssi,
			dtoh32(peer_info->tx_rate), dtoh32(peer_info->rx_rate),
			dtoh32(peer_info->age));
			printf("\t rateset ");
			dump_rateset(peer_info->rateset.rates, peer_info->rateset.count);
			printf("\r\n");
	}

	return 0;
}

static int
wl_taf_def(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_taf_define_t *taf_def;
	const struct ether_addr undef_ea = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
	taf_def = (wl_taf_define_t *) (buf);

	taf_def->text[0] = 0;
	taf_def->version = 1;

	argv++;

	/* check if the parameter is MAC address. If not, process as text command
	 * by sending all arguments in padded buffer (NULL separator) with final
	 * NULL to end the argument list.
	 */
	if ((*argv == NULL) || !wl_ether_atoe(*argv, &taf_def->ea) ||
		(strlen(*argv) != 17) || ((*argv)[2] != ':')) {

		uint32 offset = 0;
		uint32 buf_len = WLC_IOCTL_SMLEN - sizeof(*taf_def);

		/* setting undef_ea indicates no valid MAC address was supplied and this
		 * should be interpreted as free format text command
		 */
		taf_def->ea = undef_ea;

		while (*argv && (buf_len > 1)) {
			char* cp = *argv;

			while (*cp && (buf_len > 1)) {
				taf_def->text[offset++] = *cp++;
				buf_len--;
			}
			taf_def->text[offset++] = 0;
			buf_len--;

			argv++;
		}

		/*
		* NB text param block in taf define iovar requires terminating '\0' to signify
		* end of param block;
		*/
		taf_def->text[offset++] = 0;
		buf_len--;

		err = wlu_iovar_getbuf(wl, cmd->name, taf_def,
		                       sizeof(*taf_def) + offset,
		                       taf_def, WLC_IOCTL_SMLEN);
		if (err < 0) {
			return err;
		}
		if (taf_def->text[0]) {
			fputs((char *)taf_def->text, stdout);
		}
		return err;
	}

	/* at this point, the first arg was MAC address. Are there any more arguments? */
	argv++;

	if (*argv == NULL) {
		/* no more arguments, do a 'get' to find status of MAC address */
		taf_def->sch = 0;
		taf_def->prio = 0;

		err = wlu_iovar_getbuf(wl, cmd->name, taf_def,
		                            sizeof(*taf_def),
		                            taf_def, WLC_IOCTL_SMLEN);
		if (err < 0) {
			return err;
		}
		printf(MACF" sch: %s (%u) / prio: %u\n", ETHER_TO_MACF(taf_def->ea),
		       taf_def->text, taf_def->sch, taf_def->prio);
	} else {
		uint32 paramlen = sizeof(*taf_def);

		/* more arguments given, do a 'set' */
		/* in case a text argument is given rather than number, make a note */
		snprintf((char *)taf_def->text, WLC_IOCTL_SMLEN - sizeof(*taf_def), "%s", *argv);
		paramlen += strlen((char *)taf_def->text) + 1;

		if (*argv[0] >= '0' && *argv[0] <= '9') {
			/* number was given after all, convert it */
			taf_def->sch = strtoul(*argv, NULL, 0);
		}
		else {
			/* no number was given, mark invalid */
			taf_def->sch = (uint32)(~0);
		}

		/* next argument assumed to be numeric */
		argv++;

		if (*argv && (*argv[0] != '-')) {
			taf_def->prio = strtoul(*argv, NULL, 0);
		}
		else {
			taf_def->prio = 0;
		}
		/* final argument (not current used for anything), assumed numeric */
		argv++;

		if (*argv && (*argv[0] != '-')) {
			taf_def->misc = strtoul(*argv, NULL, 0);
		}
		else {
			taf_def->misc = 0;
		}

		err = wlu_iovar_setbuf(wl, cmd->name, taf_def, paramlen, taf_def, WLC_IOCTL_SMLEN);
	}

	return err;
}

/* Returns the matching config table entry from the wl_config_iovar_list for the passed config
 * iovar. If no matches are found, then returns the default (last) entry from the list
 */
static wl_config_iovar_t *get_config_iovar_entry(const char *iovar_name)
{
	int i = 0;

	while (wl_config_iovar_list[i].iovar_name) {
		if (!stricmp(iovar_name, wl_config_iovar_list[i].iovar_name))
			break;
		i++;
	}

	return &(wl_config_iovar_list[i]);
}

/* Print function for config iovar.
 */
static void wl_bcm_config_print(wl_config_iovar_t *cfg_iovar, wl_config_t *cfg)
{
	char *status_str = NULL;
	int i = 0;
	char *autostr = (cfg->config == (uint32) AUTO) ? "auto" : "";

	while (cfg_iovar->params[i].name) {
		if (cfg_iovar->params[i].value == cfg->status) {
			status_str = cfg_iovar->params[i].name;
			break;
		}
		i++;
	}

	if (status_str) {
		printf("%s %d %s\n", status_str, cfg->status, autostr);
	} else {
		/* No matching entry found in the table. Just print the value received from
		   the driver
		*/
		printf("%d %s\n", cfg->status, autostr);
	}
}

int
wl_bcm_config(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	int i = 0;
	wl_config_iovar_t *config_iovar;

	/* Get the config entry corresponding to this iovar */
	config_iovar = get_config_iovar_entry((char *)cmd->name);

	if (!config_iovar)
		return BCME_ERROR;

	if (*++argv == NULL) {
		/* Get */
		wl_config_t *cfg;
		void *ptr = NULL;

		if ((err = wlu_var_getbuf_sm(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;

		cfg = (wl_config_t *) ptr;
		cfg->config = dtoh32(cfg->config);
		cfg->status = dtoh32(cfg->status);

		/* Call the iovar's print function */
		config_iovar->pfunc(config_iovar, cfg);
	} else {
		/* Set */
		char *param = *argv++;
		bool found = 0;
		wl_config_t cfg;
		i = 0;

		/* Check if the passed param exist in the config_iovar table */
		while (config_iovar->params[i].name) {
			if (!stricmp(config_iovar->params[i].name, param)) {
				cfg.config = config_iovar->params[i].value;
				found = 1;
				break;
			}
			i++;
		}

		if (!found) {
			/* Check if an integer value is passed as the param */
			char *endptr = NULL;
			cfg.config = (uint32) strtol(param, &endptr, 0);
			if (*endptr == '\0')
				found = 1;
		}
		if (!found) {
			printf("Unsupported parameter [%s]\n", param);
			return -1;
		}

		cfg.config = htod32(cfg.config);

		err = wlu_var_setbuf(wl, cmd->name, &cfg, sizeof(wl_config_t));
	}
	return err;
}

/* Set or Get the "desired_bssid" ioctl
 */
static int
wl_desired_bssid(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr ea;
	int error = BCME_OK;

	UNUSED_PARAMETER(cmd);
	argv++;

	if (*argv == NULL) {
		if ((error = wlu_get(wl, WLC_GET_DESIRED_BSSID, &ea, ETHER_ADDR_LEN)) < 0) {
			return error;
		}
		printf("%s\n", wl_ether_etoa(&ea));
	} else {
		if (!wl_ether_atoe(*argv, &ea))
			return BCME_USAGE_ERROR;

		error = wlu_set(wl, WLC_SET_DESIRED_BSSID, &ea, ETHER_ADDR_LEN);
	}
	return error;
}

#if defined(BCMDBG)
static int wl_dump_modesw_dyn_bwsw(void *wl, cmd_t *cmd, char **argv)
{
	char *ptr;
	int err = 0;

	if (*++argv != NULL) {
		return BCME_UNSUPPORTED;
	}

	if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0,
		buf, WLC_IOCTL_MAXLEN)) < 0) {
		return err;
	}

	ptr = (char *) buf;
	fputs(ptr, stdout);

	return err;
}
#endif // endif

static int
wl_dfs_channel_forced(void *wl, cmd_t *cmd, char **argv)
{
	uint i;
	int err = -1;
	char *p;
	chanspec_t chanspec;
	wl_dfs_forced_t *dfs_frcd;
	wl_dfs_forced_t inp;
	uint32 ioctl_size;

	dfs_frcd = (wl_dfs_forced_t *)buf;
	memset(buf, 0, WL_DFS_FORCED_PARAMS_MAX_SIZE);
	memset(&inp, 0, sizeof(wl_dfs_forced_t));

	/* Get the existing configuration first. We need this for
	 * set operations also
	 */
	inp.version = DFS_PREFCHANLIST_VER;
	if ((err = wlu_iovar_getbuf(wl, cmd->name, &inp, sizeof(wl_dfs_forced_t),
		dfs_frcd, WL_DFS_FORCED_PARAMS_MAX_SIZE)) < 0) {
		return err;
	}

	if (!argv[1]) {
		/* Get Operation */
		char chanbuf[CHANSPEC_STR_LEN];
		/* List configuration shouldn't be there if we are operating on
		 * old version of driver
		 */
		if ((dfs_frcd->version == DFS_PREFCHANLIST_VER) && (dfs_frcd->chspec_list.num)) {
			printf("DFS Preferred channel list:: \n");
			for (i = 0; i < dfs_frcd->chspec_list.num; i++) {
				chanspec =
					wl_chspec32_from_driver(dfs_frcd->chspec_list.list[i]);
				/* wf_chspec_ntoa will return NULL when N mode is disabled */
				if (wf_chspec_ntoa(chanspec, chanbuf))
					printf("%s(0x%x) ", chanbuf, chanspec);
				else
					printf("(0x%x) ", chanspec);
			}
			printf("\n");
		} else {
			if (dfs_frcd->version == DFS_PREFCHANLIST_VER) {
				/* Not configured since new driver works on list */
				printf("DFS Preferred Channel:: 0x0 (None)\n");
			} else {
				chanspec = wl_chspec32_from_driver(dfs_frcd->chspec);
				/* wf_chspec_ntoa will return NULL when N mode is disabled */
				if (chanspec && wf_chspec_ntoa(chanspec, chanbuf)) {
					printf("DFS Preferred Channel:: %s (0x%x)\n",
							chanbuf, chanspec);
				} else {
					printf("DFS Preferred Channel:: 0x%x\n", chanspec);
				}
			}
		}
		return err;
	}
	if (!strcmp(argv[1], "-l")) {
		/* List configuration */
		if (!argv[2]) {
			printf("Please provide channel list\n");
			err = BCME_USAGE_ERROR;
			return err;
		}

		if (dfs_frcd->version != DFS_PREFCHANLIST_VER) {
			printf("List Configuration is not supported in this version of driver\n");
			return err;
		}
		p = strtok(argv[2], ", ");
		while (p) {
			if ((*p != '+') && (*p != '-')) {
				printf("channel should be prefixed with +/-\n");
				err = BCME_USAGE_ERROR;
				return err;
			}
			if (!(chanspec = wf_chspec_aton(p + 1))) {
				printf("Invalid channel specified\n");
				err = BCME_USAGE_ERROR;
				return err;
			}
			if (!CHSPEC_IS5G(chanspec)) {
				printf("Invalid channel specified\n");
				err = BCME_USAGE_ERROR;
				return err;
			}
			dfs_frcd->chspec = 0;
			if (*p == '+') {
				/* check if exists */
				for (i = 0; i < dfs_frcd->chspec_list.num; i++) {
					if (chanspec == dfs_frcd->chspec_list.list[i]) {
						printf("Ignoring chanspec 0x%x\n", chanspec);
						goto next_token;
					}
				}
				chanspec = wl_chspec32_to_driver(chanspec);
				dfs_frcd->chspec_list.list[dfs_frcd->chspec_list.num++] = chanspec;
			} else if (*p == '-') {
				/* check if exists */
				for (i = 0; i < dfs_frcd->chspec_list.num; i++) {
					if (chanspec == dfs_frcd->chspec_list.list[i]) {
						dfs_frcd->chspec_list.num--;
						break;
					}
				}
				while (i < dfs_frcd->chspec_list.num) {
					dfs_frcd->chspec_list.list[i] =
						dfs_frcd->chspec_list.list[i+1];
					i++;
				}
			}
next_token:
			p = strtok(NULL, ", ");
		}
		if (dfs_frcd->chspec_list.num > WL_NUMCHANSPECS) {
			printf("Maximum %d channels supported\n", WL_NUMCHANSPECS);
			err = BCME_USAGE_ERROR;
			return err;
		}
		ioctl_size = WL_DFS_FORCED_PARAMS_FIXED_SIZE +
			(dfs_frcd->chspec_list.num * sizeof(chanspec_t));
		dfs_frcd->version = DFS_PREFCHANLIST_VER;
		err = wlu_iovar_set(wl, cmd->name, dfs_frcd, ioctl_size);
	} else {
		/* No list provided. Either single channel or clear list */
		if (dfs_frcd->version == DFS_PREFCHANLIST_VER) {
			/* Clear configuration */
			dfs_frcd->chspec = 0;
			dfs_frcd->chspec_list.num = 0;

			ioctl_size = WL_DFS_FORCED_PARAMS_FIXED_SIZE +
				(dfs_frcd->chspec_list.num * sizeof(chanspec_t));
			err = wlu_iovar_set(wl, cmd->name, dfs_frcd, ioctl_size);
		}
		/* Single channel configuration. Continue as we were doing earlier */
		if (strcmp(argv[1], "0"))
			err = wl_chanspec(wl, cmd, argv);
	}
	return err;
}

static int
wl_setiproute(void *wl, cmd_t *cmd, char **argv)
{
	uint route_tbl_len;
	wlc_ipfo_route_tbl_t *route_tbl = NULL;
	uint32 entries;
	char *endptr;
	uint32 i = 0;
	struct ipv4_addr dipaddr;
	struct ether_addr ea;
	int argc;
	int ret = BCME_OK;
	int buflen = sprintf(buf, "%s", *argv) + 1;

	UNUSED_PARAMETER(cmd);
	argv++;
	route_tbl_len = WL_IPFO_ROUTE_TBL_FIXED_LEN +
		WL_MAX_IPFO_ROUTE_TBL_ENTRY * sizeof(wlc_ipfo_route_entry_t);

	/* allocate the max storage */
	if ((route_tbl = malloc(route_tbl_len)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for route table\n", route_tbl_len);
		return BCME_NOMEM;
	}

	memset(route_tbl, 0, route_tbl_len);

	if (*argv == NULL) {
		if ((ret = wlu_iovar_get(wl, buf, route_tbl, route_tbl_len)) == BCME_OK) {
			if (route_tbl->num_entry == 0) {
				printf("No entries present\n");
			} else {
				for (i = 0; i < route_tbl->num_entry; i++) {
					printf("entry%d", i);
					printf("\t%s",
						wl_iptoa(&route_tbl->route_entry[i].ip_addr));
					printf("\t%s\n",
						wl_ether_etoa(&route_tbl->route_entry[i].nexthop));
				}
			}
		}
	} else {

		argc = ARGCNT(argv);

		if (argc <= 0)
			goto usage;

		entries = strtoul(argv[0], &endptr, 0);

		if (*endptr != '\0')
			goto usage;

		if ((uint32)argc != (entries * 2 + 1))
			goto usage;

		route_tbl->num_entry = entries;
		argv++;

		for (i = 0; i < entries; i++) {
			if (!wl_atoip(argv[i*2], &dipaddr))
				goto usage;

			if (!wl_ether_atoe(argv[(i * 2 + 1)], &ea))
				goto usage;

			memcpy(&route_tbl->route_entry[i].ip_addr, &dipaddr, IPV4_ADDR_LEN);
			memcpy(&route_tbl->route_entry[i].nexthop, &ea, ETHER_ADDR_LEN);
		}
		route_tbl_len = (entries * sizeof(wlc_ipfo_route_entry_t)) + IPV4_ADDR_LEN;
		memcpy(&buf[buflen], route_tbl, route_tbl_len);
		ret = wlu_set(wl, WLC_SET_VAR, &buf[0], buflen + route_tbl_len);
	}

	free(route_tbl);
	return ret;

usage:
	fprintf(stderr, "wrong command format\n");
	if (route_tbl != NULL)
		free(route_tbl);
	return ret;
}

static int
wl_modesw_timecal(void *wl, cmd_t *cmd, char **argv)
{
	int val;
	int err = 0;
	char *ptr = NULL;
	if (*++argv == NULL) {
		/* retrieving the results */
		if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0,
			buf, WLC_IOCTL_MAXLEN)) < 0) {
			return err;
		}
		ptr = buf;
		fputs(ptr, stdout);
	}
	else
	{
		val = htod32(atoi(*argv));
		if ((err = wlu_iovar_setbuf(wl, cmd->name, &val, sizeof(val),
			buf, WLC_IOCTL_MAXLEN)) < 0) {
			return err;
		}
	}
	return err;
}

static int
wl_pcie_bus_throughput_params(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0, opt_err;
	miniopt_t to;
	pcie_bus_tput_params_t *params = NULL;
	pcie_bus_tput_stats_t *stats = NULL;
	UNUSED_PARAMETER(cmd);

	argv++; /* toss the command name */
	if (!*argv) { /* Get the pcie bus throughput stats */
		void *ptr = NULL;
		uint32 tput = 0;

		if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0) {
			fprintf(stderr, "Failed to get stats.\n");
			return err;
		}
		stats = (pcie_bus_tput_stats_t *)ptr;
		if (stats->count) {
			tput = (stats->count * stats->nbytes_per_descriptor) /
				stats->time_taken;
			tput = (tput * 8) / (1024 * 1024); /* convert to Mega bits */
			fprintf(stdout, "Seconds test run %d\nNo of dma completed %d\n"
				"Bytes transfered per dma %d\nBus throughput: %d mbps\n",
				stats->time_taken, stats->count,
				stats->nbytes_per_descriptor, tput);
		}
	}
	if (*argv) { /* Set the bus throughput params */
		params = (pcie_bus_tput_params_t *)malloc(sizeof(pcie_bus_tput_params_t));
		if (params == NULL) {
			fprintf(stderr, "Failed to allocate buffer.\n");
			return BCME_NOMEM;
		}
		memset(params, 0, sizeof(*params));

		miniopt_init(&to, __FUNCTION__, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto fail;
			}
			argv += to.consumed;
			if (to.opt == 'n') {
				if (!to.good_int) {
					fprintf(stderr, "%s: could not parse \"%s\" as an int for"
						" max_dma_descriptors \n", __FUNCTION__, to.valstr);
					err = BCME_BADARG;
					goto fail;
				}
				params->max_dma_descriptors = to.val;
			}
		}

		if (params->max_dma_descriptors == 0)
			params->max_dma_descriptors = 64; /* set default as 64 */

		if ((err = wlu_var_setbuf(wl, cmd->name, params,
			sizeof(*params)) < 0)) {
			fprintf(stderr, "failed to trigger measurement %d\n", err);
		}
	}
fail:
	if (params)
		free(params);
	return err;
}

static int
wl_interface_remove_action(void *wl, cmd_t *cmd, char **argv)
{
	int bsscfg_idx = 0;
	int consumed;
	int error;

	UNUSED_PARAMETER(cmd);

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "interface_remove", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	/*
	 * This command supports both "bss" method and "-i" method. First
	 * check if "bss" options is present, and if yes, use the index
	 * otherwise use the normal path.
	 */
	if (consumed != 0)
		error = wl_bssiovar_set(wl, "interface_remove", bsscfg_idx, NULL, 0);
	else
		error = wlu_var_setbuf(wl, cmd->name, NULL, 0);

	return error;
}

static int
wl_phy_txpwrcap_tbl(void *wl, cmd_t *cmd, char **argv)
{
	wl_txpwrcap_tbl_t txpwrcap_tbl;
	wl_txpwrcap_tbl_t *txpwrcap_tbl_ptr;
	uint8 num_antennas = 0;

	void *ptr = NULL;
	int err = 0;
	uint8 i, j, k;

	if (!(*++argv)) {		/* Get */
		if ((err = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;
		txpwrcap_tbl_ptr = ptr;

		for (i = 0; i < TXPWRCAP_MAX_NUM_CORES; i++) {
			if (txpwrcap_tbl_ptr->num_antennas_per_core[i]) {
				printf("Num Antennas on Core %d = %d\n",
					i, txpwrcap_tbl_ptr->num_antennas_per_core[i]);
				num_antennas += txpwrcap_tbl_ptr->num_antennas_per_core[i];
			}
		}

		printf("Pwr Caps with Cell On:\n");
		for (i = 0; i < num_antennas; i++) {
			printf("\t%d", txpwrcap_tbl_ptr->pwrcap_cell_on[i]);
		}
		printf("\n");

		printf("Pwr Caps with Cell Off:\n");
		for (i = 0; i < num_antennas; i++) {
			printf("\t%d", txpwrcap_tbl_ptr->pwrcap_cell_off[i]);
		}
		printf("\n");

		return err;
	} else { /* Set */
		txpwrcap_tbl.num_antennas_per_core[0] = strtol(*argv, NULL, 0);
		num_antennas = txpwrcap_tbl.num_antennas_per_core[0];

		for (i = 1; i < TXPWRCAP_MAX_NUM_CORES; i++) {
			if (*++argv) {
				txpwrcap_tbl.num_antennas_per_core[i] = strtol(*argv, NULL, 0);
				num_antennas += txpwrcap_tbl.num_antennas_per_core[i];
			} else {
				return BCME_USAGE_ERROR;
			}
		}

		if (*++argv) {
			memset(txpwrcap_tbl.pwrcap_cell_on, 127,
			        TXPWRCAP_MAX_NUM_ANTENNAS *
			        sizeof(txpwrcap_tbl.pwrcap_cell_on[0]));
			memset(txpwrcap_tbl.pwrcap_cell_off, 127,
			        TXPWRCAP_MAX_NUM_ANTENNAS *
			        sizeof(txpwrcap_tbl.pwrcap_cell_off[0]));

			i = 0;
			j = 0;
			k = 0;
			do {
				if (k >= 2 * num_antennas) {
					printf("Entries exceeded max allowed\n");
					return BCME_ERROR;
				}
				if (k >= num_antennas)
					txpwrcap_tbl.pwrcap_cell_off[j++] = strtoul(*argv, NULL, 0);
				else
					txpwrcap_tbl.pwrcap_cell_on[i++] = strtoul(*argv, NULL, 0);
				k++;
			} while (*++argv);

			if ((k != 2* num_antennas) &&
				(k != num_antennas)) {
				printf("Incorrect Number of Entries. Expected %d/%d, Entered %d\n",
					num_antennas, 2 * num_antennas, k);
				return BCME_ERROR;
			}

			if ((err = wlu_var_setbuf(wl, cmd->name, &txpwrcap_tbl,
			        sizeof(txpwrcap_tbl))) < 0) {
				printf("Unable to set the txpwrcaps.\n");
				printf("Check number of antennas for this board.\n");
				return BCME_ERROR;
			}
		}
		else
			return BCME_USAGE_ERROR;

	}
	return err;
}

#ifdef ATE_BUILD
static int
wl_gpaio(void *wl, cmd_t *cmd, char **argv)
{
	char **p = argv;
	int counter = 0;
	wl_gpaio_option_t option;
	BCM_REFERENCE(cmd);
	while (*p) {
		counter++;
		p++;
	}
	if (counter != 2) {
		return USAGE_ERROR;
	}
	if (strcmp("pmu_afeldo", argv[1]) == 0) {
		option = GPAIO_PMU_AFELDO;
	} else if (strcmp("pmu_txldo", argv[1]) == 0) {
		option = GPAIO_PMU_TXLDO;
	} else if (strcmp("pmu_vcoldo", argv[1]) == 0) {
		option = GPAIO_PMU_VCOLDO;
	} else if (strcmp("pmu_lnaldo", argv[1]) == 0) {
		option = GPAIO_PMU_LNALDO;
	} else if (strcmp("pmu_adcldo", argv[1]) == 0) {
		option = GPAIO_PMU_ADCLDO;
	} else if (strcmp("clear", argv[1]) == 0) {
		option = GPAIO_PMU_CLEAR;
	} else {
		return USAGE_ERROR;
	}
	return (wlu_iovar_setint(wl, argv[0], (int)option));
}
#endif /* ATE_BUILD */

static int
wl_macdbg_pmac(void *wl, cmd_t *cmd, char **argv)
{
	wl_macdbg_pmac_param_t pmac;
	int err = BCME_OK;
	char *p, opt;
	char *retbuf;

	memset(&pmac, 0, sizeof(pmac));
	retbuf = malloc(WL_DUMP_BUF_LEN);
	if (retbuf == NULL) {
		printf("No memory to allocate return buffer\n");
		return BCME_NOMEM;
	}

	/* skip the command name */
	argv++;

	/* Get the selection */
	if ((p = *argv) == NULL) {
		err = BCME_USAGE_ERROR;
		goto exit;
	}

	strncpy(pmac.type, p, MIN(strlen(p), sizeof(pmac.type)));
	pmac.type[sizeof(pmac.type) - 1] = '\0';
	/* skip the type */
	argv++;

	pmac.step = (uint8)(-1);
	pmac.num = 0;
	pmac.bitmap = 0;
	pmac.addr_raw = FALSE;
	pmac.w_en = FALSE;

	while ((p = *argv)) {
		argv++;
		if (!strncmp(p, "-", 1)) {
			if (strlen(p) > 2 || (p[1] != 'r' && *argv == NULL)) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			opt = p[1];

			switch (opt) {
				case 's':
					pmac.step = strtol(*argv, NULL, 0);
					argv++;
					break;
				case 'n':
					pmac.num = strtol(*argv, NULL, 0);
					argv++;
					break;
				case 'b':
					pmac.bitmap = strtoul(*argv, NULL, 0);
					argv++;
					break;
				case 'r':
					pmac.addr_raw = TRUE;
					break;
				case 'w':
					pmac.w_val = strtoul(*argv, NULL, 0);
					pmac.w_en = TRUE;
					argv++;
					break;
				default:
					printf("Invalid option!!\n");
					err = BCME_USAGE_ERROR;
					goto exit;
			}
		} else {
			pmac.addr[pmac.addr_num++] = strtol(p, NULL, 0);
			if (pmac.addr_num >= MACDBG_PMAC_ADDR_INPUT_MAXNUM) {
				printf("Reached input limitation!!\n");
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		}
	}

	if ((err = wlu_iovar_getbuf(wl, cmd->name, &pmac,
		sizeof(pmac), retbuf, WL_DUMP_BUF_LEN) < 0)) {
		goto exit;
	}

	if (!pmac.w_en) {
		fputs(retbuf, stdout);
	}
exit:
	if (retbuf) {
		free(retbuf);
	}
	return err;
}

static int wl_mu_rate(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	uint32 i = 0;
	char *endptr = NULL;
	mu_rate_t mu;
	BCM_REFERENCE(cmd);

	memset(&mu, 0x0, sizeof(mu));
	if (!argv[1]) {
		if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL,
		     0, &mu, sizeof(mu))) < 0) {
			fprintf(stderr, "Error reading svmp memory %s %d\n", argv[0], err);
			goto exit;
		}

		for (i = 0; i < 4; i++) {
			printf("0x%04x ", mu.rate_user[i]);
		}
		printf("%s\n", mu.auto_rate ? "(Auto)" : "(Fixed)");
		return err;
	}

	/* auto rate */
	if (!stricmp(argv[1], "auto") || (!stricmp(argv[1], "-1"))) {
		/* turn on auto rate */
		/* wl svmp_mem 0x20060 1 0 */
		mu.auto_rate = 1;
		err = wlu_var_setbuf(wl, cmd->name, &mu, sizeof(mu));
	} else {
		for (i = 0; i < 4; i++) {
			mu.rate_user[i] = 0xffff;
		}

		/* set rates */
		mu.auto_rate = 0;
		for (i = 1; i < 5; i++) {
			if (!argv[i])
				break;

			mu.rate_user[i-1] = strtol(argv[i], &endptr, 0);
		}

		err = wlu_var_setbuf(wl, cmd->name, &mu, sizeof(mu));

		/* barf if set mu_rate but blocked by mu_group */
		if (err != BCME_OK) {
			printf("Set fix rate failed!!!\n");
			printf("Check if blocked by mu_group setting or other error\n");
		}
	}

exit:
	return err;
}

static int wl_mu_group(void *wl, cmd_t *cmd, char **argv)
{
	int m, n, ret = 0;
	int16 temp, k = 0, support_agfsnr = 0;
	int16 *grp_opt_m, *grp_opt_k;

	mu_group_t mu_group;

	mu_group.version = WL_MU_GROUP_PARAMS_VERSION;
	mu_group.forced = WL_MU_GROUP_ENTRY_EMPTY;
	mu_group.forced_group_mcs = WL_MU_GROUP_ENTRY_EMPTY;
	mu_group.forced_group_num = 0;
	mu_group.auto_group_num   = 0;
	mu_group.group_method = WL_MU_GROUP_ENTRY_EMPTY;
	mu_group.group_number = WL_MU_GROUP_ENTRY_EMPTY;
	for (m = 0; m < WL_MU_GROUP_NGROUP_MAX; m++) {
		for (n = 0; n < WL_MU_GROUP_NUSER_MAX; n++) {
			mu_group.group_option[m][n] = WL_MU_GROUP_ENTRY_EMPTY;
		}
		mu_group.group_GID[m] = WL_MU_GROUP_ENTRY_EMPTY;
	}

	if (!argv[1]) {
		/* read mode */
		ret = wlu_iovar_getbuf(wl, cmd->name, NULL, 0, &mu_group, sizeof(mu_group));
		if (ret != 0) {
			goto exit_mu_group;
		}
		if (mu_group.version != WL_MU_GROUP_PARAMS_VERSION) {
			printf("\tIncorrect version "
			    "of mu_group struct: expect %d but get %d\n",
			    WL_MU_GROUP_PARAMS_VERSION, mu_group.version);
			return BCME_BADARG;
		}

		/* group_option can store up to 15 groups info
		 * overload unused group_option to store AGF/SNR
		 *   groups:  group_option[0:(auto_group_num-1)][:]
		 *   check:   group_option[auto_group_num][:]
		 *   agf/snr: group_option[(auto_group_num+1):NGROUP_MAX][:]
		 */
		if (mu_group.auto_group_num <= (WL_MU_GROUP_NGROUP_MAX-2)) {
			k = mu_group.auto_group_num;
			grp_opt_k = mu_group.group_option[k];
			if ((grp_opt_k[0] == WL_MU_GROUP_PARAMS_VERSION) &&
				(grp_opt_k[1] == mu_group.group_method) &&
				(grp_opt_k[2] == mu_group.auto_group_num)) {
					support_agfsnr = grp_opt_k[3];
					k++;
			}
		}
		printf("mu_group: ");
		if (mu_group.forced == WL_MU_GROUP_MODE_FORCED) {
			if (mu_group.forced_group_mcs == WL_MU_GROUP_MODE_FORCED) {
				printf("forced (fixed MCS)\n");
			} else {
				printf("forced (auto MCS)\n");
			}
			for (m = 0; m < mu_group.forced_group_num; m++) {
				grp_opt_m = mu_group.group_option[m];
				printf("  Group %d: ", m);
				for (n = 0; n < WL_MU_GROUP_NUSER_MAX; n++) {
					if (grp_opt_m[n] != WL_MU_GROUP_ENTRY_EMPTY) {
						printf("0x%03x ", grp_opt_m[n]);
					}
				}
				printf("\n");
			}
		} else {
			printf("auto\n");
			printf("VASIP grouping method: ");
			if (mu_group.group_method > 0) {
				printf("new method %d (%s)\n",
					mu_group.group_method, mu_group.group_method_name);
			} else {
				printf("old method ");
				printf("(one group for all admitted users with GID=9)\n");
			}
			printf("      group    number: %d\n", mu_group.group_number);
			if (mu_group.auto_group_num > 0) {
				printf("Latest recommended groups:\n");
			}
			for (m = 0; m < mu_group.auto_group_num; m++) {
				grp_opt_m = mu_group.group_option[m];
				printf("  Group %d: ", m);
				for (n = 0; n < WL_MU_GROUP_NUSER_MAX; n++) {
					if (grp_opt_m[n] != WL_MU_GROUP_ENTRY_EMPTY) {
						printf("0x%03x ", grp_opt_m[n]);
					} else {
						printf(" ---  ");
					}
				}
				printf("(GID=%d)", mu_group.group_GID[m]);
				if (k >= WL_MU_GROUP_NGROUP_MAX) {
					support_agfsnr = 0;
				}
				if (support_agfsnr == 3) {
					grp_opt_k = mu_group.group_option[k];
					printf("  [SNR/AGF:");
					for (n = 0; n < WL_MU_GROUP_NUSER_MAX; n++) {
						if ((grp_opt_m[n] != WL_MU_GROUP_ENTRY_EMPTY) &&
							(grp_opt_k[n] != WL_MU_GROUP_ENTRY_EMPTY)) {
							printf(" %2d/%2d",
								(grp_opt_k[n] & 0x00ff),
								((grp_opt_k[n] >> 8) & 0x00ff));
						} else {
							printf("  --- ");
						}
					}
					printf("]");
					k++;
				} else if (support_agfsnr == 1) {
					grp_opt_k = mu_group.group_option[k];
					printf("  [SNR:");
					for (n = 0; n < WL_MU_GROUP_NUSER_MAX; n++) {
						if ((grp_opt_m[n] != WL_MU_GROUP_ENTRY_EMPTY) &&
							(grp_opt_k[n] != WL_MU_GROUP_ENTRY_EMPTY)) {
							printf(" %2d",
								(grp_opt_k[n] & 0x00ff));
						} else {
							printf(" --");
						}
					}
					printf("]");
					k++;
				}
				printf("\n");
			}

		}
	} else {
		argv++;
		do {
			char *s = *argv++;
			if (!strcmp(s, "-h")) {
				ret = BCME_USAGE_ERROR;
				goto exit_mu_group;
			} else if (!strcmp(s, "-m")) {
				mu_group.group_method = (int16)strtol(*argv++, NULL, 0);
				if (mu_group.group_method < WL_MU_GROUP_METHOD_MIN) {
					ret = BCME_USAGE_ERROR;
					printf("Incorrect -m: M<%d\n", WL_MU_GROUP_METHOD_MIN);
					goto exit_mu_group;
				}
			} else if (!strcmp(s, "-n")) {
				mu_group.group_number = (int16)strtol(*argv++, NULL, 0);
				if ((mu_group.group_number < WL_MU_GROUP_NUMBER_AUTO_MIN) ||
						(mu_group.group_number >
							WL_MU_GROUP_NUMBER_AUTO_MAX)) {
					ret = BCME_USAGE_ERROR;
					printf("Incorrect '-n': N is not in the range %d~%d\n",
						WL_MU_GROUP_NUMBER_AUTO_MIN,
						WL_MU_GROUP_NUMBER_AUTO_MAX);
					goto exit_mu_group;
				}
			} else if (!strcmp(s, "-f")) {
				temp = (int)strtol(*argv++, NULL, 0);
				if ((temp == WL_MU_GROUP_MODE_AUTO) ||
						(temp == WL_MU_GROUP_MODE_FORCED)) {
					mu_group.forced_group_mcs = temp;
				}
			} else if ((!strcmp(s, "-g")) && (mu_group.forced != 0)) {
				temp = (int)strtol(*argv++, NULL, 0);
				if (temp == WL_MU_GROUP_AUTO_COMMAND) {
					mu_group.forced = WL_MU_GROUP_MODE_AUTO;
					mu_group.forced_group_num = 0;
				} else if ((temp < WL_MU_GROUP_NUMBER_FORCED_MAX) &&
						(temp == mu_group.forced_group_num)) {
					mu_group.forced = WL_MU_GROUP_MODE_FORCED;
					mu_group.forced_group_num += 1;
					m = temp;
					n = 0;
				} else {
					ret = BCME_USAGE_ERROR;
					printf("Incorrect to set froced group options: ");
					if (temp != mu_group.forced_group_num) {
						printf("group index should be successive\n");
					}
					if (mu_group.forced_group_num >=
							WL_MU_GROUP_NUMBER_FORCED_MAX) {
						printf("support up to %d forced options\n",
							WL_MU_GROUP_NUMBER_FORCED_MAX);
					}
					goto exit_mu_group;
				}
			} else if (((mu_group.forced_group_num > 0)) && (n < 4)) {
				mu_group.group_option[m][n] = (int16)strtol(s, NULL, 0);
				n += 1;
			}
		} while (*argv);

		if ((mu_group.forced == WL_MU_GROUP_MODE_FORCED) &&
				((mu_group.group_method != WL_MU_GROUP_ENTRY_EMPTY) ||
				(mu_group.group_number != WL_MU_GROUP_ENTRY_EMPTY))) {
			ret = BCME_USAGE_ERROR;
			printf("Incorrect to set forced grouping options "
					"with auto grouping parameters\n");
			goto exit_mu_group;
		}

		/* default is forced mcs for forced grouping */
		/* if not specified forced_group_mcs when forced grouping, set forced_group_mcs=1 */
		if ((mu_group.forced == WL_MU_GROUP_MODE_FORCED) &&
				(mu_group.forced_group_mcs == WL_MU_GROUP_ENTRY_EMPTY)) {
			mu_group.forced_group_mcs = WL_MU_GROUP_MODE_FORCED;
		}

		/* set mode */
		ret = wlu_var_setbuf(wl, cmd->name, &mu_group, sizeof(mu_group));
	}

exit_mu_group:
	return ret;
}

static int wl_ded_setup(void *wl, cmd_t *cmd, char **argv)
{
	dynamic_ed_setup_t setup;
	int err = -1;
	int16 val;

	memset(&setup, 0, sizeof(dynamic_ed_setup_t));
	if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0,
			&setup, sizeof(dynamic_ed_setup_t))) < 0) {
		return err;
	}

	if (!argv[1]) {
		/* read mode */
		printf("Current Dynamic ED settings:\n");
		printf("  Monitoring window: %d seconds \n", setup.ed_monitor_window);
		printf("  DISABLE SED: %d \n", setup.sed_dis);
		printf("  Min SED: %d \n", setup.sed_lower_bound);
		printf("  Max SED: %d \n", setup.sed_upper_bound);
		printf("  Max ED Threshold: %d \n", setup.ed_th_high);
		printf("  Min ED Threshold: %d \n", setup.ed_th_low);
		printf("  Threshold increment step size: %d \n", setup.ed_inc_step);
		printf("  Threshold decrement step size: %d \n", setup.ed_dec_step);
	} else {
		argv++;
		do {
			char *s = *argv++;
			if (!strcmp(s, "-h")) {
				err = BCME_USAGE_ERROR;
				goto exit_ded_setup;
			} else if (!strcmp(s, "-win")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				setup.ed_monitor_window = val;
			} else if (!strcmp(s, "-seddis")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				if ((val >= DYN_ED_MIN_SED) && (val <= DYN_ED_MAX_SED)) {
					setup.sed_dis = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
			} else if (!strcmp(s, "-minsed")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				if ((val >= DYN_ED_MIN_SED) && (val <= DYN_ED_MAX_SED)) {
					setup.sed_lower_bound = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
			} else if (!strcmp(s, "-maxsed")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				if ((val >= DYN_ED_MIN_SED) && (val <= DYN_ED_MAX_SED)) {
					setup.sed_upper_bound = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
			} else if (!strcmp(s, "-maxth")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				if ((val >= DYN_ED_MIN_TH) && (val <= DYN_ED_MAX_TH)) {
					setup.ed_th_high = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
			} else if (!strcmp(s, "-minth")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				if ((val >= DYN_ED_MIN_TH) && (val <= DYN_ED_MAX_TH)) {
					setup.ed_th_low = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
			} else if (!strcmp(s, "-inc")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				if (val > 0) {
					setup.ed_inc_step = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
			} else if (!strcmp(s, "-dec")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
				val = strtol(*argv++, NULL, 0);
				if (val > 0) {
					setup.ed_dec_step = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_ded_setup;
				}
			} else {
				err = BCME_USAGE_ERROR;
				goto exit_ded_setup;
			}
		} while (*argv);

		/* set mode */
		err = wlu_var_setbuf(wl, cmd->name, &setup, sizeof(setup));
	}

exit_ded_setup:
	return err;
}

static int wl_mu_policy(void *wl, cmd_t *cmd, char **argv)
{
	mu_policy_t policy;
	int err = -1;
	uint32 val;

	memset(&policy, 0, sizeof(mu_policy_t));
	policy.version = WL_MU_POLICY_PARAMS_VERSION;
	policy.length = sizeof(mu_policy_t);
	if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0, &policy, sizeof(mu_policy_t))) < 0) {
		return err;
	}
	/* Check the iovar version */
	if (policy.version != WL_MU_POLICY_PARAMS_VERSION) {
		err = BCME_BADARG;
		goto exit_mu_policy;
	}
	if (!argv[1]) {
		/* read mode */
		printf("Current MU policy settings:\n");
		printf("  scheduler: %s", policy.sched_timer? "ON":"OFF");
		if (policy.sched_timer)
			printf(", timer: %u seconds\n", policy.sched_timer);
		else
			printf(" \n");
		printf("  performance monitors: %s\n", policy.pfmon? "ON":"OFF");
		printf("  gpos performance monitors: %s\n", policy.pfmon_gpos? "ON":"OFF");
		printf("  forced the same BW check: %s\n", policy.samebw? "ON":"OFF");
		printf("  max number of rx streams in the clients: %u\n", policy.nrx);
		printf("  max number of admitted clients: %u\n", policy.max_muclients);
	} else {
		argv++;
		do {
			char *s = *argv++;
			if (!strcmp(s, "-h")) {
				err = BCME_USAGE_ERROR;
				goto exit_mu_policy;
			} else if (!strcmp(s, "-sched_timer")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
				val = strtoul(*argv++, NULL, 0);
				policy.sched_timer = val;
			} else if (!strcmp(s, "-pfmon")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
				val = strtoul(*argv++, NULL, 0);
				if ((val == WL_MU_POLICY_DISABLED) ||
					(val == WL_MU_POLICY_ENABLED)) {
					policy.pfmon = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
			} else if (!strcmp(s, "-pfmon_gpos")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
				val = strtoul(*argv++, NULL, 0);
				if ((val == WL_MU_POLICY_DISABLED) ||
					(val == WL_MU_POLICY_ENABLED)) {
					policy.pfmon_gpos = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
			} else if (!strcmp(s, "-samebw")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
				val = strtoul(*argv++, NULL, 0);
				if ((val == WL_MU_POLICY_DISABLED) ||
					(val == WL_MU_POLICY_ENABLED)) {
					policy.samebw = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
			} else if (!strcmp(s, "-nrx")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
				val = strtoul(*argv++, NULL, 0);
				if ((val >= WL_MU_POLICY_NRX_MIN) &&
					(val <= WL_MU_POLICY_NRX_MAX)) {
					policy.nrx = val;
				} else {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
			} else if (!strcmp(s, "-max_muclients")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto exit_mu_policy;
				}
				val = strtoul(*argv++, NULL, 0);
				policy.max_muclients = val;
			}
		} while (*argv);

		/* set mode */
		err = wlu_var_setbuf(wl, cmd->name, &policy, sizeof(policy));
	}

exit_mu_policy:
	return err;
}

static int
wl_muinfo(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	char *dump_buf;
	uint8 option = 0;

	if (cmd->get < 0)
		return -1;

	dump_buf = malloc(WL_DUMP_BUF_LEN);
	if (dump_buf == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n", WL_DUMP_BUF_LEN);
		return BCME_NOMEM;
	}
	memset(dump_buf, 0, WL_DUMP_BUF_LEN);

	if (argv[1]) {
		argv++;
		do {
			char *s = *argv++;
			if (!strcmp(s, "-h")) {
				err = BCME_USAGE_ERROR;
				goto exit_muinfo;
			} else if (!strcmp(s, "-v")) {
				option = 1;
			}
		} while (0);
	}

	err = wlu_iovar_getbuf(wl, cmd->name, &option, sizeof(uint8), dump_buf, WL_DUMP_BUF_LEN);
	if (!err) {
		fputs(dump_buf, stdout);
	}
exit_muinfo:
	free(dump_buf);

	return err;
}

static int wl_muclients(void *wl, cmd_t *cmd, char **argv)
{
	max_clients_t m;
	int err = -1;
	uint32 val;

	memset(&m, 0, sizeof(max_clients_t));
	m.version = WL_MU_CLIENTS_PARAMS_VERSION;
	m.length = sizeof(max_clients_t);
	if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0, &m, sizeof(max_clients_t))) < 0) {
		return err;
	}

	/* Check the iovar version */
	if (m.version != WL_MU_CLIENTS_PARAMS_VERSION) {
		err = BCME_BADARG;
		goto done;
	}

	if (!argv[1]) {
		/* read mode */
		printf("Maximum allowed MU users:\n");
		printf("VHT MU-MIMO :%d HE MU-MIMO :%d HE DL-OFDMA :%d"
		       " HE UL-OFDMA :%d\n", m.vhtmu, m.hemmu, m.dlofdma, m.ulofdma);
	} else {
		argv++;
		do {
			char *s = *argv++;
			if (!strcmp(s, "-h")) {
				err = BCME_USAGE_ERROR;
				goto done;
			} else if (!strcmp(s, "-vhtmu")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto done;
				}
				val = strtoul(*argv++, NULL, 0);
				m.vhtmu = val;
			} else if (!strcmp(s, "-hemmu")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto done;
				}
				val = strtoul(*argv++, NULL, 0);
				m.hemmu = val;
			} else if (!strcmp(s, "-dlofdma")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto done;
				}
				val = strtoul(*argv++, NULL, 0);
				m.dlofdma = val;
			} else if (!strcmp(s, "-ulofdma")) {
				if (!*argv) {
					err = BCME_USAGE_ERROR;
					goto done;
				}
				val = strtoul(*argv++, NULL, 0);
				m.ulofdma = val;
			}
		} while (*argv);

		/* set mode */
		err = wlu_var_setbuf(wl, cmd->name, &m, sizeof(m));
	}

done:
	return err;
}

static int wl_svmp_mem(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	svmp_mem_t mem;
	uint16 *svmp_buf;
	uint16 i;
	char *endptr = NULL;
	int nbit, ncol, dispmode, k;
	char hexprefix[] = "0x";

	if (!argv[1]) {
		fprintf(stderr, "Too few arguments\n");
		return BCME_USAGE_ERROR;
	}

	mem.addr = strtol(argv[1], &endptr, 0);

	if (!argv[2]) {
		fprintf(stderr, "%s: Length must be specified\n", cmd->name);
		err = BCME_USAGE_ERROR;
		goto exit;
	}
	mem.len = strtol(argv[2], &endptr, 0);

	nbit = 32;
	ncol = 1;
	dispmode = 1;
	k = 3;
	while (argv[k]) {
		char *s = argv[k];
		if (*argv == NULL) {
			break;
		}
		if (!strcmp(s, "-b")) { /* display bitwidth */
			k++;
			nbit = (uint8)strtol(argv[k], NULL, 0);
			if ((nbit != 16) && (nbit != 32)) {
				//nbit = 32;
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		} else if (!strcmp(s, "-c")) { /* number of column in a row */
			k++;
			ncol = (uint8)strtol(argv[k], NULL, 0);
			if ((ncol < 1) || (ncol > 16)) {
				//ncol = 1;
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		} else if (!strcmp(s, "-m")) { /* display mode */
			k++;
			dispmode = (uint8)strtol(argv[k], NULL, 0);
			if ((dispmode < 0) || (dispmode > 3)) {
				//dispmode = 0;
				err = BCME_USAGE_ERROR;
				goto exit;
			}
		} else if (k == 3) {
			mem.val = strtol(argv[k], &endptr, 0);
			err = wlu_var_setbuf(wl, cmd->name, &mem, sizeof(mem));
			goto exit;
		}
		k++;
	} /* while (argv[k]) for k>=3 */

	if (((dispmode == 3) && ((mem.addr & 0x1) || (mem.len & 0x1))) ||
			((nbit == 32) && (mem.len > 1) && (mem.addr & 0x1))) {
		err = BCME_USAGE_ERROR;
		goto exit;
	}

	if ((err = wlu_iovar_getbuf(wl, cmd->name, &mem,
		sizeof(mem), buf, WLC_IOCTL_MAXLEN)) < 0) {
		fprintf(stderr, "Error reading svmp memory %s %d\n", argv[0], err);
		goto exit;
	}

	svmp_buf = (uint16 *)buf;

	k = 0;
	for (i = 0; i < mem.len; ) {
		if (dispmode == 3) {
			printf("%6d%+6d*j ", svmp_buf[i], svmp_buf[i+1]);
			i += 2;
		} else if (dispmode == 2) {
			if (nbit == 16) {
				printf("%6d", svmp_buf[i]);
				i += 1;
			} else { /* (nbit == 32) */
				if (i < (mem.len - 1)) {
					printf("%10d", svmp_buf[i] +
						(((int32)svmp_buf[i+1]) << 16));
					i += 2;
				} else {
					printf("%6d", svmp_buf[i]);
					i += 1;
				}
			}
		} else { /* dispmode == 1 or 2 */
			if (nbit == 16) {
				printf("%.*s%04x", (dispmode<<1), hexprefix, svmp_buf[i]);
				i += 1;
			} else { /* (nbit == 32) */
				if (i < (mem.len - 1)) {
					printf("%.*s%04x%04x", (dispmode<<1), hexprefix,
						svmp_buf[i+1], svmp_buf[i]);
					i += 2;
				} else {
					printf("%.*s%04x", (dispmode<<1), hexprefix, svmp_buf[i]);
					i += 1;
				}
			}
		}
		if (((k % ncol) == (ncol - 1)) || (i == mem.len)) {
			printf("\n");
		} else {
			printf(" ");
		}
		k++;
	}
	printf("\n");

exit:
	return err;
}

#if defined(BCMDBG)
static void
wl_svmp_sampcol_printf_configs(wl_svmp_sampcol_t *psampcol, uint16 mode)
{
	char *str_phy1mux[] = {"gpioOut", "fftOut", "dbgHx", "rx1mux"};
	char *str_rx1mux[] = {"farrowOut", "iqCompOut", "dcFilterOut",
	                          "rxFilterOut", "aciFilterOut"};
	char *str_packmode[] = {"dual", "4-cores", "2-cores", "single-core"};
	char *str_packorder[] = {"big", "little"};

	printf("version of svmp sample collect params: %d\n", psampcol->version);
	printf("  enable:     %d\n", psampcol->enable);
	printf("  trigger:    %d", psampcol->trigger_mode);
	if (psampcol->trigger_mode == SVMP_SAMPCOL_TRIGGER_PKTPROC_TRANSITION) {
		printf(", %d -> %d\n", psampcol->trigger_mode_s[0], psampcol->trigger_mode_s[1]);
	} else {
		printf("\n");
	}
	printf("  waitcnt:    %d\n", psampcol->waitcnt);
	printf("  caplen:     %d\n", psampcol->caplen);
	printf("  samplerate: %dx\n", (psampcol->data_samplerate + 1));
	printf("  data:       %s", str_phy1mux[psampcol->data_sel_phy1]);
	if (psampcol->data_sel_phy1 == SVMP_SAMPCOL_PHY1MUX_RX1MUX) {
		printf(" with %s\n",
		           str_rx1mux[psampcol->data_sel_rx1-SVMP_SAMPCOL_RX1MUX_FARROWOUT]);
	} else {
		printf("\n");
	}
	printf("  dualcap:    ");
	if (psampcol->data_sel_dualcap >= SVMP_SAMPCOL_RX1MUX_FARROWOUT) {
		printf("%s\n",
		           str_rx1mux[psampcol->data_sel_dualcap-SVMP_SAMPCOL_RX1MUX_FARROWOUT]);
	} else {
		printf("OFF\n");
	}
	printf("  pack:       %s, %s-endian",
	           str_packmode[psampcol->pack_mode], str_packorder[psampcol->pack_order]);
	if (psampcol->pack_mode == SVMP_SAMPCOL_PACK_1CORE) {
		printf(" core%d\n", psampcol->pack_1core_sel);
	} else {
		printf("\n");
	}
	printf("  buf_addr:   0x%05x~0x%05x\n", psampcol->buff_addr_start, psampcol->buff_addr_end);
	if (mode == 0) {
		printf("  status:     overflow=%d, running=%d\n",
		           psampcol->status&0x1, (psampcol->status&0x2)>>1);
	}
}

static int
wl_svmp_sampcol(void *wl, cmd_t *cmd, char **argv)
{
	int n;
	int ret;

	wl_svmp_sampcol_t sampcol;

	uint16 cmd_nparam;
	uint16 cmd_enable;

	wlc_rev_info_t revinfo;
	uint32 phytype;
	uint32 phyrev;

	memset(&revinfo, 0, sizeof(revinfo));
	if ((ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return ret;

	phytype = dtoh32(revinfo.phytype);
	phyrev = dtoh32(revinfo.phyrev);

	if ((phytype != WLC_PHY_TYPE_AC) || ((phyrev != 32) && (phyrev != 33))) {
		return BCME_USAGE_ERROR;
	}

	sampcol.version           = WL_SVMP_SAMPCOL_PARAMS_VERSION;
	sampcol.enable            = 0;
	sampcol.trigger_mode      = SVMP_SAMPCOL_TRIGGER_PKTPROC_TRANSITION;
	sampcol.trigger_mode_s[0] = SVMP_SAMPCOL_PKTPROC_OFDM_PHY;
	sampcol.trigger_mode_s[1] = SVMP_SAMPCOL_PKTPROC_TIMING_SEARCH;
	sampcol.waitcnt           = 0;
	sampcol.caplen            = 128;
	sampcol.data_samplerate   = SVMP_SAMPCOL_SAMPLERATE_2XBW;
	sampcol.data_sel_phy1     = SVMP_SAMPCOL_PHY1MUX_RX1MUX;
	sampcol.data_sel_rx1      = SVMP_SAMPCOL_RX1MUX_FARROWOUT;
	sampcol.data_sel_dualcap  = 0;
	sampcol.pack_mode         = SVMP_SAMPCOL_PACK_4CORE;
	sampcol.pack_order        = 0;
	sampcol.pack_cfix_fmt     = 0;
	sampcol.pack_1core_sel    = 0;
	sampcol.buff_addr_start   = 0x25800;
	sampcol.buff_addr_end     = 0x26800;
	sampcol.int2vasip         = 1;

	ret = -1;
	cmd_nparam = 0;
	cmd_enable = 0;

	argv++;
	while (*argv) {
		char *s = *argv++;
		if (*argv == NULL) {
			ret = BCME_USAGE_ERROR;
			goto exit_svmp_sampcol;
		}
		if (!strcmp(s, "-h")) {
			ret = BCME_USAGE_ERROR;
			goto exit_svmp_sampcol;
		} else if (!strcmp(s, "-e")) {
			cmd_nparam++;
			sampcol.enable = (uint8)strtol(*argv++, NULL, 0);
			cmd_enable = 1;
		} else if (!strcmp(s, "-t")) {
			cmd_nparam++;
			sampcol.trigger_mode = (uint8)strtol(*argv++, NULL, 0);
			if (sampcol.trigger_mode == SVMP_SAMPCOL_TRIGGER_PKTPROC_TRANSITION) {
				for (n = 0; n < 2; n++) {
					if (*argv == NULL) {
						ret = BCME_USAGE_ERROR;
						goto exit_svmp_sampcol;
					} else {
						sampcol.trigger_mode_s[n]
						    = (uint8)strtol(*argv++, NULL, 0);
					}
				}
			}
		} else if (!strcmp(s, "-w")) {
			cmd_nparam++;
			sampcol.waitcnt = (uint16)strtol(*argv++, NULL, 0);
		} else if (!strcmp(s, "-l")) {
			cmd_nparam++;
			sampcol.caplen = (uint16)strtol(*argv++, NULL, 0);
		} else if (!strcmp(s, "-s")) {
			cmd_nparam++;
			sampcol.data_sel_phy1 = (uint8)strtol(*argv++, NULL, 0);
			if ((sampcol.data_sel_phy1 == SVMP_SAMPCOL_PHY1MUX_RX1MUX) &&
			        strncmp(*argv, "-", 1)) {
				sampcol.data_sel_rx1 = (uint8)strtol(*argv++, NULL, 0);
			}
		} else if (!strcmp(s, "-d")) {
			cmd_nparam++;
			sampcol.data_sel_dualcap = (uint8)strtol(*argv++, NULL, 0);
		} else if (!strcmp(s, "-r")) {
			cmd_nparam++;
			sampcol.data_samplerate = (uint8)strtol(*argv++, NULL, 0);
		} else if (!strcmp(s, "-p")) {
			cmd_nparam++;
			sampcol.pack_mode     = (uint8)strtol(*argv++, NULL, 0);
			if (strncmp(*argv, "-", 1)) {
				sampcol.pack_order    = (uint8)strtol(*argv++, NULL, 0);
			}
			if (strncmp(*argv, "-", 1)) {
				sampcol.pack_cfix_fmt = (uint8)strtol(*argv++, NULL, 0);
			}
			if ((phytype == WLC_PHY_TYPE_AC) && (phyrev == 33) &&
			        (sampcol.pack_mode == SVMP_SAMPCOL_PACK_1CORE)) {
				if (strncmp(*argv, "-", 1)) {
					sampcol.pack_1core_sel = (uint8)strtol(*argv++, NULL, 0);
				}
			}
		} else if (!strcmp(s, "-a")) {
			cmd_nparam++;
			sampcol.buff_addr_start = (uint32)strtol(*argv++, NULL, 0);
			if (strncmp(*argv, "-", 1)) {
				sampcol.buff_addr_end   = (uint32)strtol(*argv++, NULL, 0);
			}
		} else {
			argv++;
		}
	}

	if ((cmd_nparam == 0) || ((cmd_enable == 1) && (cmd_nparam == 1))) {
		cmd_enable = sampcol.enable;
		ret = wlu_iovar_get(wl, cmd->name, &sampcol, sizeof(wl_svmp_sampcol_t));
		if (sampcol.version != WL_SVMP_SAMPCOL_PARAMS_VERSION) {
			printf("\tIncorrect version "
			"of SVMP_SAMPCOL_PARAMS struct: expect %d but get %d\n",
			WL_SVMP_SAMPCOL_PARAMS_VERSION, sampcol.version);
			return BCME_BADARG;
		}
		if (cmd_nparam == 1) {
			sampcol.enable = cmd_enable;
		} else {
			wl_svmp_sampcol_printf_configs(&sampcol, 0);
		}
	}
	if (cmd_nparam > 0) {
		wl_svmp_sampcol_printf_configs(&sampcol, 1);
		ret = wlu_var_setbuf(wl, cmd->name, &sampcol, sizeof(wl_svmp_sampcol_t));
	}

exit_svmp_sampcol:
	return ret;
}
#endif // endif

static int
wl_scanmac(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	char *subcmd;
	int subcmd_len;

	/* skip iovar */
	argv++;

	/* must have subcommand */
	subcmd = *argv++;
	if (!subcmd) {
		return BCME_USAGE_ERROR;
	}
	subcmd_len = strlen(subcmd);

	if (!*argv) {
		/* get */
		uint8 buffer[OFFSETOF(wl_scanmac_t, data)];
		wl_scanmac_t *sm = (wl_scanmac_t *)buffer;
		int len = OFFSETOF(wl_scanmac_t, data);

		memset(sm, 0, len);
		if (!strncmp(subcmd, "enable", subcmd_len)) {
			sm->subcmd_id = WL_SCANMAC_SUBCMD_ENABLE;
		} else if (!strncmp(subcmd, "bsscfg", subcmd_len)) {
			sm->subcmd_id = WL_SCANMAC_SUBCMD_BSSCFG;
		} else if (!strncmp(subcmd, "config", subcmd_len)) {
			sm->subcmd_id = WL_SCANMAC_SUBCMD_CONFIG;
		} else {
			return BCME_USAGE_ERROR;
		}

		/* invoke GET iovar */
		sm->subcmd_id = dtoh16(sm->subcmd_id);
		sm->len = dtoh16(sm->len);
		if ((err = wlu_iovar_getbuf(wl, cmd->name, sm, len, buf, WLC_IOCTL_SMLEN)) < 0) {
			return err;
		}

		/* process and print GET results */
		sm = (wl_scanmac_t *)buf;
		sm->subcmd_id = dtoh16(sm->subcmd_id);
		sm->len = dtoh16(sm->len);

		switch (sm->subcmd_id) {
		case WL_SCANMAC_SUBCMD_ENABLE:
		{
			wl_scanmac_enable_t *sm_enable = (wl_scanmac_enable_t *)sm->data;
			if (sm->len >= sizeof(*sm_enable)) {
				printf("%d\n", sm_enable->enable);
			} else {
				err = BCME_BADLEN;
			}
			break;
		}
		case WL_SCANMAC_SUBCMD_BSSCFG:
		{
			wl_scanmac_bsscfg_t *sm_bsscfg = (wl_scanmac_bsscfg_t *)sm->data;
			if (sm->len >= sizeof(*sm_bsscfg)) {
				sm_bsscfg->bsscfg = dtoh32(sm_bsscfg->bsscfg);
				printf("%d\n", sm_bsscfg->bsscfg);
			} else {
				err = BCME_BADLEN;
			}
			break;
		}
		case WL_SCANMAC_SUBCMD_CONFIG:
		{
			wl_scanmac_config_t *sm_config = (wl_scanmac_config_t *)sm->data;
			if (sm->len >= sizeof(*sm_config)) {
				sm_config->scan_bitmap = dtoh16(sm_config->scan_bitmap);
				printf("mac:         %s\n", wl_ether_etoa(&sm_config->mac));
				printf("random mask: %s\n", wl_ether_etoa(&sm_config->random_mask));
				printf("scan bitmap: 0x%02X\n", sm_config->scan_bitmap);
				if (sm_config->scan_bitmap & WL_SCANMAC_SCAN_UNASSOC) {
					printf("             unassoc\n");
				}
				if (sm_config->scan_bitmap & WL_SCANMAC_SCAN_ASSOC_ROAM) {
					printf("             assoc roam\n");
				}
				if (sm_config->scan_bitmap & WL_SCANMAC_SCAN_ASSOC_PNO) {
					printf("             assoc PNO\n");
				}
				if (sm_config->scan_bitmap & WL_SCANMAC_SCAN_ASSOC_HOST) {
					printf("             assoc host\n");
				}
			} else {
				err = BCME_BADLEN;
			}
			break;
		}
		default:
			break;
		}
	}
	else {
		/* set */
		uint8 buffer[OFFSETOF(wl_scanmac_t, data) +
			MAX(sizeof(wl_scanmac_enable_t), sizeof(wl_scanmac_config_t))];
		wl_scanmac_t *sm = (wl_scanmac_t *)buffer;
		int len = OFFSETOF(wl_scanmac_t, data);

		if (!strncmp(subcmd, "enable", subcmd_len) &&
			(*argv[0] == '0' || *argv[0] == '1')) {
			wl_scanmac_enable_t *sm_enable = (wl_scanmac_enable_t *)sm->data;
			sm->subcmd_id = WL_SCANMAC_SUBCMD_ENABLE;
			sm->len = sizeof(*sm_enable);
			sm_enable->enable = atoi(argv[0]);
		} else if (!strncmp(subcmd, "config", subcmd_len) &&
			argv[0] && argv[1] && argv[2]) {
			wl_scanmac_config_t *sm_config = (wl_scanmac_config_t *)sm->data;
			char *mac = argv[0];
			char *mask = argv[1];
			char *bitmap = argv[2];
			sm->subcmd_id = WL_SCANMAC_SUBCMD_CONFIG;
			sm->len = sizeof(*sm_config);
			if (!wl_ether_atoe(mac, &sm_config->mac) ||
				!wl_ether_atoe(mask, &sm_config->random_mask)) {
				return BCME_USAGE_ERROR;
			}
			sm_config->scan_bitmap = (uint16)strtoul(bitmap, NULL, 0);
			sm_config->scan_bitmap = htod16(sm_config->scan_bitmap);
		} else {
			return BCME_USAGE_ERROR;
		}

		/* invoke SET iovar */
		len = OFFSETOF(wl_scanmac_t, data) + sm->len;
		sm->subcmd_id = htod16(sm->subcmd_id);
		sm->len = htod16(sm->len);
		err = wlu_iovar_set(wl, cmd->name, sm, len);
	}

	return err;
}

#define TCMS_MAX_CORES		(8)
#define TCMS_INPUT_BUFSIZE	(1 + 1 + 1)

static int
wl_get_tcmstbl_entry(void *wl, cmd_t *cmd, char **argv)
{
	int32 wl_tcmsarg[TCMS_INPUT_BUFSIZE] = {0, 0, 0};
	int32* obuf = (int32 *)buf;
	int err, opt_err;
	miniopt_t to;
	const char* fn_name = "wl_get_tcmstbl_entry";
	int argc = 0;

	UNUSED_PARAMETER(cmd);

	/* counting the number of arguments */
	while (argv[argc])
		argc++;

	if (argc < 2)
		return BCME_USAGE_ERROR;

	memset(buf, 0, WLC_IOCTL_MEDLEN);

	/* toss the command name */
	argv++;

	/* Validate arguments if any */
	if (*argv) {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'c') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int for core\n",
						fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if (to.val > (TCMS_MAX_CORES-1)) {
					fprintf(stderr,
						"%s: invalid core %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				wl_tcmsarg[0] = to.val;
			}
			if (to.opt == 'a') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as"
						" an int for antenna map\n",
						fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				wl_tcmsarg[1] = to.val;
			}
			if (to.opt == 's') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as"
						" an int for cell status\n",
						fn_name, to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val < 0) | (to.val > 1)) {
					fprintf(stderr,
						"%s: invalid cell status %d\n",
						fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				wl_tcmsarg[2] = to.val;
			}
		}
	} else {
		err = BCME_BADARG;
		goto exit;
	}

	if ((err = wlu_iovar_getbuf(wl, "tcmstbl", wl_tcmsarg, sizeof(wl_tcmsarg),
			buf, WLC_IOCTL_MEDLEN)) < 0) {
		printf("\n Incorrect option \n");
		err = BCME_BADOPTION;
		goto exit;
	}

	printf("(Nsts {4..1}) { %d hdBm  0x%02x } { %d hdBm  0x%02x }"
		"{ %d hdBm  0x%02x } { %d hdBm  0x%02x }\n",
		obuf[10], obuf[11], obuf[8], obuf[9],
		obuf[6], obuf[7], obuf[4], obuf[5]);
	printf("OFDM { %d hdBm  0x%02x }  CCK { %d hdBm  0x%02x }\n",
		obuf[2], obuf[3], obuf[0], obuf[1]);
exit:
	return err;
}

/*
 *  get/set IPv6 RA rate limit interval
 */
static int
wl_nd_ra_limit_intv(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0, opt_err;
	miniopt_t to;
	nd_ra_ol_limits_t *ra_limit = NULL;

	UNUSED_PARAMETER(cmd);

	if ((err = wlu_var_getbuf(wl,
		"nd_ra_limit_intv", NULL, 0, (void *)&ra_limit) < 0)) {
		printf("nd_ra_limit_intv: getbuf ioctl failed\n");
		return err;
	}

	if (*++argv) {
		/* set */
		if (argv[0][0] == 'h' ||
				argv[0][0] == '?') {
			err = BCME_USAGE_ERROR;
			goto exit;
		}

		ra_limit = calloc(1, sizeof(nd_ra_ol_limits_t));
		if (ra_limit == NULL) {
			return BCME_NOMEM;
		}

		miniopt_init(&to, __FUNCTION__, NULL, FALSE);

		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;
			if (to.opt == 't') {
				if (!to.good_int) {
					fprintf(stderr, "could not parse %s as an int for"
						" type\n", to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				ra_limit->type = to.val;
			}

			if (to.opt == 'p') {
				if (!to.good_int) {
					fprintf(stderr, "could not parse %s as an int for"
						" percentage value\n", to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				ra_limit->limits.lifetime_relative.lifetime_percent = to.val;
			}

			if (to.opt == 'm') {
				if (!to.good_int) {
					fprintf(stderr, "could not parse %s as an int for"
						" min_time\n", to.valstr);
					err = BCME_BADARG;
					goto exit;
				}
				ra_limit->limits.lifetime_relative.min_time = to.val;
			}
		}

		ra_limit->version = ND_RA_OL_LIMITS_VER;

		if (ra_limit->type == ND_RA_OL_LIMITS_REL_TYPE) {
			printf("type %d, lifetime_percent %d, min_time %d\n",
				ra_limit->type,
				ra_limit->limits.lifetime_relative.lifetime_percent,
				ra_limit->limits.lifetime_relative.min_time);
			ra_limit->length = ND_RA_OL_LIMITS_REL_TYPE_LEN;
		} else if (ra_limit->type == ND_RA_OL_LIMITS_FIXED_TYPE) {
			printf("type %d, min_time %d\n",
				ra_limit->type,
				ra_limit->limits.fixed.hold_time);
			ra_limit->length = ND_RA_OL_LIMITS_FIXED_TYPE_LEN;
		} else {
			printf("Invalid nd_ra_limit_intv type %d\n",
				ra_limit->type);
			err = BCME_BADARG;
			goto exit;
		}

		if ((err = wlu_var_setbuf(wl, "nd_ra_limit_intv", ra_limit,
				sizeof(nd_ra_ol_limits_t)) < 0)) {
			printf("%s: failed to set %d\n", cmd->name, err);
		}
	} else {
		/* get */
		if (ra_limit->version != ND_RA_OL_LIMITS_VER) {
			printf("Invalid nd_ra_limit_intv version %d\n",
				ra_limit->version);
			err = BCME_VERSION;
			goto exit;
		}

		if (ra_limit->type == ND_RA_OL_LIMITS_REL_TYPE) {
			printf(" type %d, percentage %d, fixed time %d\n",
				ra_limit->type,
				ra_limit->limits.lifetime_relative.lifetime_percent,
				ra_limit->limits.lifetime_relative.min_time);
		} else if (ra_limit->type == ND_RA_OL_LIMITS_FIXED_TYPE) {
			printf(" type %d, fixed time %d\n",
				ra_limit->type,
				ra_limit->limits.fixed.hold_time);
		} else {
			printf("Invalid nd_ra_limit_intv type %d\n",
				ra_limit->type);
			err = BCME_BADARG;
			goto exit;
		}
	}

exit:
	if (ra_limit) {
		free(ra_limit);
	}

	return err;
}

int
wl_ccode_info(void *wl, cmd_t *cmd, char **argv)
{
	int error, i;
	void *ptr;
	const char* abbrev;
	wl_ccode_info_t *ci;
	wl_ccode_entry_t *ce;
	cntry_name_t *cntry;
	const char *band_type[] = {"2G", "5G"};
	const char *ccode_type[] = {"accode", "hdcode", "11dassoccode", "11dscancode", "defccode"};

	/* skip the command name */
	argv++;

	if ((error = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return (error);

	ci = (wl_ccode_info_t *)ptr;
	if (dtoh16(ci->version) != CCODE_INFO_VERSION) {
		printf("\tIncorrect version of ccode_info IOVAR expected %d; got %d\n",
				CCODE_INFO_VERSION, dtoh16(ci->version));
		return -1;
	}
	if (dtoh16(ci->count) > 2 * WLC_NUM_CCODE_INFO) {
		printf("\tBigger than expected country codes. Expected:%d; got %d\n",
				2 * WLC_NUM_CCODE_INFO, dtoh16(ci->count));
		return -1;
	}
	for (i = 0; i < dtoh16(ci->count); i++) {
		ce = &ci->ccodelist[i];
		abbrev = ce->ccode;
		cntry = wlc_cntry_abbrev_to_country(abbrev);
		printf("%s\t%s:%s\t%s\n",
				band_type[ce->band],
				ccode_type[ce->role],
				abbrev, cntry ? cntry->name : "");
	}
	return 0;
}

#define SIM_PM_MAX_CYCLE 10000
#define SIM_PM_MAX_UP 1000
#define SIM_PM_DEF_CYCLE 100
#define SIM_PM_DEF_UP 5

static int
wl_sim_pm(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_OK, opt_err;
	miniopt_t to;
	const char* fn_name = "wl_sim_pm";
	sim_pm_params_t *params = (sim_pm_params_t *)buf;

	if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0, buf, WLC_IOCTL_SMLEN)) < 0) {
		printf("Get %s got error %d\n", cmd->name, err);
		goto exit;
	}

	if (!*++argv) {
		printf("%sabled, Cycle time: %u TUs, Up time: %u TUs\n", params->enabled ? "En" :
			"Dis", params->cycle, params->up);
		goto exit;
	}
	miniopt_init(&to, fn_name, NULL, FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		argv += to.consumed;

		if (to.opt == '\0') {
			if (!to.good_int) {
				fprintf(stderr, "%s: could not parse \"%s\" as an int for enable\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if (to.val != 0 && to.val != 1) {
				fprintf(stderr, "%s: invalid enable parameter. [0|1]\n", fn_name);
				err = BCME_BADARG;
				goto exit;
			}
			params->enabled = to.val;
		}
		if (to.opt == 'c') {
			if (!to.good_int) {
				fprintf(stderr, "%s: could not parse \"%s\" as an int for cycle\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if (to.val <= 0 || to.val > SIM_PM_MAX_CYCLE) {
				fprintf(stderr, "%s: invalid cycle time. Max. %d\n", fn_name,
					SIM_PM_MAX_CYCLE);
				err = BCME_BADARG;
				goto exit;
			}
			params->cycle = to.val;
		}
		if (to.opt == 'u') {
			if (!to.good_int) {
				fprintf(stderr, "%s: could not parse \"%s\" as an int for up\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if (to.val <= 0 || to.val > SIM_PM_MAX_UP) {
				fprintf(stderr, "%s: invalid up time. Max. %d\n", fn_name,
					SIM_PM_MAX_UP);
				err = BCME_BADARG;
				goto exit;
			}
			params->up = to.val;
		}
	}
	params->cycle = params->cycle ? params->cycle : SIM_PM_DEF_CYCLE;
	params->up = params->up ? params->up : SIM_PM_DEF_UP;
	if (params->up >= params->cycle) {
		fprintf(stderr, "%s: cycle time (%d) should be greater than up time (%d).\n",
				fn_name, params->cycle, params->up);
		err = BCME_BADARG;
		goto exit;
	}
	if ((err = wlu_iovar_set(wl, cmd->name, params, sizeof(*params))) < 0) {
		printf("Error setting variable %s\n", cmd->name);
		return err;
	}
exit:
	return err;
}

/* Health Check "hc" iovar support */

/* hc command information structure, forward decl. */
struct hc_sub_cmd_ent;

/* hc sub-command handler function, used in the hc command tables */
typedef int (*hc_cmd_fn_t)(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd, char **argv);

/* hc command information structure, definition
 * Used to dispatch action based on a sub-command name
 */
struct hc_sub_cmd_ent {
	const char *name;	/* sub-command name for command line */
	uint16 id;			/* cmd id for the handler fn */
	hc_cmd_fn_t fn;		/* sub-command handler fn */
};
typedef struct hc_sub_cmd_ent hc_sub_cmd_table_t[];

static struct hc_sub_cmd_ent* wl_hc_cmd_lookup(hc_sub_cmd_table_t tbl, char* cmd);
static void wl_hc_subcommand_list_dump(void *wl, const char *category, hc_sub_cmd_table_t tbl);
static int wl_hc_int(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd, char** argv);
static int wl_hc_exclude_bitmap(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd,
	char** argv);
static int wl_hc_setints(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd,
	int int_count, char **argv);
static int wl_hc_getints(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd,
	int int_count, char **argv);

struct hc_sub_cmd_ent hc_tx_cmds[] = {	/* HC Tx category commands */
	{"stall_threshold",   WL_HC_TX_XTLV_ID_VAL_STALL_THRESHOLD,   wl_hc_int},
	{"stall_sample_size", WL_HC_TX_XTLV_ID_VAL_STALL_SAMPLE_SIZE, wl_hc_int},
	{"stall_timeout",     WL_HC_TX_XTLV_ID_VAL_STALL_TIMEOUT,     wl_hc_int},
	{"stall_force",       WL_HC_TX_XTLV_ID_VAL_STALL_FORCE,       wl_hc_int},
	{"stall_exclude",     WL_HC_TX_XTLV_ID_VAL_STALL_EXCLUDE,     wl_hc_exclude_bitmap},
	{"fc_timeout",        WL_HC_TX_XTLV_ID_VAL_FC_TIMEOUT,        wl_hc_int},
	{"fc_force",          WL_HC_TX_XTLV_ID_VAL_FC_FORCE,          wl_hc_int},
	{"delay_to_trap",     WL_HC_TX_XTLV_ID_VAL_DELAY_TO_TRAP,     wl_hc_int},
	{"delay_to_rpt",      WL_HC_TX_XTLV_ID_VAL_DELAY_TO_RPT,      wl_hc_int},
	{"failure_to_rpt",    WL_HC_TX_XTLV_ID_VAL_FAILURE_TO_RPT,    wl_hc_int},
	{NULL, 0, NULL},		/* NUL terminated table */
};

hc_sub_cmd_table_t hc_rx_cmds = {	/* HC Rx category commands */
	{"dma_stall_timeout", WL_HC_RX_XTLV_ID_VAL_DMA_STALL_TIMEOUT, wl_hc_int},
	{"dma_stall_force",   WL_HC_RX_XTLV_ID_VAL_DMA_STALL_FORCE,   wl_hc_int},
	{"stall_threshold",   WL_HC_RX_XTLV_ID_VAL_STALL_THRESHOLD,   wl_hc_int},
	{"stall_sample_size", WL_HC_RX_XTLV_ID_VAL_STALL_SAMPLE_SIZE, wl_hc_int},
	{"stall_force",       WL_HC_RX_XTLV_ID_VAL_STALL_FORCE,       wl_hc_int},
	{NULL, 0, NULL},		/* NUL terminated table */
};

struct hc_sub_cmd_ent hc_scan_cmds[] = { /* Scan category commands */
	{"stall_threshold",	WL_HC_XTLV_ID_VAL_SCAN_STALL_THRESHOLD,	wl_hc_int},
	{NULL, 0, NULL},	/* NUL terminated table */
};

/* Main command line handler for Health Check 'hc' iovar */
static int
wl_hc(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char *sub_cmd_name;
	struct hc_sub_cmd_ent *sub_cmd;

	UNUSED_PARAMETER(cmd);

	argv++; 	/* Skip the command name */

	sub_cmd_name = *argv++;

	if (sub_cmd_name == NULL) {
		printf("hc: missing category\n"); /* usage */
		return BCME_USAGE_ERROR;
	}

	if (!strcmp(sub_cmd_name, "tx")) {
		char *attr_name = *argv++;

		if (attr_name == NULL) {
			printf("hc tx: missing attribute\n"); /* usage */
			wl_hc_subcommand_list_dump(wl, sub_cmd_name, hc_tx_cmds);
			return BCME_USAGE_ERROR;
		}

		sub_cmd = wl_hc_cmd_lookup(hc_tx_cmds, attr_name); /* lookup tx subcommand table */

		if (sub_cmd == NULL) {
			printf("hc tx: unknown sub-command \"%s\"\n", attr_name); /* usage */
			wl_hc_subcommand_list_dump(wl, sub_cmd_name, hc_tx_cmds);
			return BCME_USAGE_ERROR;
		}

	ret = (*sub_cmd->fn)(wl, WL_HC_XTLV_ID_CAT_DATAPATH_TX, sub_cmd, argv); /* execute */

	} else if (!strcmp(sub_cmd_name, "rx")) {
		char *attr_name = *argv++;

		if (attr_name == NULL) {
			printf("hc rx: missing attribute\n");	/* usage */
			wl_hc_subcommand_list_dump(wl, sub_cmd_name, hc_rx_cmds);
			return BCME_USAGE_ERROR;
		}

		sub_cmd = wl_hc_cmd_lookup(hc_rx_cmds, attr_name); /* lookup rx subcommand table */

		if (sub_cmd == NULL) {
			printf("hc rx: unknown sub-command \"%s\"\n", attr_name); /* usage */
			wl_hc_subcommand_list_dump(wl, sub_cmd_name, hc_rx_cmds);
			return BCME_USAGE_ERROR;
		}

		ret = (*sub_cmd->fn)(wl, WL_HC_XTLV_ID_CAT_DATAPATH_RX, sub_cmd, argv);

	} else if (!strcmp(sub_cmd_name, "scan")) {
		char *attr_name = *argv++;

		if (attr_name == NULL) {
			printf("hc scan: missing attribute\n");	/* usage */
			wl_hc_subcommand_list_dump(wl, sub_cmd_name, hc_scan_cmds);
			return BCME_USAGE_ERROR;
		}

		sub_cmd = wl_hc_cmd_lookup(hc_scan_cmds, attr_name);

		if (sub_cmd == NULL) {
			printf("hc scan: unknown sub-command \"%s\"\n", attr_name); /* usage */
			wl_hc_subcommand_list_dump(wl, sub_cmd_name, hc_rx_cmds);
			return BCME_USAGE_ERROR;
		}

		ret = (*sub_cmd->fn)(wl, WL_HC_XTLV_ID_CAT_SCAN, sub_cmd, argv); /* execute */

	} else {
		printf("unknown hc category \"%s\"\n", sub_cmd_name); /* usage */
		return BCME_USAGE_ERROR;
	}

	return ret;
}

/* Generic set/get of an integer attribute.
 * Just issue a wl_hc_setints() if there is a 2nd arg, or wl_hc_getints() otherwise.
 */
static int
wl_hc_int(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd, char** argv)
{
	if (argv[0] != NULL)
		return (wl_hc_setints(wl, category, hc_cmd, 1, argv));
	else
		return (wl_hc_getints(wl, category, hc_cmd, 1, argv));
}

/* Set/Get of the "exclude" bitmap.
 * Just issue a wl_hc_setints() for 2 ints if there is a 2nd arg,
 * or wl_hc_getints() for 2 ints otherwise.
 */
static int
wl_hc_exclude_bitmap(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd, char** argv)
{
	if (argv[0] != NULL)
		return (wl_hc_setints(wl, category, hc_cmd, 2, argv));
	else
		return (wl_hc_getints(wl, category, hc_cmd, 2, argv));
}

static int
wl_hc_setints(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd, int int_count, char **argv)
{
	char *valstr;
	char *endptr = NULL;
	bcm_xtlv_t *hc_tlv;
	struct {
		uint16 id;
		uint16 len;
		uint32 value[1];
	} *val_xtlv;
	uint32 words[2];
	int i;
	uint out_len;
	uint16 val_payload_len;
	uint16 container_payload_len;
	uint32 val;
	int ret = 0;

	for (i = 0; i < int_count; i++) {
		valstr = *argv++;

		if (!valstr || valstr[0] == '\0') {
			printf("hc set: missing value argument for set of \"%s\"\n", hc_cmd->name);
			return BCME_USAGE_ERROR;
		}

		val = strtoul(valstr, &endptr, 0);
		if (*endptr != '\0') {
			/* not all the value string was parsed by strtol */
			printf("set: error parsing value \"%s\" as an integer for set of \"%s\"\n",
			       valstr, hc_cmd->name);
			return BCME_USAGE_ERROR;
		}
		words[i] = val;
	}

	/* total output buffer will be an hc sub-category xtlv holding
	 * an xtvl of 1 or 2 uint32s.
	 */
	val_payload_len = (int_count * sizeof(val_xtlv->value[0]));
	container_payload_len = bcm_xtlv_size_for_data(val_payload_len,
		BCM_XTLV_OPTION_NONE);
	out_len = bcm_xtlv_size_for_data(container_payload_len,
		BCM_XTLV_OPTION_NONE);

	/* init the wrapper hc category XTLV in the IO buffer */
	hc_tlv = (bcm_xtlv_t*)(&buf[0]);
	hc_tlv->id = htol16(category);
	hc_tlv->len = htol16(container_payload_len);

	val_xtlv = (void*)hc_tlv->data;
	val_xtlv->id = htol16(hc_cmd->id);
	val_xtlv->len = htol16(val_payload_len);
	for (i = 0; i < int_count; i++) {
		val_xtlv->value[i] = htol32(words[i]);
	}

	ret = wlu_iovar_set(wl, "hc", hc_tlv, out_len);

	return ret;
}

static int
wl_hc_getints(void *wl, uint16 category, struct hc_sub_cmd_ent *hc_cmd, int int_count, char **argv)
{
	int ret = 0;
	bcm_xtlv_t *hc_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 hc_len, hc_id;
	uint16 val_len, val_id;
	struct {
		uint16 id;
		uint16 len;
		uint16 ids[1];
	} id_list;
	uint param_len;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_NONE;

	if (*argv) {
		printf("hc get: error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* total param buffer will be an hc sub-category xtlv holding
	 * an xtvl ID list of one element.
	 * no padding since the id_list is the last element.
	 */
	param_len = bcm_xtlv_size_for_data(sizeof(id_list), no_pad);

	/* init an XTLV ID list with the desired attribute ID */
	bcm_xtlv_pack_xtlv((bcm_xtlv_t *)&id_list, WL_HC_XTLV_ID_IDLIST, sizeof(uint16),
		NULL, no_pad);
	id_list.ids[0] = htol16(hc_cmd->id);

	/* init the wrapper hc category XTLV in the IO buffer and copy to it */
	hc_tlv = (bcm_xtlv_t *)&buf[0];
	bcm_xtlv_pack_xtlv(hc_tlv, category, sizeof(id_list), (const uint8 *)&id_list, no_pad);

	/* issue the 'hc' get with the one-item id list */
	if ((ret = wlu_iovar_getbuf(wl, "hc", buf, param_len, buf, WLC_IOCTL_SMLEN)))
		return ret;

	hc_len = ltoh16(hc_tlv->len);
	hc_id = ltoh16(hc_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(hc_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("hc get: return XTLV specifies length %u too long for iobuffer len %u\n",
		       hc_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the HC xtlv container */
	if (hc_id != category) {
		printf("hc get: return category %u did not match %u\n",
		       hc_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (hc_len < BCM_XTLV_HDR_SIZE) {
		printf("hc get: return XTLV container specifies length %u too small "
		       "to hold any values\n",
		       hc_len);
		return BCME_ERROR;
	}

	val_xtlv = (bcm_xtlv_t*)hc_tlv->data;
	val_len = ltoh16(val_xtlv->len);
	val_id = ltoh16(val_xtlv->id);

	/* make sure the value xtlv is valid xtlv container */
	if (!bcm_valid_xtlv(val_xtlv, hc_len, no_pad)) {
		printf("hc get: return XTLV specifies length %u too long "
		       "for containing xtlv len %u\n",
		       val_len, hc_len);
		return BCME_ERROR;
	}

	if (val_len == 0) {
		printf("hc get: return value XTLV (id:%u) empty\n", val_id);
		return BCME_ERROR;
	} else if (val_len != int_count * sizeof(int32)) {
		printf("hc get: WARNING: return value XTLV (id:%u len:%u) was not "
		       "the expected len %u\n",
		       val_id, val_len, (uint)(int_count * sizeof(int32)));
		ret = BCME_ERROR;
	}

	/* print as words if multiple of word size, otherwise hexdump */
	if (val_len % sizeof(uint32) == 0) {
		uint32 *w = (uint32*)val_xtlv->data;
		int i;
		for (i = 0; i * sizeof(uint32) < val_len; i++) {
			wl_printint((int)ltoh32(w[i]));
		}
	} else {
		wl_hexdump((uint8*)val_xtlv->data, val_len);
	}

	return (ret);
}

static struct hc_sub_cmd_ent*
wl_hc_cmd_lookup(hc_sub_cmd_table_t tbl, char* cmd)
{
	struct hc_sub_cmd_ent* entry;

	for (entry = tbl; entry->name != NULL; entry++) {
		if (!strcmp(entry->name, cmd)) {
			break;
		}
	}

	if (entry->name == NULL) {
		entry = NULL;
	}

	return entry;
}

static void
wl_hc_subcommand_list_dump(void *wl, const char *category, hc_sub_cmd_table_t tbl)
{
	struct hc_sub_cmd_ent* entry;

	BCM_REFERENCE(wl);

	printf("Sub-Commands for \"%s\":\n", category);

	for (entry = tbl; entry->name != NULL; entry++) {
		printf("  %s %s\n", category, entry->name);
	}
}

static int
wl_idauth(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char *sub_cmd_name;

	UNUSED_PARAMETER(cmd);

	/* Skip the command name */
	argv++;

	sub_cmd_name = *argv++;

	if (sub_cmd_name == NULL) {
		/* usage */
		printf("idauth: missing category\n");
		return BCME_USAGE_ERROR;
	}

	if (!strcmp(sub_cmd_name, "config")) {
		char* attr_name;

		attr_name = *argv;

		if (attr_name == NULL) {
			/* GET IDAUTH CONFIG */
			ret = wl_idauth_config_get(wl, WL_IDAUTH_CMD_CONFIG, argv);
		}
		else {
			/* SET AUTH Config */
			ret = wl_idauth_config_set(wl, WL_IDAUTH_CMD_CONFIG, argv);
		}
	} else if (!strcmp(sub_cmd_name, "peer_info")) {
		if (*argv == NULL) {
			ret = wl_idauth_dump_peer_info(wl, WL_IDAUTH_CMD_PEER_INFO, argv);
		} else {
			printf("auth peer_info get: error, extra arg \"%s\"\n", *argv);
			return BCME_USAGE_ERROR;
		}

	} else if (!strcmp(sub_cmd_name, "counters")) {
		if (*argv == NULL) {
			ret = wl_idauth_dump_counters(wl, WL_IDAUTH_CMD_COUNTERS, argv);
		} else {
			printf("auth counters get: error, extra arg \"%s\"\n", *argv);
			return BCME_USAGE_ERROR;
		}
	} else {
		/* usage */
		printf("unknown idauth sub-commands \"%s\"\n", sub_cmd_name);
		return BCME_USAGE_ERROR;
	}
	return ret;
}
static int
wl_idauth_config_set(void *wl, uint16 category, char **argv)
{
	char *valstr;
	char *endptr = NULL;
	bcm_xtlv_t *auth_tlv;
	bcm_xtlv_t *val_tlv_ptr;
	uint out_len;
	uint32 temp;
	uint16 count;
	uint16 container_payload_len = 0;
	int ret = 0;

	char *p;
	/* total output buffer will be an auth sub-category xtlv holding
	 * an xtvl of 1 or 2 uint32s.
	 */
	auth_tlv = (bcm_xtlv_t*)(&buf[0]);
	auth_tlv->id = htol16(category);
	val_tlv_ptr = (bcm_xtlv_t *)auth_tlv->data;
	while (*argv) {
		p = *argv++;
		switch (p[1]) {
			case 'a':
				valstr = *argv++;
				if (!valstr || valstr[0] == '\0') {
					printf("idauth config set: missing value for set of %d\n",
						WL_IDAUTH_XTLV_AUTH_ENAB);
					return BCME_USAGE_ERROR;
				}
				val_tlv_ptr->data[0] = (uint8)strtoul(valstr, &endptr, 0);
				val_tlv_ptr->id = htol16(WL_IDAUTH_XTLV_AUTH_ENAB);
				val_tlv_ptr->len = htol16(sizeof(uint8));
				container_payload_len += bcm_xtlv_size_for_data(sizeof(uint8),
					BCM_XTLV_OPTION_ALIGN32);
				break;
			case 'b':
				valstr = *argv++;
				if (!valstr || valstr[0] == '\0') {
					printf("idauth config set: missing value for set of %d\n",
						WL_IDAUTH_XTLV_BLKLIST_AGE);
					return BCME_USAGE_ERROR;
				}
				temp = htol32(strtoul(valstr, &endptr, 0));
				memcpy(&val_tlv_ptr->data[0], &temp, sizeof(uint32));
				val_tlv_ptr->id = htol16(WL_IDAUTH_XTLV_BLKLIST_AGE);
				val_tlv_ptr->len = htol16(sizeof(uint32));
				container_payload_len += bcm_xtlv_size_for_data(sizeof(uint32),
					BCM_XTLV_OPTION_ALIGN32);
				break;
			case 'c':
				valstr = *argv++;
				if (!valstr || valstr[0] == '\0') {
					printf("idauth config set: missing value for set of %d\n",
						WL_IDAUTH_XTLV_EAPOL_COUNT);
					return BCME_USAGE_ERROR;
				}
				count = htol16(strtoul(valstr, &endptr, 0));
				memcpy(&val_tlv_ptr->data[0], &count, sizeof(uint16));
				val_tlv_ptr->id = htol16(WL_IDAUTH_XTLV_EAPOL_COUNT);
				val_tlv_ptr->len = htol16(sizeof(uint16));
				container_payload_len += bcm_xtlv_size_for_data(sizeof(uint16),
					BCM_XTLV_OPTION_ALIGN32);
				break;
			case 'e':
				valstr = *argv++;
				if (!valstr || valstr[0] == '\0') {
					printf("idauth config set: missing value for set of %d\n",
						WL_IDAUTH_XTLV_EAPOL_INTRVL);
					return BCME_USAGE_ERROR;
				}
				temp = htol32(strtoul(valstr, &endptr, 0));
				memcpy(&val_tlv_ptr->data[0], &temp, sizeof(uint32));
				val_tlv_ptr->id = htol16(WL_IDAUTH_XTLV_EAPOL_INTRVL);
				val_tlv_ptr->len = htol16(sizeof(uint32));
				container_payload_len += bcm_xtlv_size_for_data(sizeof(uint32),
					BCM_XTLV_OPTION_ALIGN32);
				break;
			case 'g':
				valstr = *argv++;
				if (!valstr || valstr[0] == '\0') {
					printf("idauth config set: missing value for set of %d\n",
						WL_IDAUTH_XTLV_GTK_ROTATION);
					return BCME_USAGE_ERROR;
				}
				temp = htol32(strtoul(valstr, &endptr, 0));
				memcpy(&val_tlv_ptr->data[0], &temp, sizeof(uint32));
				val_tlv_ptr->id = htol16(WL_IDAUTH_XTLV_GTK_ROTATION);
				val_tlv_ptr->len = htol16(sizeof(uint32));
				container_payload_len += bcm_xtlv_size_for_data(sizeof(uint32),
					BCM_XTLV_OPTION_ALIGN32);
				break;
			case 'm':
				valstr = *argv++;
				if (!valstr || valstr[0] == '\0') {
					printf("idauth config set: missing value for set of %d\n",
						WL_IDAUTH_XTLV_BLKLIST_COUNT);
					return BCME_USAGE_ERROR;
				}
				count = htol16(strtoul(valstr, &endptr, 0));
				memcpy(&val_tlv_ptr->data[0], &count, sizeof(uint16));
				val_tlv_ptr->id = htol16(WL_IDAUTH_XTLV_BLKLIST_COUNT);
				val_tlv_ptr->len = htol16(sizeof(uint16));
				container_payload_len += bcm_xtlv_size_for_data(sizeof(uint16),
					BCM_XTLV_OPTION_ALIGN32);
				break;
			default:
				break;
		}
		val_tlv_ptr = (bcm_xtlv_t *)(&auth_tlv->data[container_payload_len]);
	}
	auth_tlv->len = htol16(container_payload_len);
	out_len = bcm_xtlv_size_for_data(container_payload_len,
		BCM_XTLV_OPTION_ALIGN32);
	ret = wlu_iovar_set(wl, "idauth", auth_tlv, out_len);
	return ret;
}
static int
wl_idauth_config_get(void *wl, uint16 category, char **argv)
{
	int ret = 0;
	bcm_xtlv_t *auth_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 auth_len, auth_id;
	uint16 val_len, val_id;
	void *val_tlv_ptr;
	uint16 len_of_tlvs;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;

	if (*argv) {
		printf("idauth config get: error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* init the wrapper idauth category XTLV in the IO buffer and copy to it */
	auth_tlv = (bcm_xtlv_t *)&buf[0];
	auth_tlv->id = htol16(category);
	auth_tlv->len = 0;
	bcm_xtlv_pack_xtlv(auth_tlv, category, 0, 0, no_pad);

	/* issue the 'idauth' get with the one-item id list */
	if ((ret = wlu_iovar_getbuf(wl, "idauth", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN)))
		return ret;

	auth_len = ltoh16(auth_tlv->len);
	auth_id = ltoh16(auth_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(auth_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("idauth config get:XTLV specifies length %u too long for iobuffer len %u\n",
		       auth_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the idauth xtlv container */
	if (auth_id != category) {
		printf("idauth config get: return category %u did not match %u\n",
		       auth_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (auth_len < BCM_XTLV_HDR_SIZE) {
		printf("idauth config get: return XTLV container specifies length %u too small"
		       "to hold any values\n",
		       auth_len);
		return BCME_ERROR;
	}
	val_xtlv = (bcm_xtlv_t*)auth_tlv->data;
	val_len = ltoh16(val_xtlv->len);
	val_id = ltoh16(val_xtlv->id);

	/* make sure the value xtlv is valid xtlv container */
	if (!bcm_valid_xtlv(val_xtlv, auth_len, no_pad)) {
		printf("idauth config get: return XTLV specifies length %u too long "
		       "for containing xtlv len %u\n",
		       val_len, auth_len);
		return BCME_ERROR;
	}

	if (val_len == 0) {
		printf("idauth config get: return value XTLV (id:%u) empty\n", val_id);
		return BCME_ERROR;
	}

	/* print as words if multiple of word size */
	val_tlv_ptr = auth_tlv->data;
	len_of_tlvs = auth_tlv->len;
	while (len_of_tlvs && len_of_tlvs > BCM_XTLV_HDR_SIZE) {
		val_xtlv = (bcm_xtlv_t *)val_tlv_ptr;
		switch (val_xtlv->id) {
			case WL_IDAUTH_XTLV_AUTH_ENAB:
				printf("%s:%d\n", "auth_enab", *((uint8*)val_xtlv->data));
				break;
			case WL_IDAUTH_XTLV_GTK_ROTATION:
				printf("%s:%d\n", "gtk_rot_interval", *((uint32*)val_xtlv->data));
				break;
			case WL_IDAUTH_XTLV_EAPOL_COUNT:
				printf("%s:%d\n", "eapol_count", *((uint16*)val_xtlv->data));
				break;
			case WL_IDAUTH_XTLV_EAPOL_INTRVL:
				printf("%s:%d\n", "eapol_interval", *((uint32*)val_xtlv->data));
				break;
			case WL_IDAUTH_XTLV_BLKLIST_COUNT:
				printf("%s:%d\n", "mic_fail_cnt_blacklist",
					*((uint16*)val_xtlv->data));
				break;
			case WL_IDAUTH_XTLV_BLKLIST_AGE:
				printf("%s:%d\n", "blacklist_age", *((uint32*)val_xtlv->data));
				break;
		}
		val_tlv_ptr = (uint8 *)val_tlv_ptr + bcm_xtlv_size(val_xtlv,
			BCM_XTLV_OPTION_ALIGN32);
		len_of_tlvs -= bcm_xtlv_size(val_xtlv, BCM_XTLV_OPTION_ALIGN32);
	}
	return (ret);
}
static int
wl_idauth_dump_counters(void *wl, uint16 category, char **argv)
{
	int ret = 0;
	bcm_xtlv_t *auth_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 auth_len, auth_id;
	uint16 val_len, val_id;
	uint32 *w;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;

	if (*argv) {
		printf("idauth counters get: error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* init the wrapper idauth category XTLV in the IO buffer and copy to it */
	auth_tlv = (bcm_xtlv_t *)&buf[0];
	auth_tlv->id = htol16(category);
	auth_tlv->len = 0;
	bcm_xtlv_pack_xtlv(auth_tlv, category, 0, 0, no_pad);

	/* issue the 'idauth' get with the one-item id list */
	if ((ret = wlu_iovar_getbuf(wl, "idauth", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN)))
		return ret;

	auth_len = ltoh16(auth_tlv->len);
	auth_id = ltoh16(auth_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(auth_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("idauth counter get:XTLV specifies length %u too long for iobuffer len %u\n",
		       auth_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the IDAUTH xtlv container */
	if (auth_id != category) {
		printf("idauth counter get: return category %u did not match %u\n",
		       auth_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (auth_len < BCM_XTLV_HDR_SIZE) {
		printf("idauth counter get: return XTLV container specifies length %u too small"
		       "to hold any values\n",
		       auth_len);
		return BCME_ERROR;
	}
	val_xtlv = (bcm_xtlv_t*)auth_tlv->data;
	val_len = ltoh16(val_xtlv->len);
	val_id = ltoh16(val_xtlv->id);

	/* make sure the value xtlv is valid xtlv container */
	if (!bcm_valid_xtlv(val_xtlv, auth_len, no_pad)) {
		printf("idauth counter: return XTLV specifies length %u too long "
		       "for containing xtlv len %u\n",
		       val_len, auth_len);
		return BCME_ERROR;
	}

	if (val_len == 0) {
		printf("idauth counter get: return value XTLV (id:%u) empty\n", val_id);
		return BCME_ERROR;
	}

	/* print as words if multiple of word size, otherwise hexdump */
	w = (uint32*)val_xtlv->data;
	if (val_xtlv->len < sizeof(wl_idauth_counters_t))
		printf("Auth counter length lesser than expected\n");
	else
		printf("%s %d\t %s %d\t%s %d\n", "authreq", w[0],
			"micfail", w[1], "4wayhsfail", w[2]);
	return (ret);
}

static int
wl_idauth_dump_peer_info(void *wl, uint16 category, char **argv)
{
	int ret = 0;
	bcm_xtlv_t *auth_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 auth_len, auth_id;
	uint16 val_len, val_id;
	auth_peer_t *w;
	int idx;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;

	if (*argv) {
		printf("idauth peer_info: error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* init the wrapper idauth category XTLV in the IO buffer and copy to it */
	auth_tlv = (bcm_xtlv_t *)&buf[0];
	auth_tlv->id = htol16(category);
	auth_tlv->len = 0;
	bcm_xtlv_pack_xtlv(auth_tlv, category, 0, 0, no_pad);

	/* issue the 'idauth' get with the one-item id list */
	if ((ret = wlu_iovar_getbuf(wl, "idauth", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN)))
		return ret;

	auth_len = ltoh16(auth_tlv->len);
	auth_id = ltoh16(auth_tlv->id);
	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(auth_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("idauth peer_info:XTLV specifies length %u too long for iobuffer len %u\n",
		       auth_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the IDAUTH xtlv container */
	if (auth_id != category) {
		printf("idauth peer_info: return category %u did not match %u\n",
		       auth_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (auth_len < BCM_XTLV_HDR_SIZE) {
		if (auth_len == 0) {
			printf("idauth peer_info: No peer present\n");
			return ret;
		} else {
			printf("idauth peer_info: return XTLV container specifies"
				"length %u too small"
				"to hold any values\n",
				auth_len);
			return BCME_ERROR;
		}
	}
	val_xtlv = (bcm_xtlv_t*)auth_tlv->data;
	val_len = ltoh16(val_xtlv->len);
	val_id = ltoh16(val_xtlv->id);

	/* make sure the value xtlv is valid xtlv container */
	if (!bcm_valid_xtlv(val_xtlv, auth_len, no_pad)) {
		printf("idauth peer_info: return XTLV specifies length %u too long "
		       "for containing xtlv len %u\n",
		       val_len, auth_len);
		return BCME_ERROR;
	}

	if (val_len == 0) {
		printf("idauth peer_info: return value XTLV (id:%u) empty\n", val_id);
		return BCME_OK;
	}

	/* print as words if multiple of word size, otherwise hexdump */
	w = (auth_peer_t*)val_xtlv->data;
	for (idx = 0; idx * sizeof(auth_peer_t) < val_xtlv->len; idx++) {
		switch (w[idx].state) {
			case WL_AUTH_PEER_STATE_AUTHORISED:
				printf("STA %s\tState:%s\tBlacklist Age:%d\n",
					wl_ether_etoa(&w[idx].peer_addr), "AUTHORIZED", 0);
				break;
			case WL_AUTH_PEER_STATE_BLACKLISTED:
				printf("STA %s\tState:%s\tBlacklist Age:%d\n",
					wl_ether_etoa(&w[idx].peer_addr), "BLACKLISTED",
					w[idx].blklist_end_time);
				break;
			case WL_AUTH_PEER_STATE_4WAY_HS_ONGOING:
				printf("STA %s\tState:%s\tBlacklist Age:%d\n",
					wl_ether_etoa(&w[idx].peer_addr), "4WAY_HS_ONGOING", 0);
				break;
			default:
				printf("idauth peer info: unknown state\n");
		}
	}
	return (ret);
}

int
fsize(void *fp)
{
	long size = BCME_ERROR;
	long cur_offset  = BCME_ERROR;

	if ((cur_offset = ftell(fp)) < 0) {
		fprintf(stderr, "Could not determine current file offset : %s\n", strerror(errno));
		return BCME_ERROR;
	}

	if (!fseek(fp, 0, SEEK_END)) {
	    if ((size = ftell(fp)) < 0)
			fprintf(stderr, "Could not determine file size : %s\n", strerror(errno));

		fseek(fp, cur_offset, SEEK_SET);
	}
	else
		fprintf(stderr, "Error in performing fseek: %s\n", strerror(errno));

	return (int)size;
}

static int
wl_wake_timer(void *wl, cmd_t *cmd, char **argv)
{
	int err, buflen;
	wake_timer_t *w_timer;
	char *endptr;

	buflen = sprintf(buf, "%s", *argv) + 1;

	if (*++(argv) == NULL) {
		buf[buflen] = '\0';
		err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_SMLEN);
		if (err < 0)
			return err;
		w_timer = (wake_timer_t *)buf;
		if (dtoh16(w_timer->ver) != WAKE_TIMER_VERSION) {
			err = BCME_VERSION;
			return err;
		}
		printf("Status:%s\n", dtoh16(w_timer->limit) ? "ENABLED": "DISABLED");
		printf("Num of events delivered:%d\n", dtoh16(w_timer->count));
		return 0;
	} else {
		w_timer =  (wake_timer_t *) (buf + buflen);
		buflen += sizeof(wake_timer_t);

		w_timer->period = htod16(strtoul(*argv, &endptr, 0));

		if (*endptr != '\0') {
			fprintf(stderr, "Type '%s' not a number?\n", *argv);
			return BCME_ERROR;
		}
		if (*++(argv) == NULL) {
			printf("Missing arg <limit>\n");
			goto usage;
		}
		w_timer->limit = htod16(strtoul(*argv, &endptr, 0));
		if (*endptr != '\0') {
			fprintf(stderr, "Type '%s' not a number?\n", *argv);
			return BCME_ERROR;
		}
		if (*++(argv)) {
			printf("extra arguments\n");
			goto usage;
		}
		w_timer->ver = htod16(WAKE_TIMER_VERSION);
		w_timer->len = htod16(sizeof(wake_timer_t));
		err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

		return err;
	}
usage:
		printf("Usage: wl wake_timer <period> <limit>\n");
	return 0;
}

static int
wl_utrace_capture(void *wl, cmd_t *cmd, char **argv)
{
	void *buff = NULL;
	uint16 *ptr = NULL;
	uint j;
	FILE *fptr = NULL;
	uint32 capture_args_size = 0;
	uchar rev_2 = FALSE;
	int err = BCME_ERROR;
	uint32 flag = WLC_UTRACE_MORE_DATA;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	/* Use _v1 version of structure for DINGO and other legacy chips and _v2 for newer chips. */
	if (wlc_ver_major(wl) <= 5) {
		/* Handle Legacy chips and chips from DINGO2 */
		capture_args_size = sizeof(wl_utrace_capture_args_v1_t);
		rev_2 = FALSE;
	} else {
		capture_args_size = sizeof(wl_utrace_capture_args_v2_t);
		rev_2 = TRUE;
	}

	fptr = fopen("utrace.txt", "w");
	if (fptr == NULL) {
		return BCME_NORESOURCE;
	}
	buff = malloc(WLC_UTRACE_LEN + capture_args_size);
	if (buff == NULL) {
		fclose(fptr);
		return BCME_NOMEM;
	}
	memset(buff, 0, WLC_UTRACE_LEN + capture_args_size);

	do {
		if ((err = wlu_iovar_getbuf(wl, cmd->name,
			buff, 0, buff, (WLC_UTRACE_LEN + capture_args_size))) < 0)
		{
			goto exit;
		}
		if (rev_2 == TRUE) {
			wl_utrace_capture_args_v2_t *capture_args;
			capture_args = (wl_utrace_capture_args_v2_t *) buff;
			flag = capture_args->flag;
			/* Check the structure rev */
			if (capture_args->version == UTRACE_CAPTURE_VER_2) {
				capture_args->length -= capture_args_size;
				ptr = (uint16 *) ((uchar*)buff + capture_args_size);
				for (j = 0; j < capture_args->length/sizeof(uint32); j++) {
					fprintf(fptr, "%04x %04x \n", ptr[j*2 + 1], ptr[j*2]);
				}
			} else {
				err = BCME_VERSION;
				flag = WLC_UTRACE_READ_END;
				printf("Error:Version Mismatch App:%d FW:%d\n",
					UTRACE_CAPTURE_VER_2, capture_args->version);
				break;
			}
		} else {
			wl_utrace_capture_args_v1_t *capture_args;
			capture_args = (wl_utrace_capture_args_v1_t *) buff;
			flag = capture_args->flag;
			ptr = (uint16 *) ((uchar*)buff + capture_args_size);
			for (j = 0; j < capture_args->length/sizeof(uint32); j++) {
				fprintf(fptr, "%04x %04x \n", ptr[j*2 + 1], ptr[j*2]);
			}

		}
	} while (flag == WLC_UTRACE_MORE_DATA);

exit:
	free(buff);
	fclose(fptr);
	return err;

}

static int
wl_eventaggr(void *wl, cmd_t *cmd, char **argv)
{
	event_aggr_config_t *pconfig;
	int	buf_len;
	int	str_len;
	const char		*str;
	int rc;
	void	*ptr = NULL;
	int count;

	count = ARGCNT(argv);

	if (*++argv == NULL) {
		/* Get operation */
		if ((rc = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return rc;

		pconfig = (event_aggr_config_t *) ptr;
		if (pconfig->version > EVENT_AGGR_CFG_VERSION) {
			printf("Version mismatch: actual %d, expected %d\n",
					pconfig->version, EVENT_AGGR_CFG_VERSION);
		}
		printf("Flags	:%x\n"
			   "Aggregate Bufsize     :%d\n"
			   "Event flush timeout   :%d\n"
			   "Number of events before flush :%d\n",
			   dtoh16(pconfig->flags), dtoh16(pconfig->bufsize),
			   dtoh16(pconfig->flush_timeout), dtoh16(pconfig->num_events_flush));
	} else {
		/* Set operation */
		if (count != 5) {
			return BCME_USAGE_ERROR;
		}

		str = "event_aggr";
		str_len = strlen(str);
		strncpy(buf, str, str_len);
		buf[str_len] = '\0';
		pconfig = (event_aggr_config_t *) (buf + str_len + 1);
		memset(pconfig, 0, sizeof(*pconfig));

		pconfig->version = EVENT_AGGR_CFG_VERSION;
		pconfig->len = sizeof(*pconfig);
		pconfig->flags = htod16(strtoul(*argv++, NULL, 0));
		pconfig->bufsize = htod16(strtoul(*argv++, NULL, 0));
		pconfig->flush_timeout = htod16(strtoul(*argv++, NULL, 0));
		pconfig->num_events_flush = htod16(strtoul(*argv++, NULL, 0));
		buf_len = str_len + 1 + sizeof(*pconfig);

		rc = wlu_set(wl, WLC_SET_VAR, buf, buf_len);
	}
	return rc;
}

static int
wl_atm_staperc(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_atm_staperc_t *sp = (wl_atm_staperc_t *) buf;

	if (*++argv) {
		if (wl_ether_atoe(*argv, &sp->ea)) {
			if (*++argv) {
				/* SET */
				sp->perc = atoi(*argv);
				err = wlu_iovar_setbuf(wl, cmd->name,
						sp, sizeof(*sp), buf, WLC_IOCTL_MEDLEN);
			} else {
				/* GET */
				err = wlu_iovar_getbuf(wl, cmd->name,
						sp, sizeof(*sp), buf, WLC_IOCTL_MEDLEN);
				printf("%d%%\n", sp->perc);
			}
		}
	} else {
		printf("Provide <mac_addr> [num_percent]\n");
		return BCME_USAGE_ERROR;
	}

	return err;
}

void
dump_bss_info(void *bi_generic)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chspec_str[CHANSPEC_STR_LEN];
	wl_bss_info_v109_1_t *bi;
	int mcs_idx = 0, start_idx = 0;
	bool start_idx_valid = FALSE;
	uint16 capability;
	uint32 version;
	uint32 length;

	bi = (wl_bss_info_v109_1_t*)bi_generic;
	version = dtoh32(bi->version);
	length = dtoh32(bi->length);

	/* Convert version 107 to 109 */
	if (version == LEGACY_WL_BSS_INFO_VERSION) {
		wl_bss_info_107_t *bi_v107 = (wl_bss_info_107_t *)bi_generic;
		bi->chanspec = CH20MHZ_CHSPEC(bi_v107->channel);
		bi->ie_length = bi_v107->ie_length;
		bi->ie_offset = sizeof(wl_bss_info_107_t);
	} else {
		/* do endian swap and format conversion for chanspec if we have
		 * not created it from legacy bi above
		 */
		bi->chanspec = wl_chspec_from_driver(bi->chanspec);
	}

	wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);

	printf("SSID: \"%s\"\n", ssidbuf);

	printf("Mode: %s\t", capmode2str(dtoh16(bi->capability)));
	printf("RSSI: %d dBm\t", (int16)(dtoh16(bi->RSSI)));

	/*
	 * SNR has valid value in only 109 version.
	 * So print SNR for 109 version only.
	 */
	if (version == WL_BSS_INFO_VERSION) {
		printf("SNR: %d dB\t", (int16)(dtoh16(bi->SNR)));
	}

	printf("noise: %d dBm\t", bi->phy_noise);
	if (bi->flags) {
		uint16 flags = dtoh16(bi->flags);
		printf("Flags: ");
		if (flags & WL_BSS_FLAGS_FROM_BEACON)
			printf("FromBcn ");
		if (flags & WL_BSS_FLAGS_FROM_CACHE)
			printf("Cached ");
		if (flags & WL_BSS_FLAGS_RSSI_ONCHANNEL)
			printf("RSSI on-channel ");
		printf("\t");
	}
	printf("Channel: %s\n", wf_chspec_ntoa(bi->chanspec, chspec_str));

	printf("BSSID: %s\t", wl_ether_etoa(&bi->BSSID));

	printf("Capability: ");
	capability = dtoh16(bi->capability);
	if (capability & DOT11_CAP_ESS)
		printf("ESS ");
	if (capability & DOT11_CAP_IBSS)
		printf("IBSS ");
	if (capability & DOT11_CAP_POLLABLE)
		printf("Pollable ");
	if (capability & DOT11_CAP_POLL_RQ)
		printf("PollReq ");
	if (capability & DOT11_CAP_PRIVACY)
		printf("WEP ");
	if (capability & DOT11_CAP_SHORT)
		printf("ShortPre ");
	if (capability & DOT11_CAP_PBCC)
		printf("PBCC ");
	if (capability & DOT11_CAP_AGILITY)
		printf("Agility ");
	if (capability & DOT11_CAP_SHORTSLOT)
		printf("ShortSlot ");
	if (capability & DOT11_CAP_RRM)
		printf("RRM ");
	if (capability & DOT11_CAP_CCK_OFDM)
		printf("CCK-OFDM ");
	printf("\n");

	printf("Supported Rates: ");
	dump_rateset(bi->rateset.rates, dtoh32(bi->rateset.count));
	printf("\n");
	if (dtoh32(bi->ie_length)) {
		wl_dump_wpa_rsn_ies((uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
		                    dtoh32(bi->ie_length));

		wl_dump_ext_cap((uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
		                    dtoh32(bi->ie_length));

	}

	if (version != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap) {
		if (bi->he_cap) {
			printf("HE Capable:\n");
		} else if (bi->vht_cap) {
			printf("VHT Capable:\n");
		} else {
			printf("HT Capable:\n");
		}
		if (CHSPEC_IS8080(bi->chanspec)) {
			 printf("\tChanspec: 5GHz channel %d-%d 80+80MHz (0x%x)\n",
			 wf_chspec_primary80_channel(bi->chanspec),
			 wf_chspec_secondary80_channel(bi->chanspec),
			 bi->chanspec);
		}
		else {
			printf("\tChanspec: %sGHz channel %d %dMHz (0x%x)\n",
				CHSPEC_IS2G(bi->chanspec)?"2.4":"5", CHSPEC_CHANNEL(bi->chanspec),
				(CHSPEC_IS160(bi->chanspec) ?
				160:(CHSPEC_IS80(bi->chanspec) ?
				80 : (CHSPEC_IS40(bi->chanspec) ?
				40 : (CHSPEC_IS20(bi->chanspec) ? 20 : 10)))),
				bi->chanspec);
		}
		printf("\tPrimary channel: %d\n", bi->ctl_ch);
		printf("\tHT Capabilities: ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_40MHZ)
			printf("40Mhz ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_20)
			printf("SGI20 ");
		if (dtoh32(bi->nbss_cap) & HT_CAP_SHORT_GI_40)
			printf("SGI40 ");
		printf("\n\tSupported HT MCS :");
		for (mcs_idx = 0; mcs_idx < (MCSSET_LEN * 8); mcs_idx++) {
			if (isset(bi->basic_mcs, mcs_idx) && !start_idx_valid) {
				printf(" %d", mcs_idx);
				start_idx = mcs_idx;
				start_idx_valid = TRUE;
			}

			if (!isset(bi->basic_mcs, mcs_idx) && start_idx_valid) {
				if ((mcs_idx - start_idx) > 1)
					printf("-%d", (mcs_idx - 1));
				start_idx_valid = FALSE;

			}
		}
		printf("\n");

		if (bi->vht_cap) {
			int i;
			uint mcs, rx_mcs, prop_mcs = VHT_PROP_MCS_MAP_NONE;
			char *mcs_str, *rx_mcs_str;

			if (bi->vht_mcsmap) {
				printf("\tNegotiated VHT MCS:\n");
				for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
					mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, dtoh16(bi->vht_mcsmap));

					/* roundup to be in sync with driver
					 * wlc_bss2wl_bss().
					 */
					if (length >= (OFFSETOF(wl_bss_info_v109_1_t,
						vht_mcsmap_prop) +
						ROUNDUP(dtoh32(bi->ie_length), 4) +
						sizeof(uint16))) {
						prop_mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i,
							dtoh16(bi->vht_mcsmap_prop));
					}
					mcs_str =
						(mcs == VHT_CAP_MCS_MAP_0_9 ? "0-9 " :
						(mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8 " :
						(mcs == VHT_CAP_MCS_MAP_0_7 ? "0-7 " :
						 " -- ")));
					if (prop_mcs != VHT_PROP_MCS_MAP_NONE)
						mcs_str =
							(mcs == VHT_CAP_MCS_MAP_0_9 ? "0-11      " :
							(mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8, 10-11" :
							(mcs == VHT_CAP_MCS_MAP_0_7 ? "0-7, 10-11" :
							 "    --    ")));

					if (mcs != VHT_CAP_MCS_MAP_NONE) {
						printf("\t\tNSS%d : %s \n", i,
							mcs_str);
					}
				}
			} else {
				printf("\tSupported VHT MCS:\n");
				for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
					mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i,
						dtoh16(bi->vht_txmcsmap));

					rx_mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i,
						dtoh16(bi->vht_rxmcsmap));

					/* roundup to be in sync with driver
					 * wlc_bss2wl_bss().
					 */
					if (length >= (OFFSETOF(wl_bss_info_v109_1_t,
						vht_txmcsmap_prop) +
						ROUNDUP(dtoh32(bi->ie_length), 4) +
						sizeof(uint16))) {
						prop_mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i,
							dtoh16(bi->vht_txmcsmap_prop));
					}

					mcs_str =
						(mcs == VHT_CAP_MCS_MAP_0_9 ? "0-9 " :
						(mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8 " :
						(mcs == VHT_CAP_MCS_MAP_0_7 ? "0-7 " : " -- ")));
					if (prop_mcs != VHT_PROP_MCS_MAP_NONE)
						mcs_str =
						    (mcs == VHT_CAP_MCS_MAP_0_9 ? "0-11      " :
						    (mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8, 10-11" :
						    (mcs == VHT_CAP_MCS_MAP_0_7 ? "0-7, 10-11" :
						     "    --    ")));

					rx_mcs_str =
						(rx_mcs == VHT_CAP_MCS_MAP_0_9 ? "0-9 " :
						(rx_mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8 " :
						(rx_mcs == VHT_CAP_MCS_MAP_0_7 ? "0-7 " : " -- ")));
					if (prop_mcs != VHT_PROP_MCS_MAP_NONE)
						rx_mcs_str =
						    (rx_mcs == VHT_CAP_MCS_MAP_0_9 ? "0-11      " :
						    (rx_mcs == VHT_CAP_MCS_MAP_0_8 ? "0-8, 10-11" :
						    (rx_mcs == VHT_CAP_MCS_MAP_0_7 ? "0-7, 10-11" :
						     "    --    ")));

					if ((mcs != VHT_CAP_MCS_MAP_NONE) ||
						(rx_mcs != VHT_CAP_MCS_MAP_NONE)) {
						printf("\t\tNSS%d Tx: %s  Rx: %s\n", i,
							mcs_str, rx_mcs_str);
					}
				}
			}
		}
		if (bi->he_cap) {
			uint16 *he_mcsmap;

			if (bi->he_neg_bw80_tx_mcs != 0xffff) {
				printf("\tNegotiated HE MCS:\n");
				he_mcsmap = &bi->he_neg_bw80_tx_mcs;
			} else {
				printf("\tSupported HE MCS:\n");
				he_mcsmap = &bi->he_sup_bw80_tx_mcs;
			}

			wl_print_hemcsnss((uint16 *)he_mcsmap);
		}

		bi->chanspec = wl_chspec_to_driver(bi->chanspec);
	}

	if (dtoh32(bi->ie_length))
	{
		wl_dump_wps((uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
			dtoh32(bi->ie_length));
	}

	if (dtoh16(bi->flags) & WL_BSS_FLAGS_HS20) {
		printf("Hotspot 2.0 capable\n");
	}

	if (bcm_find_vs_ie((uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
		dtoh32(bi->ie_length),
		(uint8 *)WFA_OUI, WFA_OUI_LEN, WFA_OUI_TYPE_OSEN) != NULL) {
		printf("OSEN supported\n");
	}
	bcm_print_vs_ie((uint8 *)(((uint8 *)bi) + dtoh16(bi->ie_offset)),
		dtoh32(bi->ie_length));

	printf("\n");
}

static void
wl_dump_wpa_rsn_ies(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;
	uint8 *rsnie;

	while ((wpaie = wlu_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wlu_is_wpa_ie(&wpaie, &parse, &parse_len))
			break;
	if (wpaie)
		wl_rsn_ie_dump((bcm_tlv_t*)wpaie);

	rsnie = wlu_parse_tlvs(cp, len, DOT11_MNG_RSN_ID);
	if (rsnie)
		wl_rsn_ie_dump((bcm_tlv_t*)rsnie);

	return;
}

static void
wl_rsn_ie_dump(bcm_tlv_t *ie)
{
	int i;
	int rsn;
	wpa_ie_fixed_t *wpa = NULL;
	rsn_parse_info_t rsn_info;
	wpa_suite_t *suite;
	uint8 std_oui[3];
	int unicast_count = 0;
	int akm_count = 0;
	uint16 capabilities;
	uint cntrs;
	int err;

	if (ie->id == DOT11_MNG_RSN_ID) {
		rsn = TRUE;
		memcpy(std_oui, WPA2_OUI, WPA_OUI_LEN);
		err = wl_rsn_ie_parse_info(ie->data, ie->len, &rsn_info);
	} else {
		rsn = FALSE;
		memcpy(std_oui, WPA_OUI, WPA_OUI_LEN);
		wpa = (wpa_ie_fixed_t*)ie;
		err = wl_rsn_ie_parse_info((uint8*)&wpa->version, wpa->length - WPA_IE_OUITYPE_LEN,
		                           &rsn_info);
	}
	if (err || rsn_info.version != WPA_VERSION)
		return;

	if (rsn)
		printf("RSN (WPA2):\n");
	else
		printf("WPA:\n");

	/* Check for multicast suite */
	if (rsn_info.mcast) {
		printf("\tmulticast cipher: ");
		if (!wlu_bcmp(rsn_info.mcast->oui, std_oui, 3)) {
			switch (rsn_info.mcast->type) {
			case WPA_CIPHER_NONE:
				printf("NONE\n");
				break;
			case WPA_CIPHER_WEP_40:
				printf("WEP64\n");
				break;
			case WPA_CIPHER_WEP_104:
				printf("WEP128\n");
				break;
			case WPA_CIPHER_TKIP:
				printf("TKIP\n");
				break;
			case WPA_CIPHER_AES_OCB:
				printf("AES-OCB\n");
				break;
			case WPA_CIPHER_AES_CCM:
				printf("AES-CCMP\n");
				break;
			case WPA_CIPHER_AES_GCM:
				printf("AES-GCMP\n");
				break;
			case WPA_CIPHER_AES_GCM256:
				printf("AES-GCMP256\n");
				break;
			default:
				printf("Unknown-%s(#%d)\n", rsn ? "RSN" : "WPA",
				       rsn_info.mcast->type);
				break;
			}
		} else {
			printf("Unknown-%02X:%02X:%02X(#%d) ",
			       rsn_info.mcast->oui[0], rsn_info.mcast->oui[1],
			       rsn_info.mcast->oui[2], rsn_info.mcast->type);
		}
	}

	/* Check for unicast suite(s) */
	if (rsn_info.ucast) {
		unicast_count = ltoh16_ua(&rsn_info.ucast->count);
		printf("\tunicast ciphers(%d): ", unicast_count);
		for (i = 0; i < unicast_count; i++) {
			suite = &rsn_info.ucast->list[i];
			if (!wlu_bcmp(suite->oui, std_oui, 3)) {
				switch (suite->type) {
				case WPA_CIPHER_NONE:
					printf("NONE ");
					break;
				case WPA_CIPHER_WEP_40:
					printf("WEP64 ");
					break;
				case WPA_CIPHER_WEP_104:
					printf("WEP128 ");
					break;
				case WPA_CIPHER_TKIP:
					printf("TKIP ");
					break;
				case WPA_CIPHER_AES_OCB:
					printf("AES-OCB ");
					break;
				case WPA_CIPHER_AES_CCM:
					printf("AES-CCMP ");
					break;
				case WPA_CIPHER_AES_GCM:
					printf("AES-GCMP ");
					break;
				case WPA_CIPHER_AES_GCM256:
					printf("AES-GCMP256 ");
					break;
				default:
					printf("WPA-Unknown-%s(#%d) ", rsn ? "RSN" : "WPA",
					       suite->type);
					break;
				}
			} else {
				printf("Unknown-%02X:%02X:%02X(#%d) ",
					suite->oui[0], suite->oui[1], suite->oui[2],
					suite->type);
			}
		}
		printf("\n");
	}
	/* Authentication Key Management */
	if (rsn_info.akm) {
		akm_count = ltoh16_ua(&rsn_info.akm->count);
		printf("\tAKM Suites(%d): ", akm_count);
		for (i = 0; i < akm_count; i++) {
			suite = &rsn_info.akm->list[i];
			if (!wlu_bcmp(suite->oui, std_oui, 3)) {
				switch (suite->type) {
				case RSN_AKM_NONE:
					printf("None ");
					break;
				case RSN_AKM_UNSPECIFIED:
					printf("%s ", rsn ? "WPA2" : "WPA");
					break;
				case RSN_AKM_PSK:
					printf("%s ", rsn ? "WPA2-PSK" : "WPA-PSK");
					break;
				case RSN_AKM_FBT_1X:
					printf("FT-802.1x ");
					break;
				case RSN_AKM_FBT_PSK:
					printf("FT-PSK ");
				case RSN_AKM_SHA256_PSK:
					printf("WPA2-PSK ");
					break;
				case RSN_AKM_SAE_PSK:
					printf("SAE ");
					break;
				case RSN_AKM_SAE_FBT:
					printf("SAE-FT ");
					break;
				default:
					printf("Unknown-%s(#%d)  ",
					       rsn ? "RSN" : "WPA", suite->type);
					break;
				}
			} else {
				printf("Unknown-%02X:%02X:%02X(#%d)  ",
					suite->oui[0], suite->oui[1], suite->oui[2],
					suite->type);
			}
		}
		printf("\n");
	}

	/* Capabilities */
	if (rsn_info.capabilities) {
		capabilities = ltoh16_ua(rsn_info.capabilities);
		printf("\tCapabilities(0x%04x): ", capabilities);
		if (rsn)
			printf("%sPre-Auth, ", (capabilities & RSN_CAP_PREAUTH) ? "" : "No ");

		printf("%sPairwise, ", (capabilities & RSN_CAP_NOPAIRWISE) ? "No " : "");

		cntrs = wl_rsn_ie_decode_cntrs((capabilities & RSN_CAP_PTK_REPLAY_CNTR_MASK) >>
		                               RSN_CAP_PTK_REPLAY_CNTR_SHIFT);

		printf("%d PTK Replay Ctr%s", cntrs, (cntrs > 1)?"s":"");

		if (rsn) {
			cntrs = wl_rsn_ie_decode_cntrs(
				(capabilities & RSN_CAP_GTK_REPLAY_CNTR_MASK) >>
				RSN_CAP_GTK_REPLAY_CNTR_SHIFT);

			printf("%d GTK Replay Ctr%s, ", cntrs, (cntrs > 1)?"s":"");
			printf("MFP: %s\n", (capabilities & (RSN_CAP_MFPR | RSN_CAP_MFPC)) ?
					((capabilities & RSN_CAP_MFPR) ? "Required" : "Capable"):
					"Disabled");
		} else {
			printf("\n");
		}
	} else {
		printf("\tNo %s Capabilities advertised\n", rsn ? "RSN" : "WPA");
	}

}

static void
wl_dump_ext_cap(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *ext_cap_ie;

	if ((ext_cap_ie = wlu_parse_tlvs(parse, parse_len, DOT11_MNG_EXT_CAP_ID))) {
		wl_ext_cap_ie_dump((bcm_tlv_t*)ext_cap_ie);
	} else
		printf("Extended Capabilities: Not_Available\n");

}

static void
wl_ext_cap_ie_dump(bcm_tlv_t* ext_cap_ie)
{

	printf("Extended Capabilities: ");

	if (ext_cap_ie->len >= CEIL(DOT11_EXT_CAP_IW, NBBY)) {
		/* check IW bit */
		if (isset(ext_cap_ie->data, DOT11_EXT_CAP_IW))
			printf("IW ");
	}

	if (ext_cap_ie->len >= CEIL(DOT11_EXT_CAP_CIVIC_LOC, NBBY)) {
		/* check Civic Location bit */
		if (isset(ext_cap_ie->data, DOT11_EXT_CAP_CIVIC_LOC))
			printf("Civic_Location ");
	}

	if (ext_cap_ie->len >= CEIL(DOT11_EXT_CAP_LCI, NBBY)) {
		/* check Geospatial Location bit */
		if (isset(ext_cap_ie->data, DOT11_EXT_CAP_LCI))
			printf("Geospatial_Location ");
	}

	if (ext_cap_ie->len > 0) {
		/* check 20/40 BSS Coexistence Management support bit */
		if (isset(ext_cap_ie->data, DOT11_EXT_CAP_OBSS_COEX_MGMT))
			printf("20/40_Bss_Coexist ");
	}

	if (ext_cap_ie->len >= CEIL(DOT11_EXT_CAP_BSSTRANS_MGMT, NBBY)) {
		/* check BSS Transition Management support bit */
		if (isset(ext_cap_ie->data, DOT11_EXT_CAP_BSSTRANS_MGMT))
			printf("BSS_Transition");
	}

	printf("\n");
}

/* Validates and parses the RSN or WPA IE contents into a rsn_parse_info_t structure
 * Returns 0 on success, or 1 if the information in the buffer is not consistant with
 * an RSN IE or WPA IE.
 * The buf pointer passed in should be pointing at the version field in either an RSN IE
 * or WPA IE.
 */
static int
wl_rsn_ie_parse_info(uint8* rsn_buf, uint len, rsn_parse_info_t *rsn)
{
	uint16 count;

	memset(rsn, 0, sizeof(rsn_parse_info_t));

	/* version */
	if (len < sizeof(uint16))
		return 1;

	rsn->version = ltoh16_ua(rsn_buf);
	len -= sizeof(uint16);
	rsn_buf += sizeof(uint16);

	/* Multicast Suite */
	if (len < sizeof(wpa_suite_mcast_t))
		return 0;

	rsn->mcast = (wpa_suite_mcast_t*)rsn_buf;
	len -= sizeof(wpa_suite_mcast_t);
	rsn_buf += sizeof(wpa_suite_mcast_t);

	/* Unicast Suite */
	if (len < sizeof(uint16))
		return 0;

	count = ltoh16_ua(rsn_buf);

	if (len < (sizeof(uint16) + count * sizeof(wpa_suite_t)))
		return 1;

	rsn->ucast = (wpa_suite_ucast_t*)rsn_buf;
	len -= (sizeof(uint16) + count * sizeof(wpa_suite_t));
	rsn_buf += (sizeof(uint16) + count * sizeof(wpa_suite_t));

	/* AKM Suite */
	if (len < sizeof(uint16))
		return 0;

	count = ltoh16_ua(rsn_buf);

	if (len < (sizeof(uint16) + count * sizeof(wpa_suite_t)))
		return 1;

	rsn->akm = (wpa_suite_auth_key_mgmt_t*)rsn_buf;
	len -= (sizeof(uint16) + count * sizeof(wpa_suite_t));
	rsn_buf += (sizeof(uint16) + count * sizeof(wpa_suite_t));

	/* Capabilites */
	if (len < sizeof(uint16))
		return 0;

	rsn->capabilities = rsn_buf;

	return 0;
}

static uint
wl_rsn_ie_decode_cntrs(uint cntr_field)
{
	uint cntrs;

	switch (cntr_field) {
	case RSN_CAP_1_REPLAY_CNTR:
		cntrs = 1;
		break;
	case RSN_CAP_2_REPLAY_CNTRS:
		cntrs = 2;
		break;
	case RSN_CAP_4_REPLAY_CNTRS:
		cntrs = 4;
		break;
	case RSN_CAP_16_REPLAY_CNTRS:
		cntrs = 16;
		break;
	default:
		cntrs = 0;
		break;
	}

	return cntrs;
}

static void bcm_print_vs_ie(uint8 *parse, int len)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		int len_tmp = 0;
		printf("VS_IE:");
		printf("%02x%02x", ie->id, ie->len);
		while (len_tmp < ie->len) {
			printf("%02x", ie->data[len_tmp]);
			len_tmp++;
		}
		printf("\n");

		if ((parse = (uint8 *)bcm_next_tlv(ie, &len)) == NULL)
			break;
	}
}

/* Helper routine to print the infrastructure mode while pretty printing the BSS list */
static const char *
capmode2str(uint16 capability)
{
	capability &= (DOT11_CAP_ESS | DOT11_CAP_IBSS);

	if (capability == DOT11_CAP_ESS)
		return "Managed";
	else if (capability == DOT11_CAP_IBSS)
		return "Ad Hoc";
	else
		return "<unknown>";
}

/* Is this body of this tlvs entry a WPA entry? If */
/* not update the tlvs buffer pointer/length */
static bool
wlu_is_wpa_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len)
{
	uint8 *ie = *wpaie;

	/* If the contents match the WPA_OUI and type=1 */
	if ((ie[1] >= 6) && !wlu_bcmp(&ie[2], WPA_OUI "\x01", 4)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[1] + 2;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

static void
wl_dump_wps(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint32 parse_len = len;
	uint8 *proprietary_ie;

	while ((proprietary_ie = wlu_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID))) {
		if (bcm_is_wps_ie(proprietary_ie, &parse, &parse_len)) {
			/* Print WPS status */
			printf("WPS: ");
			/* Print the version get from vendor extension field */
			bcm_wps_version(proprietary_ie);
			/* Print the WPS configure or Unconfigure option */
			bcm_is_wps_configured(proprietary_ie);
			break;
		}
	}
}

/*
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 */
static uint8 *
wlu_parse_tlvs(uint8 *tlv_buf, int buflen, uint key)
{
	uint8 *cp;
	int totlen;

	cp = tlv_buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= 2) {
		uint tag;
		int len;

		tag = *cp;
		len = *(cp +1);

		/* validate remaining totlen */
		if ((tag == key) && (totlen >= (len + 2)))
			return (cp);

		cp += (len + 2);
		totlen -= (len + 2);
	}

	return NULL;
}

static void
bcm_wps_version(uint8 *wps_ie)
{
	uint16 wps_len;
	uint16 wps_off, wps_suboff;
	uint16 wps_key;
	uint8 wps_field_len;

	wps_len = (uint16)*(wps_ie+TLV_LEN_OFF); /* Get the length of the WPS OUI header */
	wps_off = WPS_OUI_FIXED_HEADER_OFF; /* Skip the fixed headers */
	wps_field_len = 1;

	/* Parsing the OUI header looking for version number */
	while ((wps_len >= wps_off + 2) && (wps_field_len))
	{
		wps_key = (((uint8)wps_ie[wps_off]*256) + (uint8)wps_ie[wps_off+1]);
		if (wps_key == WPS_ID_VENDOR_EXT) {
			/* Key found */
			wps_suboff = wps_off + WPS_OUI_HEADER_SIZE;

			/* Looking for the Vendor extension code 0x00 0x37 0x2A
			 * and the Version 2 sudId 0x00
			 * if found then the next byte is the len of field which is always 1
			 * for version field the byte after is the version number
			 */
			if (!wlu_bcmp(&wps_ie[wps_suboff],  WFA_VENDOR_EXT_ID, WPS_OUI_LEN)&&
				(wps_ie[wps_suboff+WPS_WFA_SUBID_V2_OFF] == WPS_WFA_SUBID_VERSION2))
			{
				printf("V%d.%d ", (wps_ie[wps_suboff+WPS_WFA_V2_OFF]>>4),
				(wps_ie[wps_suboff+WPS_WFA_V2_OFF] & 0x0f));
				return;
			}
		}
		/* Jump to next field */
		wps_field_len = wps_ie[wps_off+WPS_OUI_HEADER_LEN+1];
		wps_off += WPS_OUI_HEADER_SIZE + wps_field_len;
	}

	/* If nothing found from the parser then this is the WPS version 1.0 */
	printf("V1.0 ");
}

static void
bcm_is_wps_configured(uint8 *wps_ie)
{
	/* Before calling this function the test of WPS_OUI type 4 should be already done
	 * If the contents match the WPS_OUI_SC_STATE
	 */
	uint16 wps_key;
	wps_key = (wps_ie[WPS_SCSTATE_OFF]*256) + wps_ie[WPS_SCSTATE_OFF+1];
	if ((wps_ie[TLV_LEN_OFF] > (WPS_SCSTATE_OFF+5))&&
		(wps_key == WPS_ID_SC_STATE))
	{
		switch (wps_ie[WPS_SCSTATE_OFF+WPS_OUI_HEADER_SIZE])
		{
			case WPS_SCSTATE_UNCONFIGURED:
				printf("Unconfigured\n");
				break;
			case WPS_SCSTATE_CONFIGURED:
				printf("Configured\n");
				break;
			default:
				printf("Unknown State\n");
		}
	}
}

/* Looking for WPS OUI in the propriatary_ie */
static bool
bcm_is_wps_ie(uint8 *ie, uint8 **tlvs, uint32 *tlvs_len)
{
	bool retval = FALSE;
	/* If the contents match the WPS_OUI and type=4 */
	if ((ie[TLV_LEN_OFF] > (WPS_OUI_LEN+1)) &&
		!wlu_bcmp(&ie[TLV_BODY_OFF], WPS_OUI "\x04", WPS_OUI_LEN + 1)) {
		retval = TRUE;
	}

	/* point to the next ie */
	ie += ie[TLV_LEN_OFF] + TLV_HDR_LEN;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return retval;
}

static bcm_tlv_t *bcm_find_vs_ie(uint8 *parse, int len,
	uint8 *oui, uint8 oui_len, uint8 oui_type)
{
	bcm_tlv_t *ie;

	while ((ie = bcm_parse_tlvs(parse, (int)len, DOT11_MNG_VS_ID))) {
		if (bcm_vs_ie_match((uint8 *)ie, oui, oui_len, oui_type))
			return ie;
		if ((ie = bcm_next_tlv(ie, &len)) == NULL)
			break;
	}
	return NULL;
}

/* vendor specific TLV match */
static bool bcm_vs_ie_match(uint8 *ie, uint8 *oui, int oui_len, uint8 type)
{
	/* If the contents match the OUI and the type */
	if (ie[TLV_LEN_OFF] >= oui_len + 1 &&
	    !wlu_bcmp(&ie[TLV_BODY_OFF], oui, oui_len) &&
	    type == ie[TLV_BODY_OFF + oui_len]) {
		return TRUE;
	}

	return FALSE;
}

static int
wl_hwareg(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_OK;
	char *p;
	wl_hwadbg_reg_param_t rwhwa;

	memset(&rwhwa, 0, sizeof(rwhwa));

	/* skip the command name */
	argv++;

	/* required arg: reg type */
	if ((p = *argv) == NULL) {
		err = BCME_USAGE_ERROR;
		goto exit;
	}
	strncpy(rwhwa.type, p, MIN(strlen(p), sizeof(rwhwa.type)));
	rwhwa.type[sizeof(rwhwa.type) - 1] = '\0';
	/* skip the type */
	argv++;

	/* required arg: reg offset */
	if ((p = *argv) == NULL) {
		err = BCME_USAGE_ERROR;
		goto exit;
	}
	rwhwa.offset = strtol(p, NULL, 0);
	/* skip the offset */
	argv++;

	/* optional arg: reg value */
	if ((p = *argv) == NULL) {
		uint32 val;
		/* GET cmd */
		if ((err = wlu_iovar_getbuf(wl, cmd->name, &rwhwa,
			sizeof(rwhwa), buf, WLC_IOCTL_SMLEN) < 0)) {
			goto exit;
		}
		val = *((uint32 *)buf);

		/* size is fixed at 4 */
		printf("0x%08x\n", dtoh32(val));
	} else {
		/* SET cmd */
		/* required arg: set value */
		if ((p = *argv) == NULL) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		rwhwa.w_val = htod32(strtol(p, NULL, 0));
		rwhwa.w_en = 1;
		argv++;

		if ((err = wlu_iovar_set(wl, cmd->name, &rwhwa, sizeof(rwhwa))) < 0) {
			printf("Error setting variable %s\n", cmd->name);
			return err;
		}
	}
exit:
	return (err);
}

int
wl_sta_supp_chan(void *wl, cmd_t *cmd, char **argv)
{
	wlc_supp_chan_list_t *sta_supp_chan;
	struct ether_addr ea;
	char *param;
	int buflen, err;
	uint8 *chanset = NULL;
	uint list_cnt, i;

	strcpy(buf, *argv);

	/* convert the ea string into an ea struct */
	if (!*++argv || !wl_ether_atoe(*argv, &ea)) {
		printf(" ERROR: no valid ether addr provided\n");
		return BCME_USAGE_ERROR;
	}

	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&ea, ETHER_ADDR_LEN);

	if ((err = wlu_get(wl, cmd->get, buf, sizeof(*sta_supp_chan))) < 0) {
		return err;
	}

	sta_supp_chan = (wlc_supp_chan_list_t *)buf;

	/* Report unrecognized version */
	if (sta_supp_chan->version > WL_STA_SUPP_CHAN_VER) {
		printf(" ERROR: unknown driver station info version %d\n", sta_supp_chan->version);
		return BCME_ERROR;
	}

	list_cnt = sta_supp_chan->count;
	chanset = (uint8 *)sta_supp_chan->chanset;

	printf("[VER %d] STA %s:\n", sta_supp_chan->version, *argv);
	printf("Supported Channels count = %d\n", list_cnt);
	printf("Channels :");
	for (i = 0; i < MAXCHANNEL; i++) {
		if (isset(chanset, i)) {
			printf(" %d", i);
		}
	}
	printf(" \n");

	return BCME_OK;
}

static int
wl_musched_cmn_cmd(void *wl, cmd_t *cmd, char **argv, uint16 flags)
{
	int ret, argc;
	int tmp1, tmp2;
	char *retbuf, *endptr;
	char **tmp_argv;
	uint musched_cmd_param_size;
	wl_musched_cmd_params_t *musched_params;
	struct ether_addr ea;
	int16 val16;

	BCM_REFERENCE(wl);

	retbuf = NULL;
	musched_params = NULL;
	ret = BCME_OK;
	musched_cmd_param_size = sizeof(wl_musched_cmd_params_t);

	argc = 0;
	tmp_argv = argv;
	while (*tmp_argv++) argc++;
	if (argc > 12) {
		fprintf(stderr, "Error: too many arguments argc: %d \n", argc);
		ret = BCME_BADARG;
		goto exit;
	}

	/* reset argc */
	argc = 0;

	if ((musched_params = malloc(musched_cmd_param_size)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for musched_cmd params\n",
		musched_cmd_param_size);
		ret = BCME_NOMEM;
		goto exit;
	}
	/* Init musched_params */
	memset(musched_params, 0, musched_cmd_param_size);
	musched_params->bw = -1;
	musched_params->ac = -1;
	musched_params->row = -1;
	musched_params->col = -1;

	/* Init flags based on the one passed from caller */
	musched_params->flags = flags;

	/* skip the command name itself */
	if (*++argv) {
		if (!strncmp(*argv, "-h", strlen("-h")) ||
			!strncmp(*argv, "help", strlen("help"))) {
			fprintf(stdout, "%s", cmd->help);
			ret = BCME_OK;
			goto exit;
		} else {
			/* if the first string is not help or -h, it is the key word string */
			strncpy(musched_params->keystr, *argv,
				MIN(strlen(*argv), WL_MUSCHED_CMD_KEYSTR_MAXLEN));
			argv++;
		}
	}

	while (*argv) {
		if (!strncmp(*argv, "-h", strlen("-h")) ||
			!strncmp(*argv, "help", strlen("help"))) {
			fprintf(stdout, "%s", cmd->help);
			ret = BCME_OK;
			goto exit;
		} else if (!strncmp(*argv, "-v", strlen("-v"))) {
			WL_MUSCHED_FLAGS_VERBOSE_SET(musched_params);
			argv++;
		} else if (!strncmp(*argv, "-u", strlen("-u"))) {
			/* convert the ea string into an ea struct */
			if (!*++argv || !wl_ether_atoe(*argv, &ea)) {
				fprintf(stderr, " ERROR: no valid ether addr provided %s\n", *argv);
				return BCME_USAGE_ERROR;
			}
			memcpy(&musched_params->ea, &ea, sizeof(ea));
			WL_MUSCHED_FLAGS_MACADDR_SET(musched_params);
			argv++;
		} else if (!strncmp(*argv, "-b", strlen("-b")) ||
			!strncmp(*argv, "--bw", strlen("--bw"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			val16 = strtol(*argv++, &endptr, 0);
			WL_MUSCHED_FLAGS_BW_SET(musched_params);
			if (val16 == 20)
				musched_params->bw = 0;
			else if (val16 == 40)
				musched_params->bw = 1;
			else if (val16 == 80)
				musched_params->bw = 2;
			else if (val16 == 160)
				musched_params->bw = 3;
			else if (val16 == -1)
				musched_params->bw = -1;
			else {
				fprintf(stderr, "Wrong bw %d\n", val16);
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
		} else if (!strncmp(*argv, "-q", strlen("-q")) ||
			!strncmp(*argv, "--ac", strlen("--ac"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			val16 = strtol(*argv++, &endptr, 0);
			WL_MUSCHED_FLAGS_AC_SET(musched_params);
			musched_params->ac = val16;
		} else if (!strncmp(*argv, "-r", strlen("-r"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			val16 = strtol(*argv++, &endptr, 0);
			musched_params->row = val16;
		} else if (!strncmp(*argv, "-c", strlen("-c"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			val16 = strtol(*argv++, &endptr, 0);
			musched_params->col = val16;
		} else if (!strncmp(*argv, "-ul", strlen("-ul"))) {
			argv++;
			WL_MUSCHED_FLAGS_UL_SET(musched_params);
		} else if (!strncmp(*argv, "-dl", strlen("-dl"))) {
			argv++;
			WL_MUSCHED_FLAGS_DL_SET(musched_params);
		} else if (!strncmp(*argv, "-bi", strlen("-bi"))) {
			argv++;
			WL_MUSCHED_FLAGS_UL_SET(musched_params);
			WL_MUSCHED_FLAGS_DL_SET(musched_params);
		} else {
			if (strstr(*argv, "[") != NULL && strstr(*argv, "]") != NULL) {
				/* search for pattern like [a long string] */
				strncpy(musched_params->argstr, *argv,
					MIN(strlen(*argv), WL_MUSCHED_CMD_ARGSTR_MAXLEN));
			} else if (strstr(*argv, "-") != NULL) {
				/* search for pattern like 1-9 */
				tmp1 = strtol(*argv, &endptr, 0);
				if (*endptr) {
					endptr++;
					tmp2 = strtol(endptr, &endptr, 0);
				} else {
					tmp2 = tmp1;
				}
				val16 = tmp1;
				while (musched_params->num_vals < WL_MUSCHED_CMD_VAL_MAXNUM - 1 &&
					val16 <= tmp2) {
					musched_params->vals[musched_params->num_vals++] = val16++;
				}
			} else if (strstr(*argv, ",") != NULL) {
				/* search for pattern like 1,2,3,4,5 */
				endptr = *argv;
				while (*endptr) {
					musched_params->vals[musched_params->num_vals] =
						strtoul(endptr, &endptr, 0);
					if (*endptr && *endptr == 's') {
						/* for the case setting secondary 80MHz RU */
						musched_params->vals[musched_params->num_vals] +=
							128;
						endptr++;
					}
					musched_params->num_vals++;
					if (*endptr) endptr++;
				}
			} else if (strstr(*argv, "s") != NULL) {
				/* setting RU for secondary 80MHz, e.g., 67s */
				musched_params->vals[musched_params->num_vals++] =
					strtol(*argv, &endptr, 0) + 128;
			} else {
				/* just a number */
				musched_params->vals[musched_params->num_vals++] =
					strtol(*argv, &endptr, 0);
			}
			argv++;
		}
	}

	if (musched_params->num_vals == 0) {
		/* get */
		if ((retbuf = malloc(WL_DUMP_BUF_LEN)) == NULL) {
			fprintf(stderr, "Error allocating return buffer for musched\n");
			ret = BCME_NOMEM;
			goto exit;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, musched_params,
			musched_cmd_param_size, retbuf, WL_DUMP_BUF_LEN)) == BCME_OK) {
			fputs(retbuf, stdout);
		}
	} else {
		/* set */
		ret = wlu_iovar_set(wl, cmd->name, musched_params, musched_cmd_param_size);
	}
exit:
	if (musched_params)
		free(musched_params);

	if (retbuf)
		free(retbuf);
	return ret;
}

static int
wl_musched_cmd(void *wl, cmd_t *cmd, char **argv)
{
	return wl_musched_cmn_cmd(wl, cmd, argv, 0);
}

static int
wl_umusched_cmd(void *wl, cmd_t *cmd, char **argv)
{
	uint32 flags = 0;
	flags |= WL_MUSCHED_FLAGS_UL_MASK;
	return wl_musched_cmn_cmd(wl, cmd, argv, flags);
}

static int
wl_txbfcfg_cmd(void *wl, cmd_t *cmd, char **argv)
{
	int ret, argc;
	char *retbuf, *endptr;
	char **tmp_argv;
	uint txbfcfg_params_size;
	wl_txbfcfg_params_t *txbfcfg_params;
	struct ether_addr ea;
	int16 val;
	bool isget;

	BCM_REFERENCE(wl);

	retbuf = NULL;
	txbfcfg_params = NULL;
	ret = BCME_OK;
	txbfcfg_params_size = sizeof(wl_txbfcfg_params_t);

	argc = 0;
	tmp_argv = argv;
	while (*tmp_argv++) argc++;
	if (argc > 12) {
		fprintf(stderr, "Error: too many arguments argc: %d \n", argc);
		ret = BCME_BADARG;
		goto exit;
	}

	/* reset argc */
	argc = 0;

	if ((txbfcfg_params = malloc(txbfcfg_params_size)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for txbfcfg_params params\n",
		txbfcfg_params_size);
		ret = BCME_NOMEM;
		goto exit;
	}
	/* Init txbfcfg_params */
	memset(txbfcfg_params, 0, txbfcfg_params_size);

	/* skip the command name itself */
	argv++;
	isget = TRUE;
	while (*argv) {
		if (!strncmp(*argv, "-h", strlen("-h")) ||
			!strncmp(*argv, "help", strlen("help"))) {
			fprintf(stdout, "%s", cmd->help);
			isget = FALSE;
			ret = BCME_OK;
			goto exit;
		} else if (!strncmp(*argv, "-v", strlen("-v"))) {
			WL_TXBFCFG_FLAGS_VERBOSE_SET(txbfcfg_params);
			argv++;
		} else if (!strncmp(*argv, "-u", strlen("-u"))) {
			/* convert the ea string into an ea struct */
			if (!*++argv || !wl_ether_atoe(*argv, &ea)) {
				fprintf(stderr, " ERROR: no valid ether addr provided %s\n", *argv);
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			memcpy(&txbfcfg_params->ea, &ea, sizeof(ea));
			argv++;
		} else {
			txbfcfg_params->index = strtol(*argv++, &endptr, 0);
			if (*argv) {
				val = strtol(*argv, &endptr, 0);
				isget = FALSE;
				argv++;
			} else {
				val = 0;
				WL_TXBFCFG_FLAGS_GETSEL_SET(txbfcfg_params);
				isget = TRUE;
			}
			txbfcfg_params->val = val;
		}
	}

	if (isget) {
		/* get */
		if ((retbuf = malloc(WL_DUMP_BUF_LEN)) == NULL) {
			fprintf(stderr, "Error allocating return buffer for txbfcfg\n");
			ret = BCME_NOMEM;
			goto exit;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, txbfcfg_params,
			txbfcfg_params_size, retbuf, WL_DUMP_BUF_LEN)) == BCME_OK) {
			fputs(retbuf, stdout);
		}
	} else {
		/* set */
		ret = wlu_iovar_set(wl, cmd->name, txbfcfg_params, txbfcfg_params_size);
	}
exit:
	if (txbfcfg_params)
		free(txbfcfg_params);

	if (retbuf)
		free(retbuf);
	return ret;
}

static int
wl_srcfg_cmd(void *wl, cmd_t *cmd, char **argv)
{
	const char dis_argv[] = "display";
	int ret, argc;
	char *retbuf;
	char **tmp_argv;
	uint srcfg_params_size;
	sr_config_t *srcfg_params;

	BCM_REFERENCE(wl);

	retbuf = NULL;
	srcfg_params = NULL;
	ret = BCME_OK;
	srcfg_params_size = sizeof(sr_config_t);

	argc = 0;
	tmp_argv = argv;
	while (*tmp_argv++) argc++;
	if (argc > 4) {
		fprintf(stderr, "Error: too many arguments argc: %d \n", argc);
		ret = BCME_BADARG;
		goto exit;
	}

	/* reset argc */
	argc = 0;

	if ((srcfg_params = malloc(srcfg_params_size)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for srcfg_params params\n",
		srcfg_params_size);
		ret = BCME_NOMEM;
		goto exit;
	}

	/* skip the command name */
	if (*++argv) {
		if (!strncmp(*argv, "-h", strlen("-h")) ||
			!strncmp(*argv, "help", strlen("help"))) {
			fprintf(stdout, "%s", cmd->help);
			ret = BCME_OK;
			goto exit;
		} else {
			/* if the first string is not help or -h, it is the command string */
			strncpy(srcfg_params->cmdstr, *argv, strlen(*argv));
			argv++;
		}
	} else {
		/* after the sr_config, there is nothing followed */
		ret = BCME_OK;
		strncpy(srcfg_params->cmdstr, dis_argv, strlen(dis_argv));
		srcfg_params->num_vals = 0;
		/* get the whole list */
		if ((retbuf = malloc(WL_DUMP_BUF_LEN)) == NULL) {
			fprintf(stderr, "Error allocating return buffer for srcfged\n");
			ret = BCME_NOMEM;
			goto exit;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, srcfg_params,
			srcfg_params_size, retbuf, WL_DUMP_BUF_LEN)) == BCME_OK) {
			fputs(retbuf, stdout);
		}

		goto exit;
	}

	if (*argv) {
		srcfg_params->val = strtol(*argv, NULL, 0);
		srcfg_params->num_vals = 1;
		fprintf(stdout, "%d", srcfg_params->val);
	} else {
		srcfg_params->num_vals = 0;
	}

	if (srcfg_params->num_vals == 0) {
		/* get the individual element */
		if ((retbuf = malloc(WL_DUMP_BUF_LEN)) == NULL) {
			fprintf(stderr, "Error allocating return buffer for srcfged\n");
			ret = BCME_NOMEM;
			goto exit;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, srcfg_params,
			srcfg_params_size, retbuf, WL_DUMP_BUF_LEN)) == BCME_OK) {
			fputs(retbuf, stdout);
		}
	} else if (srcfg_params->num_vals != 0) {
		/* set */
		ret = wlu_iovar_set(wl, cmd->name, srcfg_params, srcfg_params_size);
	}

exit:
	if (srcfg_params)
		free(srcfg_params);

	if (retbuf)
		free(retbuf);
	return ret;
}

static int
wl_monitor_config(void *wl, cmd_t *cmd, char **argv)
{
	int ret, argc;
	char *retbuf, *endptr;
	char **tmp_argv;
	bool isget;
	int i;
	uint mntcfg_param_size;
	wl_monitor_config_params_t *mntcfg_params;
	int16 val16;
	struct ether_addr ea;

	BCM_REFERENCE(wl);

	retbuf = NULL;
	mntcfg_params = NULL;
	ret = BCME_OK;
	mntcfg_param_size = sizeof(wl_monitor_config_params_t);
	isget = TRUE;

	argc = 0;
	tmp_argv = argv;
	while (*tmp_argv++) argc++;
	if (argc > 12) {
		fprintf(stderr, "Error: too many arguments argc: %d \n", argc);
		ret = BCME_BADARG;
		goto exit;
	}

	/* reset argc */
	argc = 0;

	if ((mntcfg_params = malloc(mntcfg_param_size)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for monitor_config params\n",
		mntcfg_param_size);
		ret = BCME_NOMEM;
		goto exit;
	}
	/* Init musched_params */
	memset(mntcfg_params, 0, mntcfg_param_size);
	mntcfg_params->he_bssidx_bmp = 1;

	++argv;

	while (*argv) {
		isget = FALSE;
		if (!strncmp(*argv, "-vht_aid", strlen("-vht_aid"))) {
			argv++;
			mntcfg_params->flags |= WL_MONITOR_CONFIG_FLAG_VHT_AID;
			if (*argv == NULL) {
				fprintf(stdout, "%s", cmd->help);
				ret = BCME_USAGE_ERROR;
				goto exit;
			} else if (strstr(*argv, ",") != NULL) {
				/* search for pattern like 1,2,3,4,5 */
				endptr = *argv;
				while (*endptr && (mntcfg_params->vht_num_aids <
					WL_MONITOR_CONFIG_NUM_AIDS)) {
					mntcfg_params->vht_aid[mntcfg_params->vht_num_aids] =
						strtoul(endptr, &endptr, 0);
					mntcfg_params->vht_num_aids++;
					if (*endptr) endptr++;
				}
			} else {
				val16 = strtol(*argv, &endptr, 0);
				mntcfg_params->vht_num_aids = WL_MONITOR_CONFIG_NUM_AIDS;
				for (i = 0; i < mntcfg_params->vht_num_aids; i++) {
					mntcfg_params->vht_aid[i] = val16;
				}
			}
		} else if (!strncmp(*argv, "-he_aid", strlen("-he_aid"))) {
			argv++;
			mntcfg_params->flags |= WL_MONITOR_CONFIG_FLAG_HE_AID;
			if (*argv == NULL) {
				fprintf(stdout, "%s", cmd->help);
				ret = BCME_USAGE_ERROR;
				goto exit;
			} else if (strstr(*argv, ",") != NULL) {
				/* search for pattern like 1,2,3,4,5 */
				endptr = *argv;
				while (*endptr &&
					(mntcfg_params->he_num_aids < WL_MONITOR_CONFIG_NUM_AIDS)) {
					mntcfg_params->he_aid[mntcfg_params->he_num_aids] =
						strtoul(endptr, &endptr, 0);
					mntcfg_params->he_num_aids++;
					if (*endptr) endptr++;
				}
			} else {
				val16 = strtol(*argv, &endptr, 0);
				mntcfg_params->he_num_aids = WL_MONITOR_CONFIG_NUM_AIDS;
				for (i = 0; i < mntcfg_params->he_num_aids; i++) {
					mntcfg_params->he_aid[i] = val16;
				}
			}
		} else if (!strncmp(*argv, "-aid", strlen("-aid"))) {
			argv++;
			mntcfg_params->flags |= (WL_MONITOR_CONFIG_FLAG_HE_AID |
				WL_MONITOR_CONFIG_FLAG_VHT_AID);
			if (*argv == NULL) {
				fprintf(stdout, "%s", cmd->help);
				ret = BCME_USAGE_ERROR;
				goto exit;
			} else if (strstr(*argv, ",") != NULL) {
				/* search for pattern like 1,2,3,4,5 */
				endptr = *argv;
				while (*endptr &&
					(mntcfg_params->he_num_aids < WL_MONITOR_CONFIG_NUM_AIDS)) {
					mntcfg_params->he_aid[mntcfg_params->he_num_aids] =
						strtoul(endptr, &endptr, 0);
					mntcfg_params->he_num_aids++;
					mntcfg_params->vht_aid[mntcfg_params->vht_num_aids] =
						strtoul(endptr, &endptr, 0);
					mntcfg_params->vht_num_aids++;
					if (*endptr) endptr++;
				}
			} else {
				val16 = strtol(*argv, &endptr, 0);
				mntcfg_params->he_num_aids = WL_MONITOR_CONFIG_NUM_AIDS;
				mntcfg_params->vht_num_aids = WL_MONITOR_CONFIG_NUM_AIDS;
				for (i = 0; i < mntcfg_params->he_num_aids; i++) {
					mntcfg_params->he_aid[i] = val16;
					mntcfg_params->vht_aid[i] = val16;
				}
			}
		} else if (!strncmp(*argv, "-bc", strlen("-bc"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			val16 = strtol(*argv++, &endptr, 0);
			mntcfg_params->bsscolor = val16;
			mntcfg_params->flags |= WL_MONITOR_CONFIG_FLAG_BC;
		} else if (!strncmp(*argv, "-gid", strlen("-gid"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			val16 = strtol(*argv++, &endptr, 0);
			mntcfg_params->gid = val16;
			mntcfg_params->flags |= WL_MONITOR_CONFIG_FLAG_GID;
		} else if (!strncmp(*argv, "-bssbmp", strlen("-bssbmp"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			val16 = strtol(*argv++, &endptr, 0);
			if (val16 > 3 || val16 <= 0) {
				ret = BCME_BADARG;
				goto exit;
			}
			mntcfg_params->he_bssidx_bmp = val16;
			mntcfg_params->flags |= WL_MONITOR_CONFIG_FLAG_BSSBMP;
		} else if (!strncmp(*argv, "-hetb", strlen("-hetb"))) {
			if (!*++argv) {
				ret = BCME_USAGE_ERROR;
				goto exit;
			}
			mntcfg_params->flags |= WL_MONITOR_CONFIG_FLAG_HETB;

			if (wl_ether_atoe(*argv, &ea)) {
				memcpy(&mntcfg_params->ea, &ea, ETHER_ADDR_LEN);
				mntcfg_params->flags |= WL_MONITOR_CONFIG_FLAG_HETB_MACADDR;
				mntcfg_params->hetb_en = TRUE;
			} else {
				val16 = strtol(*argv, &endptr, 0);
				if (val16 == 1) {
					mntcfg_params->hetb_en = TRUE;
				} else if (val16 == 0) {
					mntcfg_params->hetb_en = FALSE;
				} else {
					ret = BCME_BADARG;
					goto exit;
				}
			}
			argv++;
		} else if (!strncmp(*argv, "-h", strlen("-h")) ||
			!strncmp(*argv, "help", strlen("help"))) {
			fprintf(stdout, "%s", cmd->help);
			ret = BCME_OK;
			goto exit;
		} else {
			/* pass */
		}
		argv++;
	}

	if (isget) {
		/* get */
		if ((retbuf = malloc(WL_DUMP_BUF_LEN)) == NULL) {
			fprintf(stderr, "Error allocating return buffer for musched\n");
			ret = BCME_NOMEM;
			goto exit;
		}

		if ((ret = wlu_iovar_getbuf(wl, cmd->name, mntcfg_params,
			mntcfg_param_size, retbuf, WL_DUMP_BUF_LEN)) == BCME_OK) {
		}
	} else {
		/* set */
		ret = wlu_iovar_set(wl, cmd->name, mntcfg_params, mntcfg_param_size);
	}

exit:
	if (mntcfg_params)
		free(mntcfg_params);

	if (retbuf)
		free(retbuf);
	return ret;
}
#ifdef BCMDBG
static int
wl_wrr_handler(void *wl, cmd_t *cmd, char **argv)
{
	uint16 argc = 0;
	char **tmp_argv = argv;
	int ret = BCME_OK;
	uint n;
	wl_wrr_params_t wrr_params;

	/* Count number of arguments */
	while (*tmp_argv++) argc++;
	if ((!argc) || (argc > WL_WRR_MAX_ENTRIES)) {
		fprintf(stderr,
			"Error: too many arguments argc: %d max_entries:%d\n",
			argc, WL_WRR_MAX_ENTRIES);
		return BCME_BADARG;
	}

	/* argv[0] is the command name */
	if (argc == 1) {
		/* IOVAR Get operation */
		/* Get buffer */
		wl_wrr_params_t *reply = NULL;

		ret	 = wlu_var_getbuf(wl, cmd->name, &wrr_params,
				sizeof(wrr_params), (void **)&reply);
		if (ret) {
			return ret;
		}

		if (dtoh16(reply->entries) > WL_WRR_MAX_ENTRIES) {
			return BCME_BADARG;
		}

		for (n = 0; n < (dtoh16(reply->entries)); n++) {
			fprintf(stdout, "%d ",  dtoh16(reply->data[n]));
		}
		fprintf(stdout, "\n");
	} else {
		/* IOVAR Set operation */
		for (n = 1; n < argc; n++)
		{
			uint val;

			/* -1 or 0xFFFF turns off the feature */
			if (strncmp("-1", argv[n], strlen("-1")) == 0) {
				val = 0xFFFF;
			} else {
				val = atoi(argv[n]);
			}

			if (val > 0xFFFF) {
				fprintf(stderr,
					"Error: Invalid value:%d argv[%d]=%s\n",
					val, n, argv[n]);
				return BCME_BADARG;
			}
			wrr_params.data[n-1] = htod16(val);
		}
		wrr_params.entries = htod16(n-1);
		ret = wlu_var_setbuf(wl, cmd->name, &wrr_params, sizeof(wrr_params));
	}

	return ret;

}
#endif /* WLWRR */

#if defined(BCMDBG) || defined(BCMDBG_MU)
/*
 * Get or Set per AC MUTX mask.
 * "wl -i <interface_name> mutx_ac vo|vi|bk|be [mu_type]"
 * with no args for mask, print current MU types for given AC
 */
static int
wl_mutx_ac(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	uint8 ac, mask;
	char *param, *mutype;
	const char *cmdname = "mutx_ac";
	wl_mutx_ac_mg_t mutx_ac_mask, *reply;
	void *ptr = NULL;

	UNUSED_PARAMETER(cmd);

	if ((param = *++argv) == NULL)
		return BCME_USAGE_ERROR;

	if (strcmp(param, "be") == 0)
		ac = AC_BE;
	else if (strcmp(param, "bk") == 0)
		ac = AC_BK;
	else if (strcmp(param, "vi") == 0)
		ac = AC_VI;
	else if (strcmp(param, "vo") == 0)
		ac = AC_VO;
	else if (strcmp(param, "0") == 0 || strcmp(param, "1") == 0) {
		if (strcmp(param, "0") == 0) {
			mask = 0;
		} else {
			mask = MUTX_TYPE_MASK;
		}
		mutx_ac_mask.mask = htod32(mask);
		for (ac = 0; ac < AC_COUNT; ac++) {
			mutx_ac_mask.ac = htod32(ac);
			err = wlu_var_setbuf(wl, cmdname, &mutx_ac_mask, sizeof(mutx_ac_mask));
		}
		return err;
	} else {
		fprintf(stderr, "Invalid AC param %s\n", param);
		return BCME_USAGE_ERROR;
	}

	if ((mutype = *++argv) == NULL) {
		mutx_ac_mask.ac = htod32(ac);
		if ((err = wlu_var_getbuf(wl, cmdname, &mutx_ac_mask, sizeof(mutx_ac_mask),
				&ptr)) < 0)
		{
			return err;
		}
		reply = (wl_mutx_ac_mg_t *) ptr;
		reply->ac = dtoh32(reply->ac);
		reply->mask = dtoh32(reply->mask);
		printf("Access class '%s' MUTX enabled for:", param);
		if (reply->mask == 0)
		{
			printf(" none");
		} else {
			if (reply->mask & (1 << MUTX_TYPE_VHT))
				printf(" vhtmu");
			if (reply->mask & (1 << MUTX_TYPE_OFDMA))
				printf(" heofdma");
			if (reply->mask & (1 << MUTX_TYPE_HE))
				printf(" hemu");
		}
		printf("\n");
	} else {
		mask = 0;
		if (strcmp(mutype, "none") != 0 && strcmp(mutype, "0") != 0)
		{
			do {
				if (strcmp(mutype, "vhtmu") == 0)
					mask |= 1 << MUTX_TYPE_VHT;
				else if (strcmp(mutype, "heofdma") == 0)
					mask |= 1 << MUTX_TYPE_OFDMA;
				else if (strcmp(mutype, "hemu") == 0)
					mask |= 1 << MUTX_TYPE_HE;
				else {
					fprintf(stderr, "Invalid AC param %s\n", param);
					return BCME_USAGE_ERROR;
				}
			} while ((mutype = *++argv) != NULL);
		}
		mutx_ac_mask.ac = htod32(ac);
		mutx_ac_mask.mask = htod32(mask);
		err = wlu_var_setbuf(wl, cmdname, &mutx_ac_mask, sizeof(mutx_ac_mask));
	}
	return err;
}
static int
wl_macreq_cmd(void *wl, cmd_t *cmd, char **argv)
{
	wl_macreq_params_t macreq;
	int err = BCME_OK, i;
	char *p;
	char *retbuf;
	int val, strt, end;
	uint16 lidx_list[16] = {0};

	BCM_REFERENCE(wl);
	BCM_REFERENCE(cmd);
	memset(&macreq, 0, sizeof(macreq));
	retbuf = malloc(WL_DUMP_BUF_LEN);
	if (retbuf == NULL) {
		printf("No memory to allocate return buffer\n");
		return BCME_NOMEM;
	}

	/* skip the command name */
	argv++;

	/* Get the selection */
	if ((p = *argv) == NULL) {
		err = BCME_USAGE_ERROR;
		goto exit;
	}

	macreq.flag = 1; /* valid */
	macreq.type = 0;
	macreq.len = 0;
	memset(macreq.lidx_list, 0x00, sizeof(macreq.lidx_list));

	while (*argv) {
		if (!strncmp(*argv, "-h", strlen("-h")) ||
			!strncmp(*argv, "help", strlen("help"))) {
			fprintf(stdout, "%s", cmd->help);
			err = BCME_OK;
			goto exit;
		} else if (!strncmp(*argv, "-a", strlen("-a")) ||
			!strncmp(*argv, "addmtxv", strlen("addmtxv"))) {
			macreq.type = 2;
		} else if (!strncmp(*argv, "-d", strlen("-d")) ||
			!strncmp(*argv, "delmtxv", strlen("delmtxv"))) {
			macreq.type = 3;
		} else if (!strncmp(*argv, "updmtmr", strlen("updmtmr"))) {
			macreq.type = 6;
			argv++;
			p = *argv;
			macreq.len = 1;
			macreq.lidx_list[0] = strtoul(p, &p, 0);
			goto done;
		} else if (!strncmp(*argv, "-l", strlen("-l"))) {
			p = *argv;
			p = p+2;
			while (*p) {
				val = strtoul(p, &p, 0);
				if (val >= 256) {
					err = BCME_BADARG;
					goto exit;
				}
				if (*p) p++;
				setbit(lidx_list, val);
			}
		} else if (!strncmp(*argv, "-L", strlen("-L"))) {
			p = *argv;
			p = p+2;
			/* -Ix,y inclusively prints indexes x through y */
			if (*(p+1) == '\0' || *(p+2) == '\0') {
				err = BCME_BADARG;
				goto exit;
			}
			strt = bcm_strtoul(p, &p, 0);
			if (*p) p++;
			end = bcm_strtoul(p, &p, 0);
			if (end >= strt && end < 256) {
				for (i = strt; i <= end; i++) {
					setbit(lidx_list, i);
				}

			} else {
				err = BCME_BADARG;
				goto exit;
			}
		} else {
			err = BCME_BADARG;
			fprintf(stdout, "%s", cmd->help);
			goto exit;
		}
		argv++;
	}

	if (macreq.type == 3 && !macreq.len) {
		macreq.type = 5; // delete all;
	}
	for (i = 0; i < 256 && macreq.len < 8; i++) {
		if (isset(lidx_list, i)) {
			macreq.lidx_list[macreq.len++] = i;
		}
	}

	if (!macreq.len && (!(macreq.type == 5))) {
		err = BCME_BADARG;
		goto exit;
	}
	if (macreq.len > 16) {
		err = BCME_BADARG;
		goto exit;
	}
done:
	if ((err = wlu_iovar_set(wl, cmd->name, &macreq, sizeof(macreq)) < 0)) {
		goto exit;
	}

exit:
	if (retbuf) {
		free(retbuf);
	}
	return err;
}
#endif /* defined(BCMDBG) || defined(BCMDBG_MU) */

#if defined(BCMDBG)
static int
wl_cca_chan_util_interval(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	unsigned int interval;
	/* skip the command name */
	argv++;

	/* only switch -d for now */
	if (*argv != NULL && !strcmp(*argv, "-d")) {
		argv++;
		if (*argv == NULL || (interval = atoi(*argv)) <= 0) {
			printf("enter correct interval\n");
			return 0;
		}

		if ((err = wlu_iovar_set(wl, cmd->name, &interval, sizeof(unsigned int))) < 0) {
			return err;
		}
	} else {
		if ((err = wlu_iovar_get(wl, cmd->name, &interval, sizeof(unsigned int))) < 0) {
			return err;
		}
		printf("CCA utilization interval: %u secs\n", interval);
	}

	return err;
}

static int
wl_cca_chan_util_dur(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	unsigned int dur;
	/* skip the command name */
	argv++;

	/* only switch -d for now */
	if (*argv != NULL && !strcmp(*argv, "-d")) {
		argv++;
		if (*argv == NULL || (dur = htod32(atoi(*argv))) <= 0) {
			printf("enter correct duration in TUs min 30\n");
			return 0;
		}

		if (dtoh32(dur) < 30) {
			printf("Invalid duration. Enter duration in TUs min 30\n");
			return 0;
		}

		if ((err = wlu_iovar_set(wl, cmd->name, &dur, sizeof(unsigned int))) < 0) {
			return err;
		}
	} else {
		if ((err = wlu_iovar_get(wl, cmd->name, &dur, sizeof(unsigned int))) < 0) {
			return err;
		}
		printf("CCA Measurement interval: %u msecs\n", dur);
	}

	return err;
}

static int
wl_cca_chan_util(void *wl, cmd_t *cmd, char **argv)
{
	char buf[WLC_IOCTL_MAXLEN];
	int err = 0;
	char *ptr;

	/* skip the command name */
	argv++;

	if (*argv != NULL) {
		printf("Help: wl cca_chan_utilization\n");
		return err;
	}

	if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0,
		buf, WLC_IOCTL_MAXLEN)) < 0) {
		return err;
	}

		ptr = buf;
		fputs(ptr, stdout);

	return err;
}

static int
wl_cca_rx_util(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	unsigned int rx_util;
	/* skip the command name */
	argv++;

	/* only switch -d for now */
	if (*argv != NULL) {
		printf("Help: wl cca_rx_util\n" "Invalid parameter %s\n", *argv);
			return 0;

	} else {
		if ((err = wlu_iovar_get(wl, cmd->name, &rx_util, sizeof(unsigned int))) < 0) {
			return err;
		}
		printf("CCA Rx utilization: %u %%\n", rx_util);
	}

	return err;
}

static int
wl_cca_tx_util(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	unsigned int tx_util;
	/* skip the command name */
	argv++;

	/* only switch -d for now */
	if (*argv != NULL) {
		printf("Help: wl cca_tx_util\n" "Invalid parameter %s\n", *argv);
			return 0;
	} else {
		if ((err = wlu_iovar_get(wl, cmd->name, &tx_util, sizeof(unsigned int))) < 0) {
			return err;
		}
		printf("CCA Tx utilization: %u %%\n", tx_util);
	}

	return err;
}

static int
wl_cca_raw_stats(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	char cca_buf[WLC_IOCTL_MAXLEN];
	/* skip the command name */
	argv++;

	/* only switch -d for now */
	if (*argv != NULL) {
		printf("Help: wl cca_stats_raw\n" "Invalid parameter %s\n", *argv);
			return 0;
	} else {
		if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0,
		                            cca_buf, WLC_IOCTL_MAXLEN)) < 0) {
			return err;
		}
		fputs(cca_buf, stdout);
	}

	return err;
}

static int
wl_cca_delta_stats(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	char cca_buf[WLC_IOCTL_MAXLEN];
	/* skip the command name */
	argv++;

	/* only switch -d for now */
	if (*argv != NULL) {
		printf("Help: wl cca_stats_delta\n" "Invalid parameter %s\n", *argv);
			return 0;
	} else {
		if ((err = wlu_iovar_getbuf(wl, cmd->name, NULL, 0,
		                            cca_buf, WLC_IOCTL_MAXLEN)) < 0) {
			return err;
		}
		fputs(cca_buf, stdout);
	}

	return err;
}
#endif // endif
