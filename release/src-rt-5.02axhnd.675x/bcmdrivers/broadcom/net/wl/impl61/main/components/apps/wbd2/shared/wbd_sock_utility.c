/*
 * WBD socket utility for both client and server (Linux)
 *
 * Copyright 2019 Broadcom
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
 * $Id: wbd_sock_utility.c 772457 2019-02-25 10:27:32Z $
 */
#include <fcntl.h>

#include "wbd.h"
#include "wbd_sock_utility.h"

/* Closes the socket */
void
wbd_close_socket(int *sockfd)
{
	if (*sockfd == INVALID_SOCKET)
		return;

	close(*sockfd);
	*sockfd = INVALID_SOCKET;
}

/* Connects to the server given the IP address and port number */
int
wbd_connect_to_server(char* straddrs, unsigned int nport)
{
	struct sockaddr_in server_addr;
	int res, valopt;
	long arg;
	fd_set readfds;
	struct timeval tv;
	socklen_t lon;
	int sockfd;
	WBD_ENTER();

	sockfd = INVALID_SOCKET;
	memset(&server_addr, 0, sizeof(server_addr));

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		WBD_SOCKET("Socket call failed for ip[%s] port[%d] Error[%s]\n", straddrs,
			nport, strerror(errno));
		goto error;
	}

	WBD_SOCKET("Server = %s\t port = %d\n", straddrs, nport);

	/* Set nonblock on the socket so we can timeout */
	if ((arg = fcntl(sockfd, F_GETFL, NULL)) < 0 ||
		fcntl(sockfd, F_SETFL, arg | O_NONBLOCK) < 0) {
			WBD_SOCKET("fcntl call failed for ip[%s] port[%d] Error[%s]\n",
				straddrs, nport, strerror(errno));
			goto error;
	}
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(nport);
	server_addr.sin_addr.s_addr = inet_addr(straddrs);

	res = connect(sockfd, (struct sockaddr*)&server_addr,
		sizeof(struct sockaddr));
	if (res < 0) {
		if (errno == EINPROGRESS) {
			tv.tv_sec = WBD_TM_SOCKET;
			tv.tv_usec = 0;
			FD_ZERO(&readfds);
			FD_SET(sockfd, &readfds);
			if (select(sockfd+1, NULL, &readfds, NULL, &tv) > 0) {
				lon = sizeof(int);
				getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
					(void*)(&valopt), &lon);
				if (valopt) {
					WBD_SOCKET("For ip[%s] port[%d]. Error in getsockopt "
						"valopt[%d] Error[%s]\n", straddrs, nport,
						valopt, strerror(valopt));
					goto error;
				}
			} else {
				WBD_SOCKET("For ip[%s] port[%d]. Timeout or error() %d - %s\n",
					straddrs, nport, errno, strerror(errno));
				goto error;
			}
		} else {
			WBD_SOCKET("For ip[%s] port[%d]. Error connecting %d - %s\n",
				straddrs, nport, errno, strerror(errno));
			goto error;
		}
	}
	WBD_SOCKET("Connection Successfull with server : %s port[%d]\n", straddrs, nport);

	WBD_EXIT();
	return sockfd;

	/* Error Handling */
error:
	if (sockfd != INVALID_SOCKET)
		wbd_close_socket(&sockfd);
	WBD_EXIT();
	return INVALID_SOCKET;
}

/* Sends the data to socket */
unsigned int
wbd_socket_send_data(int sockfd, char *data, unsigned int len)
{
	int ret = 0;
	unsigned int nret = 0, totalsize = len, totalsent = 0;
	WBD_ENTER();

	/* Loop till all the data sent */
	while (totalsent < totalsize) {
		fd_set WriteFDs;
		struct timeval tv;

		FD_ZERO(&WriteFDs);

		if (sockfd == INVALID_SOCKET) {
			WBD_SOCKET("sockfd[%d]. Invalid socket\n", sockfd);
			goto error;
		}

		FD_SET(sockfd, &WriteFDs);

		tv.tv_sec = WBD_TM_SOCKET;
		tv.tv_usec = 0;
		if ((ret = select(sockfd+1, NULL, &WriteFDs, NULL, &tv)) > 0) {
			if (FD_ISSET(sockfd, &WriteFDs)) {
				;
			} else {
				WBD_SOCKET("sockfd[%d]. Exception occured\n", sockfd);
				goto error;
			}
		} else {
			if (ret == 0) {
				WBD_SOCKET("sockfd[%d]. Timeout occured\n", sockfd);
			} else {
				WBD_SOCKET("sockfd[%d]. Send Error : %s\n", sockfd,
					strerror(errno));
			}
			goto error;
		}

		nret = send(sockfd, &(data[totalsent]), len, 0);
		if (nret < 0) {
			WBD_SOCKET("sockfd[%d]. send error is : %s\n", sockfd, strerror(errno));
			goto error;
		}
		totalsent += nret;
		len -= nret;
		nret = 0;
	}

	WBD_EXIT();
	return totalsent;
error:
	WBD_EXIT();
	return 0;
}

/* to recieve data till the null character. caller should free the memory */
int
wbd_socket_recv_data(int sockfd, char **data)
{
	unsigned int nbytes, totalread = 0, cursize = 0;
	struct timeval tv;
	fd_set ReadFDs, ExceptFDs;
	char *buffer = NULL;
	int ret = 0;
	WBD_ENTER();

	/* Read till the null character or error */
	while (1) {
		FD_ZERO(&ReadFDs);
		FD_ZERO(&ExceptFDs);
		FD_SET(sockfd, &ReadFDs);
		FD_SET(sockfd, &ExceptFDs);
		tv.tv_sec = WBD_TM_SOCKET;
		tv.tv_usec = 0;

		/* Allocate memory for the buffer */
		if (totalread >= cursize) {
			char *tmp;

			cursize += MAX_READ_BUFFER;
			tmp = (char*)realloc(buffer, cursize);
			if (tmp == NULL) {
				WBD_SOCKET("sockfd[%d]. Failed to allocate memory for read "
					"buffer\n", sockfd);
				goto error;
			}
			buffer = tmp;
		}
		if ((ret = select(sockfd+1, &ReadFDs, NULL, &ExceptFDs, &tv)) > 0) {
			if (FD_ISSET(sockfd, &ReadFDs)) {
				/* fprintf(stdout, "SOCKET : Data is ready to read\n"); */;
			} else {
				WBD_SOCKET("sockfd[%d]. Exception occured\n", sockfd);
				goto error;
			}
		} else {
			if (ret == 0) {
				WBD_SOCKET("sockfd[%d]. Timeout occured\n", sockfd);
			} else {
				WBD_SOCKET("sockfd[%d]. Error While Reading : %s\n", sockfd,
					strerror(errno));
			}
			goto error;
		}

		nbytes = read(sockfd, buffer+totalread, (cursize - totalread));
		totalread += nbytes;

		if (nbytes <= 0) {
			WBD_SOCKET("sockfd[%d], read error is : %s\n", sockfd, strerror(errno));
			goto error;
		}

		/* Check the last byte for NULL termination */
		if (buffer[totalread-1] == '\0') {
			break;
		}
	}

	*data = buffer;

	WBD_EXIT();
	return totalread;

error:
	if (buffer)
		free(buffer);
	WBD_EXIT();
	return INVALID_SOCKET;
}

/* recieve the data of particular length from socket */
int
wbd_socket_recv_bindata(int sockfd, char *data, unsigned int dlen)
{
	unsigned int nbytes;

	nbytes = read(sockfd, data, dlen);

	if (nbytes <= 0) {
		WBD_SOCKET("sockfd[%d] read error is : %s\n", sockfd, strerror(errno));
		return INVALID_SOCKET;
	}

	return nbytes;
}

/* open a UDP packet to event dispatcher for receiving/sending data */
int wbd_open_eventfd(int portno)
{
	int reuse = 1;
	struct sockaddr_in sockaddr;
	int sockfd = INVALID_SOCKET;

	/* open loopback socket to communicate with event dispatcher */
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sockaddr.sin_port = htons(portno);

	if ((sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		WBD_SOCKET("portno[%d]. Unable to create loopback socket\n", portno);
		goto error;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		WBD_SOCKET("Unable to setsockopt to loopback socket[%d] portno[%d]\n", sockfd,
			portno);
		goto error;
	}

	if (bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		WBD_SOCKET("Unable to bind to loopback socket[%d] portno[%d]\n", sockfd, portno);
		goto error;
	}

	return sockfd;
	/* error handling */
error:
	if (sockfd != INVALID_SOCKET)
		wbd_close_socket(&sockfd);

	return INVALID_SOCKET;
}

/* Open a TCP socket for getting requests from client */
int
wbd_open_server_fd(int portno)
{
	int sockfd = INVALID_SOCKET, optval = 1;
	struct sockaddr_in sockaddr;

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockaddr.sin_port = htons(portno);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		WBD_SOCKET("portno[%d]. socket error is : %s\n", portno, strerror(errno));
		goto error;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		WBD_SOCKET("sockfd[%d] portno[%d]. setsockopt error is : %s\n", sockfd, portno,
			strerror(errno));
		goto error;
	}

	if (bind(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) < 0) {
		WBD_SOCKET("sockfd[%d] portno[%d]. bind error is : %s\n", sockfd, portno,
			strerror(errno));
		goto error;
	}

	if (listen(sockfd, 10) < 0) {
		WBD_SOCKET("sockfd[%d] portno[%d]. listen error is : %s\n", sockfd, portno,
			strerror(errno));
		goto error;
	}

	return sockfd;

error:
	if (sockfd != INVALID_SOCKET)
		wbd_close_socket(&sockfd);

	return INVALID_SOCKET;
}

/* Accept the connection from the client */
int
wbd_accept_connection(int server_fd)
{
	int childfd = INVALID_SOCKET;
	socklen_t clientlen;
	struct sockaddr_in clientaddr;

	clientlen = sizeof(clientaddr);
	childfd = accept(server_fd, (struct sockaddr *)&clientaddr, &clientlen);
	if (childfd < 0) {
		WBD_SOCKET("server_fd[%d] accept error : %s\n", server_fd, strerror(errno));
		return INVALID_SOCKET;
	}

	return childfd;
}

int
wbd_read_nl_sock(int sockFd, char *bufPtr, int seqNum, int pId)
{
	struct nlmsghdr *nlHdr;
	int readLen = 0, msgLen = 0;

	do {
		/* Recieve response from the kernel */
		if ((readLen = recv(sockFd, bufPtr, NL_SOCK_BUFSIZE - msgLen, 0)) < 0) {
			WBD_DEBUG("sockFd[%d]. SOCK READ %s\n", sockFd, strerror(errno));
			return WBDE_SOCK_KRECV_ERR;
		}

		nlHdr = (struct nlmsghdr *)bufPtr;

		/* Check if the header is valid */
		if ((NLMSG_OK(nlHdr, readLen) == 0) || (nlHdr->nlmsg_type == NLMSG_ERROR)) {
			WBD_DEBUG("sockFd[%d]. Error in recieved packet %s\n", sockFd,
				strerror(errno));
			return WBDE_SOCK_PKTHDR_ERR;
		}

		/* Check if the its the last message */
		if (nlHdr->nlmsg_type == NLMSG_DONE) {
			break;
		} else {
			/* Else move the pointer to buffer appropriately */
			bufPtr += readLen;
			msgLen += readLen;
		}

		/* Check if its a multi part message */
		if ((nlHdr->nlmsg_flags & NLM_F_MULTI) == 0) {
			/* return if its not */
			break;
		}
	} while ((nlHdr->nlmsg_seq != seqNum) || (nlHdr->nlmsg_pid != pId));

	return msgLen;
}

/* Get the IP address from socket FD */
int
wbd_sock_get_ip_from_sockfd(int sockfd, char *buf, int buflen)
{
	struct sockaddr_in peer;
	socklen_t peer_len;

	peer_len = sizeof(peer);
	if (getpeername(sockfd, (struct sockaddr*)&peer, &peer_len) == -1) {
		WBD_SOCKET("sockfd[%d]. getpeername error : %s\n", sockfd, strerror(errno));
		return WBDE_SOCK_ERROR;
	}
	snprintf(buf, buflen, "%s", inet_ntoa(peer.sin_addr));

	return WBDE_OK;
}

/* Read "length" bytes of "data" from non-blocking socket */
unsigned int
wbd_sock_recvdata(int sockfd, unsigned char *data, unsigned int length)
{
	int ret = 0;
	unsigned int nbytes, totalread = 0;
	struct timeval tv;
	fd_set ReadFDs, ExceptFDs;

	/* Keep on Reading, untill Total Read Bytes is less than length */
	while (totalread < length) {

		FD_ZERO(&ReadFDs);
		FD_ZERO(&ExceptFDs);
		FD_SET(sockfd, &ReadFDs);
		FD_SET(sockfd, &ExceptFDs);
		tv.tv_sec = WBD_TM_SOCKET;
		tv.tv_usec = 0;

		if ((ret = select(sockfd + 1, &ReadFDs, NULL, &ExceptFDs, &tv)) > 0) {
			if (!FD_ISSET(sockfd, &ReadFDs)) {
				WBD_SOCKET("sockfd[%d]. Exception occured.\n", sockfd);
				goto error;
			}
		} else {

			if (ret == 0) {
				WBD_SOCKET("sockfd[%d]. Timeout occured.\n", sockfd);
			} else {
				WBD_SOCKET("sockfd[%d]. Error While Reading : %s.\n",
					sockfd, strerror(errno));
			}
			goto error;

		}

		nbytes = read(sockfd, &(data[totalread]), (length - totalread));
		WBD_TRACE("sockfd[%d]. Read bytes  = %d\n\n", sockfd, nbytes);

		if (nbytes <= 0) {
			WBD_SOCKET("sockfd[%d], read error is : %s.\n",
				sockfd, strerror(errno));
			goto error;
		}

		totalread += nbytes;

	}

	return totalread;

error:
	return 0;
}
