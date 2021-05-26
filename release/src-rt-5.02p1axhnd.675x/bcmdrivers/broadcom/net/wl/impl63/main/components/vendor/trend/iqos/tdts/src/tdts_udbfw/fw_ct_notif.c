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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <asm/atomic.h>

#ifdef CONFIG_NF_CONNTRACK_EVENTS
#include <linux/netfilter/nf_conntrack_common.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#else
#error "CONFIG_NF_CONNTRACK_EVENTS is not defined!"
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)) || defined(CONFIG_NF_CONNTRACK_CHAIN_EVENTS)
#define CONNTRACK_NOTIFIER_BLOCK
#endif

#ifdef CONNTRACK_NOTIFIER_BLOCK
#include <linux/notifier.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
#include <net/netfilter/nf_conntrack.h>
/* Mapping Kernel 2.6 Netfilter API to Kernel 2.4 */
#define ip_conntrack        nf_conn
#define ip_conntrack_get    nf_ct_get
#define ip_conntrack_tuple  nf_conntrack_tuple
#else // older kernel
#include <linux/netfilter_ipv4/ip_conntrack.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0))
#define REGISTER_NOTIFIER(n) nf_conntrack_register_notifier(&init_net, n)
#define UNREGISTER_NOTIFIER(n) nf_conntrack_unregister_notifier(&init_net, n)
#else
#define REGISTER_NOTIFIER(n) nf_conntrack_register_notifier(n)
#define UNREGISTER_NOTIFIER(n) nf_conntrack_unregister_notifier(n)
#endif

#include "tdts_udb.h"

MODULE_LICENSE("GPL");

#undef DBG
//#define DBG(fmt, args...)	printk("[%s(%d)]: "fmt"\n", __func__, __LINE__, ##args)
#define DBG(fmt, args...)	do{} while (0)

#undef ERR
//#define ERR(fmt, args...)	pr_emerg(fmt"\n", ##args)
#define ERR(fmt, args...)	do{} while (0)

#define PROC_CTE_NOTI	"bw_cte_noti"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
#define MY_IPCT_NEW	(1 << IPCT_NEW_BIT)
#define MY_IPCT_DESTROY	(1 << IPCT_DESTROY_BIT)

#define IPCT_MAX_EVT	(IPCT_SECMARK_BIT + 1) /* assume the last event is IPCT_SECMARK_BIT */
#else
#define MY_IPCT_NEW	(1 << IPCT_NEW)
#define MY_IPCT_DESTROY	(1 << IPCT_DESTROY)

#define IPCT_MAX_EVT	(IPCT_SECMARK + 1) /* assume the last event is IPCT_SECMARK */
#endif

static atomic_t ipct_evt_cnt[IPCT_MAX_EVT] = { [0 ... IPCT_MAX_EVT-1] = ATOMIC_INIT(0) };

#ifndef CONFIG_NF_CONNTRACK_CHAIN_EVENTS
#define FORCE_NOTIFIER_REG
#endif

#ifdef CONNTRACK_NOTIFIER_BLOCK
typedef struct notifier_block my_nf_ct_event_notifier_t;
#else
typedef struct nf_ct_event_notifier __rcu my_nf_ct_event_notifier_t;
#endif

#ifdef FORCE_NOTIFIER_REG
static my_nf_ct_event_notifier_t *orig_notifier = NULL;
static my_nf_ct_event_notifier_t *new_notifier = NULL;
#endif

#ifdef CONNTRACK_NOTIFIER_BLOCK
static int handle_ct_event(struct notifier_block *this, unsigned long events, void *ptr)
#else
static int handle_ct_event(unsigned int events, struct nf_ct_event *item)
#endif
{
	int i = 0;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)) // CONNTRACK_NOTIFIER_BLOCK is also defined
	struct nf_conn *ct = (struct nf_conn *)ptr;
#else
#ifdef CONNTRACK_NOTIFIER_BLOCK
	struct nf_ct_event *item = (struct nf_ct_event *)ptr;
#endif
	struct nf_conn *ct = item->ct;
#endif // (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))

#ifdef FORCE_NOTIFIER_REG
	/* deliver to the original notifier first */
#ifdef CONNTRACK_NOTIFIER_BLOCK
	if (orig_notifier && orig_notifier->notifier_call)
	{
		orig_notifier->notifier_call(this, events, ptr);
	}
#else
	if (orig_notifier && orig_notifier->fcn)
	{
		orig_notifier->fcn(events, item);
	}
#endif
#endif // FORCE_NOTIFIER_REG

	if (!ct)
	{
		ERR("Receive CT_EVENT without conntrack!");
		return -1;
	}

	if (events & MY_IPCT_NEW)
	{
#ifdef CONN_EXTRA_NEW_BY_EVT
		new_ct_extra_entry(ct, IP_CT_DIR_ORIGINAL, 0/*ctinfo*/);
#else
		//mark_ct_extra_entry(ct);
		udb_shell_ct_event_handler((void *)ct, ct->mark, TDTS_UDB_CT_EVT_NEW);
#endif
	}
	else if (events & MY_IPCT_DESTROY)
	{
		//free_ct_extra_entry(ct);
		udb_shell_ct_event_handler((void *)ct, ct->mark, TDTS_UDB_CT_EVT_DESTROY);
	}

	/* per event counting */
	for (i = 0; i < IPCT_MAX_EVT; i++)
	{
		if (events & (1 << i))
		{
			atomic_inc(&ipct_evt_cnt[i]);
		}
	}

	return 0;
}

#ifdef CONNTRACK_NOTIFIER_BLOCK
static struct notifier_block event_notifier =
{
	.notifier_call = handle_ct_event,
};
#else
static struct nf_ct_event_notifier event_notifier =
{
	.fcn = handle_ct_event,
};
#endif

static int ct_read_procmem(
	char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	unsigned int len = 0;
	int i = 0;

	struct {
		char *cnt_name;
		int cnt_val;
	} cnts[] = {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
#define CNT_ETR(__n)	{ "IPCT_"#__n, atomic_read(&ipct_evt_cnt[IPCT_##__n##_BIT]) }
#else
#define CNT_ETR(__n)	{ "IPCT_"#__n, atomic_read(&ipct_evt_cnt[IPCT_##__n]) }
#endif
		CNT_ETR(NEW),
		CNT_ETR(RELATED),
		CNT_ETR(DESTROY),
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31))
		CNT_ETR(REFRESH),
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34))
		CNT_ETR(STATUS),
#else
		CNT_ETR(REPLY),
		CNT_ETR(ASSURED),
#endif
		CNT_ETR(PROTOINFO),
		CNT_ETR(HELPER),
		CNT_ETR(MARK),
		//CNT_ETR(NATSEQADJ),
		CNT_ETR(SECMARK),
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

typedef struct ct_proc_entry
{
	char name[64];
	void *func;
	bool is_proc_fops;

} ct_proc_entry_t;

static ct_proc_entry_t ct_proc_entries[] = {
	/*
	 * For those to be returned as DebugInfo, prefixing the file w/ "nk_"
	 */
	/* infrastructure */
	{ PROC_CTE_NOTI,	ct_read_procmem,	false },
	/* new entry here */
};

static int nr_ct_proc_entry = ARRAY_SIZE(ct_proc_entries);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
typedef ssize_t (*proc_write_t)(
	struct file *filp, const char __user *buff, size_t len, loff_t *data);

#define PI_ETR(__n) \
{ \
	.name = "bw_"#__n \
	, .fops.open = __n##_open_proc \
	, .fops.read = seq_read \
	, .fops.llseek = seq_lseek \
	, .fops.release = __n##_release_proc \
	, .fops.write = (proc_write_t)__n##_write_proc \
}
#else
#define PI_ETR(__n) \
{ \
	.name = "bw_"#__n \
	, .read_func = __n##_read_proc \
	, .write_func = __n##_write_proc \
}
#endif

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

	for (i = 0; i < nr_ct_proc_entry; i++)
	{
		ct_proc_entry_t *etr = &ct_proc_entries[i];
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

static int ct_proc_init(void)
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
	for (i = 0; i < nr_ct_proc_entry; i++)
	{
		/* proc w/ the new seq_file API */
		if (ct_proc_entries[i].is_proc_fops)
		{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
			proc_create(ct_proc_entries[i].name, S_IRUGO, NULL /* parent */
				, (const struct file_operations*)ct_proc_entries[i].func);
#else
			struct proc_dir_entry *etr;

			etr = create_proc_entry(ct_proc_entries[i].name, S_IRUGO, NULL);
			if (etr)
			{
				etr->proc_fops = (struct file_operations*)ct_proc_entries[i].func;
			}
			else
			{
				pr_emerg("create_proc_entry() failed to create %s!\n", ct_proc_entries[i].name);
			}
#endif
		}
		else
		{
			/* proc w/ len limit */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
			/* all share this fops */
			proc_create(ct_proc_entries[i].name, S_IRUGO, NULL, &fops);
#else
			create_proc_read_entry(
				ct_proc_entries[i].name
				, 0 /* default mode */
				, 0 /* parent dir */
				, (int (*)(char*, char**, off_t, int, int*, void*)) ct_proc_entries[i].func
				, NULL /* client data */
			);
#endif
		}
	}

	return 0;
}

static void ct_proc_exit(void)
{
	int i = 0;

	/* no problem if it was not registered */
	for (i = 0; i < nr_ct_proc_entry; i++)
	{
		remove_proc_entry(ct_proc_entries[i].name, 0);
	}
}

void ct_nf_exit(void)
{
	ct_proc_exit();

#ifdef FORCE_NOTIFIER_REG
	if (orig_notifier)
	{	/* restore the original cb */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0))
		struct net *net = &init_net;
		if (new_notifier == rcu_dereference(net->ct.nf_conntrack_event_cb))
		{
			rcu_assign_pointer(net->ct.nf_conntrack_event_cb, orig_notifier);
		}
#else
		if (new_notifier == nf_conntrack_event_cb)
		{
			nf_conntrack_event_cb = orig_notifier;
		}
#endif
		else
		{
			pr_emerg("ERR: ct_event notifier has already been changed!\n");
			return;
		}

		DBG("restore ct_event notifier from %p to %p", new_notifier, orig_notifier);
	}
	else
	{
		UNREGISTER_NOTIFIER(&event_notifier);
	}
#else
	UNREGISTER_NOTIFIER(&event_notifier);
#endif
}

int ct_nf_init(void)
{
	int ret = 0;

	ret = REGISTER_NOTIFIER(&event_notifier);
        DBG("conntrack notifier registered notifier[%d]!", ret);

	if (ret < 0)
	{
		ERR("conntrack notifier is already registered!");

#ifdef FORCE_NOTIFIER_REG
		/*
		 * Force the reassignment of nf_conntrack_event_cb
		 * to our handler in case it's already assigned (registered)
		 */
		new_notifier = &event_notifier;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0))
		{
			struct net *net = &init_net;
			if ((orig_notifier = rcu_dereference(net->ct.nf_conntrack_event_cb)))
			{
				rcu_assign_pointer(net->ct.nf_conntrack_event_cb, new_notifier);
			}
		}
#else
		if ((orig_notifier = nf_conntrack_event_cb))
		{
			nf_conntrack_event_cb = new_notifier;
		}
#endif
		DBG("replace ct_event notifier from %p to %p", orig_notifier, new_notifier);
#else
		pr_emerg("conntrack notifier registered failure!\n");
		return -1;
#endif
	}
	else
	{
		DBG("conntrack notifier is registered!");
	}

	if (ct_proc_init() < 0)
	{
		ERR("proc_init() failed!");
		ct_nf_exit();
		return -2;
	}

	return 0;
}

