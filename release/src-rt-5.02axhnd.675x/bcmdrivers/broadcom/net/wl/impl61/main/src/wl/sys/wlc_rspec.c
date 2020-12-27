/**
 * @file
 * @brief
 * ratespec related routines
 *
 * Copyright 2019 Broadcom
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
 * $Id: wlc_rspec.c 774708 2019-05-03 13:45:11Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_stf.h>
#include <wlc_rsdb.h>
#include <wlc_prot_g.h>
#include <wlc_vht.h>
#include <wlc_he.h>
#include <wlc_txbf.h>
#include <wlc_txc.h>
#include <wlc_ht.h>
#include <wlc_rspec.h>
#include <phy_api.h>
#if defined(WLAMPDU)
#include <wlc_ampdu.h>
#endif // endif
#ifdef WLAMSDU_TX
#include <wlc_amsdu.h>
#endif /* WLAMSDU_TX */
#if defined(WL_PROXDETECT)
#include <wlc_ftm.h>
#endif /* WL_PROXDETECT */
#include <wlc_ratelinkmem.h>
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

/* module info */
struct wlc_rspec_info {
	wlc_info_t *wlc;
};

/* MAC rate histogram */
/* TODO optimize this structure to either convert all mcs (HE, VHT, HT)
 * into say VHT mcs or save the mcs and also the PHY spec (HE, VHT, HT)
 * globally assuming all mcs are of the same PHY spec...either way we can
 * merge mcs/prop11n_mcs/vht/he arrays into a single mcs array.
 */
typedef struct {
	uint32	rate[DOT11_RATE_MAX + 1];	/**< Rates */
	uint32	mcs[WL_RATESET_SZ_HT_MCS * WL_TX_CHAINS_MAX];	/**< MCS counts */
	uint32	prop11n_mcs[WLC_11N_LAST_PROP_MCS - WLC_11N_FIRST_PROP_MCS + 1]; /** MCS counts */
	uint32	vht[WL_RATESET_SZ_VHT_MCS][WL_TX_CHAINS_MAX];	/**< VHT counts */
	uint32	he[WL_RATESET_SZ_HE_MCS][WL_TX_CHAINS_MAX];	/**< HE counts */
} wlc_mac_ratehisto_res_t;

/* iovar table */
enum {
	IOV_5G_RATE = 1,
	IOV_5G_MRATE = 2,
	IOV_2G_RATE = 3,
	IOV_2G_MRATE = 4,
	IOV_LAST
};

static const bcm_iovar_t rspec_iovars[] = {
	{"5g_rate", IOV_5G_RATE, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{"5g_mrate", IOV_5G_MRATE, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{"2g_rate", IOV_2G_RATE, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{"2g_mrate", IOV_2G_MRATE, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, 0},
	{NULL, 0, 0, 0, 0, 0}
};

static int wlc_wlrspec2ratespec(wlc_bsscfg_t *bsscfg, ratespec_t wlrspec, ratespec_t *pratespec);
static int wlc_ratespec2wlrspec(wlc_bsscfg_t *bsscfg, ratespec_t ratespec, ratespec_t *pwlrspec);
static ratespec_t wlc_rspec_to_rts_rspec_ex(wlc_info_t *wlc, ratespec_t rspec, bool use_rspec,
	bool g_protection);
static void wlc_get_rate_histo_bsscfg(wlc_bsscfg_t *bsscfg, wlc_mac_ratehisto_res_t *rhist,
	ratespec_t *most_used_ratespec, ratespec_t *highest_used_ratespec);
static int wlc_set_ratespec_override(wlc_info_t *wlc, int band_id, ratespec_t rspec, bool mcast);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

ratespec_t
wlc_lowest_basic_rspec(wlc_info_t *wlc, wlc_rateset_t *rs)
{
	ratespec_t lowest_basic_rspec;
	uint i;

	/* Use the lowest basic rate */
	lowest_basic_rspec = LEGACY_RSPEC(rs->rates[0]);
	for (i = 0; i < rs->count; i++) {
		if (rs->rates[i] & WLC_RATE_FLAG) {

			lowest_basic_rspec = LEGACY_RSPEC(rs->rates[i]);

			break;
		}
	}

#ifdef WL11N
	wlc_rspec_txexp_upd(wlc, &lowest_basic_rspec);
#endif // endif

	return (lowest_basic_rspec);
}

/* return lowest ratespec and non less than predefined rate threshold */
ratespec_t
wlc_get_compat_basic_rspec(wlc_info_t *wlc, wlc_rateset_t *rs, uint8 rate_thold)
{
	uint8 rate = rs->rates[0] & RATE_MASK;
	uint i;

	for (i = 0; i < rs->count; i++) {
		if (rate >= rate_thold)
			break;

		rate = rs->rates[i] & RATE_MASK;
	}

	return LEGACY_RSPEC(rate);
}

void
wlc_rspec_txexp_upd(wlc_info_t *wlc, ratespec_t *rspec)
{
	if (RSPEC_ISOFDM(*rspec)) {
		if (WLCISNPHY(wlc->band) &&
		    wlc->stf->ss_opmode == PHY_TXC1_MODE_CDD) {
			*rspec |= (1 << WL_RSPEC_TXEXP_SHIFT);
		} else if (WLCISACPHY(wlc->band)) {
			uint ntx = wlc_stf_txchain_get(wlc, *rspec);
			uint nss = wlc_ratespec_nss(*rspec);
			*rspec |= ((ntx - nss) << WL_RSPEC_TXEXP_SHIFT);
		}
	}
}

/* iovar dispatcher */
static int
wlc_rspec_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint a_len, uint val_size, struct wlc_if *wlcif)
{
	wlc_rspec_info_t *rsi = hdl;
	wlc_info_t *wlc = rsi->wlc;
	int err = BCME_OK;
	int32 int_val = 0;
	wlc_bsscfg_t *bsscfg;
	int32 *ret_int_ptr;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_GVAL(IOV_5G_RATE): {
		ratespec_t wlrspec;
		ratespec_t ratespec = wlc->bandstate[BAND_5G_INDEX]->rspec_override;

		err = wlc_ratespec2wlrspec(bsscfg, ratespec, &wlrspec);
		*ret_int_ptr = (int32)wlrspec;
		break;
	}

	case IOV_SVAL(IOV_5G_RATE):
		err = wlc_set_iovar_ratespec_override(wlc, bsscfg, WLC_BAND_5G,
			(ratespec_t)int_val, FALSE);
		break;

	case IOV_GVAL(IOV_5G_MRATE): {
		ratespec_t wlrspec;
		ratespec_t ratespec = wlc->bandstate[BAND_5G_INDEX]->mrspec_override;

		err = wlc_ratespec2wlrspec(bsscfg, ratespec, &wlrspec);
		*ret_int_ptr = (int32)wlrspec;
		break;
	}

	case IOV_SVAL(IOV_5G_MRATE):
		err = wlc_set_iovar_ratespec_override(wlc, bsscfg, WLC_BAND_5G,
			(ratespec_t)int_val, TRUE);
		break;

	case IOV_GVAL(IOV_2G_RATE): {
		ratespec_t wlrspec;
		ratespec_t ratespec = wlc->bandstate[BAND_2G_INDEX]->rspec_override;

		err = wlc_ratespec2wlrspec(bsscfg, ratespec, &wlrspec);
		*ret_int_ptr = (int32)wlrspec;
		break;
	}

	case IOV_SVAL(IOV_2G_RATE):
		err = wlc_set_iovar_ratespec_override(wlc, bsscfg, WLC_BAND_2G,
			(ratespec_t)int_val, FALSE);
		break;

	case IOV_GVAL(IOV_2G_MRATE): {
		ratespec_t wlrspec;
		ratespec_t ratespec = wlc->bandstate[BAND_2G_INDEX]->mrspec_override;

		err = wlc_ratespec2wlrspec(bsscfg, ratespec, &wlrspec);
		*ret_int_ptr = (int32)wlrspec;
		break;
	}

	case IOV_SVAL(IOV_2G_MRATE):
		err = wlc_set_iovar_ratespec_override(wlc, bsscfg, WLC_BAND_2G,
			(ratespec_t)int_val, TRUE);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_rspec_doiovar */

#if defined(MBSS) || defined(WLTXMONITOR)
/** convert rate in OFDM PLCP to ratespec */
ratespec_t
ofdm_plcp_to_rspec(uint8 rate)
{
	ratespec_t rspec = 0;

	/* XXX Could use OFDM_RSPEC() macro to generate the ratespec_t, but that's not
	 * available outside of wlc_rate_sel.c right now.
	 */

	switch (rate & 0x0F) {
	case 0x0B: /* 6Mbps */
		rspec = 6*2;
		break;
	case 0x0F: /* 9Mbps */
		rspec = 9*2;
		break;
	case 0x0A: /* 12 Mbps */
		rspec = 12*2;
		break;
	case 0x0E: /* 18 Mbps */
		rspec = 18*2;
		break;
	case 0x09: /* 24 Mbps */
		rspec = 24*2;
		break;
	case 0x0D: /* 36 Mbps */
		rspec = 36*2;
		break;
	case 0x08: /* 48 Mbps */
		rspec = 48*2;
		break;
	case 0x0C: /* 54 Mbps */
		rspec = 54*2;
		break;
	default:
		/* Not a valid OFDM rate */
		ASSERT(0);
		break;
	}

	return rspec;
} /* ofdm_plcp_to_rspec */
#endif /* MBSS || WLTXMONITOR */

/** transmit related */
ratespec_t
wlc_rspec_to_rts_rspec(wlc_bsscfg_t *cfg, ratespec_t rspec, bool use_rspec)
{
	wlc_info_t *wlc = cfg->wlc;
	bool g_prot = wlc->band->gmode && WLC_PROT_G_CFG_G(wlc->prot_g, cfg);
#if defined(WL_PROXDETECT) && defined(WL_FTM)
	if (WLC_BSSCFG_SECURE_FTM(cfg) && !use_rspec && !RSPEC_ISCCK(rspec)) {
		use_rspec = TRUE;
		rspec = WLC_BASIC_RATE(wlc, WLC_RATE_6M);
	}
#endif /* WL_PROXDETECT && WL_FTM */
	return wlc_rspec_to_rts_rspec_ex(wlc, rspec, use_rspec, g_prot);
}

static ratespec_t
wlc_rspec_to_rts_rspec_ex(wlc_info_t *wlc, ratespec_t rspec, bool use_rspec, bool g_prot)
{
	ratespec_t rts_rspec = 0;

	if (use_rspec) {
		/* use frame rate as rts rate */
		rts_rspec = rspec;

	} else if (g_prot && !RSPEC_ISCCK(rspec)) {
		/* Use 11Mbps as the g protection RTS target rate and fallback.
		 * Use the WLC_BASIC_RATE() lookup to find the best basic rate under the
		 * target in case 11 Mbps is not Basic.
		 * 6 and 9 Mbps are not usually selected by rate selection, but even
		 * if the OFDM rate we are protecting is 6 or 9 Mbps, 11 is more robust.
		 */
		rts_rspec = WLC_BASIC_RATE(wlc, WLC_RATE_11M);
	} else {
		/* calculate RTS rate and fallback rate based on the frame rate
		 * RTS must be sent at a basic rate since it is a
		 * control frame, sec 9.6 of 802.11 spec
		 */
		rts_rspec = WLC_BASIC_RATE(wlc, rspec);
	}

#ifdef WL11N
	if (WLC_PHY_11N_CAP(wlc->band)) {
		/* set rts txbw to correct side band */
		rts_rspec &= ~WL_RSPEC_BW_MASK;

		/* if rspec/rspec_fallback is 40MHz, then send RTS on both 20MHz channel
		 * (DUP), otherwise send RTS on control channel
		 */
		/* XXX 4360: do we need to fix up this fn wlc_rspec_to_rts_rspec() or find
		 * some other way of getting RTS/CTS tx programming info. Rev40 TxH needs
		 * much less info about RTS/CTS format and may not need a whole rspec to be
		 * created.
		 */
		if (RSPEC_IS40MHZ(rspec) && !RSPEC_ISCCK(rts_rspec))
			rts_rspec |= WL_RSPEC_BW_40MHZ;
		else
			rts_rspec |= WL_RSPEC_BW_20MHZ;

		 if ((RSPEC_IS40MHZ(rts_rspec) &&
		        (CHSPEC_IS20(wlc->chanspec))) ||
		        (RSPEC_ISCCK(rts_rspec))) {
			rts_rspec &= ~WL_RSPEC_BW_MASK;
			rts_rspec |= WL_RSPEC_BW_20MHZ;
		}

		/* pick siso/cdd as default for ofdm */
		if (RSPEC_ISOFDM(rts_rspec)) {
			rts_rspec &= ~WL_RSPEC_TXEXP_MASK;
			if (wlc->stf->ss_opmode == PHY_TXC1_MODE_CDD)
				rts_rspec |= (1 << WL_RSPEC_TXEXP_SHIFT);
		}
	}
#else
	/* set rts txbw to control channel BW if 11 n feature is not turned on */
	rts_rspec &= ~WL_RSPEC_BW_MASK;
	rts_rspec |= WL_RSPEC_BW_20MHZ;
#endif /* WL11N */

	return rts_rspec;
} /* wlc_rspec_to_rts_rspec_ex */

ratespec_t
wlc_get_current_highest_rate(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	/* return a default value for rate */
	ratespec_t reprspec;
	uint8 bw = BW_20MHZ;
	bool sgi = (WLC_HT_GET_SGI_TX(wlc->hti)) ? TRUE : FALSE;
	wlcband_t *cur_band = wlc->band;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	uint8 ratemask = 0;
	bool ldpc = FALSE;

	if (cfg->associated) {

		if (N_ENAB(wlc->pub) && CHSPEC_IS40(current_bss->chanspec))
			bw = BW_40MHZ;
#ifdef WL11AC
		if (VHT_ENAB_BAND(wlc->pub, cur_band->bandtype)) {
			if (CHSPEC_IS80(current_bss->chanspec)) {
				bw = BW_80MHZ;
			} else if (CHSPEC_IS160(current_bss->chanspec) ||
				CHSPEC_IS8080(current_bss->chanspec)) {
				bw = BW_160MHZ;
			}

			ldpc = ((wlc->stf->ldpc_tx == AUTO) || (wlc->stf->ldpc_tx == ON));
			ratemask = BAND_5G(wlc->band->bandtype) ?
				WLC_VHT_FEATURES_RATES_5G(wlc->pub) :
				WLC_VHT_FEATURES_RATES_2G(wlc->pub);
			/*
			* STA Mode  Look for scb corresponding to the bss.
			* APSTA Mode default to LDPC setting for AP mode.
			*/
			if (BSSCFG_INFRA_STA(cfg)) {
				struct scb *scb = wlc_scbfind(wlc, cfg, &current_bss->BSSID);
				if (scb) {
					ratemask = wlc_vht_get_scb_ratemask(wlc->vhti, scb);
					ldpc &= (SCB_VHT_LDPC_CAP(wlc->vhti, scb));
				}
			}
		}

		if (HE_ENAB_BAND(wlc->pub, cur_band->bandtype)) {
			ratemask = 0;

			if (CHSPEC_IS80(current_bss->chanspec))
				bw = BW_80MHZ;

			ldpc = ((wlc->stf->ldpc_tx == AUTO) || (wlc->stf->ldpc_tx == ON));

			/*
			* STA Mode  Look for scb corresponding to the bss.
			* APSTA Mode default to LDPC setting for AP mode.
			*/
			if (BSSCFG_INFRA_STA(cfg)) {
				struct scb *scb = wlc_scbfind(wlc, cfg, &current_bss->BSSID);
				if (scb) {
					ldpc &= (SCB_HE_LDPC_CAP(wlc->hei, scb));
				}
			}
		}
#endif /* WL11AC */
		/* associated, so return the max supported rate */
		reprspec = wlc_get_highest_rate(&current_bss->rateset, bw, sgi,
			ldpc, ratemask, wlc->stf->op_txstreams);
	} else {
		if (N_ENAB(wlc->pub)) {
			if (wlc_ht_is_40MHZ_cap(wlc->hti) &&
				((wlc_channel_locale_flags_in_band(wlc->cmi, cur_band->bandunit) &
				WLC_NO_40MHZ) == 0) &&
				WL_BW_CAP_40MHZ(cur_band->bw_cap)) {
				bw = BW_40MHZ;
			}
		}

		if (VHT_ENAB_BAND(wlc->pub, cur_band->bandtype) ||
			HE_ENAB_BAND(wlc->pub, cur_band->bandtype)) {
			if (WL_BW_CAP_160MHZ(cur_band->bw_cap) &&
				((wlc_channel_locale_flags_in_band(wlc->cmi, cur_band->bandunit) &
				WLC_NO_160MHZ) == 0)) {
				bw = BW_160MHZ;
			} else if (WL_BW_CAP_80MHZ(cur_band->bw_cap) &&
				((wlc_channel_locale_flags_in_band(wlc->cmi, cur_band->bandunit) &
				WLC_NO_80MHZ) == 0)) {
				bw = BW_80MHZ;
			}
			ldpc = ((wlc->stf->ldpc_tx == AUTO) || (wlc->stf->ldpc_tx == ON));
		}

		/* not associated, so return the max rate we can send */
		if (HE_ENAB_BAND(wlc->pub, cur_band->bandtype)) {
			ratemask = 0;
		} else if (VHT_ENAB_BAND(wlc->pub, cur_band->bandtype)) {
			ratemask = (BAND_5G(cur_band->bandtype)?
				WLC_VHT_FEATURES_RATES_5G(wlc->pub) :
				WLC_VHT_FEATURES_RATES_2G(wlc->pub));
		}

		reprspec = wlc_get_highest_rate(&cur_band->hw_rateset, bw, sgi,
			ldpc, ratemask, wlc->stf->op_txstreams);

		/* Report the highest CCK rate if gmode == GMODE_LEGACY_B */
		if (BAND_2G(cur_band->bandtype) && (cur_band->gmode == GMODE_LEGACY_B)) {
			wlc_rateset_t rateset;
			wlc_rateset_filter(&cur_band->hw_rateset /* src */, &rateset, FALSE,
				WLC_RATES_CCK, RATE_MASK_FULL, wlc_get_mcsallow(wlc, cfg));
			reprspec = rateset.rates[rateset.count - 1];
		}
	}
	return reprspec;
} /* wlc_get_current_highest_rate */

/** return the "current tx rate" as a ratespec */
ratespec_t
wlc_get_rspec_history(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_mac_ratehisto_res_t *rate_histo;
	ratespec_t reprspec, highest_used_ratespec = 0;
	wlcband_t *cur_band = wlc->band;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	chanspec_t chspec;

	if (cfg->associated)
		cur_band = wlc->bandstate[CHSPEC_IS2G(current_bss->chanspec) ?
			BAND_2G_INDEX : BAND_5G_INDEX];

	reprspec = cur_band->rspec_override ? cur_band->rspec_override : 0;
	if (reprspec) {
		if (RSPEC_ISHT(reprspec) || RSPEC_ISVHT(reprspec) || RSPEC_ISHE(reprspec)) {
			/* If bw in override spec not specified, use bw from chanspec */
			if (RSPEC_BW(reprspec) == WL_RSPEC_BW_UNSPECIFIED) {
				if (cfg->associated) {
					chspec = current_bss->chanspec;
				} else {
					chspec = WLC_BAND_PI_RADIO_CHANSPEC;
				}

				if (CHSPEC_IS20(chspec)) {
					reprspec |= WL_RSPEC_BW_20MHZ;
				} else if (CHSPEC_IS40(chspec)) {
					reprspec |= WL_RSPEC_BW_40MHZ;
				} else if (CHSPEC_IS80(chspec)) {
					reprspec |= WL_RSPEC_BW_80MHZ;
				} else if ((CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec))) {
					reprspec |= WL_RSPEC_BW_160MHZ;
				}

			}
			if (RSPEC_ISHE(reprspec)) {
				if (RSPEC_BW(reprspec) > WL_RSPEC_BW_20MHZ) {
					reprspec |= WL_RSPEC_LDPC;
				}
			} else if (WLC_HT_GET_SGI_TX(wlc->hti) >= ON)
				reprspec |= WL_RSPEC_SGI;
#if defined(WL_BEAMFORMING)
			if (TXBF_ENAB(wlc->pub) && wlc_txbf_get_applied2ovr(wlc->txbf))
				reprspec |= WL_RSPEC_TXBF;
#endif // endif
		}

		/* Update ratespec with actual Tx expansion number */
		reprspec &= ~WL_RSPEC_TXEXP_MASK;
		reprspec |= ((wlc_stf_txchain_get(wlc, reprspec) -
			wlc_ratespec_nsts(reprspec)) << WL_RSPEC_TXEXP_SHIFT);
		return reprspec;
	}

	if ((rate_histo = MALLOCZ(wlc->osh, sizeof(*rate_histo))) != NULL) {
		/*
		 * Loop over txrspec history, looking up rate bins, and summing
		 * nfrags into appropriate supported rate bin.
		 */
		wlc_get_rate_histo_bsscfg(cfg, rate_histo, &reprspec, &highest_used_ratespec);

		MFREE(wlc->osh, rate_histo, sizeof(*rate_histo));
	}
	else {
		/* TODO: return BCME_ERROR back to the caller... */
		WL_ERROR(("wl%d: %s: malloc failed, allocated %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
	}

	/* check for an empty history */
	if (reprspec == 0)
		return wlc_get_current_highest_rate(cfg);

	return (wlc->rpt_hitxrate ? highest_used_ratespec : reprspec);
} /* wlc_get_rspec_history */

/** Create the internal ratespec_t from the wl_ratespec */
static int
wlc_wlrspec2ratespec(wlc_bsscfg_t *bsscfg, ratespec_t wlrspec, ratespec_t *pratespec)
{
	bool islegacy = ((wlrspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_RATE);
	bool isht     = ((wlrspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HT);
	bool isvht    = ((wlrspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_VHT);
	bool ishe    = ((wlrspec & WL_RSPEC_ENCODING_MASK) == WL_RSPEC_ENCODE_HE);
	uint32 bw     =  (wlrspec & WL_RSPEC_BW_MASK);
	uint txexp    = ((wlrspec & WL_RSPEC_TXEXP_MASK) >> WL_RSPEC_TXEXP_SHIFT);
	bool isstbc   = ((wlrspec & WL_RSPEC_STBC) != 0);
	bool issgi    = ((wlrspec & WL_RSPEC_SGI) != 0);
	bool isldpc   = ((wlrspec & WL_RSPEC_LDPC) != 0);
	ratespec_t rspec;

	*pratespec = 0;

	/* check for an uspecified rate */
	if (wlrspec == 0) {
		/* pratespec has already been cleared, return OK */
		return BCME_OK;
	}

	/* set the encoding to legacy, HT, or VHT as specified, and set the rate/mcs field */
	if (islegacy) {
		uint8 rate = (uint8)RSPEC2RATE(wlrspec);
		rspec = LEGACY_RSPEC(rate);

		/* It is not a real legacy rate if the specified bw is not 20MHz (e.g. OFDM DUP40)
		 * clear the bandwidth bits so it can be correctly applied later.
		 */
		if (bw && (bw != WL_RSPEC_BW_20MHZ)) {
			rspec &= ~WL_RSPEC_BW_MASK;
		}
	} else if (isht) {
		uint8 mcs = wlrspec & WL_RSPEC_HT_MCS_MASK;
		rspec = HT_RSPEC(mcs);
	} else if (isvht) {
		uint mcs = (WL_RSPEC_VHT_MCS_MASK & wlrspec);
		uint nss = ((WL_RSPEC_VHT_NSS_MASK & wlrspec) >> WL_RSPEC_VHT_NSS_SHIFT);
		if (IS_PROPRIETARY_VHT_MCS_10_11(mcs)) {
			isldpc = TRUE;
		}
		rspec = VHT_RSPEC(mcs, nss);
	} else if (ishe) {
		uint mcs = (WL_RSPEC_HE_MCS_MASK & wlrspec);
		uint nss = ((WL_RSPEC_HE_NSS_MASK & wlrspec) >> WL_RSPEC_HE_NSS_SHIFT);
		uint8 gi = RSPEC_HE_LTF_GI(wlrspec);
		if (IS_HE_MCS_10_11(mcs)) {
			isldpc = TRUE;
		}
		rspec = HE_RSPEC(mcs, nss);
		rspec |= HE_GI_TO_RSPEC(gi);
	} else {
		return BCME_BADARG;
	}

	/* Tx chain expansion */
	rspec |= (txexp << WL_RSPEC_TXEXP_SHIFT);

	/* STBC, LDPC and Short GI */
	if (isstbc) {
		rspec |= WL_RSPEC_STBC;
	}

	if (isldpc) {
		 rspec |= WL_RSPEC_LDPC;
	}

	if (issgi) {
		 rspec |= WL_RSPEC_SGI;
	}

	/* Bandwidth */

	if (bw == WL_RSPEC_BW_20MHZ) {
		rspec |= WL_RSPEC_BW_20MHZ;
	} else if (bw == WL_RSPEC_BW_40MHZ) {
		rspec |= WL_RSPEC_BW_40MHZ;
	} else if (bw == WL_RSPEC_BW_80MHZ) {
		rspec |= WL_RSPEC_BW_80MHZ;
	} else if (bw == WL_RSPEC_BW_160MHZ) {
		rspec |= WL_RSPEC_BW_160MHZ;
	}

	*pratespec = rspec;

	return BCME_OK;
} /* wlc_wlrspec2ratespec */

/** Create the wl_ratespec from the internal ratespec_t */
static int
wlc_ratespec2wlrspec(wlc_bsscfg_t *bsscfg, ratespec_t ratespec, ratespec_t *pwlrspec)
{
	ratespec_t wlrspec;

	*pwlrspec = 0;

	/* set the encoding to legacy, HT, or VHT as specified, and set the rate/mcs field */
	if (RSPEC_ISLEGACY(ratespec)) {
		wlrspec = (WL_RSPEC_ENCODE_RATE | RSPEC2RATE(ratespec));
	} else if (RSPEC_ISHT(ratespec)) {
		wlrspec = (WL_RSPEC_ENCODE_HT | (ratespec & WL_RSPEC_HT_MCS_MASK));
	} else if (RSPEC_ISVHT(ratespec)) {
		uint mcs = (WL_RSPEC_VHT_MCS_MASK & ratespec);
		uint nss = ((WL_RSPEC_VHT_NSS_MASK & ratespec) >> WL_RSPEC_VHT_NSS_SHIFT);

		wlrspec = (WL_RSPEC_ENCODE_VHT |
		           ((nss << WL_RSPEC_VHT_NSS_SHIFT) & WL_RSPEC_VHT_NSS_MASK) |
		           (mcs & WL_RSPEC_VHT_MCS_MASK));
	} else if (RSPEC_ISHE(ratespec)) {
		uint mcs = (WL_RSPEC_HE_MCS_MASK & ratespec);
		uint nss = ((WL_RSPEC_HE_NSS_MASK & ratespec) >> WL_RSPEC_HE_NSS_SHIFT);

		wlrspec = (WL_RSPEC_ENCODE_HE |
		           ((nss << WL_RSPEC_HE_NSS_SHIFT) & WL_RSPEC_HE_NSS_MASK) |
		           (mcs & WL_RSPEC_HE_MCS_MASK));
	} else {
		return BCME_BADARG;
	}

	/* Tx chain expansion, STBC, LDPC and Short GI */
	wlrspec |= RSPEC_TXEXP(ratespec) << WL_RSPEC_TXEXP_SHIFT;
	wlrspec |= RSPEC_ISSTBC(ratespec) ? WL_RSPEC_STBC : 0;
	wlrspec |= RSPEC_ISLDPC(ratespec) ? WL_RSPEC_LDPC : 0;
	wlrspec |= RSPEC_ISSGI(ratespec) ? WL_RSPEC_SGI : 0;
	wlrspec |= (RSPEC_HE_LTF_GI(ratespec) << WL_RSPEC_HE_GI_SHIFT);

#ifdef WL_BEAMFORMING
	if (RSPEC_ISTXBF(ratespec)) {
		wlrspec |= WL_RSPEC_TXBF;
	}
#endif // endif
	/* Bandwidth */
	if (RSPEC_IS20MHZ(ratespec)) {
		wlrspec |= WL_RSPEC_BW_20MHZ;
	} else if (RSPEC_IS40MHZ(ratespec)) {
		wlrspec |= WL_RSPEC_BW_40MHZ;
	} else if (RSPEC_IS80MHZ(ratespec)) {
		wlrspec |= WL_RSPEC_BW_80MHZ;
	} else if (RSPEC_IS160MHZ(ratespec)) {
		wlrspec |= WL_RSPEC_BW_160MHZ;
	}

	if (ratespec & WL_RSPEC_OVERRIDE_RATE)
		wlrspec |= WL_RSPEC_OVERRIDE_RATE;
	if (ratespec & WL_RSPEC_OVERRIDE_MODE)
		wlrspec |= WL_RSPEC_OVERRIDE_MODE;

	*pwlrspec = wlrspec;

	return BCME_OK;
} /* wlc_ratespec2wlrspec */

int
wlc_set_iovar_ratespec_override(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int band_id,
	ratespec_t wl_rspec, bool mcast)
{
	ratespec_t rspec;
	int err;

	WL_TRACE(("wl%d: %s: band %d wlrspec 0x%08X mcast %d\n", wlc->pub->unit, __FUNCTION__,
	          band_id, wl_rspec, mcast));

	err = wlc_wlrspec2ratespec(cfg, wl_rspec, &rspec);

	if (!err) {
		err = wlc_set_ratespec_override(wlc, band_id, rspec, mcast);
	} else {
		WL_TRACE(("wl%d: %s: rspec translate err %d\n",
		          wlc->pub->unit, __FUNCTION__, err));
	}
#ifdef WLRSDB
	if (RSDB_ENAB(wlc->pub) && (!err)) {
		wlc_rsdb_config_auto_mode_switch(wlc, WLC_RSDB_AUTO_OVERRIDE_RATE,
			(wl_rspec));
	}
#endif /* WLRSDB */
	if (RATELINKMEM_ENAB(wlc->pub) && (!err)) {
		wlc_ratelinkmem_update_rate_entry(wlc, WLC_RLM_SPECIAL_RATE_SCB(wlc), NULL, 0);
	}
	return err;
}

/** transmit related, called when e.g. the user configures a fixed tx rate using the wl utility */
static int
wlc_set_ratespec_override(wlc_info_t *wlc, int band_id, ratespec_t rspec, bool mcast)
{
	wlcband_t *band;
	bool islegacy = RSPEC_ISLEGACY(rspec) && (rspec != 0); /* pre 11n */
	bool isht = RSPEC_ISHT(rspec);	/* 11n */
	bool isvht = RSPEC_ISVHT(rspec);
	bool ishe = RSPEC_ISHE(rspec);
	uint32 bw = RSPEC_BW(rspec);
	uint txexp = RSPEC_TXEXP(rspec);
	bool isstbc = RSPEC_ISSTBC(rspec);
	bool issgi = RSPEC_ISSGI(rspec);
	bool isldpc = RSPEC_ISLDPC(rspec);
	bool ishegi = RSPEC_ISHEGI(rspec);
	uint Nss, Nsts;
	int bcmerror = 0;

	/* Default number of space-time streams for all legacy OFDM/DSSS/CCK is 1 */
	Nsts = 1;

	if (band_id == WLC_BAND_2G) {
		band = wlc->bandstate[BAND_2G_INDEX];
	} else {
		band = wlc->bandstate[BAND_5G_INDEX];
	}

	WL_TRACE(("wl%d: %s: band %d rspec 0x%08X mcast %d\n", wlc->pub->unit, __FUNCTION__,
	          band_id, rspec, mcast));

	/* check for HT/VHT mode items being set on a legacy rate */
	if (islegacy && (isldpc || issgi || isstbc || ishegi)) {

		bcmerror = BCME_BADARG;
		WL_NONE(("wl%d: %s: err, legacy rate with ldpc:%d sgi:%d stbc:%d\n",
		         wlc->pub->unit, __FUNCTION__, isldpc, issgi, isstbc));
		goto done;
	}

	/* validate the combination of rate/mcs/stf is allowed */
	if (HE_ENAB_BAND(wlc->pub, band_id) && ishe) {
		uint8 mcs = rspec & WL_RSPEC_HE_MCS_MASK;
		uint8 mcs_limit = WLC_MAX_HE_MCS;

		Nss = (rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;

		WL_NONE(("wl%d: %s: HE, mcs:%d Nss:%d\n",
		         wlc->pub->unit, __FUNCTION__, mcs, Nss));

		if (mcs > mcs_limit) {
			bcmerror = BCME_BADARG;
			goto done;
		}

		/* HE (11ax) supports Nss 1-8 */
		if (Nss == 0 || Nss > 8) {
			bcmerror = BCME_BADARG;
			goto done;
		}

		/* STBC only supported by some phys */
		if (isstbc && !WLC_STBC_CAP_PHY(wlc)) {
			bcmerror = BCME_RANGE;
			goto done;
		}

		/* STBC expansion doubles the Nss */
		if (isstbc) {
			if (Nss > 1) {
				bcmerror = BCME_BADARG;
				goto done;
			}
			Nsts = Nss * 2;
		} else {
			Nsts = Nss;
		}
		if ((bw > WL_RSPEC_BW_20MHZ) && (!isldpc)) {
			bcmerror = BCME_BADARG;
			goto done;
		}
	} else if (VHT_ENAB_BAND(wlc->pub, band_id) && isvht) {
		uint8 mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
		uint8 mcs_limit = WLC_STD_MAX_VHT_MCS;

		Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		WL_NONE(("wl%d: %s: VHT, mcs:%d Nss:%d\n",
		         wlc->pub->unit, __FUNCTION__, mcs, Nss));

		/* VHT (11ac) supports MCS 0-11 */
		if (WLC_1024QAM_CAP_PHY(wlc))
			mcs_limit = WLC_MAX_VHT_MCS;

		if (mcs > mcs_limit) {
			bcmerror = BCME_BADARG;
			goto done;
		}

		/* VHT (11ac) supports Nss 1-8 */
		if (Nss == 0 || Nss > 8) {
			bcmerror = BCME_BADARG;
			goto done;
		}

		/* STBC only supported by some phys */
		if (isstbc && !WLC_STBC_CAP_PHY(wlc)) {
			bcmerror = BCME_RANGE;
			goto done;
		}

		/* STBC expansion doubles the Nss */
		if (isstbc) {
			Nsts = Nss * 2;
		} else {
			Nsts = Nss;
		}
	} else if (N_ENAB(wlc->pub) && isht) {
		/* mcs only allowed when nmode */
		uint8 mcs = rspec & WL_RSPEC_HT_MCS_MASK;

		/* none of the 11n phys support the defined HT MCS 33-76 */
		if (mcs > 32) {
			if (!WLPROPRIETARY_11N_RATES_ENAB(wlc->pub) ||
			    !WLC_HT_PROP_RATES_CAP_PHY(wlc) || !IS_PROPRIETARY_11N_MCS(mcs) ||
			    wlc->pub->ht_features == WLC_HT_FEATURES_PROPRATES_DISAB) {
				bcmerror = BCME_RANGE;
				goto done;
			}
		}

		/* calculate the number of spatial streams, Nss.
		 * MCS 32 is a 40 MHz single stream, otherwise
		 * 0-31 follow a pattern of 8 MCS per Nss.
		 */
		if (WLPROPRIETARY_11N_RATES_ENAB(wlc->pub)) {
			Nss = GET_11N_MCS_NSS(mcs);
		} else {
			Nss = (mcs == 32) ? 1 : (1 + (mcs / 8));
		}

		/* STBC only supported by some phys */
		if (isstbc && !WLC_STBC_CAP_PHY(wlc)) {
			bcmerror = BCME_RANGE;
			goto done;
		}

		/* BRCM 11n phys only support STBC expansion doubling the num spatial streams */
		if (isstbc) {
			Nsts = Nss * 2;
		} else {
			Nsts = Nss;
		}

		/* mcs 32 is a special case, DUP mode 40 only */
		if (mcs == 32) {
			if (CHSPEC_IS20(wlc->home_chanspec) ||
				(isstbc && (txexp > 1))) {
				bcmerror = BCME_RANGE;
				goto done;
			}
		}
	} else if (islegacy) {
		if (RSPEC_ISCCK(rspec)) {
			/* DSSS/CCK only allowed in 2.4G */
			if (band->bandtype != WLC_BAND_2G) {
				bcmerror = BCME_RANGE;
				goto done;
			}
		} else if (BAND_2G(band->bandtype) && (band->gmode == GMODE_LEGACY_B)) {
			/* allow only CCK if gmode == GMODE_LEGACY_B */
			bcmerror = BCME_RANGE;
			goto done;
		} else if (!RSPEC_ISOFDM(rspec)) {
			bcmerror = BCME_RANGE;
			goto done;
		}
	} else if (rspec != 0) {
		bcmerror = BCME_RANGE;
		goto done;
	}

	/* Validate a BW override if given
	 *
	 * Make sure the phy is capable
	 * Only VHT for 80MHz/80+80Mhz/160Mhz
	 * VHT/HT/OFDM for 40MHz, not CCK/DSSS
	 * MCS32 cannot be 20MHz, always 40MHz
	 */

	if (bw == WL_RSPEC_BW_160MHZ) {
		if (!(WLC_8080MHZ_CAP_PHY(wlc) || WLC_160MHZ_CAP_PHY(wlc))) {
			bcmerror = BCME_RANGE;
			goto done;
		}
		/* HE (corerev > 128) & VHT & legacy mode can be specified for > 80MHz */
		if (!isvht && !islegacy && !(D11REV_GE(wlc->pub->corerev, 129) && ishe)) {
			bcmerror = BCME_BADARG;
			goto done;
		}
	} else if (bw == WL_RSPEC_BW_80MHZ) {
		if (!WLC_80MHZ_CAP_PHY(wlc)) {
			bcmerror = BCME_RANGE;
			goto done;
		}
		/* HE, VHT & legacy mode can be specified for >40MHz */
		if (!isvht && !islegacy && !ishe) {
			bcmerror = BCME_BADARG;
			goto done;
		}
	} else if (bw == WL_RSPEC_BW_40MHZ) {
		if (!WLC_40MHZ_CAP_PHY(wlc)) {
			bcmerror = BCME_RANGE;
			goto done;
		}
		/* DSSS/CCK are only 20MHz */
		if (RSPEC_ISCCK(rspec)) {
			bcmerror = BCME_BADARG;
			goto done;
		}
	} else if (bw == WL_RSPEC_BW_20MHZ) {
		/* MCS 32 is 40MHz, cannot be forced to 20MHz */
		if (isht && (rspec & WL_RSPEC_HT_MCS_MASK) == 32) {
			bcmerror = BCME_BADARG;
		}
	}

	/* more error checks if the rate is not 0 == 'auto' */
	if (rspec != 0) {
		/* add the override flags */
		rspec = (rspec | WL_RSPEC_OVERRIDE_RATE | WL_RSPEC_OVERRIDE_MODE);

		/* make sure there are enough tx streams available for what is specified */
		if ((Nsts + txexp) > wlc->stf->op_txstreams) {
			WL_TRACE(("wl%d: %s: err, (Nsts %d + txexp %d) > op_txstreams %d\n",
			          wlc->pub->unit, __FUNCTION__, Nsts, txexp,
			          wlc->stf->op_txstreams));
			bcmerror = BCME_RANGE;
			goto done;
		}

		if ((rspec != 0) && !wlc_valid_rate(wlc, rspec, band->bandtype, TRUE)) {
			WL_TRACE(("wl%d: %s: err, failed wlc_valid_rate()\n",
			          wlc->pub->unit, __FUNCTION__));
			bcmerror = BCME_RANGE;
			goto done;
		}
	}

	/* set the rspec_override/mrspec_override for the given band */
	if (mcast) {
		band->mrspec_override = rspec;
	} else {
		band->rspec_override = rspec;
#if defined(WLAMPDU) && defined(WLATF)
	/* Let ATF know we are overriding the rate */
		wlc_ampdu_atf_rate_override(wlc, rspec, band);
#endif // endif
#ifdef WLAMSDU_TX
		if (AMSDU_TX_ENAB(wlc->pub)) {
			wlc_amsdu_max_agg_len_upd(wlc->ami);
		}
#endif /* WLAMSDU_TX */
#ifdef WLTAF
		wlc_taf_rate_override(wlc->taf_handle, rspec, band);
#endif // endif
	}

	if (!mcast) {
		wlc_reprate_init(wlc); /* reported rate initialization */
#if defined(WLAMPDU)
		/* Recalculate per-scb max_pdu size after rate overriding */
		scb_ampdu_update_config_all(wlc->ampdu_tx);
		/* txcache already invalidated */
		goto done;
#endif /* WLAMPDU */
	}

	/* invalidate txcache as transmit rate has changed */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_inv_all(wlc->txc);

done:
	return bcmerror;
} /* wlc_set_ratespec_override */

/** per bsscfg init tx reported rate mechanism */
void
wlc_bsscfg_reprate_init(wlc_bsscfg_t *bsscfg)
{
	bsscfg->txrspecidx = 0;
	bzero((char*)bsscfg->txrspec, sizeof(bsscfg->txrspec));
}

/**
 * Loop over bsscfg specific txrspec history, looking up rate bins, and summing
 * nfrags into appropriate supported rate bin. Return pointers to
 * most used ratespec and highest used ratespec.
 */
static void
wlc_get_rate_histo_bsscfg(wlc_bsscfg_t *bsscfg, wlc_mac_ratehisto_res_t *rhist,
	ratespec_t *most_used_ratespec, ratespec_t *highest_used_ratespec)
{
	int i;
	ratespec_t rspec;
	uint max_frags = 0;
	uint rate, mcs, nss;
	uint high_rate = 0;

	*most_used_ratespec = 0x0;
	*highest_used_ratespec = 0x0;

	/* [0] = rspec, [1] = nfrags */
	for (i = 0; i < NTXRATE; i++) {
		rspec = bsscfg->txrspec[i][0]; /* circular buffer of prev MPDUs tx rates */
		/* skip empty rate specs */
		if (rspec == 0)
			continue;

		if (RSPEC_ISHE(rspec)) {
			mcs = rspec & WL_RSPEC_HE_MCS_MASK;
			nss = ((rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT) - 1;
			ASSERT(mcs < WL_RATESET_SZ_HE_MCS);
			ASSERT(nss < WL_TX_CHAINS_MAX);
			rhist->he[mcs][nss] += bsscfg->txrspec[i][1]; /* [1] is for fragments */
			if (rhist->he[mcs][nss] > max_frags) {
				max_frags = rhist->he[mcs][nss];
				*most_used_ratespec = rspec;
			}
		} else if (RSPEC_ISVHT(rspec)) {
			mcs = rspec & WL_RSPEC_VHT_MCS_MASK;
			nss = ((rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT) - 1;
			ASSERT(mcs < WL_RATESET_SZ_VHT_MCS_P);
			ASSERT(nss < WL_TX_CHAINS_MAX);
			rhist->vht[mcs][nss] += bsscfg->txrspec[i][1]; /* [1] is for fragments */
			if (rhist->vht[mcs][nss] > max_frags) {
				max_frags = rhist->vht[mcs][nss];
				*most_used_ratespec = rspec;
			}
		} else if (RSPEC_ISHT(rspec)) {
			mcs = rspec & WL_RSPEC_HT_MCS_MASK;
			if (IS_PROPRIETARY_11N_MCS(mcs)) {
				mcs -= WLC_11N_FIRST_PROP_MCS;
				rhist->prop11n_mcs[mcs] += bsscfg->txrspec[i][1];
				if (rhist->prop11n_mcs[mcs] > max_frags) {
					max_frags = rhist->prop11n_mcs[mcs];
					*most_used_ratespec = rspec;
				}
			} else {
				/* ASSERT(mcs < WL_RATESET_SZ_HT_IOCTL * WL_TX_CHAINS_MAX); */
				if (mcs >= WL_RATESET_SZ_HT_IOCTL * WL_TX_CHAINS_MAX)
					continue;  /* Ignore mcs 32 if it ever comes through. */
				rhist->mcs[mcs] += bsscfg->txrspec[i][1];
				if (rhist->mcs[mcs] > max_frags) {
					max_frags = rhist->mcs[mcs];
					*most_used_ratespec = rspec;
				}
			}
		} else {
			ASSERT(RSPEC_ISLEGACY(rspec));
			rate = RSPEC2RATE(rspec);
			ASSERT(rate < (WLC_MAXRATE + 1));
			rhist->rate[rate] += bsscfg->txrspec[i][1];
			if (rhist->rate[rate] > max_frags) {
				max_frags = rhist->rate[rate];
				*most_used_ratespec = rspec;
			}
		}

		rate = RSPEC2KBPS(rspec);
		if (rate > high_rate) {
			high_rate = rate;
			*highest_used_ratespec = rspec;
		}
	}
	return;
}

wlc_rspec_info_t *
BCMATTACHFN(wlc_rspec_attach)(wlc_info_t *wlc)
{
	wlc_rspec_info_t *rsi;

	/* allocate module info */
	if ((rsi = MALLOCZ(wlc->osh, sizeof(*rsi))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	rsi->wlc = wlc;

	if (wlc_module_register(wlc->pub, rspec_iovars, "rspec", rsi, wlc_rspec_doiovar,
			NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return rsi;

fail:
	return NULL;
}

void
BCMATTACHFN(wlc_rspec_detach)(wlc_rspec_info_t *rsi)
{
	wlc_info_t *wlc;

	if (rsi == NULL)
		return;

	wlc = rsi->wlc;

	(void)wlc_module_unregister(wlc->pub, "rspec", rsi);

	MFREE(wlc->osh, rsi, sizeof(*rsi));
}

/**
 * Lookup table for Frametypes [LEGACY (CCK / OFDM) /HT/VHT/HE]
 */
const uint8 wlc_rspec_enc2ft_tbl[] = {FT_LEGACY, FT_HT, FT_VHT, FT_HE};
