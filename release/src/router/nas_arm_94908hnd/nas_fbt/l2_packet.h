/*
 * WPA Supplicant - Layer2 packet handling
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2005-2006, Devicescape Software, Inc. All Rights Reserved.
 */
/* $Copyright... $
 *
 * $Id: l2_packet.h 663931 2016-10-07 11:45:25Z $
 */

#ifndef L2_PACKET_H
#define L2_PACKET_H

#include <typedefs.h>

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#ifndef ETH_P_EAPOL
#define ETH_P_EAPOL 0x888e
#endif

#ifndef ETH_P_RSN_PREAUTH
#define ETH_P_RSN_PREAUTH 0x88c7
#endif

#ifndef ETH_P_8021Q
#define ETH_P_8021Q 0x8100
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

struct l2_packet_data {
	int fd; /* packet socket for EAPOL frames */
	char ifname[20];
	uint8 own_addr[ETH_ALEN];
	void (*rx_callback)(void *ctx, unsigned char *src_addr, unsigned char *buf, size_t len);
	void *rx_callback_ctx;
};

struct l2_ethhdr {
    uint8 h_dest[ETH_ALEN];
    uint8 h_source[ETH_ALEN];
    uint16 h_proto;
} __attribute__ ((packed));

struct l2_vlanhdr {
    uint16 vlan_id;
    uint16 h_proto;
} __attribute__ ((packed));

struct l2_packet_data * l2_packet_init(
    const char *ifname, unsigned short protocol,
    void (*rx_callback)(void *ctx, unsigned char *src_addr,
                unsigned char *buf, size_t len),
    void *rx_callback_ctx);
void l2_packet_deinit(struct l2_packet_data *l2);

int l2_packet_get_own_addr(struct l2_packet_data *l2, uint8 *addr);
int l2_packet_send(struct l2_packet_data *l2, uint8 *buf, size_t len);

#endif /* L2_PACKET_H */
