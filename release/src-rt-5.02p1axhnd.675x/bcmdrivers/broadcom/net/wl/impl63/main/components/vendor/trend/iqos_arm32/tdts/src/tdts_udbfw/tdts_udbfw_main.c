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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <net/netfilter/nf_conntrack_helper.h>
#ifdef CONFIG_NF_CONNTRACK_ZONES
#include <net/netfilter/nf_conntrack_zones.h>
#endif
#include <net/netfilter/nf_nat_helper.h>
#include <linux/proc_fs.h>
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

#include "forward_config.h"
#include "skb_access.h"
#include "fw_util.h"
#include "fw_internal.h"
#include "fw_action.h"

#if TMCFG_E_UDB_CORE_IQOS_SUPPORT
#include "fw_qos_cmd.h"
#endif

#if TMCFG_APP_K_TDTS_UDBFW_META_EXTRACT
#include "fw_meta_extract.h"
#endif

#if TMCFG_E_UDB_CORE_URL_QUERY
#include "fw_url_query.h"
#endif

#if TMCFG_E_UDB_CORE_SHN_QUERY
#include "fw_shn_agent.h"
#endif

#if CONFIG_NF_CONNTRACK_EVENTS && TMCFG_APP_K_TDTS_UDBFW_CT_NOTIF
#include "fw_ct_notif.h"
#endif

#if TMCFG_APP_K_TDTS_UDBFW_FAST_PATH
#include "fw_fast_path.h"
#endif

/* memory statistics */
atomic_t ford_mem_sta = ATOMIC_INIT(0);
atomic_t ford_mem_dyn = ATOMIC_INIT(0);

ford_stat_t ford_stat = {ATOMIC_INIT(0)};

volatile bool rmmod_in_progress __read_mostly = false;
DEFINE_PER_CPU(bool, handle_pkt); /* mit: removed "volatile" */

#if !TMCFG_E_UDB_CORE_RULE_FORMAT_V2
static unsigned int tcp_conn_max = 0;
module_param(tcp_conn_max, uint, S_IRUGO|S_IWUSR);
static unsigned int udp_flow_max = 0;
module_param(udp_flow_max, uint, S_IRUGO|S_IWUSR);
#endif

static struct nf_hook_ops hook_ops_forward, hook_ops_forward6;
static struct nf_hook_ops hook_ops_localin, hook_ops_localin6;
static struct nf_hook_ops hook_ops_localout, hook_ops_localout6;
DEFINE_HOOK(hookfn_common);

static int ford_read_procmem(
	char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	unsigned int len = 0;
	int i = 0;

	struct {
		char *cnt_name;
		int cnt_val;
	} cnts[] = {
#define CNT_ETR(__cnt)	{ #__cnt, CNT_READ(__cnt) }
		CNT_ETR(null_skb),
		CNT_ETR(null_skb_dev),
		CNT_ETR(null_skb_ct),

		CNT_ETR(drop),
		CNT_ETR(stolen),

		CNT_ETR(fastpath),
		CNT_ETR(fastpath_conn),

		CNT_ETR(nonip),
		CNT_ETR(unexp),
		CNT_ETR(unmatched_sip),
		CNT_ETR(unmatched_dip),
		CNT_ETR(unmatched_sport),
		CNT_ETR(unmatched_dport),
	};

	for (i = 0; i < ARRAY_SIZE(cnts); i++)
	{
		len += sprintf(buf + len, "%-16s : %5u%c"
			, cnts[i].cnt_name, cnts[i].cnt_val
			, ((i+1) & 1) ? '\t' : '\n');
	}
	len += sprintf(buf + len, "%s", (i & 1) ? "\n" : "");

	*eof = 1;
	return len;
}

static int ford_read_meminfo(
	char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	unsigned int len = 0;

	int fw_mem_sta = 0, fw_mem_dyn = 0, fw_mem_tot = 0;

	fw_mem_sta = atomic_read(&ford_mem_sta);
	fw_mem_dyn = atomic_read(&ford_mem_dyn);
	fw_mem_tot = fw_mem_sta + fw_mem_dyn;
	
	len += sprintf(buf + len, "Fw Static: %d (%d KB)\n", fw_mem_sta, fw_mem_sta >> 10);
	len += sprintf(buf + len, "Fw Dynamic: %d (%d KB)\n", fw_mem_dyn, fw_mem_dyn >> 10);
	len += sprintf(buf + len, "Total: %d (%d KB)\n", fw_mem_tot, fw_mem_tot >> 10);	

	*eof = 1;
	return len;
}

static int __register_hook(
	struct nf_hook_ops *ops, unsigned int hooknum, u_int8_t pf, int prio, nf_hookfn *cb)
{
	ops->hook = cb;
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4,4,0))
	ops->owner = THIS_MODULE;
#endif
	ops->pf = pf;
	ops->hooknum = hooknum;
	ops->priority = prio;

	return nf_register_hook(ops) ? -1 : 0;
}

static int localout_init(void)
{
	if (__register_hook(&hook_ops_localout
		, NF_INET_LOCAL_OUT, PF_INET, NF_IP_PRI_CONNTRACK
		, (nf_hookfn*)forward_hookfn_common) < 0)
	{
		ERR("Cannot register hook_ops_localout!\n");
		return -1;
	}
	DBG("hook_ops_localout hooked\n");

	if (__register_hook(&hook_ops_localout6
		, NF_INET_LOCAL_OUT, PF_INET6, NF_IP_PRI_CONNTRACK
		, (nf_hookfn*)forward_hookfn_common) < 0)
	{
		ERR("Cannot register hook_ops_localout6!\n");
		return -1;
	}
	DBG("hook_ops_localout6 hooked\n");

	return 0;
}

static void localout_exit(void)
{
	nf_unregister_hook(&hook_ops_localout);
	DBG("hook_ops_localout unhooked\n");

	nf_unregister_hook(&hook_ops_localout6);
	DBG("hook_ops_localout unhooked\n");
}

static int localin_init(void)
{
	if (__register_hook(&hook_ops_localin
		, NF_INET_LOCAL_IN, PF_INET, NF_IP_PRI_CONNTRACK
		, (nf_hookfn*)forward_hookfn_common) < 0)
	{
		ERR("Cannot register hook_ops_localin!\n");
		return -1;
	}
	DBG("hook_ops_localin hooked\n");

	if (__register_hook(&hook_ops_localin6
		, NF_INET_LOCAL_IN, PF_INET6, NF_IP_PRI_CONNTRACK
		, (nf_hookfn*)forward_hookfn_common) < 0)
	{
		ERR("Cannot register hook_ops_localin6!\n");
		return -1;
	}
	DBG("hook_ops_localin6 hooked\n");

	return 0;
}

static void localin_exit(void)
{
	nf_unregister_hook(&hook_ops_localin);
	DBG("hook_ops_localin unhooked\n");

	nf_unregister_hook(&hook_ops_localin6);
	DBG("hook_ops_localin unhooked\n");
}

static int hook_init(void)
{
	if (__register_hook(&hook_ops_forward
		, NF_INET_FORWARD
		, PF_INET
		, NF_IP_PRI_FILTER /*NF_IP_PRI_LAST*/
		, (nf_hookfn*)forward_hookfn_common) < 0)
	{
		ERR("Failed to register hook_ops_forward!\n");
		return -1;
	}
	DBG("hook_ops_forward hooked\n");

	/* PF_INET6 */
	if (__register_hook(&hook_ops_forward6
		, NF_INET_FORWARD
		, PF_INET6
		, NF_IP6_PRI_FILTER
		, (nf_hookfn*)forward_hookfn_common) < 0)
	{
		ERR("Failed to register hook_ops_forward6!\n");
		return -1;
	}
	DBG("hook_ops_forward6 hooked\n");

	return 0;
}

static void hook_exit(void)
{
	nf_unregister_hook(&hook_ops_forward6);
	DBG("hook_ops_forward6 unhooked\n");

	nf_unregister_hook(&hook_ops_forward);
	DBG("hook_ops_forward unhooked\n");
}

typedef struct fw_proc_entry
{
	char name[64];
	void *func;
	bool is_proc_fops;

} fw_proc_entry_t;

static fw_proc_entry_t fw_proc_entries[] = {
	{ "bw_ford_info",	ford_read_procmem,	false },
	{ "bw_ford_mem",	ford_read_meminfo,	false },
	/* new entry here */
};

static int nr_fw_proc_entry = ARRAY_SIZE(fw_proc_entries);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	
typedef int (read_proc_t)(char *page, char **start, off_t off,
	int count, int *eof, void *data);

static int proc_to_seq_show(struct seq_file *m, void *v)
{
	static char page_buf[PAGE_SIZE];
	int eof = 0;
	read_proc_t* read_func = (read_proc_t*)m->private;
	
	DBG("do read_func=%p\n", read_func);
	read_func(page_buf, NULL/*start*/, 0, sizeof(page_buf), &eof, NULL/*data*/);

	seq_printf(m, "%s", page_buf);
	return 0;
}

static int proc_oops_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Oops: check this!\n");
	return 0;
}

static int proc_to_seq_open(struct inode *inode, struct file *file)
{
	int i = 0;
	char *iname = file->f_path.dentry->d_iname;
	read_proc_t* read_func = NULL;

	for (i = 0; i < nr_fw_proc_entry; i++)
	{
		fw_proc_entry_t *etr = &fw_proc_entries[i];
		if (!strcmp(iname, etr->name))
		{
			read_func = (read_proc_t*)etr->func;
			break;
		}
	}

	if (read_func)
	{
		DBG("%s: read_func=%p hooked\n", iname, read_func);
		return single_open(file, proc_to_seq_show, read_func);
	}

	return single_open(file, proc_oops_show, NULL);
}
#endif

static int proc_init(void)
{
	int i = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	static struct file_operations fops =
	{
		.open = proc_to_seq_open,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release
	};
#endif
	/* RO entries */
	for (i = 0; i < nr_fw_proc_entry; i++)
	{
		/* proc w/ the new seq_file API */
		if (fw_proc_entries[i].is_proc_fops)
		{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
			proc_create(fw_proc_entries[i].name, S_IRUGO, NULL /* parent */
				, (const struct file_operations*)fw_proc_entries[i].func);
#else
			struct proc_dir_entry *etr;

			etr = create_proc_entry(fw_proc_entries[i].name, S_IRUGO, NULL);
			if (etr)
			{
				etr->proc_fops = (struct file_operations*)fw_proc_entries[i].func;
			}
			else
			{
				printk("create_proc_entry() failed to create %s!\n", fw_proc_entries[i].name);
			}
#endif
		}
		else
		{
			/* proc w/ len limit */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
			/* all share this fops */
			proc_create(fw_proc_entries[i].name, S_IRUGO, NULL, &fops);
#else
			create_proc_read_entry(
				fw_proc_entries[i].name
				, 0 /* default mode */
				, 0 /* parent dir */
				, (int (*)(char*, char**, off_t, int, int*, void*)) fw_proc_entries[i].func
				, NULL /* client data */
			);
#endif
		}
	}

	return 0;
}

static void proc_exit(void)
{
	int i = 0;

	/* no problem if it was not registered */
	for (i = 0; i < nr_fw_proc_entry; i++)
	{
		remove_proc_entry(fw_proc_entries[i].name, 0);
	}
}

#if TMCFG_E_UDB_CORE_CONN_EXTRA
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
#define my_nf_conntrack_find_get(n, z, t) nf_conntrack_find_get(n, z, t)
#else
#define my_nf_conntrack_find_get(n, z, t) nf_conntrack_find_get(n, t)
#endif

int forward_find_ct_by_tuples(void **ct, uint32_t *mark, skb_tuples_t *tuples, uint64_t arg)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conntrack_tuple ct_tuple;
	struct nf_conn *conntrack = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0))
	struct nf_conntrack_zone ct_zone;
	ct_zone.id =(uint16_t)arg;
	ct_zone.flags =(uint8_t) 0;
	ct_zone.dir =(uint8_t) 0;
#else
	uint16_t ct_zone = (uint16_t)arg;

#endif
	if (!tuples)
	{
		return -1;
	}

	memset(&ct_tuple, 0, sizeof(ct_tuple));

	ct_tuple.src.l3num = PF_INET;

	memcpy(&ct_tuple.src.u3.all, &tuples->sip, sizeof(ct_tuple.src.u3.all));
	memcpy(&ct_tuple.dst.u3.all, &tuples->dip, sizeof(ct_tuple.dst.u3.all));

	ct_tuple.dst.protonum = tuples->proto;
	ct_tuple.dst.dir = IP_CT_DIR_ORIGINAL;
	ct_tuple.src.u.all = htons(tuples->sport);
	ct_tuple.dst.u.all = htons(tuples->dport);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0))
	if (!(h = my_nf_conntrack_find_get(&init_net, &ct_zone, &ct_tuple)))
#else
	if (!(h = my_nf_conntrack_find_get(&init_net, ct_zone, &ct_tuple)))
#endif
	{
		//printk(KERN_INFO "cannot find conntrack tuple\n");
		return -1;
	}

	if (!(conntrack = nf_ct_tuplehash_to_ctrack(h)))
	{
		//printk(KERN_INFO "cannot find conntrack\n");
		return -2;
	}

	if (ct)
	{
		*ct = conntrack;
	}

	if (mark)
	{
		*mark = conntrack->mark;
	}

	nf_ct_put(conntrack);
	return 0;
}
#endif

typedef int (*cb_init_t)(void);
typedef void (*cb_exit_t)(void);

struct init_tbl_entry
{
	cb_init_t cb_init;
	cb_exit_t cb_exit;
};

static struct init_tbl_entry init_tbl[] =
{
#if TMCFG_E_UDB_CORE_URL_QUERY
	{wrs_init, wrs_deinit},
#endif

#if TMCFG_E_UDB_CORE_SHN_QUERY
	{shn_agent_init, shn_agent_deinit},
#endif
#if TMCFG_APP_K_TDTS_UDBFW_META_EXTRACT
	{mt_init, mt_exit},
#endif
#if TMCFG_E_UDB_CORE_IQOS_SUPPORT
	{qos_cmd_init, qos_cmd_exit},
#endif
#if CONFIG_NF_CONNTRACK_EVENTS && TMCFG_APP_K_TDTS_UDBFW_CT_NOTIF
	{ct_nf_init, ct_nf_exit},
#endif
	{proc_init, proc_exit},	

	/*
	 * Since the following init functions will
	 * register for a Netfilter hook, put them
	 * at the tail
	 */
#if TMCFG_APP_K_TDTS_UDBFW_FAST_PATH
	{fast_path_hook_init, fast_path_hook_exit},
#endif
	/* FORWARD */
	{hook_init, hook_exit},	
	/* LOCALIN */
	{localin_init, localin_exit},
	/* LOCALOUT */
	{localout_init, localout_exit},
};

static void mod_epilog(void)
{
#define MAX_SEC_IN_EPILOG	5
#ifndef time_is_before_eq_jiffies
#define time_is_before_eq_jiffies(a) time_after_eq(jiffies, a)
#endif

	unsigned long start_jiff = jiffies;
	int i = 0;

	/* signal other cores */
	rmmod_in_progress = true;
	smp_mb();

	for_each_present_cpu(i)
	{
		while(per_cpu(handle_pkt, i))
		{
			if (smp_processor_id() == i)
			{
				pr_emerg("Oops: HOOK_PROLOG() is not used properly !?");
				return;
			}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
			mdelay(50 * 1000);
#else
			cpu_relax();
#endif

			if (time_is_before_eq_jiffies(start_jiff  + (MAX_SEC_IN_EPILOG*HZ)))
			{
				pr_emerg("Oops: Stuck in CPU#%d for more than %d sec;"
					" break the loop!\n", i, MAX_SEC_IN_EPILOG);
				break;
			}
		}

		if (time_is_before_eq_jiffies(start_jiff  + (MAX_SEC_IN_EPILOG*HZ)))
		{
			pr_emerg("Oops: Stuck for more than %d sec;"
				" break the loop!\n", MAX_SEC_IN_EPILOG);

			/* show per-cpu states */
			for_each_present_cpu(i)
			{
				pr_emerg("CPU#%d: %d ", i, per_cpu(handle_pkt, i));
			}
			pr_emerg("\n");
		}
	}

	pr_info("mod epilog takes %ld jiffies\n", jiffies - start_jiff);
}

static void __forward_exit(int size)
{
	mod_epilog();

	PRT("Exit %s", FWMOD_NAME);

	while (--size >= 0)
	{
		cb_exit_t cb_exit = init_tbl[size].cb_exit;
		if (cb_exit)
		{
			cb_exit();
		}
	}

#if TMCFG_E_UDB_CORE_CONN_EXTRA
	udb_shell_reg_func_find_ct(NULL);
#endif

	if (atomic_read(&ford_mem_sta) != 0 || atomic_read(&ford_mem_dyn) != 0)
	{
		ERR("Fw static = %d, dynamic = %d\n",
			atomic_read(&ford_mem_sta), atomic_read(&ford_mem_dyn));
	}
}

int forward_init(void)
{
	int ret = 0;
	int i = 0, size = 0;

#if !TMCFG_E_UDB_CORE_RULE_FORMAT_V2
	if (0 < tcp_conn_max)
	{
		if (tdts_shell_system_setting_tcp_conn_max_set(tcp_conn_max))
		{
			pr_emerg("Failed to set maximum TCP connection number!\n");
			return -1;
		}
		PRT("Apply module param tcp_conn_max=%u", tcp_conn_max);
	}

	if (0 < udp_flow_max)
	{
		if (tdts_shell_system_setting_udp_conn_max_set(udp_flow_max))
		{
			pr_emerg("Failed to set maximum UDP flow number!\n");
			return -1;
		}
		PRT("Apply module param udp_flow_max=%u", udp_flow_max);
	}
#endif

#if TMCFG_E_UDB_CORE_CONN_EXTRA
	udb_shell_reg_func_find_ct(forward_find_ct_by_tuples);
#endif

	size = ARRAY_SIZE(init_tbl);
	for (i = 0; i < size; i++)
	{
		cb_init_t cb_init = init_tbl[i].cb_init;
		if (cb_init && (ret = cb_init()) < 0)
		{
			pr_emerg("init item #%d failed (%d)!\n", i, ret);
			goto error_exit;
		}
	}

	PRT("%s is ready", FWMOD_NAME);
	printk("sizeof forward pkt param = %d\n", (int)(sizeof(tdts_pkt_parameter_t)));

	return 0;

error_exit:
	/* exit reversely except the failed function. */
	__forward_exit(i);

	return ret;
}

void forward_exit(void)
{
	PRT("Exit %s", FWMOD_NAME);

	__forward_exit(ARRAY_SIZE(init_tbl));
}

static inline void
set_skb_type(struct sk_buff *skb, tdts_udb_param_t *fw_param)
{
	char ip_ver = SKB_IP_VER(skb);
	skb_type_t *type = &fw_param->skb_type;
	uint16_t sport = 0, dport = 0;

	*type = SKB_TYPE_MISC;

	if (unlikely(parse_skb(skb) < 0))
	{
		return;
	}

	if (!SKB_L4_HEAD_ADDR(skb))
	{
		DBG("Oops: L4 hdr is NULL w/ hook = %d, pro=%.2x, skb->pro=%.4x, L2=%p!\n"
			, fw_param->hook, (4 == ip_ver) ? SKB_IP_PRO(skb) : SKB_IPV6_NHDR(skb)
			, skb->protocol, SKB_ETH_ADDR(skb));
		return;
	}

	if (fw_param->skb_tuples.proto == IPPROTO_UDP)
	{
		sport = fw_param->skb_tuples.sport;
		dport = fw_param->skb_tuples.dport;

		if (((TDTS_HOOK_NF_LIN == fw_param->hook) && (53 == dport)) ||  /* DNS request client -> router */
			((TDTS_HOOK_NF_LOUT == fw_param->hook) && (53 == sport))) /*DNS reply to client*/
		{
			*type = SKB_TYPE_DNS;
			return;
		}

		if ((ip_ver == 4) && (unlikely((sport == 67 && dport == 68) ||
				(sport == 68 && dport == 67))))
		{
			DBG("Get a DHCP packet at localin\n");
			*type = SKB_TYPE_DHCP;
			return;
		}
		else if ((ip_ver == 6) && (unlikely((sport == 546 && dport == 547) ||
				(sport == 547 && dport == 546))))
		{
			DBG("Get a DHCPv6 packet at localin\n");
			*type = SKB_TYPE_DHCP;
			return;
		}
	}
	else if (fw_param->skb_tuples.proto == IPPROTO_TCP)
	{
		if (SKB_TCP_SYN_VAL(skb) == SKB_TCP_FLAGS(skb))
		{
			*type = SKB_TYPE_TCP_SYN;
			return;
		}
		else if ((SKB_TCP_SYN_VAL(skb) | SKB_TCP_ACK_VAL(skb)) == SKB_TCP_FLAGS(skb))
		{
			*type = SKB_TYPE_TCP_SYN_ACK;
			return;
		}
	}

	return;
}

static inline void
set_skb_flag(struct sk_buff *skb, tdts_udb_param_t *fw_param)
{
	uint8_t ip_ver = 0, ip_pro = 0;
	uint32_t *flag = &fw_param->skb_flag;

	if (likely(skb->protocol))
	{
		ip_ver = ((ETH_P_IPV6 == ntohs(skb->protocol)) ? 6 : 4);
		ip_pro = (6 == ip_ver) ? SKB_IPV6_NHDR(skb) : SKB_IP_PRO(skb);
	}

	if (likely(skb->dev))
	{
		SET_SKB_LOOPBACK(*flag, (!strcmp(skb->dev->name, "lo")));
	}

	SET_SKB_UPLOAD(*flag, is_skb_upload(skb, fw_param->dev.fw_outdev));
	SET_SKB_L2_HEADER(*flag, (SKB_ETH_ADDR(skb)));
	SET_SKB_L3_HEADER(*flag, (SKB_L3_HEAD_ADDR(skb)));
	SET_SKB_L4_HEADER(*flag, (SKB_L4_HEAD_ADDR(skb)));

	SET_SKB_ICMP_DEST_UNREACHABLE(*flag, false);

	if (IPPROTO_ICMP == ip_pro)
	{
		//workaround: for BT --> ICMP issue
		//please fix me!!
		uint8_t hdrlen = SKB_IP_IHL(skb) * 4;
		struct icmphdr *icmp_h = NULL;
		
		skb_pull(skb, hdrlen);
		icmp_h = (struct icmphdr *)skb->data;
		if (icmp_h->type == 3) //ICMP type = 3 => destination unreachable
		{
			SET_SKB_ICMP_DEST_UNREACHABLE(*flag, true);
		}
		skb_push(skb, hdrlen);
	}
	else if (IPPROTO_TCP == ip_pro)
	{
		skb_pull(skb, SKB_IP_IHL(skb) * 4);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
		skb_set_transport_header(skb, 0);
#else
		SKB_TCP_HEAD_ADDR(skb) = (struct tcphdr *)skb->data;
#endif
		skb_push(skb, SKB_IP_IHL(skb) * 4);
	}
	else if (IPPROTO_UDP == ip_pro)
	{
		skb_pull(skb, SKB_IP_IHL(skb) * 4);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22))
		skb_set_transport_header(skb, 0);
#else
		SKB_L4_HEAD_ADDR(skb) = skb->data;
#endif
		skb_push(skb, SKB_IP_IHL(skb) * 4);
	}
}

int set_skb_tuples(struct sk_buff *skb, tdts_udb_param_t *fw_param)
{
	skb_tuples_t *tuple = &fw_param->skb_tuples;
	struct ip_conntrack *ct =  (struct ip_conntrack *)fw_param->ct_ptr;

	if ((SKB_ETH(skb)) && (fw_param->hook != TDTS_HOOK_NF_LOUT)) /* NULL for localout */
	{
		switch (ntohs(__SKB_ETH_PRO(skb)))
		{
			case ETH_P_IP:
				fw_param->skb_eth_p = SKB_ETH_P_IP;
				break;
			case ETH_P_IPV6:
				fw_param->skb_eth_p = SKB_ETH_P_IPV6;
				break;
			default:
				fw_param->skb_eth_p = SKB_ETH_P_OTHER;
				break;
		}
	}
	else
	{
		fw_param->skb_eth_p = SKB_ETH_P_OTHER;
	}

	tuple->ip_ver = SKB_IP_VER(skb);
	/* Copy ip from ct? */
#if CONFIG_USE_CONNTRACK_MARK
	if (ct)
	{
		if (4 == tuple->ip_ver)
		{
			tuple->proto = SKB_IP_PRO(skb);
			if (!IS_CT_REPLY(fw_param->ct_flag))
			{
				memcpy(&tuple->sip.ipv4, (uint8_t*)&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip, 4);
				memcpy(&tuple->dip.ipv4, (uint8_t*)&ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip, 4);
			}
			else
			{
				memcpy(&tuple->dip.ipv4, (uint8_t*)&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip, 4);
				memcpy(&tuple->sip.ipv4, (uint8_t*)&ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip, 4);
			}

			if (memcmp(&tuple->sip.ipv4, &SKB_IP_SIP(skb), 4))
			{
				CNT_INC(unmatched_sip);
			}

			if (memcmp(&tuple->dip.ipv4, &SKB_IP_DIP(skb), 4))
			{
				CNT_INC(unmatched_dip);
			}
		}
		else if (6 == tuple->ip_ver)
		{
			tuple->proto = SKB_IPV6_NHDR(skb);
			if (!IS_CT_REPLY(fw_param->ct_flag))
			{
				memcpy(&tuple->sip.ipv6, (uint8_t*)&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.in6.in6_u.u6_addr8, 16);
				memcpy(&tuple->dip.ipv6, (uint8_t*)&ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.in6.in6_u.u6_addr8, 16);
			}
			else
			{
				memcpy(&tuple->dip.ipv6, (uint8_t*)&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.in6.in6_u.u6_addr8, 16);
				memcpy(&tuple->sip.ipv6, (uint8_t*)&ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.in6.in6_u.u6_addr8, 16);
			}

			if (memcmp(&tuple->sip.ipv6, SKB_IPV6_SIP(skb), 16))
			{
				CNT_INC(unmatched_sip);
			}

			if (memcmp(&tuple->dip.ipv6, SKB_IPV6_DIP(skb), 16))
			{
				CNT_INC(unmatched_dip);
			}
		}
		else
		{
			CNT_INC(nonip);
			return -2;
		}
	}
	else
#endif
	{
		if (4 == tuple->ip_ver)
		{
			tuple->proto = SKB_IP_PRO(skb);
			memcpy(&tuple->sip.ipv4, (uint8_t*)&SKB_IP_SIP(skb), 4);
			memcpy(&tuple->dip.ipv4, (uint8_t*)&SKB_IP_DIP(skb), 4);
		}
		else if (6 == tuple->ip_ver)
		{
			tuple->proto = SKB_IPV6_NHDR(skb);
			memcpy(&tuple->sip.ipv6, SKB_IPV6_SIP(skb), 16);
			memcpy(&tuple->dip.ipv6, SKB_IPV6_DIP(skb), 16);
		}
		else
		{
			CNT_INC(nonip);
			return -2;
		}
	}

#if CONFIG_USE_CONNTRACK_MARK
	if ((ct) && ((IPPROTO_TCP == tuple->proto) || (IPPROTO_UDP == tuple->proto)))
	{
		if (!IS_CT_REPLY(fw_param->ct_flag))
		{
			tuple->sport = ntohs(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.all);
			tuple->dport = ntohs(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.all);
		}
		else
		{
			tuple->sport = ntohs(ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u.all);
			tuple->dport = ntohs(ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.all);
		}

		if (IPPROTO_TCP == tuple->proto)
		{
			if (tuple->sport != ntohs(SKB_TCP_SPORT(skb)))
			{
				CNT_INC(unmatched_sport);
			}

			if (tuple->dport != ntohs(SKB_TCP_DPORT(skb)))
			{
				CNT_INC(unmatched_dport);
			}
		}
		else
		{
			if (tuple->sport != ntohs(SKB_UDP_SPORT(skb)))
			{
				CNT_INC(unmatched_sport);
			}

			if (tuple->dport != ntohs(SKB_UDP_DPORT(skb)))
			{
				CNT_INC(unmatched_dport);
			}
		}
	}
	else
#else
	if (IPPROTO_TCP == tuple->proto)
	{
		tuple->sport = ntohs(SKB_TCP_SPORT(skb));
		tuple->dport = ntohs(SKB_TCP_DPORT(skb));
	}
	else if (IPPROTO_UDP == tuple->proto)
	{
		tuple->sport = ntohs(SKB_UDP_SPORT(skb));
		tuple->dport = ntohs(SKB_UDP_DPORT(skb));
	}
	else
#endif
	{
		tuple->sport = tuple->dport = -1;
	}
	return 0;
}

#if CONFIG_USE_CONNTRACK_MARK
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
static inline bool ipv4_is_loopback(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x7f000000);
}
static inline bool ipv4_is_zeronet(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x00000000);
}
static inline bool ipv4_is_lbcast(__be32 addr)
{
	return addr == htonl(INADDR_BROADCAST);
}
#endif

static inline void
set_ct_flag(
	struct ip_conntrack *ct
	, enum ip_conntrack_info *ct_info
	, tdts_udb_param_t *fw_param)
{
	unsigned int addr_type = 0;
	skb_tuples_t *tuples = &fw_param->skb_tuples;
	uint32_t *flag = &fw_param->ct_flag;

	switch (fw_param->hook)
	{
	case TDTS_HOOK_NF_FORD:
		SET_CT_REPLY(*flag, (*ct_info >= IP_CT_IS_REPLY));
		SET_CT_NEW(*flag, (0 == ct->mark));

		if (likely(4 == tuples->ip_ver))
		{
			__be32 dip_i = *(__be32*)&tuples->dip.ipv4;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
			addr_type = inet_addr_type(&init_net, dip_i);
#else	// 2.6.24 ~ 2.6.26
			addr_type = inet_addr_type(dip_i);
#endif
#endif
			SET_CT_LOCAL(*flag, (RTN_LOCAL == addr_type) || ipv4_is_loopback(dip_i));

			if (ipv4_is_zeronet(dip_i) || ipv4_is_lbcast(dip_i) ||
				((dip_i & htonl(0x00FFFFFF)) == htonl(0x00FFFFFF)) ||
				((dip_i & htonl(0x0000FFFF)) == htonl(0x0000FFFF)) ||
				((dip_i & htonl(0x000000FF)) == htonl(0x000000FF)))
			{
				SET_CT_MCAST(*flag, true);
			}
			else
			{
				SET_CT_MCAST(*flag, (RTN_MULTICAST == addr_type));
			}
		}
		else if (6 == tuples->ip_ver)
		{
			struct in6_addr *dip = (struct in6_addr *)tuples->dip.ipv6;
			struct in6_addr *sip = (struct in6_addr *)tuples->sip.ipv6;

			addr_type = ipv6_addr_type(dip);
			if (IPV6_ADDR_LOOPBACK & ipv6_addr_type(sip))
			{
				SET_CT_LOCAL(*flag, true);
			}
			else if (IPV6_ADDR_MULTICAST & addr_type)
			{
				SET_CT_MCAST(*flag, true);
			}
		}
		else
		{
			return;
		}

		if ((IPPROTO_ICMP != tuples->proto) &&
			(IPPROTO_ICMPV6 != tuples->proto) &&
			(IPPROTO_TCP != tuples->proto) &&
			(IPPROTO_UDP != tuples->proto))
		{
			SET_CT_OTHER_PROTO(*flag, true);
		}

		// no break here

	case TDTS_HOOK_NF_LIN:
	case TDTS_HOOK_NF_LOUT:
		SET_CT_REPLY(*flag, (*ct_info >= IP_CT_IS_REPLY));
		/*
		 * bypass pkts which are not conntracked
		 * net/netfilter/nf_conntrack_core.c:
		 *	struct nf_conn nf_conntrack_untracked __read_mostly;
		 *	EXPORT_SYMBOL_GPL(nf_conntrack_untracked);
		 */
		SET_CT_UNTRACK(*flag, (ct == &nf_conntrack_untracked));

		break;

	default:
		break;
	}
}
#endif

inline void setup_param(struct sk_buff *skb , tdts_udb_param_t *fw_param
#if CONFIG_USE_CONNTRACK_MARK
	, struct ip_conntrack *ct, enum ip_conntrack_info *ctinfo
#endif
	, struct sock *sk
	, struct net_device *in_dev
	, struct net_device *out_dev
	, void *okfn)
{
#if CONFIG_USE_CONNTRACK_MARK
	if (ct)
	{
		fw_param->ct_ptr = (void *)ct;
		fw_param->ct_mark = &ct->mark;
		fw_param->ct_data = 0;
#ifdef CONFIG_NF_CONNTRACK_ZONES
		fw_param->ct_data = (uint64_t)nf_ct_zone(ct);
#endif
		set_ct_flag(ct,	ctinfo,	fw_param);
	}
	else
#endif
	{
		CNT_INC(null_skb_ct);
	}

	set_skb_flag(skb, fw_param);
	fw_param->skb_ptr = (void *)skb;
	set_skb_tuples(skb, fw_param);
	set_skb_type(skb, fw_param);
	fw_param->skb_mark = &skb->mark;
	fw_param->skb_payload_len = (4 == SKB_IP_VER(skb)) ? ntohs(SKB_IP_TOT_LEN(skb)) : ntohs(SKB_IPV6_PLEN(skb));
	fw_param->skb_len = skb->len;
	fw_param->skb_dev = (void *)skb->dev; /* NULL for localout*/

	fw_param->dev.fw_sk = sk;
	fw_param->dev.fw_send = okfn;
	fw_param->dev.fw_indev = in_dev;
	fw_param->dev.fw_outdev = out_dev;

	fw_param->skb_eth_src = SKB_ETH_SRC(skb);
	fw_param->send_redir_page = send_redir_page;

#if TMCFG_E_UDB_CORE_HWNAT
	fw_param->fast_path_data =(void*)(skb->head);
#endif

	if ((!fw_param->skb_dev) && (fw_param->hook != TDTS_HOOK_NF_LOUT))
	{
		CNT_INC(null_skb_dev);
	}
#if TMCFG_E_UDB_CORE_URL_QUERY
	wrs_set_fw_param_cb(fw_param);
#endif

#if TMCFG_E_UDB_CORE_SHN_QUERY
	shnagent_set_cb(fw_param);
#endif
}

static uint32_t hookfn_common(
	uint32_t hooknum,
	struct sock *sk,
	struct sk_buff *skb,
	const struct net_device *in_dev,
	const struct net_device *out_dev,
	void* okfn)
{
	int ret = TDTS_RES_ACCEPT;
	uint32_t verdict = NF_ACCEPT;
	tdts_udb_param_t fw_param;
#if CONFIG_USE_CONNTRACK_MARK
	enum ip_conntrack_info ctinfo;
	struct ip_conntrack *ct = nf_ct_get(skb, &ctinfo);
#endif

	HOOK_PROLOG(verdict);

	if (unlikely((!skb) || (!SKB_L3_HEAD_ADDR(skb))))
	{
		verdict = NF_DROP;
		CNT_INC(null_skb);
		goto __EXIT;
	}

	memset(&fw_param, 0, sizeof(tdts_udb_param_t));

	switch (hooknum)
	{
		case NF_INET_FORWARD:
			fw_param.hook = TDTS_HOOK_NF_FORD;
#if CONFIG_USE_CONNTRACK_MARK
			if (likely(ct))
			{
				fw_param.ct_ptr = ct;
				fw_param.ct_mark = &ct->mark;
				fw_param.skb_ptr = (void *)skb;

				if (likely(fw_param.ct_mark))
				{
					SET_SKB_UPLOAD(fw_param.skb_flag, is_skb_upload(skb, out_dev));
					fw_param.skb_mark = &skb->mark;
					fw_param.skb_len = skb->len;

					ret = udb_shell_do_fastpath_action(&fw_param);
					if (ret != TDTS_RES_CONTINUE)
					{
						goto __EXIT;
					}
				}
			}
#endif
			break;
		case NF_INET_LOCAL_IN:
			fw_param.hook = TDTS_HOOK_NF_LIN;
			break;
		case NF_INET_LOCAL_OUT:
			fw_param.hook = TDTS_HOOK_NF_LOUT;
			skb->protocol =  (4 == ((*(skb->data)) >> 4))? htons(ETH_P_IP) : htons(ETH_P_IPV6);
			break;
		default:
			goto __EXIT;
	}

	setup_param(skb, &fw_param
#if CONFIG_USE_CONNTRACK_MARK
		, ct, &ctinfo
#endif
		, sk
		, (struct net_device *)in_dev
		, (struct net_device *)out_dev
		, okfn);

	ret = udb_shell_policy_match(&fw_param);

__EXIT:
	switch (ret)
	{
	case TDTS_RES_DROP:
		CNT_INC(drop);
		verdict = NF_DROP;
		break;

	case TDTS_RES_STOLEN:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35))
		skb_dst_force(skb); // force a refcount on dst
#endif
		CNT_INC(stolen);
		verdict = NF_STOLEN;
		break;

	case TDTS_RES_ACCEPT:
	default:
		verdict = NF_ACCEPT;
		break;
	}

	HOOK_EPILOG(verdict);
}

MODULE_LICENSE("GPL");
module_init(forward_init);
module_exit(forward_exit);

