/*
* <:copyright-BRCM:2011:proprietary:standard
* 
*    Copyright (c) 2011 Broadcom 
*    All Rights Reserved
* 
*  This program is the proprietary software of Broadcom and/or its
*  licensors, and may only be used, duplicated, modified or distributed pursuant
*  to the terms and conditions of a separate, written license agreement executed
*  between you and Broadcom (an "Authorized License").  Except as set forth in
*  an Authorized License, Broadcom grants no license (express or implied), right
*  to use, or waiver of any kind with respect to the Software, and Broadcom
*  expressly reserves all rights in and to the Software and all intellectual
*  property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE
*  NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY
*  BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
* 
*  Except as expressly set forth in the Authorized License,
* 
*  1. This program, including its structure, sequence and organization,
*     constitutes the valuable trade secrets of Broadcom, and you shall use
*     all reasonable efforts to protect the confidentiality thereof, and to
*     use this information only in connection with your use of Broadcom
*     integrated circuit products.
* 
*  2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*     AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*     WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
*     RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND
*     ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
*     FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR
*     COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE
*     TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR
*     PERFORMANCE OF THE SOFTWARE.
* 
*  3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
*     ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
*     INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY
*     WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
*     IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES;
*     OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE
*     SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS
*     SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY
*     LIMITED REMEDY.
:>
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>

#include "cms.h"
#include "cms_log.h"
#include "cms_util.h"
#include "cms_core.h"
#include "cms_msg.h"
#include "cms_dal.h"
#include "cms_cli.h"

#include <bcmnvram.h>
#include "wlapi.h"
#include "wlmngr.h"
#include "wldsltr.h"
#include "wlsyscall.h"
#include "wlmdm.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <wlcsm_linux.h>


int wl_cnt =0;

#ifdef BUILD_EAPD
 void wlevt_main( void );
#endif

extern void brcm_get_lock(char *name, int timeout);
extern void brcm_release_lock(char* name);
 
// #define WLDAEMON_DBG

static void wlan_restart(unsigned char dataSync, int idx)
{

    /* 
    wl_cnt: a global variable in wldaemon.c that indicates the number of
     adaptors in the system.  This is obtained by calling wlgetintfNo()
     below.  The function wlgetintfNo() uses IOCTL's to find out this info.
    */
    
    if ((idx < 0) || (idx >= wl_cnt))
        idx = 0;

    if (idx < wl_cnt) {
	
	brcm_get_lock("wps",200);
        wlmngr_initCfg(dataSync, idx);
        wlmngr_init(idx);
	brcm_release_lock("wps");
    	wlmngr_update_assoc_list(idx);
    }
}

void wlmngr_nvram_commit_handler(char *name, char *value, char *oldvalue) {
	int idx=0;
	for (idx=0; idx<wl_cnt; idx++)
	      wlmngr_initCfg(WL_SYNC_TO_MDM_NVRAM, idx);
}

int main(int argc, char **argv)
{
#ifdef BRCM_CMS_BUILD
    void *msgHandle=NULL;
    SINT32 shmId=UNINITIALIZED_SHM_ID;
    SINT32 mdm_attached=1;  // this could be useful in the future
    CmsRet ret;
    SINT32 logLevelNum;
    CmsLogLevel logLevel=DEFAULT_LOG_LEVEL;
    char *argString="v:m:";
#else
    /* in the future, non-CMS build may want to handle some options.
     * For now, just put in some arg which is not handled anyways.
     */
    char *argString="v:";
#endif /* BRCM_CMS_BUILD */
    SINT32 c;
    char cmd[128]={0};
    int idx = -1,i=0,j=0; 
    int not_failure = 1;
    wlcsm_init();
#ifdef WLCSM_DEBUG
    bcmSystem("wlcsmdbg&");
#endif
    WLCSM_SET_TRACE("wlmngr");
    unsigned int msglen=0;



    printf("WLmngr Daemon is running\n");

#ifdef IDLE_PWRSAVE
    /* Only enable L1 mode, because L0s creates EVM issues. The power savings are the same */
    snprintf(cmd, sizeof(cmd), "echo \"l1_powersave\" > /sys/module/pcie_aspm/parameters/policy");
    bcmSystem(cmd);
#endif

    /*
     * When SIGINT is ignored, control-c on console will not cause wlmngr
     * to exit.  Normally, this is a good thing, but during debugging, could
     * be undesirable.  Add command line option to control it?
     */
#ifdef CPU_MIPS
    /* on Mips, since we are use linux threads instead of NPTL, it has
     * a bug to shutdown mutliplethread in blocking*/
    signal(SIGTERM,SIG_IGN);
#endif
    signal(SIGINT, SIG_IGN);

    while ((c = getopt(argc, argv, argString)) != -1)
    {
        switch(c)
        {
#ifdef BRCM_CMS_BUILD
            case 'm':
                shmId = atoi(optarg);
//                printf("WLMNGR(pid=%d): optarg=%s shmId=%d \n", getpid(), optarg, shmId );
                break;

            case 'v':
               logLevelNum = atoi(optarg);
               if (logLevelNum == 0)
               {
                  logLevel = LOG_LEVEL_ERR;
               }
               else if (logLevelNum == 1)
               {
                  logLevel = LOG_LEVEL_NOTICE;
               }
               else
               {
                  logLevel = LOG_LEVEL_DEBUG;
               }
               break;
#endif /* BRCM_CMS_BUILD */

            default:
                printf("WLMNGR: unsupported option %d, ignored\n", c);
                break;
        }
    }



    /*Fetch the Adapter number */
    wl_cnt = wlgetintfNo();

    is_smp_system = wlmngr_detectSMP();

#ifdef WL_WLMNGR_DBG
    printf("%s@%d wl_cnt=%d\n", __FUNCTION__, __LINE__, wl_cnt);
#endif

    if ( wldsltr_alloc( wl_cnt ) ) 
        not_failure = 0;

    if (wlmngr_alloc( wl_cnt) )
        not_failure = 0;

#ifdef BUILD_EAPD
     /* Start Event handling Daemon */
    bcmSystem("/bin/wlevt");
#endif
    sleep(1); 

#ifdef BRCM_CMS_BUILD
    /*
     * Do CMS initialization after wlevt is spawned so that wlevt does
     * not inherit any file descriptors opened by CMS init code.
     */
    cmsLog_initWithName(EID_WLMNGR, argv[0]);
    cmsLog_setLevel(logLevel);

    if ((ret = cmsMsg_initWithFlags(EID_WLMNGR, 0, &msgHandle)) != CMSRET_SUCCESS)
    {
        cmsLog_error("could not initialize msg, ret=%d", ret);
        cmsLog_cleanup();
        exit(-1);
    }

    /*
     * The wrapper has logic to request the shmId from smd if wlmngr was
     * started on the command line.  It also contains logic to skip MDM
     * initialization if we are in Pure181 mode.
     */
    if ((ret = cmsMdm_init_wrapper(&shmId, msgHandle)) != CMSRET_SUCCESS)
    {
        cmsMsg_cleanup(&msgHandle);
        cmsLog_cleanup();
        exit(-1);
    }

    /*
     * In Pure181 mode, we use new config method, wlmngr does not attach
     * to MDM, so even after cmsMdm_init_wrapper, shmId will remain in the
     * un-initialized value.
     */
    if (shmId == UNINITIALIZED_SHM_ID)
    {
       mdm_attached = 0;
    }

    {
        int wl0_present=0;
        int wl1_present=0;
        int wl2_present=0;
        g_wlmngr_restart_all=1;

#ifdef HSPOT_SUPPORT
		wlcsm_hspot_nvram_default("wl_",1);
		for (i = 0; i < wl_cnt; i++) {
			snprintf(cmd, sizeof(cmd), "wl%d_",i);
			wlcsm_hspot_nvram_default(cmd,1);
			for (j = 1; j <WL_NUM_SSID; j++) {
				snprintf(cmd, sizeof(cmd), "wl%d.%d_",i,j);
				wlcsm_hspot_nvram_default(cmd,1);
			}
		}
#endif
        /*
         * This block of code is for the old config method.  So in
         * Pure181 mode, wl0_present wl1_present and wl2_present will stay 0 even though
         * the adapters may be present.  They will be configured elsewhere (TBD).
         */
        cmsMdm_detect_adapters(&wl0_present, &wl1_present,&wl2_present);
        if (wl0_present)
        {
           wlan_restart(WL_SYNC_FROM_MDM, 0);
        }
        if (wl1_present)
        {
           wlan_restart(WL_SYNC_FROM_MDM, 1);
        }
        if (wl2_present)
        {
           wlan_restart(WL_SYNC_FROM_MDM, 2);
        }
        g_wlmngr_restart_all=0;

        if (mdm_attached)
        {
#ifdef BCMWAPI_WAI
           BcmWapi_ReadAsCertFromMdm();

           if (BcmWapi_ReadCertListFromMdm() == 1)
           {
              bcmSystem("ias -D -F /etc/AS.conf");
           }
#endif
        }
    }


/*  enable nvram commit event monitoring, when nvram commit received, will restart  wlan */
    wlcsm_register_event_hook(WLCSM_EVT_NVRAM_COMMITTED,wlmngr_nvram_commit_handler);

    /* start of main loop */
    while ( not_failure )
    {
        CmsMsgHeader *msg=NULL;
        CmsEntityId msgSrc;

        ret = cmsMsg_receive(msgHandle, &msg);
        sleep(1);

        if (ret == CMSRET_SUCCESS)
        {
            cmsLog_debug("received msg: src=%d dst=%d type=0x%x len=%d",
                         msg->dst, msg->src, msg->type, msg->dataLength);

            if (CMS_MSG_SET_LOG_LEVEL == msg->type)
            {
                cmsLog_setLevel(msg->wordData);
                cmsMsg_sendReply(msgHandle, msg, CMSRET_SUCCESS);
                CMSMEM_FREE_BUF_AND_NULL_PTR(msg);
                continue;
            }

            if (CMS_MSG_SET_LOG_DESTINATION == msg->type)
            {
                cmsLog_setDestination(msg->wordData);
                cmsMsg_sendReply(msgHandle, msg, CMSRET_SUCCESS);
                CMSMEM_FREE_BUF_AND_NULL_PTR(msg);
                continue;
            }

            if  (CMS_MSG_SYSTEM_BOOT == msg->type)
            {
                /*
                 * This message is sent to wlmngr when CMS MDM has been
                 * fully initialized and wlmngr was launched because of the
                 * EIF_LAUNCH_ON_BOOT flag.  Currently, this message does not
                 * trigger any actions.
                 */
                CMSMEM_FREE_BUF_AND_NULL_PTR(msg);
                continue;
            }

            if  (CMS_MSG_INTERNAL_NOOP == msg->type)
            {
                /* needed for CMS msg internal stuff, just igore it */
                CMSMEM_FREE_BUF_AND_NULL_PTR(msg);
                continue;
            }

            /*
             * Eventually, we want to separate all the core wlmngr code
             * from the CMS code.  However, for now, wlmngr heavily depends
             * on CMS messaging services to communicate with other daemons,
             * so all this stuff is inside ifdef BRCM_CMS_BUILD.  If
             * BRCM_CMS_BUILD is not defined, all this code will be missing
             * and wlmngr will not work.
             */
#ifdef WLDAEMON_DBG
            printf(" msg->dst[%d]\n", msg->dst );
            printf(" msg->src[%d]\n", msg->src );
            printf(" msg->type[%x]\n", msg->type);
            printf(" msg->dataLength[%d]\n", msg->dataLength );
#endif
            memset(cmd, 0, sizeof(cmd));
	    msglen=msg->dataLength;
            if ( msg->dataLength >0 ) 
            {
                char *cmdBody = (char *)(msg+1);
#ifdef WLDAEMON_DBG
                printf("(msg+1)=%s\n", cmdBody);
#endif
                /* msg->dataLength includes null byte, so check against full size of cmd */
                if (msg->dataLength > sizeof(cmd))
                {
                    cmsLog_error("msg truncated: msg->dataLength=%d sizeof(cmd)=%d",
                                 msg->dataLength, sizeof(cmd));
                }

                /* strlen does not include null byte, so check against sizeof(cmd)-1 */
                if (strlen(cmdBody) > sizeof(cmd)-1)
                {
                    cmsLog_error("msg truncated: strlen=%d sizeof(cmd)=%d",
                                 strlen(cmdBody), sizeof(cmd));
                }

                strncpy(cmd, cmdBody, sizeof(cmd)-1);
            }

            msgSrc = msg->src; 
            CMSMEM_FREE_BUF_AND_NULL_PTR(msg);

            /* Assoc request from WLEVENT */
            if (msgSrc == EID_WLEVENT  && !strncmp(cmd, "Assoc", 5) ) {
                idx = cmd[6]- '0'-1;
                if (idx < wl_cnt && idx >=0 ) {
                    wlmngr_update_assoc_list(idx);
                    wlmngr_togglePowerSave(); 
                }
            }
            /* restart request from WPS */
            else if (msgSrc == EID_WLWPS  && !strncmp(cmd, "Modify", 6) ) 
            {
#ifdef WLDAEMON_DBG
                printf("WPS Request Restart\n");
#endif
                if ( strlen(cmd) >=8 )
                {
                    idx = cmd[7]- '1';
	            if ( (idx >= 0) && (idx < wl_cnt) )
                    {

		          /* From WPS, byte 8 indicates if need to restart all wlan interfaces */
		          if(cmd[8]) {

			      for (i=0; i<wl_cnt; i++)
				      wlan_restart(WL_SYNC_TO_MDM_NVRAM, i);
		          } else 
		          {
			             wlan_restart(WL_SYNC_TO_MDM_NVRAM, idx);
		          }
		      }
	      }
            }
            /*Request from ssk initialization */
            else if ( !strncmp(cmd, "Create", 6) )
            {
               cmsLog_error("should not recv Create msg anymore!");
            }

            else if ( !strncmp(cmd, "Modify", 6) && msgSrc==EID_SSK)
            {

		/* when Lan bridge IP address change, wlan to restart*/
                if ( strlen(cmd) >=8 )
                {
                    idx = cmd[7]- '1';
	            if ( (idx >= 0) && (idx < wl_cnt) )
                    {
#ifdef WLDAEMON_DBG
                        printf("SSK Request Restart\n");
#endif
		        wlan_restart(WL_SYNC_TO_MDM_NVRAM, idx);
                    }
                }
            }
            /*sync from httpd or tr69c */
            else if ( !strncmp(cmd, "Modify", 6) && msgSrc == EID_HTTPD)
            {
                if ( strlen(cmd) >=8 )
                {
                    idx = cmd[7]- '1';
	            if ( (idx >= 0) && (idx < wl_cnt) )
                    {
#ifdef WLDAEMON_DBG
                        printf("Httpd Request Restart\n");
#endif
                        wlan_restart(WL_SYNC_FROM_MDM_HTTPD, idx);
                    }
                }
            }
            else if ( !strncmp(cmd, "PwrMngt", 7) && msgSrc == EID_HTTPD)
            {
#ifdef WLDAEMON_DBG
                printf("PwrMngt Changed Reqeust\n");
#endif
                wlmngr_togglePowerSave();
            }
            else if ( !strncmp(cmd, "Modify", 6) && msgSrc == EID_TR69C)
            {
                sleep(2); //wait for 2 seconds for ACS finishing the setting
                if ( strlen(cmd) >=8 )
                {
                    idx = cmd[7]- '1';
	            if ( (idx >= 0) && (idx < wl_cnt) )
                    {
#ifdef WLDAEMON_DBG
                        printf("TR69 Request Restart\n");
#endif
                        wlan_restart(WL_SYNC_FROM_MDM_TR69C, idx);
                    }
                }
            }
            else if ( !strncmp(cmd, "Modify", 6) && msgSrc == EID_TR64C)
            {
                if ( strlen(cmd) >=8 )
                {
                    idx = cmd[7]- '1';
	            if ( (idx >= 0) && (idx < wl_cnt) )
                    {

                        printf("TR64 Request Restart idx=%d wl_cnt = %d\n", idx, wl_cnt);

                        wlan_restart(WL_SYNC_FROM_MDM_TR69C, idx);
                    }
                }
            }
#ifdef BCMWAPI_WAI
            else if (msgSrc == EID_WLWAPID)
            {
                if (!strncmp(cmd, "Modify", 6))
                {
                    // AP certificate is installed, so we sync
                    // to MDM and restart.

                    if ( strlen(cmd) >=8 )
                    {
                        idx = cmd[7]- '1';
	                if ( (idx >= 0) && (idx < wl_cnt) )
                        {
                            wlan_restart(WL_SYNC_TO_MDM, idx);
                        }
                    }
                }
                else if (!strncmp(cmd, "Record", 6))
                {
                    // AS has issued or revoked a certificate.
                    // We do record-keeping.

                    // In order to get AS to save cert list to file,
                    // we need to stop it, record, and then restart.
                    // This could be improved.

                    bcmSystem("killall -15 ias");
                    BcmWapi_SaveAsCertToMdm();
                    BcmWapi_SaveCertListToMdm(1);
                    wlWriteMdmToFlash();
                    bcmSystem("ias -D -F /etc/AS.conf");
                    BcmWapi_SetAsPending(0);
                }
                else if (!strncmp(cmd, "Start", 5))
                {
                    if (BcmWapi_AsStatus() == 0)
                    {
                        bcmSystem("ias -D -F /etc/AS.conf");
                        BcmWapi_SaveCertListToMdm(1);
                        wlWriteMdmToFlash();
                        BcmWapi_SetAsPending(0);
                    }
                }
                else if (!strncmp(cmd, "Stop", 4))
                {
                    if (BcmWapi_AsStatus() == 1)
                    {   
                        bcmSystem("killall -15 ias");
                        BcmWapi_SaveCertListToMdm(0);
                        wlWriteMdmToFlash();
                        BcmWapi_SetAsPending(0);
                    }
                }
            }
#endif
	    else if (msgSrc == EID_WLNVRAM) {
		    if (!strncmp(cmd, "RESTORE", 7)) {
			    if(msglen>8)
				    wlmngr_restore_config_default((char *)cmd+8);
			    else
				    wlmngr_restore_config_default(NULL);
			  g_wlmngr_restart_all=1;
			    for (idx=0; idx<wl_cnt; idx++)
				    wlan_restart(WL_SYNC_TO_MDM_NVRAM, idx);
			  g_wlmngr_restart_all=0;
		    }
		    else if (!strncmp(cmd, "SAVEDEFAULT",11)) {			  
			    if(msglen>12) 
				    wlmngr_write_nvram_default((char *)cmd+12);
			    else
				    wlmngr_write_nvram_default(NULL);
		    }
		    else if (!strncmp(cmd, "MNGRRESTORE",11)) {
			  g_wlmngr_restart_all=1;
			    for (idx=0; idx<wl_cnt; idx++)
				    wlan_restart(WL_SYNC_FROM_MDM_TR69C, idx);
			  g_wlmngr_restart_all=0;
		    } else {
			  g_wlmngr_restart_all=1;
			    for (idx=0; idx<wl_cnt; idx++)
				    wlan_restart(WL_SYNC_TO_MDM_NVRAM, idx);
			  g_wlmngr_restart_all=0;
		    }
            }
        }
        else 
        {
            if (CMSRET_DISCONNECTED == ret)
            {
               if (cmsFil_isFilePresent(SMD_SHUTDOWN_IN_PROGRESS))
               {
                  /* normal smd shutdown, I will exit too. */
                  break;
               }
               else
               {
                /*
                 * uh-oh, lost comm link to smd; smd is prob dead.
                 * sleep 60 here to avoid lots of these messages or wlmngr
                 * can just exit.
                 */
                cmsLog_error("==>smd has crashed...");
                sleep(60);
               }
            }
            else
            {
               cmsLog_error("cmsMsg_receive failed, ret=%d", ret);
            }
        }
    }
#endif  /* BRCM_CMS_BUILD */

    /*Clean up*/
    wldsltr_free();
    wlmngr_free();

#ifdef BRCM_CMS_BUILD
    cmsMdm_cleanup_wrapper();
    cmsMsg_cleanup(&msgHandle);
    cmsLog_cleanup();
#endif  /* BRCM_CMS_BUILD */

#ifdef DSLCPE_WLCSM_EXT
    wlcsm_shutdown();
#endif
    return 0;
}
