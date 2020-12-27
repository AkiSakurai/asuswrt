/*
 * Windows Broadcom relay device driver interface.
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
 * $Id: irelay.h 674525 2016-12-09 04:17:19Z $
 */

/* Win9x used FILE_READ_ACCESS instead of FILE_READ_DATA */
#ifndef FILE_READ_DATA
#define FILE_READ_DATA  FILE_READ_ACCESS
#define FILE_WRITE_DATA  FILE_WRITE_ACCESS
# endif

#ifndef IOCTL_NDIS_QUERY_GLOBAL_STATS
#    define _NDIS_CONTROL_CODE(request, method) \
		CTL_CODE(FILE_DEVICE_PHYSICAL_NETCARD, request, method, FILE_ANY_ACCESS)

#    define IOCTL_NDIS_QUERY_GLOBAL_STATS   _NDIS_CONTROL_CODE(0, METHOD_OUT_DIRECT)
#endif // endif

#define IOCTL_OID_RELAY		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, \
					 FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_PKT_TX_RELAY	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, \
					 FILE_WRITE_DATA)
#define IOCTL_PKT_RX_RELAY	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, \
					 FILE_WRITE_DATA | FILE_READ_DATA)
#define IOCTL_LIST		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, \
					 FILE_WRITE_DATA | FILE_READ_DATA)
#define IOCTL_XLIST		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, \
					 FILE_WRITE_DATA | FILE_READ_DATA)
#define IOCTL_VERSION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, \
					 FILE_WRITE_DATA | FILE_READ_DATA)
#define IOCTL_BIND		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, \
					 FILE_WRITE_DATA | FILE_READ_DATA)
#define IOCTL_UNBIND		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, \
					 FILE_WRITE_DATA | FILE_READ_DATA)
#define IOCTL_IR_DUMP		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80A, METHOD_BUFFERED, \
					 FILE_WRITE_DATA | FILE_READ_DATA)

#pragma pack(push, 1)

/* structure definitions for sending packets via IOCTL_PKT_TX_RELAY */

typedef struct {
	ULONG	InstanceID;
	CHAR	Buffer[];
} RxRequest, *PRxRequest;

/* structure definitions for sending packets via IOCTL_PKT_TX_RELAY */

typedef struct {
	ULONG	InstanceID;
	ULONG	Copies;
	CHAR	Buffer[];
} TxRequest, *PTxRequest;

/* structure definitions for relaying OIDs through via IOCTL_OID_RELAY */

typedef struct {
	ULONG	OID;
	ULONG	IsQuery;
	ULONG	BufferLength;
	ULONG	Status;
} RelayHeader, *PRelayHeader;

typedef struct {
	RelayHeader rh;
	UCHAR Buffer[];
} RelayQuery, *PRelayQuery;

typedef struct {
	RelayHeader rh;
	ULONG Cookie;
	UCHAR Buffer[];
} RelaySet, *PRelaySet;

/* structure definitions for sending packets via IOCTL_BIND */

typedef struct {
    CHAR  name[80];
} BindRequest, *PBindRequest;

/* Structure for passing generic OIDs through irelay.  This is the
 * same as a normal set but it does not have a cookie.
 */
typedef RelayQuery RelayGenSet, *PRelayGenSet;

/* structure definitions for relaying OIDs through via IOCTL_VERSION */

typedef struct {
    ULONG   VersionMS;        /* e.g. 0x00030075 = "3.75" */
    ULONG   VersionLS;        /* e.g. 0x00000031 = "0.31" */
} VersionResponse, *PVersionResponse;

/* Combined generic/custom query/set relay oid structure. */

typedef struct _IRelay {
	RelayHeader rh;
	union {
		struct {
			ULONG Cookie;
			UCHAR Buffer[];
		} EpiOid;
		struct {
			UCHAR Buffer[];
		} GenOid;
		UCHAR Buffer[];
	};
} IRELAY, *PIRELAY;

#pragma pack(pop)
