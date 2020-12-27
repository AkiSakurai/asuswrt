/*
 * WLC OID module  of
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_hostoid.c 467527 2014-04-03 13:48:29Z $*
 *
 */

/**
 * @file
 * @brief
 * NDIS related feature. Moves OID processing code to host to save dongle RAM.
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
#include <proto/eap.h>
#include <proto/eapol.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_phy_hal.h>
#include <wl_export.h>
#include <wlc_scan.h>
#include <wlc_hostoid.h>
#include <wlc_ap.h>
#include <bcmendian.h>
#include <wlc_assoc.h>
#ifdef BCMSUP_PSK
#include <wlc_sup.h>
#endif
#include <wlc_pmkid.h>
#include <wlc_prot_n.h>

static int wlc_hostoid_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
enum {
	IOV_DEFAULT_BSS_INFO,
#ifdef WLCNT
	IOV_STATISTICS,
#endif /* WLCNT */
	IOV_RADIO_MPC_UPD,
	IOV_SET_NMODE,
	IOV_KEY_ABSENT,
	IOV_SCAN_IN_PROGRESS,
	IOV_BCN_PRB,
	IOV_PMKID_EVENT,
	IOV_PROT_N_MODE_RESET,
	IOV_LAST        /* In case of a need to check max ID number */
};

const bcm_iovar_t hostoid_iovars[] = {
	{"default_bss_info", IOV_DEFAULT_BSS_INFO,
	(0), IOVT_BUFFER, sizeof(wl_bss_config_t)
	},
#ifdef WLCNT
	{"statistics", IOV_STATISTICS,
	(0), IOVT_BUFFER, sizeof(wl_cnt_t)
	},
#endif /* WLCNT */
	{"mpc_upd", IOV_RADIO_MPC_UPD,
	(0), IOVT_BOOL, 0
	},
	{"set_nmode", IOV_SET_NMODE,
	(0), IOVT_UINT32, 0
	},
	{"key_absent", IOV_KEY_ABSENT,
	(0), IOVT_BOOL, 0
	},
	{"scan_in_progress", IOV_SCAN_IN_PROGRESS,
	(0), IOVT_BOOL, 0
	},
	{"bcn_prb", IOV_BCN_PRB,
	(0), IOVT_BUFFER, sizeof(int)
	},
	{"pmkid_event", IOV_PMKID_EVENT,
	(0), IOVT_UINT32, 0
	},
	{"prot_n_mode_reset", IOV_PROT_N_MODE_RESET,
	(0), IOVT_UINT32, 0
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

	if ((err = wlc_iovar_check(wlc, vi, arg, len, IOV_ISSET(actionid), wlcif)) != 0)
		return err;
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

	ASSERT(bsscfg != NULL);

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {

		case IOV_GVAL(IOV_DEFAULT_BSS_INFO): {
			wl_bss_config_t default_bss;
			bzero(&default_bss, sizeof(wl_bss_config_t));
			default_bss.beacon_period = (uint32)wlc->default_bss->beacon_period;
			default_bss.beacon_period = (uint32)wlc->default_bss->atim_window;
			default_bss.chanspec = (uint32)wlc->default_bss->chanspec;
			bcopy(&default_bss, (wl_bss_config_t *)arg, sizeof(wl_bss_config_t));
			break;
		}

		case IOV_SVAL(IOV_DEFAULT_BSS_INFO): {
			wl_bss_config_t *default_bss = (wl_bss_config_t *)arg;
			wlc->default_bss->beacon_period = (uint16)default_bss->beacon_period;
			wlc->default_bss->atim_window = (uint16)default_bss->atim_window;
			wlc->default_bss->chanspec = (chanspec_t)default_bss->chanspec;
			break;
		}

#ifdef WLCNT
		case IOV_GVAL(IOV_STATISTICS): {
			wlc_statsupd(wlc);
			bcopy(&wlc->pub->_cnt, arg, sizeof(wl_cnt_t));
			break;
		}
#endif /* WLCNT */

		case IOV_SVAL(IOV_RADIO_MPC_UPD):
			wlc->mpc_oidjoin = bool_val;
			wlc_radio_mpc_upd(wlc);
			break;

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
				key = wlc->wsec_keys[i];
				if (key && key->len > 0 &&
				    !bcmp(&bsscfg->BSSID, key->ea.octet, ETHER_ADDR_LEN)) {
					*ret_int_ptr = 0;
					break;
				}
			}
			break;
		}

		case IOV_GVAL(IOV_SCAN_IN_PROGRESS): {
			*ret_int_ptr = SCAN_IN_PROGRESS(wlc->scan);
			break;
		}

		case IOV_GVAL(IOV_BCN_PRB):
			if ((uint32)len < sizeof(uint32) + bsscfg->current_bss->bcn_prb_len) {
				return BCME_BUFTOOSHORT;
			}
			bcopy(&bsscfg->current_bss->bcn_prb_len, (char*)arg, sizeof(uint32));
			bcopy((char*)bsscfg->current_bss->bcn_prb, (char*)arg + sizeof(uint32),
				bsscfg->current_bss->bcn_prb_len);
			printf("bcn_prb_len: %d\n", bsscfg->current_bss->bcn_prb_len);
			prhex("bcn_prb", (void*)bsscfg->current_bss->bcn_prb,
				bsscfg->current_bss->bcn_prb_len);
			break;

		case IOV_SVAL(IOV_PMKID_EVENT): {
				wlc_pmkid_cache_req(wlc->pmkid_info, bsscfg);
			break;
		}

		case IOV_SVAL(IOV_PROT_N_MODE_RESET): {
			wlc_prot_n_mode_reset(wlc->prot_n, (bool)int_val);
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
