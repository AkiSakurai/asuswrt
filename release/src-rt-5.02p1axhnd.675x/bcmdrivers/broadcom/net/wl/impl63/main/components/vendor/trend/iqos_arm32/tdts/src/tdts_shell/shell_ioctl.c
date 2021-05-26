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

#include "tdts/shell/shell_ioctl.h"

/* ioctl commands. */
#include "tdts/shell/shell_ioctl_dbg.h"
#include "tdts/shell/shell_ioctl_sig.h"
#include "tdts/shell/shell_ioctl_stat.h"

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

////////////////////////////////////////////////////////////////////////////////

/*!
 * \internal
 */
struct ioctl_entry
{
	uint8_t nr;
	tdts_shell_ioctl_cb_t cb;
};

#define INIT_IOCTL_ENTRY(_nr, _cb) {_nr, _cb}

/*!
 * \note This table must be manually kept sorted by ioctl nr. :D Yeah, manually.
 */
static struct ioctl_entry ioctl_entries [] =
{
	INIT_IOCTL_ENTRY(TDTS_SHELL_IOCTL_NR_DBG, tdts_shell_ioctl_dbg),
	INIT_IOCTL_ENTRY(TDTS_SHELL_IOCTL_NR_SIG, tdts_shell_ioctl_sig),
	INIT_IOCTL_ENTRY(TDTS_SHELL_IOCTL_NR_STAT, tdts_shell_ioctl_stat)
};

static tdts_shell_ioctl_cb_t search_ioctl_cb(uint8_t nr, uint8_t op)
{
	int st, ed, mid, tbl_size;

	/*
	 * Binary search to find callback function.
	 */

	tbl_size = sizeof(ioctl_entries) / sizeof(*ioctl_entries);

	if (unlikely(tbl_size <= 0))
	{
		return NULL;
	}

	st = 0;
	ed = tbl_size - 1;
	mid = (st + ed) / 2;

	while (st <= ed)
	{
		if (ioctl_entries[mid].nr < nr)
		{
			st = mid + 1;
		}
		else if (ioctl_entries[mid].nr == nr)
		{
			return ioctl_entries[mid].cb;
		}
		else
		{
			ed = mid - 1;
		}

		mid = (st + ed) / 2;
	}

	return NULL; // Not found
}


static int verify_ioctl_entries(void)
{
	int i, tbl_size;

	tbl_size = sizeof(ioctl_entries) / sizeof(*ioctl_entries);

	/* Make sure this table is sorted. */
	for (i = 0; i < tbl_size - 1; i++)
	{
		if (ioctl_entries[i].nr >= ioctl_entries[i + 1].nr)
		{
			ERR("ioctl pool is weird near index %d", i);
			return -1;
		}
	}

	return 0;
}


static int chrdev_ioctl_handle(tdts_shell_ioctl_t __user *ioc, uint8_t is_ioc_ro)
{
	tdts_shell_ioctl_cb_t cb;

	/* Find corresponding handler for incoming ioc, i.e. dispatch request to correct handler. */
	cb = search_ioctl_cb(ioc->nr, ioc->op);
	if (likely(cb != NULL))
	{
		return cb(ioc, is_ioc_ro);
	}
	else
	{
		ERR("Cannot find corresponding handler.");
		return -EINVAL;
	}

	return 0; // OK
}

static int chrdev_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int res;
	tdts_shell_ioctl_t __user ioc; // Copy from user!
	uint8_t is_ioc_ro = 1; // Identify if ioc is read-only, avoid copy_to_user.

	DBG("Get ioctl req with cmd=0x%08x", cmd);

	if (TDTS_SHELL_IOCTL_MAGIC != _IOC_TYPE(cmd))
	{
		DBG("Unexpected ioctl magic 0x%x", _IOC_TYPE(cmd));
		return -ENOTTY;
	}

	if (TDTS_SHELL_IOCTL_NR_NA == _IOC_NR(cmd))
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

static long chrdev_unlocked_ioctl(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	DBG("Recv unlocked ioctl request");
	return chrdev_ioctl(NULL, filp, cmd, arg);
}

static long chrdev_compat_ioctl(struct file *filp
	, unsigned int cmd, unsigned long arg)
{
	DBG("Recv compat ioctl request"); // 32 bit user program in 64 bit kernel.
	return chrdev_ioctl(NULL, filp, cmd, arg);
}

static int chrdev_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int chrdev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations chrdev_fops =
{
	owner:      THIS_MODULE,
	unlocked_ioctl: chrdev_unlocked_ioctl,
#if HAVE_COMPAT_IOCTL // <linux/fs.h>
	compat_ioctl: chrdev_compat_ioctl,
#endif
	open:       chrdev_open,
	release:    chrdev_release,
};

/*!
 * \brief Initialize ioctl device
 *
 * \return 0 if ok
 * \return < 0, otherwise
 *
 * \sa tdts_shell_ioctl_exit
 */
int tdts_shell_ioctl_init(void)
{
	int res;

	if (verify_ioctl_entries() < 0)
	{
		return -EINVAL;
	}

	PRT("Init chrdev /dev/%s with major %d", TDTS_SHELL_IOCTL_CHRDEV_NAME, TDTS_SHELL_IOCTL_CHRDEV_MAJOR);

	res = register_chrdev(TDTS_SHELL_IOCTL_CHRDEV_MAJOR, TDTS_SHELL_IOCTL_CHRDEV_NAME, &chrdev_fops);
	if (res < 0)
	{
		ERR("Cannot register chrdev %d\n",
			TDTS_SHELL_IOCTL_CHRDEV_MAJOR);
		return -1;
	}

	return 0;
}

/*!
 * \brief Exit ioctl device.
 *
 * \sa tdts_shell_ioctl_init
 */
void tdts_shell_ioctl_cleanup(void)
{
	PRT("Exit chrdev /dev/%s with major %d", TDTS_SHELL_IOCTL_CHRDEV_NAME, TDTS_SHELL_IOCTL_CHRDEV_MAJOR);

	unregister_chrdev(TDTS_SHELL_IOCTL_CHRDEV_MAJOR, TDTS_SHELL_IOCTL_CHRDEV_NAME);
}

////////////////////////////////////////////////////////////////////////////////

/*!
 * \brief Copy data to output buffer in ioctl structure. (copy_to_user)
 *
 * \param ioc A pointer to ioctl structure
 * \param buf Data (kernel space) to copy to user space
 * \param buf_len Length of data
 *
 * \return 0 if ok
 * \return != 0, otherwise
 */
unsigned long tdts_shell_ioctl_copy_out(tdts_shell_ioctl_t *ioc, void *buf, uint32_t buf_len)
{
	unsigned long res;

	assert(ioc != NULL);

	if (likely(buf != NULL && buf_len > 0))
	{
		res = copy_to_user((void *)ioc->out, buf, buf_len);
		if (res != 0)
		{
			ERR("Cannot copy to user at %p %u/%u bytes", ioc->out, buf_len, ioc->out_len);
			return res;
		}
	}

	if (likely((uint32_t *)ioc->out_used_len != NULL))
	{
		res = copy_to_user((uint32_t *)ioc->out_used_len, &buf_len, sizeof(buf_len));
		if (res != 0)
		{
			return res;
		}
	}

	return 0; // OK
}
