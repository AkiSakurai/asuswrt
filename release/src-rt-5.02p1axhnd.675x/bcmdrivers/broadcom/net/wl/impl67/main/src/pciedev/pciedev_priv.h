/*
 * PCIEDEV private data structures and macro definitions
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
 * $Id: pciedev_priv.h  $
 */

#ifndef _pciedev_priv_h_
#define _pciedev_priv_h_

#include <typedefs.h>
#include <osl.h>
#include <bcmpcie.h>
#include <circularbuf.h>
#include <bcmmsgbuf.h>
#include <pcie_core.h>
#include <hndsoc.h>
#include <hnddma.h>
#include <dngl_api.h>
#include <rte_fetch.h>
#include <hnd_cplt.h>
#ifdef PCIE_DEEP_SLEEP
#include <pciedev_ob_ds.h>
#ifdef PCIEDEV_INBAND_DW
#include <pciedev_ib_ds.h>
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
#include <hnd_hchk.h>
#ifdef DONGLEBUILD
#include <pcieregsoffs.h>
#endif /* DONGLEBUILD */

/* **** Private macro definitions **** */

/* Intentionally retained */
/* Test flags to try out the metadata code path */

/* Check if Host driver is compatible with current rev8 */
#define IS_DEFAULT_HOST_PCIE_IPC(pciedev) \
	((pciedev)->host_pcie_ipc_rev == PCIE_IPC_DEFAULT_HOST_REVISION)

#define HOST_PCIE_API_REV_GE_6(pciedev) FALSE

/* Define this when only IPC rev7 based HostReady supported */
#ifdef HOSTREADY_ONLY_ENABLED
extern bool	_pciedev_hostready;
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define HOSTREADY_ONLY_ENAB()   (_pciedev_hostready)
#elif defined(HOSTREADY_ONLY_DISABLED)
	#define HOSTREADY_ONLY_ENAB()   (0)
#else
	#define HOSTREADY_ONLY_ENAB()   (1)
#endif /* ROM_ENAB_RUNTIME_CHECK */
#else
	#define HOSTREADY_ONLY_ENAB()   (0)
#endif /* HOSTREADY_ONLY_ENABLED */

#define MIN_RXDESC_AVAIL		4	/**< one message (2) +  one ioctl rqst (2) */
#define MIN_TXDESC_AVAIL		3
#define TXDESC_NEEDED			2
#define RXDESC_NEEDED			3

#define PCIEDEV_DEEPSLEEP_ENABLED	1
#define PCIEDEV_DEEPSLEEP_DISABLED	0

/* If phase bit is supported 3 rx descriptors and
 * 2 tx descriptors are needed.
 * Otherwise 2 rx descriptors and
 * 1 tx descriptor.
 */
#define MIN_RXDESC_NEED_WO_PHASE	2
#define MIN_TXDESC_NEED_WO_PHASE	1
#define RXDESC_NEED_IOCTLPYLD		2
#define TXDESC_NEED_IOCTLPYLD		1

/* indexes into tunables array */
#define PCIEBUS_H2D_NTXD	1
#define PCIEBUS_H2D_NRXD	2
#define PCIEBUS_D2H_NTXD	3
#define PCIEBUS_D2H_NRXD	4
#define RXBUFSIZE		5
#define MAXHOSTRXBUFS		6
#define MAXTXSTATUS		7
#define D2HTXCPL		8
#define D2HRXCPL		9
#define H2DRXPOST		10
#define MAXCTRLCPL              11
#define MAXPKTFETCH		12
#define MAXTXFLOW		13
#define MAXTUNABLE		14

#ifndef MAX_DMA_QUEUE_LEN_H2D
#define MAX_DMA_QUEUE_LEN_H2D	64
#endif // endif

#ifndef MAX_DMA_QUEUE_LEN_D2H
#define MAX_DMA_QUEUE_LEN_D2H	128
#endif // endif

#define MSGBUF_RING_NAME_LEN	7

#define PCIEDEV_MAX_FLOWRINGS_FETCH_REQUESTS	10

#define PCIEDEV_MAX_FLOW_STATS_COUNT	5

#define PCIEDEV_FLOWFETCH_RDINDS_TS(flowring, length, rind, ts) \
({ \
	if ((flowring) && (flowring)->flow_stats) { \
	(flowring)->flow_stats->last_fetch_rdinds[(flowring)->flow_stats->fetch_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = ((rind) & 0xffff) | \
		(((length) & 0xffff) << 16); \
	(flowring)->flow_stats->last_fetch_times[(flowring)->flow_stats->fetch_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (ts); \
	if ((flowring)->flow_stats->first_fetch_after_tx_compl == 0) \
		(flowring)->flow_stats->first_fetch_after_tx_compl = (ts); \
	(flowring)->flow_stats->fetch_count++; \
	} \
})

#define PCIEDEV_FLOWSUPP_RDINDS_TS(flowring, rind, status, seq, ts) \
({ \
	if ((flowring) && (flowring)->flow_stats) { \
	(flowring)->flow_stats->suppress_status[(flowring)->flow_stats->suppress_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (((rind) << 16) | (status)); \
	(flowring)->flow_stats->last_suppress_times[(flowring)->flow_stats->suppress_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (ts); \
	(flowring)->flow_stats->last_suppress_seq[(flowring)->flow_stats->suppress_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (seq); \
	(flowring)->flow_stats->suppress_count++; \
	} \
})

#define PCIEDEV_FLOW_PORT_OPEN_TS(flowring, ts)	\
({ \
	if ((flowring) && (flowring)->flow_stats) { \
	(flowring)->flow_stats->last_port_open_times[(flowring)->flow_stats->port_open_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (ts); \
	(flowring)->flow_stats->port_open_count++; \
	} \
})

#define PCIEDEV_FLOW_PORT_CLOSE_TS(flowring, ts)	\
({ \
	if ((flowring) && (flowring)->flow_stats) { \
	(flowring)->flow_stats->last_port_close_times[(flowring)->flow_stats->port_close_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (ts); \
	(flowring)->flow_stats->port_close_count++; \
	} \
})

#define PCIEDEV_FLOW_COMPL_TS(flowring, rindex, ts)	\
({ \
	if ((flowring) && (flowring)->flow_stats) { \
	(flowring)->flow_stats->last_tx_compl_times[(flowring)->flow_stats->tx_compl_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (ts); \
	(flowring)->flow_stats->last_tx_compl_rdinds[(flowring)->flow_stats->tx_compl_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = ((rindex) & 0xffff) | \
		(((*((flowring)->rd_ptr)) & 0xffff) << 16); \
	(flowring)->flow_stats->first_fetch_after_tx_compl = 0; \
	(flowring)->flow_stats->tx_compl_count++; \
	} \
})

#define PCIEDEV_FLOW_FLUSH_TS(flowring, ts)	\
({ \
	if ((flowring) && (flowring)->flow_stats) { \
	(flowring)->flow_stats->last_flush_times[(flowring)->flow_stats->flush_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (ts); \
	(flowring)->flow_stats->flush_pktinflight[(flowring)->flow_stats->flush_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = (flowring)->flow_info.pktinflight; \
	(flowring)->flow_stats->last_flush_rdwr[(flowring)->flow_stats->flush_count % \
	PCIEDEV_MAX_FLOW_STATS_COUNT] = ((*((flowring)->wr_ptr)) & 0xffff) | \
		(((*((flowring)->rd_ptr)) & 0xffff) << 16); \
	(flowring)->flow_stats->flush_count++; \
	} \
})

#define LCL_BUFPOOL_AVAILABLE(ring)	((ring)->buf_pool->availcnt)
/* Circular buffer with all items usable, availcnt added for empty/free checks */
#define CIR_BUFPOOL_AVAILABLE(cpool)	(((cpool)->availcnt) ? \
	WRITE_SPACE_AVAIL_CONTINUOUS((cpool)->r_ptr, \
	(cpool)->w_ptr, (cpool)->depth) : 0)

#ifdef PCIEDEV_TS_FULL_LOGGING
#define PCIEDEV_IPC_MAX_H2D_DB0 4
#define PCIEDEV_TS_LOGGING(dmaq) ((dmaq)->timestamp = OSL_SYSUPTIME())
#else
#define PCIEDEV_IPC_MAX_H2D_DB0 2
#define PCIEDEV_TS_LOGGING(dmaq) pciedev_ts_logging_min((dmaq))
#endif /* PCIEDEV_TS_MIN_LOGGING */

#ifdef PCIEDEV_USE_EXT_BUF_FOR_IOCTL
#ifndef PCIEDEV_MAX_IOCTLRSP_BUF_SIZE
#define PCIEDEV_MAX_IOCTLRSP_BUF_SIZE          16384
#endif /* PCIEDEV_MAX_IOCTLRSP_BUF_SIZE */
#endif /* PCIEDEV_USE_EXT_BUF_FOR_IOCTL */

#define HOST_DMA_BUF_POOL_EMPTY(pool)	((((pool)->ready) == NULL) ? TRUE : FALSE)

#define TRAP_IN_PCIEDRV(arg)	do {PCI_ERROR(arg); OSL_SYS_HALT();} while (0)
#define	TRAP_DUE_DMA_RESOURCES(arg)	TRAP_IN_PCIEDRV(arg)
#define	TRAP_DUE_NULL_POINTER(arg)	TRAP_IN_PCIEDRV(arg)
#define TRAP_DUE_DMA_ERROR(arg)		TRAP_IN_PCIEDRV(arg)
#define TRAP_DUE_CORERESET_ERROR(arg)	TRAP_IN_PCIEDRV(arg)
#define TRAP_DUE_USER_COMMAND(arg)	TRAP_IN_PCIEDRV(arg)
#define TRAP_DUE_EXCESS_BUFPOST(arg)	TRAP_IN_PCIEDRV(arg)
#define TRAP_DUE_WORKITEM_BADPHASE(arg)		TRAP_IN_PCIEDRV(arg)

#define MSGBUF_RING_INIT_PHASE	0x80

#ifndef PCIEDEV_MIN_RXCPLGLOM_COUNT
#define PCIEDEV_MIN_RXCPLGLOM_COUNT 4	/**< Min number of rx completions to start glomming */
#endif // endif

#define        PCIEDEV_MAX_SUP_RETRIES 7

#ifdef PCIEDEV_SUPPR_RETRY_TIMEOUT
#define PCIEDEV_SUP_EXP_ENAB(flowring) ((flowring)->sup_exp_enab)
#define PCIEDEV_SUP_EXP_ENAB_SET(flowring, enab) ((flowring)->sup_exp_enab = (enab))
#else
#define PCIEDEV_SUP_EXP_ENAB(flowring) FALSE
#define PCIEDEV_SUP_EXP_ENAB_SET(flowring, enab)
#endif /* PCIEDEV_SUPPR_RETRY_TIMEOUT */

#define PCIEDEV_SET_SUP_EXP_CNT(flowring, r_index, val)	\
({	\
	uint8 cnt = (flowring)->sup_exp_block[(r_index) >> 1];	\
	cnt = (((r_index) % 2) ? ((cnt & 0x0F) | ((val) << 4)) : ((cnt & 0xF0) | (val)));	\
	(flowring)->sup_exp_block[(r_index) >> 1] = cnt;	\
})

#define PCIEDEV_CLR_SUP_EXP_CNT(flowring, r_index) PCIEDEV_SET_SUP_EXP_CNT(flowring, r_index, 0)

#define PCIEDEV_INC_SUP_EXP_CNT(flowring, r_index)	\
	PCIEDEV_SET_SUP_EXP_CNT(flowring, r_index, (PCIEDEV_GET_SUP_EXP_CNT(flowring, r_index) + 1))

#define PCIEDEV_GET_SUP_EXP_CNT(flowring, r_index) \
	(((r_index) % 2) ? \
		(((flowring)->sup_exp_block[(r_index) >> 1] & 0xF0) >> 4) : \
		(((flowring)->sup_exp_block[(r_index) >> 1] & 0x0F)))

#define TOCK_SHIFT		5
#define TOCK_UNIT		(1 << TOCK_SHIFT)
#define PCIEDEV_MAX_SUP_TIME_MS	2560		/* 2560 msec */
#define PCIEDEV_MAX_SUP_TOCKS	(PCIEDEV_MAX_SUP_TIME_MS/TOCK_UNIT)
#define PCIEDEV_GET_CUR_TOCKS()	((hnd_time() >> TOCK_SHIFT) & 0xFF)
#define PCIEDEV_CLR_SUP_EXP_TSTAMP(flowring, r_index) \
	flowring->sup_exp_tstamp[r_index] = 0
#define PCIEDEV_SET_SUP_EXP_TSTAMP(flowring, r_index) \
	flowring->sup_exp_tstamp[r_index] = PCIEDEV_GET_CUR_TOCKS()
#define PCIEDEV_GET_SUP_EXP_ELAPSE(flowring, r_index) \
	((256 + PCIEDEV_GET_CUR_TOCKS() - flowring->sup_exp_tstamp[r_index]) & 0xff)

/* Check if reuse_sup_seq bitmap has any bit set */
#define FLOWRING_HAS_REUSED_SEQ_PKT(x)	(bcm_nonzero_bitmap(x->reuse_sup_seq, x->bitmap_size))
/** timeout to delete suppression related info from flow */
#define PCIEDEV_FLOW_RING_SUPP_TIMEOUT	5000

/* Assuming accounting of 32 events would be sufficient */
#define	MAX_EVENT_Q_LEN	32

/* Flow ring status */

/** set when creating a flow ring. is never reset. */
#define FLOW_RING_ACTIVE		(1 << 0)
/**
 * 0: either no packet needs to be fetched, or a single packet fetch is in progress.
 * 1: one or more packets need to be fetched from the host.
 */
#define FLOW_RING_PKT_PENDING		(1 << 1)
/** flow control related, set eg when WL is in power save mode */
#define FLOW_RING_PORT_CLOSED		(1 << 2)
/** flow control related, set eg when WL IF is closed to for example scan */
#define FLOW_RING_IF_CLOSED		(1 << 3)
/** Combination flag to check if flow ring is closed */
#define FLOW_RING_CLOSED		(FLOW_RING_PORT_CLOSED | FLOW_RING_IF_CLOSED)
/**
 * set when host requested to delete a ring but the applicable ring still contains packets, ring
 * will be deleted once packets are flushed by the dongle.
 */
#define FLOW_RING_DELETE_RESP_PENDING	(1 << 4)
/** set when the ring is starting to flush (but has not completed the flush yet) */
#define FLOW_RING_FLUSH_RESP_PENDING	(1 << 5)
/** set while the ring is being flushed */
#define FLOW_RING_FLUSH_PENDING		(1 << 6)
/** set when one or more packets on the ring need to be retransmitted */
#define FLOW_RING_SUP_PENDING		(1 << 7)
#define FLOW_RING_DELETE_RESP_RETRY	(1 << 9)
#define FLOW_RING_FLUSH_RESP_RETRY	(1 << 10)
/** set when txstatus and fetch is to be suppressed for fast delete ring */
#define FLOW_RING_FAST_DELETE_ACTIVE	(1 << 13)
/** relink flowring in case it was delinked due to roaming */
#define FLOW_RING_ROAM_CHECK		(1 << 14)

/* Flow ring flags */
#define FLOW_RING_FLAG_LAST_TIM		(1 << 0)
#define FLOW_RING_FLAG_INFORM_PKTPEND	(1 << 1)
#define FLOW_RING_FLAG_PKT_REQ		(1 << 2)

#define PCIE_IN_D3_SUSP_PKTMAX		16
#define PCIE_IN_D3_SUSP_EVNTMAX		16
#ifndef PCIEDEV_EVENT_BUFPOOL_MAX
#define PCIEDEV_EVENT_BUFPOOL_MAX	32
#endif /* PCIEDEV_EVENT_BUFPOOL_MAX */
#define PCIEDEV_IOCTRESP_BUFPOOL_MAX	16
#define PCIEDEV_TS_BUFPOOL_MAX		32

#ifndef PCIEDEV_MAX_PACKETFETCH_COUNT
#define PCIEDEV_MAX_PACKETFETCH_COUNT	32
#endif // endif
#define PCIEDEV_MIN_PACKETFETCH_COUNT	4
#define PCIEDEV_INC_PACKETFETCH_COUNT	2
#ifndef PCIEDEV_MAX_LOCALBUF_PKT_COUNT
#define PCIEDEV_MAX_LOCALBUF_PKT_COUNT	200
#endif // endif

#define PCIEDEV_INFOBUF_POOL_MAX	4

#define PCIEDEV_MIN_PKTQ_DEPTH		256
#define PCIEDEV_IPC_MAX_DW_TOGGLE	8

#define PCIEDEV_MIN_SHCEDLFRAG_COUNT	1

/* flowring Index */
#define FLRING_INX(x) ((x) - BCMPCIE_H2D_MSGRING_TXFLOW_IDX_START)

#define PCIEDEV_INTERNAL_SENT_D2H_PHASE		0x01
#define PCIEDEV_INTERNAL_D2HINDX_UPD		0x02

#define INUSEPOOL(ring)	((ring)->buf_pool->inuse_pool)

/* Size of local queue to store completions */
#ifndef PCIEDEV_CNTRL_CMPLT_Q_SIZE
#define PCIEDEV_CNTRL_CMPLT_Q_SIZE 8
#endif // endif

/* Status Flag for local queue to store completions */
#define	CTRL_RESP_Q_FULL	(1 << 0)

/* In the local queue,
 * PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY reserved for IOCTLs
 * 1 for ACK, 1 for Completion
 */
#define PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY	2
/* In the local queue,
 * PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY reserved for
 * general status messages
 */
#define PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY	1

/* H2D control submission buffer count */
#define CTRL_SUB_BUFCNT	4
#if (PCIEDEV_CNTRL_CMPLT_Q_SIZE < (PCIEDEV_CNTRL_CMPLT_Q_IOCTL_ENTRY + \
	PCIEDEV_CNTRL_CMPLT_Q_STATUS_ENTRY + CTRL_SUB_BUFCNT))
#error "PCIEDEV_CNTRL_CMPLT_Q_SIZE is too small!"
#endif // endif

/* MAX_HOST_RXBUFS could be tuned from chip specific Makefiles */
#ifndef MAX_HOST_RXBUFS
#define MAX_HOST_RXBUFS			256
#endif // endif

#define MAX_DMA_XFER_SZ			1540
#define MAX_DMA_BATCH_SZ		16384
#define DMA_XFER_PKTID			0xdeadbeaf

#ifdef FLOWRING_SLIDING_WINDOW
#ifndef FLOWRING_SLIDING_WINDOW_SIZE
#define	FLOWRING_SLIDING_WINDOW_SIZE	512 /* default flowring depth */
#endif // endif
#endif /* FLOWRING_SLIDING_WINDOW */

#ifndef MAX_TX_STATUS_COMBINED
#define MAX_TX_STATUS_COMBINED		32
#endif // endif

#define MSGBUF_TIMER_DELAY		1

#define	I_ERRORS			(I_PC | I_DE | I_RO | I_XU)

#define PD_DMA_INT_MASK_H2D		0x1DC00
#define PD_DMA_INT_MASK_D2H		0x1DC00
#define PD_DB_INT_MASK			0xFF0000
#define PD_DEV0_DB_INTSHIFT		16

#define PD_DEV0_DB0_INTMASK		(0x1 << PD_DEV0_DB_INTSHIFT)
#define PD_DEV0_DB1_INTMASK		(0x2 << PD_DEV0_DB_INTSHIFT)
#define PD_DEV0_DB_INTMASK		((PD_DEV0_DB0_INTMASK) | (PD_DEV0_DB1_INTMASK))

#define PD_DEV1_DB_INTSHIFT		18
#define PD_DEV1_DB0_INTMASK		(0x1 << PD_DEV1_DB_INTSHIFT)
#define PD_DEV1_DB1_INTMASK		(0x2 << PD_DEV1_DB_INTSHIFT)
#define PD_DEV1_DB_INTMASK		((PD_DEV1_DB0_INTMASK) | (PD_DEV1_DB1_INTMASK))

#define PD_DEV2_DB_INTSHIFT		20
#define PD_DEV2_DB0_INTMASK		(0x1 << PD_DEV2_DB_INTSHIFT)
#define PD_DEV2_DB1_INTMASK		(0x2 << PD_DEV2_DB_INTSHIFT)
#define PD_DEV2_DB_INTMASK		((PD_DEV2_DB0_INTMASK) | (PD_DEV2_DB1_INTMASK))

#define PD_DEV3_DB_INTSHIFT		22
#define PD_DEV3_DB0_INTMASK		(0x1 << PD_DEV3_DB_INTSHIFT)
#define PD_DEV3_DB1_INTMASK		(0x2 << PD_DEV3_DB_INTSHIFT)
#define PD_DEV3_DB_INTMASK		((PD_DEV3_DB0_INTMASK) | (PD_DEV3_DB1_INTMASK))

#define PD_DEV0_DMA_INTMASK		0x80

#define PD_FUNC0_MB_INTSHIFT		8
#define PD_FUNC0_MB_INTMASK		(0x3 << PD_FUNC0_MB_INTSHIFT)

#define PD_FUNC0_PCIE_SB_INTSHIFT	0
#define PD_FUNC0_PCIE_SB__INTMASK	(0x3 << PD_FUNC0_PCIE_SB_INTSHIFT)

#define PD_DEV0_PWRSTATE_INTSHIFT	24
#define PD_DEV0_PWRSTATE_INTMASK	(0x1 << PD_DEV0_PWRSTATE_INTSHIFT)

#define PD_DEV0_PERST_INTSHIFT		6
#define PD_DEV0_PERST_INTMASK		(0x1 << PD_DEV0_PERST_INTSHIFT)

#define PD_MSI_FIFO_OVERFLOW_INTSHIFT	28
#define PD_MSI_FIFO_OVERFLOW_INTMASK	(0x1 << PD_MSI_FIFO_OVERFLOW_INTSHIFT)

/* HMAP related constants */
#define PD_HMAP_VIO_INTSHIFT		3
#define PD_HMAP_VIO_INTMASK		(0x1 << PD_HMAP_VIO_INTSHIFT)
#define PD_HMAP_VIO_CLR_VAL		0x3 /* write 0b11 to clear HMAP violation */
#define PD_HMAP_VIO_SHIFT_VAL		17  /* bits 17:18 clear HMAP violation */

/* DMA channel 2 datapath use case
 * Implicit DMA uses DMA channel 2 (outbound only)
 */
#if defined(BCMPCIE_IDMA) && !defined(BCMPCIE_IDMA_DISABLED)
#define PD_DEV2_INTMASK PD_DEV2_DB0_INTMASK
#elif defined(BCMPCIE_DMA_CH2)
#define PD_DEV2_INTMASK PD_DEV2_DB0_INTMASK
#else
#define PD_DEV2_INTMASK 0
#endif /* BCMPCIE_IDMA || BCMPCIE_DMA_CH2 */
/* DMA channel 1 datapath use case */
#ifdef BCMPCIE_DMA_CH1
#define PD_DEV1_INTMASK PD_DEV1_DB0_INTMASK
#else
#define PD_DEV1_INTMASK 0
#endif /* BCMPCIE_DMA_CH1 */
#if defined(BCMPCIE_IDMA)
#define PD_DEV1_IDMA_DW_INTMASK PD_DEV1_DB1_INTMASK
#else
#define PD_DEV1_IDMA_DW_INTMASK 0
#endif /* BCMPCIE_IDMA */

#define PD_DEV0_INTMASK		\
	(PD_DEV0_DMA_INTMASK | PD_DEV0_DB0_INTMASK | PD_DEV0_PWRSTATE_INTMASK | \
	PD_DEV0_PERST_INTMASK | PD_DEV1_INTMASK | PD_DEV0_DB1_INTMASK | \
	PD_DEV1_IDMA_DW_INTMASK)

/* implicit DMA index */
#define	PD_IDMA_COMP			0xffff		/* implicit dma complete */

#define PCIE_D2H_DB0_VAL		(0x12345678)

#define PD_ERR_ATTN_INTMASK		(1 << 29)
#define PD_LINK_DOWN_INTMASK		(1 << 27)

#define PD_ERR_UNSPPORT			(1 << 8)

#ifndef PD_H2D_NTXD
#define PD_H2D_NTXD			256
#endif // endif
#ifndef PD_H2D_NRXD
#define PD_H2D_NRXD			256
#endif // endif
#ifndef PD_D2H_NTXD
#define PD_D2H_NTXD			256
#endif // endif
#ifndef PD_D2H_NRXD
#define PD_D2H_NRXD			256
#endif // endif
#define PD_RXBUF_SIZE			PKTBUFSZ
#define PD_RX_HEADROOM			0
#define PD_NRXBUF_POST			8

#ifndef PD_NBUF_D2H_TXCPL
#define PD_NBUF_D2H_TXCPL		8
#endif // endif
#ifndef PD_NBUF_D2H_RXCPL
#define PD_NBUF_D2H_RXCPL		8
#endif // endif
#ifndef PD_NBUF_H2D_RXPOST
#define PD_NBUF_H2D_RXPOST		4
#endif // endif

#define PCIE_DS_CHECK_INTERVAL		10
#define	PCIE_BM_CHECK_INTERVAL		5
#define	PCIEDEV_AXI_CHECK_INTERVAL	40
#define PCIE_HOST_SLEEP_ACK_INTERVAL	1
#define PCIEDEV_HOST_SLEEP_ACK_TIMEOUT	1000

/* Healthcheck threshold times */
#define PCIEDEV_IOCTL_PROC_THRESHOLD_TIME	1000 /* ms */
#define PCIEDEV_MEM2MEM_DMA_THRESHOLD_TIME	100 /* ms */
#define PCIEDEV_D3ACK_THRESHOLD_TIME		1000 /* ms */
#define PCIEDEV_MAXWAIT_TRAP_HANDLER		50000000 /* useconds */

#define PCIE_PWRMGMT_CHECK		1
#define IOCTL_PKTPOOL_SIZE		1
#define TS_PKTPOOL_SIZE			1

#define TXSTATUS_LEN			1

#define IOCTL_PKTBUFSZ			2048
#define TS_PKTBUFSZ			2048

#define DYNAMIC_RING_INIT		(0)
#define DYNAMIC_RING_ACTIVE		(1)
#define DYNAMIC_RING_DELETE_RCVD	(2)
#define DYNAMIC_RING_DELETED		(4)

#define IS_D2H_RING_ACTIVE(status) 	((status) & DYNAMIC_RING_ACTIVE)
#define IS_H2D_RING_ACTIVE		IS_D2H_RING_ACTIVE

#define IS_D2H_RING_DELRCVD(status)	((status) & DYNAMIC_RING_DELETE_RCVD)
#define IS_H2D_RING_DELRCVD		IS_D2H_RING_DELRCVD

#define IS_D2H_RING_DELETED(status)	((status) & DYNAMIC_RING_DELETED)
#define IS_H2D_RING_DELETED		IS_D2H_RING_DELETED

#define IS_H2D_RING_EMPTY(msgbuf)	\
		(READ_AVAIL_SPACE(MSGBUF_WR(msgbuf),	\
				 MSGBUF_RD(msgbuf), MSGBUF_MAX(msgbuf)) == 0)
#define IS_H2D_BUFPOOL_EMPTY(bufpool)	\
			(bufpool->ready_cnt == 0)

#define PCIE_MEM2MEM_DMA_MIN_LEN	4

/* parameterized DMA
 * 3 independent inbound/outbound DMA data pathes are provided with corerev 19 and after.
 * Maximum DMA Channel number is defined as 3 as default as of now,
 * and separate features using each DMA channel 1 or 2 enables its channels.
 * for ex, BCMPCIE_DMA_CH1/BCMPCIE_DMA_CH2/BCMPCIE_IDMA.
 */
#define MAX_DMA_CHAN	BCMPCIE_DMACHNUM

#define DMA_CH0	0
#define DMA_CH1	1
#define DMA_CH2	2

/* Legacy DMA channel is DMA channel 0 to provide backward compatibility.
 * if want to use different channel or dynamic channel change for legacy ring message interface,
 * redefine on this definition. We can also define the channel for new applications in futher.
 */
#define LEGACY_DMA_CH	DMA_CH0

#define	PCIE_ADDR_OFFSET	(1 << 31)

/* Incrementing error counters */
#define PCIEDEV_MALLOC_ERR_INCR(pciedev)        (pciedev->err_cntr->mem_alloc_err_cnt++)
#define PCIEDEV_DMA_ERR_INCR(pciedev)           (pciedev->err_cntr->dma_err_cnt++)
#define PCIEDEV_RXCPL_ERR_INCR(pciedev)         (pciedev->err_cntr->rxcpl_err_cnt++)
#define PCIEDEV_PKTFETCH_CANCEL_CNT_INCR(pciedev)       (pciedev->err_cntr->pktfetch_cancel_cnt++)
#define PCIEDEV_UNSUPPORTED_ERROR_CNT_INCR(pciedev) \
	(pciedev->err_cntr->unsupported_request_err_cnt++)
#define PCIEDEV_LINKDOWN_CNT_INCR(pdev)		(pdev->err_cntr->linkdown_cnt++)

#define PCIE_MAX_TID_COUNT		8
#define PCIE_MAX_DS_LOG_ENTRY		20

#define MAX_MEM2MEM_DMA_CHANNELS	2 /**< Tx,Rx or DTOH,HTOD */
/* extended to 3 DMA channels */
#define PCIEDEV_GET_AVAIL_DESC(pciedev, d, e, f) (*((pciedev)->avail[(d)][(e)][(f)]))

/* h2d dma is not pending */
#define h2d_dma_not_pending(pciedev, ch) \
	((pciedev)->htod_dma_q[ch]->w_index == (pciedev)->htod_dma_q[ch]->r_index)

/**
 * PCIE Mem2Mem D2H DMA Completion Sync Options using marker in each work-item, used to cope with
 * host CPU cache side effects.
 * - Modulo-253 sequence number in marker
 * - XOR Checksum in marker, with module-253 seqnum in cmn msg hdr.
 */
#if defined(PCIE_M2M_D2H_SYNC_SEQNUM)
/** modulo-253 sequence number marker in D2H messages. */
#define PCIE_M2M_D2H_SYNC   PCIE_IPC_FLAGS_D2H_SYNC_SEQNUM
#define PCIE_M2M_D2H_SYNC_MARKER_INSERT(msg, msglen, _epoch) \
	({ \
		BCM_REFERENCE(msglen); \
		*((uint32 *)(((uint32)msg)+(msglen)-sizeof(uint32))) = \
		(_epoch) % D2H_EPOCH_MODULO; \
		(_epoch)++; \
	})
#define PCIE_M2M_D2H_SYNC_MARKER_REPLACE(msg, msglen) \
	({ BCM_REFERENCE(msg); BCM_REFERENCE(msglen); })

#elif defined(PCIE_M2M_D2H_SYNC_XORCSUM)
/** Checksum in marker and (modulo-253 sequence number) in D2H Messages. */
#define PCIE_M2M_D2H_SYNC   PCIE_IPC_FLAGS_D2H_SYNC_XORCSUM
#define PCIE_M2M_D2H_SYNC_MARKER_INSERT(msg, msglen, _epoch) \
	({ \
		(msg)->cmn_hdr.epoch = (_epoch) % D2H_EPOCH_MODULO; \
		(_epoch)++; \
		*((uint32 *)(((uint32)msg)+(msglen)-sizeof(uint32))) = 0U; \
		*((uint32 *)(((uint32)msg)+(msglen)-sizeof(uint32))) = \
			bcm_compute_xor32((uint32*)(msg), (msglen) / sizeof(uint32)); \
	})
#define PCIE_M2M_D2H_SYNC_MARKER_REPLACE(msg, msglen) \
	({ \
		*((uint32 *)(((uint32)msg)+(msglen)-sizeof(uint32))) = 0U; \
		*((uint32 *)(((uint32)msg)+(msglen)-sizeof(uint32))) = \
			bcm_compute_xor32((uint32*)(msg), (msglen) / sizeof(uint32)); \
	})

#else /* ! (PCIE_M2M_D2H_SYNC_SEQNUM || PCIE_M2M_D2H_SYNC_XORCSUM) */
/* Marker is unused. Either Read Barrier, MSI or alternate solution required. */
#undef PCIE_M2M_D2H_SYNC
#define PCIE_M2M_D2H_SYNC_MARKER_INSERT(msg, msglen, epoch) \
	({ BCM_REFERENCE(msg); BCM_REFERENCE(msglen); BCM_REFERENCE(epoch); })
#define PCIE_M2M_D2H_SYNC_MARKER_REPLACE(msg, msglen) \
	({ BCM_REFERENCE(msg); BCM_REFERENCE(msglen); })

#endif /* ! (PCIE_M2M_D2H_SYNC_SEQNUM || PCIE_M2M_D2H_SYNC_XORCSUM) */
#ifdef BCMPCIE_IDMA
	#if defined(ROM_ENAB_RUNTIME_CHECK)
		#define PCIE_IDMA_ENAB(pciedev) \
			((pciedev)->idma_info && (pciedev)->idma_info->enable)
		#define PCIE_IDMA_ACTIVE(pciedev) \
			((pciedev)->idma_info && ((pciedev)->idma_info->enable) && \
			((pciedev)->idma_info->inited))
	#elif defined(BCMPCIE_IDMA_DISABLED)
		#define PCIE_IDMA_ENAB(pciedev) (0)
		#define PCIE_IDMA_ACTIVE(pciedev) (0)
	#else
		#define PCIE_IDMA_ENAB(pciedev) (1)
		#define PCIE_IDMA_ACTIVE(pciedev) \
			((pciedev)->idma_info && ((pciedev)->idma_info->enable) && \
			((pciedev)->idma_info->inited))
	#endif
	/**
	 * 4347B0 memory retention WAR
	 * HW4347-884: PCIe M2M DMA memories don't have retention
	 */
	#define PCIE_IDMA_DS(pciedev) \
		(PCIECOREREV((pciedev)->corerev) == 23) ? TRUE : FALSE
#else
	#define PCIE_IDMA_ENAB(pciedev) (0)
	#define PCIE_IDMA_ACTIVE(pciedev) (0)
	#define PCIE_IDMA_DS(pciedev) (0)
#endif /* BCMPCIE_IDMA */
#ifdef BCMPCIE_DMA_CH1
	#define PCIE_DMA1_ENAB(info) ((info)->dma_ch1_enable)
#else
	#define PCIE_DMA1_ENAB(info) (0)
#endif /* BCMPCIE_DMA_CH1 */
#ifdef BCMPCIE_DMA_CH2
	#define PCIE_DMA2_ENAB(info) ((info)->dma_ch2_enable)
#else
	#define PCIE_DMA2_ENAB(info) (0)
#endif /* BCMPCIE_DMA_CH2 */
#ifdef BCMPCIE_D2H_MSI
	#define PCIE_MSI_ENAB(info)	(info).msi_enable
#else
	#define PCIE_MSI_ENAB(info) (0)
#endif // endif
#ifdef BCMPCIE_DAR
#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define PCIE_DAR_ENAB(pciedev) ((pciedev)->dar_enable)
#elif defined(BCMPCIE_DAR_DISABLED)
	#define PCIE_DAR_ENAB(pciedev)	(0)
#else
	#define PCIE_DAR_ENAB(pciedev)	((pciedev)->dar_enable)
#endif // endif
#else
	#define PCIE_DAR_ENAB(pciedev) (0)
#endif /* BCMPCIE_DMA_CH1 */

#define MAX_IDMA_DESC	BCMPCIE_IDMA_MAX_DESC

/* check each dma channel is valid */
#define VALID_DMA_CHAN(ch, pciedev) \
		(((ch) == DMA_CH0) || \
		(((ch) == DMA_CH1) && PCIE_DMA1_ENAB(pciedev)) || \
		(((ch) == DMA_CH2) && (PCIE_DMA2_ENAB(pciedev) || \
		PCIE_IDMA_ENAB(pciedev))))
/* check each dma channel is valid without Implicit DMA */
#define VALID_DMA_CHAN_NO_IDMA(ch, pciedev) \
		(((ch) == DMA_CH0) || \
		(((ch) == DMA_CH1) && PCIE_DMA1_ENAB(pciedev)) || \
		(((ch) == DMA_CH2) && PCIE_DMA2_ENAB(pciedev)))
/* Implicit DMA doesn't support for D2H */
#define VALID_DMA_D2H_CHAN(ch, pciedev) \
	(((ch) != DMA_CH2) || !PCIE_IDMA_ENAB(pciedev))

#if defined(PCIE_DMAINDEX16) || defined(PCIE_DMAINDEX32)
#define PCIE_DMA_INDEX /* Dongle supports DMAing of indices to/from Host */
#endif /* PCIE_DMAINDEX16 || PCIE_DMAINDEX32 */

#define PCIEDEV_SCHEDULER_AC_PRIO	0
#define PCIEDEV_SCHEDULER_TID_PRIO	1
#define PCIEDEV_SCHEDULER_LLR_PRIO	2

#define PCIEDEV_MAXWAIT_TRAP_HANDLER	50000000 /* useconds */
#define FLOWCTRL_MIN_PKTCNT		48

extern const uint8 prio2fifo[];
#define PCIEDEV_PRIO2AC(prio)  prio2fifo[(prio)]

extern const uint8 tid_prio_map[];
#define PCIEDEV_TID_PRIO_MAP(prio)  tid_prio_map[(prio)]

extern const uint8 AC_prio_map[];
#define PCIEDEV_AC_PRIO_MAP(prio)  AC_prio_map[(prio)]

/* AC priority map for Lossless Roaming */
extern const uint8 AC_prio_llr_map[];
#define PCIEDEV_AC_PRIO_LLR_MAP(prio)  AC_prio_llr_map[(prio)]
extern const uint8 tid2AC_llr_map[];
#define PCIEDEV_TID2AC_LLR(prio)  tid2AC_llr_map[(prio)]

extern const uint8 tid2AC_map[];
#define PCIEDEV_TID2AC(prio)  tid2AC_map[(prio)]

/* Helper Macros */
#define FL_MAXPKTCNT(zfl) ((zfl)->flow_info.maxpktcnt)

#define FL_W_NEW(zfl) ((zfl)->w_new)
#define FL_DA(zfl) ((zfl)->flow_info.da)
#define FL_TID_AC(zfl) ((zfl)->flow_info.tid_ac)
#define FL_TID_IFINDEX(zfl) ((zfl)->flow_info.ifindex)
#define FL_PKTINFLIGHT(zfl) ((zfl)->flow_info.pktinflight)
#define FL_ID(zfl) ((zfl)->ringid)
#define FL_STATUS(zfl) ((zfl)->status)

/* **** Structure definitons **** */

struct msgbuf_ring;
struct dngl_bus;
typedef struct dngl_bus pciedev_t;

#if defined(WLSQS)

#if !(defined(BCMHWA) && defined(HWA_TXPOST_BUILD))
#error "SQS needs both BCMHWA and HWA_TXPOST_BUILD defined"
#endif /* !(BCMHWA && HWA_TXPOST_BUILD) */

#define SQS_ASSERT()            do { /* no-op */ } while (0)

/* Start of a stride walk */
extern void sqs_v2r_init(struct dngl_bus *pciedev);

/* Per flowring, a v2r_request from WL is sent to SQS V2R convertor, may fail */
extern uint16  sqs_v2r_request(void* arg, uint16 ringid, uint16 v2r_request);
extern bool  sqs_flow_ring_active(void* arg, uint16 ringid);

/* On a failed V2R conversion request (no real packets), retry to resume */
extern int  sqs_v2r_resume(struct dngl_bus *pciedev);

/* End a Stride walk, or sync after a sqs_v2r_resume */
extern void sqs_v2r_sync(struct dngl_bus *pciedev);

/* Resume the stride walk on txstatus */
extern void pciedev_sqs_stride_resume(struct dngl_bus *pciedev, uint8 handle);
/* Callback from TAF module to indicate end of a pull request set */
extern int  sqs_eops_rqst(void* arg);
#else  /* ! WLSQS */
#define SQS_ASSERT()            ASSERT(0)
#endif /* ! WLSQS */

#ifdef WLCFP

/**
 * Cached Flow Processing Flow Classifier
 *
 * When Tx post workitems are converted to packets, they are first classified
 * as CFP fastpath capable and non CFP capable lists. The CFP capable flows are
 * sorted into individual per priority pktlists.
 *
 * After all workitems are converted to packets, each CFP capable list is
 * dispatched via the CFP fastpath cfp_sendup(). Non CFP capable list is
 * dispatched via the slowpath dngl_sendup().
 *
 * In Access Point, a single flowring may be used to carry packets of all
 * priorities destined to the same station. A flowring will be associated with
 * a single CFP FlowId during flowring creation time, that identifies the
 * Cache Flow Processing fastpath's Transmit Control Block.
 *
 * CFP does not include support for "flow" classification by MacSA.
 */

/*
 * A total of (8 + 1) lists are maintained, the first 8 are per priority packet
 * lists for CFP capable cfp_sendup() fastpath, and the last list is for the
 * default dngl_sendup() slowpath (similar to dongle tx packet chaining).
 */
#define CFP_PKTLISTS_TOTAL          (NUMPRIO + 1)
#define DNGL_SENDUP_PKTLIST_IDX     (CFP_PKTLISTS_TOTAL - 1)

#define CFP_FLOWS_STATS
//#define CFP_FLOWS_DEBUG
#define CFP_FLOWS_NOOP              do { /* noop */ } while (0)

#if defined(CFP_FLOWS_STATS)
#define CFP_FLOW_STATS_CLR(x)       (x) = 0U
#define CFP_FLOW_STATS_ADD(x, c)    (x) = (x) + (c)
#define CFP_FLOW_STATS_INCR(x)      (x) = (x) + 1
extern void cfp_flows_dump(void *arg);
#else  /* ! CFP_FLOWS_STATS */
#define CFP_FLOW_STATS_CLR(x)       CFP_FLOWS_NOOP
#define CFP_FLOW_STATS_ADD(x, c)    CFP_FLOWS_NOOP
#define CFP_FLOW_STATS_INCR(x)      CFP_FLOWS_NOOP
#endif /* ! CFP_FLOWS_STATS */
#if defined(CFP_FLOWS_DEBUG)
#define CFP_FLOWS_TRACE(args)       printf args
#else
#define CFP_FLOWS_TRACE(args)       CFP_FLOWS_NOOP
#endif // endif

/** CFP packet list */
typedef struct cfp_pktlist {
	void            *head;  /**< head packet */
	void            *tail;  /**< tail packet */
	uint16           pkts;  /**< total packetis in list */
	uint8            prio;  /**< priority of all packets in list */
	uint8            pend;  /**< weight of pending counter increment */
	uint32           enqueue_stats;
	uint32           dispatch_stats;
} cfp_pktlist_t;

/** CFP based flow classifier of CFP capable and non capable packet flows */
typedef struct cfp_flows {
	uint8           pend;   /**< pending pktlists, non-CFP list counts as 2 */
	cfp_pktlist_t  *curr;   /**< current CFP capable non-empty pktlist */
	cfp_pktlist_t   pktlists[CFP_PKTLISTS_TOTAL]; /**< total packet lists */
	uint32          pend_dispatch_stats;
	uint32          prio_dispatch_stats;
	uint32          dngl_dispatch_stats;
} cfp_flows_t;
#else /* !WLCFP */
/*
 * For non cfp builds, redefine to 'int'
 * Catch accidental cfp access in non-cfp builds
 */
typedef int cfp_flows_t;
#endif /* WLCFP */

/* pktlist to store suppressed amsdu */
typedef struct amsdu_sup_pktlist {
	void            *head;  /**< head packet */
	void            *tail;  /**< tail packet */
	uint16           pkts;  /**< total packetis in list */
} amsdu_sup_pktlist_t;

/* Queue to store resp to ctrl msg */
typedef struct pciedev_ctrl_resp_q {
	uint8	w_indx;
	uint8	r_indx;
	uint8	depth;
	uint8	status;
	uint8	num_flow_ring_delete_resp_pend;
	uint8	num_flow_ring_flush_resp_pend;
	uint8	ctrl_resp_q_full;	/* Cnt: ring_cmplt_q is full */
	uint8	dma_desc_lack_cnt;	/* Cnt: not enough DMA descriptors */
	uint8	lcl_buf_lack_cnt;	/* Cnt: not enough local bufx */
	uint8	ring_space_lack_cnt;	/* Cnt: no space in host ring */
	uint8	dma_problem_cnt;	/* Cnt: problem w. DMA */
	uint8	scheduled_empty;	/* Cnt: scheduled when the queue is empty */
	ctrl_completion_item_t  *response;
} pciedev_ctrl_resp_q_t;

typedef struct flow_info {
	uint8	sa[ETHER_ADDR_LEN];
	uint8	da[ETHER_ADDR_LEN];
	uint8	tid_ac;
	uint8	ifindex;
	uint16	pktinflight;
	uint16	maxpktcnt;	/**< Number of packets need to keep flow to max throughput */
	uint8	flags;
	uint8	reqpktcnt;	/**< Number of packets FW requested */
	uint16	pktinflight_max;
	uint16  max_rate;
	uint8	frm_mask;
	uint8	iftype_ap;	/* 1 if AP interface else 0 */
} flow_info_t;

/** Node to store lcl (local) buffer pkt next/orev pointers */
typedef struct	lcl_buf {
	struct  lcl_buf *nxt;
	struct  lcl_buf *prv;
	void	*p;
} lcl_buf_t;

/** Current lcl (local) buffer that is in use */
typedef struct inuse_lcl_buf {
	void *p;
	uint8 max_items;  /**< one lcl buffer may contain multiple messages from the host */
	uint8 used_items; /**< number of host messages already parsed by firmware */
} inuse_lcl_buf_t;

/** inuse lcl (local) buf pool */
typedef struct inuse_lclbuf_pool {
	uint16 depth;
	uint16 w_ptr;
	uint16 r_ptr;
	uint8 inited;
	inuse_lcl_buf_t	buf[0];
} inuse_lclbuf_pool_t;

/** Pool of circular buffers */
typedef struct cir_buf_pool {
	uint16		depth;
	uint8		item_size;
	uint16		w_ptr;
	uint16		r_ptr;
	char*		buf;
	uint16		availcnt;
} cir_buf_pool_t;

/** Pool of lcl (local) bufs */
typedef struct lcl_buf_pool {
	uint8		buf_cnt;
	uint8		item_cnt;
	uint8		availcnt;
	uint8		rsvd;
	lcl_buf_t	*head;
	lcl_buf_t	*tail;
	lcl_buf_t	*free;
	uchar		*local_buf_in_use;
	uint32		pend_item_cnt;
	/** inuse_pool: local buffers set up to receive rx buf post messages from host */
	inuse_lclbuf_pool_t *inuse_pool;
	lcl_buf_t	buf[0];
} lcl_buf_pool_t;

typedef enum bcmpcie_mb_intr_type {
	PCIE_BOTH_MB_DB1_INTR = 0,
	PCIE_MB_INTR_ONLY,
	PCIE_DB1_INTR_ONLY,
	PCIE_NO_MB_DB1_INTR
} bcmpcie_mb_intr_type_t;

typedef uint16 (*msgring_process_message_t)(struct dngl_bus *, void *p, uint16 data_len,
	struct msgbuf_ring *ring);

/** double linked list node for priority ring */
struct prioring_pool {
	dll_t		node;				/**< double linked node */
	dll_t		active_flowring_list;		/**< Active list */
	dll_t		*last_fetch_node;		/**< last fetch node in the active list */
	uint8		tid_ac;					/* 0-7 for tid, 0-3 for AC prios */
	bool		inited;				/**< is in active_prioring_list */
	bool		schedule;
	uint16		*maxpktcnt;		/* max packet fetch count per interface index */
};

typedef struct flowring_pool	flowring_pool_t;
typedef struct prioring_pool	prioring_pool_t;

typedef struct ring_ageing_info {
	uint16 sup_cnt;
} ring_ageing_info_t;

typedef struct _flow_stats {
	/** When the last fetch is done */
	uint32	last_fetch_times[PCIEDEV_MAX_FLOW_STATS_COUNT];
	uint32	last_fetch_rdinds[PCIEDEV_MAX_FLOW_STATS_COUNT]; /* Last rdind | Last w_ind */
	uint32	fetch_count; /* Number of fetches done */
	/** When is port opened last */
	uint32	last_port_open_times[PCIEDEV_MAX_FLOW_STATS_COUNT];
	uint32	port_open_count; /* Number of port open */
	uint32	last_tx_compl_times[PCIEDEV_MAX_FLOW_STATS_COUNT]; /* When is tx status sent last */
	uint32	tx_compl_count; /* Number of port open */
	/** When is port closed last */
	uint32	last_port_close_times[PCIEDEV_MAX_FLOW_STATS_COUNT];
	uint32	port_close_count; /* Number of port close */
	/** When is flow suppressed last */
	uint32	last_suppress_times[PCIEDEV_MAX_FLOW_STATS_COUNT];
	uint32	suppress_status[PCIEDEV_MAX_FLOW_STATS_COUNT]; /* status | rindex */
	uint32	suppress_count; /* Number of suppressions */
	/** First fetch timestamp after a tx completion */
	uint32	first_fetch_after_tx_compl;
	/** Last Sequence # Suppressed */
	uint32	last_suppress_seq[PCIEDEV_MAX_FLOW_STATS_COUNT];
	/** Last rindex at tx completion */
	uint32	last_tx_compl_rdinds[PCIEDEV_MAX_FLOW_STATS_COUNT];
	uint32	last_flush_times[PCIEDEV_MAX_FLOW_STATS_COUNT]; /* last  flow flush times */
	uint32	flush_pktinflight[PCIEDEV_MAX_FLOW_STATS_COUNT]; /* pktinflight */
	uint32	last_flush_rdwr[PCIEDEV_MAX_FLOW_STATS_COUNT]; /* rd/wr index at flush */
	uint32	flush_count; /* Number of flow flush */
} flow_stats_t;

/**
 * State of a H2D consumer or D2H producer in dongle.
 *
 * Common structure used for both common rings and dynamic rings. Most fields
 * in the structure pertain to the H2D dynamic ring, aka Tx Post flow ring.
 *
 * TxPost rings may be "rolled back" to to packet suppression downstream.
 * Sliding window into the TxPost is used to make the structure independent of
 * the host side Flowring depth, and arrays such as inprocess, status_cmpl,
 * reuse_sup_seq and reuse_seq_list are sized to the sliding window size,
 * namely bitmap_size.
 *
 * lcl_buf or cir_buf may be used for managing local storage for mem2mem DMA
 * transfer of work items from the host.
 *
 * Use MSGBUF_XYZ macros, especially when accessing PCIE IPC structures.
 */
typedef struct msgbuf_ring
{
	bool		inited;
	uchar		name[MSGBUF_RING_NAME_LEN];
	pcie_ipc_ring_mem_t	*pcie_ipc_ring_mem; /**< ptr into PCIE IPC ring_mem */
	pcie_rw_index_t *wr_ptr; /**< into array of WR indices, maybe shared */
	pcie_rw_index_t *rd_ptr; /**< into array of RD indices, maybe shared */
	void		*pciedev;
	uint16		wr_pending;
	uint16		fetch_ptr;	/**< flow fetch pointer */
	uint8		current_phase;
	uchar		handle;		/**< upper layer ID */
	bool		taf_capable;
	bool		paused;
	uint16		fetch_pending;  /**< number of packets to be fetched */
	uint16		ringid; /**< scope: global for common rings, per dir for dynamic */
	uint16		status;
	flow_info_t	flow_info;
	lcl_buf_pool_t	*buf_pool;	/**< local buffer pool */
	cir_buf_pool_t	*cbuf_pool;	/**< local circular buffer pool */
	msgring_process_message_t process_rtn;
	uint16		cfp_flowid;

	/* Bitmaps/arrays tracking status information */
	uint16		bitmap_size;	/* size of bitmap arrays */
	uint8		*inprocess;	/* packets that have been fetch and queued to wl */
	uint8		*status_cmpl;	/* packets that have completed tx (success or dropped) */
	uint8		*reuse_sup_seq;	/* suppressed pkts to reuse d11 seq numbers */
	uint16		*reuse_seq_list;	/* Place holder for d11 Seq numbers from the wl */
	uint8		*reuse_key_seq_list;	/* place holder for #PN from wl */
	uint8		*reuse_sup_key_seq;	/* suppressed pkts to reuse #PN */
	/** Store for request id if we are going to send responses later */
	uint32		request_id;

	flowring_pool_t *lcl_pool;	/**< local flow ring pool */

	bool		dmaindex_h2d_supported; /**< Can we DMA r/w indices from host? */
	bool		dmaindex_d2h_supported; /**< Can we DMA r/w indices to host? */
	ring_ageing_info_t	ring_ageing_info;
#ifdef H2D_CHECK_SEQNUM
	uint32		h2d_seqnum;
#endif /* H2D_CHECK_SEQNUM */
	uint8		msi_vector_offset; /* msi vector offset per rings */
	/* Each tock unit is 32 msec for a total of 8 sec (256*32) */
	/* Max suppress retry time in tock units */
	uint8		max_sup_tocks;
	uint8		max_sup_retries; /* Max # of suppress retries */
	bool		sup_exp_enab; /* Flag to enable/disable suppression retry feature */
	uint8		*sup_exp_block; /* 4 bits retry counters per rindex */
	/* # of dropped frames due to excessive sup retries */
	uint32		dropped_counter_sup_retries;
	/* # of dropped frames due to excessive timer sup retries */
	uint32		dropped_timer_sup_retries;
	uint32		sup_retries; /* # of excessive sup retries */
	/* Retry timestamp based on hnd_time() */
	/* [(hnd_time() >> 5) & 0xFF] is saved. */
	uint8		*sup_exp_tstamp;
	flow_stats_t	*flow_stats; /* Per Flow stats/counters */
	uint8		*tcb_state;
	amsdu_sup_pktlist_t	amsdu_sup_part1;
	amsdu_sup_pktlist_t	amsdu_sup_pktc;
	uint16		wr_peeked;
	uint16		d2h_q_txs_pending;
	uint16		work_item_fetch_limit; /* Limit work item fetch: */
#ifdef PCIEDEV_FAST_DELETE_RING
	uint16		delete_idx;	/**< fast delete ring rd idx */
#endif /* PCIEDEV_FAST_DELETE_RING */
} msgbuf_ring_t;

/** PCIE IPC Accessors for msgbuf_ring_t::pcie_ipc_ring_mem */

/* msgbuf pointer into array of PCIE IPC ring mem */
#define MSGBUF_IPC_RING_MEM(msgbuf) (msgbuf)->pcie_ipc_ring_mem

#define MSGBUF_IPC_RING_ID(msgbuf)  MSGBUF_IPC_RING_MEM(msgbuf)->id

/* msgbuf 64bit host address of the host resident ring buffer */
#define MSGBUF_HADDR64(msgbuf)      (MSGBUF_IPC_RING_MEM(msgbuf)->haddr64)

/* msgbuf low 32bit of host address, used in offset computations in dongle */
#define MSGBUF_BASE(msgbuf)         (HADDR64_LO(MSGBUF_HADDR64(msgbuf)))

/* item_type specifies, legacy, cwi32, cwi64, acwi32, acwi64 format usage */
#define MSGBUF_ITEM_TYPE(msgbuf)    (MSGBUF_IPC_RING_MEM(msgbuf)->item_type)

/* max number of items in the msgbuf rings (a ring may store max-1 items) */
#define MSGBUF_MAX(msgbuf)	        (MSGBUF_IPC_RING_MEM(msgbuf)->max_items)

/* length of a message (aka work item) in the msgbuf ring */
#define MSGBUF_LEN(msgbuf)	        (MSGBUF_IPC_RING_MEM(msgbuf)->item_size)
#define MSGBUF_ITEM_SIZE(msgbuf)    MSGBUF_LEN(msgbuf)

/** Fetch RD,WR pcie_rw_index_t pointers into array of indices */
/* Pointer into the array of rd indices in dongle's memory */
#define MSGBUF_RD_PTR(msgbuf)       (((msgbuf)->rd_ptr))
/* Pointer into the array of wr indices in dongle's memory */
#define MSGBUF_WR_PTR(msgbuf)       (((msgbuf)->wr_ptr))

/* RD and WR index values fetched from the PCIE IPC state */
#define MSGBUF_RD(msgbuf)		    (*(MSGBUF_RD_PTR(msgbuf)))
#define MSGBUF_WR(msgbuf)		    (*(MSGBUF_WR_PTR(msgbuf)))

/** Address arithmetic in dongle is only the low 32bit address from host */

/** Miscellaneous Accessors of msgbuf_ring_t (local unshared) indices */
#define MSGBUF_WR_PEND(msgbuf)      ((msgbuf)->wr_pending)
/* #define MSGBUF_RD_PEND(msgbuf) ((msgbuf)->fetch_ptr) */

/** Used to implement a sliding window in a ring */
#define MSGBUF_MODULO_IDX(msgbuf, index)   ((index) % (msgbuf)->bitmap_size)

/** Space availability macros on a msgbuf_ring_t */
/* Used in H2D consumer path */
#define MSGBUF_READ_AVAIL_SPACE(msgbuf) \
	READ_AVAIL_SPACE(MSGBUF_WR(msgbuf), (msgbuf)->fetch_ptr, MSGBUF_MAX(msgbuf))

/* Required by SQS to get the number of virtual packets */

/* SQS: Value of WR index since the last time vpkts were computed */
#define MSGBUF_WR_PEEKED(msgbuf)     ((msgbuf)->wr_peeked)

/* SQS: Index in msgbuf where the next fetch should occur */
#define MSGBUF_V2R_WRINDEX(msgbuf)   ((msgbuf)->fetch_ptr)

/* If WR index changes since last peek, then new pkts have arrived */
#ifdef HWA_TXPOST_BUILD
#define MSGBUF_VPKTS(msgbuf) \
	({ \
		int new_wr = MSGBUF_WR(msgbuf); \
		int old_wr = MSGBUF_WR_PEEKED(msgbuf); \
		int vpkts  = NTXPACTIVE(old_wr, new_wr, MSGBUF_MAX(msgbuf)); \
		MSGBUF_WR_PEEKED(msgbuf) = new_wr; \
		(vpkts); \
	})
#define MSGBUF_VPKTS_AVAIL(msgbuf) \
	NTXPACTIVE((msgbuf)->fetch_ptr, MSGBUF_WR(msgbuf), MSGBUF_MAX(msgbuf))
#else /* !HWA_TXPOST_BUILD */
#define MSGBUF_VPKTS(msgbuf) \
	({ \
		int new_wr = MSGBUF_WR(msgbuf); \
		int old_wr = MSGBUF_WR_PEEKED(msgbuf); \
		int vpkts  = READ_AVAIL_SPACE(new_wr, old_wr, MSGBUF_MAX(msgbuf)); \
		MSGBUF_WR_PEEKED(msgbuf) = \
			((old_wr + vpkts) == MSGBUF_MAX(msgbuf)) ? 0 : new_wr; \
		(vpkts); \
	})
#define MSGBUF_VPKTS_AVAIL(msgbuf)	MSGBUF_READ_AVAIL_SPACE(msgbuf)
#endif /* !HWA_TXPOST_BUILD */

/* Used in D2H producer path */
#define MSGBUF_WRITE_SPACE_AVAIL(msgbuf) \
	WRITE_SPACE_AVAIL(MSGBUF_RD(msgbuf), MSGBUF_WR_PEND(msgbuf), MSGBUF_MAX(msgbuf))

#define MSGBUF_CHECK_WRITE_SPACE(msgbuf) \
	CHECK_WRITE_SPACE(MSGBUF_RD(msgbuf), MSGBUF_WR_PEND(msgbuf), MSGBUF_MAX(msgbuf))

#define MSGBUF_CHECK_NOWRITE_SPACE(msgbuf) \
	CHECK_NOWRITE_SPACE(MSGBUF_RD(msgbuf), MSGBUF_WR_PEND(msgbuf), MSGBUF_MAX(msgbuf))

/** double linked list node */
struct flowring_pool {
	dll_t node;
	msgbuf_ring_t *ring;
	prioring_pool_t *prioring;
};

typedef struct pcie_dma_item_info {
	uint16	len;
	uint8	msg_type;
	uint8	flags;
	msgbuf_ring_t *ring;
	uint32	timestamp;
	uint32  addr;
	uint32  haddr;
} dma_item_info_t;

typedef struct pcie_dma_queue {
	uint16	r_index;
	uint16	w_index;
	uint16	max_len;
	uint8	chan;
	dma_item_info_t dma_info[0];
} dma_queue_t;

typedef struct host_dma_buf {
	struct host_dma_buf *next;
	haddr64_t  buf_addr;
	uint32 pktid;
	uint16	len;
	uint16	rsvd;
} host_dma_buf_t;

typedef struct host_dma_buf_pool {
	host_dma_buf_t	*free;
	host_dma_buf_t	*ready;
	uint16		max;
	uint16		ready_cnt;
	host_dma_buf_t	pool_array[0];
} host_dma_buf_pool_t;

typedef struct pciedev_err_cntrs {
	uint16 mem_alloc_err_cnt;
	uint16 rxcpl_err_cnt;
	uint16 dma_err_cnt;
	uint16 pktfetch_cancel_cnt;
	uint16 unsupported_request_err_cnt;
	uint16 linkdown_cnt;
} pciedev_err_cntrs_t;

typedef struct pciedev_dbg_bus_cntrs {
	uint32 zero_intstatus;
	uint32 set_indices_fail;
	uint32 d2h_fetch_cancelled;
	uint32 h2d_fetch_cancelled;
	uint32 h2d_dpc_bad_dma_sts;
	uint32 setup_flowring_failed;
	uint32 alloc_ring_failed;
	uint32 link_host_msgbuf_failed;
	uint32 h2d_ctrl_submit_failed;
	uint32 txpyld_failed;
	uint32 tx_general_status_failed;
	uint32 tx_ring_status_failed;
	uint32 send_ioctl_ack_failed;
	uint32 send_ioctl_ptr_failed;
	uint32 proc_d2h_wl_event;
	uint32 xmit_wlevnt_cmplt_failed;
	uint32 d2h_mbdata_send;
	uint32 send_ioctl_completion_failed;
	uint32 pciedev_process_flow_ring_create_rqst;
	uint32 pciedev_process_flow_ring_delete_rqst;
	uint32 pciedev_send_flow_ring_delete_resp;
	uint32 pciedev_process_flow_ring_flush_rqst;
	uint32 pciedev_send_flow_ring_flush_resp;
	uint32 pciedev_send_d2h_soft_doorbell_resp;
	uint32 pciedev_init_inuse_lclbuf_pool_t;
	uint32 pciedev_schedule_flow_ring_read_buffer;
	uint32 pciedev_flow_schedule_timerfn;
	uint32 pciedev_get_maxpkt_fetch_count;
	uint32 pciedev_allocate_flowring_fetch_rqst_entry;
	uint32 pciedev_update_txstatus;
	uint32 pciedev_get_host_addr;
	uint32 pciedev_add_to_inuselist;
	uint32 pciedev_dispatch_fetch_rqst;
	uint32 pciedev_deque_fetch_cmpltq;
	uint32 pciedev_process_tx_payload;
	uint32 resource_avail_check_no_dma_descriptor;
	uint32 resource_avail_check_cbuf_buf_null;
	uint32 resource_avail_check_no_host_space;
	uint32 pciedev_check_process_d2h_message;
	uint32 pciedev_process_d2h_rxpyld;
	uint32 pciedev_process_d2h_txmetadata;
	uint32 pciedev_lbuf_callback;
	uint32 pciedev_queue_d2h_req_send;
	uint32 pciedev_tx_pyld;
	uint32 pciedev_tx_msgbuf;
	uint32 pciedev_process_reqst_packet;
	uint32 pciedev_push_pkttag_tlv_info;
	uint32 pciedev_run;
	uint32 pciedev_get_cirbuf_pool;
	uint32 pciedev_get_src_addr;
	uint32 pciedev_ring_update_readptr;
	uint32 pciedev_get_ring_space;
	uint32 pciedev_queue_rxcomplete_local;
	uint32 pciedev_queue_rxcomplete_msgring;
	uint32 pciedev_xmit_msgbuf_packet;
	uint32 pciedev_create_d2h_messages;
	uint32 pciedev_xmit_txstatus;
	uint32 pciedev_queue_txstatus;
	uint32 pciedev_xmit_rxcomplete;
	uint32 dma_msgbuf_txfast;
	uint32 pciedev_handle_h2d_pyld;
	uint32 pciedev_handle_h2d_msg_txsubmit;
	uint32 pciedev_h2dmsgbuf_dma;
	uint32 pciedev_free_lclbuf_pool;
	uint32 pciedev_fillup_rxcplid;
	uint32 pciedev_ring_update_writeptr;
	uint32 pciedev_fillup_haddr;
	uint32 pciedev_read_flow_ring_host_buffer_dest_addr_null;
	uint32 do_fetch_false;
	uint32 do_fetch_true;
	uint32 pciedev_process_rxpost_msg;
	uint32 pciedev_dispatch_core_fetch_rqst;
	uint32 pciedev_send_ts_cmpl;
	uint32 pciedev_process_d2h_tsinfo;
	uint32 pciedev_free_flowring_allocated_info;
	uint32 pciedev_d2h_read_barrier;
	uint32 pciedev_msgbuf_intr_process;
	uint32 pciedev_h2d_start_fetching_host_buffer;
	uint32 pciedev_get_fetch_count_amsdu;
	uint32 pciedev_lclpool_alloc_lclbuf;
	uint32 pciedev_put_host_addr;
	uint32 pciedev_start_reading_host_buffer;
	uint32 pciedev_buzzz_d2h_copy;
} pciedev_dbg_bus_cntrs_t;

typedef struct pciedev_fetch_cmplt_q {
	struct fetch_rqst *head;
	struct fetch_rqst *tail;
	uint32 count;
} pciedev_fetch_cmplt_q_t;

typedef struct flow_fetch_rqst {
	bool	used;
	uint8	index;
	uint16	start_ringinx;
	struct msgbuf_ring  *msg_ring;
	fetch_rqst_t	rqst;
	uint16	offset; /* offset location from where next processing need to done */
	uint8	flags;  /* flags to fetch node state See below */
	struct flow_fetch_rqst *next;

} flow_fetch_rqst_t;

/*
 * Flags to indicate if fetch node went through processing once/
 * can it be freed if its order is met
 */
#define PCIEDEV_FLOW_FETCH_FLAG_REPROCESS	0x01
#define PCIEDEV_FLOW_FETCH_FLAG_FREE		0x02

typedef struct fetch_req_list {
	flow_fetch_rqst_t	*head;
	flow_fetch_rqst_t	*tail;
} fetch_req_list_t;

typedef void (*pciedev_ds_cbs_t)(void *cbarg);
typedef struct pciedev_ds_state_tbl_entry {
	pciedev_ds_cbs_t action_fn;
	int              transition;
} pciedev_ds_state_tbl_entry_t;

typedef enum _ds_check_fail {
	DS_NO_CHECK = 0,
	DS_LTR_CHECK,
	DS_BUFPOOL_PEND_CHECK,
	DS_DMA_PEND_CHECK,
	DS_IDMA_PEND_CHECK
} ds_check_fail_t;

typedef struct _ds_check_fail_log {
	uint32 ltr_check_fail_ct;
	uint32 rx_pool_pend_fail_ct;
	uint32 tx_pool_pend_fail_ct;
	uint32 h2d_dma_txactive_fail_ct;
	uint32 h2d_dma_rxactive_fail_ct;
	uint32 d2h_dma_txactive_fail_ct;
	uint32 d2h_dma_rxactive_fail_ct;
	uint32 h2d2_dma_txactive_fail_ct;
} ds_check_fail_log_t;

typedef struct pciedev_deepsleep_log {
	int ds_state;
	int ds_event;
	int ds_transition;
	uint32  ds_time;
	ds_check_fail_log_t ds_check_fail_cntrs;
} pciedev_deepsleep_log_t;

#define PCIEDEV_CTRL_RESP_Q_WR_IDX	1
#define PCIEDEV_CTRL_RESP_Q_RD_IDX	2

/** used to record start time for metric */
typedef struct metric_ref {
	uint32 active;
	uint32 d3_suspend;
	uint32 perst;
	uint32 ltr_active;
	uint32 ltr_sleep;
	uint32 deepsleep;
} metric_ref_t;

/*
 * PCIE DMA Index feature is enabled using the "-dmaindex16" or "-dmaindex32"
 * dongle build targets. When "-dmaindex##" is enabled, the D2H DMA sync targets
 * "-chkd2hdma" or "-xorcsum" are not needed. When Compact Work Item "-acwi"
 * target is enabled, there is no marker field in D2H completion messages for
 * TxCpln or RxCpln, and the explicit D2H Sync mechanism cannot be supported.
 */

/*
 * DMA Index feature: Instead of having the host to PIO into dongle's memory
 * where the 4 arrays carrying the H2D WR, H2D RD, D2H WR and D2H RD indices,
 * the DMA Index feature is used to sync all indices into host memory. Host may
 * directly access the indices from local host system memory (DDR).
 *
 * An optimization call SBTOPCIE_INDICES may be used to sync individually the
 * H2D RD, D2H WR and D2H RD index. SBTOPCIE is not DMA based and CPU savings
 * (not requiring to program mem2mem engines+DMA dones) and PCIE bandwidth
 * reduction (2B index instead of full array) is afforded.
 *
 * Dongle bus layer uses 4 dmaindex_info objects to track the dongle side and
 * host side addresses.
 */

#define PCIE_DMAINDEX_MAXFETCHPENDING   1
#define PCIE_DMAINDEX_LOG_MAX_ENTRY     10

//#define PCIE_DMA_INDEX_DEBUG

/** A DMA Index Context describes an array of indices in dongle and host */
typedef struct dmaindex {
	haddr64_t   haddr64; /**< host address of indices array */
	dma64addr_t daddr64; /**< dongle address of indices array */
	uint32      size;    /**< size in bytes of indices array to dma */
	bool        inited;  /**< host address has been initialized (link phase) */
} dmaindex_t;

/** Logging of dmaindex transaction. Logging done per direction. */
typedef struct dmaindex_log {
	haddr64_t   haddr64;
	dma64addr_t daddr64;
	uint32      size;
	bool        is_h2d_dir;
	uint32      timestamp;
	uint32      index[3]; /**< first 12B in the array */
} dmaindex_log_t;

/* for implicit DMA */

/** Array of H2D WR indices array, similar to dmaindex */

typedef struct idma_metrics {
	uint32      intr_num[MAX_IDMA_DESC];
} idma_metrics_t;

typedef struct idma_info {
	uint32      index;
	bool        enable; /* use dma channel 2 */
	bool        inited; /* host and dev address are ready */
	bool        desc_loaded; /* desc is loaded */
	bool        chan_disabled; /* for memory retention */
	dmaindex_t  *dmaindex; /* dmaindex info */
	uint32      chan_disable_err_cnt;
	idma_metrics_t metrics;
	uint16      dmaindex_sz;
} idma_info_t;

typedef struct dma_ch_info {
	uint32  dma_ch1_h2d_cnt;
	uint32  dma_ch1_d2h_cnt;
	uint32  dma_ch2_h2d_cnt;
	uint32  dma_ch2_d2h_cnt;
} dma_ch_info_t;

#if defined(PCIE_D2H_DOORBELL_RINGER)
/*
 * Dongle may ring doorbell on host by directly writing to a 32bit host address
 * using sbtopcie translation 0.
 */
typedef void (*d2h_doorbell_ringer_fn_t)(struct dngl_bus *pciedev,
	uint32 value, uint32 addr);

typedef struct d2h_doorbell_ringer {
	bcmpcie_soft_doorbell_t db_info; /**< host doorbell, with remapped haddr */
	d2h_doorbell_ringer_fn_t db_fn;  /**< callback function to ring doorbell */
} d2h_doorbell_ringer_t;

#endif /* PCIE_D2H_DOORBELL_RINGER */

#define PCIEDEV_IOCTLREQ_PENDING	0x0001
#define PCIEDEV_IOCTLRESP_PENDING	0x0002
#define PCIEDEV_MBDATA_PENDING		0x0004
#define PCIEDEV_RXCPL_PENDING		0x0008
#define PCIEDEV_TXSTS_PENDING		0x0010
#define PCIEDEV_RXCTL_RESP_PENDING	0x0020
#define PCIEDEV_TSREQ_PENDING		0x0040

#define SET_IOCTLREQ_PENDING(pciedev) \
	mboolset((pciedev)->active_pending, PCIEDEV_IOCTLREQ_PENDING)
#define SET_IOCTLRESP_PENDING(pciedev) \
	mboolset((pciedev)->active_pending, PCIEDEV_IOCTLRESP_PENDING)
#define SET_MBDATA_PENDING(pciedev) \
	mboolset((pciedev)->active_pending, PCIEDEV_MBDATA_PENDING)
#define SET_RXCPL_PENDING(pciedev) \
	mboolset((pciedev)->active_pending, PCIEDEV_RXCPL_PENDING)
#define SET_TXSTS_PENDING(pciedev) \
	mboolset((pciedev)->active_pending, PCIEDEV_TXSTS_PENDING)
#define SET_RXCTLRESP_PENDING(pciedev) \
	mboolset((pciedev)->active_pending, PCIEDEV_RXCTL_RESP_PENDING)
#define SET_TSREQ_PENDING(pciedev) \
	mboolset((pciedev)->active_pending, PCIEDEV_TSREQ_PENDING)

#define CLR_IOCTLREQ_PENDING(pciedev) \
	mboolclr((pciedev)->active_pending, PCIEDEV_IOCTLREQ_PENDING)
#define CLR_IOCTLRESP_PENDING(pciedev) \
	mboolclr((pciedev)->active_pending, PCIEDEV_IOCTLRESP_PENDING)
#define CLR_MBDATA_PENDING(pciedev) \
	mboolclr((pciedev)->active_pending, PCIEDEV_MBDATA_PENDING)
#define CLR_RXCPL_PENDING(pciedev) \
	mboolclr((pciedev)->active_pending, PCIEDEV_RXCPL_PENDING)
#define CLR_TXSTS_PENDING(pciedev) \
	mboolclr((pciedev)->active_pending, PCIEDEV_TXSTS_PENDING)
#define CLR_RXCTLRESP_PENDING(pciedev) \
	mboolclr((pciedev)->active_pending, PCIEDEV_RXCTL_RESP_PENDING)
#define CLR_TSREQ_PENDING(pciedev) \
	mboolclr((pciedev)->active_pending, PCIEDEV_TSREQ_PENDING)

#define IS_IOCTLREQ_PENDING(pciedev) \
	mboolisset((pciedev)->active_pending, PCIEDEV_IOCTLREQ_PENDING)
#define IS_IOCTLRESP_PENDING(pciedev) \
	mboolisset((pciedev)->active_pending, PCIEDEV_IOCTLRESP_PENDING)
#define IS_MBDATA_PENDING(pciedev) \
	mboolisset((pciedev)->active_pending, PCIEDEV_MBDATA_PENDING)
#define IS_RXCPL_PENDING(pciedev) \
	mboolisset((pciedev)->active_pending, PCIEDEV_RXCPL_PENDING)
#define IS_TXSTS_PENDING(pciedev) \
	mboolisset((pciedev)->active_pending, PCIEDEV_TXSTS_PENDING)
#define IS_RXCTLRESP_PENDING(pciedev) \
	mboolisset((pciedev)->active_pending, PCIEDEV_RXCTL_RESP_PENDING)

#define PCIEDEV_IOCTL_LOCK		0x0001
#define PCIEDEV_TIMESYNC_LOCK		0x0002

#define SET_IOCTL_LOCK(pciedev) \
	mboolset((pciedev)->ctrl_lock, PCIEDEV_IOCTL_LOCK)
#define SET_TIMESYNC_LOCK(pciedev) \
	mboolset((pciedev)->ctrl_lock, PCIEDEV_TIMESYNC_LOCK)

#define IS_IOCTL_LOCK(pciedev) \
	mboolisset((pciedev)->ctrl_lock, PCIEDEV_IOCTL_LOCK)
#define IS_TIMESYNC_LOCK(pciedev) \
	mboolisset((pciedev)->ctrl_lock, PCIEDEV_TIMESYNC_LOCK)

#define CLR_IOCTL_LOCK(pciedev) \
	mboolclr((pciedev)->ctrl_lock, PCIEDEV_IOCTL_LOCK)
#define CLR_TIMESYNC_LOCK(pciedev) \
	mboolclr((pciedev)->ctrl_lock, PCIEDEV_TIMESYNC_LOCK)

#define PCIEDEV_WORK_ITEM_FETCH_LIMIT_CTRL	1
#define PCIEDEV_WORK_ITEM_FETCH_LIMIT_UNDEF 0xffff
#define PCIEDEV_WORK_ITEM_FETCH_NO_LIMIT	0xffff
#define PCIEDEV_WORK_ITEM_FETCH_HOLD		0

#define D2H_REQ_PRIO_NUM	3
#define D2H_REQ_PRIO_0		0
#define D2H_REQ_PRIO_1		1
#define D2H_REQ_PRIO_2		2
#define D2H_REQ_PRIO_BMAP	((1 << D2H_REQ_PRIO_0) |(1 << D2H_REQ_PRIO_1) | \
					(1 << D2H_REQ_PRIO_2))

/* Structure to maintain per interface level txlfrag count */
/* Used to limit the buffer usage when multiple interafces are active */
typedef struct	ifidx_tx_account {
	uint16 cur_cnt;
	uint16 max_cnt;
} ifidx_tx_account_t;

/* used to collect more detailed data for IPC Stats */
typedef struct ipc_data {
	uint32 num_rxcmplt;
	uint32 num_txstatus;

	/* Last DB from Host */
	uint32 last_h2d_db0_time[PCIEDEV_IPC_MAX_H2D_DB0];
	uint32 h2d_db0_cnt;
	uint32 last_h2d_mbdb1_time;

	/* Last DB to Host */
	uint32 last_d2h_db_time;

	/* Last IOCTL */
	uint32 last_ioctl_ack_time;
	uint32 last_ioctl_proc_time;
	ioctl_req_msg_t last_ioctl_work_item;
	/* XXX: Memory considerations
	 * void *last_ioctl_payload;
	 * uint16 last_ioctl_payload_size;
	 */
	uint32 last_ioctl_compl_time;
	uint32	last_ts_cmpl_time; /* Timestamp of the last Host Timestamp Completion Message */
	host_timestamp_msg_t last_ts_work_item; /* Last Host Timestamp Message */
	uint32	last_ts_proc_time; /* Timestamp of the last Host Timestamp Request Message */
	uint32	cmn_rings_attach_time; /* Timestamp of common ring attach */

	/* Last H2D2 DB from Host */
	uint32 last_h2d2_db0_time[PCIEDEV_IPC_MAX_H2D_DB0];
	uint32 h2d2_db0_cnt;
	uint32 last_idma_done_time[PCIEDEV_IPC_MAX_H2D_DB0];
	uint32 h2d2_idma_cnt;
	uint32 last_h2d2_db0_in_ds_time[PCIEDEV_IPC_MAX_H2D_DB0];
	uint32 h2d2_db0_in_ds_cnt;
	uint32 last_h2d1_db1_time[PCIEDEV_IPC_MAX_H2D_DB0];
	uint32 h2d1_db1_cnt;
} ipc_data_t;

/* IPC Logs */
#ifdef DBG_IPC_LOGS
/* Used to collect log of mailbox events */
typedef enum {
	IPC_LOG_D2H_MB_FWHALT = 0,          /* FW halt */
	IPC_LOG_D2H_MB_DS_REQ,              /* ds req dev to host */
	IPC_LOG_D2H_MB_DS_EXIT,             /* ds exit dev to host */
	IPC_LOG_D2H_MB_D3_REQ_ACK,          /* d3 request ack dev to host */
	IPC_LOG_H2D_MBDB1_D3_INFORM,        /* d3 inform host to dev */
	IPC_LOG_H2D_MBDB1_DS_ACK,           /* ds ack host to dev */
	IPC_LOG_H2D_MBDB1_DS_NACK,          /* ds nack host to dev */
	IPC_LOG_H2D_MBDB1_DS_ACTIVE,        /* ds active host to dev */
	IPC_LOG_H2D_MBDB1_DS_DWAKE,         /* ds devwake host to dev */
	IPC_LOG_H2D_MBDB1_CONS_INT,         /* console interrupt host to dev */
	IPC_LOG_H2D_MBDB1_FW_TRAP,          /* force fw trap host to dev */
	IPC_LOG_H2D_MBDB1_D0_INFORM_IN_USE, /* d0 inform in use - host to dev */
	IPC_LOG_H2D_MBDB1_D0_INFORM,        /* d0 inform - host to dev */
	IPC_LOG_MB_INVALID
} ipc_mb_t;

/* Used to collect log of doorbell events */
typedef enum {
	IPC_LOG_H2D_DB0 = 0,    /* host to dev doorbell */
	IPC_LOG_DB_H2D_INVALID
} ipc_db_h2d_t;

typedef enum {
	D2H_DB = 0,             /* dev to host doorbell */
	DB_D2H_INVALID
} ipc_db_d2h_t;

/** Subtypes for IPC event type */
typedef union _ipc_event_sub_t {
	ipc_mb_t mb;            /* Mailbox event subtypes */
	ipc_db_h2d_t db_h2d;    /* H2D Doorbell event subtypes */
	ipc_db_d2h_t db_d2h;    /* D2H Doorbell event subtypes */
} ipc_event_sub_t;

typedef enum {
	MAILBOX = 0,
	DOORBELL_H2D,
	DOORBELL_D2H,
	NUM_IPC_EVENTS
} ipc_event_t;

enum {
	EVENT_BUF_POOL_LOW = 32,
	EVENT_BUF_POOL_MEDIUM = 64,
	EVENT_BUF_POOL_HIGH = 128,
	EVENT_BUF_POOL_HIGHEST = 256
};

/** Used to log Doorbells and Mailbox IPC events */
typedef struct _event_log_msg {
	uint32 timestamp;
	ipc_event_sub_t subtype;
} event_log_msg;

#define MAX_BUFFER_HISTORY 10

typedef struct _ipc_eventlog {
	event_log_msg buffer[MAX_BUFFER_HISTORY];
	uint8 count;
} ipc_eventlog;

typedef struct _ipc_log {
	ipc_eventlog eventlog[NUM_IPC_EVENTS];
} ipc_log_t;
#endif /* DBG_IPC_LOGS */

/* DEVICE_WAKE related params/counters */
typedef struct _dw_counters {
	uint8	dw_gpio;		/**< GPIO used for DEVICE_WAKE */
	uint32	dw_toggle_cnt; /* # of Device_Wake toggle */
	bool	dw_edges; /* Indicates short pulse */
	uint32	last_dw_toggle_time[PCIEDEV_IPC_MAX_DW_TOGGLE];
	bool	last_dw_state[PCIEDEV_IPC_MAX_DW_TOGGLE];
	bool	dw_state; /* Current DEVICE_WAKE state */
	bool	dw_before_bm; /* Flag to indicate DW toggle before BM enable */
	uint32	dw_before_bm_last; /* Last time DW toggle seen before BM enable */
	uint32	ds_req_sent_last; /* Timestamp of last DS Enter Request */
} dw_counters_t;

/* Structure to maintain MSI vector offset per D2H interrupt */
typedef struct pciedev_msi_info {
	bool msi_enable;
	uint8 msi_vector_offset[MSI_INTR_IDX_MAX];
} pciedev_msi_info_t;

typedef enum hostwake_reason_type {
	PCIE_HOSTWAKE_REASON_TRAP = -1,
	PCIE_HOSTWAKE_REASON_WL_EVENT = 0,
	PCIE_HOSTWAKE_REASON_DATA = 1,
	PCIE_HOSTWAKE_REASON_DELAYED_WAKE = 2,
	PCIE_HOSTWAKE_REASON_LAST
} hostwake_reason_t;

typedef enum dma_mode_type {
	PCIE_DMA_MODE_ASYNCHRONOUS = 0,
	PCIE_DMA_MODE_SYNCHRONOUS = 1
} dma_mode_type_t;

typedef enum pktid_type {
	PCIE_PKTID_TYPE_CTRL = 0,
	PCIE_PKTID_TYPE_RX = 1,
	PCIE_PKTID_TYPE_TX = 2
} pktid_type_t;

typedef struct _pcie_bus_counters {
	uint32  d3_info_enter_count; /* Number of D3-INFORM received */
	uint32  d3_ack_sent_count; /* Number of D3 ACK sent */
	uint32  d3count; /* Number of D3 */
	uint32  d0count; /* Number of D0 */
	uint32  l2l3count; /* Number of L2L3 */
	uint32  l0count; /* Number of L0 */
	uint32  d3_ack_srcount; /* srcount during last D3-ACK */
	uint32  d3_srcount; /* srcount during last D3 */
	uint32  l2l3_srcount;  /* srcount during last L2L3 */
	uint32  d0_in_perst; /* Number of D0 during PERST assert */
	uint32  d0_miss_counter; /* Counts D0 miss - for debug purpose */
	uint32  d3_info_start_time; /* Last D3-INFORM received time */
	uint32  d3_info_duration; /* Last D3-INFORM to D3-ACK duration */
	uint32  h2d_doorbell_ignore; /* Number of H2D DB ignored */
	uint32  missed_l0_count; /* Number of L0 missed count */
	uint32  fake_l0count; /* Number of Fake L0 */
	hostwake_reason_t hostwake_reason; /* Last hostwake assert reason */
	uint32 hostwake_assert_last; /* Last time (in ms) hostwake asserted */
	uint32 hostwake_assert_count; /* Number of times hostwake asserted */
	bool hostwake_asserted; /* flag to indicate if hostwake asserted */
	uint32 timesync_assert_last; /* Last time (in ms) hostwake asserted */
	uint32 timesync_assert_count; /* Number of times hostwake asserted */
	uint32 timesync_deassert_last; /* Last time (in ms) hostwake asserted */
	uint32 timesync_deassert_count; /* Number of times hostwake asserted */
	uint32	missed_d3_count; /* Number of D3 missed count */
	uint32 hostready_cnt; /* Number of hostready interrupts */
	uint32 last_hostready; /* Timestamp of last hostready interrupt */
	uint32 iocstatus_no_d0; /* D0 handling when ioctstatus shows no D0 */
	uint32 hostwake_deassert_last; /* Last time (in ms) hostwake deasserted */
} pcie_bus_counters_t;

#define PCIEDEV_MAX_DS_CHECK_LOG	5
#define PCIEDEV_REGDUMP_LIST_MAX	10
typedef struct _pcie_ds_check_logs {
	uint32	timestamp;
	uint32	ds_state;
	uint32	ltr_state;
	bool	ltr_config;
	uint32	sr_count;
	uint32	regdumps[PCIEDEV_REGDUMP_LIST_MAX];
} pcie_ds_check_logs_t;

#define PCIEDEV_DS_CHECK_LOG(pciedev, regs) \
do { \
	if ((pciedev)->hc_params && \
		(pciedev)->hc_params->ds_check_reg_log) { \
		(pciedev)->hc_params->ds_check_reg_log \
			[pciedev->hc_params->ds_check_reg_log_count % \
			PCIEDEV_MAX_DS_CHECK_LOG].timestamp = OSL_SYSUPTIME(); \
		(pciedev)->hc_params->ds_check_reg_log \
			[pciedev->hc_params->ds_check_reg_log_count % \
			PCIEDEV_MAX_DS_CHECK_LOG].ds_state = (pciedev)->ds_state; \
		(pciedev)->hc_params->ds_check_reg_log \
			[pciedev->hc_params->ds_check_reg_log_count % \
			PCIEDEV_MAX_DS_CHECK_LOG].ltr_state = (pciedev)->cur_ltr_state; \
		(pciedev)->hc_params->ds_check_reg_log \
			[pciedev->hc_params->ds_check_reg_log_count % \
			PCIEDEV_MAX_DS_CHECK_LOG].ltr_config = \
				(pciedev)->ltr_sleep_after_d0; \
		(pciedev)->hc_params->ds_check_reg_log \
			[pciedev->hc_params->ds_check_reg_log_count % \
			PCIEDEV_MAX_DS_CHECK_LOG].sr_count = (pciedev)->sr_count; \
		bcopy((regs), (pciedev)->hc_params->ds_check_reg_log \
			[pciedev->hc_params->ds_check_reg_log_count % \
			PCIEDEV_MAX_DS_CHECK_LOG].regdumps, \
				PCIEDEV_REGDUMP_LIST_MAX * sizeof(uint32)); \
		pciedev->hc_params->ds_check_reg_log_count ++; \
	} \
} while (0);

typedef struct _pcie_hc_params {
	bool	hc_induce_error;
	health_check_info_t	*hc;	/* Healthcheck Descriptor */
	uint32	last_deepsleep_time_sum_reset; /* Last reset of ds sum */
	uint32	ds_notification_count; /* Number of DS notification from WL */
	pcie_ds_check_logs_t *ds_check_reg_log; /* DS regdumps */
	uint32	 ds_check_reg_log_count; /* # of DS regdumps log */
	uint32	 ds_ht_count; /* # of DS prevented due to HT */
	dngl_timer_t	*ds_hc_trigger_timer; /* DS HC trigger timer */
	bool		ds_hc_trigger_timer_active;
	uint32	ds_hc_trigger_timer_count; /* How many times timer fired */
	uint32		ds_hc_enable_val; /* NVRAM settings to disable DS HC */
	bool	no_deepsleep_rate_limit; /* Rate limit number of DS notifications */
	int32   last_ds_interval; /* how long in last DS */
	bool    disable_ds_hc; /* Disable/Enable DS Healthcheck */
	int32   last_ds_enable_time; /* Timestamp when last DS Enabled */
	uint32  no_deepsleep_time_sum; /* For how long ds intervals chip not in deepsleep */
	uint32  total_ds_sr_count; /* Total DS Enabled sr_counts before healthcheck fires */
	uint32  total_ds_interval; /* Total DS Enabled intervals before healthcheck fires */
	uint32  last_ds_enable_sr_count; /* sr count at last DS enable */
} pcie_hc_params_t;

typedef struct pcie_dmaxfer_loopback {
	struct pktq	lpbk_dma_txq;	/**< Loopback Transmit packet (multi prio) queue */
	struct pktq	lpbk_dma_txq_pend; /**< Loopback Transmit packet (multi prio) queue */
	uint32		pend_dmaxfer_len;
	uint32		pend_dmaxfer_srcaddr_hi;
	uint32		pend_dmaxfer_srcaddr_lo;
	uint32		pend_dmaxfer_destaddr_hi;
	uint32		pend_dmaxfer_destaddr_lo;
	uint32		lpbk_dma_src_delay;
	uint32		lpbk_dma_dest_delay;
	uint32		lpbk_dmaxfer_fetch_pend;
	uint32		lpbk_dmaxfer_push_pend;
	uint32		lpbk_dma_pkt_fetch_len;
	uint32		lpbk_dma_req_id;
	dngl_timer_t	*lpbk_src_dmaxfer_timer;	/**< timer to add delay in src DMA */
	dngl_timer_t	*lpbk_dest_dmaxfer_timer;	/**< timer to add delay in dest DMA */
	uint32		txlpbk_pkts;
	int32		no_delay;
	uint32		pull_pkts;
	uint32		push_pkts;
	uint32          d11_lpbk;                       /* D11 DMA loopback flag */
	int32           d11_lpbk_err;                   /* D11 DMA loopback error code */
} pcie_dmaxfer_loopback_t;

typedef struct pcie_ltr {
	uint32 ltr0_regval;	/* cache of current ltr0 reg value */
	uint32 ltr1_regval;	/* cache of current ltr1 reg value */
	uint32 ltr2_regval;	/* cache of current ltr2 reg value */
	uint32 active_lat;	/* last set active latency value */
	uint32 idle_lat;	/* last set idle latency value */
	uint32 sleep_lat;	/* last set sleep latency value */
} pcie_ltr_t;

struct _autoreg {
	pcieregs_t    *regs;
};

/* Address Translation Registers */
#define SBTOPCIE_ATR_MAX	4
typedef struct pcie_sb2pcie_atr {
	uint32 base;
	uint32 mask;
} pcie_sb2pcie_atr_t;

/**
 * Externally opaque dongle bus structure, this structur is built up in such
 * a way that variables are grouped by usage. So RX, D2H DMA, All, H2D DMA, TX and Init.
 * RX is receive wireless, transferred to host. D2H DMA does both, rx completes and
 * txstatus. Large arrays are reserved at the end. They are very costly to have in
 * the middle, because access to other variables after the array will cost (much)
 * more cycles. The purpose of grouping is to lower the d-cache misses.
 */
struct dngl_bus {
	/* RX */
	rxcpl_info_t	*rxcpl_list_h;		/**< head */
	rxcpl_info_t	*rxcpl_list_t;		/**< tail */
	uint32		rxcpl_pend_cnt;		/**< ampdu buffer reorder info */
	dngl_timer_t    *glom_timer;		/**< Timer to glom rx status and send up */
	bool		glom_timer_active;	/**< timer active or not */

	/* D2H DMA */
	/** dtoh dma queue info */
	dma_queue_t	*dtoh_dma_q[MAX_DMA_CHAN];
	hnddma_t        *d2h_di[MAX_DMA_CHAN];
	struct pktq	d2h_req_q;		/**< rx completion (multi priority) queue */

	uint32  host_physaddrhi; /* 32b physical high address for host addresses */

	uint16  txpost_item_type; /* Last created flowring's item_type */
	uint16  txpost_item_size; /* Size of a txpost work item */

	uint16  rxpost_data_buf_len; /* Fixed length of buffers posted by host */
	uint16  rxcpln_dataoffset; /* Rx SplitMode 4 - fixed dataoffset */

	/**
	 * Each message ring below consists of local buffers, and has a 1:1 relationship to a
	 * message ring in host memory.
	 */
	msgbuf_ring_t	*dtoh_txcpl;
	msgbuf_ring_t	*dtoh_rxcpl;
	msgbuf_ring_t	*dtoh_ctrlcpl;

#if defined(PCIE_M2M_D2H_SYNC)
	/** Per D2H ring, modulo-253 sequence number state. */
	uint32		rxbuf_cmpl_epoch;
	uint32		txbuf_cmpl_epoch;
	uint32		ctrl_compl_epoch;
	uint32		info_buf_compl_epoch;
#endif /* PCIE_M2M_D2H_SYNC */

#if defined(PCIE_D2H_DOORBELL_RINGER)
	/** doorbell ringing using sbtopcie translation 0 host memory write */
	d2h_doorbell_ringer_t d2h_doorbell_ringer[BCMPCIE_D2H_COMMON_MSGRINGS];
	bool		d2h_doorbell_ringer_inited;
#endif /* PCIE_D2H_DOORBELL_RINGER */

	uint32		d11rxoffset;		/**< rxoffset used for d11 dma */

	/* ALL, Datapath, both RX and TX */
	uint32		intmask;		/**< Current PCIEDEV interrupt mask */
	uint32		defintmask;		/**< Default PCIEDEV intstatus mask */
	uint32		intstatus;		/**< intstatus bits to process in dpc */
	bool		common_rings_attached;
	uint32		mb_intmask;		/**< mail box/db1 intmask */
	uint32		h2dintstatus[MAX_DMA_CHAN];
	uint32		d2hintstatus[MAX_DMA_CHAN];
	osl_t		*osh;			/**< Driver osl handle */
	struct dngl	*dngl;			/**< dongle handle */
	sbpcieregs_t	*regs;			/**< PCIE registers */
	/** Pointers to per chan DTOH/HTOD TX/RXavail */
	uint* avail[MAX_DMA_CHAN][MAX_MEM2MEM_DMA_CHANNELS][MAX_MEM2MEM_DMA_CHANNELS];

	/** Bus layer support for DMAing of indices to/from host */
#ifdef PCIE_DMA_INDEX
	/** Flag to force h2d fetch after d2h dma */
	bool            dmaindex_h2d_w_d2h;
	/** Flag to check for rx/tx desc. availability for DMAing indices */
	uint8           dmaindex_d2h_pending;

	/** H2D DMA Index support (H2D WR array and D2H RD array) */
	dmaindex_t      dmaindex_h2d_wr;
	dmaindex_t      dmaindex_d2h_rd;
	struct fetch_rqst dmaindex_h2d_fetch_rqst;
	uint16          dmaindex_h2d_fetches_queued;
	bool            dmaindex_h2d_fetch_needed;
	uint32          dmaindex_log_h2d_cnt;
	dmaindex_log_t  *dmaindex_log_h2d; /* PCIE_DMAINDEX_DEBUG */

	/** D2H DMA Index support (D2H WR array and H2D RD array) */
	dmaindex_t      dmaindex_h2d_rd;
	dmaindex_t      dmaindex_d2h_wr;
	struct fetch_rqst dmaindex_d2h_fetch_rqst;
	uint16          dmaindex_d2h_fetches_queued;
	bool            dmaindex_d2h_fetch_needed;
	uint32          dmaindex_log_d2h_cnt;
	dmaindex_log_t  *dmaindex_log_d2h; /* PCIE_DMAINDEX_DEBUG */

#if defined(SBTOPCIE_INDICES)
	msgbuf_ring_t	*txcpl_last_queued_ring; /* Flush H2D RD index, on change */
#endif /* SBTOPCIE_INDICES */
#endif /* PCIE_DMA_INDEX */

	bool		ioctl_lock; /**< block successive ioctls when a prior one is pending */
	bool		in_d3_suspend; /* (depricated with inband dw) */
	bool		bm_enabled; /* bus master enabled */
	bool		d3_ack_pending; /* (depricated with inband dw) */
	dngl_timer_t	*delayed_msgbuf_intr_timer; /**< 0 delay timer used to resched
						     * the msgq processing
						     */
	bool		delayed_msgbuf_intr_timer_on;
#ifdef PCIE_DELAYED_HOSTWAKE
	dngl_timer_t	*delayed_hostwake_timer;
	bool		delayed_hostwake_timer_active;
#endif /* PCIE_DELAYED_HOSTWAKE */
	bool		clear_db_intsts_before_db;
	uint32		clr_db_intsts_db;

	/* H2D DMA */
	/** htod dma queue info */
	dma_queue_t	*htod_dma_q[MAX_DMA_CHAN];
	hnddma_t        *h2d_di[MAX_DMA_CHAN];
	pciedev_fetch_cmplt_q_t *fcq[MAX_DMA_CHAN];
	msgbuf_ring_t	*htod_rx;
	msgbuf_ring_t	*htod_ctrl;
	bool		force_ht_on;
	bool		h2d_phase_valid;
	uint32		copycount;		/**< used for fifo split mode */
	uint32		tcmsegsz;		/**< used in descriptr split mode */

	/* TX */
	uint32		dropped_txpkts;
	uint32		cfp_exptime;    /**< CFP assigned pkt exptime: tsf_timerlow */
	cfp_flows_t	*cfp_flows;      /**< Accumulated CFP classified packet flows */
	void		*pkthead;
	void		*pkttail;
	uint8		pkt_chain_cnt;
	bool		lpback_test_mode;

	cir_buf_pool_t	*flowrings_cir_buf_pool; /**< shared cir_buf_pool */
	msgbuf_ring_t	*flowrings_table;	/** preallocated msgbuf_rings */

	dngl_timer_t	*flow_schedule_timer;
	bool		flow_sch_timer_on;
	uint16		fetch_pending;          /**< number of packets to be fetched */
	dll_pool_t	*flowrings_dll_pool;		/**< pool of flow rings */
	prioring_pool_t	*prioring;		/**< pool of prio ring */
	dll_t		active_prioring_list;	/**< Active priority ring list */
	dngl_timer_t	*flow_ageing_timer;
	bool		flow_ageing_timer_on;
	uint8		flow_supp_enab; /**< the no.of flowrings capable of handling suppression */
	uint16		flow_age_timeout;	/**< flow age timeout */
	uint8		schedule_prio;		/**< use tid or AC based priority scheduler */
	msgbuf_ring_t **h2d_submitring_ptr;	/**< array of pointers to h2d submit rings. */

	/* IOCTL */
	void*		ioctl_pktptr;
	host_dma_buf_pool_t *event_pool;
	host_dma_buf_pool_t *ioctl_resp_pool;
	struct fetch_rqst *ioctptr_fetch_rqst;
	pktpool_t	*ioctl_req_pool;	/**< ioctl req buf pool */
	uchar		*ioct_resp_buf;		/**< buffer to store ioctl response from dongle */
	uint32		ioct_resp_buf_len;	/**< buffer to store ioctl response from dongle */
	pciedev_ctrl_resp_q_t *ctrl_resp_q;	/**< Queue control completions */
	void		*ioctl_cmplt_p;
	mbool		active_pending;

	/* Info rings support */
	msgbuf_ring_t	*info_submit_ring;
	msgbuf_ring_t	*info_cpl_ring;
	host_dma_buf_pool_t *info_buf_pool;
	uint32		info_buf_dropped_no_hostbuf;
	uint32		info_buf_dropped_no_hostringspace;
	uint16		info_seqnum;

	bool		event_delivery_pend;
	bool		deepsleep_disable_state;
	uint32		pend_user_tx_pkts;
	uint32		dropped_chained_rxpkts;
	uint16		msg_pending;

	/* implicit DMA */
	idma_info_t	*idma_info;

	bool  dma_ch1_enable; /* use dma channel 1 */
	bool  dma_ch2_enable; /* use dma channel 2 */

	 /* MSI */
	uint32		msicap;				/** msi cap config */
	uint32		msivectorassign;	/** enabling the specified msi vector offsets */
	pciedev_msi_info_t	d2h_msi_info;

	/* Not much used, exception/testing/sleep */
	si_t		*sih;			/**< SiliconBackplane handle */
	dngl_timer_t	*ltr_test_timer; /**< 0 delay timer used to send txstatus and rxcomplete */
	uint8		usr_ltr_test_state;
	bool		usr_ltr_test_pend;
	uint32		lpback_test_drops;
	uint32		*h2d_mb_data_ptr; /* pcie_ipc::h2d_mb_daddr32 */
	uint32		*d2h_mb_data_ptr; /* pcie_ipc::d2h_mb_daddr32 */
	uint32		d2h_mb_data;
	/** deepsleep protocol */
	int ds_state;
	dngl_timer_t	*ds_check_timer;
	bool		ds_check_timer_on;
	bool		ds_check_timer_del;
	uint32		ds_check_timer_max;
	uint32		ds_check_interval;
	pciedev_deepsleep_log_t *ds_log;
	uint32		ds_log_count;
	pcie_bus_metrics_t *metrics;		/**< tallies used to measure power state */
	metric_ref_t	*metric_ref;		/**< used to record start time for metric */
	uint32		device_link_state;
	uint32		mailboxintmask;
	uint32		in_d3_pktcount;
	dngl_timer_t	*bm_enab_timer;
	uint32		bm_enab_check_interval;
	uint8		cur_ltr_state;
	uint8		host_wake_gpio;
	bool		pciecoreset;
	bool		no_device_inited_d3_exit;
	bool		real_d3;
	bool		bm_enab_timer_on;
	bool		health_check_timer_on;
	uint32		health_check_period;	/**< ms */
	uint32		last_health_check_time;	/**< ms */
	uint32		axi_check_period;	/**< ms */
	uint32		last_axi_check_time;	/**< ms */
	uint32		process_payload_in_d3;
	/** Keep track of overall pkt fetches pending */
	uint8		uarttrans_enab;
	uint8		*uart_event_inds_mask;
	uint16		event_seqnum;
	bcmpcie_mb_intr_type_t  db1_for_mb; /**< whether to use db1 or mb for mail box interrupt */
	/** LTR Fix, send LTR_SLEEP only after L1ss+ASPM and LTR is enabled */
	bool		ltr_sleep_after_d0;	/**< if TRUE, ltr_sleep needs to be sent */
	uint8		host_pcie_ipc_rev;
	bool		dbg_trap_enable;
	dngl_timer_t    *host_sleep_ack_timer; /* polling timer for D3ACK MB Data */
	bool    host_sleep_ack_timer_on;
	uint32  host_sleep_ack_timer_interval;

	uint32		sr_count;		/**< Counter for deepsleep */
	bool		write_trap_to_host_mem;
	bool		trap_on_badphase;

	dll_t	*last_fetch_prio; /* last fetch prio in the active prioring list */

	/* INIT */
	bool		up;			/**< device is operational */
	uint		coreid;			/**< pcie core ID */
	uint		corerev;		/**< pcie device core rev */

	/** BCMPKTPOOL related vars */
	pktpool_t	*pktpool_lfrag;		/**< TX frag */
	pktpool_t	*pktpool_rxlfrag;	/**< RX frag */
	pktpool_t	*pktpool_utxd;	/**< UTXD */
	pktpool_t	*pktpool;		/**< pktpool lbuf */
	pktpool_t	*pktpool_resv_lfrag;	/* RESV lfrag pool */
	bool		msi_in_use;
	uint32		*tunables;

	pcie_ipc_t  *pcie_ipc; /**< shared area between host & dongle */

	/** Common and Dynamic sections, for H2D and D2H, in pcie_ipc_ring_mem array */
	pcie_ipc_ring_mem_t	*h2d_cmn_pcie_ipc_ring_mem; /* H2D cmn ring section */
	pcie_ipc_ring_mem_t	*d2h_cmn_pcie_ipc_ring_mem; /* D2H cmn ring section */
	pcie_ipc_ring_mem_t	*h2d_dyn_pcie_ipc_ring_mem; /* H2D dyn ring section */
	pcie_ipc_ring_mem_t	*d2h_dyn_pcie_ipc_ring_mem; /* H2D dyn ring section */

	uint16      max_h2d_rings; /* Max H2D rings (common + flowrings) */
	uint16      max_d2h_rings; /* Max D2H rings (common + dynamic) */
	uint16      max_flowrings; /* H2D TxPost rings aka flowrings */
	uint16      max_dbg_rings; /* WIP: dynamic H2D and D2H (debug) rings */
	uint16      max_rxcpl_rings; /* Max D2H complation rings */
	uint16      max_slave_devs; /* Max interfaces = BC/MC TxPost flowrings */

	/* Large ARRAYS */
	flow_fetch_rqst_t *flowring_fetch_rqsts;
	msgbuf_ring_t	*cmn_msgbuf_ring;
	uint32 unsupport_req_err;
	uint16	coe_dump_buf_size; /* buffer size needed to store the pcie reg info for traps */
	/* DMA engines quiesced for the trap processing */
	bool	h2d_di_dma_suspend_for_trap;
	bool	d2h_dma_suspend_for_trap;

	uint32  num_tx_status_in_d3; /* Number of Tx Status generated during D3cold */
	bool induce_trap4; /* Enable TRAP4 induce during D3cold */
	uint32  clr_intstats_db_fail; /* counter for db0 intstat clearing */
	bool    ipc_data_supported;     /* Can we collect detailed stats? */
	ipc_data_t *ipc_data;    /* used to record IPC data for stats */
	dw_counters_t dw_counters; /* Device_Wake related counters/stats */
	pcie_bus_counters_t *bus_counters; /* PCIE bus level counters */
	bool    disallow_write_trapdata; /* Flag to indicate writing trap data to host */
	bool	core_reset_failed_in_trap; /* TRUE if core reset failed inside trap handler */
	/* Optional: After DMA'ing indices to host memory */
	/* DMA the ones updated by host from host memory */
	uint16  extra_lfrags; /* Need for extra lfrags from wl module */
	fetch_req_list_t	*fetch_req_pend_list; /* pending fetch list to be processed */
	ifidx_tx_account_t *ifidx_account;
	uint16 txflows_max_rate; /* Max rate across all flows */
	uint32	d3_drop_cnt_rx;
	uint32	d3_drop_cnt_evnt;
	uint32 in_d3_pktcount_rx;
	uint32 in_d3_pktcount_evnt;
	uint8 default_dma_ch;  /**< both for h2d/d2h dma channel for test mode */
	pktpool_t *ts_req_pool;	/* Host Timestamp req buf pool */
	void*		ts_pktptr; /* Host Timestamp req pointer */
	struct fetch_rqst *tsptr_fetch_rqst; /* Fetch request for Timestamp payload */
	uint16	ts_seqnum; /* Last Timestamp Req sequence number */
	host_dma_buf_pool_t	*ts_pool;
	bool tsinfo_delivery_pend;
	bool timesync; /* Timesync feature for IPC rev7 */
	uint32 d3_drop_cnt_fw_ts; /* # of FW Timestamp Info packets dropped during D3 */
	dngl_timer_t	*fwtsinfo_timer;
	bool	fwtsinfo_timer_active;
	uint32	fwtsinfo_interval;
	pcie_dmaxfer_loopback_t  *dmaxfer_loopback;
#ifdef TIMESYNC_GPIO
	uint8 time_sync_gpio;
#endif /* TIMESYNC_GPIO */
	bool timesync_asserted; /* flag to indicate whether timesync has been asserted */
	mbool ctrl_lock; /* Flag to indicate a pending ctrl item e.g. ioctl, timesync */
	bool	hostready; /* IPC Rev7 hostready feature enabled/disabled */
	inuse_lclbuf_pool_t	*htod_rx_poolreclaim_inuse_pool;
	uint16 frwd_resrv_bufcnt;
#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
	uint32  monitor_mode;
#endif /* WL_MONITOR && WL_MONITOR_DISABLED */
	bool d2h_intr_ignore;   /* D2H MB interrupt ignore */
#ifdef DBG_IPC_LOGS
	ipc_log_t *ipc_logs;
#endif // endif

	/** Number of Single AMSDU suppressed packet fetch */
	uint32	single_amsdu_fetch;
	/** Number of Single AMSDU suppressed packet fetch rejected */
	uint32	single_amsdu_fetch_reject;
	/** max m2m dma channel number */
	bool	max_m2mdma_chan;
	dma_ch_info_t	*dma_ch_info;
	/** flag to indicate trap data written to host */
	bool	write_to_host_trap_data;
	pciedev_err_cntrs_t *err_cntr;
	pciedev_dbg_bus_cntrs_t *dbg_bus;
	pcie_ltr_t ltr_info;	/* ltr info; see structure def for details */

	pcie_m2m_req_t *m2m_req;
	bool ds_inband_dw_supported; /* Whether inband DW supported */
	bool    ds_oob_dw_supported; /* Whether OOB DW supported */
	uint32  num_ds_inband_dw_assert; /* Number of inband dw assert */
	uint32  num_ds_inband_dw_deassert; /* Number of inband dw deassert */
	bool linkdown_trap_dis;
	/* # of dropped frames due to excessive sup retries */
	uint32		dropped_counter_sup_retries;
	/* # of dropped frames due to excessive timer sup retries */
	uint32		dropped_timer_sup_retries;
	uint32		sup_retries; /* total of all suppressed retries */
#ifdef PCIEDEV_FAST_DELETE_RING
	bool fastdeletering;
#endif /* PCIEDEV_FAST_DELETE_RING */
#ifdef BCM_CSIMON
	bool csimon;
#endif /* BCM_CSIMON */
	uint8	device_wake_gpio; /* Store the device_wake GPIO */
	bool	sig_det_filt_dis; /* Digital Signal Detector Filter disable */
	bool	pipeiddq1_enab; /* enable assertion of pcie_pipe_iddq during L2 state */
	bool	fastlpo_pcie_en;
	bool	fastlpo_pmu_en;
	bool	info_cpl_delivery_pend;
	/* HMAP regs required outside HMAPTEST to get status of HMAP regs when intr occured */
	pcie_hmapviolation_t hmap_regs;
	/* inprogress  is required even in non HMAPTEST builds as its used in intr processing */
	bool hmaptest_inprogress;
#ifdef HMAPTEST
	void *hmaptest_m2mread_p;
	void *hmaptest_m2mwrite_p;
#endif /* HMAPTEST */
	uint32	ds_clk_ctl_st; /* ARM clk_ctl_st */
	pcie_hc_params_t *hc_params; /* healthcheck related flags/counters */
	uint32	txstatus_in_host_sleep; /* Number of txstatus post attempt during host sleep */
	uint32	rxcomplete_in_host_sleep; /* Number of rxcpomplete attempt during host sleep */
	uint32	event_pool_max; /* Max event pool buffer size */
	void *wd; /* wakeup-data which caused host wakeup */
	uint16 wd_len; /* len of host wakeup data */
	uint32 wd_gpio_toggle_time; /* gpio toggle time of wake up data */
	ctrl_submit_item_t ctrl_submit;
	bool  dar_enable;	/* use dar register for h2d doorbell */
	bool	idma_dmaxfer_active;  /* implicit DMA is in active */
	bool    ds_dev_wake_received; /* DW DEV WAKE received in inband DW mode */
	bool	trap_data_readback; /* Data will be readback and compared when set */
	struct _autoreg *autoregs;
#if defined(BCM_DHDHDR) && !defined(BCM_DHDHDR_D3_DISABLE)
	lfrag_buf_pool_t *d3_lfbufpool;
#endif // endif
	pcie_sb2pcie_atr_t	sb2pcie_atr[SBTOPCIE_ATR_MAX];
#if defined(WLSQS)
	bool sqs_stride_in_progress;
#endif /* SQS */
#if defined(HNDBME)
	int bme_key[2]; /* D2H dual buffer streaming using bme */
#endif // endif
#if defined(BCMPCIE_IPC_HPA)
	void *hpa_hndl;
#endif // endif
	bool taf_scheduler;
	uint16	bulk_txs_pending;
};

#ifdef BCMFRAGPOOL
#if defined(BCM_DHDHDR) && !defined(BCM_DHDHDR_D3_DISABLE)
#define LFRAG_POOL_AVAIL(pciedev) \
	MIN(pktpool_avail(pciedev->pktpool_lfrag), \
		lfbufpool_avail(pciedev->d3_lfbufpool))
#else
#define LFRAG_POOL_AVAIL(pciedev) \
	pktpool_avail(pciedev->pktpool_lfrag)
#endif // endif
#if defined(BCM_DHDHDR) && !defined(BCM_DHDHDR_D3_DISABLE)
#define LFRAG_POOL_GET(pciedev) \
	pktpool_lfrag_get(pciedev->pktpool_lfrag, pciedev->d3_lfbufpool)
#else
#define LFRAG_POOL_GET(pciedev) \
	pktpool_get(pciedev->pktpool_lfrag)
#endif // endif
#endif /* BCMFRAGPOOL */

static INLINE bool pciedev_ds_in_host_sleep(struct dngl_bus *pciedev)
{
	return (
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
		(pciedev->ds_inband_dw_supported &&
		(pciedev->ds_state == DS_STATE_HOST_SLEEP_PEND_TOP ||
		pciedev->ds_state == DS_STATE_HOST_SLEEP_PEND_BOT ||
		pciedev->ds_state == DS_STATE_HOST_SLEEP)) ||
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
		(pciedev->in_d3_suspend));
}

static INLINE bool pciedev_ds_in_host_sleep_no_bus_access(struct dngl_bus *pciedev)
{
	return (
#ifdef PCIE_DEEP_SLEEP
#ifdef PCIEDEV_INBAND_DW
	pciedev->ds_inband_dw_supported ?
		pciedev->ds_state == DS_STATE_HOST_SLEEP :
#endif /* PCIEDEV_INBAND_DW */
#endif /* PCIE_DEEP_SLEEP */
		(pciedev->in_d3_suspend &&
		!pciedev->d3_ack_pending));
}

/* **** Private function declarations **** */

/* gpioX interrupt handler */
typedef void (*pciedev_gpio_handler_t)(uint32 stat, void *arg);
typedef struct pciedev_gpioh pciedev_gpioh_t;

void pciedev_bus_rebinddev(void *bus, void *dev, int ifindex);
int pciedev_bus_binddev(void *bus, void *dev, uint numslaves);
int pciedev_bus_findif(void *bus, void *dev);
int pciedev_bus_rebind_if(void *bus, void *dev, int idx, bool rebind);
void pciedev_init(struct dngl_bus *pciedev);
void pciedev_intrsoff(struct dngl_bus *pciedev);
void pciedev_intrs_deassert(struct dngl_bus *pciedev);
bool pciedev_dispatch(struct dngl_bus *pciedev);
void pciedev_intrson(struct dngl_bus *pciedev);
void pciedev_health_check(uint32 ms);
uint32 pciedev_halt_device(struct dngl_bus *pciedev);
int pciedev_pciedma_init(struct dngl_bus *p_pcie_dev);
int pciedev_check_host_wake(struct dngl_bus *pciedev, void *p);

#define PCIE_ERROR1			1
#define PCIE_ERROR2			2
#define PCIE_ERROR3			3
#define PCIE_ERROR4			4
#define PCIE_ERROR5			5
#define PCIE_ERROR6			6
#define PCIE_ERROR7			7
#define PCIE_WARNING			8

#define IOCTL_PKTBUFSZ			2048

#define LTR_MAX_ACTIVE_LATENCY_US	600	/* unit in usec */
#define LTR_MAX_SLEEP_LATENCY_MS	6	/* uint in msec */

enum {
	HTOD = 0,
	DTOH
};
enum {
	RXDESC = 0,
	TXDESC
};

/* access to config space */
enum {
	IND_ACC_RD,
	IND_ACC_WR,
	IND_ACC_AND,
	IND_ACC_OR
};

void pciedev_process_tx_payload(struct dngl_bus *pciedev, uint8 dmach);
bool  pciedev_msgbuf_intr_process(struct dngl_bus *pciedev);
bool pciedev_handle_interrupts(struct dngl_bus *pciedev);
void pciedev_detach(struct dngl_bus *pciedev);
void pciedev_bus_sendctl(struct dngl_bus *bus, void *p);
uint32 pciedev_bus_iovar(struct dngl_bus *pciedev, char *buf,
	uint32 inlen, uint32 *outlen, bool set);
int pciedev_bus_unbinddev(void *bus, void *dev);
int pciedev_bus_validatedev(void *bus, void *dev);
bool pciedev_bus_maxdevs_reached(void *bus);
int pciedev_create_d2h_messages_tx(struct dngl_bus *pciedev, void *p);
int pciedev_bus_sendctl_tx(void *dev, uint8 type, uint32 op, void *p);
int pciedev_bus_flring_ctrl(void *bus, uint32 op, void *data);
void pciedev_upd_flr_hanlde_map(struct dngl_bus *pciedev, uint8 handle, uint8 ifindex,
	bool add, uint8 *addr);
void pciedev_upd_flr_tid_state(struct dngl_bus *pciedev, uint8 tid_ac, bool open);
void pciedev_upd_flr_if_state(struct dngl_bus *pciedev, uint8 ifindex, bool open);
void pciedev_upd_flr_port_handle(struct dngl_bus *pciedev, uint8 handle, bool open, uint8 ps);
void pciedev_upd_flr_flowid_state(struct dngl_bus *pciedev, uint16 flowid, bool open);
void pciedev_process_reqst_packet(struct dngl_bus *pciedev, uint8 handle, uint8 ac_bitmap,
	uint8 count);
void pciedev_update_flr_flags(struct dngl_bus *pciedev, uint8 *addr, uint8 flags);
void pciedev_clear_flr_supr_info(struct dngl_bus *pciedev, uint8 *addr, uint8 tid);
bool pciedev_handle_h2d_dma(struct dngl_bus *pciedev, uint8 dmach);
void pciedev_flow_schedule_timerfn(dngl_timer_t *t);
void pciedev_flow_ageing_timerfn(dngl_timer_t *t);
void print_config_register_value(struct dngl_bus *pciedev, uint32 startindex, uint32 endindex);
int pciedev_h2d_start_fetching_host_buffer(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring);
void pciedev_schedule_flow_ring_read_buffer(struct dngl_bus *pciedev);
void* pciedev_lclpool_alloc_lclbuf(msgbuf_ring_t *ring);
int pciedev_fillup_rxcplid(pktpool_t *pool, void *arg, void *p, int dummy);
int pciedev_fillup_haddr(pktpool_t *pool, void *arg, void *frag, int rxcpl_needed);

int pciedev_manage_rxcplid(pktpool_t *pool, void *arg, void *p, int rxcpl_needed);
int pciedev_manage_haddr(pktpool_t *pool, void *arg, void *p, int rxcpl_needed);

#ifdef BCMPOOLRECLAIM
void pciedev_free_poolreclaim_inuse_bufpool(struct dngl_bus *pciedev,
	inuse_lclbuf_pool_t *inuse_pool);
int pciedev_init_poolreclaim_inuse_bufpool(struct dngl_bus *pciedev, msgbuf_ring_t *ring);
#endif /* #ifdef BCMPOOLRECLAIM */

void pciedev_flush_chained_pkts(struct dngl_bus *pciedev);
void pciedev_flush_glommed_rxstatus(dngl_timer_t *t);
void pciedev_queue_rxcomplete_local(struct dngl_bus *pciedev, rxcpl_info_t *rxcpl_info,
	msgbuf_ring_t *ring, bool flush);
uint8 pciedev_xmit_msgbuf_packet(struct dngl_bus *pciedev, void *p,
	uint8 msgtype, uint16  msglen, msgbuf_ring_t *ring);
bool pciedev_lbuf_callback(void *arg, void* p);
void pciedev_spktq_callback(void *arg, struct spktq *spq); /* arg is pciedev */
int pciedev_tx_pyld(struct dngl_bus *bus, void *p, ret_buf_t *ret_buf, uint16 msglen,
	uint8 msgtype);
int pciedev_tx_pyld_from_src_addr(struct dngl_bus *pciedev, void *p, ret_buf_t *data_buf,
	uint16 data_len, uint8 msgtype,
	msgbuf_ring_t *msgring, uint8 dmach, dma64addr_t src_addr);
uint16 pciedev_handle_h2d_msg_rxsubmit(struct dngl_bus *pciedev, void *p, uint16 pktlen,
	msgbuf_ring_t *ring);
uint16 pciedev_handle_h2d_msg_txsubmit(struct dngl_bus *pciedev, void *p, uint16 pktlen,
	msgbuf_ring_t *ring, uint16 fetch_indx, uint32 flags);
void pciedev_enqueue_dma_info(dma_queue_t *dmaq, uint16 msglen, uint8 msgtype, uint8 flags,
	msgbuf_ring_t *ring, uint32 addr, uint32 haddr);
dma_item_info_t *pciedev_dequeue_dma_info(dma_queue_t *dmaq);
bool pciedev_handle_d2h_dma(struct dngl_bus *pciedev, uint8 dmach);
void pciedev_handle_d2h_dma_error(struct dngl_bus *pciedev, uint8 dmach, uint32 d2hintstatus);
void pciedev_process_pending_fetches(struct dngl_bus *pciedev);
void pciedev_flush_cached_amsdu_frag(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring);
void pciedev_queue_d2h_req_send(struct dngl_bus *pciedev);
void pciedev_send_d2h_mb_data_ctrlmsg(struct dngl_bus *pciedev);
uint8 pciedev_ctrl_resp_q_avail(struct dngl_bus *pciedev);
int pciedev_process_ctrl_cmplt(struct dngl_bus *pciedev);
int pciedev_process_d2h_wlevent(struct dngl_bus *pciedev, void* p);
int pciedev_process_d2h_tsinfo(struct dngl_bus *pciedev, void* p);

void pciedev_free_reuse_seq_list(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring);

#if defined(PCIE_D2H_DOORBELL_RINGER)
void pciedev_d2h_doorbell_ringer(struct dngl_bus *pciedev, uint32 db_data, uint32 db_addr);
#endif /* PCIE_D2H_DOORBELL_RINGER */

void pciedev_generate_host_db_intr(struct dngl_bus *pciedev, uint32 data, uint32 db);
void pciedev_send_flow_ring_flush_resp(struct dngl_bus *pciedev, uint16 flowid, uint32 status,
	void *p);
int pciedev_send_flow_ring_delete_resp(struct dngl_bus *pciedev, uint16 flowid, uint32 status,
	void *p);
void pciedev_process_ioctl_done(struct dngl_bus *pciedev);
void pciedev_process_pending_flring_resp(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring);
void pciedev_host_wake_gpio_enable(struct dngl_bus *pciedev, bool state);
void pciedev_process_ioctl_pend(struct dngl_bus *pciedev);
void pciedev_process_ts_pend(struct dngl_bus *pciedev);
void pciedev_ts_logging_min(dma_item_info_t *item);

int pciedev_queue_d2h_req(struct dngl_bus *pciedev, void *p, uint8 prio);
int pciedev_dispatch_fetch_rqst(struct fetch_rqst *fr, void *arg);

#ifdef PCIE_DMAXFER_LOOPBACK
uint32 pciedev_process_do_dest_lpbk_dmaxfer_done(struct dngl_bus *pciedev, void *p);
int pciedev_process_d2h_dmaxfer_pyld(struct dngl_bus *pciedev, void *p);
#endif // endif

#ifdef HMAPTEST
void pciedev_hmaptest_m2mwrite_dmaxfer_done(struct dngl_bus *pciedev, void *p);
#endif // endif

void pciedev_rxreorder_queue_flush_cb(void *ctx, uint16 list_start_id, uint32 count);
void pciedev_send_ltr(void *dev, uint8 state);
bool pciedev_init_flowrings(struct dngl_bus *pciedev);

/* Backplane memcpy using sbtopcie 0. len specified in 4Byte units. */
void pciedev_sbcopy(uint64 haddr64, daddr32_t daddr32, int len_4B, bool h2d_dir);

#ifdef PCIE_DMA_INDEX
void pciedev_dmaindex_put(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf_ring);
void pciedev_dmaindex_get(struct dngl_bus *pciedev);
#ifdef SBTOPCIE_INDICES
extern void pciedev_sync_d2h_read_ptrs(struct dngl_bus * pciedev, msgbuf_ring_t* ring);
extern void pciedev_sbtopcie_access(struct dngl_bus * pciedev, uint64 low_addr,
	pcie_rw_index_t *data, bool read);
#endif /* SBTOPCIE_INDICES */
#endif /* PCIE_DMA_INDEX */

void pciedev_set_copycount_bytes(struct dngl_bus *pciedev, uint32 copycount, uint32 d11rxoffset);
void pciedev_set_frwd_resrv_bufcnt(struct dngl_bus *pciedev, uint16 frwd_resrv_bufcnt);
int pciedev_process_d2h_infobuf(struct dngl_bus *pciedev, void* p);

#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
void pciedev_set_monitor_mode(struct dngl_bus *pciedev, uint32 monitor_mode);
#endif /* WL_MONITOR && WL_MONITOR_DISABLED */

#ifdef PCIEDEV_DBG_LOCAL_POOL
void pciedev_check_free_p(msgbuf_ring_t *ring, lcl_buf_t *free, void *p, const char *caller);
#endif // endif

pciedev_gpioh_t *pciedev_gpio_handler_register(uint32 event, bool level, pciedev_gpio_handler_t cb,
	void *arg);
void pciedev_gpio_handler_unregister(pciedev_gpioh_t *gi);
void pciedev_send_ioctl_completion(struct dngl_bus *pciedev);
void pciedev_update_h2d_info_submitring_status(struct dngl_bus *pciedev);
bool pciedev_check_valid_phase(struct dngl_bus *pciedev, msgbuf_ring_t *ring,
	cmn_msg_hdr_t *msg_hdr, uint8 exp);
bool pciedev_check_update_valid_phase(struct dngl_bus *pciedev, msgbuf_ring_t *ring,
	cmn_msg_hdr_t *msg_hdr);
void pciedev_flowring_fetch_cb(struct fetch_rqst *fr, bool cancelled);
#ifdef BCMPCIE_IDMA
int pciedev_idma_desc_load(struct dngl_bus *pciedev);
#endif /* BCMPCIE_IDMA */

void pciedev_generate_msi_intr(struct dngl_bus *pciedev, uint8 offset);
void pciedev_extra_txlfrag_requirement(void *ctx, uint16 count);
void pciedev_update_txflow_pkts_max(struct dngl_bus *pciedev);
int pciedev_sendctl_tx(struct dngl_bus *pciedev, uint8 type, uint32 op, void *p);
void pciedev_time_sync_gpio_enable(struct dngl_bus *pciedev, bool state);
int pciedev_create_d2h_messages(struct dngl_bus *bus, void *p, msgbuf_ring_t *ring);
osl_t* pciedev_get_osh_handle(void);
/* used by pciedev_*_ds.c */
void pciedev_d2h_mbdata_send(struct dngl_bus *pciedev, uint32 data);
void pciedev_d2h_mbdata_clear(struct dngl_bus *pciedev, uint32 mb_data);
bool pciedev_can_goto_deepsleep(struct dngl_bus *pciedev);
void pciedev_disable_deepsleep(struct dngl_bus *pciedev, bool disable);
#ifdef PCIE_PWRMGMT_CHECK
int pciedev_handle_d0_enter_bm(struct dngl_bus *pciedev);
void pciedev_handle_d0_enter(struct dngl_bus *pciedev);
void pciedev_handle_l0_enter(struct dngl_bus *pciedev);
#endif /* PCIE_PWRMGMT_CHECK */
void pciedev_enable_powerstate_interrupts(struct dngl_bus *pciedev);
void pciedev_notify_devpwrstchg(struct dngl_bus *pciedev, bool hostmem_acccess_enabled);
void pciedev_crwlpciegen2_161(struct dngl_bus *pciedev, bool state);
void pciedev_WOP_disable(struct dngl_bus *pciedev, bool disable);
void pciedev_manage_TREFUP_based_on_deepsleep(struct dngl_bus *pciedev, bool deepsleep_enable);
void pciedev_dma_params_init_h2d(struct dngl_bus *pciedev, hnddma_t *di);
int pciedev_flush_h2d_dynamic_ring(struct dngl_bus *pciedev, msgbuf_ring_t *h2d_ring);
int pciedev_flush_d2h_dynamic_ring(struct dngl_bus *pciedev, msgbuf_ring_t *d2h_ring);

/* access to config space */
uint32 pciedev_indirect_access(struct dngl_bus *pciedev, uint32 addr, uint32 data,
	uint8 access_type);

#if defined(BCMPCIE_IDMA)
bool pciedev_idma_channel_enable(struct dngl_bus *pciedev, bool enable);
void pciedev_handle_deepsleep_dw_db(struct dngl_bus *pciedev);
#endif /* BCMPCIE_IDMA */
void pciedev_manage_h2d_dma_clocks(struct dngl_bus *pciedev);

#define W_REG_CFG(pciedev, b, c) pciedev_indirect_access((pciedev), (b), (c), IND_ACC_WR)
#define R_REG_CFG(pciedev, b) pciedev_indirect_access((pciedev), (b), 0, IND_ACC_RD)
#define AND_REG_CFG(pciedev, b, c) pciedev_indirect_access((pciedev), (b), (c), IND_ACC_AND)
#define OR_REG_CFG(pciedev, b, c) pciedev_indirect_access((pciedev), (b), (c), IND_ACC_OR)

#define PCIE_M2M_REQ_SUBMIT(di, req) pcie_m2m_req_submit((di), (req), FALSE)
#define PCIE_M2M_REQ_SUBMIT_IMPLICIT(di, req) pcie_m2m_req_submit((di), (req), TRUE)
#ifdef WLCFP
void pciedev_cfp_flow_delink(struct dngl_bus *pciedev, uint16 cfp_flowid);
#endif // endif

/* 128-MB PCIE Access space addressable via two 64-MB regions using
 * SB to PCIE translation 0 and 1. Each 64-MB base is:
 *
 * SBTOPCIE0_BASE  0x08000000
 * SBTOPCIE1_BASE  0x0c000000
 *
 * On chips with CCI-400, the small pcie 128 MB region base has shifted
 * CCI400_SBTOPCIE0_BASE   0x20000000
 * CCI400_SBTOPCIE1_BASE   0x24000000
 * SB to PCIE translation masks
 * SBTOPCIE0_MASK  0xfc000000
 * SBTOPCIE1_MASK  0xfc000000
 * SBTOPCIE2_MASK  0xc0000000
 *
 * After PcieGen2 rev24, 128-MB PCIE Access space addressable via
 * four 32-MB regions using SB to PCIE translation 0,1,2 and 3.
 * SBTOPCIE_32M_REGIONSZ  0x02000000
 * SBTOPCIE_32M_MASK         0xfe000000
 */

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7A__)
#define INSTR_BARRIER() \
	({ \
		__asm__ __volatile__("dsb"); \
		__asm__ __volatile__("isb"); \
	})
#else
#define INSTR_BARRIER()
#endif // endif

/* Configure sbtopcie##idx##upper when host dma address is 64 bits */
#ifdef HOST_DMA_ADDR64
#define SBTOPCIE_BASE_UPPER_CONFIG(pciedev, addr, idx) \
	({ \
		W_REG(pciedev->osh, &pciedev->regs->sbtopcie##idx##upper, \
			(uint32)(addr >> (NBITS(uint32)))); \
	})
#else
#define SBTOPCIE_BASE_UPPER_CONFIG(pciedev, addr, idx)  do { /* noop */ } while (0)
#endif // endif

/* Write the translation base address
 * Make sure translation space is not overloaded
 * Insert a instruction barrier to make sure
 * address base write actually took place
 */
#define SBTOPCIE_BASE_CONFIG(pciedev, haddr64, sch_len, idx, ret_addr, init) \
	({ \
		uint32 attrib = \
			SBTOPCIE_MEM | SBTOPCIE_PF | SBTOPCIE_WR_BURST; \
		pcie_sb2pcie_atr_t *atr = &pciedev->sb2pcie_atr[(idx)]; \
		if (init) \
			ASSERT((R_REG(pciedev->osh, \
				&pciedev->regs->sbtopcie##idx) & atr->mask) == 0); \
		if ((haddr64 & atr->mask) != ((haddr64 + sch_len) & atr->mask)) { \
			PCI_ERROR(("Fail to configure SBTOPCIE%d:" \
				"host resident buffer low addr %x len %x\n", \
				idx, ((uint32)haddr64), sch_len)); \
			ASSERT(0); \
			ret_addr = 0; \
		} else { \
			ret_addr = (haddr64 & atr->mask); \
			W_REG(pciedev->osh, &pciedev->regs->sbtopcie##idx, \
				ret_addr | attrib); \
			SBTOPCIE_BASE_UPPER_CONFIG(pciedev, haddr64, idx); \
			INSTR_BARRIER(); \
		} \
	})

/* Remap the address to one of the sbtopcie base addresses */
#define SBTOPCIE_ADDR_REMAP(pciedev, low_addr, idx, remap_addr) \
	({ \
		pcie_sb2pcie_atr_t *atr = &pciedev->sb2pcie_atr[(idx)]; \
		ASSERT(atr->base != 0); \
		(remap_addr) = (uint32)(atr->base + (low_addr & ~atr->mask)); \
	})

#if defined(BCM_BUZZZ) && defined(BCM_BUZZZ_STREAMING_BUILD)
/* D2H mem2mem transfer of buzzz logs to host */
extern int pciedev_buzzz_d2h_copy(uint32 buf_idx,
	uint32 daddr32, uint32 haddr32, uint16 len, uint8 index, uint32 offset);
extern int pciedev_buzzz_d2h_done(uint32 buf_idx);
#endif /* BCM_BUZZZ && BCM_BUZZZ_STREAMING_BUILD */

extern void pciedev_sbtopcie_access_start(struct dngl_bus * pciedev,
	sbtopcie_info_t *sbtopcie_info);
extern void pciedev_sbtopcie_access_stop(struct dngl_bus * pciedev, uint32 base_lo, uint32 base_hi);
#define PCIEDEV_TID_REMAP(pciedev, msgbuf) \
	({\
		uint8 __tid_ac = msgbuf->flow_info.tid_ac; \
		if (pciedev->schedule_prio == PCIEDEV_SCHEDULER_TID_PRIO) { \
			__tid_ac = PCIEDEV_TID2AC(msgbuf->flow_info.tid_ac); \
		} else if (pciedev->schedule_prio == PCIEDEV_SCHEDULER_LLR_PRIO) { \
			__tid_ac = PCIEDEV_TID2AC_LLR(msgbuf->flow_info.tid_ac); \
		} \
		__tid_ac; \
	})
#endif /* _pciedev_priv_h */
