/*
 * Windows/NDIS common macros that are the same for all brands (so far...)
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
 * $Id: vendor.h 674525 2016-12-09 04:17:19Z $
 */

#ifndef _VENDOR_H_
#define _VENDOR_H_

#define WIDE1(x)      L ## x
#define WIDE(x)      WIDE1(x)
#define TOSTR1(x)    # x
#define TOSTR(x)     TOSTR1(x)

/* The RELAY macros depend upon RELAY_FILE_BASE & RELAY_DEV_BASE */
#define RELAY_NT_NAME           	RELAY_FILE_BASE ".SYS"
#define RELAY_NT_FILE           	"\\\\.\\" RELAY_DEV_BASE
#define RELAY_DEVICE_NAME       	"\\Device\\" RELAY_DEV_BASE
#define RELAY_DOS_DEVICE_NAME   	"\\DosDevices\\" RELAY_DEV_BASE
#define RELAY_WIDE_PROT_NAME    	WIDE(RELAY_DEV_BASE)

/* The HWD macros depend upon HWD_FILE_BASE & HWD_DEV_BASE */
#define HWD_DRIVER_SYS			HWD_FILE_BASE ".SYS"
#define HWD_DRIVER_SERVICE		HWD_DEV_BASE
#define HWD_NT_DEVICE_NAME		WIDE("\\Device\\") WIDE(HWD_DEV_BASE)
#define HWD_DOS_DEVICE_NAME		WIDE("\\DosDevices\\") WIDE(HWD_DEV_BASE)

/* These seem to mostly be used by the NDI installer and the
 * coinstaller to help recontruct registry keys when things go bad.
 */
#define DRIVER_KEY			WIDE(NIC_DEV_BASE)
#define DRIVER_NAME			WIDE(NIC_FILE_BASE) WIDE(".SYS")
#define DEFAULT_SERVICE			NIC_DEV_BASE
#define ENET_SERVICE			ENIC_DEV_BASE
#define DELETE_VALUE_NAME		NIC_DEV_BASE " File Delete"
#define NIC_DRIVER_SYS          	NIC_FILE_BASE ".SYS"

#define VEN_PRODUCTVERSION_STR  	VEN_FILEVERSION_STR

#define FILEVERSION_STR         	VEN_FILEVERSION_STR
#define PRODUCTVERSION_STR      	VEN_PRODUCTVERSION_STR
#define PRODUCTNAME             	VEN_PRODUCTNAME
#define COMPANYNAME             	VEN_COMPANYNAME

/* Define our own company and product name */
#define VER_COMPANYNAME_STR		VEN_COMPANYNAME "\0"
#define VER_PRODUCTNAME_STR		VEN_PRODUCTNAME "\0"
#define VER_LEGALCOPYRIGHT_YEARS	"1998-2012"
#define LEGALCOPYRIGHT			COMPANYNAME
#define VER_LEGALCOPYRIGHT_STR		VER_LEGALCOPYRIGHT_YEARS ", " LEGALCOPYRIGHT \
					" All Rights Reserved."
#define VER_PRODUCTVERSION_STR		VEN_PRODUCTVERSION_STR "\0"
#define VER_PRODUCTVERSION		VEN_FILEVERSION_NUM

#if defined(BCM_USB)
#define BCM_PLATFORM			"USB"
#else
#define BCM_PLATFORM			"PCI"
#endif // endif

#define JOIN2(a, b)			a ## b
#define JOIN(a, b)			JOIN2(a, b)

#define EPI_CTRL_DLL			"3C410CTL.DLL"

#define VEN_REGENTRY			"Broadcom"
#define VEN_COMPANYNAME			"Broadcom Corporation"
#define VEN_PRODUCTNAME			"Broadcom iLine10(tm) " BCM_PLATFORM " Network Adapter"
#define VEN_DRIVER_DESCRIPTION		"Broadcom iLine10(tm) Network Adapter Driver"
#define VEN_FILEVERSION_NUM     	EPI_MAJOR_VERSION, EPI_MINOR_VERSION, EPI_RC_NUMBER, \
					EPI_INCREMENTAL_NUMBER
#define VEN_FILEVERSION_STR     	TOSTR(EPI_MAJOR_VERSION) "." TOSTR(EPI_MINOR_VERSION) "." \
					TOSTR(EPI_RC_NUMBER) "." TOSTR(EPI_INCREMENTAL_NUMBER) "\0"

#define RELAY_FILE_BASE         	"BCM42RLY"
#define RELAY_DEV_BASE          	RELAY_FILE_BASE

#define HWD_FILE_BASE           	"BCM42XHW"
#define HWD_DEV_BASE            	HWD_FILE_BASE

#if defined(BCM_USB)
#define NIC_DEV_BASE			"BCM42U"
#define NIC_FILE_BASE			"BCM42U"
#else
#define NIC_FILE_BASE			"BCM42XX"
#define NIC_DEV_BASE			NIC_FILE_BASE
#endif // endif

#define VEN_COINSTALLER_NAME    	"BCM42COI.DLL"
/* this needs to match ProductSoftwareName in the oemsetup.inf file */
#define PERFMON_SERVICE_NAME    	"BCM42XX"

#endif /* _VENDOR_H_ */
