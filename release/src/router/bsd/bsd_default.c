/*
 * bsd scheme  (Linux)
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2016,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id: bsd_default.c $
 */
#include "bsd.h"

/* The default policy for 2.4G and 5G dualband boards
 *   steer STAs to 2.4G RF from 5G RF if 5G is oversubscribed
 *   steer STAs to 5G RF from 2.4G RF if 5G is undersubscribed
 *
 * The STA selection is based on phyrate
 */
/* nvram config for eth1(5G), eth2(2.4G) ref. board e.g. BCM94706nr2hmc */
static struct nvram_tuple bsd_5g2g_policy[] = {
	{"bsd_ifnames", "eth1 eth2", 0},
	{"bsd_scheme", "2", 0},
	{"wl0_bsd_steering_policy", "80 5 3 0 0 0x40", 0},
	{"wl1_bsd_steering_policy", "0 5 3 0 0 0x50", 0},
	{"wl0_bsd_sta_select_policy", "0 0 0 0 0 1 0 0 0 0x240", 0},
	{"wl1_bsd_sta_select_policy", "0 0 0 0 0 1 0 0 0 0x240", 0},
	{"wl0_bsd_if_select_policy", "eth2", 0},
	{"wl1_bsd_if_select_policy", "eth1", 0},
	{"wl0_bsd_if_qualify_policy", "20 0x0 -75", 0},
	{"wl1_bsd_if_qualify_policy", "0 0x0 -75", 0},
	{"bsd_bounce_detect", "180 2 3600", 0},
	{0, 0, 0}
};

/* nvram config for eth1(2.4G), eth2(5G) ref. board e.g. BCM94708r */
static struct nvram_tuple bsd_2g5g_policy[] = {
	{"bsd_ifnames", "eth1 eth2", 0},
	{"bsd_scheme", "2", 0},
	{"wl0_bsd_steering_policy", "0 5 3 0 0 0x50", 0},
	{"wl1_bsd_steering_policy", "80 5 3 0 0 0x40", 0},
	{"wl0_bsd_sta_select_policy", "0 0 0 0 0 1 0 0 0 0x240", 0},
	{"wl1_bsd_sta_select_policy", "0 0 0 0 0 1 0 0 0 0x240", 0},
	{"wl0_bsd_if_select_policy", "eth2", 0},
	{"wl1_bsd_if_select_policy", "eth1", 0},
	{"wl0_bsd_if_qualify_policy", "0 0x0 -75", 0},
	{"wl1_bsd_if_qualify_policy", "20 0x0 -75", 0},
	{"bsd_bounce_detect", "180 2 3600", 0},
	{0, 0, 0}
};

/* The default policy for 3 RFs,, eth1(5G Lo), eth2(2.4G), eth3(5G Hi) board
 *   STAs are steered between 5G Low, and 5G High RFs
 *   STAs with phyrate less than 300Mbps are steered to 5G High
 *   STAs with phyrate greater than or equal to 300Mbps are steered to 5G Low
 *
 *   In case of 5G Low oversubscription, BSD steers STAs to 5G High from 5G Low
 *
 * The STA selection is based on phyrate, and is not 4Kbps active
 */
/* e.g. BCM94709acdcrh */
static struct nvram_tuple bsd_5glo_2g_5ghi_policy[] = {
	{"bsd_ifnames", "eth1 eth2 eth3", 0},
	{"bsd_scheme", "2", 0},
	{"wl1_bsd_steering_policy", "80 5 3 0 100 200 0x20", 0},  		  /* 5G lo: trigger steer when channel busy or rate < 100M or rate > 200M */
	{"wl0_bsd_steering_policy", "0 5 3 0 0 100 0x20", 0},               /* 2G: when rate > 100M go to 5G */
	{"wl2_bsd_steering_policy", "0 5 3 0 200 0 0x0", 0},              /* 5G hi: trigger steer when rate < 200M */
	{"wl1_bsd_sta_select_policy", "4 0 100 200 0 0 1 0 0 0 0x220", 0}, /* 5G lo: select sta if rate < 100 or rate > 200 */
	{"wl0_bsd_sta_select_policy", "4 0 0 0 100 0 -1 0 0 0 0x220", 0},    /* 2G: select sta if rate > 100 */
	{"wl2_bsd_sta_select_policy", "4 0 200 0 0 0 1 0 0 0 0x200", 0},  /* 5G hi: select sta if rate < 200*/
	{"wl1_bsd_if_select_policy", "eth3 eth2", 0},	/* 5G lo: preferred 5G hi, 2G, (if rate > 300M steer to 5G, and if rate < 200M steer to 2G) */
	{"wl0_bsd_if_select_policy", "eth3 eth1", 0},	/* 2G: preferred 5G hi, 5G lo */
	{"wl2_bsd_if_select_policy", "eth1 eth2", 0},	/* 5G hi: preferred 5G lo, 2G */
	{"wl1_bsd_if_qualify_policy", "60 0x0", 0},
	{"wl0_bsd_if_qualify_policy", "0 0x0", 0},
	{"wl2_bsd_if_qualify_policy", "0 0x0", 0},
	{"bsd_bounce_detect", "180 4 3600", 0},
	{0, 0, 0}
};

static struct nvram_tuple *bsd_default_policy;

void bsd_default_nvram_config(bsd_info_t *info)
{
	int idx;

	switch (info->boardtype) {
		case BCM94706NR:
		case BCM94706NRH:
		case BCM94706NR2HMC:
			BSD_INFO("set default to bsd_5g2g_policy\n");
			bsd_default_policy = bsd_5g2g_policy;
			break;
		case BCM94709ACDCRH:
			BSD_INFO("set default to bsd_5glo_2g_5ghi_policy\n");
			bsd_default_policy = bsd_5glo_2g_5ghi_policy;
			break;
		case BCM94708R:
		default:
			BSD_INFO("set default to bsd_2g5g_policy\n");
			bsd_default_policy = bsd_2g5g_policy;
			break;
	}

	for (idx = 0; bsd_default_policy[idx].name != NULL; idx++) {
		BSD_INFO("nvram_set name=%s value=%s\n",
			bsd_default_policy[idx].name, bsd_default_policy[idx].value);
		nvram_set(bsd_default_policy[idx].name, bsd_default_policy[idx].value);
	}
}
