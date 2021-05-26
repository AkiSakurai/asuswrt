/*
 * MAC command request functionality
 * Broadcom 802.11abg Networking Device Driver
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
 * $Id: wlc_macreq.c 771365 2019-01-23 09:46:44Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <wlc_types.h>
#include <wlioctl.h>
#include <802.11.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_macreq.h>
#include <wlc_hw.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_mutx.h>
#include <wlc_txbf.h>
#include <wlc_ratelinkmem.h>

/** iovar table */
enum {
	IOV_MACCMD_REQ = 0,	/* mac command request */
	IOV_LAST
};

static const bcm_iovar_t macreq_iovars[] = {
	{"macreq", IOV_MACCMD_REQ, (IOVF_SET_CLK | IOVF_GET_CLK), 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

/* module info */
struct wlc_macreq_info {
	wlc_info_t *wlc;
};

static int wlc_macreq_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint vsize, struct wlc_if *wlcif);
static int wlc_macreq_doioctl(void *ctx, uint32 cmd, void *arg, uint len, struct wlc_if *wlcif);
static int wlc_macreq_up(void *hdl);
static int wlc_dump_macreq(wlc_info_t *wlc, struct bcmstrbuf *b);
#ifdef WL_PSMX
static int wlc_maccmd_req(wlc_info_t *wlc, wl_macreq_params_t * mreq, bool block);
static int _wlc_macreq_add_del_mtxv(wlc_info_t *wlc, wl_macreq_params_t *mreq, bool blocking);
static int _wlc_macreq_add_del_lmem(wlc_info_t *wlc, wl_macreq_params_t *mreq, bool blocking);
static int _wlc_macreq_ld_mutimer(wlc_info_t *wlc, wl_macreq_params_t *mreq, bool blocking);
#endif /* WL_PSMX */

// definition for MX_MACREQ_BLK
typedef enum
{
	C_MREQ_FLAG_POS = 0,
	C_MREQ_TYPE_POS = 1,
	C_MREQ_RSVD_POS = 2,
	C_MREQ_LEN_POS  = 3,
	C_MREQ_MSG_POS  = 4,
	C_MREQ_MSG_LB   = 19,
	C_MREQ_BLK_WSZ  = 20
} eMacReqBlkDefinitions;
#define MX_MREQ_FLAG(wlc)	(MX_MACREQ_BLK(wlc) + (C_MREQ_FLAG_POS * 2))
#define MX_MREQ_TYPE(wlc)	(MX_MACREQ_BLK(wlc) + (C_MREQ_TYPE_POS * 2))
#define MX_MREQ_LEN(wlc)	(MX_MACREQ_BLK(wlc) + (C_MREQ_LEN_POS * 2))
#define MX_MREQ_MSG(wlc)	(MX_MACREQ_BLK(wlc) + (C_MREQ_MSG_POS * 2))
#define MX_MREQ_LIST(wlc)	MX_MREQ_MSG(wlc) // alias

// definition for C_MREQ_FLAG_POS
typedef enum
{
	// keep the three together
	C_MRFLAG_VLD_NBIT    = 0,  // set by host
	C_MRFLAG_DONE_NBIT   = 1,  // done
	C_MRFLAG_PEND_NBIT   = 2,  // see request, WIP
	C_MRFLAG_CLRSTS_NBIT = 3,  // set by host, clear txbf stats
	// gap
	C_MRFLAG_PSMR_NBIT   = 4,  // informative: issued to psm-r
	C_MRFLAG_PSMX_NBIT   = 5,  // informative: issued to psm-x
} eMacReqFlagBitDefinitions;

// enum for C_MREQ_TYPE value
typedef enum
{
	C_MREQ_UPD_LMEM   = 0,  // lmem[] update
	C_MREQ_DEL_LMEM   = 1,  // del lmem[]
	C_MREQ_ADD_MTXV   = 2,  // add mtxv for lmem[]
	C_MREQ_DEL1_MTXV  = 3,  // del mtxv for lmem[]
	C_MREQ_DEL2_MTXV  = 4,  // del mtxv[]
	C_MREQ_DELA_MTXV  = 5,  // del all mtxv
	C_MREQ_LD_MTMR    = 6,  // load musnd periodic timer
	C_MREQ_ADD_LMEM   = 7,  // add lmem[]
} eMacReqTypeValDefinitions;

typedef enum
{
	C_MCMDX_SND_NBIT        = 0,    // re-do sounding
	C_MCMDX_CLR_MUBF_NBIT   = 1,    // clear MU BF state
	C_MCMDX_RSTV_NBIT       = 2,    // reset VASIP
	C_MCMDX_PSMWD_NBIT      = 7,    // cause psm wd manually, for debug purpose
					// set same as in psm_r
	C_MCMDX_REQ_NBIT        = 8,    // general request in tlv format
} ePsmXMacCmdRegBitDefinitions;

#define C_MREQ_SUPPORTED_TYPES \
		((1 << C_MREQ_ADD_MTXV) | (1 << C_MREQ_DEL1_MTXV) \
		| (1 << C_MREQ_DELA_MTXV) | (1 << C_MREQ_LD_MTMR) \
		| (1 << C_MREQ_UPD_LMEM) | (1 << C_MREQ_DEL_LMEM) \
		| (1 << C_MREQ_ADD_LMEM))

wlc_macreq_info_t *
BCMATTACHFN(wlc_macreq_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	wlc_macreq_info_t *macreq;

	if ((macreq = MALLOCZ(wlc->osh, sizeof(wlc_macreq_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: macreq memory alloc. failed\n",
			wlc->pub->unit, __FUNCTION__));
		return NULL;
	}
	macreq->wlc = wlc;

	if (wlc_module_add_ioctl_fn(pub, macreq, wlc_macreq_doioctl, 0, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if ((wlc_module_register(pub, macreq_iovars, "macreq",
		macreq, wlc_macreq_doiovar, NULL, wlc_macreq_up, NULL)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wlc_dump_register(pub, "macreq", (dump_fn_t)wlc_dump_macreq, (void *)wlc);

	return macreq;

fail:
	MODULE_DETACH(macreq, wlc_macreq_detach);
	return NULL;
} /* wlc_macreq_attach */

void
BCMATTACHFN(wlc_macreq_detach)(wlc_macreq_info_t *macreq)
{
	wlc_info_t *wlc;

	if (!macreq) {
		return;
	}
	wlc = macreq->wlc;

	wlc_module_unregister(wlc->pub, "macreq", macreq);

	(void)wlc_module_remove_ioctl_fn(wlc->pub, macreq);

	MFREE(wlc->osh, macreq, sizeof(*macreq));
}

static int
wlc_macreq_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint vsize, struct wlc_if *wlcif)
{
	wlc_macreq_info_t *macreq = (wlc_macreq_info_t*)hdl;
	wlc_info_t *wlc = macreq->wlc;
	int32 int_val = 0, *ret_int_ptr;
	int err = BCME_OK;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(wlcif);
	BCM_REFERENCE(int_val);
	BCM_REFERENCE(ret_int_ptr);

	ASSERT(macreq == wlc->macreq);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val)) {
		bcopy(params, &int_val, sizeof(int_val));
	}

	ret_int_ptr = (int32 *)arg;

	switch (actionid) {

	case IOV_SVAL(IOV_MACCMD_REQ): {
		wl_macreq_params_t *mreq = (wl_macreq_params_t *)params;

		if (!((1 << mreq->type) & (C_MREQ_SUPPORTED_TYPES))) {
			return BCME_UNSUPPORTED;
		}
#ifdef WL_PSMX
		err = wlc_maccmd_req(macreq->wlc, mreq, TRUE);
#endif // endif
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
} /* wlc_macreq_doiovar */

/* ioctl dispatcher */
static int
wlc_macreq_doioctl(void *ctx, uint32 cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	int bcmerror = BCME_OK;
	wlc_macreq_info_t *macreq = ctx;
	wlc_info_t *wlc = macreq->wlc;
	int val, *pval;
	d11regs_t *regs = wlc->regs;
	uint band = 0;
	osl_t *osh = wlc->osh;
	uint i;
	bool ta_ok = FALSE;

	BCM_REFERENCE(ta_ok);
	BCM_REFERENCE(macreq);
	BCM_REFERENCE(regs);
	BCM_REFERENCE(i);
	BCM_REFERENCE(band);
	BCM_REFERENCE(osh);

	/* default argument is generic integer */
	pval = (int *) arg;

	/* This will prevent the misaligned access */
	if ((uint32)len >= sizeof(val)) {
		bcopy(pval, &val, sizeof(val));
	} else {
		val = 0;
	}

	switch (cmd) {

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return bcmerror;
} /* wlc_macreq_doioctl */

static int
wlc_macreq_up(void *hdl)
{
	wlc_macreq_info_t *macreq = (wlc_macreq_info_t *)hdl;
	wlc_info_t *wlc = macreq->wlc;
	BCM_REFERENCE(wlc);

	return BCME_OK;
} /* wlc_macreq_up */

static int
wlc_dump_macreq(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int err = BCME_OK, i;
	char	*buf = NULL;
	struct  bcmstrbuf bstr;

	BCM_REFERENCE(i);
	if (!b) {
#define AMPDU_DUMP_LEN (4 * 1024)

		buf = MALLOCZ(wlc->osh, AMPDU_DUMP_LEN);
		bcm_binit(&bstr, buf, AMPDU_DUMP_LEN);
		b = &bstr;
	}

	if (!wlc->clk) {
		bcm_bprintf(b, "%s: clk off\n", __FUNCTION__);
		goto done;
	}

#ifdef WL_PSMX
	bcm_bprintf(b, "flag:%04x ", wlc_read_shmx(wlc, MX_MREQ_FLAG(wlc)));
	bcm_bprintf(b, "type:%04x ", wlc_read_shmx(wlc, MX_MREQ_TYPE(wlc)));
	bcm_bprintf(b, "len: %04x\n", wlc_read_shmx(wlc, MX_MREQ_LEN(wlc)));
	for (i = 0; i < 16; i++) {
		if (i == 0)
		bcm_bprintf(b, "msg: %04x ", wlc_read_shmx(wlc, MX_MREQ_LIST(wlc)+(i*2)));
		else
		bcm_bprintf(b, "%04x ", wlc_read_shmx(wlc, MX_MREQ_LIST(wlc)+(i*2)));
		if ((i+1)%4)
		bcm_bprintf(b, "\n");
	}
#endif /* WL_PSMX */

done:
	if (buf) {
		MFREE(wlc->osh, buf, AMPDU_DUMP_LEN);
	}

	return err;
} /* wlc_dump_macreq */

#ifdef WL_PSMX
/* accept request and prepare command to mac in tlv format */
static int
wlc_maccmd_req(wlc_info_t *wlc, wl_macreq_params_t *mreq, bool blocking)
{

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return BCME_UNSUPPORTED;
	}

	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	if (!mreq) {
		return BCME_ERROR;
	}

	if (!((1 << mreq->type) & (C_MREQ_SUPPORTED_TYPES))) {
		return BCME_UNSUPPORTED;
	}

	/* only DEL command accepts no argument */
	if (mreq->type == C_MREQ_ADD_MTXV && !mreq->len) {
		return BCME_BADARG;
	}

	switch (mreq->type) {
		case C_MREQ_ADD_LMEM:
		case C_MREQ_UPD_LMEM:
		case C_MREQ_DEL_LMEM:
			return _wlc_macreq_add_del_lmem(wlc, mreq, blocking);
			break;
		case C_MREQ_ADD_MTXV:
		case C_MREQ_DEL1_MTXV:
		case C_MREQ_DELA_MTXV:
			/* write request */
			return _wlc_macreq_add_del_mtxv(wlc, mreq, blocking);
			break;

		case C_MREQ_LD_MTMR:
			/* write request */
			return _wlc_macreq_ld_mutimer(wlc, mreq, blocking);
			break;

		default:
			return BCME_BADARG;
	}

	return BCME_OK;
}

static int
_wlc_macreq_add_del_lmem(wlc_info_t *wlc, wl_macreq_params_t *mreq, bool blocking)
{
	osl_t *osh = wlc->osh;
	int i, val = 0;

	ASSERT(mreq || wlc->clk);

	/* idle? */
	SPINWAIT((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ), 10000);
	if ((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ)) {
		return BCME_BUSY;
	}

	/* set type of command */
	wlc_write_shmx(wlc, MX_MREQ_TYPE(wlc), mreq->type);

	/* set len */
	wlc_write_shmx(wlc, MX_MREQ_LEN(wlc), mreq->len);
	for (i = 0; i < mreq->len; i++) {
		wlc_write_shmx(wlc, MX_MREQ_LIST(wlc)+(i*2), mreq->lidx_list[i]);
	}

	/* mark the request as valid */
	wlc_write_shmx(wlc, MX_MREQ_FLAG(wlc), mreq->flag);

	W_REG(osh, D11_MACCOMMAND_psmx(wlc), MCMDX_REQ);

	if (blocking) {
		SPINWAIT((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ), 10000);
		if ((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ)) {
			return BCME_BUSY;
		}
		val = wlc_read_shmx(wlc, MX_MREQ_FLAG(wlc));
		if (!(val & (1 << C_MRFLAG_DONE_NBIT))) {
			return BCME_BUSY;
		}
	}

	WL_MUTX(("wl%d val:%04x type:%04x flag:%04x len:%04x cnt:%04x ",
		wlc->pub->unit, val, mreq->type, mreq->flag, mreq->len,
		wlc_read_shmx(wlc, MX_MACREQ_CNT(wlc))));
	WL_MUTX(("msg:"));
	for (i = 0; i < mreq->len; i++) {
		WL_MUTX(("%04x ", mreq->lidx_list[i]));
	}
	WL_MUTX(("\n"));

	return BCME_OK;
}

static int
_wlc_macreq_add_del_mtxv(wlc_info_t *wlc, wl_macreq_params_t *mreq, bool blocking)
{
	osl_t *osh = wlc->osh;
	int i, val = 0;
	int idle_time = (wlc_txbf_get_mu_sounding_period(wlc->txbf) * 1000);
	int delta = 10000;

	// for mac request to come back with ack, it takes a maximum of one sounding period
	// so wait for a maximum of one sounding period along with a delta of 10 msec
	idle_time +=  delta;

	ASSERT(mreq || wlc->clk);

	/* idle? */
	SPINWAIT((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ), idle_time);
	if ((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ)) {
		return BCME_BUSY;
	}

	/* set type of command */
	wlc_write_shmx(wlc, MX_MREQ_TYPE(wlc), mreq->type);

	/* set len */
	wlc_write_shmx(wlc, MX_MREQ_LEN(wlc), mreq->len);
	for (i = 0; i < mreq->len; i++) {
		scb_t *scb = NULL;
		uint16 fifogrp_idx = 0;

		/* see if fifo group index is assigned from command */
		/* if not derive from muclient index */

		if ((mreq->lidx_list[i] & 0x8000)) {
			scb = wlc_ratelinkmem_retrieve_cur_scb(wlc, mreq->lidx_list[i] &
				C_MACREQ_LIDX_MASK);
			if (!scb)
				return BCME_ERROR;

			mreq->lidx_list[i] = i;
			fifogrp_idx = wlc_mutx_sta_mucidx_get(wlc->mutx, scb);
			mreq->lidx_list[i] |= (fifogrp_idx << C_MACREQ_FGIDX_SHIFT);

		}
		wlc_write_shmx(wlc, MX_MREQ_LIST(wlc)+(i*2), mreq->lidx_list[i]);
	}

	/* mark the request as valid */
	wlc_write_shmx(wlc, MX_MREQ_FLAG(wlc), mreq->flag);

	W_REG(osh, D11_MACCOMMAND_psmx(wlc), MCMDX_REQ);

	if (blocking) {
		SPINWAIT((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ), idle_time);
		if ((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ)) {
			return BCME_BUSY;
		}
		val = wlc_read_shmx(wlc, MX_MREQ_FLAG(wlc));
		if (!(val & (1 << C_MRFLAG_DONE_NBIT))) {
			return BCME_BUSY;
		}
	}

	WL_MUTX(("wl%d val:%04x type:%04x flag:%04x len:%04x ",
		wlc->pub->unit, val, mreq->type, mreq->flag, mreq->len));
	WL_MUTX(("msg:"));
	for (i = 0; i < mreq->len; i++) {
		WL_MUTX(("%04x ", mreq->lidx_list[i]));
	}
	WL_MUTX(("\n"));

	return BCME_OK;
}

static int
_wlc_macreq_ld_mutimer(wlc_info_t *wlc, wl_macreq_params_t *mreq, bool blocking)
{
	osl_t *osh = wlc->osh;
	int i, val = 0;

	ASSERT(mreq || wlc->clk);

	/* idle? */
	SPINWAIT((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ), 10000);
	if ((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ)) {
		return BCME_BUSY;
	}

	/* set type of command */
	wlc_write_shmx(wlc, MX_MREQ_TYPE(wlc), mreq->type);

	/* set len */
	wlc_write_shmx(wlc, MX_MREQ_LEN(wlc), mreq->len);
	for (i = 0; i < mreq->len; i++) {
		wlc_write_shmx(wlc, MX_MREQ_LIST(wlc)+(i*2), (mreq->lidx_list[i] << 2));
	}

	/* mark the request as valid */
	wlc_write_shmx(wlc, MX_MREQ_FLAG(wlc), mreq->flag);

	W_REG(osh, D11_MACCOMMAND_psmx(wlc), MCMDX_REQ);

	if (blocking) {
		SPINWAIT((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ), 10000);
		if ((R_REG(osh, D11_MACCOMMAND_psmx(wlc)) & MCMDX_REQ)) {
			return BCME_BUSY;
		}
		val = wlc_read_shmx(wlc, MX_MREQ_FLAG(wlc));
		if (!(val & (1 << C_MRFLAG_DONE_NBIT))) {
			return BCME_BUSY;
		}
	}

	WL_MUTX(("val:%04x type:%04x flag:%04x len:%04x ",
		val, mreq->type, mreq->flag, mreq->len));
	WL_MUTX(("msg: "));
	for (i = 0; i < mreq->len; i++) {
		WL_MUTX(("%04x ", mreq->lidx_list[i]));
	}
	WL_MUTX(("\n"));

	return BCME_OK;
}

int
wlc_macreq_upd_bfi(wlc_info_t *wlc, scb_t *scb, uint16 fifogrp_idx, bool add)
{
	wl_macreq_params_t mreq;
	uint16	lidx;

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return BCME_UNSUPPORTED;
	}

	if (wlc_txbf_autotxvcfg_get(wlc->txbf, add)) {
		WL_MUTX(("wl%d: %s auto txv add/del is active\n", wlc->pub->unit, __FUNCTION__));
		return BCME_OK;
	}

	memset(&mreq, 0x00, sizeof(wl_macreq_params_t));

	mreq.flag = 1 << C_MRFLAG_VLD_NBIT;

	if (add) {
		mreq.type = C_MREQ_ADD_MTXV;
	} else {
		mreq.type = C_MREQ_DEL1_MTXV;
	}

	mreq.len = 1;
	lidx = wlc_ratelinkmem_get_scb_link_index(wlc, scb);
	if (lidx == D11_RATE_LINK_MEM_IDX_INVALID) {
		WL_MUTX(("wl%d: %s lidx (%04x) invalid, fifogrp_idx %04x, mreq.type %04x, add %d\n",
			wlc->pub->unit, __FUNCTION__, lidx, fifogrp_idx, mreq.type, add));
		return BCME_ERROR;
	}
	mreq.lidx_list[0] = lidx | (fifogrp_idx << C_MACREQ_FGIDX_SHIFT);

	return wlc_maccmd_req(wlc, &mreq, TRUE);

}

int
wlc_macreq_add_lmem(wlc_info_t *wlc, uint16 lmem_idx)
{
	wl_macreq_params_t mreq;

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return BCME_UNSUPPORTED;
	}

	if (lmem_idx == WLC_RLM_SPECIAL_LINK_IDX) {
		return BCME_OK;
	}

	memset(&mreq, 0x00, sizeof(wl_macreq_params_t));

	mreq.flag = (1 << C_MRFLAG_VLD_NBIT | 1 << C_MRFLAG_CLRSTS_NBIT);

	mreq.type = C_MREQ_ADD_LMEM;
	mreq.len = 1;
	mreq.lidx_list[0] = lmem_idx;

	return wlc_maccmd_req(wlc, &mreq, TRUE);
}

int
wlc_macreq_upd_lmem(wlc_info_t *wlc, uint16 lmem_idx, bool add, bool clr_txbf_stats)
{
	wl_macreq_params_t mreq;

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return BCME_UNSUPPORTED;
	}

	if (lmem_idx == WLC_RLM_SPECIAL_LINK_IDX) {
		return BCME_OK;
	}

	memset(&mreq, 0x00, sizeof(wl_macreq_params_t));

	mreq.flag = (1 << C_MRFLAG_VLD_NBIT | clr_txbf_stats << C_MRFLAG_CLRSTS_NBIT);

	if (add) {
		mreq.type = C_MREQ_UPD_LMEM;
	} else {
		mreq.type = C_MREQ_DEL_LMEM;
	}

	mreq.len = 1;
	mreq.lidx_list[0] = lmem_idx;

	return wlc_maccmd_req(wlc, &mreq, TRUE);
}

int
wlc_macreq_deltxv(wlc_info_t *wlc)
{
	wl_macreq_params_t mreq;

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return BCME_UNSUPPORTED;
	}

	memset(&mreq, 0x00, sizeof(wl_macreq_params_t));

	mreq.flag = (1 << C_MRFLAG_VLD_NBIT);
	mreq.type = C_MREQ_DELA_MTXV;
	mreq.len = 0;

	return wlc_maccmd_req(wlc, &mreq, TRUE);
}

int
wlc_macreq_txbf_mutimer(wlc_info_t *wlc, uint16 mutimer_val)
{
	wl_macreq_params_t mreq;

	if (D11REV_LT(wlc->pub->corerev, 128)) {
		return BCME_UNSUPPORTED;
	}

	memset(&mreq, 0x00, sizeof(wl_macreq_params_t));

	mreq.flag = 1 << C_MRFLAG_VLD_NBIT;
	mreq.type = C_MREQ_LD_MTMR;
	mreq.len = 1;
	mreq.lidx_list[0] = mutimer_val;

	return wlc_maccmd_req(wlc, &mreq, TRUE);
}

#endif /* WL_PSMX */
