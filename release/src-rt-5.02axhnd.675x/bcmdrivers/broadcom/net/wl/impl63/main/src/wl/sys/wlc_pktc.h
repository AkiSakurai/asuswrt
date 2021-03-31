/*
 * Packet chaining module header file
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
 * $Id: wlc_pktc.h 771056 2019-01-11 16:43:59Z $
 */

#ifndef __wlc_pktc_h__
#define __wlc_pktc_h__

#include <typedefs.h>
#include <wlc_types.h>
#include <ethernet.h>

#define PKTC_POLICY_SENDUPC	0	/* Default, send up chained */
#define PKTC_POLICY_SENDUPUC	1	/* Send up unchained */

/** Receive frames chaining info */
struct wlc_rfc {
	wlc_bsscfg_t		*bsscfg;
	struct scb		*scb;
	void			*wlif_dev;
	struct ether_addr	ds_ea;
	uint8			prio;
	uint8			ac;
	uint8			iv_len;
};

/** Module info */
/*
 * TODO: make this opaque and create functional APIs.
 */
struct wlc_pktc_info {
	wlc_info_t		*wlc;
	struct	wlc_rfc		rfc[2];		/**< receive chaining info */
	struct ether_addr	h_da;		/**< copy of da of head frame in chain */
	bool			h_da_valid;	/**< is h_da valid */
	uint8			policy;		/**< chaining policy */
	struct scb		*h_scb;		/* scb of last processed pkt */
};

/* Access rfc. Will change when modularized */
#define PKTC_RFC(_pktc_, _ap_)			&(_pktc_)->rfc[(_ap_) ? 1 : 0]
/* Access h_da. Will change when modularized */
#define PKTC_HDA(_pktc_)			(&(_pktc_)->h_da)
#define PKTC_HDA_VALID(_pktc_)			(_pktc_)->h_da_valid
#define PKTC_HSCB(_pktc_)			(_pktc_)->h_scb
#define PKTC_HDA_VALID_SET(_pktc_, _val_)	(_pktc_)->h_da_valid = (_val_)
#define PKTC_HSCB_SET(_pktc_, _val_)		(_pktc_)->h_scb = (_val_)

/* attach/detach */
wlc_pktc_info_t *wlc_pktc_attach(wlc_info_t *wlc);
void wlc_pktc_detach(wlc_pktc_info_t *pktc);

/*
 * TODO: follow the modular interface convention - pass module handle as the first
 * param in func's param list.
 */

void wlc_scb_pktc_enable(struct scb *scb, const wlc_key_info_t *key_info);
void wlc_scb_pktc_disable(wlc_pktc_info_t *pktc, struct scb *scb);
void wlc_pktc_sdu_prep(wlc_info_t *wlc, struct scb *scb, void *pkt, void *n, uint32 lifetime);

#endif /* __wlc_pktc_h__ */
