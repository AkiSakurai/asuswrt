/*
 * MAC Address Randomization implementation for Broadcom 802.11 Networking Driver
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
 * $Id$
 */
#ifndef _randmacpvt_h_
#define _randmacpvt_h_

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <osl.h>
#include <sbchipc.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <bcmparams.h>
#include <bcm_notif_pub.h>
#include <802.11.h>
#include <wlioctl.h>
#include <wlc_iocv.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_bmac.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <hndpmu.h>
#include <wlc_pcb.h>
#include <wlc_assoc.h>
#include <wlc_scan.h>
#ifdef WL_PROXDETECT
#include <wlc_ftm.h>
#endif /* WL_PROXDETECT */
#include <wlc_randmac.h>

#include <randmac.h>

#define RANDMAC_NAME "randmac"

typedef struct wlc_randmac_bsscfg {
	wlc_bsscfg_t *cfg;			/* Pointer to bsscfg for easy ref */
	struct ether_addr random_addr;		/* Randomized MAC address */
	struct ether_addr bsscfg_addr;		/* Original MAC address for this bsscfg */
	struct ether_addr intf_cfg_addr;	/* Original primary interface MAC address */
	uint8 refcnt;				/* Number of users of this bsscfg */
	uint8 bsscfg_idx;			/* bsscfg index */
	wl_randmac_event_mask_t events;		/* Randmac events mask */
} wlc_randmac_bsscfg_t;

typedef struct wlc_randmac_config {
	struct ether_addr rand_addr;		/* Configured randomized MAC address */
	struct ether_addr rand_mask;		/* Bits in MAC address to randomize */
	uint16 method;				/* Methods using random MAC address */
	uint8 enable;				/* randmac is enabled/disabled */
	uint8 unused_padding_1;			/* Unused byte */
} wlc_randmac_config_t;

typedef struct wlc_randmac_stats {
	uint32 set_success;		/* Set operations successful */
	uint32 set_fail;		/* Set operation failures */
	uint32 set_reqs;		/* Set operation requests */
	uint32 reset_reqs;		/* Restore operation requests */
	uint32 restore_success;		/* Restore operations successful */
	uint32 restore_fail;		/* Restore operations failed */
	uint32 events_sent;		/* Randmac method events generated */
	uint32 events_rcvd;		/* Randmac received events from other methods */
} wlc_randmac_stats_t;

#define RANDMAC_DISABLED	0
#define RANDMAC_ENABLED		1

/* This is the mainstructure of randmac module */
struct wlc_randmac_info {
	uint32 signature;
	wlc_info_t *wlc;
	wlc_randmac_bsscfg_t **randmac_bsscfg;
	wlc_randmac_config_t *config;
	wlc_randmac_stats_t stats;
	uint32 flags;
	int8 max_cfg;
	bool is_randmac_config_updated;
	bcm_notif_h h_notif;
};

/* IOVAR declarations */
enum {
	/*
	 IOV: IOV_RANDMAC
	 Purpose: This IOVAR enables/disables randmac.
	*/
	IOV_RANDMAC		= 0,
	IOV_RANDMAC_LAST
};

/* Iovars */
static const bcm_iovar_t  wlc_randmac_iovars[] = {
	{"randmac", IOV_RANDMAC, 0, 0, IOVT_BUFFER, 0},

	{NULL, 0, 0, 0, 0, 0}
};
#endif /* _randmacpvt_h_ */
