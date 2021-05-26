/*
 * PCIEDEV device API
 * pciedev is the bus device for pcie full dongle
 * Core bus operations are as below
 * 1. interrupt mechanism
 * 2. circular buffer
 * 3. message buffer protocol
 * 4. DMA handling
 * pciedev.h exposes pciedev functins required outside
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
 * $Id: pciedev.h  $
 */

#ifndef	_pciedev_h_
#define	_pciedev_h_

#include <typedefs.h>
#include <osl.h>
#include <pciedev_priv.h>
struct pcidev;

#ifdef PCIE_PHANTOM_DEV
struct pcie_phtm;
struct dngl_bus;
#endif /* PCIE_PHANTOM_DEV */

/* Chip operations */
extern struct dngl_bus *pciedev_attach(void *drv, uint vendor, uint device, osl_t *osh,
                                      void *regs, uint bus);

extern void *pciedev_dngl(struct dngl_bus *pciedev);
int pciedev_bus_binddev(void *bus, void *dev);
extern void pciedev_init(struct dngl_bus *pciedev);
extern void pciedev_intrsoff(struct dngl_bus *pciedev);
extern bool pciedev_dispatch(struct dngl_bus *pciedev);
extern void pciedev_intrson(struct dngl_bus *pciedev);

#define DEV_DMA_IOCT_PYLD 	0x1
#define DEV_DMA_IOCT_CMPLT 	0x2
#define DEV_DMA_TX_CMPLT	0x4
#define DEV_DMA_RX_CMPLT	0x8
#define	DEV_DMA_RX_PYLD		0x10
#define DEV_DMA_WL_EVENT	0x20
#define DEV_DMA_LOOPBACK_REQ	0x40
#define PCIE_ERROR1			1
#define PCIE_ERROR2			2
#define PCIE_ERROR3			3
#define PCIE_ERROR4			4
#define PCIE_ERROR5			5
#define PCIE_ERROR6			6
#define PCIE_WARNING			7

#define IOCTL_PKTBUFSZ          2048

enum {
	HTOD = 0,
	DTOH
};
enum {
	RXDESC = 0,
	TXDESC
};

#ifdef PCIE_PHANTOM_DEV
extern	struct pcie_phtm * pcie_phtm_attach(struct dngl_bus *pciedev, osl_t *osh, si_t *sih,
	void *regs, pciedev_shared_t *pcie_sh, uint32 rxoffset);
extern void pcie_phtm_init(struct pcie_phtm *phtm);
extern void pcie_phtm_bit0_intr_process(struct pcie_phtm *phtm);
extern void pcie_phtm_msgbuf_dma(struct pcie_phtm *phtm, void *dst,
	dma64addr_t src, uint16 src_len);
extern void pcie_phtm_reset(struct pcie_phtm *phtm);
extern void pcie_phtm_dma_sts_update(struct pcie_phtm *phtm, uint32 status);
extern void pcie_phtm_tx(struct pcie_phtm *phtm, void *p,
	dma64addr_t addr, uint16 msglen, uint8 msgtype);
extern void phtm_ring_dtoh_doorbell(struct pcie_phtm *phtm);
extern void pcie_phtm_sd_isr(struct dngl_bus *pciedev);
extern void pcie_phtm_usb_isr(struct dngl_bus *pciedev);
#endif /* PCIE_PHANTOM_DEV */
extern int pciedev_dispatch_fetch_rqst(struct fetch_rqst *fr, void *arg);
extern void pciedev_process_tx_payload(struct dngl_bus *pciedev);
extern circularbuf_t *pciedev_htodlcl(struct dngl_bus *pciedev);
extern circularbuf_t *pciedev_htodlcl_ctrl(struct dngl_bus *pciedev);
extern circularbuf_t *pciedev_htod_ctrlbuf(struct dngl_bus *pciedev);
extern circularbuf_t *pciedev_htodmsgbuf(struct dngl_bus *pciedev);
extern bool  pciedev_msgbuf_intr_process(struct dngl_bus *pciedev);
extern struct pcie_phtm *pciedev_phtmdev(struct dngl_bus *pciedev);
extern void pciedev_intrsupd(struct dngl_bus *pciedev);
extern bool pciedev_dpc(struct dngl_bus *pciedev);
extern void pciedev_detach(struct dngl_bus *pciedev);
void pciedev_bus_sendctl(struct dngl_bus *bus, void *p);
uint32 pciedev_bus_iovar(struct dngl_bus *pciedev, char *buf,
	uint32 inlen, uint32 *outlen, bool set);
int pciedev_bus_unbinddev(void *bus, void *dev);
extern int pciedev_bus_validatedev(void *bus, void *dev);
extern uint16 pciedev_sendup(struct dngl_bus *pciedev, void *p,
	uint16 pktlen, uint16 msglen, uint16 ring);
extern void pciedev_up_host_rptr(struct dngl_bus *pciedev, uint16 msglen);
extern void pciedev_up_lcl_rptr(struct dngl_bus *pciedev, uint16 msglen);
extern void pciedev_xmit_ioct_cmplt(struct dngl_bus *bus);
extern bool pciedev_handle_d2h_dmacomplete(struct dngl_bus *pciedev, void *buf);
extern void pciedev_set_h2dring_rxoffset(struct dngl_bus *pciedev, uint16 offset, bool ctrl);
extern int pciedev_create_d2h_messages_tx(struct dngl_bus *pciedev, void *p);
extern int pciedev_bus_flring_ctrl(void *bus, uint32 op, void *data);
extern void pciedev_upd_flr_hanlde_map(struct dngl_bus * pciedev, uint8 handle,
	bool add, uint8 *addr);
extern void pciedev_upd_flr_tid_state(struct dngl_bus * pciedev, uint8 tid_ac, bool open);
extern void pciedev_upd_flr_if_state(struct dngl_bus * pciedev, uint8 ifindex, bool open);
extern void pciedev_upd_flr_port_handle(struct dngl_bus * pciedev, uint8 handle, bool open);
extern void pciedev_process_reqst_packet(struct dngl_bus * pciedev,
	uint8 handle, uint8 ac_bitmap, uint8 count);
extern void pciedev_upd_flr_weigth(struct dngl_bus * pciedev, uint8 mac_handle,
	uint8 ac_tid, void *params);
extern void pciedev_set_ffsched(struct dngl_bus * pciedev, void *params);
extern bool pciedev_handle_h2d_dma(struct dngl_bus *pciedev);
extern void pciedev_flow_schedule_timerfn(dngl_timer_t *t);
extern void pciedev_flow_ageing_timerfn(dngl_timer_t *t);
extern int pciedev_read_flow_ring_host_buffer(struct dngl_bus *pciedev,
	msgbuf_ring_t *flow_ring, uint16 pciedev_fetch_pending);
extern void* pciedev_get_lclbuf_pool(msgbuf_ring_t * ring);
extern int pciedev_fillup_rxcplid(pktpool_t *pool, void* arg, void* frag, bool dummy);
extern int pciedev_fillup_haddr(pktpool_t *pool, void* arg, void* frag, bool rxcpl_needed);
extern void pciedev_flush_glommed_txrxstatus(dngl_timer_t *t);
extern void pciedev_queue_d2h_req_send_timer(dngl_timer_t *t);
extern void pciedev_queue_rxcomplete_local(struct dngl_bus *pciedev, rxcpl_info_t *rxcpl_info,
	msgbuf_ring_t *ring, bool flush);
extern uint8 pciedev_xmit_msgbuf_packet(struct dngl_bus *pciedev, void *p, uint16  msglen,
	msgbuf_ring_t *msgbuf);
extern bool pciedev_lbuf_callback(void *arg, void* p);
extern int pciedev_tx_pyld(struct dngl_bus *bus, void *p, ret_buf_t *ret_buf, uint16 msglen,
	ret_buf_t *metadata_buf, uint16 metadata_buf_len, uint8 msgtype);
extern uint16 pciedev_handle_h2d_msg_rxsubmit(struct dngl_bus *pciedev, void *p,
	uint16 pktlen, msgbuf_ring_t *msgbuf);
extern uint16 pciedev_handle_h2d_msg_txsubmit(struct dngl_bus *pciedev, void *p,
	uint16 pktlen, msgbuf_ring_t *msgbuf, uint16 fetch_indx);
extern bool pciedev_handle_d2h_dma(struct dngl_bus *pciedev);
extern void pciedev_queue_d2h_req_send(struct dngl_bus *pciedev);
extern int pciedev_process_d2h_wlevent(struct dngl_bus *pciedev, void* p);
#if defined(PCIE_D2H_DOORBELL_RINGER)
void pciedev_d2h_doorbell_ringer(struct dngl_bus *pciedev,
	uint32 db_data, uint32 db_addr);
#endif /* PCIE_D2H_DOORBELL_RINGER */
extern void pciedev_generate_host_db_intr(struct dngl_bus *pciedev, uint32 data, uint32 db);
extern int pciedev_init_sharedbufs(struct dngl_bus *pciedev);
extern void pciedev_send_flow_ring_flush_resp(struct dngl_bus * pciedev, uint16 flowid,
	uint32 status);
extern int pciedev_send_flow_ring_delete_resp(struct dngl_bus * pciedev,
	uint16 flowid, uint32 status);
extern void pciedev_process_ioctl_done(struct dngl_bus *pciedev);
extern void pciedev_host_wake_gpio_enable(struct dngl_bus *pciedev, bool state);
extern void pciedev_D3_ack(struct dngl_bus *pciedev);
extern void pciedev_process_ioctl_pend(struct dngl_bus *pciedev);
extern void pciedev_flush_chained_pkts(void *ctx);
extern void pciedev_dma_tx_account(struct dngl_bus *pciedev, uint16 msglen, uint8 msgtype,
	uint8 flags, msgbuf_ring_t *msgbuf);
extern void pciedev_process_pending_fetches(struct dngl_bus *pciedev);
extern void pciedev_queue_d2h_req(struct dngl_bus *pciedev, void *p);
#ifdef PCIE_DMAXFER_LOOPBACK
extern uint32 pciedev_process_do_dest_lpbk_dmaxfer_done(struct dngl_bus *pciedev, void *p);
extern int pciedev_process_d2h_dmaxfer_pyld(struct dngl_bus *pciedev, void *p);
#endif // endif
extern bool pciedev_init_flowrings(struct dngl_bus *pciedev);
#ifdef PCIE_DMA_INDEX
extern void pciedev_dma_set_indices(struct dngl_bus *pciedev, msgbuf_ring_t *msgbuf_ring);
extern void pciedev_dma_get_indices(struct dngl_bus *pciedev);
#endif /* PCIE_DMA_INDEX */
extern void pciedev_xmit_txstatus(struct dngl_bus *pciedev, msgbuf_ring_t *msgring);
extern void pciedev_xmit_rxcomplete(struct dngl_bus *pciedev, msgbuf_ring_t *ring);
extern void pciedev_reset_flowring_weight(struct dngl_bus *pciedev, msgbuf_ring_t *flow_ring);
extern void pciedev_reset_all_flowrings_weight(struct dngl_bus *pciedev);
extern uint32 pciedev_flow_ring_flush_pending(struct dngl_bus * pciedev, char* buf, uint32 inlen,
	uint32 *outlen);
#if defined(WL_MONITOR) && !defined(WL_MONITOR_DISABLED)
void pciedev_set_rxoffset_bytes(struct dngl_bus *pciedev, uint32 d11rxoffset);
void pciedev_set_monitor_mode(struct dngl_bus *pciedev, uint32 monitor_mode);
#endif /* WL_MONITOR && WL_MONITOR_DISABLED */
#endif	/* _pciedev_h_ */
