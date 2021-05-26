/*
 * wl mbo command module
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
 * $Id: wluc_mbo.c 790487 2020-08-26 07:34:45Z $
 */
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"
#include <wlioctl.h>
#include <bcmiov.h>
#include <mbo.h>

/* from bcmiov.h: non-batched command version = major|minor w/ major <= 127 */
#define WL_MBO_IOV_MAJOR_VER 1
#define WL_MBO_IOV_MINOR_VER 1
#define WL_MBO_IOV_MAJOR_VER_SHIFT 8
#define WL_MBO_IOV_VERSION \
	((WL_MBO_IOV_MAJOR_VER << WL_MBO_IOV_MAJOR_VER_SHIFT)| WL_MBO_IOV_MINOR_VER)

#define WL_MBO_CELLULAR_DATA_AWARE_AP	((0X01) << 6)
#define MBO_ASSOC_DISALLOWED_DISABLE				0x00
#define MBO_ASSOC_DISALLOWED_REASON_UNSPECIFIED			0x01
#define MBO_ASSOC_DISALLOWED_REASON_MAX_STA_LIMIT_REACHED	0x02
#define MBO_ASSOC_DISALLOWED_REASON_AIR_INTERFACE_OVERLOAD	0x03
#define MBO_ASSOC_DISALLOWED_REASON_AUTH_SERVER_OVERLOAD	0x04
#define MBO_ASSOC_DISALLOWED_REASON_INSUFFICIENT_RSSI		0x05

typedef struct wl_mbo_sub_cmd wl_mbo_sub_cmd_t;
typedef int (subcmd_handler_t)(void *wl, const wl_mbo_sub_cmd_t *cmd,
	char **argv);
typedef void (help_handler_t) (void);
struct wl_mbo_sub_cmd {
	char *name;
	uint8 version;
	uint16 cmd_id;
	uint16 type;
	subcmd_handler_t *hndler;
	help_handler_t *help;
};

static cmd_func_t wl_mbo_main;

static cmd_t wl_mbo_cmds[] = {
	{ "mbo", wl_mbo_main, WLC_GET_VAR, WLC_SET_VAR,
	"Please follow usage shown above\n"},
	{ NULL, NULL, 0, 0, NULL }
};

/* command handlers */
static subcmd_handler_t wl_mbo_sub_cmd_add_chan_pref;
static subcmd_handler_t wl_mbo_sub_cmd_del_chan_pref;
static subcmd_handler_t wl_mbo_sub_cmd_list_chan_pref;
static subcmd_handler_t wl_mbo_sub_cmd_cell_data_cap;
static subcmd_handler_t wl_mbo_sub_cmd_dump_counters;
static subcmd_handler_t wl_mbo_sub_cmd_clear_counters;
static subcmd_handler_t wl_mbo_sub_cmd_force_assoc;
static subcmd_handler_t wl_mbo_sub_cmd_bsstrans_reject;
static subcmd_handler_t wl_mbo_sub_cmd_send_notif;
static subcmd_handler_t wl_mbo_sub_cmd_ap_attr;
static subcmd_handler_t wl_mbo_sub_cmd_ap_assoc_disallowed;
static subcmd_handler_t wl_mbo_sub_cmd_ap_enab;
static subcmd_handler_t wl_mbo_sub_cmd_fwd_gas_rqst;

/* help handlers */
static help_handler_t wl_mbo_add_chan_pref_help_fn;
static help_handler_t wl_mbo_del_chan_pref_help_fn;
static help_handler_t wl_mbo_list_chan_pref_help_fn;
static help_handler_t wl_mbo_cell_data_cap_help_fn;
static help_handler_t wl_mbo_counters_help_fn;
static help_handler_t wl_mbo_clear_counters_help_fn;
static help_handler_t wl_mbo_force_assoc_help_fn;
static help_handler_t wl_mbo_bsstrans_reject_help_fn;
static help_handler_t wl_mbo_send_notif_help_fn;
static help_handler_t wl_mbo_ap_attr_help_fn;
static help_handler_t wl_mbo_ap_assoc_disallowed_help_fn;
static help_handler_t wl_mbo_ap_enab_help_fn;
static help_handler_t wl_mbo_fwd_gas_rqst_help_fn;

#define WL_MBO_CMD_ALL 0
static const wl_mbo_sub_cmd_t mbo_subcmd_lists[] = {
	{ "add_chan_pref", 0x1, WL_MBO_CMD_ADD_CHAN_PREF,
	IOVT_BUFFER, wl_mbo_sub_cmd_add_chan_pref,
	wl_mbo_add_chan_pref_help_fn
	},
	{ "del_chan_pref", 0x1, WL_MBO_CMD_DEL_CHAN_PREF,
	IOVT_BUFFER, wl_mbo_sub_cmd_del_chan_pref,
	wl_mbo_del_chan_pref_help_fn
	},
	{ "list_chan_pref", 0x1, WL_MBO_CMD_LIST_CHAN_PREF,
	IOVT_BUFFER, wl_mbo_sub_cmd_list_chan_pref,
	wl_mbo_list_chan_pref_help_fn
	},
	{ "cell_data_cap", 0x1, WL_MBO_CMD_CELLULAR_DATA_CAP,
	IOVT_BUFFER, wl_mbo_sub_cmd_cell_data_cap,
	wl_mbo_cell_data_cap_help_fn
	},
	{ "counters", 0x1, WL_MBO_CMD_DUMP_COUNTERS,
	IOVT_BUFFER, wl_mbo_sub_cmd_dump_counters,
	wl_mbo_counters_help_fn
	},
	{ "clear_counters", 0x1, WL_MBO_CMD_CLEAR_COUNTERS,
	IOVT_BUFFER, wl_mbo_sub_cmd_clear_counters,
	wl_mbo_clear_counters_help_fn
	},
#ifdef WL_MBO_WFA_CERT
	{ "force_assoc", 0x1, WL_MBO_CMD_FORCE_ASSOC,
	IOVT_BUFFER, wl_mbo_sub_cmd_force_assoc,
	wl_mbo_force_assoc_help_fn
	},
	{ "bsstrans_reject", 0x1, WL_MBO_CMD_BSSTRANS_REJECT,
	IOVT_BUFFER, wl_mbo_sub_cmd_bsstrans_reject,
	wl_mbo_bsstrans_reject_help_fn
	},
#endif /* WL_MBO_WFA_CERT */
	{ "send_notif", 0x1, WL_MBO_CMD_SEND_NOTIF,
	IOVT_BUFFER, wl_mbo_sub_cmd_send_notif,
	wl_mbo_send_notif_help_fn
	},
	{ "ap_attr", 0x1, WL_MBO_CMD_AP_ATTRIBUTE,
	IOVT_BUFFER, wl_mbo_sub_cmd_ap_attr,
	wl_mbo_ap_attr_help_fn
	},
	{ "ap_assoc_disallowed", 0x1, WL_MBO_CMD_AP_ASSOC_DISALLOWED,
	IOVT_BUFFER, wl_mbo_sub_cmd_ap_assoc_disallowed,
	wl_mbo_ap_assoc_disallowed_help_fn
	},
	{ "ap_enable", 0x1, WL_MBO_CMD_AP_ENAB,
	IOVT_BUFFER, wl_mbo_sub_cmd_ap_enab,
	wl_mbo_ap_enab_help_fn
	},
	{ "fwd_gas_rqst", 0x1, WL_MBO_CMD_AP_FWD_GAS_RQST_TO_APP,
	IOVT_BUFFER, wl_mbo_sub_cmd_fwd_gas_rqst,
	wl_mbo_fwd_gas_rqst_help_fn
	},
	{ NULL, 0, 0, 0, NULL, NULL }
};

void wluc_mbo_module_init(void)
{
	/* register mbo commands */
	wl_module_cmds_register(wl_mbo_cmds);
}

static void
wl_mbo_usage(int cmd_id)
{
	const wl_mbo_sub_cmd_t *subcmd = &mbo_subcmd_lists[0];

	if (cmd_id > (WL_MBO_CMD_LAST - 1)) {
		return;
	}
	while (subcmd->name) {
		if (cmd_id == WL_MBO_CMD_ALL) {
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
wl_mbo_main(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	const wl_mbo_sub_cmd_t *subcmd = &mbo_subcmd_lists[0];

	UNUSED_PARAMETER(cmd);
	/* skip to command name */
	argv++;

	if (*argv) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "help")) {
			wl_mbo_usage(WL_MBO_CMD_ALL);
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
		wl_mbo_usage(WL_MBO_CMD_ALL);
		return BCME_USAGE_ERROR;
	}

	return ret;
}

static int
wl_mbo_sub_cmd_add_chan_pref(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;
	bool opclass_set = FALSE, chan_set = FALSE, reason_set = FALSE, pref_set = FALSE;

	/* only set */
	if (*argv == NULL) {
		wl_mbo_usage(WL_MBO_CMD_ADD_CHAN_PREF);
		return BCME_USAGE_ERROR;
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		while ((param = *argv++)) {
			val_p = *argv++;
			if (!val_p || *val_p == '-') {
				fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
				wl_mbo_usage(WL_MBO_CMD_ADD_CHAN_PREF);
				ret = BCME_USAGE_ERROR;
				goto fail;
			}
			if (strcmp(param, "-o") == 0) {
				uint8 opclass = strtoul(val_p, NULL, 0);
				/* TBD: validation */
				opclass_set = TRUE;
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_OPCLASS,
						sizeof(opclass), &opclass, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else if (strcmp(param, "-c") == 0) {
				uint8 chan = strtoul(val_p, NULL, 0);
				/* TBD: validation */
				chan_set = TRUE;
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_CHAN,
						sizeof(chan), &chan, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else if (strcmp(param, "-p") == 0) {
				uint8 pref = strtoul(val_p, NULL, 0);
				if ((pref != MBO_STA_NON_OPERABLE_BAND_CHAN) &&
					(pref != MBO_STA_NON_PREFERRED_BAND_CHAN) &&
					(pref != MBO_STA_PREFERRED_BAND_CHAN)) {
					fprintf(stderr, "Wrong preference value %u\n", pref);
					ret = BCME_BADOPTION;
					goto fail;
				}
				pref_set = TRUE;
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_PREFERENCE,
						sizeof(pref), &pref, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else if (strcmp(param, "-r") == 0) {
				uint8 rc = strtoul(val_p, NULL, 0);
				reason_set = TRUE;
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_REASON_CODE,
						sizeof(rc), &rc, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else {
				fprintf(stderr, "Unknown param %s\n", param);
			}
		}
		if (opclass_set && chan_set && pref_set && reason_set) {
			iov_buf->len = buflen_start - buflen;
			iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
			ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
		} else {
			wl_mbo_usage(WL_MBO_CMD_ADD_CHAN_PREF);
			ret = BCME_USAGE_ERROR;
		}
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static void
wl_mbo_add_chan_pref_help_fn(void)
{
	printf("wl mbo add_chan_pref -o <op_class> -c <chan> -p <pref val> -r <reason_code>\n");
	printf("\top class: operating class defined by IEEE\n");
	printf("\tchan: valid channel number\n");
	printf("\tpref val: preference value [<0/1/255>]\n");
	printf("\t\t 0 = non operable band/chan\n");
	printf("\t\t 1 = non preferred band/chan\n");
	printf("\t\t 255 = preferred band/chan\n");
	printf("\treason code: [<0-3>]\n");
}

static int
wl_mbo_sub_cmd_del_chan_pref(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;
	bool opclass_set = FALSE, chan_set = FALSE;

	iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
	if (iov_buf == NULL) {
		return BCME_NOMEM;
	}
	/* fill header */
	iov_buf->version = WL_MBO_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;

	pxtlv = (uint8 *)&iov_buf->data[0];
	buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

	if (*argv != NULL) {
		/* parse and pack config parameters */
		while ((param = *argv++)) {
			val_p = *argv++;
			if (!val_p || *val_p == '-') {
				fprintf(stderr, "%s: No value following %s\n", __FUNCTION__, param);
				wl_mbo_usage(WL_MBO_CMD_DEL_CHAN_PREF);
				ret = BCME_USAGE_ERROR;
				goto fail;
			}
			if (strcmp(param, "-o") == 0) {
				uint8 opclass = strtoul(val_p, NULL, 0);
				opclass_set = TRUE;
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_OPCLASS,
					sizeof(opclass), &opclass, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else if (strcmp(param, "-c") == 0) {
				uint8 chan = strtoul(val_p, NULL, 0);
				chan_set = TRUE;
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_CHAN,
					sizeof(chan), &chan, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else {
				fprintf(stderr, "Unknown param %s\n", param);
			}
		}
		if (!opclass_set && !chan_set) {
			wl_mbo_usage(WL_MBO_CMD_DEL_CHAN_PREF);
			ret = BCME_USAGE_ERROR;
		}
	}
	iov_buf->len = buflen_start - buflen;
	iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
	ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static void
wl_mbo_del_chan_pref_help_fn(void)
{
	printf("wl mbo del_chan_pref -o <op_class> -c <chan>\n");
	printf("\top class: operating class defined by IEEE\n");
	printf("\tchan: valid channel number\n");
}

static int
wl_mbo_process_iov_resp_buf(void *iov_resp, uint16 cmd_id, bcm_xtlv_unpack_cbfn_t cbfn)
{
	int ret = BCME_OK;
	uint16 version;

	/* Check for version */
	version = dtoh16(*(uint16 *)iov_resp);
	if (version != WL_MBO_IOV_VERSION) {
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
wl_mbo_get_iov_resp(void *wl, const wl_mbo_sub_cmd_t *cmd, bcm_xtlv_unpack_cbfn_t cbfn)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *iov_resp = NULL;

	UNUSED_PARAMETER(wl);
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
	iov_buf->version = WL_MBO_IOV_VERSION;
	iov_buf->id = cmd->cmd_id;

	ret = wlu_iovar_getbuf(wl, "mbo", iov_buf, WLC_IOCTL_SMLEN, iov_resp, WLC_IOCTL_MAXLEN);
	if ((ret == BCME_OK) && (iov_resp != NULL)) {
		wl_mbo_process_iov_resp_buf(iov_resp, cmd->cmd_id, cbfn);
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
wl_mbo_list_chan_pref_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_OPCLASS:
			printf("Opclass: %u\t", *data);
			break;
		case WL_MBO_XTLV_CHAN:
			printf("Chan: %u\t", *data);
			break;
		case WL_MBO_XTLV_PREFERENCE:
			printf("Preference: %u\t", *data);
			break;
		case WL_MBO_XTLV_REASON_CODE:
			printf("Reason Code: %u\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_sub_cmd_list_chan_pref(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	/* only get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_list_chan_pref_cbfn);
	} else {
		wl_mbo_usage(WL_MBO_CMD_LIST_CHAN_PREF);
		return BCME_USAGE_ERROR;
	}
	return ret;
}

static void
wl_mbo_list_chan_pref_help_fn(void)
{
	printf("wl mbo list_chan_pref\n");
}

static int
wl_mbo_cell_data_cap_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_CELL_DATA_CAP:
			if (*data == 0) {
				printf("Cellular data capability: value not set\n");
			} else {
				printf("Cellular data capability: %0x\n", *data);
			}
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_sub_cmd_cell_data_cap(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_cell_data_cap_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		param = *argv++;
		if (strcmp(param, "-c") == 0) {
			val_p = *argv;
			if (val_p == NULL || *val_p == '-') {
				wl_mbo_usage(WL_MBO_CMD_CELLULAR_DATA_CAP);
				ret = BCME_USAGE_ERROR;
				goto fail;
			}
			uint8 cell_data_cap = strtoul(val_p, NULL, 0);
			if (cell_data_cap < MBO_CELL_DATA_CONN_AVAILABLE ||
				cell_data_cap > MBO_CELL_DATA_CONN_NOT_CAPABLE) {
				fprintf(stderr, "wrong value %u\n", cell_data_cap);
				ret = BCME_BADARG;
				goto fail;
			}
			buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);
			ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_CELL_DATA_CAP,
				sizeof(cell_data_cap), &cell_data_cap, BCM_XTLV_OPTION_ALIGN32);
			if (ret != BCME_OK) {
				goto fail;
			}
		} else {
			fprintf(stderr, "wrong parameter %s\n", param);
			wl_mbo_usage(WL_MBO_CMD_CELLULAR_DATA_CAP);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
		ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static void
wl_mbo_cell_data_cap_help_fn(void)
{
	printf("wl mbo cell_data_cap -c <capability>\n");
	printf("\tcapability: cellular connection capability <1/2/3>\n");
	printf("\t\t 1 = Cellular data connection available\n");
	printf("\t\t 2 = Cellular data connection not available\n");
	printf("\t\t 3 = Cellular data connection not capable\n");
}

static int
wl_mbo_dump_counters_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_COUNTERS:
			{
				wl_mbo_counters_t *cntr = (wl_mbo_counters_t *)data;
				printf("BTM req::\n");
				printf("\trcvd:%u disassoc:%u termination:%u\n",
					cntr->trans_req_rcvd, cntr->trans_req_disassoc,
					cntr->trans_req_bss_term);
				printf("Transition Reason::\n");
				printf("\tunspecified:%u frame loss:%u traffic delay:%u "
					"low bandwidth:%u\n",
					cntr->trans_resn_unspec, cntr->trans_resn_frm_loss,
					cntr->trans_resn_traffic_delay, cntr->trans_resn_insuff_bw);
				printf("\tload balance:%u low rssi:%u excss retransmission:%u\n",
					cntr->trans_resn_load_bal, cntr->trans_resn_low_rssi,
					cntr->trans_resn_xcess_retransmn);
				printf("\tGray zone:%u premium AP switch:%u\n",
					cntr->trans_resn_gray_zone, cntr->trans_resn_prem_ap_sw);
				printf("Num of transition rejection:%u\n", cntr->trans_rejn_sent);
				printf("Transition rejection reason::\n");
				printf("\texcess frame loss:%u traffic delay:%u low QoS:%u\n",
					cntr->trans_rejn_insuffic_qos_cap,
					cntr->trans_rejn_xcess_frm_loss,
					cntr->trans_rejn_xcess_traffic_delay);
				printf("\tlow RSSI:%u: high interference:%u service unavail:%u\n",
					cntr->trans_rejn_low_rssi,
					cntr->trans_rejn_high_interference,
					cntr->trans_rejn_service_unavail);
				printf("Beacon req_rcvd:%u\n", cntr->bcn_req_rcvd);
				printf("Beacon rep sent:%u\n", cntr->bcn_rep_sent);
				printf("NULL beacon rep sent:%u\n", cntr->null_bcn_rep_sent);
				printf("Switch wlan to cell:%u\n", cntr->wifi_to_cell);
			}
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_sub_cmd_dump_counters(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	/* only get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_dump_counters_cbfn);
	} else {
		wl_mbo_usage(WL_MBO_CMD_DUMP_COUNTERS);
		return BCME_USAGE_ERROR;
	}
	return ret;
}

static void
wl_mbo_counters_help_fn(void)
{
	printf("wl mbo counters\n");
}

static int
wl_mbo_sub_cmd_clear_counters(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint16 iovlen = 0;
	/* only set */
	if (*argv == NULL) {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;
		iov_buf->len = 0;
		iovlen = sizeof(bcm_iov_buf_t);
		ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
	} else {
		wl_mbo_usage(WL_MBO_CMD_CLEAR_COUNTERS);
		return BCME_USAGE_ERROR;
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static void
wl_mbo_clear_counters_help_fn(void)
{
	printf("wl mbo clear_counters\n");
}

#ifdef WL_MBO_WFA_CERT
static int
wl_mbo_force_assoc_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_ENABLE:
			printf("Force association: %s\n", (*data == 0) ? "disabled" : "enabled");
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_sub_cmd_force_assoc(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_force_assoc_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		param = *argv++;
		if (strcmp(param, "-e") == 0) {
			val_p = *argv;
			if (val_p == NULL || *val_p == '-') {
				wl_mbo_usage(WL_MBO_CMD_FORCE_ASSOC);
				ret = BCME_USAGE_ERROR;
				goto fail;
			}
			uint8 force = strtoul(val_p, NULL, 0);
			if ((force != 0) && (force != 1)) {
				fprintf(stderr, "wrong value %u\n", force);
				ret = BCME_BADARG;
				goto fail;
			}
			buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);
			ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_ENABLE,
				sizeof(force), &force, BCM_XTLV_OPTION_ALIGN32);
			if (ret != BCME_OK) {
				goto fail;
			}
		} else {
			fprintf(stderr, "wrong parameter %s\n", param);
			wl_mbo_usage(WL_MBO_CMD_FORCE_ASSOC);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
		ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static void
wl_mbo_force_assoc_help_fn(void)
{
	printf("wl mbo force_assoc -e <value>\n");
	printf("\tvalue: Enable/disable assoc attempt even if "
		"association disallowed by AP <1/0>\n");
	printf("\t\t 1 = Enable force association attempt when "
		"AP is not accepting new connection\n");
	printf("\t\t 0 = disable force association attempt\n");
}

static int
wl_mbo_bsstrans_reject_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_ENABLE:
			printf("Bss Transition Reject: %s\n",
				(*data == 0) ? "disabled" : "enabled");
			break;
		case WL_MBO_XTLV_REASON_CODE:
			printf("Reason Code: %u\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_sub_cmd_bsstrans_reject(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;
	bool reason_set = FALSE;
	uint8 enable = 0;

	/* get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_bsstrans_reject_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			return BCME_NOMEM;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);

		/* parse and pack config parameters */
		while ((param = *argv++)) {
			val_p = *argv++;
			if (!val_p || *val_p == '-') {
				fprintf(stderr, "%s: wrong usage %s\n", __FUNCTION__, param);
				wl_mbo_usage(WL_MBO_CMD_BSSTRANS_REJECT);
				ret = BCME_USAGE_ERROR;
				goto fail;
			}
			if (strcmp(param, "-e") == 0) {
				enable = strtoul(val_p, NULL, 0);
				if ((enable != 0) && (enable != 1)) {
					fprintf(stderr, "wrong value %u\n", enable);
					ret = BCME_BADARG;
					goto fail;
				}
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_ENABLE,
						sizeof(enable), &enable, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else if (strcmp(param, "-r") == 0) {
				uint8 reason = strtoul(val_p, NULL, 0);
				if (reason > MBO_TRANS_REJ_REASON_SERVICE_UNAVAIL) {
					fprintf(stderr, "wrong reason value %u\n", reason);
					ret = BCME_BADARG;
					goto fail;
				}
				reason_set = TRUE;
				ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_REASON_CODE,
						sizeof(reason), &reason, BCM_XTLV_OPTION_ALIGN32);
				if (ret != BCME_OK) {
					goto fail;
				}
			} else {
				fprintf(stderr, "Unknown param %s\n", param);
			}
		}
		if ((enable && reason_set) || (!enable)) {
			iov_buf->len = buflen_start - buflen;
			iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
			ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
		} else {
			wl_mbo_usage(WL_MBO_CMD_BSSTRANS_REJECT);
			ret = BCME_USAGE_ERROR;
		}
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static void
wl_mbo_bsstrans_reject_help_fn(void)
{
	printf("wl mbo bsstrans_reject -e <value> -r <reason>\n");
	printf("\tvalue: Enable/Disable bsstrans rejection <1/0>\n");
	printf("\t\t 1 = Enable bsstrans rejection\n");
	printf("\t\t 0 = Disable bsstrans rejection\n");
	printf("\treason: reason code for rejection <0-6>\n");
	printf("\t\t 0 = Unspecified reason\n");
	printf("\t\t 1 = Excessive frame loss expected on transition\n");
	printf("\t\t 2 = Excessive delay for current traffic stream on transition\n");
	printf("\t\t 3 = Insufficient QoS capacity expected on transition\n");
	printf("\t\t 4 = Low RSSI observed on suggested BSS\n");
	printf("\t\t 5 = High interference observed on suggested BSS\n");
	printf("\t\t 6 = Service unavailability on suggested BSS\n");
}
#endif /* WL_MBO_WFA_CERT */

static int
wl_mbo_sub_cmd_send_notif(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *param = NULL, *val_p = NULL;
	uint16 iovlen = 0;

	/* no get, only set */
	if (*argv == NULL) {
		wl_mbo_usage(WL_MBO_CMD_SEND_NOTIF);
		return BCME_USAGE_ERROR;
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		param = *argv++;
		if (strcmp(param, "-t") == 0) {
			val_p = *argv;
			if (val_p == NULL || *val_p == '-') {
				wl_mbo_usage(WL_MBO_CMD_SEND_NOTIF);
				ret = BCME_USAGE_ERROR;
				goto fail;
			}
			uint8 sub_elem_type = strtoul(val_p, NULL, 0);
			if (sub_elem_type != MBO_ATTR_CELL_DATA_CAP &&
				sub_elem_type != MBO_ATTR_NON_PREF_CHAN_REPORT) {
				fprintf(stderr, "wrong value %u\n", sub_elem_type);
				ret = BCME_BADARG;
				goto fail;
			}
			buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);
			ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_SUB_ELEM_TYPE,
				sizeof(sub_elem_type), &sub_elem_type, BCM_XTLV_OPTION_ALIGN32);
			if (ret != BCME_OK) {
				goto fail;
			}
		} else {
			fprintf(stderr, "wrong parameter %s\n", param);
			wl_mbo_usage(WL_MBO_CMD_SEND_NOTIF);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
		ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static void
wl_mbo_send_notif_help_fn(void)
{
	printf("wl mbo send_notif -t <type>\n");
	printf("\ttype: WNM Notification req sub-element type <2/3>\n");
	printf("\t\t 2 = Non-preferred chan report\n");
	printf("\t\t 3 = Cellular data capability\n");
}

static int
wl_mbo_ap_attr_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_AP_ATTR:
				printf("ap attribute : %0x\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_ap_enab_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_AP_ENAB:
				printf("MBO AP ENABLE : %0x\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_fwd_gas_rqst_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_AP_FWD_GAS_RQST_TO_APP:
				printf("FWD GAS RQST TO APP : %0x\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static int
wl_mbo_ap_assoc_disallowed_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);
	if (data == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case WL_MBO_XTLV_AP_ASSOC_DISALLOWED:
				printf("ap attribute assoc disallowed : %0x\n", *data);
			break;
		default:
			printf("%s: Unknown tlv %u\n", __FUNCTION__, type);
	}
	return BCME_OK;
}

static void
wl_mbo_ap_attr_help_fn(void)
{
	printf("Possible values for mbo ap_attr iovars are: \n");
	printf("Reserved bit(0 - 5) i.e. 0x01 - 0x20 \n");
	printf("Set bit 6 for cellular data aware i.e 0x40 \n");
	printf("Reserved bit(7) i.e 0x80 \n");
}

static void
wl_mbo_ap_enab_help_fn(void)
{
	printf("Only get /set mbo ap feature options \n");
	printf("usage: to enable/disable : wl -i <interface> mbo ap_enable <1/0> \n");
	printf("usage: get mbo ap feature : wl -i <interface> mbo ap_enable\n");
}

static void
wl_mbo_fwd_gas_rqst_help_fn(void)
{
	printf("Only get /set mbo fwd gas rqst to app options \n");
	printf("usage: to enable/disable : wl -i <interface> mbo fwd_gas_rqst <1/0> \n");
	printf("usage: get mbo ap feature : wl -i <interface> mbo fwd_gas_rqst\n");
}

static void
wl_mbo_ap_assoc_disallowed_help_fn(void)
{
	printf("Possible values for mbo ap_assoc_disallowed iovars are: \n");
	printf(" 1 - Reason unspecified \n");
	printf(" 2 - Max sta limit reached \n");
	printf(" 3 - Air interface overload \n");
	printf(" 4 - Auth server overload \n");
	printf(" 5 - Insufficient RSSI \n");
}

static int
wl_mbo_sub_cmd_ap_attr(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *val_p = NULL;
	uint16 iovlen = 0;
	uint8 ap_attr;

	/* get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_ap_attr_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		val_p = *argv;
		if (val_p == NULL) {
			wl_mbo_usage(WL_MBO_CMD_AP_ATTRIBUTE);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		ap_attr = strtoul(val_p, NULL, 0);
		if ((ap_attr == WL_MBO_CELLULAR_DATA_AWARE_AP)) {
			buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);
			ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_AP_ATTR,
				sizeof(ap_attr), &ap_attr, BCM_XTLV_OPTION_ALIGN32);
			if (ret != BCME_OK) {
				goto fail;
			}
			iov_buf->len = buflen_start - buflen;
			iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
			ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
		} else {
			wl_mbo_usage(WL_MBO_CMD_AP_ATTRIBUTE);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_mbo_sub_cmd_ap_assoc_disallowed(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *val_p = NULL;
	uint16 iovlen = 0;
	uint8 ap_assoc_disallowed;

	/* get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_ap_assoc_disallowed_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		val_p = *argv;
		if (val_p == NULL) {
			wl_mbo_usage(WL_MBO_CMD_AP_ASSOC_DISALLOWED);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		ap_assoc_disallowed = strtoul(val_p, NULL, 0);
		/* MBO standard possible values:
		 * 0: use to Disable from IOVAR.
		 * 1: Unspecified reason
		 * 2: Max number of sta association reahed
		 * 3: Air interface is overloaded
		 * 4: Authentication server overloaded
		 * 5: Insufficient RSSI
		 * 6 - 255: Reserved
		 */
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_AP_ASSOC_DISALLOWED,
				sizeof(ap_assoc_disallowed), &ap_assoc_disallowed,
				BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
		ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_mbo_sub_cmd_ap_enab(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *val_p = NULL;
	uint16 iovlen = 0;
	int32 mbo_ap_enable;

	/* get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_ap_enab_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = cmd->cmd_id;

		pxtlv = (uint8 *)&iov_buf->data[0];
		val_p = *argv;
		if (val_p == NULL) {
			wl_mbo_usage(WL_MBO_CMD_AP_ENAB);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		mbo_ap_enable = strtoul(val_p, NULL, 0);
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_AP_ENAB,
				sizeof(mbo_ap_enable), (const uint8*)&mbo_ap_enable,
				BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
		ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}

static int
wl_mbo_sub_cmd_fwd_gas_rqst(void *wl, const wl_mbo_sub_cmd_t *cmd, char **argv)
{
	int ret = BCME_OK;
	bcm_iov_buf_t *iov_buf = NULL;
	uint8 *pxtlv = NULL;
	uint16 buflen = 0, buflen_start = 0;
	char *val_p = NULL;
	uint16 iovlen = 0;
	uint8 fwd_gas_rqst;

	/* get */
	if (*argv == NULL) {
		ret = wl_mbo_get_iov_resp(wl, cmd, wl_mbo_fwd_gas_rqst_cbfn);
	} else {
		iov_buf = (bcm_iov_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
		if (iov_buf == NULL) {
			ret = BCME_NOMEM;
			goto fail;
		}
		/* fill header */
		iov_buf->version = WL_MBO_IOV_VERSION;
		iov_buf->id = WL_MBO_CMD_AP_FWD_GAS_RQST_TO_APP;

		pxtlv = (uint8 *)&iov_buf->data[0];
		val_p = *argv;
		if (val_p == NULL) {
			wl_mbo_usage(WL_MBO_CMD_AP_FWD_GAS_RQST_TO_APP);
			ret = BCME_USAGE_ERROR;
			goto fail;
		}
		fwd_gas_rqst = strtoul(val_p, NULL, 0);
		buflen = buflen_start = WLC_IOCTL_MEDLEN - sizeof(bcm_iov_buf_t);
		ret = bcm_pack_xtlv_entry(&pxtlv, &buflen, WL_MBO_XTLV_AP_FWD_GAS_RQST_TO_APP,
				sizeof(fwd_gas_rqst), &fwd_gas_rqst, BCM_XTLV_OPTION_ALIGN32);
		if (ret != BCME_OK) {
			goto fail;
		}
		iov_buf->len = buflen_start - buflen;
		iovlen = sizeof(bcm_iov_buf_t) + iov_buf->len;
		ret = wlu_var_setbuf(wl, "mbo", (void *)iov_buf, iovlen);
	}
fail:
	if (iov_buf) {
		free(iov_buf);
	}
	return ret;
}
