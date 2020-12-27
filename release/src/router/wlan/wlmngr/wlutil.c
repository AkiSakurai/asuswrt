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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sched.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mntent.h>
#include "cms_util.h"
#include "prctl.h"
#include "cms_qos.h"

#include <bcmnvram.h>

#include "wlapi.h"
#include "wlioctl.h"
#ifdef WLCONF
#include "wlutils.h"
#endif
#include "wlmngr.h"
#include "wlmdm.h"

#include "wlsyscall.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <wlcsm_lib_hspot.h>

#include "wllist.h"

#ifdef SUPPORT_WSC
#include <time.h>

#include <sys/types.h>
#include <board.h>
#include "board.h"
#include "bcm_hwdefs.h"
#endif

#include "chipinfo_ioctl.h"

#include <linux/devctl_pwrmngt.h>
//#define WL_WLMNGR_DBG

#include <assert.h>

#ifdef SUPPORT_SES
#ifndef SUPPORT_NVRAM
#error BUILD_SES depends on BUILD_NVRAM
#endif
#endif


#define WEB_BIG_BUF_SIZE_MAX_WLAN (13000 * 2)

#ifdef SUPPORT_MIMO
#define MIMO_ENABLED   ((!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_N) || !strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_AC)) && strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF))
#endif
#define MIMO_PHY ((!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_N)) || (!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_AC)))

#ifdef SUPPORT_WSC
#define WSC_RANDOM_SSID_KEY_SUPPORT    1

#define WL_DEFAULT_SSID "BrcmAP"
#define WSC_SUCCESS  0
#define WSC_FAILURE   1
#define WSC_RANDOM_SSID_TAIL_LEN	4
#define WSC_MAX_SSID_LEN  32
#ifdef WLCONF
static void wlmngr_setpriority(int wl_unit);
#endif
#define WPS_UI_PORT			40500
extern int wps_config_command;
extern int wps_action;
extern char wps_uuid[36];
extern char wps_unit[32];
extern int wps_method;
extern char wps_autho_sta_mac[sizeof("00:00:00:00:00:00")];
#endif /* SUPPORT_WSC */

extern int wl_cnt;

//**************************************************************************
// Function Name: strcat_r
// Description  : A simple string strcat tweak
// Parameters   : string1, string2, and a buffer
// Returns      : string1 and string2 strcat to the buffer
//**************************************************************************
char *strcat_r( const char *s1, const char *s2, char *buf)
{   	
   strcpy(buf, s1);
   strcat(buf, s2);
   return buf;
}



//**************************************************************************
// Function Name: wlIfcUp
// Description  : all wireless ifc up
// Parameters   : None.
// Returns      : None.
//**************************************************************************
void wlmngr_wlIfcUp(int idx)
{
   char cmd[WL_LG_SIZE_MAX];
   int i;
   for ( i=0; i<m_instance_wl[idx].numBss; i++) {
      if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {
	  	
         snprintf(cmd, sizeof(cmd), "ifconfig %s up 2>/dev/null", m_instance_wl[idx].m_ifcName[i]);
         bcmSystem(cmd);
      } else {
         snprintf(cmd, sizeof(cmd), "ifconfig %s down 2>/dev/null", m_instance_wl[idx].m_ifcName[i]);
         bcmSystem(cmd);      	
      }
   }
}

//**************************************************************************
// Function Name: wlIfcDown
// Description  : all wireless ifc down
// Parameters   : None.
// Returns      : None.
//**************************************************************************
void wlmngr_wlIfcDown(int idx)
{
   char cmd[WL_LG_SIZE_MAX];
   int i;
   for (i=0; i<m_instance_wl[idx].numBss; i++) {
         snprintf(cmd, sizeof(cmd), "ifconfig %s down 2>/dev/null", m_instance_wl[idx].m_ifcName[i]);
         bcmSystem(cmd);
   }
}


//**************************************************************************
// Function Name: setupBridge
// Description  : setup bridge for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_setupBridge(int idx)
{
   int i=0;

   char cmd[WL_LG_SIZE_MAX];

      for (i=0; i<m_instance_wl[idx].numBss; i++) {
            snprintf(cmd, sizeof(cmd), "brctl addif %s %s", m_instance_wl[idx].m_wlMssidVar[0].wlBrName, m_instance_wl[idx].m_ifcName[i]);      
            bcmSystem(cmd);
      }
      m_instance_wl[idx].onBridge = TRUE;       
}

//**************************************************************************
// Function Name: stopBridge
// Description  : remove wireless interface from bridge.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_stopBridge(int idx)
{
   int i=0;

   char cmd[WL_LG_SIZE_MAX];

      for (i=0; i < m_instance_wl[idx].numBss; i++) {
            snprintf(cmd, sizeof(cmd), "brctl delif %s %s", m_instance_wl[idx].m_wlMssidVar[i].wlBrName, m_instance_wl[idx].m_ifcName[i]);      
            bcmSystem(cmd);
      }
   m_instance_wl[idx].onBridge = FALSE;      	
}

//**************************************************************************
// Function Name: wlmngr_getPwrSaveStatus
// Description  : verify wether wireless is in low power mode.
// Parameters   : idx, *varValue.
// Returns      : *varValue.
//**************************************************************************
 void wlmngr_getPwrSaveStatus(int idx, char *varValue)
{
    UINT32 rxchain = 0;
    UINT32 radiopwr = 0;
    char buf[1024];
    FILE *fp = NULL;
    UINT32 curLine = 0;

    rxchain =  m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveEnable;
    fp = fopen("/var/wlpwrsave1", "a+b");
    if ( fp != NULL ) { 
       if (rxchain == 1)
       {	
          snprintf(buf, sizeof(buf), "wlctl -i %s rxchain_pwrsave >/var/wlpwrsave1",  m_instance_wl[idx].m_ifcName[0]);
          BCMWL_WLCTL_CMD(buf);
          curLine = ftell(fp);
          fgets(buf, sizeof(buf), fp);
          rxchain = atoi(buf);
       }

       radiopwr = m_instance_wl[idx].m_wlVar.wlRadioPwrSaveEnable;
       if (radiopwr == 1 )
       { /* erase the line */
          fseek(fp, curLine, SEEK_SET);
          snprintf(buf, sizeof(buf), "wlctl -i %s radio_pwrsave >/var/wlpwrsave1",  m_instance_wl[idx].m_ifcName[0]);
          BCMWL_WLCTL_CMD(buf);
          curLine = ftell(fp);
          fgets(buf, sizeof(buf), fp);
          radiopwr = atoi(buf);
       }
	   fclose(fp);
    } else {
#ifdef WL_WLMNGR_DBG 
         printf("/var/wlpwrsave1 open error\n"); 
#endif  	
     }

     if ( (rxchain == 1) || (radiopwr == 1) ) 
        sprintf(varValue, "1" );
     else
        sprintf(varValue, "0" ); 
     return;
}

//**************************************************************************
// Function Name: doSecurity
// Description  : setup security for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doSecurity(int idx) {
#ifdef WLCONF
    extern int wlconf_security(char *name);
    char name[32];

    snprintf(name, sizeof(name), "%s", m_instance_wl[idx].m_ifcName[0]);
    wlconf_security(name);
#else
   int i, j;
   char cmd[WL_LG_SIZE_MAX];   

#ifdef WL_WLMNGR_DBG   
   printf("%s::%s@%d Adapter idx=%d\n", __FILE__, __FUNCTION__, __LINE__, idx );
#endif

   // clear all wep keys, has to do this first 

   for(i=0; i<m_instance_wl[idx].numBss; i++) {         	
      for ( j = 0; j < 4; j++ ) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s rmwep -C %d %d",m_instance_wl[idx].m_ifcName[0],  i, j);
         BCMWL_WLCTL_CMD(cmd);
      }   
   }
   
   // do the rest of wsec   
   for(i=0; i<m_instance_wl[idx].numBss; i++) {
      if(m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid)  
         wlmngr_doSecurityOne(idx, i);
   }
#endif /* WLCONF */
}
#ifndef WLCONF
//**************************************************************************
// Function Name: doSecurityOne
// Description  : setup security for one SSID.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doSecurityOne(int idx, int ssid_idx) {
   char cmd[WL_LG_SIZE_MAX], key[WL_LG_SIZE_MAX];
   int val=0, i=0;
    
   m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlNasWillrun = FALSE;

   /* Set wsec bitvec */
   if ((strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA) == 0) ||
       (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA_PSK) == 0) ||
       (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2) == 0) ||
       (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_PSK) == 0) ||
       (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_MIX) == 0) ||
       (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_PSK_MIX) == 0)) {
      if ( strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpa, "tkip") == 0 )
         val |= TKIP_ENABLED;
      else if ( strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpa, "aes") == 0 )
         val |= AES_ENABLED;
      else if ( strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpa, "tkip+aes") == 0 )
         val |= TKIP_ENABLED | AES_ENABLED;  
                  
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlNasWillrun = TRUE;          
   } 
#ifdef BCMWAPI_WAI
   else if ((strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WAPI) == 0) ||
           (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WAPI_PSK) == 0) ||
           (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WAPI_MIX) == 0)) {
        val |= SMS4_ENABLED;
   }
#endif

   if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_RADIUS) == 0)
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlNasWillrun = TRUE;   

      
   if ( strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWep, WL_ENABLED) == 0 )
      val |= WEP_ENABLED;

   snprintf(cmd, sizeof(cmd), "wlctl -i %s  wsec -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, val);
   BCMWL_WLCTL_CMD(cmd);

   /* Set wsec restrict if WSEC_ENABLED */
   if ( ((strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_OPEN) == 0) ||
         (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_RADIUS) == 0))
      && strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWep, WL_DISABLED) == 0 )
      val = 0;
   else
      val = 1;
   snprintf(cmd, sizeof(cmd), "wlctl -i %s wsec_restrict -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, val);
   BCMWL_WLCTL_CMD(cmd);

   /* Set WPA authentication mode - radius/wpa/psk */  
   if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_RADIUS) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA_AUTH_DISABLED);
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA_AUTH_UNSPECIFIED);
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA_PSK) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA_AUTH_PSK);
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA2_AUTH_UNSPECIFIED);
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_PSK) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA2_AUTH_PSK);      
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_MIX) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA_AUTH_UNSPECIFIED+WL_WPA2_AUTH_UNSPECIFIED);
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_PSK_MIX) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA_AUTH_PSK+WL_WPA2_AUTH_PSK);            
#ifdef BCMWAPI_WAI
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WAPI) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WAPI_AUTH);
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WAPI_PSK) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WAPI_AUTH_PSK);      
   else if (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WAPI_MIX) == 0)
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, (WL_WAPI_AUTH+WL_WAPI_AUTH_PSK));
#endif
   else
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wpa_auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, WL_WPA_AUTH_DISABLED);
      
   BCMWL_WLCTL_CMD(cmd);

   if (ssid_idx == 0) {
   /* Set EAP restrict if WPA/radius is enabled */
      if ((strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_RADIUS) == 0) ||
         (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA) == 0) || (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA_PSK) == 0) ||
         (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2) == 0) || (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_PSK) == 0) ||
         (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_MIX) == 0) || (strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, WL_AUTH_WPA2_PSK_MIX) == 0))
      snprintf(cmd, sizeof(cmd), "wlctl -i %s eap 1", m_instance_wl[idx].m_ifcName[0]);
   else
      snprintf(cmd, sizeof(cmd), "wlctl -i %s  eap 0", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(cmd);
   }

   if ( strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWep, WL_ENABLED) == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 ) {
         // add non-active keys
         for ( i = 0; i < 4; i++ ) {
            //  if key is empty or active key then do nothing
            if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[i][0] == '\0' || i == (m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex64-1) )
               continue;
            BCMWL_STR_CONVERT(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[i], key);
            snprintf(cmd, sizeof(cmd), "wlctl -i %s addwep -C %d %d %s", m_instance_wl[idx].m_ifcName[0], ssid_idx, i, key);
            BCMWL_WLCTL_CMD(cmd);
         }
         // add active key
         for ( i = 0; i < 4; i++ ) {
            if ( i == (m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex64-1) && m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[i][0] != '\0' ) {
               BCMWL_STR_CONVERT(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[i], key);
               snprintf(cmd, sizeof(cmd), "wlctl -i %s addwep -C %d %d %s", m_instance_wl[idx].m_ifcName[0], ssid_idx, i, key);
               BCMWL_WLCTL_CMD(cmd);
               break;
            }
         }
      } else {
         // add non-active keys
         for ( i = 0; i < 4; i++ ) {
            //  if key is empty or active key then do nothing
            if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[i][0] == '\0' || i == (m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex128-1) )
               continue;
            BCMWL_STR_CONVERT(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[i], key);
            snprintf(cmd, sizeof(cmd), "wlctl -i %s addwep -C %d %d %s", m_instance_wl[idx].m_ifcName[0], ssid_idx, i, key);
            BCMWL_WLCTL_CMD(cmd);
         }
         // add active key
         for ( i = 0; i < 4; i++ ) {
            if ( i == (m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex128-1) && m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[i][0] != '\0' ) {
               BCMWL_STR_CONVERT(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[i], key);
               snprintf(cmd, sizeof(cmd), "wlctl -i %s addwep -C %d %d %s", m_instance_wl[idx].m_ifcName[0], ssid_idx, i, key);
               BCMWL_WLCTL_CMD(cmd);
               break;
            }
         }
      }
   }

   /* Set non-WPA authentication mode - open/shared */
   snprintf(cmd, sizeof(cmd), "wlctl -i %s auth -C %d %d", m_instance_wl[idx].m_ifcName[0], ssid_idx, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuth);
   BCMWL_WLCTL_CMD(cmd);

}


void wlmngr_macFltSetup( char *mode, char *ifcName, int idx,int bssIdx)
{
	struct wl_flt_mac_entry *pos;
	char cmd[WL_LG_SIZE_MAX];
	*cmd = '\0';
	if(!ifcName)
	{

		list_for_each(pos, (m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].m_tblFltMac) )  {
			snprintf(cmd, sizeof(cmd), "wlctl %s %s", mode, pos->macAddress);
			BCMWL_WLCTL_CMD(cmd);
		}

	}
	else 
	{

		list_for_each(pos, (m_instance_wl[idx].m_wlMssidVar[bssIdx].m_tblFltMac) )  {

			if(!strcmp(ifcName, pos->ifcName)) {
				snprintf(cmd, sizeof(cmd), "wlctl -i %s %s %s", ifcName, mode, pos->macAddress);
				BCMWL_WLCTL_CMD(cmd);
			}
		}
	}
	
}

//**************************************************************************
// Function Name: doMacFilter
// Description  : MAC filtering setup for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doMacFilter(int idx) {
   char cmd[WL_LG_SIZE_MAX];
   int i=0;

   if(!m_instance_wl[idx].mbssSupported) {
      if ( strcmp(m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlFltMacMode, WL_FLT_MAC_ALLOW) == 0 ) {
         // clear all MAC filter lists

         snprintf(cmd, sizeof(cmd), "wlctl -i %s mac none", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
         snprintf(cmd, sizeof(cmd), "wlctl -i %s macmode 2", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
         // add new MAC filter list
	     wlmngr_macFltSetup( "mac", NULL, idx ,0);	
      } else if ( strcmp(m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlFltMacMode, WL_FLT_MAC_DENY) == 0 ) {
         // clear all MAC filter lists
      
         snprintf(cmd, sizeof(cmd), "wlctl -i %s mac none", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
         snprintf(cmd, sizeof(cmd), "wlctl -i %s macmode 1", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
         // add new MAC filter list
         wlmngr_macFltSetup( "mac", NULL, idx ,0);        
      } else {
         // clear all MAC filter lists
       
         snprintf(cmd, sizeof(cmd), "wlctl -i %s mac none", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
         snprintf(cmd, sizeof(cmd), "wlctl -i %s macmode 0", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
      }
   } else {       
       for (i=0; i<m_instance_wl[idx].numBss; i++) {
       	  if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {
             if ( strcmp(m_instance_wl[idx].m_wlMssidVar[i].wlFltMacMode, WL_FLT_MAC_ALLOW) == 0 ) {
                // clear all MAC filter lists
                snprintf(cmd, sizeof(cmd), "wlctl -i %s mac none", m_instance_wl[idx].m_ifcName[i]);
                BCMWL_WLCTL_CMD(cmd);
                snprintf(cmd, sizeof(cmd), "wlctl -i %s macmode 2", m_instance_wl[idx].m_ifcName[i]);
                BCMWL_WLCTL_CMD(cmd);            
                // add new MAC filter list
                wlmngr_macFltSetup( "mac", m_instance_wl[idx].m_ifcName[i], idx,i );             
             } else if ( strcmp(m_instance_wl[idx].m_wlMssidVar[i].wlFltMacMode, WL_FLT_MAC_DENY) == 0 ) {
                // clear all MAC filter lists
                snprintf(cmd, sizeof(cmd), "wlctl -i %s mac none", m_instance_wl[idx].m_ifcName[i]);
                BCMWL_WLCTL_CMD(cmd);
                snprintf(cmd, sizeof(cmd), "wlctl -i %s macmode 1", m_instance_wl[idx].m_ifcName[i]);
                BCMWL_WLCTL_CMD(cmd);        
                // add new MAC filter list
                wlmngr_macFltSetup( "mac", m_instance_wl[idx].m_ifcName[i], idx ,i);            
             } else {
                // clear all MAC filter lists
                snprintf(cmd, sizeof(cmd), "wlctl -i %s mac none", m_instance_wl[idx].m_ifcName[i]);
                BCMWL_WLCTL_CMD(cmd);
                snprintf(cmd, sizeof(cmd), "wlctl -i %s macmode 0", m_instance_wl[idx].m_ifcName[i]);
                BCMWL_WLCTL_CMD(cmd);
             }
          }
       }
   }
}
#endif /* NU-WLCONF */
//**************************************************************************
// Function Name: setWdsMacList
// Description  : set WDS list for wireless interface.
// Parameters   : buf , size
// Returns      : None.
//**************************************************************************
void wlmngr_setWdsMacList(int idx, char *buf, int size)
{
   int i = 0, j = 0;
   char mac[32], *next=NULL;

   if(strlen(buf)==0) {
      for(i=0; i< WL_WDS_NUM;i++)
   	memset(m_instance_wl[idx].m_wlVar.wlWds[i], '\0', sizeof(m_instance_wl[idx].m_wlVar.wlWds[i]));  	
   }
   
   i=0;
   foreach(mac, buf, next) {
      if (i++ > WL_WDS_NUM)
         printf("setWdsMacList: mac list (%s) exceeds limit(%d)\n", buf, WL_WDS_NUM);
      else {         
         for (j = 0; j < WL_WDS_NUM; j++) {
            if(!strncmp(m_instance_wl[idx].m_wlVar.wlWds[j], mac, 17))
               break;                 
         }

         if( j >= WL_WDS_NUM)  { //new entry
            //find an empty slot
            for (j = 0; j < WL_WDS_NUM; j++) {
               if(strlen(m_instance_wl[idx].m_wlVar.wlWds[j]))
                  continue;
               else {
                  strncpy(m_instance_wl[idx].m_wlVar.wlWds[j],mac, sizeof(m_instance_wl[idx].m_wlVar.wlWds[j]));
                  break;
               }                
            }        
         }        
      }        
   }	
   
#ifdef SUPPORT_SES
   if( strlen(buf) && (m_instance_wl[idx].m_wlVar.wlSesEnable||m_instance_wl[idx].m_wlVar.wlSesClEnable) )
      m_instance_wl[idx].m_wlVar.wlLazyWds = WL_BRIDGE_RESTRICT_ENABLE;
#endif   

}


//**************************************************************************
// Function Name: getWdsMacList
// Description  : get WDS list for wireless interface.
// Parameters   : buf , size
// Returns      : None.
//**************************************************************************
void wlmngr_getWdsMacList(int idx, char *maclist, int size)
{
   WL_FLT_MAC_ENTRY *entry = NULL;
   WL_FLT_MAC_ENTRY *node = NULL;
   int offset =0;
   int i =0 ;


   // set wireless interface down

   /* Set the MAC list */
   memset(maclist, 0, size);


   switch (m_instance_wl[idx].m_wlVar.wlLazyWds) {
      case WL_BRIDGE_RESTRICT_ENABLE:
        for ( i = 0; i < WL_WDS_NUM; i++ ) {
            if ( m_instance_wl[idx].m_wlVar.wlWds[i][0] == '\0' )
               continue;
			
            if (offset + strlen(m_instance_wl[idx].m_wlVar.wlWds[i]) < size-2 ) {
                strcpy( (maclist+offset), m_instance_wl[idx].m_wlVar.wlWds[i]);
                offset += strlen(m_instance_wl[idx].m_wlVar.wlWds[i]); 
            /* add a space character between mac addr */
               *(maclist+offset) = ' ';
               offset++;
            }
		   
         }

         break;
      case WL_BRIDGE_RESTRICT_ENABLE_SCAN:

         node = NULL;
        list_get_next(node,  m_instance_wl[idx].m_tblWdsMac, entry);
        while ( entry != NULL ) {
			
           if (offset + strlen(entry->macAddress) < size-2 ) {
                strcpy( (maclist+offset), entry->macAddress);
	         offset += strlen(entry->macAddress); 
                *(maclist+offset) = ' ';
                offset++;
           }
	    node = entry;
           list_get_next(node,  m_instance_wl[idx].m_tblWdsMac, entry);
         }
         break;
      case WL_BRIDGE_RESTRICT_DISABLE:          
      default:
         break;
   }	

}
#ifdef WLCONF
void wlmngr_getFltMacList(int idx,int bssidx, char *maclist, int size)
{
    WL_FLT_MAC_ENTRY *entry = NULL;
    WL_FLT_MAC_ENTRY *node = NULL;
    int offset =0;
    WIRELESS_MSSID_VAR *m_wlMssidVarPtr;

    /* Set the MAC list */
    memset(maclist, 0, size);

    node = NULL;
    m_wlMssidVarPtr =  &(m_instance_wl[idx].m_wlMssidVar[bssidx]);
    list_get_next(node,  m_wlMssidVarPtr->m_tblFltMac, entry);
    while ( entry != NULL ) {

        if (offset + strlen(entry->macAddress) < size-2 ) {
            strcpy( (maclist+offset), entry->macAddress);
            offset += strlen(entry->macAddress); 
            *(maclist+offset) = ' ';
            offset++;
        }
        node = entry;
        list_get_next(node,  m_wlMssidVarPtr->m_tblFltMac, entry);
    }

}
#else
//**************************************************************************
// Function Name: doWds
// Description  : WDS setup for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doWds(int idx) {
   char cmd[WL_LG_SIZE_MAX];
   char maclist[WL_LG_SIZE_MAX]={0};
   int enable;

   if ( m_instance_wl[idx].m_wlVar.wlEnbl != TRUE )
      return;

  /* get the MAC list */
   memset(maclist, 0, sizeof(maclist));
   wlmngr_getWdsMacList(idx, maclist,WL_LG_SIZE_MAX);

   switch (m_instance_wl[idx].m_wlVar.wlLazyWds) {
      case WL_BRIDGE_RESTRICT_ENABLE:
         enable = 0;
         break;
      case WL_BRIDGE_RESTRICT_ENABLE_SCAN:
         enable = 0;
         break;
      case WL_BRIDGE_RESTRICT_DISABLE:
      default:
         enable = 1;
         break;
   }

   snprintf(cmd, sizeof(cmd), "wlctl -i %s lazywds %d", m_instance_wl[idx].m_ifcName[0], enable);
   BCMWL_WLCTL_CMD(cmd);

   snprintf(cmd, sizeof(cmd), "wlctl -i %s wds %s", m_instance_wl[idx].m_ifcName[0], maclist);
   BCMWL_WLCTL_CMD(cmd);
	  
   snprintf(cmd, sizeof(cmd), "wlctl -i %s wdstimeout 1", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(cmd);
}
#endif /* WLCONF */

//**************************************************************************
// Function Name: doWdsSec
// Description  : WDS security (WEP) setup for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doWdsSec(int idx) {
   char cmd[WL_LG_SIZE_MAX], key[WL_LG_SIZE_MAX];
   int i=0;

   if ( m_instance_wl[idx].m_wlVar.wlEnbl != TRUE )
      return;

   // reset wds_wsec and wdswsec_enable
   snprintf(cmd, sizeof(cmd), "wlctl -i %s wdswsec 0", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(cmd);           
   snprintf(cmd, sizeof(cmd), "wlctl -i %s wdswsec_enable 0", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(cmd);     

   if(m_instance_wl[idx].m_wlVar.wlWdsSecEnable) {
   
      for ( i = 0; i < WL_WDS_NUM; i++ ) {
         if ( m_instance_wl[idx].m_wlVar.wlWds[i][0] == '\0' )
            continue;   	
         
         if(m_instance_wl[idx].m_wlVar.wlWdsKey[0] != '\0') {
            //wep key not empty, enable wep
            BCMWL_STR_CONVERT(m_instance_wl[idx].m_wlVar.wlWdsKey, key);  
            //goes to index 0/key 1
      	    snprintf(cmd, sizeof(cmd), "wlctl -i %s addwep %d %s %s", m_instance_wl[idx].m_ifcName[0], 0, key, m_instance_wl[idx].m_wlVar.wlWds[i]);
            BCMWL_WLCTL_CMD(cmd);
            snprintf(cmd, sizeof(cmd), "wlctl -i %s wdswsec 1", m_instance_wl[idx].m_ifcName[0]);
            BCMWL_WLCTL_CMD(cmd);
         }
      	 snprintf(cmd, sizeof(cmd), "wlctl -i %s wdswsec_enable 1", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
      }
   } else {
      for ( i = 0; i < WL_WDS_NUM; i++ ) {   	
         if ( m_instance_wl[idx].m_wlVar.wlWds[i][0] == '\0' )
            continue;
         //remove key index 0 for the mac
      	 snprintf(cmd, sizeof(cmd), "wlctl -i %s rmwep %d %s", m_instance_wl[idx].m_ifcName[0], 0, m_instance_wl[idx].m_wlVar.wlWds[i]);
         BCMWL_WLCTL_CMD(cmd);    
      }   	
   }
}

void wlmngr_scanWdsStart(int idx) {

   //char cmd[WL_LG_SIZE_MAX];

   //snprintf(cmd, sizeof(cmd), "wlctl ssid ''");
   //bcmSystem(cmd);

   //snprintf(cmd, sizeof(cmd), "wlctl down");
   //bcmSystem(cmd);

   //snprintf(cmd, sizeof(cmd), "wlctl up");
   //bcmSystem(cmd);
}

#ifdef WLCONF
#define SCAN_WDS 0
#define SCAN_WPS 1
#define NUMCHANS 64
#define SCAN_DUMP_BUF_LEN (32 * 1024)
#define MAX_AP_SCAN_LIST_LEN 50
#define SCAN_RETRY_TIMES 5

static char scan_result[SCAN_DUMP_BUF_LEN];
static char *
wlmngr_getScanResults(char *ifname)
{
    int ret, retry_times = 0;
    wl_scan_params_t *params;
    wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
    int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
    int org_scan_time = 20, scan_time = 40;
#ifdef DSLCPE_ENDIAN
    int org_scan_time_tmp = 20;
#endif

#ifdef DSLCPE_ENDIAN
    wl_endian_probe(ifname);
#endif

    params = (wl_scan_params_t*)malloc(params_size);
    if (params == NULL) {
        return NULL;
    }

    memset(params, 0, params_size);
    params->bss_type = DOT11_BSSTYPE_ANY;
    memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
    params->scan_type = -1;
		
#ifdef DSLCPE_ENDIAN
    params->nprobes = htoe32(-1);
    params->active_time = htoe32(-1);
    params->passive_time = htoe32(-1);
    params->home_time = htoe32(-1);
    params->channel_num = htoe32(0);
#else
    params->nprobes = -1;
    params->active_time = -1;
    params->passive_time = -1;
    params->home_time = -1;
    params->channel_num = 0;
#endif

    /* extend scan channel time to get more AP probe resp */
#ifdef DSLCPE_ENDIAN
    wl_ioctl(ifname, WLC_GET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));
    org_scan_time = etoh32(org_scan_time);
#endif

    if (org_scan_time < scan_time) {
#ifdef DSLCPE_ENDIAN
        scan_time = htoe32(scan_time);
#endif
        wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &scan_time, sizeof(scan_time));
    }

retry:
    ret = wl_ioctl(ifname, WLC_SCAN, params, params_size);
    if (ret < 0) {
        if (retry_times++ < SCAN_RETRY_TIMES) {
            printf("set scan command failed, retry %d\n", retry_times);
            sleep(1);
            goto retry;
        }
    }

    sleep(2);

#ifdef DSLCPE_ENDIAN
    list->buflen = htoe32(SCAN_DUMP_BUF_LEN);
#else
    list->buflen = SCAN_DUMP_BUF_LEN;
#endif
    ret = wl_ioctl(ifname, WLC_SCAN_RESULTS, scan_result, SCAN_DUMP_BUF_LEN);
    if (ret < 0 && retry_times++ < SCAN_RETRY_TIMES) {
        printf("get scan result failed, retry %d\n", retry_times);
        sleep(1);
        goto retry;
    }

    free(params);

    /* restore original scan channel time */
#ifdef DSLCPE_ENDIAN
    org_scan_time = htoe32(org_scan_time_tmp);
#endif
    wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));

    if (ret < 0)
        return NULL;

    return scan_result;
}

static uint8*
wps_enr_parse_wsc_tlvs(uint8 *tlv_buf, int buflen, uint16 type)
{
    uint8 *cp;
    uint16 *tag;
    int totlen;
    uint16 len;
    uint8 buf[4];

    cp = tlv_buf;
    totlen = buflen;

    /* TLV: 2 bytes for T, 2 bytes for L */
    while (totlen >= 4) {
        memcpy(buf, cp, 4);
        tag = (uint16 *)buf;

        if (ntohs(tag[0]) == type)
            return (cp + 4);
        len = ntohs(tag[1]);
        cp += (len + 4);
        totlen -= (len + 4);
    }

    return NULL;
}

static uint8 *
wps_enr_parse_ie_tlvs(uint8 *tlv_buf, int buflen, uint key)
{
    uint8 *cp;
    int totlen;

    cp = tlv_buf;
    totlen = buflen;

    /* find tagged parameter */
    while (totlen >= 2) {
        uint tag;
        int len;

        tag = *cp;
        len = *(cp +1);

        /* validate remaining totlen */
        if ((tag == key) && (totlen >= (len + 2)))
            return (cp);

        cp += (len + 2);
        totlen -= (len + 2);
    }

    return NULL;
}

static bool
wps_enr_wl_is_wps_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len, char *configured)
{
    uint8 *ie = *wpaie;
    uint8 *data;

    /* If the contents match the WPA_OUI and type=1 */
    if ((ie[1] >= 6) && !memcmp(&ie[2], WPA_OUI "\x04", 4)) {
        ie += 6;
        data = wps_enr_parse_wsc_tlvs(ie, *tlvs_len-6, 0x1044);
        if (data && *data == 0x01)
            *configured = FALSE;
        else
            *configured = TRUE;
        return TRUE;
    }

    /* point to the next ie */
    ie += ie[1] + 2;
    /* calculate the length of the rest of the buffer */
    *tlvs_len -= (int)(ie - *tlvs);
    /* update the pointer to the start of the buffer */
    *tlvs = ie;

    return FALSE;
}

static bool
wps_enr_is_wps_ies(uint8* cp, uint len, char *configured)
{
    uint8 *parse = cp;
    uint parse_len = len;
    uint8 *wpaie;

    while ((wpaie = wps_enr_parse_ie_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
        if (wps_enr_wl_is_wps_ie(&wpaie, &parse, &parse_len, configured))
            break;
    if (wpaie)
        return TRUE;
    else
        return FALSE;
}

extern char *bcm_ether_ntoa(const struct ether_addr *ea, char *buf);

void wlmngr_scanResult(int idx, int chose) {
    char ssid[WL_SSID_SIZE_MAX];
    //int channel = 0;
    WL_FLT_MAC_ENTRY *entry;
    char name[32];
    wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
    wl_bss_info_t *bi;
    wl_bss_info_107_t *old_bi;
    uint i, ap_count = 0;
    char configured = 0;
    chanspec_t current_chanspec;

    snprintf(name, sizeof(name), "wl%d", idx );

    if (wlmngr_getScanResults(name) == NULL)
        return;

#ifdef DSLCPE_ENDIAN
    list->buflen = etoh32(list->buflen);
    list->version = etoh32(list->version);
    list->count = etoh32(list->count);
#endif

    if (list->count == 0)
        return;

   /* 
    * Update variable m_instance_wl[idx].wlCurrentChannel again. 
    * Because the variable was changed to default 1 by wlReadBaseCfg()
    */
    wlmngr_getCurrentChannel(idx);
    current_chanspec=wlmngr_getCurrentChSpec(idx);

    bi = list->bss_info;
    for (i = 0; i < list->count; i++) {
        /* Convert version 107 to 108 */
#ifdef DSLCPE_ENDIAN
        bi->version = etoh32(bi->version);
        bi->length = etoh32(bi->length);
        bi->chanspec = etoh16(bi->chanspec);
        bi->beacon_period = etoh16(bi->beacon_period);
        bi->capability = etoh16(bi->capability);
        bi->atim_window = etoh16(bi->atim_window);
        bi->RSSI = etoh16(bi->RSSI);
        bi->nbss_cap = etoh32(bi->nbss_cap);
        bi->vht_rxmcsmap = etoh16(bi->vht_rxmcsmap);
        bi->vht_txmcsmap = etoh16(bi->vht_txmcsmap);
        bi->ie_length = etoh32(bi->ie_length);
        bi->ie_offset = etoh16(bi->ie_offset);
        bi->SNR = etoh16(bi->SNR);
#endif
        if (bi->version == LEGACY_WL_BSS_INFO_VERSION) {
            old_bi = (wl_bss_info_107_t *)bi;
            bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
            bi->ie_length = old_bi->ie_length;
            bi->ie_offset = sizeof(wl_bss_info_107_t);
        }    
        if (bi->ie_length) {
            if(ap_count < MAX_AP_SCAN_LIST_LEN) {
                if (((chose == SCAN_WDS) && (bi->chanspec==current_chanspec)) ||
                    ((chose == SCAN_WPS) && (TRUE == wps_enr_is_wps_ies((uint8 *)(((uint8 *)bi)+bi->ie_offset), bi->ie_length, &configured)))) {

                    entry = malloc( sizeof(WL_FLT_MAC_ENTRY));
                    if ( entry != NULL ) {
                        memset(entry, 0, sizeof(WL_FLT_MAC_ENTRY) ); 
                        strncpy(ssid, (const char *)bi->SSID, sizeof(ssid));
                        if (strncmp(ssid, "\\x00", 4) == 0) {
                            strncpy (entry->ssid, "", sizeof(entry->ssid)); // 0 means empty ssid
                        } 
                        else {
                            strncpy(entry->ssid, ssid, sizeof(entry->ssid));
                        }
                        bcm_ether_ntoa(&bi->BSSID, entry->macAddress);
                        strncpy(entry->ifcName, m_instance_wl[idx].m_ifcName[MAIN_BSS_IDX], sizeof(entry->ifcName));
                        if (chose == SCAN_WPS) {
                            entry->privacy = bi->capability & DOT11_CAP_PRIVACY;
                            entry->wpsConfigured = configured;   
                            entry->wps = TRUE;
                        }
                        list_add( entry, m_instance_wl[idx].m_tblScanWdsMac, struct wl_flt_mac_entry);
                    }
                    else {
                        printf("%s@%d Could not allocat memmro to add WDS Entry\n", __FUNCTION__, __LINE__);
                    }
                }
                ap_count++;
            }
        }
        bi = (wl_bss_info_t*)((int8*)bi + bi->length);
    }   
}
//**************************************************************************
// Function Name: scanWdsResult
// Description  : scan WDS AP and return to AP mode.
// Returns      : none.
//**************************************************************************
void wlmngr_scanWdsResult(int idx) {
    wlmngr_scanResult(idx, SCAN_WDS);
}
#else /* #ifdef WLCONF */

#define ID_SSID   "SSID: \""
#define ID_CHANNEL "Channel: "
#define ID_BSSID  "BSSID: "
#define ID_WPS  "WPS: "
#define SUBID_STATUS  "Status: "
#define SUBID_PRIVACY  "Privacy: "

//**************************************************************************
// Function Name: scanWdsResult
// Description  : scan WDS AP and return to AP mode.
// Returns      : none.
//**************************************************************************
void wlmngr_scanWdsResult(int idx) {
   char cmd[WL_LG_SIZE_MAX] = {0};
   char wlwdsfile[WL_LG_SIZE_MAX];
   FILE *fp = NULL;
   char *cp;
   char *ep;
   char ech = 0;
   char ssid[WL_SSID_SIZE_MAX];
   char bssid[WL_MID_SIZE_MAX];
   int channel = 0;
   int tagidx;
   char tagstr[16]={0}; 
   int numwds = 0;
   WL_FLT_MAC_ENTRY *entry;

   // fake wds scan result before scanresult issue result
   
   snprintf(wlwdsfile, sizeof(wlwdsfile), "/var/wl%dwds", idx );

   fp = fopen(wlwdsfile, "w+");
   if ( fp != NULL ) {
      // active scan across all channels
      snprintf(cmd, sizeof(cmd), "wlctl -i %s scan", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      // wait for wlctl scan command to completed
      sleep(2);


      // get scan result
      snprintf(cmd, sizeof(cmd), "wlctl -i %s scanresults > /var/wl%dwds", m_instance_wl[idx].m_ifcName[0], idx ); 
      BCMWL_WLCTL_CMD(cmd);

      fclose(fp);
   }
   
   /* 
    * Update variable m_instance_wl[idx].wlCurrentChannel again. 
    * Because the variable was changed to default 1 by wlReadBaseCfg()
   	*/
   wlmngr_getCurrentChannel(idx);

   // parse the wds list
   fp = fopen(wlwdsfile, "r");
   if ( fp != NULL ) {
      tagidx = 0;
      while ( fgets( cmd, sizeof(cmd), fp ) ) {
         switch(tagidx) {
            case 0:
               strncpy(tagstr, ID_SSID, sizeof(tagstr));
               ech = '"';
               break;
            case 1:
               strncpy(tagstr, ID_CHANNEL, sizeof(tagstr));
               ech = '\n';
               break;
            case 2:
               strncpy(tagstr, ID_BSSID, sizeof(tagstr));
               ech = '\t';
               break;
         }
         cp = strstr(cmd, tagstr);
         if (cp) {
            cp = cp + strlen(tagstr);
            ep = strchr(cp, ech);
            if (ep) {
               *ep = 0;
            }
            switch(tagidx) {
               case 0:
                  strncpy(ssid, cp, sizeof(ssid));
                  break;
               case 1:
                  channel = atoi(cp);
                  break;
               case 2:
                  strncpy(bssid, cp, sizeof(bssid));
                  break;
            }
            if (++tagidx > 2) {
               tagidx = 0;

               if (m_instance_wl[idx].wlCurrentChannel == channel) {
               entry = malloc( sizeof(WL_FLT_MAC_ENTRY));
               if ( entry != NULL ) {
                      memset(entry, 0, sizeof(WL_FLT_MAC_ENTRY) );						 
                      if (strncmp(ssid, "\\x00", 4) == 0) {
                           strncpy (entry->ssid, "", sizeof(entry->ssid)); // 0 means empty ssid
                      } 
                      else {
                           strncpy(entry->ssid, ssid, sizeof(entry->ssid));
                      }
				  
                      strncpy(entry->macAddress, bssid, sizeof(entry->macAddress));
                      strncpy(entry->ifcName, m_instance_wl[idx].m_ifcName[MAIN_BSS_IDX], sizeof(entry->ifcName));
   
                      list_add( entry, m_instance_wl[idx].m_tblScanWdsMac, struct wl_flt_mac_entry);
                }
                else {
                      printf("%s@%d Could not allocat memmro to add WDS Entry\n", __FUNCTION__, __LINE__);
                }
                  numwds++;
               }
            }
         }
      }
      fclose(fp);

   }
}

#define SCAN_WPS 1
void wlmngr_scanResult(int idx, int chose) {
   char cmd[WL_LG_SIZE_MAX] = {0};
   char wlscanfile[WL_LG_SIZE_MAX];
   FILE *fp = NULL;
   char *cp;
   char *ep;
   char ech = 0;
   char ssid[WL_SSID_SIZE_MAX];
   char bssid[WL_MID_SIZE_MAX];
   int tagidx;
   char tagstr[16]={0}; 
   int numwds = 0;
   WL_FLT_MAC_ENTRY *entry;
   int filter = 1;
   int privacy = 0;
   int wps = 0;
   int wpsConfigured = 0;
   char tmpStr[WL_SIZE_132_MAX];

   // fake scan result before scanresult issue result
   
   snprintf(wlscanfile, sizeof(wlscanfile), "/var/wl%dscan", idx );

   fp = fopen(wlscanfile, "w+");
   if ( fp != NULL ) {
      // active scan across all channels
      snprintf(cmd, sizeof(cmd), "wlctl -i %s scan", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      // wait for wlctl scan command to completed
      sleep(2);


      // get scan result
      snprintf(cmd, sizeof(cmd), "wlctl -i %s scanresults > /var/wl%dscan", m_instance_wl[idx].m_ifcName[0], idx ); 
      BCMWL_WLCTL_CMD(cmd);
      sleep(1);

      fclose(fp);
   }
   
   /* 
    * Update variable m_instance_wl[idx].wlCurrentChannel again. 
    * Because the variable was changed to default 1 by wlReadBaseCfg()
    */
   wlmngr_getCurrentChannel(idx);

   // parse the list
   fp = fopen(wlscanfile, "r");
   if ( fp != NULL ) {
      tagidx = 0;
      while ( fgets( cmd, sizeof(cmd), fp ) ) {
         switch(tagidx) {
            case 0:
               strncpy(tagstr, ID_SSID, sizeof(tagstr));
               ech = '"';
               break;
            case 1:
               strncpy(tagstr, ID_CHANNEL, sizeof(tagstr));
               ech = '\n';
               break;
            case 2:
               strncpy(tagstr, ID_BSSID, sizeof(tagstr));
               ech = '\t';
               break;
            case 3:
               strncpy(tagstr, ID_WPS, sizeof(tagstr));
               ech = '\n';
               break;
         }
PARSESUB:
         cp = strstr(cmd, tagstr);
         if (cp) {
            cp = cp + strlen(tagstr);
            ep = strchr(cp, ech);
            if (ep) {
               *ep = 0;
            }
            switch(tagidx) {
               case 0:
                  strncpy(ssid, cp, sizeof(ssid));
                  break;
               case 1:
                  break;
               case 2:
                  strncpy(bssid, cp, sizeof(bssid));
                  break;
               case 3:
                  strncpy(tmpStr, cp, sizeof(tmpStr));
                  if (!strncmp(tmpStr, "Unsupported", 11)) {
                      wps = 0;
                      break;
                  }

                  if (!strncmp(tmpStr, "Supported", 9))	{
                      wps = 1;
                      strncpy(tagstr, SUBID_STATUS, sizeof(tagstr));
                      ech = '\n';
                      goto PARSESUB;
                  }

                  if (!strncmp(tmpStr, "Configured", 10) || !strncmp(tmpStr, "Unconfigured", 12))	{
                      if (!strncmp(tmpStr, "Configured", 10))
                          wpsConfigured = 1;
                      else
                          wpsConfigured = 0;

                      strncpy(tagstr, SUBID_PRIVACY, sizeof(tagstr));
                      ech = '\n';
                      goto PARSESUB;
                  }

                  privacy = 0;
                  if (!strcmp(tmpStr, "16"))
                      privacy = 16;
                  
                  break;
            }
            if (++tagidx > 3) {
               tagidx = 0;

               if (chose == SCAN_WPS)
                   filter = wps;
               
               //printf("==Scan ssid %s wps =%d channel=%d wps_configured %d privacy %d\n", ssid, wps, channel, wpsConfigured, privacy);
               if (filter /*&& (m_instance_wl[idx].wlCurrentChannel == channel)*/) {
                   entry = malloc( sizeof(WL_FLT_MAC_ENTRY));
                   if ( entry != NULL ) {
                      memset(entry, 0, sizeof(WL_FLT_MAC_ENTRY) );						 
                      if (strncmp(ssid, "\\x00", 4) == 0) {
                           strncpy (entry->ssid, "", sizeof(entry->ssid)); // 0 means empty ssid
                      } 
                      else {
                           strncpy(entry->ssid, ssid, sizeof(entry->ssid));
                      }
                      //cmsLog_error("========add numwds %d ssid %s ", numwds, ssid);				  
                      strncpy(entry->macAddress, bssid, sizeof(entry->macAddress));
                      strncpy(entry->ifcName, m_instance_wl[idx].m_ifcName[MAIN_BSS_IDX], sizeof(entry->ifcName));
                      entry->privacy = privacy;
                      entry->wps = wps;
                      entry->wpsConfigured = wpsConfigured;   

                      list_add( entry, m_instance_wl[idx].m_tblScanWdsMac, struct wl_flt_mac_entry);
                   }
                   else {
                      printf("%s@%d Could not allocat memmro to add scan Entry\n", __FUNCTION__, __LINE__);
                   }
                   numwds++;
               }
               /* Clean subid's variable */
               wps = 0;
               privacy = 0;
               wpsConfigured = 0;
            }
         }
      }
      fclose(fp);

   }
}
#endif /* WLCONF */
//**************************************************************************
// Function Name: enumInterfaces
// Description  : a simple enumeration of wireless intefaces, caller must call 
//                until null devicename is returned, assuming no multitasking
//                callers
// Parameters   : caller allocate devname space.
// Returns      : None.
//**************************************************************************
WL_STATUS wlmngr_enumInterfaces(int idx, char *devname) {

   static int i = 0;
   WL_STATUS status = WL_STS_OK;
   
   *devname = 0;
   if(i < WL_NUM_SSID)
      sprintf(devname, m_instance_wl[idx].m_ifcName[i++]);
   else {
      i = 0;
      *devname = '\0';
      status = WL_STS_ERR_OBJECT_NOT_FOUND;
      
   }
   return status;
}

/***************************************************************************
// Function Name: getWlInfo.
// Description  : create WlMngr object
// Parameters   : none.
// Returns      : n/a.
****************************************************************************/
void wlmngr_getWlInfo(int idx, char *buf, char *id) {
#ifdef WLCONF
#define CAP_STR_LEN 512 
   //The value of CAP_STR_LEN must be bigger than the content of wlcap and WLC_IOCTL_SMLEN (used in wl.c::wl_iovar_get). 
   //Otherwise, it causes buf issue. 
   char cmd[WL_LG_SIZE_MAX];

   snprintf(cmd, sizeof(cmd), "wl%d", idx);
   wl_iovar_get(cmd, "cap", (void *)buf, CAP_STR_LEN);

   if (!strcmp("wlcap",id)) {      
      if(buf[0])
         buf[strlen(buf)-1]='\0'; // the last char is a '\n', remove it   
   }              

   if(strlen(buf)> CAP_STR_LEN)
     printf("wlutil.c:%s %d error: buf size too small\n",__FUNCTION__,__LINE__);
#undef CAP_STR_LEN
#else /* not WLCONF */
   FILE *fp; 
   char wlcapFile[WL_LG_SIZE_MAX];
   snprintf(wlcapFile, sizeof(wlcapFile), "/var/wl%dcap", idx);
   
   if (!strcmp("wlcap",id)) {      
      fp = fopen(wlcapFile, "r");
      if(fp) {
         fgets(buf, WEB_BIG_BUF_SIZE_MAX_WLAN, fp); // buffer size from ej.c
         if(buf[0])
            buf[strlen(buf)-1]='\0'; // the last char is a '\n', remove it
         fclose(fp);   
      }              
   }
#endif /* WLCONF */
}

//**************************************************************************
// Function Name: getValidBand
// Description  : get valid band matching hardware
// Parameters   : band -- band
// Returns      : valid band
//**************************************************************************
int wlmngr_getValidBand(int idx, int band)
{
   int vbands = wlmngr_getBands( idx);
   if ((vbands & BAND_A) && (vbands & BAND_B)) {
      if (!(band == BAND_A || band == BAND_B)) {
         band = BAND_B; // 2.4G
      }
   }
   else if (vbands & BAND_B) {
      if (!(band == BAND_B)) {
         band = BAND_B; // 2.4G
      }
   }
   else if (vbands & BAND_A) {
      if (!(band == BAND_A)) {
         band = BAND_A; // 5G
      }
   }
   return band;
}

//**************************************************************************
// Function Name: getValidChannel
// Description  : get valid channel number which is in country range.
// Parameters   : channel -- current channel number.
// Returns      : valid channel number.
//**************************************************************************
int wlmngr_getValidChannel(int idx, int channel) {

   int vc, found;
   char cmd[WL_LG_SIZE_MAX];


   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d channels > /var/wl%dchlist", idx, idx ); 
   BCMWL_WLCTL_CMD(cmd);
   snprintf(cmd, sizeof(cmd), "/var/wl%dchlist", idx ); 

   FILE *fp = fopen(cmd, "r");
   if ( fp != NULL ) {

      for (found=0;;) {
         if (fscanf(fp, "%d", &vc) != 1) {
            break;
         }
         if (vc == channel) {
            found = 1;
            break;
         }
      }

      if (!found) {
         channel = 0; // Auto
      }
      fclose(fp);
      unlink(cmd);
   }
   return channel;
}

//**************************************************************************
// Function Name: getValidRate
// Description  : get valid rate which is in b range or g range.
// Parameters   : rate -- current rate in the flash.
// Returns      : valid rate.
//**************************************************************************
long wlmngr_getValidRate(int idx, long rate) {
   int i = 0, bMax = 4, gMax = 12, aMax = 8;
   long gRates[] = { 1000000,  2000000,  5500000,  6000000,
                   9000000,  11000000, 12000000, 18000000,
                   24000000, 36000000, 48000000, 54000000 };
   long bRates[] = { 1000000, 2000000, 5500000, 11000000 };
   long aRates[] = { 6000000, 9000000, 12000000, 18000000, 24000000, 36000000, 48000000, 54000000};

   long ret = 0;

#ifdef SUPPORT_MIMO
      if ( strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_G) == 0 || MIMO_PHY) {
#else
      if ( strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_G) == 0 ) {
#endif      	
         for ( i = 0; i < gMax; i++ ) {
            if ( rate == gRates[i] )
               break;
         }
         if ( i < gMax )
            ret = rate;
      } else if ( strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_B) == 0 ) {
         for ( i = 0; i < bMax; i++ ) {
            if ( rate == bRates[i] )
               break;
         }
         if ( i < bMax )
            ret = rate;
      }
      else if ( strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_A) == 0 ) {
         for ( i = 0; i < aMax; i++ ) {
            if ( rate == aRates[i] )
               break;
         }
         if ( i < aMax )
            ret = rate;
     }

   return ret;
}

//**************************************************************************
// Function Name: getCoreRev
// Description  : get current core revision.
// Parameters   : none.
// Returns      : revsion.
// Note		: wlctl revinfo output may change at each wlan release
//                if only tkip is supported in WebUI, this function
//                need to be reviewed
//**************************************************************************
int wlmngr_getCoreRev(int idx) {
   int  found=0;
   unsigned int corerev;
   char cmd[80];
   FILE *fp = NULL;
   char match_str[]="corerev";

   // get core revision

  snprintf(cmd, sizeof(cmd), "wlctl -i wl%d revinfo > /var/wl%d", idx, idx);
  BCMWL_WLCTL_CMD(cmd);
  snprintf(cmd, sizeof(cmd), "/var/wl%d",  idx);

 
   fp = fopen(cmd, "r");
   if (fp != NULL) {
      while ( !feof(fp) ){
         fscanf(fp, "%s 0x%x\n", cmd, &corerev);  
         if(!strcmp(match_str,cmd)){
            found=1;
            break;
         }  	
      }
      fclose(fp);
   }

   if(!found){
      corerev=0;
   }

   return corerev;
}

//**************************************************************************
// Function Name: getPhytype
// Description  : get wireless physical type.
// Parameters   : none.
// Returns      : physical type (b, g or a).
// Note		: wlctl wlctl phytype output may change at each wlan release 
//**************************************************************************
char *wlmngr_getPhyType(int idx ) {
   char  *phytype = WL_PHY_TYPE_B;
   char cmd[WL_LG_SIZE_MAX];
   FILE *fp = NULL;

   // get phytype
 
  snprintf(cmd, sizeof(cmd), "wlctl -i wl%d phytype > /var/wl%d", idx,idx);
  BCMWL_WLCTL_CMD(cmd);
  snprintf(cmd, sizeof(cmd), "/var/wl%d",  idx);

   // parse the phytype
   fp = fopen(cmd, "r");
   if ( fp != NULL ) {
      if (fgets(cmd, WL_LG_SIZE_MAX-1, fp)) {                       	
            switch(atoi(cmd)) {
               case WL_PHYTYPE_A:
                  phytype = WL_PHY_TYPE_A;
                  break;
               case WL_PHYTYPE_B:
                  phytype = WL_PHY_TYPE_B;
                  break;
               case WL_PHYTYPE_G:
               case WL_PHYTYPE_LP: /*phytype = WL_PHY_TYPE_LP;*/
                  //it does not make difference for webui at this moment      
                  phytype = WL_PHY_TYPE_G;
                  break;
#ifdef SUPPORT_MIMO
               case WL_PHYTYPE_N:
               case WL_PHYTYPE_HT:
               case WL_PHYTYPE_LCN:
                  phytype = WL_PHY_TYPE_N;
                  break;
               case WL_PHYTYPE_AC:
                  phytype = WL_PHY_TYPE_AC;
                  break;
#endif
            }
      }
      fclose(fp);
   }

   return phytype;
}

//**************************************************************************
// Function Name: setSsid
// Description  : execute command to set ssid.
// Parameters   : none.
// Returns      : none.
//**************************************************************************
void wlmngr_setSsid(int idx) {
   char cmd[WL_LG_SIZE_MAX], ssid[WL_LG_SIZE_MAX];
   int i = 0;



   for (i =0; i<m_instance_wl[idx].numBss; i++) {
      if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {
         BCMWL_STR_CONVERT(m_instance_wl[idx].m_wlMssidVar[i].wlSsid, ssid);
         snprintf(cmd, sizeof(cmd), "wlctl -i %s ssid -C %d %s", m_instance_wl[idx].m_ifcName[0], i, ssid);
         BCMWL_WLCTL_CMD(cmd);
      }
   }
}

//**************************************************************************
// Function Name: getWdsWsec
// Description  : get WPA parameters over WDS for nas.
// Parameters   : wl_wds<N>=mac,role,crypto,auth,ssid,passphrase.
// Returns      : boolean.
//**************************************************************************
bool wlmngr_getWdsWsec(int idx, int unit, int which, char *mac, char *role, char *crypto, char *auth, ...)
{

#ifdef SUPPORT_SES
   char value[1000], *next;

   if(m_instance_wl[idx].m_wlVar.wlWdsWsec[0] == NULL)
      return FALSE;
      
   strncpy(value, m_instance_wl[idx].m_wlVar.wlWdsWsec, sizeof(value));
   next = value;

   //separate mac
   strcpy(mac, strsep(&next, ","));
   if (!next)
      return FALSE;

   //separate role
   strcpy(role, strsep(&next, ","));
   if (!next)
      return FALSE;

   //separate crypto
   strcpy(crypto, strsep(&next, ","));
   if (!next)
      return FALSE;

   //separate auth 
   strcpy(auth, strsep(&next, ","));
   if (!next)
      return FALSE;

   if (!strcmp(auth, "psk")) {
      va_list va;
      va_start(va, auth);
      /* separate ssid */
      strcpy(va_arg(va, char *), strsep(&next, ","));
      if (!next)
         goto fail;

      //separate passphrase
      strcpy(va_arg(va, char *), next);
      va_end(va);
      return TRUE;
fail:
      va_end(va);
      return FALSE;
   }       

#endif

   return FALSE;
}

//**************************************************************************
// Function Name: getCurrentChannel
// Description  : get current channel .
// Returns      : none.
//**************************************************************************

void wlmngr_getCurrentChannel( int idx ) {
#ifndef WLCONF
#define ID_CURCHANNEL "target channel"
   char cmd[WL_LG_SIZE_MAX];
   FILE *fp = NULL;
   char *cp;

   if (!MIMO_PHY) {        
      snprintf(cmd, sizeof(cmd), "wlctl -i wl%d channel > /var/curchannel%d", idx, idx);
      BCMWL_WLCTL_CMD(cmd);    
      snprintf(cmd, sizeof(cmd), "/var/curchannel%d",  idx);
	  
      fp = fopen(cmd, "r");
      if ( fp != NULL ) {  
         while ( fgets( cmd, sizeof(cmd), fp ) ) {   	
            cp = strstr(cmd, ID_CURCHANNEL);
            if (cp) {
               cp = cp + strlen(ID_CURCHANNEL);
               m_instance_wl[idx].wlCurrentChannel = atoi(cp);
//               printf("%s: current channel %d\n", m_instance_wl[idx].m_ifcName[MAIN_BSS_IDX], m_instance_wl[idx].wlCurrentChannel);
            }                 
         }
         fclose(fp);
         unlink(cmd);      
      }
   } else {
#endif
      int sb;
      chanspec_t chanspec;
      chanspec = wlmngr_getCurrentChSpec(idx);
#ifdef WL11AC
      if ((chanspec & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_80) {
         sb = chanspec & WL_CHANSPEC_CTL_SB_MASK;
         switch (sb) {
             case WL_CHANSPEC_CTL_SB_LL:
                 m_instance_wl[idx].wlCurrentChannel = (chanspec & WL_CHANSPEC_CHAN_MASK) - 6;
                 break;
             case WL_CHANSPEC_CTL_SB_LU:
                 m_instance_wl[idx].wlCurrentChannel = (chanspec & WL_CHANSPEC_CHAN_MASK) - 2;
                 break;
             case WL_CHANSPEC_CTL_SB_UL:
                 m_instance_wl[idx].wlCurrentChannel = (chanspec & WL_CHANSPEC_CHAN_MASK) + 2;
                 break;
             case WL_CHANSPEC_CTL_SB_UU:
                 m_instance_wl[idx].wlCurrentChannel = (chanspec & WL_CHANSPEC_CHAN_MASK) + 6;
                 break;
             default:
                 m_instance_wl[idx].wlCurrentChannel = (chanspec & WL_CHANSPEC_CHAN_MASK) - 6;
                 break;
         }
      } else 
#endif
      if ((chanspec & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40) {
         sb = chanspec & WL_CHANSPEC_CTL_SB_MASK;
         m_instance_wl[idx].wlCurrentChannel = (chanspec & WL_CHANSPEC_CHAN_MASK) + ((sb == WL_CHANSPEC_CTL_SB_LOWER)?(-2):(2));
      } else {
         m_instance_wl[idx].wlCurrentChannel = chanspec & WL_CHANSPEC_CHAN_MASK;
      }
#ifndef WLCONF
   }
#endif
}

//**************************************************************************
// Function Name: getChannelIm
// Description  : get current channel interference measurement
// Returns      : none.
//**************************************************************************

int wlmngr_getChannelImState(int idx) {
    char cmd[WL_LG_SIZE_MAX];
    FILE *fp = NULL;
    char *cp = NULL;
    int sb;
    char addi[8] = {0};
    chanspec_t chanspec = wlmngr_getCurrentChSpec(idx);

    if ((chanspec & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40) {
        sb = chanspec & WL_CHANSPEC_CTL_SB_MASK;
        if (sb == WL_CHANSPEC_CTL_SB_LOWER)
            strncpy(addi, "l", sizeof(addi));
        else
            strncpy(addi, "u", sizeof(addi));
    } 
#ifdef WL11AC
    else if ((chanspec & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_80) {
            strncpy(addi, "/80", sizeof(addi));
    }
#endif
     
    snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanim_state %d%s > /var/chanim_state", idx, m_instance_wl[idx].wlCurrentChannel, addi);
    BCMWL_WLCTL_CMD(cmd);

    snprintf(cmd, sizeof(cmd), "/var/chanim_state");
    fp = fopen(cmd, "r");

    if (fp != NULL)
        cp = fgets(cmd, sizeof(cmd), fp);

    fclose(fp);
    unlink(cmd);

    if ((cp != NULL) && (*cp == '1'))
        return 1;
    else
        return 0;
}

//**************************************************************************
// Function Name: getCurrentChSpec
// Description  : get current chanspec .
// Returns      : none.
//**************************************************************************
int wlmngr_getCurrentChSpec(int idx) {
#ifdef WLCONF
   int chanspec = 0;
   char name[32];

   snprintf(name, sizeof(name), "%s", m_instance_wl[idx].m_ifcName[0]);
   wl_iovar_getint(name, "chanspec", &chanspec);
#else /* not WLCONF */
#define ID_CURSPEC_SEP '('
   char cmd[WL_LG_SIZE_MAX];
   char chanspecfile[80];
   FILE *fp = NULL;
   char *cp;
   int chanspec = 0;
   
   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanspec > /var/curchaspec%d", idx, idx );
   BCMWL_WLCTL_CMD(cmd);    
   snprintf(chanspecfile, sizeof(chanspecfile), "/var/curchaspec%d", idx );
   fp = fopen(chanspecfile, "r");
   if ( fp != NULL ) {  
      while ( fgets( cmd, sizeof(cmd), fp ) ) {   	
         cp = strchr(cmd, ID_CURSPEC_SEP);
         cp++;
         if (cp) {
            chanspec = strtol(cp, 0, 16);
            //printf("%s: current chanspec 0x%2x\n", m_instance_wl[idx].m_ifcName[MAIN_BSS_IDX], chanspec);
         }                 
      }
      fclose(fp);
      unlink(chanspecfile);      
   } else {
      printf("%s: error open file\n",__FUNCTION__);
   }
#endif

   return chanspec;
}



//**************************************************************************
// Function Name: getHwAddr
// Description  : get Mac address
// Parameters   : ifname - interface name
//                hwaddr - MAC address
// Returns      : None
//**************************************************************************
void wlmngr_getHwAddr(int idx, char *ifname, char *hwaddr) {
   char buf[WL_MID_SIZE_MAX+60];
   char cmd[WL_LG_SIZE_MAX];

   int i=0, j=0;
   char *p=NULL;
   
   strcpy(hwaddr, "");

   snprintf(cmd, sizeof(cmd), "ifconfig %s > /var/hwaddr ", ifname);
   bcmSystemMute(cmd);

   FILE *fp = fopen("/var/hwaddr", "r");
   if ( fp != NULL ) {
      if (fgets(buf, sizeof(buf), fp)) {
         for(i=2; i < (int)(sizeof(buf)-5); i++) {
            if (buf[i]==':' && buf[i+3]==':') {
               p = buf+(i-2);
               for(j=0; j < WL_MID_SIZE_MAX-1; j++, p++) {
                  if (isalnum(*p) || *p ==':') {
                     hwaddr[j] = *p;
                  }
                  else {
                     break;
                  }
               }
               hwaddr[j] = '\0';
               break;
            }
         }
      }
      fclose(fp);
   }
}

//**************************************************************************
// Function Name: setupMm_instance_wl[idx].bssMacAddr
// Description  : setup mac address for virtual interfaces
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_setupMbssMacAddr(int idx)
{
   char buf[WL_MID_SIZE_MAX];
   char cmd[WL_LG_SIZE_MAX];
   int i, start;

   /*Always read Mac from kernel*/
   wlmngr_getHwAddr(idx, m_instance_wl[idx].m_ifcName[MAIN_BSS_IDX], buf);
   strncpy(m_instance_wl[idx].bssMacAddr[MAIN_BSS_IDX], buf, WL_MID_SIZE_MAX-1);
   if (m_instance_wl[idx].m_wlVar.wlEnableUre) {
       /* URE is on, so set wlX.1 hwaddr is same as that of primary interface */
       strncpy(m_instance_wl[idx].bssMacAddr[GUEST_BSS_IDX], buf, WL_MID_SIZE_MAX-1);
       start = GUEST1_BSS_IDX;
   } else {
       start = GUEST_BSS_IDX;
   }


   // make local address
   // change proper spacing and starting of the first byte to make own local address
   bcmMacStrToNum(buf,m_instance_wl[idx].bssMacAddr[MAIN_BSS_IDX]);
   buf[0] = 96 + (buf[5]%(WL_NUM_SSID-1) * 8);
   buf[0] |= 0x2;

   for (i=start; i<WL_NUM_SSID; i++) {
       // construct virtual hw addr
       buf[5] = (buf[5]&~(WL_NUM_SSID-1))|((WL_NUM_SSID-1)&(buf[5]+1));
       bcmMacNumToStr(buf,m_instance_wl[idx].bssMacAddr[i]);
   }

   for (i=0; i<m_instance_wl[idx].numBss; i++) {
      snprintf(cmd, sizeof(cmd), "ifconfig %s hw ether %s 2>/dev/null", m_instance_wl[idx].m_ifcName[i], m_instance_wl[idx].bssMacAddr[i]);
      bcmSystem(cmd);
#ifndef WLCONF
      snprintf(cmd, sizeof(cmd), "wlctl -i %s cur_etheraddr %s  2>/dev/null", m_instance_wl[idx].m_ifcName[i], m_instance_wl[idx].bssMacAddr[i]);
      BCMWL_WLCTL_CMD(cmd);         
#endif
   } 
}

   
//**************************************************************************
// Function Name: getBands
// Description  : get Avaiable Bands (2.4G/5G)
// Parameters   : None
// Returns      : Bands
//**************************************************************************
int wlmngr_getBands(int idx)
{
   char buf[WL_LG_SIZE_MAX];
   int bands = 0;
   char cmd[80];
  
   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d bands > /var/wl%dbands", idx, idx ); 
   BCMWL_WLCTL_CMD(cmd);
   snprintf(cmd, sizeof(cmd), "/var/wl%dbands", idx ); 
   
   FILE *fp = fopen(cmd, "r");
   if ( fp != NULL ) {
      if (fgets(buf, sizeof(buf), fp)) {
         if (strchr(buf, 'a')) {
            bands |= BAND_A;
         }
         if (strchr(buf, 'b')) {
            bands |= BAND_B;
         }
      }
      fclose(fp);
   }
   m_instance_wl[idx].m_bands = bands;
   return bands;
}


//**************************************************************************
// Function Name: getCountryList
// Description  : Get country list for web GUI
// Parameters   : list - string to return
//                type - a or b
// Returns      : None
//**************************************************************************
void wlmngr_getCountryList(int idx, int argc, char **argv, char *list)
{
   char *band = 0;	
   char cmd[WL_LG_SIZE_MAX];
   char name[WL_LG_SIZE_MAX];
   char abbrv[WL_LG_SIZE_MAX];
   char line[WL_LG_SIZE_MAX];
   char *iloc, *eloc;
   int i, wsize;
   int cn = 0;
      
   if(argc >= 3) band = argv[2];
   
   if (!(*band == 'a' || *band == 'b')) {
      strcpy(list, "");
      return;
   }
   
   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d country list %c > /var/wl%dclist", idx, *band, idx);
   BCMWL_WLCTL_CMD(cmd);
   snprintf(cmd, sizeof(cmd), "/var/wl%dclist", idx );

   FILE *fp = fopen(cmd, "r");
   if ( fp != NULL ) {
      iloc = list;
      eloc = list + (WEB_BIG_BUF_SIZE_MAX_WLAN-100);

      fscanf(fp, "%*[^\n]");   // skip first line
      fscanf(fp, "%*1[\n]");  

      for (i=0;iloc<eloc;i++) {
         line[0]=0; abbrv[0]=0; name[0]=0;
         fgets(line, WL_LG_SIZE_MAX, fp);
         if (sscanf(line, "%s %[^\n]s", abbrv, name) == -1) {
            break;
         }
         sprintf(iloc, "document.forms[0].wlCountry[%d] = new Option(\"%s\", \"%s\");\n%n", i, name[0]?name:abbrv, abbrv, &wsize);
         iloc += wsize;

         if (!strcmp(m_instance_wl[idx].m_wlVar.wlCountry, abbrv)) {
            cn =1;
         }
      }

      /* Case for country abbrv that is not in countrylist. e.g US/13, put it to last */
      if (!cn && (m_instance_wl[idx].m_wlVar.wlCountry != NULL) ) {
         sprintf(iloc, "document.forms[0].wlCountry[%d] = new Option(\"%s\", \"%s\");\n%n", i, m_instance_wl[idx].m_wlVar.wlCountry, m_instance_wl[idx].m_wlVar.wlCountry, &wsize);
         iloc += wsize;
      }

      *iloc = 0;
      fclose(fp);
      unlink(cmd);
   }
}

//**************************************************************************
// Function Name: getChannelList
// Description  : Get list of allowed channels for web GUI
// Parameters   : list - string to return
// Returns      : None
// Note		: wlctl channels_in_country output may change at each wlan release
//**************************************************************************
void wlmngr_getChannelList(int idx, int argc, char **argv, char *list)
{
   char *type = 0;
   char *band = 0;
   char *bw = 0;
   char *sb = 0;
   char wlchlistfile[80];
   char wlchspeclistfile[80];
   char curPhyType[WL_SIZE_2_MAX]={0};
      
   char cmd[WL_LG_SIZE_MAX]={0};
   char channel[WL_LG_SIZE_MAX]={0};
   char *iloc, *eloc;
   int i, wsize;
   struct stat statbuf;	
   int csb = WL_CHANSPEC_CTL_SB_UPPER;  
   if(argc >= 3) type = argv[2];
   if(argc >= 4) band = argv[3];
   if(argc >= 5) bw = argv[4];   
   if(argc >= 6) sb = argv[5]; 
        
   /* Get configured phy type */
   strncpy(curPhyType, m_instance_wl[idx].m_wlVar.wlPhyType, sizeof(curPhyType)); 
   
   iloc = list;
   eloc = list + (WEB_BIG_BUF_SIZE_MAX_WLAN-100);
   sprintf(iloc, "document.forms[0].wlChannel[0] = new Option(\"Auto\", \"0\");\n%n", &wsize);
   iloc += wsize;
   i = 1;

   snprintf(wlchspeclistfile, sizeof(wlchspeclistfile), "/var/wl%dchspeclist", idx );
   unlink(wlchspeclistfile);
         
#ifdef WL_WLMNGR_DBG    
   printf("%s : (type)%s (band)%s (bw)%s (m_instance_wl[idx].m_bands)%d (sb)%s\n", __FUNCTION__, type, band, bw, m_instance_wl[idx].m_bands, sb);
#endif
            
   if((*type == 'n') || (*type == 'v')) {
      /* For 40, sideband is needed */
      if(bw && sb) {
      	 if(atoi(bw) == 40) {      	
            if (!strcasecmp(sb, "upper"))
               csb = WL_CHANSPEC_CTL_SB_UPPER;
            else
               csb = WL_CHANSPEC_CTL_SB_LOWER;
         }
      }         
   }
   
   //make sure cmd buffer resets
   *cmd = 0;
   if ((!strcmp(type, "b")) && (m_instance_wl[idx].m_bands & BAND_B)) {
         snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanspecs -b %d -w %d -c %s > /var/wl%dchspeclist", \
         idx, WL_CHANSPEC_2G, atoi(bw), m_instance_wl[idx].m_wlVar.wlCountry, idx);
   } else if ((!strcmp(type, "a")) && (m_instance_wl[idx].m_bands & BAND_A)) {
         snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanspecs -b %d -w %d -c %s > /var/wl%dchspeclist", \
         idx, WL_CHANSPEC_5G, atoi(bw), m_instance_wl[idx].m_wlVar.wlCountry, idx);
   } else if ((!strcmp(type, "n")) && *band == 'a' && (m_instance_wl[idx].m_bands & BAND_A) && (*curPhyType == 'n')) {   	  
         snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanspecs -b %d -w %d -c %s > /var/wl%dchspeclist", \
         idx, WL_CHANSPEC_5G, atoi(bw), m_instance_wl[idx].m_wlVar.wlCountry, idx);         
   } else if ((!strcmp(type, "n")) && *band == 'b' && (m_instance_wl[idx].m_bands & BAND_B) && (*curPhyType == 'n')) {  	   	   	
         snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanspecs -b %d -w %d -c %s > /var/wl%dchspeclist", \
         idx, WL_CHANSPEC_2G, atoi(bw), m_instance_wl[idx].m_wlVar.wlCountry, idx);
   } else if ((!strcmp(type, "v")) && *band == 'a' && (m_instance_wl[idx].m_bands & BAND_A) && (*curPhyType == 'v')) {  	   	
         snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanspecs -b %d -w %d -c %s > /var/wl%dchspeclist", \
         idx, WL_CHANSPEC_5G, atoi(bw), m_instance_wl[idx].m_wlVar.wlCountry, idx);   
   } else if ((!strcmp(type, "v")) && *band == 'b' && (m_instance_wl[idx].m_bands & BAND_B) && (*curPhyType == 'v')) {  	   	   	
         snprintf(cmd, sizeof(cmd), "wlctl -i wl%d chanspecs -b %d -w %d -c %s > /var/wl%dchspeclist", \
         idx, WL_CHANSPEC_2G, atoi(bw), m_instance_wl[idx].m_wlVar.wlCountry, idx);
   } 
      
   if(*cmd)
      BCMWL_WLCTL_CMD(cmd);

                    
   if (!stat(wlchlistfile, &statbuf)) {  
      FILE *fp = fopen(wlchlistfile, "r");
      if ( fp != NULL ) {
         for (; iloc<eloc; i++) {
            if (fscanf(fp, "%s", channel) != 1) {
               break;
            }
            sprintf(iloc, "document.forms[0].wlChannel[%d] = new Option(\"%s\", \"%s\");\n%n", i, channel, channel, &wsize);
            iloc += wsize;
         }
         fclose(fp);
         unlink(wlchlistfile);
      }   	
   } else if (!stat(wlchspeclistfile, &statbuf)) {
                        	
      FILE *fp = fopen(wlchspeclistfile, "r");
      if ( fp != NULL ) {
      	 char bnd[WL_MID_SIZE_MAX];
      	 char width[WL_MID_SIZE_MAX];
      	 char ctrlsb[WL_MID_SIZE_MAX];
      	 char chanspec_str[WL_MID_SIZE_MAX];
      	 int  control_channel;
      	 int  channel_index = 0;
      	 int  acbw;

         for (i = 0; iloc<eloc; i++) {
            if (atoi(bw) == 40) {
            	if (fscanf(fp, "%d%c%c %s\n", &control_channel, bnd, ctrlsb, chanspec_str)<=0)
            	   break;
              // some output doesn't have band info
                if(*bnd == 'l'|| *bnd == 'u') {
            	   *ctrlsb=*bnd;
            	   *bnd=0;
            	}
            	if((csb == WL_CHANSPEC_CTL_SB_LOWER) && (*ctrlsb != 'l')) 
            	   continue;            	   
            	if((csb == WL_CHANSPEC_CTL_SB_UPPER) && (*ctrlsb != 'u')) 
            	   continue;            	   
            	
            } else if (atoi(bw) == 20) {
            	if (fscanf(fp, "%d%c %s\n", &control_channel, bnd, chanspec_str)<=0)
            	   break;            	
            	            	
            } else if (atoi(bw) == 10) {
            	if (fscanf(fp, "%d%c%c\n", &control_channel, width, chanspec_str)<=0)
            	   break;             	 

            } else if (atoi(bw) == 80) {
            	if (fscanf(fp, "%d%c%d %s\n", &control_channel, bnd, &acbw, chanspec_str)<=0)
            	   break;             	
            }
            else {
            	 printf("unknown bandwidth:%s\n", bw);
            	 break;
            }        
            
#ifdef WL_WLMNGR_DBG            
            printf(" (channel)%d (band)%c (ctrlsb)%c (chanspecs)%s \n", control_channel, *bnd, *ctrlsb, chanspec_str);
#endif                       

            if (atoi(bw) == 80)
            	 sprintf(iloc, "document.forms[0].wlChannel[%d] = new Option(\"%d%c%d\", \"%d\");\n%n", ++channel_index, control_channel, *bnd, acbw, control_channel, &wsize);
            else
            	 sprintf(iloc, "document.forms[0].wlChannel[%d] = new Option(\"%d\", \"%d\");\n%n", ++channel_index, control_channel, control_channel, &wsize);
            iloc += wsize;
         }
         fclose(fp);
         unlink(wlchspeclistfile);         
      }    	   	
   }     
   *iloc = 0;
}

#ifdef SUPPORT_MIMO

/* From wlc_rate.[ch] */
#define MCS_TABLE_RATE(mcs, _is40) ((_is40)? mcs_rate_table[(mcs)].phy_rate_40: \
	mcs_rate_table[(mcs)].phy_rate_20)
#define MCS_TABLE_SIZE 33
	
//**************************************************************************
// Function Name: getNPhyRates
// Description  : Get list of allowed NPhyRates for web GUI
// Parameters   : list - string to return
// Returns      : None
// Note		: 
//**************************************************************************
void wlmngr_getNPhyRates(int idx, int argc, char **argv, char *list)
{
   /* From wlc_rate.[ch] */
   struct mcs_table_info {
      uint phy_rate_20;
      uint phy_rate_40;
   };

   /* rates are in units of Kbps */
   static const struct mcs_table_info mcs_rate_table[MCS_TABLE_SIZE] = {
   	{6500,   13500},	/* MCS  0 */
   	{13000,  27000},	/* MCS  1 */
   	{19500,  40500},	/* MCS  2 */
   	{26000,  54000},	/* MCS  3 */
   	{39000,  81000},	/* MCS  4 */
   	{52000,  108000},	/* MCS  5 */
   	{58500,  121500},	/* MCS  6 */
   	{65000,  135000},	/* MCS  7 */
   	{13000,  27000},	/* MCS  8 */
   	{26000,  54000},	/* MCS  9 */
   	{39000,  81000},	/* MCS 10 */
   	{52000,  108000},	/* MCS 11 */
   	{78000,  162000},	/* MCS 12 */
   	{104000, 216000},	/* MCS 13 */
   	{117000, 243000},	/* MCS 14 */
   	{130000, 270000},	/* MCS 15 */
   	{19500,  40500},	/* MCS 16 */
   	{39000,  81000},	/* MCS 17 */
   	{58500,  121500},	/* MCS 18 */
   	{78000,  162000},	/* MCS 19 */
   	{117000, 243000},	/* MCS 20 */
   	{156000, 324000},	/* MCS 21 */
   	{175500, 364500},	/* MCS 22 */
   	{195000, 405000},	/* MCS 23 */
   	{26000,  54000},	/* MCS 24 */
   	{52000,  108000},	/* MCS 25 */
   	{78000,  162000},	/* MCS 26 */
   	{104000, 216000},	/* MCS 27 */
   	{156000, 324000},	/* MCS 28 */
   	{208000, 432000},	/* MCS 29 */
   	{234000, 486000},	/* MCS 30 */
   	{260000, 540000},	/* MCS 31 */
   	{0,      6000},		/* MCS 32 */
   };
   char phyType[WL_SIZE_2_MAX]={0};

   /* -2 is for Legacy rate
    * -1 is placeholder for 'Auto'
    */
   int mcsidxs[]= { -1, -2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 32};
   int nbw = atoi(argv[2]);   /* nbw: 0 - Good Neighbor, 20 - 20MHz, 40 - 40MHz */
   char *eloc, *curloc;
   int i;

   /* Get configured phy type */
   strncpy(phyType, m_instance_wl[idx].m_wlVar.wlPhyType, sizeof(phyType)); 
   
   if ((*phyType != 'n') && (*phyType != 'v')) return;
   
   curloc = list;
   eloc = list + (WEB_BIG_BUF_SIZE_MAX_WLAN-100);

   curloc += sprintf(curloc, "\t\tdocument.forms[0].wlNMmcsidx.length = 0; \n");

   curloc += sprintf(curloc, "\t\tdocument.forms[0].wlNMmcsidx[0] = "
	                 "new Option(\"Auto\", \"-1\");\n");

         
   if( m_instance_wl[idx].m_wlVar.wlNMcsidx == -1 || m_instance_wl[idx].m_wlVar.wlRate == (ulong)0 )
	curloc += sprintf(curloc, "\t\tdocument.forms[0].wlNMmcsidx.selectedIndex = 0;\n");
   
   for (i = 1; i < (int)ARRAYSIZE(mcsidxs); i++) {
      /* MCS IDX 32 is valid only for 40 Mhz */
      if ((mcsidxs[i] == 32) && (nbw == 20 || nbw == 0))
      	continue;
      	
      curloc += sprintf(curloc, "\t\tdocument.forms[0].wlNMmcsidx[%d] = new Option(\"", i);     
      
      if (mcsidxs[i] == -2) {
      	 curloc += sprintf(curloc, "Use 54g Rate\", \"-2\");\n");
             	
         if (m_instance_wl[idx].m_wlVar.wlNMcsidx == -2 &&  nbw == m_instance_wl[idx].m_wlVar.wlNBwCap) {
      		curloc += sprintf(curloc, "\t\tdocument.forms[0].wlNMmcsidx.selectedIndex ="
      		"%d;\n", i);    		                
         }
                       		                 
      } else {
         uint mcs_rate = MCS_TABLE_RATE(mcsidxs[i], (nbw == 40));
      	 curloc += sprintf(curloc,  "%2d: %d", mcsidxs[i], mcs_rate/1000);     	
      	 /* Handle floating point generation */
         if (mcs_rate % 1000)
            curloc += sprintf(curloc, ".%d", (mcs_rate % 1000)/100);
       
        curloc += sprintf(curloc, " Mbps");
      	
      	if(nbw == 0){
	   mcs_rate = MCS_TABLE_RATE(mcsidxs[i], TRUE);
	   curloc += sprintf(curloc, " or %d", mcs_rate/1000);	   
           /* Handle floating point generation */
           if (mcs_rate % 1000)
              curloc += sprintf(curloc, ".%d", (mcs_rate % 1000)/100);           
           
           curloc += sprintf(curloc, " Mbps");

      	}
        curloc += sprintf(curloc, "\", \"%d\");\n", mcsidxs[i]);

      	if (m_instance_wl[idx].m_wlVar.wlNMcsidx == mcsidxs[i] && m_instance_wl[idx].m_wlVar.wlNBwCap == nbw)
           curloc += sprintf(curloc,"\t\tdocument.forms[0].wlNMmcsidx.selectedIndex ="
      		                 "%d;\n", i); 		                        
      		                
      }
   }
   
   if(curloc > eloc ) printf("%s: internal buffer overflow\n", __FUNCTION__);
   
   *curloc = 0;
}

#endif

//**************************************************************************
// Function Name: scanForAddr
// Description  : scan input string for mac address
// Parameters   : line - input string
//                isize - size of input string
//                start - return pointer to the address
//                size - size of mac address
// Returns      : 1 success, 0 failed
//**************************************************************************

char wlmngr_scanForAddr(char *line, int isize, char **start, int *size) {
   int is = -1;
   int i;
   for(i=2; i<isize-5; i++) {
      *start = NULL;
      if (line[i]==':' && line[i+3]==':') {
         is = i-2;
         break;
      }
   }

   for (i+=3; i<isize-2; i+=3) {
      if (line[i] != ':') {
         break;
      }
   }
   
   if (is != -1) {
      *start = line+is;
      *size = i-is + 1;
      return 1;
   }
   else {
      *size = 0;
      return 0;
   }         
}

//**************************************************************************
// Function Name: scanFileForMAC
// Description  : Look for an MAC address in a file
// Parameters   : fname - name of the file
//                mac - string for the MAC address
// Returns      : 1 - found, 0 - not found
//**************************************************************************
int wlmngr_scanFileForMAC(char *fname, char *mac) {
   char buf[WL_MID_SIZE_MAX+60];
   FILE *fp = fopen(fname, "r");
   if ( fp != NULL ) {
      for (;fgets(buf, sizeof(buf), fp);) {
         if (strstr(buf, mac)) {
            fclose(fp);
            return 1;
         }
      }
      fclose(fp);
   }
   return 0;
}

//**************************************************************************
// Function Name: aquireStationList
// Description  : Fill station list from wlctl commands
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_aquireStationList(int idx) {
   char *buf;
   struct stat statbuf;	
   int i, j;
   char cmd[WL_LG_SIZE_MAX];   
   char wl_authefile[80]; 
   char wl_assocfile[80]; 
   char wl_authofile[80]; 
   WL_STATION_LIST_ENTRY  *curBssStaList = NULL;
   WL_STATION_LIST_ENTRY  *curStaList = NULL;
   int prevNumStations;

   int l=0, k =0;
   FILE *fp = NULL;
   int buflen= 0;
   
   snprintf(wl_authefile, sizeof(wl_authefile), "/var/wl%d_authe", idx);
   snprintf(wl_assocfile, sizeof(wl_assocfile), "/var/wl%d_assoc", idx);
   snprintf(wl_authofile, sizeof(wl_authofile), "/var/wl%d_autho", idx);  

   if (m_instance_wl[idx].stationList != NULL) {
      free( m_instance_wl[idx].stationList);
      m_instance_wl[idx].stationList=NULL;
   }
   
   m_instance_wl[idx].numStations = 0;
              
   for (i=0; i<m_instance_wl[idx].numBss && i < WL_MAX_NUM_SSID; i++) {
      prevNumStations = m_instance_wl[idx].numStations;	
      m_instance_wl[idx].numStationsBSS[i] = 0;
      
      if (!m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid)
         continue;
         
      snprintf(cmd, sizeof(cmd), "wlctl -i %s authe_sta_list > /var/wl%d_authe", m_instance_wl[idx].m_ifcName[i], idx);
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s assoclist > /var/wl%d_assoc", m_instance_wl[idx].m_ifcName[i], idx);
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s autho_sta_list > /var/wl%d_autho", m_instance_wl[idx].m_ifcName[i], idx);  
      BCMWL_WLCTL_CMD(cmd);
            	
      if(!stat(wl_authefile, &statbuf)){   	  
         fp = fopen(wl_authefile, "r");
         buflen= statbuf.st_size;
         buf = (char*)malloc(buflen);
         if ( fp != NULL && buf ) {
            for (l=0;; l++) {
               if (!fgets(buf, buflen, fp)) {
                  break;
               }
            }
            if (l != 0) {
               int asize;
               char *pa; 
               curBssStaList = malloc (sizeof(WL_STATION_LIST_ENTRY) *l );
	        if ( curBssStaList == NULL ) {
			printf("%s::%s@%d curBssStaList malloc Error\n", __FILE__, __FUNCTION__, __LINE__ );
			return;
	        }
			
               rewind(fp);
                              
               for (j=0; j<l; j++) {
                  if (fgets(buf, buflen, fp)) {                
                     if (wlmngr_scanForAddr(buf, strlen(buf), &pa, &asize)) {// find mac addr
                        if (asize > WL_MID_SIZE_MAX -1) {
                           asize = WL_MID_SIZE_MAX -1;
                        }
                        strncpy(curBssStaList[j].macAddress, pa, asize);
                        curBssStaList[j].macAddress[asize] = '\0';
                        // scan in other lists
                        curBssStaList[j].associated = wlmngr_scanFileForMAC(wl_assocfile, curBssStaList[j].macAddress);
                        curBssStaList[j].authorized = wlmngr_scanFileForMAC(wl_authofile, curBssStaList[j].macAddress);                        
                        // additional info for MBSS
                        strncpy(curBssStaList[j].ssid, m_instance_wl[idx].m_wlMssidVar[i].wlSsid, sizeof(curBssStaList[j].ssid));
                        strncpy(curBssStaList[j].ifcName, m_instance_wl[idx].m_ifcName[i], sizeof(curBssStaList[j].ifcName));                        
                        //counters
                        m_instance_wl[idx].numStationsBSS[i]++;
                        m_instance_wl[idx].numStations++;
                     }
                  }
                  else {
                     break;
                  }
               }
               
               if (m_instance_wl[idx].stationList) {
               	  // create a new list, copy previous and append new sta
              	curStaList = malloc ( sizeof(WL_STATION_LIST_ENTRY) *(m_instance_wl[idx].numStations) );
	        	if ( curStaList == NULL ) {
				printf("%s::%s@%d curBssStaList malloc Error\n", __FILE__, __FUNCTION__, __LINE__ );
				return;
		        }
				  
              	  for (k=0; k< prevNumStations; k++) {
              	     curStaList[k]=m_instance_wl[idx].stationList[k];
              	  } 
				  
	           	  for (k=0; k< m_instance_wl[idx].numStationsBSS[i]; k++) {
              	     curStaList[k+prevNumStations]=curBssStaList[k];
              	  } 
			  if ( curBssStaList )
	              	  free(curBssStaList);
			  if ( m_instance_wl[idx].stationList )
	                       free(m_instance_wl[idx].stationList);
                  	  m_instance_wl[idx].stationList=curStaList;
              }
		else {   	
                  	 m_instance_wl[idx].stationList=curBssStaList;
              }            
            }
            fclose(fp);
         }           
         free(buf);         
      }
   }
// Write AssociationDevice List to MDM???
//   wlUnlockWriteAssocDev( idx+1 );

}

//**************************************************************************
// Function Name: getNumStations
// Description  : Return number of stations in the list
// Parameters   : None
// Returns      : Number of stations
//**************************************************************************
int wlmngr_getNumStations(int idx) {
   return m_instance_wl[idx].numStations;
}

//**************************************************************************
// Function Name: getStationList
// Description  : Return one station list entry
// Parameters   : i - station index
//                macAddress - MAC address
//                associated - Whether the station is associated
//                authorized - Whether the station is authorized
// Returns      : None
//**************************************************************************
void wlmngr_getStation(int idx, int i, char *macAddress, char *associated,  char *authorized, char *ssid, char *ifcName) {
   strcpy(macAddress, m_instance_wl[idx].stationList[i].macAddress);
   *associated = m_instance_wl[idx].stationList[i].associated;
   *authorized = m_instance_wl[idx].stationList[i].authorized;
   strcpy(ssid, m_instance_wl[idx].stationList[i].ssid);
   strcpy(ifcName, m_instance_wl[idx].stationList[i].ifcName);   
}


#ifdef IDLE_PWRSAVE
static inline void wlmngr_togglePowerSave_aspm(int idx, char assoc, char *cmd)
{
      if (!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_AC)) {
          if (assoc) {
	      sprintf(cmd, "wlctl -i wl%d aspm 0", idx);
          }
          else {
              /* Only enable L1 mode, because L0s creates EVM issues. The power savings are the same */
	      if (m_instance_wl[idx].m_wlVar.GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ)
	          sprintf(cmd, "wlctl -i wl%d aspm 0x102", idx);	    
	      else
	          sprintf(cmd, "wlctl -i wl%d aspm 0x2", idx);
	  }
          bcmSystem(cmd);          
      }
}
#endif

//**************************************************************************
// Function Name: togglePowerSave
// Description  : Disable PowerSave feature if any STA is associated, otherwise leave as current setting
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_togglePowerSave() {
   char cmd[WL_LG_SIZE_MAX];
   char addr[WL_MID_SIZE_MAX]; 
   char assoc=0, auth=0, associated[2]={0, 0};
   char ssid[WL_SSID_SIZE_MAX];
   char ifcName[WL_SM_SIZE_MAX];
   int stas, i, j;
#ifdef DMP_X_BROADCOM_COM_PWRMNGT_1 /* aka SUPPORT_PWRMNGT */
   char anyAssociated = 0;
   CmsRet ret = CMSRET_SUCCESS;
   PwrMngtObject pwrMngtObj;
   PWRMNGT_CONFIG_PARAMS configParms;
   int curAVSEn=0;
   UINT32 configMask;
   int chipId;
#endif

   for (i=0; i<wl_cnt; i++) {
      stas = wlmngr_getNumStations(i);
      associated[i] = 0;
      for(j=0; j<stas; j++) {
         wlmngr_getStation(i, j, addr, &assoc, &auth, ssid, ifcName);
         if (assoc) {
            associated[i] = 1;  /* for each adapter */
#ifdef DMP_X_BROADCOM_COM_PWRMNGT_1
            anyAssociated = 1;  /* for global association of all adapters */
#endif
            break; /* associated sta found */
         }
      } 
   }

   for (i=0; i<wl_cnt; i++) {
      if (associated[i]) {
          /* disable RxChainPowerSave */
          snprintf(cmd, sizeof(cmd), "wlctl -i %s rxchain_pwrsave_enable 0", m_instance_wl[i].m_ifcName[0]);
          BCMWL_WLCTL_CMD(cmd);
      } else {
          /* restore back to the user setting */
          snprintf(cmd, sizeof(cmd), "wlctl -i %s rxchain_pwrsave_enable %d", m_instance_wl[i].m_ifcName[0], m_instance_wl[i].m_wlVar.wlRxChainPwrSaveEnable);
          BCMWL_WLCTL_CMD(cmd);
      }

#if defined(IDLE_PWRSAVE) && defined(DMP_X_BROADCOM_COM_PWRMNGT_1)
      wlmngr_togglePowerSave_aspm(i, anyAssociated, cmd);
#endif
   }
   
#ifdef DMP_X_BROADCOM_COM_PWRMNGT_1 /* aka SUPPORT_PWRMNGT */
   memset(&pwrMngtObj, 0x00, sizeof(pwrMngtObj)) ;
   memset(&configParms, 0x00, sizeof(configParms)) ;
   if ((ret=wlReadPowerManagement(&pwrMngtObj)) != CMSRET_SUCCESS)
   {
      printf("Failed to get pwrMngtObj, ret=%d", ret);
      return;
   }
   curAVSEn = pwrMngtObj.avsEn;

   chipId = sysGetChipId();
#ifdef WL_WLMNGR_DBG
   printf("chipId = 0x%x\n", chipId);
#endif

   if (chipId == 0x6362) { /* only enabled for 6362 */
       if (anyAssociated) {
           /* disable Adaptive Voltage Scaling */
           configParms.avs = PWRMNGT_DISABLE;
           configMask = PWRMNGT_CFG_PARAM_MEM_AVS_MASK;
           PwrMngtCtl_SetConfig( &configParms, configMask, NULL ) ;
       } else {
           /* restore back to the user setting */
           configParms.avs = curAVSEn;
           configMask = PWRMNGT_CFG_PARAM_MEM_AVS_MASK;
           PwrMngtCtl_SetConfig( &configParms, configMask, NULL ) ;
       }
   }
#endif
}

//**************************************************************************
// Function Name: getVer
// Description  : get wireless driver version
// Parameters   : ver - version string
//                size - max string size
// Returns      : None
//**************************************************************************

void wlmngr_getVer(int idx)
{
   char *tag = "version ";
   char buf[WL_LG_SIZE_MAX];
   char *p;

   FILE *fp = NULL;

   snprintf(buf, sizeof(buf), "wlctl -i %s  ver > /var/wlver ", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(buf);      
   fp = fopen("/var/wlver", "r");
   if ( fp != NULL ) {
         fgets(buf, sizeof(buf), fp);
         fgets(buf, sizeof(buf), fp);
         p = strstr(buf, tag);
         if (p != NULL) {
         	p += strlen(tag);
         	sscanf(p, "%32s", m_instance_wl[idx].wlVer);
         }
         fclose(fp);         
   } else {
         printf("/var/wlver open error\n");   	
   }
}

//**************************************************************************
// Function Name: setupAntenna
// Description  : Setup Antenna in use
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_setupAntenna( int idx)
{  
   char cmd[WL_LG_SIZE_MAX];

   snprintf(cmd, sizeof(cmd), "wlctl -i %s  antdiv %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlAntDiv);
   BCMWL_WLCTL_CMD(cmd);      
}

//**************************************************************************
// Function Name: setupChannel
// Description  : Setup channel 
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_autoChannel( int idx)
{  
#ifdef WLCONF
   nvram_set("acs_mode", "legacy");
#else
   char cmd[WL_LG_SIZE_MAX];
   int timeout = 0;
   
   // channel
   if (m_instance_wl[idx].m_wlVar.wlChannel == 0) {
      // Auto Channel Selection - when channel # is not 0
      snprintf(cmd, sizeof(cmd), "wlctl -i %s autochannel 1", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      sleep(1);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s autochannel 2", m_instance_wl[idx].m_ifcName[0]); 
      BCMWL_WLCTL_CMD(cmd);      
      
      timeout = m_instance_wl[idx].m_wlVar.wlCsScanTimer;
   }
   
   //configure or disable auto channel periodical timer
   snprintf(cmd, sizeof(cmd), "wlctl -i %s csscantimer %d", m_instance_wl[idx].m_ifcName[0], timeout);
   BCMWL_WLCTL_CMD(cmd);             	
           	
   wlmngr_getCurrentChannel(idx);       	
#endif /* WLCONF */
}

//**************************************************************************
// Function Name: setupRegulatory
// Description  : Setup regulatory
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_setupRegulatory(int idx)
{  
   char cmd[WL_LG_SIZE_MAX];
   char buf[WL_MID_SIZE_MAX];
      
   // reset
   snprintf(cmd, sizeof(cmd), "wlctl -i %s regulatory 0", m_instance_wl[idx].m_ifcName[0]);  
   BCMWL_WLCTL_CMD(cmd);
  
   snprintf(cmd, sizeof(cmd), "wlctl -i %s radar 0 2>/dev/null", m_instance_wl[idx].m_ifcName[0]); 
   BCMWL_WLCTL_CMD(cmd);
 
   snprintf(cmd, sizeof(cmd), "wlctl -i %s spect 0 2>/dev/null", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(cmd);
         
   snprintf(buf, sizeof(buf), "wl%d_reg_mode", idx);
   if ((m_instance_wl[idx].m_wlVar.wlRegMode == REG_MODE_H) && (m_instance_wl[idx].m_wlVar.wlBand == BAND_A)) {     

      nvram_set(buf, "h");
            
	  snprintf(cmd, sizeof(cmd), "wlctl -i %s radar 1", m_instance_wl[idx].m_ifcName[0]); 
      BCMWL_WLCTL_CMD(cmd);
          
	  snprintf(cmd, sizeof(cmd), "wlctl -i %s spect 1", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);           
      // set the CAC params		
      snprintf(cmd, sizeof(cmd), "wlctl -i %s dfs_preism %d",  m_instance_wl[idx].m_ifcName[0],  m_instance_wl[idx].m_wlVar.wlDfsPreIsm);  
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s dfs_postism %d",  m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlDfsPostIsm);  
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s constraint %d",  m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlTpcDb);  
      BCMWL_WLCTL_CMD(cmd);                   
   } else if (m_instance_wl[idx].m_wlVar.wlRegMode == REG_MODE_D) {
      nvram_set(buf, "d");	
      snprintf(cmd, sizeof(cmd), "wlctl -i %s regulatory 1", m_instance_wl[idx].m_ifcName[0]);  
      BCMWL_WLCTL_CMD(cmd);
   } else
      nvram_set(buf, "off");

}

//**************************************************************************
// Function Name: printSsidList
// Description  : Print SSID list for multiple set of options in security page
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_printSsidList(int idx, char *text)
{
   int i, wsize;
   char *prtloc = text;
   for (i=0; i<m_instance_wl[idx].numBss; i++) {
        char ssidStr[BUFLEN_64];

        /* If there are any escape characters in the ssid, cmsXml_escapeStringEx will replace them; */
        cmsXml_escapeStringEx(m_instance_wl[idx].m_wlMssidVar[i].wlSsid, ssidStr, sizeof(ssidStr)-1);     
        
        sprintf(prtloc, "<option value='%d'>%s</option>\n%n", i, ssidStr, &wsize);
        prtloc +=wsize;
   }
   *prtloc = 0;
} 

//**************************************************************************
// Function Name: printMbssTbl
// Description  : Print MBSS list in wlcfg.html (wireless basic) in format below
//
//                <td><input type='checkbox' name='wlEnbl_wl0v1' value="ON"></td>
//                <td><input type='text' name='wlSsid_wl0v1' maxlength="32" size="32"></td>
//                <td><input type='checkbox' name='wlHide_wl0v1' value="ON"></td>                       
//                <td><input type='checkbox' name='wlAPIsolation_wl0v1' value="ON"></td>   
//                <td>BSSID</td> 
// 
// Parameters   : text: input buffer
// Returns      : None
//**************************************************************************
void wlmngr_printMbssTbl(int idx, char *text)
{
   int i, wsize;
   char *prtloc = text;
   #ifdef HSPOT_SUPPORT
   #if 0
   char varname[NVRAM_MAX_PARAM_LEN];
   char *ptr;
   #endif
   #endif
   for (i=MAIN_BSS_IDX+1; i<WL_NUM_SSID; i++) {      
      sprintf(prtloc, "<tr><td><input type='checkbox' name='wlEnbl_wl%dv%d' value='ON' %s></td> \n%n", 
	  	idx, i, ((m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid==ON)?"CHECKED":"") , &wsize); 
      prtloc +=wsize; 
     
      sprintf(prtloc, "<td><input type='text' name='wlSsid_wl%dv%d' maxlength='32' size='32' value='%s'></td> \n%n", 
	  	idx, i, m_instance_wl[idx].m_wlMssidVar[i].wlSsid , &wsize); 
      prtloc +=wsize;
      
      sprintf(prtloc, "<td><input type='checkbox' valign='middle' align='center' name='wlHide_wl%dv%d' value='ON' %s></td> \n%n", 
	  	idx, i, ((m_instance_wl[idx].m_wlMssidVar[i].wlHide==ON)?"CHECKED":"") , &wsize); 
      prtloc +=wsize;
      
      sprintf(prtloc, "<td><input type='checkbox' valign='middle' align='center' name='wlAPIsolation_wl%dv%d' value='ON' %s></td> \n%n", 
	  	idx, i, ((m_instance_wl[idx].m_wlMssidVar[i].wlAPIsolation==ON)?"CHECKED":"") , &wsize); 
      prtloc +=wsize;

      sprintf(prtloc, "<td><input type='checkbox' valign='middle' align='center' name='wlDisableWme_wl%dv%d' value='ON' %s %s></td> \n%n", 
	  	idx, i, ((m_instance_wl[idx].m_wlMssidVar[i].wlDisableWme==ON)?"CHECKED":""), ((m_instance_wl[idx].m_wlVar.wlWme==OFF)?"DISABLED":""), &wsize); 
      prtloc +=wsize;
#ifdef WMF
      sprintf(prtloc, "<td><input type='checkbox' valign='middle' align='center' name='wlEnableWmf_wl%dv%d' value='ON' %s %s></td> \n%n", 
	  	idx, i, ((m_instance_wl[idx].m_wlMssidVar[i].wlEnableWmf==ON)?"CHECKED":""), ((m_instance_wl[idx].wmfSupported==OFF)?"DISABLED":""), &wsize); 
      prtloc +=wsize;
#endif      
#ifdef HSPOT_SUPPORT
#if 0 /* temporarily remove the Hspot for BSSID */
      if(m_instance_wl[idx].m_wlMssidVar[i].wlEnableHspot!=2) {
	      snprintf(varname, sizeof(varname),"wl%d.%d_akm", idx,i);
	      ptr = nvram_get(varname);
	      if(ptr && (!strcmp(ptr,"wpa2")))
		      sprintf(prtloc, "<td valign='middle' align='center'><input type='checkbox' name='wlEnableHspot_wl%dv%d' value='ON' %s ></td> \n%n", 
				      idx, i, ((m_instance_wl[idx].m_wlMssidVar[i].wlEnableHspot==ON)?"CHECKED":""), &wsize); 
	      else
		      sprintf(prtloc, "<td valign='middle' align='center' width='60'><input type='checkbox' disabled=true name='wlEnableHspot_wl%dv%d' value='ON' %s >[wpa2!]</td> \n%n", 
				      idx, i, ((m_instance_wl[idx].m_wlMssidVar[i].wlEnableHspot==ON)?"CHECKED":""), &wsize); 
	      prtloc +=wsize;
      }
#endif      
#endif

      sprintf(prtloc, "<td><input type='text' valign='middle' align='center' name='wlMaxAssoc_wl%dv%d' maxlength='3' size='3' value='%d'></td> \n%n", 
	  	idx, i, m_instance_wl[idx].m_wlMssidVar[i].wlMaxAssoc , &wsize); 
      prtloc +=wsize;             

      if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid)
         sprintf(prtloc, "<td>%s</td></tr> \n%n",m_instance_wl[idx].bssMacAddr[i], &wsize); 
      else   
         sprintf(prtloc, "<td>N/A</td></tr> \n%n", &wsize);
         
      prtloc +=wsize;      
   }
   *prtloc = 0;
}

/***************************************************************************
// Function Name: getMaxMbss.
// Description  : getMaxMbss
// Parameters   : none.
// Returns      : n/a.
****************************************************************************/
int wlmngr_getMaxMbss(int idx)
{

      if (m_instance_wl[idx].maxMbss==0) 
         wlmngr_initVars(idx);
	  
      return m_instance_wl[idx].maxMbss; 
}

//**************************************************************************
// Function Name: doQoS
// Description  : setup ebtables marking for wireless interface.
//                On systems with CMS, the QoS settings will be done
//                within the CMS core code.  This function is left here
//                as a stub for non-CMS customers to fill in if desired.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doQoS(int idx)
{
   if (!m_instance_wl[idx].wmeSupported)
   	return;
}

void wlmngr_WlConfDown(int idx) {
    extern int wlconf_down(char *name);
    char name[32];

    snprintf(name, sizeof(name), "%s", m_instance_wl[idx].m_ifcName[0]);
    wlconf_down(name);
}

void wlmngr_WlConfStart(int idx) {
    extern int wlconf_start(char *name);
    char name[32];

    snprintf(name, sizeof(name), "%s", m_instance_wl[idx].m_ifcName[0]);
    wlconf_start(name);
}

//**************************************************************************
// Function Name: doWlConf
// Description  : basic setup for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doWlConf(int idx) {
   extern int wlconf(char *name);
   extern int wlconf_start(char *name);  
   char name[32];

#ifdef SUPPORT_MIMO
   if (MIMO_PHY) {
      // n required, i.e. if 1, supports only 11n STA, 11g STA cannot not assoc
      if(strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF)) {
         snprintf(name, sizeof(name), "wlctl -i %s nreqd %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlNReqd);
         BCMWL_WLCTL_CMD(name);
      }     
   }      
#endif 

   snprintf(name, sizeof(name), "%s", m_instance_wl[idx].m_ifcName[0]);
   wlconf(name);

   wlmngr_doWlconfAddi(idx);
}
#ifndef CONFIG_CMS_UTIL_HAS_GETPIDBYNAME
//**************************************************************************
// Function Name: getpidbyname
// Description  : given a name of a process/thread, return the pid.  This
//                local version is used only if the libcms_util does not
//                have this function.
// Parameters   : name of the process/thread
// Returns      : pid or -1 if not found
//**************************************************************************
#include <fcntl.h>
#include <dirent.h>
int prctl_getPidByName(const char *name)
{
   DIR *dir;
   FILE *fp;
   struct dirent *dent;
   UBOOL8 found=FALSE;
   int pid, rc, p, i;
   int rval = CMS_INVALID_PID;
   char filename[BUFLEN_256];
   char processName[BUFLEN_256];

   if (NULL == (dir = opendir("/proc")))
   {
      cmsLog_error("could not open /proc");
      return rval;
   }

   while (!found && (dent = readdir(dir)) != NULL)
   {
      /*
       * Each process has its own directory under /proc, the name of the
       * directory is the pid number.
       */
      if ((dent->d_type == DT_DIR) &&
          (CMSRET_SUCCESS == cmsUtl_strtol(dent->d_name, NULL, 10, &pid)))
      {
         snprintf(filename, sizeof(filename), "/proc/%d/stat", pid);
         if ((fp = fopen(filename, "r")) == NULL)
         {
            cmsLog_error("could not open %s", filename);
         }
         else
         {
            /* Get the process name, format: 913 (consoled) */
            memset(processName, 0, sizeof(processName));
            rc = fscanf(fp, "%d (%s", &p, processName);
            fclose(fp);

            if (rc >= 2)
            {
               i = strlen(processName);
               if (i > 0)
               {
                  /* strip out the trailing ) character */
                  if (processName[i-1] == ')')
                     processName[i-1] = 0;
               }
            }

            if (!cmsUtl_strcmp(processName, name))
            {
               rval = pid;
               found = TRUE;
            }
         }
      }
   }

   closedir(dir);

   return rval;
}
#endif  /* CONFIG_CMS_UTIL_HAS_GETPIDBYNAME */


//**************************************************************************
// Function Name: get_event_pid
// Description  : get the pid of the event thread which is processing the
//                specified wlan adapter unit number
// Parameters   : wlan adapter unit number
// Returns      : the pid of the events/0 or events/1 thread, or 0 on error
//**************************************************************************
#define TMP_WLCTL_TP_ID_FILENAME "/tmp/wlctl_tp_id"

static int get_event_pid(int wl_unit)
{
   char *name =NULL;
   int pid=CMS_INVALID_PID;  /* defined to 0 */
   int cpu=-1;
   char tmpBuf[64]={0};
   int fd;

   /*
    * first we need to find out which event thread is processing the
    * packets for this adapter.
    */
   snprintf(tmpBuf, sizeof(tmpBuf), "wlctl -i wl%d tp_id > %s",
                                          wl_unit, TMP_WLCTL_TP_ID_FILENAME);
   BCMWL_WLCTL_CMD(tmpBuf);
   fd = open(TMP_WLCTL_TP_ID_FILENAME, O_RDONLY);
   if (fd > 0)
   {
      int rc;
      memset(tmpBuf, 0, sizeof(tmpBuf));
      rc = read(fd, tmpBuf, sizeof(tmpBuf)-1);
      close(fd);
      cpu = atoi(tmpBuf);
      cmsLog_debug("read of output: rc=%d cpu=%d buf=%s", rc, cpu, tmpBuf);
   }
   unlink(TMP_WLCTL_TP_ID_FILENAME);

   if (cpu == 0)
      name = "events/0";
   else if (cpu == 1)
      name = "events/1";

   if (name != NULL) {
      pid = prctl_getPidByName(name);
   }

   cmsLog_debug("returning pid %d", pid);
   return pid;
}

//**************************************************************************
// Function Name: setpriority
// Description  : set the processing thread of the given wlan adapter unit to
//                real time priority
// Parameters   : wlan adapter unit number
// Returns      : None.
//**************************************************************************
static void wlmngr_setpriority(int wl_unit)
{
   int pid=-1, pid_wfd=-1;
   struct sched_param sp;
   sp.sched_priority = 5;/*should be same as CONFIG_BRCM_SOFTIRQ_BASE_RT_PRIO*/

   if (wl_unit == 0) {
      /* first try to find worker pid by Linux 3.4 name */
      pid = prctl_getPidByName("wl0-kthrd");
      if (pid <= 0)
      {
         /* get worker pid from Linux 2.6.30 kernel */
         pid = get_event_pid(wl_unit);
      }
      pid_wfd = prctl_getPidByName("wfd0-thrd");
   }
   else if (wl_unit == 1) {
      pid = prctl_getPidByName("wl1-kthrd");
      if (pid <= 0)
      {
         pid = get_event_pid(wl_unit);
      }
      pid_wfd = prctl_getPidByName("wfd1-thrd");
   }
   else if (wl_unit == 2) {
       pid = prctl_getPidByName("wl2-kthrd");
       if (pid <= 0)
       {
          pid = get_event_pid(wl_unit);
       }
       pid_wfd = prctl_getPidByName("wfd2-thrd");
   }

   if (pid > 0) {
      if (sched_setscheduler(pid, SCHED_RR, &sp) == -1)
         cmsLog_error("Failed to make wlan processing on unit %d realtime", wl_unit);
      //else
      //   printf("wlmngr: %s is running as Real Time process.\n", name);
   }
   if (pid_wfd > 0) {
      if (sched_setscheduler(pid_wfd, SCHED_RR, &sp) == -1)
         cmsLog_error("Failed to make wlan processing on unit %d realtime", wl_unit);
   }
}

//**************************************************************************
// Function Name: wlmngr_detectSMP
// Description  : check if we are running on a SMP system
// Parameters   : None.
// Returns      : 1 if we are on SMP system, 0 otherwise.
//**************************************************************************
#define TMP_WL_SMP_FILENAME "/tmp/wl_smp"

int wlmngr_detectSMP()
{
   char tmpBuf[256]={0};
   int fd;
   int is_smp=0;

   snprintf(tmpBuf, sizeof(tmpBuf), "cat /proc/version > %s",
                                          TMP_WL_SMP_FILENAME);
   bcmSystemEx(tmpBuf, 1);
   fd = open(TMP_WL_SMP_FILENAME, O_RDONLY);
   if (fd > 0)
   {
      memset(tmpBuf, 0, sizeof(tmpBuf));
      read(fd, tmpBuf, sizeof(tmpBuf)-1);
      close(fd);
      if (strstr(tmpBuf, "SMP"))
         is_smp = 1;
   }
   unlink(TMP_WL_SMP_FILENAME);

   return is_smp;
}

#ifdef WLCONF
void wlmngr_doWlconfAddi(int idx) 
{
   char cmd[WL_LG_SIZE_MAX];
   char rs_11b_1_2[] = "1b 2b 5.5 11";
   char rs_11g_1_2[] = "1b 2b 5.5 6 9 11 12 18 24 36 48 54";
   char rs_11g_wifi_2[] = "1b 2b 5.5b 6b 11b 12b 24b 9 18 36 48 54";
   char rs_11a_1_2[] = "6b 9 12b 18 24 36 48 54";
   char rs_11a_wifi_2[] = "6b 9 12b 18 24b 36 48 54";   
   char rs_allbasic[] = "all";
   char rs_default[] = "default";
   char *rateptr = rs_default;
   int chipinfo_stbc;

#ifdef SUPPORT_MIMO
   if (MIMO_PHY) {   

      chipinfo_stbc = wlmngr_ReadChipInfoStbc();
#ifdef WL_WLMNGR_DBG
      printf("chipinfo_stbc = %d (idx=%d)\n", chipinfo_stbc, idx);
#endif

      if ((chipinfo_stbc == 0) && (idx == 0)) {
          snprintf(cmd, sizeof(cmd), "wlctl -i %s stbc_rx 0", m_instance_wl[idx].m_ifcName[0]);
          BCMWL_WLCTL_CMD(cmd);
      } else {
          snprintf(cmd, sizeof(cmd), "wlctl -i %s stbc_rx %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlStbcRx);
          BCMWL_WLCTL_CMD(cmd);
      }

      /* Set Rxchain power save sta assoc check default to 1 */ 
      snprintf(cmd, sizeof(cmd), "wlctl -i %s rxchain_pwrsave_stas_assoc_check %d", m_instance_wl[idx].m_ifcName[0], ON);
      BCMWL_WLCTL_CMD(cmd);

      /* Set Radio power save sta assoc check default to 1 */ 
      snprintf(cmd, sizeof(cmd), "wlctl -i %s radio_pwrsave_stas_assoc_check %d", m_instance_wl[idx].m_ifcName[0], ON);
      BCMWL_WLCTL_CMD(cmd);
   }      
#endif    
         
   
   // basic rate
   if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_ALL) == 0 ) {
      rateptr = rs_allbasic;
   } else {
      if (m_instance_wl[idx].m_wlVar.wlBand == BAND_B) { //options for 11b/g	
         if(strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_B) == 0) {  	
            if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_1_2) == 0 ) {
               rateptr = rs_11b_1_2;
            } 
         } else { //for all non-11b
            if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_1_2) == 0 ) {
               rateptr = rs_11g_1_2;
            } else if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_WIFI_2) == 0 ){
               rateptr = rs_11g_wifi_2;
            }
         }
      } else { //options of 11a
         if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_1_2) == 0 ) {
            rateptr = rs_11a_1_2;
         }
         else if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_WIFI_2) == 0 ){
            rateptr = rs_11a_wifi_2;
         }                   	
     }
   }
   
   if(rateptr != rs_default) {
      snprintf(cmd, sizeof(cmd), "wlctl -i %s rateset %s", m_instance_wl[idx].m_ifcName[0], rateptr);
      BCMWL_WLCTL_CMD(cmd);
   }

   /* 
    * raise priority for wlan processing thread to boost throughput
    */
   wlmngr_setpriority(idx);

}
#else /* not WLCONF */
//**************************************************************************
// Function Name: doAdvanced
// Description  : advanced setup for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
void wlmngr_doAdvanced(int idx) {
   char cmd[WL_LG_SIZE_MAX];
   char rs_11b_1_2[] = "1b 2b 5.5 11";
   char rs_11g_1_2[] = "1b 2b 5.5 6 9 11 12 18 24 36 48 54";
   char rs_11g_wifi_2[] = "1b 2b 5.5b 6b 11b 12b 24b 9 18 36 48 54";
   char rs_11a_1_2[] = "6b 9 12b 18 24 36 48 54";
   char rs_11a_wifi_2[] = "6b 9 12b 18 24b 36 48 54";   
   char rs_allbasic[] = "all";
   char rs_default[] = "default";
   char *rateptr = rs_default;
   int  val;
   int  i;
   int nmode = OFF; /*runtime*/
   int chipinfo_stbc;

   // max association   
   if(m_instance_wl[idx].mbssSupported) {            	
      snprintf(cmd, sizeof(cmd), "wlctl -i %s maxassoc %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc);
      BCMWL_WLCTL_CMD(cmd);   
      
      for (i=0; i<m_instance_wl[idx].numBss; i++) {
         if(m_instance_wl[idx].m_wlMssidVar[i].wlMaxAssoc > m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc)
      	   m_instance_wl[idx].m_wlMssidVar[i].wlMaxAssoc = m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc;
      	   
      	 if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {      		   
       	   snprintf(cmd, sizeof(cmd), "wlctl -i %s bss_maxassoc %d", m_instance_wl[idx].m_ifcName[i], m_instance_wl[idx].m_wlMssidVar[i].wlMaxAssoc);
       	   BCMWL_WLCTL_CMD(cmd);
       	}  		
      }
   } else {
      m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc = m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlMaxAssoc;
      snprintf(cmd, sizeof(cmd), "wlctl -i %s maxassoc %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlMaxAssoc);
      BCMWL_WLCTL_CMD(cmd);       
   }   

   if (m_instance_wl[idx].m_wlVar.wlBand == BAND_B) {
      int override = WLC_PROTECTION_OFF;
      int control =  WLC_PROTECTION_CTL_OFF;

      // g mode
      switch ( m_instance_wl[idx].m_wlVar.wlgMode ) {
         case WL_MODE_G_PERFORMANCE:
            snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode Performance", m_instance_wl[idx].m_ifcName[0]);
            break;
         case WL_MODE_G_LRS:
            snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode LRS", m_instance_wl[idx].m_ifcName[0]);
            break;
         case WL_MODE_B_ONLY:
            snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode legacy", m_instance_wl[idx].m_ifcName[0]);
            break;
         default:
            snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode Auto", m_instance_wl[idx].m_ifcName[0]);
            break;
      }
      
       BCMWL_WLCTL_CMD(cmd);
      
      // g protection
       if (strcmp(m_instance_wl[idx].m_wlVar.wlProtection, WL_OFF)) {
          override = WLC_PROTECTION_AUTO;
          control =  WLC_PROTECTION_CTL_OVERLAP;
       }
          
       snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode_protection_override %d", m_instance_wl[idx].m_ifcName[0], override);
       BCMWL_WLCTL_CMD(cmd);
       snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode_protection_control %d", m_instance_wl[idx].m_ifcName[0], control);
       BCMWL_WLCTL_CMD(cmd);       


   }
   
    //fix up protection_control again (same variable between g/n in driver but different in webui)
#ifdef SUPPORT_MIMO
   if (MIMO_PHY) {   
      int override = WLC_PROTECTION_OFF;
      int control =  WLC_PROTECTION_CTL_OFF;
         	
      if (!strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF)) {
         //use g
         if (strcmp(m_instance_wl[idx].m_wlVar.wlProtection, WL_OFF)) {
            override = WLC_PROTECTION_AUTO;
            control =  WLC_PROTECTION_CTL_OVERLAP;
         }          
         snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode_protection_override %d", m_instance_wl[idx].m_ifcName[0], override);
         BCMWL_WLCTL_CMD(cmd);
         snprintf(cmd, sizeof(cmd), "wlctl -i %s gmode_protection_control %d", m_instance_wl[idx].m_ifcName[0], control);
         BCMWL_WLCTL_CMD(cmd);  
      } else {      	
         //use n
         if (strcmp(m_instance_wl[idx].m_wlVar.wlNProtection, WL_OFF)) {
            override = WLC_PROTECTION_AUTO;
            control =  WLC_PROTECTION_CTL_OVERLAP;
         }          
         snprintf(cmd, sizeof(cmd), "wlctl -i %s nmode_protection_override %d", m_instance_wl[idx].m_ifcName[0], override);
         BCMWL_WLCTL_CMD(cmd);
         snprintf(cmd, sizeof(cmd), "wlctl -i %s protection_control %d", m_instance_wl[idx].m_ifcName[0], control);
         BCMWL_WLCTL_CMD(cmd);      	      	
      }

      if (m_instance_wl[idx].m_wlVar.wlRifsAdvert != 0)
         snprintf(cmd, sizeof(cmd), "wlctl -i %s rifs_advert -1", m_instance_wl[idx].m_ifcName[0]);
      else
         snprintf(cmd, sizeof(cmd), "wlctl -i %s rifs_advert 0", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);

      if ((m_instance_wl[idx].m_wlVar.wlNBwCap == 3) && (m_instance_wl[idx].m_wlVar.wlBand == BAND_B)) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s obss_coex %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlObssCoex);
      } else {
        /* Need to disable obss coex in case of 20MHz and/or in case of 5G.  */
          snprintf(cmd, sizeof(cmd), "wlctl -i %s obss_coex 0", m_instance_wl[idx].m_ifcName[0]);
      }
      BCMWL_WLCTL_CMD(cmd);

      chipinfo_stbc = wlmngr_ReadChipInfoStbc();
#ifdef WL_WLMNGR_DBG
      printf("chipinfo_stbc = %d (idx=%d)\n", chipinfo_stbc, idx);
#endif

      if ((chipinfo_stbc == 0) && (idx == 0)) {
          snprintf(cmd, sizeof(cmd), "wlctl -i %s stbc_rx 0", m_instance_wl[idx].m_ifcName[0]);
          BCMWL_WLCTL_CMD(cmd);
      } else {
          snprintf(cmd, sizeof(cmd), "wlctl -i %s stbc_rx %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlStbcRx);
          BCMWL_WLCTL_CMD(cmd);
      }

      snprintf(cmd, sizeof(cmd), "wlctl -i %s stbc_tx %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlStbcTx);
      BCMWL_WLCTL_CMD(cmd);
   }
#endif

#ifdef SUPPORT_MIMO
   if (MIMO_PHY) {
      // n required, i.e. if 1, supports only 11n STA, 11g STA cannot not assoc
      if(strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF)) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s nreqd %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlNReqd);
         BCMWL_WLCTL_CMD(cmd);
      }     
   }      
#endif    
         
      //wlconf_aburn_ampdu_amsdu_set()
      /* First, clear WMM and afterburner settings to avoid conflicts */
      //val is afterburner_override now
      val = OFF;       
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wme 0 2>/dev/null", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);      
      snprintf(cmd, sizeof(cmd), "wlctl -i %s afterburner_override 0 2>/dev/null", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);      

      if(MIMO_PHY) {
         nmode = strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF);
      } 
          
      /* Set options based on capability */
      if(nmode) {
         if(m_instance_wl[idx].ampduSupported) {         
            snprintf(cmd, sizeof(cmd), "wlctl -i %s ampdu 1", m_instance_wl[idx].m_ifcName[0]);
            BCMWL_WLCTL_CMD(cmd);          	
         }
         if(m_instance_wl[idx].amsduSupported) {         
            snprintf(cmd, sizeof(cmd), "wlctl -i %s amsdu 1", m_instance_wl[idx].m_ifcName[0]);
            BCMWL_WLCTL_CMD(cmd);          	
         }
         if(m_instance_wl[idx].aburnSupported) {
            //allow ab in N mode. Do this last: may defeat ampdu et al */
            if (!strcmp(m_instance_wl[idx].m_wlVar.wlAfterBurnerEn, WL_OFF)) {
               snprintf(cmd, sizeof(cmd), "wlctl -i %s afterburner_override 0", m_instance_wl[idx].m_ifcName[0]);
               BCMWL_WLCTL_CMD(cmd);
            } else {
               snprintf(cmd, sizeof(cmd), "wlctl -i %s afterburner_override -1", m_instance_wl[idx].m_ifcName[0]);
               BCMWL_WLCTL_CMD(cmd);
               //Also turn off N reqd setting if ab is not OFF */
               snprintf(cmd, sizeof(cmd), "wlctl -i %s nreqd %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlNReqd);
               BCMWL_WLCTL_CMD(cmd);
            }
         }
      } else {      	
            // When N-mode is off or for non N-phy device, turn off AMPDU, AMSDU;
            //if WME is off, set the afterburner based on the configured nvram setting.      	
            snprintf(cmd, sizeof(cmd), "wlctl -i %s ampdu 0", m_instance_wl[idx].m_ifcName[0]);
            BCMWL_WLCTL_CMD(cmd);        
            snprintf(cmd, sizeof(cmd), "wlctl -i %s amsdu 0", m_instance_wl[idx].m_ifcName[0]);
            BCMWL_WLCTL_CMD(cmd);
        
            if(m_instance_wl[idx].aburnSupported) {
               if (!strcmp(m_instance_wl[idx].m_wlVar.wlAfterBurnerEn, WL_OFF))
                  val = OFF;
               else
                  val = AUTO_MODE;
            }
            
               if(m_instance_wl[idx].wmeSupported) {      
            	  if(m_instance_wl[idx].m_wlVar.wlWme!=OFF) {
                   // Can't have afterburner with WMM            	   
            	     val = OFF;            	   	
            	  }
            	  else {
            	     val = AUTO_MODE;	
            	  }                        	              	   
            	}
            	
            	snprintf(cmd, sizeof(cmd), "wlctl -i %s afterburner_override %d", m_instance_wl[idx].m_ifcName[0], val);
                BCMWL_WLCTL_CMD(cmd);            	
            }
        
        if (m_instance_wl[idx].wmeSupported) {
           if((m_instance_wl[idx].m_wlVar.wlWme!=OFF) && (val == OFF)) {
              //val is wme option now
            	   val = m_instance_wl[idx].m_wlVar.wlWme;
            	}            	            	
                snprintf(cmd, sizeof(cmd), "wlctl -i %s wme %d 2>/dev/null", m_instance_wl[idx].m_ifcName[0], val);
                BCMWL_WLCTL_CMD(cmd);
                snprintf(cmd, sizeof(cmd), "wlctl -i %s wme_noack %d 2>/dev/null", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlWmeNoAck);
                BCMWL_WLCTL_CMD(cmd);
                snprintf(cmd, sizeof(cmd), "wlctl -i %s wme_apsd %d 2>/dev/null", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlWmeApsd);
                BCMWL_WLCTL_CMD(cmd);
      
                //wmm advertisement
                if(m_instance_wl[idx].mbssSupported) {            	
                   for (i=0; i<m_instance_wl[idx].numBss; i++) {
      	              if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {
       	                 snprintf(cmd, sizeof(cmd), "wlctl -i %s wme_bss_disable %d", m_instance_wl[idx].m_ifcName[i], m_instance_wl[idx].m_wlMssidVar[i].wlDisableWme);
       	                 BCMWL_WLCTL_CMD(cmd);
       	              }    		
                    }
                 }       
                 // advanced WME AC parameter could be issued by additional wl commands               
           }

   if(m_instance_wl[idx].m_wlVar.wlBand == BAND_B) {
      if((MIMO_PHY)||
         !strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_B) ||
          ((!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_G)||!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_A)) &&
          (m_instance_wl[idx].m_wlVar.wlgMode == WL_MODE_B_ONLY || m_instance_wl[idx].m_wlVar.wlgMode == WL_MODE_G_AUTO))) {       	
         if ( strcmp(m_instance_wl[idx].m_wlVar.wlPreambleType, "long") == 0 )
            snprintf(cmd, sizeof(cmd), "wlctl -i %s plcphdr long", m_instance_wl[idx].m_ifcName[0]);
         else
            snprintf(cmd, sizeof(cmd), "wlctl -i %s plcphdr auto", m_instance_wl[idx].m_ifcName[0]);   	   	
      
         BCMWL_WLCTL_CMD(cmd); 
      }  
   }
         
   // rate
   val = m_instance_wl[idx].m_wlVar.wlRate; 

#ifdef SUPPORT_MIMO       
   if (MIMO_ENABLED) {     
      // -1 mcsidx used to designate AUTO rate if nmode is not off
      if (m_instance_wl[idx].m_wlVar.wlNMcsidx == -1)
         val = 0;
   }
#endif
   
   // rate: "rate" is being deprecated, adding a_rate and bg_rate
   if ( val  == 0 ) { /* auto rate */   
      snprintf(cmd, sizeof(cmd), "wlctl -i %s rate %d", m_instance_wl[idx].m_ifcName[0], val);
      BCMWL_WLCTL_CMD(cmd);
      if(m_instance_wl[idx].m_bands & BAND_A) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s a_rate %d", m_instance_wl[idx].m_ifcName[0], val);
         BCMWL_WLCTL_CMD(cmd);
      }
      if(m_instance_wl[idx].m_bands & BAND_B) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s bg_rate %d ", m_instance_wl[idx].m_ifcName[0], val);
         BCMWL_WLCTL_CMD(cmd);
      }
   } else {/* manual rate */
      snprintf(cmd, sizeof(cmd), "wlctl -i %s rate %3.1f ", m_instance_wl[idx].m_ifcName[0], (float)val / 1000000);
      BCMWL_WLCTL_CMD(cmd);
      if(m_instance_wl[idx].m_bands & BAND_A) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s a_rate %3.1f ", m_instance_wl[idx].m_ifcName[0], (float)val / 1000000);
         BCMWL_WLCTL_CMD(cmd);
      }
      if(m_instance_wl[idx].m_bands & BAND_B) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s bg_rate %3.1f ", m_instance_wl[idx].m_ifcName[0], (float)val / 1000000);
         BCMWL_WLCTL_CMD(cmd);
      }
   }      

#ifdef SUPPORT_MIMO      
   //For N-Phy, check if nrate needs to be applied */
   if (MIMO_PHY) {
      if(strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF)) {
         bool ismcs = (m_instance_wl[idx].m_wlVar.wlNMcsidx >= 0);   	   
         
         if (m_instance_wl[idx].m_wlVar.wlNMcsidx == 32 && m_instance_wl[idx].m_wlVar.wlNBwCap == 1) {         	
            m_instance_wl[idx].m_wlVar.wlNMcsidx = -1;
   	    ismcs = FALSE;
         }
         
         if(ismcs && (m_instance_wl[idx].m_wlVar.wlNMcsidx != 32)) {
            snprintf(cmd, sizeof(cmd), "wlctl -i %s nrate -m %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlNMcsidx );
            BCMWL_WLCTL_CMD(cmd);         
         }          
      }     
   }
#endif

   // multicast rate. "mrate" is being deprecated, adding bg_mrate and a_mrate
   if ( m_instance_wl[idx].m_wlVar.wlMCastRate == 0 ) {
      snprintf(cmd, sizeof(cmd), "wlctl -i %s mrate -1 ", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      if(m_instance_wl[idx].m_bands & BAND_A) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s a_mrate -1 ", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
      }
      if(m_instance_wl[idx].m_bands & BAND_B) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s bg_mrate -1 ", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);     	
      }      
   } else {
      snprintf(cmd, sizeof(cmd), "wlctl -i %s mrate %3.1f ", m_instance_wl[idx].m_ifcName[0], (float)m_instance_wl[idx].m_wlVar.wlMCastRate / 1000000);
      if(m_instance_wl[idx].m_bands & BAND_A) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s a_mrate %3.1f ", m_instance_wl[idx].m_ifcName[0], (float)m_instance_wl[idx].m_wlVar.wlMCastRate / 1000000);
         BCMWL_WLCTL_CMD(cmd);
      }
      if(m_instance_wl[idx].m_bands & BAND_B) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s bg_mrate %3.1f ", m_instance_wl[idx].m_ifcName[0], (float)m_instance_wl[idx].m_wlVar.wlMCastRate / 1000000);
         BCMWL_WLCTL_CMD(cmd);     	
      }       
   }
   
   // basic rate
   if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_ALL) == 0 ) {
      rateptr = rs_allbasic;
   } else {
      if (m_instance_wl[idx].m_wlVar.wlBand == BAND_B) { //options for 11b/g	
         if(strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_B) == 0) {  	
            if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_1_2) == 0 ) {
               rateptr = rs_11b_1_2;
            } 
         } else { //for all non-11b
            if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_1_2) == 0 ) {
               rateptr = rs_11g_1_2;
            } else if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_WIFI_2) == 0 ){
               rateptr = rs_11g_wifi_2;
            }
         }
      } else { //options of 11a
         if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_1_2) == 0 ) {
            rateptr = rs_11a_1_2;
         }
         else if ( strcmp(m_instance_wl[idx].m_wlVar.wlBasicRate, WL_BASIC_RATE_WIFI_2) == 0 ){
            rateptr = rs_11a_wifi_2;
         }                   	
     }
   }
   
   if(rateptr != rs_default) {
      snprintf(cmd, sizeof(cmd), "wlctl -i %s rateset %s", m_instance_wl[idx].m_ifcName[0], rateptr);
      BCMWL_WLCTL_CMD(cmd);
   }
    
   // rts
   snprintf(cmd, sizeof(cmd), "wlctl -i %s rtsthresh %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRtsThrshld);
   BCMWL_WLCTL_CMD(cmd);
   // fragmentation
   snprintf(cmd, sizeof(cmd), "wlctl -i %s fragthresh %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlFrgThrshld);
   BCMWL_WLCTL_CMD(cmd);
   // DTIM interval
   snprintf(cmd, sizeof(cmd), "wlctl -i %s dtim %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlDtmIntvl);
   BCMWL_WLCTL_CMD(cmd);
   // beacon interval
   snprintf(cmd, sizeof(cmd), "wlctl -i %s  bi %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlBcnIntvl);
   BCMWL_WLCTL_CMD(cmd);

   snprintf(cmd, sizeof(cmd), "wlctl -i %s  bcn_rotate 1", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(cmd);

   // framebursting mode
   if ( strcmp(m_instance_wl[idx].m_wlVar.wlFrameBurst, WL_OFF) == 0 )
      snprintf(cmd, sizeof(cmd), "wlctl -i %s frameburst 0", m_instance_wl[idx].m_ifcName[0]);
   else
      snprintf(cmd, sizeof(cmd), "wlctl -i %s frameburst 1", m_instance_wl[idx].m_ifcName[0]);
   BCMWL_WLCTL_CMD(cmd);

   // AP Isolation
   if(m_instance_wl[idx].mbssSupported) {            	
      for (i=0; i<m_instance_wl[idx].numBss; i++) {
      	if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {
       		snprintf(cmd, sizeof(cmd), "wlctl -i %s ap_isolate %d", m_instance_wl[idx].m_ifcName[i], m_instance_wl[idx].m_wlMssidVar[i].wlAPIsolation);
       		BCMWL_WLCTL_CMD(cmd);
       	}    		
      }
   } else {
      snprintf(cmd, sizeof(cmd), "wlctl -i %s ap_isolate %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlAPIsolation);
      BCMWL_WLCTL_CMD(cmd);
   }  
      

   // Tx Power Percentage
   /* Disable use of pwr_percent - JIRA 17789 */
   // snprintf(cmd, sizeof(cmd), "wlctl -i %s pwr_percent %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlTxPwrPcnt);
   // BCMWL_WLCTL_CMD(cmd);   
   
#ifdef WMF
   // Wireless multicast forwarding mode

   if(m_instance_wl[idx].wmfSupported != 0) {
      for (i = 0; i < m_instance_wl[idx].numBss; i++) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s wmf_bss_enable %d", m_instance_wl[idx].m_ifcName[i], m_instance_wl[idx].m_wlMssidVar[i].wlEnableWmf);
         BCMWL_WLCTL_CMD(cmd);
      }
   }
#endif

   // Universal Range Extension
   if (m_instance_wl[idx].apstaSupported != 0) {
      snprintf(cmd, sizeof(cmd), "wlctl -i %s apsta 0", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s ap 1", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s wet 0", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "wlctl -i %s mac_spoof 0", m_instance_wl[idx].m_ifcName[0]);
      BCMWL_WLCTL_CMD(cmd);
      snprintf(cmd, sizeof(cmd), "%s_mode", m_instance_wl[idx].m_ifcName[0]);
      nvram_set(cmd, "ap"); 

      if (m_instance_wl[idx].m_wlVar.wlEnableUre) {

         snprintf(cmd, sizeof(cmd), "wlctl -i %s apsta 1", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);

         if (m_instance_wl[idx].m_wlVar.wlEnableUre == 1) { /* bridge mode: Range Extender */
            snprintf(cmd, sizeof(cmd), "wlctl -i %s wet 1", m_instance_wl[idx].m_ifcName[0]);
            BCMWL_WLCTL_CMD(cmd);
            snprintf(cmd, sizeof(cmd), "%s_mode", m_instance_wl[idx].m_ifcName[0]); /* wl0 needs to be wet */
            nvram_set(cmd, "wet"); 
         } else if (m_instance_wl[idx].m_wlVar.wlEnableUre == 2) { /* routed mode: Travel Router */
            snprintf(cmd, sizeof(cmd), "%s_mode", m_instance_wl[idx].m_ifcName[0]);
            nvram_set(cmd, "sta"); 
         }

         for (i=1; i<m_instance_wl[idx].numBss; i++) { /* guest SSIDs need to be ap mode */
            snprintf(cmd, sizeof(cmd), "%s_mode", m_instance_wl[idx].m_ifcName[i]);
            nvram_set(cmd, "ap"); 
         }
#ifdef SUPPORT_WSC
         nvram_set("wps_pbc_apsta", "enabled");
         nvram_set("ure_disable", "0");
#endif
      } else {
         if (!strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET) || !strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET_MACSPOOF)) {
             // Wireless Ethernet / MAC Spoof
             snprintf(cmd, sizeof(cmd), "wlctl -i %s ap 0", m_instance_wl[idx].m_ifcName[0]);
             BCMWL_WLCTL_CMD(cmd);
             snprintf(cmd, sizeof(cmd), "wlctl -i %s wet 1", m_instance_wl[idx].m_ifcName[0]);
             BCMWL_WLCTL_CMD(cmd);
             snprintf(cmd, sizeof(cmd), "%s_mode", m_instance_wl[idx].m_ifcName[0]);
             nvram_set(cmd, "wet");
             if (!strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET_MACSPOOF))
             {
                snprintf(cmd, sizeof(cmd), "wlctl -i %s mac_spoof 1", m_instance_wl[idx].m_ifcName[0]);
                BCMWL_WLCTL_CMD(cmd);
             }
         }
      }
      snprintf(cmd, sizeof(cmd), "wlctl -i %s sta_retry_time %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlStaRetryTime);
      BCMWL_WLCTL_CMD(cmd);
   } 


#ifdef DUCATI
   if(m_instance_wl[idx].wlVecSupported != 0) {
         snprintf(cmd, sizeof(cmd), "wlctl -i %s iperf %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].wlIperf);
         BCMWL_WLCTL_CMD(cmd);
         snprintf(cmd, sizeof(cmd), "wlctl -i %s vec %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].wlVec);
         BCMWL_WLCTL_CMD(cmd);

   }
#endif

   snprintf(cmd, sizeof(cmd), "wlctl -i %s chanim_mode %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlChanImEnab);
   BCMWL_WLCTL_CMD(cmd);
 

#ifdef SUPPORT_MIMO
   if (MIMO_PHY) {

      /* When any sta associates with AP, stop Rxchain power save */ 
      snprintf(cmd, sizeof(cmd), "wlctl -i %s rxchain_pwrsave_stas_assoc_check %d", m_instance_wl[idx].m_ifcName[0], 1);
      BCMWL_WLCTL_CMD(cmd);

      /* Rxchain power save */
      snprintf(cmd, sizeof(cmd), "wlctl -i %s rxchain_pwrsave_enable %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveEnable);
      BCMWL_WLCTL_CMD(cmd);
	  
	  snprintf(cmd, sizeof(cmd), "wlctl -i %s rxchain_pwrsave_quiet_time %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveQuietTime);
      BCMWL_WLCTL_CMD(cmd);

      snprintf(cmd, sizeof(cmd), "wlctl -i %s rxchain_pwrsave_pps %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRxChainPwrSavePps);
      BCMWL_WLCTL_CMD(cmd);

      /* Radio power save */
      if ((strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_N) == 0 ) || (strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_AC) == 0 ))
      {
         if ( m_instance_wl[idx].m_wlVar.wlRadioPwrSaveEnable == 1 )
         {
            snprintf(cmd, sizeof(cmd), "wl spect 1");
            BCMWL_WLCTL_CMD(cmd); 
         }
      }
	  
      snprintf(cmd, sizeof(cmd), "wlctl -i %s radio_pwrsave_enable %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRadioPwrSaveEnable);
      BCMWL_WLCTL_CMD(cmd);
	  
      snprintf(cmd, sizeof(cmd), "wlctl -i %s radio_pwrsave_quiet_time %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRadioPwrSaveQuietTime);
      BCMWL_WLCTL_CMD(cmd);

      snprintf(cmd, sizeof(cmd), "wlctl -i %s radio_pwrsave_pps %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRadioPwrSavePps);
      BCMWL_WLCTL_CMD(cmd);

      snprintf(cmd, sizeof(cmd), "wlctl -i %s radio_pwrsave_level %d", m_instance_wl[idx].m_ifcName[0], m_instance_wl[idx].m_wlVar.wlRadioPwrSaveLevel);
      BCMWL_WLCTL_CMD(cmd);
   }
#endif

   if(idx == 1 && m_instance_wl[idx].wlMediaSupported != 0) {

         /* Enable frameburst */
         strncpy(m_instance_wl[idx].m_wlVar.wlFrameBurst, WL_ON, sizeof(m_instance_wl[idx].m_wlVar.wlFrameBurst)); 

         /* Enable wmf_bss_enable */
         for (i = 0; i < m_instance_wl[idx].numBss; i++) {
            m_instance_wl[idx].m_wlMssidVar[i].wlEnableWmf = ON;
         }
		 /*
         snprintf(cmd, sizeof(cmd), "wlctl -i %s wme_remap_tcp 1", m_instance_wl[idx].m_ifcName[0]);
         BCMWL_WLCTL_CMD(cmd);
		 */
   }

   /* 
    * raise priority for wlan processing thread to boost performance
    */
   wlmngr_setpriority(idx);

}
#endif /* WLCONF */
/* Save nvram data to MDM */
void wlmngr_write_nvram(void)
{
	char *str = malloc(MAX_NVRAM_SPACE*sizeof(char));
	char *buf = malloc(MAX_NVRAM_SPACE*sizeof(char));
	char *name;
#ifdef WL_WLMNGR_DBG  
	int size;
#endif
	char pair[WL_LG_SIZE_MAX+WL_SIZE_132_MAX];
	char format[]="%0?x%s%d";
	*str = '\0';

	sprintf(format, "%s%d%s", "%0", DEFAULT_PAIR_LEN_BYTE, "x%s%d");// format set to "%02x%s%d"
    snprintf(pair, sizeof(pair), format, strlen("pair_len_byte=?"), "pair_len_byte=", PAIR_LEN_BYTE); 
	strcat(str, pair);
	sprintf(format,"%s%d%s","%0", PAIR_LEN_BYTE, "X%s");

	nvram_getall(buf, MAX_NVRAM_SPACE*sizeof(char));
	for (name = buf; *name && (strlen(str)<MAX_NVRAM_SPACE); name += strlen(name) + 1) {
	    if(!strncmp(name,"wps_force_restart", strlen("wps_force_restart"))
	        || !strncmp(name, "pair_len_byte=", strlen("pair_len_byte="))
	        || !strncmp(name, "wl_unit", strlen("wl_unit")))
		        continue;
#if defined(WL_IMPL_PLUS) || !defined(HSPOT_SUPPORT)
            if(! wlcsm_hspot_var_isdefault(name)) {
#endif
		snprintf(pair, sizeof(pair), format, strlen(name), name);
		
		strcat(str, pair);
#ifdef WL_WLMNGR_DBG   
		printf("%s", name);
#endif
#if defined(WL_IMPL_PLUS) || !defined(HSPOT_SUPPORT)
	}
#endif
	}

#ifdef WL_WLMNGR_DBG 
	size = strlen(str);  
	printf("size: %d bytes (%d left)\n", size, MAX_NVRAM_SPACE - size);
#endif
	WLCSM_TRACE(WLCSM_TRACE_DBG," size:0x%08x,and left:%d\r\n",strlen(str),MAX_NVRAM_SPACE-strlen(str) );
	
	wlLockWriteNvram(str);
	free(buf);
	free(str);
	return;
}

/* get the default usb mount directoy name */
char * wlmngr_get_nvramdefaultfile() {

	struct mntent *ent;
	FILE *aFile;

	aFile = setmntent("/proc/mounts", "r");
	if (aFile == NULL) {
		perror("setmntent");
		exit(1);
	}
	while (NULL != (ent = getmntent(aFile))) {
		if(!strncmp(ent->mnt_fsname,"/dev/sda",8))
		return ent->mnt_dir;
	}
	endmntent(aFile);
	return NULL;
}

/* get nvram default value and write it to nvram */
void wlmngr_write_nvram_default(char *tftpip)
{
	char *name, *buf;
	char pair[WL_SIZE_132_MAX];
	FILE *nvramfile=NULL;
	char  *nvramconfdir;
	if(tftpip)		
		nvramconfdir="/var";		
	else
		nvramconfdir= wlmngr_get_nvramdefaultfile() ;

	if(nvramconfdir)
	{
		snprintf(pair, sizeof(pair), "%s/%s",nvramconfdir,"nvramdefault.txt");
		nvramfile=fopen(pair,"w");
	}
	else
		nvramfile=fopen("/mnt/disk1_1/nvramdefault.txt","w");

	if(!nvramfile) {
		fprintf(stderr,"open file error\r\n");
		exit(-1);
	}
	buf= malloc(MAX_NVRAM_SPACE);
	if(!buf) {
		fclose(nvramfile);
		fprintf(stderr,"Could no alloc enough memory\r\n");
		exit(-1);
	}
	memset(buf,0,MAX_NVRAM_SPACE);
	nvram_getall(buf, MAX_NVRAM_SPACE);
	for (name = buf; *name; name += strlen(name) + 1) {
		fprintf(nvramfile,"%s\n",name);
	}
	fclose(nvramfile);
	if(tftpip) 
	{
	  snprintf(pair, sizeof(pair), "cd /var;tftp -p -l nvramdefault.txt %s",tftpip);
	  bcmSystem(pair);
	}
	free(buf);
	return;
}


#ifdef SUPPORT_WSC
#define BOARD_DEVICE_NAME  "/dev/brcmboard"


/*check cfm device pin*/
int  wlmngr_ReadCfeDevPin ( char *pinPtr )
{
    int boardFd = 0;
    int ret=-1, rc=0, i;
    
    char pin[12]= {0};
	
    unsigned int offset =  ((size_t) &((NVRAM_DATA *)0)->wpsDevicePin);
	
    BOARD_IOCTL_PARMS ioctlParms;

    boardFd = open(BOARD_DEVICE_NAME, O_RDWR);

    if ( boardFd != -1 )
    {
        ioctlParms.string = pin;
        ioctlParms.strLen = 8;
        ioctlParms.offset = offset;
        ioctlParms.action = NVRAM;
        ioctlParms.buf    = NULL;
        ioctlParms.result = -1;

        rc = ioctl(boardFd, BOARD_IOCTL_FLASH_READ, &ioctlParms);
		

           if (rc < 0)
           {
              printf("%s@%d Read WPS Pin Failure\n", __FUNCTION__, __LINE__);
           }
	    else {
			pin[8] = '\0';
			strcpy(pinPtr, pin);
			ret = 0;
			for ( i=0; i<8; i++ ) {
				if ( (pin[i]  < (char)0x30 ) ||  (pin[i]  > (char)0x39 ) ) {
					printf("There is no Predefined DevicePin in CFE\n");
					ret = 1;
				       break;
				}
			}
			
	    }    
    }
    else
    {
       printf("Unable to open device %s", BOARD_DEVICE_NAME);
    }

    close(boardFd);
    return ret;
}

  
int wlmngr_wscPincheck(char *pin_string)
{
    unsigned long PIN = strtoul(pin_string, NULL, 10 );;
    unsigned long int accum = 0;
    unsigned int len = strlen(pin_string);

#ifdef WL_WLMNGR_DBG
    printf("pin_string=%s PIN=%lu\n", pin_string, PIN );
#endif

    if (len != 4 && len != 8)
        return 	-1;

    if (len == 8) {
        accum += 3 * ((PIN / 10000000) % 10);
        accum += 1 * ((PIN / 1000000) % 10);
        accum += 3 * ((PIN / 100000) % 10);
        accum += 1 * ((PIN / 10000) % 10);
        accum += 3 * ((PIN / 1000) % 10);
        accum += 1 * ((PIN / 100) % 10);
        accum += 3 * ((PIN / 10) % 10);
        accum += 1 * ((PIN / 1) % 10);

#ifdef WL_WLMNGR_DBG
       printf("accum=%lu\n", accum );
#endif
        if (0 == (accum % 10))
            return 0;
    }
    else if (len == 4)
        return 0;

    return -1;
}

int  wl_wscPinGet( void )
{
    char devPwd[32] ={0};

    if ( wlmngr_ReadCfeDevPin( devPwd ) ==  0 )
        if( wlmngr_wscPincheck(devPwd) == 0){
	    printf("WPS Device PIN = %s\n", devPwd);
	    nvram_set("wps_device_pin", devPwd);
	    return 0;
	}
    return -1;
}



//**************************************************************************
// Function Name: setupAll
// Description  : all setup for wireless interface.
// Parameters   : none.
// Returns      : None.
//**************************************************************************
int wlmngr_get_random_bytes(char *rand, int len)
{
	int dev_random_fd;
	dev_random_fd = open("/dev/urandom", O_RDONLY|O_NONBLOCK);
	if ( dev_random_fd < 0 ) {
		printf("Could not open /dev/urandom\n");
	       return WSC_FAILURE;
	}
		   
	read(dev_random_fd, rand, len);
	close(dev_random_fd);
	return WSC_SUCCESS;
}

/*This function is used to generate the SSID and PSK key */
/*when WSC is enabled and unconfig mode*/
void  wlmngr_genWscRandSssidPsk( int idx )
{
   char  *wscmode, *wscconfig;
#ifdef WL_WLMNGR_DBG
   char *irmode;
#endif
   unsigned short key_length;
   char dev_ssid[WSC_MAX_SSID_LEN]={0}, ssid_rand[WSC_RANDOM_SSID_TAIL_LEN+1]={0};
   char random_key[64] = {0};
   int i = 0;

   int unit =0, subunit =0;   

   char wl_unit[4]={0};

   char mode_str[32]={0}, config_str[32]={0}, ir_str[32]={0};
   char ssid_str[32]={0}, wpa_psk_str[64]={0}, akm_str[10]={0};

   
   /* wl_unit takes format: x.x or x */
   strncpy(wl_unit, nvram_safe_get("wl_unit"), 3);
   
   if ( strlen(wl_unit) == 0)   {
        nvram_set("wl_unit", "0");
	 strcpy(wl_unit, "0");
        unit = 0;
        subunit = 0;
    }
    else
    {
        unit     = isdigit(wl_unit[0])? (wl_unit[0]-0x30):0;
        if ( strlen(wl_unit) >=3 )
		subunit = isdigit(wl_unit[2]) ? (wl_unit[2]-0x30):0;
    }

    /* unit should be same as idx */
#ifdef WL_WLMNGR_DBG
   printf("%s@%d  wl_unit=%s unit=%d subunit=%d idx=%d\n", __FUNCTION__, __LINE__, wl_unit, unit, subunit, idx );	
#endif

   snprintf(mode_str, sizeof(mode_str), "wl%s_wps_mode", wl_unit );
   snprintf(config_str, sizeof(config_str), "wl%s_wps_config_state", wl_unit );
   snprintf(ir_str, sizeof(ir_str), "wl_wps_reg");

   if (unit == 0) {
         snprintf(config_str, sizeof(config_str), "lan_wps_oob");
         snprintf(ir_str, sizeof(ir_str), "lan_wps_reg");
   }
   else {
         snprintf(config_str, sizeof(config_str), "lan%s_wps_oob", wl_unit );
         snprintf(ir_str, sizeof(ir_str), "lan%s_wps_reg", wl_unit);
   }

#ifdef WL_WLMNGR_DBG
   printf("mode_str=%s\n", mode_str );
   printf("config_str=%s\n", config_str );
   printf("ir_str=%s\n", ir_str );
#endif

    /*generate SSID and key*/
    wscmode = (char *)nvram_safe_get(mode_str);
#ifdef WL_WLMNGR_DBG
    irmode = (char *)nvram_safe_get(ir_str);
#endif
    wscconfig = (char *)nvram_safe_get(config_str);

#ifdef WL_WLMNGR_DBG
    printf("NVRAM: %s=[%s] %s=[%s] %s=[%s]\n",  mode_str, wscmode, ir_str, irmode, config_str, wscconfig );
#endif

    if ( !strcmp( wscmode, "enabled") &&  !strcmp(wscconfig,"0")  ) 
    {
        /* wsc enabled + unconfig mode*/
		/*generate Random PSK key and SSID*/			
		 if ( wlmngr_get_random_bytes((char *)&key_length, sizeof(key_length)) == WSC_SUCCESS) 
		 {
			key_length = ((((long)key_length + 56791 )*13579)%8) + 8;
			while (i < key_length) {
				if ( wlmngr_get_random_bytes( (random_key+i), 1) == WSC_SUCCESS ) {
				      if ((islower(random_key[i]) || isdigit(random_key[i])) && (random_key[i] < 0x7f)) {
					    i++;
				      	}
				}
			}
#ifdef WL_WLMNGR_DBG
			printf("random_key=%s\n", random_key); 
#endif
			/*Generate Random SSID patch*/
			i=0; 
			while (i < WSC_RANDOM_SSID_TAIL_LEN) {
				if ( wlmngr_get_random_bytes((ssid_rand+i), 1) == WSC_SUCCESS ) {
				      if ( isdigit(ssid_rand[i])) {
					    i++;
				      	}
				}
			}
			ssid_rand[WSC_RANDOM_SSID_TAIL_LEN] = '\0';

			snprintf(dev_ssid, sizeof(dev_ssid), "%s%s", WL_DEFAULT_SSID, ssid_rand );
#ifdef WL_WLMNGR_DBG
			printf("Random ssid=%s\n", dev_ssid );
#endif		
			strncpy(m_instance_wl[unit].m_wlMssidVar[subunit].wlSsid, dev_ssid, sizeof(m_instance_wl[unit].m_wlMssidVar[subunit].wlSsid));
			snprintf(ssid_str, sizeof(ssid_str), "wl%s_ssid", wl_unit );
			nvram_set(ssid_str, dev_ssid);
	   
			strncpy(m_instance_wl[unit].m_wlMssidVar[subunit].wlWpaPsk, random_key, sizeof(m_instance_wl[unit].m_wlMssidVar[subunit].wlWpaPsk));
			snprintf(wpa_psk_str, sizeof(wpa_psk_str), "wl%s_wpa_psk", wl_unit );
			nvram_set(wpa_psk_str, random_key);
	
			strncpy(m_instance_wl[unit].m_wlMssidVar[subunit].wlAuthMode,"psk", sizeof(m_instance_wl[unit].m_wlMssidVar[subunit].wlAuthMode));
			snprintf(akm_str, sizeof(akm_str), "wl%s_akm", wl_unit );
			nvram_set(akm_str, "psk");

#ifdef WL_WLMNGR_DBG
			printf("%s@%d nvram: %s=[%s] %s=[%s] %s=[%s]\n",  __FUNCTION__, __LINE__, 
				ssid_str, dev_ssid, wpa_psk_str, random_key, akm_str, "psk");
#endif		

			/* Data Data to MDM*/
			wlLockWriteMdmOne(idx);

		 }
    }

}

static int
write_to_wps(int fd, char *cmd)
{
	int n;
	int len;
	struct sockaddr_in to;

	len = strlen(cmd)+1;

	/* open loopback socket to communicate with wps */
	memset(&to, 0, sizeof(to));
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	to.sin_family = AF_INET;
	to.sin_port = htons(WPS_UI_PORT);

	n = sendto(fd, cmd, len, 0, (struct sockaddr *)&to,
		sizeof(struct sockaddr_in));



	/* Sleep 100 ms to make sure
	   WPS have received socket */
	sleep(1); // USLEEP(100*1000);
	return n;
}

static int
read_from_wps(int fd, char *databuf, int datalen)
{
	int n, max_fd = -1;
	fd_set fdvar;
	struct timeval timeout;
	int recvBytes;
	struct sockaddr_in addr;
	socklen_t size = sizeof(struct sockaddr);

	timeout.tv_sec = 2;
	timeout.tv_usec = 0;

	FD_ZERO(&fdvar);

	/* get ui fd */
	if (fd >= 0) {
		FD_SET(fd, &fdvar);
		max_fd = fd;
	}

	if (max_fd == -1) {
		fprintf(stderr, "wps ui utility: no fd set!\n");
		return -1;
	}

	n = select(max_fd + 1, &fdvar, NULL, NULL, &timeout);

	if (n <= 0) {
		return n;
	}

	if (n > 0) {
		if (fd >= 0) {
			if (FD_ISSET(fd, &fdvar)) {
				recvBytes = recvfrom(fd, databuf, datalen,
					0, (struct sockaddr *)&addr, &size);

				if (recvBytes == -1) {
					fprintf(stderr, 
					"wps ui utility:recv failed, recvBytes = %d\n", recvBytes);
					return -1;
				}
				return recvBytes;
			}
			
			return -1;
		}
	}

	return -1;
}

int
parse_wps_env(char *buf)
{
	char *argv[32] = {0};
	char *item, *p, *next;
	char *name, *value;
	int i;
        /*
	int unit, subunit;
	char nvifname[IFNAMSIZ];
        */

	/* Seperate buf into argv[] */
	for (i = 0, item = buf, p = item;
		item && item[0];
		item = next, p = 0, i++) {
		/* Get next token */
		strtok_r(p, " ", &next);
		argv[i] = item;
	}

	/* Parse message */
	wps_config_command = 0;
	wps_action = 0;
	wps_method = 0; /* Add in PF #3 */
	memset(wps_autho_sta_mac, 0, sizeof(wps_autho_sta_mac)); /* Add in PF #3 */

	for (i = 0; argv[i]; i++) {
		value = argv[i];
		name = strsep(&value, "=");
		if (name) {
			if (!strcmp(name, "wps_config_command"))
				wps_config_command = atoi(value);
			else if (!strcmp(name, "wps_action"))
				wps_action = atoi(value);
			else if (!strcmp(name, "wps_uuid"))
				strncpy(wps_uuid ,value, sizeof(wps_uuid));
			else if (!strcmp(name, "wps_method")) /* Add in PF #3 */
				wps_method = atoi(value);
			else if (!strcmp(name, "wps_autho_sta_mac")) /* Add in PF #3 */
				memcpy(wps_autho_sta_mac, value, sizeof(wps_autho_sta_mac));

#if 0
/* Comment out to avoid link wlbcmshare.so and wlbcmcrypto.so, to avoid https using openSSL crashes */
			else if (!strcmp(name, "wps_ifname")) {
				if (strlen(value) <=0)
					continue;
				if (osifname_to_nvifname(value, nvifname, sizeof(nvifname)) != 0) 
					continue;

				if (get_ifname_unit(nvifname, &unit, &subunit) == -1)
					continue;
				if (unit < 0)
					unit = 0;
				if (subunit < 0)
					subunit = 0;

				if (subunit)
					snprintf(wps_unit, sizeof(wps_unit), "%d.%d", unit, subunit);
				else
					snprintf(wps_unit, sizeof(wps_unit), "%d", unit);
			}
#endif
		}
	}

	return 0;
}

int
get_wps_env()
{
	int fd = -1;
	char databuf[256];
	int datalen = sizeof(databuf);
	int count = 0, msg;
	if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		fprintf(stderr, "wps ui utility: failed to open loopback socket\n");
		return -1;
	}

	while(1){
	        write_to_wps(fd, "GET");		
		msg = read_from_wps(fd, databuf, datalen); 
		/* Receive response */
		if (msg == 0 && ++count <= 3) //in case timeout, try 3 times
			sleep(1);
		else{
		        if (msg > 0)
		                parse_wps_env(databuf);
			else if(msg < 0)
		                /* Show error message ? */
			        fprintf(stderr,  "read_from_wps() failure\n");
			break;
		}
	}

	if(msg == 0)
	        fprintf(stderr, "read_from_wps() timeout\n");

	close(fd);
	return 0;
}


int
set_wps_env(char *uibuf)
{
	int wps_fd = -1;
	struct sockaddr_in to;
	int sentBytes = 0;
	uint32 uilen = strlen(uibuf);

	if ((wps_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {

		goto exit;
	}
	
	/* send to WPS */
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	to.sin_family = AF_INET;
	to.sin_port = htons(WPS_UI_PORT);

	sentBytes = sendto(wps_fd, uibuf, uilen, 0, (struct sockaddr *) &to,
		sizeof(struct sockaddr_in));

	if (sentBytes != uilen) {
		goto exit;
	}

	/* Sleep 100 ms to make sure
	   WPS have received socket */
	sleep(1); // USLEEP(100*1000);
	return 0;

exit:
	if (wps_fd > 0)
		close(wps_fd);

	/* Show error message ?  */
	return -1;
}

bool wlmngr_isEnrSta(int idx)
{
   int intf = 0; /* APSTA-STA's intface is always 0 */
   int ret = TRUE;
    
   if (!strcmp(m_instance_wl[idx].m_wlMssidVar[intf].wsc_mode, "disabled") || 
       ((m_instance_wl[idx].m_wlVar.wlEnableUre == 0 ) && strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET)) ||
       (m_instance_wl[idx].m_wlVar.wlEnableUre && (m_instance_wl[idx].m_wlVar.wlSsidIdx == 1)))
       ret = FALSE;

   return ret;
}

bool wlmngr_isEnrAP(int idx)
{
   int intf = 1; /* APSTA-AP's intface is always 1 */
   int ret = FALSE;
    

   if (strcmp(m_instance_wl[idx].m_wlMssidVar[intf].wsc_mode, "disabled") &&  
       (m_instance_wl[idx].m_wlVar.wlEnableUre && (m_instance_wl[idx].m_wlVar.wlSsidIdx == 1)))
       ret = TRUE;

   return ret;
}

void wlmngr_saveApInfoFromScanResult(int apListIdx)
{
   int i=0;
   char mac[WL_MID_SIZE_MAX];
   char ssid[WL_SSID_SIZE_MAX];
   void *node = NULL;
   int privacy=0;
   int wpsConfigured=0;

   node = BcmWl_getScanParam(node, mac, ssid, &privacy, &wpsConfigured);

   while ( node != NULL ) {
      if (apListIdx == i)	{
         nvram_set("wps_enr_ssid", (char *)ssid);
         nvram_set("wps_enr_bssid", (char *)mac);
         
         if(privacy == 0)
            nvram_set("wps_enr_wsec", "0");
         else         
            nvram_set("wps_enr_wsec", "16");

         if(wpsConfigured == 1)
            nvram_set("wps_ap_configured", "configured");
         else         
            nvram_set("wps_ap_configured", "unconfigured");

         break;
      }          
      i++;

      node = BcmWl_getScanParam(node, mac, ssid, &privacy, &wpsConfigured);
   }
}

void wlmngr_doWpsApScan(void)
{
   // scan AP
   BcmWl_ScanWdsMacStart();
   /* Scan wps AP */
   BcmWl_ScanMacResult(SCAN_WPS);
}

void wlmngr_printScanResult(int idx, char *text)
{
   //extern int wps_enr_auto;
   int i=0, wsize;
   char *prtloc = text;
   char mac[WL_MID_SIZE_MAX];
   char ssid[WL_SSID_SIZE_MAX];
   void *node = NULL;

   /* Only for wps enr mode */
   if (!wlmngr_isEnrSta(idx))  
      return;

   // write table header
   sprintf(prtloc, "<div id=\"divWscAplist\"> \n%n", &wsize);
   prtloc +=wsize;
   sprintf(prtloc, "<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;List <b>WPS AP</b><br> \n%n", &wsize);
   prtloc +=wsize;
   sprintf(prtloc, "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\"> \n%n", &wsize);
   prtloc +=wsize;
   sprintf(prtloc, "<tr> \n%n", &wsize);
   prtloc +=wsize;
   sprintf(prtloc, "<td width='180'></td> \n%n", &wsize);
   prtloc +=wsize;
   sprintf(prtloc, "<td> \n%n", &wsize);
   prtloc +=wsize;

   sprintf(prtloc, "<select name='wlWscAplist'> \n%n", &wsize);
   prtloc +=wsize;

   // write table body
   node = BcmWl_getScanWdsMacSSID(node, mac, ssid);

   if (node == NULL) {
      sprintf(prtloc, "<option value=\"-1\" selected>None</option> \n%n", &wsize);
      prtloc +=wsize;
   }

   while ( node != NULL ) {
      sprintf(prtloc, "<option value=\"%d\" %s>%s (%s)</option> \n%n", i, (i==0)?"selected":"", ssid, mac, &wsize);
      prtloc +=wsize;
      i++;

      node = BcmWl_getScanWdsMacSSID(node, mac, ssid);
   }

   sprintf(prtloc, "</select> \n%n", &wsize);
   prtloc +=wsize;

   sprintf(prtloc, "<input type='button' name='wps_scan' onclick='doScan()' value='Rescan'> \n%n", &wsize);
   prtloc +=wsize;

   sprintf(prtloc, "</td></tr> \n%n", &wsize);
   prtloc +=wsize;
   sprintf(prtloc, "</table> \n%n", &wsize);
   prtloc +=wsize;
   sprintf(prtloc, "</div> \n%n", &wsize);
   prtloc +=wsize;

   *prtloc = 0;
}

#endif /* SUPPORT_WSC */

#define CHIPINFO_DEVICE_NAME  "/dev/chipinfo"

/*read STBC value from chipinfo*/
int wlmngr_ReadChipInfoStbc(void)
{
    int chipinfoFd;
    int rc;
    
    CHIPINFO_IOCTL_PARMS ioctlParms;
    ioctlParms.result = -1;

    chipinfoFd = open(CHIPINFO_DEVICE_NAME, O_RDWR);
    if ( chipinfoFd != -1 )
    {
        ioctlParms.action = CAN_STBC;
        rc = ioctl(chipinfoFd, CHIPINFO_IOCTL_GET_CHIP_CAPABILITY, &ioctlParms);
        if (rc < 0)
        {
            printf("%s@%d Read CHIP INFO Failure\n", __FUNCTION__, __LINE__);
        } 
    }
    else
    {
       printf("Unable to open device %s", CHIPINFO_DEVICE_NAME);
    }

    close(chipinfoFd);
	
    /* possible return values: 1 - STBC supported, 0 - STBC not supported, -EINVAL (-22)/or (-1) - no STBC in chip info */
	
    return ioctlParms.result;
}

//end of File
