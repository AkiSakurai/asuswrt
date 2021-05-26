/*
 * BMAC iovar table and registration
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
 * $Id: wlc_bmac_iocv.c 790074 2020-08-14 08:34:10Z $
 */

/**
 * @file
 * @brief
 * XXX Twiki: [WlBmacDesign]
 */

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>

#include <osl.h>
#include <bcmutils.h>
#include <d11_cfg.h>
#include <siutils.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_bmac.h>

#include <wlc_iocv_types.h>
#include <wlc_iocv_reg.h>
#include <wlc_bmac_iocv.h>

#include <pcicfg.h>
#include <pcie_core.h>
#include <bcmsrom.h>

#include <wlc_hw_priv.h>
#include <wl_export.h>

#include <sbchipc.h>
#include <bcmotp.h>
#include <bcmnvram.h>
#include <bcmdevs.h>
#include <phy_btcx_api.h>
#include <phy_misc_api.h>

#ifdef WLDIAG
#include <wlc_diag.h>
#endif // endif

#ifdef ROUTER_COMA
#include <hndchipc.h>
#include <hndjtagdefs.h>
#endif // endif
#include <hndpmu.h>

#ifdef SAVERESTORE
#include <saverestore.h>
#endif // endif

/* iovar table */
static const bcm_iovar_t wlc_bmac_iovt[] = {
#if defined(WLTEST)
	{"cis_source", IOV_BMAC_CIS_SOURCE, IOVF_MFG, 0, IOVT_INT32, 0},
	{"devpath", IOV_BMAC_DEVPATH, IOVF_MFG, 0, IOVT_BUFFER, 0},
#endif // endif
#ifdef WLDIAG
	{"diag", IOV_BMAC_DIAG, 0, 0, IOVT_UINT32, 0},
#endif /* WLDIAG */
#ifdef WLLED
	{"gpiotimerval", IOV_BMAC_SBGPIOTIMERVAL, 0, 0, IOVT_UINT32, sizeof(uint32)},
	{"leddc", IOV_BMAC_LEDDC, IOVF_OPEN_ALLOW, 0, IOVT_UINT32, sizeof(uint32)},
#endif /* WLLED */
	{"wpsgpio", IOV_BMAC_WPSGPIO, 0, 0, IOVT_UINT32, 0},
	{"wpsled", IOV_BMAC_WPSLED, 0, 0, IOVT_UINT32, 0},
	{"btclock_tune_war", IOV_BMAC_BTCLOCK_TUNE_WAR, 0, 0, IOVT_UINT32, 0},
	{"bt_regs_read",	IOV_BMAC_BT_REGS_READ, 0, 0, IOVT_BUFFER, 0},
	{"gpioout", IOV_BMAC_SBGPIOOUT, 0, 0, IOVT_BUFFER, 0},
#if (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && defined(WLTEST)
	{"otpdump", IOV_BMAC_OTPDUMP, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"otpstat", IOV_BMAC_OTPSTAT, 0, 0, IOVT_BUFFER, 0},
	{"otpsize", IOV_BMAC_OTPSIZE, 0, 0, IOVT_UINT32, 0},
	{"otpread", IOV_BMAC_OTPREAD, IOVF_MFG, 0, IOVT_BUFFER, sizeof(wl_otpread_cmd_t)},
#endif /* (defined(BCMNVRAMR) || defined (BCMNVRAMW)) && (defined(WLTEST) */
	/* || defined (BCMINTERNAL)) */
#if defined(BCMSDIODEV)
	{"sd_drivestrength", IOV_BMAC_SDIO_DRIVE, 0, 0, IOVT_UINT32, 0},
#endif /* defined(BCMSDIODEV) */
	{"aspm", IOV_BMAC_PCIEASPM, 0, 0, IOVT_INT16, 0},
	{"correrrmask", IOV_BMAC_PCIEADVCORRMASK, 0, 0, IOVT_INT16, 0},
#ifdef BCMDBG
	{"pcieclkreq", IOV_BMAC_PCIECLKREQ, 0, 0, IOVT_INT8, 0},
	{"pcielcreg", IOV_BMAC_PCIELCREG, 0, 0, IOVT_UINT32, 0},
#endif /* BCMDBG */
	{"pciereg", IOV_BMAC_PCIEREG, 0, 0, IOVT_BUFFER, 0},
	{"pmuccreg", IOV_BMAC_PMUCHIPCTRLREG, 0, 0, IOVT_BUFFER, 0},
	{"gciccreg", IOV_BMAC_GCICHIPCTRLREG, 0, 0, IOVT_BUFFER, 0},
	{"pcieserdesreg", IOV_BMAC_PCIESERDESREG, 0, 0, IOVT_BUFFER, 0},
#ifdef BCMDBG
	{"dmalpbk", IOV_BMAC_DMALPBK, IOVF_SET_UP, 0, IOVT_UINT8, 0},
#endif // endif
#if defined(WLTEST)
#ifdef BCMNVRAMW
	{"otpw", IOV_BMAC_OTPW, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"nvotpw", IOV_BMAC_NVOTPW, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"cisvar", IOV_BMAC_CISVAR, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"otplock", IOV_BMAC_OTPLOCK, IOVF_MFG, 0, IOVT_INT32, 0},
	{"otprawr", IOV_BMAC_OTP_RAW_READ, IOVF_MFG, 0, IOVT_INT32, 0},
	{"otpraw", IOV_BMAC_OTP_RAW, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"otpeccw", IOV_BMAC_OTP_ECC_WR, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"otpecc_en", IOV_BMAC_OTP_ECC_ENAB, IOVF_MFG, 0, IOVT_INT8, 0},
	{"otpecc_cleardblerr", IOV_BMAC_OTP_ECC_CLEAR, IOVF_MFG, 0, IOVT_INT8, 0},
	{"otpecc_rows", IOV_BMAC_OTP_ECC_ROWS, 0, 0, IOVT_BUFFER, 0},
#endif /* BCMNVRAMW */
	{"axi", IOV_AXI, 0, 0, IOVT_BUFFER, 0},
#endif // endif
	{"srom", IOV_BMAC_SROM, 0, 0, IOVT_BUFFER, 0},
#if defined(BCMDBG) || defined(WLTEST)
	{"srcrc", IOV_BMAC_SRCRC, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"nvram_source", IOV_BMAC_NVRAM_SOURCE, IOVF_MFG, 0, IOVT_UINT8, 0},
#endif // endif
	{"customvar1", IOV_BMAC_CUSTOMVAR1, 0, 0, IOVT_UINT32, 0},
	{"generic_dload", IOV_BMAC_GENERIC_DLOAD, 0, 0, IOVT_BUFFER, 0},
	{"noise_metric", IOV_BMAC_NOISE_METRIC, 0, 0, IOVT_UINT16, 0},
	{"avoidance_cnt", IOV_BMAC_AVIODCNT, 0, 0, IOVT_UINT32, 0},
#if defined(BCMDBG)
	{"filter_war", IOV_BMAC_FILT_WAR, 0, 0, IOVT_UINT8, 0},
	{"suspend_mac", IOV_BMAC_SUSPEND_MAC, 0, 0, IOVT_UINT32, 0},
#if !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
	{"timestamp", IOV_BMAC_TIMESTAMP, 0, 0, IOVT_BUFFER, 0},
#endif /* !defined (BCMDBG_EXCLUDE_HW_TIMESTAMP) */
	{"rcvlazy", IOV_BMAC_RCVLAZY, 0, 0, IOVT_INT32, 0},
#endif /* BCMDBG */
	{"btswitch", IOV_BMAC_BTSWITCH,	0, 0, IOVT_INT32, 0},
	{"vcofreq_pcie2", IOV_BMAC_4360_PCIE2_WAR, IOVF_SET_DOWN, 0, IOVT_INT32, 0},
	{"edcrs", IOV_BMAC_EDCRS, IOVF_SET_UP | IOVF_GET_UP, 0, IOVT_UINT8, 0},
#if defined(WLTEST)
	{"bmac_bf", IOV_BMAC_BOARDFLAGS, IOVF_SET_DOWN | IOVF_MFG, 0, IOVT_UINT32, 0},
	{"bmac_bf2", IOV_BMAC_BOARDFLAGS2, IOVF_SET_DOWN | IOVF_MFG, 0, IOVT_UINT32, 0},
#endif // endif
	{"macfreq", IOV_BMAC_MACFREQ, 0, 0, IOVT_UINT8, 0},
#if defined(BCMDBG)
	{"hw_hdrconv", IOV_BMAC_HDR_CONV, 0, 0, IOVT_UINT32, 0},
#endif // endif
#ifdef LDO3P3_MIN_RES_MASK
	{"ldo_prot_ovrd", IOV_BMAC_LDO3P3_PROTECTION_OVERRIDE, (0), 0, IOVT_INT32, 0},
#endif /* LDO3P3_MIN_RES_MASK */
#ifdef GPIO_TXINHIBIT
	{"tx_inhibit_tout", IOV_BMAC_TX_INHIBIT_TOUT, (IOVF_SET_UP), 0, IOVT_UINT16, 0},
#endif // endif
	{"otp_pu", IOV_BMAC_OTP_PU, 0, 0, IOVT_UINT32, sizeof(uint32)},
#ifdef BCMDBG_SR
	{"sr_verify", IOV_BMAC_SR_VERIFY, IOVF_GET_UP, 0, IOVT_BUFFER, 0},
#endif // endif
	{NULL, 0, 0, 0, 0, 0}
};

/*
This routine, dumps the contents of the BT registers and memory.
To access BT register, we use interconnect registers.
These registers  are at offset 0xd0, 0xd4, 0xe0 and 0xd8.
Backplane addresses low and high are at offset 0xd0 and 0xd4 and
contain the lower and higher 32 bits of a 64-bit address used for
indirect accesses respectively. Backplane indirect access
register is at offset 0xe0. Bits 3:0 of this register contain
the byte enables supplied to the system backplane on indirect
backplane accesses. So they should be set to 1. Bit 9 (StartBusy bit)
is set to 1 to start an indirect backplane access.
The hardware clears this bit to 0 when the transfer completes.
*/
#define  STARTBUSY_BIT_POLL_MAX_TIME 50
#define  INCREMENT_ADDRESS 4

#define DL_MAX_CHUNK_LEN 1408 /* 8 * 8 * 22 */

static uint wlc_bmac_dma_avoidance_cnt(wlc_hw_info_t *wlc_hw);
static int wlc_bmac_bt_regs_read(wlc_hw_info_t *wlc_hw, uint32 stAdd, uint32 dump_size, uint32 *b);

#if (defined(BCMNVRAMR) || (defined(BCMNVRAMW) && !defined(WLTEST_DISABLED))) && \
	(defined(WLTEST) && !defined(WLTEST_DISABLED))
static int wlc_bmac_cissource(wlc_hw_info_t *wlc_hw);
#endif // endif

#if defined(BCMDBG)
static void wlc_bmac_suspend_timeout(void *arg);
#endif /* BCMDBG */

#if defined(STA) && defined(BCMDBG)
static void wlc_bmac_dma_lpbk(wlc_hw_info_t *wlc_hw, bool enable);
#endif // endif

#if defined(BCMDBG)
static void
wlc_bmac_suspend_timeout(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;

	ASSERT(wlc);

	if (wlc->bmac_suspend_timer) {
		wl_del_timer(wlc->wl, wlc->bmac_suspend_timer);
		wlc->is_bmac_suspend_timer_active = FALSE;
	}

	if (wlc->pub->up) {
		wlc_bmac_enable_mac(wlc->hw);
	}

	return;
}
#endif /* BCMDBG */

#if defined(STA) && defined(BCMDBG)
static void
wlc_bmac_dma_lpbk(wlc_hw_info_t *wlc_hw, bool enable)
{
	if (BUSTYPE(wlc_hw->sih->bustype) != PCI_BUS ||
	    wlc_bmac_pio_enab_check(wlc_hw))
		return;

	if (enable) {
		wlc_bmac_suspend_mac_and_wait(wlc_hw);
		dma_fifoloopbackenable(wlc_hw->di[TX_DATA_FIFO]);
	} else {
		dma_txreset(wlc_hw->di[TX_DATA_FIFO]);
		wlc_upd_suspended_fifos_clear(wlc_hw, TX_DATA_FIFO);
		wlc_bmac_enable_mac(wlc_hw);
	}
}
#endif /* defined(STA) && defined(BCMDBG) */

/** DMA segment list related */
static uint
wlc_bmac_dma_avoidance_cnt(wlc_hw_info_t *wlc_hw)
{
	uint i, total = 0;

	/* get total DMA avoidance counts */
	for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc_hw->wlc); i++) {
		if (wlc_hw->di[i]) {
			total += dma_avoidance_cnt(wlc_hw->di[i]);
		}
	}

	return (total);
}

static int
wlc_bmac_bt_regs_read(wlc_hw_info_t *wlc_hw, uint32 stAdd, uint32 dump_size, uint32 *b)
{
	uint32 regval1;
	uint32 cur_val = stAdd;
	uint32 endAddress = stAdd + dump_size;
	int counter = 0;
	int delay_val;
	int err;
	while (cur_val < endAddress) {
		si_ccreg(wlc_hw->sih, 0xd0, ~0, cur_val);
		si_ccreg(wlc_hw->sih, 0xd4, ~0, 0);
		si_ccreg(wlc_hw->sih, 0xe0, ~0, 0x20f);
		/*
		The StartBusy bit is set to 1 to start an indirect backplane access.
		The hardware clears this field to 0 when the transfer completes.
		*/
		for (delay_val = 0; delay_val < STARTBUSY_BIT_POLL_MAX_TIME; delay_val++) {
			if (si_ccreg(wlc_hw->sih, 0xe0, 0, 0) == 0x0000000F)
				break;
			OSL_DELAY(100);
		}
		if (delay_val == (STARTBUSY_BIT_POLL_MAX_TIME)) {
			err = BCME_ERROR;
			return err;
		}
		regval1 = si_ccreg(wlc_hw->sih, 0xd8, 0, 0);
		b[counter] = regval1;
		counter++;
		cur_val += INCREMENT_ADDRESS;
	}
	return 0;
}

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && (defined(WLTEST) && \
	!defined(WLTEST_DISABLED))
static int
wlc_bmac_cissource(wlc_hw_info_t *wlc_hw)
{
	int ret = 0;

	switch (si_cis_source(wlc_hw->sih)) {
	case CIS_OTP:
		ret = WLC_CIS_OTP;
		break;
	case CIS_SROM:
		ret = WLC_CIS_SROM;
		break;
	case CIS_DEFAULT:
		ret = WLC_CIS_DEFAULT;
		break;
	default:
		ret = BCME_ERROR;
		break;
	}

	return ret;
}
#endif // endif

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

static int
wlc_bmac_doiovar(void *hw, uint32 actionid,
	void *params, uint p_len, void *arg, uint len, uint val_size, struct wlc_if *wlcif)
{
	wlc_hw_info_t *wlc_hw = (wlc_hw_info_t *)hw;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;

	BCM_REFERENCE(val_size);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy(((uint8*)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;
	bool_val2 = (int_val2 != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val2);

	WL_TRACE(("%s(): actionid=%d, p_len=%d, len=%d\n", __FUNCTION__, actionid, p_len, len));

	switch (actionid) {
#ifdef WLDIAG
	case IOV_GVAL(IOV_BMAC_DIAG): {
		uint32 result;
		uint32 diagtype;

		/* recover diagtype to run */
		bcopy((char *)params, (char *)(&diagtype), sizeof(diagtype));
		err = wlc_diag(wlc_hw->wlc, diagtype, &result);
		bcopy((char *)(&result), arg, sizeof(diagtype)); /* copy result to be buffer */
		break;
	}
#endif /* WLDIAG */

#ifdef WLLED
	case IOV_GVAL(IOV_BMAC_SBGPIOTIMERVAL):
	case IOV_GVAL(IOV_BMAC_LEDDC):
		*ret_int_ptr = si_gpiotimerval(wlc_hw->sih, 0, 0);
		break;
	case IOV_SVAL(IOV_BMAC_SBGPIOTIMERVAL):
	case IOV_SVAL(IOV_BMAC_LEDDC):
		si_gpiotimerval(wlc_hw->sih, ~0, int_val);
		break;
#endif /* WLLED */

	case IOV_SVAL(IOV_BMAC_SBGPIOOUT): {
		uint8 gpio;
		uint32 mask; /* GPIO pin mask */
		uint32 val;  /* GPIO value to program */
		mask = ((uint32*)params)[0];
		val = ((uint32*)params)[1];

		if (BCM4347_CHIP(wlc_hw->sih->chip)) {
			/* First get the GPIO pin */
			for (gpio = CC4347_PIN_GPIO_02; gpio <= CC4347_PIN_GPIO_13; gpio ++) {
				if ((mask >> gpio) & 0x1)
					break;
			}
			si_gci_enable_gpio(wlc_hw->sih, gpio, mask, val);
		} else {
			si_gpiocontrol(wlc_hw->sih, mask, 0, GPIO_HI_PRIORITY);
			si_gpioouten(wlc_hw->sih, mask, mask, GPIO_HI_PRIORITY);
			si_gpioout(wlc_hw->sih, mask, val, GPIO_HI_PRIORITY);
		}
		break;
	}

	case IOV_GVAL(IOV_BMAC_SBGPIOOUT): {
		uint32 gpio_cntrl;
		uint32 gpio_out;
		uint32 gpio_outen;

		if (len < (int) (sizeof(uint32) * 3))
			return BCME_BUFTOOSHORT;

		gpio_cntrl = si_gpiocontrol(wlc_hw->sih, 0, 0, GPIO_HI_PRIORITY);
		gpio_out = si_gpioout(wlc_hw->sih, 0, 0, GPIO_HI_PRIORITY);
		gpio_outen = si_gpioouten(wlc_hw->sih, 0, 0, GPIO_HI_PRIORITY);

		((uint32*)arg)[0] = gpio_cntrl;
		((uint32*)arg)[1] = gpio_out;
		((uint32*)arg)[2] = gpio_outen;
		break;
	}

	case IOV_GVAL(IOV_BMAC_WPSGPIO): {
		char *var;

		if ((var = getvar(wlc_hw->vars, "wpsgpio")))
			*ret_int_ptr = (uint32)bcm_strtoul(var, NULL, 0);
		else {
			*ret_int_ptr = -1;
			err = BCME_NOTFOUND;
		}

		break;
	}

	case IOV_GVAL(IOV_BMAC_WPSLED): {
		char *var;

		if ((var = getvar(wlc_hw->vars, "wpsled")))
			*ret_int_ptr = (uint32)bcm_strtoul(var, NULL, 0);
		else {
			*ret_int_ptr = -1;
			err = BCME_NOTFOUND;
		}

		break;
	}

	case IOV_GVAL(IOV_BMAC_BTCLOCK_TUNE_WAR):
		*ret_int_ptr = wlc_hw->btclock_tune_war;
		break;

	case IOV_SVAL(IOV_BMAC_BTCLOCK_TUNE_WAR):
		wlc_hw->btclock_tune_war = bool_val;
		break;

	case IOV_GVAL(IOV_BMAC_BT_REGS_READ): {
		/* the size of output dump can not be larger than the buffer size */
		if ((uint)int_val2 > len)
			err = BCME_BUFTOOSHORT;
		else
			err = wlc_bmac_bt_regs_read(wlc_hw, int_val, int_val2, (uint32*)arg);
		break;
	}

#if defined(WLTEST)
#if defined(WLTEST)
	case IOV_SVAL(IOV_BMAC_BOARDFLAGS): {
		break;
	}

	case IOV_SVAL(IOV_BMAC_BOARDFLAGS2): {
		break;
	}
#endif /* WLTEST */

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW))
	case IOV_GVAL(IOV_BMAC_OTPDUMP): {
		void *oh;
		uint32 macintmask;
		bool wasup;
		uint32 min_res_mask = 0;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (!(wasup = si_is_otp_powered(wlc_hw->sih)))
			si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);

		if ((oh = otp_init(wlc_hw->sih)) == NULL) {
			err = BCME_NOTFOUND;
		} else if (otp_dump(oh, int_val, (char *)arg, len) <= 0) {
			err = BCME_BUFTOOSHORT;
		}

		if (!wasup)
			si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

		break;
	}

	case IOV_GVAL(IOV_BMAC_OTPREAD):
	{
		void *oh;
		uint32 macintmask;
		bool wasup;
		uint32 min_res_mask = 0;
		wl_otpread_cmd_t read_cmd;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (!(wasup = si_is_otp_powered(wlc_hw->sih))) {
			si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);
		}

		if ((oh = otp_init(wlc_hw->sih)) == NULL) {
			err = BCME_NODEVICE;
		} else {
			bcopy((uint8*)params, &read_cmd, sizeof(read_cmd));

			if (otp_read(oh, (void *)&read_cmd, (char *)arg, len) <= 0) {
				err = BCME_NODEVICE;
			}
		}

		if (!wasup) {
			si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}

	case IOV_GVAL(IOV_BMAC_OTPSTAT): {
		void *oh;
		uint32 macintmask;
		bool wasup;
		uint32 min_res_mask = 0;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (!(wasup = si_is_otp_powered(wlc_hw->sih)))
			si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);

		if ((oh = otp_init(wlc_hw->sih)) == NULL) {
			err = BCME_NOTFOUND;
		} else if (otp_dumpstats(oh, int_val, (char *)arg, len) <= 0) {
			err = BCME_BUFTOOSHORT;
		}

		if (!wasup)
			si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

		break;
	}

	case IOV_GVAL(IOV_BMAC_OTPSIZE): {
		void *oh;
		uint32 macintmask;
		bool wasup;
		uint32 min_res_mask = 0;
		int otpsize;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (!(wasup = si_is_otp_powered(wlc_hw->sih))) {
			si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);
		}

		if ((oh = otp_init(wlc_hw->sih)) == NULL) {
			err = BCME_NODEVICE;
		} else {
			otpsize = otp_size(oh);
			*(uint32 *)arg = otpsize;
		}

		if (!wasup) {
			si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

		break;
	}
#endif /* BCMNVRAMR || BCMNVRAMW */
#endif // endif

	case IOV_GVAL(IOV_BMAC_PCIEADVCORRMASK):
			if ((BUSTYPE(wlc_hw->sih->bustype) != PCI_BUS) ||
			    (BUSCORETYPE(wlc_hw->sih->buscoretype) != PCIE_CORE_ID)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		*ret_int_ptr = si_pciereg(wlc_hw->sih, PCIE_ADV_CORR_ERR_MASK,
			0, 0, PCIE_CONFIGREGS);
		break;

	case IOV_SVAL(IOV_BMAC_PCIEADVCORRMASK):
	        if ((BUSTYPE(wlc_hw->sih->bustype) != PCI_BUS) ||
	            (BUSCORETYPE(wlc_hw->sih->buscoretype) != PCIE_CORE_ID)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Set all errors if -1 or else mask off undefined bits */
		if (int_val == -1)
			int_val = ALL_CORR_ERRORS;

		int_val &= ALL_CORR_ERRORS;
		si_pciereg(wlc_hw->sih, PCIE_ADV_CORR_ERR_MASK, 1, int_val,
			PCIE_CONFIGREGS);
		break;

	case IOV_GVAL(IOV_BMAC_PCIEASPM): {
		uint8 clkreq = 0;
		uint32 aspm = 0;

		if (BUSTYPE(wlc_hw->sih->bustype) != PCI_BUS) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* this command is to hide the details, but match the lcreg
		   #define PCIE_CLKREQ_ENAB		0x100
		   #define PCIE_ASPM_L1_ENAB	2
		   #define PCIE_ASPM_L0s_ENAB	1
		*/

		clkreq = si_pcieclkreq(wlc_hw->sih, 0, 0);
		aspm = si_pcielcreg(wlc_hw->sih, 0, 0);
		*ret_int_ptr = ((clkreq & 0x1) << 8) | (aspm & PCIE_ASPM_ENAB);
		break;
	}

	case IOV_SVAL(IOV_BMAC_PCIEASPM): {
		uint32 tmp;

		if (BUSTYPE(wlc_hw->sih->bustype) != PCI_BUS) {
			err = BCME_UNSUPPORTED;
			break;
		}

		tmp = si_pcielcreg(wlc_hw->sih, 0, 0);
		si_pcielcreg(wlc_hw->sih, PCIE_ASPM_ENAB,
			(tmp & ~PCIE_ASPM_ENAB) | (int_val & PCIE_ASPM_ENAB));

		si_pcieclkreq(wlc_hw->sih, 1, ((int_val & 0x100) >> 8));
		break;
	}
#ifdef BCMDBG
	case IOV_GVAL(IOV_BMAC_PCIECLKREQ):
		*ret_int_ptr = si_pcieclkreq(wlc_hw->sih, 0, 0);
		break;

	case IOV_SVAL(IOV_BMAC_PCIECLKREQ):
		if (int_val < AUTO || int_val > ON) {
			err = BCME_RANGE;
			break;
		}

		/* For AUTO, disable clkreq and then rest of the
		 * state machine will take care of it
		 */
		if (int_val == AUTO)
			si_pcieclkreq(wlc_hw->sih, 1, 0);
		else
			si_pcieclkreq(wlc_hw->sih, 1, (uint)int_val);
		break;

	case IOV_GVAL(IOV_BMAC_PCIELCREG):
		*ret_int_ptr = si_pcielcreg(wlc_hw->sih, 0, 0);
		break;

	case IOV_SVAL(IOV_BMAC_PCIELCREG):
		si_pcielcreg(wlc_hw->sih, 3, (uint)int_val);
		break;

#ifdef STA
	case IOV_SVAL(IOV_BMAC_DMALPBK):
		if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS &&
		    !wlc_bmac_pio_enab_check(wlc_hw)) {
			if (wlc_hw->dma_lpbk == bool_val)
				break;
			wlc_bmac_dma_lpbk(wlc_hw, bool_val);
			wlc_hw->dma_lpbk = bool_val;
		} else
			err = BCME_UNSUPPORTED;

		break;
#endif /* STA */
#endif /* BCMDBG */

	case IOV_SVAL(IOV_BMAC_PCIEREG):
		if (p_len < (int)sizeof(int_val) * 2) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val < 0) {
			err = BCME_BADARG;
			break;
		}
		si_pciereg(wlc_hw->sih, int_val, 1, int_val2, PCIE_PCIEREGS);
		break;

	case IOV_GVAL(IOV_BMAC_PCIEREG):
		if (p_len < (int)sizeof(int_val)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val < 0) {
			err = BCME_BADARG;
			break;
		}
		*ret_int_ptr = si_pciereg(wlc_hw->sih, int_val, 0, 0, PCIE_PCIEREGS);
		break;

	case IOV_SVAL(IOV_BMAC_EDCRS):
		if (!WLCISNPHY(wlc_hw->band) && !WLCISACPHY(wlc_hw->band)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		if (bool_val) {
			wlc_bmac_ifsctl_edcrs_set(wlc_hw, FALSE);
		} else {
			if (WLCISACPHY(wlc_hw->band))
				wlc_bmac_ifsctl_vht_set(wlc_hw, OFF);
			else
				wlc_bmac_ifsctl1_regshm(wlc_hw, (IFS_CTL1_EDCRS |
					IFS_CTL1_EDCRS_20L | IFS_CTL1_EDCRS_40), 0);
		}
		break;

	case IOV_GVAL(IOV_BMAC_EDCRS):
		if (!WLCISNPHY(wlc_hw->band) && !WLCISACPHY(wlc_hw->band)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		{
			uint16 val;
			if (D11REV_LT(wlc_hw->corerev, 40))
				val = wlc_bmac_read_shm(wlc_hw, M_IFSCTL1(wlc_hw));
			else
				val = wlc_bmac_read_shm(wlc_hw, M_IFS_PRICRS(wlc_hw));

			if (WLCISACPHY(wlc_hw->band))
				*ret_int_ptr = (val & IFS_CTL_ED_SEL_MASK) ? TRUE:FALSE;
			else
				*ret_int_ptr = (val & IFS_CTL1_EDCRS) ? TRUE:FALSE;
		}
		break;

	case IOV_SVAL(IOV_BMAC_PMUCHIPCTRLREG):
		if (p_len < (int)sizeof(int_val) * 2) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if (int_val < 0) {
			err = BCME_BADARG;
			break;
		}

		si_pmu_chipcontrol(wlc_hw->sih, int_val, ~0, int_val2);
		break;

	case IOV_GVAL(IOV_BMAC_PMUCHIPCTRLREG):
		if (p_len < (int)sizeof(int_val)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val < 0) {
			err = BCME_BADARG;
			break;
		}

		*ret_int_ptr = si_pmu_chipcontrol(wlc_hw->sih, int_val, 0, 0);
		break;

	case IOV_SVAL(IOV_BMAC_GCICHIPCTRLREG):
		if (p_len < (int)sizeof(int_val) * 2) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val < 0) {
			err = BCME_BADARG;
			break;
		}

		si_gci_chipcontrol(wlc_hw->sih, int_val, ~0, int_val2);
		break;

	case IOV_GVAL(IOV_BMAC_GCICHIPCTRLREG):
		if (p_len < (int)sizeof(int_val)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val < 0) {
			err = BCME_BADARG;
			break;
		}

		*ret_int_ptr = si_gci_chipcontrol(wlc_hw->sih, int_val, 0, 0);
		break;

	case IOV_SVAL(IOV_BMAC_PCIESERDESREG): {
		int32 int_val3;
		if (p_len < (int)sizeof(int_val) * 3) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val < 0 || int_val2 < 0) {
			err = BCME_BADARG;
			break;
		}
		if (BUSTYPE(wlc_hw->sih->bustype) != PCI_BUS) {
			err = BCME_UNSUPPORTED;
			break;
		}

		bcopy(((uint8*)params + 2 * sizeof(int_val)), &int_val3, sizeof(int_val));
		/* write dev/offset/val to serdes */
		si_pcieserdesreg(wlc_hw->sih, int_val, int_val2, 1, int_val3);
		break;
	}

	case IOV_GVAL(IOV_BMAC_PCIESERDESREG): {
		if (p_len < (int)sizeof(int_val) * 2) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (int_val < 0 || int_val2 < 0) {
			err = BCME_BADARG;
			break;
		}

		*ret_int_ptr = si_pcieserdesreg(wlc_hw->sih, int_val, int_val2, 0, 0);
		break;
	}

#if defined(BCMSDIODEV)
	case IOV_GVAL(IOV_BMAC_SDIO_DRIVE):
		return si_get_sdio_drive_strength(wlc_hw->sih, (uint32*)ret_int_ptr);
		break;
	case IOV_SVAL(IOV_BMAC_SDIO_DRIVE): {
		return si_set_sdio_drive_strength(wlc_hw->sih, (uint32)int_val);
		break;
	}
#endif // endif

#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
#ifdef BCMNVRAMW
	case IOV_GVAL(IOV_BMAC_OTPW): {
		uint32 macintmask = 0;
		uint nbytes = 40;
		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		if (wlc_hw->sih->chiprev == 0x2) {
			otp_read_region(wlc_hw->sih, OTP_NW_RGN, (uint16*)arg, &nbytes);
		}
		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}

	case IOV_SVAL(IOV_BMAC_OTPW):
	case IOV_SVAL(IOV_BMAC_NVOTPW): {
		void *oh;
		uint32 macintmask;
		uint32 min_res_mask = 0;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (actionid == IOV_SVAL(IOV_BMAC_OTPW)) {
			if (wlc_hw->sih->chiprev == 0x2) {
				err = otp_write_region(wlc_hw->sih, OTP_NW_RGN,
					(uint16 *)params, p_len, 0);
			} else {
				err = otp_write_region(wlc_hw->sih, OTP_HW_RGN,
					(uint16 *)params, p_len / 2, 0);
			}
		} else {
			bool wasup;

			if (!(wasup = si_is_otp_powered(wlc_hw->sih)))
				si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);

			oh = otp_init(wlc_hw->sih);
			if (oh != NULL)
				err = otp_nvwrite(oh, (uint16 *)params, p_len / 2);
			else
				err = BCME_NOTFOUND;

			if (!wasup)
				si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

		break;
	}

	case IOV_SVAL(IOV_BMAC_CISVAR): {
		/* note: for SDIO, this IOVAR will fail on an unprogrammed OTP. */
		uint32 macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		bool wasup;
		uint32 min_res_mask = 0;
		if (!(wasup = si_is_otp_powered(wlc_hw->sih))) {
			si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);
			if (!si_is_otp_powered(wlc_hw->sih)) {
				err = BCME_NOTFOUND;
				break;
			}
		}

		/* for OTP wrvar */
		if ((BCM4350_CHIP(wlc_hw->sih->chip) &&
		    CST4350_CHIPMODE_SDIOD(wlc_hw->sih->chipst)) ||
		    (CHIPID(wlc_hw->sih->chip) == BCM43430_CHIP_ID) ||
		    BCM4347_CHIP(wlc_hw->sih->chip) ||
		    BCM4369_CHIP(wlc_hw->sih->chip)) {
			err = otp_cis_append_region(wlc_hw->sih, OTP_HW_RGN, (char*)params, p_len);
		} else {
			err = otp_cis_append_region(wlc_hw->sih, OTP_SW_RGN, (char*)params, p_len);
		}
		if (!wasup)
			si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);

		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

		break;
	}

	case IOV_GVAL(IOV_BMAC_OTPLOCK): {
		uint32 macintmask;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		*ret_int_ptr = otp_lock(wlc_hw->sih);

		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

		break;
	}
#endif /* BCMNVRAMW */

#if defined(BCMNVRAMR) || defined(BCMNVRAMW)
	case IOV_GVAL(IOV_BMAC_OTP_RAW_READ):
	{
		uint32 macintmask;
		uint32 min_res_mask = 0;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		if (si_is_otp_disabled(wlc_hw->sih)) {
			WL_INFORM(("OTP do not exist\n"));
			err = BCME_NOTFOUND;
		} else {
			bool wasup;
			uint32 i, offset, data = 0;
			uint16 tmp;
			void * oh;
			if (!(wasup = si_is_otp_powered(wlc_hw->sih)))
				si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);

			oh = otp_init(wlc_hw->sih);
			if (oh == NULL)
				err = BCME_NOTFOUND;
			else  {
				offset = (*(uint32 *)params);
				offset *= 16;
				for (i = 0; i < 16; i++) {
					tmp = otp_read_bit(oh, i + offset);
					data |= (tmp << i);
				}
				*ret_int_ptr = data;
				WL_TRACE(("OTP_RAW_READ, offset %x:%x\n", offset, data));
			}
			if (!wasup)
				si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}

	case IOV_GVAL(IOV_BMAC_CIS_SOURCE): {
		uint32 macintmask;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		if ((*ret_int_ptr = wlc_bmac_cissource(wlc_hw)) == BCME_ERROR)
			err = BCME_ERROR;
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}

	case IOV_GVAL(IOV_BMAC_OTP_RAW):
	{
		uint32 macintmask;
		uint32 min_res_mask = 0;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		if (si_is_otp_disabled(wlc_hw->sih)) {
			WL_INFORM(("OTP do not exist\n"));
			err = BCME_NOTFOUND;
		} else {
			bool wasup;
			void * oh;

			if (!(wasup = si_is_otp_powered(wlc_hw->sih)))
				si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);

			oh = otp_init(wlc_hw->sih);
			if (oh == NULL)
				err = BCME_NOTFOUND;
			else  {
				uint32 i, j, offset, bits;
				uint8 tmp, data, *ptr;

				offset = int_val;
				bits = int_val2;

				ptr = (uint8 *)arg;
				for (i = 0; i < bits; ) {
					data = 0;
					for (j = 0; j < 8; j++, i++) {
						if (i >= bits)
							break;
						tmp = (uint8)otp_read_bit(oh, i + offset);
						data |= (tmp << j);
					}
					*ptr++ = data;
				}
			}
			if (!wasup)
				si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}
	case IOV_GVAL(IOV_BMAC_OTP_ECC_CLEAR):
	{
		uint32 macintmask;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		*ret_int_ptr = otp_ecc_clear_dblerrbit(wlc_hw->sih);

		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}
	case IOV_GVAL(IOV_BMAC_OTP_ECC_WR):
	{
		uint32 macintmask;
		uint32 offset;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		offset = int_val;
		*ret_int_ptr = otp_ecc_status(wlc_hw->sih, offset);
		/* Make sure to update err for failure cases */
		if ((*ret_int_ptr == BCME_BADARG) || (*ret_int_ptr == BCME_UNSUPPORTED)) {
			err = *ret_int_ptr;
		}
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}
#endif  /* defined(BCMNVRAMR) || defined (BCMNVRAMW) */

#if defined(BCMNVRAMW)
	case IOV_SVAL(IOV_BMAC_OTP_RAW):
	{
		uint32 macintmask;
		uint32 min_res_mask = 0;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		if (si_is_otp_disabled(wlc_hw->sih)) {
			WL_INFORM(("OTP do not exist\n"));
			err = BCME_NOTFOUND;
		} else {
			bool wasup;
			void * oh;

			if (!(wasup = si_is_otp_powered(wlc_hw->sih)))
				si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);

			oh = otp_init(wlc_hw->sih);
			if (oh == NULL)
				err = BCME_NOTFOUND;
			else  {
				uint32 offset, bits;
				uint8 *ptr;

				offset = int_val;
				bits = int_val2;
				ptr = (uint8 *)params + 2 * sizeof(int_val);

				err = otp_write_bits(oh, offset, bits, ptr);
			}
			if (!wasup)
				si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}
	case IOV_SVAL(IOV_BMAC_OTP_ECC_WR):
	{
		uint32 macintmask, int_val3 = 0, data;
		uint32 min_res_mask = 0;

		memcpy(&int_val3, ((uint8*)params + (2 * sizeof(int_val))), sizeof(int_val));
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		if (si_is_otp_disabled(wlc_hw->sih)) {
			WL_INFORM(("OTP do not exist\n"));
			err = BCME_NOTFOUND;
		} else {
			bool wasup;
			void * oh;

			if (!(wasup = si_is_otp_powered(wlc_hw->sih)))
				si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);

			oh = otp_init(wlc_hw->sih);
			if (oh == NULL)
				err = BCME_NOTFOUND;
			else  {
				uint32 offset, type;

				offset = int_val;
				type = int_val2;
				data = int_val3;
				err = otp_ecc_write(oh, offset, data, type);
			}
			if (!wasup)
				si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}
	case IOV_SVAL(IOV_BMAC_OTP_ECC_ENAB):
	{
		uint32 macintmask;
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		err = otp_ecc_enable(wlc_hw->sih, int_val);

		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}

	case IOV_GVAL(IOV_BMAC_OTP_ECC_ROWS):
	case IOV_SVAL(IOV_BMAC_OTP_ECC_ROWS): {
		void *oh;
		uint32 macintmask;
		bool wasup;
		uint32 min_res_mask = 0;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (si_is_otp_disabled(wlc_hw->sih)) {
			WL_INFORM(("OTP do not exist\n"));
			err = BCME_NOTFOUND;
			break;
		}
		if (!(wasup = si_is_otp_powered(wlc_hw->sih))) {
			si_otp_power(wlc_hw->sih, TRUE, &min_res_mask);
		}

		if ((oh = otp_init(wlc_hw->sih)) == NULL) {
			err = BCME_NOTFOUND;
		} else {
			wl_otpecc_rows_t cmd;
			uint32 i;
			uint16 version;
			uint32 cmdtype;

			bcopy((uint8*)params, &cmd, sizeof(cmd));

			version = cmd.version;
			cmdtype = cmd.cmdtype;

			if ((version != WL_OTPECC_ROWS_VER) ||
				(cmd.len != sizeof(wl_otpecc_rows_t))) {
				err = BCME_ERROR;
			} else if (cmdtype == WL_OTPECC_ROWS_CMD_READ) {
				if ((len < cmd.numrows * WL_ECCDUMP_ROW_SIZE_BYTE) ||
					(otp_ecc_rows_dump(oh, (void *)&cmd, (char *)arg, len)
					<= 0)) {
					err = BCME_BUFTOOSHORT;
				}
			} else if (cmdtype == WL_OTPECC_ROWS_CMD_LOCK) {
				for (i = 0; i < cmd.numrows; i++) {
					err = otp_ecc_write(oh, i + cmd.rowoffset,
						0, OTPPOC_PROG_ECC_READ_28NM);
					if (err < 0) {
						break;
					}
				}
			} else {
				err = BCME_ERROR;
			}
		}

		if (!wasup) {
			si_otp_power(wlc_hw->sih, FALSE, &min_res_mask);
		}

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);

		break;
	}
#endif /* BCMNVRAMW */

	case IOV_GVAL(IOV_BMAC_DEVPATH): {
		char devpath[SI_DEVPATH_BUFSZ];
		char devpath_pcie[SI_DEVPATH_BUFSZ];
		int devpath_length, pcie_devpath_length;
		int i;
		char *nvram_value;

		si_devpath(wlc_hw->sih, (char *)arg, SI_DEVPATH_BUFSZ);

		devpath_length = strlen((char *)arg);

		if (devpath_length && ((char *)arg)[devpath_length-1] == '/')
			devpath_length--;

		if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) {
			si_devpath_pcie(wlc_hw->sih, devpath_pcie, SI_DEVPATH_BUFSZ);
			pcie_devpath_length = strlen(devpath_pcie);
			if (pcie_devpath_length && devpath_pcie[pcie_devpath_length-1] == '/')
				pcie_devpath_length--;
		} else
			pcie_devpath_length = 0;

		for (i = 0; i < 10; i++) {
			snprintf(devpath, sizeof(devpath), "devpath%d", i);
			nvram_value = nvram_get(devpath);
			if (nvram_value &&
				(memcmp((char *)arg, nvram_value, devpath_length) == 0 ||
				(pcie_devpath_length &&
				memcmp(devpath_pcie, nvram_value, pcie_devpath_length) == 0))) {
				snprintf((char *)arg, SI_DEVPATH_BUFSZ, "%d:", i);
				break;
			}
		}

		break;
	}
#endif // endif

	case IOV_GVAL(IOV_BMAC_SROM): {
		srom_rw_t *s = (srom_rw_t *)arg;
		uint32 macintmask;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (si_is_sprom_available(wlc_hw->sih)) {
			if (srom_read(wlc_hw->sih, wlc_hw->sih->bustype,
			              (void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
			              s->byteoff, s->nbytes, s->buf, FALSE))
				err = BCME_ERROR;
#if defined(BCMNVRAMR) || defined(BCMNVRAMW)
		} else if (!si_is_otp_disabled(wlc_hw->sih)) {
#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
			err = otp_read_region(wlc_hw->sih, OTP_HW_RGN, s->buf,
			                      &s->nbytes);
#else
			err = BCME_UNSUPPORTED;
#endif // endif
#endif /* BCMNVRAMR || defined (BCMNVRAMW) && !defined(WLTEST_DISABLED) */
		} else
			err = BCME_NOTFOUND;

#if defined(DSLCPE)
#if defined(DSLCPE_WOMBO)
		if (err) {
			read_sromfile((void *)wlc_hw->sih->wl_srom_sw_map,
				s->buf, s->byteoff, s->nbytes);
			err = 0;
		}
#endif /* DSLCPE_WOMBO */
		/* Updated srom by user's change */
		sprom_update_params(wlc_hw->sih, s->buf);
#endif /* DSLCPE */

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}

#if (defined(BCMDBG) || (defined(WLTEST)&& !defined(WLTEST_DISABLED)))
	case IOV_SVAL(IOV_BMAC_SROM): {
		srom_rw_t *s = (srom_rw_t *)params;
		uint32 macintmask;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);

		if (si_is_sprom_available(wlc_hw->sih)) {
			if (srom_write(wlc_hw->sih, wlc_hw->sih->bustype,
			               (void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
			               s->byteoff, s->nbytes, s->buf))
				err = BCME_ERROR;
		} else if (!si_is_otp_disabled(wlc_hw->sih)) {
			/* srwrite to SROM format OTP */
			err = srom_otp_write_region_crc(wlc_hw->sih, s->nbytes, s->buf,
			                                TRUE);
		} else
			err = BCME_NOTFOUND;

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}

	case IOV_GVAL(IOV_BMAC_SRCRC): {
		srom_rw_t *s = (srom_rw_t *)params;

		*ret_int_ptr = (uint8)srom_otp_write_region_crc(wlc_hw->sih, s->nbytes,
		                                                s->buf, FALSE);
		break;
	}

	case IOV_GVAL(IOV_BMAC_NVRAM_SOURCE): {
		uint32 macintmask;
		uint16 buffer[65];
		int i;
		uint16 buffer1;
		int err1;

		/* intrs off */
		macintmask = wl_intrsoff(wlc_hw->wlc->wl);
		/* 0 for SROM; 1 for OTP; 2 for NVRAM */

		if (si_is_sprom_available(wlc_hw->sih)) {
			err = srom_read(wlc_hw->sih, wlc_hw->sih->bustype,
			                (void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
			                0, sizeof(buffer), buffer, FALSE);

			*ret_int_ptr = 2; /* NVRAM */

			if (!err)
				for (i = 0; i < (int)sizeof(buffer)/2; i++) {
					if ((buffer[i] != 0) && (buffer[i] != 0xffff)) {
						*ret_int_ptr = 0; /* SROM */
						break;
					}
				}
#ifdef BCMPCIEDEV
			if ((BUSTYPE(wlc_hw->sih->bustype) == SI_BUS) &&
			    BCM43602_CHIP(wlc_hw->sih->chip))
#else
			if (BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS)
#endif /* BCMPCIEDEV */
			{
				/* If we still think its nvram try a test write */
				if (*ret_int_ptr == 2) {
					err1 = srom_read(wlc_hw->sih, wlc_hw->sih->bustype,
					                (void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
					                8, sizeof(unsigned short), buffer, FALSE);
					buffer1 = buffer[0];
					srom_write_short(wlc_hw->sih, wlc_hw->sih->bustype,
					                 (void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
					                 8, 0x1234);
					err = srom_read(wlc_hw->sih, wlc_hw->sih->bustype,
					                (void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
					                8, sizeof(unsigned short), buffer, FALSE);
					if (!err1 && !err && buffer[0] == 0x1234) {
						*ret_int_ptr = 0; /* SROM */
						srom_write_short(wlc_hw->sih, wlc_hw->sih->bustype,
							(void *)(uintptr)wlc_hw->regs, wlc_hw->osh,
							8, buffer1);
					}
				}
			}
		} else if (EMBEDDED_2x2AX_CORE(wlc_hw->sih->chip) ||
			BCM6878_CHIP(wlc_hw->sih->chip)) {
			/* for SOC type devices, prefer to use NVRAM iso OTP */
			*ret_int_ptr = 2; /* NVRAM */
		} else {
			*ret_int_ptr = 1; /* OTP */
		}

		/* restore intrs */
		wl_intrsrestore(wlc_hw->wlc->wl, macintmask);
		break;
	}
#endif // endif

	/* XXX This feature should not be exposed to mfg but BMAC images doesn't have
	 * BCMINTERNAL defined. Need to build a special driver for 43231 chipid programmed
	 * with this feature compiled in if we want to program chipid via driver.
	 */

	case IOV_GVAL(IOV_BMAC_CUSTOMVAR1): {
		char *var;

		if ((var = getvar(wlc_hw->vars, "customvar1")))
			*ret_int_ptr = (uint32)bcm_strtoul(var, NULL, 0);
		else
			*ret_int_ptr = 0;

		break;
	}
	case IOV_SVAL(IOV_BMAC_GENERIC_DLOAD): {
		wl_dload_data_t *dload_ptr, dload_data;
		uint8 *bufptr;
		uint32 total_len;
		uint actual_data_offset;
		actual_data_offset = OFFSETOF(wl_dload_data_t, data);
		memcpy(&dload_data, (wl_dload_data_t *)arg, sizeof(wl_dload_data_t));
		total_len = dload_data.len + actual_data_offset;
		if ((bufptr = MALLOC(wlc_hw->osh, total_len)) == NULL) {
			err = BCME_NOMEM;
			break;
		}
		memcpy(bufptr, (uint8 *)arg, total_len);
		dload_ptr = (wl_dload_data_t *)bufptr;
		if (((dload_ptr->flag & DLOAD_FLAG_VER_MASK) >> DLOAD_FLAG_VER_SHIFT)
		    != DLOAD_HANDLER_VER) {
			err =  BCME_ERROR;
			MFREE(wlc_hw->osh, bufptr, total_len);
			break;
		}
		switch (dload_ptr->dload_type)	{
#ifdef BCMUCDOWNLOAD
		case DL_TYPE_UCODE:
			if (wlc_hw->wlc->is_initvalsdloaded != TRUE)
				wlc_process_ucodeparts(wlc_hw->wlc, dload_ptr->data);
			break;
#endif /* BCMUCDOWNLOAD */
		default:
			err = BCME_UNSUPPORTED;
			break;
		}
		MFREE(wlc_hw->osh, bufptr, total_len);
		break;
	}
	case IOV_GVAL(IOV_BMAC_UCDLOAD_STATUS):
		*ret_int_ptr = (int32) wlc_hw->wlc->is_initvalsdloaded;
		break;
	case IOV_GVAL(IOV_BMAC_UC_CHUNK_LEN):
		*ret_int_ptr = DL_MAX_CHUNK_LEN;
		break;

	case IOV_GVAL(IOV_BMAC_NOISE_METRIC):
		*ret_int_ptr = (int32)wlc_hw->noise_metric;
		break;
	case IOV_SVAL(IOV_BMAC_NOISE_METRIC):

		if ((uint16)int_val > NOISE_MEASURE_KNOISE) {
			err = BCME_UNSUPPORTED;
			break;
		}

		wlc_hw->noise_metric = (uint16)int_val;

		if ((wlc_hw->noise_metric & NOISE_MEASURE_KNOISE) == NOISE_MEASURE_KNOISE)
			wlc_bmac_mhf(wlc_hw, MHF3, MHF3_KNOISE, MHF3_KNOISE, WLC_BAND_ALL);
		else
			wlc_bmac_mhf(wlc_hw, MHF3, MHF3_KNOISE, 0, WLC_BAND_ALL);

		break;

	case IOV_GVAL(IOV_BMAC_AVIODCNT):
		*ret_int_ptr = wlc_bmac_dma_avoidance_cnt(wlc_hw);
		break;

#ifdef BCMDBG
	case IOV_SVAL(IOV_BMAC_FILT_WAR):
		phy_misc_set_filt_war(wlc_hw->band->pi, bool_val);
		break;

	case IOV_GVAL(IOV_BMAC_FILT_WAR):
		*ret_int_ptr = phy_misc_get_filt_war(wlc_hw->band->pi);
		break;

	case IOV_SVAL(IOV_BMAC_SUSPEND_MAC):
	{
		wlc_info_t *wlc = wlc_hw->wlc;
		bool suspend = TRUE;

		if (!(wlc_hw->up)) {
			err = BCME_NOTUP;
			break;
		}

		err = BCME_OK;

		if ((wlc->bmac_suspend_timer) && (wlc->is_bmac_suspend_timer_active)) {
			/* stop timer in case it's already active */
			wl_del_timer(wlc->wl, wlc->bmac_suspend_timer);
			wlc->is_bmac_suspend_timer_active = FALSE;

			if (int_val == 0) {
				/* MAC already suspended and timer will be cancelled immediately */
				wlc_bmac_enable_mac(wlc_hw);
			} else {
				/* MAC already suspended - timer will restart with new value */
				suspend = FALSE;
			}
		}
		if (int_val > 0) {
			if (wlc->bmac_suspend_timer == NULL) {
				wlc->bmac_suspend_timer = wl_init_timer(wlc->wl,
				     wlc_bmac_suspend_timeout, wlc, "bmac_suspend");
			}

			if (wlc->bmac_suspend_timer != NULL) {
				if (suspend) {
					wlc_bmac_suspend_mac_and_wait(wlc_hw);
				}

				wl_add_timer(wlc->wl, wlc->bmac_suspend_timer, int_val, FALSE);

				wlc->is_bmac_suspend_timer_active = TRUE;
			} else {
				err = BCME_ERROR;
			}
		}
		break;
	}
#if !defined(BCMDBG_EXCLUDE_HW_TIMESTAMP)
	case IOV_GVAL(IOV_BMAC_TIMESTAMP):
	{
		struct bcmstrbuf b;
		bcm_binit(&b, (char*)arg, len);
		bcm_bprintf(&b, "%s timestamp", wlc_dbg_get_hw_timestamp());
		break;
	}
#endif /* !BCMDBG_EXCLUDE_HW_TIMESTAMP */

	case IOV_SVAL(IOV_BMAC_RCVLAZY):
		/* Fix up the disable value if needed */
		if (int_val == 0) {
			int_val = IRL_DISABLE;
		}
		wlc_hw->intrcvlazy = (uint)int_val;
		if (wlc_hw->up) {
			wlc_bmac_rcvlazy_update(wlc_hw, wlc_hw->intrcvlazy);
		}
		break;

	case IOV_GVAL(IOV_BMAC_RCVLAZY):
		*ret_int_ptr = (int)wlc_hw->intrcvlazy;
		break;
#endif /* BCMDBG */

	case IOV_SVAL(IOV_BMAC_BTSWITCH):
		if ((int_val != OFF) && (int_val != ON) && (int_val != AUTO)) {
			return BCME_RANGE;
		}

		err = wlc_bmac_set_btswitch_ext(wlc_hw, (int8)int_val);

		break;

	case IOV_GVAL(IOV_BMAC_BTSWITCH):
		if (!WLCISACPHY(wlc_hw->band)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (!(wlc_hw->up)) {
			err = BCME_NOTUP;
			break;
		}

		/* read the phyreg to find the state of bt/wlan ovrd */
		wlc_hw->btswitch_ovrd_state = wlc_phy_get_femctrl_bt_wlan_ovrd(wlc_hw->band->pi);

		*ret_int_ptr = wlc_hw->btswitch_ovrd_state;
		break;

#ifdef BCMDBG
	case IOV_GVAL(IOV_BMAC_PCIESSID):
		*ret_int_ptr = si_pcie_get_ssid(wlc_hw->sih);
		break;

	case IOV_GVAL(IOV_BMAC_PCIEBAR0):
		*ret_int_ptr = si_pcie_get_bar0(wlc_hw->sih);
		break;
#endif /* BCMDBG */

	case IOV_SVAL(IOV_BMAC_4360_PCIE2_WAR):
		wlc_hw->vcoFreq_4360_pcie2_war = (uint)int_val;
		break;

	case IOV_GVAL(IOV_BMAC_4360_PCIE2_WAR):
		*ret_int_ptr = (int)wlc_hw->vcoFreq_4360_pcie2_war;
		break;

#if defined(SAVERESTORE) && defined(BCMDBG_SR)
	case IOV_GVAL(IOV_BMAC_SR_VERIFY): {
		struct bcmstrbuf b;
		bcm_binit(&b, (char *)arg, len);

		if (SR_ENAB()) {
			wlc_bmac_sr_verify(wlc_hw, &b);
		}
		break;
	}
	case IOV_SVAL(IOV_BMAC_SR_VERIFY): {
		if (SR_ENAB()) {
			wlc_bmac_sr_testmode(wlc_hw, int_val);
		}
		break;
	}
#endif /* defined(SAVERESTORE) && defined(BCMDBG_SR) */

	case IOV_SVAL(IOV_BMAC_MACFREQ):
	{
		wlc_bmac_switch_macfreq_dynamic(wlc_hw, (uint8)int_val);
		break;
	}
#ifdef GPIO_TXINHIBIT
	case IOV_GVAL(IOV_BMAC_TX_INHIBIT_TOUT):
		{
			*ret_int_ptr = (int) wlc_hw->tx_inhibit_tout;
			break;
		}
	case IOV_SVAL(IOV_BMAC_TX_INHIBIT_TOUT):
		{
			wlc_hw->tx_inhibit_tout = int_val;
			wlc_bmac_gpio_set_tx_inhibit_tout(wlc_hw);
			break;
		}
#endif // endif

#if defined(BCMDBG)
	case IOV_GVAL(IOV_BMAC_HDR_CONV):
	{
		if (!RXFIFO_SPLIT()) {
			err = BCME_UNSUPPORTED;
			break;
		}
		*ret_int_ptr = wlc_hw->hdrconv_mode;
		break;
	}
	case IOV_SVAL(IOV_BMAC_HDR_CONV):
	{
		wlc_info_t *wlc = wlc_hw->wlc;

		if (!RXFIFO_SPLIT()) {
			err = BCME_UNSUPPORTED;
			break;
		}
		/* Change the HW mode */
		wlc_update_splitrx_mode(wlc->hw, bool_val, FALSE);

		break;
	}
#endif /* BCMINTRENAL */

#ifdef LDO3P3_MIN_RES_MASK
	case IOV_SVAL(IOV_BMAC_LDO3P3_PROTECTION_OVERRIDE):
		err = si_pmu_min_res_ldo3p3_set(wlc_hw->sih, wlc_hw->osh, bool_val);
		break;
	case IOV_GVAL(IOV_BMAC_LDO3P3_PROTECTION_OVERRIDE):
		err = si_pmu_min_res_ldo3p3_get(wlc_hw->sih, wlc_hw->osh, ret_int_ptr);
		break;
#endif /* LDO3P3_MIN_RES_MASK */
	case IOV_GVAL(IOV_BMAC_OTP_PU):
			{
				*ret_int_ptr = (int)si_is_otp_powered(wlc_hw->sih);
				break;
			}
#ifndef WLTEST
	case IOV_SVAL(IOV_BMAC_OTP_PU):
			{
				err = si_pmu_min_res_otp_pu_set(wlc_hw->sih, wlc_hw->osh, bool_val);
				break;
			}
#endif /* !WLTEST */

#if defined(WLTEST)
	case IOV_GVAL(IOV_AXI):
	case IOV_SVAL(IOV_AXI):
	{
		bool read = (actionid == IOV_GVAL(IOV_AXI));
		uint32 axi_phys_addr = int_val;
		int n_args = read ? 1 : 2;

		if (p_len < n_args * sizeof(int_val)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		if ((axi_phys_addr & 0x3) != 0) {
			err = BCME_BADADDR;
			break;
		}

		/* axi_phys_addr is a physical address from the AXI backplane perspective,
		 * which is for some NIC builds unequal to the CPU perspective.
		 */
		if (read) {
			si_backplane_access(wlc_hw->sih, axi_phys_addr, sizeof(uint32),
				(uint32*)ret_int_ptr, TRUE);
		} else {
			si_backplane_access(wlc_hw->sih, axi_phys_addr, sizeof(uint32),
				(uint32*)&int_val2, FALSE);
		}

		break;
	}
#endif // endif

	default:
		WL_ERROR(("%s(): undefined BMAC IOVAR: %d\n", __FUNCTION__, actionid));
		err = BCME_NOTFOUND;
		break;

	}

	return err;
}

/* register iovar table/handlers to the system */
int
BCMATTACHFN(wlc_bmac_register_iovt)(wlc_hw_info_t *hw, wlc_iocv_info_t *ii)
{
	wlc_iovt_desc_t iovd;
#if defined(WLC_PATCH_IOCTL)
	wlc_iov_disp_fn_t disp_fn = IOV_PATCH_FN;
	const bcm_iovar_t* patch_table = IOV_PATCH_TBL;
#else
	wlc_iov_disp_fn_t disp_fn = NULL;
	bcm_iovar_t* patch_table = NULL;
#endif /* WLC_PATCH_IOCTL */

	ASSERT(ii != NULL);

	wlc_iocv_init_iovd(wlc_bmac_iovt,
		wlc_bmac_pack_iov, NULL,
		wlc_bmac_doiovar, disp_fn, patch_table, hw,
		&iovd);

	return wlc_iocv_register_iovt(ii, &iovd);
}
