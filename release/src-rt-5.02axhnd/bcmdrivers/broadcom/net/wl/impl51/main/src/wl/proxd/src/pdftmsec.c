/*
 * Proxd FTM method security support. See twiki FineTimingMeasurement.
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
 * $Id: pdftmsec.c 777082 2019-07-18 14:48:21Z $
 */

#include "pdftmpvt.h"

#include <sha2.h>
#include <wlc_auth.h>

#define FTM_TPK_PREFIX "BCM FTM TPK"
#define NUM_FTM_TPK_PREFIXES 5

#ifdef WL_RANGE_SEQ
/* calculate ftm tpk */
static void
calc_ftm_tpk(const uint8 *pmk, int pmk_len, const bcm_const_xlvp_t *pfxs, int npfxs,
	uint8 *tpk, int tpk_len)
{
	hmac_sha2(HASH_SHA256, pmk, pmk_len, pfxs, npfxs, NULL, 0, tpk, tpk_len);
}
#endif /* WL_RANGE_SEQ */

/* FTM TPK Truncate128 * HMAC-SHA-256(PMK, "BCM FTM TPK", * INONCE RNONCE IADDR RADDR) */
static int
make_ftm_tpk_prefixes(
	const uint8 *i_nonce, int i_nonce_len,
	const uint8 *r_nonce, int r_nonce_len,
	struct ether_addr *i_ea,
	struct ether_addr *r_ea,
	bcm_const_xlvp_t *pfxs)
{
	int n = 0;

	pfxs[n].len = sizeof(FTM_TPK_PREFIX) - 1;
	pfxs[n++].data = (uint8 *)FTM_TPK_PREFIX;

	pfxs[n].len = i_nonce_len;
	pfxs[n++].data = i_nonce;

	pfxs[n].len = r_nonce_len;
	pfxs[n++].data = r_nonce;

	pfxs[n].len = ETHER_ADDR_LEN;
	pfxs[n++].data = (uint8 *)i_ea;

	pfxs[n].len = ETHER_ADDR_LEN;
	pfxs[n++].data = (uint8 *)r_ea;

	return n;
}

#ifndef WL_RANGE_SEQ
static void
calc_ftm_tpk(const uint8 *pmk, int pmk_len, const bcm_const_xlvp_t *pfxs, int npfxs,
	uint8 *tpk, int tpk_len)
{
	hmac_sha2(HASH_SHA256, pmk, pmk_len, pfxs, npfxs, NULL, 0, tpk, tpk_len);
}
#endif /* !WL_RANGE_SEQ */

/* calculate Ri and Rr */
void
pdftm_calc_ri_rr(const uint8 *tpk, uint32 tpk_len, const uint8* rand_ri_rr,
	uint32 rand_ri_rr_len, uint8 *output, unsigned int output_len)
{
	hmac_sha2(HASH_SHA256, tpk, tpk_len, NULL, 0, rand_ri_rr, (int)rand_ri_rr_len,
		output, output_len);
	return;
}

#ifdef WL_RANGE_SEQ
int
wlc_ftm_set_tpk(wlc_ftm_t *ftm, scb_t *scb, const uint8 *tpk, uint16 tpk_len)
{
	ftm_scb_t *ftm_scb;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));

	ftm_scb = pdftm_scb_alloc(ftm, scb, NULL);
	if (!ftm_scb) {
		err = BCME_NORESOURCE;
		goto done;
	}
	ftm_scb->tpk_len = tpk_len;
	memcpy_s(ftm_scb->tpk, tpk_len, tpk, tpk_len);

done:
	FTM_LOGSEC(ftm, (("wl%d.%d: %s: status %d\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(scb->bsscfg), __FUNCTION__, err)));
	return err;
}

int
wlc_ftm_pmk_to_tpk(wlc_ftm_t *ftm, scb_t *scb, const uint8 *pmk, int pmk_len,
	const uint8 *anonce, int anonce_len, const uint8 *snonce, int snonce_len)
#else
int
wlc_ftm_new_tpk(wlc_ftm_t *ftm, scb_t *scb, const uint8 *pmk, int pmk_len,
	const uint8 *anonce, int anonce_len, const uint8 *snonce, int snonce_len)
#endif /* WL_RANGE_SEQ */
{
	ftm_scb_t *ftm_scb;
	wlc_bsscfg_t *bsscfg;
	bool is_auth;
	struct ether_addr *auth_ea;
	struct ether_addr *sup_ea;
	bcm_const_xlvp_t pfxs[NUM_FTM_TPK_PREFIXES];
	int npfxs;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));

	bsscfg = SCB_BSSCFG(scb);

	if (!bsscfg) {
		ASSERT(0);
		err = BCME_NORESOURCE;
		goto done;
	}

	/* ignore if bsscfg is not secure */
	if (!FTM_BSSCFG_SECURE(ftm, bsscfg))
		goto done;

	ftm_scb = pdftm_scb_alloc(ftm, scb, NULL);
	if (!ftm_scb) {
		err = BCME_NORESOURCE;
		goto done;
	}

	is_auth =  BSS_AUTH_TYPE(bsscfg->wlc->authi, bsscfg) != AUTH_UNUSED;
	auth_ea = is_auth ? &bsscfg->cur_etheraddr : &scb->ea;
	sup_ea = is_auth ? &scb->ea : &bsscfg->cur_etheraddr;

	npfxs = make_ftm_tpk_prefixes(anonce, anonce_len, snonce, snonce_len,
		auth_ea, sup_ea, pfxs);
	calc_ftm_tpk(pmk, pmk_len, pfxs, npfxs, ftm_scb->tpk, FTM_TPK_MAX_LEN);
	ftm_scb->tpk_len = FTM_TPK_MAX_LEN;

done:
	FTM_LOGSEC(ftm, (("wl%d.%d: %s: status %d\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(scb->bsscfg), __FUNCTION__, err)));
	return err;
}

int
pdftm_sec_check_rx_policy(pdftm_t *ftm, pdftm_session_t *sn,
	const dot11_management_header_t *hdr)
{
	uint16 fc;
#ifdef WL_RANGE_SEQ
	uint8 cat;
#else
	const uint8 cat = ((const uint8 *)(hdr + 1))[DOT11_ACTION_CAT_OFF]; /* XX Qos? */
#endif // endif

	int err = WL_PROXD_E_SEC_POLICY;

	ASSERT(FTM_VALID(ftm));
	ASSERT(FTM_VALID_SESSION(sn));

	fc = ltoh16(hdr->fc);

	/* note: frame action checked elsewhere to be FTM req or meas */

	if (fc & FC_WEP) { /* protected frame */
		/* adjust category - ideally we need body offset */
#ifdef WL_RANGE_SEQ
		cat = *(((const uint8 *)(hdr + 1)) + DOT11_IV_MAX_LEN);
#endif // endif
		if (!FTM_BSSCFG_SECURE(ftm, sn->bsscfg) ||
				(cat != DOT11_ACTION_CAT_PDPA))
			goto done;
		else
			err = BCME_OK;
	} else {  /* unprotected frame */
		/* configured to be secure or not public action */
#ifdef WL_RANGE_SEQ
		cat = *((const uint8 *)(hdr + 1));
#endif // endif
		if (FTM_BSSCFG_SECURE(ftm, sn->bsscfg) ||
				(cat != DOT11_ACTION_CAT_PUBLIC))
			goto done;
		else
			err = BCME_OK;
	}

done:
	FTM_LOGSEC(ftm, (("wl%d.%d: %s: session idx %d status %d\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(sn->bsscfg), __FUNCTION__, sn->idx, err)));
	return err;
}

int
pdftm_sec_validate_session(pdftm_t *ftm, pdftm_session_t *sn)
{
	int err = BCME_OK;
	pdftm_session_config_t *sncfg;

	ASSERT(FTM_VALID(ftm));
	ASSERT(FTM_VALID_SESSION(sn));

	sncfg = sn->config;
	if (FTM_SESSION_IS_INITIATOR(sn)) {
		/* for a secure bsscfg, make the session secure, otherwise not */
		if (FTM_BSSCFG_SECURE(ftm, sn->bsscfg))
			sncfg->flags |= WL_PROXD_SESSION_FLAG_SECURE;
		else
			 sncfg->flags &= ~WL_PROXD_SESSION_FLAG_SECURE;
	} else { /* target */
		if (FTM_SESSION_SECURE(sn)) {
			if (!FTM_BSSCFG_SECURE(ftm, sn->bsscfg)) {
				err = WL_PROXD_E_SEC_POLICY; /* secure sn over unsecure bss */
				goto done;
			}
		} else {
			if (FTM_BSSCFG_SECURE(ftm, sn->bsscfg)) {
				err = WL_PROXD_E_SEC_POLICY; /* unsecure sn over secure bss */
				goto done;
			}
		}
	}

	if (FTM_SESSION_SECURE(sn)) {
		scb_t *scb;

		/* secure sessions must be authorized i.e. have key */
		scb = wlc_scbfind_dualband(ftm->wlc, sn->bsscfg, &sncfg->burst_config->peer_mac);
		if (!scb || !SCB_AUTHORIZED(scb)) {
			err = WL_PROXD_E_SEC_NOKEY;
			goto done;
		}

		if (FTM_SESSION_SEQ_EN(sn)) {
			ftm_scb_t *ftm_scb = FTM_SCB(ftm, scb);
			if (!FTM_SCB_TPK_VALID(ftm_scb)) {
				err = WL_PROXD_E_SEC_NOKEY;
				goto done;
			}
		}
	}

done:
	FTM_LOGSEC(ftm, (("wl%d.%d: %s: session idx %d status %d\n",
		FTM_UNIT(ftm), WLC_BSSCFG_IDX(sn->bsscfg), __FUNCTION__, sn->idx, err)));
	return err;
}
