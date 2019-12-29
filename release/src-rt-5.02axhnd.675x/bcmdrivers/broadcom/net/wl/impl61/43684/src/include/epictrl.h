/*
 * Windows/NDIS Broadcom HNBU control driver API.
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
 * $Id: epictrl.h 676803 2016-12-24 20:04:01Z $
 */

#ifndef _EPICTRL_H_
#define _EPICTRL_H_

#include "typedefs.h"
#include "ethernet.h"

/* The following ifdef block is the standard way of creating macros
 * which make exporting from a DLL simpler. All files within this DLL
 * are compiled with the EPICTRL_EXPORTS symbol defined on the
 * command line. this symbol should not be defined on any project that
 * uses this DLL. This way any other project whose source files
 * include this file see EPICTRL_API functions as being imported from
 * a DLL, where as this DLL sees symbols defined with this macro as
 * being exported.
 */

#if defined(BUILD_EPICTRL_DLL)
#       define EPICTRL_API __declspec(dllexport)
#elif defined(USE_EPICTRL_DLL)
#       define EPICTRL_API __declspec(dllimport)
#else
#       define EPICTRL_API
#endif // endif

#ifdef __cplusplus
extern "C" {
#endif // endif

#define DEFAULT_EPICTRL_OVERHEAD	512

/* definitions of adapter types used in the type field below */
typedef enum {
	IR_UNKNOWN = -1,
	IR_802_3 = 0x0,
	IR_ILINE,
	IR_ENET,
	IR_CODEC,
	IR_WIRELESS,
	IR_WWAN,
	IR_USB,
	IR_LOOPBACK,
	IR_VIRTUAL_WIRELESS,
	IR_WIMAX,

	/* Add new types above this line and keep this one at the end!! */
	IR_MAXTYPE
} IRTYPE, *PIRTYPE;

#ifdef NEED_IR_TYPES
	static PCHAR ir_typestrings[] = {
	    "802.3",
	    "iline",
	    "enet",
	    "codec",
	    "wireless",
	    "wwan",
	    "usb",
	    "loopback",
	    "virtualwireless",
	    "wimax"
	};
	static PCHAR ir_prefixstrings[] = {
	    "??",
	    "il",
	    "et",
	    "co",
	    "wl",
	    "wn",
	    "ub",
	    "lp",
	    "vw",
	    "wx"
	};
#endif /* NEED_IR_TYPES */

#define IR_BRAND_MAX 10
#define MAX_ADAPTERS 10

typedef struct _ADAPTER {
	union {
		TCHAR			regkey[10];
		TCHAR			shortname[10];
	};
	union {
		TCHAR			adaptername[80];
		TCHAR			name[80];
	};
	TCHAR			wminame[256];
	BYTE			macaddr[6];
	TCHAR			description[80];
	TCHAR			componentId[64];
	TCHAR			brand[IR_BRAND_MAX];
	DWORD			instance;
	IRTYPE			type;
	BOOL			valid;
	BOOL			bvirtual;
} ADAPTER, *PADAPTER;

typedef DWORD WINERR;

EPICTRL_API WINERR ir_init(HANDLE *);
EPICTRL_API WINERR ir_adapter_list(HANDLE, PADAPTER, PDWORD);
EPICTRL_API WINERR ir_exit(HANDLE);
EPICTRL_API WINERR ir_bind(HANDLE m_dh, LPCTSTR DeviceName);
EPICTRL_API WINERR ir_unbind(HANDLE m_dh);
EPICTRL_API WINERR ir_queryinformation(HANDLE m_dh, ULONG oid, PBYTE inbuf, PDWORD inlen);
EPICTRL_API WINERR ir_setinformation(HANDLE m_dh, ULONG oid, PBYTE inbuf, PDWORD inlen);
EPICTRL_API WINERR ir_adapter_reinitialize(LPCTSTR DeviceName);
EPICTRL_API void ir_usewmiset(BOOL useWMI);
EPICTRL_API WINERR ir_adapter_disable(LPCTSTR DeviceName);
EPICTRL_API WINERR ir_adapter_enable(LPCTSTR componentId);

typedef struct _dll_private {
    HANDLE handle;
    PVOID   param;
	PVOID   reserved; /* epictrl internal use only. DO NOT USE! */
} dll_private_t;

#ifdef __cplusplus
}
#endif // endif

#endif /* _EPICTRL_H_ */
