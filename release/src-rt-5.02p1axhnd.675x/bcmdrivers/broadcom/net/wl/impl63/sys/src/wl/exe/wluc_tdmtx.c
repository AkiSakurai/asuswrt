/*
 * wl TDM Tx command module
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
 * $Id: wluc_tdmtx.c $
 */

#if defined(linux)
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#endif /* linux */

#include <wlioctl.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <sys/stat.h>
#include <errno.h>

#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

#define TDMTX_CMD_ENABLE	"enable"
#define TDMTX_CMD_STATUS	"status"
#define TDMTX_CMD_TXPRI		"txpri"
#define TDMTX_CMD_THRESHOLD	"txa_max"
#define TDMTX_CMD_DEFER		"defer"
#define TDMTX_CMD_CFG		"cfg"

static cmd_func_t wl_tdmtx;

static cmd_t wl_tdmtx_cmds[] = {
	{ "tdmtx", wl_tdmtx, WLC_GET_VAR, WLC_SET_VAR,
	"TDM Tx subcommands:\n"
	"\ttdmtx enable <0|1>\n"
	"\ttdmtx status\n"
	"\ttdmtx txpri <priority period>\n"
	"\ttdmtx txa_max <threshold>\n"
	"\ttdmtx defer <defer time>\n"
	"\ttdmtx cfg <bitmap>\n"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

static void wl_tdmtx_print_status(tdmtx_cnt_v1_t *tdmtx_cnt);

/* module initialization */
void
wluc_tdmtx_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register tdmtx commands */
	wl_module_cmds_register(wl_tdmtx_cmds);
}

static int
wl_tdmtx(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	char *subcmd;
	int subcmd_len;

	/* skip iovar */
	argv++;

	/* must have subcommand */
	subcmd = *argv++;
	if (!subcmd) {
		return BCME_USAGE_ERROR;
	}
	subcmd_len = strlen(subcmd);
	memset(buf, 0, WLC_IOCTL_MAXLEN);

	if (!*argv) {
		/* get */
		uint8 buffer[OFFSETOF(wl_tdmtx_ioc_t, data)];
		wl_tdmtx_ioc_t *tdmtx = (wl_tdmtx_ioc_t *)buffer;
		int len = OFFSETOF(wl_tdmtx_ioc_t, data);

		memset(buffer, 0, sizeof(buffer));

		if (!strncmp(subcmd, TDMTX_CMD_ENABLE, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_ENB;
		} else if (!strncmp(subcmd, TDMTX_CMD_STATUS, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_STATUS;
		} else if (!strncmp(subcmd, TDMTX_CMD_TXPRI, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_TXPRI;
		} else if (!strncmp(subcmd, TDMTX_CMD_DEFER, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_DEFER;
		} else if (!strncmp(subcmd, TDMTX_CMD_THRESHOLD, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_TXA;
		} else if (!strncmp(subcmd, TDMTX_CMD_CFG, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_CFG;
		} else {
			return BCME_USAGE_ERROR;
		}

		tdmtx->len = htod16(len);
		tdmtx->id = htod16(tdmtx->id);

		if ((err = wlu_iovar_getbuf(wl, cmd->name, tdmtx, len, buf,
			WLC_IOCTL_MEDLEN)) < 0) {
			return err;
		}

		tdmtx = (wl_tdmtx_ioc_t *)buf;
		tdmtx->id = dtoh16(tdmtx->id);
		tdmtx->len = dtoh16(tdmtx->len);

		switch (tdmtx->id) {
		case IOV_TDMTX_ENB:
			printf(" %d \n", tdmtx->data[0]);
			break;
		case IOV_TDMTX_STATUS:
			if ((tdmtx->len) < WL_CNT_TDMTX_STRUCT_SZ) {
				err = BCME_BADLEN;
			} else {
				wl_tdmtx_print_status((tdmtx_cnt_v1_t *)tdmtx->data);
			}
			break;
		case IOV_TDMTX_TXPRI:
			printf("0x%x \n", dtoh32(*((uint32 *)tdmtx->data)));
			break;
		case IOV_TDMTX_DEFER:
			printf("0x%x \n", dtoh32(*((uint32 *)tdmtx->data)));
			break;
		case IOV_TDMTX_TXA:
			printf("0x%x \n", dtoh32(*((uint32 *)tdmtx->data)));
			break;
		case IOV_TDMTX_CFG:
			printf("0x%04x \n", dtoh16(*((uint16 *)tdmtx->data)));
			break;
		default:
			break;
		}

	} else {
		/* set */
		wl_tdmtx_ioc_t *tdmtx = (wl_tdmtx_ioc_t *)buf;
		int len;
		char *endptr = NULL;

		int val = strtol(*argv, &endptr, 0);

		if (!strncmp(subcmd, TDMTX_CMD_ENABLE, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_ENB;
			tdmtx->len = sizeof(uint32);
			if (!stricmp(*argv, "on")) {
				val = 1;
			} else if (!stricmp(*argv, "off")) {
				val = 0;
			} else if (*endptr != '\0') {
				/* The string cannot be parsed as number */
				return BCME_USAGE_ERROR;
			}
			tdmtx->data[0] = val ? 1 : 0;
		} else if (!strncmp(subcmd, TDMTX_CMD_STATUS, subcmd_len)) {
			if (val != 0) {
				return BCME_USAGE_ERROR;
			}
			tdmtx->id = IOV_TDMTX_STATUS;
			tdmtx->len = sizeof(uint32);
			tdmtx->data[0] = 0;
		} else if (!strncmp(subcmd, TDMTX_CMD_TXPRI, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_TXPRI;
			tdmtx->len = sizeof(uint32);

			if (*endptr != '\0') {
				return BCME_USAGE_ERROR;
			} else {
				val = htod32(val);
			}

			*(uint32 *)tdmtx->data = val;
		} else if (!strncmp(subcmd, TDMTX_CMD_DEFER, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_DEFER;
			tdmtx->len = sizeof(uint32);
			if (*endptr != '\0') {
				return BCME_USAGE_ERROR;
			} else {
				val = htod32(val);
			}

			*(uint32 *)tdmtx->data = val;
		} else if (!strncmp(subcmd, TDMTX_CMD_THRESHOLD, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_TXA;
			tdmtx->len = sizeof(uint32);
			if (*endptr != '\0') {
				return BCME_USAGE_ERROR;
			} else {
				val = htod32(val);
			}

			*(uint32 *)tdmtx->data = val;
		} else if (!strncmp(subcmd, TDMTX_CMD_CFG, subcmd_len)) {
			tdmtx->id = IOV_TDMTX_CFG;
			tdmtx->len = sizeof(uint16);
			if (*endptr != '\0') {
				return BCME_USAGE_ERROR;
			} else {
				val = htod16(val);
			}

			*(uint16 *)tdmtx->data = val;
		} else {
			return BCME_USAGE_ERROR;
		}

		len = OFFSETOF(wl_tdmtx_ioc_t, data) + tdmtx->len;
		tdmtx->id = dtoh16(tdmtx->id);
		tdmtx->len = dtoh16(tdmtx->len);
		err = wlu_iovar_set(wl, cmd->name, tdmtx, len);
	}

	return err;

}

static void
wl_tdmtx_print_status(tdmtx_cnt_v1_t *tdmtx_cnt)
{
	printf("tdmtx_txa_on:		%u\n", dtoh32(tdmtx_cnt->tdmtx_txa_on));
	printf("tdmtx_txa_tmcnt:	%u\n", dtoh32(tdmtx_cnt->tdmtx_txa_tmcnt));
	printf("tdmtx_por_on:		%u\n", dtoh32(tdmtx_cnt->tdmtx_por_on));
	printf("tdmtx_txpuen:		%u\n", dtoh32(tdmtx_cnt->tdmtx_txpuen));
	printf("tdmtx_txpudis:		%u\n", dtoh32(tdmtx_cnt->tdmtx_txpudis));
	printf("tdmtx_txpri_on:		%u\n", dtoh32(tdmtx_cnt->tdmtx_txpri_on));
	printf("tdmtx_txdefer:		%u\n", dtoh32(tdmtx_cnt->tdmtx_txdefer));
	printf("tdmtx_txpri_dur:	%u\n", dtoh32(tdmtx_cnt->tdmtx_txpri_dur));
	printf("tdmtx_txdefer_dur:	%u\n", dtoh32(tdmtx_cnt->tdmtx_txdefer_dur));
	printf("tdmtx_txpri:		%u\n", dtoh32(tdmtx_cnt->tdmtx_txpri));
	printf("tdmtx_defer:		%u\n", dtoh32(tdmtx_cnt->tdmtx_defer));
	printf("tdmtx_txa_threshold:	%u\n", dtoh32(tdmtx_cnt->tdmtx_threshold));
	printf("tdmtx_rssi_threshold:	%d\n", (int16)dtoh32((tdmtx_cnt->tdmtx_rssi_threshold)));
	printf("tdmtx_txpwr_rsp_boff:	%u\n", dtoh32(tdmtx_cnt->tdmtx_txpwrboff));
	printf("tdmtx_txpwr_dt_boff:	%u\n", dtoh32(tdmtx_cnt->tdmtx_txpwrboff_dt));
}
