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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/slab.h> // kmalloc
#include <linux/vmalloc.h> // vmalloc
#include <linux/spinlock.h>

#include "tdts/tdts_core.h"

#define CONFIG_DEBUG (0) //!< Say 1 to debug

#define ERR(fmt, args...) printk(KERN_ERR " *** ERROR: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#define PRT(fmt, args...) printk(KERN_INFO fmt "\n", ##args)

#if CONFIG_DEBUG
#define DBG(fmt, args...) printk(KERN_DEBUG "[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#define assert(_condition) \
    do \
    { \
        if (!(_condition)) \
        { \
            printk(KERN_ERR "\n" "Assertion failed at %s:%d\n", __FUNCTION__, __LINE__); \
            BUG(); \
        } \
    } while(0)
#else
#define DBG(fmt, args...) do { } while (0)
#define assert(_condition) do { } while (0)
#endif

////////////////////////////////////////////////////////////////////////////////
#define PROC_ENTRY_NAME_NK_POLICY "nk_policy"

/*
=================  Policy Link list  ====================
Ver-4.386: # policies: 4804, # patterns:7383
nk_current_signature_num=4804
tracerid=4294967295
port_base_xlate_cnt: 685
max_ptn_db_share: 254
sig_loc_node_used: 11322
ptn_tree_cnt: 0/10240
ptn_seq_node_use_cnt: 0/81920
ptn_loc_nod_use_cnt: 0/81920
bypass_pmatch           : 0     invalid_insp_dep_byte   : 0
invalid_insp_dep_pckt   : 0     pmatch_nosig_miss       : 0
pmatch_hdr_match_only   : 0     pmatch_ac_search        : 0
port_base_hit           : 0     port_base_match         : 0
port_base_1st_rule_miss : 0     eip_match               : 0
eip_hit                 : 0
*** Matched List ***
 */

static int nk_policy_show(struct seq_file *m, void *v)
{
	tdts_core_sig_ver_t sig_ver;
	unsigned rule_num, ptn_num;

	if (tdts_core_get_sig_ver(&sig_ver) < 0)
	{
		return -1;
	}

	if(0!=tdts_core_get_rule_and_ptn_num(&rule_num,&ptn_num))
	{
		return -1;
	}

	seq_printf(m, "=================  Policy Link list  ====================\n");
	seq_printf(m, "Ver-%u.%u # policies:%u, # patterns:%u\n", 
			   sig_ver.major, sig_ver.minor,rule_num,ptn_num);

	seq_printf(m, "*** Matched List ***\n");

	{
		uint32_t buf_len = 0;
		char *buf = NULL;

		if (tdts_core_get_matched_rule_buf_len(&buf_len) < 0)
		{
			return -EINVAL;
		}

		if (0 == buf_len)
		{
			DBG("0 == buf_len\n");
			goto EXIT;
		}
		
		if (!(buf = vmalloc(buf_len)))
		{
			return -ENOMEM;
		}

		if (tdts_core_get_matched_rule_list(buf, buf_len) < 0)
		{
			if (buf)
			{
				vfree(buf);
			}
			return -EINVAL;
		}

		if (buf)
		{
			seq_printf(m, "%s", buf);
			vfree(buf);
		}
	}
	
EXIT:
	return 0;
}

static int nk_policy_open(struct inode *inode, struct file *file)
{
	return single_open(file, nk_policy_show, NULL);
}

static const struct file_operations nk_policy_fops =
{
	.owner   = THIS_MODULE,
	.open    = nk_policy_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};


static int nk_policy_init(void)
{
	proc_create(PROC_ENTRY_NAME_NK_POLICY, 0, NULL, &nk_policy_fops);

	return 0;
}


static void nk_policy_cleanup(void)
{
	remove_proc_entry(PROC_ENTRY_NAME_NK_POLICY, NULL);
}

////////////////////////////////////////////////////////////////////////////////

#define PROC_ENTRY_NAME_IPS_INFO "ips_info"

/*
================= General ====================
Engine version: 1.1.44
IPS function:  on
Max. number of TCP connections: 100K
C2S TCP stream reassembly:  on
S2C TCP stream reassembly:  on
HTTP URL normalization:  on
Dynamic memory allocation: on
================= Anomaly ====================
Number of rules: 60     , enabled: 0
================= Security ====================
Number of rules: 2012   , enabled: 2012
Number of hits: 0
================= Application ====================
Number of rules: 2732
Number of apps(behs) enabled: 98304
Number of hits: 0
==================================================
 */
static int ips_info_show(struct seq_file *m, void *v)
{
	int iret = 0;
	uint32_t maj = 0, mid = 0, min = 0;
	char local[32];

	memset(local, 0x00, sizeof(local));

	if (tdts_core_get_eng_ver(&maj, &mid, &min, local, sizeof(local)) < 0)
	{
		return -EINVAL;
	}

	seq_printf(m, "================= General ====================\n");
	seq_printf(m, "Engine version: %u.%u.%u\n", maj, mid, min);
	seq_printf(m, "Engine local version: %s\n", local);

	if (tdts_core_get_core_ver(&maj, &mid, &min, local, sizeof(local)) < 0)
	{
		return -EINVAL;
	}

	seq_printf(m, "Core version: %u_%u_%u_%s\n", maj, mid, min, local);

	return 0;
}

static int ips_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, ips_info_show, NULL);
}

static const struct file_operations ips_info_fops =
{
	.owner   = THIS_MODULE,
	.open    = ips_info_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int ips_info_init(void)
{
	proc_create(PROC_ENTRY_NAME_IPS_INFO, 0, NULL, &ips_info_fops);

	return 0;
}


static void ips_info_cleanup(void)
{
	remove_proc_entry(PROC_ENTRY_NAME_IPS_INFO, NULL);
}


struct init_tbl_entry
{
	int (*cb_init)(void);
	void (*cb_exit)(void);
};

static struct init_tbl_entry init_tbl[] =
	{
		{ips_info_init, ips_info_cleanup},
		{nk_policy_init, nk_policy_cleanup}
	};

int tdts_shell_deprecated_procfs_init(void)
{
	int i = -1, size;
	int (*cb_init)(void);
	void (*cb_exit)(void);

	size = sizeof(init_tbl) / sizeof(*init_tbl);
	for (i = 0; i < size; i++)
	{
		cb_init = init_tbl[i].cb_init;

		if (cb_init() < 0)
		{
			goto error_exit;
		}
	}

	return 0;

	error_exit:

	/* exit reversely except the failed function. */
	if (i > 0)
	{
		for (i = i - 1; i >= 0; i--)
		{
			cb_exit = init_tbl[i].cb_exit;

			cb_exit();
		}
	}

	return -1;
}

void tdts_shell_deprecated_procfs_cleanup(void)
{
	int i, size;
	void (*cb_exit)(void);

	size = sizeof(init_tbl) / sizeof(*init_tbl);

	for (i = size - 1; i >= 0; i--)
	{
		cb_exit = init_tbl[i].cb_exit;

		cb_exit();
	}
}
