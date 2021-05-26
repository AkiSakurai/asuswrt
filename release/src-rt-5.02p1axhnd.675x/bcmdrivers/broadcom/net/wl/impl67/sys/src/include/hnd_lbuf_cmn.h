/*
 * HND generic packet buffer common definitions.
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: $
 */

#ifndef _hnd_lbuf_cmn_h_
#define _hnd_lbuf_cmn_h_

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define _PADLINE(line)   pad ## line
#define _XSTR(line)      _PADLINE(line)
#define PAD              _XSTR(__LINE__)
#endif  /* PAD */

/* Enough to fit a 1500 MTU plus overhead */
#ifndef MAXPKTDATABUFSZ
#define MAXPKTDATABUFSZ  1920
#endif // endif

/* Generic flag information of lbuf
 * private flags - don't reference directly
 */
#define LBF_PRI          0x00000007  /**< priority (low 3 bits of flags) */
#define LBF_SUM_NEEDED   0x00000008  /**< host->device */
#define LBF_ALT_INTF     0x00000008  /**< internal: indicate on alternate interface */
#define LBF_SUM_GOOD     0x00000010
#define LBF_MSGTRACE     0x00000020
#define LBF_CLONE        0x00000040
#define LBF_DOT3_PKT     0x00000080
#define LBF_80211_PKT    0x00000080  /**< Clash ? */
#define LBF_PTBLK        0x00000100  /**< came from fixed block in partition, not main malloc */
#define LBF_TX_FRAG      0x00000200
#define LBF_PKT_NODROP   0x00000400  /**< for PROP_TXSTATUS */
#define LBF_PKT_EVENT    0x00000800  /**< for PROP_TXSTATUS */
#define LBF_CHAINED      0x00001000
#define LBF_RX_FRAG      0x00002000
#define LBF_METADATA     0x00004000
#define LBF_NORXCPL      0x00008000
#define LBF_EXEMPT_MASK  0x00030000
#define LBF_EXEMPT_SHIFT 16
#define LBF_PKTFETCHED   0x00040000  /**< indicates pktfetched packet */
#define LBF_FRWD_PKT     0x00080000
#ifdef WL_MONITOR
#define LBF_MON_PKT      0x00100000
#endif // endif
#define LBF_HEAPBUF      0x00200000
/*                      0x00400000  available */
#define LBF_CFP_PKT      0x00800000  /**< Cache Flow Processing capable packet */
#define LBF_BUF_ALLOC    0x01000000  /**< indicates packet has external buffer allocated */
#define LBF_HWA_PKT      0x02000000  /**< Packet owned by HWA */
#define LBF_HWA_3BPKT    0x04000000  /**< Packet is HWA 3b SWPKT */
#define LBF_MGMT_TX_PKT  0x08000000  /**< indicates mgmt tx packet */
#define LBF_HWA_HOSTREORDER 0x10000000 /**< indicates packet has AMPDU_SET_HOST_HASPKT */
#define LBF_HME_DATA     0x20000000  /**< indicates data is point to HME */
#define LBF_HWA_DDBM_PKT 0x40000000  /**< indicates packet is from DDBM */
/*                      0x80000000  available */

/* Generic flag information of lfrag */
#define LB_FIFO0_INT     0x01
#define LB_FIFO1_INT     0x02
#define LB_HDR_CONVERTED 0x04
#define LB_TXS_PROCESSED 0x08 /**< Indicate lbuf is txs processed */
#define LB_TXTS_INSERTED 0x10 /**< Indicate that Tx Timestamp inserted */
#define LB_TXS_HOLD      0x20 /**< Indicate Tx status in the middle of AMPDU */
#define LB_RX_CORRUPTED  0x40

#define FC_TLV_SIZE      8

enum lbuf_type {
	lbuf_basic = 0,
	lbuf_frag,           /**< tx frag */
	lbuf_rxfrag,
	lbuf_mgmt_tx         /**< HWA mode: legacy mgmt pkt */
};

#endif /* _hnd_lbuf_cmn_h_ */
