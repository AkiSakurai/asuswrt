/*
 * RTE DONGLE API external definitions
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
 * $Id: dngl_api.h 774418 2019-04-24 07:52:25Z $
 */

#ifndef _dngl_api_h_
#define _dngl_api_h_

#include <dngl_stats.h>
#include <osl.h>

#define DNGL_MEDIUM_UNKNOWN	0		/* medium is non-wireless (802.3) */
#define DNGL_MEDIUM_WIRELESS	1		/* medium is wireless (802.11) */

struct dngl_bus;
struct dngl;

#include <rte_timer.h>
typedef hnd_timer_t dngl_timer_t;
typedef hnd_task_t dngl_task_t;
#define dngl_init_timer(context, data, fn) hnd_timer_create(context, data, fn, NULL, NULL)
#define dngl_free_timer(t) hnd_timer_free(t)
#define dngl_add_timer(t, ms, periodic) hnd_timer_start(t, ms, periodic)
#define dngl_del_timer(t) hnd_timer_stop(t)
#include <rte.h>
#define dngl_schedule_work(context, data, fn, delay) hnd_schedule_work(context, data, fn, delay)
#ifdef BCMDBG_SD_LATENCY
#define dngl_time_now_us() hnd_time_us()
#endif /* BCMDBG_SD_LATENCY */
extern void _dngl_reboot(dngl_task_t *task);

extern void dngl_keepalive(struct dngl *dngl, uint32 sec);

/* DONGLE OS operations */
extern struct dngl *dngl_attach(struct dngl_bus *bus, void *drv, si_t *sih, osl_t *osh);
extern void dngl_detach(struct dngl *dngl);
/** Forwards transmit packets to the wireless subsystem */
extern void dngl_sendwl(struct dngl *dngl, void *p);
extern void dngl_sendup(struct dngl *dngl, void *p);
extern void dngl_ctrldispatch(struct dngl *dngl, void *p, uchar *buf);
extern void dngl_txstop(struct dngl *dngl);
extern void dngl_txstart(struct dngl *dngl);
extern void dngl_suspend(struct dngl *dngl);
extern void dngl_resume(struct dngl *dngl);
extern void dngl_init(struct dngl *dngl);
extern void dngl_halt(struct dngl *dngl);
extern void dngl_reset(struct dngl *dngl);
extern int dngl_binddev(struct dngl *dngl, void *bus, void *dev, uint numslaves);
extern void dngl_rebinddev(struct dngl *dngl, void *bus, void *new_dev, int ifindex);
extern int dngl_unbinddev(struct dngl *dngl, void *bus, void *dev);
extern int dngl_validatedev(struct dngl *dngl, void *bus, void *dev);
extern int dngl_opendev(struct dngl *dngl);
extern int dngl_findif(struct dngl *dngl, void *dev);
extern int dngl_rebind_if(struct dngl *dngl, void *dev, int idx, bool rebind);
extern void dngl_rxflowcontrol(struct dngl *dngl, bool state, int prio);
extern int dngl_sendpkt(struct dngl *dngl, void *src, void *p);
extern bool dngl_maxdevs_reached(struct dngl *dngl);

extern int dngl_sendctl(struct dngl *dngl, void *src, void *p);

extern int _dngl_devioctl(struct dngl *dngl, int ifindex,
	uint32 cmd, void *buf, int len, int *used, int *needed, bool set);
extern bool dngl_get_netif_stats(struct dngl *dngl, dngl_stats_t *stats);
extern ulong dngl_get_netif_mtu(struct dngl *dngl);
extern void dngl_get_stats(struct dngl *dngl, dngl_stats_t *stats);
extern int dngl_flowring_update(struct dngl *dngl, uint8 ifindex, uint16 flowid,
	uint8 op, uint8 * sa, uint8 *da, uint8 tid, uint8* mode);
#ifdef WLCFP
extern int dngl_bus_cfp_link(struct dngl *dngl, uint8 ifindex, uint16 ringid, uint8 tid,
	uint8* da, uint8 op, uint8** tcb_state, uint16* cfp_flowid);
#endif // endif
#ifdef FLASH_UPGRADE
extern int dngl_upgrade(struct dngl *dngl, uchar *buf, uint len);
extern int dngl_upgrade_status(struct dngl *dngl);
#endif // endif

/* Simple ioctl() call */
#define dngl_dev_ioctl(dngl, cmd, buf, len) \
	_dngl_devioctl((dngl), 0, (cmd), (buf), (len), NULL, NULL, TRUE)

/* Relay OID request through ioctl() */
#define dngl_dev_query_oid(dngl, ifindex, cmd, buf, len, written, needed) \
	_dngl_devioctl((dngl), (ifindex), (cmd), (buf), (len), (written), (needed), FALSE)
#define dngl_dev_set_oid(dngl, ifindex, cmd, buf, len, read, needed) \
	_dngl_devioctl((dngl), (ifindex), (cmd), (buf), (len), (read), (needed), TRUE)
extern int dngl_max_slave_devs(struct dngl *dngl);

#if defined(BCMUSBDEV) && defined(BCMCDC)
void dngl_pr46794WAR(struct dngl *dngl);
#endif // endif

#endif /* _dngl_api_h_ */
