/*
 * Health check module header file
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
 * $Id: hnd_hchk.h 650948 2016-07-23 04:28:56Z $
 */

#ifndef _HND_HEALTH_CHECK_H
#define _HND_HEALTH_CHECK_H

#include <typedefs.h>
#include <dngl_defs.h>

/* Health check status format:
 * reporting status size = uint32
 * 8 LSB bits are reserved for: WARN (0), ERROR (1), and other levels
 * MSB 24 bits are reserved for client to fill in its specific status
 */
#define HEALTH_CHECK_STATUS_OK			0
/* Bit positions. */
#define HEALTH_CHECK_STATUS_WARN		0x1
#define HEALTH_CHECK_STATUS_ERROR		0x2
#define HEALTH_CHECK_STATUS_TRAP		0x4
/* Indication that required information is populated in log buffers */
#define HEALTH_CHECK_STATUS_INFO_LOG_BUF	0x80
#define HEALTH_CHECK_STATUS_MASK		(0xFF)

#define HEALTH_CHECK_STATUS_MSB_SHIFT   8

typedef int (*health_check_fn)(uint8 *buffer, uint16 length, void *context,
	int16 *bytes_written);
typedef struct health_check_info health_check_info_t;
typedef struct health_check_client_info health_check_client_info_t;

/* Every top level SW entity calls this function to get a health check
 * descriptor. A descriptor is exclusively for that entity and cannot be
 * shared. The entity decides when it wants to run health check on its
 * modules. Every entity may decide to execute health check independently
 * Parameters:
 * top_level_module_type: A unique identifier for an entity in the system
 * buffer size: How much buffer is needed to populate any helpful data in
 * in event of an error. This data goes as payload in SOCRAM IND event.
 */
extern health_check_info_t* health_check_init(osl_t *osh,
	hchk_sw_entity_t top_level_module_type, uint16 buffer_size);

/* Every module within an entity calls this function to register its health check
 * callback. The health check callback is a strict observer in the system and should
 * not alter state of the system.
 * The callback returns appropriate health check status as mentioned above.
 * When an error is detected, the module could populate any data that it feels
 * is required for debugging in XTLV format.
 * Parameters:
 * desc: The health check descriptor in which a mdoules callback is going to be
 * registered
 * fn: A module specific health check callback function that will be registered in
 * the descriptor provided
 * context: Any contextual information that module needs to execute its health check
 * module_type: A module identifier in a entity.
 * return value: health_check_client_info_t*
 */
extern health_check_client_info_t*
	health_check_module_register(health_check_info_t *desc,
		health_check_fn fn, void* context, int module_id);

/* Unregister a module from health check */
extern int
health_check_module_unregister(health_check_info_t *desc,
	health_check_client_info_t *client);

/* Execute health check. This function calls registered callbacks in the descriptor
 * provided. Each entity calls this function when it thinks is the right time to do
 * health check.
 * Parameters:
 * desc: Descriptor on which health check needs to be executed
 * returns BCME status codes.
 */
extern int health_check_execute(health_check_info_t *desc,
	health_check_client_info_t** module_ids,
	uint16 num_modules);

#endif /* Health Check */
