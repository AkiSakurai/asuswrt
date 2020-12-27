/*
 * Broadcom PCIE device-side driver
 * pciedev is the bus device for pcie full dongle
 * Core bus operations are as below
 * 1. interrupt mechanism
 * 2. circular buffer
 * 3. message buffer protocol
 * 4. DMA handling
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: pciedev.c  $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmnvram.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <osl.h>
#include <hndsoc.h>
#include <proto/ethernet.h>
#include <proto/802.1d.h>
#include <sbsdio.h>
#include <pcie_core.h>
#include <sbphantom.h>
#include <dngl_bus.h>
#include <dngl_api.h>
#include <bcmcdc.h>
#include <msgtrace.h>
#include <bcmpcie.h>
#include <bcmmsgbuf.h>
#include <bcmotp.h>
#include <hndpmu.h>
#include <pciedev_priv.h>
#include <pciedev.h>
#include <pciedev_dbg.h>
#include <pcicfg.h>
#include <dngl_rte.h>
#include <event_log.h>
#include <logtrace.h>
#include <wlfc_proto.h>
#include <bcm_buzzz.h>

#ifdef PCIE_PHANTOM_DEV
# error "Phantom device builds are not supported on this twig"
#endif

/* Test flags to try out the metadata code path */

#ifdef TEST_DROP_PCIEDEV_RX_FRAMES
static uint32 pciedev_test_drop_rxframe_max = 5000;
static uint32 pciedev_test_drop_norxcpl_max = 100;
static uint32 pciedev_test_drop_rxframe = 0;
static uint32 pciedev_test_drop_norxcpl = 0;
static uint32 pciedev_test_dropped_norxcpls = 0;
static uint32 pciedev_test_dropped_rxframes = 0;
#endif /* TEST_DROP_PCIEDEV_RX_FRAMES */


#define WAR2_HWJIRA_CRWLPCIEGEN2_162

/* enable the WAR for CRWLPCIEGEN2_160, this uses the knowledge of existing war for 162  */
#define WAR_HWJIRA_CRWLPCIEGEN2_160
#define PCIEGEN2_COE_PVT_TL_CTRL_0			0x800
#define COE_PVT_TL_CTRL_0_PM_DIS_L1_REENTRY_BIT		24

#define PCIEGEN2_PVT_REG_PM_CLK_PERIOD			0x184c
#define PMCR_TREFUP_DEFAULT			6 /* in useconds */
#define PMCR_TREFUP_TPOWERON_OFFSET	5 /* in useconds */

/* PWR management stuff exists from pcierev 7 */
/* Loopback support not added for Phantom configs */
#ifdef PCIE_PHANTOM_DEV

#ifdef PCIE_PWRMGMT_CHECK
#undef PCIE_PWRMGMT_CHECK
#endif /* PCIE_PWRMGMT_CHECK */
#ifdef PCIE_DMAXFER_LOOPBACK
#undef PCIE_DMAXFER_LOOPBACK
#endif /* PCIE_DMAXFER_LOOPBACK */
#endif /* PCIE_PHANTOM_DEV */

#define TXSTATUS_LEN           1

#if defined(BCMPCIEDEV_ENABLED) && !defined(BCMMSGBUF)
#error "Bad configuration, for PCIE FULL DONGLE build Need BCMMSGBUF"
#endif

/* Change this to MAX TX FLOWRINGS */
#define MAX_SUPPORTED_FLOW_RINGS	BCMPCIE_MAX_TX_FLOWS

/* enable when you wnat to print contents of the node
static void queue_dump(void *p)
{
	flowring_pool_t * queue = (flowring_pool_t *)p;
	printf("\t\tqueue pool <%p> queue<%p>\n", queue, queue->ring);
}
*/

static void pciedev_no_ds_dw_deassrt(void *handle);
static void pciedev_no_ds_perst_assrt(void *handle);
static void pciedev_ds_check_dw_assrt(void *handle);
static void pciedev_ds_check_perst_assrt(void *handle);
static void pciedev_ds_check_ds_allowed(void *handle);
static void pciedev_ds_d0_dw_assrt(void *handle);
static void pciedev_ds_d0_db_dtoh(void *handle);
static void pciedev_ds_nods_d3cold_perst_dassrt(void *handle);
static void pciedev_ds_d3cold_dw_assrt(void *handle);
static void pciedev_ds_nods_d3cold_dw_dassrt(void *handle);
static void pciedev_ds_d3cold_hw_assrt(void *handle);
static void pciedev_ds_d3cold_perst_dassrt(void *handle);
static int pciedev_deepsleep_engine_log_dump(struct dngl_bus * pciedev);
static void pciedev_deepsleep_check_periodic(struct dngl_bus *pciedev);
static const char * pciedev_ds_state_name(bcmpcie_deepsleep_state_t state);
static const char * pciedev_ds_event_name(bcmpcie_deepsleep_event_t event);
static uint32 pciedev_get_pwrstats(struct dngl_bus *pciedev, char *buf);

static pciedev_ds_state_tbl_entry_t pciedev_ds_state_tbl[DS_LAST_STATE][DS_LAST_EVENT] = {
	{ /* state: NO_DS_STATE */
		{NULL, DS_INVALID_STATE}, /* event: DW_ASSRT */
		{pciedev_no_ds_dw_deassrt, DS_CHECK_STATE}, /* event: DW_DASSRT */
		{pciedev_no_ds_perst_assrt, NODS_D3COLD_STATE}, /* event: PERST_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_DASSRT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}  /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: DS_CHECK_STATE */
		{pciedev_ds_check_dw_assrt, NO_DS_STATE}, /* event: DW_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DW_DASSRT_EVENT */
		{pciedev_ds_check_perst_assrt, DS_D3COLD_STATE}, /* event: PERST_ASSRT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_DASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH_EVENT */
		{pciedev_ds_check_ds_allowed, DS_D0_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, NO_DS_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE} /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: DS_D0 */
		{pciedev_ds_d0_dw_assrt, NO_DS_STATE}, /* event: DW_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DW_DASSRT_EVENT */
		{NULL, DS_D3COLD_STATE},  /* event: PERST_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_DASSRT_EVENT */
		{pciedev_ds_d0_db_dtoh, DS_CHECK_STATE}, /* event: DB_TOH_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}  /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: NODS_D3COLD */
		{NULL, DS_INVALID_STATE}, /* event: DW_ASSRT_EVENT */
		{pciedev_ds_nods_d3cold_dw_dassrt, DS_D3COLD_STATE}, /* event: DW_DASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_ASSRT_EVENT */
		{pciedev_ds_nods_d3cold_perst_dassrt, NO_DS_STATE}, /* event: PERST_DASSRT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}  /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: DS_D3COLD */
		{pciedev_ds_d3cold_dw_assrt, NODS_D3COLD_STATE}, /* event: DW_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DW_DASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_ASSRT_EVENT */
		{pciedev_ds_d3cold_perst_dassrt, NO_DS_STATE}, /* event: PERST_DASSRT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{pciedev_ds_d3cold_hw_assrt, NODS_D3COLD_STATE} /* event: HOSTWAKE_ASSRT_EVENT */
	}
};

#define SCBHANDLE_PS_STATE_MASK (1 << 8)
#define SCBHANDLE_INFORM_PKTPEND_MASK (1 << 9)
#define SCBHANDLE_MASK (0xff)

typedef struct {
	void* assoc_lfrag;
	uint32 assoc_lfrag_len;
} pcie_pkttag_t;

typedef struct pcie_dma_param {
	uint32	src_low;
	uint32	src_high;
	uint32	dest_low;
	uint32	dest_high;
	uint32 	xfer_size;
} pcie_dma_param_t;

#define PCIE_PKTTAG(p) ((pcie_pkttag_t *)PKTTAG(p))

static void pciedev_send_ioctlack(struct dngl_bus *bus, uint16 status);
static void pciedev_send_ring_status(struct dngl_bus *pciedev, uint16 w_idx,
	uint32 status, uint16 ring_id);
static void pciedev_send_general_status(struct dngl_bus *pciedev, uint16 ring_id,
	uint32 status, uint32 req_id);
static void pciedev_tunables_init(struct dngl_bus *pciedev);
static bool pciedev_match(uint vendor, uint device);
static void pciedev_reset(struct dngl_bus *pciedev);

#ifdef PCIE_DELAYED_HOSTWAKE
static void pciedev_delayed_hostwake_timerfn(dngl_timer_t *t);
static void pciedev_delayed_hostwake(struct dngl_bus *pciedev, uint32 val);
static void pciedev_delayed_hostwake_timerfn(dngl_timer_t *t);
#endif /* PCIE_DELAYED_HOSTWAKE */
#ifdef PCIE_PWRMGMT_CHECK
static void pciedev_handle_d0_enter(struct dngl_bus *pciedev);
static void pciedev_handle_l0_enter(struct dngl_bus *pciedev);
static int pciedev_handle_d0_enter_bm(struct dngl_bus *pciedev);
#endif /* PCIE_PWRMGMT_CHECK */
static void pciedev_process_ioctlptr_rqst(struct dngl_bus *pciedev, void* p);
#ifndef PCIE_PHANTOM_DEV
static void pciedev_usrltrtest_timerfn(dngl_timer_t *t);
static int pciedev_pciedma_init(struct dngl_bus *p_pcie_dev);
static void pciedev_reset_pcie_interrupt(struct dngl_bus * pciedev);
static uint pd_msglevel = 2;
#endif
static int pciedev_xmit_wlevnt_cmplt(struct dngl_bus *pciedev,
	uint32 bufid, uint16 buflen, uint8 ifidx, msgbuf_ring_t *msgbuf);
static uint BCMATTACHFN(pciedev_api_shmem_init)(struct dngl_bus *pciedev);

#ifndef PCIE_PHANTOM_DEV
static void pciedev_hw_LTR_war(struct dngl_bus *pciedev);
static void pciedev_set_LTRvals(struct dngl_bus *pciedev);
static uint32 pciedev_usr_test_ltr(struct dngl_bus *pciedev, uint32 timer_val, uint8 state);
static void pciedev_send_ltr(void *dev, uint8 state);
#endif /* PCIE_PHANTOM_DEV */


#define PCIE_LPBK_SUPPORT_TXQ_SIZE	256
#define PCIE_LPBK_SUPPORT_TXQ_NPREC	1
#define PCIE_WAKE_SUPPORT_TXQ_NPREC	1

static void pciedev_send_ioctl_completion(struct dngl_bus *pciedev, void *p);

/* mail box communication handlers */
#ifndef PCIE_PHANTOM_DEV
static void pciedev_handle_host_D3_info(struct dngl_bus *pciedev);
static void pciedev_handle_host_deepsleep_ack(struct dngl_bus *pciedev);
static void pciedev_handle_host_deepsleep_nak(struct dngl_bus *pciedev);
static void pciedev_deepsleep_enter_req(struct dngl_bus *pciedev);
static void pciedev_deepsleep_exit_notify(struct dngl_bus *pciedev);
static void pciedev_d2h_mbdata_send(struct dngl_bus *pciedev, uint32 data);
static void pciedev_h2d_mb_data_process(struct dngl_bus *pciedev);
static void pciedev_perst_reset_process(struct dngl_bus *pciedev);
static void pciedev_crwlpciegen2(struct dngl_bus *pciedev);
static void pciedev_crwlpciegen2_161(struct dngl_bus *pciedev, bool state);
static void pciedev_crwlpciegen2_61(struct dngl_bus *pciedev);
static void pciedev_crwlpciegen2_180(struct dngl_bus *pciedev);
static void pciedev_reg_pm_clk_period(struct dngl_bus *pciedev);

static void pciedev_enable_device_wake(struct dngl_bus *pciedev);
static void pciedev_disable_deepsleep(struct dngl_bus *pciedev, bool disable);
static uint8  pciedev_host_wake_gpio_init(struct dngl_bus *pciedev);
#endif /* PCIE_PHANTOM_DEV */

static bool pciedev_setup_common_msgbuf_rings(struct dngl_bus *pciedev);
#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
static bool pciedev_setup_txflow_msgbuf_ring(struct dngl_bus *pciedev);
#endif

#ifdef PCIE_PHANTOM_DEV
static uint16 pciedev_htoddma_pktlen(struct dngl_bus *pciedev, msgbuf_ring_t **msgbuf);
#endif
static uint32 pciedev_get_avail_host_rxbufs(struct dngl_bus *pciedev);
static void pciedev_sched_msgbuf_intr_process(dngl_timer_t *t);
static void pciedev_ioctpyld_fetch_cb(struct fetch_rqst *fr, bool cancelled);
static int pciedev_process_flow_ring_create_rqst(struct dngl_bus * pciedev, void *p);
static bool pciedev_setup_flow_rings(struct dngl_bus *pciedev);
#ifndef BCMPCIE_SUPPORT_TX_PUSH_RING
static void pciedev_free_flowrings(struct dngl_bus *pciedev);
#endif /* BCMPCIE_SUPPORT_TX_PUSH_RING */
static int pciedev_flush_flow_ring(struct dngl_bus * pciedev, uint16 flowid);
static int pciedev_process_flow_ring_delete_rqst(struct dngl_bus * pciedev, void *p);
static void pciedev_process_flow_ring_flush_rqst(struct dngl_bus * pciedev, void *p);
static void pciedev_free_msgbuf_flowring(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf);
static uint8 pciedev_dump_lclbuf_pool(msgbuf_ring_t * ring);
static lcl_buf_pool_t *pciedev_allocate_lcl_buffers(struct dngl_bus *pciedev, uint8 bufcnt,
	uint8 item_cnt, uint8 len_items);
static void pciedev_free_lcl_buffers(struct dngl_bus *pciedev,
	lcl_buf_pool_t * pool, uint16 item_len);
static uint16 pciedev_handle_h2d_msg_ctrlsubmit(struct dngl_bus *pciedev, void *p,
	uint16 pktlen, msgbuf_ring_t *msgbuf);

#ifdef PCIE_DMAXFER_LOOPBACK
static uint32 pciedev_process_do_dest_lpbk_dmaxfer(struct dngl_bus *pciedev);
static void pciedev_process_src_lpbk_dmaxfer_done(struct fetch_rqst *fr, bool cancelled);
static uint32 pciedev_process_do_src_lpbk_dmaxfer(struct dngl_bus *pciedev);
static uint32 pciedev_schedule_src_lpbk_dmaxfer(struct dngl_bus *pciedev);
static void pciedev_done_lpbk_dmaxfer(struct dngl_bus *pciedev);
static  uint32 pciedev_prepare_lpbk_dmaxfer(struct dngl_bus *pciedev, uint32 len);
static void pciedev_process_lpbk_dmaxfer_msg(struct dngl_bus *pciedev, void *p);
static void pciedev_send_lpbkdmaxfer_complete(struct dngl_bus *pciedev,
	uint16 status, uint32 req_id);
static void pciedev_lpbk_src_dmaxfer_timerfn(dngl_timer_t *t);
static void pciedev_lpbk_dest_dmaxfer_timerfn(dngl_timer_t *t);
#endif /* PCIE_DMAXFER_LOOPBACK */
static int pciedev_dump_txring_msgbuf_ring_info(msgbuf_ring_t *msgbuf);
static int pciedev_dump_prioring_info(struct dngl_bus * pciedev);
static int pciedev_dump_rw_blob(struct dngl_bus * pciedev);
static void pciedev_init_inuse_lclbuf_pool_t(struct dngl_bus *pciedev, msgbuf_ring_t *ring);

static void pciedev_free_inuse_bufpool(struct dngl_bus *pciedev, lcl_buf_pool_t * bufpool);
static void pciedev_health_check_timerfn(dngl_timer_t *t);
static void pciedev_bm_enab_timerfn(dngl_timer_t *t);

static void pciedev_deepsleep_engine(struct dngl_bus * pciedev, bcmpcie_deepsleep_event_t event);
static void pciedev_ds_check_timerfn(dngl_timer_t *t);
static bool pciedev_can_goto_deepsleep(struct dngl_bus * pciedev);
static uint32 pcie_h2ddma_tx_get_burstlen(struct dngl_bus *pciedev);
static int pciedev_tx_ioctl_pyld(struct dngl_bus *pciedev, void *p, ret_buf_t *data_buf,
	uint16 data_len, uint8 msgtype);
static void pciedev_enable_powerstate_interrupts(struct dngl_bus *pciedev);
static void pciedev_dump_err_cntrs(struct dngl_bus *pciedev);

uint32 starttime, endtime;

extern bcm_rxcplid_list_t *g_rxcplid_list;
static uint32 pciedev_reorder_flush_cnt = 0;

#ifdef PCIE_DEBUG_DUMP_MSGBUF_RINGINFO
static void
BCMATTACHFN(pciedev_dump_common_msgbuf_ring_info)(msgbuf_ring_t *msgbuf)
{
	lcl_buf_pool_t *pool = msgbuf->buf_pool;
	uint8 i;

	PCI_TRACE(("ring %c%c%c%c: phase %d, r_ptr 0x%04x, w_ptr 0x%04x\n",
		msgbuf->name[0], msgbuf->name[1], msgbuf->name[2],
		msgbuf->name[3], msgbuf->current_phase, (msgbuf->tcm_rs_r_ptr),
		(uint32)(msgbuf->tcm_rs_w_ptr)));
	PCI_TRACE(("ring_mem 0x%04x, base_addr low 0x%04x, base_addr high 0x%04x\n",
		(uint32)(msgbuf->ringmem),
		(uint32)LOW_ADDR_32(msgbuf->ringmem->base_addr),
		(uint32)HIGH_ADDR_32(msgbuf->ringmem->base_addr)));

	PCI_TRACE(("CHECKING\n"));
	PCI_TRACE(("Ring %c%c%c%c max %d, avaialble %d\n",
		msgbuf->name[0], msgbuf->name[1], msgbuf->name[2],
		msgbuf->name[3], pool->buf_cnt, pool->availcnt));
	for (i = 0; i < pool->buf_cnt; i++) {
		PCI_TRACE(("Ring %c%c%c%c %d:  %x\n", msgbuf->name[0],
			msgbuf->name[1], msgbuf->name[2], msgbuf->name[3],
			i, pool->buf[i].p));
	}
}
#endif /* PCIE_DEBUG_DUMP_MSGBUF_RINGINFO */

static void
BCMATTACHFN(pciedev_dump_hostbuf_pool)(const char *name, host_dma_buf_pool_t *pool)
{
	uint32 i = 0;
	host_dma_buf_t *free;

	PCI_TRACE(("Pool name %c%c%c%c pool max: %d,  free 0x%x, ready %x\n",
		name[0], name[1], name[2], name[3], pool->max,
		(uint32)pool->free, (uint32)pool->ready));
	free = pool->free;
	while (free) {
		i++;
		PCI_TRACE(("free[%d] cur:%x\n", i, (uint32) free));
		free = free->next;
	}
	free = pool->ready;
	i = 0;
	while (free) {
		i++;
		PCI_TRACE(("ready[%d] cur: %x\n", i, (uint32) free));
		free = free->next;
	}
	return;

}

/* initialize the local circular buffer on the dongle side */
#ifndef PCIE_PHANTOM_DEV
static void
pciedev_set_LTRvals(struct dngl_bus *pciedev)
{
	/* make sure the LTR values good */
	/* LTR0 */
	W_REG(pciedev->osh, &pciedev->regs->configaddr, 0x844);
	W_REG(pciedev->osh, &pciedev->regs->configdata, 0x883c883c);
	/* LTR1 */
	W_REG(pciedev->osh, &pciedev->regs->configaddr, 0x848);
	W_REG(pciedev->osh, &pciedev->regs->configdata, 0x88648864);
	/* LTR2 */
	W_REG(pciedev->osh, &pciedev->regs->configaddr, 0x84C);
	W_REG(pciedev->osh, &pciedev->regs->configdata, 0x90039003);
}


static void
pciedev_hw_LTR_war(struct dngl_bus *pciedev)
{
	uint32 devstsctr2;

	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCIEGEN2_CAP_DEVSTSCTRL2_OFFSET);
	devstsctr2 = R_REG(pciedev->osh, &pciedev->regs->configdata);
	if (devstsctr2 & PCIEGEN2_CAP_DEVSTSCTRL2_LTRENAB) {
		PCI_TRACE(("add the work around for loading the LTR values, link state 0x%04x\n",
			R_REG(pciedev->osh, &pciedev->regs->iocstatus)));

		/* force the right LTR values ..same as JIRA 859 */
		pciedev_set_LTRvals(pciedev);

		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCIEGEN2_CAP_DEVSTSCTRL2_OFFSET);
		W_REG(pciedev->osh, &pciedev->regs->configdata, devstsctr2);

		/* set the LTR state to be active */
		pciedev_send_ltr(pciedev, LTR_ACTIVE);
		OSL_DELAY(1000);

		/* set the LTR state to be sleep */
		pciedev_send_ltr(pciedev, LTR_SLEEP);
		OSL_DELAY(1000);
	}
	else {
		PCI_TRACE(("no work around for loading the LTR values\n"));
	}
}
#endif /* PCIE_PHANTOM_DEV */

static uint
BCMATTACHFN(pciedev_api_shmem_init_rev3)(struct dngl_bus *pciedev, pciedev_shared_t *shmem)
{
	ring_info_t *ring_i;
	ring_mem_t *ring_m;
	uchar *ptr;
	uint32 malloc_size;
	uint32 i = 0;
	uint32 local_d2h_ringupd_ptr;

	/* need to use some configurable param */
	uint32 n_cmnrings = BCMPCIE_COMMON_MSGRINGS;
	uint32 n_txrings;

#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	/* Tx push ring */
	n_txrings = 1;
#else
	n_txrings = BCMPCIE_MAX_TX_FLOWS;
#endif

	/* ring_info, followed by ring mem followed by ring state, followed by mailboxes */
	malloc_size = sizeof(ring_info_t);

	malloc_size += (sizeof(ring_mem_t) *  (n_cmnrings + n_txrings));
	/* D2H common rings status */
	/* w_block */
	malloc_size += (sizeof(uint32) * BCMPCIE_D2H_COMMON_MSGRINGS);
	/* r_block */
	malloc_size += (sizeof(uint32) * BCMPCIE_D2H_COMMON_MSGRINGS);

	/* H2D common rings status */
	/* w_block */
	malloc_size += (sizeof(uint32)  * (BCMPCIE_H2D_COMMON_MSGRINGS  + n_txrings));
	/* r_block */
	malloc_size += (sizeof(uint32)  * (BCMPCIE_H2D_COMMON_MSGRINGS  + n_txrings));

	/* mail boxes D2H + H2D */
	malloc_size += (sizeof(uint32) * 2);

	/* alloc the blob now */
	ptr = MALLOC(pciedev->osh, malloc_size);
	if (ptr == NULL) {
		PCI_TRACE(("pciedev_api_shmem_init_rev3: failed to"
			"alloc memory for %d bytes\n", malloc_size));
		return 0;
	}
	bzero(ptr, malloc_size);

	/* set up the different pointers to this blob */
	shmem->rings_info_ptr = (uint32)ptr;
	ring_i = (ring_info_t *)shmem->rings_info_ptr;

	/* ring info init */

	ring_i->max_sub_queues = BCMPCIE_H2D_COMMON_MSGRINGS + n_txrings;

	ptr += sizeof(ring_info_t);
	ring_i->ringmem_ptr = (uint32)ptr;

	/* ring mem init */
	ring_m = (ring_mem_t *)ring_i->ringmem_ptr;
	ptr += (sizeof(ring_mem_t) * (n_cmnrings + n_txrings));

	ring_i->h2d_w_idx_ptr = (uint32)ptr;
	ptr += (sizeof(uint32) * (BCMPCIE_H2D_COMMON_MSGRINGS + n_txrings));

	/* Please do not separate these variables, host expects them in the same order */
	ring_i->h2d_r_idx_ptr = ((uint32)ptr);
	ptr += (sizeof(uint32) * (BCMPCIE_H2D_COMMON_MSGRINGS + n_txrings));

	ring_i->d2h_w_idx_ptr = (uint32)ptr;
	ptr += (BCMPCIE_D2H_COMMON_MSGRINGS * sizeof(uint32));

	local_d2h_ringupd_ptr =  ring_i->h2d_r_idx_ptr;
	PHYSADDR64HISET(pciedev->local_d2h_ringupd_dmabuf, (uint32)0);
	PHYSADDR64LOSET(pciedev->local_d2h_ringupd_dmabuf, local_d2h_ringupd_ptr);
	pciedev->local_d2h_ringupd_size =  (uint32)ptr - local_d2h_ringupd_ptr;

	ring_i->d2h_r_idx_ptr = (uint32)ptr;
	ptr += (BCMPCIE_D2H_COMMON_MSGRINGS * sizeof(uint32));

	shmem->h2d_mb_data_ptr = (uint32)ptr;
	shmem->d2h_mb_data_ptr = (uint32)(ptr + 4);

	PCI_TRACE(("h2d_r 0x%04x, h2d_w 0x%04x, d2h_r 0x%04x, d2h_w 0x%04x\n",
		ring_i->h2d_r_idx_ptr, ring_i->h2d_w_idx_ptr,
		ring_i->d2h_r_idx_ptr, ring_i->d2h_w_idx_ptr));

	for (i = 0; i < BCMPCIE_COMMON_MSGRINGS; i++) {
		pciedev->cmn_msgbuf_ring[i].ringmem = ring_m;

		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"DONGLE : idx %d ring mem %x \n", i, (uint32)ring_m);
		ring_m++;
	}
	pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT].tcm_rs_w_ptr =
		(uint32 *)(ring_i->h2d_w_idx_ptr +
		(sizeof(uint32) * BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT_IDX));
	pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT].tcm_rs_r_ptr =
		(uint32 *)(ring_i->h2d_r_idx_ptr +
		(sizeof(uint32) * BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT_IDX));

	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
		"h2d ctrl w / r %p %p  \n",
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT].tcm_rs_w_ptr,
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT].tcm_rs_r_ptr);

	pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT].tcm_rs_w_ptr =
		(uint32 *)(ring_i->h2d_w_idx_ptr +
		(sizeof(uint32) * BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT_IDX));
	pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT].tcm_rs_r_ptr =
		(uint32 *)(ring_i->h2d_r_idx_ptr +
		(sizeof(uint32) * BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT_IDX));

	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
		"h2d rxpost  w / r %p  %p \n",
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT].tcm_rs_w_ptr,
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT].tcm_rs_r_ptr);

	pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE].tcm_rs_w_ptr =
		(uint32 *)(ring_i->d2h_w_idx_ptr +
		(sizeof(uint32) * BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE_IDX));
	pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE].tcm_rs_r_ptr =
		(uint32 *)(ring_i->d2h_r_idx_ptr +
		(sizeof(uint32) * BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE_IDX));

	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
		"d2h ctrl w / r %p  %p \n",
		(uint32)pciedev->cmn_msgbuf_ring
		[BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE].tcm_rs_w_ptr,
		(uint32)pciedev->cmn_msgbuf_ring
		[BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE].tcm_rs_r_ptr);

	pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_TX_COMPLETE].tcm_rs_w_ptr =
		(uint32 *)(ring_i->d2h_w_idx_ptr +
		(sizeof(uint32) * BCMPCIE_D2H_MSGRING_TX_COMPLETE_IDX));
	pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_TX_COMPLETE].tcm_rs_r_ptr =
		(uint32 *)(ring_i->d2h_r_idx_ptr +
		(sizeof(uint32) * BCMPCIE_D2H_MSGRING_TX_COMPLETE_IDX));

	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
		"d2h txcmplt wrt/ read %p  %p \n",
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_TX_COMPLETE].tcm_rs_w_ptr,
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_TX_COMPLETE].tcm_rs_r_ptr);

	pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_RX_COMPLETE].tcm_rs_w_ptr =
		(uint32 *)(ring_i->d2h_w_idx_ptr +
		(sizeof(uint32) * BCMPCIE_D2H_MSGRING_RX_COMPLETE_IDX));
	pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_RX_COMPLETE].tcm_rs_r_ptr =
		(uint32 *)(ring_i->d2h_r_idx_ptr +
		(sizeof(uint32) * BCMPCIE_D2H_MSGRING_RX_COMPLETE_IDX));

	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
		"d2h rxcmplt wrt  read %p  %p \n",
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_RX_COMPLETE].tcm_rs_w_ptr,
		(uint32)pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_RX_COMPLETE].tcm_rs_r_ptr);

#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	/* support one push txring */
	pciedev->txp_msgbuf.ringmem = ring_m;
	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
		"TXRING memptr %x \n", (uint32)pciedev->txp_msgbuf.ringmem);

	pciedev->txp_msgbuf.tcm_rs_w_ptr =
		(uint32 *)(ring_i->h2d_w_idx_ptr +
		(sizeof(uint32) * (BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)));
	pciedev->txp_msgbuf.tcm_rs_r_ptr =
		(uint32 *)(ring_i->h2d_r_idx_ptr +
		(sizeof(uint32) * (BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)));
	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
		"h2d TXP write read %p   %p  \n",
		(uint32)pciedev->txp_msgbuf.tcm_rs_w_ptr, (uint32)pciedev->txp_msgbuf.tcm_rs_r_ptr);
#endif /* BCMPCIE_SUPPORT_TX_PUSH_RING */

	pciedev->h2d_mb_data_ptr = (uint32 *)shmem->h2d_mb_data_ptr;
	pciedev->d2h_mb_data_ptr = (uint32 *)shmem->d2h_mb_data_ptr;
	return 1;
}

bool
pciedev_init_flowrings(struct dngl_bus *pciedev)
{
	pciedev_shared_t *shmem = pciedev->pcie_sh;
#ifndef BCMPCIE_SUPPORT_TX_PUSH_RING
	ring_info_t *ring_info = (ring_info_t *)shmem->rings_info_ptr;
	ring_mem_t *rmem = (ring_mem_t *)ring_info->ringmem_ptr;
	int i;
#endif

	pciedev->flow_ring_msgbuf = MALLOCZ(pciedev->osh,
		BCMPCIE_MAX_TX_FLOWS * sizeof(msgbuf_ring_t));

	if (pciedev->flow_ring_msgbuf == NULL) {
		PCI_ERROR(("Failed to allocate blob of (%d) msgbuf_ring_t! Expected memory: %d!\n",
		BCMPCIE_MAX_TX_FLOWS, BCMPCIE_MAX_TX_FLOWS * sizeof(msgbuf_ring_t)));
		return FALSE;
	}

#ifndef BCMPCIE_SUPPORT_TX_PUSH_RING
	rmem += BCMPCIE_COMMON_MSGRINGS;

	for (i = 0; i < BCMPCIE_MAX_TX_FLOWS; i++) {
		pciedev->flow_ring_msgbuf[i].ringmem = rmem;
		rmem++;
		pciedev->flow_ring_msgbuf[i].tcm_rs_w_ptr =
			(uint32 *)(ring_info->h2d_w_idx_ptr +
			(sizeof(uint32) * (BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START + i)));
		pciedev->flow_ring_msgbuf[i].tcm_rs_r_ptr =
			(uint32 *)(ring_info->h2d_r_idx_ptr +
			(sizeof(uint32) * (BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START + i)));

		EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
			"h2d FLOW RING %d write read %p   %p  \n",
			i + BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START,
			(uint32)pciedev->flow_ring_msgbuf[i].tcm_rs_w_ptr,
			(uint32)pciedev->flow_ring_msgbuf[i].tcm_rs_r_ptr);

		pciedev->flow_ring_msgbuf[i].inited = FALSE;
		pciedev->flow_ring_msgbuf[i].buf_pool = pciedev->lcl_flowring_buf;
		snprintf((char *)pciedev->flow_ring_msgbuf[i].name,
			MSGBUF_RING_NAME_LEN, "txfl%d", i);
		pciedev->flow_ring_msgbuf[i].pciedev = pciedev;
		pciedev->flow_ring_msgbuf[i].phase_supported = FALSE;
		pciedev->flow_ring_msgbuf[i].dmarx_indices_supported = FALSE;
		pciedev->flow_ring_msgbuf[i].current_phase =  MSGBUF_RING_INIT_PHASE;
		pciedev->flow_ring_msgbuf[i].flow_info.flags = FALSE;
		pciedev->flow_ring_msgbuf[i].flow_info.reqpktcnt = 0;
		pciedev->flow_ring_msgbuf[i].fetch_pending = 0;
	}
#endif /* BCMPCIE_SUPPORT_TX_PUSH_RING */

	PCI_ERROR(("Done initing %d flowrings\n", BCMPCIE_MAX_TX_FLOWS));
	return TRUE;
}

static uint
BCMATTACHFN(pciedev_api_shmem_init)(struct dngl_bus *pciedev)
{
	pciedev_shared_t *shmem;

	/* copy pcie shared struct addr into pciedev->pcie_sh */
	hndrte_update_shared_struct(&pciedev->pcie_sh);
	shmem = pciedev->pcie_sh;

	shmem->dma_rxoffset = 0;

	pciedev->h2d_mb_data_ptr = 0;
	pciedev->d2h_mb_data_ptr = 0;

#ifdef BCMFRAGPOOL
	/* Update the total packet pool count in the shared structure. */
	/* Host will use this for flow-control */
	shmem->total_lfrag_pkt_cnt = SHARED_FRAG_POOL_LEN;
	shmem->max_host_rxbufs = MAX_HOST_RXBUFS-1;
#endif

	BUZZZ_REGISTER(shmem);

	return (pciedev_api_shmem_init_rev3(pciedev, shmem));
}

/* Global function for host notification */
static struct dngl_bus *dongle_bus = NULL;

void
pciedev_fwhalt(void)
{
	if (dongle_bus) {
		/* Assert HOST_WAKE for 20 ms to ensure that host is awake
		** to service the interrupt
		*/
		PCI_TRACE(("Firmware halt indication to host\n"));
		pciedev_host_wake_gpio_enable(dongle_bus, TRUE);
		OSL_DELAY(20000);
		pciedev_d2h_mbdata_send(dongle_bus, D2H_DEV_FWHALT);
		pciedev_host_wake_gpio_enable(dongle_bus, FALSE);
	}
}

static bool
pciedev_host_dma_buf_pool_add(host_dma_buf_pool_t *pool, addr64_t *addr, uint32 pktid, uint16 len)
{
	host_dma_buf_t *free;

	if (pool->free == NULL)
		return FALSE;

	free = pool->free;
	pool->free = free->next;

	free->buf_addr.low_addr = addr->low_addr;
	free->buf_addr.high_addr = addr->high_addr;
	free->pktid = pktid;
	free->len = len;
	free->next = pool->ready;

	pool->ready = free;
	pool->ready_cnt++;
	return TRUE;
}

static bool
pciedev_host_dma_buf_pool_get(host_dma_buf_pool_t *pool, host_dma_buf_t *ready)
{
	host_dma_buf_t *local;

	PCI_TRACE(("pciedev_host_dma_buf_pool_get:"));
	PCI_TRACE(("pool %x, ready is %x\n", (uint32)pool, (uint32)pool->ready));
	if (pool->ready == NULL)
		return FALSE;
	local = pool->ready;
	pool->ready = local->next;

	bcopy(local, ready, sizeof(host_dma_buf_t));

	local->next = pool->free;
	pool->free = local;
	pool->ready_cnt--;
	return TRUE;
}

static host_dma_buf_pool_t *
BCMATTACHFN(pciedev_init_host_dma_buf_pool_init)(struct dngl_bus *pciedev, uint16 max)
{
	host_dma_buf_pool_t *pool;
	host_dma_buf_t *free;
	uint32 alloc_size = 0;
	uint i;

	if (max == 0)
		return NULL;

	alloc_size = sizeof(host_dma_buf_pool_t) + (max * sizeof(host_dma_buf_t));

	pool = (host_dma_buf_pool_t *)MALLOC(pciedev->osh, alloc_size);
	if (pool == NULL) {
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return NULL;
	}

	bzero(pool, alloc_size);

	pool->max = max;
	/* set up descs */
	free = (host_dma_buf_t *)&pool->pool_array[0];

	pool->ready = NULL;
	pool->free = free;

	for (i = 0; i < pool->max; i++) {
		free->next = (free + 1);
		free = free+1;
	}
	free->next = NULL;
	return pool;
}

static bool
pciedev_rxbufpost_msg(struct dngl_bus *pciedev, void *p, host_dma_buf_pool_t *pool)
{
	ioctl_resp_evt_buf_post_msg_t *post;
	bool ret_val;

	post = (ioctl_resp_evt_buf_post_msg_t *)p;

	ret_val = pciedev_host_dma_buf_pool_add(pool, &post->host_buf_addr,
		post->cmn_hdr.request_id, post->host_buf_len);


	if (ret_val == FALSE) {
		PCI_ERROR(("Error adding a host buffer to pool\n"));
	}
	return ret_val;
}
static  void
pciedev_rxreorder_queue_flush_cb(void *ctx, uint16 list_start_id, uint32 count)
{
	rxcpl_info_t *p_rxcpl_info;
	struct dngl_bus *pciedev = (struct dngl_bus *)ctx;

	if ((list_start_id == 0) || (count == 0))
		return;

	p_rxcpl_info = bcm_id2rxcplinfo(list_start_id);
	PCI_TRACE(("flush called for id %d, count %d, items in rxcpl list %d\n",
		list_start_id, count, g_rxcplid_list->avail));
	pciedev_reorder_flush_cnt++;
	pciedev_queue_rxcomplete_local(pciedev, p_rxcpl_info, pciedev->dtoh_rxcpl, TRUE);
}

/* Core pciedev attach
 * 1. Intialize regs
 * 2. Initialize local circular buffers[d2h & h2d]
 * 3. Initialize shared area
 * 4. Silicon attach
 * 5. dma attach
 * 6. dongle attach
 * 7. timer attach
*/
static const char BCMATTACHDATA(rstr_PKTRXFRAGSZ_D)[] = "PKTRXFRAGSZ = %d\n";
static const char BCMATTACHDATA(rstr_eventpool)[]     = "eventpool";
static const char BCMATTACHDATA(rstr_ioctlresp)[]     = "ioctlresp";
struct dngl_bus *
BCMATTACHFN(pciedev_attach)(void *drv, uint vendor, uint device, osl_t *osh,
void *regs, uint bustype)
{

	struct dngl_bus *pciedev;
#ifndef PCIE_PHANTOM_DEV
	int bcmerror = 0;
#endif
	char *vars;
	uint vars_len;
	bool evnt_ctrl = 0;
	int i, n = 0;
	int pktq_depth = 0;

	PCI_TRACE(("pciedev_attach\n"));
	PCI_TRACE(("PKTRXFRAGSZ = %d\n", MAXPKTRXFRAGSZ));
	/* Vendor match */
	if (!pciedev_match(vendor, device)) {
		PCI_TRACE(("pciedev_attach: pciedev_match failed %d\n", MALLOCED(osh)));
		return NULL;
	}

	/* Allocate bus device state info */
	if (!(pciedev = MALLOCZ(osh, sizeof(struct dngl_bus)))) {
		PCI_ERROR(("pciedev_attach: out of memory\n"));
		return NULL;
	}


	pciedev->osh = osh;
	pciedev->regs = regs;

	dongle_bus = pciedev;

	/* Allocate error_cntrs structure */
	if (!(pciedev->err_cntr = MALLOCZ(osh, sizeof(pciedev_err_cntrs_t)))) {
		PCI_ERROR(("pciedev_attach: out of memory\n"));
		return NULL;
	}

	/* Tunables init */
	pciedev_tunables_init(pciedev);
#ifdef BCMFRAGPOOL
	pciedev->pktpool_lfrag = SHARED_FRAG_POOL;	/* shared tx frag pool */
#endif

	pciedev->pktpool_rxlfrag = SHARED_RXFRAG_POOL;	/* shared rx frag pool */
	pciedev->pktpool = SHARED_POOL;	/* shared lbuf pool */

	PCI_TRACE(("Ioctl Lock initialized...\n"));
	pciedev->ioctl_lock = FALSE;

	pciedev->d2h_dma_rxoffset = D2H_PD_RX_OFFSET;
	pciedev_api_shmem_init(pciedev);

	if (!pciedev_setup_common_msgbuf_rings(pciedev)) {
		evnt_ctrl = PCIE_ERROR1;
		goto fail2;
	}
#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	if (!pciedev_setup_txflow_msgbuf_ring(pciedev)) {
		evnt_ctrl = PCIE_ERROR2;
		goto fail2;
	}
#endif
	/* setup event and IOCTL resp buffer pools */
	pciedev->event_pool = pciedev_init_host_dma_buf_pool_init(pciedev,
		PCIEDEV_EVENT_BUFPOOL_MAX);
	if (pciedev->event_pool == NULL) {
		evnt_ctrl = PCIE_ERROR3;
		goto fail2;
	}
	pciedev->ioctl_resp_pool = pciedev_init_host_dma_buf_pool_init(pciedev,
		PCIEDEV_IOCTRESP_BUFPOOL_MAX);
	if (pciedev->ioctl_resp_pool == NULL) {
		evnt_ctrl = PCIE_ERROR4;
		goto fail2;
	}
	pciedev_dump_hostbuf_pool(rstr_eventpool, pciedev->event_pool);
	pciedev_dump_hostbuf_pool(rstr_ioctlresp, pciedev->ioctl_resp_pool);
#ifdef PCIE_DEBUG_DUMP_MSGBUF_RINGINFO
	pciedev_dump_common_msgbuf_ring_info(pciedev->htod_rx);
	pciedev_dump_common_msgbuf_ring_info(pciedev->htod_ctrl);
	pciedev_dump_common_msgbuf_ring_info(pciedev->dtoh_txcpl);
	pciedev_dump_common_msgbuf_ring_info(pciedev->dtoh_rxcpl);
	pciedev_dump_common_msgbuf_ring_info(pciedev->dtoh_ctrlcpl);

#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	pciedev_dump_common_msgbuf_ring_info(pciedev->htod_tx);
#endif
#endif /* If 0 */

	/* htod dma queue init */
	pciedev->htod_dma_rd_idx = 0;
	pciedev->htod_dma_wr_idx = 0;

	pciedev->dtoh_dma_wr_idx = pciedev->dtoh_dma_rd_idx = 0;

#ifdef PKTC_TX_DONGLE
	pciedev->pkthead = NULL;
	pciedev->pkttail = NULL;
	pciedev->pkt_chain_cnt = 0;
#endif /* PKTC_TX_DONGLE */

	/* Attach to backplane */
	if (!(pciedev->sih = si_attach(device, osh, regs, bustype, NULL, &vars, &vars_len))) {
		PCI_TRACE(("pciedev_attach: si_attach failed\n"));
		goto fail;
	}

#ifdef PCIE_PHANTOM_DEV
	pciedev->phtm = pcie_phtm_attach(pciedev, osh, pciedev->sih,
		regs, pciedev->pcie_sh, pciedev->d2h_dma_rxoffset);
	if (pciedev->phtm == NULL)
		goto fail;
#endif /* PCIE_PHANTOM_DEV */

	/* Bring back to Pcie core */
	si_setcore(pciedev->sih, PCIE2_CORE_ID, 0);

	pciedev->coreid = si_coreid(pciedev->sih);
	pciedev->corerev = si_corerev(pciedev->sih);

	/* Newr gen chip has pcie core DMA */
	/* Initialize pcie dma */

#ifndef PCIE_PHANTOM_DEV
	{
		/* for now use the PD_RX_OFFSET as the dma rxoffset */
		pciedev->h2d_dma_rxoffset = H2D_PD_RX_OFFSET;

		if (pciedev->h2d_dma_rxoffset) {
			pciedev->dummy_rxoff = MALLOC(osh, pciedev->h2d_dma_rxoffset);
			ASSERT(pciedev->dummy_rxoff);
		}

#ifdef PCIE_DMAXFER_LOOPBACK
		pktq_init(&pciedev->lpbk_dma_txq, PCIE_LPBK_SUPPORT_TXQ_NPREC,
			PCIE_LPBK_SUPPORT_TXQ_SIZE);
		pktq_init(&pciedev->lpbk_dma_txq_pend, PCIE_LPBK_SUPPORT_TXQ_NPREC,
			PCIE_LPBK_SUPPORT_TXQ_SIZE);
		pciedev->lpbk_src_dmaxfer_timer = dngl_init_timer(pciedev, NULL,
			pciedev_lpbk_src_dmaxfer_timerfn);
		pciedev->lpbk_dest_dmaxfer_timer = dngl_init_timer(pciedev, NULL,
			pciedev_lpbk_dest_dmaxfer_timerfn);
#endif /* PCIE_DMAXFER_LOOPBACK */

		pciedev->in_d3_suspend = FALSE;
		pciedev->in_d3_pktcount = 0;
		pciedev->host_wake_gpio = pciedev_host_wake_gpio_init(pciedev);

#if defined(BCMFRAGPOOL) && !defined(BCMFRAGPOOL_DISABLED)
		pktq_depth = SHARED_FRAG_POOL_LEN;
#endif
#if defined(BCMRXFRAGPOOL) && !defined(BCMRXFRAGPOOL_DISABLED)
		pktq_depth += SHARED_RXFRAG_POOL_LEN;
#endif

		/* this should be the max rxlfrags and txlfrags we could support */
		pktq_init(&pciedev->d2h_req_q, 1, MAX(PCIEDEV_MIN_PKTQ_DEPTH, pktq_depth));

#ifdef PCIE_DELAYED_HOSTWAKE
		/* Initialize the timer that is used to do deferred host_wake */
		pciedev->delayed_hostwake_timer = dngl_init_timer(pciedev, NULL,
			pciedev_delayed_hostwake_timerfn);
		pciedev->delayed_hostwake_timer_active = FALSE;
#endif /* PCIE_DELAYED_HOSTWAKE */

		/* Initialize the timer that is used to do deferred host_wake */
		pciedev->delayed_ioctlunlock_timer = dngl_init_timer(pciedev, NULL,
			pciedev_sched_msgbuf_intr_process);

		pciedev->health_check_timer = dngl_init_timer(pciedev, NULL,
			pciedev_health_check_timerfn);
		pciedev->health_check_timer_on = FALSE;

		pciedev->bm_not_enabled = FALSE;
		pciedev->bm_enab_timer = dngl_init_timer(pciedev, NULL,
		                                       pciedev_bm_enab_timerfn);
		pciedev->bm_enab_timer_on = FALSE;
		pciedev->bm_enab_check_interval = PCIE_BM_CHECK_INTERVAL;

		/* reset pcie core interrupts */
		pciedev_reset_pcie_interrupt(pciedev);

		/* clear the intstatus bit going from chipc to PCIE */
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_INT_MASK);
		W_REG(pciedev->osh, &pciedev->regs->configdata, 0);

		/* Initialize the dma */
		bcmerror = pciedev_pciedma_init(pciedev);
		 if (bcmerror) {
			PCI_TRACE(("pciedev_attach: failed initing the PCIE DMA engines \n"));
			goto fail;
		 }
	}

	pciedev_set_LTRvals(pciedev);

	pciedev_hw_LTR_war(pciedev);
#endif /* PCIE_PHANTOM_DEV */

	if (PCIECOREREV(pciedev->corerev) == 7 || PCIECOREREV(pciedev->corerev) == 11) {
		uint32 value;

		value = PMU_CC6_ENABLE_CLKREQ_WAKEUP | PMU_CC6_ENABLE_PMU_WAKEUP_ALP |
			CC6_4350_PMU_EN_EXT_PERST_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);

		if (PCIECOREREV(pciedev->corerev) == 11) {
			value = PMU_CC7_ENABLE_L2REFCLKPAD_PWRDWN | PMU_CC7_ENABLE_MDIO_RESET_WAR;
			si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL7, value, value);
		}
	}
	if ((CHIPID(pciedev->sih->chip) == BCM4345_CHIP_ID) &&
		(CHIPREV(pciedev->sih->chiprev) >= 4)) {
		uint32 value;

		value = CC6_4345_PMU_EN_ASSERT_L2_MASK | CC6_4345_PMU_EN_MDIO_MASK |
			CC6_4345_PMU_EN_PERST_DEASSERT_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);
	}

	/* Register a Callback Function at pktfree */
	lbuf_free_register(pciedev_lbuf_callback, pciedev);
#ifdef BCMPCIEDEV_ENABLED
	hndrte_fetch_bus_dispatch_cb_register(pciedev_dispatch_fetch_rqst, pciedev);
	pciedev->fcq = MALLOC(pciedev->osh, sizeof(pciedev_fetch_cmplt_q_t));
	if (pciedev->fcq == NULL) {
		evnt_ctrl = PCIE_ERROR4;
		goto fail;
	}
	bzero(pciedev->fcq, sizeof(pciedev_fetch_cmplt_q_t));
#endif

	/* For ioctl_ptr case, we need only one fetch_rqst prealloced. Do not use pool for this */
	pciedev->ioctptr_fetch_rqst = MALLOC(pciedev->osh, sizeof(struct fetch_rqst));
	if (pciedev->ioctptr_fetch_rqst == NULL) {
			evnt_ctrl = PCIE_ERROR5;
			goto fail;
	}
	bzero(pciedev->ioctptr_fetch_rqst, sizeof(struct fetch_rqst));

	/* Register a call back function to be called for every pktpool_get */
	/* After dequeuing a pkt, it should be passed to pciedev_fillup_haddr */
	/* And here, host address, bufid and length info are populated */
	if (BCMSPLITRX_ENAB()) {
		pktpool_hostaddr_fill_register(pciedev->pktpool_rxlfrag,
			pciedev_fillup_haddr, pciedev);

		pktpool_rxcplid_fill_register(pciedev->pktpool_rxlfrag,
			pciedev_fillup_rxcplid, pciedev);
	}
	pktpool_rxcplid_fill_register(pciedev->pktpool, pciedev_fillup_rxcplid, pciedev);
	/* Initialize dongle device */
	if (!(pciedev->dngl = dngl_attach(pciedev, NULL, pciedev->sih, osh))) {
		evnt_ctrl = PCIE_ERROR6;
		goto fail;
	}


#ifdef WAR1_HWJIRA_CRWLPCIEGEN2_162
	pciedev->WAR_timeout = 1; /* ms */
	pciedev->WAR_timer_on = FALSE;
	pciedev->WAR_timer_resched = FALSE;
	pciedev->WAR_timer = dngl_init_timer(pciedev, NULL, pciedev_WAR_timerfn);
#endif /* WAR1_HWJIRA_CRWLPCIEGEN2_162 */
	pciedev->mailboxintmask = 0;
	pciedev->pciecoreset = FALSE;


#ifdef HNDRTE_CONSOLE
#ifdef BCMDBG_SD_LATENCY
	hndrte_cons_addcmd("sdlat", (cons_fun_t)pciedev_print_latency, (uint32)pciedev);
#endif
#endif /* HNDRTE_CONSOLE */

	/* Set halt handler */
	hndrte_set_fwhalt(pciedev_fwhalt);

#ifdef DEBUG_PARAMS_ENABLED
/*	debug_params.txavail = pciedev->txavail; */
#endif /* DEBUG_PARAMS_ENABLED */


#ifndef PCIE_PHANTOM_DEV
	/* Initialize the timer that is used to test user LTR state requests */
	pciedev->ltr_test_timer = dngl_init_timer(pciedev, NULL, pciedev_usrltrtest_timerfn);

	/* register a call back to send ltr messages */
	hndrte_register_ltrsend_callback(pciedev_send_ltr, pciedev);

	hndrte_set_ltrstate(si_coreidx(pciedev->sih), LTR_SLEEP);

	pciedev_crwlpciegen2(pciedev);
	pciedev_reg_pm_clk_period(pciedev);
	pciedev_crwlpciegen2_61(pciedev);
	pciedev_crwlpciegen2_180(pciedev);

	/* support for device wake */
	pciedev->ds_state = NO_DS_STATE;
	pciedev->ds_check_timer = dngl_init_timer(pciedev, NULL,
		pciedev_ds_check_timerfn);
	pciedev->ds_check_timer_on = FALSE;
	pciedev->ds_check_timer_max = 0;
	pciedev->ds_check_interval = PCIE_DS_CHECK_INTERVAL;
	pciedev_enable_device_wake(pciedev);
#endif /* PCIE_PHANTOM_DEV */

	if ((pciedev->ioctl_req_pool = MALLOCZ(pciedev->osh, sizeof(pktpool_t))) == NULL) {
		PCI_ERROR(("IOCTL req pool alloc failed \n"));
		goto fail;
	}
	n = IOCTL_PKTPOOL_SIZE;
	if (pktpool_init(pciedev->osh, pciedev->ioctl_req_pool, &n,
		IOCTL_PKTBUFSZ, FALSE, lbuf_basic) != BCME_OK)
	{
		PCI_ERROR(("couldn't init pktpool for IOCTL requests\n"));
		goto fail;
	}
	pktpool_setmaxlen(pciedev->ioctl_req_pool, IOCTL_PKTPOOL_SIZE);

	/* allocate rx completion blobs */
	if (!bcm_alloc_rxcplid_list(pciedev->osh, pciedev->pcie_sh->max_host_rxbufs)) {
		PCI_ERROR(("error enabling the rx completion message IDs\n"));
		goto fail;
	}
	/* update the shared structure with the rxoffset to use */
	pciedev->prev_ifidx = 0;
	pciedev_setup_flow_rings(pciedev);
	pciedev->flow_schedule_timer =
		dngl_init_timer(pciedev, NULL, pciedev_flow_schedule_timerfn);
	pciedev->flow_sch_timer_on = FALSE;

	/* regster a callback for the reorder queue rxcomplete flush */
	hndrte_register_rxreorder_queue_flush_cb(pciedev_rxreorder_queue_flush_cb,
		(void *)pciedev);

	pciedev->metric_ref.active = hndrte_time();


	/* Register the callback for freeing glommed txstatus and rxcomplete from wl_dpc */
	hndrte_register_cb_flush_chainedpkts(pciedev_flush_chained_pkts, (void *)pciedev);

	STATIC_ASSERT(H2DRING_CTRL_SUB_ITEMSIZE >=  D2HRING_CTRL_CMPLT_ITEMSIZE);

	/* priority flow ring queue attach */
	if ((pciedev->prioring = (prioring_pool_t *)MALLOC(pciedev->osh,
		sizeof(prioring_pool_t) * PCIE_MAX_TID_COUNT)) == NULL) {
		PCI_ERROR(("priority ring pool alloc failed\n"));
		goto fail;
	}
	dll_init(&pciedev->active_prioring_list);
	for (i = 0; i < PCIE_MAX_TID_COUNT; i++) {
		dll_init(&pciedev->prioring[i].active_flowring_list);
		pciedev->prioring[i].last_fetch_node = &pciedev->prioring[i].active_flowring_list;
		pciedev->prioring[i].tid = (uint8)i;
		pciedev->prioring[i].inited = FALSE;
	}

	/* Initialize the pool  for  BCMPCIE_MAX_TX_FLOWS flows */
	pciedev->flowring_pool = dll_pool_init(pciedev->osh, BCMPCIE_MAX_TX_FLOWS,
		sizeof(flowring_pool_t));

	if (pciedev->flowring_pool == NULL)
		goto fail;

#ifdef PCIEDEV_USE_EXT_BUF_FOR_IOCTL
	pciedev->ioct_resp_buf_len = PCIEDEV_MAX_IOCTLRSP_BUF_SIZE;
	pciedev->ioct_resp_buf = MALLOCZ(pciedev->osh, pciedev->ioct_resp_buf_len);
	if (pciedev->ioct_resp_buf == NULL) {
		PCI_ERROR(("Ioctl buffer allocation failed. Only 2K ioctls will be supported.\n"));
	}
#else
	pciedev->ioct_resp_buf_len = 0;
	pciedev->ioct_resp_buf = NULL;
#endif

#ifdef PCIEDEV_HOST_PKTID_AUDIT_ENABLED
	pciedev->host_pktid_audit =
	    bcm_mwbmap_init(pciedev->osh, PCIEDEV_HOST_PKTID_AUDIT_MAX + 1);
	if (pciedev->host_pktid_audit == NULL) {
		PCI_ERROR(("host_pktid_audit allocation failed\n"));
		goto fail;
	}
	bcm_mwbmap_force(pciedev->host_pktid_audit, 0); /* pktid=0 is invalid */
#endif /* PCIEDEV_HOST_PKTID_AUDIT_ENABLED */

#if defined(PCIE_M2M_D2H_SYNC)
	/* starting module-253 seqnum values */
	pciedev->rxbuf_cmpl_epoch = D2H_EPOCH_INIT_VAL;
	pciedev->txbuf_cmpl_epoch = D2H_EPOCH_INIT_VAL;
	pciedev->ctrl_compl_epoch = D2H_EPOCH_INIT_VAL;
#endif /* PCIE_M2M_D2H_SYNC */

	return pciedev;
fail:
	if (pciedev)
		MFREE(pciedev->osh, pciedev, sizeof(struct dngl_bus));
fail2:
	if (evnt_ctrl)
		PCI_ERROR(("error setting up the host buf Error no %d evnt_ctrl\n", evnt_ctrl));

	return NULL;
}

/* Check for vendor id */
static bool
BCMATTACHFN(pciedev_match)(uint vendor, uint device)
{
	PCI_TRACE(("pciedev_match\n"));
	if (vendor != VENDOR_BROADCOM)
		return FALSE;

	return TRUE;
}

/* Initialize tunables */
static void
BCMATTACHFN(pciedev_tunables_init)(struct dngl_bus *pciedev)
{
	/* set tunables defaults (RXACK and TXDROP are stats) */
	pciedev->tunables[NTXD] = PD_NTXD;
	pciedev->tunables[NRXD] = PD_NRXD;
	pciedev->tunables[RXBUFS] = 32;
	pciedev->tunables[RXBUFSZ] = PD_RXBUF_SIZE;
	pciedev->tunables[MAXHOSTRXBUFS] = MAX_HOST_RXBUFS;
	pciedev->tunables[MAXTXSTATUS] = MAX_TX_STATUS_COMBINED;
	pciedev->tunables[MAXRXCMPLT] = MAX_RX_CMPLT_COMBINED;
	pciedev->tunables[D2HTXCPL] = PD_NBUF_D2H_TXCPL;
	pciedev->tunables[D2HRXCPL] = PD_NBUF_D2H_RXCPL;
	pciedev->tunables[H2DRXPOST] = PD_NBUF_H2D_RXPOST;

	/* For smaller DMA rings, use larger SW queue */
}

void *
pciedev_dngl(struct dngl_bus *pciedev)
{
	return pciedev->dngl;
}

#ifdef PCIE_PWRMGMT_CHECK
static void
pciedev_handle_d0_enter(struct dngl_bus *pciedev)
{
	uint32 ioc_status = R_REG(pciedev->osh, &pciedev->regs->iocstatus);

	if (ioc_status & PCIEGEN2_IOC_D0_STATE_MASK) {
		PCI_TRACE(("Enter D0:0x%x, 0x%04x\n",
			R_REG(pciedev->osh, &pciedev->regs->iocstatus),
			(uint32)__builtin_return_address(0)));

	/* Enable stats collection */
	if (PCIECOREREV(pciedev->corerev) >= 7) {
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_STAT_CTRL);
		W_REG(pciedev->osh, &pciedev->regs->configdata,
			PCIE_STAT_CTRL_RESET | PCIE_STAT_CTRL_ENABLE);
	}

		pciedev->real_d3 = FALSE;

		/* Re-program the enumeration space registers and COE private registers */
		/* only if a core reset has happenned. */
		if (pciedev->pciecoreset) {
			pciedev_crwlpciegen2_180(pciedev);

			pciedev_set_LTRvals(pciedev);
			pciedev_hw_LTR_war(pciedev);
			pciedev_crwlpciegen2(pciedev);
			pciedev_reg_pm_clk_period(pciedev);

			OSL_DELAY(1000);
		}
		else {
			if ((CHIPID(pciedev->sih->chip) == BCM4345_CHIP_ID) &&
				(CHIPREV(pciedev->sih->chiprev) >= 4)) {
				si_pmu_chipcontrol(pciedev->sih,
					PMU_CHIPCTL6,
					CC6_4345_PMU_EN_L2_DEASSERT_MASK, 0);
			}
			if (PCIECOREREV(pciedev->corerev) == 11) {
				si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6,
					CC6_4350_PMU_EN_WAKEUP_MASK, 0);
			}
			++pciedev->metrics.d0_resume_ct;
			pciedev->metrics.d3_suspend_dur += hndrte_time() -
				pciedev->metric_ref.d3_suspend;
			pciedev->metric_ref.d3_suspend = 0;
			pciedev->metric_ref.active = hndrte_time();
		}
		if (PCIECOREREV(pciedev->corerev) == 7) {
			uint32 value;
			value = CC7_4350_PMU_EN_ASSERT_L2_MASK;
			si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL7, value, 0);
		}


		pciedev->device_link_state &= ~PCIEGEN2_PWRINT_D0_STATE_MASK;
#ifdef PCIE_API_REV1
		pciedev->device_link_state |= PCIEGEN2_PWRINT_D3_STATE_MASK;
#endif /* PCIE_API_REV1 */
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_CFG_CMD);
		if (pciedev_handle_d0_enter_bm(pciedev) != BCME_OK) {
			/* Start periodic check for Bus Master Enable */
			if (!pciedev->bm_enab_timer_on) {
				dngl_add_timer(pciedev->bm_enab_timer,
					pciedev->bm_enab_check_interval, FALSE);
				pciedev->bm_enab_timer_on = TRUE;
			}
			pciedev->bm_not_enabled = TRUE;
		}
	}
}

/*
* Allow DMA over the PCIE bus only after host enables
* Bus Mastering by setting bit 2 in the PCIE config
* rega 4. Also, certain WARs can be applied at this
* point.
*/

static int
pciedev_handle_d0_enter_bm(struct dngl_bus *pciedev)
{
	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_CFG_CMD);

	if (!(R_REG(pciedev->osh, &pciedev->regs->configdata) & PCI_CMD_MASTER)) {
		PCI_TRACE(("BUS MASTER NOT ENABLED:%p\n", __builtin_return_address(0)));
		return BCME_ERROR;
	}

	PCI_TRACE(("%s\n", __FUNCTION__));

	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_INT_MASK);
	W_REG(pciedev->osh, &pciedev->regs->configdata, 0);

	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCISBMbx);
	W_REG(pciedev->osh, &pciedev->regs->configdata, 1 << 0);

	pciedev_crwlpciegen2(pciedev);
	pciedev->bm_not_enabled = FALSE;

	hndrte_notify_devpwrstchg(TRUE);
	pciedev->in_d3_suspend = FALSE;
	pciedev->in_d3_pktcount = 0;

	/* Reset Host_wake signal */
	pciedev_host_wake_gpio_enable(pciedev, FALSE);
	if (pciedev->pciecoreset) {
		pciedev->pciecoreset = FALSE;
		pciedev_hw_LTR_war(pciedev);
		pciedev_crwlpciegen2_161(pciedev,
			pciedev->cur_ltr_state == LTR_ACTIVE ? TRUE : FALSE);
	}
	/* */
	pciedev_process_tx_payload(pciedev);
	pciedev_queue_d2h_req_send(pciedev);

	pciedev_msgbuf_intr_process(pciedev);

	return BCME_OK;
}

/*
Since pcie core was reset during PERST, make sure to re-enable the
enumeration registers on L0.
*/
static void
pciedev_handle_l0_enter(struct dngl_bus *pciedev)
{
	/* If core reset did not happen, exit */
	if (pciedev->pciecoreset == FALSE)
		return;
	PCI_TRACE(("pciedev_handle_l0_enter: 0x%x\n", pciedev->mailboxintmask));
	if (pciedev->mailboxintmask) {
		W_REG(pciedev->osh, &pciedev->regs->mailboxintmsk,
			pciedev->mailboxintmask);
		pciedev->mailboxintmask = 0;
	}
	if ((CHIPID(pciedev->sih->chip) == BCM4345_CHIP_ID) &&
		(CHIPREV(pciedev->sih->chiprev) >= 4)) {

		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6,
			CC6_4345_PMU_EN_L2_DEASSERT_MASK, 0);
	}
	if (PCIECOREREV(pciedev->corerev) == 11) {
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6,
			CC6_4350_PMU_EN_WAKEUP_MASK, 0);
	}
	pciedev_deepsleep_engine(pciedev, PERST_DEASSRT_EVENT);
	pciedev_handle_d0_enter(pciedev);
	pciedev_enable_powerstate_interrupts(pciedev);

	++pciedev->metrics.perst_deassrt_ct;
	pciedev->metrics.perst_dur += hndrte_time() - pciedev->metric_ref.perst;
	pciedev->metric_ref.perst = 0;
	pciedev->metric_ref.active = hndrte_time();

	++pciedev->metrics.d0_resume_ct;
	pciedev->metrics.d3_suspend_dur += hndrte_time() - pciedev->metric_ref.d3_suspend;
	pciedev->metric_ref.d3_suspend = 0;
	pciedev->metric_ref.active = hndrte_time();
}

static void
pciedev_enable_powerstate_interrupts(struct dngl_bus *pciedev)
{
	if (!(pciedev->defintmask & PD_DEV0_PWRSTATE_INTMASK))
		return;

	/* set the new interrupt request state	 */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_mask, pciedev->device_link_state);
	return;
}

static void
pciedev_handle_d3_enter(struct dngl_bus *pciedev)
{
#ifdef PCIE_API_REV1
	pciedev->in_d3_suspend = TRUE;
#endif /* PCIE_API_REV1 */
	pciedev->mailboxintmask = R_REG(pciedev->osh, &pciedev->regs->mailboxintmsk);

	++pciedev->metrics.d3_suspend_ct;
	pciedev->metric_ref.d3_suspend = hndrte_time();
	pciedev->metrics.active_dur += hndrte_time() - pciedev->metric_ref.active;
	pciedev->metric_ref.active = 0;

	if (PCIECOREREV(pciedev->corerev) == 7) {
		uint32 value;

		value = CC7_4350_PMU_EN_ASSERT_L2_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL7, value, value);
	}
	if ((CHIPID(pciedev->sih->chip) == BCM4345_CHIP_ID) &&
		(CHIPREV(pciedev->sih->chiprev) >= 4)) {

		si_pmu_chipcontrol(pciedev->sih,
			PMU_CHIPCTL6,
			CC6_4345_PMU_EN_L2_DEASSERT_MASK,
			CC6_4345_PMU_EN_L2_DEASSERT_MASK);
	}
	if (PCIECOREREV(pciedev->corerev) == 11) {
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6,
			CC6_4350_PMU_EN_WAKEUP_MASK, CC6_4350_PMU_EN_WAKEUP_MASK);
	}

	pciedev->device_link_state &= ~PCIEGEN2_PWRINT_D3_STATE_MASK;
	pciedev->device_link_state |= PCIEGEN2_PWRINT_D0_STATE_MASK;
	pciedev->real_d3 = TRUE;
}


static void
pciedev_handle_pwrmgmt_intr(struct dngl_bus *pciedev)
{
	uint32 pwr_state;

	pwr_state = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status);
	/* clear the interrupts by writing the same value back  */
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status, pwr_state);
	pwr_state &= pciedev->device_link_state;
	if (pwr_state == 0)
		return;

	if (pwr_state & PCIEGEN2_PWRINT_D3_STATE_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:D3\n"));
		pciedev_handle_d3_enter(pciedev);
	}
	if (pwr_state & PCIEGEN2_PWRINT_D0_STATE_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:D0\n"));

		if (pciedev->pciecoreset == FALSE)
			pciedev_handle_d0_enter(pciedev);

	}

	if (pwr_state & PCIEGEN2_PWRINT_L2_L3_LINK_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:L2_L3\n"));
		pciedev->device_link_state &= ~PCIEGEN2_PWRINT_L2_L3_LINK_MASK;
		pciedev->device_link_state |= PCIEGEN2_PWRINT_L0_LINK_MASK;
	}
	if (pwr_state & PCIEGEN2_PWRINT_L0_LINK_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:L0\n"));
		pciedev->device_link_state |= PCIEGEN2_PWRINT_L2_L3_LINK_MASK;
		pciedev->device_link_state &= ~PCIEGEN2_PWRINT_L0_LINK_MASK;
		pciedev_handle_l0_enter(pciedev);
	}
	pciedev_enable_powerstate_interrupts(pciedev);
}
#endif /* PCIE_PWRMGMT_CHECK */


/* Initialize pcie device */
/* enable interrupts */
void
pciedev_init(struct dngl_bus *pciedev)
{
#ifdef PCIE_PHANTOM_DEV
	if (pciedev->phtm) {
		pcie_phtm_init(pciedev->phtm);
		pciedev->defintmask = DEF_PCIE_INTMASK;
	}
#else
	OR_REG(pciedev->osh, &pciedev->regs->control,
		PCIE_DLYPERST | PCIE_DISSPROMLD);
	AND_REG(pciedev->osh, (&pciedev->regs->control),
		~PCIE_SPERST);
	pciedev->defintmask = PD_DEV0_INTMASK;
	pciedev->intmask = pciedev->defintmask;
	pciedev->device_link_state = PCIEGEN2_PWRINT_L2_L3_LINK_MASK;
#ifdef PCIE_API_REV1
		pciedev->device_link_state |= PCIEGEN2_PWRINT_D3_STATE_MASK;
#endif /* PCIE_API_REV1 */
	pciedev_enable_powerstate_interrupts(pciedev);
	/* pciedev_enable power state interurpts */
#endif /* PCIE_PHANTOM_DEV	*/
	PCI_TRACE(("pciedev is on\n"));
#if EVENT_LOG_COMPILE
	if (pciedev->health_check_timer_on == FALSE) {
		pciedev->health_check_timer_on = TRUE;
		dngl_add_timer(pciedev->health_check_timer, 1000, TRUE);
	}
#endif

	/* tell the host wlevent_req_msg.seqnum is relevant */
#ifdef UART_TRANSPORT
	pciedev_shared.flags |= PCIE_SHARED_EVT_SEQNUM;
#endif

	/* tell the host which form of D2H DMA Complete WAR is to be used */
#if defined(PCIE_M2M_D2H_SYNC)
	pciedev_shared.flags |= PCIE_M2M_D2H_SYNC;
#endif /* PCIE_M2M_D2H_SYNC */

	/* Turn on pcie core interrupt */
	pciedev_intrson(pciedev);
	pciedev->up = TRUE;

	return;
}

/* Update and clear interrupt status bits */
bool
pciedev_dispatch(struct dngl_bus *pciedev)
{
	uint32 intstatus;

	PCI_TRACE(("pciedev_dispatch\n"));

	ASSERT(pciedev->up);

#ifdef PCIE_PHANTOM_DEV
	/* phantom dev */
	intstatus = R_REG(pciedev->osh, &pciedev->regs->intstatus);
	intstatus &= pciedev->defintmask;

	if (!intstatus)
		return FALSE;

	pciedev->intstatus = intstatus;

	/* clear asserted device-side intstatus bits */
	W_REG(pciedev->osh, &pciedev->regs->intstatus, intstatus);
#else
	/* pcie dma devices */
	uint32 h2dintstatus, d2hintstatus;
	intstatus = R_REG(pciedev->osh, &pciedev->regs->intstatus);

	/* Main pcie interrupt */
	intstatus &= pciedev->defintmask;
	pciedev->intstatus = intstatus;
	if (pciedev->intstatus)
		W_REG(pciedev->osh, &pciedev->regs->intstatus, pciedev->intstatus);
	/* DMA interrupt */
	if (intstatus & PD_DEV0_DMA_INTMASK) {
		h2dintstatus = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0);
		d2hintstatus = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0);

		PCI_TRACE(("intstatus is 0x%04x, h2dintstatus 0x%04x, d2hintstatus 0x%04x\n",
			R_REG(pciedev->osh, &pciedev->regs->intstatus),
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0),
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0)));

		pciedev->h2dintstatus = (h2dintstatus & PD_DMA_INT_MASK_H2D);
		pciedev->d2hintstatus = d2hintstatus & PD_DMA_INT_MASK_D2H;

		if (pciedev->h2dintstatus)
			W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0,
				pciedev->h2dintstatus);
		if (pciedev->d2hintstatus)
			W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0,
				pciedev->d2hintstatus);

		pciedev->intstatus &= ~PD_DEV0_DMA_INTMASK;
	}
	PCI_TRACE(("stored intstatus 0x%04x, h2dintstatus 0x%04x, d2hintstatus 0x%04x\n",
		intstatus, pciedev->h2dintstatus, pciedev->d2hintstatus));

	if (pciedev->intstatus | pciedev->h2dintstatus | pciedev->d2hintstatus) {
		/* One of the interrupt set */
	} else {
		return FALSE;
	}
#endif /* PCIE_PHANTOM_DEV */
	return TRUE;
}
#ifndef PCIE_PHANTOM_DEV
static void
pciedev_reset_pcie_interrupt(struct dngl_bus *pciedev)
{
	/* door bell interrupt */
	W_REG(pciedev->osh, &pciedev->regs->intstatus, 0XFFFFFFFF);
	/* h2d dma */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0, 0xFFFFFFFF);
	/* d 2 h dma */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0, 0xFFFFFFFF);
	/* clear PCIE core pwrstate interrupts as well */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status, 0xFFFFFFFF);


}
#endif
/* Switch off interrupt mask */
void
pciedev_intrsoff(struct dngl_bus *pciedev)
{
	/* clear all device-side intstatus bits */
	PCI_TRACE(("pciedev_intrsoff\n"));
#ifdef PCIE_PHANTOM_DEV
	W_REG(pciedev->osh, &pciedev->regs->intmask, 0);
	(void)R_REG(pciedev->osh, &pciedev->regs->intmask); /* sync readback */
#else
	/* disable dongle_2_host dma interrupts */
	AND_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intmask_0, ~PD_DMA_INT_MASK_D2H);

	/* disable host_2_dongle dma interrupts */
	AND_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intmask_0, ~PD_DMA_INT_MASK_H2D);

	/* disable dma complete interrupt in all the dma source for this core */
	/* disable dongle_2_host doorbell interrupts */
	AND_REG(pciedev->osh, &pciedev->regs->intmask, ~pciedev->intmask);
#endif /* PCIE_PHANTOM_DEV */
	pciedev->intmask = 0;
}
/* Switch on pcie interrupts */
void
pciedev_intrson(struct dngl_bus *pciedev)
{
	PCI_TRACE(("pciedev_intrson\n"));
	pciedev->intmask = pciedev->defintmask;

#ifdef PCIE_PHANTOM_DEV
	/* Switch on pcie mail box interrupts */
	W_REG(pciedev->osh, &pciedev->regs->intmask, pciedev->defintmask);
#else
	/* enable host_2_dongle dma interrupts */
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intmask_0, PD_DMA_INT_MASK_H2D);

	/* enable dongle_2_host dma interrupts */
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intmask_0, PD_DMA_INT_MASK_D2H);

	/* enable host_2_dongle doorbell interrupts */
	/* enable dongle_2_host doorbell interrupts */
	OR_REG(pciedev->osh, &pciedev->regs->intmask, pciedev->intmask);
#endif /* PCIE_PHANTOM_DEV */
}

void
pciedev_intrsupd(struct dngl_bus *pciedev)
{
	pciedev_dispatch(pciedev);
}

#ifndef PCIE_PHANTOM_DEV
/* Dongle to host doorbell interrupt */
void
pciedev_generate_host_db_intr(struct dngl_bus *pciedev, uint32 data, uint32 db)
{
	PCI_TRACE(("generate the doorbell interrupt to host\n"));

	/* Update IPC Data */
	pciedev->metrics.num_d2h_doorbell++;

	if (db == PCIE_DB_DEV2HOST_0)
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.dbls[0].dev2host_0, data);
	else if (db == PCIE_DB_DEV2HOST_1) {
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.dbls[0].dev2host_1, data);
	}
	pciedev_deepsleep_engine(pciedev, DB_TOH_EVENT);

}

static void
pciedev_generate_host_mb_intr(struct dngl_bus *pciedev)
{
	W_REG(pciedev->osh, &pciedev->regs->sbtopcimailbox, (0x1 << SBTOPCIE_MB_FUNC0_SHIFT));
}
#endif	/* PCIE_PHANTOM_DEV */

/* DPC for pcie interrupt
 * Handle only mailbox interrupts for phantom device
 * Handle doorbell interrupt and dma completion interrupt for pcie dma device
*/
bool
pciedev_dpc(struct dngl_bus *pciedev)
{
#ifdef PCIE_PHANTOM_DEV
	uint32 intstatus;
#endif
	bool resched = FALSE;
	ASSERT(pciedev);

#ifdef PCIE_PHANTOM_DEV
	intstatus = pciedev->intstatus;
	pciedev->intstatus = 0;
	/* Used for debug */
	if (intstatus & I_F0_B0) {
		PCI_TRACE(("pciedev_dpc: what really needs to be done here\n"));
	}

	/* Fn 0 bit 1 interrupt */
	if (intstatus & I_F0_B1) {
		if ((resched = pciedev_msgbuf_intr_process(pciedev))) {
			PCI_TRACE(("pciedev_msgbuf_intr_process resched \n"));
			pciedev->intstatus |= I_F0_B1;
		}
	}
#else
	/* PCIe dma door bell interrupt */
	if (pciedev->intstatus) {
		/* check the mail box data first, if used */
		if (pciedev->intstatus & PD_FUNC0_MB_INTMASK) {
			pciedev_h2d_mb_data_process(pciedev);
		}
		/*  check for door bell interrupt */
		if (pciedev->intstatus & PD_DEV0_DB_INTMASK) {
			/* Increment IPC Stat */
			pciedev->metrics.num_h2d_doorbell++;
#ifdef PCIE_PWRMGMT_CHECK
			if (pciedev->bm_not_enabled) {
				if (pciedev_handle_d0_enter_bm(pciedev) == BCME_OK) {
					if (pciedev->bm_enab_timer_on) {
						dngl_del_timer(pciedev->bm_enab_timer);
						pciedev->bm_enab_timer_on = FALSE;
					}
				}

				pciedev_enable_powerstate_interrupts(pciedev);
			}
#endif /* PCIE_PWRMGMT_CHECK */
			pciedev_msgbuf_intr_process(pciedev);
			/* clear the local int status bits */
		}
		if (pciedev->intstatus & PD_DEV0_PERST_INTMASK) {
#ifdef PCIE_PWRMGMT_CHECK
			if ((pciedev->real_d3 == FALSE) && (pciedev->in_d3_suspend == TRUE)) {
				PCI_ERROR(("PERST: faking the D3 enter to avoid deadlock\n"));
				pciedev_handle_d3_enter(pciedev);
			}
#endif
			PCI_TRACE(("PERST#\n"));
			pciedev_deepsleep_engine(pciedev, PERST_ASSRT_EVENT);
			pciedev_perst_reset_process(pciedev);
		}
#ifdef PCIE_PWRMGMT_CHECK
		if (pciedev->intstatus & PD_DEV0_PWRSTATE_INTMASK) {
			pciedev_handle_pwrmgmt_intr(pciedev);
		}
#endif /* PCIE_PWRMGMT_CHECK */
		pciedev->intstatus = 0;
	}
	/* PCIe DMA completion interrupts : H2D */
	if (pciedev->h2dintstatus) {
		/* dma from h2d side */
		if (pciedev->h2dintstatus & I_RI) {
			pciedev_handle_h2d_dma(pciedev);
		} else {
			PCI_ERROR(("bad h2d dma intstatus. 0x%04x\n",
				pciedev->h2dintstatus));
		}
		pciedev->h2dintstatus = 0;
	}
	/* PCIe DMA completion interrupts : D2H */
	if (pciedev->d2hintstatus) {
		if (pciedev->d2hintstatus & I_RI) {
			pciedev_handle_d2h_dma(pciedev);
		} else {
			PCI_ERROR(("bad d2h dma intstatus. 0x%04x\n",
				pciedev->d2hintstatus));
			while (1);
		}
		pciedev->d2hintstatus = 0;
	}
#endif /* PCIE_PHANTOM_DEV */
	if (pciedev->in_d3_suspend && pciedev->d3_ack_pending) {
		pciedev_D3_ack(pciedev);
	}


	/* Rescheduling if processing not over */
	if (pciedev->intstatus | pciedev->h2dintstatus | pciedev->d2hintstatus)
		resched = TRUE;
	return resched;
}
static void
BCMATTACHFN(pciedev_free_msgbuf_ring)(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf)
{
	uint8 i = 0;
	lcl_buf_pool_t * pool;

	if (msgbuf == NULL)
		return;
	pool = msgbuf->buf_pool;
	if (pool) {
		/* Free up individual buffers */
		for (i = 0; i < pool->buf_cnt; i++) {
			MFREE(pciedev->osh, pool->buf[i].p,
				pool->item_cnt * RING_LEN_ITEMS(msgbuf));
		}

		/* free up inuse buf pool */
		pciedev_free_inuse_bufpool(pciedev, pool);

		/* free up the pool structure */
		MFREE(pciedev->osh, pool, sizeof(lcl_buf_pool_t) +
			pool->buf_cnt * sizeof(lcl_buf_t));
	}
	bzero(msgbuf, sizeof(msgbuf_ring_t));
}


static bool
BCMATTACHFN(pciedev_allocate_msgbuf_ring)(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf_r,
	const char *name, uint8 bufcnt, uint8 item_cnt, bool phase_supported,
	bool dmarx_indices_supported, bool inusepool)
{
	uint8  name_len = 0;
	uint16 size = 0;
	uint8 i = 0;
	lcl_buf_pool_t * pool;

	name_len = strlen(name);

	if (name_len > (MSGBUF_RING_NAME_LEN - 1)) {
		name_len = MSGBUF_RING_NAME_LEN - 1;
	}

	bcopy(name, &msgbuf_r->name[0], name_len);

	msgbuf_r->pciedev = pciedev;
	msgbuf_r->phase_supported = phase_supported;
	msgbuf_r->dmarx_indices_supported = dmarx_indices_supported;
	msgbuf_r->current_phase =  MSGBUF_RING_INIT_PHASE;

	/* allocate linked List */
	size = sizeof(lcl_buf_pool_t) + bufcnt * sizeof(lcl_buf_t);
	pool = MALLOC(pciedev->osh, size);

	if (pool == NULL) {
		PCI_ERROR(("pciedev_allocate_msgbuf_ring: Failed to alloc msgbuf ring !\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return FALSE;
	}

	/* Bzero list items */
	bzero(pool, size);

	/* Initialize head/tail/free pointers */
	pool->head = &(pool->buf[0]);
	pool->free = &(pool->buf[0]);
	pool->tail = &(pool->buf[bufcnt - 1]);

	pool->buf_cnt = bufcnt;
	pool->item_cnt = item_cnt;

	pool->head->prv = NULL;
	pool->tail->nxt = NULL;

	/* Link up nodes */
	for (i = 1; i < bufcnt; i++) {
		pool->buf[i - 1].nxt = &(pool->buf[i]);
		pool->buf[i].prv = &(pool->buf[i - 1]);
	}
	msgbuf_r->buf_pool = pool;

	/* Initialize inuse pool */
	if (inusepool)
		pciedev_init_inuse_lclbuf_pool_t(pciedev, msgbuf_r);

	return TRUE;
}

static void
pciedev_sched_msgbuf_intr_process(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) t->context;
	pciedev_msgbuf_intr_process(pciedev);
}

static bool
pciedev_link_host_msgbuf_ring(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf,
	msgring_process_message_t process_rtn)
{
	uint8 i = 0;
	lcl_buf_pool_t * pool;

	if (msgbuf->inited == TRUE)
		return TRUE;

	if (LOW_ADDR_32(msgbuf->ringmem->base_addr) == 0) {

		PCI_ERROR(("ring: %c%c%c%c msgbuf_ring host base not inited\n",
			msgbuf->name[0], msgbuf->name[1],
			msgbuf->name[2], msgbuf->name[3]));
		return FALSE;
	}


	pool = msgbuf->buf_pool;
	for (i = 0; i < pool->buf_cnt; i++) {
		pool->buf[i].p = MALLOC(pciedev->osh, pool->item_cnt * RING_LEN_ITEMS(msgbuf));
	}
	pool->availcnt = pool->buf_cnt;
	msgbuf->process_rtn = process_rtn;
	msgbuf->inited = TRUE;

	return TRUE;
}

static void
BCMATTACHFN(pciedev_cleanup_common_msgbuf_rings)(struct dngl_bus *pciedev)
{
	pciedev_free_msgbuf_ring(pciedev, pciedev->htod_rx);
	pciedev_free_msgbuf_ring(pciedev, pciedev->htod_ctrl);
	pciedev_free_msgbuf_ring(pciedev, pciedev->dtoh_txcpl);
	pciedev_free_msgbuf_ring(pciedev, pciedev->dtoh_rxcpl);
	pciedev_free_msgbuf_ring(pciedev, pciedev->dtoh_ctrlcpl);
}

/* Device detach */
void
BCMATTACHFN(pciedev_detach)(struct dngl_bus *pciedev)
{
	PCI_TRACE(("pciedev_detach\n"));

	pciedev->up = FALSE;

	/* detach flow ring pool */
	dll_pool_detach(pciedev->osh, pciedev->flowring_pool, BCMPCIE_MAX_TX_FLOWS,
		sizeof(flowring_pool_t));

	pciedev_cleanup_common_msgbuf_rings(pciedev);
#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	pciedev_free_msgbuf_ring(pciedev, pciedev->htod_tx);
#else
	pciedev_free_flowrings(pciedev);
#endif /* BCMPCIE_SUPPORT_TX_PUSH_RING */

	/* Free device state */
	dngl_detach(pciedev->dngl);

	/* Put the core back into reset */
	pciedev_reset(pciedev);
	si_core_disable(pciedev->sih, 0);

	/* Detach from SB bus */
	si_detach(pciedev->sih);

	/* free ioct_resp_buf */
	if (pciedev->ioct_resp_buf)
		MFREE(pciedev->osh, pciedev->ioct_resp_buf, sizeof(*pciedev->ioct_resp_buf));

	/* Free chip state */
	MFREE(pciedev->osh, pciedev, sizeof(struct dngl_bus));
}

#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
static bool
BCMATTACHFN(pciedev_setup_txflow_msgbuf_ring)(struct dngl_bus *pciedev)
{
	bool success = FALSE;
	pciedev->htod_tx = &pciedev->txp_msgbuf;
	success = pciedev_allocate_msgbuf_ring(pciedev, pciedev->htod_tx,
			"h2dtx", 4, 32, FALSE, FALSE, FALSE);

	return success;
}
#endif /* PCIE_DEBUG_DUMP_MSGBUF_RINGINFO */

static const char BCMATTACHDATA(rstr_h2dctl)[] = "h2dctl";
static const char BCMATTACHDATA(rstr_h2drx)[]  = "h2drx";
static const char BCMATTACHDATA(rstr_d2htx)[]  = "d2htx";
static const char BCMATTACHDATA(rstr_d2hrx)[]  = "d2hrx";
static const char BCMATTACHDATA(rstr_d2hctl)[] = "d2hctl";
static bool
BCMATTACHFN(pciedev_setup_common_msgbuf_rings)(struct dngl_bus *pciedev)
{

	bool success = FALSE;
	bool d2h_phase_supported = FALSE;

#if defined(PCIE_M2M_D2H_SYNC)
	if (d2h_phase_supported == TRUE)
		goto fail;
#endif /* PCIE_M2M_D2H_SYNC */

	/* H2D control submission ...Please don't change the numbers here */
	pciedev->htod_ctrl = &pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT];
	success = pciedev_allocate_msgbuf_ring(pciedev, pciedev->htod_ctrl,
		rstr_h2dctl, 4, 1, FALSE, FALSE, FALSE);
	if (!success)
		goto fail;

	/* H2D RX POST */
	pciedev->htod_rx = &pciedev->cmn_msgbuf_ring[BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT];
	success = pciedev_allocate_msgbuf_ring(pciedev, pciedev->htod_rx,
		rstr_h2drx, pciedev->tunables[H2DRXPOST], 32, FALSE, FALSE, TRUE);
	if (!success)
		goto fail;

	/* D2H tx complete */
	pciedev->dtoh_txcpl = &pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_TX_COMPLETE];
	success = pciedev_allocate_msgbuf_ring(pciedev, pciedev->dtoh_txcpl,
		rstr_d2htx, pciedev->tunables[D2HTXCPL], MAX_TX_STATUS_COMBINED,
		d2h_phase_supported, FALSE, FALSE);
	if (!success)
		goto fail;

	/* D2H rx complete */
	pciedev->dtoh_rxcpl = &pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_RX_COMPLETE];
	success = pciedev_allocate_msgbuf_ring(pciedev, pciedev->dtoh_rxcpl,
		rstr_d2hrx, pciedev->tunables[D2HRXCPL], 32,
		d2h_phase_supported, FALSE, FALSE);
	if (!success)
		goto fail;

	/* D2H control completion */
	pciedev->dtoh_ctrlcpl = &pciedev->cmn_msgbuf_ring[BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE];
	success = pciedev_allocate_msgbuf_ring(pciedev, pciedev->dtoh_ctrlcpl,
		rstr_d2hctl, 8, 1, d2h_phase_supported, FALSE, FALSE);
	if (!success)
		goto fail;

	return success;
fail:
	pciedev_cleanup_common_msgbuf_rings(pciedev);
	return success;

}

#define PCIEDEV_MSGRING_INDEX_UNUSED	0xFF

/* Read htod and dtoh cbuf start loations from shared area */
/* host will update it before first interrupt to dongle */
int
pciedev_init_sharedbufs(struct dngl_bus *pciedev)
{
	bool init_needed = FALSE;

	init_needed |= !pciedev_link_host_msgbuf_ring(pciedev, pciedev->htod_ctrl,
		pciedev_handle_h2d_msg_ctrlsubmit);
	init_needed |= !pciedev_link_host_msgbuf_ring(pciedev, pciedev->htod_rx,
		pciedev_handle_h2d_msg_rxsubmit);
	init_needed |= !pciedev_link_host_msgbuf_ring(pciedev, pciedev->dtoh_ctrlcpl, NULL);
	init_needed |= !pciedev_link_host_msgbuf_ring(pciedev, pciedev->dtoh_txcpl, NULL);
	init_needed |= !pciedev_link_host_msgbuf_ring(pciedev, pciedev->dtoh_rxcpl, NULL);

#ifdef BCMPCIE_SUPPORT_TX_PUSH_RING
	init_needed |= !pciedev_link_host_msgbuf_ring(pciedev, pciedev->htod_tx,
		pciedev_handle_h2d_msg_txsubmit);
#endif
	if (pciedev->scratch_inited == FALSE) {
		pciedev->d2h_dma_scratchbuf_len = pciedev->pcie_sh->host_dma_scratch_buffer_len;
		if (pciedev->d2h_dma_scratchbuf_len > D2H_PD_RX_OFFSET)
			pciedev->d2h_dma_scratchbuf_len = D2H_PD_RX_OFFSET;

		if (pciedev->d2h_dma_scratchbuf_len != 0) {
			PHYSADDR64HISET(pciedev->d2h_dma_scratchbuf,
				(uint32) HIGH_ADDR_32(pciedev->pcie_sh->host_dma_scratch_buffer));
			PHYSADDR64LOSET(pciedev->d2h_dma_scratchbuf,
				(uint32) LOW_ADDR_32(pciedev->pcie_sh->host_dma_scratch_buffer));
			pciedev->d2h_dma_rxoffset = 0;
		}
		pciedev->scratch_inited = TRUE;
	}
	if (pciedev->d2h_ringupd_inited == FALSE) {
		pciedev->d2h_ringupd_buf_len = pciedev->pcie_sh->device_rings_stsblk_len;

		if (pciedev->d2h_ringupd_buf_len > pciedev->local_d2h_ringupd_size)
			pciedev->d2h_ringupd_buf_len =  pciedev->local_d2h_ringupd_size;

		if (pciedev->d2h_ringupd_buf_len != 0) {
			PHYSADDR64HISET(pciedev->d2h_ringupd_buf,
				(uint32) HIGH_ADDR_32(pciedev->pcie_sh->device_rings_stsblk));
			PHYSADDR64LOSET(pciedev->d2h_ringupd_buf,
				(uint32) LOW_ADDR_32(pciedev->pcie_sh->device_rings_stsblk));
		}
		pciedev->d2h_ringupd_inited = TRUE;
	}
	return init_needed;
}


static uint32 health_check_time = 1;

static void
pciedev_health_check_timerfn(dngl_timer_t *t)
{
	health_check_time++;
	/* Leave this as a printf for now, no debug check */
	EVENT_LOG(EVENT_LOG_TAG_PCI_DBG,
	"%d, rxcpl_pend_cnt %d, rxcpl list count %d, flush call count %d\n", health_check_time,
	((struct dngl_bus *) t->context)->rxcpl_pend_cnt, g_rxcplid_list->avail,
	pciedev_reorder_flush_cnt);
#ifdef TEST_DROP_PCIEDEV_RX_FRAMES
	printf("TEST IN PROGRESS: (drop) rxframes %d, no rxcpl frames %d, rxcpl avail %d\n",
		pciedev_test_dropped_rxframes, pciedev_test_dropped_norxcpls,
		g_rxcplid_list->avail);
#endif
}

static void
pciedev_bm_enab_timerfn(dngl_timer_t *t)
{
	pciedev_t *pciedev = (pciedev_t *) t->context;

	if (pciedev_handle_d0_enter_bm(pciedev) != BCME_OK) {
		dngl_add_timer(pciedev->bm_enab_timer,
			pciedev->bm_enab_check_interval, FALSE);
	}
	else {
		pciedev->bm_enab_timer_on = FALSE;
	}
}

/* pciedev device reset */
static void
pciedev_reset(struct dngl_bus *pciedev)
{

#ifdef PCIE_PHANTOM_DEV
	if (pciedev->phtm)
		pcie_phtm_reset(pciedev->phtm);
#else

#endif
	/* Reset core */
	si_core_reset(pciedev->sih, 0, 0);
	pciedev->intstatus = 0;
}

static uint16
pciedev_handle_h2d_msg_ctrlsubmit(struct dngl_bus *pciedev, void *p, uint16 pktlen,
	msgbuf_ring_t *msgbuf)
{
	cmn_msg_hdr_t * msg;
	uint8 msgtype;
	uint8 msglen = RING_LEN_ITEMS(pciedev->htod_ctrl);
	bool evnt_ctrl = 0;
	if (pktlen % msglen) {
			PCI_TRACE(("htod_ctrl ring HtoD%c  BAD: pktlen %d"
				"doesn't reflect item multiple %d\n",
				pciedev->htod_ctrl->name[3], pktlen, msglen));
		return -1;
	}
	/* Check if msgtype is IOCT PYLD. This needs to be processed
	 *  irrespective of the state of ioctl lock
	 */
	if (pciedev->ioctl_lock == TRUE) {
		msg = (cmn_msg_hdr_t *)p;
		msgtype = msg->msg_type;
		if (msgtype == MSG_TYPE_IOCTLPTR_REQ) {
			PCI_ERROR(("something wrong Ioctl Lock on...still gettng "
				"ioctl request to process %d!\n", msgtype));
			return -1;
		}
	}

	/* Increment corresponding IPC Stat */
	pciedev->metrics.num_submissions += pktlen / msglen;
	PCI_INFORM(("new ctrlsubmit:%d, num_submissions:%d\n",
		pktlen / msglen, pciedev->metrics.num_submissions));

	while (pktlen) {
		/* extract out message type and length */
		msg = (cmn_msg_hdr_t *)p;
		msgtype = msg->msg_type;

		switch (msgtype) {
			case MSG_TYPE_IOCTLPTR_REQ:
				if (pciedev->ioctl_lock == TRUE) {
					PCI_TRACE(("Ioctl Lock on...got ioctl req %d!\n", msgtype));
					ASSERT(0);
					break;
				}
				PCI_TRACE(("MSG_TYPE_IOCTLPTR_REQ\n"));
				pciedev_process_ioctlptr_rqst(pciedev, p);
				break;
			case MSG_TYPE_IOCTLRESP_BUF_POST:
				PCI_TRACE(("MSG_TYPE_IOCTLRESP_BUF_POST\n"));
				if (!pciedev_rxbufpost_msg(pciedev, p, pciedev->ioctl_resp_pool)) {
					evnt_ctrl = PCIE_ERROR1;
					pciedev_send_ring_status(pciedev, 0,
						BCMPCIE_MAX_IOCTLRESP_BUF,
						BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);
				}
				break;

			case MSG_TYPE_EVENT_BUF_POST:
				PCI_TRACE(("MSG_TYPE_EVENT_BUF_POST\n"));
				if (!pciedev_rxbufpost_msg(pciedev, p, pciedev->event_pool)) {
					evnt_ctrl = PCIE_ERROR2;
					pciedev_send_ring_status(pciedev, 0,
						BCMPCIE_MAX_EVENT_BUF,
						BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);
				}
				break;
#ifdef PCIE_DMAXFER_LOOPBACK
			case MSG_TYPE_LPBK_DMAXFER :
				evnt_ctrl = PCIE_ERROR3;
				pciedev_process_lpbk_dmaxfer_msg(pciedev, p);
				break;
#endif /* PCIE_DMAXFER_LOOPBACK */
			case MSG_TYPE_FLOW_RING_CREATE :
				PCI_TRACE(("MSG_TYPE_FLOW_RING_CREATE\n"));
				pciedev_process_flow_ring_create_rqst(pciedev, p);
				break;

			case MSG_TYPE_FLOW_RING_DELETE:
				PCI_TRACE(("MSG_TYPE_FLOW_RING_DELETE\n"));
				pciedev_process_flow_ring_delete_rqst(pciedev, p);
				break;

			case MSG_TYPE_FLOW_RING_FLUSH:
				PCI_TRACE(("MSG_TYPE_FLOW_RING_FLUSH\n"));
				pciedev_process_flow_ring_flush_rqst(pciedev, p);
				break;
			default:
				pciedev_send_ring_status(pciedev, 0, BCMPCIE_BADOPTION,
					BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);
				PCI_TRACE(("Unknown MSGTYPE ctrl submit ring: %d %d %d\n",
					msgtype, msglen, pktlen));
				bcm_print_bytes("unknown message", p, 8);
				break;
		}

		if (evnt_ctrl) {
				PCI_ERROR(("max limit of event %d\n", msgtype));
		}
		pktlen = pktlen - msglen;
		p = p + msglen;
	}
	return pktlen;
}

/* send ioctl response over bus */
void
pciedev_bus_sendctl(struct dngl_bus *pciedev, void *p)
{
#ifdef UART_TRANSPORT
	if (pciedev->uarttrans_enab && PKTALTINTF(p)) {
		h5_send_msgbuf(PKTDATA(pciedev->osh, p), PKTLEN(pciedev->osh, p),
		               MSG_TYPE_IOCTL_CMPLT, 0);
		PKTFREE(pciedev->osh, p, TRUE);
		return;
	}
#endif /* UART_TRANSPORT */
	pciedev_send_ioctl_completion(pciedev, p);
}

static int
pciedev_tx_ioctl_pyld(struct dngl_bus *pciedev, void *p, ret_buf_t *ret_buf,
	uint16 data_len, uint8 msgtype)
{
	dma64addr_t addr = {0, 0};
	uint16 msglen;
	uint8 txdesc = 0;
	uint8 rxdesc = 0;
	bool commit = TRUE;
	void *buf;

	/* Initialize the buf ptr with the ioctl payload addr. This could either be */
	/* from the externally allocated ioct_resp_buf or from the packet itself */
	buf = pciedev->ioct_resp_buf ? pciedev->ioct_resp_buf : PKTDATA(pciedev->osh, p);

	if ((data_len > 0) && (data_len < PCIE_MEM2MEM_DMA_MIN_LEN)) {
		data_len = PCIE_MEM2MEM_DMA_MIN_LEN;
	}
	msglen = data_len;

#ifdef PCIE_PHANTOM_DEV
	/* For phantom devices PCIE_ADDR_OFFSET is still required for sdio dma */
	/* for pcie dma devices, outbound dma is hardwired */
	/* to take in only pcie addr as dest space. so no need to give extra offset */
	if (pciedev->phtm) {
		PHYSADDR64HISET(addr, data_buf->high_addr);
		PHYSADDR64LOSET(addr, data_buf->low_addr);
		pcie_phtm_tx(pciedev->phtm, buf, addr, msglen, l_msgtype, &txdesc, &rxdesc);
	}
#else
	ret_buf->high_addr = ret_buf->high_addr & ~PCIE_ADDR_OFFSET;
	/* Program RX descriptor */
	if (pciedev->d2h_dma_scratchbuf_len) {
		if (dma_rxfast(pciedev->d2h_di, pciedev->d2h_dma_scratchbuf, 8)) {
			PCI_ERROR(("pciedev_tx_ioctl_pyld: dma_rxfast failed for rxoffset,"
				"descs_avail %d\n", PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
		} else {
			rxdesc++;
		}
	}
	PHYSADDR64HISET(addr, ret_buf->high_addr);
	PHYSADDR64LOSET(addr, ret_buf->low_addr);
	/* r buffer for data excluding the phase bits */
	if (dma_rxfast(pciedev->d2h_di, addr, (uint32)(msglen))) {
		PCI_ERROR(("pciedev_tx_ioctl_pyld: dma_rxfast failed for data, descs avail %d\n",
			PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC)));
	} else {
		rxdesc++;
	}

	PHYSADDR64HISET(addr, (uint32) 0);
	PHYSADDR64LOSET(addr, (uint32) buf);
	if (dma_msgbuf_txfast(pciedev->d2h_di, addr, commit, msglen, TRUE, commit)) {
		PCI_ERROR(("pciedev_tx_ioctl_pyld: dma fill failed  \n"));
		ASSERT(0);
	} else {
		txdesc++;
	}
#endif /* PCIE_PHANTOM_DEV */

	pciedev_dma_tx_account(pciedev, msglen, msgtype, 0, NULL, txdesc, rxdesc);

	return TRUE;
}

#ifndef PCIE_PHANTOM_DEV
static void
pciedev_usrltrtest_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) t->context;
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state, pciedev->usr_ltr_test_state);
}

static uint32
pciedev_usr_test_ltr(struct dngl_bus *pciedev, uint32 timer_val, uint8 state)
{
	/* don't accept values greater then 10 seconds */
	if (timer_val > 10000000)
		return BCME_RANGE;

	if (state > LTR_ACTIVE)
		return BCME_RANGE;

	/* test already in progress */
	if (pciedev->usr_ltr_test_pend) {
		hndrte_del_timer(pciedev->ltr_test_timer);
		pciedev->usr_ltr_test_pend = FALSE;
	}
	pciedev->usr_ltr_test_pend = TRUE;
	pciedev->usr_ltr_test_state = state;
	dngl_add_timer(pciedev->ltr_test_timer, timer_val, FALSE);

	return BCME_OK;
}
#endif /* PCIE_PHANTOM_DEV */

static uint32
pciedev_get_pwrstats(struct dngl_bus *pciedev, char *buf)
{
	wl_pwr_pcie_stats_t pcie_tuple;

	pciedev->metrics.active_dur += hndrte_time() - pciedev->metric_ref.active;
	pciedev->metric_ref.active = hndrte_time();
	pciedev->metrics.timestamp = pciedev->metric_ref.active;

	/* Get all link state counters and durations */
	if (PCIECOREREV(pciedev->corerev) >= 7) {
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L0_EVENTCNT);
		pciedev->metrics.l0_cnt = R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L0_STATETMR);
		pciedev->metrics.l0_usecs = R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L1_EVENTCNT);
		pciedev->metrics.l1_cnt = R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L1_STATETMR);
		pciedev->metrics.l1_usecs = R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L1_1_EVENTCNT);
		pciedev->metrics.l1_1_cnt = R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L1_1_STATETMR);
		pciedev->metrics.l1_1_usecs =
			R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L1_2_EVENTCNT);
		pciedev->metrics.l1_2_cnt = R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L1_2_STATETMR);
		pciedev->metrics.l1_2_usecs =
			R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L2_EVENTCNT);
		pciedev->metrics.l2_cnt = R_REG(pciedev->osh, &pciedev->regs->configdata);
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L2_STATETMR);
		pciedev->metrics.l2_usecs = R_REG(pciedev->osh, &pciedev->regs->configdata);
	}

	/* IPC Stats */
	/* Submission and completions counters updated throughout pciedev.c */

	/* Copy it into the tuple form, then that into the return buffer */
	pcie_tuple.type = WL_PWRSTATS_TYPE_PCIE;
	pcie_tuple.len = sizeof(pcie_tuple);
	ASSERT(sizeof(pciedev->metrics) == sizeof(pcie_tuple.pcie));
	memcpy(&pcie_tuple.pcie, &pciedev->metrics, sizeof(pcie_tuple.pcie));
	memcpy(buf, &pcie_tuple, sizeof(pcie_tuple));
	return sizeof(pcie_tuple);
}

static uint32
pciedev_get_avail_host_rxbufs(struct dngl_bus *pciedev)
{
	return 0;
}

static void pciedev_dump_err_cntrs(struct dngl_bus * pciedev)
{
	printf("======= pciedev error counters =======\n");
	printf("mem_alloc_err_cnt: %d\n", pciedev->err_cntr->mem_alloc_err_cnt);
	printf("rxcpl_err_cnt: %d\n", pciedev->err_cntr->rxcpl_err_cnt);
	printf("dma_err_cnt: %d\n", pciedev->err_cntr->dma_err_cnt);
	printf("pktfetch_cancel_cnt: %d\n", pciedev->err_cntr->pktfetch_cancel_cnt);
}

uint32
pciedev_bus_iovar(struct dngl_bus *pciedev, char *buf, uint32 inlen, uint32 *outlen, bool set)
{
	char *cmd = buf + 4;
	int index = 0;
	int offset = 0;
	uint32 val = 0;

	index = 0;
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

	if (!strcmp(cmd, "maxhostrxbufs")) {
			index = MAXHOSTRXBUFS;
	} else if (!strcmp(cmd, "maxtxstatus")) {
			index = MAXTXSTATUS;
	} else if (!strcmp(cmd, "maxrxcmplt")) {
			index = MAXRXCMPLT;
#ifdef PCIE_DMAXFER_LOOPBACK
	} else if (!strcmp(cmd, "txlpbk_pkts")) {
		if (set)
			pciedev->txlpbk_pkts = val;
		else
			memcpy(buf, &pciedev->txlpbk_pkts, sizeof(uint32));
		return BCME_OK;
#endif /* PCIE_DMAXFER_LOOPBACK */
#if defined(BCM_BUZZZ)
	/* BUZZZ support for OL(CR4) */
	} else if (!strcmp(cmd, "buzzz_start")) {
		BUZZZ_START(); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_stop")) {
		BUZZZ_STOP(); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_ctr")) {
		if (set) { val = (val == 0) ? 0xFF : val; } /* 0xFF = cycles cnt */
		BUZZZ_CCTR(val); return BCME_OK;
#endif /* BCM_BUZZZ */
	} else if (!strcmp(cmd, "dropped_txpkts")) {
		if (set)
			pciedev->dropped_txpkts = val;
		else
			memcpy(buf, &pciedev->dropped_txpkts, sizeof(uint32));
		return BCME_OK;
#ifndef PCIE_PHANTOM_DEV
	} else if (!strcmp(cmd, "err_cntrs")) {
			pciedev_dump_err_cntrs(pciedev);
	} else if (!strcmp(cmd, "ltr_state")) {
		if (set) {
			if (val > 2)
				val = 2;
			W_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state, val);
		}
		else {
			val = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state);
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "link_l1_active")) {
		if (set) {
			return pciedev_usr_test_ltr(pciedev, val, LTR_ACTIVE);
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "link_l1_idle")) {
		if (set) {
			return pciedev_usr_test_ltr(pciedev, val, LTR_ACTIVE_IDLE);
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "link_l1_sleep")) {
		if (set) {
			return pciedev_usr_test_ltr(pciedev, val, LTR_SLEEP);
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "ds_enter_req")) {
		if (set) {
			pciedev_deepsleep_enter_req(pciedev);
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "deepsleep_disable")) {
		if (set) {
			if (val)
				pciedev_disable_deepsleep(pciedev, TRUE);
			else
				pciedev_disable_deepsleep(pciedev, FALSE);
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "ds_exit_notify")) {
		if (set) {
			pciedev_deepsleep_exit_notify(pciedev);
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
#ifdef PCIE_DELAYED_HOSTWAKE
	} else if (!strcmp(cmd, "delayed_hostwake")) {
		if (set) {
			if (val > 300)
				return BCME_BADARG;
			pciedev_delayed_hostwake(pciedev, val);
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
#endif /* PCIE_DELAYED_HOSTWAKE */
#endif /* PCIE_PHANTOM_DEV */
	} else if (!strcmp(cmd, "dumpring")) {
		uint8 i = 0;
		for (i = 0; i <= BCMPCIE_COMMON_MSGRING_MAX_ID; i++) {
			val = pciedev_dump_lclbuf_pool(&pciedev->cmn_msgbuf_ring[i]);
		}
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "avail_hostbufs")) {
		val = pciedev_get_avail_host_rxbufs(pciedev);
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "force_no_rx_metadata")) {
		if (set) {
			/* force_metadata_len to 0 */
			pciedev->force_no_rx_metadata = val ? 1 : 0;
		}
		else {
			val = pciedev->force_no_rx_metadata;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "force_no_tx_metadata")) {
		if (set) {
			/* force_metadata_len to 0 */
			pciedev->force_no_tx_metadata = val ? 1 : 0;
		}
		else {
			val = pciedev->force_no_tx_metadata;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "sim_host_enter_d3")) {
		hndrte_notify_devpwrstchg((val ? FALSE : TRUE));
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_ringupd_blk")) {
		uchar *ptr = (uchar *)pciedev->local_d2h_ringupd_dmabuf.loaddr;
		bcm_print_bytes("ringupd", ptr, pciedev->local_d2h_ringupd_size);
		return BCME_OK;
	} else if (!strcmp(cmd, "dumptxrings")) {
		uint16 i = 0;
		val = BCME_OK;
		for (i = 0; i < BCMPCIE_MAX_TX_FLOWS; i++) {
			if (pciedev->flow_ring_msgbuf[i].inited)
				val =
				pciedev_dump_txring_msgbuf_ring_info(&pciedev->flow_ring_msgbuf[i]);
		}
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "dumppriorings")) {
		val = pciedev_dump_prioring_info(pciedev);
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "dumprwblob")) {
		val = pciedev_dump_rw_blob(pciedev);
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "start_check")) {
		if (set) {
			if (pciedev->health_check_timer_on == FALSE) {
				pciedev->health_check_timer_on = TRUE;
				dngl_add_timer(pciedev->health_check_timer, val, TRUE);
			}
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "stop_check")) {
		if (pciedev->health_check_timer_on == TRUE) {
			pciedev->health_check_timer_on = FALSE;
			dngl_del_timer(pciedev->health_check_timer);
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "pwrstats")) {
		uint16 type;

		if (set) {
			bzero(&pciedev->metrics, sizeof(pciedev->metrics));
			bzero(&pciedev->metric_ref, sizeof(pciedev->metric_ref));

			printf("\nd3_suspend\t%u\nd0_resume\t\t%u\n"
				"perst_assrt\t%u\nperst_deassrt\t%u\n"
				"active_dur\t%u\nd3_susp_dur\t%u\nperst_dur\t\t%u\n"
				"h2d_drbl\t\t%u,\td2h_drbl\t\t%u\n"
				"num_submissions\t%u,\tnum_completions\t%u\n"
				"timestamp\t%u\n",
				pciedev->metrics.d3_suspend_ct, pciedev->metrics.d0_resume_ct,
				pciedev->metrics.perst_assrt_ct, pciedev->metrics.perst_deassrt_ct,
				pciedev->metrics.active_dur ?
					pciedev->metrics.active_dur : hndrte_time(),
				pciedev->metrics.d3_suspend_dur, pciedev->metrics.perst_dur,
				pciedev->metrics.num_h2d_doorbell,
				pciedev->metrics.num_d2h_doorbell,
				pciedev->metrics.num_submissions, pciedev->metrics.num_completions,
				pciedev->metrics.timestamp);

			return BCME_OK;
		}
		if (inlen < offset + sizeof(uint16))
			return BCME_ERROR;
		if (inlen < ROUNDUP(sizeof(wl_pwr_pcie_stats_t), sizeof(uint32)))
			return BCME_BUFTOOSHORT;

		memcpy(&type, &buf[offset], sizeof(uint16));
		if (type != WL_PWRSTATS_TYPE_PCIE)
			return BCME_BADARG;

		*outlen = pciedev_get_pwrstats(pciedev, buf);
		return BCME_OK;
	} else if (!strcmp(cmd, "disconnect") && set) {
		if (val == 102) {
			while (1) { }
		}
		else if (val == 99) {
			traptest();
			return BCME_OK;
		} else
			return BCME_UNSUPPORTED;
#ifdef UART_TRANSPORT
	} else if (!strcmp(cmd, "uarttransport")) {
		if (set) {
			if (val == 0 || val == 1 || val == 2) {
				if (val && !hndrte_cons_uarttransport(val))
					return BCME_NOMEM;
				pciedev->uarttrans_enab = val;
			} else
				return BCME_BADARG;
		} else {
			val = pciedev->uarttrans_enab;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "uart_evtmsgs")) {
		if (set) {
			if (inlen >= sizeof(pciedev->uart_event_inds_mask)) {
				memcpy(&pciedev->uart_event_inds_mask[0], &buf[offset],
				       sizeof(pciedev->uart_event_inds_mask));
			} else
				return BCME_BUFTOOSHORT;
		} else {
			memcpy(buf, &pciedev->uart_event_inds_mask[0],
			       sizeof(pciedev->uart_event_inds_mask));
			*outlen = sizeof(pciedev->uart_event_inds_mask);
		}
		return BCME_OK;
#endif /* UART_TRANSPORT */
	} else if (!strcmp(cmd, "lpback")) {
		if (set)
			pciedev->lpback_test_mode = val;
		else {
			memcpy(buf, &pciedev->lpback_test_mode,
				sizeof(pciedev->lpback_test_mode));
			*outlen = sizeof(pciedev->lpback_test_mode);
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_ds_log")) {
		return (pciedev_deepsleep_engine_log_dump(pciedev));
	}  else {
			return BCME_UNSUPPORTED;
	}
	if (set) {
		pciedev->tunables[index] = val;
	} else {
		memcpy(buf, &pciedev->tunables[index], sizeof(uint32));
	}

	return BCME_OK;
}

struct pcie_phtm *
pciedev_phtmdev(struct dngl_bus *pciedev)
{
	return (pciedev->phtm);
}
#ifndef PCIE_PHANTOM_DEV
static const char BCMATTACHDATA(rstr_H2D)[] = "H2D";
static const char BCMATTACHDATA(rstr_D2H)[] = "D2H";
static int
BCMATTACHFN(pciedev_pciedma_init)(struct dngl_bus *p_pcie_dev)
{
	pcie_devdmaregs_t *dmaregs;
	osl_t *osh = p_pcie_dev->osh;
	si_t *sih = p_pcie_dev->sih;
	uint32 burstlen;

	dmaregs = &p_pcie_dev->regs->u.pcie2.h2d0_dmaregs;

	p_pcie_dev->h2d_di = dma_attach(osh, rstr_H2D, sih, &dmaregs->tx, &dmaregs->rx,
		p_pcie_dev->tunables[NTXD], p_pcie_dev->tunables[NRXD],
		p_pcie_dev->tunables[RXBUFSZ], PD_RX_HEADROOM, PD_NRXBUF_POST,
		p_pcie_dev->h2d_dma_rxoffset, &pd_msglevel);

	if (p_pcie_dev->h2d_di == NULL) {
		PCI_ERROR(("dma_attach fail for agg desc h2d\n"));
		return -1;
	}
	/* supports multiple outstanding reads */
	if (PCIECOREREV(p_pcie_dev->corerev) == 7 || PCIECOREREV(p_pcie_dev->corerev) == 11 ||
		PCIECOREREV(p_pcie_dev->corerev) == 9 || PCIECOREREV(p_pcie_dev->corerev) == 13) {
		dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_TX_MULTI_OUTSTD_RD, DMA_MR_4);
	} else {
		dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_TX_MULTI_OUTSTD_RD, DMA_MR_2);
	}

	burstlen = pcie_h2ddma_tx_get_burstlen(p_pcie_dev);
	dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_TX_BURSTLEN, burstlen);

	dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_TX_PREFETCH_CTL, 1);
	dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_TX_PREFETCH_THRESH, 3);
	dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_TX_BURSTLEN, 2);


	dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_RX_PREFETCH_CTL, 1);
	dma_param_set(p_pcie_dev->h2d_di, HNDDMA_PID_RX_PREFETCH_THRESH, 1);

	dma_rxreset(p_pcie_dev->h2d_di);
	dma_txreset(p_pcie_dev->h2d_di);

	dma_rxinit(p_pcie_dev->h2d_di);
	dma_txinit(p_pcie_dev->h2d_di);

	/* TX/RX descriptor avail ptrs */
	p_pcie_dev->avail[HTOD][TXDESC] = (uint *) dma_getvar(p_pcie_dev->h2d_di, "&txavail");
	p_pcie_dev->avail[HTOD][RXDESC] = (uint *) dma_getvar(p_pcie_dev->h2d_di, "&rxavail");

	/* D2H aggdesc update through host2dev dma engine 0 */
	dmaregs = &p_pcie_dev->regs->u.pcie2.d2h0_dmaregs;

	p_pcie_dev->d2h_di = dma_attach(osh, rstr_D2H, sih, &dmaregs->tx, &dmaregs->rx,
		p_pcie_dev->tunables[NTXD], p_pcie_dev->tunables[NRXD],
		p_pcie_dev->tunables[RXBUFSZ], PD_RX_HEADROOM, PD_NRXBUF_POST,
		p_pcie_dev->d2h_dma_rxoffset, &pd_msglevel);

	if (p_pcie_dev->d2h_di == NULL) {
		PCI_ERROR(("dma_attach fail for agg desc d2h\n"));
		return -1;
	}

	dma_param_set(p_pcie_dev->d2h_di, HNDDMA_PID_RX_PREFETCH_CTL, 1);
	dma_param_set(p_pcie_dev->d2h_di, HNDDMA_PID_RX_PREFETCH_THRESH, 1);
	dma_param_set(p_pcie_dev->d2h_di, HNDDMA_PID_RX_BURSTLEN, 2);

	dma_param_set(p_pcie_dev->d2h_di, HNDDMA_PID_TX_PREFETCH_CTL, 1);
	dma_param_set(p_pcie_dev->d2h_di, HNDDMA_PID_TX_PREFETCH_THRESH, 1);
	dma_param_set(p_pcie_dev->d2h_di, HNDDMA_PID_TX_BURSTLEN, 2);

	dma_rxreset(p_pcie_dev->d2h_di);
	dma_txreset(p_pcie_dev->d2h_di);

	dma_rxinit(p_pcie_dev->d2h_di);
	dma_txinit(p_pcie_dev->d2h_di);

	/* TX/RX descriptor avail ptrs */
	p_pcie_dev->avail[DTOH][TXDESC] = (uint *) dma_getvar(p_pcie_dev->d2h_di, "&txavail");
	p_pcie_dev->avail[DTOH][RXDESC] = (uint *) dma_getvar(p_pcie_dev->d2h_di, "&rxavail");
	return 0;
}

#endif /* PCIE_PHANTOM_DEV */
#ifdef PCIE_PHANTOM_DEV
static uint16
pciedev_htoddma_pktlen(struct dngl_bus *pciedev, msgbuf_ring_t **msgbuf)
{
	uint16 rd_ix = pciedev->htod_dma_rd_idx;
	pciedev->htod_dma_rd_idx = (rd_ix + 1) % MAX_DMA_QUEUE_LEN_H2D;
	*msgbuf = pciedev->htod_dma_q[rd_ix].msgbuf;
	return (pciedev->htod_dma_q[rd_ix].len);
}
#endif

#ifdef PCIE_DMAXFER_LOOPBACK
uint32
pciedev_process_do_dest_lpbk_dmaxfer_done(struct dngl_bus *pciedev, void *p)
{
	/* once done go back to loop, send the completion message */
	pciedev->lpbk_dmaxfer_push_pend--;

	/* put back the packet into the lpbk_dma_txq after pushing the header */
	PKTPUSH(pciedev->osh, p, sizeof(struct fetch_rqst));
	PKTSETLEN(pciedev->osh, p, pciedev->lpbk_dma_pkt_fetch_len);
	PCI_TRACE(("D2H DMA done for packet %p, still pendind %d, len %d\n",
		(uint32)p, pciedev->lpbk_dmaxfer_push_pend, PKTLEN(pciedev->osh, p)));
	pktq_penq(&pciedev->lpbk_dma_txq, 0, p);

	if (pciedev->lpbk_dmaxfer_push_pend == 0) {
		PCI_ERROR(("D2H DMA done for pending\n"));
		pciedev_schedule_src_lpbk_dmaxfer(pciedev);
	}
	return 0;
}

int
pciedev_process_d2h_dmaxfer_pyld(struct dngl_bus *pciedev, void *p)
{
	ret_buf_t haddr;
	uint32 pktlen;

	PKTPULL(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
	pktlen = PKTLEN(pciedev->osh, p);

	haddr.high_addr = pciedev->pend_dmaxfer_destaddr_hi;
	haddr.low_addr = pciedev->pend_dmaxfer_destaddr_lo;
	bcm_add_64(&pciedev->pend_dmaxfer_destaddr_hi, &pciedev->pend_dmaxfer_destaddr_lo, pktlen);

	PCI_TRACE(("sceduled the DMA  for packet %p, of len %d\n", (uint32)p, pktlen));
	pciedev_tx_pyld(pciedev, p, &haddr, D2H_MSGLEN(pciedev, pktlen), NULL, 0,
		MSG_TYPE_LPBK_DMAXFER_PYLD);
	return 0;
}

static void
pciedev_lpbk_dest_dmaxfer_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) t->context;

	pciedev_process_do_dest_lpbk_dmaxfer(pciedev);
}

static void
pciedev_lpbk_src_dmaxfer_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) t->context;

	pciedev_process_do_src_lpbk_dmaxfer(pciedev);
}

static uint32
pciedev_process_do_dest_lpbk_dmaxfer(struct dngl_bus *pciedev)
{
	cmn_msg_hdr_t *msg;
	void *p;

	/* schedule a d2h req and let it finish */
	if (pktq_empty(&pciedev->lpbk_dma_txq_pend)) {
		PCI_ERROR(("pciedev_process_do_dest_lpbk_dmaxfer:"
			"PEND PKTQ is empty why..so return\n"));
	}
	while (!pktq_empty(&pciedev->lpbk_dma_txq_pend)) {
		p = pktq_deq(&pciedev->lpbk_dma_txq_pend, 0);
		msg = (cmn_msg_hdr_t *)PKTPUSH(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
		msg->msg_type = MSG_TYPE_LPBK_DMAXFER_PYLD;
		pciedev->lpbk_dmaxfer_push_pend++;
		PCI_TRACE(("now trying "
			"to push the packet %p, len %d, pend count %d\n",
			(uint32)p, PKTLEN(pciedev->osh, p),
			pciedev->lpbk_dmaxfer_push_pend));
		pciedev_queue_d2h_req(pciedev, p);
	}
	return 0;
}

static void
pciedev_process_src_lpbk_dmaxfer_done(struct fetch_rqst *fr, bool cancelled)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)fr->ctx;
	/* buffer is in the dongle */
	if (cancelled) {
		PCI_ERROR(("pciedev_process_src_lpbk_dmaxfer_done: Request"
			" cancelled! Dropping lpbk dmaxfer rqst...\n"));
		PCIEDEV_PKTFETCH_CANCEL_CNT_INCR(pciedev);
		ASSERT(0);
		return;
	}

	/* schedule the timer to move this request to queue or move it to queue */
	pciedev->lpbk_dmaxfer_fetch_pend--;
	/* now the whole thing is in local memory */
	if (pciedev->lpbk_dmaxfer_fetch_pend == 0) {
		PCI_TRACE(("fetch of the one blob done, so push it now \n"));
		if (pciedev->lpbk_dma_dest_delay)
			dngl_add_timer(pciedev->lpbk_dest_dmaxfer_timer,
			pciedev->lpbk_dma_dest_delay, FALSE);
		else
			pciedev_process_do_dest_lpbk_dmaxfer(pciedev);
	}
}

static uint32
pciedev_process_do_src_lpbk_dmaxfer(struct dngl_bus *pciedev)
{
	struct fetch_rqst *fetch_rqst;
	void *p;
	uint32 buflen;


	if (pktq_empty(&pciedev->lpbk_dma_txq)) {
		PCI_TRACE(("pciedev_process_do_src_lpbk_dmaxfer: PKTQ is empty\n"));
	}
	while ((pciedev->pend_dmaxfer_len != 0) && (!pktq_empty(&pciedev->lpbk_dma_txq))) {
		p = pktq_deq(&pciedev->lpbk_dma_txq, 0);

		fetch_rqst = (struct fetch_rqst *)PKTDATA(pciedev->osh, p);

		PKTPULL(pciedev->osh, p, sizeof(struct fetch_rqst));

		if (pciedev->pend_dmaxfer_len > MAX_DMA_XFER_SZ)
			buflen = MAX_DMA_XFER_SZ;
		else
			buflen = pciedev->pend_dmaxfer_len;

		if (pciedev->pend_dmaxfer_len - buflen < 8)
			buflen += (pciedev->pend_dmaxfer_len - buflen);

		PKTSETLEN(pciedev->osh, p, buflen);

		pciedev->pend_dmaxfer_len -= buflen;

		PHYSADDR64HISET(fetch_rqst->haddr, pciedev->pend_dmaxfer_srcaddr_hi);
		PHYSADDR64LOSET(fetch_rqst->haddr, pciedev->pend_dmaxfer_srcaddr_lo);
		bcm_add_64(&pciedev->pend_dmaxfer_srcaddr_hi,
			&pciedev->pend_dmaxfer_srcaddr_lo, buflen);

		fetch_rqst->size = buflen;
		fetch_rqst->dest = PKTDATA(pciedev->osh, p);
		fetch_rqst->cb = pciedev_process_src_lpbk_dmaxfer_done;
		fetch_rqst->ctx = (void *)pciedev;

		pktq_penq(&pciedev->lpbk_dma_txq_pend, 0, p);
		pciedev->lpbk_dmaxfer_fetch_pend++;
		PCI_TRACE(("sending a request for %d bytes at host addr 0x%04x, "
			"buffer %p, fetch pend count is %d\n",
			buflen, pciedev->pend_dmaxfer_srcaddr_lo, (uint32)p,
			pciedev->lpbk_dmaxfer_fetch_pend));
		PCI_TRACE(("fetchrequest is at %p, and pktdata is %p\n",
			(uint32)fetch_rqst, (uint32)PKTDATA(pciedev->osh, p)));
#ifdef BCMPCIEDEV_ENABLED
		hndrte_fetch_rqst(fetch_rqst);
#endif
	}
	return 0;
}

static uint32
pciedev_schedule_src_lpbk_dmaxfer(struct dngl_bus *pciedev)
{

	if (pciedev->pend_dmaxfer_len == 0) {
		PCI_TRACE(("done with the dmalpbk request..so clean it up now\n"));
		pciedev_done_lpbk_dmaxfer(pciedev);
		return 0;
	}
	if (pciedev->lpbk_dma_src_delay)
		dngl_add_timer(pciedev->lpbk_src_dmaxfer_timer,
		pciedev->lpbk_dma_src_delay, FALSE);
	else
		pciedev_process_do_src_lpbk_dmaxfer(pciedev);
	return 0;
}

static void
pciedev_done_lpbk_dmaxfer(struct dngl_bus *pciedev)
{
	void *p;

	while (!pktq_empty(&pciedev->lpbk_dma_txq)) {
		p = pktq_pdeq(&pciedev->lpbk_dma_txq, 0);
		PKTFREE(pciedev->osh, p, TRUE);
	}
	PCI_TRACE(("send a message to indicate it is done now\n"));
	pciedev_send_lpbkdmaxfer_complete(pciedev, 0, pciedev->lpbk_dma_req_id);
}

static  uint32
pciedev_prepare_lpbk_dmaxfer(struct dngl_bus *pciedev, uint32 len)
{
	uint32 pktlen = pciedev->lpbk_dma_pkt_fetch_len;
	void *p;
	uint32 totpktlen = 0;

	if (!pktq_empty(&pciedev->lpbk_dma_txq)) {
		PCI_ERROR(("request queue non empty\n"));
		return 0;

	}
	/* lets handle a batch of max 16K */
	if (len > MAX_DMA_BATCH_SZ)
		len = MAX_DMA_BATCH_SZ;

	while (totpktlen < len) {
		p = PKTGET(pciedev->osh, pktlen, TRUE);
		if (p == NULL)
			break;
		PKTSETLEN(pciedev->osh, p, pktlen);
		pktq_penq(&pciedev->lpbk_dma_txq, 0, p);
		totpktlen += MAX_DMA_XFER_SZ;
	}
	if (totpktlen == 0)  {
		PCI_ERROR(("couldn't allocate memory for the loopback request\n"));
		return 0;
	}
	PCI_TRACE(("allocated %d packets to handle the request, pktlen %d\n",
		pktq_len(&pciedev->lpbk_dma_txq), pktlen));
	/* now start the whole process */
	pciedev_schedule_src_lpbk_dmaxfer(pciedev);
	return 1;
}

static void
pciedev_process_lpbk_dmaxfer_msg(struct dngl_bus *pciedev, void *p)
{
	pcie_dma_xfer_params_t *dmap = (pcie_dma_xfer_params_t *)p;

	if (pciedev->pend_dmaxfer_len != 0) {
		PCI_ERROR(("already a lpbk request pending, so fail the new one\n"));
		pciedev_send_lpbkdmaxfer_complete(pciedev, -1, dmap->cmn_hdr.request_id);
		return;
	}

	pciedev->pend_dmaxfer_len = ltoh32(dmap->xfer_len);
	pciedev->pend_dmaxfer_srcaddr_hi = ltoh32(dmap->host_input_buf_addr.high);
	pciedev->pend_dmaxfer_srcaddr_lo = ltoh32(dmap->host_input_buf_addr.low);
	pciedev->pend_dmaxfer_destaddr_hi = ltoh32(dmap->host_ouput_buf_addr.high);
	pciedev->pend_dmaxfer_destaddr_lo = ltoh32(dmap->host_ouput_buf_addr.low);
	pciedev->lpbk_dma_src_delay = ltoh32(dmap->srcdelay);
	pciedev->lpbk_dma_dest_delay = ltoh32(dmap->destdelay);
	pciedev->lpbk_dma_req_id = dmap->cmn_hdr.request_id;

	pciedev->lpbk_dma_pkt_fetch_len = MAX_DMA_XFER_SZ +  sizeof(struct fetch_rqst)+ 8 + 8;

	pciedev->lpbk_dmaxfer_fetch_pend = 0;
	pciedev->lpbk_dmaxfer_push_pend = 0;
	PCI_TRACE(("dma loopback request for length %d bytes\n", pciedev->pend_dmaxfer_len));

	if (pciedev_prepare_lpbk_dmaxfer(pciedev, pciedev->pend_dmaxfer_len) == 0) {
		PCI_ERROR(("error to set up the dma xfer loopback\n"));
		return;
	}
}
static void
pciedev_send_lpbkdmaxfer_complete(struct dngl_bus *pciedev, uint16 status, uint32 req_id)
{
	pcie_dmaxfer_cmplt_t *pkt;
	pkt = (pcie_dmaxfer_cmplt_t *)pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl);
	bzero(pkt, sizeof(pcie_dmaxfer_cmplt_t));

	pkt->compl_hdr.status = status;
	pkt->cmn_hdr.request_id = req_id;
	pkt->cmn_hdr.msg_type = MSG_TYPE_LPBK_DMAXFER_CMPLT;
	/* Should check if teh resources are avaialble */
	if (!pciedev_xmit_msgbuf_packet(pciedev, pkt,
		RING_LEN_ITEMS(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl)) {
		PCI_TRACE(("pciedev_send_lpbkdmaxfer_complete:"
			"failed to transmit ring status to host "));
		ASSERT(0);
	}
}
#endif /* PCIE_DMAXFER_LOOPBACK */

static void
pciedev_send_general_status(struct dngl_bus *pciedev, uint16 ring_id, uint32 status, uint32 req_id)
{
	pcie_gen_status_t *pkt;
	pkt = (pcie_gen_status_t *)pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl);
	bzero(pkt, sizeof(pcie_gen_status_t));
	pkt->compl_hdr.status = status;
	pkt->compl_hdr.flow_ring_id = ring_id;
	pkt->cmn_hdr.msg_type = MSG_TYPE_GEN_STATUS;
	pkt->cmn_hdr.request_id = req_id;
	/* Should check if teh resources are avaialble */
	if (!pciedev_xmit_msgbuf_packet(pciedev, pkt,
		RING_LEN_ITEMS(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl)) {
		PCI_TRACE(("pciedev_send_general_status: failed to transmit"
			"general status to host"));
		ASSERT(0);
	}
}

static void
pciedev_send_ring_status(struct dngl_bus *pciedev, uint16 w_idx, uint32 status, uint16 ring_id)
{
	pcie_ring_status_t *pkt;
	pkt = (pcie_ring_status_t *)pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl);
	bzero(pkt, sizeof(pcie_ring_status_t));
	pkt->write_idx = w_idx;
	pkt->compl_hdr.status = status;
	pkt->compl_hdr.flow_ring_id = ring_id;
	pkt->cmn_hdr.msg_type = MSG_TYPE_RING_STATUS;
	/* Should check if teh resources are avaialble */
	if (!pciedev_xmit_msgbuf_packet(pciedev, pkt,
		RING_LEN_ITEMS(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl)) {
		PCI_TRACE(("pciedev_send_ring_status: failed to transmit ring status to host "));
		ASSERT(0);
	}
}

static void
pciedev_send_ioctlack(struct dngl_bus *pciedev, uint16 status)
{
	ioctl_req_ack_msg_t *pkt;
	ioctl_req_msg_t *ioct_rqst;

	pkt = (ioctl_req_ack_msg_t *)pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl);
	if (pkt == NULL) {
		PCI_ERROR(("pciedev_send_ioctlack: couldn't allocate space for IOCTL PTR REQ ACK"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		ASSERT(0);
		return;
	}
	ioct_rqst = (ioctl_req_msg_t*)PKTDATA(pciedev->osh, pciedev->ioctl_pktptr);
	pkt->cmn_hdr.flags = 0;
	pkt->cmn_hdr.if_id = 0;
	pkt->cmn_hdr.msg_type = MSG_TYPE_IOCTLPTR_REQ_ACK;
	pkt->cmn_hdr.request_id = ioct_rqst->cmn_hdr.request_id;
	pkt->compl_hdr.flow_ring_id = BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT;
	pkt->compl_hdr.status = status;
	pkt->cmd = ioct_rqst->cmd;

	if (!pciedev_xmit_msgbuf_packet(pciedev, pkt,
		RING_LEN_ITEMS(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl)) {
		PCI_ERROR(("pciedev_send_ioctlack: failed to transmit ack packet to  "
			"host for IOCTL PTR REQ ACK"));
		ASSERT(0);
	}
	return;
}

/* Schedule dma to pull dwon non inline ioctl request */
static void
pciedev_process_ioctlptr_rqst(struct dngl_bus *pciedev, void* p)
{
	ioctl_req_msg_t *ioct_rqst;
	uint16 buflen;
	void * lclpkt;
	uint16 lclbuflen;
	uint8* payloadptr;
	uint32 out_buf_len;
	uint32 max_ioctl_resp_len;

	/* ioct request info */
	ioct_rqst = (ioctl_req_msg_t*)p;

	/* host buf info */
	buflen = ltoh16(ioct_rqst->input_buf_len);
	out_buf_len = ltoh16(ioct_rqst->output_buf_len);


	if (HOST_DMA_BUF_POOL_EMPTY(pciedev->ioctl_resp_pool)) {
		PCI_ERROR(("no host response IOCTL buffer available..so fail the request\n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NO_IOCTLRESP_BUF, ioct_rqst->cmn_hdr.request_id);
		return;
	}
	if (buflen > (pktpool_plen(pciedev->ioctl_req_pool) - H2DRING_CTRL_SUB_ITEMSIZE)) {
		PCI_ERROR(("no host response IOCTL buffer available..so fail the request\n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_BADOPTION, ioct_rqst->cmn_hdr.request_id);
		return;
	}

	lclpkt = pktpool_get(pciedev->ioctl_req_pool);
	if (lclpkt == NULL) {
		PCI_ERROR(("panic: Should never happen \n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NOMEM, ioct_rqst->cmn_hdr.request_id);
		return;
	}
	lclbuflen = pktpool_plen(pciedev->ioctl_req_pool);
	lclbuflen -= H2DRING_CTRL_SUB_ITEMSIZE;

	if (!pciedev->ioct_resp_buf) {
		/*
		 * If external buffer is not available for ioctl response, we will be using
		 * the lclpkt for response as well. Incase requested response is more than
		 * what can be accomodated in lclpkt, trim down the request
		 */
		if (out_buf_len > lclbuflen) {
			PCI_INFORM(("host is asking for %d resp bytes,"
			    "but buffer is only %d bytes\n",
			    out_buf_len, pciedev->ioctl_resp_pool->ready->len));
			out_buf_len = lclbuflen;
			ioct_rqst->output_buf_len = htol16(out_buf_len);
		}
	}
	/* Checking the first on the list, if not big enough truncate the result */
	max_ioctl_resp_len = pciedev->ioctl_resp_pool->ready->len;
	if (pciedev->ioct_resp_buf)
		max_ioctl_resp_len =
			MIN(pciedev->ioctl_resp_pool->ready->len, pciedev->ioct_resp_buf_len);

	if (out_buf_len > max_ioctl_resp_len) {
		PCI_INFORM(("host is asking for %d resp bytes, but buffer is only %d bytes\n",
			out_buf_len, max_ioctl_resp_len));
		out_buf_len = max_ioctl_resp_len;
		ioct_rqst->output_buf_len = htol16(out_buf_len);
	}

	if (pciedev->ioctl_pktptr != NULL) {
		PCI_ERROR(("pciedev->ioctl_pktptr is already non NULL %x\n",
			*(int *)pciedev->ioctl_pktptr));
		PCI_ERROR(("pciedev->ioctl_pktptr != NULL \n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_BADOPTION, ioct_rqst->cmn_hdr.request_id);
		ASSERT(0);
		return;
	}
	pciedev->ioctl_pktptr = lclpkt;

	PKTSETLEN(pciedev->osh, lclpkt, lclbuflen + H2DRING_CTRL_SUB_ITEMSIZE);

	/* copy message header into the packet */
	bcopy(p, PKTDATA(pciedev->osh, lclpkt), H2DRING_CTRL_SUB_ITEMSIZE);

	pciedev->ioctl_lock = TRUE;

	if (buflen == 0) {
		PCI_TRACE(("pciedev_process_ioctlptr_rqst: No payload to DMA for ioctl!\n"));
		pciedev->ioctl_pend = TRUE;
		pciedev_process_ioctl_pend(pciedev);
		return;
	}
	/* Offset ioctl payload pointer by msglen */
	payloadptr = (uint8*)PKTDATA(pciedev->osh, lclpkt) + H2DRING_CTRL_SUB_ITEMSIZE;

	PHYSADDR64HISET(pciedev->ioctptr_fetch_rqst->haddr,
		(uint32)ltoh32(ioct_rqst->host_input_buf_addr.high));
	PHYSADDR64LOSET(pciedev->ioctptr_fetch_rqst->haddr,
		(uint32) ltoh32(ioct_rqst->host_input_buf_addr.low));

	pciedev->ioctptr_fetch_rqst->size = buflen;
	pciedev->ioctptr_fetch_rqst->dest = payloadptr;
	pciedev->ioctptr_fetch_rqst->cb = pciedev_ioctpyld_fetch_cb;
	pciedev->ioctptr_fetch_rqst->ctx = (void *)pciedev;
#ifdef BCMPCIEDEV_ENABLED
	hndrte_fetch_rqst(pciedev->ioctptr_fetch_rqst);
#endif
}

/* Create wl event message and schedule dma */
int
pciedev_process_d2h_wlevent(struct dngl_bus *pciedev, void* p)
{
	uint16 pktlen = 0;
	uint32 bufid = 0;
	uint8 ifidx = 0;
	ret_buf_t haddr;
	host_dma_buf_t hostbuf;
	msgbuf_ring_t *msgring = pciedev->dtoh_ctrlcpl;


	/* extract out ifidx first */
	ifidx = PKTIFINDEX(pciedev->osh, p);

	/* remove cmn header */
	PKTPULL(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
	pktlen = PKTLEN(pciedev->osh, p);

	if (pciedev_host_dma_buf_pool_get(pciedev->event_pool, &hostbuf)) {
		haddr.low_addr = hostbuf.buf_addr.low_addr;
		haddr.high_addr = hostbuf.buf_addr.high_addr;
		bufid = hostbuf.pktid;
	}
	else {
		PCI_ERROR(("packet not avialable in the event pool...BAD......\n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NO_EVENT_BUF, -1);
		/* This need to generate an error message to host saying no event buffers */
		PKTFREE(pciedev->osh, p, TRUE);
		return BCME_ERROR;
	}

	/* Schedule wlevent payload transfer */
	pciedev_tx_pyld(pciedev, p, &haddr, D2H_MSGLEN(pciedev, pktlen),
		NULL, 0, MSG_TYPE_EVENT_PYLD);

	/* Form wl event complte messge and transfer */
	return pciedev_xmit_wlevnt_cmplt(pciedev, bufid, pktlen, ifidx, msgring);
}

/* Sendout wl event completion message to host */
static int
pciedev_xmit_wlevnt_cmplt(struct dngl_bus *pciedev, uint32 bufid, uint16 buflen,
	uint8 ifidx, msgbuf_ring_t *msgring)
{
	wlevent_req_msg_t * evnt;

	/* reserve the sapce in curcular buffer */
	evnt  = pciedev_get_lclbuf_pool(msgring);
	if (evnt == NULL) {
		PCI_ERROR(("pciedev_xmit_wlevnt_cmplt:%d:"
			"No space in DTOH ctrl msgbuf to send event\n",
			__LINE__));
		ASSERT(0);
		return BCME_ERROR;
	}

	/* Cmn header */
	evnt->cmn_hdr.msg_type = MSG_TYPE_WL_EVENT;
	evnt->cmn_hdr.request_id = htol32(bufid);
	BCMMSGBUF_SET_API_IFIDX(&evnt->cmn_hdr, ifidx);

	evnt->compl_hdr.status = 0;
	evnt->compl_hdr.flow_ring_id = BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE;

	evnt->event_data_len = htol16(buflen);

	/* schedule wlevent complete message transfer */
	if (!pciedev_xmit_msgbuf_packet(pciedev, evnt, RING_LEN_ITEMS(msgring),
		msgring)) {
		EVENT_LOG(EVENT_LOG_TAG_PCI_ERROR, "pciedev_xmit_wlevnt_cmplt: "
			"failed to transmit  wlevent complete message");
		return BCME_ERROR;
	}
	else
		return BCME_OK;
}
#ifndef PCIE_PHANTOM_DEV
/* routine to send the LTR messages */
static void
pciedev_send_ltr(void *dev, uint8 state)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)dev;

	if (pciedev->cur_ltr_state != state) {
		if (state == LTR_SLEEP)
			pciedev_crwlpciegen2_161(pciedev, FALSE);
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state, state);
		if (state == LTR_ACTIVE) {
			SPINWAIT(((R_REG(pciedev->osh,
				&pciedev->regs->u.pcie2.ltr_state) &
				LTR_FINAL_MASK) >> LTR_FINAL_SHIFT) != LTR_ACTIVE, 100);
			if (((R_REG(pciedev->osh,
				&pciedev->regs->u.pcie2.ltr_state) &
				LTR_FINAL_MASK) >> LTR_FINAL_SHIFT) != LTR_ACTIVE) {
				PCI_ERROR(("pciedev_send_ltr:Giving up:0x%x\n",
					R_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state)));
			}
			else {
				pciedev_crwlpciegen2_161(pciedev, TRUE);
			}
		}
		pciedev->cur_ltr_state  = state;
	}
}

static void
pciedev_handle_host_D3_info(struct dngl_bus *pciedev)
{
	/* make the IPC not process any more */
	/* call back the rte to notify the other cores to not touch the host mem anymore */
	hndrte_notify_devpwrstchg(FALSE);
	/* Use Tpoweron +5 value during D3 entry */
	pciedev_crwlpciegen2_161(pciedev, FALSE);
	pciedev->in_d3_suspend = TRUE;
	pciedev->d3_ack_pending = TRUE;
	if (pciedev->ds_check_timer_on) {
		dngl_del_timer(pciedev->ds_check_timer);
		pciedev->ds_check_timer_on = FALSE;
	}
	pciedev->d3_wait_for_txstatus = pciedev->pend_user_tx_pkts;

	/* enable the interrupt to get the notfication of entering D3 */
	pciedev->device_link_state |= PCIEGEN2_PWRINT_D3_STATE_MASK;
	pciedev->device_link_state &= ~PCIEGEN2_PWRINT_D0_STATE_MASK;
	pciedev_enable_powerstate_interrupts(pciedev);

	/* Make sure all the pending are handled already */
	pciedev_D3_ack(pciedev);
}

static void
pciedev_d2h_mbdata_clear(struct dngl_bus *pciedev, uint32 mb_data)
{
	if (pciedev->d2h_mb_data_ptr == 0)
		return;
	mb_data = *pciedev->d2h_mb_data_ptr;
	mb_data  &= ~mb_data;
	*pciedev->d2h_mb_data_ptr = mb_data;
}

static void
pciedev_disable_deepsleep(struct dngl_bus *pciedev, bool disable)
{
	pciedev->deepsleep_disable_state = disable;
	hndrte_disable_deepsleep(disable);
}

static void
pciedev_handle_host_deepsleep_ack(struct dngl_bus *pciedev)
{
	PCI_TRACE(("host acked the deep sleep request, so enable deep sleep now\n"));
	pciedev_d2h_mbdata_clear(pciedev, D2H_DEV_DS_ENTER_REQ);
	pciedev_deepsleep_engine(pciedev, DS_ALLOWED_EVENT);
}
static void
pciedev_handle_host_deepsleep_nak(struct dngl_bus *pciedev)
{
	PCI_TRACE("host acked the deep sleep request, so enable deep sleep now\n");
	pciedev_d2h_mbdata_clear(pciedev, D2H_DEV_DS_ENTER_REQ);
	pciedev_deepsleep_engine(pciedev, DS_NOT_ALLOWED_EVENT);
}

void
pciedev_D3_ack(struct dngl_bus *pciedev)
{
	if (!pciedev->in_d3_suspend) {
		return;
	}

	if (!pciedev->d3_ack_pending) {
		return;
	}

	if (dma_txactive(pciedev->h2d_di) || dma_rxactive(pciedev->h2d_di) ||
		dma_txactive(pciedev->d2h_di) || dma_rxactive(pciedev->d2h_di))
	{
		PCI_ERROR(("MEM2MEM DMA H2D_TX %d, H2D_RX %d, D2H_TX: %d, D2H_RX: %d\n",
			dma_txactive(pciedev->h2d_di), dma_rxactive(pciedev->h2d_di),
			dma_txactive(pciedev->d2h_di), dma_rxactive(pciedev->d2h_di)));
		return;
	}
	if (pciedev->d3_wait_for_txstatus) {
		PCI_ERROR(("pendig User Tx packets: pciedev->d3_wait_for_txstatus is %d\n",
			pciedev->d3_wait_for_txstatus));
		return;
	}
	PCI_TRACE(("acking the D3 req\n"));
	pciedev_d2h_mbdata_send(pciedev, D2H_DEV_D3_ACK);
	pciedev->d3_ack_pending = FALSE;
}

static void
pciedev_deepsleep_enter_req(struct dngl_bus *pciedev)
{
	PCI_TRACE(("sending deep sleep request to host\n"));
	pciedev_d2h_mbdata_send(pciedev, D2H_DEV_DS_ENTER_REQ);
}

static void
pciedev_deepsleep_exit_notify(struct dngl_bus *pciedev)
{
	pciedev_disable_deepsleep(pciedev, TRUE);
	/* Should we wait for HT to be avail */
	PCI_TRACE(("sending deep sleep exit notificaiton to host\n"));
	pciedev_d2h_mbdata_send(pciedev, D2H_DEV_DS_EXIT_NOTE);
}

static void
pciedev_d2h_mbdata_send(struct dngl_bus *pciedev, uint32 data)
{
	uint32 d2h_mb_data;

	if (pciedev->d2h_mb_data_ptr == 0)
		return;

	d2h_mb_data = *pciedev->d2h_mb_data_ptr;
	if (d2h_mb_data != 0) {
		uint32 i = 0;
		PCI_ERROR(("GRRRRRRR: MB transaction is already pending 0x%04x\n", d2h_mb_data));
		/* start a zero length timer to keep checking this to be zero */
		while ((i++ < 100) && d2h_mb_data) {
			OSL_DELAY(10);
			d2h_mb_data = *pciedev->d2h_mb_data_ptr;
		}
		if (i >= 100)
			PCI_ERROR(("waited 1ms for host to ack the previous mb transaction\n"));
	}
	d2h_mb_data |= data;
	*pciedev->d2h_mb_data_ptr = d2h_mb_data;
	pciedev_generate_host_mb_intr(pciedev);
}

static void
pciedev_h2d_mb_data_process(struct dngl_bus *pciedev)
{
	uint32 h2d_mb_data;

	if (pciedev->h2d_mb_data_ptr == 0)
		return;

	h2d_mb_data = *pciedev->h2d_mb_data_ptr;

	if (!h2d_mb_data)
		return;

	/* ack the message first */
	*pciedev->h2d_mb_data_ptr = 0;

	PCI_TRACE(("pciedev_h2d_mb_data_process: h2d_mb_data is 0x%04x\n", h2d_mb_data));

	/* if it is an ack for a request clear the request bits in the d2h_mb_data */
	if (h2d_mb_data & H2D_HOST_D3_INFORM) {
#ifdef DELAYED_PCIE_REPROGRAMMING
		/*
		* If the host did not do doorbell coming out of D3cold, instead doing
		* another D3cold, this code should handle it.
		*/
		if (pciedev->real_d3) {
			pciedev_handle_d0_enter(pciedev);
			pciedev_enable_powerstate_interrupts(pciedev);
		}
#endif /* DELAYED_PCIE_REPROGRAMMING */
		pciedev_handle_host_D3_info(pciedev);
	}
	if (h2d_mb_data & H2D_HOST_DS_ACK) {
		pciedev_handle_host_deepsleep_ack(pciedev);
	}
	if (h2d_mb_data & H2D_HOST_DS_NAK) {
		pciedev_handle_host_deepsleep_nak(pciedev);
	}
	/* Host to dongle console cmd interrupt */
	if (h2d_mb_data & H2D_HOST_CONS_INT) {
		hndrte_cons_check();
	}
}

static void
pciedev_device_wake_isr(uint32 status,  void *arg)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)arg;
	PCI_TRACE(("pciedev_device_wake_isr got called, %x, status 0x%04x\n",
		(uint32)pciedev, status));

	if (status & GCI_GPIO_STS_VALUE) {
		PCI_TRACE(("device wake is 1, disable deep sleep, and notify host\n"));
		pciedev_deepsleep_engine(pciedev, DW_ASSRT_EVENT);
	}
	else {
		PCI_TRACE(("device wake is 0, enable deep sleep cap, and negotiate with host\n"));
		/* Might make sense to run a timer and make sure there is activity pending */
		pciedev_deepsleep_engine(pciedev, DW_DASSRT_EVENT);
	}
}

static void
BCMATTACHFN(pciedev_enable_device_wake)(struct dngl_bus *pciedev)
{
	uint8 gci_gpio;
	uint8 wake_status;
	uint8 cur_status;

	gci_gpio = si_enable_device_wake(pciedev->sih, &wake_status, &cur_status);
	if (gci_gpio == CC_GCI_GPIO_INVALID) {
		PCI_ERROR(("pcie dev: device_wake not enabled  size of rxcomplete is %d\n",
			sizeof(rxcmplt_hdr_t)));
		pciedev->ds_state = DS_DISABLED_STATE;
		return;
	}
	PCI_TRACE(("device_wake: gpio %d, wake_status 0x%02x, cur_status 0x%02x\n",
		gci_gpio, wake_status, cur_status));
	/* while coming up, doesn't make sense to negotiate with host */
	if (cur_status & GCI_GPIO_STS_VALUE) {
		PCI_TRACE(("device_ wake init state 1, disable deep sleep\n"));
		pciedev_disable_deepsleep(pciedev, TRUE);
		pciedev->ds_state = NO_DS_STATE;
	}
	else {
		PCI_TRACE(("device_ wake init state 0, enable deep sleep cap\n"));
		/* Keep deepsleep disabled till deepsleep is allowed by host */
		pciedev_disable_deepsleep(pciedev, TRUE);
		pciedev_deepsleep_engine(pciedev, DW_DASSRT_EVENT);
	}

	/* check nvram variable for device_wake pad GPIO */
	hndrte_enable_gci_gpioint(gci_gpio, wake_status, pciedev_device_wake_isr, pciedev);
}

static uint8
BCMATTACHFN(pciedev_host_wake_gpio_init)(struct dngl_bus *pciedev)
{
	uint8 host_wake_gpio;

	host_wake_gpio = si_gci_host_wake_gpio_init(pciedev->sih);
	if (host_wake_gpio == CC_GCI_GPIO_INVALID) {
		PCI_ERROR(("pcie dev: host_wake not enabled \n"));
	}
	return host_wake_gpio;
}

void
pciedev_host_wake_gpio_enable(struct dngl_bus *pciedev, bool state)
{
	if (pciedev->host_wake_gpio == CC_GCI_GPIO_INVALID)
		return;
	si_gci_host_wake_gpio_enable(pciedev->sih, pciedev->host_wake_gpio, state);
	return;
}

#ifdef PCIE_DELAYED_HOSTWAKE
static void
pciedev_delayed_hostwake(struct dngl_bus *pciedev, uint32 val)
{
	if (pciedev->delayed_hostwake_timer_active)
		return;
	dngl_add_timer(pciedev->delayed_hostwake_timer, val * 1000, FALSE);
	pciedev->delayed_hostwake_timer_active = TRUE;
}

static void
pciedev_delayed_hostwake_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) t->context;

	pciedev_host_wake_gpio_enable(pciedev, TRUE);
	pciedev->delayed_hostwake_timer_active = FALSE;
}
#endif /* PCIE_DELAYED_HOSTWAKE */

/* Do a complete reset of PCIe core and DMA.
* Also re-write the enum space registers
*/
static void
pciedev_perst_reset_process(struct dngl_bus *pciedev)
{
	pciedev_reset_pcie_interrupt(pciedev);
	pciedev_reset(pciedev);
	if (!si_iscoreup(pciedev->sih)) {
		PCI_ERROR(("pciedev_perst_reset_process: failed to bring up PCIE core \n"));
		return;
	}
	OR_REG(pciedev->osh, &pciedev->regs->control, PCIE_SPERST);
	pciedev_init(pciedev);
	dma_rxreset(pciedev->h2d_di);
	dma_txreset(pciedev->h2d_di);

	dma_rxinit(pciedev->h2d_di);
	dma_txinit(pciedev->h2d_di);
	dma_rxreset(pciedev->d2h_di);
	dma_txreset(pciedev->d2h_di);

	dma_rxinit(pciedev->d2h_di);
	dma_txinit(pciedev->d2h_di);

	pciedev->device_link_state = PCIEGEN2_PWRINT_L0_LINK_MASK |
		PCIEGEN2_PWRINT_D0_STATE_MASK;
	pciedev_enable_powerstate_interrupts(pciedev);
	pciedev->pciecoreset = TRUE;
	OR_REG(pciedev->osh, &pciedev->regs->control, PCIE_SPERST);

	++pciedev->metrics.perst_assrt_ct;
	if (pciedev->metric_ref.active) {
		pciedev->metrics.active_dur += hndrte_time() - pciedev->metric_ref.active;
		pciedev->metric_ref.active = 0;
	}
	pciedev->metric_ref.perst = hndrte_time();
}

static void
pciedev_crwlpciegen2(struct dngl_bus *pciedev)
{
#ifdef WAR2_HWJIRA_CRWLPCIEGEN2_162
	OR_REG(pciedev->osh, &pciedev->regs->control, PCIE_DISABLE_L1CLK_GATING);
#ifdef WAR_HWJIRA_CRWLPCIEGEN2_160
	{
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCIEGEN2_COE_PVT_TL_CTRL_0);
		AND_REG(pciedev->osh, &pciedev->regs->configdata,
			~(1 << COE_PVT_TL_CTRL_0_PM_DIS_L1_REENTRY_BIT));
		PCI_TRACE(("coe pvt reg 0x%04x, value 0x%04x\n", PCIEGEN2_COE_PVT_TL_CTRL_0,
			R_REG(pciedev->osh, &pciedev->regs->configdata)));
	}
#endif /* WAR_HWJIRA_CRWLPCIEGEN2_160 */
#endif /* WAR2_HWJIRA_CRWLPCIEGEN2_162 */
}

static void
pciedev_reg_pm_clk_period(struct dngl_bus *pciedev)
{
	uint32 alp_KHz, pm_value;

	/* set REG_PM_CLK_PERIOD to the right value */
	/* default value is not conservative enough..usually set to period of Xtal freq */
	/* HW folks want us to set this to half of that */
	/* (1000000 / xtalfreq_in_kHz) * 2 */
	if (PCIECOREREV(pciedev->corerev) <= 13) {
		alp_KHz = (si_pmu_alp_clock(pciedev->sih, pciedev->osh) / 1000);
		pm_value =  (1000000 * 2) / alp_KHz;
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCIEGEN2_PVT_REG_PM_CLK_PERIOD);
		PCI_TRACE(("ALP in KHz is %d, cur PM_REG_VAL is %d, new PM_REG_VAL is %d\n",
			alp_KHz, R_REG(pciedev->osh, &pciedev->regs->configdata), pm_value));
		W_REG(pciedev->osh, &pciedev->regs->configdata, pm_value);
	}
}

/*
SW WAR implementation of CRWLPCIEGEN2-161
Read Tpoweron value programmed in PML1_sub_control2 (0x24c)
Before we enable L1.2 i.e. before firmware sends LTR sleep:
	Set PMCR_TREFUP = Tpoweron + 5us
After we disable L1.2 i.e. after firmware sends LTR active:
	Set PMCR_TREFUP = 6us (default value)

The PMCR_TREFUP  counter is a 10-bit count and REG_PMCR_TREFUP_MAX_HI
is the high 4 bits and REG_PMCR_TREFUP_MAX_LO is the low 6-bits
of the count. REG_PMCR_TREFUP_EXTEND_MAX extends the count by 8.

To obtain the count value, use the following formula:
Temp = ROUNDUP((time in usec) * 25) ### This is the count value
in 25Mhz clock period
If (Temp greater than or equal to 1024) then ### i.e. the count
									is more than 10 bits
	Set PMCR_TREFUP = ROUNDUP ((time in usec) * 25 / 8) and
	Set REG_PMCR_TREFUP_EXTEND_MAX=1
Else  ## i.e. count is less than or equal to 10 bits
	Set PMCR_TREFUP = ROUNDUP ((time in usec) * 25)
	Set REG_PMCR_TREFUP_EXTEND_MAX=0

For the second part of the WAR i.e. After we disable L1.2 i.e. after we send LTR active:
Set PMCR_TREFUP = 6us (default value)
Also set REG_PMCR_TREFUP_EXTEND_MAX=0
*/
static void
pciedev_crwlpciegen2_161(struct dngl_bus *pciedev, bool state)
{
	uint32 tpoweron = 0, trefup = 0, trefup_ext = 0,
		tpoweron_usec = 0, tpoweron_scale, tpoweron_val;
	uint32 trefup_hi = 0, trefup_lo = 0;
	uint32 clk_freq_mhz = 25; /* 25 MHz */
	uint32 val = 0;
	uint32 mask_refup = 0x3f0001e0;
	uint32 mask_refext = 0x400000;
	int scaletab[] = {2, 10, 100, -1};

	if (pciedev->in_d3_suspend) {
		PCI_ERROR(("pciedev_crwlpciegen2_161: in D3, Cannot apply LTR\n"));
		return;
	}

	/* PCI_TRACE(("%s:%s\n", __FUNCTION__, state ? "LTR_ACTIVE":"LTR_SLEEP")); */
	if (state) {
		trefup = PMCR_TREFUP_DEFAULT * clk_freq_mhz;
	}
	else {
		W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_L1SS_CTRL2);
		tpoweron = R_REG(pciedev->osh, &pciedev->regs->configdata);
		tpoweron_scale = tpoweron & PCI_TPOWER_SCALE_MASK;
		tpoweron_val = tpoweron >> PCI_TPOWER_SCALE_SHIFT;
		if ((tpoweron_scale > 3) || scaletab[tpoweron_scale] == -1) {
			PCI_ERROR(("pciedev_crwlpciegen2_161:tpoweron_scale:0x%x illegal\n",
				tpoweron_scale));
			return;
		}
		tpoweron_usec = (scaletab[tpoweron_scale] * tpoweron_val) +
			PMCR_TREFUP_TPOWERON_OFFSET;

		trefup = tpoweron_usec * clk_freq_mhz;
		if (trefup >= 0x2000) {
			PCI_ERROR(("pciedev_crwlpciegen2_161:Tpoweron:0x%x too high\n", tpoweron));
			return;
		}
	}
	if (trefup >= 0x400) {
		trefup >>= 3;
		trefup_ext = 1;
	}
	trefup_lo = trefup & 0x3f;
	trefup_hi = trefup >> 6;
	val = trefup_lo << 24 | trefup_hi << 5;
	PCI_TRACE(("tpoweron:0x%x:Tref:0x%x, hi:0x%x, lo:0x%x, ext:%d\n",
		tpoweron, trefup, trefup_hi, trefup_lo, trefup_ext));

	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_PMCR_REFUP);
	AND_REG(pciedev->osh, &pciedev->regs->configdata, ~mask_refup);
	OR_REG(pciedev->osh, &pciedev->regs->configdata, val);
	PCI_TRACE(("Reg:0x%x ::0x%x\n",
		PCI_PMCR_REFUP, R_REG(pciedev->osh, &pciedev->regs->configdata)));

	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_PMCR_REFUP_EXT);
	AND_REG(pciedev->osh, &pciedev->regs->configdata, ~mask_refext);
	OR_REG(pciedev->osh, &pciedev->regs->configdata, trefup_ext << 22);
	PCI_TRACE((":Reg:0x%x ::0x%x\n",
		PCI_PMCR_REFUP_EXT, R_REG(pciedev->osh, &pciedev->regs->configdata)));
}

/*
* SW WAR for CRWLPCIEGEN2-61
*/
static void
pciedev_crwlpciegen2_61(struct dngl_bus *pciedev)
{
	si_wrapperreg(pciedev->sih, AI_OOBSELINA74, ~0, 0);
	si_wrapperreg(pciedev->sih, AI_OOBSELINA30, ~0, 0);
}

/*
* SW WAR for CRWLPCIEGEN2-180
*/
static void
pciedev_crwlpciegen2_180(struct dngl_bus *pciedev)
{
	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCI_PMCR_REFUP);
	OR_REG(pciedev->osh, &pciedev->regs->configdata, 0x1f);
	PCI_TRACE(("Reg:0x%x ::0x%x\n",
		PCI_PMCR_REFUP, R_REG(pciedev->osh, &pciedev->regs->configdata)));
}


#endif /* PCIE_PHANTOM_DEV */

/* at this point all the resources needed should be available, this call can't fail.. */
static void
pciedev_send_ioctl_completion(struct dngl_bus *pciedev, void *p)
{
	host_dma_buf_t hostbuf;
	ioctl_comp_resp_msg_t* ioct_resp;
	uint16  msglen;
	ret_buf_t haddr;
	msgbuf_ring_t *msgbuf = pciedev->dtoh_ctrlcpl;

	/* get a new buffer locally to hold message */
	ioct_resp = (ioctl_comp_resp_msg_t *)pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl);
	if (ioct_resp == NULL) {
		ASSERT(0);
		return;
	}
	if (HOST_DMA_BUF_POOL_EMPTY(pciedev->ioctl_resp_pool)) {
		PCI_TRACE(("ioctl resp pool empty\n"));
		ASSERT(HOST_DMA_BUF_POOL_EMPTY(pciedev->ioctl_resp_pool));
		return;
	}

	/* Separate out cmplt message and ioct reponse */
	bcopy(PKTDATA(pciedev->osh, p), (void *)ioct_resp, sizeof(ioctl_comp_resp_msg_t));
	PKTPULL(pciedev->osh, p, sizeof(ioctl_comp_resp_msg_t));

	ioct_resp->cmn_hdr.msg_type = MSG_TYPE_IOCTL_CMPLT;
	/* ioctl complete msgbuf is ready to be sent. But, we cannot send */
	/* it out till the ioctl payload has been scheduled for DMA. */
	msglen = ioct_resp->resp_len;
	pciedev_host_dma_buf_pool_get(pciedev->ioctl_resp_pool, &hostbuf);
	ioct_resp->cmn_hdr.request_id = hostbuf.pktid;
	if (msglen != 0) {
		haddr.low_addr = hostbuf.buf_addr.low_addr;
		haddr.high_addr = hostbuf.buf_addr.high_addr;
		msglen = MIN(msglen, hostbuf.len);
		if (ioct_resp->resp_len > msglen) {
			PCI_ERROR(("resp len %d, host buffer size %d\n",
				ioct_resp->resp_len, msglen));
			ioct_resp->resp_len = msglen;
		}
		pciedev_tx_ioctl_pyld(pciedev, p, &haddr, D2H_MSGLEN(pciedev, msglen),
			MSG_TYPE_IOCT_PYLD);
	}
	pciedev_xmit_msgbuf_packet(pciedev, ioct_resp, RING_LEN_ITEMS(msgbuf), msgbuf);
	pciedev->ioctl_pend = FALSE;
}

void
pciedev_process_ioctl_pend(struct dngl_bus *pciedev)
{
	uint16 avail_ring_entry;
	uint32 txdesc, rxdesc;
	msgbuf_ring_t * ring = pciedev->dtoh_ctrlcpl;

	/* resources needed are allocated host buffer for response,
	 * local buffer to send ack, local buffer to send ioctl response msg,
	 * work slots on host ctrl completion ring
	*/
	if (HOST_DMA_BUF_POOL_EMPTY(pciedev->ioctl_resp_pool)) {
		PCI_ERROR(("no host buffers for ioctl resp processing\n"));
		return;
	}
	if (LCL_BUFPOOL_AVAILABLE(pciedev->dtoh_ctrlcpl) < 2) {
		/* one to send ack and one for ioctl resp */
		PCI_ERROR(("local buffers available for ioctl process is %d/(MIN: 2)\n",
			LCL_BUFPOOL_AVAILABLE(pciedev->dtoh_ctrlcpl)));
		return;
	}
	txdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, TXDESC);
	rxdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, DTOH, RXDESC);

	avail_ring_entry = WRITE_SPACE_AVAIL(DNGL_RING_RPTR(ring), WRT_PEND(ring),
		RING_MAX_ITEM(ring));
	PCI_TRACE(("ioctl pend: avail_ring_entry %d(%d), txdesc %d(%d), rxdesc %d(%d)\n",
		avail_ring_entry, 2, txdesc, MIN_TXDESC_AVAIL, rxdesc, MIN_RXDESC_AVAIL));

	if ((avail_ring_entry < 2) || (txdesc < 6) || (rxdesc < 9)) {
		PCI_ERROR(("ioctl pend: avail_ring_entry %d(%d), txdesc %d(%d), rxdesc %d(%d)\n",
			avail_ring_entry, 2, txdesc, MIN_TXDESC_AVAIL, rxdesc, MIN_RXDESC_AVAIL));
		return;
	}
	/* now that we have all we need proceed with ioctl */

	/* now that we have the request completely in TCM, ack it now */
	pciedev_send_ioctlack(pciedev, BCMPCIE_SUCCESS);
	dngl_ctrldispatch(pciedev->dngl, pciedev->ioctl_pktptr, pciedev->ioct_resp_buf);
}

void
pciedev_process_ioctl_done(struct dngl_bus *pciedev)
{
	if (pciedev->ioctl_lock) {
		pciedev->ioctl_lock = FALSE;
		PCI_TRACE(("pciedev_process_ioctl_done: Ioctl Lock Released!\n"));
	}
	else {
		PCI_ERROR(("pciedev_process_ioctl_done:"
			"Somehow ioctl lock is already released here?!\n"));
	}

	if (pciedev->ioctl_pktptr) {
		PKTFREE(pciedev->osh, pciedev->ioctl_pktptr, FALSE);
	}
	pciedev->ioctl_pktptr = NULL;
	pciedev->ioctl_pend = FALSE;
	/* There might be msgs waiting in the msgbufs.
	 * Trigger processing.
	 */
	dngl_add_timer(pciedev->delayed_ioctlunlock_timer, 0, FALSE);
}

static void
pciedev_ioctpyld_fetch_cb(struct fetch_rqst *fr, bool cancelled)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)fr->ctx;

	/* If cancelled, retry: drop request for now
	 * Might need to retry sending it down HostFetch
	 */
	/* Reset the ioctl_fetch_rqst but do not free it */
	bzero(pciedev->ioctptr_fetch_rqst, sizeof(struct fetch_rqst));

	if (cancelled) {
		PCI_ERROR(("pciedev_ioctpyld_fetch_cb:"
			"Request cancelled! Dropping ioctl ptr rqst...\n"));
		PCIEDEV_PKTFETCH_CANCEL_CNT_INCR(pciedev);
		goto cleanup;
	}

	if (pciedev->ioctl_pktptr == NULL) {
		PCI_ERROR(("pciedev_ioctpyld_fetch_cb:"
			"Recieved ioctl payload, but header pkt ptr is NULL!\n"));
		ASSERT(0);
		return;
	}

	pciedev->ioctl_pend = TRUE;
	pciedev_process_ioctl_pend(pciedev);
	return;
cleanup:
	pciedev_process_ioctl_done(pciedev);
	return;
}

/* Initialise Flow rings */
static bool
pciedev_setup_flow_rings(struct dngl_bus *pciedev)
{
	int i;

	/* Allocated common local buffer for Flow rings */
	if (!(pciedev->lcl_flowring_buf = pciedev_allocate_lcl_buffers(pciedev,
		PCIEDEV_MAX_LOCALBUF_COUNT, PCIEDEV_MAX_LOCALITEM_COUNT,
		H2DRING_TXPOST_ITEMSIZE))) {
		PCI_TRACE(("Cannot allocate local buffers for tx flow rings"));
		return FALSE;
	}

	/* Allocated common fetch request pool for Flow rings Should we use lcl_buf_pool ? */
	for (i = 0; i < PCIEDEV_MAX_FLOWRINGS_FETCH_REQUESTS; i++) {
		pciedev->flowring_fetch_rqsts[i].used = FALSE;
		pciedev->flowring_fetch_rqsts[i].index = i;
	}

	return TRUE;
}

#ifndef BCMPCIE_SUPPORT_TX_PUSH_RING
static void
BCMATTACHFN(pciedev_free_flowrings)(struct dngl_bus *pciedev)
{
	int i;
	for (i = 0; i < BCMPCIE_MAX_TX_FLOWS; i++) {
		pciedev_free_msgbuf_flowring(pciedev, &pciedev->flow_ring_msgbuf[i]);
		bzero(&pciedev->flow_ring_msgbuf[i], sizeof(msgbuf_ring_t));
	}
	pciedev_free_lcl_buffers(pciedev, pciedev->lcl_flowring_buf, H2DRING_TXPOST_ITEMSIZE);
	pciedev->lcl_flowring_buf = NULL;
}
#endif /* BCMPCIE_SUPPORT_TX_PUSH_RING */

static void
pciedev_process_insert_flowring(struct dngl_bus * pciedev, flowring_pool_t * lcl_pool, uint8 tid)
{
	prioring_pool_t * prioring;
	dll_t * prio;

	ASSERT(tid < PCIE_MAX_TID_COUNT);
	prioring = &pciedev->prioring[tid];

	lcl_pool->prioring = prioring;

	/* insert flow ring after last_fetch_node */
	dll_insert(&lcl_pool->node, prioring->last_fetch_node);
	prioring->last_fetch_node = &lcl_pool->node;

	/* insert prio ring to priority ring list based on its priority */
	if (!prioring->inited)
	{
		/* loop through active prioring list */
		prio = dll_head_p(&pciedev->active_prioring_list);
		while (!dll_end(prio, &pciedev->active_prioring_list)) {
			if (((prioring_pool_t *)prio)->tid < tid) {
				break;
			}
			/* get next priority ring  node */
			prio = dll_next_p(prio);
		}

		prioring->inited = TRUE;
		prioring->maxpktcnt = PCIEDEV_MAX_PACKETFETCH_COUNT;

		prio = dll_prev_p(prio);
		dll_insert(&prioring->node, prio);
	}
}

static void
pciedev_process_remove_flowring(struct dngl_bus * pciedev, flowring_pool_t * lcl_pool)
{
	prioring_pool_t * prioring = lcl_pool->prioring;

	/* set new fetch node */
	if (prioring->last_fetch_node == &lcl_pool->node)
	{
		prioring->last_fetch_node = dll_prev_p(&lcl_pool->node);
	}

	/* delete from active flowring list */
	dll_delete(&lcl_pool->node);

	/* remove from active prioring list if the active flowring list is empty */
	if (dll_empty(&prioring->active_flowring_list))
	{
		dll_delete(&prioring->node);

		prioring->last_fetch_node = &prioring->active_flowring_list;
		prioring->inited = FALSE;
	}

	/* reset flowring info in the node */
	lcl_pool->ring = NULL;
	lcl_pool->prioring = NULL;

	/* free up the node to free pool */
	dll_pool_free(pciedev->flowring_pool, lcl_pool);
}

/* Handle Flow create request */
static int
pciedev_process_flow_ring_create_rqst(struct dngl_bus * pciedev, void *p)
{
	tx_flowring_create_response_t  *resp;
	int status = BCMPCIE_SUCCESS;
	int response;
	msgbuf_ring_t	*flow_ring;
	tx_flowring_create_request_t *flow_ring_create_req = (tx_flowring_create_request_t *)p;
	uint16 index, flowid;
	char eabuf[ETHER_ADDR_STR_LEN];
	flowring_pool_t * lcl_pool;

	flowid = ltoh16(flow_ring_create_req->flow_ring_id);
	index = FLRING_INX(flowid);
	/* Validate mapping index, which should be less than max flows and
	 * flowid, which should be >= flow id start value
	 */
	if ((index >= BCMPCIE_MAX_TX_FLOWS) ||	(flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)) {
		PCI_ERROR(("%d: flow create error invalid flowid\n",
			flowid));
		status = BCMPCIE_RING_ID_INVALID;
		goto send_resp;
	}

	bcm_ether_ntoa((struct ether_addr *)flow_ring_create_req->da, eabuf);
	PCI_TRACE(("Create flowid %d for %c%c%c%c  prio %d"
		"maxitems %d len item %d %p\n",
		flowid, eabuf[0], eabuf[1], eabuf[2],
		eabuf[3], flow_ring_create_req->tid,
		ltoh16(flow_ring_create_req->max_items),
		ltoh16(flow_ring_create_req->len_item),
		pciedev));

	flow_ring = &pciedev->flow_ring_msgbuf[index];
	if (flow_ring->inited) {
		PCI_ERROR(("Ring in use %d\n", flowid));
		status = BCMPCIE_RING_IN_USE;
		goto send_resp;
	}
	if (ltoh16(flow_ring_create_req->len_item) != H2DRING_TXPOST_ITEMSIZE) {
		PCI_ERROR(("Item length mismatch for flowid %d: itemlen %d\n",
			flowid,
			ltoh16(flow_ring_create_req->len_item)));
		status = BCMPCIE_BADOPTION;
		goto send_resp;
	}

	response = dngl_flowring_update(pciedev->dngl, flow_ring_create_req->msg.if_id,
		0, FLOW_RING_CREATE,  flow_ring_create_req->sa,
		flow_ring_create_req->da, flow_ring_create_req->tid);

	if (response == BCME_BADARG) {
		PCI_ERROR(("BAD ifindex for flowid %d\n", flowid));
		status = BCMPCIE_NOTFOUND;
		goto send_resp;
	}
	flow_ring->status_cmpl =
		MALLOCZ(pciedev->osh, ltoh16(flow_ring_create_req->max_items)/NBBY);
	if (!flow_ring->status_cmpl) {
		PCI_ERROR(("Malloc failed for flowid %d\n", flowid));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		status = BCMPCIE_NOMEM;
		goto send_resp;
	}
	flow_ring->inprocess =
		MALLOCZ(pciedev->osh, ltoh16(flow_ring_create_req->max_items)/NBBY);
	if (!flow_ring->inprocess) {
		PCI_ERROR(("Malloc failed for flowid %d\n", flowid));
		MFREE(pciedev->osh, flow_ring->status_cmpl,
			ltoh16(flow_ring_create_req->max_items)/NBBY);
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		status = BCMPCIE_NOMEM;
		goto send_resp;
	}

	/* get a node from the pool */
	lcl_pool = (flowring_pool_t *)dll_pool_alloc(pciedev->flowring_pool);
	if (lcl_pool == NULL) {
		status = BCMPCIE_NOMEM;
		MFREE(pciedev->osh, flow_ring->status_cmpl,
			ltoh16(flow_ring_create_req->max_items)/NBBY);
		MFREE(pciedev->osh, flow_ring->inprocess,
			ltoh16(flow_ring_create_req->max_items)/NBBY);
		goto send_resp;
	}

	/* Save flow ring info */
	lcl_pool->ring = (msgbuf_ring_t *)flow_ring;
	flow_ring->lcl_pool = lcl_pool;

	/* insert to active prioring list */
	pciedev_process_insert_flowring(pciedev, lcl_pool, flow_ring_create_req->tid);

	flow_ring->ringid = flowid;
	flow_ring->status = FLOW_RING_ACTIVE;
	flow_ring->ringmem->idx = ltoh16(flow_ring_create_req->flow_ring_id);
	flow_ring->ringmem->max_item = ltoh16(flow_ring_create_req->max_items);
	flow_ring->ringmem->len_items = ltoh16(flow_ring_create_req->len_item);
	flow_ring->ringmem->base_addr.low_addr =
		flow_ring_create_req->flow_ring_ptr.low_addr;

	flow_ring->ringmem->base_addr.high_addr =
		flow_ring_create_req->flow_ring_ptr.high_addr;

	memcpy(flow_ring->flow_info.sa, flow_ring_create_req->sa, ETHER_ADDR_LEN);
	memcpy(flow_ring->flow_info.da, flow_ring_create_req->da, ETHER_ADDR_LEN);

	flow_ring->flow_info.tid = flow_ring_create_req->tid;
	flow_ring->flow_info.ifindex = flow_ring_create_req->msg.if_id;

	flow_ring->inited = TRUE;
	flow_ring->flow_info.pktinflight = 0;

	flow_ring->flow_info.maxpktcnt = PCIEDEV_MAX_PACKETFETCH_COUNT;
	flow_ring->handle = response & SCBHANDLE_MASK;
	if (response & SCBHANDLE_INFORM_PKTPEND_MASK) {
		flow_ring->flow_info.flags = FLOW_RING_FLAG_INFORM_PKTPEND;
		if (response & SCBHANDLE_PS_STATE_MASK)
			flow_ring->status |= FLOW_RING_PORT_CLOSED;
	}

send_resp:
	if ((resp = (tx_flowring_create_response_t *)
		pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl)) == NULL) {
		PCI_ERROR(("No local buf for flow create resp\n"));
		return BCME_NOMEM;
	}
	bzero(resp, sizeof(tx_flowring_create_response_t));
	resp->msg.msg_type = MSG_TYPE_FLOW_RING_CREATE_CMPLT;
	resp->msg.if_id = flow_ring_create_req->msg.if_id;
	resp->msg.request_id = ltoh32(flow_ring_create_req->msg.request_id);

	resp->cmplt.flow_ring_id = flow_ring_create_req->flow_ring_id;
	resp->cmplt.status = status;

	/* submit it to d2h contrl submission */
	if (!pciedev_xmit_msgbuf_packet(pciedev, resp,
		RING_LEN_ITEMS(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl)) {
		/* find a way to submit local ring buffer back to pool */
		PCI_ERROR(("Flow Create Resp xmit message failed: Not handled now \n"));
		ASSERT(0);
		return BCME_ERROR;
	}
	return BCME_OK;
}

static int
pciedev_flush_flow_ring(struct dngl_bus * pciedev, uint16 flowid)
{
	msgbuf_ring_t	*flow_ring;
	uint16 index;

	index = FLRING_INX(flowid);

	flow_ring = &pciedev->flow_ring_msgbuf[index];
	/* If flush is in process ignore this */

	if (flow_ring->status & FLOW_RING_FLUSH_PENDING)
		return BCME_OK;

	flow_ring->status |= FLOW_RING_FLUSH_PENDING;

	/* Read and Flush any Pkt Fetch */
	pciedev_read_flow_ring_host_buffer(pciedev, flow_ring);

	/* Call to flush any software q and TXFIFO q packets */
	dngl_flowring_update(pciedev->dngl, flow_ring->flow_info.ifindex,
		flow_ring->ringid, FLOW_RING_FLUSH,  (uint8 *)flow_ring->flow_info.sa,
		(uint8 *)flow_ring->flow_info.da, flow_ring->flow_info.tid);

	return BCME_OK;
}

static int
pciedev_process_flow_ring_delete_rqst(struct dngl_bus * pciedev, void *p)
{
	msgbuf_ring_t	*flow_ring;
	tx_flowring_delete_request_t *flow_ring_delete_req = (tx_flowring_delete_request_t *)p;
	uint16 index, flowid;

	flowid = ltoh16(flow_ring_delete_req->flow_ring_id);

	index = FLRING_INX(flowid);
	if ((index >= BCMPCIE_MAX_TX_FLOWS) ||
		(flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)) {
		PCI_ERROR(("%d: flow delete error: invalid flowid\n", flowid));
		return pciedev_send_flow_ring_delete_resp(pciedev, flowid, BCMPCIE_RING_ID_INVALID);
	}

	/* get flow ring on device data base */
	flow_ring = &pciedev->flow_ring_msgbuf[index];
	if (!flow_ring->inited)
		return pciedev_send_flow_ring_delete_resp(pciedev,
			flowid, BCMPCIE_NOTFOUND);
	flow_ring->request_id = flow_ring_delete_req->msg.request_id;
	/* Check if any packets are pending if nothing is pending send response now */
	if (DNGL_RING_WPTR(flow_ring) == DNGL_RING_RPTR(flow_ring)) {
		return pciedev_send_flow_ring_delete_resp(pciedev,
			flowid, BCMPCIE_SUCCESS);
	}
	/* Delete Resp will come after all packets are drained */
	flow_ring->status |= FLOW_RING_DELETE_RESP_PENDING;
	return pciedev_flush_flow_ring(pciedev, flowid);
}

int
pciedev_send_flow_ring_delete_resp(struct dngl_bus * pciedev, uint16 flowid, uint32 status)
{
	tx_flowring_delete_response_t  *resp;
	msgbuf_ring_t	*flow_ring;
	uint16 index;

	index = FLRING_INX(flowid);

	/* get flow ring on device data base TBD */
	flow_ring = &pciedev->flow_ring_msgbuf[index];

	if ((resp = (tx_flowring_delete_response_t *)
		pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl)) == NULL) {
		PCI_ERROR(("Error Cannot send Flow Delete Resp Flow %d \n", flowid));
		return BCME_NOMEM;
	}
	bzero(resp, sizeof(tx_flowring_delete_response_t));
	resp->msg.msg_type = MSG_TYPE_FLOW_RING_DELETE_CMPLT;
	resp->msg.if_id = flow_ring->flow_info.ifindex;
	resp->msg.request_id = flow_ring->request_id;

	resp->cmplt.flow_ring_id = flow_ring->ringid;
	resp->cmplt.status = status;

	/* submit it to d2h contrl submission */
	if (!pciedev_xmit_msgbuf_packet(pciedev, resp,
		RING_LEN_ITEMS(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl)) {
		/* find a way to submit local ring buffer back to pool */
		PCI_ERROR(("Flow Delete Resp xmit message failed: Not handled now \n"));
		ASSERT(0);
		return BCME_ERROR;
	}

	flow_ring->inited = FALSE;

	/* Remove flow ring from active prioring list */
	pciedev_process_remove_flowring(pciedev, flow_ring->lcl_pool);
	pciedev_free_msgbuf_flowring(pciedev, flow_ring);

	flow_ring->status &= ~FLOW_RING_DELETE_RESP_PENDING;
	return BCME_OK;
}


static void
pciedev_process_flow_ring_flush_rqst(struct dngl_bus * pciedev, void *p)
{
	msgbuf_ring_t	*flow_ring;
	tx_flowring_flush_request_t *flow_ring_flush_req = (tx_flowring_flush_request_t *)p;
	uint16 index, flowid;

	flowid = ltoh16(flow_ring_flush_req->flow_ring_id);
	index = FLRING_INX(flowid);

	if ((index >= BCMPCIE_MAX_TX_FLOWS) || (flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)) {
		PCI_ERROR(("%d: flow flush error: invalid flowid\n",
			flowid));
		return pciedev_send_flow_ring_flush_resp(pciedev,
			flow_ring_flush_req->flow_ring_id, BCMPCIE_RING_ID_INVALID);
	}

	/* get flow ring on device data base TBD */
	flow_ring = &pciedev->flow_ring_msgbuf[index];
	flow_ring->request_id = flow_ring_flush_req->msg.request_id;
	/* Check if any packets are pending if nothing is pending send response now */
	if (DNGL_RING_WPTR(flow_ring) == DNGL_RING_RPTR(flow_ring)) {
		pciedev_send_flow_ring_flush_resp(pciedev, flow_ring_flush_req->flow_ring_id,
			BCMPCIE_SUCCESS);
		return;
	}
	flow_ring->status |= FLOW_RING_FLUSH_RESP_PENDING;
	pciedev_flush_flow_ring(pciedev, flowid);
}

void
pciedev_send_flow_ring_flush_resp(struct dngl_bus * pciedev, uint16 flowid, uint32 status)
{
	tx_flowring_flush_response_t  *resp;
	msgbuf_ring_t	*flow_ring;
	uint16 index;

	index = FLRING_INX(flowid);

	/* get flow ring on device data base TBD */
	flow_ring = &pciedev->flow_ring_msgbuf[index];

	if ((resp = (tx_flowring_flush_response_t *)
		pciedev_get_lclbuf_pool(pciedev->dtoh_ctrlcpl)) == NULL)
		return;
	bzero(resp, sizeof(tx_flowring_flush_response_t));
	resp->msg.msg_type = MSG_TYPE_FLOW_RING_FLUSH_CMPLT;
	resp->msg.if_id = flow_ring->flow_info.ifindex;
	resp->msg.request_id = flow_ring->request_id;

	resp->cmplt.flow_ring_id = flow_ring->ringid;
	resp->cmplt.status = status;

	/* submit it to d2h contrl submission */
	if (!pciedev_xmit_msgbuf_packet(pciedev, resp,
		RING_LEN_ITEMS(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl)) {
		/* find a way to submit local ring buffer back to pool */
		PCI_ERROR(("Flow Delete Resp xmit message failed: Not handled now \n"));
		ASSERT(0);
		return;
	}
	flow_ring->status &= ~FLOW_RING_FLUSH_RESP_PENDING;
}

static void
pciedev_free_msgbuf_flowring(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf)
{
	if (msgbuf == NULL)
		return;
	msgbuf->inited = FALSE;
	msgbuf->wr_pending = msgbuf->rd_pending = 0;
	if (msgbuf->buf_pool != pciedev->lcl_flowring_buf) {
		pciedev_free_lcl_buffers(pciedev, pciedev->lcl_flowring_buf,
			RING_LEN_ITEMS(msgbuf));
		msgbuf->buf_pool = pciedev->lcl_flowring_buf;
	}
	if (msgbuf->status_cmpl)
		MFREE(pciedev->osh, msgbuf->status_cmpl, msgbuf->ringmem->max_item/NBBY);

	if (msgbuf->inprocess)
		MFREE(pciedev->osh, msgbuf->inprocess, msgbuf->ringmem->max_item/NBBY);
	msgbuf->inprocess = msgbuf->status_cmpl = NULL;
	msgbuf->fetch_pending = 0;
	msgbuf->hole_pending = FALSE;
	BCMMSGBUF_RING_SET_R_PTR(msgbuf, 0);
}

static uint8
pciedev_dump_lclbuf_pool(msgbuf_ring_t * ring)
{
	uint8 i = 0;
	lcl_buf_pool_t * pool = ring->buf_pool;

	PCI_ERROR(("Ring name: %c%c%c%c  Head %x tail %x free %x \n",
		ring->name[0], ring->name[1],
		ring->name[2], ring->name[3],
		(uint32)pool->head, (uint32)pool->tail, (uint32)pool->free));

	for (i = 0; i < pool->buf_cnt; i++) {
		PCI_ERROR(("buf %x, buf pkt %x, Nxt %x Prv %x\n",
			(uint32)&(pool->buf[i]), (uint32)pool->buf[i].p,
			(uint32)pool->buf[i].nxt, (uint32)pool->buf[i].prv));
	}
	return 0;

}

#ifdef PCIEDEV_DBG_LOCAL_POOL
void
pciedev_check_free_p(msgbuf_ring_t * ring, lcl_buf_t *free, void *p, const char *caller)
{
	while (free != NULL) {
		if (free->p == p) {
			PCI_TRACE(("caller: %c%c%c%c: freeing",
				"up the same p again, %p\n",
				caller[0], caller[1], caller[2], caller[3], p));
			pciedev_dump_lclbuf_pool(ring);
		}
		free = free->nxt;
	}
}
#endif /* PCIEDEV_DBG_LOCAL_POOL */

/* Allocate local buffer */
static lcl_buf_pool_t *
pciedev_allocate_lcl_buffers(struct dngl_bus *pciedev, uint8 bufcnt,
	uint8 item_cnt, uint8 len_items)
{
	uint16 size = 0;
	uint8 i = 0;
	lcl_buf_pool_t * pool;

	size = sizeof(lcl_buf_pool_t) + bufcnt * sizeof(lcl_buf_t);
	pool = MALLOC(pciedev->osh, size);
	if (!pool) {
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return NULL;
	}

	/* Bzero list items */
	bzero(pool, size);

	/* Initialize head/tail/free pointers */
	pool->head = &(pool->buf[0]);
	pool->free = &(pool->buf[0]);
	pool->tail = &(pool->buf[bufcnt - 1]);

	pool->buf_cnt = bufcnt;
	pool->item_cnt = item_cnt;

	pool->head->prv = NULL;
	pool->tail->nxt = NULL;

	/* Link up nodes */
	for (i = 1; i < bufcnt; i++) {
		pool->buf[i - 1].nxt = &(pool->buf[i]);
		pool->buf[i].prv = &(pool->buf[i - 1]);
	}
	for (i = 0; i < pool->buf_cnt; i++) {
		pool->buf[i].p = MALLOC(pciedev->osh, pool->item_cnt * len_items);
	}
	pool->availcnt = pool->buf_cnt;
	return pool;
}

/* Freeup local buffer */
static void pciedev_free_lcl_buffers(struct dngl_bus *pciedev,
	lcl_buf_pool_t * pool, uint16 item_len)
{
	uint8 i = 0;

	if (pool) {
		/* Free up individual buffers */
		for (i = 0; i < pool->buf_cnt; i++) {
			MFREE(pciedev->osh, pool->buf[i].p, pool->item_cnt * item_len);
		}
		/* free up the pool structure */
		MFREE(pciedev->osh, pool, sizeof(lcl_buf_pool_t) +
		pool->buf_cnt * sizeof(lcl_buf_t));
	}
}

static int
pciedev_dump_txring_msgbuf_ring_info(msgbuf_ring_t *msgbuf)
{
	lcl_buf_pool_t *pool = msgbuf->buf_pool;
	uint32 i;
	char eabuf[ETHER_ADDR_STR_LEN];
	bcm_ether_ntoa((struct ether_addr *)msgbuf->flow_info.da, eabuf);

	PCI_ERROR((
		"ring: %c%c%c%c, ID %d, phase %d, r_ptr 0x%04x, w_ptr 0x%04x\n",
		msgbuf->name[0], msgbuf->name[1],
		msgbuf->name[2], msgbuf->name[3],
		msgbuf->ringid, msgbuf->current_phase,
		(uint32)(msgbuf->tcm_rs_r_ptr),
		(uint32)(msgbuf->tcm_rs_w_ptr)));

	PCI_ERROR((
		"r_mem 0x%04x, low 0x%04x, high 0x%04x\n",
		(uint32)(msgbuf->ringmem),
		(uint32)LOW_ADDR_32(msgbuf->ringmem->base_addr),
		(uint32)HIGH_ADDR_32(msgbuf->ringmem->base_addr)));

	PCI_ERROR((
		"Flow Init %d Addr :%c%c%c%c tid=%d if=%d status=%d handle=%d\n",
		msgbuf->inited, eabuf[0],
		eabuf[1], eabuf[2],
		eabuf[3], msgbuf->flow_info.tid,
		msgbuf->flow_info.ifindex,
		msgbuf->status, msgbuf->handle));

	PCI_ERROR(("RP %d RDp %d WR %d\n", DNGL_RING_RPTR(msgbuf), msgbuf->rd_pending,
		DNGL_RING_WPTR(msgbuf)));
	PCI_TRACE(("max %d, avaialble %d\n", pool->buf_cnt, pool->availcnt));
	for (i = 0; i < pool->buf_cnt; i++) {
		PCI_TRACE(("%d:  %x\n", i, (uint32)pool->buf[i].p));
	}

	PCI_ERROR(("-----------------------------------------\n"));
	return 0;
}

static int
pciedev_dump_prioring_info(struct dngl_bus * pciedev)
{
	struct dll * cur, *prio;
	msgbuf_ring_t *msgbuf;

	PCI_ERROR(("\n\n******************************************\n"));
	PCI_ERROR(("priority list %x head %x tail %x\n",
		(uint32)&pciedev->active_prioring_list,
		(uint32)dll_head_p(&pciedev->active_prioring_list),
		(uint32)dll_tail_p(&pciedev->active_prioring_list)));
	PCI_ERROR(("+++++++++++++++++++++++++++++++++++++++++\n"));
	OSL_DELAY(200000);

	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		PCI_ERROR(("prio ring tid %d, cur %x, next %x, prev %x\n",
			prioring->tid, (uint32)prio, (uint32)prio->next_p, (uint32)prio->prev_p));
		PCI_ERROR(("-----------------------------------------\n"));
		PCI_ERROR(("flow ring list %x head %x tail %x, fetch %x\n",
			(uint32)&prioring->active_flowring_list,
			(uint32)dll_head_p(&prioring->active_flowring_list),
			(uint32)dll_tail_p(&prioring->active_flowring_list),
			(uint32)prioring->last_fetch_node));
		PCI_ERROR(("-----------------------------------------\n"));
		OSL_DELAY(200000);

		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			char eabuf[ETHER_ADDR_STR_LEN];
			msgbuf = ((flowring_pool_t *)cur)->ring;
			bcm_ether_ntoa((struct ether_addr *)msgbuf->flow_info.da, eabuf);
			PCI_ERROR(("ring: %c%c%c%c, ID %d, tid %d, cur %p, next %p, prev %p\n",
				msgbuf->name[0], msgbuf->name[1],
				msgbuf->name[2], msgbuf->name[3],
				msgbuf->ringid, msgbuf->flow_info.tid,
				cur, cur->next_p, cur->prev_p));
			PCI_ERROR(("Addr :%s, RP %d RDp %d WR %d, max fetch count %d\n",
				eabuf, DNGL_RING_RPTR(msgbuf), msgbuf->rd_pending,
				DNGL_RING_WPTR(msgbuf), msgbuf->flow_info.maxpktcnt));
			cur = dll_next_p(cur);
			OSL_DELAY(100000);
		}
		PCI_ERROR(("+++++++++++++++++++++++++++++++++++++++++\n"));

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}

	return 0;
}

uint16 tmp = 0;
static int
pciedev_dump_rw_blob(struct dngl_bus * pciedev)
{
	uint32 i;
#ifdef EVENT_LOG_COMPILE
	msgbuf_ring_t	*flow_ring;
#endif /* EVENT_LOG_COMPILE */
	struct dll * cur, * prio;

	for (i = 0; i < BCMPCIE_H2D_COMMON_MSGRINGS; i++) {
		PCI_ERROR(("ID:%d RD %d : WR: %d\n", i,
			DNGL_RING_RPTR(&pciedev->cmn_msgbuf_ring[i]),
			DNGL_RING_WPTR(&pciedev->cmn_msgbuf_ring[i])));
	}

	/* loop through active queues */
	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
#ifdef EVENT_LOG_COMPILE
			flow_ring = ((flowring_pool_t *)cur)->ring;
		BCM_REFERENCE(flow_ring);


		PCI_ERROR(("ID:%d RD:%d WR:%d RDp: %d\n", i,
			DNGL_RING_RPTR(flow_ring),
			DNGL_RING_WPTR(flow_ring),
			flow_ring->rd_pending));
#endif /* EVENT_LOG_COMPILE */
			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}
	return 0;
}

/* Free up inuse pool info */
static void
pciedev_free_inuse_bufpool(struct dngl_bus *pciedev, lcl_buf_pool_t * bufpool)
{
	inuse_lclbuf_pool_t * inuse = bufpool->inuse_pool;
	uint16 size = 0;
	uint8 bufcnt = bufpool->buf_cnt + 1;

	size = sizeof(inuse_lclbuf_pool_t) + bufcnt * sizeof(inuse_lcl_buf_t);
	if (inuse)
		MFREE(pciedev->osh, inuse, size);
}
/* Initialise inuse pool */
static void
pciedev_init_inuse_lclbuf_pool_t(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	inuse_lclbuf_pool_t * pool = NULL;
	uint16 size = 0;

	/* read/write pointer requires to allocate one element extra */
	/* read/write pointer mechanism has lower mem requirement than a linked list */
	uint8 bufcnt = ring->buf_pool->buf_cnt + 1;

	size = sizeof(inuse_lclbuf_pool_t) + bufcnt * sizeof(inuse_lcl_buf_t);
	pool = MALLOC(pciedev->osh, size);
	if (pool == NULL) {
		PCI_ERROR(("Could not allocate pool\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return;
	}
	PCI_TRACE(("ring %c%c%c%c : bufcnt %d size %d \n",
		ring->name[0], ring->name[1],
		ring->name[2], ring->name[3],
		bufcnt, size));

	/* Bzero list items */
	bzero(pool, size);

	/* initialise depth */
	pool->depth = bufcnt;

	/* pool inited */
	pool->inited = 1;

	ring->buf_pool->inuse_pool = pool;
}

static bool
pciedev_can_goto_deepsleep(struct dngl_bus * pciedev)
{
	if (pciedev->cur_ltr_state != LTR_SLEEP) {
		PCI_TRACE(("LTR state:%d\n", pciedev->cur_ltr_state));
		return FALSE;
	}
	if (pciedev->dtoh_rxcpl->buf_pool->pend_item_cnt != 0 ||
		pciedev->dtoh_txcpl->buf_pool->pend_item_cnt != 0) {
		PCI_TRACE(("rxcpl item:%d, txcpl item:%d\n",
			pciedev->dtoh_rxcpl->buf_pool->pend_item_cnt,
			pciedev->dtoh_txcpl->buf_pool->pend_item_cnt));
		return FALSE;
	}
	if (dma_txactive(pciedev->h2d_di) || dma_rxactive(pciedev->h2d_di) ||
		dma_txactive(pciedev->d2h_di) || dma_rxactive(pciedev->d2h_di))
	{
		PCI_TRACE(("MEM2MEM DMA H2D_TX %d, H2D_RX %d, D2H_TX: %d, D2H_RX: %d\n",
			dma_txactive(pciedev->h2d_di), dma_rxactive(pciedev->h2d_di),
			dma_txactive(pciedev->d2h_di), dma_rxactive(pciedev->d2h_di)));
		return FALSE;
	}
	return TRUE;
}

static void
pciedev_ds_check_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) t->context;
	if (pciedev->ds_check_timer_max ++ > PCIE_DS_CHECK_MAX_ITERATION ||
		pciedev->in_d3_suspend) {
		pciedev->ds_check_timer_on = FALSE;
		pciedev->ds_check_timer_max = 0;
		PCI_TRACE(("pciedev_ds_check_timerfn: timer done:%d!\n",
			pciedev->ds_check_timer_max));
		return;
	}
	if (pciedev_can_goto_deepsleep(pciedev)) {
		pciedev->ds_check_timer_on = FALSE;
		pciedev->ds_check_timer_max = 0;
		PCI_TRACE(("pciedev_ds_check_timerfn: sending deepsleep req to host\n"));
		pciedev_deepsleep_enter_req(pciedev);
	}
	else {
		PCI_TRACE(("pciedev_ds_check_timerfn: restart timer to retry.\n"));
		dngl_add_timer(pciedev->ds_check_timer,
			pciedev->ds_check_interval, FALSE);
	}
}

static void
pciedev_deepsleep_check_periodic(struct dngl_bus *pciedev)
{
	PCI_TRACE("pciedev_deepsleep_check_periodic\n");
	if (pciedev->in_d3_suspend)
		return;
	if (pciedev->ds_check_timer_on == FALSE) {
		pciedev->ds_check_timer_on = TRUE;
		dngl_add_timer(pciedev->ds_check_timer,
			pciedev->ds_check_interval, FALSE);
	}
}

static void
pciedev_no_ds_dw_deassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;

	PCI_TRACE("pciedev_no_ds_dw_deassrt\n");
	if (pciedev->in_d3_suspend)
		return;
	/* First try to enter deepsleep. If cannot, start a
	* timer to retry.
	*/
	if (pciedev_can_goto_deepsleep(pciedev)) {
		pciedev_deepsleep_enter_req(pciedev);
	} else {
		/* Start a timer to check for no pending DMA to host */
		pciedev_deepsleep_check_periodic(pciedev);
	}
}

static void
pciedev_no_ds_perst_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_no_ds_perst_assrt\n"));
	/* Do not allow chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, TRUE);
}

static void
pciedev_ds_check_dw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_check_dw_assrt\n"));
	/* Disable deepsleep check timer */
	if (pciedev->ds_check_timer_on) {
		dngl_del_timer(pciedev->ds_check_timer);
		pciedev->ds_check_timer_on = FALSE;
	}
	if (pciedev->in_d3_suspend)
		return;
	pciedev_deepsleep_exit_notify(pciedev);
}

static void
pciedev_ds_check_perst_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_check_perst_assrt\n"));
	/* Disable deepsleep check timer */
	if (pciedev->ds_check_timer_on) {
		dngl_del_timer(pciedev->ds_check_timer);
		pciedev->ds_check_timer_on = FALSE;
	}
	/* Allow chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, FALSE);
}
static void
pciedev_ds_check_ds_allowed(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_check_ds_allowed\n"));
	/* Allow chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, FALSE);
}

static void
pciedev_ds_d0_db_dtoh(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE("pciedev_ds_d0_db_dtoh\n");
	/* Prevent chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, TRUE);
	pciedev_deepsleep_exit_notify(pciedev);
	pciedev_deepsleep_check_periodic(pciedev);
}
static void
pciedev_ds_d0_dw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE("pciedev_ds_d0_dw_assrt\n");
	pciedev_disable_deepsleep(pciedev, TRUE);
	if (pciedev->in_d3_suspend)
		return;
	pciedev_deepsleep_exit_notify(pciedev);
}
static void
pciedev_ds_d3cold_perst_dassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE("pciedev_ds_d3cold_perst_dassrt\n");
	pciedev_disable_deepsleep(pciedev, TRUE);
}

static void
pciedev_ds_nods_d3cold_perst_dassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE("pciedev_ds_nods_d3cold_perst_dassrt\n");
	pciedev_disable_deepsleep(pciedev, TRUE);
}
static void
pciedev_ds_d3cold_dw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE("pciedev_ds_nods_d3cold_perst_dassrt\n");
	pciedev_disable_deepsleep(pciedev, TRUE);
}
static void
pciedev_ds_d3cold_hw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE("pciedev_ds_nods_d3cold_perst_dassrt\n");
	pciedev_disable_deepsleep(pciedev, TRUE);
}
static void
pciedev_ds_nods_d3cold_dw_dassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE("pciedev_ds_nods_d3cold_dw_dassrt\n");
	pciedev_disable_deepsleep(pciedev, TRUE);
}

/*
* This routine implements the state machine for the deepsleep protocol.
* http://hwnbu-twiki.sj.broadcom.com/bin/view/Mwgroup/OlympicDeepSleepOOBProtocol
*/
static void
pciedev_deepsleep_engine(struct dngl_bus * pciedev, bcmpcie_deepsleep_event_t event)
{
	if (pciedev->ds_state == DS_DISABLED_STATE || pciedev->ds_state == DS_INVALID_STATE) {
		PCI_TRACE(("pciedev_deepsleep_engine: invalid state device_wake not enabled\n"));
		return;
	}
	pciedev_ds_state_tbl_entry_t ds_entry = pciedev_ds_state_tbl[pciedev->ds_state][event];
	if (ds_entry.action_fn) {
		PCI_TRACE(("state:%s event:%s Transition:%s\n",
			pciedev_ds_state_name(pciedev->ds_state),
			pciedev_ds_event_name(event),
			pciedev_ds_state_name(ds_entry.transition)));
		ds_entry.action_fn(pciedev);
	}
	if (ds_entry.transition != DS_INVALID_STATE) {
		if (pciedev->ds_log_count == PCIE_MAX_DS_LOG_ENTRY)
			pciedev->ds_log_count = 0;
		pciedev->ds_log[pciedev->ds_log_count].ds_state = pciedev->ds_state;
		pciedev->ds_log[pciedev->ds_log_count].ds_event = event;
		pciedev->ds_log[pciedev->ds_log_count].ds_transition = ds_entry.transition;
		pciedev->ds_log[pciedev->ds_log_count].ds_time = hndrte_time();
		pciedev->ds_log_count ++;

		pciedev->ds_state = ds_entry.transition;
	}
}

static const char *
pciedev_ds_state_name(bcmpcie_deepsleep_state_t state)
{
	const char *ds_state_names[DS_LAST_STATE] = {"NO_DS_STATE", "DS_CHECK_STATE",
		"DS_D0_STATE,", "NODS_D3COLD_STATE", "DS_D3COLD_STATE"};
	if (state < 0 || state >= DS_LAST_STATE)
		return "";
	return ds_state_names[state];
}

static const char *
pciedev_ds_event_name(bcmpcie_deepsleep_event_t event)
{
	const char *ds_ev_names[DS_LAST_EVENT] = {"DW_ASSRT_EVENT", "DW_DASSRT_EVENT",
		"PERST_ASSRT_EVENT", "PERST_DEASSRT_EVENT", "DB_TOH_EVENT", "DS_ALLOWED_EVENT",
		"DS_NOT_ALLOWED_EVENT", "HOSTWAKE_ASSRT_EVENT"};

	if (event >= DS_LAST_EVENT)
		return "";
	return ds_ev_names[event];
}

static int
pciedev_deepsleep_engine_log_dump(struct dngl_bus * pciedev)
{
	int i;
	if (pciedev->ds_state == DS_DISABLED_STATE)
		return BCME_ERROR;
	for (i = 0; i < PCIE_MAX_DS_LOG_ENTRY; i++) {
		if (pciedev->ds_log[i].ds_state == DS_INVALID_STATE)
			break;
		if (pciedev->ds_log[i].ds_time == 0)
			continue;
		printf("0x%x:State:%s Event:%s Transition:%s\n",
			pciedev->ds_log[i].ds_time,
			pciedev_ds_state_name(pciedev->ds_log[i].ds_state),
			pciedev_ds_event_name(pciedev->ds_log[i].ds_event),
			pciedev_ds_state_name(pciedev->ds_log[i].ds_transition));
	}
	return BCME_OK;
}

/*
* Check the PCIE configuration device_status_control register at 0xb4.
* Read the max read request size (MRRS) bit 14-12 to see what the system allows.

* 0xb4[14:12] = 000 -> 128B
* 0xb4[14:12] = 001 -> 256B
* 0xb4[14:12] = 010 -> 512B
* 0xb4[14:12] = 011 -> 1024B
* 0xb4[14:12] = 100 -> 2048B
* 0xb4[14:12] = 101 -> 4096B

* Program the burst length to be 1K (0x6) if the system MRRS >= 1024B,
* otherwise program it up to the MRRS support.
*/
static uint32
pcie_h2ddma_tx_get_burstlen(struct dngl_bus *pciedev)
{
	uint32 burstlen = 2; /* 64B */
	uint32 mprs;

	W_REG(pciedev->osh, &pciedev->regs->configaddr, PCIECFGREG_DEVCONTROL);
	mprs = (R_REG(pciedev->osh, &pciedev->regs->configdata) &
		PCIECFGREG_DEVCONTROL_MRRS_MASK) >>
		PCIECFGREG_DEVCONTROL_MRRS_SHFT;

	switch (mprs) {
		case 0:
			burstlen = 3; /* 128B */
		break;
		case 1:
			burstlen = 4; /* 256B */
		break;
		case 2:
			burstlen = 5; /* 512B */
		break;
		case 3:
			burstlen = 6; /* 1024B */
		break;
		default:
			burstlen = 6; /* 1024B */

	}
	return burstlen;
}
