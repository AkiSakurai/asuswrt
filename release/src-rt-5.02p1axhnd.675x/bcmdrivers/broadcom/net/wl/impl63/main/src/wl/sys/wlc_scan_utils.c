/**
 * @file
 * @brief
 * scan related wrapper routines and scan results related functions
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_scan_utils.c 781150 2019-11-13 07:11:32Z $
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <hndd11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_rsdb.h>
#include <wlc_p2p.h>
#include <wlc_event.h>
#include <wl_export.h>
#include <wlc_rm.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#ifdef WLPFN
#include <wl_pfn.h>
#endif // endif
#include <wlc_lq.h>
#include <wlc_macfltr.h>
#include <wlc_ie_mgmt.h>
#include <wlc_ie_mgmt_ft.h>
#include <wlc_ie_mgmt_vs.h>
#include <wlc_ie_helper.h>
#ifdef ANQPO
#include <wl_anqpo.h>
#endif // endif
#include <wlc_rx.h>
#include <wlc_dbg.h>
#include <wlc_event_utils.h>
#include <wl_dbg.h>
#include <wlc_scan_utils.h>
#include <wlc_11d.h>
#include <wlc_iocv.h>
#include <phy_noise_api.h>
#ifdef WL_SHIF
#include <wl_shub.h>
#endif // endif
#ifdef WL_SCAN_DFS_HOME
#include <wlc_ap.h>
#include <wlc_dfs.h>
#endif // endif

/* iovar table */
enum {
	IOV_SCANABORT = 0,
	IOV_SCANRESULTS_MINRSSI = 1,
	IOV_ISCAN = 2,
	IOV_ISCANRESULTS = 3,
	IOV_ESCAN = 4,
	IOV_CACHE_TX_FRAME = 5,
	IOV_LAST		/* In case of a need to check max ID number */
};

static const bcm_iovar_t scan_utils_iovars[] = {
#ifdef STA
	{"escan", IOV_ESCAN, 0, 0, IOVT_BUFFER, WL_ESCAN_PARAMS_FIXED_SIZE},
#endif // endif
	{"scanabort", IOV_SCANABORT, 0, 0, IOVT_VOID, 0},
#ifdef STA
	{"scanresults_minrssi", IOV_SCANRESULTS_MINRSSI, (IOVF_RSDB_SET), 0, IOVT_INT32, 0},
	{"iscan", IOV_ISCAN, 0, 0, IOVT_BUFFER, WL_ISCAN_PARAMS_FIXED_SIZE},
	{"iscanresults", IOV_ISCANRESULTS, 0, 0, IOVT_BUFFER, WL_ISCAN_RESULTS_FIXED_SIZE},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

/* local function declarations */
static int wlc_scan_register_iem_fns(wlc_info_t *wlc);
static int wlc_scan_utils_wlc_down(void *ctx);
static int wlc_scan_utils_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len,
	uint val_size, struct wlc_if *wlcif);
static int wlc_scan_utils_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif);

#ifdef STA
static void wlc_iscan_timeout(void *arg);
static int wlc_BSSignorelookup(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec,
                               uchar ssid[], uint ssid_len, bool add);
static bool wlc_BSSignore(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec,
                          uchar ssid[], uint ssid_len);
static bool wlc_BSSIgnoreAdd(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec, uchar ssid[],
                    uint ssid_len);
#endif // endif

static wlc_bss_info_t *wlc_BSSadd(wlc_info_t *wlc);
static wlc_bss_info_t *wlc_BSSlookup(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec,
	uchar ssid[], uint ssid_len);

#ifdef STA
static void wlc_scan_utils_scan_data_notif(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_management_header *hdr, uint8 *body, int body_len, wlc_bss_info_t *bi);
#endif // endif

static int wlc_custom_scan_results(wlc_info_t *wlc, wl_scan_results_t *iob, uint *len,
	uint results_state);

#ifdef WL_SHIF
void wlc_scan_utils_shubscan_complete(void *arg, int status, wlc_bsscfg_t *cfg);
#endif // endif

/* handy macros for Incremental Scan */
#define ISCAN_SUCCESS(sui)  \
		(sui->custom_iscan_results_state == WL_SCAN_RESULTS_SUCCESS)
#define ISCAN_PARTIAL(sui)  \
		(sui->custom_iscan_results_state == WL_SCAN_RESULTS_PARTIAL)
#define ISCAN_ABORTED(sui)  \
		(sui->custom_iscan_results_state == WL_SCAN_RESULTS_ABORTED)
#define ISCAN_RESULTS_OK(sui) (ISCAN_SUCCESS(sui) || ISCAN_PARTIAL(sui))

/* support 100 ignore list items */
#define WLC_ISCAN_IGNORE_LIST_SZ	1200
#define WLC_ISCAN_IGNORE_MAX	(WLC_ISCAN_IGNORE_LIST_SZ / sizeof(iscan_ignore_t))

#define IGNORE_LIST_MATCH_SZ	OFFSETOF(iscan_ignore_t, chanspec)

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

/** attach/detach */
wlc_scan_utils_t *
BCMATTACHFN(wlc_scan_utils_attach)(wlc_info_t *wlc)
{
	wlc_scan_utils_t *sui;

	/* allocate module info */
	if ((sui = MALLOCZ(wlc->osh, sizeof(*sui))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	sui->wlc = wlc;

	sui->cmn = (struct wlc_scan_utils_cmn*) obj_registry_get(wlc->objr, OBJR_SCANUTILS_CMN);

	if (sui->cmn == NULL) {
		if ((sui->cmn =  (struct wlc_scan_utils_cmn*)
			MALLOCZ(wlc->osh, sizeof(struct wlc_scan_utils_cmn))) == NULL) {
			WL_ERROR(("wl%d: %s: wlc_scan_utils_cmn alloc falied\n",
				wlc->pub->unit, __FUNCTION__));
			goto fail;
		}
		/* OBJECT REGISTRY: We are the first instance, store value for key */
		obj_registry_set(wlc->objr, OBJR_SCANUTILS_CMN, sui->cmn);
	}
	(void)obj_registry_ref(wlc->objr, OBJR_SCANUTILS_CMN);

	/* allocate fixed portion of scan results */
	if ((wlc->scan_results = MALLOCZ(wlc->osh, sizeof(wlc_bss_list_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	if ((sui->custom_scan_results = MALLOCZ(wlc->osh, sizeof(wlc_bss_list_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

#ifdef STA
	/* ISCAN timer */
	if (!(sui->iscan_timer = wl_init_timer(wlc->wl, wlc_iscan_timeout, sui, "iscan"))) {
		WL_ERROR(("wl%d: iscan_timeout failed\n", wlc->pub->unit));
		goto fail;
	}
#endif // endif

	/* register IE build/parse callbacks for bcn/prbresp used during scan */
	if (wlc_scan_register_iem_fns(wlc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_scan_register_iem_fns failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register ioctl/iovar dispatchers and other module entries */
	if (wlc_module_add_ioctl_fn(wlc->pub, sui, wlc_scan_utils_doioctl, 0, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* create notification list for scan data */
	if ((bcm_notif_create_list(wlc->notif, &sui->scan_data_h)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: bcm_notif_create_list() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_register(wlc->pub, scan_utils_iovars, "scan_utils", sui,
			wlc_scan_utils_doiovar, NULL, NULL, wlc_scan_utils_wlc_down)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	sui->cmn->escan_sync_id = WLC_DEFAULT_SYNCID;

	return sui;

fail:
	MODULE_DETACH(sui, wlc_scan_utils_detach);
	return NULL;
} /* wlc_scan_utils_attach */

void
BCMATTACHFN(wlc_scan_utils_detach)(wlc_scan_utils_t *sui)
{
	wlc_info_t *wlc;

	if (sui == NULL)
		return;

	wlc = sui->wlc;

	if (sui->iscan_ignore_list) {
		MFREE(wlc->osh, sui->iscan_ignore_list, WLC_ISCAN_IGNORE_LIST_SZ);
	}

	(void)wlc_module_unregister(wlc->pub, "scan_utils", sui);
	(void)wlc_module_remove_ioctl_fn(wlc->pub, sui);

#ifdef STA
	if (sui->iscan_timer) {
		wl_free_timer(wlc->wl, sui->iscan_timer);
	}
#endif // endif

	if (wlc->scan_results) {
		wlc_bss_list_free(wlc, wlc->scan_results);
		MFREE(wlc->osh, wlc->scan_results, sizeof(wlc_bss_list_t));
	}

	if (sui->custom_scan_results) {
		wlc_bss_list_free(wlc, sui->custom_scan_results);
		MFREE(wlc->osh, sui->custom_scan_results, sizeof(wlc_bss_list_t));
	}

	if (obj_registry_unref(wlc->objr, OBJR_SCANUTILS_CMN) == 0) {
		obj_registry_set(wlc->objr, OBJR_SCANUTILS_CMN, NULL);
		if (sui) {
			MFREE(wlc->osh, sui->cmn, sizeof(struct wlc_scan_utils_cmn));
		}
	}

	if (sui->scan_data_h) {
		bcm_notif_delete_list(&sui->scan_data_h);
	}

	MFREE(wlc->osh, sui, sizeof(*sui));
} /* wlc_scan_utils_detach */

/** wlc up/down */
static int
wlc_scan_utils_wlc_down(void *ctx)
{
#ifdef STA
	wlc_scan_utils_t *sui = ctx;
	wlc_info_t *wlc = sui->wlc;
#endif // endif
	int callbacks = 0;

#ifdef STA
	if (!wl_del_timer(wlc->wl, sui->iscan_timer))
		callbacks++;
#endif // endif

	return callbacks;
}

/** "extended" scan request */
int
wlc_scan_request_ex(
	wlc_info_t *wlc,
	int bss_type,
	const struct ether_addr* bssid,
	int nssid,
	wlc_ssid_t *ssids,
	int scan_type,
	int nprobes,
	int active_time,
	int passive_time,
	int home_time,
	const chanspec_t* chanspec_list,
	int chanspec_num,
	chanspec_t chanspec_start,
	bool save_prb,
	scancb_fn_t fn, void* arg,
	int macreq,
	uint scan_flags,
	wlc_bsscfg_t *cfg,
	actcb_fn_t act_cb, void *act_cb_arg)
{
	int err = 0;
	bool cb = TRUE;

	ASSERT((ssids != NULL) && (nssid != 0));
	ASSERT(ssids->SSID_len <= DOT11_MAX_SSID_LEN);

	if ((err = wlc_mac_request_entry(wlc, NULL, macreq, scan_flags)) == BCME_OK) {
		uint8 usage;
		struct ether_addr *sa_override = NULL;

		switch (macreq) {
		case WLC_ACTION_SCAN:
		case WLC_ACTION_ISCAN:
		case WLC_ACTION_PNOSCAN:
			usage = SCAN_ENGINE_USAGE_NORM;
			break;
		case WLC_ACTION_ESCAN:
			usage = SCAN_ENGINE_USAGE_ESCAN;
			break;

		default:
			return BCME_BADARG;
		}

		if (!BSS_P2P_DISC_ENAB(wlc, cfg)) {
			/* override source MAC */
			sa_override = wlc_scanmac_get_mac(wlc->scan, macreq, cfg);
		}

#ifdef WL_SCAN_DFS_HOME
		err = wlc_scan_on_dfs_chan(wlc, chanspec_num,
				(chanspec_list ? chanspec_list[0] : 0), scan_type,
				&active_time, &passive_time);

		if (err == BCME_SCANREJECT) {
			WL_ERROR(("wl%d : %s cannot scan due to err %d\n", wlc->pub->unit,
					__FUNCTION__, err));
			return err;
		}
#endif /* WL_SCAN_DFS_HOME */
		if (fn != NULL)
			wlc_scan_set_request_ex_cb(wlc->scan, fn);

		err = wlc_scan(wlc->scan, bss_type, bssid, nssid, ssids,
		               scan_type, nprobes, active_time, passive_time, home_time,
		               chanspec_list, chanspec_num, chanspec_start, save_prb,
		               wlc_scan_request_ex_cb, arg, 0, FALSE, FALSE,
		               SCANCACHE_ENAB(wlc->scan), scan_flags, cfg, usage,
		               act_cb, act_cb_arg, sa_override);
		/* wlc_scan() invokes 'fn' even in error cases */
		cb = FALSE;
	} else {
		WL_INFORM(("wl%d: scan is blocked by others\n", wlc->pub->unit));
	}

	/* make it consistent with wlc_scan() w.r.t. invoking callback */
	if (cb && fn != NULL) {
		WL_SCAN(("wl%d: %s, can not scan due to error %d\n",
		          wlc->pub->unit, __FUNCTION__, err));
		wlc_scan_request_ex_cb(arg, WLC_E_STATUS_ERROR, cfg);
	}

	return err;
} /* wlc_scan_request_ex */

/** custom scan (user scan) */
void
wlc_custom_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	wlc_scan_utils_t *sui = wlc->sui;
#if defined(STA)
	int i;
#endif // endif

	/* This scan is blocked by other existing scan of equal or higher priority */
	/* Do this at the beginning of this function to avoid sending unnecessary events */
	if (status == WLC_E_STATUS_ERROR) {
		WL_ERROR(("wl%d: %s - scan blocked by others\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

#ifdef STA
	if (ESCAN_IN_PROGRESS(wlc->scan)) {
		wl_escan_result_t escan_result;

		escan_result.sync_id = sui->cmn->escan_sync_id;
		escan_result.version = WL_BSS_INFO_VERSION;
		escan_result.bss_count = 0;
		escan_result.buflen = WL_ESCAN_RESULTS_FIXED_SIZE;

		ASSERT(status != WLC_E_STATUS_PARTIAL);
		wlc_bss_mac_event(wlc, cfg, WLC_E_ESCAN_RESULT, NULL, status,
		                  0, 0, &escan_result, escan_result.buflen);
	} else if (wlc->pub->scanresults_on_evtcplt) {
		/* copy all bss to scan_result and return to per-port layer */
		wl_scan_results_t *scan_result;
		uint32 scan_result_len = 0, len_left = 0;
		wl_bss_info_t *wl_bi;

		/* find total length */
		scan_result_len = OFFSETOF(wl_scan_results_t, bss_info);
		for (i = 0; i < wlc->scan_results->count; i++) {
			scan_result_len += sizeof(wl_bss_info_t);
			if (wlc->scan_results->ptrs[i]->bcn_prb_len > DOT11_BCN_PRB_LEN) {
				scan_result_len +=
					(ROUNDUP(wlc->scan_results->ptrs[i]->bcn_prb_len -
					DOT11_BCN_PRB_LEN, 4));
			}
		}
		if ((scan_result = (wl_scan_results_t*)MALLOC(wlc->osh, scan_result_len)) ==
			NULL) {
			WL_ERROR(("wl%d: %s: Can't allocate scan_result\n", wlc->pub->unit,
				__FUNCTION__));
			return;
		}

		wl_bi = (wl_bss_info_t*)scan_result->bss_info;
		len_left = scan_result_len;

		for (i = 0; i < wlc->scan_results->count; i++) {
			wlc_bss_info_t *bssinfo = wlc->scan_results->ptrs[i];
			/* convert to wl_bss_info */
			if (wlc_bss2wl_bss(wlc, bssinfo, wl_bi, len_left, TRUE) != BCME_OK) {
				/* we should have allocated enough memory above */
				/* if fires, buf length needs to sync with wlc_bss2wl_bss */
				ASSERT(0);
				break;
			}
			/* update next scan_result->bss_info */
			len_left -= wl_bi->length;
			wl_bi = (wl_bss_info_t *)((char *)wl_bi + wl_bi->length);
		}
		scan_result->count = i;

		wlc_bss_mac_event(wlc, cfg, WLC_E_SCAN_COMPLETE, NULL, status, 0, 0,
			scan_result, scan_result_len);

		MFREE(wlc->osh, scan_result, scan_result_len);
	} else {
		wlc_bss_mac_event(wlc, cfg, WLC_E_SCAN_COMPLETE, NULL, status, 0, 0, 0, 0);
	}
	if (ISCAN_IN_PROGRESS(wlc)) {
		wl_del_timer(wlc->wl, sui->iscan_timer);
	}
#endif /* STA */

	if (status == WLC_E_STATUS_PARTIAL)
		status = WL_SCAN_RESULTS_PARTIAL;
	else if (status != WLC_E_STATUS_SUCCESS) {
		if (ISCAN_IN_PROGRESS(wlc))
			sui->custom_iscan_results_state = WL_SCAN_RESULTS_ABORTED;
		else
			sui->custom_scan_results_state = WL_SCAN_RESULTS_ABORTED;
		return;
	}

	/* release old BSS information */
	wlc_bss_list_free(wlc, sui->custom_scan_results);

	/* copy scan results to custom_scan_results for reporting via ioctl */
	wlc_bss_list_xfer(wlc->scan_results, sui->custom_scan_results);

	if (ISCAN_IN_PROGRESS(wlc)) {
		sui->custom_iscan_results_state = status;
	} else if (sui->custom_scan_results_state == WL_SCAN_RESULTS_PENDING) {
		sui->custom_scan_results_state = status;
	}

} /* wlc_custom_scan_complete */

int
wlc_custom_scan(wlc_info_t *wlc, char *arg, int arg_len,
	chanspec_t chanspec_start, int macreq, wlc_bsscfg_t *cfg)
{
	wl_scan_params_t params;
	uint nssid = 0;
	wlc_ssid_t *ssids = NULL;
	int bss_type = DOT11_BSSTYPE_ANY;
	const struct ether_addr* bssid = &ether_bcast;
	int scan_type = -1;
	int nprobes = -1;
	int active_time = -1;
	int passive_time = -1;
	int home_time = -1;
	chanspec_t* chanspec_list = NULL;
	uint chanspec_num = 0;
	int bcmerror = BCME_OK;
	uint i;
	uint scan_flags = 0;
	wlc_scan_utils_t *sui = wlc->sui;

	/* default to single fixed-part ssid */
	ssids = &params.ssid;

	if (arg_len >= WL_SCAN_PARAMS_FIXED_SIZE) {
		/* full wl_scan_params_t provided */
		bcopy(arg, &params, WL_SCAN_PARAMS_FIXED_SIZE);
		bssid = &params.bssid;
		bss_type = (int)params.bss_type;
		nprobes = (int)params.nprobes;
		active_time = (int)params.active_time;
		passive_time = (int)params.passive_time;
		home_time = (int)params.home_time;

		WL_SCAN(("wl%d: %s: scan_type 0x%x\n", wlc->pub->unit,
			__FUNCTION__, params.scan_type));

		if (params.scan_type == (uint8) -1) {
			scan_type = -1;
		} else if (params.scan_type == 0) {
			scan_type = DOT11_SCANTYPE_ACTIVE;
		} else if (!(params.scan_type & (WL_SCANFLAGS_PASSIVE |
			WL_SCANFLAGS_PROHIBITED))) {
			scan_type = DOT11_SCANTYPE_ACTIVE;
		} else {
			if (params.scan_type & WL_SCANFLAGS_PASSIVE)
				scan_type = DOT11_SCANTYPE_PASSIVE;
			scan_flags = params.scan_type;
		}

		chanspec_num = (uint)(params.channel_num & WL_SCAN_PARAMS_COUNT_MASK);
		nssid = (uint)((params.channel_num >> WL_SCAN_PARAMS_NSSID_SHIFT) &
		               WL_SCAN_PARAMS_COUNT_MASK);

		if ((int)(WL_SCAN_PARAMS_FIXED_SIZE + chanspec_num * sizeof(uint16)) > arg_len) {
			bcmerror = BCME_BUFTOOSHORT; /* arg buffer too short */
			goto done;
		}

		/* Optional remainder (chanspecs/ssids) will be used in place but
		 * may be unaligned.  Fixed portion copied out above, so use that
		 * space to copy remainder to max required alignment boundary.
		 */
		/* XXX Because of this force copy here, ASSERTs are used below
		 * and the non-ASSERT runtime checks for BADARG are removed.
		 */
		STATIC_ASSERT(OFFSETOF(wl_scan_params_t, channel_list) >= sizeof(uint32));
		arg += OFFSETOF(wl_scan_params_t, channel_list);
		arg_len -= OFFSETOF(wl_scan_params_t, channel_list);
		chanspec_list = (chanspec_t*)(arg - ((uintptr)arg & (sizeof(uint32) - 1)));
		bcopy(arg, (char*)chanspec_list, arg_len);

		if (chanspec_num > 0) {
			ASSERT(ISALIGNED(chanspec_list, sizeof(uint16)));
			/* if chanspec is valid leave it alone, if not assume its really
			 * just a 20Mhz channel number and convert it to a chanspec
			 */
			for (i = 0; i < chanspec_num; i++) {
				uint16 ch = chanspec_list[i] & WL_CHANSPEC_CHAN_MASK;

				/* Magic value of -1 to abort an existing scan, else just return */
				if (chanspec_list[i] == (uint16) -1) {
#ifdef STA
					if (AS_IN_PROGRESS(wlc)) {
						/* Ignore scan abort during association activity.
						 * If intended, abort assoc scan by disassoc iovar.
						 */
						bcmerror = BCME_NOTREADY;
						goto done;
					}
#endif // endif
					wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
					bcmerror = WL_SCAN_RESULTS_ABORTED;
					goto done;
				}

				if (wf_chspec_malformed(chanspec_list[i])) {
					chanspec_list[i] = CH20MHZ_CHSPEC2(ch,
					                         CHSPEC_BANDTYPE(chanspec_list[i]));
				}
			}
		}

		/* locate appended ssids */
		if (nssid) {
			uint offset = chanspec_num * sizeof(uint16);
			offset = ROUNDUP(offset, sizeof(uint32));
			if ((uint)arg_len < (offset + nssid * sizeof(wlc_ssid_t))) {
				bcmerror = BCME_BUFTOOSHORT;
				goto done;
			}
			ssids = (wlc_ssid_t *)((char*)chanspec_list + offset);
			ASSERT(ISALIGNED(ssids, sizeof(uint32)));
		}
	} else if (arg_len >= (int)sizeof(uint32)) {
		/* just an SSID provided */
		bcopy(arg, &params.ssid, MIN((int)sizeof(wlc_ssid_t), arg_len));
		if ((uint)arg_len < params.ssid.SSID_len) {
			bcmerror = BCME_BUFTOOSHORT;
			goto done;
		}
	} else {
		bcmerror = BCME_BUFTOOSHORT;
		goto done;
	}

	if (nssid == 0) {
		nssid = 1;
		ssids = &params.ssid;
	}

	for (i = 0; i < nssid; i++) {
		if (ssids[i].SSID_len > DOT11_MAX_SSID_LEN) {
			bcmerror = BCME_BADSSIDLEN;
			goto done;
		}
	}

	WL_ASSOC(("SCAN: WLC_SCAN custom scan\n"));

	bcmerror = wlc_scan_request_ex(wlc, bss_type, bssid, nssid, ssids,
	                               scan_type, nprobes, active_time, passive_time,
	                               home_time, chanspec_list, chanspec_num,
	                               chanspec_start, TRUE, wlc_custom_scan_complete,
	                               wlc, macreq, scan_flags, cfg, NULL, NULL);
	if (!bcmerror)
		wlc_bss_list_free(wlc, sui->custom_scan_results);

done:
	if (bcmerror) {
		if (macreq == WLC_ACTION_ISCAN)
			sui->custom_iscan_results_state = WL_SCAN_RESULTS_ABORTED;
		else if (bcmerror != BCME_NOTREADY)
			sui->custom_scan_results_state = WL_SCAN_RESULTS_ABORTED;
	} else {
		/* invalidate results for other scan method's query */
		if (macreq == WLC_ACTION_ISCAN) {
			sui->custom_scan_results_state = WL_SCAN_RESULTS_ABORTED;
			sui->custom_iscan_results_state = WL_SCAN_RESULTS_PENDING;
		} else if (macreq == WLC_ACTION_SCAN) {
			sui->custom_iscan_results_state = WL_SCAN_RESULTS_ABORTED;
			sui->custom_scan_results_state = WL_SCAN_RESULTS_PENDING;
		}

	}

	return bcmerror;
} /* wlc_custom_scan */

static int
wlc_custom_scan_results(wlc_info_t *wlc, wl_scan_results_t *iob, uint *len, uint results_state)
{
	wl_bss_info_t *wl_bi;
	uint32 totallen = 0;
	uint32 datalen;
	wlc_bss_info_t **scan_results = 0;
	uint scan_results_num = 0;
	int bcmerror = BCME_OK;
	uint i = 0, start_idx = 0;
	int buflen = (int)*len;
	wlc_scan_utils_t *sui = wlc->sui;

	iob->version = WL_BSS_INFO_VERSION;
	datalen = WL_SCAN_RESULTS_FIXED_SIZE;

	if (results_state == WL_SCAN_RESULTS_PENDING) {
		bcmerror = BCME_NOTREADY;
		goto done;
	}

	if (results_state == WL_SCAN_RESULTS_ABORTED) {
		bcmerror = BCME_NOTFOUND;
		goto done;
	}

	scan_results_num = sui->custom_scan_results->count;

	scan_results = sui->custom_scan_results->ptrs;

	/* If part of the results were returned before, start from there */
	if (ISCAN_RESULTS_OK(sui) && sui->iscan_result_last) {
		start_idx = sui->iscan_result_last;
		sui->iscan_result_last = 0;
	}

	/* calc buffer length: fixed + IEs */
	totallen = WL_SCAN_RESULTS_FIXED_SIZE +
	    (scan_results_num - start_idx) * sizeof(wl_bss_info_t);
	for (i = start_idx; i < scan_results_num; i ++) {
		ASSERT(scan_results[i] != NULL);
		if (scan_results[i]->bcn_prb_len > DOT11_BCN_PRB_LEN)
			totallen += ROUNDUP(scan_results[i]->bcn_prb_len -
			                    DOT11_BCN_PRB_LEN, 4);
	}

	/* convert each wlc_bss_info_t, writing into buffer */
	wl_bi = (wl_bss_info_t*)iob->bss_info;
	for (i = start_idx; i < scan_results_num; i++) {
		if (wlc_bss2wl_bss(wlc, scan_results[i], wl_bi, buflen, TRUE) == BCME_BUFTOOSHORT) {
#ifdef STA
			if (ISCAN_RESULTS_OK(sui)) {
				 /* Remember the num of results returned */
				if (i != 0)
					sui->iscan_result_last = i;
			}
#endif /* STA */
			break;
		}
		datalen += wl_bi->length;
		buflen -= wl_bi->length;
		wl_bi = (wl_bss_info_t *)((char *)wl_bi + wl_bi->length);

	}

done:
	/* buffer too short */
	if (iob->buflen < totallen) {
		/* return bytes needed if buffer is too short */
		iob->buflen = totallen;
		bcmerror = BCME_BUFTOOSHORT;
	} else
		iob->buflen = datalen;
	*len = datalen;
	iob->count = (i - start_idx);

	return bcmerror;
} /* wlc_custom_scan_results */

/** ioctl dispatcher */
static int
wlc_scan_utils_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_scan_utils_t *sui = ctx;
	wlc_info_t *wlc = sui->wlc;
	int bcmerror = BCME_OK;
	wlc_bsscfg_t *bsscfg;

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (cmd) {

	case WLC_SCAN:
	WL_ASSOC(("wl%d.%d: SCAN: WLC_SCAN custom scan\n",
		WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg)));
#ifdef WL_MONITOR
	if (MONITOR_ENAB(wlc)) {
		bcmerror = BCME_EPERM;
		WL_ERROR(("wl%d.%d: WLC_SCAN is not permitted if Monitor Mode is active\n",
		WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg)));
		break;
	}
#endif /* WL_MONITOR */

	bcmerror = wlc_custom_scan(wlc, arg, len, 0, WLC_ACTION_SCAN, bsscfg);
	break;

	case WLC_SCAN_RESULTS: {
		/* IO buffer has a uint32 followed by variable data.
		 * The uint32 declares the size of the buffer on input and
		 * size written on output (or required if insufficient buf space).
		 * If successful, return the bss count in uint32, followed by
		 * variable length wl_bss_info_t structs.
		 * BCME_BUFTOOSHORT is used only to indicate results were partial.
		 */
		wl_scan_results_t *iob = (wl_scan_results_t*)arg;
		uint buflen;

		if ((uint)len < WL_SCAN_RESULTS_FIXED_SIZE) {
			bcmerror = BCME_BADARG;
			break;
		}

		buflen = iob->buflen;

		if (buflen < WL_SCAN_RESULTS_FIXED_SIZE) {
			bcmerror = BCME_BADARG;
			break;
		}

		if (buflen > len)
			buflen = len;

		buflen -= WL_SCAN_RESULTS_FIXED_SIZE;

		bcmerror = wlc_custom_scan_results(wlc, iob, &buflen,
				sui->custom_scan_results_state);
		break;
	}

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return bcmerror;
} /* wlc_scan_utils_doioctl */

/** iovar dispatcher */
static int
wlc_scan_utils_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_scan_utils_t *sui = hdl;
	wlc_info_t *wlc = sui->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = BCME_OK;
	int32 int_val = 0;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	BCM_REFERENCE(bsscfg);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* Do the actual parameter implementation */
	switch (actionid) {

	case IOV_SVAL(IOV_SCANABORT):
#ifdef STA
		if (AS_IN_PROGRESS(wlc)) {
			/* Ignore scan abort during association activity.
			 * If intended, abort assoc scan by disassoc iovar.
			 */
			err = BCME_NOTREADY;
			break;
		}
#endif // endif
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
		break;

#ifdef STA
	case IOV_GVAL(IOV_SCANRESULTS_MINRSSI):
		bcopy(&sui->scanresults_minrssi, arg, sizeof(sui->scanresults_minrssi));
		break;

	case IOV_SVAL(IOV_SCANRESULTS_MINRSSI):
		sui->scanresults_minrssi = int_val;
		break;

	/* Reduces dongle memory usage by forwarding scan results to host */
	case IOV_SVAL(IOV_ESCAN): {
		uint32 escanver;
		uint16 action;
		uint16 escan_sync_id;
		char *inbuf = (char *) arg;

		bcopy(inbuf, &escanver, sizeof(escanver));
		if (escanver != ESCAN_REQ_VERSION) {
			WL_SCAN(("bad version %d\n", escanver));
			err = BCME_VERSION;
			break;
		}
		bcopy(inbuf + OFFSETOF(wl_escan_params_t, action), &action, sizeof(action));
		bcopy(inbuf + OFFSETOF(wl_escan_params_t, sync_id), &escan_sync_id,
		      sizeof(escan_sync_id));

		if (action == WL_SCAN_ACTION_START) {
			/* nothing to do here */
		} else if (action == WL_SCAN_ACTION_ABORT) {
			if (ESCAN_IN_PROGRESS(wlc->scan))
				wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
			break;
		} else {
			err = BCME_BADOPTION;
			break;
		}
		if (ESCAN_IN_PROGRESS(wlc->scan)) {
			WL_ERROR(("ESCAN request while ESCAN in progress, abort current scan\n"));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		}

		sui->cmn->escan_sync_id = escan_sync_id;

		/* start the scan state machine */
		err = wlc_custom_scan(wlc, inbuf + OFFSETOF(wl_escan_params_t, params),
		                      len - OFFSETOF(wl_escan_params_t, params),
		                      0, WLC_ACTION_ESCAN, bsscfg);
		break;
	}

	/* "spoon feed" scan results to the management application over a number of progressive
	 * scans.
	 */
	case IOV_SVAL(IOV_ISCAN): {
		uint32 iscanver;
		uint16 action;
		uint16 scan_duration;
		char *inbuf = (char *) arg;

		bcopy(inbuf, &iscanver, sizeof(iscanver));
		if (iscanver != ISCAN_REQ_VERSION) {
			WL_SCAN(("bad version %d\n", iscanver));
			err = BCME_VERSION;
			break;
		}
		bcopy(inbuf + OFFSETOF(wl_iscan_params_t, action), &action, sizeof(action));
		bcopy(inbuf + OFFSETOF(wl_iscan_params_t, scan_duration), &scan_duration,
		      sizeof(scan_duration));

		/* malloc the iscan_ignore_list iff required. Safe because
		 * the pointer gets zeroed when wlc is initialized,
		 * and the buffer gets freed at the same time as wlc is freed.
		 */
		if (!sui->iscan_ignore_list) {
			sui->iscan_ignore_list = (iscan_ignore_t *)
			    MALLOC(wlc->osh, WLC_ISCAN_IGNORE_LIST_SZ);
			if (!sui->iscan_ignore_list) {
				err = BCME_NOMEM;
				break;
			}
		}
		if (action == WL_SCAN_ACTION_START) {
			sui->iscan_chanspec_last = 0;
			sui->iscan_ignore_count = 0;
		} else if (action == WL_SCAN_ACTION_CONTINUE) {
			/* If we still have results from previous scan that have not been
			 * retrieved, sendup an event so the host can retrieve.
			 */
			if ((sui->iscan_result_last != 0) &&
			    (sui->iscan_result_last < sui->custom_scan_results->count)) {
				wlc_mac_event(wlc, WLC_E_SCAN_COMPLETE, NULL, WLC_E_STATUS_PARTIAL,
				              0, 0, 0, 0);
				break;
			}
			wlc->scan->iscan_cont = TRUE;
		} else  {
			err = BCME_BADOPTION;
			break;
		}

		/* kick off the scan state machine */
		inbuf += OFFSETOF(wl_iscan_params_t, params);
		sui->iscan_ignore_last = sui->iscan_ignore_count;
		sui->iscan_result_last = 0;
		err = wlc_custom_scan(wlc, inbuf, len - OFFSETOF(wl_iscan_params_t, params),
		                      sui->iscan_chanspec_last, WLC_ACTION_ISCAN,
		                      bsscfg);

		wlc->scan->iscan_cont = FALSE;
		if (err == BCME_OK && scan_duration)
			/* specified value must allow time for at least 2 channels! */
			wl_add_timer(wlc->wl, sui->iscan_timer, scan_duration, 0);

		break;
	}

	case IOV_GVAL(IOV_ISCANRESULTS): {
		wl_iscan_results_t *resp = (wl_iscan_results_t *)arg;
		uint buflen;

		/* BCME_BUFTOOSHORT is used only to indicate results were partial. */
		if (len < WL_ISCAN_RESULTS_FIXED_SIZE) {
			err = BCME_BADARG;
			break;
		}

		/* params has in/out data, so shift it to be the start of the resp buf */
		bcopy(params, resp, WL_ISCAN_RESULTS_FIXED_SIZE);

		buflen = resp->results.buflen;

		if (buflen < WL_ISCAN_RESULTS_FIXED_SIZE) {
			err = BCME_BADARG;
			break;
		}

		if (buflen > len)
			buflen = len;

		buflen -= WL_ISCAN_RESULTS_FIXED_SIZE;

		err = wlc_custom_scan_results(wlc, &resp->results, &buflen,
		                       sui->custom_iscan_results_state);
		if (err == BCME_BUFTOOSHORT)
			resp->results.buflen = buflen;
		else if (ISCAN_ABORTED(sui))
			/* query failure: dump this round of ignore elements */
			sui->iscan_ignore_count = sui->iscan_ignore_last;

		/* iscan pass is complete, but we were not able to return all the
		 * results, return status as partial.
		 */
		if (sui->custom_iscan_results_state == WL_SCAN_RESULTS_SUCCESS) {
			if (sui->iscan_result_last != 0)
				resp->status = WL_SCAN_RESULTS_PARTIAL;
			/* if we were not able to return any results because the
			 * output buffer is too small to hold even 1 result, return
			 * status as partial.
			 */
			else if (err == BCME_BUFTOOSHORT)
				resp->status = WL_SCAN_RESULTS_PARTIAL;
		} else
			resp->status = sui->custom_iscan_results_state;

		/* Force partial status to complete if the ignore list is full and
		 * all current items are delivered (avoid possible infinite loop).
		 */
		if ((resp->status == WL_SCAN_RESULTS_PARTIAL) &&
		    (sui->iscan_ignore_count == WLC_ISCAN_IGNORE_MAX)) {
			if ((sui->iscan_result_last != 0) &&
			    (sui->iscan_result_last < sui->custom_scan_results->count)) {
				/* In this case we're ok (still more in current scan
				 * list to return) so leave it alone (invite iscan_c).
				 */
			} else {
				resp->status = WL_SCAN_RESULTS_SUCCESS;
			}
		}

		/* response struct will have the actual status code */
		/* free results if host has retrieved all of them */
		if ((resp->status == WL_SCAN_RESULTS_SUCCESS) ||
			(resp->status == WL_SCAN_RESULTS_ABORTED))
			wlc_bss_list_free(wlc, sui->custom_scan_results);

		err = BCME_OK;
		break;
	}
#endif /* STA */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
} /* wlc_scan_utils_doiovar */

#ifdef STA
/** Incremental scan */
static void
wlc_iscan_timeout(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char chanbuf[CHANSPEC_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */
	wlc_scan_utils_t *sui = wlc->sui;

	if (ISCAN_IN_PROGRESS(wlc) &&
	    sui->custom_iscan_results_state != WL_SCAN_RESULTS_SUCCESS) {
		WL_INFORM(("wlc_iscan_timeout ending on chanspec %s, i %d\n",
		           wf_chspec_ntoa_ex(sui->iscan_chanspec_last, chanbuf),
		           wlc->scan_results->count));
		wlc_scan_terminate(wlc->scan, WLC_E_STATUS_PARTIAL);
	}
}
#endif /* STA */

/** Called back by the IE parse module when a frame with a certain IE is received  */
static int
wlc_scan_parse_ssid_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;

	if (data->ie == NULL) {
		WL_INFORM(("Missing SSID info in beacon\n"));
		bi->SSID_len = 0;
	}
	else if (data->ie[TLV_LEN_OFF] > DOT11_MAX_SSID_LEN) {
		WL_INFORM(("long SSID in beacon\n"));
		bi->SSID_len = 0;
	}
	else {
		bi->SSID_len = data->ie[TLV_LEN_OFF];
		bcopy(&data->ie[TLV_BODY_OFF], bi->SSID, bi->SSID_len);
	}

	return BCME_OK;
}

/** Called back by the IE parse module when a frame with a certain IE is received  */
static int
wlc_scan_parse_sup_rates_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;

	/* Check for a legacy 54G bcn/proberesp by looking for more than 8 rates
	 * in the Supported Rates elt
	 */
	if (data->ie != NULL &&
	    data->ie[TLV_LEN_OFF] > 8)
		bi->flags |= WLC_BSS_54G;

	return BCME_OK;
}

/** Called back by the IE parse module when a frame with a certain IE is received  */
static int
wlc_scan_parse_ibss_parms_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;

	/* IBSS parameters */
	if (data->ie != NULL &&
	    data->ie[TLV_LEN_OFF] >= DOT11_MNG_IBSS_PARAM_LEN)
		bi->atim_window = ltoh16_ua(&data->ie[TLV_BODY_OFF]);

	return BCME_OK;
}

/** Called back by the IE parse module when a frame with a certain IE is received  */
static int
wlc_scan_parse_wme_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;

	/* WME parameters */
	if (WME_ENAB(wlc->pub)) {
		bcm_tlv_t *wme_ie = (bcm_tlv_t *)data->ie;

		if (wme_ie != NULL) {
			bi->flags |= WLC_BSS_WME;
			bi->wme_qosinfo = ((wme_ie_t *)wme_ie->data)->qosinfo;
		}
	}

	return BCME_OK;
}

/** Called back by the IE parse module when a frame with a certain IE is received  */
static int
wlc_scan_parse_brcm_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;

	/* Is the AP from Broadcom */
	if (data->ie != NULL)
		bi->flags |= WLC_BSS_BRCM;

	return BCME_OK;
}

/** Called back by the IE parse module when a frame with a certain IE is received  */
static int
wlc_scan_parse_ext_cap_ie(void *ctx, wlc_iem_parse_data_t *data)
{
	wlc_iem_ft_pparm_t *ftpparm = data->pparm->ft;
	wlc_bss_info_t *bi = ftpparm->scan.result;

	if (data->ie != NULL) {
		dot11_extcap_ie_t *extcap_ie_tlv = (dot11_extcap_ie_t *)data->ie;
		dot11_extcap_t *cap;
		cap = (dot11_extcap_t*)extcap_ie_tlv->cap;
		if (extcap_ie_tlv->len >= CEIL(DOT11_EXT_CAP_OPER_MODE_NOTIF, 8)) {
			if (isset(cap->extcap, DOT11_EXT_CAP_OPER_MODE_NOTIF)) {
				bi->flags2 |= WLC_BSS_OPER_MODE;
			}
		}
	}
	return BCME_OK;
}

/** register Bcn/Prbrsp IE mgmt handlers */
static int
BCMATTACHFN(wlc_scan_register_iem_fns)(wlc_info_t *wlc)
{
	wlc_iem_info_t *iemi = wlc->iemi;
	int err = BCME_OK;
	uint16 scanfstbmp = FT2BMP(WLC_IEM_FC_SCAN_BCN) | FT2BMP(WLC_IEM_FC_SCAN_PRBRSP);

	/* parse */
	if ((err = wlc_iem_add_parse_fn_mft(iemi, scanfstbmp, DOT11_MNG_EXT_CAP_ID,
	      wlc_scan_parse_ext_cap_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, err %d, ext cap ie\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_add_parse_fn_mft(iemi, scanfstbmp, DOT11_MNG_SSID_ID,
	                                    wlc_scan_parse_ssid_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, err %d, ssid in scan\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_add_parse_fn_mft(iemi, scanfstbmp, DOT11_MNG_RATES_ID,
	                                    wlc_scan_parse_sup_rates_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, err %d, sup rates in scan\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_add_parse_fn_mft(iemi, scanfstbmp, DOT11_MNG_IBSS_PARMS_ID,
	                                    wlc_scan_parse_ibss_parms_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_add_parse_fn failed, err %d, ibss parms in scan\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_vs_add_parse_fn_mft(iemi, scanfstbmp, WLC_IEM_VS_IE_PRIO_WME,
	                                    wlc_scan_parse_wme_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, err %d, wme in scan\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	if ((err = wlc_iem_vs_add_parse_fn_mft(iemi, scanfstbmp, WLC_IEM_VS_IE_PRIO_BRCM,
	                                    wlc_scan_parse_brcm_ie, wlc)) != BCME_OK) {
		WL_ERROR(("wl%d: %s wlc_iem_vs_add_parse_fn failed, err %d, brcm in scan\n",
		          wlc->pub->unit, __FUNCTION__, err));
		goto fail;
	}
	return BCME_OK;

fail:
	return err;
} /* wlc_scan_register_iem_fns */

#ifdef WLRSDB
/** RSDB specific */
void
wlc_scan_utils_scan_complete(wlc_info_t *wlc, wlc_bsscfg_t *scan_cfg)
{
	WL_SCAN(("wl%d.%d:%s counts:%d\n", wlc->pub->unit,
	         scan_cfg->_idx, __FUNCTION__, wlc->scan_results->count));

	/* Move the scan results to other wlc->scan_results
	 * if this scan_info is not for requested scan info.
	 */
	if (wlc != scan_cfg->wlc) {
		uint indx, count;
		wlc_bss_list_t *bss_list_from, *bss_list_to;
		wlc_info_t *scan_req_wlc = scan_cfg->wlc;

		bss_list_to = scan_req_wlc->scan_results;
		wlc = wlc_rsdb_get_other_wlc(scan_req_wlc);
		bss_list_from = wlc->scan_results;

		/* XXX We are limiting the number of scan results from the
		 * available bss_list. Do we need to sort based on RSSI and
		 * discard lowest RSSI BSS's?
		 */
		count = MIN((scan_req_wlc->pub->tunables->maxbss - bss_list_to->count),
		            bss_list_from->count);

		WL_SCAN(("scan_req wlc:%d, bss_count to:%d, other wlc:%d"
		         " bss_count from,%d\n", scan_req_wlc->pub->unit,
		         bss_list_to->count, wlc->pub->unit, bss_list_from->count));

		for (indx = 0; indx < count; indx++) {
			bss_list_to->ptrs[bss_list_to->count++] = bss_list_from->ptrs[indx];
			bss_list_from->ptrs[indx] = NULL;
		}

		/* Free the remaining BSS's if any from current scan info. */
		wlc_bss_list_free(wlc, wlc->scan_results);
	}

	WL_SCAN(("Current wlc:%d, scan_req_wlc:%d Cur bss_count:%d"
	         " Total Avail(After copy):%d \n", wlc->pub->unit,
	         scan_cfg->wlc->pub->unit, wlc->scan_results->count,
	         scan_cfg->wlc->scan_results->count));
}
#endif /* WLRSDB */

/**
 * This function is used for finding the BSS among the scan results
 * which has the RSSI value lesser than the BSS at hand. It finds the BSS,
 * compares it to the rssi threshold provided and if RSSI < threshold
 * then it frees beacon(Probe) and resets. This BSS container will be
 * used to store the new incoming scan result.
 */
static wlc_bss_info_t *
wlc_BSS_reuse_lowest_rssi(wlc_info_t *wlc, int16 rssi_thresh)
{
	uint idx = 0;
	int16 min_rssi = 0;
	wlc_bss_info_t *BSS = NULL;
	wlc_bss_list_t *scan_results = wlc->scan_results;

	/* Find the result among the scan_results which has the least
	* RSSI value and return the BSS pointer
	*/
	for (idx = 0; idx < wlc->scan_results->count; idx++) {
		if (idx == 0 || scan_results->ptrs[idx]->RSSI < min_rssi) {
			BSS = wlc->scan_results->ptrs[idx];
			min_rssi = BSS->RSSI;
		}
	}
	/* If min_rssi is better than or equal to given rssi nothing to return */
	if (min_rssi >= rssi_thresh) {
		return NULL;
	}
	/* Free BSS->bcn_prb if allocated because it again gets allocated in the reused BSS */
	if (BSS->bcn_prb) {
		MFREE(wlc->osh, BSS->bcn_prb, BSS->bcn_prb_len);
	}

	/* Reset the values before returning the BSS pointer */
	memset((char*)BSS, 0, sizeof(wlc_bss_info_t));
	BSS->RSSI = WLC_RSSI_MINVAL;

	WL_SCAN(("wl%d: %s: Scan results have increased beyond %d"
		"Reusing the BSS with the lowset RSSI value for further storage\n",
		WLCWLUNIT(wlc), __FUNCTION__, wlc->pub->tunables->max_assoc_scan_results));

	return BSS;
}

/** Called by wlc_rx.c when it receives a frame while a scan is in progress */
int
wlc_recv_scan_parse(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	struct dot11_bcn_prb *bcn = (struct dot11_bcn_prb *)body;
	struct dot11_bcn_prb *bcn_prb = NULL;
	uint16 cap = ltoh16(bcn->capability);
	wlc_bss_info_t bi;
	wlc_bss_info_t *BSS;
	bcm_tlv_t *ssid_ie;
	uint8 *ssid = 0;
	uint8 ssid_len = 0;
	bool filter;
#if defined(STA) && defined(GSCAN)
	bool send_to_host = FALSE;
#endif /* STA && GSCAN */
#ifdef STA
	chanspec_t chanspec = 0;
#endif // endif
	wlc_scan_info_t *scan = wlc->scan;
	wlc_bsscfg_t *cfg = NULL;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char chanbuf1[CHANSPEC_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */
#ifdef STA
	wlc_scan_utils_t *sui = wlc->sui;
#endif // endif

	uint8 fk = (ltoh16(hdr->fc) & FC_KIND_MASK);

	/* Ignore packets until the channel change occurs */
	if (!(scan->state & SCAN_STATE_READY)) {
		WL_SCAN(("wl%d: SCAN: recv_scan ignore pkts until STATE_READY\n",
		         wlc->pub->unit));
		return BCME_OK;
	}

	if (SCAN_IN_PROGRESS(scan)) {
		cfg = wlc_scan_bsscfg(scan);
		BCM_REFERENCE(cfg);
	}

	/* prune based on allow/deny list */
	if ((cfg != NULL) && BSSCFG_STA(cfg)) {
		int addr_match = wlc_macfltr_addr_match(wlc->macfltr, cfg, &hdr->bssid);
		/* prune if on the deny list or not on the allow list */
		if (addr_match == WLC_MACFLTR_ADDR_DENY ||
			addr_match == WLC_MACFLTR_ADDR_NOT_ALLOW) {
			return BCME_OK;
		}
	}

	filter = (fk == FC_BEACON && !(scan->state & SCAN_STATE_PASSIVE) &&
		!wlc_quiet_chanspec(wlc->cmi, WLC_RX_CHANNEL(wlc->pub->corerev, &wrxh->rxhdr)));

	/* find and validate the SSID IE */
	if (body_len < DOT11_BCN_PRB_LEN) {
		WL_INFORM(("%s: invalid frame length\n", __FUNCTION__));
		return BCME_BADLEN;
	}

	/* find and validate the SSID IE */
	ssid_ie = bcm_parse_tlvs(body + DOT11_BCN_PRB_LEN, body_len - DOT11_BCN_PRB_LEN,
		DOT11_MNG_SSID_ID);

	if (ssid_ie != NULL) {
		ssid = ssid_ie->data;
		ssid_len = ssid_ie->len;
	}

	/*
	 * check for probe responses/beacons:
	 * - must have the correct Infra mode
	 * - must have the SSID(s) we were looking for,
	 *   unless we were doing a broadcast SSID request (ssid len == 0)
	 * - must have matching BSSID (if unicast was specified)
	 */
	if (((scan->wlc_scan_cmn->bss_type == DOT11_BSSTYPE_ANY ||
	      (scan->wlc_scan_cmn->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE &&
	       !(cap & DOT11_CAP_IBSS) &&
	       ((cap & DOT11_CAP_ESS) ||
#ifdef WLP2P
	        P2P_ENAB(wlc->pub) ||
#endif // endif
	        FALSE)) ||
	      (scan->wlc_scan_cmn->bss_type == DOT11_BSSTYPE_INDEPENDENT &&
	       (cap & DOT11_CAP_IBSS))) &&
	     (wlc_scan_ssid_match(scan, ssid, ssid_len, filter) ||
#ifdef NLO
	      (cfg && cfg->nlo) ||
#endif /* NLO */
	      FALSE) &&
	     (ETHER_ISMULTI(&scan->bssid) ||
	      bcmp(&scan->bssid, &hdr->bssid, ETHER_ADDR_LEN) == 0))) {
#if defined(BCMDBG) || defined(WLMSG_SCAN)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif // endif
		bool discard_prb_resp = FALSE;

		WL_SCAN(("wl%d: found BSS %s on chanspec 0x%x\n",
		         wlc->pub->unit, bcm_ether_ntoa(&hdr->bssid, eabuf),
		         WLC_BAND_PI_RADIO_CHANSPEC));

		WL_SCAN(("wl%d: found BSS %s on chanspec 0x%x\n",
		         wlc->pub->unit, bcm_ether_ntoa(&hdr->bssid, eabuf),
		         WLC_BAND_PI_RADIO_CHANSPEC));
		WL_PRHDRS(wlc, fk == FC_BEACON ? "rxpkt hdr (beacon)" : "rxpkt hdr (probersp)",
		          (uint8*)hdr, NULL, wrxh, body_len);
#if defined(BCMDBG) || defined(WLMSG_PRPKT)
		if (WL_PRPKT_ON()) {
			printf((fk == FC_BEACON)?"\nrxpkt (beacon)\n":"\nrxpkt (probersp)\n");
			wlc_print_rxbcn_prb(wlc, (uint8*)plcp, body_len +
				D11_PHY_RXPLCP_LEN(wlc->pub->corerev));
		}
#endif // endif

		if (!discard_prb_resp && (fk != FC_ACTION &&
			wlc_recv_scan_parse_bcn_prb(wlc, wrxh, &hdr->bssid, (fk == FC_BEACON),
		                                body, body_len, &bi))) {
			/* parse fail, discard this new bi */
			WL_INFORM(("wl%d: %s: tossing bcn/prb resp "
			           "collected on chanspec %s wlc_parse_bcn_prb failed\n",
			           wlc->pub->unit, __FUNCTION__,
			           wf_chspec_ntoa_ex(D11RXHDR_ACCESS_VAL(&wrxh->rxhdr,
			           wlc->pub->corerev, RxChan), chanbuf1)));
			discard_prb_resp = TRUE;
		}

		if (!discard_prb_resp &&
		    /* Make sure bcn/prb was received on a channel in our scan list */
		    !wlc_scan_in_scan_chanspec_list(wlc->scan, bi.chanspec)) {
			WL_INFORM(("%s: bcn/prb on chanspec 0x%x, not in scan chanspec list\n",
			           __FUNCTION__, bi.chanspec));
			discard_prb_resp = TRUE;
		}

#ifdef WLP2P
		if (!discard_prb_resp && P2P_ENAB(wlc->pub) && fk != FC_ACTION &&
		    SCAN_IN_PROGRESS(wlc->scan) &&
		    wlc_p2p_recv_parse_bcn_prb(wlc->p2p, cfg, (fk == FC_BEACON),
		                &bi.rateset, body, body_len) != BCME_OK) {
			WL_INFORM(("wl%d: %s: tossing bcn/prbrsp, wlc_p2p_parse_bcn_prb failed\n",
			           wlc->pub->unit, __FUNCTION__));
			discard_prb_resp = TRUE;
		}
#endif // endif

		/* check bssid, ssid AND band matching */
		BSS = wlc_BSSlookup(wlc, (uchar *)&hdr->bssid, bi.chanspec, bi.SSID, bi.SSID_len);

		if (BSS) {
			/* update existing record according to the following */
			if (bi.RSSI == WLC_RSSI_INVALID) {
				return BCME_OK;		/* ignore invalid RSSI */
			} else if (BSS->RSSI == WLC_RSSI_INVALID) {
				/* fall though - always take valid RSSI */
			} else if ((BSS->flags & WLC_BSS_RSSI_ON_CHANNEL) &&
			          !(bi.flags & WLC_BSS_RSSI_ON_CHANNEL)) {
				return BCME_OK;		/* ignore off channel RSSI */
			} else if (!(BSS->flags & WLC_BSS_RSSI_ON_CHANNEL) &&
			            (bi.flags & WLC_BSS_RSSI_ON_CHANNEL)) {
				/* fall though - always prefer RSSI_ON_CHANNEL */
			} else if ((fk == FC_BEACON) && !(BSS->flags & WLC_BSS_BEACON)) {
				return BCME_OK;		/* ignore beacon & keep probe response */
			} else if ((fk != FC_BEACON) && (BSS->flags & WLC_BSS_BEACON)) {
				/* fall though - always prefer probe response to beacon */
			} else if (BSS->RSSI > bi.RSSI) {
				return BCME_OK;		/* ignore weaker RSSI */
			} else if (BSS->RSSI < bi.RSSI) {
				/* fall througth - alwyas keep strong RSSI */
			}
		}

		/* do not allow to update bcn/probe response if the RSSI is invalid */
		if (!discard_prb_resp && !ISSIM_ENAB(wlc->pub->sih)) {
			if (bi.RSSI == WLC_RSSI_INVALID) {
				discard_prb_resp = TRUE;         /* ignore invalid RSSI */
			}
		}

		/* do not allow to update bcn/probe response if BT activity is enabled */
		if (!discard_prb_resp && D11REV_LT(wlc->pub->corerev, 128) &&
		    (wrxh->rxhdr.lt80.RxStatus1 & RXS_GRANTBT)) {
			discard_prb_resp = TRUE; /* ignore weaker RSSI in presence of BT activity */
		}

#ifdef STA
		if (!discard_prb_resp && sui->scanresults_minrssi &&
		         bi.RSSI < (int16) (sui->scanresults_minrssi)) {
			/* filter this one out if the user specified a minimum signal strength */
			WL_INFORM(("wl%d: %s: tossing bcn/prb resp "
				   "for BSS on channel spec %d since target rssi was %d\n",
				   wlc->pub->unit, __FUNCTION__, bi.chanspec, bi.RSSI));
			discard_prb_resp = TRUE;
		}

		if (discard_prb_resp) {
#ifdef WL_OCE
			if (OCE_ENAB(wlc->pub) && fk != FC_ACTION) {
				wlc_scan_utils_scan_data_notif(wlc, wrxh, plcp, hdr, body,
					body_len, &bi);
			}
#endif /* WL_OCE */
			return BCME_OK;
		}

		/* TODO: an opportunity to use the notification! */

#ifdef ANQPO
		/* process only probe responses received on current channel */
		if (ANQPO_ENAB(wlc->pub) && scan->wlc_scan_cmn->is_hotspot_scan &&
			(bi.flags2 & WLC_BSS_HS20) && fk == FC_PROBE_RESP) {
			if (bi.flags & WLC_BSS_RSSI_ON_CHANNEL) {
				ASSERT(cfg);
				wl_anqpo_process_scan_result(wlc->anqpo, &bi,
					body + DOT11_BCN_PRB_LEN, body_len - DOT11_BCN_PRB_LEN,
					cfg->_idx);
			}
		}
#endif	/* ANQPO */

		if (fk != FC_ACTION) {
			wlc_scan_utils_scan_data_notif(wlc, wrxh, plcp, hdr, body, body_len, &bi);
		}

#ifdef WL11D
		/* Check for country information immediately */
		if (WLC_AUTOCOUNTRY_ENAB(wlc)) {
			wlc_11d_scan_result(wlc->m11d, &bi, body, (uint)body_len);
		}
#endif /* WL11D */

		if (ESCAN_IN_PROGRESS(wlc->scan)) {
			wl_escan_result_t *escan_result;
			wl_bss_info_t *escan_bi;
			uint escan_result_len;
			osl_t *osh = cfg->wlc->osh;

			bi.bcn_prb_len = 0;
			if (scan->state & SCAN_STATE_SAVE_PRB) {
			       /* save raw probe response frame */
			       if ((bi.bcn_prb =
			            (struct dot11_bcn_prb *)MALLOCZ(osh, body_len)) != NULL) {
				       bcopy((char*)body, (char*)bi.bcn_prb, body_len);
				       bi.bcn_prb_len = (uint16)body_len;
			       } else {
					WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
						body_len, MALLOCED(osh)));
				       /* No point proceeding any further, since malloc */
				       /* failed here, it will fail again, most likely. */
				       return BCME_NOMEM;
			       }
			}

			escan_result_len = sizeof(wl_escan_result_t) + bi.bcn_prb_len
						+ sizeof(wl_bss_info_t);

			if ((escan_result =
			     (wl_escan_result_t *)MALLOCZ(osh, escan_result_len)) == NULL) {
				WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
					(int)escan_result_len, MALLOCED(osh)));
				if (bi.bcn_prb_len) {
					MFREE(osh, bi.bcn_prb, bi.bcn_prb_len);
				}
				return BCME_NOMEM;
			}

			escan_bi = (wl_bss_info_t*)escan_result->bss_info;

			if (wlc_bss2wl_bss(wlc, &bi, escan_bi,
			                   sizeof(wl_bss_info_t) + bi.bcn_prb_len,
			                   TRUE) != BCME_OK) {
				WL_SCAN_ERROR(("escan: results buffer too"
						"short %s()\n", __FUNCTION__));
			}
			else {
				escan_result->sync_id = sui->cmn->escan_sync_id;
				escan_result->version = WL_BSS_INFO_VERSION;
				escan_result->bss_count = 1;
				escan_result->buflen = WL_ESCAN_RESULTS_FIXED_SIZE +
					escan_bi->length;

				/* In RSDB Parallel Scanning, wlc where scan request is given
				 * may be different than current wlc. So, use the wlc
				 * from cfg->wlc. For non-RSDB, cfg->wlc should be same as wlc.
				 */
				if (!RSDB_ENAB(wlc->pub)) {
					ASSERT(wlc == cfg->wlc);
				}
				wlc_bss_mac_event(cfg->wlc, cfg, WLC_E_ESCAN_RESULT, &hdr->sa,
				                  WLC_E_STATUS_PARTIAL,
				                  0, 0,	escan_result, escan_result->buflen);
			}
			MFREE(osh, escan_result, escan_result_len);

#ifdef WLSCANCACHE
			if (SCANCACHE_ENAB(cfg->wlc->scan) &&
#ifdef WLP2P
			    !BSS_P2P_DISC_ENAB(wlc, cfg) &&
#endif // endif
			    TRUE) {
				wlc_scan_add_bss_cache(scan, &bi);
			}
#endif /* WLSCANCACHE */

			if (bi.bcn_prb_len) {
				MFREE(osh, bi.bcn_prb, bi.bcn_prb_len);
			}
			return BCME_OK;
		}

		/* Iscan: add new BSS only if not previously reported (update existing is ok). */
		if (!BSS && ISCAN_IN_PROGRESS(wlc)) {
			/* use actual bcn_prb rx channel for comparison chanspec */
			chanspec = WLC_RX_CHANNEL(wlc->pub->corerev, &wrxh->rxhdr) <<
				WL_CHANSPEC_CHAN_SHIFT;
			/* OR in the other bits */
			chanspec |= (chanspec_t)(bi.chanspec & ~(WL_CHANSPEC_CHAN_MASK));
			/* ignore if it's in the ignore list; add otherwise */
			if (wlc_BSSignore(wlc, (uchar *)&hdr->bssid, chanspec, bi.SSID,
			                  bi.SSID_len))
				return BCME_OK;
		}
#endif /* STA */
#if defined(STA) && defined(GSCAN)
		if (WLPFN_ENAB(wlc->pub) && GSCAN_ENAB(wlc->pub) &&
		    wl_pfn_scan_in_progress(wlc->pfn)) {
			send_to_host = wl_pfn_is_ch_bucket_flag_enabled(wlc->pfn,
			                 wf_chspec_ctlchan(bi.chanspec),
			                 CH_BUCKET_REPORT_FULL_RESULT);
		}
#endif // endif
		/* make sure we have enough space before proceeding */
		if (scan->state & SCAN_STATE_SAVE_PRB ||
#if defined(STA) && defined(GSCAN)
		    send_to_host ||
#endif // endif
#ifdef NLO
		    (cfg && cfg->nlo) ||
#endif /* NLO */
		    FALSE) {
			bcn_prb = (struct dot11_bcn_prb *) MALLOCZ(wlc->osh, body_len);
			if (!bcn_prb) {
				WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__, body_len,
					MALLOCED(wlc->osh)));
				return BCME_NOMEM;
			}
		}

		/* add it if it was not already in the list */
		if (!BSS) {
			if (AS_IN_PROGRESS(wlc) &&
				wlc->pub->tunables->max_assoc_scan_results != 0 &&
				(wlc->scan_results->count >=
				(uint)wlc->pub->tunables->max_assoc_scan_results)) {
				if ((BSS = wlc_BSS_reuse_lowest_rssi(wlc, bi.RSSI)) == NULL) {
					if (bcn_prb)
						MFREE(wlc->osh, bcn_prb, body_len);
					WL_INFORM(("%s: exceeded the limit of %d scan results"
						"tossing the BSS\n", __FUNCTION__,
						wlc->pub->tunables->max_assoc_scan_results));
					return BCME_OK;
				}
			} else {
				BSS = wlc_BSSadd(wlc);
			}
#ifdef STA
			if (BSS && ISCAN_IN_PROGRESS(wlc)) {
				wlc_BSSIgnoreAdd(wlc, (uchar *)&hdr->bssid, chanspec,
					bi.SSID, bi.SSID_len);
			}
#endif /* STA */
		}

		if (BSS) {
			/* free the prb pointer, prevent memory leakage */
			if (BSS->bcn_prb) {
				MFREE(wlc->osh, BSS->bcn_prb, BSS->bcn_prb_len);
				BSS->bcn_prb = NULL;
				BSS->bcn_prb_len = 0;
			}
			/* update the entry regardless existing or new */
			bcopy((char*)&bi, (char*)BSS, sizeof(wlc_bss_info_t));

#if defined(WLSCANCACHE)

			BSS->timestamp = OSL_SYSUPTIME();
#endif // endif
			if ((scan->state & SCAN_STATE_SAVE_PRB) ||
#if defined(STA) && defined(GSCAN)
			    send_to_host ||
#endif // endif
#ifdef NLO
			    (cfg && cfg->nlo) ||
#endif /* NLO */
			    FALSE) {
				/* scan completion responsible for freeing frame */
				ASSERT(!BSS->bcn_prb);
				ASSERT(bcn_prb != NULL);

				/* save raw probe response frame */
				BSS->bcn_prb = bcn_prb;
				bcopy((char*)body, (char*)BSS->bcn_prb, body_len);
				BSS->bcn_prb_len = (uint16) body_len;
			}

			{
			ratespec_t rspec = wlc_recv_compute_rspec(wlc->pub->corerev, &wrxh->rxhdr,
				plcp);
			BSS->rx_tsf_l = wlc_recover_tsf32(wlc, wrxh);
			BSS->rx_tsf_l += wlc_compute_bcn_payloadtsfoff(wlc, rspec);
			}

#if defined(STA) && defined(WLPFN)
			/* if this is a pfn scan */
			if (WLPFN_ENAB(wlc->pub) &&
			    (wl_pfn_scan_in_progress(wlc->pfn) ||
#ifdef NLO
			     /* or if nlo enabled and not in speedy assoc recreation */
			     (cfg && cfg->nlo && !(cfg->assoc->flags & AS_F_SPEEDY_RECREATE)) ||
#endif /* NLO */
			     FALSE)) {
				/* process network */
				wl_pfn_process_scan_result(wlc->pfn, BSS);
#if defined(STA) && defined(GSCAN)
				if (GSCAN_ENAB(wlc->pub) && send_to_host) {
					wlc_send_pfn_full_scan_result(wlc, BSS, cfg, hdr);
				}
#endif // endif
				if (!(scan->state & SCAN_STATE_SAVE_PRB))
					wlc_bss_list_free(wlc, wlc->scan_results);
				return BCME_OK;
			}
#endif	/* STA && WLPFN */

#ifdef STA
			/* Simulation can be terminated immediately. */
			if (ISSIM_ENAB(wlc->pub->sih))
				wlc_scan_terminate(wlc->scan, WLC_E_STATUS_SUCCESS);

			/* Single channel/bssid can terminate immediately. */
			if (!ETHER_ISMULTI(&wlc->scan->bssid) &&
			    !ETHER_ISNULLADDR(&wlc->scan->bssid) &&
			    wlc_scan_chnum(wlc->scan) == 1 &&
#ifdef WLP2P
			    /* XXX  When P2P GC is assoc scanning in VSDB, the scan should
			     * not be terminated before listening probe response from the
			     * intended GO.
			     */
			    cfg != NULL && !BSS_P2P_ENAB(wlc, cfg) &&
#endif // endif
			    TRUE) {
				wlc_scan_terminate(wlc->scan, WLC_E_STATUS_SUCCESS);
			}
#endif /* STA */
		} else {
			if (bcn_prb)
				MFREE(wlc->osh, bcn_prb, body_len);
#ifdef STA
			/* sui->iscan_chanspec_last is updated in the
			 * scan_timer, just terminated the scan
			 */
			/* no room in list or out of memory */
			WL_INFORM(("%s: can't add BSS: "
			           "terminating scan in process\n", __FUNCTION__));
#ifdef WLRCC
			if (WLRCC_ENAB(wlc->pub) && cfg->roam->active) {
				/* terminate with WLC_E_STATUS_SUCCESS to process
				 * the collected BSS information
				 */
				WL_ROAM(("%s: terminating the roam scan "
					"with WLC_E_STATUS_SUCCESS\n", __FUNCTION__));
				wlc_scan_terminate(scan, WLC_E_STATUS_SUCCESS);
			} else
#else
				wlc_scan_terminate(scan, WLC_E_STATUS_PARTIAL);
#endif // endif
			return BCME_OK;
#endif /* STA */
		}

#ifdef WLP2P
		if (P2P_ENAB(wlc->pub) && cfg != NULL && fk != FC_ACTION) {

			if (wlc_eventq_test_ind(wlc->eventq, WLC_E_PROBRESP_MSG))
			{
				wl_event_rx_frame_data_t rxframe_data;

				wlc_recv_prep_event_rx_frame_data(wlc, wrxh, plcp, &rxframe_data);

				wlc_bss_mac_rxframe_event(wlc, cfg, WLC_E_PROBRESP_MSG, &hdr->sa,
				                          0, 0,
				                          0, (char *)hdr,
				                          body_len + DOT11_MGMT_HDR_LEN,
				                          &rxframe_data);
			}

			/* advance assoc state machine */
			if (BSS_P2P_ENAB(wlc, cfg) &&
			    cfg->assoc != NULL && cfg->assoc->state != AS_IDLE &&
			    bcmp(&scan->bssid, &hdr->bssid, ETHER_ADDR_LEN) == 0) {
				WL_SCAN(("wl%d.%d: match BSSID %s and move to association\n",
				         wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
				         bcm_ether_ntoa(&scan->bssid, eabuf)));
				wlc_scan_terminate(scan, WLC_E_STATUS_SUCCESS);
			}
		}
#endif /* WLP2P */
	}
	return BCME_OK;
} /* wlc_recv_scan_parse */

/** Called when a frame is received */
static wlc_bss_info_t *
wlc_BSSadd(wlc_info_t *wlc)
{
	wlc_bss_info_t	*BSS;

#if defined(STA) && defined(DONGLEBUILD)
	/* For dongle, limit stored scan results (except assoc scan) further */
	if (SCAN_IN_PROGRESS(wlc->scan) && !AS_IN_PROGRESS(wlc))
		if (wlc->scan_results->count >= wlc->pub->tunables->maxubss)
			return NULL;
#endif /* STAS && DONGLEBUILD */

	if (wlc->scan_results->count == (uint)wlc->pub->tunables->maxbss)
		return (NULL);

	/* allocate a new entry */
	BSS = (wlc_bss_info_t *)MALLOCZ(wlc->osh, sizeof(wlc_bss_info_t));
	if (BSS) {
		bzero((char*)BSS, sizeof(wlc_bss_info_t));
		BSS->RSSI = WLC_RSSI_MINVAL;
		wlc->scan_results->ptrs[wlc->scan_results->count++] = BSS;
	} else {
		WL_ERROR((WLC_MALLOC_ERR, WLCWLUNIT(wlc), __FUNCTION__,
			(int)sizeof(wlc_bss_info_t), MALLOCED(wlc->osh)));
	}
	return BSS;
}

/** Called when a frame is received */
static wlc_bss_info_t *
wlc_BSSlookup(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec, uchar ssid[], uint ssid_len)
{
	wlc_bss_info_t	*BSS;
	uint indx;

	BSS = NULL;

	/* search for the BSS descriptor which matches
	 * the bssid AND band(2G/5G) AND SSID
	 */
	for (indx = 0; indx < wlc->scan_results->count; indx++) {
		BSS = wlc->scan_results->ptrs[indx];
		if (!bcmp(bssid, (char*)&BSS->BSSID, ETHER_ADDR_LEN) &&
		    CHSPEC_BAND(chanspec) == CHSPEC_BAND(BSS->chanspec) &&
		    ssid_len == BSS->SSID_len &&
		    !bcmp(ssid, BSS->SSID, ssid_len))
			break;
	}

	if (indx != wlc->scan_results->count)
		return (BSS);
	else
		return (NULL);
}

#ifdef STA

/** Called when a frame is received. STA specific. */
static int
wlc_BSSignorelookup(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec, uchar ssid[],
                    uint ssid_len, bool add)
{
	uint indx;
	uint16 ssid_sum = 0;
	iscan_ignore_t match;
	uint16 band = CHSPEC_BAND(chanspec);
	wlc_scan_utils_t *sui = wlc->sui;

	/* memory savings: compute the sum of the ssid bytes */
	for (indx = 0; indx < ssid_len; ++indx)
		ssid_sum += ssid[indx];

	/* match on bssid/ssid_sum/ssid_len/band */
	bcopy(bssid, &match.bssid, ETHER_ADDR_LEN);
	match.ssid_sum = ssid_sum;
	match.ssid_len = (uint8) ssid_len;

	/* ignore it if it's already in the list */
	for (indx = 0; indx < sui->iscan_ignore_count; indx++) {
		if (!bcmp(&match, &sui->iscan_ignore_list[indx], IGNORE_LIST_MATCH_SZ) &&
		    band == CHSPEC_BAND(sui->iscan_ignore_list[indx].chanspec)) {
			return (int)indx;
		}
	}

	if (add && indx < WLC_ISCAN_IGNORE_MAX) {
		match.chanspec = chanspec;
		bcopy(&match, &sui->iscan_ignore_list[sui->iscan_ignore_count++],
		      sizeof(iscan_ignore_t));
	}

	return -1;
}

/** Called when a frame is received. STA specific. */
static bool
wlc_BSSIgnoreAdd(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec, uchar ssid[],
                    uint ssid_len)
{
	bool rc = FALSE;
	uint indx;
	uint16 ssid_sum = 0;
	iscan_ignore_t new_entry;
	wlc_scan_utils_t *sui = wlc->sui;

	/* memory savings: compute the sum of the ssid bytes */
	for (indx = 0; indx < ssid_len; ++indx) {
		ssid_sum += ssid[indx];
	}
	/* match on bssid/ssid_sum/ssid_len/band */
	bcopy(bssid, &new_entry.bssid, ETHER_ADDR_LEN);
	new_entry.ssid_sum = ssid_sum;
	new_entry.ssid_len = (uint8) ssid_len;

	if (sui->iscan_ignore_count < WLC_ISCAN_IGNORE_MAX) {
		new_entry.chanspec = chanspec;
		bcopy(&new_entry, &sui->iscan_ignore_list[sui->iscan_ignore_count++],
		      sizeof(iscan_ignore_t));
		rc = TRUE;
	} else {
		rc = FALSE;
	}

	return rc;
}

/** Called when a frame is received. STA specific. */
static bool
wlc_BSSignore(wlc_info_t *wlc, uchar *bssid, chanspec_t chanspec, uchar ssid[], uint ssid_len)
{
	int indx;

	indx = wlc_BSSignorelookup(wlc, bssid, chanspec, ssid, ssid_len, FALSE);
	return (indx >= 0);
}
#endif /* STA */

/**
 * Decide what bss type (DOT11_BSSTYPE_XXXX) the bcn/prbrsp comes from
 * when both ESS and IBSS are 0s or 1s (0s is legitimate; 1s is not).
 */
static int
wlc_recv_scan_decide_bss_type(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh,
	struct dot11_bcn_prb *hdr, uint8 *body, uint body_len)
{
	return DOT11_BSSTYPE_INDEPENDENT;
}

/** Called by e.g. wlc_rx.c and wlc_recv_scan_parse() */
int
wlc_recv_scan_parse_bcn_prb(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh,
	struct ether_addr *bssid, bool beacon,
	uint8 *body, uint body_len, wlc_bss_info_t *bi)
{
	struct dot11_bcn_prb *fixed = (struct dot11_bcn_prb *) body;
	wlc_rateset_t sup_rates, ext_rates;
	uint16 ft;
	uint16 cap;
	uint16 chan;
	chanspec_t rx_chspec;
	int err = BCME_OK;
	wlc_pre_parse_frame_t ppf;
	wlc_iem_upp_t upp;
	wlc_iem_ft_pparm_t ftpparm;
	wlc_iem_pparm_t pparm;
	wlc_bsscfg_t *cfg = wlc_scan_bsscfg(wlc->scan);

	if (body_len < sizeof(struct dot11_bcn_prb)) {
		WL_INFORM(("wl%d: %s: invalid frame length\n", wlc->pub->unit, __FUNCTION__));
		return (BCME_ERROR);
	}

	/* do not parse the beacon/probe response if BSSID is a NULL or a MULTI cast address */
	if (ETHER_ISMULTI(bssid) || ETHER_ISNULLADDR(bssid)) {
		WL_ERROR(("wl%d: %s: invalid BSSID\n", wlc->pub->unit, __FUNCTION__));
		return BCME_BADARG;
	}

	body += DOT11_BCN_PRB_LEN;
	body_len -= DOT11_BCN_PRB_LEN;

	/* grab the chanspec from the rxheader */
	rx_chspec = wlc_recv_mgmt_rx_chspec_get(wlc, wrxh);

	/* Fill in bss with new info */
	bzero((char*)bi, sizeof(wlc_bss_info_t));

	bcopy((char *)bssid, (char *)&bi->BSSID, ETHER_ADDR_LEN);

	cap = ltoh16_ua(&fixed->capability);
	if ((cap & DOT11_CAP_ESS) && !(cap & DOT11_CAP_IBSS))
		bi->bss_type = DOT11_BSSTYPE_INFRASTRUCTURE;
	else if (!(cap & DOT11_CAP_ESS) && (cap & DOT11_CAP_IBSS))
		bi->bss_type = DOT11_BSSTYPE_INDEPENDENT;
	else {
		/* What would be the appropriate bss_type? */
		bi->bss_type = (int8)
			wlc_recv_scan_decide_bss_type(wlc, cfg, wrxh, fixed, body, body_len);
	}
	bi->capability = cap;

	bi->flags |= beacon ? WLC_BSS_BEACON : 0;

	bi->RSSI = wrxh->rssi;
	bi->flags2 |= (bi->RSSI == WLC_RSSI_INVALID) ? WLC_BSS_RSSI_INVALID : 0;

	bi->beacon_period = ltoh16_ua(&fixed->beacon_interval);

	ft = beacon ? WLC_IEM_FC_SCAN_BCN : WLC_IEM_FC_SCAN_PRBRSP;

	/* prepare pre-parse call */
	bzero(&sup_rates, sizeof(sup_rates));
	bzero(&ext_rates, sizeof(ext_rates));
	chan = 0xffff;
	bzero(&ppf, sizeof(ppf));
	ppf.sup = &sup_rates;
	ppf.ext = &ext_rates;
	ppf.chan = &chan;

	/* parse rateset and pull out raw rateset */
	if (wlc_scan_pre_parse_frame(wlc, NULL, ft, body, body_len, &ppf) != BCME_OK) {

		return (BCME_ERROR);
	}
	wlc_combine_rateset(wlc, &sup_rates, &ext_rates, &bi->rateset);

	/* check the freq. */
	if (chan != 0xffff) {
		/* TODO:6GHZ: how are 6g channels specified in beacons */
		/* other code should be restricting this value */
		ASSERT(chan <= 0xff);
		if (!CH_NUM_VALID_RANGE(chan) ||
		   (!wlc_valid_chanspec_db(wlc->cmi,
		      (chan | WL_CHANSPEC_BW_20 | WL_CHANNEL_BAND(chan))) &&
		    !(wlc->scan->state & SCAN_STATE_PROHIBIT))) {
			WL_INFORM(("%s: bad channel in beacon: %d\n", __FUNCTION__, chan));
			return (BCME_BADCHAN);
		}
	} else {
		/* 802.11a beacons don't have a DS tlv in them.
		 * Figure it out from the current mac channel.
		 */
		chan = CHSPEC_CHANNEL(rx_chspec); /* TODO:6GHZ: use chspec instead of chan ? */

		/* other code should be restricting this value */
		ASSERT(chan <= 0xff);

		if (!CH_NUM_VALID_RANGE(chan)) {
			WL_INFORM(("wl%d:%s: bad chanspec fr rx_chan: %x->%x\n",
				wlc->pub->unit, __FUNCTION__, CHSPEC_CHANNEL(rx_chspec), chan));
			return BCME_BADCHAN;
		}
	}

	/* flag if this is an RSSI reading on same channel as the bcn/prb was transmitted */
	if (CHSPEC_CHANNEL(rx_chspec) == chan) {  /* TODO:6GHZ: use chspec instead of chan ? */
		bi->flags |= WLC_BSS_RSSI_ON_CHANNEL;
	}

	/* extract phy_noise from the phy */
	bi->phy_noise = phy_noise_avg(WLC_PI(wlc));

	/* extract SNR from rxh */
	bi->SNR = (int16)wlc_lq_recv_snr_compute(wlc, (int8)bi->RSSI, bi->phy_noise);

	/* DS parameters */
	/* TODO:6GHZ: extract 6Ghz channel from beacon */
	bi->chanspec = (WL_CHANSPEC_BW_20 | WL_CHANNEL_BAND(chan) | chan);

	/* prepare IE mgmt calls */
	wlc_iem_parse_upp_init(wlc->iemi, &upp);
	bzero(&ftpparm, sizeof(ftpparm));
	ftpparm.scan.result = bi;
	ftpparm.scan.chan = (uint8)chan;
	bzero(&pparm, sizeof(pparm));
	pparm.ft = &ftpparm;
	pparm.wrxh = wrxh;
	pparm.rxchan = CHSPEC_CHANNEL(rx_chspec);  /* TODO:6GHZ: use chspec instead of chan */

	/* parse IEs */
	err = wlc_iem_parse_frame(wlc->iemi, cfg, ft, &upp, &pparm, body, body_len);

	/* check for bogus fields populated when parsing IEs */
	if (err == BCME_OK) {
		if (!CH_NUM_VALID_RANGE(CHSPEC_CHANNEL(bi->chanspec)) ||
			wf_chspec_malformed(bi->chanspec)) {
			WL_INFORM(("wl%d:%s: bad chanspec fr parse ie: %x\n",
				wlc->pub->unit, __FUNCTION__, bi->chanspec));
			err = BCME_BADCHAN;
		}
	}
	return err;
} /* wlc_recv_scan_parse_bcn_prb */

/* other interfaces... */

bool
wlc_scan_utils_iscan_inprog(wlc_info_t *wlc)
{
	wlc_scan_utils_t *sui = wlc->sui;

	return sui->custom_iscan_results_state == WL_SCAN_RESULTS_PENDING;
}

void
wlc_scan_utils_set_chanspec(wlc_info_t *wlc, chanspec_t chanspec)
{
	wlc_scan_utils_t *sui = wlc->sui;

	sui->iscan_chanspec_last = chanspec;
}

void
wlc_scan_utils_set_syncid(wlc_info_t *wlc, uint16 syncid)
{
	wlc_scan_utils_t *sui = wlc->sui;

	sui->cmn->escan_sync_id = syncid;
}

int
wlc_scan_utils_rx_scan_register(wlc_info_t *wlc, scan_utl_rx_scan_data_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->sui->scan_data_h;

	return bcm_notif_add_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

int
wlc_scan_utils_rx_scan_unregister(wlc_info_t *wlc, scan_utl_rx_scan_data_fn_t fn, void *arg)
{
	bcm_notif_h hdl = wlc->sui->scan_data_h;

	return bcm_notif_remove_interest(hdl, (bcm_notif_client_callback)fn, arg);
}

#ifdef STA
static void
wlc_scan_utils_scan_data_notif(wlc_info_t *wlc, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_management_header *hdr, uint8 *body, int body_len, wlc_bss_info_t *bi)
{
	scan_utl_scan_data_t scan_data;
	bcm_notif_h hdl = wlc->sui->scan_data_h;

	scan_data.wrxh = wrxh;
	scan_data.plcp = plcp;
	scan_data.hdr = hdr;
	scan_data.body = body;
	scan_data.body_len = body_len;
	scan_data.bi = bi;

	bcm_notif_signal(hdl, &scan_data);
}
#endif /* STA */

#ifdef WL_SHIF
int
wlc_scan_utils_shubscan_request(wlc_info_t *wlc)
{
	wlc_ssid_t ssid;
	int ret;

	memset(&ssid, 0, sizeof(ssid));

	ret = wlc_scan_request_ex(wlc, DOT11_BSSTYPE_ANY, &ether_bcast,
			1, &ssid, DOT11_SCANTYPE_ACTIVE, -1, -1, -1, -1, NULL,
			0, 0, FALSE, wlc_scan_utils_shubscan_complete, wlc,
			WLC_ACTION_LPRIO_EXCRX, WL_SCANFLAGS_OFFCHAN,
			NULL, NULL, NULL);

	return ret;
}

void
wlc_scan_utils_shubscan_complete(void *arg, int status, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	wlc_scan_utils_t *sui = wlc->sui;

	wl_shub_scan_complete_cb(wlc, status, cfg);
	wlc_bss_list_free(wlc, sui->custom_scan_results);
}
#endif /* WL_SHIF */
