/*
 * WLC OID module  of
 * Broadcom 802.11bang Networking Device Driver
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
 * $Id: wlc_hostoid.c 739670 2018-01-09 08:34:52Z $*
 *
 */

/**
 * @file
 * @brief
 * NDIS related feature. Moves OID processing code to host to save dongle RAM.
 */

/**
 * @file
 * @brief
 * XXX Twiki: [HostOidProcessing]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>

#ifndef WLC_HOSTOID
#error "Cannot use this file without WLC_HOSTOID defined"
#endif /* WLC_HOSTOID */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <proto/802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_phy_hal.h>
#include <wl_export.h>
#include <wlc_hostoid.h>
#include <wlc_ap.h>
#include <wlc_assoc.h>
#include <bcmendian.h>
#ifdef BCMSUP_PSK
#include <wlc_sup.h>
#endif // endif
#include <wlc_prot_n.h>

static int wlc_hostoid_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
enum {
#ifdef WLCNT
	IOV_STATISTICS,
#endif /* WLCNT */
	IOV_SET_NMODE,
	IOV_KEY_ABSENT,
	IOV_PROT_N_MODE_RESET,
	IOV_SET_SSID,
	IOV_LAST        /* In case of a need to check max ID number */
};
const bcm_iovar_t hostoid_iovars[] = {
#ifdef WLCNT
	{"statistics", IOV_STATISTICS,
	(0), IOVT_BUFFER, sizeof(wl_cnt_t)
	},
#endif /* WLCNT */
	{"set_nmode", IOV_SET_NMODE,
	(0), IOVT_UINT32, 0
	},
	{"key_absent", IOV_KEY_ABSENT,
	(0), IOVT_BOOL, 0
	},
	{"prot_n_mode_reset", IOV_PROT_N_MODE_RESET,
	(0), IOVT_UINT32, 0
	},
	{ "set_ssid", IOV_SET_SSID,
	(0), IOVT_BUFFER, sizeof(wlc_ssid_t)
	},
	{NULL, 0, 0, 0, 0}
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

int
BCMATTACHFN(wlc_hostoid_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	if (wlc_module_register(pub, hostoid_iovars, "hostoid", wlc, wlc_hostoid_doiovar,
		NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: hostoid wlc_module_register() failed\n", pub->unit));
		return BCME_ERROR;
	}

	return BCME_OK;
}

int
BCMATTACHFN(wlc_hostoid_detach)(wlc_info_t *wlc)
{
	wlc_module_unregister(wlc->pub, "hostoid", wlc);
	return BCME_OK;
}

static int
wlc_hostoid_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_info_t * wlc = (wlc_info_t *)hdl;
	int err = BCME_OK;
	bool bool_val;
	int32 int_val = 0;
	wlc_bsscfg_t *bsscfg;
	int32 *ret_int_ptr;
	wlc_bss_info_t *current_bss = NULL;

	BCM_REFERENCE(bool_val);
	if ((err = wlc_iovar_check(wlc, vi, arg, len, IOV_ISSET(actionid), wlcif)) != 0)
		return err;
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	ASSERT(bsscfg != NULL);

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
#ifdef WLCNT
		case IOV_GVAL(IOV_STATISTICS): {
			wlc_statsupd(wlc);
			bcopy(&wlc->pub->_cnt, arg, sizeof(wl_cnt_t));
			break;
		}
#endif /* WLCNT */
#ifdef WL11N
		case IOV_SVAL(IOV_SET_NMODE):
			err = wlc_set_nmode(wlc, int_val);
			break;
#endif /* WL11N */

		case IOV_GVAL(IOV_KEY_ABSENT): {
			wsec_key_t *key = NULL;
			int i;
			*ret_int_ptr = 1;
			for (i = DOT11_MAX_DEFAULT_KEYS; i < WSEC_MAX_KEYS; i++) {
				key = WSEC_KEY(wlc, i);
				if (key && key->len > 0 &&
				    !bcmp(&bsscfg->BSSID, key->ea.octet, ETHER_ADDR_LEN)) {
					*ret_int_ptr = 0;
					break;
				}
			}
			break;
		}

		case IOV_SVAL(IOV_PROT_N_MODE_RESET): {
			wlc_prot_n_mode_reset(wlc->prot_n, (bool)int_val);
			break;
		}

		case IOV_SVAL(IOV_SET_SSID): {
			wlc_ssid_t ssid_info;
			int i;
			wlc_bss_info_t *current_bss = NULL;
#ifdef BCMDBG
			char ssidbuf[256];
#endif // endif
			memcpy(&ssid_info, (wlc_ssid_t *)arg, sizeof(wlc_ssid_t));
			if (ssid_info.SSID_len == 32 && (bsscfg->BSS == 1)) {
				/* search non-control chars */
				for (i = 0; i < 32; i++) {
					if (ssid_info.SSID[i] >= 0x20)
						break;
				}

				if (i == 32) {
					WL_OID(
						("wl%d: %s: got a bogus SSID, disassociating\n",
						wlc->pub->unit, __FUNCTION__));

					/* do a disassociation instead of an SSID set */
					wlc_ioctl(wlc, WLC_DISASSOC, NULL, 0, NULL);
					wlc->mpc_oidjoin = FALSE;
					wlc_radio_mpc_upd(wlc);
					break;
				}
			}
			if (bsscfg->associated == 1 && !bsscfg->BSS) {
				current_bss = wlc_get_current_bss(bsscfg);
				if (ssid_info.SSID_len == current_bss->SSID_len &&
					!bcmp(ssid_info.SSID, (char*)current_bss->SSID,
					ssid_info.SSID_len) &&
					(((current_bss->capability & DOT11_CAP_PRIVACY) ==
					(wlc->default_bss->capability & DOT11_CAP_PRIVACY)) &&
					current_bss->beacon_period ==
					wlc->default_bss->beacon_period &&
					current_bss->atim_window ==
					wlc->default_bss->atim_window &&
					current_bss->chanspec ==
					wlc->default_bss->chanspec)) {
					WL_OID(("\tNew SSID is the same as current, ignoring.\n"));
					break;
				}
			}
			wlc->mpc_oidjoin = TRUE;
			wlc_radio_mpc_upd(wlc);

			/* if can't up, we're done */
			if (!wlc->pub->up) {
				wlc->mpc_oidjoin = FALSE;
				wlc_radio_mpc_upd(wlc);
				break;
			}
			/* attempt to join a BSS with the requested SSID */
			/* but don't create an IBSS if IBSS Lock Out is turned on */
			if (!((wlc->ibss_allowed == FALSE) && (bsscfg->BSS == 0))) {
				wl_join_assoc_params_t *assoc_params = NULL;
				int assoc_params_len = 0;

#ifdef BCMDBG
				bcm_format_ssid(ssidbuf, ssid_info.SSID, ssid_info.SSID_len);
				WL_OID(("wl%d: %s: set ssid %s\n", wlc->pub->unit,
					__FUNCTION__, ssidbuf));
#endif // endif
				if ((uint)len >= WL_JOIN_PARAMS_FIXED_SIZE) {
					bool reset_chanspec_num = FALSE;
					uint16 bssid_cnt;
					int32 chanspec_num;

					assoc_params = &((wl_join_params_t *)arg)->params;
					assoc_params_len = len - OFFSETOF(wl_join_params_t, params);
					bssid_cnt = load16_ua(&assoc_params->bssid_cnt);
					chanspec_num = load32_ua(&assoc_params->chanspec_num);
					if (bssid_cnt && (chanspec_num == 0))
					{
						reset_chanspec_num = TRUE;
						store32_ua((uint8 *)&assoc_params->chanspec_num,
							bssid_cnt);
					}

					err = wlc_assoc_chanspec_sanitize(wlc,
						(chanspec_list_t *)&assoc_params->chanspec_num,
						len - OFFSETOF(wl_join_params_t,
						params.chanspec_num));

					if (reset_chanspec_num)
						store32_ua((uint8 *)&assoc_params->chanspec_num, 0);

					if (err != BCME_OK)
						break;
				}
				wlc_join(wlc, wlc->cfg, ssid_info.SSID, ssid_info.SSID_len,
					NULL,
					assoc_params, assoc_params_len);
			}

			wlc->mpc_oidjoin = FALSE;
			wlc_radio_mpc_upd(wlc);
			break;
		}

		default:
			/* cant remove this variable because it's defined in the
			 * preamble (the IOVAR handler that was using it was removed)
			 * but have to access to prevent 'unused variable' errors.
			 */
			(void)current_bss;

			err = BCME_UNSUPPORTED;
			break;
	}

	return err;
}
