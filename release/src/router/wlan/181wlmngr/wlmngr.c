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

#include "board.h"

#ifdef DSLCPE_1905
#include <wps_1905.h>
#endif
#include "odl.h"
#include <pthread.h>
#include <bcmnvram.h>
#include <bcmconfig.h>

#include "wlioctl.h"
#include "wlutils.h"
#include "wlmngr.h"
#include "wllist.h"

#include "wlsyscall.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include <wlcsm_lib_dm.h>
//#include "wlmngr_http.h"

#ifdef SUPPORT_WSC
#include <time.h>
#endif
#include <wlcsm_lib_api.h>
#include <wlcsm_lib_dm.h>
#include <wlcsm_linux.h>
// #define WL_WLMNGR_DBG

#if defined(HSPOT_SUPPORT)
#include <wlcsm_lib_hspot.h>
#endif
#define IFC_BRIDGE_NAME	 "br0"

int wl_visal_start_cnt=0;

#define WL_PREFIX "_wl"
pthread_mutex_t g_WLMNGR_THREAD_MUTEX= PTHREAD_MUTEX_INITIALIZER; /**< mutex for Synchronization between Thread */
unsigned int g_radio_idx=0;
pthread_mutex_t g_WLMNGR_REBOOT_MUTEX= PTHREAD_MUTEX_INITIALIZER; /**< mutex for sync reboot */
static int _wlmngr_handle_wlcsm_cmd_get_dmvar(t_WLCSM_MNGR_VARHDR  *hdr,char *varName,char *varValue);

#if 0
static inline void  printR13( int line)
{
    register unsigned long r13  asm("r13");
    fprintf(stderr,"line:%d, sp:%08x\n",line,r13);
}
#else
#define printR13(...)
#endif
void wlmngr_get_thread_lock(void)
{
    pthread_mutex_lock(&g_WLMNGR_THREAD_MUTEX);
}

void wlmngr_release_thread_lock(void)
{
    pthread_mutex_unlock(&g_WLMNGR_THREAD_MUTEX);
}

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
static bool wlmngr_detectAcsd(void);

int get_wps_env();
int set_wps_env(char *uibuf);


void wlmngr_enum_ifnames(unsigned int idx)
{
    char ifbuf[WL_SIZE_512_MAX] = {0};
    char brlist[WL_SIZE_256_MAX];
    char name[32],nv_name[32],nv_valu[64];
    char *next=NULL,*lnext=NULL;
    int index=0,bridgeIndex=0;
    char br_ifnames[128];
    wlcsm_dm_get_bridge_info(ifbuf);
    if(ifbuf[0]!='\0') {
        /*rest lanx_ifnames */
        for (index = 1; index<MAX_BR_NUM; ++index) {
            sprintf(brlist,"lan%d_ifnames",index);
            if(nvram_get(brlist))
                wlcsm_nvram_unset(brlist);
        }

        /* reset wlx_vifs */
        sprintf(nv_name,"wl%d_vifs",idx);
        wlcsm_nvram_unset(nv_name);
        /* reset wlx.y_ifname, but leave wlx_ifname unchanged regardless */

        for (index=1; index<WL_RADIO_WLNUMBSS(idx); index++) {
            sprintf(brlist,"wl%d.%d_ifname",idx,index);
            if(wlcsm_nvram_get(brlist))
                wlcsm_nvram_unset(brlist);
        }

        for (index = 0; index  <WL_WIFI_RADIO_NUMBER; ++index) {
            sprintf(brlist,"wl%d",index);
            if(strstr(ifbuf,brlist)==NULL && strlen(ifbuf)+strlen(brlist)+1<WL_SIZE_512_MAX) {
                strcat(ifbuf,":");
                strcat(ifbuf,brlist);
            }
        }
        for_each(brlist,ifbuf,lnext) {
            index=0;
            bridgeIndex=0;
            memset(br_ifnames,0,sizeof(br_ifnames));
            foreachcolon(name,brlist,next) {
                if(index++==0) {

                    /* here it is the bridge name,and we get index from it */
                    if(sscanf(name,"br%d",&bridgeIndex)) {
                        if (bridgeIndex == 0)
                            snprintf(nv_name, sizeof(nv_name), "lan_hwaddr");
                        else
                            snprintf(nv_name, sizeof(nv_name), "lan%d_hwaddr", bridgeIndex);

                        wlmngr_getHwAddr(0, name, nv_valu);
                        nvram_set(nv_name, nv_valu);

                        if (bridgeIndex == 0)
                            snprintf(nv_name, sizeof(nv_name), "lan_ifname");
                        else
                            snprintf(nv_name, sizeof(nv_name), "lan%d_ifname", bridgeIndex);
                        nvram_set(nv_name,name);

                    } else {
                        WLCSM_TRACE(WLCSM_TRACE_LOG," BRIDGE name is in different format??? \r\n" );
                    }

                } else {
                    if(strlen(br_ifnames))
                        snprintf(br_ifnames+strlen(br_ifnames), sizeof(br_ifnames)-strlen(br_ifnames), " %s",name);
                    else
                        snprintf(br_ifnames, sizeof(br_ifnames), "%s",name);
                }
            }

            if (bridgeIndex == 0)
                snprintf(nv_name, sizeof(nv_name), "lan_ifnames");
            else
                snprintf(nv_name, sizeof(nv_name), "lan%d_ifnames", bridgeIndex);

            WLCSM_TRACE(WLCSM_TRACE_DBG,"br_ifnames:%s\r\n",br_ifnames);
            nvram_set(nv_name,br_ifnames);
            bridgeIndex++;
        }

        /* construct wlx_vifs */
        brlist[0]='\0';
        for (index=1; index<WL_RADIO_WLNUMBSS(idx); index++) {
            if(WL_BSSID_WLENBLSSID(idx,index)) {
                snprintf(nv_name,sizeof(nv_name)," wl%d.%d",idx,index);
                strcat(brlist,nv_name);
            }
        }
        if(strlen(brlist)>1) {
            snprintf(nv_name,sizeof(nv_name),"wl%d_vifs",idx);
            wlcsm_nvram_set(nv_name,brlist);
        }
    }
}

/* hook function for DM layer to get runtime object pointer */
void *wlmngr_get_runtime_obj_pointer(unsigned int radio_idx,unsigned int sub_idx,unsigned int oid,WLCSM_DM_MNGR_CMD cmd,void *par)
{
    switch ( oid ) {
    case WLMNGR_DATA_POS_WIFI_ASSOCIATED_DEVICE:
        WLCSM_TRACE(WLCSM_TRACE_LOG," TODO: return the Assocaited device pointer, currently we are using different structure,to consolidate \r\n" );
        break;
    case WLMNGR_DATA_POS_WIFI_SSID_STATS:
        WLCSM_TRACE(WLCSM_TRACE_LOG," TODO:get WIFI SSID STATISTICS \r\n" );
        break;
    case WLMNGR_DATA_POS_WIFI_RADIO_STATS:
        WLCSM_TRACE(WLCSM_TRACE_LOG," TODOL get RADIO SATISTICS \r\n" );
        break;
    default:
        WLCSM_TRACE(WLCSM_TRACE_LOG,"ERROR: NO SUCH RUNTIME OBJECT!!!!!!!!!!!!!!!!!!!!\n" );
        break;
    }
    return NULL;
}


static int __wlmngr_handle_wlcsm_var_validate(t_WLCSM_MNGR_VARHDR  *hdr, char *varName,char *varValue)
{
    int ret=0;
    ret=wlmngr_handle_special_var(hdr,varName,varValue,WLMNGR_VAR_OPER_VALIDATE);
    WLCSM_TRACE(WLCSM_TRACE_DBG, "ret:%d",ret);
    return ret== WLMNGR_VAR_HANDLE_FAILURE?WLMNGR_VAR_HANDLE_FAILURE:0;
}

static int _wlmngr_handle_wlcsm_cmd_reg_dm_event(t_WLCSM_MNGR_VARHDR  *hdr, char *varName,char *varValue)
{
    /* sub_idx is the event number */
    WLCSM_TRACE(WLCSM_TRACE_LOG," REGISTER event:%d \r\n",hdr->sub_idx );
    return  wlcsm_dm_reg_event(hdr->sub_idx,WLCSM_MNGR_CMD_GET_SAVEDM(hdr->radio_idx));
}
static int _wlmngr_handle_wlcsm_cmd_pwrreset_event(t_WLCSM_MNGR_VARHDR  *hdr, char *varName,char *varValue)
{
#ifdef IDLE_PWRSAVE
    wlmngr_togglePowerSave();
#endif
    return 0;
}
static int _wlmngr_handle_wlcsm_cmd_setdmdbglevel_event(t_WLCSM_MNGR_VARHDR  *hdr, char *varName,char *varValue)
{
    return wlcsm_dm_set_dbglevel(varValue);
}

static int _wlmngr_handle_wlcsm_cmd_set_dmvar(t_WLCSM_MNGR_VARHDR  *hdr, char *varName,char *varValue)
{
    unsigned int pos=0;
    int ret=0;
    WLCSM_NAME_OFFSET *name_offset= wlcsm_dm_get_mngr_entry(hdr,varValue,&pos);
    if(!name_offset) {
        WLCSM_TRACE(WLCSM_TRACE_LOG," reurn not successull \r\n" );
        ret=-1;
    } else {
        WLCSM_TRACE(WLCSM_TRACE_LOG,"dm oid is matching to wlmngr %s and offset:%d\r\n",name_offset->name,name_offset->offset);
        ret=__wlmngr_handle_wlcsm_var_validate(hdr,name_offset->name,varValue);
        if(!ret) {
            WLCSM_TRACE(WLCSM_TRACE_LOG," set wlmngr var by DM OID \r\n" );
            WLCSM_MNGR_CMD_SET_CMD(hdr->radio_idx,0);
            ret=wlmngr_set_var(hdr,name_offset->name,varValue);
        }
    }
    return ret;
}

static int _wlmngr_handle_wlcsm_cmd_validate_dmvar(t_WLCSM_MNGR_VARHDR  *hdr, char *varName,char *varValue)
{

    unsigned int pos=0;
    int ret=0;
    WLCSM_NAME_OFFSET *name_offset= wlcsm_dm_get_mngr_entry(hdr,varValue,&pos);

    if(!name_offset) {
        WLCSM_TRACE(WLCSM_TRACE_LOG," reurn not successull \r\n" );
        ret=-1;
    } else {
        WLCSM_TRACE(WLCSM_TRACE_LOG,"dm oid is matching to wlmngr %s and offset:%d\r\n",name_offset->name,name_offset->offset);
        ret=__wlmngr_handle_wlcsm_var_validate(hdr,name_offset->name,varValue);
    }
    return ret;
}

//
//**************************************************************************
// Function Name: getGPIOverlays
// Description  : get the value of GPIO overlays
// Parameters   : interface idx.
// Returns      : none.
//**************************************************************************
unsigned long wlmngr_adjust_GPIOOverlays(const unsigned int idx )
{
    int f = open( "/dev/brcmboard", O_RDWR );
    unsigned long  GPIOOverlays = 0;
    if( f > 0 ) {
        BOARD_IOCTL_PARMS IoctlParms;
        memset( &IoctlParms, 0x00, sizeof(IoctlParms) );
        IoctlParms.result = -1;
        IoctlParms.string = (char *)&GPIOOverlays;
        IoctlParms.strLen = sizeof(GPIOOverlays);
        ioctl(f, BOARD_IOCTL_GET_GPIOVERLAYS, &IoctlParms);
        WL_RADIO(idx).GPIOOverlays=GPIOOverlays;
        WLCSM_TRACE(WLCSM_TRACE_LOG," ---SETTING GPIOOverlay---:%u \r\n",GPIOOverlays );
        close(f);
    }
    return GPIOOverlays;
}

//**************************************************************************
// Function Name: setASPM
// Description  : set aspm according to GPIOOverlays
// Parameters   : interface idx.
// Returns      : none.
//**************************************************************************
void wlmngr_setASPM(const unsigned int idx )
{

#ifdef IDLE_PWRSAVE
    {
        char cmd[WL_SIZE_132_MAX];
        int is_dhd=0;
        snprintf(cmd,sizeof(cmd),"wl%d",idx);
        is_dhd=!dhd_probe(cmd);
        /* Only enable L1 mode, because L0s creates EVM issues. The power savings are the same */
        if (WL_RADIO(idx).GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ) {
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


static  void _wlmngr_nv_adjust_security(const unsigned int idx,SYNC_DIRECTION direction)
{
    char buf[WL_SIZE_512_MAX];
    int i=0;
    for ( i=0; i<WL_RADIO_WLNUMBSS(idx); i++) {
        if(!WL_APSEC_WLAUTHMODE(idx,i)) {
            WLCSM_TRACE(WLCSM_TRACE_DBG," AUTHMODE IS NOT SET\r\n");
            continue;
        }
        if(!WL_APSEC_WLAUTHMODE(idx,i)||!strcmp(WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_OPEN)||
                strlen(WL_APSEC_WLAUTHMODE(idx,i))==0) {
            snprintf(buf, sizeof(buf), "%s_auth_mode",WL_BSSID_NAME(idx,i));
            nvram_set(buf,"");
            snprintf(buf, sizeof(buf), "%s_akm",WL_BSSID_NAME(idx,i));
            nvram_set(buf,"");
        } else if(!strcmp(WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_RADIUS)) {
            snprintf(buf, sizeof(buf), "%s_akm",WL_BSSID_NAME(idx,i));
            nvram_set(buf,"");
        } else {
            if(WL_APSEC_WLAUTHMODE(idx,i)) {
                snprintf(buf, sizeof(buf), "%s_auth_mode",WL_BSSID_NAME(idx,i));
                nvram_set(buf,"none");
                if(strcmp(WL_APSEC_WLAUTHMODE(idx,i),"none")) {
                    snprintf(buf, sizeof(buf), "%s_akm",WL_BSSID_NAME(idx,i));
                    nvram_set(buf,WL_APSEC_WLAUTHMODE(idx,i));
                }
            }
        }
    }
    nvram_set("wl_key1",WL_APSEC_WLKEY1(idx,0));
    nvram_set("wl_key2",WL_APSEC_WLKEY2(idx,0));
    nvram_set("wl_key3",WL_APSEC_WLKEY3(idx,0));
    nvram_set("wl_key4",WL_APSEC_WLKEY4(idx,0));
    snprintf(buf, sizeof(buf), "%d",WL_APSEC_WLKEYINDEX(idx,0));
    nvram_set("wl_key",buf);
    nvram_set("wl_wep",WL_APSEC_WLWEP(idx,0));
}

static  void _wlmngr_nv_adjust_wps(const unsigned int idx ,SYNC_DIRECTION direction)
{
    int br,i;
    char buf[WL_MID_SIZE_MAX]= {0};
    char cmd[WL_SIZE_256_MAX]= {0};

    if ( direction== WL_SYNC_FROM_DM ) {
        for(i=0; i<WL_RADIO_WLNUMBSS(idx); i++) {
            if(WL_BSSID_BRIDGE_NAME(idx,i)) {
                br = WL_BSSID_BRIDGE_NAME(idx,i)[2] - 0x30;
                if ( br == 0 )
                    snprintf(cmd, sizeof(cmd), "lan_wps_oob");
                else
                    snprintf(cmd, sizeof(cmd), "lan%d_wps_oob", br);

                if(strcmp(WL_APWPS_WLWSCAPMODE(idx,i),"0"))
                    nvram_set(cmd,"disabled");
                else
                    nvram_set(cmd,"enabled");
            } else
                WLCSM_TRACE(WLCSM_TRACE_ERR,"ERROR: NO bridge interface name???? \r\n" );
        }
    }
}

static  void _wlmngr_nvram_adjust(const unsigned int idx,int direction)
{

    _wlmngr_nv_adjust_wps(idx,direction);
    _wlmngr_nv_adjust_security(idx,direction);
#ifndef NO_CMS
    wlmngr_nv_adjust_chanspec(idx,direction);
#endif
}

static  void _wlmngr_adjust_country_rev(const unsigned int idx , SYNC_DIRECTION  direction)
{

    /* in TR181- country and rev is specific  in one regulationDomain field, here we split it into two */
    char * countryrev= WL_RADIO(idx).wlCountryRev;
    char ccode[10]= {0}; /* make it bigger to tolerate the wrong counryrev */
    int  regrev=-1;

    if(countryrev) {

        if ( direction== WL_SYNC_FROM_DM ) {

            sscanf(countryrev,"%02s/%d",ccode,&regrev);

            if(strlen(countryrev)==2) regrev=0;

            if(regrev>=0 && strlen(ccode)==2) {
                WL_RADIO_REGREV(idx)=regrev;
                wlcsm_strcpy(&WL_RADIO_COUNTRY(idx),ccode);
                WLCSM_TRACE(WLCSM_TRACE_LOG," country:%s,reg:%d \r\n",WL_RADIO_COUNTRY(idx),WL_RADIO_REGREV(idx) );
            } else {
                WLCSM_TRACE(WLCSM_TRACE_ERR," !!!!ADJUST COUNTRY FAILURE!!!!  \r\n" );
                WLCSM_TRACE(WLCSM_TRACE_ERR," country:%s,reg:%d \r\n",WL_RADIO_COUNTRY(idx),WL_RADIO_REGREV(idx) );
            }
        } else {

            snprintf(ccode, sizeof(ccode), "%s/%d",WL_RADIO_COUNTRY(idx),WL_RADIO_REGREV(idx));
            wlcsm_strcpy(&WL_RADIO_COUNTRYREV(idx),ccode);
            WLCSM_TRACE(WLCSM_TRACE_LOG," countryrev:%s \r\n",WL_RADIO_COUNTRYREV(idx));
        }
    } else

        WLCSM_TRACE(WLCSM_TRACE_ERR," !!!!!!!!!!!!what, THERE IS NO COUNTRY REV????? \r\n" );
}


void wlmngr_adjust_security(const unsigned int idx ,SYNC_DIRECTION direction)
{
    int i=0;

    if(direction==WL_SYNC_FROM_DM) {
        for(i=0; i<WL_RADIO_WLNUMBSS(idx); i++) {

            if(WL_APSEC_WLSECMODE(idx,i)==NWIFI_SECURITY_MODE_WEP_64 || (WL_APSEC_WLSECMODE(idx,i)==NWIFI_SECURITY_MODE_WEP_128))  {
                wlcsm_strcpy(&WL_APSEC_WLWEP(idx,i),"enabled");
            } else
                wlcsm_strcpy(&WL_APSEC_WLWEP(idx,i),"disabled");

            switch ( WL_APSEC_WLSECMODE(idx,i) ) {
            case NWIFI_SECURITY_MODE_OPEN:
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_OPEN);
                break;
            case NWIFI_SECURITY_MODE_WEP_64:
                WL_APSEC_WLKEYBIT(idx,i)=WL_BIT_KEY_64;
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_SHARED);
                break;
            case NWIFI_SECURITY_MODE_WEP_128:
                WL_APSEC_WLKEYBIT(idx,i)=WL_BIT_KEY_128;
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_SHARED);
                break;
            case NWIFI_SECURITY_MODE_WPA_Personal:
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA_PSK);
                break;
            case NWIFI_SECURITY_MODE_WPA2_Personal:
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2_PSK);
                break;
            case NWIFI_SECURITY_MODE_WPA_WPA2_Personal:
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2_PSK_MIX);
                break;
            case NWIFI_SECURITY_MODE_WPA_Enterprise:
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA);
                break;
            case NWIFI_SECURITY_MODE_WPA2_Enterprise:
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2);
                break;
            case NWIFI_SECURITY_MODE_WPA_WPA2_Enterprise:
                wlcsm_strcpy(&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2_MIX);
                break;
            }
        }
    } else {
        for(i=0; i<WL_RADIO_WLNUMBSS(idx); i++) {
            if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_OPEN) == 0)
                WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_OPEN;
            else if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA_PSK) == 0)
                WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WPA_Personal;

            else if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2_PSK) == 0)
                WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WPA2_Personal;

            else if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2_PSK_MIX) == 0)
                WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WPA_WPA2_Personal;

            else if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA) == 0)
                WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WPA_Enterprise;

            else if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2) == 0)
                WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WPA2_Enterprise;

            else if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_WPA2_MIX) == 0)
                WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WPA_WPA2_Enterprise;

            else if (strcmp((char *)&WL_APSEC_WLAUTHMODE(idx,i),WL_AUTH_SHARED) == 0) {
                if (WL_APSEC_WLKEYBIT(idx,i) == WL_BIT_KEY_64)
                    WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WEP_64;
                else if (WL_APSEC_WLKEYBIT(idx,i) == WL_BIT_KEY_128)
                    WL_APSEC_WLSECMODE(idx,i) = NWIFI_SECURITY_MODE_WEP_128;
            }
        }
    }
}



void wlmngr_adjust_radio_runtime(int idx)
{
    int i=0;
    char wlver[WL_VERSION_STR_LEN]= {0};
    char cmd[WL_SIZE_132_MAX]= {0};
    char buf[WL_CAP_STR_LEN]= {0};

    WL_RADIO(idx).wlCoreRev=wlmngr_getCoreRev(idx);
    WL_RADIO(idx).wlRate=wlmngr_getValidRate(idx, WL_RADIO(idx).wlRate);
    WL_RADIO(idx).wlMCastRate=  wlmngr_getValidRate(idx, WL_RADIO(idx).wlMCastRate);
    WL_RADIO(idx).wlBand= wlmngr_getValidBand(idx,WL_RADIO(idx).wlBand);

    wlcsm_strcpy(&WL_PHYTYPE(idx),wlmngr_getPhyType(idx));

    if(wlmngr_getVer(idx,wlver)) {
        wlcsm_strcpy(&(WL_RADIO(idx).wlVersion),wlver);
        WLCSM_TRACE(WLCSM_TRACE_LOG," version is:%s \r\n",WL_RADIO(idx).wlVersion );
    } else
        WLCSM_TRACE(WLCSM_TRACE_ERR," ERROR:could not get adapter version \r\n" );

    snprintf(cmd, sizeof(cmd), "wl%d", idx);
    wl_iovar_get(cmd, "cap", (void *)buf, WL_CAP_STR_LEN);


    if(strstr(buf, "afterburner")) {
        WL_RADIO(idx).wlHasAfterburner=TRUE;
    }
    if(strstr(buf, "ampdu")) {
        WL_RADIO(idx).wlAmpduSupported=FALSE;
    } else {
        /* A-MSDU doesn't work with AMPDU */
        if(strstr(buf, "amsdu")) {
            WL_RADIO(idx).wlAmsduSupported=FALSE;
        }
    }

    if(strstr(buf, "wme")) {
        WL_RADIO_WLHASWME(idx)=TRUE;
        for(i=0; i<WL_RADIO_WLNUMBSS(idx); i++) {
            WL_AP_WLWME(idx,i)=TRUE;
        }
    }

#ifdef WMF
    /* for WMF-- we enable then unconditionally for now because of 43602 dongle  */
    WL_RADIO_WLHASWMF(idx)=TRUE;
#endif

    for(i=0; i<WL_RADIO_WLNUMBSS(idx); i++) {
        if ( strncmp(WL_RADIO(idx).wlNmode, WL_OFF,strlen(WL_OFF)) &&
                ( strncmp(WL_PHYTYPE(idx), WL_PHY_TYPE_N,strlen(WL_PHY_TYPE_N)) ||
                  strncmp(WL_PHYTYPE(idx), WL_PHY_TYPE_AC,strlen(WL_PHY_TYPE_AC))) &&
                !strncmp(WL_APSEC(idx,i).wlWpa, "tkip",4))
            wlcsm_strcpy(&(WL_APSEC(idx,i).wlWpa), "tkip+aes");

    }
}


/*************************************************************//* *
  * @brief  internal API to write nvram by parameter values
  *
  * 	adjust wlmngr variables to solve the relationship between
  * 	variables and run_time configurationjust for one adapter,
  *
  * @return void
  ****************************************************************/
static  void _wlmmgr_vars_adjust(const unsigned int idx ,SYNC_DIRECTION direction)
{

    if ( direction==WL_SYNC_FROM_DM ) {

        _wlmngr_adjust_country_rev(idx, direction);
        wlmngr_adjust_GPIOOverlays(idx); /* GPIOOverlay is run time, only for this FROM DM direction */
        wlmngr_adjust_radio_runtime(idx);

    } else {

        WLCSM_TRACE(WLCSM_TRACE_LOG," !!!!TODO:--- Adjust VAR befor write to DM \r\n" );

    }
    wlmngr_update_possibble_channels(idx,direction);
}


#ifdef DSLCPE_1905
WLMNGR_1905_CREDENTIAL g_wlmngr_wl_credentials[MAX_WLAN_ADAPTER][WL_MAX_NUM_MBSSID];
typedef struct struct_value {
    int offset;
    char *name;
    char isInt;
} STRUCT_VALUE;


static int _wlmngr_check_config_status_change (const unsigned int idx , int *bssid,int *credential_changed, int *change_status)
{

    STRUCT_VALUE items[]= {
        {CREDENTIAL_VAROFF(wlSsid),	 	"ssid",		0},
        {CREDENTIAL_VAROFF(wlWpaPsk), 	        "wpa_psk",	0},
        {CREDENTIAL_VAROFF(wlAuthMode), 	"akm",		0},
        {CREDENTIAL_VAROFF(wlAuth),		"auth",		1},
        {CREDENTIAL_VAROFF(wlWep), 		"wep",		0},
        {CREDENTIAL_VAROFF(wlWpa), 		"crypto", 	0},
        {-1,NULL}
    };
    STRUCT_VALUE *item=items;
    WLMNGR_1905_CREDENTIAL    *pmssid;
    int i=0,ret=0;
    char buf[32],name[32],tmp[100],*str;
    int br=0;
    int oob=0;

    *bssid = 0;

    for (i = 0 ; i<WL_RADIO_WLMAXMBSS(idx); i++) {
        br = WL_BSSID_WLBRNAME(idx,i)[2] - 0x30;

        if ( br == 0 )
            snprintf(name, sizeof(name), "lan_wps_oob");
        else
            snprintf(name, sizeof(name), "lan%d_wps_oob", br);
        strncpy(buf, nvram_safe_get(name), sizeof(buf));
        if (buf && strcmp(buf,"enabled"))
            oob=2;
        else
            oob=1;

        if(((g_wlmngr_wl_credentials[idx][i].lan_wps_oob)&0xf)!=oob) {
            if(oob==1)  *change_status= WPS_1905_CONF_TO_UNCONF;
            else *change_status= WPS_1905_UNCONF_TO_CONF;
            *bssid=i;
            *credential_changed=0;
            g_wlmngr_wl_credentials[idx][i].lan_wps_oob=oob;
            ret=1;
        }

        if(ret==0)	 {
            if(oob==1)  *change_status= WPS_1905_CONF_NOCHANGE_UNCONFIGURED;
            else *change_status= WPS_1905_CONF_NOCHANGE_CONFIGURED;
        }

        snprintf(name, sizeof(name), "%s_", WL_BSSID_IFNAME(idx,i));
        pmssid=(WLMNGR_1905_CREDENTIAL *)(&(g_wlmngr_wl_credentials[idx][i]));
        for(item=items; item->name!=NULL; item++) {
            str=nvram_get(strcat_r(name, item->name, tmp));
            /* for akm, it is kind of special*/
            if(!strcmp(item->name,"akm") && (str==NULL||strlen(str)==0)) {
                str="open";
            }
            if(!item->isInt) {
                char *value=CREDENTIAL_STRVARVALUE(pmssid,item->offset);
                if((str==NULL && strlen(value)!=0)|| (str!=NULL && strcmp(str,value))) {
                    *bssid=i;
                    *credential_changed=1;
                    ret= 1;
                }
                if(ret) {
                    if(!str)  value[0]='\0';
                    else strcpy(value,str);
                }
            } else {
                int *value=CREDENTIAL_INTVARPTR(pmssid,item->offset);
                if(str!=NULL && atoi(str)!=*value) {
                    *bssid=i;
                    ret= 1;
                    *credential_changed=1;
                    *value=atoi(str);
                }

            }
        }
    }
    /* use that bit to indicat if this wlmngr boot time, if not boot time, use return value */
    if(!(g_wlmngr_wl_credentials[idx][0].lan_wps_oob&0x10)) {
        g_wlmngr_wl_credentials[idx][0].lan_wps_oob|=0x10;
        return 0;
    }
    return ret;
}

/** @brief  checking if wlan credential changed
*
*   check if wireless lan credential changed when some application change it from TR69 etc APPs
*/

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
        WLCSM_TRACE(WLCSM_TRACE_LOG, "Unable to create loopback socket\n");
        goto exit0;
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
        WLCSM_TRACE(WLCSM_TRACE_LOG, "Unable to setsockopt to loopback socket %d.\n", sock_fd);
        goto exit1;
    }

    if (bind(sock_fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        WLCSM_TRACE(WLCSM_TRACE_LOG, "Unable to bind to loopback socket %d\n", sock_fd);
        goto exit1;
    }
    WLCSM_TRACE(WLCSM_TRACE_LOG, "opened loopback socket %d in port %d\n", sock_fd, port);
    return sock_fd;

    /*  error handling */
exit1:
    close(sock_fd);

exit0:
    WLCSM_TRACE(WLCSM_TRACE_LOG, "failed to open loopback socket\n");
    return -1;
}


/*
* ===  FUNCTION  ======================================================================
*         Name:  _wlmngr_send_notification_1905
*  Description:
* =====================================================================================
*/

static int _wlmngr_send_notification_1905 (const unsigned int idx ,int bssid,int credential_changed, int conf_status)
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
            WPS_1905_NOTIFY_MESSAGE notify_msg;
            WPS_1905_MESSAGE *pmsg = (WPS_1905_MESSAGE *)malloc(buflen);
            if(pmsg) {
                notify_msg.confStatus=conf_status;
                notify_msg.credentialChanged=credential_changed;
                snprintf(notify_msg.ifName, sizeof(notify_msg.ifName), "%s",WL_BSSID_IFNAME(idx,bssid));

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
                free(pmsg);
                close(sock_fd);
                if (buflen != rc) {
                    printf("%s: sendto failed", __FUNCTION__);
                    return 1;
                } else {
                    return 0;
                }
            }
        }
    }
    return 1;
}


#endif
/*************************************************************//* *
  * @brief  internal API to write nvram by parameter values
  *
  *	some parameters mapping to nvram entries in the system where
  *	wlconf depends on to configure wl interfaces. This API will read
  *	the mapping and write each single Nvram entry.
  *
  * @return void
  ****************************************************************/
void _wlmngr_write_wl_nvram(const unsigned int idx)
{
    char name[128];
    int i=0,j=0,entries_num=0;

    WLCSM_NVRAM_MNGR_MAPPING *mapping;
    WLCSM_WLAN_ADAPTER_STRUCT *adapter;

    adapter=&(gp_adapter_objs[idx]);
    char *value,tmp_str[1024];
    entries_num=sizeof(g_wlcsm_nvram_mngr_mapping)/sizeof(WLCSM_NVRAM_MNGR_MAPPING);

    for(i=0; i<adapter->radio.wlNumBss; i++) {

        for ( j=0; j<entries_num; j++ ) {

            mapping= &(g_wlcsm_nvram_mngr_mapping[j]);
            if(mapping->type==MNGR_GENERIC_VAR) continue;
            else if((i==0 || ((i>0) && mapping->type==MNGR_SSID_SPECIFIC_VAR))) {
                if(!i)
                    snprintf(name, sizeof(name), "wl%d_%s",idx,mapping->nvram_var);
                else
                    snprintf(name, sizeof(name), "wl%d.%d_%s",idx,i,mapping->nvram_var);

                WLCSM_TRACE(WLCSM_TRACE_LOG," j:%d,name of nvram is:%s \r\n",j,name);

                value=wlcsm_mapper_get_mngr_value(idx,i, mapping,tmp_str);
                if(value)
                    wlcsm_nvram_set(name,value);
                WLCSM_TRACE(WLCSM_TRACE_LOG," j:%d,name of nvram is:%s and value:%s \r\n",j,name,value?value:"NULL");
            }
        }
    }
}


#if defined(HSPOT_SUPPORT)

static void wlmngr_HspotCtrl(const unsigned int idx )
{

    /* First to kill all hspotap process if already start*/
    bcmSystem("killall -q -15 hspotap 2>/dev/null");
    bcmSystem("/bin/hspotap&");
}
#endif

static bool enableBSD()
{
    int i=0;
    char buf[WL_MID_SIZE_MAX];
    nvram_set("bsd_role","0");
    if( act_wl_cnt == 0 ) return FALSE;
    for (i=0; i<WL_WIFI_RADIO_NUMBER; i++) {
        if( WL_RADIO_WLENBL(i) == TRUE && WL_RADIO_BSDROLE(i) > 0 ) {
            snprintf(buf, sizeof(buf), "%d", WL_RADIO_BSDROLE(i));
            nvram_set("bsd_role",buf);
            snprintf(buf, sizeof(buf), "%d", WL_RADIO_BSDPPORT(i));
            nvram_set("bsd_pport",buf);
            snprintf(buf, sizeof(buf), "%d", WL_RADIO_BSDHPORT(i));
            nvram_set("bsd_hpport",buf);
            snprintf(buf, sizeof(buf), "%s", WL_RADIO_BSDHELPER(i));
            nvram_set("bsd_helper",buf);
            snprintf(buf, sizeof(buf), "%s", WL_RADIO_BSDPRIMARY(i));
            nvram_set("bsd_primary",buf);
            return TRUE;
        }
    }
    return FALSE;
}

static void wlmngr_BSDCtrl(const unsigned int idx )
{
    /* First to kill all bsd process if already start*/
    bcmSystem("killall -q -15 bsd 2>/dev/null");
    if ( enableBSD() == TRUE )
        bcmSystem("/bin/bsd&");
}

static bool enableSSD()
{
    int i=0;
    char buf[WL_MID_SIZE_MAX];
    nvram_set("ssd_enable","0");
    if( act_wl_cnt == 0 ) return FALSE;
    for (i=0; i<WL_WIFI_RADIO_NUMBER; i++)
        if( WL_RADIO_WLENBL(i) == TRUE && WL_RADIO_SSDENABLE(i) > 0 ) {
            snprintf(buf, sizeof(buf), "%d", WL_RADIO_SSDENABLE(i));
            nvram_set("ssd_enable",buf);
            return TRUE;
        }
    return FALSE;
}

static void wlmngr_SSDCtrl(const unsigned int idx )
{
    /* First to kill all ssd process if already start*/
    bcmSystem("killall -q -15 ssd 2>/dev/null");
    if ( enableSSD() == TRUE )
        bcmSystem("/bin/ssd&");
}

static unsigned int _bits_count(unsigned int  n)
{
    unsigned int count = 0;
    while ( n > 0 ) {
        if ( n & 1 )
            count++;
        n >>= 1;
    }
    return count;
}

void wlmngr_postStart(const unsigned int idx )
{
    //check if support beamforming
    if( wlmngr_getCoreRev(idx) >= 40 ) {
        char tmp[100], prefix[] = "wlXXXXXXXXXX_";
        int txchain;
        snprintf(prefix, sizeof(prefix), "%s_", WL_BSSID_IFNAME(idx,0));
        wlmngr_getVarFromNvram(&txchain, strcat_r(prefix, "txchain", tmp), "int");

        if( _bits_count((unsigned int) txchain) > 1)
            WL_RADIO_WLTXBFCAPABLE(idx)=1;
    }
    if(!g_wlmngr_restart_all ||( g_wlmngr_restart_all && (wl_visal_start_cnt)==WL_WIFI_RADIO_NUMBER)) {
#ifdef __CONFIG_TOAD__
        bcmSystem("toad");
#endif
#ifdef EXT_ACS
        /* Usermode autochannel */
        if (wlmngr_detectAcsd())
            wlmngr_startAcsd(idx);
#endif
#ifdef __CONFIG_VISUALIZATION__
        bcmSystem("vis-dcon");
        bcmSystem("vis-datacollector");
#endif
        wl_visal_start_cnt=0;
    }

}

#ifdef DSLCPE_WLCSM_EXT

extern char *wlcsm_prefix_match(char *name);

static int _wlmngr_tobe_saved_nvram(char *name)
{

    char *prefix=wlcsm_prefix_match(name);
    int index=0;
    if(prefix) {
        int entries_num=sizeof(g_wlcsm_nvram_mngr_mapping)/sizeof(WLCSM_NVRAM_MNGR_MAPPING);
        char *var_name=name+strlen(prefix);
        char *nvram_var;
        for ( index=0; index<entries_num; index++) {
            nvram_var= g_wlcsm_nvram_mngr_mapping[index].nvram_var;
            if(!strncmp(var_name,nvram_var,strlen(nvram_var))) return 0;
        }
    }
    return 1;
}


/**
 * wlmngr write run time nvram to data module, only update nvram which is not in regular entries
 */
int  wlmngr_save_nvram(void)
{
    char *name;
    char *pair,*buf = malloc(MAX_NVRAM_SPACE);
    int pair_len_max=WL_LG_SIZE_MAX+WL_SIZE_132_MAX;
    if(!buf) {
        fprintf(stderr,"could not allocate memory for buf\n");
        return -1;
    } else if(!(pair=malloc(WL_LG_SIZE_MAX+WL_SIZE_132_MAX))) {
        fprintf(stderr,"could not allocate memory for pair\n");
        free(buf);
        return -1;
    }

    if(g_wifi_obj.nvram) {
        free(g_wifi_obj.nvram);
        g_wifi_obj.nvram=NULL;
    }

    if(!(g_wifi_obj.nvram=malloc(MAX_NVRAM_SPACE))) {
        fprintf(stderr,"could not allocate memory for buf\n");
        free(buf);
        free(pair);
        return -1;
    }

    nvram_getall(buf, MAX_NVRAM_SPACE*sizeof(char));
    memset(g_wifi_obj.nvram,0,MAX_NVRAM_SPACE);
    strcat(g_wifi_obj.nvram,"FFFF");
    for (name = buf; *name && (strlen(g_wifi_obj.nvram)<MAX_NVRAM_SPACE); name += strlen(name) + 1) {
        if(!strncmp(name,"wps_force_restart", strlen("wps_force_restart"))
                || !strncmp(name, "pair_len_byte=", strlen("pair_len_byte="))
                || !strncmp(name, "acs_ifnames", strlen("acs_ifnames"))
                || !strncmp(name, "wl_unit", strlen("wl_unit")))
            continue;
        if(_wlmngr_tobe_saved_nvram(name)) {

#if defined(HSPOT_SUPPORT)
            if(! wlcsm_hspot_var_isdefault(name)) {
#endif
                snprintf(pair, pair_len_max, "%03X%s", strlen(name), name);
                strcat(g_wifi_obj.nvram, pair);
#if defined(HSPOT_SUPPORT)
            }
#endif
        }
    }
    wlcsm_dm_save_nvram();
    free(g_wifi_obj.nvram);
    g_wifi_obj.nvram=NULL;
    free(buf);
    free(pair);
    return 0;
}

#endif
//**************************************************************************
// Function Name: initNvram
// Description  : initialize nvram settings if any.
// Parameters   : None.
// Returns      : None.
//**************************************************************************
#ifdef DSLCPE_WLCSM_EXT
#if defined(CONFIG_BCM96000) || defined(CONFIG_BCM960333) || defined(CONFIG_BCM96318) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963381)
/* Low end old platforms (no-fap, no-wfd)*/
void wlmngr_setupTPs(void)
{
    char cmd[WL_SIZE_132_MAX];
    int enbl_idx=0;
    int tp = (is_smp_system) ? 1 : 0;

    /* if there exists only one adapter, put it to TP1.
     * if there exists two adapters, put wl0 on TP0, wl1 on TP1.
     * if there exists three adapters, put wl0 on TP0, wl1 on TP1. wl2 on TP0
     */
    if (act_wl_cnt >= 2) {
        snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id 0", WL_BSSID_IFNAME(0,0));
        BCMWL_WLCTL_CMD(cmd);
        snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id %d",WL_BSSID_IFNAME(1,0), tp);
        BCMWL_WLCTL_CMD(cmd);
        if (act_wl_cnt == 3) {
            snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id %d", WL_BSSID_IFNAME(2,0), 0);
            BCMWL_WLCTL_CMD(cmd);
        }
    } else if (act_wl_cnt == 1 && WL_WIFI_RADIO_NUMBER >1) {
        if (WL_RADIO_WLENBL(0) == TRUE)
            enbl_idx=0;
        else
            enbl_idx=1;
        snprintf(cmd, sizeof(cmd), "wlctl -i %s tp_id %d", WL_BSSID_IFNAME(enbl_idx,0), tp);
        WLCSM_TRACE(WLCSM_TRACE_LOG," cmd:%s \r\n",cmd );
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

    pid_nic[0] = wlmngr_getPidByName("wl0-kthrd");
    pid_nic[1] = wlmngr_getPidByName("wl1-kthrd");
    pid_dhd[0] = wlmngr_getPidByName("dhd0_dpc");
    pid_dhd[1] = wlmngr_getPidByName("dhd1_dpc");
    pid_wfd[0] = wlmngr_getPidByName("wfd0-thrd");
    pid_wfd[1] = wlmngr_getPidByName("wfd1-thrd");

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
        pid_wl = wlmngr_getPidByName(process_name);

        if (pid_wl <= 0) {
            snprintf(process_name, sizeof(process_name), "wl%d-kthrd",radio);
            pid_wl = wlmngr_getPidByName(process_name);
        }

        if (pid_wl > 0) {
            snprintf(process_name, sizeof(process_name), "wfd%d-thrd",radio);
            pid_wfd = wlmngr_getPidByName(process_name);

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
void wlmngr_devicemode_overwrite(const unsigned int radio_idx)
{
    char name[100];
    int i = 0;
    struct nvram_tuple *t;
    if(!strncmp(nvram_safe_get("devicemode"), "1", 1)) {
        for (t = router_defaults_override_type1; t->name; t++) {
            if(!strncmp(t->name,"router_disable",14))
                continue;
            if (!strncmp(t->name, "wl_", 3)) {
                for (i = 0; i < WL_RADIO_WLNUMBSS(radio_idx) && i < WL_MAX_NUM_SSID; i++) {
                    WLCSM_TRACE(WLCSM_TRACE_ERR, " set %s= %s\r\n", t->name, t->value);
                    if(!i)
                        sprintf(name, "wl%d_%s", radio_idx, t->name + 3);
                    else
                        sprintf(name, "wl%d.%d_%s", radio_idx, i, t->name + 3);
                    WLCSM_TRACE(WLCSM_TRACE_ERR, " set %s= %s\r\n", name, t->value);
                    nvram_set(name, t->value);
                    wlcsm_nvram_update_runtime_mngr(name, t->value);
                    wlmngr_special_nvram_handler(name, t->value);
                }
            } else {
                nvram_set(t->name, t->value);
                WLCSM_TRACE(WLCSM_TRACE_ERR, " set %s= %s\r\n", t->name, t->value);
            }
        }
        nvram_set("pre_devicemode","1");
    }  else {
        char *premode= nvram_get("pre_devicemode");
        if(premode && (!strncmp(premode, "1", 1))) {
            fprintf(stderr," DEVICE MODE CHANGED!!! SUGGEST TO RESTORE DEFAULT AND RECONFIGURE DEVICE!!!! \r\n");
        }
    }
}

void wlmngr_pre_setup(const unsigned int idx )
{

    char *restart;
    restart = nvram_get("wps_force_restart");
    if ( restart!=NULL && (strncmp(restart, "y", 1)!= 0) &&  (strncmp(restart, "Y", 1)!= 0)  ) {
        nvram_set("wps_force_restart", "Y");
    } else {
        wlmngr_stopServices(idx);
        wlmngr_WlConfDown(idx);
        wlmngr_wlIfcDown(idx);
    }
}


static int wlmngr_stop_wps(void)
{
    int ret = 0;
    FILE *fp = NULL;
    char saved_pid[32],cmd[64];
    int i, wait_time = 3;
    pid_t pid;

    if (((fp = fopen("/tmp/wps_monitor.pid", "r")) != NULL) &&
            (fgets(saved_pid, sizeof(saved_pid), fp) != NULL)) {
        /* remove new line first */
        for (i = 0; i < sizeof(saved_pid); i++) {
            if (saved_pid[i] == '\n')
                saved_pid[i] = '\0';
        }
        saved_pid[sizeof(saved_pid) - 1] = '\0';
        snprintf(cmd,64,"kill %s",saved_pid);
        bcmSystem(cmd);

        do {
            if ((pid = get_pid_by_name("/bin/wps_monitor")) <= 0)
                break;
            wait_time--;
            sleep(1);
        } while (wait_time);

        if (wait_time == 0) {
            printf("Unable to kill wps_monitor!\n");
            ret=1;
        }
    }
    if (fp) {
        fclose(fp);
        if(!ret)
            bcmSystem("rm -rf /tmp/wps_monitor.pid");
    }

    return ret;
}

void wlmngr_setup(const unsigned int idx )
{

    char cmd[WL_SIZE_132_MAX];

#ifdef SUPPORT_WSC
    char *restart;
    bool is_wps_restarted = FALSE;
#endif

    wlmngr_setupTPs();
    wlmngr_devicemode_overwrite(idx);

    snprintf(cmd, sizeof(cmd), "wlctl -i wl%d phy_watchdog 0", idx);
    WLCSM_TRACE(WLCSM_TRACE_LOG," cmd:%s \r\n",cmd );
    BCMWL_WLCTL_CMD(cmd);

#ifdef SUPPORT_WSC
    restart = nvram_get("wps_force_restart");
    if ( restart!=NULL && (strncmp(restart, "y", 1)!= 0) &&  (strncmp(restart, "Y", 1)!= 0)  ) {
        nvram_set("wps_force_restart", "Y");
    } else
#endif

    {
        WLCSM_TRACE(WLCSM_TRACE_LOG," ............ \r\n" );

        // setup mac address
        wlmngr_setupMbssMacAddr(idx);

        WLCSM_TRACE(WLCSM_TRACE_LOG," ENUM INTERFACE NEEDS DM SUPPORT,THUS IT NEEDS LOCK \r\n" );

        wlmngr_enum_ifnames(idx);

        WLCSM_TRACE(WLCSM_TRACE_LOG," ENUM INTERFACE LOCK \r\n" );

        // enable/disable wireless
        if ( WL_RADIO_WLENBL(idx) == TRUE ) {

            if (!wlmngr_detectAcsd())
                wlmngr_autoChannel(idx);

            wlmngr_doWlConf(idx);
            wlmngr_setup_if_mac(idx);
            wlmngr_doSecurity(idx);
            //do WEP over WDS
            wlmngr_doWdsSec(idx);
        }
        //execute common service/daemon

        if(!g_wlmngr_restart_all ||( g_wlmngr_restart_all && (++wl_visal_start_cnt)==WL_WIFI_RADIO_NUMBER)) {
            wlmngr_startServices(idx);
        }

        //move "wl up" later after wps_monitor/wps cmds is due to Win7 logo requirement.
        //Win7 logo needs to see WPS IE present in Beacon right after configured AP within 1 second
        if ( WL_RADIO_WLENBL(idx) == TRUE ) {
            wlmngr_WlConfStart(idx);

            // turn on only active ifcs
            wlmngr_wlIfcUp(idx);
        }

        is_wps_restarted = TRUE; // WPS service is restarted
    }

    wlmngr_doQoS(idx);

    snprintf(cmd, sizeof(cmd), "wlctl -i wl%d phy_watchdog 1", idx);
    BCMWL_WLCTL_CMD(cmd);

    /* Enable flow cache */
    snprintf(cmd, sizeof(cmd), "wlctl -i wl%d fcache 1", idx);
    BCMWL_WLCTL_CMD(cmd);

    // send ARP packet with bridge IP and hardware address to device
    // this piece of code is -required- to make br0's mac work properly
    // in all cases
    snprintf(cmd, sizeof(cmd), "/usr/sbin/sendarp -s %s -d %s", WL_BSSID(idx,0).wlBrName,WL_BSSID(idx,0).wlBrName);
    bcmSystem(cmd);

#if defined(HSPOT_SUPPORT)
    wlmngr_HspotCtrl(idx);
#endif

    wlmngr_postStart(idx);

    wlmngr_setASPM(idx);
}

#endif


//**************************************************************************
// Function Name: clearSesLed
// Description  : clear SES LED.
// Parameters   : none.
// Returns      : none.
//**************************************************************************
void wlmngr_clearSesLed(const unsigned int idx )
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


#if defined(SUPPORT_WSC)
void wlmngr_startWsc(const unsigned int idx )
{
    int i=0, j=0;
    char buf[64];
    char *buf1;

    int br;
    char ifnames[128];

    strncpy(buf, nvram_safe_get("wl_unit"), sizeof(buf));

    if (buf[0] == '\0') {
        nvram_set("wl_unit", "0");
        i = 0;
        j = 0;
    } else {
        if ((buf[1] != '\0') && (buf[2] != '\0')) {
            buf[3] = '\0';
            j = isdigit(buf[2])? atoi(&buf[2]):0;
        } else {
            j = 0;
        }

        buf[1] = '\0';
        i = isdigit(buf[0]) ? atoi(&buf[0]):0;
    }

    WLCSM_TRACE(WLCSM_TRACE_LOG," .............. \r\n" );
    nvram_set("wps_mode", WL_APWPS_WLWSCMODE(i,j)); //enabled/disabled
    nvram_set("wl_wps_config_state", WL_APWPS_WLWSCAPMODE(i,j)); // 1/0
    nvram_set("wl_wps_reg",         "enabled");
    if (strlen(nvram_safe_get("wps_version2")) == 0)
#ifdef WPS_V2
        nvram_set("wps_version2", "enabled");
#else
        nvram_set("wps_version2", "disabled");
#endif
    /* Since 5.22.76 release, WPS IR is changed to per Bridge. Previous IR enabled/disabled is
    Per Wlan Intf */

    for ( br=0; br<MAX_BR_NUM; br++ ) {
        if ( br == 0 )
            snprintf(buf, sizeof(buf), "lan_ifnames");
        else
            snprintf(buf, sizeof(buf), "lan%d_ifnames", br);
        buf1=nvram_get(buf);
        if(!buf1) continue;
        else  strncpy(ifnames, buf1, sizeof(ifnames));

        if (ifnames[0] =='\0')
            continue;
        if ( br == 0 )
            snprintf(buf, sizeof(buf), "lan_wps_reg");
        else
            snprintf(buf, sizeof(buf), "lan%d_wps_reg", br);
        nvram_set(buf, "enabled");
    }

    if (nvram_get("wps_config_method") == NULL) { /* initialization */
        set_wps_config_method_default(); /* for DTM 1.1 test */
        if (nvram_match("wps_version2", "enabled"))
            nvram_set("_wps_config_method", "sta-pin"); /* This var is only for WebUI use, default to sta-pin */
        else
            nvram_set("_wps_config_method", "pbc"); /* This var is only for WebUI use, default to PBC */
    }

    nvram_set("wps_uuid",           "0x000102030405060708090a0b0c0d0ebb");
    nvram_set("wps_device_name",    "BroadcomAP");
    nvram_set("wps_mfstring",       "Broadcom");
    nvram_set("wps_modelname",      "Broadcom");
    nvram_set("wps_modelnum",       "123456");
    nvram_set("boardnum",           "1234");
    nvram_set("wps_timeout_enable",	"0");

    nvram_set("wps_config_command", "0");
    nvram_set("wps_status", "0");
    nvram_set("wps_method", "1");
    nvram_set("wps_config_command", "0");
    nvram_set("wps_proc_mac", "");
    nvram_set("wps_sta_pin", "00000000");
    nvram_set("wps_currentband", "");
    nvram_set("wps_autho_sta_mac", "00:00:00:00:00:00");
    nvram_set("router_disable", "0");


    if (strlen(nvram_safe_get("wps_device_pin")) != 8)
        wl_wscPinGen();

    if (nvram_match("wps_restart", "1")) {
        nvram_set("wps_restart", "0");
    } else {
        nvram_set("wps_restart", "0");
        nvram_set("wps_proc_status", "0");
    }
    bcmSystem("/bin/wps_monitor&");
}

#endif //end of SUPPORT_WSC


#if defined(SUPPORT_WSC)
void set_wps_config_method_default(void)
{
    if (nvram_match("wps_version2", "enabled")) {
        nvram_set("wps_config_method", "0x228c");
        /* WPS_UI_MEHTOD_PBC */
        nvram_set("wps_method", "2");

    } else
        nvram_set("wps_config_method", "0x84");
}
#endif /* SUPPORT_WSC */



//**************************************************************************
// Function Name: stopServices
// Description  : stop deamon services
//                which is required/common for each reconfiguration
// Parameters   : None
// Returns      : None
//**************************************************************************
void wlmngr_stopServices(const unsigned int idx )
{

#ifdef SUPPORT_WSC
    bcmSystem("killall -q -9 lld2d 2>/dev/null");
    bcmSystem("rm -rf /var/run/lld2d* 2>/dev/null");
    bcmSystem("killall -q -9 wps_ap 2>/dev/null");
    bcmSystem("killall -q -9 wps_enr 2>/dev/null");
    wlmngr_stop_wps();
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
#endif
#ifdef BCMWAPI_WAI
    bcmSystem("killall -q -15 wapid");
#endif
#if defined(HSPOT_SUPPORT)
    bcmSystem("killall -q -15 hspotap");
#endif

    bcmSystem("killall -q -15 bsd");
    bcmSystem("killall -q -15 ssd");
#ifdef __CONFIG_TOAD__
    bcmSystem("killall -q -15 toad");
#endif
#ifdef __CONFIG_VISUALIZATION__
    bcmSystem("killall -q -9 vis-datacollector");
    bcmSystem("killall -q -9 vis-dcon");
#endif
    usleep(300000);

}

/* Start acsd apps */
#ifdef EXT_ACS
void wlmngr_startAcsd( int idx)
{
    int timeout = 0;
    char cmd[WL_SIZE_132_MAX];

    /*only run acsd when radio is enabled and SSID is enabled, otherwize acsd may hangup there
     *preventing wlmngr finish restarting */

    if( WL_BSSID_WLENBLSSID(idx,0) && WL_RADIO_WLENBL(idx) ) {
        /* not background, due to acs init scan should be before wl up */
        bcmSystem("/bin/acsd");

        timeout = WL_RADIO(idx).wlCsScanTimer*60;

        /* Use default(one year)  whenever scan_timer is 0 */
        if(timeout) {
            snprintf(cmd, sizeof(cmd), "acs_cli -i %s acs_cs_scan_timer %d", WL_BSSID_IFNAME(idx,0), timeout);
            bcmSystem(cmd);
        }
    }
}
#endif

//**************************************************************************
// Function Name: startService
// Description  : start deamon services
//                which is required/common for each reconfiguration
// Parameters   : None
// Returns      : None
//**************************************************************************
#ifdef DSLCPE_WLCSM_EXT
void wlmngr_startServices(const unsigned int idx )
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

    WLCSM_TRACE(WLCSM_TRACE_LOG," .................... \r\n" );
#ifdef BCMWAPI_WAI
    bcmSystem("wapid");
#endif
    wlmngr_BSDCtrl(idx);
    wlmngr_SSDCtrl(idx);
}

#endif
//**************************************************************************
// Function Name: getVar
// Description  : get value by variable name.
// Parameters   : var - variable name.
// Returns      : value - variable value.
//**************************************************************************
void wlmngr_getVar(const unsigned int idx , char *varName, char *varValue)

{
    WLCSM_TRACE(WLCSM_TRACE_LOG," TODO:get var \r\n" );
}
//**************************************************************************
// Function Name: getVar
// Description  : get value with full input params.
//**************************************************************************
#define MAXVAR 10

char *wlmngr_getVarEx(const unsigned int idx , int argc, char **argv, char *varValue)
{

    char *var = argv[1];
    char *ret_str=NULL;

    WLCSM_TRACE(WLCSM_TRACE_LOG," !!!! try to get :%s \r\n",var );

    if (!strcmp(var, "wlNPhyRates")) {
        ret_str= wlmngr_getNPhyRates(idx, argc, argv, varValue);
    } else if (!strcmp(var, "wlChannelList"))
        ret_str= wlmngr_getChannelList(idx, argc, argv, varValue);
#if 0
    if (!strcmp(var, "wlCountryList"))
        wlmngr_getCountryList(idx, argc, argv, varValue);
#endif
    return ret_str;
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

    if(!strcmp(type,"int")) {
        if(*temp_s)
            *(int*)var = atoi(temp_s);
        else
            *(int*)var = 0;
    } else if(!strcmp(type,"string")) {
        if(*temp_s) {
            len = strlen(temp_s);

            /* Don't truncate tail-space if existed for SSID string */
            if (strstr(name, "ssid") == NULL) {
                if ((strstr(name, "wpa_psk") == NULL) && (len > 0) && (temp_s[len - 1] == ' '))
                    temp_s[len - 1] = '\0';
            }
            strcpy((char*)var,temp_s);
        } else {
            *(char*)var = 0;
        }
    } else {
        printf("wlmngr_getVarFromNvram:type not found\n");
    }
}


#ifdef DSLCPE_WLCSM_EXT
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
#endif




/*************************************************************//**
 * @brief API for restart and/or save dm in seperated thread.
 *
 * 	In order to make wlmngr keep serving request for var set/get
 * 	when receving restart, it will be execute in a sepearted
 * 	thread, this will make sure whichever process depending
 * 	on the reply for variables to unlock DM be able to keep going
 * 	until release DM lock. HTTPD is such a case.
 *
 * @return void
 ***************************************************************/
static void _wlmngr_restart_handler(void *arg)
{
    unsigned int radio_idx= g_radio_idx,idx=0;
    int from=WLCSM_MNGR_CMD_GET_SOURCE(radio_idx);
    int dm_direction= WL_SYNC_FROM_DM;

    /* after getting the g_radio_idx, unlock it */
    WLMNGR_RESTART_UNLOCK();
    WLCSM_TRACE(WLCSM_TRACE_LOG," restart radio_idx:%u \r\n",radio_idx);

    if(from==WLCSM_MNGR_RESTART_NVRAM) {
        WLCSM_TRACE(WLCSM_TRACE_LOG," restart from nvram and save to dm \r\n" );
        if(wlmngr_save_nvram())
            WLCSM_TRACE(WLCSM_TRACE_LOG," save nvram error \r\n" );
        else
            WLCSM_TRACE(WLCSM_TRACE_LOG," save nvram ok \r\n" );

        wlcsm_dm_save_config(0,from);
        WLCSM_TRACE(WLCSM_TRACE_LOG," restart from nvram and save to dm  done\r\n" );
    } else if(WLCSM_MNGR_CMD_GET_SAVEDM(radio_idx)) {
        /* if radio_idx is too big, we will save all instead of individual*/
        if(WLCSM_MNGR_CMD_GET_IDX(radio_idx)>=WL_WIFI_RADIO_NUMBER) {
            wlmngr_save_nvram();
            wlcsm_dm_save_config(0,from);
            WLCSM_MNGR_CMD_SET_IDX(radio_idx,0);
        } else
            wlcsm_dm_save_config(WLCSM_MNGR_CMD_GET_IDX(radio_idx)+1,from);
        WLCSM_TRACE(WLCSM_TRACE_LOG," save  %d all to mdm done restar wlan request \r\n",radio_idx );
    }

    if(from>WLCSM_MNGR_RESTART_FROM_MDM)  {
        WLCSM_TRACE(WLCSM_TRACE_LOG," reloading datamodel data...  \r\n" );

        /**Wait for following setting. A little tricky for multiple object set and restart. @JC*/
        sleep(3);
        if(arg) {
            WLCSM_TRACE(WLCSM_TRACE_LOG,"try to get wlmngr global structure lock  \r\n" );
            wlmngr_get_thread_lock(); /* if restart from a seperate thread, then lock */
            WLCSM_TRACE(WLCSM_TRACE_LOG,"getted wlmngr global structure lock  \r\n" );

        }

        idx= WLCSM_MNGR_CMD_GET_IDX(radio_idx);
        if(WLCSM_MNGR_CMD_GET_SOURCE(radio_idx)!=WLCSM_MNGR_RESTART_TR69C)
            idx= WLCSM_MNGR_CMD_GET_IDX(radio_idx)+1;

#ifdef NO_CMS
        WLCSM_DM_DECLARE(nocms);
        WLCSM_DM_SELECT(nocms,idx,from);
#else
#if defined(SUPPORT_DM_PURE181) ||  defined(PURE181)
        WLCSM_DM_DECLARE(tr181);
        WLCSM_DM_SELECT(tr181,idx,from);
#else
        WLCSM_DM_DECLARE(tr98);
        WLCSM_DM_SELECT(tr98,idx,from);
#endif
#endif
        if(from) {
            WLCSM_TRACE(WLCSM_TRACE_LOG," DM init error, thus,we will return from here, do not need to continue as DM has problem   \r\n" );
        } else {
            WLCSM_TRACE(WLCSM_TRACE_LOG," DM init done! \r\n" );
            if(!WL_WIFI_RADIO_NUMBER)
                /* first to deinit DM since DM was initalized successfully from here */
                WLCSM_TRACE(WLCSM_TRACE_LOG,"  WIRELESS INTERFACE:%d , WLCSM  error \r\n",WL_WIFI_RADIO_NUMBER);
        }
        if(arg) wlmngr_release_thread_lock();
        dm_direction= WL_SYNC_FROM_DM;
    } else
        dm_direction=WL_SYNC_TO_DM;

    if(from==WLCSM_MNGR_RESTART_NVRAM) {
        wlan_restart_all(WLCSM_MNGR_CMD_GET_WAIT(g_radio_idx),dm_direction);
    } else {
        wlmngr_restart_interface(radio_idx,dm_direction);
    }
    g_wlmngr_restart=0;

    WLCSM_TRACE(WLCSM_TRACE_LOG," restart done! \r\n" );
    if(!WLCSM_MNGR_CMD_GET_WAIT(radio_idx)) pthread_exit(0);
}

static int _wlmngr_start_wl_restart(t_WLCSM_MNGR_VARHDR *hdr)
{
    pthread_t save_dm_thread=0;
    unsigned int radio_idx=hdr->radio_idx;
    int from=WLCSM_MNGR_CMD_GET_SOURCE(radio_idx);
    WLCSM_SET_TRACE("wlmngr");
    g_wlmngr_restart=1;

    /* lock setting g_radio_idx */
    WLMNGR_RESTART_LOCK();

    g_radio_idx=radio_idx;

    WLCSM_TRACE(WLCSM_TRACE_LOG,"radio_idx:0x%08x, wait:%d,from:%d\r\n",radio_idx,WLCSM_MNGR_CMD_GET_WAIT(radio_idx),from);

    /* if restart from TR69C, always restart from a seperate thread or if wait for finish tag set*/
    if(WLCSM_MNGR_CMD_GET_WAIT(radio_idx) && from <WLCSM_MNGR_RESTART_FROM_MDM) {

        _wlmngr_restart_handler(NULL);

    } else {
        if(pthread_create(&save_dm_thread,NULL,(void *)&_wlmngr_restart_handler,(void *)&g_radio_idx)) {

            WLCSM_TRACE(WLCSM_TRACE_LOG," could not create save dm thread \r\n" );
            return 1 ;
        }
        /* detach it to release resource when it is done */
        pthread_detach(save_dm_thread);
    }

    WLCSM_TRACE(WLCSM_TRACE_LOG," restart handled  \r\n" );

    return 0;
}


static void _wlmngr_updateStationList(const unsigned int idx,unsigned int sub_idx)
{
    char *buf;
    struct stat statbuf;
    int i,j;
    static char cmd[WL_SIZE_132_MAX];
    static char wl_authefile[80];
    static char wl_assocfile[80];
    static char wl_authofile[80];
    int num_of_stas=0;
    FILE *fp = NULL;
    int buflen= 0;

    if (!WL_BSSID_WLENBLSSID(idx,sub_idx)) {
        WLCSM_TRACE(WLCSM_TRACE_ERR,"WHAT? WHY THIS AP IS NOT ENABLED?  \r\n" );
        return;
    }

    /* first to clear up existing sta informations and reconstruct latter */
    if (WL_AP_STAS(idx,sub_idx)!= NULL) {
        /*release sta mac memory*/
        for (i = 0; i < WL_AP(idx,sub_idx).numStations; i++) {
            if(WL_AP_STAS(idx,sub_idx)[i].macAddress) {
                free(WL_AP_STAS(idx,sub_idx)[i].macAddress);
                WL_AP_STAS(idx,sub_idx)[i].macAddress=NULL;
            }
        }
        free(WL_AP_STAS(idx,sub_idx));
        WL_AP_STAS(idx,sub_idx)=NULL;
    } else
        WLCSM_TRACE(WLCSM_TRACE_LOG," cusBssStaList is null \r\n" );
    WL_AP(idx,sub_idx).numStations = 0;
    /* for now stalist should be cleared */

    /* using wlctl to get current association list  */
    snprintf(wl_authefile, sizeof(wl_authefile), "/var/wl%d_authe", idx);
    snprintf(wl_assocfile, sizeof(wl_assocfile), "/var/wl%d_assoc", idx);
    snprintf(wl_authofile, sizeof(wl_authofile), "/var/wl%d_autho", idx);
    snprintf(cmd, sizeof(cmd), "wlctl -i %s authe_sta_list > /var/wl%d_authe",WL_BSSID_NAME(idx,sub_idx), idx);
    BCMWL_WLCTL_CMD(cmd);
    snprintf(cmd, sizeof(cmd), "wlctl -i %s assoclist > /var/wl%d_assoc",WL_BSSID_NAME(idx,sub_idx), idx);
    BCMWL_WLCTL_CMD(cmd);
    snprintf(cmd, sizeof(cmd), "wlctl -i %s autho_sta_list > /var/wl%d_autho",WL_BSSID_NAME(idx,sub_idx), idx);
    BCMWL_WLCTL_CMD(cmd);

    if(!stat(wl_authefile, &statbuf)) {

        fp = fopen(wl_authefile, "r");
        if(fp) {
            buflen= statbuf.st_size;
            buf = (char*)malloc(buflen);
            if (buf ) {
                for (num_of_stas=0;; num_of_stas++) {
                    if (!fgets(buf, buflen, fp) || buf[0]=='\n' || buf[0]=='\r') {
                        break;
                    }
                }
                if (num_of_stas > 0) {
                    int asize;
                    char *pa;
                    WL_AP_STAS(idx,sub_idx)= malloc (sizeof(WLCSM_WLAN_AP_STA_STRUCT) *num_of_stas );
                    if ( WL_AP_STAS(idx,sub_idx) == NULL ) {
                        printf("%s::%s@%d WL_AP_STAS(idx,sub_idx) malloc Error\n", __FILE__, __FUNCTION__, __LINE__ );
                        free(buf);
                        fclose(fp);
                        return;
                    }

                    rewind(fp);

                    for (j=0; j<num_of_stas; j++) {
                        if (fgets(buf, buflen, fp)) {
                            if (wlmngr_scanForAddr(buf, buflen, &pa, &asize)) {
                                asize= (asize > WL_MID_SIZE_MAX -1)?  ( WL_MID_SIZE_MAX -1):asize;
                                WL_AP_STAS(idx,sub_idx)[j].macAddress=malloc(asize);
                                if( !WL_AP_STAS(idx,sub_idx)[j].macAddress) {
                                    free(buf);
                                    fclose(fp);
                                    return;
                                }
                                strncpy(WL_AP_STAS(idx,sub_idx)[j].macAddress, pa, asize);
                                WL_AP_STAS(idx,sub_idx)[j].macAddress[asize] = '\0';
                                pa = &(WL_AP_STAS(idx,sub_idx)[j].macAddress[asize-1]);
                                if (*pa == '\n' || *pa == '\r') *pa='\0';
                                WL_AP_STAS(idx,sub_idx)[j].associated = wlmngr_scanFileForMAC(wl_assocfile, WL_AP_STAS(idx,sub_idx)[j].macAddress);
                                WL_AP_STAS(idx,sub_idx)[j].authorized = wlmngr_scanFileForMAC(wl_authofile, WL_AP_STAS(idx,sub_idx)[j].macAddress);
                                WL_AP(idx,sub_idx).numStations++;
                            }
                        }
                    }
                }
                free(buf);
            }
            fclose(fp);
        }
    }
    unlink(wl_authefile);
    unlink(wl_assocfile);
    unlink(wl_authofile);

}
static unsigned int  _wlmngr_sta_list_fillup(const unsigned int idx ,char *varValue)
{
    WL_STALIST_SUMMARIES *sta_summaries=(WL_STALIST_SUMMARIES *)varValue;
    unsigned int j,i=0,ret_size,numStations=0;
    WLCSM_WLAN_AP_STA_STRUCT *curBssStaList=NULL;
    WL_STATION_LIST_ENTRY *sta=sta_summaries->stalist_summary;

    for (i=0; i<WL_RADIO_WLNUMBSS(idx) && i < WL_MAX_NUM_SSID; i++) {
        WLCSM_TRACE(WLCSM_TRACE_LOG," sta for i:%d number:%d \r\n",i,WL_AP(idx,i).numStations);
        numStations+=WL_AP(idx,i).numStations;
        curBssStaList =WL_AP_STAS(idx,i);
        if(curBssStaList!=NULL) {
            WLCSM_TRACE(WLCSM_TRACE_LOG," not empty \r\n" );
            for (j = 0; j < WL_AP(idx,i).numStations; j++) {
                WLCSM_TRACE(WLCSM_TRACE_LOG," macaddress:%s \r\n",curBssStaList->macAddress );
                memcpy(sta->macAddress,curBssStaList->macAddress,18);
                sta->associated=curBssStaList->associated;
                sta->authorized=curBssStaList->authorized;
                sta->radioIndex=idx;
                sta->ssidIndex =i;
                memcpy(sta->ssid,WL_BSSID_WLSSID(idx,i),strlen(WL_BSSID_WLSSID(idx,i))+1);
                memcpy(sta->ifcName,WL_BSSID_NAME(idx,i),strlen(WL_BSSID_NAME(idx,i))+1);
                sta++;
                curBssStaList++;
            }
        } else WLCSM_TRACE(WLCSM_TRACE_LOG," Bss is emtpy? \r\n" );

    }
    WLCSM_TRACE(WLCSM_TRACE_LOG," total:%d \r\n",numStations );

    sta_summaries->num_of_stas=numStations;
    ret_size=(sta_summaries->num_of_stas)*sizeof(WL_STATION_LIST_ENTRY)+sizeof(WL_STALIST_SUMMARIES);
    varValue[ret_size]='\0';
    return ret_size+1;
}


int wlmngr_restart_interface(unsigned int radio_idx,SYNC_DIRECTION direction)
{
    int i=0;
#ifdef DSLCPE_1905
    int credential_changed=0;
    int config_status=0;
#endif
    unsigned int idx;
    brcm_get_lock("wps",200);
    if(!WLCSM_MNGR_CMD_GET_WAIT(radio_idx))
        wlmngr_get_thread_lock();
    idx=WLCSM_MNGR_CMD_GET_IDX(radio_idx);

    if(WLCSM_MNGR_CMD_GET_SOURCE(radio_idx)==WLCSM_MNGR_RESTART_TR69C && idx>0) {
        /*tr69 index starts at 1 */
        idx--;
    }

    act_wl_cnt = 0;
    for (i=0; i<WL_WIFI_RADIO_NUMBER; i++)
        act_wl_cnt += (WL_RADIO_WLENBL(i) == TRUE) ? 1 : 0;
    wlmngr_pre_setup(idx); /* shutdown all the services(apps) first */
    _wlmmgr_vars_adjust(idx,direction);
    _wlmngr_write_wl_nvram(idx);
    _wlmngr_nvram_adjust(idx,direction);
    if(!WLCSM_MNGR_CMD_GET_WAIT(radio_idx))
        wlmngr_release_thread_lock();
    wlmngr_setup(idx);
#ifdef DSLCPE_1905
    if(_wlmngr_check_config_status_change(idx,&i,&credential_changed,&config_status))
        _wlmngr_send_notification_1905(idx,i,credential_changed,config_status);
#endif
    /*The only reason here to save configurat again is to trigger CMS's RTL
     *to add wl interface to bridge */
    wlcsm_dm_save_config(idx+1,WLCSM_MNGR_CMD_GET_SOURCE(radio_idx));
    brcm_release_lock("wps");
    WLCSM_TRACE(WLCSM_TRACE_DBG, " WLMNGR RESTART INTERFACE DONE\n");
    return 0;
}

static int _wlmngr_handle_wlcsm_wl_sta_event (t_WLCSM_MNGR_VARHDR *hdr,char *varName,char *varValue)
{
    _wlmngr_updateStationList( WLCSM_MNGR_CMD_GET_IDX(hdr->radio_idx),hdr->sub_idx);
    if(g_wlcsm_dm_mngr_handlers[WLCSM_DM_MNGR_EVENT_STAS_CHANGED].func) {
        int numStations=0;
        char *staBuffer;
        int stalist_size, i=0, idx=WLCSM_MNGR_CMD_GET_IDX(hdr->radio_idx);
        for (i=0; i<WL_RADIO_WLNUMBSS(idx) && i < WL_MAX_NUM_SSID; i++)
            numStations+=WL_AP(idx,i).numStations;
        stalist_size=numStations*sizeof(WL_STATION_LIST_ENTRY)+sizeof(WL_STALIST_SUMMARIES)+1;
        staBuffer=malloc(stalist_size);
        if(staBuffer) {
            _wlmngr_sta_list_fillup( WLCSM_MNGR_CMD_GET_IDX(hdr->radio_idx), staBuffer);
            g_wlcsm_dm_mngr_handlers[WLCSM_DM_MNGR_EVENT_STAS_CHANGED].func(WLCSM_MNGR_CMD_GET_IDX(hdr->radio_idx),hdr->sub_idx, staBuffer);
            free(staBuffer);
        }
    }

#ifdef IDLE_PWRSAVE
    wlmngr_togglePowerSave();
#endif
    return 0;
}
static int  _wlmngr_handle_wlcsm_cmd_restart(t_WLCSM_MNGR_VARHDR *hdr,char *varName,char *varValue)
{

    if(!g_wlmngr_restart) {
        switch(WLCSM_MNGR_CMD_GET_SOURCE(hdr->radio_idx)) {

        case WLCSM_MNGR_RESTART_HTTPD:
        case WLCSM_MNGR_RESTART_MDM:
        case WLCSM_MNGR_RESTART_TR69C:
        case WLCSM_MNGR_RESTART_NVRAM: {

            /* for httpd,we expected all parameters has been setup correctly in the
             * global structure, and no structure varaible changes, so restart will not
             * involved in any DM operation,but in case it is asked to save to MDM, it
             * will be done in a seperated thread to make wlmngr still be responsivie
             * to httpd since httpd hold the lock. IDX is marked in case SAVEING to MDM
             * is needed
             */
            _wlmngr_start_wl_restart(hdr);
            WLCSM_TRACE(WLCSM_TRACE_LOG," to restart, but we returen here \r\n" );
            break;
        }
        default:
            WLCSM_TRACE(WLCSM_TRACE_LOG," restar wlan request from :%d	 \r\n",WLCSM_MNGR_CMD_GET_SOURCE(hdr->radio_idx) );
            break;
        }
    }
    return 0;
}




typedef  int (*MNGR_CMD_HANDLER)(t_WLCSM_MNGR_VARHDR  *hdr,char *name,char *value);

static MNGR_CMD_HANDLER g_mngr_cmd_handlers[]= {
    _wlmngr_handle_wlcsm_cmd_restart,
    _wlmngr_handle_wlcsm_wl_sta_event,
    _wlmngr_handle_wlcsm_cmd_validate_dmvar,
    _wlmngr_handle_wlcsm_cmd_get_dmvar,
    _wlmngr_handle_wlcsm_cmd_set_dmvar,
    _wlmngr_handle_wlcsm_cmd_reg_dm_event,
    _wlmngr_handle_wlcsm_cmd_pwrreset_event,
    _wlmngr_handle_wlcsm_cmd_setdmdbglevel_event,
};


static  int  _wlmngr_get_var(t_WLCSM_MNGR_VARHDR *hdr,char *varName,char *varValue,int oid)
{
    char *idxStr;
    char *inputVarName=varName;
    char varNameBuf[WL_MID_SIZE_MAX];
    char ifcName[WL_MID_SIZE_MAX];
    char *next;
    int ret=0;
    int i=0;
    int num=0;
    char mac[32];

    unsigned int idx=WLCSM_MNGR_CMD_GET_IDX(hdr->radio_idx);
    char *tmp = NULL;
    unsigned int wlcsm_mngr_cmd=WLCSM_MNGR_CMD_GET_CMD(hdr->radio_idx);
    if(wlcsm_mngr_cmd) {
        if(wlcsm_mngr_cmd>WLCSM_MNGR_CMD_LAST) {
            WLCSM_TRACE(WLCSM_TRACE_LOG," WLCSM CMD IS TOO BIG, NO SUCH COMMDN \r\n" );
            return 0;
        } else {
            return g_mngr_cmd_handlers[wlcsm_mngr_cmd-1](hdr,varName,varValue);
        }
    } else {

        if((varName==NULL || *varName=='\0')||
                ((hdr->sub_idx) >= WL_RADIO_WLMAXMBSS(WLCSM_MNGR_CMD_GET_IDX(hdr->radio_idx)))) {

            WLCSM_TRACE(WLCSM_TRACE_ERR," Invalid name!! or idx is too big :%d\r\n",hdr->radio_idx );
            return 0;
        }

        strncpy(varNameBuf,varName,sizeof(varNameBuf));
        varName = varNameBuf;
        if((next=strstr(varName," "))) {
            /* the varname is a special string that require extend care */
            char *argv[20];
            char *prev=varName;
            int argc=1;
            do {
                varName[next-varName]='\0';
                argv[argc++]=prev;
                prev=next+1;
            } while((next=strstr(prev," ")));

            argv[argc++]=prev;

            if(wlmngr_getVarEx(idx, argc, argv, varValue))
                return strlen(varValue)+1;
            else return 0;

        } else {

            int sub_idx=hdr->sub_idx;
            /* regulare varName handling here */
            idxStr=strstr(varName, WL_PREFIX);
            if(idxStr) {
                sub_idx = atoi(idxStr+strlen(WL_PREFIX)+2); /* wlXx */
                *idxStr = '\0';
            }

            ret=wlmngr_handle_special_var(hdr,varName,varValue,WLMNGR_VAR_OPER_GET);
            if(ret) return ret;
            else {

                if ( strcmp(varName, "wlCurIdxIfcName") == 0 ) {
                    snprintf(varValue, WL_MID_SIZE_MAX, "%s", WL_BSSID_IFNAME(idx,0));
                } else if (strcmp(varName, "wlBands") == 0 ) {
                    snprintf(varValue, WL_MID_SIZE_MAX, "%d", wlmngr_getBands(idx));
                } else if (strcmp(varName, "wlChanImState") == 0) {
                    snprintf(varValue, WL_MID_SIZE_MAX, "%d", wlmngr_getChannelImState(idx));
                } else if (strcmp(varName, "wlCurrentBw") == 0 ) {
                    snprintf(varValue, WL_MID_SIZE_MAX, "%d", ((wlmngr_getCurrentChSpec(idx) & WL_CHANSPEC_BW_MASK) >> WL_CHANSPEC_BW_SHIFT));
                } else if (strcmp(varName, "wlpwrsave") == 0) {
                    wlmngr_getPwrSaveStatus(idx, varValue);
                } else if (strcmp(varName,"wl_stalist_summaries") == 0) {
                    return _wlmngr_sta_list_fillup(idx,varValue);
#if defined(__CONFIG_HSPOT__)
                }  else if ( strcmp(varName, "wlEnableHspot") == 0 ) {
                    snprintf(varValue, WL_MID_SIZE_MAX, "%s", "2");
                }  else if ( strcmp(varName, "wlPassPoint") == 0 ) {
                    snprintf(varValue, WL_MID_SIZE_MAX, "%s", "1");
#endif
                } else if (strcmp(varName,"wdsscan_summaries") == 0) {
                    WL_WDSAPLIST_SUMMARIES *wdsap_summaries=(WL_WDSAPLIST_SUMMARIES *)varValue;
                    int ret_size=0;
                    wlmngr_scanWdsResult(idx);
                    if (WL_RADIO(idx).m_tblScanWdsMac != NULL) {
                        WL_FLT_MAC_ENTRY *entry = NULL;
                        int ap_count=0;
                        list_for_each(entry, (WL_RADIO(idx).m_tblScanWdsMac) ) {
                            WLCSM_TRACE(WLCSM_TRACE_LOG," mac:%s,ssid:%s \r\n",entry->macAddress,entry->ssid );
                            memcpy(wdsap_summaries->wdsaplist_summary[ap_count].mac,entry->macAddress,WL_MID_SIZE_MAX);
                            memcpy(wdsap_summaries->wdsaplist_summary[ap_count].ssid,entry->ssid,WL_SSID_SIZE_MAX);
                            WLCSM_TRACE(WLCSM_TRACE_LOG," mac:%s,ssid:%s \r\n",wdsap_summaries->wdsaplist_summary[ap_count].mac,
                                        wdsap_summaries->wdsaplist_summary[ap_count].ssid);
                            ap_count++;
                        }
                        wdsap_summaries->num_of_aps=ap_count;
                        WLCSM_TRACE(WLCSM_TRACE_LOG," there are :%d ap scanned \r\n",wdsap_summaries->num_of_aps );
                        ret_size=(wdsap_summaries->num_of_aps)*sizeof(WL_WDSAP_LIST_ENTRY);
                    } else {
                        wdsap_summaries->num_of_aps=0;
                        WLCSM_TRACE(WLCSM_TRACE_LOG," there is no scan ap find \r\n" );
                    }
                    ret_size+=sizeof(WL_WDSAPLIST_SUMMARIES);
                    varValue[ret_size]='\0';
                    return ret_size+1;
                } else if (strcmp(varName,"wl_mbss_summaries") == 0) {
                    int num_of_mbss= WL_RADIO_WLNUMBSS(idx);
                    WL_BSSID_SUMMARIES *bss_summaries=(WL_BSSID_SUMMARIES *)varValue;
                    WL_BSSID_SUMMARY  *summary=bss_summaries->bssid_summary;
                    bss_summaries->num_of_bssid=num_of_mbss-1;
#if !defined(HSPOT_SUPPORT)
                    bss_summaries->wlSupportHspot=WLHSPOT_NOT_SUPPORTED;
#else
                    bss_summaries->wlSupportHspot=1;
#endif
                    for (i = 0; i < num_of_mbss-1; i++) {
                        summary[i].wlEnbl=WL_BSSID_WLENBLSSID(idx,i+1);
                        summary[i].wlHide=WL_AP_WLHIDE(idx,i+1);
                        summary[i].wlIsolation=WL_AP_WLAPISOLATION(idx,i+1);
                        summary[i].wlWme=WL_AP_WLWME(idx,i+1);
                        summary[i].wlDisableWme=WL_AP_WLDISABLEWME(idx,i+1);
                        summary[i].wlEnableWmf=WL_BSSID_WLENABLEWMF(idx,i+1);
                        summary[i].wmfSupported=WL_RADIO_WLHASWMF(idx);
#if !defined(HSPOT_SUPPORT)
                        summary[i].wlEnableHspot=WLHSPOT_NOT_SUPPORTED;
#else
                        summary[i].wlEnableHspot=WL_AP_WLENABLEHSPOT(idx,i+1);
#endif
                        summary[i].max_clients=WL_AP_WLMAXASSOC(idx,i+1);

                        if(WL_BSSID_WLBSSID(idx,i+1))
                            strncpy(summary[i].bssid,WL_BSSID_WLBSSID(idx,i+1), sizeof(summary[i].bssid));
                        else
                            summary[i].bssid[0]='\0';

                        if(WL_BSSID_WLSSID(idx,i+1))
                            strncpy(summary[i].wlSsid,WL_BSSID_WLSSID(idx,i+1), sizeof(summary[i].wlSsid));
                        else
                            summary[i].wlSsid[0]='\0';

                    }

                    /* we only have Virual BSS filled up,so minus one */
                    varValue[sizeof(WL_BSSID_SUMMARIES)+(num_of_mbss-1)*sizeof(WL_BSSID_SUMMARY)]='\0';
                    return sizeof(WL_BSSID_SUMMARIES)+(num_of_mbss-1)*sizeof(WL_BSSID_SUMMARY)+1;

                }

                else if (!strncmp(varName, "wlWds",5) && sscanf(varName,"wlWds%d",&num)) {

                    if( WL_RADIO_WLWDS(idx)) {
                        WLCSM_TRACE(WLCSM_TRACE_LOG," WLWDS str:%s \r\n",WL_RADIO_WLWDS(idx));
                        for_each(mac,WL_RADIO_WLWDS(idx),next) {
                            if(i++==num)
                                snprintf(varValue, WL_MID_SIZE_MAX, "%s", mac);
                        }
                    } else {
                        varValue[0]='\0';
                        WLCSM_TRACE(WLCSM_TRACE_LOG," wlWDS is NULL, the varName is:%s \r\n",varName );
                    }
                }

                else if (nvram_match("wps_version2", "enabled") && strcmp(varName, "wlWscAuthoStaMac") == 0) {
                    tmp = nvram_safe_get("wps_autho_sta_mac");
                    snprintf(varValue, WL_MID_SIZE_MAX, "%s", tmp);

                } else if (strcmp(varName, "wlSsidList") == 0 ) {
                    wlmngr_printSsidList(idx, varValue);
                } else if(strcmp(varName, "wlCurrentChannel") == 0 ) {
                    wlmngr_getCurrentChannel(idx);
                    snprintf(varValue, WL_MID_SIZE_MAX, "%d", WL_RADIO_WLCURRENTCHANNEL(idx));
                } else if ( strcmp(varName, "wlInterface") == 0 ) {
                    bcmGetWlName(0, 0, ifcName);
                    if ( bcmIsValidWlName(ifcName) == TRUE )
                        strcpy(varValue, "1");
                    else
                        strcpy(varValue, "0");
                } else if ( strcmp(varName, "lan_hwaddr") == 0 ) {
                    wlmngr_getHwAddr(idx, IFC_BRIDGE_NAME, varValue);
                    bcmProcessMarkStrChars(varValue);
                } else if(strcmp(varName, "wlInfo") == 0 ) {
                    wlmngr_getWlInfo(idx, varValue, "wlcap");
#ifndef SUPPORT_SES
                } else if(strcmp(varName, "wlSesAvail") == 0 ) {
                    strcpy(varValue, "0");
#endif
                } else {
                    char *temp=NULL;
                    if (strcmp(varName, "wlSsid_2") == 0 )
                        varName="wlSsid";
                    else if (strcmp(varName, "wlEnbl_2") == 0 )
                        varName="wlEnblSsid";
                    if(!oid)
                        temp = wlcsm_dm_mngr_get_all_value(idx,sub_idx,varName,varValue);
                    else
                        temp=wlcsm_dm_mngr_get_value(idx,sub_idx,varName,varValue,oid);
                    if(!temp)
                        return 0;
                }

            }
        }
    }
    varName = inputVarName;
    ret=strlen(varValue)+1;
    return ret;
}

static int _wlmngr_handle_wlcsm_cmd_get_dmvar(t_WLCSM_MNGR_VARHDR  *hdr,char *varName,char *varValue)
{
    unsigned int mngr_oid=0;
    WLCSM_NAME_OFFSET *name_offset= wlcsm_dm_get_mngr_entry(hdr,varValue,&mngr_oid);

    if(!name_offset) {
        WLCSM_TRACE(WLCSM_TRACE_LOG," reurn not successull \r\n" );
        return 0;
    } else {
        WLCSM_TRACE(WLCSM_TRACE_LOG," mngr_oid:%u \r\n",mngr_oid);
        WLCSM_MNGR_CMD_SET_CMD(hdr->radio_idx,0);
        return _wlmngr_get_var(hdr,name_offset->name,varValue,mngr_oid);
    }
}


int wlmngr_get_var(t_WLCSM_MNGR_VARHDR *hdr,char *varName,char *varValue)
{
    return _wlmngr_get_var(hdr,varName,varValue,0);
}


int wlmngr_set_var(t_WLCSM_MNGR_VARHDR *hdr,char *varName,char *varValue)
{

    unsigned int wlcsm_mngr_cmd=WLCSM_MNGR_CMD_GET_CMD(hdr->radio_idx);
    int ret=0;
    //WLCSM_TRACE(WLCSM_TRACE_LOG,"%d:%d set_var:%s ,varValue:%s\r\n",hdr->radio_idx,hdr->sub_idx,varName,varValue );
    if(wlcsm_mngr_cmd) {
        if(wlcsm_mngr_cmd>WLCSM_MNGR_CMD_LAST) {
            WLCSM_TRACE(WLCSM_TRACE_LOG," WLCSM CMD IS TOO BIG, NO SUCH COMMAND \r\n" );
            return -1;
        } else {
            return g_mngr_cmd_handlers[wlcsm_mngr_cmd-1](hdr,varName,varValue);
        }
    } else {
        ret=wlmngr_handle_special_var(hdr,varName,varValue,WLMNGR_VAR_OPER_SET);
        if(ret) return 0;
        else
            return wlcsm_dm_mngr_set_all_value(WLCSM_MNGR_CMD_GET_IDX(hdr->radio_idx),
                                               hdr->sub_idx,varName,varValue);
        return ret;
    }
}
/* End of file */
