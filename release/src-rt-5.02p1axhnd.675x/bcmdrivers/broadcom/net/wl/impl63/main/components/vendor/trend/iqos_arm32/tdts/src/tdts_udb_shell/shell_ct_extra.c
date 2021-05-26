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
#include <linux/version.h>
#include <linux/types.h>
#include <linux/netfilter.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <net/netfilter/nf_conntrack.h>
#else
#include <linux/netfilter_ipv4/ip_conntrack.h>
#endif

#include "tdts_udb.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
/* Mapping Kernel 2.6 Netfilter API to Kernel 2.4 */
#define ip_conntrack        nf_conn
#define ip_conntrack_get    nf_ct_get
#define ip_conntrack_tuple  nf_conntrack_tuple
#endif

static unsigned int sess_num = 0;
module_param(sess_num, uint, S_IRUGO);

static unsigned int sess_timeout = 0;
module_param(sess_timeout, uint, S_IRUGO);

void udb_shell_ct_event_handler(void *conntrack, uint32_t mark, int ct_evt)
{
	udb_core_ct_event_handler(conntrack, mark, ct_evt);
}
EXPORT_SYMBOL(udb_shell_ct_event_handler);

void udb_shell_reg_func_find_ct(int (*func)(void **, uint32_t *, skb_tuples_t *, uint64_t))
{
	udb_core_reg_func_find_ct(func);
}
EXPORT_SYMBOL(udb_shell_reg_func_find_ct);

int udb_shell_ct_extra_init(void)
{
	cte_init_param_t param;

	if (0 >= sess_num)
	{
		printk(KERN_EMERG "module param, sess_num, is NULL or zero!\n");
		return -EINVAL;
	}

	param.sess_num = sess_num;
	param.sess_timeout = sess_timeout;

	return udb_core_ct_extra_init(&param);
}

void udb_shell_ct_extra_exit(void)
{
	udb_core_ct_extra_exit();
}

