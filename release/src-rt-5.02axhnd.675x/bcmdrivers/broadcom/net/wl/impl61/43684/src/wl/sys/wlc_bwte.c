/*
* BT WLAN TUNNEL ENGINE
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
* $Id: wlc_bwte.c 773199 2019-03-14 14:36:42Z $
*
*/
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbchipc.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <802.1d.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_objregistry.h>
#include <rte_gpio.h>
#include <hnd_gci.h>
#include <sbgci.h>

#include <wlc_bwte.h>
#include <wlc_bwte_priv.h>

#undef BWTE_BTINTR_GPIO
#define BWTE_BTINTR_GCI

#if defined(BWTE_BTINTR_GPIO) && defined(BWTE_BTINTR_GCI)
#error "GPIO and GCI based interrupting cannot be enabled together"
#endif // endif
#if !defined(BWTE_BTINTR_GPIO) && !defined(BWTE_BTINTR_GCI)
#error "Either GPIO or GCI based interrupting should be enabled"
#endif // endif

enum {
	IOV_BWTE_STATS,
	IOV_BWTE_STATS_CLEAR,
	IOV_BWTE_LAST
};

/* IOVar table */
static const bcm_iovar_t bwte_iovars[] = {
#ifdef WLCNT
	{"bwte_stats", IOV_BWTE_STATS,
	(0), 0, IOVT_BUFFER, sizeof(bwte_stats_t)
	},
	{"bwte_stats_clear", IOV_BWTE_STATS_CLEAR,
	(0), 0, IOVT_INT32, 0
	},
#endif /* WLCNT */
	{NULL, 0, 0, 0, 0, 0 }
};

/* forward declarations */
static int wlc_bwte_doiovar(void *hdl, uint32 actionid,
		void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif);

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static bool bwte_is_client_id_valid(bwte_info_t *bwte_info, int client_id)
{
	return ((client_id >= 0) && (client_id < bwte_info->sb->max_clients) &&
		bwte_info->sb->csb[client_id]);
}

#ifdef BWTE_BTINTR_GPIO
static int bwte_toggle_gpio(bwte_info_t *bwte_info, bool toggle_wlan)
{
	uint32 trigger_addr, trigger_bit, trigger_type, val;
	bwte_gpio_t *gpio = NULL;

	if (!bwte_info) {
		WL_ERROR(("%s: NULL bwte_info pointer\n", __FUNCTION__));
		return -1;
	}

	if (!toggle_wlan && !bwte_info->sb->btgpio.addr) {
		WL_ERROR(("%s: no BT gpio address\n", __FUNCTION__));
		return -1;
	}

	gpio = toggle_wlan ? &bwte_info->sb->wlangpio : &bwte_info->sb->btgpio;

	trigger_addr = (uint32)(gpio->addr);
	if (!toggle_wlan) {
		trigger_addr += BT_ADDR_OFFSET;
		bwte_info->wl2bt_intrcnt++;
	}
	trigger_bit = gpio->trigger_bit;
	trigger_type = gpio->trigger_type;

	if (trigger_type ==  BWTE_GPIO_TRIGGER_EDGE_FALLING) {
		val = R_REG(bwte_info->osh, (uint32 *)trigger_addr);
		val |= 1 << trigger_bit;
		W_REG(bwte_info->osh, (uint32 *)trigger_addr, val);
		val &= ~(1 << trigger_bit);
		W_REG(bwte_info->osh, (uint32 *)trigger_addr, val);
	} else if (trigger_type ==  BWTE_GPIO_TRIGGER_EDGE_BOTH) {
		val = R_REG(bwte_info->osh, (uint32 *)trigger_addr);
		val ^= (1 << trigger_bit);
		W_REG(bwte_info->osh, (uint32 *)trigger_addr, val);
	} else {
		WL_ERROR(("dont support trigger_type(%d) now\n", trigger_type));
		return -1;
	}
	return 0;
}
#endif /* BWTE_BTINTR_GPIO */

#ifdef BWTE_BTINTR_GCI
static int bwte_gciack_tobt(bwte_info_t *bwte_info, bool val)
{
	si_t *sih;
	if (!bwte_info) {
		WL_ERROR(("%s: NULL bwte_info pointer\n", __FUNCTION__));
		return -1;
	}

	sih = bwte_info->sih;
	if (val) {
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[3]),
			GCI_MAILBOXACK_TOBT, GCI_MAILBOXACK_TOBT);
	}
	else {
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[3]),
			GCI_MAILBOXACK_TOBT, 0);
	}

	return 0;
}

static int bwte_gcidata_tobt(bwte_info_t *bwte_info, uint16 val)
{
	si_t *sih;
	if (!bwte_info) {
		WL_ERROR(("%s: NULL bwte_info pointer\n", __FUNCTION__));
		return -1;
	}

	sih = bwte_info->sih;
	if (val) {
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[2]),
			ALLONES_32, (0x0500 << 16) | 0xCAFE);
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[3]),
			GCI_MAILBOXDATA_TOBT | GCI_WAKE_TOBT,
			GCI_MAILBOXDATA_TOBT | GCI_WAKE_TOBT);
	}
	else {
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[2]),
			ALLONES_32, 0);
		si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[3]),
			GCI_MAILBOXDATA_TOBT | GCI_WAKE_TOBT, 0);
	}

	return 0;
}
#endif /* BWTE_BTINTR_GCI */

static bool bwte_process_btmsg(bwte_info_t *bwte_info, bwte_ctlchan_t *chan,
	wlc_bwte_cb ctl_func, void* arg)
{
	bool toggle_bt = FALSE;
	int ret;

	if (!bwte_info || !chan) {
		WL_ERROR(("%s: NULL pointer, bwte_info(%p), chan(%p)\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(bwte_info), OSL_OBFUSCATE_BUF(chan)));
		return toggle_bt;
	}

	if (chan->msg_inuse && chan->msg && chan->msg_len) {
		if (ctl_func) {
			ret = ctl_func(arg, (uchar *)chan->msg + BT_ADDR_OFFSET, chan->msg_len);
			if (ret) {
				return FALSE;
			}
		}
		/* set msg_inuse to 0 indicating finish using the msg */
		chan->msg_inuse = 0;
		toggle_bt = TRUE;
	}

	return toggle_bt;
}

static bool bwte_process_btdata(bwte_info_t *bwte_info, bwte_datachan_t *chan,
	wlc_bwte_cb data_func, void* arg)
{
	bool toggle_bt = FALSE;
	uint16 rd_idx;
	int ret;

	if (!bwte_info || !chan) {
		WL_ERROR(("%s: NULL pointer, bwte_info(%p), chan(%p)\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(bwte_info), OSL_OBFUSCATE_BUF(chan)));
		return toggle_bt;
	}

	rd_idx = chan->rd_idx;
	while (rd_idx != chan->wr_idx) {
		/* have data to queue */
		if (data_func) {
			ret = data_func(arg, (uchar *)chan->pkts[rd_idx].p + BT_ADDR_OFFSET,
				chan->pkts[rd_idx].len);
			if (ret) {
				break;
			}
		}
		toggle_bt = TRUE;
		rd_idx++;
		if (rd_idx == chan->nodes_cnt) {
			rd_idx = 0;
		}
	}

	/* notify bt */
	if (toggle_bt) {
		chan->rd_idx = rd_idx;
	}

	return toggle_bt;
}

static bool bwte_client_isr(bwte_info_t *bwte_info, int client_id, bool defer_bt_toggle)
{
	bool toggle_bt = FALSE;

	if (!bwte_info) {
		WL_ERROR(("%s: bwte_info is NULL\n", __FUNCTION__));
		return FALSE;
	}

	if (!bwte_is_client_id_valid(bwte_info, client_id)) {
		WL_ERROR(("%s: client_id error, client_id(%d)\n", __FUNCTION__, client_id));
		return FALSE;
	}

	wlc_bwte_reclaim_wlan_buf(bwte_info, client_id, WLC_BWTE_CTL_MSG, FALSE);
	wlc_bwte_reclaim_wlan_buf(bwte_info, client_id, WLC_BWTE_LO_DATA, FALSE);
	wlc_bwte_reclaim_wlan_buf(bwte_info, client_id, WLC_BWTE_HI_DATA, FALSE);

	if (bwte_process_btmsg(bwte_info, &bwte_info->sb->csb[client_id]->bt2wlan_ctlchan,
		bwte_info->client[client_id].ctl_func, bwte_info->client[client_id].arg)) {
		toggle_bt = TRUE;
	}

	if (bwte_process_btdata(bwte_info, &bwte_info->sb->csb[client_id]->bt2wlan_scochan,
		bwte_info->client[client_id].hi_data_func, bwte_info->client[client_id].arg)) {
		toggle_bt = TRUE;
	}

	if (bwte_process_btdata(bwte_info, &bwte_info->sb->csb[client_id]->bt2wlan_aclchan,
		bwte_info->client[client_id].lo_data_func, bwte_info->client[client_id].arg)) {
		toggle_bt = TRUE;
	}

	if (toggle_bt && !defer_bt_toggle) {
#ifdef BWTE_BTINTR_GPIO
		bwte_toggle_gpio(bwte_info, FALSE);
#endif /* BWTE_BTINTR_GPIO */
#ifdef BWTE_BTINTR_GCI
		if (bwte_info->inttobt != STATE_NONE) {
			bwte_info->inttobt_pending = TRUE;
		}
		else {
			bwte_info->inttobt = W2B_DATA_SET;
			bwte_gcidata_tobt(bwte_info, TRUE);
			/* update interrupt status */
			bwte_info->stats->wl2bt_dset_cnt++;
		}
#endif /* BWTE_BTINTR_GCI */
	}

	return toggle_bt;
}

#ifdef BWTE_BTINTR_GCI
static int bwte_gci_update_intstate(bwte_info_t *bwte_info, uint32 event,
	uint32 input, uint32 *data)
{
	bool ret = FALSE;
	si_t *sih = bwte_info->sih;

	if (event & GCI_MAILBOXDATA_TOWLAN) {
		/* BT >>> WLAN
		 * [i/p] >>> B2W_DATA_SET
		 * [o/p] <<< W2B_ACK_SET
		 * [i/p] >>> B2W_DATA_CLEAR
		 * [o/p] <<< W2B_ACK_CLEAR
		 */
		if (input & GCI_MAILBOXDATA_TOWLAN) {
			if (bwte_info->intfrombt != STATE_NONE) {
				WL_ERROR(("%s: invalid state from BT (%d:%d)\n", __FUNCTION__,
						bwte_info->inttobt, bwte_info->intfrombt));
				bwte_info->stats->state_error_1++;
				return FALSE;
			}
			bwte_info->intfrombt = B2W_DATA_SET;
			bwte_gciack_tobt(bwte_info, TRUE);
			bwte_info->intfrombt = W2B_ACK_SET;

			/* update interrupt status */
			bwte_info->stats->bt2wl_dset_cnt++;
			bwte_info->stats->wl2bt_aset_cnt++;
		}
		else {
			if (bwte_info->intfrombt != W2B_ACK_SET) {
				WL_ERROR(("%s: invalid state from BT (%d:%d)\n", __FUNCTION__,
						bwte_info->inttobt, bwte_info->intfrombt));
				bwte_info->stats->state_error_2++;
				return FALSE;
			}
			bwte_info->intfrombt = B2W_DATA_CLEAR;
			*data = si_gci_direct(sih,
				GCI_OFFSETOF(sih, gci_input[6]), 0, 0);

			bwte_gciack_tobt(bwte_info, FALSE);
			bwte_info->intfrombt = W2B_ACK_CLEAR;
			bwte_info->intfrombt = STATE_NONE;

			/* update interrupt status */
			bwte_info->stats->bt2wl_dclear_cnt++;
			bwte_info->stats->wl2bt_aclear_cnt++;

			/* received valid data return TRUE */
			ret = TRUE;
		}
	}

	if (event & GCI_MAILBOXACK_TOWLAN) {
		/* WLAN >>> BT
		 * [o/p] <<< W2B_DATA_SET
		 * [i/p] >>> B2W_ACK_SET
		 * [o/p] <<< W2B_DATA_CLEAR
		 * [i/p] >>> B2W_ACK_CLEAR
		 */
		if (input & GCI_MAILBOXACK_TOWLAN) {
			if (bwte_info->inttobt != W2B_DATA_SET) {
				WL_ERROR(("%s: invalid state to BT (%d:%d)\n", __FUNCTION__,
						bwte_info->inttobt, bwte_info->intfrombt));
				bwte_info->stats->state_error_3++;
				return FALSE;
			}
			bwte_info->inttobt = B2W_ACK_SET;
			bwte_gcidata_tobt(bwte_info, FALSE);
			bwte_info->inttobt = W2B_DATA_CLEAR;

			/* update interrupt status */
			bwte_info->stats->bt2wl_aset_cnt++;
			bwte_info->stats->wl2bt_dclear_cnt++;
		}
		else {
			if (bwte_info->inttobt != W2B_DATA_CLEAR) {
				WL_ERROR(("%s: invalid state to BT (%d:%d)\n", __FUNCTION__,
						bwte_info->inttobt, bwte_info->intfrombt));
				bwte_info->stats->state_error_4++;
				return FALSE;
			}
			bwte_info->inttobt = B2W_ACK_CLEAR;
			/* clear MailBoxdata */
			si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[2]),
				ALLONES_32, 0);
			bwte_info->inttobt = STATE_NONE;

			/* update interrupt status */
			bwte_info->stats->bt2wl_aclear_cnt++;

			if (bwte_info->inttobt_pending == TRUE) {
				bwte_info->inttobt = W2B_DATA_SET;
				bwte_gcidata_tobt(bwte_info, TRUE);
				bwte_info->inttobt_pending = FALSE;

				/* update interrupt status */
				bwte_info->stats->wl2bt_dset_cnt++;
			}
		}
	}

	return ret;
}
#endif /* BWTE_BTINTR_GCI */

#ifdef BWTE_BTINTR_GPIO
static void bwte_gpio_bt2wl_handler(uint32 stat, void *arg)
{
	bwte_info_t *bwte_info = (bwte_info_t *)arg;
	int i, processed_client_cnt;
	bool toggle_bt = FALSE;

	if (!bwte_info) {
		WL_ERROR(("%s: arg is NULL\n", __FUNCTION__));
		return;
	}

#ifdef BWTE_DEBUG_ISR
	bwte_info->wlan_isr_level = 1 - bwte_info->wlan_isr_level;
	si_gpioout(bwte_info->sih, 1 << GPIO_ISR_IN, bwte_info->wlan_isr_level << GPIO_ISR_IN,
		GPIO_DRV_PRIORITY);
#endif // endif
	bwte_info->stats->bt2wl_intrcnt++;

#ifdef BWTE_BOTH_EDGE
	/* change polarity to simulate both edge intr */
	si_gpioevent(bwte_info->sih, GPIO_REGEVT_INTPOL, 1 << bwte_info->sb->wlangpio.trigger_bit,
		R_REG(bwte_info->osh, (uint32 *)bwte_info->sb->wlangpio.addr));
#endif // endif

	processed_client_cnt = 0;
	for (i = 0; i < bwte_info->sb->max_clients; i++) {
		if (processed_client_cnt == bwte_info->client_cnt) {
			break;
		}

		if (bwte_client_isr(bwte_info, i, TRUE)) {
			toggle_bt = TRUE;
		}

		if (bwte_info->sb->csb[i]) {
			processed_client_cnt++;
		}
	}

	if (toggle_bt) {
		bwte_toggle_gpio(bwte_info, FALSE);
	}

#ifdef BWTE_DEBUG_ISR
	si_gpioout(bwte_info->sih, 1 << GPIO_ISR_OUT, bwte_info->wlan_isr_level << GPIO_ISR_OUT,
		GPIO_DRV_PRIORITY);
#endif // endif
}
#endif /* BWTE_BTINTR_GPIO */

#ifdef BWTE_BTINTR_GCI
static void bwte_gci_bt2wl_handler(uint32 event, uint32 input, void *arg)
{
	bwte_info_t *bwte_info = (bwte_info_t *)arg;
	int i, processed_client_cnt;
	bool toggle_bt = FALSE;
	uint32 data;

	if (!bwte_info) {
		WL_ERROR(("%s: arg is NULL\n", __FUNCTION__));
		return;
	}

	/* return TRUE if there is valid data */
	if (!bwte_gci_update_intstate(bwte_info, event, input, &data)) {
		return;
	}

	/* received MailBox interrupt from BT */
	bwte_info->stats->bt2wl_intrcnt++;

	processed_client_cnt = 0;
	for (i = 0; i < bwte_info->sb->max_clients; i++) {
		if (processed_client_cnt == bwte_info->client_cnt) {
			break;
		}

		if (bwte_client_isr(bwte_info, i, TRUE)) {
			toggle_bt = TRUE;
		}

		if (bwte_info->sb->csb[i]) {
			processed_client_cnt++;
		}
	}

	if (toggle_bt) {
		if (bwte_info->inttobt != STATE_NONE) {
			bwte_info->inttobt_pending = TRUE;
		}
		else {
			bwte_info->inttobt = W2B_DATA_SET;
			bwte_gcidata_tobt(bwte_info, TRUE);
			/* update interrupt status */
			bwte_info->stats->wl2bt_dset_cnt++;
		}
	}
}
#endif /* BWTE_BTINTR_GCI */

#ifdef BWTE_BTINTR_GPIO
static int
BCMATTACHFN(bwte_gpio_init)(bwte_info_t *bwte_info, si_t *sih)
{
	uint8 gpio_input = CC_GCI_GPIO_INVALID;
	uint8 fnsel = 0xff;
	rte_gpioh_t	*gi;

	if (!bwte_info || !sih) {
		WL_ERROR(("%s: NULL pointer, bwte_info(%p), sih(%p)\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(bwte_info), OSL_OBFUSCATE_BUF(sih)));
		return -1;
	}

	/* device_wake_opt (GPIO-1) is not used in 4359 */
	if (CHIPID(sih->chip) == BCM4359_CHIP_ID) {
		/* GPIO-1 default for tbow */
		gpio_input = CC_GCI_GPIO_1;
	}

	if (getvar(NULL, "bwte_gpio_bt2wl")) {
		gpio_input = (uint8)getintvar(NULL, "bwte_gpio_bt2wl");
	}

	if (gpio_input == CC_GCI_GPIO_INVALID) {
		WL_ERROR(("no bwte_gpio_bt2wl setting in nvram\n"));
		return -1;
	}

	WL_ERROR(("bwte_gpio_bt2wl = %d\n", gpio_input));

	bwte_info->sb->wlangpio.addr = (void *)si_corereg_addr(sih, SI_CC_IDX,
		OFFSETOF(chipcregs_t, gpioout));

	bwte_info->sb->wlangpio.trigger_bit = gpio_input;
#ifdef BWTE_BOTH_EDGE
	bwte_info->sb->wlangpio.trigger_type = BWTE_GPIO_TRIGGER_EDGE_BOTH;
#else
	bwte_info->sb->wlangpio.trigger_type = BWTE_GPIO_TRIGGER_EDGE_FALLING;
#endif // endif

	if (CHIPID(sih->chip) == BCM4335_CHIP_ID) {
		fnsel = CC4335_FNSEL_SAMEASPIN;
	} else if (BCM4350_CHIP(sih->chip)) {
		fnsel = CC4350_FNSEL_SAMEASPIN;
	} else {
		WL_ERROR(("%s: Unsupported chip(0x%x)\n", __FUNCTION__, CHIPID(sih->chip)));
	}

	if (fnsel != 0xff) {
		si_gci_set_functionsel(sih, gpio_input, fnsel);
#ifdef BWTE_DEBUG_ISR
		si_gci_set_functionsel(sih, GPIO_ISR_IN, fnsel);
		si_gci_set_functionsel(sih, GPIO_ISR_OUT, fnsel);
#endif // endif
	}

#ifdef BWTE_DEBUG_ISR
	bwte_info->wlan_isr_level = 1;
	/* isr in gpio */
	si_gpiocontrol(sih, (1 << GPIO_ISR_IN), 0, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, 1 << GPIO_ISR_IN, 1 << GPIO_ISR_IN, GPIO_DRV_PRIORITY);
	si_gpioout(sih, 1 << GPIO_ISR_IN, 1 << GPIO_ISR_IN, GPIO_DRV_PRIORITY);

	/* isr out gpio */
	si_gpiocontrol(sih, (1 << GPIO_ISR_OUT), 0, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, 1 << GPIO_ISR_OUT, 1 << GPIO_ISR_OUT, GPIO_DRV_PRIORITY);
	si_gpioout(sih, 1 << GPIO_ISR_OUT, 1 << GPIO_ISR_OUT, GPIO_DRV_PRIORITY);
#endif // endif

	si_gpiocontrol(sih, (1 << gpio_input), 0, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, 1 << gpio_input, 1 << gpio_input, GPIO_DRV_PRIORITY);
	/* set default level to low */
	si_gpioout(sih, 1 << gpio_input, 0 << gpio_input, GPIO_DRV_PRIORITY);

#ifdef BWTE_BOTH_EDGE
	/* set rising edge detection */
	si_gpioevent(sih, GPIO_REGEVT_INTPOL, 1 << gpio_input, 0 << gpio_input);
#else
	/* set falling edge detection */
	si_gpioevent(sih, GPIO_REGEVT_INTPOL, 1 << gpio_input, 1 << gpio_input);
#endif // endif

	si_gpioevent(sih, GPIO_REGEVT_INTMSK, 1 << gpio_input, 1 << gpio_input);
	/* Register the GPIO interrupt handler (FALSE = edge-detect) */
	gi = hnd_gpio_handler_register((1 << gpio_input), FALSE,
		bwte_gpio_bt2wl_handler, bwte_info);
	if (!gi)
		return BCME_ERROR;

	return 0;
}
#endif /* BWTE_BTINTR_GPIO */

#ifdef BWTE_BTINTR_GCI
static int
BCMATTACHFN(bwte_gci_init)(bwte_info_t *bwte_info, si_t *sih)
{
	ASSERT(bwte_info && sih);

	/* enable interrupt for MailBoxDataValidToWLAN, MailBoxAckToWLAN and WakeToWLAN */
	si_gci_indirect(sih, GCI_REGIDX(GCI_BT_MBDATA_TOWLAN_POS),
		GCI_OFFSETOF(sih, gci_eventintmask),
		GCI_MAILBOXDATA_TOWLAN | GCI_MAILBOXACK_TOWLAN | GCI_WAKE_TOWLAN,
		GCI_MAILBOXDATA_TOWLAN | GCI_MAILBOXACK_TOWLAN | GCI_WAKE_TOWLAN);

	/* enable event interrupt */
	si_gci_direct(sih, GCI_OFFSETOF(sih, gci_intmask),
		GCI_INTSTATUS_EVENT | GCI_INTSTATUS_EVENTWAKE,
		GCI_INTSTATUS_EVENT | GCI_INTSTATUS_EVENTWAKE);

	/* clear wlan output */
	si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[2]), ALLONES_32, 0);
	si_gci_direct(sih, GCI_OFFSETOF(sih, gci_output[3]), ALLONES_32, 0);

	/* register BT to WLAN event interrupt handler */
	bwte_info->gci_event_handle =
		hnd_gci_mb_handler_register(GCI_REGIDX(GCI_BT_MBDATA_TOWLAN_POS),
		GCI_MAILBOXDATA_TOWLAN | GCI_MAILBOXACK_TOWLAN | GCI_WAKE_TOWLAN,
		bwte_gci_bt2wl_handler, bwte_info);

	/* enable gci interrupt */
	si_gci_int_enable(sih, TRUE);

	return 0;
}
#endif /* BWTE_BTINTR_GCI */

void*
BCMATTACHFN(wlc_bwte_attach)(wlc_info_t* wlc)
{
	bwte_info_t *bwte_info;
	si_t *hndrte_sih;
	int ret = BCME_OK;
	size_t len;

	hndrte_sih = wlc->pub->sih;
	bwte_info = (bwte_info_t*) obj_registry_get(wlc->objr, OBJR_BWTE_INFO);
	if (bwte_info != NULL) {
		/* Found previous instance, reuse  */
		goto exit;
	}
	bwte_info = MALLOCZ(wlc->osh, sizeof(bwte_info_t));
	if (bwte_info == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	obj_registry_set(wlc->objr, OBJR_BWTE_INFO, bwte_info);

	len = sizeof(bwte_shared_block) + (sizeof(bwte_client_shared_block*) * BWTE_MAX_CLIENT_CNT);
	bwte_info->sb = MALLOCZ(wlc->osh, len);
	if (bwte_info->sb == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	bwte_info->client = MALLOCZ(wlc->osh,
			sizeof(bwte_client_info_t) * BWTE_MAX_CLIENT_CNT);
	if (bwte_info->client == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	bwte_info->sb->max_clients = BWTE_MAX_CLIENT_CNT;
	bwte_info->sb->csb = (bwte_client_shared_block**)
		((uint8*)bwte_info->sb + sizeof(bwte_shared_block));
	bwte_info->objr = wlc->objr;
	bwte_info->osh = wlc->osh;
	bwte_info->sih = hndrte_sih;
	bwte_info->sb->version = BWTE_SHARED_BLCOK_VERSION;
	bwte_info->sb->length = sizeof(bwte_shared_block);

	bwte_info->stats = MALLOCZ(wlc->osh, sizeof(bwte_stats_t));
	if (bwte_info->stats == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	WL_TRACE(("corerev(%d), bp_addr(%p), bp_data(%p), gpioout_addr(%p)\n",
		si_corerev(bwte_info->sih),
		si_corereg_addr(bwte_info->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, bp_addrlow)),
		si_corereg_addr(bwte_info->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, bp_data)),
		si_corereg_addr(bwte_info->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpioout))));
	WL_TRACE(("sb(%p), b_g(%p), w_g(%p), b2w_i(%p), w2b_i(%p)\n",
		OSL_OBFUSCATE_BUF(&bwte_info->sb), OSL_OBFUSCATE_BUF(&bwte_info->sb->btgpio),
		OSL_OBFUSCATE_BUF(&bwte_info->sb->wlangpio),
		OSL_OBFUSCATE_BUF(&bwte_info->stats->bt2wl_intrcnt),
		OSL_OBFUSCATE_BUF(&bwte_info->stats->wl2bt_intrcnt)));
	si_corereg(bwte_info->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, bp_addrlow), ~0, 0);
	si_corereg(bwte_info->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, bp_data), ~0, 0);

	/* update shared structure with MAGIC_NUMBER */
	bwte_info->sb->magic_num = FIXED_LOCCATION_MAGIC;

#ifdef BWTE_BTINTR_GPIO
	bwte_gpio_init(bwte_info, bwte_info->sih);
#endif /* BWTE_BTINTR_GPIO */
#ifdef BWTE_BTINTR_GCI
	bwte_gci_init(bwte_info, bwte_info->sih);

	bwte_info->intfrombt = STATE_NONE;
	bwte_info->inttobt = STATE_NONE;
#endif /* BWTE_BTINTR_GCI */

	/* register a module (to handle iovars) */
	ret = wlc_module_register(wlc->pub, bwte_iovars, "bwte_iovars", bwte_info,
		wlc_bwte_doiovar, NULL, NULL, NULL);
	if (ret) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed, ret=%d\n",
			wlc->pub->unit, __FUNCTION__, ret));
		goto fail;
	}

	si_corereg(bwte_info->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, bp_data), ~0,
		(uint)(&bwte_info->sb));

	/* make sure this is the last thing we do - the BT side will start
	   accessing as soon as this is set.
	*/
	si_corereg(bwte_info->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, bp_addrlow), ~0,
		FIXED_LOCCATION_MAGIC);

exit:
	(void)obj_registry_ref(wlc->objr, OBJR_BWTE_INFO);
	return (void *)bwte_info;

fail:
	MODULE_DETACH(bwte_info, wlc_bwte_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_bwte_detach)(bwte_info_t *bwte_info)
{
	if (bwte_info && (obj_registry_unref(bwte_info->objr, OBJR_BWTE_INFO) == 0)) {
		obj_registry_set(bwte_info->objr, OBJR_BWTE_INFO, NULL);
		size_t len = (sizeof(bwte_shared_block) +
			(sizeof(bwte_client_shared_block*) * bwte_info->sb->max_clients));

#ifdef BWTE_BTINTR_GCI
		if (bwte_info->gci_event_handle) {
			hnd_gci_mb_handler_unregister(bwte_info->gci_event_handle);
			bwte_info->gci_event_handle = NULL;
		}
#endif /* BWTE_BTINTR_GCI */

		MFREE(bwte_info->osh, bwte_info->sb, len);
		MFREE(bwte_info->osh, bwte_info->client,
				sizeof(bwte_client_info_t) * BWTE_MAX_CLIENT_CNT);
		MFREE(bwte_info->osh, bwte_info->stats, sizeof(bwte_stats_t));
		MFREE(bwte_info->osh, bwte_info, sizeof(bwte_info_t));
	}
}

static int
wlc_bwte_doiovar(void *hdl, uint32 actionid,
	void *p, uint plen, void *a, uint alen, uint vsize, struct wlc_if *wlcif)
{
	bwte_info_t *bwte_info = hdl;
	bwte_stats_t *stats = (bwte_stats_t *)p;
	int err = 0;

	if (stats->version != WL_BWTE_STATS_VERSION) {
		WL_ERROR(("%s: unsupported version %d\n", __FUNCTION__, stats->version));
		return BCME_VERSION;
	}

	switch (actionid) {
#ifdef WLCNT
		case IOV_GVAL(IOV_BWTE_STATS):
		{
			if (alen < sizeof(bwte_stats_t))
				return BCME_BUFTOOSHORT;

			/* update bwte state */
			bwte_info->stats->inttobt = bwte_info->inttobt;
			bwte_info->stats->intfrombt = bwte_info->intfrombt;
			memcpy((char *)a, (char *)bwte_info->stats, sizeof(bwte_stats_t));
			break;
		}

		case IOV_GVAL(IOV_BWTE_STATS_CLEAR):
		{
			memset((char *)bwte_info->stats, 0, sizeof(bwte_stats_t));
			break;
		}
#endif /* WLCNT */

		default:
			err = BCME_UNSUPPORTED;
			break;
	}

	return err;
}

int wlc_bwte_register_client(bwte_info_t *bwte_info, int client_id, wlc_bwte_cb f_ctl,
	wlc_bwte_cb f_lo_data, wlc_bwte_cb f_hi_data, void* arg)
{
	bwte_client_shared_block* p_csb;

	if (!bwte_info) {
		WL_ERROR(("%s: %d\n", __FUNCTION__, __LINE__));
		return BCME_BADARG;
	}

	if ((client_id < 0) || (client_id >= bwte_info->sb->max_clients)) {
		WL_ERROR(("%s: Invlaid client_id(%d)\n", __FUNCTION__, client_id));
		return BCME_BADARG;
	}

	if (bwte_info->sb->csb[client_id]) {
		WL_ERROR(("%s: csb already exist\n", __FUNCTION__));
		return BCME_NOTUP;
	}

	if (!f_ctl && !f_lo_data && !f_hi_data) {
		return BCME_BADARG;
	}

	bwte_info->sb->csb[client_id] = (bwte_client_shared_block *)MALLOCZ(bwte_info->osh,
		sizeof(bwte_client_shared_block));
	if (!bwte_info->sb->csb[client_id]) {
		return BCME_NORESOURCE;
	}
	p_csb = bwte_info->sb->csb[client_id];

	p_csb->bt2wlan_aclchan.nodes_cnt = DATA_PKT_SLOT_CNT;
	p_csb->wlan2bt_aclchan.nodes_cnt = DATA_PKT_SLOT_CNT;

	p_csb->bt2wlan_scochan.nodes_cnt = DATA_PKT_SLOT_CNT;
	p_csb->wlan2bt_scochan.nodes_cnt = DATA_PKT_SLOT_CNT;

	WL_TRACE(("b2w_c(%p), w2b_c(%p), b2w_a(%p), w2b_a(%p), b2w_s(%p), w2b_s(%p)\n",
		OSL_OBFUSCATE_BUF(&p_csb->bt2wlan_ctlchan),
		OSL_OBFUSCATE_BUF(&p_csb->wlan2bt_ctlchan),
		OSL_OBFUSCATE_BUF(&p_csb->bt2wlan_aclchan),
		OSL_OBFUSCATE_BUF(&p_csb->wlan2bt_aclchan),
		OSL_OBFUSCATE_BUF(&p_csb->bt2wlan_scochan),
		OSL_OBFUSCATE_BUF(&p_csb->wlan2bt_scochan)));

	bwte_info->client[client_id].ctl_func = f_ctl;
	bwte_info->client[client_id].lo_data_func = f_lo_data;
	bwte_info->client[client_id].hi_data_func = f_hi_data;
	bwte_info->client[client_id].arg = arg;
	bwte_info->client_cnt++;

	return BCME_OK;
}

void wlc_bwte_unregister_client(bwte_info_t *bwte_info, int client_id)
{
	if (!bwte_info) {
		WL_ERROR(("%s: %d\n", __FUNCTION__, __LINE__));
		return;
	}

	if (!bwte_is_client_id_valid(bwte_info, client_id)) {
		return;
	}

	MFREE(bwte_info->osh, (void *)bwte_info->sb->csb[client_id],
		sizeof(bwte_client_shared_block));
	bwte_info->sb->csb[client_id] = NULL;
	bwte_info->client[client_id].ctl_func = NULL;
	bwte_info->client[client_id].lo_data_func = NULL;
	bwte_info->client[client_id].hi_data_func = NULL;
}

int wlc_bwte_send(bwte_info_t *bwte_info, int client_id, wlc_bwte_payload_t payload, uchar* buf,
	int len, wlc_bwte_cb free_func, void* arg)
{
	bool toggle_bt = FALSE;

	if (!bwte_info) {
		WL_ERROR(("%s: bwte_info is NULL\n", __FUNCTION__));
		return BCME_BADARG;
	}

	if (!bwte_is_client_id_valid(bwte_info, client_id)) {
		WL_ERROR(("%s: client_id error, client_id(%d)\n", __FUNCTION__, client_id));
		return BCME_BADARG;
	}

	if (!buf || !len) {
		WL_ERROR(("%s: msg buffer error, msg(%p, %d)\n",
			__FUNCTION__, OSL_OBFUSCATE_BUF(buf), len));
		return BCME_BADARG;
	}

	if (payload == WLC_BWTE_CTL_MSG) {
		bwte_ctlchan_t *chan = &bwte_info->sb->csb[client_id]->wlan2bt_ctlchan;

		if (chan->msg_inuse || chan->msg) {
			WL_ERROR(("%s: existing msg ongoing (%d, %p)\n", __FUNCTION__,
				chan->msg_inuse, OSL_OBFUSCATE_BUF(chan->msg)));
			return BCME_NORESOURCE;
		}

		chan->msg = buf;
		chan->msg_len = len;
		chan->free_func = free_func;
		chan->arg = arg;
		chan->msg_inuse = 1;
		toggle_bt = TRUE;
	} else if ((payload == WLC_BWTE_LO_DATA) || (payload == WLC_BWTE_HI_DATA)) {
		bwte_datachan_t *chan = (payload == WLC_BWTE_LO_DATA) ?
			&bwte_info->sb->csb[client_id]->wlan2bt_aclchan :
			&bwte_info->sb->csb[client_id]->wlan2bt_scochan;

		if (((chan->wr_idx + 1) % chan->nodes_cnt) != chan->rd_idx) {
			chan->pkts[chan->wr_idx].p = buf;
			if (((uint32)buf) % 4) {
				WL_ERROR(("Unalligned addr: 0x%p\n",
					OSL_OBFUSCATE_BUF(chan->pkts[chan->wr_idx].p)));
			}
			chan->pkts[chan->wr_idx].len = len;
			chan->pkts[chan->wr_idx].free_func = free_func;
			chan->pkts[chan->wr_idx].arg = arg;
			chan->wr_idx++;
			if (chan->wr_idx == chan->nodes_cnt) {
				chan->wr_idx = 0;
			}
			toggle_bt = TRUE;
		}
	}

	if (toggle_bt) {
#ifdef BWTE_BTINTR_GPIO
		bwte_toggle_gpio(bwte_info, FALSE);
#endif /* BWTE_BTINTR_GPIO */
#ifdef BWTE_BTINTR_GCI
		if (bwte_info->inttobt != STATE_NONE) {
			bwte_info->inttobt_pending = TRUE;
		}
		else {
			bwte_info->inttobt = W2B_DATA_SET;
			bwte_gcidata_tobt(bwte_info, TRUE);
			/* update interrupt status */
			bwte_info->stats->wl2bt_dset_cnt++;
		}
#endif /* BWTE_BTINTR_GCI */
		return BCME_OK;
	} else {
		return BCME_NORESOURCE;
	}
}

void wlc_bwte_process_bt_intr(bwte_info_t *bwte_info, int client_id)
{
	if (!bwte_info) {
		WL_ERROR(("%s: bwte_info is NULL\n", __FUNCTION__));
		return;
	}

	bwte_client_isr(bwte_info, client_id, FALSE);
}

void wlc_bwte_reclaim_wlan_buf(bwte_info_t *bwte_info, int client_id, wlc_bwte_payload_t payload,
	bool cleanup)
{
	if (!bwte_info) {
		WL_ERROR(("%s: arg is NULL\n", __FUNCTION__));
		return;
	}

	if (!bwte_is_client_id_valid(bwte_info, client_id)) {
		WL_ERROR(("%s: client_id error, client_id(%d)\n", __FUNCTION__, client_id));
		return;
	}

	if (payload == WLC_BWTE_CTL_MSG) {
		bwte_ctlchan_t *chan = &bwte_info->sb->csb[client_id]->wlan2bt_ctlchan;

		if (!chan->msg_inuse && chan->msg) {
			/* bt finish using wlan message */
			if (chan->free_func) {
				chan->free_func(chan->arg, chan->msg, chan->msg_len);
			}
			chan->msg = NULL;
			chan->msg_len = 0;
			chan->free_func = NULL;
			chan->arg = NULL;
		}
	} else if ((payload == WLC_BWTE_LO_DATA) || (payload == WLC_BWTE_HI_DATA)) {
		bwte_datachan_t *chan = (payload == WLC_BWTE_LO_DATA) ?
			&bwte_info->sb->csb[client_id]->wlan2bt_aclchan :
			&bwte_info->sb->csb[client_id]->wlan2bt_scochan;
		bwte_pkt_t *bwte_pkt;
		uint16 end_idx;

		end_idx = cleanup ? chan->wr_idx : chan->rd_idx;

		while (chan->free_idx != end_idx)
		{
			bwte_pkt = &chan->pkts[chan->free_idx];
			if (!bwte_pkt->p) {
				break;
			}

			if (bwte_pkt->free_func) {
				bwte_pkt->free_func(bwte_pkt->arg, bwte_pkt->p, bwte_pkt->len);
			}

			bwte_pkt->p = NULL;
			bwte_pkt->len = 0;
			chan->free_idx++;
			if (chan->free_idx == chan->nodes_cnt) {
				chan->free_idx = 0;
			}
		}

		if (cleanup) {
			chan->rd_idx = chan->wr_idx = chan->free_idx = 0;
		}
	}
}
