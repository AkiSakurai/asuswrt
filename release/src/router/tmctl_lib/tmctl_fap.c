/***********************************************************************
 *
 *  Copyright (c) 2010  Broadcom Corporation
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include "os_defs.h"

#include "tmctl_fap.h"

/* =========== Static Functions =========== */

static tmctl_ret_e tmctlFap_isQueueClear(const char* ifname,
                                         BOOL* isQueueClear_p);
static tmctl_ret_e tmctlFap_resetToAutoMode(const char* ifname);
static tmctl_ret_e tmctlFap_determineNewArbiter(int maxQueues, int maxSpQueues,
                                                tmctl_portQcfg_t* portQcfg_p,
                                                tmctl_queueCfg_t* newQcfg_p,
                                                fapIoctl_tmArbiterType_t *arbiterType_p,
                                                int* arbiterArg_p);
static tmctl_ret_e tmctlFap_determineNewMode(const char* ifname);
static tmctl_ret_e tmctlFap_getPortQueueCfg(const char* ifname,
                                            int maxQueues,
                                            tmctl_portQcfg_t* portQcfg_p);

/* ----------------------------------------------------------------------------
 * This function scans all FAP TM queues to see if all queues are deleted.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    isQueueClear_p (OUT) pointer to receive if all queues are deleted.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
static tmctl_ret_e tmctlFap_isQueueClear(const char* ifname,
                                         BOOL* isQueueClear_p)
{
   tmctl_ret_e ret = TMCTL_SUCCESS;
   tmctl_portTmParms_t tmParms;
   tmctl_portQcfg_t portQcfg;
   tmctl_queueCfg_t* qcfg_p;
   int i;

   ret = tmctlFap_getPortTmParms(ifname, &tmParms);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_getPortTmParms ERROR! ret=%d", ret);
      return ret;
   }

   ret = tmctlFap_getPortQueueCfg(ifname, tmParms.maxQueues, &portQcfg);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_getPortQueueCfg ERROR! ret=%d", ret);
      return ret;
   }

   *isQueueClear_p = TRUE;

   qcfg_p = &(portQcfg.qcfg[0]);
   for (i = 0; i < tmParms.maxQueues; i++, qcfg_p++)
   {
      if (qcfg_p->qid >= 0)
      {
         if (qcfg_p->qsize > 0)
         {
            /* There is still active queue. */
            *isQueueClear_p = FALSE;
         }
      }
   }

   return ret;
}


/* ----------------------------------------------------------------------------
 * This function reset MANUAL mode and switch to AUTO mode.
 * If AUTO mode is not configured yet, it gives a dummy 
 * initial value to avoid FAP error message. This value will 
 * be over-written after port is link up.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
static tmctl_ret_e tmctlFap_resetToAutoMode(const char* ifname)
{

   tmctl_ret_e ret = TMCTL_SUCCESS;

   /* Reset manual mode */
   fapCtlTm_modeReset((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL);

   /* Disable manual mode */
   fapCtlTm_portEnable((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, 0);

   /* Set port to auto mode */
   fapCtlTm_setPortMode((char *)ifname, FAP_IOCTL_TM_MODE_AUTO);

   /* tmctl SHOULD NOT enable AUTO mode for an Ethernet port.
      To enable or disable AUTO mode is decided by Ethernet driver,
      bcmPktDma_EthSetPhyRate(), when link is UP. */
   //fapCtlTm_portEnable((char *)ifname, FAP_IOCTL_TM_MODE_AUTO, 1);

   /* Apply new settings */
   fapCtlTm_apply((char *)ifname);

   return ret;

}  /* End of tmctlFap_resetToAutoMode() */


/* ----------------------------------------------------------------------------
 * This function determines new FAP TM arbitration settings based on
 * current and new queue setting.
 *
 * ===============================
 * Arbiter Mode State Machine
 * ===============================
 *    WRR/SPWRR + SP  = SP/SPWRR
 *       SP/WFQ + SP  = SP
 *     SP/SPWRR + WRR = WRR/SPWRR
 *      WRR/WFQ + WRR = WRR
 * SP/WRR/SPWRR + WFQ = WFQ
 * ===============================
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    maxQueues (IN) max number of queues supported by the port.
 *    portQcfg_p (IN) structure of the port queue configuration.
 *    newQcfg_p (IN) structure of the new queue configuration.
 *    arbiterType_p (OUT) pointer to receive new arbitration type.
 *    arbiterArg_p (OUT) pointer to receive new arbitration argument.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
static tmctl_ret_e tmctlFap_determineNewArbiter(int maxQueues, int maxSpQueues,
                                                tmctl_portQcfg_t* portQcfg_p,
                                                tmctl_queueCfg_t* newQcfg_p,
                                                fapIoctl_tmArbiterType_t *arbiterType_p,
                                                int* arbiterArg_p)
{
   int lowestSpQid = maxQueues;
   BOOL foundSp = FALSE;
   BOOL foundWrr = FALSE;
   //BOOL foundWfq = FALSE;
   int i;
   tmctl_queueCfg_t* qcfg_p;

   tmctl_debug("Enter: maxQueues=%d maxSpQueues=%d", maxQueues, maxSpQueues);

   qcfg_p = &(portQcfg_p->qcfg[0]);
   for (i = 0; i < maxQueues; i++, qcfg_p++)
   {
      if (qcfg_p->qid >= 0)
      {
         if (qcfg_p->schedMode == TMCTL_SCHED_SP)
         {
            foundSp = TRUE;
            if (qcfg_p->qid < lowestSpQid)
               lowestSpQid = qcfg_p->qid;
         }
         else if(qcfg_p->schedMode == TMCTL_SCHED_WRR)
         {
            foundWrr = TRUE;
         }
         /*
         else if(qcfg_p->schedMode == TMCTL_SCHED_WFQ)
         {
            foundWfq = TRUE;
         }
         */
      }
   }

   /* -------------------------------
    * Arbiter Mode State Machine
    * -------------------------------
    *    WRR/SPWRR + SP  = SP/SPWRR
    *       SP/WFQ + SP  = SP
    *     SP/SPWRR + WRR = WRR/SPWRR
    *      WRR/WFQ + WRR = WRR
    * SP/WRR/SPWRR + WFQ = WFQ
    * -------------------------------
    */

   if(newQcfg_p->schedMode == TMCTL_SCHED_SP)
   {
      if(foundWrr) /* WRR or SPWRR */
      {
         if (newQcfg_p->qid == 0)
         {
            *arbiterType_p = FAP_IOCTL_TM_ARBITER_TYPE_SP;
         }
         else
         {
            *arbiterType_p = FAP_IOCTL_TM_ARBITER_TYPE_SP_WRR;
         }
      }
      else /* SP or WFQ */
      {
         *arbiterType_p = FAP_IOCTL_TM_ARBITER_TYPE_SP;
      }
   }
   else if(newQcfg_p->schedMode == TMCTL_SCHED_WRR)
   {
      if(foundSp) /* SP or SPWRR */
      {
         if (newQcfg_p->qid == (maxQueues - 1))
         {
            *arbiterType_p = FAP_IOCTL_TM_ARBITER_TYPE_WRR;
         }
         else
         {
            *arbiterType_p = FAP_IOCTL_TM_ARBITER_TYPE_SP_WRR;
         }
      }
      else /* WRR or WFQ */
      {
         *arbiterType_p = FAP_IOCTL_TM_ARBITER_TYPE_WRR;
      }
   }
   else if(newQcfg_p->schedMode == TMCTL_SCHED_WFQ)
   {
      *arbiterType_p = FAP_IOCTL_TM_ARBITER_TYPE_WFQ;
   }
   else
   {
      tmctl_error("Queue scheduling mode is not supported, schedMode = %d", newQcfg_p->schedMode);
      return TMCTL_ERROR;
   }

   if(*arbiterType_p == FAP_IOCTL_TM_ARBITER_TYPE_SP_WRR)
   {
      if((newQcfg_p->schedMode == TMCTL_SCHED_SP) &&
         (newQcfg_p->qid < lowestSpQid))
      {
         *arbiterArg_p = newQcfg_p->qid;
      }
      else if((newQcfg_p->schedMode == TMCTL_SCHED_WRR) &&
              (newQcfg_p->qid >= lowestSpQid))
      {
         *arbiterArg_p = newQcfg_p->qid + 1;
      }
      else
      {
         *arbiterArg_p = lowestSpQid;
      }
      if(*arbiterArg_p < (maxQueues - maxSpQueues))
      {
         tmctl_error("Number of priority queue exceeds the limit, lowestSpQid = %d", *arbiterArg_p);
         return TMCTL_ERROR;
      }
   }
   else
   {
      *arbiterArg_p = 0;
   }

   return TMCTL_SUCCESS;

}  /* End of tmctlFap_getNewArbiterType() */


/* ----------------------------------------------------------------------------
 * This function determines new FAP TM mode.
 * It is called when queue or port settings is changed.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
static tmctl_ret_e tmctlFap_determineNewMode(const char* ifname)
{

   tmctl_ret_e ret = TMCTL_SUCCESS;
   fapIoctl_tmShapingType_t portShapingType;
   int portKbps = 0;
   int portMbs  = 0;
   fapIoctl_tmShapingType_t autoPortShapingType;
   int autoPortKbps = 0;
   int autoPortMbs  = 0;
   BOOL isQueueClear;
   int rc;

   /* Check if there is any queue left */
   ret = tmctlFap_isQueueClear(ifname, &isQueueClear);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_isQueueClear ERROR! ifname=%s ret=%d", ifname, ret);
      return ret;
   }

   if(!isQueueClear)
   {
      /* Some queues left, do nothing. */
      return ret;
   }

   /* All queues are deleted, check port shaping */
   if ((rc = fapCtlTm_getPortConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL,
                                    &portKbps, &portMbs, &portShapingType)))
   {
      tmctl_error("fapCtlTm_getPortConfig ERROR! ifname=%s", ifname);
      return TMCTL_ERROR;
   }

   /* Get FAP TM port config from AUTO mode. */
   if ((rc = fapCtlTm_getPortConfig((char *)ifname, FAP_IOCTL_TM_MODE_AUTO, &autoPortKbps,
                                    &autoPortMbs, &autoPortShapingType)))
   {
      tmctl_error("fapCtlTm_getPortConfig ERROR! ifname=%s auto mode", ifname);
      return TMCTL_ERROR;
   }

   if(portKbps != autoPortKbps)
   {
      /* Port shaping still works, do nothing. */
      return ret;
   }

   /* No port shaper and queue, reset to auto mode */

   ret = tmctlFap_resetToAutoMode(ifname);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_resetToAutoMode ERROR! ifname=%s ret=%d", ifname, ret);
      return ret;
   }

   return ret;

}  /* End of tmctlFap_determineNewMode() */


/* ----------------------------------------------------------------------------
 * This function gets the FAP TM queue configuration of the port.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    maxQueues (IN) max number of queues supported by the port.
 *    portQcfg_p (OUT) structure to receive the port queue configuration.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
static tmctl_ret_e tmctlFap_getPortQueueCfg(const char* ifname,
                                            int maxQueues,
                                            tmctl_portQcfg_t* portQcfg_p)
{
   tmctl_ret_e ret = TMCTL_SUCCESS;
   int  qid;
   tmctl_queueCfg_t* qcfg_p;

   memset(portQcfg_p, 0, sizeof(tmctl_portQcfg_t));

   portQcfg_p->numQueues = 0;

   qcfg_p = &(portQcfg_p->qcfg[0]);
   for (qid = 0; qid < maxQueues; qid++, qcfg_p++)
   {
      ret = tmctlFap_getQueueCfg(ifname, qid, qcfg_p);
      if (ret == TMCTL_ERROR)
      {
         tmctl_error("tmctlFap_getQueueCfg ERROR!");
         break;
      }
      else if (ret == TMCTL_SUCCESS)
      {
         portQcfg_p->numQueues++;
      }
   }

   return TMCTL_SUCCESS;

}  /* End of tmctlFap_getPortQueueCfg() */


/* =========== Public Functions =========== */

/* ----------------------------------------------------------------------------
 * This function initializes the FAP TM configuration for a port.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    tmParms_p (IN) Port tm parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_portTmInit(const char* ifname,
                                tmctl_portTmParms_t* tmParms_p)
{
   tmctl_ret_e ret = TMCTL_SUCCESS;

   tmctl_debug("Enter: ifname=%s schedCaps=0x%x maxQueues=%d portShaper=%d queueShaper=%d",
               ifname, tmParms_p->schedCaps, tmParms_p->maxQueues,
               tmParms_p->portShaper, tmParms_p->queueShaper);

   ret = tmctlFap_resetToAutoMode(ifname);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_resetToAutoMode ERROR! ifname=%s ret=%d", ifname, ret);
      return ret;
   }

   return ret;

}  /* End of tmctlFap_portTmInit() */


/* ----------------------------------------------------------------------------
 * This function un-initializes the FAP TM configuration of a port.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_portTmUninit(const char* ifname)
{
   tmctl_ret_e ret = TMCTL_SUCCESS;

   tmctl_debug("Enter: ifname=%s", ifname);

   ret = tmctlFap_resetToAutoMode(ifname);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_resetToAutoMode ERROR! ifname=%s ret=%d", ifname, ret);
      return ret;
   }

   return ret;

}  /* End of tmctlFap_portTmUninit() */


/* ----------------------------------------------------------------------------
 * This function gets the configuration of a FAP TM queue.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    qid (IN) Queue ID.
 *    qcfg_p (OUT) structure to receive configuration parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_getQueueCfg(const char* ifname,
                                 int qid,
                                 tmctl_queueCfg_t* qcfg_p)
{
   fapIoctl_tmArbiterType_t arbiterType = FAP_IOCTL_TM_ARBITER_TYPE_SP;
   int arbiterArg;
   int maxRateKbps;
   int minRateKbps;
   int mbs;
   int weight;
   int qsize;
   int rc;

   tmctl_debug("Enter: ifname=%s qid=%d", ifname, qid);

   if ((rc = fapCtlTm_getArbiterConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL,
                                       &arbiterType, &arbiterArg)))
   {
      tmctl_error("fapCtlTm_getArbiterConfig ERROR! ifname=%s", ifname);
      return TMCTL_ERROR;
   }

   if ((rc = fapCtlTm_getQueueConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, qid,
                                     &maxRateKbps, &minRateKbps, &mbs, &weight, &qsize)))
   {
      tmctl_error("fapCtlTm_getQueueConfig ERROR! ifname=%s qid=%d", ifname, qid);
      return TMCTL_ERROR;
   }

   memset(qcfg_p, 0, sizeof(tmctl_queueCfg_t));

   if (arbiterType == FAP_IOCTL_TM_ARBITER_TYPE_WFQ)
   {
      qcfg_p->schedMode = TMCTL_SCHED_WFQ;
      qcfg_p->priority  = 0;
   }
   else if (arbiterType == FAP_IOCTL_TM_ARBITER_TYPE_SP_WRR)
   {
      if(qid < arbiterArg)
      {
         qcfg_p->schedMode = TMCTL_SCHED_WRR;
         qcfg_p->priority  = 0;
      }
      else
      {
         qcfg_p->schedMode = TMCTL_SCHED_SP;
         qcfg_p->priority  = qid;
      }
   }
   else if (arbiterType == FAP_IOCTL_TM_ARBITER_TYPE_WRR)
   {
      qcfg_p->schedMode = TMCTL_SCHED_WRR;
      qcfg_p->priority  = 0;
   }
   else
   {
      qcfg_p->schedMode = TMCTL_SCHED_SP;
      qcfg_p->priority  = qid;
   }

   qcfg_p->weight = weight;
   qcfg_p->qid = qid;
   qcfg_p->qsize = qsize;
   qcfg_p->shaper.shapingRate = maxRateKbps;
   qcfg_p->shaper.shapingBurstSize = mbs;
   qcfg_p->shaper.minRate = minRateKbps;

   tmctl_debug("Done: ifname=%s qid=%d priority=%d schedMode=%d qsize=%d wt=%d shapingRate=%d burstSize=%d minRate=%d",
               ifname, qcfg_p->qid, qcfg_p->priority, qcfg_p->schedMode, qcfg_p->qsize, qcfg_p->weight,
               qcfg_p->shaper.shapingRate, qcfg_p->shaper.shapingBurstSize, qcfg_p->shaper.minRate);

   return TMCTL_SUCCESS;

}  /* End of tmctlFap_getQueueCfg() */


/* ----------------------------------------------------------------------------
 * This function sets the configuration of a queue.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name. 
 *    tmParms_p (IN) port tm parameters.
 *    qcfg_p (IN) structure containing the queue config parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_setQueueCfg(const char* ifname,
                                 tmctl_portTmParms_t* tmParms_p,
                                 tmctl_queueCfg_t* qcfg_p)
{
   tmctl_ret_e ret = TMCTL_SUCCESS;
   fapIoctl_tmArbiterType_t arbiterType;
   int arbiterArg;
   tmctl_portQcfg_t portQcfg;
   fapIoctl_tmShapingType_t portShapingType;
   int portKbps = 0;
   int portMbs = 0;
   fapIoctl_tmShapingType_t autoPortShapingType;
   int autoPortKbps = 0;
   int autoPortMbs  = 0;
   int queueKbps = 0;
   int queueMbs  = 0;

   int rc;

   tmctl_debug("Enter: ifname=%s qid=%d priority=%d schedMode=%d qsize=%d wt=%d shapingRate=%d burstSize=%d minRate=%d",
               ifname, qcfg_p->qid, qcfg_p->priority, qcfg_p->schedMode, qcfg_p->qsize, qcfg_p->weight,
               qcfg_p->shaper.shapingRate, qcfg_p->shaper.shapingBurstSize, qcfg_p->shaper.minRate);

   /* For non-xtm SP queue, there is backward compatibility issue
      because X_BROADCOM_COM_QueueId of the old Eth egress queue
      did not follow the current assignment scheme.
      Therefore, we have to use queue priority as qid.
      For non-xtm WRR or WFQ queue, there is NO backward compatibility
      issue because the old Eth egress queue only supports SP. */
   if(qcfg_p->schedMode == TMCTL_SCHED_SP)
   {
      qcfg_p->qid = qcfg_p->priority;
   }

   /* Get the configuration of all the port queues.
    * This is used for verifying whether the new queue config
    * would break the allowable queue scheduling scheme.
    */
   ret = tmctlFap_getPortQueueCfg(ifname, tmParms_p->maxQueues, &portQcfg);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_getPortQueueCfg ERROR! ret=%d", ret);
      return ret;
   }

   /* Calculate new arbiter type and argument. */
   ret = tmctlFap_determineNewArbiter(tmParms_p->maxQueues, tmParms_p->maxSpQueues,
                                      &portQcfg, qcfg_p, &arbiterType, &arbiterArg);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_determineNewArbiter ERROR! ifname=%s ret=%d", ifname, ret);
      return ret;
   }

   /* qcfgNew is good. Continue to configure port and queue. */

   /* Get port shaping rate */
   if ((rc = fapCtlTm_getPortConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, &portKbps,
                                    &portMbs, &portShapingType)))
   {
      tmctl_error("fapCtlTm_getPortConfig ERROR! ifname=%s", ifname);
      return TMCTL_ERROR;
   }

   /* Get new port shaping type. */
   switch (arbiterType)
   {
   case FAP_IOCTL_TM_ARBITER_TYPE_WRR:
      /* There is no shaper for WRR. So the shaping type shall be DISABLED. */
      portShapingType = FAP_IOCTL_TM_SHAPING_TYPE_DISABLED;
      break;

   case FAP_IOCTL_TM_ARBITER_TYPE_WFQ:
      portShapingType = FAP_IOCTL_TM_SHAPING_TYPE_RATIO;
      break;

   case FAP_IOCTL_TM_ARBITER_TYPE_SP:
   case FAP_IOCTL_TM_ARBITER_TYPE_SP_WRR:
   default:
      portShapingType = FAP_IOCTL_TM_SHAPING_TYPE_RATE;
      break;
   }

   /* If shaping rate is zero, get new port shaping rate. */
   if (portKbps <= 0)
   {
      /* Get FAP TM port config from AUTO mode. */
      if ((rc = fapCtlTm_getPortConfig((char *)ifname, FAP_IOCTL_TM_MODE_AUTO, &autoPortKbps,
                                       &autoPortMbs, &autoPortShapingType)))
      {
         tmctl_error("fapCtlTm_getPortConfig ERROR! ifname=%s auto mode", ifname);
         return TMCTL_ERROR;
      }
      if (autoPortKbps <= 0)
      {
         /* Set port shaping rate to 990 Mbps if AUTO mode is not configured yet. */
         portKbps = 990 * 1000; /* 990 Mbps */
         portMbs = 2000; /* 2000 bytes */
      }
      else
      {
         portKbps = autoPortKbps;
         portMbs = autoPortMbs;
      }
   }

   /* Set port shaping settings */
   if ((rc = fapCtlTm_portConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, portKbps, portMbs,
                                 portShapingType)))
   {
      tmctl_error("fapCtlTm_portConfig ERROR! ifname=%s portKbps=%d portMbs=%d portShapingType=%d",
                  ifname, portKbps, portMbs, portShapingType);
      return TMCTL_ERROR;
   }

   /* Set arbiter type */
   if ((rc = fapCtlTm_arbiterConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, arbiterType, arbiterArg)))
   {
      tmctl_error("fapCtlTm_arbiterConfig ERROR! ifname=%s arbiterType=%d arbiterArg=%d",
                  ifname, arbiterType, arbiterArg);
      return TMCTL_ERROR;
   }

   if(portShapingType == FAP_IOCTL_TM_SHAPING_TYPE_RATE)
   {
      /* Config min shaping rate */
      fapCtlTm_queueConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, qcfg_p->qid,
                           FAP_IOCTL_TM_SHAPER_TYPE_MIN, qcfg_p->shaper.minRate, qcfg_p->shaper.shapingBurstSize);

      /* Config max shaping rate */
      if(qcfg_p->shaper.shapingRate <= 0)
      {
         queueKbps = portKbps;
         queueMbs = portMbs;
      }
      else
      {
         queueKbps = qcfg_p->shaper.shapingRate;
         queueMbs = qcfg_p->shaper.shapingBurstSize;
      }
      fapCtlTm_queueConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, qcfg_p->qid,
                           FAP_IOCTL_TM_SHAPER_TYPE_MAX, queueKbps, queueMbs);
   }

   /* Set queue weight */
   fapCtlTm_setQueueWeight((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, qcfg_p->qid, qcfg_p->weight ? qcfg_p->weight : 1);

   /* Set port mode to manual */
   fapCtlTm_setPortMode((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL);

   /* Enable manual mode */
   fapCtlTm_portEnable((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, 1);

   /* Apply new settings */
   fapCtlTm_apply((char *)ifname);

   return ret;

}  /* End of tmctlFap_setQueueCfg() */


/* ----------------------------------------------------------------------------
 * This function dislocates the queue from FAP TM driver.
 * Note that the FAP TM queue is not deleted. Un-configured bit will be set and
 * qsize will returned zero when getting queue configuration.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    qid (IN) Queue ID.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_delQueueCfg(const char* ifname,
                                 int qid)
{
   tmctl_ret_e ret = TMCTL_SUCCESS;
   int rc;

   tmctl_debug("Enter: ifname=%s qid=%d", ifname, qid);

   /* Unconfigure this queue */
   if ((rc = fapCtlTm_queueUnconfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, qid)))
   {
      tmctl_error("fapCtlTm_queueUnconfig ERROR! ifname=%s qid=%d", ifname, qid);
      return TMCTL_ERROR;
   }

   /* Determine new mode is auto or manual */
   ret = tmctlFap_determineNewMode(ifname);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_determineNewMode ERROR! ifname=%s ret=%d", ifname, ret);
      return ret;
   }

   return ret;

}  /* End of tmctlFap_delQueueCfg() */

/* ----------------------------------------------------------------------------
 * This function gets the port shaping rate.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    shaper_p (OUT) Shaper parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_getPortShaper(const char* ifname,
                                   tmctl_shaper_t* shaper_p)
{
   tmctl_ret_e ret = TMCTL_SUCCESS;
   fapIoctl_tmShapingType_t portShapingType;
   int portKbps = 0;
   int portMbs  = 0;
   int rc;

   tmctl_debug("Enter: ifname=%s", ifname);

   /* Get port shaping settings */
   if ((rc = fapCtlTm_getPortConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, &portKbps,
                                    &portMbs, &portShapingType)))
   {
      tmctl_error("fapCtlTm_getPortConfig ERROR! ifname=%s", ifname);
      return TMCTL_ERROR;
   }

   shaper_p->shapingRate      = portKbps;
   shaper_p->shapingBurstSize = portMbs;
   shaper_p->minRate          = 0;  /* not supported */

   tmctl_debug("Done: ifname=%s shapingRate=%d shapingBurstSize=%d minRate=%d",
               ifname, shaper_p->shapingRate, shaper_p->shapingBurstSize, shaper_p->minRate);

   return ret;

}  /* End of tmctlFap_getPortShaper() */


/* ----------------------------------------------------------------------------
 * This function sets the port shaping rate. If the specified shaping rate
 * is greater than 0, FAP TM mode will be switched from auto to manual.
 * And the shaper rate will be set to this value. Otherwise, the shaper
 * rate of manual mode will be set according to auto mode, or even change
 * back to auto mode if no queues configured.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    shaper_p (IN) Shaper parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_setPortShaper(const char* ifname,
                                   tmctl_shaper_t* shaper_p)
{

   tmctl_ret_e ret = TMCTL_SUCCESS;
   fapIoctl_tmShapingType_t portShapingType;
   int portKbps = 0;
   int portMbs = 0;
   fapIoctl_tmShapingType_t autoPortShapingType;
   int autoPortKbps = 0;
   int autoPortMbs  = 0;
   int rc;

   tmctl_debug("Enter: ifname=%s shapingRate=%d shapingBurstSize=%d minRate=%d",
               ifname, shaper_p->shapingRate, shaper_p->shapingBurstSize, shaper_p->minRate);

   /* Get port shaping type */
   if ((rc = fapCtlTm_getPortConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, &portKbps,
                                    &portMbs, &portShapingType)))
   {
      tmctl_error("fapCtlTm_getPortConfig ERROR! ifname=%s", ifname);
      return TMCTL_ERROR;
   }

   portKbps = shaper_p->shapingRate;
   portMbs = shaper_p->shapingBurstSize;

   /* If shaping rate is zero, get auto port shaping rate. */
   if (portKbps <= 0)
   {
      /* Get FAP TM port config from AUTO mode. */
      if ((rc = fapCtlTm_getPortConfig((char *)ifname, FAP_IOCTL_TM_MODE_AUTO, &autoPortKbps,
                                       &autoPortMbs, &autoPortShapingType)))
      {
         tmctl_error("fapCtlTm_getPortConfig ERROR! ifname=%s auto mode", ifname);
         return TMCTL_ERROR;
      }
      if (autoPortKbps <= 0)
      {
         /* Set port shaping rate to 990 Mbps if AUTO mode is not configured yet. */
         portKbps = 990 * 1000; /* 990 Mbps */
         portMbs = 2000; /* 2000 bytes */
      }
      else
      {
         portKbps = autoPortKbps;
         portMbs = autoPortMbs;
      }
   }

   /* Set port shaping rate, minRate is not supported. */
   if ((rc = fapCtlTm_portConfig((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL,
                                 portKbps, portMbs, portShapingType)))
   {
      tmctl_error("fapCtlTm_portConfig ERROR! ifname=%s portKbps=%d portMbs=%d portShapingType=%d",
                  ifname, portKbps, portMbs, portShapingType);
      return TMCTL_ERROR;
   }

   /* Set port mode to manual */
   fapCtlTm_setPortMode((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL);

   /* Enable manual mode */
   fapCtlTm_portEnable((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, 1);

   /* Apply new settings */
   fapCtlTm_apply((char *)ifname);

   /* Determine new mode is auto or manual */
   ret = tmctlFap_determineNewMode(ifname);
   if (ret == TMCTL_ERROR)
   {
      tmctl_error("tmctlFap_determineNewMode ERROR! ifname=%s ret=%d", ifname, ret);
      return ret;
   }

   return ret;

}  /* End of tmctlFap_setPortShaper() */


/* ----------------------------------------------------------------------------
 * This function allocates a free FAP TM queue profile index.
 *
 * Parameters:
 *    queueProfileId_p (OUT) pointer for returned queue profile index.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_allocQueueProfileId(int* queueProfileId_p)
{
   int queueProfileId;

   tmctl_debug("Enter: ");

   if (fapCtlTm_allocQueueProfileId(&queueProfileId))
   {
      tmctl_error("tmctlFap_allocQueueProfileId ERROR!");
      return TMCTL_ERROR;
   }

   *queueProfileId_p = queueProfileId;

   tmctl_debug("Done: queueProfileId=%d ", *queueProfileId_p);
   return TMCTL_SUCCESS;

}  /* End of tmctlFap_allocQueueProfileId */


/* ----------------------------------------------------------------------------
 * This function frees a FAP TM queue profile index.
 *
 * Parameters:
 *    queueProfileId (IN) queue profile index to be freed.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_freeQueueProfileId(int queueProfileId)
{
   tmctl_debug("Enter: queueProfileId=%d ", queueProfileId);

   if (fapCtlTm_freeQueueProfileId(queueProfileId))
   {
      tmctl_error("tmctlFap_freeQueueProfileId ERROR!");
      return TMCTL_ERROR;
   }

   return TMCTL_SUCCESS;

}  /* End of tmctlFap_freeQueueProfileId */


/* ----------------------------------------------------------------------------
 * This function gets the queue profile of a FAP TM queue profile.
 *
 * Parameters:
 *    queueProfileId (IN) Queue Profile ID.
 *    queueProfile_p (OUT) structure to receive the queue profile parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_getQueueProfile(int queueProfileId,
                                     tmctl_queueProfile_t* queueProfile_p)
{
   int minThreshold, maxThreshold, dropProbability;

   tmctl_debug("Enter: queueProfileId=%d", queueProfileId);

   if (fapCtlTm_getQueueProfileConfig(queueProfileId, &dropProbability,
             &minThreshold, &maxThreshold))
   {
      tmctl_error("tmctlFap_getQueueProfile ERROR! queueProfileId=%d", queueProfileId);
      return TMCTL_ERROR;
   }

   memset(queueProfile_p, 0, sizeof(tmctl_queueProfile_t));
   queueProfile_p->dropProb = dropProbability;
   queueProfile_p->minThreshold = minThreshold;
   queueProfile_p->maxThreshold = maxThreshold;

   tmctl_debug("Done: queueProfileId=%d dropProbability=%d minThreshold=%d maxThreshold=%d ",
               queueProfileId, queueProfile_p->dropProb, queueProfile_p->minThreshold,
               queueProfile_p->maxThreshold);

   return TMCTL_SUCCESS;
}  /* End of tmctlFap_getQueueProfile */


/* ----------------------------------------------------------------------------
 * This function sets the queue profile of a FAP TM queue profile.
 *
 * Parameters:
 *    queueProfileId (IN) Queue Profile ID.
 *    queueProfile_p (IN) structure containing the queue profile parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_setQueueProfile(int queueProfileId,
                                     tmctl_queueProfile_t* queueProfile_p)
{
   tmctl_debug("Enter: queueProfileId=%d dropProbability=%d minThreshold=%d maxThreshold=%d ",
               queueProfileId, queueProfile_p->dropProb, queueProfile_p->minThreshold,
               queueProfile_p->maxThreshold);

   /* Set queue profile setting */
   if (fapCtlTm_queueProfileConfig(queueProfileId, queueProfile_p->dropProb,
             queueProfile_p->minThreshold, queueProfile_p->maxThreshold))
   {
      tmctl_error("tmctlFap_setQueueProfile ERROR! queueProfileId=%d", queueProfileId);
      return TMCTL_ERROR;
   }

   /* no need to use fapCtlTm_apply((char *)ifname); */

   return TMCTL_SUCCESS;
}  /* End of tmctlFap_setQueueProfile */


/* ----------------------------------------------------------------------------
 * This function gets the drop algorithm of a FAP TM queue.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    qid (IN) Queue ID.
 *    dropAlg_p (OUT) structure to receive the drop algorithm parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_getQueueDropAlg(const char* ifname,
                                     int qid,
                                     tmctl_queueDropAlg_t* dropAlg_p)
{
   fapIoctl_tmDropAlg_t dropAlgorithm;
   int queueProfileIdLo, queueProfileIdHi;
   uint32_t priorityMask0, priorityMask1;

   tmctl_debug("Enter: ifname=%s qid=%d", ifname, qid);

   if (fapCtlTm_getQueueDropAlgConfig((char *)ifname, qid,
             &dropAlgorithm, &queueProfileIdLo, &queueProfileIdHi,
             &priorityMask0, &priorityMask1))
   {
      tmctl_error("tmctlFap_getQueueDropAlg ERROR! ifname=%s qid=%d", ifname, qid);
      return TMCTL_ERROR;
   }

   memset(dropAlg_p, 0, sizeof(tmctl_queueDropAlg_t));

   if (dropAlgorithm == FAP_IOCTL_TM_DROP_ALG_RED)
   {
      dropAlg_p->dropAlgorithm  = TMCTL_DROP_RED;
   }
   else if (dropAlgorithm == FAP_IOCTL_TM_DROP_ALG_WRED)
   {
      dropAlg_p->dropAlgorithm = TMCTL_DROP_WRED;
   }
   else
   {
      dropAlg_p->dropAlgorithm = TMCTL_DROP_DT;
   }

   dropAlg_p->queueProfileIdLo = queueProfileIdLo;
   dropAlg_p->queueProfileIdHi = queueProfileIdHi;
   dropAlg_p->priorityMask0 = priorityMask0;
   dropAlg_p->priorityMask1 = priorityMask1;

   tmctl_debug("Done: ifname=%s qid=%d dropAlgorithm=%d queueProfileIdLo=%d "
               "queueProfileIdHi=%d priorityMask0=0x%x priorityMask1=0x%x",
               ifname, qid, dropAlg_p->dropAlgorithm, dropAlg_p->queueProfileIdLo,
               dropAlg_p->queueProfileIdHi, dropAlg_p->priorityMask0,
               dropAlg_p->priorityMask1);

   return TMCTL_SUCCESS;

} /* End of tmctlFap_getQueueDropAlg() */


/* ----------------------------------------------------------------------------
 * This function sets the drop algorithm of a FAP TM queue.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name. 
 *    qid (IN) Queue ID.
 *    dropAlg_p (IN) structure containing the drop algorithm parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_setQueueDropAlg(const char* ifname,
                                     int qid,
                                     tmctl_queueDropAlg_t* dropAlg_p)
{
   fapIoctl_tmDropAlg_t dropAlgorithm;

   tmctl_debug("Enter: ifname=%s qid=%d dropAlgorithm=%d queueProfileIdLo=%d "
               "queueProfileIdHi=%d priorityMask0=0x%x priorityMask1=0x%x",
               ifname, qid, dropAlg_p->dropAlgorithm, dropAlg_p->queueProfileIdLo,
               dropAlg_p->queueProfileIdHi, dropAlg_p->priorityMask0,
               dropAlg_p->priorityMask1);

   if (dropAlg_p->dropAlgorithm == TMCTL_DROP_RED)
   {
      dropAlgorithm = FAP_IOCTL_TM_DROP_ALG_RED;
   }
   else if (dropAlg_p->dropAlgorithm == TMCTL_DROP_WRED)
   {
      dropAlgorithm = FAP_IOCTL_TM_DROP_ALG_WRED;
   }
   else
   {
      dropAlgorithm = FAP_IOCTL_TM_DROP_ALG_DT;
   }

   /* Set queue drop algorithm setting */
   if (fapCtlTm_queueDropAlgConfig((char *)ifname, qid, dropAlgorithm,
                                   dropAlg_p->queueProfileIdLo,
                                   dropAlg_p->queueProfileIdHi,
                                   dropAlg_p->priorityMask0,
                                   dropAlg_p->priorityMask1))
   {
      tmctl_error("fapCtlTm_queueDropAlgConfig ERROR! ifname=%s qid=%d dropAlgorithm=%d", ifname, qid, dropAlgorithm);
      return TMCTL_ERROR;
   }

   /* Apply new settings */
   fapCtlTm_apply((char *)ifname);

   return TMCTL_SUCCESS;

} /* End of tmctlFap_setQueueDropAlg() */

/* ----------------------------------------------------------------------------
 * This function sets the drop algorithm of a FAP TM queue in another way.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name. 
 *    qid (IN) Queue ID.
 *    dropAlgorithm (IN) drop algorithm.
 *    dropAlgLo_p (IN) pointer to drop algorithm structure.
 *    dropAlgHi_p (IN) pointer to drop algorithm structure.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_setQueueDropAlgExt(const char* ifname,
                                        int qid,
                                        tmctl_dropAlg_e dropAlgorithm,
                                        tmctl_queueDropAlgExt_t* dropAlgLo_p,
                                        tmctl_queueDropAlgExt_t* dropAlgHi_p)
{
   fapIoctl_tmDropAlg_t fapDropAlgorithm;

   tmctl_debug("Enter: ifname=%s qid=%d dropAlgorithm=%d"
               "redMinThresholdLo=%d redMaxThresholdLo=%d redPercentageLo=%d"
               "redMinThresholdHi=%d redMaxThresholdHi=%d redPercentageHi=%d"
               ifname, qid, dropAlgorithm,
               dropAlgLo_p->redMinThreshold, dropAlgLo_p->redMaxThreshold, dropAlgLo_p->redPercentage,
               dropAlgHi_p->redMinThreshold, dropAlgHi_p->redMaxThreshold, dropAlgHi_p->redPercentage);

   if (dropAlgorithm == TMCTL_DROP_RED)
   {
      fapDropAlgorithm = FAP_IOCTL_TM_DROP_ALG_RED;
   }
   else if (dropAlgorithm == TMCTL_DROP_WRED)
   {
      fapDropAlgorithm = FAP_IOCTL_TM_DROP_ALG_WRED;
   }
   else
   {
      fapDropAlgorithm = FAP_IOCTL_TM_DROP_ALG_DT;
   }

   if (fapCtlTm_queueDropAlgConfigExt((char *)ifname, qid, fapDropAlgorithm,
                                      dropAlgLo_p->redPercentage,
                                      dropAlgLo_p->redMinThreshold,
                                      dropAlgLo_p->redMaxThreshold,
                                      dropAlgHi_p->redPercentage,
                                      dropAlgHi_p->redMinThreshold,
                                      dropAlgHi_p->redMaxThreshold))
   {
      tmctl_error("fapCtlTm_queueDropAlgConfigExt ERROR! ifname=%s qid=%d dropAlgorithm=%d", ifname, qid, dropAlgorithm);
      return TMCTL_ERROR;
   }

   /* Apply new settings */
   fapCtlTm_apply((char *)ifname);

   return TMCTL_SUCCESS;

} /* End of tmctlFap_setQueueDropAlg() */

/* ----------------------------------------------------------------------------
 * This function gets the drop algorithm of a FAP TM XTM Channel.
 *
 * Parameters:
 *    chid (IN) XTM Channel ID.
 *    dropAlg_p (OUT) structure to receive the drop algorithm parameters.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_getXtmChannelDropAlg(int chid,
                                          tmctl_queueDropAlg_t* dropAlg_p)
{
   fapIoctl_tmDropAlg_t dropAlgorithm;
   int queueProfileIdLo, queueProfileIdHi;
   uint32_t priorityMask0, priorityMask1;

   tmctl_debug("Enter: chid=%d", chid);

   if (fapCtlTm_getXtmChannelDropAlgConfig(chid,
             &dropAlgorithm, &queueProfileIdLo, &queueProfileIdHi,
             &priorityMask0, &priorityMask1))
   {
      tmctl_error("tmctlFap_getXtmChannelDropAlg ERROR! chid=%d", chid);
      return TMCTL_ERROR;
   }

   memset(dropAlg_p, 0, sizeof(tmctl_queueDropAlg_t));

   if (dropAlgorithm == FAP_IOCTL_TM_DROP_ALG_RED)
   {
      dropAlg_p->dropAlgorithm  = TMCTL_DROP_RED;
   }
   else if (dropAlgorithm == FAP_IOCTL_TM_DROP_ALG_WRED)
   {
      dropAlg_p->dropAlgorithm = TMCTL_DROP_WRED;
   }
   else
   {
      dropAlg_p->dropAlgorithm = TMCTL_DROP_DT;
   }

   dropAlg_p->queueProfileIdLo = queueProfileIdLo;
   dropAlg_p->queueProfileIdHi = queueProfileIdHi;
   dropAlg_p->priorityMask0 = priorityMask0;
   dropAlg_p->priorityMask1 = priorityMask1;

   tmctl_debug("Done: chid=%d dropAlgorithm=%d queueProfileIdLo=%d "
               "queueProfileIdHi=%d priorityMask0=0x%x priorityMask1=0x%x",
               chid, dropAlg_p->dropAlgorithm, dropAlg_p->queueProfileIdLo,
               dropAlg_p->queueProfileIdHi, dropAlg_p->priorityMask0,
               dropAlg_p->priorityMask1);

   return TMCTL_SUCCESS;

} /* End of tmctlFap_getXtmChannelDropAlg() */


/* ----------------------------------------------------------------------------
 * This function sets the drop algorithm of a FAP TM XTM Channel.
 *
 * Parameters:
 *    chid (IN) XTM Channel ID.
 *    dropAlg_p (IN) structure containing the drop algorithm parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_setXtmChannelDropAlg(int chid,
                                          tmctl_queueDropAlg_t* dropAlg_p)
{
   fapIoctl_tmDropAlg_t dropAlgorithm;

   tmctl_debug("Enter: chid=%d dropAlgorithm=%d queueProfileIdLo=%d "
               "queueProfileIdHi=%d priorityMask0=0x%x priorityMask1=0x%x",
               chid, dropAlg_p->dropAlgorithm, dropAlg_p->queueProfileIdLo,
               dropAlg_p->queueProfileIdHi, dropAlg_p->priorityMask0,
               dropAlg_p->priorityMask1);

   if (dropAlg_p->dropAlgorithm == TMCTL_DROP_RED)
   {
      dropAlgorithm = FAP_IOCTL_TM_DROP_ALG_RED;
   }
   else if (dropAlg_p->dropAlgorithm == TMCTL_DROP_WRED)
   {
      dropAlgorithm = FAP_IOCTL_TM_DROP_ALG_WRED;
   }
   else
   {
      dropAlgorithm = FAP_IOCTL_TM_DROP_ALG_DT;
   }

   /* Set queue drop algorithm setting */
   if (fapCtlTm_xtmChannelDropAlgConfig(chid, dropAlgorithm,
                                        dropAlg_p->queueProfileIdLo,
                                        dropAlg_p->queueProfileIdHi,
                                        dropAlg_p->priorityMask0,
                                        dropAlg_p->priorityMask1))
   {
      tmctl_error("tmctlFap_setXtmChannelDropAlg ERROR! chid=%d dropAlgorithm=%d", chid, dropAlgorithm);
      return TMCTL_ERROR;
   }

   return TMCTL_SUCCESS;

} /* End of tmctlFap_setXtmChannelDropAlg() */

/* ----------------------------------------------------------------------------
 * This function sets the drop algorithm of a FAP TM XTM Channel in another way.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name. 
 *    qid (IN) Queue ID.
 *    dropAlgorithm (IN) drop algorithm.
 *    dropAlgLo_p (IN) pointer to drop algorithm structure.
 *    dropAlgHi_p (IN) pointer to drop algorithm structure.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_setXtmChannelDropAlgExt(int chid,
                                             tmctl_dropAlg_e dropAlgorithm,
                                             tmctl_queueDropAlgExt_t* dropAlgLo_p,
                                             tmctl_queueDropAlgExt_t* dropAlgHi_p)
{
   fapIoctl_tmDropAlg_t fapDropAlgorithm;

   tmctl_debug("Enter: chid=%d dropAlgorithm=%d"
               "redMinThresholdLo=%d redMaxThresholdLo=%d redPercentageLo=%d"
               "redMinThresholdHi=%d redMaxThresholdHi=%d redPercentageHi=%d"
               chid, dropAlgorithm,
               dropAlgLo_p->redMinThreshold, dropAlgLo_p->redMaxThreshold, dropAlgLo_p->redPercentage,
               dropAlgHi_p->redMinThreshold, dropAlgHi_p->redMaxThreshold, dropAlgHi_p->redPercentage);

   if (dropAlgorithm == TMCTL_DROP_RED)
   {
      fapDropAlgorithm = FAP_IOCTL_TM_DROP_ALG_RED;
   }
   else if (dropAlgorithm == TMCTL_DROP_WRED)
   {
      fapDropAlgorithm = FAP_IOCTL_TM_DROP_ALG_WRED;
   }
   else
   {
      fapDropAlgorithm = FAP_IOCTL_TM_DROP_ALG_DT;
   }

   if (fapCtlTm_xtmChannelDropAlgConfigExt(chid, fapDropAlgorithm,
                                           dropAlgLo_p->redPercentage,
                                           dropAlgLo_p->redMinThreshold,
                                           dropAlgLo_p->redMaxThreshold,
                                           dropAlgHi_p->redPercentage,
                                           dropAlgHi_p->redMinThreshold,
                                           dropAlgHi_p->redMaxThreshold))
   {
      tmctl_error("fapCtlTm_xtmChannelDropAlgConfigExt ERROR! chid=%d dropAlgorithm=%d", chid, dropAlgorithm);
      return TMCTL_ERROR;
   }

   return TMCTL_SUCCESS;

} /* End of tmctlFap_setXtmChannelDropAlgExt() */

/* ----------------------------------------------------------------------------
 * This function gets the queue statistics of a FAP TM queue.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    qid (IN) Queue ID.
 *    stats_p (OUT) structure to receive the queue statistics.
 *
 * Return:
 *    tmctl_return_e enum value.
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_getQueueStats(const char* ifname,
                                   int qid,
                                   tmctl_queueStats_t* stats_p)
{
   uint32_t txPackets;
   uint32_t txBytes;
   uint32_t droppedPackets;
   uint32_t droppedBytes;
   int rc;

   tmctl_debug("Enter: ifname=%s qid=%d", ifname, qid);

   if ((rc = fapCtlTm_getQueueStats((char *)ifname, FAP_IOCTL_TM_MODE_MANUAL, qid,
                                    &txPackets, &txBytes, &droppedPackets, &droppedBytes)))
   {
      tmctl_error("fapCtlTm_getQueueStats ERROR! ifname=%s qid=%d", ifname, qid);
      return TMCTL_ERROR;
   }

   memset(stats_p, 0, sizeof(tmctl_queueStats_t));

   stats_p->txPackets  = txPackets;
   stats_p->txBytes  = txBytes;
   stats_p->droppedPackets  = droppedPackets;
   stats_p->droppedBytes  = droppedBytes;

   tmctl_debug("Done: ifname=%s qid=%d txPackets=%u txBytes=%u droppedPackets=%u droppedBytes=%u",
               ifname, qid, stats_p->txPackets, stats_p->txBytes,
               stats_p->droppedPackets, stats_p->droppedBytes);

   return TMCTL_SUCCESS;

} /* End of tmctlFap_getQueueStats() */


/* ----------------------------------------------------------------------------
 * This function gets port TM parameters (capabilities) from fapctl driver.
 *
 * Parameters:
 *    ifname (IN) fapctl interface name.
 *    tmParms_p (OUT) structure to return port TM parameters.
 *
 * Return:
 *    tmctl_ret_e enum value
 * ----------------------------------------------------------------------------
 */
tmctl_ret_e tmctlFap_getPortTmParms(const char* ifname,
                                    tmctl_portTmParms_t* tmParms_p)
{
   int  rc;
   uint32_t  schedCaps;
   int  maxQueues;
   int  maxSpQueues;
   uint8_t portShaper;
   uint8_t queueShaper;

   tmctl_debug("Enter: ifname=%s", ifname);

   if ((rc = fapCtlTm_getPortCapability((char *)ifname, &schedCaps, &maxQueues,
                                        &maxSpQueues, &portShaper, &queueShaper)))
   {
      tmctl_error("fapCtlTm_GetPortTmParms ERROR! ifname=%s", ifname);
      return TMCTL_ERROR;
   }

   memset(tmParms_p, 0, sizeof(tmctl_portTmParms_t));

   if (schedCaps & FAP_TM_SP_CAPABLE)
      tmParms_p->schedCaps |= TMCTL_SP_CAPABLE;
   if (schedCaps & FAP_TM_WRR_CAPABLE)
      tmParms_p->schedCaps |= TMCTL_WRR_CAPABLE;
   if (schedCaps & FAP_TM_WDRR_CAPABLE)
      tmParms_p->schedCaps |= TMCTL_WDRR_CAPABLE;
   if (schedCaps & FAP_TM_WFQ_CAPABLE)
      tmParms_p->schedCaps |= TMCTL_WFQ_CAPABLE;
   if (schedCaps & FAP_TM_SP_WRR_CAPABLE)
      tmParms_p->schedCaps |= TMCTL_SP_WRR_CAPABLE;

   tmParms_p->maxQueues   = maxQueues;
   tmParms_p->maxSpQueues = maxSpQueues;
   tmParms_p->portShaper  = portShaper;
   tmParms_p->queueShaper = queueShaper;

   return TMCTL_SUCCESS;

}  /* End of tmctlFap_getPortTmParms() */

