/*
 * WPA Supplicant - Layer2 packet handling
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2005-2006, Devicescape Software, Inc. All Rights Reserved.
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: l2_packet.c 664710 2016-10-13 14:21:15Z $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/if.h>

#include "common.h"
#include "eloop.h"
#include "l2_packet.h"
#include <linux/filter.h>

/* Get own address from stored data */
int l2_packet_get_own_addr(struct l2_packet_data *l2, uint8 *addr)
{
	memcpy(addr, l2->own_addr, ETH_ALEN);
	return 0;
}

/* Send l2 packet */
int l2_packet_send(struct l2_packet_data *l2, uint8 *buf, size_t len)
{
	int ret;
	ret = send(l2->fd, buf, len, 0);
	if (ret < 0)
		perror("l2_packet_send - send");
	return ret;
}

/* Recieve the l2 packet and call the callback */
static void l2_packet_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct l2_packet_data *l2 = eloop_ctx;
	uint8 buf[2300];
	int res;
	struct l2_ethhdr *ethhdr;

	res = recv(sock, buf, sizeof(buf), 0);
	if (res < 0) {
		perror("l2_packet_receive - recv");
		return;
	}
	if (res < sizeof(*ethhdr)) {
		printf("l2_packet_receive: Dropped too short %d packet\n",
		       res);
		return;
	}

	ethhdr = (struct l2_ethhdr *) buf;

	l2->rx_callback(l2->rx_callback_ctx, ethhdr->h_source,
		(unsigned char *) (buf),
		res);
}

/* Initialize l2 socket */
struct l2_packet_data * l2_packet_init(
	const char *ifname, unsigned short protocol,
	void (*rx_callback)(void *ctx, unsigned char *src_addr, unsigned char *buf, size_t len),
	void *rx_callback_ctx)
{
	struct l2_packet_data *l2;
	struct ifreq ifr;
	struct sockaddr_ll ll;

	l2 = malloc(sizeof(struct l2_packet_data));
	if (l2 == NULL)
		return NULL;
	memset(l2, 0, sizeof(*l2));
	strncpy(l2->ifname, ifname, sizeof(l2->ifname)-1);
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;

	l2->fd = socket(PF_PACKET, SOCK_RAW, htons(protocol));
	if (l2->fd < 0) {
		perror("socket(PF_PACKET, SOCK_RAW)");
		free(l2);
		return NULL;
	}
	strncpy(ifr.ifr_name, l2->ifname, sizeof(ifr.ifr_name)-1);
	if (ioctl(l2->fd, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl[SIOCGIFINDEX]");
		close(l2->fd);
		free(l2);
		return NULL;
	}

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_ifindex = ifr.ifr_ifindex;
	ll.sll_protocol = htons(protocol);
	if (bind(l2->fd, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		perror("bind[PF_PACKET]");
		close(l2->fd);
		free(l2);
		return NULL;
	}

	if (ioctl(l2->fd, SIOCGIFHWADDR, &ifr) < 0) {
		perror("ioctl[SIOCGIFHWADDR]");
		close(l2->fd);
		free(l2);
		return NULL;
	}
	memcpy(l2->own_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	eloop_register_read_sock(l2->fd, l2_packet_receive, l2, NULL);

	return l2;
}

/* Deinitialize l2 socket */
void l2_packet_deinit(struct l2_packet_data *l2)
{
	if (l2 == NULL)
		return;

	if (l2->fd >= 0) {
		eloop_unregister_read_sock(l2->fd);
		close(l2->fd);
	}

	free(l2);
}
