/*
 * wl btl command module - BT log
 *
 * Copyright 2019 Broadcom
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
 * $Id:$
 */

#include <wlioctl.h>

#if defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_btl;

static cmd_t wl_btl_cmds[] = {
	{ "bus:btl", wl_btl, WLC_GET_VAR, WLC_SET_VAR,
	"BT log subcommands:\n"
	"\tbus:btl enable <0|1>\n"
	"\tbus:btl stats\n"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_btl_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register btl commands */
	wl_module_cmds_register(wl_btl_cmds);
}

static int
wl_btl(void *wl, cmd_t *cmd, char **argv)
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

	if (!*argv) {
		/* get */
		uint8 buffer[OFFSETOF(wl_btl_t, data)];
		wl_btl_t *btl = (wl_btl_t *)buffer;
		int len = OFFSETOF(wl_btl_t, data);

		memset(btl, 0, len);
		if (!strncmp(subcmd, "enable", subcmd_len)) {
			btl->subcmd_id = WL_BTL_SUBCMD_ENABLE;
		} else if (!strncmp(subcmd, "stats", subcmd_len)) {
			btl->subcmd_id = WL_BTL_SUBCMD_STATS;
		} else {
			return BCME_USAGE_ERROR;
		}
		len += btl->len;

		/* invoke GET iovar */
		btl->subcmd_id = htod16(btl->subcmd_id);
		btl->len = htod16(btl->len);
		if ((err = wlu_iovar_getbuf(wl, cmd->name, btl, len, buf, WLC_IOCTL_MEDLEN)) < 0) {
			return err;
		}

		/* process and print GET results */
		btl = (wl_btl_t *)buf;
		btl->subcmd_id = dtoh16(btl->subcmd_id);
		btl->len = dtoh16(btl->len);

		switch (btl->subcmd_id) {
		case WL_BTL_SUBCMD_ENABLE:
		{
			wl_btl_enable_t *btl_enable = (wl_btl_enable_t *)btl->data;
			if (btl->len >= sizeof(*btl_enable)) {
				printf("%d\n", btl_enable->enable);
			} else {
				err = BCME_BADLEN;
			}
			break;
		}
		case WL_BTL_SUBCMD_STATS:
		{
			wl_btl_stats_t *btl_stats = (wl_btl_stats_t *)btl->data;
			if (btl->len >= sizeof(*btl_stats)) {
				printf("BT interrupts:                    %d\n",
					btl_stats->bt_interrupt);
				printf("config requests:                  %d\n",
					btl_stats->config_req);
				printf("config responses success:         %d\n",
				       btl_stats->config_res_success);
				printf("config responses fail:            %d\n",
					btl_stats->config_res_fail);
				printf("log requests:                     %d\n",
					btl_stats->log_req);
				printf("log responses success:            %d\n",
					btl_stats->log_res_success);
				printf("log responses fail:               %d\n",
					btl_stats->log_res_fail);
				printf("indirect read fail:               %d\n",
					btl_stats->indirect_read_fail);
				printf("indirect write fail:              %d\n",
					btl_stats->indirect_write_fail);
				printf("DMA fail:                         %d\n",
					btl_stats->dma_fail);
				printf("min log request duration (usec):  %d\n",
					btl_stats->min_log_req_duration);
				printf("max log request duration (usec):  %d\n",
					btl_stats->max_log_req_duration);

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
		wl_btl_t *btl = (wl_btl_t *)buf;
		int len;

		if (!strncmp(subcmd, "enable", subcmd_len) &&
			(!strcmp(argv[0], "0") || !strcmp(argv[0], "1"))) {
			wl_btl_enable_t *btl_enable = (wl_btl_enable_t *)btl->data;
			btl->subcmd_id = WL_BTL_SUBCMD_ENABLE;
			btl->len = sizeof(*btl_enable);
			btl_enable->enable = atoi(argv[0]);
		} else {
			return BCME_USAGE_ERROR;
		}

		/* invoke SET iovar */
		len = OFFSETOF(wl_btl_t, data) + btl->len;
		btl->subcmd_id = htod16(btl->subcmd_id);
		btl->len = htod16(btl->len);
		err = wlu_iovar_set(wl, cmd->name, btl, len);
	}

	return err;
}
