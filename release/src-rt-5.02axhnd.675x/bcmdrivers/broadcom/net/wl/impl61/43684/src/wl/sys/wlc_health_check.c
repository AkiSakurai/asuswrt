/* Health check module
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_health_check.c 680193 2017-01-19 00:42:16Z $
 */
#ifdef WL_HEALTH_CHECK
#include <osl.h>
#include <siutils.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <bcmwifi_channels.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc.h>
#include "wlc_health_check.h"

#define HEALTH_CHECK_XTLV_HEADER_SIZE	4
#define HEALTH_CHECK_STATUS_OK			0

static void wlc_health_check_watchdog(void* hdl);

/* client health check struct */
struct health_check_client_info {

	health_check_module_fn module_fptr; /* module call back pointer */
	uint8 *buffer; /* client module should allocate it's own */
					/* buffer for health check */
	uint16 max_length; /* allcated buffer length including TLV header */
	void *module_context_ptr; /* modules context pointer */
	/* like for scan as a module ,scan_info will be context */
	uint16 module_id; /* any unique  module specific id */
	uint8 health_status;
	osl_t *osh;
	struct health_check_client_info *next;

};

/* health check module struct */
struct health_check_info {

	health_check_client_info_t *client_health_check;
	wlc_info_t			*wlc;		/* pointer to wlc */

};

/* health check attach function
 *  allocate memory for health_check_info.
 *  register health check watchdog function.
 *
 */

health_check_info_t *
BCMATTACHFN(wlc_health_check_attach)(wlc_info_t *wlc)
{
	health_check_info_t *hc = NULL;
	/* watchdog_fn_t watchdog_fn = NULL; */
	int err;
	hc = (health_check_info_t*)MALLOCZ(wlc->osh, sizeof(health_check_info_t));
	if (hc == NULL)
	{
		WL_ERROR(("Health Check attach failed\n"));
		return NULL;
	}

	hc->wlc = wlc;

	err = wlc_module_register(wlc->pub, NULL, "wlc_hc", hc, NULL,
	                          wlc_health_check_watchdog, NULL, NULL);
	if (err != BCME_OK) {
		WL_ERROR(("Health check watchdog fail\n"));
		return NULL;
	}
	return hc;
}

/* Health check detach
 */
void
BCMATTACHFN(wlc_health_check_detach)(health_check_info_t *hc)
{
	wlc_info_t *wlc;
	health_check_client_info_t *client_info = hc->client_health_check;
	wlc = hc->wlc;
	wlc_module_unregister(wlc->pub, "wlc_hc", hc);
	/* dregister  clients  */
	wlc_health_check_dregister_clients(client_info);
	MFREE(hc->wlc->osh, hc, sizeof(health_check_info_t));
}

/* Registering clients to the health check module
 * This  is a public fuction
 * and should be called by any module/client whosoever
 * wants to register itself for health check service
 *
 */
int wlc_health_check_register_client(health_check_info_t *hc,
		health_check_module_fn fn, void *module_context_ptr,
		int module_id, uint16 buf_len)

{
	health_check_client_info_t *client_info = NULL;
	uint16 local_buf_size = 0;
	local_buf_size = (buf_len + HEALTH_CHECK_XTLV_HEADER_SIZE);

	ASSERT(fn != NULL);

	/* check if client already registerd or not */
	client_info = hc->client_health_check;
	while (client_info) {
		if (client_info->module_id == module_id) {

			WL_ERROR(("Client already registered\n"));
			return BCME_OK;
		}
		client_info = client_info->next;
	}

	/* create client */
	client_info = (health_check_client_info_t*)MALLOCZ(hc->wlc->osh,
		sizeof(health_check_client_info_t));

	if (client_info == NULL)
	{
		WL_ERROR(("Health check init: Out of memory on buffer "
			"Out of memory on module_id: %d\n", module_id));
		return BCME_NOMEM;
	}
	client_info->osh = hc->wlc->osh;

	/* allocating client buffer */
	client_info->buffer = (uint8*)MALLOCZ(hc->wlc->osh, local_buf_size);

	if (client_info->buffer == NULL)
	{
		WL_ERROR(("Out of memory on buffer "
			"Out of memory on buffer allocation for top level SW entity"));
		MFREE(hc->wlc->osh, hc, sizeof(health_check_info_t));
		return BCME_NOMEM;
	}

	client_info->module_fptr = fn;
	client_info->module_context_ptr = module_context_ptr;
	client_info->module_id = module_id;
	client_info->health_status = HEALTH_CHECK_STATUS_OK;
	client_info->max_length = local_buf_size;
	client_info->osh = hc->wlc->osh;
	client_info->next = hc->client_health_check;
	hc->client_health_check = client_info;

	return BCME_OK;

}

/* health check client dregistration
 *
 */
int
wlc_health_check_dregister_clients(health_check_client_info_t *client_info)
{
	health_check_client_info_t *client_info_next = NULL;

	while (client_info) {
		client_info_next = client_info->next;
		if (client_info->buffer) {

			MFREE(client_info->osh, client_info->buffer, client_info->max_length);
		}
		MFREE(client_info->osh, client_info, sizeof(health_check_client_info_t));
		client_info = client_info_next;
	}
	return BCME_OK;
}

/* Description:
 * This routine will be invoked from  wlc_watchdog()
 * peoridically.
 * It will call the registered client's call back one after
 * another
 */
static void
wlc_health_check_watchdog(void* hdl)
{

	health_check_info_t *hc = NULL;
	uint8* buffer_ptr = NULL;
	uint16 remaining_len = 0, bytes_written = 0;
/*	int health_status =  HEALTH_CHECK_STATUS_OK; */

	if (hdl == NULL) {
		WL_ERROR(("Invalid handle:%s\n", __func__));
		ASSERT(hdl);
		return;
	}

	hc = (health_check_info_t*)hdl;
	if (hc->client_health_check) {
		health_check_client_info_t *client_info = hc->client_health_check;
		while (client_info) {

			if (client_info->buffer) {

				/* keep header room for TLV  */
				buffer_ptr = (client_info->buffer + HEALTH_CHECK_XTLV_HEADER_SIZE);

				/* derive remaining length */
				remaining_len = client_info->buffer +
						client_info->max_length - buffer_ptr;
				client_info->module_fptr(buffer_ptr, remaining_len,
						client_info->module_context_ptr, &bytes_written);

				/* TLV orientation is not implemented for time being
				*/

			}
			client_info = client_info->next;
		}

	}

	return;
}
#endif /* HEALTH_CHECK */
