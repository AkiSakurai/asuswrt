/*
 * WBD socket utility for both client and server (Linux)
 *
 * Copyright 2020 Broadcom
 *
 * This program is the proprietary software of Broadcom and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY
 * WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 * THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY
 * DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY,
 * NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES,
 * ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING
 * OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL
 * BROADCOM OR ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL,
 * SPECIAL, INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR
 * IN ANY WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN
 * IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii)
 * ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF
 * OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 * NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wbd_sock_utility.h 772457 2019-02-25 10:27:32Z $
 */

#ifndef _WBD_SOCK_UTILITY_H_
#define _WBD_SOCK_UTILITY_H_

#include <net/if.h>
#include <arpa/inet.h>
#include <linux/rtnetlink.h>

#define INVALID_SOCKET		-1
#define MAX_READ_BUFFER		1448
#define WBD_BUFFSIZE_4K		4096

#define NL_SOCK_BUFSIZE		8192

extern void wbd_close_socket(int *sockfd);

extern int wbd_connect_to_server(char *straddrs, unsigned int nport);

extern unsigned int wbd_socket_send_data(int sockfd, char *data, unsigned int len);

extern int wbd_socket_recv_data(int sockfd, char **data);

extern int wbd_socket_recv_bindata(int sockfd, char *data, unsigned int dlen);

extern int wbd_open_eventfd(int portno);

extern int wbd_open_server_fd(int portno);

extern int wbd_accept_connection(int server_fd);

extern int wbd_read_nl_sock(int sockFd, char *bufPtr, int seqNum, int pId);

/* Get the IP address from socket FD */
extern int wbd_sock_get_ip_from_sockfd(int sockfd, char *buf, int buflen);

/* Read "length" bytes of "data" from non-blocking socket */
extern unsigned int wbd_sock_recvdata(int sockfd, unsigned char *data,
	unsigned int length);

#endif /* _WBD_SOCK_UTILITY_H_ */
