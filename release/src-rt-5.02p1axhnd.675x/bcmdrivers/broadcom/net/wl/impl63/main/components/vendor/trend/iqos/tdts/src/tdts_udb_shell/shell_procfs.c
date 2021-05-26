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
#include <asm/uaccess.h>

#include <linux/slab.h> // kmalloc
#include <linux/vmalloc.h> // vmalloc
#include <linux/spinlock.h>

#include "udb/shell/shell_procfs.h"

#if TMCFG_E_UDB_CORE_CONN_EXTRA
#include "udb/shell/shell_procfs_ct_extra.h"
#endif

#if TMCFG_E_UDB_CORE_IQOS_SUPPORT
#include "udb/shell/shell_procfs_qos.h"
#endif

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
#if TMCFG_E_UDB_CORE
static int app_patrol_proc_show(struct seq_file *m, void *v)
{
	return tdts_core_show_app_patrol(m, v);
}

static int app_patrol_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, app_patrol_proc_show, NULL);
}

static int udb_mem_proc_show(struct seq_file *m, void *v)
{
	return tdts_core_udb_mem_seq_print(m, v);
}

static int udb_mem_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, udb_mem_proc_show, NULL);
}

#if TMCFG_E_UDB_CORE_MEMTRACK
static int udb_memtrack_show(struct seq_file *m, void *v)
{
	return tdts_core_udb_memtrack_print(m, v);
}

static int udb_memtrack_open(struct inode *inode, struct file *file)
{
	return single_open(file, udb_memtrack_show, NULL);
}
#endif

static int udb_procmem_show(struct seq_file *m, void *v)
{
	return udb_read_procmem(m, v);
}

static int udb_procmem_open(struct inode *inode, struct file *file)
{
	return single_open(file, udb_procmem_show, NULL);
}

static int udb_ver_proc_show(struct seq_file *m, void *v)
{
	return udb_core_ver_seq_print(m, v);
}

static int udb_ver_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, udb_ver_proc_show, NULL);
}

#if TMCFG_E_UDB_CORE_WBL
static int udb_wbl_proc_show(struct seq_file *m, void *v)
{
	return tdts_core_udb_wbl_proc_read(m, v);
}

static int udb_wbl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, udb_wbl_proc_show, NULL);
}
#endif

#if TMCFG_E_UDB_CORE_APP_WBL
static int udb_app_wbl_proc_show(struct seq_file *m, void *v)
{
	return tdts_core_udb_app_wbl_proc_read(m, v);
}

static int udb_app_wbl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, udb_app_wbl_proc_show, NULL);
}
#endif

static int drop_cnt_proc_show(struct seq_file *m, void *v)
{
	return tdts_core_show_ford_drop(m, v);
}

static int drop_cnt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, drop_cnt_proc_show, NULL);
}

static int dpi_conf_proc_show(struct seq_file *m, void *v)
{
	return tdts_core_dpi_conf_seq_print(m, v);
}

static int dpi_conf_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dpi_conf_proc_show, NULL);
}

static int dpi_conf_proc_write(
	struct file *file, const char *buf, unsigned long count, void *data)
{
#define KBUF_SIZ 16
	char kbuf[KBUF_SIZ+1], *anchor = NULL;
	int cp_len = 0, val = 0, ret = 0;

	cp_len = min_t(unsigned long, (unsigned long)KBUF_SIZ, count);
	if (copy_from_user(kbuf, buf, cp_len))
	{
		return -EFAULT;
	}
	kbuf[cp_len] = 0;

	anchor = kbuf;
	while (' ' == *anchor)	anchor++;	/* skip leading space */

	val = simple_strtol(anchor, NULL, 16);
	DBG("cp_len=%d, kbuf=%s, val=%.8x\n", cp_len, kbuf, val);
	/* TODO: check validity of val */

	ret = udb_core_set_dpi_cfg(val);

	return (0 > ret) ? ret : count;

#if 0 // TODO: TMCFG_APP_K_TDTS_UDBFW_FAST_PATH
	fast_path_set_fwd_mode(!tdts_shell_get_dpi_cfg() ? FWD_MODE_FASTPATH : FWD_MODE_HOST);
#endif
}
#endif // TMCFG_E_UDB_CORE

#define PI_ETR(__n, __o, __r, __w) \
{ \
	.name = __n \
	, .fops.open = __o \
	, .fops.read = seq_read \
	, .fops.llseek = seq_lseek \
	, .fops.release = __r \
	, .fops.write = (fops_write_t)__w \
}

typedef ssize_t (*fops_write_t)(struct file *, const char __user *, size_t, loff_t *);
typedef int (*fops_open_t)(struct inode *, struct file *);
typedef int (*fops_release_t)(struct inode *, struct file *);

static struct
{
	char *name;
	struct file_operations fops;

} udb_shell_proc[] = {
#if TMCFG_E_UDB_CORE
	PI_ETR("bw_app_patrol", app_patrol_proc_open, single_release, NULL)
	, PI_ETR("bw_udb_mem", udb_mem_proc_open, single_release, NULL)
	, PI_ETR("bw_dpi_conf", dpi_conf_proc_open, single_release, dpi_conf_proc_write)
	, PI_ETR("bw_drop_cnt", drop_cnt_proc_open, single_release, NULL)	
#if TMCFG_E_UDB_CORE_IQOS_SUPPORT
	, PI_ETR("bw_qos_debug", qos_debug_proc_open, single_release, qos_debug_proc_write)
#endif
#if TMCFG_E_UDB_CORE_CONN_EXTRA
	, PI_ETR("bw_cte_stat", cte_stat_proc_open, single_release, NULL)
	, PI_ETR("bw_cte_dump", cte_dump_proc_open, single_release, NULL)
#endif
#if TMCFG_E_UDB_CORE_MEMTRACK
	, PI_ETR("bw_memtrack", udb_memtrack_open, single_release, NULL)
#endif
	, PI_ETR("bw_udb_stat", udb_procmem_open, single_release, NULL)
	, PI_ETR("bw_udb_ver", udb_ver_proc_open, single_release, NULL)
#if TMCFG_E_UDB_CORE_WBL
	, PI_ETR("bw_wbl_stat", udb_wbl_proc_open, single_release, NULL)
#endif
#if TMCFG_E_UDB_CORE_APP_WBL
	, PI_ETR("bw_app_wbl", udb_app_wbl_proc_open, single_release, NULL)
#endif
#endif // TMCFG_E_UDB_CORE
};

int udb_shell_procfs_init(void)
{
	int i = 0, size = 0;
	mode_t mode;

	size = sizeof(udb_shell_proc) / sizeof(*udb_shell_proc);

	for (i = 0; i < size; i++)
	{
		if (udb_shell_proc[i].fops.write)
		{
			mode = S_IRUGO | S_IWUGO;
		}
		else
		{
			mode = S_IRUGO;
		}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
		proc_create(udb_shell_proc[i].name, mode, NULL, &udb_shell_proc[i].fops);
#else
		struct proc_dir_entry *etr = NULL;
		etr = create_proc_entry(udb_shell_proc[i].name, mode, NULL);
		if (etr)
		{
			etr->proc_fops = &udb_shell_proc[i].fops;
		}
		else
		{
			printk("create_proc_entry() failed to create %s!\n", udb_shell_proc[i].name);
		}
#endif
	}

	return 0;
}

void udb_shell_procfs_cleanup(void)
{
	int i = 0, size = 0;

	size = sizeof(udb_shell_proc) / sizeof(*udb_shell_proc);

	for (i = size - 1; i >= 0; i--)
	{
		remove_proc_entry(udb_shell_proc[i].name, NULL);
	}
}

