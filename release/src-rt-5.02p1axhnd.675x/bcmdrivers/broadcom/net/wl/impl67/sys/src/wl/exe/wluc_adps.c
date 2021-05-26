/*
 * wl adps command module -  Adaptive Power Save - Legacy
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
 * $Id: wluc_adps.c 783260 2020-01-20 15:12:12Z $
 */

#include <wlioctl.h>
#include <wlioctl_utils.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmiov.h>
#include <wlc_adps_internal.h>
#include "wlu_common.h"
#include "wlu.h"

#define ADPS_MODE_OFF		0
#define ADPS_STEP_ELEMENT_NUM	3

#define ADPS_CMD_NAME		"adps"
#define ADPS_SUB_CMD_VER	0x01

#define ADPS_FLAG_BIT_MASK	0x01

typedef struct wl_adps_sub_cmd wl_adps_sub_cmd_t;
typedef int (sub_cmd_handler_t)(void *wl, const wl_adps_sub_cmd_t *cmd, char **argv);
typedef void (sub_cmd_help_handler_t)(void);
struct wl_adps_sub_cmd {
	char *name;
	uint16 version;
	uint16 id;
	uint16 type;
	sub_cmd_handler_t *handler;
	sub_cmd_help_handler_t *help;
};

static cmd_func_t wl_adps_main;

static cmd_t wl_adps_cmds[] = {
	{"adps", wl_adps_main, WLC_GET_VAR, WLC_SET_VAR, ""},
	{NULL, NULL, 0, 0, NULL}
};

/* sub command handler */
static sub_cmd_handler_t wl_adps_sub_cmd_mode;
static sub_cmd_handler_t wl_adps_sub_cmd_dump;
static sub_cmd_handler_t wl_adps_sub_cmd_dump_clear;
static sub_cmd_handler_t wl_adps_sub_cmd_rssi;

#define ADPS_HELP_ALL	0
/* sub command help handler */
static sub_cmd_help_handler_t wl_adps_sub_cmd_help_mode_fn;
static sub_cmd_help_handler_t wl_adps_sub_cmd_help_dump_fn;
static sub_cmd_help_handler_t wl_adps_sub_cmd_help_dump_clear_fn;
static sub_cmd_help_handler_t wl_adps_sub_cmd_help_rssi_fn;

static const wl_adps_sub_cmd_t wl_adps_sub_cmd_list[] = {
	{"mode", WL_ADPS_IOV_VER, WL_ADPS_IOV_MODE, IOVT_BUFFER,
	wl_adps_sub_cmd_mode, wl_adps_sub_cmd_help_mode_fn
	},
	{"dump", WL_ADPS_IOV_VER, WL_ADPS_IOV_DUMP, IOVT_BUFFER,
	wl_adps_sub_cmd_dump, wl_adps_sub_cmd_help_dump_fn
	},
	{"dump_clear", WL_ADPS_IOV_VER, WL_ADPS_IOV_DUMP_CLEAR, IOVT_BUFFER,
	wl_adps_sub_cmd_dump_clear, wl_adps_sub_cmd_help_dump_clear_fn
	},
	{"rssi", WL_ADPS_IOV_VER, WL_ADPS_IOV_RSSI, IOVT_BUFFER,
	wl_adps_sub_cmd_rssi, wl_adps_sub_cmd_help_rssi_fn
	},
	{ NULL, 0, 0, 0, NULL, NULL }
};

static char *buf;

/* module initialization */
void
wluc_adps_module_init(void)
{
	/* get thr global buf */
	buf = wl_get_buf();

	/* register adps commands */
	wl_module_cmds_register(wl_adps_cmds);
}

static void
wl_adps_help_all()
{
	const wl_adps_sub_cmd_t *sub_cmd = &wl_adps_sub_cmd_list[0];

	while (sub_cmd->name) {
		printf("%s - ", sub_cmd->name);
		sub_cmd->help();
		sub_cmd++;
	}
}

static void
wl_adps_help(uint16 type)
{
	const wl_adps_sub_cmd_t *sub_cmd = &wl_adps_sub_cmd_list[0];

	if (type == ADPS_HELP_ALL) {
		wl_adps_help_all();
		return;
	}

	while (sub_cmd->name) {
		if (sub_cmd->type == type) {
			if (sub_cmd->help) {
				printf("%s - ", sub_cmd->name);
				sub_cmd->help();
				break;
			}
		}
		sub_cmd++;
	}
}

static int
wl_adps_validate_bcm_iov_buf(const bcm_iov_buf_t *iov_buf, uint16 version, uint16 id)
{
	if (iov_buf->version != version) {
		printf("Wrong version (%x/%x)\n", iov_buf->version, version);
		return BCME_VERSION;
	}

	if (iov_buf->id != id) {
		printf("Invalid id\n");
		return BCME_ERROR;
	}

	return BCME_OK;
}

static int
wl_adps_sub_cmd_mode(void *wl, const wl_adps_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;

	void *ptr = NULL;
	wl_adps_params_v1_t *data = NULL;
	bcm_iov_buf_t *iov_buf = NULL;
	bcm_iov_buf_t *resp = NULL;

	uint8 band;
	uint32 band_type;
	uint8 *pdata = NULL;

	int buf_size = OFFSETOF(bcm_iov_buf_t, data);
	int alloc_len = buf_size + sizeof(*data);

	if ((iov_buf = (bcm_iov_buf_t *)calloc(1, alloc_len)) == NULL) {
		fprintf(stderr, "Error allocating %u bytes for adps dump\n",
			(unsigned int)WLC_IOCTL_MEDLEN);
		return BCME_NOMEM;
	}

	iov_buf->version = WL_ADPS_IOV_VER;
	iov_buf->id = cmd->id;

	if (*argv == NULL || !str2bandtype(*argv, &band_type, BND_IS_STRING)) {
		ret = BCME_BADARG;
		goto exit;
	}

	band = band_type;

	if (!*++argv) {
		pdata = (uint8 *)iov_buf->data;
		*pdata++ = band;
		buf_size += sizeof(band);
		iov_buf->len = sizeof(band);
		if ((ret = wlu_var_getbuf(wl, ADPS_CMD_NAME,
			(void *)iov_buf, buf_size, &ptr)) != BCME_OK) {
			printf("Fail to get adps mode (%d)\n", ret);
			goto exit;
		}

		resp = (bcm_iov_buf_t *)ptr;
		if ((ret = wl_adps_validate_bcm_iov_buf(resp,
				WL_ADPS_IOV_VER, iov_buf->id)) != BCME_OK) {
			printf("Invalid bcm iov buf\n");
			goto exit;
		}

		data = (wl_adps_params_v1_t *)resp->data;
		printf("%s\n", data->mode > 0 ? "On" : "Off");
	}
	else {
		data = (wl_adps_params_v1_t *)iov_buf->data;

		data->version = ADPS_SUB_IOV_VERSION_1;
		data->length = sizeof(*data);

		data->band = band;
		data->mode = strtoul(*argv, NULL, 0);
		if (*++argv) {
			ret = BCME_BADARG;
			goto exit;
		}

		buf_size += sizeof(*data);
		iov_buf->len = sizeof(*data);
		ret = wlu_var_setbuf(wl, ADPS_CMD_NAME, (void *)iov_buf, buf_size);
	}
exit:
	if (iov_buf) {
		free(iov_buf);
	}

	return ret;
}

static int
wl_adps_sub_cmd_rssi(void *wl, const wl_adps_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	int buf_size = OFFSETOF(bcm_iov_buf_t, data);
	int alloc_len = OFFSETOF(bcm_iov_buf_t, data) + sizeof(wl_adps_rssi_params_v1_t);

	void *ptr = NULL;
	char *endptr = NULL;
	wl_adps_rssi_params_v1_t *data = NULL;

	bcm_iov_buf_t *iov_buf = NULL;
	bcm_iov_buf_t *resp = NULL;
	int argc = ARGCNT(argv);

	uint8 band;
	uint8 *pdata = NULL;

	if ((iov_buf = (bcm_iov_buf_t *)calloc(1, alloc_len)) == NULL) {
		fprintf(stderr, "Error allocating %u bytes for adps dump\n",
			(unsigned int)WLC_IOCTL_MEDLEN);
		return BCME_NOMEM;
	}

	iov_buf->version = WL_ADPS_IOV_VER;
	iov_buf->id = cmd->id;

	data = (wl_adps_rssi_params_v1_t *)iov_buf->data;
	if (*argv && (!stricmp(*argv, "b") || !stricmp(*argv, "2g"))) {
		band = WLC_BAND_2G;
	} else if (*argv && (!stricmp(*argv, "a") || !stricmp(*argv, "5g"))) {
		band = WLC_BAND_5G;
	} else {
		ret = BCME_BADARG;
		goto exit;
	}

	if (!*++argv) {
		pdata = (uint8 *)iov_buf->data;
		*pdata++ = band;
		buf_size += sizeof(band);
		iov_buf->len = sizeof(band);
		if ((ret = wlu_var_getbuf(wl, ADPS_CMD_NAME,
			(void *)iov_buf, buf_size, &ptr)) != BCME_OK) {
			printf("Fail to get adps mode (%d)\n", ret);
			goto exit;
		}

		resp = (bcm_iov_buf_t *)ptr;
		if ((ret = wl_adps_validate_bcm_iov_buf(resp,
				WL_ADPS_IOV_VER, iov_buf->id)) != BCME_OK) {
			printf("Invalid bcm iov buf\n");
			goto exit;
		}

		data = (wl_adps_rssi_params_v1_t *)resp->data;

		printf("High threshold: %d\n", data->rssi.thresh_hi);
		printf("Low threshold: %d\n", data->rssi.thresh_lo);
	}
	else {
		data->version = ADPS_SUB_IOV_VERSION_1;
		data->band = band;

		if (argc != 3) {
			ret = BCME_USAGE_ERROR;
			goto exit;
		}

		data->rssi.thresh_hi = strtoul(*argv++, &endptr, 0);
		if (*endptr != '\0') {
			ret = BCME_BADARG;
			goto exit;
		}

		data->rssi.thresh_lo = strtoul(*argv++, &endptr, 0);
		if (*endptr != '\0') {
			ret = BCME_BADARG;
			goto exit;
		}

		data->length = sizeof(*data);
		iov_buf->len = sizeof(*data);
		buf_size += iov_buf->len;
		ret = wlu_var_setbuf(wl, ADPS_CMD_NAME, (void *)iov_buf, buf_size);
	}
exit:
	if (iov_buf) {
		free(iov_buf);
	}

	return ret;
}

static void
wl_adps_dump_print_flag_string(uint8 flags)
{
	bool print;

	uint8 mask;
	uint8 shift = 0;
	uint8 remained = flags;

	while (remained) {
		print = FALSE;
		mask = (ADPS_FLAG_BIT_MASK << shift) & flags;
		switch (mask) {
		case OPERATION_PAUSE:
			printf("Pause");
			print = TRUE;
			break;
		case WEAK_SIGNAL:
			printf("Weak Singal");
			print = TRUE;
			break;
		case BTCOEX_ACTIVE:
			printf("BTCoex Active");
			print = TRUE;
			break;
		default:
			break;
		}

		remained >>= ADPS_FLAG_BIT_MASK;
		if (print && remained > 0) {
			printf(" | ");
		}
		shift++;
	}
}

static int
wl_adps_dump_print(bcm_tlv_t *tlv)
{
	int i;
	int ret = BCME_OK;

	switch (tlv->id) {
		case WL_ADPS_IOV_DUMP_CMD_SUMMARY: {
			wl_adps_dump_summary_v1_t *summary =
				(wl_adps_dump_summary_v1_t *)tlv->data;
			if (tlv->len < OFFSETOF(wl_adps_dump_summary_v1_t, flags)) {
				printf("Id: %x - invaild length (%d)\n", tlv->id, tlv->len);
				ret = BCME_BADLEN;
				break;
			}

			if (summary->mode == ADPS_MODE_OFF) {
				ret = BCME_NOTUP;
				break;
			}

			printf("ADPS Mode = On, Current ADPS PM = %s, ",
				summary->current_step ? "High" : "Low");
			printf("Flags = 0x%02x", summary->flags);
			if (summary->flags & ADPS_FLAGS_MASK) {
				printf(" (");
				wl_adps_dump_print_flag_string(summary->flags);
				printf(")\n");
			}
			else {
				printf("\n");
			}

			printf("ADPS PM Dur (Low/High): %d/%d (Counter: %d/%d)\n",
				summary->stat[0].duration, summary->stat[1].duration,
				summary->stat[0].counts, summary->stat[1].counts);

			break;
		}
		default: {
			printf("Dump 0x%x: len = %d\n", tlv->id, tlv->len);
			for (i = 0; i < tlv->len; i++) {
				printf("%02x", tlv->data[i]);
			}
			printf("\n");
			ret = BCME_BADARG;

			break;
		}
	}

	return ret;
}

static int
wl_adps_sub_cmd_dump(void *wl, const wl_adps_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	uint8 dump_sub_cmd = WL_ADPS_IOV_DUMP_CMD_SUMMARY;
	int buf_size = OFFSETOF(bcm_iov_buf_t, data) + sizeof(dump_sub_cmd);

	void *ptr = NULL;
	uint8 *data = NULL;
	bcm_iov_buf_t *iov_buf = NULL;
	bcm_iov_buf_t *resp = NULL;

	int remaining_bytes;
	bcm_tlv_t *tlv;

	UNUSED_PARAMETER(argv);

	if ((iov_buf = (bcm_iov_buf_t *)calloc(1, buf_size)) == NULL) {
		fprintf(stderr, "Error allocating %u bytes for adps dump\n",
			(unsigned int)WLC_IOCTL_MEDLEN);
		return BCME_NOMEM;
	}

	iov_buf->version = WL_ADPS_IOV_VER;
	iov_buf->len = sizeof(dump_sub_cmd);
	iov_buf->id = cmd->id;

	data = (uint8 *)iov_buf->data;
	data[0] = dump_sub_cmd;

	if ((ret = wlu_var_getbuf(wl, ADPS_CMD_NAME,
			(void *)iov_buf, buf_size, &ptr)) != BCME_OK) {
		printf("Fail to get adps dump (%d)\n", ret);
		goto exit;
	}

	resp = (bcm_iov_buf_t *)ptr;

	if (resp->version != WL_ADPS_IOV_VER) {
		printf("Wrong version\n");
		ret = BCME_VERSION;
		goto exit;
	}

	if (resp->id != cmd->id) {
		printf("Invalid id\n");
		ret = BCME_ERROR;
		goto exit;
	}

	remaining_bytes = resp->len;
	tlv = (bcm_tlv_t *)resp->data;

	if (!bcm_valid_tlv(tlv, remaining_bytes)) {
		printf("Invalid tlv\n");
		ret = BCME_BADLEN;
		goto exit;
	}

	if (tlv->data[0] == ADPS_MODE_OFF) {
		printf("ADPS Status: Off\n");
	}
	else {
		while (tlv) {
			ret = wl_adps_dump_print(tlv);
			if (ret == BCME_NOTUP) {
				printf("ADPS Status: Off");
				ret = BCME_OK;
				break;
			}
			tlv = bcm_next_tlv(tlv, &remaining_bytes);
		}
	}

exit:
	if (iov_buf) {
		free(iov_buf);
	}

	return ret;
}

static int
wl_adps_sub_cmd_dump_clear(void *wl, const wl_adps_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	int buf_size = OFFSETOF(bcm_iov_buf_t, data);

	UNUSED_PARAMETER(argv);

	bcm_iov_buf_t *iov_buf = NULL;

	if ((iov_buf = (bcm_iov_buf_t *)calloc(1, buf_size)) == NULL) {
		fprintf(stderr, "Error allocating %u bytes for adps dump\n",
			(unsigned int)WLC_IOCTL_MEDLEN);
		return BCME_NOMEM;
	}

	iov_buf->version = WL_ADPS_IOV_VER;
	iov_buf->len = 0;
	iov_buf->id = cmd->id;

	ret = wlu_var_setbuf(wl, ADPS_CMD_NAME, (void *)iov_buf, buf_size);

	if (iov_buf) {
		free(iov_buf);
	}

	return ret;
}

#define ADPS_HELP_ALL	0
/* sub command help handler */
static void
wl_adps_sub_cmd_help_mode_fn(void)
{
	printf("Enable/Disable ADaptive Power Save\n");
	printf("Usage: wl adps mode [band] <mode>\n");
	printf("\t[band] a|b|2g|5g\n");
	printf("\t<mode> 0: Off, 1: On\n");
}

static void
wl_adps_sub_cmd_help_dump_fn(void)
{
	printf("Dump ADaptive Power Save status\n");
	printf("\tUsage: wl adps dump\n");
}

static void
wl_adps_sub_cmd_help_dump_clear_fn(void)
{
	printf("Clear ADaptive Power Save dump stat\n");
	printf("Usage: wl adps dump_clear\n");
}

static void
wl_adps_sub_cmd_help_rssi_fn(void)
{
	printf("Get/Set adps operation pause rssi threshold\n");
	printf("Usage: wl adps rssi [band] <high rssi threshold> <low rssi threshold>\n");
	printf("\t[band] a|b|2g|5g\n");
	printf("\t<high rssi threshold> rssi value to resume adps operation\n");
	printf("\t<low rssi threshold> rssi value to pause adps operation\n");
}

static const wl_adps_sub_cmd_t*
wl_adps_get_sub_cmd_info(char **argv)
{
	char *cmd_name = *argv;
	const wl_adps_sub_cmd_t *p_sub_cmd = &wl_adps_sub_cmd_list[0];

	while (p_sub_cmd->name) {
		if (stricmp(p_sub_cmd->name, cmd_name) == 0) {
			return p_sub_cmd;
		}
		p_sub_cmd++;
	}

	return NULL;
}

static int
wl_adps_main(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_USAGE_ERROR;
	const wl_adps_sub_cmd_t *sub_cmd = NULL;

	UNUSED_PARAMETER(cmd);

	/* skip to command name */
	argv++;

	if (*argv) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "help"))  {
			wl_adps_help(ADPS_HELP_ALL);
			return BCME_OK;
		}

		sub_cmd = wl_adps_get_sub_cmd_info(argv);
		if (sub_cmd == NULL) {
			wl_adps_help(ADPS_HELP_ALL);
			return BCME_OK;
		}

		if (sub_cmd->handler) {
			/* skip to sub command name */
			ret = sub_cmd->handler(wl, sub_cmd, ++argv);
		}
	}

	return ret;
}
