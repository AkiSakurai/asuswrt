/*
 * RTE dongle private definitions
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dngl_rte.h 445853 2013-12-31 12:18:31Z $
 */

#ifndef	_dngl_rte_h_
#define	_dngl_rte_h_

#include <bcmcdc.h>

#ifndef MAXVSLAVEDEVS
#define MAXVSLAVEDEVS	0
#endif

#if MAXVSLAVEDEVS > BDC_FLAG2_IF_MASK
#error "MAXVSLAVEDEVS is too big"
#endif

typedef struct dngl {
	void *bus;			/* generic bus handle */
	osl_t *osh;			/* OS abstraction layer handler */
	si_t *sih;			/* sb/si backplane handler */
	void *proto;			/* BDC/RNDIS proto handler */
	dngl_stats_t stats;
	int medium;
	hndrte_dev_t *rtedev;
	hndrte_dev_t *slave;
	hndrte_dev_t **vslave;	/* virtual slave device handles for AP interfaces
							* [Note: max is  BDC_FLAG2_IF_MASK ]
							*/
#ifdef BCMET
	et_cb_t cb;			/* Link event handler */
#endif
	uint8 unit;			/* Device index */
	bool up;
	bool devopen;
#ifdef FLASH_UPGRADE
	int upgrade_status;		/* Upgrade return status code */
#endif
	int tunable_max_slave_devs;
#ifdef BCM_FD_AGGR
	void *rpc_th;           /* handle for the bcm rpc tp module */
	dngl_timer_t *rpctimer;     /* Timer for toggling gpio output */
	uint16 rpctime;     /* max time  to push the aggregation */
	bool rpctimer_active;   /* TRUE = rpc timer is running */
    bool fdaggr;    /* 1 = aggregation enabled */
#endif
	uint16	data_seq_no;
	uint16  ioctl_seq_no;
	uint16	data_seq_no_prev;
	uint16  ioctl_seq_no_prev;

} dngl_t;

#endif	/* _dngl_rte_h_ */
