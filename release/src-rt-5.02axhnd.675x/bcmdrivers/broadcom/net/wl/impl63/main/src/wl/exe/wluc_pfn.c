/*
 * wl pfn command module
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
 * $Id: wluc_pfn.c 774769 2019-05-07 08:46:22Z $
 */

#include <wlioctl.h>

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>

static cmd_func_t wl_pfn_set;
static cmd_func_t wl_pfn_add;
static cmd_func_t wl_pfn_ssid_param;
static cmd_func_t wl_pfn_add_bssid;
static cmd_func_t wl_pfn_cfg;
static cmd_func_t wl_pfn;
static cmd_func_t wl_pfnbest, wl_pfnbest_bssid;
static cmd_func_t wl_pfn_suspend;
static cmd_func_t wl_pfnlbest;
static cmd_func_t wl_pfn_mem;
static cmd_func_t wl_pfn_override;
static cmd_func_t wl_pfn_macaddr;
#if defined(linux)
static cmd_func_t wl_pfn_event_check;
#endif   /* linux */
static cmd_func_t wl_event_filter;
static cmd_func_t wl_pfn_roam_alert_thresh;
#ifdef WL_MPF
static cmd_func_t wl_pfn_mpfset;
static cmd_func_t wl_mpf_map;
static cmd_func_t wl_mpf_state;
#endif /* WL_MPF */

static cmd_t wl_pfn_cmds[] = {
	{ "pfnset", wl_pfn_set, -1, -1,
	"Configures preferred network offload parameter\n"
	"\tpfnset syntax is: pfnset [scanfrq xxxxx(30 sec)] [netimeout xxxx(60 sec)]"
	"[slowfrq xxxx(180 sec)] [bestn (2)|[1-BESTN_MAX]] [mscan (0)|[0-MSCAN_MAX]]"
	"[bdscan (0)|1] [adapt (off)|[smart, strict, slow]]"
	"[rssi_delta xxxx(30 dBm)] [sort (listorder)|rssi] [bkgscan (0)|1] [immediateevent (0)|1]"
	"[immediate 0|(1)] [repeat (10)|[1-20]] [exp (2)|[1-5]] [separate 0|(1)]"
	"[bestn_bssid (0)|1]"},
	{ "pfnadd", wl_pfn_add, -1, -1,
	"Adding SSID based preferred networks to monitor and connect\n"
	"\tpfnadd syntax is: pfnadd ssid <SSID> [hidden (0)|1]"
	"[imode (bss)|ibss] [same_network (0)|1] [imprecise (0)|1] [trig a|abg|bg]"
	"[clear] [no_aging (0)|1]"
	"[amode (open)|shared] [wpa_auth (wpadisabled)|wpapsk|wpa2psk|wpanone|any]"
	"[wsec WEP|TKIP|AES|TKIPAES] [suppress (neither)|found|lost] [rssi <rssi>(0 dBm)]\n"
	"Up to 16 SSID networks can be added together in one pfnadd\n"
	"\tTo specify more than one WPA methods, use a number (same format as wpa_auth iovar) "
	"as the parameter of wpa_auth (e.g. 0x84 for wpapsk and wpa2psk.)"},
	{ "pfn_ssid_cfg", wl_pfn_ssid_param, WLC_GET_VAR, WLC_SET_VAR,
	"Adding PFN SSID params to be used to determine FOUND when associated\n"
	"\tpfn_ssid_cfg syntax is: pfn_ssid_cfg [min5g_rssi <rssi>(-45 dBm)]\n"
	"\t[min2g_rssi <rssi>(-50 dBm)] [init_score_max (110)]\n"
	"\t[cur_bssid_bonus (0)] [same_ssid_bonus (0)] [secure_bonus (0)]\n"
	"\t[band_5g_bonus (0)] [clear]"},
	{ "pfnadd_bssid", wl_pfn_add_bssid, -1, -1,
	"Adding BSSID based preferred networks to monitor and connect\n"
	"\tpfnadd_bssid syntax is: pfnadd_bssid bssid <BSSID> [suppress (neither)|found|lost]"
	"[rssi <rssi>(0 dBm)]\n"
	"\tUp to 150 BSSIDs can be added together in one pfnadd_bssid"},
	{ "pfncfg", wl_pfn_cfg, -1, -1,
	"Configures channel list and report type\n"
	"Usage: pfncfg [channel <list>] [report <type>] [prohibited 1|0] [history_off 1|0]\n"
	"\treport <type> is ssidonly, bssidonly, or both (default: both)\n"
	"\tprohibited flag 1: allow and (passively) scan any channel (default 0)"},
	{ "pfn", wl_pfn, -1, -1,
	"Enable/disable preferred network off load monitoring\n"
	"\tpfn syntax is: pfn 0|1"},
	{ "pfnclear", wl_var_void, -1, WLC_SET_VAR,
	"Clear the preferred network list\n"
	"\tpfnclear syntax is: pfnclear"},
	{ "pfnbest", wl_pfnbest, -1, -1,
	"Get the best n networks in each of up to m scans, with 16bit timestamp\n"
	"\tpfnbest syntax is: pfnbest"},
	{ "pfnlbest", wl_pfnlbest, -1, -1,
	"Get the best n networks in each scan, up to m scans, with 32bit timestamp\n"
	"\tpfnbest syntax is: pfnlbest"},
	{ "pfnbest_bssid", wl_pfnbest_bssid, -1, -1,
	"Get the best n networks in each of up to m scans, without ssid\n"
	"\tpfnbest syntax is: pfnbest_bssid"},
	{ "pfnsuspend", wl_pfn_suspend, -1, -1,
	"Suspend/resume pno scan\n"
	"\tpfnsuspend syntax is: pfnsuspend 0|1"},
	{ "pfnmem", wl_pfn_mem, -1, -1,
	"Get supported mscan with given bestn\n"
	"\tpfnmem syntax is: pfnmscan bestn [1-BESTN_MAX]"},
#if defined(linux)
	{ "pfneventchk", wl_pfn_event_check, -1, -1,
	"Listen and prints the preferred network off load event from dongle\n"
	"\tpfneventchk syntax is: pfneventchk [ifname]"},
#endif   /* linux */
	{ "event_filter", wl_event_filter, -1, -1,
	"Set/get event filter\n"
	"\tevent_filter syntax is: event_filter [value]"},
	{ "pfn_roam_alert_thresh", wl_pfn_roam_alert_thresh, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set PFN and roam alert threshold\n"
	"\tUsage: wl pfn_roam_alert_thresh [pfn_alert_thresh] [roam_alert_thresh]"
	},
	{ "pfn_override", wl_pfn_override, WLC_GET_VAR, WLC_SET_VAR,
	"Temporary override for PNO scan parameters:\n"
	"    pfn_override [<start> <duration> scanfrq <secs> [netimeout <secs>]\n"
	"                 [adapt (off | smart | strict | slow)] [repeat cnt>]\n"
	"                 [exp <cnt>] [slowfrq <secs>]]\n"
	"    <start> is seconds until these parameters should be activated\n"
	"    <duration> is seconds these parameters should remain in force\n"
	"    Unspecified parameters use the values from pfnset."},
	{ "pfn_macaddr", wl_pfn_macaddr, WLC_GET_VAR, WLC_SET_VAR,
	"Private MAC address to use as source for PNO scans:\n"
	"    pfn_macaddr [<mac>]"},
#ifdef WL_MPF
	{ "pfn_mpfset", wl_pfn_mpfset, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set mpf-based pfn parameters\n"
	"    pfn_mpfset <groupid> [state <state> scanfrq <secs> [netimeout <secs>]\n"
	"                         [adapt (off | smart | strict | slow)] [repeat <cnt>]\n"
	"                         [exp <cnt>] [slowfrq <secs>] ] ...\n"
	"    <groupid> is 0 (SSID list) or 1 (BSSID list)\n"
	"    <state> is 0 thru 3\n"
	"    unspecified states or subparameters us the values from pfnset."},
	{ "mpf_map", wl_mpf_map, WLC_GET_VAR, WLC_SET_VAR,
	"  mpf_map [type <type>] <mask> <val>/<state>[/<name>] ...\n"
	"    <type> must be 0 if present no (effect)\n"
	"    <mask> and <val> are 16-bit hex values, max 1s allowed: 3\n"
	"    <state> is a small number, 0 thru N-1 (N is # of bit combos)\n"
	"    <name> is an optional string name for the state"},
	{ "mpf_state", wl_mpf_state, WLC_GET_VAR, WLC_SET_VAR,
	"  mpf_state [type <type>] [<state> | <name> | gpio ]\n"
	"  <mpf_state [type <type>] [<state> | <name> | gpio ]\n"
	"    <type> must be 0 if present no (effect)\n"
	"    <state> or <name>, if specified, override current value,\n"
	"    setting gpio returns to simply tracking hardware state"},
#endif /* WL_MPF */
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_pfn_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register pfn commands */
	wl_module_cmds_register(wl_pfn_cmds);
}

static int
wl_pfn_set(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_pfn_param_t pfn_param;

	UNUSED_PARAMETER(cmd);

	/* Setup default values */
	pfn_param.version = PFN_VERSION;
	/* Sorting based on list order, no back ground scan, no autoswitch,
	  * no immediate event report, no adaptvie scan, but immediate scan
	  */
	pfn_param.flags = (PFN_LIST_ORDER << SORT_CRITERIA_BIT | ENABLE << IMMEDIATE_SCAN_BIT);
	/* Scan frequency of 30 sec */
	pfn_param.scan_freq = 30;
	/* slow adapt scan is off by default */
	pfn_param.slow_freq = 0;
	/* RSSI margin of 30 dBm */
	pfn_param.rssi_margin = 30;
	/* Network timeout 60 sec */
	pfn_param.lost_network_timeout = 60;
	/* best n = 2 by default */
	pfn_param.bestn = DEFAULT_BESTN;
	/* mscan m=0 by default, so not record best networks by default */
	pfn_param.mscan = DEFAULT_MSCAN;
	/*  default repeat = 10 */
	pfn_param.repeat = DEFAULT_REPEAT;
	/* by default, maximum scan interval = 2^2*scan_freq when adaptive scan is turned on */
	pfn_param.exp = DEFAULT_EXP;

	while (*++argv) {
		if (!stricmp(*argv, "scanfrq")) {
			if (*++argv)
				pfn_param.scan_freq = atoi(*argv);
			else {
				fprintf(stderr, "Missing scanfrq option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "netimeout")) {
			if (*++argv)
				pfn_param.lost_network_timeout = atoi(*argv);
			else {
				fprintf(stderr, "Missing netimeout option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "rssi_delta")) {
			if (*++argv)
				pfn_param.rssi_margin = atoi(*argv);
			else {
				fprintf(stderr, "Missing rssi_delta option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "sort")) {
			if (*++argv) {
				pfn_param.flags &= ~SORT_CRITERIA_MASK;
				if (!stricmp(*argv, "listorder"))
					pfn_param.flags |= (PFN_LIST_ORDER << SORT_CRITERIA_BIT);
				else if (!stricmp(*argv, "rssi"))
					pfn_param.flags |= (PFN_RSSI << SORT_CRITERIA_BIT);
				else {
					fprintf(stderr, "Invalid sort option %s\n", *argv);
					return BCME_USAGE_ERROR;
				}
			} else {
				fprintf(stderr, "Missing sort option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "immediateevent")) {
			if (*++argv) {
				if (!stricmp(*argv, "1")) {
					pfn_param.flags |= IMMEDIATE_EVENT_MASK;
				} else if (!stricmp(*argv, "0")) {
					pfn_param.flags &= ~IMMEDIATE_EVENT_MASK;
				} else {
					fprintf(stderr, "Invalid immediateevent option\n");
					return BCME_USAGE_ERROR;
				}
			} else {
				fprintf(stderr, "Missing immediateevent option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "bkgscan")) {
			if (*++argv) {
				pfn_param.flags &= ~ENABLE_BKGRD_SCAN_MASK;
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << ENABLE_BKGRD_SCAN_BIT);
				else
					pfn_param.flags |= (DISABLE << ENABLE_BKGRD_SCAN_BIT);
			} else {
				fprintf(stderr, "Missing bkgscan option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "immediate")) {
			pfn_param.flags &= ~IMMEDIATE_SCAN_MASK;
			if (*++argv) {
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << IMMEDIATE_SCAN_BIT);
				else
					pfn_param.flags |= (DISABLE << IMMEDIATE_SCAN_BIT);
			} else {
				fprintf(stderr, "Missing immediate option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "bdscan")) {
			if (*++argv) {
				pfn_param.flags &= ~ENABLE_BD_SCAN_MASK;
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << ENABLE_BD_SCAN_BIT);
				else
					pfn_param.flags |= (DISABLE << ENABLE_BD_SCAN_BIT);
			} else {
				fprintf(stderr, "Missing bdscan option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "separate")) {
			if (*++argv) {
				pfn_param.flags &= ~REPORT_SEPERATELY_MASK;
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << REPORT_SEPERATELY_BIT);
				else
					pfn_param.flags |= (DISABLE << REPORT_SEPERATELY_BIT);
			} else {
				fprintf(stderr, "Missing seperate option\n");
				return -1;
			}
		} else if (!stricmp(*argv, "adapt")) {
			if (*++argv) {
				pfn_param.flags &= ~ENABLE_ADAPTSCAN_MASK;
				if (!stricmp(*argv, "off")) {
					pfn_param.flags |= (OFF_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else if (!stricmp(*argv, "smart")) {
					pfn_param.flags |= (SMART_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else if (!stricmp(*argv, "strict")) {
					pfn_param.flags |= (STRICT_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else if (!stricmp(*argv, "slow")) {
					pfn_param.flags |= (SLOW_ADAPT << ENABLE_ADAPTSCAN_BIT);
				} else {
					fprintf(stderr, "Invalid adaptive scan option %s\n", *argv);
					return BCME_USAGE_ERROR;
				}
			} else {
				fprintf(stderr, "Missing adaptive scan option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "bestn")) {
			pfn_param.bestn = atoi(*++argv);
		} else if (!stricmp(*argv, "mscan")) {
			pfn_param.mscan = atoi(*++argv);
		} else if (!stricmp(*argv, "repeat")) {
			pfn_param.repeat = atoi(*++argv);
			if (pfn_param.repeat < 1 || pfn_param.repeat > 20) {
				fprintf(stderr, "repeat %d out of range (1-20)\n",
					pfn_param.repeat);
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "exp")) {
			pfn_param.exp = atoi(*++argv);
			if (pfn_param.exp < 1 || pfn_param.exp > 5) {
				fprintf(stderr, "exp %d out of range (1-5)\n",
					pfn_param.exp);
				return BCME_BADARG;
			}
		} else if (!stricmp(*argv, "slowfrq")) {
			if (*++argv)
				pfn_param.slow_freq = atoi(*argv);
			else {
				fprintf(stderr, "Missing slowfrq option\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "bestn_bssid")) {
			if (*++argv) {
				pfn_param.flags &= ~BESTN_BSSID_ONLY_MASK;
				if (atoi(*argv))
					pfn_param.flags |= (ENABLE << BESTN_BSSID_ONLY_BIT);
				else
					pfn_param.flags |= (DISABLE << BESTN_BSSID_ONLY_BIT);
			} else {
				fprintf(stderr, "Missing bestn_bssid option\n");
				return BCME_USAGE_ERROR;
			}
		}  else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			return BCME_USAGE_ERROR;
		}
	}

	if ((((pfn_param.flags & ENABLE_ADAPTSCAN_MASK) ==
	    (SLOW_ADAPT << ENABLE_ADAPTSCAN_BIT)) &&
	    !pfn_param.slow_freq) ||
	    (((pfn_param.flags & ENABLE_ADAPTSCAN_MASK) !=
	    (SLOW_ADAPT << ENABLE_ADAPTSCAN_BIT)) &&
	    pfn_param.slow_freq)) {
		fprintf(stderr, "SLOW_ADAPT flag and slowfrq value not match\n");
		return BCME_BADARG;
	}
	pfn_param.version = htod32(pfn_param.version);
	pfn_param.scan_freq = htod32(pfn_param.scan_freq);
	pfn_param.lost_network_timeout = htod32(pfn_param.lost_network_timeout);
	pfn_param.flags = htod16(pfn_param.flags);
	pfn_param.rssi_margin = htod16(pfn_param.rssi_margin);
	pfn_param.slow_freq = htod32(pfn_param.slow_freq);

	if ((err = wlu_iovar_set(wl, "pfn_set", &pfn_param, sizeof(wl_pfn_param_t))))
		return (err);

	return (0);
}

static bool
validate_hex(char hexchar)
{
	if ((hexchar >= '0' && hexchar <= '9') ||
		(hexchar >= 'a' || hexchar <= 'z') ||
		(hexchar >= 'A' || hexchar <= 'Z'))
		return TRUE;
	else
		return FALSE;
}

static uint8
char2hex(char hexchar)
{
	if (hexchar >= '0' && hexchar <= '9')
		return (hexchar - '0');
	else if (hexchar >= 'a' && hexchar <= 'f')
		return (hexchar - 'a' + 10);
	else if (hexchar >= 'A' && hexchar <= 'F')
		return (hexchar - 'A' + 10);
	else
	{
		fprintf(stderr, "non-hex\n");
		return 0xff;
	}
}

#define MAXNUM_SSID_PER_ADD	16

static int
wl_pfn_add(void *wl, cmd_t *cmd, char **argv)
{
	int         err;
	wl_pfn_t    *p_pfn_element = NULL;
	int         i, pfn_element_len, cnt;
	wl_pfn_t    *pssidnet = NULL;
	uint32       val;

	UNUSED_PARAMETER(cmd);

	pfn_element_len = MAXNUM_SSID_PER_ADD * sizeof(wl_pfn_t);
	p_pfn_element = (wl_pfn_t *)malloc(pfn_element_len);
	if (p_pfn_element == NULL) {
		fprintf(stderr, "Failed to allocate buffer for %d bytes\n", pfn_element_len);
		return BCME_NOMEM;
	}
	memset(p_pfn_element, '\0', pfn_element_len);

	pssidnet = p_pfn_element;
	for (i = 0; i < MAXNUM_SSID_PER_ADD; i++) {
		/* Default setting, open, no WPA, no WEP and bss */
		pssidnet->auth = DOT11_OPEN_SYSTEM;
		pssidnet->wpa_auth = WPA_AUTH_DISABLED;
		pssidnet->wsec = 0;
		pssidnet->infra = 1;
		pssidnet->flags = 0;
		pssidnet++;
	}
	cnt = -1;
	pssidnet = p_pfn_element;
	while (*++argv) {
		if (!stricmp(*argv, "ssid")) {
			if (*++argv) {
				if (++cnt >= MAXNUM_SSID_PER_ADD) {
					fprintf(stderr, "exceed max 16 SSID per pfn_add\n");
					err = BCME_BADARG;
					goto error;
				}
				if (cnt > 0) {
					pssidnet->flags = htod32(pssidnet->flags);
					pssidnet++;
				}
				strncpy((char *)pssidnet->ssid.SSID, *argv,
				        sizeof(pssidnet->ssid.SSID));
				pssidnet->ssid.SSID_len =
				   strlen((char *)pssidnet->ssid.SSID);
				if (pssidnet->ssid.SSID_len > 32) {
					fprintf(stderr, "SSID too long: %s\n", *argv);
					err = BCME_BADARG;
					goto error;
				}
				pssidnet->ssid.SSID_len = htod32(pssidnet->ssid.SSID_len);
			} else {
				fprintf(stderr, "no value for ssid\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		}
		else if (!stricmp(*argv, "hidden")) {
			if (pssidnet->ssid.SSID_len == 0) {
				fprintf(stderr, "Wrong! Start with SSID\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
			if (*++argv) {
				val = **argv - '0';
				if (val != ENABLE && val != DISABLE) {
					fprintf(stderr, "invalid hidden setting, use 0/1\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				pssidnet->flags |= val << WL_PFN_HIDDEN_BIT;
			} else {
				fprintf(stderr, "no value for hidden\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		}  else if (!stricmp(*argv, "imode")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (!stricmp(*argv, "bss")) {
					pssidnet->infra = 1;
				} else if (!stricmp(*argv, "ibss")) {
					pssidnet->infra = 0;
				} else {
					fprintf(stderr, "Invalid imode arg %s\n", *argv);
					err = BCME_USAGE_ERROR;
					goto error;
				}
				pssidnet->infra = htod32(pssidnet->infra);
			} else {
				fprintf(stderr, "Missing option for imode\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "amode")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (!stricmp(*argv, "open"))
					pssidnet->auth = DOT11_OPEN_SYSTEM;
				else if (!stricmp(*argv, "shared"))
					pssidnet->auth = DOT11_SHARED_KEY;
				else {
					fprintf(stderr, "Invalid imode arg %s\n", *argv);
					err = BCME_USAGE_ERROR;
					goto error;
				}
				pssidnet->auth = htod32(pssidnet->auth);
			} else {
				fprintf(stderr, "Missing option for amode\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "wpa_auth")) {
			if (*++argv) {
				uint32 wpa_auth;
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}

				/* figure requested auth, allow "any" */
				if (!stricmp(*argv, "wpapsk"))
					pssidnet->wpa_auth = WPA_AUTH_PSK;
				else if (!stricmp(*argv, "wpa2psk"))
					pssidnet->wpa_auth = WPA2_AUTH_PSK;
				else if (!stricmp(*argv, "wpadisabled"))
					pssidnet->wpa_auth = WPA_AUTH_DISABLED;
				else if (!stricmp(*argv, "any"))
					pssidnet->wpa_auth = WPA_AUTH_PFN_ANY;
				else if ((wpa_auth = strtoul(*argv, 0, 0)))
					pssidnet->wpa_auth = wpa_auth;
				else {
					fprintf(stderr, "Invalid wpa_auth option %s\n", *argv);
					err = BCME_USAGE_ERROR;
					goto error;
				}
				pssidnet->wpa_auth = htod32(pssidnet->wpa_auth);
			} else {
				fprintf(stderr, "Missing option for wpa_auth\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "wsec")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (!stricmp(*argv, "WEP")) {
					pssidnet->wsec = WEP_ENABLED;
				} else if (!stricmp(*argv, "TKIP"))
					pssidnet->wsec = TKIP_ENABLED;
				else if (!stricmp(*argv, "AES"))
					pssidnet->wsec = AES_ENABLED;
				else if (!stricmp(*argv, "TKIPAES"))
					pssidnet->wsec = TKIP_ENABLED | AES_ENABLED;
				else {
					fprintf(stderr, "Invalid wsec option %s\n", *argv);
					err = BCME_USAGE_ERROR;
					goto error;
				}
				pssidnet->wsec = htod32(pssidnet->wsec);
			} else {
				fprintf(stderr, "Missing option for wsec\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "suppress")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (!stricmp(*argv, "found")) {
					pssidnet->flags |= WL_PFN_SUPPRESSFOUND_MASK;
				} else if (!stricmp(*argv, "lost")) {
					pssidnet->flags |= WL_PFN_SUPPRESSLOST_MASK;
				} else if (!stricmp(*argv, "neither")) {
					pssidnet->flags &=
					 ~(WL_PFN_SUPPRESSLOST_MASK | WL_PFN_SUPPRESSFOUND_MASK);
				} else {
					fprintf(stderr, "Invalid suppress option %s\n", *argv);
					err = BCME_USAGE_ERROR;
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for suppress\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "rssi")) {
			if (*++argv) {
				int rssi = atoi(*argv);
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (rssi >= -128 && rssi <= 0) {
					pssidnet->flags |= (rssi << WL_PFN_RSSI_SHIFT)
						& WL_PFN_RSSI_MASK;
				} else {
					fprintf(stderr, "Invalid rssi option %s\n", *argv);
					err = BCME_BADARG;
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for rssi\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "trig")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (!stricmp(*argv, "a")) {
					pssidnet->flags |= WL_PFN_SSID_A_BAND_TRIG;
				} else if (!stricmp(*argv, "bg")) {
					pssidnet->flags |= WL_PFN_SSID_BG_BAND_TRIG;
				} else if (!stricmp(*argv, "abg")) {
					pssidnet->flags |=
					  (WL_PFN_SSID_A_BAND_TRIG | WL_PFN_SSID_BG_BAND_TRIG);
				} else {
					fprintf(stderr, "Invalid trigger option %s\n", *argv);
					err = BCME_USAGE_ERROR;
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for trigger\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "same_network")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				val = **argv - '0';
				if (val != ENABLE && val != DISABLE) {
					fprintf(stderr, "invalid same_network setting, use 0/1\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (val) {
					pssidnet->flags |= WL_PFN_SSID_SAME_NETWORK;
				}
			} else {
				fprintf(stderr, "Missing option for same_network\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "no_aging")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				val = **argv - '0';
				if (val != ENABLE && val != DISABLE) {
					fprintf(stderr, "invalid same_network setting, use 0/1\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (val) {
					pssidnet->flags |= WL_PFN_SUPPRESS_AGING_MASK;
				}
			} else {
				fprintf(stderr, "Missing option for same_network\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "imprecise")) {
			if (*++argv) {
				if (pssidnet->ssid.SSID_len == 0) {
					fprintf(stderr, "Wrong! Start with SSID\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				val = **argv - '0';
				if (val != ENABLE && val != DISABLE) {
					fprintf(stderr, "invalid imprecise setting, use 0/1\n");
					err = BCME_USAGE_ERROR;
					goto error;
				}
				if (val) {
					pssidnet->flags |= WL_PFN_SSID_IMPRECISE_MATCH;
				}
			} else {
				fprintf(stderr, "Missing option for imprecise match\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "clear")) {
			if (*++argv) {
				if (cnt >= 1) {
					fprintf(stderr, "Will only clear, ssids ignored\n");
				}
				cnt = 0;
				pssidnet = p_pfn_element;
				pssidnet->flags = WL_PFN_FLUSH_ALL_SSIDS;
			} else {
				fprintf(stderr, "Missing option for clear\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		}	else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			err = BCME_USAGE_ERROR;
			goto error;
		}
	}
	pssidnet->flags = htod32(pssidnet->flags);

	pfn_element_len = (cnt + 1) * sizeof(wl_pfn_t);
	if ((err = wlu_iovar_set(wl, "pfn_add", p_pfn_element,
	     pfn_element_len))) {
		fprintf(stderr, "pfn_add fail\n");
		goto error;
	}
	free(p_pfn_element);
	return (0);

error:
	free(p_pfn_element);
	return err;
}

#define DEFAULT_INIT_SCORE_MAX  110
#define DEFAULT_MIN_5G_RSSI      -45
#define DEFAULT_MIN_2G_RSSI      -50

static int
wl_pfn_ssid_param(void *wl, cmd_t *cmd, char **argv)
{
	int i = 0, err = BCME_OK;
	wl_pfn_ssid_cfg_t cfg;
	char *endptr = NULL;
	uint16 val;

	UNUSED_PARAMETER(cmd);

	/* process a GET */
	if (!*(argv + 1)) {
		if (cmd->get < 0)
			return -1;
		memset(&cfg, 0, sizeof(cfg));
		if ((err = wlu_iovar_get(wl, "pfn_ssid_cfg", (void *)&cfg,
		   sizeof(wl_pfn_ssid_cfg_t))) < 0) {
			fprintf(stderr, "Failed to get pfn_ssid_cfg %d\n", err);
			return err;
		}
		if (cfg.version != WL_PFN_SSID_CFG_VERSION) {
			fprintf(stderr, "Mismatched version expected %d, got %d\n",
				WL_PFN_SSID_CFG_VERSION, cfg.version);
			return err;
		}
		fprintf(stderr, "min2G_rssi %d min5G_rssi %d init_score_max %d\n",
			cfg.params.min2G_rssi, cfg.params.min5G_rssi,
			dtoh16(cfg.params.init_score_max));
		fprintf(stderr, "band_5g_bonus %d secure_bonus %d same_ssid_bonus %d\n",
			dtoh16(cfg.params.band_5g_bonus), dtoh16(cfg.params.secure_bonus),
			dtoh16(cfg.params.same_ssid_bonus));
		fprintf(stderr, "cur_bssid_bonus %d\n", dtoh16(cfg.params.cur_bssid_bonus));
		return 0;
	}

	/* process a SET */
	cfg.version = htod16(WL_PFN_SSID_CFG_VERSION);
	cfg.flags = 0;
	cfg.params.band_5g_bonus = 0;
	cfg.params.secure_bonus = 0;
	cfg.params.same_ssid_bonus = 0;
	cfg.params.init_score_max = htod16(DEFAULT_INIT_SCORE_MAX);
	cfg.params.cur_bssid_bonus = 0;
	cfg.params.min2G_rssi = DEFAULT_MIN_2G_RSSI;
	cfg.params.min5G_rssi = DEFAULT_MIN_5G_RSSI;
	while (*++argv) {
		if (!stricmp(*argv, "min5g_rssi")) {
			if (*++argv) {
				int rssi = strtol(*argv, &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
				if (rssi >= -128 && rssi <= 0) {
					cfg.params.min5G_rssi = rssi;
				} else {
					fprintf(stderr, "Invalid rssi option %s\n", *argv);
					err = BCME_BADARG;
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for rssi\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}

		} else if (!stricmp(*argv, "min2g_rssi")) {
			if (*++argv) {
				int rssi = strtol(*argv, &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
				if (rssi >= -128 && rssi <= 0) {
					cfg.params.min2G_rssi = rssi;
				} else {
					fprintf(stderr, "Invalid rssi option %s\n", *argv);
					err = BCME_BADARG;
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for rssi\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "init_score_max")) {
			if (*++argv) {
				val = strtoul(*argv, &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
				cfg.params.init_score_max = htod16(val);
			} else {
				fprintf(stderr, "Missing option for init_score_max\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "cur_bssid_bonus")) {
			if (*++argv) {
				val = strtoul(*argv, &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
				cfg.params.cur_bssid_bonus = htod16(val);
			} else {
				fprintf(stderr, "Missing option for cur_bssid_bonus\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}

		} else if (!stricmp(*argv, "same_ssid_bonus")) {
			if (*++argv) {
				val = strtoul(*argv, &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
				cfg.params.same_ssid_bonus = htod16(val);
			} else {
				fprintf(stderr, "Missing option for same_ssid_bonus\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}

		} else if (!stricmp(*argv, "secure_bonus")) {
			if (*++argv) {
				val = strtoul(*argv, &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
				cfg.params.secure_bonus = htod16(val);
			} else {
				fprintf(stderr, "Missing option for secure_bonus\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "band_5g_bonus")) {
			if (*++argv) {
				val = strtoul(*argv, &endptr, 0);
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
				cfg.params.band_5g_bonus = htod16(val);
			} else {
				fprintf(stderr, "Missing option for band_5g_bonus\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}

		} else if (!stricmp(*argv, "clear")) {
			cfg.flags = WL_PFN_SSID_CFG_CLEAR;
			if (i) {
				fprintf(stderr, "Params will ONLY be cleared\n");
			}
		}
		i++;
	}
	if ((err = wlu_iovar_set(wl, "pfn_ssid_cfg", &cfg,
	   sizeof(wl_pfn_ssid_cfg_t)))) {
		fprintf(stderr, "Failed to set pfn_ssid_cfg %d\n", err);
	}
error:
	return err;
}

#define MAXNUM_BSSID_PER_ADD	180

static int
wl_pfn_add_bssid(void *wl, cmd_t *cmd, char **argv)
{
	int                 err;
	uint8               *ptr;
	int                 i, bssidlistlen, cnt;
	wl_pfn_bssid_t      *bssidlist;
	wl_pfn_bssid_t      *pbssid = NULL;

	UNUSED_PARAMETER(cmd);

	if (!*(argv + 1)) {
		fprintf(stderr, "Invalid command\n");
		return BCME_USAGE_ERROR;
	}

	bssidlistlen = MAXNUM_BSSID_PER_ADD * sizeof(wl_pfn_bssid_t);
	bssidlist = (wl_pfn_bssid_t *)malloc(bssidlistlen);
	if (bssidlist == NULL) {
		fprintf(stderr, "Failed to allocate buffer for %d bytes\n", bssidlistlen);
		return BCME_NOMEM;
	}
	memset(bssidlist, '\0', bssidlistlen);

	cnt = 0;
	while (*++argv) {
		if (!stricmp(*argv, "bssid")) {
			if (*++argv) {
				if (cnt >= MAXNUM_BSSID_PER_ADD) {
					fprintf(stderr, "exceed max %d BSSID per pfn_add_bssid\n",
						MAXNUM_BSSID_PER_ADD);
					err = BCME_BADARG;
					goto error;
				}
				if (!cnt)
					pbssid = bssidlist;
				else {
					pbssid->flags = htod16(pbssid->flags);
					pbssid++;
				}

				ptr = (uint8 *)*argv;
				for (i = 0; i < ETHER_ADDR_LEN; i++)
				{
					if (!validate_hex(*ptr) || !validate_hex(*(ptr + 1)))
					{
						fprintf(stderr, "non-hex in BSSID\n");
						err = BCME_BADARG;
						goto error;
					}
					pbssid->macaddr.octet[i] =
					      char2hex(*ptr) << 4 | char2hex(*(ptr+1));
					ptr += 3;
				}
				cnt++;
			} else {
				fprintf(stderr, "Missing option for bssid\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "suppress")) {
			if (!pbssid || ETHER_ISNULLADDR(pbssid->macaddr.octet)) {
				fprintf(stderr, "Wrong! Start with BSSID\n");
				err = BCME_BADARG;
				goto error;
			}
			if (*++argv) {
				if (!stricmp(*argv, "found")) {
					pbssid->flags |= WL_PFN_SUPPRESSFOUND_MASK;
				} else if (!stricmp(*argv, "lost")) {
					pbssid->flags |= WL_PFN_SUPPRESSLOST_MASK;
				} else if (!stricmp(*argv, "neither")) {
					pbssid->flags &=
					 ~(WL_PFN_SUPPRESSFOUND_MASK | WL_PFN_SUPPRESSLOST_MASK);
				} else {
					fprintf(stderr, "Invalid suppress option %s\n", *argv);
					err = BCME_USAGE_ERROR;
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for suppress\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else if (!stricmp(*argv, "rssi")) {
			if (*++argv) {
				int rssi = atoi(*argv);
				if (!pbssid || ETHER_ISNULLADDR(pbssid->macaddr.octet)) {
					fprintf(stderr, "Wrong! Start with BSSID\n");
					err = BCME_BADARG;
					goto error;
				}
				if (rssi >= -128 && rssi <= 0) {
					pbssid->flags |= (rssi << WL_PFN_RSSI_SHIFT)
						& WL_PFN_RSSI_MASK;
				} else {
					fprintf(stderr, "Invalid rssi option %s\n", *argv);
					err = BCME_BADARG;
					goto error;
				}
			} else {
				fprintf(stderr, "Missing option for rssi\n");
				err = BCME_USAGE_ERROR;
				goto error;
			}
		} else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			err = BCME_USAGE_ERROR;
			goto error;
		}
	}
	pbssid->flags = htod16(pbssid->flags);

	bssidlistlen = cnt * sizeof(wl_pfn_bssid_t);
	if ((err = wlu_iovar_setbuf(wl, "pfn_add_bssid", bssidlist,
	                            bssidlistlen, buf, ETHER_MAX_LEN))) {
		fprintf(stderr, "pfn_add_bssid fail\n");
		goto error;
	}
	free(bssidlist);
	return 0;

error:
	free(bssidlist);
	return err;
}

static int
wl_pfn_cfg(void *wl, cmd_t *cmd, char **argv)
{
	wl_pfn_cfg_t pfncfg_param;
	int          nchan = 0;
	int          err;

	UNUSED_PARAMETER(cmd);

	memset(&pfncfg_param, '\0', sizeof(wl_pfn_cfg_t));

	/* Setup default values */
	pfncfg_param.reporttype = WL_PFN_REPORT_ALLNET;
	pfncfg_param.channel_num = 0;

	while (*++argv) {
		if (!stricmp(*argv, "report")) {
			if (*++argv) {
				if (!stricmp(*argv, "all")) {
					pfncfg_param.reporttype = WL_PFN_REPORT_ALLNET;
				} else if (!stricmp(*argv, "ssidonly")) {
					pfncfg_param.reporttype = WL_PFN_REPORT_SSIDNET;
				} else if (!stricmp(*argv, "bssidonly")) {
					pfncfg_param.reporttype = WL_PFN_REPORT_BSSIDNET;
				} else {
					fprintf(stderr, "Invalid report option %s\n", *argv);
					return BCME_USAGE_ERROR;
				}
			} else {
				fprintf(stderr, "no value for report\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "channel")) {
			if (*++argv) {
				nchan = wl_parse_channel_list(*argv, pfncfg_param.channel_list,
				                              WL_NUMCHANNELS);
				if (nchan < 0) {
					fprintf(stderr, "error parsing channel\n");
					return BCME_BADARG;
				}
			} else {
				fprintf(stderr, "Missing option for channel\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "prohibited")) {
			if (*++argv) {
				pfncfg_param.flags &= ~WL_PFN_CFG_FLAGS_PROHIBITED;
				if (atoi(*argv))
					pfncfg_param.flags |= WL_PFN_CFG_FLAGS_PROHIBITED;
			} else {
				fprintf(stderr, "Missing prohibited option value\n");
				return BCME_USAGE_ERROR;
			}
		} else if (!stricmp(*argv, "history_off")) {
			if (*++argv) {
				pfncfg_param.flags &= ~WL_PFN_CFG_FLAGS_HISTORY_OFF;
				if (atoi(*argv))
					pfncfg_param.flags |= WL_PFN_CFG_FLAGS_HISTORY_OFF;
			} else {
				fprintf(stderr, "Missing history_off option value\n");
				return BCME_USAGE_ERROR;
			}
		} else {
			fprintf(stderr, "Invalid parameter %s\n", *argv);
			return BCME_USAGE_ERROR;
		}
	}

	pfncfg_param.reporttype = htod32(pfncfg_param.reporttype);
	pfncfg_param.channel_num = htod32(nchan);
	pfncfg_param.flags = htod32(pfncfg_param.flags);

	if ((err = wlu_iovar_set(wl, "pfn_cfg", &pfncfg_param,
	     sizeof(wl_pfn_cfg_t)))) {
		fprintf(stderr, "pfn_cfg fail\n");
		return err;
	}

	return 0;
}

static int
wl_pfn(void *wl, cmd_t *cmd, char **argv)
{
	int err, val;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		val = atoi(*argv);
		err = wlu_iovar_setint(wl, "pfn", (val ? 1 : 0));
	} else {
		err = wlu_iovar_getint(wl, "pfn", &val);
		if (!err)
			wl_printint(val);
	}

	return err;
}

/* Use MEDLEN for efficient read, pre-calculate maximum possible nets for later checks */
#define WL_MAX_BESTNET_V1 ((WLC_IOCTL_MEDLEN - OFFSETOF(wl_pfn_scanresults_v1_t, netinfo)) \
			   / sizeof(wl_pfn_net_info_v1_t))
#define WL_MAX_BESTNET_V2 ((WLC_IOCTL_MEDLEN - OFFSETOF(wl_pfn_scanresults_v2_t, netinfo)) \
			   / sizeof(wl_pfn_net_info_v2_t))

static int
wl_pfnbest(void *wl, cmd_t *cmd, char **argv)
{
	int	err;
	wl_pfn_scanresults_v1_t *bestnet_v1;
	wl_pfn_net_info_v1_t *netinfo_v1;
	wl_pfn_scanresults_v2_t *bestnet_v2;
	wl_pfn_net_info_v2_t *netinfo_v2;
	uint status;
	uint32 i, j;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		fprintf(stderr, "Invalid parameter %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* Use generic buffer to read and parse results */
	bestnet_v1 = (wl_pfn_scanresults_v1_t *)buf;
	bestnet_v2 = (wl_pfn_scanresults_v2_t *)buf;

	/* Read results until completion indicated */
	do {
		bzero(buf, WLC_IOCTL_MEDLEN);

		if ((err = wlu_iovar_get(wl, "pfnbest", (void*)buf, WLC_IOCTL_MEDLEN))) {
			fprintf(stderr, "pfnbest fail\n");
			return err;
		}

		if (bestnet_v2->version == PFN_SCANRESULTS_VERSION_V2) {
			/* Use version 2 variables for processing */
			status = bestnet_v2->status;
			if (bestnet_v2->count > WL_MAX_BESTNET_V2) {
				fprintf(stderr, "Invalid data, count %d exceeds max buflen\n",
					bestnet_v2->count);
				return BCME_ERROR;
			}

			printf("ver %d, status %d, count %d\n",
				bestnet_v2->version, bestnet_v2->status, bestnet_v2->count);
			netinfo_v2 = bestnet_v2->netinfo;
			for (i = 0; i < bestnet_v2->count; i++) {
				for (j = 0; j < netinfo_v2->pfnsubnet.SSID_len; j++)
					printf("%c", netinfo_v2->pfnsubnet.u.SSID[j]);
				printf("\n");
				printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
						netinfo_v2->pfnsubnet.BSSID.octet[0],
						netinfo_v2->pfnsubnet.BSSID.octet[1],
						netinfo_v2->pfnsubnet.BSSID.octet[2],
						netinfo_v2->pfnsubnet.BSSID.octet[3],
						netinfo_v2->pfnsubnet.BSSID.octet[4],
						netinfo_v2->pfnsubnet.BSSID.octet[5]);
				printf("channel: %d, RSSI: %d, timestamp: %d\n",
					netinfo_v2->pfnsubnet.channel, netinfo_v2->RSSI,
					netinfo_v2->timestamp);
				netinfo_v2++;
			}
		} else if (bestnet_v1->version == PFN_SCANRESULTS_VERSION_V1) {
			/* Use version 1 variables for processing */
			status = bestnet_v1->status;
			if (bestnet_v1->count > WL_MAX_BESTNET_V1) {
				fprintf(stderr, "Invalid data, count %d exceeds max buflen\n",
				        bestnet_v1->count);
				return BCME_ERROR;
			}

			printf("ver %d, status %d, count %d\n",
			       bestnet_v1->version, bestnet_v1->status, bestnet_v1->count);
			netinfo_v1 = bestnet_v1->netinfo;
			for (i = 0; i < bestnet_v1->count; i++) {
				for (j = 0; j < netinfo_v1->pfnsubnet.SSID_len; j++)
					printf("%c", netinfo_v1->pfnsubnet.SSID[j]);
				printf("\n");
				printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
						netinfo_v1->pfnsubnet.BSSID.octet[0],
						netinfo_v1->pfnsubnet.BSSID.octet[1],
						netinfo_v1->pfnsubnet.BSSID.octet[2],
						netinfo_v1->pfnsubnet.BSSID.octet[3],
						netinfo_v1->pfnsubnet.BSSID.octet[4],
						netinfo_v1->pfnsubnet.BSSID.octet[5]);
				printf("channel: %d, RSSI: %d, timestamp: %d\n",
					netinfo_v1->pfnsubnet.channel, netinfo_v1->RSSI,
					netinfo_v1->timestamp);
				netinfo_v1++;
			}
		} else {
			fprintf(stderr, "Unrecognized version %d\n", bestnet_v1->version);
			return BCME_ERROR;
		}
	} while (status != PFN_COMPLETE);

	return 0;
}

/* Use MEDLEN for efficient read, pre-calculate maximum possible nets for later checks */
#define WL_MAX_LBESTNET_V1 ((WLC_IOCTL_MEDLEN - OFFSETOF(wl_pfn_lscanresults_v1_t, netinfo)) \
			    / sizeof(wl_pfn_net_info_v1_t))
#define WL_MAX_LBESTNET_V2 ((WLC_IOCTL_MEDLEN - OFFSETOF(wl_pfn_lscanresults_v2_t, netinfo)) \
			    / sizeof(wl_pfn_lnet_info_v2_t))

static int
wl_pfnlbest(void *wl, cmd_t *cmd, char **argv)
{
	int	err;
	wl_pfn_lscanresults_v1_t *bestnet_v1;
	wl_pfn_lnet_info_v1_t *netinfo_v1;
	wl_pfn_lscanresults_v2_t *bestnet_v2;
	wl_pfn_lnet_info_v2_t *netinfo_v2;
	uint status;
	uint32 i, j;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		fprintf(stderr, "Invalid parameter %s\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* Use generic buffer to read and parse results */
	bestnet_v1 = (wl_pfn_lscanresults_v1_t *)buf;
	bestnet_v2 = (wl_pfn_lscanresults_v2_t *)buf;

	/* Read results until completion indicated */
	do {
		bzero(buf, WLC_IOCTL_MEDLEN);

		if ((err = wlu_iovar_get(wl, "pfnlbest", (void *)buf, WLC_IOCTL_MEDLEN))) {
			fprintf(stderr, "pfnlbest fail\n");
			return err;
		}

		if (bestnet_v2->version == PFN_LBEST_SCAN_RESULT_VERSION_V2) {
			/* Use version 2 variables for processing */
			status = bestnet_v2->status;
			if (bestnet_v2->count > WL_MAX_LBESTNET_V2) {
				fprintf(stderr, "Invalid data, count %d exceeds max buflen\n",
					bestnet_v2->count);
				return BCME_ERROR;
			}

			printf("ver %d, status %d, count %d\n",
				bestnet_v2->version, bestnet_v2->status, bestnet_v2->count);
			netinfo_v2 = bestnet_v2->netinfo;
			for (i = 0; i < bestnet_v2->count; i++) {
				for (j = 0; j < netinfo_v2->pfnsubnet.SSID_len; j++)
					printf("%c", netinfo_v2->pfnsubnet.u.SSID[j]);
				printf("\n");
				printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
						netinfo_v2->pfnsubnet.BSSID.octet[0],
						netinfo_v2->pfnsubnet.BSSID.octet[1],
						netinfo_v2->pfnsubnet.BSSID.octet[2],
						netinfo_v2->pfnsubnet.BSSID.octet[3],
						netinfo_v2->pfnsubnet.BSSID.octet[4],
						netinfo_v2->pfnsubnet.BSSID.octet[5]);
				printf("channel: %d, flags: %d, RSSI: %d, timestamp: %d\n",
						netinfo_v2->pfnsubnet.channel, netinfo_v2->flags,
						netinfo_v2->RSSI, netinfo_v2->timestamp);
				printf("RTT0: %d, RTT1: %d\n", netinfo_v2->rtt0, netinfo_v2->rtt1);
				netinfo_v2++;
			}
		} else if (bestnet_v1->version == PFN_LBEST_SCAN_RESULT_VERSION_V1) {
			/* Use version 1 variables for processing */
			status = bestnet_v1->status;
			if (bestnet_v1->count > WL_MAX_LBESTNET_V1) {
				fprintf(stderr, "Invalid data, count %d exceeds max buflen\n",
				        bestnet_v1->count);
				return BCME_ERROR;
			}

			printf("ver %d, status %d, count %d\n",
			       bestnet_v1->version, bestnet_v1->status, bestnet_v1->count);
			netinfo_v1 = bestnet_v1->netinfo;
			for (i = 0; i < bestnet_v1->count; i++) {
				for (j = 0; j < netinfo_v1->pfnsubnet.SSID_len; j++)
					printf("%c", netinfo_v1->pfnsubnet.SSID[j]);
				printf("\n");
				printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
						netinfo_v1->pfnsubnet.BSSID.octet[0],
						netinfo_v1->pfnsubnet.BSSID.octet[1],
						netinfo_v1->pfnsubnet.BSSID.octet[2],
						netinfo_v1->pfnsubnet.BSSID.octet[3],
						netinfo_v1->pfnsubnet.BSSID.octet[4],
						netinfo_v1->pfnsubnet.BSSID.octet[5]);
				printf("channel: %d, flags: %d, RSSI: %d, timestamp: %d\n",
						netinfo_v1->pfnsubnet.channel, netinfo_v1->flags,
						netinfo_v1->RSSI, netinfo_v1->timestamp);
				printf("RTT0: %d, RTT1: %d\n", netinfo_v1->rtt0, netinfo_v1->rtt1);
				netinfo_v1++;
			}
		} else {
			fprintf(stderr, "Unrecognized version %d\n", bestnet_v1->version);
			return BCME_ERROR;
		}
	} while (status == PFN_INCOMPLETE);

	return 0;
}

static int
wl_pfnbest_bssid(void *wl, cmd_t *cmd, char **argv)
{
	int	err;
	wl_pfn_scanhist_bssid_t *bestnet;
	wl_pfn_net_info_bssid_t *netinfo;
	uint32 i;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		fprintf(stderr, "Invalid parameter %s\n", *argv);
		return BCME_ERROR;
	}

	/* Use generic buffer to read and parse results */
	bestnet = (wl_pfn_scanhist_bssid_t *)buf;

	do {
		memset(bestnet, 0, WLC_IOCTL_MEDLEN);

		if ((err = wlu_iovar_get(wl, "pfnbest_bssid",
		                         (void *)bestnet, WLC_IOCTL_MEDLEN))) {
			fprintf(stderr, "pfnbest_bssid fail\n");
			return err;
		}

		printf("ver %d, status %d, count %d\n",
		       bestnet->version, bestnet->status, bestnet->count);
		netinfo = bestnet->netinfo;
		for (i = 0; i < bestnet->count; i++) {
			printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
			        netinfo->BSSID.octet[0],
			        netinfo->BSSID.octet[1],
			        netinfo->BSSID.octet[2],
			        netinfo->BSSID.octet[3],
			        netinfo->BSSID.octet[4],
			        netinfo->BSSID.octet[5]);
			printf("channel: %d, RSSI: %d, timestamp: %d\n",
			       netinfo->channel, netinfo->RSSI, netinfo->timestamp);
			netinfo++;
		}
	} while (bestnet->status != PFN_COMPLETE);

	return 0;
}

static int
wl_pfn_suspend(void *wl, cmd_t *cmd, char **argv)
{
	int	err, val;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {
		val = atoi(*argv);
		err = wlu_iovar_setint(wl, "pfn_suspend", (val ? 1 : 0));
	} else {
		err = wlu_iovar_getint(wl, "pfn_suspend", &val);
		if (!err)
			wl_printint(val);
	}

	return err;
}

static int
wl_pfn_mem(void *wl, cmd_t *cmd, char **argv)
{
	int	err, val;

	UNUSED_PARAMETER(cmd);

	if (*++argv && !stricmp(*argv, "bestn")) {
		if (*++argv)
			val = atoi(*argv);
		else {
			fprintf(stderr, "Missing bestn value\n");
			return -1;
		}
	} else {
		fprintf(stderr, "Missing bestn option\n");
		return -1;
	}

	err = wlu_iovar_setint(wl, "pfnmem", val);
	if (err) {
		fprintf(stderr, "pfnmem set wrong!\n");
		return err;
	}

	err = wlu_iovar_getint(wl, "pfnmem", &val);
	if (!err)
		wl_printint(val);
	else
		fprintf(stderr, "pfnmem get wrong!\n");
	return err;
}

#if defined(linux)
static void
wl_pfn_printnet(void *ptr, int event_type)
{
	wl_pfn_net_info_v1_t *netinfo_v1;
	wl_pfn_net_info_v2_t *netinfo_v2;
	uint32 i, j;

	if (WLC_E_PFN_NET_FOUND == event_type) {
		printf("WLC_E_PFN_NET_FOUND:\n");
	} else if (WLC_E_PFN_NET_LOST == event_type) {
		printf("WLC_E_PFN_NET_LOST:\n");
	} else if (WLC_E_PFN_BSSID_NET_FOUND == event_type) {
		printf("WLC_E_PFN_BSSID_NET_FOUND:\n");
	} else if (WLC_E_PFN_BSSID_NET_LOST == event_type) {
		printf("WLC_E_PFN_BSSID_NET_LOST:\n");
	} else {
		return;
	}

	if (((wl_pfn_scanresults_v2_t *)ptr)->version == PFN_SCANRESULTS_VERSION_V2) {
		netinfo_v2 = ((wl_pfn_scanresults_v2_t *)ptr)->netinfo;
		printf("ver %d, status %d, count %d\n",
			((wl_pfn_scanresults_v2_t *)ptr)->version,
			((wl_pfn_scanresults_v2_t *)ptr)->status,
			((wl_pfn_scanresults_v2_t *)ptr)->count);
		for (i = 0; i < ((wl_pfn_scanresults_v2_t *)ptr)->count; i++) {
			printf("%d. ", i + 1);
			for (j = 0; j < netinfo_v2->pfnsubnet.SSID_len; j++)
				printf("%c", netinfo_v2->pfnsubnet.u.SSID[j]);
			printf("\n");
			printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
					netinfo_v2->pfnsubnet.BSSID.octet[0],
					netinfo_v2->pfnsubnet.BSSID.octet[1],
					netinfo_v2->pfnsubnet.BSSID.octet[2],
					netinfo_v2->pfnsubnet.BSSID.octet[3],
					netinfo_v2->pfnsubnet.BSSID.octet[4],
					netinfo_v2->pfnsubnet.BSSID.octet[5]);
			printf("channel %d, RSSI %d, timestamp %d\n",
			       netinfo_v2->pfnsubnet.channel, netinfo_v2->RSSI,
			       netinfo_v2->timestamp);
			netinfo_v2++;
		}
	} else  if (((wl_pfn_scanresults_v1_t *)ptr)->version == PFN_SCANRESULTS_VERSION_V1) {
		netinfo_v1 = ((wl_pfn_scanresults_v1_t *)ptr)->netinfo;
		printf("ver %d, status %d, count %d\n",
			((wl_pfn_scanresults_v1_t *)ptr)->version,
			((wl_pfn_scanresults_v1_t *)ptr)->status,
			((wl_pfn_scanresults_v1_t *)ptr)->count);
		for (i = 0; i < ((wl_pfn_scanresults_v1_t *)ptr)->count; i++) {
			printf("%d. ", i + 1);
			for (j = 0; j < netinfo_v1->pfnsubnet.SSID_len; j++)
				printf("%c", netinfo_v1->pfnsubnet.SSID[j]);
			printf("\n");
			printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
					netinfo_v1->pfnsubnet.BSSID.octet[0],
					netinfo_v1->pfnsubnet.BSSID.octet[1],
					netinfo_v1->pfnsubnet.BSSID.octet[2],
					netinfo_v1->pfnsubnet.BSSID.octet[3],
					netinfo_v1->pfnsubnet.BSSID.octet[4],
					netinfo_v1->pfnsubnet.BSSID.octet[5]);
			printf("channel %d, RSSI %d, timestamp %d\n",
			       netinfo_v1->pfnsubnet.channel, netinfo_v1->RSSI,
			       netinfo_v1->timestamp);

			netinfo_v1++;
		}
	}
}

static int
wl_pfn_event_check(void *wl, cmd_t *cmd, char **argv)
{
	int                 fd, err;
	struct sockaddr_ll	sll;
	struct ifreq        ifr;
	char                ifnames[IFNAMSIZ] = {"eth1"};
	bcm_event_t         * event;
	char                data[512];
	int                 event_type;
	struct ether_addr   *addr;
	char                eabuf[ETHER_ADDR_STR_LEN];
	wl_pfn_scanresults_v1_t *ptr_v1;
	wl_pfn_net_info_v1_t   *info_v1;
	wl_pfn_scanresults_v2_t *ptr_v2;
	wl_pfn_net_info_v2_t	*info_v2;
	uint32              i, j;
	uint32              foundcnt, lostcnt;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	/* Override default ifname explicitly or implicitly */
	if (*++argv) {
		if (strlen(*argv) >= IFNAMSIZ) {
			printf("Interface name %s too long\n", *argv);
			return BCME_USAGE_ERROR;
		}
		strncpy(ifnames, *argv, IFNAMSIZ);
	} else {
		strncpy(ifnames, ((struct ifreq *)wl)->ifr_name, (IFNAMSIZ - 1));
	}

	ifnames[IFNAMSIZ - 1] = '\0';

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifnames, IFNAMSIZ);

	fd = socket(PF_PACKET, SOCK_RAW, hton16(ETHER_TYPE_BRCM));
	if (fd < 0) {
		printf("Cannot create socket %d\n", fd);
		return BCME_ERROR;
	}

	err = ioctl(fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		close(fd);
		return BCME_ERROR;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = hton16(ETHER_TYPE_BRCM);
	sll.sll_ifindex = ifr.ifr_ifindex;
	err = bind(fd, (struct sockaddr *)&sll, sizeof(sll));
	if (err < 0) {
		printf("Cannot get index %d\n", err);
		close(fd);
		return BCME_ERROR;
	}

	/* Pre-set the results pointers for any data we might receive */
	ptr_v1 = (wl_pfn_scanresults_v1_t *)(data + sizeof(bcm_event_t));
	ptr_v2 = (wl_pfn_scanresults_v2_t *)(data + sizeof(bcm_event_t));

	while (1) {
		recv(fd, data, sizeof(data), 0);
		event = (bcm_event_t *)data;
		addr = (struct ether_addr *)&(event->event.addr);

		event_type = ntoh32(event->event.event_type);

		if (addr != NULL) {
			sprintf(eabuf, "%02x:%02x:%02x:%02x:%02x:%02x",
				(uchar)addr->octet[0]&0xff,
				(uchar)addr->octet[1]&0xff,
				(uchar)addr->octet[2]&0xff,
				(uchar)addr->octet[3]&0xff,
				(uchar)addr->octet[4]&0xff,
				(uchar)addr->octet[5]&0xff);
		}

		if (ntoh32(event->event.datalen)) {
			if (WLC_E_PFN_SCAN_COMPLETE == event_type) {
				/* Version check for PFN */
				if (ptr_v2->version == PFN_SCANRESULTS_VERSION_V2) {
					info_v2 = ptr_v2->netinfo;
					foundcnt = ptr_v2->count & 0xffff;
					lostcnt = ptr_v2->count >> 16;
					printf("ver %d, status %d, found %d, lost %d\n",
						ptr_v2->version, ptr_v2->status,
					       foundcnt, lostcnt);
					if (foundcnt)
						printf("Network found:\n");
					for (i = 0; i < foundcnt; i++) {
						printf("%d. ", i + 1);
						for (j = 0; j < info_v2->pfnsubnet.SSID_len; j++)
							printf("%c", info_v2->pfnsubnet.u.SSID[j]);
						printf("\n");
						printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
								info_v2->pfnsubnet.BSSID.octet[0],
								info_v2->pfnsubnet.BSSID.octet[1],
								info_v2->pfnsubnet.BSSID.octet[2],
								info_v2->pfnsubnet.BSSID.octet[3],
								info_v2->pfnsubnet.BSSID.octet[4],
								info_v2->pfnsubnet.BSSID.octet[5]);
						printf("channel %d, RSSI %d, timestamp %d\n",
							info_v2->pfnsubnet.channel, info_v2->RSSI,
							info_v2->timestamp);
						info_v2++;
					}
					if (lostcnt)
						printf("Network lost:\n");
					for (i = 0; i < lostcnt; i++) {
						printf("%d. ", i + 1);
						for (j = 0; j < info_v2->pfnsubnet.SSID_len; j++)
							printf("%c", info_v2->pfnsubnet.u.SSID[j]);
						printf("\n");
						printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
								info_v2->pfnsubnet.BSSID.octet[0],
								info_v2->pfnsubnet.BSSID.octet[1],
								info_v2->pfnsubnet.BSSID.octet[2],
								info_v2->pfnsubnet.BSSID.octet[3],
								info_v2->pfnsubnet.BSSID.octet[4],
								info_v2->pfnsubnet.BSSID.octet[5]);
						printf("channel %d, RSSI %d, timestamp %d\n",
							info_v2->pfnsubnet.channel, info_v2->RSSI,
							info_v2->timestamp);
						info_v2++;
					}
				} else if (ptr_v1->version == PFN_SCANRESULTS_VERSION_V1) {
					info_v1 = ptr_v1->netinfo;
					foundcnt = ptr_v1->count & 0xffff;
					lostcnt = ptr_v1->count >> 16;
					printf("ver %d, status %d, found %d, lost %d\n",
						ptr_v1->version, ptr_v1->status,
					       foundcnt, lostcnt);
					if (foundcnt)
						printf("Network found:\n");
					for (i = 0; i < foundcnt; i++) {
						printf("%d. ", i + 1);
						for (j = 0; j < info_v1->pfnsubnet.SSID_len; j++)
							printf("%c", info_v1->pfnsubnet.SSID[j]);
						printf("\n");
						printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
								info_v1->pfnsubnet.BSSID.octet[0],
								info_v1->pfnsubnet.BSSID.octet[1],
								info_v1->pfnsubnet.BSSID.octet[2],
								info_v1->pfnsubnet.BSSID.octet[3],
								info_v1->pfnsubnet.BSSID.octet[4],
								info_v1->pfnsubnet.BSSID.octet[5]);
						printf("channel %d, RSSI %d, timestamp %d\n",
							info_v1->pfnsubnet.channel, info_v1->RSSI,
							info_v1->timestamp);
						info_v1++;
					}
					if (lostcnt)
						printf("Network lost:\n");
					for (i = 0; i < lostcnt; i++) {
						printf("%d. ", i + 1);
						for (j = 0; j < info_v1->pfnsubnet.SSID_len; j++)
							printf("%c", info_v1->pfnsubnet.SSID[j]);
						printf("\n");
						printf("BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
								info_v1->pfnsubnet.BSSID.octet[0],
								info_v1->pfnsubnet.BSSID.octet[1],
								info_v1->pfnsubnet.BSSID.octet[2],
								info_v1->pfnsubnet.BSSID.octet[3],
								info_v1->pfnsubnet.BSSID.octet[4],
								info_v1->pfnsubnet.BSSID.octet[5]);
						printf("channel %d, RSSI %d, timestamp %d\n",
							info_v1->pfnsubnet.channel, info_v1->RSSI,
							info_v1->timestamp);
						info_v1++;
					}
				} else {
					fprintf(stderr, "Unrecognized version %d\n",
					        ptr_v1->version);
					return BCME_ERROR;
				}
			} else if ((WLC_E_PFN_NET_FOUND == event_type) ||
			           (WLC_E_PFN_NET_LOST == event_type) ||
			           (WLC_E_PFN_BSSID_NET_FOUND == event_type) ||
			           (WLC_E_PFN_BSSID_NET_LOST == event_type)) {
				wl_pfn_printnet((void *)(data + sizeof(bcm_event_t)), event_type);
			}

			if (WLC_E_LINK == event_type) {
				if (ntoh16(event->event.flags) & WLC_EVENT_MSG_LINK)
					printf("MACEVENT Link up :%s\n", eabuf);
				else
					printf("MACEVENT Link down :%s\n", eabuf);
			}
		} else {
			if (WLC_E_PFN_SCAN_NONE == event_type) {
				printf("Got WLC_E_PFN_SCAN_NONE\n");
			}
			if (WLC_E_PFN_SCAN_ALLGONE == event_type) {
				printf("Got WLC_E_PFN_SCAN_ALLGONE\n");
			}
			if (WLC_E_PFN_BEST_BATCHING == event_type) {
				printf("Got WLC_E_PFN_BEST_BATCHING\n");
			}
		}
	}

	close(fd);
	return (0);
}
#endif /* linux */

static int
wl_event_filter(void *wl, cmd_t *cmd, char **argv)
{
	int     err;
	uint8   event_inds_mask[WL_EVENTING_MASK_LEN];  /* event bit mask */

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	memset(event_inds_mask, '\0', WL_EVENTING_MASK_LEN);

	/* Register for following event for pfn */
	event_inds_mask[WLC_E_LINK / 8] |= 1 << (WLC_E_LINK % 8);
	event_inds_mask[WLC_E_PFN_NET_FOUND / 8] |= 1 << (WLC_E_PFN_NET_FOUND % 8);
	event_inds_mask[WLC_E_PFN_NET_LOST / 8] |= 1 << (WLC_E_PFN_NET_LOST % 8);
	event_inds_mask[WLC_E_PFN_SCAN_NONE/ 8] |= 1 << (WLC_E_PFN_SCAN_NONE % 8);
	event_inds_mask[WLC_E_PFN_SCAN_ALLGONE/ 8] |= 1 << (WLC_E_PFN_SCAN_ALLGONE % 8);

	if ((err = wlu_iovar_set(wl, "event_msgs", &event_inds_mask, WL_EVENTING_MASK_LEN)))
		return (err);

	return (0);
}

static int
wl_pfn_roam_alert_thresh(void *wl, cmd_t *cmd, char **argv)
{
	int err, buflen;
	wl_pfn_roam_thresh_t *pfn_roam_alert;

	buflen = sprintf(buf, "%s", *argv) + 1;

	if (*++(argv) == NULL) {
		buf[buflen] = '\0';
		err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN);
		if (err < 0)
			return err;

		pfn_roam_alert = (wl_pfn_roam_thresh_t *)buf;
		printf("pfn_alert_thresh %u\n", pfn_roam_alert->pfn_alert_thresh);
		printf("roam_alert_thresh %u\n", pfn_roam_alert->roam_alert_thresh);
		return 0;

	} else {
		pfn_roam_alert = (wl_pfn_roam_thresh_t *) (buf + buflen);
		buflen += sizeof(wl_pfn_roam_thresh_t);

		pfn_roam_alert->pfn_alert_thresh = (uint32) strtoul(*argv, NULL, 0);

		if (*++(argv) == NULL) {
			printf("Incorrect number of arguments\n");
			return BCME_ERROR;
		}
		pfn_roam_alert->roam_alert_thresh = (uint32) strtoul(*argv, NULL, 0);

		if (*++(argv)) {
			printf("extra arguments\n");
			return BCME_ERROR;
		}
		err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

		return err;
	}
	return 0;
}

static char *adaptname[] = { "off", "smart", "strict", "slow" };

static int
wl_pfn_override(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_pfn_override_param_t pfnov_params;
	wl_pfn_override_param_t *pfnov_paramp;
	wl_pfn_mpf_state_params_t *statep;
	char *endptr;

	uint16 start, duration;

	/* Initialize the structure (both set and get) */
	memset(&pfnov_params, 0, sizeof(pfnov_params));
	pfnov_params.version = dtoh16(WL_PFN_OVERRIDE_VERSION);

	if (*++argv == NULL) {
		/* No arguments: do a GET and display results */
		err = wlu_iovar_getbuf(wl, cmd->name,
		                       &pfnov_params, sizeof(pfnov_params),
		                       buf, WLC_IOCTL_MAXLEN);
		if (err >= 0) {
			pfnov_paramp = (wl_pfn_override_param_t *)buf;
			if (dtoh16(pfnov_paramp->version) != WL_PFN_OVERRIDE_VERSION) {
				fprintf(stderr, "Incorrect version (%d != %d).\n",
				        dtoh16(pfnov_paramp->version), WL_PFN_OVERRIDE_VERSION);
				return -1;
			}
			start = dtoh16(pfnov_paramp->start_offset);
			duration = dtoh16(pfnov_paramp->duration);

			if (duration) {
				if (start == 0)
					printf("Active, remaining duration %d\n", duration);
				else
					printf("Scheduled in %d, duration %d\n", start, duration);

				/* Disaply the actual parameters */
				statep = &pfnov_paramp->override;
				statep->scan_freq = dtoh32(statep->scan_freq);
				statep->lost_network_timeout =
				        dtoh32(statep->lost_network_timeout);
				statep->flags = dtoh16(statep->flags);
				statep->slow_freq = dtoh32(statep->slow_freq);

				printf("Scan frequency: %d\n", statep->scan_freq);
				if (statep->lost_network_timeout)
					printf("Net timeout: %d\n",
					       dtoh32(statep->lost_network_timeout));
				if (statep->flags & WL_PFN_MPF_ADAPT_ON_MASK) {
					uint atype;
					atype = statep->flags & WL_PFN_MPF_ADAPTSCAN_MASK;
					atype = atype >> WL_PFN_MPF_ADAPTSCAN_BIT;
					printf("Adapt: %s\n", adaptname[atype]);
				}
				if (statep->exp)
					printf("  Exp: %d\n", statep->exp);
				if (statep->repeat)
					printf("  Repeat: %d\n", statep->repeat);
				if (statep->slow_freq)
					printf("  Slow: %d\n", statep->slow_freq);
			} else {
				printf("Override not configured\n");
			}
		}
	} else {
		/* Additional arguments: parse and do a set */

		/* Start field (required) */
		start = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Non-numeric start value %s\n", *argv);
			return -1;
		}

		/* Duration field (required) */
		if (*++argv == NULL) {
			fprintf(stderr, "Duration required\n");
			return -1;
		}
		duration = strtoul(*argv, &endptr, 0);
		argv++;

		/* Allow duration and start both 0 to mean cancel, otherwise
		 * zero duration is meaningless.  A nonzero duration requires
		 * a scan frequency at a minimum
		 */
		if (!duration) {
			if (start) {
				fprintf(stderr, "Start with no duration invalid\n");
				return -1;
			}
		} else {
			/* Read the actual timing parameters */
			statep = &pfnov_params.override;
			if ((*argv == NULL) || strcmp(*argv, "scanfrq")) {
				fprintf(stderr, "scanfrq required as first parameter\n");
				return -1;
			}

			if (argv[1] == NULL) {
				fprintf(stderr, "Missing value for '%s'\n", *argv);
				return -1;
			}
			argv++;
			statep->scan_freq = strtoul(*argv, &endptr, 0);
			if (*endptr != '\0') {
				fprintf(stderr, "Non-numeric scanfrq %s\n", *argv);
				return -1;
			}
			argv++;

			/* As long as there was a scan_freq, pick up other options */
			while (*argv && statep->scan_freq) {
				if (*argv && !strcmp(*argv, "netimeout")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->lost_network_timeout = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric netimeout %s\n", *argv);
						return -1;
					}
					argv++;
				} else if (*argv && !strcmp(*argv, "adapt")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					if (!strcmp(*argv, "off")) {
						statep->flags |=
						        (OFF_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else if (!strcmp(*argv, "smart")) {
						statep->flags |=
						        (SMART_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else if (!strcmp(*argv, "strict")) {
						statep->flags |=
						        (STRICT_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else if (!strcmp(*argv, "slow")) {
						statep->flags |=
						        (SLOW_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else {
						fprintf(stderr,
						        "Invalid adaptive scan option %s\n",
						        *argv);
						return -1;
					}
					statep->flags |= WL_PFN_MPF_ADAPT_ON_MASK;
					argv++;
				} else if (*argv && !strcmp(*argv, "repeat")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->repeat = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric repeat value %s\n", *argv);
						return -1;
					}
					argv++;
				} else if (*argv && !strcmp(*argv, "exp")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->exp = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric exp value %s\n", *argv);
						return -1;
					}
					argv++;
				} else if (*argv && !strcmp(*argv, "slowfrq")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->slow_freq = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric slowfrq value %s\n", *argv);
						return -1;
					}
					argv++;
				} else {
					/* Not a recognized option, exit loop */
					break;
				}
			}

			statep->scan_freq = htod32(statep->scan_freq);
			statep->lost_network_timeout = htod32(statep->lost_network_timeout);
			statep->flags = htod16(statep->flags);
			statep->slow_freq = htod32(statep->slow_freq);
		}

		/* Should be no leftover args at this point */
		if (*argv) {
			fprintf(stderr, "Input error at %s\n", *argv);
			return -1;
		}

		/* Fill in the start and duration fields */
		pfnov_params.start_offset = htod16(start);
		pfnov_params.duration = htod16(duration);

		/* Now do the set */
		err = wlu_iovar_setbuf(wl, cmd->name,
		                       &pfnov_params, sizeof(pfnov_params), buf, WLC_IOCTL_MAXLEN);
	}

	return err;
}

static int
wl_pfn_macaddr(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	wl_pfn_macaddr_cfg_t pfn_mac;

	UNUSED_PARAMETER(cmd);

	memset(&pfn_mac, 0, sizeof(pfn_mac));
	pfn_mac.version = WL_PFN_MACADDR_CFG_VER;

	if (argv[1] != NULL) {
		if (!wl_ether_atoe(argv[1], &pfn_mac.macaddr)) {
			fprintf(stderr, "Invalid MAC address %s\n", *argv);
			return -1;
		}

		return wlu_iovar_set(wl, "pfn_macaddr", &pfn_mac, sizeof(pfn_mac));
	} else {
		if ((ret = wlu_iovar_get(wl, "pfn_macaddr", &pfn_mac, sizeof(pfn_mac))) < 0)
			return ret;

		printf("%s\n", wl_ether_etoa(&pfn_mac.macaddr));
	}

	return ret;
}

#ifdef WL_MPF
static int
wl_pfn_mpfset(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_pfn_mpf_param_t mpf_params;
	wl_pfn_mpf_param_t *mpf_paramp;
	wl_pfn_mpf_state_params_t *statep;
	char *endptr;
	uint state;

	if (*++argv == NULL) {
		fprintf(stderr, "Requires at least <groupid>\n");
		return -1;
	}

	/* Initialize the structure (both set and get) */
	memset(&mpf_params, 0, sizeof(mpf_params));
	mpf_params.version = dtoh16(WL_PFN_MPF_VERSION);
	mpf_params.groupid = dtoh16(strtoul(*argv, &endptr, 0));
	if (*endptr != '\0') {
		fprintf(stderr, "Non-numeric groupid %s\n", *argv);
		return -1;
	}

	if (*++argv == NULL) {
		/* No arguments beyond groupid: do a GET and display results */
		err = wlu_iovar_getbuf(wl, cmd->name,
		                       &mpf_params, OFFSETOF(wl_pfn_mpf_param_t, state),
		                       buf, WLC_IOCTL_MAXLEN);
		if (err >= 0) {
			mpf_paramp = (wl_pfn_mpf_param_t *)buf;
			if (dtoh16(mpf_paramp->version) != WL_PFN_MPF_VERSION) {
				fprintf(stderr, "Incorrect version (%d != %d).\n",
				        dtoh16(mpf_paramp->version), WL_PFN_MPF_VERSION);
				return -1;
			}
			if (mpf_paramp->groupid != mpf_params.groupid) {
				fprintf(stderr, "Groupid modified (%d -> %d)?\n",
				        dtoh16(mpf_params.groupid), dtoh16(mpf_paramp->groupid));
				return -1;
			}

			for (state = 0, statep = mpf_paramp->state;
			     state < WL_PFN_MPF_STATES_MAX; state++, statep++) {
				if (!statep->scan_freq)
					continue;

				statep->scan_freq = dtoh32(statep->scan_freq);
				statep->lost_network_timeout =
				        dtoh32(statep->lost_network_timeout);
				statep->flags = dtoh16(statep->flags);
				statep->slow_freq = dtoh32(statep->slow_freq);

				printf("State %d:\n"
				       "  Scan frequency: %d\n",
				       state, statep->scan_freq);
				if (statep->lost_network_timeout)
					printf("  Net timeout: %d\n",
					       dtoh32(statep->lost_network_timeout));
				if (statep->flags & WL_PFN_MPF_ADAPT_ON_MASK) {
					uint atype;
					atype = statep->flags & WL_PFN_MPF_ADAPTSCAN_MASK;
					atype = atype >> WL_PFN_MPF_ADAPTSCAN_BIT;
					printf("  Adapt: %s\n", adaptname[atype]);
				}
				if (statep->exp)
					printf("  Exp: %d\n", statep->exp);
				if (statep->repeat)
					printf("  Repeat: %d\n", statep->repeat);
				if (statep->slow_freq)
					printf("  Slow: %d\n", statep->slow_freq);
			}
		}
	} else {
		/* Additional arguments: parse and do a set */
		while (*argv && !strcmp(*argv, "state")) {
			if (argv[1] == NULL) {
				fprintf(stderr, "Missing value for '%s'\n", *argv);
				return -1;
			}
			argv++;
			state = strtoul(*argv, &endptr, 0);
			if (*endptr != '\0') {
				fprintf(stderr, "Non-numeric state %s\n", *argv);
				return -1;
			}
			if (state >= WL_PFN_MPF_STATES_MAX) {
				fprintf(stderr, "Invalid state %d\n", state);
				return -1;
			}
			argv++;

			/* Set up for this state and get the scan frequency */
			statep = &mpf_params.state[state];
			if (statep->scan_freq) {
				fprintf(stderr, "Repeated state (%d)\n", state);
				return -1;
			}

			if (*argv && !strcmp(*argv, "scanfrq")) {
				if (argv[1] == NULL) {
					fprintf(stderr, "Missing value for '%s'\n", *argv);
					return -1;
				}
				argv++;
				statep->scan_freq = strtoul(*argv, &endptr, 0);
				if (*endptr != '\0') {
					fprintf(stderr, "Non-numeric scanfrq %s\n", *argv);
					return -1;
				}
			} else {
				fprintf(stderr, "State requires scanfrq\n");
				return -1;
			}
			argv++;

			/* If scan_freq is 0, don't allow other options */
			if (!statep->scan_freq)
				continue;

			/* Pick up any other options */
			while (*argv) {
				if (*argv && !strcmp(*argv, "netimeout")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->lost_network_timeout = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric netimeout %s\n", *argv);
						return -1;
					}
					argv++;
				} else if (*argv && !strcmp(*argv, "adapt")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					if (!strcmp(*argv, "off")) {
						statep->flags |=
						        (OFF_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else if (!strcmp(*argv, "smart")) {
						statep->flags |=
						        (SMART_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else if (!strcmp(*argv, "strict")) {
						statep->flags |=
						        (STRICT_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else if (!strcmp(*argv, "slow")) {
						statep->flags |=
						        (SLOW_ADAPT << WL_PFN_MPF_ADAPTSCAN_BIT);
					} else {
						fprintf(stderr,
						        "Invalid adaptive scan option %s\n",
						        *argv);
						return -1;
					}
					statep->flags |= WL_PFN_MPF_ADAPT_ON_MASK;
					argv++;
				} else if (*argv && !strcmp(*argv, "repeat")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->repeat = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric repeat value %s\n", *argv);
						return -1;
					}
					argv++;
				} else if (*argv && !strcmp(*argv, "exp")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->exp = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric exp value %s\n", *argv);
						return -1;
					}
					argv++;
				} else if (*argv && !strcmp(*argv, "slowfrq")) {
					if (argv[1] == NULL) {
						fprintf(stderr,
						        "Missing value for '%s'\n", *argv);
						return -1;
					}
					argv++;
					statep->slow_freq = strtoul(*argv, &endptr, 0);
					if (*endptr != '\0') {
						fprintf(stderr,
						        "Non-numeric slowfrq value %s\n", *argv);
						return -1;
					}
					argv++;
				} else {
					/* Not a recognized option, exit inner loop */
					break;
				}
			}

			statep->scan_freq = htod32(statep->scan_freq);
			statep->lost_network_timeout = htod32(statep->lost_network_timeout);
			statep->flags = htod16(statep->flags);
			statep->slow_freq = htod32(statep->slow_freq);
		}

		/* Should be no leftover args at this point */
		if (*argv) {
			fprintf(stderr, "Input error at %s\n", *argv);
			return -1;
		}

		/* Now do the set */
		err = wlu_iovar_setbuf(wl, cmd->name,
		                       &mpf_params, sizeof(mpf_params), buf, WLC_IOCTL_MAXLEN);
	}

	return err;
}

static int
wl_mpf_map(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_mpf_map_t map;
	wl_mpf_map_t *mapp;
	wl_mpf_val_t *valp, *valp2;
	uint16 mask, val, state, type = 0;
	uint8 count;
	char *endptr;
	uint bitcount;

	argv++;

	memset(&map, 0, sizeof(map));
	map.version = dtoh16(WL_MPF_VERSION);

	if (*argv && !strcmp(*argv, "type")) {
		argv++;
		if (*argv == NULL) {
			fprintf(stderr, "No type value specified\n");
			return -1;
		}
		type = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Non-numeric type value %s\n", *argv);
			return -1;
		}
		if (type != 0) {
			fprintf(stderr, "Nonzero type not yet implemented\n");
			return -1;
		}
		/* If/when nonzero allowed, swap/assign it here */
		argv++;
	}

	if (*argv == NULL) {
		/* No arguments beyond type, do a GET and display results */
		err = wlu_iovar_getbuf(wl, cmd->name,
		                       &map, OFFSETOF(wl_mpf_map_t, mask), buf, WLC_IOCTL_MAXLEN);
		if (err >= 0) {
			mapp = (wl_mpf_map_t *)buf;
			if (dtoh16(mapp->version) != WL_MPF_VERSION) {
				fprintf(stderr, "Incorrect version (%d != %d).\n",
				        dtoh16(mapp->version), WL_MPF_VERSION);
				return -1;
			}
			printf("Type %d, mask %04x, %d state mappings:\n",
			       dtoh16(mapp->type), dtoh16(mapp->mask), mapp->count);
			bitcount = bcm_bitcount((uint8*)&mapp->mask, sizeof(mapp->mask));
			if (bitcount > WL_MPF_MAX_BITS) {
				fprintf(stderr, "Invalid mask: more than %d bits\n",
				        WL_MPF_MAX_BITS);
				return -1;
			}

			count = dtoh16(mapp->count);
			if (!count && !bitcount) {
				printf("No map configured\n");
			} else {
				if (count != (1 << bitcount)) {
					fprintf(stderr, "Bit/count mismatch (%d != %d)\n",
					        bitcount, count);
					return -1;
				}
			}

			for (valp = mapp->vals; count; count--, valp++) {
				if (valp->name[WL_MPF_STATE_NAME_MAX-1]) {
					char c = valp->name[WL_MPF_STATE_NAME_MAX-1];
					valp->name[WL_MPF_STATE_NAME_MAX-1] = 0;
					fprintf(stderr, "Invalid name: %s%c\n",
					        valp->name, c);
					return -1;
				}

				printf(" Value: %04x  State: %d  (%s)\n",
				       dtoh16(valp->val), dtoh16(valp->state), valp->name);
			}
		}
	} else {
		/* Additional arguments: parse and do a set */
		mask = strtoul(*argv, &endptr, 16);
		if (*endptr != '\0') {
			fprintf(stderr, "Cannot parse mask %s\n", *argv);
			return -1;
		}
		bitcount = bcm_bitcount((uint8*)&mask, sizeof(mask));
		if (bitcount > WL_MPF_MAX_BITS) {
			fprintf(stderr, "Invalid mask: more than %d bits\n",
			        WL_MPF_MAX_BITS);
			return -1;
		}
		map.mask = htod16(mask);
		argv++;

		count = 1 << bitcount;
		map.count = count;

		for (valp = map.vals; count && *argv; count--, argv++, valp++) {
			val = strtoul(*argv, &endptr, 16);
			if (endptr == *argv || *endptr != '/') {
				fprintf(stderr, "Invalid value, or missing /state: %s\n", *argv);
				return -1;
			}
			if (val & ~mask) {
				fprintf(stderr, "Value bits outside mask: %04x/%04x -> %04x\n",
				        val, mask, val & ~mask);
				return -1;
			}
			val = htod16(val);
			for (valp2 = map.vals; valp2 < valp; valp2++) {
				if (val == valp2->val) {
					fprintf(stderr, "Invalid repeated value: %04x\n",
					        dtoh16(val));
					return -1;
				}
			}
			valp->val = val;

			endptr++;
			if (*endptr == '\0' || *endptr == '/') {
				fprintf(stderr, "Missing required /<state>: %s\n", *argv);
				return -1;
			}
			state = strtoul(endptr, &endptr, 0);
			if (*endptr != '\0' && *endptr != '/') {
				fprintf(stderr, "Non-numeric state value %s\n", endptr);
				return -1;
			}
			if (state >= map.count) {
				fprintf(stderr, "Invalid state %d, must be 0-%d\n",
				        state, map.count - 1);
				return -1;
			}
			valp->state = htod16(state);

			if (*endptr++ == '/') {
				if (strlen(endptr) >= WL_MPF_STATE_NAME_MAX - 1) {
					fprintf(stderr, "Name %s too long, limit %d chars\n",
					        endptr, WL_MPF_STATE_NAME_MAX - 2);
					return -1;
				}
				if (isdigit(*endptr)) {
					fprintf(stderr, "Names cannot start with a digit: %s\n",
					        endptr);
					return -1;
				}
				if (!strcmp(endptr, "gpio")) {
					fprintf(stderr, "Name 'gpio' is reserved\n");
					return -1;
				}
				strcpy(valp->name, endptr);
			}
		}

		/* Closing overall checks */
		if (count) {
			fprintf(stderr, "Specify all %d possible values, missing %d\n",
			        map.count, count);
			return -1;
		} else if (*argv) {
			fprintf(stderr, "Too many arguments for %d possible states at %s\n",
			        map.count, *argv);
			return -1;
		}

		/* All ok, do the set */
		err = wlu_iovar_setbuf(wl, cmd->name,
		                       &map, sizeof(map), buf, WLC_IOCTL_MAXLEN);
	}

	return err;
}

static int
wl_mpf_state(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	wl_mpf_state_t mpstate;
	wl_mpf_state_t *mpstatep;
	uint16 state, type = 0;
	char *endptr;

	argv++;

	memset(&mpstate, 0, sizeof(mpstate));
	mpstate.version = dtoh16(WL_MPF_VERSION);

	if (*argv && !strcmp(*argv, "type")) {
		argv++;
		if (*argv == NULL) {
			fprintf(stderr, "No type value specified\n");
			return -1;
		}
		type = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			fprintf(stderr, "Non-numeric type value %s\n", *argv);
			return -1;
		}
		if (type != 0) {
			fprintf(stderr, "Nonzero type not yet implemented\n");
			return -1;
		}
		/* If/when nonzero allowed, swap/assign it here */
		argv++;
	}

	if (*argv == NULL) {
		/* No arguments beyond type, do a GET and display results */
		err = wlu_iovar_getbuf(wl, cmd->name,
		                       &mpstate, OFFSETOF(wl_mpf_state_t, state),
		                       buf, WLC_IOCTL_MAXLEN);
		if (err >= 0) {
			mpstatep = (wl_mpf_state_t *)buf;
			if (dtoh16(mpstatep->version) != WL_MPF_VERSION) {
				fprintf(stderr, "Incorrect version (%d != %d).\n",
				        dtoh16(mpstatep->version), WL_MPF_VERSION);
				return -1;
			}
			if (mpstatep->name[WL_MPF_STATE_NAME_MAX-1]) {
				fprintf(stderr, "Invalid name\n");
				mpstatep->name[WL_MPF_STATE_NAME_MAX-1] = '\0';
			}
			printf("Type %d: state %d, name %s (%s)\n",
			       dtoh16(mpstatep->type), dtoh16(mpstatep->state),
			       (mpstatep->name ? mpstatep->name : "(unknown)"),
			       ((mpstatep->force) ? "forced" : "auto"));
		}
	} else {
		/* Should be one argument, but need to determine what kind */
		if (isdigit(**argv)) {
			state = strtoul(*argv, &endptr, 0);
			if (*endptr != '\0') {
				fprintf(stderr, "Non-numeric state: %s\n", *argv);
				return -1;
			}
			mpstate.state = htod16(state);
			mpstate.force = 1;
		} else if (!strcmp(*argv, "gpio")) {
			mpstate.force = 0;
		} else {
			if (strlen(*argv) >= WL_MPF_STATE_NAME_MAX-1) {
				fprintf(stderr, "Name %s too long, limit %d chars\n",
				        *argv, WL_MPF_STATE_NAME_MAX - 2);
				return -1;
			}
			strcpy(mpstate.name, *argv);
			mpstate.force = 1;
		}
		argv++;

		if (*argv) {
			fprintf(stderr, "Too many arguments at %s\n", *argv);
			return -1;
		}

		/* All ok, do the set */
		err = wlu_iovar_setbuf(wl, cmd->name,
		                       &mpstate, sizeof(mpstate), buf, WLC_IOCTL_MAXLEN);
	}

	return err;
}
#endif /* WL_MPF */
