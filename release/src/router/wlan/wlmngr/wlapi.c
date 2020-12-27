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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cms.h"
#include "cms_log.h"
#include "cms_util.h"
#include "cms_core.h"
#include "cms_msg.h"
#include "cms_dal.h"
#include "cms_cli.h"

#include <bcmnvram.h>
#include "wlapi.h"
#include "wlmngr.h"
#include "wlsyscall.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include "wllist.h"

int wl_index =0;
extern int wl_cnt;

//**************************************************************************
// Function Name: BcmWl_Uninit
// Description  : wireless uninitialization stub.
// Returns      : None.
//**************************************************************************
void BcmWl_Uninit(void) {
   wlmngr_unInit(wl_index);
   wlmngr_wlIfcDown(wl_index);
}

//**************************************************************************
// Function Name: BcmWl_Setup
// Description  : configure and setup wireless interface.
// Returns      : None.
//**************************************************************************
void BcmWl_Setup(WL_SETUP_TYPE type) {
    wlmngr_setup(wl_index, type);
}

//**************************************************************************
// Function Name: getVar
// Description  : get value by variable name.
// Parameters   : var - variable name.
// Returns      : value - variable value.
//**************************************************************************
void BcmWl_GetVar(char *varName, char *varValue) {
    if (wl_index < wl_cnt) {
		 wlmngr_getVar(wl_index, varName, varValue);
    }
    else 
		*varValue = '\0';
}

//**************************************************************************
// Function Name: getVarEx
// Description  : get value with all inputs
// Parameters   : var - variable name.
// Returns      : value - variable value.
//**************************************************************************
void BcmWl_GetVarEx(int argc, char **argv, char *varValue) {
    if (wl_index < wl_cnt) {
               wlmngr_getVarEx(wl_index, argc, argv, varValue);
    }
    else 
		*varValue = '\0';
}

//**************************************************************************
// Function Name: setVar
// Description  : set value by variable name.
// Parameters   : var - variable name.
// Returns      : value - variable value.
//**************************************************************************
void BcmWl_SetVar(char *varName, char *varValue) {
    if (wl_index < wl_cnt) {
                wlmngr_setVar(wl_index, varName, varValue);
    }
}

/***************************************************************************
// Function Name: BcmWl_GetSsidIdex.
// Description  : Get current Ssid Idx 
// Parameters   : NONE
// Returns      : index number starts 0-3, -1 for failure.
****************************************************************************/
static int BcmWl_GetSsidIdx() {

     char wlSsidIdx[WL_MID_SIZE_MAX];
     int idx=0;
     BcmWl_GetVar("wlSsidIdx", wlSsidIdx);
     idx=wlSsidIdx[0]-'0'; 
     if(idx<0 ||idx>3) return -1;
     return idx;
}

//temparary disable macfilter
/***************************************************************************
// Function Name: BcmWl_getFilterMac.
// Description  : retrieve the MAC address filter information.
// Parameters   : pVoid - point to start position in the list.
//                mac - buffer to contains the information.
// Returns      : FltMacNode or NULL if not found.
****************************************************************************/
void* BcmWl_getFilterMac(void *pVoid, char *mac, char *ssid, char* ifcName) {
   WL_FLT_MAC_ENTRY *entry;


   WIRELESS_MSSID_VAR *m_wlMssidVarPtr;
   int idx= BcmWl_GetSsidIdx(); 
   if(idx<0) 
	    return NULL;
   m_wlMssidVarPtr =  &(m_instance_wl[wl_index].m_wlMssidVar[idx]);

   list_get_next( ( (struct wl_flt_mac_entry *)pVoid),  (m_wlMssidVarPtr->m_tblFltMac),  entry);
	
   if ( entry != NULL ) {
      strncpy(mac, entry->macAddress, WL_MID_SIZE_MAX);
      if(ssid && *(entry->ssid))
            strncpy(ssid, entry->ssid, WL_SSID_SIZE_MAX);
      if(ifcName && *(entry->ifcName))
            strncpy(ifcName, entry->ifcName, WL_SM_SIZE_MAX);            
   }
    
   return entry;
}

/***************************************************************************
// Function Name: BcmWl_addFilterMac.
// Description  : add the MAC address filter to the table.
// Parameters   : mac - the given MAC address filter object.
// Returns      : operation status.
****************************************************************************/
WL_STATUS BcmWl_addFilterMac(char *mac, char *ssid, char *ifcName) {
   struct wl_flt_mac_entry *entry;

   WIRELESS_MSSID_VAR *m_wlMssidVarPtr;
   int idx= BcmWl_GetSsidIdx(); 
   if(idx<0) 
	    return WL_STS_ERR_GENERAL;
   m_wlMssidVarPtr =  &(m_instance_wl[wl_index].m_wlMssidVar[idx]);

   entry = malloc( sizeof(WL_FLT_MAC_ENTRY));
   if ( entry != NULL ) {
        memset(entry, 0, sizeof(WL_FLT_MAC_ENTRY));
        strncpy(entry->macAddress, mac, sizeof(entry->macAddress));
        if(ssid && *ssid)
               strncpy(entry->ssid, ssid, sizeof(entry->ssid));
	   	   
        if(ifcName && *ifcName) {
	       strncpy(entry->ifcName, ifcName, sizeof(entry->ifcName));
        }
	    
	list_add( entry, m_wlMssidVarPtr->m_tblFltMac, struct wl_flt_mac_entry);
	
        return WL_STS_OK;
   }

 return WL_STS_ERR_GENERAL;
}

/***************************************************************************
// Function Name: BcmWl_addFilterMac2.
// Description  : add the MAC address filter to the table.
// Parameters   : mac, ssid, ifcName, wlan idx, ssid.
// Returns      : operation status.
****************************************************************************/
WL_STATUS BcmWl_addFilterMac2(char *mac, char *ssid, char *ifcName,int wl_idx, int idx) {
   struct wl_flt_mac_entry *entry;

   WIRELESS_MSSID_VAR *m_wlMssidVarPtr;
   m_wlMssidVarPtr =  &(m_instance_wl[wl_idx].m_wlMssidVar[idx]);

   entry = malloc( sizeof(WL_FLT_MAC_ENTRY));
   if ( entry != NULL ) {
        memset(entry, 0, sizeof(WL_FLT_MAC_ENTRY));
        strncpy(entry->macAddress, mac, sizeof(entry->macAddress));
        if(ssid && *ssid)
              strncpy(entry->ssid, ssid, sizeof(entry->ssid));
	   	   
        if(ifcName && *ifcName) {
	       strncpy(entry->ifcName, ifcName, sizeof(entry->ifcName));
        }
	    
	list_add( entry, m_wlMssidVarPtr->m_tblFltMac, struct wl_flt_mac_entry);
	
        return WL_STS_OK;
   }

 return WL_STS_ERR_GENERAL;
}

/***************************************************************************
// Function Name: BcmWl_removeFilterMac.
// Description  : remove the MAC address filter out of the table.
// Parameters   : mac - the given MAC address filter object.
// Returns      : operation status.
****************************************************************************/
WL_STATUS BcmWl_removeFilterMac(char *mac, char *ifcName) {

    struct wl_flt_mac_entry *pos;
    WIRELESS_MSSID_VAR *m_wlMssidVarPtr;
    int idx= BcmWl_GetSsidIdx(); 
    if(idx<0) 
	    return WL_STS_ERR_GENERAL;
    m_wlMssidVarPtr =  &(m_instance_wl[wl_index].m_wlMssidVar[idx]);

    if( !ifcName)
        return WL_STS_OK;
	
    list_for_each(pos, (m_wlMssidVarPtr->m_tblFltMac) )  {
        if (!strcmp(pos->ifcName, ifcName) && !strcmp(pos->macAddress, mac)) {
            list_remove(m_wlMssidVarPtr->m_tblFltMac, struct wl_flt_mac_entry, pos);
	     break;
        }
    }

   return WL_STS_OK;
}


/***************************************************************************
// Function Name: BcmWl_removeAllFilterMac.
// Description  : remove all the MAC addresses filter out of the table.
// Parameters   : the filter object.
// Returns      : operation status.
****************************************************************************/
WL_STATUS BcmWl_removeAllFilterMac(int wl_idx, int idx) {
    WIRELESS_MSSID_VAR *m_wlMssidVarPtr;
    m_wlMssidVarPtr =  &(m_instance_wl[wl_idx].m_wlMssidVar[idx]);
    if(m_wlMssidVarPtr!=NULL && m_wlMssidVarPtr->m_tblFltMac!=NULL){
        fprintf(stderr,"removeAllFilterMac works?\n");
	list_del_all(m_wlMssidVarPtr->m_tblFltMac,struct wl_flt_mac_entry);
    }
    return WL_STS_OK;
}
    
/***************************************************************************
// Function Name: BcmWl_IsWdsMacConfigured.
// Description  : verify if the WDS MAC address has selected.
// Parameters   : mac - buffer to contains the information.
// Returns      : WdsMacNode or NULL if not found.
****************************************************************************/
void* BcmWl_IsWdsMacConfigured(char *mac) {
   WL_FLT_MAC_ENTRY *entry =NULL;

   list_get( (m_instance_wl[wl_index].m_tblWdsMac), struct wl_flt_mac_entry, macAddress, entry, mac, strcmp);

   return (void *)entry;
}

/***************************************************************************
// Function Name: BcmWl_ScanMacResult.
// Description  : scan chosen AP's MAC address information.
// Returns      : None.
****************************************************************************/
void BcmWl_ScanMacResult(int chose) {

    wlmngr_scanResult(wl_index, chose);
}
/***************************************************************************
// Function Name: BcmWl_ScanWdsMacResult.
// Description  : scan WDS AP's MAC address information.
// Returns      : None.
****************************************************************************/
void BcmWl_ScanWdsMacResult(void) {

    wlmngr_scanWdsResult(wl_index);
}

/***************************************************************************
// Function Name: BcmWl_ScanWdsMacStart.
// Description  : scan WDS AP's MAC address information.
// Returns      : None.
****************************************************************************/
void BcmWl_ScanWdsMacStart(void) {
   list_del_all( (m_instance_wl[wl_index].m_tblScanWdsMac), struct wl_flt_mac_entry);
   
    wlmngr_scanWdsStart(wl_index);
}

/***************************************************************************
// Function Name: BcmWl_getScanWdsMac.
// Description  : retrieve the WDS AP's MAC address information.
// Parameters   : pVoid - point to start position in the list.
//                mac - buffer to contains the information.
// Returns      : WdsMacNode or NULL if not found.
****************************************************************************/
void* BcmWl_getScanWdsMac(void *pVoid, char *mac) {
   WL_FLT_MAC_ENTRY *entry;

   list_get_next( ((WL_FLT_MAC_ENTRY *)pVoid), (m_instance_wl[wl_index].m_tblScanWdsMac), entry);
   if ( entry != NULL )
   	  strncpy( mac, entry->macAddress, WL_MID_SIZE_MAX );
   
   return (void *)entry;
}

/***************************************************************************
// Function Name: BcmWl_getScanWdsMacSSID
// Description  : retrieve WDS AP's MAC address *and SSID*
// Parameters   : pVoid - point to start position in the list
//                mac - buffer for mac addr
//                ssid - buffer for ssid
// Returns      : WdsMacNode or NULL if not found
****************************************************************************/

void *BcmWl_getScanWdsMacSSID(void *pVoid, char *mac, char *ssid)
{
   WL_FLT_MAC_ENTRY *entry = NULL;

   list_get_next( ((WL_FLT_MAC_ENTRY *)pVoid), (m_instance_wl[wl_index].m_tblScanWdsMac), entry);
   if ( entry != NULL ) {
       strncpy( mac, entry->macAddress, WL_MID_SIZE_MAX );
       strncpy(ssid, entry->ssid, WL_SSID_SIZE_MAX);
   }
   return (void *)entry;
}

/***************************************************************************
// Function Name: BcmWl_addWdsMac.
// Description  : add the WDS AP's MAC address to the table.
// Parameters   : mac - the given WDS AP MAC address object.
// Returns      : operation status.
****************************************************************************/
WL_STATUS BcmWl_addWdsMac(char *mac) {
   WL_FLT_MAC_ENTRY *entry;
   WL_STATUS ret = WL_STS_OK;

   entry = malloc( sizeof(WL_FLT_MAC_ENTRY));
   memset( entry, 0, sizeof(WL_FLT_MAC_ENTRY) );
   
   if ( entry != NULL ) {
        strncpy(entry->macAddress, mac, sizeof(entry->macAddress));

        list_add( entry, m_instance_wl[wl_index].m_tblWdsMac, struct wl_flt_mac_entry);
   }
   else {
   	printf("%s@%d Could not allocat memmro to add WDS Entry\n", __FUNCTION__, __LINE__);
   	ret = WL_STS_ERR_GENERAL;
   }

   return ret;
}

/***************************************************************************
// Function Name: BcmWl_removeAllWdsMac.
// Description  : remove all WDS AP's MAC address out of the table.
// Parameters   : None.
// Returns      : operation status.
****************************************************************************/
WL_STATUS BcmWl_removeAllWdsMac(void) {
    list_del_all( (m_instance_wl[wl_index].m_tblWdsMac), struct wl_flt_mac_entry);
     return WL_STS_OK;
}

/***************************************************************************
// Function Name: BcmWl_startService
// Description  : start up wireless service.
// Returns      : None.
****************************************************************************/
void BcmWl_startService() {
   wlmngr_startServices(wl_index);
}

/***************************************************************************
// Function Name: BcmWl_stopService
// Description  : stop wireless service.
// Returns      : None.
****************************************************************************/
void BcmWl_stopService() {
   wlmngr_stopServices(wl_index);
}

/***************************************************************************
// Function Name: BcmWl_aquireStationList
// Description  : Fill station list from wlctl commands
// Returns      : None
****************************************************************************/
void BcmWl_aquireStationList() {
   wlmngr_aquireStationList(wl_index);
}

/***************************************************************************
// Function Name: BcmWl_getNumStations
// Description  : Return number of stations in the list
// Returns      : Number of stations
****************************************************************************/
int BcmWl_getNumStations() {
   return wlmngr_getNumStations(wl_index);
}

/***************************************************************************
// Function Name: BcmWl_getStation
// Description  : Return one station list entry
// Parameters   : i - station index
//                macAddress - MAC address
//                associated - Whether the station is associated
//                authorized - Whether the station is authorized
// Returns      : None
****************************************************************************/
void BcmWl_getStation(int i, char *macAddress, char *associated,  char *authorized, char *ssid, char *ifcName) {
    wlmngr_getStation(wl_index, i, macAddress, associated, authorized, ssid, ifcName);
}

//**************************************************************************
// Function Name: BcmWl_enumInterfaces
// Description  :  a simple enumeration of wireless intefaces, caller must call 
//                until null devicename is returned, assuming no multitasking
//                callers
// Input/Output : aller allocated char *devname
// Returns      : return other than WL_STS_OK if enumeration ends
//**************************************************************************
WL_STATUS BcmWl_enumInterfaces(char *devname)
{
   return wlmngr_enumInterfaces(wl_index, devname);	
}

//**************************************************************************
// Function Name: BcmWl_GetMaxMbss
// Description  : get numbers of MBSS supported
// Input/Output : none
// Returns      : return numbers of MBSS supported
//**************************************************************************
int  BcmWl_GetMaxMbss(void)
{
     return wlmngr_getMaxMbss(wl_index);
}

//**************************************************************************
// Function Name: BcmWl_Switch_instance
// Description  : get current index in use
// Input/Output : index
// Returns      : none
//**************************************************************************

void  BcmWl_Switch_instance(int index)
{
    // This function allows WEB GUI (userspace/private/apps/httpd/cgi_wl.c)
    // to get the adaptor index being configured by functions in this file.
    wl_index = index;
}

//**************************************************************************
// Function Name: BcmWl_isMacFltEmpty
// Description  : Check whether Mac filter list is empty 
// Input/Output : none
// Returns      : Return 0 If find entry in list. Otherwise return 1 (list is empty)
//**************************************************************************

int BcmWl_isMacFltEmpty(void) {
    struct wl_flt_mac_entry *pos;
    int ret = 1;	

    WIRELESS_MSSID_VAR *m_wlMssidVarPtr;
    int idx= BcmWl_GetSsidIdx(); 
    if(idx<0) 
	    return 1;

    m_wlMssidVarPtr =  &(m_instance_wl[wl_index].m_wlMssidVar[idx]);
    list_for_each(pos, (m_wlMssidVarPtr->m_tblFltMac) )  {
           /* Find entry in list */
           ret = 0;
    }
    return ret;
}

/***************************************************************************
// Function Name: BcmWl_getScanParam
// Returns      : WdsMacNode or NULL if not found
****************************************************************************/

void *BcmWl_getScanParam(void *pVoid, char *mac, char *ssid, int *privacy, int *wpsConfigured)
{
   WL_FLT_MAC_ENTRY *entry = NULL;

   list_get_next( ((WL_FLT_MAC_ENTRY *)pVoid), (m_instance_wl[wl_index].m_tblScanWdsMac), entry);
   if ( entry != NULL ) {
       strncpy( mac, entry->macAddress, WL_MID_SIZE_MAX );
       strncpy(ssid, entry->ssid, WL_SSID_SIZE_MAX);
       *privacy = entry->privacy;
       *wpsConfigured = entry->wpsConfigured;
   }
   return (void *)entry;
}
