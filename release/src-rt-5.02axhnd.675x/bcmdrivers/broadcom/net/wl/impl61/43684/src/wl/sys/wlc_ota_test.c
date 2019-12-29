/*
* WLOTA feature.
* On a high level there are two modes of operation
* 1. Non Tethered Mode
* 2. Tethered Mode
*
* IN non tethered mode, a cmd flow file which contains encoded test
*	information is downloaded to device.
*	Format of the cmd flow file is pre defined. Host interprest the cmd flow file
*	and passes down a test "structure" to dongle.
*	Reading and parsing can be done by brcm wl utility or any host software which can do
*	the same operation.
*
*	Once cmd flow file is downloaded, a "trigger" cmd is
*	called to put the device into testing mode. It will wait for a sync packet from
* 	tester as a part of handshake mechanism. if device successfully decodes sync packet
*	from an expected mac address, device is good to start with the test sequece.
*	Right now only two kinds of test are downloaded to device.
*		ota_tx
*		ota_rx
*
*	ota_tx/ota_rx takes in arguments as
*	test chan bandwidth contrlchan rates stf txant rxant tx_ifs tx_len num_pkt pwrctrl
*		start:delta:end
*
*	Cmd flow file should have a test setup information like various mac address, sycn timeout.
*	Format is:  synchtimeoout(seconds) synchbreak/loop synchmac txmac rxmac
*
* In tethered mode, test flow is passed down in form of wl iovars through batching mode
*	Sequence of operation is
*	test_stream start	[start batching mode operation]
*	test_stream test_setup  [where test_setup is of the same format in cmd flow file]
*	test_stream test_cmd	[should be of same format of ota_tx /ota_rx in cmd_flow file]
*	test_stream stop	[stops batching mode operation and downloads the file to dongle]
*$Id: wlc_ota_test.c 772932 2019-03-07 09:10:45Z $
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
*/

#ifdef WLOTA_EN

/* ---------------------Include files -------------------------- */
#include <wlc_cfg.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc_pio.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <wlc_types.h>
#include <wl_export.h>
#include <wlc_ota_test.h>
#include <wlc_phy_hal.h>
#ifdef WLRSDB
/* allow BW_xxMHZ (defined in wlc_rate.h) being re-defined in
 * components/phy/old/wlc_phy_int.h.
 */
#undef BW_20MHZ
#undef BW_40MHZ
#undef BW_80MHZ
#undef BW_160MHZ
#include <wlc_phy_int.h>
#endif /* WLRSDB */
#include <wlc_phy_types.h>
#include <phy_calmgr_api.h>
#include <wlc_iocv.h>
#include <wlc_rsdb.h>
#include <pcie_core.h>

#ifdef WLC_SW_DIVERSITY
#include <wlc_swdiv.h>
#define SWDIV_TX_RX_CORE0_5G_SHIFT SWDIV_PLCY_BANDOFFSET(SWDIV_BANDSEL_5G)
#endif /* WLC_SW_DIVERSITY */

/* The following can be turned on for debug or unit-tests,
 * for a release/production build, these flags should be turned off.
 */
#undef OTA_TEST_DEBUG		/* OTADebugDrv + unit tests */
#undef OTA_TEST_TRACE_FUNC	/* trace functions */

#ifdef OTA_TEST_DEBUG
#define OTA_TRACE(x)			printf x
#define OTA_TRACE_UNITTEST_PREFIX	"OTADebugDrv: "
#else
#define OTA_TRACE(x)
#define OTA_TRACE_UNITTEST_PREFIX	""
#endif /* OTA_TEST_DEBUG */
#ifdef OTA_TEST_TRACE_FUNC
#define OTA_FUNC_ENTER			printf("----------> %s \n", __FUNCTION__);
#define OTA_FUNC_EXIT			printf("<---------- %s \n", __FUNCTION__);
#else
#define OTA_FUNC_ENTER
#define OTA_FUNC_EXIT
#endif /* OTA_TEST_TRACE_FUNC */

#ifdef OTA_TEST_DEBUG
/* Enable WL_INFORM.
 * WL_INFORM should be defined if using 'inform' build option, however,
 * config_pcie_mfgtest_swdiv failed to build with 'inform' option.
 */
#undef WL_INFORM
#define	WL_INFORM(args)		do {if (wl_msg_level & WL_INFORM_VAL) WL_PRINT(args);} while (0)
#endif /* OTA_TEST_DEBUG */

#define PKTENG_DURATION 2000  /* pkteng duration in msec */
#define OTA_SYNC_PASS -1 /* Proceed on sync failure */
#define OTA_SYNC_FAIL 0 /* Exit on Sync failure */
#define OTA_SYNC_RETRY_FAIL 1 /* Retry to sync ; Exit on failure */
#define OTA_SYNC_RETRY_PASS -2 /* Retry to sync ; Proceed even on sync failure */

#define OTA_TRIGGER_ALL 2 /* To trigger ota test on both cores */

/* To Trigger test on other slice after test completes on current slice */
#define OTA_TRIGGER_ON_BOTH_CORES_SEQUENTIALLY 3
#define OTA_TRIGGER_RESET 0 /* Reset value for ota_trigger */
#define OTA_SYNC_BLOCKING_MODE 1
#define OTA_SYNC_UNBLOCK_MODE 0

#define OTA_DEFAULT_PKTENG_MODE WL_PKTENG_SYNCHRONOUS_UNBLK
#define OTA_TEST_SCHEDULE_DELAY 50 /* 50 ms delay before scheduling each test timer */
#define OTA_SYNC_RETRY_DELAY 10 /* 10ms delay before scheduling each ota_sync retry */
#define OTA_ASYNC_TIMER_INTERVAL 1 /* 50ms interval for async_timer */

static void wlc_schedule_ota_test(void *arg);
static void wlc_schedule_ota_tx_test(void *arg);
static void wlc_ota_test_wait_for_sync(void *arg);
static void wlc_ota_test_arbitrate(wlc_info_t * wlc);
static void wlc_ota_test_engine_reset(wlc_info_t * wlc);
static void wlc_ota_trigger_test_engine(wlc_info_t * wlc);
static void wlc_ota_test_cleanup(ota_test_info_t * ota_info);
static void wlc_ota_test_exit_engine(wlc_info_t *wlc);
static void wlc_ota_async_mode_cb(void *arg);
static void wlc_ota_move_to_next_test(wlc_info_t *wlc);
static void wlc_ota_initiate_trigger(wlc_info_t *wlc);
static int wlc_ota_test_rssi_get(wlc_info_t * wlc, ota_test_info_t * ota_info, void *arg, int len);
STATIC uint16 wlc_ota_test_rssi_maxcnt(ota_test_info_t * ota_info);
static void wlc_ota_sync_fail_handle(wlc_info_t *wlc, int8 skip_test_reason);
static void wlc_ota_test_stop_handle(wlc_info_t *wlc);

/* device info */
typedef struct ota_device_info {
	bool	bInit;
	/* for antenna diversity */
	bool	b_swdiv_rx_supported;
	uint32	swdiv_rx_policy;	/* 4 cores */
} ota_device_info_t;
static void wlc_ota_test_get_devinfo(wlc_info_t *wlc, ota_device_info_t *devinfo);
static int wlc_ota_test_is_ant_valid(wlc_info_t *wlc, uint8 ant_mask);
static void wlc_ota_test_setup_antenna(wlc_info_t *wlc, wl_ota_test_args_t *test_arg);
#ifdef WLC_SW_DIVERSITY
static int wlc_ota_test_force_antenna(wlc_info_t *wlc, uint8 ant_mask, bool bTxCmd);
#endif /* WLC_SW_DIVERSITY */

#define OTA_SYNC_RX_NFRAMES 1

typedef struct /* rxd_sync_info */ {
	wl_ota_rx_rssi_t rssi;
	bool		rssi_valid; /* set to TRUE if 'rssi' is valid */
} rxd_sync_info_t;
static void wlc_ota_get_rx_rssi(wlc_info_t *wlc, int rxd_pktcnt, bool is_rxd_sync,
	wl_ota_rx_rssi_t *rx_rssi);
static void wlc_ota_store_rx_rssiEx(wlc_info_t *wlc);
static void wlc_ota_store_rxd_sync_rssi(wlc_info_t *wlc, int rxd_pktcnt, bool bSet);
static void *wlc_ota_alloc_rxdsync_pkteng(wlc_info_t *wlc, wl_pkteng_t *in_pkteng, int *out_plen);
static void ota_set_rssi_average(wlc_info_t *wlc, int pkt_nframes, bool bSet);

/* Main OTA test structure */
struct ota_test_info {
	/* Pointer back to wlc structure */
	wlc_info_t *wlc;

	wl_ota_test_vector_t * test_vctr;	/* test vector */

	struct wl_timer *test_timer ;  /* timer to track ota test */
	struct wl_timer *tx_test_timer; /* timer to track lp tx test */
	struct wl_timer *sync_timer;	/* timer to track sync operation */

	int16 test_phase;		/* cur test cnt. Shows the index of test being run */
	uint16 tx_test_phase;		/* cur tx test phase */
	uint8 test_stage;		/* Test stages 1. idle 2. active 3. success 4. fail */
	uint8 download_stage;		/* download stage */
	int8 test_skip_reason;		/* test fail reason */
	uint8 test_loop_cnt;		/* Looping cnt in dbg mode */
	uint8 ota_sync_status;		/* OTA sync status */
	uint8 ota_sync_wait_cnt;	/* Cntr maintained to check no of iteration for sync */
	wl_ota_rx_rssi_t *rx_rssi; /* Pointer to the array of RSSI */
	struct wl_timer *async_mode_timer; /* timer  to track status of test in async mode */
	bool sync_mode;			/* Sync_mode 1= sync 0=sync_unblk mode of pkteng is used */
	uint32 sync_rx_start_cnt;	/* Starting value rx for ota_sync */
	uint32 ota_trigger_val;		/* Parameter given for ota_trigger iovar */
	ota_device_info_t default_devinfo;

	/* support rssi report for sync-pkt */
	rxd_sync_info_t		rxd_sync_info;
	uint16			sync_pktcnt; /* expected sync-pkts for ota_sync */

};
/* ota test iovar function */
STATIC int
ota_test_doiovar
(
	void                                    *hdl,
	uint32                          actionid,
	void                                    *p,
	uint                                    plen,
	void                                    *a,
	uint                                     alen,
	uint                                     vsize,
	struct wlc_if           *wlcif
);

#define UOTA_RSSI_MAXPKTS	((1<< C_UOTA_RXFST_NBIT) - 1)

/* OTA test iovar enums */
enum {
	IOV_OTA_TRIGGER,
	IOV_OTA_LOADTEST,
	IOV_OTA_TESTSTATUS,
	IOV_OTA_TESTSTOP,
	IOV_OTA_RSSI,
	IOV_OTA_SYNC_PKTCNT,
};
static const bcm_iovar_t ota_test_iovars[] = {
	{"ota_trigger", IOV_OTA_TRIGGER,
	(IOVF_OPEN_ALLOW), 0, IOVT_BOOL, 0
	},
	{"ota_loadtest", IOV_OTA_LOADTEST,
	(0), 0, IOVT_BUFFER, WL_OTA_ARG_PARSE_BLK_SIZE,
	},
	{"ota_teststatus", IOV_OTA_TESTSTATUS,
	(0), 0, IOVT_BUFFER, sizeof(wl_ota_test_status_t),
	},
	{"ota_teststop", IOV_OTA_TESTSTOP,
	(0), 0, IOVT_BOOL, 0,
	},
	{"ota_rssi", IOV_OTA_RSSI,
	(0), 0, IOVT_BUFFER, sizeof(wl_ota_test_rssi_t),
	},
	{ "ota_syncpktcnt", IOV_OTA_SYNC_PKTCNT,
	(0), 0, IOVT_UINT16, 0,
	},
	{NULL, 0, 0, 0, 0, 0 }
};
ota_test_info_t *
BCMATTACHFN(wlc_ota_test_attach)(wlc_info_t *wlc)
{
	ota_test_info_t * ota_test_info = NULL;

	/* ota test info */
	ota_test_info = (ota_test_info_t *)MALLOCZ(wlc->osh, sizeof(ota_test_info_t));
	if (ota_test_info == NULL) {
		WL_ERROR(("wl%d: %s: MALLOCZ failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* attach ota test vector */
	ota_test_info->test_vctr = (wl_ota_test_vector_t *)MALLOCZ(wlc->osh,
		sizeof(wl_ota_test_vector_t));
	if (ota_test_info->test_vctr == NULL) {
		WL_ERROR(("wl%d: %s: MALLOCZ failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* Timers for wl ota test */
	if (!(ota_test_info->test_timer = wl_init_timer(wlc->wl, wlc_schedule_ota_test, wlc,
		"test_timer"))) {
		WL_ERROR(("wl%d:  wl_init_timer for test_timer failed\n", wlc->pub->unit));
		goto fail;
	}
	/* Timer for tracing the test_status in case of async mode for tx */
	if (!(ota_test_info->async_mode_timer = wl_init_timer(wlc->wl, wlc_ota_async_mode_cb, wlc,
		"async_mode_timer"))) {
		WL_ERROR(("wl%d:  wl_init_timer for async_mode_timer failed\n", wlc->pub->unit));
		goto fail;
	}

	/* Split up the full ota test cases so that tx buffer issues are not there */
	if (!(ota_test_info->tx_test_timer = wl_init_timer(wlc->wl, wlc_schedule_ota_tx_test, wlc,
		"tx_test_timer"))) {
		WL_ERROR(("wl%d:  wl_init_timer for tx_test_timer failed\n", wlc->pub->unit));
		goto fail;
	}
	if (!(ota_test_info->sync_timer = wl_init_timer(wlc->wl, wlc_ota_test_wait_for_sync, wlc,
		"sync_timer"))) {
		WL_ERROR(("wl%d:  wl_init_timer for sync_timer failed\n", wlc->pub->unit));
		goto fail;
	}
	ota_test_info->wlc = wlc;
	ota_test_info->test_skip_reason = 0;
	ota_test_info->tx_test_phase = 0;
	ota_test_info->test_phase = -1;
	ota_test_info->test_stage = WL_OTA_TEST_IDLE;
	wlc->iov_block = &ota_test_info->test_stage;

	/* ota test rssi */
	ota_test_info->rx_rssi =
		(wl_ota_rx_rssi_t *)MALLOC(wlc->osh,
		sizeof(wl_ota_rx_rssi_t) * WL_OTA_TEST_MAX_NUM_RSSI);
	if (ota_test_info->rx_rssi == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	memset(ota_test_info->rx_rssi, 0, sizeof(wl_ota_rx_rssi_t) * WL_OTA_TEST_MAX_NUM_RSSI);

	ota_test_info->rxd_sync_info.rssi_valid = FALSE; /* reset */
	ota_test_info->sync_pktcnt = OTA_SYNC_RX_NFRAMES;

	/* Register this module. */
	if (wlc_module_register(wlc->pub,
		ota_test_iovars,
		"ota_test",
		ota_test_info,
		ota_test_doiovar,
		NULL, NULL,
		NULL)) {
		WL_ERROR(("ota_test module registering failed \n"));
		goto fail;
	}

	return ota_test_info;

fail:
	/* Free up the memory */
	wlc_ota_test_cleanup(ota_test_info);

	return NULL;
}

STATIC uint16
wlc_ota_test_rssi_maxcnt(ota_test_info_t * ota_info)
{
	return WL_OTA_TEST_MAX_NUM_RSSI;
}

static void
BCMATTACHFN(wlc_ota_test_cleanup)(ota_test_info_t * ota_info)
{

	if (ota_info) {
		/* release rssi buffer */
		if (ota_info->rx_rssi)
			MFREE(ota_info->wlc->osh, ota_info->rx_rssi,
				sizeof(wl_ota_rx_rssi_t) * wlc_ota_test_rssi_maxcnt(ota_info));

		/* Kill the test timers */
		if (ota_info->test_timer) {
			wl_free_timer(ota_info->wlc->wl, ota_info->test_timer);
			ota_info->test_timer = NULL;
		}

		if (ota_info->tx_test_timer) {
			wl_free_timer(ota_info->wlc->wl, ota_info->tx_test_timer);
			ota_info->tx_test_timer = NULL;
		}

		if (ota_info->sync_timer) {
			wl_free_timer(ota_info->wlc->wl, ota_info->sync_timer);
			ota_info->sync_timer = NULL;
		}
		if (ota_info->async_mode_timer) {
			wl_free_timer(ota_info->wlc->wl, ota_info->async_mode_timer);
			ota_info->async_mode_timer = NULL;
		}

		/* release test vector */
		if (ota_info->test_vctr)
			MFREE(ota_info->wlc->osh, ota_info->test_vctr,
					sizeof(wl_ota_test_vector_t));

		/* Free test info */
		MFREE(ota_info->wlc->osh, ota_info, sizeof(ota_test_info_t));
	}
}
void
BCMATTACHFN(wlc_ota_test_detach)(ota_test_info_t * ota_info)
{

	/* Unregister this module */
	wlc_module_unregister(ota_info->wlc->pub, "ota_test", ota_info->wlc);

	/* Free up the memory */
	wlc_ota_test_cleanup(ota_info);
}

#ifdef OTA_TEST_DEBUG
/*
 * definition for id-string mapping.
 * This is used to map an id to a text-string for debug-display
 */
typedef struct ota_test_strmap_entry {
	int32		id;
	char		*text;
} ota_test_strmap_entry_t;

/*
 * lookup 'id' (as a key) from a table
 * if found, return the entry pointer, otherwise return NULL
 */
static const ota_test_strmap_entry_t*
ota_test_get_strmap_info(int32 id, const ota_test_strmap_entry_t *p_table, uint32 num_entries)
{
	int i;
	const ota_test_strmap_entry_t *p_entry;

	/* scan thru the table till end */
	p_entry = p_table;
	for (i = 0; i < (int) num_entries; i++)
	{
		if (p_entry->id == id)
			return p_entry;
		p_entry++;		/* next entry */
	}

	return NULL;			/* not found */
}

/*
 * map a actionid to a text-string for display
 */
static const char *
ota_test_actionid_to_str(uint32 actionid)
{
	/* iovar mapping */
	static const ota_test_strmap_entry_t ota_test_iovar_strmap [] = {
		/* actionid		string */
		{ IOV_GVAL(IOV_OTA_TESTSTATUS),	"OTA_TESTSTATUS" },
		{ IOV_SVAL(IOV_OTA_LOADTEST),	"OTA_LOADTEST" },
		{ IOV_SVAL(IOV_OTA_TRIGGER),	"OTA_TRIGGER" },
		{ IOV_SVAL(IOV_OTA_TESTSTOP),	"OTA_TESTSTOP" },
		{ IOV_GVAL(IOV_OTA_RSSI),	"OTA_RSSI" },
		{ IOV_SVAL(IOV_OTA_SYNC_PKTCNT), "OTA_SYNC_PKTCNT" },
		{ IOV_GVAL(IOV_OTA_SYNC_PKTCNT), "OTA_SYNC_PKTCNT" },
	};

	const ota_test_strmap_entry_t *p_entry = ota_test_get_strmap_info(actionid,
		&ota_test_iovar_strmap[0], ARRAYSIZE(ota_test_iovar_strmap));

	if (p_entry)
		return (p_entry->text);

	return "invalid";
}

#endif /* OTA_TEST_DEBUG */

STATIC int
ota_test_doiovar(void *hdl, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	int err = BCME_OK; /* 0; */
	ota_test_info_t * ota_info;
	bool bool_val = FALSE;
	int32 int_val = 0;

	ota_info = hdl;
	wlc_info_t *wlc = ota_info->wlc;
	wl_ota_test_vector_t * test_vctr = ota_info->test_vctr;

#ifdef WLRSDB
	int idx;
	wlc_info_t *wlc_iter;
#endif /* WLRSDB */

	OTA_FUNC_ENTER

	OTA_TRACE(("%s enter %s, actionid=%d(%s)\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
		actionid, ota_test_actionid_to_str(actionid)));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	/* convenience int and bool vals for first 4 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {
	case IOV_GVAL(IOV_OTA_TESTSTATUS): {
		uint16 cnt;
		wl_ota_test_status_t * test_status = (wl_ota_test_status_t *)MALLOC(wlc->osh,
			sizeof(wl_ota_test_status_t));

		if (test_status == NULL) {
			/* Malloc failures; Dont proceed */
			err = BCME_NOMEM;
			break;
		}

		/* Copy test details */
		bcopy(params, &cnt, sizeof(uint16));

		test_status->test_cnt = test_vctr->test_cnt;
		test_status->loop_test = test_vctr->loop_test;
		test_status->cur_test_cnt = wlc->ota_info->test_phase;
		test_status->skip_test_reason = wlc->ota_info->test_skip_reason;
		test_status->file_dwnld_valid = test_vctr->file_dwnld_valid;
		test_status->sync_timeout = test_vctr->sync_timeout;
		test_status->sync_fail_action = test_vctr->sync_fail_action;
		test_status->test_stage = wlc->ota_info->test_stage;
		test_status->sync_status = wlc->ota_info->ota_sync_status;

		bcopy(&(test_vctr->sync_mac), &(test_status->sync_mac), sizeof(struct ether_addr));
		bcopy(&(test_vctr->tx_mac), &(test_status->tx_mac), sizeof(struct ether_addr));
		bcopy(&(test_vctr->rx_mac), &(test_status->rx_mac), sizeof(struct ether_addr));

		if ((cnt  > 0) && (cnt <= test_vctr->test_cnt)) {
			bcopy(&(test_vctr->test_arg[cnt - 1]), &(test_status->test_arg),
				sizeof(wl_ota_test_args_t));
		}

		bcopy(test_status, (wl_ota_test_status_t *)arg, sizeof(wl_ota_test_status_t));

		/* Free test pointers */
		MFREE(wlc->osh, test_status, sizeof(wl_ota_test_status_t));
		break;
		}
	case IOV_SVAL(IOV_OTA_LOADTEST): {
		uint8 * ptr1;
		uint8 num_loop;
		uint32 rem;
		uint32 size;
		wl_ota_test_vector_t* input_test_vctr = (wl_ota_test_vector_t*)params;

		num_loop = sizeof(wl_ota_test_vector_t) / WL_OTA_ARG_PARSE_BLK_SIZE;
		rem = sizeof(wl_ota_test_vector_t) % WL_OTA_ARG_PARSE_BLK_SIZE;

		/* use new uint * pnt so that we can do byte wide operation on it */
		ptr1 = (uint8 *)test_vctr;

		if (wlc->ota_info->download_stage == 0) {
			/* reset all test flags */
			wlc_ota_test_engine_reset(wlc);

			bzero(test_vctr, sizeof(wl_ota_test_vector_t));

			if (input_test_vctr->version != WL_OTA_TESTVEC_T_VERSION) {
				err = BCME_VERSION;
				break;
			}
		}

		/* decide whether to copy full WL_OTA_ARG_PARSE_BLK_SIZE bytes of pending bytes */
		size = (wlc->ota_info->download_stage == num_loop) ?
			rem : WL_OTA_ARG_PARSE_BLK_SIZE;

		/* Copy WL_OTA_ARG_PARSE_BLK_SIZE bytes ata atime */
		bcopy(params, (ptr1 + wlc->ota_info->download_stage * WL_OTA_ARG_PARSE_BLK_SIZE),
			size);

		wlc->ota_info->download_stage++;

		if (wlc->ota_info->download_stage == (num_loop + 1)) {
			/* Last stage of copying */
			test_vctr->file_dwnld_valid = TRUE;
			wlc->ota_info->download_stage = 0;
		}
		break;
	}
	case IOV_SVAL(IOV_OTA_TRIGGER):
		if (wlc->ota_info->test_stage == WL_OTA_TEST_ACTIVE) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (bool_val) {
			wlc->ota_info->ota_trigger_val = int_val;
			if ((int_val == OTA_TRIGGER_ALL ||
				int_val == OTA_TRIGGER_ON_BOTH_CORES_SEQUENTIALLY) &&
				test_vctr->loop_test) {
				WL_ERROR(("LOOP test not supported for sequential "
					"and simultaneous trigger.\n"));
				err = BCME_UNSUPPORTED;
				break;
			}

#ifdef WLRSDB
			if (int_val == OTA_TRIGGER_ALL && RSDB_ENAB(wlc->pub)) {
				/* Issue trigger command on both core */
					FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
						if (wlc_iter->ota_info->test_stage ==
						WL_OTA_TEST_ACTIVE) {
							err = BCME_UNSUPPORTED;
							break;
						}
						/* Store the trigger value */
						wlc_iter->ota_info->ota_trigger_val = int_val;
						wlc_ota_initiate_trigger(wlc_iter);
					}
			} else
#endif /* WLRSDB */
			{
				wlc_ota_initiate_trigger(wlc);
			}
		}
		break;
	case IOV_SVAL(IOV_OTA_TESTSTOP):
#ifdef WLRSDB
		if (RSDB_ENAB(wlc->pub) && wlc->ota_info->ota_trigger_val == OTA_TRIGGER_ALL) {
			/* Issue Stop command on both core */
			FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
				wlc_ota_test_stop_handle(wlc_iter);
			}
		} else
#endif // endif
		{
			wlc_ota_test_stop_handle(wlc);
		}
		break;
	case IOV_GVAL(IOV_OTA_RSSI):
		err = wlc_ota_test_rssi_get(wlc, ota_info, arg, len);
		break;

	case IOV_SVAL(IOV_OTA_SYNC_PKTCNT):
	{
		if (int_val >= 1 && int_val <= (uint16)(UOTA_RSSI_MAXPKTS))
			wlc->ota_info->sync_pktcnt = (uint16) int_val;
		else
			err = BCME_BADARG;
		break;
	}

	case IOV_GVAL(IOV_OTA_SYNC_PKTCNT):
	{
		if (arg != NULL && len >= sizeof(uint32))
			*((uint32 *) arg) = (uint32) wlc->ota_info->sync_pktcnt;
		else
			err = BCME_BUFTOOSHORT;

		break;
	}

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	OTA_FUNC_EXIT

	return err;
}

static void
wlc_ota_test_stop_handle(wlc_info_t *wlc)
{
	/* reset all test flags */
	wlc_ota_test_engine_reset(wlc);
	wlc_statsupd(wlc);
	/* wl down */
	/* Halt test on both cores */
	wl_down(wlc->wl);
}

static void
wlc_ota_initiate_trigger(wlc_info_t * wlc)
{
	wlc_ota_trigger_test_engine(wlc);
	memset(wlc->ota_info->rx_rssi, 0,
		sizeof(wl_ota_rx_rssi_t) * wlc_ota_test_rssi_maxcnt
		(wlc->ota_info));
	wlc->ota_info->test_vctr->test_rxcnt = 0;

}
static int
wlc_ota_test_rssi_get(wlc_info_t * wlc, ota_test_info_t * ota_info, void *arg, int len)
{
	wl_ota_test_rssi_t *otr = (wl_ota_test_rssi_t*)arg;
	int ret = 0;
	int bufsz;

	OTA_FUNC_ENTER

	bufsz = (sizeof(wl_ota_rx_rssi_t) * wlc_ota_test_rssi_maxcnt(ota_info))
		+ WL_OTA_TEST_RSSI_FIXED_SIZE;

	/* Check if the buffer passed is enough to fill */
	if (len < bufsz) {
		ret = BCME_BUFTOOSHORT;
		goto exit;
	}

	otr->version = WL_OTARSSI_T_VERSION;
	otr->testcnt = MIN(ota_info->test_vctr->test_rxcnt, wlc_ota_test_rssi_maxcnt(ota_info));
	memcpy((wl_ota_rx_rssi_t *)otr->rx_rssi, ota_info->rx_rssi,
		sizeof(wl_ota_rx_rssi_t) * (otr->testcnt));
	ret = BCME_OK;

exit:
	OTA_FUNC_EXIT
	return ret;
}

static void
wlc_ota_test_engine_reset(wlc_info_t * wlc)
{
	wl_pkteng_t pkteng;

	OTA_FUNC_ENTER

	/*  delete timers */
	wl_del_timer(wlc->wl, wlc->ota_info->test_timer);
	wl_del_timer(wlc->wl, wlc->ota_info->tx_test_timer);
	wl_del_timer(wlc->wl, wlc->ota_info->sync_timer);
	wl_del_timer(wlc->wl, wlc->ota_info->async_mode_timer);

	/* reset test variables */
	wlc->ota_info->test_stage = WL_OTA_TEST_IDLE;
	wlc->ota_info->test_skip_reason = 0;
	wlc->ota_info->test_phase = -1;
	wlc->ota_info->tx_test_phase = 0;
	wlc->ota_info->test_loop_cnt = 0;
	wlc->ota_info->ota_sync_wait_cnt = 0;
	wlc->ota_info->ota_sync_status = WL_OTA_SYNC_IDLE;

	wlc_bmac_pkteng_poll_interval_reset(wlc->hw);

	/* Resetting the status of ota_trigger to avoid continuous triggering */
	wlc->ota_info->ota_trigger_val = OTA_TRIGGER_RESET;

	/* Halt the pkteng if it is running */
	memcpy(&pkteng, 0, sizeof(wl_pkteng_t));
	pkteng.flags = WL_PKTENG_PER_RX_STOP;
	wlc_iovar_op(wlc, "pkteng", NULL, 0, &pkteng, sizeof(wl_pkteng_t), IOV_SET, NULL);

	memcpy(&pkteng, 0, sizeof(wl_pkteng_t));
	pkteng.flags = WL_PKTENG_PER_TX_STOP;
	wlc_iovar_op(wlc, "pkteng", NULL, 0, &pkteng, sizeof(wl_pkteng_t), IOV_SET, NULL);

	wlc_ota_store_rxd_sync_rssi(wlc, 0, FALSE); /* reset */

	OTA_FUNC_EXIT
}
static void
wlc_ota_trigger_test_engine(wlc_info_t * wlc)
{
	OTA_FUNC_ENTER

	/*  delete timers */
	wl_del_timer(wlc->wl, wlc->ota_info->test_timer);
	wl_del_timer(wlc->wl, wlc->ota_info->tx_test_timer);
	wl_del_timer(wlc->wl, wlc->ota_info->sync_timer);
	wl_del_timer(wlc->wl, wlc->ota_info->async_mode_timer);

	/* reset test variables */
	wlc->ota_info->test_skip_reason = 0;
	wlc->ota_info->tx_test_phase = 0;
	wlc->ota_info->test_stage = WL_OTA_TEST_ACTIVE;
	wlc->ota_info->test_phase = 0;
	wlc->ota_info->ota_sync_wait_cnt = 0;

	wlc_ota_store_rxd_sync_rssi(wlc, 0, FALSE); /* reset */

	wl_indicate_maccore_state(wlc->wl, LTR_ACTIVE);

	/* call with a 50ms delay timer */
	wl_add_timer(wlc->wl, wlc->ota_info->test_timer, OTA_TEST_SCHEDULE_DELAY, 0);

	OTA_FUNC_EXIT
}

static void
wlc_ota_test_start_pkteng(wlc_info_t * wlc, uint8 mask, wl_ota_test_args_t *test_arg,
	uint8 sync, bool txSyncRssi)
{
	int new_plen;
	wl_pkteng_t *new_pkteng;
	uint8 dst_addr[6] = {0x00};
	uint8 src_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	wl_pkteng_t pkteng;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;

	OTA_FUNC_ENTER

	/* copy pkteng information from flow file */
	pkteng.delay = test_arg->pkteng.delay;
	pkteng.nframes = test_arg->pkteng.nframes;
	pkteng.length = test_arg->pkteng.length;
	pkteng.seqno = 0;

	wlc->ota_info->sync_mode = (OTA_DEFAULT_PKTENG_MODE == WL_PKTENG_SYNCHRONOUS) ?
		OTA_SYNC_BLOCKING_MODE : OTA_SYNC_UNBLOCK_MODE;
	if (sync) {
		pkteng.flags = OTA_DEFAULT_PKTENG_MODE | mask;
	}
	else {
		pkteng.flags = mask;
	}
	/* copy destination address differently for rx and tx testing */
	if (mask == WL_PKTENG_PER_TX_START) {
		bcopy(&(test_vctr->tx_mac), dst_addr,  sizeof(dst_addr));
	} else if (mask == WL_PKTENG_PER_RX_WITH_ACK_START) {
		bcopy(&(test_vctr->rx_mac), dst_addr,  sizeof(dst_addr));
	}
	bcopy(dst_addr, pkteng.dest.octet, sizeof(dst_addr));
	bcopy(src_addr, pkteng.src.octet, sizeof(src_addr));

	/* check if need to pass sync-RSSI-report to pkteng */
	if (txSyncRssi) {
		new_pkteng = wlc_ota_alloc_rxdsync_pkteng(wlc, &pkteng, &new_plen);
		if (new_pkteng != NULL) {
			/* trigger  pkteng */
			wlc_iovar_op(wlc, "pkteng", NULL, 0, new_pkteng, new_plen, IOV_SET, NULL);
			MFREE(wlc->osh, new_pkteng, new_plen);
		}
		else {
			wlc_iovar_op(wlc, "pkteng", NULL, 0, &pkteng, sizeof(wl_pkteng_t),
				IOV_SET, NULL);
		}

		wlc_ota_store_rxd_sync_rssi(wlc, 0, FALSE); /* reset */

	}
	else {
		/* trigger  pkteng */
		wlc_iovar_op(wlc, "pkteng", NULL, 0, &pkteng, sizeof(wl_pkteng_t), IOV_SET, NULL);
	}

	OTA_FUNC_EXIT
}

/* parse rate string to rspec */
static void
wlc_ota_test_set_rate(wlc_info_t * wlc, wl_ota_test_args_t *test_arg, uint8 i)
{
	uint tx_exp = 0;
	uint32 rspec = 0;
	uint8 val;
	uint8 Nss;
	bool ldpc, sgi;
	uint16 ht_set = test_arg->rt_info.rate_val_mbps[i] & HT_MCS_INUSE;
	uint16 vht_set = test_arg->rt_info.rate_val_mbps[i] & VHT_MCS_INUSE;
	uint32 coremask[2] = {0, 0};
	uint8 mcs_mask[4] = {0, 0, 0, 0}; /* pre-initialize # of streams {core:4 | stream:4} */
	uint8 streams = 1;
	uint8 cores = 0;
#ifdef WLRSDB
	struct phy_info *pi = (struct phy_info *) (wlc->hw->band->pi);
	uint8 max_no_cores = PHYCORENUM(pi->pubpi->phy_corenum);
#else
	uint8 max_no_cores = PHY_CORE_MAX;
#endif // endif

	OTA_FUNC_ENTER

	val = test_arg->rt_info.rate_val_mbps[i] & OTA_RATE_MASK;

	tx_exp = 0;
	ldpc = test_arg->ldpc;
	sgi = test_arg->sgi;
	if (max_no_cores >= 3) {
		if (test_arg->stf_mode == OTA_STF_SISO)
			streams = 1;
		else if (test_arg->stf_mode == OTA_STF_CDD)
			streams = 1;
		else if (test_arg->stf_mode == OTA_STF_STBC)
			streams = 1;
		else if (test_arg->stf_mode == OTA_STF_SDM)
			streams = 3;
		Nss = streams;
		cores = WL_OTA_TEST_GET_CORE(test_arg->txant);
		/* set CCK/OFDM mask to be same as txant */
		coremask[1] |= cores;
		coremask[1] |= cores << 8;
	} else if (max_no_cores >= 2) {
		if (test_arg->stf_mode == OTA_STF_SISO)
			streams = 1;
		else if (test_arg->stf_mode == OTA_STF_CDD)
			streams = 1;
		else if (test_arg->stf_mode == OTA_STF_STBC)
			streams = 1;
		else if (test_arg->stf_mode == OTA_STF_SDM)
			streams = 2;
		Nss = streams;
		cores = WL_OTA_TEST_GET_CORE(test_arg->txant);
		/* set CCK/OFDM mask to be same as txant */
		coremask[1] |= cores;
		coremask[1] |= cores << 8;
	} else {
		Nss = 1; /* default Nss = 1 */
	    /* To set SISO: wl txcore -s 1 -c 1 */
	    /* To set CDD: wl txcore -s 1 -c 3 */
		if (test_arg->stf_mode == OTA_STF_SISO)
			cores = 1;
		else if (test_arg->stf_mode == OTA_STF_CDD)
			cores = 3;
	}

	if (ht_set) {
		rspec = WL_RSPEC_ENCODE_HT;	/* 11n HT */
		rspec |= val;
	} else if (vht_set)  {
		rspec = WL_RSPEC_ENCODE_VHT;	/* 11ac VHT */
		rspec |= (Nss << WL_RSPEC_VHT_NSS_SHIFT) | val;
	} else {
		rspec = WL_RSPEC_ENCODE_RATE;	/* 11abg */
		rspec |= val;
	}

	/* set the other rspec fields */
	rspec |= (tx_exp << WL_RSPEC_TXEXP_SHIFT);
	rspec |= test_arg->bw << WL_RSPEC_BW_SHIFT;

	if (ht_set || vht_set) {
		rspec |= (ldpc ? WL_RSPEC_LDPC : 0);
		rspec |= (sgi  ? WL_RSPEC_SGI  : 0);
	}

	if (test_arg->chan <= CH_MAX_2G_CHANNEL)
		wlc_iovar_op(wlc, "2g_rate", NULL, 0, &rspec, sizeof(rspec), IOV_SET, NULL);
	else
		wlc_iovar_op(wlc, "5g_rate", NULL, 0, &rspec, sizeof(rspec), IOV_SET, NULL);

	if (ht_set || vht_set) {
		mcs_mask[streams-1] = ((cores & 0xf) << 4) | (streams & 0xf);
		coremask[0] |= mcs_mask[0] << 0;
		coremask[0] |= mcs_mask[1] << 8;
		coremask[0] |= mcs_mask[2] << 16;
		coremask[0] |= mcs_mask[3] << 24;

		OTA_TRACE(("%s, ht_set=%d,vht_set=%d,max_no_cores=%d,streams=%d\n",
			__FUNCTION__, ht_set, vht_set, max_no_cores, streams));
		OTA_TRACE(("%s, set via txcore,test_arg(txant=0x%02x,stf_mode=%d),cores=0x%02x\n",
			__FUNCTION__, test_arg->txant, test_arg->stf_mode, cores));

		wlc_iovar_op(wlc, "txcore", NULL, 0, coremask, sizeof(uint32)*2, IOV_SET, NULL);
	}

	OTA_TRACE(("%s exit %s, test_arg->txant=0x%02x, cores=0x%x\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
		test_arg->txant, cores));

	OTA_FUNC_EXIT
}

/* wl txpwr1 -o -q dbm iovar */
static void
wlc_ota_test_set_txpwr(wlc_info_t * wlc, int8 target_pwr)
{
	int32 int_val;

	OTA_FUNC_ENTER

	if (target_pwr != -1)
		int_val = (WL_TXPWR_OVERRIDE | target_pwr);
	else
		int_val = 127;

	/* wl txpwr1 -o -q 60 */
	wlc_iovar_op(wlc, "qtxpower", NULL, 0, &int_val, sizeof(int_val), IOV_SET, NULL);

	OTA_FUNC_EXIT
}
/* Force phy calibrations */
static int8
wlc_ota_test_force_phy_cal(wlc_info_t * wlc)
{

	uint8 wait_ctr = 0;
	int val2;

	OTA_FUNC_ENTER

	wlc_iovar_setint(wlc, "phy_forcecal", PHY_PERICAL_DRIVERUP);

	OSL_DELAY(1000 * 100);
	wait_ctr = 0;

	while (wait_ctr < 5) {
		wlc_iovar_getint(wlc, "phy_activecal", &val2);
		if (val2 == 0)
			break;
		else
			OSL_DELAY(1000 * 10);
		wait_ctr++;
	}
	if (wait_ctr == 5) {
		WL_ERROR(("Force cal failure \n"));
		return WL_OTA_SKIP_TEST_CAL_FAIL;
	}

	OTA_FUNC_EXIT

	return BCME_OK;
}
/* Test init sequence */
static int8
wlc_ota_test_init_seq(wlc_info_t *wlc)
{
	int8 skip_test_reason;
	int isup;
	char cntry[WLC_CNTRY_BUF_SZ] = "ALL";

	OTA_FUNC_ENTER

	/* wl down */
	wl_down(wlc->wl);

	/* wl country ALL */
	wlc_iovar_op(wlc, "country", NULL, 0, cntry, sizeof(cntry), IOV_SET, NULL);

	/* wl mpc 0 */
	wlc_iovar_setint(wlc, "mpc", 0);

	/* wl obss_coex 0 */
	wlc_iovar_setint(wlc, "obss_coex", 0);

	/* stbc_tx 0 */
	wlc_iovar_setint(wlc, "stbc_tx", 0);

	/* stbc_rx 0 */
	wlc_iovar_setint(wlc, "stbc_rx", 0);

	/* txbf 0 */
	wlc_iovar_setint(wlc, "txbf", 0);

	/* wl band auto */
	wlc_set(wlc, WLC_SET_BAND, WLC_BAND_AUTO);

	/* txpwr1 -1 */

	/* ibss_gmode -1 */
	/* not supported yet */

	/* wl mimo_bw_cap 1 */
	wlc_iovar_setint(wlc, "mimo_bw_cap", 1);

	/* wl phy_watchdog 0 */
	wlc_iovar_setint(wlc, "phy_watchdog", 0);

	/* wl interference 0 */
	wlc_set(wlc, WLC_SET_INTERFERENCE_MODE, INTERFERE_NONE);

	/* wl tempsense_disable 1 */
	wlc_iovar_setint(wlc, "tempsense_disable", 1);

	/* wl vht_features 3 */
	wlc_iovar_setint(wlc, "vht_features", 3);

	/* wl ampdu 0 */
	wlc_iovar_setint(wlc, "ampdu", 0);

	/* wl scansuppress 1 */
	wlc_set(wlc, WLC_SET_SCANSUPPRESS, 1);

	/* wl up */
	wl_up(wlc->wl);

	/* Override txpwrctrl Init base Index
	 * And issue a wl up/down sequence so that it takes
	 * for the init channel
	 */
	wlc_iovar_setint(wlc, "phy_txpwr_ovrinitbaseidx", 1);

	/* Enable quarter dB RSSI reporting resolution */
	wlc_iovar_setint(wlc, "rssi_qdB_en", 1);

	/* wl down */
	wl_down(wlc->wl);

	wl_indicate_maccore_state(wlc->wl, LTR_ACTIVE);

	/* wl up */
	wl_up(wlc->wl);

	/* wl PM 0 */
	wlc_set(wlc, WLC_SET_PM, 0);

	/* wl phy_scraminit 127 */
	wlc_iovar_setint(wlc, "phy_scraminit", 0x7f);

	/* wl rtsthresh 3840 */
	wlc_iovar_setint(wlc, "rtsthresh", 3840);

	/* wl glacial_timer 15000 */
	wlc_iovar_setint(wlc, "glacial_timer", 15000);

	/* fast_timer 15000 */
	wlc_iovar_setint(wlc, "fast_timer", 15000);

	/* slow_timer 15000 */
	wlc_iovar_setint(wlc, "slow_timer", 15000);

	/* wl plcphdr long */
	wlc_set(wlc, WLC_SET_PLCPHDR, WLC_PLCP_LONG);

	/* should check the number of cores available first */

	/* txchain 3 (must be 3 before phy_forcecal) */
	/* wlc_iovar_setint(wlc, "txchain", 3); */

	/* rxchain 3 (must be 3 before phy_forcecal) */
	/* wlc_iovar_setint(wlc, "rxchain", 3); */

	/* wl phy_forcecal 1 */
	if ((skip_test_reason = wlc_ota_test_force_phy_cal(wlc)) != BCME_OK)
		return skip_test_reason;

	/* wl phy_percal 0 */
	wlc_iovar_setint(wlc, "phy_percal", 0);

	/* wl isup */
	if (!wlc_get(wlc, WLC_GET_UP, &isup)) {
		if (!isup) {
			skip_test_reason = WL_OTA_SKIP_TEST_WL_NOT_UP;
			return skip_test_reason;
		}
	}

	OTA_FUNC_EXIT

	return BCME_OK;
}
/* Wait for sync packet */
static int
wlc_ota_test_wait_for_sync_pkt(wlc_info_t *wlc)
{

	uint8 dst_addr[6];
	uint8 src_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint32 pktengrxducast_start;

	wl_pkteng_t pkteng;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;

	OTA_FUNC_ENTER

	/* set the flags for RSSI averaging */
	ota_set_rssi_average(wlc, wlc->ota_info->sync_pktcnt, TRUE);

	/* Ppulate pkteng structure */
	bcopy(&(test_vctr->sync_mac), dst_addr, sizeof(dst_addr));
	pkteng.flags = OTA_DEFAULT_PKTENG_MODE | WL_PKTENG_PER_RX_WITH_ACK_START;
	pkteng.delay = PKTENG_DURATION;	/* Wait for 2 second in sync loop */
	pkteng.nframes = wlc->ota_info->sync_pktcnt;
	pkteng.length = 0x0;
	pkteng.seqno = 0;
	bcopy(dst_addr, pkteng.dest.octet, sizeof(dst_addr));
	bcopy(src_addr, pkteng.src.octet, sizeof(src_addr));

	/* get counter value before start of pkt engine */
	wlc_ctrupd(wlc, MCSTOFF_RXGOODUCAST);
	pktengrxducast_start = MCSTVAR(wlc->pub, pktengrxducast);

	/* Trigger pkteng */
	wlc_iovar_op(wlc, "pkteng", NULL, 0, &pkteng, sizeof(wl_pkteng_t), IOV_SET, NULL);

	OTA_FUNC_EXIT
	return pktengrxducast_start;
}
/* Set the channel */
static int
wlc_ota_test_set_chan(wlc_info_t *wlc, wl_ota_test_args_t *test_arg)
{
	uint8 band;
	uint32 chanspec = 0;
	uint8 skip_test_reason;
	int32 int_val;

	OTA_FUNC_ENTER

	/* Populate channel number */
	chanspec = chanspec | test_arg->chan;

	/* Populate the band */
	band = (test_arg->chan <= CH_MAX_2G_CHANNEL) ? WLC_BAND_2G : WLC_BAND_5G;

	if (band == WLC_BAND_2G)
		chanspec = chanspec | WL_CHANSPEC_BAND_2G;
	else
		chanspec = chanspec | WL_CHANSPEC_BAND_5G;

	/* Populate Bandwidth */
	if (test_arg->bw == WL_OTA_TEST_BW_20MHZ)
		chanspec = chanspec | WL_CHANSPEC_BW_20;
	else if (test_arg->bw == WL_OTA_TEST_BW_40MHZ)
		chanspec = chanspec | WL_CHANSPEC_BW_40;
	else
		chanspec = chanspec | WL_CHANSPEC_BW_80;

	/* Populate sideband */
	if (test_arg->bw == WL_OTA_TEST_BW_20MHZ)
		chanspec = chanspec | WL_CHANSPEC_CTL_SB_NONE;
	else if (test_arg->control_band == 'l')
		chanspec = chanspec | WL_CHANSPEC_CTL_SB_LOWER;
	else if (test_arg->control_band == 'u')
		chanspec = chanspec | WL_CHANSPEC_CTL_SB_UPPER;

	/* wl band b/a */
	wlc_set(wlc, WLC_SET_BAND, band);

	/* invoke wl chanspec iovar */
	wlc_iovar_op(wlc, "chanspec", NULL, 0, &chanspec, sizeof(uint32), IOV_SET, NULL);

	if (test_arg->bw == WL_OTA_TEST_BW_40MHZ) {
		int_val = PHY_TXC1_BW_40MHZ;
	} else {
		int_val = -1;
	}

	/* wl mimo txbw */
	wlc_iovar_op(wlc, "mimo_txbw", NULL, 0, &int_val, sizeof(int_val), IOV_SET, NULL);

	/* Do a force cal */
	if ((skip_test_reason = wlc_ota_test_force_phy_cal(wlc)) != BCME_OK) {
		return skip_test_reason;
	}

	/* Set tx and rx ant */
	/* TODO: Use test_arg->txant, and test_arg->rxant */

	OTA_FUNC_EXIT

	return BCME_OK;
}

/* Packet engine Rx */
static void
wlc_ota_start_rx_test(wlc_info_t *wlc, wl_ota_test_args_t *test_arg)
{
	OTA_FUNC_ENTER

	wlc_ota_store_rxd_sync_rssi(wlc, 0, FALSE); /* reset */

	/* set the flags for RSSI averaging */
	ota_set_rssi_average(wlc, test_arg->pkteng.nframes, TRUE);

	/* set antenna policy for Rx test */
	wlc_ota_test_setup_antenna(wlc, test_arg);

	/* invoke pkteng iovar */
	wlc_ota_test_start_pkteng(wlc, WL_PKTENG_PER_RX_WITH_ACK_START, test_arg, 1, FALSE);

	if (wlc->ota_info->sync_mode == OTA_SYNC_BLOCKING_MODE) {
		/* Store RSSI Value */
		wlc_ota_store_rx_rssiEx(wlc);

		/* clear the flags for RSSI averaging */
		ota_set_rssi_average(wlc, 0, FALSE);

		wlc_ota_move_to_next_test(wlc);
	} else {
		wl_add_timer(wlc->wl, wlc->ota_info->async_mode_timer,
			OTA_ASYNC_TIMER_INTERVAL, TRUE);
	}

	OTA_FUNC_EXIT
}
/* Tx Analysis */
static void
wlc_schedule_ota_tx_test(void *arg)
{

	wlc_info_t *wlc = (wlc_info_t *)arg;
	wl_ota_test_args_t *test_arg;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;

	uint8 i;
	int8 j;
	int32 txidx[2];

	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on_all(wlc, TRUE);
	}

	OTA_FUNC_ENTER

	test_arg = &(test_vctr->test_arg[wlc->ota_info->test_phase]);

	/* save the current rate id */
	i = wlc->ota_info->tx_test_phase;

	/* stop any packetengine running */
	wlc_ota_test_start_pkteng(wlc, WL_PKTENG_PER_TX_STOP, test_arg, 0, FALSE);

	/* Set rate */
	wlc_ota_test_set_rate(wlc, test_arg, i);

	/* set antenna policy for Tx test */
	wlc_ota_test_setup_antenna(wlc, test_arg);

	/* Loop through the target power/index */
	for (j = test_arg->pwr_info.start_pwr; j <= test_arg->pwr_info.end_pwr;
		j = j + test_arg->pwr_info.delta_pwr) {

		/* stop any packetengine running */
		wlc_ota_test_start_pkteng(wlc, WL_PKTENG_PER_TX_STOP, test_arg, 0, FALSE);

		if (test_arg->pwr_info.pwr_ctrl_on == 1) {
			/* Sweep target power */
			wlc_iovar_setint(wlc, "phy_txpwrctrl", 1);
			wlc_ota_test_set_txpwr(wlc, j);
		} else if (test_arg->pwr_info.pwr_ctrl_on == 0) {
			/* Sweep target index */
			txidx[0] = j;
			txidx[1] = j;
			wlc_iovar_setint(wlc, "phy_txpwrctrl", 0);
			/* tx idx */
			wlc_iovar_op(wlc, "phy_txpwrindex", NULL, 0, txidx,
				sizeof(txidx), IOV_SET, NULL);
		} else if (test_arg->pwr_info.pwr_ctrl_on == -1) {
			/* default power */
			wlc_iovar_setint(wlc, "phy_txpwrctrl", 1);
			wlc_ota_test_set_txpwr(wlc, -1);
		}

		/* packetengine tx */
		wlc_ota_test_start_pkteng(wlc, WL_PKTENG_PER_TX_START, test_arg, 1, TRUE);

		if (test_arg->pwr_info.pwr_ctrl_on == -1)
			break;
	}
	/* For Synchronous Call */
	if (wlc->ota_info->sync_mode == OTA_SYNC_BLOCKING_MODE) {
		wlc_ota_move_to_next_test(wlc);
	} else {
		/* For Synchronous Non Blocking Call */
		wl_add_timer(wlc->wl, wlc->ota_info->async_mode_timer,
			OTA_ASYNC_TIMER_INTERVAL, TRUE);
	}

	OTA_FUNC_EXIT
}
static void wlc_schedule_ota_test(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	int16 cnt = 0;
	int8 skip_test_reason = 0;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;

	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on_all(wlc, TRUE);
	}

	OTA_FUNC_ENTER

	/* get the default devinfo if has not been initialized */
	wlc_ota_test_get_devinfo(wlc, &wlc->ota_info->default_devinfo);

	/* Initialize sync counters */
	wlc->ota_info->ota_sync_status = WL_OTA_SYNC_IDLE;
	wlc->ota_info->ota_sync_wait_cnt = 0;

	/* Set the timer call back interval to 1ms */
	wlc_bmac_pkteng_poll_interval_set(wlc->hw, OTA_ASYNC_TIMER_INTERVAL);

	if (wlc->ota_info->test_phase == -1) {
		/* We shouldnt be here */
		skip_test_reason = WL_OTA_SKIP_TEST_UNKNOWN_CALL;
		goto skip_test;
	}

	/* Check if download has happened. Exit if not */
	if (test_vctr->file_dwnld_valid == FALSE) {
		skip_test_reason = WL_OTA_SKIP_TEST_FILE_DWNLD_FAIL;
		goto skip_test;
	}

	if (wlc->ota_info->test_phase == 0) {
		/* Do the init only once */
		if ((skip_test_reason = wlc_ota_test_init_seq(wlc)) != BCME_OK) {
			WL_ERROR(("Init seq failed. \n"));
			goto skip_test;
		}
	}

	/* Current test phase */
	cnt = wlc->ota_info->test_phase;

	if (test_vctr->test_cnt == 0) {
		skip_test_reason = WL_OTA_SKIP_TEST_NO_TEST_FOUND;
		goto skip_test;
	}

	/* Set the channel */
	if ((skip_test_reason = wlc_ota_test_set_chan(wlc,
		&(test_vctr->test_arg[cnt]))) != BCME_OK) {
		goto skip_test;
	}

	if (test_vctr->test_arg[cnt].wait_for_sync) {
		/* Schedule a ota sync and return */
		wl_add_timer(wlc->wl, wlc->ota_info->sync_timer, OTA_TEST_SCHEDULE_DELAY, 0);
		goto exit;
	}

	/* Main test engine */
	/* Right now only two test cases either TX or RX */
	/* schedule a test and return */
	wlc_ota_test_arbitrate(wlc);
	goto exit;

skip_test:
	if (skip_test_reason != 0) {
		/* reset all test flags */
		wlc_ota_test_engine_reset(wlc);
		/* wl down */
		wl_down(wlc->wl);
		WL_ERROR(("Test skipped due to reason %d \n", skip_test_reason));
	}

	/* Save the reason for skipping the test */
	wlc->ota_info->test_stage = WL_OTA_TEST_FAIL;
	wlc->ota_info->test_skip_reason = skip_test_reason;

exit:
	OTA_FUNC_EXIT

	return;
}
static void
wlc_ota_test_arbitrate(wlc_info_t * wlc)
{
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;
	uint8 cnt;

	OTA_FUNC_ENTER

	/* Current test phase */
	cnt = wlc->ota_info->test_phase;

	switch (test_vctr->test_arg[cnt].cur_test) {
		case WL_OTA_TEST_TX:
			OTA_TRACE(("%s %s wl%d: add tx_test_timer, %d-ms delay for OTA_TX\n",
				OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
				OTA_TEST_SCHEDULE_DELAY));

			/* scheduling tx test case */
			wl_add_timer(wlc->wl, wlc->ota_info->tx_test_timer,
				OTA_TEST_SCHEDULE_DELAY, 0);
			break;
		case WL_OTA_TEST_RX:
			OTA_TRACE(("%s %s wl%d: OTA_RX\n",
				OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit));

			/* start rx test case */
			wlc_ota_start_rx_test(wlc, &(test_vctr->test_arg[cnt]));
			break;
	}

	OTA_FUNC_EXIT
}
static void
wlc_ota_test_wait_for_sync(void *arg)
{

	wlc_info_t *wlc = (wlc_info_t *)arg;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;
	uint8 cnt, streams = 1;
	uint32 coremask[2] = {0, 0};
	uint8 mcs_mask[4] = {0, 0, 0, 0}; /* pre-initialize # of streams {core:4 | stream:4} */
	uint8 cores = 0;
	uint8 max_no_cores = PHY_CORE_MAX;

	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on_all(wlc, TRUE);
	}

	OTA_FUNC_ENTER

	OTA_TRACE(("%s enter %s: max_no_cores=%d\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
		max_no_cores));

	/* Current test phase */
	cnt = wlc->ota_info->test_phase;
	if (max_no_cores >= 2) {
	    cores = WL_OTA_TEST_GET_CORE(test_vctr->test_arg[cnt].txant);
	    coremask[1] |= cores;
	    coremask[1] |= cores << 8;
	    mcs_mask[streams-1] = ((cores & 0xf) << 4) | (streams & 0xf);
	    coremask[0] |= mcs_mask[0] << 0;
	    coremask[0] |= mcs_mask[1] << 8;
	    coremask[0] |= mcs_mask[2] << 16;
	    coremask[0] |= mcs_mask[3] << 24;
	    wlc_iovar_op(wlc, "txcore", NULL, 0, coremask, sizeof(uint32)*2, IOV_SET, NULL);
	    OTA_TRACE(("%s %s, test_arg->txant=0x%02x, cores=0x%x\n",
	        OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
	        test_vctr->test_arg[cnt].txant, cores));
	}

	/*
	 * The tester may be configured to test multiple OTA on different ports.
	 * For tester to identify the DUT, the sync-ACK must be sent thru the same
	 * antenna used by the following ota_tx / ota_rx commands.
	 */
	OTA_TRACE(("%s %s: about to set swdiv right before wait_for_sync\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__));
	wlc_ota_test_setup_antenna(wlc, &(test_vctr->test_arg[cnt]));

	wlc->ota_info->ota_sync_status = WL_OTA_SYNC_ACTIVE;
	wlc->ota_info->sync_rx_start_cnt = wlc_ota_test_wait_for_sync_pkt(wlc);
	wl_add_timer(wlc->wl, wlc->ota_info->async_mode_timer, OTA_ASYNC_TIMER_INTERVAL, TRUE);

	OTA_FUNC_EXIT

}

static void
wlc_ota_async_mode_cb(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	int8 skip_test_reason;
	int32 pktengrxducast_start, pktengrxducast_end;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;
	pktengrxducast_start = wlc->ota_info->sync_rx_start_cnt;
	uint8 sync_max_loops = (test_vctr->sync_timeout * 1000 + PKTENG_DURATION/2)
		/PKTENG_DURATION;
	bool sync_fail_enable = FALSE;
	int32 pkteng_status;
	uint8 cnt;
	int32	delta_rxducast;

	/* Current test phase */
	cnt = wlc->ota_info->test_phase;

	/* Query the Pkteng Status */
	wlc_iovar_op(wlc, "pkteng_status", NULL, 0, &pkteng_status,
		sizeof(int), IOV_GET, NULL);

	if (pkteng_status != BCME_OK) {
		goto exit;
	}

	/* Delete the Async Timer */
	wl_del_timer(wlc->wl, wlc->ota_info->async_mode_timer);

	switch (test_vctr->test_arg[cnt].cur_test) {
		case WL_OTA_TEST_TX:
			break;
		case WL_OTA_TEST_RX:
			/* Store RSSI Value */
			if (wlc->ota_info->ota_sync_status != WL_OTA_SYNC_ACTIVE) {
				wlc_ota_store_rx_rssiEx(wlc);
			}
			break;
	}

	if (wlc->ota_info->ota_sync_status == WL_OTA_SYNC_ACTIVE) {
		/* get counter update after sync packet */
		wlc_ctrupd(wlc, MCSTOFF_RXGOODUCAST);
		pktengrxducast_end = MCSTVAR(wlc->pub, pktengrxducast);

		/* return if failed to recieve the sync packet */
		if ((delta_rxducast = pktengrxducast_end - pktengrxducast_start) < 1) {
			skip_test_reason = (WL_OTA_SKIP_TEST_SYNCH_FAIL);
		} else {
			skip_test_reason = BCME_OK;
			wlc_ota_store_rxd_sync_rssi(wlc, delta_rxducast, TRUE);

			OTA_TRACE(("%s %s wl%d: delta_rxducast=%d, req_sync_pktcnt=%d\n",
				OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
				delta_rxducast, wlc->ota_info->sync_pktcnt));
		}

		/* clear the flags for RSSI averaging */
		ota_set_rssi_average(wlc, 0, FALSE);

		if (skip_test_reason != BCME_OK) {
			OTA_TRACE(("%s %s wl%d: OTA_SYNC Fail, ducast(start=%d,end=%d) "
				"sync_fail(action=%d,wait_cnt=%d,maxloop=%d)\n",
				OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
				pktengrxducast_start, pktengrxducast_end,
				test_vctr->sync_fail_action, wlc->ota_info->ota_sync_wait_cnt,
				sync_max_loops));

			if (test_vctr->sync_fail_action == OTA_SYNC_RETRY_FAIL) {
				/* sync retry */
				if (wlc->ota_info->ota_sync_wait_cnt < sync_max_loops) {
					wlc->ota_info->ota_sync_wait_cnt++;
					wl_add_timer(wlc->wl, wlc->ota_info->sync_timer,
						OTA_SYNC_RETRY_DELAY, 0);
				} else {
					sync_fail_enable = TRUE;
					goto sync_fail;
				}
			} else if (test_vctr->sync_fail_action == OTA_SYNC_FAIL) {
				/* skip test */
				sync_fail_enable = TRUE;
				goto sync_fail;
			} else if (test_vctr->sync_fail_action == OTA_SYNC_PASS) {
				sync_fail_enable = FALSE;
				wlc_ota_test_arbitrate(wlc);
			} else if (test_vctr->sync_fail_action == OTA_SYNC_RETRY_PASS) {
				/* sync retry */
				if (wlc->ota_info->ota_sync_wait_cnt <
					sync_max_loops)	{
					wlc->ota_info->ota_sync_wait_cnt++;
					wl_add_timer(wlc->wl,
					wlc->ota_info->sync_timer, OTA_SYNC_RETRY_DELAY, 0);
				} else {
					wlc_ota_test_arbitrate(wlc);
				}
			}
		} else {
			OTA_TRACE(("%s %s wl%d: OTA_SYNC PASSED pktengucast=%d(start=%d, end=%d)\n",
				OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
				MCSTVAR(wlc->pub, pktengrxducast),
				pktengrxducast_start, pktengrxducast_end));

			wlc_ota_test_arbitrate(wlc);
		}
sync_fail:

		if (sync_fail_enable) {
#ifdef WLRSDB
			if (RSDB_ENAB(wlc->pub)) {
				int idx;
				wlc_info_t *wlc_iter;
				/* Stop Test on both cores */
				FOREACH_WLC(wlc->cmn, idx, wlc_iter) {
					/* Check if the file is downloaded on current cores */
					if (wlc_iter->ota_info->test_vctr->file_dwnld_valid) {
						wlc_ota_sync_fail_handle(wlc_iter,
							skip_test_reason);
					}
				}
			}
#else
			wlc_ota_sync_fail_handle(wlc, skip_test_reason);

#endif /* WLRSDB */

		} else {
			wlc->ota_info->ota_sync_status = WL_OTA_SYNC_IDLE;
		}
	} else {
		OTA_TRACE(("%s %s wl%d: OTA_TEST (%s) END test_phase=%d ucast_count=%d\n",
			OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
			test_vctr->test_arg[cnt].cur_test == WL_OTA_TEST_TX ? "tx" : "rx",
			cnt,
			MCSTVAR(wlc->pub, pktengrxducast)));

		wlc_ota_move_to_next_test(wlc);
	}

exit:
	return;
}

static void
wlc_ota_sync_fail_handle(wlc_info_t *wlc, int8 skip_test_reason)
{
	/* reset all test flags */
	wlc_ota_test_engine_reset(wlc);
	/* wl down */
	wl_down(wlc->wl);

	WL_ERROR(("Test skipped due to reason %d \n", skip_test_reason));
	wlc->ota_info->ota_sync_status = WL_OTA_SYNC_FAIL;
	wlc->ota_info->test_stage = WL_OTA_TEST_FAIL;
	wlc->ota_info->test_skip_reason = skip_test_reason;
}

static void
wlc_ota_move_to_next_test(wlc_info_t *wlc)
{
	wl_ota_test_args_t *test_arg;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;
	int cnt = wlc->ota_info->test_phase;
	int test_mode, tx_ratecnt;
	test_arg = &(test_vctr->test_arg[cnt]);
	test_mode = test_vctr->test_arg[cnt].cur_test;
	tx_ratecnt = NULL;

	if (SRPWR_ENAB()) {
		wlc_srpwr_request_on_all(wlc, TRUE);
	}

	OTA_FUNC_ENTER

	switch (test_mode) {
		case WL_OTA_TEST_TX:
			wlc->ota_info->tx_test_phase++;
			tx_ratecnt = (wlc->ota_info->tx_test_phase ==
			test_arg->rt_info.rate_cnt);
			break;

		case WL_OTA_TEST_RX:
			wlc->ota_info->test_vctr->test_rxcnt++;
			tx_ratecnt = 1;	/* True in case of RX */

			/* clear the flags for RSSI averaging */
			ota_set_rssi_average(wlc, 0, FALSE);
			break;
	}

	if (tx_ratecnt) {
		/* increment test phase id */
		wlc->ota_info->test_phase++;
		wlc->ota_info->tx_test_phase = 0;
		if (wlc->ota_info->test_phase == test_vctr->test_cnt) {
			/* Exit out of test gracefully */
				wlc_ota_test_exit_engine(wlc);
		} else {
			/* Call back for next test phase */
			wl_add_timer(wlc->wl, wlc->ota_info->test_timer,
			OTA_TEST_SCHEDULE_DELAY, 0);
		}
	} else {
		/* call back for next rate phase id */
		wl_add_timer(wlc->wl, wlc->ota_info->tx_test_timer, OTA_TEST_SCHEDULE_DELAY, 0);
	}

	OTA_FUNC_EXIT
}

static void
wlc_ota_test_exit_engine(wlc_info_t *wlc)
{
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;

	OTA_FUNC_ENTER

	/* Increment loop cntr: for litepoint regression */
	wlc->ota_info->test_loop_cnt++;

	/* Reset test states */
	wlc->ota_info->test_phase = 0;
	wlc->ota_info->test_stage = WL_OTA_TEST_SUCCESS;

	/* wl down */
	wlc_statsupd(wlc);
	wl_down(wlc->wl);

		/* Sequential Trigger on other core after the test on current core completes */
#ifdef WLRSDB
		if (wlc->ota_info->ota_trigger_val == OTA_TRIGGER_ON_BOTH_CORES_SEQUENTIALLY &&
			RSDB_ENAB(wlc->pub)) {
			wlc_info_t *other_wlc = wlc_rsdb_get_other_wlc(wlc);
			wlc_ota_initiate_trigger(other_wlc);

		}
#endif /* WLRSDB */

	/* Resetting the status of ota_trigger to avoid continuous triggering */
	wlc->ota_info->ota_trigger_val = OTA_TRIGGER_RESET;

	/* Debug feature */
	/* If loop test enabled, regress the test infinite times */
	if ((test_vctr->loop_test == -1) || (wlc->ota_info->test_loop_cnt < test_vctr->loop_test))
		wlc_ota_trigger_test_engine(wlc);
	else
		wlc->ota_info->test_loop_cnt = 0;

	wlc_bmac_pkteng_poll_interval_reset(wlc->hw);

	OTA_FUNC_EXIT
}

/*
 * get current device info and set the caller-provided 'devinfo' buffer
 */
static void
wlc_ota_test_get_devinfo(wlc_info_t *wlc, ota_device_info_t *devinfo)
{
	if (devinfo->bInit)
		return;	/* init already */

	OTA_FUNC_ENTER

#ifndef WLC_SW_DIVERSITY
	devinfo->b_swdiv_rx_supported = FALSE;
	devinfo->swdiv_rx_policy = 0;
#else
	/* default swdiv_tx/rx_policy:
	 * use 'auto selection' (core-0 only) for 2.4G/5G
	 */
	uint32 default_policy = (uint32) SWDIV_POLICY_AUTO;
	default_policy |= (default_policy << SWDIV_TX_RX_CORE0_5G_SHIFT);

	/* check if SW diversity is supported and
	 * get the current rx/tx antenna policy for SW diversity
	 */
	int err;
	err = wlc_iovar_getint(wlc, "swdiv_rx_policy", (int *) &devinfo->swdiv_rx_policy);
	if (err != BCME_OK) {
		WL_ERROR(("Fail to get swdiv_rx_policy, err=%d\n", err));
		devinfo->swdiv_rx_policy = default_policy;
	}
	devinfo->b_swdiv_rx_supported = (err == BCME_OK) ? TRUE : FALSE;

#ifdef OTA_TEST_DEBUG
	/* check if swdiv_tx_policy IOVAR is supported */
	uint32 swdiv_tx_policy;
	bool b_swdiv_tx_supported = FALSE;
	err = wlc_iovar_getint(wlc, "swdiv_tx_policy", (int *) &swdiv_tx_policy);
	if (err == BCME_OK)
		b_swdiv_tx_supported = TRUE;

	OTA_TRACE(("%s %s: swdiv_supported=(tx=%d,rx=%d), swdiv_rx_policy=(0x%08x)\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
		b_swdiv_tx_supported, devinfo->b_swdiv_rx_supported,
		devinfo->swdiv_rx_policy));
#endif /* OTA_TEST_DEBUG */

#endif /* WLC_SW_DIVERSITY */

	devinfo->bInit = TRUE; /* initialized */

	OTA_FUNC_EXIT
}

/*
 * check if the ant_mask of txant/rxant-parameter is valid or not
 * ant_mask: the ant_mask field of txant/rxant-parameter,
 *           caller may retrieve the ant_mask out of rxant/txant-parameter
 *           based on the ota-cmd-id from a specific test-vector using
 *           WL_OTA_TEST_GET_ANT(test_arg->rxant/txant)
 * if SW-diversity is supported:
 *     valid ant_mask : 0, 0x01 (force antenna for Ant0), 0x2 (force antenna for Ant1)
 * if SW-diversity is not supported, ant_mask should be set to 0.
 */
static int
wlc_ota_test_is_ant_valid(wlc_info_t *wlc, uint8 ant_mask)
{
	int	err = BCME_OK;

	OTA_FUNC_ENTER

#ifndef WLC_SW_DIVERSITY
	/* if SW diversity is not supported, only allow default ant_mask */
	if ((ant_mask & WL_OTA_TEST_ANT_MASK) != 0) {
		WL_ERROR(("Invalid ant_mask=0x%02x, SW diversity is not supported\n",
			ant_mask));
		err = BCME_UNSUPPORTED;
	}
#else
	/* SW diversity is supported, make sure the field is valid */
	if (ant_mask != 0 && ant_mask != WL_OTA_TEST_FORCE_ANT0 &&
		ant_mask != WL_OTA_TEST_FORCE_ANT1) {
		WL_ERROR(("Invalid ant_mask=0x%02x\n", ant_mask));
		err = BCME_BADARG;
	}
#endif /* WLC_SW_DIVERSITY */

	OTA_TRACE(("%s exit %s: ant_mask=0x%02x, err=%d\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
		ant_mask, err));

	OTA_FUNC_EXIT

	return err;
}

/* Setup/Select antenna for ota_tx/rx tests */
static void
wlc_ota_test_setup_antenna(wlc_info_t *wlc, wl_ota_test_args_t *test_arg)
{
	uint8	ant_mask;
	bool	bTxCmd;

	OTA_FUNC_ENTER

	/* for TX: get the ant_mask field from the txant-param,
	 * for RX, get the ant_mask field from rxant-param
	 */
	bTxCmd = (test_arg->cur_test == WL_OTA_TEST_TX) ? TRUE : FALSE;
	ant_mask = WL_OTA_TEST_GET_ANT(bTxCmd ? test_arg->txant : test_arg->rxant);

	/* validate the ant_mask */
	if (wlc_ota_test_is_ant_valid(wlc, ant_mask) != BCME_OK)
		goto exit;

#ifdef WLC_SW_DIVERSITY
	/* set swdiv_rx policy if SW-diversity is supported */
	if (wlc->ota_info->default_devinfo.b_swdiv_rx_supported) {
		wlc_ota_test_force_antenna(wlc, ant_mask, bTxCmd);
	}
#endif /* WLC_SW_DIVERSITY */

exit:
	OTA_FUNC_EXIT
	return;
}

#ifdef WLC_SW_DIVERSITY
/*
 * set/force antenna by setting the SW diversity policy.
 * input:
 *   ant_mask: a valid/upper-4 bits of on an txant/rxant-parameter.
 *             caller should have validated the ant_mask via wlc_ota_test_is_ant_valid().
 *   bTxCmd: set to TRUE for ota_tx, otherwise set to FALSE.
 */
static int
wlc_ota_test_force_antenna(wlc_info_t *wlc, uint8 ant_mask, bool bTxCmd)
{
	int	err = BCME_OK;
	char	*p_swdiv_iovar = "swdiv_rx_policy";
	uint32	current_policy = 0;	/* 4 cores */
	uint32	new_policy = 0;		/* 4 cores */
	uint8	c0_policy;
#define C0_POLICY_MASK ((SWDIV_PLCY_MASK << SWDIV_TX_RX_CORE0_5G_SHIFT) \
	| SWDIV_PLCY_MASK)

#ifndef OTA_TEST_DEBUG
	UNUSED_PARAMETER(bTxCmd);
#endif /* OTA_TEST_DEBUG */

	OTA_FUNC_ENTER

	/* get the current SW diversity policy setting */
	if ((err = wlc_iovar_getint(wlc, p_swdiv_iovar, (int *) &current_policy)) != BCME_OK) {
		WL_ERROR(("Failed to get the current %s, err=%d\n", p_swdiv_iovar, err));
		goto exit;
	}

	/* figure out the new SW diversity policy setting based on
	 * the ant-mask field of txant/rxant-parameter.
	 * (core-0 only, core-1, core-2, core-3 should be preserved)
	 */
	if (ant_mask == 0) {
		/* reset C0 tx/rx policy for SW diversity to default before OTA-test starts */
		ota_device_info_t *devinfo = &wlc->ota_info->default_devinfo;
		c0_policy = (uint8) devinfo->swdiv_rx_policy;
		c0_policy &= C0_POLICY_MASK;

		OTA_TRACE(("%s %s, default swdiv_policy=0x%08x, c0_policy=0x%02x\n",
			OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
			(uint8) devinfo->swdiv_rx_policy,
			c0_policy));
	}
	else {
		/* map ant_mask -> swdiv_rx_policy */
		if (ant_mask == WL_OTA_TEST_FORCE_ANT0)
			c0_policy = SWDIV_POLICY_FORCE_0; /* force antenna to Ant 0 */
		else /* if (ant_mask == WL_OTA_TEST_FORCE_ANT1) */
			c0_policy = SWDIV_POLICY_FORCE_1; /* force antenna to Ant 1 */
		/* set the same for both 2.4G and 5G */
		c0_policy |= (c0_policy << SWDIV_TX_RX_CORE0_5G_SHIFT);

		OTA_TRACE(("%s %s, non-zero ant_mask=0x%02x, c0_policy=0x%02x\n",
			OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
			ant_mask,
			c0_policy));
	}
	/* preserve c3,c2,c1 policy */
	new_policy = (current_policy & ~C0_POLICY_MASK) | c0_policy;

	OTA_TRACE(("%s %s for %s: ant_mask=0x%02x, current_policy=0x%08x, new_policy=0x%08x\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
		bTxCmd ? "ota_tx" : "ota_rx",
		ant_mask, current_policy, new_policy));

	/* update the policy for core0 if changed */
	if (current_policy != new_policy) {
		err = wlc_iovar_setint(wlc, p_swdiv_iovar, (int) new_policy);
		if (err != BCME_OK) {
			WL_ERROR(("Failed to set %s from 0x%04x to 0x%04x, err=%d\n",
				p_swdiv_iovar,
				current_policy, new_policy, err));
		}

		OTA_TRACE(("%s %s: set %s from 0x%08x to 0x%08x, err=%d\n\n",
			OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__,
			p_swdiv_iovar,
			current_policy, new_policy, err));
	}

exit:
	OTA_FUNC_EXIT
	return err;
}
#endif /* WLC_SW_DIVERSITY */

/*
 * get RSSI from pkteng via 'pkgeng_stats' IOVAR and store the RSSI to caller-provided buffer
 * Input:
 *   rxd_pktcnt: for ota_rx, specify the num of pakcets DUT want to receive,
 *               for ota_sync, specify the num of sync-packets received.
 *   is_rxd_sync: If TRUE, this is called for ota_sync. Otherwise, it is called for ota_rx.
 */
static void
wlc_ota_get_rx_rssi(wlc_info_t *wlc, int rxd_pktcnt, bool is_rxd_sync, wl_ota_rx_rssi_t *rx_rssi)
{
	wl_pkteng_stats_t pkteng_stats;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint16	rxavgpwr;

	memset(rx_rssi, 0, sizeof(*rx_rssi));

	/* common */
	wlc_iovar_op(wlc, "pkteng_stats", NULL, 0, &pkteng_stats,
		sizeof(wl_pkteng_stats_t), IOV_GET, NULL);

	/* If none of the pkts in the first 50% are recieved then RSSI is set to -1
	*  Where as rssi 0 indicates an unused rx test slot
	*  For 'rxd-sync-pkt, no need to check the average.
	*/
	rxavgpwr = wlc_bmac_read_shm(wlc_hw, M_MFGTEST_RXAVGPWR_ANT0(wlc_hw));
	OTA_TRACE(("%s %s wl%d: wlc_bmac_read_shm(M_MFGTEST_RXAVGPWR_ANT0) return %d\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
		rxavgpwr));
	if ((rxavgpwr == 0) && !is_rxd_sync) {
		rx_rssi->rssi = (-1 << 2);
	}
	else {
		rx_rssi->rssi = (pkteng_stats.rssi << 2) + (pkteng_stats.rssi_qdb);
	}
	rx_rssi->chanspec = wlc->chanspec;
	rx_rssi->pktcnt = rxd_pktcnt;

	/* dump pkteng_status */
	OTA_TRACE(("%s %s wl%d: pkteng_stats(lostfrmcnt=%d,rssi=%d,snr=%d,rssi_qdb=%d)\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
		pkteng_stats.lostfrmcnt,
		pkteng_stats.rssi, pkteng_stats.snr, pkteng_stats.rssi_qdb));
	/*
	prhex("SJLDebugDrv rxpktcnt(16-bit/entry)",
	(void *) &pkteng_stats->rxpktcnt[0], sizeof(pkteng_stats->rxpktcnt));
	*/

	/* dupmp OTA rssi */
	OTA_TRACE(("%s %s wl%d: rx_rssi(pktcnt=%d,chanspec=0x%x,rssi=%d),corerev=%d,rxavgpwr=%d\n",
		OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
		rx_rssi->pktcnt, rx_rssi->chanspec, rx_rssi->rssi,
		wlc_hw->corerev, rxavgpwr));

}

/*
 * store RSSI for ota_rx test
 */
static void
wlc_ota_store_rx_rssiEx(wlc_info_t *wlc)
{
	uint16 rxcnt = wlc->ota_info->test_vctr->test_rxcnt;
	uint8 cnt;
	/* Current test phase */
	cnt = wlc->ota_info->test_phase;
	wl_ota_test_vector_t * test_vctr = wlc->ota_info->test_vctr;
	wl_ota_test_args_t *test_arg = &(test_vctr->test_arg[cnt]);

	/* call pkteng stats to store RSSI value */
	if (rxcnt < wlc_ota_test_rssi_maxcnt(wlc->ota_info)) {
		/* call pkteng stats to get RSSI value */
		wl_ota_rx_rssi_t *ota_rx_rssi = &wlc->ota_info->rx_rssi[rxcnt];
		wlc_ota_get_rx_rssi(wlc, test_arg->pkteng.nframes, FALSE, ota_rx_rssi);

		WL_INFORM(("wl%d: ota_rx test_rxcnt:%d; rx_rssi(pktcnt:%d,chanspec:0x%x,rssi:%d)\n",
			wlc->pub->unit, wlc->ota_info->test_vctr->test_rxcnt,
			ota_rx_rssi->pktcnt, ota_rx_rssi->chanspec, ota_rx_rssi->rssi));
	}

}

/*
 * set/store or reset RSSI for rxd-sync pkt
 * Input:
 *    bSet: if true, query and store the rssi.
 *          if false, clear the rssi for rxd-sync-pkt.
 *    rxd_pktcnt: specifed received num of pkts for the rx-sync-operation.
 */
static void
wlc_ota_store_rxd_sync_rssi(wlc_info_t *wlc, int rxd_pktcnt, bool bSet)
{
	wl_ota_rx_rssi_t *rx_rssi = &wlc->ota_info->rxd_sync_info.rssi;

	if (bSet) {
		/* call pkteng stats to store RSSI value */
		wlc_ota_get_rx_rssi(wlc, rxd_pktcnt, TRUE, rx_rssi);
		wlc->ota_info->rxd_sync_info.rssi_valid = TRUE;

		WL_INFORM(("wl%d: rxd_sync_rssi(pktcnt=%d,chanspec=0x%x,rssi=%d)\n",
			wlc->pub->unit,
			rx_rssi->pktcnt, rx_rssi->chanspec, rx_rssi->rssi));
	} else  {
		/* reset */
		memset(rx_rssi, 0, sizeof(wlc->ota_info->rxd_sync_info.rssi));
		wlc->ota_info->rxd_sync_info.rssi_valid = FALSE;

		OTA_TRACE(("%s %s wl%d: reset/clear rxd_sync_info\n",
			OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit));
	}

}

/*
 * allocate/manage a pkteng-buffer to include the caller-provided 'pkteng-header' +
 *    a rxd-sync-info/RSSI-report previously set when a sync-pkt is received,
 *    for caller to pass down to the 'pkteng' later.
 * if 'rxd_sync_info' is not available, return NULL,
 * otherwise, return a newly allocated buffer, caller should MFREE this buffer once
 * it is no longer needed.
 */
static void *
wlc_ota_alloc_rxdsync_pkteng(wlc_info_t *wlc, wl_pkteng_t *in_pkteng, int *out_plen)
{
	wl_pkteng_t *out_pkteng = NULL;
	int	pkteng_len;
	wl_ota_rx_rssi_t	*rssi;

	*out_plen = 0;			/* init */
	if (!wlc->ota_info->rxd_sync_info.rssi_valid)
		return ((void *) NULL);		/* rxd-sync not available, ignore */

	/* allocate a temp buffer for 'pkteng'-header + wl_ota_rx_rssi_t(rxd-sync-info) */
	pkteng_len = sizeof(wl_pkteng_t) + sizeof(wlc->ota_info->rxd_sync_info.rssi); /*  */
	out_pkteng = (wl_pkteng_t *) MALLOCZ(wlc->osh, pkteng_len);
	if (out_pkteng == NULL) {
		/* report error */
		return ((void *) NULL);		/* no memory */
	}

	/* setup the header */
	memcpy((void *)out_pkteng, (void *)in_pkteng, sizeof(wl_pkteng_t));

	/* append rxd_sync_info as user-data */
	rssi = (wl_ota_rx_rssi_t *) (out_pkteng + 1);
	rssi->pktcnt = htol16(wlc->ota_info->rxd_sync_info.rssi.pktcnt);
	rssi->chanspec = htol32(wlc->ota_info->rxd_sync_info.rssi.chanspec);
	rssi->rssi = htol16(wlc->ota_info->rxd_sync_info.rssi.rssi);

	*out_plen = pkteng_len;
	return (void *) out_pkteng;
}

/* set or clear the flags for RSSI averaging */
static void
ota_set_rssi_average(wlc_info_t *wlc, int pkteng_nframes, bool bSet)
{
	uint16 flags = 0;
	wlc_hw_info_t *wlc_hw = wlc->hw;

	/* set the flags for RSSI averaging */
	if (D11REV_IS(wlc_hw->corerev, 48) ||
		D11REV_IS(wlc_hw->corerev, 60) ||
		D11REV_IS(wlc_hw->corerev, 61) || D11REV_IS(wlc_hw->corerev, 62)) {

		if (bSet) {
			/* bit 15 indicate rx RSSI ON and bit 14 indicates first frame,
			*   ucode will clear bit 14 after it recieves the first frame
			*   0-14 has rx packet cnt for RSSI averaging (50% of the rx frames)
			*/
			flags = (1 << C_UOTA_RSSION_NBIT) |
				(1 << C_UOTA_RXFST_NBIT) |
				(MIN(pkteng_nframes / 2, UOTA_RSSI_MAXPKTS));
		}
		else
			flags = 0;

		OTA_TRACE(("%s %s wl%d: D11REV=%d, set shm(offset=0x%x), flags=0x%x\n",
			OTA_TRACE_UNITTEST_PREFIX, __FUNCTION__, wlc->pub->unit,
			wlc_hw->corerev, M_MFGTEST_UOTARXTEST(wlc_hw), flags));

		wlc_bmac_write_shm(wlc_hw, M_MFGTEST_UOTARXTEST(wlc_hw), flags);
	}
}

#endif /* WLOTA_EN */
