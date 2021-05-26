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

#include <linux/version.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/skbuff.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#include <net/netfilter/nf_conntrack.h>
#else
#include <linux/netfilter_ipv4/ip_conntrack.h>
#endif

#include "tdts_udb.h"
#include "forward_config.h"
#include "fw_fast_path.h"
#include "fw_internal.h"
#include "skb_access.h"

#include <linux/blog.h>
#include <linux/netdevice.h>
#include <linux/nbuff.h>
#include <fcache.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
/* Mapping Kernel 2.6 Netfilter API to Kernel 2.4 */
#define ip_conntrack        nf_conn
#define ip_conntrack_get    nf_ct_get
#define ip_conntrack_tuple  nf_conntrack_tuple
#endif

extern int br_fwdcb_register(fwdcb_t fwdcb);
extern int enet_fwdcb_register(fwdcb_t fwdcb);
extern void ip_conntrack_fc_resume(struct sk_buff *skb, struct nf_conn *ct, u_int8_t isv4);

static struct nf_hook_ops hook_ops_preroute, hook_ops_preroute6;
DEFINE_HOOK(hookfn_preroute_fc);

static int __register_hook(
	struct nf_hook_ops *ops, unsigned int hooknum, u_int8_t pf, int prio, nf_hookfn *cb)
{
	ops->hook = cb;
	ops->owner = THIS_MODULE;
	ops->pf = pf;
	ops->hooknum = hooknum;
	ops->priority = prio;

	return nf_register_hook(ops) ? -1 : 0;
}

int fc_hook_cb(void *pNBuff, struct net_device *dev)
{
#define IS_TX_DEV_WAN(dev) (dev->priv_flags & IFF_WANDEV)

	struct ip_conntrack *conntrack;
	uint32_t ctmark = 0;
	uint32_t pktlen = 0;
	uint32_t *skb_mark = NULL;
	tdts_res_t ret = TDTS_RES_ACCEPT;
	tdts_udb_param_t fw_param;

	if (NULL == pNBuff)
	{
		printk("pNBuff NULL\n");
		return PKT_DONE;
	}

	CNT_INC(fastpath);
	memset(&fw_param, 0, sizeof(tdts_udb_param_t));
	if(IS_FKBUFF_PTR(pNBuff))
	{
		FkBuff_t *pfkb = PNBUFF_2_FKBUFF(pNBuff);
		if (pfkb->mark) {
			B_IQOS_RESTORE_SKBMARK(pfkb, conntrack);
			ctmark = conntrack->mark;
			pktlen = pfkb->len;
			skb_mark = &pfkb->mark; // TODO: FIXME! pfkb->mark is unsigned long!! --mit@20170303
		}
	}
	else
	{
		struct sk_buff *pskb = PNBUFF_2_SKBUFF(pNBuff);
		if (pskb->mark) {
			B_IQOS_RESTORE_SKBMARK(pskb, conntrack);
			ctmark = conntrack->mark;
			pktlen = pskb->len;
			skb_mark = &pskb->mark;
		}
	}

	SET_SKB_UPLOAD(fw_param.skb_flag, IS_TX_DEV_WAN(dev));
	fw_param.skb_len = pktlen;
	fw_param.ct_mark = &ctmark;
	fw_param.skb_mark = skb_mark;
	fw_param.hook = TDTS_HOOK_FAST_PATH;

	ret = udb_shell_do_fastpath_action(&fw_param);

	switch (ret)
	{
	case TDTS_RES_DROP:
		return PKT_DROP;
	case TDTS_RES_CONTINUE:
	case TDTS_RES_ACCEPT:
	default:
		return PKT_DONE;
	}
}

int fast_path_hook_init(void)
{
	blog_lock();
	fc_flush();
	blog_unlock();
	PRT("flush fc\n");

	BROADSTREAM_IQOS_SET_ENABLE(1);
	br_fwdcb_register(fc_hook_cb);
	enet_fwdcb_register(fc_hook_cb);

	if (__register_hook(&hook_ops_preroute
		, NF_INET_POST_ROUTING, PF_INET, NF_IP_PRI_MANGLE
		, forward_hookfn_preroute_fc) < 0)
	{
		ERR("Cannot register hook_ops_preroute!\n");
		return -1;
	}
	DBG("hook_ops_preroute hooked\n");

	if (__register_hook(&hook_ops_preroute6
		, NF_INET_POST_ROUTING, PF_INET6, NF_IP_PRI_MANGLE
		, forward_hookfn_preroute_fc) < 0)
	{
		ERR("Cannot register hook_ops_preroute6!\n");
		return -1;
	}
	DBG("hook_ops_preroute6 hooked\n");

	return 0;
}

void fast_path_hook_exit(void)
{
	BROADSTREAM_IQOS_SET_ENABLE(0);
	br_fwdcb_register(NULL);
	enet_fwdcb_register(NULL);

	nf_unregister_hook(&hook_ops_preroute);
	DBG("hook_ops_preroute unhooked\n");

	nf_unregister_hook(&hook_ops_preroute6);
	DBG("hook_ops_preroute6 unhooked\n");
}

/*
 * For FC (Flow-Cache) integration ?
 */
static uint32_t hookfn_preroute_fc(
	uint32_t hooknum,
	struct sock *sk,
	struct sk_buff *skb,
	const struct net_device *in_dev,
	const struct net_device *out_dev,
	void* okfn)
{
	uint32_t verdict = NF_ACCEPT;
	tdts_udb_param_t fw_param;
	tdts_act_t act;

#if CONFIG_USE_CONNTRACK_MARK
	struct ip_conntrack *conntrack;
	enum ip_conntrack_info ctinfo;
#endif
	HOOK_PROLOG(verdict);
	if (unlikely(!skb))
	{
		verdict = NF_DROP;
		CNT_INC(null_skb);
		return NF_DROP;
	}

	memset(&fw_param, 0, sizeof(tdts_udb_param_t));
	fw_param.skb_ptr = (void *)skb;

#if CONFIG_USE_CONNTRACK_MARK
	conntrack = ip_conntrack_get(skb, &ctinfo);
	if (!conntrack)
	{
		DBG(" * WARNING: empty conntrack in skb"); // Possibly already closed by peer.
		HOOK_EPILOG(verdict);
	}

	fw_param.ct_ptr = (void *)conntrack;
	fw_param.ct_mark = &conntrack->mark;
	fw_param.hook = TDTS_HOOK_FAST_PATH;

	act = udb_shell_get_action(&fw_param);

	if (fcacheStatus() == 0)
		HOOK_EPILOG(verdict);

	if (act == TDTS_ACT_BYPASS)
	{
#define IS_IPV4 1
#define IS_IPV6 0
		CNT_INC(fastpath_conn);

		if (likely(SKB_ETH(skb)))
		{
			if (__SKB_ETH_PRO(skb) == htons(ETH_P_IP))
			{
				ip_conntrack_fc_resume(skb, conntrack, IS_IPV4);
			}
			else
			{
				ip_conntrack_fc_resume(skb, conntrack, IS_IPV6);
			}
		} else {
			blog_skip(skb, blog_skip_reason_dpi);
		}
	}
	else
	{
		blog_skip(skb, blog_skip_reason_dpi);
	}
#endif

	HOOK_EPILOG(verdict);
}

