/*
* <:copyright-BRCM:2011:proprietary:standard
* 
*    Copyright (c) 2011 Broadcom 
*    All Rights Reserved
* 
*  This program is the proprietary software of Broadcom and/or its
*  licensors, and may only be used, duplicated, modified or distributed pursuant
*  to the terms and conditions of a separate, written license agreement executed
*  between you and Broadcom (an "Authorized License").  Except as set forth in
*  an Authorized License, Broadcom grants no license (express or implied), right
*  to use, or waiver of any kind with respect to the Software, and Broadcom
*  expressly reserves all rights in and to the Software and all intellectual
*  property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
*  NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
*  BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
* 
*  Except as expressly set forth in the Authorized License,
* 
*  1. This program, including its structure, sequence and organization,
*     constitutes the valuable trade secrets of Broadcom, and you shall use
*     all reasonable efforts to protect the confidentiality thereof, and to
*     use this information only in connection with your use of Broadcom
*     integrated circuit products.
* 
*  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*     AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*     WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
*     RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
*     ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
*     FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
*     COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
*     TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
*     PERFORMANCE OF THE SOFTWARE.
* 
*  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
*     ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
*     INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
*     WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
*     IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
*     OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
*     SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
*     SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
*     LIMITED REMEDY.
:>
*/

/*
* Wlmngr  is using 'wlconf' in dev branch currently. Wlconf uses ioctl  
* function instead of 'wlctl' shell command to configure Wlan drvier.  
* Wlconf codes are under flag 'WLCONF' in userspace currently.  
* You can change it to old configuration way by removing 
* 'EXT_WLCONF'=y in wlconfig_lx_wl_dslcpe. 
*/

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cms.h"
#include "board.h"

#include "cms_util.h"
#include "mdm.h"
#include "mdm_private.h"
#include "odl.h"
#include "cms_boardcmds.h"
#include "cms_boardioctl.h"
#include "cms_core.h"

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

#include "wllist.h"
#include "wldsltr.h"

#ifdef SUPPORT_WSC
#include <time.h>
#endif

#ifdef DSLCPE_1905
#include <wps_1905.h>
#endif

#include<bcmconfig.h>

int g_wlmngr_restart_all=0;
// #define WL_WLMNGR_DBG

#ifdef SUPPORT_SES
#ifndef SUPPORT_NVRAM
#error BUILD_SES depends on BUILD_NVRAM
#endif
#endif

int wl_visal_start_cnt=0;
#define IFC_BRIDGE_NAME	 "br0"

#define WL_PREFIX "_wl"

WLAN_ADAPTER_STRUCT *m_instance_wl; //[IFC_WLAN_MAX]; /*Need to Optimise to Dynamic buffer allocation*/
extern int wl_cnt;

bool cur_mbss_on = FALSE; /* current mbss status, off by default at init */
int act_wl_cnt = 0; /* total enabled wireless adapters */
int is_smp_system = 0;  /* set to 1 if we are on SMP system */

#ifdef SUPPORT_WSC
int wps_config_command;
int wps_action;
char wps_uuid[36];
char wps_unit[32];
int wps_method;
char wps_autho_sta_mac[sizeof("00:00:00:00:00:00")];
int wps_enr_auto = 0;
#define MAX_BR_NUM	8
#endif

#define WLHSPOT_NOT_SUPPORTED 2
/* These functions are to be used only within this file */
static void wlmngr_getVarFromNvram(void *var, const char *name, const char *type);
static void wlmngr_setVarToNvram(void *var, const char *name, const char *type);
static void wlmngr_NvramMapping(int idx, const int dir);
static bool wlmngr_detectAcsd(void);
static void set_wps_config_method_default(void);
void wlmngr_enum_ifnames(void);

void wlmngr_issueServiceCmd(int idx);
int get_wps_env();
int set_wps_env(char *uibuf);

/*Dynamic Allocate adapter buffers*/
int wlmngr_alloc( int adapter_cnt ) 
{
   if ( (m_instance_wl = malloc( sizeof(WLAN_ADAPTER_STRUCT) * adapter_cnt )) == NULL ) {
    printf("%s@%d alloc m_instance_wl[size %d] Err \n", __FUNCTION__, __LINE__, 
        sizeof(WLAN_ADAPTER_STRUCT) * adapter_cnt);
    return -1;
   }

   /* Zero the buffers */
   memset( m_instance_wl, 0, sizeof(WLAN_ADAPTER_STRUCT) * adapter_cnt );
   return 0;
}

void wlmngr_free(void ) 
{
   if ( m_instance_wl != NULL )
    free(m_instance_wl);
}

void nmode_no_tkip(int idx)
{
    int i;

    if ( !strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF)  || 
		(strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_N) && strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_AC)))
        return;
   
    for (i = 0; i < WL_NUM_SSID; i++)
    {
        if (strcmp(m_instance_wl[idx].m_wlMssidVar[i].wlWpa, "tkip") == 0)
            strncpy(m_instance_wl[idx].m_wlMssidVar[i].wlWpa, "tkip+aes", sizeof(m_instance_wl[idx].m_wlMssidVar[i].wlWpa));
    }
    return;
}

/*Do basic init for buffer clean and data-structure sync*/
void wlmngr_initCfg ( unsigned char dataSync, int idx )
{
	char need_save = 0;
	char need_nvram_save = 0;
	int cnt;
#ifdef DSLCPE_1905
	int bssid;
	int credential_changed;
	int conf_status;
	if(dataSync!=WL_SYNC_FROM_MDM_TR69C) 
	{
		/*here nvram changed, but MDM has not been changed yet, so for 1905, check changes here */
		credential_changed = wlmngr_checkCredentialChange(idx,&bssid,&conf_status);
		wlmngr_send_notification_1905(idx,bssid,(dataSync==WL_SYNC_FROM_MDM_HTTPD),credential_changed,conf_status);
	}

#endif
    switch ( dataSync ) {
        case WL_SYNC_TO_MDM:
        case WL_SYNC_TO_MDM_NVRAM:
           for (cnt=0; cnt<wl_cnt; cnt++) {
	            wlmngr_NvramMapping(cnt, MAP_FROM_NVRAM);
           }
            nmode_no_tkip(idx);
            wlLockWriteMdm();
            need_save = 1;
	     if (dataSync == WL_SYNC_TO_MDM_NVRAM)
                 need_nvram_save =1;
            break;

        case WL_SYNC_FROM_MDM_TR69C:
            wlLockReadMdmOne(idx);          
            wlLockReadMdmTr69Cfg(idx);
            wldsltr_set(idx);
#ifdef DSLCPE_1905
            /* here MDM has been changed and it has been loaded from MDM to wlmngr data structure,before it write NVRAM, 
             * check changes*/
            credential_changed = wlmngr_checkCredentialChange(idx,&bssid,&conf_status);
            wlmngr_send_notification_1905(idx,bssid,0,credential_changed,conf_status);
#endif
            wlLockWriteMdmOne(idx);
            need_save = 1;
            break;
        case WL_SYNC_FROM_MDM_HTTPD:
        case WL_SYNC_FROM_MDM:
            memset ( (void *)(m_instance_wl+idx), 0, sizeof(WLAN_ADAPTER_STRUCT) );
	    if (dataSync == WL_SYNC_FROM_MDM_HTTPD)
                 need_nvram_save =1;
	    break;
        default:
            break;
    }

     if (need_nvram_save)
          wlmngr_write_nvram();

     if ( need_save || need_nvram_save) {
	   if ( wlWriteMdmToFlash() != CMSRET_SUCCESS )
	      printf("%s::%s@%d Write to Flash failure\n", __FILE__, __FUNCTION__, __LINE__ );
	}

    return;
}

#if defined(HSPOT_SUPPORT) || defined(WL_IMPL_PLUS)

static bool enableHspot() {
	int i=0,j=0;
	char tempS[16];
	char *hspot_ucc=NULL;
	if(act_wl_cnt==0) return FALSE;
	/* by default, we adhere to original UCC testing,in case using new UCC
	 * where it change the HESSID to fix value, we will not set HESSID here*/
#ifdef WL_IMPL_PLUS
	hspot_ucc="YES";
#else
	hspot_ucc=nvram_get("hspot_ucc_new");
#endif
	for (i=0; i<wl_cnt; i++) {
		if(m_instance_wl[i].m_wlVar.wlEnbl == TRUE) {
			for(j=0;j<WL_NUM_SSID;j++) {
				if(m_instance_wl[i].m_wlMssidVar[j].wlEnableHspot==1) {
					/* we need to set hspot's hessid to the same as AP MAC 
					because hspot will reset it to default when get hs_reset cmd*/
					if(!hspot_ucc) {
						snprintf(tempS, sizeof(tempS), "%s_hessid",m_instance_wl[i].m_ifcName[j]);
						nvram_set(tempS,m_instance_wl[i].bssMacAddr[j]);
					}
					return TRUE;
				}
			}
		}
	}

#ifdef WL_IMPL_PLUS
	/* always run the hspotap application no matter what */
	return TRUE;
#else
	return FALSE;
#endif
}

static void wlmngr_HspotCtrl(int idx) {

	/* First to kill all hspotap process if already start*/
	bcmSystem("killall -q -15 hspotap 2>/dev/null");
	if ( enableHspot() == TRUE )
		bcmSystem("/bin/hspotap&");   
}
#endif

static bool enableBSD() {
    int i=0;
    char buf[WL_MID_SIZE_MAX];
    nvram_set("bsd_role","0");
    if( act_wl_cnt == 0 ) return FALSE;
    for (i=0; i<wl_cnt; i++)
        if( m_instance_wl[i].m_wlVar.wlEnbl == TRUE && m_instance_wl[i].m_wlVar.bsdRole > 0 ){
	    snprintf(buf, sizeof(buf), "%d", m_instance_wl[i].m_wlVar.bsdRole);
	    nvram_set("bsd_role",buf);
	    snprintf(buf, sizeof(buf), "%d", m_instance_wl[i].m_wlVar.bsdPport);
	    nvram_set("bsd_pport",buf);
	    snprintf(buf, sizeof(buf), "%d", m_instance_wl[i].m_wlVar.bsdHport);
	    nvram_set("bsd_hpport",buf);
	    snprintf(buf, sizeof(buf), "%s", m_instance_wl[i].m_wlVar.bsdHelper);
	    nvram_set("bsd_helper",buf);
	    snprintf(buf, sizeof(buf), "%s", m_instance_wl[i].m_wlVar.bsdPrimary);
	    nvram_set("bsd_primary",buf);
	    return TRUE;
        }
    return FALSE;
}
	
static void wlmngr_BSDCtrl(int idx) {
    /* First to kill all bsd process if already start*/
    bcmSystem("killall -q -15 bsd 2>/dev/null");
    if ( enableBSD() == TRUE ) 
        bcmSystem("/bin/bsd&");   
}

static bool enableSSD() {
    int i=0;
    char buf[WL_MID_SIZE_MAX];
    nvram_set("ssd_enable","0");
    if( act_wl_cnt == 0 ) return FALSE;
    for (i=0; i<wl_cnt; i++)
        if( m_instance_wl[i].m_wlVar.wlEnbl == TRUE && m_instance_wl[i].m_wlVar.ssdEnable > 0 ){
	    snprintf(buf, sizeof(buf), "%d", m_instance_wl[i].m_wlVar.ssdEnable);
	    nvram_set("ssd_enable",buf);
	    return TRUE;
        }
    return FALSE;
}

static void wlmngr_SSDCtrl(int idx) {
    /* First to kill all ssd process if already start*/
    bcmSystem("killall -q -15 ssd 2>/dev/null");
    if ( enableSSD() == TRUE ) 
        bcmSystem("/bin/ssd&");   
}

static unsigned int bits_count(unsigned int n) {
    unsigned int count = 0;
    while ( n > 0 ) {
        if ( n & 1 )
	    count++;
	n >>= 1;
    }
    return count;
}

void wlmngr_postStart(int idx){
    //check if support beamforming 
    if( wlmngr_getCoreRev(idx) >= 40 ) {
        char tmp[100], prefix[] = "wlXXXXXXXXXX_";
	int txchain;
	snprintf(prefix, sizeof(prefix), "%s_", m_instance_wl[idx].m_ifcName[0]);
	wlmngr_getVarFromNvram(&txchain, strcat_r(prefix, "txchain", tmp), "int");
	if( bits_count((unsigned int) txchain) > 1)
	    m_instance_wl[idx].m_wlVar.wlTXBFCapable=1;
    }

    wlLockWriteMdmOne(idx);

    if(!g_wlmngr_restart_all || ( g_wlmngr_restart_all &&  (++wl_visal_start_cnt)==wl_cnt)) {
#ifdef EXT_ACS
    /* Usermode autochannel */
    if (wlmngr_detectAcsd())
        wlmngr_startAcsd(idx);
#endif
#ifdef __CONFIG_VISUALIZATION__
	    bcmSystem("vis-dcon");
	    bcmSystem("vis-datacollector");
	    wl_visal_start_cnt=0;
#endif
    } 
  
}

/***************************************************************************
// Function Name: init.
// Description  : create WlMngr object
// Parameters   : none.
// Returns      : n/a.
****************************************************************************/
void wlmngr_init(int idx)
{
    char prefix[]="wl";
    int i=0;
#ifdef WL_WLMNGR_DBG
    printf("%s::%s@%d Adapter idx=%d\n", __FILE__, __FUNCTION__, __LINE__, idx );
#endif
    //set ASPM according to GPIOOverlays 
    //wlmngr_setASPM(idx);

    m_instance_wl[idx].m_refresh = FALSE;
    m_instance_wl[idx].wlInitDone = FALSE;

    wlmngr_initNvram(idx);

    wlmngr_retrieve(idx, FALSE);

#ifdef WL_WLMNGR_DBG
    printf("m_instance_wl[%d].m_ifcName[0]=%s\n", idx, m_instance_wl[idx].m_ifcName[0] );
#endif

    wlmngr_initVars(idx);

    //get unit number
    m_instance_wl[idx].m_unit = atoi(m_instance_wl[idx].m_ifcName[0]+strlen(prefix));

    //empty station list
    m_instance_wl[idx].numStations = 0;
    m_instance_wl[idx].stationList = NULL;
    m_instance_wl[idx].onBridge = FALSE;

    act_wl_cnt = 0;
    for (i=0; i<wl_cnt; i++)
       act_wl_cnt += ((m_instance_wl[i].m_wlVar.wlEnbl == TRUE) ? 1 : 0);

#ifdef WL_WLMNGR_DBG   
   printf("%s::%s@%d Adapter idx=%d\n", __FILE__, __FUNCTION__, __LINE__, idx );
#endif
    
#ifdef WLCONF
    /* Issue wlctl to setup wlan and start related process  */
    wlmngr_setup(idx, WL_SETUP_ALL);
#else
    /* Save Data to MDM*/
    wlLockWriteMdmOne(idx);

    /* Issue wlctl to setup wlan and start related process  */
    wlmngr_setup(idx, WL_SETUP_ALL);
#endif /* WLCONF */
    m_instance_wl[idx].wlInitDone = TRUE;
#ifdef WL_WLMNGR_DBG
    printf("\n\n%s Wlan Init Done!\n", __FUNCTION__ );
#endif
    wldsltr_get(idx);
    if ( wlLockWriteMdmTr69Cfg( idx )  != CMSRET_SUCCESS)
        printf("%s@%d wlWriteTr69Cfg Error\n", __FUNCTION__, __LINE__ );

    wlLockReadMdmTr69Cfg( idx );
#ifdef WL_WLMNGR_DBG
    printf("%s@%d\n", __FUNCTION__, __LINE__ );
#endif
    WLCSM_TRACE(WLCSM_TRACE_DBG, "DONE!!!!!");
}
/***************************************************************************
// Function Name: unInit.
// Description  : free interfaced information.
// Parameters   : n/a.
// Returns      : n/a.
****************************************************************************/
void wlmngr_unInit(int idx ) 
{
}

void wlmngr_retrieve(int idx, int isDefault) 
{
    int i;
#if defined(SUPPORT_WSC)
    char lan[32], buf[128];
    int br, cnt;
#endif

    for (i = 0; i < 30; i++)
    {
        if (wlLockReadMdm() != CMSRET_SUCCESS)
            sleep(1);
        else
            break;
    }
    if (i>=30)
        printf("wlmngr_retrieve() failed!\n");

#if defined(SUPPORT_WSC)
	 /* for each adapter */
        for (cnt=0; cnt<wl_cnt; cnt++) {
              /* for each intf */ 
              for (i = 0 ; i<WL_NUM_SSID; i++) {
                    br = m_instance_wl[cnt].m_wlMssidVar[i].wlBrName[2] - 0x30;

                    /* Set the OOB state */
                    if ( br == 0 )
                             snprintf(lan, sizeof(lan), "lan_wps_oob");
                    else
                             snprintf(lan, sizeof(lan), "lan%d_wps_oob", br);
                    strncpy(buf, nvram_safe_get(lan), sizeof(buf));
		      if ( buf[0]=='\0')
			  	continue;
                    if (!strcmp(buf,"enabled")) {
                           strncpy(m_instance_wl[cnt].m_wlMssidVar[i].wsc_config_state, "0", 2);
                    	}
                    else {
                           strncpy(m_instance_wl[cnt].m_wlMssidVar[i].wsc_config_state, "1", 2);
                    	}

              }
      	}
#endif

     // assign interface name
     for (i = 0 ; i<WL_NUM_SSID; i++) {
          if ( i==0 )
           snprintf(m_instance_wl[idx].m_ifcName[i], sizeof(m_instance_wl[idx].m_ifcName[i]), "wl%d", idx);
       else 
           snprintf(m_instance_wl[idx].m_ifcName[i], sizeof(m_instance_wl[idx].m_ifcName[i]), "wl%d.%d", idx, i);
        
#ifdef WL_WLMNGR_DBG
          printf("New===========m_instance_wl[%d].m_ifcName[%d]=%s\n", idx, i, m_instance_wl[idx].m_ifcName[i] );
#endif
      }
      strncpy(m_instance_wl[idx].m_wlVar.wlWlanIfName, m_instance_wl[idx].m_ifcName[0], sizeof(m_instance_wl[idx].m_wlVar.wlWlanIfName));

      strncpy(m_instance_wl[idx].m_wlVar.wlPhyType, wlmngr_getPhyType(idx), sizeof(m_instance_wl[idx].m_wlVar.wlPhyType));
      // need to get core revision from wlctl
      m_instance_wl[idx].m_wlVar.wlCoreRev = wlmngr_getCoreRev(idx);
      // need to validate rate since the rate
      // that stored in flash might be invalid
      m_instance_wl[idx].m_wlVar.wlBand = wlmngr_getValidBand(idx, m_instance_wl[idx].m_wlVar.wlBand);
      m_instance_wl[idx].m_wlVar.wlRate = wlmngr_getValidRate(idx, m_instance_wl[idx].m_wlVar.wlRate);         
      m_instance_wl[idx].m_wlVar.wlMCastRate = wlmngr_getValidRate( idx, m_instance_wl[idx].m_wlVar.wlMCastRate);
      wlmngr_getVer(idx);

#ifdef SUPPORT_SES
      m_instance_wl[idx].m_wlVar.wlSesEvent = 2; /*SES_EVTI_INIT_CONDITION*/ 
#endif

      nmode_no_tkip(idx);
      wlmngr_NvramMapping(idx, MAP_TO_NVRAM);
}

/* added for 11AC automation  */
//**************************************************************************
// Function Name: wlmngr_removeall_nvram
// Description  : unset all nvram entries
// Parameters   : None.
// Returns      : None.
//**************************************************************************
static void wlmngr_removeall_nvram(void)
{
    char *name,*val, *buf;
    char pair[WL_SIZE_132_MAX];
    buf=malloc(MAX_NVRAM_SPACE);
    if(!buf) {
        fprintf(stderr,"!!!!COULD NOT ALLOC MEM\n");
        return;
    }
    nvram_getall(buf, MAX_NVRAM_SPACE);
    for (name = buf; *name; name += strlen(name) + 1) {
        memcpy(pair,name,strlen(name)+1);
        val = strchr(pair, '=');
        if (val) {
            *val = '\0';
            val++;            
            nvram_unset(pair);
        }
        else        
            fprintf(stderr,"%s:%d  Error name:%s \r\n",__FUNCTION__,__LINE__,name);
        
    }
    free(buf);
    return;
}

//**************************************************************************
// Function Name: wlmngr_restore_config_default
// Description  : restore nvram configuation from either usb disk or tftp server
// Parameters   : tftpip- if present, it will restore from tftpserver,if null, 
//                restore from usb.
// Returns      : None.
//**************************************************************************
void wlmngr_restore_config_default(char *tftpip) {

    char *val;
    char pair[WL_SIZE_132_MAX];
    FILE *ofp;
    char  *nvramconfdir= NULL;
    if(tftpip)
    {
        snprintf(pair, sizeof(pair), "cd /var;tftp -g -r nvramdefault.txt %s",tftpip);
        bcmSystem(pair);
        nvramconfdir= "/var";
    }
    else
        nvramconfdir= wlmngr_get_nvramdefaultfile() ;

    if(nvramconfdir) {
        snprintf(pair, sizeof(pair), "%s/%s",nvramconfdir,"nvramdefault.txt");
        ofp=fopen(pair,"r");
    }
    else
        ofp=fopen("/mnt/disk1_1/nvramdefault.txt","r");

    if(!ofp) {
        fprintf(stderr,"%s:%d open configuration file error \r\n",__FUNCTION__,__LINE__ );
        return;
    }

    wlmngr_removeall_nvram();

    while(fgets(pair, sizeof(pair), ofp)) {
        val=strchr(pair,'\n');
        if(val) *val='\0';
        val = strchr(pair, '=');
        if (val) {
            *val = '\0';
            val++;          
            nvram_set(pair,val);
        } else
        fprintf(stderr,"%s:%d  Error ............... \r\n",__FUNCTION__,__LINE__);
    }
    fclose(ofp);
}

//**************************************************************************
// Function Name: initNvram
// Description  : initialize nvram settings if any.
// Parameters   : None.
// Returns      : None.
//**************************************************************************
void wlmngr_initNvram(int idx)
{	
    char *pos, *name=NULL, *val=NULL;
    char *str = malloc(MAX_NVRAM_SPACE*sizeof(char));
    int len;
    char pair[WL_LG_SIZE_MAX+WL_SIZE_132_MAX];
    int pair_len_byte = DEFAULT_PAIR_LEN_BYTE;
    int flag = 0;
    char format[]="%0?x"; //to store format string, such as %02x or %03x

    *str = '\0';	
    wlLockReadNvram(str, MAX_NVRAM_SPACE);
#ifdef WL_WLMNGR_DBG
    printf("str=%s\n", str);
#endif

    if (str[0] != '\0') {
         /* Parse str: format is xxname1=val1xxname2=val2xx...: xx is the len of pair of name&value */
         for (pos=str; *pos; ) {
              if( strlen(pos)< pair_len_byte) {
                     printf("nvram date corrupted[%s]..\n", pos);
		     free(str);
                     return;
              }
		
              strncpy(pair, pos, pair_len_byte);
              pair[pair_len_byte]='\0';

	      sprintf(format, "%s%d%s", "%0", pair_len_byte, "x"); //format set to "%02%d%s" or "%03%d%s"
	      if ( sscanf(pair, format, &len) !=1 ) {
	             printf("len error [%s]\n", pair);
		     free(str);
		     return;
	      }
		
#ifdef WL_WLMNGR_DBG
              printf("len=%d\n", len);
#endif
              if (len < pair_len_byte || len>= WL_LG_SIZE_MAX+WL_SIZE_132_MAX) {
                     printf("corrupted nvam...\n");
		     free(str);
                     return;
              }
              if (strlen(pos+pair_len_byte) < len ) {
                     printf("nvram date corrupted[%s]..\n", pos);
		     free(str);
                     return;
              }
		
              strncpy(pair, pos+pair_len_byte, len);
              pair[len]='\0';
#ifdef WL_WLMNGR_DBG
              printf("pair=%s\n", pair);
#endif
		
              pos += pair_len_byte+len;

              name = pair;
              val = strchr(pair, '=');
              if (val) {
                     *val = '\0';
                     val++;
#ifdef WL_WLMNGR_DBG
                     printf("name=%s val=%s\n", name, val );
#endif
		     if(flag==0 && !strcmp(name,"pair_len_byte"))
		            pair_len_byte=atoi(val);
                     if(strncmp(val,"*DEL*",5))
                            nvram_set( name, val);
		     flag = 1;
              }
              else { 
                     printf("pair not patch.[%s]..\n", pair);
		     free(str);
                     return;
              }
	    }
    }
    free(str);
}

/***************************************************************************
// Function Name: initVars.
// Description  : initialize vars based on run-time info
// Parameters   : none.
// Returns      : n/a.
****************************************************************************/
void wlmngr_initVars(int idx)
{
#define CAP_STR_LEN 250
    char buf[WL_LG_SIZE_MAX];
    char cmd[WL_LG_SIZE_MAX];
#ifdef WLCONF
    int i;
#else
    FILE *fp;
#endif
    
    m_instance_wl[idx].numBss = m_instance_wl[idx].maxMbss = WL_LEGACY_MSSID_NUMBER;
    m_instance_wl[idx].mbssSupported = FALSE;
    m_instance_wl[idx].aburnSupported=TRUE;
    m_instance_wl[idx].amsduSupported=FALSE;
   
    //retrive wlan driver info
#ifdef WLCONF
    snprintf(cmd, sizeof(cmd), "wl%d", idx);
    wl_iovar_get(cmd, "cap", (void *)buf, CAP_STR_LEN);
#else
   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d cap > /var/wl%dcap", idx, idx);
   BCMWL_WLCTL_CMD(cmd);

   snprintf(cmd, sizeof(cmd), "/var/wl%dcap", idx);
    
   fp = fopen(cmd, "r"); 
   if ( fp != NULL ) {   	   	
      for (;fgets(buf, sizeof(buf), fp);) {
#endif /* WLCONF */
#ifdef SUPPORT_SES      	
    if (!strstr(buf, "sta")) {
        m_instance_wl[idx].m_wlVar.wlSesWdsMode = 2;
        m_instance_wl[idx].m_wlVar.wlSesClEnable = 0;	
    }        
#endif
    if(strstr(buf, "1ssid")) {
        m_instance_wl[idx].numBss=1;
        /* for PSI comptibility */
        m_instance_wl[idx].mbssSupported=FALSE;
    }              
    if(strstr(buf, "mbss4")) {
        m_instance_wl[idx].numBss=m_instance_wl[idx].maxMbss = 4;
        m_instance_wl[idx].mbssSupported=TRUE;
    }
    // we are limiting it to 4 for now.
     if(strstr(buf, "mbss8")) {
        m_instance_wl[idx].numBss=m_instance_wl[idx].maxMbss = WL_MAX_NUM_SSID;
        m_instance_wl[idx].mbssSupported=TRUE;
    }
    if(strstr(buf, "mbss16")) {
        m_instance_wl[idx].numBss=m_instance_wl[idx].maxMbss = WL_MAX_NUM_SSID;
        m_instance_wl[idx].mbssSupported=TRUE;
    }
    if(strstr(buf, "afterburner")) {
        m_instance_wl[idx].aburnSupported=TRUE;
    }
    if(strstr(buf, "ampdu")) {
        m_instance_wl[idx].ampduSupported=TRUE;
    }         
    else {
        /* A-MSDU doesn't work with AMPDU */
        if(strstr(buf, "amsdu")) {
            m_instance_wl[idx].amsduSupported=TRUE;
        } 
    }
    if(strstr(buf, "wme")) {
        m_instance_wl[idx].wmeSupported=TRUE;         	
    }
#ifdef WMF
    if(strstr(buf, "wmf")) {
        m_instance_wl[idx].wmfSupported=TRUE;
    }
#endif

#ifdef DSLCPE
       m_instance_wl[idx].wmfSupported=TRUE; /* temporarly make wmf eanbled unconditionally, 43602 cap forgot to inlcude wmf? */
#endif
#ifdef DUCATI
    if(strstr(buf, "vec")) {
        m_instance_wl[idx].wlVecSupported=TRUE;
    }
#endif
    if(strstr(buf, "sta")) {
        m_instance_wl[idx].apstaSupported=TRUE;         	
        snprintf(cmd, sizeof(cmd), "wl%d_apsta", idx);
        nvram_set(cmd, "1");
    }
    if(strstr(buf, "media")) {
        m_instance_wl[idx].wlMediaSupported=TRUE;         	
    }

#ifndef WLCONF
     }
      fclose(fp);
   } else {
      
      printf("/var/wl%dcap open error\n", idx );
   }
#endif 
#if  defined(SUPPORT_WSC)
   if (strlen(nvram_safe_get("wps_device_pin")) != 8)
       wl_wscPinGet();
   if (strlen(nvram_safe_get("wps_version2")) == 0)
#ifdef WPS_V2
       nvram_set("wps_version2", "enabled");
#else
       nvram_set("wps_version2", "disabled");
#endif
     /* For WPS */
     nvram_set("router_disable", "0");
#endif /* SUPPORT_WSC */
#ifdef WLCONF
     /* For wlconf */
     snprintf(buf, sizeof(buf), "wl%d_ifname", idx);
     snprintf(cmd, sizeof(cmd), "wl%d", idx);
     nvram_set(buf, cmd);

     /* For MULTI-BSS */
     strncpy(cmd, "", sizeof(cmd));
     for (i=1; i<m_instance_wl[idx].numBss; i++) {
         if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {
            snprintf(buf, sizeof(buf), "wl%d.%d ", idx, i);
            strcat(cmd, buf);
         }
     }
     snprintf(buf, sizeof(buf), "wl%d_vifs", idx);
     nvram_set(buf, cmd);
#endif /* WLCONF */
#undef CAP_STR_LEN
}

#if defined(CONFIG_BCM96000) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963381)
/* Low end old platforms (no-fap, no-wfd)*/
void wlmngr_setupTPs(void)
{
    char cmd[WL_LG_SIZE_MAX];
    int enbl_idx=0;
    int tp = (is_smp_system) ? 1 : 0;

    /* if there exists only one adapter, put it to TP1.
     * if there exists two adapters, put wl0 on TP0, wl1 on TP1.
     * if there exists three adapters, put wl0 on TP0, wl1 on TP1. wl2 on TP0
     */
    if (act_wl_cnt >= 2) {
        snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id 0", m_instance_wl[0].m_ifcName[0]);
        BCMWL_WLCTL_CMD(cmd);
        snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id %d", m_instance_wl[1].m_ifcName[0], tp);
        BCMWL_WLCTL_CMD(cmd);
        if (act_wl_cnt == 3) {
            snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id %d", m_instance_wl[2].m_ifcName[0], 0);
            BCMWL_WLCTL_CMD(cmd);
        }
    } else if (act_wl_cnt == 1) {
        if (m_instance_wl[0].m_wlVar.wlEnbl == TRUE) 
            enbl_idx=0;
        else
            enbl_idx=1;
        snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id %d", m_instance_wl[enbl_idx].m_ifcName[0], tp);
        BCMWL_WLCTL_CMD(cmd);
    }

    return;
}

#elif defined(CONFIG_BCM963268)|| defined(CONFIG_BCM96362)
/* FAP based platforms */
void wlmngr_setupTPs(void)
{
    int pid_nic[2], pid_dhd[2], pid_wfd[2];
    int pid_wl0, pid_wl1, pid_wl2;
    int rc;
    cpu_set_t mask, wfd_mask;

    pid_nic[0] = prctl_getPidByName("wl0-kthrd");
    pid_nic[1] = prctl_getPidByName("wl1-kthrd");
    pid_dhd[0] = prctl_getPidByName("dhd0_dpc");
    pid_dhd[1] = prctl_getPidByName("dhd1_dpc");
    pid_wfd[0] = prctl_getPidByName("wfd0-thrd");
    pid_wfd[1] = prctl_getPidByName("wfd1-thrd");

    /* if there exists only one adapter, put it to TP1.
     * if there exists two adapters, put wl0 on TP0, wl1 on TP1.
     * if there exists three adapters, put wl0 on TP0, wl1 on TP1. wl2 on TP0
     */
    if (act_wl_cnt >= 2) {
        /* there exists 2 active adapters - nic/nic, dhd/nic, dhd/dhd */
        if (pid_dhd[0] > 0)
            pid_wl0 = pid_dhd[0]; /* first adapter is dhd */
        else
            pid_wl0 = pid_nic[0]; /* first adapter is nic */

        /* bind to TP0 */
        CPU_ZERO(&mask);
        CPU_SET(0, &mask);
        rc = sched_setaffinity(pid_wl0, sizeof(cpu_set_t), &mask);
        if (rc < 0)
            printf("sched_setaffinity failed, rc=%d, pid_wl0=%d\n", rc, pid_wl0);

        if (pid_dhd[1] > 0)
            pid_wl1 = pid_dhd[1]; /* second adapter is dhd */
        else
            pid_wl1 = pid_nic[1]; /* second adapter is nic */

        /* bind to TP1 */
        CPU_ZERO(&mask);
        CPU_SET(1, &mask);
        rc = sched_setaffinity(pid_wl1, sizeof(cpu_set_t), &mask);
        if (rc < 0)
            printf("sched_setaffinity failed, rc=%d, pid_wl1=%d\n", rc, pid_wl1);
    } else if (act_wl_cnt == 1) {
        /* there exists only one active adapter */
        if (pid_dhd[0] > 0) {
            pid_wl0 = pid_dhd[0]; /* adapter is dhd */
            /* bind dhd0_dpc to TP0 */
            CPU_ZERO(&mask);
            CPU_SET(0, &mask);
        } else {
            pid_wl0 = pid_nic[0]; /* adapter is nic */
            /* bind wl0-kthrd to TP1 */
            CPU_ZERO(&mask);
            CPU_SET(1, &mask);
        }

        rc = sched_setaffinity(pid_wl0, sizeof(cpu_set_t), &mask);
        if (rc < 0)
            printf("sched_setaffinity failed, rc=%d, pid_wl0=%d\n", rc, pid_wl0);
        rc = sched_setaffinity(pid_wfd[0], sizeof(cpu_set_t), &wfd_mask);
        if (rc < 0)
            printf("sched_setaffinity failed, rc=%d, pid_wfd0=%d\n", rc, pid_wfd[0]);
    }

    return;
}
#else
/* High-end wfd based platforms */
void wlmngr_setupTPs(void)
{
    int radio, cpu, max_cpu;
    int pid_wl, pid_wfd;
    char process_name[16];
    cpu_set_t mask;
    int rc;


    max_cpu = sysconf( _SC_NPROCESSORS_ONLN );
    for (radio=0; radio < MAX_WLAN_ADAPTER; radio++) {

        snprintf(process_name, sizeof(process_name), "dhd%d_dpc",radio);
        pid_wl = prctl_getPidByName(process_name);

        if (pid_wl <= 0) {
            snprintf(process_name, sizeof(process_name), "wl%d-kthrd",radio);
            pid_wl = prctl_getPidByName(process_name);
        }

        if (pid_wl > 0) {
            snprintf(process_name, sizeof(process_name), "wfd%d-thrd",radio);
            pid_wfd = prctl_getPidByName(process_name);

            /*
             * Policy - Current P1
             * P1 - sequencial radio/cpu assignment
             *          cpu = (radio%max_cpu);
             * P2 - reverse sequencial radio/cpu assignment
             *          cpu = max_cpu - 1 - (radio%max_cpu);
             *
             */
            cpu = (radio%max_cpu);
            
            CPU_ZERO(&mask);
            CPU_SET(cpu, &mask);
            
            rc = sched_setaffinity(pid_wl, sizeof(cpu_set_t), &mask);
            if (rc < 0)
                printf("sched_setaffinity failed, rc=%d, wl%d, pid=%d\n", rc, radio, pid_wl);

            if (pid_wfd > 0) {
                rc = sched_setaffinity(pid_wfd, sizeof(cpu_set_t), &mask);
                if (rc < 0)
                    printf("sched_setaffinity failed, rc=%d, wfd%d, pid=%d\n", rc, radio, pid_wfd);
            }
        }
    }

   return;
}

#endif
extern struct nvram_tuple router_defaults_override_type1[];

static void wlmngr_devicemode_overwrite(const unsigned int idx)
{
	char temp[100],temp2[100], *name, prefix[8];
	int i=0;
	struct nvram_tuple *t;
	if((!strncmp(nvram_safe_get("devicemode"), "1", 1))) {
		for (t = router_defaults_override_type1; t->name; t++) {
			if(!strncmp(t->name,"router_disable",14)) 
				continue;
			for (i = 0; i < WL_NUM_SSID; i++) {
				if (!strncmp(t->name, "wl_", 3)) {
					if(i)	
						sprintf(temp, "wl%d.%d_%s", idx,i,t->name+3);
					else 
						sprintf(temp, "wl%d_%s", idx,t->name+3);
					name = temp;
				} else
					name = t->name;
				nvram_set(name, t->value);
				WLCSM_TRACE(WLCSM_TRACE_ERR, " set %s= %s\r\n", name, t->value);
			}
		}
		sprintf(prefix,"wl%d_",idx);
		wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlFrameBurst, strcat_r(prefix, "frameburst", temp), "string"); 
		wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlTafEnable, strcat_r(prefix, "taf_enable", temp), "int");
		wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlPspretendRetryLimit, strcat_r(prefix, "pspretend_retry_limit", temp), "int");
		wlmngr_getVarFromNvram(&temp2, strcat_r(prefix, "reg_mode", temp), "string"); 
		if (!strcmp(temp2, "h"))
			m_instance_wl[idx].m_wlVar.wlRegMode = REG_MODE_H;
		else if (!strcmp(temp2, "d"))    
			m_instance_wl[idx].m_wlVar.wlRegMode = REG_MODE_D;
		else
			m_instance_wl[idx].m_wlVar.wlRegMode = REG_MODE_OFF;

		for (i = 0; i < WL_NUM_SSID; i++) {
			if(i)
				sprintf(prefix, "wl%d.%d", idx,i);
			else 
				sprintf(prefix, "wl%d", idx);
			wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlEnableWmf,strcat_r(prefix, "wmf_bss_enable", temp), "int");
		}
		nvram_set("pre_devicemode","1");
	}  else {
		char *premode= nvram_get("pre_devicemode");
		if(premode && (!strncmp(premode, "1", 1))) {
			fprintf(stderr," DEVICE MODE CHANGED!!! SUGGEST TO RESTORE DEFAULT AND RECONFIGURE DEVICE!!!! \r\n");
		} 
	}
}


//**************************************************************************
// Function Name: setup
// Description  : configure and setup wireless interface.
// Parameters   : type -- kind of setup.
// Returns      : None.
//**************************************************************************
void wlmngr_setup(int idx, WL_SETUP_TYPE type) {

   char cmd[WL_LG_SIZE_MAX];
#ifndef WLCONF
   int i;
   bool mbss_on = FALSE;
#endif

#ifdef SUPPORT_WSC
   char *restart;
   bool is_wps_restarted = FALSE;
#endif

#ifdef WL_WLMNGR_DBG
   printf("%s::%s@%d Adapter idx=%d\n", __FILE__, __FUNCTION__, __LINE__, idx );
#endif

   wlmngr_setupTPs();

   wlmngr_devicemode_overwrite(idx);
   //CRDDB00017249 war
   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d phy_watchdog 0", idx);   	
   BCMWL_WLCTL_CMD(cmd);  

#ifdef SUPPORT_WSC

    restart = nvram_get("wps_force_restart");
    if ( restart!=NULL && (strncmp(restart, "y", 1)!= 0) &&  (strncmp(restart, "Y", 1)!= 0)  ) 
    {
       nvram_set("wps_force_restart", "Y");
    } 
    else
#endif	
     {
       //stop common service/daemon 
       wlmngr_stopServices(idx);   
      
#ifdef WLCONF
       wlmngr_WlConfDown(idx);
#else

       /* up wlan to finish the LED initialization */
       snprintf(cmd, sizeof(cmd), "wlctl -i %s up", m_instance_wl[idx].m_ifcName[0]);
       BCMWL_WLCTL_CMD(cmd); 
       
       //overwrite sprom and set led dc 100% for full led brightness
       snprintf(cmd, sizeof(cmd), "wlctl -i %s leddc 0 2>/dev/null", m_instance_wl[idx].m_ifcName[0]); 
       BCMWL_WLCTL_CMD(cmd);                    
      
       //turn down wireless
       snprintf(cmd, sizeof(cmd), "wlctl -i %s down", m_instance_wl[idx].m_ifcName[0]);   	
       BCMWL_WLCTL_CMD(cmd); 
   
       /* clean up wds when wl down */
       snprintf(cmd, sizeof(cmd), "wlctl -i %s wds none", m_instance_wl[idx].m_ifcName[0]);
       BCMWL_WLCTL_CMD(cmd);

       //assume if there is a guest bss enabled,
       //need to turn on mbss flag
       for (i=1; i<m_instance_wl[idx].numBss; i++) {
          if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid) {
             mbss_on = TRUE;
             break;
          }
       }
       

       // turn on multiple SSID
       if (m_instance_wl[idx].mbssSupported)
       {
          if ((cur_mbss_on == FALSE) || (mbss_on == TRUE))
          {
              /* Not allow mbss to be set from 1 to 0 */
              snprintf(cmd, sizeof(cmd), "wlctl -i %s mbss %d", m_instance_wl[idx].m_ifcName[0], mbss_on);
              cur_mbss_on = mbss_on; /* keep the current mbss status */
          }
       }
          
       BCMWL_WLCTL_CMD(cmd);   
       
       //disable all BSS configs
       for (i=0; i<m_instance_wl[idx].numBss; i++) {
          snprintf(cmd, sizeof(cmd), "wlctl -i %s bss -C %d down", m_instance_wl[idx].m_ifcName[0], i);
          BCMWL_WLCTL_CMD(cmd);
       }
#endif /* WLCONF */

       // set ssid for each BSS configs
       wlmngr_setSsid(idx);

       // disable all wl ifcs
       wlmngr_wlIfcDown(idx);

       // enable/disable wireless
       if ( m_instance_wl[idx].m_wlVar.wlEnbl == TRUE ) {
#ifndef WLCONF
          // setup mac address
	  wlmngr_setupMbssMacAddr(idx);

          // hide and country
          wlmngr_doBasic(idx);
          
          // advanced features
          wlmngr_doAdvanced(idx);
          
          // MAC filter
          wlmngr_doMacFilter(idx);  
            
          /* Move back. This function should be called to cleanup QOS queeu when wlan is disabled */
          // Qos
          /* wlmngr_doQoS(idx);  */

          //select antenna
          wlmngr_setupAntenna(idx);
                          
          //start wlc/netif
          snprintf(cmd, sizeof(cmd), "wlctl -i %s up", m_instance_wl[idx].m_ifcName[0]);      
          BCMWL_WLCTL_CMD(cmd);   
#endif /* UN-WLCONF */                          
          if (!wlmngr_detectAcsd())
              // Legacy in-driver auto channel
              wlmngr_autoChannel(idx);
#ifdef WLCONF
          wlmngr_doWlConf(idx);
          // setup mac address
	  wlmngr_setupMbssMacAddr(idx);
#endif
          // security for each BSS configs
          wlmngr_doSecurity(idx);

          //do WEP over WDS
          wlmngr_doWdsSec(idx);
#ifndef WLCONF
          /* Save Data to MDM*/
          wlLockWriteMdmOne(idx);
#endif
       } 
       //execute common service/daemon
       wlmngr_enum_ifnames();
       wlmngr_startServices(idx);
       wlmngr_issueServiceCmd(idx); 

       //move "wl up" later after wps_monitor/wps cmds is due to Win7 logo requirement.
       //Win7 logo needs to see WPS IE present in Beacon right after configured AP within 1 second
       if ( m_instance_wl[idx].m_wlVar.wlEnbl == TRUE ) {
#ifdef WLCONF
           wlmngr_WlConfStart(idx);
#else 
           // Turn on enabled ssid
           for (i = 0; i < m_instance_wl[idx].numBss; i++) {
               if (m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid 
                   && !strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_AP)) {
                    snprintf(cmd, sizeof(cmd), "wlctl -i %s bss -C %d up", m_instance_wl[idx].m_ifcName[0], i);
                    BCMWL_WLCTL_CMD(cmd);
               }
           }

           // WET (main ssid, wl0/wl1/wl2) to join with upstream AP
           if (!strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET) || !strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET_MACSPOOF)) {
               // Wireless Ethernet / MAC Spoof
               snprintf(cmd, sizeof(cmd), "wlctl -i %s bss -C 0 up", m_instance_wl[idx].m_ifcName[0]);
               BCMWL_WLCTL_CMD(cmd);
           }

#endif	    /* WLCONF */

           // turn on only active ifcs            
           wlmngr_wlIfcUp(idx);
       }
#ifndef WLCONF
       // wds
       wlmngr_doWds(idx);
#endif
	   
#ifdef SUPPORT_WSC
       is_wps_restarted = TRUE; // WPS service is restarted
#endif
   }

   wlmngr_doQoS(idx);                 
   //CRDDB00017249 war
   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d phy_watchdog 1", idx);   	
   BCMWL_WLCTL_CMD(cmd);  
   
   /* Enable flow cache */
   snprintf(cmd, sizeof(cmd), "wlctl -i wl%d fcache 1", idx);   	
   BCMWL_WLCTL_CMD(cmd);

   // send ARP packet with bridge IP and hardware address to device
   // this piece of code is -required- to make br0's mac work properly
   // in all cases
   snprintf(cmd, sizeof(cmd), "/usr/sbin/sendarp -s %s -d %s", m_instance_wl[idx].m_wlMssidVar[0].wlBrName, m_instance_wl[idx].m_wlMssidVar[0].wlBrName);
   bcmSystem(cmd);     


#ifdef SUPPORT_WSC
   if (is_wps_restarted == FALSE)
#endif
       wlmngr_issueServiceCmd(idx);
#if defined(HSPOT_SUPPORT) || defined(WL_IMPL_PLUS)
       wlmngr_HspotCtrl(idx);
#endif

       wlmngr_postStart(idx);
}

/* Update tr69 assocList Object */
void wlmngr_update_assoc_list(int idx)
{
	wlmngr_aquireStationList(idx);
	wlLockWriteAssocDev( idx+1 );
}

//**************************************************************************
// Function Name: clearSesLed
// Description  : clear SES LED.
// Parameters   : none.
// Returns      : none.
//**************************************************************************
void wlmngr_clearSesLed(int idx)
{   
   int f = open( "/dev/brcmboard", O_RDWR );
   /* set led off */
   if( f > 0 ) {
      int  led = 0;   	
      BOARD_IOCTL_PARMS IoctlParms;
      memset( &IoctlParms, 0x00, sizeof(IoctlParms) );
      IoctlParms.result = -1;
      IoctlParms.string = (char *)&led;
      IoctlParms.strLen = sizeof(led);         
      ioctl(f, BOARD_IOCTL_SET_SES_LED, &IoctlParms);
      close(f);
   }
}

//**************************************************************************
// Function Name: getGPIOverlays
// Description  : get the value of GPIO overlays
// Parameters   : interface idx.
// Returns      : none.
//**************************************************************************
void wlmngr_getGPIOverlays(int idx)
{   
   int f = open( "/dev/brcmboard", O_RDWR );
   if( f > 0 ) {
      unsigned long  GPIOOverlays = 0;   	
      BOARD_IOCTL_PARMS IoctlParms;
      memset( &IoctlParms, 0x00, sizeof(IoctlParms) );
      IoctlParms.result = -1;
      IoctlParms.string = (char *)&GPIOOverlays;
      IoctlParms.strLen = sizeof(GPIOOverlays);         
      ioctl(f, BOARD_IOCTL_GET_GPIOVERLAYS, &IoctlParms);
      m_instance_wl[idx].m_wlVar.GPIOOverlays=GPIOOverlays;
      close(f);
   }
}

//**************************************************************************
// Function Name: setASPM
// Description  : set aspm according to GPIOOverlays
// Parameters   : interface idx.
// Returns      : none.
//**************************************************************************
void wlmngr_setASPM(int idx)
{
     wlmngr_getGPIOverlays(idx);
#ifdef IDLE_PWRSAVE
    {
        char cmd[WL_LG_SIZE_MAX];
        int is_dhd=0;
        snprintf(cmd,sizeof(cmd),"wl%d",idx);
        is_dhd=!dhd_probe(cmd);
        /* Only enable L1 mode, because L0s creates EVM issues. The power savings are the same */
        if (m_instance_wl[idx].m_wlVar.GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ) {
            if(is_dhd)
                snprintf(cmd, sizeof(cmd), "dhdctl -i wl%d aspm 0x102", idx);
            else
                snprintf(cmd, sizeof(cmd), "wlctl -i wl%d aspm 0x102", idx);
        } else {
            if(is_dhd)
                snprintf(cmd, sizeof(cmd), "dhdctl -i wl%d aspm 0x2", idx);
            else
                snprintf(cmd, sizeof(cmd), "wlctl -i wl%d aspm 0x2", idx);
        }
        bcmSystem(cmd);
    }
#endif
}

//**************************************************************************
// Function Name: startSes
// Description  : execute command to start SES.
// Parameters   : none.
// Returns      : none.
//**************************************************************************
#ifdef SUPPORT_SES
void wlmngr_startSes(int idx) 
{
   printf("wlSesEnable=%d ,wlSesEvent=%d, wlSesStates=%s\n", 
    m_instance_wl[idx].m_wlVar.wlSesEnable,m_instance_wl[idx].m_wlVar.wlSesEvent, m_instance_wl[idx].m_wlVar.wlSesStates );
     
   m_instance_wl[idx].m_refresh = TRUE; 
   if ( m_instance_wl[idx].m_wlVar.wlSesEnable == FALSE )
      return;
                    
   bcmSystem("ses -f \n");	
}

//**************************************************************************
// Function Name: stopSes
// Description  : stop SES process.
// Parameters   : none.
// Returns      : none.
//**************************************************************************
void wlmngr_stopSes(int idx) 
{
   int  pid = 0;
   int  retries=10;   
   if ( (pid = bcmGetPid("ses -f")) > 0 ){   	      
      if ( m_instance_wl[idx].m_wlVar.wlSesEnable == FALSE ) {
         //clean up by restart ses then kill it
         kill(pid, SIGHUP);
         m_instance_wl[idx].m_wlVar.wlSesEvent=2;   	
         wlmngr_setVarToNvram(idx,&m_wlVar.wlSesEvent, "ses_event", "int"); 

         bcmSystem("ses -f \n"); 
         pid = 0;
         while ( (retries-- > 0) && (pid <= 0)) {
            usleep(200);
            pid = bcmGetPid("ses -f");      
         }
         sleep(1);
         if( pid > 0)
            kill(pid, SIGHUP);
      }else  {
         kill(pid, SIGHUP);
      }        
   }	
}		

//**************************************************************************
// Function Name: startSesCl
// Description  : execute command to start SES client.
// Parameters   : none.
// Returns      : none.
//**************************************************************************
void wlmngr_startSesCl(int idx) 
{
   printf("echo wlSesClEnable=%d ,wlSesClEvent=%d \n", 
    m_instance_wl[idx].m_wlVar.wlSesClEnable,m_instance_wl[idx].m_wlVar.wlSesClEvent);
     
   m_instance_wl[idx].m_refresh = TRUE; 
   if ( m_instance_wl[idx].m_wlVar.wlSesClEnable == FALSE )
      return;
                    
   bcmSystem("ses_cl -f \n");   
}

//**************************************************************************
// Function Name: stopSesCl
// Description  : stop SES client process.
// Parameters   : none.
// Returns      : none.
//**************************************************************************
void wlmngr_stopSesCl(int idx) 
{
   int  pid = 0;
     
   if ( (pid = bcmGetPid("ses_cl -f")) > 0 ){   	      
         kill(pid, SIGHUP);     
   }	
}
#endif // end SUPPORT_SES

#if defined(SUPPORT_WSC)
void wlmngr_startWsc(int idx)
{
    int i=0, j=0;
    char buf[64];
	
    int br;
    char ifnames[128];

    strncpy(buf, nvram_safe_get("wl_unit"), sizeof(buf));

    if (buf[0] == '\0')
    {
        nvram_set("wl_unit", "0");
        i = 0;
        j = 0;
    }
    else
    {
        if ((buf[1] != '\0') && (buf[2] != '\0'))
        {
            buf[3] = '\0';
            j = isdigit(buf[2])? atoi(&buf[2]):0;
        }
        else
        {
            j = 0;
        }

        buf[1] = '\0';
        i = isdigit(buf[0]) ? atoi(&buf[0]):0;
    }

    if(m_instance_wl[i].m_ifcName[0][0] == '\0') //must use [0][0] not [j][0]
    {
	bcmSystem("/bin/wps_monitor&");
	return;
    }

    nvram_set("wps_mode", m_instance_wl[i].m_wlMssidVar[j].wsc_mode); //enabled/disabled
    nvram_set("wl_wps_config_state", m_instance_wl[i].m_wlMssidVar[j].wsc_config_state); // 1/0
    nvram_set("wl_wps_reg",         "enabled");

    /* Since 5.22.76 release, WPS IR is changed to per Bridge. Previous IR enabled/disabled is 
       Per Wlan Intf */

    for ( br=0; br<MAX_BR_NUM; br++ ) {
        if ( br == 0 )
           snprintf(buf, sizeof(buf), "lan_ifnames");
        else
           snprintf(buf, sizeof(buf), "lan%d_ifnames", br);

        strncpy(ifnames, nvram_safe_get(buf), sizeof(ifnames));
	 if (ifnames[0] =='\0')
	 	continue;
          if ( br == 0 )
                snprintf(buf, sizeof(buf), "lan_wps_reg");
          else
                snprintf(buf, sizeof(buf), "lan%d_wps_reg", br);
          nvram_set(buf, "enabled");
    }

    nvram_set("wps_uuid",           "0x000102030405060708090a0b0c0d0ebb");
    nvram_set("wps_device_name",    "BroadcomAP");
    nvram_set("wps_mfstring",       "Broadcom");
    nvram_set("wps_modelname",      "Broadcom");
    nvram_set("wps_modelnum",       "123456");
    nvram_set("boardnum",           "1234");
    nvram_set("wps_timeout_enable",	"0");
    
    if (nvram_get("wps_config_method") == NULL) /* initialization */
    {
        set_wps_config_method_default(); /* for DTM 1.1 test */
        if (nvram_match("wps_version2", "enabled"))
           nvram_set("_wps_config_method", "sta-pin"); /* This var is only for WebUI use, default to sta-pin */
        else
           nvram_set("_wps_config_method", "pbc"); /* This var is only for WebUI use, default to PBC */
    }
		
    nvram_set("wps_config_command", "0");
    nvram_set("wps_status", "0");
    nvram_set("wps_method", "1");
    nvram_set("wps_config_command", "0");
    nvram_set("wps_proc_mac", "");

    if (nvram_match("wps_restart", "1")) {
       nvram_set("wps_restart", "0");
    }
    else {
       nvram_set("wps_restart", "0");
       nvram_set("wps_proc_status", "0");
    }

    nvram_set("wps_sta_pin", "00000000");
    nvram_set("wps_currentband", "");   
    nvram_set("wps_autho_sta_mac", "00:00:00:00:00:00");

    wlmngr_NvramMapping(i, MAP_FROM_NVRAM);
    bcmSystem("/bin/wps_monitor&");

    return;
}
#endif //end of SUPPORT_WSC


#if defined(SUPPORT_WSC)
void wlmngr_stopWps( int idx)
{   
     get_wps_env();
     /* WPS is inaction?? */
     if ( wps_config_command != 0 ) {
	 set_wps_env("SET wps_config_command=2 wps_action=0");
        sleep(1);
      }
      return;
}

/* Issue WPS Cmd */
void wlmngr_issueWpsCmd( int idx)
{
  char cfg[32]={0};
  char buf[256]={0};
  char pin[16]={0};
  char mac[32]={0};
  int apPin_action=5;

  /*
       wps_method: 
         WPS_UI_METHOD_NONE		0 
         WPS_UI_METHOD_PIN		1
         WPS_UI_METHOD_PBC		2

       wps_action: 
         WPS_UI_ACT_NONE			0
         WPS_UI_ACT_ENROLL		    1
         WPS_UI_ACT_CONFIGAP		2
         WPS_UI_ACT_ADDENROLLEE		3
         WPS_UI_ACT_STA_CONFIGAP    4
         WPS_UI_ACT_STA_GETAPCONFIG	5
   */

  strncpy(cfg, nvram_safe_get("wps_config"), sizeof(cfg));
  
  if (!strcmp(cfg, "client-pbc") || !strcmp(cfg, "client-pbc-reset")) {

     wlmngr_stopWps(idx);

     if (nvram_match("wps_version2", "enabled")) {
         strncpy(mac, nvram_get("wps_autho_sta_mac"), sizeof(mac));

         if (wlmngr_isEnrSta(idx)) 
             snprintf(buf, sizeof(buf), "SET wps_method=2 wps_sta_pin=00000000 wps_action=1 wps_config_command=1 wps_pbc_method=2 wps_ifname=wl%s wps_autho_sta_mac=%s wps_enr_auto=%d wps_enr_ssid=%s wps_enr_bssid=%s wps_enr_wsec=%s", 
                     nvram_get("wl_unit"), mac, wps_enr_auto, nvram_get("wps_enr_ssid"), nvram_get("wps_enr_bssid"), nvram_get("wps_enr_wsec"));
         else
             snprintf(buf, sizeof(buf), "SET wps_method=2 wps_sta_pin=00000000 wps_action=3 wps_config_command=1 wps_pbc_method=2 wps_ifname=wl%s wps_autho_sta_mac=%s", nvram_get("wl_unit"), mac);

     } else
         snprintf(buf, sizeof(buf), "SET wps_method=2 wps_sta_pin=00000000 wps_action=3 wps_config_command=1 wps_pbc_method=2 wps_ifname=wl%s", nvram_get("wl_unit"));
     set_wps_env(buf);
  }

  if ( !strcmp(cfg, "client-pin") || !strcmp(cfg, "client-pin-reset")) {
     wlmngr_stopWps(idx);
     strncpy(pin, nvram_get("wps_sta_pin"), sizeof(pin));
     if (nvram_match("wps_version2", "enabled")) {
         strncpy(mac, nvram_get("wps_autho_sta_mac"), sizeof(mac));

         if (wlmngr_isEnrSta(idx))
             snprintf(buf, sizeof(buf), "SET wps_method=1 wps_action=1 wps_config_command=1 wps_ifname=wl%s wps_enr_auto=%d wps_enr_ssid=%s wps_enr_bssid=%s wps_enr_wsec=%s", 
                     nvram_get("wl_unit"), wps_enr_auto, nvram_get("wps_enr_ssid"), nvram_get("wps_enr_bssid"), nvram_get("wps_enr_wsec"));
         else
             snprintf(buf, sizeof(buf), "SET wps_method=1 wps_sta_pin=%s wps_action=3 wps_config_command=1 wps_pbc_method=2 wps_ifname=wl%s wps_autho_sta_mac=%s", pin, nvram_get("wl_unit"),mac);
     } else
         snprintf(buf, sizeof(buf), "SET wps_method=1 wps_sta_pin=%s wps_action=3 wps_config_command=1 wps_pbc_method=2 wps_ifname=wl%s", pin,nvram_get("wl_unit"));
     set_wps_env(buf);
  }

  if (!strcmp(cfg, "ap-pin") || !strcmp(cfg, "ap-pbc")) {
     wlmngr_stopWps(idx);
     if (wlmngr_isEnrSta(idx)) {
         strncpy(pin, nvram_get("wps_ApPinGetCfg"), sizeof(pin));
         if (!strcmp(pin, "1"))
             apPin_action = 5;
         else
             apPin_action = 4;

         strncpy(pin, nvram_get("wps_sta_pin"), sizeof(pin));
         snprintf(buf, sizeof(buf), "SET wps_method=1 wps_action=%d wps_config_command=1 wps_pbc_method=2 wps_ifname=wl%s wps_stareg_ap_pin=%s wps_scstate=%s wps_enr_auto=%d wps_enr_ssid=%s wps_enr_bssid=%s wps_enr_wsec=%s",
             apPin_action, nvram_get("wl_unit"), pin, nvram_get("wps_ap_configured"), wps_enr_auto, nvram_get("wps_enr_ssid"), nvram_get("wps_enr_bssid"), nvram_get("wps_enr_wsec"));
     }
	 else 
        snprintf(buf, sizeof(buf), "SET wps_method=1 wps_action=2 wps_config_command=1 wps_pbc_method=2 wps_ifname=wl%s",nvram_get("wl_unit"));

     set_wps_env(buf);
  }

  nvram_set("wps_config", "DONE");

  sleep(1); //MUST (Win7 logo): wait some time for wps commands to be completed

  return;
}

void set_wps_config_method_default(void)
{
    if (nvram_match("wps_version2", "enabled"))
        nvram_set("wps_config_method", "0x228c");
    else
        nvram_set("wps_config_method", "0x84");
}
#endif /* SUPPORT_WSC */  

/* remove ifname from nvram when service is down */
static void wlmngr_remove_acs_ifnames(int idx) {
    char name[WL_SIZE_512_MAX]= {'\0'};
    char ifname[WL_SM_SIZE_MAX],*next;
    char wlname[WL_SM_SIZE_MAX];
    int inited=0;
    char *p1=nvram_get("acs_ifnames");
    if(p1) {
        snprintf(wlname,WL_SM_SIZE_MAX,"wl%d",idx);
        foreach(ifname,p1,next) {
            if(strncmp(wlname,ifname,WL_SM_SIZE_MAX)) {
                if(inited) {
                    if((strlen(name)+strlen(ifname)+1) < WL_SIZE_512_MAX) {
                        strcat(name," ");
                        strcat(name,ifname);
                    } else {
                        fprintf(stderr, "!!!ifname in acs_ifnames is so long???\n");
                    }
                } else {
                    inited=1;
                    snprintf(name,WL_SIZE_512_MAX,"%s",ifname);
                }
            }
        }
        if(inited) nvram_set("acs_ifnames",name);
    }
    return ;
}
//**************************************************************************
// Function Name: stopServices
// Description  : stop deamon services 
//                which is required/common for each reconfiguration
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_stopServices(int idx)
{
#ifdef SUPPORT_SES    
   //stop ses   	
   wlmngr_stopSesCl(idx);
   wlmngr_stopSes(idx);
   wlmngr_clearSesLed(idx);
#endif   

  #ifdef SUPPORT_WSC
    bcmSystem("killall -q -9 lld2d 2>/dev/null");
    bcmSystem("killall -q -9 wps_ap 2>/dev/null");
    bcmSystem("killall -q -9 wps_enr 2>/dev/null");
    bcmSystem("killall -q -15 wps_monitor 2>/dev/null"); // Kill Child Thread first, -15: SIGTERM to force to remove WPS IE
    /* For WPS version 1 bcmupnp
    bcmSystem("killall -15 bcmupnp 2>/dev/null"); // -15: SIGTERM, force bcmupnp to send SSDP ByeBye message out (refer to CR18802) 
    bcmSystem("rm -rf /var/bcmupnp.pid");
    */ 

    wlmngr_clearSesLed(idx);
  #endif
    bcmSystem("killall -q -9 nas 2>/dev/null");
    bcmSystem("killall -q -9 eapd 2>/dev/null");
#ifdef EXT_ACS
    bcmSystem("killall -q -9 acsd 2>/dev/null");	
    wlmngr_remove_acs_ifnames(idx);
#endif
#ifdef BCMWAPI_WAI
    bcmSystem("killall -q -15 wapid");
#endif
#if defined(HSPOT_SUPPORT) || defined(WL_IMPL_PLUS)
    bcmSystem("killall -q -15 hspotap");
#endif

    bcmSystem("killall -q -15 bsd");
    bcmSystem("killall -q -15 ssd");
    
#ifdef __CONFIG_VISUALIZATION__
    bcmSystem("killall -q -9 vis-datacollector");
    bcmSystem("killall -q -9 vis-dcon");
#endif
}

/* Start acsd apps */
#ifdef EXT_ACS
void wlmngr_startAcsd( int idx)
{
#ifndef WLCONF
    char *ptr, buf[64];
    int i;
    nvram_set("acs_ifnames", "");

    for (i=0; i<=idx; i++) {
	/* create acs_ifnames for acsd */	   
	if (m_instance_wl[i].m_wlVar.wlEnbl == TRUE) {
		
		ptr = nvram_safe_get("acs_ifnames");
		if (*ptr !='\0')
			snprintf(buf, sizeof(buf), "%s wl%d", ptr, i);
		else
			snprintf(buf, sizeof(buf), "wl%d", i);
			
		nvram_set("acs_ifnames", buf);
	}
    } 
#endif

    /* not background, due to acs init scan should be before wl up */
    bcmSystem("/bin/acsd");

}
#endif

//**************************************************************************
// Function Name: startService
// Description  : start deamon services 
//                which is required/common for each reconfiguration
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_startServices(int idx)
{ 
    char tmp[64];
    char *lan_ifname;

    if (act_wl_cnt == 0)
       return;  /* all adapters are disabled */

    lan_ifname = nvram_safe_get("lan_ifname");
#ifdef SUPPORT_WSC
     /* For WPS version 1 bcmupnp
     snprintf(tmp, sizeof(tmp), "/bin/bcmupnp -D");
     */

     snprintf(tmp, sizeof(tmp), "/bin/lld2d %s", lan_ifname);
     bcmSystem(tmp);
#endif
    bcmSystem("/bin/eapd");
    bcmSystem("/bin/nas");


#ifdef SUPPORT_WSC
        wlmngr_startWsc(idx);
#endif

#ifdef SUPPORT_SES    
   //restart SES
   wlmngr_startSes(idx);   
   wlmngr_startSesCl(idx);  
#endif
#ifdef BCMWAPI_WAI
    bcmSystem("wapid");
#endif
    wlmngr_BSDCtrl(idx);
    wlmngr_SSDCtrl(idx);
}
   
void wlmngr_issueServiceCmd(int idx)
{
#if defined(SUPPORT_WSC)  
    wlmngr_issueWpsCmd(idx);
#endif /* SUPPORT_WSC */
}

//**************************************************************************
// Function Name: getVar
// Description  : get value by variable name.
// Parameters   : var - variable name.
// Returns      : value - variable value.
//**************************************************************************
void wlmngr_getVar(int idx, char *varName, char *varValue) {
   int ssid_idx = m_instance_wl[idx].m_wlVar.wlSsidIdx;
   char *idxStr;
   char *inputVarName=varName;
   char varNameBuf[WL_MID_SIZE_MAX];
   char ifcName[WL_MID_SIZE_MAX];
#if defined(SUPPORT_WSC)
   char var_name[32];
   const char *tmp = NULL;
   int br=0;
   int i;
   int wpsDisable = 0;
#endif

#ifdef __CONFIG_VISUALIZATION__

   if ( strcmp(varName, "wlVisualization") == 0 ) 
   {
      sprintf(varValue, "%s", "1");
      goto getVar_done;
   }
#endif
/* to make it work with both IMPL22 and IMPL20, enable after IMPL22 */
#if defined(__CONFIG_HSPOT__) && defined(WL_IMPL_PLUS)

   if ( strcmp(varName, "wlPassPoint") == 0 ) 
   {
      sprintf(varValue, "%s", "1");
      goto getVar_done;
   }
#endif

   if ( strcmp(varName, "wlSyncNvram") == 0 ) 
   {
      wlmngr_NvramMapping(idx, MAP_FROM_NVRAM);
      sprintf(varValue, "%s", "1");
      goto getVar_done;
   }

   if(varName==NULL || *varName=='\0')
      printf("invalid variable");
  
   // swap varName buffer before processing
   strncpy(varNameBuf,varName,sizeof(varNameBuf));   
   varName = varNameBuf;  
   idxStr=strstr(varName, WL_PREFIX);
        
   if(idxStr) {
      //idxStr overrides idx
      ssid_idx = atoi(idxStr+strlen(WL_PREFIX)+2); /* wlXx */
      *idxStr = '\0';
   }

   if(idx >= WL_NUM_SSID)
    goto getVar_done;

#ifdef SUPPORT_WSC
           if (strcmp(varName, "wlWscAPMode") == 0)
           {
               br = m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlBrName[2] - 0x30;
               if ( br == 0 )
                  snprintf(var_name, sizeof(var_name), "lan_wps_oob");
              else
                   snprintf(var_name, sizeof(var_name), "lan%d_wps_oob", br);

              tmp = nvram_safe_get(var_name);
            
              sprintf(varValue, "%s", (strcmp(tmp, "enabled"))?"1":"0");
              goto getVar_done;
           }
          
           if (strcmp(varName, "wlWscMode") == 0)
           {
             /*WPS only support Adapter 0*/

              snprintf(var_name, sizeof(var_name), "%s_wps_mode", m_instance_wl[idx].m_ifcName[ssid_idx]);
              tmp = nvram_safe_get(var_name);
              sprintf(varValue, "%s", tmp);
                
              goto getVar_done;
           }

           if (strcmp(varName, "wlWscDevPin") == 0)
           {
              tmp = nvram_safe_get("wps_device_pin");
              sprintf(varValue, "%s", tmp);
              goto getVar_done;
           }

           if (strcmp(varName, "wlWscAvail") == 0)
           {
              sprintf(varValue, "%s", "yes");
              goto getVar_done;
           }

           if (strcmp(varName, "wlWscStaPin") == 0)
           {
              tmp = nvram_safe_get("wps_sta_pin");

              if (strcmp(tmp, "00000000") == 0)
                 *varValue = 0;
              else
                 sprintf(varValue, "%s", tmp);

              goto getVar_done;
           }
            
          if (strcmp(varName, "wlWscIRMode") == 0)
          {
              tmp = nvram_safe_get("wl_wps_reg");
              sprintf(varValue, "%s", tmp);
              goto getVar_done;
          }

          if (strcmp(varName, "wlWscAddER") == 0)
          {
              tmp = nvram_safe_get("wps_addER");
              sprintf(varValue, "%s", tmp);
              goto getVar_done;
          }

          if (strcmp(varName, "wlWscCfgMethod") == 0)
          {
              tmp = nvram_safe_get("_wps_config_method");
              sprintf(varValue, "%s", tmp);
              goto getVar_done;
          }

          if (strcmp(varName, "wlWscVer2") == 0)
          {
              tmp = nvram_safe_get("wps_version2");
              sprintf(varValue, "%s", tmp);
              goto getVar_done;
          }

          if (nvram_match("wps_version2", "enabled") && strcmp(varName, "wlWscAuthoStaMac") == 0)
          {
              tmp = nvram_safe_get("wps_autho_sta_mac");
              sprintf(varValue, "%s", tmp);
              goto getVar_done;
          }

          if(strcmp(varName, "wlScanResult") == 0 ) {   
              wlmngr_printScanResult(idx, varValue);
              goto getVar_done;
          }

          if(strcmp(varName, "wlWscIsEnrSta") == 0 ) {   
              if (wlmngr_isEnrSta(idx))  
                 sprintf(varValue, "%s", "1");
              else
                 sprintf(varValue, "%s", "0");

              goto getVar_done;
          }

          if(strcmp(varName, "wlWscIsEnrAp") == 0 ) {   
              if (wlmngr_isEnrAP(idx))  
                 sprintf(varValue, "%s", "1");
              else
                 sprintf(varValue, "%s", "0");

              goto getVar_done;
          }
#endif
          
   if ( strcmp(varName, "wlBssid") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].bssMacAddr[ssid_idx]);
   }
   else if ( strcmp(varName, "wlCountry") == 0 )
      strcpy(varValue, m_instance_wl[idx].m_wlVar.wlCountry);
   else if ( strcmp(varName, "wlRegRev") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRegRev);
   else if ( strcmp(varName, "wlWds0") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlVar.wlWds[0]);
      bcmProcessMarkStrChars(varValue);
   } else if ( strcmp(varName, "wlWds1") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlVar.wlWds[1]);
      bcmProcessMarkStrChars(varValue);
   } else if ( strcmp(varName, "wlWds2") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlVar.wlWds[2]);
      bcmProcessMarkStrChars(varValue);
   } else if ( strcmp(varName, "wlWds3") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlVar.wlWds[3]);
      bcmProcessMarkStrChars(varValue);
   } 
   else if ( strcmp(varName, "wlMode") == 0 )
      strcpy(varValue, m_instance_wl[idx].m_wlVar.wlMode);
   else if ( strcmp(varName, "wlLazyWds") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlLazyWds);
   else if ( strcmp(varName, "wlPreambleType") == 0 )
      strcpy(varValue, m_instance_wl[idx].m_wlVar.wlPreambleType);   
   else if ( strcmp(varName, "wlCoreRev") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlCoreRev);
   else if ( strcmp(varName, "wlChannel") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlChannel);
   else if(strcmp(varName, "wlCurrentChannel") == 0 ) {
      wlmngr_getCurrentChannel(idx);   	
      sprintf(varValue, "%d", m_instance_wl[idx].wlCurrentChannel);        
   }
   else if ( strcmp(varName, "wlFrgThrshld") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlFrgThrshld);
   else if ( strcmp(varName, "wlRtsThrshld") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRtsThrshld);
   else if ( strcmp(varName, "wlDtmIntvl") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlDtmIntvl);
   else if ( strcmp(varName, "wlBcnIntvl") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlBcnIntvl);
   else if ( strcmp(varName, "wlFrameBurst") == 0 )
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlFrameBurst);
   else if ( strcmp(varName, "wlRate") == 0 )
      sprintf(varValue, "%lu", m_instance_wl[idx].m_wlVar.wlRate);
   else if ( strcmp(varName, "wlPhyType") == 0 )
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlPhyType);
   else if ( strcmp(varName, "wlBasicRate") == 0 )
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlBasicRate);
   else if ( strcmp(varName, "wlgMode") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlgMode);
   else if ( strcmp(varName, "wlProtection") == 0 )
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlProtection);
   else if ( strcmp(varName, "wlRefresh") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_refresh);
   } else if ( strcmp(varName, "wlInterface") == 0 ) {
      bcmGetWlName(0, 0, ifcName);
      if ( bcmIsValidWlName(ifcName) == TRUE )
         strcpy(varValue, "1");
      else
         strcpy(varValue, "0");
   }
   else if ( strcmp(varName, "wlVersion") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].wlVer);
   }
   else if (strcmp(varName, "wlBand") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlBand);
   }
   else if (strcmp(varName, "wlMCastRate") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlMCastRate);
   }
   else if (strcmp(varName, "wlAfterBurnerEn") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlAfterBurnerEn);
   }
   else if (strcmp(varName, "wlBands") == 0 ) {
      sprintf(varValue, "%d", wlmngr_getBands(idx));
   }
   else if (strcmp(varName, "wlHasAfterburner") == 0 ) {
      sprintf(varValue, "%d",  m_instance_wl[idx].aburnSupported);
   }
#ifdef WMF
   else if (strcmp(varName, "wlHasWmf") == 0 ) {
      sprintf(varValue, "%d",  m_instance_wl[idx].wmfSupported);
   }
#endif
#ifdef DUCATI
   else if (strcmp(varName, "wlHasVec") == 0 ) {
      sprintf(varValue, "%d",  m_instance_wl[idx].wlVecSupported);
   }
   else if (strcmp(varName, "wlIperf") == 0) {
      sprintf(varValue, "%d",  m_instance_wl[idx].wlIperf);
   }
   else if (strcmp(varName, "wlVec") == 0) {
      sprintf(varValue, "%d",  m_instance_wl[idx].wlVec);
   }
#endif
   else if (strcmp(varName, "wlInfra") == 0 )
       sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlInfra);
   else if (strcmp(varName, "wlAntDiv") == 0 )
       sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAntDiv);
   else if ( strcmp(varName, "wlan_ifname") == 0 )
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlWlanIfName);
   else if ( strcmp(varName, "lan_hwaddr") == 0 ) {
      wlmngr_getHwAddr(idx, IFC_BRIDGE_NAME, varValue);
      bcmProcessMarkStrChars(varValue);
   }   
   else if (strcmp(varName, "wlHasWme") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].wmeSupported);
   else if(strcmp(varName, "wlWme") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlWme); 
   else if(strcmp(varName, "wlWmeNoAck") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlWmeNoAck);
   else if(strcmp(varName, "wlWmeApsd") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlWmeApsd);                   
   else if(strcmp(varName, "wlTxPwrPcnt") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlTxPwrPcnt);                 
   else if(strcmp(varName, "wlRegMode") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRegMode);
   else if(strcmp(varName, "wlDfsPreIsm") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlDfsPreIsm);
   else if(strcmp(varName, "wlDfsPostIsm") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlDfsPostIsm);
   else if(strcmp(varName, "wlTpcDb") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlTpcDb);                              
   else if(strcmp(varName, "wlInfo") == 0 )
      wlmngr_getWlInfo(idx, varValue, "wlcap");
   else if(strcmp(varName, "wlQosTbl") == 0 ) {
#ifdef WL_WLMNGR_DBG
    printf("%s::%s%d wlQosTbl\n", __FILE__, __FUNCTION__, __LINE__);
#endif
   }
   else if(strcmp(varName, "wlQosVars") == 0 ) {
#ifdef WL_WLMNGR_DBG
        printf("%s::%s%d wlQosvards\n", __FILE__, __FUNCTION__, __LINE__);
#endif
   }
   else if(strcmp(varName, "wlGlobalMaxAssoc") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc);       
   }   
#ifdef SUPPORT_SES               
   else if(strcmp(varName, "wlSesEnable") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlSesEnable); 
   else if(strcmp(varName, "wlSesEvent") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlSesEvent);
   else if(strcmp(varName, "wlSesAvail") == 0 )                
      strcpy(varValue, "1");            
   else if(strcmp(varName, "wlSesWdsMode") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlSesWdsMode); 
   else if(strcmp(varName, "wlSesClEnable") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlSesClEnable); 
   else if(strcmp(varName, "wlSesClEvent") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlSesClEvent);         
#endif   
#ifdef SUPPORT_MIMO
   else if(strcmp(varName, "wlNBwCap") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlNBwCap);       
   }
   else if(strcmp(varName, "wlNCtrlsb") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlNCtrlsb);       
   }   
   else if(strcmp(varName, "wlNBand") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlNBand);       
   }
   else if(strcmp(varName, "wlNMcsidx") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlNMcsidx);       
   }         
   else if(strcmp(varName, "wlNProtection") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlNProtection);       
   }
   else if(strcmp(varName, "wlRifs") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlRifs);       
   }
   else if(strcmp(varName, "wlAmpdu") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlAmpdu);       
   }
   else if(strcmp(varName, "wlAmsdu") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlAmsdu);       
   }
   else if(strcmp(varName, "wlNmode") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlNmode);       
   }    
   else if(strcmp(varName, "wlCurrentSb") == 0 ) {       
      int chanspec = wlmngr_getCurrentChSpec(idx);
      if (((chanspec & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT) == 3) {/* 40MHz */
          switch ((chanspec & WL_CHANSPEC_CTL_SB_MASK) >> WL_CHANSPEC_CTL_SB_SHIFT)	{
              case (WL_CHANSPEC_CTL_SB_LOWER >> WL_CHANSPEC_CTL_SB_SHIFT):
                  sprintf(varValue, "%s", "Lower");
                  break;

              case (WL_CHANSPEC_CTL_SB_UPPER >> WL_CHANSPEC_CTL_SB_SHIFT):
                  sprintf(varValue, "%s", "Upper");
                  break;

              default:
                  sprintf(varValue, "%s", "N/A");
                  break;
          }
      }	
      else
          sprintf(varValue, "%s", "N/A"); /* 20 and 80Mhz */

   }
   else if(strcmp(varName, "wlCurrentBw") == 0 ) {
      sprintf(varValue, "%d", ((wlmngr_getCurrentChSpec(idx) & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT));
   }
   else if(strcmp(varName, "wlNReqd") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlNReqd);       
   }   
#endif    
   else if ( strcmp(varName, "wlSsidRetrieve") == 0 ) {
      wlmngr_retrieve(idx, FALSE);
      strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[0].wlSsid);
      bcmProcessMarkStrChars(varValue);
   }
   // multiple SSID related
   else if(strcmp(varName, "wlMbssTbl") == 0 ) {   
      wlmngr_printMbssTbl(idx, varValue) ;
   }
   else if(strcmp(varName, "wlSupportMbss") == 0 ) {   
      sprintf(varValue, "%d", m_instance_wl[idx].mbssSupported);  
   }   
   else if ( strcmp(varName, "wlSsidIdx") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlSsidIdx);
   }
   else if ( strcmp(varName, "wlCurIdxIfcName") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_ifcName[ssid_idx]);
   }      
   else if ( strcmp(varName, "wlEnbl") == 0 ) {
      if(ssid_idx == MAIN_BSS_IDX)
         sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlEnbl);
      else
         sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlEnblSsid);      
   }   
   else if ( strcmp(varName, "wlEnbl_2") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[GUEST_BSS_IDX].wlEnblSsid);
   else if ( strcmp(varName, "wlSsid") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlSsid);
      bcmProcessMarkStrChars(varValue);
   }
   else if ( strcmp(varName, "wlSsid_2") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[GUEST_BSS_IDX].wlSsid);
      bcmProcessMarkStrChars(varValue);
   }
   else if ( strcmp(varName, "wlAuth") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuth);
   }
   else if ( strcmp(varName, "wlAuthMode") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode);
   }
   else if ( strcmp(varName, "wlKeyBit") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit);
   } 
   else if ( strcmp(varName, "wlWpaPsk") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpaPsk);
      bcmProcessMarkStrChars(varValue);
   } 
   else if ( strcmp(varName, "wlWpaGTKRekey") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpaGTKRekey);
   }
   else if ( strcmp(varName, "wlRadiusServerIP") == 0 ) {
      sprintf(varValue, "%s", inet_ntoa(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlRadiusServerIP));
   }
   else if ( strcmp(varName, "wlRadiusPort") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlRadiusPort);
   }
   else if ( strcmp(varName, "wlRadiusKey") == 0 ) {
      strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlRadiusKey);
      bcmProcessMarkStrChars(varValue);
   } 
   else if ( strcmp(varName, "wlWep") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWep);
   }
   else if ( strcmp(varName, "wlWpa") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpa);
   }
   else if ( strcmp(varName, "wlKeyIndex") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex64);
      else
         sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex128);
   }
   else if ( strcmp(varName, "wlKey1") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[0]);
      else
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[0]);
      bcmProcessMarkStrChars(varValue);
   } 
   else if ( strcmp(varName, "wlKey2") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[1]);
      else
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[1]);
      bcmProcessMarkStrChars(varValue);
   }
   else if ( strcmp(varName, "wlKey3") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[2]);
      else
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[2]);
      bcmProcessMarkStrChars(varValue);
   }
   else if ( strcmp(varName, "wlKey4") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[3]);
      else
         strcpy(varValue, m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[3]);
      bcmProcessMarkStrChars(varValue);
   }
   else if(strcmp(varName, "wlPreauth") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlPreauth);
   else if(strcmp(varName, "wlNetReauth") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlNetReauth);
   else if(strcmp(varName, "wlMFP") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlMFP);
   else if(strcmp(varName, "wlSsdType") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlSsdType);
   else if(strcmp(varName, "wlSsidList") == 0 ) {
      wlmngr_printSsidList(idx, varValue);
   }
   else if ( strcmp(varName, "wlFltMacMode") == 0 )
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlFltMacMode);   
   else if (strcmp(varName, "wlAPIsolation") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAPIsolation);       
   }   
   else if ( strcmp(varName, "wlHide") == 0 )
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlHide);     
#ifdef SUPPORT_WSC
   else if (nvram_match("wps_version2", "enabled") && (strcmp(varName, "wlIsForceWpsDisable") == 0)) {
      /* Checke Hidden Access point: if there is any of Hiden(include virtual), wpsDisable=1  */
      for (i=MAIN_BSS_IDX; i<WL_NUM_SSID; i++) {
         if ((m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid == 1) &&
          (m_instance_wl[idx].m_wlMssidVar[i].wlHide == 1)) {
            wpsDisable = 1;
            break;
         }
      }

      /* Checke Mac filter: if 'Allow' is choosed and no any MAC address is in filter list, wpsDisable=1 */
      if (!wpsDisable) {
         if ((strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlFltMacMode, "allow") == 0) && BcmWl_isMacFltEmpty())
            wpsDisable = 1;
      }
      sprintf(varValue, "%d", wpsDisable);
   }
#endif
   else if(strcmp(varName, "wlMaxAssoc") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlMaxAssoc);       
   }
   else if(strcmp(varName, "wlDisableWme") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlDisableWme);       
   }     
#ifdef WMF
   else if(strcmp(varName, "wlEnableWmf") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlEnableWmf);       
   }     
#endif
   else if(strcmp(varName, "wlHasApsta") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].apstaSupported);         	
   }
   else if(strcmp(varName, "wlEnableUre") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlEnableUre);       
   }
   else if ( strcmp(varName, "wlEnableHspot") == 0 ) {
#if defined(WL_IMPL_PLUS) || !defined(HSPOT_SUPPORT)
      sprintf(varValue, "%d", WLHSPOT_NOT_SUPPORTED);
#else
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlEnableHspot);
#endif
   }
   else if( strcmp(varName, "wlTXBFCapable") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlTXBFCapable); 
    }
   else if( strcmp(varName, "wlEnableBFR") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlEnableBFR);
   }
   else if( strcmp(varName, "wlEnableBFE") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlEnableBFE);
   }
   else if ( strcmp(varName, "bsdRole") == 0 ) {
     sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.bsdRole);
   } 
   else if ( strcmp(varName, "bsdHport") == 0 ) {
     sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.bsdHport);
   } 
   else if ( strcmp(varName, "bsdPport") == 0 ) {
     sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.bsdPport);
   } 
   else if ( strcmp(varName, "bsdHelper") == 0 ) {
     sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.bsdHelper);
   } 
   else if ( strcmp(varName, "bsdPrimary") == 0 ) {
     sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.bsdPrimary);
   } 
   else if ( strcmp(varName, "ssdEnable") == 0 ) {
     sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.ssdEnable);
   } 
   else if ( strcmp(varName, "wlTafEnable") == 0 ) {
     sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlTafEnable);
   } 
   else if ( strcmp(varName, "wlAtf") == 0 ) {
     sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAtf);
   } 
   else if ( strcmp(varName, "wlPspretendThreshold") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlPspretendThreshold);
   } 
   else if ( strcmp(varName, "wlPspretendRetryLimit") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlPspretendRetryLimit);
   } 
   else if ( strcmp(varName, "wlAcsFcsMode") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsFcsMode);
   } 
   else if ( strcmp(varName, "wlAcsDfs") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsDfs);
   } 
   else if ( strcmp(varName, "wlAcsCsScanTimer") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsCsScanTimer);
   } 
   else if ( strcmp(varName, "wlAcsCiScanTimer") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsCiScanTimer);
   } 
   else if ( strcmp(varName, "wlAcsCiScanTimeout") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsCiScanTimeout);
   } 
   else if ( strcmp(varName, "wlAcsScanEntryExpire") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsScanEntryExpire);
   } 
   else if ( strcmp(varName, "wlAcsTxIdleCnt") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsTxIdleCnt);
   } 
   else if ( strcmp(varName, "wlAcsChanDwellTime") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsChanDwellTime);
   } 
   else if ( strcmp(varName, "wlAcsChanFlopPeriod") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlAcsChanFlopPeriod);
   } 
   else if ( strcmp(varName, "wlIntferPeriod") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlIntferPeriod);
   } 
   else if ( strcmp(varName, "wlIntferCnt") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlIntferCnt);
   } 
   else if ( strcmp(varName, "wlIntferTxfail") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlIntferTxfail);
   } 
   else if ( strcmp(varName, "wlIntferTcptxfail") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlIntferTcptxfail);
   } 
   else if ( strcmp(varName, "wlAcsDfsrImmediate") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlAcsDfsrImmediate);
   } 
   else if ( strcmp(varName, "wlAcsDfsrDeferred") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlAcsDfsrDeferred);
   } 
   else if ( strcmp(varName, "wlAcsDfsrActivity") == 0 ) {
      sprintf(varValue, "%s", m_instance_wl[idx].m_wlVar.wlAcsDfsrActivity);
   } 
   else if(strcmp(varName, "wlStaRetryTime") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlStaRetryTime);       
   }     
   else if(strcmp(varName, "wlInstance_id") == 0 ) {
      sprintf(varValue, "%d", idx);       
   }   
   else if(strcmp(varName, "wlMaxMbss") == 0 ) {
      sprintf(varValue, "%d", m_instance_wl[idx].maxMbss);       
   }    
   else if (strcmp(varName, "wlChanImState") == 0) {
      sprintf(varValue, "%d", wlmngr_getChannelImState(idx));
   }
   else if (strcmp(varName, "wlChanImEnab") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlChanImEnab);
   }
   else if (strcmp(varName, "wlRifsAdvert") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRifsAdvert);
   }
   else if (strcmp(varName, "wlObssCoex") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlObssCoex);
   }
   else if (strcmp(varName, "wlRxChainPwrSaveEnable") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveEnable);
   }
   else if (strcmp(varName, "wlRxChainPwrSaveQuietTime") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveQuietTime);
   }
   else if (strcmp(varName, "wlRxChainPwrSavePps") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRxChainPwrSavePps);
   }
   else if (strcmp(varName, "wlRadioPwrSaveEnable") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRadioPwrSaveEnable);
   }
   else if (strcmp(varName, "wlRadioPwrSaveQuietTime") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRadioPwrSaveQuietTime);
   }
   else if (strcmp(varName, "wlRadioPwrSavePps") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRadioPwrSavePps);
   } 
   else if (strcmp(varName, "wlRadioPwrSaveLevel") == 0) {
      sprintf(varValue, "%d", m_instance_wl[idx].m_wlVar.wlRadioPwrSaveLevel);
   }
   else if(strcmp(varName, "wlWdsSec") == 0 ) {
      sprintf(varValue, "%d",  m_instance_wl[idx].m_wlVar.wlWdsSec);       
   }
   else if(strcmp(varName, "wlWdsSecEnable") == 0 ) {
      sprintf(varValue, "%d",  m_instance_wl[idx].m_wlVar.wlWdsSecEnable);       
   }       
   else if(strcmp(varName, "wlWdsKey") == 0 ) {
      sprintf(varValue, "%s",  m_instance_wl[idx].m_wlVar.wlWdsKey);       
   }
#ifndef BCMWAPI_WAI
   else if (strcmp(varName, "wlWapiAvail") == 0 )
      strcpy(varValue, "0");
#else
   else if (strcmp(varName, "wlWapiAvail") == 0 )
      strcpy(varValue, "1");
   else if (strcmp(varName, "wlWapiAsEnable") == 0 )
      sprintf(varValue, "%d", BcmWapi_AsStatus());
   else if (strcmp(varName, "wlWapiAsPending") == 0 )
      sprintf(varValue, "%d", BcmWapi_AsPendingStatus());
   else if (strcmp(varName, "wlWapiCertListFull") == 0 )
      sprintf(varValue, "%d", BcmWapi_CertListFull());
   else if (strcmp(varName, "wlWapiRevokeListFull") == 0 )
      sprintf(varValue, "%d", BcmWapi_RevokeListFull());
   else if (strcmp(varName, "wlWapiApCertStatus") == 0)
      BcmWapi_ApCertStatus(m_instance_wl[idx].m_ifcName[ssid_idx], varValue);
#endif
    else if (strcmp(varName, "wlpwrsave") == 0) {
      wlmngr_getPwrSaveStatus(idx, varValue);	 
   } 
   else {
      strcpy(varValue, "");
   }

getVar_done:
   // swap back original varName
   varName = inputVarName;
}

//**************************************************************************
// Function Name: getVar
// Description  : get value with full input params.
//**************************************************************************
#define MAXVAR 10

void wlmngr_getVarEx(int idx, int argc, char **argv, char *varValue){

   char *var = argv[1];   

#ifdef SUPPORT_SES    
   //map settings from  nvram interface
   wlmngr_NvramMapping(idx, MAP_FROM_NVRAM);
#endif

#ifdef SUPPORT_MIMO
   if (!strcmp(var, "wlNPhyRates"))
      wlmngr_getNPhyRates(idx, argc, argv, varValue);
   else
#endif   
   if (!strcmp(var, "wlChannelList"))
      wlmngr_getChannelList(idx, argc, argv, varValue);
   else if (!strcmp(var, "wlCountryList"))
      wlmngr_getCountryList(idx, argc, argv, varValue);  
   return;
}
   
//**************************************************************************
// Function Name: setVar
// Description  : set value by variable name.
// Parameters   : var - variable name.
// Returns      : value - variable value.
//**************************************************************************
/* wl_add_var_check */
void wlmngr_setVar(int idx, char *varName, char *varValue) {
   int ssid_idx = m_instance_wl[idx].m_wlVar.wlSsidIdx;
   char *idxStr;
   char *inputVarName=varName;
   char varNameBuf[WL_MID_SIZE_MAX];
   int i;
#if defined(SUPPORT_WSC)
    char lan[32];
    int br_tmp, br=0, cnt;
    char *methodValue;
    unsigned short config_method;
#endif


#ifdef WL_WLMNGR_DBG
    printf("%s (%s = %s)\n", __FUNCTION__, varName, varValue); 
#endif

#ifdef SUPPORT_WSC
           if (strcmp(varName, "wlWscMode") == 0)
           {
              if (varValue != NULL) {
                 strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wsc_mode, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wsc_mode));
              }
           }

           if (strcmp(varName, "wlWscAPMode") == 0)
           {
              if (varValue != NULL) {
                    strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wsc_config_state, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wsc_config_state));
                    br_tmp = m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlBrName[2] - 0x30;

                    if ( br_tmp == 0 )
                             snprintf(lan, sizeof(lan), "lan_wps_oob");
                    else
                             snprintf(lan, sizeof(lan), "lan%d_wps_oob", br_tmp);
                    if ( !strcmp(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wsc_config_state,"0"))
                             nvram_set(lan, "enabled");
                    else
                             nvram_set(lan, "disabled");
                    /* for each adapter */
                   for (cnt=0; cnt<wl_cnt; cnt++) {
                        /* for each intf */ 
                        for (i = 0 ; i<WL_NUM_SSID; i++) {
                             br = m_instance_wl[cnt].m_wlMssidVar[i].wlBrName[2] - 0x30;
                             if ( br == br_tmp)
                                 strncpy(m_instance_wl[cnt].m_wlMssidVar[i].wsc_config_state, varValue, sizeof(m_instance_wl[cnt].m_wlMssidVar[i].wsc_config_state));
                       }
                   }
              }
           }

           if (strcmp(varName, "wsc_event") == 0)
           {
             if (varValue != NULL) {
                 nvram_set("wps_event", varValue);
              }
           }

           if (strcmp(varName, "wsc_config_command") == 0)
           {
             if (varValue != NULL) {
		   if ( *varValue == '1' ) {// restart WPS process
	                nvram_set("wps_config_command", "2");
		   	  usleep(500000);
		   }
 		   	
                 nvram_set("wps_config_command", varValue);
	          }
           }
           if (strcmp(varName, "wsc_force_restart") == 0)
           {
             if (varValue != NULL) {
                 nvram_set("wps_force_restart", varValue);
              }
           }

           if (strcmp(varName, "wsc_method") == 0)
           {
             if (varValue != NULL) {
                 nvram_set("wps_method", varValue);
              }
           }

           if (strcmp(varName, "wsc_addER") == 0)
           {
             if (varValue != NULL) {
                 nvram_set("wps_addER", varValue);
              }
           }
           
           if (strcmp(varName, "wsc_proc_status") == 0)
           {
             if (varValue != NULL) {
                 nvram_set("wps_proc_status", varValue);
              }
           }

          if (strcmp(varName, "wlWscIRMode") == 0)
          {
             if (varValue != NULL) {
                 nvram_set("wl_wps_reg", varValue);
                }
          }
           if (strcmp(varName, "wlWscDevPin") == 0)
           {
              if ((varValue != NULL) && (strlen(varValue) == 8))
                 nvram_set("wps_device_pin", varValue);
              else
                 nvram_unset("wps_device_pin");
           }
 
           if (nvram_match("wps_version2", "enabled") && strcmp(varName, "wlWscAuthoStaMac") == 0)
           {
              if (varValue != NULL) 
              {
                 nvram_set("wps_autho_sta_mac", varValue);
              }
           }

           if (strcmp(varName, "wlWscStaPin") == 0)
           {
               if ((varValue != NULL) && (((strlen(varValue) == 8) || (strlen(varValue) == 4)) ||
                   (nvram_match("wps_version2", "enabled") && (strlen(varValue) == 9) && (varValue[4] < '0' || varValue[4] > '9'))))
               { // validate the pin
                   /* add in PF#3, session 7.4.3 "1234-5670" */
                   if (strlen(varValue) == 9) {
                      char pin[8];

                      /* remove '-' */
                      memcpy(pin, varValue+5, 5);
                      memcpy(varValue+4, pin, 5);                   
                   }
                   
                   if ( wlmngr_wscPincheck(varValue)) 
                        nvram_set("wps_sta_pin", "ErrorPin");
                   else 
                        nvram_set("wps_sta_pin", varValue);
            }
            else {
		     if ( strlen(varValue) == 0 )
	                nvram_set("wps_sta_pin", "00000000");
		     else	  	
	                nvram_set("wps_sta_pin", "");
               }
           }

            if (strcmp(varName, "wlWscConfig") == 0)
           {
              if ((varValue != NULL) && (strlen(varValue) > 0))  {
                 nvram_set("wps_config", varValue);
              }
              else {
                 nvram_unset("wps_config");
              }
           }

           if (strcmp(varName, "wlWscCfgMethod") == 0)
           {
              bool cfg_changed = FALSE;

              if ((varValue != NULL) && (strlen(varValue) > 0))  {
                  nvram_set("_wps_config_method", varValue);                 
                  methodValue = nvram_safe_get("wps_config_method");
                  config_method =  strtoul(methodValue, NULL, 16);
                  if (!strcmp(varValue, "ap-pin")) {
                      /* WPS_CONFMET_PBC = 0x0080 */
                      if ((config_method & 0x80) == 0x80) {
                          nvram_set("wps_config_method", "0x4");
                          cfg_changed = TRUE;
                      }
                  } else {
                      if ((config_method & 0x80) != 0x80) {
                          set_wps_config_method_default();
                          cfg_changed = TRUE;
                      }
                  }
              }

              if (cfg_changed == TRUE) { /* need to restart wps_monitor to take effect */
                  bcmSystem("killall -q -15 wps_monitor 2>/dev/null");
                  sleep(1);
                  bcmSystem("/bin/wps_monitor&");
              }
           }

           if (strcmp(varName, "wlWscApListIdx") == 0)
           {
              if (varValue != NULL) 
              {
                 wlmngr_saveApInfoFromScanResult(atoi(varValue));
              }
           }

           if (strcmp(varName, "wlWscApPinGetCfg") == 0)
           {
              if (varValue != NULL) 
              {
                 nvram_set("wps_ApPinGetCfg", varValue);
              }
           }

           if (strcmp(varName, "wlWpsApScan") == 0)
           {
              if (varValue != NULL) 
              {
                 wlmngr_doWpsApScan();
              }
           }
#endif

   // swap varName buffer before processing
   strncpy(varNameBuf,varName,sizeof(varNameBuf));   
   varName = varNameBuf;  
   idxStr=strstr(varName, WL_PREFIX);
   
   // get virtual interface name, idxStr in format _wlXvY 
   if(idxStr) {
    //idxStr overrides idx
    ssid_idx = atoi(idxStr+strlen(WL_PREFIX)+2); /* wlXx */
    *idxStr = '\0';
   }
   
   if(ssid_idx >= WL_NUM_SSID)
    goto setVar_done;
            
   if ( strcmp(varName, "wlCountry") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlCountry, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlCountry));
   else if ( strcmp(varName, "wlRegRev") == 0 )
      m_instance_wl[idx].m_wlVar.wlRegRev = atoi(varValue);
   else if ( strcmp(varName, "wlWds0") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlWds[0], varValue, sizeof(m_instance_wl[idx].m_wlVar.wlWds[0]));
   else if ( strcmp(varName, "wlWds1") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlWds[1], varValue, sizeof(m_instance_wl[idx].m_wlVar.wlWds[1]));
   else if ( strcmp(varName, "wlWds2") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlWds[2], varValue, sizeof(m_instance_wl[idx].m_wlVar.wlWds[2]));
   else if ( strcmp(varName, "wlWds3") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlWds[3], varValue, sizeof(m_instance_wl[idx].m_wlVar.wlWds[3]));
   else if ( strcmp(varName, "wlMode") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlMode, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlMode));
   else if ( strcmp(varName, "wlLazyWds") == 0 )
      m_instance_wl[idx].m_wlVar.wlLazyWds = atoi(varValue);
   else if ( strcmp(varName, "wlPreambleType") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlPreambleType, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlPreambleType));
   else if ( strcmp(varName, "wlChannel") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlChannel = atoi(varValue);
   } else if ( strcmp(varName, "wlFrgThrshld") == 0 )
      m_instance_wl[idx].m_wlVar.wlFrgThrshld = atoi(varValue);
   else if ( strcmp(varName, "wlRtsThrshld") == 0 )
      m_instance_wl[idx].m_wlVar.wlRtsThrshld = atoi(varValue);
   else if ( strcmp(varName, "wlDtmIntvl") == 0 )
      m_instance_wl[idx].m_wlVar.wlDtmIntvl = atoi(varValue);
   else if ( strcmp(varName, "wlBcnIntvl") == 0 )
      m_instance_wl[idx].m_wlVar.wlBcnIntvl = atoi(varValue);
   else if ( strcmp(varName, "wlFrameBurst") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlFrameBurst, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlFrameBurst));           
   else if ( strcmp(varName, "wlRate") == 0 )
      m_instance_wl[idx].m_wlVar.wlRate = atol(varValue);
   else if ( strcmp(varName, "wlPhyType") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlPhyType, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlPhyType));
   else if ( strcmp(varName, "wlBasicRate") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlBasicRate, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlBasicRate));
   else if ( strcmp(varName, "wlgMode") == 0 )
      m_instance_wl[idx].m_wlVar.wlgMode = atoi(varValue);
   else if ( strcmp(varName, "wlProtection") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlProtection, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlProtection));
   else if ( strcmp(varName, "wlRefresh") == 0 ) {
      m_instance_wl[idx].m_refresh = atoi(varValue);
   }
   else if (strcmp(varName, "wlBand") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlBand = atoi(varValue);
   }
   else if (strcmp(varName, "wlMCastRate") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlMCastRate = atoi(varValue);
   }
   else if (strcmp(varName, "wlAfterBurnerEn") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlVar.wlAfterBurnerEn, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlAfterBurnerEn));
   }
   else if (strcmp(varName, "wlInfra") == 0 )
      m_instance_wl[idx].m_wlVar.wlInfra = atoi(varValue);
   else if (strcmp(varName, "wlAntDiv") == 0 )
      m_instance_wl[idx].m_wlVar.wlAntDiv = atoi(varValue);
   else if ( strcmp(varName, "wlan_ifname") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlWlanIfName, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlWlanIfName));
   else if (strcmp(varName, "wlWme") == 0 )
      m_instance_wl[idx].m_wlVar.wlWme = atoi(varValue);  
   else if (strcmp(varName, "wlWmeNoAck") == 0 )
      m_instance_wl[idx].m_wlVar.wlWmeNoAck = atoi(varValue);  
   else if (strcmp(varName, "wlWmeApsd") == 0 )
      m_instance_wl[idx].m_wlVar.wlWmeApsd = atoi(varValue);                  
   else if (strcmp(varName, "wlTxPwrPcnt") == 0 )
      m_instance_wl[idx].m_wlVar.wlTxPwrPcnt = atoi(varValue); 
   else if (strcmp(varName, "wlRegMode") == 0 )
      m_instance_wl[idx].m_wlVar.wlRegMode = atoi(varValue);
   else if (strcmp(varName, "wlDfsPreIsm") == 0 )
      m_instance_wl[idx].m_wlVar.wlDfsPreIsm = atoi(varValue);
   else if (strcmp(varName, "wlDfsPostIsm") == 0 )
      m_instance_wl[idx].m_wlVar.wlDfsPostIsm = atoi(varValue);
   else if (strcmp(varName, "wlTpcDb") == 0 )
      m_instance_wl[idx].m_wlVar.wlTpcDb = atoi(varValue);
   else if (strcmp(varName, "wlGlobalMaxAssoc") == 0 )
      m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc = atoi(varValue);        
#ifdef SUPPORT_SES      
   else if (strcmp(varName, "wlSesEnable") == 0 )   	
      m_instance_wl[idx].m_wlVar.wlSesEnable = atoi(varValue);       
   else if (strcmp(varName, "wlSesEvent") == 0 )
      m_instance_wl[idx].m_wlVar.wlSesEvent = atoi(varValue);
   else if (strcmp(varName, "wlSesWdsMode") == 0 )   	
      m_instance_wl[idx].m_wlVar.wlSesWdsMode = atoi(varValue);       
   else if (strcmp(varName, "wlSesClEnable") == 0 )   	
      m_instance_wl[idx].m_wlVar.wlSesClEnable = atoi(varValue);       
   else if (strcmp(varName, "wlSesClEvent") == 0 )   	
      m_instance_wl[idx].m_wlVar.wlSesClEvent = atoi(varValue);       
#endif       
#ifdef SUPPORT_MIMO
   else if (strcmp(varName, "wlNBwCap") == 0 )
      m_instance_wl[idx].m_wlVar.wlNBwCap = atoi(varValue);
   else if (strcmp(varName, "wlNCtrlsb") == 0 )
      m_instance_wl[idx].m_wlVar.wlNCtrlsb = atoi(varValue);
   else if (strcmp(varName, "wlNBand") == 0 )
      m_instance_wl[idx].m_wlVar.wlNBand = atoi(varValue);
   else if (strcmp(varName, "wlNMcsidx") == 0 )
      m_instance_wl[idx].m_wlVar.wlNMcsidx = atoi(varValue); 
   else if ( strcmp(varName, "wlNProtection") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlNProtection, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlNProtection));
   else if ( strcmp(varName, "wlRifs") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlRifs, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlRifs));
   else if ( strcmp(varName, "wlAmsdu") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlAmsdu, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlAmsdu));
   else if ( strcmp(varName, "wlAmpdu") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlAmpdu, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlAmpdu));
   else if ( strcmp(varName, "wlNmode") == 0 )
      strncpy(m_instance_wl[idx].m_wlVar.wlNmode, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlNmode));
   else if (strcmp(varName, "wlNReqd") == 0 )
      m_instance_wl[idx].m_wlVar.wlNReqd = atoi(varValue);      
#endif        
   // multiple SSID related
   else if ( strcmp(varName, "wlSsidIdx") == 0 ) {
      char tmp[32];
      int sidx = m_instance_wl[idx].m_wlVar.wlSsidIdx = atoi(varValue);
      strncpy(tmp, &m_instance_wl[idx].m_ifcName[sidx][2], 4);
      nvram_set("wl_unit", tmp);
   }
   else if ( strcmp(varName, "wlEnbl") == 0 ) {
       if(ssid_idx == MAIN_BSS_IDX)
            m_instance_wl[idx].m_wlVar.wlEnbl = atoi(varValue);
       m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlEnblSsid = atoi(varValue);       
   }
   else if ( strcmp(varName, "wlEnbl_2") == 0 )
      m_instance_wl[idx].m_wlMssidVar[GUEST_BSS_IDX].wlEnblSsid = atoi(varValue);

   else if ( strcmp(varName, "wlSsid") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlSsid, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlSsid));
   }
   else if ( strcmp(varName, "wlSsid_2") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlMssidVar[GUEST_BSS_IDX].wlSsid, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[GUEST_BSS_IDX].wlSsid));
   }
   else if ( strcmp(varName, "wlKey1") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[0], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[0]));
      else
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[0], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[0]));
   } else if ( strcmp(varName, "wlKey2") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[1], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[1]));
      else
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[1], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[1]));
   } else if ( strcmp(varName, "wlKey3") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[2], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[2]));
      else
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[2], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[2]));
   } else if ( strcmp(varName, "wlKey4") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[3], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[3]));
      else
         strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[3], varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[3]));
    }
   else if ( strcmp(varName, "wlWpaPsk") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpaPsk, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpaPsk));
   }
   else if ( strcmp(varName, "wlWpaGtkRekey") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpaGTKRekey = atoi(varValue);
   else if ( strcmp(varName, "wlRadiusServerIP") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlRadiusServerIP.s_addr = inet_addr(varValue);
   else if ( strcmp(varName, "wlRadiusPort") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlRadiusPort = atoi(varValue);
   else if ( strcmp(varName, "wlRadiusKey") == 0 )
      strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlRadiusKey, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlRadiusKey));
   else if ( strcmp(varName, "wlWpa") == 0 ) {
      if ( strncmp(varValue, "tkip aes", sizeof("tkip aes")) == 0)
          varValue[4] = '+';
      strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpa, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWpa));
   } 
   else if ( strcmp(varName, "wlWep") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWep, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlWep));
   }
   else if ( strcmp(varName, "wlAuth") == 0 ) {
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuth = atoi(varValue);
   }
   else if ( strcmp(varName, "wlKeyIndex") == 0 ) {
      if ( m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
         m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex64 = atoi(varValue);
      else
         m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex128 = atoi(varValue);
    }
   else if ( strcmp(varName, "wlAuthMode") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode, varValue, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAuthMode));
   }
   else if ( strcmp(varName, "wlKeyBit") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit = atoi(varValue);
   else if (strcmp(varName, "wlPreauth") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlPreauth = atoi(varValue);                
   else if (strcmp(varName, "wlNetReauth") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlNetReauth = atoi(varValue);
   else if (strcmp(varName, "wlMFP") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlMFP = atoi(varValue);
   else if (strcmp(varName, "wlSsdType") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlSsdType = atoi(varValue);
   else if ( strcmp(varName, "wlHide") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlHide = atoi(varValue);
   else if (strcmp(varName, "wlAPIsolation") == 0 ) {
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlAPIsolation = atoi(varValue);      
   }      
   else if (strcmp(varName, "wlFltMacMode") == 0 ) {
      snprintf(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlFltMacMode, sizeof(m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlFltMacMode), varValue);   	
      if(!m_instance_wl[idx].mbssSupported) {      	 
         for (i=0; i<m_instance_wl[idx].numBss; i++)	
            snprintf(m_instance_wl[idx].m_wlMssidVar[i].wlFltMacMode, sizeof(m_instance_wl[idx].m_wlMssidVar[i].wlFltMacMode), varValue);
      }        
   }       
   else if (strcmp(varName, "wlMaxAssoc") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlMaxAssoc = atoi(varValue);
   else if (strcmp(varName, "wlDisableWme") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlDisableWme = atoi(varValue);    
#ifdef WMF
   else if (strcmp(varName, "wlEnableWmf") == 0 )
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlEnableWmf = atoi(varValue);    
#endif
   else if (strcmp(varName, "wlEnableUre") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlEnableUre = atoi(varValue);    
   }
   else if ( strcmp(varName, "wlEnableHspot") == 0 ) {
#if defined(HSPOT_SUPPORT) || defined(WL_IMPL_PLUS)
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlEnableHspot = atoi(varValue);
#else
      m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlEnableHspot =WLHSPOT_NOT_SUPPORTED;
#endif
   }
   else if ( strcmp(varName, "wlEnableBFR") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlEnableBFR = atoi(varValue); 
   }
   else if ( strcmp(varName, "wlEnableBFE") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlEnableBFE = atoi(varValue);  
   }
   else if ( strcmp(varName, "bsdRole") == 0 ) {
      m_instance_wl[idx].m_wlVar.bsdRole = atoi(varValue);
   }
   else if ( strcmp(varName, "bsdHport") == 0 ) {
      m_instance_wl[idx].m_wlVar.bsdHport = atoi(varValue);
   }
   else if ( strcmp(varName, "bsdPport") == 0 ) {
      m_instance_wl[idx].m_wlVar.bsdPport = atoi(varValue);
   }
   else if ( strcmp(varName, "bsdHelper") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlVar.bsdHelper, varValue, sizeof(m_instance_wl[idx].m_wlVar.bsdHelper));
   }
   else if ( strcmp(varName, "bsdPrimary") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlVar.bsdPrimary, varValue, sizeof(m_instance_wl[idx].m_wlVar.bsdPrimary));
   }
   else if ( strcmp(varName, "ssdEnable") == 0 ) {
      m_instance_wl[idx].m_wlVar.ssdEnable = atoi(varValue);
   }
   else if ( strcmp(varName, "wlTafEnable") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlTafEnable = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAtf") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAtf = atoi(varValue);
   }
   else if ( strcmp(varName, "wlPspretendThreshold") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlPspretendThreshold = atoi(varValue);
   }
   else if ( strcmp(varName, "wlPspretendRetryLimit") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlPspretendRetryLimit = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsFcsMode") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsFcsMode = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsDfs") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsDfs = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsCsScanTimer") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsCsScanTimer = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsCiScanTimer") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsCiScanTimer = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsCiScanTimeout") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsCiScanTimeout = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsTxIdleCnt") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsTxIdleCnt = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsChanDwellTime") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsChanDwellTime = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsChanFlopPeriod") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlAcsChanFlopPeriod = atoi(varValue);
   }
   else if ( strcmp(varName, "wlIntferPeriod") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlIntferPeriod = atoi(varValue);
   }
   else if ( strcmp(varName, "wlIntferCnt") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlIntferCnt = atoi(varValue);
   }
   else if ( strcmp(varName, "wlIntferTxfail") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlIntferTxfail = atoi(varValue);
   }
   else if ( strcmp(varName, "wlIntferTcptxfail") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlIntferTcptxfail = atoi(varValue);
   }
   else if ( strcmp(varName, "wlAcsDfsrImmediate") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlVar.wlAcsDfsrImmediate, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlAcsDfsrImmediate));
   }
   else if ( strcmp(varName, "wlAcsDfsrDeferred") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlVar.wlAcsDfsrDeferred, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlAcsDfsrDeferred));
   }
   else if ( strcmp(varName, "wlAcsDfsrActivity") == 0 ) {
      strncpy(m_instance_wl[idx].m_wlVar.wlAcsDfsrActivity, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlAcsDfsrActivity));
   }
   else if (strcmp(varName, "wlStaRetryTime") == 0 ) {
      m_instance_wl[idx].m_wlVar.wlStaRetryTime = atoi(varValue);    
   }
#ifdef DUCATI
   else if (strcmp(varName, "wlIperf") == 0 )
      m_instance_wl[idx].wlIperf = atoi(varValue);    
   else if (strcmp(varName, "wlVec") == 0 )
      m_instance_wl[idx].wlVec = atoi(varValue);    
#endif
   else if (strcmp(varName, "wlChanImEnab") == 0) {
      m_instance_wl[idx].m_wlVar.wlChanImEnab = atoi(varValue);
   }
   else if (strcmp(varName, "wlRifsAdvert") == 0) {
      m_instance_wl[idx].m_wlVar.wlRifsAdvert= atoi(varValue);
   }
   else if (strcmp(varName, "wlObssCoex") == 0) {
      m_instance_wl[idx].m_wlVar.wlObssCoex = atoi(varValue);
   }
   else if (strcmp(varName, "wlRxChainPwrSaveEnable") == 0) {
      m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveEnable = atoi(varValue);
   }
   else if (strcmp(varName, "wlRxChainPwrSaveQuietTime") == 0) {
      m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveQuietTime = atoi(varValue);
   }
   else if (strcmp(varName, "wlRxChainPwrSavePps") == 0) {
      m_instance_wl[idx].m_wlVar.wlRxChainPwrSavePps = atoi(varValue);
   }
   else if (strcmp(varName, "wlRadioPwrSaveEnable") == 0) {
      m_instance_wl[idx].m_wlVar.wlRadioPwrSaveEnable = atoi(varValue);
   }
   else if (strcmp(varName, "wlRadioPwrSaveQuietTime") == 0) {
      m_instance_wl[idx].m_wlVar.wlRadioPwrSaveQuietTime = atoi(varValue);
   }
   else if (strcmp(varName, "wlRadioPwrSavePps") == 0) {
      m_instance_wl[idx].m_wlVar.wlRadioPwrSavePps = atoi(varValue);
   }
   else if (strcmp(varName, "wlRadioPwrSaveLevel") == 0) {
      m_instance_wl[idx].m_wlVar.wlRadioPwrSaveLevel = atoi(varValue);
   }
   else if (strcmp(varName, "wlWdsSec") == 0 )
      m_instance_wl[idx].m_wlVar.wlWdsSec = atoi(varValue); 
   else if (strcmp(varName, "wlWdsSecEnable") == 0 )
      m_instance_wl[idx].m_wlVar.wlWdsSecEnable = atoi(varValue);
   else if (strcmp(varName, "wlWdsKey") == 0 )
            strncpy( m_instance_wl[idx].m_wlVar.wlWdsKey, varValue, sizeof(m_instance_wl[idx].m_wlVar.wlWdsKey)); 
   else if(varValue)
      strcpy(varValue, "");

   //map settings to nvram interface
   if ( strcmp(varName, "wlSyncNvram") == 0 )
      wlmngr_NvramMapping(idx, MAP_TO_NVRAM);

setVar_done:
   // swap back original varName
   varName = inputVarName;
}

//**************************************************************************
// Function Name: wlmngr_getVarFromNvram
// Description  : retrieve var from nvram by name
// Parameters   : var, name, and type
// Returns      : none
//**************************************************************************
void wlmngr_getVarFromNvram(void *var, const char *name, const char *type)
{   	
    char temp_s[256] = {0};
    int len;

    strncpy(temp_s, nvram_safe_get(name), sizeof(temp_s));
    
    if(!strcmp(type,"int")){  
        if(*temp_s)  	        
           *(int*)var = atoi(temp_s);
        else
           *(int*)var = 0;
    }else if(!strcmp(type,"string")) {
        if(*temp_s)
        {
            len = strlen(temp_s);
            
            /* Don't truncate tail-space if existed for SSID string */
            if (strstr(name, "ssid") == NULL) {
                if ((strstr(name, "wpa_psk") == NULL) && (len > 0) && (temp_s[len - 1] == ' '))
                    temp_s[len - 1] = '\0';
            }
            strcpy((char*)var,temp_s);
        }
        else { 	
        *(char*)var = 0;
        }
    }else {
        printf("wlmngr_getVarFromNvram:type not found\n");	
    }		
}

//**************************************************************************
// Function Name: setVarFromNvram
// Description  : set var to nvram by name
// Parameters   : var, name, and type
// Returns      : none
//**************************************************************************
void wlmngr_setVarToNvram(void *var, const char *name, const char *type)
{   	
    char temp_s[100] = {0};
    int len;

    if(!strcmp(type,"int")){    
        snprintf(temp_s, sizeof(temp_s), "%d",*(int*)var);	        
        nvram_set(name, temp_s);
    } else if(!strcmp(type,"string") && ((!var) ||
                     ((!strcmp((char*)var,"")) || (!strncmp((char*)var,"*DEL*",5))))) {
        nvram_unset(name);
    } else if(!strcmp(type,"string")) {
        if (var != NULL)
           len = strlen(var);
        else
           len = 0;

        /* Don't truncate tail-space if existed for SSID string */
        if (strstr(name, "ssid") == NULL) {
            if ((strstr(name, "wpa_psk") == NULL) && (len > 0) && (((char*)var)[len - 1] == ' '))
                ((char *)var)[len - 1] = '\0';
        }
        nvram_set(name, (char*)var);
    }else {
        printf("wlmngr_setVarToNvram:type not found\n");	
    }		
}

#ifdef DSLCPE_1905
typedef struct struct_value {
	int offset;
	char *name;
	char isInt;
} STRUCT_VALUE;

/* check if configuration status changed */
static int _check_config_status_change (int idx,int *bssid, int *change_status)
{

#if defined(SUPPORT_WSC)
	int i=0;
	char buf[32],name[32];
	int br=0;

	for (i = 0 ; i<WL_NUM_SSID; i++) {
		br = m_instance_wl[idx].m_wlMssidVar[i].wlBrName[2] - 0x30;

		/* Set the OOB state */
		if ( br == 0 )
			snprintf(name, sizeof(name), "lan_wps_oob");
		else
			snprintf(name, sizeof(name), "lan%d_wps_oob", br);
		strncpy(buf, nvram_safe_get(name), sizeof(buf));
		if ( buf[0]=='\0')
			continue;

		if (!strcmp(buf,"enabled")) {
			sprintf(name,"%d",0);
		}	
		else {
			sprintf(name,"%d",1);
		}

		if(strcmp(m_instance_wl[idx].m_wlMssidVar[i].wsc_config_state, name)) {
			*bssid=i;
			if(!strcmp(name,"0"))  *change_status= WPS_1905_CONF_TO_UNCONF;
			else *change_status= WPS_1905_UNCONF_TO_CONF;
			return 1;
		} else {
			if(!strcmp(name,"0"))  *change_status= WPS_1905_CONF_NOCHANGE_UNCONFIGURED;
			else *change_status= WPS_1905_CONF_NOCHANGE_CONFIGURED;
		}

	}
#endif
	return 0;
}
/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  wlmngr_checkCredentialChange
 *  Description:  checking if wlan credential changed
 * =====================================================================================
 */ 
int wlmngr_checkCredentialChange (int idx, int *bssid, int *conf_status)
{
	char *str=NULL;
	int i=0;
	char name[32],tmp[100];
	int ret=0;
	STRUCT_VALUE items[]= { 
		{MSSID_VAROFF(wlSsid),	 	"ssid",		0},
		{MSSID_VAROFF(wlWpaPsk), 	"wpa_psk",	0},
		{MSSID_VAROFF(wlAuthMode), 	"akm",		0},
		{MSSID_VAROFF(wlAuth),		"auth",		1},
		{MSSID_VAROFF(wlWep), 		"wep",		0},
		{MSSID_VAROFF(wlWpa), 		"crypto", 	0},
		{-1,NULL}
	};

	STRUCT_VALUE *item=items;
	WIRELESS_MSSID_VAR *pmssid;

	*bssid = 0;
	ret =_check_config_status_change (idx,bssid,conf_status);
	if ( !ret ) {
		for (i = 0; i < WL_NUM_SSID; i++) {
			snprintf(name, sizeof(name), "%s_", m_instance_wl[idx].m_ifcName[i]);
			pmssid=(WIRELESS_MSSID_VAR *)(m_instance_wl[idx].m_wlMssidVar+i);
			if(pmssid->wlEnblSsid) {
				for(item=items;item->name!=NULL;item++) {
					str=nvram_get(strcat_r(name, item->name, tmp));
					/* for akm, it is kind of special*/
					if(!strcmp(item->name,"akm") && (str==NULL||strlen(str)==0)) {
						str="open";
					}
					if(!item->isInt) {
						char *value=MSSID_STRVARVALUE(pmssid->wlSsid,item->offset);
						if((str==NULL && strlen(value)!=0)|| (str!=NULL && strcmp(str,value))) {
							*bssid=i;
							ret= 1;
							break;
						}
					}
					else {
						int value=MSSID_INTVARVALUE(pmssid->wlSsid,item->offset);
						if(str!=NULL && atoi(str)!=value) {
							*bssid=i;
							ret= 1;
							break;
						}
					}
				}
			}
		}
	}

	return ret;
}		

static int open_udp_socket(char *addr, uint16 port)
{
	int reuse = 1;
	int sock_fd;
	struct sockaddr_in sockaddr;

	/*  open loopback socket to communicate with EAPD */
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = inet_addr(addr);
	sockaddr.sin_port = htons(port);

	if ((sock_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		cmsLog_debug( "Unable to create loopback socket\n");
		goto exit0;
	}

	if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		cmsLog_debug( "Unable to setsockopt to loopback socket %d.\n", sock_fd);
		goto exit1;
	}

	if (bind(sock_fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		cmsLog_debug( "Unable to bind to loopback socket %d\n", sock_fd);
		goto exit1;
	}
	cmsLog_debug( "opened loopback socket %d in port %d\n", sock_fd, port);
	return sock_fd;

	/*  error handling */
exit1:
	close(sock_fd);

exit0:
	cmsLog_debug( "failed to open loopback socket\n");
	return -1;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  wlmngr_send_notification_1905
 *  Description:  
 * =====================================================================================
 */

int wlmngr_send_notification_1905 (int idx,int bssid,int from_http,int credential_changed, int conf_status)
{
	struct sockaddr_in sockAddr;
	int rc;
	int port;
	char *portnum=nvram_safe_get("1905_com_socket");
	char nvramVar[32];

	rc = sscanf(portnum,"%d",&port);
	if ( rc == 1 ) {
		int sock_fd;
		char *value;

		sock_fd=open_udp_socket(WPS_1905_ADDR,WPS_1905_PORT);
		if(sock_fd>=0) {
			int buflen = sizeof(WPS_1905_MESSAGE) + sizeof(WPS_1905_NOTIFY_MESSAGE);
			WPS_1905_MESSAGE *pmsg = (WPS_1905_MESSAGE *)malloc(buflen);
			WPS_1905_NOTIFY_MESSAGE notify_msg;

			notify_msg.confStatus=conf_status;
			notify_msg.credentialChanged=credential_changed;
			snprintf(notify_msg.ifName, sizeof(notify_msg.ifName), "%s", m_instance_wl[idx].m_ifcName[bssid]);

			memset(pmsg,'\0',buflen);
			pmsg->cmd=WPS_1905_NOTIFY_CLIENT_RESTART;
			pmsg->len=sizeof(WPS_1905_NOTIFY_MESSAGE);
			pmsg->status=1;
			memcpy((char *)(pmsg+1),&notify_msg,sizeof(WPS_1905_NOTIFY_MESSAGE));

			/*  kernel address */
			memset(&sockAddr, 0, sizeof(sockAddr));
			sockAddr.sin_family      = AF_INET;
			sockAddr.sin_addr.s_addr = htonl(0x7f000001); /*  127.0.0.1 */
			sockAddr.sin_port        = htons(port);

			rc = sendto(sock_fd, pmsg, buflen, 0, (struct sockaddr *)&sockAddr,sizeof(sockAddr));
			close(sock_fd);
			if (buflen != rc) {
				printf("%s: sendto failed", __FUNCTION__);
				return 1;
			}
			else {
				return 0;
			}
		}
	}
	return 1;
}		/* -----  end of function wlmngr_send_notification_1905  ----- */

#endif

//**************************************************************************
// Function Name: wlmngr_NvramMapping
// Description  : retrieve/set nvram parameters for applications IPC
// Parameters   : dir (direction)
// Returns      : None
//**************************************************************************
void wlmngr_NvramMapping(int idx, const int dir)
{
#define MIMO_PHY ((!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_N)) || (!strcmp(m_instance_wl[idx].m_wlVar.wlPhyType, WL_PHY_TYPE_AC)))
    char tmp[100], tmp_s[256],prefix[] = "wlXXXXXXXXXX_";
    char name[32];
    int  maxMacFilterBlockSize= WL_MACFLT_NUM * WL_MACADDR_SIZE + 2; //! boundary issue
    char *tmp_s2= malloc(maxMacFilterBlockSize);

#ifdef WL_WLMNGR_DBG    
    char cmd[WL_LG_SIZE_MAX];
#endif
      
    int set;
    int i;

#if defined(SUPPORT_WSC)
    char lan[32], buf[128];
    int br, cnt, found, br_num;
#endif
    int val;

   /*re-read nvram*/
    nvram_set("wlmngr","re-read");
    memset(tmp_s2, 0, maxMacFilterBlockSize);

    if(dir != MAP_FROM_NVRAM && dir != MAP_TO_NVRAM)
        printf("wlmngr_NvramMapping: %d not supported\n",dir);

    set = (dir==MAP_TO_NVRAM)? 1:0;

    /*get nvram prefix*/
    snprintf(prefix, sizeof(prefix), "%s_", m_instance_wl[idx].m_ifcName[0]);

    /* phy type */
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlPhyType, strcat_r(prefix, "phytype", tmp), "string");    	    	
    /* phy type does not need to get back from nvram */
    /* Multi BSS */
    i = 0;

    if(set)
    {
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlEnbl,                        strcat_r(prefix, "radio", tmp), "int");
        for (i = 0; i < WL_NUM_SSID; i++) {
            snprintf(name, sizeof(name), "%s_", m_instance_wl[idx].m_ifcName[i]);
            if (!m_instance_wl[idx].m_wlVar.wlEnableUre) {
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlMode,                    strcat_r(name, "mode", tmp), "string");
            }
            wlmngr_setVarToNvram(&m_instance_wl[idx].bssMacAddr[i],                         strcat_r(name, "hwaddr", tmp), "string");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid,            strcat_r(name, "bss_enabled", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlSsid,                strcat_r(name, "ssid", tmp), "string");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlWpa,                 strcat_r(name, "crypto", tmp), "string");    
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlWpaPsk,              strcat_r(name, "wpa_psk", tmp), "string");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlRadiusKey,           strcat_r(name, "radius_key", tmp), "string");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlWpaGTKRekey,         strcat_r(name, "wpa_gtk_rekey", tmp), "int");
            wlmngr_setVarToNvram(inet_ntoa(m_instance_wl[idx].m_wlMssidVar[i].wlRadiusServerIP), strcat_r(name, "radius_ipaddr", tmp),"string");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlRadiusPort,          strcat_r(name, "radius_port", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlNetReauth,           strcat_r(name, "net_reauth", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlPreauth,             strcat_r(name, "preauth", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlMFP,                 strcat_r(name, "mfp", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlSsdType,             strcat_r(name, "ssd_type", tmp), "int");

#if defined(SUPPORT_WSC)
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wsc_mode,              strcat_r(name, "wps_mode", tmp), "string");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wsc_config_state,      strcat_r(name, "wps_config_state", tmp), "string");
#endif
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlMaxAssoc,            strcat_r(name, "bss_maxassoc", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlAPIsolation,         strcat_r(name, "ap_isolate", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlHide,                strcat_r(name, "closed", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlEnableWmf,           strcat_r(name, "wmf_bss_enable", tmp), "int");
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlDisableWme,          strcat_r(name, "wme_bss_disable", tmp), "int");

            /* Mac Filter */
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlFltMacMode,          strcat_r(name, "macmode", tmp), "string");
            wlmngr_getFltMacList(idx,i, tmp_s2, maxMacFilterBlockSize);
            wlmngr_setVarToNvram(tmp_s2, strcat_r(name, "maclist", tmp), "string"); 

            /* rxchain_pwrsave_enable */
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveEnable, strcat_r(name, "rxchain_pwrsave_enable", tmp), "int");
            /* rxchain_pwrsave_quiet_time */
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlRxChainPwrSaveQuietTime, strcat_r(name, "rxchain_pwrsave_quiet_time", tmp), "int");
            /* rxchain_pwrsave_pps */
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlRxChainPwrSavePps, strcat_r(name, "rxchain_pwrsave_pps", tmp), "int");
        }

#if defined(SUPPORT_WSC)
        /* Since 5.22.76 release, WPS OOB(Config/Unconfig) state is changed to per Bridge, nram variable lan_wps_oob 
           saves br0 oob state; lan1_wps_oob saves br1 OOB state. m_wlMssidVar[i].wsc_config_state stores intf i's OOB state. 
           So mapping between nvram parameter lan_wps_oob and intf structure m_wlMssidVar[i].wsc_config_state is not
           1-1 mapping. 
           Mapping from m_wlMssidVar[i].wsc_config_state to lan_wps_oob is to use one of intf's wsc_config_state, which 
           is in br0
           Mapping from lan_wps_oob is to change all the m_wlMssidVar[i].wsc_config_state 
           when m_wlMssidVar[i].wsc_config_state is in br0.
        */
        /* for each adapter */
	for(br_num=0; br_num<MAX_BR_NUM; br_num++) {
           found = 0;
           for (cnt=0; cnt<wl_cnt && !found; cnt++) {
              /* for each intf */ 
              for (i = 0 ; i<WL_NUM_SSID && !found; i++) {
                    br = m_instance_wl[cnt].m_wlMssidVar[i].wlBrName[2] - 0x30;
                    if (br_num != br)
                       continue;
                    else
                       found=1;

                    /* Set the OOB state */
                    if ( br == 0 )
                             snprintf(lan, sizeof(lan), "lan_wps_oob");
                    else
                             snprintf(lan, sizeof(lan), "lan%d_wps_oob", br);
					
                    if ( !strcmp(m_instance_wl[cnt].m_wlMssidVar[i].wsc_config_state,"0"))
                             nvram_set(lan, "enabled");
                    else
                             nvram_set(lan, "disabled");
              }
           }
      	}
#endif
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlEnbl,                      strcat_r(prefix, "radio", tmp), "int");
        for (i = 0; i < WL_NUM_SSID; i++) {
            snprintf(name, sizeof(name), "%s_", m_instance_wl[idx].m_ifcName[i]);
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlMode,                      strcat_r(name, "mode", tmp), "string");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].bssMacAddr[i],                       strcat_r(name, "hwaddr", tmp), "string");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlEnblSsid,          strcat_r(name, "bss_enabled", tmp), "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlSsid,              strcat_r(name, "ssid", tmp), "string");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlWpa,               strcat_r(name, "crypto", tmp), "string");    
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlWpaPsk,            strcat_r(name, "wpa_psk", tmp), "string");
		   
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlRadiusKey,         strcat_r(name, "radius_key", tmp),  "string");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlWpaGTKRekey,       strcat_r(name, "wpa_gtk_rekey", tmp),  "int");
            wlmngr_getVarFromNvram(tmp, strcat_r(name, "radius_ipaddr", tmp), "string");
      	    m_instance_wl[idx].m_wlMssidVar[i].wlRadiusServerIP.s_addr = inet_addr(tmp);
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlRadiusPort,        strcat_r(name, "radius_port", tmp),  "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlNetReauth,         strcat_r(name, "net_reauth", tmp),  "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlPreauth,           strcat_r(name, "preauth", tmp),  "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlMFP,               strcat_r(name, "mfp", tmp),  "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlSsdType,           strcat_r(name, "ssd_type", tmp),  "int");

#if defined(SUPPORT_WSC)
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wsc_mode,            strcat_r(name, "wps_mode", tmp), "string");
#endif
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlMaxAssoc,          strcat_r(name, "bss_maxassoc", tmp), "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlAPIsolation,       strcat_r(name, "ap_isolate", tmp), "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlHide,              strcat_r(name, "closed", tmp), "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlEnableWmf,         strcat_r(name, "wmf_bss_enable", tmp), "int");
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlDisableWme,        strcat_r(name, "wme_bss_disable", tmp), "int");

            /* Mac Filter */
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlFltMacMode,        strcat_r(name, "macmode", tmp), "string");
	    tmp_s2[0] = '\0';
	    strncpy(tmp_s2, nvram_safe_get(strcat_r(name, "maclist", tmp)), maxMacFilterBlockSize);
	    BcmWl_removeAllFilterMac(idx,i);
	    if(tmp_s2[0]!='\0'){
	          char* tokens;
		  int count=0;
		  tokens=strtok(tmp_s2, " ");
		  while(tokens!=NULL && count < WL_MACFLT_NUM){
		        BcmWl_addFilterMac2(tokens, m_instance_wl[idx].m_wlMssidVar[i].wlSsid, m_instance_wl[idx].m_ifcName[i], idx, i);
			tokens=strtok(NULL, " ");
			count++;
		  }
		  fprintf(stderr, "More than count=%d %d mac addresses will be dropped\n", count, WL_MACFLT_NUM);
		  memset(tmp_s2, 0, maxMacFilterBlockSize);
		  wlmngr_getFltMacList(idx,i, tmp_s2, maxMacFilterBlockSize);
		  wlmngr_setVarToNvram(tmp_s2, strcat_r(name, "maclist", tmp), "string"); 
	    }
	}

#if defined(SUPPORT_WSC)
	 /* for each adapter */
        for (cnt=0; cnt<wl_cnt; cnt++) {
              /* for each intf */ 
              for (i = 0 ; i<WL_NUM_SSID; i++) {
                    br = m_instance_wl[cnt].m_wlMssidVar[i].wlBrName[2] - 0x30;

                    /* Set the OOB state */
                    if ( br == 0 )
                             snprintf(lan, sizeof(lan), "lan_wps_oob");
                    else
                             snprintf(lan, sizeof(lan), "lan%d_wps_oob", br);
                    strncpy(buf, nvram_safe_get(lan), sizeof(buf));
		      if ( buf[0]=='\0')
			  	continue;
                    if (!strcmp(buf,"enabled"))
                           strcpy(m_instance_wl[cnt].m_wlMssidVar[i].wsc_config_state, "0");
                    else
                           strcpy(m_instance_wl[cnt].m_wlMssidVar[i].wsc_config_state, "1");
              }
      	}
#endif
    }

    /*auth_mode*//* Network auth mode (radius|none) */
    /*akm*//* WPA akm list (wpa|wpa2|psk|psk2) */

    if(set)
    {
        for (i = 0; i < WL_NUM_SSID; i++)
        {
            snprintf(name, sizeof(name), "%s_", m_instance_wl[idx].m_ifcName[i]);

            if(!strcmp(m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode, WL_AUTH_OPEN))
            {
                nvram_unset(strcat_r(name, "auth_mode", tmp));
                nvram_unset(strcat_r(name, "akm", tmp));
            } 
            else if(!strcmp(m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode, WL_AUTH_RADIUS))
            {
                strncpy(tmp_s, WL_AUTH_RADIUS, sizeof(tmp_s));
                wlmngr_setVarToNvram(tmp_s, strcat_r(name, "auth_mode", tmp), "string");  
                nvram_unset(strcat_r(name, "akm", tmp));
            }
            else
            {
                strncpy(tmp_s, "none", sizeof(tmp_s));
                wlmngr_setVarToNvram(tmp_s, strcat_r(name, "auth_mode", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode, strcat_r(name, "akm", tmp), "string");      	
            }
        }
    } 
    else
    {
        for (i = 0; i < WL_NUM_SSID; i++)
        {
            snprintf(name, sizeof(name), "%s_", m_instance_wl[idx].m_ifcName[i]);

            wlmngr_getVarFromNvram(tmp_s, strcat_r(name, "auth_mode", tmp), "string");           
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode, strcat_r(name, "akm", tmp), "string");

            if (!strcmp(tmp_s, WL_AUTH_RADIUS))
                strncpy(m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode, WL_AUTH_RADIUS, sizeof(m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode));
          
            if (m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode[0] == '\0')
                strncpy(m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode,WL_AUTH_OPEN, sizeof(m_instance_wl[idx].m_wlMssidVar[i].wlAuthMode));
        }
    }

#ifdef WL_WLMNGR_NVRAM_DBG   
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ssid=%s", m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlSsid);
    bcmSystemMute(cmd);
#endif   
   
   /*ses_ssid*/
#ifdef SUPPORT_SES   
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlSesSsid, strcat_r(prefix, "ses_ssid", tmp), "string");    	    	
    else 
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesSsid, strcat_r(prefix, "ses_ssid", tmp), "string");
#endif
      
   /*closed*//* network type (0|1) */
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlHide, strcat_r(prefix, "closed", tmp), "int");
    else    
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlHide, strcat_r(prefix, "closed", tmp), "int");

#ifdef WL_WLMNGR_NVRAM_DBG    
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: closed=%d", m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlHide);
    bcmSystemMute(cmd);
#endif

#ifdef SUPPORT_SES
    /*ses_closed*//* network type (0|1) */   
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlSesHide, strcat_r(prefix, "ses_closed", tmp), "int");
    else
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesHide, strcat_r(prefix, "ses_closed", tmp), "int");
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: closed=%d", m_instance_wl[idx].m_wlVar.wlSesHide);
    bcmSystemMute(cmd);
#endif


    if(set)
    {
	wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlEnableBFR, strcat_r(prefix, "txbf_bfr_cap", tmp), "int");
	wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlEnableBFE, strcat_r(prefix, "txbf_bfe_cap", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.bsdRole, strcat_r(prefix, "bsd_role", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.bsdHport, strcat_r(prefix, "bsd_hport", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.bsdPport, strcat_r(prefix, "bsd_pport", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.bsdHelper, strcat_r(prefix, "bsd_helper", tmp), "string");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.bsdPrimary, strcat_r(prefix, "bsd_primary", tmp), "string");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.ssdEnable, strcat_r(prefix, "ssd_enable", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlTafEnable, strcat_r(prefix, "taf_enable", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAtf, strcat_r(prefix, "atf", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlPspretendThreshold, strcat_r(prefix, "pspretend_threshold", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlPspretendRetryLimit, strcat_r(prefix, "pspretend_retry_limit", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsFcsMode, strcat_r(prefix, "acs_fcs_mode", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfs, strcat_r(prefix, "acs_dfs", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsCsScanTimer, strcat_r(prefix, "acs_cs_scan_timer", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsCiScanTimer, strcat_r(prefix, "acs_ci_scan_timer", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsCiScanTimeout, strcat_r(prefix, "acs_ci_scan_timeout", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsScanEntryExpire, strcat_r(prefix, "acs_scan_entry_expire", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsTxIdleCnt, strcat_r(prefix, "acs_tx_idle_cnt", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsChanDwellTime, strcat_r(prefix, "acs_chan_dwell_time", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsChanFlopPeriod, strcat_r(prefix, "acs_chan_flop_period", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlIntferPeriod, strcat_r(prefix, "intfer_period", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlIntferCnt, strcat_r(prefix, "intfer_cnt", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlIntferTxfail, strcat_r(prefix, "intfer_txfail", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlIntferTcptxfail, strcat_r(prefix, "intfer_tcptxfail", tmp), "int");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfsrImmediate, strcat_r(prefix, "acs_dfsr_immediate", tmp), "string");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfsrDeferred, strcat_r(prefix, "acs_dfsr_deferred", tmp), "string");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfsrActivity, strcat_r(prefix, "acs_dfsr_activity", tmp), "string");
    }
    else 
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlEnableBFR, strcat_r(prefix, "txbf_bfr_cap", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlEnableBFE, strcat_r(prefix, "txbf_bfe_cap", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.bsdRole, strcat_r(prefix, "bsd_role", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.bsdHport, strcat_r(prefix, "bsd_hport", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.bsdPport, strcat_r(prefix, "bsd_pport", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.bsdHelper, strcat_r(prefix, "bsd_helper", tmp), "string");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.bsdPrimary, strcat_r(prefix, "bsd_primary", tmp), "string");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.ssdEnable, strcat_r(prefix, "ssd_enable", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlTafEnable, strcat_r(prefix, "taf_enable", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAtf, strcat_r(prefix, "atf", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlPspretendThreshold, strcat_r(prefix, "pspretend_threshold", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlPspretendRetryLimit, strcat_r(prefix, "pspretend_retry_limit", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsFcsMode, strcat_r(prefix, "acs_fcs_mode", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfs, strcat_r(prefix, "acs_dfs", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsCsScanTimer, strcat_r(prefix, "acs_cs_scan_timer", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsCiScanTimer, strcat_r(prefix, "acs_ci_scan_timer", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsCiScanTimeout, strcat_r(prefix, "acs_ci_scan_timeout", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsScanEntryExpire, strcat_r(prefix, "acs_scan_entry_expire", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsTxIdleCnt, strcat_r(prefix, "acs_tx_idle_cnt", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsChanDwellTime, strcat_r(prefix, "acs_chan_dwell_time", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsChanFlopPeriod, strcat_r(prefix, "acs_chan_flop_period", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlIntferPeriod, strcat_r(prefix, "intfer_period", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlIntferCnt, strcat_r(prefix, "intfer_cnt", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlIntferTxfail, strcat_r(prefix, "intfer_txfail", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlIntferTcptxfail, strcat_r(prefix, "intfer_tcptxfail", tmp), "int");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfsrImmediate, strcat_r(prefix, "acs_dfsr_immediate", tmp), "string");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfsrDeferred, strcat_r(prefix, "acs_dfsr_deferred", tmp), "string");
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlAcsDfsrActivity, strcat_r(prefix, "acs_dfsr_activity", tmp), "string");
    }

/*Save WEP key and wep Index (Only works with Idx=1 now */
    if(set)
    {
	int ssid_idx = m_instance_wl[idx].m_wlVar.wlSsidIdx;
        if (m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex64,  "wl_key", "int");
        else
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyIndex128, "wl_key", "int");

        if (m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeyBit == WL_BIT_KEY_64 )
        {
			wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[0],  "wl_key1", "string");
    		wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[1],  "wl_key2", "string");
			wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[2],  "wl_key3", "string");
			wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys64[3],  "wl_key4", "string");
        }
        else	
        {
			wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[0],  "wl_key1", "string");
    		wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[1],  "wl_key2", "string");
			wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[2],  "wl_key3", "string");
			wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[ssid_idx].wlKeys128[3],  "wl_key4", "string");
        }

        for (i = 0; i < WL_NUM_SSID; i++)
        {
            snprintf(tmp_s, sizeof(tmp_s), "%s_", m_instance_wl[idx].m_ifcName[i]);

            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlEnableHspot,         strcat_r(tmp_s, "hspot", tmp), "int");

            if (m_instance_wl[idx].m_wlMssidVar[i].wlKeyBit == WL_BIT_KEY_64 )
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeyIndex64,  strcat_r(tmp_s, "key", tmp), "int");
            else
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeyIndex128,  strcat_r(tmp_s, "key", tmp), "int");

            if (m_instance_wl[idx].m_wlMssidVar[i].wlKeyBit == WL_BIT_KEY_64 )
            {
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[0],  strcat_r(tmp_s, "key1", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[1],  strcat_r(tmp_s, "key2", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[2],  strcat_r(tmp_s, "key3", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[3],  strcat_r(tmp_s, "key4", tmp), "string");
            }
            else
            {
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[0],  strcat_r(tmp_s, "key1", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[1],  strcat_r(tmp_s, "key2", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[2],  strcat_r(tmp_s, "key3", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[3],  strcat_r(tmp_s, "key4", tmp), "string");
            }
        }
    }
    else
    {
        for (i = 0; i < WL_NUM_SSID; i++)
        {
            snprintf(tmp_s, sizeof(tmp_s), "%s_", m_instance_wl[idx].m_ifcName[i]);

            if (m_instance_wl[idx].m_wlMssidVar[i].wlKeyBit == WL_BIT_KEY_64 )
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeyIndex64,  strcat_r(tmp_s, "key", tmp), "int");
            else
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeyIndex128,  strcat_r(tmp_s, "key", tmp), "int");

            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlEnableHspot,       strcat_r(tmp_s, "hspot", tmp), "int");

            if (m_instance_wl[idx].m_wlMssidVar[i].wlKeyBit == WL_BIT_KEY_64)
            {
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[0], strcat_r(tmp_s, "key1", tmp), "string");
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[1], strcat_r(tmp_s, "key2", tmp), "string");
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[2], strcat_r(tmp_s, "key3", tmp), "string");
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys64[3], strcat_r(tmp_s, "key4", tmp), "string");
            }
            else
            {
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[0], strcat_r(tmp_s, "key1", tmp), "string");
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[1], strcat_r(tmp_s, "key2", tmp), "string");
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[2], strcat_r(tmp_s, "key3", tmp), "string");
                wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlKeys128[3], strcat_r(tmp_s, "key4", tmp), "string");
            }
        }
    }

#ifdef WL_WLMNGR_NVRAM_DBG   
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: wpa_psk=%s", m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlWpaPsk);    
    bcmSystemMute(cmd);        
#endif

#ifdef SUPPORT_SES
   /*ses_wpa_psk*//*WPA pre-shared key */    
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlSesWpaPsk, strcat_r(prefix, "ses_wpa_psk", tmp), "string");
    else    
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesWpaPsk, strcat_r(prefix, "ses_wpa_psk", tmp), "string");
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_wpa_psk=%s", m_instance_wl[idx].m_wlVar.wlSesWpaPsk);    
    bcmSystemMute(cmd); 
#endif

#ifdef WL_WLMNGR_NVRAM_DBG   
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: crypto=%s", m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlWpa);
    bcmSystemMute(cmd); 
#endif

#ifdef SUPPORT_SES
  /*ses_crypto*//* WPA encryption (tkip|aes|tkip+aes) */   
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlSesWpa, strcat_r(prefix, "ses_crypto", tmp), "string");    
    else
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesWpa, strcat_r(prefix, "ses_crypto", tmp), "string");    
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_crypto=%s", m_instance_wl[idx].m_wlVar.wlSesWpa);
    bcmSystemMute(cmd); 
#endif
      
   /*auth*//* Shared key authentication (opt 0 or req 1) */
    for (i = 0; i < WL_NUM_SSID; i++)
    {
       snprintf(tmp_s, sizeof(tmp_s), "%s_", m_instance_wl[idx].m_ifcName[i]);
       if(set)
           wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlAuth, strcat_r(tmp_s, "auth", tmp), "int");    
       else    
           wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlAuth, strcat_r(tmp_s, "auth", tmp), "int");    
    }

#ifdef WL_WLMNGR_NVRAM_DBG   
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: auth=%d", m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlAuth);
    bcmSystemMute(cmd); 
#endif

#ifdef SUPPORT_SES
   /*ses_auth*//* Shared key authentication (opt 0 or req 1) */   
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlSesAuth, strcat_r(prefix, "ses_auth", tmp), "int");    
    else    
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesAuth, strcat_r(prefix, "ses_auth", tmp), "int");    
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_auth=%d", m_instance_wl[idx].m_wlVar.wlSesAuth);
    bcmSystemMute(cmd); 
#endif
      
    /*wep*//* WEP encryption (enabled|disabled) */
    if(set)
    {
        int i;

        for (i = 0; i < WL_NUM_SSID; i++)
        {
            snprintf(tmp_s, sizeof(tmp_s), "%s_", m_instance_wl[idx].m_ifcName[i]);

            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[i].wlWep,  strcat_r(tmp_s, "wep", tmp), "string");
        }

        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlWep, "wl_wep", "string");
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlWep, strcat_r(prefix, "wep", tmp), "string");    
    }

#ifdef WL_WLMNGR_NVRAM_DBG   
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: wep=%s", m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlWep);
    bcmSystemMute(cmd);
#endif

#ifdef SUPPORT_SES
   /*ses_wep*//* WEP encryption (enabled|disabled) */   
    if(set)
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlSesWep, strcat_r(prefix, "ses_wep", tmp), "string");    
    else
       wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesWep, strcat_r(prefix, "ses_wep", tmp), "string");    
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: wep=%s", m_instance_wl[idx].m_wlVar.wlSesWep);
    bcmSystemMute(cmd);
#endif

#ifdef SUPPORT_SES
    /*ses_auth_mode*//* Network auth mode (radius|none) */
    /*ses_akm*//* WPA akm list (wpa|wpa2|psk|psk2) */ 
     
    if(set)
    {
        
        if(!strcmp(m_instance_wl[idx].m_wlVar.wlSesAuthMode, WL_AUTH_OPEN))
        {
            strncpy(tmp_s,"none",sizeof(tmp_s));
            nvram_unset(strcat_r(prefix, "ses_auth_mode", tmp));
            printf("unset akm\n");
            nvram_unset(strcat_r(prefix, "ses_akm", tmp));
        }
        else
        {
            if (!strcmp(m_instance_wl[idx].m_wlVar.wlSesAuthMode, WL_AUTH_RADIUS))
            {  
                strncpy(tmp_s,WL_AUTH_RADIUS,sizeof(tmp_s));    	
                wlmngr_setVarToNvram(tmp_s, strcat_r(prefix, "ses_auth_mode", tmp), "string");  
                nvram_unset(strcat_r(prefix, "ses_akm", tmp));
            }
            else
            {
                strncpy(tmp_s,"none",sizeof(tmp_s));
                wlmngr_setVarToNvram(tmp_s, strcat_r(prefix, "ses_auth_mode", tmp), "string");
                wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlSesAuthMode, strcat_r(prefix, "ses_akm", tmp), "string");      	
            }
      }

#ifdef WL_WLMNGR_NVRAM_DBG
      snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_akm=%s", m_instance_wl[idx].m_wlVar.wlSesAuthMode);
      bcmSystemMute(cmd);      
      snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_auth_mode=%s", tmp_s);
      bcmSystemMute(cmd);            
#endif   
    }
    else
    {
        wlmngr_getVarFromNvram(tmp_s, strcat_r(prefix, "ses_auth_mode", tmp), "string");           
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesAuthMode, strcat_r(prefix, "ses_akm", tmp), "string");

        if(!strcmp(tmp_s, WL_AUTH_RADIUS))
        {
            strncpy(m_instance_wl[idx].m_wlVar.wlSesAuthMode,WL_AUTH_RADIUS, sizeof(m_instance_wl[idx].m_wlVar.wlSesAuthMode));      	
        }
      
        if (m_instance_wl[idx].m_wlMssidVar[MAIN_BSS_IDX].wlAuthMode[0] == '\0')
        {
            strncpy(m_instance_wl[idx].m_wlVar.wlSesAuthMode,WL_AUTH_OPEN, sizeof(m_instance_wl[idx].m_wlVar.wlSesAuthMode));
        } 
      
#ifdef WL_WLMNGR_NVRAM_DBG           
        snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: wlSesAuthMode= %s", m_instance_wl[idx].m_wlVar.wlSesAuthMode);
        bcmSystemMute(cmd);                 
#endif
    }
#endif
   
   /*channel*/     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlChannel, strcat_r(prefix, "channel", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlChannel, strcat_r(prefix, "channel", tmp), "int"); 
    }

#ifdef WL_WLMNGR_NVRAM_DBG   
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: wlChannel=%d", m_instance_wl[idx].m_wlVar.wlChannel); 
    bcmSystemMute(cmd);                   
#endif

    /*wds*/
    if(set)
    { 
        wlmngr_getWdsMacList(idx, tmp_s, sizeof(tmp_s));   	
        wlmngr_setVarToNvram(&tmp_s, strcat_r(prefix, "wds", tmp), "string"); 

        switch (m_instance_wl[idx].m_wlVar.wlLazyWds) {
            case WL_BRIDGE_RESTRICT_ENABLE:
                val = 0;
                break;
            case WL_BRIDGE_RESTRICT_ENABLE_SCAN:
                val = 0;
                break;
            case WL_BRIDGE_RESTRICT_DISABLE:
            default:
                val = 1;
                break;
        }
        wlmngr_setVarToNvram(&val, strcat_r(prefix, "lazywds", tmp), "int");
    } 
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlLazyWds, strcat_r(prefix, "lazywds", tmp), "int");
        wlmngr_getVarFromNvram(&tmp_s, strcat_r(prefix, "wds", tmp), "string"); 
        wlmngr_setWdsMacList(idx, tmp_s, sizeof(tmp_s));
    }

#ifdef WL_WLMNGR_NVRAM_DBG   
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: wds=%s", tmp_s); 
    bcmSystemMute(cmd);                   
#endif

#ifdef SUPPORT_SES
    /*wds99*/     
    if (set)
    {
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlWdsWsec, strcat_r(prefix, "wds99", tmp), "string");    	
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlWdsWsec, strcat_r(prefix, "wds99", tmp), "string"); 
    }
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: wlWdsWsec=%s", m_instance_wl[idx].m_wlVar.wlWdsWsec); 
    bcmSystemMute(cmd);                   
#endif
  
#ifdef SUPPORT_SES  
    /*ses_event (3|4|6|7)*/
    if (set)
    {
        if (m_instance_wl[idx].m_wlVar.wlSesEvent == 7 || m_instance_wl[idx].m_wlVar.wlSesClEvent == 4)
        { /* patch */
            nvram_unset(strcat_r(prefix, "wds99", tmp));
        }
    
        wlmngr_setVarToNvram(&(m_instance_wl[idx].m_wlVar.wlSesEvent), "ses_event", "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlSesEvent), "ses_event", "int"); 
    }
#endif
   
#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_event=%d", m_instance_wl[idx].m_wlVar.wlSesEvent); 
    bcmSystemMute(cmd);
#endif
   
#ifdef SUPPORT_SES   
    /*ses_fsm_current_states XX:XX*/     
    if(set)
    {
        wlmngr_setVarToNvram(&(m_instance_wl[idx].m_wlVar.wlSesStates), "ses_fsm_current_states", "string"); 
    } 
    else 
    {
        wlmngr_getVarFromNvram(&(m_instance_wl[idx].m_wlVar.wlSesStates), "ses_fsm_current_states", "string"); 
    }
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_fsm_current_states=%s", m_wlVar.wlSesStates); 
    bcmSystemMute(cmd);                   
#endif

#ifdef SUPPORT_SES
   /*ses_wds_mode (0-4)*/     
    if(set)
    { 
        wlmngr_setVarToNvram(&(m_instance_wl[idx].m_wlVar.wlSesWdsMode), "ses_wds_mode", "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&(m_instance_wl[idx].m_wlVar.wlSesWdsMode), "ses_wds_mode", "int"); 
    }
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_wds_mode=%s", m_instance_wl[idx].m_wlVar.wlSesWdsMode); 
    bcmSystemMute(cmd);                   
#endif
#ifdef SUPPORT_SES
   /*ses_cl_enable*/     
    if(set)
    {
        wlmngr_setVarToNvram(&(m_instance_wl[idx].m_wlVar.wlSesClEnable), "ses_cl_enable", "int"); 
    } 
    else
    {
        wlmngr_getVarFromNvram(&(m_instance_wl[idx].m_wlVar.wlSesClEnable), "ses_cl_enable", "int"); 
    }
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_cl_enable=%s", m_instance_wl[idx].m_wlVar.wlSesClEnable); 
    bcmSystemMute(cmd);                   
#endif

#ifdef SUPPORT_SES
   /*ses_cl_event*/     
    if(set)
    {
        wlmngr_setVarToNvram(&(m_instance_wl[idx].m_wlVar.wlSesClEvent), "ses_cl_event", "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&(m_instance_wl[idx].m_wlVar.wlSesClEvent), "ses_cl_event", "int"); 
    }
#endif

#if defined(WL_WLMNGR_NVRAM_DBG) && defined(SUPPORT_SES)
    snprintf(cmd, sizeof(cmd), "echo wlmngr_NvramMapping: ses_cl_event=%s", m_instance_wl[idx].m_wlVar.wlSesClEvent); 
    bcmSystemMute(cmd);                   
#endif

    /* WEP over WDS */     
    if (set)
    {
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlWdsSecEnable, strcat_r(prefix, "wdssec_enable", tmp), "int");    	
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlWdsSecEnable, strcat_r(prefix, "wdssec_enable", tmp), "int"); 
    }

   /*URE*/     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].apstaSupported, strcat_r(prefix, "apsta", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].apstaSupported, strcat_r(prefix, "apsta", tmp), "int"); 
    }

  

    /* nband */
    if(set)
    {
        m_instance_wl[idx].m_wlVar.wlNBand = m_instance_wl[idx].m_wlVar.wlBand;
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlNBand, strcat_r(prefix, "nband", tmp), "int");    	    	
    }
    else {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlNBand, strcat_r(prefix, "nband", tmp), "int");
        m_instance_wl[idx].m_wlVar.wlBand = m_instance_wl[idx].m_wlVar.wlNBand;
	}

    /* chanspec */
    memset(name, 0, sizeof(name)); 

    if (m_instance_wl[idx].m_wlVar.wlChannel < 0)
        m_instance_wl[idx].m_wlVar.wlChannel = 0;

    /* for 20M and 10M */
    snprintf(name, sizeof(name), "%d", m_instance_wl[idx].m_wlVar.wlChannel);
    {
#ifdef SUPPORT_MIMO     
        if(MIMO_PHY && (m_instance_wl[idx].m_wlVar.wlChannel != 0)) {         
            if (m_instance_wl[idx].m_wlVar.wlNBwCap == WLC_BW_CAP_80MHZ)
                strcat(name, "/80");
            else if (m_instance_wl[idx].m_wlVar.wlNBwCap == WLC_BW_CAP_40MHZ) {
                if (m_instance_wl[idx].m_wlVar.wlNCtrlsb == WL_CTL_SB_LOWER)
                    strcat(name, "l");
                else if (m_instance_wl[idx].m_wlVar.wlNCtrlsb == WL_CTL_SB_UPPER)
                    strcat(name, "u");
            } 
        }
#endif//SUPPORT_MIMO
    }
    if(set)
    {
        nvram_set( strcat_r(prefix, "chanspec", tmp), name);    	    	
    }
#ifdef WL11AC
    /* bw_cap */
    {

	int bw_cap =m_instance_wl[idx].m_wlVar.wlNBwCap; 

        if(set)
        {
            wlmngr_setVarToNvram(&bw_cap, strcat_r(prefix, "bw_cap", tmp), "int");    	    	
        } else {
             wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlNBwCap ,
                        strcat_r(prefix, "bw_cap", tmp), "int");
        }
    }
#else
    /* nbw_cap */
    {
        if(set)
        {
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlNBwCap, strcat_r(prefix, "bw_cap", tmp), "int");    	    	
        } else {
             wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlNBwCap ,
                        strcat_r(prefix, "bw_cap", tmp), "int");
        }
    }
#endif    
#ifdef SUPPORT_MIMO     
    if(MIMO_PHY) {         
        /* nmode */ 
        if(set)
        { 
            val = AUTO_MODE;
            if (!strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_OFF))
                val = OFF;    
            else if (!strcmp(m_instance_wl[idx].m_wlVar.wlNmode, WL_ON))
                val = ON;    
            wlmngr_setVarToNvram(&val, strcat_r(prefix, "nmode", tmp), "int"); 
        }
        else
        {
            val = 0;
            wlmngr_getVarFromNvram(&val, strcat_r(prefix, "nmode", tmp), "int");
            if (val == ON)
                snprintf(m_instance_wl[idx].m_wlVar.wlNmode, sizeof(m_instance_wl[idx].m_wlVar.wlNmode), WL_ON);
            else if(val== OFF)
                snprintf(m_instance_wl[idx].m_wlVar.wlNmode, sizeof(m_instance_wl[idx].m_wlVar.wlNmode), WL_OFF);            
            else 
                snprintf(m_instance_wl[idx].m_wlVar.wlNmode, sizeof(m_instance_wl[idx].m_wlVar.wlNmode), WL_AUTO);            
        }

        /* nmode_protection */ 	
        if(set)
        { 
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlNProtection, strcat_r(prefix, "nmode_protection", tmp), "string");      	      	
        } else {      	
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlNProtection, strcat_r(prefix, "nmode_protection", tmp), "string");      	      	
        }

        /* rifs_advert */     
        memset(name, 0, sizeof(name));
        if(set)
        { 
            if (m_instance_wl[idx].m_wlVar.wlRifsAdvert != 0)
                snprintf(name, sizeof(name), WL_AUTO);
            else
                snprintf(name, sizeof(name), WL_OFF);
            wlmngr_setVarToNvram(&name, strcat_r(prefix, "rifs_advert", tmp), "string"); 
        }
        else
        {
            wlmngr_getVarFromNvram(&name, strcat_r(prefix, "rifs_advert", tmp), "string");
            if (!strcmp(name, WL_AUTO))
                m_instance_wl[idx].m_wlVar.wlRifsAdvert = AUTO_MODE;
            else     
                m_instance_wl[idx].m_wlVar.wlRifsAdvert = OFF;
        }

        /* obss_coex */     
        if(set)
        { 
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlObssCoex, strcat_r(prefix, "obss_coex", tmp), "int"); 
        }
        else
        {
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlObssCoex, strcat_r(prefix, "obss_coex", tmp), "int");
        }

        /* nmcsidx */ 	
        if(set)
        {    	      	
            wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlNMcsidx, strcat_r(prefix, "nmcsidx", tmp), "int");
        } 
        else 
        {      	
            wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlNMcsidx, strcat_r(prefix, "nmcsidx", tmp), "int");      	      	
        }

    }
#endif /* SUPPORT_MIMO */

	/* rate */ 	
	if(set)
	{    	      	
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlRate, strcat_r(prefix, "rate", tmp), "int");
	} 
	else 
	{      	
	    wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlRate, strcat_r(prefix, "rate", tmp), "int");      	      	
	}

    /* mrate */ 	
    if(set)
    {    	      	
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlMCastRate, strcat_r(prefix, "mrate", tmp), "int");
    } 
    else 
    {      	
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlMCastRate, strcat_r(prefix, "mrate", tmp), "int");      	      	
    }

    /* rateset */ 	
    if(set)
    {    	      	
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlBasicRate, strcat_r(prefix, "rateset", tmp), "string");
    } 
    else 
    {      	
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlBasicRate, strcat_r(prefix, "rateset", tmp), "string");      	      	
    }

    /* plcphdr */ 	
    if(set)
    {    	      	
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlPreambleType, strcat_r(prefix, "plcphdr", tmp), "string");
    } 
    else 
    {      	
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlPreambleType, strcat_r(prefix, "plcphdr", tmp), "string");      	      	
    }

    /* country_code */ 	
    if(set)
    {    	      	
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlCountry, strcat_r(prefix, "country_code", tmp), "string");
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlRegRev, strcat_r(prefix, "country_rev", tmp), "int");   
    } 
    else 
    {      	
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlCountry, strcat_r(prefix, "country_code", tmp), "string");      	      	
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlRegRev, strcat_r(prefix, "country_rev", tmp), "int");      	      	
    }

    /* gmode */ 	
    if(set)
    {    	      	
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlgMode, strcat_r(prefix, "gmode", tmp), "int");
    } 
    else 
    {      	
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlgMode, strcat_r(prefix, "gmode", tmp), "int");      	      	
    }

    /* gmode_protection */ 	
    if(set)
    {    	      	
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlProtection, strcat_r(prefix, "gmode_protection", tmp), "string");
    } 
    else 
    {      	
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlProtection, strcat_r(prefix, "gmode_protection", tmp), "string");      	      	
    }

    /* bcn */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlBcnIntvl, strcat_r(prefix, "bcn", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlBcnIntvl, strcat_r(prefix, "bcn", tmp), "int"); 
    }

    /* dtim */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlDtmIntvl, strcat_r(prefix, "dtim", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlDtmIntvl, strcat_r(prefix, "dtim", tmp), "int"); 
    }

    /* rts */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlRtsThrshld, strcat_r(prefix, "rts", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlRtsThrshld, strcat_r(prefix, "rts", tmp), "int"); 
    }

    /* frag */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlFrgThrshld, strcat_r(prefix, "frag", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlFrgThrshld, strcat_r(prefix, "frag", tmp), "int"); 
    }

    /* maxassoc */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc, strcat_r(prefix, "maxassoc", tmp), "int"); 


    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlGlobalMaxAssoc, strcat_r(prefix, "maxassoc", tmp), "int"); 
    }

    /* frameburst */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlFrameBurst, strcat_r(prefix, "frameburst", tmp), "string"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlFrameBurst, strcat_r(prefix, "frameburst", tmp), "string"); 
    }

    /* reg_mode */     
    if(set)
    { 
        memset(name, 0, sizeof(name));
        switch(m_instance_wl[idx].m_wlVar.wlRegMode) {
            case REG_MODE_H:
                snprintf(name, sizeof(name), "h");
                break;
            case REG_MODE_D:
                snprintf(name, sizeof(name), "d");
                break;
            case REG_MODE_OFF:
                snprintf(name, sizeof(name), WL_OFF);
                break;
            default:
                break;
        }
        wlmngr_setVarToNvram(&name, strcat_r(prefix, "reg_mode", tmp), "string"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&name, strcat_r(prefix, "reg_mode", tmp), "string"); 
        if (!strcmp(name, "h"))
            m_instance_wl[idx].m_wlVar.wlRegMode = REG_MODE_H;
        else if (!strcmp(name, "d"))    
            m_instance_wl[idx].m_wlVar.wlRegMode = REG_MODE_D;
        else
            m_instance_wl[idx].m_wlVar.wlRegMode = REG_MODE_OFF;
    }

    /* dfs_preism */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlDfsPreIsm, strcat_r(prefix, "dfs_preism", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlDfsPreIsm, strcat_r(prefix, "dfs_preism", tmp), "int"); 
    }

    /* dfs_postism */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlDfsPostIsm, strcat_r(prefix, "dfs_postism", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlDfsPostIsm, strcat_r(prefix, "dfs_postism", tmp), "int"); 
    }

    /* tpc_db */     
    if(set)
    { 
        wlmngr_setVarToNvram(&m_instance_wl[idx].m_wlVar.wlTpcDb, strcat_r(prefix, "tpc_db", tmp), "int"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&m_instance_wl[idx].m_wlVar.wlTpcDb, strcat_r(prefix, "tpc_db", tmp), "int"); 
    }

    /* wme */     
    if(set)
    { 
        memset(name, 0, sizeof(name));
        switch(m_instance_wl[idx].m_wlVar.wlWme) {
            case AUTO_MODE:
                snprintf(name, sizeof(name), WL_AUTO);
                break;
            case ON:
                snprintf(name, sizeof(name), WL_ON);
                break;
            case OFF:
                snprintf(name, sizeof(name), WL_OFF);
                break;
            default:
                break;
        }
        wlmngr_setVarToNvram(&name, strcat_r(prefix, "wme", tmp), "string"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&name, strcat_r(prefix, "wme", tmp), "string"); 
        if (!strcmp(name, WL_AUTO))
            m_instance_wl[idx].m_wlVar.wlWme = AUTO_MODE;
        else if (!strcmp(name, WL_ON))    
            m_instance_wl[idx].m_wlVar.wlWme = ON;
        else
            m_instance_wl[idx].m_wlVar.wlWme = OFF;
    }

    /* wme_no_ack */     
    if(set)
    { 
        memset(name, 0, sizeof(name));
        switch(m_instance_wl[idx].m_wlVar.wlWmeNoAck) {
            case ON:
                snprintf(name, sizeof(name), WL_ON);
                break;
            case OFF:
                snprintf(name, sizeof(name),  WL_OFF);
                break;
            default:
                break;
        }
        wlmngr_setVarToNvram(&name, strcat_r(prefix, "wme_no_ack", tmp), "string"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&name, strcat_r(prefix, "wme_no_ack", tmp), "string"); 
        if (!strcmp(name, WL_ON))    
            m_instance_wl[idx].m_wlVar.wlWmeNoAck = ON;
        else
            m_instance_wl[idx].m_wlVar.wlWmeNoAck = OFF;

    }

    /* wme_apsd */     
    if(set)
    { 
        memset(name, 0, sizeof(name));
        switch(m_instance_wl[idx].m_wlVar.wlWmeApsd) {
            case ON:
                snprintf(name, sizeof(name), WL_ON);
                break;
            case OFF:
                snprintf(name, sizeof(name), WL_OFF);
                break;
            default:
                break;
        }
        wlmngr_setVarToNvram(&name, strcat_r(prefix, "wme_apsd", tmp), "string"); 
    }
    else
    {
        wlmngr_getVarFromNvram(&name, strcat_r(prefix, "wme_apsd", tmp), "string"); 
        if (!strcmp(name, WL_ON))    
            m_instance_wl[idx].m_wlVar.wlWmeApsd = ON;
        else
            m_instance_wl[idx].m_wlVar.wlWmeApsd = OFF;

    }

    /* Universal Range Extension */
    if(set)	{
        if (m_instance_wl[idx].apstaSupported != 0) {
            snprintf(name, sizeof(name), "%s_mode", m_instance_wl[idx].m_ifcName[0]);
            nvram_set(name, "ap"); 

            if (m_instance_wl[idx].m_wlVar.wlEnableUre) {
                if (m_instance_wl[idx].m_wlVar.wlEnableUre == 1) { /* bridge mode: Range Extender */
                    snprintf(name, sizeof(name), "%s_mode", m_instance_wl[idx].m_ifcName[0]); /* wl0 needs to be wet */
                    nvram_set(name, "wet"); 
                } else if (m_instance_wl[idx].m_wlVar.wlEnableUre == 2) { /* routed mode: Travel Router */
                    snprintf(name, sizeof(name), "%s_mode", m_instance_wl[idx].m_ifcName[0]);
                    nvram_set(name, "sta"); 
                }

                for (i=1; i<m_instance_wl[idx].numBss; i++) { /* guest SSIDs need to be ap mode */
                    snprintf(name, sizeof(name), "%s_mode", m_instance_wl[idx].m_ifcName[i]);
                    nvram_set(name, "ap"); 
                }
            } else {
                if (!strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET) || !strcmp(m_instance_wl[idx].m_wlVar.wlMode, WL_OPMODE_WET_MACSPOOF)) {
                    /* Wireless Ethernet / MAC Spoof */
                    snprintf(name, sizeof(name), "%s_mode", m_instance_wl[idx].m_ifcName[0]);
                    nvram_set(name, "wet");
                }
            }
        } 
    }
    /*re-read nvram*/
    nvram_set("wlmngr","done");
    free(tmp_s2);
}

static bool wlmngr_detectAcsd(void)
{
   FILE *fp;
    
   fp = fopen("/bin/acsd", "r"); 

   if ( fp != NULL ) {   	   	
      /* ACSD found */
      fclose(fp);
      return TRUE;
   } else {
      /* ACSD not found */
      return FALSE;
   }
}


/* End of file */
