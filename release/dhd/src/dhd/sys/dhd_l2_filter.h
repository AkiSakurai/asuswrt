/*
 * L2 Filter handling functions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dhd_l2_filter.h 473138 2014-04-30 14:01:06Z $
 *
 */
#ifndef _dhd_l2_filter_h_
#define _dhd_l2_filter_h_
extern int dhd_process_gratuitous_arp(dhd_pub_t *pub, void *pktbuf);
extern void dhd_l2_filter_watchdog(dhd_pub_t *dhdp);
extern void dhd_l2_filter_arp_table_update(dhd_pub_t *pub, int ifidx, bool all,
	uint8 *del_ea, bool periodic);
extern int dhd_l2fltr_pkt_handle(dhd_pub_t *pub, int ifidx, void *pktbuf, bool istx);
void *dhd_init_l2_arp_table(dhd_pub_t *dhdp, int ifidx);
void dhd_deinit_l2_arp_table(dhd_pub_t* pub, void* ptable);
#endif /* _dhd_l2_filter_h */
