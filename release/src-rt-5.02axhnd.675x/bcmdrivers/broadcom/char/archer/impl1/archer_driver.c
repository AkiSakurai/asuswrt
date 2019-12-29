/*
  <:copyright-BRCM:2017:proprietary:standard

  Copyright (c) 2017 Broadcom 
  All Rights Reserved

  This program is the proprietary software of Broadcom and/or its
  licensors, and may only be used, duplicated, modified or distributed pursuant
  to the terms and conditions of a separate, written license agreement executed
  between you and Broadcom (an "Authorized License").  Except as set forth in
  an Authorized License, Broadcom grants no license (express or implied), right
  to use, or waiver of any kind with respect to the Software, and Broadcom
  expressly reserves all rights in and to the Software and all intellectual
  property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
  NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
  BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.

  Except as expressly set forth in the Authorized License,

  1. This program, including its structure, sequence and organization,
  constitutes the valuable trade secrets of Broadcom, and you shall use
  all reasonable efforts to protect the confidentiality thereof, and to
  use this information only in connection with your use of Broadcom
  integrated circuit products.

  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
  AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
  WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
  RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
  ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
  FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
  COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
  TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
  PERFORMANCE OF THE SOFTWARE.

  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
  ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
  INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
  WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
  IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
  OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
  SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
  SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
  LIMITED REMEDY.
  :> 
*/

/*
*******************************************************************************
*
* File Name  : archer_driver.c
*
* Description: Main archer driver implementation
*
*******************************************************************************
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/bcm_log.h>
#include <linux/kthread.h>
#include <linux/nbuff.h>
#include <linux/blog.h>
#include <linux/bcm_skb_defines.h>
#include <net/ipv6.h>
#include "bcm_ubus4.h"
#include "fcachehw.h"
#include "bcmenet.h"

#include "sysport_rsb.h"
#include "sysport_classifier.h"
#include "sysport_driver.h"

#include "archer.h"
#include "archer_driver.h"
#include "archer_thread.h"
#include "archer_socket.h"
#include "archer_wlan.h"
#include "archer_dpi.h"
#include "bcm_archer_dpi.h"

#include "archer_xtmrt.h"

int iudma_driver_host_bind(archer_host_hooks_t *hooks_p);

/*******************************************************************************
 *
 * Global Variables and Definitions
 *
 *******************************************************************************/

#undef ARCHER_DECL
#define ARCHER_DECL(x) #x,

static const char *archer_ioctl_cmd_name[] =
{
    ARCHER_DECL(ARCHER_IOC_STATUS)
    ARCHER_DECL(ARCHER_IOC_BIND)
    ARCHER_DECL(ARCHER_IOC_UNBIND)
    ARCHER_DECL(ARCHER_IOC_DEBUG)
    ARCHER_DECL(ARCHER_IOC_FLOWS)
    ARCHER_DECL(ARCHER_IOC_UCAST_L3)
    ARCHER_DECL(ARCHER_IOC_UCAST_L2)
    ARCHER_DECL(ARCHER_IOC_MCAST)
    ARCHER_DECL(ARCHER_IOC_HOST)
    ARCHER_DECL(ARCHER_IOC_MODE)
    ARCHER_DECL(ARCHER_IOC_STATS)
    ARCHER_DECL(ARCHER_IOC_SYSPORT)
    ARCHER_DECL(ARCHER_IOC_MPDCFG)
    ARCHER_DECL(ARCHER_IOC_WOL)
    ARCHER_DECL(ARCHER_IOC_DPI)
    ARCHER_DECL(ARCHER_IOC_XTMDROPALG_SET)
    ARCHER_DECL(ARCHER_IOC_XTMDROPALG_GET)
    ARCHER_DECL(ARCHER_IOC_MAX)
};

typedef struct {
    int rx_count;
} archer_netdevice_t;

#if defined(CONFIG_BCM_ARCHER_GSO)
typedef struct {
    struct net_device *dev;
    struct net_device *skb_dev_orig;
    pNBuff_t *pNBuff_list;
    int nbuff_count;
    int nbuff_max;
} archer_driver_gso_t;
#endif

typedef struct {
    int status;              /* status: Enable=1 or Disable=0     */
    int activates;           /* number of activates (downcalls)   */
    int activate_overflows;  /* number of activate overflows      */
    int activate_failures;   /* number of activate failures       */
    int deactivates;         /* number of deactivates (downcalls) */
    int deactivate_failures; /* number of deactivate failures     */
    int flushes;             /* number of clear (upcalls)         */
    int active;
#if defined(CONFIG_BCM_ARCHER_GSO)
    archer_driver_gso_t gso;
#endif
    struct device *dummy_dev;
} archer_driver_t;

static archer_driver_t archer_driver_g;   /* Protocol layer global context */

int archer_packet_length_max_g = BCM_MAX_PKT_LEN;

int archer_packet_headroom_g = BCM_PKT_HEADROOM;

int archer_skb_tailroom_g = BCM_SKB_TAILROOM;

/*******************************************************************************
 *
 * Public API
 *
 *******************************************************************************/

/*
 *------------------------------------------------------------------------------
 * Function   : archer_activate
 * Description: This function is invoked when an Archer flow needs to be
 *              activated.
 * Parameters :
 *  blog_p: pointer to a blog object (for multicast only)
 * Returns    : fhw_tuple: 32bit index to refer to a flow in HW
 *------------------------------------------------------------------------------
 */
int archer_activate(Blog_t *blog_orig_p, uint32_t key_in)
{
    bcmFun_t *bcmFunPrepend = bcmFun_get(BCM_FUN_ID_RUNNER_PREPEND);
    BCM_runnerPrepend_t prepend;
    sysport_flow_key_t flow_key;
    int is_activation = 1;
    Blog_t *blog_p;
    int ret;

    BCM_ASSERT(blog_orig_p != BLOG_NULL);

#if defined(CONFIG_BCM_ARCHER_SIM)
    blog_p = archer_sim_blog_get(blog_orig_p);
    if(!blog_p)
    {
        ret = SYSPORT_CLASSIFIER_ERROR_INVALID;

        goto abort_activate;
    }
#else
    blog_p = blog_orig_p;
#endif

    __debug("\n::: archer_activate :::\n\n");
    __dump_blog(blog_p);

    prepend.blog_p = blog_p;
    prepend.size = 0;

    if(bcmFunPrepend)
    {
        bcmFunPrepend(&prepend);

        if(prepend.size > CMDLIST_PREPEND_SIZE_MAX)
        {
            __logInfo("Invalid prepend data size, aborting flow creation: size %d", prepend.size);

            ret = SYSPORT_CLASSIFIER_ERROR_INVALID;

            goto abort_activate;
        }
    }

    if(blog_p->rx.info.bmap.PLD_L2)
    {
        ret = archer_ucast_l2_activate(blog_p, &flow_key, prepend.data, prepend.size);
        if(ret)
        {
            __logInfo("Could not archer_ucast_l2_activate");

            goto abort_activate;
        }
    }
    else
    {
        if(blog_p->rx.multicast)
        {
            ret = archer_mcast_activate(blog_p, &flow_key, &is_activation);
            if(ret)
            {
                __logInfo("Could not archer_mcast_activate");

                goto abort_activate;
            }
        }
        else
        {
            ret = archer_ucast_activate(blog_p, &flow_key, prepend.data, prepend.size);
            if(ret)
            {
                __logInfo("Could not archer_ucast_activate");

                goto abort_activate;
            }
        }
    }

    if(is_activation)
    {
        archer_driver_g.activates++;
        archer_driver_g.active++;
    }

    __debug("::: %s: flow_key <0x%08x>, cumm_activates <%u> :::\n\n",
            sysport_rsb_flow_type_name[flow_key.flow_type],
            flow_key.u32, archer_driver_g.activates);

#if defined(CONFIG_BCM_ARCHER_SIM)
    archer_sim_blog_put(blog_orig_p, blog_p);
#endif

    return flow_key.u32;

abort_activate:
    if(SYSPORT_CLASSIFIER_ERROR_OVERFLOW(ret))
    {
        archer_driver_g.activate_overflows++;

        __logInfo("cumm_activate_overflows<%u>", archer_driver_g.activate_overflows);
    }
    else
    {
        archer_driver_g.activate_failures++;

        __logInfo("cumm_activate_failures<%u>", archer_driver_g.activate_failures);
    }

#if defined(CONFIG_BCM_ARCHER_SIM)
    archer_sim_blog_put(blog_orig_p, blog_p);
#endif

    return FHW_TUPLE_INVALID;
}

/*
 *------------------------------------------------------------------------------
 * Function   : archer_refresh
 * Description: This function is invoked to collect flow statistics
 * Parameters :
 *  fhw_tuple : 32bit index to refer to an Archer flow
 * Returns    : Total hits and octets on this connection.
 *------------------------------------------------------------------------------
 */
static int archer_refresh(uint32_t fhw_tuple, uint32_t *packets_p, uint32_t *octets_p)
{
    sysport_classifier_flow_stats_t stats;
    int ret;

    ret = sysport_classifier_flow_stats_get((sysport_flow_key_t)fhw_tuple, &stats);
    if(ret)
    {
        __logError("Could not get stats for fhw_tuple <0x%08x>", fhw_tuple);
    }
    else
    {
        *packets_p = stats.packets;
        *octets_p = stats.bytes;
    }

    return ret;
}

/*
 *------------------------------------------------------------------------------
 * Function   : archer_refresh_pathstat
 * Description: This function is invoked to collect pathstat statistics
 * Parameters :
 *  fhw_tuple : pathstat counter index
 * Returns    : Total hits and octets on this connection.
 *------------------------------------------------------------------------------
 */
static int archer_refresh_pathstat(uint32_t pathstat_index, uint32_t *packets_p, uint32_t *octets_p)
{
    sysport_classifier_pathstat_t pathstat;
    int ret;

    ret = sysport_classifier_pathstat_get(pathstat_index, &pathstat);
    if(ret)
    {
        __logError("Could not get pathstat for index <%u>", pathstat_index);
    }
    else
    {
        *packets_p = pathstat.packets;
        *octets_p = pathstat.bytes;
    }

    return ret;
}

/*
 *------------------------------------------------------------------------------
 * Function   : archer_reset_stats
 * Description: This function is invoked to reset stats for a flow
 * Parameters :
 *  fhw_tuple: 32bit index to refer to an Archer flow
 * Returns    : 0 on success.
 *------------------------------------------------------------------------------
 */
static int archer_reset_stats(uint32_t fhw_tuple)
{
    int ret;

    ret = sysport_classifier_flow_stats_reset((sysport_flow_key_t)fhw_tuple);
    if(ret)
    {
        __logError("Could not reset get stats for fhw_tuple <0x%08x>", fhw_tuple);
    }

    return ret;
}

/*
 *------------------------------------------------------------------------------
 * Function   : archer_deactivate
 * Description: This function is invoked when an Archer flow needs to be
 *              deactivated.
 * Parameters :
 *  fhw_tuple: 32bit index to refer to a flow in HW
 *  blog_p: pointer to a blog object (for multicast only)
 * Returns    : Remaining number of active ports (for multicast only)
 *------------------------------------------------------------------------------
 */
static int archer_deactivate(uint32_t fhw_tuple, uint32_t *packets_p,
                             uint32_t *octets_p, struct blog_t *blog_p)
{
    sysport_flow_key_t flow_key;
    int is_deactivation = 1;
    int ret;

    __debug("\n::: archer_deactivate :::\n\n");
    __dump_blog(blog_p);

    flow_key.u32 = fhw_tuple;

    /* Fetch last hit count */
    ret = archer_refresh(fhw_tuple, packets_p, octets_p);
    if(ret)
    {
        goto abort_deactivate;
    }

    if(blog_p->rx.info.bmap.PLD_L2)
    {
        ret = archer_ucast_l2_deactivate(flow_key);
        if(ret)
        {
            goto abort_deactivate;
        }
    }
    else
    {
        if(blog_p->rx.multicast)
        {
            ret = archer_mcast_deactivate(blog_p, &is_deactivation);
            if(ret)
            {
                goto abort_deactivate;
            }
        }
        else
        {
            ret = archer_ucast_deactivate(flow_key);
            if(ret)
            {
                goto abort_deactivate;
            }
        }
    }

    if(is_deactivation)
    {
        archer_driver_g.deactivates++;
        archer_driver_g.active--;
    }

    __logDebug("::: %s: flow_key <0x%08x>, hits<%u>, bytes<%u>, cumm_deactivates<%u> :::\n",
               sysport_rsb_flow_type_name[flow_key.flow_type],
               flow_key.u32, *packets_p, *octets_p, archer_driver_g.deactivates);

    return ret;

abort_deactivate:
    archer_driver_g.deactivate_failures++;

    return ret;
}

static int archer_update(BlogUpdate_t update, uint32_t fhw_tuple, Blog_t *blog_p)
{
    sysport_flow_key_t flow_key;
    int ret = SYSPORT_CLASSIFIER_ERROR_INVALID;

    __debug("\n::: archer_update :::\n\n");
    __debug("update %u, flow_key 0x%x\n", update, fhw_tuple);

    flow_key.u32 = fhw_tuple;

    switch(update)
    {
        case BLOG_UPDATE_DPI_QUEUE:
            ret = sysport_classifier_flow_dpi_queue_set(flow_key, blog_p->dpi_queue);
            break;

#if defined(CONFIG_BCM_DPI_WLAN_QOS)
        case BLOG_UPDATE_DPI_PRIORITY:
            if(blog_p->wfd.nic_ucast.is_chain)
                /* Flow in NIC mode */
                ret = sysport_classifier_flow_dpi_priority_set(flow_key, -1, blog_p->wfd.nic_ucast.priority);
            else
                /* Flow in DHD mode */
                ret = sysport_classifier_flow_dpi_priority_set(flow_key, blog_p->wfd.dhd_ucast.flowring_idx, blog_p->wfd.dhd_ucast.priority);
            break;
#endif

        default:
            __logError("Invalid BLOG Update: <%d>", update);
    }

    return ret;
}

/******************************************************************
 *
 * Flow Cache Binding
 *
 *****************************************************************/

#if defined(CONFIG_BCM_FHW)
static FC_CLEAR_HOOK fhw_clear_hook_fp = NULL;

/*
 *------------------------------------------------------------------------------
 * Function   : archer_flow_cache_clear
 * Description: Clears FlowCache association(s) to Archer entries.
 *------------------------------------------------------------------------------
 */
static int archer_flow_cache_clear(uint32_t key, const FlowScope_t scope)
{
    int count;

    /* Upcall into FlowCache */
    if(fhw_clear_hook_fp)
    {
        archer_driver_g.flushes += fhw_clear_hook_fp(key, scope);
    }

    count = sysport_classifier_flow_delete_all();

    __debug("key<%03u> scope<%s> cumm_flushes<%u>",
            key, (scope == System_e) ? "System" : "Match",
            archer_driver_g.flushes);

    return count;
}

/*
 *------------------------------------------------------------------------------
 * Function   : archer_clear
 * Description: This function is invoked when all entries pertaining to
 *              a tuple in Archer need to be cleared.
 * Parameters :
 *  tuple: FHW Engine instance and match index
 * Returns    : success
 *------------------------------------------------------------------------------
 */
static int archer_clear(uint32_t fhw_tuple)
{
    return 0;
}

#endif /* CONFIG_BCM_FHW */

/*
 *------------------------------------------------------------------------------
 * Function   : archer_fc_bind
 * Description: Binds the Archer driver functions to the Flow Cache hooks.
 *------------------------------------------------------------------------------
 */
void archer_fc_bind(void)
{
#if defined(CONFIG_BCM_FHW)
    if(!archer_driver_g.status)
    {
        FhwHwAccPrio_t prioIx = FHW_PRIO_0;
        FhwBindHwHooks_t hwHooks = {};
        /* Initialize HW Hooks - Start */
        hwHooks.activate_fn = (HOOKP32)archer_activate;
        hwHooks.deactivate_fn = (HOOK4PARM)archer_deactivate;
        hwHooks.update_fn = (HOOK3PARM)archer_update;
        hwHooks.refresh_fn = (HOOK3PARM)archer_refresh;
        hwHooks.refresh_pathstat_fn = (HOOK3PARM)archer_refresh_pathstat;
        hwHooks.fhw_clear_fn = &fhw_clear_hook_fp;
        hwHooks.reset_stats_fn =(HOOK32) archer_reset_stats;
        hwHooks.cap = ( (1<<HW_CAP_IPV4_UCAST) | (1<<HW_CAP_L2_UCAST) |
                        (1<<HW_CAP_IPV4_MCAST) | (1<<HW_CAP_PATH_STATS) );
#if defined(CONFIG_BLOG_IPV6)
        hwHooks.cap |= (1<<HW_CAP_IPV6_UCAST) | (1<<HW_CAP_IPV6_TUNNEL) | (1<<HW_CAP_IPV6_MCAST);
#endif
        hwHooks.max_ent = SYSPORT_UCAST_FLOW_MAX;
        hwHooks.max_hw_pathstat = SYSPORT_CLASSIFIER_PATHSTAT_MAX;
        /* Bind to fc HW layer for learning connection configurations dynamically */
        hwHooks.clear_fn = (HOOK32)archer_clear;
        /* Initialize HW Hooks - End */

        /* Block flow-cache from packet processing and try to push the flows */
        blog_lock();

        fhw_bind_hw(prioIx, &hwHooks);

        BCM_ASSERT(fhw_clear_hook_fp);

#if defined(CONFIG_BCM_ARCHER_SIM)
        archer_sim_enable();
#endif
        archer_driver_g.status = 1;

        blog_unlock();

        __print("Enabled Archer binding to Flow Cache\n");
    }
    else
    {
        __print("Already Enabled\n");
    }
#else
    __print("Flow Cache is not built\n");
#endif
}

/*
 *------------------------------------------------------------------------------
 * Function   : archer_fc_unbind
 * Description: Clears all active Flow Cache associations with Archer.
 *              Unbind flow cache hooks.
 *------------------------------------------------------------------------------
 */
void archer_fc_unbind(void)
{
#if defined(CONFIG_BCM_FHW)
    if(archer_driver_g.status)
    {
        FhwBindHwHooks_t hwHooks = {};
        FhwHwAccPrio_t prioIx = FHW_PRIO_0;

        /* Block flow-cache from packet processing and try to push the flows */
        blog_lock(); 

        /* Clear system wide active FlowCache associations, and disable learning. */

        archer_flow_cache_clear(0, System_e);

        hwHooks.fhw_clear_fn = &fhw_clear_hook_fp;

        fhw_bind_hw(prioIx, &hwHooks);

        fhw_clear_hook_fp = (FC_CLEAR_HOOK)NULL;

#if defined(CONFIG_BCM_ARCHER_SIM)
        archer_sim_disable();
#endif
        archer_driver_g.status = 0;

        blog_unlock();

        __print("Disabled Archer binding to Flow Cache\n");
    }
    else
    {
        __print("Already Disabled\n");
    }
#else
    __print("Flow Cache is not built\n");
#endif
}

/*
 *------------------------------------------------------------------------------
 *  Helper Functions
 *------------------------------------------------------------------------------
 */
static int archer_dummy_device_alloc(void)
{
    if(archer_driver_g.dummy_dev == NULL)
    {
        archer_driver_g.dummy_dev = kzalloc(sizeof(struct device), GFP_KERNEL);

        if(archer_driver_g.dummy_dev == NULL)
        {
            __logError("Could not allocate dummy_dev");

            return -1;
        }
#if defined(CONFIG_BCM963178) || defined(CONFIG_BCM947622)
        dma_set_coherent_mask(archer_driver_g.dummy_dev, DMA_BIT_MASK(32));
#ifdef CONFIG_BCM_GLB_COHERENCY
        arch_setup_dma_ops(archer_driver_g.dummy_dev, 0, 0, NULL, true);
#endif
#else
        dma_set_coherent_mask(archer_driver_g.dummy_dev, DMA_BIT_MASK(32));
#if !defined (CONFIG_BCM963268)
        archer_driver_g.dummy_dev->archdata.dma_coherent = IS_DDR_COHERENT;
#endif
#endif
    }

    return 0;
}

static void archer_dummy_device_free(void)
{
    if(archer_driver_g.dummy_dev)
    {
        kfree(archer_driver_g.dummy_dev);
    }
}

void *archer_coherent_mem_alloc(int size, void **phys_addr_pp)
{
    dma_addr_t phys_addr;
    void *p = dma_alloc_coherent(archer_driver_g.dummy_dev,
                                 size, &phys_addr, GFP_KERNEL);

    *phys_addr_pp = (void *)phys_addr;

    return p; /* return host address */
}

void archer_coherent_mem_free(int size, void *phys_addr_p, void *p)
{
    dma_addr_t phys_addr = (dma_addr_t)phys_addr_p;

    dma_free_coherent(archer_driver_g.dummy_dev,
                      size, p, phys_addr);
}

void *archer_mem_alloc(int size)
{
    return kmalloc(size, GFP_KERNEL);
}

void archer_mem_free(void *p)
{
    kfree(p);
}

long archer_driver_get_time_ns(void)
{
    struct timespec64 ts;

    ktime_get_real_ts64(&ts);

    return ts.tv_nsec;
}

void archer_driver_nbuff_params(pNBuff_t pNBuff, uint8_t **data_p, uint32_t *length_p)
{
    nbuff_get_context(pNBuff, data_p, length_p);
}

void *archer_driver_skb_params(void *buf_p, uint8_t **data_p, uint32_t *length_p, void **fkbInSkb_p)
{
    struct sk_buff *skb_p = (struct sk_buff *)buf_p;

    *data_p = skb_p->data;
    *length_p = skb_p->len;
    *fkbInSkb_p = &skb_p->fkbInSkb;

    return skb_p->prev;
}

static int archer_driver_brcm_tag_info(Blog_t *blog_p, int *switch_port_p, int *switch_queue_p)
{
    int intf_index;

    *switch_queue_p = SKBMARK_GET_Q_PRIO(blog_p->mark);

    return sysport_driver_logical_port_to_phys_port(blog_p->tx.info.channel,
                                                    &intf_index, switch_port_p);
}

static int archer_driver_host_bind(void *arg_p)
{
    archer_host_hooks_t *hooks_p = (archer_host_hooks_t *)arg_p;
    int ret;

    switch(hooks_p->host_type)
    {
        case ARCHER_HOST_TYPE_ENET:
            ret = sysport_driver_host_bind(hooks_p);
            break;

        case ARCHER_HOST_TYPE_XTMRT:
#if defined(CONFIG_BCM94908) || defined(CONFIG_BCM963268) ||            \
    (defined(CONFIG_BCM963178) && (defined(CONFIG_BCM_XTMRT) || defined(CONFIG_BCM_XTMRT_MODULE)))
            ret = iudma_driver_host_bind(hooks_p);
            break;
#endif
        default:
            __logError("Invalid host_type %d\n", hooks_p->host_type);
            ret = -1;
    }

    return ret;
}

static int archer_driver_tcp_ack_mflows_set(int enable)
{
    sysport_classifier_tcp_pure_ack_enable(enable);

    return 0;
}

/*******************************************************************
 *
 * Archer Kernel Threads
 *
 *******************************************************************/

typedef int (*archer_thread_fn)(void *arg);

typedef struct {
    int work_avail;
    wait_queue_head_t rx_thread_wqh;
    struct task_struct *rx_thread;
} archer_thread_t;

/* Archer Upstream Thread */
static archer_thread_t archer_thread_us_g;

void *archer_thread_get(archer_thread_id_t thread_id)
{
    switch(thread_id)
    {
        case ARCHER_THREAD_ID_US:
            return &archer_thread_us_g;

        default:
            __logError("Invalid thread_id %u");
    }

    return NULL;
}

void archer_thread_wakeup(archer_task_t *task)
{
    archer_thread_t *thread_p = (archer_thread_t *)task->thread_p;

    if(!thread_p->work_avail)
    {
        thread_p->work_avail = 1;

        wake_up_interruptible(&thread_p->rx_thread_wqh);
    }
}

static void archer_thread_create(archer_thread_t *thread_p,
                                 archer_thread_fn thread_fn,
                                 char *thread_name)
{
    init_waitqueue_head(&thread_p->rx_thread_wqh);

    thread_p->rx_thread = kthread_create(thread_fn, thread_p, thread_name);

    wake_up_process(thread_p->rx_thread);
}

static int archer_thread_handler(void *arg)
{
    archer_thread_t *thread_p = (archer_thread_t *)arg;

    while(1)
    {
        wait_event_interruptible(thread_p->rx_thread_wqh,
                                 thread_p->work_avail);

        if(kthread_should_stop())
        {
            __logError("kthread_should_stop detected");

            break;
        }

        if(archer_task_loop(&thread_p->work_avail))
        {
            /* We have exhausted our budget, yield the CPU to other threads */
            schedule();
        }
    }

    return 0;
}

/*******************************************************************************
 *
 * Initialization and IOCTL
 *
 *******************************************************************************/

static void archer_ioctl_flow_dump(archer_ioctl_cmd_t cmd, unsigned long arg)
{
    sysport_classifier_flow_dump_t dump;
    sysport_rsb_flow_type_t flow_type;

    switch(cmd)
    {
        case ARCHER_IOC_FLOWS:
            dump.flow_type = SYSPORT_RSB_FLOW_TYPE_UNKNOWN;
            break;

        case ARCHER_IOC_UCAST_L3:
            dump.flow_type = SYSPORT_RSB_FLOW_TYPE_UCAST_L3;
            break;

        case ARCHER_IOC_UCAST_L2:
            dump.flow_type = SYSPORT_RSB_FLOW_TYPE_UCAST_L2;
            break;

        case ARCHER_IOC_MCAST:
            dump.flow_type = SYSPORT_RSB_FLOW_TYPE_MCAST;
            break;

        default:
            BCM_ASSERT(0);
    }

    dump.max = arg;
    if(!dump.max)
    {
        dump.max = SYSPORT_UCAST_FLOW_INDEX_MAX;
    }

    sysport_classifier_flow_table_dump(&dump);

    __print("---------------------------------------------\n");
    for(flow_type=0; flow_type<SYSPORT_RSB_FLOW_TYPE_MAX; ++flow_type)
    {
        __print("\t%09s: %d out of %d Flows\n",
                (SYSPORT_RSB_FLOW_TYPE_UNKNOWN == flow_type) ?
                "TOTAL" : sysport_rsb_flow_type_name[flow_type],
                dump.stats[flow_type].shown, dump.stats[flow_type].found);
    }
    __print("---------------------------------------------\n\n");
}

/*
 *------------------------------------------------------------------------------
 * Function Name: archer_ioctl
 * Description  : Main entry point to handle user applications IOCTL requests.
 * Returns      : 0 - success or error
 *------------------------------------------------------------------------------
 */
static long archer_ioctl(struct file *filep, unsigned int command, unsigned long arg)
{
    archer_ioctl_cmd_t cmd = (command >= ARCHER_IOC_MAX) ?
        ARCHER_IOC_MAX : (archer_ioctl_cmd_t)command;
    int ret = 0;

    __logDebug("cmd %s (%d), arg<0x%08lX>",
               archer_ioctl_cmd_name[cmd - ARCHER_IOC_STATUS], cmd, arg);

    switch(cmd)
    {
        case ARCHER_IOC_STATUS:
            __print("ARCHER Status:\n"
                    "\tAcceleration %s, Active <%u>, Max <%u>, IPv6 %s\n"
                    "\tActivates           : %u\n"
                    "\tActivate Overflows  : %u\n"
                    "\tActivate Failures   : %u\n"
                    "\tDeactivates         : %u\n"
                    "\tDeactivate Failures : %u\n"
                    "\tFlushes             : %u\n\n",
                    (archer_driver_g.status == 1) ?  "Enabled" : "Disabled",
                    archer_driver_g.active, SYSPORT_UCAST_FLOW_MAX,
#if defined(CONFIG_BLOG_IPV6)
                    "Enabled",
#else
                    "Disabled",
#endif
                    archer_driver_g.activates, archer_driver_g.activate_overflows,
                    archer_driver_g.activate_failures, archer_driver_g.deactivates,
                    archer_driver_g.deactivate_failures, archer_driver_g.flushes);
            cmdlist_print_stats(NULL);
            __print("\n");
            break;

        case ARCHER_IOC_BIND:
            archer_fc_bind();
            break;

        case ARCHER_IOC_UNBIND:
            archer_fc_unbind();
            break;

        case ARCHER_IOC_DEBUG:
            bcmLog_setLogLevel(BCM_LOG_ID_ARCHER, arg);
            bcmLog_setLogLevel(BCM_LOG_ID_CMDLIST, arg);
            break;

        case ARCHER_IOC_FLOWS:
        case ARCHER_IOC_UCAST_L3:
        case ARCHER_IOC_UCAST_L2:
        case ARCHER_IOC_MCAST:
            archer_ioctl_flow_dump(cmd, arg);
            break;

        case ARCHER_IOC_HOST:
            archer_host_info_dump();
            break;

        case ARCHER_IOC_MODE:
            archer_host_mode_set(arg);
            break;

        case ARCHER_IOC_STATS:
#if defined(CONFIG_BCM94908) || defined(CONFIG_BCM963268) || \
    (defined(CONFIG_BCM963178) && (defined(CONFIG_BCM_XTMRT) || defined(CONFIG_BCM_XTMRT_MODULE)))
            iudma_driver_stats();
#endif
#if defined(CONFIG_BCM963178) || defined(CONFIG_BCM947622) || defined(CONFIG_BCM963158)
            sysport_driver_stats();
#endif
            archer_wlan_stats();
            break;

        case ARCHER_IOC_SYSPORT:
            switch(arg)
            {
                case ARCHER_SYSPORT_CMD_REG_DUMP:
                    sysport_driver_reg_dump();
                    break;

                case ARCHER_SYSPORT_CMD_PORT_DUMP:
                    sysport_driver_port_dump();
                    break;

                default:
                    __logError("Invalid SYSPORT command: %d", arg);
            }
            break;

#if defined(CONFIG_BCM947622)
        case ARCHER_IOC_MPDCFG:
        {
            archer_mpd_cfg_t mpd_cfg;

            copy_from_user (&mpd_cfg, (void *)arg, sizeof(archer_mpd_cfg_t));

            sysport_wol_mpd_cfg (&mpd_cfg);
        }
        break;

        case ARCHER_IOC_WOL:
        {
            char intf_name[16];

            copy_from_user (&intf_name, (void *)arg, 16);

            sysport_wol_enter(intf_name);
        }
        break;
#endif // CONFIG_BCM947622

        case ARCHER_IOC_DPI:
            ret = archer_dpi_command(arg);
            break;
#if (defined(CONFIG_BCM_XTMRT) || defined(CONFIG_BCM_XTMRT_MODULE))
        case ARCHER_IOC_XTMDROPALG_SET:
        {
            archer_dropalg_config_t cfg;

            copy_from_user (&cfg, (void *)arg, sizeof(archer_dropalg_config_t));

            iudma_tx_dropAlg_set(&cfg);
        }
        break;

        case ARCHER_IOC_XTMDROPALG_GET:
        {
            archer_dropalg_config_t cfg;

            copy_from_user (&cfg, (void *)arg, sizeof(archer_dropalg_config_t));

            iudma_tx_dropAlg_get(&cfg);

            copy_to_user((void *)arg, &cfg, sizeof(archer_dropalg_config_t));
        }
        break;
#endif
        default:
            __logError("Invalid Command [%u]", command);
            ret = -1;
    }

    return ret;
}

/*
 *------------------------------------------------------------------------------
 * Function Name: archer_open
 * Description  : Called when an user application opens this device.
 * Returns      : 0 - success
 *------------------------------------------------------------------------------
 */
static int archer_open(struct inode *inode, struct file *filp)
{
    __logDebug("Archer Open");

    return 0;
}

/* Global file ops */
static struct file_operations archer_fops =
{
    .unlocked_ioctl = archer_ioctl,
#if defined(CONFIG_COMPAT)
    .compat_ioctl = archer_ioctl,
#endif
    .open = archer_open,
};

#if defined(CONFIG_BCM_ARCHER_GSO)
static int archer_dev_open(struct net_device *dev)
{
    __print("Open %s Netdevice\n", dev->name);

    return 0;
}

static int archer_dev_change_mtu(struct net_device *dev, int new_mtu)
{
    if(new_mtu < ETH_ZLEN || new_mtu > ENET_MAX_MTU_PAYLOAD_SIZE)
    {
        return -EINVAL;
    }

    dev->mtu = new_mtu;

    return 0;
}

static netdev_tx_t archer_dev_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    archer_driver_gso_t *gso_p = &archer_driver_g.gso;

    if(likely(gso_p->nbuff_count < gso_p->nbuff_max))
    {
        dev->stats.tx_packets++;
        dev->stats.tx_bytes += skb->len;

        gso_p->pNBuff_list[gso_p->nbuff_count++] = SKBUFF_2_PNBUFF(skb);
    }
    else
    {
        dev->stats.tx_dropped++;

#if defined(CONFIG_BCM_GLB_COHERENCY)
        nbuff_free(SKBUFF_2_PNBUFF(skb));
#else
        nbuff_flushfree(SKBUFF_2_PNBUFF(skb));
#endif
    }

//    printk("%s,%d: count %d, max %d\n", __FUNCTION__, __LINE__, gso_p->nbuff_count, gso_p->nbuff_max);

    return NETDEV_TX_OK;
}

void archer_gso(pNBuff_t pNBuff, int nbuff_max, pNBuff_t *pNBuff_list, int *nbuff_count_p)
{
    archer_driver_gso_t *gso_p = &archer_driver_g.gso;

    if(IS_SKBUFF_PTR(pNBuff))
    {
        struct sk_buff *skb = PNBUFF_2_SKBUFF(pNBuff);

        gso_p->skb_dev_orig = skb->dev;
        gso_p->pNBuff_list = pNBuff_list;
        gso_p->nbuff_count = 0;
        gso_p->nbuff_max = nbuff_max;

        gso_p->dev->stats.rx_packets++;
        gso_p->dev->stats.rx_bytes += skb->len;

        // Send SKB through the Linux stack and let it do GSO
        skb->dev = gso_p->dev;

        dev_queue_xmit(skb);

//        printk("%s,%d: count %d\n", __FUNCTION__, __LINE__, gso_p->nbuff_count);

        *nbuff_count_p = gso_p->nbuff_count;
    }
    else
    {
        pNBuff_list[0] = pNBuff;

        *nbuff_count_p = 1;
    }
}

static const struct net_device_ops archer_netdev_ops_g =
{
    .ndo_open = archer_dev_open,
    .ndo_start_xmit = archer_dev_start_xmit,
    .ndo_change_mtu = archer_dev_change_mtu
};

static struct net_device * __init archer_create_netdevice(void)
{
    struct net_device *dev;
    int ret;

    dev = alloc_netdev(sizeof(archer_netdevice_t), "archer",
                       NET_NAME_UNKNOWN, ether_setup);
    if(!dev)
    {
        __logError("Failed to allocate Archer netdev\n");

        return NULL;
    }

    dev->watchdog_timeo = 2 * HZ;

    netif_carrier_off(dev);
    netif_stop_queue(dev);

    dev->netdev_ops = &archer_netdev_ops_g;

    dev->destructor = free_netdev;

    dev->tx_queue_len = 0;

    rtnl_lock();

    ret = register_netdevice(dev);
    if(ret)
    {
        __logError("Failed to register Archer netdev\n");

        rtnl_unlock();

        free_netdev(dev);

        return NULL;
    }
    else
    {
        __logDebug("Registered Archer netdev\n");
    }

    archer_dev_change_mtu(dev, BCM_ENET_DEFAULT_MTU_SIZE);

    netif_start_queue(dev);
    netif_carrier_on(dev);

    ret = dev_open(dev);

    rtnl_unlock();

    return dev;
}

static void __exit archer_remove_netdevice(struct net_device *dev)
{
    __logDebug("Unregister %s netdev\n", dev->name);
    
    rtnl_lock();

    unregister_netdevice(dev);

    rtnl_unlock();
}
#endif /* CONFIG_BCM_ARCHER_GSO */

/*
*******************************************************************************
* Function   : archer_construct
* Description: Constructs the Archer Driver
*******************************************************************************
*/
int __init archer_construct(void)
{
    cmdlist_hooks_t cmdlist_hooks;
    int ret;

    printk(CLRcb "Broadcom Archer Packet Accelerator Intializing" CLRnl);

#if defined(CC_ARCHER_SIM_FC_HOOK)
    bcmLog_setLogLevel(BCM_LOG_ID_ARCHER, BCM_LOG_LEVEL_DEBUG);
    bcmLog_setLogLevel(BCM_LOG_ID_CMDLIST, BCM_LOG_LEVEL_DEBUG);
#else
    bcmLog_setLogLevel(BCM_LOG_ID_ARCHER, BCM_LOG_LEVEL_ERROR);
#endif

    memset(&archer_driver_g, 0, sizeof(archer_driver_t));

#if defined(CONFIG_BCM_ARCHER_GSO)
    archer_driver_g.gso.dev = archer_create_netdevice();
    if(!archer_driver_g.gso.dev)
    {
        return -1;
    }
#endif

    ret = archer_dummy_device_alloc();
    if(ret)
    {
        return ret;
    }

#if !defined(CONFIG_BCM_ARCHER_SIM)
    archer_task_construct();

    archer_thread_create(&archer_thread_us_g, archer_thread_handler, "bcm_archer_us");

    archer_socket_construct();

    archer_dpi_construct();

    ret = archer_wlan_construct();
    if(ret)
    {
        __logError("Could not archer_wlan_construct");

        return ret;
    }

#if defined(CONFIG_BCM963178) || defined(CONFIG_BCM947622) || defined(CONFIG_BCM963158)
    ret = sysport_driver_construct();
    if(ret)
    {
        return ret;
    }
#endif

#if defined(CONFIG_BCM94908) || defined(CONFIG_BCM963268) || \
    (defined(CONFIG_BCM963178) && (defined(CONFIG_BCM_XTMRT) || defined(CONFIG_BCM_XTMRT_MODULE)))
    ret = iudma_driver_construct();
    if(ret)
    {
        return ret;
    }
#endif
#endif

    ret = archer_host_construct();
    if(ret)
    {
        return ret;
    }

    ret = sysport_classifier_construct();
    if(ret)
    {
        archer_dummy_device_free();

        return ret;
    }

    cmdlist_hooks.ipv6_addresses_table_add = NULL;
    cmdlist_hooks.ipv4_addresses_table_add = archer_ucast_ipv4_addresses_table_add;
    cmdlist_hooks.brcm_tag_info = archer_driver_brcm_tag_info;
    cmdlist_bind(&cmdlist_hooks);

#if defined(CONFIG_BCM_ARCHER_SIM)
    archer_sim_init();
#endif

#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
    archer_iq_register();
#endif

    archer_fc_bind();

    ret = register_chrdev(ARCHER_DRV_MAJOR, ARCHER_DRV_NAME, &archer_fops);
    if(ret)
    {
        __logError("Unable to get major number <%d>", ARCHER_DRV_MAJOR);

        return ret;
    }

    __print(CLRcb ARCHER_MODNAME " Char Driver " ARCHER_VER_STR " Registered <%d>" CLRnl, ARCHER_DRV_MAJOR);

    bcmFun_reg(BCM_FUN_ID_ARCHER_HOST_BIND, archer_driver_host_bind);
    bcmFun_reg(BCM_FUN_ID_ENET_SYSPORT_CONFIG, sysport_driver_host_config);
    bcmFun_reg(BCM_FUN_ID_ENET_SYSPORT_QUEUE_MAP, sysport_driver_queue_map);
    bcmFun_reg(BCM_FUN_ID_ENET_BOND_RX_PORT_MAP, sysport_driver_lookup_port_map);

    blog_tcp_ack_mflows_set_fn = archer_driver_tcp_ack_mflows_set;
    archer_driver_tcp_ack_mflows_set(blog_support_get_tcp_ack_mflows());

    return 0;
}

/*
*******************************************************************************
* Function   : archer_destruct
* Description: Destructs the Archer Driver
*******************************************************************************
*/
void __exit archer_destruct(void)
{
    blog_tcp_ack_mflows_set_fn = NULL;

#if defined(CONFIG_BCM_ARCHER_GSO)
    archer_remove_netdevice(archer_driver_g.gso.dev);
#endif

    archer_fc_unbind();

#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
    archer_iq_deregister();
#endif

    cmdlist_unbind();

    sysport_classifier_destruct();

    archer_dummy_device_free();

    archer_host_destruct();
}

module_init(archer_construct);
module_exit(archer_destruct);

EXPORT_SYMBOL(archer_dpi_enable);
EXPORT_SYMBOL(archer_dpi_ds_queue_config);
EXPORT_SYMBOL(archer_dpi_ds_port_config);

MODULE_DESCRIPTION(ARCHER_MODNAME);
MODULE_VERSION(ARCHER_VERSION);
MODULE_LICENSE("Proprietary");
