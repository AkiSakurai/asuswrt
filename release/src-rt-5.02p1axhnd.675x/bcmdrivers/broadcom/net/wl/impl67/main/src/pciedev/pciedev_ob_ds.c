/*
 * PCIEDEV OOB Deepsleep implementation
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
 * $Id: pciedev_oob_ds.c $
 */

#ifdef PCIE_DEEP_SLEEP
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmnvram.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <osl.h>
#include <hndsoc.h>
#include <hnd_cplt.h>
#include <ethernet.h>
#include <802.1d.h>
#include <sbchipc.h>
#include <pcie_core.h>
#include <dngl_api.h>
#include <pciedev.h>
#include <pciedev_dbg.h>
#include <pciedev_priv.h>
#include <pciedev_ob_ds.h>

static pciedev_ds_state_tbl_entry_t *
get_pciedev_ds_state_entry(bcmpcie_ob_deepsleep_state_t ds_state,
	bcmpcie_ob_deepsleep_event_t event);

static void pciedev_no_ds_dw_deassrt(void *handle);
static void pciedev_no_ds_perst_assrt(void *handle);
static void pciedev_ds_check_dw_assrt(void *handle);
static void pciedev_ds_check_perst_assrt(void *handle);
static void pciedev_ds_check_ds_allowed(void *handle);
static void pciedev_ds_d0_dw_assrt(void *handle);
static void pciedev_ds_d0_db_dtoh(void *handle);
static void pciedev_ds_nods_d3cold_perst_dassrt(void *handle);
static void pciedev_ds_d3cold_dw_assrt(void *handle);
static void pciedev_ds_nods_d3cold_dw_dassrt(void *handle);
static void pciedev_ds_d3cold_hw_assrt(void *handle);
static void pciedev_ds_d3cold_perst_dassrt(void *handle);
static void pciedev_deepsleep_check_periodic(struct dngl_bus *pciedev);
static void pciedev_device_wake_isr(uint32 status,  void *arg);

static pciedev_ds_state_tbl_entry_t pciedev_ob_ds_state_tbl[DS_LAST_STATE][DS_LAST_EVENT] = {
	{ /* state: NO_DS_STATE */
		{NULL, DS_INVALID_STATE}, /* event: DW_ASSRT */
		{pciedev_no_ds_dw_deassrt, DS_CHECK_STATE}, /* event: DW_DASSRT */
		{pciedev_no_ds_perst_assrt, NODS_D3COLD_STATE}, /* event: PERST_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_DASSRT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}  /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: DS_CHECK_STATE */
		{pciedev_ds_check_dw_assrt, NO_DS_STATE}, /* event: DW_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DW_DASSRT_EVENT */
		{pciedev_ds_check_perst_assrt, DS_D3COLD_STATE}, /* event: PERST_ASSRT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_DASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH_EVENT */
		{pciedev_ds_check_ds_allowed, DS_D0_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, NO_DS_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE} /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: DS_D0 */
		{pciedev_ds_d0_dw_assrt, NO_DS_STATE}, /* event: DW_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DW_DASSRT_EVENT */
		{NULL, DS_D3COLD_STATE}, /* event: PERST_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_DASSRT_EVENT */
		{pciedev_ds_d0_db_dtoh, DS_CHECK_STATE}, /* event: DB_TOH_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE} /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: NODS_D3COLD */
		{NULL, DS_INVALID_STATE}, /* event: DW_ASSRT_EVENT */
		{pciedev_ds_nods_d3cold_dw_dassrt, DS_D3COLD_STATE}, /* event: DW_DASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_ASSRT_EVENT */
		{pciedev_ds_nods_d3cold_perst_dassrt, NO_DS_STATE}, /* event: PERST_DASSRT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE} /* event: HOSTWAKE_ASSRT_EVENT */
	},
	{ /* state: DS_D3COLD */
		{pciedev_ds_d3cold_dw_assrt, NODS_D3COLD_STATE}, /* event: DW_ASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DW_DASSRT_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: PERST_ASSRT_EVENT */
		{pciedev_ds_d3cold_perst_dassrt, NO_DS_STATE}, /* event: PERST_DASSRT */
		{NULL, DS_INVALID_STATE}, /* event: DB_TOH_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_ALLOWED_EVENT */
		{NULL, DS_INVALID_STATE}, /* event: DS_NOT_ALLOWED_EVENT */
		{pciedev_ds_d3cold_hw_assrt, NODS_D3COLD_STATE} /* event: HOSTWAKE_ASSRT_EVENT */
	}
};

static pciedev_ds_state_tbl_entry_t *
BCMRAMFN(get_pciedev_ds_state_entry)(bcmpcie_ob_deepsleep_state_t ds_state,
	bcmpcie_ob_deepsleep_event_t event)
{
	if ((event >= DS_LAST_EVENT)) {
		TRAP_IN_PCIEDRV(("Invalid DS event %d\n", event));
	}
	return &pciedev_ob_ds_state_tbl[ds_state][event];
}

void
pciedev_ob_deepsleep_engine(struct dngl_bus * pciedev, bcmpcie_ob_deepsleep_event_t event)
{
	if (!pciedev->ds_oob_dw_supported) {
		PCI_ERROR(("%p:OOB DEVICE WAKE NOT SUPPORTED !!!\n", __builtin_return_address(0)));
		OSL_SYS_HALT();
	}
	if (pciedev->ds_state == DS_DISABLED_STATE || pciedev->ds_state == DS_INVALID_STATE ||
		pciedev->bus_counters->hostwake_reason == PCIE_HOSTWAKE_REASON_TRAP) {
		PCI_TRACE(("pciedev_ob_deepsleep_engine: invalid state device_wake not enabled\n"));
		return;
	}
	pciedev_ds_state_tbl_entry_t *ds_entry =
		get_pciedev_ds_state_entry(pciedev->ds_state, event);
	if (ds_entry->action_fn) {
		PCI_TRACE(("state:%s event:%s Transition:%s\n",
			pciedev_ob_ds_state_name(pciedev->ds_state),
			pciedev_ob_ds_event_name(event),
			pciedev_ob_ds_state_name(ds_entry->transition)));
		ds_entry->action_fn(pciedev);
	}
	if (ds_entry->transition != DS_INVALID_STATE) {
		if (pciedev->ds_log_count == PCIE_MAX_DS_LOG_ENTRY)
			pciedev->ds_log_count = 0;
		pciedev->ds_log[pciedev->ds_log_count].ds_state = pciedev->ds_state;
		pciedev->ds_log[pciedev->ds_log_count].ds_event = event;
		pciedev->ds_log[pciedev->ds_log_count].ds_transition = ds_entry->transition;
		pciedev->ds_log[pciedev->ds_log_count].ds_time = OSL_SYSUPTIME();
		bzero(&(pciedev->ds_log[pciedev->ds_log_count].ds_check_fail_cntrs),
			sizeof(ds_check_fail_log_t));
		pciedev->ds_log_count ++;

		pciedev->ds_state = ds_entry->transition;
	}
}

const char *
pciedev_ob_ds_state_name(bcmpcie_ob_deepsleep_state_t state)
{
	const char *ds_state_names[DS_LAST_STATE] =
		{"NO_DS_STATE", "DS_CHECK_STATE", "DS_D0_STATE,",
		"NODS_D3COLD_STATE", "DS_D3COLD_STATE"};
	if (state < 0 || state >= DS_LAST_STATE)
		return "";
	return ds_state_names[state];
}

const char *
pciedev_ob_ds_event_name(bcmpcie_ob_deepsleep_event_t event)
{
	const char *ds_ev_names[DS_LAST_EVENT] = {"DW_ASSRT_EVENT",
		"DW_DASSRT_EVENT", "PERST_ASSRT_EVENT",
		"PERST_DEASSRT_EVENT", "DB_TOH_EVENT", "DS_ALLOWED_EVENT", "DS_NOT_ALLOWED_EVENT",
		"HOSTWAKE_ASSRT_EVENT"};

	if (event >= DS_LAST_EVENT)
		return "";
	return ds_ev_names[event];
}

static void
pciedev_no_ds_dw_deassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;

	PCI_TRACE(("pciedev_no_ds_dw_deassrt\n"));
	if (pciedev->in_d3_suspend) {
		pciedev->dw_counters.dw_before_bm = TRUE;
		pciedev->dw_counters.dw_before_bm_last = OSL_SYSUPTIME();
		return;
	}
	/* First try to enter deepsleep. If cannot, start a
	* timer to retry.
	*/
	if (pciedev_can_goto_deepsleep(pciedev)) {
		pciedev_deepsleep_enter_req(pciedev);
	} else {
		/* Start a timer to check for no pending DMA to host */
		pciedev_deepsleep_check_periodic(pciedev);
	}
}

static void
pciedev_no_ds_perst_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_no_ds_perst_assrt\n"));
	/* Do not allow chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, TRUE);
}

static void
pciedev_ds_check_dw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_check_dw_assrt\n"));
	/* Disable deepsleep check timer */
	if (pciedev->ds_check_timer_on) {
		dngl_del_timer(pciedev->ds_check_timer);
		pciedev->ds_check_timer_on = FALSE;
		pciedev->ds_check_timer_max = 0;
	}
	if (!(pciedev->in_d3_suspend)) {
			pciedev_deepsleep_exit_notify(pciedev);
	}

}

static void
pciedev_ds_check_perst_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_check_perst_assrt\n"));
	/* Disable deepsleep check timer */
	if (pciedev->ds_check_timer_on) {
		dngl_del_timer(pciedev->ds_check_timer);
		pciedev->ds_check_timer_on = FALSE;
		pciedev->ds_check_timer_max = 0;
	}
	/* Allow chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, FALSE);
}

static void
pciedev_ds_check_ds_allowed(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_check_ds_allowed\n"));
	/* Allow chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, FALSE);
}

static void
pciedev_ds_d0_dw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_d0_dw_assrt\n"));
	pciedev_disable_deepsleep(pciedev, TRUE);
	if (!(pciedev->in_d3_suspend)) {
		pciedev_deepsleep_exit_notify(pciedev);
	}
}

static void
pciedev_ds_d0_db_dtoh(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_d0_db_dtoh\n"));
	if (pciedev->in_d3_suspend) {
		PCI_ERROR(("Prevent sending DS-EXIT while in D3 suspend\n"));
		return;
	}
	/* Prevent chip to go to deepsleep */
	pciedev_disable_deepsleep(pciedev, TRUE);
	pciedev_deepsleep_exit_notify(pciedev);
	pciedev_deepsleep_check_periodic(pciedev);
}

static void
pciedev_ds_nods_d3cold_perst_dassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_nods_d3cold_perst_dassrt\n"));
	pciedev_disable_deepsleep(pciedev, TRUE);
}
static void
pciedev_ds_d3cold_dw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_nods_d3cold_perst_dassrt\n"));
	pciedev_disable_deepsleep(pciedev, TRUE);
}

static void
pciedev_ds_nods_d3cold_dw_dassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_nods_d3cold_dw_dassrt\n"));
	pciedev_disable_deepsleep(pciedev, FALSE);
}

static void
pciedev_ds_d3cold_hw_assrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_nods_d3cold_perst_dassrt\n"));
	pciedev_disable_deepsleep(pciedev, TRUE);
}

static void
pciedev_ds_d3cold_perst_dassrt(void *handle)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)handle;
	PCI_TRACE(("pciedev_ds_d3cold_perst_dassrt\n"));
	pciedev_disable_deepsleep(pciedev, TRUE);
}

static void
pciedev_deepsleep_check_periodic(struct dngl_bus *pciedev)
{
	PCI_TRACE(("pciedev_deepsleep_check_periodic\n"));
	if (pciedev->in_d3_suspend)
		return;
	if (pciedev->ds_check_timer_on == FALSE) {
		pciedev->ds_check_timer_on = TRUE;
		pciedev->ds_check_timer_max = 0;
		dngl_add_timer(pciedev->ds_check_timer,
			pciedev->ds_check_interval, FALSE);
	}
}

static void
pciedev_device_wake_isr(uint32 status,  void *arg)
{
	struct dngl_bus *pciedev = (struct dngl_bus *)arg;
	uint32 cnt;

	if (!pciedev->ds_oob_dw_supported) {
		return;
	}
	/*
	* If the DW pulse is short, only one interrupt is recieved with
	* status showing both the positive and negative edge bits set.
	* So, check if that is the case and capture that information.
	*/
	if (status & (1 << GCI_GPIO_STS_POS_EDGE_BIT) &&
		status & (1 << GCI_GPIO_STS_NEG_EDGE_BIT)) {
		pciedev->dw_counters.dw_edges = TRUE;
	}
	else {
		pciedev->dw_counters.dw_edges = FALSE;
	}
	pciedev->dw_counters.dw_state = status & GCI_GPIO_STS_VALUE ? TRUE : FALSE;
	/* Record the time */
	cnt = pciedev->dw_counters.dw_toggle_cnt % PCIEDEV_IPC_MAX_DW_TOGGLE;
	pciedev->dw_counters.dw_toggle_cnt =
		pciedev->dw_counters.dw_edges ?
		pciedev->dw_counters.dw_toggle_cnt + 2 :
		pciedev->dw_counters.dw_toggle_cnt + 1;
	pciedev->dw_counters.last_dw_toggle_time[cnt] = OSL_SYSUPTIME();
	pciedev->dw_counters.last_dw_state[cnt] = pciedev->dw_counters.dw_state;
	pciedev_trigger_deepsleep_dw(pciedev);
}

void
pciedev_deepsleep_enter_req(struct dngl_bus *pciedev)
{
	PCI_TRACE(("sending deep sleep request to host\n"));
	pciedev->dw_counters.ds_req_sent_last = OSL_SYSUPTIME();
	pciedev_d2h_mbdata_send(pciedev, PCIE_IPC_D2HMB_DEV_DS_ENTER_REQ);
}

void
pciedev_deepsleep_exit_notify(struct dngl_bus *pciedev)
{
	uint32 buf = 1;

	pciedev_disable_deepsleep(pciedev, TRUE);

	pciedev_manage_TREFUP_based_on_deepsleep(pciedev, PCIEDEV_DEEPSLEEP_DISABLED);

	/* should we wait for HT to be avail */
	pciedev_d2h_mbdata_send(pciedev, PCIE_IPC_D2HMB_DEV_DS_EXIT_NOTE);
	/* Notify to WL */
	dngl_dev_ioctl(pciedev->dngl, RTEDEVDSNOTIFY, &buf, sizeof(uint32));
}

void
pciedev_handle_host_deepsleep_ack(struct dngl_bus *pciedev)
{
	uint32 buf = 0;
	PCI_TRACE(("host acked the deep sleep request, so enable deep sleep now\n"));

	pciedev_manage_TREFUP_based_on_deepsleep(pciedev, PCIEDEV_DEEPSLEEP_ENABLED);

	pciedev_d2h_mbdata_clear(pciedev, PCIE_IPC_D2HMB_DEV_DS_ENTER_REQ);

#if defined(BCMPCIE_IDMA)
	if (PCIE_IDMA_ACTIVE(pciedev) && PCIE_IDMA_DS(pciedev)) {
		pciedev_idma_channel_enable(pciedev, FALSE);
	}
#endif /* BCMPCIE_IDMA */

	pciedev_ob_deepsleep_engine(pciedev, DS_ALLOWED_EVENT);
	/* Notify to WL */
	dngl_dev_ioctl(pciedev->dngl, RTEDEVDSNOTIFY, &buf, sizeof(uint32));
}

void
pciedev_handle_host_deepsleep_nak(struct dngl_bus *pciedev)
{
	PCI_TRACE(("host acked the deep sleep request, so enable deep sleep now\n"));
	pciedev_d2h_mbdata_clear(pciedev, PCIE_IPC_D2HMB_DEV_DS_ENTER_REQ);
	pciedev_ob_deepsleep_engine(pciedev, DS_NOT_ALLOWED_EVENT);
}

/*
 * Trigger the deepsleep engine based on the device_wake state.
 * Read the current status only if gpioread is TRUE
 */
void
pciedev_trigger_deepsleep_dw(struct dngl_bus *pciedev)
{
	bool dwstate = pciedev->dw_counters.dw_state;
	bool dwedge = pciedev->dw_counters.dw_edges;

	if (!pciedev->ds_oob_dw_supported) {
		return;
	}
	if (!pciedev->common_rings_attached) {
		PCI_ERROR(("Cannot trigger ds engine. Common rings not attached yet!\n"));
		return;
	}

	if (dwedge) {
		/* Short pulse case. Apply both assert/de-assert DS logic */
		if (dwstate) {
			pciedev_ob_deepsleep_engine(pciedev, DW_DASSRT_EVENT);
		}
		else {
			pciedev_ob_deepsleep_engine(pciedev, DW_ASSRT_EVENT);
		}
	}
	if (dwstate) {
#if defined(UART_TRAP_DBG) && defined(SECI_UART)
			si_seci_clk_force(pciedev->sih, 1);
#endif /* UART_TRAP_DBG && SECI_UART */
		pciedev_ob_deepsleep_engine(pciedev, DW_ASSRT_EVENT);
	}
	else {
#if defined(UART_TRAP_DBG) && defined(SECI_UART)
			si_seci_clk_force(pciedev->sih, 0);
#endif /* UART_TRAP_DBG */
		pciedev_ob_deepsleep_engine(pciedev, DW_DASSRT_EVENT);
	}
}

/*
* After BM is enabled by the host, check if DEVICE_WAKE line is
* low and if so send DS-REQ if we could not do it earlier.
*/
void
pciedev_dw_check_after_bm(struct dngl_bus *pciedev)
{
	if (pciedev->in_d3_suspend) {
		return;
	}
	/* If hostready is enabled, no need to process DEVICE_WAKE
	* toggle that came before hostready.
	*/
	if (pciedev->hostready) {
		return;
	}

	/* Right after D3cold exit, DS state should be in DW asserted */
	pciedev_ob_deepsleep_engine(pciedev, DW_ASSRT_EVENT);

	/* If Device Wake is Low. Send DS-REQ if we couldn't send earlier */
	if (pciedev->dw_counters.dw_before_bm && !pciedev->ds_check_timer_on) {
		pciedev_trigger_deepsleep_dw(pciedev);
	}
}

int
pciedev_enable_device_wake(struct dngl_bus *pciedev)
{
	uint8 cur_status = 0;
	uint8 wake_status;
	uint8 gci_gpio = pciedev->dw_counters.dw_gpio;

	pciedev->device_wake_gpio =  CC_GCI_GPIO_INVALID;
	if (!pciedev->ds_oob_dw_supported) {
		PCI_ERROR(("OOB not allowed. Cannot enable device_wake\n"));
		return BCME_ERROR;
	}
	if ((si_enable_device_wake(pciedev->sih,
		&wake_status, &cur_status)) == CC_GCI_GPIO_INVALID) {
		PCI_ERROR(("pcie dev: device_wake not enabled\n"));
		pciedev->ds_state = DS_DISABLED_STATE;
		return BCME_ERROR;
	}

	pciedev->device_wake_gpio = gci_gpio;
	PCI_TRACE(("device_wake: gpio %d, wake_status 0x%02x, cur_status 0x%02x ds_state %d\n",
		gci_gpio, wake_status, cur_status, pciedev->ds_state));

	pciedev_disable_deepsleep(pciedev, TRUE);

	if (cur_status) {
		PCI_TRACE(("device_ wake init state 1, disable deep sleep\n"));
		pciedev->ds_state = NO_DS_STATE;
		pciedev->dw_counters.dw_state = TRUE; /* DEVICE_WAKE asserted */
	} else {
		PCI_TRACE(("device_ wake init state 0\n"));
		/* Keep deepsleep disabled till deepsleep is allowed by host */
		pciedev->dw_counters.dw_state = FALSE; /* DEVICE_WAKE de_asserted */
		if (pciedev->common_rings_attached) {
			pciedev_ob_deepsleep_engine(pciedev, DW_DASSRT_EVENT);
		} else {
			PCI_PRINT(("COMMON RING NOT ATTACHED!\n"));
		}
	}

	if (hnd_enable_gci_gpioint(gci_gpio, wake_status, pciedev_device_wake_isr,
			pciedev) == NULL) {
		PCI_ERROR(("%s: Cannot register gci device_wake handler\n", __FUNCTION__));
		return BCME_ERROR;
	}
	return BCME_OK;
}
#endif /* PCIE_DEEP_SLEEP */
