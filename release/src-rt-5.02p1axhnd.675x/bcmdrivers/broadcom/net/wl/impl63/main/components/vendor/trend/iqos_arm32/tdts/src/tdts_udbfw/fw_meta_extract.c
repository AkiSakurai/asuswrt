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

#include <linux/kernel.h>
#include <linux/version.h>
#include <net/ipv6.h>
#include <net/route.h>
#include <linux/skbuff.h>

#ifndef CONFIG_NETFILTER
#warning "CONFIG_NETFILTER is not defined!!!"
#endif

#include <linux/netfilter_ipv6.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_nat_helper.h>
#ifdef CONFIG_COMPAT
#include <asm/compat.h>
#endif

/* Mapping Kernel 2.6 Netfilter API to Kernel 2.4 */
#define ip_conntrack        nf_conn
#define ip_conntrack_get    nf_ct_get
#define ip_conntrack_tuple  nf_conntrack_tuple

#else // older kernel
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_conntrack_tuple.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#endif

#include "skb_access.h"
#include "forward_config.h"
#include "fw_meta_extract.h"
#include "fw_util.h"

#if TMCFG_E_UDB_CORE_SHN_QUERY
#include "fw_shn_agent.h"
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
/* Mapping Kernel 2.6 Netfilter API to Kernel 2.4 */
#define ip_conntrack        nf_conn
#define ip_conntrack_get    nf_ct_get
#define ip_conntrack_tuple  nf_conntrack_tuple
#endif

#if 0 // TODO: TMCFG_E_UDB_CORE_IQOS_SUPPORT
extern unsigned int qos_dbg_level;

#define META_DBG(fmt, args...) \
do \
{ \
	if (qos_dbg_level >= QOS_DBG_LEVEL_DBG) \
	{ \
		printk("[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args); \
	} \
} while (0)

#else
#define META_DBG(fmt, args...) do { } while (0)
#endif

extern int set_skb_tuples(struct sk_buff *skb, tdts_udb_param_t *fw_param);

int bndwth_report(char *data, unsigned int len, tdts_pkt_parameter_t *mt)
{
#if (CONFIG_USE_CONNTRACK_MARK && TMCFG_E_UDB_CORE_IQOS_SUPPORT)
	tdts_udb_param_t fw_param;
	tdts_meta_bndwth_info_t *info = (tdts_meta_bndwth_info_t *)data;

	META_DBG("%s, request %u KBps",	info->description, info->bndwth);

	memset(&fw_param, 0, sizeof(tdts_udb_param_t));

	if (likely((mt) && (mt->private_ptr)))
	{
		struct sk_buff *skb = NULL;
		pvt_param *pvt = (pvt_param *)mt->private_ptr;

		if (likely(skb = pvt->skb))
		{
			struct ip_conntrack *ct = NULL;
			enum ip_conntrack_info ctinfo;
			bool is_upload = is_skb_upload(skb, NULL);

			SET_SKB_UPLOAD(fw_param.skb_flag, is_upload);

			if (!(ct = ip_conntrack_get(skb, &ctinfo)))
			{
				ERR("Failed to get conntrack from skb!\n");
				return -1;
			}

			fw_param.ct_ptr = (void *)ct;
			fw_param.ct_mark = &ct->mark;

			set_skb_tuples(skb, &fw_param);

			if (is_upload)
			{
				fw_param.skb_eth_src = SKB_ETH_SRC(skb);
			}

			udb_shell_update_qos_data(mt, NULL, info, &fw_param);
		}
		else
		{
			ERR("No skb!\n");
		}
	}
#endif
	return 0;
}

int paid_conn_report(
	char *data, unsigned int len, tdts_pkt_parameter_t *mt)
{
#if (CONFIG_USE_CONNTRACK_MARK && TMCFG_E_UDB_CORE_IQOS_SUPPORT)
	tdts_udb_param_t fw_param;
	tdts_meta_paid_info_t *info = (tdts_meta_paid_info_t *)data;

	META_DBG("Paid conn = %d\n", info->paid);

	memset(&fw_param, 0, sizeof(tdts_udb_param_t));

	if (likely((mt) && (mt->private_ptr)))
	{
		struct sk_buff *skb = NULL;
		pvt_param *pvt = (pvt_param *)mt->private_ptr;

		if (likely(skb = pvt->skb))
		{
			struct ip_conntrack *ct = NULL;
			enum ip_conntrack_info ctinfo;
			bool is_upload = is_skb_upload(skb, NULL);

			SET_SKB_UPLOAD(fw_param.skb_flag, is_upload);

			if (!(ct = ip_conntrack_get(skb, &ctinfo)))
			{
				ERR("Failed to get conntrack from skb!\n");
				return -1;
			}

			fw_param.ct_ptr = (void *)ct;
			fw_param.ct_mark = &ct->mark;

			set_skb_tuples(skb, &fw_param);

			if (is_upload)
			{
				fw_param.skb_eth_src = SKB_ETH_SRC(skb);
			}

			udb_shell_update_qos_data(mt, info, NULL, &fw_param);
		}
		else
		{
			ERR("No skb!\n");
		}
	}
#endif
	return 0;
}

#if 0 // TMCFG_E_UDB_CORE_DC_DNS_REPLY
int dns_reply_report(char *data, unsigned int len, tdts_pkt_parameter_t *param)
{
	unsigned char ip_ver = 0;

	if (likely((param) && (param->private_ptr) &&( param->hook != TDTS_HOOK_NF_LOUT)))
	{
		uint8_t uid = 0;
		pvt_param *pvt = (pvt_param *)param->private_ptr;

		if (likely(uid = pvt->uid))
		{
			udb_shell_update_dns_reply(uid, ip_ver, (uint8_t *)data);
		}
		else
		{
			//ERR("No skb!\n");
			return -1;
		}

		return 0;
	}

	return -1;
}
#endif // TMCFG_E_UDB_CORE_DC_DNS_REPLY

#if TMCFG_E_UDB_CORE_DC_UNKNOWN_DEVID
int devid_un_report(char *data, unsigned int len, tdts_pkt_parameter_t *param)
{
	unsigned char ip_ver = 0;
	tdts_meta_devid_un_t *info = (tdts_meta_devid_un_t *)data;
	
	if (likely((param)) && (param->private_ptr))
	{
		uint8_t uid = 0;
		pvt_param *pvt = (pvt_param *)param->private_ptr;

		if (likely(uid = pvt->uid))
		{
			if (TDTS_META_DEVID_UN_TYPE_HTTP_UA == info->type)
			{
				udb_shell_update_devid_un_http_ua(
					uid, ip_ver, (uint8_t *)&info->ua);
			}
			else if (TDTS_META_DEVID_UN_TYPE_BOOTP == info->type)
			{
				udb_shell_update_devid_un_bootp(
					uid, ip_ver, (uint8_t *)&info->bootp);
			}
		}
		else
		{
			//ERR("No skb!\n");
			return -1;
		}

		return 0;
	}

	return -1;
}
#endif // TMCFG_E_UDB_CORE_DC_UNKNOWN_DEVID

#if TMCFG_E_UDB_CORE_SHN_QUERY
int upnp_report(char *data, unsigned int len, tdts_pkt_parameter_t *param)
{
	int ret = -1;
	pvt_param *pvt = NULL;
	void *cb = get_shnagent_cb();

	if ((!param) || (!data))
	{
		return ret;
	}

	pvt = (pvt_param *)param->private_ptr;
	if (likely(pvt))
	{
		DBG("udb_shell_update_upnp_data uid = %d pvt = %p\n", pvt->uid, pvt);
		udb_shell_update_upnp_data(data, pvt->uid, cb);
		ret = 0;
	}
	return ret;
}
#endif

#if TMCFG_E_UDB_CORE_HTTP_REFER
int url_report(char *data, unsigned int len, tdts_pkt_parameter_t *param)
{
	tdts_meta_url_info_t *info = (tdts_meta_url_info_t *)data;

	DBG("Domain = %.*s\n", info->domain_len, info->domain);
	DBG("Path = %.*s\n", info->path_len, info->path);
	DBG("Referer = %.*s\n", info->referer_len, info->referer);

	return 0;
}
#endif

int mt_init(void)
{
#if TMCFG_E_UDB_CORE_IQOS_SUPPORT
	if (tdts_shell_dpi_register_mt(bndwth_report, TDTS_META_TYPE_BNDWTH) < 0)
	{
		return -1;
	}

	if (tdts_shell_dpi_register_mt(paid_conn_report, TDTS_META_TYPE_PAID_CONN) < 0)
	{
		return -2;
	}
#endif

#if TMCFG_E_UDB_CORE_DC_UNKNOWN_DEVID
	if (tdts_shell_dpi_register_mt(devid_un_report, TDTS_META_TYPE_DEVID_UN) < 0)
	{
		return -3;
	}
#endif

#if 0 // TMCFG_E_UDB_CORE_DC_DNS_REPLY
	if (tdts_shell_dpi_register_mt(dns_reply_report, TDTS_META_TYPE_DNS_REPLY) < 0)
	{
		return -4;
	}
#endif

#if TMCFG_E_UDB_CORE_SHN_QUERY
	if (tdts_shell_dpi_register_mt(upnp_report, TDTS_META_TYPE_UPNP) < 0)
	{
		return -5;
	}
#endif

#if TMCFG_E_UDB_CORE_HTTP_REFER
	if (tdts_shell_dpi_register_mt(url_report, TDTS_META_TYPE_URL) < 0)
	{
		return -6;
	}
#endif
	return 0;
}

void mt_exit(void)
{
#if TMCFG_E_UDB_CORE_IQOS_SUPPORT
	tdts_shell_dpi_unregister_mt(bndwth_report, TDTS_META_TYPE_BNDWTH);
	tdts_shell_dpi_unregister_mt(paid_conn_report, TDTS_META_TYPE_PAID_CONN);
#endif
#if TMCFG_E_UDB_CORE_DC_UNKNOWN_DEVID
	tdts_shell_dpi_unregister_mt(devid_un_report, TDTS_META_TYPE_DEVID_UN);
#endif
#if TMCFG_E_UDB_CORE_SHN_QUERY
	tdts_shell_dpi_unregister_mt(upnp_report, TDTS_META_TYPE_UPNP);
#endif
#if TMCFG_E_UDB_CORE_HTTP_REFER
	tdts_shell_dpi_unregister_mt(url_report, TDTS_META_TYPE_URL);
#endif
}
