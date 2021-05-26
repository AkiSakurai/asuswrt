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
#include <linux/bitmap.h>

#include <asm/atomic.h>
#include <linux/time.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <linux/if_ether.h>

#include <linux/netfilter.h>
#include <linux/netfilter_bridge.h>
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

#include "tdts/tdts_core.h"
#include "tdts/tdts_shell.h"

#if TMCFG_E_SHELL_ATIMER
#include "shell_atimer.h"
#endif

#if TMCFG_E_SHELL_DEPRECATED_PROCFS
#include "shell_deprecated_procfs.h"
#endif



////////////////////////////////////////////////////////////////////////////////
#define CONFIG_DEBUG (0) //!< Say 1 to debug
#define THIS_MOD_NAME "tdts"

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


int tdts_shell_system_setting_tcp_conn_max_set(unsigned int conn_max)
{
	return tdts_core_system_setting_tcp_conn_max_set(conn_max);
}
EXPORT_SYMBOL(tdts_shell_system_setting_tcp_conn_max_set);

int tdts_shell_system_setting_tcp_conn_max_get(unsigned int *conn_max_ptr)
{
	return tdts_core_system_setting_tcp_conn_max_get(conn_max_ptr);
}
EXPORT_SYMBOL(tdts_shell_system_setting_tcp_conn_max_get);

int tdts_shell_system_setting_tcp_conn_timeout_set(unsigned int timeout)
{
	return tdts_core_system_setting_tcp_conn_timeout_set(timeout);
}
EXPORT_SYMBOL(tdts_shell_system_setting_tcp_conn_timeout_set);

int tdts_shell_system_setting_tcp_conn_timeout_get(unsigned int *timeout_ptr)
{
	return tdts_core_system_setting_tcp_conn_timeout_get(timeout_ptr);
}
EXPORT_SYMBOL(tdts_shell_system_setting_tcp_conn_timeout_get);


int tdts_shell_system_setting_udp_conn_max_set(unsigned int conn_max)
{
	return tdts_core_system_setting_udp_conn_max_set(conn_max);
}
EXPORT_SYMBOL(tdts_shell_system_setting_udp_conn_max_set);

int tdts_shell_system_setting_udp_conn_max_get(unsigned int *conn_max_ptr)
{
	return tdts_core_system_setting_udp_conn_max_get(conn_max_ptr);
}
EXPORT_SYMBOL(tdts_shell_system_setting_udp_conn_max_get);

/*
 * The data pointed by sip and dip are in NETWORK order.
 * The values of sport and dport are in HOST order.
 */
int tdts_shell_tcp_conn_remove(uint8_t ip_ver,
		uint8_t *sip,
		uint8_t *dip,
		uint16_t sport,
		uint16_t dport)
{
	return tdts_core_tcp_conn_remove(ip_ver, sip, dip, sport, dport);
}
EXPORT_SYMBOL(tdts_shell_tcp_conn_remove);

/*
 * The data pointed by sip and dip are in NETWORK order.
 * The values of sport and dport are in HOST order.
 */
int tdts_shell_udp_conn_remove(uint8_t ip_ver,
		uint8_t *sip,
		uint8_t *dip,
		uint16_t sport,
		uint16_t dport)
{
	return tdts_core_udp_conn_remove(ip_ver, sip, dip, sport, dport);
}
EXPORT_SYMBOL(tdts_shell_udp_conn_remove);

#if TMCFG_E_CORE_PORT_SCAN_DETECTION
int tdts_shell_port_scan_log_cb_set(port_scan_log_func cb)
{
	return tdts_core_port_scan_log_cb_set(cb);
}
EXPORT_SYMBOL(tdts_shell_port_scan_log_cb_set);

void *tdts_shell_port_scan_context_alloc(void)
{
	return tdts_core_port_scan_context_alloc();
}
EXPORT_SYMBOL(tdts_shell_port_scan_context_alloc);

int tdts_shell_port_scan_context_dealloc(void *ctx)
{
	return tdts_core_port_scan_context_dealloc(ctx);
}
EXPORT_SYMBOL(tdts_shell_port_scan_context_dealloc);
#endif

#if TMCFG_E_CORE_IP_SWEEP_DETECTION
int tdts_shell_ip_sweep_log_cb_set(ip_sweep_log_func cb)
{
	return tdts_core_ip_sweep_log_cb_set(cb);
}
EXPORT_SYMBOL(tdts_shell_ip_sweep_log_cb_set);

void *tdts_shell_ip_sweep_context_alloc(void)
{
	return tdts_core_ip_sweep_context_alloc();
}
EXPORT_SYMBOL(tdts_shell_ip_sweep_context_alloc);

int tdts_shell_ip_sweep_context_dealloc(void *ctx)
{
	return tdts_core_ip_sweep_context_dealloc(ctx);
}
EXPORT_SYMBOL(tdts_shell_ip_sweep_context_dealloc);
#endif

//------------------------------------------------------------------------------

int tdts_shell_dpi_l2_eth(tdts_pkt_parameter_t *pkt_parameter_ptr)
{
	int ret;

	local_bh_disable();
	ret = tdts_core_pkt_processor(pkt_parameter_ptr);
	local_bh_enable();

	return ret;
}
EXPORT_SYMBOL(tdts_shell_dpi_l2_eth);

int tdts_shell_dpi_l3_skb(struct sk_buff *skb, tdts_pkt_parameter_t *param)
{
	int ret = -1;
	
	if (skb_linearize(skb))
	{
		// Possibly memory leak.
		ERR("skb_linearize(skb) ERROR\n");
		ret = -1;	
		goto EXIT;
	}

	switch (ntohs(skb->protocol))
	{
	case ETH_P_IP:
		tdts_set_pkt_parameter_l3_ip(param, skb->data, skb->len);
		break;
	case ETH_P_IPV6:
		tdts_set_pkt_parameter_l3_ip6(param, skb->data, skb->len);
		break;
	default:
		DBG("Bypass unsupported skb->protocol 0x%04x", skb->protocol);
		
		//ret = -1;	
		ret = 0;	
		goto EXIT;
	}

#if TMCFG_E_SHELL_ATIMER
	tdts_set_pkt_parameter_pkt_time(param, tdts_shell_atimer_get_sec());
#else
	/* The caller is responsible to setup the pkt time */
#endif

	local_bh_disable();
	ret = tdts_core_pkt_processor(param);
	local_bh_enable();
EXIT:
	return ret;

}
EXPORT_SYMBOL(tdts_shell_dpi_l3_skb);

#if TMCFG_E_CORE_METADATA_EXTRACT
int tdts_shell_dpi_register_mt(meta_extract cb, int type)
{
	if ((NULL == cb) || (type >= TDTS_META_TYPE_MAX))
	{
		ERR("Register mt failed. cb = %p, type = %d\n"
			, cb, type);
		return -1;
	}

	return meta_cb_register(cb, type);
}
EXPORT_SYMBOL (tdts_shell_dpi_register_mt);

int tdts_shell_dpi_unregister_mt(meta_extract cb, int type)
{
	return meta_cb_unregister(cb, type);
}
EXPORT_SYMBOL (tdts_shell_dpi_unregister_mt);
#endif

int tdts_shell_suspect_list_init(void *void_ptr, unsigned mem_size, unsigned entry_num)
{
	return tdts_core_suspect_list_init(void_ptr, mem_size, entry_num);
}
EXPORT_SYMBOL (tdts_shell_suspect_list_init);

int tdts_shell_flood_mem_init(void *void_ptr, unsigned mem_size, unsigned record_max)
{
	return tdts_core_flood_mem_init(void_ptr, mem_size, record_max);
}
EXPORT_SYMBOL (tdts_shell_flood_mem_init);

int tdts_shell_flood_record(void *void_ptr, unsigned record_max, flood_spec_t *spec, pkt_info_t *pkt)
{
	return tdts_core_flood_record(void_ptr, record_max, spec, pkt);
}
EXPORT_SYMBOL (tdts_shell_flood_record);

void tdts_shell_flood_record_output_and_init(void *void_ptr, unsigned record_max, uint32_t signature_id,
	uint16_t flood_type, flood_log_cb log_cb)
{
        tdts_core_flood_record_output_and_init(void_ptr, record_max, signature_id, flood_type, log_cb);
}
EXPORT_SYMBOL (tdts_shell_flood_record_output_and_init);

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

////////////////////////////////////////////////////////////////////////////////
static int shell_spin_is_locked(void *lck)
{
	return spin_is_locked((spinlock_t *) lck);
}

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

	/* FIXME: Uh, normally, we should use sleep version. */
	lck = shell_kmalloc_atomic(sizeof(spinlock_t));

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
static void shell_schedule(void)
{
	schedule();
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////


static int shell_bitop_find_first_zero_bit(const void * p, unsigned long size)
{
	return find_first_zero_bit(p, size);
}

static void shell_bitop_set_bit(int bit, unsigned long *p)
{
	set_bit(bit, p);
}

static void shell_bitop_clear_bit(int bit, unsigned long * p)
{
	clear_bit(bit, p);
}

static void shell_bitop_bitmap_zero(unsigned long *dst, int nbits)
{
	bitmap_zero(dst, nbits);
}

static int shell_bitop_test_bit(int nr, const unsigned long *addr)
{
	return test_bit(nr, addr);
}

static int shell_find_next_bit(const unsigned long *p, int size, int offset)
{
	return find_next_bit(p, size, offset);
}

static unsigned long shell_find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	return find_next_zero_bit(addr, size, offset);
}



////////////////////////////////////////////////////////////////////////////////
#if 0
static int shell_seq_printf(void *sf, const char *format, va_list ap)
{
	/*
	 * FIXME
	 */

//	int ret = 0;
//
//	va_list argptr;
//
//	va_start(argptr, format);
//	printk(format, argptr);
//	ret = seq_printf(sf, format, argptr);
//	va_end(argptr);
//
//	return ret;

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
#endif

////////////////////////////////////////////////////////////////////////////////

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
	iret = tdts_core_syscall_set_rwlock(shell_rwlock_init, shell_rwlock_alloc,
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

#if 0
	/* Setup seq_print functions */
	iret = tdts_core_syscall_set_seq_printf(shell_seq_printf);
	if (iret)
	{
		ERR("tdts_core_syscall_set_seq_printf %d", iret);
		goto EXIT;
	}
#endif

	/* Setup malloc functions */
	iret = tdts_core_syscall_set_malloc((vmalloc_t) shell_vmalloc, (vfree_t) shell_vfree,
		(kmalloc_t) shell_kmalloc_atomic, (kmalloc_t) shell_kmalloc_sleep, (kfree_t) shell_kfree);
	if (iret)
	{
		ERR("tdts_core_syscall_set_malloc %d", iret);
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
	iret = tdts_core_syscall_set_smp_processor_id(shell_smp_processor_id);
	if (iret)
	{
		ERR("tdts_core_syscall_set_smp_processor_id %d", iret);
		goto EXIT;
	}

	/* Setup schedule */
	iret = tdts_core_syscall_set_schedule(shell_schedule);
	if (iret)
	{
		ERR("tdts_core_syscall_set_schedule %d", iret);
		goto EXIT;
	}
	
#if 0 //TMCFG_ARCH_MIPS
	/* Setup interrupt control */
	tdts_core_syscall_set_local_irq_control();
#endif

	return 0;

EXIT:
	return iret;
}

static void unregister_syscall(void)
{
	tdts_core_syscall_cleanup();

	return;
}

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
		{register_syscall, unregister_syscall},
		{tdts_core_init, tdts_core_cleanup},
#if TMCFG_E_SHELL_ATIMER
		{tdts_shell_atimer_init, tdts_shell_atimer_cleanup},
#endif
#if TMCFG_E_SHELL_DEPRECATED_PROCFS
		{tdts_shell_deprecated_procfs_init, tdts_shell_deprecated_procfs_cleanup},
#endif
		{tdts_shell_ioctl_init, tdts_shell_ioctl_cleanup}
	};

static int tdts_shell_init(void)
{
	int i = -1, size, value;
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

	/* It is time to adjust some settings */
	/*
	tdts_shell_system_setting_tcp_conn_max_set(4000);
	*/
	tdts_shell_system_setting_tcp_conn_max_get(&value);
	PRT("tcp_conn_max = %u\n",value);

	/*
	tdts_shell_system_setting_tcp_conn_timeout_set(300);
	*/	
	tdts_shell_system_setting_tcp_conn_timeout_get(&value);
	PRT("tcp_conn_timeout = %u sec\n",value);
	

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

static void tdts_shell_cleanup(void)
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

////////////////////////////////////////////////////////////////////////////////

static int shell_mod_init(void)
{
	return tdts_shell_init();
}

static void shell_mod_exit(void)
{
	tdts_shell_cleanup();
}

MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION("TDTS shell module");
MODULE_SUPPORTED_DEVICE(TMCFG_E_KMOD_IOCTL_DEV_NAME);

module_init(shell_mod_init);
module_exit(shell_mod_exit);

