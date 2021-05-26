/*
 * RTE layer for pcie device
 * hnd_dev_ops_t & dngl_bus_ops for pciedev are defined here
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
#include <osl_ext.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <dngl_bus.h>
#include <dngl_api.h>
#include <circularbuf.h>
#include <bcmpcie.h>
#include <bcmmsgbuf.h>
#include <pciedev.h>
#include <pciedev_priv.h>
#include <pciedev_dbg.h>
#include <hndsoc.h>
#include <wlfc_proto.h>
#include <flring_fc.h>
#include <rte_dev.h>
#include <rte_isr.h>
#include <rte_ioctl.h>
#include <rte_gpio.h>
#include <hnd_cplt.h>
#include <pciedev_rte.h>

typedef struct {
	int unit;
	hnd_dev_t *rtedev;
	struct dngl_bus *pciedev;
	osl_t *osh;
	volatile void *regs;
	uint16 device;
	struct dngl *dngl;
} drv_t;

/* Reclaimable strings */
static const char BCMATTACHDATA(rstr_fmt_banner)[] = "%s: Broadcom PCIE MSGBUF driver\n";
static const char BCMATTACHDATA(rstr_fmt_devname)[] = "pciemsgbuf%d";

/* Driver entry points */
static void *pciedev_probe(hnd_dev_t *dev, volatile void *regs, uint bus, uint16 device,
                          uint coreid, uint unit);
#ifndef RTE_POLL
static bool pciedev_isr(void *cbdata);
static bool pciedev_worklet(void *cbdata);
#endif /* !RTE_POLL */

static void pciedev_run(hnd_dev_t *ctx);
static int pciedev_open(hnd_dev_t *dev);
static int pciedev_send(hnd_dev_t *src, hnd_dev_t *dev, struct lbuf *lb);
static void pciedev_txflowcontrol(hnd_dev_t *dev, bool state, int prio);
static int pciedev_close(hnd_dev_t *dev);
static int pciedev_ioctl(hnd_dev_t *dev, uint32 cmd, void *buf, int len,
	int *used, int *needed, int set);

static hnd_dev_ops_t pciedev_funcs = {
	probe:		pciedev_probe,
	open:		pciedev_open,
	close:		pciedev_close,
	xmit:		pciedev_send,
	ioctl:		pciedev_ioctl,
	txflowcontrol:	pciedev_txflowcontrol,
	poll:		pciedev_run
};

static struct dngl_bus_ops pciedev_bus_ops = {
	rebinddev:	pciedev_bus_rebinddev,
	binddev:	pciedev_bus_binddev,
	sendctl:	pciedev_bus_sendctl,
	iovar:		pciedev_bus_iovar,
	unbinddev:	pciedev_bus_unbinddev,
	tx:		pciedev_create_d2h_messages_tx,
	flowring_ctl:	pciedev_bus_flring_ctrl,
	sendctl_tx:	pciedev_bus_sendctl_tx,
	rebind_if:	pciedev_bus_rebind_if,
	findif:		pciedev_bus_findif,
	validatedev:	pciedev_bus_validatedev,
	maxdevs_reached:	pciedev_bus_maxdevs_reached
};

struct dngl_bus_ops *
BCMRAMFN(get_pciedev_bus_ops)(void)
{
	return &pciedev_bus_ops;
}

static hnd_dev_t pciedev_dev = {
	name:		"pciedev",
	ops:		&pciedev_funcs
};

hnd_dev_t *
BCMRAMFN(get_pciedev_dev)(void)
{
	return &pciedev_dev;
}

/** Number of devices found */
static int found = 0;
int pci_msg_level = PCI_ERROR_VAL;

static uint32
pciedev_handle_trap(hnd_dev_t *rtedev, uint8 trap_type)
{
	drv_t *drv = (drv_t *)rtedev->softc;

	return pciedev_halt_device(drv->pciedev);
}

bool
pciedev_bus_maxdevs_reached(void *bus)
{
	drv_t *drv = ((hnd_dev_t *)bus)->softc;
	return dngl_maxdevs_reached(drv->dngl);
}

static const char BCMATTACHDATA(rstr_pciedev_probe_malloc_failed)[] =
	"pciedev_probe: malloc failed\n";

/** probe function for pcie device */
static void *
BCMATTACHFN(_pciedev_probe)(hnd_dev_t *rtedev, volatile void *regs, uint bus, uint16 device,
                        uint coreid, uint unit)
{
	drv_t *drv;
	osl_t *osh = NULL;

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
		goto fail1;
	}

#ifndef RTE_POLL
	/* PCIE Mailbox ISR */
	if (hnd_isr_register(coreid, unit, bus,
		pciedev_isr, rtedev, pciedev_worklet, rtedev, NULL) == NULL) {
		PCI_ERROR(("pciedev_probe: hnd_isr_register failed\n"));
		goto fail2;
	}
#endif	/* !RTE_POLL */
	if (hnd_register_trapnotify_callback(pciedev_handle_trap, rtedev) < 0) {
		PCI_ERROR(("pciedev_probe: Register trap callback failed\n"));
		goto fail2;
	}

	drv->dngl = pciedev_dngl(drv->pciedev);

	(void)snprintf(rtedev->name, sizeof(rtedev->name), rstr_fmt_devname, found);
	printf(rstr_fmt_banner, rtedev->name);

	found++;

	return (void*)drv;
fail2:
	MODULE_DETACH(drv->pciedev, pciedev_detach);
fail1:
	MFREE(osh, drv, sizeof(*drv));
	drv = NULL;
fail:
	osl_detach(osh);
	osh = NULL;
	return NULL;
} /* _pciedev_probe */

/* Declare a runtime flag with value 0 to trick the compiler
 * to not complain "defined but not used" warning...and also
 * to allow the optimizer to remove any code referenced under
 * 'if (0) {}' only.
 */
BCM_ATTACH_REF_DECL()

/* pciedev_probe is wrapper function and is forced to be non discard so that
 * it can be reference in a gloabl non discarded data structure which
 * otherwise will trigger the linker error "X referenced in section Y of Z.o:
 * defined in discarded section xxx of Z.o".
 * Use the 'if (0) {}' trick mentioned above to remove the wrapped
 * function in builds where it is not needed such as in ROML builds
 * but in builds where the wrapped function is actually needed such as
 * in RAM or ROM offload builds add the wrapped function in reclaim_protect.cfg
 * to skip the discarded code referenced in non discarded section error.
 * Keep this function out of ROML builds to force it in RAM in ROM offload builds.
 */
static void *
BCMRAMFN(pciedev_probe)(hnd_dev_t *rtedev, volatile void *regs, uint bus, uint16 device,
                        uint coreid, uint unit)
{
	if (BCM_ATTACH_REF()) {
		return _pciedev_probe(rtedev, regs, bus, device, coreid, unit);
	}
	return NULL;
}

void
pciedev_bus_rebinddev(void *bus, void *dev, int ifindex)
{
	drv_t *drv = ((hnd_dev_t *)bus)->softc;
	dngl_rebinddev(drv->dngl, bus, dev, ifindex);
}
int
pciedev_bus_findif(void *bus, void *dev)
{
	drv_t *drv = ((hnd_dev_t *)bus)->softc;
	return dngl_findif(drv->dngl, dev);
}

int
pciedev_bus_rebind_if(void *bus, void *dev, int idx, bool rebind)
{
	drv_t *drv = ((hnd_dev_t *)bus)->softc;
	return dngl_rebind_if(drv->dngl, dev, idx, rebind);
}
int
pciedev_bus_binddev(void *bus, void *dev, uint numslaves)
{
	drv_t *drv = ((hnd_dev_t *)bus)->softc;
	return dngl_binddev(drv->dngl, bus, dev, numslaves);
}

int
pciedev_bus_unbinddev(void *bus, void *dev)
{
	drv_t *drv = ((hnd_dev_t *)bus)->softc;
	return dngl_unbinddev(drv->dngl, bus, dev);
}

/* Don't make it BCMATTACHFN, it is called after reclaim */
static int
pciedev_open(hnd_dev_t *dev)
{
	drv_t *drv = dev->softc;
	int bcmerror;

	PCI_TRACE(("pciedev_open: %s\n", dev->name));

	/* Initialize DMA */
	bcmerror = pciedev_pciedma_init(drv->pciedev);
	if (bcmerror) {
		PCI_TRACE(("pciedev_attach: failed initing the PCIE DMA engines \n"));
		return -1;
	}

	/* Initialize chip */
	pciedev_init(drv->pciedev);

	return 0;
}

#ifndef RTE_POLL

/** Primary PCIE ISR */
static bool
pciedev_isr(void *cbdata)
{
	hnd_dev_t *rtedev = cbdata;
	drv_t *drv = rtedev->softc;

	/* Disable interrupts */
	pciedev_intrsoff(drv->pciedev);

	/* Request worklet */
	return TRUE;
}

/** Primary PCIE DPC function */
static bool
pciedev_worklet(void *cbdata)
{
	hnd_dev_t *rtedev = cbdata;

	pciedev_run(rtedev);

	/* Don't reschedule */
	return FALSE;
}

#endif /* !RTE_POLL */

static void
pciedev_run(hnd_dev_t *rtedev)
{
	OSL_INTERRUPT_SAVE_AREA

	drv_t *drv = rtedev->softc;
	ASSERT(drv->pciedev);

	if (pciedev_dispatch(drv->pciedev)) {
		if (pciedev_handle_interrupts(drv->pciedev)) {
			/* More work to be done, reschedule this function */
			hnd_worklet_reschedule();
			return;
		}
	}

	/* Enable interrupts */
	OSL_DISABLE
	pciedev_intrson(drv->pciedev);
	OSL_RESTORE
}

/** close pcie device */
static int
BCMATTACHFN(_pciedev_close)(hnd_dev_t *dev)
{
#ifndef BCMNODOWN
	drv_t *drv = dev->softc;

	PCI_TRACE(("pciedev_close: drv exit%d\n", drv->unit));

	MODULE_DETACH(drv->pciedev, pciedev_detach);
	osl_detach(drv->osh);
#endif /* BCMNODOWN */

	return 0;
}

/* pciedev_close is wrapper function and is forced to be non discard so that
 * it can be reference in a gloabl non discarded data structure which
 * otherwise will trigger the linker error "X referenced in section Y of Z.o:
 * defined in discarded section xxx of Z.o".
 * Use the 'if (0) {}' trick mentioned above to remove the wrapped
 * function in builds where it is not needed such as in ROML builds
 * but in builds where the wrapped function is actually needed such as
 * in RAM or ROM offload builds add the wrapped function in reclaim_protect.cfg
 * to skip the discarded code referenced in non discarded section error.
 * Keep this function out of ROML builds to force it in RAM in ROM offload builds.
 */
static int
BCMRAMFN(pciedev_close)(hnd_dev_t *dev)
{
	if (BCM_ATTACH_REF()) {
		return _pciedev_close(dev);
	}
	return BCME_OK;
}

/** Forwards a packet towards the host */
static int
pciedev_send(hnd_dev_t *src, hnd_dev_t *dev, struct lbuf *lb)
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
pciedev_txflowcontrol(hnd_dev_t *dev, bool state, int prio)
{
	return;
}

int
pciedev_bus_sendctl_tx(void *dev, uint8 type, uint32 op, void *p)
{
	drv_t *drv = ((hnd_dev_t *)dev)->softc;
	struct dngl_bus *pciedev = (struct dngl_bus *)drv->pciedev;

	p = PKTFRMNATIVE(pciedev->osh, p);
	return pciedev_sendctl_tx(pciedev, type, op, p);

}

/**
 * PROP_TXSTATUS specific function. Called when the WL layer wants to report a flow control related
 * event (eg MAC_OPEN), this function consumes (terminates) those events, which is a difference
 * compared to eg USB dongles, in which case the host instead of firmware terminates the events.
 */
int pciedev_bus_flring_ctrl(void *dev, uint32 op, void *data)
{
	drv_t *drv = ((hnd_dev_t *)dev)->softc;
	struct dngl_bus *pciedev = (struct dngl_bus *)drv->pciedev;
	flowring_op_data_t	*op_data = (flowring_op_data_t *)data;

	PCI_TRACE(("pciedev_bus_flring_ctrl: flowid:%d op:%d ifidx:%d TID:%d "
			"DA:%02x:%02x:%02x:%02x:%02x:%02x\n",
			op_data->flowid, op, op_data->ifindex, op_data->tid,
			op_data->addr[0], op_data->addr[1], op_data->addr[2],
			op_data->addr[3], op_data->addr[4], op_data->addr[5]));

	switch (op) {
		case WLFC_CTL_TYPE_MAC_OPEN:
		case WLFC_CTL_TYPE_MAC_CLOSE:
			pciedev_upd_flr_port_handle(pciedev, op_data->handle,
				(op == WLFC_CTL_TYPE_MAC_OPEN), op_data->flags);
			break;

		case WLFC_CTL_TYPE_MACDESC_ADD:
		case WLFC_CTL_TYPE_MACDESC_DEL:
			pciedev_upd_flr_hanlde_map(pciedev, op_data->handle, op_data->ifindex,
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

		case WLFC_CTL_TYPE_UPDATE_FLAGS:
			pciedev_update_flr_flags(pciedev, op_data->addr, op_data->flags);
			break;

		case WLFC_CTL_TYPE_CLEAR_SUPPR:
			pciedev_clear_flr_supr_info(pciedev, op_data->addr, op_data->tid);
			break;

		case WLFC_CTL_TYPE_FLOWID_OPEN:
		case WLFC_CTL_TYPE_FLOWID_CLOSE:
			pciedev_upd_flr_flowid_state(pciedev, op_data->flowid,
				(op == WLFC_CTL_TYPE_FLOWID_OPEN));
			break;
#ifdef WLSQS
		case WLFC_CTL_TYPE_SQS_STRIDE:
			pciedev_sqs_stride_resume(pciedev, op_data->handle);
			break;
#endif // endif
		default :
			PCI_ERROR(("Need to handle flow control operation %d\n", op));
	}
	return 0;
}

int
pciedev_bus_validatedev(void *bus, void *dev)
{
	drv_t *drv = ((hnd_dev_t *)bus)->softc;
	return dngl_validatedev(drv->dngl, bus, dev);
}

static int
pciedev_ioctl(hnd_dev_t *rtedev, uint32 cmd, void *buf, int len, int *used, int *needed, int set)
{
	drv_t *drv = (drv_t *)rtedev->softc;
	int ret = BCME_OK;
	uint32 outlen = 0;
	struct dngl_bus *pciedev = (struct dngl_bus *)drv->pciedev;
	uint16 start;
	uint32 cnt;
	uint8 state;
	uint32 copycount = 0;
	uint32 d11rxoffset = 0;

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
	case BUS_FLUSH_RXREORDER_Q:
		ASSERT(buf);
		ASSERT(len == 2*sizeof(uint32));
		start = (uint16) (((uint32*)buf)[0]);
		cnt = ((uint32*)buf)[1];
		pciedev_rxreorder_queue_flush_cb((void *)pciedev, start, cnt);
		break;
	case BUS_SET_LTR_STATE:
		ASSERT(buf);
		state = (uint8) *((uint32*)buf);
		pciedev_send_ltr((void *)pciedev, state);
		break;
	case BUS_FLUSH_CHAINED_PKTS:
		pciedev_flush_chained_pkts(pciedev);
		break;
	case BUS_SET_COPY_COUNT:
		copycount  = (((uint32*)buf)[0]);
		d11rxoffset = ((uint32*)buf)[1];

		pciedev_set_copycount_bytes(drv->pciedev, copycount, d11rxoffset);
		break;
	case BUS_UPDATE_EXTRA_TXLFRAGS:
		pciedev_extra_txlfrag_requirement((drv->pciedev), (uint16) *((uint32*)buf));
		break;
	case BUS_UPDATE_FLOW_PKTS_MAX:
		pciedev_update_txflow_pkts_max(drv->pciedev);
		break;
	case BUS_UPDATE_FRWD_RESRV_BUFCNT:
	{
		uint16 frwd_resrv_bufcnt = (((uint16*)buf)[0]);
		pciedev_set_frwd_resrv_bufcnt(drv->pciedev, frwd_resrv_bufcnt);
		break;
	}
	case BUS_FLOW_FLUSH_PEND:
		ret = pciedev_flow_ring_flush_pending(drv->pciedev, (char*) buf, len, &outlen);
		break;
#ifdef WLCFP
	case BUS_CFP_FLOW_DELINK :
	{
		uint16 flowid = *((uint16*)buf);
		pciedev_cfp_flow_delink(drv->pciedev, flowid);
		break;
	}
#endif // endif
#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	case BUS_SET_MONITOR_MODE:
	{
		uint32 monitor_mode = 0;
		monitor_mode = (uint8) *((uint32*)buf);
		pciedev_set_monitor_mode(drv->pciedev, monitor_mode);
		break;
	}
#endif /* WL_MONITOR && WL_MONITOR_DISABLED */
	case BUS_SBTOPCIE_ACCESS_START:
	{
		/* Start the sbtopcie access */
		pciedev_sbtopcie_access_start(drv->pciedev, (sbtopcie_info_t*)buf);
		break;
	}
	case BUS_SBTOPCIE_ACCESS_STOP:
	{
		uint64 base_bkp64 = *((uint64*)buf);
		/* Stop the sbtopcie access */
		pciedev_sbtopcie_access_stop(drv->pciedev, base_bkp64, (base_bkp64 >> 32));
		break;
	}
	case BUS_TAF_SCHEDULER_CONFIG:
	{
		bool taf_enable = (*((bool*)buf)) ? TRUE : FALSE;
		pciedev->taf_scheduler = taf_enable;
	}
	default:
		ret = BCME_ERROR;
	}

	if (used)
		*used = outlen;

	return ret;
}

pciedev_gpioh_t *
pciedev_gpio_handler_register(uint32 event, bool level,
	pciedev_gpio_handler_t cb, void *arg)
{
	return (pciedev_gpioh_t *)rte_gpio_handler_register(event, level, cb, arg);
}

void
pciedev_gpio_handler_unregister(pciedev_gpioh_t *gi)
{
	rte_gpio_handler_unregister((rte_gpioh_t *)gi);
}

osl_t* pciedev_get_osh_handle(void)
{
	return (osl_t *) ((drv_t *) (pciedev_dev.softc))->osh;
}

struct dngl_bus* pciedev_get_handle(void)
{
	return (struct dngl_bus *) ((drv_t *) (pciedev_dev.softc))->pciedev;
}
