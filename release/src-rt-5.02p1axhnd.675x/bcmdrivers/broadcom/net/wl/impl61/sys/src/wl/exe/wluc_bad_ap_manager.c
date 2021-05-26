/*
 * wl bad ap manager command module
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
 * $Id: wluc_bad_ap_manager.c 774769 2019-05-07 08:46:22Z $
 */

#include <wlioctl.h>
#include <wlioctl_utils.h>

#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmiov.h>
#include "wlu_common.h"
#include "wlu.h"
#include <wlioctl_bad_ap_manager.h>

typedef struct wl_bam_sub_cmd wl_bam_sub_cmd_t;
typedef int (subcmd_handler_t)(void *wl, const wl_bam_sub_cmd_t *cmd,
	char **argv);
typedef void (help_handler_t) (const wl_bam_sub_cmd_t *cmd);

struct wl_bam_sub_cmd {
	char *name;
	uint16 cmd_id;
	uint16 type;
	subcmd_handler_t *hndler;
	help_handler_t *help;
};

#define HELP_STR1	"help"
#define HELP_STR2	"-h"
#define PR_ERR(...)	fprintf(stderr, __VA_ARGS__)
#define PRINT(...) fprintf(stdout, __VA_ARGS__)

/* BAD AP IOVAR COMMON START */
static subcmd_handler_t wl_bam_sub_cmd_enable;
static subcmd_handler_t wl_bam_sub_cmd_disable;
static subcmd_handler_t wl_bam_sub_cmd_config;
static subcmd_handler_t wl_bam_sub_cmd_dump;

static help_handler_t wl_bam_sub_cmd_enable_help;
static help_handler_t wl_bam_sub_cmd_disable_help;
static help_handler_t wl_bam_sub_cmd_config_help;
static help_handler_t wl_bam_sub_cmd_dump_help;

static cmd_func_t wl_bam_main;

static cmd_t wl_bam_cmds[] = {
	{BAM_CMD_NAME, wl_bam_main, WLC_GET_VAR, WLC_SET_VAR, ""},
	{NULL, NULL, 0, 0, NULL}
};

static const wl_bam_sub_cmd_t bam_sub_cmd_list[] =
{
	{"enable",
		WL_BAM_CMD_ENABLE_V1,
		IOVT_BUFFER,
		wl_bam_sub_cmd_enable,
		wl_bam_sub_cmd_enable_help},

	{"disable",
		WL_BAM_CMD_DISABLE_V1,
		IOVT_BUFFER,
		wl_bam_sub_cmd_disable,
		wl_bam_sub_cmd_disable_help},

	{"config",
		WL_BAM_CMD_CONFIG_V1,
		IOVT_BUFFER,
		wl_bam_sub_cmd_config,
		wl_bam_sub_cmd_config_help},

	{"dump",
		WL_BAM_CMD_DUMP_V1,
		IOVT_BUFFER,
		wl_bam_sub_cmd_dump,
		wl_bam_sub_cmd_dump_help},
	{NULL, 0, 0, NULL, NULL}
};

/* module initialization */
void
wluc_bam_module_init(void)
{
	wl_module_cmds_register(wl_bam_cmds);
}

static void
wl_bam_help_all(void)
{
	wl_bam_sub_cmd_t *curcmd;

	PRINT("USAGE - wl %s <sub_cmd>\n", BAM_CMD_NAME);
	PRINT("\tsupported <sub_cmd>s: %s %s", HELP_STR1, HELP_STR2);

	curcmd = (wl_bam_sub_cmd_t *)&bam_sub_cmd_list[0];
	while (curcmd->name) {
		PRINT(" %s", curcmd->name);
		curcmd++;
	}
	PRINT("\n\tHELP for each <sub_cmd> : wl %s <sub_cmd> %s\n",
		BAM_CMD_NAME, HELP_STR1);

	curcmd = (wl_bam_sub_cmd_t *)&bam_sub_cmd_list[0];
	while (curcmd->name) {
		if (curcmd->help) {
			PRINT("\n");
			curcmd->help(curcmd);
		}
		curcmd++;
	}
	return;
}

static int
wl_bam_main(void *wl, cmd_t *cmd, char **argv)
{
	const wl_bam_sub_cmd_t *subcmd = &bam_sub_cmd_list[0];

	/* skip to command name */
	UNUSED_PARAMETER(cmd);
	argv++;

	if (*argv) {
		if (strcmp(*argv, HELP_STR1) == 0 ||
			strcmp(*argv, HELP_STR2) == 0) {
			wl_bam_help_all();
			return BCME_OK;
		}
		while (subcmd->name) {
			if (strcmp(subcmd->name, *argv) == 0) {
				if (subcmd->hndler) {
					return subcmd->hndler(wl, subcmd, ++argv);
				}
			}
			subcmd++;
		}
	}

	wl_bam_help_all();
	return BCME_OK;
}

/* sub command enable */
static void
wl_bam_sub_cmd_enable_help(const wl_bam_sub_cmd_t *cmd)
{
	PRINT("HELP for %s\n", cmd->name);
	PRINT("\tCMD HELP: wl %s %s [ %s | %s ]\n", BAM_CMD_NAME, cmd->name, HELP_STR1, HELP_STR2);
	PRINT("\tCMD GET for %s: wl %s %s\n", cmd->name, BAM_CMD_NAME, cmd->name);
	PRINT("\tCMD SET for %s: wl %s %s <type 1> <type 2> .. <type n>\n",
		cmd->name, BAM_CMD_NAME, cmd->name);
	PRINT("\tsupported <type n>s: %s", BAM_TYPE_NO_BCN_STR);

	PRINT("\n");
}

static int
wl_bam_sub_cmd_enable_get(void *wl, const wl_bam_sub_cmd_t *cmd)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint16	iovlen = 0;
	struct wl_bam_iov_enable_v1 *enable;

	iov_buf = (bcm_iov_buf_t *)calloc(1, BAM_IOCTL_BUF_LEN);
	if (iov_buf == NULL) {
		return BCME_NOMEM;
	}

	iov_buf->version = WL_BAM_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;
	iovlen = sizeof(bcm_iov_buf_t);

	ret = wlu_iovar_getbuf(wl, BAM_CMD_NAME,
		(void *)iov_buf, iovlen, (void *)iov_buf, BAM_IOCTL_BUF_LEN);
	if (ret != BCME_OK) {
		if (ret == BCME_IOCTL_ERROR) {
			int bcmerr;
			if (wlu_iovar_getint(wl, "bcmerror", &bcmerr) >= 0) {
				ret = bcmerr;
			}
		}
		PR_ERR("Fail to get %s status : err = %d\n", cmd->name, ret);
		goto finish;
	}

	enable = (struct wl_bam_iov_enable_v1 *)&iov_buf->data[0];

	PRINT("Enabled BAD type:%x ", enable->bad_type);
	if (enable->bad_type == BAM_TYPE_NONE) {
		PRINT("(NONE)\n");
		goto finish;
	} else if (enable->bad_type == BAM_TYPE_ALL) {
		PRINT("(ALL)\n");
		goto finish;
	} else {
		PRINT("(");
	}

	if (enable->bad_type & BAM_TYPE_NO_BCN) {
		PRINT(" %s", BAM_TYPE_NO_BCN_STR);
	}

	PRINT(" )\n");

finish:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_bam_sub_cmd_enable(void *wl, const wl_bam_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	struct wl_bam_iov_enable_v1 *enable;
	uint16	iovlen = 0;

	/* GET */
	if (*argv == NULL) {
		return wl_bam_sub_cmd_enable_get(wl, cmd);
	}

	/* HELP */
	if (strcmp(*argv, HELP_STR1) == 0 ||
		strcmp(*argv, HELP_STR2) == 0) {
		if (cmd->help) {
			cmd->help(cmd);
		}
		return BCME_OK;
	}

	/* SET */
	iov_buf = (bcm_iov_buf_t *)calloc(1, BAM_IOCTL_BUF_LEN);
	if (iov_buf == NULL) {
		return BCME_NOMEM;
	}
	iovlen = BAM_IOCTL_BUF_LEN;
	iov_buf->version = WL_BAM_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;
	enable = (struct wl_bam_iov_enable_v1 *)&iov_buf->data[0];

	enable->bad_type = BAM_TYPE_NONE;
	while (*argv) {
		if (strcmp(*argv, BAM_TYPE_NO_BCN_STR) == 0) {
			enable->bad_type |= BAM_TYPE_NO_BCN;
		} else {
			enable->bad_type = strtoul(*argv, NULL, 16);
			break;
		}

		argv++;
	}

	ret = wlu_var_setbuf(wl, BAM_CMD_NAME, (void *)iov_buf, iovlen);
	if (ret != BCME_OK) {
		if (ret == BCME_IOCTL_ERROR) {
			int bcmerr;
			if (wlu_iovar_getint(wl, "bcmerror", &bcmerr) >= 0) {
				ret = bcmerr;
			}
		}
		PR_ERR("Fail to %s sub types : ret = %d\n", cmd->name, ret);
		goto fail;
	}

fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

/* sub command disable */
static void
wl_bam_sub_cmd_disable_help(const wl_bam_sub_cmd_t *cmd)
{
	PRINT("HELP for %s\n", cmd->name);
	PRINT("\tCMD HELP: wl %s %s [ %s | %s ]\n", BAM_CMD_NAME, cmd->name, HELP_STR1, HELP_STR2);
	PRINT("\tCMD GET for %s: NOT SUPPORTED\n", cmd->name);
	PRINT("\tCMD SET for %s: wl %s %s <type 1> <type 2> .. <type n>\n",
		cmd->name, BAM_CMD_NAME, cmd->name);
	PRINT("\tsupported <type n>s: %s", BAM_TYPE_NO_BCN_STR);

	PRINT("\n");
}

static int
wl_bam_sub_cmd_disable(void *wl, const wl_bam_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	struct wl_bam_iov_enable_v1 *disable;
	uint16	iovlen = 0;

	/* GET */
	if (*argv == NULL) {
		if (cmd->help) {
			cmd->help(cmd);
		}
		return BCME_BADARG;
	}

	/* HELP */
	if (strcmp(*argv, HELP_STR1) == 0 ||
		strcmp(*argv, HELP_STR2) == 0) {
		if (cmd->help) {
			cmd->help(cmd);
		}
		return BCME_OK;
	}

	/* SET */
	iov_buf = (bcm_iov_buf_t *)calloc(1, BAM_IOCTL_BUF_LEN);
	if (iov_buf == NULL) {
		return BCME_NOMEM;
	}

	iovlen = BAM_IOCTL_BUF_LEN;
	iov_buf->version = WL_BAM_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;
	disable = (struct wl_bam_iov_enable_v1 *)&iov_buf->data[0];

	disable->bad_type = BAM_TYPE_NONE;
	while (*argv) {
		if (strcmp(*argv, BAM_TYPE_NO_BCN_STR) == 0) {
			disable->bad_type |= BAM_TYPE_NO_BCN;
		} else {
			disable->bad_type = strtoul(*argv, NULL, 16);
			break;
		}

		argv++;
	}

	ret = wlu_var_setbuf(wl, BAM_CMD_NAME, (void *)iov_buf, iovlen);
	if (ret != BCME_OK) {
		if (ret == BCME_IOCTL_ERROR) {
			int bcmerr;
			if (wlu_iovar_getint(wl, "bcmerror", &bcmerr) >= 0) {
				ret = bcmerr;
			}
		}
		PR_ERR("Fail to %s sub types : ret = %d\n", cmd->name, ret);
		goto fail;
	}

fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

/* sub command config */
static void
wl_bam_sub_cmd_config_bcn_help(void)
{
	PRINT("\n\t<param>s for %s\n", BAM_TYPE_NO_BCN_STR);
	PRINT("\tdecision_cnt_bad : Range = %d ~ %d\n",
		BAM_BCN_DECISION_BAD_MIN, BAM_BCN_DECISION_BAD_MAX);
	PRINT("\tdecision_cnt_normal : Range = %d ~ %d\n",
		BAM_BCN_DECISION_NORM_MIN, BAM_BCN_DECISION_NORM_MAX);
	PRINT("\tretry_max_bad : Range = %d ~ %d\n",
		BAM_BCN_RETRY_BAD_MIN, BAM_BCN_RETRY_BAD_MAX);
	PRINT("\tretry_max_unknown : Range = %d ~ %d\n",
		BAM_BCN_RETRY_UNKNOWN_MIN, BAM_BCN_RETRY_UNKNOWN_MAX);
	PRINT("\tnull_send_prd : Range = %d ~ %d\n",
		BAM_BCN_NULL_PERIOD_MIN, BAM_BCN_NULL_PERIOD_MAX);
	PRINT("\twait_pkt_time : Range = %d ~ %d\n",
		BAM_BCN_WAIT_PKT_TIME_MIN, BAM_BCN_WAIT_PKT_TIME_MAX);
	PRINT("\ttraining_trig_time : Range = %d ~ %d\n",
		BAM_BCN_TRIG_TIME_MIN, BAM_BCN_TRIG_TIME_MAX);
}

static void
wl_bam_sub_cmd_config_help(const wl_bam_sub_cmd_t *cmd)
{

	PRINT("HELP for %s\n", cmd->name);
	PRINT("\tCMD HELP: wl %s %s [ %s | %s ]\n", BAM_CMD_NAME, cmd->name, HELP_STR1, HELP_STR2);
	PRINT("\tCMD GET for %s: wl %s %s <type>\n", cmd->name, BAM_CMD_NAME, cmd->name);
	PRINT("\tCMD SET for %s: wl %s %s <type> <param1> <param2> .. <param n>\n",
		cmd->name, BAM_CMD_NAME, cmd->name);
	PRINT("\tsupported <type>:");

	wl_bam_sub_cmd_config_bcn_help();
}

static int
wl_bam_bcn_config_set(void *obuf, char **argv)
{

	struct wl_bam_iov_bcn_config_v1 *config = (struct wl_bam_iov_bcn_config_v1 *)obuf;

	config->decision_cnt_bad = strtoul(*argv++, NULL, 0);
	if (config->decision_cnt_bad < BAM_BCN_DECISION_BAD_MIN ||
		config->decision_cnt_bad > BAM_BCN_DECISION_BAD_MAX) {
		return BCME_RANGE;
	}

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	config->decision_cnt_normal = strtoul(*argv++, NULL, 0);
	if (config->decision_cnt_normal < BAM_BCN_DECISION_NORM_MIN ||
		config->decision_cnt_normal >= BAM_BCN_DECISION_NORM_MAX) {
		return BCME_RANGE;
	}

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	config->retry_max_bad = strtoul(*argv++, NULL, 0);
	if (config->retry_max_bad < BAM_BCN_RETRY_BAD_MIN ||
		config->retry_max_bad > BAM_BCN_RETRY_BAD_MAX) {
		return BCME_RANGE;
	}

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	config->retry_max_unknown = strtoul(*argv++, NULL, 0);
	if (config->retry_max_unknown < BAM_BCN_RETRY_UNKNOWN_MIN ||
		config->retry_max_unknown > BAM_BCN_RETRY_UNKNOWN_MAX) {
		return BCME_RANGE;
	}

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	config->null_send_prd = strtoul(*argv++, NULL, 0);
	if (config->null_send_prd < BAM_BCN_NULL_PERIOD_MIN ||
		config->null_send_prd > BAM_BCN_NULL_PERIOD_MAX) {
		return BCME_RANGE;
	}

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	config->wait_pkt_time = strtoul(*argv++, NULL, 0);
	if (config->wait_pkt_time < BAM_BCN_WAIT_PKT_TIME_MIN ||
		config->wait_pkt_time > BAM_BCN_WAIT_PKT_TIME_MAX) {
		return BCME_RANGE;
	}

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	config->training_trig_time = strtoul(*argv++, NULL, 0);
	if (config->training_trig_time < BAM_BCN_TRIG_TIME_MIN ||
		config->training_trig_time > BAM_BCN_TRIG_TIME_MAX) {
		return BCME_RANGE;
	}

	return BCME_OK;
}

static void
wl_bam_bcn_config_print(void *buf)
{
	struct wl_bam_iov_bcn_config_v1 *config = (struct wl_bam_iov_bcn_config_v1 *)buf;

	PRINT("CONFIGURED VALUE for %s\n", BAM_TYPE_NO_BCN_STR);
	PRINT("decision_cnt_bad : %d\n", config->decision_cnt_bad);
	PRINT("decision_cnt_normal : %d\n", config->decision_cnt_normal);
	PRINT("retry_max_bad : %d\n", config->retry_max_bad);
	PRINT("retry_max_unknown : %d\n", config->retry_max_unknown);
	PRINT("null_send_prd : %d\n", config->null_send_prd);
	PRINT("wait_pkt_time : %d\n", config->wait_pkt_time);
	PRINT("training_trig_time : %d\n", config->training_trig_time);
	return;
}

static int
wl_bam_sub_cmd_config(void *wl, const wl_bam_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	struct wl_bam_iov_config_v1 *config;
	uint16 iovlen = 0;
	uint8 is_set = FALSE;

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}

	/* HELP */
	if (strcmp(*argv, HELP_STR1) == 0 ||
		strcmp(*argv, HELP_STR2) == 0) {
		if (cmd->help) {
			cmd->help(cmd);
		}
		return BCME_OK;
	}

	iov_buf = (bcm_iov_buf_t *)calloc(1, BAM_IOCTL_BUF_LEN);
	if (iov_buf == NULL) {
		return BCME_NOMEM;
	}
	iov_buf->version = WL_BAM_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;
	config = (struct wl_bam_iov_config_v1 *)&iov_buf->data[0];
	iovlen = sizeof(bcm_iov_buf_t) + sizeof(struct wl_bam_iov_config_v1);
	iovlen += sizeof(struct wl_bam_iov_bcn_config_v1);

	/* GET SUB TYPE AND PARAMETERS */
	if (strcmp(*argv, BAM_TYPE_NO_BCN_STR) == 0) {
		config->bad_type = BAM_TYPE_NO_BCN;
		argv++;
		if (*argv != NULL) {
			is_set = TRUE;
			ret = wl_bam_bcn_config_set(&config->config[0], argv);
		}
	} else {
		ret = BCME_USAGE_ERROR;
	}

	if (ret != BCME_OK) {
		goto fail;
	}

	/* IOVAR */
	if (is_set) {
		ret = wlu_var_setbuf(wl, BAM_CMD_NAME, (void *)iov_buf, iovlen);
	} else {
		ret = wlu_iovar_getbuf(wl, BAM_CMD_NAME,
			(void *)iov_buf, iovlen, (void *)iov_buf, BAM_IOCTL_BUF_LEN);
	}

	if (ret != BCME_OK) {
		if (ret == BCME_IOCTL_ERROR) {
			int bcmerr;
			if (wlu_iovar_getint(wl, "bcmerror", &bcmerr) >= 0) {
				ret = bcmerr;
			}
		}
		PR_ERR("Fail to %s %s : ret = %d\n", cmd->name, is_set?"set":"get", ret);
		goto fail;
	}
	if (is_set) {
		goto fail;
	}

	/* PRINT GET RESULT */
	config = (struct wl_bam_iov_config_v1 *)&iov_buf->data[0];
	switch (config->bad_type) {
		case BAM_TYPE_NO_BCN:
			wl_bam_bcn_config_print(&config->config[0]);
			break;
		default:
			PR_ERR("INVALID BAD TYPE:%x\n", config->bad_type);
			break;
	}

fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

/* sub command dump */
static void
wl_bam_sub_cmd_dump_help(const wl_bam_sub_cmd_t *cmd)
{

	PRINT("HELP for %s\n", cmd->name);
	PRINT("\tCMD HELP: wl %s %s [ %s | %s ]\n", BAM_CMD_NAME, cmd->name, HELP_STR1, HELP_STR2);
	PRINT("\tCMD GET for %s: wl %s %s <type>\n", cmd->name, BAM_CMD_NAME, cmd->name);
	PRINT("\tCMD SET for %s: NOT SUPPORTED\n", cmd->name);
	PRINT("\tsupported <type>: %s", BAM_TYPE_NO_BCN_STR);

	PRINT("\n");
}

static void
wl_bam_bcn_dump(struct wl_bam_iov_dump_v1 *iov)
{
	struct wl_bam_iov_dump_bcn_elem_v1 *elem;
	uint32 count = iov->count;

	elem = (struct wl_bam_iov_dump_bcn_elem_v1 *)iov->dump;

	while (count > 0) {
		PRINT("wlc_idx: %d\n", elem->wlc_idx);
		PRINT("bss_idx: %d\n", elem->bss_idx);
		PRINT("ap_type: %d\n", elem->ap_type);
		PRINT("state: %d\n", elem->state);
		PRINT("retry_cnt: %d\n", elem->retry_cnt);
		PRINT("bad_cnt: %d\n", elem->bad_cnt);
		PRINT("normal_cnt: %d\n\n", elem->normal_cnt);
		count--;
		elem++;
	}
}

static int
wl_bam_sub_cmd_dump(void *wl, const wl_bam_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint16 iovlen;
	struct wl_bam_iov_dump_v1 *dump;

	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}

	/* HELP */
	if (strcmp(*argv, HELP_STR1) == 0 ||
		strcmp(*argv, HELP_STR2) == 0) {
		if (cmd->help) {
			cmd->help(cmd);
		}
		return BCME_OK;
	}

	iov_buf = (bcm_iov_buf_t *)calloc(1, BAM_IOCTL_BUF_LEN);
	if (iov_buf == NULL) {
		return BCME_NOMEM;
	}
	iov_buf->version = WL_BAM_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;
	dump = (struct wl_bam_iov_dump_v1 *)&iov_buf->data[0];
	iovlen = sizeof(bcm_iov_buf_t) + sizeof(struct wl_bam_iov_dump_v1);
	dump->count = 0;

	/* GET SUB TYPE */
	if (strcmp(*argv, BAM_TYPE_NO_BCN_STR) == 0) {
		dump->bad_type = BAM_TYPE_NO_BCN;
	} else {
		ret = BCME_USAGE_ERROR;
		goto fail;
	}

	ret = wlu_iovar_getbuf(wl, BAM_CMD_NAME,
		(void *)iov_buf, iovlen, (void *)iov_buf, BAM_IOCTL_BUF_LEN);
	if (ret != BCME_OK) {
		if (ret == BCME_IOCTL_ERROR) {
			int bcmerr;
			if (wlu_iovar_getint(wl, "bcmerror", &bcmerr) >= 0) {
				ret = bcmerr;
			}
		}
		PR_ERR("Fail to get %s : ret = %d\n", cmd->name, ret);
		goto fail;
	}

	PRINT("Current STatus for %s\n", *argv);

	dump = (struct wl_bam_iov_dump_v1 *)&iov_buf->data[0];

	PRINT("enabled bsscfg count: %d\n", dump->count);
	switch (dump->bad_type) {
		case BAM_TYPE_NO_BCN:
			wl_bam_bcn_dump(dump);
			break;
		default:
			PR_ERR("INVALDI BAD TYPE: %x\n", dump->bad_type);
			break;
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}
