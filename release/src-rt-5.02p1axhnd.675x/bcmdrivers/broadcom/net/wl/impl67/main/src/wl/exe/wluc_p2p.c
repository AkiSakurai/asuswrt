/*
 * wl p2p command module
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
 * $Id: wluc_p2p.c 786051 2020-04-15 05:35:40Z $
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

#ifdef LINUX
#include <time.h>
#endif /* LINUX */

static cmd_func_t wl_p2p_state;
static cmd_func_t wl_p2p_scan;
static cmd_func_t wl_p2p_ifadd;
static cmd_func_t wl_p2p_ifdel;
static cmd_func_t wl_p2p_ifupd;
static cmd_func_t wl_p2p_if;
static cmd_func_t wl_p2p_ops;
static cmd_func_t wl_p2p_noa;
static cmd_func_t wl_p2p_conf;

static cmd_t wl_p2p_cmds[] = {
	{ "p2p_ssid", wl_ssid, WLC_GET_VAR, WLC_SET_VAR,
	"set WiFi P2P wildcard ssid.\n"
	"\tUsage: wl p2p_ssid <ssid>"
	},
	{ "p2p_state", wl_p2p_state, -1, WLC_SET_VAR,
	"set WiFi P2P discovery state.\n"
	"\tUsage: wl p2p_state <state> [<chanspec> <dwell time>]"
	},
	{ "p2p_scan", wl_p2p_scan, -1, WLC_SET_VAR,
	"initiate WiFi P2P scan.\n"
	"\tUsage: wl p2p_scan S|E <scan parms>\n"
	SCAN_USAGE
	},
	{ "p2p_ifadd", wl_p2p_ifadd, -1, WLC_SET_VAR,
	"add WiFi P2P interface\n"
	"\tUsage: wl p2p_ifadd <MAC-address> go|client|dyngo [chanspec]\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_ifdel", wl_p2p_ifdel, -1, WLC_SET_VAR,
	"delete WiFi P2P interface\n"
	"\tUsage: wl p2p_ifdel <MAC-address>\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_ifupd", wl_p2p_ifupd, -1, WLC_SET_VAR,
	"update an interface to WiFi P2P interface\n"
	"\tUsage: wl p2p_ifupd <MAC-address> go|client\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_if", wl_p2p_if, WLC_GET_VAR, -1,
	"query WiFi P2P interface bsscfg index\n"
	"\tUsage: wl p2p_if <MAC-address>\n"
	"MAC-address: xx:xx:xx:xx:xx:xx"
	},
	{ "p2p_noa", wl_p2p_noa, WLC_GET_VAR, WLC_SET_VAR,
	"set/get WiFi P2P NoA schedule\n"
	"\tUsage: wl p2p_noa <type> <type-specific-params>\n"
	"\t\ttype 0: Scheduled Absence (on GO): <type> <action> <action-specific-params>\n"
	"\t\t\taction -1: Cancel the schedule: <type> <action>\n"
	"\t\t\taction 0,1,2: <type> <action> <option> <option-specific-params>\n"
	"\t\t\t\taction 0: Do nothing during absence periods\n"
	"\t\t\t\taction 1: Sleep during absence periods\n"
	"\t\t\t\toption 0: <start:tsf> <interval> <duration> <count> ...\n"
	"\t\t\t\toption 1 [<start-percentage>] <duration-percentage>\n"
	"\t\t\t\toption 2 <start:tsf-offset> <interval> <duration> <count>\n"
	"\t\ttype 1: Requested Absence (on GO): \n"
	"\t\t\taction -1: Cancel the schedule: <type> <action>\n"
	"\t\t\taction 2: <type> <action> <option> <option-specific-params>\n"
	"\t\t\t\taction 2: Turn off GO beacons and probe responses during absence period\n"
	"\t\t\t\toption 2 <start:tsf-offset> <interval> <duration> <count>"
	},
	{ "p2p_ops", wl_p2p_ops, WLC_GET_VAR, WLC_SET_VAR,
	"set/get WiFi P2P OppPS and CTWindow\n"
	"\tUsage: wl p2p_ops <ops> [<ctw>]\n"
	"\t\t<ops>:\n"
	"\t\t\t0: Disable OppPS\n"
	"\t\t\t1: Enable OppPS\n"
	"\t\t<ctw>:\n"
	"\t\t\t10 and up to beacon interval"
	},
	{ "p2p_da_override", wl_iov_mac, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set WiFi P2P device interface addr\n"
	"\tUsage: wl p2p_da_override <MAC-address>\n"
	"MAC-address: xx:xx:xx:xx:xx:xx\n"
	"(When MAC-address is set to 00:00:00:00:00:00, default da restored)"
	},
	{ "p2p_conf", wl_p2p_conf, -1, WLC_SET_VAR,
	"enable/disable WiFi P2P device GO configuration\n"
	"\tUsage: wl p2p_conf <0|1> <channel> <ssid_name>\n"
	"\t0 - Disable P2P GO configuration\n"
	"\t1 - Enable P2P GO configuration\n"
	"\tchannel - channel to configure on P2P GO\n"
	"\tssid - ssid name to configure with GO"
	},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_p2p_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register p2p commands */
	wl_module_cmds_register(wl_p2p_cmds);
}

static int
wl_p2p_state(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_disc_st_t st;
	int count;
	char *endptr;

	argv++;

	count = ARGCNT(argv);
	if (count < 1)
		return BCME_USAGE_ERROR;

	st.state = (uint8) strtol(argv[0], &endptr, 0);
	if (st.state == WL_P2P_DISC_ST_LISTEN) {
		if (count != 3)
			return BCME_USAGE_ERROR;
		if ((st.chspec = wf_chspec_aton(argv[1])) == 0) {
			fprintf(stderr, "error parsing chanspec arg \"%s\"\n", argv[1]);
			return BCME_BADARG;
		}
		st.chspec = wl_chspec_to_driver(st.chspec);
		if (st.chspec == INVCHANSPEC) {
			return BCME_USAGE_ERROR;
		}
		st.dwell = (uint16) strtol(argv[2], &endptr, 0);
	}

	return wlu_var_setbuf(wl, cmd->name, &st, sizeof(st));
}

static int
wl_p2p_scan(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_scan_t *params = NULL;
	int params_size = 0;
	int malloc_size = 0;
	int sparams_size = 0;
	int err = 0;
	wl_scan_version_t *ver;
	void *ptr;
	uint16 version = 0;
	wl_scan_parse_param_t scan_parse_params;

	/* get the scan version to decide which scan_version to use */
	if ((err = wlu_var_getbuf(wl, "scan_ver", NULL,
	       0, &ptr)) == BCME_OK) {
		if (ptr != NULL) {
			ver = (wl_scan_version_t *)ptr;
			if (ver->scan_ver_major == WL_SCAN_VERSION_MAJOR_V2) {
				version = WL_SCAN_VERSION_MAJOR_V2;
			}
		}
	}

	if (*(argv + 1) != NULL) {
		malloc_size = sizeof(wl_p2p_scan_t);
		switch (toupper(**(argv + 1))) {
			case 'S':
				if (version == WL_SCAN_VERSION_MAJOR_V2) {
					malloc_size += WL_SCAN_PARAMS_FIXED_SIZE_V2 +
						WL_NUMCHANNELS * sizeof(uint16);
				} else {
					malloc_size += WL_SCAN_PARAMS_FIXED_SIZE_V1 +
						WL_NUMCHANNELS * sizeof(uint16);
				}
				break;
			case 'E':
				if (version == WL_SCAN_VERSION_MAJOR_V2) {
					malloc_size += OFFSETOF(wl_escan_params_v2_t, params) +
						WL_SCAN_PARAMS_FIXED_SIZE_V2 +
						WL_NUMCHANNELS * sizeof(uint16);
				} else {
					malloc_size += OFFSETOF(wl_escan_params_v1_t, params) +
						WL_SCAN_PARAMS_FIXED_SIZE_V1 +
						WL_NUMCHANNELS * sizeof(uint16);
				}
			break;
		}
	}
	if (malloc_size == 0) {
		fprintf(stderr, "wrong syntax, need 'S' or 'E'\n");
		return BCME_BADARG;
	}

	malloc_size += WL_SCAN_PARAMS_SSID_MAX * sizeof(wlc_ssid_t);
	params = (wl_p2p_scan_t *)malloc(malloc_size);
	if (params == NULL) {
		fprintf(stderr, "Error allocating %d bytes for scan params\n", malloc_size);
		return BCME_NOMEM;
	}
	memset(params, 0, malloc_size);
	memset(&scan_parse_params, 0, sizeof(struct wl_scan_parse_param));
	/* Initialize scan_parse_params */
	wl_scan_init_scan_parse_params(&scan_parse_params);

	/* parse argv and populate scan params */
	if ((err = wl_scan_parse_params(wl, cmd, argv + 1, &sparams_size,
		&scan_parse_params, version)) != BCME_OK) {
		goto exit;
	}
	switch (toupper(**(argv + 1))) {
	case 'S': {
		void *sparams = (params+1);
		sparams_size = malloc_size - sizeof(wl_p2p_scan_t);

		params->type = 'S';

		if (version == WL_SCAN_VERSION_MAJOR_V2) {
			if ((err = wl_scan_prep(wl, cmd, argv + 1, (wl_scan_params_v2_t *)sparams,
				&sparams_size, &scan_parse_params, version)) == 0) {
				params_size = sizeof(wl_p2p_scan_t) + sparams_size;
			}
		} else {
			if ((err = wl_scan_prep(wl, cmd, argv + 1, (wl_scan_params_v1_t *)sparams,
				&sparams_size, &scan_parse_params, version)) == 0) {
				params_size = sizeof(wl_p2p_scan_t) + sparams_size;
			}
		}

		break;
	}

	case 'E': {
		void *eparams = (params+1);

		params->type = 'E';

		if (version == WL_SCAN_VERSION_MAJOR_V2) {
			sparams_size = malloc_size - sizeof(wl_p2p_scan_t) -
				sizeof(wl_escan_params_v2_t);
			((wl_escan_params_v2_t *)eparams)->version = htod32(ESCAN_REQ_VERSION_V2);
			((wl_escan_params_v2_t *)eparams)->action = htod16(WL_SCAN_ACTION_START);

#if defined(linux)
			srand((unsigned)time(NULL));
			((wl_escan_params_v2_t *)eparams)->sync_id = htod16(rand() & 0xffff);
#else
			((wl_escan_params_v2_t *)eparams)->sync_id = htod16(4321);
#endif /* #if defined(linux) */

			if ((err = wl_scan_prep(wl, cmd, argv + 1,
				(&((wl_escan_params_v2_t *)eparams)->params), &sparams_size,
				&scan_parse_params, version)) == 0) {
				params_size = sizeof(wl_p2p_scan_t) + sizeof(wl_escan_params_v2_t) +
					sparams_size;
			}
		} else {
			((wl_escan_params_v1_t *)eparams)->version = htod32(ESCAN_REQ_VERSION_V1);
			((wl_escan_params_v1_t *)eparams)->action = htod16(WL_SCAN_ACTION_START);

#if defined(linux)
			srand((unsigned)time(NULL));
			((wl_escan_params_v1_t *)eparams)->sync_id = htod16(rand() & 0xffff);
#else
			((wl_escan_params_v1_t *)eparams)->sync_id = htod16(4321);
#endif /* #if defined(linux) */

			if ((err = wl_scan_prep(wl, cmd, argv + 1,
				(&((wl_escan_params_v1_t *)eparams)->params), &sparams_size,
				&scan_parse_params, version)) == 0) {
				params_size = sizeof(wl_p2p_scan_t) + sizeof(wl_escan_params_v1_t) +
					sparams_size;
			}
		}
		break;
		  }
	}

	if (!err)
		err = wlu_iovar_setbuf(wl, cmd->name, params, params_size, buf, WLC_IOCTL_MAXLEN);
exit:
	free(params);
	return err;
}

static int
wl_p2p_ifadd(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_if_t ifreq;
	int count;

	argv++;

	count = ARGCNT(argv);
	if (count < 2)
		return BCME_USAGE_ERROR;

	if (!wl_ether_atoe(argv[0], &ifreq.addr))
		return BCME_USAGE_ERROR;

	if (stricmp(argv[1], "go") == 0)
		ifreq.type = WL_P2P_IF_GO;
	else if (stricmp(argv[1], "client") == 0)
		ifreq.type = WL_P2P_IF_CLIENT;
	else if (stricmp(argv[1], "dyngo") == 0)
		ifreq.type = WL_P2P_IF_DYNBCN_GO;
	else
		return BCME_USAGE_ERROR;

	if (ifreq.type == WL_P2P_IF_GO || ifreq.type == WL_P2P_IF_DYNBCN_GO) {
		if (count > 2) {
			if ((ifreq.chspec = wf_chspec_aton(argv[2])) == 0) {
				fprintf(stderr, "error parsing chanspec arg \"%s\"\n", argv[2]);
				return BCME_BADARG;
			}
			ifreq.chspec = wl_chspec_to_driver(ifreq.chspec);
			if (ifreq.chspec == INVCHANSPEC) {
				return BCME_BADARG;
			}
		}
		else
			ifreq.chspec = 0;
	}

	return wlu_var_setbuf(wl, cmd->name, &ifreq, sizeof(ifreq));
}

static int
wl_p2p_ifdel(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr addr;
	int count;

	argv++;

	count = ARGCNT(argv);
	if (count != 1)
		return BCME_USAGE_ERROR;

	if (!wl_ether_atoe(argv[0], &addr))
		return BCME_USAGE_ERROR;

	return wlu_var_setbuf(wl, cmd->name, &addr, sizeof(addr));
}

static int
wl_p2p_ifupd(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_if_t ifreq;
	int count;
	int ret;
	int bsscfg_idx = 0;
	int consumed = 0;

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((ret = wl_cfg_option(argv, cmd->name, &bsscfg_idx, &consumed)) != 0)
		return ret;
	argv += consumed;
	if (consumed == 0)
		bsscfg_idx = -1;

	count = ARGCNT(argv);
	if (count < 2)
		return BCME_USAGE_ERROR;

	if (!wl_ether_atoe(argv[0], &ifreq.addr))
		return BCME_USAGE_ERROR;

	if (stricmp(argv[1], "go") == 0)
		ifreq.type = WL_P2P_IF_GO;
	else if (stricmp(argv[1], "client") == 0)
		ifreq.type = WL_P2P_IF_CLIENT;
	else
		return BCME_USAGE_ERROR;

	ifreq.chspec = 0;

	if (bsscfg_idx == -1)
		return wlu_var_setbuf(wl, cmd->name, &ifreq, sizeof(ifreq));
	return wlu_bssiovar_setbuf(wl, cmd->name, bsscfg_idx,
	                          &ifreq, sizeof(ifreq),
	                          buf, WLC_IOCTL_MAXLEN);
}

static int
wl_p2p_if(void *wl, cmd_t *cmd, char **argv)
{
	struct ether_addr addr;
	int count;
	wl_p2p_ifq_t *ptr;
	int err;

	argv++;

	count = ARGCNT(argv);
	if (count != 1)
		return BCME_USAGE_ERROR;

	if (!wl_ether_atoe(argv[0], &addr))
		return BCME_USAGE_ERROR;

	err = wlu_var_getbuf(wl, cmd->name, &addr, sizeof(addr), (void*)&ptr);
	if (err >= 0)
		printf("%u %s\n", dtoh32(ptr->bsscfgidx), (ptr->ifname));

	return err;
}

static int
wl_p2p_ops(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_ops_t ops;
	int count;
	char *endptr;

	argv++;

	count = ARGCNT(argv);
	if (count < 1) {
		wl_p2p_ops_t *ops;
		int err;

		err = wlu_var_getbuf(wl, cmd->name, NULL, 0, (void *)&ops);
		if (err != BCME_OK) {
			fprintf(stderr, "%s: error %d\n", cmd->name, err);
			return err;
		}

		printf("ops: %u ctw: %u\n", ops->ops, ops->ctw);

		return BCME_OK;
	}

	ops.ops = (uint8) strtol(argv[0], &endptr, 0);
	if (ops.ops != 0) {
		if (count != 2)
			return BCME_USAGE_ERROR;
		ops.ctw = (uint8) strtol(argv[1], &endptr, 0);
	}
	else
		ops.ctw = 0;

	return wlu_var_setbuf(wl, cmd->name, &ops, sizeof(ops));
}

static int
wl_p2p_noa(void *wl, cmd_t *cmd, char **argv)
{
	int count;
	wl_p2p_sched_t *noa;
	int len;
	int i;
	char *endptr;

	argv ++;

	strcpy(buf, cmd->name);

	count = ARGCNT(argv);
	if (count < 2) {
		int err = wlu_get(wl, WLC_GET_VAR, buf, WLC_IOCTL_MAXLEN);
		wl_p2p_sched_t *sched;
		int i;

		if (err != BCME_OK) {
			fprintf(stderr, "%s: error %d\n", cmd->name, err);
			return err;
		}

		sched = (wl_p2p_sched_t *)buf;
		for (i = 0; i < 16; i ++) {
			if (sched->desc[i].count == 0)
				break;
			printf("start: %u interval: %u duration: %u count: %u\n",
			       sched->desc[i].start, sched->desc[i].interval,
			       sched->desc[i].duration, sched->desc[i].count);
		}

		return BCME_OK;
	}

	len = strlen(buf);

	noa = (wl_p2p_sched_t *)&buf[len + 1];
	len += 1;

	noa->type = (uint8)strtol(argv[0], &endptr, 0);
	len += sizeof(noa->type);
	noa->action = (uint8)strtol(argv[1], &endptr, 0);
	len += sizeof(noa->action);

	argv += 2;
	count -= 2;

	/* action == -1 is to cancel the current schedule */
	if (noa->action == WL_P2P_SCHED_ACTION_RESET) {
		/* the fixed portion of wl_p2p_sched_t with action == WL_P2P_SCHED_ACTION_RESET
		 * is required to cancel the curret schedule.
		 */
		len += (char *)&noa->desc[0] - ((char *)buf + len);
	}
	/* Take care of any special cases only and let all other cases fall through
	 * as normal 'start/interval/duration/count' descriptions.
	 * All cases start with 'type' 'action' 'option'.
	 * Any count value greater than 255 is to repeat unlimited.
	 */
	else {
		switch (noa->type) {
		case WL_P2P_SCHED_TYPE_ABS:
		case WL_P2P_SCHED_TYPE_REQ_ABS:
			if (count < 1)
				return BCME_USAGE_ERROR;
			noa->option = (uint8)strtol(argv[0], &endptr, 0);
			len += sizeof(noa->option);
			argv += 1;
			count -= 1;
			break;
		}

		/* add any paddings before desc field */
		len += (char *)&noa->desc[0] - ((char *)buf + len);

		switch (noa->type) {
		case WL_P2P_SCHED_TYPE_ABS:
			switch (noa->option) {
			case WL_P2P_SCHED_OPTION_BCNPCT:
				if (count == 1) {
					noa->desc[0].duration = htod32(strtol(argv[0], &endptr, 0));
					noa->desc[0].start = 100 - noa->desc[0].duration;
				}
				else if (count == 2) {
					noa->desc[0].start = htod32(strtol(argv[0], &endptr, 0));
					noa->desc[0].duration = htod32(strtol(argv[1], &endptr, 0));
				}
				else {
					fprintf(stderr, "Usage: wl p2p_noa 0 %d 1 "
					        "<start-pct> <duration-pct>\n",
					        noa->action);
					return BCME_USAGE_ERROR;
				}
				len += sizeof(wl_p2p_sched_desc_t);
				break;

			default:
				if (count < 4 || (count % 4) != 0) {
					fprintf(stderr, "Usage: wl p2p_noa 0 %d 0 "
					        "<start> <interval> <duration> <count> ...\n",
					        noa->action);
					return BCME_USAGE_ERROR;
				}
				goto normal;
			}
			break;

		default:
			if (count != 4) {
				fprintf(stderr, "Usage: wl p2p_noa 1 %d "
				        "<start> <interval> <duration> <count> ...\n",
				        noa->action);
				return BCME_USAGE_ERROR;
			}
			/* fall through... */
		normal:
			for (i = 0; i < count; i += 4) {
				noa->desc[i / 4].start = htod32(strtoul(argv[i], &endptr, 0));
				noa->desc[i / 4].interval = htod32(strtol(argv[i + 1], &endptr, 0));
				noa->desc[i / 4].duration = htod32(strtol(argv[i + 2], &endptr, 0));
				noa->desc[i / 4].count = htod32(strtol(argv[i + 3], &endptr, 0));
				len += sizeof(wl_p2p_sched_desc_t);
			}
			break;
		}
	}

	return wlu_set(wl, WLC_SET_VAR, buf, len);
}

static int
wl_p2p_conf(void *wl, cmd_t *cmd, char **argv)
{
	wl_p2p_config_params_t p2p_conf;
	int count;
	char *endptr;

	argv++;

	count = ARGCNT(argv);
	if (count < 1)
		return BCME_USAGE_ERROR;

	p2p_conf.enable = (uint16) strtol(argv[0], &endptr, 0);

	if (p2p_conf.enable == WL_P2P_ENABLE_CONF) {
		if (count != 3)
			return BCME_USAGE_ERROR;
		if ((p2p_conf.chanspec = wf_chspec_aton(argv[1])) == 0) {
			fprintf(stderr, "error parsing chanspec arg \"%s\"\n", argv[1]);
			return BCME_BADARG;
		}
		if (strlen(argv[2]) > DOT11_MAX_SSID_LEN) {
			fprintf(stderr, "SSID arg \"%s\" must be 32 chars or less\n", argv[2]);
			return BCME_BADARG;
		}
		p2p_conf.ssid.SSID_len = strlen(argv[2]);
		memcpy(p2p_conf.ssid.SSID, argv[2], p2p_conf.ssid.SSID_len);
	}
	else if (p2p_conf.enable == WL_P2P_DISABLE_CONF) {
		if (count != 1)
			return BCME_USAGE_ERROR;
	}
	return wlu_var_setbuf(wl, cmd->name, &p2p_conf, sizeof(p2p_conf));
}
