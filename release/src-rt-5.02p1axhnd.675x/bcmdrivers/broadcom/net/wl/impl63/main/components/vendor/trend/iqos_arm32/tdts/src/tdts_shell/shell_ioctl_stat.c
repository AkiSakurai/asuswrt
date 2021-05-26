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

#include <linux/vmalloc.h>

#include "tdts/shell/shell_ioctl_stat.h"
#include "tdts/core/core.h"
#include "tdts/shell/shell.h"

#define CONFIG_DEBUG (1) //!< Say 1 to debug

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

static int tdts_shell_ioctl_stat_op_get_spec(tdts_shell_ioctl_t *ioc)
{

	return 0;
}

static int tdts_shell_ioctl_stat_op_get_matched_rule(tdts_shell_ioctl_t *ioc)
{
	int ret = 0;
	void *buf = NULL;
	uint32_t buf_used_len = 0;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len <= 0 || (uint32_t *)ioc->out_used_len == NULL))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		ret = -EINVAL;
		goto __error;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}

	ret = tdts_core_get_matched_rule_info(buf, ioc->out_len, &buf_used_len);
	if (unlikely(ret != 0))
	{
		ERR("tdts_core_get_matched_rule_info() fail!\n");
		goto __error;
	}

	ret = tdts_shell_ioctl_copy_out(ioc, buf, buf_used_len);
	if (unlikely(ret != 0))
	{
		ERR("copy to user fail!");
		ret = -EFAULT;
	}
__error:
	if (NULL != buf)
	{
		vfree(buf);
	}
	return ret;
}

static int tdts_shell_ioctl_stat_op_get_engine_status(tdts_shell_ioctl_t *ioc)
{
	int ret = 0;
	void *buf = NULL;
	uint32_t buf_used_len = 0;
	tdts_shell_eng_status_t *eng_status_ptr;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len < sizeof(*eng_status_ptr) || (uint32_t *)ioc->out_used_len == NULL))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		ret = -EINVAL;
		goto __error;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}

	eng_status_ptr = (tdts_shell_eng_status_t *)buf;

	eng_status_ptr->eng_ver.major = TDTS_ENGINE_VER_MAJ;
	eng_status_ptr->eng_ver.middle = TDTS_ENGINE_VER_MID;
	eng_status_ptr->eng_ver.minor = TDTS_ENGINE_VER_MIN;

	ret = tdts_core_get_sig_ver(&(eng_status_ptr->sig_ver));
	buf_used_len += sizeof(*eng_status_ptr);
	if (unlikely(ret != 0))
	{
		ERR("tdts_core_get_sig_ver() fail!\n");
		goto __error;
	}

	DBG("Engine ver. = %d.%d.%d, Signature ver. = %d.%d", eng_status_ptr->eng_ver.major,
			eng_status_ptr->eng_ver.middle, eng_status_ptr->eng_ver.minor,
			eng_status_ptr->sig_ver.major, eng_status_ptr->sig_ver.minor);

	ret = tdts_shell_ioctl_copy_out(ioc, buf, buf_used_len);
	if (unlikely(ret != 0))
	{
		ERR("copy to user fail!");
		ret = -EFAULT;
	}
__error:
	if (NULL != buf)
	{
		vfree(buf);
	}
	return ret;
}

static int tdts_shell_ioctl_stat_op_get_tcp_conn_num(tdts_shell_ioctl_t *ioc)
{
	int ret = 0;
	void *buf = NULL;
	uint32_t buf_used_len = 0;
	uint32_t *conn_num_ptr;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len < sizeof(uint32_t) || (uint32_t *)ioc->out_used_len == NULL))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		ret = -EINVAL;
		goto __error;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	//-------------------------------------------------------------------------
	conn_num_ptr = (uint32_t *)buf;

	ret = tdts_core_get_tcp_conn_num(conn_num_ptr);
	buf_used_len += sizeof(uint32_t);
	if (unlikely(ret != 0))
	{
		ERR("tdts_core_get_tcp_conn_num() fail!\n");
		goto __error;
	}
	DBG("conn_num = %u\n",*conn_num_ptr);

	//-------------------------------------------------------------------------
	ret = tdts_shell_ioctl_copy_out(ioc, buf, buf_used_len);
	if (unlikely(ret != 0))
	{
		ERR("copy to user fail!");
		ret = -EFAULT;
	}
__error:
	if (NULL != buf)
	{
		vfree(buf);
	}
	return ret;
}


static int tdts_shell_ioctl_stat_op_get_rule_mem_usage(tdts_shell_ioctl_t *ioc)
{
	int ret = 0;
	void *buf = NULL;
	uint32_t buf_used_len = 0;
	unsigned long *mem_usage_ptr;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len < sizeof(*mem_usage_ptr) || (uint32_t *)ioc->out_used_len == NULL))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		ret = -EINVAL;
		goto __error;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	//-------------------------------------------------------------------------
	mem_usage_ptr = (unsigned long *)buf;

	ret = tdts_core_get_rule_mem_usage(mem_usage_ptr);
	buf_used_len += sizeof(uint32_t);
	if (unlikely(ret != 0))
	{
		ERR("tdts_core_get_rule_mem_usage() fail!\n");
		goto __error;
	}
	DBG("mem_usage = %u\n",*mem_usage_ptr);

	//-------------------------------------------------------------------------
	ret = tdts_shell_ioctl_copy_out(ioc, buf, buf_used_len);
	if (unlikely(ret != 0))
	{
		ERR("copy to user fail!");
		ret = -EFAULT;
	}
__error:
	if (NULL != buf)
	{
		vfree(buf);
	}
	return ret;
}



int tdts_shell_ioctl_stat(tdts_shell_ioctl_t *ioc, uint8_t is_ioc_ro)
{
	DBG("Recv ioctl req with op %x", ioc->op);

	switch (ioc->op)
	{
	case TDTS_SHELL_IOCTL_STAT_OP_GET_SPEC:
		return tdts_shell_ioctl_stat_op_get_spec(ioc);
	case TDTS_SHELL_IOCTL_STAT_OP_GET_MATCHED_RULE:
		return tdts_shell_ioctl_stat_op_get_matched_rule(ioc);
	case TDTS_SHELL_IOCTL_STAT_OP_GET_ENG_STATUS:
		return tdts_shell_ioctl_stat_op_get_engine_status(ioc);
	case TDTS_SHELL_IOCTL_STAT_OP_GET_TCP_CONN_NUM:
		return tdts_shell_ioctl_stat_op_get_tcp_conn_num(ioc);
	case TDTS_SHELL_IOCTL_STAT_OP_GET_RULE_MEM_USAGE:		
		return tdts_shell_ioctl_stat_op_get_rule_mem_usage(ioc);
	default:
		ERR("Undefined op %u", ioc->op);
		return -EINVAL;
	}

	return 0;
}
