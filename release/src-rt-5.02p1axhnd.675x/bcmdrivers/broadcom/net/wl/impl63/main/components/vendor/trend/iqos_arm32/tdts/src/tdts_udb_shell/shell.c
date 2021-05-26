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
#include <linux/netdevice.h>
#include <linux/slab.h> // kmalloc
#include <linux/vmalloc.h> // vmalloc
#include <linux/spinlock.h>

#include <asm/atomic.h>
#include <linux/time.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/if_ether.h>

#include <linux/netfilter.h>
//#include <linux/netfilter_bridge.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>

#include <net/netfilter/nf_conntrack.h>

#include <linux/bottom_half.h> //local_bh_disable();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_nat_helper.h>

#ifdef CONFIG_COMPAT
#include <asm/compat.h>
#endif

/* Mapping Kernel 2.6 Netfilter API to Kernel 2.4 */
#define ip_conntrack        nf_conn
#define ip_conntrack_get    nf_ct_get
#define ip_conntrack_tuple  nf_conntrack_tuple

#else // older kernel
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_conntrack_tuple.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#endif

#include "udb/tdts_udb_shell.h"
#include "udb/shell/shell_ioctl.h"
#if TMCFG_E_UDB_SHELL_PROCFS
#include "udb/shell/shell_procfs.h"
#endif

////////////////////////////////////////////////////////////////////////////////
#define CONFIG_DEBUG (0) //!< Say 1 to debug
#define THIS_MOD_NAME "tdts_udb"

#define ERR(fmt, args...) printk(KERN_ERR " *** ERROR: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#define PRT(fmt, args...) printk(KERN_INFO THIS_MOD_NAME ": " fmt "\n", ##args)

#if CONFIG_DEBUG
#define DBG(fmt, args...) printk(KERN_DEBUG "[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#define assert(_condition) \
    do \
    { \
        if (!(_condition)) \
        { \
            printk(KERN_ERR "\n" FWMOD_NAME  ": Assertion failed at %s:%d\n", __FUNCTION__, __LINE__); \
            BUG(); \
        } \
    } while(0)
#else
#define DBG(fmt, args...) do { } while (0)
#define assert(_condition) do { } while (0)
#endif

#define MEM_DBG(fmt, args...) printk(KERN_DEBUG "[%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)

////////////////////////////////////////////////////////////////////////////////

static int shell_vprintf(const char *format, va_list ap)
{
	return vprintk(format, ap);
}

static int shell_vsprintf(char *str, const char *format, va_list ap)
{
	return vsprintf(str, format, ap);
}

static int shell_vsnprintf(char *str, unsigned long size, const char *format, va_list ap)
{
	return vsnprintf(str, size, format, ap);
}

////////////////////////////////////////////////////////////////////////////////
static void *shell_vmalloc(unsigned long size)
{
	return vmalloc(size);
}

static void shell_vfree(const void *addr)
{
	return vfree((void *) addr);
}

static void *shell_kmalloc_atomic(unsigned long size)
{
	return kmalloc(size, GFP_ATOMIC);
}

static void *shell_kmalloc_sleep(unsigned long size)
{
	return kmalloc(size, GFP_KERNEL);
}

static void shell_kfree(const void *x)
{
	return kfree((void *) x);
}

///////////////////////////////////////////////////////////////////////////////
static void *shell_alloc_skb_atomic(unsigned long size)
{
	return alloc_skb(size, GFP_ATOMIC);
}

static void *shell_skb_clone_atomic(const void *skb_ptr)
{
	return skb_clone((void *) skb_ptr, GFP_ATOMIC);
}

static void shell_kfree_skb(const void *x)
{
	kfree_skb((void *) x);
} 
////////////////////////////////////////////////////////////////////////////////
static void shell_spin_lock_bh(void *lck)
{
	spin_lock_bh((spinlock_t *) lck);
}

static void shell_spin_unlock_bh(void *lck)
{
	spin_unlock_bh((spinlock_t *) lck);
}

static void shell_spin_lock(void *lck)
{
	spin_lock((spinlock_t *) lck);
}

static void shell_spin_unlock(void *lck)
{
	spin_unlock((spinlock_t *) lck);
}

static void *shell_spin_lock_alloc(void)
{
	spin_lock_t *lck;

	lck = shell_kmalloc_sleep(sizeof(spinlock_t));

	return lck;
}

static void shell_spin_lock_free(void *lck)
{
	shell_kfree(lck);
}

static int shell_spin_lock_init(void *lck)
{
	spin_lock_init(((spinlock_t *) lck));
	return 0;
}

static int shell_spin_is_locked(void *lck)
{
	return spin_is_locked((spinlock_t *) lck);
}

////////////////////////////////////////////////////////////////////////////////

static void shell_read_lock_bh(void *lck)
{
	read_lock_bh((rwlock_t *) lck);
}

static void shell_read_unlock_bh(void *lck)
{
	read_unlock_bh((rwlock_t *) lck);
}

static void shell_read_lock(void *lck)
{
	read_lock((rwlock_t *) lck);
}

static void shell_read_unlock(void *lck)
{
	read_unlock((rwlock_t *) lck);
}

static void shell_write_lock_bh(void *lck)
{
	write_lock_bh((rwlock_t *) lck);
}

static void shell_write_unlock_bh(void *lck)
{
	write_unlock_bh((rwlock_t *) lck);
}

static void shell_write_lock(void *lck)
{
	write_lock((rwlock_t *) lck);
}

static void shell_write_unlock(void *lck)
{
	write_unlock((rwlock_t *) lck);
}

static void *shell_rwlock_alloc(void)
{
	void *lck;

	lck = shell_kmalloc_sleep(sizeof(rwlock_t));

	return lck;
}

static void *shell_rwlock_alloc_atomic(void)
{
	void *lck;

	lck = shell_kmalloc_atomic(sizeof(rwlock_t));

	return lck;
}

static void shell_rwlock_free(void *lck)
{
	shell_kfree(lck);
}

static int shell_rwlock_init(void *lck)
{
	rwlock_init((rwlock_t *) lck);
	return 0;
}

static void shell_local_bh_disable(void)
{
	local_bh_disable();
}

static void shell_local_bh_enable(void)
{
	local_bh_enable();
}

static void shell_smp_mb(void)
{
	smp_mb();
}

static unsigned int shell_smp_processor_id(void)
{
	return smp_processor_id();
}

////////////////////////////////////////////////////////////////////////////////
unsigned long shell_get_seconds(void)
{
	return get_seconds();
}

void *shell_timer_list_alloc(void)
{
	void *pret = NULL;

	pret = kmalloc(sizeof(struct timer_list), GFP_KERNEL);

	return pret;
}

void *shell_timer_list_alloc_atomic(void)
{
	void *pret = NULL;

	pret = kmalloc(sizeof(struct timer_list), GFP_ATOMIC);

	return pret;
}

void shell_timer_list_free(void *timer_list_ptr)
{
	if (timer_list_ptr)
	{
		kfree(timer_list_ptr);
		timer_list_ptr = NULL;
	}
}

void shell_timer_setup(void *timer_list_ptr, void (*function)(unsigned long),
		unsigned long data)
{
	init_timer((struct timer_list *)timer_list_ptr);
	setup_timer((struct timer_list *)timer_list_ptr, function, data);
}

int shell_timer_mod(void *timer_list_ptr, unsigned long period)
{
	return mod_timer((struct timer_list *)timer_list_ptr, jiffies + (HZ * period));
}

int shell_timer_del(void *timer_list_ptr)
{
	return del_timer((struct timer_list *)timer_list_ptr);
}

int shell_timer_del_sync(void *timer_list_ptr)
{
	return del_timer_sync((struct timer_list *)timer_list_ptr);
}

////////////////////////////////////////////////////////////////////////////////
int shell_bitop_find_first_zero_bit(const void * p, unsigned long size)
{
	return find_first_zero_bit(p, size);
}

void shell_bitop_set_bit(int bit, unsigned long *p)
{
	set_bit(bit, p);
}

void shell_bitop_clear_bit(int bit, unsigned long * p)
{
	clear_bit(bit, p);
}

void shell_bitop_bitmap_zero(unsigned long *dst, int nbits)
{
	bitmap_zero(dst, nbits);
}

int shell_bitop_test_bit(int nr, const unsigned long *addr)
{
	return test_bit(nr, addr);
}

int shell_find_next_bit(const unsigned long *p, int size, int offset)
{
	return find_next_bit(p, size, offset);
}

unsigned long shell_find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	return find_next_zero_bit(addr, size, offset);
}

////////////////////////////////////////////////////////////////////////////////
static int shell_seq_printf(void *sf, const char *format, va_list ap)
{
	struct seq_file *m = (struct seq_file *) sf;
	int len;

	if (m->count < m->size) {
	        len = vsnprintf(m->buf + m->count, m->size - m->count, format, ap);
	        if (m->count + len < m->size) {
	                m->count += len;
	                return 0;
	        }
	}
	m->count = m->size;
	return -1;

}

////////////////////////////////////////////////////////////////////////////////
#if TMCFG_E_UDB_CORE
#define CALLING_TEST_VALUE		(0x55AA55AA)

static int register_syscall(void)
{
	int iret = -1;
	unsigned int calling_test_value;

	iret = tdts_core_syscall_init();
	if (iret)
	{
		ERR("tdts_core_syscall_init %d", iret);
		goto EXIT;
	}

	/* Verify if you can get the test value correctly. This's to prevent compiler flag issues. */
	tdts_core_syscall_set_test_value(CALLING_TEST_VALUE);
	calling_test_value = tdts_core_syscall_get_test_value();
	if (calling_test_value != CALLING_TEST_VALUE)
	{
		ERR("CALLING_TEST_VALUE %d", iret);
		goto EXIT;
	}

	/* Setup spinlock */
	iret = tdts_core_syscall_set_spin_lock(shell_spin_lock_init,
		shell_spin_lock_alloc, shell_spin_lock_free,
		shell_spin_lock, shell_spin_unlock,
		shell_spin_lock_bh, shell_spin_unlock_bh,
		shell_spin_is_locked);
	if (iret)
	{
		ERR("tdts_core_syscall_set_spin_lock %d", iret);
		goto EXIT;
	}

	/* Setup rwlock */
	iret = tdts_core_syscall_set_rwlock(
		shell_rwlock_init,
		shell_rwlock_alloc,
		shell_rwlock_alloc_atomic,
		shell_rwlock_free,
		shell_read_lock, shell_read_unlock,
		shell_read_lock_bh, shell_read_unlock_bh,
		shell_write_lock, shell_write_unlock,
		shell_write_lock_bh, shell_write_unlock_bh);

	/* Setup print functions (only for debug) */
	iret = tdts_core_syscall_set_printf(shell_vprintf, shell_vsprintf, shell_vsnprintf);
	if (iret)
	{
		ERR("tdts_core_syscall_set_printf %d", iret);
		goto EXIT;
	}

	/* Setup seq_print functions */
	iret = tdts_core_syscall_set_seq_printf(shell_seq_printf);
	if (iret)
	{
		ERR("tdts_core_syscall_set_seq_printf %d", iret);
		goto EXIT;
	}

	/* Setup malloc functions */
	iret = tdts_core_syscall_set_malloc((vmalloc_t) shell_vmalloc, (vfree_t) shell_vfree,
		(kmalloc_t) shell_kmalloc_atomic, (kmalloc_t) shell_kmalloc_sleep, (kfree_t) shell_kfree);
	if (iret)
	{
		ERR("tdts_core_syscall_set_malloc %d", iret);
		goto EXIT;
	}

	/* Setup skbuff functions */
	iret = tdts_core_syscall_set_alloc_skb(
		(alloc_skb_t) shell_alloc_skb_atomic,
		(skb_clone_t) shell_skb_clone_atomic,
		(kfree_skb_t) shell_kfree_skb);
	if (iret)
	{
		ERR("tdts_core_syscall_set_alloc_skb %d\n", iret);
		goto EXIT;
	}

	/* Setup local_bh_* functions */
	iret = tdts_core_syscall_set_local_bh(shell_local_bh_disable,shell_local_bh_enable);
	if (iret)
	{
		ERR("tdts_core_syscall_set_local_bh %d", iret);
		goto EXIT;
	}


	/* Set smp_mb */
	iret = tdts_core_syscall_set_smp_mb(shell_smp_mb);
	if (iret)
	{
		ERR("tdts_core_syscall_set_local_bh %d", iret);
		goto EXIT;
	}

	/* Setup smp_processor_id */
	tdts_core_syscall_set_smp_processor_id(shell_smp_processor_id);
	if (iret)
	{
		ERR("tdts_core_syscall_set_smp_processor_id %d", iret);
		goto EXIT;
	}

	/* Setup time functions */
	iret = tdts_core_syscall_set_time(shell_get_seconds, shell_timer_list_alloc,
			shell_timer_list_alloc_atomic, shell_timer_list_free,
			shell_timer_setup, shell_timer_mod,
			shell_timer_del, shell_timer_del_sync);
	if (iret)
	{
		ERR("tdts_core_syscall_set_time %d", iret);
		goto EXIT;
	}

	/* Setup bit functions */
	iret = tdts_core_syscall_set_bitop(shell_bitop_find_first_zero_bit,
			shell_bitop_set_bit, shell_bitop_clear_bit, shell_bitop_bitmap_zero,
			shell_bitop_test_bit, shell_find_next_bit, shell_find_next_zero_bit);
	if (iret)
	{
		ERR("tdts_core_syscall_set_bitop %d", iret);
		goto EXIT;
	}

	return 0;

EXIT:
	return iret;
}

static void unregister_syscall(void)
{
	tdts_core_syscall_cleanup();

	return;
}
#endif // TMCFG_E_UDB_CORE

////////////////////////////////////////////////////////////////////////////////

static int print_init_msg(void)
{
	return 0;
}

static void print_exit_msg(void)
{
}

struct init_tbl_entry
{
	int (*cb_init)(void);
	void (*cb_exit)(void);
};

static struct init_tbl_entry init_tbl[] =
	{
		{print_init_msg, print_exit_msg},
#if TMCFG_E_UDB_CORE
		{register_syscall, unregister_syscall},

		/* UDB Shell */
#if TMCFG_E_UDB_CORE_MEMTRACK
		{udb_shell_memtrack_init, udb_shell_memtrack_exit},
#endif
#if TMCFG_E_UDB_CORE_CONN_EXTRA
		{udb_shell_ct_extra_init, udb_shell_ct_extra_exit},
#endif		
		{udb_shell_udb_init, udb_shell_udb_exit},
#if TMCFG_E_UDB_CORE_IQOS_SUPPORT
		{udb_shell_qos_init, udb_shell_qos_exit},
#endif
#if TMCFG_E_UDB_CORE_ANOMALY_PREVENT
		{udb_shell_anomaly_init, udb_shell_anomaly_exit},
#endif
#if TMCFG_E_UDB_SHELL_PROCFS
		{udb_shell_procfs_init, udb_shell_procfs_cleanup},
#endif
		{udb_shell_ioctl_init, udb_shell_ioctl_cleanup}
#endif // TMCFG_E_UDB_CORE
	};

static int udb_shell_mod_init(void)
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

static void udb_shell_mod_exit(void)
{
#if TMCFG_E_UDB_CORE
	extern atomic_t udb_mem_sta;
	extern atomic_t udb_mem_dyn;
#endif

	int i, size;
	void (*cb_exit)(void);

	size = sizeof(init_tbl) / sizeof(*init_tbl);

	for (i = size - 1; i >= 0; i--)
	{
		cb_exit = init_tbl[i].cb_exit;

		cb_exit();
	}

#if TMCFG_E_UDB_CORE	
	if (atomic_read(&udb_mem_sta) != 0 || atomic_read(&udb_mem_dyn) != 0)
	{
		ERR("Udb static = %d, dynamic = %d\n",
			atomic_read(&udb_mem_sta), atomic_read(&udb_mem_dyn));
	}
#endif
}

MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION("SHN UDB Module");

module_init(udb_shell_mod_init);
module_exit(udb_shell_mod_exit);

