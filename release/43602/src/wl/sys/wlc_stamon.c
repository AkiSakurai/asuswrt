/*
 * STA monitor implementation
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_stamon.c $
 */

/**
 * @file
 * @brief
 * This is an AP/router specific feature.
 */


#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_stamon.h>
#include <wlc_patch.h>

/* wlc access macros */
#define WLCUNIT(x) ((x)->wlc->pub->unit)
#define WLCPUB(x) ((x)->wlc->pub)
#define WLCOSH(x) ((x)->wlc->osh)
#define WLCHW(x) ((x)->wlc->hw)
#define WLCREGS(x) ((x)->wlc->regs)
#define WLC(x) ((x)->wlc)
#define WLCCLK(x) ((x)->wlc->clk)
/* other helper macros */
#define STAMON_SUPPORT(x)	((x)->support)
#define RCM_CTL(x) (WLCREGS((x))->u_rcv.d11regs.rcm_ctl)
#define RCM_COND_DLY(x) (WLCREGS((x))->u_rcv.d11regs.rcm_cond_dly)

struct wlc_stamon_sta_cfg_entry {
	struct ether_addr ea;
	bool sniff_enab; /* TRUE means this station is currently is being sniffed by ucode */
};

struct wlc_stamon_info {
	wlc_info_t *wlc;
	struct wlc_stamon_sta_cfg_entry *stacfg;
	uint32 stacfg_memsize; /* memory size of the stacfg array */

	bool support;		/* STA monitor feature support or not */
	uint16 stacfg_num; /* Total number of currently configured STAs */
	uint16 amt_start_idx;
	uint16 amt_max_idx;
};
#define STACFG_MAX_ENTRY(_stamon_info_) \
	((_stamon_info_)->stacfg_memsize/sizeof(struct wlc_stamon_sta_cfg_entry))

/* IOVar table */
enum {
	IOV_STA_MONITOR = 0,	/* Add/delete the monitored STA's MAC addresses. */
	IOV_LAST
};

static const bcm_iovar_t stamon_iovars[] = {
	{"sta_monitor", IOV_STA_MONITOR,
	(IOVF_SET_UP), IOVT_BUFFER, sizeof(wlc_stamon_sta_config_t)
	},

	{NULL, 0, 0, 0, 0 }
};

/* **** Private Functions Prototypes *** */

/* Forward declarations for functions registered for this module */
static int wlc_stamon_doiovar(
		    void                *hdl,
		    const bcm_iovar_t   *vi,
		    uint32              actionid,
		    const char          *name,
		    void                *p,
		    uint                plen,
		    void                *a,
		    int                 alen,
		    int                 vsize,
		    struct wlc_if       *wlcif);

static bool wlc_stamon_chip_supported(wlc_stamon_info_t *stamon_ctxt);
static bool wlc_stamon_is_pre_ac_chip(wlc_stamon_info_t *stamon_ctxt);


/* **** Public Functions *** */
/*
 * Initialize the sta monitor private context and resources.
 * Returns a pointer to the sta monitor private context, NULL on failure.
 */
wlc_stamon_info_t *
BCMATTACHFN(wlc_stamon_attach)(wlc_info_t *wlc)
{
	wlc_pub_t *pub = wlc->pub;
	wlc_stamon_info_t *stamon_ctxt;
	uint32 i = 0;
	uint32 malloc_size, stacfg_memsize;

	stacfg_memsize = (sizeof(struct wlc_stamon_sta_cfg_entry) *
		AMT_MAX_STA_MONITOR);
	malloc_size = sizeof(wlc_stamon_info_t) + stacfg_memsize;

	stamon_ctxt = (wlc_stamon_info_t*)MALLOC(pub->osh, malloc_size);
	if (stamon_ctxt == NULL) {
		WL_ERROR(("wl%d: %s: stamon_ctxt MALLOC failed; total "
			"mallocs %d bytes\n",
			wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* Initializing stamon_ctxt structure */
	stamon_ctxt->wlc = wlc;
	stamon_ctxt->stacfg_memsize = stacfg_memsize;
	stamon_ctxt->stacfg =
		(struct wlc_stamon_sta_cfg_entry*)(((uint8*)stamon_ctxt) +
		sizeof(wlc_stamon_info_t));

	stamon_ctxt->amt_max_idx = AMT_MAXIDX_STA_MONITOR + 1;
	stamon_ctxt->amt_start_idx = stamon_ctxt->amt_max_idx -
		AMT_MAX_STA_MONITOR;
	stamon_ctxt->stacfg_num = 0;

	for (i = 0; i < STACFG_MAX_ENTRY(stamon_ctxt); i++) {
		bcopy(&ether_null, &stamon_ctxt->stacfg[i].ea, ETHER_ADDR_LEN);
		stamon_ctxt->stacfg[i].sniff_enab = FALSE;
	}

	/* Check whether current mac core revision supports STA monitor feature */
	stamon_ctxt->support = wlc_stamon_chip_supported(stamon_ctxt);

	/* register module */
	if (wlc_module_register(
			    wlc->pub,
			    stamon_iovars,
			    "sta_monitor",
			    stamon_ctxt,
			    wlc_stamon_doiovar,
			    NULL,
			    NULL,
			    NULL)) {
				WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
				    WLCUNIT(stamon_ctxt),
				    __FUNCTION__));

				goto fail;
			    }


	/* Self enabling the STA monitor feature */
	wlc->pub->_stamon = TRUE;

	return stamon_ctxt;

fail:
	MFREE(pub->osh, stamon_ctxt, malloc_size);

	return NULL;
}

/*
 * Release net detect private context and resources.
 */
void
BCMATTACHFN(wlc_stamon_detach)(wlc_stamon_info_t *stamon_ctxt)
{
	if (stamon_ctxt != NULL) {
		/* Disabling the STA monitor feature */
		WLCPUB(stamon_ctxt)->_stamon = FALSE;

		/* Unregister the module */
		wlc_module_unregister(WLCPUB(stamon_ctxt), "sta_monitor", stamon_ctxt);

		/* Free the all context memory */
		MFREE(WLCOSH(stamon_ctxt), stamon_ctxt,
			(sizeof(wlc_stamon_info_t) + stamon_ctxt->stacfg_memsize));
	}
}


/*
 * ea:  MAC address of STA to enable/disable sniffing.
 *      NULL means enable sniffing for all configured STAs.
 * enable: FALSE/TRUE - disable/enable sniffing.
 *
 * NOTE: currently only one STA is supported.
 */
int
wlc_stamon_sta_sniff_enab(wlc_stamon_info_t *stamon_ctxt,
	struct ether_addr *ea, bool enab)
{
	uint16 attr = 0;
	uint8 i = 0;
	bool cmd_apply = FALSE;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf [ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */

	if (stamon_ctxt == NULL)
		return BCME_UNSUPPORTED;

	if (!STAMON_SUPPORT(stamon_ctxt)) {
		WL_ERROR(("wl%d: %s: STA monitor feature is not supported.\n",
			WLCUNIT(stamon_ctxt), __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	for (i = 0; i < STACFG_MAX_ENTRY(stamon_ctxt); i++) {
		cmd_apply = FALSE;
		if (ea != NULL) {
			if (bcmp(ea, &stamon_ctxt->stacfg[i].ea, ETHER_ADDR_LEN) == 0)
				cmd_apply = TRUE;
		} else {
			cmd_apply = TRUE;
		}

		if (cmd_apply) {
			if (enab) {
				if (wlc_stamon_is_pre_ac_chip(stamon_ctxt)) {
					wlc_bmac_set_rxe_addrmatch(WLCHW(stamon_ctxt),
						RCM_F_BSSID_0_OFFSET, &stamon_ctxt->stacfg[i].ea);
					/* fix missing ucode inits */
					if (WLCCLK(stamon_ctxt)) {
						W_REG(WLCOSH(stamon_ctxt),
							&RCM_CTL(stamon_ctxt), 0xb);
						W_REG(WLCOSH(stamon_ctxt),
							&RCM_COND_DLY(stamon_ctxt),
							0x020b);
					}
					WL_INFORM(("wl%d: %s: Sniffing enabled for STA:[%s]\n",
						WLCUNIT(stamon_ctxt), __FUNCTION__,
						bcm_ether_ntoa(&stamon_ctxt->stacfg[i].ea, eabuf)));
				} else {
					attr = (AMT_ATTR_VALID | AMT_ATTR_A2);
					wlc_bmac_write_amt(WLCHW(stamon_ctxt),
						(stamon_ctxt->amt_start_idx + i),
						&stamon_ctxt->stacfg[i].ea, attr);

					wlc_set_shm(WLC(stamon_ctxt), M_STA_MONITOR_N,
						(stamon_ctxt->amt_start_idx + i),
						sizeof(stamon_ctxt->amt_start_idx));

					stamon_ctxt->stacfg[i].sniff_enab = TRUE;

					WL_INFORM(("wl%d: %s: Sniffing enabled for "
						"STA:[%s] AMT:[attr:%04X idx:%d].\n",
						WLCUNIT(stamon_ctxt), __FUNCTION__,
						bcm_ether_ntoa(&stamon_ctxt->stacfg[i].ea, eabuf),
						attr, (stamon_ctxt->amt_start_idx + i)));
				}
			} else { /* disable */
				if (wlc_stamon_is_pre_ac_chip(stamon_ctxt)) {
					wlc_bmac_set_rxe_addrmatch(WLCHW(stamon_ctxt),
						RCM_F_BSSID_0_OFFSET, &ether_null);
					/* fix missing ucode inits */
					if (WLCCLK(stamon_ctxt)) {
						W_REG(WLCOSH(stamon_ctxt),
							&RCM_CTL(stamon_ctxt),
							0xb);
						W_REG(WLCOSH(stamon_ctxt),
							&RCM_COND_DLY(stamon_ctxt),
							0x020b);
					}
					WL_INFORM(("wl%d: %s: Sniffing disabled for STA:[%s]\n",
						WLCUNIT(stamon_ctxt), __FUNCTION__,
						bcm_ether_ntoa(&stamon_ctxt->stacfg[i].ea, eabuf)));
				} else {
					wlc_bmac_write_amt(WLCHW(stamon_ctxt),
						(stamon_ctxt->amt_start_idx + i), &ether_null, 0);

					wlc_set_shm(WLC(stamon_ctxt), M_STA_MONITOR_N, 0xffff,
						sizeof(stamon_ctxt->amt_start_idx));

					stamon_ctxt->stacfg[i].sniff_enab = FALSE;

					WL_INFORM(("wl%d: %s: Sniffing disabled for "
						"STA:[%s] AMT:[attr:%04X idx:%d].\n",
						WLCUNIT(stamon_ctxt), __FUNCTION__,
						bcm_ether_ntoa(&stamon_ctxt->stacfg[i].ea, eabuf),
						attr, (stamon_ctxt->amt_start_idx + i)));
				}
			}
		}
	}

	return BCME_OK;
}

/* Add the MAC address of the STA into the STA list.
 * If "STA monitor mode" is enabled the sniffing will automatically started
 * for the frames which A2(transmitter) MAC address in 802.11 header will
 * match the specified MAC address. The new added MAC address would overwrite
 * previously added MAC address and sniffing will be continued for new MAC
 * address matches.
 * NOTE: It is allowed to add the STA MAC to STA list if "STA monitor mode"
 * is NOT enabled.
 */
int
wlc_stamon_sta_config(wlc_stamon_info_t *stamon_ctxt, wlc_stamon_sta_config_t* cfg)
{
	uint8 i;

	if (stamon_ctxt == NULL)
		return BCME_UNSUPPORTED;

	ASSERT(cfg != NULL);

	if (!STAMON_SUPPORT(stamon_ctxt)) {
		WL_ERROR(("wl%d: %s: STA monitor feature is not supported.\n",
			WLCUNIT(stamon_ctxt), __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	/* Check MAC address validity. The MAC address must be valid unicast. */
	if (ETHER_ISNULLADDR(&(cfg->ea)) ||
		!ETHER_ISUCAST(&(cfg->ea))) {
		WL_ERROR(("wl%d: %s: Attempt to configure invalid address of monitored STA.\n",
			WLCUNIT(stamon_ctxt), __FUNCTION__));
		return BCME_BADARG;
	}

	if (cfg->cmd == STAMON_CFG_CMD_ADD) {
		/* Add the MAC address of the monitored STA. */
		for (i = 0; i < STACFG_MAX_ENTRY(stamon_ctxt); i++) {
			if (bcmp(&ether_null, &stamon_ctxt->stacfg[i].ea, ETHER_ADDR_LEN) == 0) {
				/* Adding to the list */
				bcopy(&cfg->ea, &stamon_ctxt->stacfg[i].ea, ETHER_ADDR_LEN);
				stamon_ctxt->stacfg_num++;
				/* Start sniffing frames */
				wlc_stamon_sta_sniff_enab(stamon_ctxt,
					&stamon_ctxt->stacfg[i].ea, TRUE);
				break;
			}
		}
	} else if (cfg->cmd == STAMON_CFG_CMD_DEL) {
		/* Delete the MAC address of the monitored STA */
		for (i = 0; i < STACFG_MAX_ENTRY(stamon_ctxt); i++) {
			if (bcmp(&cfg->ea, &stamon_ctxt->stacfg[i].ea, ETHER_ADDR_LEN) == 0) {
				wlc_stamon_sta_sniff_enab(stamon_ctxt,
					&stamon_ctxt->stacfg[i].ea, FALSE);
				bcopy(&ether_null, &stamon_ctxt->stacfg[i].ea, ETHER_ADDR_LEN);
				stamon_ctxt->stacfg_num--;
				break;
			}
		}
	} else
		return BCME_BADARG;

	return BCME_OK;
}

int
wlc_stamon_sta_get(wlc_stamon_info_t *stamon_ctxt, struct ether_addr *ea)
{
	if (stamon_ctxt == NULL)
		return BCME_UNSUPPORTED;

	ASSERT(ea != NULL);

	if (!STAMON_SUPPORT(stamon_ctxt)) {
		WL_ERROR(("wl%d: %s: STA monitor feature is not supported.\n",
			WLCUNIT(stamon_ctxt), __FUNCTION__));
		return BCME_UNSUPPORTED;
	}

	bcopy(&stamon_ctxt->stacfg[0].ea, ea, ETHER_ADDR_LEN);

	return BCME_OK;
}

bool
wlc_stamon_sta_find(wlc_stamon_info_t *stamon_ctxt, struct ether_addr *ea)
{
	uint8 i = 0;

	if (stamon_ctxt == NULL)
		return FALSE;

	ASSERT(ea != NULL);

	if (!STAMON_SUPPORT(stamon_ctxt))
		return FALSE;

	for (i = 0; i < STACFG_MAX_ENTRY(stamon_ctxt); i++)
		if (bcmp(ea, &stamon_ctxt->stacfg[i].ea, ETHER_ADDR_LEN) == 0)
			return TRUE;

	return FALSE;
}

uint16
wlc_stamon_sta_num(wlc_stamon_info_t *stamon_ctxt)
{
	if (stamon_ctxt == NULL)
		return 0;

	if (!STAMON_SUPPORT(stamon_ctxt))
		return 0;

	return stamon_ctxt->stacfg_num;
}

/* **** Private Functions *** */

static int
wlc_stamon_doiovar(
	void                *hdl,
	const bcm_iovar_t   *vi,
	uint32              actionid,
	const char          *name,
	void                *p,
	uint                plen,
	void                *a,
	int                 alen,
	int                 vsize,
	struct wlc_if       *wlcif)
{
	wlc_stamon_info_t	*stamon_ctxt = hdl;
	int			err = BCME_OK;

	BCM_REFERENCE(vi);
	BCM_REFERENCE(name);
	BCM_REFERENCE(p);
	BCM_REFERENCE(plen);
	BCM_REFERENCE(vsize);

	switch (actionid) {
	case IOV_GVAL(IOV_STA_MONITOR):
		{
			wlc_stamon_sta_config_t *stamon_cfg = (wlc_stamon_sta_config_t*)a;
			if (alen < (int)sizeof(wlc_stamon_sta_config_t))
				return BCME_BUFTOOSHORT;
			err = wlc_stamon_sta_get(stamon_ctxt, &stamon_cfg->ea);
			break;
		}
	case IOV_SVAL(IOV_STA_MONITOR):
		{
			wlc_stamon_sta_config_t *stamon_cfg = (wlc_stamon_sta_config_t *)a;
			err = wlc_stamon_sta_config(stamon_ctxt, stamon_cfg);
			break;
		}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static bool
wlc_stamon_chip_supported(wlc_stamon_info_t *stamon_ctxt)
{
	/* Check whether current mac core revision supports STA monitor feature */
	if (D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 24) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 28) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 29) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 30) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 40) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 42))
		return TRUE;
	return FALSE;
}

static bool
wlc_stamon_is_pre_ac_chip(wlc_stamon_info_t *stamon_ctxt)
{
	if (D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 24) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 28) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 29) ||
		D11REV_IS(WLCPUB(stamon_ctxt)->corerev, 30))
		return TRUE;
	return FALSE;
}
