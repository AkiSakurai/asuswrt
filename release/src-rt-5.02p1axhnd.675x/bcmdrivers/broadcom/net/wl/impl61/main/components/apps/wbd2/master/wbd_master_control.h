/*
 * WBD Steering Policy declarations
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
 * $Id: wbd_master_control.h 774421 2019-04-24 09:41:50Z $
 */

#ifndef _WBD_MASTER_CONTROL_H_
#define _WBD_MASTER_CONTROL_H_

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"

/* Get the number of weightage policies defined */
extern int wbd_get_max_tbss_wght();

/* Get the number of backhaul steering weightage policies defined */
extern int wbd_get_max_tbss_wght_bh();

/* Get the number of threshold policies defined for 2G */
extern int wbd_get_max_tbss_thld_2g();

/* Get the number of threshold policies defined for 5G */
extern int wbd_get_max_tbss_thld_5g();

/* Get the number of threshold policies defined for backhaul */
extern int wbd_get_max_tbss_thld_bh();

/* Get the number of schemes defined */
extern int wbd_get_max_tbss_algo();

/* Get the weightage policy based on index */
extern wbd_tbss_wght_t *wbd_get_tbss_wght(wbd_master_info_t *master_info);

/* Get the weightage policy based on index for backhaul */
extern wbd_tbss_wght_t *wbd_get_tbss_wght_bh(wbd_master_info_t *master_info);

/* Get the threshold policy based on index for 2G */
extern wbd_tbss_thld_t *wbd_get_tbss_thld_2g(wbd_master_info_t *master_info);

/* Get the threshold policy based on index for 5G */
extern wbd_tbss_thld_t *wbd_get_tbss_thld_5g(wbd_master_info_t *master_info);

/* Get the threshold policy based on index for backhaul */
extern wbd_tbss_thld_t *wbd_get_tbss_thld_bh(wbd_master_info_t *master_info);

/* Create the timer to identify the target BSS for weak STA */
int wbd_master_create_identify_target_bss_timer(wbd_master_info_t *master_info,
	i5_dm_clients_type *i5_sta);

/* Send steer req msg */
void wbd_master_send_steer_req(unsigned char *al_mac, unsigned char *sta_mac,
	unsigned char *bssid, i5_dm_bss_type *tbss);

/* Check if the backhaul STA steering is possible or not to any of the backhaul BSS in the network
 * by checking whether it can form a loop if we steer it to the BSS or not
 */
int wbd_master_is_bsta_steering_possible(i5_dm_clients_type *i5_sta);

/* Is TBSS timer created for any of the backhaul STAs */
int wbd_master_is_tbss_created_for_bsta();

/* Start the backhaul optimization on backhaul STA join */
void wbd_master_start_bh_opt(unsigned char *sta_mac);

/* Create the backhaul optimization timer. Check if we should start all the backhaul optimization
 * in the network. This can be called if the device is joined the network or any of the backhaul
 * STA is joined the network
 */
void wbd_master_create_bh_opt_timer(i5_dm_clients_type *i5_sta);
#endif /* _WBD_MASTER_CONTROL_H_ */
