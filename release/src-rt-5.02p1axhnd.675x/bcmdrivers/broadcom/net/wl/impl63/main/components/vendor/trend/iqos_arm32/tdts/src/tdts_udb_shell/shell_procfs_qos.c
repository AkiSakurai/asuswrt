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

//#define DBG(fmt, args...) printk(KERN_DEBUG "[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#define DBG(fmt, args...) do { } while (0)

int qos_debug_proc_show(struct seq_file *m, void *v)
{
	int val = -1;
	udb_core_qos_get_dbg_level(&val);
	seq_printf(m, "%d\n", val);
	return 0;
}

int qos_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, qos_debug_proc_show, NULL);
}

int qos_debug_proc_write(
	struct file *file, const char *buf, unsigned long count, void *data)
{
#define KBUF_SIZ 16
	char kbuf[KBUF_SIZ+1], *anchor;
	int cp_len, val;

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

	udb_core_qos_set_dbg_level(val);

	return count;
}

