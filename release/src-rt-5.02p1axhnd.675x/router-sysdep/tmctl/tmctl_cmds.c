/***********************************************************************
 *
 *  Copyright (c) 2014  Broadcom Corporation
 *  All Rights Reserved
 *
<:label-BRCM:2014:proprietary:standard

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


/*****************************************************************************
*    Description:
*
*      TMCtl command line utility.
*
*****************************************************************************/

/* ---- Include Files ----------------------------------------------------- */

#include "tmctl_cmds.h"
#include "tmctl_api_trace.h"
#include <string.h>


/* ---- Private Constants and Types --------------------------------------- */

#define TMCTL_PARMS_BIT_DEVTYPE         0x00000001
#define TMCTL_PARMS_BIT_INTF            0x00000002
#define TMCTL_PARMS_BIT_CFGFLAGS        0x00000004
#define TMCTL_PARMS_BIT_QID             0x00000008
#define TMCTL_PARMS_BIT_QPRIO           0x00000010
#define TMCTL_PARMS_BIT_QSIZE           0x00000020
#define TMCTL_PARMS_BIT_WEIGHT          0x00000040
#define TMCTL_PARMS_BIT_SCHEDMODE       0x00000080
#define TMCTL_PARMS_BIT_SHAPINGRATE     0x00000100
#define TMCTL_PARMS_BIT_BURSTSIZE       0x00000200
#define TMCTL_PARMS_BIT_MINRATE         0x00000400
#define TMCTL_PARMS_BIT_DROPALG         0x00000800
#define TMCTL_PARMS_BIT_REDMINTHR       0x00001000
#define TMCTL_PARMS_BIT_REDMAXTHR       0x00002000
#define TMCTL_PARMS_BIT_REDPCT          0x00004000
#define TMCTL_PARMS_BIT_QPROFLO         0x00008000
#define TMCTL_PARMS_BIT_QPROFHI         0x00010000
#define TMCTL_PARMS_BIT_PRIOMASK0       0x00020000
#define TMCTL_PARMS_BIT_PRIOMASK1       0x00040000
#define TMCTL_PARMS_BIT_REDMINTHRHI     0x00080000
#define TMCTL_PARMS_BIT_REDMAXTHRHI     0x00100000
#define TMCTL_PARMS_BIT_REDPCTHI        0x00200000
#define TMCTL_PARMS_BIT_DSCP            0x00400000
#define TMCTL_PARMS_BIT_PBIT            0x00800000
#define TMCTL_PARMS_BIT_DIR             0x01000000
#define TMCTL_PARMS_BIT_ENABLE          0x02000000
#define TMCTL_PARMS_BIT_QOSTYPE         0x04000000
#define TMCTL_PARMS_BIT_NUMQUEUES       0x08000000
#define TMCTL_PARMS_BIT_MINBUFS         0x10000000

#define TMCTL_CFGKEY_PORT \
 (TMCTL_PARMS_BIT_DEVTYPE | TMCTL_PARMS_BIT_INTF)
#define TMCTL_CFGKEY_QUEUE \
 (TMCTL_PARMS_BIT_DEVTYPE | TMCTL_PARMS_BIT_INTF | TMCTL_PARMS_BIT_QID)
#define TMCTL_CFGKEY_DSCP \
 (TMCTL_PARMS_BIT_DSCP | TMCTL_PARMS_BIT_PBIT)
#define TMCTL_CFGKEY_PBIT \
 (TMCTL_PARMS_BIT_PBIT | TMCTL_PARMS_BIT_QID)
#define TMCTL_CFGKEY_FORCEDSCP \
 (TMCTL_PARMS_BIT_ENABLE | TMCTL_PARMS_BIT_DIR)
#define TMCTL_CFGKEY_PKTQOS \
 (TMCTL_PARMS_BIT_QOSTYPE | TMCTL_PARMS_BIT_DIR)

#define TMCTL_CMD_HELP_PORTMINIT \
  "    tmctl <porttminit|porttmuninit>\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--flag} <cfgflags>\n" \
  "          {--numqueues} <numqueues>\n\n"
#define TMCTL_CMD_HELP_GETQCFG \
  "    tmctl getqcfg\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n\n"
#define TMCTL_CMD_HELP_SETQCFG \
  "    tmctl setqcfg\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n" \
  "          {--priority} <priority>\n" \
  "          {--qsize} <qsize>\n" \
  "          {--weight} <weight>\n" \
  "          {--schedmode} <schedmode>\n" \
  "          {--shapingrate} <shapingrate>\n" \
  "          {--burstsize} <burstsize>\n" \
  "          {--minrate} <minrate>\n" \
  "          {--minbufs} <minbufs>\n\n"
#define TMCTL_CMD_HELP_DELQCFG \
  "    tmctl delqcfg\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n\n"
#define TMCTL_CMD_HELP_GETPORTSHAPER \
  "    tmctl getportshaper\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n\n"
#define TMCTL_CMD_HELP_SETPORTSHAPER \
  "    tmctl setportshaper\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--shapingrate} <shapingrate>\n" \
  "          {--burstsize} <burstsize>\n" \
  "          {--minrate} <minrate>\n\n"
#define TMCTL_CMD_HELP_GETORLSHAPER \
  "    tmctl getorlshaper\n" \
  "          {--devtype} <devicetype>\n\n"
#define TMCTL_CMD_HELP_SETORLSHAPER \
  "    tmctl setorlshaper\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--shapingrate} <shapingrate>\n" \
  "          {--burstsize} <burstsize>\n\n"
#define TMCTL_CMD_HELP_LINKORLSHAPER \
  "    tmctl linkorlshaper\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n\n"
#define TMCTL_CMD_HELP_UNLINKORLSHAPER \
  "    tmctl unlinkorlshaper\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n\n" 
#define TMCTL_CMD_HELP_GETQPROF \
  "    tmctl getqprof\n" \
  "          {--qprofid} <qprofid>\n\n"
#define TMCTL_CMD_HELP_SETQPROF \
  "    tmctl setqprof\n" \
  "          {--qprofid} <qprofid>\n" \
  "          {--redminthr} <redminthr>\n" \
  "          {--redmaxthr} <redmaxthr>\n" \
  "          {--redpct} <redpercentage>\n\n"
#define TMCTL_CMD_HELP_GETQDROPALG \
  "    tmctl getqdropalg\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n\n"
#define TMCTL_CMD_HELP_SETQDROPALG \
  "    tmctl setqdropalg\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n" \
  "          {--dropalg} <dropalgorithm>\n" \
  "          {--loredminthr} <redminthr>\n" \
  "          {--loredmaxthr} <redmaxthr>\n" \
  "          {--loredpct} <redpercentage>\n" \
  "          {--hiredminthr} <redminthr>\n" \
  "          {--hiredmaxthr} <redmaxthr>\n" \
  "          {--hiredpct} <redpercentage>\n" \
  "          {--qprofid} <qprofid>\n" \
  "          {--qprofidhi} <qprofidhi>\n" \
  "          {--priomask0} <hex mask>\n" \
  "          {--priomask1} <hex mask>\n\n"
#define TMCTL_CMD_HELP_SETQDROPALGEXT \
  "    tmctl setqdropalgx\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n" \
  "          {--dropalg} <dropalgorithm>\n" \
  "          {--loredminthr} <redminthr>\n" \
  "          {--loredmaxthr} <redmaxthr>\n" \
  "          {--loredpct} <redpercentage>\n" \
  "          {--hiredminthr} <redminthr>\n" \
  "          {--hiredmaxthr} <redmaxthr>\n" \
  "          {--hiredpct} <redpercentage>\n" \
  "          {--priomask0} <hex mask>\n" \
  "          {--priomask1} <hex mask>\n\n"
#define TMCTL_CMD_HELP_SETQSIZE \
  "    tmctl setqsize\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n" \
  "          {--qsize} <qsize>\n\n"
#define TMCTL_CMD_HELP_SETQSHAPER \
  "    tmctl setqshaper\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n" \
  "          {--minrate} <minrate>\n" \
  "          {--shapingrate} <shapingrate>\n" \
  "          {--burstsize} <burstsize>\n\n"
#define TMCTL_CMD_HELP_GETPORTTMPARMS \
  "    tmctl getporttmparms\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n\n"
#define TMCTL_CMD_HELP_GETQSTATS \
  "    tmctl getqstats\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--qid} <qid>\n\n"
#define TMCTL_CMD_HELP_GETDSCPTOPBIT \
  "    tmctl getdscptopbit\n" \
  "          {--devtype} <optional (enable=1)>\n" \
  "          {--if} <optional (enable=1)>\n" \
  "          {--enable} <enable remark>\n\n"
#define TMCTL_CMD_HELP_SETDSCPTOPBIT \
  "    tmctl setdscptopbit\n" \
  "          {--dscp} <dscp>\n" \
  "          {--pbit} <pbit>\n" \
  "          {--devtype} <optional (enable==1)>\n" \
  "          {--if} <optional (enable==1)>\n" \
  "          {--enable} <enable remark>\n\n"
#define TMCTL_CMD_HELP_GETPBITTOQ \
  "    tmctl getpbittoq\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n\n"
#define TMCTL_CMD_HELP_SETPBITTOQ \
  "    tmctl setpbittoq\n" \
  "          {--devtype} <devicetype>\n" \
  "          {--if} <interface>\n" \
  "          {--pbit} <pbit>\n" \
  "          {--qid} <qid>\n\n"
#define TMCTL_CMD_HELP_GETFORCEDSCP \
  "    tmctl getforcedscp\n" \
  "          {--dir} <direction>\n\n"
#define TMCTL_CMD_HELP_SETFORCEDSCP \
  "    tmctl setforcedscp\n" \
  "          {--dir} <direction>\n" \
  "          {--enable} <enable>\n\n"
#define TMCTL_CMD_HELP_GETPKTBASEDQOS \
  "    tmctl getpktbasedqos\n" \
  "          {--dir} <direction>\n" \
  "          {--qostype} <type>\n\n"
#define TMCTL_CMD_HELP_SETPKTBASEDQOS \
  "    tmctl setpktbasedqos\n" \
  "          {--dir} <direction>\n" \
  "          {--qostype} <type>\n" \
  "          {--enable} <enable>\n\n"
#define TMCTL_CMD_HELP_CREATEPOLICER \
  "    tmctl createpolicer\n" \
  "          {--dir} <direction> (necessary)\n" \
  "          {--pid} <policerindex>\n" \
  "          {--ptype} <policertype> (necessary)\n" \
  "          {--cir} <cir> (necessary)\n" \
  "          {--cbs} <cbs>\n" \
  "          {--ebs} <ebs>\n" \
  "          {--pir} <pir> (necessary if dual rate)\n" \
  "          {--pbs} <pbs>\n\n"
#define TMCTL_CMD_HELP_MODIFYPOLICER \
  "    tmctl modifypolicer\n" \
  "          {--dir} <direction> (necessary)\n" \
  "          {--pid} <policerindex> (necessary)\n" \
  "          {--cir} <cir>\n" \
  "          {--cbs} <cbs>\n" \
  "          {--ebs} <ebs>\n" \
  "          {--pir} <pir>\n" \
  "          {--pbs} <pbs>\n\n"
#define TMCTL_CMD_HELP_DELETEPOLICER \
  "    tmctl deletepolicer\n" \
  "          {--dir} <direction> (necessary)\n" \
  "          {--pid} <policerindex> (necessary)\n\n"
#define TMCTL_PARMS_HELP_DEVTYPE \
  "    devtype = 0(ETH)|1(EPON)|2(GPON)|3(XTM)|4(SVCQ)|5(NONE)\n"
#define TMCTL_PARMS_HELP_INTF \
  "    interface = tcont_id(GPON)|llid(EPON)|ifname(ETH&XTM), 0-based, -1: all tcont_id(GPON)\n"
#define TMCTL_PARMS_HELP_CFGFLAGS \
  "    cfgflags = bit  0   : 0(no)|1(yes), 1: create/delete subsidiary queues\n" \
  "               bit  3   : 0(disable)|1(enable) dual rate\n" \
  "               bits 8-11: 0x0 Port scheduler type SP_WRR combo\n" \
  "                          0x1 Port scheduler type SP\n" \
  "                          0x2 Port scheduler type WRR\n" \
  "                          0x3 Port scheduler type WDRR\n" \
  "                          0x4 Port scheduler type WFQ\n"
#define TMCTL_PARMS_HELP_QID \
  "    qid = <0,7(MAX_Q_PER_TM-1)>, \n"
#define TMCTL_PARMS_HELP_QPRIO \
  "    priority = <0,7(MAX_Q_PRIO-1)>, lower value, lower priority\n"
#define TMCTL_PARMS_HELP_QSIZE  \
  "    qsize = <0,QUEUE_SIZE> (SET not supported)\n"
#define TMCTL_PARMS_HELP_WEIGHT \
  "    weight = <1,63>\n"
#define TMCTL_PARMS_HELP_SCHEDMODE \
  "    schedmode = 1(SP)|2(WRR)|3(WDRR)|4(WFQ)\n"
#define TMCTL_PARMS_HELP_SHAPINGRATE \
  "    shapingrate = kbps, 0: no shaping, -1: not supported\n"
#define TMCTL_PARMS_HELP_BURSTSIZE \
  "    burstsize = shaping burst size in bytes, -1: not supported\n"
#define TMCTL_PARMS_HELP_MINRATE \
  "    minrate = minimum rate in kbps, 0: no shaping, -1: not supported\n"
#define TMCTL_PARMS_HELP_REDMINTHR \
  "    redminthr = RED minimum threshold\n"
#define TMCTL_PARMS_HELP_REDMAXTHR \
  "    redmaxthr = RED maximum threshold\n"
#define TMCTL_PARMS_HELP_REDPCT \
  "    redpercentage = <0,100>, drop probability max_p percentage\n"
#define TMCTL_PARMS_HELP_DROPALG \
  "    dropalgorithm = 0(DT)|1(RED)|2(WRED)\n"
#define TMCTL_PARMS_HELP_QPROFID \
  "    qprofid[hi] = queue profile ID\n"
#define TMCTL_PARMS_HELP_PRIOMASK \
  "    priomask[0|1] = priority mask in hex\n"
#define TMCTL_PARMS_HELP_DSCP \
  "    dscp = <0,63>\n"
#define TMCTL_PARMS_HELP_PBIT \
  "    pbit = <0,7>\n"
#define TMCTL_PARMS_HELP_DIR \
  "    dir = 0(downstream)|1(upstream)\n"
#define TMCTL_PARMS_HELP_ENABLE \
  "    enable = 0(disable)|1(enable)\n"
#define TMCTL_PARMS_HELP_QOSTYPE \
  "    qostype = 0(FC)|1(IC)|2(MCAST)\n"
#define TMCTL_PARMS_HELP_NUMQUEUES \
  "    numqueues = 8|16|32\n"
#define TMCTL_PARMS_HELP_MINBUFS \
  "    minbufs = 0..65536\n"
#define TMCTL_PARMS_HELP_POLICERID \
  "    pid = policer index\n"
#define TMCTL_PARMS_HELP_POLICERTYPE \
  "    ptype = policer type, 0 for single bucket, 1 for srTCM, 2 for trTCM\n"
#define TMCTL_PARMS_HELP_CIR \
  "    cir = rate in bps\n"
#define TMCTL_PARMS_HELP_CBS \
  "    cbs = burst size\n"
#define TMCTL_PARMS_HELP_EBS \
  "    ebs = burst size\n"
#define TMCTL_PARMS_HELP_PIR \
  "    pir = rate in bps\n"
#define TMCTL_PARMS_HELP_PBS \
  "    pbs = burst size\n"

#define UNASSIGNED -1

typedef struct
{
    uint32 bitMask;
    uint32 devType;
    tmctl_if_t ifInfo;
    uint32 cfgFlags;
    int qid;
    int priority;
    int qsize;
    int weight;
    int schedMode;
    int shapingRate;
    int shapingBurstSize;
    int minRate;
    int dropAlgorithm;
    int redMinThreshold;
    int redMaxThreshold;
    int redPercentage;
    int redMinThresholdHi;
    int redMaxThresholdHi;
    int redPercentageHi;
    int queueProfileIdLo;
    int queueProfileIdHi;
    uint32 priorityMask0;
    uint32 priorityMask1;
    uint32 dscp;
    uint32 pbit;
    int dir;
    BOOL enable;
    int qostype;
    int numQueues;
    int minBufs;
    int policerid;
    int policertype;
    int cir;
	int cbs;
} tmctl_cmd_data_t;

typedef struct
{
    int dir;
    int policerid;
    int policertype;
    int cir;
    int cbs;
    int ebs;
    int pir;
    int pbs;
} tmctl_policer_data_t;

/* ---- Private Function Prototypes --------------------------------------- */

#define tmctl_queueCfgInit(devType, qCfg) \
{ \
    if (devType == TMCTL_DEV_GPON) \
    { \
        qCfg.qsize = TMCTL_DEF_TCONT_Q_SZ; \
    } \
    else if (devType == TMCTL_DEV_EPON) \
    { \
        qCfg.qsize = TMCTL_DEF_LLID_Q_SZ; \
    } \
    else \
    { \
        qCfg.qsize = TMCTL_DEF_ETH_Q_SZ_DS; \
    } \
    qCfg.weight = 0; \
    qCfg.minBufs = 0; \
    qCfg.schedMode = TMCTL_SCHED_SP; \
    qCfg.shaper.shapingRate = 0; \
    qCfg.shaper.shapingBurstSize = 0; \
    qCfg.shaper.minRate = 0; \
}

#define tmctl_shaperCfgInit(shaper) \
{ \
    shaper.shapingRate = 0; \
    shaper.shapingBurstSize = 0; \
    shaper.minRate = 0; \
}

#define tmctl_queueProfileCfgInit(qCfg) \
{ \
    qCfg.dropProb = 0; \
    qCfg.minThreshold = 0; \
    qCfg.maxThreshold = 0; \
}

#define tmctl_queueDropAlgCfgInit(qCfg) \
{ \
    qCfg.dropAlgorithm = TMCTL_DROP_DT; \
    qCfg.queueProfileIdLo = 0; \
    qCfg.queueProfileIdHi = 0; \
    qCfg.priorityMask0 = 0; \
    qCfg.priorityMask1 = 0; \
    qCfg.dropAlgLo.redMinThreshold = 0; \
    qCfg.dropAlgLo.redMaxThreshold = 0; \
    qCfg.dropAlgLo.redPercentage = 0; \
    qCfg.dropAlgHi.redMinThreshold = 0; \
    qCfg.dropAlgHi.redMaxThreshold = 0; \
    qCfg.dropAlgHi.redPercentage = 0; \
}

#ifndef TMCTL_API_TRACE
#define tmctl_portTmInitTrace       tmctl_portTmInit
#define tmctl_portTmUninitTrace     tmctl_portTmUninit
#define tmctl_getQueueCfgTrace      tmctl_getQueueCfg
#define tmctl_setQueueCfgTrace      tmctl_setQueueCfg
#define tmctl_delQueueCfgTrace      tmctl_delQueueCfg
#define tmctl_getPortShaperTrace    tmctl_getPortShaper
#define tmctl_setPortShaperTrace    tmctl_setPortShaper
#define tmctl_getOverAllShaperTrace tmctl_getOverAllShaper
#define tmctl_setOverAllShaperTrace tmctl_setOverAllShaper
#define tmctl_linkOverAllShaperTrace tmctl_linkOverAllShaper
#define tmctl_unlinkOverAllShaperTrace tmctl_unlinkOverAllShaper
#define tmctl_getQueueProfileTrace  tmctl_getQueueProfile
#define tmctl_setQueueProfileTrace  tmctl_setQueueProfile
#define tmctl_getQueueDropAlgTrace  tmctl_getQueueDropAlg
#define tmctl_setQueueDropAlgTrace  tmctl_setQueueDropAlg
#define tmctl_setQueueDropAlgExtTrace    tmctl_setQueueDropAlgExt
#define tmctl_setQueueSizeTrace     tmctl_setQueueSize
#define tmctl_setQueueShaperTrace   tmctl_setQueueShaper
#define tmctl_getXtmChannelDropAlgTrace  tmctl_getXtmChannelDropAlg
#define tmctl_setXtmChannelDropAlgTrace  tmctl_setXtmChannelDropAlg
#define tmctl_getQueueStatsTrace    tmctl_getQueueStats
#define tmctl_getPortTmParmsTrace   tmctl_getPortTmParms
#define tmctl_getDscpToPbitTrace    tmctl_getDscpToPbit
#define tmctl_setDscpToPbitTrace    tmctl_setDscpToPbit
#define tmctl_getPbitToQTrace       tmctl_getPbitToQ
#define tmctl_setPbitToQTrace       tmctl_setPbitToQ
#define tmctl_getForceDscpToPbitTrace    tmctl_getForceDscpToPbit
#define tmctl_setForceDscpToPbitTrace    tmctl_setForceDscpToPbit
#define tmctl_getPktBasedQosTrace   tmctl_getPktBasedQos
#define tmctl_setPktBasedQosTrace   tmctl_setPktBasedQos
#define tmctl_createPolicerTrace    tmctl_createPolicer
#define tmctl_modifyPolicerTrace    tmctl_modifyPolicer
#define tmctl_deletePolicerTrace    tmctl_deletePolicer

#endif /* TMCTL_API_TRACE */

#define tmctlPrint printf

static int tmCtl_cmdDataHelper(OPTION_INFO *pOptions, int optNum,
  tmctl_cmd_data_t *pTmctlData);
static int tmCtl_policerDataHandler(OPTION_INFO *pOptions, int optNum,
  tmctl_policer_data_t *pTmctlData);

static void tmctl_portTmParmsPrint(tmctl_portTmParms_t *pTmParms);
static void tmctl_queueCfgPrint(tmctl_queueCfg_t *pQueueCfg);
static void tmctl_shaperCfgPrint(tmctl_shaper_t *pShaperCfg);
static void tmctl_queueProfilePrint(tmctl_queueProfile_t *pQueueProf);
static void tmctl_queueDropAlgPrint(tmctl_queueDropAlg_t *pQueueCfg);
static void tmctl_queueStatsPrint(tmctl_queueStats_t *pQueueCfg);
static void tmctl_dscpToPbitPrint(tmctl_dscpToPbitCfg_t *pDscpToPbitCfg);
static void tmctl_PbitToQPrint(tmctl_pbitToQCfg_t *pPbitToQCfg);
static void tmctl_ForceDscpPrint(BOOL *pEnable);
static void tmctl_PktBasedQosPrint(BOOL *pEnable);
static void tmctl_cmdDataToQueueCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueCfg_t *pQueueCfg);
static void tmctl_cmdDataToShaperCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_shaper_t *pShaperCfg);
static void tmctl_cmdDataToQProfCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueProfile_t *pQProfCfg) ;
static void tmctl_cmdDataToDropAlgCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueDropAlg_t *pQueueCfg) ;
static void tmctl_cmdDataToDropAlgExtCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueDropAlg_t *pQueueCfg);
static int tmctl_helpHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_portTmInitHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_portTmUnInitHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getQCfgHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setQCfgHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_delQCfgHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getPortShaperHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setPortShaperHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getOverAllShaperHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setOverAllShaperHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_linkOverAllShaperHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_unlinkOverAllShaperHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getQProfHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setQProfHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getQDropAlgHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setQDropAlgHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setQueueSizeHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setQueueShaperHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getPortTmParmsHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setQDropAlgExtHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getQStatsHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getDscpToPbitHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setDscpToPbitHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getPbitToQHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setPbitToQHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getForceDscpHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setForceDscpHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_getPktBasedQosHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_setPktBasedQosHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_createPolicerHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_modifyPolicerHandler(OPTION_INFO *pOptions, int optNum);
static int tmctl_deletePolicerHandler(OPTION_INFO *pOptions, int optNum);


/* ---- Public Variables -------------------------------------------------- */

COMMAND_INFO tmctl_cmds[] =
{
    {"--help", {""}, tmctl_helpHandler},
    {
        "porttminit",
        {
            "--devtype", "--if", "--flag", "--numqueues"
        },
        tmctl_portTmInitHandler
    },
    {
        "porttmuninit",
        {
            "--devtype", "--if", "--flag"
        },
        tmctl_portTmUnInitHandler
    },
    {
        "getqcfg",
        {
            "--devtype", "--if", "--qid"
        },
        tmctl_getQCfgHandler
    },
    {
        "setqcfg",
        {
            "--devtype", "--if", "--qid", "--priority", "--qsize",
            "--weight", "--schedmode", "--shapingrate", "--burstsize",
            "--minrate", "--minbufs"
        },
        tmctl_setQCfgHandler
    },
    {
        "delqcfg",
        {
            "--devtype", "--if", "--qid"
        },
        tmctl_delQCfgHandler
    },
    {
        "getportshaper",
        {
            "--devtype", "--if"
        },
        tmctl_getPortShaperHandler
    },
    {
        "setportshaper",
        {
          "--devtype", "--if", "--shapingrate", "--burstsize",
          "--minrate"
        },
        tmctl_setPortShaperHandler
    },
	{
        "getorlshaper",
        {
          "--devtype" 
        },
        tmctl_getOverAllShaperHandler
    },
    {
        "setorlshaper",
        {
          "--devtype", "--shapingrate", "--burstsize",
        },
        tmctl_setOverAllShaperHandler
    },
    {
        "linkorlshaper",
        {
          "--devtype", "--if"
        },
        tmctl_linkOverAllShaperHandler
    },
    {
        "unlinkorlshaper",
        {
          "--devtype", "--if"
        },
        tmctl_unlinkOverAllShaperHandler
    },
    {
        "getqprof",
        {
            "--qprofid"
        },
        tmctl_getQProfHandler
    },
    {
        "setqprof",
        {
            "--qprofid", "--redminthr", "--redmaxthr", "--redpct"
        },
        tmctl_setQProfHandler
    },
    {
        "getqdropalg",
        {
          "--devtype", "--if", "--qid"
        },
        tmctl_getQDropAlgHandler
    },
    {
        "setqdropalg",
        {
            "--devtype", "--if", "--qid", "--dropalg",
            "--loredminthr", "--loredmaxthr", "--loredpct",
            "--hiredminthr", "--hiredmaxthr", "--hiredpct",
            "--qprofid", "--qprofidhi", "--priomask0", "--priomask1"
        },
        tmctl_setQDropAlgHandler
    },
    {
        "setqdropalgx",
        {
            "--devtype", "--if", "--qid", "--dropalg",
            "--loredminthr", "--loredmaxthr", "--loredpct",
            "--hiredminthr", "--hiredmaxthr", "--hiredpct",
            "--priomask0", "--priomask1"
        },
        tmctl_setQDropAlgExtHandler
    },
    {
        "setqsize",
        {
            "--devtype", "--if", "--qid", "--qsize"
        },
        tmctl_setQueueSizeHandler
    },
    {
        "setqshaper",
        {
            "--devtype", "--if", "--qid",
            "--minrate", "--shapingrate", "--burstsize"
        },
        tmctl_setQueueShaperHandler
    },
    {
        "getporttmparms",
        {
            "--devtype", "--if"
        },
        tmctl_getPortTmParmsHandler
    },
    {
        "getqstats",
        {
            "--devtype", "--if", "--qid"
        },
        tmctl_getQStatsHandler
    },
    {
        "getdscptopbit",
        {
            "--devtype", "--if", "--enable"
        },
        tmctl_getDscpToPbitHandler
    },
    {
        "setdscptopbit",
        {
            "--dscp", "--pbit", "--devtype", "--if", "--enable"
        },
        tmctl_setDscpToPbitHandler
    },
    {
        "getpbittoq",
        {
            "--devtype", "--if"
        },
        tmctl_getPbitToQHandler
    },
    {
        "setpbittoq",
        {
            "--devtype", "--if", "--pbit", "--qid"
        },
        tmctl_setPbitToQHandler
    },
    {
        "getforcedscp",
        {
            "--dir"
        },
        tmctl_getForceDscpHandler
    },
    {
        "setforcedscp",
        {
            "--dir", "--enable"
        },
        tmctl_setForceDscpHandler
    },
    {
        "getpktbasedqos",
        {
            "--dir", "--qostype"
        },
        tmctl_getPktBasedQosHandler
    },
    {
        "setpktbasedqos",
        {
            "--dir", "--qostype", "--enable"
        },
        tmctl_setPktBasedQosHandler
    },
    {
        "createpolicer",
        {
            "--dir", "--pid", "--ptype", "--cir", "--cbs", "--ebs", "--pir", "--pbs"
        },
        tmctl_createPolicerHandler
    },
    {
        "modifypolicer",
        {
            "--dir", "--pid", "--cir", "--cbs", "--ebs", "--pir", "--pbs"
        },
        tmctl_modifyPolicerHandler
    },
    {
        "deletepolicer",
        {
            "--dir", "--pid"
        },
        tmctl_deletePolicerHandler
    }
};


/* ---- Private Variables ------------------------------------------------- */

static char *pgmName = "tmctl";


/* ---- Functions --------------------------------------------------------- */


/*****************************************************************************
*  FUNCTION:  tmctl_usage
*  PURPOSE:   TMCtl command usage function.
*  PARAMETERS:
*      None.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
void tmctl_usage(void)
{
    printf(
        "Traffic management control (TMCtl) utility:\n"
        "  Usage:\n"
        "    tmctl <command> <parameters ...>\n\n"
        "  Commands:\n"
        TMCTL_CMD_HELP_PORTMINIT
        TMCTL_CMD_HELP_GETQCFG
        TMCTL_CMD_HELP_SETQCFG
        TMCTL_CMD_HELP_DELQCFG
        TMCTL_CMD_HELP_GETPORTSHAPER
        TMCTL_CMD_HELP_SETPORTSHAPER
        TMCTL_CMD_HELP_GETORLSHAPER
        TMCTL_CMD_HELP_SETORLSHAPER
        TMCTL_CMD_HELP_LINKORLSHAPER
        TMCTL_CMD_HELP_UNLINKORLSHAPER
        TMCTL_CMD_HELP_GETQPROF
        TMCTL_CMD_HELP_SETQPROF
        TMCTL_CMD_HELP_GETQDROPALG
        TMCTL_CMD_HELP_SETQDROPALG
        TMCTL_CMD_HELP_SETQSIZE
        TMCTL_CMD_HELP_SETQSHAPER
        TMCTL_CMD_HELP_SETQDROPALGEXT
        TMCTL_CMD_HELP_GETPORTTMPARMS
        TMCTL_CMD_HELP_GETQSTATS
        TMCTL_CMD_HELP_GETDSCPTOPBIT
        TMCTL_CMD_HELP_SETDSCPTOPBIT
        TMCTL_CMD_HELP_GETPBITTOQ
        TMCTL_CMD_HELP_SETPBITTOQ
        TMCTL_CMD_HELP_GETFORCEDSCP
        TMCTL_CMD_HELP_SETFORCEDSCP
        TMCTL_CMD_HELP_GETPKTBASEDQOS
        TMCTL_CMD_HELP_SETPKTBASEDQOS
        TMCTL_CMD_HELP_CREATEPOLICER
        TMCTL_CMD_HELP_MODIFYPOLICER
        TMCTL_CMD_HELP_DELETEPOLICER
        "  Parameters:\n"
        TMCTL_PARMS_HELP_DEVTYPE
        TMCTL_PARMS_HELP_INTF
        TMCTL_PARMS_HELP_CFGFLAGS
        TMCTL_PARMS_HELP_QID
        TMCTL_PARMS_HELP_QPRIO
        TMCTL_PARMS_HELP_QSIZE
        TMCTL_PARMS_HELP_WEIGHT
        TMCTL_PARMS_HELP_SCHEDMODE
        TMCTL_PARMS_HELP_SHAPINGRATE
        TMCTL_PARMS_HELP_BURSTSIZE
        TMCTL_PARMS_HELP_MINRATE
        TMCTL_PARMS_HELP_REDMINTHR
        TMCTL_PARMS_HELP_REDMAXTHR
        TMCTL_PARMS_HELP_REDPCT
        TMCTL_PARMS_HELP_DROPALG
        TMCTL_PARMS_HELP_QPROFID
        TMCTL_PARMS_HELP_PRIOMASK
        TMCTL_PARMS_HELP_DSCP
        TMCTL_PARMS_HELP_PBIT
        TMCTL_PARMS_HELP_DIR
        TMCTL_PARMS_HELP_ENABLE
        TMCTL_PARMS_HELP_QOSTYPE
        TMCTL_PARMS_HELP_NUMQUEUES
        TMCTL_PARMS_HELP_CIR
        TMCTL_PARMS_HELP_POLICERTYPE
        TMCTL_PARMS_HELP_POLICERID
    );
}

/*****************************************************************************
*  FUNCTION:  tmCtl_cmdDataHelper
*  PURPOSE:   TMCtl command helper function.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*      pTmctlData - pointer to TMCtl data.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmCtl_cmdDataHelper(OPTION_INFO *pOptions, int optNum,
  tmctl_cmd_data_t *pTmctlData)
{
    int optNumOffset;
    int ifIdx;
    char *ifName = NULL;
    tmctl_cmd_data_t cmdData;

    optNumOffset = optNum;
    memset(pTmctlData, 0x0, sizeof(tmctl_cmd_data_t));
    memset(&cmdData, 0x0, sizeof(tmctl_cmd_data_t));

    while (optNumOffset)
    {
        if (!strcasecmp(pOptions->pOptName, "--devtype"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_DEVTYPE);
                return TMCTL_INVALID_OPTION;
            }

            cmdData.bitMask |= TMCTL_PARMS_BIT_DEVTYPE;
            cmdData.devType = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--if"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_INTF);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_INTF;
            ifName = pOptions->pParms[0];
        }
        else if (!strcasecmp(pOptions->pOptName, "--flag"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_CFGFLAGS);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_CFGFLAGS;
            cmdData.cfgFlags = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--qid"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_QID);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_QID;
            cmdData.qid = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--priority"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_QPRIO);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_QPRIO;
            cmdData.priority = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--qsize"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_QSIZE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_QSIZE;
            cmdData.qsize = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--weight"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_WEIGHT);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_WEIGHT;
            cmdData.weight = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--schedmode"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_SCHEDMODE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_SCHEDMODE;
            cmdData.schedMode = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--shapingrate"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_SHAPINGRATE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_SHAPINGRATE;
            cmdData.shapingRate = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--burstsize"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_BURSTSIZE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_BURSTSIZE;
            cmdData.shapingBurstSize = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--minrate"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_MINRATE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_MINRATE;
            cmdData.minRate = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--dropalg"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_DROPALG);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_DROPALG;
            cmdData.dropAlgorithm = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if ((!strcasecmp(pOptions->pOptName, "--redminthr")) ||
          (!strcasecmp(pOptions->pOptName, "--loredminthr")))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_REDMINTHR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_REDMINTHR;
            cmdData.redMinThreshold = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if ((!strcasecmp(pOptions->pOptName, "--redmaxthr")) ||
          (!strcasecmp(pOptions->pOptName, "--loredmaxthr")))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_REDMAXTHR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_REDMAXTHR;
            cmdData.redMaxThreshold = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if ((!strcasecmp(pOptions->pOptName, "--redpct")) ||
          (!strcasecmp(pOptions->pOptName, "--loredpct")))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_REDPCT);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_REDPCT;
            cmdData.redPercentage = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--hiredminthr"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_REDMINTHR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_REDMINTHRHI;
            cmdData.redMinThresholdHi = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--hiredmaxthr"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_REDMAXTHR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_REDMAXTHRHI;
            cmdData.redMaxThresholdHi = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--hiredpct"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_REDPCT);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_REDPCTHI;
            cmdData.redPercentageHi = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--qprofid"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_QPROFID);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_QPROFLO;
            cmdData.queueProfileIdLo = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--qprofidhi"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_QPROFID);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_QPROFHI;
            cmdData.queueProfileIdHi = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--priomask0"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_PRIOMASK);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_PRIOMASK0;
            cmdData.priorityMask0 = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--priomask1"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_PRIOMASK);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_PRIOMASK1;
            cmdData.priorityMask1 = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--dscp"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_DSCP);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_DSCP;
            cmdData.dscp = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--pbit"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_PBIT);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_PBIT;
            cmdData.pbit = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--dir"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_DIR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_DIR;
            cmdData.dir = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--enable"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_ENABLE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_ENABLE;
            cmdData.enable = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--qostype"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_QOSTYPE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_QOSTYPE;
            cmdData.qostype = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--numqueues"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_NUMQUEUES);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_NUMQUEUES;
            cmdData.numQueues = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--minbufs"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_MINBUFS);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.bitMask |= TMCTL_PARMS_BIT_MINBUFS;
            cmdData.minBufs = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else
        {
            fprintf(stderr, "%s: invalid option [%s]\n",
             pgmName, pOptions->pOptName);
            return TMCTL_INVALID_OPTION;
        }

        optNumOffset--;
        pOptions++;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) == TMCTL_CFGKEY_PORT)
    {
        if (cmdData.devType == TMCTL_DEV_ETH)
        {
            cmdData.ifInfo.ethIf.ifname = ifName;
        }
        else if (cmdData.devType == TMCTL_DEV_XTM)
        {
            cmdData.ifInfo.xtmIf.ifname = ifName;
        }
        else if (cmdData.devType == TMCTL_DEV_EPON)
        {
            ifIdx = strtoul(ifName, NULL, 0);
            cmdData.ifInfo.eponIf.llid = ifIdx;
        }
        else if (cmdData.devType == TMCTL_DEV_GPON)
        {
            ifIdx = strtoul(ifName, NULL, 0);
            cmdData.ifInfo.gponIf.tcontid = ifIdx;
        }
    }

#ifdef TMCTL_VALIDATE_PARMS
    /* TODO: update during the integration on BCM68380 platform. */
    if (cmdData.bitMask & TMCTL_PARMS_BIT_DEVTYPE)
    {
        if (cmdData.devType > TMCTL_DEV_XTM)
        {
            return TMCTL_INVALID_PARAMETER;
        }
    }

    if (cmdData.bitMask & TMCTL_PARMS_BIT_QID)
    {
        if (qid > MAX_TMCTL_QUEUES_BASELINE)
        {
            return TMCTL_INVALID_PARAMETER;
        }
    }
#endif

    memcpy(pTmctlData, &cmdData, sizeof(tmctl_cmd_data_t));

    return TMCTL_CMD_PARSE_OK;
}

/*****************************************************************************
*  FUNCTION:  tmCtl_policerDataHandler
*  PURPOSE:   TMCtl command helper function.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*      pTmctlData - pointer to TMCtl policer data.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmCtl_policerDataHandler(OPTION_INFO *pOptions, int optNum,
  tmctl_policer_data_t *pTmctlData)
{
    int optNumOffset;
    tmctl_policer_data_t cmdData;
    
    optNumOffset = optNum;
    memset(pTmctlData, 0x0, sizeof(tmctl_policer_data_t));
    memset(&cmdData, 0x0, sizeof(tmctl_policer_data_t));
    cmdData.dir = UNASSIGNED;
    cmdData.policerid = UNASSIGNED;
    cmdData.cir = UNASSIGNED;
    cmdData.cbs = UNASSIGNED;
    cmdData.ebs = UNASSIGNED;
    cmdData.pir = UNASSIGNED;
    cmdData.pbs = UNASSIGNED;
    
    while (optNumOffset)
    {
        if (!strcasecmp(pOptions->pOptName, "--dir"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_DIR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.dir = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--pid"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_POLICERID);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.policerid = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--ptype"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_POLICERTYPE);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.policertype = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--cir"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_CIR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.cir = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--cbs"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_CBS);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.cbs = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--ebs"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_EBS);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.ebs = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--pir"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_PIR);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.pir = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else if (!strcasecmp(pOptions->pOptName, "--pbs"))
        {
            if (pOptions->nNumParms != 1)
            {
                fprintf(stderr, "%s: %s\n",
                  pgmName, TMCTL_PARMS_HELP_PBS);
                return TMCTL_INVALID_OPTION;
            }
            cmdData.pbs = strtoul(pOptions->pParms[0], NULL, 0);
        }
        else
        {
            fprintf(stderr, "%s: invalid option [%s]\n",
             pgmName, pOptions->pOptName);
            return TMCTL_INVALID_OPTION;
        }
        
        optNumOffset--;
        pOptions++;
    }
    memcpy(pTmctlData, &cmdData, sizeof(tmctl_policer_data_t));
    return TMCTL_CMD_PARSE_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_portTmParmsPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pTmParms - pointer to TMCtl port TM capability.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_portTmParmsPrint(tmctl_portTmParms_t *pTmParms)
{
    if (pTmParms == NULL)
    {
        fprintf(stderr, "%s: invalid tmparms pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    sched caps    : 0x%08x\n", pTmParms->schedCaps);
    tmctlPrint("    max queues    : %d\n", pTmParms->maxQueues);
    tmctlPrint("    max sp queues : %d\n", pTmParms->maxSpQueues);
    tmctlPrint("    port shaper   : %d\n", pTmParms->portShaper);
    tmctlPrint("    queue shaper  : %d\n", pTmParms->queueShaper);
    tmctlPrint("    config flags  : 0x%08x\n", pTmParms->cfgFlags);
    tmctlPrint("    num queues    : %d\n", pTmParms->numQueues);
    tmctlPrint("    dual rate     : %d\n", pTmParms->dualRate);
}

/*****************************************************************************
*  FUNCTION:  tmctl_queueCfgPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_queueCfgPrint(tmctl_queueCfg_t *pQueueCfg)
{
    if (pQueueCfg == NULL)
    {
        fprintf(stderr, "%s: invalid queueCfg pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    qid      : %d\n", pQueueCfg->qid);
    tmctlPrint("    priority : %d\n", pQueueCfg->priority);
    tmctlPrint("    qsize    : %d\n", pQueueCfg->qsize);
    tmctlPrint("    weight   : %d\n", pQueueCfg->weight);
    tmctlPrint("    minBufs  : %d\n", pQueueCfg->minBufs);
    tmctlPrint("    schedMode: %d\n", pQueueCfg->schedMode);

    tmctl_shaperCfgPrint(&pQueueCfg->shaper);
}

/*****************************************************************************
*  FUNCTION:  tmctl_shaperCfgPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pShaperCfg - pointer to TMCtl shaper config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_shaperCfgPrint(tmctl_shaper_t *pShaperCfg)
{
    if (pShaperCfg == NULL)
    {
        fprintf(stderr, "%s: invalid shapercfg pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    shaper   : (%d, %d, %d)\n",
      pShaperCfg->shapingRate, pShaperCfg->shapingBurstSize,
      pShaperCfg->minRate);
}

/*****************************************************************************
*  FUNCTION:  tmctl_shaperCfgPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pShaperCfg - pointer to TMCtl shaper config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_overAllShaperInfoPrint(uint32 devType, tmctl_if_t ifInfo, tmctl_shaper_t *pShaperCfg)
{
    int portId = 0;
    
    if (pShaperCfg == NULL)
    {
        fprintf(stderr, "%s: invalid shapercfg pointer.\n", pgmName);
        return;
    }
    
    tmctlPrint("    shaper   : (Rate=%d, Burst=%d)\n",
      pShaperCfg->shapingRate, pShaperCfg->shapingBurstSize);
    
    tmctlPrint("    ports    : \n");

    if (devType == TMCTL_DEV_GPON)
    {
        for (portId = 0; portId < TMCTL_CMD_MAX_TEST_BITS; portId++)
        {
            if (TestBitsSet(ifInfo.portId.portMap, 1U << portId))    
            {
                tmctlPrint("               tcont%d\n", portId + 1);
            }
        }
    }
}

/*****************************************************************************
*  FUNCTION:  tmctl_queueProfilePrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl queue profile data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_queueProfilePrint(tmctl_queueProfile_t *pQueueProf)
{
    if (pQueueProf == NULL)
    {
        fprintf(stderr, "%s: invalid queueCfg pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    minimum threshold : %d\n", pQueueProf->minThreshold);
    tmctlPrint("    maximum threshold : %d\n", pQueueProf->maxThreshold);
    tmctlPrint("    drop probability  : %d\n", pQueueProf->dropProb);
}

/*****************************************************************************
*  FUNCTION:  tmctl_queueDropAlgPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_queueDropAlgPrint(tmctl_queueDropAlg_t *pQueueCfg)
{
    if (pQueueCfg == NULL)
    {
        fprintf(stderr, "%s: invalid queueCfg pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    algorithm  : %d\n", pQueueCfg->dropAlgorithm);

    if (pQueueCfg->dropAlgorithm == TMCTL_DROP_RED)
    {
        if (pQueueCfg->queueProfileIdLo != 0)
        {
            tmctlPrint("    queue profile ID: %d\n", pQueueCfg->queueProfileIdLo);
        }
        else
        {
            tmctlPrint("    queue drop minimum threshold: %d\n", pQueueCfg->dropAlgLo.redMinThreshold);
            tmctlPrint("    queue drop maximum threshold: %d\n", pQueueCfg->dropAlgLo.redMaxThreshold);
            tmctlPrint("    queue drop probability: %d\n", pQueueCfg->dropAlgLo.redPercentage);
        }
    }
    else if (pQueueCfg->dropAlgorithm == TMCTL_DROP_WRED)
    {
        if ((pQueueCfg->queueProfileIdLo != 0) || (pQueueCfg->queueProfileIdHi != 0))
        {
            tmctlPrint("    queue profile ID Lo: %d\n", pQueueCfg->queueProfileIdLo);
            tmctlPrint("    queue profile ID Hi: %d\n", pQueueCfg->queueProfileIdHi);
        }
        else
        {
            tmctlPrint("    low class drop minimum threshold: %d\n", pQueueCfg->dropAlgLo.redMinThreshold);
            tmctlPrint("    low class drop maximum threshold: %d\n", pQueueCfg->dropAlgLo.redMaxThreshold);
            tmctlPrint("    low class drop probability: %d\n", pQueueCfg->dropAlgLo.redPercentage);
            tmctlPrint("    high class drop minimum threshold: %d\n", pQueueCfg->dropAlgHi.redMinThreshold);
            tmctlPrint("    high class drop maximum threshold: %d\n", pQueueCfg->dropAlgHi.redMaxThreshold);
            tmctlPrint("    high class drop probability: %d\n", pQueueCfg->dropAlgHi.redPercentage);
        }
        tmctlPrint("    priority mask#0: 0x%x\n", pQueueCfg->priorityMask0);
        tmctlPrint("    priority mask#1: 0x%x\n", pQueueCfg->priorityMask1);
    }
}

/*****************************************************************************
*  FUNCTION:  tmctl_queueStatsPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_queueStatsPrint(tmctl_queueStats_t *pQueueCfg)
{
    if (pQueueCfg == NULL)
    {
        fprintf(stderr, "%s: invalid queueCfg pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    txPackets:      %u\n", pQueueCfg->txPackets);
    tmctlPrint("    txBytes:        %u\n", pQueueCfg->txBytes);
    tmctlPrint("    droppedPackets: %u\n", pQueueCfg->droppedPackets);
    tmctlPrint("    droppedBytes:   %u\n", pQueueCfg->droppedBytes);
}


/*****************************************************************************
*  FUNCTION:  tmctl_dscpToPbitPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl dscp to pbit config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_dscpToPbitPrint(tmctl_dscpToPbitCfg_t *pDscpToPbitCfg)
{
    int i = 0;

    if (pDscpToPbitCfg == NULL)
    {
        fprintf(stderr, "%s: invalid dscptToPbitCfg pointer.\n", pgmName);
        return;
    }

    for (i = 0; i < TOTAL_DSCP_NUM; i++)
        tmctlPrint("    dscp[%d] : %d\n", i, pDscpToPbitCfg->dscp[i]);
}


/*****************************************************************************
*  FUNCTION:  tmctl_PbitToQPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl pbit to queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_PbitToQPrint(tmctl_pbitToQCfg_t *pPbitToQCfg)
{
    int i = 0;

    if (pPbitToQCfg == NULL)
    {
        fprintf(stderr, "%s: invalid PbitToQCfg pointer.\n", pgmName);
        return;
    }

    for (i = 0; i < TOTAL_PBIT_NUM; i++)
        tmctlPrint("    pbit[%d] : %d\n", i, pPbitToQCfg->pbit[i]);
}


/*****************************************************************************
*  FUNCTION:  tmctl_ForceDscpPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl force dscp config.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_ForceDscpPrint(BOOL *pEnable)
{
    if (pEnable == NULL)
    {
        fprintf(stderr, "%s: invalid ForceDscpCfg pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    enable : %d\n", (*pEnable));
}


/*****************************************************************************
*  FUNCTION:  tmctl_PktBasedQosPrint
*  PURPOSE:   TMCtl command print helper function.
*  PARAMETERS:
*      pQueueCfg - pointer to TMCtl packet based qos config.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_PktBasedQosPrint(BOOL *pEnable)
{
    if (pEnable == NULL)
    {
        fprintf(stderr, "%s: invalid pktBasedQosCfg pointer.\n", pgmName);
        return;
    }

    tmctlPrint("    enable : %d\n", (*pEnable));
}


/*****************************************************************************
*  FUNCTION:  tmctl_cmdDataToQueueCfg
*  PURPOSE:   TMCtl command data to API data convert function.
*  PARAMETERS:
*      pCmdData - pointer to command data.
*      pQueueCfg - pointer to TMCtl API queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_cmdDataToQueueCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueCfg_t *pQueueCfg)
{
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_QID)
    {
        pQueueCfg->qid = pCmdData->qid;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_QPRIO)
    {
        pQueueCfg->priority = pCmdData->priority;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_QSIZE)
    {
        pQueueCfg->qsize = pCmdData->qsize;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_WEIGHT)
    {
        pQueueCfg->weight = pCmdData->weight;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_SCHEDMODE)
    {
        pQueueCfg->schedMode = pCmdData->schedMode;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_MINBUFS)
    {
        pQueueCfg->minBufs = pCmdData->minBufs;
    }

    tmctl_cmdDataToShaperCfg(pCmdData, &pQueueCfg->shaper);
}

/*****************************************************************************
*  FUNCTION:  tmctl_cmdDataToShaperCfg
*  PURPOSE:   TMCtl command data to API data convert function.
*  PARAMETERS:
*      pCmdData - pointer to command data.
*      pShaperCfg - pointer to TMCtl API shaper config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_cmdDataToShaperCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_shaper_t *pShaperCfg)
{
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_SHAPINGRATE)
    {
        pShaperCfg->shapingRate = pCmdData->shapingRate;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_BURSTSIZE)
    {
        pShaperCfg->shapingBurstSize = pCmdData->shapingBurstSize;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_MINRATE)
    {
        pShaperCfg->minRate = pCmdData->minRate;
    }
}

/*****************************************************************************
*  FUNCTION:  tmctl_cmdDataToQProfCfg
*  PURPOSE:   TMCtl command data to API data convert function.
*  PARAMETERS:
*      pCmdData - pointer to command data.
*      pQueueCfg - pointer to TMCtl API queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_cmdDataToQProfCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueProfile_t *pQProfCfg)
{
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMINTHR)
    {
        pQProfCfg->minThreshold = pCmdData->redMinThreshold;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMAXTHR)
    {
        pQProfCfg->maxThreshold = pCmdData->redMaxThreshold;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDPCT)
    {
        pQProfCfg->dropProb = pCmdData->redPercentage;
    }
}

/*****************************************************************************
*  FUNCTION:  tmctl_cmdDataToDropAlgCfg
*  PURPOSE:   TMCtl command data to API data convert function.
*  PARAMETERS:
*      pCmdData - pointer to command data.
*      pQueueCfg - pointer to TMCtl API queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_cmdDataToDropAlgCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueDropAlg_t *pQueueCfg)
{
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_DROPALG)
    {
        pQueueCfg->dropAlgorithm = pCmdData->dropAlgorithm;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMINTHR)
    {
        pQueueCfg->dropAlgLo.redMinThreshold = pCmdData->redMinThreshold;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMAXTHR)
    {
        pQueueCfg->dropAlgLo.redMaxThreshold = pCmdData->redMaxThreshold;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDPCT)
    {
        pQueueCfg->dropAlgLo.redPercentage = pCmdData->redPercentage;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMINTHRHI)
    {
        pQueueCfg->dropAlgHi.redMinThreshold = pCmdData->redMinThresholdHi;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMAXTHRHI)
    {
        pQueueCfg->dropAlgHi.redMaxThreshold = pCmdData->redMaxThresholdHi;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDPCTHI)
    {
        pQueueCfg->dropAlgHi.redPercentage = pCmdData->redPercentageHi;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_QPROFLO)
    {
        pQueueCfg->queueProfileIdLo = pCmdData->queueProfileIdLo;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_QPROFHI)
    {
        pQueueCfg->queueProfileIdHi = pCmdData->queueProfileIdHi;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_PRIOMASK0)
    {
        pQueueCfg->priorityMask0 = pCmdData->priorityMask0;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_PRIOMASK1)
    {
        pQueueCfg->priorityMask1 = pCmdData->priorityMask1;
    }
}

/*****************************************************************************
*  FUNCTION:  tmctl_cmdDataToDropAlgCfg
*  PURPOSE:   TMCtl command data to API data convert function.
*  PARAMETERS:
*      pCmdData - pointer to command data.
*      pQueueCfg - pointer to TMCtl API queue config data.
*  RETURNS:
*      None.
*  NOTES:
*      None.
*****************************************************************************/
static void tmctl_cmdDataToDropAlgExtCfg(tmctl_cmd_data_t *pCmdData,
  tmctl_queueDropAlg_t *pQueueCfg)
{
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_DROPALG)
    {
        pQueueCfg->dropAlgorithm = pCmdData->dropAlgorithm;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMINTHR)
    {
        pQueueCfg->dropAlgLo.redMinThreshold = pCmdData->redMinThreshold;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMAXTHR)
    {
        pQueueCfg->dropAlgLo.redMaxThreshold = pCmdData->redMaxThreshold;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDPCT)
    {
        pQueueCfg->dropAlgLo.redPercentage = pCmdData->redPercentage;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMINTHRHI)
    {
        pQueueCfg->dropAlgHi.redMinThreshold = pCmdData->redMinThresholdHi;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDMAXTHRHI)
    {
        pQueueCfg->dropAlgHi.redMaxThreshold = pCmdData->redMaxThresholdHi;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_REDPCTHI)
    {
        pQueueCfg->dropAlgHi.redPercentage = pCmdData->redPercentageHi;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_PRIOMASK0)
    {
        pQueueCfg->priorityMask0 = pCmdData->priorityMask0;
    }
    if (pCmdData->bitMask & TMCTL_PARMS_BIT_PRIOMASK1)
    {
        pQueueCfg->priorityMask1 = pCmdData->priorityMask1;
    }
}

/*****************************************************************************
*  FUNCTION:  tmctl_helpHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_helpHandler(OPTION_INFO *pOptions __attribute__((unused)), int optNum __attribute__((unused)))
{
    tmctl_usage();
    tmctlDebugPrint("options:%p, num:%d\n", pOptions, optNum);
    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_portTmInitHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_portTmInitHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_PORTMINIT);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_PORTMINIT);
        return TMCTL_INVALID_OPTION;
    }

    if ((cmdData.bitMask & TMCTL_PARMS_BIT_NUMQUEUES) != TMCTL_PARMS_BIT_NUMQUEUES)
        cmdData.numQueues = UNASSIGNED;

    tmctlRet = tmctl_portTmInitTrace(cmdData.devType, &cmdData.ifInfo,
      cmdData.cfgFlags, cmdData.numQueues);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_portTmInit() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_portTmUnInitHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_portTmUnInitHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_PORTMINIT);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_PORTMINIT);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet = tmctl_portTmUninitTrace(cmdData.devType, &cmdData.ifInfo);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_portTmUninit() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_getQCfgHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getQCfgHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueCfg_t queueCfg;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQCFG);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQCFG);
        return TMCTL_INVALID_OPTION;
    }

    memset(&queueCfg, 0x0, sizeof(tmctl_queueCfg_t));
    tmctlRet = tmctl_getQueueCfgTrace(cmdData.devType, &cmdData.ifInfo,
      cmdData.qid, &queueCfg);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getQueueCfg() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_queueCfgPrint(&queueCfg);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setQCfgHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setQCfgHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueCfg_t queueCfg;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQCFG);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQCFG);
        return TMCTL_INVALID_OPTION;
    }

    memset(&queueCfg, 0x0, sizeof(tmctl_queueCfg_t));
    tmctlRet = tmctl_getQueueCfgTrace(cmdData.devType, &cmdData.ifInfo,
      cmdData.qid, &queueCfg);
    if ((tmctlRet != TMCTL_SUCCESS) && (tmctlRet != TMCTL_NOT_FOUND))
    {
        fprintf(stderr, "%s: tmctl_getQueueCfg() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    if (tmctlRet == TMCTL_NOT_FOUND)
    {
        tmctl_queueCfgInit(cmdData.devType, queueCfg);
    }

    tmctl_cmdDataToQueueCfg(&cmdData, &queueCfg);
    tmctlRet = tmctl_setQueueCfgTrace(cmdData.devType, &cmdData.ifInfo,
      &queueCfg);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setQueueCfg() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_delQCfgHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_delQCfgHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_DELQCFG);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_DELQCFG);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet = tmctl_delQueueCfgTrace(cmdData.devType, &cmdData.ifInfo,
      cmdData.qid);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_delQueueCfg() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_getPortShaperHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getPortShaperHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_shaper_t portShaper;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPORTSHAPER);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPORTSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    memset(&portShaper, 0x0, sizeof(tmctl_shaper_t));
    tmctlRet = tmctl_getPortShaperTrace(cmdData.devType, &cmdData.ifInfo,
      &portShaper);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getPortShaper() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_shaperCfgPrint(&portShaper);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setPortShaperHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setPortShaperHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_shaper_t portShaper;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETPORTSHAPER);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETPORTSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    tmctl_shaperCfgInit(portShaper);
    tmctlRet = tmctl_getPortShaperTrace(cmdData.devType, &cmdData.ifInfo,
      &portShaper);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getPortShaper() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_cmdDataToShaperCfg(&cmdData, &portShaper);
    tmctlRet = tmctl_setPortShaperTrace(cmdData.devType, &cmdData.ifInfo,
      &portShaper);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setPortShaper() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}


/*****************************************************************************
*  FUNCTION:  tmctl_getOverAllShaperHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getOverAllShaperHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_shaper_t overAllShaper;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;
        
    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETORLSHAPER);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_PARMS_BIT_DEVTYPE) != TMCTL_PARMS_BIT_DEVTYPE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETORLSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    memset(&overAllShaper, 0x0, sizeof(tmctl_shaper_t));
    tmctlRet = tmctl_getOverAllShaperTrace(cmdData.devType, &cmdData.ifInfo, &overAllShaper);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getOverAllShaper() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_overAllShaperInfoPrint(cmdData.devType, cmdData.ifInfo, &overAllShaper);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setOverAllShaperHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setOverAllShaperHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_shaper_t overAllShaper;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETORLSHAPER);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_PARMS_BIT_DEVTYPE) != TMCTL_PARMS_BIT_DEVTYPE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETORLSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    if ((cmdData.bitMask & TMCTL_PARMS_BIT_SHAPINGRATE) != TMCTL_PARMS_BIT_SHAPINGRATE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETORLSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    tmctl_shaperCfgInit(overAllShaper);

    tmctl_cmdDataToShaperCfg(&cmdData, &overAllShaper);
    tmctlRet = tmctl_setOverAllShaperTrace(cmdData.devType, &overAllShaper);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setOverAllShaper() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_linkOverAllShaperHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_linkOverAllShaperHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;
        
    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETORLSHAPER);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_LINKORLSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet = tmctl_linkOverAllShaperTrace(cmdData.devType, &cmdData.ifInfo);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_linkOverAllShaperHandler() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_unLinkOverAllShaperHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_unlinkOverAllShaperHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETORLSHAPER);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_UNLINKORLSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet = tmctl_unlinkOverAllShaperTrace(cmdData.devType, &cmdData.ifInfo);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_unlinkOverAllShaperHandler() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_getQProfHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getQProfHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueProfile_t qProf;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQPROF);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_PARMS_BIT_QPROFLO) == 0)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQPROF);
        return TMCTL_INVALID_OPTION;
    }

    memset(&qProf, 0x0, sizeof(tmctl_queueProfile_t));
    tmctlRet = tmctl_getQueueProfileTrace(cmdData.queueProfileIdLo, &qProf);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getQueueProfile() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_queueProfilePrint(&qProf);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setQProfHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setQProfHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueProfile_t qProf;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQPROF);
        return helperRet;
    }

    if (((cmdData.bitMask & TMCTL_PARMS_BIT_QPROFLO) != 0) &&
        (cmdData.queueProfileIdLo == 0))
    {
        fprintf(stderr, "%s: invalid input. cannot configure default queue "
                        "profile\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQPROF);
        return TMCTL_INVALID_OPTION;
    }

    if ((cmdData.bitMask & TMCTL_PARMS_BIT_QPROFLO) == 0)
    {
        tmctlPrint("will attempt to allocate a new profile\n");
    }

    tmctl_queueProfileCfgInit(qProf);
    tmctlRet = tmctl_getQueueProfileTrace(cmdData.queueProfileIdLo, &qProf);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getQueueProfile() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_cmdDataToQProfCfg(&cmdData, &qProf);
    tmctlRet = tmctl_setQueueProfileTrace(cmdData.queueProfileIdLo, &qProf);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setQueueProfile() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_getQDropAlgHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getQDropAlgHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueDropAlg_t qDropAlg;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQDROPALG);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQDROPALG);
        return TMCTL_INVALID_OPTION;
    }

    memset(&qDropAlg, 0x0, sizeof(tmctl_queueDropAlg_t));
    if (cmdData.devType == TMCTL_DEV_ETH)
    {
        tmctlRet = tmctl_getQueueDropAlgTrace(cmdData.devType, &cmdData.ifInfo,
          cmdData.qid, &qDropAlg);
    }
    else if (cmdData.devType == TMCTL_DEV_XTM)
    {
        tmctlRet = tmctl_getXtmChannelDropAlgTrace(cmdData.devType, cmdData.qid,
          &qDropAlg);
    }
    else
    {
        tmctlRet = TMCTL_CMD_EXEC_ERROR;
    }

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getQueueDropAlg() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_queueDropAlgPrint(&qDropAlg);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setQDropAlgHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setQDropAlgHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueDropAlg_t qDropAlg;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQDROPALG);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQDROPALG);
        return TMCTL_INVALID_OPTION;
    }

    tmctl_queueDropAlgCfgInit(qDropAlg);
    if (cmdData.devType == TMCTL_DEV_ETH)
    {
        tmctlRet = tmctl_getQueueDropAlgTrace(cmdData.devType, &cmdData.ifInfo,
          cmdData.qid, &qDropAlg);
        if (tmctlRet != TMCTL_SUCCESS)
        {
            fprintf(stderr, "%s: tmctl_getQueueDropAlg() failed, ret = %d.\n",
              pgmName, tmctlRet);
            return TMCTL_CMD_EXEC_ERROR;
        }

        tmctl_cmdDataToDropAlgCfg(&cmdData, &qDropAlg);
        tmctlRet = tmctl_setQueueDropAlgTrace(cmdData.devType, &cmdData.ifInfo,
          cmdData.qid, &qDropAlg);
    }
    else if (cmdData.devType == TMCTL_DEV_XTM)
    {
        tmctlRet = tmctl_getXtmChannelDropAlgTrace(cmdData.devType, cmdData.qid,
          &qDropAlg);
        if (tmctlRet != TMCTL_SUCCESS)
        {
            fprintf(stderr, "%s: tmctl_getXtmChannelDropAlg() failed, ret = %d.\n",
              pgmName, tmctlRet);
            return TMCTL_CMD_EXEC_ERROR;
        }

        tmctl_cmdDataToDropAlgCfg(&cmdData, &qDropAlg);
        tmctlRet = tmctl_setXtmChannelDropAlgTrace(cmdData.devType, cmdData.qid,
          &qDropAlg);
    }
    else
    {
        tmctlRet = TMCTL_CMD_EXEC_ERROR;
    }

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setQueueDropAlg() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setQueueSizeHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setQueueSizeHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQSIZE);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQSIZE);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet = tmctl_setQueueSizeTrace(cmdData.devType, &cmdData.ifInfo,
          cmdData.qid, cmdData.qsize);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setQueueSize() failed, ret = %d.\n",
            pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setQueueShaperHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setQueueShaperHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;
    tmctl_shaper_t queueShaper;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQSHAPER);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQSHAPER);
        return TMCTL_INVALID_OPTION;
    }

    tmctl_cmdDataToShaperCfg(&cmdData, &queueShaper);
    tmctlRet = tmctl_setQueueShaperTrace(cmdData.devType, &cmdData.ifInfo,
          cmdData.qid, &queueShaper);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setQueueShaper() failed, ret = %d.\n",
            pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}


/*****************************************************************************
*  FUNCTION:  tmctl_getPortTmParmsHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getPortTmParmsHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_portTmParms_t portTmParms;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPORTTMPARMS);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPORTTMPARMS);
        return TMCTL_INVALID_OPTION;
    }

    memset(&portTmParms, 0x0, sizeof(tmctl_portTmParms_t));
    tmctlRet = tmctl_getPortTmParmsTrace(cmdData.devType, &cmdData.ifInfo,
      &portTmParms);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getPortTmParms() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_portTmParmsPrint(&portTmParms);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setQDropAlgExtHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setQDropAlgExtHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueDropAlg_t qDropAlg;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    memset(&cmdData, 0x0, sizeof(tmctl_cmd_data_t));
    memset(&qDropAlg, 0x0, sizeof(tmctl_queueDropAlg_t));
    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQDROPALGEXT);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETQDROPALGEXT);
        return TMCTL_INVALID_OPTION;
    }

    tmctl_cmdDataToDropAlgExtCfg(&cmdData, &qDropAlg);
    tmctlRet = tmctl_setQueueDropAlgExtTrace(cmdData.devType, &cmdData.ifInfo,
      cmdData.qid, &qDropAlg);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setQueueDropAlgExt() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_getQStatsHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getQStatsHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_queueStats_t qStats;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQSTATS);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_QUEUE) != TMCTL_CFGKEY_QUEUE)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETQSTATS);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet = tmctl_getQueueStatsTrace(cmdData.devType, &cmdData.ifInfo,
      cmdData.qid, &qStats);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getQueueStat() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_queueStatsPrint(&qStats);

    return TMCTL_CMD_EXEC_OK;
}


/*****************************************************************************
*  FUNCTION:  tmctl_getDscpToPbitHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getDscpToPbitHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_dscpToPbitCfg_t dscpToPbitCfg;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETDSCPTOPBIT);
        return helperRet;
    }

    memset(&dscpToPbitCfg, 0x0, sizeof(tmctl_dscpToPbitCfg_t));
    /* back compatibility, default 0 is qos mode */
    if (cmdData.bitMask & TMCTL_PARMS_BIT_ENABLE)
        dscpToPbitCfg.remark = cmdData.enable;

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) == TMCTL_CFGKEY_PORT)
    {
        dscpToPbitCfg.devType= cmdData.devType;
        dscpToPbitCfg.if_p= &cmdData.ifInfo;
    }
    else
        dscpToPbitCfg.devType = TMCTL_DEV_NONE;
        
    tmctlRet = tmctl_getDscpToPbitTrace(&dscpToPbitCfg);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getDscpToPbit() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_dscpToPbitPrint(&dscpToPbitCfg);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setDscpToPbitHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setDscpToPbitHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet, i;
    tmctl_dscpToPbitCfg_t dscpToPbitSetT;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETDSCPTOPBIT);
        return helperRet;
    }

    memset(&dscpToPbitSetT, 0x0, sizeof(tmctl_dscpToPbitCfg_t));
    /* mask map by default */
    for (i=0; i<TOTAL_DSCP_NUM; i++)
        dscpToPbitSetT.dscp[i] = U32_MAX;

    /* back compatibility, default 0 is qos mode */
    if (cmdData.bitMask & TMCTL_PARMS_BIT_ENABLE)
        dscpToPbitSetT.remark = cmdData.enable;

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) == TMCTL_CFGKEY_PORT)
    {
        dscpToPbitSetT.devType= cmdData.devType;
        dscpToPbitSetT.if_p= &cmdData.ifInfo;

        if ((cmdData.bitMask & TMCTL_CFGKEY_DSCP) == TMCTL_CFGKEY_DSCP)
        {
            tmctlRet = tmctl_getDscpToPbitTrace(&dscpToPbitSetT);
            if (tmctlRet == TMCTL_ERROR)
            {
                fprintf(stderr,
                    "%s: get dscp to pbit during set dscp to pbit failed, ret = %d.\n",
                    pgmName, tmctlRet);
                return TMCTL_CMD_EXEC_ERROR;
            }
         }
    }
    else
        /* back compatibility, default no dev, table level operation */
        dscpToPbitSetT.devType= TMCTL_DEV_NONE;

    if ((cmdData.bitMask & TMCTL_CFGKEY_DSCP) == TMCTL_CFGKEY_DSCP)
        dscpToPbitSetT.dscp[cmdData.dscp] = cmdData.pbit;
    
    tmctlRet = tmctl_setDscpToPbitTrace(&dscpToPbitSetT);

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setDscpToPbit() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_getPbitToQHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getPbitToQHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_pbitToQCfg_t pbitToQCfg;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPBITTOQ);
        return helperRet;
    }


    memset(&pbitToQCfg, 0x0, sizeof(tmctl_pbitToQCfg_t));
    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) == 0)
    {
        cmdData.devType = TMCTL_DEV_NONE;
    }
    else  if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) != TMCTL_CFGKEY_PORT)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPBITTOQ);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet =
        tmctl_getPbitToQTrace(cmdData.devType, &cmdData.ifInfo, &pbitToQCfg);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getPbitToQ() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_PbitToQPrint(&pbitToQCfg);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setPbitToQHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setPbitToQHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_pbitToQCfg_t PbitToQSetT;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet, i;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETPBITTOQ);
        return helperRet;
    }

    memset(&PbitToQSetT, 0x0, sizeof(tmctl_pbitToQCfg_t));
    /* mask map by default */
    for (i=0; i<TOTAL_PBIT_NUM; i++)
        PbitToQSetT.pbit[i] = U32_MAX;

    if ((cmdData.bitMask & TMCTL_CFGKEY_PORT) == TMCTL_CFGKEY_PORT)
    {
        if ((cmdData.bitMask & TMCTL_CFGKEY_PBIT) == TMCTL_CFGKEY_PBIT)
        {
            tmctlRet = tmctl_getPbitToQTrace(cmdData.devType, &cmdData.ifInfo, &PbitToQSetT);
            if (tmctlRet == TMCTL_ERROR)
            {
                fprintf(stderr,
                    "%s: get pbit to q during set pbit to q failed, ret = %d.\n",
                    pgmName, tmctlRet);
                return TMCTL_CMD_EXEC_ERROR;
            }
         }
    }
    else
        cmdData.devType = TMCTL_DEV_NONE;

    if ((cmdData.bitMask & TMCTL_CFGKEY_PBIT) == TMCTL_CFGKEY_PBIT)
        PbitToQSetT.pbit[cmdData.pbit] = cmdData.qid;

    tmctlRet =
        tmctl_setPbitToQTrace(cmdData.devType, &cmdData.ifInfo, &PbitToQSetT);

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setPbitToQ() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_getForceDscpHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getForceDscpHandler(OPTION_INFO *pOptions, int optNum)
{
    BOOL enable = FALSE;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETFORCEDSCP);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_PARMS_BIT_DIR) != TMCTL_PARMS_BIT_DIR)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETFORCEDSCP);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet =
        tmctl_getForceDscpToPbitTrace(cmdData.dir, &enable);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getForceDscpToPbit() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_ForceDscpPrint(&enable);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setForceDscpHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setForceDscpHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETFORCEDSCP);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_FORCEDSCP) != TMCTL_CFGKEY_FORCEDSCP)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETFORCEDSCP);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet = tmctl_setForceDscpToPbitTrace(cmdData.dir, &cmdData.enable);

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_setForceDscpToPbit() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}


/*****************************************************************************
*  FUNCTION:  tmctl_getPktBasedQosHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_getPktBasedQosHandler(OPTION_INFO *pOptions, int optNum)
{
    BOOL enable = FALSE;
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPKTBASEDQOS);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PKTQOS) != TMCTL_CFGKEY_PKTQOS)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_GETPKTBASEDQOS);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet =
        tmctl_getPktBasedQosTrace(cmdData.dir, cmdData.qostype, &enable);
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getPktBasedQos() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    tmctl_PktBasedQosPrint(&enable);

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_setPktBasedQosHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_setPktBasedQosHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_cmd_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_cmdDataHelper(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETFORCEDSCP);
        return helperRet;
    }

    if ((cmdData.bitMask & TMCTL_CFGKEY_PKTQOS) != TMCTL_CFGKEY_PKTQOS)
    {
        fprintf(stderr, "%s: invalid options.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_SETPKTBASEDQOS);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet =
        tmctl_setPktBasedQosTrace(cmdData.dir, cmdData.qostype, &cmdData.enable);

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_getPktBasedQos() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_createPolicerHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_createPolicerHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_policer_data_t cmdData;
    tmctl_policer_t policer;
    tmctl_ret_e tmctlRet;
    int helperRet;
    
    helperRet = tmCtl_policerDataHandler(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_CREATEPOLICER);
        return helperRet;
    }
    if ((cmdData.dir == UNASSIGNED) || (cmdData.cir == UNASSIGNED))
    {
        fprintf(stderr, "%s: Lack of necessary parameteres input\n", __FUNCTION__);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_CREATEPOLICER);
        return TMCTL_INVALID_OPTION;
    }
    if (((cmdData.policertype == 2) || (cmdData.policertype == 3)) &&
    (cmdData.pir == UNASSIGNED))
    {
        fprintf(stderr, "%s: Lack of necessary parameteres input\n", __FUNCTION__);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_CREATEPOLICER);
        return TMCTL_INVALID_OPTION;
    }
    
    memset(&policer, 0, sizeof(tmctl_policer_t));
    
    // Parameters dir and cir are necessary. Others are initilized as 0.
    policer.dir = cmdData.dir;
    policer.cir = cmdData.cir;

    //By default, the type is single token bucket
    policer.type = TMCTL_POLICER_SINGLE_TOKEN_BUCKET;
    if (cmdData.policertype == 0)
        policer.type = TMCTL_POLICER_SINGLE_TOKEN_BUCKET;
    else if (cmdData.policertype == 1)
        policer.type = TMCTL_POLICER_SINGLE_RATE_TCM;
    else if (cmdData.policertype == 2)
        policer.type = TMCTL_POLICER_TWO_RATE_TCM;
    else if (cmdData.policertype == 3)
        policer.type = TMCTL_POLICER_TWO_RATE_WITH_OVERFLOW;

    //If policerId not specified, use TMCTL_INVALID_KEY to indicate the value should be allocated by system
    if (cmdData.policerid != UNASSIGNED)
        policer.policerId = cmdData.policerid;
    else
        policer.policerId = TMCTL_INVALID_KEY;

    //If cbs not specified, use TMCTL_INVALID_KEY to indicate the value should be allocated by system
    if (cmdData.cbs != UNASSIGNED)
        policer.cbs = cmdData.cbs;
    else
        policer.cbs = TMCTL_INVALID_KEY;

    //If ebs not specified, use TMCTL_INVALID_KEY to indicate the value should be allocated by system
    if (cmdData.ebs != UNASSIGNED)
        policer.ebs = cmdData.ebs;
    else if (policer.type == TMCTL_POLICER_SINGLE_RATE_TCM)
        policer.ebs = TMCTL_INVALID_KEY;
	
    if (cmdData.pir != UNASSIGNED)
        policer.pir = cmdData.pir;

    //If pbs not specified, use TMCTL_INVALID_KEY to indicate the value should be allocated by system
    if (cmdData.pbs != UNASSIGNED)
        policer.pbs = cmdData.pbs;
    else if ((policer.type == TMCTL_POLICER_TWO_RATE_TCM) || 
             (policer.type == TMCTL_POLICER_TWO_RATE_WITH_OVERFLOW))
        policer.pbs = TMCTL_INVALID_KEY;

    tmctlRet =
        tmctl_createPolicerTrace(&policer);

    printf("policer %d created\n", policer.policerId);

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_createPolicer() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_modifyPolicerHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_modifyPolicerHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_policer_data_t cmdData;
    tmctl_policer_t policer;
    tmctl_ret_e tmctlRet;
    int helperRet;
    
    helperRet = tmCtl_policerDataHandler(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_MODIFYPOLICER);
        return helperRet;
    }
    if ((cmdData.dir == UNASSIGNED) || (cmdData.policerid == UNASSIGNED))
    {
        fprintf(stderr, "%s: Lack of necessary parameteres input\n", __FUNCTION__);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_MODIFYPOLICER);
        return TMCTL_INVALID_OPTION;
    }
    
    memset(&policer, 0, sizeof(tmctl_policer_t));

    //If the parameters not specified, use TMCTL_INVALID_KEY to 
    //indicate the value not changed
    policer.dir = cmdData.dir;
    policer.policerId = cmdData.policerid;
    policer.cir = TMCTL_INVALID_KEY;
    policer.cbs = TMCTL_INVALID_KEY;
    policer.ebs = TMCTL_INVALID_KEY;
    policer.pir = TMCTL_INVALID_KEY;
    policer.pbs = TMCTL_INVALID_KEY;
    
    
    if (cmdData.cir != UNASSIGNED)
        policer.cir = cmdData.cir;
    
    if (cmdData.cbs != UNASSIGNED)
        policer.cbs = cmdData.cbs;
    
    if (cmdData.ebs != UNASSIGNED)
        policer.ebs = cmdData.ebs;
    
    if (cmdData.pir != UNASSIGNED)
        policer.pir = cmdData.pir;
    
    if (cmdData.pbs != UNASSIGNED)
        policer.pbs = cmdData.pbs;
    
    tmctlRet =
        tmctl_modifyPolicerTrace(&policer);
    
    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_modifyPolicer() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }
    
    return TMCTL_CMD_EXEC_OK;
}

/*****************************************************************************
*  FUNCTION:  tmctl_deletePolicerHandler
*  PURPOSE:   TMCtl command handler.
*  PARAMETERS:
*      pOptions - pointer to option array.
*      optNum - number of options.
*  RETURNS:
*      0 - success, others - error.
*  NOTES:
*      None.
*****************************************************************************/
static int tmctl_deletePolicerHandler(OPTION_INFO *pOptions, int optNum)
{
    tmctl_policer_data_t cmdData;
    tmctl_ret_e tmctlRet;
    int helperRet;

    helperRet = tmCtl_policerDataHandler(pOptions, optNum, &cmdData);
    if (helperRet != TMCTL_CMD_PARSE_OK)
    {
        fprintf(stderr, "%s: invalid input.\n", pgmName);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_DELETEPOLICER);
        return helperRet;
    }

    if ((cmdData.dir == UNASSIGNED) || (cmdData.policerid == UNASSIGNED))
    {
        fprintf(stderr, "%s: Lack of necessary parameteres input\n", __FUNCTION__);
        printf("Usage:\n%s\n", TMCTL_CMD_HELP_DELETEPOLICER);
        return TMCTL_INVALID_OPTION;
    }

    tmctlRet =
        tmctl_deletePolicerTrace(cmdData.dir, cmdData.policerid);

    if (tmctlRet != TMCTL_SUCCESS)
    {
        fprintf(stderr, "%s: tmctl_deletePolicer() failed, ret = %d.\n",
          pgmName, tmctlRet);
        return TMCTL_CMD_EXEC_ERROR;
    }

    return TMCTL_CMD_EXEC_OK;
}



