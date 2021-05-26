/*
 * wl esp command module
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
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"
#include <wlioctl.h>
#include <bcmiov.h>
#include <802.11.h>

typedef struct wl_esp_sub_cmd wl_esp_sub_cmd_t;
typedef int (subcmd_handler_t)(void *wl, const wl_esp_sub_cmd_t *cmd,
	char **argv);
typedef void (help_handler_t) (void);
struct wl_esp_sub_cmd {
	char *name;
	uint8 version;
	uint16 cmd_id;
	uint16 type;
	subcmd_handler_t *hndler;
	help_handler_t *help;
};

static cmd_func_t wl_esp_main;

static cmd_t wl_esp_cmds[] = {
	{ "esp", wl_esp_main, WLC_GET_VAR, WLC_SET_VAR,
	"Please follow usage shown above\n"},
	{ NULL, NULL, 0, 0, NULL }
};

/* command handlers */
static subcmd_handler_t wl_esp_sub_cmd_enable;
static subcmd_handler_t wl_esp_sub_cmd_static;

/* help handlers */
static help_handler_t wl_esp_enable_help_fn;
static help_handler_t wl_esp_static_help_fn;

#define WL_ESP_CMD_ALL 0
static const wl_esp_sub_cmd_t esp_subcmd_lists[] = {
	{ "enable", 0x1, WL_ESP_CMD_ENABLE,
	IOVT_BUFFER, wl_esp_sub_cmd_enable,
	wl_esp_enable_help_fn
	},
	{ "static", 0x2, WL_ESP_CMD_STATIC,
	IOVT_BUFFER, wl_esp_sub_cmd_static,
	wl_esp_static_help_fn
	},
	{ NULL, 0, 0, 0, NULL, NULL }
};

void wluc_esp_module_init(void)
{
	/* register esp commands */
	wl_module_cmds_register(wl_esp_cmds);
}

static void
wl_esp_usage(int cmd_id)
{
	const wl_esp_sub_cmd_t *subcmd = &esp_subcmd_lists[0];

	if (cmd_id > (WL_ESP_CMD_LAST - 1)) {
		return;
	}
	while (subcmd->name) {
		if (cmd_id == WL_ESP_CMD_ALL) {
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
wl_esp_main(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	const wl_esp_sub_cmd_t *subcmd = &esp_subcmd_lists[0];

	UNUSED_PARAMETER(cmd);
	/* skip to command name */
	argv++;

	if (*argv) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "help")) {
			wl_esp_usage(WL_ESP_CMD_ALL);
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
	} else {
		wl_esp_usage(WL_ESP_CMD_ALL);
		return BCME_USAGE_ERROR;
	}

	return ret;
}

static void
wl_esp_enable_help_fn(void)
{
	printf("Enable/Disable ESP functionality:\n");
	printf("Set: wl esp enable <0|1>\n");
	printf("Get: wl esp enable\n");
}

static void
wl_esp_static_help_fn(void)
{
	printf("Set ESP static data:\n");
	printf("Set: wl esp static -a <ac_cat> -t <type> -v <value>\n");
	printf("\tac_cat: access category\n");
	printf("\t\t0 = AC_BK\n");
	printf("\t\t1 = AC_BE\n");
	printf("\t\t2 = AC_VI\n");
	printf("\t\t3 = AC_VO\n");
	printf("\ttype: data type\n");
	printf("\t\t0 = data format\n");
	printf("\t\t1 = ba window size\n");
	printf("\t\t2 = estimated air time fraction\n");
	printf("\t\t3 = ppdu duration\n");
	printf("\t\t255 = disable static\n");
}

static int
wl_esp_enable_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_ESP_XTLV_ENABLE:
			printf("Enable: %d\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_esp_prespss_iov_resp_buf(void *iov_resp, uint16 cmd_id, bcm_xtlv_unpack_cbfn_t cbfn)
{
	int ret = BCME_OK;
	uint16 version;

	/* Check for version */
	version = dtoh16(*(uint16 *)iov_resp);
	if (version != WL_ESP_IOV_VERSION) {
		ret = BCME_UNSUPPORTED;
	}
	bcm_iov_buf_t *p_resp = (bcm_iov_buf_t *)iov_resp;
	if (p_resp->id == cmd_id && cbfn != NULL) {
		ret = bcm_unpack_xtlv_buf((void *)p_resp, (uint8 *)p_resp->data, p_resp->len,
			BCM_XTLV_OPTION_ALIGN32, cbfn);
	}
	return ret;
}

static int
wl_esp_get_iov_resp(void *wl, const wl_esp_sub_cmd_t *cmd, bcm_xtlv_unpack_cbfn_t cbfn)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *iov_resp = NULL;

	iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_SMLEN);
	if (iov_buf == NULL) {
		ret = BCME_NOMEM;
		goto fail;
	}
	iov_resp = (uint8 *)calloc(1, WLC_IOCTL_MAXLEN);
	if (iov_resp == NULL) {
		ret = BCME_NOMEM;
		goto fail;
	}
	/* fill header */
	iov_buf->version = WL_ESP_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;

	ret = wlu_iovar_getbuf(wl, "esp", iov_buf, WLC_IOCTL_SMLEN, iov_resp, WLC_IOCTL_MAXLEN);
	if ((ret == BCME_OK) && (iov_resp != NULL)) {
		wl_esp_prespss_iov_resp_buf(iov_resp, cmd->cmd_id, cbfn);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	if (iov_resp) {
		free(iov_resp);
	}
	return ret;
}

static int
wl_esp_sub_cmd_enable(void *wl, const wl_esp_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *val_p = NULL;
	uint16 iovlen = 0;

	if (*argv == NULL) {
		/* get */
		ret = wl_esp_get_iov_resp(wl, cmd, wl_esp_enable_cbfn);
	} else {
		/* set */
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_ESP_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;
		uint8 enable = strtoul(val_p, NULL, 0);
		if ((int8)enable < 0 || (int8)enable > 1) {
			wl_esp_usage(WL_ESP_CMD_ENABLE);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		/* TBD: validation */
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_ESP_XTLV_ENABLE,
				sizeof(enable), &enable, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "esp", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_esp_sub_cmd_static(void *wl, const wl_esp_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;
	uint8 all_params = 0;

	/* only set */
	if (*argv == NULL)
		return BCME_UNSUPPORTED;

	iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
	if (iov_buf == NULL) {
		return BCME_NOMEM;
	}
	/* fill header */
	iov_buf->version = WL_ESP_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;

	pxtlv = (uint8 *)&iov_buf->data[0];
	buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

	/* parse and pack config parameters */
	while ((param = *argv++)) {
		val_p = *argv++;
		if (!val_p || *val_p == '-') {
			fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
			wl_esp_usage(WL_ESP_CMD_STATIC);
			ret = BCME_USAGE_ERROR;
			break;
		}

		if (strcmp(param, "-a") == 0) {
			uint8 ac = strtoul(val_p, NULL, 0);
			ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_ESP_XTLV_STATIC_AC,
				sizeof(ac), &ac, BCM_XTLV_OPTION_ALIGN32);
			if (ret != BCME_OK) {
				break;
			}
			all_params++;
		} else if (strcmp(param, "-t") == 0) {
			uint8 type = strtoul(val_p, NULL, 0);
			ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_ESP_XTLV_STATIC_TYPE,
				sizeof(type), &type, BCM_XTLV_OPTION_ALIGN32);
			if (ret != BCME_OK) {
				break;
			}
			all_params++;
		} else if (strcmp(param, "-v") == 0) {
			uint8 val = strtoul(val_p, NULL, 0);
			ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_ESP_XTLV_STATIC_VAL,
				sizeof(val), &val, BCM_XTLV_OPTION_ALIGN32);
			if (ret != BCME_OK) {
				break;
			}
			all_params++;
		} else {
			fprintf(stderr, "Unknown param %s\n", param);
			break;
		}
	}

	if ((ret != BCME_OK) || !all_params || (all_params == 2)) {
		wl_esp_usage(WL_ESP_CMD_STATIC);
		goto fail;
	}

	iov_buf->len = buflen_start - buflen;
	iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
	ret = wlu_var_setbuf(wl, "esp", (void *)iov_buf, iovlen);

fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}
