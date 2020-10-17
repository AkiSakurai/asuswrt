/*
  <:copyright-BRCM:2019:proprietary:standard

  Copyright (c) 2019 Broadcom 
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
* File Name  : archer_iq.c
*
* Description: This file implements Ingress QoS for Archer.
*
*******************************************************************************
*/

/* -----------------------------------------------------------------------------
 *                      Ingress QoS (IQ)
 * -----------------------------------------------------------------------------
 * Ingress QoS feature defines a flow has low or high ingress priority at an 
 * ingress interface based on a pre-defined criterion or the layer4 (TCP, UDP)
 * destination port. Under normal load conditions all received packets are
 * accepted and forwarded. But under CPU congestion high ingress priority are
 * accepted whereas low ingress priority packets are dropped.
 *
 * CPU congestion detection:
 * -------------------------
 * Ingress QoS constantly monitors the queue depth for an interface to detect
 * CPU congestion. If the queue depth is greater than the IQ high threshold,
 * CPU congestion has set in. When the queue depth is less than IQ low
 * threshold, then CPU congestion has abated.  
 *
 * CPU Congestion Set:
 * -------------------
 * When the CPU is congested only high ingress priority are accepted 
 * whereas low ingress priority packets are dropped.
 *
 * These are some of the pre-defined criterion for a flow to be 
 * high ingress priority:
 * a) High Priority Interface
 *    - Any packet received from XTM 
 *    - Any packet received from or sent to WLAN 
 * b) Multicast
 * c) Flows configured by default through the following ALGs
 *    - SIP (default SIP UDP port = 5060), RTSP ports for data
 *    - RTSP (default UDP port = 554), RTSP ports for data
 *    - H.323 (default UDP port = 1719, 1720)
 *    - MGCP (default UDP ports 2427 and 2727)
 *    Note:- The above ALGs are not invoked for bridging mode. Therefore 
 *           in bridging mode, user code needs to call APIs such as
 *           fap_iqos_add_L4port() to classify a L4 port as high.
 *      
 * d) Other flows configured through Ingress QoS APIs:
 *    - DNS  UDP port = 53  
 *    - DHCP UDP port = 67  
 *    - DHCP UDP port = 68  
 *    - HTTP TCP port = 80, and 8080  
 *    Note:- If required user code can add more flows in the iq_init() or
 *           use the fap_iqos_add_L4port().
 *
 * Note: API prototypes are given in iqos.h file.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bcm_log.h>
#include <linux/blog.h>

#include <linux/iqos.h>
#include <ingqos.h>

#include "sysport_rsb.h"
#include "sysport_classifier.h"
#include "archer.h"
#include "archer_driver.h"

#include <archer_cpu_queues.h>

//#define CC_ARCHER_IQ_DEBUG

#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))

/*******************************************************************
 *
 * Internal prototypes
 *
 *******************************************************************/
typedef struct {

    uint16_t port;          /* L4 dest port */
    uint8_t  is_static : 1, /* static entry */
             unused    : 4,
             prio      : 3; /* priority */
    uint8_t  next_idx;      /* overflow bucket index */

} archer_iq_l4port_entry_t;

typedef struct {

    uint8_t  free_count;
    uint8_t  next_idx;

} archer_iq_free_ovllst_t;

typedef enum {
    ARCHER_IQ_L4PROTO_TCP,
    ARCHER_IQ_L4PROTO_UDP,

    ARCHER_IQ_L4PROTO_MAX,

} archer_iq_L4proto_t;

#define ARCHER_IQ_L4PROTO_IDX(ipproto)  \
    ((ipproto == BLOG_IPPROTO_UDP) ? ARCHER_IQ_L4PROTO_UDP : ARCHER_IQ_L4PROTO_TCP)

/* size of hash table */
#define ARCHER_IQ_HASH_TBL_SIZE     64
#define ARCHER_IQ_OVFTBL_SIZE       64

typedef struct {

    uint8_t field;
    uint8_t next_idx : 4,
            prio     : 4;

} archer_iq_generic_entry_t;

#define ARCHER_IQ_MAX_GENERIC_ENT   16  /* only 4 bits is used for indexing, maximum list size is 16 */

typedef struct {

    uint8_t free_list;
    uint8_t valid_list;
    archer_iq_generic_entry_t   ent[ARCHER_IQ_MAX_GENERIC_ENT];

} archer_iq_gen_t;


typedef struct {

    int enable;

    /* data structures for filtering of L4 dst ports */
    archer_iq_l4port_entry_t    htbl[ARCHER_IQ_L4PROTO_MAX][ARCHER_IQ_HASH_TBL_SIZE];
    archer_iq_l4port_entry_t    ovltbl[ARCHER_IQ_L4PROTO_MAX][ARCHER_IQ_OVFTBL_SIZE];

    archer_iq_free_ovllst_t     free_ovllst[ARCHER_IQ_L4PROTO_MAX];

    /* data structures for filtering of IP protocols */
    archer_iq_gen_t         ipproto_tbl;

    /* data structures for filtering of DSCP */
    archer_iq_gen_t         dscp_tbl;

} archer_iq_t;

static archer_iq_t  archer_iq_g;

/*******************************************************************
 *
 * Local Functions
 *
 *******************************************************************/
/*
 *------------------------------------------------------------------------------
 * Function     : _hash 
 * Description  : Computes a simple hash from a 32bit value.
 *------------------------------------------------------------------------------
 */
static inline
uint32_t _hash( uint32_t hash_val )
{
    hash_val ^= ( hash_val >> 16 );
    hash_val ^= ( hash_val >>  8 );
    hash_val ^= ( hash_val >>  3 );
    return ( hash_val );
}


static uint8_t archer_iq_hash( uint32_t port )
{
    uint8_t hashIx = (uint8_t) _hash(port) % ARCHER_IQ_HASH_TBL_SIZE;

    /* if hash happens to be 0, make it 1 */
    if (hashIx == 0 ) 
        hashIx = 1;

    return hashIx;
}

static inline uint8_t archer_iq_alloc_ovlent (int l4proto_idx)
{
    uint8_t ovl_idx = 0;

    archer_iq_t *archer_iq = &archer_iq_g;

    if (archer_iq->free_ovllst[l4proto_idx].next_idx == 0)
    {
        // error checking only
        BCM_ASSERT(archer_iq->free_ovllst[l4proto_idx].free_count == 0)
    }
    else
    {
        ovl_idx = archer_iq->free_ovllst[l4proto_idx].next_idx;

        archer_iq->free_ovllst[l4proto_idx].next_idx = archer_iq->ovltbl[l4proto_idx][ovl_idx].next_idx;
        archer_iq->free_ovllst[l4proto_idx].free_count--;
    }
    return ovl_idx;
}

static int archer_iq_add_L4port (int l4proto_idx, uint16_t dport, uint8_t is_static, uint8_t prio)
{
    int addIdx = -1;
    uint8_t hashIdx, nextIdx;
    archer_iq_t *archer_iq = &archer_iq_g;

    hashIdx = archer_iq_hash (dport);

    if (archer_iq->htbl[l4proto_idx][hashIdx].port == dport)
    {
        /* already have an entry */
        if (archer_iq->htbl[l4proto_idx][hashIdx].is_static != is_static)
        {
            /* if the is_static entry is different, either the one of the setting request will be static */
            archer_iq->htbl[l4proto_idx][hashIdx].is_static = 1;
        }
        if (archer_iq->htbl[l4proto_idx][hashIdx].prio != prio)
        {
            /* pick the highest priority */
            if (archer_iq->htbl[l4proto_idx][hashIdx].prio < prio)
            {
                archer_iq->htbl[l4proto_idx][hashIdx].prio = prio;
            }
        }
        addIdx = hashIdx;
    }
    else if (archer_iq->htbl[l4proto_idx][hashIdx].port == 0)
    {
        /* new entry in the hash table */
        archer_iq->htbl[l4proto_idx][hashIdx].port = dport;
        archer_iq->htbl[l4proto_idx][hashIdx].prio = prio;
        archer_iq->htbl[l4proto_idx][hashIdx].is_static = is_static;
        archer_iq->htbl[l4proto_idx][hashIdx].next_idx = 0;
        addIdx = hashIdx;
    }
    else
    {
        uint8_t ovl_idx = 0;

        /* hash table entry occupied, search if a matching entry exists in overflow table */
        nextIdx = archer_iq->htbl[l4proto_idx][hashIdx].next_idx;

        while (nextIdx)
        {
            if (archer_iq->ovltbl[l4proto_idx][nextIdx].port == dport)
            {
                if (archer_iq->ovltbl[l4proto_idx][nextIdx].is_static != is_static)
                {
                    /* if the is_static entry is different, either the one of the setting request will be static */
                    archer_iq->ovltbl[l4proto_idx][nextIdx].is_static = 1;
                }
                if (archer_iq->ovltbl[l4proto_idx][nextIdx].prio != prio)
                {
                    /* pick the highest priority */
                    if (archer_iq->ovltbl[l4proto_idx][nextIdx].prio < prio)
                    {
                        archer_iq->ovltbl[l4proto_idx][nextIdx].prio = prio;
                    }
                }
                ovl_idx = nextIdx;
                addIdx = ovl_idx;    
            }
            nextIdx = archer_iq->ovltbl[l4proto_idx][addIdx].next_idx;
        }

        if (ovl_idx == 0)
        {
            /* no matching entry found in overflow list, need to allocate one */
            ovl_idx = archer_iq_alloc_ovlent(l4proto_idx);
            if (ovl_idx)
            {
                archer_iq->ovltbl[l4proto_idx][ovl_idx].next_idx = 0;
                archer_iq->ovltbl[l4proto_idx][ovl_idx].port = dport;
                archer_iq->ovltbl[l4proto_idx][ovl_idx].is_static = is_static;
                archer_iq->ovltbl[l4proto_idx][ovl_idx].prio = prio;

                /* look for the end of the list to add this entry */
                if (archer_iq->htbl[l4proto_idx][hashIdx].next_idx)
                {
                    nextIdx = archer_iq->htbl[l4proto_idx][hashIdx].next_idx;
                    while (archer_iq->ovltbl[l4proto_idx][nextIdx].next_idx)
                    {
                        nextIdx = archer_iq->ovltbl[l4proto_idx][nextIdx].next_idx;
                    }

                    archer_iq->ovltbl[l4proto_idx][nextIdx].next_idx = ovl_idx;
                }
                else
                {
                    archer_iq->htbl[l4proto_idx][hashIdx].next_idx = ovl_idx;
                }
                addIdx = ovl_idx;
            }
        }
    }

    __logInfo("Archer IQ add L4 port proto_idx %d dport %d prio %d returns %d\n", l4proto_idx, 
        dport, prio, addIdx);

    return addIdx;
}

static int archer_iq_remove_L4port (int l4proto_idx, uint16_t dport, uint8_t is_static)
{
    int ret = -1;
    uint8_t hashIdx, nextIdx;
    archer_iq_t *archer_iq = &archer_iq_g;

    hashIdx = archer_iq_hash (dport);
 
    if (archer_iq->htbl[l4proto_idx][hashIdx].port == 0)
    {
        __logError ("requested ipProto %s port %d not found\n", (l4proto_idx == ARCHER_IQ_L4PROTO_UDP) ? "UDP" : "TCP", dport);
    }
    else if ((archer_iq->htbl[l4proto_idx][hashIdx].port == dport) &&
             (archer_iq->htbl[l4proto_idx][hashIdx].is_static == is_static))
    {
        /* found entry in the hash table */
        ret = hashIdx;
        nextIdx = archer_iq->htbl[l4proto_idx][hashIdx].next_idx;

        if (nextIdx)
        {
            /* move the overflow entries to the main hash table */
            archer_iq->htbl[l4proto_idx][hashIdx].next_idx = archer_iq->ovltbl[l4proto_idx][nextIdx].next_idx;
            archer_iq->htbl[l4proto_idx][hashIdx].port = archer_iq->ovltbl[l4proto_idx][nextIdx].port;
            archer_iq->htbl[l4proto_idx][hashIdx].prio = archer_iq->ovltbl[l4proto_idx][nextIdx].prio;
            archer_iq->htbl[l4proto_idx][hashIdx].is_static = archer_iq->ovltbl[l4proto_idx][nextIdx].is_static;

            /* return the overflow entry to the head of the free list */

            archer_iq->ovltbl[l4proto_idx][nextIdx].next_idx = archer_iq->free_ovllst[l4proto_idx].next_idx;
            archer_iq->ovltbl[l4proto_idx][nextIdx].port = 0;
            archer_iq->free_ovllst[l4proto_idx].next_idx = nextIdx;
            archer_iq->free_ovllst[l4proto_idx].free_count++;
        }
        else
        {
            /* nothing on the overflow list, just mark the hash table as unused */
            archer_iq->htbl[l4proto_idx][hashIdx].port = 0;
        }
    }
    else
    {
        uint8_t remIdx = 0;
        uint8_t prvIdx = 0;

        /* entry not in main hash table, search the overflow list */
        nextIdx = archer_iq->htbl[l4proto_idx][hashIdx].next_idx;

        while(nextIdx)
        {
            if ((archer_iq->ovltbl[l4proto_idx][nextIdx].port == dport) &&
                (archer_iq->ovltbl[l4proto_idx][nextIdx].is_static == is_static))
            {
                remIdx = nextIdx;
                break;
            }
            prvIdx = nextIdx;
            nextIdx = archer_iq->ovltbl[l4proto_idx][nextIdx].next_idx;
        }

        if(remIdx)
        {
            /* entry found in overflow list */
            if(prvIdx)
            {
                archer_iq->ovltbl[l4proto_idx][prvIdx].next_idx = archer_iq->ovltbl[l4proto_idx][remIdx].next_idx;
            }
            else
            {
                /* first overflow entry */
                archer_iq->htbl[l4proto_idx][hashIdx].next_idx = archer_iq->ovltbl[l4proto_idx][remIdx].next_idx;
            }

            /* return the found entry to the overflow list */
            archer_iq->ovltbl[l4proto_idx][remIdx].next_idx = archer_iq->free_ovllst[l4proto_idx].next_idx;
            archer_iq->ovltbl[l4proto_idx][remIdx].port = 0;
            archer_iq->free_ovllst[l4proto_idx].next_idx = remIdx;
            archer_iq->free_ovllst[l4proto_idx].free_count++;
            ret = remIdx;
        }
    }

    __logInfo("Archer IQ remove L4 port proto_idx %d dport %d returns %d\n", l4proto_idx, dport, ret);
    return ret;
}

static int archer_iq_get_entry_prio (archer_iq_gen_t *tbl, uint8_t field, uint8_t *retPrio)
{
    int ret = -1;
    uint8_t valid_idx;
    
    valid_idx = tbl->valid_list;

    while (valid_idx)
    {
        if (tbl->ent[valid_idx].field == field)
        {
            ret = valid_idx;
            *retPrio = tbl->ent[valid_idx].prio;
            break;
        }
        valid_idx = tbl->ent[valid_idx].next_idx;
    }

    return ret;
}

static int archer_iq_add_prio (archer_iq_gen_t *tbl, uint8_t field, uint8_t prio)
{
    int ret = -1;
    uint8_t add_idx, retPrio;

    /* check if the requested protocol is already on the list */
    ret = archer_iq_get_entry_prio (tbl, field, &retPrio);
    if(ret > 0)
    {
        /* requested priority has changed */
        if(retPrio != prio)
        {
            tbl->ent[ret].prio = prio;
        }
    }
    else
    {
        /* create a new entry in the protocol list */
        if (tbl->free_list)
        {
            /* allocate 1 free entry */
            add_idx = tbl->free_list;
            tbl->free_list = tbl->ent[add_idx].next_idx;

            /* add to the front of the valid list */
            tbl->ent[add_idx].next_idx = tbl->valid_list;
            tbl->ent[add_idx].field = field;
            tbl->ent[add_idx].prio = prio;
            tbl->valid_list = add_idx;

            ret = add_idx;
        }
    }

    return ret;
}

static int archer_iq_remove_prio (archer_iq_gen_t *tbl, uint8_t field)
{
    int ret = -1;
    uint8_t valid_idx, prvIdx = 0;
    
    valid_idx = tbl->valid_list;

    while (valid_idx)
    {
        if (tbl->ent[valid_idx].field == field)
        {
            ret = valid_idx;
            break;
        }
        prvIdx = valid_idx;
        valid_idx = tbl->ent[valid_idx].next_idx;
    }

    if (valid_idx)
    {
        ret = valid_idx;
        /* entry found, remove it */
        if (prvIdx)
        {
            /* entry in the middle of valid list */
            tbl->ent[prvIdx].next_idx = tbl->ent[valid_idx].next_idx;
        }
        else
        {
            /* first entry of valid list */
            tbl->valid_list = tbl->ent[valid_idx].next_idx;
        }
        /* return the entry to the free_list */
        tbl->ent[valid_idx].next_idx = tbl->free_list;
        tbl->ent[valid_idx].field = 0xFF;
        tbl->free_list = valid_idx;
    }

    return ret;
}

int archerIq_add_entry(void *iq_param)
{
    iq_param_t *param = (iq_param_t *)iq_param;

    uint32_t key_mask = param->key_mask;
    iq_key_data_t *key_data = &param->key_data;
    uint8_t ipProto = key_data->ip_proto;
    uint16_t dport  = key_data->l4_dst_port;
    uint8_t dscp  = key_data->dscp;
    int ret = -1;

    if ((key_mask ^ (IQ_KEY_MASK_DST_PORT | IQ_KEY_MASK_IP_PROTO)) == 0)
    {
        /* for L4 dst Port we only support TCP and UDP for now */
        if ((ipProto == BLOG_IPPROTO_UDP) || (ipProto == BLOG_IPPROTO_TCP))
        {
            ret = archer_iq_add_L4port (ARCHER_IQ_L4PROTO_IDX(ipProto), dport, param->action.is_static, param->action.value);
        }
    }

    if ((key_mask ^ IQ_KEY_MASK_IP_PROTO) == 0)
    {
        /* filter on IP protocol */
        ret = archer_iq_add_prio (&archer_iq_g.ipproto_tbl, ipProto, param->action.value);
    }

    if ((key_mask ^ IQ_KEY_MASK_DSCP) == 0)
    {
        /* filter on DSCP */
        ret = archer_iq_add_prio (&archer_iq_g.dscp_tbl, dscp, param->action.value);
    }

    if (ret < 0)
    {
       __logError("Unable to add Archer IngQos Entry keymask 0x%x, ipProto %d dst port %d, dscp %d action 0x%x\n",
                key_mask, ipProto, dport, dscp, param->action.word);
    }
    return 0;
}

int archerIq_delete_entry(void *iq_param)
{
    iq_param_t *param = (iq_param_t *)iq_param;

    uint32_t key_mask = param->key_mask;
    iq_key_data_t *key_data = &param->key_data;
    uint8_t ipProto = key_data->ip_proto;
    uint8_t dscp = key_data->dscp;
    uint16_t dport  = key_data->l4_dst_port;
    int ret = -1;

    if ((key_mask ^ (IQ_KEY_MASK_DST_PORT | IQ_KEY_MASK_IP_PROTO)) == 0)
    {
        /* for L4 dst Port we only support TCP and UDP for now */
        if ((ipProto == BLOG_IPPROTO_UDP) || (ipProto == BLOG_IPPROTO_TCP))
        {
            ret = archer_iq_remove_L4port (ARCHER_IQ_L4PROTO_IDX(ipProto), dport, param->action.is_static);
        }
    }

    if ((key_mask ^ IQ_KEY_MASK_IP_PROTO) == 0)
    {
        /* filter on IP protocol */
        ret = archer_iq_remove_prio (&archer_iq_g.ipproto_tbl, ipProto);
    }

    if ((key_mask ^ IQ_KEY_MASK_DSCP) == 0)
    {
        /* filter on DSCP */
        ret = archer_iq_remove_prio (&archer_iq_g.dscp_tbl, dscp);
    }

    if (ret < 0)
    {
       __logError("Unable to remove Archer IngQos Entry keymask 0x%x, ipProto %d dst port %d, dscp %d action 0x%x\n",
                key_mask, ipProto, dport, dscp, param->action.word);
    }
    return 0;
}

int archerIq_setStatus(void *iq_param)
{
    iq_param_t *param = (iq_param_t *)iq_param;
    archer_iq_t *archer_iq = &archer_iq_g;
    uint32_t status = param->status;
    
    __print ("Archer IQ status changed from %d to %d\n", archer_iq->enable, status);
    archer_iq->enable = status;

    return 0;
}

int archerIq_dumpStatus(void *iq_param)
{
    archer_iq_t *archer_iq = &archer_iq_g;
    __print("Archer IQ status %d\n", archer_iq->enable);

    return 0;
}

int archerIq_dump_porttbl(void *iq_param)
{
    int protoIdx, idx;
    archer_iq_t *archer_iq = &archer_iq_g;

    for (protoIdx = 0; protoIdx < ARCHER_IQ_L4PROTO_MAX; protoIdx++)
    {
        __print("IP protocol %s hash table\n", (protoIdx == ARCHER_IQ_L4PROTO_UDP) ? "UDP" : "TCP" );

        for (idx = 0; idx < ARCHER_IQ_OVFTBL_SIZE; idx++)
        {
            if (archer_iq->htbl[protoIdx][idx].port)
            {
                __print("hashIdx %d port %d static %d prio %d next_idx %d\n",
                    idx,
                    archer_iq->htbl[protoIdx][idx].port,
                    archer_iq->htbl[protoIdx][idx].is_static,
                    archer_iq->htbl[protoIdx][idx].prio,
                    archer_iq->htbl[protoIdx][idx].next_idx);
            }
        }
        for (idx = 0; idx < ARCHER_IQ_OVFTBL_SIZE; idx++)
        {
            if (archer_iq->ovltbl[protoIdx][idx].port)
            {
                __print("ovltbl entry idx %d port %d static %d prio %d next_idx %d\n",
                    idx,
                    archer_iq->ovltbl[protoIdx][idx].port,
                    archer_iq->ovltbl[protoIdx][idx].is_static,
                    archer_iq->ovltbl[protoIdx][idx].prio,
                    archer_iq->ovltbl[protoIdx][idx].next_idx);
            }
        }
        __print("number of entries in freelist %d start_idx %d\n",
                archer_iq->free_ovllst[protoIdx].free_count,
                archer_iq->free_ovllst[protoIdx].next_idx);

#if defined (CC_ARCHER_IQ_DEBUG)
        idx = archer_iq->free_ovllst[protoIdx].next_idx;
        __print("free_ovllist -> %d", idx);
        while (idx)
        {
            idx = archer_iq->ovltbl[protoIdx][idx].next_idx;
            __print(" -> %d", idx);
        }
        __print("\n");
#endif
    }

    __print ("IP protocol valid list\n");
    idx = archer_iq->ipproto_tbl.valid_list;
    while (idx)
    {
        __print("idx %d ipproto %d prio %d\n", idx, 
            archer_iq->ipproto_tbl.ent[idx].field,
            archer_iq->ipproto_tbl.ent[idx].prio);

        idx = archer_iq->ipproto_tbl.ent[idx].next_idx;
    }

#if defined (CC_ARCHER_IQ_DEBUG)
    idx = archer_iq->ipproto_tbl.free_list;
    __print("ipproto list free -> %d", idx);

    while (idx)
    {
        idx = archer_iq->ipproto_tbl.ent[idx].next_idx;
        __print(" -> %d", idx);
    }
    __print("\n");
#endif

    __print ("DSCP valid list\n");
    idx = archer_iq->dscp_tbl.valid_list;
    while (idx)
    {
        __print("idx %d dscp %d prio %d\n", idx, 
            archer_iq->dscp_tbl.ent[idx].field,
            archer_iq->dscp_tbl.ent[idx].prio);

        idx = archer_iq->dscp_tbl.ent[idx].next_idx;
    }

#if defined (CC_ARCHER_IQ_DEBUG)
    idx = archer_iq->dscp_tbl.free_list;
    __print("dscp list free -> %d", idx);
    while (idx)
    {
        idx = archer_iq->dscp_tbl.ent[idx].next_idx;
        __print(" -> %d", idx);
    }
    __print("\n");
#endif

    return 0;
}


static const iq_hw_info_t archerIq_info_db = {
	.mask_capability = (IQ_KEY_MASK_IP_PROTO + IQ_KEY_MASK_DST_PORT + IQ_KEY_MASK_DSCP),
	.add_entry = archerIq_add_entry,
	.delete_entry = archerIq_delete_entry,
	.set_status = archerIq_setStatus,
	.get_status = archerIq_dumpStatus,
	.dump_table = archerIq_dump_porttbl,
};

static uint8_t archer_iq_lookup_L4port (uint8_t ipProto, uint16_t l4_dport)
{
    uint8_t retPrio = 0;
    uint8_t hashIdx, nextIdx, l4proto_idx;
    int found = 0;
    archer_iq_t *archer_iq = &archer_iq_g;

    hashIdx = archer_iq_hash (l4_dport);
    l4proto_idx = ARCHER_IQ_L4PROTO_IDX(ipProto);

    if (archer_iq->htbl[l4proto_idx][hashIdx].port == l4_dport)
    {
        retPrio = archer_iq->htbl[l4proto_idx][hashIdx].prio;
        found = 1;
    }
    else
    {
        /* not in main hash table, search overflow */
        nextIdx = archer_iq->htbl[l4proto_idx][hashIdx].next_idx;
        while (nextIdx)
        {
            if (archer_iq->ovltbl[l4proto_idx][nextIdx].port == l4_dport)
            {
                retPrio = archer_iq->htbl[l4proto_idx][hashIdx].prio;
                found = 1;
                break;
            }
            nextIdx = archer_iq->ovltbl[l4proto_idx][nextIdx].next_idx;
        }
    }
    return retPrio;
}

/*
*******************************************************************************
* Function   : archer_iq_register
* Description: register Archer (HW accelerator) with IngQos Driver
*******************************************************************************
*/
int __init archer_iq_register(void)
{
    int protoIdx, idx;
    archer_iq_t *archer_iq = &archer_iq_g;

    /* initialize data structures */
    for (protoIdx = 0; protoIdx < ARCHER_IQ_L4PROTO_MAX; protoIdx++)
    {
        memset (archer_iq->htbl[protoIdx], 0, sizeof(archer_iq->htbl[protoIdx]));
        memset (archer_iq->ovltbl[protoIdx], 0, sizeof(archer_iq->ovltbl[protoIdx]));
    }
    /* setup the overflow free entry list */
    for (protoIdx = 0; protoIdx < ARCHER_IQ_L4PROTO_MAX; protoIdx++)
    {
        for (idx = 1; idx < ARCHER_IQ_OVFTBL_SIZE-1; idx++)
        {
            archer_iq->ovltbl[protoIdx][idx].next_idx = idx+1;
        }
        archer_iq->ovltbl[protoIdx][ARCHER_IQ_OVFTBL_SIZE-1].next_idx = 0;

        archer_iq->free_ovllst[protoIdx].free_count = ARCHER_IQ_OVFTBL_SIZE-1;
        archer_iq->free_ovllst[protoIdx].next_idx = 1;
    }

    for (idx = 1; idx < ARCHER_IQ_MAX_GENERIC_ENT-1; idx++)
    {
        archer_iq->ipproto_tbl.ent[idx].next_idx = idx+1;
        archer_iq->ipproto_tbl.ent[idx].field = 0xFF; /* 0xFF is the reserved IP protocol */
    }
    archer_iq->ipproto_tbl.ent[ARCHER_IQ_MAX_GENERIC_ENT-1].next_idx = 0;
    archer_iq->ipproto_tbl.ent[ARCHER_IQ_MAX_GENERIC_ENT-1].field = 0xFF;
    archer_iq->ipproto_tbl.free_list = 1;
    archer_iq->ipproto_tbl.valid_list = 0;

    for (idx = 1; idx < ARCHER_IQ_MAX_GENERIC_ENT-1; idx++)
    {
        archer_iq->dscp_tbl.ent[idx].next_idx = idx+1;
        archer_iq->dscp_tbl.ent[idx].field = 0xFF; /* 0xFF is the reserved IP protocol */
    }
    archer_iq->dscp_tbl.ent[ARCHER_IQ_MAX_GENERIC_ENT-1].next_idx = 0;
    archer_iq->dscp_tbl.ent[ARCHER_IQ_MAX_GENERIC_ENT-1].field = 0xFF;
    archer_iq->dscp_tbl.free_list = 1;
    archer_iq->dscp_tbl.valid_list = 0;

    if (bcm_iq_register_hw ((iq_hw_info_t *)&archerIq_info_db))
    {
        __logError("failed to register Archer to Ingress QoS\n");
    }
    else
    {
        __logInfo("Complete registering Archer to Ingress QoS\n");
    }
    return 0;
}

/*
*******************************************************************************
* Function   : archer_iq_deregister
* Description: unregister Archer (HW accelerator) with IngQos Driver
*******************************************************************************
*/
void __exit archer_iq_deregister(void)
{
    bcm_iq_unregister_hw ((iq_hw_info_t *)&archerIq_info_db);
}

/*
*******************************************************************************
* Function   : archer_iq_get_l4dport_ipproto
* Description: extra L4 dst port and IP protocol from data packet
*******************************************************************************
*/
static void archer_iq_get_l4dport_ipproto(sysport_rsb_t *rsb_p, uint8_t *packet_p, uint16_t *dport, uint16_t *ipproto)
{
    /* L4 ports are in the same location for both TCP and UDP packets so the TCP pachet header is used */
    BlogTcpHdr_t *th_p;

    if(SYSPORT_RSB_L3_TYPE_IPV4 == rsb_p->l3_type ||
       SYSPORT_RSB_L3_TYPE_4IN6 == rsb_p->l3_type)
    {
        /* IPv4 packet */
        BlogIpv4Hdr_t *ip_p = (BlogIpv4Hdr_t *)(packet_p + rsb_p->ip_offset);
        int ihl = ip_p->ihl << 2;
        th_p = ((void *)ip_p) + ihl;
        *ipproto = ip_p->proto;
    }
    else
    {
        /* IPv6 packet */
        BlogIpv6Hdr_t *ip6_p = (BlogIpv6Hdr_t *)(packet_p + rsb_p->ip_offset);
        th_p = (BlogTcpHdr_t *)(ip6_p + 1);
        *ipproto = ip6_p->nextHdr;
    }
    *dport = ntohs(th_p->dPort);
}

#endif

/*
*******************************************************************************
* Function   : archer_iq_sort
* Description: sort incoming data to high / lo priority
*******************************************************************************
*/
uint8_t archer_iq_sort (sysport_rsb_t *rsb_p, uint8_t *packet_p)
{
#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
    uint8_t retPrio = 0; 

    archer_iq_t *archer_iq = &archer_iq_g;
    uint16_t dport = 0, proto = 0xFF, dscp;

    if (!archer_iq->enable)
    {
        return CPU_RX_LO;
    }
    else if (rsb_p->tuple.header.valid &&
        (rsb_p->tuple.header.flow_type == SYSPORT_RSB_FLOW_TYPE_MCAST))
    {
        retPrio = 1;
    }
    else if (rsb_p->l3_type != SYSPORT_RSB_L3_TYPE_UNKNOWN)
    {
        /* extract the l4 dport and ip protocol from packet */
        archer_iq_get_l4dport_ipproto (rsb_p, packet_p, &dport, &proto);

        /* check filtering on dscp */
        dscp = (rsb_p->ip_tos >> 2);
        archer_iq_get_entry_prio (&archer_iq->dscp_tbl, dscp, &retPrio);
    }

    if (!retPrio && (rsb_p->tcp || rsb_p->udp))
    {
        retPrio = archer_iq_lookup_L4port(proto, dport);
    }

    if (!retPrio && (rsb_p->l3_type != SYSPORT_RSB_L3_TYPE_UNKNOWN))
    {
        archer_iq_get_entry_prio (&archer_iq->ipproto_tbl, proto, &retPrio);
    }

    /* map priority to CPU queue ID fo network drivers */
    return ((retPrio > 0) ? CPU_RX_HI : CPU_RX_LO);
#else
    return CPU_RX_HI; /* default to high priority queue if ING_QOS not supported */
#endif
}

