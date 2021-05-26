/*
 * RTE layer for pcie device
 * hndrte_devfuncs_t & dngl_bus_ops for pciedev are defined here
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
 * $Id: pciedev_rte.c $
 */

#include <osl.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <dngl_bus.h>
#include <dngl_api.h>
#include <circularbuf.h>
#include <bcmpcie.h>
#include <bcmmsgbuf.h>
#include <pciedev.h>
#include <pciedev_dbg.h>
#include <hndsoc.h>
#include <wlfc_proto.h>
#include <flring_fc.h>
#include <event_log.h>

typedef struct {
	int unit;
	hndrte_dev_t *rtedev;
	struct dngl_bus *pciedev;
	osl_t *osh;
	void *regs;
	uint16 device;
	struct dngl *dngl;
	hndrte_timer_t  dpcTimer;	/* 0 delay timer used to schedule dpc */
} drv_t;

/* Reclaimable strings */
static const char BCMATTACHDATA(rstr_fmt_banner)[] = "%s: Broadcom PCIE MSGBUF driver\n";
static const char BCMATTACHDATA(rstr_fmt_devname)[] = "pciemsgbuf%d";

/* Driver entry points */
static void *pciedev_probe(hndrte_dev_t *dev, void *regs, uint bus, uint16 device,
                          uint coreid, uint unit);
static void pciedev_isr(hndrte_dev_t *ctx);

#ifdef PCIE_PHANTOM_DEV
static void sd_dma_isr(hndrte_dev_t *rtedev);
static void usb_dma_isr(hndrte_dev_t *rtedev);
#endif /* PCIE_PHANTOM_DEV */
static int pciedev_open(hndrte_dev_t *dev);
static void _pciedev_dpctask(hndrte_timer_t *timer);
static int pciedev_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb);
static void pciedev_txflowcontrol(hndrte_dev_t *dev, bool state, int prio);
static int pciedev_close(hndrte_dev_t *dev);
static int pciedev_ioctl(hndrte_dev_t *dev, uint32 cmd, void *buf, int len,
	int *used, int *needed, int set);

static hndrte_devfuncs_t pciedev_funcs = {
	probe:		pciedev_probe,
	open:		pciedev_open,
	close:		pciedev_close,
	xmit:		pciedev_send,
	ioctl:      pciedev_ioctl,
	txflowcontrol:	pciedev_txflowcontrol,
	poll:		pciedev_isr
};

struct dngl_bus_ops pciedev_bus_ops = {
	binddev:	pciedev_bus_binddev,
	sendctl:	pciedev_bus_sendctl,
	iovar:		pciedev_bus_iovar,
	unbinddev:	pciedev_bus_unbinddev,
	tx:		pciedev_create_d2h_messages_tx,
	flowring_ctl:	pciedev_bus_flring_ctrl,
	validatedev:    pciedev_bus_validatedev
};

hndrte_dev_t pciedev_dev = {
	name:		"pciedev",
	funcs:		&pciedev_funcs
};

/* Number of devices found */
static int found = 0;
int pci_msg_level = PCI_ERROR_VAL | PCI_TRACE_VAL | PCI_INFORM_VAL;

/** probe function for pcie device */
static const char BCMATTACHDATA(rstr_pciedev_probe_malloc_failed)[] =
	"pciedev_probe: malloc failed\n";
static void *
BCMATTACHFN(pciedev_probe)(hndrte_dev_t *rtedev, void *regs, uint bus, uint16 device,
                        uint coreid, uint unit)
{
	drv_t *drv;
	osl_t *osh;

	PCI_TRACE(("pciedev_probe\n"));

	if (found >= 8) {
		PCI_ERROR(("pciedev_probe: too many units\n"));
		goto fail;
	}

	osh = osl_attach(rtedev);

	if (!(drv = (drv_t *)MALLOC(osh, sizeof(drv_t)))) {
		printf(rstr_pciedev_probe_malloc_failed);
		goto fail;
	}

	bzero(drv, sizeof(drv_t));

	drv->unit = found;
	drv->rtedev = rtedev;
	drv->device = device;
	drv->regs = regs;

	drv->osh = osh;

	/* Allocate chip state */
	if (!(drv->pciedev = pciedev_attach(drv, VENDOR_BROADCOM, device, osh, regs, bus))) {
		PCI_ERROR(("pciedev_probe: pciedev_attach failed\n"));
		goto fail;
	}
#ifndef HNDRTE_POLLING
	/* PCIE Mailbox ISR */
	if (hndrte_add_isr(0, coreid, unit, (isr_fun_t)pciedev_isr, rtedev, bus)) {
		PCI_ERROR(("pciedev_probe: hndrte_add_isr failed\n"));
		pciedev_detach(drv->pciedev);
		goto fail;
	}
#ifdef PCIE_PHANTOM_DEV
	/* SD DAM ISR */
	if (hndrte_add_isr(0, SDIOD_CORE_ID, unit, (isr_fun_t)sd_dma_isr, rtedev, bus)) {
		PCI_ERROR(("pciedev_probe: hndrte_add_isr failed for sd_dma\n"));
		pciedev_detach(drv->pciedev);
		goto fail;
	}
	/* USB DMA ISR */
	if (hndrte_add_isr(0, USB20D_CORE_ID, unit, (isr_fun_t)usb_dma_isr, rtedev, bus)) {
		PCI_ERROR(("pciedev_probe: hndrte_add_isr failed for sd_dma\n"));
		pciedev_detach(drv->pciedev);
		goto fail;
	}
#endif /* PCIE_PHANTOM_DEV */
#endif	/* !HNDRTE_POLLING */
	drv->dngl = pciedev_dngl(drv->pciedev);

	sprintf(rtedev->name, rstr_fmt_devname, found);
	printf(rstr_fmt_banner, rtedev->name);

	found++;

	return (void*)drv;

fail:
	return NULL;
}
int
pciedev_bus_binddev(void *bus, void *dev)
{
	drv_t *drv = ((hndrte_dev_t *)bus)->softc;
	return dngl_binddev(drv->dngl, bus, dev);
}
int
pciedev_bus_unbinddev(void *bus, void *dev)
{
	drv_t *drv = ((hndrte_dev_t *)bus)->softc;
	return dngl_unbinddev(drv->dngl, bus, dev);
}

int
pciedev_bus_validatedev(void *bus, void *dev)
{
	drv_t *drv = ((hndrte_dev_t *)bus)->softc;
	return dngl_validatedev(drv->dngl, bus, dev);
}

static int
BCMATTACHFN(pciedev_open)(hndrte_dev_t *dev)
{
	drv_t *drv = dev->softc;

	PCI_TRACE(("pciedev_open:\n"));

#ifdef RSOCK
	/* init the dongle state */
	dngl_init(drv->dngl);
#endif // endif

	bzero(&drv->dpcTimer, sizeof(drv->dpcTimer));
	drv->dpcTimer.mainfn = _pciedev_dpctask;
	drv->dpcTimer.data = drv;

	/* Initialize chip */
	pciedev_init(drv->pciedev);

	return 0;
}

/** dpc for pcie core */
static void
_pciedev_dpc(drv_t *drv)
{
	if (pciedev_dpc(drv->pciedev)) {
		if (!hndrte_add_timer(&drv->dpcTimer, 0, FALSE)) {
			DBG_BUS_INC(drv->pciedev, _pciedev_dpc);
			ASSERT(FALSE);
		}
	/* re-enable interrupts */
	} else {
		pciedev_intrson(drv->pciedev);
	}
}

static void
_pciedev_dpctask(hndrte_timer_t *timer)
{
	drv_t *drv = (drv_t *)timer->data;

	pciedev_intrsupd(drv->pciedev);
	_pciedev_dpc(drv);
}
#ifdef PCIE_PHANTOM_DEV

/** dpc functions for usb and sdio core */
static void
sd_dma_isr(hndrte_dev_t *rtedev)
{
	drv_t *drv = rtedev->softc;

	ASSERT(drv->pciedev);
	pcie_phtm_sd_isr(drv->pciedev);
}

static void
usb_dma_isr(hndrte_dev_t *rtedev)
{
	drv_t *drv = rtedev->softc;

	pcie_phtm_usb_isr(drv->pciedev);
}
#endif /* PCIE_PHANTOM_DEV */

/** ISR for pcie interrupt */
static void
pciedev_isr(hndrte_dev_t *rtedev)
{
	drv_t *drv = rtedev->softc;
	ASSERT(drv->pciedev);
	PCI_TRACE(("pciedev_isr is called\n"));
	/* call common first level interrupt handler */
	if (pciedev_dispatch(drv->pciedev)) {
		/* if more to do... */
		pciedev_intrsoff(drv->pciedev);
		_pciedev_dpc(drv);
	}
}

/** close pcie device */
int
BCMATTACHFN(pciedev_close)(hndrte_dev_t *dev)
{
#ifndef BCMNODOWN
	drv_t *drv = dev->softc;

	PCI_TRACE(("pciedev_close: drv exit%d\n", drv->unit));
	pciedev_detach(drv->pciedev);
	osl_detach(drv->osh);
#endif /* BCMNODOWN */

	return 0;
}

/** Forwards a packet towards the host */
static int
pciedev_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb)
{
	drv_t *drv = dev->softc;
#ifdef PKTC_FDAP
	void *n;
	struct dngl *dngl = drv->dngl;

	FOREACH_CHAINED_PKT(lb, n) {
		PKTCLRCHAINED(drv->osh, lb);
		PKTCCLRFLAGS(lb);
		dngl_sendpkt((void *)dngl, src, (void *)lb);
	}
	return 0;
#else
	return dngl_sendpkt((void *)(drv->dngl), src, (void *)lb);
#endif // endif
}
static void
pciedev_txflowcontrol(hndrte_dev_t *dev, bool state, int prio)
{
	return;
}

/**
 * PROP_TXSTATUS specific function. Called when the WL layer wants to report a flow control related
 * event (eg MAC_OPEN), this function consumes (terminates) those events, which is a difference
 * compared to eg USB dongles, in which case the host instead of firmware terminates the events.
 */
int pciedev_bus_flring_ctrl(void *dev, uint32 op, void *data)
{
	if (dev != NULL && data != NULL) {
		drv_t *drv = ((hndrte_dev_t *)dev)->softc;
		struct dngl_bus *pciedev = (struct dngl_bus *)drv->pciedev;
		flowring_op_data_t	*op_data = (flowring_op_data_t *)data;

		switch (op) {
		case WLFC_CTL_TYPE_MAC_OPEN:
		case WLFC_CTL_TYPE_MAC_CLOSE:
			pciedev_upd_flr_port_handle(pciedev, op_data->handle,
				(op == WLFC_CTL_TYPE_MAC_OPEN));
			break;

		case WLFC_CTL_TYPE_MACDESC_ADD:
		case WLFC_CTL_TYPE_MACDESC_DEL:
			pciedev_upd_flr_hanlde_map(pciedev, op_data->handle,
				(op == WLFC_CTL_TYPE_MACDESC_ADD), op_data->addr);
			break;

		case WLFC_CTL_TYPE_INTERFACE_OPEN:
		case WLFC_CTL_TYPE_INTERFACE_CLOSE:
			pciedev_upd_flr_if_state(pciedev, op_data->ifindex,
				(op == WLFC_CTL_TYPE_INTERFACE_OPEN));
			break;

		case WLFC_CTL_TYPE_TID_OPEN:
		case WLFC_CTL_TYPE_TID_CLOSE:
			pciedev_upd_flr_tid_state(pciedev, op_data->tid,
				(op == WLFC_CTL_TYPE_TID_OPEN));
			break;

		case WLFC_CTL_TYPE_MAC_REQUEST_PACKET:
			pciedev_process_reqst_packet(pciedev, op_data->handle,
				op_data->tid, op_data->minpkts);
			break;

		case WLFC_CTL_TYPE_UPD_FLR_WEIGHT:
			pciedev_upd_flr_weigth(pciedev, op_data->handle,
				op_data->tid, op_data->extra_params);
			break;

		case WLFC_CTL_TYPE_ENAB_FFSCH:
			pciedev_set_ffsched(pciedev, op_data->extra_params);
			break;

		default :
			PCI_ERROR(("Need to handle flow control operation %d\n", op));
		}
	}
	return 0;
}

static int
pciedev_ioctl(hndrte_dev_t *rtedev, uint32 cmd, void *buf, int len, int *used, int *needed, int set)
{
	drv_t *drv = (drv_t *)rtedev->softc;
	int ret = BCME_OK;
	uint32 outlen = 0;
	uint32 d11rxoffset;

	switch (cmd) {
	case BUS_GET_VAR:
	case BUS_SET_VAR:
		ASSERT((cmd == BUS_GET_VAR) == !set);
		if (strncmp((char *)buf, "bus:", strlen("bus:"))) {
			ret = BCME_ERROR;
			break;
		}

		ret = pciedev_bus_iovar(drv->pciedev, (char *)buf, len, &outlen, set);
		break;
	case BUS_FLOW_FLUSH_PEND:
		ret = pciedev_flow_ring_flush_pending(drv->pciedev, (char*) buf, len, &outlen);
		break;
	case BUS_SET_RX_OFFSET:
		d11rxoffset = ((uint32*)buf)[0];

		pciedev_set_rxoffset_bytes(drv->pciedev, d11rxoffset);
		break;
#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	case BUS_SET_MONITOR_MODE:
		{
			uint32 monitor_mode = 0;
			monitor_mode = (uint8) *((uint32*)buf);
			pciedev_set_monitor_mode(drv->pciedev, monitor_mode);
			break;
		}
#endif /* WL_MONITOR && WL_MONITOR_DISABLED */
	default:
		ret = BCME_ERROR;
	}

	if (used)
		*used = outlen;

	return ret;
}
