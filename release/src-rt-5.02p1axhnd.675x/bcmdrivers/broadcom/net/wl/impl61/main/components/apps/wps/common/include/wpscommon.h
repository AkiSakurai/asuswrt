/*
 * WPS Common
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
 * $Id: wpscommon.h 768362 2018-10-11 06:45:41Z $
 */

#ifndef _WPS_COMMON_
#define _WPS_COMMON_

#include <wpstypes.h>
#include <portability.h>
#include <wps_utils.h>

/* Config methods */
#define WPS_CONFMET_USBA            0x0001	/* Deprecated in WSC 2.0 */
#define WPS_CONFMET_ETHERNET        0x0002	/* Deprecated in WSC 2.0 */
#define WPS_CONFMET_LABEL           0x0004
#define WPS_CONFMET_DISPLAY         0x0008
#define WPS_CONFMET_EXT_NFC_TOK     0x0010
#define WPS_CONFMET_INT_NFC_TOK     0x0020
#define WPS_CONFMET_NFC_INTF        0x0040
#define WPS_CONFMET_PBC             0x0080
#define WPS_CONFMET_KEYPAD          0x0100
/* WSC 2.0 */
#define WPS_CONFMET_VIRT_PBC        0x0280
#define WPS_CONFMET_PHY_PBC         0x0480
#define WPS_CONFMET_VIRT_DISPLAY    0x2008
#define WPS_CONFMET_PHY_DISPLAY     0x4008

#define REGISTRAR_ID_STRING        "WFA-SimpleConfig-Registrar-1-0"
#define ENROLLEE_ID_STRING        "WFA-SimpleConfig-Enrollee-1-0"

#define BUF_SIZE_64_BITS    8
#define BUF_SIZE_128_BITS   16
#define BUF_SIZE_160_BITS   20
#define BUF_SIZE_256_BITS   32
#define BUF_SIZE_512_BITS   64
#define BUF_SIZE_1024_BITS  128
#define BUF_SIZE_1536_BITS  192
#define NVRAM_BUFSIZE	100

#define PERSONALIZATION_STRING  "Wi-Fi Easy and Secure Key Derivation"
#define PRF_DIGEST_SIZE         BUF_SIZE_256_BITS
#define KDF_KEY_BITS            640

#define WPS_RESULT_SUCCESS_RESTART			100
#define WPS_RESULT_SUCCESS					101
#define WPS_RESULT_PROCESS_TIMEOUT			102
#define WPS_RESULT_USR_BREAK				103
#define WPS_RESULT_FAILURE					104
#define WPS_RESULT_ENROLLMENT_PINFAIL			105
#define WPS_RESULT_REGISTRATION_PINFAIL			106
#define WPS_RESULT_ENROLLMENT_M2DFAIL			107
#define WPS_RESULT_ENROLLMENT_CHOFAIL			108
#define WPS_RESULT_REGISTRATION_CHOFAIL			109

typedef enum {
	SCMODE_UNKNOWN = 0,
	SCMODE_STA_ENROLLEE,
	SCMODE_STA_REGISTRAR,
	SCMODE_AP_ENROLLEE,
	SCMODE_AP_REGISTRAR,
} WPS_SCMODE;

typedef enum {
	WPS_INIT		= 0,	/* Idle and ready to be initiated */
	WPS_ASSOCIATED		= 1,	/* Any request event was detected, example PBC */
	WPS_OK			= 2,	/* WPS procedure (M1 ~ M8) was successfully done */
	WPS_MSG_ERR		= 3,	/* Any error during WPS procedure */
	WPS_TIMEOUT		= 4,	/* WPS procedure was incomplete within overall timeout */
	WPS_SENDM2		= 5,	/* Send M2 msg */
	WPS_SENDM7		= 6,	/* Send M7 msg */
	WPS_MSGDONE		= 7,	/* WPS DONE msg was processed completely */
	WPS_PBCOVERLAP		= 8,	/* PBC overlap detected */
	WPS_FIND_PBC_AP		= 9,	/* Found an AP with selregistar as true in PBC method */
	WPS_ASSOCIATING		= 10,	/* WPS sta associate to AP after finding PBC AP */
	WPS_FIND_SEL_AP		= 11,	/* Found a AP with selregistar as true in PIN method */
	WPS_NFC_WR_CFG		= 12,
	WPS_NFC_WR_PW		= 13,
	WPS_NFC_WR_CPLT		= 14,
	WPS_NFC_RD_CFG		= 15,
	WPS_NFC_RD_PW		= 16,
	WPS_NFC_RD_CPLT		= 17,
	WPS_NFC_HO_S		= 18,
	WPS_NFC_HO_R		= 19,
	WPS_NFC_HO_NDEF		= 20,
	WPS_NFC_HO_CPLT		= 21,
	WPS_NFC_OP_ERROR	= 22,
	WPS_NFC_OP_STOP		= 23,
	WPS_NFC_OP_TO		= 24,
	WPS_NFC_FM		= 25,
	WPS_NFC_FM_CPLT		= 26,
	WPS_NFC_HO_DPI_MISMATCH	= 27,
	WPS_NFC_HO_PKH_MISMATCH	= 28,
	WPS_MAP_TIMEOUT		= 29	/* Multiap timeout occured in WPS sta */
} WPS_SCSTATE;

typedef enum {
	TRANSPORT_TYPE_UFD = 1,
	TRANSPORT_TYPE_EAP,
	TRANSPORT_TYPE_WLAN,
	TRANSPORT_TYPE_NFC,
	TRANSPORT_TYPE_BT,
	TRANSPORT_TYPE_IP,
	TRANSPORT_TYPE_UPNP_CP,
	TRANSPORT_TYPE_UPNP_DEV,
	TRANSPORT_TYPE_MAX /* insert new transport types before TRANSPORT_TYPE_MAX */
} TRANSPORT_TYPE;

#define SM_FAILURE    0
#define SM_SUCCESS    1
#define SM_SET_PASSWD 2
#define WPS_WLAN_EVENT_TYPE_PROBE_REQ_FRAME	1
#define WPS_WLAN_EVENT_TYPE_EAP_FRAME		2

#define WPSM_WPSA_PORT		40000 + (1 << 6)
#define WPSAP_WPSM_PORT		40000 + (1 << 7)
#define WPS_LOOPBACK_ADDR		"127.0.0.1"

extern void RAND_bytes(unsigned char *buf, int num);
bool wps_getUpnpDevGetDeviceInfo(void *mc_dev);
void wps_setUpnpDevGetDeviceInfo(void *mc_dev, bool value);

#endif /* _WPS_COMMON_ */
