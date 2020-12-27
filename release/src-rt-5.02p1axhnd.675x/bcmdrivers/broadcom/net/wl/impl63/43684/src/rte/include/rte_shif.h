/*
 * API's to access Sensor Hub chip connected to WIFI module
 * through debug UART
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
 * $Id: $
 */

#ifndef	_BCM_SHIF_H_
#define _BCM_SHIF_H_

#define SHIF_DELIM_NOTDEFINED	0xFF
#define SHIF_LEN_NOTDEFINED	-1
#define SHIF_DEFAULT_DELIM	'\n'
#define SHIF_DEFAULT_TIMEOUT	10 /* in ms */
#define SHIF_AWAKE_TIMEOUT	100 /* in ms */

#define SHIF_TX_ALLOWED		(0x0)
#define SHIF_TX_WAITING		(0x1)
#define SHIF_TX_FAILED		(0x2)
#define SHIF_TX_COMPLETE	(0x3)
#define SHIF_RX_WAITING		(0x4)
#define SHIF_RX_FAILED		(0x5)
#define SHIF_RX_COMPLETE	(0x6)

#define SHIF_PKT_NOTDEFINED 0xFF

typedef struct shif_info shif_info_t;
typedef struct _shub_pkt shub_pkt_t;

typedef void (*rx_cb_t)(void *ctx, const shub_pkt_t *buf, uint16 len);
typedef void (*tx_status_cb_t)(void *cb_param, uint8 type, uint8 status);

shif_info_t * rte_shif_init(si_t *sih, osl_t *osh, uint8 sh_pin, uint8 int_pin,
		tx_status_cb_t tx_status_cb);
void rte_shif_deinit(shif_info_t *shif_p);
int rte_shif_send(shif_info_t *shifp, char *buf, uint16 pkt_type, bool retry, uint16 len);
uint8 rte_shif_get_rxdata(shif_info_t *shif_p, char *buf, uint8 count);
int rte_shif_wp_enable(shif_info_t *shif_p, int enable, uint8 sh_pin, uint8 int_pin);

int rte_shif_config_rx_completion(shif_info_t *shif_p, char delim, int len,
		int timeout, rx_cb_t rx_cb, void *param);
int rte_shif_up_fn(shif_info_t *shifp);
void rte_shif_enable(shif_info_t *shifp, bool enable);
#endif /* _BCM_SHIF_H_ */
