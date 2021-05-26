/*
 * wl fils command module
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
 * $Id: wluc_fils.c 684714 2017-02-14 07:45:24Z $
 */
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"
#include <wlioctl.h>
#include <bcmiov.h>
#include <oce.h>

typedef struct wl_oce_sub_cmd wl_oce_sub_cmd_t;
typedef int (subcmd_handler_t)(void *wl, const wl_oce_sub_cmd_t *cmd,
	char **argv);
typedef void (help_handler_t) (void);
struct wl_oce_sub_cmd {
	char *name;
	uint8 version;
	uint16 cmd_id;
	uint16 type;
	subcmd_handler_t *hndler;
	help_handler_t *help;
};

static cmd_func_t wl_oce_main;

static cmd_t wl_oce_cmds[] = {
	{ "oce", wl_oce_main, WLC_GET_VAR, WLC_SET_VAR,
	"Please follow usage shown above\n"},
	{ NULL, NULL, 0, 0, NULL }
};

/* command handlers */
static subcmd_handler_t wl_oce_sub_cmd_enable;
static subcmd_handler_t wl_oce_sub_cmd_probe_def_time;
static subcmd_handler_t wl_oce_sub_cmd_fd_tx_period;
static subcmd_handler_t wl_oce_sub_cmd_fd_tx_duration;
static subcmd_handler_t wl_oce_sub_cmd_rssi_th;
static subcmd_handler_t wl_oce_sub_cmd_rwan_links;
static subcmd_handler_t wl_oce_sub_cmd_retry_delay;

/* help handlers */
static help_handler_t wl_oce_enable_help_fn;
static help_handler_t wl_oce_probe_def_time_help_fn;
static help_handler_t wl_oce_fd_tx_period_help_fn;
static help_handler_t wl_oce_fd_tx_duration_help_fn;
static help_handler_t wl_oce_rssi_th_help_fn;
static help_handler_t wl_oce_rwan_links_help_fn;
static help_handler_t wl_oce_retry_delay_help_fn;

#define WL_OCE_CMD_ALL 0
static const wl_oce_sub_cmd_t oce_subcmd_lists[] = {
	{ "enable", 0x1, WL_OCE_CMD_ENABLE,
	IOVT_BUFFER, wl_oce_sub_cmd_enable,
	wl_oce_enable_help_fn
	},
	{ "probe_def_time", 0x1, WL_OCE_CMD_PROBE_DEF_TIME,
	IOVT_BUFFER, wl_oce_sub_cmd_probe_def_time,
	wl_oce_probe_def_time_help_fn
	},
	{ "fd_tx_period", 0x1, WL_OCE_CMD_FD_TX_PERIOD,
	IOVT_BUFFER, wl_oce_sub_cmd_fd_tx_period,
	wl_oce_fd_tx_period_help_fn
	},
	{ "fd_tx_duration", 0x1, WL_OCE_CMD_FD_TX_DURATION,
	IOVT_BUFFER, wl_oce_sub_cmd_fd_tx_duration,
	wl_oce_fd_tx_duration_help_fn
	},
	{ "rssi_th", 0x1, WL_OCE_CMD_RSSI_TH,
	IOVT_BUFFER, wl_oce_sub_cmd_rssi_th,
	wl_oce_rssi_th_help_fn
	},
	{ "rwan_links", 0x1, WL_OCE_CMD_RWAN_LINKS,
	IOVT_BUFFER, wl_oce_sub_cmd_rwan_links,
	wl_oce_rwan_links_help_fn
	},
	{ "retry_delay", 0x1, WL_OCE_CMD_RETRY_DELAY,
	IOVT_BUFFER, wl_oce_sub_cmd_retry_delay,
	wl_oce_retry_delay_help_fn
	},
	{ NULL, 0, 0, 0, NULL, NULL }
};

void wluc_oce_module_init(void)
{
	/* register oce commands */
	wl_module_cmds_register(wl_oce_cmds);
}

static void
wl_oce_usage(int cmd_id)
{
	const wl_oce_sub_cmd_t *subcmd = &oce_subcmd_lists[0];

	if (cmd_id > (WL_OCE_CMD_LAST - 1)) {
		return;
	}
	while (subcmd->name) {
		if (cmd_id == WL_OCE_CMD_ALL) {
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
wl_oce_main(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	const wl_oce_sub_cmd_t *subcmd = &oce_subcmd_lists[0];

	UNUSED_PARAMETER(cmd);
	/* skip to command name */
	argv++;

	if (*argv) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "help")) {
			wl_oce_usage(WL_OCE_CMD_ALL);
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
		wl_oce_usage(WL_OCE_CMD_ALL);
		return BCME_USAGE_ERROR;
	}

	return ret;
}

static void
wl_oce_enable_help_fn(void)
{
	printf("Enable/Disable OCE functionality:\n");
	printf("Set: wl oce enable <value>\n");
	printf("Get: wl oce enable\n");
}

static void
wl_oce_retry_delay_help_fn(void)
{
	printf("Set/Get retry delay in seconds to ignore association request\n");
	printf("Set: wl oce retry_delay <value>\n");
	printf("Get: wl oce retry_delay\n");
}

static void
wl_oce_probe_def_time_help_fn(void)
{
	printf("Probe Request Deferral Time:\n");
	printf("Set: wl oce probe_def_time <value>\n");
	printf("Get: wl oce probe_def_time\n");
}

static void
wl_oce_fd_tx_period_help_fn(void)
{
	printf("FILS Discovery Frame TX period:\n");
	printf("Set: wl oce fd_tx_period <range: %d ms to %d ms>\n", OCE_MIN_FD_TX_PERIOD,
		OCE_MAX_FD_TX_PERIOD);
	printf("Get: wl oce fd_tx_period\n");
}

static void
wl_oce_fd_tx_duration_help_fn(void)
{
	printf("FILS Discovery Frame TX duration:\n");
	printf("Set: wl oce fd_tx_duration <value>\n");
	printf("Get: wl oce fd_tx_duration\n");
}

static void
wl_oce_rssi_th_help_fn(void)
{
	printf("RSSI Threashold:\n");
	printf("Set: wl oce rssi_th <range: %d to %d>\n", OCE_MAX_RSSI_TH, OCE_MIN_RSSI_TH);
	printf("Get: wl oce rssi_th\n");
}

static void
wl_oce_rwan_links_help_fn(void)
{
	printf("Reduced WAN Metrics links:\n");
	printf("Set: wl oce rwan_links <value>\n");
	printf("Get: wl oce rwan_links\n");
}

static int
wl_oce_enable_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_OCE_XTLV_ENABLE:
			printf("Enable: %d\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_oce_retry_delay_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_OCE_XTLV_RETRY_DELAY:
			printf("retry delay: %d\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_oce_probe_def_time_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_OCE_XTLV_PROBE_DEF_TIME:
			printf("Probe Deferral Time: %d\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_oce_fd_tx_period_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_OCE_XTLV_FD_TX_PERIOD:
			printf("FD TX Period: %d\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_oce_fd_tx_duration_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_OCE_XTLV_FD_TX_DURATION:
			printf("FD TX Duration: %d\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_oce_rssi_th_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);

	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_OCE_XTLV_RSSI_TH:
			printf("RSSI Threashold: %d\n", (int8)(*data));
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_oce_rwan_links_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);

	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_OCE_XTLV_RWAN_LINKS:
			printf("Reduced WAN Metrics : uplink 0x%02x, downlink 0x%02x\n",
				((uint8)(*data)>>4)&0xf, ((uint8)(*data))&0xf);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_oce_process_iov_resp_buf(void *iov_resp, uint16 cmd_id, bcm_xtlv_unpack_cbfn_t cbfn)
{
	int ret = BCME_OK;
	uint16 version;

	/* Check for version */
	version = dtoh16(*(uint16 *)iov_resp);
	if (version != WL_OCE_IOV_VERSION) {
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
wl_oce_get_iov_resp(void *wl, const wl_oce_sub_cmd_t *cmd, bcm_xtlv_unpack_cbfn_t cbfn)
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
	iov_buf->version = WL_OCE_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;

	ret = wlu_iovar_getbuf(wl, "oce", iov_buf, WLC_IOCTL_SMLEN, iov_resp, WLC_IOCTL_MAXLEN);
	if ((ret == BCME_OK) && (iov_resp != NULL)) {
		wl_oce_process_iov_resp_buf(iov_resp, cmd->cmd_id, cbfn);
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
wl_oce_sub_cmd_retry_delay(void *wl, const wl_oce_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;
	uint8 retry_delay = 0;

	/* only set */
	if (*argv == NULL) {
		ret = wl_oce_get_iov_resp(wl, cmd, wl_oce_retry_delay_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_OCE_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;
		if (!val_p || *val_p == '-') {
			fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
			wl_oce_usage(WL_OCE_CMD_RETRY_DELAY);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		retry_delay = strtoul(val_p, NULL, 0);
		/* TBD: validation */
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_OCE_XTLV_RETRY_DELAY,
				sizeof(retry_delay), &retry_delay, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "oce", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_oce_sub_cmd_enable(void *wl, const wl_oce_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* only set */
	if (*argv == NULL) {
		ret = wl_oce_get_iov_resp(wl, cmd, wl_oce_enable_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_OCE_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;
		if (!val_p || *val_p == '-') {
			fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
			wl_oce_usage(WL_OCE_CMD_ENABLE);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		uint8 enable = strtoul(val_p, NULL, 0);
		/* TBD: validation */
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_OCE_XTLV_ENABLE,
				sizeof(enable), &enable, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "oce", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_oce_sub_cmd_probe_def_time(void *wl, const wl_oce_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* only set */
	if (*argv == NULL) {
		ret = wl_oce_get_iov_resp(wl, cmd, wl_oce_probe_def_time_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_OCE_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;
		if (!val_p || *val_p == '-') {
			fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
			wl_oce_usage(WL_OCE_CMD_PROBE_DEF_TIME);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		uint8 prb_def_time = strtoul(val_p, NULL, 0);
		/* TBD: validation */
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_OCE_XTLV_PROBE_DEF_TIME,
				sizeof(prb_def_time), &prb_def_time, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "oce", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_oce_sub_cmd_fd_tx_period(void *wl, const wl_oce_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* only set */
	if (*argv == NULL) {
		ret = wl_oce_get_iov_resp(wl, cmd, wl_oce_fd_tx_period_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_OCE_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;
		if (!val_p || *val_p == '-') {
			fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
			wl_oce_usage(WL_OCE_CMD_FD_TX_PERIOD);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		uint8 fd_tx_period = strtoul(val_p, NULL, 0);

		if (fd_tx_period && ((fd_tx_period < OCE_MIN_FD_TX_PERIOD) ||
			(fd_tx_period > OCE_MAX_FD_TX_PERIOD))) {
			ret = BCME_USAGE_ERROR;
			goto fail;
		}

		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_OCE_XTLV_FD_TX_PERIOD,
				sizeof(fd_tx_period), &fd_tx_period, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "oce", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_oce_sub_cmd_fd_tx_duration(void *wl, const wl_oce_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* only set */
	if (*argv == NULL) {
		ret = wl_oce_get_iov_resp(wl, cmd, wl_oce_fd_tx_duration_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_OCE_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;
		if (!val_p || *val_p == '-') {
			fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
			wl_oce_usage(WL_OCE_CMD_FD_TX_DURATION);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		uint8 fd_tx_duration = strtoul(val_p, NULL, 0);
		/* TBD: validation */
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_OCE_XTLV_FD_TX_DURATION,
				sizeof(fd_tx_duration), &fd_tx_duration, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "oce", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_oce_sub_cmd_rssi_th(void *wl, const wl_oce_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *val_p = NULL;
	uint16 iovlen = 0;

	/* only set */
	if (*argv == NULL) {
		ret = wl_oce_get_iov_resp(wl, cmd, wl_oce_rssi_th_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_OCE_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;

		int8 rssi_th = strtoul(val_p, NULL, 0);

		if (rssi_th && ((rssi_th < OCE_MIN_RSSI_TH) || (rssi_th > OCE_MAX_RSSI_TH))) {
			wl_oce_usage(WL_OCE_CMD_RSSI_TH);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}

		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_OCE_XTLV_RSSI_TH,
				sizeof(rssi_th), (void *)&rssi_th, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "oce", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_oce_sub_cmd_rwan_links(void *wl, const wl_oce_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* only set */
	if (*argv == NULL) {
		ret = wl_oce_get_iov_resp(wl, cmd, wl_oce_rwan_links_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_OCE_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		val_p = *argv++;
		if (!val_p || *val_p == '-') {
			fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
			wl_oce_usage(WL_OCE_CMD_RWAN_LINKS);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		uint8 rwan_links = strtoul(val_p, NULL, 0);
		/* TBD: validation */
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_OCE_XTLV_RWAN_LINKS,
				sizeof(rwan_links), &rwan_links, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;

		ret = wlu_var_setbuf(wl, "oce", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}
