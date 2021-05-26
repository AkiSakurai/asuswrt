/**
 * +--------------------------------------------------------------------------+
 * wlc_csimon.c
 *
 * Implementation of Channel State Information (CSI) Producer subsystem (Dongle)
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * +--------------------------------------------------------------------------+
 */

/**
 * For Theory of operation of overall CSIMON please refer to the CSIMON handling
 * section in dhd_msgbuf.c
 *
 */

#include <osl.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <bcmpcie.h>
#if defined(DONGLEBUILD)
#include <bcmhme.h>
#include <rte_isr.h>
#else /* ! DONGLEBUILD */
#include <linux/netlink.h>
#endif /* ! DONGLEBUILD */
#include <bcm_ring.h>
#include <pcicfg.h>
#include <wl_dbg.h>
#include <wl_export.h>
#include <wlc.h>
#include <wlc_types.h>
#include <wlc_scb.h>
#include <wlc_stf.h>
#include <wlc_dump.h>
#include <wlc_csimon.h>

/* Compile time pick either hndm2m or sb2pcie based transfer of CSI structure */
#define CSIMON_M2MCPY_BUILD        /* Asynchronous transfer with callback */
#ifndef CSIMON_M2MCPY_BUILD
#define CSIMON_SBCOPY_BUILD        /* Synchronous transfer, no callback */
#endif  /* !CSIMON_M2MCPY_BUILD */

#if !defined(CSIMON_M2MCPY_BUILD) && !defined(CSIMON_SBCOPY_BUILD)
#error "Must define a xfer mechanism"
#endif // endif

//#define CSIMON_FILE_BUILD
//#define SINGLE_M2M_XFER		/* single M2MDMA transfer from sysmem to host mem */
#define CSIMON_WD_BASED_USR_XFER

#if  ((CSIMON_REPORT_SIZE + CSIMON_HEADER_SIZE) > CSIMON_RING_ITEM_SIZE)
#error "CSIMON header + report cannot be greater than CSIMON item size"
#endif // endif

/* wlc access macros */
#define WLC(x) ((x)->wlc)
#define WLCPUB(x) ((x)->wlc->pub)
#define WLCUNIT(x) ((x)->wlc->pub->unit)

/* CSIMON Info xfer macro */
#define CSIxfer(x) (x)->csimon_info->xfer

/* CSIMON local ring manipulation */
#define CSIMON_RING_IDX2ELEM(ring_base, idx) \
	(((wlc_csimon_rec_t *)(ring_base)) + (idx))

#if defined(DONGLEBUILD)
/**
 *  pciedev_sbcopy used to transfer preamble and indices, and,
 *  in case of CSIMON_SBCOPY_BUILD, for csi data transfer too.
 */
void pciedev_sbcopy(uint64 haddr64, daddr32_t daddr32, int len_4B,
	bool h2d_dir);
#endif /* DONGLEBUILD */

/**
 * +--------------------------------------------------------------------------+
 *  Section: Definitions and Declarations
 * +--------------------------------------------------------------------------+
 */

#define C_CSI_IDX_NUM  2 // Number of SVMP memory-resident CSI reports

/* M_CSI_STATUS */
typedef enum
{
	C_CSI_IDX_NBIT          = 0,    // CSI index
	C_CSI_IDX_LB            = 4,
	C_CSI_ARM_NBIT          = 5     // ARM CSI capture for next RX frame
} eCsiStatusRegBitDefinitions;

#define C_CSI_IDX_BSZ          NBITS(C_CSI_IDX)
#define C_CSI_IDX_MASK         0x1F

/* C_CSI_VSEQN_POS */
typedef enum
{
	C_CSI_SEQN_NBIT         = 0,    // CSI index
	C_CSI_SEQN_LB           = 14,
	C_CSI_VLD_NBIT          = 15    // Record is valid
} eCsiValidSeqNBitDefinitions;
#define C_CSI_SEQN_BSZ         NBITS(C_CSI_SEQN)

/** csi info block: M_CSI_BLKS */
typedef enum
{
	C_CSI_ADDRL_POS         = 0,    // MAC ADDR LOW
	C_CSI_ADDRM_POS         = 1,    // MAC ADDR MED
	C_CSI_ADDRH_POS         = 2,    // MAC ADDR HI
	C_CSI_VSEQN_POS         = 3,    // Frame seq number
	C_CSI_RXTSFL_POS        = 4,    // CSI RX-TSF-time: lower 16 bits
	C_CSI_RXTSFML_POS       = 5,    // CSI RX-TSF-time: higher 16 bits
	C_CSI_RSSI0_POS         = 6,    // Ant0, Ant1 RSSI
	C_CSI_RSSI1_POS         = 7,    // Ant2, Ant3 RSSI
	C_CSI_BLK_WSZ
} eCsiInfoWordDefinitions;

#define M_CSI_MACADDRL(wlc)    (M_CSI_BLKS(wlc) + (C_CSI_ADDRL_POS * 2))
#define M_CSI_MACADDRM(wlc)    (M_CSI_BLKS(wlc) + (C_CSI_ADDRM_POS * 2))
#define M_CSI_MACADDRH(wlc)    (M_CSI_BLKS(wlc) + (C_CSI_ADDRH_POS * 2))
#define M_CSI_VSEQN(wlc)       (M_CSI_BLKS(wlc) + (C_CSI_VSEQN_POS * 2))
#define M_CSI_RXTSFL(wlc)      (M_CSI_BLKS(wlc) + (C_CSI_RXTSFL_POS * 2))
#define M_CSI_RXTSFML(wlc)     (M_CSI_BLKS(wlc) + (C_CSI_RXTSFML_POS * 2))
#define M_CSI_RSSI0(wlc)       (M_CSI_BLKS(wlc) + (C_CSI_RSSI0_POS * 2))
#define M_CSI_RSSI1(wlc)       (M_CSI_BLKS(wlc) + (C_CSI_RSSI1_POS * 2))

/** M2M callback data used by the callback function after the M2M xfer */
typedef struct csimon_m2m_cb_data {
	scb_t   *scb;		/**< pointer to the SCB */
	uint16  seqn;		/**< Sequence number in SHM - has valid bit */
	uint16  svmp_rpt_idx;	/**< CSI Report index in SVMP */
} csimon_m2m_cb_data_t;

union wlc_csimon_hdr {
	 uint8 u8[CSIMON_HEADER_SIZE];
	 struct {
	     uint32 format_id;		/**< Id/version of the CSI report format */
	     uint16 client_ea[3];	/**< client MAC address - 3 16-bit words */
	     uint16 bss_ea[3];		/**< BSS MAC address - 3 16-bit words */
	     chanspec_t chanspec;	/**< band, channel, bandwidth */
	     uint8 txstreams;		/**< number of tx spatial streams */
	     uint8 rxstreams;		/**< number of rx spatial streams */
	     uint32 report_ts;		/**< CSI Rx TSF timer timestamp */
	     uint32 assoc_ts;		/**< TSF timer timestamp at association time */
	     uint8 rssi[4];		/**< RSSI for each rx chain */
	 };
} __csimon_aligned;
typedef union wlc_csimon_hdr wlc_csimon_hdr_t;

struct wlc_csimon_rec {
	wlc_csimon_hdr_t csimon_hdr;
	uint8 csi_data[CSIMON_RING_ITEM_SIZE - CSIMON_HEADER_SIZE];
} __csimon_aligned;
typedef struct wlc_csimon_rec wlc_csimon_rec_t;

typedef struct csimon_xfer {

	/* Runtime State */
	uint32		    svmp_rd_idx;    /* Index of the SVMP CSI report */
	bcm_ring_t          local_ring;      /* ring of wlc_csimon_hdr_t or rec_t */
#if !defined(DONGLEBUILD)
	wlc_csimon_rec_t  * local_ring_base;
#else /* DONGLEBUILD */
	wlc_csimon_hdr_t  * local_ring_base;
	bcm_ring_t          host_ring;      /* current write and read index */
	csimon_ring_elem_t* table_base;     /* 32b ptr to table base in host */
	csimon_ipc_hme_t  * hme;
	/* PCIE IPC */
	uint64              wr_addr_u64;    /* csimon_ipc_hme::prmble::wr_idx */
	uint64              rd_addr_u64;    /* csimon_ipc_hme::prmble::rd_idx */
	haddr64_t           hme_haddr64;    /* csimon_ipc_hme */
#endif /* DONGLEBUILD */
#ifdef CSIMON_M2MCPY_BUILD
	m2m_dd_key_t        m2m_dd_csi;     /* M2M_DD_CSI usr registered key */
#endif // endif
	/* Statistics */
	uint32              bytes;          /* total bytes transferred */
	uint32              xfers;          /* CSI records transferred */
	uint32              drops;          /* CSI records dropped */
	uint32              m2m_drops;      /* CSI records dropped:M2M error */
	uint32              host_rng_drops; /* CSI records dropped:host ring full */
	uint32              fails;          /* CSI failures like m2m xfer */

} csimon_xfer_t;

struct wlc_csimon_sta {
	struct ether_addr ea;       /**< MAC addr - also in SCB if associated */
	uint16 timeout;                         /**< CSI timeout(ms) for STA */
	struct wl_timer *timer;                 /**< timer for CSI monitor */
	uint32 assoc_ts;                        /**< client association timestamp */
	/**< Callback data for M2M DMA xfer of the CSI data */
	csimon_m2m_cb_data_t m2m_cb_data;
	/**< run time boolean indicating if M2M copy is in progress */
	bool m2mcpy_busy;

	/**< Null frames sent */
	uint32 null_frm_cnt;
	/**< Number of CSI records successfully xferred by M2M DMA */
	uint32 m2mxfer_cnt;
	/**< Null frames not successfully acked */
	uint32 ack_fail_cnt;
	/**< The application is not reading the reports fast enough */
	uint32 rec_ovfl_cnt;
	/**< SVMP to host memory xfer failures */
	uint32 xfer_fail_cnt;
	/**< Both the CSI records/reports in mac memory were invalid */
	uint32 rpt_invalid_cnt;

	scb_t *scb;                             /**< back pointer to SCB */
};

struct wlc_csimon_info {
	/**< current number of clients doing CSI */
	uint8                num_clients;
	/**< CSIMON xfer related info */
	csimon_xfer_t        xfer;
	/**< Info about the clients doing CSI */
	wlc_csimon_sta_t     sta_info[CSIMON_MAX_STA];
	/**< CSIMON state like enabled/disabled and counters */
	csimon_state_t       state;
#if !defined(DONGLEBUILD)
	/**< Watchdog timeout(ms) for sending the CSIMON reports */
	uint16               wd_timeout;
	/**< Watchdog timer for sending the CSIMON reports */
	struct wl_timer     *wd_timer;
	/**< Netlink socket structure */
	struct sock         *nl_sock;
	/**< Netlink socket id */
	int                  nl_sock_id;
#endif /* !DONGLEBUILD */
	/**< M2M core interrupt name */
	char				irqname[32];
	/**< Back pointer to wlc */
	wlc_info_t          *wlc;
};

/* IOVar table */
enum {
	  IOV_CSIMON = 0, /* Enable/disable; Add/delete monitored STA's MAC addr */
	  IOV_CSIMON_STATE = 1, /* CSIMON enabled/disabled and counters */
	  IOV_LAST
};

static const bcm_iovar_t csimon_iovars[] = {
	{"csimon", IOV_CSIMON,
	(IOVF_SET_UP), 0, IOVT_BUFFER, sizeof(wlc_csimon_sta_config_t)
	},
	{"csimon_state", IOV_CSIMON_STATE,
	(0), 0, IOVT_BUFFER, sizeof(csimon_state_t)
	},
	{NULL, 0, 0, 0, 0, 0 }
};

static int8 wlc_csimon_sta_find(wlc_csimon_info_t *csimon_ctxt,
                                const struct ether_addr *ea);
static int wlc_csimon_doiovar(void *hdl, uint32 actionid, void *p, uint plen,
             void *a, uint alen, uint vsize, struct wlc_if *wlcif);

/* Ring macro used for NIC mode */
#define CSIMON_RING_IDX2ELEM(ring_base, idx) \
	    (((wlc_csimon_rec_t *)(ring_base)) + (idx))

/**
 * +--------------------------------------------------------------------------+
 *  Section: Functional Interface
 * +--------------------------------------------------------------------------+
 */
void // Add/start client-based timer
wlc_csimon_timer_add(wlc_info_t *wlc, struct scb *scb)
{
	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(scb);

	wl_add_timer(wlc->wl, scb->csimon->timer, scb->csimon->timeout, TRUE);

} // wlc_csimon_timer_add()

void // Delete/free client-based timer
wlc_csimon_timer_del(wlc_info_t *wlc, struct scb *scb)
{
	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(scb);

	wl_del_timer(wlc->wl, scb->csimon->timer);
	wl_free_timer(wlc->wl, scb->csimon->timer);
	scb->csimon->timer = NULL;
} // wlc_csimon_timer_del()

bool //  Check if client-based timer is allocated
wlc_csimon_timer_isnull(struct scb *scb)
{
	CSIMON_ASSERT(scb);

	if (scb->csimon->timer == NULL) {
		return TRUE;
	}
	return FALSE;
} // wlc_csimon_timer_isnull()

void // Set the association timestamp as a reference
wlc_csimon_assocts_set(wlc_info_t *wlc, struct scb *scb)
{
		uint32 tsf_l;

		CSIMON_ASSERT(wlc);
		CSIMON_ASSERT(scb);

		/* association timestamp as a reference - using TSF register */
		wlc_read_tsf(wlc, &tsf_l, NULL);
		scb->csimon->assoc_ts = tsf_l;
} // wlc_csimon_assocts_set()

bool // Check if CSIMON feature is enabled and STA is a member of the STA list
wlc_csimon_enabled(scb_t *scb)
{
	bool retval = FALSE;
	int idx = -1;

	if (scb && scb->bsscfg && scb->bsscfg->wlc) {
		wlc_info_t *wlc = scb->bsscfg->wlc;
		/* Also check if the STA is member of the STA list */
		if (CSIMON_ENAB(wlc->pub) &&
		   ((idx = wlc_csimon_sta_find(wlc->csimon_info, &scb->ea)) >= 0)) {
			CSIMON_DEBUG("wl%d:Found STA in the list at idx %d "
						 "for SCB DA "MACF"\n", wlc->pub->unit, idx,
						 ETHER_TO_MACF(scb->ea));
			retval = TRUE;
		} else {
			CSIMON_DEBUG("wl%d:csimon_enable %d STA in the list at idx %d "
			             "for SCB DA "MACF"\n", wlc->pub->unit,
			             CSIMON_ENAB(wlc->pub), idx, ETHER_TO_MACF(scb->ea));
		}
	}

	return retval;
} // wlc_csimon_enabled()

#if defined(DONGLEBUILD)
// Invoked from the command line
int pciedev_csimon_dump(void)
{
	printf("Use 'wl -i <wlX> dump csimon'.\n");

	return BCME_OK;
} // pciedev_csimon_dump()
#endif /* DONGLEBUILD */

int // CSIMON SCB Initialization
wlc_csimon_scb_init(wlc_info_t *wlc, scb_t *scb)
{
	int idx;

	if (!wlc || !scb) {
		return BCME_BADARG;
	}

	if (wlc_csimon_enabled(scb)) {
		/* Get the index of this client in the CSIMON_STA list */
		idx = wlc_csimon_sta_find(wlc->csimon_info, &scb->ea);
		 /* Set the pointer to the appropriate CSIMON_STA */
		if (idx >= 0) {
			scb->csimon = &(wlc->csimon_info->sta_info[idx]);
			/* Set the back pointer in the CSIMON_STA */
			wlc->csimon_info->sta_info[idx].scb = scb;
			CSIMON_DEBUG("wl%d:Found STA in the list at idx %d "
						 "for SCB DA "MACF"\n", wlc->pub->unit, idx,
						 ETHER_TO_MACF(scb->ea));
		} else { /* should not happen if wlc_csimon_enabled returns true */
			CSIMON_DEBUG("wl%d:NOT Found STA in the list at idx %d "
						 "for SCB DA "MACF"\n", wlc->pub->unit, idx,
						 ETHER_TO_MACF(scb->ea));
			return BCME_NOTFOUND;
		}
		/* Initialize the per-client timer */
		scb->csimon->timer = wl_init_timer(wlc->wl, wlc_csimon_scb_timer,
		                                   scb, "csimon");
		if (!(scb->csimon->timer)) {
			WL_ERROR(("wl%d: csimon timer init failed for "MACF"\n",
			          wlc->pub->unit, ETHER_TO_MACF(scb->ea)));
			return BCME_NORESOURCE;
		}
		CSIMON_DEBUG("wl%d:Init csimon timer %p timeout %u "
				"for SCB DA "MACF"\n", wlc->pub->unit, scb->csimon->timer,
				scb->csimon->timeout, ETHER_TO_MACF(scb->ea));
	}
	return BCME_OK;
}

void
wlc_csimon_scb_timer(void *arg)
{
	scb_t *scb = (scb_t *)arg;
	wlc_info_t *wlc;

	CSIMON_ASSERT(scb);

	wlc = scb->bsscfg->wlc;

	/* Send null frame for CSI Monitor */
	if (!wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, 0,
		WLF_CSI_NULL_PKT, PRIO_8021D_VO, NULL, NULL)) {
		WL_ERROR(("wl%d.%d: %s: wlc_sendnulldata failed\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(scb->bsscfg), __FUNCTION__));
	} else {
		CSIMON_DEBUG("wl%d: !!!!!!!!sent null frame for DA "MACF"\n",
				wlc->pub->unit, ETHER_TO_MACF(scb->ea));
		wlc->csimon_info->state.null_frm_cnt++;
		scb->csimon->null_frm_cnt++;
	}
	/* delete old CSI timer */
	wl_del_timer(scb->bsscfg->wlc->wl, scb->csimon->timer);

	/* add new CSI timer */
	wl_add_timer(scb->bsscfg->wlc->wl, scb->csimon->timer,
	             scb->csimon->timeout, TRUE);
	CSIMON_DEBUG("wl%d: TIMEOUT: added csi timer for %d ms SCB "MACF" scb %p\n",
			wlc->pub->unit, scb->csimon->timeout, ETHER_TO_MACF(scb->ea), scb);
}

#if !defined(DONGLEBUILD)

#ifdef CSIMON_FILE_BUILD
#include <linux/fs.h>

static int
wlc_csimon_write(wlc_csimon_info_t *csimon, void * csi_buf, ssize_t csi_buf_len)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
	ssize_t         ret;
	mm_segment_t    old_fs;
	struct file     *file;
	loff_t          offset;
	int             flags       = O_RDWR | O_CREAT | O_LARGEFILE | O_SYNC;
	const char      *filename   = CSIMON_FILENAME;

	ASSERT(csimon != NULL);
	ASSERT(csi_buf != NULL);

	old_fs = get_fs(); /* current addr_limit: user|kernel space */
	set_fs(KERNEL_DS); /* change to "kernel data segment" address limit */

	file = filp_open(filename, flags, 0600);
	if (IS_ERR(file)) {
		WL_ERROR(("%s filp_open(%s) failed\n", __FUNCTION__, filename));
		ret = PTR_ERR(file);
		goto exit2;
	}

	offset = default_llseek(file, 0, SEEK_END); /* seek to EOF */
	if (offset < 0) {
		ret = (ssize_t)offset;
		goto exit1;
	}

	if (offset >= CSIMON_FILESIZE) {    /* csimon file is bounded, drop */
		ret = BCME_NORESOURCE;
		goto exit1;
	}

	ret = kernel_write(file, csi_buf, csi_buf_len, &offset);
	if (ret != csi_buf_len) {
		WL_ERROR(("%s kernel_write %s failed: %zd\n",
			__FUNCTION__, filename, ret));
		if (ret > 0)
			ret = -EIO;
		goto exit1;
	}

	ret = BCME_OK;

exit1:
	filp_close(file, NULL);

exit2:
	set_fs(old_fs); // restore saved address limit

	return ret;

#else   /* LINUX_VERSION_CODE < 4 */
	return BCME_OK;
#endif	/* LINUX_VERSION_CODE < 4 */

} // wlc_csimon_write()

#else /* ! CSIMON_FILE_BUILD */

static int // send the CSI record over netlink socket to the user application
wlc_csimon_netlink_send(wlc_csimon_info_t *csimon, void * csi_buf, ssize_t csi_buf_len)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlmh = NULL;
	int ret;

	CSIMON_ASSERT(csimon);
	CSIMON_ASSERT(csimon->wlc);

	/* Allocate an SKB and its data area */
	skb = alloc_skb(NLMSG_SPACE(csi_buf_len), GFP_KERNEL);
	if (skb == NULL) {
		WL_ERROR(("Allocation failure!\n"));
		return BCME_NORESOURCE;
	}
	skb_put(skb, NLMSG_SPACE(csi_buf_len));

	/* Fill in the netlink msg header that is at skb->data */
	nlmh = (struct nlmsghdr *)skb->data;
	nlmh->nlmsg_len = NLMSG_SPACE(csi_buf_len);
	nlmh->nlmsg_pid = 0; /* kernel */
	nlmh->nlmsg_flags = 0;

	/* Copy the CSI record into the netlink SKB structure */
	/* XXX Can we avoid this copy inside kernel space? Apparently mmap netlink
	 * could do but not sure if we can use it in this case where we have the
	 * CSI records in HME. Also the mmap netlink support has been removed from
	 * the latest Linux kernels.
	 */
	memcpy(NLMSG_DATA(nlmh), csi_buf, csi_buf_len);
	{
		struct timespec64 ts;
		ktime_get_real_ts64(&ts);
		CSIMON_DEBUG("wl%d: nl multicast at %lu.%lu for skb %p nlmh %p \n",
		  csimon->wlc->pub->unit, (ulong)ts.tv_sec, ts.tv_nsec/1000, skb, nlmh);
	}

	/* Send the CSI record over netlink multicast */
	if (csimon->nl_sock == NULL) {
		WL_ERROR(("%s: nl mcast not done as nl_sock is NULL\n", __FUNCTION__));
		return BCME_NORESOURCE;
	}
	ret = netlink_broadcast(csimon->nl_sock, skb, 0, CSIMON_GRP_BIT, GFP_KERNEL);
	if (ret < 0) {
		WL_ERROR(("%s: nl mcast failed %d sock %p skb %p\n", __FUNCTION__,
		           ret, csimon->nl_sock, skb));
		return ret;
	}
	else {
		CSIMON_DEBUG("wl%d: nl mcast successful ret %d socket %p skb %p\n",
		           csimon->wlc->pub->unit, ret, csimon->nl_sock, skb);
	}

	return BCME_OK;

#else   /* LINUX_VERSION_CODE < 4 */
	return BCME_OK;
#endif	/* LINUX_VERSION_CODE < 4 */
}

#endif /* ! CSIMON_FILE_BUILD */

static void // Watchdog-like timer for CSIMON to transfer CSI records to the user-space
wlc_csimon_wd_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	int elem_idx = 0;
	static uint32 records = 0;
	wlc_csimon_rec_t *elem;

	CSIMON_ASSERT(wlc);

	/* Transfer the CSI records present in the host memory ring */
	while ((elem_idx = bcm_ring_cons(&CSIxfer(wlc).local_ring, CSIMON_LOCAL_RING_DEPTH))
	            != BCM_RING_EMPTY) {
		elem = CSIMON_RING_IDX2ELEM(CSIxfer(wlc).local_ring_base, elem_idx);
		OSL_CACHE_INV((void*)(elem), CSIMON_RING_ITEM_SIZE);

		/* print on console if enabled */
		//__wlc_csimon_rpt_print_console((uint16 *)(elem->data));

#ifdef CSIMON_FILE_BUILD
		/* Transfer to a file */
		if (wlc_csimon_write(wlc->csimon_info, elem, CSIMON_RING_ITEM_SIZE) != BCME_OK) {
			wlc->csimon_info->state.usr_xfer_fail_cnt++;
		}
		else {
			wlc->csimon_info->state.usrxfer_cnt++;
		}
#else /* ! CSIMON_FILE_BUILD */
		/* Transfer over netlink socket */
		if (wlc_csimon_netlink_send(wlc->csimon_info, (void *) elem,
		    CSIMON_RING_ITEM_SIZE)) {
			wlc->csimon_info->state.usr_xfer_fail_cnt++;
			WL_ERROR(("%s: CSIMON drops with nl send %d\n", __FUNCTION__,
			           wlc->csimon_info->state.usr_xfer_fail_cnt));
		}
		else {
			wlc->csimon_info->state.usrxfer_cnt++;
		}
#endif /* ! CSIMON_FILE_BUILD */
		records++;
		CSIMON_DEBUG("wl%d: num_records %u\n", wlc->pub->unit, records);
	}

	/* delete old wd timer */
	wl_del_timer(wlc->wl, wlc->csimon_info->wd_timer);

	/* add new wd timer */
	wl_add_timer(wlc->wl, wlc->csimon_info->wd_timer, wlc->csimon_info->wd_timeout, TRUE);
	CSIMON_DEBUG("wl%d: TIMEOUT: added wd timer for %d ms \n",
			wlc->pub->unit, wlc->csimon_info->wd_timeout);
}
#endif /* !DONGLEBUILD */

#if defined(CSIMON_SBCOPY_BUILD)
static INLINE int // Transfer a CSI structure to Host using synchronous copy
__wlc_csimon_xfer_sbcopy(void * csi_hdr, int csi_len, uint32 svmp_addr,
	void *xfer_scb)
{
	int wr_idx;
	csimon_ring_elem_t *elem;
	uint64 elem_haddr_u64;

	CSIMON_ASSERT(csi_len <= CSIMON_RING_ITEM_SIZE);
	CSIMON_ASSERT((csi_len % sizeof(int)) == 0); // pciedev_sbcopy is 32b copies

	/* Lazilly refresh the host updated read index */
	if (bcm_ring_is_full(&CSIxfer(wlc).host_ring, CSIMON_RING_ITEMS_MAX)) {
		uint32 lazy_rd_idx;
		pciedev_sbcopy(CSIxfer(wlc).rd_addr_u64, (uintptr)&lazy_rd_idx, 1, TRUE);

		CSIxfer(wlc).host_ring.read = lazy_rd_idx; // refresh and try again
		if (bcm_ring_is_full(&CSIxfer(wlc).host_ring, CSIMON_RING_ITEMS_MAX)) {
			CSIxfer(wlc).drops++;
			return BCME_NORESOURCE;
		}
	}

	/* Determine the destination in host circular ring */
	wr_idx = bcm_ring_prod(&CSIxfer(wlc).host_ring, CSIMON_RING_ITEMS_MAX);
	elem = CSIMON_TABLE_IDX2ELEM(CSIxfer(wlc).table_base, wr_idx);

	/* M2M transfer the CSI structure to destination */
	elem_haddr_u64 = (((uint64)(HADDR64_HI(CSIxfer(wlc).hme_haddr64))) << 32)
	               | (uintptr)elem;
	pciedev_sbcopy(elem_haddr_u64, (uintptr)csi_hdr,
	               csi_len / sizeof(int), FALSE);

	CSIxfer(wlc).xfers += 1;
	CSIxfer(wlc).bytes += csi_len;

	/* Post the updated write index to the host ring preamble */
	pciedev_sbcopy(CSIxfer(wlc).wr_addr_u64,
	               (uintptr)&CSIxfer(wlc).host_ring.write, 1, FALSE);

	return BCME_OK;

}  // __wlc_csimon_xfer_sbcopy()

#endif /* CSIMON_SBCOPY_BUILD */

#if defined(CSIMON_M2MCPY_BUILD)
/*
 * Transfer CSI record to Host using asynchronous copy. A CSI record consists of
 * a fixed width CSI header that contains the channel number, MAC address, TSF,
 * RSSI etc. The CSI record also has a CSI report (H-matrix data) that is
 * transferred to the host without peeking into it. The CSI report could have
 * variable length based on phy bandwidth, number of Rx antenna, and decimation
 * factor but here we transfer a fixed length data from SVMP memory. It is the
 * user application's responsibility to pick correct length of the report based
 * on the report metadata inside the first 32 bytes of the report.
 *
 * csi_hdr:		Sysmem resident CSI header that includes MAC addr, RSSI
 * csi_len:		Total length of data to be transferred; it includes CSI
 *			header of CSIMON_HEADER_LENGTH length and CSI report
 * svmp_addr_u32:	Address of the CSI report resident in SVMP memory
 * xfer_arg:		Callback function to be called after the data transfer
 *			is complete
 *
 */
#if defined(DONGLEBUILD)
static INLINE int
__wlc_csimon_xfer_m2mcpy(wlc_info_t *wlc, scb_t *scb, void * csi_hdr,
                         int csi_len, uint32 *svmp_addrp_u32, void *xfer_arg)
{
	int wr_idx;
	m2m_dd_cpy_t cpy_key;
	csimon_ring_elem_t *elem;
#ifdef SINGLE_M2M_XFER
	dma64addr_t xfer_src, xfer_dst;
#else /* ! SINGLE_M2M_XFER */
	dma64addr_t xfer_src_sv, xfer_dst_sv, xfer_src_sm, xfer_dst_sm;
#endif /* ! SINGLE_M2M_XFER */

	CSIMON_ASSERT(csi_len <= CSIMON_RING_ITEM_SIZE);
	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(wlc->csimon_info);
	CSIMON_ASSERT(scb);
	CSIMON_ASSERT(scb->csimon);

	/* Lazilly refresh the host updated read index */
	if (bcm_ring_is_full(&CSIxfer(wlc).host_ring, CSIMON_RING_ITEMS_MAX)) {
		uint32 lazy_rd_idx;
		pciedev_sbcopy(CSIxfer(wlc).rd_addr_u64, (uintptr)&lazy_rd_idx, 1, TRUE);

		CSIxfer(wlc).host_ring.read = lazy_rd_idx; // refresh and try again
		if (bcm_ring_is_full(&CSIxfer(wlc).host_ring, CSIMON_RING_ITEMS_MAX)) {
			CSIxfer(wlc).drops++;
			CSIxfer(wlc).host_rng_drops++;
			wlc->csimon_info->state.rec_ovfl_cnt++;
			scb->csimon->rec_ovfl_cnt++;
			return BCME_NORESOURCE;
		}
	}
	CSIMON_DEBUG("osh %p key %d\n", wlc->osh, CSIxfer(wlc).m2m_dd_csi);

	if (m2m_dd_avail(wlc->osh, CSIxfer(wlc).m2m_dd_csi) < 2) {
		wlc->csimon_info->state.xfer_fail_cnt++;
		scb->csimon->xfer_fail_cnt++;
		return BCME_NORESOURCE;
	}

	/* Determine the destination in host circular ring */
	wr_idx          = bcm_ring_prod(&CSIxfer(wlc).host_ring,
	                                CSIMON_RING_ITEMS_MAX);
	elem            = CSIMON_TABLE_IDX2ELEM(CSIxfer(wlc).table_base, wr_idx);

#ifdef SINGLE_M2M_XFER
	xfer_src.hiaddr = 0U;
	xfer_src.loaddr = (uint32)((uintptr)csi_hdr);

	xfer_dst.hiaddr = HADDR64_HI(CSIxfer(wlc).hme_haddr64);
	xfer_dst.loaddr = (uint32)((uintptr)(elem));
	cpy_key	= m2m_dd_xfer(wlc->osh, CSIxfer(wlc).m2m_dd_csi, &xfer_src, &xfer_dst,
	                      csi_len, xfer_arg,
	                      M2M_DD_XFER_COMMIT | M2M_DD_XFER_RESUME);
	if (cpy_key == M2M_INVALID) {
		wlc->csimon_info->state.xfer_fail_cnt++;
		scb->csimon->xfer_fail_cnt++;
		WL_ERROR(("CSIMON: Failure %u in m2m_dd_xfer\n", CSIxfer(wlc).fails));
		CSIMON_ASSERT(0);
	} else {
		CSIxfer(wlc).xfers += 1;
		CSIxfer(wlc).bytes += csi_len;
	}
	CSIMON_DEBUG("xfer sys mem 0x%x to elem idx %d host 0x%x len %u"
			" xfers %u\n", xfer_src.loaddr,
			wr_idx, xfer_dst.loaddr, csi_len, CSIxfer(wlc).xfers);

#else /* ! SINGLE_M2M_XFER */

	/* src_sv is csi report in SVMP memory */
	xfer_src_sv.hiaddr = 0U;
	xfer_src_sv.loaddr = (uint32)((uintptr)svmp_addrp_u32);

	/* dst_sv is element at index wr_idx plus an offset in host circular ring */
	xfer_dst_sv.hiaddr = HADDR64_HI(CSIxfer(wlc).hme_haddr64);
	xfer_dst_sv.loaddr = (uint32)((uintptr)elem) + CSIMON_HEADER_SIZE;

	/* src_sm is csi data in dongle system memory */
	xfer_src_sm.hiaddr = 0U;
	xfer_src_sm.loaddr = (uint32)((uintptr)csi_hdr);

	/* dst_sm is element at index wr_idx in host circular ring */
	xfer_dst_sm.hiaddr = HADDR64_HI(CSIxfer(wlc).hme_haddr64);
	xfer_dst_sm.loaddr = (uint32)((uintptr)elem);

	CSIMON_DEBUG("xfer SVMP mem 0x%x to idx %d host 0x%x len %u\n",
			xfer_src_sv.loaddr, wr_idx,
	        xfer_dst_sv.loaddr, (csi_len - CSIMON_HEADER_SIZE));

	cpy_key		   = m2m_dd_xfer(wlc->osh, CSIxfer(wlc).m2m_dd_csi,
	                              &xfer_src_sv, &xfer_dst_sv,
	                              (csi_len - CSIMON_HEADER_SIZE),
	                              0U, 0U);
	if (cpy_key == M2M_INVALID) {
		WL_ERROR(("CSIMON: Failure %u in first m2m_dd_xfer\n",
		          CSIxfer(wlc).fails));
	        CSIMON_ASSERT(0);
	}
	cpy_key		   = m2m_dd_xfer(wlc->osh, CSIxfer(wlc).m2m_dd_csi,
	                         &xfer_src_sm, &xfer_dst_sm, CSIMON_HEADER_SIZE,
	                         xfer_arg, M2M_DD_XFER_COMMIT | M2M_DD_XFER_RESUME);

	if (cpy_key == M2M_INVALID) {
		wlc->csimon_info->state.xfer_fail_cnt++;
		scb->csimon->xfer_fail_cnt++;
		WL_ERROR(("CSIMON: Failure %u in 2nd m2m_dd_xfer\n", CSIxfer(wlc).fails));
		CSIMON_ASSERT(0);
	} else {
		CSIxfer(wlc).xfers += 1;
		CSIxfer(wlc).bytes += csi_len;
	}
	CSIMON_DEBUG("xfer sys mem 0x%x to host 0x%x len %u xfers %u\n",
	             xfer_src_sm.loaddr, xfer_dst_sm.loaddr,
	             CSIMON_HEADER_SIZE, CSIxfer(wlc).xfers);
#endif /* ! SINGLE_M2M_XFER */

	return BCME_OK;
} // __wlc_csimon_xfer_m2mcpy()
#else /* ! DONGLEBUILD */
static INLINE int
__wlc_csimon_xfer_m2mcpy(wlc_info_t *wlc, scb_t *scb, void * csi_rec,
                         int csi_len, uint32 *svmp_addrp_u32, void *xfer_arg)
{
	m2m_dd_cpy_t cpy_key;
	dma64addr_t xfer_src, xfer_dst;
	uintptr xfer_dst_u64;

	CSIMON_ASSERT(csi_len <= CSIMON_RING_ITEM_SIZE);
	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(wlc->csimon_info);
	CSIMON_ASSERT(scb);
	CSIMON_ASSERT(scb->csimon);

	if (m2m_dd_avail(wlc->osh, CSIxfer(wlc).m2m_dd_csi) < 1) {
		wlc->csimon_info->state.xfer_fail_cnt++;
		scb->csimon->xfer_fail_cnt++;
		return BCME_NORESOURCE;
	}

	/* Determine the destination in local circular ring */
	xfer_src.hiaddr = 0U;
	xfer_src.loaddr = (uint32)((uintptr)svmp_addrp_u32);

	xfer_dst_u64 = (uintptr)VIRT_TO_PHYS((wlc_csimon_rec_t *)csi_rec +
	                                     CSIMON_HEADER_SIZE);
#if defined(__ARM_ARCH_7A__)
	xfer_dst.hiaddr = 0U;
#else
	xfer_dst.hiaddr = (uint32)(((uint64)xfer_dst_u64) >> 32);
#endif // endif
	xfer_dst.loaddr = (uint32)(xfer_dst_u64);

	cpy_key	= m2m_dd_xfer(wlc->osh, CSIxfer(wlc).m2m_dd_csi, &xfer_src,
	                      &xfer_dst, (csi_len - CSIMON_HEADER_SIZE),
	                      xfer_arg,
	                      M2M_DD_XFER_COMMIT | M2M_DD_XFER_RESUME);
	if (cpy_key == M2M_INVALID) {
		wlc->csimon_info->state.xfer_fail_cnt++;
		scb->csimon->xfer_fail_cnt++;
		WL_ERROR(("CSIMON: Failure %u in m2m_dd_xfer\n", CSIxfer(wlc).fails));
		CSIMON_ASSERT(0);
	}
	CSIMON_DEBUG("xfer SVMP mem 0x%08x local hi 0x%08x lo 0x%08x csi_rec %p len"
				 " %u\n", xfer_src.loaddr, xfer_dst.hiaddr, xfer_dst.loaddr,
				 csi_rec, csi_len - CSIMON_HEADER_SIZE);

	return BCME_OK;
} // __wlc_csimon_xfer_m2mcpy()
#endif /* ! DONGLEBUILD */

#endif /* CSIMON_M2MCPY_BUILD */

static INLINE int // Transfer a CSI record to host DDR
__wlc_csimon_xfer(wlc_info_t *wlc, scb_t *scb, void * csi_hdr, int csi_len,
                  uint32 *svmp_addrp, void *cb_data)
{
#if   defined(CSIMON_SBCOPY_BUILD)
	return __wlc_csimon_xfer_sbcopy(csi_hdr, csi_len, svmp_addrp, cb_data);
#elif defined(CSIMON_M2MCPY_BUILD)
	return __wlc_csimon_xfer_m2mcpy(wlc, scb, csi_hdr, csi_len, svmp_addrp,
	                                cb_data);
#else
	return BCME_ERROR;
#endif // endif
} // wlc_csimon_xfer()

static void	// Clear the client level statistics
wlc_csimon_sta_stats_clr(wlc_csimon_info_t *ctxt, int8 idx)
{
	if (idx < 0 || idx >= CSIMON_MAX_STA) {
		WL_ERROR(("Incorrect client index %d for deletion of stats\n", idx));
		return;
	}
	ctxt->sta_info[idx].null_frm_cnt = 0;
	ctxt->sta_info[idx].m2mxfer_cnt = 0;
	ctxt->sta_info[idx].ack_fail_cnt = 0;
	ctxt->sta_info[idx].rec_ovfl_cnt = 0;
	ctxt->sta_info[idx].xfer_fail_cnt = 0;
	return;
} // wlc_csimon_sta_staiiiiits_clr()

static int // Dump all the CSIMON counters: per-module and per-client
wlc_csimon_dump(void *ctx, bcmstrbuf_t *b)
{
	wlc_csimon_info_t *c_info = (wlc_csimon_info_t *)ctx;
	int idx;

	bcm_bprintf(b, "CSI Monitor: %u\n", CSIMON_ENAB(WLCPUB(c_info)));
	bcm_bprintf(b, "Number of clients: %u\n\n", c_info->num_clients);
	bcm_bprintf(b, "Aggregate Statistics\n------------------------\n");

#if defined(DONGLEBUILD)
	bcm_bprintf(b, "null_frm_cnt %u xfer_to_ddr_cnt %u ack_fail_cnt %u\n"
	            "rec_ovfl_cnt %u xfer_to_ddr_fail_cnt %u\n\n",
	            c_info->state.null_frm_cnt, c_info->state.m2mxfer_cnt,
	            c_info->state.ack_fail_cnt,
	            c_info->state.rec_ovfl_cnt, c_info->state.xfer_fail_cnt);
#else /* ! DONGLEBUILD */
	bcm_bprintf(b, "null_frm_cnt %u xfer_to_usr_cnt %u ack_fail_cnt %u\n"
	            "rec_ovfl_cnt %u xfer_to_ddr_fail_cnt %u "
	            "xfer_to_usr_fail_cnt %u\n\n",
	            c_info->state.null_frm_cnt, c_info->state.usrxfer_cnt,
	            c_info->state.ack_fail_cnt, c_info->state.rec_ovfl_cnt,
	            c_info->state.xfer_fail_cnt, c_info->state.usr_xfer_fail_cnt);
#endif /* ! DONGLEBUILD */
	bcm_bprintf(b, "Per-client Statistics\n------------------------\n");
	for (idx = 0; idx < CSIMON_MAX_STA; idx++) {
		if (bcmp(&ether_null, &c_info->sta_info[idx].ea, ETHER_ADDR_LEN) == 0) {
			continue;
		}
		bcm_bprintf(b, MACF"\t monitor_interval %d milliseconds\n",
		            ETHER_TO_MACF(c_info->sta_info[idx].ea),
		            c_info->sta_info[idx].timeout);
		bcm_bprintf(b, "null_frm_cnt %u xfer_to_ddr_cnt %u ack_fail_cnt %u\n"
		 "rec_ovfl_cnt %u xfer_to_ddr_fail_cnt %u\n\n",
		 c_info->sta_info[idx].null_frm_cnt, c_info->sta_info[idx].m2mxfer_cnt,
		 c_info->sta_info[idx].ack_fail_cnt, c_info->sta_info[idx].rec_ovfl_cnt,
		 c_info->sta_info[idx].xfer_fail_cnt);
	}

	bcm_bprintf(b, "More Details\n------------------------\n");
#if defined(DONGLEBUILD)
	bcm_bprintf(b, "Version:" CSIMON_VRP_FMT "\nHOST ADDR" HADDR64_FMT
		"\nHOST RING %p WR %u RD %u\nDNGL RING %p WR %u RD %u\n",
		CSIMON_VRP_VAL(CSIMON_VERSIONCODE), HADDR64_VAL(c_info->xfer.hme_haddr64),
		c_info->xfer.table_base, c_info->xfer.host_ring.write,
		c_info->xfer.host_ring.read, c_info->xfer.local_ring_base,
		c_info->xfer.local_ring.write, c_info->xfer.local_ring.read);
#ifdef BCM_DEBUG
		bcm_bprintf(b, "\tHME: wr_addr %08x rd_addr %08x\n",
			(uint32)c_info->xfer.wr_addr_u64, (uint32)c_info->xfer.rd_addr_u64);
#endif // endif
#else /* ! DONGLEBUILD */
	bcm_bprintf(b, "Version:" CSIMON_VRP_FMT "\nLOCAL RING %p WR %u RD %u\n",
		CSIMON_VRP_VAL(CSIMON_VERSIONCODE), c_info->xfer.local_ring_base,
		c_info->xfer.local_ring.write, c_info->xfer.local_ring.read);
#ifndef CSIMON_FILE_BUILD
	bcm_bprintf(b, "Netlink subsystem for CSIMON: %d\n", c_info->nl_sock_id);
#endif // endif
#endif /* ! DONGLEBUILD */

	return BCME_OK;
} // wlc_csimon_dump()

static int // Clear all the CSIMON counters: per-module and per-client
wlc_csimon_dump_clr(void *ctx)
{
	wlc_csimon_info_t *c_info = (wlc_csimon_info_t *)ctx;
	int idx;

	/* Clear the aggregate statistics */
	c_info->state.null_frm_cnt = 0;
	c_info->state.m2mxfer_cnt = 0;
	c_info->state.usrxfer_cnt = 0;
	c_info->state.ack_fail_cnt = 0;
	c_info->state.rec_ovfl_cnt = 0;
	c_info->state.xfer_fail_cnt = 0;
	c_info->state.usr_xfer_fail_cnt = 0;

	/* Now clear the client level statistics */
	for (idx = 0; idx < CSIMON_MAX_STA; idx++) {
		if (bcmp(&ether_null, &c_info->sta_info[idx].ea, ETHER_ADDR_LEN) == 0) {
			continue;
		}
		wlc_csimon_sta_stats_clr(c_info, idx);
	}

	return BCME_OK;
} // wlc_csimon_dump_clr()

wlc_csimon_info_t * // Attach the CSIMON module
BCMATTACHFN(wlc_csimon_attach)(wlc_info_t *wlc)
{
	int                  i;
	wlc_csimon_info_t   *csimon_ctxt = NULL;
	uint32               mem_size;
	uint32               align_bits = 6;	/* 64-Byte alignment */
	dmaaddr_t            pap;			/* ignored */
	uint32               alloced;		/* ignored */
#if defined(DONGLEBUILD)
	wlc_csimon_hdr_t    *local_ring_base;
	/* Dongle sysmem-resident ring element holds CSI header */
	mem_size = CSIMON_HEADER_SIZE * CSIMON_LOCAL_RING_DEPTH;
#else /* ! DONGLEBUILD */
	wlc_csimon_rec_t    *local_ring_base;
	/* NIC host memory resident whole CSI record */
	mem_size = CSIMON_RING_ITEM_SIZE * CSIMON_LOCAL_RING_DEPTH;
#endif /* ! DONGLEBUILD */

	if (!wlc) return NULL;

	/* Memory allocation */
	csimon_ctxt = (wlc_csimon_info_t*)MALLOCZ(wlc->pub->osh,
			sizeof(wlc_csimon_info_t));
	if (csimon_ctxt == NULL) {
		WL_ERROR(("wl%d: %s: csimon_ctxt MALLOCZ failed; total "
			"mallocs %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	csimon_ctxt->wlc = wlc;

	if (D11REV_LT(wlc->pub->corerev, 129)) {
		WL_ERROR(("wl%d: %s: Not enabling CSIMON support for D11 rev %d\n",
			wlc->pub->unit, __FUNCTION__, wlc->pub->corerev));
		return csimon_ctxt;
	}

	/* STA list for handling multiple STAs for CSI */
	csimon_ctxt->num_clients = 0;
	for (i = 0; i < CSIMON_MAX_STA; i++) {
		memset(&csimon_ctxt->sta_info[i], 0, sizeof(wlc_csimon_sta_t));
	}

#if !defined(DONGLEBUILD)
	csimon_ctxt->wd_timeout = 100;
	/* Initialize the CSIMON watchdog timer */
	csimon_ctxt->wd_timer = wl_init_timer(wlc->wl, wlc_csimon_wd_timer,
	                                      wlc, "csimon_wd");
	if (!(csimon_ctxt->wd_timer)) {
		WL_ERROR(("wl%d: csimon wd timer init failed \n", wlc->pub->unit));
		MFREE(wlc->pub->osh, csimon_ctxt, sizeof(wlc_csimon_info_t));
		return NULL;
	}
	CSIMON_DEBUG("wl%d:Init csimon wd timer %p timeout %u\n", wlc->pub->unit,
	              csimon_ctxt->wd_timer, csimon_ctxt->wd_timeout);
#endif /* ! DONGLEBUILD */

	/* Memory for CSI headers for FD and whole CSI records for NIC mode */
	local_ring_base = DMA_ALLOC_CONSISTENT(wlc->osh, mem_size, align_bits, &alloced,
	                                          (&pap), NULL);
	if (local_ring_base == NULL) {
		WL_ERROR(("wl%d: %s: csimon local_ring_mem alloc failed; size %u "
			"align_bits %u depth %u\n",
			wlc->pub->unit, __FUNCTION__, mem_size, align_bits,
			CSIMON_LOCAL_RING_DEPTH));
#if !defined(DONGLEBUILD)
		wl_free_timer(wlc->wl, csimon_ctxt->wd_timer);
#endif /* ! DONGLEBUILD */
		MFREE(wlc->pub->osh, csimon_ctxt, sizeof(wlc_csimon_info_t));
		return NULL;
	}
	csimon_ctxt->xfer.local_ring_base = local_ring_base;
	csimon_ctxt->xfer.m2m_dd_csi = M2M_INVALID;

	/* Register module */
	if (wlc_module_register(
			    wlc->pub,
			    csimon_iovars,
			    "csimon",
			    csimon_ctxt,
			    wlc_csimon_doiovar,
			    NULL,
			    NULL,
			    NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          WLCUNIT(csimon_ctxt), __FUNCTION__));
		MFREE(wlc->pub->osh, csimon_ctxt, sizeof(wlc_csimon_info_t));
		DMA_FREE_CONSISTENT(wlc->osh, (void *)local_ring_base, mem_size,
		                    pap, NULL);
		return NULL;
	}

	/* Register csimon dump function */
	wlc_dump_add_fns(wlc->pub, "csimon", wlc_csimon_dump, wlc_csimon_dump_clr,
	                 csimon_ctxt);

	printf("CSIMON module registered\n");
#if !defined(DONGLEBUILD)
#ifndef CSIMON_FILE_BUILD
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
	{
		int nl_id = 0;

		/* There is a netlink socket per radio */
		if (wlc->pub->unit == 0) {
			nl_id = NETLINK_CSIMON0;
		} else if (wlc->pub->unit == 1) {
			nl_id = NETLINK_CSIMON1;
		} else { /* Third radio */
			nl_id = NETLINK_CSIMON2;
		}
		csimon_ctxt->nl_sock_id = nl_id;
		csimon_ctxt->nl_sock = netlink_kernel_create(&init_net, nl_id, NULL);
		if (csimon_ctxt->nl_sock == NULL) {
			csimon_ctxt->nl_sock_id = -1;
			WL_ERROR(("wl%d: Failed creating netlink: nl_id %d\n",
			           wlc->pub->unit, nl_id));
		} else {
			CSIMON_DEBUG("wl%d: Created netlink: nl_id %d\n",
			           wlc->pub->unit, nl_id);
		}
	}
#endif	/* LINUX_VERSION_CODE < 4 */
#endif /* ! CSIMON_FILE_BUILD */
#endif /* !DONGLEBUILD */
	return csimon_ctxt;
} // wlc_csimon_attach()

int // Initialize the global CSIMON object
wlc_csimon_init(wlc_info_t *wlc)
{
#if defined(DONGLEBUILD)
	uint64              hme_addr_u64;
	csimon_preamble_t   preamble;
	int                 len_4B;
#endif /* DONGLEBUILD */

	printf("CSIMON: " CSIMON_VRP_FMT " Initialization\n",
		CSIMON_VRP_VAL(CSIMON_VERSIONCODE));

	bcm_ring_init(&CSIxfer(wlc).local_ring);

#if defined(DONGLEBUILD)
	if (CSIxfer(wlc).table_base != (csimon_ring_elem_t *)NULL) {
		printf("CSIMON: already initialized ...\n");
		return BCME_OK;
	}

	bcm_ring_init(&CSIxfer(wlc).host_ring);
	CSIxfer(wlc).hme_haddr64  = hme_get(HME_USER_CSIMON, CSIMON_IPC_HME_BYTES);
	CSIMON_ASSERT(HADDR64_LO(CSIxfer(wlc).hme_haddr64) != 0U);

	CSIxfer(wlc).hme          = (csimon_ipc_hme_t *)
	                        ((uintptr)HADDR64_LO(CSIxfer(wlc).hme_haddr64));
	CSIxfer(wlc).table_base   = &(CSIxfer(wlc).hme->table[0]);

	HADDR64_TO_U64(CSIxfer(wlc).hme_haddr64, hme_addr_u64);

	CSIxfer(wlc).wr_addr_u64  = hme_addr_u64
	                      + OFFSETOF(csimon_ipc_hme_t, preamble)
	                      + OFFSETOF(csimon_preamble_t, write_idx);
	CSIxfer(wlc).rd_addr_u64  = hme_addr_u64
	                      + OFFSETOF(csimon_ipc_hme_t, preamble)
	                      + OFFSETOF(csimon_preamble_t, read_idx);

	/* Fill up preamble next and transfer to host */
	memset(&preamble, 0, sizeof(csimon_preamble_t));
	len_4B = sizeof(csimon_preamble_t) / sizeof(int);

	preamble.version_code = CSIMON_VERSIONCODE;
	preamble.elem_size    = CSIMON_RING_ITEM_SIZE;
	preamble.table_daddr32 = (uintptr)&CSIxfer(wlc).hme->table[0];
	pciedev_sbcopy(hme_addr_u64, (uintptr)&preamble, len_4B, FALSE);
	CSIMON_DEBUG("wl%d: versioncode %d ring elem size %d\n",
	             wlc->pub->unit, preamble.version_code, preamble.elem_size);

#endif /* DONGLEBUILD */
#ifdef CSIMON_M2MCPY_BUILD
	if (CSIxfer(wlc).m2m_dd_csi != M2M_INVALID) {
		printf("CSIMON: M2M usr already registered ...\n");
		return BCME_OK;
	}

	CSIxfer(wlc).m2m_dd_csi = m2m_dd_usr_register(wlc->osh, M2M_DD_CSI,
	                            M2M_DD_CH0, &CSIxfer(wlc), wlc_csimon_m2m_dd_done_cb,
	                            NULL, 0); // No wake cb, threshold
#endif /* CSIMON_M2MCPY_BUILD */
	CSIMON_DEBUG("wl%d: m2m_dd_key %d\n", wlc->pub->unit, CSIxfer(wlc).m2m_dd_csi);
	return BCME_OK;
}  // wlc_csimon_init()

void // Detach the CSIMON module
BCMATTACHFN(wlc_csimon_detach)(wlc_csimon_info_t *ci)
{
	uint32 mem_size = 0;
	dmaaddr_t pa;		/* ignored */

	CSIMON_ASSERT(ci);
	CSIMON_ASSERT(ci->wlc);

	BCM_REFERENCE(mem_size);

	/* Free the local memory for CSI headers/records */
#if defined(DONGLEBUILD)
	mem_size = CSIMON_HEADER_SIZE * CSIMON_LOCAL_RING_DEPTH;
#else /* ! DONGLEBUILD */
	mem_size = CSIMON_RING_ITEM_SIZE * CSIMON_LOCAL_RING_DEPTH;
#endif /* ! DONGLEBUILD */
	PHYSADDRHISET(pa, 0);
	PHYSADDRLOSET(pa, 0);
	BCM_REFERENCE(pa);

	DMA_FREE_CONSISTENT(ci->wlc->osh, (void *)ci->xfer.local_ring_base,
	                    mem_size, pa, NULL);

	/* Free the CSIMON data structure memory */
	MFREE(ci->wlc->osh, ci, sizeof(wlc_csimon_info_t));

}  // wlc_csimon_detach()

static INLINE void // Reset valid bit in ucode SHM to indicate report has been read
__wlc_csimon_shm_update(wlc_info_t *wlc, uint16 seqn, uint16 svmp_rpt_idx)
{
	seqn &= ~ (1 << C_CSI_VLD_NBIT);
	wlc_write_shm(wlc, (M_CSI_VSEQN(wlc) + svmp_rpt_idx*16), seqn);
	CSIMON_DEBUG("wl%d: Reset valid bit: 0x%x to shmem 0x%x \n", wlc->pub->unit,
	             seqn, (M_CSI_VSEQN(wlc) + svmp_rpt_idx*16));
} // wlc_csimon_shm_update()

//#define CSIMON_PIO_CONSOLE_PRINT 1

static INLINE void // Print the CSI 'report' to the console
__wlc_csimon_rpt_print_console(wlc_info_t *wlc, wlc_csimon_rec_t *csimon_rec,
                               uint32 svmp_offset, uint16 seqn, uint16 rpt_idx)
{
#ifdef CSIMON_PIO_CONSOLE_PRINT
	uint16 mem_len = CSIMON_REPORT_SIZE / sizeof(uint16); /* 16-bit words */
	int i, j;
	uint num_col = 16, mem_to_dump = 256;
	uint16 rpt_words[(CSIMON_RING_ITEM_SIZE-CSIMON_HEADER_SIZE)/sizeof(uint16)];

	/* Copy the CSI report from SVMP memory to local memory */
	wlc_svmp_mem_read(wlc->hw, rpt_words, svmp_offset, mem_len);
	printf("wl%d:copied SVMP Offset 0x%x into local mem 0x%p\n",
			wlc->pub->unit, svmp_offset, rpt_words);

	/* Dump the report */
	for (i = 0; i < (mem_to_dump / num_col); i++) {
		for (j = 0; j < num_col; j++) {
			printf("0x%04x\t", rpt_words[i * num_col + j]);
		}
		printf("\n");
	}
#endif /* CSIMON_PIO_CONSOLE_PRINT */
} // __wlc_csimon_rpt_print_console()

static INLINE void // Read CSI header parameters from ucode shared memory
__wlc_read_shm_csimon_hdr(wlc_info_t *wlc, wlc_csimon_hdr_t *csimon_hdr, uint8 idx)
{
	uint32 tsf_l, tsf_h, rssi0, rssi1;

	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(csimon_hdr);

	/* MAC Address */
	csimon_hdr->client_ea[0] = wlc_read_shm(wlc, (M_CSI_MACADDRL(wlc) + idx*16));
	csimon_hdr->client_ea[1] = wlc_read_shm(wlc, (M_CSI_MACADDRM(wlc) + idx*16));
	csimon_hdr->client_ea[2] = wlc_read_shm(wlc, (M_CSI_MACADDRH(wlc) + idx*16));

	/* Time stamp using TSF register values */
	tsf_l = (uint32)wlc_read_shm(wlc, (M_CSI_RXTSFL(wlc) + idx*16));
	tsf_h = (uint32)wlc_read_shm(wlc, (M_CSI_RXTSFML(wlc) + idx*16));
	csimon_hdr->report_ts = tsf_l | (tsf_h << 16);

	/* RSSI for up to 4 antennae */
	rssi0 = (uint32)wlc_read_shm(wlc, (M_CSI_RSSI0(wlc) + idx*16));
	rssi1 = (uint32)wlc_read_shm(wlc, (M_CSI_RSSI1(wlc) + idx*16));
	csimon_hdr->rssi[0] = rssi0;
	csimon_hdr->rssi[1] = rssi0 >> 8;
	csimon_hdr->rssi[2] = rssi1;
	csimon_hdr->rssi[3] = rssi1 >> 8;

	CSIMON_DEBUG("wl%d:EA low 0x%x middle 0x%x high 0x%x\n"
	              "\tTSF low 0x%x hi 0x%x timestamp %u\n"
	              "\trssi0 0x%x rssi1 0x%x RSSI[0] 0x%0x RSSI[1] 0x%0x RSSI[2]"
		      " 0x%0x RSSI[3] 0x%0x\n",
		      wlc->pub->unit, csimon_hdr->client_ea[0],
		      csimon_hdr->client_ea[1], csimon_hdr->client_ea[2],
		      tsf_l, tsf_h, csimon_hdr->report_ts,
		      rssi0, rssi1, csimon_hdr->rssi[0], csimon_hdr->rssi[1],
		      csimon_hdr->rssi[2], csimon_hdr->rssi[3]);
} // __wlc_read_shm_csimon_hdr()

static INLINE int // Build the CSIMON header
__wlc_csimon_hdr_build(wlc_info_t * wlc, struct scb *scb,
                     wlc_csimon_hdr_t *csimon_hdr, uint16 *seqn)
{
	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(scb);
	CSIMON_ASSERT(csimon_hdr);
	CSIMON_ASSERT(seqn);

	/* First set the CSI report format id/version. This should be a supporting
	 * MAC/PHY.
	 */
	if (D11REV_GE(wlc->pub->corerev, 129)) {
		((wlc_csimon_hdr_t *)csimon_hdr)->format_id = CSI_REPORT_FORMAT_ID;
	} else {
		WL_ERROR(("wl%d: CSIMON not supported on this radio chip\n",
				wlc->pub->unit));
		return BCME_EPERM;
	}
	CSIMON_DEBUG("D11 core rev %d \n", wlc->pub->corerev);
	CSIMON_DEBUG("seq no loc 0x%x\n", M_CSI_VSEQN(wlc));

	/* Get shmem params for CSI header as a part of overall CSI record */

	/* Get the sequence number that indicates if the CSI record is valid */
	*seqn = wlc_read_shm(wlc, (M_CSI_VSEQN(wlc) + CSIxfer(wlc).svmp_rd_idx*16));

	/* Now read the header parameters from SHM */
	if (*seqn & (1 << C_CSI_VLD_NBIT)) {
		__wlc_read_shm_csimon_hdr(wlc, csimon_hdr, CSIxfer(wlc).svmp_rd_idx);
		CSIMON_DEBUG("wl%d:***************svmp_rd_idx %u\n",
				wlc->pub->unit, CSIxfer(wlc).svmp_rd_idx);
	} else {
		CSIMON_DEBUG("wl%d:valid bit NOT SET; try next record\n",
				wlc->pub->unit);
		CSIxfer(wlc).svmp_rd_idx =
			(CSIxfer(wlc).svmp_rd_idx + 1) % CSIMON_SVMP_RING_DEPTH;
		*seqn = wlc_read_shm(wlc, (M_CSI_VSEQN(wlc) +
		                     CSIxfer(wlc).svmp_rd_idx*16));
		if (*seqn & (1 << C_CSI_VLD_NBIT)) {
			__wlc_read_shm_csimon_hdr(wlc, csimon_hdr,
			                          CSIxfer(wlc).svmp_rd_idx);
			CSIMON_DEBUG("wl%d:==================svmp_rd_idx %u\n",
					wlc->pub->unit, CSIxfer(wlc).svmp_rd_idx);
			CSIxfer(wlc).svmp_rd_idx = (CSIxfer(wlc).svmp_rd_idx + 1) %
				CSIMON_SVMP_RING_DEPTH;
		} else {
			/* Increment a failure cnt as both the CSI records are invalid */
			scb->csimon->rpt_invalid_cnt++;
			WL_ERROR(("wl%d: Both SVMP CSI records invalid: idx %u\n",
					wlc->pub->unit, CSIxfer(wlc).svmp_rd_idx));
			return BCME_NOTREADY;
		}
	}
	/* Number of tx rx streams, chanspec with channel, bandwidth */
	csimon_hdr->txstreams = wlc->stf->op_txstreams;
	csimon_hdr->rxstreams = wlc->stf->op_rxstreams;
	csimon_hdr->chanspec = scb->bsscfg->current_bss->chanspec;

	/* BSSID and association timestamp */
	csimon_hdr->bss_ea[0] = scb->bsscfg->BSSID.octet[0] |
	                        scb->bsscfg->BSSID.octet[1] << 8;
	csimon_hdr->bss_ea[1] = scb->bsscfg->BSSID.octet[2] |
	                        scb->bsscfg->BSSID.octet[3] << 8;
	csimon_hdr->bss_ea[2] = scb->bsscfg->BSSID.octet[4] |
	                        scb->bsscfg->BSSID.octet[5] << 8;

	csimon_hdr->assoc_ts = scb->csimon->assoc_ts;

	CSIMON_DEBUG("wl%d: Format id %u Tx streams 0x%x Rx streams 0x%x chanspec "
				 "0x%0x\n", wlc->pub->unit, csimon_hdr->format_id,
				 csimon_hdr->txstreams, csimon_hdr->rxstreams,
				 csimon_hdr->chanspec);
	CSIMON_DEBUG("wl%d: Assoc TS %u BSS[0] 0x%x BSS[1] 0x%x BSS[2] 0x%x\n",
			wlc->pub->unit, csimon_hdr->assoc_ts, csimon_hdr->bss_ea[0],
			csimon_hdr->bss_ea[1], csimon_hdr->bss_ea[2]);

	return BCME_OK;
}

int // Copy to host STA CSI record: header from sysmem + report from SVMP mem
wlc_csimon_record_copy(wlc_info_t *wlc, scb_t *scb)
{
	uint32 svmp_offset;		/* Offset of csi_rpt0 or csi_rpt1 in SVMP memory */
	uint32 *svmp_addrp_u32;   /* Actual address of the csi_rpt in SVMP memory */
	uint16 seqn = 0;		/* Seq no field from SHM - managed by ucode */
	int local_wr_idx;
	int ret = BCME_OK;
	wlc_csimon_hdr_t *csimon_hdr;
	void *csimon_unit;
#if !defined(DONGLEBUILD)
	wlc_csimon_rec_t *csimon_rec;
#endif // endif

	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(scb);

	BCM_REFERENCE(svmp_addrp_u32);
	BCM_REFERENCE(local_wr_idx);
	BCM_REFERENCE(csimon_hdr);
#ifdef CSIMON_PIO_CONSOLE_PRINT
	/* Print the CSI report to console - use for debugging. */
	svmp_offset = VASIP_SHARED_OFFSET(wlc->hw, csi_rpt0);
	__wlc_csimon_rpt_print_console(wlc, NULL, svmp_offset, seqn,
	                               CSIxfer(wlc).svmp_rd_idx);
	return ret;
#endif /* CSIMON_PIO_CONSOLE_PRINT */

	if ((bcm_ring_prod_pend(&CSIxfer(wlc).local_ring, &local_wr_idx,
	                        CSIMON_LOCAL_RING_DEPTH)) == BCM_RING_FULL) {
		CSIxfer(wlc).drops++;
		return BCME_NORESOURCE;
	}
#if defined(DONGLEBUILD)
	csimon_hdr = CSIxfer(wlc).local_ring_base + local_wr_idx;
	csimon_unit = csimon_hdr;
#else /* ! DONGLEBUILD */
	csimon_rec = CSIxfer(wlc).local_ring_base + local_wr_idx;
	csimon_hdr = &csimon_rec->csimon_hdr;
	csimon_unit = csimon_rec;
#endif /* ! DONGLEBUILD */
	ret = __wlc_csimon_hdr_build(wlc, scb, csimon_hdr, &seqn);
	if (ret != BCME_OK) {
		return ret;
	}

	/* Transfer the whole CSI record that includes CSI header and CSI report
	 * (H-matrix). The CSI header consists of MAC addr, RSSI etc.
	 */

	/* Source address of the CSI report in SVMP memory */
	svmp_offset = VASIP_SHARED_OFFSET(wlc->hw, csi_rpt0);
	if (CSIxfer(wlc).svmp_rd_idx == 1) {
		svmp_offset = VASIP_SHARED_OFFSET(wlc->hw, csi_rpt1);
	}

	svmp_addrp_u32 = wlc_vasip_addr_int(wlc->hw, svmp_offset);

	/* Fill in the callback data that is needed after M2M xfer is done */
	scb->csimon->m2m_cb_data.scb = scb;
	scb->csimon->m2m_cb_data.seqn = seqn;
	scb->csimon->m2m_cb_data.svmp_rpt_idx = CSIxfer(wlc).svmp_rd_idx;

	ret = __wlc_csimon_xfer(wlc, scb, csimon_unit, CSIMON_RING_ITEM_SIZE,
	                      svmp_addrp_u32, (void*)(&(scb->csimon->m2m_cb_data)));
	if (ret == BCME_OK) {
		/* commit the local ring write index, now */
		bcm_ring_prod_done(&CSIxfer(wlc).local_ring, local_wr_idx);
		scb->csimon->m2mcpy_busy = TRUE;
	}
	CSIxfer(wlc).svmp_rd_idx = (CSIxfer(wlc).svmp_rd_idx + 1) %
	                           CSIMON_SVMP_RING_DEPTH;

	return ret;
}   // wlc_csimon_record_copy()

void // Handle Null frame ack failure
wlc_csimon_ack_failure_process(wlc_info_t *wlc, scb_t *scb)
{
	CSIMON_ASSERT(wlc);
	CSIMON_ASSERT(wlc->csimon_info);
	CSIMON_ASSERT(scb);
	CSIMON_ASSERT(scb->csimon);

	/* Increment the client level and CSIMON level failure counters */
	scb->csimon->ack_fail_cnt++;
	wlc->csimon_info->state.ack_fail_cnt++;

	WL_ERROR(("%s: csimon ack failure; %u for "MACF" \n", __FUNCTION__,
	          scb->csimon->ack_fail_cnt, ETHER_TO_MACF(scb->ea)));

} // wlc_csimon_ack_failure_process()

#if !defined(DONGLEBUILD)
//#define CSIMON_M2M_CONSOLE_PRINT
static INLINE void // Print the 'm2m xfer'ed CSI report' and header to console
__wlc_csimon_rec_console_print(wlc_info_t *wlc, wlc_csimon_rec_t *csimon_rec)
{
#ifdef CSIMON_M2M_CONSOLE_PRINT
	int i, j;
	uint num_col = 8, mem_to_dump = 64;
	uint32 *rpt_words = (uint32 *)csimon_rec;

	/* Dump the record */
	printf("CSI record at %p\n", csimon_rec);
	for (i = 0; i < (mem_to_dump / num_col); i++) {
		for (j = 0; j < num_col; j++) {
			printf("0x%08x\t", rpt_words[i * num_col + j]);
		}
		printf("\n");
	}
#endif /* CSIMON_M2M_CONSOLE_PRINT */
} // __wlc_csimon_rec_console_print()
#endif /* ! DONGLEBUILD */

void // function callback at the completion of the DD-based M2M xfer
wlc_csimon_m2m_dd_done_cb(void *usr_cbdata,
    dma64addr_t *xfer_src, dma64addr_t *xfer_dst, int xfer_len, void *xfer_arg)
{
	csimon_xfer_t *csimon = (csimon_xfer_t *)usr_cbdata;
	csimon_m2m_cb_data_t *m2m_cb_data;
	wlc_info_t *wlc;
	uint16 seqn, svmp_rpt_idx;

	BCM_REFERENCE(csimon);
	m2m_cb_data = (csimon_m2m_cb_data_t *)(xfer_arg);
	CSIMON_ASSERT(m2m_cb_data->scb);
	CSIMON_ASSERT(m2m_cb_data->scb->bsscfg);
	wlc = m2m_cb_data->scb->bsscfg->wlc;
	CSIMON_ASSERT(wlc);
	seqn = m2m_cb_data->seqn;
	svmp_rpt_idx = m2m_cb_data->svmp_rpt_idx;

#if defined(DONGLEBUILD)
	/* Post the updated write index given that the xfer is successful */
	pciedev_sbcopy(CSIxfer(wlc).wr_addr_u64,
		(uintptr)&CSIxfer(wlc).host_ring.write, 1, FALSE);

	/* CSI header ring item is consumed */
	bcm_ring_cons(&CSIxfer(wlc).local_ring, CSIMON_LOCAL_RING_DEPTH);
#endif /* DONGLEBUILD */

	m2m_cb_data->scb->csimon->m2mcpy_busy = FALSE;
	m2m_cb_data->scb->csimon->m2mxfer_cnt++;
	wlc->csimon_info->state.m2mxfer_cnt++;
	CSIMON_DEBUG("wl%d:xfer cnt %u\n", wlc->pub->unit,
			m2m_cb_data->scb->csimon->m2mxfer_cnt);

	/* Indicate to the ucode that the CSI report has been copied */
	__wlc_csimon_shm_update(wlc, seqn, svmp_rpt_idx);

#if !defined(DONGLEBUILD) && defined(CSIMON_M2M_CONSOLE_PRINT)
	/* Print CSI report (partially) to the console for quick verification */
	int elem_idx;
	wlc_csimon_rec_t *elem;

	elem_idx = bcm_ring_cons(&CSIxfer(wlc).local_ring, CSIMON_LOCAL_RING_DEPTH);
	elem = CSIMON_RING_IDX2ELEM(CSIxfer(wlc).local_ring_base, elem_idx);
	OSL_CACHE_INV((void*)(elem), CSIMON_RING_ITEM_SIZE);
	__wlc_csimon_rec_console_print(wlc, elem);

	/* Transfer over netlink socket */
	if (wlc_csimon_netlink_send(wlc->csimon_info, (void *) elem,
		CSIMON_RING_ITEM_SIZE)) {
		wlc->csimon_info->state.usr_xfer_fail_cnt++;
		WL_ERROR(("%s: CSIMON drops with nl send %d\n", __FUNCTION__,
		          wlc->csimon_info->state.usr_xfer_fail_cnt));
	}
#endif /* ! DONGLEBUILD && CSIMON_M2M_CONSOLE_PRINT */

} // wlc_csimon_m2m_dd_done_cb()

//-------------- IOVAR and client/station list management --------------------//

static INLINE int8 // Return the idx of the STA with given MAC address in the MAC list
wlc_csimon_sta_find(wlc_csimon_info_t *csimon_ctxt, const struct ether_addr *ea)
{
	int i;

	CSIMON_ASSERT(csimon_ctxt);
	CSIMON_ASSERT(ea);

	for (i = 0; i < CSIMON_MAX_STA; i++) {
		if (bcmp(ea, &csimon_ctxt->sta_info[i].ea, ETHER_ADDR_LEN) == 0)
			return i;
	}

	return -1;
}

static int // Start CSI monitoring for all the stations in the list
wlc_csimon_enable_all_stations(wlc_csimon_info_t *ctxt)
{
	int8 idx;
	uint32 tsf_l;
	scb_t *scb;

	CSIMON_ASSERT(ctxt);

	CSIMON_DEBUG("wl%d: Enabling all %d stations\n", ctxt->wlc->pub->unit,
	             ctxt->num_clients);

	/* Start CSIMON timer for the STAs in list */
	for (idx = 0; idx < CSIMON_MAX_STA; idx++) {
		if (bcmp(&ether_null, &ctxt->sta_info[idx].ea, ETHER_ADDR_LEN) == 0) {
			continue;
		}

		scb = wlc_scbapfind(ctxt->wlc, &ctxt->sta_info[idx].ea);
		if (scb && SCB_ASSOCIATED(scb)) {
			scb->csimon = &ctxt->sta_info[idx];
			/* Initialize the per-STA timer */
			ctxt->sta_info[idx].timer = wl_init_timer(ctxt->wlc->wl,
			                               wlc_csimon_scb_timer, scb, "csimon");
			if (!(ctxt->sta_info[idx].timer)) {
				WL_ERROR(("wl%d: csimon timer init failed for "MACF"\n",
				  ctxt->wlc->pub->unit, ETHER_TO_MACF(ctxt->sta_info[idx].ea)));
				return BCME_NORESOURCE;
			}
			/* CSI Monitoring start timestamp as a reference - TSF reg */
			wlc_read_tsf(ctxt->wlc, &tsf_l, NULL);
			ctxt->sta_info[idx].assoc_ts = tsf_l;
			/* Start the per-STA timer */
			wl_add_timer(ctxt->wlc->wl, ctxt->sta_info[idx].timer,
			             ctxt->sta_info[idx].timeout, TRUE);
			CSIMON_DEBUG("wl%d: started CSI timer for SCB DA "MACF" assocTS %u\n",
				ctxt->wlc->pub->unit, ETHER_TO_MACF(ctxt->sta_info[idx].ea),
			    tsf_l);
		}
	}
	return BCME_OK;
}

static void // Stop CSI monitoring all the stations in the list
wlc_csimon_disable_all_stations(wlc_csimon_info_t *ctxt)
{
	int8 idx;

	CSIMON_ASSERT(ctxt);

	CSIMON_DEBUG("wl%d: Disabling all %d stations\n", ctxt->wlc->pub->unit, ctxt->num_clients);
	for (idx = 0; idx < CSIMON_MAX_STA; idx++) {
		/* Stop/Free STA timer */
		if (ctxt->sta_info[idx].timer != NULL) {
			wl_del_timer(ctxt->wlc->wl, ctxt->sta_info[idx].timer);
			wl_free_timer(ctxt->wlc->wl, ctxt->sta_info[idx].timer);
			ctxt->sta_info[idx].timer = NULL;
		}
	}
}

static void // Clear all monitored stations from the list
wlc_csimon_delete_all_stations(wlc_csimon_info_t *ctxt)
{
	int8 idx;

	CSIMON_ASSERT(ctxt);

	CSIMON_DEBUG("wl%d: Deleting all %d stations\n", ctxt->wlc->pub->unit,
	             ctxt->num_clients);
	for (idx = 0; idx < CSIMON_MAX_STA; idx++) {
		if (ETHER_ISNULLADDR(&(ctxt->sta_info[idx].ea)))
			continue;
		/* Stop/Free STA timer */
		if (ctxt->sta_info[idx].timer != NULL) {
			wl_del_timer(ctxt->wlc->wl, ctxt->sta_info[idx].timer);
			wl_free_timer(ctxt->wlc->wl, ctxt->sta_info[idx].timer);
			ctxt->sta_info[idx].timer = NULL;
		}
		bcopy(&ether_null, &ctxt->sta_info[idx].ea, ETHER_ADDR_LEN);
		ctxt->num_clients--;
	}
}

static int // Copy configured MAC addresses to the output maclist
wlc_csimon_sta_maclist_get(wlc_csimon_info_t *ctxt, struct maclist *ml)
{
	uint i, j;

	if (ctxt == NULL)
		return BCME_UNSUPPORTED;

	CSIMON_ASSERT(ml != NULL);

	/* ml->count contains maximum number of MAC addresses ml can carry. */
	for (i = 0, j = 0; i < CSIMON_MAX_STA && j < ml->count; i++) {
		if (!ETHER_ISNULLADDR(&ctxt->sta_info[i].ea)) {
			bcopy(&ctxt->sta_info[i].ea, &ml->ea[j], ETHER_ADDR_LEN);
			j++;
		}
	}
	/* return the number of copied MACs to the maclist */
	ml->count = j;

	return BCME_OK;
}

static int // List the clients being monitored by CSIMON module with get iovar
wlc_csimon_process_get_cmd_options(wlc_csimon_info_t *ctxt, void* param,
	int paramlen, void* bptr, int len)
{
	int err = BCME_ERROR;
	struct maclist *maclist;

	maclist = (struct maclist *) bptr;

	if (len < (int)(sizeof(maclist->count) +
			(ctxt->num_clients * sizeof(*(maclist->ea))))) {
		return BCME_BUFTOOSHORT;
	}
	err = wlc_csimon_sta_maclist_get(ctxt, maclist);
	return err;
}

/*
 * Process commands passed via cfg->cmd parameter
 *
 * CSIMON_CFG_CMD_ENB command enables CSI Monitor feature at runtime
 *
 * CSIMON_CFG_CMD_DSB command disables CSI Monitor feature at runtime
 *
 * CSIMON_CFG_CMD_ADD command adds given MAC address of the STA into the STA
 * list and start capturing CSI if associated. The address must be valid unicast
 *
 * CSIMON_CFG_CMD_DEL command removes given MAC address of the STA from the STA
 * list and stops capturing CSI. The address must be valid unicast or broadcast.
 * The broadcast mac address specification clears all stations in the list.
 *
 * Return values:
 * BCME_UNSUPPORTED - feature is not supported
 * BCME_BADARG - invalid argument
 * BCME_NORESOURCE - no more entry in the STA list
 * BCME_ERROR - feature is supported but not enabled
 * BCME_NOTFOUND - entry not found
 * BCME_OK - success
 */
static int
wlc_csimon_sta_config(wlc_csimon_info_t *ctxt, wlc_csimon_sta_config_t *cfg)
{
	int8 idx;
	int ret;
	wlc_info_t *wlc;

	if (ctxt == NULL)
		return BCME_UNSUPPORTED;

	CSIMON_ASSERT(cfg);

	wlc = ctxt->wlc;

	CSIMON_DEBUG("wl%d stations %d \n", wlc->pub->unit, ctxt->num_clients);
	switch (cfg->cmd) {
	case CSIMON_CFG_CMD_DSB:
		if (CSIMON_ENAB(WLCPUB(ctxt))) {
			/* Disable all existing STAs including deleting timer */
			wlc_csimon_disable_all_stations(ctxt);
			/* Disable CSI Monitor feature */
			WLCPUB(ctxt)->_csimon = FALSE;

			CSIMON_DEBUG("wl%d %d stations cfg_cmd %d\n", wlc->pub->unit,
			              ctxt->num_clients, cfg->cmd);
		}
		break;
	case CSIMON_CFG_CMD_ENB:
		/* CSIMON supported on D11 core rev 129 and above */
		if (!D11REV_GE(wlc->pub->corerev, 129)) {
			WL_ERROR(("wl%d: CSI Monitor not supported on this radio chip! \n",
				WLCUNIT(ctxt)));
			return BCME_EPERM;
		}
		if (!CSIMON_ENAB(WLCPUB(ctxt))) {
			/* Enable CSI Monitor feature */
			WLCPUB(ctxt)->_csimon = TRUE;
			/* Enable all existing STAs including starting timer */
			if ((ret = wlc_csimon_enable_all_stations(ctxt)) != BCME_OK)
				return ret;
			CSIMON_DEBUG("wl%d stations %d cfg_cmd %d\n", wlc->pub->unit,
			             ctxt->num_clients, cfg->cmd);
#if !defined(DONGLEBUILD) && defined(CSIMON_WD_BASED_USR_XFER)
			wl_add_timer(wlc->wl, ctxt->wd_timer, ctxt->wd_timeout, TRUE);
			CSIMON_DEBUG("wl%d added tmr %d ms\n", wlc->pub->unit,
			             ctxt->wd_timeout);
#endif /* ! DONGLEBUILD */

		}
		break;
	case CSIMON_CFG_CMD_ADD:
		/* CSIMON supported on D11 core rev 129 and above */
		if (!D11REV_GE(wlc->pub->corerev, 129)) {
			WL_ERROR(("wl%d: CSI Monitor not supported on this radio chip! \n",
				WLCUNIT(ctxt)));
			return BCME_EPERM;
		}
		/* The MAC address must be a valid unicast address */
		if (ETHER_ISNULLADDR(&(cfg->ea)) || ETHER_ISMULTI(&(cfg->ea))) {
			WL_ERROR(("wl%d: %s: Invalid MAC address.\n",
				WLCUNIT(ctxt), __FUNCTION__));
			return BCME_BADARG;
		}
		/* Search existing entry in the list */
		idx = wlc_csimon_sta_find(ctxt, &cfg->ea);
		if (idx < 0) {
			/* Search free entry in the list */
			idx = wlc_csimon_sta_find(ctxt, &ether_null);
			if (idx < 0) {
				WL_ERROR(("wl%d:%s: CSIMON MAC list is full (%u clients) and "
				          "can't add ["MACF"] \n", WLCUNIT(ctxt), __FUNCTION__,
				          CSIMON_MAX_STA, ETHERP_TO_MACF(&cfg->ea)));
				return BCME_NORESOURCE;
			}
			/* Add MAC address to the list */
			bcopy(&cfg->ea, &ctxt->sta_info[idx].ea, ETHER_ADDR_LEN);
			ctxt->num_clients++;
			CSIMON_DEBUG("Added " MACF " to the list at idx %d; clients %d \n",
			             ETHERP_TO_MACF(&cfg->ea), idx, ctxt->num_clients);
		}

		/* Set/Update monitor interval */
		if (cfg->monitor_interval) {
			CSIMON_DEBUG("wl%d prev_int %d upd_int %d\n", wlc->pub->unit,
			             ctxt->sta_info[idx].timeout, cfg->monitor_interval);
			ctxt->sta_info[idx].timeout = cfg->monitor_interval;
		} else {
			ctxt->sta_info[idx].timeout = CSIMON_DEFAULT_TIMEOUT_MSEC;
		}

		/* Start sending the null frames */
		if (CSIMON_ENAB(WLCPUB(ctxt))) {
			scb_t *scb;
			uint32 tsf_l;

			scb = wlc_scbapfind(wlc, &ctxt->sta_info[idx].ea);
			CSIMON_DEBUG("scb %p idx %d\n", scb, idx);
			if (scb && SCB_ASSOCIATED(scb)) {
				scb->csimon = &ctxt->sta_info[idx];
				/* Free up any existing timer */
				if (ctxt->sta_info[idx].timer) {
					wl_del_timer(wlc->wl, ctxt->sta_info[idx].timer);
					wl_free_timer(wlc->wl, ctxt->sta_info[idx].timer);
					ctxt->sta_info[idx].timer = NULL;
					CSIMON_DEBUG("Freed timer %p\n", ctxt->sta_info[idx].timer);
				}
				/* Initialize per-STA timer */
				ctxt->sta_info[idx].timer = wl_init_timer(wlc->wl,
				                           wlc_csimon_scb_timer, scb, "csimon");
				if (!(ctxt->sta_info[idx].timer)) {
					WL_ERROR(("wl%d: csimon timer init failed for "MACF"\n",
					  wlc->pub->unit, ETHER_TO_MACF(cfg->ea)));
					return BCME_NORESOURCE;
				}
				CSIMON_DEBUG("Init timer %p\n", ctxt->sta_info[idx].timer);
				/* CSI Monitoring start timestamp as a reference - TSF reg */
				wlc_read_tsf(wlc, &tsf_l, NULL);
				ctxt->sta_info[idx].assoc_ts = tsf_l;
				/* Start per-STA timer */
				wl_add_timer(wlc->wl, ctxt->sta_info[idx].timer,
				             ctxt->sta_info[idx].timeout, TRUE);
				CSIMON_DEBUG("wl%d: started CSI timer for SCB DA "MACF
				             " assocTS %u\n", wlc->pub->unit,
				             ETHER_TO_MACF(cfg->ea), tsf_l);
			}
		}
		CSIMON_DEBUG("wl%d stations %d cfg_cmd %d\n", wlc->pub->unit,
		             ctxt->num_clients, cfg->cmd);
		break;
	case CSIMON_CFG_CMD_DEL:
		if (!CSIMON_ENAB(WLCPUB(ctxt)) && !(ctxt->num_clients)) {
			WL_ERROR(("wl%d: %s: Feature is not enabled\n",
			         WLCUNIT(ctxt), __FUNCTION__));
			return BCME_ERROR;
		}
		/* Broadcast mac address indicates to clear all stations in the list */
		if (ETHER_ISBCAST(&(cfg->ea))) {
			wlc_csimon_delete_all_stations(ctxt);
		} else {
			/* Search specified MAC address in the list */
			idx = wlc_csimon_sta_find(ctxt, &cfg->ea);
			if (idx < 0) {
				WL_ERROR(("wl%d: %s: Entry not found\n",
					WLCUNIT(ctxt), __FUNCTION__));
				return BCME_NOTFOUND;
			}

			/* Stop sending null frames to this station */
			if (ctxt->sta_info[idx].timer != NULL) {
				wl_del_timer(wlc->wl, ctxt->sta_info[idx].timer);
				wl_free_timer(wlc->wl, ctxt->sta_info[idx].timer);
				ctxt->sta_info[idx].timer = NULL;
				CSIMON_DEBUG("wl%d: freed CSI timer for SCB DA "MACF"\n",
				             wlc->pub->unit, ETHER_TO_MACF(cfg->ea));
			}
			wlc_csimon_sta_stats_clr(ctxt, idx);
			bcopy(&ether_null, &ctxt->sta_info[idx].ea, ETHER_ADDR_LEN);
			ctxt->num_clients--;
			CSIMON_DEBUG("wl%d stations %d cfg_cmd %d tmr %p\n", wlc->pub->unit,
			            ctxt->num_clients, cfg->cmd, ctxt->sta_info[idx].timer);
		}
		break;
	case CSIMON_CFG_CMD_RSTCNT:
		ctxt->state.null_frm_cnt = 0;
		ctxt->state.ack_fail_cnt = 0;
		ctxt->state.m2mxfer_cnt = 0;
		ctxt->state.usrxfer_cnt = 0;
		ctxt->state.rec_ovfl_cnt = 0;
		ctxt->state.xfer_fail_cnt = 0;
		ctxt->state.usr_xfer_fail_cnt = 0;
		break;
	default:
		return BCME_BADARG;
	}
	return BCME_OK;
}

static int // CSIMON IOVAR
wlc_csimon_doiovar(
	void                *hdl,
	uint32              actionid,
	void                *p,
	uint                plen,
	void                *a,
	uint                 alen,
	uint                 vsize,
	struct wlc_if       *wlcif)
{
	wlc_csimon_info_t	*csimon_ctxt = hdl;
	int			err = BCME_OK;

	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlcif);

	switch (actionid) {
	case IOV_GVAL(IOV_CSIMON):
		/* Process csimon command */
		err = wlc_csimon_process_get_cmd_options(csimon_ctxt, p, plen,
		                                         a, alen);
		break;
	case IOV_SVAL(IOV_CSIMON):
		{
			wlc_csimon_sta_config_t *csimon_cfg = (wlc_csimon_sta_config_t *)a;

			if (csimon_cfg->version != CSIMON_STACONFIG_VER) {
				return BCME_VERSION;
			}
			if ((alen >= CSIMON_STACONFIG_LENGTH) &&
				(csimon_cfg->length >= CSIMON_STACONFIG_LENGTH)) {
				/* Set CSIMON or STA parameter */
				err = wlc_csimon_sta_config(csimon_ctxt, csimon_cfg);
			} else {
				return BCME_BUFTOOSHORT;
			}
			break;
		}
	case IOV_GVAL(IOV_CSIMON_STATE):
		{
			csimon_state_t *state = (csimon_state_t *)a;
			csimon_ctxt->state.version = CSIMON_CNTR_VER;
			csimon_ctxt->state.length = sizeof(csimon_ctxt->state);
			if (alen < csimon_ctxt->state.length)
				return BCME_BUFTOOSHORT;
			*state = csimon_ctxt->state;
			state->enabled = CSIMON_ENAB(csimon_ctxt->wlc->pub);
			break;
		}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
} // wlc_csimon_doiovar()

char * // Construct and return name of M2M IRQ
wlc_csimon_irqname(wlc_info_t *wlc, void *btparam)
{
	wlc_csimon_info_t *wlc_csimon = wlc->csimon_info;
	BCM_REFERENCE(btparam);

#if defined(CONFIG_BCM_WLAN_DPDCTL)
	if (btparam != NULL) {
		/* bustype = PCI, even embedded 2x2AX devices have virtual pci underneath */
		snprintf(wlc_csimon->irqname, sizeof(wlc_csimon->irqname),
			"wlpcie:%s, wlan_%d_m2m", pci_name(btparam), wlc->pub->unit);
	} else
#endif /* CONFIG_BCM_WLAN_DPDCTL */
	{
		snprintf(wlc_csimon->irqname, sizeof(wlc_csimon->irqname), "wlan_%d_m2m",
			wlc->pub->unit);
	}

	return wlc_csimon->irqname;
}

#if !defined(DONGLEBUILD)
bool BCMFASTPATH // ISR for M2M interrupts
wlc_csimon_isr(wlc_info_t *wlc, bool *wantdpc)
{
	void *cbdata = (void *)m2m_dd_eng_get(wlc->osh,
	   CSIxfer(wlc).m2m_dd_csi);
	return m2m_dd_isr(cbdata, wantdpc);
}

void BCMFASTPATH // DPC for M2M interrupts
wlc_csimon_dpc(wlc_info_t *wlc)
{
	void *cbdata = (void *)m2m_dd_eng_get(wlc->osh,
	   CSIxfer(wlc).m2m_dd_csi);
	m2m_dd_worklet(cbdata);
}

uint // M2M core bitmap in oobselouta30 reg
wlc_csimon_m2m_si_flag(wlc_info_t *wlc)
{
	return m2m_dd_si_flag_get(wlc->osh);
}

void // Disable M2M interrupts
wlc_csimon_intrsoff(wlc_info_t *wlc)
{
	void *m2m_eng;

	CSIMON_ASSERT(wlc);

	/* Get the M2MDMA engine pointer */
	m2m_eng = (void *)m2m_dd_eng_get(wlc->osh,
	   CSIxfer(wlc).m2m_dd_csi);
	CSIMON_ASSERT(m2m_eng);

	/* Disable channel-based M2M interrupts for CSI */
	m2m_intrsoff(wlc->osh, m2m_eng);

	CSIMON_DEBUG("Disabled interrupts for m2m eng %p m2m_usr %d\n", m2m_eng,
	             CSIxfer(wlc).m2m_dd_csi);

} /* wlc_sts_xfer_intrsoff() */

#endif /* ! DONGLEBUILD */
