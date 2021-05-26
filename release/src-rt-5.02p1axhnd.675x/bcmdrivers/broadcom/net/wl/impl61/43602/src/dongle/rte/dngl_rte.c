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
 * $Id: dngl_rte.c 715700 2017-08-14 05:55:14Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <proto/ethernet.h>
#include <trxhdr.h>
#include <bcmnvram.h>

#include <osl.h>
#include <wlioctl.h>
#include <bcmendian.h>
#include <epivers.h>
#include <sflash.h>
#include <sbchipc.h>
#include <dngl_stats.h>
#ifdef BCMET
#include <etioctl.h>
#endif // endif

#include <dngl_bus.h>
#include <dngl_api.h>
#include <dngl_protocol.h>
#include <dngl_dbg.h>
#include <dngl_rte.h>
#ifdef BCM_FD_AGGR
#include <bcm_rpc_tp.h>
#endif // endif
#include <bcmmsgbuf.h>

static uint8 found = 0;

int dngl_msglevel = DNGL_ERROR;

struct dngl_bus_ops *bus_ops = NULL;
#ifdef HNDRTE_CONSOLE
#ifdef BCMDBG
static void dngl_dump_busregs(void);
static void dngl_bus_msgbits(uint32 arg, uint argc, char *argv[]);
#endif // endif
#endif /* HNDRTE_CONSOLE */
#ifdef BCM_OL_DEV

void
dngl_rxoe(struct dngl *dngl, void *p);
#endif // endif

#ifdef BCM_FD_AGGR
static void dngl_rpctimer(dngl_timer_t *t);
static int dngl_aggr_iovar(struct dngl *dngl, char *buf, int inlen, int *outlen, bool set);
#endif // endif
/* Find the device from the ifindex */
static hndrte_dev_t *
dngl_finddev(struct dngl *dngl, int ifindex)
{
	hndrte_dev_t *slave = dngl->slave;

	/* Forward packet to the enslaved device based on the IF index in the BCD header */
	/* IF 0: primary enslaved device pointed by dngl->slave.
	 * IF 1 and up: virtual device by dngl->vslave[IF-1].
	 */
	if ((ifindex > 0) && (dngl->tunable_max_slave_devs > 0)) {
		if (--ifindex >= dngl->tunable_max_slave_devs ||
		    (slave = dngl->vslave[ifindex]) == NULL) {
			err("vslave %d not found", ifindex);
			return NULL;
		}
	}

	return slave;
}

/* Call Broadcom wireless driver private ioctl */
int
_dngl_devioctl(struct dngl *dngl, int ifindex,
	uint32 cmd, void *buf, int len, int *used, int *needed, bool set)
{
	int ret = -1;
	hndrte_dev_t *slave;

	/* buf must be 4-byte aligned */
	ASSERT(ISALIGNED(buf, 4));

	trace("ioctl %s cmd 0x%x, len %d", set ? "SET" : "QUERY", cmd, len);
#if BCM_FD_AGGR
	if (cmd == WLC_SET_VAR || cmd == WLC_GET_VAR) {
		if (!strncmp((char *)buf, "rpc_", 4)) {
			ret = dngl_aggr_iovar(dngl, buf, len, used, set);
			return ret;
		}
	}
#endif // endif
	if ((slave = dngl_finddev(dngl, ifindex)) != NULL)
		ret = slave->funcs->ioctl(slave, cmd, buf, len, used, needed, set);
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

#if MAXVSLAVEDEVS > 0
	dngl->tunable_max_slave_devs = MAXVSLAVEDEVS;

	if (!(dngl->vslave =
	      (hndrte_dev_t **)MALLOC(osh, MAXVSLAVEDEVS * sizeof(hndrte_dev_t *)))) {
		err("vslave MALLOC failed");
		goto fail;
	}

	memset(dngl->vslave, 0, MAXVSLAVEDEVS * sizeof(hndrte_dev_t *));
#endif /* MAXVSLAVEDEVS > 0 */

#ifdef BCM_FD_AGGR
	if (!(dngl->rpc_th = bcm_rpc_tp_attach(osh, dngl)))
		goto fail;
	bcm_rpc_tp_register_cb(dngl->rpc_th, NULL, NULL, (rpc_rx_fn_t)dngl_sendwl, dngl,
		NULL);
	dngl->rpctimer = dngl_init_timer(dngl, NULL, dngl_rpctimer);
	dngl->rpctime = BCM_RPC_TP_DNGL_TMOUT;
#endif // endif

	return dngl;

fail:
	if (dngl)
		if (MFREE(NULL, dngl, sizeof(dngl_t)))
			err("MFREE error");
	return NULL;
}

void
BCMATTACHFN(dngl_detach)(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);

	/* unbind from enslaved device */
	if (dngl->slave) {
#ifdef BCMET
		int pcb;
		uint32 cmd;
		cmd = OID_ET_UNSETCALLBACK;
		pcb = (int) &dngl->cb;
		if (dngl_dev_ioctl(dngl, cmd, &pcb, sizeof(pcb)) < 0)
			err("%s: UNSET CALLBACK failed", dngl->rtedev->name);
#endif // endif
		dngl->slave->chained = NULL;
		dngl->rtedev->chained = NULL;
	}

	proto_detach(dngl->proto);
#ifdef BCM_FD_AGGR
	if (dngl->rpc_th) {
		bcm_rpc_tp_deregister_cb(dngl->rpc_th);
		bcm_rpc_tp_detach(dngl->rpc_th);
	}
	if (dngl->rpctimer) {
		if (dngl->rpctimer_active)
			dngl_del_timer(dngl->rpctimer);
		dngl_free_timer(dngl->rpctimer);
	}
#endif // endif
}

/* Net interface running faster than USB. Flow-control the net interface */
void
dngl_txstop(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);
	/* pause network device indications */
	if (dngl->slave->funcs->txflowcontrol)
		dngl->slave->funcs->txflowcontrol(dngl->slave, TRUE, 0);
}

void
dngl_txstart(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);
	/* resume network device indications */
	if (dngl->slave->funcs->txflowcontrol)
		dngl->slave->funcs->txflowcontrol(dngl->slave, FALSE, 0);
}

/* flow control called by dev receiving packets from us */
void
dngl_rxflowcontrol(struct dngl *dngl, bool state, int prio)
{
	trace("%s flowctl %s", dngl->rtedev->name, state == ON ? "on" : "off");
	bus_ops->rxflowcontrol(dngl->bus, state, prio);
}

/* Set the ifindex in the BDC header */
static int
dngl_setifindex(struct dngl *dngl, void *src, void *p)
{
	int ifindex = 0;

	/* save IF index in the bdc header */
	/* IF 0: primary enslaved device pointed by dngl->slave.
	 * IF 1 and up: virtual device by dngl->vslave[IF-1].
	 */
	if (src != NULL && src != dngl->slave) {
		for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
			if (src == dngl->vslave[ifindex])
				break;
		}
		if (ifindex >= dngl->tunable_max_slave_devs) {
			err("vslave %p not found, dropped pkt", src);
			return -1;
		}
#ifndef BCMMSGBUF
		{
			struct bdc_header *bdc;

			bdc = (struct bdc_header *)PKTDATA(dngl->osh, p);
			bdc->flags2 |= (uint8)(ifindex + 1);
		}
#else
		{
			/* bdc header not valid for pcie full dongle */
			/* In rx path, when non splitrx mode is enabled */
			/* ifidx field in lbuf is used */
			PKTSETIFINDEX(dngl->osh, p, ifindex + 1);
		}
#endif /* BCMMSGBUF */

	}

	return ifindex;
}

/* Transmit a stack packet onto the bus */
int
#ifdef BCMUSBDEV_BMAC
dngl_sendpkt(struct dngl *dngl, void *src, void *p, uint32 ep_idx)
#else
dngl_sendpkt(struct dngl *dngl, void *src, void *p)
#endif /* BCMUSBDEV_BMAC */
{
	trace("%s", dngl->rtedev->name);
	uint32 pktlen;
	int ret;

	p = PKTFRMNATIVE(dngl->osh, p);

	/* Push protocol-level header */
	if ((p = proto_pkt_header_push(dngl->proto, p)) &&
	    dngl_setifindex(dngl, src, p) >= 0) {
#ifdef  BCM_FD_AGGR
		if (dngl->fdaggr & BCM_FDAGGR_D2H_ENABLED) {
			bcm_rpc_tp_buf_send(dngl->rpc_th, p);
			if (dngl->rpctimer) {
				if (!dngl->rpctimer_active) {
					dngl_add_timer(dngl->rpctimer, dngl->rpctime, FALSE);
					dngl->rpctimer_active = TRUE;
				}
			}
		} else
#endif /* BCM_FD_AGGR */
		{
		/* for pcie full dongle, pkt is freed inside bus_ops->tx
		* for wl events. So keep backup ok totlen before calling bus->tx
		* use pkttotlen() since p could be a chained pkt
		*/
		pktlen = pkttotlen(dngl->osh, p);
#ifdef BCMUSBDEV_BMAC
			ret = bus_ops->tx(dngl->bus, p, ep_idx);
#else
			ret = bus_ops->tx(dngl->bus, p);
#endif /* BCMUSBDEV_BMAC */
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
}

/* Transmit a stack packet onto the bus */
int
dngl_sendctl(struct dngl *dngl, void *src, void *p)
{
	trace("%s", dngl->rtedev->name);

	p = PKTFRMNATIVE(dngl->osh, p);

	/* Push protocol-level header */
	if ((p = proto_pkt_header_push(dngl->proto, p)) &&
	    dngl_setifindex(dngl, src, p) >= 0) {
		bus_ops->sendctl(dngl->bus, p);
	}
	return 0;
}

/*
 * Send a packet received from the bus to the enslaved device or queue
 * it for later transmission to the enslaved device
 */
#ifdef PKTC_TX_DONGLE
#ifdef BCMMSGBUF
/*
 * PCIe IPC BCMMSGBUF: All packets in packet chain
 * - are to the same interface,
 * - do not include a protocol header, and
 * - are from Tx Lfrag pool and PKTNEXT() is NULL, so no PKTTONATIVE()
 */
void
dngl_sendwl(struct dngl *dngl, void *p, uint32 pktcnt)
{
	void *n;
#ifndef DNGL_LB
	int p_ifindex;
	hndrte_dev_t *slave;
	p_ifindex = PKTIFINDEX(dngl->osh, p);
	slave = dngl_finddev(dngl, p_ifindex);
	if (slave == NULL) {
		goto drop_pkt_chain;
	}
	ASSERT(slave->funcs != NULL);
	dngl->stats.rx_packets += pktcnt;
	/* All packets are from Tx Lfrag pool, so no need to PKTTONATIVE */
	if (slave->funcs->xmit(dngl->rtedev, slave, (struct lbuf *)p) != 0) {
		dngl->stats.rx_dropped += pktcnt;
		err("dropped pktc 0x%p pktcnt %u for %s", p, pktcnt, slave->name);
	}
	return;

drop_pkt_chain:
	/* Traverse packet chain list, dropping each packet */
	while (p != NULL) {
		n = PKTLINK(p);
		PKTSETLINK(p, NULL);
		PKTFREE(dngl->osh, p, FALSE);
		p = n;
	}
	dngl->stats.rx_dropped += pktcnt;
#else /* DNGL_LB */
	while (p != NULL) {
		n = PKTLINK(p);
		bus_ops->tx(dngl->bus, p); /* bus loopback: from rx path to tx path */
		p = n;
	}
#endif /* DNGL_LB */
}
#else /* !BCMMSGBUF */
void
dngl_sendwl(struct dngl *dngl, void *p)
{
#ifndef DNGL_LB
	struct lbuf *lb;
	hndrte_dev_t *slave = NULL;
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
		p_drop = proto_pkt_header_pull(dngl->proto, p);

		if (!p_drop) {
			if (!tail) {
				slave = dngl_finddev(dngl, p_ifindex);
				if (slave != NULL) {
					lb = PKTTONATIVE(dngl->osh, p);
					head = tail = lb;
				} else {
					PKTFREE(dngl->osh, p, FALSE);
					p_drop = 1;
				}
			}
			else {
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

			ASSERT(slave != NULL && slave->funcs != NULL);
			if (slave->funcs->xmit(dngl->rtedev, slave, head) != 0) {
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
	}

#else
	/* loopback from rx path to tx path */
	while (p != NULL) {
		bus_ops->tx(dngl->bus, p);
		p = PKTLINK(p);
	}
#endif /* DNGL_LB */
}
#endif /* !BCMMSGBUF */
#else /* PKTC_TX_DONGLE */
void
dngl_sendwl(struct dngl *dngl, void *p)
{
#ifndef DNGL_LB
	struct lbuf *lb;
	hndrte_dev_t *slave = NULL;
	int ifindex = 0;
#ifdef WLC_HIGH
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
#endif /* WLC_HIGH */

	/* Pull protocol-level header */
	if (proto_pkt_header_pull(dngl->proto, p))
		return;

	if ((lb = PKTTONATIVE(dngl->osh, p)) == NULL ||
	    (slave = dngl_finddev(dngl, ifindex)) == NULL ||
	    (slave->funcs->xmit(dngl->rtedev, slave, lb) != 0)) {
		dngl->stats.rx_dropped++;
		trace("dropped pkt 0x%p; len %d for %s", p, PKTLEN(dngl->osh, p),
		    slave == NULL ? "unknown" : slave->name);
	}
	else
		dngl->stats.rx_packets++;
#else
	/* loopback from rx path to tx path */
	bus_ops->tx(dngl->bus, p);
#endif /* DNGL_LB */
}
#endif /* PKTC_TX_DONGLE */

int
dngl_flowring_update(struct dngl *dngl, uint8 ifindex, uint16 flowid, uint8 op, uint8 * sa,
	uint8 *da, uint8 tid)
{
	hndrte_dev_t *slave = NULL;
	slave = dngl_finddev(dngl, ifindex);
	if (!slave)
		return BCME_BADARG;
	else if (slave->funcs->flowring_link_update) {
		return slave->funcs->flowring_link_update(slave, flowid,  op,  sa, da,  tid);
	} else
		return BCME_OK;
}

void
dngl_ctrldispatch(struct dngl *dngl, void *p, uchar *buf)
{
	proto_ctrldispatch(dngl->proto, p, buf);
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

#if defined(HNDRTE_CONSOLE) && defined(BCMUSBDEV)
#if defined(DTMTEST)
static void
dngl_initiate_resume(void *arg)
{
	struct dngl *dngl = (struct dngl *)arg;

	if (dngl == NULL)
		return;

	err("rmwk: dngl_initiate_resume");
	bus_ops->resume(dngl->bus);
}
#endif /* DTMTEST */

extern void dngl_reboot(void *arg);

void
dngl_reboot(void *arg)
{
	dngl_schedule_work(arg, NULL, _dngl_reboot, 200);
}
#endif /* HNDRTE_CONSOLE && BCMUSBDEV */

/* bind/enslave to the device */
/* Bus can be bound to multiple devices but not the other way around.
 * Return >= 0 to indicate success and I/F index carried in BDC header.
 * Return < 0 to indicate failure.
 */
int
dngl_binddev(struct dngl *dngl, void *bus, void *dev)
{
	trace("start");

	if (bus == NULL || dev == NULL) {
		err("bus and/or dev is NULL");
		return -1;
	}

	/* set as the primary device */
	if (dngl->rtedev == NULL && dngl->slave == NULL) {
		dngl->rtedev = bus;
		dngl->slave = dev;
		((hndrte_dev_t *)bus)->chained = dev;
		((hndrte_dev_t *)dev)->chained = bus;
	}
	/* set as the virtual device */
	else if (dngl->rtedev == bus && dngl->slave != dev) {
		int ifindex;
#ifdef BCMDBG
		for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
			if (dngl->vslave[ifindex] == dev)
				break;
		}
		if (ifindex < dngl->tunable_max_slave_devs) {
			err("dev is already bound");
			return -1;
		}
#endif /* BCMDBG */
		for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
			if (dngl->vslave[ifindex] == NULL) {
				dngl->vslave[ifindex] = dev;
				break;
			}
		}

		if (ifindex >= dngl->tunable_max_slave_devs) {
			err("too many virtual devices: %d, ifidx:%d", dngl->tunable_max_slave_devs,
				ifindex);
			return -1;
		}
		((hndrte_dev_t *)dev)->chained = bus;
		return ifindex + 1;
	}
	else {
		err("rtedev %p slave %p bus %p dev %p inconsistent",
		    dngl->rtedev, dngl->slave, bus, dev);
		return -1;
	}

	trace("  call proto_attach");
	if (!(dngl->proto = proto_attach(dngl->osh, dngl, dngl->bus,
	                                 ((hndrte_dev_t *)bus)->name, FALSE))) {
		err("proto_attach failed");
		return -1;
	}

	dbg("%s: %s Network Adapter (%s)", ((hndrte_dev_t *)bus)->name,
	    getvar(NULL, "manf") ? : "Broadcom", ((hndrte_dev_t *)dev)->name ? : "P-t-P");

#if defined(HNDRTE_CONSOLE) && defined(BCMUSBDEV)
#if defined(DTMTEST)
	/* DTM USB Device Framework for remote wakeup
	 *
	 * During this test, wl cmd is not available and there
	 * are no external switches on Neptune board to initiate
	 * remote wakeup so using serial interface for this.
	 */
	hndrte_cons_addcmd("rmwk", (cons_fun_t)dngl_initiate_resume, (uintptr)dngl);
#endif /* DTMTEST */
	hndrte_cons_addcmd("reboot", (cons_fun_t)dngl_reboot, (uintptr)dngl);

#ifdef BCMDBG
	hndrte_cons_addcmd("br", (cons_fun_t)dngl_dump_busregs, (uintptr)dngl);
	hndrte_cons_addcmd("msg", (cons_fun_t)dngl_bus_msgbits, (uintptr)dngl);
#endif // endif
#endif /* HNDRTE_CONSOLE && BCMUSBDEV */
	return 0;
}

/* unbind the wireless device */
int
dngl_unbinddev(struct dngl *dngl, void *bus, void *dev)
{
	if (bus == NULL || dev == NULL) {
		err("bus and/or dev is NULL");
		return -1;
	}

	/* validate primary device */
	if (dngl->rtedev == NULL && dngl->slave == NULL) {
		err("No primary device");
		return -1;
	}
	/* check for the virtual device */
	else if (dngl->rtedev == bus && dngl->slave != dev) {
		int ifindex;

		for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
			if (dngl->vslave[ifindex] == dev) {
				dngl->vslave[ifindex] = NULL;
				((hndrte_dev_t *)dev)->chained = NULL;
				return ifindex + 1;
			}
		}

		err("dev %p is not bound", dev);
		return -1;
	}
	else {
		err("rtedev %p slave %p bus %p dev %p inconsistent",
		    dngl->rtedev, dngl->slave, bus, dev);
		return -1;
	}
}

#if defined(PKTC_TX_DONGLE) && defined(BCMMSGBUF)
void
dngl_sendup(struct dngl *dngl, void *p, uint32 pktcnt)
#else
void
dngl_sendup(struct dngl *dngl, void *p)
#endif // endif
{
#ifdef BCM_FD_AGGR
	if (dngl->fdaggr & BCM_FDAGGR_H2D_ENABLED)
		bcm_rpc_tp_rx_from_dnglbus(dngl->rpc_th, p);
	else
#endif // endif
#if defined(PKTC_TX_DONGLE) && defined(BCMMSGBUF)
		dngl_sendwl(dngl, p, pktcnt);
#else
		dngl_sendwl(dngl, p);
#endif // endif
}

/* Validate the wireless device */
int
dngl_validatedev(struct dngl *dngl, void *bus, void *dev)
{
	int ifindex = 0;

	if (bus == NULL || dev == NULL) {
		err("dngl_validatedev: bus and/or dev is NULL");
		return BCME_BADARG;
	}

	/* Check for primary device */
	if (dngl->slave == dev) {
		return BCME_OK;
	}

	/* Check in virtual interfaces */
	for (ifindex = 0; ifindex < dngl->tunable_max_slave_devs; ifindex ++) {
		if (dev == dngl->vslave[ifindex]) {
			return BCME_OK;
		}
	}

	err("dngl_validatedev: dev %p not found", dev);
	return BCME_NOTFOUND;
}

int
dngl_opendev(struct dngl *dngl)
{
	int ret;

	trace("%s", dngl->rtedev->name);

	ASSERT(dngl->slave);
	ret = dngl->slave->funcs->open(dngl->slave);

	if (ret)
		return ret;

	dngl->devopen = TRUE;
	return 0;
}

/* Get device stats */
void
dngl_get_stats(struct dngl *dngl, dngl_stats_t *stats)
{
	trace("%s", dngl->rtedev->name);
	/* hndrte_stats_t happens to mirror first 8 ulongs in linux net_device_stats */
	bcopy(&dngl->stats, stats, sizeof(dngl->stats));
}

/* Get enslaved net interface stats */
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
	} else
		return FALSE;
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
}

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
	proto_dev_event(dngl->proto, (void *) &link);
}
#endif /* BCMET */
#ifdef HNDRTE_CONSOLE
#ifdef BCMDBG
static void
dngl_dump_busregs(void)
{
	bus_ops->dumpregs();
}

static void
dngl_bus_loopback(void)
{
	bus_ops->loopback();
}

static void
dngl_bus_xmit(uint32 arg, uint argc, char *argv[])
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

static void
dngl_bus_msgbits(uint32 arg, uint argc, char *argv[])
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
#endif /* HNDRTE_CONSOLE */
void
dngl_init(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);

	/* check if init called w/o previous halt */
	if (dngl->up)
		return;

	/* Open slave device */
	if (dngl->slave) {
		int magic;
#ifndef BCMRECLAIM
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

#ifdef HNDRTE_CONSOLE
#ifdef BCMDBG
	hndrte_cons_addcmd("msg", (cons_fun_t)dngl_bus_msgbits, 0);
	hndrte_cons_addcmd("br", (cons_fun_t)dngl_dump_busregs, 0);
	hndrte_cons_addcmd("lb", (cons_fun_t)dngl_bus_loopback, 0);
	hndrte_cons_addcmd("tx", (cons_fun_t)dngl_bus_xmit, 0);
#endif /* BCMDBG */
#endif /* HNDRTE_CONSOLE */
	dngl->up = TRUE;
}

void
BCMATTACHFN(dngl_halt)(struct dngl *dngl)
{
	trace("%s", dngl->rtedev->name);

	dngl->up = FALSE;

	/* close the slave device & revert to its HW MAC addr */
	if (dngl->slave) {
		uint etheraddr[ROUNDUP(sizeof(struct ether_addr), 4)/4]; /* 32-bit align */
		int len;

#ifndef BCMRECLAIM
		dngl->slave->funcs->close(dngl->slave);
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
	struct dngl *dngl = (struct dngl *) task->context;

	si_watchdog(dngl->sih, 1);
}

void *
dngl_proto(struct dngl *dngl)
{
	return dngl->proto;
}

void
dngl_keepalive(struct dngl *dngl, uint32 msec)
{
	/* set the watchdog for # msec */
	si_watchdog_ms(dngl->sih, msec);
}
#ifdef BCM_FD_AGGR

uint32
dngl_set_bus_agglimit(struct dngl *dngl, uint32 val)
{
	uint32 outlen = 0;
	uchar cmdstr[] = "bus:rpc_dngl_agglimit";
	uchar buf[sizeof(cmdstr) + sizeof(uint32)];
	memcpy(buf, cmdstr, sizeof(cmdstr));
	*(uint32*) &buf[sizeof(cmdstr)] = val;
	bus_ops->iovar(dngl->bus, (char *)buf, sizeof(buf), &outlen, TRUE);

	if (val) {
		bus_ops->iovar(dngl->bus, (char *)buf, sizeof(buf), &outlen, FALSE);
		if (outlen == sizeof(uint32))
			val = *(uint32*)buf;
	}

	return val;
}

int
dngl_sendpkt_aggr(struct dngl *dngl, void *p)
{
	if (bus_ops->tx(dngl->bus, p)) {
		/* p could be chained pkt */
		dngl->stats.tx_bytes += pkttotlen(dngl->osh, p);
		dngl->stats.tx_packets++;
	} else {
		dngl->stats.tx_dropped++;
		err("dropped pkt");
	}

	return 0;
}

static void
dngl_rpctimer(dngl_timer_t *t)
{
	struct dngl *dngl = (struct dngl *)t->context;
	dngl->rpctimer_active = FALSE;
	bcm_rpc_tp_watchdog(dngl->rpc_th);
}

static int
dngl_aggr_iovar(struct dngl *dngl, char *buf, int inlen, int *outlen, bool set)
{
	uint32 rtn = BCME_OK;
	int offset;
	uint32 val = 0;

	trace("%s: %s", __FUNCTION__, buf);

	for (offset = 0; offset < inlen; ++offset) {
		if (buf[offset] == '\0')
			break;
	}

	if (buf[offset] != '\0')
		return BCME_BADARG;

	++offset;

	if (set && (offset + sizeof(uint32)) > inlen)
		return BCME_BUFTOOSHORT;

	if (set)
		memcpy(&val, buf + offset, sizeof(uint32));

	if (!strcmp(buf, "rpc_agg")) {
		if (set) {
			if (val & BCM_RPC_TP_DNGL_AGG_MASK)
				bcm_rpc_tp_agg_set(dngl->rpc_th, val, TRUE);
			else
				bcm_rpc_tp_agg_set(dngl->rpc_th, BCM_RPC_TP_DNGL_AGG_MASK, FALSE);

			dngl->fdaggr = 0;
			if (val & BCM_RPC_TP_HOST_AGG_MASK)
				dngl->fdaggr |= BCM_FDAGGR_H2D_ENABLED;
			if (val & BCM_RPC_TP_DNGL_AGG_MASK)
				dngl->fdaggr |= BCM_FDAGGR_D2H_ENABLED;
		} else {
			val = bcm_rpc_tp_agg_get(dngl->rpc_th);
		}
	}
	else if (!strcmp(buf, "rpc_dngl_agglimit")) {
		uint8 sf;
		uint16 bytes;
		if (set) {
			sf = val >> 16;
			bytes = val & 0xFFFF;
			bcm_rpc_tp_agg_limit_set(dngl->rpc_th, sf, bytes);
		} else {
			bcm_rpc_tp_agg_limit_get(dngl->rpc_th, &sf, &bytes);
			val = (sf << 16) + (bytes & 0xffff);
		}
	}
	else {
		rtn = BCME_UNSUPPORTED;
	}

	if (!set) {
		memcpy(buf, &val, sizeof(uint32));
		*outlen = sizeof(uint32);
	}

	return rtn;
}
#endif /* BCM_FD_AGGR */
