/*
 * wl tvpm command module
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
 * $Id: wluc_leakyapstats.c 674525 2016-12-09 04:17:19Z $
 */

#include <wlioctl.h>

#include <bcmutils.h>
#include "wlu_common.h"
#include "wlu.h"

static int wluc_tvpm_tvpm(void *wl, cmd_t *cmd, char **argv);

static cmd_t wl_tvpm_cmds[] = {
	{ "tvpm", wluc_tvpm_tvpm, WLC_GET_VAR, WLC_SET_VAR,
	"Thermal, Voltage, Power Mitigation control and status\n" },
	{ NULL, NULL, 0, 0, NULL }
};

/* module initialization */
void
wluc_tvpm_module_init(void)
{
	/* register leaky_ap_stats commands */
	wl_module_cmds_register(wl_tvpm_cmds);
}

static int
wluc_tvpm_tvpm(void *wl, cmd_t *cmd, char **argv)
{
	wl_tvpm_req_t* tvpm_req = NULL;
	size_t reqlen = sizeof(wl_tvpm_req_t) + sizeof(wl_tvpm_status_t);
	uint8 *outbuf = NULL;
	size_t outlen = WLC_IOCTL_MEDLEN;
	int err = BCME_OK;
	int val;
	bool iov_set = FALSE;

	/* skip the command name */
	argv++;

	tvpm_req = malloc(reqlen);
	if (tvpm_req == NULL) {
		fprintf(stderr, "unable to allocate memory\n");
		err = BCME_NOMEM;
		goto done;
	}
	outbuf = malloc(outlen);
	if (outbuf == NULL) {
		fprintf(stderr, "unable to allocate memory\n");
		err = BCME_NOMEM;
		goto done;
	}

	tvpm_req->version = TVPM_REQ_CURRENT_VERSION;
	tvpm_req->length = reqlen;

	if (*argv == NULL) {
		wl_tvpm_status_t* status;

		/* Read TVPM status */
		if (cmd->get < 0) {
			goto done;
		}

		tvpm_req->req_type = WL_TVPM_REQ_STATUS;
		err = wlu_iovar_getbuf(wl, cmd->name, tvpm_req, reqlen, outbuf, outlen);
		if (err < 0) {
			printf("WL_TVPM_REQ_STATUS: err %d\n", err);
			goto done;
		}
		status = (wl_tvpm_status_t*)outbuf;

		printf("enable = %u\n", status->enable);
		printf("tx_dutycycle = %u\n", status->tx_dutycycle);
		printf("tx_power_backoff = %d\n", status->tx_power_backoff);
		printf("num_active_chains = %u\n", status->num_active_chains);
		printf("temperature = %d\n", status->temp);
		printf("vbat = %u\n", status->vbat);
	} else if (strncmp(*argv, "cltm", sizeof("cltm")) == 0) {
		tvpm_req->req_type = WL_TVPM_REQ_CLTM_INDEX;
		++argv;
		if (*argv == NULL) {
			/* Read tvpm cltm index */
			tvpm_req->req_type = WL_TVPM_REQ_CLTM_INDEX;
			err = wlu_iovar_getbuf(wl, cmd->name, tvpm_req, reqlen, outbuf, outlen);
			if (err < 0) {
				printf("REQ_CLTM_INDEX: err %d\n", err);
			} else {
				printf("%d\n", *(int32*)outbuf);
			}
		} else {
			/* Write tvpm cltm index */
			val = atoi(*argv);
			if (val == -1) {
				err = BCME_BADARG;
				goto done;
			}
			*(int32*)(tvpm_req->value) = val;
			iov_set = TRUE;
		}
	} else if (strncmp(*argv, "ppm", sizeof("ppm")) == 0) {
		tvpm_req->req_type = WL_TVPM_REQ_PPM_INDEX;
		++argv;
		if (*argv == NULL) {
			/* Read tvpm ppm index */
			tvpm_req->req_type = WL_TVPM_REQ_PPM_INDEX;
			err = wlu_iovar_getbuf(wl, cmd->name, tvpm_req, reqlen, outbuf, outlen);
			if (err < 0) {
				printf("REQ_PPM_INDEX: err %d\n", err);
			} else {
				printf("%d\n", *(int32*)outbuf);
			}
		} else {
			/* Write tvpm ppm index */
			val = atoi(*argv);
			if (val == -1) {
				err = BCME_BADARG;
				goto done;
			}
			*(int32*)(tvpm_req->value) = val;
			iov_set = TRUE;
		}
	} else if (strncmp(*argv, "0", sizeof("0")) == 0) {
		/* Disable tvpm */
		tvpm_req->req_type = WL_TVPM_REQ_ENABLE;
		*(int32*)(tvpm_req->value) = 0;
		iov_set = TRUE;
	} else if (strncmp(*argv, "1", sizeof("1")) == 0) {
		/* Enable tvpm */
		tvpm_req->req_type = WL_TVPM_REQ_ENABLE;
		*(int32*)(tvpm_req->value) = 1;
		iov_set = TRUE;
	} else {
		err = BCME_BADARG;
		goto done;
	}

	if (iov_set) {
		err = wlu_iovar_set(wl, cmd->name, tvpm_req, reqlen);
		if (err < 0) {
			goto done;
		}
	}

done:
	if (tvpm_req) {
		free(tvpm_req);
	}
	if (outbuf) {
		free(outbuf);
	}
	return err;
}
