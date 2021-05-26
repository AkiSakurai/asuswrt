/*
 * WBD data structure testing stub file
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
 * $Id: _wbd_ds.c 788830 2020-07-13 11:47:42Z $
 */

#include "wbd.h"
#include "wbd_shared.h"
#include "wbd_ds.h"
#include "wbd_sock_utility.h"
#include <unistd.h>
#include <fcntl.h>

/* ================================================================================ */
/* -------------------------------- TEST FUNCTION --------------------------------- */
/* ================================================================================ */
int test_wbd_ds()
{
	return 0;
}

/* return value contains the number of bytes read or -1 for error
 * free the buffer after reading the data
 */
static int
wbd_onrecv_hld(int sockfd, unsigned char **out_data, unsigned int *out_length)
{
	i5_higher_layer_data_recv_t hl_data_obj;
	unsigned int length = 0, ret_recv = 0;
	unsigned int sock_hdr_len = sizeof(hl_data_obj.tag) + sizeof(hl_data_obj.length);
	unsigned char header_buf[sizeof(hl_data_obj.tag) + sizeof(hl_data_obj.length)] = {0};
	unsigned char *buf = NULL;

	/* Validate args */
	if (!out_data || !out_length) {
		printf("Invalid Arguments\n");
		return -1;
	}

	/* First Pass : Read socket just till Header, to Get Total Read Buffer size from Header */
	ret_recv = wbd_sock_recvdata(sockfd, header_buf, sock_hdr_len);

	/* Check socket read bytes are valid or not */
	if (ret_recv <= 0) {

		printf("sockfd[%d]. Read Failed.\n", sockfd);
		return -1;
	}

	/* Get the Total Read Buffer size from Header Buffer, skipping first 4 bytes of tag */
	if (ret_recv >= sock_hdr_len) {

		memcpy(&length, header_buf + sizeof(hl_data_obj.tag), sizeof(length));
		printf("Total size : %u Total Read : %u\n", length, ret_recv);

	} else {

		printf("sockfd[%d]. Doesn't contain any size.\n", sockfd);
		return -1;
	}

	/* Alllocate Read Buffer of size "length", Note: length includes header size(8) as well */
	buf = (unsigned char *)malloc(sizeof(unsigned char) * (length));
	if (!buf) {
		printf("malloc error\n");
		return -1;
	}

	/* Copy Header 8 bytes to Read Buffer */
	memcpy(buf, header_buf, sock_hdr_len);

	/* If there is no additional data to be read apart from Header, Exit */
	if (length == sock_hdr_len) {
		printf("sockfd[%d]. Read Buffer length is 0. indicates there is no data.", sockfd);
		goto end;
	}

	/* Second Pass : Read Actual data, skipping Header 8 bytes, which we already read */
	ret_recv = wbd_sock_recvdata(sockfd, buf + sock_hdr_len,
		(length - sock_hdr_len));
	printf("Total size : %u Total Read : %u\n", length - sock_hdr_len, ret_recv);

	/* Check socket read bytes are valid or not */
	if (ret_recv <= 0) {

		if (buf != NULL) {
			free(buf);
			buf = NULL;
		}

		printf("sockfd[%d]. Read Failed.\n", sockfd);
		return -1;
	}

end:
	/* Return Filled Data Buffer & Total Read Bytes */
	*out_data = buf;
	*out_length = length;

	return 0;
}

static int
stub_recv_hld(int evt_port, unsigned char *out_src_al_mac_str, unsigned char *out_protocol,
	unsigned char *out_header, unsigned int *out_header_len,
	unsigned char *out_payload, unsigned int *out_payload_len)
{
	int server_fd = INVALID_SOCKET, childfd = INVALID_SOCKET, rcv_ret = -1, ret = WBDE_OK;
	unsigned int payload_length = 0, header_offset = 0, payload_offset = 0;
	unsigned char *read_buf = NULL, *data_buf = NULL;
	unsigned int rcv_length = 0;
	i5_higher_layer_data_recv_t *hl_data = NULL;
	long arg;

	/* Validate args */
	if (!out_src_al_mac_str || !out_protocol || !out_header || !out_header_len ||
		!out_payload || !out_payload_len) {
		ret = WBDE_INV_ARG;
		goto end;
	}

	/* Try to open the server FD */
	server_fd = wbd_open_server_fd(evt_port);
	if (server_fd == INVALID_SOCKET) {
		printf("Failed to create server socket at port (0x%x) for HLD. Err=%d (%s)\n",
			evt_port, ret, wbderrorstr(ret));
		ret = INVALID_SOCKET;
		goto end;
	}

	/* Accept the connection */
	if ((childfd = wbd_accept_connection(server_fd)) == INVALID_SOCKET) {
		printf("hndl->sockfd[%d], %s\n", server_fd, wbderrorstr(WBDE_COM_FL_ACPT));
		ret = INVALID_SOCKET;
		goto end;
	}

	/* Set nonblock on the socket so we can timeout */
	if ((arg = fcntl(childfd, F_GETFL, NULL)) < 0 ||
		fcntl(childfd, F_SETFL, arg | O_NONBLOCK) < 0) {
		printf("fcntl call failed for port[%d] Error[%s]\n", evt_port, strerror(errno));
		ret = INVALID_SOCKET;
		goto end;
	}

	/* Get the data from client */
	rcv_ret = wbd_onrecv_hld(childfd, &read_buf, &rcv_length);

	if ((rcv_ret < 0) || (read_buf == NULL)) {
		printf("No data to read. Closing Socket: hndl->sockfd[%d] childfd[%d]\n",
			server_fd, childfd);
		ret = INVALID_SOCKET;
		goto end;
	}
	hl_data  = (i5_higher_layer_data_recv_t *)read_buf;
	data_buf = (unsigned char *)read_buf;

	payload_length = hl_data->length - (sizeof(hl_data->tag) + sizeof(hl_data->length) +
		sizeof(hl_data->src_al_mac) + sizeof(hl_data->protocol) +
		sizeof(hl_data->header_length) + hl_data->header_length);

	printf("HLD RECEIVED :\n Tag[%u] Length[%u] SRC_AL_MAC["MACDBG"] "
		"Protocol[%u] Header_Length[%u] Payload_Length[%u]\n",
		hl_data->tag, hl_data->length, MAC2STRDBG(hl_data->src_al_mac),
		hl_data->protocol, hl_data->header_length, payload_length);

	header_offset = sizeof(hl_data->tag) + sizeof(hl_data->length) +
		sizeof(hl_data->src_al_mac) + sizeof(hl_data->protocol) +
		sizeof(hl_data->header_length);

	payload_offset = header_offset + hl_data->header_length;

	wbd_hexdump_ascii("HLD HDR RECEIVED : ", &data_buf[header_offset], hl_data->header_length);
	if (payload_length > 0) {
		wbd_hexdump_ascii("HLD PLD RECEIVED : ", &data_buf[payload_offset], payload_length);
	}

	/* Assign out data pointers */
	*out_protocol = hl_data->protocol;

	memcpy(out_src_al_mac_str, hl_data->src_al_mac, sizeof(hl_data->src_al_mac));

	memcpy(out_header, &data_buf[header_offset], hl_data->header_length);
	*out_header_len = hl_data->header_length;

	if (payload_length > 0) {
		memcpy(out_payload, &data_buf[payload_offset], payload_length);
	}
	*out_payload_len = payload_length;
end:
	if (read_buf) {
		free(read_buf);
	}
	wbd_close_socket(&server_fd);

	return ret;
}

int
hle_recv_hld(int evt_port, unsigned char *out_src_al_mac_str, unsigned char *out_protocol,
	unsigned char *out_header, unsigned int *out_header_len,
	unsigned char *out_payload, unsigned int *out_payload_len)
{
	int sockfd = -1, childfd = -1, optval = 1, ret = 0;
	unsigned int payload_length = 0, header_offset = 0, payload_offset = 0;
	unsigned char *read_buf = NULL, *data_buf = NULL;
	i5_higher_layer_data_recv_t hl_data_obj, *hl_data = NULL;
	long arg;
	struct sockaddr_in sockaddr;
	socklen_t clientlen;
	struct sockaddr_in clientaddr;
	unsigned int rcv_length = 0;
	unsigned int sock_hdr_len = sizeof(hl_data_obj.tag) + sizeof(hl_data_obj.length);
	unsigned char header_buf[sizeof(hl_data_obj.tag) + sizeof(hl_data_obj.length)] = {0};

	unsigned int nbytes = 0, totalread = 0;
	struct timeval tv;
	fd_set ReadFDs, ExceptFDs;

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sockaddr.sin_port = htons(evt_port);

	/* Open a TCP Socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	/* Set Socket options */
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	/* bind socket and assign it a local address */
	bind(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr));

	/* listen for connections on a socket */
	listen(sockfd, 10);

	/* Accept the connection from the client */
	clientlen = sizeof(clientaddr);
	childfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);

	/* Set nonblock on the socket so we can timeout */
	arg = fcntl(childfd, F_GETFL, NULL);
	fcntl(childfd, F_SETFL, arg | O_NONBLOCK);

	/* First Pass : Read socket just till Header, to Get Total Read Buffer size from Header */
	nbytes = 0, totalread = 0;

	/* Keep on Reading, untill Total Read Bytes is less than sock_hdr_len */
	while (totalread < sock_hdr_len) {
		FD_ZERO(&ReadFDs);
		FD_ZERO(&ExceptFDs);
		FD_SET(childfd, &ReadFDs);
		FD_SET(childfd, &ExceptFDs);
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		/* monitor multiple file descriptors */
		select(childfd + 1, &ReadFDs, NULL, &ExceptFDs, &tv);

		/* Receives the sock_data from socket */
		nbytes = read(childfd, &(header_buf[totalread]), (sock_hdr_len - totalread));

		totalread += nbytes;
	}

	/* Get the Total Read Buffer size from Header Buffer, skipping first 4 bytes of tag */
	if (totalread >= sock_hdr_len) {
		memcpy(&rcv_length, header_buf + sizeof(hl_data_obj.tag), sizeof(rcv_length));
		printf("Total size : %u Total Read : %u\n", rcv_length, totalread);
	}

	/* Allocate Read Buf of size "rcv_length", Note : rcv_length includes hdr_sz(8) as well */
	read_buf = (unsigned char *)malloc(sizeof(unsigned char) * (rcv_length));

	/* Copy Header 8 bytes to Read Buffer */
	memcpy(read_buf, header_buf, sock_hdr_len);

	/* Second Pass : Read Actual data, skipping Header 8 bytes, which we already read */
	nbytes = 0, totalread = 0;

	/* Keep on Reading, untill Total Read Bytes is less than (rcv_length - sock_hdr_len) */
	while (totalread < (rcv_length - sock_hdr_len)) {
		FD_ZERO(&ReadFDs);
		FD_ZERO(&ExceptFDs);
		FD_SET(childfd, &ReadFDs);
		FD_SET(childfd, &ExceptFDs);
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		/* monitor multiple file descriptors */
		select(childfd + 1, &ReadFDs, NULL, &ExceptFDs, &tv);

		/* Receives the sock_data from socket */
		nbytes = read(childfd, &(read_buf[sock_hdr_len + totalread]),
			((rcv_length - sock_hdr_len) - totalread));

		totalread += nbytes;
	}

	if ((totalread < 0) || (read_buf == NULL)) {
		printf("No data to read. Closing Socket: hndl->sockfd[%d] childfd[%d]\n",
			sockfd, childfd);
		ret = -1;
		goto end;
	}

	hl_data = (i5_higher_layer_data_recv_t *)read_buf;
	data_buf = (unsigned char *)read_buf;

	header_offset = sizeof(hl_data->tag) + sizeof(hl_data->length) +
		sizeof(hl_data->src_al_mac) + sizeof(hl_data->protocol) +
		sizeof(hl_data->header_length);

	payload_offset = header_offset + hl_data->header_length;

	payload_length = hl_data->length - payload_offset;

	printf("HLD RECEIVED :\n Tag[%u] Length[%u] SRC_AL_MAC["MACDBG"] "
		"Protocol[%u] Header_Length[%u] Payload_Length[%u]\n",
		hl_data->tag, hl_data->length, MAC2STRDBG(hl_data->src_al_mac),
		hl_data->protocol, hl_data->header_length, payload_length);

	wbd_hexdump_ascii("HLD HDR RECEIVED : ", &data_buf[header_offset], hl_data->header_length);
	if (payload_length > 0) {
		wbd_hexdump_ascii("HLD PLD RECEIVED : ", &data_buf[payload_offset], payload_length);
	}

	/* Assign out data pointers */
	*out_protocol = hl_data->protocol;

	memcpy(out_src_al_mac_str, hl_data->src_al_mac, sizeof(hl_data->src_al_mac));

	memcpy(out_header, &data_buf[header_offset], hl_data->header_length);
	*out_header_len = hl_data->header_length;

	if (payload_length > 0) {
		memcpy(out_payload, &data_buf[payload_offset], payload_length);
	}
	*out_payload_len = payload_length;

end:
	/* Free read_buf & Close the socket */
	if (read_buf) {
		free(read_buf);
	}
	close(sockfd);
	sockfd = -1;

	return ret;
}

int
stub_recv_hld_hlpr(unsigned int evt_port, unsigned int max_payload_length)
{
	int ret = WBDE_OK;
	unsigned char protocol = 0;
	char src_al_mac_str[20] = {0};
	unsigned char *header = NULL, *payload = NULL;
	unsigned int header_length = 0, payload_length = 0;
	struct ether_addr src_al_mac;

	header = (unsigned char*)wbd_malloc(30, &ret);
	WBD_ASSERT();

	payload = (unsigned char*)wbd_malloc(max_payload_length, &ret);
	WBD_ASSERT();

	/* Receive Dummy HLD Data */
	ret = stub_recv_hld(evt_port, (unsigned char*)&src_al_mac, &protocol,
	/* ret = hle_recv_hld(evt_port, (unsigned char*)&src_al_mac, &protocol, */
		header, &header_length, payload, &payload_length);

	wbd_ether_etoa((unsigned char*)&src_al_mac, src_al_mac_str);

end:
	/* Free Dummy HLD Header and Payload memory */
	if (header) {
		free(header);
	}
	if (payload) {
		free(payload);
	}

	return ret;
}

static int
stub_prepare_dummy_hld(unsigned char **out_buffer, unsigned int in_length)
{
	int ret = WBDE_OK;
	unsigned iter_idx = 0, iter_digit = 0, digit = 0;
	unsigned char * data = NULL;

	data = (unsigned char*)wbd_malloc(in_length, &ret);
	WBD_ASSERT();

	for (iter_idx = 0, digit = 0; iter_idx < in_length; digit++) {

		if (digit >= 10) {
			digit = 0;
		}

		for (iter_digit = 0; iter_digit < 16; iter_digit++) {

			if (iter_idx < in_length) {
				data[iter_idx++] = digit;
			} else {
				goto end;
			}
		}

	}

end:
	*out_buffer = data;
	return ret;
}

/* cli_port = EAPD_WKSP_WBD_AGENTCLI_PORT(50260) / EAPD_WKSP_WBD_CTRLCLI_PORT(50261) */
static int
stub_send_hld(unsigned int cli_port, unsigned char *in_dst_al_mac_str, unsigned char in_protocol,
	unsigned char *in_payload, unsigned int in_payload_length)
{
	int sockfd = -1, ret = WBDE_OK;
	unsigned int sock_data_len = 0, payload_offset = 0;
	unsigned char *sock_data = NULL;
	i5_higher_layer_data_send_t hl_data_obj, *hl_data = NULL;

	/* Get Socket data Size and Allocate the Buffer for Socket data to be sent */
	payload_offset = sizeof(hl_data_obj.tag) + sizeof(hl_data_obj.length) +
		sizeof(hl_data_obj.dst_al_mac) + sizeof(hl_data_obj.protocol);
	sock_data_len = payload_offset + in_payload_length;

	sock_data = (unsigned char*)wbd_malloc(sock_data_len, &ret);
	WBD_ASSERT();

	/* Prepare sock data. */
	hl_data = (i5_higher_layer_data_send_t*)sock_data;

	/* [1] Higher Layer Data TAG. Its value is fixed to I5_API_CMD_HLE_SEND_HL_DATA (44) */
	hl_data->tag = 44;

	/* [2] Length : Total Length of all the fields of this structure object, which is =
	* 4 + 4 + 6 + 1 + Paylaod length (Variable)
	*/
	hl_data->length = sock_data_len;

	/* [3] Destination AL MAC Address : AL Mac address of receiver of HLD */
	memcpy(hl_data->dst_al_mac, in_dst_al_mac_str, IEEE1905_MAC_ADDR_LEN);

	/* [4] Higher Layer Protocol ID of HLD, sent towards HLE through BRCM MAP Entity */
	hl_data->protocol = in_protocol;

	/* [5] Copy Actual Payload */
	if (in_payload_length > 0) {
		memcpy(&sock_data[payload_offset], in_payload, in_payload_length);
	}

	/* Connect to the server */
	sockfd = wbd_connect_to_server(WBD_LOOPBACK_IP, cli_port);
	if (sockfd == INVALID_SOCKET) {
		printf("Failed to connect\n");
		ret = WBDE_SOCK_ERROR;
		goto end;
	}

	wbd_hexdump_ascii("HLE(wb_cli -t 1) Sending HLD : ", sock_data, sock_data_len);

	/* Send the payload */
	if (wbd_socket_send_data(sockfd, (char*)sock_data, sock_data_len) <= 0) {
		printf("Failed to send\n");
		ret = WBDE_SOCK_ERROR;
		goto end;
	}

end:
	if (sock_data) {
		free(sock_data);
	}
	wbd_close_socket(&sockfd);

	return ret;
}

int
hle_send_hld(unsigned int cli_port, unsigned char *in_dst_al_mac_str, unsigned char in_protocol,
	unsigned char *in_payload, unsigned int in_payload_length)
{
	int sockfd = -1, res, valopt;
	struct sockaddr_in server_addr;
	long arg;
	fd_set readfds;
	struct timeval tv;
	socklen_t lon;
	unsigned int sock_data_len = 0, payload_offset = 0, nret = 0, totalsize = 0, totalsent = 0;
	unsigned char *sock_data = NULL;
	i5_higher_layer_data_send_t hl_data_obj, *hl_data = NULL;

	/* Get Socket data Size and Allocate the Buffer for Socket data to be sent */
	payload_offset = sizeof(hl_data_obj.tag) + sizeof(hl_data_obj.length) +
		sizeof(hl_data_obj.dst_al_mac) + sizeof(hl_data_obj.protocol);
	sock_data_len = payload_offset + in_payload_length;
	sock_data = (unsigned char*)malloc(sock_data_len);

	/* Prepare sock data. */
	hl_data = (i5_higher_layer_data_send_t*)sock_data;

	/* [1] Higher Layer Data TAG. Its value is fixed to I5_API_CMD_HLE_SEND_HL_DATA (44) */
	hl_data->tag = 44;

	/* [2] Length : Total Length of all the fields of this structure object, which is =
	* 4 + 4 + 6 + 1 + Paylaod length (Variable)
	*/
	hl_data->length = sock_data_len;

	/* [3] Destination AL MAC Address : AL Mac address of receiver of HLD */
	memcpy(hl_data->dst_al_mac, in_dst_al_mac_str, IEEE1905_MAC_ADDR_LEN);

	/* [4] Higher Layer Protocol ID of HLD, sent towards HLE through BRCM MAP Entity */
	hl_data->protocol = in_protocol;

	/* [5] Copy Actual Payload */
	if (in_payload_length > 0) {
		memcpy(&sock_data[payload_offset], in_payload, in_payload_length);
	}

	/* Create a TCP Socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	/* Set nonblock on the socket so we can timeout */
	arg = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, arg | O_NONBLOCK);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(cli_port);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	/* Connect to the server */
	res = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
	if ((res < 0) && (errno == EINPROGRESS)) {
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);

		/* monitor multiple file descriptors */
		if (select(sockfd+1, NULL, &readfds, NULL, &tv) > 0) {
			lon = sizeof(int);
			getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
		}
	}

	wbd_hexdump_ascii("HLE(wb_cli -t 1) Sending HLD : ", sock_data, sock_data_len);

	nret = 0, totalsize = sock_data_len, totalsent = 0;

	/* Loop till all the sock_data sent */
	while (totalsent < totalsize) {
		fd_set WriteFDs;
		struct timeval tv;
		FD_ZERO(&WriteFDs);
		FD_SET(sockfd, &WriteFDs);
		tv.tv_sec = 10;
		tv.tv_usec = 0;

		/* monitor multiple file descriptors */
		select(sockfd+1, NULL, &WriteFDs, NULL, &tv);

		/* Sends the sock_data to socket */
		nret = send(sockfd, &(sock_data[totalsent]), sock_data_len, 0);

		totalsent += nret;
		sock_data_len -= nret;
		nret = 0;
	}

	/* Free sock_data & Close the socket */
	if (sock_data) {
		free(sock_data);
	}
	close(sockfd);
	sockfd = -1;

	return 0;
}

int
stub_send_hld_hlpr(unsigned int cli_port, char *dst_al_mac_str,
	unsigned char protocol, unsigned int payload_length)
{
	/* Prapare Dummy HLD Data */
	int ret = WBDE_OK;
	unsigned char* payload = NULL;
	struct ether_addr dst_al_mac;

	if (payload_length > 0) {
		/* Allocate Dummy HLD Payload memory */
		ret = stub_prepare_dummy_hld(&payload, payload_length);
		WBD_ASSERT();
	}

	wbd_ether_atoe(dst_al_mac_str, &dst_al_mac);

	/* Send Dummy HLD Data */
	ret = stub_send_hld(cli_port, (unsigned char*)&dst_al_mac, protocol,
	/* ret = hle_send_hld(cli_port, (unsigned char*)&dst_al_mac, protocol, */
		payload, payload_length);
	WBD_ASSERT();

end:
	/* Free Dummy HLD Payload memory */
	if (payload) {
		free(payload);
	}
	return ret;
}

/* ================================================================================ */
