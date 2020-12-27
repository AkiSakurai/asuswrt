/*
 * wl rrm command module
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
 * $Id: wluc_rrm.c 778135 2019-08-23 09:40:47Z $
 */

/* WL_BCN_RPT_SUPPORT is a precommit hook and shall be cleanned up after complete
 * checkin of FW and wl app is done
 */
#define WL_BCN_RPT_SUPPORT
#include <wlioctl.h>

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_rrm;
static cmd_func_t wl_rrm_nbr_req;
static cmd_func_t wl_rrm_bcn_req;
static cmd_func_t wl_rrm_chload_req;
static cmd_func_t wl_rrm_noise_req;
static cmd_func_t wl_rrm_frame_req;
static cmd_func_t wl_rrm_stat_req;
static cmd_func_t wl_rrm_stat_rpt;
static cmd_func_t wl_rrm_lm_req;
static cmd_func_t wl_rrm_nbr_list;
static cmd_func_t wl_rrm_nbr_del_nbr;
static cmd_func_t wl_rrm_nbr_add_nbr;
static cmd_func_t wl_rrm_nbr_scan;
static cmd_func_t wl_rrm_txstrm_req;
static cmd_func_t wl_rrm_lci_req;
static cmd_func_t wl_rrm_civic_req;
static cmd_func_t wl_rrm_locid_req;
static cmd_func_t wl_rrm_config;
static cmd_func_t wl_rrm_nbr_ssid;
static cmd_func_t wl_rrm_nbr_lci;
static cmd_func_t wl_rrm_nbr_civic;
static cmd_func_t wl_rrm_nbr;
static cmd_func_t wl_rrm_frng;

typedef struct wl_bcn_report_cfg wl_bcn_report_cfg_t;
static cmd_func_t wl_bcn_report;
static int wl_bcn_report_version_get(void *wl, uint16 category, char **argv);
static int wl_bcn_report_config_get(void *wl, uint16 category, wl_bcn_report_cfg_t *bcn_rpt_cfg);
static int wl_bcn_report_vndr_ie_get(void *wl, uint16 category, vndr_ie_t **vndr_ie);
static int wl_bcn_report_config_set(void *wl, uint16 category, char **argv);
static int wl_bcn_report_vndr_ie_set(void *wl, uint16 category, char **argv);

#define BCN_REPORT_HELP	"Usage: wl -i <iface_name> bcn_report <subcmd> [<subcmd_options>]\n"
#define BCN_REPORT_VER	"ver: Gets IOVAR version\n"
#define BCN_REPORT_CONFIG_HELP	"config:" \
	"[-o <override>] [-m <0-3>] [-n <#of_bcn_report>] [-t <cache_valid_period>]\n" \
	"\t    -o : overrides CCX IE setting\n" \
	"\t\t0 - Caches only CCX IE APs beacon info\n" \
	"\t\t1 - Caches non-CCX IE APs beacon info\n" \
	"\t    -m : Bit value to indicate the modes\n" \
	"\t\t0 - disables beacon report\n" \
	"\t\t1 - Unsolicited mode is supported\n" \
	"\t\t2 - Solicited mode is supported\n" \
	"\t\t3 - Both Unsolicited and solicited mode are supported\n" \
	"\t    -n : Number of beacon info for scan cache table.\n" \
	"\t\t0 - disables beacon report\n" \
	"\t\t8 - Maximum and default scan cache entry\n" \
	"\t    -t : time in milliseconds to hold the scan cache information\n" \
	"\t\t120000 - Default value (120 sec)\n" \
	"\t\t 60000 - Minimum valid value (60 sec)\n" \
	"\t\tNot valid for mode = 1\n"

#define BCN_REPORT_VNDR_IE_HELP	"vndr_ie: <vendor_specific_ie>\n"

static cmd_t wl_rrm_cmds[] = {
	{ "rrm", wl_rrm, WLC_GET_VAR, WLC_SET_VAR,
	"enable or disable RRM feature\n"
	"\tUsage: wl rrm [0/1] to disable/enable RRM feature"},
	{ "rrm_bcn_req", wl_rrm_bcn_req, -1, WLC_SET_VAR,
	"send 11k beacon measurement request\n"
	"\tUsage: wl rrm_bcn_req [bcn mode] [da] [duration] [random int] [channel] [ssid]"
	" [repetitions]"},
	{ "rrm_chload_req", wl_rrm_chload_req, -1, WLC_SET_VAR,
	"send 11k channel load measurement request\n"
	"\tUsage: wl rrm_chload_req [regulatory] [da] [duration] [random int] [channel]"
	" [repetitions]"},
	{ "rrm_noise_req", wl_rrm_noise_req, -1, WLC_SET_VAR,
	"send 11k noise measurement request\n"
	"\tUsage: wl rrm_noise_req [regulatory] [da] [duration] [random int] [channel]"
	" [repetitions] "},
	{ "rrm_frame_req", wl_rrm_frame_req, -1, WLC_SET_VAR,
	"send 11k frame measurement request\n"
	"\tUsage: wl rrm_frame_req [regulatory] [da] [duration] [random int] [channel] [ta]"
	" [repetitions]"},
	{ "rrm_stat_req", wl_rrm_stat_req, -1, WLC_SET_VAR,
	"send 11k stat measurement request\n"
	"\tUsage: wl rrm_stat_req [da] [random int] [duration] [peer] [group id] [repetitions]"},
	{ "rrm_stat_rpt", wl_rrm_stat_rpt, -1, WLC_GET_VAR,
	"Read 11k stat measurement report from STA\n"
	"\tUsage: wl rrm_stat_rpt [mac]"},
	{ "rrm_lm_req", wl_rrm_lm_req, -1, WLC_SET_VAR,
	"send 11k link measurement request\n"
	"\tUsage: wl rrm_lm_req [da]"},
	{ "rrm_nbr_req", wl_rrm_nbr_req, -1, WLC_SET_VAR,
	"send 11k neighbor report measurement request\n"
	"\tUsage: wl rrm_nbr_req [ssid]"},
	{ "rrm_nbr_list", wl_rrm_nbr_list, WLC_GET_VAR, -1,
	"get 11k neighbor report list\n"
	"\tUsage: wl rrm_nbr_list"},
	{ "rrm_nbr_del_nbr", wl_rrm_nbr_del_nbr, -1, WLC_SET_VAR,
	"delete node from 11k neighbor report list\n"
	"\tUsage: wl rrm_nbr_del_nbr [bssid]"},
	{ "rrm_nbr_add_nbr", wl_rrm_nbr_add_nbr, -1, WLC_SET_VAR,
	"add node to 11k neighbor report list\n"
	"\tUsage: wl rrm_nbr_add_nbr <[bssid] [bssid info] [regulatory] [channel] [phytype]>"
	" [ssid] [chanspec] [prefence]"},
	{"rrm_nbr_scan", wl_rrm_nbr_scan, WLC_GET_VAR, WLC_SET_VAR,
	"enable or disable dynamic nbr report\n"
	"\tUsage: wl rrm_nbr_scan [1/0] to enable/disable dynamic nbr report"},
	{ "rrm_txstrm_req", wl_rrm_txstrm_req, -1, WLC_SET_VAR,
	"Send 802.11k Transmit Stream/Category measurement request frame\n"
	"\tUsage: wl rrm_txstrm_req [da] [random int] [duration] "
	"[repetitions] [peer mac] [tid] [bin0_range]"},
	{ "rrm_nbr", wl_rrm_nbr, -1, WLC_SET_VAR,
	"add Civic/LCI/SSID subelement to 11k neighbor report list element\n"
	"\tUsage: wl rrm_nbr civic <bssid> <civic_subelement>\n"
	"\tUsage: wl rrm_nbr lci <bssid> <lci_subelement>\n"
	"\tUsage: wl rrm_nbr chanspec <bssid> <chanspec>\n"
	"\tUsage: wl rrm_nbr ssid <bssid> <ssid>"},
	{ "rrm_frng", wl_rrm_frng, -1, WLC_SET_VAR,
	"send RRM FTM Range request/report frame\n"
	"\tUsage: wl rrm_frng send_req <da> <init delay> <min_ap_count> <num_aps> <max age>"
	" [<bssid> <channel> <regulatory> <phytype> <bssid_info>...]\n"
	"\tUsage: wl rrm_frng send_rep <da> <range entry count> <error entry count>"
	" <measurement start time> <bssid> <range> <max range error> <error code>\n"
	"\tUsage: wl rrm_frng direct <0|1>\n"},
#ifdef WL_PROXDETECT
	{ "rrm_lci_req", wl_rrm_lci_req, -1, WLC_SET_VAR,
	"send 11k LCI measurement request\n"
	"\tUsage: wl rrm_lci_req <da> <subject>"},
	{ "rrm_civic_req", wl_rrm_civic_req, -1, WLC_SET_VAR,
	"send 11k civic location measurement request\n"
	"\tUsage: wl rrm_civic_req <da> <subject> <type> <service intvl> <siu>"},
#else
	{ "rrm_lci_req", wl_rrm_lci_req, -1, WLC_SET_VAR,
	"Send 802.11k Location Configuration Information (LCI) request frame\n"
	"\tUsage: wl rrm_lci_req [da] [repetitions] [locaton sbj] "
	"[latitude resln] [longitude resln] [altitude resln]"},
	{ "rrm_civic_req", wl_rrm_civic_req, -1, WLC_SET_VAR,
	"Send 802.11k Location Civic request frame\n"
	"\tUsage: wl rrm_civic_req [da] [repetitions] [locaton sbj] "
	"[location type] [siu] [si]"},
#endif /* WL_PROXDETECT */
	{ "rrm_locid_req", wl_rrm_locid_req, -1, WLC_SET_VAR,
	"Send 802.11k Location Identifier request frame\n"
	"\tUsage: wl rrm_locid_req [da] [repetitions] [locaton sbj] "
	"[siu] [si]"},
	{ "rrm_config", wl_rrm_config, -1, WLC_SET_VAR,
	"Configure information (LCI/Civic location) for self\n"
	"\tUsage: wl rrm_config lci [lci_location]\n"
	"\tUsage: wl rrm_config civic [civic_location]\n"
	"\tUsage: wl rrm_config locid [location_identifier]"},
	{ "bcn_report", wl_bcn_report, WLC_GET_VAR, WLC_SET_VAR,
	BCN_REPORT_HELP
	"\t"BCN_REPORT_VER
	"\t"BCN_REPORT_CONFIG_HELP
	"\t"BCN_REPORT_VNDR_IE_HELP
	},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_rrm_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register rrm commands */
	wl_module_cmds_register(wl_rrm_cmds);
}

/* RM Enable Capabilities */
static dbg_msg_t rrm_msgs[] = {
	{DOT11_RRM_CAP_LINK,	"Link_Measurement"},				/* bit0 */
	{DOT11_RRM_CAP_NEIGHBOR_REPORT,	"Neighbor_Report"},			/* bit1 */
	{DOT11_RRM_CAP_PARALLEL,	"Parallel_Measurement"},		/* bit2 */
	{DOT11_RRM_CAP_REPEATED,	"Repeated_Measurement"},		/* bit3 */
	{DOT11_RRM_CAP_BCN_PASSIVE,	"Beacon_Passive"},			/* bit4 */
	{DOT11_RRM_CAP_BCN_ACTIVE,	"Beacon_Active"},			/* bit5 */
	{DOT11_RRM_CAP_BCN_TABLE,	"Beacon_Table"},			/* bit6 */
	{DOT11_RRM_CAP_BCN_REP_COND,	"Beacon_measurement_Reporting_Condition"}, /* bit7 */
	{DOT11_RRM_CAP_FM,	"Frame_Measurement"},				/* bit8 */
	{DOT11_RRM_CAP_CLM,	"Channel_load_Measurement"},			/* bit9 */
	{DOT11_RRM_CAP_NHM,	"Noise_Histogram_measurement"},			/* bit10 */
	{DOT11_RRM_CAP_SM,	"Statistics_Measurement"},			/* bit11 */
	{DOT11_RRM_CAP_LCIM,	"LCI_Measurement"},				/* bit12 */
	{DOT11_RRM_CAP_LCIA,	"LCI_Azimuth"},					/* bit13 */
	{DOT11_RRM_CAP_TSCM,	"Tx_Stream_Category_Measurement"},		/* bit14 */
	{DOT11_RRM_CAP_TTSCM,	"Triggered_Tx_stream_Category_Measurement"},	/* bit15 */
	{DOT11_RRM_CAP_AP_CHANREP,	"AP_Channel_Report"},			/* bit16 */
	{DOT11_RRM_CAP_RMMIB,	"RM_MIB"},					/* bit17 */
	/* bit 18-23, unused */
	{DOT11_RRM_CAP_MPC0,	"Measurement_Pilot_Capability_Bit0"},		/* bit24 */
	{DOT11_RRM_CAP_MPC1,	"Measurement_Pilot_Capability_Bit1"},		/* bit25 */
	{DOT11_RRM_CAP_MPC2,	"Measurement_Pilot_Capability_Bit2"},		/* bit26 */
	{DOT11_RRM_CAP_MPTI,	"Measurement_Pilot_Transmission_Information"},	/* bit27 */
	{DOT11_RRM_CAP_NBRTSFO,	"Neighbor_Report_TSF_Offset"},			/* bit28 */
	{DOT11_RRM_CAP_RCPI,	"RCPI_Measurement"},				/* bit29 */
	{DOT11_RRM_CAP_RSNI,	"RSNI_Measurement"},				/* bit30 */
	{DOT11_RRM_CAP_BSSAAD,	"BSS_Average_Access_Delay"},			/* bit31 */
	{DOT11_RRM_CAP_BSSAAC,	"BSS_Available_Admission_Capacity"},		/* bit32 */
	{DOT11_RRM_CAP_AI,	"Antenna_Information"},				/* bit33 */
	{DOT11_RRM_CAP_FTM_RANGE,	"FTM_Range_Reporting"},			/* bit34 */
	{DOT11_RRM_CAP_CIVIC_LOC,	"Civic_Location_Measurement"},		/* bit35 */
	{DOT11_RRM_CAP_IDENT_LOC,	"Identifier_Location_Measurement"},	/* bit36 */
	{0,		NULL}
};

static bool rrm_input_validation(uint val, uint hval, dbg_msg_t *dbg_msg)
{
	int i;
	uint32 flag = 0;

	if ((val == 0) && (hval == 0))
		return TRUE;

	for (i = 0; dbg_msg[i].value <= DOT11_RRM_CAP_BSSAAD; i++)
		flag |= 1 << dbg_msg[i].value;
	flag = ~flag;
	if (val & flag)
		return FALSE;

	flag = 0;
	if (hval != 0) {
		for (; dbg_msg[i].value; i++) {
			flag |= 1 << (dbg_msg[i].value - DOT11_RRM_CAP_BSSAAC);
		}
		flag = ~flag;
		if (hval & flag)
			return FALSE;
	}

	return TRUE;
}

static int
wl_rrm(void *wl, cmd_t *cmd, char **argv)
{
	int err, i;
	uint hval = 0, val = 0, len, found, rmcap_del = 0, rmcap2_del = 0;
	uint rmcap_add = 0, rmcap2_add = 0;
	char *endptr = NULL;
	dbg_msg_t *dbg_msg = rrm_msgs;
	void *ptr = NULL;
	dot11_rrm_cap_ie_t rrm_cap, *reply;
	uint high = 0, low = 0, bit = 0, hbit = 0;
	char *s = NULL;
	char t[32], c[32] = "0x";

	err = wlu_var_getbuf_sm(wl, cmd->name, &rrm_cap, sizeof(rrm_cap), &ptr);
	if (err < 0)
		return err;
	reply = (dot11_rrm_cap_ie_t *)ptr;

	high = reply->cap[4];
	low = reply->cap[0] | (reply->cap[1] << 8) | (reply->cap[2] << 16) | (reply->cap[3] << 24);
	if (!*++argv) {
		if (high != 0)
			printf("0x%x%08x", high, low);
		else
			printf("0x%x ", low);
		for (i = 0; ((bit = dbg_msg[i].value) <= DOT11_RRM_CAP_BSSAAD); i++) {
			if (low & (1 << bit))
				printf(" %s", dbg_msg[i].string);
		}
		for (; (hbit = dbg_msg[i].value); i++) {
			if (high & (1 << (hbit - DOT11_RRM_CAP_BSSAAC)))
				printf(" %s", dbg_msg[i].string);
		}
		printf("\n");

		return err;
	}
	while (*argv) {
		s = *argv;

		found = 0;
		if (*s == '+' || *s == '-')
			s++;
		else {
			/* used for clearing previous value */
			rmcap_del = ~0;
			rmcap2_del = ~0;
		}
		val = strtoul(s, &endptr, 0);
		/* Input is decimal number or hex with prefix 0x and > 32 bits */
		if (val == 0xFFFFFFFF) {
			if (!(*s == '0' && *(s+1) == 'x')) {
				fprintf(stderr,
				"Msg bits >32 take only numerical input in hex\n");
				val = 0;
			} else {
				/* Input number with prefix 0x */
				len = strlen(s);
				hval = strtoul(strncpy(t, s, len-8), &endptr, 0);
				*endptr = 0;
				s = s + strlen(t);
				s = strcat(c, s);
				val = strtoul(s, &endptr, 0);
				/* Input number > 64bit */
				if (hval == 0xFFFFFFFF) {
					fprintf(stderr, "Invalid entry for RM Capabilities\n");
					hval = 0;
					val = 0;
				}
			}
		}
		/* validet the input number */
		if (!rrm_input_validation(val, hval, dbg_msg))
			goto usage;
		/* Input is a string */
		if (*endptr != '\0') {
			for (i = 0; ((bit = dbg_msg[i].value) <= DOT11_RRM_CAP_BSSAAD); i++) {
				if (stricmp(dbg_msg[i].string, s) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				for (; (hbit = dbg_msg[i].value); i++) {
					if (stricmp(dbg_msg[i].string, s) == 0)
						break;
				}
				if (hbit)
					hval = 1 << (hbit - DOT11_RRM_CAP_BSSAAC);
				else
					hval = 0;
			} else {
				val = 1 << bit;
			}
			if (!val && !hval)
			      goto usage;
		}
		if (**argv == '-') {
			rmcap_del |= val;
			if (!found)
				rmcap2_del |= hval;
		}
		else {
			rmcap_add |= val;
			if (!found)
				rmcap2_add |= hval;
		}
		++argv;
	}

	low &= ~rmcap_del;
	high &= ~rmcap2_del;
	low |= rmcap_add;
	high |= rmcap2_add;

	rrm_cap.cap[4] = high;
	rrm_cap.cap[0] = low & 0x000000ff;
	rrm_cap.cap[1] = (low & 0x0000ff00) >> 8;
	rrm_cap.cap[2] = (low & 0x00ff0000) >> 16;
	rrm_cap.cap[3] = (low & 0xff000000) >> 24;

	err = wlu_var_setbuf(wl, cmd->name, &rrm_cap, sizeof(dot11_rrm_cap_ie_t));
	return err;

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");
	for (i = 0; (bit = dbg_msg[i].value) <= DOT11_RRM_CAP_BSSAAD; i++) {
		fprintf(stderr, "\n0x%04x %s", (1 << bit), dbg_msg[i].string);
	}
	for (; (hbit = dbg_msg[i].value); i++) {
		hbit -= DOT11_RRM_CAP_BSSAAC;
		fprintf(stderr, "\n0x%x00000000 %s", (1 << hbit), dbg_msg[i].string);
	}
	fprintf(stderr, "\n");
	return BCME_OK;
}

static int
wl_rrm_stat_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	statreq_t sreq_buf;

	memset(&sreq_buf, 0, sizeof(statreq_t));

	if (argv[1]) {
		/* da */
		if (!wl_ether_atoe(argv[1], &sreq_buf.da)) {
			printf("wl_rrm_stat_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* random interval */
	if (argv[2]) {
		sreq_buf.random_int = htod32(strtoul(argv[2], NULL, 0));
	}
	/* duration */
	if (argv[3]) {
		sreq_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}
	/* peer address */
	if (argv[4]) {
		if (!wl_ether_atoe(argv[4], &sreq_buf.peer)) {
			printf("wl_rrm_stat_req parsing peer failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* group id */
	if (argv[5]) {
		sreq_buf.group_id =
			htod32(strtoul(argv[5], NULL, 0));
	}
	/* repetitions */
	if (argv[6]) {
		sreq_buf.reps = htod32(strtoul(argv[6], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmd->name, &sreq_buf, sizeof(sreq_buf));
	return err;
}

static int
wl_rrm_stat_rpt(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_USAGE_ERROR;
	statrpt_t *rpt_ptr, rpt;
	int cnt;

	if (!*++argv) return -1;

	/* get sta mac address */
	if (!wl_ether_atoe(*argv++, &rpt.addr))
		goto done;

	rpt.ver = htod16(WL_RRM_RPT_VER);
	memset(buf, 0, WLC_IOCTL_MEDLEN);
	if ((ret = wlu_iovar_getbuf(wl, cmd->name, (void *) &rpt,
		sizeof(rpt), buf, WLC_IOCTL_MEDLEN)) < 0) {
		fprintf(stderr, "ERROR: cmd:%s\n", cmd->name);
		goto done;
	}

	/* display the sta info */
	rpt_ptr = (statrpt_t *)buf;
	rpt_ptr->ver = dtoh16(rpt_ptr->ver);

	/* Report unrecognized version */
	if (rpt_ptr->ver != WL_RRM_RPT_VER) {
		fprintf(stderr, "ERROR: Mismatch ver[%d] Driver ver[%d]\n",
			WL_RRM_RPT_VER, rpt_ptr->ver);
		goto done;
	}

	printf("ver:%d timestamp:%u flag:%d len:%d\n",
		rpt_ptr->ver, dtoh32(rpt_ptr->timestamp),
		dtoh16(rpt_ptr->flag), rpt_ptr->len);
	for (cnt = 0; cnt < rpt_ptr->len; cnt++) {
		if (cnt % 8 == 0)
			printf("\n[%d]:", cnt);
		printf("[0x%02x][%d] ", rpt_ptr->data[cnt], (signed char)(rpt_ptr->data[cnt]));
	}
	printf("\n");

done:
	return ret;
}

static int
wl_rrm_frame_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	framereq_t freq_buf;

	memset(&freq_buf, 0, sizeof(framereq_t));
	if (argv[1]) {
		/* Regulatory class */
		freq_buf.reg = htod32(strtoul(argv[1], NULL, 0));
	}

	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &freq_buf.da)) {
			printf("wl_rrm_frame_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* duration */
	if (argv[3]) {
		freq_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}
	/* random interval */
	if (argv[4]) {
		freq_buf.random_int = htod32(strtoul(argv[4], NULL, 0));
	}
	/* channel */
	if (argv[5]) {
		freq_buf.chan = htod32(strtoul(argv[5], NULL, 0));
	}
	/* transmit address */
	if (argv[6]) {
		if (!wl_ether_atoe(argv[6], &freq_buf.ta)) {
			printf("wl_rrm_frame_req parsing ta failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* repetitions */
	if (argv[7]) {
		freq_buf.reps = htod32(strtoul(argv[7], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmd->name, &freq_buf, sizeof(freq_buf));
	return err;
}

static int
wl_rrm_chload_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	rrmreq_t chreq_buf;

	memset(&chreq_buf, 0, sizeof(rrmreq_t));

	if (argv[1]) {
		/* Regulatory class */
		chreq_buf.reg = htod32(strtoul(argv[1], NULL, 0));
	}
	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &chreq_buf.da)) {
			printf("wl_rrm_chload_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* duration */
	if (argv[3]) {
		chreq_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}
	/* random interval */
	if (argv[4]) {
		chreq_buf.random_int = htod32(strtoul(argv[4], NULL, 0));
	}
	/* channel */
	if (argv[5]) {
		chreq_buf.chan = htod32(strtoul(argv[5], NULL, 0));
	}
	/* repetitions */
	if (argv[6]) {
		chreq_buf.reps = htod32(strtoul(argv[6], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmd->name, &chreq_buf, sizeof(chreq_buf));
	return err;
}

static int
wl_rrm_noise_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	rrmreq_t nreq_buf;

	printf("wl_rrm_noise_req\n");

	memset(&nreq_buf, 0, sizeof(rrmreq_t));
	if (argv[1]) {
		/* Regulatory class */
		nreq_buf.reg = htod32(strtoul(argv[1], NULL, 0));
	}
	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &nreq_buf.da)) {
			printf("wl_rrm_noise_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* duration */
	if (argv[3]) {
		nreq_buf.dur = htod32(strtoul(argv[3], NULL, 0));

	}
	/* random interval */
	if (argv[4]) {
		nreq_buf.random_int = htod32(strtoul(argv[4], NULL, 0));
	}
	/* channel */
	if (argv[5]) {
		nreq_buf.chan = htod32(strtoul(argv[5], NULL, 0));
	}
	/* repetitions */
	if (argv[6]) {
		nreq_buf.reps = htod32(strtoul(argv[6], NULL, 0));
	}
	err = wlu_iovar_set(wl, cmd->name, &nreq_buf, sizeof(nreq_buf));
	return err;
}

static int
wl_rrm_bcn_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	uint8 mode = 0;
	bcn_req_t *bcnreq_buf;
	wlc_ssid_t ssid;
	int i, num = 0, malloc_len;

	/* number of chanspecs */
	if (argv[9]) {
		num = htod32(strtoul(argv[9], NULL, 0));
	}
	malloc_len = sizeof(bcn_req_t) + sizeof(chanspec_t) * num;

	bcnreq_buf = (bcn_req_t *) calloc(1, malloc_len);
	if (bcnreq_buf == NULL) {
		printf("Failed to allocate buffer of %d bytes\n", malloc_len);
		return BCME_NOMEM;
	}

	bcnreq_buf->version = WL_RRM_BCN_REQ_VER;
	if (argv[1]) {
		/* bcn mode: ACTIVE/PASSIVE/SCAN_CACHE */
		mode = htod32(strtoul(argv[1], NULL, 0));
		if (mode > 2) {
			printf("wl_rrm_bcn_req parsing bcn mode failed\n");
			err = BCME_BADARG;
			goto done;
		}
		bcnreq_buf->bcn_mode = mode;
	}
	/* da */
	if (argv[2]) {
		if (!wl_ether_atoe(argv[2], &bcnreq_buf->da)) {
			printf("wl_rrm_bcn_req parsing da failed\n");
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}
	/* duration */
	if (argv[3]) {
		bcnreq_buf->dur = htod32(strtoul(argv[3], NULL, 0));
	}

	/* random interval */
	if (argv[4]) {
		bcnreq_buf->random_int = htod32(strtoul(argv[4], NULL, 0));
	}

	/* channel */
	if (argv[5]) {
		bcnreq_buf->channel = htod32(strtoul(argv[5], NULL, 0));
	}
	printf("wl_rrm_bcn_req:bcn mode: %d, duration: %d, "
		"chan: %d\n", mode, bcnreq_buf->dur, bcnreq_buf->channel);

	/* SSID */
	if (argv[6]) {
		uint32 len;

		len = strlen(argv[6]);
		if (len > DOT11_MAX_SSID_LEN) {
			printf("ssid too long\n");
			err = BCME_BADARG;
			goto done;
		}
		memset(&ssid, 0, sizeof(wlc_ssid_t));
		memcpy(ssid.SSID, argv[6], len);
		ssid.SSID_len = len;
		memcpy(&bcnreq_buf->ssid, &ssid, sizeof(wlc_ssid_t));
	}

	/* repetitions */
	if (argv[7]) {
		bcnreq_buf->reps = htod32(strtoul(argv[7], NULL, 0));
	}

	/* request elements */
	if (argv[8]) {
		bcnreq_buf->req_elements = htod32(strtoul(argv[8], NULL, 0));
	}

	printf("wl_rrm_bcn_req: ssid: %s, rep: %d, elements: %d "
		"num chspecs: %d\n", bcnreq_buf->ssid.SSID, bcnreq_buf->reps,
		bcnreq_buf->req_elements, num);
	bcnreq_buf->chspec_list.num = num;

	/* List of chanspecs */
	for (i = 0; i < num; i++) {
		printf("chanspec list:\n");
		if (!argv[10 + i]) {
			printf("Incomplete channel list. provided: %d required: %d\n",
				i, num);
			err = BCME_BADARG;
			goto done;
		}
		bcnreq_buf->chspec_list.list[i] = htod32(strtoul(argv[10 + i], NULL, 0));
		printf("0x%x ", bcnreq_buf->chspec_list.list[i]);
	}
	printf("\n");
	err = wlu_iovar_set(wl, cmd->name, bcnreq_buf, malloc_len);
done:
	free(bcnreq_buf);
	return err;
}

static int
wl_rrm_lm_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;

	struct ether_addr da;

	if (argv[1]) {
		if (!wl_ether_atoe(argv[1], &da)) {
			printf("wl_rrm_lm_req parsing arg1 failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	err = wlu_iovar_set(wl, cmd->name, &da, sizeof(da));
	return err;
}

static int
wl_rrm_nbr_req(void *wl, cmd_t *cmd, char **argv)
{
	int err, buflen;
	wlc_ssid_t ssid;

	strcpy(buf, cmd->name);
	buflen = strlen(cmd->name) + 1;

	if (*++argv) {
		uint32 len;

		len = strlen(*argv);
		if (len > DOT11_MAX_SSID_LEN) {
			printf("ssid too long\n");
			return BCME_BADARG;
		}
		memset(&ssid, 0, sizeof(wlc_ssid_t));
		memcpy(ssid.SSID, *argv, len);
		ssid.SSID_len = len;
		memcpy(&buf[buflen], &ssid, sizeof(wlc_ssid_t));
		buflen += sizeof(wlc_ssid_t);
	}

	err = wlu_set(wl, WLC_SET_VAR, buf, buflen);

	return err;
}

/* For mapping customer's user space command, two calls of the same iovar. */
static int
wl_rrm_nbr_list_legacy(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0, buflen, i;
	uint16 list_cnt;
	nbr_element_t *nbr_elt;
	uint8 *ptr;

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, cmd->name);
	buflen = strlen(cmd->name) + 1;

	if (*++argv != NULL) {
		return BCME_USAGE_ERROR;
	}

	if ((err = wlu_get(wl, WLC_GET_VAR, buf, buflen)) < 0) {
		return err;
	}

	list_cnt = *(uint16 *)buf;

	if (list_cnt == 0) {
		return err;
	}

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, cmd->name);
	buflen = strlen(cmd->name) + 1;

	memcpy(&buf[buflen], &list_cnt, sizeof(uint16));

	printf("RRM Neighbor Report List:\n");

	if ((err = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MAXLEN)) < 0) {
		return err;
	}

	ptr = (uint8 *)buf;

	for (i = 0; i < list_cnt; i++) {
		nbr_elt = (nbr_element_t *)ptr;
		printf("AP %2d: ", i + 1);
		printf("bssid %s ", wl_ether_etoa(&nbr_elt->bssid));
		printf("bssid_info %08x ", load32_ua(&nbr_elt->bssid_info));
		printf("reg %2d channel %3d phytype %d\n", nbr_elt->reg,
			nbr_elt->channel, nbr_elt->phytype);

		ptr += TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN;
	}

	return err;
}

static int
wl_rrm_nbr_list_v1(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0, i;
	uint16 list_cnt;
	nbr_list_t *nbr_list;
	nbr_rpt_elem_t *nbr_elt;

	if (*++argv != NULL) {
		return BCME_USAGE_ERROR;
	}

	memset(buf, 0, WLC_IOCTL_MAXLEN);
	strcpy(buf, cmd->name);

	if ((err = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MAXLEN)) < 0) {
		return err;
	}

	nbr_list = (nbr_list_t *)buf;

	list_cnt = nbr_list->count;

	if (list_cnt == 0) {
		return err;
	}

	printf("RRM Neighbor Report List:\n");

	nbr_elt = (nbr_rpt_elem_t *)((uint8 *)nbr_list + nbr_list->fixed_length);
	for (i = 0; i < list_cnt; i++) {
		printf("AP %2d: ", i + 1);
		printf("bssid %s ", wl_ether_etoa(&nbr_elt->bssid));
		printf("bssid_info 0x%08x ", load32_ua(&nbr_elt->bssid_info));
		printf("reg 0x%02x channel 0x%02x phytype 0x%02x ", nbr_elt->reg,
			nbr_elt->channel, nbr_elt->phytype);
		printf("ssid %s chanspec 0x%04x preference 0x%02x\n", nbr_elt->ssid.SSID,
			nbr_elt->chanspec, nbr_elt->bss_trans_preference);

		nbr_elt++;
	}
	return err;
}

static int
wl_rrm_nbr_list(void *wl, cmd_t *cmd, char **argv)
{
	/* for wlc_ver_major < 9 legacy neighbor report element */
	if (wlc_ver_major(wl) < KUDU_WLC_VER_MAJOR) {
		return wl_rrm_nbr_list_legacy(wl, cmd, argv);
	} else {
		return wl_rrm_nbr_list_v1(wl, cmd, argv);
	}
}

static int
wl_rrm_nbr_del_nbr(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	struct ether_addr ea;

	if (*++argv == NULL) {
		printf("no bssid specified\n");
		return BCME_USAGE_ERROR;
	} else {
		if (!wl_ether_atoe(*argv, &ea)) {
			printf("Incorrect bssid format\n");
			return BCME_ERROR;
		}
		err = wlu_iovar_set(wl, cmd->name, &ea, sizeof(ea));
	}

	return err;
}

static int
wl_rrm_nbr_add_nbr_legacy(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	nbr_element_t nbr_elt;

	memset(&nbr_elt, 0, sizeof(nbr_element_t));

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc < 6) {
		return BCME_USAGE_ERROR;
	}

	/* bssid */
	if (!wl_ether_atoe(argv[1], &nbr_elt.bssid)) {
		printf("wl_rrm_nbr_add_nbr parsing bssid failed\n");
		return BCME_USAGE_ERROR;
	}

	/* bssid info */
	nbr_elt.bssid_info = htod32(strtoul(argv[2], NULL, 0));

	/* Regulatory class */
	nbr_elt.reg = htod32(strtoul(argv[3], NULL, 0));

	/* channel */
	nbr_elt.channel = htod32(strtoul(argv[4], NULL, 0));

	/* phytype */
	nbr_elt.phytype = htod32(strtoul(argv[5], NULL, 0));

	nbr_elt.id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbr_elt.len = DOT11_NEIGHBOR_REP_IE_FIXED_LEN;

	err = wlu_iovar_set(wl, cmd->name, &nbr_elt, TLV_HDR_LEN + DOT11_NEIGHBOR_REP_IE_FIXED_LEN);
	return err;
}

static int
wl_rrm_nbr_add_nbr_v1(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	nbr_rpt_elem_t nbr_elt;

	memset(&nbr_elt, 0, sizeof(nbr_rpt_elem_t));

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc < 6) {
		return BCME_USAGE_ERROR;
	}

	/* bssid */
	if (!wl_ether_atoe(argv[1], &nbr_elt.bssid)) {
		printf("wl_rrm_nbr_add_nbr parsing bssid failed\n");
		return BCME_USAGE_ERROR;
	}

	/* bssid info */
	nbr_elt.bssid_info = htod32(strtoul(argv[2], NULL, 0));

	/* Regulatory class */
	nbr_elt.reg = htod32(strtoul(argv[3], NULL, 0));

	/* channel */
	nbr_elt.channel = htod32(strtoul(argv[4], NULL, 0));

	/* phytype */
	nbr_elt.phytype = htod32(strtoul(argv[5], NULL, 0));

	/* ssid */
	if (argc >= 7 && argv[6]) {
		nbr_elt.ssid.SSID_len = strlen(argv[6]);
		memcpy(&(nbr_elt.ssid.SSID), argv[6], nbr_elt.ssid.SSID_len);
	}

	/* chanspec */
	if (argc >= 8 && argv[7]) {
		nbr_elt.chanspec = htod32(strtoul(argv[7], NULL, 0));
	}

	/* bss_trans_preference */
	if (argc >= 9 && argv[8]) {
		nbr_elt.bss_trans_preference = htod32(strtoul(argv[8], NULL, 0));
	}

	nbr_elt.version = WL_RRM_NBR_RPT_VER;
	nbr_elt.id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbr_elt.len = sizeof(nbr_rpt_elem_t);

	err = wlu_iovar_set(wl, cmd->name, &nbr_elt, nbr_elt.len);
	return err;
}

static int
wl_rrm_nbr_add_nbr(void *wl, cmd_t *cmd, char **argv)
{
	/* wlc_ver_major < 9 legacy neighbor report element */
	if (wlc_ver_major(wl) < KUDU_WLC_VER_MAJOR) {
		return wl_rrm_nbr_add_nbr_legacy(wl, cmd, argv);
	} else {
		return wl_rrm_nbr_add_nbr_v1(wl, cmd, argv);
	}
}

static int
wl_rrm_nbr_scan(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	uint8 val = 0;

	if (!*++argv) {
		err = wlu_iovar_get(wl, cmd->name, (void *)&val, sizeof(val));
		printf("%d\n", val);
	} else {
		val = htod32(strtoul(argv[0], NULL, 0));
		err = wlu_iovar_setint(wl, cmd->name, val);
	}
	return err;
}

static int
wl_rrm_txstrm_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	txstrmreq_t tscm_buf;

	memset(&tscm_buf, 0, sizeof(tscm_buf));

	/* da */
	if (argv[1]) {
		if (!wl_ether_atoe(argv[1], &tscm_buf.da)) {
			printf("wl_rrm_txstrm_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* random interval */
	if (argv[2]) {
		tscm_buf.random_int = htod32(strtoul(argv[2], NULL, 0));
	}
	/* duration */
	if (argv[3]) {
		tscm_buf.dur = htod32(strtoul(argv[3], NULL, 0));
	}
	/* repetitions */
	if (argv[4]) {
		tscm_buf.reps = htod32(strtoul(argv[4], NULL, 0));
	}
	/* peer */
	if (argv[5]) {
		if (!wl_ether_atoe(argv[5], &tscm_buf.peer)) {
			printf("wl_rrm_txstrm_req parsing peer failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* tid */
	if (argv[6]) {
		tscm_buf.tid = htod32(strtoul(argv[6], NULL, 0));
	}
	/* bin0 range */
	if (argv[7]) {
		tscm_buf.bin0_range = htod32(strtoul(argv[7], NULL, 0));
	}

	err = wlu_iovar_set(wl, cmd->name, &tscm_buf, sizeof(tscm_buf));
	return err;
}

#ifdef WL_PROXDETECT
static int
wl_rrm_lci_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	int argc;
	const char *cmdname = "rrm_lci_req";
	lci_req_t lcireq_buf;

	UNUSED_PARAMETER(cmd);
	memset(&lcireq_buf, 0, sizeof(lcireq_buf));

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc < 3) {
		return BCME_USAGE_ERROR;
	}

	if (argv[1]) {
		/* da */
		if (!wl_ether_atoe(argv[1], &lcireq_buf.da)) {
			printf("wl_rrm_lci_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}

	/* subject */
	if (argv[2]) {
		lcireq_buf.subject = (uint8)strtoul(argv[2], NULL, 0);
	}

	/* Max Age, 0 = reserved, 0xffff = any time is acceptable */
	if (argv[3]) {
		lcireq_buf.max_age = htod16((uint16) strtoul(argv[3], NULL, 0));
	}

	lcireq_buf.version = WL_RRM_LCI_REQ_VER;
	err = wlu_iovar_set(wl, cmdname, &lcireq_buf, sizeof(lcireq_buf));
	return err;
}

static int
wl_rrm_civic_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	int argc;
	const char *cmdname = "rrm_civic_req";
	civic_req_t civicreq_buf;

	UNUSED_PARAMETER(cmd);
	memset(&civicreq_buf, 0, sizeof(civicreq_buf));

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 6) {
		return BCME_USAGE_ERROR;
	}

	if (argv[1]) {
		/* da */
		if (!wl_ether_atoe(argv[1], &civicreq_buf.da)) {
			printf("wl_rrm_civic_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* subject */
	if (argv[2]) {
		civicreq_buf.subject = (uint8)strtoul(argv[2], NULL, 0);
	}
	/* type */
	if (argv[3]) {
		civicreq_buf.type = (uint8)strtoul(argv[3], NULL, 0);
	}
	/* service_interval */
	if (argv[4]) {
		civicreq_buf.service_interval = htod16(strtoul(argv[4], NULL, 0));
	}
	/* si_units */
	if (argv[5]) {
		civicreq_buf.si_units = (uint8)strtoul(argv[5], NULL, 0);
	}

	civicreq_buf.version = WL_RRM_CIVIC_REQ_VER;
	err = wlu_iovar_set(wl, cmdname, &civicreq_buf, sizeof(civicreq_buf));
	return err;
}

#else
static int
wl_rrm_lci_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	lcireq_t lci_buf;

	memset(&lci_buf, 0, sizeof(lci_buf));

	/* da */
	if (argv[1]) {
		if (!wl_ether_atoe(argv[1], &lci_buf.da)) {
			printf("wl_rrm_lci_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* repetitions */
	if (argv[2]) {
		lci_buf.reps = htod32(strtoul(argv[2], NULL, 0));
	}
	/* Location Subject */
	if (argv[3]) {
		lci_buf.subj = htod32(strtoul(argv[3], NULL, 0));
	}
	/* Latitude Resolution */
	if (argv[4]) {
		lci_buf.lat_res = htod32(strtoul(argv[4], NULL, 0));
	}
	/* Longitude Resolution */
	if (argv[5]) {
		lci_buf.lon_res = htod32(strtoul(argv[5], NULL, 0));
	}
	/* Altitude Resolution */
	if (argv[6]) {
		lci_buf.alt_res = htod32(strtoul(argv[6], NULL, 0));
	}

	err = wlu_iovar_set(wl, cmd->name, &lci_buf, sizeof(lci_buf));
	return err;
}

static int
wl_rrm_civic_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	civicreq_t civic_buf;

	memset(&civic_buf, 0, sizeof(civic_buf));

	/* da */
	if (argv[1]) {
		if (!wl_ether_atoe(argv[1], &civic_buf.da)) {
			printf("wl_rrm_civic_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* repetitions */
	if (argv[2]) {
		civic_buf.reps = htod32(strtoul(argv[2], NULL, 0));
	}
	/* Location Subject */
	if (argv[3]) {
		civic_buf.subj = htod32(strtoul(argv[3], NULL, 0));
	}
	/* Location Type */
	if (argv[4]) {
		civic_buf.civloc_type = htod32(strtoul(argv[4], NULL, 0));
	}
	/* Location service interval unit */
	if (argv[5]) {
		civic_buf.siu = htod32(strtoul(argv[5], NULL, 0));
	}
	/* Location service interval */
	if (argv[6]) {
		civic_buf.si = htod32(strtoul(argv[6], NULL, 0));
	}

	err = wlu_iovar_set(wl, cmd->name, &civic_buf, sizeof(civic_buf));
	return err;
}
#endif /* WL_PROXDETECT */

static int
wl_rrm_locid_req(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	locidreq_t locid_buf;

	memset(&locid_buf, 0, sizeof(locid_buf));

	/* da */
	if (argv[1]) {
		if (!wl_ether_atoe(argv[1], &locid_buf.da)) {
			printf("wl_rrm_locid_req parsing da failed\n");
			return BCME_USAGE_ERROR;
		}
	}
	/* repetitions */
	if (argv[2]) {
		locid_buf.reps = htod32(strtoul(argv[2], NULL, 0));
	}
	/* Location Subject */
	if (argv[3]) {
		locid_buf.subj = htod32(strtoul(argv[3], NULL, 0));
	}
	/* Location service interval unit */
	if (argv[4]) {
		locid_buf.siu = htod32(strtoul(argv[4], NULL, 0));
	}
	/* Location service interval */
	if (argv[5]) {
		locid_buf.si = htod32(strtoul(argv[5], NULL, 0));
	}

	err = wlu_iovar_set(wl, cmd->name, &locid_buf, sizeof(locid_buf));
	return err;
}

static int
wl_rrm_parse_location(char *loc_arg, char *bufptr, int buflen)
{
	int len = 0;
	char *ptr = loc_arg;
	char hex[] = "XX";

	if (!loc_arg) {
		printf("%s: Location data is missing\n", __FUNCTION__);
		len = -1;
		goto done;
	}

	len = strlen(loc_arg)/2;
	if ((len <= 0) || (len > buflen)) {
		len = -1;
		goto done;
	}

	if ((uint16)strlen(ptr) != len*2) {
		printf("%s: Invalid length. Even number of characters expected.\n",
			__FUNCTION__);
		len = -1;
		goto done;
	}

	while (*ptr) {
		strncpy(hex, ptr, 2);
		*bufptr++ = (char) strtoul(hex, NULL, 16);
		ptr += 2;
	}
done:
	return len;
}

static int
wl_rrm_self_lci_civic(void *wl, cmd_t *cmd, char **argv, int argc, int cmd_id)
{
	int err = BCME_OK;
	int len = 0;
	char *bufptr;
	wl_rrm_config_ioc_t *rrm_config_cmd = NULL;
	wl_rrm_config_ioc_t rrm_config_param;
	int malloc_len = sizeof(*rrm_config_cmd) + TLV_BODY_LEN_MAX -
		DOT11_MNG_IE_MREP_FIXED_LEN;

	rrm_config_cmd = (wl_rrm_config_ioc_t *) calloc(1, malloc_len);
	if (rrm_config_cmd == NULL) {
		printf("Failed to allocate buffer of %d bytes\n", malloc_len);
		err = BCME_NOMEM;
		goto done;
	}

	rrm_config_cmd->id = cmd_id;

	if (argc == 3) {
		bufptr = (char *)&rrm_config_cmd->data[0];
		len = wl_rrm_parse_location(argv[1], bufptr, TLV_BODY_LEN_MAX -
			DOT11_MNG_IE_MREP_FIXED_LEN);
		if (len <= 0) {
			printf("%s: parsing location arguments failed\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}

		rrm_config_cmd->len = len;
		err = wlu_iovar_set(wl, cmd->name, (void *)rrm_config_cmd,
			WL_RRM_CONFIG_MIN_LENGTH + len);
		goto done;

	} else if (argc == 2) {
		memset(&rrm_config_param, 0, sizeof(rrm_config_param));
		rrm_config_param.id = cmd_id;
		err = wlu_iovar_getbuf(wl, cmd->name, (void *)&rrm_config_param,
			sizeof(rrm_config_param), (void *)rrm_config_cmd, malloc_len);
		if (err < 0) {
			printf("wlu_iov_get for \"%s\" returned error %d\n", cmd->name, err);
			goto done;
		}

		prhex(cmd_id == WL_RRM_CONFIG_GET_CIVIC ? "Self Civic data" :
			cmd_id == WL_RRM_CONFIG_GET_LCI ? "Self LCI data" :
			"Self Location Identifier Data",
			rrm_config_cmd->data, rrm_config_cmd->len);
		printf("\n");
	} else {
		err = BCME_USAGE_ERROR;
	}
done:
	if (rrm_config_cmd)
		free(rrm_config_cmd);

	return err;
}

static int
wl_rrm_config(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_OK;
	int argc = 0;

	if (!argv[1]) {
		printf("%s: no element type given, must be lci or civic\n", __FUNCTION__);
		err = BCME_USAGE_ERROR;
		goto done;
	}

	for (argc = 0; argv[argc]; argc++)
		;

	if (strcmp(argv[1], "lci") == 0) {
		argv++;
		err = wl_rrm_self_lci_civic(wl, cmd, argv, argc,
			(argc == 3) ? WL_RRM_CONFIG_SET_LCI : WL_RRM_CONFIG_GET_LCI);
	} else if (strcmp(argv[1], "civic") == 0) {
		argv++;
		err = wl_rrm_self_lci_civic(wl, cmd, argv, argc,
			(argc == 3) ? WL_RRM_CONFIG_SET_CIVIC : WL_RRM_CONFIG_GET_CIVIC);
	} else if (strcmp(argv[1], "locid") == 0) {
		argv++;
		err = wl_rrm_self_lci_civic(wl, cmd, argv, argc,
			(argc == 3) ? WL_RRM_CONFIG_SET_LOCID : WL_RRM_CONFIG_GET_LOCID);
	} else {
		printf("%s: incorrect element type, not lci, civic or locid\n", __FUNCTION__);
		err = BCME_USAGE_ERROR;
	}
done:
	return err;
}

static int
wl_bcn_report_version_get(void *wl, uint16 category, char **argv)
{
	int ret = 0;
	bcm_xtlv_t *bcn_rpt_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 bcn_rpt_len, bcn_rpt_id;
	uint16 val_len;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;

	if (*argv) {
		printf("error, extra arg \"%s\"\n", *argv);
		return BCME_USAGE_ERROR;
	}

	/* init the wrapper bcn_report category XTLV in the IO buffer and copy to it */
	bcn_rpt_tlv = (bcm_xtlv_t *)&buf[0];
	bcn_rpt_tlv->id = htol16(category);
	bcn_rpt_tlv->len = 0;
	bcm_xtlv_pack_xtlv(bcn_rpt_tlv, category, 0, 0, no_pad);

	/* issue the 'bcn_report' get with the one-item id list */
	ret = wlu_iovar_getbuf(wl, "bcn_report", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN);
	if (ret) {
		return ret;
	}

	bcn_rpt_len = ltoh16(bcn_rpt_tlv->len);
	bcn_rpt_id = ltoh16(bcn_rpt_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(bcn_rpt_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("XTLV specifies length %u too long for iobuffer len %u\n",
			bcn_rpt_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the bcn_report xtlv container */
	if (bcn_rpt_id != category) {
		printf("return category %u did not match %u\n", bcn_rpt_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (bcn_rpt_len < BCM_XTLV_HDR_SIZE) {
		printf("return XTLV container specifies length %u too small to hold any values\n",
				bcn_rpt_len);
		return BCME_ERROR;
	}
	val_xtlv = (bcm_xtlv_t*)bcn_rpt_tlv->data;
	val_len = ltoh16(val_xtlv->len);

	/* make sure the value xtlv is valid xtlv container */
	if (!bcm_valid_xtlv(val_xtlv, bcn_rpt_len, no_pad)) {
		printf("return XTLV specifies length %u too long for containing xtlv len %u\n",
				val_len, bcn_rpt_len);
		return BCME_ERROR;
	}

	if (val_len == 0) {
		printf("bcn_report config get: return value XTLV (id:%u) empty\n",
			ltoh16(val_xtlv->id));
		return BCME_ERROR;
	}

	if (val_xtlv->id == WL_BCN_RPT_CMD_VER) {
		printf("version:%d\n", *((uint8*)val_xtlv->data));
	}
	return (ret);
}
static int
wl_bcn_report_config_get(void *wl, uint16 category, wl_bcn_report_cfg_t *bcn_rpt_cfg)
{
	int ret = BCME_OK;
	bcm_xtlv_t *bcn_rpt_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 bcn_rpt_len, bcn_rpt_id;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;

	/* init the wrapper bcn_report category XTLV in the IO buffer and copy to it */
	bcn_rpt_tlv = (bcm_xtlv_t *)&buf[0];
	bcn_rpt_tlv->id = htol16(category);
	bcn_rpt_tlv->len = 0;
	bcm_xtlv_pack_xtlv(bcn_rpt_tlv, category, 0, 0, no_pad);

	/* issue the 'bcn_report' get with the one-item id list */
	ret = wlu_iovar_getbuf(wl, "bcn_report", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN);
	if (ret) {
		return ret;
	}

	bcn_rpt_len = ltoh16(bcn_rpt_tlv->len);
	bcn_rpt_id = ltoh16(bcn_rpt_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(bcn_rpt_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("XTLV specifies length %u too long for iobuffer len %u\n",
			bcn_rpt_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the bcn_report xtlv container */
	if (bcn_rpt_id != category) {
		printf("return category %u did not match %u\n", bcn_rpt_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (bcn_rpt_len < BCM_XTLV_HDR_SIZE) {
		printf("return XTLV container specifies length %u too small to hold any values\n",
				bcn_rpt_len);
		return BCME_ERROR;
	}
	val_xtlv = (bcm_xtlv_t*)bcn_rpt_tlv->data;

	memcpy(bcn_rpt_cfg, (uint8*)val_xtlv->data, sizeof(*bcn_rpt_cfg));

	return (ret);
}

static int
wl_bcn_report_vndr_ie_get(void *wl, uint16 category, vndr_ie_t **vndr_ie)
{
	int ret = 0;
	bcm_xtlv_t *bcn_rpt_tlv;
	bcm_xtlv_t *val_xtlv;
	uint16 bcn_rpt_len, bcn_rpt_id;
	const bcm_xtlv_opts_t no_pad = BCM_XTLV_OPTION_ALIGN32;
	int no_of_bytes;
	vndr_ie_t *vndr_ie_ptr;

	/* init the wrapper bcn_report category XTLV in the IO buffer and copy to it */
	bcn_rpt_tlv = (bcm_xtlv_t *)&buf[0];
	bcn_rpt_tlv->id = htol16(category);
	bcn_rpt_tlv->len = 0;
	bcm_xtlv_pack_xtlv(bcn_rpt_tlv, category, 0, 0, no_pad);

	/* issue the 'bcn_report' get with the one-item id list */
	ret = wlu_iovar_getbuf(wl, "bcn_report", buf, BCM_XTLV_HDR_SIZE, buf, WLC_IOCTL_SMLEN);
	if (ret) {
		return ret;
	}

	bcn_rpt_len = ltoh16(bcn_rpt_tlv->len);
	bcn_rpt_id = ltoh16(bcn_rpt_tlv->id);

	/* make sure the return xtlv is valid for the iobuffer */
	if (!bcm_valid_xtlv(bcn_rpt_tlv, WLC_IOCTL_SMLEN, no_pad)) {
		printf("XTLV specifies length %u too long for iobuffer len %u\n",
			bcn_rpt_len, WLC_IOCTL_SMLEN);
		return BCME_ERROR;
	}

	/* the return buf should start with the bcn_report xtlv container */
	if (bcn_rpt_id != category) {
		printf("return category %u did not match %u\n", bcn_rpt_id, category);
		return BCME_ERROR;
	}

	/* make sure the container has enough room for an XTLV header */
	if (bcn_rpt_len < BCM_XTLV_HDR_SIZE) {
		printf("return XTLV container specifies length %u too small to hold any values\n",
				bcn_rpt_len);
		return BCME_ERROR;
	}
	val_xtlv = (bcm_xtlv_t*)bcn_rpt_tlv->data;

	no_of_bytes = VNDR_IE_FIXED_LEN + ((char*)val_xtlv->data)[1];
	vndr_ie_ptr = (vndr_ie_t *) calloc(1, no_of_bytes);
	if (vndr_ie_ptr != NULL) {
		memcpy(vndr_ie_ptr, (uint8*)val_xtlv->data, no_of_bytes);
		*vndr_ie = vndr_ie_ptr;
		ret = BCME_OK;
	} else {
		ret = BCME_NOMEM;
	}

	return (ret);
}
static int
wl_bcn_report_config_set(void *wl, uint16 category, char **argv)
{
	char *valstr;
	char *endptr = NULL;
	bcm_xtlv_t *bcn_rpt_tlv;
	uint out_len, val;
	int ret = BCME_OK;
	wl_bcn_report_cfg_t *bcn_rpt_cfg;
	wl_bcn_report_cfg_t bcn_rpt_cfg_temp;

	char *p;
	/* total output buffer will be an auth sub-category xtlv holding
	 * an xtvl of 1 or 2 uint32s.
	 */
	bcn_rpt_tlv = (bcm_xtlv_t*)(&buf[0]);
	bcn_rpt_tlv->id = htol16(category);
	bcn_rpt_cfg = (wl_bcn_report_cfg_t*)bcn_rpt_tlv->data;
	/* GET Beacon report CONFIG */
	ret = wl_bcn_report_config_get(wl, WL_BCN_RPT_CMD_CONFIG, &bcn_rpt_cfg_temp);
	if (ret) {
		return (ret);
	}
	*bcn_rpt_cfg = bcn_rpt_cfg_temp;
	while (*argv) {
		p = *argv++;
		switch (p[1]) {
		case 'o':
			valstr = *argv++;
			if (!valstr || valstr[0] == '\0') {
				printf("bcn_report config set: missing value for "
					"override option\n");
				return BCME_USAGE_ERROR;
			}
			val = (uint8)strtoul(valstr, &endptr, 0);
			if (val > WL_BCN_RPT_CCX_IE_OVERRIDE) {
				return BCME_RANGE;
			}
			bcn_rpt_cfg->flags &= ~WL_BCN_RPT_CCX_IE_OVERRIDE;
			bcn_rpt_cfg->flags |= (uint8)val;
			break;
		case 'm':
			valstr = *argv++;
			if (!valstr || valstr[0] == '\0') {
				printf("bcn_report config set: missing value for mode option\n");
				return BCME_USAGE_ERROR;
			}

			val = (uint8)strtoul(valstr, &endptr, 0);
			if (val > WL_BCN_RPT_ASSOC_SCAN_MODE_MAX) {
				return BCME_RANGE;
			}
			bcn_rpt_cfg->flags &= ~WL_BCN_RPT_ASSOC_SCAN_MODE_MASK;
			bcn_rpt_cfg->flags |= val << WL_BCN_RPT_ASSOC_SCAN_MODE_SHIFT;
			break;
		case 'n':
			valstr = *argv++;
			if (!valstr || valstr[0] == '\0') {
				printf("bcn_report config set: missing value for "
					"number of cache entries\n");
				return BCME_USAGE_ERROR;
			}
			val = (uint8)strtoul(valstr, &endptr, 0);
			if (val > WL_BCN_RPT_ASSOC_SCAN_CACHE_COUNT_MAX) {
				return BCME_RANGE;
			}
			bcn_rpt_cfg->scan_cache_cnt = val;
			break;
		case 't':
			valstr = *argv++;
			if (!valstr || valstr[0] == '\0') {
				printf("bcn_report config set: missing value for timeout option\n");
				return BCME_USAGE_ERROR;
			}
			val = htol32(strtoul(valstr, &endptr, 0));
			if (val < WL_BCN_RPT_ASSOC_SCAN_CACHE_TIMEOUT_MIN) {
				return BCME_RANGE;
			}
			bcn_rpt_cfg->scan_cache_timeout = val;
			break;
		default:
			if (strstr(valstr, "h")) {
				printf("%s", BCN_REPORT_HELP);
				printf("%s\n", BCN_REPORT_CONFIG_HELP);
				return BCME_OK;
			}
			ret = BCME_USAGE_ERROR;
			break;
		}
	}

	if (ret == BCME_OK) {
		bcn_rpt_tlv->len = htol16(sizeof(*bcn_rpt_cfg));
		out_len = bcm_xtlv_size_for_data(sizeof(*bcn_rpt_cfg), BCM_XTLV_OPTION_ALIGN32);
		ret = wlu_iovar_set(wl, "bcn_report", bcn_rpt_tlv, out_len);
	}
	return ret;
}

static int
wl_bcn_report_vndr_ie_set(void *wl, uint16 category, char **argv)
{
	char *valstr;
	bcm_xtlv_t *bcn_rpt_tlv;
	int ret = 0;
	vndr_ie_t *vndr_ie;
	int32 sizeof_ie;
	int32 out_len;

	/* total output buffer will be an auth sub-category xtlv holding
	 * an xtvl of 1 or 2 uint32s.
	 */
	bcn_rpt_tlv = (bcm_xtlv_t*)(&buf[0]);
	bcn_rpt_tlv->id = htol16(category);
	vndr_ie = (vndr_ie_t*)bcn_rpt_tlv->data;

	valstr = *argv++;
	sizeof_ie = strlen(valstr);
	if (strstr(valstr, "h")) {
		printf("%s", BCN_REPORT_HELP);
		printf("%s\n", BCN_REPORT_VNDR_IE_HELP);
		return BCME_OK;
	}

	if ((ret = get_ie_data((uchar*)valstr, (uchar*)vndr_ie, sizeof_ie))) {
		return ret;
	}

	/* As two characters form one bytes we are dividing by 2 */
	bcn_rpt_tlv->len = htol16(sizeof(*vndr_ie) + sizeof_ie/2);
	out_len = bcm_xtlv_size_for_data(sizeof(*vndr_ie) + sizeof_ie/2, BCM_XTLV_OPTION_ALIGN32);
	ret = wlu_iovar_set(wl, "bcn_report", bcn_rpt_tlv, out_len);
	return ret;
}

static int
wl_bcn_report(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	char *sub_cmd_name, *ptr;
	wl_bcn_report_cfg_t bcn_rpt_cfg;
	char* attr_name;
	vndr_ie_t *vndr_ie = NULL;
	int i;
	UNUSED_PARAMETER(cmd);

	/* Skip the command name */
	argv++;

	sub_cmd_name = *argv++;

	if (sub_cmd_name == NULL) {
		/* usage */
		printf("bcn_report: missing category\n");
		return BCME_USAGE_ERROR;
	}

	if (!strcmp(sub_cmd_name, "ver")) {
		/* GET beacon report ver */
		ret = wl_bcn_report_version_get(wl, WL_BCN_RPT_CMD_VER, argv);
	} else if (!strcmp(sub_cmd_name, "config")) {
		attr_name = *argv;

		if (attr_name == NULL) {
			/* GET Beacon report CONFIG */
			ret = wl_bcn_report_config_get(wl, WL_BCN_RPT_CMD_CONFIG, &bcn_rpt_cfg);
			if (ret) {
				return (ret);
			}
			printf("override:%d\n", bcn_rpt_cfg.flags & WL_BCN_RPT_CCX_IE_OVERRIDE);
			printf("mode:%#x\n", (bcn_rpt_cfg.flags & WL_BCN_RPT_ASSOC_SCAN_MODE_MASK)
					>> WL_BCN_RPT_ASSOC_SCAN_MODE_SHIFT);
			printf("cache_timeout:%u\n", bcn_rpt_cfg.scan_cache_timeout);
			printf("cache_timeout pending:%d\n", bcn_rpt_cfg.scan_cache_timer_pend);
			printf("cache_count:%d\n", bcn_rpt_cfg.scan_cache_cnt);
		}
		else {
			/* SET Beacon report Config */
			ret = wl_bcn_report_config_set(wl, WL_BCN_RPT_CMD_CONFIG, argv);
		}
	} else if (!strcmp(sub_cmd_name, "vndr_ie")) {
		attr_name = *argv;
		if (attr_name == NULL) {
			/* GET Beacon report vendor IE */
			ret = wl_bcn_report_vndr_ie_get(wl, WL_BCN_RPT_CMD_VENDOR_IE, &vndr_ie);
			if (ret == BCME_OK && (vndr_ie)) {
				ptr = (char*)vndr_ie;
				printf("vendor IE:");
				for (i = 0; i < (vndr_ie->len + VNDR_IE_HDR_LEN); i++) {
					printf("%02x", ((uchar*)ptr)[0]);
					ptr++;
				}
				printf("\n");
			}
			if (vndr_ie) {
				free(vndr_ie);
			}
		} else {
			/* SET Beacon report vendor ie */
			ret = wl_bcn_report_vndr_ie_set(wl, WL_BCN_RPT_CMD_VENDOR_IE, argv);
		}
	} else {
		/* usage */
		if (!strstr(sub_cmd_name, "h")) {
			printf("unknown bcn_report sub-commands \"%s\"\n", sub_cmd_name);
		}
		ret = BCME_USAGE_ERROR;
	}

	return ret;
}

static int
wl_rrm_frng_req(void *wl, cmd_t *cmd, char **argv, int argc, int cmd_id)
{
	int err = 0, i, buflen = WL_RRM_FRNG_MIN_LENGTH;
	frngreq_t *frng_buf;
	wl_rrm_frng_ioc_t *rrm_frng_cmd = NULL;
	int min_args = 7; /* can leave off target params to indicate using current neighbor list */
	int target_args = 6;
	int malloc_len = sizeof(*rrm_frng_cmd) + sizeof(frngreq_t) +
		(sizeof(frngreq_target_t) * (DOT11_FTM_RANGE_ENTRY_MAX_COUNT-1));

	rrm_frng_cmd = (wl_rrm_frng_ioc_t *) calloc(1, malloc_len);
	if (rrm_frng_cmd == NULL) {
		printf("Failed to allocate buffer of %d bytes\n", malloc_len);
		err = BCME_NOMEM;
		goto done;
	}

	rrm_frng_cmd->id = cmd_id;

	if (argc < min_args) {
		printf("%s: too few arguments %d\n", __FUNCTION__, argc);
		err = BCME_USAGE_ERROR;
		goto done;
	}

	frng_buf = (frngreq_t *) &rrm_frng_cmd->data[0];
	frng_buf->reps = 0; /* Only one repetition of a range request */
	frng_buf->event = WL_RRM_EVENT_NONE; /* Receive processing should set this */

	if (argv[1]) {
		/* da */
		if (!wl_ether_atoe(argv[1], &frng_buf->da)) {
			printf("%s: parsing da failed\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}

	/* max initialization delay */
	if (argv[2]) {
		frng_buf->max_init_delay = htod16((uint16)(strtoul(argv[2], NULL, 0)));
	}

	/* min AP count (1-DOT11_FTM_RANGE_ENTRY_MAX_COUNT) */
	if (argv[3]) {
		frng_buf->min_ap_count = (uint8) strtoul(argv[3], NULL, 0);
		if (frng_buf->min_ap_count < 1 ||
			frng_buf->min_ap_count > DOT11_FTM_RANGE_ENTRY_MAX_COUNT) {
			printf("%s: min AP count must be between 1 and 15\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}

	/* num APs */
	if (argv[4]) {
		frng_buf->num_aps = (uint8) strtoul(argv[4], NULL, 0);
		if (frng_buf->num_aps < frng_buf->min_ap_count)
		{
			printf("%s: num_aps %d must be >= min_ap_count %d\n",
				__FUNCTION__, frng_buf->num_aps, frng_buf->min_ap_count);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}

	/* 6 arguments for each target */
	if (argc > (min_args + (frng_buf->num_aps * target_args))) {
		printf("%s: incorrect # of arguments: %d\n", __FUNCTION__, argc);
		err = BCME_USAGE_ERROR;
		goto done;
	}

	/* Max Age, 0 = unused, 0xffff = any time is acceptable */
	if (argv[5]) {
		frng_buf->max_age = htod16((uint16) strtoul(argv[5], NULL, 0));
	}

	buflen += OFFSETOF(frngreq_t, targets);

	for (i = 0; i < frng_buf->num_aps && i < DOT11_FTM_RANGE_ENTRY_MAX_COUNT; i++) {
		if (argv[6+(i*target_args)]) {
			if (!wl_ether_atoe(argv[6+(i*target_args)], &frng_buf->targets[i].bssid)) {
				printf("%s: parsing bssid %d failed\n", __FUNCTION__, i);
				err = BCME_USAGE_ERROR;
				goto done;
			}
		} else {
			/* try to use current neighbor list */
			buflen += sizeof(frngreq_target_t);
			break;
		}

		if (argv[7+(i*target_args)]) {
			frng_buf->targets[i].channel = (uint8) strtoul(argv[7+(i*target_args)],
				NULL, 0);
		} else {
			printf("%s: not enough arguments: %d\n", __FUNCTION__, argc);
			err = BCME_USAGE_ERROR;
			goto done;
		}

		if (argv[8+(i*target_args)]) {
			frng_buf->targets[i].reg = (uint8) strtoul(argv[8+(i*target_args)],
				NULL, 0);
		} else {
			printf("%s: not enough arguments: %d\n", __FUNCTION__, argc);
			err = BCME_USAGE_ERROR;
			goto done;
		}

		if (argv[9+(i*target_args)]) {
			frng_buf->targets[i].phytype = (uint8) strtoul(argv[9+(i*target_args)],
				NULL, 0);
		} else {
			printf("%s: not enough arguments: %d\n", __FUNCTION__, argc);
			err = BCME_USAGE_ERROR;
			goto done;
		}

		if (argv[10+(i*target_args)]) {
			frng_buf->targets[i].bssid_info = (uint32)strtoul(argv[10+(i*target_args)],
				NULL, 0);
		} else {
			printf("%s: not enough arguments: %d\n", __FUNCTION__, argc);
			err = BCME_USAGE_ERROR;
			goto done;
		}

		if (argv[11+(i*target_args)]) {
			chanspec_t chanspec = wf_chspec_aton(argv[11+(i*target_args)]);
			frng_buf->targets[i].chanspec = wl_chspec_to_driver(chanspec);
		} else {
			printf("%s: not enough arguments: %d\n", __FUNCTION__, argc);
			err = BCME_USAGE_ERROR;
			goto done;
		}

		buflen += sizeof(frngreq_target_t);
	}

	rrm_frng_cmd->len = buflen - WL_RRM_FRNG_MIN_LENGTH;

	err = wlu_iovar_set(wl, cmd->name, (void *)rrm_frng_cmd, buflen);

	if (err != BCME_OK) {
		printf("%s: err %d returned from wlu_iovar_set\n", __FUNCTION__, err);
	}

done:
	if (rrm_frng_cmd) {
		free(rrm_frng_cmd);
	}

	return err;
}

static int
wl_rrm_frng_rep(void *wl, cmd_t *cmd, char **argv, int argc, int cmd_id)
{
	int err = 0;
	int buflen = 0;
	frngrep_t *frng_buf;
	wl_rrm_frng_ioc_t *rrm_frng_cmd = NULL;
	int malloc_len = sizeof(*rrm_frng_cmd) + sizeof(*frng_buf);

	rrm_frng_cmd = (wl_rrm_frng_ioc_t *) calloc(1, malloc_len);

	if (rrm_frng_cmd == NULL) {
		printf("Failed to allocate buffer of %d bytes\n", malloc_len);
		err = BCME_NOMEM;
		goto done;
	}

	rrm_frng_cmd->id = (uint16)cmd_id;

	/*
	 * Currently, this function allows only the sending of one
	 * range entry report with optionally one error report --
	 * defining all the fields for multiple entries of both
	 * range entry cound and error entry count is too unwieldy
	 * for use in a wl command
	 */

	if (argc < 9) {
		printf("%s: incorrect # of arguments %d\n", __FUNCTION__, argc);
		err = BCME_USAGE_ERROR;
		goto done;
	}

	frng_buf = (frngrep_t *)rrm_frng_cmd->data;
	buflen += OFFSETOF(frngrep_t, range_entries);

	frng_buf->dialog_token = 0; /* token should be input and match with request token */
	frng_buf->event = WL_RRM_EVENT_NONE; /* Receive processing should set this */

	/* da */
	if (argv[1]) {
		if (!wl_ether_atoe(argv[1], &frng_buf->da)) {
			printf("%s: parsing da failed\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}

	/*
	 * Range entry count, currently only one allowed in wl command, however
	 * keeping the paramter allows for easier future expansion
	*/
	if (argv[2]) {
		frng_buf->range_entry_count = (uint8)(strtoul(argv[2], NULL, 0));
		if (frng_buf->range_entry_count != 1) {
			printf("%s: range entry count can be only 1\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}

	/* Error entry count, 0-1 */
	if (argv[3]) {
		frng_buf->error_entry_count = (uint8)(strtoul(argv[3], NULL, 0));
		if (frng_buf->error_entry_count > 1) {
			printf("%s: error entry count can be only 0 or 1\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}
		if (frng_buf->error_entry_count == 1 && argc < 10) {
			printf("%s: incorrect # of arguments %d\n", __FUNCTION__, argc);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}

	/* Measurement start time */
	if (argv[4]) {
		frng_buf->range_entries[0].start_tsf = htod32((uint32)(strtoul(argv[4], NULL, 0)));
		if (frng_buf->error_entry_count != 0) {
			frng_buf->error_entries[0].start_tsf = htod32((uint32)(strtoul(argv[4],
				NULL, 0)));
		}
	}

	/* BSSID whose range is being reported */
	if (argv[5]) {
		if (!wl_ether_atoe(argv[5], &frng_buf->range_entries[0].bssid)) {
			printf("%s: parsing bssid failed\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}
		if (frng_buf->error_entry_count != 0) {
			(void) wl_ether_atoe(argv[5], &frng_buf->error_entries[0].bssid);
		}
	}

	/* Range */
	if (argv[6]) {
		frng_buf->range_entries[0].range = htod16((uint16)(strtoul(argv[6], NULL, 0)));
	}

	/* Max range error */
	if (argv[7]) {
		frng_buf->range_entries[0].max_err = htod16((uint16)(strtoul(argv[7], NULL, 0)));
	}

	/* Error code */
	if (argv[8]) {
		frng_buf->error_entries[0].code = (uint8)(strtoul(argv[8], NULL, 0));

		if ((frng_buf->error_entries[0].code != DOT11_FTM_RANGE_ERROR_AP_INCAPABLE) &&
			(frng_buf->error_entries[0].code != DOT11_FTM_RANGE_ERROR_AP_FAILED) &&
			(frng_buf->error_entries[0].code != DOT11_FTM_RANGE_ERROR_TX_FAILED))
		{
			printf("%s: error code %d incorrect, should be %d, %d, or %d\n",
				__FUNCTION__, frng_buf->error_entries[0].code,
				DOT11_FTM_RANGE_ERROR_AP_INCAPABLE,
				DOT11_FTM_RANGE_ERROR_AP_FAILED,
				DOT11_FTM_RANGE_ERROR_TX_FAILED);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}

	buflen += DOT11_FTM_RANGE_ENTRY_MAX_COUNT * sizeof(frngrep_range_t);
	buflen += frng_buf->error_entry_count * sizeof(frngrep_error_t);
	rrm_frng_cmd->len = buflen;
	buflen += WL_RRM_FRNG_MIN_LENGTH;

	err = wlu_iovar_set(wl, cmd->name, (void *)rrm_frng_cmd, buflen);

	if (err != BCME_OK) {
		printf("%s: err %d returned from wlu_iovar_set\n", __FUNCTION__, err);
	}

done:
	if (rrm_frng_cmd) {
		free(rrm_frng_cmd);
	}

	return err;
}

static int
wl_rrm_frng_dir(void *wl, cmd_t *cmd, char **argv, int argc, int cmd_id)
{
	int err = 0;
	int buflen = 0;
	wl_rrm_frng_ioc_t *rrm_frng_cmd = NULL;
	uint8 mode = 0;
	int malloc_len = sizeof(*rrm_frng_cmd) + sizeof(mode);

	rrm_frng_cmd = (wl_rrm_frng_ioc_t *) calloc(1, malloc_len);

	if (rrm_frng_cmd == NULL) {
		printf("Failed to allocate buffer of %d bytes\n", malloc_len);
		err = BCME_NOMEM;
		goto done;
	}

	rrm_frng_cmd->id = (uint16)cmd_id;

	if (argc != 3) {
		printf("%s: incorrect # of arguments %d\n", __FUNCTION__, argc);
		err = BCME_USAGE_ERROR;
		goto done;
	}

	/* input is 1 or 0 */
	if (argv[1]) {
		mode = (uint8)(strtoul(argv[1], NULL, 0));
		if (mode != 1 && mode != 0) {
			printf("%s: expected 0|1 value\n", __FUNCTION__);
			err = BCME_USAGE_ERROR;
			goto done;
		}
	}
	rrm_frng_cmd->data[0] = mode;
	buflen = sizeof(mode);
	rrm_frng_cmd->len = buflen;
	buflen += WL_RRM_FRNG_MIN_LENGTH;

	err = wlu_iovar_set(wl, cmd->name, (void *)rrm_frng_cmd, buflen);

	if (err != BCME_OK) {
		printf("%s: err %d returned from wlu_iovar_set\n", __FUNCTION__, err);
	}

done:
	if (rrm_frng_cmd) {
		free(rrm_frng_cmd);
	}

	return err;
}

static int
wl_rrm_frng(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;
	int argc;

	if (!argv[1]) {
		printf("%s: no subcommand given, must be send_req|send_rep|direct\n", __FUNCTION__);
		return BCME_USAGE_ERROR;
	}

	for (argc = 0; argv[argc]; argc++)
		;

	if (strcmp(argv[1], "send_req") == 0) {
		argv++;
		err = wl_rrm_frng_req(wl, cmd, argv, argc, WL_RRM_FRNG_SET_REQ);
	} else if (strcmp(argv[1], "send_rep") == 0) {
		argv++;
		err = wl_rrm_frng_rep(wl, cmd, argv, argc, WL_RRM_FRNG_SET_REP);
	} else if (strcmp(argv[1], "direct") == 0) {
		argv++;
		err = wl_rrm_frng_dir(wl, cmd, argv, argc, WL_RRM_FRNG_SET_DIR);
	} else {
		printf("%s: incorrect command, not send_req|send_rep|direct\n", __FUNCTION__);
		return BCME_USAGE_ERROR;
	}

	return err;
}

static int
wl_rrm_nbr_ssid(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	nbr_rpt_elem_t *nbr_elt;
	const char *cmdname = "ssid";
	char smbuf[WLC_IOCTL_SMLEN];
	int buflen = 0;

	memset(smbuf, 0, sizeof(smbuf));
	strncpy(&smbuf[buflen], cmdname, strlen(cmdname));
	buflen += strlen(cmdname) + 1;
	nbr_elt = (nbr_rpt_elem_t *) &smbuf[buflen];
	buflen += sizeof(*nbr_elt);

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 3) {
		return BCME_USAGE_ERROR;
	}

	/* bssid */
	if (!wl_ether_atoe(argv[1], &nbr_elt->bssid)) {
		printf("wl_rrm_nbr_ssid parsing bssid failed\n");
		return BCME_USAGE_ERROR;
	}

	/* SSID */
	if (argc == 3 && argv[2]) {
		uint32 len;
		wlc_ssid_t ssid;

		len = strlen(argv[2]);
		if (len > DOT11_MAX_SSID_LEN) {
			printf("ssid too long\n");
			return (-1);
		}
		memset(&ssid, 0, sizeof(wlc_ssid_t));
		memcpy(ssid.SSID, argv[2], len);
		ssid.SSID_len = len;
		memcpy(&nbr_elt->ssid, &ssid, sizeof(wlc_ssid_t));
	} else {
		memset(&nbr_elt->ssid, 0, sizeof(wlc_ssid_t));
	}

	nbr_elt->id = DOT11_MNG_NEIGHBOR_REP_ID;

	/*
	Note that nbr_elt.len will be the size of the neighbor element
	itself, as this structure does not go out on the wire
	*/
	nbr_elt->len = sizeof(nbr_elt);
	/*
	printf("wl_rrm_nbr_add_nbr element len %d\n", nbr_elt->len);
	*/

	nbr_elt->version = WL_RRM_NBR_RPT_VER;

	err = wlu_iovar_set(wl, cmd->name, smbuf, buflen);

	return err;
}

static int
wl_rrm_nbr_chanspec(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	nbr_rpt_elem_t *nbr_elt;
	const char *cmdname = "chanspec";
	char smbuf[WLC_IOCTL_SMLEN];
	int buflen = 0;

	memset(smbuf, 0, sizeof(smbuf));
	strncpy(&smbuf[buflen], cmdname, strlen(cmdname));
	buflen += strlen(cmdname) + 1;
	nbr_elt = (nbr_rpt_elem_t *) &smbuf[buflen];
	buflen += sizeof(*nbr_elt);

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 3) {
		return BCME_USAGE_ERROR;
	}

	/* bssid */
	if (!wl_ether_atoe(argv[1], &nbr_elt->bssid)) {
		printf("wl_rrm_nbr_chanspec parsing bssid failed\n");
		return BCME_USAGE_ERROR;
	}

	/* chanspec */
	if (argc == 3 && argv[2]) {
		chanspec_t chanspec = wf_chspec_aton(argv[2]);
		if (chanspec == 0 || !(wf_chspec_valid(chanspec))) {
			return BCME_BADCHAN;
		}
		nbr_elt->chanspec = wl_chspec_to_driver(chanspec);
	}

	nbr_elt->id = DOT11_MNG_NEIGHBOR_REP_ID;
	nbr_elt->len = sizeof(nbr_elt);
	nbr_elt->version = WL_RRM_NBR_RPT_VER;

	err = wlu_iovar_set(wl, cmd->name, smbuf, buflen);

	return err;
}

static int
wl_rrm_nbr_lci(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	int len = 0;
	char smbuf[WLC_IOCTL_SMLEN*2];
	char *bufptr = &smbuf[0];
	const char *cmdname = "lci";
	int buflen = 0;

	memset(smbuf, 0, sizeof(smbuf));
	strncpy(&smbuf[buflen], cmdname, strlen(cmdname));
	buflen += strlen(cmdname) + 1;
	bufptr += buflen;

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 3) {
		return BCME_USAGE_ERROR;
	}

	/* bssid */
	if (!wl_ether_atoe(argv[1], (struct ether_addr *)bufptr)) {
		printf("%s: parsing bssid failed\n", __FUNCTION__);
		return BCME_USAGE_ERROR;
	}

	bufptr += ETHER_ADDR_LEN;
	buflen += ETHER_ADDR_LEN;

	len = wl_rrm_parse_location(argv[2], bufptr, sizeof(smbuf));
	buflen += len;

	if (len < 0) {
		printf("%s: parsing LCI location arguments failed\n", __FUNCTION__);
		return -1;
	}

	err = wlu_iovar_set(wl, cmd->name, smbuf, buflen);

	return err;
}

static int
wl_rrm_nbr_civic(void *wl, cmd_t *cmd, char **argv)
{
	int argc;
	int err = 0;
	int len = 0;
	char smbuf[WLC_IOCTL_SMLEN*2];
	char *bufptr = &smbuf[0];
	const char *cmdname = "civic";
	int buflen = 0;

	memset(smbuf, 0, sizeof(smbuf));
	strncpy(&smbuf[buflen], cmdname, strlen(cmdname));
	buflen += strlen(cmdname) + 1;
	bufptr += buflen;

	for (argc = 0; argv[argc]; argc++)
		;

	if (argc != 3) {
		return BCME_USAGE_ERROR;
	}

	/* bssid */
	if (!wl_ether_atoe(argv[1], (struct ether_addr *) bufptr)) {
		printf("%s: parsing bssid failed\n", __FUNCTION__);
		return BCME_USAGE_ERROR;
	}

	bufptr += ETHER_ADDR_LEN;
	buflen += ETHER_ADDR_LEN;

	len = wl_rrm_parse_location(argv[2], bufptr, sizeof(smbuf));
	buflen += len;

	if (len < 0) {
		printf("%s: parsing Civic location arguments failed\n", __FUNCTION__);
		return -1;
	}

	err = wlu_iovar_set(wl, cmd->name, smbuf, buflen);

	return err;
}

static int
wl_rrm_nbr(void *wl, cmd_t *cmd, char **argv)
{
	int err = 0;

	if (!argv[1]) {
		printf("%s: no element type given, must be ssid, lci, or civic\n", __FUNCTION__);
		return BCME_USAGE_ERROR;
	}

	if (strcmp(argv[1], "ssid") == 0) {
		argv++;
		err = wl_rrm_nbr_ssid(wl, cmd, argv);
	} else if (strcmp(argv[1], "lci") == 0) {
		argv++;
		err = wl_rrm_nbr_lci(wl, cmd, argv);
	} else if (strcmp(argv[1], "civic") == 0) {
		argv++;
		err = wl_rrm_nbr_civic(wl, cmd, argv);
	} else if (strcmp(argv[1], "chanspec") == 0) {
		argv++;
		err = wl_rrm_nbr_chanspec(wl, cmd, argv);
	} else {
		printf("%s: incorrect element type, not ssid, lci, or civic\n", __FUNCTION__);
		return BCME_USAGE_ERROR;
	}

	return err;
}
