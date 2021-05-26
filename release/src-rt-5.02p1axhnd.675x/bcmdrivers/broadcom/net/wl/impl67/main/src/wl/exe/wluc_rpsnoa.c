/*
 * wl rpsnoa command module - radio power save with P2P NOA (Notification of Absence)
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
 * $Id: wluc_rpsnoa.c 783260 2020-01-20 15:12:12Z $
 */

#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"
#include <wlioctl.h>
#include <bcmiov.h>

#define WL_RPSNOA_IOV_MAJOR_VER 1
#define WL_RPSNOA_IOV_MINOR_VER 0
#define WL_RPSNOA_IOV_MAJOR_VER_SHIFT 8
#define WL_RPSNOA_IOV_VERSION \
	((WL_RPSNOA_IOV_MAJOR_VER << WL_RPSNOA_IOV_MAJOR_VER_SHIFT)| WL_RPSNOA_IOV_MINOR_VER)

typedef struct wl_rpsnoa_sub_cmd wl_rpsnoa_sub_cmd_t;

typedef int (subcmd_handler_t)(void *wl, const wl_rpsnoa_sub_cmd_t *cmd,
	char **argv);
typedef void (help_handler_t) (void);

struct wl_rpsnoa_sub_cmd {
	char *name;
	uint8 version;
	uint16 cmd_id;
	uint16 type;
	subcmd_handler_t *hndler;
	help_handler_t *help;
};

static cmd_func_t wl_rpsnoa_main;

static cmd_t wl_rpsnoa_cmds[] = {
	{ "rpsnoa", wl_rpsnoa_main, WLC_GET_VAR, WLC_SET_VAR,
	"Please type wl \"rpsnoa\" for detailed information\n"},
	{ NULL, NULL, 0, 0, NULL }
};

/* command handlers */
static subcmd_handler_t wl_rpsnoa_sub_cmd_enable;
static subcmd_handler_t wl_rpsnoa_sub_cmd_status;
static subcmd_handler_t wl_rpsnoa_sub_cmd_params;

/* help handlers */
static help_handler_t wl_rpsnoa_enable_help_fn;
static help_handler_t wl_rpsnoa_status_help_fn;
static help_handler_t wl_rpsnoa_params_help_fn;

#define WL_RPSNOA_CMD_ALL 0

static const wl_rpsnoa_sub_cmd_t rpsnoa_subcmd_lists[] = {
	{ "enable", 0x1, WL_RPSNOA_CMD_ENABLE,
	IOVT_BUFFER, wl_rpsnoa_sub_cmd_enable,
	wl_rpsnoa_enable_help_fn
	},
	{ "status", 0x1, WL_RPSNOA_CMD_STATUS,
	IOVT_BUFFER, wl_rpsnoa_sub_cmd_status,
	wl_rpsnoa_status_help_fn
	},
	{ "params", 0x1, WL_RPSNOA_CMD_PARAMS,
	IOVT_BUFFER, wl_rpsnoa_sub_cmd_params,
	wl_rpsnoa_params_help_fn
	},
	{ NULL, 0, 0, 0, NULL, NULL }
};

void wluc_rpsnoa_module_init(void)
{
	/* register mbo commands */
	wl_module_cmds_register(wl_rpsnoa_cmds);
}

static void
wl_rpsnoa_usage(int cmd_id)
{
	const wl_rpsnoa_sub_cmd_t *subcmd = &rpsnoa_subcmd_lists[0];

	if (cmd_id > WL_RPSNOA_CMD_LAST - 1)
		return;

	while (subcmd->name) {
		if (cmd_id == WL_RPSNOA_CMD_ALL) {
			subcmd->help();
		} else if (cmd_id == subcmd->cmd_id) {
			subcmd->help();
		} else {
			/* do nothing */
		}
		subcmd++;
	}
	return;
}

static int
wl_rpsnoa_main(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	const wl_rpsnoa_sub_cmd_t *subcmd = &rpsnoa_subcmd_lists[0];

	UNUSED_PARAMETER(cmd);
	/* skip to command name */
	argv++;

	if (*argv) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "help")) {
			wl_rpsnoa_usage(WL_RPSNOA_CMD_ALL);
			return BCME_OK;
		}
		while (subcmd->name) {
			if (strcmp(subcmd->name, *argv) == 0) {
				if (subcmd->hndler) {
					ret = subcmd->hndler(wl, subcmd, ++argv);
					return ret;
				}
			}
			subcmd++;
		}
		if (subcmd->name == NULL)
			return BCME_UNSUPPORTED;
	} else {
		wl_rpsnoa_usage(WL_RPSNOA_CMD_ALL);
		return BCME_OK;
	}

	return ret;
}

static int
wl_rpsnoa_sub_cmd_enable(void *wl, const wl_rpsnoa_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	char *endptr = NULL;
	rpsnoa_iovar_t iov_in;
	rpsnoa_iovar_t *iov_resp = NULL;
	uint32 i;
	uint32 bandtype;

	memset(&iov_in, 0, sizeof(iov_in));

	if (argv == NULL || !str2bandtype(*argv, &bandtype, BND_IS_STRING_INC_BAND_ALL)) {
		ret = BCME_BADARG;
		goto fail;
	}

	iov_in.data->band = bandtype;
	iov_in.hdr.ver = WL_RPSNOA_IOV_VERSION;
	iov_in.hdr.len = sizeof(iov_in);
	iov_in.hdr.subcmd = cmd->cmd_id;
	iov_in.hdr.cnt = 0;

	if (*++argv) {
		iov_in.data->value = strtoul(*argv, &endptr, 0);
		if (*endptr != '\0' || iov_in.data->value > 1) {
			ret = BCME_BADARG;
			goto fail;
		}
		ret = wlu_var_setbuf(wl, wl_rpsnoa_cmds[0].name, (void*)&iov_in, iov_in.hdr.len);
	} else {
		ret = wlu_var_getbuf(wl, wl_rpsnoa_cmds[0].name, (void *)&iov_in,
			iov_in.hdr.len, (void**)&iov_resp);
		if (ret)
			goto fail;

		if (iov_resp->hdr.ver != WL_RPSNOA_IOV_VERSION) {
			ret = BCME_VERSION;
			goto fail;
		}

		if (iov_resp->hdr.subcmd != cmd->cmd_id) {
			ret = BCME_NOTFOUND;
			goto fail;
		}

		if (iov_resp->hdr.cnt < 1) {
			ret = BCME_ERROR;
			goto fail;
		}

		for (i = 0; i < iov_resp->hdr.cnt; i++)
			printf("band : %s, enable %d\n",
				((iov_resp->data[i].band == WLC_BAND_2G) ? "2G" :
				((iov_resp->data[i].band == WLC_BAND_5G) ? "5G" :
				((iov_resp->data[i].band == WLC_BAND_6G) ? "6G" : "BAND ALL"))),
				iov_resp->data[i].value);

	}

fail:
	return ret;
}

static int
wl_rpsnoa_sub_cmd_status(void *wl, const wl_rpsnoa_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	rpsnoa_iovar_t iov_in;
	rpsnoa_iovar_t *iov_resp = NULL;
	uint32 i;
	uint32 bandtype;

	memset(&iov_in, 0, sizeof(iov_in));

	if (argv == NULL || !str2bandtype(*argv, &bandtype, BND_IS_STRING_INC_BAND_ALL)) {
		ret = BCME_BADARG;
		goto fail;
	}

	iov_in.data->band = bandtype;
	iov_in.hdr.ver = WL_RPSNOA_IOV_VERSION;
	iov_in.hdr.len = sizeof(iov_in);
	iov_in.hdr.subcmd = cmd->cmd_id;
	iov_in.hdr.cnt = 0;

	if (*++argv) {
		ret = BCME_BADARG;
		goto fail;
	} else {
		ret = wlu_var_getbuf(wl, wl_rpsnoa_cmds[0].name, (void *)&iov_in,
			iov_in.hdr.len, (void**)&iov_resp);
		if (ret)
			goto fail;

		if (iov_resp->hdr.ver != WL_RPSNOA_IOV_VERSION) {
			ret = BCME_VERSION;
			goto fail;
		}

		if (iov_resp->hdr.subcmd != cmd->cmd_id) {
			ret = BCME_NOTFOUND;
			goto fail;
		}

		if (iov_resp->hdr.cnt < 1) {
			ret = BCME_ERROR;
			goto fail;
		}

		for (i = 0; i < iov_resp->hdr.cnt; i++)
			printf("band : %s, status %d\n",
				((iov_resp->data[i].band == WLC_BAND_2G) ? "2G" :
				((iov_resp->data[i].band == WLC_BAND_5G) ? "5G" :
				((iov_resp->data[i].band == WLC_BAND_6G) ? "6G" : "BAND ALL"))),
				iov_resp->data[i].value);
	}

fail:
	return ret;

}

static int
wl_rpsnoa_sub_cmd_params(void *wl, const wl_rpsnoa_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	rpsnoa_iovar_params_t iov_in;
	rpsnoa_iovar_params_t *iov_resp = NULL;
	char *endptr = NULL;
	uint32 i;
	uint32 bandtype;

	memset(&iov_in, 0, sizeof(iov_in));

	if (argv == NULL || !str2bandtype(*argv, &bandtype, BND_IS_STRING_INC_BAND_ALL)) {
		ret = BCME_BADARG;
		goto fail;
	}

	iov_in.param->band = bandtype;
	iov_in.hdr.ver = WL_RPSNOA_IOV_VERSION;
	iov_in.hdr.len = sizeof(iov_in);
	iov_in.hdr.subcmd = cmd->cmd_id;
	iov_in.hdr.cnt = 0;

	if (*++argv) {
		iov_in.param->pps = strtoul(*argv++, &endptr, 0);
		if (*endptr != '\0') {
			ret = BCME_BADARG;
			goto fail;
		}

		if (*argv == '\0') {
			ret = BCME_BADARG;
			goto fail;
		}

		iov_in.param->quiet_time = strtoul(*argv++, &endptr, 0);
		if (*endptr != '\0') {
			ret = BCME_BADARG;
			goto fail;
		}

		if (*argv == '\0') {
			ret = BCME_BADARG;
			goto fail;
		}

		iov_in.param->level = strtoul(*argv++, &endptr, 0);
		if (*endptr != '\0') {
			ret = BCME_BADARG;
			goto fail;
		}

		if (*argv == '\0') {
			ret = BCME_BADARG;
			goto fail;
		}

		iov_in.param->stas_assoc_check = strtoul(*argv++, &endptr, 0);
		if (*endptr != '\0') {
			ret = BCME_BADARG;
			goto fail;
		}
		ret = wlu_var_setbuf(wl, wl_rpsnoa_cmds[0].name, (void*)&iov_in, iov_in.hdr.len);
	} else {
		ret = wlu_var_getbuf(wl, wl_rpsnoa_cmds[0].name, (void *)&iov_in,
			iov_in.hdr.len, (void**)&iov_resp);
		if (ret)
			goto fail;

		if (iov_resp->hdr.ver != WL_RPSNOA_IOV_VERSION) {
			ret = BCME_VERSION;
			goto fail;
		}

		if (iov_resp->hdr.subcmd != cmd->cmd_id) {
			ret = BCME_NOTFOUND;
			goto fail;
		}

		if (iov_resp->hdr.cnt < 1) {
			ret = BCME_ERROR;
			goto fail;
		}

		for (i = 0; i < iov_resp->hdr.cnt; i++)
			printf("band : %s, pps %d, quiet_time %d, level %d, stas_assoc_check %d\n",
				((iov_resp->param[i].band == WLC_BAND_2G) ? "2G" :
				((iov_resp->param[i].band == WLC_BAND_5G) ? "5G" :
				((iov_resp->param[i].band == WLC_BAND_6G) ? "6G" :"BAND ALL"))),
				iov_resp->param[i].pps, iov_resp->param[i].quiet_time,
				iov_resp->param[i].level, iov_resp->param[i].stas_assoc_check);
	}
fail:
	return ret;
}

static void
wl_rpsnoa_enable_help_fn(void)
{
	printf("\n");
	printf("wl rpsnoa enable [band] [value]\n");
	printf("-enable and disable radio power save, when value is not specified, ");
	printf("works as get function\n");
	printf("\tband: AP radio power save on specified band\n");
	printf("\t\t b or 2g = 2.4GHz\n");
	printf("\t\t a or 5g = 5GHz\n");
	printf("\t\t 6g = 6GHz\n");
	printf("\t\t all = 2.4GHz, 5GHz and 6GHz\n");
	printf("\tvalue: Enable/Disable AP radio power save with NOA\n");
	printf("\t\t 1 = Enable power save\n");
	printf("\t\t 0 = Disable power save\n");
	printf("\t\t not specified : returns 1 when radio power save is enabled, ");
	printf("otherwise 0\n");
	printf("\n");
}

static void
wl_rpsnoa_status_help_fn(void)
{
	printf("\n");
	printf("wl rpsnoa status [band]\n");
	printf("-Gets current radio power save activity\n");
	printf("\tband: AP radio power save on specified band\n");
	printf("\t\t b or 2g = 2.4GHz\n");
	printf("\t\t a or 5g = 5GHz\n");
	printf("\t\t 6g = 6GHz\n");
	printf("\t\t all = 2.4GHz, 5GHz and 6GHz\n");
	printf("\n");
}

static void
wl_rpsnoa_params_help_fn(void)
{
	printf("\n");
	printf("wl rpsnoa params [band] [pps] [quiet] [level] [assoc check]\n");
	printf("-sets radio power save parameters, when [band] [pps] [level] ");
	printf("[assoc check] is not specified, works as get function\n");
	printf("\tband: AP radio power save on specified band\n");
	printf("\t\t b or 2g = 2.4GHz\n");
	printf("\t\t a or 5g = 5GHz\n");
	printf("\t\t 6g = 6GHz\n");
	printf("\t\t all = 2.4GHz, 5GHz and 6GHz\n");
	printf("\tpps: packet per sec\n");
	printf("\tquiet: quiet time\n");
	printf("\tlevel: amount of time to sleep, min is 1, max is 5\n");
	printf("\tassoc check: if assoc check is 1, radio power save is deactivated ");
	printf("when STA is associated\n");
	printf("\n");
}
