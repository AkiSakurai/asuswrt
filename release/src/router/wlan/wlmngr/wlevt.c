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

/* This file includes the Wlan Driver Event handling */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <linux/if_packet.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "cms.h"
#include "cms_log.h"
#include "cms_util.h"
#include "cms_core.h"
#include "cms_msg.h"
#include "cms_dal.h"
#include "cms_cli.h"
#include "wlmdm.h"
#include <bcmnvram.h>
#include "wlsyscall.h"

#include <typedefs.h>
#include <proto/ethernet.h>
#include <wlioctl.h>
//#include <eapd.h>

/* Following Definition is from eapd.h. When eapd.h is opened, these definition should be removed*/
#define EAPD_WKSP_PORT_INDEX_SHIFT	4
#define EAPD_WKSP_SPORT_OFFSET		(1 << 5)

#define EAPD_WKSP_MEVENT_UDP_PORT       44000
#define EAPD_WKSP_MEVENT_UDP_RPORT      EAPD_WKSP_MEVENT_UDP_PORT
#define EAPD_WKSP_MEVENT_UDP_SPORT      EAPD_WKSP_MEVENT_UDP_PORT + EAPD_WKSP_SPORT_OFFSET


int wl_cnt =0;
int sock = -1;

void wlmngr_aquireStationList(int);
int wldsltr_alloc(int);
int wlmngr_alloc(int);
void wldsltr_free(void );
void wlmngr_free(void );

static void *msgHandle = NULL;
/* Init basic data structure */
static int wlevt_init( void )
{
	int reuse = 1;
	int err = 0;
	CmsRet ret = CMSRET_INTERNAL_ERROR;
	
	struct sockaddr_in sockaddr;

	/* open loopback socket to communicate with EAPD */
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sockaddr.sin_port = htons(EAPD_WKSP_MEVENT_UDP_SPORT);

	if (( sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		printf("%s@%d Unable to create socket\n", __FUNCTION__, __LINE__ );
		err = -1;
	}
	else if ( (err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse))) < 0) {
		printf("%s@%d: Unable to setsockopt to loopback socket %d.\n", __FUNCTION__, __LINE__, sock);
	}
	else if ( (err = bind(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) < 0) {
		printf("%s@%d Unable to bind to loopback socket %d\n", __FUNCTION__, __LINE__, sock);
	}

	if ( err < 0  && sock >= 0 ) {
		printf("%s@%d: failure. Close socket\n", __FUNCTION__, __LINE__ );
		close(sock);
		return err;
	}
#ifdef DSLCPE_EVT
	else 
		printf("%s@%d: opened loopback socket %d\n", __FUNCTION__, __LINE__, sock);
#endif

	if ((ret = cmsMsg_initWithFlags(EID_WLEVENT, 0, &msgHandle)) != CMSRET_SUCCESS)  {
		printf("could not initialize msg, ret=%d", ret);
		return -1;
	}
	return err;

}

/* De-initialization */
static void wlevt_deinit (void )
{
	if ( sock >= 0 )
		close(sock);

	if ( msgHandle )
		cmsMsg_cleanup(&msgHandle);
	return;
}

void wlevt_send_msg(int idx)
{
	static char buf[sizeof(CmsMsgHeader) + 32]={0};
	CmsRet ret = CMSRET_INTERNAL_ERROR;


	CmsMsgHeader *msg=(CmsMsgHeader *) buf;
	snprintf((char *)(msg + 1), sizeof(buf), "Assoc:%d", idx+1);

	msg->dataLength = 32;
	msg->type = CMS_MSG_WLAN_CHANGED;
	msg->src = EID_WLEVENT;
	msg->dst = EID_WLMNGR;
	msg->flags_event = 1;
	msg->flags_request = 0;

	if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
		printf("could not send CMS_MSG_WLAN_CHANGED msg to wlmngr, ret=%d", ret);
#ifdef DSLCPE_EVT
	else
		printf("message CMS_MSG_WLAN_CHANGED sent successfully");
#endif

	return;
 }

#ifdef DSLCPE_WPS_SEC_CLONE
/* Returns 1 if wl0 is up and running, 0 if not, -1 if it couldn't be
 * determined (error opening socket) */
static int wl0_ready(void)
{
	int sockfd;
	struct sockaddr_ll socket_address;
	struct ifreq ifr;
	int rc;

	memset(&socket_address, 0, sizeof(struct sockaddr_ll));
	strcpy(ifr.ifr_name, "wl0");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		return -1;
	}

	rc = ioctl(sockfd, SIOCGIFFLAGS, (char *) &ifr);
	close(sockfd);
	if (rc < 0) {
		return -1;
	}

	if (ifr.ifr_flags & IFF_RUNNING)
		return 1;
	else
		return 0;
}
#endif

/* Recv and handle event from wlan Driver */
static  void wlevt_main_loop( void )
{
	int bytes, len = 4096;
	char pkt[4096] = {0};
	struct sockaddr_in from;
	int sock_len = sizeof(struct sockaddr_in);
	int idx =0;
	
	bcm_event_t *dpkt;
	fd_set fdset;
	int fdmax = -1;
#ifdef DSLCPE_WPS_SEC_CLONE
	struct timeval tval;
#endif
#ifdef DSLCPE_EVT
	char * data;
	int status = 0;
	unsigned char * addr;
	int cnt=0;
#endif /* DSLCPE_EVT */			
	
	
	printf("wlevt is ready for new msg...\n");

	if (daemon(1, 1) == -1) {
		printf("%s: daemon error\n", __FUNCTION__);
		exit(errno);
	}

	FD_ZERO(&fdset);
	FD_SET(sock, &fdset);
	fdmax = sock;
	
	while ( 1 ) {
#ifdef DSLCPE_WPS_SEC_CLONE
		tval.tv_sec = 1;
		tval.tv_usec = 0;
		if (wl0_ready() > 0) {
			/* Return wlan interface to AP mode if last WPS-STA
			 * didn't succeed */
			if ((strcmp(nvram_get("wps_proc_status"), "4") == 0)
				  && (strcmp(nvram_get("wl0_mode"), "sta") == 0)) {
				struct timespec tspec;
				struct timespec trem;
				printf("wps-sta failed, return to AP mode\n");
				nvram_set("wl0_mode", "ap");
				nvram_set("wps_proc_status", "0");
				system("killall -SIGUSR1 wps_monitor");
				tspec.tv_sec = 2;
				tspec.tv_nsec = 0;
				while (nanosleep(&tspec, &trem) == -1) {
					tspec = trem;
				}
			}
		}
		select(fdmax+1, &fdset, NULL, NULL, &tval);
#else
    #ifdef DSLCPE_EVT
		status = select(fdmax+1, &fdset, NULL, NULL, NULL);
    #else
		select(fdmax+1, &fdset, NULL, NULL, NULL);
    #endif
#endif

		if (FD_ISSET(sock, &fdset)) 
		{
#ifdef DSLCPE_EVT
		 	printf("receive eapd event[%d]\n", cnt++);
#endif /* DSLCPE_EVT */			
				 	
			bytes = recvfrom(sock, pkt, len, 0, (struct sockaddr *)&from, (socklen_t *)&sock_len);

			if ( bytes <=0 ) {
				printf("Recv bytes Failure...\n");
				continue;
			}

			dpkt = (bcm_event_t *)pkt;
#ifdef DSLCPE_EVT
			data = (char *)(dpkt+1);
#endif

			if ( BCMILCP_BCM_SUBTYPE_EVENT != ntohs(dpkt->bcm_hdr.usr_subtype)) {
				printf("Not BCM Event received. Ignore \n");
				continue;
			}
#ifdef DSLCPE_EVT
		/* Parsing event here. WLC_E_xxx event definition is in bcmevent.h */
			printf("ntohs(dpkt->bcm_hdr.usr_subtype)=%x\n", ntohs(dpkt->bcm_hdr.usr_subtype) );
			printf("version=%x flags=%x event_type=%x status=%x reason=%x auth_type=%x datalen=%x\n",
			        dpkt->event.version, dpkt->event.flags, ntohl(dpkt->event.event_type), dpkt->event.status, dpkt->event.reason, 
				dpkt->event.auth_type, dpkt->event.datalen);

			addr = (unsigned char *)(&(dpkt->event.addr));
			printf("STA ADDR: [%02x-%02x-%02x-%02x-%02x-%02x]\n",addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
#endif /* DSLCPE_EVT */			

			/* Here, We could check Event type based on ntohl(dpkt->event.event_type) 
			    To make further action. 
			*/
			/* Event Assoc/Auth is passed to update tr69 element*/
			  if ((ntohl(dpkt->event.event_type) == WLC_E_AUTH_IND) ||
			      (ntohl(dpkt->event.event_type) == WLC_E_DEAUTH_IND) ||
			      (ntohl(dpkt->event.event_type) == WLC_E_ASSOC_IND) ||
			      (ntohl(dpkt->event.event_type) == WLC_E_REASSOC_IND) ||
			      (ntohl(dpkt->event.event_type) == WLC_E_DISASSOC_IND)) {
				/* Send event to update assoc list*/
				idx = dpkt->event.ifname[2] - '0';
				wlevt_send_msg(idx);
			}
		}
	}
	return;
}

/* Event Handling Main, called from wldaemon */
int main( void )
{
	if (wlevt_init() >=0) {
		/* main loop for waiting driver message */
		wlevt_main_loop();
	}

	wlevt_deinit();
	exit(-1);
}	



