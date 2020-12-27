/***********************************************************************
 *
 *  Copyright (c) 2008  Broadcom Corporation
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

#include "cms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/utsname.h>
#include <bcmnvram.h>
#include <wlapi.h>
#include <wlmngr.h>
#include "cms_util.h"
#include "cms_core.h"
#include "rut_pmap.h"

static int debug = 0;

static int run_for_each_port(L2BridgingEntryObject *pBr, L2BridgingFilterObject *pFtr, InstanceIdStack *pFtrStk, char *lan_ifnames)
{
    L2BridgingIntfObject *pIf = NULL;
    InstanceIdStack IfStk = EMPTY_INSTANCE_ID_STACK;

    char lan_ifname[32];
    char nv_name[32];
    char nv_valu[32];
    UINT32 key;
 
    if ((cmsObj_getNext(MDMOID_L2_BRIDGING_FILTER, pFtrStk, (void **)&pFtr)) != CMSRET_SUCCESS)
        return -1;

    if (pFtr == NULL)
        return -1;

    if ((pFtr->filterBridgeReference == (SINT32)pBr->bridgeKey) && (cmsUtl_strcmp(pFtr->filterInterface, MDMVS_LANINTERFACES)))
    {      
        cmsUtl_strtoul(pFtr->filterInterface, NULL, 0, &key);

        if(rutPMap_getAvailableInterfaceByKey(key, &IfStk, &pIf) == CMSRET_SUCCESS)
        {
            if (pIf == NULL)
            {
                cmsObj_free((void **)&pFtr);
                return -1;
            }

            if (!cmsUtl_strcmp(pIf->interfaceType, MDMVS_LANINTERFACE))
            {
                rutPMap_availableInterfaceReferenceToIfName(pIf->interfaceReference, lan_ifname);

                /* Example: nvram_set("wl0_ifname", "wl0"); */
                
                snprintf(nv_name, sizeof(nv_name), "%s_ifname", lan_ifname);
                nvram_set(nv_name, lan_ifname);

                if (debug)
                    printf(">>>>>%s=%s<<<<<\n", nv_name, lan_ifname);

                /* Example: nvram_set("wl0_hwaddr", "11:22:33:AA:BB:CC"); */

	            snprintf(nv_name, sizeof(nv_name), "%s_hwaddr", lan_ifname);
                wlmngr_getHwAddr(0, lan_ifname, nv_valu);
                nvram_set(nv_name, nv_valu);

                if (debug)
                    printf(">>>>>%s=%s<<<<<\n", nv_name, nv_valu);

                if (lan_ifnames[0] != '\0')
                    strcat(lan_ifnames, " ");

                strcat(lan_ifnames, lan_ifname);

            }
//          else if (!cmsUtl_strcmp(tmpObj->interfaceType, MDMVS_WANINTERFACE))
//          {
//              rutPMap_availableInterfaceReferenceToIfName(tmpObj->interfaceReference, wanIfName);
//              cmsLog_debug("wanIfName=%s",  wanIfName);
//          }
          
            cmsObj_free((void **) &pIf);
        }
    }

    cmsObj_free((void **)&pFtr);
    return 0;
}

static int run_for_each_br(L2BridgingEntryObject *pBr, InstanceIdStack *pBrStk)
{
    L2BridgingFilterObject *pFtr = NULL;
    InstanceIdStack FtrStk = EMPTY_INSTANCE_ID_STACK;

    char br[32];
    char nv_name[32];
    char nv_valu[32*(1+WL_MAX_NUM_SSID)];

    if ((cmsObj_getNext(MDMOID_L2_BRIDGING_ENTRY, pBrStk, (void **)&pBr)) != CMSRET_SUCCESS)
        return -1;

    if (pBr == NULL)
        return -1;

    /* Example: nvram_set("lan_ifnames", "eth0 eth1 wl0 usb0"); */

    if (pBr->bridgeKey == 0)
        snprintf(nv_name, sizeof(nv_name), "lan_ifnames");
    else
        snprintf(nv_name, sizeof(nv_name), "lan%d_ifnames", pBr->bridgeKey);

    memset(nv_valu, 0, sizeof(nv_valu));

    while (run_for_each_port(pBr, pFtr, &FtrStk, nv_valu) == 0)
      ;

    nvram_set(nv_name, nv_valu);

    if (debug)
        printf(">>>>>%s=%s<<<<<\n", nv_name, nv_valu);

    /* Example: nvram_set("lan_ifname", "br0"); */

    if (pBr->bridgeKey == 0)
        snprintf(nv_name, sizeof(nv_name), "lan_ifname");
    else
        snprintf(nv_name, sizeof(nv_name), "lan%d_ifname", pBr->bridgeKey);

    snprintf(br, sizeof(br), "br%d", pBr->bridgeKey);
    nvram_set(nv_name, br);

    if (debug)
        printf(">>>>>%s=%s<<<<<\n", nv_name, br);

    /* Example: nvram_set("lan_hwaddr", "11:22:33:AA:BB:CC"); */

    if (pBr->bridgeKey == 0)
        snprintf(nv_name, sizeof(nv_name), "lan_hwaddr");
    else
        snprintf(nv_name, sizeof(nv_name), "lan%d_hwaddr", pBr->bridgeKey);

    wlmngr_getHwAddr(0, br, nv_valu);
    nvram_set(nv_name, nv_valu);

    if (debug)
        printf(">>>>>%s=%s<<<<<\n\n\n\n", nv_name, nv_valu);

    return 0;
}

void wlmngr_enum_ifnames(void)
{
    L2BridgingEntryObject *pBr = NULL;
    InstanceIdStack BrStk = EMPTY_INSTANCE_ID_STACK;

    if (cmsLck_acquireLockWithTimeout(30000) != CMSRET_SUCCESS)
    {
        printf("Get lock failed!\n");
        return;
    }

    while (run_for_each_br(pBr, &BrStk) == 0)
        ;

	cmsLck_releaseLock();
    return;
}

/* End of file */
