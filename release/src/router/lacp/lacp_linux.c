/*
 * Broadcom LACP driver for linux
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */

#include <typedefs.h>
#include <osl.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <bcmutils.h>
#include <linux/completion.h>
#include <proto/ethernet.h>
#include <proto/bcmlacp.h>
#include <bcmendian.h>
#include <etioctl.h>
#include "lacp_linux.h"
#include "lacpc.h"
#include "lacp_debug.h"

#undef DEBUG_AGGCB

#define LACP_TSK_INTERVAL		1000	/* in millisecond */

typedef struct lacp_info {
	osl_t *osh;			/* OS layer handler */
	void *lacpc;			/* lacp commont info */
	struct net_device *dev;		/* netdev that can TX/RX LACP packets */
	et_agg_ctl_t agg_ctl;		/* agg controller */
} lacp_info_t;

/* LACP multicast address */
static const struct ether_addr ether_lacpdu_mcast = {{0x01, 0x80, 0xC2, 0x00, 0x00, 0x02}};

static lacp_info_t *g_lacp = NULL;

static int lacp_netdev_event(struct notifier_block *this, unsigned long event, void *ptr);

static struct notifier_block lacp_netdev_notifier = {
	.notifier_call = lacp_netdev_event
};

static int
lacp_tsk_thread(void *data)
{
	lacp_tsk_ctl_t *tsk_ctl = (lacp_tsk_ctl_t *) data;

	ASSERT(tsk_ctl);

	while (1) {
		/* thread terminated */
		if (tsk_ctl->terminated == TRUE)
			break;

		if (tsk_ctl->looper)
			tsk_ctl->looper(tsk_ctl->data);

		OSL_SLEEP(LACP_TSK_INTERVAL);
	}
	complete_and_exit(&(tsk_ctl->completed), 0);
}

lacp_tsk_ctl_t*
lacp_tsk_start(osl_t *osh, looper_t looper_func, void *param, char *name)
{
	lacp_tsk_ctl_t *tsk_ctl;

	if (!(tsk_ctl = (lacp_tsk_ctl_t *)MALLOCZ(osh, sizeof(lacp_tsk_ctl_t)))) {
		LACP_ERROR("out of memory, malloced %d bytes\n",
			MALLOCED(osh));
		return tsk_ctl;
	}

	tsk_ctl->osh = osh;
	init_completion(&((tsk_ctl)->completed));
	mutex_init(&(tsk_ctl)->lock);
	(tsk_ctl)->looper = looper_func;
	(tsk_ctl)->data = param;
	(tsk_ctl)->terminated = FALSE;
	tsk_ctl->lacp_kthread_tsk = kthread_run(lacp_tsk_thread, tsk_ctl, name);

	return tsk_ctl;
}

void
lacp_tsk_stop(lacp_tsk_ctl_t *tsk_ctl)
{
	if (tsk_ctl) {
		(tsk_ctl)->terminated = TRUE;
		wait_for_completion(&((tsk_ctl)->completed));
		kthread_stop(tsk_ctl->lacp_kthread_tsk);
		MFREE(tsk_ctl->osh, tsk_ctl, sizeof(lacp_tsk_ctl_t));
	}
}

void
lacp_tsk_lock(lacp_tsk_ctl_t *tsk_ctl)
{
	if (tsk_ctl)
		mutex_lock(&tsk_ctl->lock);
}

void
lacp_tsk_unlock(lacp_tsk_ctl_t *tsk_ctl)
{
	if (tsk_ctl)
		mutex_unlock(&tsk_ctl->lock);
}

int32
lacp_send(void *lacpi, void *pkt, int32 pkt_len)
{
	lacp_info_t *lacp = (lacp_info_t *)lacpi;
	struct sk_buff *skb;
	struct ether_header *eh;

	if (!lacp) {
	        LACP_ERROR("lacp is NULL\n");
	        return BCME_ERROR;
	}

	skb = dev_alloc_skb(pkt_len + ETHER_HDR_LEN);
	if (!skb)
		return BCME_NOMEM;

	skb->dev = lacp->dev;
	skb_reset_mac_header(skb);
	skb->network_header = skb->mac_header + ETHER_HDR_LEN;
	skb->protocol = HTON16(ETHER_TYPE_SLOW);

	/* fill ethernet header */
	eh = (struct ether_header *)skb_put(skb, sizeof(lacpdu_t) + ETHER_HDR_LEN);
	bcopy(skb->dev->dev_addr, eh->ether_shost, ETHER_ADDR_LEN);
	bcopy(&ether_lacpdu_mcast, eh->ether_dhost, ETHER_ADDR_LEN);
	eh->ether_type = HTON16(ETHER_TYPE_SLOW);

	/* fill lacp pkt content */
	bcopy(pkt, skb->data + ETHER_HDR_LEN, pkt_len);

	dev_queue_xmit(skb);

	return BCME_OK;
}

int32
lacp_get_linksts(void *lacpi, uint32 *linksts)
{
	lacp_info_t *lacp = (lacp_info_t *)lacpi;
	char outbuf[8];
	uint32 *out;
	int32 ret = BCME_ERROR;

	if (!lacp) {
		LACP_ERROR("lacp is NULL\n");
		goto err;
	}

	if (lacp->agg_ctl.fn) {
		out = (uint32 *)outbuf;
		out[0] = 1;

		ASSERT((out[0] * sizeof(*out)) <= sizeof(outbuf));

		ret = lacp->agg_ctl.fn(lacp->agg_ctl.dev, AGG_GET_LINKSTS, NULL, outbuf);
		if (ret != BCME_OK)
			goto err;

		*linksts = out[1];
		LACP_LINUX(("linksts 0x%x\n", *linksts));
	}

err:
	return ret;
}

int32
lacp_get_portsts(void *lacpi, int8 portid, uint32 *link, uint32 *speed,
	uint32 *duplex)
{
	lacp_info_t *lacp = (lacp_info_t *)lacpi;
	char inbuf[8], outbuf[16];
	uint32 *in, *out;
	int32 ret = BCME_ERROR;

	if (!lacp) {
		LACP_ERROR("lacp is NULL\n");
		goto err;
	}

	if (lacp->agg_ctl.fn) {
		in = (uint32 *)inbuf;
		in[0] = 1;
		in[1] = portid;

		out = (uint32 *)outbuf;
		out[0] = 3;

		ASSERT((out[0] * sizeof(*out)) <= sizeof(outbuf));
		ASSERT((in[0] * sizeof(*in)) <= sizeof(inbuf));

		ret = lacp->agg_ctl.fn(lacp->agg_ctl.dev, AGG_GET_PORTSTS, inbuf, outbuf);
		if (ret != BCME_OK)
			goto err;

		*link = out[1];
		*speed = out[2];
		*duplex = out[3];
		LACP_LINUX(("pid %d link 0x%x speed 0x%x duplex 0x%x\n",
			portid, *link, *speed, *duplex));
	}

err:
	return ret;
}

int32
lacp_update_agg(void *lacpi, int8 group, uint32 portmap)
{
	lacp_info_t *lacp = (lacp_info_t *)lacpi;
	char inbuf[12];
	uint32 *in;
	int32 ret = BCME_ERROR;

	if (!lacp) {
	        LACP_ERROR("lacp is NULL\n");
	        goto err;
	}

	if (lacp->agg_ctl.fn) {
		in = (uint32 *)inbuf;
		in[0] = 2;
		in[1] = group;
		in[2] = portmap;

		ASSERT((in[0] * sizeof(*in)) <= sizeof(inbuf));

		ret = lacp->agg_ctl.fn(lacp->agg_ctl.dev, AGG_SET_GRP, inbuf, NULL);
	}

err:
	return ret;
}

int32
lacp_set_pid_report(void *lacpi, int8 on)
{
	lacp_info_t *lacp = (lacp_info_t *)lacpi;
	char inbuf[8];
	uint32 *in;
	int32 ret = BCME_ERROR;

	if (!lacp) {
		LACP_ERROR("lacp is NULL\n");
		goto err;
	}

	if (lacp->agg_ctl.fn) {
		in = (uint32 *)inbuf;
		in[0] = 1;
		in[1] = on;

		ASSERT((in[0] * sizeof(*in)) <= sizeof(inbuf));

		ret = lacp->agg_ctl.fn(lacp->agg_ctl.dev, AGG_SET_BHDR, inbuf, NULL);
	}

err:
	return ret;
}

#ifdef DEBUG_AGG
void static
lacp_agg_test(lacp_info_t *lacp)
{
	uint32 linksts, link, speed, duplex;
	int port;

	if (!lacp) {
		LACP_ERROR("lacp is NULL\n");
		return;
	}

	/* update agg test */
	if (lacp_update_agg((void *)lacp, 0, 0x1) != BCME_OK) /* grp 0 map 1 (error case ) */
		LACP_MSG("failed to set grp 0 map 0x1\n");
	if (lacp_update_agg((void *)lacp, 1, 0x2) != BCME_OK) /* grp 1 map 2 */
		LACP_MSG("failed to set grp 1 map 0x2\n");
	if (lacp_update_agg((void *)lacp, 2, 0x3) != BCME_OK) /* grp 2 map 3 */
		LACP_MSG("failed to set grp 2 map 0x3\n");

	/* get link status test */
	lacp_get_linksts((void *)lacp, &linksts);
	LACP_MSG("linksts = 0x%x\n", linksts);

	/* get port status test */
	for (port = 0; port < (LACPC_MAX_PORT + 1); port++) {
		lacp_get_portsts((void *)lacp, port, &link, &speed, &duplex);
		LACP_MSG("port %d status: link 0x%x speed 0x%x duplex 0x%x\n",
			port, link, speed, duplex);
	}

	/* turn on/off pid reported by agg */
	if (lacp_set_pid_report(lacp, LACPC_ON) != BCME_OK)
		LACP_MSG("failed to turn on bhdr\n");
	if (lacp_set_pid_report(lacp, LACPC_OFF) != BCME_OK)
		LACP_MSG("failed to turn off bhdr\n");
}
#endif /* DEBUG_AGGCB */

int32
lacp_get_hostmac(void *lacpi, uint8 *hostmac)
{
	lacp_info_t *lacp = (lacp_info_t *)lacpi;

	if (!lacp || !hostmac) {
	        LACP_ERROR("input NULL pointer, lacp %p hostmac %p\n", lacp, hostmac);
	        return BCME_ERROR;
	}

	if (!lacp->dev) {
		LACP_ERROR("lacp dev is NULL\n");
		return BCME_ERROR;
	}

	bcopy(lacp->dev->dev_addr, hostmac, ETHER_ADDR_LEN);

	return BCME_OK;
}

static int
lacp_rcv(struct sk_buff *skb, struct net_device *dev,
	struct packet_type *pt, struct net_device *orig_dev)
{
	lacp_info_t *lacp = g_lacp;

	if (!lacp) {
		LACP_ERROR("lacp is NULL\n");
		return BCME_ERROR;
	}

	lacpc_rcv(lacp->lacpc, PKTDATA(NULL, skb), PKTLEN(NULL, skb));

	kfree_skb(skb);

	return NET_RX_SUCCESS;
}

static struct packet_type lacp_packet_type __read_mostly = {
	.type =	HTON16(ETHER_TYPE_SLOW),
	.func =	lacp_rcv,
};

static void
lacp_start(struct net_device *dev, int8 on)
{
	lacp_info_t *lacp = g_lacp;
	lacpc_osl_fn_t func;

	if (!lacp) {
		LACP_ERROR("g_lacp is NULL");
		return;
	}

	if (lacp->lacpc)
		return;

	if (on == LACPC_ON)
		lacp_set_pid_report(lacp, LACPC_ON);

	func.send_fn = lacp_send;
	func.update_agg_fn = lacp_update_agg;
	func.get_portsts_fn = lacp_get_portsts;
	func.get_linksts_fn = lacp_get_linksts;
	func.get_hostmac_fn = lacp_get_hostmac;

	lacp->dev = dev;
	lacp->lacpc = lacpc_init(lacp, lacp->osh, &func, on);

	dev_add_pack(&lacp_packet_type);

#ifdef DEBUG_AGG
	lacp_agg_test(lacp);
#endif /* DEBUG_AGG */

	return;
}

static void
lacp_stop(void)
{
	lacp_info_t *lacp = g_lacp;

	if (!lacp) {
		LACP_ERROR("g_lacp is NULL");
		return;
	}

	if (!lacp->lacpc)
		return;

	lacpc_deinit(lacp->lacpc);
	lacp->lacpc = NULL;
	dev_remove_pack(&lacp_packet_type);
}

static int
lacp_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	lacp_info_t *lacp = g_lacp;
	struct net_device *dev;
	const char *name;
	int8 on;

	if (!lacp || !ptr)
		return NOTIFY_DONE;

	name = getvar(NULL, "lacpdev");
	if (!name)
		return NOTIFY_DONE;

	dev = (struct net_device *)ptr;
	if (strcmp(dev->name, name))
		return NOTIFY_DONE;

	on = getintvar(NULL, "lacp");

	switch (event) {
	case NETDEV_REGISTER:
	case NETDEV_UP:
		if (on == LACPC_OFF)
			break;
		lacp_start(dev, on);
		break;

	case NETDEV_UNREGISTER:
	case NETDEV_DOWN:
		lacp_set_pid_report(lacp, LACPC_OFF);
		lacp_stop();
		break;
	}

	return NOTIFY_DONE;
}

static int32 __init
lacp_init(void)
{
	lacp_info_t *lacp;
	osl_t *osh;
	struct net_device *dev;
	uint8 name[IFNAMSIZ] = "agg";

	ASSERT(!g_lacp);

	osh = osl_attach(NULL, PCI_BUS, FALSE);
	ASSERT(osh);

	lacp = MALLOC(osh, sizeof(lacp_info_t));
	if (!lacp) {
		osl_detach(osh);
		LACP_ERROR("out of memory\n");
		return BCME_NOMEM;
	}
	bzero(lacp, sizeof(lacp_info_t));

	lacp->osh = osh;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36))
	dev = dev_get_by_name(&init_net, name);
#else
	dev = dev_get_by_name(name);
#endif
	if (!dev) {
		MFREE(osh, lacp, sizeof(lacp_info_t));
		osl_detach(osh);
		LACP_ERROR("cannot find agg dev\n");
		return BCME_ERROR;
	}

	bcopy(DEV_PRIV(dev), &lacp->agg_ctl, sizeof(et_agg_ctl_t));

	dev_put(dev);

	g_lacp = lacp;

	register_netdevice_notifier(&lacp_netdev_notifier);

	return BCME_OK;
}
module_init(lacp_init);

static void __exit
lacp_deinit(void)
{
	lacp_info_t *lacp = g_lacp;
	osl_t *osh;

	if (!lacp) {
		LACP_ERROR("g_lacp is NULL");
		return;
	}

	lacp_stop();

	osh = lacp->osh;
	MFREE(osh, lacp, sizeof(lacp_info_t));

	if (MALLOCED(osh))
		LACP_ERROR("Memory leak of bytes %d\n", MALLOCED(osh));

	osl_detach(osh);

	unregister_netdevice_notifier(&lacp_netdev_notifier);
}
module_exit(lacp_deinit);
