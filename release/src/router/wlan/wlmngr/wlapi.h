/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
<:label-BRCM:2012:proprietary:standard

 This program is the proprietary software of Broadcom and/or its
 licensors, and may only be used, duplicated, modified or distributed pursuant
 to the terms and conditions of a separate, written license agreement executed
 between you and Broadcom (an "Authorized License").  Except as set forth in
 an Authorized License, Broadcom grants no license (express or implied), right
 to use, or waiver of any kind with respect to the Software, and Broadcom
 expressly reserves all rights in and to the Software and all intellectual
 property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
 NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
 BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

 Except as expressly set forth in the Authorized License,

 1. This program, including its structure, sequence and organization,
    constitutes the valuable trade secrets of Broadcom, and you shall use
    all reasonable efforts to protect the confidentiality thereof, and to
    use this information only in connection with your use of Broadcom
    integrated circuit products.

 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
    PERFORMANCE OF THE SOFTWARE.

 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
    LIMITED REMEDY.
:>
 *
 ************************************************************************/

#ifndef __WL_API_H__
#define __WL_API_H__

#include "wldefs.h"

extern void BcmWl_Uninit(void);
extern void BcmWl_Setup(WL_SETUP_TYPE type);
extern void BcmWl_GetVar(char *varName, char *varValue);
extern void BcmWl_GetVarEx(int argc, char **argv, char *varValue);
extern void BcmWl_SetVar(char *varName, char *varValue);
extern void *BcmWl_getFilterMac(void *pVoid, char *mac, char *ssid, char* ifcName);
extern WL_STATUS BcmWl_addFilterMac(char *mac, char *ssid, char *ifcName);
extern WL_STATUS BcmWl_addFilterMac2(char *mac, char *ssid, char *ifcName, int wl_idx, int idx);
extern WL_STATUS BcmWl_removeFilterMac(char *mac, char *ifcName);
extern WL_STATUS BcmWl_removeAllFilterMac(int wl_idx, int idx);
extern void *BcmWl_IsWdsMacConfigured(char *mac);
extern void BcmWl_ScanWdsMacStart(void);
extern void BcmWl_ScanWdsMacResult(void);
extern void *BcmWl_getScanWdsMac(void *pVoid, char *mac);
extern void *BcmWl_getScanWdsMacSSID(void *pVoid, char *mac, char *ssid);
extern WL_STATUS BcmWl_addWdsMac(char *mac);
extern WL_STATUS BcmWl_removeAllWdsMac(void);
extern void BcmWl_startService(void);
extern void BcmWl_startService_All(void);
extern void BcmWl_stopService(void);
extern void BcmWl_aquireStationList(void);
extern int BcmWl_getNumStations(void);
extern void BcmWl_getStation(int i, char *macAddress, char *associated,  char *authorized, char *ssid, char *ifcName);
extern WL_STATUS BcmWl_enumInterfaces(char *devname);
extern int  BcmWl_GetMaxMbss(void);
extern void  BcmWl_Switch_instance(int index);
extern int BcmWl_isMacFltEmpty(void);
/* exported by libwlctl */
extern void wlctl_cmd(char *cmd);
extern void *BcmWl_getScanParam(void *pVoid, char *mac, char *ssid, int *privacy, int *wpsConfigured);
extern void BcmWl_ScanMacResult(int chose);

#ifdef DSLCPE_SHLIB
#define BCMWL_WLCTL_CMD(x)    wlctl_cmd(x)
#define BCMWL_STR_CONVERT(in, out) bcmConvertStrToShellStr(in, out)
#else
#define BCMWL_WLCTL_CMD(x) bcmSystem(x)
#define BCMWL_STR_CONVERT(in, out) bcmConvertStrToShellStr(in, out)
#endif

#ifdef BCMWAPI_WAI
int BcmWapi_CertListFull();
int BcmWapi_RevokeListFull();
int BcmWapi_AsPendingStatus();
void BcmWapi_SetAsPending(int s);
int BcmWapi_AsStatus();
void BcmWapi_ApCertStatus(char *ifc_name, char *status);
int BcmWapi_CertRevoke(char *sn_str);
int BcmWapi_GetCertList(int argc, char **argv, FILE *wp);
int BcmWapi_CertAsk(char *name, unsigned int period, char *ret_msg);
int BcmWapi_InstallApCert(char *as_file_name, char *usr_file_name);
int BcmWapi_SaveAsCertToMdm();
int BcmWapi_ReadAsCertFromMdm();
int BcmWapi_SaveCertListToMdm(int run);
int BcmWapi_ReadCertListFromMdm();
#endif

#endif
