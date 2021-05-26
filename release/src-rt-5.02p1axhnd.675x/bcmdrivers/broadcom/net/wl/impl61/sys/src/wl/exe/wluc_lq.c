/*
 * wl lq command module
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
 * $Id: wluc_lq.c 774873 2019-05-09 09:37:32Z $
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
#include <miniopt.h>

#define LQCM_REPORT_INVALID	0xFFFFFFFF

static cmd_func_t wl_rssi_event, wl_chan_qual_event;
static cmd_func_t wl_chanim_state, wl_chanim_mode;
static cmd_func_t wl_chanim_acs_record;
static cmd_func_t wl_chanim_stats;
static cmd_func_t wl_lqcm;

static cmd_t wl_lq_cmds[] = {
	{ "rssi_event", wl_rssi_event, WLC_GET_VAR, WLC_SET_VAR,
	"Set parameters associated with RSSI event notification\n"
	"\tusage: wl rssi_event <rate_limit> <rssi_levels>\n"
	"\trate_limit: Number of events posted to application will be limited"
	" to 1 per this rate limit. Set to 0 to disable rate limit.\n"
	"\trssi_levels: Variable number of RSSI levels (maximum 8) "
	" in increasing order (e.g. -85 -70 -60). An event will be posted"
	" each time the RSSI of received beacons/packets crosses a level."},
	{ "chq_event", wl_chan_qual_event, WLC_GET_VAR, WLC_SET_VAR,
	"Set parameters associated with channel quality  event notification\n"
	"\tusage: wl chq_event <rate_limit> <cca_levels> <nf_levels> <nf_lte_levels>\n"
	"\trate_limit: Number of events posted to application will be limited"
	" to 1 per this rate limit. Set to 0 to disable rate limit.\n"
	"\tcsa/nf/nf_lte levels: Variable number of threshold levels (maximum 8)"
	" in pairs of hi-to-low/lo-to-hi, and in increasing order (e.g. -90 -85 -80)."
	" A 0 0 pair terminates level array for one metric."
	" An event will be posted whenever a threshold is being crossed."},
	{"chanim_state", wl_chanim_state, WLC_GET_VAR, -1,
	"get channel interference state\n"
	"\tUsage: wl chanim_state channel\n"
	"\tValid channels: 1 - 14\n"
	"\treturns: 0 - Acceptable; 1 - Severe"
	},
	{"chanim_mode", wl_chanim_mode, WLC_GET_VAR, WLC_SET_VAR,
	"get/set channel interference measure (chanim) mode\n"
	"\tUsage: wl chanim_mode <value>\n"
	"\tvalue: 0 - disabled; 1 - detection only; 2 - detection and avoidance"
	},
	{"chanim_acs_record", wl_chanim_acs_record, WLC_GET_VAR, -1,
	"get the auto channel scan record. \n"
	"\t Usage: wl acs_record"
	},
	{"chanim_stats", wl_chanim_stats, WLC_GET_VAR, -1,
	"get chanim stats \n"
	"\t Usage: wl chanim_stats"
	},
	{"lqcm", wl_lqcm, WLC_GET_VAR, WLC_SET_VAR,
	"Controls LQCM. \n"
	"\tUsage: \n"
	"\t1. wl lqcm -e <0/1> : 0 - Disable LQCM, 1: Enable LQCM  \n"
	"\t2. wl lqcm -r       : reports LQCM index."
	},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_lq_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register lq commands */
	wl_module_cmds_register(wl_lq_cmds);
}

static int
wl_chan_qual_event(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	const char *CHAN_QUAL_NAME[WL_CHAN_QUAL_TOTAL] = {"   CCA", "    NF", "NF_LTE"};

	if (!*++argv) {
		/* get */
		void *ptr = NULL;
		wl_chan_qual_event_t chq;
		uint i, j;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		memcpy(&chq, ptr, sizeof(chq));
		chq.rate_limit_msec = dtoh32(chq.rate_limit_msec);

		printf("rate per %dms\n", chq.rate_limit_msec);
		for (i = 0; i < WL_CHAN_QUAL_TOTAL; i++) {
			printf("%s[%d]:", CHAN_QUAL_NAME[i], chq.metric[i].id);
			for (j = 0; (j < chq.metric[i].num_levels) &&
				(j < MAX_CHAN_QUAL_LEVELS); j++) {
				printf(" (%d, %d)", chq.metric[i].htol[j], chq.metric[i].ltoh[j]);
			}
			printf("\n");
		}
	} else {
		/* set */
		wl_chan_qual_event_t chq;
		uint i;

		memset(&chq, 0, sizeof(wl_chan_qual_event_t));
		chq.rate_limit_msec = atoi(*argv++);
		chq.rate_limit_msec = htod32(chq.rate_limit_msec);
		chq.num_metrics = htod16(WL_CHAN_QUAL_TOTAL);

		for (i = 0; i < WL_CHAN_QUAL_TOTAL; i++) {
			chq.metric[i].id = i;
			while (argv[0] && argv[1]) {
				int16 htol, ltoh;
				htol = htod16(atoi(*argv++));
				ltoh = htod16(atoi(*argv++));

				/* double zeros terminate one metric */
				if ((htol == 0) && (ltoh == 0))
					break;

				/* make sure that ltoh >= htol */
				if (ltoh < htol)
					return -1;

				/* ignore extra thresholds */
				if (chq.metric[i].num_levels >= MAX_CHAN_QUAL_LEVELS)
					continue;

				chq.metric[i].htol[chq.metric[i].num_levels] = htol;
				chq.metric[i].ltoh[chq.metric[i].num_levels] = ltoh;

				/* all metric threshold levels must be in increasing order */
				if (chq.metric[i].num_levels > 0) {
					if ((chq.metric[i].htol[chq.metric[i].num_levels] <=
						chq.metric[i].htol[chq.metric[i].num_levels - 1]) ||
					    (chq.metric[i].ltoh[chq.metric[i].num_levels] <=
						chq.metric[i].ltoh[chq.metric[i].num_levels - 1])) {
						return -1;
					}
				}

				(chq.metric[i].num_levels)++;
			}
		}

		if (*argv) {
			/* too many parameters */
			return -1;
		}

		ret = wlu_var_setbuf(wl, cmd->name, &chq, sizeof(chq));
	}
	return ret;
}

static int
wl_rssi_event(void *wl, cmd_t *cmd, char **argv)
{
	int ret;

	if (!*++argv) {
		/* get */
		void *ptr = NULL;
		wl_rssi_event_t rssi;
		uint i;

		if ((ret = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return ret;

		memcpy(&rssi, ptr, sizeof(rssi));
		rssi.rate_limit_msec = dtoh32(rssi.rate_limit_msec);

		printf("%d", rssi.rate_limit_msec);
		for (i = 0; i < rssi.num_rssi_levels; i++) {
			printf(" %d", rssi.rssi_levels[i]);
		}
		printf("\n");
	} else {
		/* set */
		wl_rssi_event_t rssi;

		memset(&rssi, 0, sizeof(wl_rssi_event_t));
		rssi.rate_limit_msec = atoi(*argv);

		while (*++argv && rssi.num_rssi_levels < MAX_RSSI_LEVELS) {
			rssi.rssi_levels[rssi.num_rssi_levels++] = atoi(*argv);
			if (rssi.num_rssi_levels > 1) {
				if (rssi.rssi_levels[rssi.num_rssi_levels - 1] <=
					rssi.rssi_levels[rssi.num_rssi_levels - 2]) {
					/* rssi levels must be in increasing order */
					return BCME_USAGE_ERROR;
				}
			}
		}

		if (*argv) {
			/* too many parameters */
			return BCME_USAGE_ERROR;
		}

		rssi.rate_limit_msec = htod32(rssi.rate_limit_msec);
		ret = wlu_var_setbuf(wl, cmd->name, &rssi, sizeof(rssi));
	}
	return ret;
}

static int
wl_chanim_state(void *wl, cmd_t *cmd, char **argv)
{
	uint32 chanspec;
	int argc = 0;
	int ret, val;

	argv++;

	/* find the arg count */
	while (argv[argc])
		argc++;

	if (argc != 1)
		return BCME_USAGE_ERROR;

	chanspec = wf_chspec_aton(*argv);
	chanspec = wl_chspec32_to_driver(chanspec);
	if (chanspec == INVCHANSPEC) {
		return BCME_USAGE_ERROR;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, &chanspec, sizeof(chanspec),
	                       buf, WLC_IOCTL_SMLEN);
	if (ret < 0)
		return ret;
	val = *(int*)buf;
	val = dtoh32(val);

	printf("%d\n", val);
	return 0;
}

static int
wl_chanim_mode(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr;
	int mode;

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_iovar_getint(wl, cmd->name, &mode)) < 0)
			return ret;

		switch (mode) {
		case CHANIM_DISABLE:
			printf("CHANIM mode: disabled.\n");
			break;
		case CHANIM_DETECT:
			printf("CHANIM mode: detect only.\n");
			break;
		case CHANIM_EXT:
			printf("CHANIM mode: external (acsd).\n");
			break;
		case CHANIM_ACT:
			printf("CHANIM mode: detect + act.\n");
			break;
		}
		return 0;
	} else {
		mode = CHANIM_DETECT;
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return BCME_USAGE_ERROR;

		switch (val) {
			case 0:
				mode = CHANIM_DISABLE;
				break;
			case 1:
				mode = CHANIM_DETECT;
				break;
			case 2:
				mode = CHANIM_EXT;
				break;
			case 3:
				mode = CHANIM_ACT;
				break;
			default:
				return BCME_BADARG;
		}

		mode = htod32(mode);
		return wlu_iovar_setint(wl, cmd->name, mode);
	}
}

static int
wl_chanim_acs_record(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err = 0, i;
	wl_acs_record_t *result;

	/* need to add to this str if new acs trigger type is added */
	const char *trig_str[] = {"None", "IOCTL", "CHANIM", "TIMER", "BTA"};

	UNUSED_PARAMETER(argv);

	if ((err = wlu_var_getbuf(wl, cmd->name, NULL, 0, &ptr)) < 0)
		return err;

	result = (wl_acs_record_t *) ptr;

	if (!result->count) {
		printf("There is no ACS recorded\n");
		return err;
	}

	printf("current timestamp: %u (ms)\n", result->timestamp);

	printf("Timestamp(ms)  ACS Trigger  Selected Channel  Glitch Count  CCA Count\n");
	for (i = 0; i < result->count; i++) {
		uint8 idx = CHANIM_ACS_RECORD - result->count + i;
		chanim_acs_record_t * record = &result->acs_record[idx];

		record->selected_chspc = wl_chspec_from_driver(record->selected_chspc);

		printf("%10u \t%s \t%10d \t%12d \t%8d\n", record->timestamp,
		   trig_str[record->trigger], wf_chspec_ctlchan(record->selected_chspc),
		   record->glitch_cnt, record->ccastats);
	}
	return err;
}

static void
wl_chanim_stats_us_print(void *ptr, uint32 req_count, int version)
{
	wl_chanim_stats_us_t *list;
	wl_chanim_stats_us_v1_t *list_v1;
	wl_chanim_stats_us_v2_t *list_v2;
	chanim_stats_us_t *stats_us;
	chanim_stats_us_v1_t *stats_us_v1;
	chanim_stats_us_v2_t *stats_us_v2;
	uint32  buflen;
	uint32  count;
	uint32  us_ver;

	if (version < WL_CHANIM_STATS_VERSION_3) {
		list = (wl_chanim_stats_us_t*)ptr;
		buflen = dtoh32(list->buflen);
		count = dtoh32(list->count);
		list_v1 = NULL;
		us_ver = 0;
	} else {
		list_v1 = (wl_chanim_stats_us_v1_t*)ptr;
		us_ver = dtoh32(list_v1->version);
		buflen = dtoh32(list_v1->buflen);
		count = dtoh32(list_v1->count);
		if (us_ver == WL_CHANIM_STATS_US_VERSION_2)
			list_v2 = (wl_chanim_stats_us_v2_t*)ptr;
		list = NULL;
	}

	printf("version: %d \n", version);
	if (version == WL_CHANIM_STATS_VERSION_3) {
		printf("us version: %d \n", us_ver);
	}

	if (count == WL_CHANIM_COUNT_US_RESET) {
		printf("Reset done\n");
		return;
	}
	if (buflen == 0) {
		count = 0;
		printf("Sorry, your program received a incorrect buffer lenght\n");
		return;
	}
	if ((count == 1) && ((req_count == WL_CHANIM_COUNT_US_ONE) ||
			(req_count == WL_CHANIM_US_DUR_GET))) {
		if (version < WL_CHANIM_STATS_VERSION_3) {
			stats_us = list->stats_us;
			printf("%-8s  %-8s  %-8s  %-8s  %-8s  %-8s  %-8s  %-8s  %-8s\n",
					"chanspec", "tx", "inbss", "obss", "total_tm", "busy_tm",
					"pri20", "sec20", "sec40");
			printf("0x%-6x  ", dtoh16(stats_us->chanspec));
			printf("%-8u  %-8u  %-8u  %-8u  %-8u  %-8u  %-8u  %-8u\n",
					dtoh32(stats_us->tx_tm),
					dtoh32(stats_us->rx_bss),
					dtoh32(stats_us->rx_obss),
					dtoh32(stats_us->total_tm),
					dtoh32(stats_us->busy_tm),
					dtoh32(stats_us->rxcrs_pri20),
					dtoh32(stats_us->rxcrs_sec20),
					dtoh32(stats_us->rxcrs_sec40));
		} else {
			unsigned int i;
			printf("%-8s  %-8s  %-8s  %-8s  %-8s  %-8s  %-8s  %-8s  %-8s"
					"  %-8s  %-8s  %-8s  %-8s  %-8s  %-8s\n",
					"chanspec", "tx", "inbss", "obss", "nocat", "nopkt",
					"doze", "txop", "goodtx", "badtx",  "total_tm", "busy_tm",
					"pri20", "sec20", "sec40");
			if (us_ver == WL_CHANIM_STATS_US_VERSION_1) {
				stats_us_v1 = list_v1->stats_us_v1;
				printf("0x%-6x  ", dtoh16(stats_us_v1->chanspec));

				for (i = 0; i < CCASTATS_MAX; i++) {
					printf("%-8u  ", dtoh32(stats_us_v1->ccastats_us[i]));
				}
				printf("%-8u  %-8u  %-8u  %-8u  %-8u\n",
						dtoh32(stats_us_v1->total_tm),
						dtoh32(stats_us_v1->busy_tm),
						dtoh32(stats_us_v1->rxcrs_pri20),
						dtoh32(stats_us_v1->rxcrs_sec20),
						dtoh32(stats_us_v1->rxcrs_sec40));
			} else if (us_ver == WL_CHANIM_STATS_US_VERSION_2) {
				stats_us_v2 = list_v2->stats_us_v2;
				printf("0x%-6x  ", dtoh16(stats_us_v2->chanspec));

				for (i = 0; i < CCASTATS_MAX; i++) {
					printf("%-8llu  ", dtoh64(stats_us_v2->ccastats_us[i]));
				}
				printf("%-8llu  %-8llu  %-8llu  %-8llu  %-8llu\n",
						dtoh64(stats_us_v2->total_tm),
						dtoh64(stats_us_v2->busy_tm),
						dtoh64(stats_us_v2->rxcrs_pri20),
						dtoh64(stats_us_v2->rxcrs_sec20),
						dtoh64(stats_us_v2->rxcrs_sec40));
			} else {
				printf("Sorry, your driver has wl_chanim_stats version %d "
					"but this program supports version upto %d.\n",
					us_ver, WL_CHANIM_STATS_US_VERSION);
			}
		}

	} else if (count >= 1 && req_count == WL_CHANIM_COUNT_US_ALL) {
		unsigned int i, j;
		printf("CHAN Interference Measurement:\n");
		printf("Stats during last scan:\n");
		if (version <  WL_CHANIM_STATS_VERSION_3) {
			for (i = 0; i < count; i++) {
				printf("chanspec: 0x%4x total_time: %u busy_time: %u rx_obss: %u\n",
					dtoh16(list->stats_us[i].chanspec),
					dtoh32(list->stats_us[i].total_tm),
					dtoh32(list->stats_us[i].rxcrs_pri20),
					dtoh32(list->stats_us[i].rx_obss));
			}
			return;
		}
		for (i = 0; i < count; i++) {
			printf("%-10s  %-10s  %-10s  %-10s  %-10s  %-10s  %-10s  %-10s"
				"  %-10s  %-10s  %-10s  %-10s\n",
				"chanspec", "tx", "inbss", "obss", "nocat", "nopkt",
				"doze", "txop", "goodtx", "badtx",  "total_tm", "busy_tm");
			if (us_ver == WL_CHANIM_STATS_US_VERSION_1) {
				printf("0x%-8x  ",
					dtoh16(list_v1->stats_us_v1[i].chanspec));

				for (j = 0; j < CCASTATS_MAX; j++) {
					printf("%-10u  ",
						dtoh32(list_v1->stats_us_v1[i].ccastats_us[j]));
				}
				printf("%-10u  %-10u\n",
					dtoh32(list_v1->stats_us_v1[i].total_tm),
					dtoh32(list_v1->stats_us_v1[i].rxcrs_pri20));
			} else if (us_ver == WL_CHANIM_STATS_US_VERSION_2) {
				printf("0x%-8x  ", dtoh16(list_v2->stats_us_v2[i].chanspec));

				for (j = 0; j < CCASTATS_MAX; j++) {
					printf("%-10llu  ",
						dtoh64(list_v2->stats_us_v2[i].ccastats_us[j]));
				}
				printf("%-10llu  %-10llu\n",
					dtoh64(list_v2->stats_us_v2[i].total_tm),
					dtoh64(list_v2->stats_us_v2[i].rxcrs_pri20));
			} else {
				printf("Sorry, your driver has wl_chanim_stats version %d "
					"but this program supports version upto %d.\n",
					us_ver, WL_CHANIM_STATS_US_VERSION);
			}
		}
	}
	printf("\n");
}

static void wl_chanim_stats_print_all(wl_chanim_stats_t *list)
{
	unsigned int i;
	printf("CHAN Interference Measurement:\n");
	printf("Stats during last scan:\n");
	for (i = 0; i < list->count; i++) {
		printf(" chanspec: 0x%x crsglitch cnt: %d bad plcp: %d noise: %d\n",
			dtoh16(list->stats[i].chanspec), dtoh32(list->stats[i].glitchcnt),
			dtoh32(list->stats[i].badplcp), list->stats[i].bgnoise);

		printf("\t cca_txdur: %d cca_inbss: %d cca_obss:"
			"%d cca_nocat: %d cca_nopkt: %d\n",
			list->stats[i].ccastats[CCASTATS_TXDUR],
			list->stats[i].ccastats[CCASTATS_INBSS],
			list->stats[i].ccastats[CCASTATS_OBSS],
			list->stats[i].ccastats[CCASTATS_NOCTG],
			list->stats[i].ccastats[CCASTATS_NOPKT]);
	}

}

static void
wl_chanim_stats_print(void *ptr, uint32 req_count)
{
	wl_chanim_stats_t *list;
	unsigned int i;

	list = (wl_chanim_stats_t*)ptr;

	list->buflen = dtoh32(list->buflen);
	list->version = dtoh32(list->version);
	list->count = dtoh32(list->count);

	printf("version: %d \n", list->version);

	if (list->buflen == 0) {
		list->version = 0;
		list->count = 0;
		printf("Sorry, your program received a incorrect buffer lenght\n");
		return;
	}
	if (list->version == WL_CHANIM_STATS_VERSION_3) {
		if (list->count == 1 && req_count == WL_CHANIM_COUNT_ONE) {
			chanim_stats_t *stats_v3 = (chanim_stats_t *)list->stats;

			printf("chanspec tx   inbss   obss   nocat   nopkt   doze     txop     "
				"goodtx  badtx   glitch   badplcp  knoise  idle  timestamp\n");
			printf("0x%4x\t", dtoh16(stats_v3->chanspec));
			for (i = 0; i < CCASTATS_MAX; i++) {
				printf("%d\t", stats_v3->ccastats[i]);
			}
			printf("%d\t%d\t%d\t%d\t%d", dtoh32(stats_v3->glitchcnt),
				dtoh32(stats_v3->badplcp),
				stats_v3->bgnoise, stats_v3->chan_idle,
				dtoh32(stats_v3->timestamp));
		} else if (list->count >= 1 && req_count == WL_CHANIM_COUNT_ALL) {
			wl_chanim_stats_print_all(list);
		}
	} else if (list->version == WL_CHANIM_STATS_VERSION_2) {
		if (list->count == 1 && req_count == WL_CHANIM_COUNT_ONE) {
			chanim_stats_v2_t *stats_v2 = (chanim_stats_v2_t *)list->stats;

			printf("chanspec tx   inbss   obss   nocat   nopkt   doze     txop     "
				"goodtx  badtx   glitch   badplcp  knoise  idle  timestamp\n");
			printf("0x%4x\t", dtoh16(stats_v2->chanspec));
			for (i = 0; i < CCASTATS_V2_MAX; i++) {
				printf("%d\t", stats_v2->ccastats[i]);
			}
			printf("%d\t%d\t%d\t%d\t%d", dtoh32(stats_v2->glitchcnt),
				dtoh32(stats_v2->badplcp), stats_v2->bgnoise,
				stats_v2->chan_idle, dtoh32(stats_v2->timestamp));
			printf("\n");
		} else if (list->count >= 1 && req_count == WL_CHANIM_COUNT_ALL) {
			wl_chanim_stats_print_all(list);
		}
	}
	printf("\n");
}

static int wl_chanim_stats_us_dur(void *wl, cmd_t *cmd, char **argv, int version)
{
	int err;
	wl_chanim_stats_us_v1_t param_us;
	void *ptr;

	argv++;
	if (*argv != NULL) {
		param_us.dur = atoi(*argv);
		param_us.count = htod32(WL_CHANIM_US_DUR);
	} else {
		param_us.count = htod32(WL_CHANIM_US_DUR_GET);
	}

	param_us.buflen = htod32(sizeof(wl_chanim_stats_us_v2_t) +
			sizeof(chanim_stats_us_v2_t));
	if ((err = wlu_var_getbuf(wl, cmd->name, &param_us, sizeof(wl_chanim_stats_us_v2_t),
		&ptr)) < 0) {
		printf("failed to get chanim results\n");
	}
	wl_chanim_stats_us_print(ptr, param_us.count, version);
	return err;

}
static int
wl_chanim_stats(void *wl, cmd_t *cmd, char **argv)
{
	wl_chanim_stats_t param;
	wl_chanim_stats_t *stats;
	int stats_size;
	int  err;
	void *ptr;

	argv++;
	if (*argv != NULL) {
		if (!strcmp(*argv, "us")) {
			wl_chanim_stats_t *list;
			int version;
			param.count = WL_CHANIM_READ_VERSION;
			param.buflen = htod32(sizeof(wl_chanim_stats_t));
			if ((err = wlu_var_getbuf(wl, cmd->name, &param,
				sizeof(wl_chanim_stats_t), &ptr)) < 0) {
				printf("failed to get chanim results");
				return err;
			}

			list = (wl_chanim_stats_t*)ptr;
			version = dtoh32(list->version);

			argv++;
			if (*argv != NULL) {
				if (!strcmp(*argv, "all")) {
					param.count = htod32(WL_CHANIM_COUNT_US_ALL);
					param.buflen = htod32(WL_CHANIM_BUF_LEN);
				} else if (!strcmp(*argv, "reset")) {
					param.count = htod32(WL_CHANIM_COUNT_US_RESET);
					param.buflen = 0;
				} else if (!strcmp(*argv, "dur")) {
					if (version == WL_CHANIM_STATS_VERSION_3) {
						err = wl_chanim_stats_us_dur(wl, cmd,
								argv, version);
						return err;
					}
				} else {
					printf("Invalid option\n");
					return 0;
				}
			} else {
				param.count = htod32(WL_CHANIM_COUNT_US_ONE);
				param.buflen = htod32(sizeof(wl_chanim_stats_us_v2_t) +
						sizeof(chanim_stats_us_v2_t));
			}
			if ((err = wlu_var_getbuf(wl, cmd->name, &param,
					sizeof(wl_chanim_stats_us_v2_t), &ptr)) < 0) {
				printf("failed to get chanim results\n");
				return err;
			}

			wl_chanim_stats_us_print(ptr, param.count, version);
			return 0;
		} else if (!strcmp(*argv, "all")) {
			param.buflen = htod32(WL_CHANIM_BUF_LEN);
			param.count = htod32(WL_CHANIM_COUNT_ALL);
		} else {
			printf("Invalid option\n");
			return 0;
		}
	} else {
		param.buflen = htod32(sizeof(wl_chanim_stats_t));
		param.count = htod32(WL_CHANIM_COUNT_ONE);
	}

	/* get fw chanim stats version */
	stats_size = WL_CHANIM_STATS_FIXED_LEN +
		MAX(sizeof(chanim_stats_t), sizeof(chanim_stats_v2_t));
	stats = (wl_chanim_stats_t *)malloc(stats_size);
	if (stats == NULL) {
		fprintf(stderr, "memory alloc failure\n");
		return BCME_NOMEM;
	}
	memset(stats, 0, stats_size);
	stats->buflen = htod32(stats_size);
	if ((err = wlu_var_getbuf(wl, cmd->name, stats, stats_size, &ptr)) < 0) {
		printf("failed to get chanim results");
		free(stats);
		return err;
	}
	memcpy(stats, ptr, stats_size);
	stats->version = dtoh32(stats->version);
	if (!((stats->version == WL_CHANIM_STATS_VERSION) ||
		(stats->version == WL_CHANIM_STATS_VERSION_2))) {
		printf("Sorry, your driver has wl_chanim_stats version %d "
			"but this program supports only version %d and %d.\n",
			stats->version, WL_CHANIM_STATS_VERSION_2, WL_CHANIM_STATS_VERSION);
		free(stats);
		return 0;
	}

	/* get fw chanim stats */
	if ((err = wlu_var_getbuf(wl, cmd->name, &param, stats_size, &ptr)) < 0) {
		printf("failed to get chanim results");
		free(stats);
		return err;
	}

	wl_chanim_stats_print(ptr, param.count);
	free(stats);
	return (err);
}

static int
wl_lqcm(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	int err = 0, opt_err, lqcm_report_req = 0;
	const char* fn_name = "wl_lqcm";
	uint32 lqcm_report = LQCM_REPORT_INVALID;

	/* toss the command name */
	argv++;

	miniopt_init(&to, fn_name, "r", FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = BCME_USAGE_ERROR;
			return err;
		}
		argv += to.consumed;

		if (to.opt == 'e') {
			if ((!to.good_int) || ((to.val < 0) || (to.val > 1))) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an boolean"
					" for enable/disable option \n",
					fn_name, to.valstr);

				err = BCME_BADARG;
				return err;
			}
			lqcm_report = to.val;
		} else if (to.opt == 'r') {
			lqcm_report_req = 1;
		} else {
			err = BCME_USAGE_ERROR;
			return err;
		}
	}

	if ((lqcm_report != LQCM_REPORT_INVALID) && (lqcm_report_req)) {
		err = BCME_USAGE_ERROR;
		return err;
	}

	if (lqcm_report != LQCM_REPORT_INVALID) {
		err = wlu_var_setbuf(wl, cmd->name, &lqcm_report, sizeof(uint32));
		return err;
	} else if (lqcm_report_req == 0) {
		/* Report the index, if no argument is passed. */
		lqcm_report_req = 1;
	}

	/* lqcm_report is a packed 32 bit in the following manner
	* bit<7:0> --> LQCM enable/disable
	* bit<15:8> --> LQCM Tx Index
	* bit<23:16> --> LQCM Rx Index
	*/

	if (lqcm_report_req == 1) {
		if ((err = wlu_iovar_get(wl, cmd->name, &lqcm_report, sizeof(uint32))) < 0)
			return err;
		if ((lqcm_report & 1) == 0)
			printf("LQCM is in OFF state.\n");
		else {
			printf("LQCM uplink index   :%03d \n", ((lqcm_report >> 8) & 0xff));
			printf("LQCM downlink index :%03d \n", ((lqcm_report >> 16) & 0xff));
		}
	}

	return err;
}
