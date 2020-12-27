/*
 * linux version of remote Wl transport mechanisms (pipes).
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
 * $Id: wlu_pipe_linux.c 712601 2017-07-25 12:01:48Z $
 */

/*  Revision History: Linux version of remote Wl transport mechanisms (pipes).
 *
 * Date        Author         Description
 *
 * 27-Dec-2007 Suganthi        Version 0.0
 *
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <termios.h>
#include <fcntl.h>
#include <802.11.h>
#include <bcmendian.h>
#include <bcmcdc.h>
#include <802.11.h>
#include <wlioctl.h>

#include <typedefs.h>
#include "wlu_remote.h"
#include <miniopt.h>
#if defined(RWL_DONGLE) || defined(RWL_SERIAL)
#define READ_DELAY 			500000
#define BAUD_RATE_115200	115200
#define VMIN_VAL			16
#define VTIME_VAL			50
#define LINUX_SYNC_DELAY	200
extern char  *g_rwl_device_name_serial;
extern int g_rwl_device_baud;
#endif // endif

#define MICRO_SEC_CONVERTER_VAL		1000
int g_irh;
int g_shellsync_pid;

#ifdef RWL_SOCKET
#define MAX_INTERFACE_NAME	32
static int
rwl_opensocket(int AddrFamily, int Type, int Protocol)
{
	int SockDes;

	if ((SockDes = socket(AddrFamily, Type, Protocol)) == -1) {
		perror("rwl_opensocket Fails:");
		DPRINT_ERR(ERR, "\nerrno:%d\n", errno);
		return FAIL;
	}
	return SockDes;
}

static int
rwl_set_socket_option(int SocketDes, int Level, int OptName, int Val)
{
	if (setsockopt(SocketDes, Level, OptName, &Val, sizeof(int)) == -1) {
		perror("Error at SetTCPSocketOpt()");
		DPRINT_ERR(ERR, "\n errno:%d\n", errno);
		return FAIL;
	}
	return SUCCESS;
}

/* Function to connect with the server waiting in the same port */
int
rwl_connectsocket(int SocketDes, struct sockaddr* SerAddr, int SizeOfAddr)
{
	if (connect(SocketDes, SerAddr, SizeOfAddr) == -1) {
		perror("Failed to connect() to server");
		DPRINT_ERR(ERR, "\n errno:%d\n", errno);
		return FAIL;
	}
	return SUCCESS;
}

/*
 * Function for associating a local address with a socket.
 */
int
rwl_bindsocket(int SocketDes, struct sockaddr * MyAddr, int SizeOfAddr)
{
	if (bind(SocketDes, MyAddr, SizeOfAddr) == -1) {
		perror("Error at rwl_bindSocket()");
		DPRINT_ERR(ERR, "\n errno:%d\n", errno);
		return FAIL;
	}
	return SUCCESS;
}

/*
 * Function for making the socket to listen for incoming connection.
 */
int
rwl_listensocket(int SocketDes, int BackLog)
{
	if (listen(SocketDes, BackLog) == -1) {
		perror("Error at rwl_listensocket()");
		DPRINT_ERR(ERR, "\n errno:%d\n", errno);
		return FAIL;
	}
	return SUCCESS;
}

/*
 * Function for permitting an incoming connection attempt on a socket
 * Function  called by server
 */
int
rwl_acceptconnection(int SocketDes, struct sockaddr* ClientAddr, int *SizeOfAddr)
{
	int NewSockDes;

	if ((NewSockDes = accept(SocketDes, ClientAddr, (socklen_t*)SizeOfAddr)) == -1) {
		perror("Error at rwl_acceptConnection()");
		DPRINT_ERR(ERR, "\n errno:%d\n", errno);
		return FAIL;
	}
	return NewSockDes;
}

static int
rwl_closesocket(int SocketDes)
{
	if (close(SocketDes) == -1) {
		perror("Error at rwl_closeSocket()");
		DPRINT_ERR(ERR, "\n errno:%d\n", errno);
		return FAIL;
	}
	return SUCCESS;
}

/* Transmit the response in the opened TCP stream socket */
int
rwl_send_to_streamsocket(int SocketDes, const char* SendBuff, int data_size, int Flag)
{
	int total_numwritten = 0,  numwritten = 0;
	while (total_numwritten < data_size) {
		if ((numwritten = send(SocketDes, SendBuff,
			data_size - total_numwritten, Flag)) == -1) {
			perror("Failed to send()");
			DPRINT_ERR(ERR, "\n errno:%d\n", errno);
			return (FAIL);
		}

		/* Sent successfully at first attempt no more retries */
		if (numwritten == data_size) {
			total_numwritten = numwritten;
			break;
		}

		/* If socket is busy we may hit this condition */
		if (numwritten != data_size - total_numwritten) {
			DPRINT_DBG(OUTPUT, "wanted to send %d bytes sent only %d bytes\n",
				data_size - total_numwritten, numwritten);
		}

		/* Now send the remaining buffer */
		total_numwritten += numwritten;
		SendBuff += numwritten;
	}

	return total_numwritten;
}

/* Receive the response from the opened TCP stream socket */
int
rwl_receive_from_streamsocket(int SocketDes, char* RecvBuff, int data_size, int Flag)
{
	int numread = 0;
	int total_numread = 0;

	while (total_numread < data_size) {
		if ((numread = recv(SocketDes, RecvBuff, data_size - total_numread, Flag)) == -1) {
			perror("Failed to Receive()");
			DPRINT_ERR(ERR, "\n errno:%d\n", errno);
			return FAIL;
		}

		if (numread != data_size - total_numread) {
			DPRINT_DBG(OUTPUT, "asked %d bytes got %d bytes\n",
				data_size - total_numread, numread);
		}

		if (numread == 0)
			break;

		total_numread += numread;
		RecvBuff += numread;
	}

	return numread;
}

int
rwl_init_server_socket_setup(int argc, char** argv, uint remote_type)
{
	char netif[MAX_INTERFACE_NAME];
	unsigned short servPort;
	struct sockaddr_in ServerAddress;
	int err, SockDes, val;

	/* Default option */
	servPort = DEFAULT_SERVER_PORT;

	strcpy(netif, "eth0");

	/* User option can override default arguments */
	if (argc == 3) {
		argv++;

		if (isalpha(**argv) == FALSE) {
			DPRINT_ERR(ERR, "USAGE ERROR:Incorrect network interface\n");
			return FAIL;
		}
		strcpy(netif, *argv);
		argv++;

		if (isdigit(**argv) == FALSE) {
			DPRINT_ERR(ERR, "USAGE ERROR:Incorrect port\n");
			return FAIL;
		}
		servPort = atoi(*argv);
	}

	if (argc == 2) {
		argv++;

		if (isalpha(**argv) == FALSE) {
			if (isdigit(**argv) == FALSE) {
				DPRINT_ERR(ERR, "USAGE ERROR\n");
				return FAIL;
			}
			else
				servPort = atoi(*argv);
		}
		else
			strcpy(netif, *argv);
	}

	DPRINT_INFO(OUTPUT, "INFO: Network Interface:%s, Port:%d\n",
		netif, servPort);

	if ((SockDes = (*(int *)rwl_open_transport(remote_type, NULL, 0, 0))) == FAIL)
	   return FAIL;

	val = 1;
	if ((rwl_set_socket_option(SockDes, SOL_SOCKET, SO_REUSEADDR, val)) == FAIL)
		return FAIL;

	memset(&ServerAddress, 0, sizeof(ServerAddress));

	rwl_GetifAddr(netif, &ServerAddress);
	ServerAddress.sin_family = AF_INET; /* host byte order */
	ServerAddress.sin_port = hton16(servPort); /* short, network byte order */

	if (((err = rwl_bindsocket(SockDes, (struct sockaddr *)&ServerAddress,
	sizeof(ServerAddress))) == FAIL))
		return err;
	if ((err = rwl_listensocket(SockDes, BACKLOG)) == FAIL)
		return err;

	DPRINT_DBG(OUTPUT, "Waiting for client to connect...\n");

	return SockDes;
}

int rwl_GetifAddr(char *ifname, struct sockaddr_in *sa)
{
	struct ifreq ifr;
	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (fd < 0)
	{
	   DPRINT_ERR(ERR, "socket open error\n");
	   return FAIL;
	}

	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0)
	{
		 memcpy(sa, (struct sockaddr_in *)&ifr.ifr_addr, sizeof(struct sockaddr_in));
	         close(fd);
		 return SUCCESS;
	}
	close(fd);
	return FAIL;
}
#endif /* RWL_SOCKET */

#if defined(RWL_SERIAL) || defined(RWL_DONGLE)

static speed_t
rwl_termios_b(int baud)
{
	speed_t result;
	switch (baud) {
		case 50:     result = B50;     break;
		case 75:     result = B75;     break;
		case 110:    result = B110;    break;
		case 134:    result = B134;    break;
		case 150:    result = B150;    break;
		case 200:    result = B200;    break;
		case 300:    result = B300;    break;
		case 600:    result = B600;    break;
		case 1200:   result = B1200;   break;
		case 1800:   result = B1800;   break;
		case 2400:   result = B2400;   break;
		case 4800:   result = B4800;   break;
		case 9600:   result = B9600;   break;
		case 19200:  result = B19200;  break;
		case 38400:  result = B38400;  break;
		case 57600:  result = B57600;  break;
		case 115200: result = B115200; break;
		case 230400: result = B230400; break;
		default:     result = B115200; break;
	}
	return result;
}
static int
rwl_open_serial(int remote_type, char *port, int baud)
{
	struct termios tio;
	int fCom;
	long BAUD, DATABITS, STOPBITS, PARITYON;
	speed_t baud_rate;

	DPRINT_DBG(OUTPUT, "\n rwl_open_serial:%s\n", port);
	if (remote_type == REMOTE_DONGLE)
		fCom = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
	else
		fCom = open(port, O_RDWR | O_NOCTTY | O_NDELAY | O_SYNC);
	if (fCom < 0) {
		DPRINT_ERR(ERR, "open COM failed with error %d.\n", errno);
		return fCom;
	} else {
		/* To make the read as a blocking operation */
		fcntl(fCom, F_SETFL, 0);
	}

	bzero(&tio, sizeof(tio));
	/* Get the current option for the port... */
	tcgetattr(fCom, &tio);
	/* Set the baud rate */
	cfsetispeed(&tio, rwl_termios_b(baud));
	cfsetospeed(&tio, rwl_termios_b(baud));
	if (remote_type == REMOTE_DONGLE) {
		if (tcsetattr(fCom, TCSANOW, &tio) < 0) {
			perror("tcsetattr:setspeed");
			return FAIL;
		}

		baud_rate = cfgetospeed(&tio);
		DPRINT_DBG(OUTPUT, "Baud_rate set is:%d\n", baud_rate);

		BAUD = baud_rate;
		DATABITS = CS8;
		STOPBITS = 1;
		PARITYON = 0;

		tio.c_cflag = BAUD |  DATABITS | STOPBITS | PARITYON | CLOCAL | CREAD;
		tio.c_iflag = IGNPAR;
		tio.c_oflag = 0;
		tio.c_lflag = 0;
		tio.c_cc[VMIN] = VMIN_VAL;
		tio.c_cc[VTIME] = VTIME_VAL;

		tcflush(fCom, TCIOFLUSH);
		if (tcsetattr(fCom, TCSANOW, &tio) < 0) {
			perror("tcsetattr:");
			return FAIL;
		}

		if (tcgetattr(fCom, &tio) < 0) {
			perror("tcgetattr:");
			return FAIL;
		}

		DPRINT_DBG(OUTPUT, "tcgetattr:VMIN is:%d\n", tio.c_cc[VMIN]);
		DPRINT_DBG(OUTPUT, "tcgetattr:VTIME is:%d\n", tio.c_cc[VTIME]);
		tcflush(fCom, TCIOFLUSH);
	}
	else {
		UNUSED_PARAMETER(PARITYON);
		UNUSED_PARAMETER(STOPBITS);
		UNUSED_PARAMETER(DATABITS);
		UNUSED_PARAMETER(BAUD);
		UNUSED_PARAMETER(baud_rate);

		/* Enable the receiver and set local mode  */
		tio.c_cflag |= (CLOCAL | CREAD);

		tio.c_cflag &= ~PARENB;
		tio.c_cflag &= ~CSTOPB;
		tio.c_cflag &= ~CSIZE;
		tio.c_cflag |= CS8;
		tio.c_cc[VTIME] = 255;
		tio.c_cc[VMIN] = 1;

		tio.c_iflag = 0;
		tio.c_iflag  |= IGNBRK;

		tio.c_oflag  &= ~OPOST;
		tio.c_oflag  &= ~OLCUC;
		tio.c_oflag  &= ~ONLCR;
		tio.c_oflag  &= ~OCRNL;
		tio.c_oflag  &= ~ONOCR;
		tio.c_oflag  &= ~ONLRET;
		tio.c_oflag  &= ~OFILL;

		tio.c_lflag &= ~ICANON;
		tio.c_lflag &= ~ISIG;
		tio.c_lflag &= ~XCASE;
		tio.c_lflag &= ~ECHO;
		tio.c_lflag &= ~FLUSHO;
		tio.c_lflag &= ~IEXTEN;
		tio.c_lflag |= NOFLSH;
		/* Set the new tio for the port... */
		tcsetattr(fCom, TCSANOW, &tio);
		tcflush(fCom, TCIOFLUSH);
	}
	return (fCom);
}

int
rwl_write_serial_port(void* hndle, char* write_buf, unsigned long size, unsigned long *numwritten)
{
	int ret;

	ret = write((*(int *)hndle), (const void*)write_buf, size);
	*numwritten = ret;
	if (ret == -1) {
		perror("WriteToPort Failed");
		DPRINT_ERR(ERR, "Errno:%d\n", errno);
		return FAIL;
	}
	if (*numwritten != size) {
		DPRINT_ERR(ERR, "rwl_write_serial_port failed numwritten %ld != len %ld\n",
		*numwritten, size);
		return FAIL;
	}
	return SUCCESS;
}

static int
rwl_check_inqueue(void *hndle, int timeout_ms, int *bufferlen)
{
	int inbuflen = 0;
	int stat;
	int delay_ms = 10;
	int n;
	int div = timeout_ms / delay_ms;
	if (bufferlen)
		*bufferlen = 0;
	for (n = 0; n < div; n++)
	{
		stat = ioctl(*(int *)hndle, FIONREAD, &inbuflen);
		if (stat == 0 && 0 < inbuflen)
		{
			if (bufferlen)
				*bufferlen = inbuflen;
			return 0;
		}
		usleep(delay_ms * 1000);
	}

	return -1;
}

int
rwl_read_serial_port(void* hndle, char* read_buf, uint data_size, uint *numread)
{
	int ret;
	uint total_numread = 0;
	int count = 0;
	int inqueuelen;
	int readlen;
	while (total_numread < data_size) {
		if (rwl_check_inqueue (hndle, 10000, &inqueuelen) != 0)
		{
				DPRINT_ERR(ERR, "input buffer is empty\n");
			return FAIL;
		}
		readlen = data_size - total_numread;
		if (inqueuelen < readlen)
			readlen = inqueuelen;

		ret = read(*(int *)hndle, read_buf, readlen);

		*numread = ret;
		if (ret == -1) {
			perror("ReadFromPort Failed");
			DPRINT_ERR(ERR, "Errno:%d\n", errno);
			return FAIL;
		}
		if (*numread != data_size - total_numread) {
			DPRINT_DBG(OUTPUT, "asked for %d bytes got %d bytes\n",
				data_size - total_numread, *numread);
		}
		if ((*numread == 0) && (count > 10000)) {
			DPRINT_ERR(ERR, "rwl_read_serial_port failed after %d retries\n",
				count);
			return FAIL;
		}

		total_numread += *numread;
		read_buf += *numread;
		count++;
	}
	return SUCCESS;
}

void
rwl_sync_delay(uint noframes)
{
	if (noframes > 1) {
		rwl_sleep(LINUX_SYNC_DELAY);
	}
}

#endif /* RWL_DONGLE ||RWL_SERIAL */

#if defined(RWL_DONGLE) || defined(RWL_SOCKET) || defined(RWL_SERIAL)
void*
rwl_open_transport(int remote_type, char *port, int ReadTotalTimeout, int debug)
{
	void* hndle;

	UNUSED_PARAMETER(port);
	UNUSED_PARAMETER(ReadTotalTimeout);
	UNUSED_PARAMETER(debug);

	switch (remote_type) {
#if defined(RWL_DONGLE) || defined(RWL_SERIAL)
	case REMOTE_SERIAL:
#ifdef RWL_SERIAL
			g_rwl_device_name_serial = port;
#endif // endif
	case REMOTE_DONGLE:
			if ((g_irh = rwl_open_serial(remote_type, g_rwl_device_name_serial,
					g_rwl_device_baud)) == FAIL) {
			/* Initial port opening settings failed in reboot.
			 * So retry opening the serial port
			 */
				if ((g_irh = rwl_open_serial(remote_type, g_rwl_device_name_serial,
						g_rwl_device_baud)) == FAIL) {
					DPRINT_ERR(ERR, "Can't open serial port\n");
					return NULL;
				}
			}
			break;
#endif /* RWL_DONGLE || RWL_SERIAL */

#ifdef RWL_SOCKET
		case REMOTE_SOCKET:
			if ((g_irh = rwl_opensocket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == FAIL) {
				DPRINT_ERR(ERR, "\nCan't open socket \n");
				return NULL;
			}

			break;
#endif /* RWL_SOCKET */

		default:
		DPRINT_ERR(ERR, "rwl_open_transport: Unknown remote_type %d\n", remote_type);
		return NULL;
		break;
	} /* end - switch case */

	hndle = (void*) &g_irh;
	return hndle;
}

int
rwl_close_transport(int remote_type, void* Des)
{
	switch (remote_type) {
#ifdef RWL_SOCKET
		case REMOTE_SOCKET:
			if (Des != NULL) {
				if (rwl_closesocket(*(int *)Des) == FAIL)
					return FAIL;
			}
		break;
#endif /* RWL_SOCKET */

#if defined(RWL_DONGLE) || defined(RWL_SERIAL)
	case REMOTE_DONGLE:
	case REMOTE_SERIAL:
			if (close(*(int *)Des) == -1)
				return FAIL;
		break;
#endif /* RWL_DONGLE || RWL_SERIAL */

		default:
			DPRINT_ERR(ERR, "close_pipe: Unknown remote_type %d\n", remote_type);
		break;
	}
	return SUCCESS;
}
#endif /* #if defined (RWL_DONGLE) || defined (RWL_SOCKET) */

void
rwl_sleep(int delay)
{
	usleep(delay * MICRO_SEC_CONVERTER_VAL);
}

#if defined(WLMSO) || defined(WLDYNLIB)
int
rwl_init_socket(void)
{
	return 0;
}
#endif // endif
