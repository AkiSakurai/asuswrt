/*
 * wl dpstats/pktq_stats command module
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
 * $Id: $
 */

#include <wlioctl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"
#include <errno.h>

static cmd_func_t wl_iov_pktqlog_params;
static cmd_func_t wl_scb_bs_data;
static cmd_func_t wl_scb_rx_report;

/* wl twt top level command list */
static cmd_t wl_perf_utils_cmds[] = {
	{ "pktq_stats", wl_iov_pktqlog_params, WLC_GET_VAR, -1,
	"Dumps packet queue log info for [C] common, [B] broadcast, [A] AMPDU, [N] NAR or [P] power"
	" save queues\nA:, D:, N: or P: are used to prefix a MAC address (a colon : separator is "
	"necessary),\nor else C: / B: is used alone. The '+' option after the colon gives more "
	"details.\nUp to 4 parameters may be given, the common queue is default when no "
	"parameters\nare supplied\nUse D:<xx:xx:xx:xx:xx:xx> option to release memory used for "
	"logging.\nUse '/<PREC>' as suffix to restrict to certain prec indices; "
	"multiple /<PREC>/<PREC>/...can be used\nAlso, '//' as a suffix to the MAC address or "
	"'C://' will enable automatic logging of\nall prec as they are seen.\nFull automatic "
	"operation is also possible with the shorthand\n'A:' (or 'A://'), 'P:' (or 'P://') etc "
	"which scans through all known addresses for\nthose parameters that take a MAC address.\n"
	"Optional parameter '-%' will show output format in percent terms for some data.\n"
	"wl pktq_stats [-%] [C:[+]]|[B:]|[A:[+]|D:|P:|N:[+]<xx:xx:xx:xx:xx:xx>]"
	"[/<PREC>[/<PREC>]][//]..." },
	{ "dpstats", wl_iov_pktqlog_params, WLC_GET_VAR, -1,
	"Same usage/syntax as 'pktq_stats' command; supports addition options:\n"
	"Optional parameter '-nr' can be used for non-reset; that is data stats are cumulative and "
	"not automatically cleared.\n""Address prefix [M] will dump multi-user TX statistics;\n"
	"Address prefix [S] will dump traffic scheduler information.\n"
	"wl dpstats [-%] [-nr] [C:[+]]|[B:]|[A:[+]|D:|P:|M:|N:[+]|S:|<xx:xx:xx:xx:xx:xx>]"
	"[/<TID>[/<TID>]][//]..." },
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
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_perf_utils_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register TWT commands */
	wl_module_cmds_register(wl_perf_utils_cmds);
}

static char* wl_pktqlog_timestamp(uint32 time)
{
	static char timestamp[20];

	uint32 secs = time / 1000000;
	uint32 fraction = (time - secs*1000000 + 5) / 10;
	uint32 mins = secs / 60;

	secs -= mins * 60;

	snprintf(timestamp, sizeof(timestamp), "[%d:%02d.%05d]:", mins, secs, fraction);

	return timestamp;
}

static void
wl_txq_prec_dump(wl_iov_pktq_log_t* iov_pktq_log, uint32 index, bool hide_unknown, bool is_aqm,
	bool percent)
{
	enum {
		PL_NOT_USE = 0, PL5 = 1 << 1, PL6 = 1 << 2,  PLS5 = 1 << 3, PLS6 = 1 << 4,
		PLL = 1 << 5, PLPCENT = 1 << 6, PLNORM = 1 << 7, PLPS = 1 << 8,
		PLSQS = 1 << 9, P_PREC = 1 << 10, P_TID = 1 << 11,
		PLSCB_QUEUE = 1 << 12, PLX_SQSS = 1 << 13, PL_AQM = 1 << 14, PLX_SCB_QUEUE = 1 << 15
	} usage_part, usage_full, usage_exclude;

	enum {
		PREC_LOG_PREC, PREC_LOG_TID, PREC_LOG_WME_AC, PREC_LOG_REQUESTED,
		PREC_LOG_SQS_VREQ, PREC_LOG_SQS_VUTIL, PREC_LOG_STORED, PREC_LOG_SELFSAVE,
		PREC_LOG_SAVE, PREC_LOG_FULLDROP, PREC_LOG_DROPPED, PREC_LOG_SACRIFICED,
		PREC_LOG_TXFAIL, PREC_LOG_RETRIED, PREC_LOG_RETRIED100, PREC_LOG_RTSFAIL,
		PREC_LOG_RETRYDROP, PREC_LOG_RETRYDROP100, PREC_LOG_PSRETRY, PREC_LOG_SUPPRESSED,
		PREC_LOG_ACKED, PREC_LOG_AVAIL, PREC_LOG_UTILISATION, PREC_LOG_UTILISATION100,
		PREC_LOG_QLENGTH, PREC_LOG_DATAR, PREC_LOG_RATER, PREC_LOG_PHYR, PREC_LOG_MCS,
		PREC_LOG_BW, PREC_LOG_SGI, PREC_LOG_NSS, PREC_LOG_AIRUSE100, PREC_LOG_EFFICIENCY100,
		NUM_PREC_LOGS
	};

	typedef struct {
		const char* heading;
		const char* format;
		const char* aqm_subst;
		const uint32 usage;
	} prec_log_definitions_t;

	const char* pdump_9u = "%9u";
	const char* pdump_8u = "%8u";
	const char* pdump_5u = "%5u";
	const char* pdump_102f = "%10.2f";
	const char* pdump_1e = " %.1e";
	const char* pdump_82f = "%8.2f";
	const char* pdump_61f = "%6.1f";
	const char* pdump_71f = "%7.1f";
	const char* pdump_aqm5 = "    -";
	const char* pdump_aqm6 = "     -";
	const char* pdump_aqm7 = "      -";
	const char* pdump_aqm8 = "       -";
	const char* pdump_aqm10 = "         -";
	const char* pdump_aqm12 = "           -";
	const char* pdump_nss = " %2s/%2s/%2s/%2s";
	const char* pdump_mcs = " %8s";

	const prec_log_definitions_t counter_defs[NUM_PREC_LOGS] = {
		/* PREC_LOG_PREC */
		{ "prec:", "  %02u:",         NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PLPS | P_PREC },
		/* PREC_LOG_TID */
		{ " tid:", "  %02u:",         NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PLPS | P_TID },
		/* PREC_LOG_WME_AC */
		{ "(AC)",  " %2s ",           NULL,
		PLS6 | PLL |       PL6 | PLNORM | PLPCENT },
		/* PREC_LOG_REQUESTED */
		{ "    rqstd", pdump_9u,      NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PLPS },
		/* PREC_LOG_SQS_VREQ */
		{ ", sqsv enq", pdump_9u,     NULL,
		PLL |       PL6 | PLNORM | PLPCENT | PLSQS },
		/* PREC_LOG_SQS_VUTIL */
		{ ",sqmax", pdump_5u,      NULL,
		PLS6 | PLL       | PL6 | PLNORM | PLPCENT | PLSQS },
		/* PREC_LOG_STORED */
		{ ",  stored", pdump_8u,      NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PLPS | PLX_SQSS },
		/* PREC_LOG_SELFSAVE */
		{ ",selfsave", pdump_8u,      NULL,
		PLL | PL5 |  PL6 |     PLNORM | PLPCENT | PLX_SCB_QUEUE },
		/* PREC_LOG_SAVE */
		{ ",   saved", pdump_8u,      NULL,
		PLL | PL5 |  PL6 |     PLNORM | PLPCENT | PLX_SCB_QUEUE },
		/* PREC_LOG_FULLDROP */
		{ ",fulldrop", pdump_8u,      NULL,
		PLL | PL5 | PL6 |      PLNORM | PLPCENT | PLX_SCB_QUEUE },
		/* PREC_LOG_DROPPED */
		{ ", dropped", pdump_8u,      NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PLPS | PLX_SQSS },
		/* PREC_LOG_SACRIFICED */
		{ ",sacrficd", pdump_8u,      NULL,
		PLL | PL5 | PL6     | PLNORM | PLPCENT | PLPS | PLX_SCB_QUEUE},
		/* PREC_LOG_TXFAIL */
		{ ",  txfail", pdump_8u,      NULL,
		PLL |      PL6 | PLNORM | PLPCENT },
		/* PREC_LOG_RETRIED  */
		{ ", retried", pdump_8u,      pdump_aqm8,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM },
		/* PREC_LOG_RETRIED100  */
		{ ",  %retry", pdump_82f,     pdump_aqm8,
		PLS5 | PLS6 | PLL | PL5 | PL6 |          PLPCENT },
		/* PREC_LOG_RTSFAIL */
		{ ", rtsfail", pdump_8u,      pdump_aqm8,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT },
		/* PREC_LOG_RETRYDROP */
		{ ",rtrydrop", pdump_8u,      NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM },
		/* PREC_LOG_RETRYDROP100 */
		{ ",%rtydrop", pdump_1e,     NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6          | PLPCENT },
		/* PREC_LOG_PSRETRY */
		{ ", psretry", pdump_8u,      NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT },
		/* PREC_LOG_SUPPRESSED */
		{ ",supprssd", pdump_8u,      NULL,
		PLL | PL5 | PL6 | PLNORM | PLPCENT | PLSCB_QUEUE },
		/* PREC_LOG_ACKED */
		{ ",    acked", pdump_9u,     NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT },
		/* PREC_LOG_AVAIL */
		{ ",   avail", NULL,          NULL, PL_NOT_USE },
		/* PREC_LOG_UTILISATION */
		{ ",utlsn", pdump_5u,      NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM |           PLPS },
		/* PREC_LOG_UTILISATION100 */
		{ ",%utlsn", pdump_61f,     NULL,
		PLS5 | PLS6 | PLL | PL5 | PL6          | PLPCENT | PLPS },
		/* PREC_LOG_QLENGTH */
		{ ",q len", pdump_5u,         NULL,
		PLS5 |        PLL | PL5 | PL6 | PLNORM | PLPCENT | PLPS | PLX_SQSS },
		/* PREC_LOG_DATAR */
		{ ",data Mbits", pdump_102f,  pdump_aqm10,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PL_AQM },
		/* PREC_LOG_RATER */
		{ ", phy Mbits", pdump_102f,  pdump_aqm10,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PL_AQM },
		/* PREC_LOG_PHYR */
		{ ",rate Mbits", pdump_102f,  pdump_aqm10,
		PLL | PL5 | PL6 | PLNORM | PLPCENT | PLSCB_QUEUE | PL_AQM },
		/* PREC_LOG_MCS */
		{ ",mcs l/m/h", pdump_mcs, pdump_aqm7,
		PLS6 | PLL       | PL6 | PLNORM | PLPCENT | PLSCB_QUEUE | PL_AQM },
		/* PREC_LOG_BW */
		{ ", bw MHz", pdump_71f,      pdump_aqm7,
		PLS5 | PLS6 | PLL       | PL6 | PLNORM | PLPCENT | PLSCB_QUEUE | PL_AQM },
		/* PREC_LOG_SGI */
		{ ", %sgi", " %4s",           pdump_aqm5,
		PLS6 | PLL       | PL6 | PLNORM | PLPCENT | PLSCB_QUEUE | PL_AQM },
		/* PREC_LOG_NSS */
		{ ",%nss 1/2/3/4", pdump_nss, pdump_aqm12,
		PLS6 | PLL       | PL6 | PLNORM | PLPCENT | PL_AQM },
		/* PREC_LOG_AIRUSE100 */
		{ ",  %air", pdump_61f,       pdump_aqm6,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PL_AQM },
		/* PREC_LOG_EFFICIENCY100 */
		{ ", %effcy", pdump_71f,      pdump_aqm7,
		PLS5 | PLS6 | PLL | PL5 | PL6 | PLNORM | PLPCENT | PL_AQM }
	};

	uint8  prec;
	pktq_log_format_v06_t* logv06 = NULL;
	pktq_log_format_v05_t* logv05 = NULL;

	uint32 num_prec = 0;
	uint32 prec_mask = 0;
	uint32 pspretend_time = (uint32)-1;
	uint32 wme_ac = 0;
	uint32 logmask = 0;
	uint32 flags = 0;
	bool skip_odd_prec = FALSE;
	bool pl_long;

	usage_exclude = 0;
	usage_full = 0;
	pl_long = iov_pktq_log->params.addr_type[index] & 0x80 ? TRUE : FALSE;

	switch (iov_pktq_log->version) {
		case 5:
			logv05 = &iov_pktq_log->pktq_log.v05;
			prec_mask = logv05->counter_info[index];
			num_prec = logv05->num_prec[index];
			usage_full |= PL5;
			usage_full |= pl_long ? PLL : PLS5;
			pspretend_time = logv05->pspretend_time_delta[index];
			break;
		case 6:
			logv06 = &iov_pktq_log->pktq_log.v06;
			prec_mask = logv06->counter_info[index];
			num_prec = logv06->num_prec[index];
			usage_full |= PL6;
			usage_full |= pl_long ? PLL : PLS6;
			flags = (prec_mask & ~0xFFFF);

			if (flags & PKTQ_LOG_PS_QUEUE) {
				pspretend_time = logv06->pspretend_time_delta[index];
			} else {
				wme_ac = logv06->wme_ac[index];
			}
			if ((flags & PKTQ_LOG_SQS) && !pl_long &&
					(iov_pktq_log->params.addr_type[index] & 0x7F) != 'C') {
				usage_exclude |= PLX_SQSS;
			}
			break;
		default:
			fprintf(stderr, "unknown/unsupported binary format (%x)\n",
				iov_pktq_log->version);
			return;
	}

	usage_part = usage_full;

	/* test for 'unknown' data; unknown means either that
	 * the queue is invalid or else that the logging
	 * is not active at all.
	 */
	if (((prec_mask & 0xFFFF) == 0) && hide_unknown) {
		return;
	}

	if ((num_prec == 0) && hide_unknown) {
		return;
	}

	if (flags & PKTQ_LOG_PS_QUEUE) {
		usage_full |= PLPS;
	}

	if (flags & PKTQ_LOG_SQS) {
		usage_part |= PLSQS;
	} else {
		usage_exclude |= PLSQS;
	}

	if (percent) {
		usage_full |= PLPCENT;
	} else {
		usage_full |= PLNORM;
	}

	if (!is_aqm && iov_pktq_log->version == 5) {
		usage_exclude |= PL_AQM;
	}

	if (flags & PKTQ_LOG_AQM) {
		is_aqm = TRUE;
	} else if (num_prec <= NUMPRIO) {
		is_aqm = FALSE;
	}

	if (flags & PKTQ_LOG_AMPDU_PREC) {
		usage_exclude |= P_TID;
		skip_odd_prec = is_aqm;
	} else if (num_prec <= NUMPRIO) {
		usage_exclude |= P_PREC;
	} else {
		usage_exclude |= P_TID;
		skip_odd_prec = is_aqm;
	}

	if (iov_pktq_log->version >= 6) {
		usage_exclude |= (flags & PKTQ_LOG_SCB_QUEUE) ? PLX_SCB_QUEUE : PLSCB_QUEUE;
	}

	if (iov_pktq_log->version == 5) {
		char   marker[4] = "[X]";
		char * headings = &logv05->headings[0];
		char * heading_start;
		char * heading_end;

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
	}

	if (iov_pktq_log->version == 6) {
		char * timestamp = (flags & PKTQ_LOG_DISPLAY_TIMESTAMP) ?
			wl_pktqlog_timestamp(logv06->timestamp) : "";

		/* simplified headings for v06 */
		if (flags & PKTQ_LOG_DISPLAY_ETHERADDR) {
			fprintf(stdout, "%s%s "MACF"\n", timestamp, logv06->counters[index].heading,
				ETHER_TO_MACF(iov_pktq_log->params.ea[index]));
		} else {
			fprintf(stdout, "%s%s\n", timestamp, logv06->counters[index].heading);
		}
	}

	/* Note that this is zero if the data is invalid */
	if (!num_prec) {
		fprintf(stdout, "parameter %c:%s not valid\n",
			iov_pktq_log->params.addr_type[index] != 0 ?
			iov_pktq_log->params.addr_type[index] & 0x7F : ' ',
			wl_ether_etoa(&iov_pktq_log->params.ea[index]));
		return;
	}

	if (num_prec > WL_IOV_PKTQ_LOG_PRECS) {
		fprintf(stdout, "invalid parameter block (%u)", num_prec);
		return;
	}

	for (prec = 0; prec < NUM_PREC_LOGS; prec++) {
		logmask = counter_defs[prec].usage;
		if ((logmask & usage_part) && ((logmask & usage_full) == usage_full) &&
				((logmask & usage_exclude) == 0)) {
			fputs(counter_defs[prec].heading, stdout);
		}
	}

	fprintf(stdout, "  (%s%sv%u%s)%s\n", iov_pktq_log->params.addr_type[index] & 0x80 ? "+":"",
		percent ? "%":"", iov_pktq_log->version, iov_pktq_log->version == 5 ? "b" : "",
	        prec_mask & PKTQ_LOG_DEVELOPMENT_VERSION ? " D" : "");

	for (prec = 0; prec < num_prec; prec++) {
		float tput = 0.0;
		float txrate_succ = 0.0;
		float txrate_main = 0.0;
		float airuse = 0.0;
		float efficiency = 0.0;
		float retrydrop100 = 0.0;
		float retry100 = 0.0;
		float utilisation100 = 0.0;
		float txbw_succ = 0.0;
		float txsgi_succ = 0.0;
		uint32 try_count = 0;
		uint32 acked;
		uint64 airtime = 0;
		char* ac = "";
		float nss[4] = {0.0};
		char* nss_blank = " -";
		char* snss[4] = {nss_blank, nss_blank, nss_blank, nss_blank};
#define PKTQLOG_STR_LEN 4
		char   lnss[4][PKTQLOG_STR_LEN] = {""};
		char   mcs[12] = {"    -"};
		char   sgi_percent[8];
		uint32 nss_total = 0;

		struct {
			uint32 requested;	uint32 stored;
			uint32 saved;		uint32 selfsaved;
			uint32 full_dropped;	uint32 dropped;
			uint32 sacrificed;	uint32 busy;
			uint32 retry;		uint32 ps_retry;
			uint32 suppress;	uint32 retry_drop;
			uint32 max_avail;	uint32 max_used;
			uint32 queue_capacity;	uint32 rtsfail;
			uint32 acked;		uint64 txrate_succ;
			uint64 txrate_main;	uint64 throughput;
			uint64 time_delta;	uint64 airtime;
			uint64 bandwidth;	uint32 nss[4];
			uint32 sqsv_requested;	uint32 sqsv_max_used;
			uint16 mcs_bitfield;	uint32 sgi_count;
		} counters;

		if (!(prec_mask & (1 << prec))) {
			continue;
		}

		memset(&counters, 0, sizeof(counters));

#define PREC_COUNT_COPY(a, b, c)	a.c = b->counters[index][prec].c
#define PREC_COUNT_COPY6(a, b, c)	a.c = b->counters[index].pktq[prec].c

		switch (iov_pktq_log->version) {
			case 6:
				if (flags & PKTQ_LOG_SCB_QUEUE) {
					PREC_COUNT_COPY6(counters, logv06, bandwidth);
					PREC_COUNT_COPY6(counters, logv06, txrate_main);
					PREC_COUNT_COPY6(counters, logv06, suppress);
					PREC_COUNT_COPY6(counters, logv06, sqsv_requested);
					PREC_COUNT_COPY6(counters, logv06, sqsv_max_used);
				} else {
					PREC_COUNT_COPY6(counters, logv06, saved);
					PREC_COUNT_COPY6(counters, logv06, selfsaved);
					PREC_COUNT_COPY6(counters, logv06, full_dropped);
					PREC_COUNT_COPY6(counters, logv06, sacrificed);
				}
				PREC_COUNT_COPY6(counters, logv06, requested);
				PREC_COUNT_COPY6(counters, logv06, stored);
				PREC_COUNT_COPY6(counters, logv06, dropped);
				PREC_COUNT_COPY6(counters, logv06, busy);
				PREC_COUNT_COPY6(counters, logv06, retry);
				PREC_COUNT_COPY6(counters, logv06, ps_retry);

				PREC_COUNT_COPY6(counters, logv06, retry_drop);
				PREC_COUNT_COPY6(counters, logv06, max_used);
				PREC_COUNT_COPY6(counters, logv06, queue_capacity);
				PREC_COUNT_COPY6(counters, logv06, rtsfail);
				PREC_COUNT_COPY6(counters, logv06, acked);
				PREC_COUNT_COPY6(counters, logv06, txrate_succ);

				PREC_COUNT_COPY6(counters, logv06, throughput);
				PREC_COUNT_COPY6(counters, logv06, time_delta);
				PREC_COUNT_COPY6(counters, logv06, airtime);

				PREC_COUNT_COPY6(counters, logv06, nss[0]);
				PREC_COUNT_COPY6(counters, logv06, nss[1]);
				PREC_COUNT_COPY6(counters, logv06, nss[2]);
				PREC_COUNT_COPY6(counters, logv06, nss[3]);
				PREC_COUNT_COPY6(counters, logv06, mcs_bitfield);
				PREC_COUNT_COPY6(counters, logv06, sgi_count);

				nss_total = counters.nss[0] + counters.nss[1] + counters.nss[2] +
					counters.nss[3];

				if (nss_total > 0) {
					int strm;
					float factor = 100.0 / (float)nss_total;
					for (strm = 0; strm < 4; strm ++) {

						if (counters.nss[strm] == 0) {
							snss[strm] = nss_blank;
							continue;
						}
						nss[strm] = factor * (float)counters.nss[strm];

						if (nss[strm] >= 99.5) {
							snss[strm] = "XX";
						} else {
							snss[strm] = lnss[strm];
							snprintf(snss[strm], PKTQLOG_STR_LEN,
								 "%2.0f", nss[strm]);
						}
					}
				}
				if (counters.mcs_bitfield) {
					uint8 mcs_l = (counters.mcs_bitfield >> 0) & 0x1F;
					uint8 mcs_m = (counters.mcs_bitfield >> 5) & 0x1F;
					uint8 mcs_h = (counters.mcs_bitfield >> 10) & 0x1F;
					snprintf(mcs, sizeof(mcs), "%2u/%2u/%2u",
						mcs_l, mcs_m, mcs_h);
				}

				if (!(flags & PKTQ_LOG_PS_QUEUE)) {
					uint8 _ac = (wme_ac >> (prec * 2)) & 0x3;
					switch (_ac) {
						case 0: ac = "BE"; break;
						case 1: ac = "BK"; break;
						case 2: ac = "VI"; break;
						case 3: ac = "VO"; break;
						default: break;
					}
				}
				break;
			case 5:
				PREC_COUNT_COPY(counters, logv05, airtime);
				PREC_COUNT_COPY(counters, logv05, requested);
				PREC_COUNT_COPY(counters, logv05, stored);
				PREC_COUNT_COPY(counters, logv05, saved);
				PREC_COUNT_COPY(counters, logv05, selfsaved);
				PREC_COUNT_COPY(counters, logv05, full_dropped);
				PREC_COUNT_COPY(counters, logv05, dropped);
				PREC_COUNT_COPY(counters, logv05, sacrificed);
				PREC_COUNT_COPY(counters, logv05, busy);
				PREC_COUNT_COPY(counters, logv05, retry);
				PREC_COUNT_COPY(counters, logv05, ps_retry);
				PREC_COUNT_COPY(counters, logv05, suppress);
				PREC_COUNT_COPY(counters, logv05, retry_drop);
				PREC_COUNT_COPY(counters, logv05, max_avail);
				PREC_COUNT_COPY(counters, logv05, max_used);
				PREC_COUNT_COPY(counters, logv05, queue_capacity);
				PREC_COUNT_COPY(counters, logv05, rtsfail);
				PREC_COUNT_COPY(counters, logv05, acked);
				PREC_COUNT_COPY(counters, logv05, txrate_succ);
				PREC_COUNT_COPY(counters, logv05, txrate_main);
				PREC_COUNT_COPY(counters, logv05, throughput);
				PREC_COUNT_COPY(counters, logv05, time_delta);
				break;
			default:
				return;
		}

		acked = counters.acked;
		try_count = counters.acked + counters.retry;
		tput = (float)counters.throughput;
		airtime = counters.airtime;

		txrate_succ = (float)((float)counters.txrate_succ / 1000.0);

		if (iov_pktq_log->version == 6) {
			txrate_succ *= (float)(PERF_LOG_RATE_FACTOR_100);

		} else if (iov_pktq_log->version == 5) {
			txrate_succ *= (float)(PERF_LOG_RATE_FACTOR_500);
		}

		if (skip_odd_prec && !(prec & 1) && (prec_mask & (1 << (prec + 1)))) {
			uint32 hi_acked;
			uint32 hi_retry;
			uint64 hi_throughput;
			uint64 hi_airtime;

			switch (iov_pktq_log->version) {
				case 6:
					hi_acked = logv06->counters[index].pktq[prec+1].acked;
					hi_retry = logv06->counters[index].pktq[prec+1].retry;
					hi_throughput =
						logv06->counters[index].pktq[prec+1].throughput;
					hi_airtime = logv06->counters[index].pktq[prec+1].airtime;
					txbw_succ = logv06->counters[index].pktq[prec+1].bandwidth;
					txsgi_succ = logv06->counters[index].pktq[prec+1].sgi_count;
					break;
				case 5:
					hi_acked = logv05->counters[index][prec+1].acked;
					hi_retry = logv05->counters[index][prec+1].retry;
					hi_throughput =
						(uint64)logv05->counters[index][prec+1].throughput;
					hi_airtime = logv05->counters[index][prec+1].airtime;
					break;
				default:
					return;
			}
			acked += hi_acked;
			try_count += hi_acked + hi_retry;
			airtime += hi_airtime;
			tput += (float)hi_throughput;
		}

		/* convert bytes to bits */
		tput *= 8.0;

		txsgi_succ += (float)counters.sgi_count;

		txbw_succ += (float)counters.bandwidth;
		if (nss_total) {
			txbw_succ /= (float)nss_total;
		} else if (acked) {
			txbw_succ /= (float)acked;
		}

		if (acked) {
			txrate_succ /= (float)acked;
			txsgi_succ = (100.0 * txsgi_succ) / (float)acked;
		} else {
			txrate_succ = 0.0;
			txsgi_succ = 0.0;
		}

		snprintf(sgi_percent, sizeof(sgi_percent), "%4.1f", txsgi_succ);
		if (!strcmp(sgi_percent, "100.0")) {
			sgi_percent[3] = 0;
		}

		if (counters.time_delta != 0) {
			if (airtime) {
				efficiency = (float)(100.0 * tput);
				efficiency /= (float)(airtime);
			}

			if (counters.txrate_succ) {
				efficiency /= txrate_succ;
				if (efficiency > 100.0) {
					efficiency = 100.0;
				}
			} else {
				efficiency = 0;
			}

			/* converts to rate of bits per us,
			   because time_delta is in micro-seconds
			*/
			tput /= (float)counters.time_delta;

			/* Calculate % airtime */
			airuse = (float)((float)airtime * 100.0 / (float)counters.time_delta);
			if (airuse > 100.0) {
				airuse = 100.0;
			}
		} else {
			tput = 0;
			efficiency = 0.0;
		}

		if (try_count) {
			txrate_main = (float)((float)counters.txrate_main / 1000.0);
			txrate_main /= (float)try_count;
		} else {
			txrate_main = 0.0;
		}
		if (iov_pktq_log->version == 6) {
			txrate_main *= (float)(PERF_LOG_RATE_FACTOR_100);

		} else if (iov_pktq_log->version == 5) {
			txrate_main *= (float)(PERF_LOG_RATE_FACTOR_500);
		}

		if (counters.retry_drop + counters.acked > 0) {
			retrydrop100 = ((float)counters.retry_drop/
					(float)(counters.retry_drop + counters.acked)) * 100.0;
			retry100 = ((float)counters.retry/
				(float)(counters.retry_drop + counters.acked)) * 100.0;
		} else {
			retrydrop100 = 0.0;
			retry100 = 0.0;
		}

		if (counters.queue_capacity > 0) {
			utilisation100 = ((float)counters.max_used/(float)counters.queue_capacity) *
				100.00;
		} else {
			utilisation100 = 0.0;
		}

#define PREC_LOG_OUT(a, ...) \
		logmask = counter_defs[(a)].usage; \
		if ((logmask & usage_part) && ((logmask & usage_full) == usage_full) && \
			((logmask & usage_exclude) == 0)) { \
			if ((a) > PREC_LOG_REQUESTED) { fputs(",", stdout); } \
				if (skip_odd_prec && counter_defs[(a)].aqm_subst && (prec & 1)) { \
				fputs(counter_defs[(a)].aqm_subst, stdout); \
				} else {\
				fprintf(stdout, counter_defs[(a)].format, __VA_ARGS__); \
			}\
		}

		PREC_LOG_OUT(PREC_LOG_PREC, prec);
		PREC_LOG_OUT(PREC_LOG_TID, prec);
		PREC_LOG_OUT(PREC_LOG_WME_AC, ac);
		PREC_LOG_OUT(PREC_LOG_REQUESTED, counters.requested);
		PREC_LOG_OUT(PREC_LOG_SQS_VREQ, counters.sqsv_requested);
		PREC_LOG_OUT(PREC_LOG_SQS_VUTIL, counters.sqsv_max_used);
		PREC_LOG_OUT(PREC_LOG_STORED, counters.stored);
		PREC_LOG_OUT(PREC_LOG_SELFSAVE, counters.selfsaved);
		PREC_LOG_OUT(PREC_LOG_SAVE, counters.saved);
		PREC_LOG_OUT(PREC_LOG_FULLDROP, counters.full_dropped);
		PREC_LOG_OUT(PREC_LOG_DROPPED, counters.dropped);
		PREC_LOG_OUT(PREC_LOG_SACRIFICED, counters.sacrificed);
		PREC_LOG_OUT(PREC_LOG_TXFAIL, counters.busy);
		PREC_LOG_OUT(PREC_LOG_RETRIED, counters.retry);
		PREC_LOG_OUT(PREC_LOG_RETRIED100, retry100);
		PREC_LOG_OUT(PREC_LOG_RTSFAIL, counters.rtsfail);
		PREC_LOG_OUT(PREC_LOG_RETRYDROP, counters.retry_drop);
		PREC_LOG_OUT(PREC_LOG_RETRYDROP100, retrydrop100);
		PREC_LOG_OUT(PREC_LOG_PSRETRY, counters.ps_retry);
		PREC_LOG_OUT(PREC_LOG_SUPPRESSED, counters.suppress);
		PREC_LOG_OUT(PREC_LOG_ACKED, counters.acked);
		PREC_LOG_OUT(PREC_LOG_AVAIL, counters.max_avail);
		PREC_LOG_OUT(PREC_LOG_UTILISATION, counters.max_used);
		PREC_LOG_OUT(PREC_LOG_UTILISATION100, utilisation100);
		PREC_LOG_OUT(PREC_LOG_QLENGTH, counters.queue_capacity);
		PREC_LOG_OUT(PREC_LOG_DATAR, tput);
		PREC_LOG_OUT(PREC_LOG_RATER, txrate_succ);
		PREC_LOG_OUT(PREC_LOG_PHYR, txrate_main);
		PREC_LOG_OUT(PREC_LOG_MCS, mcs);
		PREC_LOG_OUT(PREC_LOG_BW, txbw_succ);
		PREC_LOG_OUT(PREC_LOG_SGI, sgi_percent);
		PREC_LOG_OUT(PREC_LOG_NSS, snss[0], snss[1], snss[2], snss[3]);
		PREC_LOG_OUT(PREC_LOG_AIRUSE100, airuse);
		PREC_LOG_OUT(PREC_LOG_EFFICIENCY100, efficiency);
		fputs("\n", stdout);
	}
	if (pspretend_time != (uint32)-1) {
		fprintf(stdout, "Total time in ps pretend state is %d milliseconds\n",
			(pspretend_time + 500)/1000);
	}
	fputs("\n", stdout);
}

static void
wl_scheduler_dump(wl_iov_pktq_log_t* iov_pktq_log, uint32 index, bool hide_unknown, bool is_aqm,
	bool percent)
{
	BCM_REFERENCE(iov_pktq_log);
	BCM_REFERENCE(index);
	BCM_REFERENCE(hide_unknown);
	BCM_REFERENCE(is_aqm);
	BCM_REFERENCE(percent);
	fprintf(stdout, "scheduler\n");
}

static void
wl_mutx_dump(wl_iov_pktq_log_t* iov_pktq_log, uint32 index, bool hide_unknown, bool is_aqm,
	bool percent)
{
	enum {
		MUTX_LOG_TID, MUTX_LOG_COUNT_RATIO, MUTX_LOG_RU_BW, MUTX_LOG_RU_COUNT_RATIO,
		MUTX_MIMO_GROUP,
		NUM_MUTX_LOGS
	};

	typedef struct {
		const char* heading;
		const char* format;
	} mutx_log_definitions_t;

	const float ru_size[MAC_LOG_MU_RU_MAX] = {
		26 * 0.078125 /* MAC_LOG_MU_RU_26 */,
		52 * 0.078125 /* MAC_LOG_MU_RU_52 */,
		106 * 0.078125 /* MAC_LOG_MU_RU_106 */,
		242 * 0.078125 /* MAC_LOG_MU_RU_242 */,
		484 * 0.078125 /* MAC_LOG_MU_RU_484 */,
		996 * 0.078125 /* MAC_LOG_MU_RU_996 */,
		2 * 996 * 0.078125 /* MAC_LOG_MU_RU_2x996 */,
	};

	const mutx_log_definitions_t counter_defs[NUM_MUTX_LOGS] = {
		/* MUTX_LOG_TID */
		{ " tid:",       "  %02u:" },
		/* MUTX_LOG_COUNT_RATIO */
		{ " % su / vhm/ hem/ heo/hemo",   "  %4s,%4s,%4s,%4s,%4s" },
		/* MUTX_LOG_RU_BW */
		{ ";  RUtone+MHz",  "     %7.2f" },
		/* MUTX_LOG_RU_COUNT_RATIO */
		{ ";  %RU 2 /   4 /   8 /  20 /  40 /  80 /  160",  "    %4s, %4s, %4s, %4s, %4s, "
		"%4s, %4s" },
		/* MUTX_MIMO_GROUP */
		{ ";  %mmusr 2 /   3 /   4 ", "       %4s, %4s, %4s"},

	};

	uint32 tid;
	uint32 tid_mask = 0;
	uint32 num_tid = 0;
	uint32 flags = 0;
	pktq_log_format_v06_t* logv06 = NULL;
	bool pl_long = iov_pktq_log->params.addr_type[index] & 0x80 ? TRUE : FALSE;
	char * timestamp = NULL;

	BCM_REFERENCE(is_aqm);

	switch (iov_pktq_log->version) {
		case 6:
			logv06 = &iov_pktq_log->pktq_log.v06;
			tid_mask = logv06->counter_info[index];
			num_tid = logv06->num_prec[index];
			flags = (tid_mask & ~0xFFFF);
			timestamp = (flags & PKTQ_LOG_DISPLAY_TIMESTAMP) ?
				wl_pktqlog_timestamp(logv06->timestamp) : "";
			break;
		default:
			fprintf(stderr, "unknown/unsupported binary format (%x)\n",
				iov_pktq_log->version);
			return;
	}

	/* test for 'unknown' data; unknown means either that
	 * the queue is invalid or else that the logging
	 * is not active at all.
	 */
	if (((tid_mask & 0xFFFF) == 0) && hide_unknown) {
		return;
	}
	if ((num_tid == 0) && hide_unknown) {
		return;
	}

	/* The driver can pass back a character string for additional info */
	fprintf(stdout, "%smulti user transmit "MACF" %s\n", timestamp,
		ETHER_TO_MACF(iov_pktq_log->params.ea[index]), logv06->counters[index].heading);

	/* Note that this is zero if the data is invalid */
	if (!num_tid) {
		fprintf(stdout, "parameter %c:"MACF" not valid\n",
			iov_pktq_log->params.addr_type[index] != 0 ?
			iov_pktq_log->params.addr_type[index] & 0x7F : ' ',
			ETHER_TO_MACF(iov_pktq_log->params.ea[index]));
		return;
	}

	/* tid integer just used as a genric index here, it is not a tid ! */
	for (tid = 0; tid < NUM_MUTX_LOGS; tid++) {
		if (!pl_long && tid == MUTX_LOG_RU_BW) {
			continue;
		}
		fputs(counter_defs[tid].heading, stdout);
	}
	fprintf(stdout, "  (%s%sv%u)%s\n", iov_pktq_log->params.addr_type[index] & 0x80 ? "+":"",
		percent ? "%":"", iov_pktq_log->version,
		tid_mask & PKTQ_LOG_DEVELOPMENT_VERSION ? " D" : "");

	for (tid = 0; tid < num_tid; tid++) {
		int type;
		struct {
			uint64  total_count;
			uint64  ru_total_count;
			uint32  count[MAC_LOG_MU_MAX];
			uint32  ru_count[MAC_LOG_MU_RU_MAX];
			uint32  mimo_count[8];
			uint64  mimo_total_count;
			float   ru_BW;
		} counters;

		char count[MAC_LOG_MU_MAX][8];
		char ru_count[MAC_LOG_MU_RU_MAX][8];
		char mimo_count[MAC_LOG_MU_MIMO_USER_MAX][8];

		if (!(tid_mask & (1 << tid))) {
			continue;
		}
		memset(&counters, 0, sizeof(counters));
#define MUTXLOG_COUNT_COPY6(a, b, c)	a.c = b->counters[index].mu[tid].c

		switch (iov_pktq_log->version) {
			case 6:
				for (type = 0; type < MAC_LOG_MU_MAX; type++) {
					MUTXLOG_COUNT_COPY6(counters, logv06, count[type]);
					counters.total_count += counters.count[type];
				}
				for (type = 0; type < MAC_LOG_MU_RU_MAX; type++) {
					MUTXLOG_COUNT_COPY6(counters, logv06, ru_count[type]);
					counters.ru_total_count += counters.ru_count[type];
					counters.ru_BW += ru_size[type] * counters.ru_count[type];
				}
				for (type = 0; type < MAC_LOG_MU_MIMO_USER_MAX; type++) {
					MUTXLOG_COUNT_COPY6(counters, logv06, mimo_count[type]);
					counters.mimo_total_count += counters.mimo_count[type];
				}

				if (counters.ru_total_count > 0) {
					counters.ru_BW /= (float)counters.ru_total_count;
				}

				for (type = 0; type < MAC_LOG_MU_MAX; type++) {
					if (counters.total_count > 0 && counters.count[type] > 0) {
						float local_count = (float) counters.count[type];
						local_count *= 100.0;
						local_count /= (float)counters.total_count;
						if (local_count < 99.95) {
							snprintf(count[type], 8, "%4.1f",
								local_count);
						} else {
							snprintf(count[type], 8, "100");
						}
					} else {
						snprintf(count[type], 8, "   -");
					}
				}
				for (type = 0; type < MAC_LOG_MU_RU_MAX; type++) {
					if (counters.ru_total_count > 0 &&
							counters.ru_count[type] > 0) {
						float local_count = (float) counters.ru_count[type];
						local_count *= 100.0;
						local_count /= (float)counters.ru_total_count;
						if (local_count < 99.95) {
							snprintf(ru_count[type], 8, "%4.1f",
								local_count);
						} else {
							snprintf(ru_count[type], 8, "100");
						}
					} else {
						snprintf(ru_count[type], 8, "   -");
					}
				}
				for (type = 0; type < MAC_LOG_MU_MIMO_USER_MAX; type++) {
					if (counters.mimo_total_count > 0 &&
							counters.mimo_count[type] > 0) {
						float loc_count = (float) counters.mimo_count[type];
						loc_count *= 100.0;
						loc_count /= (float)counters.mimo_total_count;
						if (loc_count < 99.95) {
							snprintf(mimo_count[type], 8, "%4.1f",
								loc_count);
						} else {
							snprintf(mimo_count[type], 8, "100");
						}
					} else {
						snprintf(mimo_count[type], 8, "   -");
					}
				}
				break;
			default:
				return;
		}

#define MUTX_LOG_OUT(a, ...) \
		if ((a) > MUTX_LOG_COUNT_RATIO) { fputs(";", stdout); }\
		fprintf(stdout, counter_defs[(a)].format, __VA_ARGS__);

		MUTX_LOG_OUT(MUTX_LOG_TID, tid);
		MUTX_LOG_OUT(MUTX_LOG_COUNT_RATIO,
			count[0], count[1], count[2], count[3], count[4]);
		if (pl_long) {
			MUTX_LOG_OUT(MUTX_LOG_RU_BW, counters.ru_BW);
		}
		MUTX_LOG_OUT(MUTX_LOG_RU_COUNT_RATIO,
			ru_count[0], ru_count[1], ru_count[2], ru_count[3], ru_count[4],
			ru_count[5], ru_count[6]);
		MUTX_LOG_OUT(MUTX_MIMO_GROUP, mimo_count[1], mimo_count[2], mimo_count[3]);
		fputs("\n", stdout);
	}

	fputs("\n", stdout);
}

static void
wl_txq_macparam_dump(wl_iov_pktq_log_t* iov_pktq_log, bool hide_unknown, bool is_aqm, bool percent)
{
	uint32 index;
	uint32 version = iov_pktq_log->version;
	pktq_log_format_v06_t* logv06 = (version == 6) ? &iov_pktq_log->pktq_log.v06 : NULL;

	for (index = 0; index < (uint8)iov_pktq_log->params.num_addrs; index++) {
		bool supported = FALSE;

		switch (iov_pktq_log->params.addr_type[index] & 0x7F) {

			case 'D':
				if (logv06 == NULL) {
					break;
				}
				supported = TRUE;
				fprintf(stdout, MACF" logging %s\n",
					ETHER_TO_MACF(iov_pktq_log->params.ea[index]),
					logv06->num_prec[index] == 0xFF ? "freed" : "not freed");
				break;
			case 'B':
				if (logv06 == NULL) {
					break;
				}
			/* fall through */
			case 'A':
			case 'C':
			case 'N':
			case 'P':
				supported = TRUE;
				/* "pktq_stats" */
				wl_txq_prec_dump(iov_pktq_log, index, hide_unknown, is_aqm,
					percent);
				break;

			case 'M':
				if (logv06) {
					supported = TRUE;
					wl_mutx_dump(iov_pktq_log, index, hide_unknown, is_aqm,
						percent);
				}
				break;

			case 'S':
				if (logv06) {
					supported = TRUE;
					wl_scheduler_dump(iov_pktq_log, index, hide_unknown, is_aqm,
						percent);
				}
				break;

			default:
				break;
		}
		if (!supported) {
			fprintf(stdout, "prefix '%c' not supported in v%u with "MACF"\n",
				iov_pktq_log->params.addr_type[index] & 0x7F, version,
				ETHER_TO_MACF(iov_pktq_log->params.ea[index]));
		}
	}

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
	struct maclist* maclist = NULL;
	wlc_rev_info_t revinfo;
	uint32 corerev;
	bool percent = FALSE;
	bool resetstats = TRUE;
	bool dpstats = FALSE;

	if (cmd->get < 0) {
		return -1;
	}

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	corerev = dtoh32(revinfo.corerev);

	if (!strcmp(*argv, "dpstats")) {
		dpstats = TRUE;
	}

	do {
		bool  loop_assoclist = FALSE;

		memset(full_params, 0, sizeof(*full_params));
		memset(&loop_params, 0, sizeof(loop_params));
		memset(&loop_extra_params, 0, sizeof(loop_extra_params));

		/* only pass up to WL_IOV_MAC_PARAM_LEN parameters */
		while (params->num_addrs < WL_IOV_MAC_PARAM_LEN && *macaddrs) {
			bool    full_auto = FALSE;
			char*   ptr = *macaddrs;
			uint32  bitmask = 0;

			if (ptr[0] == '-') {
				++ptr;
				if (dpstats && (!strcmp(ptr, "nr") || !strcmp(ptr, "noreset"))) {
					resetstats = FALSE;
				} else if (!strcmp(ptr, "%") || !strcmp(ptr, "percent")) {
					percent = TRUE;
				} else {
					fprintf(stdout, "bad option '%s'\n", *macaddrs);
					return BCME_BADOPTION;
				}
				++macaddrs;
				continue;
			}
			if (resetstats == FALSE) {
				bitmask |= PKTQ_LOG_NO_RESET;
			}

			/* is there a prefix character? */
			if (ptr[1] == ':') {
				params->addr_type[params->num_addrs] = toupper((int)(ptr[0]));

				/* move ptr to skip over prefix */
				ptr += 2;

				/* is there the 'long form' option ? */
				if (ptr[0] == '+') {
					/* check for + additional info option, set top bit */
					params->addr_type[params->num_addrs] |= 0x80;
					ptr++;
				}

				if ((ptr[0] == 0) || (ptr[0] == '/' || ptr[0] == ',')) {
					/* this is the fully automatic mode */
					full_auto = TRUE;
				}
			}

			if ((params->addr_type[params->num_addrs] & 0x7F) == 'C')  {
				/* the prefix C: denotes no given MAC address (refer to "common") */
				full_auto = FALSE;
				if (dpstats) {
					bitmask |= PKTQ_LOG_AUTO;
				}
			} else if ((params->addr_type[params->num_addrs] & 0x7F) == 'B')  {
				/* the prefix B: denotes no given MAC address ("broadcast" queue) */
				bitmask |= PKTQ_LOG_AUTO; /* enable self-activating logging */
				full_auto = FALSE;
			} else if (full_auto) {
				loop_assoclist = TRUE;
				loop_params.addr_type[loop_params.num_addrs] =
					params->addr_type[params->num_addrs];
			} else if (wl_ether_atoe(ptr, &params->ea[params->num_addrs])) {
				/* length of MAC addr string excl end char */
				ptr += (ETHER_ADDR_STR_LEN - 1);
			} else {
				params->addr_type[params->num_addrs] = 0;
				printf("bad parameter '%s'\n", *macaddrs);
				++macaddrs;
				continue;
			}

			while (ptr && (ptr[0] == ',' || ptr[0] == '/') &&
					((ptr[1] >= '0' && ptr[1] <= '9') || ptr[1] == '/' ||
					ptr[1] == ',')) {

				uint8 prec;
				char* endptr = 0;

				if (ptr[1] == '/' || ptr[1] == ',') {
					/* this is the 'auto' setting */
					bitmask |= PKTQ_LOG_AUTO;
					ptr += 2;
				} else {
					ptr++;

					prec = (uint8)strtoul(ptr, &endptr, 10);

					if (prec <= 15) {
						bitmask |= (1 << prec);
					} else {
						printf("bad precedence %d (will be ignored)\n",
						       prec);
					}
					ptr = endptr;
				}
			}

			if ((bitmask & 0xFFFF) == 0 && !(bitmask & PKTQ_LOG_AUTO)) {
				/* PKTQ_LOG_DEF_PREC is ignored in V4, it is used to indicate no
				 * prec was selected
				 */
				bitmask |= (0xFFFF | PKTQ_LOG_DEF_PREC);
			}

			if (full_auto) {
				loop_extra_params.addr_info[loop_params.num_addrs] = bitmask;
				loop_params.num_addrs++;
			} else {
				extra_params->addr_info[params->num_addrs] = bitmask;
				params->num_addrs ++;
			}

			++macaddrs;
		}

		/* if no valid params found, pass default prefix 'C' with no mac address */
		if (params->num_addrs == 0 && !loop_assoclist && !dpstats) {
			params->addr_type[0] = 'C';
			extra_params->addr_info[0] = 0xFFFF;
			params->num_addrs = 1;
		}

		if (params->num_addrs) {
			/* set a "version" indication (ie extra_params present) */
			params->num_addrs |= ((dpstats ? 6 : 4) << 8);

			if ((ret = wlu_iovar_getbuf(wl, cmd->name, full_params,
					sizeof(*full_params), buf, WLC_IOCTL_MAXLEN)) < 0) {
				fprintf(stderr, "error getting %s (%u)\n", argv[0],
					(uint32)sizeof(wl_iov_pktq_log_t));
				return ret;
			}

			wl_txq_macparam_dump((wl_iov_pktq_log_t*)buf, FALSE, corerev >= 40,
				percent);
		}
		if (!loop_assoclist) {
			continue;
		}

		if (maclist == NULL) {
			maclist = malloc(WLC_IOCTL_MEDLEN);

			if (!maclist) {
				fprintf(stderr, "unable to allocate memory\n");
				return -ENOMEM;
			}
			maclist->count = htod32((WLC_IOCTL_MEDLEN - sizeof(int)) / ETHER_ADDR_LEN);

			if ((ret = wlu_get(wl, WLC_GET_ASSOCLIST, maclist, WLC_IOCTL_MEDLEN)) < 0) {
				fprintf(stderr, "cannot get assoclist\n");
				free(maclist);
				return ret;
			}
			maclist->count = dtoh32(maclist->count);

			if (maclist->count == 0) {
				fprintf(stderr, "no available addresses in assoclist for "
					"automatic operation\n");
				continue;
			}

		}

		for (index = 0; index < loop_params.num_addrs; index++) {
			uint32 ea_index = 0;

			while (ea_index < maclist->count) {

				memset(full_params, 0, sizeof(*full_params));

				while ((params->num_addrs < WL_IOV_MAC_PARAM_LEN) &&
						(ea_index < maclist->count)) {

					params->addr_type[params->num_addrs] =
						loop_params.addr_type[index];
					params->ea[params->num_addrs] = maclist->ea[ea_index];
					extra_params->addr_info[params->num_addrs] =
						loop_extra_params.addr_info[index] | PKTQ_LOG_AUTO;

					params->num_addrs++;
					ea_index++;
				}

				/* set a "version" indication */
				params->num_addrs |= ((dpstats ? 6 : 4) << 8);

				if ((ret = wlu_iovar_getbuf(wl, cmd->name, full_params,
						sizeof(*full_params), buf, WLC_IOCTL_MAXLEN)) < 0) {
					fprintf(stderr, "error getting %s (%u)\n", argv[0],
						(uint32)sizeof(wl_iov_pktq_log_t));
					free(maclist);
					return ret;
				}

				wl_txq_macparam_dump((wl_iov_pktq_log_t*)buf, TRUE, corerev >= 40,
					percent);
			}
		}
	} while (*macaddrs);

	if (maclist) {
		free(maclist);
	}
	return 0;
}

static void
wl_float_to_string_rounding(char *s, float f, uint64 total)
{
	if (total == 0)
		sprintf(s, "%5.5s", "-");
	else if (10*(int)f == (int)(f*10.0))
		sprintf(s, "%5.0f", f);
	else
		sprintf(s, "%5.1f", f);
}

static void
wl_scb_bs_data_convert_v2(iov_bs_data_struct_t *v2)
{
	/* This only take care of endianess between driver and application */
	int argn;
	for (argn = 0; argn < v2->structure_count; ++argn) {
		iov_bs_data_record_t *rec;
		iov_bs_data_counters_t *ctr;

		rec = &v2->structure_record[argn];
		ctr = &rec->station_counters;

		rec->station_flags = dtoh16(rec->station_flags);

#define DEVICE_TO_HOST64(xyzzy) ctr->xyzzy = dtoh64(ctr->xyzzy)
#define DEVICE_TO_HOST32(xyzzy) ctr->xyzzy = dtoh32(ctr->xyzzy)
		DEVICE_TO_HOST64(throughput);
		DEVICE_TO_HOST64(txrate_main);
		DEVICE_TO_HOST64(txrate_succ);
		DEVICE_TO_HOST32(retry_drop);
		DEVICE_TO_HOST32(rtsfail);
		DEVICE_TO_HOST32(retry);
		DEVICE_TO_HOST32(acked);
		DEVICE_TO_HOST32(ru_acked);
		DEVICE_TO_HOST32(mu_acked);
		DEVICE_TO_HOST32(time_delta);
		DEVICE_TO_HOST32(airtime);
		DEVICE_TO_HOST32(txbw);
		DEVICE_TO_HOST32(txnss);
		DEVICE_TO_HOST32(txmcs);
#undef DEVICE_TO_HOST64
#undef DEVICE_TO_HOST32
	}
}

static void
wl_scb_bs_data_convert_v1(iov_bs_data_struct_v1_t *v1, iov_bs_data_struct_t *v2)
{
	int argn;
	int max_stations;

	v2->structure_version = v1->structure_version;
	v2->structure_count = v1->structure_count;

	/* Calculating the maximum number of stations that v2 can hold  */
	max_stations = (WLC_IOCTL_MAXLEN / sizeof(iov_bs_data_struct_t)) - 1;

	for (argn = 0; (argn < v1->structure_count) &&
		(argn < max_stations); ++argn) {
		iov_bs_data_record_v1_t *rec_v1;
		iov_bs_data_counters_v1_t *ctr_v1;
		iov_bs_data_record_t *rec_v2;
		iov_bs_data_counters_t *ctr_v2;

		rec_v2 = &v2->structure_record[argn];
		ctr_v2 = &rec_v2->station_counters;

		rec_v1 = &v1->structure_record[argn];
		ctr_v1 = &rec_v1->station_counters;

		memcpy(&rec_v2->station_address, &rec_v1->station_address, ETHER_ADDR_LEN);
		rec_v2->station_flags = dtoh16(rec_v1->station_flags);

		ctr_v2->throughput = (uint64)dtoh32(ctr_v1->throughput);
		ctr_v2->txrate_main = (uint64)dtoh32(ctr_v1->txrate_main);
		ctr_v2->txrate_succ = (uint64)dtoh32(ctr_v1->txrate_succ);
		ctr_v2->txrate_succ *= (PERF_LOG_RATE_FACTOR_500 / PERF_LOG_RATE_FACTOR_100);

		ctr_v2->retry_drop = dtoh32(ctr_v1->retry_drop);
		ctr_v2->rtsfail = dtoh32(ctr_v1->rtsfail);
		ctr_v2->retry = dtoh32(ctr_v1->retry);
		ctr_v2->acked = dtoh32(ctr_v1->acked);
		ctr_v2->time_delta = dtoh32(ctr_v1->time_delta);
		ctr_v2->airtime = dtoh32(ctr_v1->airtime);

		ctr_v2->txbw = 0;
		ctr_v2->txmcs = 0;
		ctr_v2->txnss = 0;
		ctr_v2->ru_acked = 0;
		ctr_v2->mu_acked = 0;
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
	bool v1_style = FALSE;
	float total_throughput = 0.0;
	bool display_overall = TRUE;
	uint32 time_delta = 0;
	uint32 cumul_airtime = 0;
	void *p = NULL;

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
		if (!strcmp(argv[argn], "-v1")) {	/* Comma separator */
			v1_style = TRUE;
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
	data->structure_count = dtoh16(data->structure_count);

	if (data->structure_count == 0) {
		printf("No stations are currently associated.\n");
		return BCME_OK;
	}

	if (data->structure_version == SCB_BS_DATA_STRUCT_VERSION_v1) {
		/* Alloc some memory and convert all incoming v1 format
		 * into v2, and redirect data to it
		 */
		p = malloc(WLC_IOCTL_MAXLEN);
		if (!p) {
			return BCME_NOMEM;
		}
		wl_scb_bs_data_convert_v1((iov_bs_data_struct_v1_t *)data,
			(iov_bs_data_struct_t *)p);
		data = (iov_bs_data_struct_t*)p;
		v1_style = TRUE;
	} else if (data->structure_version == SCB_BS_DATA_STRUCT_VERSION) {
		wl_scb_bs_data_convert_v2(data);
	} else {
		fprintf(stderr, "wlu / wl driver mismatch, expect V%d format, got %d.\n",
			SCB_BS_DATA_STRUCT_VERSION, data->structure_version);
		return BCME_IOCTL_ERROR;
	}

	/* Display Column headers - mac address always, then, depending on display mode */
	printf("%17s%c", "Station Address", sep);
	switch (display_mode) {
	case DISPLAY_RAW:
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s "
			"%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
			"retry_drop", "rtsfail", "retry", "txrate_main",
			"txrate_succ", "acked", "throughput", "time_delta", "airtime",
			"txbw", "txmcs", "txnss", "ru", "mu");
		break;
	case DISPLAY_COOKED:
		if (v1_style) {
			printf("%10s%c%10s%c%10s%c%10s%c%10s\n", "PHY Mbps", sep, "Data Mbps", sep,
				"Air Use", sep, "Data Use", sep, "Retries");
		} else {
			printf("%10s%c%10s%c%10s%c%10s%c%10s"
				"%5s%c%5s%c%5s%c%7s%c%7s%c\n", "PHY Mbps", sep, "Data Mbps", sep,
				"Air Use", sep, "Data Use", sep, "Retries",
				"bw", sep, "mcs", sep, "Nss", sep,
				"ofdma", sep, "mu-mimo", sep);
		}
		break;
	}

	/* Sum up total throughput */
	for (argn = 0; argn < data->structure_count; ++argn) {
		iov_bs_data_record_t *rec;
		iov_bs_data_counters_t *ctr;
		float data_rate;

		rec = &data->structure_record[argn];
		ctr = &rec->station_counters;

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
			printf("%9d %9d %9d %9llu %9llu %9d %9llu %9d %9d %9d %9d %9d %9d %9d\n",
				ctr->retry_drop, ctr->rtsfail, ctr->retry,
				ctr->txrate_main, ctr->txrate_succ, ctr->acked,
				ctr->throughput, ctr->time_delta, ctr->airtime,
				ctr->txbw, ctr->txmcs, ctr->txnss,
				ctr->ru_acked, ctr->mu_acked);
			break;
		case DISPLAY_COOKED:
			{
			float data_rate;
			float phy_rate;
			float use, air, rtr;
			float p_ru = 0, p_mu = 0;

			/* Calculate PHY rate */
			phy_rate = (ctr->acked) ?
				(float)((float)ctr->txrate_succ *
				(PERF_LOG_RATE_FACTOR_100 / 1000.0) / (float)ctr->acked) : 0.0;

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

			if (v1_style) {
				printf("%10.1f%c%10.1f%c%9.1f%%%c%9.1f%%%c%9.1f%%\n",
					phy_rate, sep, data_rate, sep, air, sep, use, sep, rtr);
			} else {
				float bw = 0, mcs = 0, nss = 0;
				char st_nss[10];
				char st_mcs[10];
				char st_bw[10];

				if (ctr->acked) {
					bw = ctr->txbw*1.0/ctr->acked;
					mcs = ctr->txmcs*1.0/ctr->acked;
					nss = ctr->txnss*1.0/ctr->acked;
					p_ru = (100.0*ctr->ru_acked)/ctr->acked;
					p_mu = (100.0*ctr->mu_acked)/ctr->acked;
				}

				wl_float_to_string_rounding(st_bw, bw, (uint64)(ctr->acked));
				wl_float_to_string_rounding(st_mcs, mcs, (uint64)(ctr->acked));
				wl_float_to_string_rounding(st_nss, nss, (uint64)(ctr->acked));

				printf("%10.1f%c%10.1f%c%9.1f%%%c%9.1f%%%c%9.1f%%"
					"%s%c%s%c%s%c%6.1f%%%c%6.1f%%%c\n",
					phy_rate, sep, data_rate, sep, air, sep, use, sep, rtr,
					st_bw, sep, st_mcs, sep, st_nss, sep,
					p_ru, sep, p_mu, sep);
			}
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

	if (p) {
		free(p);
	}
	return BCME_OK;
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
	char st_tones[10];

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
			"%8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s\n",
			"tid", "rxampdu", "rxmpdu", "rxbyte", "rxphyrate",
			"rxholes", "dup", "oow", "rtries", "rxbw",
			"rxmcs", "rxnss", "ofdma", "tones", "dtime");
		break;
	case DISPLAY_COOKED:
		printf("%4s%c%7s%c%8s%c"
			"%10s%c%10s%c%5s%c%5s%c"
			"%5s%c%5s%c%5s%c"
			"%5s%c%5s%c%6s%c%6s%c%6s\n",
			"tid", sep, "ampdu", sep, "mpdu", sep,
			"Data Mbps", sep, "PHY Mbps", sep, "bw", sep, "mcs", sep,
			"Nss", sep, "oow", sep, "holes", sep,
			"dup", sep, "rtries", sep, "ofdma", sep, "tones", sep, "air");
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
			DEVICE_TO_HOST32(rxampdu_ofdma);
			DEVICE_TO_HOST32(rxtones);
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
					"%8u %8u "
					"%8u %8u %8u\n",
					tid, ctr->rxampdu, ctr->rxmpdu, ctr->rxbyte,
					ctr->rxphyrate, ctr->rxholes, ctr->rxdup,
					ctr->rxoow, ctr->rxretried, ctr->rxbw,
					ctr->rxmcs, ctr->rxnss,
					ctr->rxampdu_ofdma, ctr->rxtones, rec->time_delta);
				break;
			case DISPLAY_COOKED:
				{
				float bw = 0, nss = 0, speed = 0;
				float phyrate = 0, mcs = 0, tones = 0;
				float ratio_air = 0, ratio_ofdma = 0;

				if (ctr->rxampdu) {
					bw = ctr->rxbw/ctr->rxampdu;
					phyrate = (ctr->rxphyrate/1000.0)/ctr->rxampdu;
					mcs = ctr->rxmcs*1.0/ctr->rxampdu;
					nss = ctr->rxnss*1.0/ctr->rxampdu;
					ratio_ofdma = ctr->rxampdu_ofdma * 100.0 / ctr->rxampdu;

					if (rec->time_delta) {
						speed = (ctr->rxbyte*8.0) / rec->time_delta;
						ratio_air = speed * 100.0 / phyrate;
					}

					if (ctr->rxampdu_ofdma) {
						tones = ctr->rxtones * 1.0 / ctr->rxampdu_ofdma;
					}
				}

				wl_float_to_string_rounding(st_nss, nss, ctr->rxampdu);
				wl_float_to_string_rounding(st_mcs, mcs, ctr->rxampdu);
				wl_float_to_string_rounding(st_bw, bw, ctr->rxampdu);
				wl_float_to_string_rounding(st_tones, tones, ctr->rxampdu);

				printf("%3d%c%7d%c%8d%c"
					"%10.1f%c%10.1f%c"
					"%s%c%s%c%s%c"
					"%5d%c%5d%c%5d%c%5d%c"
					"%6.0f%%%c%6s%c%5.0f%%\n",
					tid, sep, ctr->rxampdu, sep, ctr->rxmpdu, sep,
					speed, sep, phyrate, sep,
					st_bw, sep, st_mcs, sep, st_nss, sep,
					ctr->rxoow, sep, ctr->rxholes, sep,
					ctr->rxdup, sep, ctr->rxretried, sep,
					ratio_ofdma, sep, st_tones, sep, ratio_air);
				}
				break;
			}
		}
	}

	/* Add total summary */
	if (display_overall && (display_mode == DISPLAY_COOKED)) {
		ctr = &allc;

		if (ctr->rxampdu) {
			float speed = 0;

			if (rec->time_delta) {
				speed = (ctr->rxbyte*8.0) / rec->time_delta;
			}

			printf("                 (overall)%c", sep);
			printf("%3c%c%7d%c%8d%c"
				"%10.1f%c%10c%c%5c%c%5c%c%5c%c"
				"%5c%c%5c%c%5c%c%5c%c"
				"%6c%c%6c%c%6c\n",
				'-', sep, ctr->rxampdu, sep, ctr->rxmpdu, sep,
				speed, sep, '-', sep, '-', sep, '-', sep, '-', sep,
				'-', sep, '-', sep,
				'-', sep, '-', sep,
				'-', sep, '-', sep, '-');
		}
	}

	return BCME_OK;
}
