/** @file dngle_rte.c
 *
 * RTE dongle core support file
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
 * $Id: dngl_rte.c 774418 2019-04-24 07:52:25Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <ethernet.h>
#include <trxhdr.h>
#include <bcmnvram.h>

#include <osl.h>
#include <wlioctl.h>
#include <bcmendian.h>
#include <epivers.h>
#include <sflash.h>
#include <sbchipc.h>
#include <dngl_stats.h>
#include <hndsoc.h>
#ifdef BCMET
#include <etioctl.h>
#endif // endif

#include <dngl_bus.h>
#include <dngl_api.h>
#include <dngl_protocol.h>
#include <dngl_dbg.h>
#include "dngl_rte.h"
#include <rte_cons.h>
#include <rte.h>
#include <rte_ioctl.h>

static uint8 found = 0;

/* this must be same as WL_RTE_HIDDEN_IF */
#define DNGL_RTE_HIDDEN_IF (1 << 31)

int dngl_msglevel = DNGL_ERROR;

#ifdef RTE_CONS
#ifdef BCMDBG
static void dngl_dump_busregs(void *arg, int argc, char *argv[]);
static void dngl_bus_msgbits(void *arg, int argc, char *argv[]);
#endif // endif
#endif /* RTE_CONS */

#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFF
#endif /* UINT32_MAX */

static hnd_dev_t*dngl_get_slave(struct dngl *dngl, int ifindex);
static int dngl_add_cons_cmds(struct dngl *dngl);

/** Find the device from the ifindex */
static hnd_dev_t *
dngl_finddev(struct dngl *dngl, int ifindex)
{
	hnd_dev_t *slave;
	if (dngl_get_slave(dngl, ifindex) != NULL) {
		slave = dngl->slaves[dngl->iface2slave_map[ifindex]];
	} else {
		err("slave %d not found", ifindex);
		slave = NULL;
	}

	return slave;
}

/* Find the device iface idx */
int
dngl_findif(struct dngl *dngl, void *dev)
{
	int ifindex;
	ASSERT(dev);
	for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
		if (dngl_get_slave(dngl, ifindex) == dev) {
			break;
		}
	}
	if (ifindex >= dngl->tunable_max_slave_devs) {
		err("vslave %p not found ", dev);
		return -1;
	} else {
		return ifindex;
	}
}

int
dngl_rebind_if(struct dngl *dngl, void *dev, int idx, bool rebind)
{
	int devindex;
	/* devindex is now idx of dev in dngl->iface2slave_map[] */
	devindex = dngl_findif(dngl, dev);
	ASSERT(devindex != -1);

	trace("ifindex %d, os_idx %d", devindex, idx);

	/* os idx for dev is ifindex */
	/* if rebind is true just swap the dev idx with idx provided */
	if (rebind) {
		if (devindex != idx) {
			int tmp = dngl->iface2slave_map[idx];
			dngl->iface2slave_map[idx] =
				dngl->iface2slave_map[devindex];
			dngl->iface2slave_map[devindex] = tmp;
		}
	} else {
		if (dngl->iface2osif[devindex] != -1) {
			dngl->iface2osif[devindex] = idx;
		} else {
			err("iface2osif is not initialised");
			return -1;
		}
	}
	return 0;
}

/* Call Broadcom wireless driver private ioctl */
int
_dngl_devioctl(struct dngl *dngl, int ifindex,
	uint32 cmd, void *buf, int len, int *used, int *needed, bool set)
{
	int ret = -1;
	hnd_dev_t *slave;

	/* buf must be 4-byte aligned */
	ASSERT(ISALIGNED(buf, 4));

	trace("ioctl %s cmd 0x%x, len %d", set ? "SET" : "QUERY", cmd, len);

	if (cmd == WLC_SET_VAR || cmd == WLC_GET_VAR) {
		if (!strncmp((char *)buf, "memuse", 6)) {
			ret = hnd_get_heapuse((memuse_info_t *)buf);
			return ret;
		}
	}

	if ((slave = dngl_finddev(dngl, ifindex)) != NULL)
		ret = slave->ops->ioctl(slave, cmd, buf, len, used, needed, set);
	trace("status = %d/0x%x", ret, ret);

	return ret;
}

struct dngl *
BCMATTACHFN(dngl_attach)(struct dngl_bus *bus, void *drv, si_t *sih, osl_t *osh)
{
	struct dngl *dngl = NULL;

	trace("called");

	if (found >= 8) {
		err("too many units");
		goto fail;
	}

	if (!(dngl = MALLOC(osh, sizeof(dngl_t)))) {
		err("MALLOC failed");
		goto fail;
	}
	memset(dngl, 0, sizeof(dngl_t));
	dngl->bus = bus;
	dngl->osh = osh;
	dngl->sih = sih;
	dngl->unit = found;
	dngl->data_seq_no = 0;
	dngl->data_seq_no_prev = 0;
	dngl->ioctl_seq_no = 0;
	dngl->ioctl_seq_no_prev = 0;

	found++;

	dngl->tunable_max_slave_devs = MAXVSLAVEDEVS + si_numd11coreunits(sih);

#if defined(WLRSDB) && !defined(WLRSDB_DISABLED)
	/* Increment slave for accomodating new bsscfg
	 * during secondary bsscfg MOVE.
	 */
	dngl->tunable_max_slave_devs += WLRSDB_CLONE_ADJUST_SLAVES;
	/* Account for the hidden devs in case of RSDB required
	 * for connections like nan
	 */
	dngl->tunable_max_slave_devs += MAXVSLAVEDEVS;
#endif // endif
	/* 1 (real_slave) + MAXVSLAVEDEVS -1 (for zero based index) */
	dngl->tunable_max_host_if = 1 + MAXVSLAVEDEVS - 1;

	if (!(dngl->slaves = (hnd_dev_t **)MALLOCZ(osh,
	dngl->tunable_max_slave_devs * sizeof(hnd_dev_t *)))) {
		err("slave list MALLOC failed");
		goto fail;
	}

	memset(dngl->slaves, 0, dngl->tunable_max_slave_devs * sizeof(hnd_dev_t *));

	if (!(dngl->iface2slave_map =
	      (int *)MALLOCZ(osh, dngl->tunable_max_slave_devs * sizeof(int)))) {
		err("interface-2-slave map MALLOC failed");
		goto fail;
	}

	memset(dngl->iface2slave_map, -1, dngl->tunable_max_slave_devs * sizeof(int));
	dngl->num_activeslave_devs = 0;

	if (!(dngl->iface2osif =
	      (int *)MALLOCZ(osh, dngl->tunable_max_slave_devs * sizeof(int)))) {
		err("interface-2-osif map MALLOC failed");
		goto fail;
	}

	memset(dngl->iface2osif, -1, dngl->tunable_max_slave_devs * sizeof(int));

	return dngl;

fail:
	if (dngl)
		if (MFREE(NULL, dngl, sizeof(dngl_t)))
			err("MFREE error");

	return NULL;
} /* dngl_attach */

void
BCMATTACHFN(dngl_detach)(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);

	/* unbind from enslaved device */
	if (dngl->primary_slave) {
#ifdef BCMET
		int pcb;
		uint32 cmd;
		cmd = OID_ET_UNSETCALLBACK;
		pcb = (int) &dngl->cb;
		if (dngl_dev_ioctl(dngl, cmd, &pcb, sizeof(pcb)) < 0)
			err("%s: UNSET CALLBACK failed", dngl->rtedev->name);
#endif // endif
		dngl->primary_slave->chained = NULL;
		dngl->slaves[0] = NULL;
		dngl->iface2slave_map[0] = -1;
		dngl->num_activeslave_devs = 0;
		dngl->rtedev->chained = NULL;
	}

	proto_ops->proto_detach_fn(dngl->proto);
	if (dngl->iface2osif) {
		MFREE(dngl->osh, dngl->iface2osif,
			dngl->tunable_max_slave_devs * sizeof(int));
		dngl->iface2osif = NULL;
	}
}

/** Net interface running faster than USB. Flow-control the net interface */
void
dngl_txstop(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);
	/* pause network device indications */
	if (dngl->primary_slave->ops->txflowcontrol)
		dngl->primary_slave->ops->txflowcontrol(dngl->primary_slave, TRUE, 0);
}

void
dngl_txstart(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);
	/* resume network device indications */
	if (dngl->primary_slave->ops->txflowcontrol)
		dngl->primary_slave->ops->txflowcontrol(dngl->primary_slave, FALSE, 0);
}

/** flow control called by dev receiving packets from us */
void
dngl_rxflowcontrol(struct dngl *dngl, bool state, int prio)
{
	trace("%s flowctl %s", dngl->rtedev->name, state == ON ? "on" : "off");
	bus_ops->rxflowcontrol(dngl->bus, state, prio);
}

/** Set the ifindex in the BDC header */
static int
dngl_setifindex(struct dngl *dngl, void *src, void *p)
{
	int ifindex = 0;
	bool l_found = FALSE;
	int i = 0;

	/* save IF index in the bdc header */
	/* IF 0: primary enslaved device pointed by dngl->slave.
	 * IF 1 and up: virtual device by dngl->vslave[IF-1].
	 */
	/* Assume the bdc header has been initialized */

	/* First, look for real slaves.
	 * Real slaves occupy top portion of slaves array.
	 * Real slaves map to primary interface i.e, IF 0.
	 */
	for (i = 0; i < dngl->num_realslave_devs; i++) {
		if (dngl->slaves[i] == src) {
			l_found = TRUE;
			break;
		}
	}

	if (!l_found && src != NULL) {
		for (ifindex = 1; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
			if (dngl_get_slave(dngl, ifindex) == src)
				break;
		}
		if (ifindex >= dngl->tunable_max_slave_devs) {
			err("vslave %p not found, dropped pkt", src);
			return -1;
		}
		if (dngl->iface2osif[ifindex] != -1) {
			ifindex = dngl->iface2osif[ifindex];
		}
#ifndef BCMMSGBUF
		{
			struct bdc_header *bdc;

			bdc = (struct bdc_header *)PKTDATA(dngl->osh, p);
			bdc->flags2 |= (uint8)(ifindex);
		}
#else
		{
			/* bdc header not valid for pcie full dongle */
			/* In rx path, when non splitrx mode is enabled */
			/* ifidx field in lbuf is used */
			PKTSETIFINDEX(dngl->osh, p, ifindex);
		}
#endif /* BCMMSGBUF */

	}

	return ifindex;
} /* dngl_setifindex */

/** Forwards a stack packet towards the host */
int
dngl_sendpkt(struct dngl *dngl, void *src, void *p)
{
	trace("%s", dngl->rtedev->name);
	uint32 pktlen;
	int ret;

	p = PKTFRMNATIVE(dngl->osh, p);

	/* Push protocol-level header */
	if ((p = proto_ops->proto_pkt_header_push_fn(dngl->proto, p)) &&
	    dngl_setifindex(dngl, src, p) >= 0) {
		{
		/* for pcie full dongle, pkt is freed inside bus_ops->tx
		* for wl events. So keep backup ok totlen before calling bus->tx
		* use pkttotlen() since p could be a chained pkt
		*/
		pktlen = pkttotlen(dngl->osh, p);
			/* calls e.g. pciedev_create_d2h_messages_tx() */
			ret = bus_ops->tx(dngl->bus, p);
			if (ret) {
				dngl->stats.tx_bytes += pktlen;
				dngl->stats.tx_packets++;
			} else {
				dngl->stats.tx_dropped++;
				err("dropped pkt");
			}
		}
	}

	return 0;
} /* dngl_sendpkt */

/** Transmit a stack packet onto the bus */
int
dngl_sendctl(struct dngl *dngl, void *src, void *p)
{
	trace("%s", dngl->rtedev->name);

	p = PKTFRMNATIVE(dngl->osh, p);

	/* Push protocol-level header */
	if ((p = (proto_ops->proto_pkt_header_push_fn)(dngl->proto, p)) &&
		dngl_setifindex(dngl, src, p) >= 0) {
		bus_ops->sendctl(dngl->bus, p);
	}

	return 0;
}

/**
 * Send a packet received from the bus to the enslaved device or queue
 * it for later transmission to the enslaved device
 */
#ifdef PKTC_TX_DONGLE
void
dngl_sendwl(struct dngl *dngl, void *p)
{
#ifndef DNGL_LB
	struct lbuf *lb;
	hnd_dev_t *slave = NULL;
	int p_ifindex = 0;
#ifndef BCMMSGBUF
	int n_ifindex = 0;
	struct bdc_header *p_bdc;
#endif // endif
	int p_drop;
	void *head = NULL, *tail = NULL, *n;

	trace("%s: pkt len %d", dngl->rtedev->name, PKTLEN(dngl->osh, p));

	while (p != NULL) {
		n = PKTLINK(p);
#ifndef BCMMSGBUF
		/* Save the BDC header pointer but only use it after the validation
		 * is done in proto_pkt_header_pull().
		 */
		p_bdc = (struct bdc_header *)PKTDATA(dngl->osh, p);
		p_ifindex = p_bdc->flags2 & BDC_FLAG2_IF_MASK;

		if (n != NULL) {
			struct bdc_header *n_bdc;

			n_bdc = (struct bdc_header *)PKTDATA(dngl->osh, n);
			n_ifindex = n_bdc->flags2 & BDC_FLAG2_IF_MASK;
		}
#else
		p_ifindex = PKTIFINDEX(dngl->osh, p);
#endif /* BCMMSGBUF */

		/* Pull protocol-level header */
		p_drop = proto_ops->proto_pkt_header_pull_fn(dngl->proto, p);

		if (!p_drop) {
			if (!tail) {
				slave = dngl_finddev(dngl, p_ifindex);
				if (slave != NULL) {
					lb = PKTTONATIVE(dngl->osh, p);
					head = tail = lb;
				} else {
					PKTSETLINK(p, NULL);
					PKTFREE(dngl->osh, p, FALSE);
					p_drop = 1;
				}
			} else {
				lb = PKTTONATIVE(dngl->osh, p);
				PKTSETLINK(tail, lb);
				tail = lb;
			}
		}

		p_drop ? dngl->stats.rx_dropped++ : dngl->stats.rx_packets++;

		/* conclude the linking and send it up if
		    1) n == NULL (end of the link)
		    2) p (current pkt) is dropped
		    3) ifindex of n (next pkt) is different
		*/
		if (head != NULL &&
				(n == NULL ||
#ifndef BCMMSGBUF
				p_ifindex != n_ifindex ||
#endif // endif
				p_drop)) {
			PKTSETLINK(tail, NULL);

			ASSERT(slave != NULL && slave->ops != NULL);
			if (slave->ops->xmit(dngl->rtedev, slave, head) != 0) {
				/* stats.rx_dropped may not be exact in case of
				   the multiple pkts linking
				*/
				dngl->stats.rx_dropped++;
				trace("dropped pkt 0x%p; len %d for %s", p,
				    PKTLEN(dngl->osh, head), slave->name);
			}

			head = tail = NULL;
		}

		p = n;
	} /* while (p) */
#else
	/* loopback from rx path to tx path */
	while (p != NULL) {
		bus_ops->tx(dngl->bus, p);
		p = PKTLINK(p);
	}
#endif /* DNGL_LB */
} /* dngl_sendup */
#else /* PKTC_TX_DONGLE */
void
dngl_sendwl(struct dngl *dngl, void *p)
{
#ifndef DNGL_LB
	struct lbuf *lb;
	hnd_dev_t *slave = NULL;
	int ifindex = 0;
#ifndef BCMMSGBUF
	/* bdc header not valid for msgbuf protocol */
	struct bdc_header *bdc;

	trace("%s: pkt len %d", dngl->rtedev->name, PKTLEN(dngl->osh, p));

	/* Save the BDC header pointer but only use it after the validation
	 * is done in proto_pkt_header_pull().
	 */
	bdc = (struct bdc_header *)PKTDATA(dngl->osh, p);
	ifindex = bdc->flags2 & BDC_FLAG2_IF_MASK;
#else
	ifindex = PKTIFINDEX(dngl->osh, p);
#endif /* BCMMSGBUF */
	/* Pull protocol-level header */
	if (proto_ops->proto_pkt_header_pull_fn(dngl->proto, p))
		return;

	if ((lb = PKTTONATIVE(dngl->osh, p)) == NULL ||
	    (slave = dngl_finddev(dngl, ifindex)) == NULL ||
	    (slave->ops->xmit(dngl->rtedev, slave, lb) != 0)) {
		dngl->stats.rx_dropped++;
		trace("dropped pkt 0x%p; len %d for %s", p, PKTLEN(dngl->osh, p),
		    slave == NULL ? "unknown" : slave->name);
	} else {
		dngl->stats.rx_packets++;
	}
#else
	/* loopback from rx path to tx path */
	bus_ops->tx(dngl->bus, p);
#endif /* DNGL_LB */
} /* dngl_sendup */
#endif /* PKTC_TX_DONGLE */

/**
 * This function informs the WL subsystem about an operation on a flow ring, so the WL subsystem can
 * take action on that event. A 'flowring' is a PCIe full dongle specific concept.
 *
 *     @param[in] op	e.g. FLOW_RING_CREATE, FLOW_RING_FLUSH, FLOW_RING_TIM_RESET
 */
int
dngl_flowring_update(struct dngl *dngl, uint8 ifindex, uint16 flowid, uint8 op, uint8 * sa,
	uint8 *da, uint8 tid, uint8 *mode)
{
	hnd_dev_t *slave = NULL;

	slave = dngl_finddev(dngl, ifindex);

	if (!slave) {
		return BCME_ERROR;
	} else if (slave->ops->flowring_link_update) {
		/* calls e.g. wl_flowring_update() (so a call to the WL subsystem) */
		return slave->ops->flowring_link_update(slave, flowid,  op,  sa, da,  tid, mode);
	}

	return BCME_ERROR;
}

#ifdef WLCFP
/**
 * Link a flow ring with CFP layer cubby
 * populate cfp layer flowid and a TCB state array in flow ring layer
 */
int dngl_bus_cfp_link(struct dngl *dngl, uint8 ifindex, uint16 ringid, uint8 tid,
	uint8* da, uint8 op, uint8** tcb_state, uint16* cfp_flowid)
{
	hnd_dev_t *slave;

	/* Find slave */
	slave = dngl_finddev(dngl, ifindex);

	if (!slave) {
		return BCME_ERROR;
	} else if (slave->ops->cfp_flow_link) {
		/* call to the WL subsystem to link CFP flow with bus layer */
		return slave->ops->cfp_flow_link(slave, ringid, tid,
			da, op, tcb_state, cfp_flowid);
	}

	return BCME_ERROR;
}
#endif /* WLCFP */

void
dngl_ctrldispatch(struct dngl *dngl, void *p, uchar *buf)
{
	proto_ops->proto_ctrldispatch_fn(dngl->proto, p, buf);
}

void
dngl_resume(struct dngl *dngl)
{
	/* resumed on bus: power up misc. resources */
	trace("%s", dngl->rtedev->name);
}

void
dngl_suspend(struct dngl *dngl)
{
	/* suspended on bus: power down misc. resources */
	trace("%s", dngl->rtedev->name);
}

#if defined(RTE_CONS) && defined(BCMUSBDEV)
#if defined(DTMTEST)
static void
dngl_initiate_resume(void *arg, int argc, char *argv[])
{
	struct dngl *dngl = (struct dngl *)arg;

	if (dngl == NULL)
		return;

	err("rmwk: dngl_initiate_resume");
	bus_ops->resume(dngl->bus);
}
#endif /* DTMTEST */

extern void dngl_reboot(void *arg, int argc, char *argv[]);

void
dngl_reboot(void *arg, int argc, char *argv[])
{
	dngl_schedule_work(arg, NULL, _dngl_reboot, 200);
}
#endif /* RTE_CONS && BCMUSBDEV */

/**
 * bind/enslave to the device
 * Bus can be bound to multiple devices but not the other way around.
 * Return >= 0 to indicate success and I/F index carried in BDC header.
 * Return < 0 to indicate failure.
 */
/**
 * if numslaves is not -1 in case of virtual slave, it denotes
 * the os index passed from the host. If DNGL_RTE_HIDDEN_IF bit
 * is set in numslaves it denotes hidden IF.
 */
int
dngl_binddev(struct dngl *dngl, void *bus, void *dev, uint numslaves)
{
	hnd_dev_t *ldev = NULL;
	uint i;
	uint32 if_index = numslaves;
	/* max_ifidx is the maximum allowable host interfaces */
	uint8 max_ifidx = dngl->tunable_max_host_if;
	trace("start");

	if (bus == NULL || dev == NULL) {
		err("bus and/or dev is NULL");
		return BCME_ERROR;
	}

	/* set as the primary device */
	if (dngl->rtedev == NULL && dngl->primary_slave == NULL && numslaves > 0) {
		dngl->rtedev = bus;
		/* Book keep of num_realslave_devs in dngl_rte is done here.
		 * This uses slave slot in else condition(virtual devices)
		 * of the block greater than or equal to num_realslave_devs.
		 * This will help to protect real slaves in slave array.
		 * Slave array should fill first numslaves slot for num_realslave_devs.
		 */
		for (i = 0; i < numslaves; i++) {
			ldev = ((hnd_dev_t *)dev) + i;
			dngl->slaves[i] = ldev;
			((hnd_dev_t *)ldev)->chained = bus;
		}
		dngl->iface2slave_map[0] = 0;
		dngl->num_realslave_devs = numslaves;
		dngl->primary_slave = dev;
		((hnd_dev_t *)bus)->chained = dev;
		dngl->num_activeslave_devs += numslaves;
	}
	/* set as the virtual device */
	else if (dngl->rtedev == bus && dngl->primary_slave != dev) {
		int ifindex, slaveidx;
#ifdef BCMDBG
		for (ifindex = dngl->num_realslave_devs;
			ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
			if (dngl_get_slave(dngl, ifindex) == dev)
				break;
		}
		if (ifindex < dngl->tunable_max_slave_devs) {
			err("dev is already bound");
			return BCME_ERROR;
		}
#endif /* BCMDBG */

		if ((if_index > 0) && (if_index <= max_ifidx)) {
			if (dngl->iface2slave_map[if_index] != -1)  {
				/* Meaning ifindex is not free return BCME_ERROR */
				err("Virtual interface with ifidx:%d is already present", if_index);
				return BCME_ERROR;
			}
			ifindex = if_index;
		} else if (if_index == UINT32_MAX) {
			/* if host index is not provided, allocated from the available index */
			for (ifindex = dngl->num_realslave_devs - 1;
					ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
				if (dngl->iface2slave_map[ifindex] == -1) {
					break;
				}
			}
		} else if (if_index & DNGL_RTE_HIDDEN_IF) {
			/* Hidden interfaces are allocated from the higher available indices */
			for (ifindex = dngl->tunable_max_slave_devs - 1;
				ifindex > max_ifidx; ifindex --) {
				if (dngl->iface2slave_map[ifindex] == -1) {
					break;
				}
			}
		} else {
			/* Meaning ifindex is not valid return BCME_ERROR */
			err("Virtual interface with ifidx:%d is not valid ", if_index);
			return BCME_ERROR;
		}
		for (slaveidx = dngl->num_realslave_devs;
			slaveidx < dngl->tunable_max_slave_devs; slaveidx++) {
			if (dngl->slaves[slaveidx] == NULL) {
				break;
			}
		}

		if (ifindex >= dngl->tunable_max_slave_devs ||
			slaveidx >= dngl->tunable_max_slave_devs) {
			err("too many virtual devices: %d, ifidx:%d, slaveidx:%d",
				dngl->tunable_max_slave_devs, ifindex, slaveidx);
			return BCME_ERROR;
		}

		dngl->slaves[slaveidx] = dev;
		dngl->iface2slave_map[ifindex] = slaveidx;
		dngl->num_activeslave_devs++;
		/* Any correction in the iface to osif has */
		/* to be done by the module adding the interface */
		dngl->iface2osif[ifindex] = ifindex;

		((hnd_dev_t *)dev)->chained = bus;
		return ifindex;
	} else {
		err("rtedev %p slave %p bus %p dev %p inconsistent",
		    dngl->rtedev, dngl->primary_slave, bus, dev);
		return BCME_ERROR;
	}

	trace("  call proto_attach");
	dngl->proto = proto_ops->proto_attach_fn(dngl->osh, dngl, dngl->bus,
	                                 ((hnd_dev_t *)bus)->name, FALSE);
	if (!(dngl->proto)) {
		err("proto_attach failed");
		return BCME_ERROR;
	}

	dbg("%s: %s Network Adapter (%s)", ((hnd_dev_t *)bus)->name,
	    getvar(NULL, "manf") ? : "Broadcom", ((hnd_dev_t *)dev)->name ? : "P-t-P");

	return dngl_add_cons_cmds(dngl);
} /* dngl_binddev */

static int
dngl_add_cons_cmds(struct dngl *dngl)
{
#if defined(RTE_CONS) && defined(BCMUSBDEV)
#if defined(DTMTEST)
	/* DTM USB Device Framework for remote wakeup
	 *
	 * During this test, wl cmd is not available and there
	 * are no external switches on Neptune board to initiate
	 * remote wakeup so using serial interface for this.
	 */
	if (!hnd_cons_add_cmd("rmwk", dngl_initiate_resume, dngl))
		return BCME_ERROR;
#endif /* DTMTEST */
	if (!hnd_cons_add_cmd("reboot", dngl_reboot, dngl))
		return BCME_ERROR;

#ifdef BCMDBG
	if (!hnd_cons_add_cmd("br", dngl_dump_busregs, dngl))
		return BCME_ERROR;

	if (!hnd_cons_add_cmd("msg", dngl_bus_msgbits, dngl))
		return BCME_ERROR;
#endif // endif
#endif /* RTE_CONS && BCMUSBDEV */
	return BCME_OK;
}

/** Remapping of an existing interface to a new real/virtual slave */
void
dngl_rebinddev(struct dngl *dngl, void *bus, void *new_dev, int ifindex)
{
	int i;
	for (i = 0; i < dngl->tunable_max_slave_devs; i++) {
		/* real slaves will never get replaced */
		if (dngl->slaves[i] == new_dev) {  /* case of real slave */
			dngl->primary_slave = new_dev;
			break;
		} else if (dngl->slaves[i] == NULL) {
			dngl->slaves[i] = new_dev; /* new virt dev to slave array */
			((hnd_dev_t *)new_dev)->chained = (hnd_dev_t *)bus; /* dev <-> bus */
			dngl->slaves[dngl->iface2slave_map[ifindex]] = NULL;
			break;
		}
	}
	if (i >= dngl->tunable_max_slave_devs) {
		err("No matching slave");
		ASSERT(0);
	}
	dngl->iface2slave_map[ifindex] = i; /* new slave being assigned to interface */
}

/** unbind the wireless device */
int
dngl_unbinddev(struct dngl *dngl, void *bus, void *dev)
{
	if (bus == NULL || dev == NULL) {
		err("bus and/or dev is NULL");
		return -1;
	}

	/* validate primary device */
	if (dngl->rtedev == NULL && dngl->primary_slave == NULL) {
		err("No primary device");
		return -1;
	}
	/* check for the virtual device */
	else if (dngl->rtedev == bus && dngl->primary_slave != dev) {
		int ifindex;

		for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
			if (dngl_get_slave(dngl, ifindex) == dev) {
				dngl->slaves[dngl->iface2slave_map[ifindex]] = NULL;
				dngl->iface2slave_map[ifindex] = -1;
				dngl->num_activeslave_devs--;
				((hnd_dev_t *)dev)->chained = NULL;
				return ifindex;
			}
		}

		err("dev %p is not bound", dev);
		return -1;
	} else {
		err("rtedev %p slave %p bus %p dev %p inconsistent",
		    dngl->rtedev, dngl->primary_slave, bus, dev);
		return -1;
	}
}

void
dngl_sendup(struct dngl *dngl, void *p)
{
	dngl_sendwl(dngl, p);
}

/* Validate the wireless device */
int
dngl_validatedev(struct dngl *dngl, void *bus, void *dev)
{
	int ifindex = 0;
	int slave_idx = 0;

	if (bus == NULL || dev == NULL) {
		err("dngl_validatedev: bus and/or dev is NULL");
		return BCME_BADARG;
	}

	for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
		slave_idx = dngl->iface2slave_map[ifindex];
		if ((slave_idx != -1) && (dev == dngl->slaves[slave_idx])) {
			return BCME_OK;
		}
	}

	err("dngl_validatedev: dev %p not found", dev);
	return BCME_NOTFOUND;
}

int
dngl_opendev(struct dngl *dngl)
{
	int ret = 0;
	int slave_idx;
	hnd_dev_t *dev = NULL;

	trace("%s", dngl->rtedev->name);

	ASSERT(dngl->primary_slave);

	for (slave_idx = 0; slave_idx < dngl->num_realslave_devs; slave_idx++) {
		dev = dngl->slaves[slave_idx];
		ret = dev->ops->open(dev);

		if (ret)
			return ret;
	}

	dngl->devopen = TRUE;

	return 0;
}

/** Get device stats */
void
dngl_get_stats(struct dngl *dngl, dngl_stats_t *stats)
{
	trace("%s", dngl->rtedev->name);
	/* dngl_stats_t happens to mirror first 8 ulongs in linux net_device_stats */
	bcopy(&dngl->stats, stats, sizeof(dngl->stats));
}

/** Get enslaved net interface stats */
bool
dngl_get_netif_stats(struct dngl *dngl, dngl_stats_t *stats)
{
	int ret;

	trace("%s", dngl->rtedev->name);
	bzero(stats, sizeof(dngl_stats_t));
	/* dngl_stats_t happens to mirror first 8 ulongs in linux net_device_stats */
	if ((ret = dngl_dev_ioctl(dngl, RTEGSTATS, stats, sizeof(dngl_stats_t))) < 0) {
		err("%s: error reading slave addr: %d", dngl->rtedev->name, ret);
		return TRUE;
	} else {
		return FALSE;
	}
}

ulong
dngl_get_netif_mtu(struct dngl *dngl)
{
	uint32 val = 0;
	int ret;

	trace("%s", dngl->rtedev->name);
	if ((ret = dngl_dev_ioctl(dngl, RTEGMTU, &val, sizeof(val))) < 0)
		err("%s: error reading slave MTU: %d", dngl->rtedev->name, ret);

	return val;
}

#ifdef FLASH_UPGRADE
/** FLASH_UPGRADE specific function */
int
dngl_upgrade(struct dngl *dngl, uchar *buf, uint len)
{
	uint32 offset;
	uint origidx;
	chipcregs_t *ccregs;
	int ret = 0;
	static int upgrade_size;
	static uint32 upgrade_crc, trx_crc;

	if (len < 4)
		return BCME_BADLEN;

	/* First word of each chunk is the offset */
	offset = ltoh_ua((uint32 *) buf);
	buf += 4;
	len -= 4;

	if (offset == 0) {
		struct trx_header trx;

		/* Examine TRX header */
		if (len < sizeof(struct trx_header)) {
			dbg("%s: dngl_upgrade: File is too small (%d bytes)",
			       dngl->rtedev->name, len);
			return BCME_BADLEN;
		}

		/* Avoid alignment issues */
		memcpy(&trx, buf, sizeof(struct trx_header));
		if (ltoh32(trx.magic) != TRX_MAGIC ||
		    ltoh32(trx.len) > TRX_MAX_LEN ||
		    ltoh32(trx.len) < sizeof(struct trx_header)) {
			dbg("%s: dngl_upgrade: Bad trx header", dngl->rtedev->name);
			return BCME_ERROR;
		}
		upgrade_crc = hndcrc32((uint8 *) &((struct trx_header *) buf)->flag_version,
		                       len - OFFSETOF(struct trx_header, flag_version),
		                       CRC32_INIT_VALUE);
		trx_crc = ltoh32(trx.crc32);
		upgrade_size = ltoh32(trx.len) - sizeof(struct trx_header);
		len -= sizeof(struct trx_header);
		buf += sizeof(struct trx_header);
	} else {
		offset -= sizeof(struct trx_header);
		upgrade_crc = hndcrc32((uint8 *) buf, len, upgrade_crc);
	}

	origidx = si_coreidx(dngl->sih);

	ccregs = (chipcregs_t *)si_setcore(dngl->sih, CC_CORE_ID, 0);
	if (!ccregs) {
		dbg("%s: dngl_upgrade: bad ccregs", dngl->rtedev->name);
		ret = -1;
		goto end;
	}

	if (offset == 0) {
		/* Initialize serial flash access (only support serial flash for now) */
		if (!sflash_init(dngl->sih, ccregs)) {
			dbg("%s: *** dngl_upgrade: sflash_init failed",
			       dngl->rtedev->name);
			ret = -2;
			goto end;
		}
		dngl->upgrade_status = WLC_UPGRADE_PENDING;
	}

	if (len && (sflash_commit(dngl->sih, ccregs, offset, len, buf) != 0)) {
		dbg("%s: *** dngl_upgrade: sflash_commit failed", dngl->rtedev->name);
		ret = -3;
		goto end;
	}

	trace("%s: dngl_upgrade: wrote %d bytes to offset %d",
	      dngl->rtedev->name, len, offset);

	/* check if done */
	if ((upgrade_size -= len) == 0) {
		if (upgrade_crc != trx_crc) {
			dbg("%s: *** dngl_upgrade: Bad CRC", dngl->rtedev->name);
			ret = -4;
		} else {
			dngl->upgrade_status = WLC_UPGRADE_SUCCESS;
			dbg("%s: *** dngl_upgrade: upgrade success",
			       dngl->rtedev->name);
		}
	}

end:
	/* restore core index */
	si_setcoreidx(dngl->sih, origidx);
	if (ret != 0)
		dngl->upgrade_status = ret;

	return ret;
} /* dngl_upgrade */

/** FLASH_UPGRADE specific function */
int
dngl_upgrade_status(struct dngl *dngl)
{
	return dngl->upgrade_status;
}
#endif /* FLASH_UPGRADE */

#ifdef BCMET
static void
dngl_et_event(void *context, int link)
{
	struct dngl *dngl = (struct dngl *) context;

	trace("%s", dngl->rtedev->name);
	proto_ops->proto_dev_event_fn(dngl->proto, (void *) &link);
}
#endif /* BCMET */

#ifdef RTE_CONS
#ifdef BCMDBG

/** RTE_CONS&&BCMDBG specific function */
static void
dngl_dump_busregs(void *arg, int argc, char *argv[])
{
	bus_ops->dumpregs();
}

/** RTE_CONS&&BCMDBG specific function */
static void
dngl_bus_loopback(void *arg, int argc, char *argv[])
{
	bus_ops->loopback();
}

/** RTE_CONS&&BCMDBG specific function */
static void
dngl_bus_xmit(void *arg, int argc, char *argv[])
{
	int len = -1;
	int clen = 0;
	bool ctl = FALSE;
	uint argnum = 1;

	if (argnum < argc) {
		ctl = !strcmp(argv[argnum], "-c");
		if (ctl)
			argnum++;
	}

	if (argnum < argc)
		len = atoi(argv[argnum++]);

	if (argnum < argc)
		clen = atoi(argv[argnum++]);

	bus_ops->xmit(len, clen, ctl);
}

/** RTE_CONS&&BCMDBG specific function */
static void
dngl_bus_msgbits(void *arg, int argc, char *argv[])
{
	uint bits, newbits;

	if (argc > 1) {
		newbits = bcm_strtoul(argv[1], NULL, 16);
		bits = bus_ops->msgbits(&newbits);
	} else {
		bits = bus_ops->msgbits(NULL);
	}

	printf("Message bits: 0x%0x\n", bits);
}

#endif /* BCMDBG */
#endif /* RTE_CONS */

void
dngl_init(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);

	/* check if init called w/o previous halt */
	if (dngl->up)
		return;

	/* Open slave device */
	if (dngl->primary_slave) {
		int magic;
#ifndef BCM_RECLAIM_INIT_FN_DATA
		int err = 0;

		err = dngl_opendev(dngl);
		if (err)
			err("error: device open failed\n");
#endif // endif

		if (dngl_dev_ioctl(dngl, WLC_GET_MAGIC, &magic, sizeof(magic)) >= 0 &&
		    magic == WLC_IOCTL_MAGIC) {
			dngl->medium = DNGL_MEDIUM_WIRELESS;
		}

#ifdef BCMET
		if (!dngl->cb.fn) {
			uint32 cmd;
			int pcb;
			uint32 Status = 0;
			/* Handle all events */
			dngl->cb.fn = dngl_et_event;
			cmd = OID_ET_SETCALLBACK;
			dngl->cb.context = dngl;
			/* Register event handler */
			pcb = (int) &dngl->cb;
			if (dngl_dev_ioctl(dngl, cmd, &pcb, sizeof(pcb)) < 0) {
				err("%s: SET CALLBACK failed", dngl->rtedev->name);
				Status = BCME_ERROR;
				return;
			}
		}
#endif /* BCMET */
	}

#ifdef RTE_CONS
#ifdef BCMDBG
	if (!hnd_cons_add_cmd("msg", dngl_bus_msgbits, 0) ||
		!hnd_cons_add_cmd("br", dngl_dump_busregs, 0) ||
		!hnd_cons_add_cmd("lb", dngl_bus_loopback, 0) ||
		!hnd_cons_add_cmd("tx", dngl_bus_xmit, 0))
	   return;
#endif /* BCMDBG */
#endif /* RTE_CONS */
	dngl->up = TRUE;
} /* dngl_init */

void
BCMATTACHFN(dngl_halt)(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);

	dngl->up = FALSE;

	/* close the slave device & revert to its HW MAC addr */
	if (dngl->primary_slave) {
		uint etheraddr[ROUNDUP(sizeof(struct ether_addr), 4)/4]; /* 32-bit align */
		int len;

#ifndef BCM_RECLAIM_INIT_FN_DATA
		dngl->primary_slave->ops->close(dngl->primary_slave);
		dngl->devopen = FALSE;
#endif // endif

		/* WHQL: restore permanent ether addr on halt */
		len = sizeof(etheraddr);
		if (dngl_dev_ioctl(dngl, RTEGPERMADDR, etheraddr, len) < 0)
			err("%s: RTEGPERMADDR failed", dngl->rtedev->name);
		else
			if (dngl_dev_ioctl(dngl, RTESHWADDR, etheraddr, len) < 0)
				err();
	}
}

void
dngl_reset(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);

	/* purge any stale ctrl & intr packets */
	bus_ops->softreset(dngl->bus);
}

void
_dngl_reboot(dngl_task_t *task)
{
	struct dngl *dngl = (struct dngl *) hnd_timer_get_ctx(task);

	si_watchdog(dngl->sih, 1);
	while (1);
}

void
dngl_keepalive(struct dngl *dngl, uint32 msec)
{
	/* set the watchdog for # msec */
	si_watchdog_ms(dngl->sih, msec);
}

static hnd_dev_t*
dngl_get_slave(struct dngl *dngl, int ifindex)
{
	int slave_idx;
	hnd_dev_t *dev = NULL;
	if (ifindex >= 0 &&
	    ifindex < dngl->tunable_max_slave_devs &&
	    (slave_idx = dngl->iface2slave_map[ifindex]) != -1) {
		dev = dngl->slaves[slave_idx];
	} else {
		dev = NULL;
	}

	return dev;
}

int
dngl_max_slave_devs(struct dngl *dng)
{
	return dng->tunable_max_slave_devs;
}

bool
dngl_maxdevs_reached(struct dngl *dngl)
{
	return (dngl->num_activeslave_devs >= (uint16)dngl->tunable_max_slave_devs);
}

#if defined(BCMUSBDEV) && defined(BCMCDC)
#include <dngl_cdc.h>
void
dngl_pr46794WAR(struct dngl *dngl)
{
	proto_pr46794WAR(dngl->proto);
}
#endif // endif
