/*
 * Copyright 2014 Trend Micro Incorporated
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software without 
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>

#include <linux/bitmap.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <net/dst.h>

#include "udb/tdts_udb_shell.h"

#define CONFIG_DEBUG (0) //!< Say 1 to debug

#if CONFIG_DEBUG
#define DBG(fmt, args...) printk(KERN_DEBUG "[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#else
#define DBG(fmt, args...) do { } while (0)
#endif

/* ap mode = 0 -> nat mode, ap mode = 1 -> bridge mode , ap mode =2 -> repeate mode*/
unsigned int mode = 0;
module_param(mode, uint, S_IRUGO);
EXPORT_SYMBOL(mode); // export for forwarding module

char *dev_wan = NULL;
module_param(dev_wan, charp, S_IRUGO);
EXPORT_SYMBOL(dev_wan); // export for forwarding module

char *dev_lan = "br0";
module_param(dev_lan, charp, S_IRUGO);
EXPORT_SYMBOL(dev_lan); // export for forwarding module

char *gw_mac = NULL;
module_param(gw_mac, charp, S_IRUGO);

unsigned short port_scan_tholds[6] = {50,50,50,50,50,50};
module_param_array(port_scan_tholds, ushort, NULL, S_IRUGO);

static unsigned int user_timeout = 0;
module_param(user_timeout, uint, S_IRUGO);
static unsigned int app_timeout = 0;
module_param(app_timeout, uint, S_IRUGO);
static unsigned int app_idle_time = 0;
module_param(app_idle_time, uint, S_IRUGO);

#if TMCFG_E_UDB_CORE_RULE_FORMAT_V2
extern int tdts_shell_set_binding_version(unsigned int magic, unsigned int version);
#endif

int udb_shell_udb_init(void)
{
	char tmpH = 0;
	char tmpL = 0;
	size_t count = 0;

#if TMCFG_E_UDB_CORE
	udb_init_param_t udb_init_param;

	udb_init_param.wan_dev = dev_wan;
	udb_init_param.lan_dev = dev_lan;
	udb_init_param.mode = mode;
	udb_init_param.user_timeout = user_timeout;
	udb_init_param.app_timeout = app_timeout;
	udb_init_param.app_idle_time = app_idle_time;
	udb_init_param.dpi_l3_scan = (dpi_l3_scan_t)tdts_shell_dpi_l3_skb;
#if TMCFG_E_UDB_CORE_RULE_FORMAT_V2
	udb_init_param.dpi_set_binding_ver = (dpi_set_binding_ver_t)tdts_shell_set_binding_version;
#endif
#if TMCFG_E_UDB_CORE_ANOMALY_PREVENT && TMCFG_E_CORE_PORT_SCAN_DETECTION
#if TMCFG_E_UDB_CORE_RULE_FORMAT_V2
	udb_init_param.dpi_remove_tcp_connection_by_tuple = (dpi_remove_tcp_connection_by_tuple_t)tdts_shell_remove_tcp_connection_by_tuple;
	udb_init_param.dpi_remove_udp_connection_by_tuple = (dpi_remove_udp_connection_by_tuple_t)tdts_shell_remove_udp_connection_by_tuple;
#endif
	udb_init_param.port_scan_tholds = port_scan_tholds;
#endif
#endif // TMCFG_E_UDB_CORE
	
	if (!dev_wan)
	{
		printk(KERN_EMERG "Please specify module parameter \"dev_wan\"...");
		return -EINVAL;
	}

	if (gw_mac && strlen(gw_mac)==(ETH_ALEN*2))
	{
		for(count = 0; count <ETH_ALEN; count++) 
		{
			tmpH = gw_mac[(count<<1)];
			tmpL = gw_mac[(count<<1) + 1];			
			tmpH = !(tmpH >= '0' && tmpH <= '9') ? !(tmpH >= 'A' && tmpH <= 'F') ? !(tmpH >= 'a' && tmpH <= 'f') ? tmpH : tmpH - 'a' + 10 : tmpH - 'A' + 10 : tmpH - '0';
			tmpL = !(tmpL >= '0' && tmpL <= '9') ? !(tmpL >= 'A' && tmpL <= 'F') ? !(tmpL >= 'a' && tmpL <= 'f') ? tmpL : tmpL - 'a' + 10 : tmpL - 'A' + 10 : tmpL - '0';
			
			gw_mac[count] = (tmpH<<4) + tmpL;			
		}		
	}

#if TMCFG_E_UDB_CORE
	return udb_core_udb_init(&udb_init_param);
#else
	return 0;
#endif
}

void udb_shell_udb_exit(void)
{
#if TMCFG_E_UDB_CORE
	udb_core_udb_exit();
#endif
}

int udb_shell_memtrack_init(void)
{
#if TMCFG_E_UDB_CORE_MEMTRACK
	return udb_core_memtrack_init();
#else
	return 0;
#endif
}

void udb_shell_memtrack_exit(void)
{
#if TMCFG_E_UDB_CORE_MEMTRACK
	udb_core_memtrack_exit();
#endif
}

int udb_shell_wan_detection(uint8_t *dev_name, uint32_t len)
{
#if TMCFG_E_UDB_CORE_TMDBG
	return udb_core_wan_detection(dev_name, len);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(udb_shell_wan_detection);

////////////////////////////////////////////////////////////////////////////////
tdts_res_t udb_shell_do_fastpath_action(tdts_udb_param_t *fw_param)
{
#if TMCFG_E_UDB_CORE
	return udb_core_do_fastpath_action(fw_param);
#else
	return TDTS_RES_ACCEPT;
#endif
}
EXPORT_SYMBOL(udb_shell_do_fastpath_action);

int udb_shell_usr_msg_handler(uint8_t *msg, int size, int pid, int type)
{
#if TMCFG_E_UDB_CORE
	return tdts_core_fw_usr_msg_handler(msg, size, pid, type);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(udb_shell_usr_msg_handler);

int udb_shell_update_devid_un_http_ua(uint8_t uid, uint8_t ip_ver, uint8_t *data)
{
#if TMCFG_E_UDB_CORE
	return tdts_core_update_devid_un_http_ua(uid, ip_ver, data);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(udb_shell_update_devid_un_http_ua);

int udb_shell_update_devid_un_bootp(uint8_t uid, uint8_t ip_ver, uint8_t *data)
{
#if TMCFG_E_UDB_CORE
	return tdts_core_update_devid_un_bootp(uid, ip_ver, data);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(udb_shell_update_devid_un_bootp);

#if 0 // TMCFG_E_UDB_CORE_DC_DNS_REPLY
int udb_shell_update_dns_reply(uint8_t uid, uint8_t ip_ver, uint8_t *data)
{
#if TMCFG_E_UDB_CORE
	return tdts_core_update_dns_reply(uid, ip_ver, data);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(udb_shell_update_dns_reply);
#endif

int udb_shell_update_upnp_data(uint8_t *data, uint32_t index, void *cb)
{
#if TMCFG_E_UDB_CORE
	return tdts_core_update_upnp(data, index, cb);
#else
	return -1;
#endif
}
EXPORT_SYMBOL(udb_shell_update_upnp_data);

tdts_act_t udb_shell_get_action(tdts_udb_param_t *fw_param)
{
#if TMCFG_E_UDB_CORE
	return udb_core_get_action(fw_param, 0);
#else
	return TDTS_ACT_SCAN;
#endif
}
EXPORT_SYMBOL(udb_shell_get_action);

tdts_res_t udb_shell_policy_match(tdts_udb_param_t *fw_param)
{
#if TMCFG_E_UDB_CORE
	return udb_core_policy_match(fw_param);
#else
	return TDTS_RES_ACCEPT;
#endif
}
EXPORT_SYMBOL (udb_shell_policy_match);

