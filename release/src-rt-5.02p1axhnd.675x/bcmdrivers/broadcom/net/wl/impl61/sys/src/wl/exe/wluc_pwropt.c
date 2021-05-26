/*
 * wl power optimization feature related module
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
 * $Id$
 */

#include <wlioctl.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <sys/stat.h>
#include <errno.h>

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_bcntrim_stats;
static cmd_func_t wl_bcntrim_cfg;
static cmd_func_t wl_bcntrim_status;
static cmd_func_t wl_ops_cfg;
static cmd_func_t wl_ops_status;
static cmd_func_t wl_nap_status;

static cmd_t wl_pwropt_cmds[] = {
	{ "bcntrim_stats", wl_bcntrim_stats, WLC_GET_VAR, -1,
	"Get Beacon Trim Statistics\n"
	"\tUsage: wl bcntrim_stats\n"},
	{ "bcntrim_cfg", wl_bcntrim_cfg, WLC_GET_VAR, WLC_SET_VAR,
	"bcntrim_cfg subcommands:\n"
	"\tbcntrim_cfg phy_rate_thresh [<rate> (0 thru 48 in 500kbps unit)]\n"
	"\t\tdefault is 12, when set to 0, no phy rate limit to disable  bcntrim\n"
	"\tbcntrim_cfg override_disable_mask [<mask> (default is 0)]\n"
	"\tbcntrim_cfg tsf_drift_limit [<drift> (default is 2300 usec)]\n"},
	{ "bcntrim_status", wl_bcntrim_status, WLC_GET_VAR, -1,
	"Get Beacon Trim Status\n"
	"\tUsage: wl bcntrim_status [reset]\n"},
	{ "ops_cfg", wl_ops_cfg, WLC_GET_VAR, WLC_SET_VAR,
	"ops_cfg subcommands:\n"
	"\tops_cfg enable [<bits> (0 thru 0xF)]\n"
	"\t\tdefault is 0xF, when set to 0, disables ops]\n"
	"\tops_cfg max_dur [<val> (default is 12500) ( 512 thru 12500)]\n"
	"\tops_cfg reset [<val> (bitmap of slices; 0 reset all slices)]\n"},
	{ "ops_status", wl_ops_status, WLC_GET_VAR, -1,
	"\tGet OPS status information\n"
	"\tUsage: wl ops_status\n"},
	{ "nap_status", wl_nap_status, WLC_GET_VAR, -1,
	"\tGet napping status information\n"
	"\tUsage: wl nap_status\n"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_pwropt_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register bcntrim commands */
	wl_module_cmds_register(wl_pwropt_cmds);
}
/*
 * Get Beacon Trim Stats
 *	wl bcntrim_stats
 */
static int
wl_bcntrim_stats(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint16 op_buffer[BCNTRIM_STATS_MAX];

	UNUSED_PARAMETER(cmd);

	argv++;

	if (*argv == NULL) {
		if ((err = wlu_iovar_get(wl, cmd->name, (void *) op_buffer,
			(BCNTRIM_STATS_MAX * 2))) < 0)
			return (err);

		printf("- Beacon Trim Statistics -\n");
		printf("\tBeacons seen: %d\n",  dtoh16(op_buffer[2]));
		printf("\tTrimming considered: %d\n", dtoh16(op_buffer[0]));
		printf("\tTrimmed: %d\n",  dtoh16(op_buffer[1]));
		printf("\tReasons for not trimming:\n");
		printf("\t\tTIM element not found: %d\n",  dtoh16(op_buffer[3]));
		printf("\t\tBeacon length change: %d\n",  dtoh16(op_buffer[4]));
		printf("\t\tExceed TSF Drift: %d\n",  dtoh16(op_buffer[5]));
		printf("\t\tTIM bit set: %d\n",  dtoh16(op_buffer[6]));
		printf("\t\tWake: %d\n",  dtoh16(op_buffer[7]));
		printf("\t\tSSID len change: %d\n",  dtoh16(op_buffer[8]));
		printf("\t\tDTIM change: %d\n",  dtoh16(op_buffer[9]));
	} else {
		/* Set not supported */
		return USAGE_ERROR;
	}
	return err;
}

static int
wl_bcntrim_cfg(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	char *subcmd;
	int subcmd_len;
	uint cfg_hdr_len;

	/* skip iovar */
	argv++;

	/* must have subcommand */
	subcmd = *argv++;
	if (!subcmd) {
		return BCME_USAGE_ERROR;
	}
	subcmd_len = strlen(subcmd);
	cfg_hdr_len = OFFSETOF(wl_bcntrim_cfg_v1_t, data);

	/* GET has 0 parameter */
	if (!*argv) {
		uint8 buffer[cfg_hdr_len];
		wl_bcntrim_cfg_v1_t *bcntrim_cfg = (wl_bcntrim_cfg_v1_t *)buffer;

		memset(bcntrim_cfg, 0, cfg_hdr_len);
		if (!strncmp(subcmd, "phy_rate_thresh", subcmd_len)) {
			bcntrim_cfg->subcmd_id = WL_BCNTRIM_CFG_SUBCMD_PHY_RATE_THRESH;
		} else if (!strncmp(subcmd, "override_disable_mask", subcmd_len)) {
			bcntrim_cfg->subcmd_id = WL_BCNTRIM_CFG_SUBCMD_OVERRIDE_DISABLE_MASK;
		} else if (!strncmp(subcmd, "tsf_drift_limit", subcmd_len)) {
			bcntrim_cfg->subcmd_id = WL_BCNTRIM_CFG_SUBCMD_TSF_DRIFT_LIMIT;
		} else {
			return BCME_USAGE_ERROR;
		}
		/* invoke GET iovar */
		bcntrim_cfg->version = WL_BCNTRIM_CFG_VERSION_1;
		bcntrim_cfg->version = htod16(bcntrim_cfg->version);
		bcntrim_cfg->subcmd_id = htod16(bcntrim_cfg->subcmd_id);
		bcntrim_cfg->len = htod16(cfg_hdr_len);
		if ((err = wlu_iovar_getbuf(wl, cmd->name, bcntrim_cfg, cfg_hdr_len,
		                            buf, WLC_IOCTL_SMLEN)) < 0) {
			return err;
		}
		/* process and print GET results */
		bcntrim_cfg = (wl_bcntrim_cfg_v1_t *)buf;
		bcntrim_cfg->version = dtoh16(bcntrim_cfg->version);
		bcntrim_cfg->subcmd_id = dtoh16(bcntrim_cfg->subcmd_id);
		bcntrim_cfg->len = dtoh16(bcntrim_cfg->len);
		if (bcntrim_cfg->version != WL_BCNTRIM_CFG_VERSION_1) {
			fprintf(stderr,
			        "Invalid bcntrim_cfg version %d\n", bcntrim_cfg->version);
			err = BCME_VERSION;
			return err;
		}

		switch (bcntrim_cfg->subcmd_id) {
		case WL_BCNTRIM_CFG_SUBCMD_PHY_RATE_THRESH:
		{
			wl_bcntrim_cfg_phy_rate_thresh_t *phy_thresh =
			    (wl_bcntrim_cfg_phy_rate_thresh_t *)bcntrim_cfg->data;
			if (bcntrim_cfg->len >= cfg_hdr_len + sizeof(*phy_thresh)) {
				printf("phy_rate_thresh %d (in 500Kbps)\n",
				       phy_thresh->rate);
			} else {
				err = BCME_BADLEN;
			}
			break;
		}
		case WL_BCNTRIM_CFG_SUBCMD_OVERRIDE_DISABLE_MASK:
		{
			wl_bcntrim_cfg_override_disable_mask_t *override_disable =
				(wl_bcntrim_cfg_override_disable_mask_t *)bcntrim_cfg->data;
			if (bcntrim_cfg->len >= cfg_hdr_len + sizeof(*override_disable)) {
				printf("override_disable_mask: 0x%x\n",
				       dtoh32(override_disable->mask));
			} else {
				err = BCME_BADLEN;
			}
				break;
		}
		case WL_BCNTRIM_CFG_SUBCMD_TSF_DRIFT_LIMIT:
		{
			wl_bcntrim_cfg_tsf_drift_limit_t *bcntrim_tsf =
				(wl_bcntrim_cfg_tsf_drift_limit_t *)bcntrim_cfg->data;
			if (bcntrim_cfg->len >= cfg_hdr_len + sizeof(*bcntrim_tsf)) {
				printf("tsf_drift_limit: %d(usec)\n",
				       dtoh16(bcntrim_tsf->drift));
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
		wl_bcntrim_cfg_v1_t *bcntrim_cfg = (wl_bcntrim_cfg_v1_t *)buf;
		int len;
		char *endptr = NULL;
		uint val = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			return BCME_USAGE_ERROR;
		}

		if (!strncmp(subcmd, "phy_rate_thresh", subcmd_len)) {
			wl_bcntrim_cfg_phy_rate_thresh_t *rate_thresh =
				(wl_bcntrim_cfg_phy_rate_thresh_t *)bcntrim_cfg->data;
			if (val > BCNTRIM_MAX_PHY_RATE) {
			fprintf(stderr,
				    "error: %s, allowed phy rate  0 -- %d (in 500Kbps)\n",
					argv[0], BCNTRIM_MAX_PHY_RATE);
				return BCME_RANGE;
			}
			bcntrim_cfg->subcmd_id = WL_BCNTRIM_CFG_SUBCMD_PHY_RATE_THRESH;
			bcntrim_cfg->len = cfg_hdr_len + sizeof(*rate_thresh);
			rate_thresh->rate = htod32(val);
		} else if (!strncmp(subcmd, "override_disable_mask", subcmd_len)) {
			uint override_mask = val & WL_BCNTRIM_OVERRIDE_DISABLE_MASK;
			wl_bcntrim_cfg_override_disable_mask_t *override_disable =
				(wl_bcntrim_cfg_override_disable_mask_t *)bcntrim_cfg->data;
			if (!val || (val && override_mask)) {
				/* currently supported override reasons */
				bcntrim_cfg->subcmd_id =
				        WL_BCNTRIM_CFG_SUBCMD_OVERRIDE_DISABLE_MASK;
				bcntrim_cfg->len = cfg_hdr_len + sizeof(*override_disable);
				override_disable->mask = htod32(override_mask);
			} else {
				return BCME_EPERM;
			}
		} else if (!strncmp(subcmd, "tsf_drift_limit", subcmd_len)) {
			wl_bcntrim_cfg_tsf_drift_limit_t *bcntrim_tsf =
				(wl_bcntrim_cfg_tsf_drift_limit_t *)bcntrim_cfg->data;
			if (val > BCNTRIM_MAX_TSF_DRIFT) {
				fprintf(stderr,
				        "error: %s, allowed tsf drift  0 -- %d \n",
				        argv[0], BCNTRIM_MAX_TSF_DRIFT);
				return BCME_RANGE;
			}
			bcntrim_cfg->subcmd_id = WL_BCNTRIM_CFG_SUBCMD_TSF_DRIFT_LIMIT;
			bcntrim_cfg->len = cfg_hdr_len + sizeof(*bcntrim_tsf);
			bcntrim_tsf->drift =  (uint16)val;
			bcntrim_tsf->drift =  htod16(bcntrim_tsf->drift);
		} else {
			return BCME_USAGE_ERROR;
		}
		/* invoke SET iovar */
		len = bcntrim_cfg->len;
		bcntrim_cfg->version = htod16(WL_BCNTRIM_CFG_VERSION_1);
		bcntrim_cfg->subcmd_id = htod16(bcntrim_cfg->subcmd_id);
		bcntrim_cfg->len = htod16(bcntrim_cfg->len);

		err = wlu_iovar_set(wl, cmd->name, bcntrim_cfg, len);
	}
	return err;
}

static const char *bcntrim_status_fw_state_str_tbl[] = {
	"HOST",
	"PHY_RATE",
	"QUIET_IE",
	"QBSSLOAD_IE",
};

static void
wl_bcntrim_status_fw_disable_dump(uint32 fw_state)
{
	uint i, n;
	bool first = TRUE;
	printf("FW Disable Reason : 0x%04x (", fw_state);
	if (fw_state == 0) {
		printf("%s)\n", "NONE");
		return;
	}
	n = MIN(ARRAYSIZE(bcntrim_status_fw_state_str_tbl), NBITS(fw_state));
	for (i = 0; i < n; i++) {
		if (fw_state & (0x1 << (i))) {
			printf("%s%s", first ? "": ", ", bcntrim_status_fw_state_str_tbl[i]);
			first = FALSE;
		}
	}
	printf(")\n");
}

static int
wl_bcntrim_status(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	const char *reset = "reset";
	wl_bcntrim_status_query_v1_t status_query;
	wl_bcntrim_status_v1_t *reply;

	bzero(&status_query, sizeof(status_query));
	status_query.version = htod16(WL_BCNTRIM_STATUS_VERSION_1);
	status_query.len = sizeof(status_query);
	status_query.len = htod16(status_query.len);

	/* skip iovar and check for reset */
	argv++;
	if (*argv && strlen(*argv) == strlen(reset)) {
		if (!strncmp(*argv, reset, strlen(*argv)))
			status_query.reset = 1;
	}
	/* invoke GET iovar */
	ret = wlu_iovar_getbuf(wl, cmd->name, &status_query, sizeof(status_query),
	                       buf, WLC_IOCTL_MEDLEN);
	if (ret != BCME_OK)
		return ret;

	/* process and print GET results */
	reply = (wl_bcntrim_status_v1_t *)buf;
	reply->version = dtoh16(reply->version);
	reply->len = dtoh16(reply->len);
	if (reply->version != WL_BCNTRIM_STATUS_VERSION_1) {
		fprintf(stderr,
		        "Invalid bcntrim_status version %d\n", reply->version);
		ret = BCME_VERSION;
		return ret;
	}
	if (reply->len >= (OFFSETOF(wl_bcntrim_status_v1_t, data) +
	                   (sizeof(uint16)*BCNTRIM_STATS_MAX))) {
		printf("Current Slice index: %d\n", reply->curr_slice_id);
		wl_bcntrim_status_fw_disable_dump(dtoh32(reply->fw_status));
		printf("Applied config: %d\n", dtoh32(reply->applied_cfg));
		printf("Disable duration(ms) : %d\n", dtoh32(reply->total_disable_dur));
		printf("\tBeacons seen: %d\n",  dtoh32(reply->data[2]));
		printf("\tTrimming considered: %d\n", dtoh32(reply->data[0]));
		printf("\tTrimmed: %d\n",  dtoh32(reply->data[1]));
		printf("Reasons for not trimming:\n");
		printf("\tTIM element not found: %d\n",  dtoh32(reply->data[3]));
		printf("\tBeacon length change: %d\n",  dtoh32(reply->data[4]));
		printf("\tExceed TSF Drift: %d\n",  dtoh32(reply->data[5]));
		printf("\tTIM bit set: %d\n",  dtoh32(reply->data[6]));
		printf("\tWake: %d\n",  dtoh32(reply->data[7]));
		printf("\tSSID len change: %d\n",  dtoh32(reply->data[8]));
		printf("\tDTIM change: %d\n",  dtoh32(reply->data[9]));
	} else {
		ret = BCME_BADLEN;
	}
	return ret;
}

static int
wl_ops_cfg(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	char *subcmd;
	int subcmd_len;
	uint cfg_hdr_len;

	/* skip iovar */
	argv++;

	/* must have subcommand */
	subcmd = *argv++;
	if (!subcmd) {
		return BCME_USAGE_ERROR;
	}
	subcmd_len = strlen(subcmd);
	cfg_hdr_len = OFFSETOF(wl_ops_cfg_v1_t, data);

	/* GET has 0 parameter */
	if (!*argv) {

		uint8 buffer[cfg_hdr_len];
		wl_ops_cfg_v1_t *ops_cfg = (wl_ops_cfg_v1_t *)buffer;

		memset(ops_cfg, 0, cfg_hdr_len);
		if (!strncmp(subcmd, "enable", subcmd_len)) {
			ops_cfg->subcmd_id = WL_OPS_CFG_SUBCMD_ENABLE;
		} else if (!strncmp(subcmd, "max_dur", subcmd_len)) {
			ops_cfg->subcmd_id = WL_OPS_CFG_SUBCMD_MAX_SLEEP_DUR;
		} else {
			return BCME_USAGE_ERROR;
		}

		/* invoke GET iovar */
		ops_cfg->version = WL_OPS_CFG_VERSION_1;
		ops_cfg->version = htod16(ops_cfg->version);
		ops_cfg->subcmd_id = htod16(ops_cfg->subcmd_id);
		ops_cfg->len = htod16(cfg_hdr_len);
		if ((err = wlu_iovar_getbuf(wl, cmd->name, ops_cfg, cfg_hdr_len,
		                            buf, WLC_IOCTL_SMLEN)) < 0) {
			return err;
		}

		/* process and print GET results */
		ops_cfg = (wl_ops_cfg_v1_t *)buf;
		ops_cfg->version = dtoh16(ops_cfg->version);
		ops_cfg->subcmd_id = dtoh16(ops_cfg->subcmd_id);
		ops_cfg->len = dtoh16(ops_cfg->len);

		if (ops_cfg->version != WL_OPS_CFG_VERSION_1) {
			fprintf(stderr,
			        "Invalid ops_cfg version %d\n", ops_cfg->version);
			err = BCME_VERSION;
			return err;
		}

		switch (ops_cfg->subcmd_id) {
		case WL_OPS_CFG_SUBCMD_ENABLE:
		{
			wl_ops_cfg_enable_t *enable_cfg =
			    (wl_ops_cfg_enable_t *)ops_cfg->data;
			if (ops_cfg->len >= cfg_hdr_len + sizeof(*enable_cfg)) {
				printf("config (0x%x), FW cap (0x%x)\n",
				       (dtoh32(enable_cfg->bits) & WL_OPS_CFG_MASK),
				       ((dtoh32(enable_cfg->bits) & WL_OPS_CFG_CAP_MASK)
				       >> WL_OPS_CFG_CAP_SHIFT));
			} else {
				fprintf(stderr, "ops_cfg enable: short len %u < %u\n",
					ops_cfg->len, (cfg_hdr_len + (uint)sizeof(*enable_cfg)));
				err = BCME_BADLEN;
			}
			break;
		}
		case WL_OPS_CFG_SUBCMD_MAX_SLEEP_DUR:
		{
			wl_ops_cfg_max_sleep_dur_t *max_sleep =
				(wl_ops_cfg_max_sleep_dur_t *)ops_cfg->data;
			if (ops_cfg->len >= cfg_hdr_len + sizeof(*max_sleep)) {
				printf("max sleep dur %u(us)\n", dtoh32(max_sleep->val));
			} else {
				fprintf(stderr, "ops_cfg max_dur: short len %u < %u\n",
					ops_cfg->len, (cfg_hdr_len + (uint)sizeof(*max_sleep)));
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
		wl_ops_cfg_v1_t *ops_cfg = (wl_ops_cfg_v1_t *)buf;
		int len;
		char *endptr = NULL;
		uint val = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0') {
			return BCME_USAGE_ERROR;
		}

		if (!strncmp(subcmd, "enable", subcmd_len)) {
			wl_ops_cfg_enable_t *enable_cfg =
			    (wl_ops_cfg_enable_t *)ops_cfg->data;
			if (val & ~WL_OPS_SUPPORTED_CFG) {
				fprintf(stderr, "error: %s, supported bits 0x%x\n",
				        argv[0], WL_OPS_SUPPORTED_CFG);
				return BCME_RANGE;
			}
			ops_cfg->subcmd_id = WL_OPS_CFG_SUBCMD_ENABLE;
			ops_cfg->len = cfg_hdr_len + sizeof(*enable_cfg);
			enable_cfg->bits = htod32(val);
		} else if (!strncmp(subcmd, "max_dur", subcmd_len)) {
			wl_ops_cfg_max_sleep_dur_t *max_dur =
				(wl_ops_cfg_max_sleep_dur_t *)ops_cfg->data;
			if (val < WL_OPS_MINOF_MAX_SLEEP_DUR || val > WL_OPS_MAX_SLEEP_DUR) {
				fprintf(stderr,
				        "error: %s, allowed max sleep dur %u -- %u (us)\n",
						argv[0], WL_OPS_MINOF_MAX_SLEEP_DUR,
				        WL_OPS_MAX_SLEEP_DUR);
				return BCME_RANGE;
			}
			ops_cfg->subcmd_id = WL_OPS_CFG_SUBCMD_MAX_SLEEP_DUR;
			ops_cfg->len = cfg_hdr_len + sizeof(*max_dur);
			max_dur->val = htod32(val);
		} else if (!strncmp(subcmd, "reset", subcmd_len)) {
			wl_ops_cfg_reset_stats_t *reset =
				(wl_ops_cfg_reset_stats_t *)ops_cfg->data;
			ops_cfg->subcmd_id = WL_OPS_CFG_SUBCMD_RESET_STATS;
			ops_cfg->len = cfg_hdr_len + sizeof(*reset);
			reset->val = htod32(val);
		} else {
			return BCME_USAGE_ERROR;
		}

		/* invoke SET iovar */
		len = ops_cfg->len;
		ops_cfg->version = htod16(WL_OPS_CFG_VERSION_1);
		ops_cfg->subcmd_id = htod16(ops_cfg->subcmd_id);
		ops_cfg->len = htod16(ops_cfg->len);

		err = wlu_iovar_set(wl, cmd->name, ops_cfg, len);
	}
	return err;
}

static const char *ops_status_fw_state_str_tbl[] = {
	"HOST",
	"UNASSOC",
	"SCAN",
	"BCN_MISS",
};

static void
wl_ops_status_fw_disable_dump(uint32 fw_state)
{
	uint i, n;
	bool first = TRUE;

	printf("FW Disable Reason : 0x%04x (", fw_state);
	if (fw_state == 0) {
		printf("%s)\n", "NONE");
		return;
	}
	n = MIN(ARRAYSIZE(ops_status_fw_state_str_tbl), NBITS(fw_state));
	for (i = 0; i < n; i++) {
		if (fw_state & (0x1 << (i))) {
			printf("%s%s", first ? "": ", ", ops_status_fw_state_str_tbl[i]);
			first = FALSE;
		}
	}
	printf(")\n");
}

static int
wl_ops_status(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_OK;
	wl_ops_status_v1_t ops_status;
	wl_ops_status_v1_t *status_ptr;

	/* toss the command name */
	argv++;

	bzero(&ops_status, sizeof(ops_status));
	ops_status.version = htod16(WL_OPS_STATUS_VERSION_1);
	ops_status.len = sizeof(ops_status);
	ops_status.len = htod16(ops_status.len);

	/* invoke GET iovar */
	err = wlu_iovar_getbuf(wl, cmd->name, &ops_status, sizeof(ops_status),
	                       buf, WLC_IOCTL_MEDLEN);
	if (err != BCME_OK)
		return err;

	status_ptr = (wl_ops_status_v1_t *)buf;
	status_ptr->version = dtoh16(status_ptr->version);
	status_ptr->len = dtoh16(status_ptr->len);
	/* Check version */
	if (status_ptr->version != WL_OPS_STATUS_VERSION_1) {
		fprintf(stderr, "Unexpected version: %u\n",
		        status_ptr->version);
		return BCME_VERSION;
	}
	/* validate length */
	if (status_ptr->len < (uint8) sizeof(*status_ptr)) {
		fprintf(stderr, "OPS Status: short len %d < %d\n",
		        status_ptr->len, (int)sizeof(*status_ptr));
		return BCME_BADLEN;
	}

	printf("Current Slice Index: %d\n", status_ptr->slice_index);
	/* dump fw disable */
	wl_ops_status_fw_disable_dump(dtoh32(status_ptr->disable_reasons));
	printf("Applied Config: 0x%x\n", dtoh32(status_ptr->applied_ops_config));
	printf("Disable OBSS: %d\n", status_ptr->disable_obss);
	printf("Disable Dur(ms): %u\n", dtoh32(status_ptr->disable_duration));
	printf("Partial Dur: %u\n", dtoh32(status_ptr->partial_ops_dur));
	printf("Full Dur: %u\n", dtoh32(status_ptr->full_ops_dur));
	printf("OPS Count:\n");
	printf("\tNAV: %u\n", dtoh32(status_ptr->nav_cnt));
	printf("\tPLCP: %u\n", dtoh32(status_ptr->plcp_cnt));
	printf("\tMYBSS: %u\n", dtoh32(status_ptr->mybss_cnt));
	printf("\tOBSS: %u\n", dtoh32(status_ptr->obss_cnt));
	printf("OPS Miss Count:\n");
	printf("\t< Min Dur: %u\n", dtoh32(status_ptr->miss_dur_cnt));
	printf("\t> Max Dur: %u\n", dtoh32(status_ptr->max_dur_cnt));
	printf("\tWake: %u\n", dtoh32(status_ptr->wake_cnt));
	printf("\tBCN Wait: %u\n", dtoh32(status_ptr->bcn_wait_cnt));
	printf("\tPreempt Thresh: %u\n", dtoh32(status_ptr->miss_premt_cnt));
	printf("Hist OPS Dur Count:\n");
	printf("\t%10u: [0-1]ms\n", dtoh32(status_ptr->count_dur_hist[0]));
	printf("\t%10u: [1-2]ms\n", dtoh32(status_ptr->count_dur_hist[1]));
	printf("\t%10u: [2-4]ms\n", dtoh32(status_ptr->count_dur_hist[2]));
	printf("\t%10u: [4-8]ms\n", dtoh32(status_ptr->count_dur_hist[3]));
	printf("\t%10u: [ >8]ms\n", dtoh32(status_ptr->count_dur_hist[4]));

	return err;
}

static const char *nap_status_fw_state_str_tbl[] = {
	"HOST",
	"RSSI",
	"SCAN",
	"ASSOC",
};

static const char *nap_status_hw_state_str_tbl[] = {
	"NAP",
	"", "", "", "", "", "",
	"Core Down"
};

static void
wl_nap_status_fw_status_dump(uint16 fw_state)
{
	uint i, n;
	bool first = TRUE;

	printf("FW Disable Reason : 0x%04x (", fw_state);
	if (fw_state == 0) {
		printf("%s)\n", "NONE");
		return;
	}
	n = MIN(ARRAYSIZE(nap_status_fw_state_str_tbl), NBITS(fw_state));
	for (i = 0; i < n; i++) {
		if (fw_state & (0x1 << (i))) {
			printf("%s%s", first ? "": ", ", nap_status_fw_state_str_tbl[i]);
			first = FALSE;
		}
	}
	printf(")\n");
}

static void
wl_nap_status_hw_status_dump(uint8 hw_state)
{
	uint i, n;
	bool first = TRUE;

	printf("HW Status Bits    : 0x%02x   ", hw_state);
	n = MIN(ARRAYSIZE(nap_status_hw_state_str_tbl), NBITS(hw_state));
	for (i = 0; i < n; i++) {
		if (hw_state & (0x1 << (i))) {
			printf("%s%s", first ? "(": ", ", nap_status_hw_state_str_tbl[i]);
			first = FALSE;
		}
	}
	printf("%s\n", hw_state ? ")" : "");
}

static int
wl_nap_status(void *wl, cmd_t *cmd, char **argv)
{
	int err = BCME_OK;
	wl_nap_status_v1_t nap_status;
	wl_nap_status_v1_t *status_ptr;

	/* toss the command name */
	argv++;

	bzero(&nap_status, sizeof(nap_status));
	nap_status.version = htod16(WL_NAP_STATUS_VERSION_1);
	nap_status.len = sizeof(nap_status);
	nap_status.len = htod16(nap_status.len);

	/* invoke GET iovar */
	err = wlu_iovar_getbuf(wl, cmd->name, &nap_status, sizeof(nap_status),
	                       buf, WLC_IOCTL_MEDLEN);
	if (err != BCME_OK)
		return err;

	status_ptr = (wl_nap_status_v1_t *)buf;
	status_ptr->version = dtoh16(status_ptr->version);
	status_ptr->len = dtoh16(status_ptr->len);
	/* Check version */
	if (status_ptr->version != WL_NAP_STATUS_VERSION_1) {
		fprintf(stderr, "Unexpected version: %u\n",
		        status_ptr->version);
		return BCME_VERSION;
	}
	/* validate length */
	if (status_ptr->len < (uint8) sizeof(*status_ptr)) {
		fprintf(stderr, "NAP Status: short len %d < %d\n",
		        status_ptr->len, (int)sizeof(*status_ptr));
		return BCME_BADLEN;
	}

	/* Display content */
	printf("Current Slice Index: %d\n", status_ptr->slice_index);
	wl_nap_status_fw_status_dump(dtoh16(status_ptr->fw_status));
	wl_nap_status_hw_status_dump(status_ptr->hw_status);
	printf("Total Disable Dur(ms): %u\n", dtoh32(status_ptr->total_disable_dur));

	return err;
}
