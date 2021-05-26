/*
 * dynamic tx power control command handling module
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

#ifdef WIN32
#include <windows.h>
#endif // endif
#include <wlioctl.h>
#include <wlioctl_utils.h>

/* Because IL_BIGENDIAN was removed there are few warnings that need
 * to be fixed. Windows was not compiled earlier with IL_BIGENDIAN.
 * Hence these warnings were not seen earlier.
 * For now ignore the following warnings
 */
#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4761)
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmiov.h>
#include "wlu_common.h"
#include "wlu.h"
#include "wlu_rates_matrix.h"

#define HELP_STR1	"help"
#define HELP_STR2	"-h"
#define PR_ERR(...)	fprintf(stderr, __VA_ARGS__)
#define PRINT(...) fprintf(stdout, __VA_ARGS__)

/* internal ioctl interface selection */
typedef struct wl_dtpc_sub_cmd wl_dtpc_sub_cmd_t;

/* internal forward declarations */
typedef int (subcmd_handler_t)(void *wl, const wl_dtpc_sub_cmd_t *cmd,
	char **argv);
typedef void (help_handler_t) (const wl_dtpc_sub_cmd_t *cmd);

static cmd_func_t wl_dtpc_main;

struct wl_dtpc_sub_cmd {
	char *name;
	uint16 cmd_id;
	uint16 type;
	subcmd_handler_t *hndler;
	help_handler_t *help;
};

static cmd_t wl_dtpc_cmds[] = {
	{"dtpc", wl_dtpc_main, WLC_GET_VAR, WLC_SET_VAR, ""},
	{NULL, NULL, 0, 0, NULL}
};

/* each iovar hander prototype defs */
static subcmd_handler_t wl_dtpc_sub_cmd_en;
static subcmd_handler_t wl_dtpc_sub_cmd_txsthres;
static subcmd_handler_t wl_dtpc_sub_cmd_prb_step;
static subcmd_handler_t wl_dtpc_sub_cmd_alpha;
static subcmd_handler_t wl_dtpc_sub_cmd_trigint;
static subcmd_handler_t wl_dtpc_sub_cmd_headroom;
static subcmd_handler_t wl_dtpc_sub_cmd_clmovr;

static help_handler_t wl_dtpc_sub_cmd_en_help;
static help_handler_t wl_dtpc_sub_cmd_txsthres_help;
static help_handler_t wl_dtpc_sub_cmd_prb_step_help;
static help_handler_t wl_dtpc_sub_cmd_alpha_help;
static help_handler_t wl_dtpc_sub_cmd_trigint_help;
static help_handler_t wl_dtpc_sub_cmd_headroom_help;
static help_handler_t wl_dtpc_sub_cmd_clmovr_help;

/* internal prototypes */

/* cmd list */
static const wl_dtpc_sub_cmd_t dtpc_sub_cmd_list[] =
{
	{"en",
		WL_DTPC_CMD_EN,
		IOVT_BUFFER,
		wl_dtpc_sub_cmd_en,
		wl_dtpc_sub_cmd_en_help},
	{"txs_thres",
		WL_DTPC_CMD_TXS_THRES,
		IOVT_BUFFER,
		wl_dtpc_sub_cmd_txsthres,
		wl_dtpc_sub_cmd_txsthres_help},
	{"prb_step",
		WL_DTPC_CMD_PROBSTEP,
		IOVT_BUFFER,
		wl_dtpc_sub_cmd_prb_step,
		wl_dtpc_sub_cmd_prb_step_help},
	{"alpha",
		WL_DTPC_CMD_ALPHA,
		IOVT_BUFFER,
		wl_dtpc_sub_cmd_alpha,
		wl_dtpc_sub_cmd_alpha_help},
	{"trigint",
		WL_DTPC_CMD_TRIGINT,
		IOVT_BUFFER,
		wl_dtpc_sub_cmd_trigint,
		wl_dtpc_sub_cmd_trigint_help},
	{"headroom",
		WL_DTPC_CMD_HEADROOM,
		IOVT_BUFFER,
		wl_dtpc_sub_cmd_headroom,
		wl_dtpc_sub_cmd_headroom_help},
	{"clmovr",
		WL_DTPC_CMD_CLMOVR,
		IOVT_BUFFER,
		wl_dtpc_sub_cmd_clmovr,
		wl_dtpc_sub_cmd_clmovr_help},
	{NULL, 0, 0, NULL, NULL}
};

/* module initialization */
void
wluc_dtpc_module_init(void)
{
	/* register dtpc module cmd handler */
	wl_module_cmds_register(wl_dtpc_cmds);
}

static void
wl_dtpc_help_all(void)
{
	wl_dtpc_sub_cmd_t *curcmd;

	PRINT("USAGE - wl %s <sub_cmd>\n", "dtpc");
	PRINT("\tsupported <sub_cmd>s: %s %s", HELP_STR1, HELP_STR2);

	curcmd = (wl_dtpc_sub_cmd_t *)&dtpc_sub_cmd_list[0];
	while (curcmd->name) {
		PRINT(" %s", curcmd->name);
		curcmd++;
	}
	PRINT("\n\tHELP for each <sub_cmd> : wl %s <sub_cmd> %s\n",
		"dtpc", HELP_STR1);

	curcmd = (wl_dtpc_sub_cmd_t *)&dtpc_sub_cmd_list[0];
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
wl_dtpc_main(void *wl, cmd_t *cmd, char **argv)
{
	const wl_dtpc_sub_cmd_t *subcmd = &dtpc_sub_cmd_list[0];

	/* skip to command name */
	UNUSED_PARAMETER(cmd);
	argv++;

	if (*argv) {
		if (strcmp(*argv, HELP_STR1) == 0 ||
			strcmp(*argv, HELP_STR2) == 0) {
			wl_dtpc_help_all();
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

	wl_dtpc_help_all();
	return BCME_OK;
}

/* help functions */
static void
wl_dtpc_sub_cmd_en_help(const wl_dtpc_sub_cmd_t *cmd)
{
	PRINT("subcmd: %s\n", cmd->name);
	PRINT("\tRuntime enable and disable flag\n");
	PRINT("\tGET/SET: wl dtpc %s [0/1]\n", cmd->name);
	PRINT("\t  0: disable, 1: enable\n");
}

static void
wl_dtpc_sub_cmd_txsthres_help(const wl_dtpc_sub_cmd_t *cmd)
{
	PRINT("subcmd: %s\n", cmd->name);
	PRINT("\tCollecting txs statistics until this threshold\n");
	PRINT("\tGET/SET: wl dtpc %s [# of pkt counts]\n", cmd->name);
	PRINT("\t  e.g. wl dtpc %s 8 : PSR evaluation happens every 8 txstatus\n",
		cmd->name);
}
static void
wl_dtpc_sub_cmd_prb_step_help(const wl_dtpc_sub_cmd_t *cmd)
{
	PRINT("subcmd: %s\n", cmd->name);
	PRINT("\tStep size of tx power offset used during probing (linear search case)\n");
	PRINT("\tGET/SET: wl dtpc %s [step size]\n", cmd->name);
	PRINT("\t  1/2/3/../8 : given [input] * qdB \n");
}

static void
wl_dtpc_sub_cmd_alpha_help(const wl_dtpc_sub_cmd_t *cmd)
{
	PRINT("subcmd: %s\n", cmd->name);
	PRINT("\tdtpc PSR EMA alpha factor\n");
	PRINT("\tGET/SET: wl dtpc %s [1/2/4/8]\n", cmd->name);
}

static void
wl_dtpc_sub_cmd_trigint_help(const wl_dtpc_sub_cmd_t *cmd)
{
	PRINT("subcmd: %s\n", cmd->name);
	PRINT("\tpower probing trigger interval (msec)\n");
	PRINT("\tSET: wl dtpc %s\n", cmd->name);
}

static void
wl_dtpc_sub_cmd_headroom_help(const wl_dtpc_sub_cmd_t *cmd)
{
	PRINT("subcmd: %s\n", cmd->name);
	PRINT("\tprint out DTPC headrooms\n");
	PRINT("\tGET: headroom <rspec>\n");
	PRINT("\t   get dtpc headroom for ratespec <rspec>\n");
	PRINT("\tGET: headroom all\n");
	PRINT("\t   get dtpc headroom for all known ratespecs\n");
}

static void
wl_dtpc_sub_cmd_clmovr_help(const wl_dtpc_sub_cmd_t *cmd)
{
	PRINT("subcmd: %s\n", cmd->name);
	PRINT("\tclmlimit overriding (qdB)\n");
	PRINT("\tSET: wl dtpc %s [0 ~ 40]\n", cmd->name);
}

/* simple input query type and set case handling function */
static int
wl_dtpc_sub_cmd_cmn_val(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv, bool signval)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint32 *val = NULL;
	uint16 iov_len = sizeof(*iov_buf);
	wl_dtpc_cfg_v1_t *dtpc_cfg;

	if ((iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_SMLEN)) == NULL) {
		return BCME_NOMEM;
	}
	iov_len += sizeof(*dtpc_cfg);

	/* common bcm_iov_buf info */
	iov_buf->version = WL_DTPC_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;
	iov_buf->len = iov_len;

	dtpc_cfg = (wl_dtpc_cfg_v1_t *)&iov_buf->data[0];
	/* dtpc module input info */
	dtpc_cfg->version = WL_DTPC_IOV_VERSION;
	memset(dtpc_cfg->pmac, 0, ETHER_ADDR_LEN); /* no bzero for compatibility */

	if (*argv == NULL) {
		/* get case handling */
		val = (uint32 *)&iov_buf->data[0];
		ret = wlu_iovar_getbuf(wl, "dtpc", (void *)iov_buf, iov_len,
			(void *)iov_buf, WLC_IOCTL_SMLEN);
		if (ret != BCME_OK) {
			if (ret == BCME_IOCTL_ERROR) {
				int bcm_err;
				if (wlu_iovar_getint(wl, "bcmerror", &bcm_err) >= 0) {
					ret = bcm_err;
				}
			}
			PR_ERR("get subcmd %s fails : err = %d\n", cmd->name, ret);
			goto finish;
		} else {
			if (signval) {
				int32 *sval;
				sval = (int32 *)&iov_buf->data[0];
				PRINT("%d\n", (int)*sval);
			} else {
				val = (uint32 *)&iov_buf->data[0];
				PRINT("%d\n", (int)*val);
			}
		}
	} else {
		/* set case handling */
		uint32 inputval = strtoul(*argv++, NULL, 0);
		if (*argv != NULL) {
			ret = BCME_USAGE_ERROR;
			goto finish;
		}
		iov_len += sizeof(val);
		val = (uint32 *)&iov_buf->data[0];
		*val = inputval;
		ret = wlu_iovar_setbuf(wl, "dtpc", (void *)iov_buf, iov_len,
			(void *)iov_buf, WLC_IOCTL_SMLEN);
		if (ret == BCME_OK) {
			PRINT("OK\n");
		}
	}

finish:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;

}

int
wl_dtpc_get_headroom(void *wl, uint32 rspec, uint8 *headroom)
{
	int ret = BCME_OK;
	uint8 buf[WLC_IOCTL_SMLEN];

	if ((ret = wlu_iovar_getbuf(wl, "dtpc_headroom",
		(void*)&rspec, sizeof(uint32), buf, WLC_IOCTL_SMLEN)) < 0) {
		PR_ERR("dtpc_get_headroom: wlu_iovar_getbuf error: %d\n", ret);
		return ret;
	}

	*headroom = (uint8)buf[0];
	return ret;
}

int
wl_dtpc_get_headroom_all(void *wl)
{
	int i, bw, len, max_nss, max_bw, ret = BCME_OK;
	wl_dtpc_cfg_headroom_t dtpc_headroom;
	char *bw_str[] = {"20", "40", "80", "160"};

	if ((ret = wlu_iovar_getbuf(wl, "dtpc_headroom_all", NULL, 0,
		(void*)&dtpc_headroom, sizeof(wl_dtpc_cfg_headroom_t))) < 0) {
		PR_ERR("dtpc_get_headroom_all: wlu_iovar_getbuf error: %d\n", ret);
		return ret;
	}

	len = dtpc_headroom.len;
	max_bw = dtpc_headroom.max_bw;
	max_nss = dtpc_headroom.max_nss;

	if (max_bw < 1 || max_bw > 4)
		return BCME_ERROR;

	PRINT("Rate                  Chains");
	for (bw = 0; bw < max_bw; bw++) {
		PRINT(" %sin%s", bw_str[bw], bw_str[max_bw-1]);
	}
	PRINT("\n");

	for (i = 0; i < len; i += max_bw) {
		PRINT("%-24s%-5d", get_reg_rate_string_from_ratespec(dtpc_headroom.rspec[i]),
			max_nss);
		for (bw = 0; bw < max_bw; bw++) {
			PRINT(" %-7.2f", (float)dtpc_headroom.headroom[i]/4);
		}
		PRINT("\n");
	}
	return ret;
}

static int
wl_dtpc_sub_cmd_en(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	ret = wl_dtpc_sub_cmd_cmn_val(wl, cmd, argv, 0);
	return ret;
}

static int
wl_dtpc_sub_cmd_txsthres(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	ret = wl_dtpc_sub_cmd_cmn_val(wl, cmd, argv, 0);
	return ret;
}

static int
wl_dtpc_sub_cmd_prb_step(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	ret = wl_dtpc_sub_cmd_cmn_val(wl, cmd, argv, 0);
	return ret;
}

static int
wl_dtpc_sub_cmd_alpha(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	ret = wl_dtpc_sub_cmd_cmn_val(wl, cmd, argv, 0);
	return ret;
}

static int
wl_dtpc_sub_cmd_trigint(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	ret = wl_dtpc_sub_cmd_cmn_val(wl, cmd, argv, 0);
	return ret;
}

static int
wl_dtpc_sub_cmd_headroom(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	uint32 rspec;
	uint8 headroom;

	if (*argv == NULL) {
		wl_dtpc_sub_cmd_headroom_help(cmd);
		return BCME_BADARG;
	}

	if (!strncmp(*argv, "all", strlen("all"))) {
		ret = wl_dtpc_get_headroom_all(wl);
	} else {
		rspec = (int)strtol(*argv, NULL, 0);
		if ((ret = wl_dtpc_get_headroom(wl, rspec, &headroom)) < 0) {
			PR_ERR("wl_dtpc_get_headroom failed\n");
			return ret;
		}
		PRINT("%s %d\n", get_reg_rate_string_from_ratespec(rspec), headroom);
	}
	return ret;
}

static int
wl_dtpc_sub_cmd_clmovr(void *wl, const wl_dtpc_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	ret = wl_dtpc_sub_cmd_cmn_val(wl, cmd, argv, 1);
	return ret;
}
