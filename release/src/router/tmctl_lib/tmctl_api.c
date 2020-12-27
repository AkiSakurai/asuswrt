/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
<:label-BRCM:2012:proprietary:standard

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
 *
 ************************************************************************/

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "os_defs.h"


#include "tmctl_api.h"
#include "tmctl_api_trace.h"
#include "tmctl_ethsw.h"
#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858) || defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)
#include "tmctl_rdpa.h"
#endif
#include "tmctl_fap.h"
#include "tmctl_bcmtm.h"


#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
/* ----------------------------------------------------------------------------
 * This function converts device type and interface from TMCtl to RDPA format.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port/TCONT/LLID identifier.
 *    rdpaDev_p (OUT) rdpa device type.
 *    ifId_p (OUT) interface id.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */

static inline tmctl_ret_e tmctl_ifConvert(tmctl_devType_e devType,
                                          tmctl_if_t*     if_p,
                                          int*            rdpaDev_p,
                                          int*            ifId_p)
{
   int rdpaDev;
   int ifId;

   tmctl_ret_e ret = TMCTL_SUCCESS;

   switch (devType)
   {
      case TMCTL_DEV_ETH:
         rdpaDev = RDPA_IOCTL_DEV_PORT;
         ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, (rdpa_if*)&ifId, 1);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
            return ret;
         }
         break;

      case TMCTL_DEV_EPON:
         rdpaDev = RDPA_IOCTL_DEV_LLID;
         ifId = if_p->eponIf.llid;
         break;

      case TMCTL_DEV_GPON:
         rdpaDev = RDPA_IOCTL_DEV_TCONT;
         ifId = if_p->gponIf.tcontid;
         break;

      case TMCTL_DEV_SVCQ:
         rdpaDev = RDPA_IOCTL_DEV_NONE;
         ifId = 0;
         break;

      default:
         tmctl_error("tmctl_ifConvert: invalid device type %d", devType);
         return TMCTL_ERROR;
   }

   *rdpaDev_p = rdpaDev;
   *ifId_p = ifId;
   return ret;
}
#endif /* CHIP_6838 || CHIP_6848 || CHIP_6858 */


/* ----------------------------------------------------------------------------
 * This function initializes the basic TM settings for a port/tcont/llid based
 * on TM capabilities.
 *
 * Note that if the port had already been initialized, all its existing
 * configuration will be deleted before re-initialization.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port/TCONT/LLID identifier.
 *    cfgFlags (IN) Port TM initialization config flags.
 *                  See bit definitions in tmctl_api.h
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_portTmInit(tmctl_devType_e devType,
                             tmctl_if_t*     if_p,
                             uint32_t        cfgFlags)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_portTmInitTrace(devType, if_p, cfgFlags);

   tmctl_debug("Enter: devType=%d", devType);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      /* TMCTL must apply the same port setting to all the ports in LAG/Trunk group */
      int      idx = 0;
      uint32_t schedType;
      rdpa_if  rdpaIf;
      rdpa_if  rdpaIfArray[TMCTL_MAX_PORTS_IN_LAG_GRP] =
                                {[0 ... (TMCTL_MAX_PORTS_IN_LAG_GRP-1)] = rdpa_if_none};
      tmctl_portTmParms_t tmParms;

      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, rdpaIfArray, TMCTL_MAX_PORTS_IN_LAG_GRP);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      ret = tmctl_getPortTmParms(TMCTL_DEV_ETH, if_p, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctl_getPortTmParms returns error");
         return ret;
      }

      if (tmParms.maxQueues == 0 || tmParms.maxQueues > MAX_TMCTL_QUEUES)
      {
         tmctl_error("Invalid maxQueues=%d ifname=%s", tmParms.maxQueues,
                     if_p->ethIf.ifname);
         return TMCTL_ERROR;
      }

      if (tmParms.maxSpQueues > tmParms.maxQueues)
      {
         tmctl_error("Invalid maxSpQueues=%d maxQueues=%d ifname=%s",
                     tmParms.maxSpQueues, tmParms.maxQueues, if_p->ethIf.ifname);
         return TMCTL_ERROR;
      }

      /* check the configured scheduler type */
      schedType = cfgFlags & TMCTL_SCHED_TYPE_MASK;

      if ((schedType == TMCTL_SCHED_TYPE_SP     && !(tmParms.schedCaps & TMCTL_SP_CAPABLE)) ||
          (schedType == TMCTL_SCHED_TYPE_WRR    && !(tmParms.schedCaps & TMCTL_WRR_CAPABLE)) ||
          (schedType == TMCTL_SCHED_TYPE_SP_WRR && !(tmParms.schedCaps & TMCTL_SP_WRR_CAPABLE)))
      {
         tmctl_error("Configured scheduler type %d is not supported", schedType);
         return TMCTL_ERROR;
      }

      while (idx < TMCTL_MAX_PORTS_IN_LAG_GRP && ((rdpaIf = rdpaIfArray[idx++]) != rdpa_if_none))
      {
         /* In 631x8, LAN port and queue shapings are performed by Ethernet switch.
          * So we don't init rdpa tm for port and queue shaper.
          */
         if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
         {
            tmParms.portShaper  = FALSE;
            tmParms.queueShaper = FALSE;
         }

         ret = tmctlRdpa_TmInit(RDPA_IOCTL_DEV_PORT, rdpaIf, &tmParms, cfgFlags);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlRdpa_TmInit ERROR! ret=%d", ret);
         }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      tmctl_portTmParms_t tmParms;

      ret = tmctl_getPortTmParms(TMCTL_DEV_ETH, if_p, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctl_getPortTmParms returns error");
         return ret;
      }

      if (tmParms.maxQueues == 0 || tmParms.maxQueues > MAX_TMCTL_QUEUES)
      {
         tmctl_error("Invalid maxQueues=%d ifname=%s", tmParms.maxQueues,
                     if_p->ethIf.ifname);
         return TMCTL_ERROR;
      }

      if (tmParms.maxSpQueues > tmParms.maxQueues)
      {
         tmctl_error("Invalid maxSpQueues=%d maxQueues=%d ifname=%s",
                     tmParms.maxSpQueues, tmParms.maxQueues, if_p->ethIf.ifname);
         return TMCTL_ERROR;
      }
      ret = tmctlFap_portTmInit(if_p->ethIf.ifname, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_portTmInit ERROR! ret=%d", ret);
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)   
      tmctl_portTmParms_t tmParms;

      ret = tmctl_getPortTmParms(TMCTL_DEV_ETH, if_p, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctl_getPortTmParms returns error");
         return ret;
      }

      if (tmParms.maxQueues == 0 || tmParms.maxQueues > MAX_TMCTL_QUEUES)
      {
         tmctl_error("Invalid maxQueues=%d ifname=%s", tmParms.maxQueues,
                     if_p->ethIf.ifname);
         return TMCTL_ERROR;
      }

      if (tmParms.maxSpQueues > tmParms.maxQueues)
      {
         tmctl_error("Invalid maxSpQueues=%d maxQueues=%d ifname=%s",
                     tmParms.maxSpQueues, tmParms.maxQueues, if_p->ethIf.ifname);
         return TMCTL_ERROR;
      }
      ret = tmctlBcmTm_portTmInit(if_p->ethIf.ifname, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_portTmInit ERROR! ret=%d", ret);
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   tmctl_portTmParms_t tmParms;
   uint32_t            schedType;
   int                 rdpaDev;
   int                 ifId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   ret = tmctl_getPortTmParms(devType, if_p, &tmParms);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctl_getPortTmParms returns error, devType=%d, if=%d",
                  devType, ifId);
      return ret;
   }

   tmctl_debug("tmctl_getPortTmParms(): devType=%d, if=%d, "
               "schedCaps=%d, maxQueues=%d, maxSpQueues=%d,"
               "portShaper=%d, queueShaper=%d, cfgFlags=%d",
               devType, ifId, tmParms.schedCaps,
               tmParms.maxQueues, tmParms.maxSpQueues,
               tmParms.portShaper, tmParms.queueShaper, tmParms.cfgFlags);

   if (tmParms.maxQueues == 0 || tmParms.maxQueues > MAX_TMCTL_QUEUES)
   {
      tmctl_error("Invalid maxQueues=%d devType=%d if=%d",
                  tmParms.maxQueues, devType, ifId);
      return TMCTL_ERROR;
   }

   if (tmParms.maxSpQueues > tmParms.maxQueues)
   {
      tmctl_error("Invalid maxSpQueues=%d maxQueues=%d devType=%d",
                  tmParms.maxSpQueues, tmParms.maxQueues, devType);
      return TMCTL_ERROR;
   }

   /* check the configured scheduler type */
   schedType = cfgFlags & TMCTL_SCHED_TYPE_MASK;

   if ((schedType == TMCTL_SCHED_TYPE_SP     && !(tmParms.schedCaps & TMCTL_SP_CAPABLE)) ||
       (schedType == TMCTL_SCHED_TYPE_WRR    && !(tmParms.schedCaps & TMCTL_WRR_CAPABLE)) ||
       (schedType == TMCTL_SCHED_TYPE_SP_WRR && !(tmParms.schedCaps & TMCTL_SP_WRR_CAPABLE)))
   {
      tmctl_error("Configured scheduler type %d is not supported", schedType);
      return TMCTL_ERROR;
   }

   if ((devType == TMCTL_DEV_GPON) || (devType == TMCTL_DEV_EPON) ||
     (devType == TMCTL_DEV_SVCQ))
   {
      tmParms.portShaper = FALSE;
   }
   else if (devType == TMCTL_DEV_ETH)
   {
      if (ifId == rdpa_if_wan0)
      {
         /* Keep the queueShaper setting. */
         tmParms.portShaper  = FALSE;
      }
      else
      {
         tmParms.portShaper  = FALSE;
         tmParms.queueShaper = FALSE;
      }
   }

   ret = tmctlRdpa_TmInit(rdpaDev, ifId, &tmParms, cfgFlags);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_TmInit ERROR! rdpaDev=%d, ifId=%d, ret=%d",
                  rdpaDev, ifId, ret);
   }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

   return ret;

}  /* End of tmctl_portTmInit() */


/* ----------------------------------------------------------------------------
 * This function un-initializes all TM configurations of a port. This
 * function may be called when the port is down.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_portTmUninit(tmctl_devType_e devType,
                               tmctl_if_t*     if_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_portTmUninitTrace(devType, if_p);

   tmctl_debug("Enter: devType=%d", devType);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      /* TMCTL must apply the same port setting to all the ports in LAG/Trunk group */
      int      idx=0;
      rdpa_if  rdpaIf;
      rdpa_if  rdpaIfArray[TMCTL_MAX_PORTS_IN_LAG_GRP] =
                                {[0 ... (TMCTL_MAX_PORTS_IN_LAG_GRP-1)] = rdpa_if_none};

      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, rdpaIfArray, TMCTL_MAX_PORTS_IN_LAG_GRP);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      while(idx < TMCTL_MAX_PORTS_IN_LAG_GRP && ((rdpaIf = rdpaIfArray[idx++]) != rdpa_if_none))
      {
          if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
          {
             tmctl_debug("Don't uninit TM on LAN port");
          }
          else
          {
             /* EthWAN */
             ret = tmctlRdpa_TmUninit(RDPA_IOCTL_DEV_PORT, rdpaIf);
             if (ret == TMCTL_ERROR)
             {
                tmctl_error("tmctlRdpa_TmUninit ERROR! ret=%d", ret);
                break;
             }
          }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_portTmUninit(if_p->ethIf.ifname);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_portTmUninit ERROR! ret=%d", ret);
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)   
      ret = tmctlBcmTm_portTmUninit(if_p->ethIf.ifname);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_portTmUninit ERROR! ret=%d", ret);
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int rdpaDev;
   int ifId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   ret = tmctlRdpa_TmUninit(rdpaDev, ifId);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_TmUninit ERROR! rdpaDev=%d, ifId=%d, ret=%d",
                  rdpaDev, ifId, ret);
   }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

   return ret;

}  /* End of tmctl_portTmUninit() */


/* ----------------------------------------------------------------------------
 * This function gets the configuration of a software queue. If the
 * configuration is not found, qid in the config structure will be
 * returned as -1.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    queueId (IN) Queue ID must be in the range of [0..maxQueues-1].
 *    qcfg_p (OUT) Structure to receive configuration parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getQueueCfg(tmctl_devType_e   devType,
                              tmctl_if_t*       if_p,
                              int               queueId,
                              tmctl_queueCfg_t* qcfg_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getQueueCfgTrace(devType, if_p, queueId, qcfg_p);

   tmctl_debug("Enter: devType=%d qid=%d", devType, queueId);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      rdpa_if rdpaIf;
      int     tmId;

      /* Only need to get queueCfg from one port; All LAG ports will be having the same config*/
      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, &rdpaIf, 1);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      ret = tmctlRdpa_getQueueCfg(RDPA_IOCTL_DEV_PORT, rdpaIf,
                                  queueId, &tmId, qcfg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlRdpa_getQueueCfg ERROR! ret=%d", ret);
      }

      /* In 631x8, LAN queue shaping is performed by Ethernet switch.
       *           EthWAN queue shaping is performed by Runner Overall RL TM
       */
      if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
      {
         /* LAN */
         ret = tmctlEthSw_getQueueShaper(if_p->ethIf.ifname,
                                         qcfg_p->qid,
                                         &(qcfg_p->shaper));
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlEthSw_getQueueShaper ERROR!");
            return ret;
         }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_getQueueCfg(if_p->ethIf.ifname, queueId, qcfg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_getQueueCfg ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_getQueueCfg(if_p->ethIf.ifname, queueId, qcfg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_getQueueCfg ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int rdpaDev;
   int ifId;
   int tmId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   ret = tmctlRdpa_getQueueCfg(rdpaDev, ifId,
                               queueId, &tmId, qcfg_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_getQueueCfg ERROR! ret=%d", ret);
   }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

   return ret;

}  /* End of tmctl_getQueueCfg() */


/* ----------------------------------------------------------------------------
 * This function configures a software queue for a port. The qeueu ID shall
 * be specified in the configuration parameter structure. If the queue
 * already exists, its configuration will be modified. Otherwise, the queue
 * will be added.
 *
 * Note that for Ethernet port with an external Switch, the new queue
 * configuration may not be applied immediately to the Switch. For instance,
 * SF2 only supports one of the following priority queuing options:
 *
 *    Q0  Q1  Q2  Q3  Q4  Q5  Q6  Q7
 * 1) SP  SP  SP  SP  SP  SP  SP  SP
 * 2) WRR WRR WRR WRR WRR WRR WRR SP
 * 3) WRR WRR WRR WRR WRR WRR SP  SP
 * 4) WRR WRR WRR WRR WRR SP  SP  SP
 * 5) WRR WRR WRR WRR SP  SP  SP  SP
 * 6) WRR WRR WRR WRR WRR WRR WRR WRR
 *
 * This function will commit the new queue configuration to SF2 only when
 * all the queue configurations of the port match one of the priority
 * queuing options supported by the Switch.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    qcfg_p (IN) Queue config parameters.
 *                Notes:
 *                - qid must be in the range of [0..maxQueues-1].
 *                - For 63268, 63138 or 63148 TMCTL_DEV_ETH device type,
 *                  -- the priority of SP queue must be set to qid.
 *                  -- the priority of WRR/WDRR/WFQ queue must be set to 0.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setQueueCfg(tmctl_devType_e   devType,
                              tmctl_if_t*       if_p,
                              tmctl_queueCfg_t* qcfg_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_setQueueCfgTrace(devType, if_p, qcfg_p);

   tmctl_debug("Enter: devType=%d qid=%d priority=%d qsize=%d schedMode=%d wt=%d minRate=%d kbps=%d mbs=%d",
               devType, qcfg_p->qid, qcfg_p->priority, qcfg_p->qsize, qcfg_p->schedMode,
               qcfg_p->weight, qcfg_p->shaper.minRate,
               qcfg_p->shaper.shapingRate, qcfg_p->shaper.shapingBurstSize);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      /* TMCTL must apply the same port setting to all the ports in LAG/Trunk group */
      rdpa_if             rdpaIf;
      tmctl_portTmParms_t tmParms;
      uint32_t            schedType;
      BOOL                dsQueueShaper = FALSE ;
      int                 idx = 0;
      rdpa_if             rdpaIfArray[TMCTL_MAX_PORTS_IN_LAG_GRP] =
                              {[0 ... (TMCTL_MAX_PORTS_IN_LAG_GRP-1)] = rdpa_if_none};

      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, rdpaIfArray, TMCTL_MAX_PORTS_IN_LAG_GRP);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      if ((qcfg_p->schedMode == TMCTL_SCHED_SP) && (qcfg_p->qid != qcfg_p->priority))
      {
         tmctl_error("Priority of SP queue must be equal to qid. qid=%d priority=%d",
                     qcfg_p->qid, qcfg_p->priority);
         return TMCTL_ERROR;
      }

      if ((qcfg_p->schedMode == TMCTL_SCHED_WRR || qcfg_p->schedMode == TMCTL_SCHED_WFQ) &&
          (qcfg_p->priority != 0))
      {
         tmctl_error("Priority of WRR/WFQ queue must be 0. qid=%d priority=%d",
                     qcfg_p->qid, qcfg_p->priority);
         return TMCTL_ERROR;
      }

      /* get the port tm parameters */
      ret = tmctl_getPortTmParms(TMCTL_DEV_ETH, if_p, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctl_getPortTmParms returns error");
         return ret;
      }

      schedType = tmParms.cfgFlags & TMCTL_SCHED_TYPE_MASK;

      if ((qcfg_p->schedMode == TMCTL_SCHED_SP &&
           schedType != TMCTL_SCHED_TYPE_SP  && schedType != TMCTL_SCHED_TYPE_SP_WRR) ||
          (qcfg_p->schedMode == TMCTL_SCHED_WRR &&
           schedType != TMCTL_SCHED_TYPE_WRR && schedType != TMCTL_SCHED_TYPE_SP_WRR))
      {
         tmctl_error("Queue sched mode %d is not supported", qcfg_p->schedMode);
         return TMCTL_ERROR;
      }

      while (idx < TMCTL_MAX_PORTS_IN_LAG_GRP && ((rdpaIf = rdpaIfArray[idx++]) != rdpa_if_none))
      {
         /* In 631x8, LAN queue shapings are performed by the switch. */
         if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
         {
            dsQueueShaper       = tmParms.queueShaper;
            tmParms.queueShaper = FALSE;
         }

         ret = tmctlRdpa_setQueueCfg(RDPA_IOCTL_DEV_PORT, rdpaIf, &tmParms, qcfg_p);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlRdpa_setQueueCfg ERROR! ret=%d", ret);
            return ret;
         }

         /* Commit port queue configuration to Ethernet Switch as needed.
          * In 631x8, only LAN interface is facilitated with an external switch.
          */
         if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
         {
            /* LAN */
            tmctl_portQcfg_t portQcfg;

            /* set Eth Switch queue shaper */
            if (dsQueueShaper)
            {
               ret = tmctlEthSw_setQueueShaper(if_p->ethIf.ifname,
                                               qcfg_p->qid,
                                               &(qcfg_p->shaper));
               if (ret == TMCTL_ERROR)
               {
                  tmctl_error("tmctlEthSw_setQueueShaper ERROR!");
                  return ret;
               }
            }

            /* set Eth Switch port scheduler only when all queues had been configured */
            ret = tmctlRdpa_getDevQueueCfg(RDPA_IOCTL_DEV_PORT, rdpaIf,
                                           tmParms.maxQueues, &portQcfg);
            if (ret == TMCTL_ERROR)
            {
               tmctl_error("tmctlRdpa_getPortQueueCfg ERROR! ret=%d", ret);
               return ret;
            }

            if (portQcfg.numQueues == tmParms.maxQueues)
            {
               ret = tmctlEthSw_setPortSched(if_p->ethIf.ifname, &portQcfg);
               if (ret == TMCTL_ERROR)
               {
                  tmctl_error("tmctlEthSw_setPortSched ERROR!");
                  return ret;
               }
            }
         }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      tmctl_portTmParms_t tmParms;
      uint32_t            schedType;

      /* get the port tm parameters */
      ret = tmctl_getPortTmParms(TMCTL_DEV_ETH, if_p, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctl_getPortTmParms returns error");
         return ret;
      }

      schedType = tmParms.cfgFlags & TMCTL_SCHED_TYPE_MASK;

      if ((qcfg_p->schedMode == TMCTL_SCHED_SP &&
           schedType != TMCTL_SCHED_TYPE_SP  && schedType != TMCTL_SCHED_TYPE_SP_WRR) ||
          (qcfg_p->schedMode == TMCTL_SCHED_WRR &&
           schedType != TMCTL_SCHED_TYPE_WRR && schedType != TMCTL_SCHED_TYPE_SP_WRR) ||
          (qcfg_p->schedMode == TMCTL_SCHED_WFQ &&
           schedType != TMCTL_SCHED_TYPE_WFQ))
      {
         tmctl_error("Queue sched mode %d is not supported", qcfg_p->schedMode);
         return TMCTL_ERROR;
      }

      ret = tmctlFap_setQueueCfg(if_p->ethIf.ifname, &tmParms, qcfg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_setQueueCfg ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)   
      tmctl_portTmParms_t tmParms;
      uint32_t            schedType;

      /* get the port tm parameters */
      ret = tmctl_getPortTmParms(TMCTL_DEV_ETH, if_p, &tmParms);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctl_getPortTmParms returns error");
         return ret;
      }

      schedType = tmParms.cfgFlags & TMCTL_SCHED_TYPE_MASK;

      if ((qcfg_p->schedMode == TMCTL_SCHED_SP &&
           schedType != TMCTL_SCHED_TYPE_SP  && schedType != TMCTL_SCHED_TYPE_SP_WRR) ||
          (qcfg_p->schedMode == TMCTL_SCHED_WRR &&
           schedType != TMCTL_SCHED_TYPE_WRR && schedType != TMCTL_SCHED_TYPE_SP_WRR) ||
          (qcfg_p->schedMode == TMCTL_SCHED_WFQ &&
           schedType != TMCTL_SCHED_TYPE_WFQ))
      {
         tmctl_error("Queue sched mode %d is not supported", qcfg_p->schedMode);
         return TMCTL_ERROR;
      }

      ret = tmctlBcmTm_setQueueCfg(if_p->ethIf.ifname, &tmParms, qcfg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_setQueueCfg ERROR! ret=%d", ret);
         return ret;
      }
#endif      
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   tmctl_portTmParms_t tmParms;
   uint32_t            schedType;
   int                 rdpaDev;
   int                 ifId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   if (qcfg_p->qid >= MAX_TMCTL_QUEUES)
   {
      tmctl_error("Queue id must < %d, qid=%d",
                  MAX_TMCTL_QUEUES, qcfg_p->qid);
      return TMCTL_ERROR;
   }

   if (qcfg_p->priority > TMCTL_PRIO_MAX)
   {
      tmctl_error("Priority of SP queue must < %d, qid=%d priority=%d",
                  TMCTL_PRIO_MAX, qcfg_p->qid, qcfg_p->priority);
      return TMCTL_ERROR;
   }

   if ((qcfg_p->schedMode == TMCTL_SCHED_WRR || qcfg_p->schedMode == TMCTL_SCHED_WFQ) &&
        (qcfg_p->priority != 0))
   {
      tmctl_error("Priority of WRR/WFQ queue must be 0. qid=%d priority=%d",
                  qcfg_p->qid, qcfg_p->priority);
      return TMCTL_ERROR;
   }

   if ((qcfg_p->schedMode == TMCTL_SCHED_WRR || qcfg_p->schedMode == TMCTL_SCHED_WFQ) &&
        (qcfg_p->weight == 0))
   {
      tmctl_error("Weight of WRR/WFQ queue must be non-zero. qid=%d",
                  qcfg_p->qid);
      return TMCTL_ERROR;
   }

   /* get the port tm parameters */
   ret = tmctl_getPortTmParms(devType, if_p, &tmParms);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctl_getPortTmParms returns error");
      return ret;
   }

   schedType = tmParms.cfgFlags & TMCTL_SCHED_TYPE_MASK;

   if ((qcfg_p->schedMode == TMCTL_SCHED_SP &&
        schedType != TMCTL_SCHED_TYPE_SP  && schedType != TMCTL_SCHED_TYPE_SP_WRR) ||
       (qcfg_p->schedMode == TMCTL_SCHED_WRR &&
        schedType != TMCTL_SCHED_TYPE_WRR && schedType != TMCTL_SCHED_TYPE_SP_WRR) ||
       (qcfg_p->schedMode == TMCTL_SCHED_WFQ &&
        schedType != TMCTL_SCHED_TYPE_WFQ))
   {
      tmctl_error("Queue sched mode %d is not supported", qcfg_p->schedMode);
      return TMCTL_ERROR;
   }

   ret = tmctlRdpa_setQueueCfg(rdpaDev, ifId, &tmParms, qcfg_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_setQueueCfg ERROR! ret=%d", ret);
   }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

   return ret;

}  /* End of tmctl_setQueueCfg() */


/* ----------------------------------------------------------------------------
 * This function deletes a software queue from a port.
 *
 * Note that for Ethernet port with an external Switch, the corresponding
 * Switch queue will not be deleted.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    queueId (IN) The queue ID.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_delQueueCfg(tmctl_devType_e devType,
                              tmctl_if_t*     if_p,
                              int             queueId)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_delQueueCfgTrace(devType, if_p, queueId);

   tmctl_debug("Enter: devType=%d qid=%d", devType, queueId);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      /* TMCTL must apply the same port setting to all the ports in LAG/Trunk group */
      int      idx=0;
      rdpa_if  rdpaIf;
      rdpa_if  rdpaIfArray[TMCTL_MAX_PORTS_IN_LAG_GRP] =
                                {[0 ... (TMCTL_MAX_PORTS_IN_LAG_GRP-1)] = rdpa_if_none};

      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, rdpaIfArray, TMCTL_MAX_PORTS_IN_LAG_GRP);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      while(idx < TMCTL_MAX_PORTS_IN_LAG_GRP && ((rdpaIf = rdpaIfArray[idx++]) != rdpa_if_none))
      {
          ret = tmctlRdpa_delQueueCfg(RDPA_IOCTL_DEV_PORT, rdpaIf, queueId);
          if (ret == TMCTL_ERROR)
          {
             tmctl_error("tmctlRdpa_delQueueCfg ERROR! ret=%d", ret);
          }

          if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
          {
             /* LAN */
             tmctl_shaper_t shaper;

             /* Disable Eth Switch queue shaper */
             memset(&shaper, 0, sizeof(tmctl_shaper_t));

             ret = tmctlEthSw_setQueueShaper(if_p->ethIf.ifname, queueId, &shaper);
             if (ret == TMCTL_ERROR)
             {
                tmctl_error("tmctlEthSw_setQueueShaper ERROR!");
                return ret;
             }
          }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)   
      ret = tmctlFap_delQueueCfg(if_p->ethIf.ifname, queueId);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_delQueueCfg ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_delQueueCfg(if_p->ethIf.ifname, queueId);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_delQueueCfg ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int rdpaDev;
   int ifId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   ret = tmctlRdpa_delQueueCfg(rdpaDev, ifId, queueId);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_delQueueCfg ERROR! ret=%d", ret);
   }
#endif  /* CHIP_6838 || CHIP_6848  || CHIP_6858 */

   return ret;

}  /* End of tmctl_delQueueCfg() */


/* ----------------------------------------------------------------------------
 * This function gets the port shaper configuration.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    shaper_p (OUT) The shaper parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getPortShaper(tmctl_devType_e devType,
                                tmctl_if_t*     if_p,
                                tmctl_shaper_t* shaper_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getPortShaperTrace(devType, if_p, shaper_p);

   tmctl_debug("Enter: devType=%d", devType);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      rdpa_if rdpaIf;

      /* Only need to get the PortShaper Config from one port; All trunk/LAG ports will have the same */
      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, &rdpaIf,1);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      /* In 631x8, LAN port shaping is performed by Ethernet switch.
       *           EthWAN port shaping is performed by Runner Overall RL TM
       */
      if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
      {
         /* LAN */
         ret = tmctlEthSw_getPortShaper(if_p->ethIf.ifname, shaper_p);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlEthSw_getQueueShaper ERROR!");
         }
      }
      else
      {
         /* WAN */
         ret = tmctlRdpa_getPortShaper(RDPA_IOCTL_DEV_PORT, rdpaIf, shaper_p);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlRdpa_getPortShaper ERROR! ret=%d", ret);
         }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_getPortShaper(if_p->ethIf.ifname, shaper_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_getPortShaper ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_getPortShaper(if_p->ethIf.ifname, shaper_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_getPortShaper ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int                 rdpaDev;
   int                 ifId;

   switch (devType)
   {
      case TMCTL_DEV_ETH:
      case TMCTL_DEV_SVCQ:
          ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
          if (ret != TMCTL_SUCCESS)
          {
              return TMCTL_ERROR;
          }
          ret = tmctlRdpa_getTmRlCfg(rdpaDev, ifId, shaper_p);
          if (ret == TMCTL_ERROR)
          {
              tmctl_error("tmctlRdpa_getTmRlCfg ERROR! ret=%d", ret);
              return ret;
          }
          break;

      case TMCTL_DEV_EPON:
          tmctl_error("EPON device is not supported");
          ret = TMCTL_ERROR;
          break;

      case TMCTL_DEV_GPON:
          tmctl_error("GPON device is not supported");
          ret = TMCTL_ERROR;
          break;

      default:
         tmctl_error("Invalid device type %d", devType);
         ret = TMCTL_ERROR;
   }

#endif /* CHIP_6838 || CHIP_6848 || CHIP_6858 */
   return ret;

}  /* End of tmctl_getPortShaper() */


/* ----------------------------------------------------------------------------
 * This function configures the port shaper for shaping rate, shaping burst
 * size and minimum rate. If port shaping is to be done by the external
 * Switch, the corresponding Switch port shaper will be configured.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    shaper_p (IN) The shaper parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setPortShaper(tmctl_devType_e devType,
                                tmctl_if_t*     if_p,
                                tmctl_shaper_t* shaper_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_setPortShaperTrace(devType, if_p, shaper_p);

   tmctl_debug("Enter: devType=%d portKbps=%d portMbs=%d", devType,
               shaper_p->shapingRate, shaper_p->shapingBurstSize);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      /* TMCTL must apply the same port setting to all the ports in LAG/Trunk group */
      int      idx=0;
      rdpa_if  rdpaIf;
      rdpa_if  rdpaIfArray[TMCTL_MAX_PORTS_IN_LAG_GRP] =
                                {[0 ... (TMCTL_MAX_PORTS_IN_LAG_GRP-1)] = rdpa_if_none};

      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, rdpaIfArray, TMCTL_MAX_PORTS_IN_LAG_GRP);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      while(idx < TMCTL_MAX_PORTS_IN_LAG_GRP && ((rdpaIf = rdpaIfArray[idx++]) != rdpa_if_none))
      {
          /* In 631x8, LAN port shaping is performed by Ethernet switch.
           *           EthWAN port shaping is performed by Runner Overall RL TM
           */
          if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
          {
             /* LAN */
             ret = tmctlEthSw_setPortShaper(if_p->ethIf.ifname, shaper_p);
             if (ret == TMCTL_ERROR)
             {
                tmctl_error("tmctlEthSw_setPortShaper ERROR! ret=%d", ret);
             }
          }
          else
          {
             /* WAN */
             ret = tmctlRdpa_setPortShaper(RDPA_IOCTL_DEV_PORT, rdpaIf, shaper_p);
             if (ret == TMCTL_ERROR)
             {
                tmctl_error("tmctlRdpa_setPortShaper ERROR! ret=%d", ret);
             }

          }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_setPortShaper(if_p->ethIf.ifname, shaper_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_setPortShaper ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_setPortShaper(if_p->ethIf.ifname, shaper_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_setPortShaper ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int                 rdpaDev;
   int                 ifId;

   switch (devType)
   {
      case TMCTL_DEV_ETH:
      case TMCTL_DEV_SVCQ:
      case TMCTL_DEV_EPON:
          ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
          if (ret != TMCTL_SUCCESS)
          {
              return TMCTL_ERROR;
          }
          ret = tmctlRdpa_setTmRlCfg(rdpaDev, ifId, shaper_p);
          if (ret == TMCTL_ERROR)
          {
              tmctl_error("tmctlRdpa_setTmRlCfg ERROR! ret=%d", ret);
              return ret;
          }
          break;

      case TMCTL_DEV_GPON:
          tmctl_error("GPON device is not supported yet");
          ret = TMCTL_ERROR;
          break;

      default:
         tmctl_error("Invalid device type %d", devType);
         ret = TMCTL_ERROR;
   }


#endif /* CHIP_6838 || CHIP_6848 || CHIP_6858 */
   return ret;

}  /* End of tmctl_setPortShaper() */


/* ----------------------------------------------------------------------------
 * This function allocates a free queue profile index.
 *
 * Parameters:
 *    queueProfileId_p (OUT) Queue Profile ID.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_allocQueueProfileId(int* queueProfileId_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_allocQueueProfileIdTrace(queueProfileId_p);

   tmctl_debug("Enter: ");

#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
   ret = tmctlFap_allocQueueProfileId(queueProfileId_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_allocQueueProfileId ERROR! ret=%d", ret);
      return ret;
   }
#endif
#endif

#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
   ret = tmctlBcmTm_allocQueueProfileId(queueProfileId_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlBcmTm_allocQueueProfileId ERROR! ret=%d", ret);
      return ret;
   }
#endif
#endif

   return ret;

}  /* End of tmctl_allocQueueProfileId() */


/* ----------------------------------------------------------------------------
 * This function free a queue profile index.
 *
 * Parameters:
 *    queueProfileId (IN) Queue Profile ID.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_freeQueueProfileId(int queueProfileId)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_freeQueueProfileIdTrace(queueProfileId);

   tmctl_debug("Enter: queueProfileId=%d ", queueProfileId);

#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
   ret = tmctlFap_freeQueueProfileId(queueProfileId);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_freeQueueProfileId ERROR! ret=%d", ret);
      return ret;
   }
#endif
#endif

#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
   ret = tmctlBcmTm_freeQueueProfileId(queueProfileId);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlBcmTm_freeQueueProfileId ERROR! ret=%d", ret);
      return ret;
   }
#endif   
#endif

   return ret;

}  /* End of tmctl_freeQueueProfileId() */


/* ----------------------------------------------------------------------------
 * This function gets the queue profile of a queue profile index.
 *
 * Parameters:
 *    queueProfileId (IN) Queue ID.
 *    queueProfile_p (OUT) The drop algorithm configuration.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getQueueProfile(int                   queueProfileId,
                                  tmctl_queueProfile_t* queueProfile_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getQueueProfileTrace(queueProfileId, queueProfile_p);

   tmctl_debug("Enter: qProfileId=%d", queueProfileId);

#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
   ret = tmctlFap_getQueueProfile(queueProfileId, queueProfile_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_getQueueProfile ERROR! ret=%d", ret);
      return ret;
   }
#endif
#endif

#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
   ret = tmctlBcmTm_getQueueProfile(queueProfileId, queueProfile_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlBcmTm_getQueueProfile ERROR! ret=%d", ret);
      return ret;
   }
#endif
#endif

   return ret;
   
} /* End of tmctl_getQueueProfile() */


/* ----------------------------------------------------------------------------
 * This function sets the queue profile of a queue profile index.
 *
 * Parameters:
 *    queueProfileId (IN) Queue Profile ID.
 *    queueProfile_p (IN) The drop algorithm configuration.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setQueueProfile(int                   queueProfileId,
                                  tmctl_queueProfile_t* queueProfile_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_setQueueProfileTrace(queueProfileId, queueProfile_p);

   tmctl_debug("Enter: queueProfileId=%d dropProb=%d minThreshold=%d maxThreshold=%d",
               queueProfileId, queueProfile_p->dropProb, queueProfile_p->minThreshold,
               queueProfile_p->maxThreshold);

#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
   ret = tmctlFap_setQueueProfile(queueProfileId, queueProfile_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_setQueueProfile ERROR! ret=%d", ret);
      return ret;
   }
#endif
#endif

#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
   ret = tmctlBcmTm_setQueueProfile(queueProfileId, queueProfile_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlBcmTm_setQueueProfile ERROR! ret=%d", ret);
      return ret;
   }
#endif
#endif

   return ret;
   
} /* End of tmctl_setQueueProfile() */

/* ----------------------------------------------------------------------------
 * This function gets the drop algorithm of a queue.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    queueId (IN) Queue ID.
 *    dropAlg_p (OUT) The drop algorithm configuration.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getQueueDropAlg(tmctl_devType_e       devType,
                                  tmctl_if_t*           if_p,
                                  int                   queueId,
                                  tmctl_queueDropAlg_t* dropAlg_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getQueueDropAlgTrace(devType, if_p, queueId, dropAlg_p);

   tmctl_debug("Enter: devType=%d qid=%d", devType, queueId);

   if (devType == TMCTL_DEV_ETH)
   {
#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_getQueueDropAlg(if_p->ethIf.ifname, queueId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_getQueueDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_getQueueDropAlg(if_p->ethIf.ifname, queueId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_getQueueDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
   }

   return ret;

}  /* End of tmctl_getQueueDropAlg() */


/* ----------------------------------------------------------------------------
 * This function sets the drop algorithm of a queue.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    queueId (IN) Queue ID.
 *    dropAlg_p (IN) The drop algorithm configuration.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setQueueDropAlgExt(tmctl_devType_e          devType,
                                     tmctl_if_t*              if_p,
                                     int                      queueId,
                                     tmctl_dropAlg_e          dropAlgorithm,
                                     tmctl_queueDropAlgExt_t* dropAlgLo_p,
                                     tmctl_queueDropAlgExt_t* dropAlgHi_p)
{

   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_debug("Enter: devType=%d ifname=%s qid=%d dropAlgorithm=%d "
               "Lo(minThr=%d maxThr=%d pct=%d) "
               "Hi(minThr=%d maxThr=%d pct=%d)",
               devType, if_p->ethIf.ifname, queueId, dropAlgorithm, 
               dropAlgLo_p->redMinThreshold, dropAlgLo_p->redMaxThreshold,
               dropAlgLo_p->redPercentage,
               dropAlgHi_p->redMinThreshold, dropAlgHi_p->redMaxThreshold,
               dropAlgHi_p->redPercentage);

   tmctl_setQueueDropAlgExtTrace(devType, if_p, queueId, dropAlgorithm,
                                 dropAlgLo_p, dropAlgHi_p);

#if defined(CHIP_63138) || defined(CHIP_63148)

   if (dropAlgorithm == TMCTL_DROP_RED)
   {
      /* Currently Runner firmware does not support RED.
       * RED can be implemented as WRED with equal low and high
       * class thresholds.
       */
      dropAlgorithm = TMCTL_DROP_WRED;
      dropAlgHi_p->redMinThreshold = dropAlgLo_p->redMinThreshold;
      dropAlgHi_p->redMaxThreshold = dropAlgLo_p->redMaxThreshold;
   }

   if (devType == TMCTL_DEV_ETH)
   {
      int      idx=0;
      rdpa_if  rdpaIf;
      
      /* TMCTL must apply the same port setting to all the ports in LAG/Trunk group */
      rdpa_if  rdpaIfArray[TMCTL_MAX_PORTS_IN_LAG_GRP] =
                                {[0 ... (TMCTL_MAX_PORTS_IN_LAG_GRP-1)] = rdpa_if_none};

      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, rdpaIfArray, TMCTL_MAX_PORTS_IN_LAG_GRP);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      while(idx < TMCTL_MAX_PORTS_IN_LAG_GRP && ((rdpaIf = rdpaIfArray[idx++]) != rdpa_if_none))
      {
         ret = tmctlRdpa_setQueueDropAlg(RDPA_IOCTL_DEV_PORT, rdpaIf, queueId, dropAlgorithm,
                                         dropAlgLo_p, dropAlgHi_p);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlRdpa_setQueueDropAlg ERROR! ret=%d", ret);
            return ret;
         }
      }
   }
#endif

#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
   if (devType == TMCTL_DEV_ETH)
   {
      ret = tmctlFap_setQueueDropAlgExt(if_p->ethIf.ifname, queueId, dropAlgorithm,
                                        dropAlgLo_p, dropAlgHi_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_setQueueDropAlgExt ERROR! ret=%d", ret);
         return ret;
      }
   }
#endif
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int rdpaDev;
   int ifId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   ret = tmctlRdpa_setQueueDropAlg(rdpaDev, ifId, queueId, dropAlgorithm,
                                   dropAlgLo_p, dropAlgHi_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_setQueueDropAlg ERROR! ret=%d", ret);
      return ret;
   }
#endif

   return ret;

}  /* End of tmctl_setQueueDropAlgExt() */


tmctl_ret_e tmctl_setQueueDropAlg(tmctl_devType_e       devType,
                                  tmctl_if_t*           if_p,
                                  int                   queueId,
                                  tmctl_queueDropAlg_t* dropAlg_p)
{

   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_setQueueDropAlgTrace(devType, if_p, queueId, dropAlg_p);

   tmctl_debug("Enter: devType=%d qid=%d dropAlgorithm=%d queueProfileIdLo=%d "
               "queueProfileIdHi=%d priorityMask0=0x%x priorityMask1=0x%x",
               devType, queueId, dropAlg_p->dropAlgorithm, dropAlg_p->queueProfileIdLo,
               dropAlg_p->queueProfileIdHi, dropAlg_p->priorityMask0,
               dropAlg_p->priorityMask1);

   if (devType == TMCTL_DEV_ETH)
   {
#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_setQueueDropAlg(if_p->ethIf.ifname, queueId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_setQueueDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_setQueueDropAlg(if_p->ethIf.ifname, queueId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_setQueueDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
   }

   return ret;

}  /* End of tmctl_setQueueDropAlg() */

/* ----------------------------------------------------------------------------
 * This function gets the drop algorithm of a XTM channel.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    channelId (IN) Channel ID.
 *    dropAlg_p (OUT) The drop algorithm configuration.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getXtmChannelDropAlg(tmctl_devType_e       devType,
                                       int                   channelId,
                                       tmctl_queueDropAlg_t* dropAlg_p)
{

   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getXtmChannelDropAlgTrace(devType, channelId, dropAlg_p);

   tmctl_debug("Enter: devType=%d channelId=%d", devType, channelId);

   if (devType == TMCTL_DEV_XTM)
   {
#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_getXtmChannelDropAlg(channelId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_getXtmChannelDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_getXtmChannelDropAlg(channelId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_getXtmChannelDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
   }

   return ret;

}  /* End of tmctl_getXtmChannelDropAlg() */


/* ----------------------------------------------------------------------------
 * This function sets the drop algorithm of a XTM channel.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    channelId (IN) Channel ID.
 *    dropAlg_p (IN) The drop algorithm configuration.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setXtmChannelDropAlg(tmctl_devType_e       devType,
                                       int                   channelId,
                                       tmctl_queueDropAlg_t* dropAlg_p)
{

   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_setXtmChannelDropAlgTrace(devType, channelId, dropAlg_p);

   tmctl_debug("Enter: devType=%d channelId=%d dropAlgorithm=%d queueProfileIdLo=%d "
               "queueProfileIdHi=%d priorityMask0=0x%x priorityMask1=0x%x",
               devType, channelId, dropAlg_p->dropAlgorithm, dropAlg_p->queueProfileIdLo,
               dropAlg_p->queueProfileIdHi, dropAlg_p->priorityMask0,
               dropAlg_p->priorityMask1);

   if (devType == TMCTL_DEV_XTM)
   {
#if defined(CHIP_63268)
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_setXtmChannelDropAlg(channelId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_setXtmChannelDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
#if defined(CHIP_63381)
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_setXtmChannelDropAlg(channelId, dropAlg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_setXtmChannelDropAlg ERROR! ret=%d", ret);
         return ret;
      }
#endif
#endif
   }

   return ret;

}  /* End of tmctl_setXtmChannelDropAlg() */

/* ----------------------------------------------------------------------------
 * This function gets the statistics of a queue.
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    queueId (IN) Queue ID.
 *    stats_p (OUT) The queue stats.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getQueueStats(tmctl_devType_e     devType,
                                tmctl_if_t*         if_p,
                                int                 queueId,
                                tmctl_queueStats_t* stats_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getQueueStatsTrace(devType, if_p, queueId, stats_p);

   tmctl_debug("Enter: devType=%d qid=%d", devType, queueId);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      rdpa_if rdpaIf;
      /* FIXME : This will not work for LAG/Trunk ports
.......* We must aggregate the Queue Stats for all the trunk/LAG ports */
      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, &rdpaIf, 1);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      /* get the queue statistics */
      ret = tmctlRdpa_getQueueStats(RDPA_IOCTL_DEV_PORT, rdpaIf, queueId, stats_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlRdpa_getQueueStats ERROR! ret=%d", ret);
         return ret;
      }
   }
   else if (devType == TMCTL_DEV_XTM)
   {
      rdpa_if rdpaIf = rdpa_if_wan1;

      /* get the queue statistics */
      ret = tmctlRdpa_getQueueStats(RDPA_IOCTL_DEV_PORT, rdpaIf, queueId, stats_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlRdpa_getQueueStats ERROR! ret=%d", ret);
         return ret;
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_getQueueStats(if_p->ethIf.ifname, queueId, stats_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_getQueueStats ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_getQueueStats(if_p->ethIf.ifname, queueId, stats_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_getQueueStats ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int rdpaDev;
   int ifId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   ret = tmctlRdpa_getQueueStats(rdpaDev, ifId, queueId, stats_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_getQueueStats ERROR, ret=%d", ret);
      return ret;
   }
#endif

   return ret;

}  /* End of tmctl_getQueueStats() */


/* ----------------------------------------------------------------------------
 * This function gets port TM parameters (capabilities).
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    tmParms_p (OUT) Structure to return port TM parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getPortTmParms(tmctl_devType_e      devType,
                                 tmctl_if_t*          if_p,
                                 tmctl_portTmParms_t* tmParms_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getPortTmParmsTrace(devType, if_p, tmParms_p);

   tmctl_debug("Enter: devType=%d", devType);

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_4908)

   if (devType == TMCTL_DEV_ETH)
   {
      rdpa_if rdpaIf;

      /* Only get the TmParms from one port; All trunk/LAG ports share the same */
      ret = tmctlRdpa_getRdpaIfByIfname(if_p->ethIf.ifname, &rdpaIf,1);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("Cannot find rdpaIf by ifname %s", if_p->ethIf.ifname);
         return ret;
      }

      /* get the port tm parameters */
      if (rdpaIf >= rdpa_if_lan0 && rdpaIf <= rdpa_if_lan_max)
      {
         ret = tmctlEthSw_getPortTmParms(if_p->ethIf.ifname, tmParms_p);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlEthSw_getPortTmParms ERROR! ret=%d", ret);
            return ret;
         }
      }
      else
      {
         ret = tmctlRdpa_getTmParms(RDPA_IOCTL_DEV_PORT, rdpaIf, tmParms_p);
         if (ret == TMCTL_ERROR)
         {
            tmctl_error("tmctlRdpa_getTmParms ERROR! ret=%d", ret);
            return ret;
         }
      }
   }

#endif

#if defined(CHIP_63268)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_FAPCTL)
      ret = tmctlFap_getPortTmParms(if_p->ethIf.ifname, tmParms_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_getPortTmParms ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_63381)
   if (devType == TMCTL_DEV_ETH)
   {
#if defined(SUPPORT_BCMTM)
      ret = tmctlBcmTm_getPortTmParms(if_p->ethIf.ifname, tmParms_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlBcmTm_getPortTmParms ERROR! ret=%d", ret);
         return ret;
      }
#endif
   }
#endif

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   int rdpaDev;
   int ifId;

   ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
   if (ret != TMCTL_SUCCESS)
   {
      return TMCTL_ERROR;
   }

   ret = tmctlRdpa_getTmParms(rdpaDev, ifId, tmParms_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_getTmParms ERROR! ret=%d", ret);
      return ret;
   }
#endif

   return ret;

}  /* End of tmctl_getPortTmParms() */


/* ----------------------------------------------------------------------------
 * This function gets the configuration of dscp to pbit table. If the
 * configuration is not found, ....
 *
 * Parameters:
 *    cfg_p (OUT) Structure to receive configuration parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getDscpToPbit(tmctl_dscpToPbitCfg_t* cfg_p)
{
   tmctl_ret_e ret = TMCTL_UNSUPPORTED;

   tmctl_getDscpToPbitTrace();

   tmctl_debug("Enter: ");

#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
   ret = tmctlRdpa_getDscpToPbit(cfg_p);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlRdpa_getDscpToPbit ERROR! ret=%d", ret);
   }
#endif  /* CHIP_63138 || CHIP_63148 || CHIP_6838 || CHIP_6848 || CHIP_6858 */

   return ret;

}  /* End of tmctl_getDscpToPbit() */


/* ----------------------------------------------------------------------------
 * This function sets the configuration of dscp to pbit table. 
 *
 * Parameters:
 *    cfg_p (IN) config parameters.
 *               
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setDscpToPbit(tmctl_dscpToPbitCfg_t* cfg_p)
{
    tmctl_ret_e ret = TMCTL_UNSUPPORTED;
    int i = 0;

    tmctl_setDscpToPbitTrace(cfg_p);
    tmctl_debug("Enter: ");
    for (i = 0; i < TOTAL_DSCP_NUM; i++)
    {
        tmctl_debug("dscp[%d]=%d", i, cfg_p->dscp[i]);
    }
   
#if defined(CHIP_63138) || defined(CHIP_63148) || defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
    for (i = 0; i < TOTAL_DSCP_NUM; i++)
    {
        if (cfg_p->dscp[i] > MAX_PBIT_VALUE)
        {
            tmctl_error("Pbit must < %d, dscp[%d]=%d", 
                MAX_PBIT_VALUE, i, cfg_p->dscp[i]);
            return TMCTL_ERROR;
        }
    }

    ret = tmctlRdpa_setDscpToPbit(cfg_p);
    if (ret == TMCTL_ERROR)
    {
        tmctl_error("tmctl_setDscpToPbit ERROR! ret=%d", ret);
    }
   
#endif  /* CHIP_63138 || CHIP_63148 || CHIP_6838 || CHIP_6848 || CHIP_6858 */

    return ret;

}  /* End of tmctl_setDscpToPbit() */


/* ----------------------------------------------------------------------------
 * This function gets the configuration of pbit to q table. If the
 * configuration is not found, ....
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    cfg_p (OUT) Structure to receive configuration parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getPbitToQ(tmctl_devType_e devType, 
                                 tmctl_if_t* if_p, 
                                 tmctl_pbitToQCfg_t* cfg_p)
{
    tmctl_ret_e ret = TMCTL_UNSUPPORTED;

    tmctl_getPbitToQTrace(devType, if_p, cfg_p);

    tmctl_debug("Enter: devType=%d", devType);

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
    int rdpaDev;
    int ifId;

    ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
    if (ret != TMCTL_SUCCESS)
    {
        return TMCTL_ERROR;
    }

    ret = tmctlRdpa_getPbitToQ(rdpaDev, ifId, cfg_p);
   
    if (ret == TMCTL_ERROR)
    {
        tmctl_error("tmctlRdpa_getPbitToQ ERROR! ret=%d", ret);
    }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

    return ret;

}  /* End of tmctl_getPbitToQ() */


/* ----------------------------------------------------------------------------
 * This function sets the configuration of pbit to q table. 
 *
 * Parameters:
 *    devType (IN) tmctl device type.
 *    if_p (IN) Port identifier.
 *    cfg_p (IN) config parameters.
 *               
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setPbitToQ(tmctl_devType_e devType, 
                                 tmctl_if_t* if_p, 
                                 tmctl_pbitToQCfg_t* cfg_p)
{
    tmctl_ret_e ret = TMCTL_UNSUPPORTED;
    int i = 0;
    
    tmctl_setPbitToQTrace(devType, if_p, cfg_p);

    tmctl_debug("Enter: devType=%d", devType);

    for (i = 0; i < TOTAL_PBIT_NUM; i++)
    {
        tmctl_debug("pbit[%d]=%d", i, cfg_p->pbit[i]);
    }

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
    int                 rdpaDev;
    int                 ifId;
    
    ret = tmctl_ifConvert(devType, if_p, &rdpaDev, &ifId);
    if (ret != TMCTL_SUCCESS)
    {
        return TMCTL_ERROR;
    }

    ret = tmctlRdpa_setPbitToQ(rdpaDev, ifId, cfg_p);
   
    if (ret == TMCTL_ERROR)
    {
        tmctl_error("tmctlRdpa_setPbitToQ ERROR! ret=%d", ret);
    }
   
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

   return ret;

}  /* End of tmctl_setPbitToQ() */


/* ----------------------------------------------------------------------------
 * This function gets the configuration of dscp to pbit feature.
 *
 * Parameters:
 *    dir (IN) direction.
 *    enable_p (OUT) enable or disable
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getForceDscpToPbit(tmctl_dir_e dir, BOOL* enable_p)
{
    tmctl_ret_e ret = TMCTL_UNSUPPORTED;

    tmctl_getForceDscpToPbitTrace(dir, enable_p);

    tmctl_debug("Enter: dir=%d", dir);

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
    ret = tmctlRdpa_getForceDscpToPbit(dir, enable_p);
    if (ret == TMCTL_ERROR)
    {
        tmctl_error("tmctlRdpa_getForceDscpToPbit ERROR! ret=%d", ret);
    }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

    return ret;

}  /* End of tmctl_getForceDscpToPbit() */


/* ----------------------------------------------------------------------------
 * This function sets the configuration of dscp to pbit feature.
 *
 * Parameters:
 *    dir (IN) direction.
 *    enable_p (IN) enable or disable
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setForceDscpToPbit(tmctl_dir_e dir, BOOL* enable_p)
{
    tmctl_ret_e ret = TMCTL_UNSUPPORTED;

    tmctl_setForceDscpToPbitTrace(dir, enable_p);

    tmctl_debug("Enter: dir=%d enable=%d", dir, (*enable_p));

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
  
    ret = tmctlRdpa_setForceDscpToPbit(dir, enable_p);
    if (ret == TMCTL_ERROR)
    {
        tmctl_error("tmctlRdpa_setForceDscpToPbit ERROR! ret=%d", ret);
    }

#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

    return ret;

}  /* End of tmctl_setForceDscpToPbit() */


/* ----------------------------------------------------------------------------
 * This function gets the configuration of packet based qos.
 *
 * Parameters:
 *    dir (IN) direction.
 *    type (IN) qos type
 *    enable_p (OUT) enable or disable
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_getPktBasedQos(tmctl_dir_e dir, 
                                  tmctl_qosType_e type, 
                                  BOOL* enable_p)
{
    tmctl_ret_e ret = TMCTL_UNSUPPORTED;

    tmctl_getPktBasedQosTrace(dir, type, enable_p);

    tmctl_debug("Enter: dir=%d type=%d", dir, type);

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
    if (dir >= TMCTL_DIR_MAX)
    {
        tmctl_error("dir our of range. dir=%d", dir);
        return TMCTL_ERROR;
    }

    if (type >= TMCTL_QOS_MAX)
    {
        tmctl_error("type our of range. type=%d", type);
        return TMCTL_ERROR;
    }
    
    ret = tmctlRdpa_getPktBasedQos(dir, type, enable_p);
    if (ret == TMCTL_ERROR)
    {
        tmctl_error("tmctlRdpa_getPktBasedQos ERROR! ret=%d", ret);
    }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

    return ret;

}  /* End of tmctl_getPktBasedQos() */


/* ----------------------------------------------------------------------------
 * This function sets the configuration of packet based qos.
 *
 * Parameters:
 *    dir (IN) direction.
 *    type (IN) qos type
 *    enable_p (IN) enable or disable
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctl_setPktBasedQos(tmctl_dir_e dir, 
                                  tmctl_qosType_e type, 
                                  BOOL* enable_p)
{
    tmctl_ret_e ret = TMCTL_UNSUPPORTED;
   
    tmctl_setPktBasedQosTrace(dir, type, enable_p);
   
    tmctl_debug("Enter: dir=%d type=%d enable=%d", dir, type, (*enable_p));

#if defined(CHIP_6838) || defined(CHIP_6848) || defined(CHIP_6858)
    if (dir >= TMCTL_DIR_MAX)
    {
        tmctl_error("dir our of range. dir=%d", dir);
        return TMCTL_ERROR;
    }

    if (type >= TMCTL_QOS_MAX)
    {
        tmctl_error("type our of range. type=%d", type);
        return TMCTL_ERROR;
    }
    
    ret = tmctlRdpa_setPktBasedQos(dir, type, enable_p);
    if (ret == TMCTL_ERROR)
    {
        tmctl_error("tmctlRdpa_setPktBasedQos ERROR! ret=%d", ret);
    }
#endif  /* CHIP_6838 || CHIP_6848 || CHIP_6858 */

    return ret;

}  /* End of tmctl_setPktBasedQos() */


