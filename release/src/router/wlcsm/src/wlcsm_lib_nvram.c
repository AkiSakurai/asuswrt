/*************************************************
* <:copyright-BRCM:2013:proprietary:standard
*
*    Copyright (c) 2013 Broadcom
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
/**
 *	@file	 wlcsm_lib_nvram.c
 *	@brief	 wlcsm nvram related functions
 *
 *	wlcsm nvram acts as middleware between user application and
 *	nvram storage in kernel.Nvram item value change is closely
 *	monitored in a seperated thread and broadcasted to user applications
 *	which are using nvram middleware. User application register its hook
 *	to respond to nvram varibble changes.It is Application's sole responsibility
 *	to react to the change.Nvram middle ware has no knowldge of App's intersts.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <bcmnvram.h>
#include <linux/netlink.h>
#include <linux/in.h>
#include <linux/if.h>
#include <errno.h>
#include  <pthread.h>
#include <sys/time.h>
#include  <sys/prctl.h>
#include  <stdarg.h>
#include "wlcsm_linux.h"
#include "wlcsm_lib_api.h"
#include "wlcsm_lib_netlink.h"
#include "wlcsm_lib_nvram.h"

#ifdef WLCSM_DEBUG
#include <sys/time.h>
unsigned int g_WLCSM_TRACE_LEVEL = 1;
char g_WLCSM_TRACE_PROC[32] = {0};
#endif

#define m_NO_CREATE 0
#define m_CREATE_NEW 1

#define _FIND_LOCAL_NVRAM(name,tuple,index) \
	do { \
		int i=_hash(name);\
		*(index)=i;\
		for(tuple=wlcsm_local_nvram_hash[i];tuple&&strcmp(tuple->name,name);tuple=tuple->next) { } \
	} while(0)

#define ROOM_AVAILABLE(p,n,v) (strlen(v)<=strlen(p->value)) /**< check if the memory of pointer p has enough room to hold name and value pair */

WLCSM_EVENT_HOOK_FUNC g_WLCSM_EVENT_HOOKS[WLCSM_EVT_LAST]; 	/**< specic event hook as some apps only intersted in specific event */
WLCSM_EVENT_HOOK_GENERICFUNC  g_WLCSM_EVENT_GENERIC_HOOK=NULL;	/**< generic hook for listening to all wlcsm events */


pthread_mutex_t g_WLCSM_NVRAM_MUTEX= PTHREAD_MUTEX_INITIALIZER; /**< mutex for protecting nvram manipulation */

#define _NVRAM_UNLOCK() pthread_mutex_unlock(&g_WLCSM_NVRAM_MUTEX)
#define _NVRAM_LOCK()  pthread_mutex_lock(&g_WLCSM_NVRAM_MUTEX)

static char g_temp_buf[MAX_NLRCV_BUF_SIZE];
static struct nvram_tuple * wlcsm_local_nvram_hash[32] = { NULL }; /**< hashtable for local nvram cache  */


typedef struct t_wlcsm_trace_match {
    char name[16];
    unsigned char level;
} t_WLCSM_TRACE_MATCH;

t_WLCSM_TRACE_MATCH trace_matches[]= {
    { "clr", WLCSM_TRACE_NONE  },
    { "dbg", WLCSM_TRACE_DBG  },
    { "err", WLCSM_TRACE_ERR  },
    { "log", WLCSM_TRACE_LOG  },
    { "func", WLCSM_TRACE_FUNC  },
    { "pkt", WLCSM_TRACE_PKT  }
};


/*the following definition is from wldef.h*/
#define WL_MID_SIZE_MAX  32
#define WL_SSID_SIZE_MAX 48
#define WL_WEP_KEY_SIZE_MAX WL_MID_SIZE_MAX
#define WL_WPA_PSK_SIZE_MAX  72  // max 64 hex or 63 char
#define WL_UUID_SIZE_MAX  40

#define WL_DEFAULT_VALUE_SIZE_MAX  255
#define WL_DEFAULT_NAME_SIZE_MAX  36
#define WL_WDS_SIZE_MAX  80
#define WL_MACFLT_NUM 64
#define WL_SINGLEMAC_SIZE 18
/* internal structure */

/*Differennt Nvram variable has different value length. To keep the Hash table static and sequence,
when one nvrma variable is inserted into hash table, the location will not dynamic change.
This structure is used to keep nvram name and value length*/
/* When new nvram variable is defined and max length is more than WL_DEFAULT_VALUE_SIZE_MAX,
the name and max length should be added into var_len_tab*/

struct   nvram_var_len_table {
    char *name;
    unsigned int  max_len;
};
#define  _LOCAL_SYNC_ENABLED_  (g_WLCSM_IS_DAEMON==m_AS_DAEMON)
#define  _STR_VALUE_EQUAL(v1,v2) ((v1 && v2 && !strcmp(v1,v2)) || (!v1 && !v2))
#define  _CUSTOM_VALUE_SIZE_MAX(name) (WLCSM_NAMEVALUEPAIR_MAX-(strlen(name)+sizeof(int)*2)-1)

/*nvram variable vs max length table*/
struct nvram_var_len_table var_len_tab[] = {
    {"wsc_ssid",     WL_SSID_SIZE_MAX+1},
    {"wsc_uuid",    WL_UUID_SIZE_MAX+1},
    {"wps_ssid",     WL_SSID_SIZE_MAX+1},
    {"wps_uuid",    WL_UUID_SIZE_MAX+1},
    {"radius_key",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"wpa_psk",    WL_WPA_PSK_SIZE_MAX+1},
    {"key1",          WL_MID_SIZE_MAX+1 },
    {"key2",          WL_MID_SIZE_MAX+1 },
    {"key3",          WL_MID_SIZE_MAX+1 },
    {"key4",          WL_MID_SIZE_MAX+1 },
    {"wds",            WL_WDS_SIZE_MAX },
    {"maclist",        WL_SINGLEMAC_SIZE * WL_MACFLT_NUM },
    {"maclist_x",        WL_SINGLEMAC_SIZE * WL_MACFLT_NUM + 1 },
    {"lan_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"lan1_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"lan2_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"lan3_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"lan4_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"br0_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"br1_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"br2_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"br3_ifnames",  WL_DEFAULT_VALUE_SIZE_MAX},
    {"wldbg",           1024 },
    {"netauthlist",     128 },
    {"oplist",          128 },
    {"osu_frndname",    51 },
    {"osu_icons",       36 },
    {"osu_uri",         128 },
    {"realmlist",       128 },
    {"venuelist",       320 },
    {"3gpplist",       	128},
    {"osu_servdesc",   	128},
    {"concaplist",   	128},
    {"qosmapie",   	80},
    {"radarthrs",   	128},
    {"toa-sta-1",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"toa-sta-2",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"toa-sta-3",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"toa-sta-4",    WL_DEFAULT_VALUE_SIZE_MAX},
#ifdef BCA_HNDROUTER
    /* pa nvram parameters */
    {"pa5ga0",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5ga1",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5ga2",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5ga3",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g40a0",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g40a1",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g40a2",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g40a3",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g80a0",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g80a1",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g80a2",    WL_DEFAULT_VALUE_SIZE_MAX},
    {"pa5g80a3",    WL_DEFAULT_VALUE_SIZE_MAX},
#endif /* BCA_HNDROUTER */
    {"vifs",			WL_DEFAULT_VALUE_SIZE_MAX},
    {"rc_support",		1012},
    {"qos_rulelist",		_CUSTOM_VALUE_SIZE_MAX("qos_rulelist")},
    {"vts_rulelist",		_CUSTOM_VALUE_SIZE_MAX("vts_rulelist")},
    {"game_vts_rulelist",	_CUSTOM_VALUE_SIZE_MAX("game_vts_rulelist")},
    {"nc_setting_conf",		_CUSTOM_VALUE_SIZE_MAX("nc_setting_conf")},
    {"asus_device_list",	_CUSTOM_VALUE_SIZE_MAX("asus_device_list")},
    {"vpnc_clientlist",		_CUSTOM_VALUE_SIZE_MAX("vpnc_clientlist")},
    {"wtf_game_list",		_CUSTOM_VALUE_SIZE_MAX("wtf_game_list")},
    {"wtf_server_list",		_CUSTOM_VALUE_SIZE_MAX("wtf_server_list")},
    {"wtf_rulelist",		_CUSTOM_VALUE_SIZE_MAX("wtf_rulelist")},
    {"sshd_hostkey",		_CUSTOM_VALUE_SIZE_MAX("sshd_hostkey")},
    {"sshd_dsskey",		_CUSTOM_VALUE_SIZE_MAX("sshd_dsskey")},
    {"sshd_ecdsakey",		_CUSTOM_VALUE_SIZE_MAX("sshd_ecdsakey")},
    {"sched",			590},
    {"MULTIFILTER_MAC",		32*16},
    {"MULTIFILTER_DEVICENAME",	_CUSTOM_VALUE_SIZE_MAX("MULTIFILTER_DEVICENAME")},
    {"MULTIFILTER_MACFILTER_DAYTIME",	_CUSTOM_VALUE_SIZE_MAX("MULTIFILTER_MACFILTER_DAYTIME")},
    {"MULTIFILTER_TMP",		_CUSTOM_VALUE_SIZE_MAX("MULTIFILTER_TMP")},
    {"wrs_rulelist",		_CUSTOM_VALUE_SIZE_MAX("wrs_rulelist")},
    {"wrs_app_rulelist",	48*16},
    {"client_info_tmp",		_CUSTOM_VALUE_SIZE_MAX("client_info_tmp")},
    {"asus_device_list",	_CUSTOM_VALUE_SIZE_MAX("asus_device_list")},
    {"dhcp_staticlist",		_CUSTOM_VALUE_SIZE_MAX("dhcp_staticlist")},
    {"lan_route",		_CUSTOM_VALUE_SIZE_MAX("lan_route")},
    {"sr_rulelist",		_CUSTOM_VALUE_SIZE_MAX("sr_rulelist")},
    {"autofw_rulelist",		_CUSTOM_VALUE_SIZE_MAX("autofw_rulelist")},
    {"vts_rulelist",		_CUSTOM_VALUE_SIZE_MAX("vts_rulelist")},
    {"url_rulelist",		_CUSTOM_VALUE_SIZE_MAX("url_rulelist")},
    {"keyword_rulelist",	_CUSTOM_VALUE_SIZE_MAX("keyword_rulelist")},
    {"filter_lwlist",		_CUSTOM_VALUE_SIZE_MAX("filter_lwlist")},
    {"ipv6_fw_rulelist",	_CUSTOM_VALUE_SIZE_MAX("ipv6_fw_rulelist")},
    {"custom_clientlist",	_CUSTOM_VALUE_SIZE_MAX("custom_clientlist")},
    {"qos_irates",		512},
    {"PM_LETTER_CONTENT",	512},
    {"fb_comment",		_CUSTOM_VALUE_SIZE_MAX("fb_comment")},
    {"tl_date_start",		512},
    {"tl_cycle",		_CUSTOM_VALUE_SIZE_MAX("tl_cycle")},
    {"wans_routing_rulelist",	_CUSTOM_VALUE_SIZE_MAX("wans_routing_rulelist")},
    {"qos_orates",		_CUSTOM_VALUE_SIZE_MAX("qos_orates")},
    {"yadns_rulelist",		_CUSTOM_VALUE_SIZE_MAX("yadns_rulelist")},
    {"share_link_param",	_CUSTOM_VALUE_SIZE_MAX("share_link_param")},
    {"share_link_result",	_CUSTOM_VALUE_SIZE_MAX("share_link_result")},
    {"share_link_host",		_CUSTOM_VALUE_SIZE_MAX("share_link_host")},
    {"captive_portal",		_CUSTOM_VALUE_SIZE_MAX("captive_portal")},
    {"captive_portal_adv_profile",	_CUSTOM_VALUE_SIZE_MAX("captive_portal_adv_profile")},
    {"wollist",			_CUSTOM_VALUE_SIZE_MAX("wollist")},
    {"nc_setting_conf",		_CUSTOM_VALUE_SIZE_MAX("nc_setting_conf")},
    {"ipsec_profile_1",		_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_1")},
    {"ipsec_profile_2",		_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_2")},
    {"ipsec_profile_3",		_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_3")},
    {"ipsec_profile_4",		_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_4")},
    {"ipsec_profile_5",		_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_5")},
    {"ipsec_client_list_1",	_CUSTOM_VALUE_SIZE_MAX("ipsec_client_list_1")},
    {"ipsec_client_list_2",	_CUSTOM_VALUE_SIZE_MAX("ipsec_client_list_2")},
    {"ipsec_client_list_3",	_CUSTOM_VALUE_SIZE_MAX("ipsec_client_list_3")},
    {"ipsec_client_list_4",	_CUSTOM_VALUE_SIZE_MAX("ipsec_client_list_4")},
    {"ipsec_client_list_5",	_CUSTOM_VALUE_SIZE_MAX("ipsec_client_list_5")},
    {"ipsec_profile_client_1",	_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_client_1")},
    {"ipsec_profile_client_2",	_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_client_2")},
    {"ipsec_profile_client_3",	_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_client_3")},
    {"ipsec_profile_client_4",	_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_client_4")},
    {"ipsec_profile_client_5",	_CUSTOM_VALUE_SIZE_MAX("ipsec_profile_client_5")},
    {"qos_bw_rulelist",		_CUSTOM_VALUE_SIZE_MAX("qos_bw_rulelist")},
    {"filter_lwlist",		_CUSTOM_VALUE_SIZE_MAX("filter_lwlist")},
    {"cloud_sync",		_CUSTOM_VALUE_SIZE_MAX("cloud_sync")},
    {"vpnc_pptp_options_x_list",_CUSTOM_VALUE_SIZE_MAX("vpnc_pptp_options_x_list")},
    {"sshd_authkeys",		_CUSTOM_VALUE_SIZE_MAX("sshd_authkeys")},
    {"tr_ca_cert",		_CUSTOM_VALUE_SIZE_MAX("tr_ca_cert")},
    {"tr_client_cert",		_CUSTOM_VALUE_SIZE_MAX("tr_client_cert")},
    {"tr_client_key",		_CUSTOM_VALUE_SIZE_MAX("tr_client_key")},
    {"vpn_serverx_clientlist",	_CUSTOM_VALUE_SIZE_MAX("vpn_serverx_clientlist")},  
    {"vpn_server_ccd_cal",	_CUSTOM_VALUE_SIZE_MAX("vpn_server_ccd_cal")},
    {"vpn_crt_server_static",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_static")},
    {"vpn_crt_server_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_ca")},
    {"vpn_crt_server_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_crt")},
    {"vpn_crt_server_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_key")},
    {"vpn_crt_server_client_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_client_crt")},
    {"vpn_crt_server_client_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_client_key")},
    {"vpn_crt_server_dh",		_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_dh")},
    {"vpn_crt_server_crl",		_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server_crl")},
    {"vpn_crt_server1_static",		_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_static")},
    {"vpn_crt_server1_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_ca")},
    {"vpn_crt_server1_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_crt")},
    {"vpn_crt_server1_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_key")},
    {"vpn_crt_server1_client_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_client_crt")},
    {"vpn_crt_server1_client_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_client_key")},
    {"vpn_crt_server1_dh",		_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_dh")},
    {"vpn_crt_server1_crl",		_CUSTOM_VALUE_SIZE_MAX("vpn_crt_server1_crl")},
    {"vpn_crt_client_static",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client_static")},
    {"vpn_crt_client_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client_ca")},
    {"vpn_crt_client_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client_crt")},
    {"vpn_crt_client_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client_key")},
    {"vpn_crt_client_crl",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client_crl")},
    {"vpn_crt_client1_static",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client1_static")},
    {"vpn_crt_client1_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client1_ca")},
    {"vpn_crt_client1_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client1_crt")},
    {"vpn_crt_client1_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client1_key")},
    {"vpn_crt_client1_crl",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client1_crl")},
    {"vpn_crt_client2_static",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client2_static")},
    {"vpn_crt_client2_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client2_ca")},
    {"vpn_crt_client2_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client2_crt")},
    {"vpn_crt_client2_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client2_key")},
    {"vpn_crt_client2_crl",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client2_crl")},
    {"vpn_crt_client3_static",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client3_static")},
    {"vpn_crt_client3_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client3_ca")},
    {"vpn_crt_client3_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client3_crt")},
    {"vpn_crt_client3_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client3_key")},
    {"vpn_crt_client3_crl",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client3_crl")},
    {"vpn_crt_client4_static",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client4_static")},
    {"vpn_crt_client4_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client4_ca")},
    {"vpn_crt_client4_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client4_crt")},
    {"vpn_crt_client4_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client4_key")},
    {"vpn_crt_client4_crl",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client4_crl")},
    {"vpn_crt_client5_static",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client5_static")},
    {"vpn_crt_client5_ca",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client5_ca")},
    {"vpn_crt_client5_crt",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client5_crt")},
    {"vpn_crt_client5_key",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client5_key")},
    {"vpn_crt_client5_crl",	_CUSTOM_VALUE_SIZE_MAX("vpn_crt_client5_crl")},
    {"vpn_server_custom",	_CUSTOM_VALUE_SIZE_MAX("vpn_server_custom")},
    {"vpn_client1_custom",	_CUSTOM_VALUE_SIZE_MAX("vpn_client1_custom")},
    {"vpn_client2_custom",	_CUSTOM_VALUE_SIZE_MAX("vpn_client2_custom")},
    {"vpn_client3_custom",	_CUSTOM_VALUE_SIZE_MAX("vpn_client3_custom")},
    {"vpn_client4_custom",	_CUSTOM_VALUE_SIZE_MAX("vpn_client4_custom")},
    {"vpn_client5_custom",	_CUSTOM_VALUE_SIZE_MAX("vpn_client5_custom")},
    {"pptpd_clientlist",	_CUSTOM_VALUE_SIZE_MAX("pptpd_clientlist")},
    {"pptpd_sr_rulelist",	_CUSTOM_VALUE_SIZE_MAX("pptpd_sr_rulelist")},
    {"vpnc_dev_policy_list",	_CUSTOM_VALUE_SIZE_MAX("vpnc_dev_policy_list")},
    {"vpnc_dev_policy_list_tmp",_CUSTOM_VALUE_SIZE_MAX("vpnc_dev_policy_list_tmp")},
    {"vlan_rulelist",		_CUSTOM_VALUE_SIZE_MAX("vlan_rulelist")},
    {"subnet_rulelist",		_CUSTOM_VALUE_SIZE_MAX("subnet_rulelist")},
    {"gvlan_rulelist",		_CUSTOM_VALUE_SIZE_MAX("gvlan_rulelist")},
    {"cfg_device_list",		_CUSTOM_VALUE_SIZE_MAX("cfg_device_list")},
    {"cfg_relist",		_CUSTOM_VALUE_SIZE_MAX("cfg_relist")},
    {"acs_excl_chans",		_CUSTOM_VALUE_SIZE_MAX("wlx_acs_excl_chans")},
};

#define VAR_LEN_COUNT (sizeof(var_len_tab) /sizeof(struct nvram_var_len_table))

#ifndef BCA_HNDROUTER
static char *g_WL_MATCH_NAMES[]= { "wl_","wl0_","wl1_","wl2_",
                                   "wl0.1_","wl0.2_","wl0.3_",
                                   "wl1.1_","wl1.2_","wl1.3_",
                                   "wl2.1_","wl2.2_","wl2.3_",
                                   NULL
                                 };
#endif /* ! BCA_HNDROUTER */

#ifdef BCA_HNDROUTER
#define SI_MAXCORES 16
#define WL_MAXBSSCFG	16	/* maximum number of BSS Configs we can configure */

static char *wl_prefix_check(char *name)
{
    char *prefix = g_temp_buf;
    int buf_size = sizeof(g_temp_buf);
    int i, j;

    /* check for wl_ */
    snprintf(prefix, buf_size, "wl_");
    if (!strncmp(name, prefix, strlen(prefix)))
        goto found;

    for (i = 0; i < SI_MAXCORES; i++) {
        /* check wl prefix for devpath%d */
        snprintf(prefix, buf_size, "%d:", i);
        if (!strncmp(name, prefix, strlen(prefix)))
            goto found;

        /* check for wlX_*/
        snprintf(prefix, buf_size, "wl%d_", i);
        if (!strncmp(name, prefix, strlen(prefix)))
            goto found;

        for (j = 0; j < WL_MAXBSSCFG; j++) {
            /* check for wlX.Y_*/
            snprintf(prefix, buf_size, "wl%d.%d_", i, j);
            if (!strncmp(name, prefix, strlen(prefix)))
            	goto found;
        }
    }

    /* not found */
    prefix[0] = '\0';

found:

    return prefix;
}
#endif /* BCA_HNDROUTER */

/*Check nvram variable and return itsmax  value length*/
static int var_maxlen(char *name,uint32 len)
{
    int idx =0,i=0;
    char short_name[64];
#ifdef BCA_HNDROUTER
    char *prefix;
#endif /* BCA_HNDROUTER */

    WLCSM_TRACE(WLCSM_TRACE_NVRAM,"Check_var name=[%s]\n", name );
    memset(short_name, 0, sizeof(short_name));

#ifdef BCA_HNDROUTER
    BCM_REFERENCE(i);
    prefix = wl_prefix_check(name);
    if (prefix[0] != '\0')
        strcpy(short_name, name + strlen(prefix));
    else
        strcpy(short_name, name);
#else /* ! BCA_HNDROUTER */
    for ( i=0; g_WL_MATCH_NAMES[i]; i++ ) {
        if(!strncmp(name,g_WL_MATCH_NAMES[i],strlen(g_WL_MATCH_NAMES[i]))) {
            strcpy(short_name, name+strlen(g_WL_MATCH_NAMES[i]));
            break;
        }
    }

    if(!g_WL_MATCH_NAMES[i]) {
        strcpy(short_name, name );
    }
#endif /* ! BCA_HNDROUTER */

    for ( idx=0; idx < VAR_LEN_COUNT && var_len_tab[idx].name[0] !='\0'; idx++ ) {
        if ( !strcmp( var_len_tab[idx].name, short_name) ) {
            WLCSM_TRACE(WLCSM_TRACE_NVRAM, "[%s] Max Len [%d]\n", name, var_len_tab[idx].max_len );
            return var_len_tab[idx].max_len;
        }
    }

    if(len>WL_DEFAULT_VALUE_SIZE_MAX)
        fprintf(stderr,"!!!!!!wl variable:%s's value size:%d is bigger than allowed size:%d!!! \r\n",
                name,len,WL_DEFAULT_VALUE_SIZE_MAX);
    return WL_DEFAULT_VALUE_SIZE_MAX;
}

static inline unsigned int _hash(char *s)
{
    unsigned int hash = 0;
    while (*s) {
        hash = 31 * hash + *s++;
    }
    return hash% ARRAYSIZE(wlcsm_local_nvram_hash);
}



static inline void _nvram_local_unset(struct nvram_tuple *tuple)
{

    tuple->value=NULL;

}

static struct nvram_tuple *_nvram_local_new(char *name, char *value)
{
    int len;
    struct nvram_tuple *t;
    int index=_hash(name);

    len=strlen(name) + 1 + var_maxlen(name,strlen(value?value:"")) + 1;

    if(value &&  strlen(value)>=len) {
        fprintf(stderr, "nvram/value pair: is invalid or bigger than its max length\r\n");
        return NULL;
    }

    if (!(t = _MALLOC_(sizeof(struct nvram_tuple) +len))) {
        WLCSM_TRACE(WLCSM_TRACE_NVRAM,"---:%s:%d  Relocated failed name:%s,value:%s\r\n",__FUNCTION__,__LINE__,name,value );
        return NULL;
    }

    memset( &t[1], 0, len );
    t->name = (char *) &t[1];
    strcpy(t->name, name);
    if(value) {
        t->value = t->name + strlen(name) + 1;
        strcpy(t->value, value);
    } else
        t->value=NULL;

    t->next=wlcsm_local_nvram_hash[index];
    wlcsm_local_nvram_hash[index]=t;

    WLCSM_TRACE(WLCSM_TRACE_NVRAM,"---:%s:%d  t pointer:%p \r\n",__FUNCTION__,__LINE__,t );
    return t;
}


static inline void  _nvram_change_notifciation(char *name, char *value,char *tuplevalue)
{

    if(g_WLCSM_EVENT_GENERIC_HOOK||g_WLCSM_EVENT_HOOKS[WLCSM_EVT_NVRAM_CHANGED]) {

        WLCSM_TRACE(WLCSM_TRACE_NVRAM,"----:%s:%d  notify the other process about nvram change \r\n",__FUNCTION__,__LINE__ );

        if(g_WLCSM_EVENT_GENERIC_HOOK)
            g_WLCSM_EVENT_GENERIC_HOOK(WLCSM_EVT_NVRAM_CHANGED,name,value,tuplevalue);

        if(g_WLCSM_EVENT_HOOKS[WLCSM_EVT_NVRAM_CHANGED])
            g_WLCSM_EVENT_HOOKS[WLCSM_EVT_NVRAM_CHANGED](name,value,tuplevalue);

    }
}

static int _nvram_local_update(struct nvram_tuple *tuple,char *name, char *value)
{

    int var_len=0;
    if(!value) {
        tuple->value=NULL;
        return WLCSM_SUCCESS;
    }
    var_len=var_maxlen(name,strlen(value));
    if(strlen(value)>var_len) {
        fprintf(stderr,"nvram/value pair: is bigger than its max length\r\n");
        value="";
        return WLCSM_GEN_ERR;
    }

    tuple->value = tuple->name + strlen(name) + 1;
    memset(tuple->value,0,var_len);
    strcpy(tuple->value,value);

    return WLCSM_SUCCESS;
}

#include <rtconfig.h>
#ifdef RTCONFIG_JFFS_NVRAM
#include <limits.h>
extern int f_exists(const char *file);
extern int d_exists(const char *path);
extern int f_read_string(const char *file, char *buffer, int max);
extern int jffs_nvram_getall(int len_nvram, char *buf, int count);

/*
	this custom_nvrm_list shouldn't set / commit into /data/.kernel_nvram.setting to avoid "\n"
	because the format of /data/.kernel_nvram.setting can't include "\n"
*/
static char *custom_nvram_list[] = {
	"vpn_server_custom",
	"vpn_client1_custom",
	"vpn_client2_custom",
	"vpn_client3_custom",
	"vpn_client4_custom",
	"vpn_client5_custom",
	NULL
};

/*
	check which nvram doesn't write back /data/.kernel_nvram.setting via wlcsm_nvram_commit()
*/
int custom_strstr_nvram(const char *name)
{
	int i;

	for (i = 0; custom_nvram_list[i] != NULL; i++) {
		if (strstr(name, custom_nvram_list[i]))
			return 1;
	}

	return 0;
}
#endif

int wlcsm_nvram_commit (void)
{

    int ret=WLCSM_GEN_ERR;
#if defined(NAND_SYS) && !defined(BRCM_CMS_BUILD)
    char  *name,*buf;
    FILE *fp;
#ifdef RTCONFIG_JFFS_NVRAM
    int len;
#endif
    buf=malloc(NVRAM_SPACE);
    if(!buf) {
        fprintf(stderr,"Could not allocate memory\n");
        return WLCSM_GEN_ERR;
    }
#ifdef RTCONFIG_JFFS_NVRAM
    len = wlcsm_nvram_getall(buf,NVRAM_SPACE);
    len = jffs_nvram_getall(len,buf,NVRAM_SPACE);
#else
    wlcsm_nvram_getall(buf,NVRAM_SPACE);
#endif

    fp=fopen(KERNEL_NVRAM_FILE_NAME,"w+");
    if(!fp) {
        fprintf(stderr,"%s:%d could not open nvram file  \r\n",__FUNCTION__,__LINE__ );
        free(buf);
        return WLCSM_GEN_ERR;
    }
    for (name = buf; *name; name += strlen(name) + 1) {
#ifdef RTCONFIG_JFFS_NVRAM
	/* strip custom_nvram_list */
	if (custom_strstr_nvram(name) == 1) {
		//printf("%s : name=%s\n", __FUNCTION__, name);
		continue;
	}
#endif
 
        fputs(name,fp);
        fputc('\n',fp);
    }
    fclose(fp);
    free(buf);
#endif
    if(wlcsm_netlink_send_mesg(WLCSM_MSG_NVRAM_COMMIT,NULL,0)== WLCSM_SUCCESS) {
        t_WLCSM_MSG_HDR *hdr=wlcsm_unicast_recv_mesg(g_temp_buf);
        if(hdr!=NULL && hdr->type==WLCSM_MSG_NVRAM_COMMIT) ret=WLCSM_SUCCESS;

    }
    return ret;
}

static size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t srclen, len;

	srclen = strlen(src);
	if (size <= 0)
		return srclen;

	len = (srclen < size) ? srclen : size - 1;
	memcpy(dst, src, len); /* should not overlap */
	dst[len] = '\0';

	return srclen;
}

static char *nvram_xfr_buf = NULL;

char *wlcsm_nvram_xfr(char *buf)
{
	size_t count = strlen(buf)*2 + 1;
	char tmpbuf[1024];
	char *value = NULL;

	if (count > sizeof(tmpbuf))
		return NULL;

	if (!nvram_xfr_buf)
		nvram_xfr_buf = (char *) malloc(1024 + 1);

	if (!nvram_xfr_buf)
		return NULL;

	strcpy(tmpbuf, buf);

	if (wlcsm_netlink_send_mesg(WLCSM_MSG_NVRAM_XFR, (char *) tmpbuf, strlen(tmpbuf) + 1) == WLCSM_SUCCESS) {
		t_WLCSM_MSG_HDR *hdr = wlcsm_unicast_recv_mesg(g_temp_buf);
		if (hdr != NULL && hdr->type == WLCSM_MSG_NVRAM_XFR) {
			if (hdr->len) {
				value = (void *) (hdr + 1);
				strlcpy(nvram_xfr_buf, value, hdr->len + 1);
				return nvram_xfr_buf;
			}
		}
	}

	return NULL;
}

#ifdef DUMP_PREV_OOPS_MSG
int wlcsm_dump_prev_oops(void)
{
	int ret = WLCSM_GEN_ERR;

	if (wlcsm_netlink_send_mesg(WLCSM_MSG_DUMP_PREV_OOPS, NULL, 0) == WLCSM_SUCCESS) {
		t_WLCSM_MSG_HDR *hdr = wlcsm_unicast_recv_mesg(g_temp_buf);
		if (hdr != NULL && hdr->type == WLCSM_MSG_DUMP_PREV_OOPS)
			ret = WLCSM_SUCCESS;
	}

	return ret;
}
#endif

int wlcsm_nvram_unset (char *name)
{
    int buflen=_get_valuepair_total_len(name,NULL,0);
    t_WLCSM_NAME_VALUEPAIR *buf;
    struct nvram_tuple *tuple;
    int index;

    _NVRAM_LOCK();
    _FIND_LOCAL_NVRAM(name,tuple,&index);
    if(tuple)  {
        index= _hash(tuple->name);
        wlcsm_local_nvram_hash[index]=tuple->next;
        free(tuple);
    }

    buf=wlcsm_get_namevalue_buf(name,NULL,0);
    if(buf!=NULL) {
        if(wlcsm_netlink_send_mesg(WLCSM_MSG_NVRAM_UNSET,(char *)buf,buflen)== WLCSM_SUCCESS) {
            t_WLCSM_MSG_HDR *hdr=wlcsm_unicast_recv_mesg(g_temp_buf);
            if(hdr!=NULL && hdr->type==WLCSM_MSG_NVRAM_UNSET) {
                WLCSM_TRACE(WLCSM_TRACE_NVRAM, "nvram unset successful\n");
                free(buf);
                _NVRAM_UNLOCK();
                return WLCSM_SUCCESS;
            }
        }
        free(buf);
    }
    _NVRAM_UNLOCK();
    return WLCSM_GEN_ERR;
}


void wlcsm_register_event_hook(t_WLCSM_EVENT_TYPE type,WLCSM_EVENT_HOOK_FUNC hook)
{
    wlcsm_init(m_AS_DAEMON);
    g_WLCSM_EVENT_HOOKS[type]=hook;
}


void wlcsm_register_event_generic_hook(WLCSM_EVENT_HOOK_GENERICFUNC hook)
{
    wlcsm_init(m_AS_DAEMON);
    g_WLCSM_EVENT_GENERIC_HOOK=hook;
}


void wlcsm_nvram_commit_update()
{
    if(g_WLCSM_EVENT_GENERIC_HOOK)
        g_WLCSM_EVENT_GENERIC_HOOK(WLCSM_EVT_NVRAM_COMMITTED);
    if(g_WLCSM_EVENT_HOOKS[WLCSM_EVT_NVRAM_COMMITTED])
        g_WLCSM_EVENT_HOOKS[WLCSM_EVT_NVRAM_COMMITTED](NULL,NULL,NULL);
}

int wlcsm_nvram_local_update(char *name, char *value)
{
    int ret=WLCSM_SUCCESS;
    struct nvram_tuple *tuple=NULL;
    int index=0;

    _NVRAM_LOCK();
    _FIND_LOCAL_NVRAM(name,tuple,&index);
    WLCSM_TRACE(WLCSM_TRACE_DBG,"name:%s,value:%s\r\n",name ,value?value:"NULL");

    if(tuple) {

        WLCSM_TRACE(WLCSM_TRACE_NVRAM," found local tuple:name:%s,value:%s,invalue:%s \r\n",tuple->name, tuple->value?tuple->value:"NULL" ,value?value:"NULL");
        if(_STR_VALUE_EQUAL(value,tuple->value)) {
            _NVRAM_UNLOCK();
            return ret;
        } else {
            int len=0;
            char *tuplevalue=NULL;
            if(tuple->value) {
                len=strlen(tuple->value)+1;
                tuplevalue=(char *)_MALLOC_(len);
                strcpy(tuplevalue,tuple->value);
            }
            ret=_nvram_local_update(tuple,name,value);
            _NVRAM_UNLOCK();
            _nvram_change_notifciation(name,value,tuplevalue);
            if(tuplevalue)
                _MFREE_(tuplevalue);
        }
    } else
        _NVRAM_UNLOCK();
    return ret;
}


int wlcsm_nvram_set(char *name, char*value)
{

    struct nvram_tuple *tuple=NULL;
    int index=0,  buflen;
    t_WLCSM_NAME_VALUEPAIR *buf;
    /* nvram set NULL is the same as unset */
    if(!value) return wlcsm_nvram_unset(name);
    _NVRAM_LOCK();
    _FIND_LOCAL_NVRAM(name,tuple,&index);
    if(tuple) {
        WLCSM_TRACE(WLCSM_TRACE_NVRAM," found local tuple:name:%s,value:%s,invalue:%s \r\n",tuple->name, tuple->value ,value);
        if(_STR_VALUE_EQUAL(value,tuple->value)) {
            if(g_WLCSM_IS_DAEMON==m_AS_DAEMON) {
                _NVRAM_UNLOCK();
                return WLCSM_SUCCESS;
            }
        } else if(_nvram_local_update(tuple,name,value)==WLCSM_GEN_ERR) {
            _NVRAM_UNLOCK();
            return WLCSM_GEN_ERR;
        }
    } else if(_nvram_local_new(name,value)==NULL) {
        _NVRAM_UNLOCK();
        return WLCSM_GEN_ERR;
    }

    buflen=_get_valuepair_total_len(name,value,0);
    buf=wlcsm_get_namevalue_buf(name,value,0);
    if(buf!=NULL) {
        if(wlcsm_netlink_send_mesg(WLCSM_MSG_NVRAM_SET,(char *)buf,buflen)== WLCSM_SUCCESS) {
            t_WLCSM_MSG_HDR *hdr=wlcsm_unicast_recv_mesg(g_temp_buf);
            if(hdr!=NULL && hdr->type==WLCSM_MSG_NVRAM_SET) {
                WLCSM_TRACE(WLCSM_TRACE_NVRAM, "nvram set successful\n");
                free(buf);
                _NVRAM_UNLOCK();
                return WLCSM_SUCCESS;
            }
        }
        free(buf);
    }
    _NVRAM_UNLOCK();
    WLCSM_TRACE(WLCSM_TRACE_NVRAM, "nvram set failed\n");
    return WLCSM_GEN_ERR;
}



char *wlcsm_nvram_get (char *name )
{
    struct nvram_tuple *tuple=NULL;
    int index=0;
    char *value=NULL;

    _NVRAM_LOCK();
    _FIND_LOCAL_NVRAM(name,tuple,&index);

    /* when there is no update daemon or not find in local, go to kernel for the value */

    if((g_WLCSM_IS_DAEMON==m_NOTAS_DAEMON || tuple==NULL) &&
       (wlcsm_netlink_send_mesg(WLCSM_MSG_NVRAM_GET,(char *)name,strlen(name)+1)== WLCSM_SUCCESS)) {

        t_WLCSM_MSG_HDR *hdr=wlcsm_unicast_recv_mesg(g_temp_buf);

        if(hdr!=NULL && hdr->type==WLCSM_MSG_NVRAM_GET ) {

            if(hdr->len) value= VALUEPAIR_VALUE((t_WLCSM_NAME_VALUEPAIR *)(hdr+1));

            if(tuple)
                _nvram_local_update(tuple,name,value);
            else if(value)
                tuple=_nvram_local_new(name,value);
        }
    }
    _NVRAM_UNLOCK();

    if(tuple)	return tuple->value;
    else	return NULL;
}



int wlcsm_nvram_getall(char *buf, int count)
{
    int ret=WLCSM_GEN_ERR;
    t_WLCSM_MSG_HDR *hdr;
    char lenstr[8];
    char *start = buf;
    int len=0;

    memset(buf,'\0',count);
    sprintf(lenstr,"%d",count);
    _NVRAM_LOCK();
    if(wlcsm_netlink_send_mesg(WLCSM_MSG_NVRAM_GETALL,lenstr,strlen(lenstr)+1)== WLCSM_SUCCESS) {
        while(1) {

            hdr=wlcsm_unicast_recv_mesg(g_temp_buf);

            if(!hdr || hdr->len==0) {
                WLCSM_TRACE(WLCSM_TRACE_NVRAM,"----:%s:%d Did not recevie anything?????  \r\n",__FUNCTION__,__LINE__ );
                break;

            }

            if((hdr->type==WLCSM_MSG_NVRAM_GETALL)||hdr->type==WLCSM_MSG_NVRAM_GETALL_DONE) {

                if((count-len)>hdr->len) {
                    memcpy(buf,(char *)(hdr+1),hdr->len);
                    buf+=hdr->len;

                } else  {
                    ret=WLCSM_SUCCESS;
                    break;
                }

                if(hdr->type==WLCSM_MSG_NVRAM_GETALL_DONE) {
                    ret=WLCSM_SUCCESS;
                    break;
                }
            }
        }
    }
    _NVRAM_UNLOCK();
    return (ret!=WLCSM_SUCCESS)?ret:buf-start;
}
void wlcsm_nvram_release_all(void)
{
    uint i;
    struct nvram_tuple *t, *next;
    /* Free hash table */
    _NVRAM_LOCK();
    for (i = 0; i < ARRAYSIZE(wlcsm_local_nvram_hash); i++) {
        for (t = wlcsm_local_nvram_hash[i]; t; t = next) {
            next = t->next;
            free(t);
        }
        wlcsm_local_nvram_hash[i] = NULL;
    }
    _NVRAM_UNLOCK();
}

#ifdef BCA_HNDROUTER
char *wlcsm_prefix_match(char *name)
{
    char *prefix;

    prefix = wl_prefix_check(name);
    if (prefix[0] != '\0')
        return prefix;
    else
        return NULL;
}
#else /* ! BCA_HNDROUTER */
char *wlcsm_prefix_match(char *name)
{
    int i=0;
    for ( i=0; g_WL_MATCH_NAMES[i]; i++ ) {
        if(!strncmp(name,g_WL_MATCH_NAMES[i],strlen(g_WL_MATCH_NAMES[i]))) {
            break;
        }
    }
    return g_WL_MATCH_NAMES[i];
}
#endif /* ! BCA_HNDROUTER */

char *wlcsm_trim_str(char *value)
{
    char *in_str=value;
    if(!in_str ||strlen(in_str)==0) return in_str;
    else {
        int len=strlen(in_str);
        len--;
        while(len>=0 &&
              (in_str[len]==' '||
               in_str[len]=='"' ||
               in_str[len]=='\n'))
            len--;
        in_str[len+1]='\0';
        /*  trim leading space or "*/
        while(*in_str==' '|| *in_str=='"') in_str++;
        return in_str;
    }
}

char *wlcsm_nvram_name_parser(char *name,unsigned int *idx, unsigned int *ssid_idx)
{

    if(name) {
        char *nvram_name=strstr(name,"_");
        if(nvram_name) {
            int ret=sscanf(name,"wl%d.%d",idx,ssid_idx);
            if(ret) {
                if(ret==1) *ssid_idx=0;
                return (char *)(nvram_name+1);
            }
        }
    }
    *idx=0;
    *ssid_idx=0;
    return NULL;
}

#ifdef WLCSM_DEBUG
void wlcsm_set_trace (char *procname)
{
    char buf[32], *tracelvl;
    snprintf (buf, 32, "%s_trace", procname);
    tracelvl = wlcsm_nvram_get (buf);
    snprintf (g_WLCSM_TRACE_PROC, 32, "%s", procname);
    if (tracelvl) {
        sscanf (tracelvl, "%d", &g_WLCSM_TRACE_LEVEL);
        return;
    } else {
        g_WLCSM_TRACE_LEVEL = 0;
    }
}

void wlcsm_print(const char *fmt, ...)
{
    char msg[MAX_NLRCV_BUF_SIZE]= {'\0'};
    char recv_hdr[sizeof(t_WLCSM_MSG_HDR)+4];
    va_list args;
    int n=0;
    va_start(args,fmt);
    n=vsnprintf(msg,MAX_NLRCV_BUF_SIZE,fmt,args);
    va_end(args);
    if(n>0 && n<MAX_NLRCV_BUF_SIZE && wlcsm_netlink_send_mesg(WLCSM_MSG_DEBUG_LOGMESSAGE,msg,n+1)== WLCSM_SUCCESS) {
        t_WLCSM_MSG_HDR *hdr=wlcsm_unicast_recv_mesg(recv_hdr);
        if(hdr && hdr->type==WLCSM_MSG_DEBUG_LOGMESSAGE && (hdr->len>1))
            return;
        else
            fprintf(stderr,msg);
    }
}

/* default for userspace, otherwize put k ahead, example
 * k+err
 * k-err
 * kclr
 * no+- is to set */
int wlcsm_set_trace_level(char *tl)
{
    char setuser=0;
    char addaction=0;
    if(tl[0]=='+'||tl[0]=='-'||tl[0]=='u') setuser=1;
    if(tl[0]=='u'||tl[0]=='k') tl++;
    if(tl[0]=='+') {
        addaction=2;
        tl++;
    } else if(tl[0]=='-') {
        addaction=1;
        tl++;
    }
    unsigned int i=0;
    unsigned int tracelevel=0;
    for(; i<sizeof(trace_matches)/sizeof(t_WLCSM_TRACE_MATCH); i++) {

        if (!strcmp(trace_matches[i].name,tl)) {
            tracelevel= trace_matches[i].level;
            break;
        }
    }
    if(setuser) {
        if(addaction==2)  g_WLCSM_TRACE_LEVEL|=tracelevel;
        else if(addaction==1)
            g_WLCSM_TRACE_LEVEL &= (~tracelevel);
        else
            g_WLCSM_TRACE_LEVEL=tracelevel;
    } else {
        tracelevel|= (addaction<<30);
        if(wlcsm_netlink_send_mesg(WLCSM_MSG_NVRAM_SETTRACE,(char *)&tracelevel,sizeof(unsigned int))== WLCSM_SUCCESS) {
            t_WLCSM_MSG_HDR *hdr=wlcsm_unicast_recv_mesg(g_temp_buf);
            if(hdr!=NULL && hdr->type==WLCSM_MSG_NVRAM_SETTRACE)
                WLCSM_TRACE(WLCSM_TRACE_NVRAM, "trace set successful\n");
            return 0;
        }
    }
    return -1;
}

/*simple API to evaluate elapsed time beteen instructions */
struct timeval g_TIME_START= {0},g_TIME_CUR= {0},g_TIME_PRE= {0};

int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1)
{
    long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;

    return (diff<0);
}

void wlcsm_restart_time(char *func,int line) {
    gettimeofday(&g_TIME_START,NULL);
    wlcsm_print("%s#%d,reset time\n",func,line);
    g_TIME_PRE.tv_sec=g_TIME_START.tv_sec;
    g_TIME_PRE.tv_usec=g_TIME_START.tv_usec;
}


void wlcsm_delta_time(char *func,int line,char *prompt) {
    struct timeval delta;
    gettimeofday(&g_TIME_CUR,NULL);
    timeval_subtract(&delta,&g_TIME_CUR,&g_TIME_PRE);
    g_TIME_PRE.tv_sec=g_TIME_CUR.tv_sec;
    g_TIME_PRE.tv_usec=g_TIME_CUR.tv_usec;
    if(prompt)
        wlcsm_print("%s:%d: %s: delta:	%lu#%08lu(s#us)\n",func,line,
                    prompt,
                    delta.tv_sec,
                    delta.tv_usec);
}

void wlcsm_total_time(char *func,int line) {
    struct timeval delta;
    gettimeofday(&g_TIME_CUR,NULL);
    timeval_subtract(&delta,&g_TIME_CUR,&g_TIME_START);
    wlcsm_print( "%s:%d:total:	%lu#%08lu(s#us)\n",func,line,
                 delta.tv_sec,
                 delta.tv_usec);
    g_TIME_PRE.tv_sec=g_TIME_CUR.tv_sec;
    g_TIME_PRE.tv_usec=g_TIME_CUR.tv_usec;
}

#endif
