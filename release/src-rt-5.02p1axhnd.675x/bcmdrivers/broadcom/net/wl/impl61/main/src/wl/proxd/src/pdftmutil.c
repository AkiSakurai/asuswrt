/*
 * Proxd FTM method implementation - utils. See twiki FineTimingMeasurement.
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
 * $Id: pdftmutil.c 777979 2019-08-19 23:14:13Z $
 */

#include "pdftmpvt.h"

/* external interface */
uint64
pdftm_intvl2usec(const wl_proxd_intvl_t *intvl)
{
	uint64 ret;
	ret = intvl->intvl;
	switch (intvl->tmu) {
	case WL_PROXD_TMU_TU:			ret = FTM_TU2MICRO(ret); break;
	case WL_PROXD_TMU_SEC:			ret *= 1000000; break;
	case WL_PROXD_TMU_NANO_SEC:		ret = intvl->intvl / 1000; break;
	case WL_PROXD_TMU_PICO_SEC:		ret = intvl->intvl / 1000000; break;
	case WL_PROXD_TMU_MILLI_SEC:	ret *= 1000; break;
	case WL_PROXD_TMU_MICRO_SEC:	/* fall through */
	default:						break;
	}
	return ret;
}

uint64
pdftm_intvl2nsec(const wl_proxd_intvl_t *intvl)
{
	uint64 ret;
	ret = intvl->intvl;
	switch (intvl->tmu) {
	case WL_PROXD_TMU_TU:			ret = FTM_TU2MICRO(ret) * 1000; break;
	case WL_PROXD_TMU_SEC:			ret *= 1000000000; break;
	case WL_PROXD_TMU_MILLI_SEC:	ret *= 1000000; break;
	case WL_PROXD_TMU_MICRO_SEC:	ret *= 1000; break;
	case WL_PROXD_TMU_PICO_SEC:		ret = intvl->intvl / 1000; break;
	case WL_PROXD_TMU_NANO_SEC:		/* fall through */
	default:						break;
	}
	return ret;
}

uint64
pdftm_intvl2psec(const wl_proxd_intvl_t *intvl)
{
	uint64 ret;
	ret = intvl->intvl;
	switch (intvl->tmu) {
	case WL_PROXD_TMU_TU:			ret = FTM_TU2MICRO(ret) * 1000000; break;
	case WL_PROXD_TMU_SEC:			ret *= 1000000000000ULL; break;
	case WL_PROXD_TMU_MILLI_SEC:	ret *= 1000000000; break;
	case WL_PROXD_TMU_MICRO_SEC:	ret *= 1000000; break;
	case WL_PROXD_TMU_NANO_SEC:		ret *= 1000; break;
	case WL_PROXD_TMU_PICO_SEC:		/* fall through */;
	default:						break;
	}
	return ret;
}

/* 64 bit division w/ 16 bits divisor */
uint64
pdftm_div64(uint64 val, uint16 div)
{
	uint32 hi, lo, rem;
	uint64 ret;

	hi = (uint32)(val >> 32);
	lo = (uint32)val;

	ret = (uint64)(hi / div) << 32;
	rem = hi % div;

	hi = (rem << 16) | (lo >> 16);
	ret += (uint64)(hi / div) << 16;
	rem = hi % div;

	lo = (rem << 16) | (lo & 0xffff);
	ret += (uint64)(lo / div);
	return ret;
}

bool
pdftm_need_proxd(pdftm_t *ftm, uint flag)
{
	int i;
	bool need = FALSE;
	uint test_bit;
	int num_bits;

	ASSERT(FTM_VALID(ftm));
	num_bits = ftm->ftm_cmn->enabled_bss_len * NBBY;
	for (i = 0; i < num_bits; i += FTM_BSSCFG_NUM_OPTIONS) {
		test_bit = i + flag;
		if (isset(ftm->ftm_cmn->enabled_bss, test_bit)) {
			need = TRUE;
			break;
		}
	}
	return need;
}

bcm_tlv_t*
pdftm_tlvdup(pdftm_t *ftm, const bcm_tlv_t *tlv)
{
	uint tlv_size;
	bcm_tlv_t *out_tlv = NULL;

	if (!tlv)
		goto done;

	tlv_size = BCM_TLV_SIZE(tlv);
	out_tlv = MALLOCZ(FTM_OSH(ftm), tlv_size);
	if (out_tlv)
		memcpy(out_tlv, tlv, tlv_size);

done:
	return out_tlv;
}

#define CUR_REF_TICK(wlc) CURRENT_PMU_TIME(wlc)

/* PMU time is in units of 32kHz clock  -  x * 1000 / 32 */
#define FTM_PMU_TIME_TO_MICRO(_pmu_time) (((uint64)(_pmu_time) * 1000) >> 5)

uint64
pdftm_get_pmu_tsf(pdftm_t *ftm)
{
#ifdef WL_FTM_MSCH
	return (msch_current_time(ftm->wlc->msch_info));
#else
	uint32 pmu_time;
	uint64 cur_tsf;
	uint32 pmu_diff;
	wlc_info_t *wlc;

	wlc = ftm->wlc;
	pmu_time = CUR_REF_TICK(wlc);

	/* rollover support */
	if (pmu_time < ftm->ftm_cmn->last_pmu)
		pmu_diff = ~ftm->ftm_cmn->last_pmu + pmu_time;
	else
		pmu_diff = pmu_time - ftm->ftm_cmn->last_pmu;

	cur_tsf = ftm->ftm_cmn->last_tsf;
	if (!wlc->scc_per)
		cur_tsf += FTM_PMU_TIME_TO_MICRO(pmu_diff); /* nominal */
	else
		cur_tsf += wlc_ref_tick_to_us(wlc, pmu_diff);

	ftm->ftm_cmn->last_pmu = pmu_time;
	ftm->ftm_cmn->last_tsf = cur_tsf;
	FTM_LOGSCHED(ftm, (("wl%d: %s: pmu time %u current tsf %u.%u scc.frac %u.%u\n",
		FTM_UNIT(ftm), __FUNCTION__, pmu_time, FTM_LOG_TSF_ARG(cur_tsf),
		wlc->scc_per, wlc->scc_per_frac)));
	return cur_tsf;
#endif /* WL_FTM_MSCH */
}

void
pdftm_bsscfg_clear_options(pdftm_t *ftm, wlc_bsscfg_t *bsscfg)
{
	int bpos, epos;

	bpos = FTM_BSSCFG_OPTION_BIT(bsscfg, 0);
	epos = bpos + FTM_BSSCFG_NUM_OPTIONS;
	for (; bpos < epos; ++bpos) {
		clrbit(ftm->ftm_cmn->enabled_bss, bpos);
	}
}
