/* D11reglist prototype for Broadcom 802.11abgn
 * Networking Adapter Device Drivers.
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
 * $Id: d11reglist_proto.h 708017 2017-06-29 14:11:45Z $
 */
#ifndef _d11reglist_proto_h_
#define _d11reglist_proto_h_

/* this is for dump_mac */
enum {
	D11REG_TYPE_IHR16  = 0,
	D11REG_TYPE_IHR32  = 1,
	D11REG_TYPE_SCR    = 2,
	D11REG_TYPE_SHM    = 3,
	D11REG_TYPE_TPL    = 4,
	D11REG_TYPE_GE64   = 5,
	D11REG_TYPE_KEYTB  = D11REG_TYPE_GE64,
	D11REG_TYPE_IHRX16 = 6,
	D11REG_TYPE_SCRX   = 7,
	D11REG_TYPE_SHMX   = 8,
	D11REG_TYPE_MAX    = 9
};

#define D11REGTYPENAME {		\
	"ihr", "ihr", "scr", "shm",	\
	"tpl", "keytb", "ihrx", "scrx",	\
	"shmx"				\
}

typedef struct _d11regs_bmp_list {
	uint8 type;
	uint16 addr;
	uint32 bitmap;
	uint8 step;
	uint16 cnt; /* can be used together with bitmap or by itself */
} d11regs_list_t;

#define D11REG_BLK_SIZE		32
typedef struct _d11regs_addr_list {
	uint8 type;
	uint16 cnt;
	uint16 addr[D11REG_BLK_SIZE]; /* allow up to 32 per list */
} d11regs_addr_t;

#endif /* _d11reglist_proto_h_ */
