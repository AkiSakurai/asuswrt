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
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/ioctl.h>

#include <asm/uaccess.h>

#include "udb/shell/shell_ioctl.h"

////////////////////////////////////////////////////////////////////////////////
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

#ifndef uintptr_t
#define uintptr_t unsigned long
#endif
////////////////////////////////////////////////////////////////////////////////

static int udb_shell_ioctl_copy_out(
	udb_shell_ioctl_t *ioc, void *buf, uint32_t buf_len)
{
	unsigned long res = 0;
	void *out = NULL;
	uint32_t *out_used_len = 0;
	uint32_t buf_used_len = 0;

	assert(ioc != NULL);

	out = (void *)(uintptr_t)ioc->out;
	out_used_len = (uint32_t *)(uintptr_t)ioc->out_used_len;

	res = udb_ioctl_copy_out(ioc->nr, ioc->op, buf, buf_len, &buf_used_len);
	if (res)
	{
		return res;
	}

	assert(buf_used_len <= buf_len);

	if (likely(buf != NULL && buf_used_len > 0 && out != NULL))
	{
		res = copy_to_user(out, buf, buf_used_len);
		if (res != 0)
		{
			ERR("Cannot copy to user at %p %u/%u bytes",
				out, buf_used_len, ioc->out_len);
			return res;
		}
	}

	if (likely(out_used_len != NULL))
	{
		res = copy_to_user(out_used_len, &buf_used_len, sizeof(buf_used_len));
		if (res != 0)
		{
			return res;
		}
	}

	return 0; // OK
}

static int udb_shell_ioctl_copy_in(
	udb_shell_ioctl_t *ioc, void *buf, uint32_t buf_len)
{
	unsigned long res = 0;
	void *in_raw = NULL;

	assert(ioc != NULL);

	in_raw = (void *)(uintptr_t)ioc->in_raw;

	if (likely(buf != NULL && buf_len > 0 && in_raw != NULL))
	{
		res = copy_from_user(buf, in_raw, buf_len);
		if (res != 0)
		{
			ERR("Cannot copy from user at %p %u/%u bytes",
				in_raw, buf_len, ioc->in_len);
			return res;
		}

		return udb_ioctl_copy_in(ioc->nr, ioc->op, buf, buf_len);
	}

	return 0; // OK
}

static int udb_shell_ioctl_copy_none(udb_shell_ioctl_t *ioc)
{
	assert(ioc != NULL);

	return udb_ioctl_copy_none(ioc->nr, ioc->op);
}

static int chrdev_ioctl_handle(udb_shell_ioctl_t __user *ioc, uint8_t is_ioc_ro)
{
	int ret = 0;
	void *buf = NULL;

	if (ioc->in_raw && ioc->in_len)
	{
		MALLOC_IOC_IN(ioc, buf);

		ret = udb_shell_ioctl_copy_in(ioc, buf, ioc->in_len);
		if (unlikely(0 != ret))
		{
			DBG("udb_shell_ioctl_copy_in() fail!");
		}

		VFREE_INIT(buf, ioc->in_len);
	}
	else if (ioc->out && ioc->out_len)
	{
		MALLOC_IOC_OUT(ioc, buf);

		ret = udb_shell_ioctl_copy_out(ioc, buf, ioc->out_len);
		if (unlikely(0 != ret))
		{
			DBG("udb_shell_ioctl_copy_out() fail!");
		}

		VFREE_INIT(buf, ioc->out_len);
	}
	else
	{
		ret = udb_shell_ioctl_copy_none(ioc);
		if (unlikely(0 != ret))
		{
			DBG("udb_shell_ioctl_copy_none() fail!");
		}
	}

	return ret;
}

static int chrdev_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int res = 0;
	udb_shell_ioctl_t __user ioc; // Copy from user!
	uint8_t is_ioc_ro = 1; // Identify if ioc is read-only, avoid copy_to_user.

	DBG("Get ioctl req with cmd=0x%08x", cmd);

	if (UDB_SHELL_IOCTL_MAGIC != _IOC_TYPE(cmd))
	{
		DBG("Unexpected ioctl magic 0x%x", _IOC_TYPE(cmd));
		return -ENOTTY;
	}

	if (UDB_IOCTL_NR_NA == _IOC_NR(cmd))
	{
		DBG("Invalid ioctl nr 0");
		return -EINVAL;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		res = access_ok(VERIFY_WRITE, (void *) arg, _IOC_SIZE(cmd)); // return 0 if not ok.
		if (!res)
		{
			DBG("Cannot access arg res=%d, arg=0x%x, sz=%d\n",
				res, (unsigned int) arg, _IOC_SIZE(cmd));
			return -EFAULT;
		}

		is_ioc_ro = 0;
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		res = access_ok(VERIFY_READ, (void *) arg, _IOC_SIZE(cmd)); // return 0 if not ok.
		if (!res)
		{
			DBG("Cannot access arg res=%d, arg=0x%x, sz=%d\n",
				res, (unsigned int) arg, _IOC_SIZE(cmd));
			return -EFAULT;
		}
	}

	if (unlikely((res = copy_from_user(&ioc, (void *) arg, _IOC_SIZE(cmd)))))
	{
		DBG("Cannot copy from user with code %d\n", res);
		return -EFAULT;
	}

	DBG("ioc magic=0x%x nr=0x%x op=0x%x (in=%p %u bytes, out=%p %u bytes, out_used_len=%p)",
		ioc.magic, ioc.nr, ioc.op,
		(void *)ioc.in_raw, ioc.in_len,
		(void *)ioc.out, ioc.out_len, (uint32_t *)ioc.out_used_len);

	return chrdev_ioctl_handle(&ioc, is_ioc_ro);
}

static long my_chrdev_unlocked_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	DBG("Recv unlocked ioctl request");
	return chrdev_ioctl(NULL, filp, cmd, arg);
}

static long my_chrdev_compat_ioctl(struct file *filp
	, unsigned int cmd, unsigned long arg)
{
	DBG("Recv compat ioctl request"); // 32 bit user program in 64 bit kernel.
	return chrdev_ioctl(NULL, filp, cmd, arg);
}

static int my_chrdev_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int my_chrdev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations chrdev_fops =
{
	owner:      THIS_MODULE,
	unlocked_ioctl: my_chrdev_unlocked_ioctl,
#if HAVE_COMPAT_IOCTL // <linux/fs.h>
	compat_ioctl: my_chrdev_compat_ioctl,
#endif
	open:       my_chrdev_open,
	release:    my_chrdev_release,
};

/*!
 * \brief Initialize ioctl device
 *
 * \return 0 if ok
 * \return < 0, otherwise
 *
 * \sa tdts_shell_ioctl_exit
 */
int udb_shell_ioctl_init(void)
{
	int res = 0;

	PRT("Init chrdev /dev/%s with major %d", UDB_SHELL_IOCTL_CHRDEV_NAME, UDB_SHELL_IOCTL_CHRDEV_MAJOR);

	res = register_chrdev(UDB_SHELL_IOCTL_CHRDEV_MAJOR, UDB_SHELL_IOCTL_CHRDEV_NAME, &chrdev_fops);
	if (res < 0)
	{
		ERR("Cannot register chrdev %d\n",
			UDB_SHELL_IOCTL_CHRDEV_MAJOR);
		return -1;
	}

	return 0;
}

/*!
 * \brief Exit ioctl device.
 *
 * \sa tdts_shell_ioctl_init
 */
void udb_shell_ioctl_cleanup(void)
{
	PRT("Exit chrdev /dev/%s with major %d", UDB_SHELL_IOCTL_CHRDEV_NAME, UDB_SHELL_IOCTL_CHRDEV_MAJOR);

	unregister_chrdev(UDB_SHELL_IOCTL_CHRDEV_MAJOR, UDB_SHELL_IOCTL_CHRDEV_NAME);
}

