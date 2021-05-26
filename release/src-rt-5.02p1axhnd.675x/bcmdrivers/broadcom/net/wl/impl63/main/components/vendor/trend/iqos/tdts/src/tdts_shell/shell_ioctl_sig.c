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

#include "tdts/shell/shell_ioctl_sig.h"
#include "tdts/core/core.h"
#include "tdts/shell/shell.h"

#define CONFIG_DEBUG (0) //!< Say 1 to debug

#define ERR(fmt, args...) printk(KERN_ERR " *** ERROR: [%s:%d] " fmt "\n", __func__, __LINE__, ##args)
#define PRT(fmt, args...) printk(KERN_INFO fmt "\n", ##args)

#if CONFIG_DEBUG
#define DBG(fmt, args...) printk(KERN_DEBUG "[%s:%d] " fmt "\n", __func__, __LINE__, ##args)
#define assert(_condition) \
    do \
    { \
        if (!(_condition)) \
        { \
            printk(KERN_ERR "\n" "Assertion failed at %s:%d\n", __func__, __LINE__); \
            BUG(); \
        } \
    } while(0)
#else
#define DBG(fmt, args...) do { } while (0)
#define assert(_condition) do { } while (0)
#endif

static int tdts_shell_ioctl_sig_op_load(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;

	if (TDTS_SHELL_IOCTL_TYPE_RAW != ioc->in_type)
	{
		ERR("Invalid ioctl in_type: %d", ioc->in_type);
		return -EINVAL;
	}

	if (ioc->in_len <= 0)
	{
		ERR("Invalid ioctl in_len: %d", ioc->in_len);
		return -EINVAL;
	}

	if (NULL == (void *)ioc->in_raw)
	{
		ERR("Invalid ioctl in_raw: NULL");
		return -EINVAL;
	}

	buf = vmalloc(ioc->in_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->in_len);
		return -ENOMEM;
	}

	if (copy_from_user(buf, (void *)ioc->in_raw, ioc->in_len) != 0)
	{
		ERR("Cannot copy table from user %u bytes", ioc->in_len);

		vfree(buf);
		return -EFAULT;
	}

	ret = tdts_core_rule_parsing_trf_load(buf, ioc->in_len);

	if (ret)
	{
		ERR("tdts_core_rule_parsing_trf_load() fail!\n");
	}


	vfree(buf);

	return ret;
}

static int tdts_shell_ioctl_sig_op_unload(void)
{
	tdts_core_rule_parsing_trf_unload();

	return 0;
}

static int tdts_shell_ioctl_sig_op_get_sig_ver(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len != sizeof(tdts_core_sig_ver_t)
											|| (uint32_t *)ioc->out_used_len == NULL))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		return -EINVAL;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	ret = tdts_core_get_sig_ver(buf);
	buf_used_len = ioc->out_len;

	if (ret)
	{
		ERR("tdts_core_get_sig_ver() fail!\n");
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

static int tdts_shell_ioctl_sig_op_get_sig_num(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t ptn_num = 0; // no used
	uint32_t buf_used_len;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len != sizeof(uint32_t)))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		return -EINVAL;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	ret = tdts_core_get_rule_and_ptn_num(buf, &ptn_num);
	buf_used_len = ioc->out_len;

	if (ret)
	{
		ERR("tdts_core_get_rule_and_ptn_num() fail!\n");
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

static int tdts_shell_ioctl_sig_get_ano_sec_tbl_len(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len != sizeof(uint32_t)))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		return -EINVAL;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	ret = tdts_core_rule_parsing_get_ptn_data_len(buf);
	buf_used_len = ioc->out_len;

	if (ret)
	{
		ERR("tdts_core_rule_parsing_get_ptn_data_len() fail!\n");
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

static int tdts_shell_ioctl_sig_op_ano_sec_tbl_copy(tdts_shell_ioctl_t *ioc)
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

	ret = tdts_core_rule_parsing_ptn_data_copy(buf, ioc->out_len, &buf_used_len);
	if (unlikely(ret != 0))
	{
		ERR("tdts_core_rule_parsing_ptn_data_copy() fail!\n");
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

static int tdts_shell_ioctl_sig_op_data_free(void)
{
	tdts_core_rule_parsing_ptn_data_free();

	return 0;
}

static int tdts_shell_ioctl_sig_op_set_state(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;

	if (TDTS_SHELL_IOCTL_TYPE_U32 != ioc->in_type)
	{
		ERR("Invalid ioctl in_type: %d", ioc->in_type);
		return -EINVAL;
	}

	if (sizeof(((tdts_shell_ioctl_t *) 0)->in_u32) != ioc->in_len)
	{
		ERR("Invalid ioctl in_len: %d", ioc->in_len);
		return -EINVAL;
	}

	ret = tdts_core_system_setting_state_set(ioc->in_u32);

	if (ret)
	{
		ERR("tdts_core_system_setting_state_set() fail!\n");
	}

	return ret;
}

static int tdts_shell_ioctl_sig_op_get_state(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len != sizeof(uint32_t)))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		return -EINVAL;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	ret = tdts_core_system_setting_state_get(buf);
	buf_used_len = ioc->out_len;

	if (ret)
	{
		ERR("tdts_core_system_setting_state_get() fail!\n");
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

static int tdts_shell_ioctl_sig_op_devid_data_len_get(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len != sizeof(uint32_t)))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		return -EINVAL;
	}

	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	ret = tdts_core_devid__data_len_get(buf);
	buf_used_len = ioc->out_len;

	if (ret)
	{
		ERR("tdts_core_devid__data_len_get() fail!\n");
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

static int tdts_shell_ioctl_sig_op_devid_data_copy(tdts_shell_ioctl_t *ioc)
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

	ret = tdts_core_devid_data_copy(buf, ioc->out_len, &buf_used_len);
	if (unlikely(ret != 0))
	{
		ERR("tdts_core_rule_parsing_ptn_data_copy() fail!\n");
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

static int tdts_shell_ioctl_sig_op_appid_get_nr_cat_name(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

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

	ret = tdts_core_appid_get_nr_cat_name(buf, ioc->out_len, &buf_used_len);

	if (ret)
	{
		DBG("tdts_core_appid_get_all_cat_name() fail!\n");
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

static int tdts_shell_ioctl_sig_op_appid_cat_name_copy(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

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

	ret = tdts_core_appid_get_all_cat_name(buf, ioc->out_len, &buf_used_len);

	if (ret)
	{
		DBG("tdts_core_appid_get_all_cat_name() fail!\n");
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

static int tdts_shell_ioctl_sig_op_appid_get_nr_beh_name(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

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

	ret = tdts_core_appid_get_nr_beh_name(buf, ioc->out_len, &buf_used_len);
	if (ret)
	{
		ERR("tdts_core_appid_get_all_beh_name() fail!\n");
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

static int tdts_shell_ioctl_sig_op_appid_beh_name_copy(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

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

	ret = tdts_core_appid_get_all_beh_name(buf, ioc->out_len, &buf_used_len);

	if (ret)
	{
		ERR("tdts_core_appid_get_all_beh_name() fail!\n");
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

static int tdts_shell_ioctl_sig_op_appid_get_nr_app_name(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

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

	ret = tdts_core_appid_get_nr_app_name(buf, ioc->out_len, &buf_used_len);

	if (ret)
	{
		DBG("tdts_core_appid_get_all_app_name() fail!\n");
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

static int tdts_shell_ioctl_sig_op_appid_app_name_copy(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

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

	ret = tdts_core_appid_get_all_app_name(buf, ioc->out_len, &buf_used_len);

	if (ret)
	{
		DBG("tdts_core_appid_get_all_app_name() fail!\n");
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

static int tdts_shell_ioctl_sig_op_get_nr_app_id(tdts_shell_ioctl_t *ioc)
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

	ret = tdts_core_appid_get_nr_appid(buf, ioc->out_len, &buf_used_len);

	if (unlikely(ret != 0))
	{
		DBG("tdts_core_appid_get_all_appid() fail!\n");
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

static int tdts_shell_ioctl_sig_op_get_app_db(tdts_shell_ioctl_t *ioc)
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

	ret = tdts_core_appid_get_all_appid(buf, ioc->out_len, &buf_used_len);

	if (unlikely(ret != 0))
	{
		DBG("tdts_core_appid_get_all_appid() fail!\n");
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

static int tdts_shell_ioctl_sig_op_get_bndwth_num(tdts_shell_ioctl_t *ioc)
{
	int ret = -1;
	void *buf = NULL;
	uint32_t buf_used_len;

	if (unlikely((void *)ioc->out == NULL || ioc->out_len != sizeof(unsigned int)
											|| (uint32_t *)ioc->out_used_len == NULL))
	{
		ERR("Invalid output argument: %p %u bytes", (void *)ioc->out, ioc->out_len);
		return -EINVAL;
	}

	/* */
	buf = vmalloc(ioc->out_len);
	if (unlikely(buf == NULL))
	{
		ERR("Cannot malloc container %u bytes", ioc->out_len);
		ret = -ENOMEM;
		goto __error;
	}
	ret = tdts_core_get_bndwth_num(buf);
	buf_used_len = ioc->out_len;

	if (ret)
	{
		ERR("tdts_core_get_bndwth_num() fail!\n");
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

static int tdts_shell_ioctl_sig_op_get_bndwth_db(tdts_shell_ioctl_t *ioc)
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

	ret = tdts_core_get_bndwth_ent(buf, ioc->out_len, &buf_used_len);

	if (unlikely(ret != 0))
	{
		DBG("tdts_core_appid_get_bndwth_ent() fail!\n");
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

int tdts_shell_ioctl_sig(tdts_shell_ioctl_t *ioc, uint8_t is_ioc_ro)
{
	DBG("Recv ioctl req with op %x", ioc->op);

	switch (ioc->op)
	{
	case TDTS_SHELL_IOCTL_SIG_OP_LOAD:
		return tdts_shell_ioctl_sig_op_load(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_UNLOAD:
		return tdts_shell_ioctl_sig_op_unload();
	case TDTS_SHELL_IOCTL_SIG_OP_GET_SIG_VER:
		return tdts_shell_ioctl_sig_op_get_sig_ver(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_SIG_NUM:
		return tdts_shell_ioctl_sig_op_get_sig_num(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_ANO_SEC_TBL_LEN:
		return tdts_shell_ioctl_sig_get_ano_sec_tbl_len(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_ANO_SEC_TBL:
		return tdts_shell_ioctl_sig_op_ano_sec_tbl_copy(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_FREE_SHARED_INFO_DATA:
		return tdts_shell_ioctl_sig_op_data_free();
	case TDTS_SHELL_IOCTL_SIG_OP_SET_STATE:
		return tdts_shell_ioctl_sig_op_set_state(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_STATE:
		return tdts_shell_ioctl_sig_op_get_state(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_DEVID_DATA_LEN:
		return tdts_shell_ioctl_sig_op_devid_data_len_get(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_DEVID_DATA:
		return tdts_shell_ioctl_sig_op_devid_data_copy(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_NR_CAT_NAME:
		return tdts_shell_ioctl_sig_op_appid_get_nr_cat_name(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_CAT_NAME:
		return tdts_shell_ioctl_sig_op_appid_cat_name_copy(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_NR_BEH_NAME:
		return tdts_shell_ioctl_sig_op_appid_get_nr_beh_name(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_BEH_NAME:
		return tdts_shell_ioctl_sig_op_appid_beh_name_copy(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_NR_APP_NAME:
		return tdts_shell_ioctl_sig_op_appid_get_nr_app_name(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_APP_NAME:
		return tdts_shell_ioctl_sig_op_appid_app_name_copy(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_NR_APP_ID:
		return tdts_shell_ioctl_sig_op_get_nr_app_id(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_APP_DB:
		return tdts_shell_ioctl_sig_op_get_app_db(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_BNDWTH_NUM:
		return tdts_shell_ioctl_sig_op_get_bndwth_num(ioc);
	case TDTS_SHELL_IOCTL_SIG_OP_GET_BNDWTH_DB:
		return tdts_shell_ioctl_sig_op_get_bndwth_db(ioc);
	default:
		ERR("Undefined op %u", ioc->op);
		return -EINVAL;
	}

	return 0;
}
