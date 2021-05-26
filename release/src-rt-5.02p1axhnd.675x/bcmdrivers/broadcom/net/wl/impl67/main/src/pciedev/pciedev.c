/*
 * Broadcom PCIE device-side driver
 * pciedev is the bus device for pcie full dongle
 * Core bus operations are as below
 * 1. interrupt mechanism
 * 2. circular buffer
 * 3. message buffer protocol
 * 4. DMA handling
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
 * $Id: pciedev.c  $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <bcm_math.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmnvram.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <osl.h>
#include <hndsoc.h>
#include <hnd_cplt.h>
#include <ethernet.h>
#include <802.1d.h>
#include <sbsdio.h>
#include <sbchipc.h>
#include <pcie_core.h>
#include <dngl_bus.h>
#include <dngl_api.h>
#include <bcmcdc.h>
#include <bcm_buzzz.h>
#include <bcmpcie.h>
#include <bcmhme.h>
#include <bcmmsgbuf.h>
#include <bcmotp.h>
#include <hndpmu.h>
#include <circularbuf.h>
#include <pciedev_priv.h>
#include <pciedev.h>
#include <pciedev_dbg.h>
#include <pcicfg.h>
#include <wlfc_proto.h>
#include <rte_fetch.h>
#include <rte_cons.h>
#include <rte_uart.h>
#include <rte_trap.h>
#include <rte_ioctl.h>
#include <hnd_resvpool.h>
#if defined(SAVERESTORE)
#include <saverestore.h>
#endif /* SAVERESTORE */
#include <d11_cfg.h>
#include <hnd_event.h>
#include <hnd_ds.h>
#include <wlioctl.h>

#ifdef HNDBME
#include <hndbme.h>
#endif // endif
#if defined(HNDM2M)
#include <hndm2m.h>
#endif // endif
#include <rte_cfg.h>

#ifdef BCMHWA
#include <hwa_export.h>
#endif // endif
#ifdef BCM_CSIMON
#include <bcm_csimon.h>
#endif /* BCM_CSIMON */

#ifdef SW_PAGING
#include <swpaging.h>
#endif // endif

#define PCIE_POOLRECLAIM_LCLBUF_DEPTH 4

/* Induce errors in the rx path to check the code  */
#ifdef TEST_DROP_PCIEDEV_RX_FRAMES
static uint32 pciedev_test_drop_rxframe_max = 5000;
static uint32 pciedev_test_drop_norxcpl_max = 100;
static uint32 pciedev_test_drop_rxframe = 0;
static uint32 pciedev_test_drop_norxcpl = 0;
static uint32 pciedev_test_dropped_norxcpls = 0;
static uint32 pciedev_test_dropped_rxframes = 0;
#endif /* TEST_DROP_PCIEDEV_RX_FRAMES */

/** enable dumping COE regs on device traps */
#define DUMP_PCIEREGS_ON_TRAP			1

#define PCIEGEN2_COE_PVT_TL_CTRL_0		0x800
#define PCIEGEN2_COE_PVT_PHY_DBG_CLKREQ_0	0x1e10
#define COE_PVT_TL_CTRL_0_PM_DIS_L1_REENTRY_BIT	24

#define PCIEGEN2_PVT_REG_PM_CLK_PERIOD		0x184c
#define PMCR_TREFUP_DEFAULT			6 /* in useconds */
#define PMCR_TREFUP_TPOWERON_OFFSET		5 /* in useconds */
#define	REG_PMCR_EXT_TIMER_SEL_MASK		0xC000
#define	REG_PMCR_EXT_TIMER_SHIFT		10

static uint32 pciedev_get_pwrstats(struct dngl_bus *pciedev, char *buf);
void pciedev_ds_check_fail_log(struct dngl_bus *pciedev, ds_check_fail_t ds_check_fail);
#if defined(SAVERESTORE)
static void pciedev_sr_restore(void *arg, bool wake_from_deepsleep);
static bool pciedev_sr_save(void *arg);
#endif /* SAVERESTORE */

#ifdef HOSTREADY_ONLY_ENABLED
bool	_pciedev_hostready = TRUE;
#else
bool	_pciedev_hostready = FALSE;
#endif /* HOSTREADY_ONLY_ENABLED */

#ifdef PCIE_DEEP_SLEEP

#if defined(BCMPCIEDEV_ENABLED) && !defined(BCMMSGBUF)
#error "Bad configuration, for PCIE FULL DONGLE build Need BCMMSGBUF"
#endif // endif

static int pciedev_deepsleep_engine_log_dump(struct dngl_bus * pciedev);
#endif /* PCIE_DEEP_SLEEP */

#define SCBHANDLE_PS_STATE_MASK		(1 << 8)
#define SCBHANDLE_INFORM_PKTPEND_MASK	(1 << 9)
#define SCBHANDLE_MASK			(0xff)
#define PCIEGEN2_MSI_CAP_REG_OFFSET	0x58
#define MSI_ENABLED_CHECK		0x10000

#define COMMON_RING			TRUE
#define DYNAMIC_RING			FALSE

static const char BCMATTACHDATA(tx_burstlen)[] = "tx_burstlen";
static const char BCMATTACHDATA(rx_burstlen)[] = "rx_burstlen";
static const char BCMATTACHDATA(rstr_pcie_linkdown_trap_dis)[] = "pcie_linkdown_trap_dis";

static void pciedev_send_ioctlack(struct dngl_bus *bus, uint16 status);
static void pciedev_send_ring_status(struct dngl_bus *pciedev, uint16 w_idx,
	uint32 req_id, uint32 status, uint16 ring_id);
static void pciedev_send_ts_cmpl(struct dngl_bus *bus, uint16 status);
static int pciedev_schedule_ctrl_cmplt(struct dngl_bus *pciedev,
	ctrl_completion_item_t *ctrl_cmplt);
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
static void pciedev_ds_hc_trigger(struct dngl_bus *pciedev);
static const char BCMATTACHDATA(rstr_ds_hc_enable)[] = "ds_hc_enable";
static const char BCMATTACHDATA(rstr_event_pool_max)[] = "event_pool_max";

static void pciedev_initiate_dummy_fwtsinfo(struct dngl_bus *pciedev, uint32 val);
static void pciedev_dump_ts_pool(struct dngl_bus *pciedev);
static void pciedev_dump_last_host_ts(struct dngl_bus *pciedev);
static void pciedev_fwtsinfo_timerfn(dngl_timer_t *t);

static void pciedev_process_ioctlptr_rqst(struct dngl_bus *pciedev, void *p);
static void pciedev_process_ts_rqst(struct dngl_bus *pciedev, void *p);
static void pciedev_usrltrtest_timerfn(dngl_timer_t *t);
static void pciedev_reset_pcie_interrupt(struct dngl_bus * pciedev);
static uint pd_msglevel = 1;

static int pciedev_xmit_wlevnt_cmplt(struct dngl_bus *pciedev,
	uint32 pktid, uint16 buflen, uint8 ifidx, msgbuf_ring_t *ring);
static int pciedev_xmit_fw_ts_cmplt(struct dngl_bus *pciedev,
	uint32 pktid, uint16 buflen, uint8 ifidx, msgbuf_ring_t *msgbuf, uint8 flags);

static void pciedev_generate_dummy_fwtsinfo(struct dngl_bus *pciedev);

static int pciedev_hw_LTR_war(struct dngl_bus *pciedev);
static void pciedev_set_LTRvals(struct dngl_bus *pciedev);
static uint32 pciedev_usr_test_ltr(struct dngl_bus *pciedev, uint32 timer_val, uint8 state);

#define PCIE_LPBK_SUPPORT_TXQ_SIZE	256
#define PCIE_LPBK_SUPPORT_TXQ_NPREC	1
#define PCIE_WAKE_SUPPORT_TXQ_NPREC	1

/* mail box communication handlers */
static void pciedev_h2d_mb_data_check(struct dngl_bus *pciedev);
static void pciedev_h2d_mb_data_process(struct dngl_bus *pciedev, uint32 mb_data);
static void pciedev_perst_reset_process(struct dngl_bus *pciedev);
static void pciedev_crwlpciegen2(struct dngl_bus *pciedev);
static void pciedev_crwlpciegen2_61(struct dngl_bus *pciedev);
static void pciedev_crwlpciegen2_180(struct dngl_bus *pciedev);
static void pciedev_crwlpciegen2_182(struct dngl_bus *pciedev);
static void pciedev_reg_pm_clk_period(struct dngl_bus *pciedev);
static void pciedev_handle_host_D3_info(struct dngl_bus *pciedev);
static void pciedev_D3_ack(struct dngl_bus *pciedev);

#ifdef PCIE_DEEP_SLEEP
#if !defined(PCIEDEV_INBAND_DW)
static void pciedev_disable_ds_state_machine(struct dngl_bus *pciedev);
#endif /* !defined(PCIEDEV_INBAND_DW) */
static void pciedev_ds_check_timerfn(dngl_timer_t *t);
#endif /* PCIE_DEEP_SLEEP */

static uint8  pciedev_host_wake_gpio_init(struct dngl_bus *pciedev);
#ifdef TIMESYNC_GPIO
static uint8  pciedev_time_sync_gpio_init(struct dngl_bus *pciedev);
#endif /* TIMESYNC_GPIO */
static void pciedev_enable_perst_deassert_wake(struct dngl_bus *pciedev);

static uint32 pciedev_get_avail_host_rxbufs(struct dngl_bus *pciedev);
static void pciedev_sched_msgbuf_intr_process(dngl_timer_t *t);
static void pciedev_ioctpyld_fetch_cb(struct fetch_rqst *fr, bool cancelled);
static void pciedev_tsync_pyld_fetch_cb(struct fetch_rqst *fr, bool cancelled);
static void pciedev_flush_h2d_dma_xfers(struct dngl_bus *pciedev, uint8 dmach);
static void pciedev_cancel_pending_fetch_requests(struct dngl_bus *pciedev, uint8 dmach);
static void pciedev_flush_fetch_requests(void* arg);

#if defined(BCMPCIE_IPC_HPA)
/** Host PacketId Audit Implementation for DHD_PCIE_PKTID mode */
bcmpcie_ipc_hpa_t * /* Allocate and Initialize HPA state */
BCMATTACHFN(bcmpcie_ipc_hpa_init)(osl_t *osh)
{
	int i;
	bcmpcie_ipc_hpa_t * hpa;

	hpa = (bcmpcie_ipc_hpa_t *) MALLOCZ(osh, sizeof(bcmpcie_ipc_hpa_t));
	if (hpa == BCMPCIE_IPC_HPA_NULL) {
		PCI_ERROR(("bcmpcie_ipc_hpa_t alloc failure\n"));
		goto done;
	}

	for (i = 0; i < BCMPCIE_IPC_HPA_MWBMAP_TOTAL; i++) {
		hpa->mwbmap[i] = bcm_mwbmap_init(osh, BCMPCIE_IPC_HPA_MWBMAP_ITEMS);
		if (hpa->mwbmap[i] == (struct bcm_mwbmap*)NULL) {
			PCI_ERROR(("bcmpcie_ipc_hpa mwbmap[%d] init failure\n", i));
			goto fail;
		}
	}

	bcm_mwbmap_force(hpa->mwbmap[0], 0); /* pktid = 0 is invalid */

	goto done;

fail:
	for (i = 0; i < BCMPCIE_IPC_HPA_MWBMAP_TOTAL; i++) {
		if (hpa->mwbmap[i] != (struct bcm_mwbmap *)NULL) {
			bcm_mwbmap_fini(osh, hpa->mwbmap[i]);
			hpa->mwbmap[i]  = (struct bcm_mwbmap *)NULL;
		}
	}
	MFREE(osh, hpa, sizeof(bcmpcie_ipc_hpa_t));
	hpa = BCMPCIE_IPC_HPA_NULL;

done:
	return hpa;

} /* bcmpcie_ipc_hpa_init() */

void /* Debug dump HPA runtime state */
bcmpcie_ipc_hpa_dump(bcmpcie_ipc_hpa_t *hpa)
{
	bcmpcie_ipc_hpa_stats_t * ctrl = &hpa->stats[BCMPCIE_IPC_PATH_CONTROL];
	bcmpcie_ipc_hpa_stats_t * recv = &hpa->stats[BCMPCIE_IPC_PATH_RECEIVE];
	bcmpcie_ipc_hpa_stats_t * xmit = &hpa->stats[BCMPCIE_IPC_PATH_TRANSMIT];

	if (hpa == BCMPCIE_IPC_HPA_NULL) {
		printf("HPA NULL\n");
		return;
	}

	printf("HPA Stats: Format PATH Test<Req,Rsp,Rlb> Fail<Req,Rsp,Rlb>\n"
		"\tCTRL Test<%u,%u,%u> Fail<%u,%u,%u>\n"
		"\tRECV Test<%u,%u,%u> Fail<%u,%u,%u>\n"
		"\tXMIT Test<%u,%u,%u> Fail<%u,%u,%u>\n"
		"\tErrors <%u>\n",
		ctrl->test[0], ctrl->test[1], ctrl->test[2],
		ctrl->fail[0], ctrl->fail[1], ctrl->fail[2],
		recv->test[0], recv->test[1], recv->test[2],
		recv->fail[0], recv->fail[1], recv->fail[2],
		xmit->test[0], xmit->test[1], xmit->test[2],
		xmit->fail[0], xmit->fail[1], xmit->fail[2],
		hpa->errors);

#ifdef BCMPCIE_IPC_HPA_PKTID_ERR_LOG
	{
		uint32 i, *log = hpa->log;
		for (i = 0; i < BCMPCIE_IPC_HPA_PKTID_ERR_MAX; ++i)
			if (log[i])
				printf("\t\tPktId[%d] = 0x%08x %u\n", i, (int)log[i], log[i]);
	}
#endif /* BCMPCIE_IPC_HPA_PKTID_ERR_LOG */

} /* bcmpcie_ipc_hpa_dump() */

void /* Audit failure handler - count, log, trap, ... */
bcmpcie_ipc_hpa_trap(bcmpcie_ipc_hpa_t *hpa, uint32 hpa_pktid,
	const bcmpcie_ipc_path_t hpa_path, const bcmpcie_ipc_trans_t hpa_trans)
{
	hpa->stats[hpa_path].fail[hpa_trans]++; /* increment stats */
	BCMPCIE_IPC_HPA_PKTID_ERR(hpa, hpa_pktid); /* log pktid */
	TRAP_IN_PCIEDRV(("HPA pktid<0x%08x,%u> path<%u> trans<%u> errors<%u>\n",
		hpa_pktid, hpa_pktid, hpa_path, hpa_trans, hpa->errors));
} /* bcmpcie_ipc_hpa_trap() */

void /* Test a PktId on entry and exit from dongle (including rollback) */
bcmpcie_ipc_hpa_test(struct dngl_bus *pciedev, uint32 hpa_pktid,
	const bcmpcie_ipc_path_t hpa_path, const bcmpcie_ipc_trans_t hpa_trans)
{
	int hpa_mwbmap_bitix;
	struct bcm_mwbmap *hpa_mwbmap;
	bcmpcie_ipc_hpa_t *hpa = ((bcmpcie_ipc_hpa_t *)((pciedev)->hpa_hndl));

	if (hpa == NULL) {
		TRAP_IN_PCIEDRV(("bcmpcie_ipc_hpa_test hpa NULL"));
		goto done;
	}

	if ((hpa_pktid == 0) || (hpa_pktid > BCMPCIE_IPC_HPA_PKTID_TOTAL)) {
		goto trap;
	}

	hpa->stats[hpa_path].test[hpa_trans]++;
	hpa_mwbmap = hpa->mwbmap[BCMPCIE_IPC_HPA_MWBMAP_INDEX(hpa_pktid)];
	hpa_mwbmap_bitix = BCMPCIE_IPC_HPA_MWBMAP_BITIX(hpa_pktid);

	if (hpa_trans == BCMPCIE_IPC_TRANS_REQUEST) { /* Request */
		if (!bcm_mwbmap_isfree(hpa_mwbmap, hpa_mwbmap_bitix))
			goto trap; /* Trap if bitix is already allocated */
		bcm_mwbmap_force(hpa_mwbmap, hpa_mwbmap_bitix); /* otherwise allocate */
	} else { /* Response or Rollback */
		if (bcm_mwbmap_isfree(hpa_mwbmap, hpa_mwbmap_bitix))
			goto trap; /* Trap if bitix is NOT allocated */
		bcm_mwbmap_free(hpa_mwbmap, hpa_mwbmap_bitix); /* release bit */
	}
	goto done;

trap:
	bcmpcie_ipc_hpa_trap(hpa, hpa_pktid, hpa_path, hpa_trans);

done:
	return;

} /* bcmpcie_ipc_hpa_test() */

#endif   /* BCMPCIE_IPC_HPA */

#if defined(WLSQS)
/**
 * SQS : Abstract Flowrings as an extension of SCB MPDU Queues, eliminating
 * a dual stage scheduling in a full dongle driver.
 */

/* Private to pciedev.c */
static int  sqs_vpkts_dump(struct dngl_bus *pciedev, uint16 cfp_flowid);

static int /* wl bus:sqs_vpkts debug dump the vpkts for flowrings */
sqs_vpkts_dump(struct dngl_bus *pciedev, uint16 cfp_flowid)
{
	msgbuf_ring_t	*msgbuf;	/* flow ring hdl */
	struct dll	*cur, *prio;	/* d11 hdl */
	int new_wr, old_wr;
	int wr_delta, ntxp_active;
	char addr_str[ETHER_ADDR_STR_LEN];
	uint8 tid_ac;

	/* loop through nodes, weighted ordered queue implementation */
	prio = dll_head_p(&pciedev->active_prioring_list);

	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		/* List of Priorities */
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);

		while (!dll_end(cur, &prioring->active_flowring_list)) {
			/* List of flowrings in the given priority */
			msgbuf = ((flowring_pool_t *)cur)->ring;

			/* Check for matching cfp_flowid */
			if ((cfp_flowid != (uint16)SCB_ALL_FLOWS) &&
				(msgbuf->cfp_flowid != cfp_flowid)) {
				cur = dll_next_p(cur);
				continue;
			}

			/* print the read/write pointers */
			bcm_ether_ntoa((struct ether_addr *)msgbuf->flow_info.da, addr_str);
			old_wr = MSGBUF_WR_PEEKED(msgbuf);
			new_wr = MSGBUF_WR(msgbuf);
			wr_delta = READ_AVAIL_SPACE(new_wr, old_wr, MSGBUF_MAX(msgbuf));
			ntxp_active = NTXPACTIVE((msgbuf)->fetch_ptr, new_wr, MSGBUF_MAX(msgbuf));

			tid_ac = PCIEDEV_TID_REMAP(pciedev, msgbuf);

			printf("da<%s> old_wr<%u> new_wr<%u> delta<%u> active<%u> fetch_ptr<%u>\n",
				addr_str, old_wr, new_wr, wr_delta, ntxp_active, msgbuf->fetch_ptr);
			printf("   tid<%u> vpkts<%u> v2r<%u>\n",
				tid_ac,
				wlc_sqs_vpkts(msgbuf->cfp_flowid, tid_ac),
				wlc_sqs_v2r_pkts(msgbuf->cfp_flowid, tid_ac));

			/* next node */
			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}

	return BCME_OK;
}
#endif /* WLSQS */

/*
 * PCIE IPC Dongle Host Training Phases:
 *
 * All new bus layer features/support must abide by these phases.
 *
 * "SETUP Phase" : Dongle allocates and initialize all shared structures
 *   pciedev_attach()
 *   +  pciedev_setup_common_rings() setup all common rings
 *      +   pciedev_setup_msgbuf_lpool() per common ring, attach lcl_buf_pool
 *   +  pciedev_setup_flowrings() setup all flowrings, pool, fetch_rqsts
 *   +  pciedev_setup_pcie_ipc() setup the PCIE IPC shared objects
 *
 *
 * "BIND  Phase" : Bind bus layer subsystems to the PCIE IPC dongle side
 *   pciedev_attach()
 *   +  pciedev_bind_pcie_ipc()                bind pciedev bus layer
 *      +   pciedev_h2d_msgbuf_bind_pcie_ipc() bind H2D common rings
 *      +   pciedev_d2h_msgbuf_bind_pcie_ipc() bind D2H common rings
 *      +/- pciedev_dmaindex_bind_pcie_ipc()   bind dmaindex subsystem
 *      +/- pciedev_idma_bind_pcie_ipc()       bind implicit DMA subsystem
 *      +   hme_bind_pcie_ipc()                bind HostMemoryExtn subsystem
 *
 * Dongle to Host training and Host wakes dongle
 * + Host will now setup its side, and wake dongle,
 * + On dongle resumption, the bus layer will enter the LINK phase.
 *
 *
 * "LINK  Phase" : Link bus layer subsystems to the PCIE_IPC host side
 *   +  pciedev_link_pcie_ipc()                link pciedev bus layer
 *      +   hme_link_pcie_ipc()                link HostMemoryExtn subsystem
 *      +/- pciedev_dmaindex_link_pcie_ipc()   link dmaindex subsystem
 *      +/- pciedev_idma_link_pcie_ipc()       link implicit DMA subsystem
 *      +   pciedev_msgbuf_link_pcie_ipc       link common rings (+lcl buffers)
 *      +   pciedev_flowrings_link_pcie_ipc    link flowrings (+cir buffers)
 *
 *  NOTE: When flowrings are created, they explicitly invoke
 *		+   pciedev_h2d_msgbuf_bind_pcie_ipc() bind flowring
 *      +   pciedev_flowring_sync_pcie_ipc()   sync pcie_ipc from create message
 */

/** Common rings setup/cleanup (also used for support info/debug rings) */
static int  BCMATTACHFN(pciedev_setup_common_rings)(struct dngl_bus *pciedev);
static void BCMATTACHFN(pciedev_cleanup_common_rings)(struct dngl_bus *pciedev);
/** Helpers to setup/cleanup lcl_buf_pools for msgbuf (common and info rings) */
static int BCMATTACHFN(pciedev_setup_msgbuf_lpool)(struct dngl_bus *pciedev,
	msgbuf_ring_t *ring, const char *name, uint16 ringid,
	uint8 bufcnt, uint8 item_cnt, uint16 fetch_limit,
	bool dmaindex_d2h_supported, bool dmaindex_h2d_supported, bool inusepool);
static void pciedev_cleanup_msgbuf_lpool(struct dngl_bus *pciedev,
	msgbuf_ring_t *ring);

/** TxPost flowrings setup/cleanup */
static int  BCMATTACHFN(pciedev_setup_flowrings)(struct dngl_bus *pciedev);
static void BCMATTACHFN(pciedev_cleanup_flowrings)(struct dngl_bus *pciedev);
/** Helpers to setup/cleanup cir_buf_pool for TxPost flowrings */
static msgbuf_ring_t *pciedev_allocate_flowring(struct dngl_bus *pciedev);
static cir_buf_pool_t *pciedev_setup_flowrings_cpool(struct dngl_bus *pciedev,
	uint16 item_cnt, uint8 items_size);
static void pciedev_cleanup_flowrings_cpool(struct dngl_bus *pciedev,
	cir_buf_pool_t *cir_buf_pool);

/* Setup PCIE IPC shared structures and data fill dongle side info */
static int BCMATTACHFN(pciedev_setup_pcie_ipc)(struct dngl_bus *pciedev);

/* Bind bus layer to the PCIe IPC dongle side shared regions */
static int BCMATTACHFN(pciedev_bind_pcie_ipc)(struct dngl_bus *pciedev);

/* Bind bus layer msgbuf_ring to the PCIe IPC dongle side shared regions */
static int pciedev_h2d_msgbuf_bind_pcie_ipc(struct dngl_bus *pciedev,
	uint16 ringid, msgbuf_ring_t *ring, bool is_common_ring);
static int pciedev_d2h_msgbuf_bind_pcie_ipc(struct dngl_bus *pciedev,
	uint16 ringid, msgbuf_ring_t *ring, bool is_common_ring);

/* Link bus layer to the PCIE IPC host side shared regions */
static int pciedev_link_pcie_ipc(struct dngl_bus *pciedev);

/* Link bus layer msgbuf_ring to the PCIE IPC host side shared regions */
static int pciedev_msgbuf_link_pcie_ipc(struct dngl_bus *pciedev,
	msgbuf_ring_t *ring, msgring_process_message_t process_rtn);

/* Link flowrings to PCIE IPC host side shared regions */
static int pciedev_flowrings_link_pcie_ipc(struct dngl_bus *pciedev);

/* Sync the host side pcie_ipc_ring_mem with host side configuration received
 * in a flowring create message.
 */
static int pciedev_flowrings_sync_pcie_ipc(struct dngl_bus *pciedev,
	msgbuf_ring_t *flow_ring, uint16 flow_ring_id,
	haddr64_t haddr64, uint16 max_items, uint8 item_type, uint16 item_size);

/* Dump the PCIE IPC shared structures */
static int pciedev_dump_pcie_ipc(struct dngl_bus *pciedev);

/** Use mem2mem DMA to sync indices arrays to/from host */
#ifdef PCIE_DMA_INDEX
/* Setup the bus layer dmaindex */
static int  BCMATTACHFN(pciedev_setup_dmaindex)(struct dngl_bus *pciedev);
static void BCMATTACHFN(pciedev_cleanup_dmaindex)(struct dngl_bus *pciedev);
/** Bind the bus layer dmaindex to the PCIE IPC dongle side */
static int  BCMATTACHFN(pciedev_dmaindex_bind_pcie_ipc)(struct dngl_bus *pciedev);
/** Link the bus layer dmaindex to the PCIE IPC host side */
static int pciedev_dmaindex_link_pcie_ipc(struct dngl_bus *pciedev);
#ifdef PCIE_DMA_INDEX_DEBUG /* Log dmaindex transaction */
#define WL_PCIE_DMAINDEX_LOG(args) pciedev_dmaindex_log args
static void pciedev_dmaindex_log(struct dngl_bus *pciedev,
	bool d2h, bool is_h2d_dir, dmaindex_t *dmaindex);
static int pciedev_dmaindex_log_dump(struct dngl_bus * pciedev, int option);
#else
#define WL_PCIE_DMAINDEX_LOG(args) do {} while (0)
#endif /* PCIE_DMA_INDEX_DEBUG */
/** RTE Host2Dongle Fetch request callbacks for H2D-WR and D2H-RD index sync */
static void pciedev_dmaindex_h2d_fetch_cb(struct fetch_rqst *fr, bool cancelled);
static void pciedev_dmaindex_d2h_fetch_cb(struct fetch_rqst *fr, bool cancelled);
#endif /* PCIE_DMA_INDEX */

/** Use Implicit DMA HW to transfer a subset of the DMA indices to dongle */
#ifdef BCMPCIE_IDMA
static int  BCMATTACHFN(pciedev_setup_idma)(struct dngl_bus *pciedev);
static void BCMATTACHFN(pciedev_cleanup_idma)(struct dngl_bus *pciedev);
/** Bind the Bus layer iDMA to the PCIE IPC dongle side */
static int  BCMATTACHFN(pciedev_idma_bind_pcie_ipc)(struct dngl_bus *pciedev);
/** Link the Bus layer iDMA to the PCIE IPC host side */
static int  pciedev_idma_link_pcie_ipc(struct dngl_bus *pciedev);
#endif /* BCMPCIE_IDMA */

/* Logs added for IPC events (DB/MB) */
#ifdef DBG_IPC_LOGS
static void pciedev_ipc_log(struct dngl_bus * pciedev, ipc_event_t event_type,
	uint8 stype, uint32 timestamp);
#define IPC_LOG(pciedev, event_type, stype, ts) \
	pciedev_ipc_log((pciedev), (event_type), (stype), (ts))
#else
#define IPC_LOG(pciedev, event_type, stype, ts)
#endif // endif

#if defined(PCIE_D2H_DOORBELL_RINGER)
static void pciedev_d2h_ring_config_soft_doorbell(struct dngl_bus *pciedev,
	const ring_config_req_t *ring_config_req);
static void pciedev_send_d2h_soft_doorbell_resp(struct dngl_bus *pciedev,
	const ring_config_req_t *ring_config_req, int16 status);
#endif /* PCIE_D2H_DOORBELL_RINGER */

#ifdef BCMPCIE_D2H_MSI
static void pciedev_d2h_ring_config_msi(struct dngl_bus *pciedev,
	const ring_config_req_t *ring_config_req);
static void pciedev_send_d2h_ring_config_resp(struct dngl_bus *pciedev,
	const ring_config_req_t *ring_config_req, int16 status);
#endif /* BCMPCIE_D2H_MSI */

static void pciedev_process_d2h_ring_config(struct dngl_bus * pciedev, void *p);

static int pciedev_process_flow_ring_create_rqst(struct dngl_bus * pciedev, void *p);
static int pciedev_flush_flow_ring(struct dngl_bus * pciedev, uint16 flowid);
static int pciedev_process_flow_ring_delete_rqst(struct dngl_bus * pciedev, void *p);
static void pciedev_process_flow_ring_flush_rqst(struct dngl_bus * pciedev, void *p);
static void pciedev_free_msgbuf_flowring(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring);
static void
pciedev_free_flowring_allocated_info(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring);
static uint8 pciedev_dump_lclbuf_pool(msgbuf_ring_t *ring);
static uint16 pciedev_handle_h2d_msg_ctrlsubmit(struct dngl_bus *pciedev, void *p,
	uint16 pktlen, msgbuf_ring_t *ring);

#ifdef HMAPTEST
static int
pciedev_do_arm_hmaptest(struct dngl_bus *pciedev, haddr64_t haddr64, int32 is_write, uint32 len);
static int
pciedev_do_m2m_hmaptest(struct dngl_bus *pciedev, haddr64_t haddr64, int32 is_write, uint32 len);
static void pciedev_hmaptest_m2mwrite_do_dmaxfer(struct dngl_bus *pciedev, void * p,
	uint32 haddr_hi, uint32 haddr_lo);
static void pciedev_hmaptest_m2mread_dmaxfer_done(struct fetch_rqst *fr, bool cancelled);
static void pciedev_hmaptest_m2mread_do_dmaxfer(struct dngl_bus *pciedev, void * p,
	uint32 haddr_hi, uint32 haddr_lo);
static int
pciedev_process_hmaptest_msg(struct dngl_bus *pciedev, char *buf, int offset,
	bool set, uint32 *outlen);
static int
pciedev_process_hmap_msg(struct dngl_bus *pciedev, char * buf, int offset,
	bool set, uint32 *outlen);
#endif /* HMAPTEST */
/* Below functions are for generic HMAP functionality and not only for HMAPTEST
 * HMAP intr handling will be required without HMAPTEST also.
 */
static bool pciedev_hmap_intr_process(struct dngl_bus *pciedev);

#ifdef PCIE_DMAXFER_LOOPBACK
static uint32 pciedev_process_do_dest_lpbk_dmaxfer(struct dngl_bus *pciedev);
static void pciedev_process_src_lpbk_dmaxfer_done(struct fetch_rqst *fr, bool cancelled);
static uint32 pciedev_process_do_src_lpbk_dmaxfer(struct dngl_bus *pciedev);
static uint32 pciedev_schedule_src_lpbk_dmaxfer(struct dngl_bus *pciedev);
static void pciedev_done_lpbk_dmaxfer(struct dngl_bus *pciedev);
static uint32 pciedev_prepare_lpbk_dmaxfer(struct dngl_bus *pciedev, uint32 len);
static void pciedev_process_lpbk_dmaxfer_msg(struct dngl_bus *pciedev, void *p);
static int pciedev_send_lpbkdmaxfer_complete(struct dngl_bus *pciedev,
	uint16 status, uint32 req_id);
static void pciedev_lpbk_src_dmaxfer_timerfn(dngl_timer_t *t);
static void pciedev_lpbk_dest_dmaxfer_timerfn(dngl_timer_t *t);
static int pciedev_d11_dma_lpbk_init(struct dngl_bus *pciedev);
static int pciedev_d11_dma_lpbk_uninit(struct dngl_bus *pciedev);
static int pciedev_d11_dma_lpbk_run(struct dngl_bus *pciedev, uint8* buf, int len);
static uint32 pciedev_process_lpbk_dma_txq_pend_pkt(struct dngl_bus *pciedev);
static uint32 pciedev_fetchrq_lpbk_dma_txq_pkt(struct dngl_bus *pciedev);
#endif /* PCIE_DMAXFER_LOOPBACK */

static void pciedev_handle_host_deepsleep_mbdata(struct dngl_bus *pciedev, uint32 h2d_mb_data);
static int pciedev_dump_flow_ring(msgbuf_ring_t *ring);
static int pciedev_dump_prio_rings(struct dngl_bus * pciedev);
static int pciedev_dump_rw_indices(struct dngl_bus * pciedev);
static void pciedev_init_inuse_lclbuf_pool_t(struct dngl_bus *pciedev, msgbuf_ring_t *ring);

static void pciedev_free_inuse_bufpool(struct dngl_bus *pciedev, lcl_buf_pool_t * bufpool);
static void pciedev_bm_enab_timerfn(dngl_timer_t *t);
static void pciedev_host_sleep_ack_timerfn(dngl_timer_t *t);

static void pcie_init_mrrs(struct dngl_bus *pciedev, uint32 mrrs);
static int pciedev_tx_ioctl_pyld(struct dngl_bus *pciedev, void *p, ret_buf_t *data_buf,
	uint16 data_len, uint8 msgtype);
static int pciedev_dump_err_cntrs(struct dngl_bus *pciedev);
static pciedev_ctrl_resp_q_t * pciedev_setup_ctrl_resp_q(struct dngl_bus *pciedev,
	uint8 item_cnt);
static void pciedev_ctrl_resp_q_idx_upd(struct dngl_bus *pcidev, uint8 idx_type);
static void pciedev_flow_get_opt_count(struct dngl_bus * pciedev);
#ifdef HEALTH_CHECK
static uint32 pciedev_read_clear_aer_uc_error_status(struct dngl_bus *pciedev);
static uint32 pciedev_read_clear_aer_corr_error_status(struct dngl_bus *pciedev);
static uint32 pciedev_read_clear_tlcntrl_5_status(struct dngl_bus *pciedev);
static void pciedev_dump_aer_hdr_log(struct dngl_bus *pciedev,
	uint32 *config_regs, uint32 size, int *count);
static int pciedev_healthcheck_fn(uint8 *buffer, uint16 length, void *context,
		int16 *bytes_written);
static bool pciedev_d2h_dma_taking_too_long(struct dngl_bus *pciedev, uint32 now, uint32 *rw_ind);
static bool pciedev_h2d_dma_taking_too_long(struct dngl_bus *pciedev, uint32 now, uint32 *rw_ind);
static bool pciedev_check_link_down(struct dngl_bus *pciedev);
static bool pciedev_d3ack_taking_too_long(struct dngl_bus *pciedev, uint32 now);
static bool pciedev_ioctl_taking_too_long(struct dngl_bus *pciedev, uint32 now);
static bool pciedev_check_device_AER(struct dngl_bus *pciedev, uint32 *config_regs, uint32 size);
static bool pciedev_msi_intstatus_asserted(struct dngl_bus *pciedev);
static void pciedev_dma_stall_reg_collect(struct dngl_bus *pciedev,
	bool h2d, uint32 *pcie_config_regs, int size);
static int pciedev_chip_not_in_ds_too_long(void *arg,
	uint32 now, bool hnd_ds_hc, uint32 *reg, uint32 size,
	uint32 ds_check_lo_thr, uint32 ds_check_hi_thr);
static void pciedev_ds_healthcheck_cntr_update(struct dngl_bus *pciedev,
	uint32 time_now, bool reset);
static void pciedev_ds_hc_trigger_timerfn(dngl_timer_t *t);
#else
#define	pciedev_ds_healthcheck_cntr_update(pciedev, time_now, reset) {}
#endif /* HEALTH_CHECK */

static void pciedev_set_T_REF_UP(struct dngl_bus *pciedev, uint32 trefup);
static void pciedev_handle_hostready_intr(struct dngl_bus *pciedev);

static void
pciedev_mdio_write(struct dngl_bus *pciedev, int blk_addr, int addr, int data, uint32 mdiv);

#ifdef DUMP_PCIEREGS_ON_TRAP
static uint32 BCMATTACHFN(pciedev_dump_coe_list_array_size)(struct dngl_bus *pciedev);
#endif  /* DUMP_PCIEREGS_ON_TRAP */
static int pciedev_gpio_signal_test(struct dngl_bus *pciedev,
	uint8 gpio_num, bool set, uint32 *pval);

#define MDIOCTL2_DIVISOR_VAL_3			3
#define MDIOCTL2_DIVISOR_VAL_8			8
#define MDIO_SERDES_BLKADDR			0x1f
#define MDIO_DELAY_1000US			1000

#define PCIE_PMCR_REFUP_T_DS_ENTER		28
#define PCIE_PMCR_REFUP_T_DS_EXIT		8
#define PCIE_PMCR_REFUP_T_DS_ENTER_REV19	100
#define PCIE_PMCR_REFUP_T_DS_EXIT_REV19		30

/*
 * chip default value for TrefUp (pcie ep reg 0x1814 and 0x1818) for corerev23
 * TrefUp default time is 1.2 us
 */
#define PCI_PMCR_REFUP_DEFAULT_REV23		0x9e051812
#define PCI_PMCR_REFUP_EXT_DEFAULT_REV23	0x00208a14

#define PCIE_FASTLPO_ENAB(pciedev) \
	(FASTLPO_ENAB() && (((pciedev)->fastlpo_pcie_en) && ((pciedev)->fastlpo_pmu_en)))

extern bcm_rxcplid_list_t *g_rxcplid_list;
static uint32 pciedev_reorder_flush_cnt = 0;

typedef struct lpbk_ctx {
	struct fetch_rqst fr;
	void *p;
} pciedev_lpbk_ctx_t;

uint32 pcie_dump_registers_address[] = {
	PCI_CFG_CMD, PCI_CFG_STAT, PCI_ADV_ERR_CAP,
	PCI_UC_ERR_STATUS, PCI_UNCORR_ERR_MASK, PCI_UCORR_ERR_SEVR, PCI_CORR_ERR_STATUS,
	PCI_CORR_ERR_MASK, PCI_TL_HDR_FC_ST, PCI_TL_TGT_CRDT_ST, PCI_TL_SMLOGIC_ST, PCI_PHY_CTL_0,
	PCI_LINK_STATE_DEBUG, PCI_RECOVERY_HIST, PCI_PHY_LTSSM_HIST_0, PCI_PHY_LTSSM_HIST_1,
	PCI_PHY_LTSSM_HIST_2, PCI_PHY_LTSSM_HIST_3, PCI_PHY_DBG_CLKREG_0, PCI_PHY_DBG_CLKREG_1,
	PCI_PHY_DBG_CLKREG_2, PCI_PHY_DBG_CLKREG_3, PCI_TL_CTRL_5, PCI_DL_ATTN_VEC, PCI_DL_STATUS};

/** Accessor functions */
static uint32 BCMRAMFN(pciedev_pcie_dump_registers_address)(uint8 idx)
{
	return pcie_dump_registers_address[idx];
}

#ifdef UART_TRAP_DBG
#define H2D0_DMAREGS_TX_BASE_ADDRESS	(SI_ENUM_BASE(NULL) + 0x3200)
#define H2D0_DMAREGS_RX_BASE_ADDRESS	(SI_ENUM_BASE(NULL) + 0x3220)
#define D2H0_DMAREGS_TX_BASE_ADDRESS	(SI_ENUM_BASE(NULL) + 0x3240)
#define D2H0_DMAREGS_RX_BASE_ADDRESS	(SI_ENUM_BASE(NULL) + 0x3260)

#define NUM_OF_ADDRESSES_PER_DMA_DUMP	6

uint32 dma_registers_address[] = {
	H2D0_DMAREGS_TX_BASE_ADDRESS, H2D0_DMAREGS_RX_BASE_ADDRESS,
	D2H0_DMAREGS_TX_BASE_ADDRESS, D2H0_DMAREGS_RX_BASE_ADDRESS };

uint32 pcie_common_registers[] = {
	OFFSETOF(chipcregs_t, pmustatus), OFFSETOF(chipcregs_t, pmucontrol),
	OFFSETOF(chipcregs_t, res_state), OFFSETOF(chipcregs_t, min_res_mask)};

static uint32 BCMRAMFN(pciedev_dma_registers_address)(uint8 idx)
{
	return dma_registers_address[idx];
}

static uint32 BCMRAMFN(pciedev_pcie_common_registers)(uint8 idx)
{
	return pcie_common_registers[idx];
}

#endif /* UART_TRAP_DBG */

#ifdef PCIE_DUMP_COMMON_RING_DEBUG
static void /* Only for common rings ! */
BCMATTACHFN(pciedev_dump_common_ring)(msgbuf_ring_t *ring)
{
	uint8 i;
	uint16 ringid = ring->ringid;
	lcl_buf_pool_t *pool = ring->buf_pool;

	PCI_TRACE(("ring%u %c%c%c%c: phase %d, (ptr,val) rd (%p,%u) wr (%p,%u)\n",
		ringid, ring->name[0], ring->name[1], ring->name[2],
		ring->name[3], ring->current_phase,
		MSGBUF_RD_PTR(ring), MSGBUF_RD(ring), MSGBUF_WR_PTR(ring), MSGBUF_WR(ring)));
	PCI_TRACE(("\tring%u pcie_ipc_ring_mem 0x%08" HADDR64_FMT "\n", ringid,
		MSGBUF_IPC_RING_MEM(ring), HADDR64_VAL(MSGBUF_HADDR64(ring))));
	PCI_TRACE(("\tring%u pool buf_cnt %d availcnt %d\n",
		ringid, pool->buf_cnt, pool->availcnt));
	for (i = 0; i < pool->buf_cnt; i++) {
		PCI_TRACE(("\tring%u buf%d:  %x\n", ringid, i, pool->buf[i].p));
	}
}
#endif /* PCIE_DUMP_COMMON_RING_DEBUG */

static void
BCMATTACHFN(pciedev_dump_hostbuf_pool)(const char *name, host_dma_buf_pool_t *pool)
{
	uint32 i = 0;
	host_dma_buf_t *hdma_free;

	PCI_TRACE(("Pool name %c%c%c%c pool max: %d,  free 0x%x, ready %x\n",
		name[0], name[1], name[2], name[3], pool->max,
		(uint32)pool->free, (uint32)pool->ready));
	hdma_free = pool->free;
	while (hdma_free) {
		i++;
		PCI_TRACE(("free[%d] cur:%x\n", i, (uint32) hdma_free));
		hdma_free = hdma_free->next;
	}
	hdma_free = pool->ready;
	i = 0;
	while (hdma_free) {
		i++;
		PCI_TRACE(("ready[%d] cur: %x\n", i, (uint32) hdma_free));
		hdma_free = hdma_free->next;
	}
}

uint32
pciedev_indirect_access(struct dngl_bus *pciedev, uint32 addr, uint32 data, uint8 access_type)
{
	uint32 ret = 0;

	W_REG(pciedev->osh, PCIE_configindaddr(pciedev->autoregs), addr);
	switch (access_type) {
		case IND_ACC_RD:
			break;
		case IND_ACC_WR:
			W_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs), data);
			break;
		case IND_ACC_AND:
			ret = data & R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs));
			W_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs), ret);
			break;
		case IND_ACC_OR:
			ret = data | R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs));
			W_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs), ret);
			break;
		default:
			PCI_ERROR(("invalid access type : %d\n", access_type));
			ASSERT(0);
			break;
	}

	ret = R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs));
	return ret;
}

#define PCIEDEV_PWRREQ_CLEAR(pciedev) \
	AND_REG(pciedev->osh, &pciedev->regs->u.pcie2.powerctl, \
		~(SRPWR_DMN0_PCIE_MASK << SRPWR_REQON_SHIFT))

/** power save related */
static void
pciedev_set_LTRvals(struct dngl_bus *pciedev)
{
	/* LTR0 */
	W_REG_CFG(pciedev, PCIE_LTR0_REG_OFFSET, pciedev->ltr_info.ltr0_regval);
	/* LTR1 */
	W_REG_CFG(pciedev, PCIE_LTR1_REG_OFFSET, pciedev->ltr_info.ltr1_regval);
	/* LTR2 */
	W_REG_CFG(pciedev, PCIE_LTR2_REG_OFFSET, pciedev->ltr_info.ltr2_regval);
}

/** power save related WAR */
static int
pciedev_hw_LTR_war(struct dngl_bus *pciedev)
{
	uint32 devstsctr2;

	if (PCIECOREREV(pciedev->corerev) > 13) {
		/* set the LTR state to be active */
		pciedev_send_ltr(pciedev, LTR_ACTIVE);
		OSL_DELAY(1000);

		/* set the LTR state to be sleep */
		pciedev_send_ltr(pciedev, LTR_SLEEP);
		OSL_DELAY(1000);
		return 1;
	}

	/* XXX WAR for CRWLPCIEGEN2-159 (Send LTR request may not propagate and trigger LTR message)
	 * HW JIRA: HW4345-846, make sure the LTR values are properly set
	 */
	devstsctr2 = R_REG_CFG(pciedev, PCIEGEN2_CAP_DEVSTSCTRL2_OFFSET);
	if (devstsctr2 & PCIEGEN2_CAP_DEVSTSCTRL2_LTRENAB) {
		PCI_TRACE(("add the work around for loading the LTR values, link state 0x%04x\n",
			R_REG(pciedev->osh, PCIE_pcieiostatus(pciedev->autoregs))));

		/* force the right LTR values ..same as JIRA 859 */
		pciedev_set_LTRvals(pciedev);

		si_core_wrapperreg(pciedev->sih, 3, 0x60, 0x8080, 0);

		/* Enable the LTR */
		W_REG_CFG(pciedev, PCIEGEN2_CAP_DEVSTSCTRL2_OFFSET, devstsctr2);

		/* set the LTR state to be active */
		pciedev_send_ltr(pciedev, LTR_ACTIVE);
		OSL_DELAY(1000);

		/* set the LTR state to be sleep */
		pciedev_send_ltr(pciedev, LTR_SLEEP);
		OSL_DELAY(1000);
		return 1;
	} else {
		PCI_TRACE(("no work around for loading the LTR values\n"));
		return 0;
	}
}

/** Global function for host notification */
static struct dngl_bus *dongle_bus = NULL;

static void
pciedev_fwhalt(void *arg, uint32 trap_data)
{
	if (dongle_bus) {
		dongle_bus->bus_counters->hostwake_reason = PCIE_HOSTWAKE_REASON_TRAP;
		pciedev_host_wake_gpio_enable(dongle_bus, TRUE);
		OSL_DELAY(20000);
		/* Make sure the bus is accessible */
		if (pciedev_ds_in_host_sleep(dongle_bus)) {
			SPINWAIT(!(R_REG(dongle_bus->osh,
				PCIE_pciecontrol(dongle_bus->autoregs)) & PCIE_RST),
				PCIEDEV_MAXWAIT_TRAP_HANDLER);
			if (R_REG(dongle_bus->osh,
				PCIE_pciecontrol(dongle_bus->autoregs)) & PCIE_RST) {
				PCI_TRACE(("PERST_L de-asserted\n"));
				/* XXX : Perst indication in pcie control is raw but there are
				* delays after that which keeps reset to COE core asserted
				* ie (time to run the sequencer + fixed delay in core core
				*/
				/* Add some delay before accessing the config space */
				OSL_DELAY(10000);
				/* XXX: CRWLPCIEGEN2-223:
				 * Can't Access COE space when PERST is asserted
				 */
				if ((PCIECOREREV(dongle_bus->corerev) < 15) ||
					(PCIECOREREV(dongle_bus->corerev) == 16) ||
					(PCIECOREREV(dongle_bus->corerev) == 17) ||
					(PCIECOREREV(dongle_bus->corerev) == 21)) {
					/* Reset the PCIE core here as the AXI
					* timeout during D3cold does
					* not fully recover the PCIE core.
					*/
					si_core_reset(dongle_bus->sih, 0, 0);
					if (!si_iscoreup(dongle_bus->sih)) {
						dongle_bus->core_reset_failed_in_trap = TRUE;
						PCI_ERROR(("reset_process: failed to"
							"bring up PCIE core \n"));
						return;
					}
					/* Reprogram the mailboxintmask
					* as PCIE core reset clears it.
					* This is needed for the DB/MSI
					* to be propagated to the host.
					*/
					if (dongle_bus->mailboxintmask) {
						W_REG(dongle_bus->osh,
							PCIE_mailboxintmask(dongle_bus->autoregs),
							dongle_bus->mailboxintmask);
					}
				}

				/* PERST_L is not asserted anymore. Now check for BM enable */
				W_REG(dongle_bus->osh, PCIE_configindaddr(dongle_bus->autoregs),
					PCI_CFG_CMD);
				SPINWAIT(!(R_REG(dongle_bus->osh,
					PCIE_configinddata(dongle_bus->autoregs)) & PCI_CMD_MASTER),
					PCIEDEV_MAXWAIT_TRAP_HANDLER);
				if (!(R_REG(dongle_bus->osh,
					PCIE_configinddata(dongle_bus->autoregs))
					& PCI_CMD_MASTER)) {
					dongle_bus->disallow_write_trapdata = TRUE;
					PCI_ERROR(("Cannot write trap data to host memory."
						"Bus unaccessible\n"));
					return;
				}
			}
			else {
				PCI_ERROR(("PERST_L is not de-asserted. giving up...\n"));
				return;
			}
		}

		pciedev_d2h_mbdata_send(dongle_bus,
			PCIE_IPC_D2HMB_DEV_FWHALT | trap_data);
	}
}

/** Called on receiving 'rx buf post' message from the host */
static int
pciedev_host_dma_buf_pool_add(struct dngl_bus *pciedev, host_dma_buf_pool_t *pool,
	haddr64_t *addr, uint32 pktid, uint16 len)
{
	host_dma_buf_t *hdma_free;

	if (pool->free == NULL) {
		PCI_ERROR(("Error adding a host buffer to pool\n"));
		return BCME_NOMEM;
	}

	/* Check if host has posted more than FW can handle */
	if (pool->ready_cnt >= pool->max) {
		PCI_ERROR(("Error: Host posted more than max. buffers \n"));
		return BCME_ERROR;
	}

	hdma_free = pool->free;
	pool->free = hdma_free->next;

	hdma_free->buf_addr.low_addr = addr->low_addr;
	hdma_free->buf_addr.high_addr = addr->high_addr;

	BCMPCIE_IPC_HPA_TEST(pciedev, pktid,
		BCMPCIE_IPC_PATH_CONTROL, BCMPCIE_IPC_TRANS_REQUEST);

	hdma_free->pktid = pktid;
	hdma_free->len = len;
	hdma_free->next = pool->ready;

	pool->ready = hdma_free;
	pool->ready_cnt++;

	PCI_INFORM(("pktid 0x%08x, len %d, buffer (0x%08x:0x%08x)\n", pktid,
		len, hdma_free->buf_addr.high_addr, hdma_free->buf_addr.low_addr));
	return BCME_OK;
}

/** Called on sending 'iotcl complete' or 'event' message to the host */
static bool
pciedev_host_dma_buf_pool_get(host_dma_buf_pool_t *pool, host_dma_buf_t *ready)
{
	host_dma_buf_t *local;

	PCI_TRACE(("pciedev_host_dma_buf_pool_get:"));
	PCI_TRACE(("pool %x, ready is %x\n", (uint32)pool, (uint32)pool->ready));
	if (pool->ready == NULL) {
		return FALSE;
	}

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
	host_dma_buf_t *hdma_free;
	uint32 alloc_size = 0;
	uint i;

	if (max == 0) {
		PCI_TRACE(("pciedev_init_host_dma_buf_pool_init (no. of pool:%d)\n", max));
		return NULL;
	}

	alloc_size = sizeof(host_dma_buf_pool_t) + (max * sizeof(host_dma_buf_t));

	pool = (host_dma_buf_pool_t *)MALLOCZ(pciedev->osh, alloc_size);
	if (pool == NULL) {
		PCI_ERROR(("pciedev_init_host_dma_buf_pool_init: malloc failed\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return NULL;
	}

	pool->max = max;
	/* set up descs */
	hdma_free = (host_dma_buf_t *)&pool->pool_array[0];

	pool->ready = NULL;
	pool->free = hdma_free;

	for (i = 0; i < (pool->max - 1); i++) {
		hdma_free->next = (hdma_free + 1);
		hdma_free = hdma_free+1;
	}

	hdma_free->next = NULL;

	return pool;
}

static dma_queue_t*
pciedev_dma_queue_init(struct dngl_bus *pciedev, uint16 max_q_len, uint8 chan)
{
	uint16 size = 0;
	dma_queue_t *dmaq;

	/* return null if max_q_len is 0 */
	if (!max_q_len) {
		PCI_ERROR(("pciedev_dma_queue_init: max_q_len is zero\n"));
		return NULL;
	}

	/* allocate the dma_queue_t along with max_q_len of dma_item_info_t */
	size = sizeof(dma_queue_t) + (max_q_len * sizeof(dma_item_info_t));
	dmaq = MALLOCZ(pciedev->osh, size);
	if (dmaq == NULL) {
		PCI_ERROR(("pciedev_dma_queue_init: failed to allocate %d bytes\n", size));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return NULL;
	}

	/* initialize the queue */
	dmaq->max_len = max_q_len;
	dmaq->r_index = dmaq->w_index = 0;

	/* save the dma channel number */
	dmaq->chan = chan;

	return dmaq;
}

/** Called on receiving a 'buffer post' message from the host for IOCTL or event */
static int
pciedev_rxbufpost_msg(struct dngl_bus *pciedev, void *p, host_dma_buf_pool_t *pool)
{
	ioctl_resp_evt_buf_post_msg_t *post;
	int ret_val;

	post = (ioctl_resp_evt_buf_post_msg_t *)p;

	/* Check the case where the host could be posting more than we could hold */
	/* This check is in pciedev_host_dma_buf_pool_add */
	ret_val = pciedev_host_dma_buf_pool_add(pciedev, pool, &post->host_buf_addr,
		post->cmn_hdr.request_id, post->host_buf_len);

	return (ret_val == BCME_OK) ? TRUE : FALSE;
}

/** AMPDU reordering on the PCIe bus layer. Called back via firmware AMPDU RX subsystem */
void
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

static void
pciedev_mdio_write(struct dngl_bus *pciedev, int blk_addr, int addr, int data, uint32 mdiv)
{
	PCI_TRACE(("pciedev_mdio_write: blk_addr=0x%x; addr=0x%x; data=0x%x; mdiv=0x%x\n",
			blk_addr, addr, data, mdiv));

	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.mdiocontrol,
		mdiv | (MDIO_SERDES_BLKADDR << MDIOCTL2_REGADDR_SHF));
	OSL_DELAY(MDIO_DELAY_1000US);
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.mdiowrdata,
		((blk_addr << MDIODATA2_DEVADDR_SHF) & ~MDIODATA2_DONE) | MDIODATA2_DONE);
	OSL_DELAY(MDIO_DELAY_1000US);

	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.mdiocontrol,
		mdiv | (addr << MDIOCTL2_REGADDR_SHF) | MDIOCTL2_SLAVE_BYPASS);
	OSL_DELAY(MDIO_DELAY_1000US);
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.mdiowrdata,
		(data & ~MDIODATA2_DONE) | MDIODATA2_DONE);
	OSL_DELAY(MDIO_DELAY_1000US);
}

static void
pciedev_linkloss_war(struct dngl_bus *pciedev)
{
	//Mdio write 0x210 0x4 0x88a0 : Set refclk pad hysteresis to 45mv
	pciedev_mdio_write(pciedev, 0x210, 4, 0x88a0, MDIOCTL2_DIVISOR_VAL_8);

	/* WAR: CRWLPCIEGEN2-499 : 1MHz refclk sense settings */
	if (PCIECOREREV(pciedev->corerev) == 23) {
		//Mdio write 0x160 0xc 0x50: Change N value of 1MHz clock
		pciedev_mdio_write(pciedev, 0x160, 0xc, 0x50, MDIOCTL2_DIVISOR_VAL_8);

		//Mdio write 0x150 0xc 0x2603: Increasing tolerance for 1MHz
		pciedev_mdio_write(pciedev, 0x150, 0xc, 0x2603, MDIOCTL2_DIVISOR_VAL_8);
	}

	/* WAR: Sig Det filter */
	if ((PCIECOREREV(pciedev->corerev) == 23) &&
		(pciedev->sig_det_filt_dis == FALSE)) {
		/* write filter offset */
		//Mdio write 0xf6 0x17 0xc0c8 : filter setting 10-20ns
		pciedev_mdio_write(pciedev, 0xf6, 0x17, 0xc0c8, MDIOCTL2_DIVISOR_VAL_8);

		/* enable filter */
		//Mdio write 0xf7 0x19 0x1000: enable filter
		pciedev_mdio_write(pciedev, 0xf7, 0x19, 0x1000, MDIOCTL2_DIVISOR_VAL_8);
	}
}

/* WAR:HW4347-746 dislable L0S */
static void pciedev_disable_l0s(struct dngl_bus *pciedev)
{
	AND_REG_CFG(pciedev, PCIECFGREG_LINK_STATUS_CTRL, (~PCIE_ASPM_L0s_ENAB));
}

static void
pciedev_trefup_init(struct dngl_bus *pciedev)
{
	uint32 mask_refup =
		(PCI_PMCR_TREFUP_LO_MASK << PCI_PMCR_TREFUP_LO_SHIFT) |
		(PCI_PMCR_TREFUP_HI_MASK << PCI_PMCR_TREFUP_HI_SHIFT);
	uint32 mask_refext =
		0x1 << PCI_PMCR_TREFUP_EXT_SHIFT;

	/* for corerev23, init TrefUp Time as chip default if 1M is enabled */
	AND_REG_CFG(pciedev, PCI_PMCR_REFUP, ~mask_refup);
	OR_REG_CFG(pciedev, PCI_PMCR_REFUP, PCI_PMCR_REFUP_DEFAULT_REV23 & mask_refup);
	PCI_TRACE(("Reg1:0x%x ::0x%x\n",
		PCI_PMCR_REFUP, R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs))));

	AND_REG_CFG(pciedev, PCI_PMCR_REFUP_EXT, ~mask_refext);
	OR_REG_CFG(pciedev, PCI_PMCR_REFUP_EXT,
		PCI_PMCR_REFUP_EXT_DEFAULT_REV23 & mask_refext);
	PCI_TRACE(("Reg3:0x%x ::0x%x\n",
		PCI_PMCR_REFUP_EXT, R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs))));
}

static const char BCMATTACHDATA(rstr_PKTRXFRAGSZ_D)[] = "PKTRXFRAGSZ = %d\n";
static const char BCMATTACHDATA(rstr_eventpool)[]     = "eventpool";
static const char BCMATTACHDATA(rstr_ioctlresp)[]     = "ioctlresp";
static const char BCMATTACHDATA(rstr_sig_det_filt_dis)[] = "sig_det_filt_dis";
static const char BCMATTACHDATA(rstr_pcie_pipeiddq1_enab)[] = "pcie_pipeiddq1_enab";

/**
 * Core pciedev attach
 * 1. Intialize regs
 * 2. Initialize local circular buffers[d2h & h2d]
 * 3. Initialize PCIe IPC shared structure
 * 4. Silicon attach
 * 5. dma attach
 * 6. dongle attach
 * 7. timer attach
 */
struct dngl_bus *
BCMATTACHFN(pciedev_attach)(void *drv, uint vendor, uint device, osl_t *osh,
volatile void *regs, uint bustype)
{
	struct dngl_bus *pciedev;
	char *vars;
	uint vars_len;
	bool evnt_ctrl = 0;
	int i, n = 0;
	int pktq_depth = 0;
#ifdef PCIE_ERR_ATTN_CHECK
	uint32 err_code_state, err_type;
#endif // endif
	int max_d2h_queue_len = MAX_DMA_QUEUE_LEN_D2H;
#ifdef MEM_ALLOC_STATS
	memuse_info_t mu;
#endif /* MEM_ALLOC_STATS */

	PCI_TRACE(("%s\n", __FUNCTION__));
	PCI_TRACE((rstr_PKTRXFRAGSZ_D, MAXPKTRXFRAGSZ));

	/* Vendor match */
	if (!pciedev_match(vendor, device)) {
		PCI_TRACE(("%s: pciedev_match failed %d\n", __FUNCTION__, MALLOCED(osh)));
		return NULL;
	}

	/* Allocate bus device state info */
	if (!(pciedev = MALLOCZ(osh, sizeof(struct dngl_bus)))) {
		PCI_ERROR(("%s: out of memory\n", __FUNCTION__));
		return NULL;
	}

	pciedev->osh = osh;
	pciedev->regs = regs;

	dongle_bus = pciedev;

	/* Allocate error_cntrs structure */
	if (!(pciedev->err_cntr = MALLOCZ(osh, sizeof(pciedev_err_cntrs_t)))) {
		PCI_ERROR(("%s: out of memory\n", __FUNCTION__));
		goto fail;
	}

	/* Allocate autoregs structure */
	if (!(pciedev->autoregs = MALLOCZ(osh, sizeof(struct _autoreg)))) {
		PCI_ERROR(("%s: out of memory\n", __FUNCTION__));
		goto fail;
	}
	pciedev->autoregs->regs = regs;
#ifdef DBG_BUS
	if (!(pciedev->dbg_bus = MALLOCZ(osh, sizeof(pciedev_dbg_bus_cntrs_t)))) {
		PCI_ERROR(("%s: out of memory\n", __FUNCTION__));
		goto fail;
	}
#endif /* DBG_BUS */

	/* Allocate MAXTUNABLE tunables */
	if (!(pciedev->tunables = MALLOCZ(osh, (sizeof(uint32) * MAXTUNABLE)))) {
		MFREE(pciedev->osh, pciedev->err_cntr, sizeof(pciedev_err_cntrs_t));
		PCI_ERROR(("%s: out of memory for tunables\n", __FUNCTION__));
		goto fail;
	}

	/* Allocate pcie bus metrics */
	if (!(pciedev->metrics = (pcie_bus_metrics_t *) MALLOCZ(osh, sizeof(pcie_bus_metrics_t)))) {
		PCI_ERROR(("%s: out of memory to allocate metrics\n", __FUNCTION__));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail;
	}

	/* Allocate metric ref */
	if (!(pciedev->metric_ref = (metric_ref_t *) MALLOCZ(osh, sizeof(metric_ref_t)))) {
		PCI_ERROR(("%s: out of memory to allocate metric_ref\n", __FUNCTION__));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail;
	}

	/* Allocate ds_log for logging deep sleep information */
	if (!(pciedev->ds_log = (pciedev_deepsleep_log_t *) MALLOCZ(osh,
			PCIE_MAX_DS_LOG_ENTRY * sizeof(pciedev_deepsleep_log_t)))) {
		PCI_ERROR(("pciedev_attach: Failed to allocate ds_log\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail;
	}

	/* Initialize the struct for storing bus counters stats */
	if ((pciedev->bus_counters =
		MALLOCZ(pciedev->osh, sizeof(pcie_bus_counters_t))) == NULL) {
		PCI_ERROR(("malloc failed for bus counters!\n"));
		goto fail;
	}

#ifdef UART_TRANSPORT
	/* Allocate uart event inds mask */
	if (!(pciedev->uart_event_inds_mask = (uint8 *) MALLOCZ(osh,
			(sizeof(uint8) * WL_EVENTING_MASK_LEN)))) {
		PCI_ERROR(("pciedev_attach: out of memory to allocate uart_event_inds_mask\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail;
	}
#endif /* UART_TRANSPORT */

	/* Tunables init */
	pciedev_tunables_init(pciedev);

	/* Max Flowrings = (1 bcmc flowring per bss) + (N ucast flowrings),
	 * where N is
	 *     (no-SQS): 4 ac  x num_stations
	 *     (w/ SQS): 8 tid x num stations (see PCIE_IPC_DCAP1_FLOWRING_TID)
	 *
	 * See per chip roml/ram.mk files for PCIE_TXFLOWS
	 */
	pciedev->max_flowrings = pciedev->tunables[MAXTXFLOW];

	pciedev->max_h2d_rings = pciedev->max_flowrings +
		pciedev->max_dbg_rings + BCMPCIE_H2D_COMMON_MSGRINGS;

	pciedev->max_rxcpl_rings = 0; /* in addition to the default rxcpl */
	pciedev->max_d2h_rings = BCMPCIE_D2H_COMMON_MSGRINGS +
		pciedev->max_rxcpl_rings + pciedev->max_dbg_rings;

#ifdef BCMFRAGPOOL
	pciedev->pktpool_lfrag = SHARED_FRAG_POOL;	/* shared tx frag pool */
#endif /* BCMFRAGPOOL */
#if defined(BCM_DHDHDR) && !defined(BCM_DHDHDR_D3_DISABLE)
	pciedev->d3_lfbufpool = D3_LFRAG_BUF_POOL;
#endif /* BCM_DHDHDR */
#ifdef BCMRESVFRAGPOOL
	pciedev->pktpool_resv_lfrag = RESV_FRAG_POOL;	/* shared tx frag pool */
#endif /* BCMRESVFRAGPOOL */
#ifdef BCMRXFRAGPOOL
	pciedev->pktpool_rxlfrag = SHARED_RXFRAG_POOL;	/* shared rx frag pool */
#endif // endif
	pciedev->pktpool = SHARED_POOL;	/* shared lbuf pool */

	PCI_TRACE(("Ioctl Lock initialized...\n"));
	pciedev->ioctl_lock = FALSE;

#if defined(BCMPCIE_DMA_CH1) || defined(BCMPCIE_DMA_CH2)
	if (!(pciedev->dma_ch_info = (dma_ch_info_t *) MALLOCZ(osh, sizeof(dma_ch_info_t)))) {
		PCI_ERROR(("%s: out of memory to allocate dma_ch_info\n", __FUNCTION__));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail;
	}

#ifdef BCMPCIE_DMA_CH1
	pciedev->dma_ch1_enable = TRUE;
#endif /* BCMPCIE_DMA_CH1 */
#ifdef BCMPCIE_DMA_CH2
	pciedev->dma_ch2_enable = TRUE;
#endif /* BCMPCIE_DMA_CH2 */
#endif /* BCMPCIE_DMA_CH1 || BCMPCIE_DMA_CH1 */

	pciedev->default_dma_ch = LEGACY_DMA_CH;

	/* SETUP common msgbuf_ring support in the bus layer */
	if (pciedev_setup_common_rings(pciedev) != BCME_OK) {
		evnt_ctrl = PCIE_ERROR1;
		goto fail2;
	}

	/* SETUP flowring msgbuf_ring support in the bus layer */
	if (pciedev_setup_flowrings(pciedev) != BCME_OK) {
		evnt_ctrl = PCIE_ERROR1;
		goto fail2;
	}

#if defined(PCIE_DMA_INDEX)
	/* SETUP: dmaindex subsystem */
	if (pciedev_setup_dmaindex(pciedev) != BCME_OK) {
		evnt_ctrl = PCIE_ERROR1;
		goto fail2;
	}
#endif /* PCIE_DMA_INDEX */

#if defined(BCMPCIE_IDMA) && !defined(BCMPCIE_IDMA_DISABLED)
	/* SETUP: Implicit DMA subsystem */
	if (pciedev_setup_idma(pciedev) != BCME_OK) {
		evnt_ctrl = PCIE_ERROR1;
		goto fail2;
	}
#endif /* BCMPCIE_IDMA && !BCMPCIE_IDMA_DISABLED */

	/* SETUP the PCIE IPC : including pcie_ipc_rings, pcie_ipc_rings_mem */
	if (pciedev_setup_pcie_ipc(pciedev) != BCME_OK) {
		evnt_ctrl = PCIE_ERROR1;
		goto fail2;
	}

	/* BIND the pciedev bus layer to the dongle side PCIE IPC */
	if (pciedev_bind_pcie_ipc(pciedev) != BCME_OK) {
		evnt_ctrl = PCIE_ERROR1;
		goto fail2;
	}

#if defined(PCIE_D2H_DOORBELL_RINGER)
	/* Setup ringers to be default: pciedev_generate_host_db_intr */
	for (i = 0; i < BCMPCIE_D2H_COMMON_MSGRINGS; i++) {
		d2h_doorbell_ringer_t *ringer;
		ringer = &pciedev->d2h_doorbell_ringer[i];
		ringer->db_info.haddr.high = 0; /* Not a true host memory address */
		ringer->db_info.haddr.low = PCIE_DB_DEV2HOST_0;
		ringer->db_info.value = PCIE_D2H_DB0_VAL;
		ringer->db_fn = pciedev_generate_host_db_intr;
	}
#endif /* PCIE_D2H_DOORBELL_RINGER */

#ifdef BCMPCIE_D2H_MSI
	if (!PCIE_MSI_ENAB(pciedev->d2h_msi_info)) {
		pciedev->d2h_msi_info.msi_vector_offset[MSI_INTR_IDX_MAILBOX] =
			BCMPCIE_D2H_MSI_OFFSET_DEFAULT;
	}
#endif /* BCMPCIE_D2H_MSI */

	pciedev->event_pool_max = PCIEDEV_EVENT_BUFPOOL_MAX;

	if (getvar(NULL, rstr_event_pool_max) != NULL) {
		pciedev->event_pool_max =  getintvar(NULL, rstr_event_pool_max);
	}
	/* setup event and IOCTL resp buffer pools */
	pciedev->event_pool = pciedev_init_host_dma_buf_pool_init(pciedev,
		pciedev->event_pool_max);
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

	/* setup Timestamp resp buffer pools */
	pciedev->ts_pool = pciedev_init_host_dma_buf_pool_init(pciedev,
		PCIEDEV_TS_BUFPOOL_MAX);
	if (pciedev->ts_pool == NULL) {
		evnt_ctrl = PCIE_ERROR3;
		goto fail2;
	}

	pciedev_dump_hostbuf_pool(rstr_eventpool, pciedev->event_pool);
	pciedev_dump_hostbuf_pool(rstr_ioctlresp, pciedev->ioctl_resp_pool);

	for (i = 0; i < MAX_DMA_CHAN; i++) {
		if (VALID_DMA_CHAN_NO_IDMA(i, pciedev)) {
			/* htod dma queue init */
			if (!(pciedev->htod_dma_q[i] =
				pciedev_dma_queue_init(pciedev,
					MAX_DMA_QUEUE_LEN_H2D, i))) {
				PCI_ERROR(("%s: out of mem htod_dma_q \n",
					__FUNCTION__));
				goto fail;
			}

			/* dtoh dma queue init */
			if (!(pciedev->dtoh_dma_q[i] =
				pciedev_dma_queue_init(pciedev, max_d2h_queue_len, i))) {
				PCI_ERROR(("%s: out of memory for dtoh_dma_q \n", __FUNCTION__));
				goto fail;
			}
		}
	}

#ifdef WLCFP
	{	/* Section: cfp_flows_attach */
		cfp_flows_t *cfp_flows;

		/* Attach cfp_flows data structure */
		pciedev->cfp_flows = MALLOCZ(osh, sizeof(cfp_flows_t));

		/* Check for malloc failures */
		if (pciedev->cfp_flows == NULL) {
			PCI_ERROR(("%s: Failed to allocate cfp flow memory\n",
				__FUNCTION__));
			goto fail;
		}

		/* Initialize Cached Flow Processing state in the PCIe bus layer */
		cfp_flows = pciedev->cfp_flows;

		cfp_flows->pend = (uint8)0;
		cfp_flows->curr = (cfp_pktlist_t *)NULL;

		/* Initialize CFP capable and non-CFP capable pktlists */
		for (i = 0; i < CFP_PKTLISTS_TOTAL; i++) {
			cfp_flows->pktlists[i].head = (void *)NULL;
			cfp_flows->pktlists[i].tail = (void *)NULL;
			cfp_flows->pktlists[i].pkts = (uint16)0;
			cfp_flows->pktlists[i].prio = (uint8)i;
			cfp_flows->pktlists[i].pend = 1;

			CFP_FLOW_STATS_CLR(cfp_flows->pktlists[i].enqueue_stats);
			CFP_FLOW_STATS_CLR(cfp_flows->pktlists[i].dispatch_stats);
		}

		/* Assign a pend weight of 2 for non-CFP capable pktlist */
		cfp_flows->pktlists[DNGL_SENDUP_PKTLIST_IDX].pend = 2;

		/* Clear CFP Flows statistics */
		CFP_FLOW_STATS_CLR(cfp_flows->pend_dispatch_stats);
		CFP_FLOW_STATS_CLR(cfp_flows->prio_dispatch_stats);
		CFP_FLOW_STATS_CLR(cfp_flows->dngl_dispatch_stats);

		/* Register a cfp_flows_dump handler */
		if (!hnd_cons_add_cmd("cfp_flows", (cons_fun_t)cfp_flows_dump, cfp_flows))
			goto fail;

		CFP_FLOWS_TRACE(("CFP Flows Enabled\n"));
	}

#else  /* ! WLCFP */

#ifdef PKTC_TX_DONGLE
	pciedev->pkthead = NULL;
	pciedev->pkttail = NULL;
	pciedev->pkt_chain_cnt = 0;
#endif /* PKTC_TX_DONGLE */
#endif /* ! WLCFP */

	/* Attach to backplane */
	if (!(pciedev->sih = si_attach(device, osh, regs, bustype, NULL, &vars, &vars_len))) {
		PCI_TRACE(("%s: si_attach failed\n", __FUNCTION__));
		goto fail;
	}

	/* Bring back to Pcie core */
	si_setcore(pciedev->sih, PCIE2_CORE_ID, 0);

	pciedev->coreid = si_coreid(pciedev->sih);
	pciedev->corerev = si_corerev(pciedev->sih);

	/* 128-MB PCIE Access space addressable via
	 * two 64-MB regions using SB to PCIE translation 0 and 1. OR
	 * four 32-MB regions using SB to PCIE translation 0,1,2 and 3 after PcieGen2 rev24.
	 */
	if (PCIECOREREV(pciedev->corerev) >= 24) {
		if (BCM43684_CHIP(pciedev->sih->chip) ||
			BCM6715_CHIP(pciedev->sih->chip)) {
			/* On chips with CCI-400, the small pcie 128 MB region base has shifted */
			for (i = 0; i < SBTOPCIE_ATR_MAX; i++)
				pciedev->sb2pcie_atr[i].base = CCI400_SBTOPCIE0_BASE +
					i * SBTOPCIE_32M_REGIONSZ;
		} else {
			for (i = 0; i < SBTOPCIE_ATR_MAX; i++)
				pciedev->sb2pcie_atr[i].base = SBTOPCIE0_BASE +
					i * SBTOPCIE_32M_REGIONSZ;
		}
		for (i = 0; i < SBTOPCIE_ATR_MAX; i++)
			pciedev->sb2pcie_atr[i].mask = SBTOPCIE_32M_MASK;
	} else { /* < 24 */
		if (BCM4365_CHIP(pciedev->sih->chip)) {
			/* On chips with CCI-400, the small pcie 128 MB region base has shifted */
			pciedev->sb2pcie_atr[0].base = CCI400_SBTOPCIE0_BASE;
			pciedev->sb2pcie_atr[1].base = CCI400_SBTOPCIE1_BASE;
		} else {
			pciedev->sb2pcie_atr[0].base = SBTOPCIE0_BASE;
			pciedev->sb2pcie_atr[1].base = SBTOPCIE1_BASE;
		}
		pciedev->sb2pcie_atr[0].mask = SBTOPCIE0_MASK;
		pciedev->sb2pcie_atr[1].mask = SBTOPCIE1_MASK;
	}

#ifdef SW_PAGING
	/* Setup sb2pcie for initial hostmem access before HME is ready */
	{
		uint32 host_addr_lo = host_page_info[0];
		uint32 host_addr_hi = host_page_info[1];
		pcie_sb2pcie_atr_t *atr2 = &pciedev->sb2pcie_atr[2];
		/* 2b0:AccessType RW=00, 1b2:PrefetchEn=1, 1b3:WriteBurstEn=1 */
		const uint32 rw_pref_wrburst =
			SBTOPCIE_MEM | SBTOPCIE_PF | SBTOPCIE_WR_BURST; /* 0xC 4b1100 */

		PCI_ERROR(("Configuring sbtopcie2 for SW Paging lo=0x%08x hi=0x%08x\n",
			host_addr_lo, host_addr_hi));
		/* Configure SBTOPCIE Translation 2 register */
		W_REG(pciedev->osh, PCIE_sbtopcietranslation2(pciedev->autoregs),
			((host_addr_lo & atr2->mask) | rw_pref_wrburst));
		W_REG(pciedev->osh, PCIE_sbtopcietranslation2upper(pciedev->autoregs),
			host_addr_hi);
	}
#endif /* SW_PAGING */

#if defined(HNDBME) && defined(BCM_BUZZZ) && defined(BCM_BUZZZ_STREAMING_BUILD)
	{	/* CAUTION: Assumes Host's high32 is 0U */
		int bme_key; /* user registration key */
		uint8 eng_idx;

		PCI_ERROR(("BUZZZ PCIEDEV bme_init\n"));
		/* bme initialized in RTE. Bind BME service to bus' osh */
		bme_init(pciedev->sih, pciedev->osh);

		/* Two engines for dual buffer streaming.
		 * All dongle chipsets have a minimum of 2 engines.
		 */
		for (eng_idx = 0; eng_idx < 2; eng_idx++) {
			bme_key = bme_get_key(pciedev->osh, BME_USR_FD0 + eng_idx);
			if (bme_key == BME_INVALID) { /* not yet registered */
				bme_set_t bme_set;
				bme_set.idx = eng_idx;
				bme_key = bme_register_user(pciedev->osh, BME_USR_FD0 + eng_idx,
					BME_SEL_IDX, bme_set, BME_MEM_DNGL, BME_MEM_PCIE, 0U, 0U);
				if (bme_key == BME_INVALID) {
					PCI_ERROR(("PCIEDEV bme_register_user %u engine %u fail\n",
						BME_USR_FD0 + eng_idx, eng_idx));
					goto fail;
				}
			}
			pciedev->bme_key[eng_idx] = bme_key;
		}

		PCI_ERROR(("PCIEDEV bme_register_user dual engines bme_key[%x, %x]\n",
			pciedev->bme_key[0], pciedev->bme_key[1]));
	}
#endif /* HNDBME && BCM_BUZZZ && BCM_BUZZZ_STREAMING_BUILD */

	/* Newer gen chip has pcie core DMA */
	/* Initialize pcie dma */
	{
#ifdef PCIE_DMAXFER_LOOPBACK
		pciedev->dmaxfer_loopback =
			(pcie_dmaxfer_loopback_t *) MALLOCZ(osh, sizeof(pcie_dmaxfer_loopback_t));

		if (!pciedev->dmaxfer_loopback) {
			PCI_ERROR(("%s: out of memory to allocate pci dmaxfer loopback\n",
					__FUNCTION__));
			PCIEDEV_MALLOC_ERR_INCR(pciedev);
			goto fail;
		}

		pktq_init(&pciedev->dmaxfer_loopback->lpbk_dma_txq,
				PCIE_LPBK_SUPPORT_TXQ_NPREC, PCIE_LPBK_SUPPORT_TXQ_SIZE);

		pktq_init(&pciedev->dmaxfer_loopback->lpbk_dma_txq_pend,
				PCIE_LPBK_SUPPORT_TXQ_NPREC, PCIE_LPBK_SUPPORT_TXQ_SIZE);

		pciedev->dmaxfer_loopback->lpbk_src_dmaxfer_timer =
			dngl_init_timer(pciedev, NULL, pciedev_lpbk_src_dmaxfer_timerfn);

		pciedev->dmaxfer_loopback->lpbk_dest_dmaxfer_timer =
			dngl_init_timer(pciedev, NULL, pciedev_lpbk_dest_dmaxfer_timerfn);
#endif /* PCIE_DMAXFER_LOOPBACK */

		pciedev->in_d3_suspend = FALSE;
		pciedev->bus_counters->hostwake_asserted = FALSE;
		pciedev->in_d3_pktcount_rx = 0;
		pciedev->in_d3_pktcount_evnt = 0;
		pciedev->host_wake_gpio = pciedev_host_wake_gpio_init(pciedev);

#if defined(BCMFRAGPOOL) && !defined(BCMFRAGPOOL_DISABLED)
		pktq_depth = SHARED_FRAG_POOL_LEN;
#elif defined(BCMHWA) && defined(HWA_TXPOST_BUILD)
		pktq_depth = HWA_TXPATH_PKTS_MAX;
#endif // endif
#if defined(BCMRXFRAGPOOL) && !defined(BCMRXFRAGPOOL_DISABLED)
		pktq_depth += SHARED_RXFRAG_POOL_LEN;
#endif // endif
#if defined(BCMRESVFRAGPOOL) && !defined(BCMRESVFRAGPOOL_DISABLED)
		pktq_depth += RESV_FRAG_POOL_LEN;
#endif // endif
		/* Account for events as well */
		pktq_depth += MAX_EVENT_Q_LEN;

		/* this should be the max rxlfrags and txlfrags we could support */
		pktq_init(&pciedev->d2h_req_q, D2H_REQ_PRIO_NUM,
			MAX(PCIEDEV_MIN_PKTQ_DEPTH, pktq_depth));

#ifdef PCIE_DELAYED_HOSTWAKE
		/* Initialize the timer that is used to do deferred host_wake */
		pciedev->delayed_hostwake_timer = dngl_init_timer(pciedev, NULL,
			pciedev_delayed_hostwake_timerfn);
		pciedev->delayed_hostwake_timer_active = FALSE;
#endif /* PCIE_DELAYED_HOSTWAKE */

		/* Initialize the timer that is used to do deferred host_wake */
		pciedev->delayed_msgbuf_intr_timer_on = FALSE;
		pciedev->delayed_msgbuf_intr_timer = dngl_init_timer(pciedev, NULL,
			pciedev_sched_msgbuf_intr_process);

		pciedev->health_check_timer_on = FALSE;

		pciedev->bm_enabled = TRUE;
		pciedev->bm_enab_timer = dngl_init_timer(pciedev, NULL,
		                                       pciedev_bm_enab_timerfn);
		pciedev->bm_enab_timer_on = FALSE;
		pciedev->bm_enab_check_interval = PCIE_BM_CHECK_INTERVAL;

		pciedev->host_sleep_ack_timer = dngl_init_timer(pciedev, NULL,
			pciedev_host_sleep_ack_timerfn);
		pciedev->host_sleep_ack_timer_on = FALSE;
		pciedev->host_sleep_ack_timer_interval = PCIE_HOST_SLEEP_ACK_INTERVAL;

		pciedev->max_m2mdma_chan = MAX_DMA_CHAN;

		/* reset pcie core interrupts */
		pciedev_reset_pcie_interrupt(pciedev);

		/* clear the intstatus bit going from backplane cores to PCIE */
		si_wrapperreg(pciedev->sih, AI_OOBSELINA74, AI_OOBSELINA74_CORE_MASK, 0);
		si_wrapperreg(pciedev->sih, AI_OOBSELINA30, AI_OOBSELINA30_CORE_MASK, 0);

		/* initialize ltr latency values */
		pciedev->ltr_info.active_lat = (LTR_SCALE_US << 10) | LTR_LATENCY_60US;
		pciedev->ltr_info.idle_lat = (LTR_SCALE_US << 10) | LTR_LATENCY_100US;
		pciedev->ltr_info.sleep_lat = (LTR_SCALE_MS << 10) | LTR_LATENCY_3MS;

		/* initialize ltr default setting */
		pciedev->ltr_info.ltr0_regval = PCIE_LTR0_REG_DEFAULT_60;
		pciedev->ltr_info.ltr1_regval = PCIE_LTR1_REG_DEFAULT;
		pciedev->ltr_info.ltr2_regval = PCIE_LTR2_REG_DEFAULT;
	}

	pciedev_set_LTRvals(pciedev);

	/* Look for L1ss+ASPM from host periodically. */
	if (PCIECOREREV(pciedev->corerev) < 14) {
		pciedev->ltr_sleep_after_d0 = TRUE;
	}
	if (pciedev_handle_d0_enter_bm(pciedev) != BCME_OK) {
		/* Start periodic check for Bus Master Enable */
		if (!pciedev->bm_enab_timer_on) {
			dngl_add_timer(pciedev->bm_enab_timer,
				pciedev->bm_enab_check_interval, FALSE);
			pciedev->bm_enab_timer_on = TRUE;
		}
	}

	/* 4350C0/C1 set bit 4, 6, 17 in PMU chip control reg 6 */
	/* 4350C1+: set bit 25, 27 in PMU chip control reg 7 */
	if (PCIECOREREV(pciedev->corerev) == 7 || PCIECOREREV(pciedev->corerev) == 11) {
		uint32 value;

		value = PMU_CC6_ENABLE_CLKREQ_WAKEUP | PMU_CC6_ENABLE_PMU_WAKEUP_ALP |
			CC6_4350_PMU_EN_EXT_PERST_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);

		if (PCIECOREREV(pciedev->corerev) == 11) {
			value = PMU_CC7_ENABLE_L2REFCLKPAD_PWRDWN | PMU_CC7_ENABLE_MDIO_RESET_WAR;
			si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL7, value, value);
		}
	} else if (PCIECOREREV(pciedev->corerev) == 9) {
		uint32 mask = PMU43602_CC2_PCIE_CLKREQ_L_WAKE_EN |
		       PMU43602_CC2_ENABLE_L2REFCLKPAD_PWRDWN |
		       PMU43602_CC2_PERST_L_EXTEND_EN;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL2, mask, mask);
	} else if (PCIECOREREV(pciedev->corerev) == 23) {
		uint32 mask = 0, value;

		value = PMU_CC6_ENABLE_CLKREQ_WAKEUP | PMU_CC6_ENABLE_PMU_WAKEUP_ALP;

		if (PCIECOREREV(pciedev->corerev) == 23) {
			value |= PMU_CC6_ENABLE_PCIE_RETENTION | PMU_CC6_ENABLE_PMU_EXT_PERST;
		}

		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);

		value = PMU_CC2_GCI2_WAKE;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL2, value, value);

		if (PCIECOREREV(pciedev->corerev) == 23) {
			value = PMU_CC10_FORCE_PCIE_SW_ON;
			mask = PMU_CC10_FORCE_PCIE_SW_ON;

			/* for reducing L1.2 exit latency */
			value |= (PMU_CC10_PCIE_PWRSW_RESET_CNT_8US <<
				PMU_CC10_PCIE_PWRSW_RESET0_CNT_SHIFT) |
				(PMU_CC10_PCIE_PWRSW_RESET_CNT_4US <<
				PMU_CC10_PCIE_PWRSW_RESET1_CNT_SHIFT) |
				(PMU_CC10_PCIE_PWRSW_UP_DLY_0US <<
				PMU_CC10_PCIE_PWRSW_UP_DLY_SHIFT) |
				(PMU_CC10_PCIE_PWRSW_FORCE_PWROK_DLY_4US <<
				PMU_CC10_PCIE_PWRSW_FORCE_PWROK_DLY_SHIFT);
			mask |= PMU_CC10_PCIE_PWRSW_RESET0_CNT_MASK |
				PMU_CC10_PCIE_PWRSW_RESET1_CNT_MASK |
				PMU_CC10_PCIE_PWRSW_UP_DLY_MASK |
				PMU_CC10_PCIE_PWRSW_FORCE_PWROK_DLY_MASK;
		}
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL10, mask, value);
	}

	/* 4349 set bit 4, 6, 13 in PMU chip control reg 6 */
	if ((PCIECOREREV(pciedev->corerev) == 13) ||
		(PCIECOREREV(pciedev->corerev) == 17) ||
		(PCIECOREREV(pciedev->corerev) == 21)) {
		uint32 value;

		value = PMU_CC6_ENABLE_CLKREQ_WAKEUP | PMU_CC6_ENABLE_PMU_WAKEUP_ALP |
			CC6_4349_PMU_EN_EXT_PERST_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);
	}

	if (PCIECOREREV(pciedev->corerev) == 16) {
		uint32 value;

		value = CC6_4349_PMU_EN_EXT_PERST_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);
	}

	/* enable PMU level changes for 4345B0/B1 */
	/* set bit 15 and 24  in PMU chip control reg 6 */
	if ((PCIECOREREV(pciedev->corerev) == 12) || (PCIECOREREV(pciedev->corerev) == 14)) {
		uint32 value;

		value = CC6_4345_PMU_EN_ASSERT_L2_MASK | CC6_4345_PMU_EN_MDIO_MASK |
			CC6_4345_PMU_EN_PERST_DEASSERT_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);
	}

	/* enable PMU level changes for 4349a0 */
	/* set bit 15 and 16  in PMU chip control reg 6 */
	if ((PCIECOREREV(pciedev->corerev) == 13) ||
		(PCIECOREREV(pciedev->corerev) == 17) ||
		(PCIECOREREV(pciedev->corerev) == 21)) {
		uint32 value;

		value = CC6_4349_PMU_ENABLE_L2REFCLKPAD_PWRDWN	| CC6_4349_PMU_EN_MDIO_MASK;
		si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6, value, value);
	}

	/* Register a Callback Function at pktfree */
	lbuf_free_register(pciedev_lbuf_callback, pciedev);

	/* Register a Callback Function for spktq */
	spktq_free_register(pciedev_spktq_callback, pciedev);

#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		/* pciedev implementation to dispatch a fetch request */
		hnd_fetch_bus_dispatch_cb_register(pciedev_dispatch_fetch_rqst, pciedev);
		/* pciedev implementation to flush pending fetch requests */
		hnd_fetch_bus_flush_cb_register(pciedev_flush_fetch_requests, pciedev);

		for (i = 0; i < MAX_DMA_CHAN; i++) {
			if (VALID_DMA_CHAN_NO_IDMA(i, pciedev)) {
				pciedev->fcq[i] = MALLOCZ(pciedev->osh,
					sizeof(pciedev_fetch_cmplt_q_t));
				if (pciedev->fcq[i] == NULL) {
					evnt_ctrl = PCIE_ERROR4;
					while (--i >= 0) {
						MFREE(pciedev->osh, pciedev->fcq[i],
							sizeof(pciedev_fetch_cmplt_q_t));
					}
					goto fail;
				}
			}
		}
	}
#endif /* BCMPCIEDEV */

	/* For ioctl_ptr case, we need only one fetch_rqst prealloced. Do not use pool for this */
	pciedev->ioctptr_fetch_rqst = MALLOCZ(pciedev->osh, sizeof(struct fetch_rqst));
	if (pciedev->ioctptr_fetch_rqst == NULL) {
		evnt_ctrl = PCIE_ERROR5;
		goto fail;
	}

	/* Register a call back function to be called for every pktpool_get */
	/* After dequeuing a pkt, it should be passed to pciedev_fillup_haddr
	 * or pciedev_manage_haddr
	 */
	/* And here, host address, pktid and length info are populated */
#if defined(BCMPCIEDEV)
	if (BCMPCIEDEV_ENAB() && BCMSPLITRX_ENAB()) {
		pciedev->copycount = 0;
		pciedev->tcmsegsz = 0;
		if (SPLIT_RXMODE1() || SPLIT_RXMODE2()) {
			pciedev->tcmsegsz = PKTRXFRAGSZ;
		}
#if defined(BCMRXFRAGPOOL) && !defined(BCMRXFRAGPOOL_DISABLED)
		pktpool_hostaddr_fill_register(pciedev->pktpool_rxlfrag,
			pciedev_manage_haddr, pciedev);

		pktpool_rxcplid_fill_register(pciedev->pktpool_rxlfrag,
			pciedev_manage_rxcplid, pciedev);
#endif // endif
	}
#endif /* defined(BCMPCIEDEV) */
	/* SHARED_POOL also need rxcplid because hnd_pktfetch_dispatch allocate it for RX. */
	pktpool_rxcplid_fill_register(pciedev->pktpool, pciedev_manage_rxcplid, pciedev);

	/* Initialize dongle device */
	if (!(pciedev->dngl = dngl_attach(pciedev, NULL, pciedev->sih, osh))) {
		evnt_ctrl = PCIE_ERROR6;
		goto fail;
	}

	/* For ts_ptr case, we need only one fetch_rqst prealloced. Do not use pool for this */
	pciedev->tsptr_fetch_rqst = MALLOCZ(pciedev->osh, sizeof(struct fetch_rqst));
	if (pciedev->tsptr_fetch_rqst == NULL) {
			evnt_ctrl = PCIE_ERROR7;
			goto fail;
	}

#ifdef WAR1_HWJIRA_CRWLPCIEGEN2_162
	pciedev->WAR_timeout = 1; /* ms */
	pciedev->WAR_timer_on = FALSE;
	pciedev->WAR_timer_resched = FALSE;
	pciedev->WAR_timer = dngl_init_timer(pciedev, NULL, pciedev_WAR_timerfn);
#endif /* WAR1_HWJIRA_CRWLPCIEGEN2_162 */
	pciedev->mailboxintmask = 0;
	/* pciecoreset becomes TRUE when host asserts RESET# when core is in 'survive PERST' mode */
	pciedev->pciecoreset = FALSE;

#ifdef UART_TRAP_DBG
	/* cause Trap 4 using UART  */
	if (!hnd_cons_add_cmd("\a", (cons_fun_t)pciedev_trap_from_uart, pciedev))
		goto fail;
	/* dump PCIE registers information using the UART  */
	if (!hnd_cons_add_cmd("\x5", (cons_fun_t)pciedev_dump_pcie_info, pciedev))
		goto fail;
#endif /* UART_TRAP_DBG */

	/* check if msi is in use, used for rev 6 of API */
	if (R_REG_CFG(pciedev, PCIEGEN2_MSI_CAP_REG_OFFSET) & MSI_ENABLED_CHECK) {
		pciedev->msi_in_use = TRUE;
	}
	pciedev->clear_db_intsts_before_db = FALSE;

#ifdef RTE_CONS
#ifdef BCMDBG_SD_LATENCY
	if (!hnd_cons_add_cmd("sdlat", pciedev_print_latency, pciedev))
		goto fail;
#endif // endif
#endif /* RTE_CONS */

	/* Set halt handler */
	hnd_set_fwhalt(pciedev_fwhalt, pciedev);

#ifdef DEBUG_PARAMS_ENABLED
/*	debug_params.txavail = pciedev->txavail; */
#endif /* DEBUG_PARAMS_ENABLED */

	/* Initialize the timer that is used to test user LTR state requests */
	pciedev->ltr_test_timer = dngl_init_timer(pciedev, NULL, pciedev_usrltrtest_timerfn);

	pciedev_send_ltr(pciedev, LTR_SLEEP);

	pciedev_crwlpciegen2(pciedev);
	pciedev_reg_pm_clk_period(pciedev);
	if (PCIECOREREV(pciedev->corerev) <= 13) {
		pciedev_crwlpciegen2_61(pciedev);
	}
	pciedev_crwlpciegen2_180(pciedev);
	si_pcie_disable_oobselltr(pciedev->sih);

	if (PCIECOREREV(pciedev->corerev) == 23) {
		pciedev->fastlpo_pcie_en = si_pmu_fast_lpo_enable_pcie(pciedev->sih);
		pciedev->fastlpo_pmu_en = si_pmu_fast_lpo_enable_pmu(pciedev->sih);
		if (PCIE_FASTLPO_ENAB(pciedev)) {
			pciedev_trefup_init(pciedev);
		}
		if (getvar(NULL, rstr_sig_det_filt_dis) != NULL) {
			pciedev->sig_det_filt_dis = getintvar(NULL, rstr_sig_det_filt_dis);
		}
		pciedev->pipeiddq1_enab = 1;
		if (getvar(NULL, rstr_pcie_pipeiddq1_enab) != NULL) {
			pciedev->pipeiddq1_enab = getintvar(NULL, rstr_pcie_pipeiddq1_enab);
		}
	}

#ifdef PCIE_DEEP_SLEEP
	pciedev->ds_state = DS_INVALID_STATE;
	pciedev->ds_check_timer = dngl_init_timer(pciedev, NULL,
		pciedev_ds_check_timerfn);
	pciedev->ds_check_timer_on = FALSE;
	pciedev->ds_check_timer_max = 0;
	pciedev->ds_check_interval = PCIE_DS_CHECK_INTERVAL;
	pciedev_disable_deepsleep(pciedev, TRUE);

#ifndef PCIEDEV_INBAND_DW
	pciedev->ds_oob_dw_supported = 1;
	if ((pciedev->dw_counters.dw_gpio =
		si_get_device_wake_opt(pciedev->sih)) == CC_GCI_GPIO_INVALID) {
		pciedev->ds_oob_dw_supported = 0;
		pciedev_disable_ds_state_machine(pciedev);
	}
#endif // endif
#endif /* PCIE_DEEP_SLEEP */
	if ((PCIECOREREV(pciedev->corerev) == 13) ||
		(PCIECOREREV(pciedev->corerev) == 17) ||
		(PCIECOREREV(pciedev->corerev) == 21)) {
		pciedev_enable_perst_deassert_wake(pciedev);
	}

	/* reduce pcie L1.2 exit latency */
	if (PCIECOREREV(pciedev->corerev) == 23) {
		AND_REG_CFG(pciedev, PCI_PHY_CTL_0, ~PCI_SLOW_PMCLK_EXT_RLOCK);
	}

	if ((pciedev->ioctl_req_pool = MALLOCZ(pciedev->osh, sizeof(pktpool_t))) == NULL) {
		PCI_ERROR(("IOCTL req pool alloc failed \n"));
		goto fail;
	}

	pciedev->ctrl_resp_q = pciedev_setup_ctrl_resp_q(pciedev, pciedev->tunables[MAXCTRLCPL]);
	if (pciedev->ctrl_resp_q == NULL) {
		PCI_ERROR(("Cmplt queue failed\n"));
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

	if ((pciedev->ts_req_pool = MALLOCZ(pciedev->osh, sizeof(pktpool_t))) == NULL) {
		PCI_ERROR(("Timestamp req pool alloc failed \n"));
		goto fail;
	}
	n = TS_PKTPOOL_SIZE;
	if (pktpool_init(pciedev->osh, pciedev->ts_req_pool, &n,
		TS_PKTBUFSZ, FALSE, lbuf_basic) != BCME_OK)
	{
		PCI_ERROR(("couldn't init pktpool for TIMESTAMP requests\n"));
		goto fail;
	}
	pktpool_setmaxlen(pciedev->ts_req_pool, TS_PKTPOOL_SIZE);

	/* allocate rx completion blobs */
	if (!bcm_alloc_rxcplid_list(pciedev->osh, pciedev->pcie_ipc->max_rx_pkts)) {
		PCI_ERROR(("error enabling the rx completion message IDs\n"));
		goto fail;
	}

	/* Flowrings may not be setup yet, as we do not know what TxPost formats
	 * will be advertized by the host. Setup involving construction of lcl_buf
	 * pools need to know the size of a TxPost.
	 */

	pciedev->flow_schedule_timer =
		dngl_init_timer(pciedev, NULL, pciedev_flow_schedule_timerfn);
	pciedev->flow_sch_timer_on = FALSE;

	pciedev->flow_ageing_timer =
		dngl_init_timer(pciedev, NULL, pciedev_flow_ageing_timerfn);
	pciedev->flow_ageing_timer_on = FALSE;
	pciedev->flow_supp_enab = 0;
	pciedev->flow_age_timeout = PCIEDEV_FLOW_RING_SUPP_TIMEOUT;
	pciedev->metric_ref->active = OSL_SYSUPTIME();
	pciedev->extra_lfrags = 0;

	/* Register timer function for flushing glommed rx status */
	pciedev->glom_timer = dngl_init_timer(pciedev, NULL,
		pciedev_flush_glommed_rxstatus);

	STATIC_ASSERT(H2DRING_CTRL_SUB_ITEMSIZE >=  D2HRING_CTRL_CMPLT_ITEMSIZE);

	/* Enable stats collection */
	if (PCIECOREREV(pciedev->corerev) >= 7) {
		W_REG_CFG(pciedev, PCI_STAT_CTRL,
			PCIE_STAT_CTRL_RESET | PCIE_STAT_CTRL_ENABLE);
	}

	/* priority flow ring queue attach */
	if ((pciedev->prioring = (prioring_pool_t *)MALLOC(pciedev->osh,
		sizeof(prioring_pool_t) * PCIE_MAX_TID_COUNT)) == NULL) {
		PCI_ERROR(("priority ring pool alloc failed\n"));
		goto fail;
	}

	/* Update max number of slave devices supported */
	pciedev->max_slave_devs = dngl_max_slave_devs(pciedev->dngl);

	dll_init(&pciedev->active_prioring_list);
	pciedev->last_fetch_prio = &pciedev->active_prioring_list;

	/* prioring could be tid or AC based, at init intialize to max first */
	for (i = 0; i < PCIE_MAX_TID_COUNT; i++) {
		dll_init(&pciedev->prioring[i].active_flowring_list);
		pciedev->prioring[i].last_fetch_node = &pciedev->prioring[i].active_flowring_list;
		pciedev->prioring[i].tid_ac = (uint8)i;
		pciedev->prioring[i].inited = FALSE;
	}

	/* Initialize the pool for max_flowrings flows */
	pciedev->flowrings_dll_pool =
		dll_pool_init(pciedev->osh, pciedev->max_flowrings, sizeof(flowring_pool_t));

	if (pciedev->flowrings_dll_pool == NULL)
		goto fail;

#ifdef MEM_ALLOC_STATS
	hnd_get_heapuse(&mu);
	mu.max_flowring_alloc += sizeof(flowring_pool_t);
	hnd_update_mem_alloc_stats(&mu);
#endif /* MEM_ALLOC_STATS */

	/* Initialize the struct for storing IPC data for stats */
	if ((pciedev->ipc_data = MALLOCZ(pciedev->osh, sizeof(ipc_data_t))) == NULL) {
		PCI_ERROR(("malloc failed for ipc_data!\n"));
		pciedev->ipc_data_supported = FALSE;
	} else
		pciedev->ipc_data_supported = TRUE;

#ifdef DBG_IPC_LOGS
	if ((pciedev->ipc_logs = MALLOCZ(pciedev->osh, sizeof(ipc_log_t))) == NULL) {
		PCI_ERROR(("malloc failed for ipc_logs!\n"));
	}
#endif // endif

#ifdef PCIEDEV_USE_EXT_BUF_FOR_IOCTL
	pciedev->ioct_resp_buf_len = PCIEDEV_MAX_IOCTLRSP_BUF_SIZE;
	pciedev->ioct_resp_buf = MALLOCZ(pciedev->osh, pciedev->ioct_resp_buf_len);
	if (pciedev->ioct_resp_buf == NULL) {
		PCI_ERROR(("Ioctl buffer allocation failed. Only 2K ioctls will be supported.\n"));
	}
#else
	pciedev->ioct_resp_buf_len = 0;
	pciedev->ioct_resp_buf = NULL;
#endif // endif

#if defined(BCMPCIE_IPC_HPA)
	/* Initialize the Host PacketId Audit Tool ... allocate mwbmap */
	pciedev->hpa_hndl = BCMPCIE_IPC_HPA_INIT(pciedev->osh);
#endif   /* BCMPCIE_IPC_HPA */

#if defined(PCIE_M2M_D2H_SYNC)
	/* starting epoch values */
	pciedev->rxbuf_cmpl_epoch = D2H_EPOCH_INIT_VAL;
	pciedev->txbuf_cmpl_epoch = D2H_EPOCH_INIT_VAL;
	pciedev->ctrl_compl_epoch = D2H_EPOCH_INIT_VAL;
	pciedev->info_buf_compl_epoch = D2H_EPOCH_INIT_VAL;
#endif /* PCIE_M2M_D2H_SYNC */

	if (!HOSTREADY_ONLY_ENAB()) {
		if (HOST_PCIE_API_REV_GE_6(pciedev))
			pciedev->db1_for_mb = PCIE_NO_MB_DB1_INTR;
		else
			pciedev->db1_for_mb = PCIE_BOTH_MB_DB1_INTR;
	}

	/* Update max number of slave devices supported */
	pciedev->max_slave_devs = dngl_max_slave_devs(pciedev->dngl);

	/* Allocate per ifidx stats structure */
	pciedev->ifidx_account = (ifidx_tx_account_t *)MALLOCZ(pciedev->osh,
		pciedev->max_slave_devs * sizeof(ifidx_tx_account_t));

	if (pciedev->ifidx_account == NULL)
		goto fail;

#ifdef PCIE_ERR_ATTN_CHECK
	/* Clearing err_code and CoE core reg 0x814 after attach */
	err_type = R_REG_CFG(pciedev, PCI_CFG_TLCNTRL_5);
	OR_REG_CFG(pciedev, PCI_CFG_TLCNTRL_5, err_type);
	err_code_state = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_code_logreg);
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_code_logreg, err_code_state);
#endif // endif

#ifdef HEALTH_CHECK
	/*
	* Read and clear the AER status registers during attach in case any bits were not
	* cleared during device RST. From here on, any AER UC errors will be treated as
	* fatal.
	*/
	if (pciedev_check_device_AER(pciedev, NULL, 0) == TRUE) {
		PCI_ERROR(("Detected AER errors during pcie attach. "
			"Errors treated as non-fatal.\n"));
	}
#endif /* HEALTH_CHECK */

#ifdef DUMP_PCIEREGS_ON_TRAP
	pciedev->coe_dump_buf_size = pciedev_dump_coe_list_array_size(pciedev);
#endif /* DUMP_PCIEREGS_ON_TRAP */
#if defined(SAVERESTORE)
	/* Register the routines that gets called during ARM WFI and Wakeup */
	sr_register_save(pciedev->sih, pciedev_sr_save, (void *)pciedev);
	sr_register_restore(pciedev->sih, pciedev_sr_restore, (void *)pciedev);
#endif /* SAVERESTORE */
	pciedev->fwtsinfo_timer = dngl_init_timer(pciedev, NULL,
		pciedev_fwtsinfo_timerfn);
	pciedev->fwtsinfo_timer_active = FALSE;
#ifdef TIMESYNC_GPIO
	pciedev->time_sync_gpio = pciedev_time_sync_gpio_init(pciedev);
#endif /* TIMESYNC_GPIO */
#if defined(HEALTH_CHECK) && !defined(HEALTH_CHECK_DISABLED)
	/* Initialize the struct for healthcheck parameters */
	if ((pciedev->hc_params =
		MALLOCZ(pciedev->osh, sizeof(pcie_hc_params_t))) == NULL) {
		PCI_ERROR(("malloc failed for healthcheck params!\n"));
		goto fail;
	}
	/* Request a block for pciedev health check.
	 * This block of buffer that will be
	 * sent to the host for this top level module
	 */
	pciedev->hc_params->hc =
		health_check_init(pciedev->osh,
			HCHK_SW_ENTITY_PCIE, sizeof(bcm_dngl_pcie_hc_t));
	if (pciedev->hc_params->hc == NULL) {
		PCI_ERROR(("Health check init failed\n"));
		goto fail;
	}
	else {
		health_check_module_register(pciedev->hc_params->hc,
			pciedev_healthcheck_fn, (void *)pciedev, 0);
	}
	pciedev->hc_params->last_ds_interval = -1;
	pciedev->hc_params->disable_ds_hc = TRUE; /* Disable healthcheck deepsleep by default */
	if (getvar(NULL, rstr_ds_hc_enable) != NULL) {
		pciedev->hc_params->ds_hc_enable_val =  getintvar(NULL, rstr_ds_hc_enable);
	}
	if (!(pciedev->hc_params->ds_check_reg_log = (pcie_ds_check_logs_t *) MALLOCZ(osh,
			PCIEDEV_MAX_DS_CHECK_LOG * sizeof(pcie_ds_check_logs_t)))) {
		PCI_ERROR(("Couldn't allocate memory for DS healthcheck log\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
	}
	/* Initialize the timer that is used to validate deepsleep healthcheck */
	pciedev->hc_params->ds_hc_trigger_timer = dngl_init_timer(pciedev, NULL,
		pciedev_ds_hc_trigger_timerfn);
	pciedev->hc_params->ds_hc_trigger_timer_active = FALSE;
	/* Register deelsleep healthcheck callback */
	hnd_hc_bus_ds_cb_register(pciedev_chip_not_in_ds_too_long, pciedev);
#endif /* HEALTH_CHECK  && !defined(HEALTH_CHECK_DISABLED) */

	if (PCIECOREREV(pciedev->corerev) == 23) {
		pciedev_linkloss_war(pciedev);
		/* enable assertion of pcie_pipe_iddq during L2 state */
		AND_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ~PCIE_PipeIddqDisable1);
	}

	if ((PCIECOREREV(pciedev->corerev) == 23) || (PCIECOREREV(pciedev->corerev) == 24)) {
		/* WAR:HW4347-746 disable L0S */
		pciedev_disable_l0s(pciedev);
		/* enable assertion of pcie_pipe_iddq during L2 state */
		AND_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ~PCIE_PipeIddqDisable1);
	}

	/* Allocate M2M structure */
	pciedev->m2m_req = (pcie_m2m_req_t *)MALLOCZ(pciedev->osh,
		sizeof(pcie_m2m_req_t) + (PCIE_M2M_REQ_VEC_MAX * sizeof(pcie_m2m_vec_t)));

	if (pciedev->m2m_req == NULL)
		goto fail;

	pciedev->linkdown_trap_dis = getintvar(NULL, rstr_pcie_linkdown_trap_dis);
	return pciedev;
fail:
	if (pciedev) {
		MODULE_DETACH(pciedev, pciedev_detach);
	}
fail2:
	if (evnt_ctrl) {
		PCI_ERROR(("error setting up the host buf Error no %d evnt_ctrl\n", evnt_ctrl));
	}

	return NULL;
} /* pciedev_attach */

/** Check for vendor id */
static bool
BCMATTACHFN(pciedev_match)(uint vendor, uint device)
{
	PCI_TRACE(("pciedev_match\n"));
	if (vendor != VENDOR_BROADCOM)
		return FALSE;

	return TRUE;
}

static const char BCMATTACHDATA(rstr_maxflring)[]	= "maxflring";
/** Initialize tunables */
static void
BCMATTACHFN(pciedev_tunables_init)(struct dngl_bus *pciedev)
{
	char *var;

	/* set tunables defaults (RXACK and TXDROP are stats) */
	pciedev->tunables[PCIEBUS_H2D_NTXD] = PD_H2D_NTXD;
	pciedev->tunables[PCIEBUS_H2D_NRXD] = PD_H2D_NRXD;
	pciedev->tunables[PCIEBUS_D2H_NTXD] = PD_D2H_NTXD;
	pciedev->tunables[PCIEBUS_D2H_NRXD] = PD_D2H_NRXD;
	pciedev->tunables[RXBUFSIZE] = PD_RXBUF_SIZE;
	pciedev->tunables[MAXHOSTRXBUFS] = MAX_HOST_RXBUFS;
	pciedev->tunables[MAXTXSTATUS] = MAX_TX_STATUS_COMBINED;
	pciedev->tunables[D2HTXCPL] = PD_NBUF_D2H_TXCPL;
	pciedev->tunables[D2HRXCPL] = PD_NBUF_D2H_RXCPL;
	pciedev->tunables[H2DRXPOST] = PD_NBUF_H2D_RXPOST;
	pciedev->tunables[MAXCTRLCPL] = PCIEDEV_CNTRL_CMPLT_Q_SIZE;
	pciedev->tunables[MAXPKTFETCH] = PCIEDEV_MAX_PACKETFETCH_COUNT;

	/* override max h2d flowring number if nvram present */
	var = getvar(NULL, rstr_maxflring);
	if (var && bcm_atoi(var) && (bcm_atoi(var) < BCMPCIE_MAX_TX_FLOWS)) {
		pciedev->tunables[MAXTXFLOW] = bcm_atoi(var);
		PCI_ERROR(("override max tx flowrings from %d to %d\n",
			BCMPCIE_MAX_TX_FLOWS, pciedev->tunables[MAXTXFLOW]));
	}
	else {
		pciedev->tunables[MAXTXFLOW] = BCMPCIE_MAX_TX_FLOWS;
	}

#ifdef BCMHWA
	/* HWA 2b and 4b don't need lcl_buf */
	HWA_TXCPLE_EXPR(pciedev->tunables[D2HTXCPL] = 0);
	HWA_RXCPLE_EXPR(pciedev->tunables[D2HRXCPL] = 0);
#endif // endif
}

void *
pciedev_dngl(struct dngl_bus *pciedev)
{
	return pciedev->dngl;
}

void
pciedev_notify_devpwrstchg(struct dngl_bus *pciedev, bool hostmem_acccess_enabled)
{
	uint32 buf = (hostmem_acccess_enabled ? 1 : 0);

	dngl_dev_ioctl(pciedev->dngl, RTEDEVPWRSTCHG, &buf, sizeof(uint32));
}

/** wakes host */
static void
pciedev_pme_hostwake(struct dngl_bus *pciedev, bool state)
{
	uint32 buf = state;
	if ((PCIECOREREV(pciedev->corerev) == 16) || (PCIECOREREV(pciedev->corerev) >= 23)) {
		/* HW JIRA: CRWLPCIEGEN2-250
		 * Setting cfg clk (bit 3) is part of SW WAR for PCIe rev 16 for SW PME generation
		 * bit3 is useful when link is in L2/L3. When set to 1, and if forceTlClkOn = '0',
		 * then cfg_clk will be driven by alp_clk. This should be set by software when it
		 * wants to program some CoE core registers and PERST is asserted.
		 */
		if (state)
			OR_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs),
					PCIE_FORCECFGCLKON_ALP);
		/* Set/Clear PME bit depending on host in sleep/wake */
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.pme_source,
				(state << PCIE_SWPME_FN0_SHF));
	}
	else
		dngl_dev_ioctl(pciedev->dngl, RTEDEVPMETOGGLE, &buf, sizeof(uint32));
}

#ifdef PCIE_PWRMGMT_CHECK
/**
 * XXX RB:20405 - Fix for D0 interrupt miss
 * Workaround for a hardware issue (D0 interrupt missing at D3 exit). While putting the device into
 * D3 and the link to L2/L3 state, we are missing the D0 interrupt after L0. If this happens, no
 * IOCTL would work. As an workaround, all the D0 entry logic is applied at the doorbell interrupt
 * handler after making sure that device is indeed in D0 state by reading the pcie iocstatus
 * register.
 *
 * With IPC rev7, we have a more full proof solution for this issue, where host issues DoorBell1 to
 * indicate D3 exit. Only after that, D0 interrupt handler is invoked.
 *
 * Calls pciedev_handle_d0_enter_bm().
 */
void
pciedev_handle_d0_enter(struct dngl_bus *pciedev)
{
	uint32 ioc_status = R_REG(pciedev->osh, PCIE_pcieiostatus(pciedev->autoregs));

	PCI_TRACE(("TIME:%u pciedev_handle_d0_enter \n", hnd_time()));

	/* XXX: D0 state check won't block the D0 handler execution.
	* Instead just keep a counter.
	*/
	if (!(ioc_status & PCIEGEN2_IOC_D0_STATE_MASK)) {
		PCI_ERROR(("%s:Enter D0:0x%x, 0x%04x\n",
			__FUNCTION__, ioc_status, (uint32)CALL_SITE));
		pciedev->bus_counters->iocstatus_no_d0 ++;
	}
	pciedev->real_d3 = FALSE;

	if ((R_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs)) & PCIE_RST) == 0) {
		PCI_ERROR(("pciedev_handle_d0_enter:Fake PERST# De-assert\n"));
		pciedev->bus_counters->fake_l0count ++;
		return;
	}

	/* Re-program the enumeration space registers and COE private registers */
	/* only if a core reset has happenned. */
	if (pciedev->pciecoreset) {
		pciedev_crwlpciegen2_180(pciedev);

		pciedev_set_LTRvals(pciedev);
		pciedev_crwlpciegen2(pciedev);
		pciedev_reg_pm_clk_period(pciedev);

		OSL_DELAY(1000);
	}
	else {
		/* D3hot case, remove WOP */
		pciedev_WOP_disable(pciedev, TRUE);

		++pciedev->metrics->d0_resume_ct;
		pciedev->metrics->d3_suspend_dur += OSL_SYSUPTIME() -
			pciedev->metric_ref->d3_suspend;
		pciedev->metric_ref->active = OSL_SYSUPTIME();
	}

	pciedev->device_link_state &= ~PCIEGEN2_PWRINT_D0_STATE_MASK;
	pciedev->bm_enabled = FALSE;
	if (pciedev_handle_d0_enter_bm(pciedev) != BCME_OK) {
		if (pciedev->hostready && !pciedev->bm_enabled) {
			PCI_ERROR(("HostReady received without BM Enabled!\n"));
			OSL_SYS_HALT();
		}
		if (!HOSTREADY_ONLY_ENAB()) {
			/* Start periodic check for Bus Master Enable */
			/* Do not start BM poll timer if hostready is enabled */
			if (!pciedev->hostready && !pciedev->bm_enab_timer_on) {
				dngl_add_timer(pciedev->bm_enab_timer,
					pciedev->bm_enab_check_interval, FALSE);
				pciedev->bm_enab_timer_on = TRUE;
			}
		}
		if (PCIECOREREV(pciedev->corerev) < 19) {
			AND_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ~PCIE_SPERST);
		}
	}
} /* pciedev_handle_d0_enter */

/**
 * XXX Fix for D3 ack intermittent failure
 * This temporarily called function contains two WARs. One has to do with ASPM/LTR, the other one
 * with bus mastering.
 *
 * Description of bus mastering WAR: Allow DMA over the PCIE bus only after host enables Bus
 * Mastering by setting bit 2 in the PCIE config reg 4. Also, certain WARs can be applied at this
 * point.
 *
 * Description of LTR WAR: for CRWLPCIEGEN2-178 ('H2D DMA corruption after L2_3 (perst assertion)'),
 * fixed in PCIe rev 14.
 *
 * RB:26620 WAR update based on Bus Master Enable from the Host (porting from RB26525 for 7.10)
 */
int
pciedev_handle_d0_enter_bm(struct dngl_bus *pciedev)
{
	uint32 perst_l = 0;

	perst_l = (R_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs)) & PCIE_RST);

	PCI_TRACE(("TIME:%u pciedev_handle_d0_enter_bm \n", hnd_time()));

	if (perst_l == 0) {
		/* XXX RB:30770 JIRA:SW4349-704 JIRA:SW4349-582 Chip hangs as PERST asserted without
		 * L2L3 interrupt due to firrmware having config space access.
		 */
		PCI_TRACE(("PERST_L ASSERTED 0x%x\n", perst_l));
		return BCME_ERROR;
	}
	PCI_TRACE(("%s\n", __FUNCTION__));

	if (PCIECOREREV(pciedev->corerev) == 16) {
		/* XXX
		 * HW problem: with rev16, new functionality was added, but that new functionality
		 * has a hardware bug, solved in later core revs. The new functionality is described
		 * by CRWLPCIEGEN2-250. The HW problem: when perst is asserted, the SW PME bit is
		 * not requesting the cfgclk, so pmePerFunction does not get synced in pciecore.
		 * WAR is to  immediately reset regPcieIntClkControl.forceCfgClkOnAlp (bit 3 in
		 * control reg 0x0) after setting the SW PME bit to allow the sync to happen.
		 * RB:57256, JIRA:SWWLAN-83963, Adding SW support for generate PME by setting PME
		 * register bit for PCIe rev 16
		 */
		AND_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ~PCIE_FORCECFGCLKON_ALP);
	}

	if (pciedev->ltr_sleep_after_d0) {
		/* at this point, ltr_sleep has to be sent */
		if (R_REG_CFG(pciedev, PCIECFGREG_PML1_SUB_CTRL1)
			& ASPM_L1_2_ENA_MASK) {
			if (pciedev_hw_LTR_war(pciedev)) { /* sends active+sleep LTRs to host */
				pciedev->ltr_sleep_after_d0 = FALSE;
			}
		}
	}

	if (!pciedev->bm_enabled) {
		if (R_REG_CFG(pciedev, PCI_CFG_CMD) & PCI_CMD_MASTER) {

			pciedev->bm_enabled = TRUE;
#if defined(BCMPCIE_IDMA)
			if (PCIE_IDMA_ACTIVE(pciedev) && PCIE_IDMA_DS(pciedev)) {
				pciedev_idma_channel_enable(pciedev, TRUE);
			}
#endif /* BCMPCIE_IDMA */
			pciedev_crwlpciegen2_182(pciedev);
			pciedev_crwlpciegen2(pciedev);

			pciedev_notify_devpwrstchg(pciedev, TRUE);
			pciedev->in_d3_suspend = FALSE;

			pciedev_manage_TREFUP_based_on_deepsleep(pciedev,
				PCIEDEV_DEEPSLEEP_DISABLED);

			pciedev->in_d3_pktcount_rx = 0;
			pciedev->in_d3_pktcount_evnt = 0;
			if (pciedev->bus_counters->hostwake_asserted) {
				/* Reset Host_wake signal */
				PCI_ERROR(("%s: Resetting hostwake signal\n", __func__));
				pciedev_host_wake_gpio_enable(pciedev, FALSE);
				pciedev->bus_counters->hostwake_asserted = FALSE;
				pciedev->bus_counters->hostwake_deassert_last = OSL_SYSUPTIME();
			}
			if (pciedev->pciecoreset) {
				pciedev->pciecoreset = FALSE;
				pciedev_crwlpciegen2_161(pciedev,
					pciedev->cur_ltr_state == LTR_ACTIVE ? TRUE : FALSE);
			}
			/* */
			pciedev_process_tx_payload(pciedev, pciedev->default_dma_ch);
			pciedev_queue_d2h_req_send(pciedev);
			/* Kick off the enqueued fetch requests */
			hnd_dmadesc_avail_cb();
			pciedev_msgbuf_intr_process(pciedev);  /* starts fetching messages */
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
			if (!pciedev->ds_inband_dw_supported)
#endif /* PCIEDEV_INBAND_DW */
			{
				pciedev_dw_check_after_bm(pciedev);
			}
#endif /* PCIE_DEEP_SLEEP */
		} else {
			PCI_ERROR(("pciedev_handle_d0_enter_bm: PCI_CMD_MASTER=0x%x\n",
				R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs))));
		}
	}

	/*
	* Continue with the timer if LTR is still not enabled
	* or Host did not enable Bus Master.
	*/
	if (pciedev->ltr_sleep_after_d0 || !pciedev->bm_enabled)
		return BCME_ERROR;

	return BCME_OK;
} /* pciedev_handle_d0_enter_bm */

/**
 * This function was introduced as a WAR for the 4345/4350:H2D DMA corruption after L2_3 (perst
 * assertion). It is called from various functions.
 * Since pcie core was reset during PERST, make sure to re-enable the
 * enumeration registers on L0.
 */
void
pciedev_handle_l0_enter(struct dngl_bus *pciedev)
{
	uint32 perst_l = 0;

	PCI_TRACE(("TIME:%u pciedev_handle_l0_enter \n", hnd_time()));

	perst_l = (R_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs)) & PCIE_RST);

	if (perst_l == 0) {
		PCI_ERROR(("PERST_L ASSERTED 0x%x\n", perst_l));
		pciedev->bus_counters->fake_l0count ++;
		return;
	}

	/* If core reset did not happen, exit */
	if (pciedev->pciecoreset == FALSE)
		return;

	PCI_TRACE(("pciedev_handle_l0_enter: 0x%x\n", pciedev->mailboxintmask));
	if (pciedev->mailboxintmask) {
		W_REG(pciedev->osh, PCIE_mailboxintmask(pciedev->autoregs),
			pciedev->mailboxintmask);
	}

	/* when in D0 or L0, host should be able to reset the dongle (including e.g. ARM core) */
	AND_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ~PCIE_SPERST);

	pciedev_send_ltr(pciedev, LTR_SLEEP);
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	if (pciedev->ds_inband_dw_supported) {
		pciedev_disable_deepsleep(pciedev, TRUE);
	}
	else
#endif /* PCIEDEV_INBAND_DW */
	if (pciedev->ds_oob_dw_supported) {
		pciedev_ob_deepsleep_engine(pciedev, PERST_DEASSRT_EVENT);
	}
#endif /* PCIE_DEEP_SLEEP */
	pciedev_handle_d0_enter(pciedev);
	pciedev_enable_powerstate_interrupts(pciedev);

	++pciedev->metrics->perst_deassrt_ct;
	pciedev->metrics->perst_dur += OSL_SYSUPTIME() - pciedev->metric_ref->perst;
	pciedev->metric_ref->perst = 0;
	pciedev->metric_ref->active = OSL_SYSUPTIME();

	++pciedev->metrics->d0_resume_ct;
	pciedev->metrics->d3_suspend_dur += OSL_SYSUPTIME() - pciedev->metric_ref->d3_suspend;
	pciedev->metric_ref->active = OSL_SYSUPTIME();
} /* pciedev_handle_l0_enter */

void
pciedev_enable_powerstate_interrupts(struct dngl_bus *pciedev)
{
	if (!(pciedev->defintmask & PD_DEV0_PWRSTATE_INTMASK))
		return;

	/* set the new interrupt request state	 */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_mask, pciedev->device_link_state);
}

static void
pciedev_clear_pwrstate_intmask(struct dngl_bus *pciedev)
{
	uint32 pwr_state;

	PCI_TRACE(("TIME:%u pciedev_clear_pwrstate_intmask \n", hnd_time()));
	pwr_state = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status);
	/* clear the interrupts by writing the same value back  */
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status, pwr_state);

	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_mask, 0);
}

static void
pciedev_handle_d3_enter(struct dngl_bus *pciedev)
{
	PCI_TRACE(("TIME:%u pciedev_handle_d3_enter \n", hnd_time()));
	pciedev->mailboxintmask = R_REG(pciedev->osh, PCIE_mailboxintmask(pciedev->autoregs));

	++pciedev->metrics->d3_suspend_ct;
	pciedev->metric_ref->d3_suspend = OSL_SYSUPTIME();
	pciedev->metrics->active_dur += OSL_SYSUPTIME() - pciedev->metric_ref->active;
	pciedev->metric_ref->active = 0;

	/* When the dongle transitions into D3cold, the only way for the host to get it in a D0
	 * state again is by asserting PERST#. If that happens, the dongle has to transition to D0,
	 * without resetting ARM/d11 core etc, in other words it has to 'survive PERST'.
	 */
	OR_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), PCIE_SPERST);

	pciedev->bus_counters->d3_srcount = pciedev->sr_count;
	pciedev->device_link_state &= ~PCIEGEN2_PWRINT_D3_STATE_MASK;
	pciedev->device_link_state |= PCIEGEN2_PWRINT_D0_STATE_MASK;
	pciedev->real_d3 = TRUE;

	/* Retry host wake if any packets queued in transition to D3 */
	pciedev_check_host_wake(pciedev, NULL);
}

int
pciedev_check_host_wake(struct dngl_bus *pciedev, void *p)
{
	int ret = BCME_OK;
	cmn_msg_hdr_t * msg;
	void * pkt = NULL;
	uint8 msgtype;
	uint32 d3_pktcount = pciedev->in_d3_pktcount_rx + pciedev->in_d3_pktcount_evnt;

	PCI_TRACE(("pciedev_check_host_wake: d3_pktcount=%d\n", d3_pktcount));

	if (pciedev->bus_counters->hostwake_asserted == TRUE)
		return ret;

	if (p != NULL)
		pkt = p;

	/* store host wake reason for analysis */
	if ((d3_pktcount || (pkt != NULL)) &&
			(pciedev->real_d3)) {

		if (!pktq_empty(&pciedev->d2h_req_q)) {
			pkt = pktqprec_peek(&pciedev->d2h_req_q, 0);
		}

		if (pkt == NULL) {
			PCI_ERROR(("False wake request\n"));
			ASSERT(0);
			return BCME_ERROR;
		}

		msg = (cmn_msg_hdr_t *)PKTDATA(pciedev->osh, pkt);
		msgtype = msg->msg_type;
		if (msgtype == MSG_TYPE_WL_EVENT) {
			pciedev->bus_counters->hostwake_reason = PCIE_HOSTWAKE_REASON_WL_EVENT;
		} else {
			pciedev->bus_counters->hostwake_reason = PCIE_HOSTWAKE_REASON_DATA;
		}

		/* Initiate host wake and send Host_Wake Signal */
		if (pciedev->bus_counters->hostwake_asserted == FALSE) {
			/* Send Host_Wake Signal */
			pciedev_host_wake_gpio_enable(pciedev, TRUE);
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
			if (pciedev->ds_inband_dw_supported) {
				pciedev_disable_deepsleep(pciedev, TRUE);
			}
			else
#endif /* PCIEDEV_INBAND_DW */
			if (pciedev->ds_oob_dw_supported) {
				pciedev_ob_deepsleep_engine(pciedev, HOSTWAKE_ASSRT_EVENT);
			}
#endif /* PCIE_DEEP_SLEEP */
			pciedev->bus_counters->hostwake_assert_count ++;
			pciedev->bus_counters->hostwake_asserted = TRUE;
			pciedev->bus_counters->hostwake_assert_last = OSL_SYSUPTIME();
			PCI_ERROR(("Assert Hostwake Signal in D3cold: Reason %d\n",
				pciedev->bus_counters->hostwake_reason));

			/* Save latest wakeup data, free previous one if any */
			if (pciedev->wd) {
				MFREE(pciedev->osh, pciedev->wd, pciedev->wd_len);
				pciedev->wd = NULL;
				pciedev->wd_len = 0;
			}
			pciedev->wd = MALLOCZ(pciedev->osh, pkttotlen(pciedev->osh, pkt));
			if (pciedev->wd == NULL) {
				PCI_ERROR(("Malloc failed for wakeup data\n"));
			} else {
				pciedev->wd_gpio_toggle_time =
					pciedev->bus_counters->hostwake_assert_last;
				pciedev->wd_len = pkttotlen(pciedev->osh, pkt);
				pktcopy(pciedev->osh, pkt, 0, pciedev->wd_len, pciedev->wd);
			}
		}
	}

	return ret;
}

static void
pciedev_handle_pwrmgmt_intr(struct dngl_bus *pciedev)
{
	uint32 pwr_state;

	pwr_state = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status);
	/* clear the interrupts by writing the same value back  */
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status, pwr_state);
	pwr_state &= pciedev->device_link_state;
	if (pwr_state == 0) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr: pwr_state=0x%x\n", pwr_state));
		return;
	}

	if (pwr_state & PCIEGEN2_PWRINT_D3_STATE_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:D3\n"));
		pciedev->bus_counters->d3count ++;
		pciedev_handle_d3_enter(pciedev);
	}

	if (pwr_state & PCIEGEN2_PWRINT_D0_STATE_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:D0: corereset:%s\n",
			pciedev->pciecoreset ? "Y":"N"));
		pciedev->bus_counters->d0count ++;
		/* Do not apply the D0 enter logic here if pcie core
		* is reset as part of HW JIRA: CRWLPCIEGEN2-178
		*
		*/
		/* Do not execute D0 handling logic here if hostready is enabled */
		if (!HOSTREADY_ONLY_ENAB() && !pciedev->hostready &&
			pciedev->pciecoreset == FALSE) {
			uint32 perst_l;
			uint32 intstat_perst;
			/*
			* Make sure that PERST is not already asserted when D0 is received.
			* Allow 5 ms before querying the pcie control registers.
			*/
			OSL_DELAY(5000);
			perst_l = (R_REG(pciedev->osh,
				PCIE_pciecontrol(pciedev->autoregs)) & PCIE_RST);
			intstat_perst = (R_REG(pciedev->osh,
				PCIE_intstatus(pciedev->autoregs)) & PCIE_PERST);
			if (perst_l == 0 && intstat_perst) {
				PCI_ERROR(("PERST_L ASSERTED:%d pwr_state:0x%x"
					"perst in instat:%d\n",
					perst_l, pwr_state, intstat_perst));
				pciedev->bus_counters->d0_in_perst ++;
				return;
			}
			pciedev_WOP_disable(pciedev, TRUE);
			pciedev_handle_d0_enter(pciedev);
		}

	}

	if (pwr_state & PCIEGEN2_PWRINT_L2_L3_LINK_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:L2_L3\n"));
		pciedev->bus_counters->l2l3_srcount = pciedev->sr_count;
		pciedev->bus_counters->l2l3count ++;
		pciedev->device_link_state &= ~PCIEGEN2_PWRINT_L2_L3_LINK_MASK;
		pciedev->device_link_state |= PCIEGEN2_PWRINT_L0_LINK_MASK;
	}
	if (pwr_state & PCIEGEN2_PWRINT_L0_LINK_MASK) {
		PCI_TRACE(("pciedev_handle_pwrmgmt_intr:L0\n"));
		PCI_ERROR(("PCIe PERST# Deasserted, active\n"));
		pciedev->bus_counters->l0count ++;
		pciedev->device_link_state |= PCIEGEN2_PWRINT_L2_L3_LINK_MASK;
		pciedev->device_link_state &= ~PCIEGEN2_PWRINT_L0_LINK_MASK;
		if (!HOSTREADY_ONLY_ENAB()) {
			/* Do not execute D3cold exit handling logic here if hostready
			* is enabled.
			*/
			if (!pciedev->hostready) {
				pciedev_WOP_disable(pciedev, TRUE);
				pciedev_handle_l0_enter(pciedev);
			}
		}
	}
	pciedev_enable_powerstate_interrupts(pciedev);
} /* pciedev_handle_pwrmgmt_intr */

#endif /* PCIE_PWRMGMT_CHECK */

#ifdef BCMPCIE_D2H_MSI
static void
pciedev_handle_msi_fifo_overflow_intr(struct dngl_bus *pciedev)
{
	/* reset FIFO */
	PCI_ERROR(("MSI FIFO overflow\n"));
	OR_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), PCIE_MSI_FIFO_CLEAR);
}
#endif /* BCMPCIE_D2H_MSI */

#ifdef PCIE_ERR_ATTN_CHECK
static void
pciedev_dump_err_attn_logs(struct dngl_bus *pciedev)
{
	uint32 err_hdr_logreg1, err_hdr_logreg2, err_hdr_logreg3;
	uint32 err_hdr_logreg4, err_code_logreg, err_type, err_code_state;

	err_type = R_REG_CFG(pciedev, PCI_CFG_TLCNTRL_5);

	err_hdr_logreg1 = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_hdr_logreg1);
	err_hdr_logreg2 = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_hdr_logreg2);
	err_hdr_logreg3 = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_hdr_logreg3);
	err_hdr_logreg4 = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_hdr_logreg4);
	err_code_logreg = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_code_logreg);

	PCI_PRINT(("ERROR ATTENTION LOGS!!!\n"));
	PCI_PRINT(("err_hdr_logreg1:0x%x, err_hdr_logreg2:0x%x\n",
			err_hdr_logreg1, err_hdr_logreg2));
	PCI_PRINT(("err_hdr_logreg3:0x%x, err_hdr_logreg4:0x%x\n",
			err_hdr_logreg3, err_hdr_logreg4));
	PCI_PRINT(("err_code_logreg:0x%x, err_type:0x%x \n", err_code_logreg, err_type));

	/* clear the enumeration reg by writing the same value back  */
	err_code_state = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_code_logreg);
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_code_logreg, err_code_state);

	OR_REG_CFG(pciedev, PCI_CFG_TLCNTRL_5, err_type);

	if (err_type & PD_ERR_UNSPPORT) {
		/*
		* Treat this as a non-fatal error for now. We'll clear it but
		* will just increment a counter.
		*/
		PCIEDEV_UNSUPPORTED_ERROR_CNT_INCR(pciedev);

		/*
		* Also, clear out the AER uncorrectable error register's
		* Unsupported Request bit.
		*/
		OR_REG_CFG(pciedev, PCI_UC_ERR_STATUS, PCI_UC_ERR_URES);
	}
}
#endif /* PCIE_ERR_ATTN_CHECK */

/** Called from RTE layer on an open() device call. Initializes pcie device, enables interrupts. */
void
pciedev_init(struct dngl_bus *pciedev)
{
	uint crwlpciegen2_117_disable = 0;
	uint32 ctrlflags;

	/* CRWLPCIEGEN2-117 pcie_pipe_Iddq should be controlled
	 * by the L12 state from MAC to save power by putting the
	 * SerDes analog in IDDQ mode
	 */
	if (PCIECOREREV(pciedev->corerev) == 9 || PCIECOREREV(pciedev->corerev) == 13 ||
		PCIECOREREV(pciedev->corerev) == 14) {
		crwlpciegen2_117_disable = PCIE_PipeIddqDisable0 | PCIE_PipeIddqDisable1;
	}

	ctrlflags = PCIE_DLYPERST | PCIE_DISSPROMLD | crwlpciegen2_117_disable;
	if (PCIECOREREV(pciedev->corerev) < 19) {
		ctrlflags |= PCIE_SPERST;
	} else {
		/* SurvivePerst POR value is '1', but the desired behavior is that if the host
		 * asserts PERST#, the entire dongle (including e.g. arm) should reset, except if
		 * the dongle is in D3cold state.
		 */
		AND_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ~PCIE_SPERST);
	}

	if ((PCIECOREREV(pciedev->corerev) == 16) || (PCIECOREREV(pciedev->corerev) >= 23)) {
		ctrlflags |= (PCIE_EN_MDIO_IN_PERST | PCIE_FORCECFGCLKON_ALP);
	}

	/* For 4347b0 TL_CLK_DETCT needs to enabled to access config space during perst */
	if ((PCIECOREREV(pciedev->corerev) == 23) || (PCIECOREREV(pciedev->corerev) == 24)) {
		ctrlflags |= PCIE_TL_CLK_DETCT;
	}

	OR_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ctrlflags);

	/* enable assertion of pcie_pipe_iddq during L2 state */
	if (pciedev->pipeiddq1_enab == 1) {
		AND_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), ~PCIE_PipeIddqDisable1);
	}

	pciedev->defintmask = PD_DEV0_INTMASK;
	if (!HOSTREADY_ONLY_ENAB()) {
		if (pciedev->db1_for_mb == PCIE_BOTH_MB_DB1_INTR)
			pciedev->mb_intmask = PD_DEV0_DB1_INTMASK | PD_FUNC0_MB_INTMASK;
		else if (pciedev->db1_for_mb == PCIE_DB1_INTR_ONLY)
			pciedev->mb_intmask = PD_DEV0_DB1_INTMASK;
		else if (pciedev->db1_for_mb == PCIE_MB_INTR_ONLY)
			pciedev->mb_intmask = PD_FUNC0_MB_INTMASK;

		pciedev->defintmask |= pciedev->mb_intmask;
	}
#ifdef BCMPCIE_D2H_MSI
	if ((PCIECOREREV(pciedev->corerev) == 16) ||
		(PCIECOREREV(pciedev->corerev) >= 23)) {
		pciedev->defintmask = pciedev->defintmask | PD_MSI_FIFO_OVERFLOW_INTMASK;
	}
#endif // endif

#ifdef PCIE_ERR_ATTN_CHECK
	if ((PCIECOREREV(pciedev->corerev) == 16) ||
		(PCIECOREREV(pciedev->corerev) >= 23)) {
		pciedev->defintmask = pciedev->defintmask | PD_ERR_ATTN_INTMASK |
				PD_LINK_DOWN_INTMASK;
	}
#endif /* PCIE_ERR_ATTN_CHECK */

#if defined(PCIE_DEEP_SLEEP) || defined(BCMPCIE_DAR) || defined(BCMPCIE_DMA_CH2)
	/* We are not interested in DEV2_DB interrupt except above compile cases */
	pciedev->defintmask |= PD_DEV2_INTMASK;
#endif // endif

	pciedev->intmask = pciedev->defintmask;

	pciedev->device_link_state = PCIEGEN2_PWRINT_L2_L3_LINK_MASK;
	pciedev_enable_powerstate_interrupts(pciedev);
	/* pciedev_enable power state interurpts */
	PCI_TRACE(("pciedev is on\n"));

#ifdef PCIE_DMA_INDEX
	pciedev->pcie_ipc->flags |= PCIE_IPC_FLAGS_DMA_INDEX;
#endif // endif
#ifdef PCIE_DMAINDEX16
	pciedev->pcie_ipc->flags |= PCIE_IPC_FLAGS_2BYTE_INDICES;
#endif // endif
#ifdef BCM_DHDHDR
	/* Advertize to DHD to insert SFH/LLCSNAP */
	pciedev->pcie_ipc->flags |= PCIE_IPC_FLAGS_DHDHDR;
#endif /* BCM_DHDHDR */
#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ENAB(pciedev)) {
		pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_IDMA;
	}
#endif /* BCMPCIE_IDMA */
#ifdef BCMHWA
	/* HWA IDMA capabilities */
	HWA_TXPOST_EXPR(pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_HWA_TXPOST);
	HWA_RXPOST_EXPR(pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_HWA_RXPOST_IDMA);
	HWA_TXCPLE_EXPR(pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_HWA_TXCPL_IDMA);
	HWA_RXCPLE_EXPR(pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_HWA_RXCPL_IDMA);
#ifdef RXCPL4
	HWA_RXCPLE_EXPR(pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_HWA_RXCPL4);
#endif /* RXCPL4 */
#endif /* BCMHWA */
#if defined(WLSQS)
	/* SQS requires ucast flowrings be maintained per TID, instead of per AC */
	pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_FLOWRING_TID; /* request DHD */
#endif // endif

#if defined(BCMPCIE_DAR) && !defined(BCMPCIE_DAR_DISABLED)
	pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_DAR;
#endif /* BCMPCIE_DAR */

	/* Support for HostReady */
	pciedev->pcie_ipc->flags |= PCIE_IPC_FLAGS_HOSTRDY_SUPPORT;

	/* If the device_wake GPIO is not setup, disable OOB device_wake */
	if (pciedev->ds_oob_dw_supported == 0) {
		pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_NO_OOB_DW;
	}
#ifdef PCIEDEV_INBAND_DW
	pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_INBAND_DS;
#endif /* PCIEDEV_INBAND_DW */

	pciedev->cur_ltr_state = LTR_SLEEP;
	/* Update LTR stat */
	++pciedev->metrics->ltr_sleep_ct;
	if (pciedev->metric_ref->ltr_active) {
		pciedev->metrics->ltr_active_dur += OSL_SYSUPTIME() -
			pciedev->metric_ref->ltr_active;
		pciedev->metric_ref->ltr_active = 0;
	}
	pciedev->metric_ref->ltr_sleep = OSL_SYSUPTIME();

	/* tell the host which form of D2H DMA Complete WAR is to be used */
#if defined(PCIE_M2M_D2H_SYNC)
	pciedev->pcie_ipc->flags |= PCIE_M2M_D2H_SYNC; /* PCIE_IPC_FLAGS */
#endif /* PCIE_M2M_D2H_SYNC */

	/* tell the host fw capability MSI and multiple-MSI */
#ifdef BCMPCIE_D2H_MSI
	pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_MSI_MULTI_MSG;
#endif /* BCMPCIE_D2H_MSI */

	/* Enable H2D, D2H dma interrupts */
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intmask_0, PD_DMA_INT_MASK_H2D);
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intmask_0, PD_DMA_INT_MASK_D2H);
#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ENAB(pciedev)) {
		/* DMA error, rx/tx interrupt mask */
		OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intmask_2,
			PD_DMA_INT_MASK_H2D);
		/* Flow ring group interrupt mask for all 16 groups */
		OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_frg_intmask_2,
			PD_IDMA_COMP);
	}
#endif /* BCMPCIE_IDMA */
#ifdef BCMPCIE_DMA_CH1
	if (PCIE_DMA1_ENAB(pciedev)) {
		OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intmask_1, PD_DMA_INT_MASK_H2D);
		OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intmask_1, PD_DMA_INT_MASK_D2H);
	}
#endif /* BCMPCIE_DMA_CH1 */
#ifdef BCMPCIE_DMA_CH2
	if (PCIE_DMA2_ENAB(pciedev)) {
		OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intmask_2, PD_DMA_INT_MASK_H2D);
		OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intmask_2, PD_DMA_INT_MASK_D2H);
	}
#endif /* BCMPCIE_DMA_CH2 */

#ifdef PCIEDEV_FAST_DELETE_RING
	/* Support for Fast Delete Ring */
	pciedev->pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_FAST_DELETE_RING;
#endif /* PCIEDEV_FAST_DELETE_RING */

#ifdef BCM_CSIMON
	/* Support for CSI monitor */
	pciedev->pcie_ipc->dcap2 |= PCIE_IPC_DCAP2_CSI_MONITOR;
#endif /* BCM_CSIMON */

	/* Turn on pcie core interrupt */
	pciedev_intrson(pciedev);
	pciedev->up = TRUE;

	/* TAF scheduler turned OFF by default */
	pciedev->taf_scheduler = FALSE;

#ifdef WLSQS
	/* Register a Callback function to trigger V2R request from TAF scheduler */
	wlc_sqs_pull_packets_register(sqs_v2r_request, pciedev);

	/* Register a call back function to trigger end of pull request set from TAF */
	wlc_sqs_eops_rqst_register(sqs_eops_rqst, pciedev);

	/* Register a callback function to report the flowring status */
	wlc_sqs_flowring_status_register(sqs_flow_ring_active, pciedev);
#endif /* WLSQS */
	if (BCMSPLITRX_ENAB())
		PCI_ERROR(("SPLITRX_MODE_%d enabled : tcmsegsize %d \n",
			BCMSPLITRX_MODE(), pciedev->tcmsegsz));
	else
		PCI_ERROR(("SPLITRX Disabled \n"));
} /* pciedev_init */

/**
 * Called after the PCIe core receives an interrupt, because:
 *     a. one of the DMA engines in the PCIe core requires attention OR
 *     b. a message was received from the host or
 *     c. a message was sent by this device to the host
 *
 * Updates and clears interrupt status bits. Returns TRUE if further (DPC) processing is required.
 */
bool
pciedev_dispatch(struct dngl_bus *pciedev)
{
	uint32 intstatus;
	uint32 h2dintstatus[MAX_DMA_CHAN], d2hintstatus[MAX_DMA_CHAN];	/* pcie dma devices */

	PCI_TRACE(("pciedev_dispatch\n"));

	ASSERT(pciedev->up);

	/* Main pcie interrupt */
	intstatus = R_REG(pciedev->osh, PCIE_intstatus(pciedev->autoregs));
	intstatus &= pciedev->defintmask;

	pciedev->intstatus |= intstatus;	/* Make sure we don't lose interrupts */
	if (pciedev->intstatus)
		W_REG(pciedev->osh, PCIE_intstatus(pciedev->autoregs), pciedev->intstatus);
	/*
	 * Optimize for the case where we don't have to look
	 * at the other dma interrupts if bit 7 of this intstatus is not set
	 */
	/* DMA interrupt */
	if ((PCIECOREREV(pciedev->corerev) == 16) ||
		(intstatus & PD_DEV0_DMA_INTMASK)) {
		h2dintstatus[DMA_CH0] = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0);
		d2hintstatus[DMA_CH0] = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0);
		/* Check if bit7 of intstatus set, but not any of the above 2 */

		PCI_TRACE(("intstatus is 0x%8x, h2dintstatus 0x%8x, d2hintstatus 0x%8x\n",
			R_REG(pciedev->osh, PCIE_intstatus(pciedev->autoregs)),
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0),
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0)));

		pciedev->h2dintstatus[DMA_CH0] = h2dintstatus[DMA_CH0] & PD_DMA_INT_MASK_H2D;
		pciedev->d2hintstatus[DMA_CH0] = d2hintstatus[DMA_CH0] & PD_DMA_INT_MASK_D2H;

		if (pciedev->h2dintstatus[DMA_CH0])
			W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0,
				pciedev->h2dintstatus[DMA_CH0]);
		if (pciedev->d2hintstatus[DMA_CH0])
			W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0,
				pciedev->d2hintstatus[DMA_CH0]);
#ifdef BCMPCIE_IDMA
		if (PCIE_IDMA_ACTIVE(pciedev)) {
			h2dintstatus[DMA_CH2] =
				R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_2);
			/* Check if bit7 of intstatus set, but not any of the above 2 */
			PCI_TRACE(("idma h2dintstatus2 0x%08x\n", h2dintstatus[DMA_CH2]));
			pciedev->h2dintstatus[DMA_CH2] =
				h2dintstatus[DMA_CH2] & PD_DMA_INT_MASK_H2D;
			if (pciedev->h2dintstatus[DMA_CH2]) {
				uint32 h2d_frg_intstat_2;

				h2d_frg_intstat_2 = R_REG(pciedev->osh,
					&pciedev->regs->u.pcie2.h2d_frg_intstat_2);
				/* Save iDMA transfer completed indices. */
				pciedev->idma_info->index = (h2d_frg_intstat_2 & PD_IDMA_COMP);

#if defined(BCMDBG) || defined(WLTEST)
			{
				uint32 i, index;

				i = 0;
				index = pciedev->idma_info->index;
				while (index) {
					if (index & 0x1) {
						/* interrupts per group statistic */
						pciedev->idma_info->metrics.intr_num[i]++;
					}
					index = index >> 1;
					i++;
				}
			}
#endif // endif
				/* Ack it */
				W_REG(pciedev->osh,
					&pciedev->regs->u.pcie2.h2d_intstat_2,
					h2dintstatus[DMA_CH2]);
				W_REG(pciedev->osh,
					&pciedev->regs->u.pcie2.h2d_frg_intstat_2,
					h2d_frg_intstat_2);
			}
		}
#endif /* BCMPCIE_IDMA */

#ifdef BCMPCIE_DMA_CH1
		if (PCIE_DMA1_ENAB(pciedev)) {
			h2dintstatus[DMA_CH1] =
				R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_1);
			d2hintstatus[DMA_CH1] =
				R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_1);
			/* Check if bit7 of intstatus set, but not any of the above 2 */
			PCI_TRACE(("dma1 h2dintstatus1 0x%08x, d2hintstatus1 0x%08x\n",
					h2dintstatus[DMA_CH1],
					d2hintstatus[DMA_CH1]));

			pciedev->h2dintstatus[DMA_CH1] =
				h2dintstatus[DMA_CH1] & PD_DMA_INT_MASK_H2D;
			pciedev->d2hintstatus[DMA_CH1] =
				d2hintstatus[DMA_CH1] & PD_DMA_INT_MASK_D2H;

			if (pciedev->h2dintstatus[DMA_CH1])
				W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_1,
					pciedev->h2dintstatus[DMA_CH1]);
			if (pciedev->d2hintstatus[DMA_CH1])
				W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_1,
					pciedev->d2hintstatus[DMA_CH1]);
		}
#endif /* BCMPCIE_DMA_CH1 */
#ifdef BCMPCIE_DMA_CH2
		if (PCIE_DMA2_ENAB(pciedev)) {
			h2dintstatus[DMA_CH2] =
				R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_2);
			d2hintstatus[DMA_CH2] =
				R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_2);
			/* Check if bit7 of intstatus set, but not any of the above 2 */
			PCI_TRACE(("dma2 h2dintstatus2 0x%08x, d2hintstatus2 0x%08x\n",
					h2dintstatus[DMA_CH2],
					d2hintstatus[DMA_CH2]));

			pciedev->h2dintstatus[DMA_CH2] =
				h2dintstatus[DMA_CH2] & PD_DMA_INT_MASK_H2D;
			pciedev->d2hintstatus[DMA_CH2] =
				d2hintstatus[DMA_CH2] & PD_DMA_INT_MASK_D2H;

			if (pciedev->h2dintstatus[DMA_CH2])
				W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_2,
					pciedev->h2dintstatus[DMA_CH2]);
			if (pciedev->d2hintstatus[DMA_CH2])
				W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_2,
					pciedev->d2hintstatus[DMA_CH2]);
		}
#endif /* BCMPCIE_DMA_CH2 */
		pciedev->intstatus &= ~PD_DEV0_DMA_INTMASK;
	}
	PCI_TRACE(("stored intst 0x%8x, h2dintst 0x%8x, d2hintst 0x%8x\n",
		intstatus, pciedev->h2dintstatus[DMA_CH0], pciedev->d2hintstatus[DMA_CH0]));

	if ((pciedev->intstatus |
			pciedev->h2dintstatus[DMA_CH0] | pciedev->d2hintstatus[DMA_CH0]) ||
#if defined(BCMPCIE_IDMA)
		(PCIE_IDMA_ACTIVE(pciedev) &&
		pciedev->h2dintstatus[DMA_CH2]) ||
#endif /* BCMPCIE_IDMA */
#ifdef BCMPCIE_DMA_CH2
		(PCIE_DMA2_ENAB(pciedev) &&
			(pciedev->h2dintstatus[DMA_CH2] | pciedev->d2hintstatus[DMA_CH2])) ||
#endif /* BCMPCIE_DMA_CH2 */
#ifdef BCMPCIE_DMA_CH1
		(PCIE_DMA1_ENAB(pciedev) &&
			(pciedev->h2dintstatus[DMA_CH1] | pciedev->d2hintstatus[DMA_CH1])) ||
#endif /* BCMPCIE_DMA_CH1 */
		0)	{
		/* One of the interrupt set */
	} else {
		DBG_BUS_INC(pciedev, zero_intstatus);
		return FALSE;
	}

	return TRUE;
} /* pciedev_dispatch */

static void
pciedev_reset_pcie_interrupt(struct dngl_bus *pciedev)
{
	PCI_TRACE(("pciedev_reset_pcie_interrupt\n"));

	/* door bell interrupt */
	W_REG(pciedev->osh, PCIE_intstatus(pciedev->autoregs), 0xFFFFFFFF);
	/* h2d dma */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_0, 0xFFFFFFFF);
	/* d2h dma */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_0, 0xFFFFFFFF);
#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ENAB(pciedev)) {
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_2, 0xFFFFFFFF);
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_frg_intstat_2, 0xFFFFFFFF);
	}
#endif /* BCMPCIE_IDMA */
#ifdef BCMPCIE_DMA_CH1
	if (PCIE_DMA1_ENAB(pciedev)) {
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_1, 0xFFFFFFFF);
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_1, 0xFFFFFFFF);
	}
#endif /* BCMPCIE_DMA_CH1 */
#ifdef BCMPCIE_DMA_CH2
	if (PCIE_DMA2_ENAB(pciedev)) {
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d_intstat_2, 0xFFFFFFFF);
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h_intstat_2, 0xFFFFFFFF);
	}
#endif /* BCMPCIE_DMA_CH2 */
	/* clear PCIE core pwrstate interrupts as well */
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.pwr_int_status, 0xFFFFFFFF);
}

/** Switch off interrupt mask */
void
pciedev_intrsoff(struct dngl_bus *pciedev)
{
	/* clear all device-side intstatus bits */
	PCI_TRACE(("pciedev_intrsoff\n"));
	/* disable dma complete interrupt in all the dma source for this core */
	/* disable dongle_2_host doorbell interrupts */
	AND_REG(pciedev->osh, PCIE_intmask(pciedev->autoregs), ~pciedev->intmask);
	pciedev->intmask = 0;
}

/** Deassert interrupt */
void
pciedev_intrs_deassert(struct dngl_bus *pciedev)
{
	/* disable dma complete interrupt in all the dma source for this core */
	/* disable dongle_2_host doorbell interrupts */
	AND_REG(pciedev->osh, PCIE_intmask(pciedev->autoregs), ~pciedev->defintmask);
}

/** Switch on pcie interrupts */
void
pciedev_intrson(struct dngl_bus *pciedev)
{
	PCI_TRACE(("pciedev_intrson\n"));
	pciedev->intmask = pciedev->defintmask;

	/* enable host_2_dongle doorbell interrupts */
	/* enable dongle_2_host doorbell interrupts */
	OR_REG(pciedev->osh, PCIE_intmask(pciedev->autoregs), pciedev->intmask);
}

#if defined(PCIE_D2H_DOORBELL_RINGER)

#if defined(SSWAR)
#error "SSWAR: is also using sbtopcie translation 0"
#endif // endif

/**
 * Soft doorbell to wake a Host side HW thread, by posting a WR transaction to
 * the PCIE complex via the SBTOPCIE Transl0. Host side PCIE RC will route the
 * address to a HW (Runner Network Processor's) register that will wake a
 * HW thread selected by the value. This is to avoid DHD to wakeup and forward
 * the interrupt to the Runner.
 */
void
pciedev_d2h_doorbell_ringer(struct dngl_bus *pciedev,
	uint32 db_data, uint32 db_addr)
{
	*((uint32*)db_addr) = db_data; /* write to host addr via sbtopcie transl0 */
}
#endif /* PCIE_D2H_DOORBELL_RINGER */

/**
 * Notifies host by dongle to host doorbell interrupt. Used for eg signalling when firmware has
 * consumed a host message.
 */
void
pciedev_generate_host_db_intr(struct dngl_bus *pciedev, uint32 data, uint32 db)
{
	uint32 ts = OSL_SYSUPTIME();

	/* make the data to be a wptr offset */
	PCI_TRACE(("generate the doorbell interrupt to host\n"));

	/* Update IPC Data */
	pciedev->metrics->num_d2h_doorbell++;

	if (pciedev->ipc_data_supported)
		pciedev->ipc_data->last_d2h_db_time = ts;

	IPC_LOG(pciedev, DOORBELL_D2H, D2H_DB, ts);

	/*
	 * This is a no-op in all cases other than when we are in device sleep
	 * or if we are in host sleep
	 */
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	if (pciedev->ds_inband_dw_supported) {
		pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_MSI_ISSUED);
	}
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
	if (db == PCIE_DB_DEV2HOST_0) {
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.dbls[0].dev2host_0, data);
		if (pciedev->clear_db_intsts_before_db) {
			/* Read back the doorbell register to make sure it is flushed */
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.dbls[0].dev2host_0);
			pciedev->clr_db_intsts_db++;
			if (!R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs))) {
				PCI_ERROR(("pciedev_generate_host_db_intr DB0 fail\n"));
				pciedev->clr_intstats_db_fail ++;
			}
			PCI_TRACE(("Mail box int before the clear is 0x%x\n",
				R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs))));
			W_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs),
				PCIE_MB_TOPCIE_D2H0_DB0);
			R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs));
			PCI_TRACE(("Mail box int after the clear is 0x%x\n",
				R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs))));
		}
		PCI_TRACE(("Mail box int after the read is 0x%x, mask is 0x%04x, ctrl cpl %d, %d\n",
			R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs)),
			R_REG(pciedev->osh, PCIE_mailboxintmask(pciedev->autoregs)),
			MSGBUF_RD(pciedev->dtoh_ctrlcpl),
			MSGBUF_WR(pciedev->dtoh_ctrlcpl)));
	} else if (db == PCIE_DB_DEV2HOST_1) {
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.dbls[0].dev2host_1, data);
		if (pciedev->clear_db_intsts_before_db) {
			/* Read back the doorbell register to make sure it is flushed */
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.dbls[0].dev2host_1);
			W_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs),
				PCIE_MB_TOPCIE_D2H0_DB1);
			R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs));
		}
	}
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	if (!pciedev->ds_inband_dw_supported &&
		pciedev->ds_oob_dw_supported &&
		!pciedev->in_d3_suspend) {
#else
	if (pciedev->ds_oob_dw_supported && !pciedev->in_d3_suspend) {
#endif /* PCIEDEV_INBAND_DW */
		pciedev_ob_deepsleep_engine(pciedev, DB_TOH_EVENT);
	}
#endif /* PCIE_DEEP_SLEEP */

}

/** notifies host of eg firmware halt, d3 ack, deep sleep */
static void
pciedev_generate_host_mb_intr(struct dngl_bus *pciedev)
{
#ifdef BCMPCIE_D2H_MSI
	if (PCIE_MSI_ENAB(pciedev->d2h_msi_info)) {
		pciedev_generate_msi_intr(pciedev,
			pciedev->d2h_msi_info.msi_vector_offset[MSI_INTR_IDX_MAILBOX]);
	} else
#endif /* BCMPCIE_D2H_MSI */
	{
		W_REG(pciedev->osh, PCIE_sbtopciemailbox(pciedev->autoregs),
			(0x1 << SBTOPCIE_MB_FUNC0_SHIFT));
	}
}

void
pciedev_generate_msi_intr(struct dngl_bus * pciedev, uint8 offset)
{
	switch (offset) {
		case BCMPCIE_D2H_MSI_OFFSET_MB0:
			W_REG(pciedev->osh, PCIE_sbtopciemailbox(pciedev->autoregs),
				(0x1 << SBTOPCIE_MB_FUNC0_SHIFT));
			PCI_TRACE(("generate the doorbell interrupt to host\n"));
			pciedev->metrics->num_d2h_doorbell++;
#ifdef PCIE_DEEP_SLEEP
			if (pciedev->ds_oob_dw_supported) {
				pciedev_ob_deepsleep_engine(pciedev, DB_TOH_EVENT);
			}
#endif /* PCIE_DEEP_SLEEP */
			break;
		case BCMPCIE_D2H_MSI_OFFSET_MB1:
			W_REG(pciedev->osh, PCIE_sbtopciemailbox(pciedev->autoregs),
				(0x1 << SBTOPCIE_MB1_FUNC0_SHIFT));
			PCI_TRACE(("generate the doorbell interrupt to host\n"));
			pciedev->metrics->num_d2h_doorbell++;
#ifdef PCIE_DEEP_SLEEP
			if (pciedev->ds_oob_dw_supported) {
				pciedev_ob_deepsleep_engine(pciedev, DB_TOH_EVENT);
			}
#endif /* PCIE_DEEP_SLEEP */
			break;
		case BCMPCIE_D2H_MSI_OFFSET_DB0:
			pciedev_generate_host_db_intr(pciedev,
				PCIE_D2H_DB0_VAL, PCIE_DB_DEV2HOST_0);
			break;
		case BCMPCIE_D2H_MSI_OFFSET_DB1:
			pciedev_generate_host_db_intr(pciedev,
				PCIE_D2H_DB0_VAL, PCIE_DB_DEV2HOST_1);
			break;
		default:
			PCI_ERROR(("msi invalid vector offset : %d\n", offset));
			break;
	}
}

/** Section on PCIE IPC: SETUP, BIND and LINK phases */

/**
 * pciedev_setup_pcie_ipc - Dongle-Host Bus layer training.
 *
 * Setup the Bus relevant fields in the PCIe IPC structure.
 * OS layer fields will be initialized in RTE layer - see hnd_shared_xyz_init()
 * OS layer will zero out the entire pcie_ipc_t structure.
 *
 * Allocate and initialize all other PCIE IPC Ring and Messaging objects.
 *     1. struct pcie_ipc_rings
 *     2. pcie_ipc_ring_mem_t array[ max_h2drings + max_d2h_rings ]
 *     3. Array of all H2D ring's WR indices
 *     4. Array of all H2D ring's RD indices
 *     5. Array of all D2H ring's WR indices
 *     6. Array of all D2H ring's RD indices
 *     7. H2D 32bit Mailbox messaging
 *     8. D2H 32bit Mailbox messaging
 *
 * Each of the above 8 objects need not be carved out of one contiguous block.
 * The order in which the above 8 objects are placed is not relevant.
 *
 *  Array of indices in dongle's memory may be:
 *  1. Directly accessed by Host (PIO)
 *  2. Dongle DMAs indices to/from host when dmaindex## feature is built
 *  3. Dongle may use SBTOPCIE to transfer indices
 *     DMA indices will be used for H2D WR array
 *  4. Implicit DMA HW for H2D WR indices
 *  5. Implicit FlowRing manager for H2D WR indices
 *  6. HWA 1a, 2b, 3a, 4b blocks
 *
 * Note: If the indices arrays for (H2D WR and D2H RD) and (H2D RD and D2H WR)
 * were placed together, then an optimization in DMAINDEX  would allow for
 * a "single DMA of H2D_WR and D2H_RD", and a "single DMA of H2D RD and D2H WR"
 * Both Host and Dongle would need to obey this pairing of arrays. Presently,
 * no assumption is made and each array will be DMAed independently.
 *
 */
static int
BCMATTACHFN(pciedev_setup_pcie_ipc)(struct dngl_bus *pciedev)
{
	daddr32_t daddr32;
	uint32 malloc_size;
	pcie_ipc_t *pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings;

	/* Size of a WR or RD index is defined by "dmaindex16" or "dmaindex32" */
	const uint32 arrays_per_dir = 2; /* WR and RD indices arrays */
	const uint32 index_sz = PCIE_RW_INDEX_SZ; /* sizeof(rw_index_sz_t) */
	const uint32 h2d_index_array_sz = (index_sz * pciedev->max_h2d_rings);
	const uint32 d2h_index_array_sz = (index_sz * pciedev->max_d2h_rings);
	const uint32 total_rings = (pciedev->max_h2d_rings + pciedev->max_d2h_rings);
	const uint32 num_mailbox = 2; /* H2D and D2H */
	const uint32 size_mailbox = sizeof(uint32);

	ASSERT(pciedev->max_h2d_rings != 0);
	ASSERT(pciedev->max_d2h_rings != 0);
	ASSERT(pciedev->max_flowrings != 0);

	/* Fetch the pcie_ipc_t from RTE OS. OS parts will be already initialized */
	pcie_ipc = hnd_get_pcie_ipc();

	/* DATAFILL 2. struct pcie_ipc with bus side info */

	/* Validate PCIE IPC Revision set by RTE */
	ASSERT(hnd_get_pcie_ipc_revision() == PCIE_IPC_REVISION);
	ASSERT(PCIE_IPC_REV_GET(pcie_ipc->flags) == PCIE_IPC_REVISION);

	/* Minimum backward compatible default Dongle Host Driver PCIE IPC.
	 * Newer hosts will update revision in hcap1.
	 * Legacy hosts are unaware of hcap1 and default value will be returned.
	 */
	PCIE_IPC_REV_SET(pcie_ipc->hcap1, PCIE_IPC_DEFAULT_HOST_REVISION);

#ifdef BCMFRAGPOOL
	/* Advertize the total Tx packet pools in dongle */
	pcie_ipc->max_tx_pkts = SHARED_FRAG_POOL_LEN;
#endif // endif
#if defined(BCMHWA) && defined(HWA_TXPOST_BUILD)
	pcie_ipc->max_tx_pkts = HWA_TXPATH_PKTS_MAX;
#endif // endif
#if defined(BCMRXFRAGPOOL) || defined(HWA_PKTPGR_BUILD)
	/* Advertize the total Rx packet pools in dongle */
	ASSERT(pciedev->tunables[MAXHOSTRXBUFS] >= 1);
	pcie_ipc->max_rx_pkts = pciedev->tunables[MAXHOSTRXBUFS];
#endif /* BCMFRAGPOOL || HWA_PKTPGR_BUILD */
	pcie_ipc->dma_rxoffset = 0U; /* to be deprecated */

	/* Allocate a contiguous memory for all PCIe IPC rings and messaging */

	/* 1. struct pcie_ipc_rings */
	malloc_size = sizeof(pcie_ipc_rings_t);

	/* 2. pcie_ipc_ring_mem_t array[ max_h2drings + max_d2h_rings ] */
	malloc_size += (sizeof(pcie_ipc_ring_mem_t) * total_rings);

	/* Space two arrays, one for WR indices and another for RD indices
	 * 3. Array of all H2D ring's WR indices
	 * 4. Array of all H2D ring's RD indices
	 */
	malloc_size += (h2d_index_array_sz * arrays_per_dir);

	/* Space two arrays, one for WR indices and another for RD indices
	 * 5. Array of all D2H ring's WR indices
	 * 6. Array of all D2H ring's RD indices
	 */
	malloc_size += (d2h_index_array_sz * arrays_per_dir);

	/* Space for two 32bit mailboxes, one per direction
	 * 7. H2D Mailbox
	 * 8. D2H Mailbox
	 */
	malloc_size += (size_mailbox * num_mailbox);

	/* Allocate memory for all the PCIE IPC Rings and Messaging objects */
	if ((daddr32 = (daddr32_t) MALLOCZ(pciedev->osh, malloc_size)) == NULL) {
		PCI_TRACE(("Setup PCIE IPC: alloc size %d fail\n", malloc_size));
		return BCME_ERROR;
	}

	/** --- Carve out sub-sections from the contiguous memory --- */

	/* CARVE 1. struct pcie_ipc_rings */
	pcie_ipc->rings_daddr32 = daddr32;
	daddr32 += sizeof(pcie_ipc_rings_t);

	pcie_ipc_rings = (pcie_ipc_rings_t *)pcie_ipc->rings_daddr32;

	/* CARVE 2. pcie_ipc_ring_mem_t array */
	pcie_ipc_rings->ring_mem_daddr32 = daddr32;
	daddr32 += (sizeof(pcie_ipc_ring_mem_t) * total_rings);

	/* CARVE 3. Array of all H2D ring's WR indices */
	pcie_ipc_rings->h2d_wr_daddr32 = daddr32;
	daddr32 += h2d_index_array_sz;

	/* CARVE 4. Array of all H2D ring's RD indices */
	pcie_ipc_rings->h2d_rd_daddr32 = daddr32;
	daddr32 += h2d_index_array_sz;

	/* CARVE 5. Array of all D2H ring's WR indices */
	pcie_ipc_rings->d2h_wr_daddr32 = daddr32;
	daddr32 += d2h_index_array_sz;

	/* CARVE 6. Array of all D2H ring's RD indices */
	pcie_ipc_rings->d2h_rd_daddr32 = daddr32;
	daddr32 += d2h_index_array_sz;

	/* CARVE 7. H2D mailbox */
	pcie_ipc->h2d_mb_daddr32 = daddr32;
	daddr32 += sizeof(uint32);

	/* CARVE 8. D2H mailbox */
	pcie_ipc->d2h_mb_daddr32 = daddr32;
	daddr32 += sizeof(uint32);

	/** --- DATAFILL subsections --- */

	/* DATAFILL 2. struct pcie_ipc_rings */
	pcie_ipc_rings->max_h2d_rings  = pciedev->max_h2d_rings;
	pcie_ipc_rings->max_d2h_rings  = pciedev->max_d2h_rings;
	pcie_ipc_rings->max_flowrings  = pciedev->max_flowrings;
#if defined(MBSS_MAXSLAVES)
	/* primary interface + MAXSLAVES */
	pcie_ipc_rings->max_interfaces = MBSS_MAXSLAVES + 1;
#elif defined(MAXVSLAVEDEVS)
	/* primary interface + MAXVSLAVES */
	pcie_ipc_rings->max_interfaces = MAXVSLAVEDEVS + 1;
#else
	pcie_ipc_rings->max_interfaces = 2;
#endif // endif

#ifdef BCMPCIE_IPC_ACWI
	pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_ACWI;
	/* Force to use TXPOST::CWI64 */
	pcie_ipc->flags |= PCIE_IPC_FLAGS_NO_TXPOST_CWI32;
	pcie_ipc_rings->txpost_format = MSGBUF_WI_COMPACT;
	pcie_ipc_rings->rxpost_format = MSGBUF_WI_COMPACT;
	pcie_ipc_rings->txcpln_format = MSGBUF_WI_COMPACT;
	pcie_ipc_rings->rxcpln_format = MSGBUF_WI_COMPACT;

#ifdef BCMHWA
	/* Config HWA aggregate format according to the HWA_PCIEIPC_WI_AGGR_CNT */
	HWA_TXCPLE_EXPR(pcie_ipc_rings->txcpln_format = (HWA_PCIEIPC_WI_AGGR_CNT > 0) ?
		MSGBUF_WI_AGGREGATE : MSGBUF_WI_COMPACT);
	HWA_RXCPLE_EXPR(pcie_ipc_rings->rxcpln_format = (HWA_PCIEIPC_WI_AGGR_CNT > 0) ?
		MSGBUF_WI_AGGREGATE : MSGBUF_WI_COMPACT);
#endif /* BCMHWA */
#else /* ! BCMPCIE_IPC_ACWI */
	pcie_ipc->dcap1 &= ~PCIE_IPC_DCAP1_ACWI;
	pcie_ipc_rings->txpost_format = MSGBUF_WI_LEGACY;
	pcie_ipc_rings->rxpost_format = MSGBUF_WI_LEGACY;
	pcie_ipc_rings->txcpln_format = MSGBUF_WI_LEGACY;
	pcie_ipc_rings->rxcpln_format = MSGBUF_WI_LEGACY;
#endif /* ! BCMPCIE_IPC_ACWI */

	/*
	 * When .11 to .3 is supported in MAC (RxSplitMode4), a fixed dataoffset
	 * of (FIFO #0 RxStatus = 4 Bytes) will be placed per 802.3 packet sent to
	 * host. When .11 to .3 is via driver, the dataoffset (start of .3 pkt in
	 * host rx buffer will not be fixed and would need to be conveyed to host
	 * via the RxCompletion Work Item (to include cwi_t and acwi_t formats
	 */
#ifdef RXSPLITMODE4
#error "FIXME RxSplitMode4 settings ... "
	pcie_ipc->flags |= PCIE_IPC_FLAGS_MAC_D11TOD3;
	pcie_ipc_rings->rxcpln_dataoffset = 8;
#else /* ! RXSPLITMODE4 */
	/* Software based .11 to .3 conversion (Rx SplitMode-2) */
	// pcie_ipc->dcap1 |= PCIE_IPC_DCAP1_MAC_D3TOD11;
	pcie_ipc_rings->rxcpln_dataoffset = ~0; /* data offset in rxcpln */
#endif /* ! RXSLITMODE4 */

	return BCME_OK;

} /* pciedev_setup_pcie_ipc */

/**
 * Bind the bus layer to the initialized PCIE IPC
 * Bind bus layer subsystems:
 *    H2D Common rings, D2H Common rings, dmaindex, iDMA, iFRM
 * Bind Host memory Extension Service
 */
static int
BCMATTACHFN(pciedev_bind_pcie_ipc)(struct dngl_bus *pciedev)
{
	uint16 ringid;
	pcie_ipc_t *pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings;
	pcie_ipc_ring_mem_t *pcie_ipc_ring_mem;

	pcie_ipc = hnd_get_pcie_ipc();
	pcie_ipc_rings = (pcie_ipc_rings_t*)pcie_ipc->rings_daddr32;
	pcie_ipc_ring_mem = (pcie_ipc_ring_mem_t*)pcie_ipc_rings->ring_mem_daddr32;

	/* Bind the pciedev bus layer to the initialized PCIE IPC */
	pciedev->pcie_ipc = pcie_ipc;
	pciedev->host_pcie_ipc_rev = PCIE_IPC_REV_GET(pcie_ipc->hcap1);

	pciedev->h2d_mb_data_ptr = (uint32 *)pcie_ipc->h2d_mb_daddr32;
	pciedev->d2h_mb_data_ptr = (uint32 *)pcie_ipc->d2h_mb_daddr32;

	pciedev->rxcpln_dataoffset = pcie_ipc_rings->rxcpln_dataoffset;

	/*
	 *  Post Rev5 support for new "dynamic" rings. Note TxPost flowrings may
	 *  be considered as dynamic h2D rings as they are explicitly created.
	 *
	 *  All non-dynamic common rings are created during the initial dongle host
	 *  training/handshake.
	 *
	 *  CAUTION: pcie_ipc_ring_mem is indexed by a ring's "id" field for common
	 *  rings. For dynamic ring's the ring id should be used in combination with
	 *  the base h2d_dyn_pcie_ipc_ring_mem and d2h_dyn_pcie_ipc_ring_mem.
	 *  A TxPost flowring is considered a dynamic H2D ring and flowid (saved in
	 *  msgbuf_ring::id is H2D scoped - w.r.t h2d_dyn_pcie_ipc_ring_mem).
	 *
	 *  Sections in the array of pcie_ipc_ring_mem:
	 *      1. H2D Common Rings (Ctrl Submit, RxBufPost)
	 *      2. D2H Common Rings (CtrlComplete, TxComplete, RxComplete)
	 *      3. H2D Flowrings and any other H2D Dynamic rings
	 *      4. D2H Dynamic Rings (including rings for per AC RxCpln)
	 *
	 */
	pciedev->h2d_cmn_pcie_ipc_ring_mem = /* H2D Common */
		pcie_ipc_ring_mem;

	pciedev->d2h_cmn_pcie_ipc_ring_mem = /* D2H Common */
		pciedev->h2d_cmn_pcie_ipc_ring_mem + BCMPCIE_H2D_COMMON_MSGRINGS;

	pciedev->h2d_dyn_pcie_ipc_ring_mem = /* H2D Dynamic */
		pciedev->d2h_cmn_pcie_ipc_ring_mem + BCMPCIE_D2H_COMMON_MSGRINGS;

	pciedev->d2h_dyn_pcie_ipc_ring_mem = /* D2H Dynamic */
		pciedev->h2d_dyn_pcie_ipc_ring_mem +
		(pcie_ipc_rings->max_h2d_rings - BCMPCIE_H2D_COMMON_MSGRINGS);

	/* Bind the bus common msgbuf_ring to the PCIE IPC */
	for (ringid = 0; ringid < BCMPCIE_H2D_COMMON_MSGRINGS; ringid++)
	{
		if (pciedev_h2d_msgbuf_bind_pcie_ipc(pciedev, ringid,
			&pciedev->cmn_msgbuf_ring[ringid], COMMON_RING) != BCME_OK)
		{
			PCI_ERROR(("Bind H2D msgbuf common ring failure\n"));
			return BCME_ERROR;
		}
	}

	for (ringid = BCMPCIE_H2D_COMMON_MSGRINGS;
	        ringid < BCMPCIE_COMMON_MSGRINGS; ringid++)
	{
		if (pciedev_d2h_msgbuf_bind_pcie_ipc(pciedev,
			(ringid - BCMPCIE_H2D_COMMON_MSGRINGS),
		    &pciedev->cmn_msgbuf_ring[ringid], COMMON_RING) != BCME_OK)
		{
			PCI_ERROR(("Bind D2H msgbuf common ring failure\n"));
			return BCME_ERROR;
		}
	}

	/* Now, start binding each bus layer feature */

#ifdef PCIE_DMA_INDEX
	/* Bind the bus dmaindex feature to the dongle side PCIE IPC */
	if (pciedev_dmaindex_bind_pcie_ipc(pciedev) != BCME_OK) {
		PCI_ERROR(("Bind dmaindex failure\n"));
		return BCME_ERROR;
	}
#endif /* PCIE_DMA_INDEX */

#ifdef BCMPCIE_IDMA
	/* Bind the bus idma feature to the dongle side PCIE IPC */
	if (pciedev_idma_bind_pcie_ipc(pciedev) != BCME_OK) {
		PCI_ERROR(("Bind iDMA failure\n"));
		return BCME_ERROR;
	}
#endif /* BCMPCIE_IDMA */

#ifdef BCMHME
	if (hme_bind_pcie_ipc((void*)pciedev, pcie_ipc) != BCME_OK) {
		PCI_ERROR(("Bind HME failure\n"));
		return BCME_ERROR;
	}
#endif /* BCMHME */

	return BCME_OK;

} /* pciedev_bind_pcie_ipc */

/**
 * Bind a bus layer msgbuf_ring to the PCIE IPC Ring Mem shared structures.
 * - pointer into array of pcie_ipc_ring_mem_t
 * - pointer into array of WR indices
 * - pointer into array of RD indices
 *
 * NOTE: parameter index is the per direction H2D and D2H scoped ring index.
 *
 * Do not use the msgbuf_ring::ringid in these two bind functions. The ringid
 * is global scoped for Common rings and per direction scoped for Dynamic rings.
 *
 * E.g. The first D2H ring ControlCompletion ringid is 2 and not 0, and the
 * first two Dynamic D2H rings will have the same ringids as the D2H Common
 * rings, namely TxCpln and RxCpln.
 */

/** Bind bus layer H2D msgbuf_ring to the PCIe IPC dongle side */
static int
pciedev_h2d_msgbuf_bind_pcie_ipc(struct dngl_bus *pciedev,
	uint16 ringid, msgbuf_ring_t *ring, bool is_common_ring)
{
	pcie_ipc_t *pcie_ipc = pciedev->pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings = (pcie_ipc_rings_t *)pcie_ipc->rings_daddr32;
	pcie_ipc_ring_mem_t *msgbuf_ipc_ring_mem = NULL;

	if (ringid >= pciedev->max_h2d_rings)
		return BCME_BADARG;

	/* Setup the pointer into the array of pcie_ipc_ring_mem */
	if (is_common_ring) {
		if (ringid >= BCMPCIE_H2D_COMMON_MSGRINGS)
			return BCME_BADARG;
		msgbuf_ipc_ring_mem = pciedev->h2d_cmn_pcie_ipc_ring_mem + ringid;
	} else {
		if (ringid >= pciedev->max_h2d_rings)
			return BCME_BADARG;
		msgbuf_ipc_ring_mem = pciedev->h2d_dyn_pcie_ipc_ring_mem +
			(ringid - BCMPCIE_H2D_COMMON_MSGRINGS);
	}

	/* Bind msgbuf to the shared pcie_ipc_ring_mem object */
	MSGBUF_IPC_RING_MEM(ring) = msgbuf_ipc_ring_mem;

	/* Setup pointers to WR and RD index in the array of indices */
	MSGBUF_WR_PTR(ring) = (pcie_rw_index_t *)
		(pcie_ipc_rings->h2d_wr_daddr32 + (sizeof(pcie_rw_index_t) * ringid));
	MSGBUF_RD_PTR(ring) = (pcie_rw_index_t *)
		(pcie_ipc_rings->h2d_rd_daddr32 + (sizeof(pcie_rw_index_t) * ringid));

	PCI_TRACE(("h2d%d: pcie_ipc_ring_mem %p, wr_ptr %p, rd_ptr %p \n", ringid,
		MSGBUF_IPC_RING_MEM(ring), MSGBUF_WR_PTR(ring), MSGBUF_RD_PTR(ring)));

	return BCME_OK;

} /* pciedev_h2d_msgbuf_bind_pcie_ipc */

/* Bind a bus layer D2H msgbuf_ring to the PCIE IPC dongle side */
static int
pciedev_d2h_msgbuf_bind_pcie_ipc(struct dngl_bus *pciedev, uint16 ringid,
	msgbuf_ring_t *ring, bool is_common_ring)
{
	pcie_ipc_t *pcie_ipc = pciedev->pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings = (pcie_ipc_rings_t *)pcie_ipc->rings_daddr32;
	pcie_ipc_ring_mem_t *msgbuf_ipc_ring_mem = NULL;

	/* Setup the pointer into the array of pcie_ipc_ring_mem */
	if (is_common_ring) {
		if (ringid > BCMPCIE_D2H_COMMON_MSGRINGS)
			return BCME_BADARG;
		msgbuf_ipc_ring_mem = pciedev->d2h_cmn_pcie_ipc_ring_mem + ringid;
	} else {
		if (ringid > pciedev->max_d2h_rings)
			return BCME_BADARG;
		msgbuf_ipc_ring_mem = pciedev->d2h_dyn_pcie_ipc_ring_mem +
			(ringid - BCMPCIE_D2H_COMMON_MSGRINGS);
	}

	/* Bind msgbuf to the shared pcie_ipc_ring_mem object */
	MSGBUF_IPC_RING_MEM(ring) = msgbuf_ipc_ring_mem;

	/* Setup pointers to WR and RD index in the array of indices */
	MSGBUF_WR_PTR(ring) = (pcie_rw_index_t *)
		(pcie_ipc_rings->d2h_wr_daddr32 + (sizeof(pcie_rw_index_t) * ringid));
	MSGBUF_RD_PTR(ring) = (pcie_rw_index_t *)
		(pcie_ipc_rings->d2h_rd_daddr32 + (sizeof(pcie_rw_index_t) * ringid));

	PCI_TRACE(("d2h%d: pcie_ipc_ring_mem %p, wr_ptr %p, rd_ptr %p \n", ringid,
		MSGBUF_IPC_RING_MEM(ring), MSGBUF_WR_PTR(ring), MSGBUF_RD_PTR(ring)));

	return BCME_OK;

} /* pciedev_d2h_msgbuf_bind_pcie_ipc */

/**
 * Link the bus layer to the PCIE IPC host side information. This is the entry
 * point after the host has datafilled it's PCIE IPC information.
 */
static int
pciedev_link_pcie_ipc(struct dngl_bus *pciedev)
{
	int ret;
	uint32 mailboxint;
	pcie_ipc_t *pcie_ipc = pciedev->pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings = (pcie_ipc_rings_t*)pcie_ipc->rings_daddr32;

	pciedev->host_pcie_ipc_rev = PCIE_IPC_REV_GET(pcie_ipc->hcap1);

	if (IS_DEFAULT_HOST_PCIE_IPC(pciedev))
	{
		pcie_ipc_rings->txpost_format = MSGBUF_WI_LEGACY;
		pcie_ipc_rings->rxpost_format = MSGBUF_WI_LEGACY;
		pcie_ipc_rings->txcpln_format = MSGBUF_WI_LEGACY;
		pcie_ipc_rings->rxcpln_format = MSGBUF_WI_LEGACY;
	}

	/* Link bus layer to host side PCIE IPC */
	pciedev->rxpost_data_buf_len = pcie_ipc_rings->rxpost_data_buf_len;
	/* Setup the host phys addr hi for MAC DMA access over PCIE */
	pciedev->host_physaddrhi =
		ltoh32(pcie_ipc->host_physaddrhi) | PCIE_ADDR_OFFSET;

	switch (pcie_ipc_rings->txpost_format) {
		case MSGBUF_WI_LEGACY:
			pciedev->txpost_item_type = MSGBUF_WI_WI64;
			pciedev->txpost_item_size = sizeof(host_txbuf_post_t);
			ASSERT(sizeof(host_txbuf_post_t) == H2DRING_TXPOST_ITEMSIZE);
			break;
		case MSGBUF_WI_COMPACT:
			/* Use TXPOST::CWI64 always */
			pciedev->txpost_item_type = MSGBUF_WI_CWI64;
			pciedev->txpost_item_size = sizeof(hwa_txpost_cwi64_t);
			ASSERT(sizeof(hwa_txpost_cwi64_t) == HWA_TXPOST_CWI64_BYTES);
			break;
		default:
			PCI_ERROR(("Invalid PCIe IPC TxPost format %u\n",
				pcie_ipc_rings->txpost_format));
			ASSERT(0);
			ret = BCME_BADOPTION;
			goto link_failure;
	}

	PCI_INFORM(("TxPost flowrings item type %u size %u\n",
		pciedev->txpost_item_type, pciedev->txpost_item_size));

#ifdef PCIEDEV_FAST_DELETE_RING
	/* Host acknowledges ability to handle fast ring delete */
	if (pcie_ipc->hcap1 & PCIE_IPC_HCAP1_FAST_DELETE_RING) {
		PCI_TRACE(("Receive Fast Delete Ring indication\n"));
		pciedev->fastdeletering = TRUE;
	}
#endif /* PCIEDEV_FAST_DELETE_RING */

#ifdef BCM_CSIMON
	/* Host acknowledges ability to handle CSI monitor */
	if (pcie_ipc->hcap2 & PCIE_IPC_HCAP2_CSI_MONITOR) {
		PCI_PRINT(("Receive CSI Monitor indication\n"));
		pciedev->csimon = TRUE;
	}
#endif /* BCM_CSIMON */

	/* Initialization of host memory extension region */
#ifdef BCMHME
	if ((ret = hme_link_pcie_ipc((void*)pciedev, pcie_ipc)) != BCME_OK) {
		PCI_ERROR(("Link HME failure\n"));
		goto link_failure;
	}
#endif /* BCMHME */

#ifdef HNDM2M
	if ((ret = m2m_link_pcie_ipc(pciedev, pcie_ipc)) != BCME_OK) {
		PCI_ERROR(("Link M2M failure\n"));
		goto link_failure;
	}
#endif // endif

#ifdef PCIE_DMA_INDEX
	/* Link the bus dmaindex feature to host side addresses */
	if ((ret = pciedev_dmaindex_link_pcie_ipc(pciedev)) != BCME_OK) {
		PCI_ERROR(("Link dmaindex failure\n"));
		goto link_failure;
	}
#endif /* PCIE_DMA_INDEX */

#ifdef BCMPCIE_IDMA
	if ((ret = pciedev_idma_link_pcie_ipc(pciedev)) != BCME_OK) {
		PCI_ERROR(("Link iDMA failure\n"));
		goto link_failure;
	}
#endif /* BCMPCIE_IDMA */

#if defined(WLSQS)
	/* SQS requires ucast flowrings be maintained per TID, instead of per AC */
	if (!(pciedev->pcie_ipc->hcap1 & PCIE_IPC_HCAP1_FLOWRING_TID)) {
		PCI_ERROR(("Link SQS per TID Ucast failure\n"));
		goto link_failure; /* DHD does not acknowledge per TID Ucast Flowring */
	}
#endif /* WLSQS */

	/* Link the H2D and D2H common msgbuf_rings to the host side PCIE IPC */
	if ((ret = pciedev_msgbuf_link_pcie_ipc(pciedev, pciedev->htod_ctrl,
	               pciedev_handle_h2d_msg_ctrlsubmit)) != BCME_OK) {
		PCI_ERROR(("Link H2D Ctrl submit failure\n"));
		goto link_failure;
	}

	if ((ret = pciedev_msgbuf_link_pcie_ipc(pciedev, pciedev->htod_rx,
	               pciedev_handle_h2d_msg_rxsubmit)) != BCME_OK) {
		PCI_ERROR(("Link H2D RxPost failure\n"));
		goto link_failure;
	}

	if ((ret = pciedev_msgbuf_link_pcie_ipc(pciedev, pciedev->dtoh_ctrlcpl,
	               NULL)) != BCME_OK) {
		PCI_ERROR(("Link D2H Ctrl Cpln failure\n"));
		goto link_failure;
	}

	if ((ret = pciedev_msgbuf_link_pcie_ipc(pciedev, pciedev->dtoh_txcpl,
	               NULL)) != BCME_OK) {
		PCI_ERROR(("Link D2H Tx Cpln failure\n"));
		goto link_failure;
	}

	if ((ret = pciedev_msgbuf_link_pcie_ipc(pciedev, pciedev->dtoh_rxcpl,
	               NULL)) != BCME_OK) {
		PCI_ERROR(("Link D2H Rx Cpln failure\n"));
		goto link_failure;
	}

	if (pciedev_flowrings_link_pcie_ipc(pciedev) != BCME_OK) {
		PCI_ERROR(("Link Flowrings failure\n"));
		goto link_failure;
	}

	/* Clear the mailboxint bits 0:1 in case host
	* has somehow set these bits.
	*/
	mailboxint =
		R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs)) &
			PD_FUNC0_PCIE_SB__INTMASK;
	if (mailboxint) {
		W_REG(pciedev->osh,
			PCIE_intstatus(pciedev->autoregs), PD_FUNC0_MB_INTMASK);
		(void)R_REG(pciedev->osh, PCIE_intstatus(pciedev->autoregs));
		mailboxint =
			R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs)) &
				PD_FUNC0_PCIE_SB__INTMASK;
		if (mailboxint) {
			PCI_ERROR(("COULDN'T CLEAR mailboxint bits 0:1 0x%x:0x%x\n",
				mailboxint,
				R_REG(pciedev->osh, PCIE_intstatus(pciedev->autoregs))));
			OSL_SYS_HALT();
		}
	}

#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	if (pciedev->ds_inband_dw_supported) {
		/* Now that we are initialized, kick sleep state machine */
		pciedev->ds_state = DS_STATE_ACTIVE;
		pciedev->ds_check_timer_on = TRUE;
		pciedev->ds_check_timer_max = 0;
		dngl_add_timer(pciedev->ds_check_timer, 0, FALSE);
	}
	else
#endif /* PCIEDEV_INBAND_DW */
		if (pciedev->ds_oob_dw_supported &&
			pciedev->ds_state != DS_DISABLED_STATE) {
			/* Now, trigger DS Engine */
			pciedev_trigger_deepsleep_dw(pciedev);
		}
#endif /* PCIE_DEEP_SLEEP */

#ifdef BCMHWA
	if (hwa_dev) {
		HWA_CPLENG_EXPR(dma64addr_t db_haddr64);
#if defined(HWA_CPLENG_BUILD)
		int idx;
#endif // endif

		/* Initialize HWA core */
		hwa_config(hwa_dev);

		/* HWA RXPOST takes over */
		HWA_RXPOST_EXPR(pciedev->htod_rx->paused = TRUE);

#if defined(HWA_CPLENG_BUILD) || defined(HWA_RXPOST_BUILD)
		OR_REG(pciedev->osh,
			&pciedev->regs->control, (PCIE_IDMA_MODE_EN | PCIE_HWA_EN));

		PCI_ERROR(("%s(): enable PCIE_IDMA_MODE_EN | PCIE_HWA_EN\n", __FUNCTION__));
#endif /* HWA_CPLENG_BUILD || HWA_RXPOST_BUILD */

#ifdef HWA_CPLENG_BUILD
		/* Configure software doorbell for HWA CPLE */
		/* TX CPLE */
		HADDR64_LO_SET(db_haddr64, (uint32)&pciedev->regs->u.pcie2.dbls[0].dev2host_0);
		HADDR64_HI_SET(db_haddr64, 0);
		hwa_sw_doorbell_request(hwa_dev, HWA_TXCPL_DOORBELL, 0,
			db_haddr64, PCIE_D2H_DB0_VAL);
		PCI_ERROR(("%s(): Register a software doorbell for HWA TX CPLE\n",
			__FUNCTION__));

		// RX CPLE
		// Use scratch buffer as RX doorbell address. For QT verification.
		for (idx = 1; idx <= 4; idx++) {
			uint32 db_val = PCIE_D2H_DB0_VAL + idx;

			HADDR64_LO_SET(db_haddr64,
				(uint32)&pciedev->regs->u.pcie2.dbls[0].dev2host_0);
			HADDR64_HI_SET(db_haddr64, 0);
			hwa_sw_doorbell_request(hwa_dev, HWA_RXCPL_DOORBELL,
				idx-1, db_haddr64, db_val);
			PCI_ERROR(("%s(): Register a software doorbell val<0x%x> for "
				"HWA RX[%d] CPLE\n", __FUNCTION__, db_val, idx-1));
		}
#endif /* HWA_CPLENG_BUILD */
	}
#endif /* BCMHWA */

	return BCME_OK;

link_failure:
	return ret;
} /* pciedev_link_pcie_ipc */

static int
pciedev_dump_pcie_ipc(struct dngl_bus *pciedev)
{
	/* FD assumes HostOrder is LittleEndian */
	pcie_ipc_t *pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings;

	if ((pcie_ipc = pciedev->pcie_ipc) == NULL)
		return BCME_ERROR;

	if ((pcie_ipc_rings = (pcie_ipc_rings_t*)pcie_ipc->rings_daddr32) == NULL)
		return BCME_ERROR;

	PCI_PRINT(("PCIE IPC FWID 0x%08x Rev: host %x dngl %x\n",
		pcie_ipc->fwid,
		pcie_ipc->flags & PCIE_IPC_FLAGS_REVISION_MASK,
		pcie_ipc->hcap1 & PCIE_IPC_FLAGS_REVISION_MASK));
	PCI_PRINT(("\tflags 0x%08x dcap1 0x%08x dcap2 0x%08x hcap1 0x%08x hcap2 0x%08x\n",
		pcie_ipc->flags, pcie_ipc->dcap1, pcie_ipc->dcap2,
		pcie_ipc->hcap1, pcie_ipc->hcap2));
	PCI_PRINT(("\tTrap 0x%08x Assrt expr 0x%08x file 0x%08x line 0x%08x cons 0x%08x\n",
		pcie_ipc->trap_daddr32, pcie_ipc->assert_exp_daddr32,
		pcie_ipc->assert_file_daddr32, pcie_ipc->assert_line,
		pcie_ipc->console_daddr32));
	PCI_PRINT(("\tMax pkts Tx %u Rx %u dma rx offset %u MBox H2D 0x%08x D2H 0x%08x\n",
		pcie_ipc->max_tx_pkts, pcie_ipc->max_rx_pkts, pcie_ipc->dma_rxoffset,
		pcie_ipc->h2d_mb_daddr32, pcie_ipc->d2h_mb_daddr32));
	PCI_PRINT(("\tHost Mem %u" HADDR64_FMT " host_physaddrhi 0x%08x\n",
		pcie_ipc->host_mem_len, HADDR64_VAL(pcie_ipc->host_mem_haddr64),
		pcie_ipc->host_physaddrhi));
	PCI_PRINT(("\tLog: buzzz 0x%08x\n",
		pcie_ipc->buzzz_daddr32));
		PCI_PRINT(("IPC Rings 0x%08x Ring Mem 0x%08x, Max H2D %u D2H %u FR %u Ifs %u\n",
		pcie_ipc->rings_daddr32, pcie_ipc_rings->ring_mem_daddr32,
		pcie_ipc_rings->max_h2d_rings, pcie_ipc_rings->max_d2h_rings,
		pcie_ipc_rings->max_flowrings, pcie_ipc_rings->max_interfaces));
	PCI_PRINT(("\tDngl Indices Arrays: H2D WR 0x%08x RD 0x%08x D2H WR 0x%08x RD 0x%08x\n",
		pcie_ipc_rings->h2d_wr_daddr32, pcie_ipc_rings->h2d_rd_daddr32,
		pcie_ipc_rings->d2h_wr_daddr32, pcie_ipc_rings->d2h_rd_daddr32));
	PCI_PRINT(("\tHost Indices Arrays: H2D WR" HADDR64_FMT " RD" HADDR64_FMT
		"D2H WR" HADDR64_FMT " RD" HADDR64_FMT "\n",
		HADDR64_VAL(pcie_ipc_rings->h2d_wr_haddr64),
		HADDR64_VAL(pcie_ipc_rings->h2d_rd_haddr64),
		HADDR64_VAL(pcie_ipc_rings->d2h_wr_haddr64),
		HADDR64_VAL(pcie_ipc_rings->d2h_rd_haddr64)));
	PCI_PRINT(("\tRxPost data_buf_len %u RxCpln data_offset %u\n",
		pcie_ipc_rings->rxpost_data_buf_len,
		pcie_ipc_rings->rxcpln_dataoffset));
	PCI_PRINT(("\tRing[FMT,type,sz] "
		"TxP[%u,%u,%u] RxP[%u,%u,%u] TxC[%u,%u,%u] RxC[%u,%u,%u]\n",
		pcie_ipc_rings->txpost_format, pciedev->txpost_item_type,
		pciedev->txpost_item_size,
		pcie_ipc_rings->rxpost_format, MSGBUF_ITEM_TYPE(pciedev->htod_rx),
		MSGBUF_ITEM_SIZE(pciedev->htod_rx),
		pcie_ipc_rings->txcpln_format, MSGBUF_ITEM_TYPE(pciedev->dtoh_txcpl),
		MSGBUF_ITEM_SIZE(pciedev->dtoh_txcpl),
		pcie_ipc_rings->rxcpln_format, MSGBUF_ITEM_TYPE(pciedev->dtoh_rxcpl),
		MSGBUF_ITEM_SIZE(pciedev->dtoh_rxcpl)));

#ifdef BCMHME
	hme_dump_pcie_ipc((void*)pciedev, pcie_ipc);
#endif /* BCMHME */

	return BCME_OK;

} /* pciedev_dump_pcie_ipc */

#ifdef PCIE_DMA_INDEX
/**
 * dmaindex structures are part of the pciedev dngl_bus structure.
 */
static int
BCMATTACHFN(pciedev_setup_dmaindex)(struct dngl_bus *pciedev)
{
	struct fetch_rqst *fetch_rqst;
	uint32 dmaindex_h2d_size = (PCIE_RW_INDEX_SZ * pciedev->max_h2d_rings);
	uint32 dmaindex_d2h_size = (PCIE_RW_INDEX_SZ * pciedev->max_d2h_rings);

#if defined(PCIE_DMA_INDEX_DEBUG)
	int log_mem_size = PCIE_DMAINDEX_LOG_MAX_ENTRY * sizeof(dmaindex_log_t);

	if (!(pciedev->dmaindex_log_h2d = MALLOCZ(pciedev->osh, log_mem_size))) {
		PCI_ERROR(("dmaindex malloc dmaindex_log_h2d failure\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
	}
	if (!(pciedev->dmaindex_log_d2h = MALLOCZ(pciedev->osh, log_mem_size))) {
		PCI_ERROR(("dmaindex malloc dmaindex_log_d2h failure\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
	}
#endif /* PCIE_DMA_INDEX_DEBUG */

	/* H2D direction dmaindex support */
	pciedev->dmaindex_h2d_wr.size = dmaindex_h2d_size;
	pciedev->dmaindex_h2d_rd.size = dmaindex_h2d_size;

	/* Pre-initialize fetch of H2D WR indices */
	pciedev->dmaindex_h2d_fetches_queued = 0;
	pciedev->dmaindex_h2d_fetch_needed   = FALSE;

	fetch_rqst = &pciedev->dmaindex_h2d_fetch_rqst;
	// fetch_rqst->haddr in link phase
	fetch_rqst->size  = pciedev->dmaindex_h2d_wr.size;
	// fetch_rqst->dest in bind phase
	fetch_rqst->cb    = pciedev_dmaindex_h2d_fetch_cb;
	fetch_rqst->ctx   = (void *)pciedev;
	fetch_rqst->next  = NULL;

	/* D2H direction dmaindex support */
	pciedev->dmaindex_d2h_wr.size = dmaindex_d2h_size;
	pciedev->dmaindex_d2h_rd.size = dmaindex_d2h_size;

	/* Pre-initialize fetch of D2H RD indices */
	pciedev->dmaindex_d2h_fetches_queued = 0;
	pciedev->dmaindex_d2h_fetch_needed   = FALSE;

	fetch_rqst = &pciedev->dmaindex_d2h_fetch_rqst;
	// fetch_rqst->haddr in link phase
	fetch_rqst->size  = pciedev->dmaindex_d2h_rd.size;
	// fetch_rqst->dest in bind phase
	fetch_rqst->cb    = pciedev_dmaindex_d2h_fetch_cb;
	fetch_rqst->ctx   = (void *)pciedev;
	fetch_rqst->next  = NULL;

	/* bus::dmaindex_h2d_w_d2h command to enable h2d index fetch with d2h */
	pciedev->dmaindex_h2d_w_d2h = FALSE;

	return BCME_OK;

} /* pciedev_setup_dmaindex */

static void
BCMATTACHFN(pciedev_cleanup_dmaindex)(struct dngl_bus *pciedev)
{
#if defined(PCIE_DMA_INDEX_DEBUG)
	int log_mem_size = PCIE_DMAINDEX_LOG_MAX_ENTRY * sizeof(dmaindex_log_t);

	if (pciedev->dmaindex_log_h2d) {
		MFREE(pciedev->osh, pciedev->dmaindex_log_h2d, log_msm_sz);
		pciedev->dmaindex_log_h2d = NULL;
	}
	if (pciedev->dmaindex_log_d2h) {
		MFREE(pciedev->osh, pciedev->dmaindex_log_d2h, log_msm_sz);
		pciedev->dmaindex_log_d2h = NULL;
	}
#endif /* PCIE_DMA_INDEX_DEBUG */
} /* pciedev_cleanup_dmaindex */

/**
 * Bind the bus layer dmaindex feature to the dongle side of PCIE IPC.
 * Each of the 4 Indices arrays' dongle address and size are retrieved and
 * saved in the bus layer as part of the PCIE IPC BIND phase. The Host side
 * addresses will be retrieved and and saved later in the PCIE IPC LINK
 * phase, at which time the dmaindex support will be enabled for the rings.
 */
static int
BCMATTACHFN(pciedev_dmaindex_bind_pcie_ipc)(struct dngl_bus *pciedev)
{
	pcie_ipc_t *pcie_ipc = pciedev->pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings = (pcie_ipc_rings_t*)pcie_ipc->rings_daddr32;

	/* Bind H2D WR and RD indices arrays' dongle side info */
	pciedev->dmaindex_h2d_wr.daddr64.lo = pcie_ipc_rings->h2d_wr_daddr32;
	pciedev->dmaindex_h2d_rd.daddr64.lo = pcie_ipc_rings->h2d_rd_daddr32;

	PCI_INFORM(("h2d [sz %u wr 0x%08x rd 0x%08x]\n",
		pciedev->dmaindex_h2d_wr.size,
		pciedev->dmaindex_h2d_wr.daddr64.lo,
		pciedev->dmaindex_h2d_rd.daddr64.lo));

	/* Bind D2H WR and RD indices arrays' dongle side info */
	pciedev->dmaindex_d2h_wr.daddr64.lo = pcie_ipc_rings->d2h_wr_daddr32;
	pciedev->dmaindex_d2h_rd.daddr64.lo = pcie_ipc_rings->d2h_rd_daddr32;

	PCI_INFORM(("d2h [sz %u wr 0x%08x rd 0x%08x]\n",
		pciedev->dmaindex_d2h_wr.size,
		pciedev->dmaindex_d2h_wr.daddr64.lo,
		pciedev->dmaindex_d2h_rd.daddr64.lo));

	/* Bind the H2D WR fetch request and D2H RD fetch request */
	pciedev->dmaindex_h2d_fetch_rqst.dest = (uint8 *)
		pcie_ipc_rings->h2d_wr_daddr32;
	pciedev->dmaindex_d2h_fetch_rqst.dest = (uint8 *)
		pcie_ipc_rings->d2h_rd_daddr32;

	PCI_TRACE(("Bind dmaindex success\n"));

	return BCME_OK;

} /* pciedev_dmaindex_bind_pcie_ipc */

/**
 * Link the bus layer dmaindex feature to the host side of PCIE IPC.
 * Each of the 4 Indices arrays' host address are retrieved and saved in the
 * bus layer as part of the PCIE IPC BIND phase.
 */
static int
pciedev_dmaindex_link_pcie_ipc(struct dngl_bus *pciedev)
{
	pcie_ipc_t *pcie_ipc = pciedev->pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings = (pcie_ipc_rings_t*)pcie_ipc->rings_daddr32;

	/* Link H2D WR indices arrays' host side info */
	if (!HADDR64_IS_ZERO(pcie_ipc_rings->h2d_wr_haddr64))
	{
		HADDR64_SET(pciedev->dmaindex_h2d_wr.haddr64,
		              pcie_ipc_rings->h2d_wr_haddr64);
		pciedev->dmaindex_h2d_wr.inited = TRUE;

		PCI_INFORM(("link dmaindex h2d wr" HADDR64_FMT "\n",
			HADDR64_VAL(pciedev->dmaindex_h2d_wr.haddr64)));
	} else {
		PCI_ERROR(("No host's dmaindex h2d wr info\n"));
	}

	/* Link H2D RD indices arrays' host side info */
	if (!HADDR64_IS_ZERO(pcie_ipc_rings->h2d_rd_haddr64))
	{
		HADDR64_SET(pciedev->dmaindex_h2d_rd.haddr64,
		              pcie_ipc_rings->h2d_rd_haddr64);
		pciedev->dmaindex_h2d_rd.inited = TRUE;

		PCI_INFORM(("link dmaindex h2d rd" HADDR64_FMT "\n",
			HADDR64_VAL(pciedev->dmaindex_h2d_rd.haddr64)));
	} else {
		PCI_ERROR(("No host's dmaindex h2d rd info\n"));
	}

	/* Link D2H WR indices arrays' host side info */
	if (!HADDR64_IS_ZERO(pcie_ipc_rings->d2h_wr_haddr64))
	{
		HADDR64_SET(pciedev->dmaindex_d2h_wr.haddr64,
		              pcie_ipc_rings->d2h_wr_haddr64);
		pciedev->dmaindex_d2h_wr.inited = TRUE;

		PCI_INFORM(("link dmaindex d2h wr" HADDR64_FMT "\n",
			HADDR64_VAL(pciedev->dmaindex_d2h_wr.haddr64)));
	} else {
		PCI_ERROR(("No host's dmaindex d2h wr info\n"));
	}

	/* Link D2H RD indices arrays' host side info */
	if (!HADDR64_IS_ZERO(pcie_ipc_rings->d2h_rd_haddr64))
	{
		HADDR64_SET(pciedev->dmaindex_d2h_rd.haddr64,
		              pcie_ipc_rings->d2h_rd_haddr64);
		pciedev->dmaindex_d2h_rd.inited = TRUE;

		PCI_INFORM(("link dmaindex d2h rd" HADDR64_FMT "\n",
			HADDR64_VAL(pciedev->dmaindex_d2h_rd.haddr64)));
	} else {
		PCI_ERROR(("No host's dmaindex d2h rd info\n"));
	}

	/* Link up the dmaindex H2D WR indices fetch_rqst support */
	if (pciedev->dmaindex_h2d_wr.inited) {
		HADDR64_SET(pciedev->dmaindex_h2d_fetch_rqst.haddr,
			pcie_ipc_rings->h2d_wr_haddr64);
	}

	/* Link up the dmaindex D2H RD indices fetch_rqst support */
	if (pciedev->dmaindex_d2h_rd.inited) {
		HADDR64_SET(pciedev->dmaindex_d2h_fetch_rqst.haddr,
			pcie_ipc_rings->d2h_rd_haddr64);
	}

	PCI_TRACE(("Link dmaindex success\n"));

	return BCME_OK;

} /* pciedev_dmaindex_link_pcie_ipc */

#ifdef PCIE_DMA_INDEX_DEBUG

/** Function to record the DMA transactions of indices */
static void
pciedev_dmaindex_log(struct dngl_bus *pciedev, bool d2h, bool is_h2d_dir,
	dmaindex_t *dmaindex)
{
	dmaindex_log_t *dmaindex_log_entry;

	/* If indices are DMA'ed to host memory */
	if (d2h) {
		dmaindex_log_entry =
			&pciedev->dmaindex_log_d2h[pciedev->dmaindex_log_d2h_cnt];
		pciedev->dmaindex_log_d2h_cnt =
			(pciedev->dmaindex_log_d2h_cnt + 1) % PCIE_DMAINDEX_LOG_MAX_ENTRY;

	} else { /* If indices are DMA'ed from host memory */
		dmaindex_log_entry =
			&pciedev->dmaindex_log_h2d[pciedev->dmaindex_log_h2d_cnt];
		pciedev->dmaindex_log_h2d_cnt =
			(pciedev->dmaindex_log_h2d_cnt + 1) % PCIE_DMAINDEX_LOG_MAX_ENTRY;
	}

	dmaindex_log_entry->is_h2d_dir = is_h2d_dir;
	HADDR64_SET(dmaindex_log_entry->haddr64, dmaindex->haddr64);
	dmaindex_log_entry->daddr64 = dmaindex->daddr64;
	dmaindex_log_entry->size = dmaindex->size;
	dmaindex_log_entry->timestamp = OSL_SYSUPTIME();

	/* Copy 12Bytes of the data (indices) being DMA'ed */
	bcopy((uchar *)dmaindex->daddr64.lo, &dmaindex_log_entry->index, 12);

} /* pciedev_dmaindex_log */

/** Dump FW's record of DMA'ing indices */
static int
pciedev_dmaindex_log_dump(struct dngl_bus * pciedev, int option)
{
	int i;
	dmaindex_log_t *dmaindex_log_entry;
	switch (option) {
		case 1:
			if (pciedev->dmaindex_log_d2h) {
				PCI_PRINT(("D2H INDXDMA LOG\n"));
				for (i = 0; i < PCIE_DMAINDEX_LOG_MAX_ENTRY; i++) {
					dmaindex_log_entry = &pciedev->dmaindex_log_d2h[i];
					PCI_PRINT(("Entry:%d\tsubm. is_h2d_dir:%d\tsize:%d\t"
						"timestamp:0x%x\n" HADDR64_FMT "\n"
						" dadd64  low 0x%x high 0x%x\n"
						" First 3 indices:0x%02x, 0x%02x, 0x%02x\n",
						i, dmaindex_log_entry->is_h2d_dir,
						dmaindex_log_entry->size,
						dmaindex_log_entry->timestamp,
						HADDR64_VAL(dmaindex_log_entry->haddr64),
						dmaindex_log_entry->daddr64.lo,
						dmaindex_log_entry->daddr64.hi,
						dmaindex_log_entry->index[0],
						dmaindex_log_entry->index[1],
						dmaindex_log_entry->index[2]));
				}
			} else
				PCI_PRINT(("d2h_index_dma_log does not exist\n"));
			break;
		case 2:
			if (pciedev->dmaindex_log_h2d) {
				PCI_PRINT(("H2D INDXDMA LOG\n"));
				for (i = 0; i < PCIE_DMAINDEX_LOG_MAX_ENTRY; i++) {
					dmaindex_log_entry = &pciedev->dmaindex_log_h2d[i];
					PCI_PRINT(("Entry:%d\tsubm. is_h2d_dir:%d\tsize:%d\t"
						"timestamp:0x%x\n" HADDR64_FMT "\n"
						" dadd64  low 0x%x high 0x%x\n"
						" First 3 indices:0x%02x, 0x%02x, 0x%02x\n",
						i, dmaindex_log_entry->is_h2d_dir,
						dmaindex_log_entry->size,
						dmaindex_log_entry->timestamp,
						HADDR64_VAL(dmaindex_log_entry->haddr64),
						dmaindex_log_entry->daddr64.lo,
						dmaindex_log_entry->daddr64.hi,
						dmaindex_log_entry->index[0],
						dmaindex_log_entry->index[1],
						dmaindex_log_entry->index[2]));
				}
			} else
				PCI_PRINT(("h2d_index_dma_log does not exist\n"));
			break;
		default:
			break;
	}

	return BCME_OK;
} /* pciedev_dmaindex_log_dump */

#endif /* PCIE_DMA_INDEX_DEBUG */

#ifndef SBTOPCIE_INDICES
/**
 * Sync to host the H2D RD and D2H WR indices arrays.
 */
void
pciedev_dmaindex_put(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	dma_queue_t *dmaq = pciedev->dtoh_dma_q[pciedev->default_dma_ch];
	hnddma_t *di = pciedev->d2h_di[pciedev->default_dma_ch];
	pcie_m2m_req_t *m2m_req = pciedev->m2m_req;

	dmaindex_t *dmaindex_h2d_rd, *dmaindex_d2h_wr;
	uint32 dmaindex_h2d_rd_size, dmaindex_d2h_wr_size;

	dmaindex_h2d_rd = &pciedev->dmaindex_h2d_rd;
	dmaindex_d2h_wr = &pciedev->dmaindex_d2h_wr;

	dmaindex_h2d_rd_size = dmaindex_h2d_rd->size;
	dmaindex_d2h_wr_size = dmaindex_d2h_wr->size;

	if ((dmaindex_h2d_rd->inited == 0) || (dmaindex_d2h_wr->inited == 0)) {

		PCI_ERROR(("%s: Host memory for dma'ing"
			"d2h-w/h2d-r indices not inited!\n", __FUNCTION__));
		DBG_BUS_INC(pciedev, set_indices_fail);

		goto exit;
	}

	PCIE_M2M_REQ_INIT(m2m_req);

	/* Program rx descriptors for h2d-r/d2h-w indices */
	PCIE_M2M_REQ_RX_SETUP(m2m_req, dmaindex_h2d_rd->haddr64, dmaindex_h2d_rd_size);
	PCIE_M2M_REQ_RX_SETUP(m2m_req, dmaindex_d2h_wr->haddr64, dmaindex_d2h_wr_size);

	/* Program tx descriptors for h2d-r/d2h-w indices */
	PCIE_M2M_REQ_TX_SETUP(m2m_req, dmaindex_h2d_rd->daddr64, dmaindex_h2d_rd_size);
	PCIE_M2M_REQ_TX_SETUP(m2m_req, dmaindex_d2h_wr->daddr64, dmaindex_d2h_wr_size);

	/* Submit m2m request */
	if (PCIE_M2M_REQ_SUBMIT(di, m2m_req)) {
		pciedev->dmaindex_d2h_pending = 1;
		PCI_ERROR(("%s: m2m desciptors not available\n", __FUNCTION__));
		DBG_BUS_INC(pciedev, set_indices_fail);

		goto exit;
	}

	pciedev->dmaindex_d2h_pending = 0;

	pciedev_enqueue_dma_info(dmaq, dmaindex_h2d_rd_size + dmaindex_d2h_wr_size,
		MSG_TYPE_INDX_UPDATE, PCIEDEV_INTERNAL_D2HINDX_UPD, ring, 0, 0);

	if (pciedev->dmaindex_log_d2h) {
		WL_PCIE_DMAINDEX_LOG((pciedev, TRUE, TRUE,  dmaindex_h2d_rd));
		WL_PCIE_DMAINDEX_LOG((pciedev, TRUE, FALSE, dmaindex_d2h_wr));
	}

	/* Optional: after dma'ing indices to host, fetch them from host memory */
	if (!PCIE_IDMA_ACTIVE(pciedev)) {
		if (pciedev->dmaindex_h2d_w_d2h) {
			/* don't need to fetch indice if it is in D3 */
			if (!pciedev_ds_in_host_sleep(pciedev)) {
				pciedev_dmaindex_get(pciedev);
			}
		}
	}

exit:
	return;
}
#endif /* ! SBTOPCIE_INDICES */

/**
 * Fetch DMA write indices from host memory
 */
static void
pciedev_dmaindex_get_h2d_wr(struct dngl_bus *pciedev)
{
	/* If host-buffer initialized, fetch request for h2d w-indices */
	if ((pciedev->dmaindex_h2d_fetches_queued < PCIE_DMAINDEX_MAXFETCHPENDING) &&
	    (pciedev->dmaindex_h2d_wr.inited)) {
		pciedev->dmaindex_h2d_fetch_rqst.flags = 0;
		hnd_fetch_rqst(&pciedev->dmaindex_h2d_fetch_rqst);
		pciedev->dmaindex_h2d_fetches_queued++;
		pciedev->dmaindex_h2d_fetch_needed = FALSE;
	} else
		pciedev->dmaindex_h2d_fetch_needed = TRUE;
} /* pciedev_dmaindex_get_h2d_wr */

#ifndef SBTOPCIE_INDICES
/**
 * Fetch DMA read indices from host memory
 */
static void
pciedev_dmaindex_get_d2h_rd(struct dngl_bus *pciedev)
{
	if ((pciedev->dmaindex_d2h_fetches_queued < PCIE_DMAINDEX_MAXFETCHPENDING) &&
	    (pciedev->dmaindex_d2h_rd.inited)) {
		pciedev->dmaindex_d2h_fetch_rqst.flags = 0;
		hnd_fetch_rqst(&pciedev->dmaindex_d2h_fetch_rqst);
		pciedev->dmaindex_d2h_fetches_queued++;
		pciedev->dmaindex_d2h_fetch_needed = FALSE;
	} else /* Even if can't create fetch rq., don't ignore the drbl */
		pciedev->dmaindex_d2h_fetch_needed = TRUE;
} /* pciedev_dmaindex_get_d2h_rd */
#endif /* !SBTOPCIE_INDICES */

/**
 * Fetches DMA write/read indices from host memory.
 * Called only if host addresses are initialized in pcie_ipc_rings structure.
 */
void
pciedev_dmaindex_get(struct dngl_bus *pciedev)
{
	if (pciedev->in_d3_suspend) {
		return;
	}

	/* Get both read and write indices from host */
	pciedev_dmaindex_get_h2d_wr(pciedev);
#ifndef SBTOPCIE_INDICES
	pciedev_dmaindex_get_d2h_rd(pciedev);
#endif // endif
} /* pciedev_dmaindex_get */

static void
pciedev_dmaindex_h2d_fetch_cb(struct fetch_rqst *fr, bool cancelled)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)fr->ctx;

	if (cancelled) {
		PCI_ERROR(("pciedev_indxdma_fetch_cb: Request cancelled!...\n"));
		DBG_BUS_INC(pciedev, h2d_fetch_cancelled);
		goto cleanup;
	}

	pciedev_msgbuf_intr_process(pciedev);  /* starts fetching messages */

cleanup:
	pciedev->dmaindex_h2d_fetches_queued--;

#ifdef PCIE_DMA_INDEX_DEBUG
	/* Log the transaction for h2d-w */
	if (pciedev->dmaindex_log_h2d)
		WL_PCIE_DMAINDEX_LOG((pciedev, FALSE, TRUE, &pciedev->dmaindex_h2d_wr));
#endif /* PCIE_DMA_INDEX_DEBUG */

	if (pciedev->dmaindex_h2d_fetch_needed)
		pciedev_dmaindex_get_h2d_wr(pciedev);
}

static void
pciedev_dmaindex_d2h_fetch_cb(struct fetch_rqst *fr, bool cancelled)
{

	struct dngl_bus *pciedev = (struct dngl_bus *)fr->ctx;

	if (cancelled) {
		PCI_ERROR(("pciedev_indxdma_fetch_cb: Request cancelled!...\n"));
		DBG_BUS_INC(pciedev, d2h_fetch_cancelled);
		goto cleanup;
	}

cleanup:
	pciedev->dmaindex_d2h_fetches_queued--;

#ifdef PCIE_DMA_INDEX_DEBUG
	/* Log the transaction for d2h-r */
	if (pciedev->dmaindex_log_h2d)
		WL_PCIE_DMAINDEX_LOG((pciedev, FALSE, FALSE, &pciedev->dmaindex_d2h_rd));
#endif /* PCIE_DMA_INDEX_DEBUG */

#ifndef SBTOPCIE_INDICES
	if (pciedev->dmaindex_d2h_fetch_needed)
		pciedev_dmaindex_get_d2h_rd(pciedev);
#else
	PCI_ERROR(("pciedev_dmaindex_d2h_fetch_cb: Should not happen!...\n"));
#endif // endif
}

#endif /* PCIE_DMA_INDEX */

#ifdef BCMPCIE_IDMA

#define IDMA_SET_DMAINDEX(i, h, l, s, ts) \
({ \
	HADDR64_HI_SET((i)->daddr64, h); \
	HADDR64_LO_SET((i)->daddr64, l); \
	(i)->size = (s); \
	(ts) -= (s); \
	(l) += (s); \
	(i)++; \
})
#define IDMA_UPD_DMAINDEX(i, h, l) \
({ \
	ASSERT((i)->size != 0); \
	HADDR64_HI_SET((i)->haddr64, h); \
	HADDR64_LO_SET((i)->haddr64, l); \
	(l) += (i)->size; \
	(i)->inited = TRUE; \
	(i)++; \
})

static int
BCMATTACHFN(pciedev_setup_idma)(struct dngl_bus *pciedev)
{
	pciedev->idma_info = (idma_info_t*) MALLOCZ(pciedev->osh, sizeof(idma_info_t));
	if (pciedev->idma_info == NULL) {
		PCI_ERROR(("Setup iDMA: malloc idma_info_t failure\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail_cleanup;
	}

	pciedev->idma_info->enable = TRUE;
	return BCME_OK;

fail_cleanup:
	return BCME_NOMEM;

} /* pciedev_setup_idma */

static void
BCMATTACHFN(pciedev_cleanup_idma)(struct dngl_bus *pciedev)
{
	if (pciedev->idma_info) {
		pciedev->idma_info->enable = FALSE;
		MFREE(pciedev->osh, pciedev->idma_info, sizeof(idma_info_t));
		pciedev->idma_info = NULL;
	}
} /* pciedev_cleanup_idma */

/** Bind the bus layer iDMA subsystem to the PCIE IPC dongle side */
static int
BCMATTACHFN(pciedev_idma_bind_pcie_ipc)(struct dngl_bus *pciedev)
{
	int i = 0;
	dmaindex_t *dmaindex;
	daddr32_t daddr32;
	uint32 size, alloc_sz;
	pcie_ipc_t *pcie_ipc = pciedev->pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings = (pcie_ipc_rings_t*)pcie_ipc->rings_daddr32;
	idma_info_t *idma_info = pciedev->idma_info;
	uint32 idma_h2d_size = (PCIE_RW_INDEX_SZ * pciedev->max_h2d_rings);
	uint32 idma_d2h_size = (PCIE_RW_INDEX_SZ * pciedev->max_d2h_rings);

	if (!PCIE_IDMA_ENAB(pciedev))
		return BCME_OK;

	/* Allocate FRM write index dmaindex info */
	alloc_sz = sizeof(dmaindex_t) * MAX_IDMA_DESC;
	dmaindex = MALLOCZ(pciedev->osh, alloc_sz);
	if (dmaindex == NULL) {
		PCI_ERROR(("iDMA bind pcie_ipc malloc dmaindex failure\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail_cleanup;
	}
	idma_info->dmaindex = dmaindex;
	idma_info->dmaindex_sz = alloc_sz;

	/* Implicit DMAing d2h read and h2d write indices from host memory */
	/* See bcmpcie.h for iDMA partition layout. */
	/* NOTE: iDMA is including Common rings */

	/* 0. D2H_RD */
	daddr32 = pcie_ipc_rings->d2h_rd_daddr32;
	size = idma_d2h_size;
	IDMA_SET_DMAINDEX(dmaindex, 0U, daddr32, size, idma_d2h_size);

	/* 1. H2D_WR */
	daddr32 = pcie_ipc_rings->h2d_wr_daddr32;
	size = BCMPCIE_IDMA_FLOWS_PER_SET * PCIE_RW_INDEX_SZ;
	for (i = 0; i < MAX_IDMA_DESC-2; i++) {
		if (size > idma_h2d_size)
			size = idma_h2d_size;
		IDMA_SET_DMAINDEX(dmaindex, 0U, daddr32, size, idma_h2d_size);
		if (idma_h2d_size == 0)
			break;
	}

	/* 2. H2D_WR, rest indices */
	if (idma_h2d_size > 0) {
		size = idma_h2d_size;
		IDMA_SET_DMAINDEX(dmaindex, 0U, daddr32, size, idma_h2d_size);
	}

	PCI_TRACE(("Bind iDMA success\n"));

	return BCME_OK;

fail_cleanup:
	/* pciedev_detach will clean it up. */
	return BCME_NOMEM;

} /* pciedev_idma_bind_pcie_ipc */

/** Link the bus layer iDMA subsystem to the PCIE IPC dongle side.
 * Allocate the descriptors for the iDMA's mem2mem engine.
 */
static int
pciedev_idma_link_pcie_ipc(struct dngl_bus *pciedev)
{
	uint index = 0;
	uint haddr64_h, haddr64_l;
	dmaindex_t *dmaindex;
	pcie_ipc_t *pcie_ipc = pciedev->pcie_ipc;
	pcie_ipc_rings_t *pcie_ipc_rings = (pcie_ipc_rings_t *)pcie_ipc->rings_daddr32;

	if (!PCIE_IDMA_ENAB(pciedev))
		return BCME_OK;

	if (IS_DEFAULT_HOST_PCIE_IPC(pciedev))
		return BCME_UNSUPPORTED;

	/* Host acknowledges ability to handle iDMA */
	if (!(pcie_ipc->hcap1 & PCIE_IPC_HCAP1_IDMA)) {
		PCI_ERROR(("Host does not support iDMA\n"));
		return BCME_OK;
	}

	/* H2D WR */
	if (HADDR64_IS_ZERO(pcie_ipc_rings->h2d_wr_haddr64)) {
		return BCME_BADARG;
	}

	/* D2H RD */
	if (HADDR64_IS_ZERO(pcie_ipc_rings->d2h_rd_haddr64)) {
		return BCME_BADARG;
	}

	dmaindex = pciedev->idma_info->dmaindex;

	/* 0. D2H_RD */
	haddr64_h = HADDR64_HI(pcie_ipc_rings->d2h_rd_haddr64);
	haddr64_l = HADDR64_LO(pcie_ipc_rings->d2h_rd_haddr64);
	IDMA_UPD_DMAINDEX(dmaindex, haddr64_h, haddr64_l);
	index++;

	/* 1. H2D_WR */
	haddr64_h = HADDR64_HI(pcie_ipc_rings->h2d_wr_haddr64);
	haddr64_l = HADDR64_LO(pcie_ipc_rings->h2d_wr_haddr64);
	ASSERT((0xFFFFFFFF - haddr64_l) >= (PCIE_RW_INDEX_SZ * pciedev->max_h2d_rings));
	while (dmaindex->size != 0 && index < MAX_IDMA_DESC) {
		IDMA_UPD_DMAINDEX(dmaindex, haddr64_h, haddr64_l);
		index++;
	}

	pciedev->idma_info->inited = TRUE;

	/* program 16rx/16tx desc and prefetch desc */
	if (!pciedev->idma_info->desc_loaded &&
	        ((uint32)(pciedev->h2d_di[DMA_CH2]) != 0)) {
		pciedev_dma_params_init_h2d(pciedev, pciedev->h2d_di[DMA_CH2]);

		if (pciedev_idma_desc_load(pciedev) != BCME_OK) {
			PCI_ERROR(("implicit dma desc load fail\n"));
			pciedev->idma_info->desc_loaded = FALSE;
			return BCME_ERROR;
		} else {
			pciedev->idma_info->desc_loaded = TRUE;
		}
	} else {
		PCI_ERROR(("idma not prog, inited %d, desc_loaded %d, h2d_di2 0x%8x\n",
			pciedev->idma_info->inited, pciedev->idma_info->desc_loaded,
			(uint32)(pciedev->h2d_di[DMA_CH2])));
		ASSERT(0);
		return BCME_ERROR;
	}

#if defined(BCMDBG) || defined(WLTEST)
	index = 0;
	dmaindex = pciedev->idma_info->dmaindex;
	while (dmaindex->inited && index < MAX_IDMA_DESC) {
		PCI_ERROR(("link idma index%d "
			"size %d" HADDR64_FMT " daddr64 lo 0x%8x\n",
			index, dmaindex->size, HADDR64_VAL(dmaindex->haddr64),
			dmaindex->daddr64.lo));
		index++; dmaindex++;
	}
#endif // endif

	PCI_TRACE(("Link idma success\n"));

	return BCME_OK;

} /* pciedev_idma_link_pcie_ipc */

#endif /* BCMPCIE_IDMA */

#ifdef DBG_IPC_LOGS
/** Add logs for IPC events (Doorbells and Mailbox events ) */
void pciedev_ipc_log(struct dngl_bus * pciedev, ipc_event_t event_type, uint8 stype, uint32 ts)
{
	if (pciedev->ipc_logs) {
		ipc_eventlog *ipc_event_log = &(pciedev->ipc_logs->eventlog[event_type]);
		int index = ipc_event_log->count % MAX_BUFFER_HISTORY;
		ipc_event_log->count++;
		ipc_event_log->buffer[index].timestamp = ts;

		switch (event_type) {
			case DOORBELL_D2H:
				ASSERT(stype < DB_D2H_INVALID);
				ipc_event_log->buffer[index].subtype.db_d2h = stype;
			break;
			case DOORBELL_H2D:
				ASSERT(stype < IPC_LOG_DB_H2D_INVALID);
				ipc_event_log->buffer[index].subtype.db_h2d = stype;
			break;
			case MAILBOX:
				ASSERT(stype < IPC_LOG_MB_INVALID);
				ipc_event_log->buffer[index].subtype.mb = stype;
			break;
			default:
				ASSERT(0);
			break;
		}
	}
}
#endif /* DBG_IPC_LOGS */

/**
 * This function is called when the core is in 'survive PERST' mode (because it is in D3cold) and
 * the host asserts the #PERST line (with the intent of transitioning the core from D3 to D0).
 */
static int
pciedev_handle_perst_intr(struct dngl_bus *pciedev)
{
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	if (pciedev->ds_inband_dw_supported)
	{
		if (pciedev->ds_state != DS_STATE_INVALID &&
		   pciedev->ds_state != DS_STATE_DISABLED &&
		   pciedev->ds_state != DS_STATE_HOST_SLEEP) {
			TRAP_IN_PCIEDRV(("Unexpected #PERST\n"));
		}
	}
	else
#endif /* PCIEDEV_INBAND_DW */
	if (pciedev->ds_oob_dw_supported) {
		pciedev_ob_deepsleep_engine(pciedev, PERST_ASSRT_EVENT);
	}
#endif /* PCIE_DEEP_SLEEP */

	if (PCIECOREREV(pciedev->corerev) <= 13) {
		pciedev_perst_reset_process(pciedev);
	}

	if (!HOSTREADY_ONLY_ENAB()) {
		/* Gate power state interrupts */
		pciedev_clear_pwrstate_intmask(pciedev);
	}
	pciedev->pciecoreset = TRUE;

	++pciedev->metrics->perst_assrt_ct;
	if (pciedev->metric_ref->active) {
		pciedev->metrics->active_dur += OSL_SYSUPTIME() -
			pciedev->metric_ref->active;
		pciedev->metric_ref->active = 0;
	}
	pciedev->metric_ref->perst = OSL_SYSUPTIME();

	return BCME_OK;
}

/**
 * Called after the PCIe core receives an interrupt, because:
 *     a. one of the DMA engines in the PCIe core requires attention OR
 *     b. a message was received from the host or
 *     c. a message was sent by this device to the host
 *
 * Handle doorbell interrupt and dma completion interrupt for pcie dma device
 */
bool
pciedev_handle_interrupts(struct dngl_bus *pciedev)
{
	bool resched = FALSE;
	bool in_d3_suspend_stored = pciedev->in_d3_suspend;
	uint32 tx_status1, rx_status1;
	uint32 ts, cnt;
	ASSERT(pciedev);

	/* PCIe dma door bell interrupt */
	if (pciedev->intstatus) {
		/* check the mail box data first, if used */
		if ((pciedev->intstatus & pciedev->mb_intmask)) {
			pciedev_h2d_mb_data_check(pciedev); /* eg PCIe D0/D3 power handling */
		}
		/*  check for hostready (doorbell1) interrupt */
		if (pciedev->intstatus & PD_DEV0_DB1_INTMASK) {
			/* DAR access - set power enable before DAR by host
			 * disableafter receive doorbell interrupt
			 */
			if (PCIE_DAR_ENAB(pciedev)) {
				PCIEDEV_PWRREQ_CLEAR(pciedev);
			}
			pciedev_handle_hostready_intr(pciedev);
		}
		/*  check for door bell interrupt */
		if (pciedev->intstatus & PD_DEV0_DB0_INTMASK) {
			if (PCIE_DAR_ENAB(pciedev)) {
				PCIEDEV_PWRREQ_CLEAR(pciedev);
			}

			/* Increment IPC Stat */
			pciedev->metrics->num_h2d_doorbell++;

			ts = OSL_SYSUPTIME();
			/* Record the time */
			if (pciedev->ipc_data_supported) {
				cnt = pciedev->ipc_data->h2d_db0_cnt % PCIEDEV_IPC_MAX_H2D_DB0;
				pciedev->ipc_data->h2d_db0_cnt ++;
				pciedev->ipc_data->last_h2d_db0_time[cnt] = ts;
			}

			IPC_LOG(pciedev, DOORBELL_H2D, IPC_LOG_H2D_DB0, ts);

			/* If the doorbell0 is received at the same interrupt
			 * context of the D3-INFORM (doorbell1), ignore the
			 * doorbell0.
			 */
#ifdef PCIEDEV_INBAND_DW
			if (!pciedev->ds_inband_dw_supported &&
				pciedev->ds_oob_dw_supported &&
			    !in_d3_suspend_stored && pciedev->in_d3_suspend)
#else
			if (pciedev->ds_oob_dw_supported &&
				!in_d3_suspend_stored && pciedev->in_d3_suspend)
#endif /* PCIEDEV_INBAND_DW */
			{
				pciedev->bus_counters->h2d_doorbell_ignore++;
			}
			else
			{
#ifdef PCIE_PWRMGMT_CHECK
				if (!pciedev->bm_enabled) {
					/* If hostready is supported, DB0 cannot come before
					 * hostready
					 */
					if (pciedev->hostready) {
						PCI_PRINT(("DB0 without HostReady.\n"));
						OSL_SYS_HALT();
					}
					if (!HOSTREADY_ONLY_ENAB()) {
						pciedev->bus_counters->d0_miss_counter ++;
						if (pciedev->pciecoreset) {
							pciedev->bus_counters->missed_l0_count++;
							pciedev_handle_l0_enter(pciedev);
						}
						else {
							pciedev_handle_d0_enter(pciedev);
						}
						if (pciedev_handle_d0_enter_bm(pciedev) ==
							BCME_OK) {
							if (pciedev->bm_enab_timer_on) {
								dngl_del_timer(
									pciedev->bm_enab_timer);
								pciedev->bm_enab_timer_on = FALSE;
							}
						}
					}
					pciedev_enable_powerstate_interrupts(pciedev);
				}
#endif /* PCIE_PWRMGMT_CHECK */
			}

#ifdef PCIE_DMA_INDEX
			/* If supported and initialized dma/fetch r/w indices from host */
			if (!PCIE_IDMA_ACTIVE(pciedev) &&
			        (pciedev->dmaindex_h2d_wr.inited ||
			         pciedev->dmaindex_d2h_rd.inited))
			{
				pciedev_dmaindex_get(pciedev); /* fetch request */
			} else {
				pciedev_msgbuf_intr_process(pciedev);
			}
#else
			pciedev_msgbuf_intr_process(pciedev);  /* starts fetching host messages */
			/* clear the local int status bits */
#endif /* PCIE_DMA_INDEX */
		}
		/* HMAP interrupt priority is below Doorbell Interrupt */
		/* HMAP Violation */
		if ((pciedev->intstatus & PD_HMAP_VIO_INTMASK)) {
			pciedev_hmap_intr_process(pciedev);
			/* Trap [except while testing] */
			if (pciedev->hmaptest_inprogress) {
				PCI_PRINT(("hmaptest in progress not TRAP'ing\n"));
			} else {
				HND_DIE();
			}
		}
#if defined(BCMPCIE_IDMA)
		/* check for h2d2 door bell interrupt */
		if (PCIE_IDMA_ACTIVE(pciedev)) {
			if (pciedev->intstatus & PD_DEV2_INTMASK) {
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
				if (pciedev->ds_inband_dw_supported &&
					(pciedev->ds_state == DS_STATE_DEVICE_SLEEP)) {
					PCI_TRACE(("ignore H2D2 doorbell "
						"during dev sleep state\n"));

					if (PCIE_DAR_ENAB(pciedev)) {
						PCIEDEV_PWRREQ_CLEAR(pciedev);
					}

					if (pciedev->ipc_data_supported) {
						cnt = pciedev->ipc_data->h2d2_db0_in_ds_cnt %
							PCIEDEV_IPC_MAX_H2D_DB0;
						pciedev->ipc_data->h2d2_db0_in_ds_cnt++;
						pciedev->ipc_data->last_h2d2_db0_in_ds_time[cnt] =
							OSL_SYSUPTIME();
					}
				}
				else
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
				{
					PCI_TRACE(("H2D2 doorbell, ds_state %d\n",
						pciedev->ds_state));
					pciedev->idma_dmaxfer_active = TRUE;

					if (PCIE_DAR_ENAB(pciedev)) {
						PCIEDEV_PWRREQ_CLEAR(pciedev);
					}

					if (pciedev->ipc_data_supported) {
						cnt = pciedev->ipc_data->h2d2_db0_cnt %
							PCIEDEV_IPC_MAX_H2D_DB0;
						pciedev->ipc_data->h2d2_db0_cnt++;
						pciedev->ipc_data->last_h2d2_db0_time[cnt] =
							OSL_SYSUPTIME();
					}

					if (PCIE_IDMA_DS(pciedev)) {
						if (!pciedev->force_ht_on) {
							PCI_TRACE(("%s: requesting force HT "
								"for this core\n",
								__FUNCTION__));
							pciedev->force_ht_on = TRUE;
							pciedev_manage_h2d_dma_clocks(pciedev);
						}
						if (PCIE_IDMA_ACTIVE(pciedev)) {
							pciedev_idma_channel_enable(pciedev, TRUE);
						}
					}
				}
			}

#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
			/* H2D Dev1 db1 for device wake from device sleep */
			if (pciedev->intstatus & PD_DEV1_IDMA_DW_INTMASK) {
				PCI_TRACE(("H2D1 doorbell_1 for device wake, "
					"ds_state %d\n",
					pciedev->ds_state));
				if (PCIE_DAR_ENAB(pciedev)) {
					PCIEDEV_PWRREQ_CLEAR(pciedev);
				}

				if (pciedev->ds_state == DS_STATE_DEVICE_SLEEP) {
					pciedev_handle_deepsleep_dw_db(pciedev);
				} else if (pciedev->ds_state == DS_STATE_DEVICE_SLEEP_WAIT) {
					/* consider the race condition between
					 * ds ack and device wake
					 */
					PCI_TRACE(("handle deepsleep_dw doorbell "
						"received in DS_STATE_DEVICE_SLEEP_WAIT\n"));
					pciedev->ds_dev_wake_received = TRUE;
				}

				if (pciedev->ipc_data_supported) {
					cnt = pciedev->ipc_data->h2d1_db1_cnt %
						PCIEDEV_IPC_MAX_H2D_DB0;
					pciedev->ipc_data->h2d1_db1_cnt++;
					pciedev->ipc_data->last_h2d1_db1_time[cnt] =
					OSL_SYSUPTIME();
				}
			}
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
		}
#endif /* BCMPCIE_IDMA */
		if (pciedev->intstatus & PD_DEV0_PERST_INTMASK) {
			/* HW JIRA: CRWLPCIEGEN2-178: H2D DMA
			* corruption after L2_3 (perst assertion)
			*/
#ifdef PCIE_PWRMGMT_CHECK
			if ((pciedev->real_d3 == FALSE) && (pciedev_ds_in_host_sleep(pciedev))) {
				/* Seen cases where D3 didn't happen before PERST */
				PCI_ERROR(("PERST: faking the D3 enter to avoid deadlock\n"));
				pciedev->bus_counters->missed_d3_count++;
				pciedev_handle_d3_enter(pciedev);
			}
#endif // endif
			PCI_ERROR(("PCIe PERST# Asserted, reset\n"));
			pciedev_handle_perst_intr(pciedev);

			if (pciedev->induce_trap4) {
				/* Access a config register during PERST# for AXI trap */
				(void)R_REG_CFG(pciedev, PCI_CFG_CMD);
			}
		}
#ifdef PCIE_PWRMGMT_CHECK
		if (pciedev->intstatus & PD_DEV0_PWRSTATE_INTMASK) {
			pciedev_handle_pwrmgmt_intr(pciedev);
		}
#endif /* PCIE_PWRMGMT_CHECK */
#ifdef PCIE_ERR_ATTN_CHECK
		if (pciedev->intstatus & PD_ERR_ATTN_INTMASK) {
			pciedev_dump_err_attn_logs(pciedev);
			HND_DIE();
		}
		if (pciedev->intstatus & PD_LINK_DOWN_INTMASK) {
			PCI_PRINT(("Unexpected Link down !!\n"));
			pciedev_dump_err_attn_logs(pciedev);
			PCIEDEV_LINKDOWN_CNT_INCR(pciedev);
			if (!pciedev->linkdown_trap_dis) {
				HND_DIE();
			}
		}
#endif // endif
#ifdef BCMPCIE_D2H_MSI
		/* MSI FIFO Overflow */
		if (pciedev->intstatus & PD_MSI_FIFO_OVERFLOW_INTMASK) {
			pciedev_handle_msi_fifo_overflow_intr(pciedev);
		}
#endif // endif
		/* Check for the dma pending interrupt */
		pciedev->intstatus = 0;
	}

	/* PCIe DMA completion interrupts : H2D */
	if (pciedev->h2dintstatus[DMA_CH0]) {
		/* dma from h2d side */
		if (pciedev->h2dintstatus[DMA_CH0] & I_PD) {
			/* Since we can not say which intr comes first (err_attn or dma err),
			 *  dump the error attention logs also and enforce TRAP
			*/
#ifdef PCIE_ERR_ATTN_CHECK
			pciedev_dump_err_attn_logs(pciedev);
#endif // endif
			tx_status1 = pciedev->regs->u.pcie2.h2d0_dmaregs.tx.status1;
			rx_status1 = pciedev->regs->u.pcie2.h2d0_dmaregs.rx.status1;
			PCI_PRINT(("H2D DMA data transfer error !!!\n"));
			PCI_PRINT(("tx_status1:0x%x, rx_status1:0x%x\n", tx_status1, rx_status1));
			HND_DIE();
		}
		if (pciedev->h2dintstatus[DMA_CH0] & I_ERRORS) {
			TRAP_DUE_DMA_ERROR(("bad h2d dma intstatus: 0x%04x\n",
				pciedev->h2dintstatus[DMA_CH0]));
		} else if (pciedev->h2dintstatus[DMA_CH0] & I_RI) {
			pciedev_handle_h2d_dma(pciedev, DMA_CH0);
		}
		pciedev->h2dintstatus[DMA_CH0] = 0;
	}

	/* PCIe DMA2 completion interrupts : H2D */
#ifdef BCMPCIE_IDMA
	if (PCIE_IDMA_ACTIVE(pciedev) && pciedev->h2dintstatus[DMA_CH2]) {
		PCI_TRACE(("IDMA done, ds_state %d\n", pciedev->ds_state));
		pciedev->idma_dmaxfer_active = FALSE;
		if (PCIE_IDMA_DS(pciedev)) {
			if (!pciedev->ds_oob_dw_supported && !pciedev->ds_inband_dw_supported) {
				for (cnt = 0; cnt < MAX_DMA_CHAN; cnt++) {
					if (VALID_DMA_CHAN_NO_IDMA(cnt, pciedev) &&
						pciedev->h2d_di[cnt]) {
						if (!h2d_dma_not_pending(pciedev, cnt)) {
							break;
						}
					}
				}
				if (pciedev->force_ht_on) {
					PCI_TRACE(("%s: idma complete disable force HT "
						"for this core\n",
						__FUNCTION__));
					pciedev->force_ht_on = FALSE;
					pciedev_manage_h2d_dma_clocks(pciedev);
				}
			}
#ifdef PCIE_DEEP_SLEEP
			if (PCIE_IDMA_ACTIVE(pciedev)) {
				if (pciedev->ds_oob_dw_supported &&
					pciedev->ds_state == DS_D0_STATE) {
					pciedev_idma_channel_enable(pciedev, FALSE);
				}
			}
#endif /* PCIE_DEEP_SLEEP */
		}

		/* dma from h2d side */
		if (pciedev->h2dintstatus[DMA_CH2] & I_RI) {
			/* There might be new messages in the circular buffer.
			* Schedule DMA for those too
			*/
			pciedev_msgbuf_intr_process(pciedev);
		} else {
			PCI_ERROR(("idma: bad h2d dma2 intst 0x%8x, intstidx 0x%8x\n",
				pciedev->h2dintstatus[DMA_CH2], pciedev->idma_info->index));

			if (pciedev->h2dintstatus[DMA_CH2] & (I_PC | I_PD | I_DE | I_RU)) {
				PCI_ERROR(("idma desc reload from error\n"));
				pciedev_dma_params_init_h2d(pciedev, pciedev->h2d_di[DMA_CH2]);
				if (pciedev_idma_desc_load(pciedev) != BCME_OK) {
					PCI_ERROR(("%s: implicit dma desc load fail\n",
						__FUNCTION__));
					pciedev->idma_info->desc_loaded = FALSE;
				} else {
					pciedev->idma_info->desc_loaded = TRUE;
				}
			}
		}
		pciedev->h2dintstatus[DMA_CH2] = 0;

		if (pciedev->ipc_data_supported) {
			cnt = pciedev->ipc_data->h2d2_idma_cnt %
				PCIEDEV_IPC_MAX_H2D_DB0;
			pciedev->ipc_data->h2d2_idma_cnt++;
			pciedev->ipc_data->last_idma_done_time[cnt] =
				OSL_SYSUPTIME();
		}
	}
#endif /* BCMPCIE_IDMA */

#ifdef BCMPCIE_DMA_CH1
	if (PCIE_DMA1_ENAB(pciedev) && pciedev->h2dintstatus[DMA_CH1]) {
		/* dma1 from h2d side */
		pciedev->h2dintstatus[DMA_CH1] = 0;
		pciedev->dma_ch_info->dma_ch1_h2d_cnt++;
	}
#endif /* BCMPCIE_DMA_CH1 */

#ifdef BCMPCIE_DMA_CH2
	if (PCIE_DMA2_ENAB(pciedev) && pciedev->h2dintstatus[DMA_CH2]) {
		/* dma2 from h2d side */
		if (pciedev->h2dintstatus[DMA_CH2] & I_RI) {
			pciedev_handle_h2d_dma(pciedev, DMA_CH2);
		} else {
			PCI_ERROR(("bad h2d dma2 intstatus. 0x%8x\n",
				pciedev->h2dintstatus[DMA_CH2]));
			DBG_BUS_INC(pciedev, h2d_dpc_bad_dma_sts);
		}
		pciedev->h2dintstatus[DMA_CH2] = 0;
		pciedev->dma_ch_info->dma_ch2_h2d_cnt++;
	}
#endif /* BCMPCIE_DMA_CH2 */

	/* PCIe DMA completion interrupts : D2H */
	if (pciedev->d2hintstatus[DMA_CH0]) {
		if (pciedev->d2hintstatus[DMA_CH0] & I_PD) {
		     /* Since we can not say which intr comes first (err_attn or dma err),
		                 *  dump the error attention logs also and enforce TRAP
		                */
#ifdef PCIE_ERR_ATTN_CHECK
			pciedev_dump_err_attn_logs(pciedev);
#endif // endif
			tx_status1 = pciedev->regs->u.pcie2.d2h0_dmaregs.tx.status1;
			rx_status1 = pciedev->regs->u.pcie2.d2h0_dmaregs.rx.status1;
			PCI_PRINT(("D2H DMA data transfer error !!!\n"));
			PCI_PRINT(("tx_status1:0x%x, rx_status1:0x%x\n", tx_status1, rx_status1));
			HND_DIE();
		}
		if (pciedev->d2hintstatus[DMA_CH0] & I_ERRORS) {
			TRAP_DUE_DMA_ERROR(("bad d2h dma intstatus. 0x%04x\n",
				pciedev->d2hintstatus[DMA_CH0]));
		} else if (pciedev->d2hintstatus[DMA_CH0] & I_RI) {
			pciedev_handle_d2h_dma(pciedev, DMA_CH0);
		}
		pciedev->d2hintstatus[DMA_CH0] = 0;
	}
#ifdef BCMPCIE_DMA_CH1
	if (PCIE_DMA1_ENAB(pciedev) && pciedev->d2hintstatus[DMA_CH1]) {
		if (pciedev->d2hintstatus[DMA_CH1] & I_RI) {
			pciedev_handle_d2h_dma(pciedev, DMA_CH1);
		} else {
			PCI_ERROR(("bad d2h dma1 intstatus. 0x%8x\n",
				pciedev->d2hintstatus[DMA_CH1]));
			while (1);
		}
		pciedev->d2hintstatus[DMA_CH1] = 0;
		pciedev->dma_ch_info->dma_ch1_d2h_cnt++;
	}
#endif /* BCMPCIE_DMA_CH1 */
#ifdef BCMPCIE_DMA_CH2
	if (PCIE_DMA2_ENAB(pciedev) && pciedev->d2hintstatus[DMA_CH2]) {
		if (pciedev->d2hintstatus[DMA_CH2] & I_RI) {
			pciedev_handle_d2h_dma(pciedev, DMA_CH2);
		} else {
			PCI_ERROR(("bad d2h dma2 intstatus. 0x%8x\n",
				pciedev->d2hintstatus[DMA_CH2]));
			while (1);
		}
		pciedev->d2hintstatus[DMA_CH2] = 0;
		pciedev->dma_ch_info->dma_ch2_d2h_cnt++;
	}
#endif /* BCMPCIE_DMA_CH2 */
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	if (pciedev->ds_inband_dw_supported &&
		(pciedev->ds_state == DS_STATE_HOST_SLEEP_PEND_TOP)) {
		pciedev_host_sleep_ack_check(pciedev);
	} else
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
#ifdef PCIEDEV_INBAND_DW
	if (!pciedev->ds_inband_dw_supported &&
		pciedev->in_d3_suspend && pciedev->d3_ack_pending)
#else
	if (pciedev->in_d3_suspend && pciedev->d3_ack_pending)
#endif /* PCIEDEV_INBAND_DW */
	{
		pciedev_D3_ack(pciedev);
	}

	/* Rescheduling if processing not over */
	if ((pciedev->intstatus |
			pciedev->h2dintstatus[DMA_CH0] | pciedev->d2hintstatus[DMA_CH0]) ||
#if defined(BCMPCIE_IDMA)
		(PCIE_IDMA_ACTIVE(pciedev) &&
		pciedev->h2dintstatus[DMA_CH2]) ||
#endif /* BCMPCIE_IDMA */
#ifdef BCMPCIE_DMA_CH2
		(PCIE_DMA2_ENAB(pciedev) &&
			(pciedev->h2dintstatus[DMA_CH2] | pciedev->d2hintstatus[DMA_CH2])) ||
#endif /* BCMPCIE_DMA_CH2 */
#ifdef BCMPCIE_DMA_CH1
		(PCIE_DMA1_ENAB(pciedev) &&
			(pciedev->h2dintstatus[DMA_CH1] | pciedev->d2hintstatus[DMA_CH1])) ||
#endif /* BCMPCIE_DMA_CH1 */
		0) {
		resched = TRUE;
	}

	return resched;
} /* pciedev_dpc */

/**
 * Once the host has datafilled the pcie_ipc_rings info for a ring, link the bus
 * layer msgbuf_ring to it. Since a msgbuf_ring_t in dongle is directly
 * referencing the pcie_ipc_rings_mem, the linking is mostly in place.
 *
 * Here we only need to allocate the lcl buffers for the lcl_buf_pool.
 */
static int
pciedev_msgbuf_link_pcie_ipc(struct dngl_bus *pciedev, msgbuf_ring_t *ring,
	msgring_process_message_t process_rtn)
{
	uint8 i = 0;
	lcl_buf_pool_t * pool;

	if ((ring->inited == TRUE) || (ring->buf_pool == NULL))
		return BCME_OK;

	if (HADDR64_IS_ZERO(MSGBUF_HADDR64(ring)))
	{
		PCI_ERROR(("ring: %c%c%c%c msgbuf_ring host base not inited\n",
			ring->name[0], ring->name[1], ring->name[2], ring->name[3]));
		DBG_BUS_INC(pciedev, link_host_msgbuf_failed);
		return BCME_BADARG;
	}

	pool = ring->buf_pool;
	for (i = 0; i < pool->buf_cnt; i++) {
		/* only freed in detach */
		pool->buf[i].p = /* uses the host provided work item_size */
			MALLOC_PERSIST(pciedev->osh, pool->item_cnt * MSGBUF_LEN(ring));
	}
	pool->availcnt = pool->buf_cnt;
	ring->process_rtn = process_rtn;
	ring->inited = TRUE;

	return BCME_OK;

} /* pciedev_msgbuf_link_pcie_ipc */

/**
 * TxPost flowrings are instantiated using explicit vreate flowring messages,
 * which will contain the host side base address of the flowring.
 * The pcie_ipc_ring_mem::haddr64 is not yet set and dongle will set it up in
 * pciedev_flowring_sync_pcie_ipc() at flowring creation time.
 * Here we simply create the shared circular buffer pool and share it with all
 * pre-allocated flowrings table.
 */
static int
pciedev_flowrings_link_pcie_ipc(struct dngl_bus *pciedev)
{
	int i;
	msgbuf_ring_t *flow_ring;

	/* Allocated common circular local buffer for Flow rings */
	ASSERT(pciedev->txpost_item_size != 0);

	pciedev->flowrings_cir_buf_pool = pciedev_setup_flowrings_cpool(pciedev,
		PCIEDEV_MAX_LOCALBUF_PKT_COUNT, pciedev->txpost_item_size);
	if (!pciedev->flowrings_cir_buf_pool) {
		PCI_ERROR(("Link flowring: setup cir_buf_pool failure\n"));
		return BCME_ERROR;
	}

	for (i = 0; i < pciedev->max_flowrings; i++) {
		flow_ring = &pciedev->flowrings_table[i];

		flow_ring->cbuf_pool = pciedev->flowrings_cir_buf_pool;

#ifdef PCIE_DMA_INDEX
		flow_ring->dmaindex_d2h_supported =
			(pciedev->dmaindex_h2d_rd.inited) ? TRUE : FALSE;
		flow_ring->dmaindex_h2d_supported =
			(pciedev->dmaindex_h2d_wr.inited) ? TRUE : FALSE;
#else /* !PCIE_DMA_INDEX */
		flow_ring->dmaindex_d2h_supported = FALSE;
		flow_ring->dmaindex_h2d_supported = FALSE;
#endif /* !PCIE_DMA_INDEX */
	}

	return BCME_OK;

} /* pciedev_flowrings_link_pcie_ipc */

/** Populate the pcie_ipc_ring_mem with host side configuration received in a
 * H2D flow ring create control message.
 * NOTE: Flowrings use fields in pcie_ipc_ring_mem in native host order.
 */
static int
pciedev_flowrings_sync_pcie_ipc(struct dngl_bus *pciedev,
	msgbuf_ring_t *flow_ring, uint16 flow_ring_id,
	haddr64_t haddr64, uint16 max_items, uint8 item_type, uint16 item_size)
{
	ASSERT(MSGBUF_IPC_RING_MEM(flow_ring) != NULL);

	MSGBUF_IPC_RING_ID(flow_ring) = flow_ring_id;

	HADDR64_SET(MSGBUF_HADDR64(flow_ring), haddr64);

	MSGBUF_ITEM_TYPE(flow_ring) = item_type;
	MSGBUF_MAX(flow_ring) = max_items;
	MSGBUF_LEN(flow_ring) = item_size;

	PCI_INFORM(("flow_ring %u sync " HADDR64_FMT " type %u max %u size %u\n",
		flow_ring_id, HADDR64_VAL(haddr64), item_type, max_items, item_size));

	return BCME_OK;

} /* pciedev_flowrings_sync_pcie_ipc */

/** Device detach */
void
BCMATTACHFN(pciedev_detach)(struct dngl_bus *pciedev)
{
	PCI_TRACE(("pciedev_detach\n"));

	pciedev->up = FALSE;

	if (pciedev->m2m_req)
		MFREE(pciedev->osh, pciedev->m2m_req, sizeof(pcie_m2m_req_t) +
			(PCIE_M2M_REQ_VEC_MAX * sizeof(pcie_m2m_vec_t)));

#if defined(HNDBME) && defined(BCM_BUZZZ) && defined(BCM_BUZZZ_STREAMING_BUILD)
	{
		uint8 eng_idx;
		int bme_key;
		for (eng_idx = 0; eng_idx < 2; eng_idx++) {
			bme_key = pciedev->bme_key[eng_idx];
			if ((bme_key != 0) && (bme_key != BME_INVALID)) {
				bme_unregister_user(pciedev->osh, bme_key);
				pciedev->bme_key[eng_idx] = 0;
			}
		}
	}
#endif /* HNDBME && BCM_BUZZZ && BCM_BUZZZ_STREAMING_BUILD */

	if (pciedev->ds_log)
		MFREE(pciedev->osh, pciedev->ds_log, sizeof(pciedev_deepsleep_log_t));

	if (pciedev->metrics)
		MFREE(pciedev->osh, pciedev->metrics, sizeof(pcie_bus_metrics_t));

	if (pciedev->metric_ref)
		MFREE(pciedev->osh, pciedev->metric_ref, sizeof(metric_ref_t));

#ifdef UART_TRANSPORT
	/* Allocate uart event inds mask */
	if (pciedev->uart_event_inds_mask)
		MFREE(pciedev->osh, pciedev->uart_event_inds_mask,
				(sizeof(uint8) * WL_EVENTING_MASK_LEN));
#endif // endif

	pciedev_cleanup_msgbuf_lpool(pciedev, pciedev->info_submit_ring);
	pciedev_cleanup_msgbuf_lpool(pciedev, pciedev->info_cpl_ring);

#if defined(BCMPCIE_IDMA) && !defined(BCMPCIE_IDMA_DISABLED)
	pciedev_cleanup_idma(pciedev);
#endif // endif
#ifdef PCIE_DMA_INDEX
	pciedev_cleanup_dmaindex(pciedev);
#endif // endif
	pciedev_cleanup_flowrings(pciedev);
	pciedev_cleanup_common_rings(pciedev);

	if (pciedev->tunables) {

		/* detach flow ring pool */
		dll_pool_detach(pciedev->osh, pciedev->flowrings_dll_pool,
			pciedev->max_flowrings, sizeof(flowring_pool_t));

		MFREE(pciedev->osh, pciedev->tunables, (sizeof(uint32) * MAXTUNABLE));
	}

	if (pciedev->err_cntr)
		MFREE(pciedev->osh, pciedev->err_cntr, sizeof(pciedev_err_cntrs_t));

	if (pciedev->dbg_bus)
		MFREE(pciedev->osh, pciedev->dbg_bus, sizeof(pciedev_dbg_bus_cntrs_t));

	/* Free device state */
	if (pciedev->dngl)
		dngl_detach(pciedev->dngl);

	/* Put the core back into reset */
	if (pciedev->sih) {
		pciedev_reset(pciedev);
		si_core_disable(pciedev->sih, 0);

		/* Detach from SB bus */
		MODULE_DETACH(pciedev->sih, si_detach);
	}

#if defined(PCIE_DMA_INDEX) && defined(PCIE_DMA_INDEX_DEBUG)
	/* Free dmaindex logging blocks */
	if (pciedev->dmaindex_log_h2d) {
		MFREE(pciedev->osh, pciedev->dmaindex_log_h2d,
			PCIE_DMAINDEX_LOG_MAX_ENTRY * sizeof(dmaindex_log_t));
	}
	if (pciedev->dmaindex_log_d2h) {
		MFREE(pciedev->osh, pciedev->dmaindex_log_d2h,
			PCIE_DMAINDEX_LOG_MAX_ENTRY * sizeof(dmaindex_log_t));
	}
#endif /* PCIE_DMA_INDEX && PCIE_DMA_INDEX_DEBUG */

#ifdef BCMPCIE_IDMA
	if (pciedev->idma_info) {
		if (PCIE_IDMA_ENAB(pciedev)) {
			if (pciedev->idma_info->dmaindex) {
				MFREE(pciedev->osh, pciedev->idma_info->dmaindex,
					pciedev->idma_info->dmaindex_sz);
			}
			MFREE(pciedev->osh, pciedev->idma_info, sizeof(idma_info_t));
		}
	}
#endif /* BCMPCIE_IDMA */

#if defined(BCMPCIE_DMA_CH1) || defined(BCMPCIE_DMA_CH2)
	if (pciedev->dma_ch_info) {
		MFREE(pciedev->osh, pciedev->dma_ch_info,
				sizeof(dma_ch_info_t));
	}
#endif /* BCMPCIE_DMA_CH1 || BCMPCIE_DMA_CH2 */

	/* free ioct_resp_buf */
	if (pciedev->ioct_resp_buf)
		MFREE(pciedev->osh, pciedev->ioct_resp_buf, sizeof(*pciedev->ioct_resp_buf));

#ifdef PCIE_DMAXFER_LOOPBACK
	pktq_deinit(&pciedev->dmaxfer_loopback->lpbk_dma_txq);
	pktq_deinit(&pciedev->dmaxfer_loopback->lpbk_dma_txq_pend);
#endif	/* PCIE_DMAXFER_LOOPBACK */

	pktq_deinit(&pciedev->d2h_req_q);

	if (pciedev->ctrl_resp_q)
		MFREE(pciedev->osh, pciedev->ctrl_resp_q, sizeof(pciedev_ctrl_resp_q_t));

	/* Free ipc_data, used for storing data for IPC stats */
	if (pciedev->ipc_data_supported)
		MFREE(pciedev->osh, pciedev->ipc_data, sizeof(ipc_data_t));

	/* Free bus_counters, used for storing bus counter stats */
	if (pciedev->bus_counters)
		MFREE(pciedev->osh, pciedev->bus_counters, sizeof(pcie_bus_counters_t));

	/* Free chip state */
	MFREE(pciedev->osh, pciedev, sizeof(struct dngl_bus));
} /* pciedev_detach */

/** Section for PCIE IPC Dongle Host Training Phases */

static const char BCMATTACHDATA(rstr_h2dctl)[] = "h2dctl";
static const char BCMATTACHDATA(rstr_h2drx)[]  = "h2drx";
static const char BCMATTACHDATA(rstr_d2hctl)[] = "d2hctl";
static const char BCMATTACHDATA(rstr_d2htx)[]  = "d2htx";
static const char BCMATTACHDATA(rstr_d2hrx)[]  = "d2hrx";

/**
 * Invoked in attach phase to setup a table of msgbuf_ring for the common rings.
 * Setup each common ring and furnish it with a lcl_buf_pool.
 */
static int
BCMATTACHFN(pciedev_setup_common_rings)(struct dngl_bus *pciedev)
{
	int ret;
	uint16 ringid;
	msgbuf_ring_t *ring;
#ifdef PCIE_DMA_INDEX
	/* When dmaindex## is enabled, common rings assume that host will support */
	bool dmaindex_supported = TRUE;
#else
	bool dmaindex_supported = FALSE;
#endif /* PCIE_DMA_INDEX */

	if (pciedev->cmn_msgbuf_ring != NULL) {
		PCI_ERROR(("Setup common rings: cmn_msgbuf_ring non NULL\n"));
		return BCME_OK;
	}

	/* Allocate msgbuf_ring objesct for all common rings */
	if (!(pciedev->cmn_msgbuf_ring = (msgbuf_ring_t*) MALLOCZ(pciedev->osh,
	        (BCMPCIE_COMMON_MSGRINGS * sizeof(msgbuf_ring_t)))))
	{
		PCI_ERROR(("Setup common rings: malloc cmn_msgbuf_ring failure\n"));
		ret = BCME_NOMEM;
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		goto fail_cleanup;
	}

	/* Setup each common ring and attach a lcl_buf_pool */

	/* #0. H2D control submission */
	ringid = BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT;
	ring = &pciedev->cmn_msgbuf_ring[ringid];
	if ((ret = pciedev_setup_msgbuf_lpool(pciedev, ring, rstr_h2dctl, ringid,
	        CTRL_SUB_BUFCNT, 1, PCIEDEV_WORK_ITEM_FETCH_LIMIT_CTRL,
	        dmaindex_supported, dmaindex_supported, FALSE)) != BCME_OK)
	{
		PCI_ERROR(("Setup common rings: setup htod_ctrl failure %d\n", ret));
		goto fail_cleanup;
	}
	pciedev->htod_ctrl = ring;

	/* #1. H2D Rx post */
	ringid = BCMPCIE_H2D_MSGRING_RXPOST_SUBMIT;
	ring = &pciedev->cmn_msgbuf_ring[ringid];
	if ((ret = pciedev_setup_msgbuf_lpool(pciedev, ring, rstr_h2drx, ringid,
	        pciedev->tunables[H2DRXPOST], 32, PCIEDEV_WORK_ITEM_FETCH_NO_LIMIT,
	        dmaindex_supported, dmaindex_supported, TRUE)) != BCME_OK)
	{
		PCI_ERROR(("Setup common rings: setup htod_rx failure %d\n", ret));
		goto fail_cleanup;
	}
	pciedev->htod_rx = ring;

	/* #2. D2H control completion */
	ringid = BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE;
	ring = &pciedev->cmn_msgbuf_ring[ringid];
	if ((ret = pciedev_setup_msgbuf_lpool(pciedev, ring, rstr_d2hctl, ringid,
	        8, 1, PCIEDEV_WORK_ITEM_FETCH_LIMIT_UNDEF,
	        dmaindex_supported, dmaindex_supported, FALSE)) != BCME_OK)
	{
		PCI_ERROR(("Setup common rings: setup dtoh_ctrlcpl failure %d\n", ret));
		goto fail_cleanup;
	}
	pciedev->dtoh_ctrlcpl = ring;

	/* #3. D2H tx complete */
	ringid = BCMPCIE_D2H_MSGRING_TX_COMPLETE;
	ring = &pciedev->cmn_msgbuf_ring[ringid];
	if ((ret = pciedev_setup_msgbuf_lpool(pciedev, ring, rstr_d2htx, ringid,
	        pciedev->tunables[D2HTXCPL], pciedev->tunables[MAXTXSTATUS],
	        PCIEDEV_WORK_ITEM_FETCH_LIMIT_UNDEF,
	        dmaindex_supported, dmaindex_supported, FALSE)) != BCME_OK)
	{
		PCI_ERROR(("Setup common rings: setup dtoh_txcpl failure %d\n", ret));
		goto fail_cleanup;
	}
	pciedev->dtoh_txcpl = ring;

	/* #4. D2H rx complete */
	ringid = BCMPCIE_D2H_MSGRING_RX_COMPLETE;
	ring = &pciedev->cmn_msgbuf_ring[ringid];
	if ((ret = pciedev_setup_msgbuf_lpool(pciedev, ring, rstr_d2hrx, ringid,
	        pciedev->tunables[D2HRXCPL], 32,
	        PCIEDEV_WORK_ITEM_FETCH_LIMIT_UNDEF,
	        dmaindex_supported, dmaindex_supported, FALSE)) != BCME_OK)
	{
		PCI_ERROR(("Setup common rings: setup dtoh_rxcpl failure %d\n", ret));
		goto fail_cleanup;
	}
	pciedev->dtoh_rxcpl = ring;

#ifdef PCIE_DEBUG_DUMP_COMMON_RING
	pciedev_dump_common_ring(pciedev->htod_rx);
	pciedev_dump_common_ring(pciedev->htod_ctrl);
	pciedev_dump_common_ring(pciedev->dtoh_txcpl);
	pciedev_dump_common_ring(pciedev->dtoh_rxcpl);
	pciedev_dump_common_ring(pciedev->dtoh_ctrlcpl);
#endif /* PCIE_DEBUG_DUMP_COMMON_RING */

	return BCME_OK;

fail_cleanup:

	pciedev_cleanup_common_rings(pciedev);

	return ret;

} /* pciedev_setup_common_rings */

/** Free common_ring resources allocated by pciedev_setup_common_rings() */
static void
BCMATTACHFN(pciedev_cleanup_common_rings)(struct dngl_bus *pciedev)
{
	if (pciedev->cmn_msgbuf_ring != NULL)
	{
		/* Free up per common ring lcl_buf_pool resources, if allocated */
		pciedev_cleanup_msgbuf_lpool(pciedev, pciedev->dtoh_rxcpl);
		pciedev_cleanup_msgbuf_lpool(pciedev, pciedev->dtoh_txcpl);
		pciedev_cleanup_msgbuf_lpool(pciedev, pciedev->dtoh_ctrlcpl);
		pciedev_cleanup_msgbuf_lpool(pciedev, pciedev->htod_rx);
		pciedev_cleanup_msgbuf_lpool(pciedev, pciedev->htod_ctrl);

		/* Free up the table of common msgbuf_rings */
		MFREE(pciedev->osh, pciedev->cmn_msgbuf_ring,
			(BCMPCIE_COMMON_MSGRINGS * sizeof(msgbuf_ring_t)));

		pciedev->htod_ctrl       = NULL;
		pciedev->htod_rx         = NULL;
		pciedev->dtoh_ctrlcpl    = NULL;
		pciedev->dtoh_txcpl      = NULL;
		pciedev->dtoh_rxcpl      = NULL;

		pciedev->cmn_msgbuf_ring = NULL;
	}

} /* pciedev_cleanup_common_rings */

/**
 * Helper to setup a msgbuf_ring and attach a lcl_buf_pool. Used for common
 * rings and support info/debug rings, too.
 *
 * Individual buffers for the pool will be allocated during link phase, as the
 * work item size will be known then. see pciedev_msgbuf_link_pcie_ipc()
 *
 * Flowrings do not use pciedev_setup_msgbuf_lpool(). Flowrings use a single
 * shared cir_buf_pool instead of per ring lcl_buf. The shared cir_buf_pool is
 * setup in pciedev_flowrings_link_pcie_ipc() and all flowrings point to it.
 */
static int
BCMATTACHFN(pciedev_setup_msgbuf_lpool)(struct dngl_bus *pciedev,
	msgbuf_ring_t *ring, const char *name, uint16 ringid,
	uint8 bufcnt, uint8 item_cnt, uint16 fetch_limit,
	bool dmaindex_d2h_supported, bool dmaindex_h2d_supported, bool inusepool)
{
	uint8  name_len = 0;
	uint16 size = 0;
	uint8 i = 0;
	lcl_buf_pool_t *buf_pool;

	if (ring == NULL)
		return BCME_ERROR;

	name_len = strlen(name);
	if (name_len > (MSGBUF_RING_NAME_LEN - 1)) {
		name_len = MSGBUF_RING_NAME_LEN - 1;
	}
	bcopy(name, &ring->name[0], name_len);

	ring->pciedev = pciedev;
	ring->ringid = ringid; /* may be undefined for dynamic info/debug rings */

	ring->current_phase =  MSGBUF_RING_INIT_PHASE;
#ifdef H2D_CHECK_SEQNUM
	ring->h2d_seqnum = H2D_EPOCH_INIT_VAL; /* Init epoch value */
#endif /* H2D_CHECK_SEQNUM */

	ring->dmaindex_d2h_supported = dmaindex_d2h_supported;
	ring->dmaindex_h2d_supported = dmaindex_h2d_supported;

	ring->work_item_fetch_limit = fetch_limit;

	/* allocate memory for a lcl_buf_pool and a list of lcl_buf */
	if (bufcnt) {
		size = sizeof(lcl_buf_pool_t) + (bufcnt * sizeof(lcl_buf_t));

		if (!(buf_pool = (lcl_buf_pool_t*) MALLOCZ(pciedev->osh, size))) {
			PCI_ERROR(("malloc msgbuf ring lcl_buf_pool failure\n"));
			PCIEDEV_MALLOC_ERR_INCR(pciedev);
			DBG_BUS_INC(pciedev, alloc_ring_failed);
			return BCME_NOMEM;
		}

		/* Initialize head/tail/free pointers */
		buf_pool->head = &(buf_pool->buf[0]);
		buf_pool->free = &(buf_pool->buf[0]);
		buf_pool->tail = &(buf_pool->buf[bufcnt - 1]);

		buf_pool->buf_cnt = bufcnt;
		buf_pool->item_cnt = item_cnt;

		/* Link up nodes */
		buf_pool->head->prv = NULL;
		buf_pool->tail->nxt = NULL;

		for (i = 1; i < bufcnt; i++) {
			buf_pool->buf[i - 1].nxt = &(buf_pool->buf[i]);
			buf_pool->buf[i].prv     = &(buf_pool->buf[i - 1]);
		}

		ring->buf_pool = buf_pool; /* common and info/dbg rings use lcl_buf */

		/* Initialize inuse pool */
		if (inusepool)
			pciedev_init_inuse_lclbuf_pool_t(pciedev, ring);
	}

	ring->cbuf_pool = NULL; /* only flowrings use cir_buf */

	return BCME_OK;

} /* pciedev_setup_msgbuf_lpool */

/**
 * Cleanup resources allocated by pciedev_setup_msgbuf_lpool.
 *
 * If a msgbuf went through a link phase, then the buffers allocated to the pool
 * also need to be freed.
 */
static void
pciedev_cleanup_msgbuf_lpool(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	uint8 i = 0;
	lcl_buf_pool_t *buf_pool;

	if (ring == NULL) {
		PCI_TRACE(("pciedev_cleanup_msgbuf_lpool: ring is NULL\n"));
		return;
	}

	PCI_TRACE(("Cleanup msgbuf %c%c%c%c lcl_buf_pool\n",
		ring->name[0], ring->name[1], ring->name[2], ring->name[3]));

	buf_pool = ring->buf_pool;

	if (buf_pool) {

		/* msgbuf_ring went through a link phase, free individual buffers */
		if (ring->inited) {
			int buf_sz = buf_pool->item_cnt * MSGBUF_LEN(ring);

			for (i = 0; i < buf_pool->buf_cnt; i++) {
				if (buf_pool->buf[i].p) {
					MFREE(pciedev->osh, buf_pool->buf[i].p, buf_sz);
				}
			}
		}

		/* free up inuse buf buf_pool */
		pciedev_free_inuse_bufpool(pciedev, buf_pool);

		/* free up the buf_pool structure */
		MFREE(pciedev->osh, buf_pool,
			sizeof(lcl_buf_pool_t) + (buf_pool->buf_cnt * sizeof(lcl_buf_t)));
	}

	bzero(ring, sizeof(msgbuf_ring_t));

} /* pciedev_cleanup_msgbuf_lpool */

/**
 * Invoked by pciedev_attach to allocate various objects for TxPost Flowrings.
 *
 * All TxPost flowrings share a single cir_buf pool (instead of lcl_buf) which
 * will be allocated during pciedev_flowrings_link_pcie_ipc, when the TxPost
 * work item size is known. All flowrings use the same fixed work item size.
 */
static int
BCMATTACHFN(pciedev_setup_flowrings)(struct dngl_bus *pciedev)
{
	int i;
	int malloc_size;
#ifdef MEM_ALLOC_STATS
	memuse_info_t mu;
#endif /* MEM_ALLOC_STATS */

	PCI_TRACE(("Setup flowrings: max_flowrings is %d\n",
		pciedev->max_flowrings));

	if (pciedev->h2d_submitring_ptr != NULL) {
		PCI_ERROR(("Setup flowrings: h2d_submitring_ptr is not NULL\n"));
		return BCME_OK;
	}

	/* Allocate flowring indirection table.
	 * h2d_submitring_ptr is indexed by flowid and provides a pointer to a
	 * msgbuf_ring_t allocated from the flowrings_table
	 * h2d_submitring_ptr includes slots for the two H2D common rings (unused)
	 */
	malloc_size = (pciedev->max_h2d_rings * sizeof(msgbuf_ring_t *));
	if (!(pciedev->h2d_submitring_ptr = (msgbuf_ring_t **)
	        MALLOCZ(pciedev->osh, malloc_size)))
	{
		PCI_ERROR(("Setup flowrings: malloc %d h2d_submitring_ptr failure\n",
			malloc_size));
		goto fail_cleanup;
	}

	/* Allocate pool of msgbuf_ring for flowrings */
	malloc_size = pciedev->max_flowrings * sizeof(msgbuf_ring_t);
	if (!(pciedev->flowrings_table = (msgbuf_ring_t*)
	        MALLOCZ(pciedev->osh, malloc_size)))
	{
		PCI_ERROR(("Setup flowrings: malloc %u flowrings_table[%u] failure\n",
			malloc_size, pciedev->max_flowrings));
		goto fail_cleanup;
	}

#ifdef MEM_ALLOC_STATS
	hnd_get_heapuse(&mu);
	mu.max_flowring_alloc += sizeof(msgbuf_ring_t *);
	hnd_update_mem_alloc_stats(&mu);
#endif /* MEM_ALLOC_STATS */

	/* Pre-initialize each flowring msgbuf_ring */
	for (i = 0; i < pciedev->max_flowrings; i++) {
		msgbuf_ring_t *flowring = &pciedev->flowrings_table[i];
		/*
		 * Following msgbuf_ring_t::fields  will be setup post LINK phase
		 * cbuf_pool : based on host defined txpost work item item_size
		 * dmaindex_h2d_supported: based on successful dmaindex link PCIE IPC
		 * dmaindex_d2h_supported: based on successful dmaindex link PCIE IPC
		 */
		flowring->inited              = FALSE;
		snprintf((char *)flowring->name, MSGBUF_RING_NAME_LEN, "txfl%d", i);
		flowring->pciedev             = pciedev;
		flowring->current_phase       = MSGBUF_RING_INIT_PHASE;
		flowring->flow_info.flags     = FALSE;
		flowring->flow_info.reqpktcnt = 0;
		flowring->fetch_pending       = 0;
#ifdef H2D_CHECK_SEQNUM
		flowring->h2d_seqnum          = H2D_EPOCH_INIT_VAL;
#endif /* H2D_CHECK_SEQNUM */
		flowring->d2h_q_txs_pending   = 0;
		flowring->cfp_flowid		= SCB_FLOWID_INVALID;
	}

	/* Allocate pool of fetch request objects */
	malloc_size = PCIEDEV_MAX_FLOWRINGS_FETCH_REQUESTS * sizeof(flow_fetch_rqst_t);
	if (!(pciedev->flowring_fetch_rqsts = (flow_fetch_rqst_t*)
	         MALLOCZ(pciedev->osh, malloc_size)))
	{
		PCI_ERROR(("Setup flowrings: malloc %d fetch_rqsts[%d] failure\n",
			malloc_size, PCIEDEV_MAX_FLOWRINGS_FETCH_REQUESTS));
		goto fail_cleanup;
	}
	/* Pri-initialize all fetch requests */
	for (i = 0; i < PCIEDEV_MAX_FLOWRINGS_FETCH_REQUESTS; i++) {
		flow_fetch_rqst_t *fetch_rqst = &pciedev->flowring_fetch_rqsts[i];
		fetch_rqst->used = FALSE;
		fetch_rqst->index = i;
		fetch_rqst->rqst.cb = pciedev_flowring_fetch_cb;
		fetch_rqst->rqst.ctx = (void*)fetch_rqst;
		fetch_rqst->offset = 0;
		fetch_rqst->flags = 0;
		fetch_rqst->rqst.next = NULL;
		fetch_rqst->next = NULL;
	}

	/* Allocate fetch req pend list */
	malloc_size = sizeof(fetch_req_list_t);
	if (!(pciedev->fetch_req_pend_list = (fetch_req_list_t*)
	        MALLOCZ(pciedev->osh, malloc_size)))
	{
		PCI_ERROR(("Setup flowrings: malloc fetch_req_pend_list failure\n"));
		goto fail_cleanup;
	}
	/* Setup fetch req pend list */
	pciedev->fetch_req_pend_list->head = NULL;
	pciedev->fetch_req_pend_list->tail = NULL;

	pciedev->schedule_prio = PCIEDEV_SCHEDULER_AC_PRIO;

	return BCME_OK;

fail_cleanup:

	DBG_BUS_INC(pciedev, setup_flowring_failed);
	PCIEDEV_MALLOC_ERR_INCR(pciedev);

	pciedev_cleanup_flowrings(pciedev);

	return BCME_NOMEM;

} /* pciedev_setup_flowrings */

/** Cleanup resources allocated by pciedev_setup_flowrings */
static void
BCMATTACHFN(pciedev_cleanup_flowrings)(struct dngl_bus *pciedev)
{
	int i;

	/* Free the local circular buffer pool shared by all flowrings */
	if (pciedev->flowrings_cir_buf_pool) {
		pciedev_cleanup_flowrings_cpool(pciedev, pciedev->flowrings_cir_buf_pool);
		pciedev->flowrings_cir_buf_pool = NULL;
	}

	/* Free the fetch req pend list */
	if (pciedev->fetch_req_pend_list) {
		MFREE(pciedev->osh, pciedev->fetch_req_pend_list,
			sizeof(fetch_req_list_t));
		pciedev->fetch_req_pend_list = NULL;
	}

	/* Free the pool of fetch request objects */
	if (pciedev->flowring_fetch_rqsts) {
		MFREE(pciedev->osh, pciedev->flowring_fetch_rqsts,
			PCIEDEV_MAX_FLOWRINGS_FETCH_REQUESTS * sizeof(flow_fetch_rqst_t));
		pciedev->flowring_fetch_rqsts = NULL;
	}

	/* Free the table of msgbuf_ring for flowrings */
	if (pciedev->flowrings_table) {
		for (i = 0; i < pciedev->max_flowrings; i++) {
			pciedev_free_msgbuf_flowring(pciedev, &pciedev->flowrings_table[i]);
		}

		MFREE(pciedev->osh, pciedev->flowrings_table,
			pciedev->max_flowrings * sizeof(msgbuf_ring_t));
		pciedev->flowrings_table = NULL;
	}

	/* Free the flowring indirection table */
	if (pciedev->h2d_submitring_ptr) {
		MFREE(pciedev->osh, pciedev->h2d_submitring_ptr,
			(pciedev->max_h2d_rings * sizeof(msgbuf_ring_t *)));
		pciedev->h2d_submitring_ptr = NULL;
	}

} /* pciedev_cleanup_flowrings */

/**
 * XXX Remove the h2d_submitring_ptr indirection table, and directly use the
 * flowid to allocate from the flowrings_table.
 */
static msgbuf_ring_t *
pciedev_allocate_flowring(struct dngl_bus *pciedev)
{
	uint32 i;

	for (i = 0; i < pciedev->max_flowrings; i++) {
		if (pciedev->flowrings_table[i].inited == FALSE)
			return &pciedev->flowrings_table[i];
	}
	return NULL;
} /* pciedev_allocate_flowring */

/**
 * In the link phase, the size of an Txpost item will be known.
 * pciedev_flowrings_link_pcie_ipc() will invoke pciedev_setup_flowrings_cpool()
 * to setup the shared cir_buf_pool and point all flowring's to it.
 */
static cir_buf_pool_t *
pciedev_setup_flowrings_cpool(struct dngl_bus *pciedev,
	uint16 item_cnt, uint8 items_size)
{
	cir_buf_pool_t *cir_buf_pool;

	/* allocate circular buffer pool meta data */
	cir_buf_pool = MALLOCZ(pciedev->osh, sizeof(cir_buf_pool_t));
	if (!cir_buf_pool) {
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return NULL;
	}

	/* allocate actual circular buffer */
	cir_buf_pool->buf = MALLOCZ(pciedev->osh, item_cnt * items_size);
	if (!cir_buf_pool->buf) {
		MFREE(pciedev->osh, cir_buf_pool, sizeof(cir_buf_pool_t));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return NULL;
	}

	/* Initialize circular buffer */
	cir_buf_pool->depth = item_cnt;
	cir_buf_pool->item_size = items_size;
	cir_buf_pool->availcnt = item_cnt;

	/* begin with read & write ptr indexed to 0 */
	cir_buf_pool->r_ptr = cir_buf_pool->w_ptr = 0;

	return cir_buf_pool;

} /* pciedev_setup_flowrings_cpool */

/** Freeup the local circular buffer pool */
static void
pciedev_cleanup_flowrings_cpool(struct dngl_bus *pciedev,
	cir_buf_pool_t *cir_buf_pool)
{
	if (cir_buf_pool) {
		if (cir_buf_pool->buf) {
			MFREE(pciedev->osh, cir_buf_pool->buf,
				(cir_buf_pool->depth  * cir_buf_pool->item_size));
			cir_buf_pool->buf = NULL;
		}
		MFREE(pciedev->osh, cir_buf_pool, sizeof(cir_buf_pool_t));
	}
} /* pciedev_cleanup_flowrings_cpool */

#define PCIEDEV_MSGRING_INDEX_UNUSED	0xFF

static uint32 health_check_time = 1;

void
pciedev_health_check(uint32 ms)
{
	struct dngl_bus *p = dongle_bus;
	if (p->health_check_timer_on == FALSE) {
		return;
	}
	if ((ms - p->last_health_check_time) >= p->health_check_period) {
		p->last_health_check_time = ms;
		health_check_time++;
		PCI_TRACE(("CTRL SUBMIT: %d, %d\n", MSGBUF_WR(p->htod_ctrl),
			MSGBUF_RD(p->htod_ctrl)));
		PCI_TRACE(("CTRL CPL: %d, %d\n", MSGBUF_WR(p->dtoh_ctrlcpl),
			MSGBUF_RD(p->dtoh_ctrlcpl)));
		PCI_TRACE(("%d, rxcpl_pend_cnt %d, rxcpl list count %d, flush call count %d\n",
		health_check_time, p->rxcpl_pend_cnt,
		g_rxcplid_list->avail, pciedev_reorder_flush_cnt));
#ifdef TEST_DROP_PCIEDEV_RX_FRAMES
		PCI_PRINT(("TEST IN PROGRESS: (drop) rxframes %d, no rxcpl frames %d,"
				" rxcpl avail %d\n", pciedev_test_dropped_rxframes,
				pciedev_test_dropped_norxcpls, g_rxcplid_list->avail));
#endif // endif
		if (p->hc_params) {
			health_check_execute(p->hc_params->hc, NULL, 0);
		}
	}
	if ((ms - p->last_axi_check_time) >= p->axi_check_period) {
		p->last_axi_check_time = ms;
		si_clear_backplane_to(p->sih);
	}
}

/** starts fetching messages from host memory into device memory. Function is a timer callback. */
static void
pciedev_sched_msgbuf_intr_process(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)hnd_timer_get_ctx(t);
	pciedev->delayed_msgbuf_intr_timer_on = FALSE;
	pciedev_msgbuf_intr_process(pciedev);
}

/** Fix for D3 Ack problem */
static void
pciedev_bm_enab_timerfn(dngl_timer_t *t)
{
	pciedev_t *pciedev = (pciedev_t *) hnd_timer_get_ctx(t);

	if (pciedev_handle_d0_enter_bm(pciedev) != BCME_OK) {
		dngl_add_timer(pciedev->bm_enab_timer,
			pciedev->bm_enab_check_interval, FALSE);
	} else {
		pciedev->bm_enab_timer_on = FALSE;
	}
}
static void
pciedev_host_sleep_ack_timerfn(dngl_timer_t *t)
{
	pciedev_t *pciedev = (pciedev_t *)hnd_timer_get_ctx(t);
	pciedev->host_sleep_ack_timer_on = FALSE;
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	if (pciedev->ds_inband_dw_supported)	{
		pciedev_host_sleep_ack_check(pciedev);
	}
	else
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
	{
		pciedev_D3_ack(pciedev);
	}
}
/** pciedev device reset, called on eg detach and PERST# detection by the PCIe core */
static void
pciedev_reset(struct dngl_bus *pciedev)
{
	/* Reset core */
	si_core_reset(pciedev->sih, 0, 0);
	pciedev->intstatus = 0;
}

/**
 * called when a new control message from the host (in parameter 'p') is available in device memory
 */
static uint16
pciedev_handle_h2d_msg_ctrlsubmit(struct dngl_bus *pciedev, void *p, uint16 pktlen,
	msgbuf_ring_t *ring)
{
	cmn_msg_hdr_t * msg;
	uint8 msgtype;
	uint8 msglen = MSGBUF_LEN(pciedev->htod_ctrl);
	bool evnt_ctrl = 0;
	uint32	work_item = 0;
#ifdef H2D_CHECK_SEQNUM
	uint8 ring_seqnum = 0;
	uint8 msg_seqnum = 0;
#endif /* H2D_CHECK_SEQNUM */

	if (pktlen % msglen) {
			PCI_TRACE(("htod_ctrl ring HtoD%c  BAD: pktlen %d"
				"doesn't reflect item multiple %d\n",
				pciedev->htod_ctrl->name[3], pktlen, msglen));
		DBG_BUS_INC(pciedev, h2d_ctrl_submit_failed);
		return BCME_ERROR;
	}
	/* reset the fetch limit for control work item fetch */
	pciedev->htod_ctrl->work_item_fetch_limit = PCIEDEV_WORK_ITEM_FETCH_LIMIT_CTRL;
	/* Check if msgtype is IOCT PYLD. This needs to be processed
	 *  irrespective of the state of ioctl lock
	 */
	if (pciedev->ioctl_lock == TRUE) {
		msg = (cmn_msg_hdr_t *)p;
		msgtype = msg->msg_type;
		if (msgtype == MSG_TYPE_IOCTLPTR_REQ) {
			PCI_ERROR(("something wrong Ioctl Lock on...still gettng "
				"ioctl request to process %d!\n", msgtype));
			DBG_BUS_INC(pciedev, h2d_ctrl_submit_failed);
			return BCME_ERROR;
		}
	}
#if  defined(PCIE_DMA_INDEX) && defined(SBTOPCIE_INDICES)
	/* On every control post interrupt sync up read
	 * indices from host
	 *
	 * XXX Optimization possible: sync up read pointer
	 * on a group instead of every message
	 * While submitting to d2h rings, sync up their read pointers from host
	 */
	if (pciedev->dmaindex_d2h_rd.inited) {
		pciedev_sync_d2h_read_ptrs(pciedev, pciedev->dtoh_ctrlcpl);
	}
#endif /* PCIE_DMA_INDEX */

	while (pktlen) {

#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
		bcmpcie_ib_deepsleep_state_t previous_ds_state = pciedev->ds_state;
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
		/* extract out message type and length */
		msg = (cmn_msg_hdr_t *)p;
		msgtype = msg->msg_type;

#ifdef H2D_CHECK_SEQNUM
		/* Check for sequence number sanity */
		ring_seqnum = ring->h2d_seqnum % H2D_EPOCH_MODULO;
		msg_seqnum = msg->epoch;

		if (msg_seqnum == ring_seqnum) {
			ring->h2d_seqnum++;
		} else {
			DBG_BUS_INC(pciedev, h2d_ctrl_submit_failed);
			PCI_ERROR(("CONTROLPOST :error in seqnum : got %d exp %d \n",
				msg_seqnum, ring_seqnum));
			ASSERT(0);
		}
#endif /* H2D_CHECK_SEQNUM */

		switch (msgtype) {
			case MSG_TYPE_IOCTLPTR_REQ:
				if (pciedev->ioctl_lock == TRUE) {
					PCI_TRACE(("Ioctl Lock on...got ioctl req %d!\n", msgtype));
					ASSERT(0);
					DBG_BUS_INC(pciedev, h2d_ctrl_submit_failed);
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
						msg->request_id, BCMPCIE_MAX_IOCTLRESP_BUF,
						BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);
				}
				break;

			case MSG_TYPE_EVENT_BUF_POST:
				PCI_TRACE(("MSG_TYPE_EVENT_BUF_POST\n"));
				if (!pciedev_rxbufpost_msg(pciedev, p, pciedev->event_pool)) {
					evnt_ctrl = PCIE_ERROR2;
					pciedev_send_ring_status(pciedev, 0,
						msg->request_id, BCMPCIE_MAX_EVENT_BUF,
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

			case MSG_TYPE_D2H_RING_CONFIG:
				PCI_TRACE(("MSG_TYPE_D2H_RING_CONFIG\n"));
				pciedev_process_d2h_ring_config(pciedev, p);
				break;

			case MSG_TYPE_H2D_MAILBOX_DATA:
			{
				h2d_mailbox_data_t *h2d_mb_data;
				PCI_TRACE(("MSG_TYPE_H2D_MAILBOX_DATA\n"));
				h2d_mb_data = (h2d_mailbox_data_t *)p;
				pciedev_h2d_mb_data_process(pciedev, h2d_mb_data->mail_box_data);
			}
				break;

			case MSG_TYPE_HOSTTIMSTAMP:
				if (!pciedev->timesync) {
					evnt_ctrl = PCIE_ERROR1;
					pciedev_send_ring_status(pciedev, 0,
						msg->request_id, BCMPCIE_BADOPTION,
						BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);
				} else {
					if (IS_TIMESYNC_LOCK(pciedev)) {
						PCI_ERROR(("TS Lock on...got ioctl req %d!\n",
							msgtype));
						ASSERT(0);
						DBG_BUS_INC(pciedev, h2d_ctrl_submit_failed);
						break;
					}
					PCI_TRACE(("MSG_TYPE_HOSTTIMSTAMP\n"));
					pciedev_process_ts_rqst(pciedev, p);
				}
				break;

			case MSG_TYPE_TIMSTAMP_BUFPOST:
				if (!pciedev->timesync) {
					evnt_ctrl = PCIE_ERROR1;
					pciedev_send_ring_status(pciedev, 0,
						msg->request_id, BCMPCIE_BADOPTION,
						BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);
				} else {
					PCI_TRACE(("MSG_TYPE_TIMSTAMP_BUFPOST\n"));
					if (!pciedev_rxbufpost_msg(pciedev, p, pciedev->ts_pool)) {
						evnt_ctrl = PCIE_ERROR1;
						pciedev_send_ring_status(pciedev, 0,
							msg->request_id, BCMPCIE_MAX_TS_EVENT_BUF,
							BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);

						PCI_ERROR(("Posted more than max Timestamp "
							"Buffer %d!\n", msgtype));
						HND_DIE();
					}
				}
				break;

			default:
				pciedev_send_ring_status(pciedev, 0,
					msg->request_id, BCMPCIE_BADOPTION,
					BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT);
				PCI_ERROR(("Unknown MSGTYPE ctrl submit ring: %d %d %d\n",
					msgtype, msglen, pktlen));
				DBG_BUS_INC(pciedev, h2d_ctrl_submit_failed);
				bcm_print_bytes("unknown message", p, 8);
				break;
		}

#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported &&
		    (previous_ds_state == DS_STATE_DEVICE_SLEEP &&
		     pciedev->ds_state == DS_STATE_DEVICE_SLEEP)) {
			TRAP_IN_PCIEDRV(("Processed message while in DEVICE_SLEEP"));
		}
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */

		if (evnt_ctrl) {
				PCI_ERROR(("max limit of event %d\n", msgtype));
				DBG_BUS_INC(pciedev, h2d_ctrl_submit_failed);
		}

		pktlen = pktlen - msglen;
		p = (uint8 *)p + msglen;
		work_item++;
	}

	/* Increment corresponding IPC Stat */
	pciedev->metrics->num_submissions += work_item;
	PCI_INFORM(("new ctrlsubmit:%d, num_submissions:%d\n",
		pktlen / msglen, pciedev->metrics->num_submissions));

	return pktlen;
} /* pciedev_handle_h2d_msg_ctrlsubmit */

/** send ioctl response to the host */
void
pciedev_bus_sendctl(struct dngl_bus *pciedev, void *p)
{
#ifdef UART_TRANSPORT
	if (pciedev->uarttrans_enab && PKTALTINTF(p)) {
		int ret;
		ret = h5_send_msgbuf(PKTDATA(pciedev->osh, p), PKTLEN(pciedev->osh, p),
		               MSG_TYPE_IOCTL_CMPLT, 0);
		if (ret != BCME_OK)
			PCI_ERROR(("cmd resp h5_send_msgbuf failed w/status %d\n", ret));
		PKTFREE(pciedev->osh, p, TRUE);
		return;
	}
#endif /* UART_TRANSPORT */

	/* only thing goes this way are the IOCTL completions */
	if (IS_IOCTLRESP_PENDING(pciedev)) {
		PCI_PRINT(("something is seriously wrong, ioctl ptr is %p\n",
			pciedev->ioctl_cmplt_p));
	}
	pciedev->ioctl_cmplt_p = p;
	SET_IOCTLRESP_PENDING(pciedev);
	pciedev_send_ioctl_completion(pciedev);
}

/** called when IOCTL completion has to be sent to the host */
static int
pciedev_tx_ioctl_pyld(struct dngl_bus *pciedev, void *p, ret_buf_t *ret_buf,
	uint16 data_len, uint8 msgtype)
{
	dma_queue_t *dmaq = pciedev->dtoh_dma_q[pciedev->default_dma_ch];
	hnddma_t *di = pciedev->d2h_di[pciedev->default_dma_ch];
	pcie_m2m_req_t *m2m_req = pciedev->m2m_req;
	dma64addr_t addr = { .lo = 0, .hi = 0 };
	uint16 msglen;
	void *buf;

	/* Initialize the buf ptr with the ioctl payload addr. This could either be */
	/* from the externally allocated ioct_resp_buf or from the packet itself */
	buf = pciedev->ioct_resp_buf ? pciedev->ioct_resp_buf : PKTDATA(pciedev->osh, p);

	if ((data_len > 0) && (data_len < PCIE_MEM2MEM_DMA_MIN_LEN)) {
		data_len = PCIE_MEM2MEM_DMA_MIN_LEN;
	}

	msglen = data_len;
	ret_buf->high_addr = ret_buf->high_addr & ~PCIE_ADDR_OFFSET;

	PCIE_M2M_REQ_INIT(m2m_req);

	/* Program RX descriptor */

	/* rx buffer for data excluding the phase bits */
	PHYSADDR64HISET(addr, ret_buf->high_addr);
	PHYSADDR64LOSET(addr, ret_buf->low_addr);
	/* r buffer for data excluding the phase bits */
	PCIE_M2M_REQ_RX_SETUP(m2m_req, addr, msglen);

	/* tx for data excluding the phase bits */
	PHYSADDR64HISET(addr, (uint32) 0);
	PHYSADDR64LOSET(addr, (uint32) buf);
	PCIE_M2M_REQ_TX_SETUP(m2m_req, addr, msglen);

	if (PCIE_M2M_REQ_SUBMIT(di, m2m_req)) {
		DBG_BUS_INC(pciedev, txpyld_failed);
		TRAP_DUE_DMA_RESOURCES(("pciedev_tx_ioctl_pyld: "
			"m2m dma failed, rx descs avail = %d tx desc avail = %d\n",
			PCIEDEV_GET_AVAIL_DESC(pciedev, pciedev->default_dma_ch, DTOH, RXDESC),
			PCIEDEV_GET_AVAIL_DESC(pciedev, pciedev->default_dma_ch, DTOH, TXDESC)));
		goto exit;
	}

	pciedev_enqueue_dma_info(dmaq, msglen, msgtype, 0, NULL, 0, 0);

	return TRUE;

exit:
	return FALSE;
} /* pciedev_tx_ioctl_pyld */

static void
pciedev_usrltrtest_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) hnd_timer_get_ctx(t);
	W_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state, (uint32)pciedev->usr_ltr_test_state);
}

#ifdef DUMP_PCIEREGS_ON_TRAP

typedef struct coe_regs {
	/* to indicate if this is a Core reg or internal  reg */
	uint8  reg_type;
	/* register size */
	uint8  reg_size;
	/* starting register address */
	uint32 start_addr;
	/* bit mask indicating regs which are valid from start_addr with a step of reg_size */
	uint32 mask;
} coe_regs_list_t;

#define PCIECORE_COE_REG_TYPE		1
#define PCIECORE_ENM_REG_TYPE		2
#define PCIECORE_GPIO_REG_TYPE		3
#define PCICORE_AXI_DBG_TYPE		4

/**
 * XXX:
 * this is not declared as static const, although that is the right thing to do
 * reason being if declared as static const, compile/link process would that in
 * read only section...
 * currently this code/array is used to identify the registers which are dumped
 * during trap processing
 * and usually for the trap buffer, .rodata buffer is reused,  so for now just static
*/
static coe_regs_list_t g_coe_dump_list1[] = {
	{PCIECORE_COE_REG_TYPE, 4, 0x0004, 0x00000001},
	{PCIECORE_COE_REG_TYPE, 4, 0x0010, 0x0000000F},
	{PCIECORE_COE_REG_TYPE, 4, 0x0070, 0x00000007},
	{PCIECORE_COE_REG_TYPE, 4, 0x0080, 0x00000003},
	{PCIECORE_COE_REG_TYPE, 4, 0x00b0, 0x00000A0E},
	{PCIECORE_COE_REG_TYPE, 4, 0x0100, 0x00000FFF},
	{PCIECORE_COE_REG_TYPE, 4, 0x0800, 0xF00F007F},
	{PCIECORE_COE_REG_TYPE, 4, 0x0900, 0xFFFFFFFF},
	{PCIECORE_COE_REG_TYPE, 4, 0x0980, 0x0000007F},
	{PCIECORE_COE_REG_TYPE, 4, 0x0a10, 0x00000007},
	{PCIECORE_COE_REG_TYPE, 4, 0x1000, 0x00073C3F},
	{PCIECORE_COE_REG_TYPE, 4, 0x1400, 0x0000033F},
	{PCIECORE_COE_REG_TYPE, 4, 0x1800, 0x00081F7F},
	{PCIECORE_COE_REG_TYPE, 4, 0x18e0, 0x00000001},
	{PCIECORE_COE_REG_TYPE, 4, 0x1c00, 0x00000311},
	{PCIECORE_COE_REG_TYPE, 4, 0x1cd8, 0x003FFFFF},
	{PCIECORE_COE_REG_TYPE, 4, 0x1e10, 0x0000000F},
	{PCIECORE_ENM_REG_TYPE, 4, 0x0040, 0x0000000B},
	{PCIECORE_ENM_REG_TYPE, 4, 0x0200, 0x0000003F},
	{PCIECORE_ENM_REG_TYPE, 4, 0x0220, 0x0000003F},
	{PCIECORE_ENM_REG_TYPE, 4, 0x0240, 0x0000003F},
	{PCIECORE_ENM_REG_TYPE, 4, 0x0260, 0x0000003F}
};

static coe_regs_list_t *
BCMRAMFN(pciedev_get_coe_dump_list1)(uint *list_size)
{
	*list_size = ARRAYSIZE(g_coe_dump_list1);
	return g_coe_dump_list1;
}

/**
 * This structure is applicable for all pcie
 * revs (rev 14, 17, 21) except rev 19. If a new
 * pcie rev is introduced whose GPIO reg structure is
 * different from this or pcie rev 19, please add it
 * appropriately.
 */
static coe_regs_list_t g_gpio_dump_list_version_1[] = {
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x021200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x041200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x061200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x021300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x041300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x061300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001500, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001600, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001700, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x201000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x201100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x201200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x201300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x201400, 0x00000001}
};

static coe_regs_list_t *
BCMRAMFN(pciedev_get_gpio_dump_list_version_1)(uint *list_size)
{
	*list_size = ARRAYSIZE(g_gpio_dump_list_version_1);
	return g_gpio_dump_list_version_1;
}

/** This structure is applicable only for pcie rev 19 */
static coe_regs_list_t g_gpio_dump_list_pcierev19[] = {
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x001300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x401000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x4016000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x401100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x421100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x441100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x461100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x401200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x401300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x401400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x421400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x441400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x461400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x401500, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x801000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x801100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x801200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x801300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x801400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0x801500, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xA01000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xA01100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xA01200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xA01300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xA01400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xA01500, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xC01000, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xC01100, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xC01200, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xC01300, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xC01400, 0x00000001},
	{PCIECORE_GPIO_REG_TYPE, 4, 0xC01500, 0x00000001},
	/* Mask must always be 0x3 for AXI_DBG_TYPE */
	{PCICORE_AXI_DBG_TYPE, 4, 0x80000000, 0x00000003},
	{PCICORE_AXI_DBG_TYPE, 4, 0x80010000, 0x00000003},
	{PCICORE_AXI_DBG_TYPE, 4, 0x80020000, 0x00000003},
	{PCICORE_AXI_DBG_TYPE, 4, 0x01000000, 0x00000003},
	{PCICORE_AXI_DBG_TYPE, 4, 0x01010000, 0x00000003},
	{PCICORE_AXI_DBG_TYPE, 4, 0x01020000, 0x00000003}
};

static coe_regs_list_t *
BCMRAMFN(pciedev_get_gpio_dump_list_pcierev19)(uint *list_size)
{
	*list_size = ARRAYSIZE(g_gpio_dump_list_pcierev19);
	return g_gpio_dump_list_pcierev19;
}

static uint32
read_axi_dbg_reg(struct dngl_bus *pciedev, uint32 offset, uint32 j)
{
	uint32 reg_val;

	if (j == 0) {
		/* update the axi_dbg_ctl only one time */
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.axi_dbg_ctl, offset);
		reg_val = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.axi_dbg_data0);
	} else {
		reg_val = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.axi_dbg_data1);
	}

	return reg_val;
}

static uint32
read_gpio_reg(struct dngl_bus *pciedev, uint32 offset)
{
	uint32 reg_val;

	W_REG(pciedev->osh, PCIE_gpioselect(pciedev->autoregs), offset);
	reg_val = R_REG(pciedev->osh, PCIE_GpioOut(pciedev->autoregs));

	return reg_val;
}

static uint32
read_coe_reg(struct dngl_bus *pciedev, uint32 offset)
{
	return (R_REG_CFG(pciedev, offset));
}

static uint32
BCMATTACHFN(pciedev_dump_coe_list_array_size)(struct dngl_bus *pciedev)
{
	coe_regs_list_t *ptr;
	uint32 size, i = 0, j;
	uint coe_dump_list_size;
	coe_regs_list_t *coe_dump_list1 = pciedev_get_coe_dump_list1(&coe_dump_list_size);

	BCM_REFERENCE(pciedev);

	size = sizeof(g_coe_dump_list1);
	for (i = 0; i < coe_dump_list_size; i++) {
		ptr = &coe_dump_list1[i];
		for (j = 0; j < NBITS(uint32); j++) {
			if ((ptr->mask) & (1 << j))
				size += ptr->reg_size;
		}
	}
	/* This is applicable only for pcie rev 19 and 23 */
	if (PCIECOREREV(pciedev->corerev) >= 23) {
		uint list_size;
		coe_regs_list_t *gpio_dump_list_pcierev19 =
		        pciedev_get_gpio_dump_list_pcierev19(&list_size);

		size += sizeof(g_gpio_dump_list_pcierev19);
		for (i = 0; i < list_size; i++) {
			ptr = &gpio_dump_list_pcierev19[i];
			for (j = 0; j < NBITS(uint32); j++) {
				if ((ptr->mask) & (1 << j))
					size += ptr->reg_size;
			}
		}

	} else {
		uint list_size;
		coe_regs_list_t *gpio_dump_list_version_1 =
		        pciedev_get_gpio_dump_list_version_1(&list_size);

		/* This is applicable only for all pcie revs other than 19 */
		size += sizeof(g_gpio_dump_list_version_1);
		for (i = 0; i < list_size; i++) {
			ptr = &gpio_dump_list_version_1[i];
			for (j = 0; j < NBITS(uint32); j++) {
				if ((ptr->mask) & (1 << j))
					size += ptr->reg_size;
			}
		}
	}
	return size;
}

static uint32
pciedev_dump_coe_list_array(struct dngl_bus *pciedev, uchar *p)
{
	uint32 i = 0, j = 0;
	uchar *start = p;
	coe_regs_list_t *ptr;
	uint32 reg_val = 0, val = 0;
	volatile uint32 *x;
	coe_regs_list_t *gpio_ptr;
	uint32 arr_size = 0;

	uint coe_dump_list_size;
	coe_regs_list_t *coe_dump_list1 = pciedev_get_coe_dump_list1(&coe_dump_list_size);

	for (i = 0; i < coe_dump_list_size; i++) {
		ptr = &coe_dump_list1[i];
		bcopy(ptr, p, sizeof(*ptr));
		p += sizeof(*ptr);
		reg_val = ptr->start_addr;
		if (reg_val) {
			for (j = 0; j < NBITS(uint32); j++) {
				if (ptr->mask & (1 << j)) {
					switch (ptr->reg_type) {
					case PCIECORE_ENM_REG_TYPE:
						x = (volatile uint32 *)
							(((volatile uchar *)pciedev->regs) +
							reg_val);
						val = R_REG(pciedev->osh, x);
						break;
					case PCIECORE_COE_REG_TYPE:
						val = read_coe_reg(pciedev, reg_val);
						break;
					default:
						val = 0xFFFFFFFF;
					}
					bcopy(&val, p, ptr->reg_size);
					p += ptr->reg_size;
				}
				reg_val +=  ptr->reg_size;
			}
		}
	}

	/* This is applicable only for pcie rev 19 and 23 */
	if (PCIECOREREV(pciedev->corerev) >= 23) {
		uint list_size;
		coe_regs_list_t *gpio_dump_list_pcierev19 =
		        pciedev_get_gpio_dump_list_pcierev19(&list_size);

		arr_size = list_size;
		gpio_ptr = gpio_dump_list_pcierev19;

	} else {
		uint list_size;
		coe_regs_list_t *gpio_dump_list_version_1 =
		        pciedev_get_gpio_dump_list_version_1(&list_size);

		/* This is applicable only for all pcie revs other than 19 */
		arr_size = list_size;
		gpio_ptr = gpio_dump_list_version_1;
	}

	for (i = 0; i < arr_size; i++) {
		ptr = &gpio_ptr[i];
		bcopy(ptr, p, sizeof(*ptr));
		p += sizeof(*ptr);
		reg_val = ptr->start_addr;
		if (reg_val) {
			for (j = 0; j < NBITS(uint32); j++) {
				if (ptr->mask & (1 << j)) {
					switch (ptr->reg_type) {
						case PCIECORE_GPIO_REG_TYPE:
							val = read_gpio_reg(pciedev, reg_val);
							break;
						case PCICORE_AXI_DBG_TYPE:
							val = read_axi_dbg_reg(pciedev, reg_val, j);
							break;
						default:
							val = 0xFFFFFFFF;
					}
					bcopy(&val, p, ptr->reg_size);
					p += ptr->reg_size;
				}
				reg_val +=  ptr->reg_size;
			}
		}
	}
	return (p - start);
}

static void
pciedev_dump_coe_regs_binary(struct dngl_bus *pciedev)
{
	bcm_xtlv_t *p;
	uint32 req_size;
	uint32 allocated_size;
	const bcm_xtlv_opts_t xtlv_opts = BCM_XTLV_OPTION_ALIGN32;

	/* check if there are any registers which need to be dumped */
	req_size = pciedev->coe_dump_buf_size;
	if (req_size == 0)
		return;

	req_size = bcm_xtlv_size_for_data(req_size, xtlv_opts);

	/* try to get a buffer */
	p = (bcm_xtlv_t *)OSL_GET_FATAL_LOGBUF(pciedev->osh, req_size, &allocated_size);
	if ((p != NULL) && (allocated_size >= req_size)) {
		/* add tag and len , each 2 bytes wide D11 core index */
		p->id = 3;
		p->len = allocated_size - BCM_XTLV_HDR_SIZE_EX(xtlv_opts);
		pciedev_dump_coe_list_array(pciedev, p->data);
	}
}
#endif /* DUMP_PCIEREGS_ON_TRAP */

uint32
pciedev_halt_device(struct dngl_bus *pciedev)
{
	hnddma_t		*h2d_di = pciedev->h2d_di[pciedev->default_dma_ch];
	hnddma_t		*d2h_di = pciedev->d2h_di[pciedev->default_dma_ch];

	/* suspend */
	if (h2d_di) {
		if (!dma_txsuspended(h2d_di)) {
			pciedev->h2d_di_dma_suspend_for_trap = TRUE;
			dma_txsuspend(h2d_di);
		}
		dma_rxreset(h2d_di);
	}
	if (d2h_di) {
		if (!dma_txsuspended(d2h_di)) {
			pciedev->d2h_dma_suspend_for_trap = TRUE;
			dma_txsuspend(d2h_di);
		}
		dma_rxreset(d2h_di);
	}
#ifdef DUMP_PCIEREGS_ON_TRAP
	if ((PCIECOREREV(pciedev->corerev) == 16) || (PCIECOREREV(pciedev->corerev) >= 23) ||
		((R_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs)) & PCIE_RST) != 0)) {
		pciedev_dump_coe_regs_binary(pciedev);
	}
#endif /* DUMP_PCIEREGS_ON_TRAP */

	return ((1 << si_coreidx(pciedev->sih)));
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
		hnd_timer_stop(pciedev->ltr_test_timer);
		pciedev->usr_ltr_test_pend = FALSE;
	}
	pciedev->usr_ltr_test_pend = TRUE;
	pciedev->usr_ltr_test_state = state;
	dngl_add_timer(pciedev->ltr_test_timer, timer_val, FALSE);

	return BCME_OK;
}

/** IOVAR requesting power statistics */
static uint32
pciedev_get_pwrstats(struct dngl_bus *pciedev, char *buf)
{
	wl_pwr_pcie_stats_t pcie_tuple;

	pciedev->metrics->active_dur += OSL_SYSUPTIME() - pciedev->metric_ref->active;
	pciedev->metric_ref->active = OSL_SYSUPTIME();
	pciedev->metrics->timestamp = pciedev->metric_ref->active;

	if (pciedev->metric_ref->ltr_active) {
		pciedev->metrics->ltr_active_dur += OSL_SYSUPTIME() -
			pciedev->metric_ref->ltr_active;
		pciedev->metric_ref->ltr_active = OSL_SYSUPTIME();
	} else if (pciedev->metric_ref->ltr_sleep) {
		pciedev->metrics->ltr_sleep_dur += OSL_SYSUPTIME() -
			pciedev->metric_ref->ltr_sleep;
		pciedev->metric_ref->ltr_sleep = OSL_SYSUPTIME();
	}
	/* Get all link state counters and durations */
	if (PCIECOREREV(pciedev->corerev) >= 7) {
		pciedev->metrics->l0_cnt = R_REG_CFG(pciedev, PCI_L0_EVENTCNT);
		pciedev->metrics->l0_usecs = R_REG_CFG(pciedev, PCI_L0_STATETMR);
		pciedev->metrics->l1_cnt = R_REG_CFG(pciedev, PCI_L1_EVENTCNT);
		pciedev->metrics->l1_usecs = R_REG_CFG(pciedev, PCI_L1_STATETMR);
		pciedev->metrics->l1_1_cnt = R_REG_CFG(pciedev, PCI_L1_1_EVENTCNT);
		pciedev->metrics->l1_1_usecs = R_REG_CFG(pciedev, PCI_L1_1_STATETMR);
		pciedev->metrics->l1_2_cnt = R_REG_CFG(pciedev, PCI_L1_2_EVENTCNT);
		pciedev->metrics->l1_2_usecs = R_REG_CFG(pciedev, PCI_L1_2_STATETMR);
		pciedev->metrics->l2_cnt = R_REG_CFG(pciedev, PCI_L2_EVENTCNT);
		pciedev->metrics->l2_usecs = R_REG_CFG(pciedev, PCI_L2_STATETMR);
	}

	if (buf == NULL)
		return 0;

	/* IPC Stats */
	/* Submission and completions counters updated throughout pciedev.c */
	/* Copy it into the tuple form, then that into the return buffer */
	pcie_tuple.type = WL_PWRSTATS_TYPE_PCIE;
	pcie_tuple.len = sizeof(pcie_tuple);
	ASSERT(sizeof(*(pciedev->metrics)) == sizeof(pcie_tuple.pcie));
	memcpy(&pcie_tuple.pcie, pciedev->metrics, sizeof(pcie_tuple.pcie));
	memcpy(buf, &pcie_tuple, sizeof(pcie_tuple));
	return sizeof(pcie_tuple);
} /* pciedev_get_pwrstats */

static int pciedev_dump_err_cntrs(struct dngl_bus * pciedev)
{
	PCI_PRINT(("======= pciedev error counters =======\n"));
	PCI_PRINT(("mem_alloc_err_cnt: %d\n", pciedev->err_cntr->mem_alloc_err_cnt));
	PCI_PRINT(("rxcpl_err_cnt: %d\n", pciedev->err_cntr->rxcpl_err_cnt));
	PCI_PRINT(("dma_err_cnt: %d\n", pciedev->err_cntr->dma_err_cnt));
	PCI_PRINT(("pktfetch_cancel_cnt: %d\n", pciedev->err_cntr->pktfetch_cancel_cnt));
	PCI_PRINT(("linkdown_cnt: %d\n", pciedev->err_cntr->linkdown_cnt));

	return BCME_OK;
}

#if defined(__ARM_ARCH_7R__)
static int *
BCMRAMFN(get_mpu_region_mid_addr)(void)
{
#ifdef MPU_RAM_PROTECT_ENABLED
	extern char _ram_mpu_region_start[];
	extern char _ram_mpu_region_end[];

	/* write to Text segment check if we generate a Trap */
	return (int*)ALIGN_SIZE(((uint32)_ram_mpu_region_start +
	                         (uint32)_ram_mpu_region_end)/2, sizeof(uint32));
#else
	return NULL;
#endif  /* MPU_RAM_PROTECT_ENABLED */
}
#endif /* __ARM_ARCH_7R__ */

#define TRAP_USER_WHILE_1_VAL_102			102
#define TRAP_USER_WRITE_TO_NULL_POINTER_98	98
#define TRAP_USER_WRITE_TO_HOST_MEMORY_97	97
#define TRAP_USER_READ_FROM_HOST_MEMORY_96	96
#define TRAP_USER_WRITE_TO_TEXT_SEGMENT_95	95
#define TRAP_USER_TRAPTEST_99				99
#define TRAP_USER_INTIATE_TRAP_4_100		100
#define TRAP_USER_INTIATE_SYSHALT_104		104
#define TRAP_USER_INTIATE_SYSHALT_105		105
#define TRAP_USER_INTIATE_DS_HC_106			106

static uint32 pcidev_handle_user_disconnect(struct dngl_bus *pciedev, uint val)
{
#if defined(__ARM_ARCH_7R__)
	int *p;
#endif /* __ARM_ARCH_7R__ */

	switch (val) {
		case TRAP_USER_WHILE_1_VAL_102 :
			/* enter infinite while to cause dead man timer */
			while (1);
			break;
#if defined(__ARM_ARCH_7R__)
		case TRAP_USER_WRITE_TO_NULL_POINTER_98 :
			/* wtite to NULL address check if we generate a Trap */
			p = (int*)0;
			*p = 0x55555555;
			break;
		case TRAP_USER_WRITE_TO_HOST_MEMORY_97 :
			/* wtite to host memory check if we generate a Trap */
			p = (int*)(PCIEDEV_HOSTADDR_MAP_BASE & PCIEDEV_HOSTADDR_MAP_WIN_MASK);
			*p = 0x55555555;
			break;
		case TRAP_USER_READ_FROM_HOST_MEMORY_96 :
			/* read from host memory check if we generate a Trap */
			p = (int*)(PCIEDEV_HOSTADDR_MAP_BASE & PCIEDEV_HOSTADDR_MAP_WIN_MASK);
			PCI_PRINT(("host memory value: %x\n", *p));
			break;
		case TRAP_USER_WRITE_TO_TEXT_SEGMENT_95 :
			p = get_mpu_region_mid_addr();
			PCI_PRINT(("WRITING TO:%p\n", p));
			*p = 0x55555555;
			break;
#endif /* __ARM_ARCH_7R__ */

		case TRAP_USER_TRAPTEST_99 :
			traptest();
			break;
#ifdef PCIE_CTO_DEBUG
		case TRAP_USER_INTIATE_TRAP_4_100 :
			pciedev->induce_trap4 = TRUE;
			break;
#endif /* PCIE_CTO_DEBUG */
		case TRAP_USER_INTIATE_SYSHALT_104:
		case TRAP_USER_INTIATE_SYSHALT_105:
			OSL_SYS_HALT();
			break;
		case TRAP_USER_INTIATE_DS_HC_106 :
			if (HND_DS_HC_ENAB() &&
				pciedev->hc_params &&
				pciedev->hc_params->disable_ds_hc == FALSE) {
				pciedev_ds_hc_trigger(pciedev);
			}
			break;
		default:
			return BCME_UNSUPPORTED;
	}
	return BCME_OK;
}

static uint32
pciedev_bus_ltr_upd_reg(struct dngl_bus *pciedev, uint32 offset, uint32 latency, uint32 unit)
{
	uint32 val;

	if ((offset != PCIE_LTR0_REG_OFFSET) &&
	    (offset != PCIE_LTR1_REG_OFFSET) &&
	    (offset != PCIE_LTR2_REG_OFFSET)) {
		return 0;
	}

	/* enforce 10 bit latency value */
	val = latency & PCIE_LTR_LAT_VALUE_MASK;
	/* unit=2 sets latency in units of 1us, unit=4 sets unit of 1ms */
	val |= (unit << PCIE_LTR_LAT_SCALE_SHIFT);
	val |= (1 << PCIE_LTR_SNOOP_REQ_SHIFT);	/* enable latency requirement */
	val |= val << 16;	/* repeat for no snoop */

	/* applied the set value if not in D3 suspend */
	if (!pciedev->in_d3_suspend) {
		/* can access config in D3 with newer chips */
		W_REG_CFG(pciedev, offset, val);
		val = R_REG_CFG(pciedev, offset);
	}
	/* cache the value */
	if (offset == PCIE_LTR0_REG_OFFSET)
		pciedev->ltr_info.ltr0_regval = val;
	else if (offset == PCIE_LTR1_REG_OFFSET)
		pciedev->ltr_info.ltr1_regval = val;
	else
		pciedev->ltr_info.ltr2_regval = val;

	return val;
}

/* Memory optimization: option to disable it ~7K */
#ifdef BUS_IOVAR_DISABLED
#define PCIEDEV_IOVAR_DISABLED	(1)
#else
#define PCIEDEV_IOVAR_DISABLED	(0)
#endif // endif

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

	if (!strcmp(cmd, "pwrstats")) {
		uint16 type;

		if (set) {
			bzero(pciedev->metrics, sizeof(*pciedev->metrics));
			bzero(pciedev->metric_ref, sizeof(*pciedev->metric_ref));

			PCI_PRINT(("\nd3_suspend\t%u\nd0_resume\t\t%u\n"
				"perst_assrt\t%u\nperst_deassrt\t%u\n"
				"active_dur\t%u\nd3_susp_dur\t%u\nperst_dur\t\t%u\n"
				"num ltr_active: %u\nltr_active_dur: %u\n"
				"num ltr_sleep: %u\nltr_leep_dur: %u\n"
				"num deepsleep: %u\ndeepsleep_dur: %u\n"
				"h2d_drbl\t\t%u,\td2h_drbl\t\t%u\n"
				"num_submissions\t%u,\tnum_completions\t%u\n"
				"timestamp\t%u\n",
				pciedev->metrics->d3_suspend_ct,
				pciedev->metrics->d0_resume_ct,
				pciedev->metrics->perst_assrt_ct,
				pciedev->metrics->perst_deassrt_ct,
				pciedev->metrics->active_dur ?
					pciedev->metrics->active_dur : OSL_SYSUPTIME(),
				pciedev->metrics->d3_suspend_dur, pciedev->metrics->perst_dur,
				pciedev->metrics->ltr_active_ct, pciedev->metrics->ltr_active_dur,
				pciedev->metrics->ltr_sleep_ct, pciedev->metrics->ltr_sleep_dur,
				pciedev->metrics->deepsleep_count, pciedev->metrics->deepsleep_dur,
				pciedev->metrics->num_h2d_doorbell,
				pciedev->metrics->num_d2h_doorbell,
				pciedev->metrics->num_submissions,
				pciedev->metrics->num_completions,
				pciedev->metrics->timestamp));

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
		return (pcidev_handle_user_disconnect(pciedev, val));
#if defined(PCIEDEV_BUS_DBG) || (!defined(PCIEDEV_BUS_DBG) && \
	defined(PCIE_BUS_ENABLE_FETCH_INDEX))
#ifdef PCIE_DMA_INDEX
	} else if (!strcmp(cmd, "dmaindex_h2d_w_d2h")) {
		/* Fetch indices from host after DMA'ing indices to host */
		if (set) {
			if (!(pciedev->dmaindex_h2d_wr.inited) ||
			        !(pciedev->dmaindex_d2h_rd.inited)) {
				return BCME_NOTREADY;
			}
			if (val) {
				pciedev->dmaindex_h2d_w_d2h = TRUE;
			} else {
				pciedev->dmaindex_h2d_w_d2h = FALSE;
			}
		}
		else {
			val = pciedev->dmaindex_h2d_w_d2h ? 1 : 0;
			memcpy(buf, &val, sizeof(uint32));
		}

		return BCME_OK;
#endif /* PCIE_DMA_INDEX */
#endif /* PCIEDEV_BUS_|| (!PCIEDEV_BUS_DBG && DBGPCIE_BUS_ENABLE_FETCH_INDEX) */
	} else if (!strcmp(cmd, "fl_prio_map")) {
		if (set) {
			if (!dll_empty(&pciedev->active_prioring_list))
				return BCME_BUSY;
			pciedev->schedule_prio = val;
		} else {
			val = pciedev->schedule_prio;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
#ifdef SECI_UART
	} else if (!strcmp(cmd, "serial")) {
		if (set) {
			/* expected val is 0 or 1 */
			if (val > 1) {
				return BCME_BADARG;
			}

			si_seci_clk_force(pciedev->sih, val);
			if (val == 1) {
				hnd_cons_uart_muxenab(pciedev->sih,
					UART_DEFAULT_PIN_MAP);
			}
		} else {
			val = si_seci_clk_force_status(pciedev->sih);
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
#endif /* SECI_UART */
#ifdef AXI_TIMEOUTS
	} else if (!strcmp(cmd, "backplane_to")) {
#define D11_REG_OFF	0x120	/* maccontrol */
		volatile uint8 *d11_regs;
		uint32 intr_val, origidx;
		/* hack to read a core without clocks. assumes wl is down */
		PCI_PRINT(("before read wl reg\n"));
		d11_regs = (volatile uint8 *)si_switch_core(pciedev->sih, D11_CORE_ID,
				&origidx, &intr_val);

		/* if wl is down & timeouts are enabled this will trigger a trap */
		(void)R_REG(pciedev->osh, ((volatile uint32 *)(d11_regs + D11_REG_OFF)));

		/* if we make it here wl must have been up */
		PCI_PRINT(("after read wl reg: wl must be up\n"));
		si_restore_core(pciedev->sih, origidx, intr_val);
		return BCME_OK;
#endif /* AXI_TIMEOUTS */
	}

	if (PCIEDEV_IOVAR_DISABLED) {
		return BCME_UNSUPPORTED;
	}

#ifdef PCIE_INDUCE_ERRS
		if (!strcmp(cmd, "ur_err")) {
			pciedev->unsupport_req_err = TRUE;
			return BCME_OK;
		}
#endif // endif

	if (!strcmp(cmd, "maxhostrxbufs")) {
			index = MAXHOSTRXBUFS;
	} else if (!strcmp(cmd, "maxtxstatus")) {
			index = MAXTXSTATUS;
#ifdef PCIE_DMAXFER_LOOPBACK
	} else if (!strcmp(cmd, "txlpbk_pkts")) {
		if (set)
			pciedev->dmaxfer_loopback->txlpbk_pkts = val;
		else
			memcpy(buf, &pciedev->dmaxfer_loopback->txlpbk_pkts, sizeof(uint32));
		return BCME_OK;
#endif /* PCIE_DMAXFER_LOOPBACK */
#ifdef HMAPTEST
	} else if (!strcmp(cmd, "hmap")) {
		return pciedev_process_hmap_msg(pciedev, buf, offset, set, outlen);
	} else if (!strcmp(cmd, "hmaptest")) {
		/* "bus:hmaptest" */
		return pciedev_process_hmaptest_msg(pciedev, buf, offset,  set, outlen);
#endif /* HMAPTEST */
#if defined(BCMPCIE_IPC_HPA)
	} else if (!strcmp(cmd, "hpa")) {
		BCMPCIE_IPC_HPA_DUMP(pciedev); return BCME_OK;
#endif   /* BCMPCIE_IPC_HPA */
#if defined(BCM_BUZZZ)
	/* BUZZZ control for CR4, CA7 */
	} else if (!strcmp(cmd, "buzzz_help")) {
		printf("\tbuzzz_start\n\tbuzzz_stop\n\tbuzzz_show\n\tbuzzz_status\n"
			"\tbuzzz_mode\n\tbuzzz_skip(num_starts)\n\tbuzzz_config(group)\n");
	} else if (!strcmp(cmd, "buzzz_show")) {
		BCM_BUZZZ_SHOW(); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_dump")) {
		BCM_BUZZZ_DUMP(); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_start")) {
		BCM_BUZZZ_START(); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_stop")) {
		BCM_BUZZZ_STOP(); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_wrap")) {
		BCM_BUZZZ_WRAP(); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_status")) {
		printf("STATUS=%d\n", BCM_BUZZZ_STATUS());
		return BCME_OK;
	} else if (!strcmp(cmd, "buzzz")) {
		uint8 mode = BCM_BUZZZ_MODE();
		if (mode == BCM_BUZZZ_MODE_FUNC)
			printf("Mode Function Call Tracing\n");
		else if (mode == BCM_BUZZZ_MODE_EVENT)
			printf("Mode Event Tracing\n");
		else
			printf("Mode Undefined\n");
		return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_skip")) {
		if (set) { val = (val == 0) ? 0xFF : val; } /* 0xFF = 255 starts */
		BCM_BUZZZ_SKIP(val); return BCME_OK;
	} else if (!strcmp(cmd, "buzzz_config")) {
		if (set) { val = (val == 0) ? 0xFF : val; } /* 0xFF = cycles cnt */
			BCM_BUZZZ_CONFIG(val); return BCME_OK;
#endif /* BCM_BUZZZ */
	} else if (!strcmp(cmd, "dropped_txpkts")) {
		if (set)
			pciedev->dropped_txpkts = val;
		else
			memcpy(buf, &pciedev->dropped_txpkts, sizeof(uint32));
		return BCME_OK;
#if defined(WLSQS)
	} else if (!strcmp(cmd, "sqs_vpkts")) {
		val = sqs_vpkts_dump(pciedev, SCB_ALL_FLOWS);
		return BCME_OK;
#endif /* WLSQS */
#if defined(BCMHME)
	} else if (!strcmp(cmd, "hme")) {
		bool verbose;
		verbose = FALSE;
		hme_dump(verbose);
		return BCME_OK;
#endif /* BCMHME */
	} else if (!strcmp(cmd, "pcie_ipc")) {
		val = pciedev_dump_pcie_ipc(pciedev);
		return BCME_OK;
#if defined(BCM_CSIMON)
	} else if (!strcmp(cmd, "csimon_dump")) {
		if (pciedev->csimon)
			return pciedev_csimon_dump();
		return BCME_NOTUP;
#endif /* BCM_CSIMON */
	} else if (!strcmp(cmd, "err_cntrs")) {
		val = pciedev_dump_err_cntrs(pciedev);
		return BCME_OK;
	} else if (!strcmp(cmd, "ltr_state")) {
		if (set) {
			if (val > 2)
				val = 2;
			W_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state, val);
		} else {
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
#ifdef PCIE_DEEP_SLEEP
	} else if (!strcmp(cmd, "ds_enter_req")) {
		if (set) {
#ifdef PCIEDEV_INBAND_DW
			if (pciedev->ds_inband_dw_supported) {
				pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_DEVICE_SLEEP_INFORM);
			}
			else
#endif /* PCIEDEV_INBAND_DW */
			{
				pciedev_deepsleep_enter_req(pciedev);
			}
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
#ifdef PCIEDEV_INBAND_DW
			if (pciedev->ds_inband_dw_supported) {
				pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_MSI_ISSUED);
			}
			else
#endif /* PCIEDEV_INBAND_DW */
			{
				pciedev_deepsleep_exit_notify(pciedev);
			}
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
#endif /* PCIE_DEEP_SLEEP */
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
	} else if (!strcmp(cmd, "dump_lclbuf")) {
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
	} else if (!strcmp(cmd, "sim_host_enter_d3")) {
		pciedev_notify_devpwrstchg(pciedev, (val ? FALSE : TRUE));
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_d2h_wr")) {
#ifdef PCIE_DMA_INDEX
		uchar *ptr = (uchar *)pciedev->dmaindex_d2h_wr.daddr64.lo;
		bcm_print_bytes("ringupd", ptr, pciedev->dmaindex_d2h_wr.size);
#endif /* PCIE_DMA_INDEX */
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_flowrings")) {
		uint16 i = 0;
		val = BCME_OK;
		for (i = 0; i < pciedev->max_flowrings; i++) {
			if (pciedev->flowrings_table[i].inited)
				val = pciedev_dump_flow_ring(&pciedev->flowrings_table[i]);
		}
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "d2h_mb_data")) {
		PCI_PRINT(("send d2h mail box data to host 0x%04x\n", val));
		pciedev_d2h_mbdata_send(pciedev, val);
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_prio_rings")) {
		val = pciedev_dump_prio_rings(pciedev);
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_rw_indices")) {
		val = pciedev_dump_rw_indices(pciedev);
		memcpy(buf, &val, sizeof(uint32));
		return BCME_OK;
	} else if (!strcmp(cmd, "start_check")) {
		if (set) {
			if (pciedev->health_check_timer_on == FALSE) {
				pciedev->health_check_timer_on = TRUE;
				pciedev->health_check_period = val;
			}
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "stop_check")) {
		if (pciedev->health_check_timer_on == TRUE) {
			pciedev->health_check_timer_on = FALSE;
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "metrics")) {
		if ((pciedev->metrics == NULL) || (pciedev->metric_ref == NULL))
			return BCME_BADARG;
		if (set) {
			bzero(pciedev->metrics, sizeof(pcie_bus_metrics_t));
			bzero(pciedev->metric_ref, sizeof(metric_ref_t));
		} else {
			pciedev->metrics->active_dur +=
				OSL_SYSUPTIME() - pciedev->metric_ref->active;
			pciedev->metric_ref->active = OSL_SYSUPTIME();
			val = pciedev->metrics->active_dur;
		}
		PCI_PRINT(("\nd3_suspend\t\t%u\nd0_resume\t\t%u\nperst_assrt\t%u\n"
			"perst_deassrt\t%u\nactive_dur\t%u\nd3_susp_dur\t%u\n"
			"perst_dur\t%u\n",
			pciedev->metrics->d3_suspend_ct, pciedev->metrics->d0_resume_ct,
			pciedev->metrics->perst_assrt_ct, pciedev->metrics->perst_deassrt_ct,
			pciedev->metrics->active_dur ?
			pciedev->metrics->active_dur : OSL_SYSUPTIME(),
			pciedev->metrics->d3_suspend_dur, pciedev->metrics->perst_dur));
		return BCME_OK;
#ifdef BCMPCIE_IDMA
	} else if (!strcmp(cmd, "idmametrics")) {
		if (PCIE_IDMA_ENAB(pciedev)) {
			int idx;
			idma_info_t *idma_info = pciedev->idma_info;
			dmaindex_t *dmaindex;

			if (set) {
				bzero(&idma_info->metrics, sizeof(idma_metrics_t));
			}
			PCI_PRINT(("\nenable\t%u\ninited\t%u\n"
				"desc_loaded\t%u",
				idma_info->enable,
				idma_info->inited,
				idma_info->desc_loaded));

#if defined(BCMDBG) || defined(WLTEST)
			PCI_PRINT(("\nintr cnt:"));
			dmaindex = idma_info->dmaindex;
			for (idx = 0; idx < MAX_IDMA_DESC; idx++) {
				if (dmaindex->inited) {
					PCI_PRINT(("  %d", idma_info->metrics.intr_num[idx]));
				}
				else {
					PCI_PRINT(("  NA"));
				}
				dmaindex++;
			}
#endif // endif

			dmaindex = idma_info->dmaindex;
			for (idx = 0; idx < MAX_IDMA_DESC; idx++) {
				if (!dmaindex->inited)
					continue;
				PCI_PRINT(("\ndmaindex%2u size %3u"
					HADDR64_FMT " daddr64 lo 0x%8x",
					idx, dmaindex->size,
					HADDR64_VAL(dmaindex->haddr64),
					dmaindex->daddr64.lo));
				dmaindex++;
			}
			PCI_PRINT(("\n"));
			return BCME_OK;
		} else {
			PCI_PRINT(("idma is not enabled\n"));
			return BCME_ERROR;
		}
#endif /* BCMPCIE_IDMA */
	} else if (!strcmp(cmd, "pwrstats")) {
		uint16 type;

		if (set)
			return BCME_ERROR;
		if (inlen < offset + sizeof(uint16))
			return BCME_ERROR;
		if (inlen < ROUNDUP(sizeof(wl_pwr_pcie_stats_t), sizeof(uint32)))
			return BCME_BUFTOOSHORT;

		memcpy(&type, &buf[offset], sizeof(uint16));
		if (type != WL_PWRSTATS_TYPE_PCIE)
			return BCME_BADARG;

		*outlen = pciedev_get_pwrstats(pciedev, buf);
		return BCME_OK;
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
#ifdef PCIE_DEEP_SLEEP
	} else if (!strcmp(cmd, "dump_ds_log")) {
		return (pciedev_deepsleep_engine_log_dump(pciedev));
#endif /* PCIE_DEEP_SLEEP */
	} else if (!strcmp(cmd, "dump_dma_index_log") && set) {
#if defined(PCIE_DMA_INDEX) && defined(PCIE_DMA_INDEX_DEBUG)
		if (val == 1 || val == 2)
			return (pciedev_dmaindex_log_dump(pciedev, val));
		else
			return BCME_BADARG;
#else
		return BCME_UNSUPPORTED;
#endif /* PCIE_DMA_INDEX && PCIE_DMA_INDEX_DEBUG */
	} else if (!strcmp(cmd, "no_d3_exit")) {
		if (set)
			pciedev->no_device_inited_d3_exit = val;
		else {
			*buf = pciedev->no_device_inited_d3_exit;
			*outlen = sizeof(bool);
		}
#ifdef UART_TRAP_DBG
	} else if (!strcmp(cmd, "uarttrapenab")) {
		if (set) {
			if (val == 0 || val == 1) {
				pciedev_uart_debug_enable(pciedev, val);
			}
			else
				return BCME_BADARG;
		} else
			val = pciedev_uart_debug_enable(pciedev, -1);
#endif /* UART_TRAP_DBG */
	} else if (!strcmp(cmd, "sr_count")) {
		if (set) {
			pciedev->sr_count = val;
		} else {
			val = pciedev->sr_count;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
#ifdef BCMPCIE_IDMA
	} else if (!strcmp(cmd, "idma_desc_error")) {
		PCI_TRACE(("trigger idma descriptor protocol error\n"));
		if (PCIE_IDMA_ACTIVE(pciedev)) {
			pciedev_dma_params_init_h2d(pciedev, pciedev->h2d_di[DMA_CH2]);
			if (pciedev_idma_desc_load(pciedev) != BCME_OK) {
				PCI_ERROR(("trigger idma_desc_error fail - desc load fail\n"));
				pciedev->idma_info->desc_loaded = FALSE;
			} else {
				pciedev->idma_info->desc_loaded = TRUE;
			}
		} else {
			PCI_TRACE(("trigger idma_desc_error fail - IDMA not active\n"));
		}
		return BCME_OK;
#endif /* BCMPCIE_IDMA */
	} else if (!strcmp(cmd, "dw_count")) {
		if (set) {
			pciedev->dw_counters.dw_toggle_cnt = val;
		}
		else {
			val = pciedev->dw_counters.dw_toggle_cnt;
			memcpy(buf, &val, sizeof(uint32));
		}
	} else if (!strcmp(cmd, "dw_type")) {
		if (!set) {
#ifdef PCIEDEV_INBAND_DW
			if (pciedev->ds_inband_dw_supported) {
				val = 2;
			}
			else
#endif /* PCIEDEV_INBAND_DW */
			if (pciedev->ds_oob_dw_supported) {
				val = 1;
			} else {
				val = 0;
			}
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
#ifdef PCIEDEV_INBAND_DW
	} else if (!strcmp(cmd, "dw_inb_assert")) {
		if (set) {
			pciedev->num_ds_inband_dw_assert = val;
		} else {
			val = pciedev->num_ds_inband_dw_assert;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "dw_inb_deassert")) {
		if (set) {
			pciedev->num_ds_inband_dw_deassert = val;
		} else {
			val = pciedev->num_ds_inband_dw_deassert;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
#endif /* PCIEDEV_INBAND_DW */
	} else if (!strcmp(cmd, "socramind")) {
		if (set) {
			if (val == 0)
				SOCRAM_ASSRT_INDICATE();
			else if (val == 1) {
				if (pciedev->hc_params) {
					pciedev->hc_params->hc_induce_error = TRUE;
				}
			}
			else
				return BCME_UNSUPPORTED;
		}
#if defined(PCIEDEV_BUS_DBG) && defined(BCMRESVFRAGPOOL)
	} else if (!strcmp(cmd, "resv_pool")) {
		if (set) {
			rsvpool_own(pciedev->pktpool_resv_lfrag, val);
		}
		return BCME_OK;
#endif /* PCIEDEV_BUS_DBG && BCMRESVFRAGPOOL */
	} else if (!strcmp(cmd, "dmach_sel")) {
		PCI_PRINT(("dmach_sel, val : %d\n", val));
		if (val == DMA_CH1) {
#ifdef BCMPCIE_DMA_CH1
			pciedev->default_dma_ch = DMA_CH1;
#else
			return BCME_UNSUPPORTED;
#endif // endif
		} else if (val == DMA_CH2) {
#ifdef BCMPCIE_DMA_CH2
			pciedev->default_dma_ch = DMA_CH2;
#else
			return BCME_UNSUPPORTED;
#endif // endif
		} else
			return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "fwtsinfo")) {
		if (set) {
			pciedev_initiate_dummy_fwtsinfo(pciedev, val);
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_tsbuf")) {
		if (!set) {
			pciedev_dump_ts_pool(pciedev);
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "dump_last_host_ts")) {
		if (!set) {
			pciedev_dump_last_host_ts(pciedev);
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "hostready_count")) {
		if (!set) {
			val = pciedev->bus_counters->hostready_cnt;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
#if defined(__ARM_ARCH_7R__) && defined(MPU_RAM_PROTECT_ENABLED)
	} else if (!strcmp(cmd, "dump_mpu")) {
		if (!set) {
			dump_mpu_regions();
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "mpu_read_test")) {
		if (set) {
			uint32 *p = (uint32 *)val;
			PCI_PRINT(("Reading memory 0x%p value: %x\n", p, *p));
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "mpu_write_test")) {
		if (set) {
			uint32 *p = (uint32 *)val;
			PCI_PRINT(("Writing to memory 0x%p value: %x\n", p, 0xdeadbeaf));
			*p = 0xdeadbeaf;
			PCI_PRINT(("Reading memory back 0x%p value: %x\n", p, *p));
		}
		return BCME_OK;
#endif /* defined(__ARM_ARCH_7R__) && defined(MPU_RAM_PROTECT_ENABLED) */
#ifdef TIMESYNC_GPIO
	} else if (!strcmp(cmd, "time_sync_gpio")) {
		int err = BCME_OK;
		err = pciedev_gpio_signal_test(pciedev, pciedev->time_sync_gpio, set, &val);
		if (!set && err == BCME_OK) {
			memcpy(buf, &val, sizeof(uint32));
		}
		return err;
#endif // endif
	} else if (!strcmp(cmd, "device_wake_gpio")) {
		if (!set) {
			int err = BCME_OK;
			err = pciedev_gpio_signal_test(pciedev,
				pciedev->device_wake_gpio, FALSE, &val);
			if (err == BCME_OK) {
				memcpy(buf, &val, sizeof(uint32));
			}
			return err;
		}
		return BCME_UNSUPPORTED;
	} else if (!strcmp(cmd, "pcie_wake_gpio")) {
		int err = BCME_OK;
		err = pciedev_gpio_signal_test(pciedev, pciedev->host_wake_gpio, set, &val);
		if (!set && err == BCME_OK) {
			memcpy(buf, &val, sizeof(uint32));
		}
		return err;
	} else if (!strcmp(cmd, "ltr_active_lat")) {
		if (set) {
			if ((val <= 0) || (val > LTR_MAX_ACTIVE_LATENCY_US)) {
				PCI_ERROR(("Active Latency Out-of-Range; "
						"must be between 1 ~ 600usec\n"));
				return BCME_RANGE;
			}
			val = pciedev_bus_ltr_upd_reg(pciedev, PCIE_LTR0_REG_OFFSET,
				val, LTR_SCALE_US);
			if (val == 0) {
				return BCME_ERROR;
			} else {
				pciedev->ltr_info.active_lat = val & 0x1fff;
			}
		} else {
			val = pciedev->ltr_info.active_lat;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
	} else if (!strcmp(cmd, "ltr_sleep_lat")) {
		if (set) {
			if ((val <= 0) || (val > LTR_MAX_SLEEP_LATENCY_MS)) {
				PCI_ERROR(("Sleep Latency Out-of-Range; "
						"must be between 1 ~ 6msec\n"));
				return BCME_RANGE;
			}
			val = pciedev_bus_ltr_upd_reg(pciedev, PCIE_LTR2_REG_OFFSET,
				val, LTR_SCALE_MS);
			if (val == 0) {
				return BCME_ERROR;
			} else {
				pciedev->ltr_info.sleep_lat = val & 0x1fff;
			}
		} else {
			val = pciedev->ltr_info.sleep_lat;
			memcpy(buf, &val, sizeof(uint32));
		}
		return BCME_OK;
#ifdef BCMPMU_STATS
	} else if (!strcmp(cmd, "trap_data_readback")) {
		if (set) {
			pciedev->trap_data_readback = val ? TRUE : FALSE;
			return BCME_OK;
		}
		return BCME_UNSUPPORTED;
	}
	} else if (PMU_STATS_ENAB()) {
		if (!strcmp(cmd, "pmustatstimer_dump")) {
			si_pmustatstimer_dump(pciedev->sih);
			return BCME_OK;
		} else if (!strcmp(cmd, "pmustatstimer_start")) {
			si_pmustatstimer_start(pciedev->sih, val);
			return BCME_OK;
		} else if (!strcmp(cmd, "pmustatstimer_stop")) {
			si_pmustatstimer_stop(pciedev->sih, val);
			return BCME_OK;
		} else if (!strcmp(cmd, "pmustatstimer_read")) {
			printf("pmustatstimer_read - timer %d : ", val);
			val = si_pmustatstimer_read(pciedev->sih, val);
			printf("%d\n", val);
			memcpy(buf, &val, sizeof(uint32));
			return BCME_OK;
		} else if (!strcmp(cmd, "pmustatstimer_clear")) {
			si_pmustatstimer_clear(pciedev->sih, val);
			return BCME_OK;
		} else if (!strcmp(cmd, "pmustatstimer_cfg_cnt_mode")) {
			uint8 cnt_mode = val & 0xffff;
			uint8 timerid = (val >> 16) & 0xffff;
			si_pmustatstimer_cfg_cnt_mode(pciedev->sih, cnt_mode, timerid);
			return BCME_OK;
		} else if (!strcmp(cmd, "pmustatstimer_cfg_src_num")) {
			uint8 src_num = val & 0xffff;
			uint8 timerid = (val >> 16) & 0xffff;
			si_pmustatstimer_cfg_src_num(pciedev->sih, src_num, timerid);
			return BCME_OK;
		} else {
			return BCME_UNSUPPORTED;
		}
#endif /* BCMPMU_STATS */
	} else if (!strcmp(cmd, "wakeup_data") && !set) {
		uint16 ret_len;
		wl_host_wakeup_data_v2_t *wd;

		ret_len = pciedev->wd_len + OFFSETOF(wl_host_wakeup_data_v2_t, data);
		if (inlen < ret_len)
			return BCME_BUFTOOSHORT;
		wd = (wl_host_wakeup_data_v2_t *)buf;
		wd->ver = HOST_WAKEUP_DATA_VER_2;
		if (pciedev->wd) {
			wd->len = pciedev->wd_len;
			wd->gpio_toggle_time = pciedev->wd_gpio_toggle_time;
			memcpy(wd->data, pciedev->wd, pciedev->wd_len);
			MFREE(pciedev->osh, pciedev->wd, pciedev->wd_len);
			pciedev->wd = NULL;
			pciedev->wd_len = 0;
		} else {
			wd->len = 0;
		}
		*outlen = ret_len;
		return BCME_OK;
	} else {
		return BCME_UNSUPPORTED;
	}

	if (set) {
		pciedev->tunables[index] = val;
	} else {
		memcpy(buf, &pciedev->tunables[index], sizeof(uint32));
	}

	return BCME_OK;
} /* pciedev_bus_iovar */

static uint32
pciedev_get_avail_host_rxbufs(struct dngl_bus *pciedev)
{
	return 0;
}

void
pciedev_dma_params_init_h2d(struct dngl_bus *pciedev, hnddma_t *di)
{
	uint32 burstlen;

	/* supports multiple outstanding reads */
	if (PCIECOREREV(pciedev->corerev) == 15 || PCIECOREREV(pciedev->corerev) == 20) {
		dma_param_set(di, HNDDMA_PID_TX_MULTI_OUTSTD_RD, DMA_MR_12);
	} else if (PCIECOREREV(pciedev->corerev) == 7 ||
		PCIECOREREV(pciedev->corerev) == 11 ||
		PCIECOREREV(pciedev->corerev) == 9 ||
		PCIECOREREV(pciedev->corerev) >= 13) {
		dma_param_set(di, HNDDMA_PID_TX_MULTI_OUTSTD_RD, DMA_MR_4);
	} else {
		dma_param_set(di, HNDDMA_PID_TX_MULTI_OUTSTD_RD, DMA_MR_2);
	}

	burstlen = pcie_h2ddma_tx_get_burstlen(pciedev);
#ifdef PCIE_INDUCE_ERRS
		/* Introducing MRRS error due to burstlen exceeding MRRS */
		if (getvar(NULL, tx_burstlen) != NULL) {
			burstlen =	getintvar(NULL, tx_burstlen);
		}
#endif // endif
	dma_param_set(di, HNDDMA_PID_TX_BURSTLEN, burstlen);

	dma_param_set(di, HNDDMA_PID_TX_PREFETCH_CTL, 1);
	dma_param_set(di, HNDDMA_PID_TX_PREFETCH_THRESH, 3);

	dma_param_set(di, HNDDMA_PID_RX_PREFETCH_CTL, 1);
	dma_param_set(di, HNDDMA_PID_RX_PREFETCH_THRESH, 1);

	if (!dma_rxreset(di)) {
		TRAP_DUE_DMA_ERROR(("dma_rxreset failed for h2d_di engine\n"));
	}
	if (!dma_txreset(di)) {
		TRAP_DUE_DMA_ERROR(("dma_txreset failed for h2d_di engine\n"));
	}
	dma_rxinit(di);
	dma_txinit(di);
}

static void
pciedev_dma_params_init_d2h(struct dngl_bus *pciedev, hnddma_t *di)
{
#ifdef PCIE_INDUCE_ERRS
	uint32	devctrl = 0, burstlen;
#endif // endif

	dma_param_set(di, HNDDMA_PID_RX_PREFETCH_CTL, 1);
	dma_param_set(di, HNDDMA_PID_RX_PREFETCH_THRESH, 1);
#ifdef PCIE_INDUCE_ERRS
	/* Introducing MPS error due to d2h rx_burstlen exceeding MPS */
	if (getvar(NULL, rx_burstlen) != NULL) {
		burstlen =	getintvar(NULL, rx_burstlen);
		dma_param_set(di, HNDDMA_PID_RX_BURSTLEN, burstlen);
		/* Just to make sure that default MPS is 128 Bytes */
		AND_REG_CFG(pciedev, PCIECFGREG_DEVCONTROL,
			~(PCIECFGREG_DEVCTRL_MPS_MASK));
	}
	else
#endif // endif
	dma_param_set(di, HNDDMA_PID_RX_BURSTLEN,
		PCIECOREREV(pciedev->corerev) >= 19 ? DMA_BL_128 : DMA_BL_64);
#ifdef PCIDMA_WAIT_CMPLT_ON
	if ((PCIECOREREV(pciedev->corerev) == 16) ||
		(PCIECOREREV(pciedev->corerev) == 23) ||
		(PCIECOREREV(pciedev->corerev) == 24)) {
		dma_param_set(di, HNDDMA_PID_RX_WAIT_CMPL, 1);
	}
#endif // endif
	dma_param_set(di, HNDDMA_PID_TX_PREFETCH_CTL, 1);
	dma_param_set(di, HNDDMA_PID_TX_PREFETCH_THRESH, 1);
	dma_param_set(di, HNDDMA_PID_TX_BURSTLEN,
		PCIECOREREV(pciedev->corerev) >= 19 ? DMA_BL_128 : DMA_BL_64);

	if (!dma_rxreset(di)) {
		TRAP_DUE_DMA_ERROR(("dma_rxreset failed for d2h_di engine\n"));
	}
	if (!dma_txreset(di)) {
		TRAP_DUE_DMA_ERROR(("dma_txreset failed for d2h_di engine\n"));
	}
	dma_rxinit(di);
	dma_txinit(di);
}

int
pciedev_pciedma_init(struct dngl_bus *p_pcie_dev)
{
	pcie_devdmaregs_t *dmaregs;
	osl_t *osh = p_pcie_dev->osh;
	si_t *sih = p_pcie_dev->sih;
	uint8 i;

	/* Initialize the MRRS for BCM4365/BCM4366 */
	if (PCIECOREREV(p_pcie_dev->corerev) == 15 || PCIECOREREV(p_pcie_dev->corerev) == 20) {
		pcie_init_mrrs(p_pcie_dev, 1024);
	}

	for (i = 0; i < MAX_DMA_CHAN; i++) {
		if (VALID_DMA_CHAN_NO_IDMA(i, p_pcie_dev)) {
			/* Configure the dma engines for both tx and rx */
			dmaregs = &p_pcie_dev->regs->u.pcie2.h2d0_dmaregs + i * 2;
			/* Configure the dma engines for both tx and rx */
			p_pcie_dev->h2d_di[i] =
				dma_attach(osh, "H2D", sih,
					&dmaregs->tx, &dmaregs->rx,
					p_pcie_dev->tunables[PCIEBUS_H2D_NTXD],
					p_pcie_dev->tunables[PCIEBUS_H2D_NRXD],
					p_pcie_dev->tunables[RXBUFSIZE],
					PD_RX_HEADROOM, PD_NRXBUF_POST,
					0 /* h2d dma rxoffset */, &pd_msglevel);
			if (p_pcie_dev->h2d_di[i] == NULL) {
				PCI_ERROR(("dma_attach fail agg desc h2d, chan %d\n", i));
				return BCME_ERROR;
			}
			pciedev_dma_params_init_h2d(p_pcie_dev, p_pcie_dev->h2d_di[i]);
			/* TX/RX descriptor avail ptrs */
			p_pcie_dev->avail[i][HTOD][TXDESC] =
				(uint *) dma_getvar(p_pcie_dev->h2d_di[i], "&txavail");
			p_pcie_dev->avail[i][HTOD][RXDESC] =
				(uint *) dma_getvar(p_pcie_dev->h2d_di[i], "&rxavail");

			/* D2H aggdesc update through host2dev dma engine 0 */
			dmaregs = &p_pcie_dev->regs->u.pcie2.d2h0_dmaregs + i * 2;
			/* Configure the dma engines for both tx and rx */
			p_pcie_dev->d2h_di[i] =
				dma_attach(osh, "D2H", sih,
					&dmaregs->tx, &dmaregs->rx,
					p_pcie_dev->tunables[PCIEBUS_D2H_NTXD],
					p_pcie_dev->tunables[PCIEBUS_D2H_NRXD],
					p_pcie_dev->tunables[RXBUFSIZE],
					PD_RX_HEADROOM, PD_NRXBUF_POST,
					0 /* d2h dma rxoffset */, &pd_msglevel);
			if (p_pcie_dev->d2h_di[i] == NULL) {
				PCI_ERROR(("dma_attach fail for agg desc d2h, chan %d\n", i));
				return BCME_ERROR;
			}
			pciedev_dma_params_init_d2h(p_pcie_dev, p_pcie_dev->d2h_di[i]);
			/* TX/RX descriptor avail ptrs */
			p_pcie_dev->avail[i][DTOH][TXDESC] =
				(uint *) dma_getvar(p_pcie_dev->d2h_di[i], "&txavail");
			p_pcie_dev->avail[i][DTOH][RXDESC] =
				(uint *) dma_getvar(p_pcie_dev->d2h_di[i], "&rxavail");
		}
	}

#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ENAB(p_pcie_dev)) {
		/* Configure the dma engines for both tx and rx */
		dmaregs = &p_pcie_dev->regs->u.pcie2.h2d2_dmaregs;
		/*     Hw provides max 16 Implicit dma rx/tx desc.
		*       However current Implicit DMA feature uses only two desc
		*       (h2d write index transfer and d2h read index transfer).
		*       For DMA attach function, it should use grater desc number
		*       than the real use which is two.
		*       Also it should be two's power. So, attach here 16 descs for now.
		*         TBD : to reduce the memory, fix dma apis to attach equal desc number
		*                  as the real use.
		*       Attaches Implicit DMA here because dma attach is in attach context.
		*         TBD : find the way to attach in pciedev_idma_init() instead here,
		*               so we can save memory if implicit dma is not inited.
		*/
		/* Ideally, we should set ntxd and nrxd to MAX_IDMA_DESC+1, but
		* since we don't merge the NOTPOWEROF2_DD from EAGLE so let's
		* twice MAX_IDMA_DESC for ntxd and nrxd.
		*/
		p_pcie_dev->h2d_di[DMA_CH2] =
			dma_attach(osh, "H2D", sih,
				&dmaregs->tx, &dmaregs->rx,
				MAX_IDMA_DESC*2, MAX_IDMA_DESC*2,
				p_pcie_dev->tunables[RXBUFSIZE],
				PD_RX_HEADROOM, MAX_IDMA_DESC-1,
				0 /* h2d dma rxoffset */, &pd_msglevel);
		pciedev_dma_params_init_h2d(p_pcie_dev, p_pcie_dev->h2d_di[DMA_CH2]);
		/* TX/RX descriptor avail ptrs */
		p_pcie_dev->avail[DMA_CH2][HTOD][TXDESC] =
			(uint *) dma_getvar(p_pcie_dev->h2d_di[DMA_CH2], "&txavail");
		p_pcie_dev->avail[DMA_CH2][HTOD][RXDESC] =
			(uint *) dma_getvar(p_pcie_dev->h2d_di[DMA_CH2], "&rxavail");
	}
#endif /* BCMPCIE_IDMA */

	return 0;
} /* pciedev_pciedma_init */

#ifdef HMAPTEST

void
pciedev_hmaptest_m2mwrite_dmaxfer_done(struct dngl_bus *pciedev, void *p)
{

	PCI_PRINT(("pciedev_hmaptest_m2mwrite_dmaxfer_done\n"));
	/* just set a flag saying DMA transfer done */
	PKTFREE(pciedev->osh, p, TRUE);
	pciedev->hmaptest_m2mwrite_p = NULL; /* marks dma done */
	pciedev->hmaptest_inprogress = FALSE;
	return;
}

void
pciedev_hmaptest_m2mwrite_do_dmaxfer(struct dngl_bus *pciedev,
	void * p, uint32 haddr_hi, uint32 haddr_lo)
{
	ret_buf_t haddr;
	uint32 pktlen;

	PCI_PRINT(("pciedev_hmaptest_m2mwrite_do_dmaxfer\n"));
	pktlen = PKTLEN(pciedev->osh, p);

	haddr.high_addr = haddr_hi;
	haddr.low_addr = haddr_lo;

	PCI_PRINT(("sceduled the DMA  for packet 0x%8x, of len %d\n", (uint32)p, pktlen));
	PCI_PRINT(("sceduled the M2M write DMA  to haddr.hi=%08x haddr.lo=%08x len %d\n",
		haddr_hi, haddr_lo, pktlen));
	pciedev_tx_pyld(pciedev, p, &haddr, pktlen, MSG_TYPE_HMAPTEST_PYLD);

	return;
}

void
pciedev_hmaptest_m2mread_dmaxfer_done(struct fetch_rqst *fr, bool cancelled)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)fr->ctx;
	uchar * pktdata;
	uint32 i, len, pktlen;
	void *p = pciedev->hmaptest_m2mread_p;
	/* buffer is in the dongle */
	PCI_PRINT(("pciedev_hmaptest_m2mread_dmaxfer_done\n"));
	if (cancelled) {
		PCI_PRINT(("pciedev_hmaptest_m2mread_dmaxfer_done: Request"
			" cancelled! Dropping hmaptest dmaxfer rqst...\n"));
		PCIEDEV_PKTFETCH_CANCEL_CNT_INCR(pciedev);
		ASSERT(0);
		return;
	}
	/* Printing pattern only in DBG builds */
	pktdata = PKTDATA(pciedev->osh, p);
	pktlen = PKTLEN(pciedev->osh, p);
	PCI_PRINT(("hmaptest: m2m read pattern =\n"));
	for (i = 0; i < pktlen; i += len) {
		PCI_PRINT(("%s", (pktdata + i)));
		len = strlen((char *)(pktdata + i)) + 1;
	}
	PCI_PRINT(("\n\n"));
	PKTFREE(pciedev->osh, pciedev->hmaptest_m2mread_p, TRUE);
	pciedev->hmaptest_m2mread_p = NULL; /* marks dma done */
	pciedev->hmaptest_inprogress = FALSE;

}

void
pciedev_hmaptest_m2mread_do_dmaxfer(struct dngl_bus *pciedev,
	void * p, uint32 haddr_hi, uint32 haddr_lo)
{
	struct fetch_rqst *fetch_rqst;
	uint32 pktlen;
	fetch_rqst = (struct fetch_rqst *)PKTDATA(pciedev->osh, p);
	PKTPULL(pciedev->osh, p, sizeof(struct fetch_rqst));

	PHYSADDR64HISET(fetch_rqst->haddr, haddr_hi);
	PHYSADDR64LOSET(fetch_rqst->haddr, haddr_lo);
	pktlen = PKTLEN(pciedev->osh, p);
	fetch_rqst->size = pktlen;
	fetch_rqst->dest = PKTDATA(pciedev->osh, p);
	fetch_rqst->cb = pciedev_hmaptest_m2mread_dmaxfer_done;
	fetch_rqst->ctx = (void *)pciedev;
	fetch_rqst->flags = 0;

	PCI_PRINT(("sending a request for %d bytes at host addr 0x%04x, "
			"buffer 0x%8x\n", pktlen, haddr_lo, (uint32)p));
	PCI_PRINT(("fetchrequest is at 0x%8x, and pktdata is 0x%8x\n",
			(uint32)fetch_rqst, (uint32)PKTDATA(pciedev->osh, p)));
#ifdef BCMPCIEDEV
		if (BCMPCIEDEV_ENAB()) {
			hnd_fetch_rqst(fetch_rqst);
		}
#endif /* BCMPCIEDEV */

	return;
}

int
pciedev_do_arm_hmaptest(struct dngl_bus *pciedev, haddr64_t haddr64, int32 is_write, uint32 len)
{

	uint32 hiaddr, loaddr;
	int data;
	int ret = BCME_OK;
	uint32 i;
	len = ALIGN_SIZE(len, (sizeof(int32)));

	if (is_write) {
		PCI_PRINT(("hmaptest: ARM write to host mem\n"));
	} else {
		PCI_PRINT(("hmaptest: ARM read from host mem\n"));
	}

	for (i = 0; i < len; i += sizeof(int32)) {
		/* ARM access host mem */
		hiaddr = HADDR64_HI(haddr64);
		loaddr = HADDR64_LO(haddr64);
		data = 0xbabecafe;
		math_add_64(&hiaddr, &loaddr, i);
		if ((ret = si_bpind_access(pciedev->sih, (PCIE_ADDR_OFFSET | hiaddr), loaddr,
			&data, (is_write ? FALSE : TRUE))) != BCME_OK) {
			PCI_PRINT(("hmaptest: ARM access host addr failed\n"));
			return ret;
		}
		PCI_PRINT(("val=%08x addr.lo = %08x\n", data, loaddr));
	}
	/* ARM HMAPTEST complete */
	pciedev->hmaptest_inprogress = FALSE;
	return ret;

}

int
pciedev_do_m2m_hmaptest(struct dngl_bus *pciedev, haddr64_t haddr64, int32 is_write, uint32 len)
{

	void * p;
	uint32 maxbuflen, i;
	uchar * pktdata;
	uint32 hiaddr, loaddr;
	int ret = BCME_OK;
	hiaddr = HADDR64_HI(haddr64);
	loaddr = HADDR64_LO(haddr64);
	PCI_PRINT(("hmaptest: wl bus:hmaptest do m2m dma access write=%d\n", is_write));
	if (len < PCIE_MEM2MEM_DMA_MIN_LEN) {
		PCI_PRINT(("hmaptest: wl bus:hmaptest invalid len=%d\n", len));
		return BCME_ERROR;
	}
	if (is_write) {
		char * pattern = "fw hmap m2m write\n";
		uint32 patternlen = strlen(pattern) + 1;
		/* using maxbuflen = min(PKTBUFSZ,len) */
		maxbuflen = MIN(len, (PKTBUFSZ));

		p = PKTGET(pciedev->osh, maxbuflen, TRUE);
		if (p == NULL) {
			PCI_PRINT(("%s pktget failed\n", __FUNCTION__));
			return BCME_NOMEM;
		}

		PKTSETLEN(pciedev->osh, p, maxbuflen);
		pktdata = PKTDATA(pciedev->osh, p);
		memset(pktdata, 0, maxbuflen);
		/* fill some pattern before writing */
		/* print the sent pattern */
		PCI_PRINT(("hmaptest: m2m write pattern =\n"));
		for (i = 0; i < maxbuflen; i += patternlen) {
			memcpy((pktdata + i), pattern, patternlen);
			PCI_PRINT(("%s", (pktdata + i)));
		}
		*(pktdata + maxbuflen) = '\0';
		pciedev->hmaptest_m2mwrite_p = p; /* marks dma start */
		pciedev_hmaptest_m2mwrite_do_dmaxfer(pciedev, p, hiaddr, loaddr);

	} else {
		len += sizeof(struct fetch_rqst);
		maxbuflen = MIN(len, (PKTBUFSZ));

		p = PKTGET(pciedev->osh, maxbuflen, TRUE);
		if (p == NULL) {
			PCI_PRINT(("%s pktget failed\n", __FUNCTION__));
			return BCME_NOMEM;
		}
		pktdata = PKTDATA(pciedev->osh, p);
		memset(pktdata, 0, maxbuflen);
		pciedev->hmaptest_m2mread_p = p;
		PKTSETLEN(pciedev->osh, p, maxbuflen);
		pciedev_hmaptest_m2mread_do_dmaxfer(pciedev, p, hiaddr, loaddr);
	}
	return ret;
}

static int
pciedev_process_hmaptest_msg(struct dngl_bus *pciedev, char * buf, int offset,
	bool set, uint32 *outlen)
{
	uint32 len;
	int ret = BCME_OK;
	haddr64_t haddr64;
	int32 is_write, hmap_accesstype;
	pcie_hmaptest_t *hmap = (pcie_hmaptest_t*)&buf[offset];

	/* HMAP enabled from corerev 24 [0x18] */
	if ((PCIECOREREV(pciedev->corerev) < 24)) {
		PCI_PRINT(("hmaptest: HMAP not available in pci corerev=%d\n",
			(PCIECOREREV(pciedev->corerev))));
		return BCME_UNSUPPORTED;
	}

	if (!set) {
		return BCME_UNSUPPORTED;
	}

	hmap = (pcie_hmaptest_t*)&buf[offset];
	is_write = ltoh32(hmap->is_write);
	hmap_accesstype = ltoh32(hmap->accesstype);
	len = ltoh32(hmap->xfer_len);
	HADDR64_LO_SET(haddr64, ltoh32(hmap->host_addr_lo));
	HADDR64_HI_SET(haddr64, ltoh32(hmap->host_addr_hi));
	PCI_PRINT(("hmaptest: process_hmaptest_msg called accesstype=%d is_write=%d len=%d\n",
		hmap_accesstype, is_write, len));
	PCI_PRINT(("hmaptest: is_write =%d accesstype=%d\n", is_write, hmap_accesstype));
	PCI_PRINT(("hmaptest:" HADDR64_FMT "\n", HADDR64_VAL(haddr64)));

	/* checks before starting HMAPTEST */
	/* iovar indicates to stop */
	if (hmap_accesstype == HMAPTEST_ACCESS_NONE) {
		pciedev->hmaptest_inprogress = FALSE;
		PCI_PRINT(("hmaptest: Stopped! status of prev test (M2M) r=%p w=%p \n",
			pciedev->hmaptest_m2mread_p, pciedev->hmaptest_m2mwrite_p));
		return BCME_OK;
	}
	/* Host did not specify address to access for HMAPTEST */
	if (HADDR64_LO(haddr64) == 0) {
		PCI_PRINT(("hmaptest: Err: DHD did not specify address to access for the test\n"));
		PCI_PRINT(("hmaptest: Err: likely DHD not build with HMAPTEST, return here!\n"));
		return BCME_UNSUPPORTED;
	}
	if (pciedev->hmaptest_inprogress) {
		PCI_PRINT(("hmaptest: already in progress\n"));
		return BCME_BUSY;
	}

	pciedev->hmaptest_inprogress = TRUE;

	if (hmap_accesstype == HMAPTEST_ACCESS_ARM) {
		if ((ret = pciedev_do_arm_hmaptest(pciedev, haddr64, is_write, len)) != BCME_OK) {
			PCI_PRINT(("hmaptest: pciedev_do_arm_hmaptest failed ret=%d \n", ret));
			return ret;
		}
	} else if (hmap_accesstype == HMAPTEST_ACCESS_M2M) {
		if ((ret = pciedev_do_m2m_hmaptest(pciedev, haddr64, is_write, len)) != BCME_OK) {
			PCI_PRINT(("hmaptest: pciedev_do_m2m_hmaptest failed ret=%d \n", ret));
			return ret;
		}
	}
	/* TODO have an API similar to D11 loopback for D11 HMAPTEST to host mem
	 * currently d11 loopback API  does loopback only on lbuf
	 */

	return BCME_OK;
}

static int
pciedev_process_hmap_msg(struct dngl_bus *pciedev, char *buf, int offset,
	bool set, uint32 *outlen)
{
	uint32 val;
	pcie_hmap_t *hmap_params;
	uint32 nwindows, i;

	/* HMAP enabled from corerev 24 [0x18] */
	if ((PCIECOREREV(pciedev->corerev) < 24)) {
		PCI_PRINT(("hmap: HMAP not available in pci corerev=%d\n",
			(PCIECOREREV(pciedev->corerev))));
		return BCME_UNSUPPORTED;
	}
	if (set) {
		hmap_params = (pcie_hmap_t*)&buf[offset];
		val = ltoh32(hmap_params->enable);
		PCI_PRINT(("hmap:pciedev_process_hmap_msg called val=%d set =%d \n", val, set));
		/* enable / disable using defintmask */
		if (val) {
			pciedev->defintmask = pciedev->defintmask | PD_HMAP_VIO_INTMASK;
		} else {
			pciedev->defintmask = pciedev->defintmask & ~(PD_HMAP_VIO_INTMASK);
		}
	} else {
		PCI_PRINT(("hmap:pciedev_process_hmap_msg called set =%d \n", set));
		hmap_params = (pcie_hmap_t*)buf;
		hmap_params->enable = htol32((pciedev->defintmask & (PD_HMAP_VIO_INTMASK))
					>> PD_HMAP_VIO_INTSHIFT);
		PCI_PRINT(("hmap:pciedev_process_hmap_msg hmap enabled =%08x \n",
			hmap_params->enable));
		hmap_params->hmap_violationaddr_lo =
			htol32(pciedev->hmap_regs.hmap_violationaddr_lo);
		hmap_params->hmap_violationaddr_hi =
			htol32(pciedev->hmap_regs.hmap_violationaddr_hi);
		hmap_params->hmap_violation_info = htol32(pciedev->hmap_regs.hmap_violation_info);
		nwindows = R_REG(pciedev->osh,
			&pciedev->regs->u.pcie2.hmap_window_config);
		hmap_params->window_config = htol32(nwindows);
		PCI_PRINT(("hmap:pciedev_process_hmap_msg window_config =%08x\n",
			hmap_params->window_config));
		nwindows = (nwindows & PCI_HMAP_NWINDOWS_MASK) >> PCI_HMAP_NWINDOWS_SHIFT;
		hmap_params->nwindows = htol32(nwindows);
		PCI_PRINT(("hmap:pciedev_process_hmap_msg return info nwindows=%d\n", nwindows));
		for (i = 0; i < nwindows; i++) {
			hmap_params->hwindows[i].baseaddr_lo = htol32(R_REG(pciedev->osh,
				&pciedev->regs->u.pcie2.hmapwindow[i].baseaddr_lo));

			hmap_params->hwindows[i].baseaddr_hi = htol32(R_REG(pciedev->osh,
				&pciedev->regs->u.pcie2.hmapwindow[i].baseaddr_hi));

			hmap_params->hwindows[i].windowlength = htol32(R_REG(pciedev->osh,
				&pciedev->regs->u.pcie2.hmapwindow[i].windowlength));
		}
		*outlen = sizeof(pcie_hmap_t) + ((nwindows) * sizeof(hmapwindow_t));
		/* clear global pciedev->hmap_regs */
		memset(&pciedev->hmap_regs, 0, sizeof(pcie_hmapviolation_t));
	}
	return BCME_OK;
}
#endif /* HMAPTEST */

bool
pciedev_hmap_intr_process(struct dngl_bus *pciedev)
{
	uint32 val;
	/* Read HMAP violation Registers and save them to global */
	pciedev->hmap_regs.hmap_violationaddr_lo = R_REG(pciedev->osh,
		&pciedev->regs->u.pcie2.hmapviolation.hmap_violationaddr_lo);
	pciedev->hmap_regs.hmap_violationaddr_hi = R_REG(pciedev->osh,
		&pciedev->regs->u.pcie2.hmapviolation.hmap_violationaddr_hi);
	pciedev->hmap_regs.hmap_violation_info = R_REG(pciedev->osh,
		&pciedev->regs->u.pcie2.hmapviolation.hmap_violation_info);

		/* clear HMAP violation */
		/* Error 18:17	00 - No errors,
		 * 01 - Device has detected a violation,
		 * 10 - Undefined, 11 - Multiple violations detected.
		 * Software writes 0b11 to clear error
		 * and deassert the HMAP violation interrupt.
		 */
	val = (PD_HMAP_VIO_CLR_VAL << PD_HMAP_VIO_SHIFT_VAL);
	W_REG(pciedev->osh,
		&pciedev->regs->u.pcie2.hmapviolation.hmap_violation_info, val);
	(void)R_REG(pciedev->osh,
		&pciedev->regs->u.pcie2.hmapviolation.hmap_violation_info);
	PCI_PRINT(("HMAP Violation occured!!\n"));
	PCI_PRINT(("addr.lo=%08x addr.hi=%08x\n", pciedev->hmap_regs.hmap_violationaddr_lo,
		pciedev->hmap_regs.hmap_violationaddr_hi));
	PCI_PRINT(("violation_info=%08x\n", pciedev->hmap_regs.hmap_violation_info));

	return FALSE;
} /* pciedev_hmap_intr_process */

#ifdef PCIE_DMAXFER_LOOPBACK
uint32
pciedev_process_do_dest_lpbk_dmaxfer_done(struct dngl_bus *pciedev, void *p)
{
	/* once done go back to loop, send the completion message */
	pciedev->dmaxfer_loopback->lpbk_dmaxfer_push_pend--;

	/* put back the packet into the lpbk_dma_txq after pushing the header */
	PKTPUSH(pciedev->osh, p, sizeof(pciedev_lpbk_ctx_t));
	PKTSETLEN(pciedev->osh, p, pciedev->dmaxfer_loopback->lpbk_dma_pkt_fetch_len);

	PCI_TRACE(("D2H DMA done for packet 0x%8x, still pendind %d, len %d\n", (uint32)p,
		pciedev->dmaxfer_loopback->lpbk_dmaxfer_push_pend, PKTLEN(pciedev->osh, p)));

	pktq_penq(&pciedev->dmaxfer_loopback->lpbk_dma_txq, 0, p);

	if (pciedev->dmaxfer_loopback->lpbk_dma_dest_delay &&
			!pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq_pend)) {
		dngl_add_timer(pciedev->dmaxfer_loopback->lpbk_dest_dmaxfer_timer,
				pciedev->dmaxfer_loopback->lpbk_dma_dest_delay, FALSE);
		return BCME_OK;
	}

	if (pciedev->dmaxfer_loopback->lpbk_dmaxfer_push_pend == 0 ||
		pciedev->dmaxfer_loopback->no_delay) {
		++pciedev->dmaxfer_loopback->push_pkts;
		PCI_TRACE(("D2H DMA done for pending\n"));
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

	haddr.high_addr = pciedev->dmaxfer_loopback->pend_dmaxfer_destaddr_hi;

	haddr.low_addr = pciedev->dmaxfer_loopback->pend_dmaxfer_destaddr_lo;

	math_add_64(&pciedev->dmaxfer_loopback->pend_dmaxfer_destaddr_hi,
		&pciedev->dmaxfer_loopback->pend_dmaxfer_destaddr_lo, pktlen);

	PCI_TRACE(("sceduled the DMA  for packet 0x%8x, of len %d\n", (uint32)p, pktlen));
	pciedev_tx_pyld(pciedev, p, &haddr, pktlen, MSG_TYPE_LPBK_DMAXFER_PYLD);

	return 0;
}

static void
pciedev_lpbk_dest_dmaxfer_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) hnd_timer_get_ctx(t);
	PCI_TRACE(("sceduled DMA for lpbk_dma_txq_pend"));
	pciedev_process_lpbk_dma_txq_pend_pkt(pciedev);
}

static void
pciedev_lpbk_src_dmaxfer_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) hnd_timer_get_ctx(t);
	PCI_TRACE(("sceduled DMA for fetchrq lpbk dma txq"));
	pciedev_fetchrq_lpbk_dma_txq_pkt(pciedev);
}

static uint32
pciedev_process_lpbk_dma_txq_pend_pkt(struct dngl_bus *pciedev)
{
	cmn_msg_hdr_t *msg;
	void *p;

	/* schedule a d2h req and let it finish */
	if (pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq_pend)) {
		PCI_ERROR(("pciedev_process_lpbk_dma_txq_pend_pkt:"
			"PEND PKTQ is empty why..so return\n"));
		return BCME_OK;
	}

	p = pktq_deq(&pciedev->dmaxfer_loopback->lpbk_dma_txq_pend, 0);
	msg = (cmn_msg_hdr_t *)PKTPUSH(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
	msg->msg_type = MSG_TYPE_LPBK_DMAXFER_PYLD;
	pciedev->dmaxfer_loopback->lpbk_dmaxfer_push_pend++;

	PCI_TRACE(("now trying "
		"to push the packet 0x%8x, len %d, pend count %d\n",
		(uint32)p, PKTLEN(pciedev->osh, p),
		pciedev->dmaxfer_loopback->lpbk_dmaxfer_push_pend));

	(void)pciedev_queue_d2h_req(pciedev, p, D2H_REQ_PRIO_0);
	if (!IS_IOCTLREQ_PENDING(pciedev))
		pciedev_queue_d2h_req_send(pciedev);

	return BCME_OK;
}

static uint32
pciedev_process_do_dest_lpbk_dmaxfer(struct dngl_bus *pciedev)
{
	/* schedule a d2h req and let it finish */
	if (pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq_pend)) {
		PCI_ERROR(("pciedev_process_do_dest_lpbk_dmaxfer:"
			"PEND PKTQ is empty why..so return\n"));
		return BCME_OK;
	}

	while (!pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq_pend)) {
	}

	return 0;
}

/* Send IOCTL to wl module to initialize the DMA engine in D11 core for the loopback test */
static int
pciedev_d11_dma_lpbk_init(struct dngl_bus *pciedev)
{
	return dngl_dev_ioctl(pciedev->dngl, RTED11DMALPBK_INIT, NULL, 0);
}

/* Send IOCTL to wl module to uninitialize the D11 DMA loopback test */
static int
pciedev_d11_dma_lpbk_uninit(struct dngl_bus *pciedev)
{
	return dngl_dev_ioctl(pciedev->dngl, RTED11DMALPBK_UNINIT, NULL, 0);
}

/* Send IOCTL with buffer information to wl module to kick off a D11 DMA loopback test */
static int
pciedev_d11_dma_lpbk_run(struct dngl_bus *pciedev, uint8* buf, int len)
{
	d11_dmalpbk_args_t args = {buf, len};

	return dngl_dev_ioctl(pciedev->dngl, RTED11DMALPBK_RUN, (void *)&args, sizeof(args));
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

	if (pciedev->dmaxfer_loopback->d11_lpbk && (pciedev->dmaxfer_loopback->d11_lpbk_err == 0)) {
		pciedev->dmaxfer_loopback->d11_lpbk_err =
			pciedev_d11_dma_lpbk_run(pciedev, fr->dest, fr->size);
	}

	if (pciedev->dmaxfer_loopback->lpbk_dma_src_delay) {
		if ((pciedev->dmaxfer_loopback->pend_dmaxfer_len != 0) &&
			!pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq)) {
			dngl_add_timer(pciedev->dmaxfer_loopback->lpbk_src_dmaxfer_timer,
			pciedev->dmaxfer_loopback->lpbk_dma_src_delay, FALSE);
			return;
		} else {
			pciedev->dmaxfer_loopback->lpbk_dmaxfer_fetch_pend = 0;
		}
	} else {
		/* schedule the timer to move this request to queue or move it to queue */
		pciedev->dmaxfer_loopback->lpbk_dmaxfer_fetch_pend--;
	}

	/* now the whole thing is in local memory */
	if (pciedev->dmaxfer_loopback->lpbk_dmaxfer_fetch_pend == 0 ||
		pciedev->dmaxfer_loopback->no_delay) {
		++pciedev->dmaxfer_loopback->pull_pkts;
		PCI_TRACE(("fetch of the one blob done, so push it now \n"));
		if (pciedev->dmaxfer_loopback->lpbk_dma_dest_delay)
			dngl_add_timer(pciedev->dmaxfer_loopback->lpbk_dest_dmaxfer_timer,
			pciedev->dmaxfer_loopback->lpbk_dma_dest_delay, FALSE);
		else
			pciedev_process_do_dest_lpbk_dmaxfer(pciedev);
	}
}

static uint32
pciedev_fetchrq_lpbk_dma_txq_pkt(struct dngl_bus *pciedev)
{
	pciedev_lpbk_ctx_t *ctx;
	struct fetch_rqst *fetch_rqst;
	void *p;
	uint32 buflen;

	if (pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq)) {
		PCI_TRACE(("pciedev_process_do_src_lpbk_dmaxfer: PKTQ is empty\n"));
		return BCME_OK;
	}

	p = pktq_deq(&pciedev->dmaxfer_loopback->lpbk_dma_txq, 0);

	ctx = (pciedev_lpbk_ctx_t *)PKTDATA(pciedev->osh, p);
	fetch_rqst = &ctx->fr;

	PKTPULL(pciedev->osh, p, sizeof(*ctx));

	if (pciedev->dmaxfer_loopback->pend_dmaxfer_len > MAX_DMA_XFER_SZ)
		buflen = MAX_DMA_XFER_SZ;
	else
		buflen = pciedev->dmaxfer_loopback->pend_dmaxfer_len;

	if (pciedev->dmaxfer_loopback->pend_dmaxfer_len - buflen < 8)
		buflen += (pciedev->dmaxfer_loopback->pend_dmaxfer_len - buflen);

	PKTSETLEN(pciedev->osh, p, buflen);

	pciedev->dmaxfer_loopback->pend_dmaxfer_len -= buflen;

	PHYSADDR64HISET(fetch_rqst->haddr,
			pciedev->dmaxfer_loopback->pend_dmaxfer_srcaddr_hi);

	PHYSADDR64LOSET(fetch_rqst->haddr,
			pciedev->dmaxfer_loopback->pend_dmaxfer_srcaddr_lo);

	math_add_64(&pciedev->dmaxfer_loopback->pend_dmaxfer_srcaddr_hi,
			&pciedev->dmaxfer_loopback->pend_dmaxfer_srcaddr_lo, buflen);

	fetch_rqst->size = buflen;
	fetch_rqst->dest = PKTDATA(pciedev->osh, p);
	fetch_rqst->cb = pciedev_process_src_lpbk_dmaxfer_done;
	fetch_rqst->ctx = (void *)pciedev;
	fetch_rqst->flags = 0;
	ctx->p = p;

	pciedev->dmaxfer_loopback->lpbk_dmaxfer_fetch_pend++;

	PCI_TRACE(("sending a request for %d bytes at host addr 0x%04x, "
		"buffer 0x%8x, fetch pend count is %d\n",
		buflen, pciedev->dmaxfer_loopback->pend_dmaxfer_srcaddr_lo, (uint32)p,
		pciedev->dmaxfer_loopback->lpbk_dmaxfer_fetch_pend));

	PCI_TRACE(("fetchrequest is at 0x%8x, and pktdata is 0x%8x\n",
		(uint32)fetch_rqst, (uint32)PKTDATA(pciedev->osh, p)));
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		hnd_fetch_rqst(fetch_rqst);
	}
#endif /* BCMPCIEDEV */
	return BCME_OK;
}

static uint32
pciedev_process_do_src_lpbk_dmaxfer(struct dngl_bus *pciedev)
{
	if (pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq)) {
		PCI_TRACE(("pciedev_process_do_src_lpbk_dmaxfer: PKTQ is empty\n"));
		pciedev_done_lpbk_dmaxfer(pciedev);
	}

	while ((pciedev->dmaxfer_loopback->pend_dmaxfer_len != 0) &&
		(!pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq))) {
		pciedev_fetchrq_lpbk_dma_txq_pkt(pciedev);
	}

	return 0;
} /* pciedev_process_do_src_lpbk_dmaxfer */

static uint32
pciedev_schedule_src_lpbk_dmaxfer(struct dngl_bus *pciedev)
{

	if (pciedev->dmaxfer_loopback->pend_dmaxfer_len == 0 &&
		pciedev->dmaxfer_loopback->lpbk_dmaxfer_fetch_pend == 0 &&
		pciedev->dmaxfer_loopback->lpbk_dmaxfer_push_pend == 0) {
		PCI_TRACE(("done with the dmalpbk request..so clean it up now\n"));
		pciedev_done_lpbk_dmaxfer(pciedev);
		return 0;
	}
	if (pciedev->dmaxfer_loopback->lpbk_dma_src_delay)
		dngl_add_timer(pciedev->dmaxfer_loopback->lpbk_src_dmaxfer_timer,
		pciedev->dmaxfer_loopback->lpbk_dma_src_delay, FALSE);
	else
		pciedev_process_do_src_lpbk_dmaxfer(pciedev);

	return 0;
}

static void
pciedev_done_lpbk_dmaxfer(struct dngl_bus *pciedev)
{
	void *p;
	int err = 0;

	while (!pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq)) {
		p = pktq_pdeq(&pciedev->dmaxfer_loopback->lpbk_dma_txq, 0);
		PKTFREE(pciedev->osh, p, TRUE);
	}
	PCI_TRACE(("send a message to indicate it is done now\n"));

	if (pciedev->dmaxfer_loopback->d11_lpbk) {
		pciedev_d11_dma_lpbk_uninit(pciedev);
		err = pciedev->dmaxfer_loopback->d11_lpbk_err;
	}

	pciedev_send_lpbkdmaxfer_complete(pciedev, err, pciedev->dmaxfer_loopback->lpbk_dma_req_id);
}

static  uint32
pciedev_prepare_lpbk_dmaxfer(struct dngl_bus *pciedev, uint32 len)
{
	uint32 pktlen = pciedev->dmaxfer_loopback->lpbk_dma_pkt_fetch_len;
	void *p;
	uint32 totpktlen = 0;

	if (!pktq_empty(&pciedev->dmaxfer_loopback->lpbk_dma_txq)) {
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
		pktq_penq(&pciedev->dmaxfer_loopback->lpbk_dma_txq, 0, p);
		totpktlen += MAX_DMA_XFER_SZ;
	}

	if (totpktlen == 0)  {
		PCI_ERROR(("couldn't allocate memory for the loopback request\n"));
		return 0;
	}

	PCI_TRACE(("allocated %d packets to handle the request, pktlen %d\n",
		pktq_n_pkts_tot(&pciedev->dmaxfer_loopback->lpbk_dma_txq), pktlen));

	/* now start the whole process */
	pciedev_schedule_src_lpbk_dmaxfer(pciedev);

	return 1;
}

static void
pciedev_process_lpbk_dmaxfer_msg(struct dngl_bus *pciedev, void *p)
{
	pcie_dma_xfer_params_t *dmap = (pcie_dma_xfer_params_t *)p;
	int ret;

	if (pciedev->dmaxfer_loopback->pend_dmaxfer_len != 0) {
		PCI_ERROR(("already a lpbk request pending, so fail the new one\n"));
		pciedev_send_lpbkdmaxfer_complete(pciedev, -1, dmap->cmn_hdr.request_id);
		return;
	}

	pciedev->dmaxfer_loopback->pend_dmaxfer_len = ltoh32(dmap->xfer_len);

	pciedev->dmaxfer_loopback->pend_dmaxfer_srcaddr_hi =
		ltoh32(dmap->host_input_buf_addr.high);

	pciedev->dmaxfer_loopback->pend_dmaxfer_srcaddr_lo =
		ltoh32(dmap->host_input_buf_addr.low);

	pciedev->dmaxfer_loopback->pend_dmaxfer_destaddr_hi =
		ltoh32(dmap->host_ouput_buf_addr.high);

	pciedev->dmaxfer_loopback->pend_dmaxfer_destaddr_lo =
		ltoh32(dmap->host_ouput_buf_addr.low);

	pciedev->dmaxfer_loopback->lpbk_dma_src_delay = ltoh32(dmap->srcdelay);
	pciedev->dmaxfer_loopback->lpbk_dma_dest_delay = ltoh32(dmap->destdelay);
	pciedev->dmaxfer_loopback->lpbk_dma_req_id = dmap->cmn_hdr.request_id;

	pciedev->dmaxfer_loopback->no_delay = (dmap->destdelay == 0 && dmap->srcdelay == 0);
	pciedev->dmaxfer_loopback->pull_pkts = 0;
	pciedev->dmaxfer_loopback->push_pkts = 0;

	pciedev->dmaxfer_loopback->lpbk_dma_pkt_fetch_len =
		MAX_DMA_XFER_SZ +  sizeof(pciedev_lpbk_ctx_t)+ 8 + 8;

	pciedev->dmaxfer_loopback->lpbk_dmaxfer_fetch_pend = 0;
	pciedev->dmaxfer_loopback->lpbk_dmaxfer_push_pend = 0;

	pciedev->dmaxfer_loopback->d11_lpbk = (dmap->flags & PCIE_DMA_XFER_FLG_D11_LPBK_MASK)
		>> PCIE_DMA_XFER_FLG_D11_LPBK_SHIFT;

	if (pciedev->dmaxfer_loopback->d11_lpbk) {
		PCI_ERROR(("Do d11 dma loopback\n"));
		pciedev->dmaxfer_loopback->d11_lpbk_err = 0;
		ret = pciedev_d11_dma_lpbk_init(pciedev);
		if (ret) {
			PCI_ERROR(("error to set up the d11 dma loopback, %d\n", ret));
			pciedev->dmaxfer_loopback->pend_dmaxfer_len = 0;
			pciedev_send_lpbkdmaxfer_complete(pciedev, ret, dmap->cmn_hdr.request_id);
			return;
		}
	}

	PCI_TRACE(("dma loopback request for length %d bytes\n",
			pciedev->dmaxfer_loopback->pend_dmaxfer_len));

	if (pciedev_prepare_lpbk_dmaxfer(pciedev,
		pciedev->dmaxfer_loopback->pend_dmaxfer_len) == 0) {
		PCI_ERROR(("error to set up the dma xfer loopback\n"));
		return;
	}
}

static int
pciedev_send_lpbkdmaxfer_complete(struct dngl_bus *pciedev, uint16 status, uint32 req_id)
{
	pcie_dmaxfer_cmplt_t *pkt;
	int ret;

	pkt = (pcie_dmaxfer_cmplt_t *)pciedev_lclpool_alloc_lclbuf(pciedev->dtoh_ctrlcpl);
	if (pkt) {
		bzero(pkt, sizeof(pcie_dmaxfer_cmplt_t));
		pkt->compl_hdr.status = status;
		pkt->cmn_hdr.request_id = req_id;
		pkt->cmn_hdr.msg_type = MSG_TYPE_LPBK_DMAXFER_CMPLT;
		/* Should check if teh resources are avaialble */
		if (!pciedev_xmit_msgbuf_packet(pciedev, pkt,
		        MSG_TYPE_LPBK_DMAXFER_CMPLT, MSGBUF_LEN(pciedev->dtoh_ctrlcpl),
		        pciedev->dtoh_ctrlcpl))
		{
			PCI_TRACE(("pciedev_send_lpbkdmaxfer_complete:"
				"failed to transmit ring status to host "));
			ret = BCME_TXFAIL;
			ASSERT(0);
		} else {
			ret = BCME_OK;
		}
	} else {
		ret = BCME_NOMEM;
	}

	return ret;
}
#endif /* PCIE_DMAXFER_LOOPBACK */

static  pciedev_ctrl_resp_q_t *
BCMATTACHFN(pciedev_setup_ctrl_resp_q)(struct dngl_bus *pciedev, uint8 item_cnt)
{
	pciedev_ctrl_resp_q_t * ctrl_resp_q;
	if (!(ctrl_resp_q =
		MALLOCZ(pciedev->osh, sizeof(pciedev_ctrl_resp_q_t)))) {
		PCI_ERROR(("Cmplt queue failed\n"));
		return NULL;
	}

	if (!(ctrl_resp_q->response = (ctrl_completion_item_t *) MALLOCZ(pciedev->osh,
		(sizeof(ctrl_completion_item_t) * item_cnt)))) {
		PCI_ERROR(("response information of cmplt queue failed\n"));
		return NULL;
	}
	ctrl_resp_q->depth = item_cnt;
	return ctrl_resp_q;
}

static void
pciedev_ctrl_resp_q_idx_upd(struct dngl_bus *pciedev, uint8 idx_type)
{
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;

	if (idx_type == PCIEDEV_CTRL_RESP_Q_WR_IDX) {
		resp_q->w_indx = (resp_q->w_indx + 1) % pciedev->tunables[MAXCTRLCPL];
	}
	else {
		resp_q->r_indx = (resp_q->r_indx + 1) % pciedev->tunables[MAXCTRLCPL];
	}
	if (resp_q->w_indx != resp_q->r_indx)
		SET_RXCTLRESP_PENDING(pciedev);
	else
		CLR_RXCTLRESP_PENDING(pciedev);
}

int
pciedev_process_ctrl_cmplt(struct dngl_bus *pciedev)
{
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	msgbuf_ring_t *ring = pciedev->dtoh_ctrlcpl;
	msgbuf_ring_t	*flow_ring;
	struct dll * cur, * prio;
	prioring_pool_t *prioring;
	ctrl_completion_item_t *resp;

	uint32 txdesc, rxdesc;

	uint8 r_indx = resp_q->r_indx;

	if (IS_RING_SPACE_EMPTY(resp_q->r_indx, resp_q->w_indx, resp_q->depth)) {
		/* Should not come here */
		PCI_PRINT(("ctrl resp queue empty..so return\n"));
		resp_q->scheduled_empty++;
		return BCME_OK;
	}

	/* Check for DMA Descriptors */
	txdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, pciedev->default_dma_ch, DTOH, TXDESC);
	rxdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, pciedev->default_dma_ch, DTOH, RXDESC);

	if ((txdesc < MIN_TXDESC_NEED_WO_PHASE) || (rxdesc < MIN_RXDESC_NEED_WO_PHASE)) {
		PCI_TRACE(("Not enough DMA Descriptors!\n"));
		resp_q->dma_desc_lack_cnt++;
		return BCME_ERROR;
	}

	/* Check for local ring bufs for D2H Ctrl Cmplt */
	if ((resp = (ctrl_completion_item_t *)
			pciedev_lclpool_alloc_lclbuf(pciedev->dtoh_ctrlcpl)) == NULL) {
		PCI_ERROR(("Ring: d2hc Local ring bufs not available\n"));
		return BCME_ERROR;
	}

	/* Check for available entries in D2H Ctrl Cmplt */
	if (MSGBUF_WRITE_SPACE_AVAIL(ring) < 1) {
		resp_q->ring_space_lack_cnt++;
		PCI_TRACE(("Ring: d2hc space not available\n"));
		return BCME_ERROR;
	}

	memcpy((void *)resp, &(resp_q->response[r_indx].ctrl_response),
		sizeof(ctrl_completion_item_t));

	/* submit it to d2h contrl submission ring */
	if (!pciedev_xmit_msgbuf_packet(pciedev, resp,
	        ((cmn_msg_hdr_t*)resp)->msg_type,
	        MSGBUF_LEN(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl))
	{
		/* find a way to submit local ring buffer back to pool */
		resp_q->dma_problem_cnt++;
		PCI_ERROR(("Ring: d2hc xmit message failed: Not handled now\n"));
		return BCME_ERROR;
	}

	pciedev_ctrl_resp_q_idx_upd(pciedev, PCIEDEV_CTRL_RESP_Q_RD_IDX);

	/* Now that we one space in ctrl_resp_q
	 * clear out pending flring responses (delete or flush)
	 */
	if ((resp_q->num_flow_ring_delete_resp_pend) ||
		(resp_q->num_flow_ring_flush_resp_pend)) {
			/* loop through nodes */
			prio = dll_head_p(&pciedev->active_prioring_list);
			while (!dll_end(prio, &pciedev->active_prioring_list)) {
				prioring = (prioring_pool_t *)prio;

				cur = dll_head_p(&prioring->active_flowring_list);
				while (!dll_end(cur, &prioring->active_flowring_list)) {
					flow_ring = ((flowring_pool_t *)cur)->ring;
					if ((flow_ring->status & FLOW_RING_DELETE_RESP_RETRY) ||
						(flow_ring->status & FLOW_RING_FLUSH_RESP_RETRY)) {
						pciedev_process_pending_flring_resp(pciedev,
							flow_ring);
						return BCME_OK;
					}
					/* next node */
					cur = dll_next_p(cur);
				}
				/* get next priority ring node */
				prio = dll_next_p(prio);
			}
	}

	/* If we have more than (PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY +
	 * PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY) entries available,
	 * then we can accept new ctrl messages
	 */
	if (WRITE_SPACE_AVAIL(resp_q->r_indx, resp_q->w_indx, resp_q->depth) >
		(PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY + PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY)) {
			pciedev->ctrl_resp_q->status &= ~CTRL_RESP_Q_FULL;
	}

	return BCME_OK;
}

static int
pciedev_schedule_ctrl_cmplt(struct dngl_bus *pciedev, ctrl_completion_item_t *ctrl_cmplt)
{
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	cmn_msg_hdr_t *msg_hdr = (cmn_msg_hdr_t *)(ctrl_cmplt->ctrl_response);

	uint8 w_indx = resp_q->w_indx;
	uint8 wr_space_avail;

	if (IS_RING_SPACE_FULL(resp_q->r_indx, resp_q->w_indx, resp_q->depth)) {
		/* Should NOT come here! */
		TRAP_DUE_DMA_RESOURCES(("Ctrl cmplt q full\n"));
	}

	/* PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY (2) entries reserved for IOCTLs
	 * PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY (1) is reserved for general status
	 * messages
	 * If not enough space in the local queue:
	 * MSG_TYPE_FLOW_RING_CREATE_CMPLT and MSG_TYPE_RING_STATUS are not retried.
	 * Hence Trap.
	 * MSG_TYPE_FLOW_RING_DELETE_CMPLT and MSG_TYPE_FLOW_RING_FLUSH_CMPLT are retried
	 * Hence return BCME_ERROR
	 * MSG_TYPE_GEN_STATUS is not retried but has one space reserved
	 * Hence Trap
	 */
	wr_space_avail = WRITE_SPACE_AVAIL(resp_q->r_indx, resp_q->w_indx, resp_q->depth);
	switch (msg_hdr->msg_type) {
		case MSG_TYPE_FLOW_RING_CREATE_CMPLT:
		case MSG_TYPE_D2H_RING_CREATE_CMPLT:
		case MSG_TYPE_H2D_RING_CREATE_CMPLT:
		case MSG_TYPE_D2H_RING_DELETE_CMPLT:
		case MSG_TYPE_H2D_RING_DELETE_CMPLT:
		case MSG_TYPE_RING_STATUS:
			if (wr_space_avail <= (PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY +
				PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY)) {
					pciedev->ctrl_resp_q->ctrl_resp_q_full++;
					TRAP_DUE_DMA_RESOURCES(("Cannot send msg type %d:"
						" Ring cmplt q full\n", msg_hdr->msg_type));
			}
			break;
		case MSG_TYPE_FLOW_RING_DELETE_CMPLT:
		case MSG_TYPE_FLOW_RING_FLUSH_CMPLT:
		case MSG_TYPE_D2H_MAILBOX_DATA:
			if (wr_space_avail <= (PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY +
				PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY)) {
					pciedev->ctrl_resp_q->ctrl_resp_q_full++;
					PCI_ERROR(("Cannot send msg type %d: Ring cmplt q full\n",
						msg_hdr->msg_type));
					return BCME_ERROR;
				}
			break;
		case MSG_TYPE_GEN_STATUS:
			if (wr_space_avail <= PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY) {
				pciedev->ctrl_resp_q->ctrl_resp_q_full++;
				TRAP_DUE_DMA_RESOURCES(("Cannot send msg type %d:"
					" Ring cmplt q full\n", msg_hdr->msg_type));
			}
			break;
		default:
			PCI_ERROR(("Wrong message type\n"));
			ASSERT(0);
			break;
	}
	/* Copy the info into a local queue */
	memcpy(&(resp_q->response[w_indx].ctrl_response),
		(void *)(ctrl_cmplt->ctrl_response), sizeof(ctrl_completion_item_t));
	pciedev_ctrl_resp_q_idx_upd(pciedev, PCIEDEV_CTRL_RESP_Q_WR_IDX);

	pciedev_queue_d2h_req_send(pciedev);

	return BCME_OK;
}

/** notify the host of a problem (generally on the 'control submit' message ring) */
static void
pciedev_send_general_status(struct dngl_bus *pciedev, uint16 ring_id, uint32 status, uint32 req_id)
{
	ctrl_completion_item_t ctrl_cmplt;

	/* Copy the message into the local queue
	 * and send after making sure resources are available
	 */
	memset(&ctrl_cmplt, 0, sizeof(ctrl_completion_item_t));
	ctrl_cmplt.pcie_gen_status.compl_hdr.status = status;
	ctrl_cmplt.pcie_gen_status.compl_hdr.flow_ring_id = ring_id;
	ctrl_cmplt.pcie_gen_status.cmn_hdr.msg_type = MSG_TYPE_GEN_STATUS;
	ctrl_cmplt.pcie_gen_status.cmn_hdr.request_id = req_id;

	/* If this fails due to lack of space in the queue,
	 * FW traps in pciedev_schedule_ctrl_cmplt.
	 */
	(void)pciedev_schedule_ctrl_cmplt(pciedev, &ctrl_cmplt);
}

/** notify the host of a problem (generally when host message could not be parsed) */
static void
pciedev_send_ring_status(struct dngl_bus *pciedev, uint16 w_idx, uint32 req_id, uint32 status,
	uint16 ring_id)
{
	ctrl_completion_item_t ctrl_cmplt;

	/* Copy the message into the local queue
	 * and send after making sure resources are available
	 */
	memset(&ctrl_cmplt, 0, sizeof(ctrl_completion_item_t));
	ctrl_cmplt.pcie_ring_status.write_idx = w_idx;
	ctrl_cmplt.pcie_ring_status.compl_hdr.status = status;
	ctrl_cmplt.pcie_ring_status.compl_hdr.flow_ring_id = ring_id;
	ctrl_cmplt.pcie_ring_status.cmn_hdr.msg_type = MSG_TYPE_RING_STATUS;
	ctrl_cmplt.pcie_ring_status.cmn_hdr.request_id = req_id;

	/* If this fails due to lack of space in the queue,
	 * FW traps in pciedev_schedule_ctrl_cmplt.
	 */
	(void)pciedev_schedule_ctrl_cmplt(pciedev, &ctrl_cmplt);
}

/** notify host that an IOTCL has been received in device memory */
static void
pciedev_send_ioctlack(struct dngl_bus *pciedev, uint16 status)
{
	ioctl_req_msg_t *ioct_rqst;
	ioctl_req_ack_msg_t *ioctl_ack;
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	uint8 w_indx = resp_q->w_indx;

	if (IS_RING_SPACE_FULL(resp_q->r_indx, resp_q->w_indx, resp_q->depth)) {
		/* This should NOT happen for IOCTL ACK
		 * for IOCTL ACK and Completion there is
		 * space reserved
		 */
		pciedev->ctrl_resp_q->ctrl_resp_q_full++;
		DBG_BUS_INC(pciedev, send_ioctl_ack_failed);
		TRAP_DUE_DMA_RESOURCES(("Cannot send ioctl_ack: Ring cmplt q full\n"));
	}

	ioct_rqst = (ioctl_req_msg_t*)PKTDATA(pciedev->osh, pciedev->ioctl_pktptr);
	/* Copy IOCTL ACK into the local ctrl_resp_q */
	ioctl_ack = (ioctl_req_ack_msg_t *)&(resp_q->response[w_indx].ioct_ack);
	pciedev_ctrl_resp_q_idx_upd(pciedev, PCIEDEV_CTRL_RESP_Q_WR_IDX);
	memset(ioctl_ack, 0, sizeof(ctrl_completion_item_t));

	/* Populate the content */
	ioctl_ack->cmn_hdr.msg_type = MSG_TYPE_IOCTLPTR_REQ_ACK;
	ioctl_ack->cmn_hdr.request_id = ioct_rqst->cmn_hdr.request_id;
	ioctl_ack->compl_hdr.flow_ring_id = BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT;
	ioctl_ack->compl_hdr.status = status;
	ioctl_ack->cmd = ioct_rqst->cmd;

	/* Send items from the ctrl_resp_q */
	pciedev_queue_d2h_req_send(pciedev);

	if (pciedev->ipc_data_supported) {
		/* Record time for the ioctl_ack - Not necessarily true anymore */
		pciedev->ipc_data->last_ioctl_ack_time = OSL_SYSUPTIME();
	}

	return;
}

static void
pciedev_send_ts_cmpl(struct dngl_bus *pciedev, uint16 status)
{
	host_timestamp_msg_t *ts_rqst;
	host_timestamp_msg_cpl_t *ts_cmpl;
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	uint8 w_indx = resp_q->w_indx;

	if (IS_RING_SPACE_FULL(resp_q->r_indx, resp_q->w_indx, resp_q->depth)) {
		pciedev->ctrl_resp_q->ctrl_resp_q_full++;
		DBG_BUS_INC(pciedev, pciedev_send_ts_cmpl);
		TRAP_DUE_DMA_RESOURCES(("Cannot send ts_cmpl: Ring cmplt q full\n"));
	}

	ts_rqst = (host_timestamp_msg_t *)PKTDATA(pciedev->osh, pciedev->ts_pktptr);
	/* Copy IOCTL ACK into the local ctrl_resp_q */
	ts_cmpl = (host_timestamp_msg_cpl_t *)&(resp_q->response[w_indx].host_ts_cpl);
	pciedev_ctrl_resp_q_idx_upd(pciedev, PCIEDEV_CTRL_RESP_Q_WR_IDX);
	memset(ts_cmpl, 0, sizeof(ctrl_completion_item_t));

	/* Populate the content */
	ts_cmpl->msg.msg_type = MSG_TYPE_HOSTTIMSTAMP_CMPLT;
	ts_cmpl->msg.request_id = ts_rqst->msg.request_id;
	ts_cmpl->cmplt.flow_ring_id = BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT;
	ts_cmpl->cmplt.status = status;
	ts_cmpl->xt_id = ts_rqst->xt_id;

	/* Send items from the ctrl_resp_q */
	pciedev_queue_d2h_req_send(pciedev);

	if (pciedev->ipc_data_supported) {
		/* Record time for the ts_cmpl - Not necessarily true anymore */
		pciedev->ipc_data->last_ts_cmpl_time = OSL_SYSUPTIME();
	}
	if (pciedev->ts_pktptr) {
		PKTFREE(pciedev->osh, pciedev->ts_pktptr, FALSE);
		pciedev->ts_pktptr = NULL;
	}
	if (IS_TIMESYNC_LOCK(pciedev)) {
		CLR_TIMESYNC_LOCK(pciedev);
		PCI_TRACE(("ts Lock Released!\n"));
	}
	else {
		PCI_ERROR(("Somehow ts lock is already released here?!\n"));
		DBG_BUS_INC(pciedev, pciedev_send_ts_cmpl);
	}

	return;
}

/** Schedule dma to pull down non inline ioctl request */
static void
pciedev_process_ioctlptr_rqst(struct dngl_bus *pciedev, void *p)
{
	ioctl_req_msg_t *ioct_rqst;
	uint16 buflen;
	void  *lclpkt;
	uint16 lclbuflen;
	uint8 *payloadptr;
	uint32 out_buf_len;
	uint32 max_ioctl_resp_len;

	/* ioct request info */
	ioct_rqst = (ioctl_req_msg_t*)p;

	/* Store the ioctl_work item of the last ioctl and time */
	if (pciedev->ipc_data_supported) {
		bcopy(p, &pciedev->ipc_data->last_ioctl_work_item, sizeof(ioctl_req_msg_t));
		pciedev->ipc_data->last_ioctl_proc_time = OSL_SYSUPTIME();
	}

	/* host buf info */
	buflen = ltoh16(ioct_rqst->input_buf_len);
	out_buf_len = ltoh16(ioct_rqst->output_buf_len);

	if (HOST_DMA_BUF_POOL_EMPTY(pciedev->ioctl_resp_pool)) {
		PCI_ERROR(("no host response IOCTL buffer available..so fail the request\n"));
		DBG_BUS_INC(pciedev, send_ioctl_ptr_failed);
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NO_IOCTLRESP_BUF, ioct_rqst->cmn_hdr.request_id);
		return;
	}
	if (buflen > (pktpool_max_pkt_bytes(pciedev->ioctl_req_pool) - H2DRING_CTRL_SUB_ITEMSIZE)) {
		DBG_BUS_INC(pciedev, send_ioctl_ptr_failed);
		PCI_ERROR(("no host response IOCTL buffer available..so fail the request\n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_BADOPTION, ioct_rqst->cmn_hdr.request_id);
		return;
	}

	lclpkt = pktpool_get(pciedev->ioctl_req_pool);
	if (lclpkt == NULL) {
		PCI_ERROR(("panic: Should never happen \n"));
		DBG_BUS_INC(pciedev, send_ioctl_ptr_failed);
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NOMEM, ioct_rqst->cmn_hdr.request_id);
		return;
	}
	lclbuflen = pktpool_max_pkt_bytes(pciedev->ioctl_req_pool);
	lclbuflen -= H2DRING_CTRL_SUB_ITEMSIZE;

	if (!pciedev->ioct_resp_buf) {
		/*
		 * If external buffer is not available for ioctl response, we will be using
		 * the lclpkt for response as well. In case requested response is more than
		 * what can be accommodated in lclpkt, trim down the request
		 */
		if (out_buf_len > lclbuflen) {
			PCI_INFORM(("host is asking for %d resp bytes,"
						"but buffer is only %d bytes\n",
						out_buf_len, pciedev->ioctl_resp_pool->ready->len));
			out_buf_len = lclbuflen;
			ioct_rqst->output_buf_len = htol16(out_buf_len);
		}
	}

#ifdef PCIEDEV_USE_EXT_BUF_FOR_IOCTL
	/*
	 * if PCIEDEV_USE_EXT_BUF_FOR_IOCTL is used, then the requested buffer size
	 * must fit into the malloced ioct_resp_buf, otherwise we'll overflow the buffer.
	 */
	if (out_buf_len > PCIEDEV_MAX_IOCTLRSP_BUF_SIZE) {
		out_buf_len = PCIEDEV_MAX_IOCTLRSP_BUF_SIZE;
		ioct_rqst->output_buf_len = htol16(out_buf_len);
	}
#endif // endif

	/* Checking the first on the list, if not big enough truncate the result */
	max_ioctl_resp_len = pciedev->ioctl_resp_pool->ready->len;
	if (pciedev->ioct_resp_buf)
		max_ioctl_resp_len =
			MIN(pciedev->ioctl_resp_pool->ready->len, pciedev->ioct_resp_buf_len);

	if (out_buf_len > max_ioctl_resp_len) {
		PCI_ERROR(("host is asking for %d resp bytes, but buffer is only %d bytes\n",
			out_buf_len, max_ioctl_resp_len));
		out_buf_len = max_ioctl_resp_len;
		ioct_rqst->output_buf_len = htol16(out_buf_len);
	}

	if (pciedev->ioctl_pktptr != NULL) {
		PCI_ERROR(("pciedev->ioctl_pktptr is already non NULL 0x%08x\n",
			(uint32)pciedev->ioctl_pktptr));
		PCI_ERROR(("pciedev->ioctl_pktptr != NULL \n"));
		DBG_BUS_INC(pciedev, send_ioctl_ptr_failed);
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
	SET_IOCTL_LOCK(pciedev);

	if (buflen == 0) {
		PCI_TRACE(("pciedev_process_ioctlptr_rqst: No payload to DMA for ioctl!\n"));
		SET_IOCTLREQ_PENDING(pciedev);
		pciedev_process_ioctl_pend(pciedev);
		DBG_BUS_INC(pciedev, send_ioctl_ptr_failed);
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
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		hnd_fetch_rqst(pciedev->ioctptr_fetch_rqst); /* enqueues request */
	}
#endif /* BCMPCIEDEV */
} /* pciedev_process_ioctlptr_rqst */

/** Schedule dma to pull down non inline Host Timestamp request */
static void
pciedev_process_ts_rqst(struct dngl_bus *pciedev, void *p)
{
	host_timestamp_msg_t *ts_rqst;
	uint16 buflen;
	void  *lclpkt;
	uint16 lclbuflen;
	uint8 *payloadptr;

	/* ts request info */
	ts_rqst = (host_timestamp_msg_t*)p;

	/* Store the ts_work item of the last ts and time */
	if (pciedev->ipc_data_supported) {
		bcopy(p, &pciedev->ipc_data->last_ts_work_item, sizeof(host_timestamp_msg_t));
		pciedev->ipc_data->last_ts_proc_time = OSL_SYSUPTIME();
	}

	/* host buf info */
	buflen = ltoh16(ts_rqst->input_data_len);
	if (buflen > (pktpool_max_pkt_bytes(pciedev->ts_req_pool) - H2DRING_CTRL_SUB_ITEMSIZE)) {
		PCI_ERROR(("no host Timestamp buffer available..so fail the request\n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_BADOPTION, ts_rqst->msg.request_id);
		return;
	}

	lclpkt = pktpool_get(pciedev->ts_req_pool);
	if (lclpkt == NULL) {
		PCI_ERROR(("panic: Should never happen \n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NOMEM, ts_rqst->msg.request_id);
		return;
	}
	lclbuflen = pktpool_max_pkt_bytes(pciedev->ts_req_pool);
	lclbuflen -= H2DRING_CTRL_SUB_ITEMSIZE;

	if (pciedev->ts_pktptr != NULL) {
		PCI_ERROR(("pciedev->ts_pktptr is already non NULL 0x%08x\n",
			(uint32)pciedev->ts_pktptr));
		PCI_ERROR(("pciedev->ts_pktptr != NULL \n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_BADOPTION, ts_rqst->msg.request_id);
		ASSERT(0);
		return;
	}
	pciedev->ts_pktptr = lclpkt;

	PKTSETLEN(pciedev->osh, lclpkt, lclbuflen + H2DRING_CTRL_SUB_ITEMSIZE);

	/* copy message header into the packet */
	bcopy(p, PKTDATA(pciedev->osh, lclpkt), H2DRING_CTRL_SUB_ITEMSIZE);

	SET_TIMESYNC_LOCK(pciedev);

	if (buflen == 0) {
		PCI_TRACE(("pciedev_process_tsptr_rqst: No payload to DMA for ts!\n"));
		/* TRAP */
		ASSERT(0);
		return;
	}
	/* Offset ts payload pointer by msglen */
	payloadptr = (uint8*)PKTDATA(pciedev->osh, lclpkt) + H2DRING_CTRL_SUB_ITEMSIZE;

	PHYSADDR64HISET(pciedev->tsptr_fetch_rqst->haddr,
		(uint32)ltoh32(ts_rqst->host_buf_addr.high));
	PHYSADDR64LOSET(pciedev->tsptr_fetch_rqst->haddr,
		(uint32) ltoh32(ts_rqst->host_buf_addr.low));

	pciedev->tsptr_fetch_rqst->size = buflen;
	pciedev->tsptr_fetch_rqst->dest = payloadptr;
	pciedev->tsptr_fetch_rqst->cb = pciedev_tsync_pyld_fetch_cb;
	pciedev->tsptr_fetch_rqst->ctx = (void *)pciedev;
#ifdef BCMPCIEDEV
	if (BCMPCIEDEV_ENAB()) {
		hnd_fetch_rqst(pciedev->tsptr_fetch_rqst);
	}
#endif /* BCMPCIEDEV */
}

int
pciedev_process_d2h_tsinfo(struct dngl_bus *pciedev, void *p)
{
	uint16 pktlen = 0;
	uint32 pktid = 0;
	uint8 ifidx = 0;
	uint8 seqnum_reset = 0;
	ret_buf_t haddr;
	host_dma_buf_t hostbuf;
	cmn_msg_hdr_t *msg_hdr;
	msgbuf_ring_t *ring = pciedev->dtoh_ctrlcpl;
	host_dma_buf_pool_t *pool = pciedev->ts_pool;

	if (!pciedev->timesync) {
		PCI_ERROR(("TimeSync Feature is not supported by host\n"));
		PKTFREE(pciedev->osh, p, TRUE);
		return BCME_ERROR;
	}
	/* extract out ifidx first */
	ifidx = PKTIFINDEX(pciedev->osh, p);

	msg_hdr = (cmn_msg_hdr_t *)PKTDATA(pciedev->osh, p);
	if (msg_hdr->flags & BCMPCIE_CMNHDR_FLAGS_TS_SEQNUM_INIT) {
		seqnum_reset = 0x1;
		msg_hdr->flags &= ~BCMPCIE_CMNHDR_FLAGS_TS_SEQNUM_INIT;
	}

	/* remove cmn header */
	PKTPULL(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
	pktlen = PKTLEN(pciedev->osh, p);

	if (pciedev_host_dma_buf_pool_get(pool, &hostbuf)) {
		haddr.low_addr = hostbuf.buf_addr.low_addr;
		haddr.high_addr = hostbuf.buf_addr.high_addr;
		pktid = hostbuf.pktid;
		BCMPCIE_IPC_HPA_TEST(pciedev, pktid,
			BCMPCIE_IPC_PATH_CONTROL, BCMPCIE_IPC_TRANS_RESPONSE);
	}
	else {
		DBG_BUS_INC(pciedev, pciedev_process_d2h_tsinfo);
		PCI_ERROR(("packet not avialable in the timesync pool...BAD......\n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NO_TS_EVENT_BUF, -1);
		/* this need to generate an error message to host saying no event buffers */
		PKTFREE(pciedev->osh, p, TRUE);
		return BCME_ERROR;
	}

	/* Schedule Firmware Timestamp Info payload transfer */
	PCI_TRACE(("event buf: pktid 0x%08x, pyld len %d\n", pktid,  pktlen));

	pciedev_tx_pyld(pciedev, p, &haddr, pktlen, MSG_TYPE_TS_EVENT_PYLD);

	/* Form firmware timestamp complete messge and transfer */
	return pciedev_xmit_fw_ts_cmplt(pciedev, pktid, pktlen, ifidx, ring, seqnum_reset);
}

/** Create wl event message and schedule dma */
int
pciedev_process_d2h_wlevent(struct dngl_bus *pciedev, void *p)
{
	uint16 pktlen = 0;
	uint32 pktid = 0;
	uint8 ifidx = 0;
	ret_buf_t haddr;
	host_dma_buf_t hostbuf;
	msgbuf_ring_t *ring = pciedev->dtoh_ctrlcpl;

	/* extract out ifidx first */
	ifidx = PKTIFINDEX(pciedev->osh, p);

	/* remove cmn header */
	PKTPULL(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
	pktlen = PKTLEN(pciedev->osh, p);

	if (pciedev_host_dma_buf_pool_get(pciedev->event_pool, &hostbuf)) {
		haddr.low_addr = hostbuf.buf_addr.low_addr;
		haddr.high_addr = hostbuf.buf_addr.high_addr;
		pktid = hostbuf.pktid;
		BCMPCIE_IPC_HPA_TEST(pciedev, pktid,
			BCMPCIE_IPC_PATH_CONTROL, BCMPCIE_IPC_TRANS_RESPONSE);
	} else {
		PCI_ERROR(("packet not avialable in the event pool...BAD......\n"));
		pciedev_send_general_status(pciedev, BCMPCIE_H2D_MSGRING_CONTROL_SUBMIT,
			BCMPCIE_NO_EVENT_BUF, -1);
		/* this need to generate an error message to host saying no event buffers */
		PKTFREE(pciedev->osh, p, TRUE);
		return BCME_ERROR;
	}

	/* Make sure the pktlen fit into the host buffer */
	ASSERT(pktlen <= hostbuf.len);

	/* Schedule wlevent payload transfer */
	pciedev_tx_pyld(pciedev, p, &haddr, pktlen, MSG_TYPE_EVENT_PYLD);

	/* Form wl event complete message and transfer */
	return pciedev_xmit_wlevnt_cmplt(pciedev, pktid, pktlen, ifidx, ring);
}

/** Send wl event completion message to host */
static int
pciedev_xmit_wlevnt_cmplt(struct dngl_bus *pciedev, uint32 pktid, uint16 buflen,
	uint8 ifidx, msgbuf_ring_t *ring)
{
	wlevent_req_msg_t * evnt;

	/* reserve the space in circular buffer */
	evnt  = pciedev_lclpool_alloc_lclbuf(ring);
	if (evnt == NULL) {
		DBG_BUS_INC(pciedev, xmit_wlevnt_cmplt_failed);
		PCI_ERROR(("pciedev_xmit_wlevnt_cmplt:"
			"No space in DTOH ctrl ring to send event for ifidx: %d\n",
			ifidx));
		ASSERT(0);
		return BCME_ERROR;
	}

	/* Cmn header */
	evnt->cmn_hdr.msg_type = MSG_TYPE_WL_EVENT;
	evnt->cmn_hdr.request_id = htol32(pktid);
	BCMMSGBUF_SET_API_IFIDX(&evnt->cmn_hdr, ifidx);

	evnt->compl_hdr.status = 0;
	evnt->compl_hdr.flow_ring_id = BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE;

	evnt->event_data_len = htol16(buflen);
	evnt->seqnum = htol16(pciedev->event_seqnum);
	++pciedev->event_seqnum;

	/* schedule wlevent complete message transfer */
	if (!pciedev_xmit_msgbuf_packet(pciedev, evnt,
	        MSG_TYPE_WL_EVENT, MSGBUF_LEN(ring), ring))
	{
		DBG_BUS_INC(pciedev, xmit_wlevnt_cmplt_failed);
		PCI_ERROR(("pciedev_xmit_wlevnt_cmplt: "
			"failed to transmit  wlevent complete message"));
		return BCME_ERROR;
	} else {
		return BCME_OK;
	}
}

/** Send firmware timestamp completion message to host */
static int
pciedev_xmit_fw_ts_cmplt(struct dngl_bus *pciedev, uint32 pktid, uint16 buflen,
	uint8 ifidx, msgbuf_ring_t *ring, uint8 flags)
{
	fw_timestamp_event_msg_t *ts_evnt;

	/* reserve the space in circular buffer */
	ts_evnt  = pciedev_lclpool_alloc_lclbuf(ring);
	if (ts_evnt == NULL) {
		PCI_ERROR(("pciedev_xmit_fw_ts_cmplt:"
			"No space in DTOH ctrl ring to send Firmware Timestamp for ifidx: %d\n",
			ifidx));
		ASSERT(0);
		return BCME_ERROR;
	}

	PCI_TRACE(("%s\n", __FUNCTION__));
	/* Cmn header */
	ts_evnt->msg.msg_type = MSG_TYPE_FIRMWARE_TIMESTAMP;
	ts_evnt->msg.request_id = htol32(pktid);
	BCMMSGBUF_SET_API_IFIDX(&ts_evnt->msg, ifidx);

	ts_evnt->cmplt.status = 0;
	ts_evnt->cmplt.flow_ring_id = BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE;

	if (flags == 0x1) {
		pciedev->ts_seqnum = 0;
	}

	++pciedev->ts_seqnum;
	ts_evnt->buf_len = htol16(buflen);
	ts_evnt->seqnum = htol16(pciedev->ts_seqnum);
	/* schedule Firmware Timestamp Info complete message transfer */
	if (!pciedev_xmit_msgbuf_packet(pciedev, ts_evnt,
	        MSG_TYPE_FIRMWARE_TIMESTAMP, MSGBUF_LEN(ring), ring))
	{
		PCI_PRINT(("pciedev_xmit_fw_ts_cmplt: "
			"failed to transmit  Firmware Timestamp complete message"));
		return BCME_ERROR;
	}
	else
		return BCME_OK;
}

/** routine to send the LTR messages */
void
pciedev_send_ltr(void *dev, uint8 state)
{
	uint32  time_now = OSL_SYSUPTIME();
	struct dngl_bus *pciedev = (struct dngl_bus *)dev;

	if (pciedev->cur_ltr_state != state) {
		if (state == LTR_SLEEP) {
			pciedev_crwlpciegen2_161(pciedev, FALSE);
			/* Update LTR stat */
			++pciedev->metrics->ltr_sleep_ct;
			pciedev->metrics->ltr_active_dur += time_now -
				pciedev->metric_ref->ltr_active;
			pciedev->metric_ref->ltr_sleep = time_now;
			pciedev->metric_ref->ltr_active = 0;
			/* If deepsleep is enabled, restart the healthcheck counters
			* as LTR is not active
			*/
			if (HND_DS_HC_ENAB() &&
				!pciedev->deepsleep_disable_state) {
				pciedev_ds_healthcheck_cntr_update(pciedev, time_now, FALSE);
			}
		}
		W_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state, (uint32)state);
		if (state == LTR_ACTIVE) {
			SPINWAIT(((R_REG(pciedev->osh,
				&pciedev->regs->u.pcie2.ltr_state) &
				LTR_FINAL_MASK) >> LTR_FINAL_SHIFT) != LTR_ACTIVE, 100);
			if (((R_REG(pciedev->osh,
				&pciedev->regs->u.pcie2.ltr_state) &
				LTR_FINAL_MASK) >> LTR_FINAL_SHIFT) != LTR_ACTIVE) {
				PCI_ERROR(("pciedev_send_ltr:Giving up:0x%x\n",
					R_REG(pciedev->osh, &pciedev->regs->u.pcie2.ltr_state)));
			} else {
				pciedev_crwlpciegen2_161(pciedev, TRUE);
			}
			/* If deepsleep is enabled, reset the healthcheck counters
			* as LTR is active
			*/
			if (HND_DS_HC_ENAB() &&
				!pciedev->deepsleep_disable_state) {
				pciedev_ds_healthcheck_cntr_update(pciedev, time_now, TRUE);
			}
			++pciedev->metrics->ltr_active_ct;
			pciedev->metrics->ltr_sleep_dur += time_now -
				pciedev->metric_ref->ltr_sleep;
			pciedev->metric_ref->ltr_active = time_now;
			pciedev->metric_ref->ltr_sleep = 0;
		}
		pciedev->cur_ltr_state  = state;
	}
}

void
pciedev_send_d2h_mb_data_ctrlmsg(struct dngl_bus *pciedev)
{
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	ctrl_completion_item_t ctrl_cmplt;

	if (!IS_MBDATA_PENDING(pciedev) || (pciedev->d2h_mb_data == 0)) {
		if (IS_MBDATA_PENDING(pciedev) || (pciedev->d2h_mb_data)) {
			PCI_ERROR(("data intcosistancy %d, %d\n", IS_MBDATA_PENDING(pciedev),
				pciedev->d2h_mb_data));
			ASSERT(0);
		}
		return;
	}

	if (IS_RING_SPACE_FULL(resp_q->r_indx, resp_q->w_indx, resp_q->depth)) {
		return;
	}

	memset(&ctrl_cmplt, 0, sizeof(ctrl_completion_item_t));

	ctrl_cmplt.pcie_ring_status.cmn_hdr.msg_type = MSG_TYPE_D2H_MAILBOX_DATA;
	ctrl_cmplt.d2h_mailbox_data.d2h_mailbox_data = pciedev->d2h_mb_data;

	CLR_MBDATA_PENDING(pciedev);
	pciedev->d2h_mb_data = 0;
	if (pciedev_schedule_ctrl_cmplt(pciedev, &ctrl_cmplt) != BCME_OK) {
		pciedev->d2h_mb_data = ctrl_cmplt.d2h_mailbox_data.d2h_mailbox_data;
		SET_MBDATA_PENDING(pciedev);
		return;
	}
}

#ifdef PCIE_DEEP_SLEEP
void
pciedev_d2h_mbdata_clear(struct dngl_bus *pciedev, uint32 mb_data)
{
	uint32 mb_data_read;

	if (pciedev->d2h_mb_data_ptr) {
		mb_data_read = *pciedev->d2h_mb_data_ptr;
		mb_data_read  &= ~mb_data;
		*pciedev->d2h_mb_data_ptr = mb_data_read;
	}
}

#if !defined(PCIEDEV_INBAND_DW)
static void
pciedev_disable_ds_state_machine(struct dngl_bus *pciedev)
{
	pciedev_disable_deepsleep(pciedev, FALSE);
	pciedev->ds_state = DS_DISABLED_STATE;
	pciedev_manage_TREFUP_based_on_deepsleep(pciedev, TRUE);
}
#endif /* !defined(PCIEDEV_INBAND_DW) */

void
pciedev_disable_deepsleep(struct dngl_bus *pciedev, bool disable)
{
#ifdef HEALTH_CHECK
	uint32	time_now = OSL_SYSUPTIME();
#endif /* HEALTH_CHECK */
	pciedev->deepsleep_disable_state = disable;

#if defined(BCMPCIE_IDMA)
	if ((PCIE_IDMA_ACTIVE(pciedev) && PCIE_IDMA_DS(pciedev)) && !disable) {
			pciedev_idma_channel_enable(pciedev, disable);
	}
#endif /* BCMPCIE_IDMA */

#ifdef PCIEDEV_INBAND_DW
	if (!disable && pciedev->ds_inband_dw_supported) {
			pciedev_manage_TREFUP_based_on_deepsleep(pciedev, !disable);
	}
#endif /* PCIEDEV_INBAND_DW */
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__)
	/* this could be done in different ways for now using the ARM to control it */
	pciedev->ds_clk_ctl_st = si_arm_disable_deepsleep(pciedev->sih, disable);
#endif /* defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__) */
#if defined(UART_TRAP_DBG) && defined(SECI_UART)
#ifdef PCIEDEV_INBAND_DW
	if (pciedev->ds_inband_dw_supported) {
		/* Need to be in active to use RX UART */
		si_seci_clk_force(pciedev->sih, disable);
	}
#endif /* PCIEDEV_INBAND_DW */
#endif /* UART_TRAP_DBG && SECI_UART */
#ifdef PCIEDEV_INBAND_DW
	if (disable && pciedev->ds_inband_dw_supported) {
		pciedev_manage_TREFUP_based_on_deepsleep(pciedev, !disable);
	}
#endif /* PCIEDEV_INBAND_DW */
	if (HND_DS_HC_ENAB()) {
		if (disable) {
			if (pciedev->hc_params &&
				!pciedev->hc_params->last_ds_interval) {
				pciedev_ds_healthcheck_cntr_update(pciedev, time_now, TRUE);
			}
		}
		else {
			pciedev_ds_healthcheck_cntr_update(pciedev, time_now, FALSE);
			if (pciedev->cur_ltr_state == LTR_ACTIVE) {
				/* If LTR is active, reset the counters */
				pciedev_ds_healthcheck_cntr_update(pciedev, time_now, TRUE);
			}
		}
	}
}

#endif /* PCIE_DEEP_SLEEP */

/** dongle notifies the host of D3 ack, deep sleep, fw halt */
void
pciedev_d2h_mbdata_send(struct dngl_bus *pciedev, uint32 data)
{
	uint32 d2h_mb_data;
	uint32 ts = OSL_SYSUPTIME();

	if (data & PCIE_IPC_D2HMB_DEV_FWHALT)
	{
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_D2H_MB_FWHALT, ts);
	} else {
		switch (data) {
			case PCIE_IPC_D2HMB_DEV_DS_EXIT_NOTE:
				IPC_LOG(pciedev, MAILBOX, IPC_LOG_D2H_MB_DS_EXIT, ts);
			break;
			case PCIE_IPC_D2HMB_DEV_DS_ENTER_REQ:
				IPC_LOG(pciedev, MAILBOX, IPC_LOG_D2H_MB_DS_REQ, ts);
			break;
			case PCIE_IPC_D2HMB_DEV_D3_ACK:
				IPC_LOG(pciedev, MAILBOX, IPC_LOG_D2H_MB_D3_REQ_ACK, ts);
			break;
			break;
			default:
				PCI_ERROR(("%s:Unknown d2h_mb_data:0x%x\n", __FUNCTION__, data));
			break;
		}
	}

	if (pciedev->d2h_mb_data_ptr) {
		d2h_mb_data = *pciedev->d2h_mb_data_ptr;
		if (d2h_mb_data != 0) {
			uint32 i = 0;
			PCI_ERROR(("GRRRRRRR: MB transaction is already pending 0x%04x\n",
				d2h_mb_data));
			/* start a zero length timer to keep checking this to be zero */
			while ((i++ < 100) && d2h_mb_data) {
				OSL_DELAY(10);
				d2h_mb_data = *pciedev->d2h_mb_data_ptr;
			}
			if (i >= 100) {
				PCI_ERROR(("waited 1ms for host to ack the previous mb "
					"transaction 0x%04x\n", d2h_mb_data));
			}
		}
		d2h_mb_data |= data;
		*pciedev->d2h_mb_data_ptr = d2h_mb_data;

		/* memory barrier before generating host mb intr. */
		DMB();

		if (!pciedev->d2h_intr_ignore)
			pciedev_generate_host_mb_intr(pciedev);
		else
			pciedev->d2h_intr_ignore = FALSE;
	}
}

/** host notifying dongle of various low-level events */
static void
pciedev_h2d_mb_data_check(struct dngl_bus *pciedev)
{
	uint32 h2d_mb_data;

	if (pciedev->h2d_mb_data_ptr == 0)
		return;

	h2d_mb_data = *pciedev->h2d_mb_data_ptr;

	PCI_TRACE(("pciedev_h2d_mb_data_check: h2d_mb_data=0x%04x\n", h2d_mb_data));

	if (!h2d_mb_data) {
		return;
	}

	/* ack the message first */
	*pciedev->h2d_mb_data_ptr = 0U;

	pciedev_h2d_mb_data_process(pciedev, h2d_mb_data);

	if (!HOSTREADY_ONLY_ENAB()) {
		/* For backward compatibility, allow both DB1 and MB based interrupts
		 * at the beginning and then select based on what host is using at the
		 * first time.
		 */
		if (pciedev->db1_for_mb == PCIE_BOTH_MB_DB1_INTR) {
			if (pciedev->intstatus & PD_DEV0_DB1_INTMASK) {
				pciedev->db1_for_mb = PCIE_DB1_INTR_ONLY;
				pciedev->mb_intmask = PD_DEV0_DB1_INTMASK;
			}
			else if (pciedev->intstatus & PD_FUNC0_MB_INTMASK) {
				pciedev->db1_for_mb = PCIE_MB_INTR_ONLY;
				pciedev->mb_intmask = PD_FUNC0_MB_INTMASK;
			}
			pciedev->defintmask &= ~(PD_DEV0_DB1_INTMASK | PD_FUNC0_MB_INTMASK);
			pciedev->defintmask |= pciedev->mb_intmask;
			pciedev->intmask = pciedev->defintmask;
		}
	}
}

static void
pciedev_h2d_mb_data_process(struct dngl_bus *pciedev, uint32 h2d_mb_data)
{
	uint32 ts = OSL_SYSUPTIME();
	/* Store the time mb / db1 is received */
	if (pciedev->ipc_data_supported)
		pciedev->ipc_data->last_h2d_mbdb1_time = ts;

	/* if it is an ack for a request clear the request bits in the d2h_mb_data */
		if (h2d_mb_data & PCIE_IPC_H2DMB_DS_HOST_SLEEP_INFORM) {
		/* check if host configured to ignore D2H ACK interrupt */
		if (h2d_mb_data & PCIE_IPC_H2DMB_HOST_ACK_NOINT)
			pciedev->d2h_intr_ignore = TRUE;
		else
			pciedev->d2h_intr_ignore = FALSE;

#ifdef PCIE_PWRMGMT_CHECK
			/*
			* XXX: JIRA:SWWLAN-39360, handle D0 interrupt miss
			* If the host did not do doorbell coming out of D3cold, instead doing
			* another D3, this code should handle it.
			*/
		if (!pciedev->bm_enabled || pciedev->ltr_sleep_after_d0) {
			if (!pciedev->bm_enabled) {
				if (pciedev->hostready) {
					PCI_ERROR(("D3-INFORM without Hostready!\n"));
					OSL_SYS_HALT();
				}
				PCI_PRINT(("D3-INFORM: d0 missed\n"));
				pciedev->bus_counters->d0_miss_counter ++;
			}
			pciedev_handle_d0_enter_bm(pciedev);
			/*
			* Make sure to disable the bm timer since we are
			* entering in D3cold.
			*/
			if (pciedev->bm_enab_timer_on) {
				dngl_del_timer(pciedev->bm_enab_timer);
				pciedev->bm_enab_timer_on = FALSE;
			}
			pciedev_enable_powerstate_interrupts(pciedev);
		}
#endif /* PCIE_PWRMGMT_CHECK */
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_D3_INFORM, ts);
#ifdef PCIEDEV_INBAND_DW
#ifdef PCIE_DEEP_SLEEP
		if (pciedev->ds_inband_dw_supported) {
			/* set before host sleep to prevent access during D3 */
			pciedev_manage_TREFUP_based_on_deepsleep(pciedev,
				PCIEDEV_DEEPSLEEP_DISABLED);
			pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_HOST_SLEEP_INFORM);
		} else
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
		{
			pciedev_handle_host_D3_info(pciedev);
		}
	}

	/* Making rom friendly by introducing wrapper function */
	pciedev_handle_host_deepsleep_mbdata(pciedev, h2d_mb_data);

	/* Host to dongle console cmd interrupt */
	if (h2d_mb_data & PCIE_IPC_H2DMB_HOST_CONS_INT) {
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_CONS_INT, ts);
		hnd_cons_check();
	}
	/* Host to dongle force TRAP */
	if (h2d_mb_data & PCIE_IPC_H2DMB_FW_TRAP) {
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_FW_TRAP, ts);
		PCI_PRINT(("Forced TRAP via mailbox!\n"));
		traptest();
	}

} /* pciedev_h2d_mb_data_process */

/**
 * Called when host informed firmware that it desires to transition the dongle to the d3 power
 * state.
 */
static void
pciedev_handle_host_D3_info(struct dngl_bus *pciedev)
{
	/* While entering D3, resetting back timer stats */
	pciedev->bus_counters->d3_info_duration = 0;
	pciedev->bus_counters->hostwake_assert_last = 0;
	pciedev->bus_counters->hostwake_deassert_last = 0;
	pciedev->bus_counters->d3_info_start_time = OSL_SYSUPTIME();
	pciedev_send_ltr(pciedev, LTR_ACTIVE);
	if (PCIECOREREV(pciedev->corerev) < 14) {
		pciedev->ltr_sleep_after_d0 = TRUE;
	}

	pciedev->bus_counters->d3_info_enter_count++;
	pciedev_notify_devpwrstchg(pciedev, FALSE);
	/* Use Tpoweron +5 value during D3 entry */
	pciedev_crwlpciegen2_161(pciedev, FALSE);

	pciedev_manage_TREFUP_based_on_deepsleep(pciedev, PCIEDEV_DEEPSLEEP_ENABLED);

	pciedev->in_d3_suspend = TRUE;
	pciedev->d3_ack_pending = TRUE;
	if (pciedev->ds_check_timer_on) {
		dngl_del_timer(pciedev->ds_check_timer);
		pciedev->ds_check_timer_on = FALSE;
		pciedev->ds_check_timer_max = 0;
	}

	/* enable the interrupt to get the notfication of entering D3 */
	pciedev->device_link_state |= PCIEGEN2_PWRINT_D3_STATE_MASK;
	pciedev->device_link_state &= ~PCIEGEN2_PWRINT_D0_STATE_MASK;
	pciedev_enable_powerstate_interrupts(pciedev);

	/* Reset the Timesync sequence counter */
	pciedev->ts_seqnum = 0;

	/* make sure all the pending are handled already */
	pciedev_D3_ack(pciedev);
}

/**
 * Called when host informed firmware that it desires to transition the dongle to the d3 power
 * state. This function posts a d3-ack message back to the host.
 */
static void
pciedev_D3_ack(struct dngl_bus *pciedev)
{
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	uint32 d3info_duration;
	uint8 i;

	PCI_TRACE(("pciedev_D3_ack\n"));

	if (!pciedev->in_d3_suspend) {
		return;
	}

	if (!pciedev->d3_ack_pending) {
		return;
	}

	/* Make sure all txstatus due to MAC DMA FLUSH are taken care of */
	if (PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev)) {
		PCI_PRINT(("RX: pend count is %d\n", PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev)));
		PCIEDEV_XMIT_RXCOMPLETE(pciedev);
	}
	if (PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev)) {
		PCI_PRINT(("TX: pend count is %d\n", PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev)));
		PCIEDEV_XMIT_TXSTATUS(pciedev);
	}

	for (i = 0; i < MAX_DMA_CHAN; i++) {
		if (VALID_DMA_CHAN_NO_IDMA(i, pciedev) &&
			pciedev->h2d_di[i] && pciedev->d2h_di[i]) {

			/* TBD : In case it needs to protect Implicit DMA as well,
			 * one way is to add implicit dma status flag
			 * which is set by Host and cleared by Dev
			 */
			if (dma_txactive(pciedev->h2d_di[i]) ||
				dma_rxactive(pciedev->h2d_di[i]) ||
				dma_txactive(pciedev->d2h_di[i]) ||
				dma_rxactive(pciedev->d2h_di[i]))
			{
				PCI_ERROR(("M2M DMA H2D_TX %d, RX %d, D2H_TX: %d, RX: %d\n",
						dma_txactive(pciedev->h2d_di[i]),
						dma_rxactive(pciedev->h2d_di[i]),
						dma_txactive(pciedev->d2h_di[i]),
						dma_rxactive(pciedev->d2h_di[i])));
				return;
			}
		}
	}

#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ACTIVE(pciedev)) {
		if (pciedev->idma_dmaxfer_active) {
			PCI_ERROR(("M2M H2D Implicit DMA is active\n"));
			return;
		}
	}
#endif /* BCMPCIE_IDMA */

	/* If host did not read the mailbox data yet and D3Ack timeout is
	* not imminent, let's wait and poll for mailbox data to be cleared
	* by the host. Only then send the D3Ack.
	*/
	d3info_duration = OSL_SYSUPTIME() - pciedev->bus_counters->d3_info_start_time;
	if ((pciedev->d2h_mb_data_ptr && *pciedev->d2h_mb_data_ptr) &&
		(d3info_duration < (PCIEDEV_HOST_SLEEP_ACK_TIMEOUT - 10))) {
		if (!pciedev->host_sleep_ack_timer_on) {
			dngl_add_timer(pciedev->host_sleep_ack_timer,
				pciedev->host_sleep_ack_timer_interval, FALSE);
			pciedev->host_sleep_ack_timer_on = TRUE;
		}
		return;
	}

	PCI_TRACE(("TIME:%u pciedev_D3_ack acking the D3 req\n", hnd_time()));

	/* Make sure control resp q is flushed before posting D3-ACK */
	if (READ_AVAIL_SPACE(resp_q->w_indx, resp_q->r_indx, resp_q->depth)) {
		if (pciedev_process_ctrl_cmplt(pciedev) != BCME_OK) {
			PCI_ERROR(("Could not flush control resp q:%d\n",
				READ_AVAIL_SPACE(resp_q->w_indx, resp_q->r_indx, resp_q->depth)));
			return;
		}
	}
	if (READ_AVAIL_SPACE(resp_q->w_indx, resp_q->r_indx, resp_q->depth)) {
		PCI_ERROR(("control resp q still not empty:%d\n",
				READ_AVAIL_SPACE(resp_q->w_indx, resp_q->r_indx, resp_q->depth)));
		return;
	}

	pciedev_d2h_mbdata_send(pciedev, PCIE_IPC_D2HMB_DEV_D3_ACK);
	if (!pciedev->d2h_mb_data) {
		pciedev->d3_ack_pending = FALSE;
		pciedev->bus_counters->d3_ack_sent_count ++;
		pciedev->bus_counters->d3_info_duration =
			OSL_SYSUPTIME() - pciedev->bus_counters->d3_info_start_time;
		pciedev->bus_counters->d3_ack_srcount = pciedev->sr_count;

		/* Enable WOP for incoming PERST# */
		pciedev_WOP_disable(pciedev, FALSE);
#if defined(BCMPCIE_IDMA)
		if (PCIE_IDMA_ACTIVE(pciedev) && PCIE_IDMA_DS(pciedev)) {
			if (pciedev->ds_oob_dw_supported) {
				pciedev_idma_channel_enable(pciedev, FALSE);
			}
		}
#endif /* BCMPCIE_IDMA */
		pciedev->bm_enabled = FALSE;
	} else {
		PCI_PRINT(("couldn't send the D3 ack for some reason....\n"));
		TRAP_DUE_DMA_RESOURCES(("Cannot send D3 ACK: BAD\n"));
	}
}

static void
pciedev_handle_host_deepsleep_mbdata(struct dngl_bus *pciedev, uint32 h2d_mb_data)
{
#ifdef PCIE_DEEP_SLEEP
	uint32 ts = OSL_SYSUPTIME();

	if (h2d_mb_data & PCIE_IPC_H2DMB_DS_DEVICE_SLEEP_ACK) {
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported) {
			pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_DEVICE_SLEEP_ACK);
		} else
#endif /* PCIEDEV_INBAND_DW */
		{
			pciedev_handle_host_deepsleep_ack(pciedev);
		}
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_DS_ACK, ts);
	}
	if (h2d_mb_data & PCIE_IPC_H2DMB_DS_DEVICE_SLEEP_NAK) {
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported) {
			TRAP_IN_PCIEDRV(("H2DMB_DS_DEVICE_SLEEP_NAK not supported"));
		}
		else
#endif /* PCIEDEV_INBAND_DW */
		{
			pciedev_handle_host_deepsleep_nak(pciedev);
		}
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_DS_NACK, ts);
	}
	if (h2d_mb_data & PCIE_IPC_H2DMB_DS_ACTIVE) {
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported) {
			pciedev->num_ds_inband_dw_deassert ++;
			pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_ACTIVE);
		}
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_DS_ACTIVE, ts);
#endif /* PCIEDEV_INBAND_DW */
	}

	if (h2d_mb_data & PCIE_IPC_H2DMB_DS_DEVICE_WAKE) {
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported) {
			pciedev->num_ds_inband_dw_assert ++;
			pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_DEVICE_WAKE);
		}
		IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_DS_DWAKE, ts);
#endif /* PCIEDEV_INBAND_DW */
	}
#endif /* PCIE_DEEP_SLEEP */
}

#if defined(BCMPCIE_IDMA)
void
pciedev_handle_deepsleep_dw_db(struct dngl_bus *pciedev)
{
#if defined(PCIE_DEEP_SLEEP) && defined(PCIEDEV_INBAND_DW)
	uint32 ts = OSL_SYSUPTIME();

	if (pciedev->ds_inband_dw_supported) {
		pciedev->num_ds_inband_dw_assert ++;
		pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_DEVICE_WAKE);
	}
	IPC_LOG(pciedev, MAILBOX, IPC_LOG_H2D_MBDB1_DS_DWAKE, ts);
#endif /* PCIE_DEEP_SLEEP && PCIEDEV_INBAND_DW */
}
#endif /* BCMPCIE_IDMA */

/**  input expected is trefup in usec */
static void
pciedev_set_T_REF_UP(struct dngl_bus *pciedev, uint32 trefup)
{
	uint32 trefup_hi = 0, trefup_ext = PCI_PMCR_TREFUP_EXT_OFF, trefup_lo = 0;
	uint32 mask_refup = 0x3f0001e0;
	uint32 mask_refext = 0x400000;
	uint32 clk_freq_mhz = 25; /* 25 MHz */
	uint32 val = 0;

	if (pciedev->in_d3_suspend) {
		OSL_SYS_HALT();
	}

	trefup = trefup * clk_freq_mhz;

	if (trefup >= PCI_PMCR_TREFUP_MAX_SCALE) {
		PCI_ERROR(("pciedev_set_T_REF_UP:0x%x too high\n", trefup));
		OSL_SYS_HALT();
		return;
	}

	if (trefup >= PCI_PMCR_TREFUP_MAX) {
		trefup >>= PCI_PMCR_TREFUP_EXT_SCALE;
		trefup_ext = PCI_PMCR_TREFUP_EXT_ON;
	}
	trefup_lo = trefup & PCI_PMCR_TREFUP_LO_MASK;
	trefup_hi = trefup >> PCI_PMCR_TREFUP_LO_BITS;
	val = trefup_lo << PCI_PMCR_TREFUP_LO_SHIFT | trefup_hi << PCI_PMCR_TREFUP_HI_SHIFT;
	PCI_TRACE(("Tref:0x%x, hi:0x%x, lo:0x%x, ext:%d\n",
		trefup, trefup_hi, trefup_lo, trefup_ext));

	AND_REG_CFG(pciedev, PCI_PMCR_REFUP, ~mask_refup);
	OR_REG_CFG(pciedev, PCI_PMCR_REFUP, val);
	PCI_TRACE(("Reg:0x%x ::0x%x\n",
		PCI_PMCR_REFUP, R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs))));

	AND_REG_CFG(pciedev, PCI_PMCR_REFUP_EXT, ~mask_refext);
	OR_REG_CFG(pciedev, PCI_PMCR_REFUP_EXT, trefup_ext << PCI_PMCR_TREFUP_EXT_SHIFT);
	PCI_TRACE((":Reg:0x%x ::0x%x\n",
		PCI_PMCR_REFUP_EXT, R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs))));
}

/**
 * XXX: WAR added to support cases where the host configured t_power_on is small could
 * cause the device to not wake up on clock req, if there is race condition of
 * device entering deepsleep and the clk req comes at the same time
 * so the base WAR is to make sure the t power on has big enough value
 * this speciific change is to make sure the exit latency is lower for non deep sleep cases
 * and the suggestion was to modify the t_ref_up to 8/28us based on the deep sleep state machine
 */
void
pciedev_manage_TREFUP_based_on_deepsleep(struct dngl_bus *pciedev, bool deepsleep_enable)
{
#ifdef PCIE_DEEP_SLEEP
	uint32 trefupTime;

	if ((PCIECOREREV(pciedev->corerev) == 14) || (PCIECOREREV(pciedev->corerev) == 16) ||
		(PCIECOREREV(pciedev->corerev) == 17) || (PCIECOREREV(pciedev->corerev) == 21) ||
		((PCIECOREREV(pciedev->corerev) == 23) && !PCIE_FASTLPO_ENAB(pciedev))) {
		if (pciedev->in_d3_suspend)
			return;
		if (pciedev->ds_state != DS_DISABLED_STATE) {
			if (!si_arm_deepsleep_disabled(pciedev->sih)) {
				OSL_SYS_HALT();
			}

			if ((PCIECOREREV(pciedev->corerev) == 23) && !PCIE_FASTLPO_ENAB(pciedev)) {
				trefupTime = (deepsleep_enable == PCIEDEV_DEEPSLEEP_ENABLED) ?
					PCIE_PMCR_REFUP_T_DS_ENTER_REV19 :
					PCIE_PMCR_REFUP_T_DS_EXIT_REV19;
			} else {
				trefupTime = (deepsleep_enable == PCIEDEV_DEEPSLEEP_ENABLED) ?
					PCIE_PMCR_REFUP_T_DS_ENTER : PCIE_PMCR_REFUP_T_DS_EXIT;
			}
		} else {
			/* if no deep sleep protocol, force the T_REF_UP to be 28 */
			if ((PCIECOREREV(pciedev->corerev) == 23) && !PCIE_FASTLPO_ENAB(pciedev)) {
				trefupTime = PCIE_PMCR_REFUP_T_DS_ENTER_REV19;
			} else {
				trefupTime = PCIE_PMCR_REFUP_T_DS_ENTER;
			}
		}

		pciedev_set_T_REF_UP(pciedev, trefupTime);
	}
#endif /* PCIE_DEEP_SLEEP */
}

static void
pciedev_perst_deassert_wake_isr(uint32 status,  void *arg)
{
	/* Next D0/L0, DO_INFORM takes care of functional part */
	PCI_TRACE(("Interrupt on raising edge of perst\n"));
}

/** Enable chip wake up condition on pos edge of PERST */
static void
BCMATTACHFN(pciedev_enable_perst_deassert_wake)(struct dngl_bus *pciedev)
{
	uint8 gci_perst_gpio = CC_GCI_GPIO_15;
	uint8 perst_wake_mask;
	uint8 perst_cur_status;

	si_enable_perst_wake(pciedev->sih, &perst_wake_mask, &perst_cur_status);

	si_gci_gpioint_handler_register(pciedev->sih, gci_perst_gpio,
			perst_wake_mask, pciedev_perst_deassert_wake_isr, pciedev);
}

static uint8
BCMATTACHFN(pciedev_host_wake_gpio_init)(struct dngl_bus *pciedev)
{
	uint8 host_wake_gpio;

	host_wake_gpio = si_gci_host_wake_gpio_init(pciedev->sih);

	return host_wake_gpio;
}

void
pciedev_host_wake_gpio_enable(struct dngl_bus *pciedev, bool state)
{
	if (pciedev->host_wake_gpio != CC_GCI_GPIO_INVALID)
		si_gci_host_wake_gpio_enable(pciedev->sih, pciedev->host_wake_gpio, state);
	else
		pciedev_pme_hostwake(pciedev, state);

	if (state) {
		pciedev->bus_counters->hostwake_assert_count++;
		pciedev->bus_counters->hostwake_asserted = TRUE;
		pciedev->bus_counters->hostwake_assert_last = OSL_SYSUPTIME();
		PCI_ERROR(("Assert Hostwake Reason: %u\n", pciedev->bus_counters->hostwake_reason));
#ifdef PCIE_DEEP_SLEEP
		if (pciedev->ds_oob_dw_supported) {
			pciedev_ob_deepsleep_engine(pciedev, HOSTWAKE_ASSRT_EVENT);
		}
#endif /* PCIE_DEEP_SLEEP */
	}
}

#ifdef TIMESYNC_GPIO
static uint8
BCMATTACHFN(pciedev_time_sync_gpio_init)(struct dngl_bus *pciedev)
{
	uint8 time_sync_gpio;

	time_sync_gpio = si_gci_time_sync_gpio_init(pciedev->sih);

	return time_sync_gpio;
}

void
pciedev_time_sync_gpio_enable(struct dngl_bus *pciedev, bool state)
{
	if (pciedev->time_sync_gpio == CC_GCI_GPIO_INVALID) {
		PCI_ERROR(("Invalid time sync GPIO\n"));
		return;
	}
	si_gci_time_sync_gpio_enable(pciedev->sih, pciedev->time_sync_gpio, state);
	if (state) {
		pciedev->bus_counters->timesync_assert_count++;
		pciedev->timesync_asserted = TRUE;
		pciedev->bus_counters->timesync_assert_last = OSL_SYSUPTIME();
	} else {
		pciedev->bus_counters->timesync_deassert_count++;
		pciedev->timesync_asserted = FALSE;
		pciedev->bus_counters->timesync_deassert_last = OSL_SYSUPTIME();
	}
}
#endif /* TIMESYNC_GPIO */

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
	struct dngl_bus *pciedev = (struct dngl_bus *) hnd_timer_get_ctx(t);

	pciedev->bus_counters->hostwake_reason = PCIE_HOSTWAKE_REASON_DELAYED_WAKE;
	pciedev_host_wake_gpio_enable(pciedev, TRUE);
	pciedev->delayed_hostwake_timer_active = FALSE;
}

#endif /* PCIE_DELAYED_HOSTWAKE */

/**
 * PCIe core generated an interrupt because host asserted PERST#. Do a complete reset of PCIe core
 * and DMA, but not of the entire dongle hardware. Also re-write the enum space registers.
 * Only called for rev <= 13.
 */
static void
pciedev_perst_reset_process(struct dngl_bus *pciedev)
{
	uint8 i;

	PCI_TRACE(("TIME:%u pciedev_perst_reset_process \n", hnd_time()));

	if (R_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs)) & PCIE_RST) {
		PCI_ERROR(("pciedev_perst_reset_process:"
		"Delayed WOP CHIP wake programming Happened\n"));
	}
	pciedev_reset_pcie_interrupt(pciedev);
	pciedev_reset(pciedev);

	if (!si_iscoreup(pciedev->sih)) {
		TRAP_DUE_CORERESET_ERROR(("reset_process: failed to bring up PCIE core \n"));
		return;
	}

	OR_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), PCIE_SPERST);
	pciedev_init(pciedev);

	for (i = 0; i < MAX_DMA_CHAN; i++) {
		if (VALID_DMA_CHAN(i, pciedev)) {
			if (!dma_rxreset(pciedev->h2d_di[i])) {
				TRAP_DUE_DMA_ERROR(("dma_rxreset failed for h2d_di engine\n"));
				return;
			}
			if (!dma_txreset(pciedev->h2d_di[i])) {
				TRAP_DUE_DMA_ERROR(("dma_txreset failed for h2d_di engine\n"));
				return;
			}
			dma_rxinit(pciedev->h2d_di[i]);
			dma_txinit(pciedev->h2d_di[i]);

			if (VALID_DMA_D2H_CHAN(i, pciedev)) {
				if (!dma_rxreset(pciedev->d2h_di[i])) {
					TRAP_DUE_DMA_ERROR(("dma_rxreset failed for"
					"d2h_di engine\n"));
					return;
				}
				if (!dma_txreset(pciedev->d2h_di[i])) {
					TRAP_DUE_DMA_ERROR(("dma_txreset failed for"
					"d2h_di engine\n"));
					return;
				}
				dma_rxinit(pciedev->d2h_di[i]);
				dma_txinit(pciedev->d2h_di[i]);
			}
		}
	}

	 /* clear L0 interrupt status happen on pcie corereset */
	OR_REG(pciedev->osh,
		&pciedev->regs->u.pcie2.pwr_int_status, PCIEGEN2_PWRINT_L0_LINK_MASK);

	pciedev->device_link_state = PCIEGEN2_PWRINT_L0_LINK_MASK |
		PCIEGEN2_PWRINT_D0_STATE_MASK;

	pciedev_enable_powerstate_interrupts(pciedev);

	OR_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs), PCIE_SPERST);
} /* pciedev_perst_reset_process */

static void
pciedev_crwlpciegen2(struct dngl_bus *pciedev)
{
	if ((PCIECOREREV(pciedev->corerev) <= 13) || (PCIECOREREV(pciedev->corerev) == 14) ||
		(PCIECOREREV(pciedev->corerev) == 17) || (PCIECOREREV(pciedev->corerev) == 21)) {
		/* XXX JIRA:CRWLPCIEGEN2_162 Doorbell writes does not cause a pcie interrupt when
		 * link is in L1.2
		 */
		OR_REG(pciedev->osh,
			PCIE_pciecontrol(pciedev->autoregs), PCIE_DISABLE_L1CLK_GATING);

		if (PCIECOREREV(pciedev->corerev) < 13) {
			AND_REG_CFG(pciedev, PCIEGEN2_COE_PVT_TL_CTRL_0,
				~(1 << COE_PVT_TL_CTRL_0_PM_DIS_L1_REENTRY_BIT));
			PCI_TRACE(("coe pvt reg 0x%04x, value 0x%04x\n", PCIEGEN2_COE_PVT_TL_CTRL_0,
				R_REG(pciedev->osh, PCIE_configinddata(pciedev->autoregs))));
		}
	}
}

/**
 * XXX WAR for HW JIRA CRWLPCIEGEN2-178 (H2D DMA corruption after L2_3 (perst assertion)) RB:20983
 */
static void
pciedev_reg_pm_clk_period(struct dngl_bus *pciedev)
{
	uint32 alp_KHz, pm_value;

	/* set REG_PM_CLK_PERIOD to the right value */
	/* default value is not conservative enough..usually set to period of Xtal freq */
	/* HW folks want us to set this to half of that */
	/* (1000000 / xtalfreq_in_kHz) * 2 */
	if ((PCIECOREREV(pciedev->corerev) <= 14) ||
		(PCIECOREREV(pciedev->corerev) == 16) ||
		(PCIECOREREV(pciedev->corerev) == 17) ||
		(PCIECOREREV(pciedev->corerev) == 21)) {
		alp_KHz = (si_pmu_alp_clock(pciedev->sih, pciedev->osh) / 1000);
		pm_value =  (1000000 * 2) / alp_KHz;
		PCI_TRACE(("ALP in KHz is %d, cur PM_REG_VAL is %d, new PM_REG_VAL is %d\n",
			alp_KHz, R_REG_CFG(pciedev, PCIEGEN2_PVT_REG_PM_CLK_PERIOD),
			pm_value));
		W_REG_CFG(pciedev, PCIEGEN2_PVT_REG_PM_CLK_PERIOD, pm_value);
	}
}

/**
 * SW WAR implementation of CRWLPCIEGEN2-161. Not required for Gen2 PCIe cores.
 *
 * Read Tpoweron value programmed in PML1_sub_control2 (0x24c)
 * Before we enable L1.2 i.e. before firmware sends LTR sleep:
 * 	Set PMCR_TREFUP = Tpoweron + 5us
 * After we disable L1.2 i.e. after firmware sends LTR active:
 * 	Set PMCR_TREFUP = 6us (default value)
 *
 * The PMCR_TREFUP  counter is a 10-bit count and REG_PMCR_TREFUP_MAX_HI
 * is the high 4 bits and REG_PMCR_TREFUP_MAX_LO is the low 6-bits
 * of the count. REG_PMCR_TREFUP_EXTEND_MAX extends the count by 8.
 *
 * To obtain the count value, use the following formula:
 * Temp = ROUNDUP((time in usec) * 25) ### This is the count value
 * in 25Mhz clock period
 * If (Temp greater than or equal to 1024) then ### i.e. the count
 * 									is more than 10 bits
 * 	Set PMCR_TREFUP = ROUNDUP ((time in usec) * 25 / 8) and
 * 	Set REG_PMCR_TREFUP_EXTEND_MAX=1
 * Else  ## i.e. count is less than or equal to 10 bits
 * 	Set PMCR_TREFUP = ROUNDUP ((time in usec) * 25)
 * 	Set REG_PMCR_TREFUP_EXTEND_MAX=0
 *
 * For the second part of the WAR i.e. After we disable L1.2 i.e. after we send LTR active:
 * Set PMCR_TREFUP = 6us (default value)
 * Also set REG_PMCR_TREFUP_EXTEND_MAX=0
 */
void
pciedev_crwlpciegen2_161(struct dngl_bus *pciedev, bool state)
{
	uint32 tpoweron = 0, trefup = 0,
		tpoweron_usec = 0, tpoweron_scale, tpoweron_val;
	int scaletab[] = {2, 10, 100, -1};

	if (PCIECOREREV(pciedev->corerev) >= 13) {
		return;
	}

	if (pciedev_ds_in_host_sleep(pciedev)) {
		PCI_ERROR(("pciedev_crwlpciegen2_161: in D3, Cannot apply LTR\n"));
		return;
	}

	/* PCI_TRACE(("%s:%s\n", __FUNCTION__, state ? "LTR_ACTIVE":"LTR_SLEEP")); */
	if (state) {
		trefup = PMCR_TREFUP_DEFAULT;
	} else {
		tpoweron = R_REG_CFG(pciedev, PCI_L1SS_CTRL2);
		tpoweron_scale = tpoweron & PCI_TPOWER_SCALE_MASK;
		tpoweron_val = tpoweron >> PCI_TPOWER_SCALE_SHIFT;
		if ((tpoweron_scale > 3) || scaletab[tpoweron_scale] == -1) {
			PCI_ERROR(("pciedev_crwlpciegen2_161:tpoweron_scale:0x%x illegal\n",
				tpoweron_scale));
			return;
		}
		tpoweron_usec = (scaletab[tpoweron_scale] * tpoweron_val) +
			PMCR_TREFUP_TPOWERON_OFFSET;

		trefup = tpoweron_usec;
	}

	pciedev_set_T_REF_UP(pciedev, trefup);
} /* pciedev_crwlpciegen2_161 */

/**
 * SW WAR for CRWLPCIEGEN2-61 (Clkreq for Interrupts going towards PCIe are not gated by Interrupt
 * Mask). Fixed in core rev 14.
 */
static void
pciedev_crwlpciegen2_61(struct dngl_bus *pciedev)
{
	si_wrapperreg(pciedev->sih, AI_OOBSELINA74, ~0, 0);
	si_wrapperreg(pciedev->sih, AI_OOBSELINA30, ~0, 0);
}

#ifndef PCI_REG_PMCR_TCRPW_MAX_MASK
#define PCI_REG_PMCR_TCRPW_MAX_MASK  0x1F
#endif // endif

/**
 * SW WAR for CRWLPCIEGEN2-180 (max value of 'external CLKREQ deasserted minimum time' is not
 * large enough). Fixed in rev13.
 */
static void
pciedev_crwlpciegen2_180(struct dngl_bus *pciedev)
{
	if (PCIECOREREV(pciedev->corerev) == 13) {
		OR_REG_CFG(pciedev, PCI_PL_SPARE, PCI_CONFIG_EXT_CLK_MIN_TIME_MASK);

		PCI_TRACE(("Reg:0x%x ::0x%x\n",
			PCI_PL_SPARE, R_REG_CFG(pciedev, PCI_PL_SPARE)));
	} else if (PCIECOREREV(pciedev->corerev) >= 14) {
		/* XXX RB:41455. Olympic platform requires Tcrpw of 6usec. Carry out this
		 * programming to arrive at 6usec.
		 */
		uint r_val = 0;
		uint timer_sel = 0;
		uint ext_timer_mask = 0;
		uint tcrpw_val = 0xc;

		r_val = R_REG_CFG(pciedev, PCIEGEN2_PVT_REG_PM_CLK_PERIOD);
		timer_sel = (r_val & ~REG_PMCR_EXT_TIMER_SEL_MASK) |  REG_PMCR_EXT_TIMER_SEL_MASK;
		ext_timer_mask = r_val | (1 << REG_PMCR_EXT_TIMER_SHIFT);
		W_REG_CFG(pciedev, PCIEGEN2_PVT_REG_PM_CLK_PERIOD,
			(ext_timer_mask | timer_sel));

		PCI_TRACE(("Reg:0x%x ::0x%x\n",
			PCIEGEN2_PVT_REG_PM_CLK_PERIOD,
			R_REG_CFG(pciedev, PCIEGEN2_PVT_REG_PM_CLK_PERIOD)));

		r_val = R_REG_CFG(pciedev, PCI_PMCR_REFUP);
		W_REG_CFG(pciedev, PCI_PMCR_REFUP,
			((r_val & ~PCI_REG_PMCR_TCRPW_MAX_MASK) | tcrpw_val));

		PCI_TRACE(("Reg:0x%x ::0x%x\n",
			PCI_PMCR_REFUP, R_REG_CFG(pciedev, PCI_PMCR_REFUP)));
	} else  {
		OR_REG_CFG(pciedev, PCI_PMCR_REFUP, 0x1f);

		PCI_TRACE(("Reg:0x%x ::0x%x\n",
			PCI_PMCR_REFUP, R_REG_CFG(pciedev, PCI_PMCR_REFUP)));
	}
}

/**
 * Sends an 'ioctl completion' message to the host. At this point all the resources needed should be
 * available, this call can't fail..
 */
void
pciedev_send_ioctl_completion(struct dngl_bus *pciedev)
{
	host_dma_buf_t hostbuf;
	ioctl_comp_resp_msg_t *ioct_resp;
	uint16  msglen;
	ret_buf_t haddr;
	uint32 txdesc, rxdesc;
	void *p = pciedev->ioctl_cmplt_p;
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	uint8 w_indx = resp_q->w_indx;

	if ((p == NULL) || !IS_IOCTLRESP_PENDING(pciedev)) {
		PCI_INFORM(("pciedev_send_ioctl_completion: p is 0x%p, pending 0x%04x\n",
			p, pciedev->active_pending));
		return;
	}

	/* If IOCTL Completion needs payload to be DMA'ed
	 * Check for DMA Descriptors for the payload
	 */
	msglen = ((ioctl_comp_resp_msg_t *)PKTDATA(pciedev->osh, p))->resp_len;

	if (msglen != 0) {
		txdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, pciedev->default_dma_ch, DTOH, TXDESC);
		rxdesc = PCIEDEV_GET_AVAIL_DESC(pciedev, pciedev->default_dma_ch, DTOH, RXDESC);

		if ((txdesc < TXDESC_NEED_IOCTLPYLD) || (rxdesc < RXDESC_NEED_IOCTLPYLD)) {
			/* Cannot send payload now, so return */
			DBG_BUS_INC(pciedev, send_ioctl_completion_failed);
			PCI_TRACE(("Not enough DMA Descriptors!\n"));
			resp_q->dma_desc_lack_cnt++;
			return;
		}
	}

	if (!WRITE_SPACE_AVAIL(resp_q->r_indx, resp_q->w_indx, resp_q->depth)) {
		/* This should NOT happen for IOCTL COMPLETIONs
		 * For IOCTL ACK and COMPLETION there is
		 * space reserved
		 */
		pciedev->ctrl_resp_q->ctrl_resp_q_full++;
		DBG_BUS_INC(pciedev, send_ioctl_completion_failed);
		TRAP_DUE_DMA_RESOURCES(("Cannot send ioctl cmplt: Ring cmplt q full\n"));
		return;
	}

	ioct_resp = (ioctl_comp_resp_msg_t *)&(resp_q->response[w_indx].ioctl_resp);
	bzero(ioct_resp, sizeof(ctrl_completion_item_t));

	/* Separate out cmplt message and ioct response
	 * and copy the work-item into local queue
	 */
	bcopy(PKTDATA(pciedev->osh, p), (void *)ioct_resp, sizeof(ioctl_comp_resp_msg_t));
	PKTPULL(pciedev->osh, p, sizeof(ioctl_comp_resp_msg_t));

	ioct_resp->cmn_hdr.msg_type = MSG_TYPE_IOCTL_CMPLT;
	/* ioctl complete msgbuf is ready to be sent. But, we cannot send */
	/* it out till the ioctl payload has been scheduled for DMA. */
	if (pciedev_host_dma_buf_pool_get(pciedev->ioctl_resp_pool, &hostbuf) == FALSE) {
		TRAP_DUE_DMA_RESOURCES(("IOCTL resp pool not available\n"));
		return;
	}
	pciedev_ctrl_resp_q_idx_upd(pciedev, PCIEDEV_CTRL_RESP_Q_WR_IDX);
	BCMPCIE_IPC_HPA_TEST(pciedev, hostbuf.pktid,
		BCMPCIE_IPC_PATH_CONTROL, BCMPCIE_IPC_TRANS_RESPONSE);
	ioct_resp->cmn_hdr.request_id = hostbuf.pktid;
	if (msglen != 0) {
		haddr.low_addr = hostbuf.buf_addr.low_addr;
		haddr.high_addr = hostbuf.buf_addr.high_addr;
		msglen = MIN(msglen, hostbuf.len);
		if (ioct_resp->resp_len > msglen) {
			PCI_ERROR(("resp len %d, host buffer size %d\n",
				ioct_resp->resp_len, msglen));
			ioct_resp->resp_len = msglen;
			DBG_BUS_INC(pciedev, send_ioctl_completion_failed);
		}
		pciedev_tx_ioctl_pyld(pciedev, p, &haddr, msglen, MSG_TYPE_IOCT_PYLD);
	}
	/* Payload and completion are guaranteed to be sent */
	pciedev->ioctl_cmplt_p = NULL;
	CLR_IOCTLRESP_PENDING(pciedev);

	pciedev_queue_d2h_req_send(pciedev);

	if (pciedev->ipc_data_supported)
		pciedev->ipc_data->last_ioctl_compl_time = OSL_SYSUPTIME();

	return;
} /* pciedev_send_ioctl_completion */

void
pciedev_process_ioctl_pend(struct dngl_bus *pciedev)
{
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;

	/* resources needed are allocated host buffer for response,
	 * local buffer to send ack, local buffer to send ioctl response msg,
	 * work slots on host ctrl completion ring
	*/
	if (HOST_DMA_BUF_POOL_EMPTY(pciedev->ioctl_resp_pool)) {
		PCI_ERROR(("no host buffers for ioctl resp processing\n"));
		return;
	}

	/* if previous IOCTL completion waiting for resources, don't send ACK again */
	if (WRITE_SPACE_AVAIL(resp_q->r_indx, resp_q->w_indx, resp_q->depth) < 2) {
		PCI_ERROR(("should not happen\n"));
		return;
	}
	CLR_IOCTLREQ_PENDING(pciedev);
	pciedev_send_ioctlack(pciedev, BCMPCIE_SUCCESS);

	dngl_ctrldispatch(pciedev->dngl, pciedev->ioctl_pktptr, pciedev->ioct_resp_buf);
} /* pciedev_process_ioctl_pend */

void
pciedev_process_ts_pend(struct dngl_bus *pciedev)
{
	pciedev_ctrl_resp_q_t *resp_q = pciedev->ctrl_resp_q;
	host_timestamp_msg_t *ts_rqst;
	void *payloadptr;

	if (!pciedev->ts_pktptr) {
		PCI_ERROR(("should not happen. TRAP!!\n"));
		return;
	}
	/* if previous control completion waiting for resources, don't send ACK again */
	if (!WRITE_SPACE_AVAIL(resp_q->r_indx, resp_q->w_indx, resp_q->depth)) {
		PCI_ERROR(("should not happen\n"));
		return;
	}
	CLR_TSREQ_PENDING(pciedev);
	ts_rqst = (host_timestamp_msg_t *)PKTDATA(pciedev->osh, pciedev->ts_pktptr);
	payloadptr = PKTDATA(pciedev->osh, pciedev->ts_pktptr) + H2DRING_CTRL_SUB_ITEMSIZE;

	dngl_dev_ioctl(pciedev->dngl, RTEDEVTIMESYNC, payloadptr, ts_rqst->input_data_len);
	PCI_TRACE(("SEND TS COMPLETE\n"));
	pciedev_send_ts_cmpl(pciedev, BCMPCIE_SUCCESS);
}

void
pciedev_process_ioctl_done(struct dngl_bus *pciedev)
{
	if (pciedev->ioctl_lock) {
		pciedev->ioctl_lock = FALSE;
		CLR_IOCTL_LOCK(pciedev);
		PCI_TRACE(("pciedev_process_ioctl_done: Ioctl Lock Released!\n"));
	} else {
		PCI_ERROR(("pciedev_process_ioctl_done:"
			"Somehow ioctl lock is already released here?!\n"));
	}

	if (pciedev->ioctl_pktptr) {
		PKTFREE(pciedev->osh, pciedev->ioctl_pktptr, FALSE);
	}
	pciedev->ioctl_pktptr = NULL;
	CLR_IOCTLREQ_PENDING(pciedev);
	/* There might be msgs waiting in the msgbufs.
	 * Trigger processing.
	 */
	if (!pciedev->delayed_msgbuf_intr_timer_on) {
		pciedev->delayed_msgbuf_intr_timer_on = TRUE;
		dngl_add_timer(pciedev->delayed_msgbuf_intr_timer, 0, FALSE);
	}

}

/** RTOS requests us to fetch an ioctl from the host */
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
	SET_IOCTLREQ_PENDING(pciedev);

	pciedev_process_ioctl_pend(pciedev);
	return;
cleanup:
	pciedev_process_ioctl_done(pciedev);
	return;
}

static void
pciedev_tsync_pyld_fetch_cb(struct fetch_rqst *fr, bool cancelled)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)fr->ctx;

	/* If cancelled, retry: drop request for now
	 * Might need to retry sending it down HostFetch
	 */
	/* Reset the ioctl_fetch_rqst but do not free it */
	bzero(pciedev->tsptr_fetch_rqst, sizeof(struct fetch_rqst));

	if (cancelled) {
		PCI_ERROR(("pciedev_tsync_pyld_fetch_cb:"
			"Request cancelled! Dropping ts ptr rqst...\n"));
		PCIEDEV_PKTFETCH_CANCEL_CNT_INCR(pciedev);
		return;
	}

	if (pciedev->ts_pktptr == NULL) {
		PCI_ERROR(("pciedev_tsync_pyld_fetch_cb:"
			"Recieved ts payload, but header pkt ptr is NULL!\n"));
		ASSERT(0);
		return;
	}
	SET_TSREQ_PENDING(pciedev);

	pciedev_process_ts_pend(pciedev);
	return;
}

static int
pciedev_process_insert_flowring(struct dngl_bus * pciedev, flowring_pool_t * lcl_pool, uint8 ac_tid)
{
	prioring_pool_t * prioring;
	dll_t * prio;
	uint8 mapped_ac_tid, i;

	ASSERT(ac_tid < PCIE_MAX_TID_COUNT);
#if defined(WLSQS)
	/* convert to one resume stride circle */
	ac_tid = 0;
#endif // endif
	prioring = &pciedev->prioring[ac_tid];

	lcl_pool->prioring = prioring;
	if (pciedev->schedule_prio == PCIEDEV_SCHEDULER_AC_PRIO) {
		mapped_ac_tid = PCIEDEV_AC_PRIO_MAP(ac_tid);
	} else if (pciedev->schedule_prio == PCIEDEV_SCHEDULER_LLR_PRIO) {
		mapped_ac_tid = PCIEDEV_AC_PRIO_LLR_MAP(ac_tid);
	} else {
		mapped_ac_tid = PCIEDEV_TID_PRIO_MAP(ac_tid);
	}

	/* insert flow ring after last_fetch_node */
	dll_insert(&lcl_pool->node, prioring->last_fetch_node);
	prioring->last_fetch_node = &lcl_pool->node;

	/* insert prio ring to priority ring list based on its priority */
	if (!prioring->inited) {
		uint8 map_prio;
		/* loop through active prioring list */
		prio = dll_head_p(&pciedev->active_prioring_list);
		while (!dll_end(prio, &pciedev->active_prioring_list)) {
			if (pciedev->schedule_prio == PCIEDEV_SCHEDULER_AC_PRIO) {
				map_prio = PCIEDEV_AC_PRIO_MAP(((prioring_pool_t *)prio)->tid_ac);
			} else if (pciedev->schedule_prio == PCIEDEV_SCHEDULER_LLR_PRIO) {
				map_prio =
					PCIEDEV_AC_PRIO_LLR_MAP(((prioring_pool_t *)prio)->tid_ac);
			} else {
				map_prio = PCIEDEV_TID_PRIO_MAP(((prioring_pool_t *)prio)->tid_ac);
			}

			if (map_prio < mapped_ac_tid) {
				break;
			}
			/* get next priority ring  node */
			prio = dll_next_p(prio);
		}

		/* allocate maxpktcnt per ifindex */
		prioring->maxpktcnt =
			MALLOCZ(pciedev->osh, pciedev->max_slave_devs * sizeof(uint16));
		if (!prioring->maxpktcnt) {
			PCI_ERROR(("Malloc failed for prioring->maxpktcnt\n"));
			PCIEDEV_MALLOC_ERR_INCR(pciedev);
			ASSERT(0);
			return BCME_NOMEM;
		}
		/* Initialize maxpktcnt for each ifidx */
		for (i = 0; i < pciedev->max_slave_devs; i++) {
			prioring->maxpktcnt[i] = pciedev->tunables[MAXPKTFETCH];
		}

		prioring->inited = TRUE;

		prio = dll_prev_p(prio);
		dll_insert(&prioring->node, prio);
		pciedev->last_fetch_prio = &prioring->node;
	}

	return BCME_OK;
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
	if (dll_empty(&prioring->active_flowring_list)) {
		/* set new last fetch prio back to head */
		if (pciedev->last_fetch_prio == &prioring->node)
			pciedev->last_fetch_prio = &pciedev->active_prioring_list;

		dll_delete(&prioring->node);

		MFREE(pciedev->osh, prioring->maxpktcnt,
				pciedev->max_slave_devs * sizeof(uint16));

		prioring->last_fetch_node = &prioring->active_flowring_list;
		prioring->inited = FALSE;
	}

	/* reset flowring info in the node */
	lcl_pool->ring = NULL;
	lcl_pool->prioring = NULL;

	/* free up the node to free pool */
	dll_pool_free(pciedev->flowrings_dll_pool, lcl_pool);
}

/** Handle 'flow create' request (in 'p') from host */
static int
pciedev_process_flow_ring_create_rqst(struct dngl_bus * pciedev, void *p)
{
	int status = BCMPCIE_SUCCESS;
	int response;
	msgbuf_ring_t *flow_ring = NULL;
	tx_flowring_create_request_t *flow_ring_create_req;
	ctrl_completion_item_t ctrl_cmplt;	/**< to send the response to the host */
	uint16 flowid, max_items, item_size;
	uint8 item_type;
	char eabuf[ETHER_ADDR_STR_LEN];
	flowring_pool_t *lcl_pool;		/**< not to be confused with 'local buf' */

#ifdef BCMHWA
	HWA_TXPOST_EXPR(dma64addr_t haddr64);
#endif // endif

#ifdef MEM_ALLOC_STATS
	memuse_info_t mu;
	uint32 freemem_before, bytes_allocated;
	static uint32 max_mem_alloc = 0;

	hnd_get_heapuse(&mu);
	freemem_before = mu.arena_free;
#endif /* MEM_ALLOC_STATS */

	flow_ring_create_req = (tx_flowring_create_request_t *)p;

	flowid = ltoh16(flow_ring_create_req->flow_ring_id);
	max_items = ltoh16(flow_ring_create_req->max_items);
	item_size = ltoh16(flow_ring_create_req->len_item);
	item_type = flow_ring_create_req->item_type;

	bcm_ether_ntoa((struct ether_addr *)flow_ring_create_req->da, eabuf);
	PCI_TRACE(("Create flowid %d for %c%c%c%c  prio %d"
		"max_items %d item len item %d type %d %p\n",
		flowid, eabuf[0], eabuf[1], eabuf[2], eabuf[3],
		flow_ring_create_req->tid, max_items, item_size, item_type, pciedev));

	/* Validate mapping index, which should be less than max flows and
	 * flowid, which should be >= flow id start value
	 */
	if ((flowid >= pciedev->max_h2d_rings) ||
		(flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START))
	{
		PCI_ERROR(("%d: flow create error invalid flowid\n", flowid));
		status = BCMPCIE_RING_ID_INVALID;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

	/* Ensure that the request uses the the correct item type and item size
	 * that was established during PCIE IPC Training phase.
	 * All flowrings use a single shared cir_buf_pool that was instantiated
	 * for this single item_size. If more than one item_size is needed, then
	 * multiple cir_buf_pool's need to be instantiated.
	 */
	if (item_type != pciedev->txpost_item_type) {
		PCI_ERROR(("Create flowring: flowid %u item type %u, expected %u\n",
			flowid, item_type, pciedev->txpost_item_type));
		status = BCMPCIE_BADOPTION;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

	if (item_size != pciedev->txpost_item_size) {
		PCI_ERROR(("Create flowring: flowid %u item size %u, expected %u\n",
			flowid, item_size, pciedev->txpost_item_size));
		status = BCMPCIE_BADOPTION;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}
	/* XXX Remove h2d_submitring_ptr based indirection. Directly use the
	 * pciedev->flowrings_table[flowid]->inited without an explicit allocation
	 * using pciedev_allocate_flowring() below.
	 */
	if (pciedev->h2d_submitring_ptr[flowid] != NULL) {
		PCI_ERROR(("Ring in use %d\n", flowid));
		status = BCMPCIE_RING_IN_USE;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

	flow_ring = pciedev_allocate_flowring(pciedev);
	if ((flow_ring == NULL) || (flow_ring->inited)) {
		PCI_ERROR(("Ring in use %d\n", flowid));
		status = BCMPCIE_RING_IN_USE;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

	flow_ring->ringid = flowid;

	/* Now msgbuf_ring::pcie_ipc_ring_mem, wr_ptr and rd_ptr will be assigned */
	if (pciedev_h2d_msgbuf_bind_pcie_ipc(pciedev,
	        flowid, flow_ring, DYNAMIC_RING) != BCME_OK)
	{
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		ASSERT(0);
	}

	/* MSGBUF_IPC_RING_MEM(flow_ring) will be set now, so lets sync host info */
	pciedev_flowrings_sync_pcie_ipc(pciedev, flow_ring,
		flowid, flow_ring_create_req->haddr64, max_items, item_type, item_size);

	/* informs WL subsystem about the FLOW_RING_CREATE operation */
	response = dngl_flowring_update(pciedev->dngl, flow_ring_create_req->msg.if_id,
		flowid, FLOW_RING_CREATE,  flow_ring_create_req->sa,
		flow_ring_create_req->da, flow_ring_create_req->tid,
		&flow_ring->flow_info.iftype_ap);

	if (response == BCME_BADARG) {
		PCI_ERROR(("BAD ifindex for flowid %d\n", flowid));
		status = BCMPCIE_NOTFOUND;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

#if defined(BCMHWA) && defined(HWA_TXPOST_BUILD)
	/* informs HWA subsystem about the FLOW_RING_CREATE operation */
	haddr64.loaddr = HADDR64_LO_LTOH(flow_ring_create_req->haddr64);
	haddr64.hiaddr = HADDR64_HI_LTOH(flow_ring_create_req->haddr64);

	hwa_txpost_frc_table_config(hwa_dev, flowid,
		flow_ring_create_req->msg.if_id,
		flow_ring_create_req->max_items /* number of WI in ring */,
		&haddr64, flow_ring->flow_info.iftype_ap);
#endif /* BCMHWA && HWA_TXPOST_BUILD */

#if !defined(FLOWRING_USE_SHORT_BITSETS)
	flow_ring->bitmap_size = max_items;
#else
	/* Before allocating inprocess, status_cmpl and other
	 * bitset arrays, find the limits for them.
	 *
	 * Bitmap size should ideally be minimum of the following
	 * for optimal memory consumption.
	 *   - Flowring size
	 *   - Flowring sliding window size
	 *   - Lfrag pool len
	 */
#ifdef BCMFRAGPOOL
	flow_ring->bitmap_size = MIN(pktpool_max_pkts(pciedev->pktpool_lfrag), max_items);
#endif // endif
#if defined(BCMHWA) && defined(HWA_TXPOST_BUILD)
	flow_ring->bitmap_size = MIN(HWA_TXPATH_PKTS_MAX, max_items);
#endif // endif
#ifdef FLOWRING_SLIDING_WINDOW
	flow_ring->bitmap_size = MIN(flow_ring->bitmap_size, FLOWRING_SLIDING_WINDOW_SIZE);
#endif // endif
	if ((max_items % flow_ring->bitmap_size) != 0) {
		/* Flowring size has to be a multiple of the bitmap_size.
		 * Otherwise, flowring wraparound will not work well.
		 * So round it up to the nearest factor.
		 */
		while ((max_items % flow_ring->bitmap_size) != 0)
			flow_ring->bitmap_size++;
	}
#endif /* FLOWRING_USE_SHORT_BITSETS */

	/* bcm_count_zeros_sequence relies on 32bit manipulations */
	ASSERT((flow_ring->bitmap_size % (NBBY * sizeof(uint32))) == 0);

	PCI_INFORM(("flow_create : bitmap_size=%d  max_items=%d\n",
		flow_ring->bitmap_size, max_items));

	/* Make sure bitmap size is multiple of NBBY */
	if ((flow_ring->bitmap_size % NBBY) != 0) {
		PCI_ERROR(("bitmap size is not multiple of %d\n", NBBY));
		OSL_SYS_HALT();
	}

	flow_ring->status_cmpl =
		MALLOCZ(pciedev->osh, flow_ring->bitmap_size/NBBY);
	if (!flow_ring->status_cmpl) {
		PCI_ERROR(("Malloc failed for flowid %d\n", flowid));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		status = BCMPCIE_NOMEM;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

	flow_ring->inprocess =
		MALLOCZ(pciedev->osh, flow_ring->bitmap_size/NBBY);
	if (!flow_ring->inprocess) {
		PCI_ERROR(("Malloc failed for flowid %d\n", flowid));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		status = BCMPCIE_NOMEM;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

	flow_ring->reuse_sup_seq =
		MALLOCZ(pciedev->osh, flow_ring->bitmap_size/NBBY);
	if (!flow_ring->reuse_sup_seq) {
		PCI_ERROR(("Malloc failed for flowid %d\n", flowid));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		status = BCMPCIE_NOMEM;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}
	flow_ring->ring_ageing_info.sup_cnt = 0;

#ifdef WL_REUSE_KEY_SEQ
	flow_ring->reuse_sup_key_seq =
		MALLOCZ(pciedev->osh, flow_ring->bitmap_size/NBBY);
	if (!flow_ring->reuse_sup_key_seq) {
		PCI_ERROR(("Malloc failed for flowid %d\n", flowid));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		status = BCMPCIE_NOMEM;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}
#endif // endif

	/* get a node from the pool */
	lcl_pool = (flowring_pool_t *)dll_pool_alloc(pciedev->flowrings_dll_pool);
	if (lcl_pool == NULL) {
		status = BCMPCIE_NOMEM;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}
	if ((flow_ring->flow_stats =
		MALLOCZ(pciedev->osh, sizeof(flow_stats_t))) == NULL) {
		PCI_ERROR(("Malloc failed for flow_stats for flowid %d\n", flowid));
		status = BCMPCIE_NOMEM;
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		goto send_resp;
	}

	/* Save flow ring info */
	lcl_pool->ring = (msgbuf_ring_t *)flow_ring;
	flow_ring->lcl_pool = lcl_pool;

	/* insert to active prioring list */
	if (pciedev_process_insert_flowring(pciedev, lcl_pool, flow_ring_create_req->tid)
			!= BCME_OK) {
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_create_rqst);
		PCI_ERROR(("pciedev_process_insert_flowring failed for flowid %d\n", flowid));
		status = BCMPCIE_NOMEM;
		goto send_resp;
	}

	flow_ring->status = FLOW_RING_ACTIVE;

	memcpy(flow_ring->flow_info.sa, flow_ring_create_req->sa, ETHER_ADDR_LEN);
	memcpy(flow_ring->flow_info.da, flow_ring_create_req->da, ETHER_ADDR_LEN);

	flow_ring->flow_info.tid_ac = flow_ring_create_req->tid;
	flow_ring->flow_info.ifindex = flow_ring_create_req->msg.if_id;

#ifdef WLCFP
	/**
	 * Link a flow ring with CFP layer cubby
	 * populate cfp layer flowid and a TCB state array in flow ring layer
	 */
	{
		int response_cfp;
		uint8 tid_ac = PCIEDEV_TID_REMAP(pciedev, flow_ring);
		response_cfp = dngl_bus_cfp_link(pciedev->dngl, flow_ring_create_req->msg.if_id,
				flowid, tid_ac,
				flow_ring_create_req->da, CFP_FLOWID_LINK,
				&flow_ring->tcb_state, &flow_ring->cfp_flowid);
		if (response_cfp != BCME_OK) {
			PCI_ERROR(("CFP link failed for flowid %d response code  %d \n",
				flowid, response_cfp));
			ASSERT(0);
			status = BCMPCIE_NOTFOUND;
			goto send_resp;
		}
	}
#endif /* WLCFP */

	/* BCMC flows are not supported by TAF scheduler currently */
	if (flow_ring->flow_info.iftype_ap && ETHER_ISMULTI(flow_ring->flow_info.da)) {
		flow_ring->taf_capable = FALSE;
	} else {
		flow_ring->taf_capable = TRUE;
	}

	flow_ring->current_phase = MSGBUF_RING_INIT_PHASE;
	flow_ring->flow_info.pktinflight = 0;
	flow_ring->flow_info.flags = FALSE;
	flow_ring->flow_info.max_rate = 0;

	flow_ring->inited = TRUE;

	/* Update per flow reserved pkt count */
	pciedev_update_txflow_pkts_max(pciedev);

	/* setup indirection */
	pciedev->h2d_submitring_ptr[flowid] = flow_ring;

#ifdef PCIEDEV_SUPPR_RETRY_TIMEOUT
	/* Enable Suppress Retry feature for infra flowring only */
	if (flow_ring->flow_info.ifindex == 0) {
		PCIEDEV_SUP_EXP_ENAB_SET(flow_ring, TRUE);
		flow_ring->max_sup_retries = PCIEDEV_MAX_SUP_RETRIES;
		flow_ring->max_sup_tocks = PCIEDEV_MAX_SUP_TOCKS;
	}
	flow_ring->dropped_counter_sup_retries = 0;
	flow_ring->dropped_timer_sup_retries = 0;
	flow_ring->sup_retries = 0;
#endif /* PCIEDEV_SUPPR_RETRY_TIMEOUT */

	/* Update packet count TBD */

	flow_ring->flow_info.maxpktcnt = pciedev->tunables[MAXPKTFETCH];
	flow_ring->handle = response & SCBHANDLE_MASK;
	if (response & SCBHANDLE_INFORM_PKTPEND_MASK) {
		flow_ring->flow_info.flags = FLOW_RING_FLAG_INFORM_PKTPEND;
	}
	if (response & SCBHANDLE_PS_STATE_MASK) {
		flow_ring->status |= FLOW_RING_PORT_CLOSED;
	}

send_resp:
	if (flow_ring && (status != BCMPCIE_SUCCESS))
		pciedev_free_flowring_allocated_info(pciedev, flow_ring);

	/* Prepare the response */
	memset(&ctrl_cmplt, 0, sizeof(ctrl_completion_item_t));
	ctrl_cmplt.txfl_create_resp.cmplt.status = status;
	ctrl_cmplt.txfl_create_resp.cmplt.flow_ring_id = flow_ring_create_req->flow_ring_id;
	ctrl_cmplt.txfl_create_resp.cmn_hdr.msg_type = MSG_TYPE_FLOW_RING_CREATE_CMPLT;
	ctrl_cmplt.txfl_create_resp.cmn_hdr.if_id = flow_ring_create_req->msg.if_id;
	ctrl_cmplt.txfl_create_resp.cmn_hdr.request_id = flow_ring_create_req->msg.request_id;
	/* Add response to the local queue */
	if (pciedev_schedule_ctrl_cmplt(pciedev, &ctrl_cmplt) != BCME_OK)
		return BCME_ERROR;

#ifdef MEM_ALLOC_STATS
	hnd_get_heapuse(&mu);
	bytes_allocated = freemem_before - mu.arena_free;
	if (bytes_allocated > max_mem_alloc) {
		mu.max_flowring_alloc =
			mu.max_flowring_alloc - max_mem_alloc + bytes_allocated;
		max_mem_alloc = bytes_allocated;
	}
	mu.total_flowring_alloc += bytes_allocated;
	hnd_update_mem_alloc_stats(&mu);
#endif /* MEM_ALLOC_STATS */

	return BCME_OK;
} /* pciedev_process_flow_ring_create_rqst */

void
pciedev_extra_txlfrag_requirement(void *ctx, uint16 count)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)ctx;

	PCI_TRACE(("pciedev_extra_txlfrag_requirement_cb called count %d %d\n",
		count, pciedev->extra_lfrags));
	if (!count || (pciedev->extra_lfrags < count)) {
		pciedev->extra_lfrags = count;
	}
}

static void
pciedev_free_flowring_allocated_info(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring)
{
#ifdef MEM_ALLOC_STATS
	memuse_info_t mu;
	uint32 freemem_before;

	hnd_get_heapuse(&mu);
	freemem_before = mu.arena_free;
#endif /* MEM_ALLOC_STATS */

	if (flow_ring->status_cmpl)
		MFREE(pciedev->osh, flow_ring->status_cmpl, flow_ring->bitmap_size/NBBY);

	if (flow_ring->inprocess)
		MFREE(pciedev->osh, flow_ring->inprocess, flow_ring->bitmap_size/NBBY);

	if (flow_ring->reuse_seq_list) {
		flow_ring->ring_ageing_info.sup_cnt = 0;
		pciedev_free_reuse_seq_list(pciedev, flow_ring);
	}

	if (flow_ring->reuse_sup_seq) {
		MFREE(pciedev->osh, flow_ring->reuse_sup_seq, flow_ring->bitmap_size/NBBY);
	}

#ifdef WL_REUSE_KEY_SEQ
	if (flow_ring->reuse_sup_key_seq) {
		MFREE(pciedev->osh, flow_ring->reuse_sup_key_seq, flow_ring->bitmap_size/NBBY);
	}
	flow_ring->reuse_sup_key_seq = NULL;
#endif // endif
	if (flow_ring->flow_stats)
		MFREE(pciedev->osh, flow_ring->flow_stats, sizeof(flow_stats_t));
	flow_ring->inprocess = flow_ring->status_cmpl = flow_ring->reuse_sup_seq = NULL;
	flow_ring->flow_stats = NULL;

#ifdef MEM_ALLOC_STATS
	hnd_get_heapuse(&mu);
	mu.total_flowring_alloc -= freemem_before - mu.arena_free;
	hnd_update_mem_alloc_stats(&mu);
#endif /* MEM_ALLOC_STATS */

}

static int
pciedev_flush_flow_ring(struct dngl_bus * pciedev, uint16 flowid)
{
	msgbuf_ring_t	*flow_ring;

	if (flowid >= pciedev->max_h2d_rings) {
		PCI_ERROR(("flowid=%d: flush flow error : invalid flowid\n", flowid));
		return pciedev_send_flow_ring_delete_resp(pciedev, flowid,
			BCMPCIE_RING_ID_INVALID, NULL);
	}

	if (pciedev->h2d_submitring_ptr[flowid] == NULL) {
		PCI_ERROR(("flowid=%d: flush flow error : flow ring is NULL\n", flowid));
		return pciedev_send_flow_ring_delete_resp(pciedev, flowid,
				BCMPCIE_NOTFOUND, NULL);
	}

	flow_ring = pciedev->h2d_submitring_ptr[flowid];
	/* If flush is in process ignore this */

	if (flow_ring->status & FLOW_RING_FLUSH_PENDING)
		return BCME_OK;

	/* If there was a partial fetched AMSDU then clean that up. */
	pciedev_flush_cached_amsdu_frag(pciedev, flow_ring);

	flow_ring->status |= FLOW_RING_FLUSH_PENDING;

	/* Read and Flush any Pkt Fetch */
	pciedev_h2d_start_fetching_host_buffer(pciedev, flow_ring);

	/* Calls WL subsystem, flushes any software q and TXFIFO q packets */
	dngl_flowring_update(pciedev->dngl, flow_ring->flow_info.ifindex,
		flow_ring->ringid, FLOW_RING_FLUSH,  (uint8 *)flow_ring->flow_info.sa,
		(uint8 *)flow_ring->flow_info.da, flow_ring->flow_info.tid_ac, NULL);

	/* We should have cleared all pkts in WL Software txq pkts for the ring */
	/* If any remaining pkts for ring are in TXFIFO, call txfifo flush now */
	if (flow_ring->flow_info.pktinflight) {
		dngl_flowring_update(pciedev->dngl, flow_ring->flow_info.ifindex,
			flow_ring->ringid, FLOW_RING_FLUSH_TXFIFO, (uint8 *)flow_ring->flow_info.sa,
			(uint8 *)flow_ring->flow_info.da, (pciedev->schedule_prio) ?
			flow_ring->flow_info.tid_ac | PCIEDEV_IS_AC_TID_MAP_MASK :
			flow_ring->flow_info.tid_ac, NULL);
	}

	/* Log Flow Flush stats */
	PCIEDEV_FLOW_FLUSH_TS(flow_ring, OSL_SYSUPTIME());

	return BCME_OK;
}

/** Set frwd_resrv_bufcnt coming from wl layer */
void
pciedev_set_frwd_resrv_bufcnt(struct dngl_bus *pciedev, uint16 frwd_resrv_bufcnt)
{
	pciedev->frwd_resrv_bufcnt = frwd_resrv_bufcnt;
}

void print_config_register_value(struct dngl_bus *pciedev, uint32 startindex, uint32 endindex)
{
	int i;
	uint32 dump_array_size;

	dump_array_size = sizeof(pcie_dump_registers_address) / sizeof(uint32);

	if ((startindex > dump_array_size) || (endindex > dump_array_size)) {
		PCI_PRINT(("error invalid index to: %s\n", __FUNCTION__));
		return;
	}

	for (i = startindex; i < endindex; i++) {
		uint32 val;
		val = R_REG_CFG(pciedev, pciedev_pcie_dump_registers_address(i));
		PCI_PRINT(("offset: %x: value %08x\n",
			pciedev_pcie_dump_registers_address(i), val));
	}
}

#ifdef UART_TRAP_DBG
void
pciedev_dump_pcie_info(uint32 arg)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)(uintptr)arg;
	int i, j;

	if (!pciedev->dbg_trap_enable)
		return;

	PCI_PRINT(("chip common registers\n"));

	for (i = 0; i < sizeof(pcie_common_registers) / sizeof(uint32); i++)
		PCI_PRINT(("offset: %x value: %08x\n", pciedev_pcie_common_registers(i),
			si_corereg(pciedev->sih, SI_CC_IDX, pciedev_pcie_common_registers(i), 0,
			0)));

	PCI_PRINT(("pcie registers:\n"));

	print_config_register_value(pciedev, 0,
		sizeof(pcie_dump_registers_address) / sizeof(uint32));

	PCI_PRINT(("pcie control: %08x\n", R_REG(pciedev->osh,
		PCIE_pciecontrol(pciedev->autoregs))));

	ai_dump_APB_Bridge_registers(pciedev->sih);

	for (i = 0; i < sizeof(dma_registers_address) / sizeof(uint32); i++) {
		PCI_PRINT(("\noffset: %x\n", pciedev_dma_registers_address(i)));
		for (j = 0; j < NUM_OF_ADDRESSES_PER_DMA_DUMP; j++) {
			if (j == 3)
				PCI_PRINT(("\n"));
			PCI_PRINT(("%08x ", *(uint32*)(pciedev_dma_registers_address(i)+j*4)));
		}
	}
} /* pciedev_dump_pcie_info */
void
pciedev_trap_from_uart(uint32 arg)
{

	struct dngl_bus *pciedev = (struct dngl_bus *)(uintptr)arg;

	if (!pciedev->dbg_trap_enable)
		return;

	traptest();

	OSL_SYS_HALT();
} /* pciedev_trap_from_uart */

bool
pciedev_uart_debug_enable(struct dngl_bus *pciedev, int enab)
{
	if (enab == 0 || enab == 1)
		pciedev->dbg_trap_enable = (bool)enab;
	return pciedev->dbg_trap_enable;
}
#endif /* UART_TRAP_DBG */

static int
pciedev_process_flow_ring_delete_rqst(struct dngl_bus * pciedev, void *p)
{
	msgbuf_ring_t	*flow_ring;
	tx_flowring_delete_request_t *flow_ring_delete_req = (tx_flowring_delete_request_t *)p;
	uint16 flowid;
#ifdef PCIEDEV_FAST_DELETE_RING
	uint16 bit_idx, rd_idx, wr_idx, ring_max, total;
#endif /* PCIEDEV_FAST_DELETE_RING */

	flowid = ltoh16(flow_ring_delete_req->flow_ring_id);
	PCI_TRACE(("pciedev_process_flow_ring_delete_rqst: flowid=%d\n", flowid));

	if ((flowid >= pciedev->max_h2d_rings) ||
		(flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)) {
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_delete_rqst);
		PCI_ERROR(("%d: flow delete error: invalid flowid\n", flowid));
		return pciedev_send_flow_ring_delete_resp(pciedev, flowid,
			BCMPCIE_RING_ID_INVALID, p);
	}

	/* get flow ring on device data base */
	flow_ring = pciedev->h2d_submitring_ptr[flowid];
	if ((flow_ring == NULL) || (!flow_ring->inited))
		return pciedev_send_flow_ring_delete_resp(pciedev,
			flowid, BCMPCIE_NOTFOUND, p);
	flow_ring->request_id = flow_ring_delete_req->msg.request_id;

#ifdef WLCFP
	/* Clear the ROAM_CHECK flag before deletion */
	flow_ring->status &= ~FLOW_RING_ROAM_CHECK;
#endif /* WLCFP */

	/* Check if any packets are pending if nothing is pending send response now */
	if (MSGBUF_WR(flow_ring) == MSGBUF_RD(flow_ring)) {
#ifdef PCIEDEV_FAST_DELETE_RING
		if (pciedev->fastdeletering) {
			flow_ring->delete_idx = MSGBUF_WR(flow_ring);
		}
#endif /* PCIEDEV_FAST_DELETE_RING */
		if (flow_ring->d2h_q_txs_pending == 0) {
			return pciedev_send_flow_ring_delete_resp(pciedev, flowid,
				BCMPCIE_SUCCESS, p);
		}
		flow_ring->status |= FLOW_RING_DELETE_RESP_PENDING;
		return BCME_OK;
	}
#ifdef PCIEDEV_FAST_DELETE_RING
	if (pciedev->fastdeletering) {
		/* Start looking for suppressed and out of order gaps. Avoid as many
		 * txstatus as possible by determining the lowest possible delete_idx.
		 * If this happens to be the rd_idx then set FAST_DELETE_ACTIVE here to
		 * start suppressing txstatus and fetching more host data. Txstatus
		 * suppress can only be done for contiguous space.
		 */
		wr_idx = MSGBUF_WR(flow_ring);
		rd_idx = MSGBUF_RD(flow_ring);
		ring_max = MSGBUF_MAX(flow_ring);

		/* Determine if wr is beyond rd_idx + bitmap_size. No need to fetch
		 * anything beyond that, since they were never fetched, so no gap
		 * for that.
		 */
		total = (wr_idx > rd_idx) ? (wr_idx - rd_idx) : (wr_idx + ring_max - rd_idx);

		if (total >= flow_ring->bitmap_size)
			total = flow_ring->bitmap_size - 1;
		bit_idx = MSGBUF_MODULO_IDX(flow_ring, rd_idx + total);

		while (bit_idx != MSGBUF_MODULO_IDX(flow_ring, rd_idx)) {
			if (isset(flow_ring->status_cmpl, bit_idx))
				break;
			if (bit_idx) {
				bit_idx--;
			} else {
				bit_idx = flow_ring->bitmap_size - 1;
			}
		}
		/* if status_cmpl not set for bit_idx then enable suppress now */
		if (!isset(flow_ring->status_cmpl, bit_idx)) {
			flow_ring->status |= FLOW_RING_FAST_DELETE_ACTIVE;
			flow_ring->delete_idx = rd_idx;
		} else {
			/* store index which can be returned and used for swithching to
			 * FAST_DELETE_ACTIVE
			 */
			if (bit_idx > MSGBUF_MODULO_IDX(flow_ring, rd_idx)) {
				total = bit_idx - MSGBUF_MODULO_IDX(flow_ring, rd_idx);
			} else {
				total = bit_idx + flow_ring->bitmap_size -
					MSGBUF_MODULO_IDX(flow_ring, rd_idx);
			}
			/* This one has status_cmpl, so from next one suppresss should start */
			total++;
			flow_ring->delete_idx = (rd_idx + total) % ring_max;
		}
	}
	if ((flow_ring->status & FLOW_RING_FAST_DELETE_ACTIVE) &&
	    (flow_ring->flow_info.pktinflight == 0) && (flow_ring->fetch_pending == 0)) {
		if (flow_ring->d2h_q_txs_pending == 0) {
			return pciedev_send_flow_ring_delete_resp(pciedev, flowid,
				BCMPCIE_SUCCESS, p);
		}
		flow_ring->status |= FLOW_RING_DELETE_RESP_PENDING;
		return BCME_OK;
	}
#endif /* PCIEDEV_FAST_DELETE_RING */
	/* Delete Resp will come after all packets are drained */
	flow_ring->status |= FLOW_RING_DELETE_RESP_PENDING;
	return pciedev_flush_flow_ring(pciedev, flowid);
}

int
pciedev_send_flow_ring_delete_resp(struct dngl_bus * pciedev, uint16 flowid,
	uint32 status, void *del_req_p)
{
	msgbuf_ring_t	*flow_ring;
	ctrl_completion_item_t ctrl_cmplt;
	tx_flowring_delete_request_t *del_req = (tx_flowring_delete_request_t *)del_req_p;

	PCI_TRACE(("pciedev_send_flow_ring_delete_resp: flowid=%d\n", flowid));

	/* get flow ring on device data base TBD */
	flow_ring = pciedev->h2d_submitring_ptr[flowid];
	if (!(flow_ring || del_req)) {
		DBG_BUS_INC(pciedev, pciedev_send_flow_ring_delete_resp);
		TRAP_DUE_NULL_POINTER(("Trapping for NULL at flowring=0x%x  del_req=0x%x\n",
			(uint32)flow_ring, (uint32)del_req));
		ASSERT(0);
	}

	/* Before sending response, make sure pktinflight is zero */
	if ((flow_ring != NULL) && (flow_ring->flow_info.pktinflight != 0)) {
		TRAP_IN_PCIEDRV(("TRAP: flowid: %d pktinflight is non-zero %d !!!\n", flowid,
			flow_ring->flow_info.pktinflight));
	}

	/* Prepare the response */
	memset(&ctrl_cmplt, 0, sizeof(ctrl_completion_item_t));
	ctrl_cmplt.txfl_delete_resp.cmplt.status = status;
	ctrl_cmplt.txfl_delete_resp.msg.msg_type = MSG_TYPE_FLOW_RING_DELETE_CMPLT;
	if (flow_ring) {
		ctrl_cmplt.txfl_delete_resp.cmplt.flow_ring_id = flow_ring->ringid;
		ctrl_cmplt.txfl_delete_resp.msg.if_id = flow_ring->flow_info.ifindex;
		ctrl_cmplt.txfl_delete_resp.msg.request_id = flow_ring->request_id;
#ifdef PCIEDEV_FAST_DELETE_RING
		if (pciedev->fastdeletering) {
			/* transfer the read_idx for which the deletering started. */
			ctrl_cmplt.txfl_delete_resp.read_idx = flow_ring->delete_idx;
			flow_ring->status &= ~FLOW_RING_FAST_DELETE_ACTIVE;
		}
#endif /* PCIEDEV_FAST_DELETE_RING */
	} else {
		ctrl_cmplt.txfl_delete_resp.cmplt.flow_ring_id = del_req->flow_ring_id;
		ctrl_cmplt.txfl_delete_resp.msg.if_id = del_req->msg.if_id;
		ctrl_cmplt.txfl_delete_resp.msg.request_id = del_req->msg.request_id;
	}

	/* Add the response to the local queue */
	if (pciedev_schedule_ctrl_cmplt(pciedev, &ctrl_cmplt) != BCME_OK) {
		DBG_BUS_INC(pciedev, pciedev_send_flow_ring_delete_resp);
		if (!flow_ring) {
			PCI_ERROR(("%d: Send delete response error: can't send ctrl compl\n",
				flowid));
			return BCME_ERROR;
		}
		flow_ring->status |= FLOW_RING_DELETE_RESP_RETRY;
		pciedev->ctrl_resp_q->num_flow_ring_delete_resp_pend++;
		return BCME_ERROR;
	}
	/* flow_ring null means we got non-active ringid from host, we should
	 * have taken care as del_req(valid pointer), and should have all data
	 * needed to send the response as done above, Lets return from here as we are done
	 */
	if (!flow_ring) {
		PCI_TRACE(("pciedev_send_flow_ring_delete_resp: non-active ringid from host\n"));
		return BCME_OK;
	}

	/* If flow is not inited yet, it implies lcl_pool is not allocated as well.
	 * Just return the deletion err to host and not to remove flowring.
	 */
	if (!flow_ring->inited) {
		PCI_ERROR(("%s: del a non-inited flow!\n", __FUNCTION__));
		pciedev->h2d_submitring_ptr[flowid] = NULL;
		return BCME_NOTREADY;
	}

#ifdef WLCFP
	{
		uint8 tid_ac = PCIEDEV_TID_REMAP(pciedev, flow_ring);
		/* De-Link the flowring and CFP flow */
		dngl_bus_cfp_link(pciedev->dngl, flow_ring->flow_info.ifindex,
			flowid, tid_ac,
			(uint8 *)flow_ring->flow_info.da,
			CFP_FLOWID_DELINK, NULL, &(flow_ring->cfp_flowid));
	}
#endif /* WLCFP */

	/* If the work item made it to the queue, it will be sent
	 * when resources are available so continue with the delete
	 */

	/* Remove flow ring from active prioring list */
	pciedev_process_remove_flowring(pciedev, flow_ring->lcl_pool);

	pciedev_free_msgbuf_flowring(pciedev, flow_ring);

	/* Update per flow max pkts since one flow has been deleted */
	pciedev_update_txflow_pkts_max(pciedev);
	pciedev->h2d_submitring_ptr[flowid] = NULL;
	flow_ring->status &= ~FLOW_RING_DELETE_RESP_PENDING;

	if (dll_empty(&pciedev->active_prioring_list) &&
		PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev)) {
		PCI_TRACE(("%s: pend count is %d\n",
			__FUNCTION__, PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev)));
		PCIEDEV_XMIT_TXSTATUS(pciedev);
	}

	PCI_TRACE(("%s: wr %d rd %d pktinflight %d wr_pend %d fetch_pending %d id %d\n",
		__FUNCTION__, MSGBUF_WR(flow_ring), MSGBUF_RD(flow_ring),
		flow_ring->flow_info.pktinflight, MSGBUF_WR_PEND(flow_ring),
		flow_ring->fetch_pending, flow_ring->ringid));

	/* If this is for a previous response that could not be sent
	 * decrement the counter and clear the status bit
	 */
	if ((pciedev->ctrl_resp_q->num_flow_ring_delete_resp_pend) &&
		(flow_ring->status & FLOW_RING_DELETE_RESP_RETRY)) {
		pciedev->ctrl_resp_q->num_flow_ring_delete_resp_pend--;
		flow_ring->status &= ~FLOW_RING_DELETE_RESP_RETRY;
	}

#if defined(BCMHWA) && defined(HWA_TXPOST_BUILD)
	/* informs HWA subsystem about the FLOW_RING_DELETE operation */
	hwa_txpost_frc_table_reset(hwa_dev, flowid);
#endif /* BCMHWA && HWA_TXPOST_BUILD */

	return BCME_OK;

} /* pciedev_send_flow_ring_delete_resp */

static void
pciedev_process_flow_ring_flush_rqst(struct dngl_bus * pciedev, void *p)
{
	msgbuf_ring_t	*flow_ring;
	tx_flowring_flush_request_t *flow_ring_flush_req = (tx_flowring_flush_request_t *)p;
	uint16 flowid;

	flowid = ltoh16(flow_ring_flush_req->flow_ring_id);

	if ((flowid >= pciedev->max_h2d_rings) ||
			(flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)) {
		PCI_ERROR(("%d: flow flush error: invalid flowid\n",
			flowid));
		DBG_BUS_INC(pciedev, pciedev_process_flow_ring_flush_rqst);
		return pciedev_send_flow_ring_flush_resp(pciedev,
			flowid, BCMPCIE_RING_ID_INVALID, p);
	}

	/* get flow ring on device data base TBD */
	flow_ring = pciedev->h2d_submitring_ptr[flowid];
	if (flow_ring == NULL) {
		PCI_ERROR(("%d: flow flush error: invalid ringptr\n",
			flowid));
		return pciedev_send_flow_ring_flush_resp(pciedev,
			flowid, BCMPCIE_RING_ID_INVALID, p);
	}
	flow_ring->request_id = flow_ring_flush_req->msg.request_id;
	/* Check if any packets are pending if nothing is pending send response now */
	if (MSGBUF_WR(flow_ring) == MSGBUF_RD(flow_ring)) {
		pciedev_send_flow_ring_flush_resp(pciedev, flowid,
			BCMPCIE_SUCCESS, p);
		return;
	}
	flow_ring->status |= FLOW_RING_FLUSH_RESP_PENDING;
	pciedev_flush_flow_ring(pciedev, flowid);
}

void
pciedev_send_flow_ring_flush_resp(struct dngl_bus * pciedev, uint16 flowid,
	uint32 status, void *p)
{
	msgbuf_ring_t	*flow_ring;
	ctrl_completion_item_t ctrl_cmplt;
	tx_flowring_flush_request_t *flush_req = (tx_flowring_flush_request_t *)p;

	PCI_TRACE(("pciedev_send_flow_ring_flush_resp: flowid=%d\n", flowid));

	if ((flowid >= pciedev->max_h2d_rings) ||
		(flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)) {
		flow_ring = NULL;
	} else {
		/* get flow ring on device data base TBD */
		flow_ring = pciedev->h2d_submitring_ptr[flowid];
		if (!(flow_ring || flush_req)) {
			DBG_BUS_INC(pciedev, pciedev_send_flow_ring_flush_resp);
			TRAP_DUE_NULL_POINTER(("Trap for NULL at flowring=0x%x flush_req=0x%x\n",
				(uint32)flow_ring, (uint32)flush_req));
			ASSERT(0);
		}
	}

	memset(&ctrl_cmplt, 0, sizeof(ctrl_completion_item_t));
	ctrl_cmplt.txfl_flush_resp.msg.msg_type = MSG_TYPE_FLOW_RING_FLUSH_CMPLT;
	ctrl_cmplt.txfl_flush_resp.cmplt.status = status;
	if (flow_ring) {
		ctrl_cmplt.txfl_flush_resp.msg.if_id = flow_ring->flow_info.ifindex;
		ctrl_cmplt.txfl_flush_resp.msg.request_id = flow_ring->request_id;
		ctrl_cmplt.txfl_flush_resp.cmplt.flow_ring_id = flow_ring->ringid;
	} else {
		ctrl_cmplt.txfl_flush_resp.cmplt.flow_ring_id = flush_req->flow_ring_id;
		ctrl_cmplt.txfl_flush_resp.msg.if_id = flush_req->msg.if_id;
		ctrl_cmplt.txfl_flush_resp.msg.request_id = flush_req->msg.request_id;
	}

	if (pciedev_schedule_ctrl_cmplt(pciedev, &ctrl_cmplt) != BCME_OK) {
		DBG_BUS_INC(pciedev, pciedev_send_flow_ring_flush_resp);
		if (!flow_ring) {
			PCI_ERROR(("%d: Send flush response error: can't send ctrl compl\n",
				flowid));
			return;
		}
		flow_ring->status |= FLOW_RING_FLUSH_RESP_RETRY;
		pciedev->ctrl_resp_q->num_flow_ring_flush_resp_pend++;
		return;
	}
	/* flow_ring null means we got non-active ringid from host, we should
	 * have taken care as flush_req(valid pointer), and should have all data
	 * needed to send the response as done above, Lets return from here as we are done
	 */
	if (!flow_ring) {
		PCI_TRACE(("pciedev_send_flow_ring_flush_resp: non-active ringid from host\n"));
		return;
	}

	/* If the work item made it to the queue, it will be sent
	 * when resources are available
	 */
	flow_ring->status &= ~FLOW_RING_FLUSH_RESP_PENDING;

	/* If this is for a previous response that could not be sent
	 * decrement the counter and clear the status bit
	 */
	if ((pciedev->ctrl_resp_q->num_flow_ring_flush_resp_pend) &&
		(flow_ring->status & FLOW_RING_FLUSH_RESP_RETRY)) {
		pciedev->ctrl_resp_q->num_flow_ring_flush_resp_pend--;
		flow_ring->status &= ~FLOW_RING_FLUSH_RESP_RETRY;
	}
}

/* In case a flowring is flushed or deleted from host, all packets linked to the corresponding
 * flowid need to be freed as soon as possible. This function is called indirectly from
 * wl_send_cb in case packet fetching is used.
 * When this function is called, ioctl_buf should contain the flowid.
 * If flowring is not inited or flush is pending, the ioctl_buf will be set to 1.
 * In all other cases, ioctl_buf will be set to 0.
 */
uint32
pciedev_flow_ring_flush_pending(struct dngl_bus *pciedev, char *ioctl_buf,
	uint32 inlen, uint32 *outlen)
{
	msgbuf_ring_t *flow_ring;
	uint16 flowid, index;

	/* Expecting uint16 flowid in ioctl_buf, so inlen should be 2 bytes */
	if (inlen != 2) {
		return BCME_BADARG;
	}

	flowid = *(uint16*)ioctl_buf;
	index = FLRING_INX(flowid);

	if ((index >= pciedev->max_h2d_rings) || (flowid < BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)) {
		PCI_ERROR(("%s: %d invalid flowid\n",
			__FUNCTION__, flowid));
		return BCME_BADARG;
	}

	flow_ring = pciedev->h2d_submitring_ptr[flowid];
	ASSERT(flow_ring != NULL);

	if (!flow_ring->inited || (flow_ring->status & FLOW_RING_FLUSH_PENDING)) {
		*(uint16*)ioctl_buf = 1;
	} else {
		*(uint16*)ioctl_buf = 0;
	}

	return BCME_OK;
}

static void
pciedev_free_msgbuf_flowring(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring)
{
	PCI_TRACE(("pciedev_free_msgbuf_flowring\n"));

	if (flow_ring != NULL) {
		ASSERT(flow_ring->d2h_q_txs_pending == 0);
		flow_ring->inited = FALSE;
		flow_ring->wr_pending = flow_ring->fetch_ptr = 0;
		pciedev_free_flowring_allocated_info(pciedev, flow_ring);
		flow_ring->fetch_pending = 0;
		MSGBUF_RD(flow_ring) = 0;
#ifdef WLSQS
		flow_ring->wr_peeked = 0;
#endif // endif

#if defined(PCIE_DMA_INDEX) && defined(SBTOPCIE_INDICES)
		/* Sync the read index with host mem */
		pciedev_sync_h2d_read_ptrs(pciedev, flow_ring);
#endif /* PCIE_DMA_INDEX & SBTOPCIE_INDICES */

#ifdef PCIEDEV_SUPPR_RETRY_TIMEOUT
		/* Disable Suppression Ageout for this flow */
		if (PCIEDEV_SUP_EXP_ENAB(flow_ring)) {
			PCIEDEV_SUP_EXP_ENAB_SET(flow_ring, FALSE);
		}
#endif /* PCIEDEV_SUPPR_RETRY_TIMEOUT */
		flow_ring->cfp_flowid = SCB_FLOWID_INVALID;
	}
}

#if defined(PCIE_D2H_DOORBELL_RINGER)
/*
 * Notion of soft doorbell:
 *
 * Instead of using legacy INT (Dev2Host Doorbell 0 register) that is directed
 * to Host CPU (DHD), a notion of a software doorbell may be deployed per D2H
 * Completion ring (TxCompletion and RxCompletion).
 *
 * DHD may request a soft doorbell using a ring_config request sent over the
 * H2D Control Submit ring. A soft doorbell will include an <haddr64, val32>
 * tuple and the ring_id. The 64bit host address must have high 32bit as 0U, as
 * SBTOPCIE will be used to write the val32 to the haddr64.low.
 *
 * Use Case: Dongle can directly wakeup a hardware thread in a core of a host
 * side network processor, by preforming a write to the "network processor's
 * register" over PCIe. The value written is specific to the network processor
 * and may include a core and thread encoding, in a multi core network processor
 * Additional match and remap may be needed on the host side to redirect the
 * PCIe transaction to the appropriate register.
 *
 * NOTE: SBTOPCIE Translation 0 is reserved for soft doorbell.
 */
void
pciedev_d2h_ring_config_soft_doorbell(struct dngl_bus *pciedev,
	const ring_config_req_t *ring_config_req)
{
	int16 resp_status = BCMPCIE_SUCCESS;
	d2h_doorbell_ringer_t *ringer;
	bool use_default_ringer = FALSE;

	const uint16 ring_id = ltoh16(ring_config_req->ring_id);
	const uint16 d2h_ring_idx = ring_id - BCMPCIE_H2D_COMMON_MSGRINGS;
	const bcmpcie_soft_doorbell_t *db_info = &ring_config_req->soft_doorbell;

	/* Only the low 32bit in a haddr64 is relevant with SBTOPCIE */
	uint32 db_haddr32 = HADDR64_LO_LTOH(db_info->haddr);

	pcie_sb2pcie_atr_t *atr0 = &pciedev->sb2pcie_atr[0];

	/* 2b0:AccessType RW=00, 1b2:PrefetchEn=1, 1b3:WriteBurstEn=1 */
	const uint32 rw_pref_wrburst =
		SBTOPCIE_MEM | SBTOPCIE_PF | SBTOPCIE_WR_BURST; /* 0xC 4b1100 */

	ringer = &pciedev->d2h_doorbell_ringer[d2h_ring_idx];

	PCI_INFORM(("D2H ring_id %d haddr" HADDR64_FMT " value 0x%08x\n", ring_id,
		HADDR64_VAL(db_info->haddr), db_info->value));

	/* Ensure 32bit high is always 0U */
	if (HADDR64_HI(db_info->haddr) != 0U) {
		PCI_ERROR(("%s only 32bit soft doorbells supported\n", __FUNCTION__));
		use_default_ringer = TRUE;
		resp_status = BCMPCIE_BADOPTION;

	} else if (db_haddr32 == 0U) { /* Soft Doorbell not requested */

		use_default_ringer = TRUE;

	} else if (pciedev->d2h_doorbell_ringer_inited != TRUE) {

		/* Configure SBTOPCIE Translation 0 register, once */
		W_REG(pciedev->osh, PCIE_sbtopcietranslation0(pciedev->autoregs),
			((db_haddr32 & atr0->mask) | rw_pref_wrburst));
#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__)
		/* Ensure sbtopcie0 is written before haddr is accessed. */
		__asm__ __volatile__("dsb");
		__asm__ __volatile__("isb");
#endif /* __ARM_ARCH_7M__ || __ARM_ARCH_7R__ */
		pciedev->d2h_doorbell_ringer_inited = TRUE;

	} else { /* Soft Doorbell already configured for one D2H Completion ring */

		/* Ascertain all doorbells in 64M or 32M region managed by SBTOPCIE0 */
		uint32 val = R_REG(pciedev->osh, PCIE_sbtopcietranslation0(pciedev->autoregs));
		uint32 map = (db_haddr32 & atr0->mask) | rw_pref_wrburst;

		if (val != map) {
			PCI_ERROR(("%s all doorbells not in %s region\n", __FUNCTION__,
				(atr0->mask == SBTOPCIE_32M_MASK) ? "32M" : "64M"));
			use_default_ringer = TRUE;
			resp_status = BCMPCIE_BADOPTION;
		}
	}

	if (use_default_ringer) { /* Default PCIE doorbell interrupt */
		ringer->db_info.haddr.low = PCIE_DB_DEV2HOST_0;
		ringer->db_info.value = PCIE_D2H_DB0_VAL;
		ringer->db_fn = pciedev_generate_host_db_intr;
	} else { /* host soft doorbell write */
		/* Save sbtopcie mapped host address and doorbell value for use in
		 * pciedev_d2h_doorbell_ringer(), in place of D2H PCIE interrupt
		 */

		/* XXX:
		 * On BCM4365, BCM43684, CA7/CCI-400 required the 128M small
		 * PCIE window to be at 0x20000000, instead of 0x08000000
		 */
		ringer->db_info.haddr.low = /* map to 1st 64MB or 32M region */
			atr0->base + (db_haddr32 & ~atr0->mask);

		ringer->db_info.value = db_info->value;
		ringer->db_fn = pciedev_d2h_doorbell_ringer;

#if defined(BCMHWA) && defined(HWA_CPLENG_BUILD)
		if (hwa_dev) {
			dma64addr_t db_haddr64;

			/* Update software doorbell for HWA CPLE */
			/* TX CPLE */
			if (ring_id == BCMPCIE_D2H_MSGRING_TX_COMPLETE) {
				HADDR64_HI_SET(db_haddr64,
					HWA_HOSTADDR64_HI32(HADDR64_HI_LTOH(db_info->haddr)));
				HADDR64_LO_SET(db_haddr64, HADDR64_LO_LTOH(db_info->haddr));
				hwa_sw_doorbell_request(hwa_dev, HWA_TXCPL_DOORBELL, 0,
					db_haddr64, db_info->value);
				PCI_ERROR(("%s(): Update a software doorbell val<0x%x> for "
					"HWA TX CPLE\n", __FUNCTION__, db_info->value));
			} else if ((ring_id >= BCMPCIE_D2H_MSGRING_RX_COMPLETE) &&
				(ring_id < (BCMPCIE_D2H_MSGRING_RX_COMPLETE + 4))) {
				/* RX CPLE */
				HADDR64_HI_SET(db_haddr64,
					HWA_HOSTADDR64_HI32(HADDR64_HI_LTOH(db_info->haddr)));
				HADDR64_LO_SET(db_haddr64, HADDR64_LO_LTOH(db_info->haddr));
				hwa_sw_doorbell_request(hwa_dev, HWA_RXCPL_DOORBELL,
					(ring_id - BCMPCIE_D2H_MSGRING_RX_COMPLETE),
					db_haddr64, db_info->value);
				PCI_ERROR(("%s(): Update a software doorbell val<0x%x> for "
					"HWA RX[%d] CPLE\n", __FUNCTION__, db_info->value,
					(ring_id - BCMPCIE_D2H_MSGRING_RX_COMPLETE)));
			}
		}
#endif /* BCMHWA */
	}

	/* Send a ring config response ... */
	pciedev_send_d2h_soft_doorbell_resp(pciedev, ring_config_req, resp_status);

} /* pciedev_d2h_ring_config_soft_doorbell */

void
pciedev_send_d2h_soft_doorbell_resp(struct dngl_bus *pciedev,
	const ring_config_req_t *ring_config_req, int16 status)
{
	ring_config_resp_t *ring_config_resp;

	if ((ring_config_resp = (ring_config_resp_t *)
	        pciedev_lclpool_alloc_lclbuf(pciedev->dtoh_ctrlcpl)) == NULL)
		return;
	memset(ring_config_resp, 0, sizeof(ring_config_resp_t));

	ring_config_resp->cmn_hdr.msg_type = MSG_TYPE_D2H_RING_CONFIG_CMPLT;
	ring_config_resp->cmn_hdr.if_id = ring_config_req->msg.if_id;
	ring_config_resp->cmn_hdr.request_id = ring_config_req->msg.request_id;
	ring_config_resp->compl_hdr.flow_ring_id = ring_config_req->ring_id;
	ring_config_resp->compl_hdr.status = status;

	/* transmit ring_config_resp via d2h control completion ring */
	if (!pciedev_xmit_msgbuf_packet(pciedev, ring_config_resp,
	        MSG_TYPE_D2H_RING_CONFIG_CMPLT,
	        MSGBUF_LEN(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl))
	{
		PCI_ERROR(("Ring Config response xmit message failed\n"));
		DBG_BUS_INC(pciedev, pciedev_send_d2h_soft_doorbell_resp);
		ASSERT(0);
		return;
	}
}
#endif /* PCIE_D2H_DOORBELL_RINGER */

#ifdef BCMPCIE_D2H_MSI
void
pciedev_d2h_ring_config_msi(struct dngl_bus *pciedev, const ring_config_req_t *ring_config_req)
{
	pciedev_msi_info_t *d2h_msi_info_ptr = &(pciedev->d2h_msi_info);
	msgbuf_ring_t	*ctrl_msgbuf_ring;
	const bcmpcie_msi_offset_config_t *msi_info = &ring_config_req->msi_offset;
	uint16	intr_idx, vector_offset;
	uint16	len = msi_info->len;
	uint8	ret = BCMPCIE_SUCCESS;
	uint8	i;

	PCI_TRACE(("pciedev_d2h_ring_config_msi\n"));

	if (len > MSI_INTR_IDX_MAX) {
		PCI_ERROR(("%s invalid len\n", __FUNCTION__));
		ret  = BCMPCIE_BADOPTION;
		goto fail;
	}

	for (i = 0; i < len; i++) {
		intr_idx = msi_info->bcmpcie_msi_offset[i].intr_idx;
		if (intr_idx >= MSI_INTR_IDX_MAX) {
			PCI_ERROR(("%s invalid interrupt index\n", __FUNCTION__));
			ret  = BCMPCIE_BADOPTION;
			goto fail;
		}
		vector_offset = msi_info->bcmpcie_msi_offset[i].msi_offset;
		if ((vector_offset < BCMPCIE_D2H_MSI_OFFSET_MB0) ||
			(vector_offset >= BCMPCIE_D2H_MSI_OFFSET_MAX)) {
			PCI_ERROR(("%s invalid vector offset\n", __FUNCTION__));
			ret  = BCMPCIE_BADOPTION;
			goto fail;
		}
		d2h_msi_info_ptr->msi_vector_offset[intr_idx] = vector_offset;
		ctrl_msgbuf_ring =
			&(pciedev->cmn_msgbuf_ring[intr_idx+BCMPCIE_D2H_MSGRING_CONTROL_COMPLETE]);
		if (intr_idx <= MSI_INTR_IDX_RXP_CMPL_RING)
			ctrl_msgbuf_ring->msi_vector_offset =
				d2h_msi_info_ptr->msi_vector_offset[intr_idx];
	}

	/* Configure MSI vector table to enable specific vectors */
	pciedev->msivectorassign = MSIVEC_MB_0 | MSIVEC_MB_1 | MSIVEC_D2H0_DB0 | MSIVEC_D2H0_DB1;
	W_REG(pciedev->osh, PCIE_msivectorassignment(pciedev->autoregs), pciedev->msivectorassign);
	if (PCIECOREREV(pciedev->corerev) < 15) {
		pciedev->msicap = R_REG_CFG(pciedev, PCIECFGREG_MSI_CAP);
		W_REG_CFG(pciedev, PCIECFGREG_MSI_CAP,
			(pciedev->msicap &
			(~(MSICAP_NUM_MSG_MASK | MSICAP_NUM_MSG_EN_MASK))) |
			(5<<MSICAP_NUM_MSG_SHF) | (5<<MSICAP_NUM_MSG_EN_SHF));
	}
	/* Configure PCIEControl to enable Multi msg MSI */
	/**
	 * set PCIE_MULTIMSI_EN even in single-MSI,
	 * so it uses MSI vector offset specified instead of use offset 0
	 */
	OR_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs),
		PCIE_MULTIMSI_EN | PCIE_MSI_B2B_EN);

	d2h_msi_info_ptr->msi_enable = TRUE;

fail:
	/* Send a ring config response ... */
	pciedev_send_d2h_ring_config_resp(pciedev, ring_config_req, ret);
}

void
pciedev_send_d2h_ring_config_resp(struct dngl_bus *pciedev,
	const ring_config_req_t *ring_config_req, int16 status)
{
	ring_config_resp_t *ring_config_resp;

	if ((ring_config_resp = (ring_config_resp_t *)
	        pciedev_lclpool_alloc_lclbuf(pciedev->dtoh_ctrlcpl)) == NULL) {
	        PCI_ERROR(("pciedev_send_d2h_ring_config_resp: alloc failed\n"));
		return;
	}

	memset(ring_config_resp, 0, sizeof(ring_config_resp_t));

	ring_config_resp->cmn_hdr.msg_type = MSG_TYPE_D2H_RING_CONFIG_CMPLT;
	ring_config_resp->cmn_hdr.if_id = ring_config_req->msg.if_id;
	ring_config_resp->cmn_hdr.request_id = ring_config_req->msg.request_id;
	ring_config_resp->compl_hdr.flow_ring_id = ring_config_req->ring_id;
	ring_config_resp->compl_hdr.status = htol16(status);

	/* transmit ring_config_resp via d2h control completion ring */
	if (!pciedev_xmit_msgbuf_packet(pciedev, ring_config_resp,
	        MSG_TYPE_D2H_RING_CONFIG_CMPLT,
	        MSGBUF_LEN(pciedev->dtoh_ctrlcpl), pciedev->dtoh_ctrlcpl))
	{
		PCI_ERROR(("Ring Config response xmit message failed\n"));
		ASSERT(0);
		DBG_BUS_INC(pciedev, pciedev_send_flow_ring_flush_resp);
		return;
	}
}
#endif /* BCMPCIE_D2H_MSI */

void
pciedev_process_d2h_ring_config(struct dngl_bus *pciedev, void *p)
{
	ring_config_req_t *ring_config_req = (ring_config_req_t *)p;
	uint16 subtype = ring_config_req->subtype;

	switch (subtype) {
#if defined(PCIE_D2H_DOORBELL_RINGER)
		case D2H_RING_CONFIG_SUBTYPE_SOFT_DOORBELL:
			pciedev_d2h_ring_config_soft_doorbell(pciedev, ring_config_req);
			break;
#endif /* PCIE_D2H_DOORBELL_RINGER */

#ifdef BCMPCIE_D2H_MSI
		case D2H_RING_CONFIG_SUBTYPE_MSI_DOORBELL:
			pciedev_d2h_ring_config_msi(pciedev, ring_config_req);
			break;
#endif /* BCMPCIE_D2H_MSI */

		default:
			PCI_TRACE(("Unknown subtype %d in ring_config_req\n", subtype));
			break;
	}
}

static uint8
pciedev_dump_lclbuf_pool(msgbuf_ring_t * ring)
{
	uint8 i = 0;
	lcl_buf_pool_t * pool = ring->buf_pool;

	PCI_ERROR(("Ring name: %c%c%c%c  Head %x tail %x free %x \n",
		ring->name[0], ring->name[1], ring->name[2], ring->name[3],
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
pciedev_check_free_p(msgbuf_ring_t * ring, lcl_buf_t *lcl_free, void *p, const char *caller)
{
	while (lcl_free != NULL) {
		if (lcl_free->p == p) {
			PCI_TRACE(("caller: %c%c%c%c: freeing",
				"up the same p again, %p\n",
				caller[0], caller[1], caller[2], caller[3], p));
			pciedev_dump_lclbuf_pool(ring);
		}
		lcl_free = lcl_free->nxt;
	}
}
#endif /* PCIEDEV_DBG_LOCAL_POOL */

static int
pciedev_dump_flow_ring(msgbuf_ring_t *ring)
{
	cir_buf_pool_t *cir_buf_pool = ring->cbuf_pool;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 availcnt = CIR_BUFPOOL_AVAILABLE(cir_buf_pool);
	bcm_ether_ntoa((struct ether_addr *)ring->flow_info.da, eabuf);

	BCM_REFERENCE(availcnt);

	PCI_ERROR((
		"ring: %c%c%c%c, ID %d, phase %d, rd_ptr 0x%08x, wr_ptr 0x%08x\n",
		ring->name[0], ring->name[1], ring->name[2], ring->name[3],
		ring->ringid, ring->current_phase,
		(uint32)MSGBUF_RD_PTR(ring), (uint32)MSGBUF_WR_PTR(ring)));

	PCI_ERROR(("pcie_ipc_ring_mem 0x%08x" HADDR64_FMT "\n",
		(uint32)MSGBUF_IPC_RING_MEM(ring), HADDR64_VAL(MSGBUF_HADDR64(ring))));

	PCI_ERROR((
		"Flow Init %d Addr :%s tid/ac=%d if=%d status=%d handle=%d\n",
		ring->inited, eabuf, ring->flow_info.tid_ac, ring->flow_info.ifindex,
		ring->status, ring->handle));

	PCI_ERROR(("RP %d RDp %d WR %d WRp %d\n", MSGBUF_RD(ring), ring->fetch_ptr,
		MSGBUF_WR(ring), MSGBUF_WR_PEND(ring)));
	PCI_ERROR(("d2h_q_txs_pending %d\n", ring->d2h_q_txs_pending));
	PCI_ERROR(("Total count: %d, Avaialble count %d\n", cir_buf_pool->depth, availcnt));
	PCI_ERROR(("Buffer start address: 0x%08x, Read index: %d, Write index: %d\n",
		(uint32)cir_buf_pool->buf, cir_buf_pool->r_ptr, cir_buf_pool->w_ptr));

	PCI_ERROR(("-----------------------------------------\n"));
	return 0;
}

static int /* Dump the active prio rings in the flowring scheduler */
pciedev_dump_prio_rings(struct dngl_bus * pciedev)
{
	struct dll * cur, *prio;
	msgbuf_ring_t *ring;
	uint8 i = 0;

	PCI_ERROR(("\n\n******************************************\n"));
	PCI_ERROR(("Interface list stats : \n"));
	for (i = 0; i < pciedev->max_slave_devs; i++) {
		PCI_ERROR(("Ifidx %d :   max count %d   cur count %d \n",
			i, pciedev->ifidx_account[i].max_cnt, pciedev->ifidx_account[i].cur_cnt));
	}

	prio = dll_head_p(&pciedev->active_prioring_list);
	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		prioring_pool_t *prioring = (prioring_pool_t *)prio;

		cur = dll_head_p(&prioring->active_flowring_list);
		while (!dll_end(cur, &prioring->active_flowring_list)) {
			char eabuf[ETHER_ADDR_STR_LEN];
			ring = ((flowring_pool_t *)cur)->ring;
			bcm_ether_ntoa((struct ether_addr *)ring->flow_info.da, eabuf);

			PCI_ERROR(("Ring: %c%c%c%c, ID %d, tid_ac %d, ifidx %d\n",
				ring->name[0], ring->name[1], ring->name[2], ring->name[3],
				ring->ringid, ring->flow_info.tid_ac,
				ring->flow_info.ifindex));

			PCI_ERROR(("Flow rate %d Max Flow rate %d Max pkts %d \n",
				ring->flow_info.max_rate, pciedev->txflows_max_rate,
				ring->flow_info.pktinflight_max));

			PCI_ERROR(("\tMax fetch count %d Pktinflight %d Fetch pending %d \n",
				ring->flow_info.maxpktcnt, ring->flow_info.pktinflight,
				ring->fetch_pending));

			PCI_ERROR(("\tAddr :%s, RP %d RDp %d WR %d\n\n",
				eabuf, MSGBUF_RD(ring), ring->fetch_ptr, MSGBUF_WR(ring)));

			cur = dll_next_p(cur);
		}
		PCI_ERROR(("+++++++++++++++++++++++++++++++++++++++++\n"));

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}

	return BCME_OK;

} /* pciedev_dump_prio_rings */

static int
pciedev_dump_rw_indices(struct dngl_bus * pciedev)
{
	uint32 i;

	for (i = 0; i < BCMPCIE_H2D_COMMON_MSGRINGS; i++) {
		PCI_ERROR(("ID:%d RD %d : WR: %d\n", i,
			MSGBUF_RD(&pciedev->cmn_msgbuf_ring[i]),
			MSGBUF_WR(&pciedev->cmn_msgbuf_ring[i])));
	}

	return BCME_OK;

} /* pciedev_dump_rw_indices */

/** Free up inuse pool info */
static void
BCMATTACHFN(pciedev_free_inuse_bufpool)(struct dngl_bus *pciedev, lcl_buf_pool_t * bufpool)
{
	inuse_lclbuf_pool_t * inuse = bufpool->inuse_pool;
	uint16 size = 0;
	uint8 bufcnt = bufpool->buf_cnt + 1;

	size = sizeof(inuse_lclbuf_pool_t) + bufcnt * sizeof(inuse_lcl_buf_t);
	if (inuse)
		MFREE(pciedev->osh, inuse, size);
}

/** Initialise inuse pool */
static void
BCMATTACHFN(pciedev_init_inuse_lclbuf_pool_t)(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	inuse_lclbuf_pool_t * pool = NULL;
	uint16 size = 0;

	/* read/write pointer requires to allocate one element extra */
	/* read/write pointer mechanism has lower mem requirement than a linked list */
	uint8 bufcnt = ring->buf_pool->buf_cnt + 1;

	size = sizeof(inuse_lclbuf_pool_t) + bufcnt * sizeof(inuse_lcl_buf_t);
	pool = MALLOCZ(pciedev->osh, size);
	if (pool == NULL) {
		PCI_ERROR(("Could not allocate pool\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		DBG_BUS_INC(pciedev, pciedev_init_inuse_lclbuf_pool_t);
		return;
	}
	PCI_TRACE(("ring %c%c%c%c : bufcnt %d size %d \n",
		ring->name[0], ring->name[1], ring->name[2], ring->name[3], bufcnt, size));

	/* initialise depth */
	pool->depth = bufcnt;

	/* pool inited */
	pool->inited = 1;

	ring->buf_pool->inuse_pool = pool;
}

#ifdef BCMPOOLRECLAIM
/**
 * Free up inuse pool info needed by poolreclaim.
 * This function is used when pktpool_reclaim() is used.
 */
void
pciedev_free_poolreclaim_inuse_bufpool(struct dngl_bus *pciedev, inuse_lclbuf_pool_t *inuse_pool)
{
	uint16 size = 0;

	if (inuse_pool) {
		size = sizeof(inuse_lclbuf_pool_t) + inuse_pool->depth * sizeof(inuse_lcl_buf_t);
		MFREE(pciedev->osh, inuse_pool, size);
	}
}

/**
 * Initialise inuse pool needed by poolreclaim.
 * This function is used when pktpool_reclaim() is used.
 */
int
pciedev_init_poolreclaim_inuse_bufpool(struct dngl_bus *pciedev, msgbuf_ring_t *ring)
{
	inuse_lclbuf_pool_t *pool = NULL;
	uint16 size = 0;

	/* read/write pointer requires to allocate one element extra */
	/* read/write pointer mechanism has lower mem requirement than a linked list */
	uint8 bufcnt = PCIE_POOLRECLAIM_LCLBUF_DEPTH + 1;

	size = sizeof(inuse_lclbuf_pool_t) + bufcnt * sizeof(inuse_lcl_buf_t);
	pool = MALLOCZ(pciedev->osh, size);
	if (pool == NULL) {
		DBG_BUS_INC(pciedev, pciedev_init_poolreclaim_inuse_bufpool);
		PCI_ERROR(("Could not allocate pool\n"));
		PCIEDEV_MALLOC_ERR_INCR(pciedev);
		return BCME_NOMEM;
	}
	PCI_TRACE(("ring %c%c%c%c : bufcnt %d size %d \n",
		ring->name[0], ring->name[1], ring->name[2], ring->name[3], bufcnt, size));

	/* initialise depth */
	pool->depth = bufcnt;

	/* pool inited */
	pool->inited = TRUE;

	pciedev->htod_rx_poolreclaim_inuse_pool = pool;

	return BCME_OK;
}
#endif /* BCMPOOLRECLAIM */

#ifdef PCIE_DEEP_SLEEP
bool
pciedev_can_goto_deepsleep(struct dngl_bus * pciedev)
{
	uint8 i;

	if (pciedev->cur_ltr_state != LTR_SLEEP) {
		PCI_TRACE(("LTR state:%d\n", pciedev->cur_ltr_state));
		pciedev_ds_check_fail_log(pciedev, DS_LTR_CHECK);
		return FALSE;
	}
	/* Make sure all txstatus due to MAC DMA FLUSH are taken care of */
	if (PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev)) {
		PCI_TRACE(("RX: pend count is %d\n", PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev)));
		PCIEDEV_XMIT_RXCOMPLETE(pciedev);
	}
	if (PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev)) {
		PCI_TRACE(("TX: pend count is %d\n", PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev)));
		PCIEDEV_XMIT_TXSTATUS(pciedev);
	}
	if (PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev) != 0 ||
		PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev) != 0) {
		PCI_TRACE(("rxcpl item:%d, txcpl item:%d\n",
			PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev),
			PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev)));
		pciedev_ds_check_fail_log(pciedev, DS_BUFPOOL_PEND_CHECK);
		return FALSE;
	}

	for (i = 0; i < MAX_DMA_CHAN; i++) {
		if (VALID_DMA_CHAN_NO_IDMA(i, pciedev) &&
			pciedev->h2d_di[i] && pciedev->d2h_di[i]) {
			/* TBD : In case it needs to protect Implicit DMA as well,
			 * one way is to add implicit dma status flag
			 * which is set by Host and cleared by Dev
			 */
			if (dma_txactive(pciedev->h2d_di[i]) ||
				dma_rxactive(pciedev->h2d_di[i]) ||
				dma_txactive(pciedev->d2h_di[i]) ||
				dma_rxactive(pciedev->d2h_di[i]))
			{
				PCI_TRACE(("M2M DMA H2D_TX %d, RX %d, D2H_TX: %d, RX: %d\n",
					dma_txactive(pciedev->h2d_di[i]),
					dma_rxactive(pciedev->h2d_di[i]),
					dma_txactive(pciedev->d2h_di[i]),
					dma_rxactive(pciedev->d2h_di[i])));
				pciedev_ds_check_fail_log(pciedev, DS_DMA_PEND_CHECK);
				return FALSE;
			}
		}
	}

#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ACTIVE(pciedev)) {
		if (pciedev->idma_dmaxfer_active) {
			PCI_TRACE(("M2M H2D Implicit DMA is active\n"));
			pciedev_ds_check_fail_log(pciedev, DS_IDMA_PEND_CHECK);
			return FALSE;
		}
	}
#endif /* BCMPCIE_IDMA */

	return TRUE;
}

static void
pciedev_ds_check_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) hnd_timer_get_ctx(t);

	if (pciedev_ds_in_host_sleep(pciedev)) {
		if (pciedev->ds_check_timer_on) {
			dngl_del_timer(pciedev->ds_check_timer);
			pciedev->ds_check_timer_on = FALSE;
			pciedev->ds_check_timer_max = 0;
		}
		PCI_TRACE(("pciedev_ds_check_timerfn: timer done:%d!\n",
			pciedev->ds_check_timer_max));
		return;
	}
	if (pciedev_can_goto_deepsleep(pciedev)) {
		if (pciedev->ds_check_timer_on) {
			dngl_del_timer(pciedev->ds_check_timer);
			pciedev->ds_check_timer_on = FALSE;
			pciedev->ds_check_timer_max = 0;
		}
		PCI_TRACE(("pciedev_ds_check_timerfn: sending deepsleep req to host\n"));
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported) {
			pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_DEVICE_SLEEP_INFORM);
		}
		else
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
		if (pciedev->ds_oob_dw_supported) {
			pciedev_deepsleep_enter_req(pciedev);
		}
	} else {
		PCI_TRACE(("pciedev_ds_check_timerfn: restart timer to retry.\n"));
		pciedev->ds_check_timer_max ++;
		dngl_add_timer(pciedev->ds_check_timer,
			pciedev->ds_check_interval, FALSE);
	}
}

static int
pciedev_deepsleep_engine_log_dump(struct dngl_bus * pciedev)
{
	int i;
	if (pciedev->ds_state == DS_DISABLED_STATE)
		return BCME_ERROR;
	for (i = 0; i < PCIE_MAX_DS_LOG_ENTRY; i++) {
		if (pciedev->ds_log[i].ds_state == DS_DISABLED_STATE)
			break;
		if (pciedev->ds_log[i].ds_time == 0)
			continue;
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported) {
			PCI_PRINT(("0x%x:State:%s Event:%s Transition:%s\n",
				pciedev->ds_log[i].ds_time,
				pciedev_ib_ds_state_name(pciedev->ds_log[i].ds_state),
				pciedev_ib_ds_event_name(pciedev->ds_log[i].ds_event),
				pciedev_ib_ds_state_name(pciedev->ds_log[i].ds_transition)));
		} else
#endif /* PCIEDEV_INBAND_DW */
		{
			PCI_PRINT(("0x%x:State:%s Event:%s Transition:%s\n",
				pciedev->ds_log[i].ds_time,
				pciedev_ob_ds_state_name(pciedev->ds_log[i].ds_state),
				pciedev_ob_ds_event_name(pciedev->ds_log[i].ds_event),
				pciedev_ob_ds_state_name(pciedev->ds_log[i].ds_transition)));
		}
	}
	return BCME_OK;
}
#endif /* PCIE_DEEP_SLEEP */

static void
pcie_init_mrrs(struct dngl_bus *pciedev, uint32 mrrs)
{
	uint32 devctl, mr;

	switch (mrrs) {
	case 128:
		mr = 0;
		break;
	case 256:
		mr = 1;
		break;
	case 512:
		mr = 2;
		break;
	case 1024:
		/* intentional fall through */
	default:
		mr = 3;
		break;
	}
	devctl = R_REG_CFG(pciedev, PCIECFGREG_DEVCONTROL);
	devctl &= ~PCIECFGREG_DEVCONTROL_MRRS_MASK;
	devctl |= mr << PCIECFGREG_DEVCONTROL_MRRS_SHFT;
	W_REG_CFG(pciedev, PCIECFGREG_DEVCONTROL, devctl);
}

/**
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
uint32
pcie_h2ddma_tx_get_burstlen(struct dngl_bus *pciedev)
{
	uint32 burstlen = 2; /* 64B */
	uint32 mrrs;

	mrrs = (R_REG_CFG(pciedev, PCIECFGREG_DEVCONTROL)
		& PCIECFGREG_DEVCONTROL_MRRS_MASK) >>
		PCIECFGREG_DEVCONTROL_MRRS_SHFT;

	switch (mrrs) {
		case PCIE_CAP_DEVCTRL_MRRS_128B:
			burstlen = DMA_BL_128; /* 128B */
		break;
		case PCIE_CAP_DEVCTRL_MRRS_256B:
			burstlen = DMA_BL_256; /* 256B */
		break;
		case PCIE_CAP_DEVCTRL_MRRS_512B:
			burstlen = DMA_BL_512; /* 512B */
		break;
		case PCIE_CAP_DEVCTRL_MRRS_1024B:
			burstlen = DMA_BL_1024; /* 1024B */
		break;
		default:
			burstlen = DMA_BL_1024; /* 1024B */

	}
	return burstlen;
}

uint32
pcie_h2ddma_rx_get_burstlen(struct dngl_bus *pciedev)
{
	uint32 burstlen = 2; /* 64B */
	uint32 mps;

	mps = (R_REG_CFG(pciedev, PCIECFGREG_DEVCONTROL)
		& PCIECFGREG_DEVCTRL_MPS_MASK) >>
		PCIECFGREG_DEVCTRL_MPS_SHFT;

	switch (mps) {
		case PCIE_CAP_DEVCTRL_MPS_128B:
			burstlen = DMA_BL_128; /* 128B */
		break;
		case PCIE_CAP_DEVCTRL_MPS_256B:
			burstlen = DMA_BL_256; /* 256B */
		break;
		case PCIE_CAP_DEVCTRL_MPS_512B:
			burstlen = DMA_BL_512; /* 512B */
		break;
		case PCIE_CAP_DEVCTRL_MPS_1024B:
			burstlen = DMA_BL_1024; /* 1024B */
		break;
		default:
			burstlen = DMA_BL_1024; /* 1024B */
	}

	return burstlen;
}

static void
pciedev_crwlpciegen2_182(struct dngl_bus *pciedev)
{
	if (!HOSTREADY_ONLY_ENAB()) {
		if (pciedev->db1_for_mb == PCIE_MB_INTR_ONLY &&
			PCIECOREREV(pciedev->sih->buscorerev) <= 13) {
			/* XXX CRWLPCIEGEN2-182: Send a fake mailbox interrupt so that we never
			* miss a real mailbox interrupt from host.
			*/
			W_REG_CFG(pciedev, PCISBMbx, 1 << 0);
		}
	}
}

/** Set copycount and d11 rxoffset coming from wl layer */
void
pciedev_set_copycount_bytes(struct dngl_bus *pciedev, uint32 copycount, uint32 d11rxoffset)
{
#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	if (pciedev->monitor_mode && !RXFIFO_SPLIT()) {
		pciedev->copycount = 0;
	}
	else
#endif // endif
	{
		pciedev->copycount = (copycount * 4) + d11rxoffset;
	}
	pciedev->d11rxoffset = d11rxoffset;
}

#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
void
pciedev_set_monitor_mode(struct dngl_bus *pciedev, uint32 monitor_mode)
{
		pciedev->monitor_mode = monitor_mode;
}
#endif /* WL_MONITOR && WL_MONITOR_DISABLED */

/**
 *
 * Log the deepsleep enter precondition status
 *
 */
void
pciedev_ds_check_fail_log(struct dngl_bus *pciedev, ds_check_fail_t ds_check_fail)
{
	ds_check_fail_log_t * ds_check_fail_log;
	uint32 log_ind = pciedev->ds_log_count ?
		(pciedev->ds_log_count - 1) : (PCIE_MAX_DS_LOG_ENTRY  - 1);

	PCI_TRACE(("%s:%d:%d: type:%d\n",
		__FUNCTION__, pciedev->ds_check_timer_max, log_ind, ds_check_fail));

	ds_check_fail_log = &pciedev->ds_log[log_ind].ds_check_fail_cntrs;

	switch (ds_check_fail) {
		case DS_LTR_CHECK:
			ds_check_fail_log->ltr_check_fail_ct++;
		break;
		case DS_BUFPOOL_PEND_CHECK:
			if (PCIEDEV_RXCPL_PEND_ITEM_CNT(pciedev))
				ds_check_fail_log->rx_pool_pend_fail_ct++;
			if (PCIEDEV_TXCPL_PEND_ITEM_CNT(pciedev))
				ds_check_fail_log->tx_pool_pend_fail_ct++;
		break;
		case DS_DMA_PEND_CHECK:
			if (dma_txactive(pciedev->h2d_di[pciedev->default_dma_ch]))
				ds_check_fail_log->h2d_dma_txactive_fail_ct++;
			if (dma_rxactive(pciedev->h2d_di[pciedev->default_dma_ch]))
				ds_check_fail_log->h2d_dma_rxactive_fail_ct++;
			if (dma_txactive(pciedev->d2h_di[pciedev->default_dma_ch]))
				ds_check_fail_log->d2h_dma_txactive_fail_ct++;
			if (dma_rxactive(pciedev->d2h_di[pciedev->default_dma_ch]))
				ds_check_fail_log->d2h_dma_rxactive_fail_ct++;
		break;
		case DS_IDMA_PEND_CHECK:
			ds_check_fail_log->h2d2_dma_txactive_fail_ct++;
		break;
		default:
			PCI_PRINT(("Unknown type. Cannot log\n"));
			return;
	}
}

void
pciedev_sr_stats(struct dngl_bus *pciedev, bool arm_wakeup)
{
	if (arm_wakeup) {
		pciedev->sr_count ++;
		++pciedev->metrics->deepsleep_count;
		pciedev->metrics->deepsleep_dur +=
			OSL_SYSUPTIME() - pciedev->metric_ref->deepsleep;
	} else {
		pciedev->metric_ref->deepsleep = OSL_SYSUPTIME();
	}
}

/**
 *
 * Wake_On_PERST bit needs to be enabled before entering
 * into D3cold i.e. before host asserts PERST# so that
 * chip can go to SR deepsleep in D3cold.
 * This bit needs to be disabled after PERST# is de-asserted
 * so that chip can go to SR deepsleep in L1.2 state.
 *
 */
void
pciedev_WOP_disable(struct dngl_bus *pciedev, bool disable)
{
	switch (PCIECOREREV(pciedev->corerev)) {
		case 7:
			si_pmu_chipcontrol(pciedev->sih,
				PMU_CHIPCTL7, CC7_4350_PMU_EN_ASSERT_L2_MASK,
				disable ? 0 : CC7_4350_PMU_EN_ASSERT_L2_MASK);
		break;
		case 9:
			si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL2,
				PMU43602_CC2_PCIE_PERST_L_WAKE_EN,
				disable ? 0 : PMU43602_CC2_PCIE_PERST_L_WAKE_EN);
			if (disable) {
				AND_REG(pciedev->osh,
					(PCIE_pciecontrol(pciedev->autoregs)), ~PCIE_WakeModeL2);
			} else {
				OR_REG(pciedev->osh,
					(PCIE_pciecontrol(pciedev->autoregs)), PCIE_WakeModeL2);
			}
		break;
		case 11:
			/* 4350Cx: Clear bit18 of chipcontrol6 so that chip can go to
			* SR deepsleep in L1ss.
			*/
			si_pmu_chipcontrol(pciedev->sih, PMU_CHIPCTL6,
				CC6_4350_PMU_EN_WAKEUP_MASK,
				disable ? 0 : CC6_4350_PMU_EN_WAKEUP_MASK);
		break;
		case 12:
		case 14:
			/* 4345: Clear bit14 of chipcontrol6 */
			si_pmu_chipcontrol(pciedev->sih,
				PMU_CHIPCTL6,
				CC6_4345_PMU_EN_L2_DEASSERT_MASK,
				disable ? 0 : CC6_4345_PMU_EN_L2_DEASSERT_MASK);
		break;
		case 16: /* 4364 */
		case 13: /* 4349 based */
		case 17:
		case 21:
			/* Clear bit14 of chipcontrol6 */
			si_pmu_chipcontrol(pciedev->sih,
				PMU_CHIPCTL6,
				CC6_4349_PMU_EN_L2_DEASSERT_MASK,
				disable ? 0 : CC6_4349_PMU_EN_L2_DEASSERT_MASK);
			si_pmu_chipcontrol(pciedev->sih,
				PMU_CHIPCTL6,
				CC6_4349_PMU_EN_ASSERT_L2_MASK,
				disable ? 0 : CC6_4349_PMU_EN_ASSERT_L2_MASK);
		break;
		case 23: /* 4347b0 */
		case 24: /* 4369 */
			if (disable) {
				AND_REG(pciedev->osh,
					(PCIE_pciecontrol(pciedev->autoregs)), ~PCIE_WakeModeL2);
			} else {
				OR_REG(pciedev->osh,
					(PCIE_pciecontrol(pciedev->autoregs)), PCIE_WakeModeL2);
			}
		break;
		default:
			PCI_PRINT(("WOP is not enabled for this chip\n"));
		break;
	}
}
#if defined(SAVERESTORE)
/**
 * This callback gets called before ARM goes to WFI. It executes
 * the routine to update the counters related to deepsleep.
 */
static bool
pciedev_sr_save(void *arg)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)arg;
	pciedev_sr_stats(pciedev, FALSE);

#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ACTIVE(pciedev) && PCIE_IDMA_DS(pciedev)) {
		if (!pciedev->ds_oob_dw_supported && !pciedev->ds_inband_dw_supported) {
			pciedev_idma_channel_enable(pciedev, FALSE);
		}
	}
#endif /* BCMPCIE_IDMA */

	return TRUE;
}

/**
 * This callback gets called after ARM wakes from deeepsleep.
 * It executes the routine to update the counters related to
 * deepsleep.
 */
static void
pciedev_sr_restore(void *arg, bool wake_from_deepsleep)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)arg;
	pciedev_sr_stats(pciedev, wake_from_deepsleep);
}
#endif /* SAVERESTORE */
/** get optimal pkts per flow/ifidx */
static void
pciedev_flow_get_opt_count(struct dngl_bus * pciedev)
{
	uint8 ifindex;
	int i;
	msgbuf_ring_t *flow_ring;
	uint16 cur_rate = 0;
	uint16 optimal_pkts = 0;
	uint16 max_rate = pciedev->txflows_max_rate;
	uint16 pkts_max = 0;

#ifdef BCMFRAGPOOL
	pkts_max = pktpool_max_pkts(pciedev->pktpool_lfrag);
#endif // endif
#if defined(BCMHWA) && defined(HWA_TXPOST_BUILD)
	pkts_max = HWA_TXPATH_PKTS_MAX;
#endif // endif
#ifdef BCMRESVFRAGPOOL
	if (BCMRESVFRAGPOOL_ENAB())
		pkts_max += rsvpool_maxlen(pciedev->pktpool_resv_lfrag);
#endif // endif

	/* reset ifidx max counts */
	for (i = 0; i < pciedev->max_slave_devs; i++) {
		pciedev->ifidx_account[i].max_cnt = 0;
	}
	for (i = 0; i < pciedev->max_flowrings; i++) {
		flow_ring = &pciedev->flowrings_table[i];
		if (!flow_ring->inited)
			continue;
		cur_rate = flow_ring->flow_info.max_rate;

		/* Estimate optimal pkts if max rate is defined */
		if (max_rate)
			optimal_pkts = (pkts_max  * cur_rate / max_rate);

		optimal_pkts = MAX(FLOWCTRL_MIN_PKTCNT, optimal_pkts);

		flow_ring->flow_info.pktinflight_max = optimal_pkts;
		/* Update per ifidx max count */
		ifindex = flow_ring->flow_info.ifindex;

		/* Evaluate max count per interface
		*  Max of all flows per interface
		*/
		if (optimal_pkts > pciedev->ifidx_account[ifindex].max_cnt)
			pciedev->ifidx_account[ifindex].max_cnt = optimal_pkts;

		PCI_TRACE(("Ring id %d  TID_AC %d Cur rate %d Max rate %d Optimal pkts %d \n",
			flow_ring->ringid, flow_ring->flow_info.tid_ac,
			flow_ring->flow_info.max_rate, pciedev->txflows_max_rate, optimal_pkts));
	}
}

/** Estimate max no of packets reserved for each flow */
void
pciedev_update_txflow_pkts_max(struct dngl_bus * pciedev)
{
	int i;
	uint16 max_rate = 0;
	int16 flow_rate = 0;
	msgbuf_ring_t *flow_ring;
	for (i = 0; i < pciedev->max_flowrings; i++) {
		flow_ring = &pciedev->flowrings_table[i];
		if (flow_ring->inited) {
			/*  read max phy rate from WL layer */
			flow_rate = dngl_flowring_update(pciedev->dngl,
				flow_ring->flow_info.ifindex, 0, FLOW_RING_GET_PKT_MAX,
				(uint8*)flow_ring->flow_info.sa,
				(uint8*)flow_ring->flow_info.da,
				(pciedev->schedule_prio) ?
				flow_ring->flow_info.tid_ac | PCIEDEV_IS_AC_TID_MAP_MASK :
				flow_ring->flow_info.tid_ac, NULL);

			/* Update per flow max rate */
			if (flow_rate >= 0)
				flow_ring->flow_info.max_rate = flow_rate;

			max_rate = MAX(max_rate, flow_ring->flow_info.max_rate);
		}
	}

	/* update max rates across all flows */
	pciedev->txflows_max_rate = max_rate;

	/* Get optimal pkts per flow */
	pciedev_flow_get_opt_count(pciedev);
}

int
pciedev_sendctl_tx(struct dngl_bus *pciedev, uint8 type, uint32 op, void *p)
{
	cmn_msg_hdr_t *msg;
#ifdef PCIE_PWRMGMT_CHECK
	if ((pciedev->in_d3_suspend) && (pciedev->no_device_inited_d3_exit)) {
		PKTFREE(pciedev->osh, p, TRUE);
		return 0;
	}
#endif /* PCIE_PWRMGMT_CHECK */

	PCI_TRACE(("SENDCTL_TX\n"));
	switch (type) {
		case PCIEDEV_FIRMWARE_TSINFO_FIRST:
		case PCIEDEV_FIRMWARE_TSINFO_MIDDLE:
			if (PKTHEADROOM(pciedev->osh, p) < sizeof(cmn_msg_hdr_t)) {
				PKTFREE(pciedev->osh, p, TRUE);
				return 0;
			}
			PKTPUSH(pciedev->osh, p, sizeof(cmn_msg_hdr_t));
			msg = (cmn_msg_hdr_t *)PKTDATA(pciedev->osh, p);
			msg->msg_type = MSG_TYPE_FIRMWARE_TIMESTAMP;

			if (type == PCIEDEV_FIRMWARE_TSINFO_FIRST) {
				msg->flags = BCMPCIE_CMNHDR_FLAGS_TS_SEQNUM_INIT;
			}

			return (pciedev_create_d2h_messages(pciedev, p, pciedev->dtoh_ctrlcpl));
		default :
			PCI_ERROR(("Unknown operation type:%d, code:%d\n", type, op));
			break;
	}
	return 0;
}

static void
pciedev_initiate_dummy_fwtsinfo(struct dngl_bus *pciedev, uint32 val)
{
	pciedev->fwtsinfo_interval = val;
	PCI_PRINT(("%s val:%d\n", __FUNCTION__, val));
	if (pciedev->fwtsinfo_timer_active) {
		if (pciedev->fwtsinfo_interval == 0) {
			dngl_del_timer(pciedev->fwtsinfo_timer);
			pciedev->fwtsinfo_timer_active = FALSE;
		}
		return;
	}
	if (pciedev->fwtsinfo_interval == 0) {
		return;
	}
	dngl_add_timer(pciedev->fwtsinfo_timer, val * 1000, FALSE);
	pciedev->fwtsinfo_timer_active = TRUE;
}

static void
pciedev_fwtsinfo_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) hnd_timer_get_ctx(t);

	PCI_PRINT(("%s\n", __FUNCTION__));
	if (pciedev->fwtsinfo_interval) {
		pciedev_generate_dummy_fwtsinfo(pciedev);
		dngl_add_timer(pciedev->fwtsinfo_timer,
			pciedev->fwtsinfo_interval * 1000, FALSE);
		pciedev->fwtsinfo_timer_active = TRUE;
	} else {
		pciedev->fwtsinfo_timer_active = FALSE;
	}
}
static void
pciedev_generate_dummy_fwtsinfo(struct dngl_bus *pciedev)
{
	void *p;
	ts_fw_clock_info_t *buf;
	uint32 pktlen = (sizeof(cmn_msg_hdr_t) + sizeof(ts_fw_clock_info_t));

	if (pciedev->in_d3_suspend) {
		PCI_ERROR(("Bus not available:pciedev_generate_dummy_fwtsinfo\n"));
		return;
	}
	if (!pciedev->timesync) {
		PCI_ERROR(("TimeSync Feature is not supported by host\n"));
		return;
	}
	p = PKTGET(pciedev->osh, pktlen, TRUE);
	if (p == NULL) {
		PCI_PRINT(("couldn't alloc info packet, so return\n"));
		return;
	}
	bzero(PKTDATA(pciedev->osh, p), pktlen);

	PKTSETLEN(pciedev->osh, p, pktlen);
	PKTPULL(pciedev->osh, p, sizeof(cmn_msg_hdr_t));

	buf = (ts_fw_clock_info_t *)PKTDATA(pciedev->osh, p);

	buf->xtlv.id = BCMMSGBUF_FW_CLOCK_INFO_TAG;
	buf->xtlv.len = sizeof(ts_fw_clock_info_t);
	buf->ts.ts_low = 0x12345678;
	buf->ts.ts_high = 0x8;
	strncpy((char *)(buf->clk_src), "ILP", strlen("ILP"));
	buf->nominal_clock_freq = 0x11;

#ifdef TIMESYNC_GPIO
	PCI_PRINT(("Toggle Timesync GPIO\n"));
	/* Tgoggle Timesync GPIO for 10 ms */
	pciedev_time_sync_gpio_enable(pciedev, TRUE);
	OSL_DELAY(10000); /* 10 ms of pulse width */
	pciedev_time_sync_gpio_enable(pciedev, FALSE);
#endif /* TIMESYNC_GPIO */

	PCI_PRINT(("sending info message to host\n"));
	pciedev_sendctl_tx(pciedev, PCIEDEV_FIRMWARE_TSINFO_MIDDLE, 0, p);
}

static void
pciedev_dump_ts_pool(struct dngl_bus *pciedev)
{
	host_dma_buf_t *local;
	int count = 0;
	host_dma_buf_pool_t *pool = pciedev->ts_pool;

	if (!pciedev->timesync) {
		PCI_ERROR(("TimeSync Feature is not supported by host\n"));
		return;
	}
	PCI_PRINT(("pool %x, ready is %x, ready_cnt:%d\n",
		(uint32)pool, (uint32)pool->ready, pool->ready_cnt));
	if (pool->ready == NULL || pool->ready_cnt == 0) {
		PCI_PRINT(("TS POOL is EMPTY!!!\n"));
		return;
	}
	local = pool->ready;
	while (local) {
		PCI_PRINT(("%d:Local Addr Low:0x%x, Local Addr Hi:0x%x, Pktid:0x%x\n",
			count ++,
			local->buf_addr.low_addr,
			local->buf_addr.high_addr,
			local->pktid));
		local = local->next;
	}
}

static void
pciedev_dump_last_host_ts(struct dngl_bus *pciedev)
{
	if (!pciedev->timesync) {
		PCI_ERROR(("TimeSync Feature is not supported by host\n"));
		return;
	}

	if (pciedev->ipc_data_supported) {
		if (pciedev->ipc_data->last_ts_proc_time) {
		PCI_PRINT(("%d:%d::msg_type:0x%02x, if_id:0x%02x, flags:0x%02x, "
			"req_id:0x%x, xt_id:0x%04x, input_data_len:%d, seqnum:%d, "
			"addr_lo:0x%x, addr_hi:0x%x\n",
			pciedev->ipc_data->last_ts_proc_time,
			pciedev->ipc_data->last_ts_cmpl_time,
			pciedev->ipc_data->last_ts_work_item.msg.msg_type,
			pciedev->ipc_data->last_ts_work_item.msg.if_id,
			pciedev->ipc_data->last_ts_work_item.msg.flags,
			pciedev->ipc_data->last_ts_work_item.msg.request_id,
			pciedev->ipc_data->last_ts_work_item.xt_id,
			pciedev->ipc_data->last_ts_work_item.input_data_len,
			pciedev->ipc_data->last_ts_work_item.seqnum,
			pciedev->ipc_data->last_ts_work_item.host_buf_addr.low,
			pciedev->ipc_data->last_ts_work_item.host_buf_addr.high));
		}
	}
}

/**
 * This routine handles the IPC Rev7 defined Hostready interrupt from h2d doorbell 1
 * The very first DB1 interrupt is used by the host to signal end of common ring
 * initialization. Subsequent DB1 interrupts are used as indicator of D3 exit
 */
static void
pciedev_handle_hostready_intr(struct dngl_bus *pciedev)
{
	PCI_INFORM(("%s Hostready Enabled?%s\n", __FUNCTION__, pciedev->hostready ? "Y":"N"));

	/* Record the time */
	pciedev->bus_counters->hostready_cnt++;
	pciedev->bus_counters->last_hostready = OSL_SYSUPTIME();

	/* Host indicated common ring initialization complete */
	if (pciedev->common_rings_attached == FALSE) {

		if (pciedev_link_pcie_ipc(pciedev) == BCME_OK) {
			pciedev->ipc_data->cmn_rings_attach_time =
				pciedev->bus_counters->last_hostready;
			pciedev->common_rings_attached = TRUE;

#ifdef PCIE_DEEP_SLEEP
			if (pciedev->ds_oob_dw_supported) {
				/* Now, trigger DS Engine */
				pciedev_trigger_deepsleep_dw(pciedev);
			}
#endif /* PCIE_DEEP_SLEEP */
		}
	}
	else {
		/* Make sure hostready is enabled */
		if (!pciedev->hostready)
			return;
		/* Make sure PCIE bus is in D3 */
		if (!pciedev_ds_in_host_sleep(pciedev)) {
			/* If this is the first Hostready and rings
			* already initialized, just return.
			*/
			if (pciedev->bus_counters->hostready_cnt == 1)
				return;
			PCI_ERROR(("HostReady recieved without D3-INFORM\n"));
			OSL_SYS_HALT();
		}
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
		if (pciedev->ds_inband_dw_supported) {
			pciedev_ib_deepsleep_engine(pciedev, DS_EVENT_ACTIVE);
		}
		else
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
		{
			if (pciedev->pciecoreset == FALSE) {
				/* D3hot case */
				pciedev_handle_d0_enter(pciedev);
			} else {
				/* D3cold case */
				pciedev_WOP_disable(pciedev, TRUE);
				pciedev_handle_l0_enter(pciedev);
			}
			pciedev_enable_powerstate_interrupts(pciedev);
#ifdef PCIE_DEEP_SLEEP
			/* DS state change based on last DW state */
			if (pciedev->ds_oob_dw_supported) {
				if (pciedev->dw_counters.dw_state) {
					pciedev_ob_deepsleep_engine(pciedev, DW_ASSRT_EVENT);
				}
				else {
					/* Kick off DS state machine to transition to deepsleep */
					pciedev_ob_deepsleep_engine(pciedev, DW_DASSRT_EVENT);
				}
			}
#endif /* PCIE_DEEP_SLEEP */
		}
	}
}

static int
pciedev_gpio_signal_test(struct dngl_bus *pciedev, uint8 gpio_num, bool set, uint32 *pval)
{
	uint8 signal_test_gpio = gpio_num;
	uint32 val = 0;

	if (signal_test_gpio == CC_GCI_GPIO_INVALID) {
		return BCME_UNSUPPORTED;
	}

	if (pval == NULL) {
		return BCME_NORESOURCE;
	} else {
		val = *pval;
	}

	if (set) {
		if (val != 0 && val != 1) {
			return BCME_BADARG;
		}
		if (val) {
			si_gci_host_wake_gpio_enable
				(pciedev->sih, signal_test_gpio, TRUE);
		} else {
			si_gci_host_wake_gpio_enable
				(pciedev->sih, signal_test_gpio, FALSE);
		}
	} else {
		*pval = (si_gpioin(pciedev->sih) & signal_test_gpio);
	}

	return BCME_OK;
}

#ifdef HEALTH_CHECK

/* read the config space error to make sure if the device has an AER report */
bool
pciedev_check_device_AER(struct dngl_bus *pciedev, uint32 *config_regs, uint32 size)
{
	int count = 0;
	uint32 uc_err_status;
	uint32 corr_err_status;
	uint32 tl_ctrl5_status;

	if (R_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs)) & PCIE_RST) {
		/* be able to access config during D3 with newer chips */
		if (!pciedev->in_d3_suspend) {
			uc_err_status = pciedev_read_clear_aer_uc_error_status(pciedev);
			if (uc_err_status != 0) {
				if (config_regs && count < size) {
					config_regs[count++] = uc_err_status;
				}
				PCI_PRINT(("device Uncorrectible error 0x%04x\n", uc_err_status));

				corr_err_status =
					pciedev_read_clear_aer_corr_error_status(pciedev);
				if (config_regs && count < size) {
					config_regs[count++] = corr_err_status;
				}
				PCI_PRINT(("device correctable error 0x%04x\n", corr_err_status));

				tl_ctrl5_status =
					pciedev_read_clear_tlcntrl_5_status(pciedev);
				if (config_regs && count < size) {
					config_regs[count++] = tl_ctrl5_status;
				}
				PCI_PRINT(("TL CTRL 5 status 0x%04x\n", tl_ctrl5_status));

				/* add the 4 TLP header regs to the config_regs context */
				pciedev_dump_aer_hdr_log(pciedev, config_regs, size, &count);

				return TRUE;
			}
		}
	}
	return FALSE;
}

/* Reset or restart the DS healthcheck counters */
static void
pciedev_ds_healthcheck_cntr_update(struct dngl_bus *pciedev, uint32 time_now, bool reset)
{
	uint32	ds_interval = 0;

	if (pciedev->hc_params == NULL) {
		return;
	}
	if (reset) {
		/* Reset the DS healthcheck counters. Take into account time wraps */
		if (pciedev->hc_params->last_ds_enable_time) {
			ds_interval = (time_now > pciedev->hc_params->last_ds_enable_time) ?
				(time_now - pciedev->hc_params->last_ds_enable_time) : 1;
		} else {
			ds_interval = 0;
		}
		pciedev->hc_params->total_ds_interval += ds_interval;
		/* If the reset happens within a ms after DS enabled, assign 1 */
		pciedev->hc_params->last_ds_interval = ds_interval ? ds_interval : 1;
		pciedev->hc_params->total_ds_sr_count +=
			pciedev->sr_count - pciedev->hc_params->last_ds_enable_sr_count;
	} else {
		/* Restart the DS healthcheck counters */
		pciedev->hc_params->last_ds_interval = 0;
		pciedev->hc_params->last_ds_enable_sr_count = pciedev->sr_count;
		pciedev->hc_params->last_ds_enable_time = time_now;
	}
}

/* read and clear the PCI_UC_ERR_STATUS config space register */
uint32
pciedev_read_clear_aer_uc_error_status(struct dngl_bus *pciedev)
{
	uint32 uc_err_status;

	uc_err_status = R_REG_CFG(pciedev, PCI_UC_ERR_STATUS);
	OR_REG_CFG(pciedev, PCI_UC_ERR_STATUS, uc_err_status);

	return uc_err_status;
}

/* read and clear the PCI_CORR_ERR_STATUS config space register */
uint32
pciedev_read_clear_aer_corr_error_status(struct dngl_bus *pciedev)
{
	uint32 corr_err_status;

	corr_err_status = R_REG_CFG(pciedev, PCI_CORR_ERR_STATUS);
	OR_REG_CFG(pciedev, PCI_CORR_ERR_STATUS, corr_err_status);

	return corr_err_status;
}

/* read and clear the PCI_CORR_ERR_STATUS config space register */
uint32
pciedev_read_clear_tlcntrl_5_status(struct dngl_bus *pciedev)
{
	uint32 tlcntrl_5_status;
	uint32 err_code_state;

	tlcntrl_5_status = R_REG_CFG(pciedev, PCI_CFG_TLCNTRL_5);
	OR_REG_CFG(pciedev, PCI_CFG_TLCNTRL_5, tlcntrl_5_status);

	err_code_state = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_code_logreg);
	OR_REG(pciedev->osh, &pciedev->regs->u.pcie2.err_code_logreg, err_code_state);

	return tlcntrl_5_status;
}

/* read and dump the 4 PCI_HDR_LOG config space registers */
#define MAX_TLP_HDR_REGS	4

void pciedev_dump_aer_hdr_log(struct dngl_bus *pciedev,
	uint32 *config_regs, uint32 size, int *count)
{
	uint32 tlp_hdr_data;
	uint32 tlp_hdr_addr, i;

	for (i = 0, tlp_hdr_addr = PCI_TLP_HDR_LOG1;
	     i < MAX_TLP_HDR_REGS;
	     i++, tlp_hdr_addr += sizeof(tlp_hdr_addr)) {
		tlp_hdr_data = R_REG_CFG(pciedev, tlp_hdr_addr);

		if ((config_regs != NULL) && (*count < size)) {
			config_regs[(*count)++] = tlp_hdr_data;
		}

		PCI_PRINT(("device TLP header log %d: 0x%8x\n", i + 1, tlp_hdr_data));
	}
}

/*
* Determine if chip not going to deepsleep
* Two possible scenarios when this routine gets to verify the deepsleep
* a) FW is already out of DS Enabled state : In this case, verify the
*    counter/interval of the last DS Enabled period
*
* b) FW is in the DS Enabled state: In this case, verify the deepsleep
*    counter/interval of the current DS Enabled period.
*
* The last_ds_interval captures how long chip was in the last DS Enabled interval.
* It is also used as a flag to indicate if the FW is still in DS Enabled
* state or not when the healthcheck ran. For scenario a) above, last_ds_interval
* will contain non zero value. However, healthcheck would only check the ds counter
* for scenario a) once. Therefore, it will set it to -1 if it is non-zero. In case,
* it is 0 i.e. scenario b), the counters/interval needs to be moved.
*
* The sum of continuous ds enabled period where sr count did not move is
* captured by the no_deepsleep_time_sum. It is used to check for three
* different thresholds and corresponding actions taken.
* 1) PCIEDEV_DS_CHECK_TIME_LO_THRESHOLD : If this threshold crosses, relevant
*    register dumps will be sent to UART and Event Log.
* 2) PCIEDEV_DS_CHECK_TIME_THRESHOLD: If this threshold crosses, SOCRAM
*    indication event will be generated and sent to host to capture SOCRAM dump.
* 3) PCIEDEV_NO_DS_TRAP_THRESHOLD: If this threshold crosses, FW TRAPs.
*/
static int
pciedev_chip_not_in_ds_too_long(void *arg,
	uint32 now, bool hnd_ds_hc, uint32 *regs, uint32 size,
	uint32 ds_check_lo_thr, uint32 ds_check_hi_thr)
{
	int count = 0;
	int i;
	uint32 ds_interval;
	uint32 ds_sr_count;
	uint32 current_ds_interval;

	struct dngl_bus *pciedev = (struct dngl_bus *)arg;

	if (pciedev->hc_params == NULL) {
		return 0;
	}
	/* If DS healthcheck is disabled or WL scan in progress, return with FALSE */
	if (pciedev->hc_params->disable_ds_hc) {
		pciedev->hc_params->total_ds_interval = 0;
		pciedev->hc_params->total_ds_sr_count = 0;
		pciedev->hc_params->no_deepsleep_time_sum = 0;
		pciedev->hc_params->last_ds_interval = -1;
		return 0;
	}
	/* Only run this healthcheck if last DS Enabled period is already taken into account */
	if (pciedev->hc_params->last_ds_interval == -1)
		return 0;
	/* Take into account time wrap */
	current_ds_interval = (now > pciedev->hc_params->last_ds_enable_time) ?
		(now -  pciedev->hc_params->last_ds_enable_time) : 1;
	ds_interval = pciedev->hc_params->last_ds_interval ?
		pciedev->hc_params->total_ds_interval :
		pciedev->hc_params->total_ds_interval + current_ds_interval;
	ds_sr_count = pciedev->hc_params->last_ds_interval ?
		pciedev->hc_params->total_ds_sr_count :
		pciedev->hc_params->total_ds_sr_count +
			(pciedev->sr_count - pciedev->hc_params->last_ds_enable_sr_count);

	/* Re-init the last_ds_interval if it is scenario a) above or move to current time for b) */
	pciedev->hc_params->total_ds_interval = 0;
	pciedev->hc_params->total_ds_sr_count = 0;
	if (pciedev->hc_params->last_ds_interval) {
		pciedev->hc_params->last_ds_interval = -1;
	}
	else {
		pciedev_ds_healthcheck_cntr_update(pciedev, now, FALSE);
	}
	/* If the chip went in deepsleep re-int the no deepsleep sum counter
	* Else check all relevant conditions before incrementing the deepsleeep total
	* sum counter. Make an exception for for L1ss disabled.
	* Make exception for force HT during h2d DMA.
	*/
	if (ds_sr_count == 0 &&
		!hnd_ds_hc &&
		!pciedev->ltr_sleep_after_d0 &&
		!pciedev->force_ht_on) {
		pciedev->hc_params->no_deepsleep_time_sum += ds_interval;
		pciedev->hc_params->no_deepsleep_rate_limit = FALSE;
	} else {
		pciedev->hc_params->no_deepsleep_rate_limit = TRUE;
	}
	if (ds_sr_count == 0 && hnd_health_check_ds_notification()) {
		PCI_PRINT(("DS is prevented due to :0x%x\n",
			hnd_health_check_ds_notification_dump()));
		pciedev->hc_params->ds_notification_count ++;
	}
	if (ds_sr_count == 0 && pciedev->force_ht_on) {
		PCI_PRINT(("DS is prevented due to HT\n"));
		pciedev->hc_params->ds_ht_count ++;
	}
	/* Reset all the counters if chip went to deepsleep at least once
	 * in last hc interval.
	*/
	if (ds_sr_count) {
		pciedev->hc_params->no_deepsleep_time_sum = 0;
		pciedev->hc_params->last_deepsleep_time_sum_reset = now;
		pciedev->hc_params->ds_notification_count = 0;
		pciedev->hc_params->ds_ht_count = 0;
	}
	/* Check if the SR count not moving. Ignore the case when the SR count
	 * did not move for valid reason
	*/
	if ((ds_sr_count == 0) &&
		!pciedev->hc_params->no_deepsleep_rate_limit &&
		(pciedev->hc_params->no_deepsleep_time_sum >= ds_check_lo_thr)) {
		/* Healthcheck buf pointer passed to retrieve reg dumps cannot be NULL */
		ASSERT(regs);

		/* Collect relevant wrapper registers */
		if (regs && count < size) {
			/* PCIE core */
			regs[count++] =
				si_core_wrapperreg(pciedev->sih,
					si_coreidx(pciedev->sih), OOB_ITOPOOBB, 0, 0);
		}
		if (regs && count < size) {
			/* Chipcommon core */
			regs[count++] =
				si_core_wrapperreg(pciedev->sih, SI_CC_IDX, OOB_ITOPOOBB, 0, 0);
		}
		if (regs && count < size) {
			/* D11 core */
			regs[count++] =
				si_core_wrapperreg(pciedev->sih,
				si_findcoreidx(pciedev->sih, D11_CORE_ID, 0), OOB_ITOPOOBB, 0, 0);
		}
		if (regs && count < size) {
			/* D11 secondary core */
			regs[count++] =
				si_core_wrapperreg(pciedev->sih,
				si_findcoreidx(pciedev->sih, D11_CORE_ID, 1), OOB_ITOPOOBB, 0, 0);
		}
		if (regs && count < size) {
			/* ARM core */
			regs[count++] =
				si_core_wrapperreg(pciedev->sih,
				si_findcoreidx(pciedev->sih, ARM_CORE_ID, 0), OOB_ITOPOOBB, 0, 0);
		}
		/* Get PCIe L1.2 histogram */
		if (!pciedev_ds_in_host_sleep(pciedev) && regs && count < size) {
			regs[count++] = R_REG_CFG(pciedev, PCIEGEN2_COE_PVT_PHY_DBG_CLKREQ_0);
		}
		if (!pciedev_ds_in_host_sleep(pciedev)) {
			pciedev_get_pwrstats(pciedev, NULL);
		}
		/* Get clk_ctl_st of PCIE core */
		if (regs && count < size) {
			regs[count++] = R_REG(pciedev->osh, &pciedev->regs->u.pcie2.clk_ctl_st);
		}
		/* Get clk_ctl_st of chipc core */
		if (regs && count < size) {
			regs[count++] = si_corereg(pciedev->sih,
				SI_CC_IDX, OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0);
		}
		/* Get clk_ctl_st of ARM core */
		if (regs && count < size) {
			regs[count++] = pciedev->ds_clk_ctl_st;
		}
		PCI_PRINT(("DS_STATE:%d LTR:%d L1ss:%d sr_count:%d,"
					" last_ds_sr_count:%d, last_ds_time:%d, nods_time:%d\n",
					pciedev->ds_state, pciedev->cur_ltr_state,
					pciedev->ltr_sleep_after_d0, pciedev->sr_count,
					pciedev->hc_params->last_ds_enable_sr_count,
					pciedev->hc_params->last_ds_enable_time,
					pciedev->hc_params->no_deepsleep_time_sum));

		PCIEDEV_DS_CHECK_LOG(pciedev, regs);
		for (i = 0; i < count; i++) {
			PCI_PRINT(("regs[%d] = 0x%x\n", i, regs[i]));
		}
		if (pciedev->hc_params->no_deepsleep_time_sum >= ds_check_hi_thr) {
			return (pciedev->hc_params->no_deepsleep_time_sum);
		}
	}
	return 0;
}
/*
* Determine if h2d DMA is delayed.
*/
static bool
pciedev_h2d_dma_taking_too_long(struct dngl_bus *pciedev, uint32 now, uint32 *rw_ind)
{
	int i;

	for (i = 0; i < MAX_DMA_CHAN; i++) {
		if (VALID_DMA_CHAN_NO_IDMA(i, pciedev) &&
			pciedev->htod_dma_q[i]) {
			int wr_idx = pciedev->htod_dma_q[i]->w_index;
			int rd_idx = pciedev->htod_dma_q[i]->r_index;

			if (wr_idx == rd_idx) {
				continue;
			}
			if (pciedev->htod_dma_q[i]->dma_info[rd_idx].timestamp &&
				(now - pciedev->htod_dma_q[i]->dma_info[rd_idx].timestamp) >
				PCIEDEV_MEM2MEM_DMA_THRESHOLD_TIME) {
				/**
				 * Now check for the DMA registers to see,
				 * if the DMA stuck at the HW level
				 */
				if (dma_txpending(pciedev->h2d_di[i])) {
					*rw_ind = ((wr_idx & 0xffff) << 16) | (rd_idx & 0xffff);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

static void
pciedev_dma_stall_reg_collect(struct dngl_bus *pciedev, bool h2d, uint32 *pcie_config_regs,
	int size)
{
	int count = 0;

	if (!size) {
		return;
	}
	if (h2d) {
		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d0_dmaregs.tx.control);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d0_dmaregs.tx.ptr);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d0_dmaregs.tx.addrlow);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d0_dmaregs.tx.status0);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.h2d0_dmaregs.tx.status1);
		if (count >= size)
			return;
	} else {
		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h0_dmaregs.tx.control);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h0_dmaregs.tx.ptr);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h0_dmaregs.tx.addrlow);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h0_dmaregs.tx.status0);
		if (count >= size)
			return;

		pcie_config_regs[count++] =
			R_REG(pciedev->osh, &pciedev->regs->u.pcie2.d2h0_dmaregs.tx.status1);
		if (count >= size)
			return;
	}
}

/*
* Determine if d2h DMA is delayed.
*/
static bool
pciedev_d2h_dma_taking_too_long(struct dngl_bus *pciedev, uint32 now, uint32 *rw_ind)
{
	int i;

	for (i = 0; i < MAX_DMA_CHAN; i++) {
		if (VALID_DMA_CHAN_NO_IDMA(i, pciedev) &&
			pciedev->dtoh_dma_q[i]) {
			int wr_idx = pciedev->dtoh_dma_q[i]->w_index;
			int rd_idx = pciedev->dtoh_dma_q[i]->r_index;

			if (wr_idx == rd_idx) {
				continue;
			}
			if (pciedev->dtoh_dma_q[i]->dma_info[rd_idx].timestamp &&
				(now - pciedev->dtoh_dma_q[i]->dma_info[rd_idx].timestamp) >
				PCIEDEV_MEM2MEM_DMA_THRESHOLD_TIME) {
				/**
				 * Now check for the DMA registers to see,
				 * if the DMA stuck at the HW level
				 */
				if (dma_txpending(pciedev->d2h_di[i])) {
					*rw_ind = ((wr_idx & 0xffff) << 16) | (rd_idx & 0xffff);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/*
* Determine if IOCTL/IOVAR response is delayed.
*/
static bool
pciedev_ioctl_taking_too_long(struct dngl_bus *pciedev, uint32 now)
{
	if (!pciedev->ioctl_lock)
		return FALSE;

	if (pciedev->ipc_data &&
		pciedev->ipc_data->last_ioctl_proc_time &&
		(now - pciedev->ipc_data->last_ioctl_proc_time >
			PCIEDEV_IOCTL_PROC_THRESHOLD_TIME)) {
		return TRUE;
	}
	return FALSE;
}

/*
* Determine if D3-ACK is delayed.
*/
static bool
pciedev_d3ack_taking_too_long(struct dngl_bus *pciedev, uint32 now)
{
	if (pciedev->bus_counters->d3_info_enter_count ==
		pciedev->bus_counters->d3_ack_sent_count)
		return FALSE;

	if (now - pciedev->bus_counters->d3_info_start_time >
		PCIEDEV_D3ACK_THRESHOLD_TIME) {
		return TRUE;
	}
	return FALSE;
}

/* read the config space register to check if there is PCIe link down */
static bool
pciedev_check_link_down(struct dngl_bus *pciedev)
{
	uint32 dl_status;

	if (pciedev_ds_in_host_sleep(pciedev)) {
		return FALSE;
	}
	dl_status = R_REG_CFG(pciedev, PCI_DL_STATUS);
	if (!(dl_status & PCI_DL_STATUS_PHY_LINKUP)) {
		PCI_ERROR(("PCIE LINK DOWN Detected!!\n"));
		print_config_register_value(pciedev, 0,
			sizeof(pcie_dump_registers_address) / sizeof(uint32));
		return TRUE;
	}
	return FALSE;
}

/*
* Check if any intstatus bit for MSI (h2d DB) is asserted
*/
bool
pciedev_msi_intstatus_asserted(struct dngl_bus *pciedev)
{
	uint32 int_stat;

	if (pciedev->clear_db_intsts_before_db && pciedev->common_rings_attached) {
		if ((int_stat = R_REG(pciedev->osh, PCIE_mailboxint(pciedev->autoregs)))) {
			PCI_ERROR(("int_stat:0x%x\n", int_stat));
			return TRUE;
		}
	}
	return FALSE;
}
/*
* Healthcheck PCIe client callback routine:
* If any issue found, collect the necessary information
* in the buffer supplied by healthcheck service and return
* apporpriate status.
*
*/
static int
pciedev_healthcheck_fn(uint8 *buffer, uint16 length, void *context, int16 *bytes_written)
{
	int i;
	struct dngl_bus *pciedev = (struct dngl_bus *)context;
	bcm_dngl_pcie_hc_t *pcie_hc = (bcm_dngl_pcie_hc_t *)buffer;
	uint32 current_time_ms = OSL_SYSUPTIME();
	uint32  rw_ind = 0;

	if (buffer == NULL || length == 0) {
		PCI_ERROR(("PCIE HC: buffer unavailable\n"));
		return HEALTH_CHECK_STATUS_OK;
	}
	if (pciedev->hc_params == NULL) {
		PCI_ERROR(("PCIE HC: HC params unavailable\n"));
		return HEALTH_CHECK_STATUS_OK;
	}
	if (pciedev_check_device_AER(pciedev,
		pcie_hc->pcie_config_regs, HC_PCIEDEV_CONFIG_REGLIST_MAX)) {
		if (buffer && length >= sizeof(bcm_dngl_pcie_hc_t)) {
			pcie_hc->pcie_flag |= HEALTH_CHECK_PCIEDEV_FLAG_AER;
		}
		PCI_PRINT(("AER ERROR NOTIFIED!\n"));
		return (HEALTH_CHECK_STATUS_TRAP);
	}
	/*
	* intstatus bit for MSI (h2d DB) should never be asserted. If asserted,
	* MSI cannot be sent to the host.
	*/
	if (pciedev_msi_intstatus_asserted(pciedev)) {
		if (buffer && length >= sizeof(bcm_dngl_pcie_hc_t)) {
			pcie_hc->pcie_flag |= HEALTH_CHECK_PCIEDEV_FLAG_MSI_INT;
		}
		PCI_PRINT(("MSI ERROR NOTIFIED!\n"));
		return (HEALTH_CHECK_STATUS_TRAP);
	}
	if (pciedev_check_link_down(pciedev)) {
		PCI_PRINT(("LINK DOWN ERROR NOTIFIED!\n"));
		if (buffer && length >= sizeof(bcm_dngl_pcie_hc_t)) {
			pcie_hc->pcie_flag |= HEALTH_CHECK_PCIEDEV_FLAG_LINKDOWN;
		}
		return (HEALTH_CHECK_STATUS_TRAP);
	}

	if (pciedev->hc_params->hc_induce_error) {
		PCI_PRINT(("HEALTH CHECK ERROR NOTIFIED!\n"));
		pciedev->hc_params->hc_induce_error = FALSE;
		pcie_hc->pcie_err_ind_type = HEALTH_CHECK_PCIEDEV_INDUCED_IND;
		goto indication;
	}

	if (pciedev_h2d_dma_taking_too_long(pciedev, current_time_ms, &rw_ind)) {
		pcie_hc->pcie_err_ind_type = HEALTH_CHECK_PCIEDEV_H2D_DMA_IND;
		goto indication;
	}

	if (pciedev_d2h_dma_taking_too_long(pciedev, current_time_ms, &rw_ind)) {
		pcie_hc->pcie_err_ind_type = HEALTH_CHECK_PCIEDEV_D2H_DMA_IND;
		goto indication;
	}

	if (pciedev_ioctl_taking_too_long(pciedev, current_time_ms)) {
		pcie_hc->pcie_err_ind_type = HEALTH_CHECK_PCIEDEV_IOCTL_STALL_IND;
		goto indication;
	}

	if (pciedev_d3ack_taking_too_long(pciedev, current_time_ms)) {
		pcie_hc->pcie_err_ind_type = HEALTH_CHECK_PCIEDEV_D3ACK_STALL_IND;
		goto indication;
	}

	*bytes_written = 0;
	return HEALTH_CHECK_STATUS_OK;

indication :
	if (length < sizeof(bcm_dngl_pcie_hc_t)) {
		PCI_ERROR(("PCIE HC:insufficient buffer, got:%d, requested:%d\n",
			length, sizeof(bcm_dngl_pcie_hc_t)));
		return HEALTH_CHECK_STATUS_OK;
	}
	pcie_hc->version = HEALTH_CHECK_PCIEDEV_VERSION_1;
	pcie_hc->pcie_flag |=
		pciedev_ds_in_host_sleep(pciedev) ? HEALTH_CHECK_PCIEDEV_FLAG_IN_D3: 0;
	pcie_hc->pcie_control_reg = R_REG(pciedev->osh, PCIE_pciecontrol(pciedev->autoregs));

	switch (pcie_hc->pcie_err_ind_type) {
		case HEALTH_CHECK_PCIEDEV_H2D_DMA_IND:
		case HEALTH_CHECK_PCIEDEV_D2H_DMA_IND:
			pcie_hc->pcie_control_reg = rw_ind;
			pciedev_dma_stall_reg_collect(pciedev,
				pcie_hc->pcie_err_ind_type == HEALTH_CHECK_PCIEDEV_H2D_DMA_IND ?
				TRUE : FALSE,
				pcie_hc->pcie_config_regs,
				HC_PCIEDEV_CONFIG_REGLIST_MAX);
			break;

		default:
			pcie_hc->pcie_control_reg = R_REG(pciedev->osh,
				PCIE_pciecontrol(pciedev->autoregs));
			if (!pciedev_ds_in_host_sleep(pciedev)) {
				for (i = 0;
					i < sizeof(pcie_dump_registers_address) / sizeof(uint32);
					i++) {
					if (i == HC_PCIEDEV_CONFIG_REGLIST_MAX)
						break;
					pcie_hc->pcie_config_regs[i] =
						R_REG_CFG(pciedev,
							pciedev_pcie_dump_registers_address(i));
			    }
			}
			break;
	}

	*bytes_written = sizeof(bcm_dngl_pcie_hc_t);

	return (pcie_hc->pcie_flag << HEALTH_CHECK_STATUS_MSB_SHIFT |
		HEALTH_CHECK_STATUS_ERROR);
}

static void
pciedev_ds_hc_trigger_timerfn(dngl_timer_t *t)
{
	struct dngl_bus *pciedev = (struct dngl_bus *) hnd_timer_get_ctx(t);

	if (pciedev->hc_params == NULL) {
		return;
	}
	pciedev->hc_params->ds_hc_trigger_timer_count ++;
}

#endif /* HEALTH_CHECK */

static void
pciedev_ds_hc_trigger(struct dngl_bus *pciedev)
{
	if (pciedev->hc_params == NULL) {
		return;
	}
	if (pciedev->hc_params->ds_hc_trigger_timer_active)
		return;

	/* Force HT to keep the chip up */
	si_corereg(pciedev->sih, SI_CC_IDX,
		OFFSETOF(chipcregs_t, clk_ctl_st), CCS_FORCEHT, CCS_FORCEHT);
	SPINWAIT(((si_corereg(pciedev->sih, SI_CC_IDX,
		OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0) &
		CCS_FORCEHT) != CCS_FORCEHT), PMU_MAX_TRANSITION_DLY);
	ASSERT((si_corereg(pciedev->sih, SI_CC_IDX,
		OFFSETOF(chipcregs_t, clk_ctl_st), 0, 0) &
		CCS_FORCEHT) == CCS_FORCEHT);

	/* Wake up ARM every second to kick off healthcheck */
	dngl_add_timer(pciedev->hc_params->ds_hc_trigger_timer, 1000, TRUE);
	pciedev->hc_params->ds_hc_trigger_timer_active = TRUE;
}

#if defined(BCMPCIE_IDMA)
bool
pciedev_idma_channel_enable(struct dngl_bus *pciedev, bool enable)
{
	if (enable && pciedev->idma_info->chan_disabled) {
		PCI_TRACE(("%s enable DMA_CH2\n", __FUNCTION__));
		dma_chan_enable(pciedev->h2d_di[DMA_CH2], TRUE);
		pciedev->idma_info->chan_disabled = FALSE;
	} else if (!enable && !pciedev->idma_info->chan_disabled) {
		PCI_TRACE(("%s disable DMA_CH2\n", __FUNCTION__));
		dma_chan_enable(pciedev->h2d_di[DMA_CH2], FALSE);
		pciedev->idma_info->chan_disabled = TRUE;
		if (pciedev->idma_dmaxfer_active) {
			PCI_TRACE(("%s disable DMA_CH2 during idma_dmaxfer_active\n",
				__FUNCTION__));
			pciedev->idma_info->chan_disable_err_cnt++;
			pciedev->idma_dmaxfer_active = FALSE;
		}
	}
	return TRUE;
}
#endif /* BCMPCIE_IDMA */

#ifdef WLCFP
/**
 * Delink a cfp flowid from bus layer
 * Loop through all valid flowrings to check for any linked flows
 */
void
pciedev_cfp_flow_delink(struct dngl_bus * pciedev, uint16 cfp_flowid)
{
	msgbuf_ring_t	*flow_ring;	/* flow ring hdl */
	struct dll	*cur, *prio;	/* d11 hdl */

	/* loop through nodes, weighted ordered queue implementation */
	prio = dll_head_p(&pciedev->active_prioring_list);

	while (!dll_end(prio, &pciedev->active_prioring_list)) {
		/* List of Priorities */
		prioring_pool_t *prioring = (prioring_pool_t *)prio;
		cur = dll_head_p(&prioring->active_flowring_list);

		while (!dll_end(cur, &prioring->active_flowring_list)) {
			/* List of flowrings in the given priority */
			flow_ring = ((flowring_pool_t *)cur)->ring;

			/* Check for matching cfp_flowid */
			if (flow_ring->cfp_flowid != cfp_flowid) {
				cur = dll_next_p(cur);
				continue;
			}

			/* TCB state should also be set , if entry found */
			ASSERT(flow_ring->tcb_state);

			/* Delink now */
			flow_ring->tcb_state = NULL;
			flow_ring->cfp_flowid = SCB_FLOWID_INVALID;
			/* Give a chance to do relink if it's just roaming in STA mode */
			if (!flow_ring->flow_info.iftype_ap)
				flow_ring->status |= FLOW_RING_ROAM_CHECK;

			/* next node */
			cur = dll_next_p(cur);
		}

		/* get next priority ring node */
		prio = dll_next_p(prio);
	}
}
#endif /* WLCFP */

#if defined(BCM_BUZZZ) && defined(BCM_BUZZZ_STREAMING_BUILD)

/* Copy a buzzz log buffer from Dongle to Host using D2H mem2mem DMA */
static int pciedev_buzzz_d2h_copy_m2m(struct dngl_bus *pciedev,
	uint32 daddr32, uint32 haddr32, uint16 len, uint8 index, uint32 offset);

static int BCM_BUZZZ_NOINSTR_FUNC
pciedev_buzzz_d2h_copy_m2m(struct dngl_bus * pciedev,
	uint32 daddr32, uint32 haddr32, uint16 len, uint8 index, uint32 offset)
{
	hnddma_t *di;
	pcie_m2m_req_t *m2m_req;
	dma_queue_t *dmaq;

	haddr64_t haddr64;
	dma64addr_t daddr64;

	di = pciedev->d2h_di[pciedev->default_dma_ch];
	m2m_req = pciedev->m2m_req;
	dmaq = pciedev->dtoh_dma_q[pciedev->default_dma_ch];

	haddr64.hi = 0U; haddr64.lo = haddr32;
	daddr64.hi = 0U; daddr64.lo = daddr32;

	PCIE_M2M_REQ_INIT(m2m_req);
	PCIE_M2M_REQ_RX_SETUP(m2m_req, haddr64, len); /* Rx into BUZZZ host memory */
	PCIE_M2M_REQ_TX_SETUP(m2m_req, daddr64, len); /* Tx from BUZZZ dongle memory */

	/* Submit m2m request */
	if (PCIE_M2M_REQ_SUBMIT(di, m2m_req)) {
		PCI_ERROR(("%s: buzzz m2m desciptors not available\n", __FUNCTION__));
		DBG_BUS_INC(pciedev, pciedev_buzzz_d2h_copy);
		return BCME_NORESOURCE;
	}

	/* Saved fields len, index and offset will be returned to buzzz on
	 * D2H dmacomplete via bcm_buzzz_d2h_done()
	 */
	pciedev_enqueue_dma_info(dmaq, len, MSG_TYPE_BUZZZ_STREAM,
	                       index, (void*)offset, 0, 0);
	return BCME_OK;

} /* pciedev_buzzz_d2h_copy_m2m */

/* Used by BUZZZ streaming mode to transfer a logged buffer by either an
 * asynchronous mem2mem or a BME channel (available on specific SoCs)
 */
int BCM_BUZZZ_NOINSTR_FUNC
pciedev_buzzz_d2h_copy(uint32 buf_idx,
	uint32 daddr32, uint32 haddr32, uint16 len, uint8 index, uint32 offset)
{
	struct dngl_bus *pciedev = dongle_bus;
#ifdef HNDBME
	{
		int bme_key = pciedev->bme_key[buf_idx];
		if ((bme_key != 0) && (bme_key != BME_INVALID)) {
			bme_sync_eng(pciedev->osh, buf_idx);
			/* Max bme dma size: BME_D64_XP_BC_MASK :
			 * BUZZZ does not use entire 8KBytes. Adjust length by -1
			 */
			bme_copy(pciedev->osh, bme_key,
				(const void*)daddr32, (void*)haddr32, len - 1);
			return BCME_OK;
		}
	}
#endif /* HNDBME */
	return pciedev_buzzz_d2h_copy_m2m(pciedev,
	           daddr32, haddr32, len, index, offset);

} /* pciedev_buzzz_d2h_copy */

/* Invoked by BUZZZ streaming mode bcm_buzzz_swap, to check whether a previously
 * requested BME DMA has completed. When BUZZZ is streaming function calls, a
 * busy loop is used to sync on BME done.
 */
int BCM_BUZZZ_NOINSTR_FUNC
pciedev_buzzz_d2h_done(uint32 buf_idx)
{
#ifdef HNDBME
	struct dngl_bus *pciedev = dongle_bus;
	int bme_key = pciedev->bme_key[buf_idx];
	if ((bme_key != 0) && (bme_key != BME_INVALID)) {
		bme_sync_eng(pciedev->osh, buf_idx); /* loops until DMA completes */
		return 1;
	}
#endif /* HNDBME */

	return 0; /* mem2mem uses bcm_buzzz_d2h_done handler */

} /* pciedev_buzzz_d2h_done */

#if defined(BCM_BUZZZ_KPI_QUE_LEVEL) && (BCM_BUZZZ_KPI_QUE_LEVEL > 0)
uint8 *
buzzz_bus(uint8 *buzzz_log)
{
	msgbuf_ring_t *msgbuf;
	uint16 ring, ring_cnt;
	struct dngl_bus * pciedev = dongle_bus;
	const uint16 ring_max = 256; // 8KBytes allows a max of 512 rings, limit 256

	bcm_buzzz_subsys_hdr_t *buzzz_log_bus;

	struct buzzz_log_ring {
		uint16 idx; char ea[ETHER_ADDR_LEN];
		uint16 rd; uint16 rd_pend;
		uint16 wr; uint8 cfp_flowid; uint8 max512;
	} * buzzz_log_ring;

	if (pciedev == NULL) return buzzz_log;

	buzzz_log_bus  = (bcm_buzzz_subsys_hdr_t*)buzzz_log;
	buzzz_log_ring = (struct buzzz_log_ring *)(buzzz_log_bus + 1);
	ring_cnt = 0;

	for (ring = 0; ring < pciedev->max_h2d_rings; ring++)
	{
		if (pciedev->h2d_submitring_ptr[ring])
		{
			msgbuf = (msgbuf_ring_t *)pciedev->h2d_submitring_ptr[ring];
			if (msgbuf->inited) {
				buzzz_log_ring->idx        = msgbuf->ringid;
				bcm_ether_ntoa((struct ether_addr *)msgbuf->flow_info.da,
					buzzz_log_ring->ea);
				buzzz_log_ring->rd         = MSGBUF_RD(msgbuf);
				buzzz_log_ring->rd_pend    = msgbuf->fetch_ptr;
				buzzz_log_ring->wr         = MSGBUF_WR(msgbuf);
				buzzz_log_ring->cfp_flowid = msgbuf->cfp_flowid;
				buzzz_log_ring->max512     = MSGBUF_MAX(msgbuf) / 512;

				buzzz_log_ring++;
				ring_cnt++;
				if (ring == ring_max)
					break;
			}
		}
	}

	// setup subsys section header
	buzzz_log_bus->id  = BUZZZ_BUS_SUBSYS;
	buzzz_log_bus->u16 = ring_cnt;

	return (uint8*)buzzz_log_ring;

}   /* buzzz_bus */

#endif /* BCM_BUZZZ_KPI_QUE_LEVEL */
#endif /* BCM_BUZZZ && BCM_BUZZZ_STREAMING_BUILD */

/*
 * Start sb2pcie access to host memmory using sbtopcie0 address translation
 *
 * Used for accessing Host mem temporarily
 * Configure the base address and remapped address
 * Caller expected to restore base address.
 */
void
pciedev_sbtopcie_access_start(struct dngl_bus * pciedev, sbtopcie_info_t *sbtopcie_info)
{
	uint32 base_addr;

	ASSERT(pciedev->regs);
	BCM_REFERENCE(base_addr);

	/* Use sbtopcie translation 0 */
	/* Remap the address as per transalation 0 */
	SBTOPCIE_ADDR_REMAP(pciedev, sbtopcie_info->haddr64, 0,
		*(sbtopcie_info->remap_addr));

	/* Take a backup of base address */
	*(sbtopcie_info->base_hi) = R_REG(pciedev->osh, &pciedev->regs->sbtopcie0upper);
	*(sbtopcie_info->base_lo) = R_REG(pciedev->osh, &pciedev->regs->sbtopcie0);

	/* Update the Base register */
	SBTOPCIE_BASE_CONFIG(pciedev, sbtopcie_info->haddr64, sbtopcie_info->len,
		0, base_addr, FALSE);
}

/*
 * Stop sb2pcie access to host mem
 * Restore base register to previous value
 */
void
pciedev_sbtopcie_access_stop(struct dngl_bus * pciedev, uint32 base_lo, uint32 base_hi)
{
	ASSERT(pciedev->regs);

	/* restore back old base address */
	W_REG(pciedev->osh, &pciedev->regs->sbtopcie0, base_lo);
	W_REG(pciedev->osh, &pciedev->regs->sbtopcie0upper, base_hi);
}
/*
 * Flush pending PCIE H2D dma transfers.
 * Triggered on device reset/down.
 */
static void
pciedev_flush_h2d_dma_xfers(struct dngl_bus *pciedev, uint8 dmach)
{
	hnddma_t *di = pciedev->h2d_di[dmach];

	/* Wait till all scheduled H2D DMA transactions finish */
	while (dma_txactive(di) || dma_rxactive(di)) {
		/* H2D DMA completion handler */
		pciedev_handle_h2d_dma(pciedev, pciedev->default_dma_ch);
		OSL_DELAY(10);
	}

	ASSERT(dma_txactive(di) == 0);
	ASSERT(dma_rxactive(di) == 0);
}
/* Mark all pending fetch requests are cancelled */
static void
pciedev_cancel_pending_fetch_requests(struct dngl_bus *pciedev, uint8 dmach)
{
	pciedev_fetch_cmplt_q_t *fcq = pciedev->fcq[dmach];
	struct fetch_rqst *fr;

	if (fcq->head == NULL)
		return;

	fr = fcq->head;

	while (fr) {
		/* Mark the request as cancelled and wait for DMA to get over */
		FETCH_RQST_FLAG_SET(fr, FETCH_RQST_CANCELLED);

		/* Move to the next one */
		fr = fr->next;
	}
}

/* Flush all pending fetch requests in pciedev */
static void
pciedev_flush_fetch_requests(void* arg)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)arg;
	uint8 dmach = pciedev->default_dma_ch;

	/* Mark all pending fetch request as cancelled */
	pciedev_cancel_pending_fetch_requests(pciedev, dmach);

	/* Wait for all DMA transactions to get over */
	pciedev_flush_h2d_dma_xfers(pciedev, dmach);
}
