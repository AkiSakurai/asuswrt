/*
 * PHY and RADIO specific portion of Broadcom BCM43XX 802.11abgn
 * Networking Device Driver.
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
 * $Id: wlc_phy_lcn20.c $*
 */

/* XXX WARNING: phy structure has been changed, read this first
 *
 * This submodule is for LCN20 phy only. It depends on the common submodule wlc_phy_cmn.c
 */

#include <wlc_cfg.h>

#if (defined(LCN20CONF) && (LCN20CONF != 0))
#include <typedefs.h>
#include <qmath.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wlc_phy_radio.h>
#include <bitfuncs.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <proto/802.11.h>
#include <hndpmu.h>
#include <bcmsrom_fmt.h>
#include <sbsprom.h>
#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>
#include <wlc_phy_lcn20.h>
#include <wlc_phyreg_lcn20.h>
#include <sbchipc.h>
#include <wlc_phyreg_lcn20.h>
#include <wlc_phytbl_lcn20.h>
#include <wlc_radioreg_20692.h>
#include <bcmotp.h>
#include <wlc_phy_shim.h>

static void wlc_phy_init_lcn20phy(phy_info_t *pi);
static void wlc_phy_cal_init_lcn20phy(phy_info_t *pi);
static void wlc_phy_detach_lcn20phy(phy_info_t *pi);
static void wlc_phy_chanspec_set_lcn20phy(phy_info_t *pi, chanspec_t chanspec);
static void wlc_phy_txpower_recalc_target_lcn20phy(phy_info_t *pi);

static void wlc_lcn20phy_set_tx_pwr_ctrl(phy_info_t *pi, uint16 mode);
static void wlc_lcn20phy_set_tx_pwr_by_index(phy_info_t *pi, int indx);

static void wlc_phy_set_tx_iqcc_lcn20phy(phy_info_t *pi, uint16 a, uint16 b);
static void wlc_phy_get_tx_iqcc_lcn20phy(phy_info_t *pi, uint16 *a, uint16 *b);
static uint16 wlc_phy_get_tx_locc_lcn20phy(phy_info_t *pi);
static void wlc_phy_set_tx_locc_lcn20phy(phy_info_t *pi, uint16 didq);
static void wlc_phy_get_radio_loft_lcn20phy(phy_info_t *pi, uint8 *ei0,
	uint8 *eq0, uint8 *fi0, uint8 *fq0);

#if defined(WLTEST)
static void wlc_phy_carrier_suppress_lcn20phy(phy_info_t *pi);
#endif // endif

#if defined(BCMDBG) || defined(WLTEST)
static int wlc_phy_long_train_lcn20phy(phy_info_t *pi, int channel);
#endif // endif
static void wlc_lcn20phy_anacore(phy_info_t *pi, bool on);
static void wlc_lcn20phy_switch_radio(phy_info_t *pi, bool on);
static void wlc_phy_watchdog_lcn20phy(phy_info_t *pi);
static void wlc_lcn20phy_btc_adjust(phy_info_t *pi, bool btactive);
static void wlc_lcn20phy_write_table(phy_info_t *pi, const phytbl_info_t *pti);
static void wlc_lcn20phy_read_table(phy_info_t *pi, phytbl_info_t *pti);
static void wlc_lcn20phy_calib_modes(phy_info_t *pi, uint mode);
static bool wlc_lcn20phy_txpwr_srom_read(phy_info_t *pi);
static void wlc_lcn20phy_rev0_reg_init(phy_info_t *pi);
static void wlc_lcn20phy_txpwrctrl_init(phy_info_t *pi);
static void wlc_lcn20phy_set_bbmult(phy_info_t *pi, uint8 m0);
static void wlc_lcn20phy_tx_pu(phy_info_t *pi, bool bEnable);
static void wlc_lcn20phy_set_tx_gain_override(phy_info_t *pi, bool bEnable);
static void wlc_lcn20phy_set_tx_gain(phy_info_t *pi,  phy_txgains_t *target_gains);
static int32 wlc_lcn20phy_tssi2dbm(int32 tssi, int32 a1, int32 b0, int32 b1);
static void wlc_lcn20phy_clear_tx_power_offsets(phy_info_t *pi);
static void wlc_lcn20phy_dccal_force_cal(phy_info_t *pi, uint8 dccal_mode, int8 num_retry);
static void wlc_lcn20phy_rx_iq_cal(phy_info_t *pi);
static void wlc_lcn20phy_tx_iqlo_cal_txpwr(phy_info_t *pi);

#if defined(LP_P2P_SOFTAP) || defined(WL_LPC)
#ifdef WL_LPC
static uint8 wlc_lcn20phy_lpc_getminidx(void);
static void wlc_lcn20phy_lpc_setmode(phy_info_t *pi, bool enable);
static uint8 wlc_lcn20phy_lpc_getoffset(uint8 index);
static uint8 wlc_lcn20phy_lpc_get_txcpwrval(uint16 phytxctrlword);
static void wlc_lcn20phy_lpc_set_txcpwrval(uint16 *phytxctrlword, uint8 txcpwrval);
#ifdef WL_LPC_DEBUG
static uint8 * wlc_lcn20phy_lpc_get_pwrlevelptr(void);
#endif // endif
#endif /* WL_LPC */
static void wlc_lcn20phy_lpc_write_maclut(phy_info_t *pi);
#endif /* LP_P2P_SOFTAP || WL_LPC */

/* Functions for fresh buffer access in wlc_phy_lcn20.c file */
#define LCN20PHY_MALLOC(pi, size) wlc_lcn20_malloc((pi)->u.pi_lcn20phy, size, __LINE__)
#define LCN20PHY_MFREE(pi, addr, size) wlc_lcn20_mfree((pi)->u.pi_lcn20phy, __LINE__)

static void *wlc_lcn20_malloc(phy_info_lcn20phy_t *pi_lcn20phy, uint16 size, uint32 line);
static void wlc_lcn20_mfree(phy_info_lcn20phy_t *pi_lcn20phy, uint32 line);

static void wlc_lcn20phy_radio20692_init(phy_info_t *pi);
static void wlc_lcn20phy_radio20692_rc_cal(phy_info_t *pi);
static void wlc_lcn20phy_radio20692_tia_config(phy_info_t *pi);
static void wlc_lcn20phy_radio20692_channel_tune(phy_info_t *pi, uint8 channel);
static void wlc_lcn20phy_radio20692_vcocal_isdone(phy_info_t *pi, bool set_delay);
static void wlc_lcn20phy_radio20692_vcocal(phy_info_t *pi);
static void wlc_lcn20phy_radio20692_tssisetup(phy_info_t *pi);
static void wlc_lcn20phy_radio20692_rx_iq_cal_setup(phy_info_t *pi);
static void wlc_lcn20phy_radio20692_rx_iq_cal_cleanup(phy_info_t *pi);

#define wlc_radio20692_rc_cal_done(pi)  \
	(0 != (READ_RADIO_REGFLD_20692(pi, WL_RCCAL_CFG3, 0, wl_rccal_DONE)))

#define XTAL_FREQ_37P4MHZ			37400000

/* Table ID's and offsets */
#define LCN20PHY_TBL_ID_IQLOCAL			0x00
#define LCN20PHY_TBL_ID_TXPWRCTL		0x07
#define LCN20PHY_TBL_ID_RFSEQ			0x08
#define LCN20PHY_TBL_ID_DCOE			0x22
#define LCN20PHY_TBL_ID_IDAC			0x23
#define LCN20PHY_TBL_ID_IDACGMAP		0x24
#define LCN20PHY_TBL_ID_RFSEQ			0x08
#define LCN20PHY_TBL_ID_DCOE			0x22
#define LCN20PHY_TBL_ID_IDAC			0x23
#define LCN20PHY_TBL_ID_IDACGMAP		0x24
#define LCN20PHY_TBL_ID_SAMPLEPLAY		0x15

/* TXIQLOcal tbl offsets */
#define LCN20PHY_TXIQLOCAL_IQCOEFF_OFFSET	64
#define LCN20PHY_TXIQLOCAL_DLOCOEFF_OFFSET	67
#define LCN20PHY_TXIQLOCAL_IQCOMP_OFFSET	96
#define LCN20PHY_TXIQLOCAL_DLOCOMP_OFFSET	98
#define LCN20PHY_TXIQLOCAL_BPHY_IQCOMP_OFFSET	112
#define LCN20PHY_TXIQLOCAL_BPHY_DLOCOMP_OFFSET	114
#define LCN20PHY_TXIQLOCAL_IQBESTCOEFF_OFFSET	128
#define LCN20PHY_TXIQLOCAL_DLOBESTCOEFF_OFFSET	131

/* Tx Pwr Ctrl table offsets */
#define LCN20PHY_TX_PWR_CTRL_RATE_OFFSET 	832
#define LCN20PHY_TX_PWR_CTRL_MAC_OFFSET 	128
#define LCN20PHY_TX_PWR_CTRL_GAIN_OFFSET 	192
#define LCN20PHY_TX_PWR_CTRL_IQ_OFFSET		320
#define LCN20PHY_TX_PWR_CTRL_LO_OFFSET		448
#define LCN20PHY_TX_PWR_CTRL_PWR_OFFSET		576
#define LCN20PHY_TX_PWR_CTRL_EST_PWR_OFFSET	704

#define LCN20PHY_TX_PWR_CTRL_START_NPT		0
#define LCN20PHY_TX_PWR_CTRL_START_INDEX_2G	90
#define BTC_POWER_CLAMP 20  /* 10dBm in 1/2dBm */
#define LCN20_TARGET_PWR  60
#define txpwrctrl_off(pi) (0x7 != ((phy_reg_read(pi, LCN20PHY_TxPwrCtrlCmd) & 0xE000) >> 13))

/* Vmid/Gain settings */
#define AUXPGA_VBAT_VMID_VAL 0x95
#define AUXPGA_VBAT_GAIN_VAL 0x2
#define AUXPGA_TEMPER_VMID_VAL 0xad
#define AUXPGA_TEMPER_GAIN_VAL 0x1

/* DCCAL modes 0-7: 3bits {idact,idacc,dcoe} */
#define LCN20PHY_DCCALMODE_BYPASS	0
#define LCN20PHY_DCCALMODE_DCOE		1
#define LCN20PHY_DCCALMODE_IDACC	2
#define LCN20PHY_DCCALMODE_IDACT	4
#define LCN20PHY_DCCALMODE_ALL	\
	(LCN20PHY_DCCALMODE_DCOE |\
	LCN20PHY_DCCALMODE_IDACC |\
	LCN20PHY_DCCALMODE_IDACT)

/* idacc_reinit_mode:
* disable reinits			0
* per-packet reinit			1
* tx2rx_only				4
* tx2rx_reinit+tx2rx_only	6
*/
#define LCN20PHY_IDACCMODE_DISREINIT	0
#define LCN20PHY_IDACCMODE_PPREINIT		1
#define LCN20PHY_IDACCMODE_TX2RX		4
#define LCN20PHY_IDACCMODE_REINITTX2RX	6

/* delay in usec */
#define LCN20PHY_SPINWAIT_DCCAL	40

#define LCN20PHY_BYPASS_MASK	\
	(LCN20PHY_phyreg2dccal_control_0_dcoe_bypass_MASK |\
	LCN20PHY_phyreg2dccal_control_0_idacc_bypass_MASK |\
	LCN20PHY_phyreg2dccal_control_0_idact_bypass_MASK)

#define LCN20PHY_IDACCMODE_MASK	\
	(LCN20PHY_phyreg2dccal_config_3_idacc_ppkt_reinit_MASK |\
	LCN20PHY_phyreg2dccal_config_3_idacc_tx2rx_reinit_MASK |\
	LCN20PHY_phyreg2dccal_config_3_idacc_tx2rx_only_MASK)

#define LCN20PHY_INITDCOE_MASK	\
	(LCN20PHY_phyreg2dccal_config_0_dcoe_acc_cexp_MASK |\
	LCN20PHY_phyreg2dccal_config_0_dcoe_wait_cinit_MASK |\
	LCN20PHY_phyreg2dccal_config_0_dcoe_lna1_inpshort_MASK |\
	LCN20PHY_phyreg2dccal_config_0_dcoe_lna1_outshort_MASK |\
	LCN20PHY_phyreg2dccal_config_0_dcoe_lna1_init_MASK |\
	LCN20PHY_phyreg2dccal_config_0_dcoe_zero_idac_MASK)

#define LCN20PHY_INITDCOE_VAL	\
	((4 << LCN20PHY_phyreg2dccal_config_0_dcoe_acc_cexp_SHIFT) |\
	(16 << LCN20PHY_phyreg2dccal_config_0_dcoe_wait_cinit_SHIFT) |\
	(0 << LCN20PHY_phyreg2dccal_config_0_dcoe_lna1_inpshort_SHIFT) |\
	(1 << LCN20PHY_phyreg2dccal_config_0_dcoe_lna1_outshort_SHIFT) |\
	(0 << LCN20PHY_phyreg2dccal_config_0_dcoe_lna1_init_SHIFT) |\
	(1 << LCN20PHY_phyreg2dccal_config_0_dcoe_zero_idac_SHIFT))

#define LCN20PHY_INITIDACC_MASK	\
	(LCN20PHY_phyreg2dccal_config_2_idacc_acc_cexp_MASK |\
	LCN20PHY_phyreg2dccal_config_2_idacc_wait_cinit_MASK |\
	LCN20PHY_phyreg2dccal_config_2_idacc_mag_select_MASK |\
	LCN20PHY_phyreg2dccal_config_2_idac_lna1_refidx_MASK |\
	LCN20PHY_phyreg2dccal_config_2_idac_lna1_scaling_MASK)

#define LCN20PHY_INITIDACC_VAL	\
	((4 << LCN20PHY_phyreg2dccal_config_2_idacc_acc_cexp_SHIFT) |\
	(16 << LCN20PHY_phyreg2dccal_config_2_idacc_wait_cinit_SHIFT) |\
	(2 << LCN20PHY_phyreg2dccal_config_2_idacc_mag_select_SHIFT) |\
	(5 << LCN20PHY_phyreg2dccal_config_2_idac_lna1_refidx_SHIFT) |\
	(0 << LCN20PHY_phyreg2dccal_config_2_idac_lna1_scaling_SHIFT))

#define LCN20PHY_NUM_DIG_FILT_COEFFS 17
#define LCN20PHY_NUM_TX_DIG_FILTERS_CCK 17
/* filter id, followed by coefficients */
uint16 LCN20PHY_txdigfiltcoeffs_cck[LCN20PHY_NUM_TX_DIG_FILTERS_CCK]
	[1+LCN20PHY_NUM_DIG_FILT_COEFFS] = {
	{ 0, 1, 0x19f, 0xff52, 0x40, 0x80, 0x40, 0x318, 0xfe78, 0x40, 0x80, 0x40, 0x30a,
	0xfe2e, 0x40, 0x80, 0x40, 8},
	{ 1, 1, 0x192, 0xff37, 0x29f, 0xff02, 0x260, 0xff47, 0x103, 0x3b, 0x103, 0x44, 0x36,
	0x44, 0x5d, 0xa7, 0x5d, 8},
	{ 2, 1, 415, 1874, 64, 128, 64, 792, 1656, 192, 384, 192, 778, 1582, 64, 128, 64, 8},
	{ 3, 1, 302, 1841, 129, 258, 129, 658, 1720, 205, 410, 205, 754, 1760, 170, 340, 170, 8},
	{ 20, 1, 360, -164, 242, -314, 242, 752, -328, 205, -203, 205, 767, -288, 253, 183, 253, 8},
	{ 21, 1, 360, 1884, 149, 1874, 149, 752, 1720, 205, 1884, 205, 767, 1760, 256, 273, 256, 8},
	{ 22, 1, 360, 1884, 98, 1948, 98, 752, 1720, 205, 1924, 205, 767, 1760, 256, 352, 256, 8},
	{ 23, 1, 350, 1884, 116, 1966, 116, 752, 1720, 205, 2008, 205, 767, 1760, 129, 235, 129, 8},
	{ 24, 1, 325, 1884, 32, 40, 32, 756, 1720, 256, 471, 256, 766, 1760, 262, 1878, 262, 8},
	{ 25, 1, 299, 1884, 51, 64, 51, 736, 1720, 256, 471, 256, 765, 1760, 262, 1878, 262, 8},
	{ 26, 1, 277, 1943, 39, 117, 88, 637, 1838, 64, 192, 144, 614, 1864, 128, 384, 288, 8},
	{ 27, 1, 245, 1943, 49, 147, 110, 626, 1838, 162, 485, 363, 613, 1864, 62, 186, 139, 8},
	{ 28, 1, 360, 1884, 149, 1874, 149, 752, 1720, 205, 1884, 205, 767, 1760, 114, 121, 114, 8},
	{ 30, 1, 302, 1841, 61, 122, 61, 658, 1720, 205, 410, 205, 754, 1760, 170, 340, 170, 8},
	{ 31, 1, 319, 1817, 490, 0, 490, 699, 1678, 324, 0, 324, 754, 1760, 114, 0, 114, 8},
	{ 40, 1, 360, 1884, 242, 1734, 242, 752, 1720, 205, 1845, 205, 767, 1760, 511, 370, 511, 8},
	{ 50, 1, 0x1d9, 0xff0c, 0x20, 0x40, 0x20, 0x3a2, 0xfe41, 0x10, 0x20, 0x10, 0x3a1,
	0xfe58, 0x10, 0x20, 0x10, 8}
	};

#define LCN20PHY_NUM_TX_DIG_FILTERS_OFDM 9
uint16 LCN20PHY_txdigfiltcoeffs_ofdm[LCN20PHY_NUM_TX_DIG_FILTERS_OFDM]
	[1+LCN20PHY_NUM_DIG_FILT_COEFFS] = {
	{ 0, 0, 0xa2, 0, 0x100, 0x100, 0x0, 0x0, 0x0, 0x100, 0x0, 0x0,
	0x278, 0xfea0, 152, 304, 152, 8},
	{ 1, 0, 374, -135, 16, 32, 16, 799, -396, 50, 32, 50,
	0x750, -469, 212, -50, 212, 8},
	{ 2, 0, 375, -234, 37, 76, 37, 799, -396, 32, 20, 32,
	748, -270, 128, -30, 128, 8},
	{3, 0, 375, 0xFF16, 37, 76, 37, 799, 0xFE74, 32, 20, 32, 748,
	0xFEF2, 148, 0xFFDD, 148, 8},
	{4, 0, 307, 1966, 53, 106, 53, 779, 1669, 53, 2038, 53, 765,
	1579, 212, 1846, 212, 8},
	{5, 0, 0x1c5, 0xff1d, 0x20, 0x40, 0x20, 0, 0, 0x100, 0, 0, 0x36b,
	0xfe82, 0x14, 0x29, 0x14, 8},
	{ 6, 0, 375, -234, 37, 76, 37, 799, -396, 32, 20, 32,
	748, -270, 114, -27, 114, 8},
	{10, 0, 0xa2, 0, 0x100, 0x100, 0x0, 0, 0, 511, 0, 0, 0x278,
	0xfea0, 256, 511, 256, 8},
	{12, 0, 394, -234, 29, 58, 29, 800, -394, 24, 48, 24, 836,
	-352, 38, 76, 38, 8}
	};

#define  LCN20PHY_MAX_CCK_DB_SCALING 7
#define  LCN20PHY_DB2SCALEFCTR_SHIFT 6
/* The scale factors are based on the below formula
	round ((pow(10, (-db)/20.0)) << DB2SCALEFCTR_SHIFT)
*/
uint8 LCN20PHY_db2scalefctr_cck[LCN20PHY_MAX_CCK_DB_SCALING] =
	{57, 51, 45, 40, 36, 32, 29};

/* RX farrow defines and macros */
#define LCN20PHY_RXFAR_BITSINMU	24
#define LCN20PHY_RXFAR_NUM		3
#define LCN20PHY_RXFAR_DEN		2
#define LCN20PHY_RXFAR_BW		20
#define LCN20PHY_RXFAR_M		8
#define LCN20PHY_RXFAR_VCODIV	6

/* den * vco_div * M * 2 * bw / num */
#define LCN20PHY_RXFAR_FACTOR	\
	((LCN20PHY_RXFAR_DEN) *\
	(LCN20PHY_RXFAR_VCODIV) *\
	(LCN20PHY_RXFAR_M) *\
	(2 * LCN20PHY_RXFAR_BW))/LCN20PHY_RXFAR_NUM

#define LCN20PHY_TX_FARROW_RATIO	1920
static const
uint32 mu_deltaLUT[14] = {
	7533053,
	7526147,
	7519268,
	7512418,
	7505597,
	7498803,
	7492037,
	7485299,
	7478588,
	7471904,
	7465248,
	7458618,
	7452016,
	7436278
};

/* TR switch modes */
#define LCN20PHY_TRS_TXMODE	0
#define LCN20PHY_TRS_RXMODE	1
#define LCN20PHY_TRS_MUTE	2
#define LCN20PHY_TRS_TXRXMODE	3

/* ------ definitions abd tables for RXIQ calibrations ----- */

/* setmode: 0 - fetch values from phyregs into *pcomp
 *		  1 - deposit values from *pcomp into phyregs
 *		  2 - set all rxiq coeffs to 0
 *
 * pcomp: input/output comp buffer
 */
#define LCN20PHY_RXIQCOMP_GET	0
#define LCN20PHY_RXIQCOMP_SET	1
#define LCN20PHY_RXIQCOMP_RESET	2

#define LCN20PHY_RXIQCAL_TONEFREQKHZ_0		2000
#define LCN20PHY_RXIQCAL_TONEFREQKHZ_1		7000
#define LCN20PHY_RXIQCAL_MAXNUM_FREQS	4
#define LCN20PHY_RXIQCAL_TONEAMP		181
#define LCN20PHY_RXIQCAL_NUMSAMPS		0x4000

#define LCN20PHY_RXCAL_NUMRXGAINS		16

#define LCN20PHY_RXIQCAL_LEAKAGEPATH	1
#define LCN20PHY_RXIQCAL_PAPDPATH		2

#define LCN20PHY_SPINWAIT_IQEST_QT_USEC		1000*1000
#define LCN20PHY_SPINWAIT_IQEST_USEC		100*1000

/* Gain Candidates For leakage path :  LNA, TIA, Farrow
* LNA, TIA, Farrow
* Leakage path works only for 5g
*/
static const lcn20phy_rxcal_rxgain_t
gaintbl_lkpath[LCN20PHY_RXCAL_NUMRXGAINS] = {
	{ -4, 0, 2, 0 },
	{ -4, 0, 1, 0 },
	{ -4, 0, 0, 0 },
	{ -3, 0, 0, 0 },
	{ -2, 0, 0, 0 },
	{ -1, 0, 0, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 2, 0, 0 },
	{ 0, 3, 0, 0 },
	{ 0, 4, 0, 0 },
	{ 0, 5, 0, 0 },
	{ 0, 6, 0, 0 },
	{ 0, 7, 0, 0 },
	{ 0, 8, 0, 0 },
	{ 0, 9, 0, 0 }
};
/* Gain Candidates For papd loopback  : Tia ,Farrow & dvga2 */
static const lcn20phy_rxcal_rxgain_t
gaintbl_papdpath[LCN20PHY_RXCAL_NUMRXGAINS] = {
	{ 0, 1, 1, 0 },
	{ 0, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 2, 0, 0 },
	{ 0, 3, 0, 0 },
	{ 0, 4, 0, 0 },
	{ 0, 5, 0, 0 },
	{ 0, 6, 0, 0 },
	{ 0, 7, 0, 0 },
	{ 0, 8, 0, 0 },
	{ 0, 9, 0, 0 },
	{ 0, 10, 0, 0 },
	{ 0, 10, 0, 1 },
	{ 0, 10, 0, 2 },
	{ 0, 10, 0, 3 },
	{ 0, 10, 0, 4 }
};

/* ------ definitions and tables for TXIQLO calibrations ----- */

/* restart : start from zero coeffs
* refine : from previous results
* dlo_recal : do only digital LO cal
*/
typedef enum {
	TXIQLOCAL_RESTART,
	TXIQLOCAL_REFINE,
	TXIQLOCAL_DLORECAL
} phy_txiqlocal_mode_t;

/* Reference Gain "Ladders" (bbmult is normalized to 1.0 here for later scaling) */
static const
lcn20phy_txiqcal_ladder_t lcn20phy_txiqlocal_ladder_lo[] = {
{3, 0}, {4, 0}, {6, 0}, {9, 0}, {13, 0}, {18, 0},
{25, 0}, {25, 1}, {25, 2}, {25, 3}, {25, 4}, {25, 5},
{25, 6}, {25, 7}, {35, 7}, {50, 7}, {71, 7}, {100, 7}};
static const
lcn20phy_txiqcal_ladder_t lcn20phy_txiqlocal_ladder_iq[] = {
{3, 0}, {4, 0}, {6, 0}, {9, 0}, {13, 0}, {18, 0},
{25, 0}, {35, 0}, {50, 0}, {71, 0}, {100, 0}, {100, 1},
{100, 2}, {1, 3}, {100, 4}, {100, 5}, {1, 6}, {100, 7}};

#define LCN20PHY_TXIQLOCAL_THRSLAD	0x3d
/* gain control segments */
#define LCN20PHY_TXIQLOCAL_NSGCTRL	0x76
/* correlation segments */
#define LCN20PHY_TXIQLOCAL_NSCORRS	0x79
#define LCN20PHY_TXIQLOCAL_CMDNUM	\
	(LCN20PHY_TXIQLOCAL_NSCORRS << 8) |\
	(LCN20PHY_TXIQLOCAL_NSGCTRL)

/* 1 = requesting iqcal mode of sample play buffer */
#define LCN20PHY_TXIQLOCAL_SPBMODE	1

#define LCN20PHY_TXIQLOCAL_TONEFHZ	(2 * 1000 * 1000)
#define LCN20PHY_TXIQLOCAL_TONEAMP	250

#define LCN20PHY_SPINWAIT_TXIQLOCAL_USEC	100*1000

#define LCN20PHY_CAL_TYPE_IQ                 0
#define LCN20PHY_CAL_TYPE_LOFT_DIG           2

/* ------ definitions and tables for RCCAL ----- */

/* Coefficients generated by 47xxtcl/rgphy/20691/ */
/* lpf_tx_coefficient_generator/filter_tx_tiny_generate_python_and_tcl.py */
static const uint16 lpf_g10[6][15] = {
	{1188, 1527, 1866, 2206, 2545, 2545, 1188, 1188,
	1188, 1188, 1188, 1188, 1188, 1188, 1188},
	{3300, 4242, 5185, 6128, 7071, 7071, 3300, 3300,
	3300, 3300, 3300, 3300, 3300, 3300, 3300},
	{16059, 16059, 16059, 17294, 18529, 18529, 9882,
	1976, 2470, 3088, 3953, 4941, 6176, 7906, 12353},
	{24088, 24088, 25941, 31500, 37059, 37059, 14823,
	2964, 3705, 4632, 5929, 7411, 9264, 11859, 18529},
	{29647, 32118, 34589, 42001, 49412, 49412, 19765,
	3705, 4941, 6176, 7411, 9882, 12353, 14823, 24706},
	{32941, 36236, 39530, 42824, 46118, 49412, 19765,
	4117, 4941, 6588, 8235, 9882, 13176, 16470, 26353}
};
static const uint16 lpf_g12[6][15] = {
	{1882, 1922, 1866, 1752, 1606, 1275, 2984, 14956,
	11880, 9436, 7495, 5954, 4729, 3756, 2370},
	{5230, 5341, 5185, 4868, 4461, 3544, 8289, 41544,
	33000, 26212, 20821, 16539, 13137, 10435, 6584},
	{24872, 19757, 15693, 13424, 11425, 9075, 24258, 24316,
	24144, 23972, 24374, 24201, 24029, 24432, 24086},
	{37309, 29635, 25351, 24452, 22850, 18151, 36388, 36474,
	36216, 35959, 36561, 36302, 36044, 36648, 36130},
	{44360, 38172, 32654, 31496, 29433, 23379, 46870, 44045,
	46648, 46318, 44150, 46759, 46428, 44254, 46538},
	{49288, 43066, 37319, 32113, 27471, 23379, 46870, 48939,
	46648, 49406, 49055, 46759, 49523, 49172, 49640}
};
static const uint16 lpf_g21[6][15] = {
	{1529, 1497, 1542, 1643, 1793, 2257, 965, 192, 242,
	305, 384, 483, 609, 766, 1215},
	{4249, 4160, 4285, 4565, 4981, 6270, 2681, 534, 673,
	847, 1067, 1343, 1691, 2129, 3375},
	{6135, 7723, 9723, 11367, 13356, 16814, 6290, 6275,
	6320, 6365, 6260, 6305, 6350, 6245, 6335},
	{9202, 11585, 13543, 14041, 15025, 18916, 9435, 9413,
	9480, 9548, 9391, 9458, 9525, 9368, 9503},
	{13760, 15990, 18693, 19380, 20738, 26108, 13023, 13858,
	13085, 13178, 13825, 13054, 13147, 13793, 13116},
	{22016, 25197, 29078, 33791, 39502, 46415, 23152, 22173,
	23262, 21964, 22121, 23207, 21912, 22068, 21860}
};
static const uint16 lpf_g11[6] = {994, 2763, 12353, 18529, 17470, 23293};
static const uint16 g_passive_rc_tx[6] = {62, 172, 772, 1158, 1544, 2058};
static const uint16 biases[6] = {24, 48, 96, 96, 128, 128};
static const int8 g_index1[15] = {0, 1, 2, 3, 4, 5, -2, -9, -8, -7, -6, -5, -4, -3, -1};

#define LCN20PHY_MAX_2069_RCCAL_WAITLOOPS 100
#define LCN20PHY_NUM_2069_RCCAL_CAPS 3

/* ------ definitions and tables for Radio tuning ----- */

/* AUTO-GENERATED (by gen_tune_20692, called from tunedb2tcl_20692.sh) */
static const chan_info_20692_lcn20phy_t chan_info_20692_rev1_lcn20phy_26MHz[] = {
	{
	1,    2412,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x096c,  0x0000,  0x0000,  0x0459,  0x3b13,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612e,  0x0011,  0x6008,
	0x403f
	},
	{
	2,    2417,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x0971,  0x0000,  0x0000,  0x045b,  0x89d8,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612e,  0x0011,  0x6008,
	0x403f
	},
	{
	3,    2422,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x0976,  0x0000,  0x0000,  0x045d,  0xd89d,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	4,    2427,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x097b,  0x0000,  0x0000,  0x0460,  0x2762,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	5,    2432,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x0980,  0x0000,  0x0000,  0x0462,  0x7627,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	6,    2437,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x0985,  0x0000,  0x0000,  0x0464,  0xc4ec,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	7,    2442,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x098a,  0x0000,  0x0000,  0x0467,  0x13b1,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	8,    2447,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x098f,  0x0000,  0x0000,  0x0469,  0x6276,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5138,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	9,    2452,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x0994,  0x0000,  0x0000,  0x046b,  0xb13b,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5139,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	10,    2457,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x0999,  0x0000,  0x0000,  0x046e,  0x0000,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5139,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	11,    2462,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x099e,  0x0000,  0x0000,  0x0470,  0x4ec4,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5139,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	12,    2467,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x09a3,  0x0000,  0x0000,  0x0472,  0x9d89,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5139,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	13,    2472,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x09a8,  0x0000,  0x0000,  0x0474,  0xec4e,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5139,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	14,    2484,  0x0034,  0x0104,  0x000d,  0x3023,  0x004f,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x001a,  0x0000,  0x09a2,
	0x0000,  0x09b4,  0x0000,  0x0000,  0x047a,  0x7627,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5139,  0x6130,  0x0011,  0x6008,
	0x403f
	}
};

static const chan_info_20692_lcn20phy_t chan_info_20692_rev1_lcn20phy_37p4MHz[] = {
	{
	1,    2412,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x096c,  0x0000,  0x0000,  0x0305,  0xe75b,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612e,  0x0011,  0x6008,
	0x403f
	},
	{
	2,    2417,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x0971,  0x0000,  0x0000,  0x0307,  0x820d,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612e,  0x0011,  0x6008,
	0x403f
	},
	{
	3,    2422,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x0976,  0x0000,  0x0000,  0x0309,  0x1cbf,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	4,    2427,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x097b,  0x0000,  0x0000,  0x030a,  0xb771,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	5,    2432,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x0980,  0x0000,  0x0000,  0x030c,  0x5223,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	6,    2437,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x0985,  0x0000,  0x0000,  0x030d,  0xecd5,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	7,    2442,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x098a,  0x0000,  0x0000,  0x030f,  0x8787,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	8,    2447,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x098f,  0x0000,  0x0000,  0x0311,  0x2239,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	9,    2452,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x0994,  0x0000,  0x0000,  0x0312,  0xbceb,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	10,    2457,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x0999,  0x0000,  0x0000,  0x0314,  0x579d,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	11,    2462,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x099e,  0x0000,  0x0000,  0x0315,  0xf24f,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5127,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	12,    2467,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x09a3,  0x0000,  0x0000,  0x0317,  0x8d01,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5128,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	13,    2472,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x09a8,  0x0000,  0x0000,  0x0319,  0x27b3,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5128,  0x612f,  0x0011,  0x6008,
	0x403f
	},
	{
	14,    2484,  0x004b,  0x0176,  0x0013,  0x3032,  0x0016,  0x0002,
	0xf001,  0x08a8,  0x05d8,  0x03d9,  0x0027,  0x0025,  0x0000,  0x0991,
	0x0000,  0x09b4,  0x0000,  0x0000,  0x031d,  0x015e,  0x0008,  0x583f,
	0x1212,  0x142f,  0x1f1f,  0x1f1f,  0x5128,  0x6130,  0x0011,  0x6008,
	0x403f
	}
};

/* TIA LUT tables */
#define TIA_LUT_20692_LEN 109

static const uint8 tiaRC_20692_8b_20[]= { /* LUT 0--51 (20 MHz) */
	0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff,
	0xb7, 0xb5, 0x97, 0x81, 0xe5, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x14, 0x1d, 0x28, 0x34, 0x34, 0x34,
	0x34, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40,
	0x5b, 0x6c, 0x80, 0x80
};

static const uint16 tiaRC_20692_16b_20[]= { /* LUT 52--108 (20 MHz) */
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00b5, 0x0080, 0x005a,
	0x0040, 0x002d, 0x0020, 0x0017, 0x0000, 0x0100, 0x00b5, 0x0080,
	0x005b, 0x0040, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0080, 0x001f, 0x1fe0, 0x7fff, 0x0001, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000
};

/* ------- PHY register read/write/modfiy macros ----- */
/* radio-specific macros */
#define RADIO_REG_2069X(pi, id, regnm, core)	RF##core##_##id##_##regnm(pi->pubpi.radiorev)
#define RADIO_REG_20691(pi, regnm, core)	RADIO_REG_2069X(pi, 20691, regnm, 0)
#define RADIO_REG_20692(pi, regnm, core)	RADIO_REG_2069X(pi, 20692, regnm, 0)
#define RADIO_REG_20693(pi, regnm, core)	RADIO_REG_2069X(pi, 20693, regnm, 0)

#define _READ_RADIO_REG(pi, reg)		read_radio_reg(pi, reg)

#define READ_RADIO_REG(pi, regpfx, regnm) \
	_READ_RADIO_REG(pi, regpfx##_2069_##regnm)

#define READ_RADIO_REG_20692(pi, regnm, core) \
	_READ_RADIO_REG(pi, RADIO_REG_20692(pi, regnm, core))

#define READ_RADIO_REGFLD(pi, regpfx, regnm, fldname) \
	((_READ_RADIO_REG(pi, regpfx##_2069_##regnm) & \
	              RF_2069_##regnm##_##fldname##_MASK) \
	              >> RF_2069_##regnm##_##fldname##_SHIFT)

#define READ_RADIO_REGFLD_20692(pi, regnm, core, fldname) \
	((_READ_RADIO_REG(pi, RADIO_REG_20692(pi, regnm, core)) & \
		RF_20692_##regnm##_##fldname##_MASK(pi->pubpi.radiorev)) \
		>> RF_20692_##regnm##_##fldname##_SHIFT(pi->pubpi.radiorev))

#define RADIO_REG(pi, regnm, core)	\
	((RADIOID((pi)->pubpi.radioid) == BCM20691_ID) \
		? RADIO_REG_20691(pi, regnm, core) : \
	 (RADIOID((pi)->pubpi.radioid) == BCM20692_ID) \
		? RADIO_REG_20692(pi, regnm, core) : \
	 (RADIOID((pi)->pubpi.radioid) == BCM20693_ID) \
		? RADIO_REG_20693(pi, regnm, core) : INVALID_ADDRESS)

#define _WRITE_RADIO_REG(pi, reg, val)		write_radio_reg(pi, reg, val)

#define WRITE_RADIO_REG_20692(pi, regnm, core, val) \
	_WRITE_RADIO_REG(pi, RADIO_REG_20692(pi, regnm, core), val)

#define _MOD_RADIO_REG(pi, reg, mask, val)	mod_radio_reg(pi, reg, mask, val)

#define MOD_RADIO_REG(pi, regpfx, regnm, fldname, value) \
	_MOD_RADIO_REG(pi, \
		regpfx##_2069_##regnm, \
		RF_2069_##regnm##_##fldname##_MASK, \
		((value) << RF_2069_##regnm##_##fldname##_SHIFT))

#define MOD_RADIO_REG_2069X(pi, id, regnm, core, fldname, value) \
		_MOD_RADIO_REG(pi, \
			RADIO_REG_##id(pi, regnm, core), \
			RF_##id##_##regnm##_##fldname##_MASK(pi->pubpi.radiorev), \
			((value) << RF_##id##_##regnm##_##fldname##_SHIFT(pi->pubpi.radiorev)))

#define MOD_RADIO_REG_20692(pi, regnm, core, fldname, value) \
		MOD_RADIO_REG_2069X(pi, 20692, regnm, core, fldname, value)

/* phyreg-specific macros */
#define _PHY_REG_READ(pi, reg)			phy_reg_read(pi, reg)

#define READ_LCN20PHYREG(pi, reg) \
	_PHY_REG_READ(pi, LCN20PHY_##reg)

#define READ_LCN20PHYREGFLD(pi, reg, field)				\
	((READ_LCN20PHYREG(pi, reg)					\
	 & LCN20PHY_##reg##_##field##_##MASK) >>	\
	 LCN20PHY_##reg##_##field##_##SHIFT)

#define _PHY_REG_MOD(pi, reg, mask, val)	phy_reg_mod(pi, reg, mask, val)

#define MOD_LCN20PHYREG(pi, reg, field, value)				\
	_PHY_REG_MOD(pi, LCN20PHY_##reg,		\
		LCN20PHY_##reg##_##field##_MASK,	\
		((value) << LCN20PHY_##reg##_##field##_##SHIFT))

#define _PHY_REG_WRITE(pi, reg, val)		phy_reg_write(pi, reg, val)

#define WRITE_LCN20PHYREG(pi, reg, val) \
	_PHY_REG_WRITE(pi, LCN20PHY_##reg, val)

/* Table read/write macros */
#define wlc_lcn20phy_common_read_table(pi, tbl_id, tbl_ptr, tbl_len, tbl_width, tbl_offset) \
	wlc_phy_common_read_table(pi, tbl_id, tbl_ptr, tbl_len, tbl_width, tbl_offset, \
	wlc_lcn20phy_read_table)

#define wlc_lcn20phy_common_write_table(pi, tbl_id, tbl_ptr, tbl_len, tbl_width, tbl_offset) \
	wlc_phy_common_write_table(pi, tbl_id, tbl_ptr, tbl_len, tbl_width, tbl_offset, \
	wlc_lcn20phy_write_table)

/* Power control macros */
#define LCN20PHY_DACGAIN_MASK	\
	(0xf << 7)
#define LCN20PHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(LCN20PHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT + 8)
#define LCN20PHY_txgainctrlovrval1_pagain_ovr_val1_MASK \
	(0xff << LCN20PHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define wlc_lcn20phy_set_start_tx_pwr_idx(pi, idx) \
	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlCmd, \
		LCN20PHY_TxPwrCtrlCmd_pwrIndex_init_MASK, \
		(uint16)(idx*2) << LCN20PHY_TxPwrCtrlCmd_pwrIndex_init_SHIFT)

#define wlc_lcn20phy_set_start_CCK_tx_pwr_idx(pi, idx) \
		phy_reg_mod(pi, LCN20PHY_TxPwrCtrlCmdCCK, \
			LCN20PHY_TxPwrCtrlCmdCCK_pwrIndex_init_cck_MASK, \
			(uint16)(idx*2) << LCN20PHY_TxPwrCtrlCmdCCK_pwrIndex_init_cck_SHIFT)

#define wlc_lcn20phy_set_tx_pwr_npt(pi, npt) \
	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlNnum, \
		LCN20PHY_TxPwrCtrlNnum_Npt_intg_log2_MASK, \
		(uint16)(npt) << LCN20PHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

#define wlc_lcn20phy_get_tx_pwr_ctrl(pi) \
	(phy_reg_read((pi), LCN20PHY_TxPwrCtrlCmd) & \
			(LCN20PHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK | \
			LCN20PHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK | \
			LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK))

#define wlc_lcn20phy_get_tx_pwr_npt(pi) \
	((phy_reg_read(pi, LCN20PHY_TxPwrCtrlNnum) & \
		LCN20PHY_TxPwrCtrlNnum_Npt_intg_log2_MASK) >> \
		LCN20PHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

/* the bitsize of the register is 9 bits for lcn20phy */

#define wlc_lcn20phy_get_current_tx_pwr_idx_if_pwrctrl_on(pi) \
	(phy_reg_read(pi, LCN20PHY_TxPwrCtrlStatusExt) & 0x1ff)

#define wlc_lcn20phy_get_target_tx_pwr(pi) \
	((phy_reg_read(pi, LCN20PHY_TxPwrCtrlTargetPwr) & \
		LCN20PHY_TxPwrCtrlTargetPwr_targetPwr0_MASK) >> \
		LCN20PHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

#define wlc_lcn20phy_set_target_tx_pwr(pi, target) \
	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlTargetPwr, \
		LCN20PHY_TxPwrCtrlTargetPwr_targetPwr0_MASK, \
		(uint16)MAX(pi->u.pi_lcn20phy->tssi_minpwr_limit, \
		(MIN(pi->u.pi_lcn20phy->tssi_maxpwr_limit, \
		(uint16)(target)))) << LCN20PHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

#define wlc_lcn20phy_enable_tx_gain_override(pi) \
	wlc_lcn20phy_set_tx_gain_override(pi, TRUE)
#define wlc_lcn20phy_disable_tx_gain_override(pi) \
	wlc_lcn20phy_set_tx_gain_override(pi, FALSE)

#define wlc_lcn20phy_tx_gain_override_enabled(pi) \
	(0 != (phy_reg_read((pi), LCN20PHY_AfeCtrlOvr) & LCN20PHY_AfeCtrlOvr_dacattctrl_ovr_MASK))

#define LCN20PHY_DISABLE_STALL(pi)	\
	PHY_REG_MOD(pi, LCN20PHY, RxFeCtrl1, disable_stalls, 1)

#define LCN20PHY_ENABLE_STALL(pi, stall_val) \
	PHY_REG_MOD(pi, LCN20PHY, RxFeCtrl1, disable_stalls, stall_val)

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  Function Implementation */
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

/* ATTACH */
bool
BCMATTACHFN(wlc_phy_attach_lcn20phy)(phy_info_t *pi)
{
	/* phy_info_lcn20phy_t *pi_lcn20; */

	pi->u.pi_lcn20phy = (phy_info_lcn20phy_t*)MALLOC(pi->sh->osh, sizeof(phy_info_lcn20phy_t));
	if (pi->u.pi_lcn20phy == NULL) {
	PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	bzero((char *)pi->u.pi_lcn20phy, sizeof(phy_info_lcn20phy_t));

	/* pi_lcn20 = pi->u.pi_lcn20phy; */

#if defined(PHYCAL_CACHING)
	/* Reset the var as no cal cache context should exist yet */
	pi->phy_calcache_num = 0;
#endif // endif

	if (!NORADIO_ENAB(pi->pubpi)) {
		pi->hwpwrctrl = TRUE;
		pi->hwpwrctrl_capable = TRUE;
	}

	/* Get xtal frequency from PMU */
	pi->xtalfreq = si_alp_clock(pi->sh->sih);
	ASSERT((pi->xtalfreq % 1000) == 0);

	PHY_INFORM(("wl%d: %s: using %d.%d MHz xtalfreq for RF PLL\n",
		pi->sh->unit, __FUNCTION__,
		pi->xtalfreq / 1000000, pi->xtalfreq % 1000000));

	pi->pi_fptr.init = wlc_phy_init_lcn20phy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_lcn20phy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_lcn20phy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_recalc_target_lcn20phy;
#if defined(BCMDBG) || defined(WLTEST)
	pi->pi_fptr.longtrn = wlc_phy_long_train_lcn20phy;
#endif // endif
	pi->pi_fptr.txiqccget = wlc_phy_get_tx_iqcc_lcn20phy;
	pi->pi_fptr.txiqccset = wlc_phy_set_tx_iqcc_lcn20phy;
	pi->pi_fptr.txloccget = wlc_phy_get_tx_locc_lcn20phy;
	pi->pi_fptr.txloccset = wlc_phy_set_tx_locc_lcn20phy;
	pi->pi_fptr.radioloftget = wlc_phy_get_radio_loft_lcn20phy;
	pi->pi_fptr.phywatchdog = wlc_phy_watchdog_lcn20phy;
	pi->pi_fptr.phybtcadjust = wlc_lcn20phy_btc_adjust;
#if defined(WLTEST)
	pi->pi_fptr.carrsuppr = wlc_phy_carrier_suppress_lcn20phy;
#endif // endif
	pi->pi_fptr.anacore = wlc_lcn20phy_anacore;
	pi->pi_fptr.switchradio = wlc_lcn20phy_switch_radio;
	pi->pi_fptr.phywritetable = wlc_lcn20phy_write_table;
	pi->pi_fptr.phyreadtable = wlc_lcn20phy_read_table;
	pi->pi_fptr.calibmodes = wlc_lcn20phy_calib_modes;
	pi->pi_fptr.detach = wlc_phy_detach_lcn20phy;

#ifdef WL_LPC
		pi->pi_fptr.lpcgetminidx = wlc_lcn20phy_lpc_getminidx;
		pi->pi_fptr.lpcgetpwros = wlc_lcn20phy_lpc_getoffset;
		pi->pi_fptr.lpcgettxcpwrval = wlc_lcn20phy_lpc_get_txcpwrval;
		pi->pi_fptr.lpcsettxcpwrval = wlc_lcn20phy_lpc_set_txcpwrval;
		pi->pi_fptr.lpcsetmode = wlc_lcn20phy_lpc_setmode;
#ifdef WL_LPC_DEBUG
		pi->pi_fptr.lpcgetpwrlevelptr = wlc_lcn20phy_lpc_get_pwrlevelptr;
#endif // endif
#endif /* WL_LPC */

	if (!wlc_lcn20phy_txpwr_srom_read(pi))
		return FALSE;

#if defined(WLTEST)
	pi->pi_fptr.swctrlmapsetptr = NULL;
	pi->pi_fptr.swctrlmapgetptr = NULL;
#endif // endif

	return TRUE;
}

static void
BCMATTACHFN(wlc_phy_detach_lcn20phy)(phy_info_t *pi)
{
	MFREE(pi->sh->osh, pi->u.pi_lcn20phy, sizeof(phy_info_lcn20phy_t));
}

static void *
wlc_lcn20_malloc(phy_info_lcn20phy_t *pi_lcn20phy, uint16 size, uint32 line)
{
	uint8 *ret_ptr = NULL;
	if (pi_lcn20phy->calbuffer_inuse) {
		PHY_ERROR(("FATAL Error: Concurrent LCN20PHY memory allocation @ line %d\n", line));
		ASSERT(FALSE);
	} else if (size > LCN20PHY_CALBUFFER_MAX_SZ) {
		PHY_ERROR(("FATAL Error: Buffer size (%d) required > MAX @ line %d\n", size, line));
		ASSERT(FALSE);
	} else {
		/* printf("Allocation @ line %d ...", line); */
		pi_lcn20phy->calbuffer_inuse = TRUE;
		ret_ptr = pi_lcn20phy->calbuffer;
	}
	return ret_ptr;
}

static void
wlc_lcn20_mfree(phy_info_lcn20phy_t *pi_lcn20phy, uint32 line)
{
	if (!pi_lcn20phy->calbuffer_inuse) {
		PHY_ERROR(("FATAL Error: MFree called but no prev alloc @ line %d\n", line));
		ASSERT(FALSE);
	} else {
		/* printf("Deallocation @ line %d\n", line); */
		pi_lcn20phy->calbuffer_inuse = FALSE;
	}
}

static void
WLBANDINITFN(wlc_phy_cal_init_lcn20phy)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

/* BEGIN: SET CHANNEL Below functions are part of set channel funtionality */

static void
wlc_lcn20phy_bandset(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	PHY_REG_MOD(pi, LCN20PHY, lpphyCtrl, muxGmode, 1);
	PHY_REG_MOD(pi, LCN20PHY, ChannelControl, currentBand, 0);
}

static void
wlc_lcn20phy_tx_farrow_init(phy_info_t *pi, uint8 channel)
{
	const chan_info_20692_lcn20phy_t *chi;
	int fi = LCN20PHY_TX_FARROW_RATIO;
	uint32 mu_delta, freq = 0;

	/* Write the tuning tables */
	if (pi->xtalfreq == XTAL_FREQ_37P4MHZ)
		chi = &chan_info_20692_rev1_lcn20phy_37p4MHz[channel];
	else
		chi = &chan_info_20692_rev1_lcn20phy_26MHz[channel];
	ASSERT(chi != NULL);
	freq = chi->freq;

	mu_delta = mu_deltaLUT[channel - 1];

	phy_reg_write(pi, LCN20PHY_MuInitialLOAD, 0x2);
	phy_reg_write(pi, LCN20PHY_TxResamplerSampSyncVal, freq / wlc_phy_gcd(freq, fi));
	phy_reg_write(pi, LCN20PHY_TxResamplerMuDelta_u, ((mu_delta >> 16) & 0x3f) + 4096);
	phy_reg_write(pi, LCN20PHY_TxResamplerMuDelta_l, (mu_delta & 0xffff));
	phy_reg_write(pi, LCN20PHY_TxResamplerMuDeltaInit_u, ((mu_delta >> 16) & 0x3f) + 4096);
	phy_reg_write(pi, LCN20PHY_TxResamplerMuDeltaInit_l, (mu_delta & 0xffff));
}

static void wlc_lcn20phy_reset_iir_filter(phy_info_t *pi)
{
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LCN20PHY, sslpnCalibClkEnCtrl, forceTxfiltClkOn, 1)

		PHY_REG_MOD_ENTRY(LCN20PHY, sslpnCtrl0, txSoftReset, 1)

		PHY_REG_MOD_ENTRY(LCN20PHY, sslpnCtrl0, txSoftReset, 0)

		PHY_REG_MOD_ENTRY(LCN20PHY, sslpnCalibClkEnCtrl, forceTxfiltClkOn, 0)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
wlc_lcn20phy_cckscale_tx_iir_filter(phy_info_t *pi)
{
	int8 scale_fctr_db = pi->u.pi_lcn20phy->cckscale_fctr_db;

	/* scale CCK tx-iir coefficients based on nvram
	* parameter 'cckscale' (default set to -1)
	*/
	if (scale_fctr_db > 0) {
		uint8 scale_fctr;
		uint16 fltcoeffB1, fltcoeffB2, fltcoeffB3;

		if (scale_fctr_db > LCN20PHY_MAX_CCK_DB_SCALING)
			scale_fctr_db =  LCN20PHY_MAX_CCK_DB_SCALING;
		scale_fctr = LCN20PHY_db2scalefctr_cck[scale_fctr_db-1];

		fltcoeffB1 = phy_reg_read(pi, LCN20PHY_ccktxfilt20CoeffStg2B1);
		fltcoeffB2 = phy_reg_read(pi, LCN20PHY_ccktxfilt20CoeffStg2B2);
		fltcoeffB3 = phy_reg_read(pi, LCN20PHY_ccktxfilt20CoeffStg2B3);

		phy_reg_write(pi, LCN20PHY_ccktxfilt20CoeffStg2B1,
		((fltcoeffB1 * scale_fctr) >> LCN20PHY_DB2SCALEFCTR_SHIFT));
		phy_reg_write(pi, LCN20PHY_ccktxfilt20CoeffStg2B2,
		((fltcoeffB2 * scale_fctr) >> LCN20PHY_DB2SCALEFCTR_SHIFT));
		phy_reg_write(pi, LCN20PHY_ccktxfilt20CoeffStg2B3,
		((fltcoeffB3 * scale_fctr) >> LCN20PHY_DB2SCALEFCTR_SHIFT));
	}
}

static int
wlc_lcn20phy_load_tx_iir_filter(phy_info_t *pi, phy_tx_iir_filter_mode_t mode, int16 filt_type)
{
	int16 filt_index = -1, j;
	uint16 (*dac_coeffs_table)[LCN20PHY_NUM_DIG_FILT_COEFFS+1];
	uint8 max_filter_type, max_filter_coeffs;
	uint16 *addr_coeff;

	uint16 addr_cck[] = {
		LCN20PHY_ccktxfilt20Stg1Shft,
		LCN20PHY_ccktxfilt20CoeffStg0A1,
		LCN20PHY_ccktxfilt20CoeffStg0A2,
		LCN20PHY_ccktxfilt20CoeffStg0B1,
		LCN20PHY_ccktxfilt20CoeffStg0B2,
		LCN20PHY_ccktxfilt20CoeffStg0B3,
		LCN20PHY_ccktxfilt20CoeffStg1A1,
		LCN20PHY_ccktxfilt20CoeffStg1A2,
		LCN20PHY_ccktxfilt20CoeffStg1B1,
		LCN20PHY_ccktxfilt20CoeffStg1B2,
		LCN20PHY_ccktxfilt20CoeffStg1B3,
		LCN20PHY_ccktxfilt20CoeffStg2A1,
		LCN20PHY_ccktxfilt20CoeffStg2A2,
		LCN20PHY_ccktxfilt20CoeffStg2B1,
		LCN20PHY_ccktxfilt20CoeffStg2B2,
		LCN20PHY_ccktxfilt20CoeffStg2B3,
		LCN20PHY_ccktxfilt20CoeffStg0_leftshift
		};

	uint16 addr_ofdm[] = {
		LCN20PHY_txfilt20Stg1Shft,
		LCN20PHY_txfilt20CoeffStg0A1,
		LCN20PHY_txfilt20CoeffStg0A2,
		LCN20PHY_txfilt20CoeffStg0B1,
		LCN20PHY_txfilt20CoeffStg0B2,
		LCN20PHY_txfilt20CoeffStg0B3,
		LCN20PHY_txfilt20CoeffStg1A1,
		LCN20PHY_txfilt20CoeffStg1A2,
		LCN20PHY_txfilt20CoeffStg1B1,
		LCN20PHY_txfilt20CoeffStg1B2,
		LCN20PHY_txfilt20CoeffStg1B3,
		LCN20PHY_txfilt20CoeffStg2A1,
		LCN20PHY_txfilt20CoeffStg2A2,
		LCN20PHY_txfilt20CoeffStg2B1,
		LCN20PHY_txfilt20CoeffStg2B2,
		LCN20PHY_txfilt20CoeffStg2B3,
		LCN20PHY_txfilt20CoeffStg0_leftshift
		};
	switch (mode) {
	case (TX_IIR_FILTER_OFDM):
		addr_coeff = (uint16 *)addr_ofdm;
		dac_coeffs_table = LCN20PHY_txdigfiltcoeffs_ofdm;
		max_filter_type = LCN20PHY_NUM_TX_DIG_FILTERS_OFDM;
		break;
	case (TX_IIR_FILTER_CCK):
		addr_coeff = (uint16 *)addr_cck;
		dac_coeffs_table = LCN20PHY_txdigfiltcoeffs_cck;
		max_filter_type = LCN20PHY_NUM_TX_DIG_FILTERS_CCK;
		break;
	default:
		/* something wierd happened if coming here */
		addr_coeff = NULL;
		dac_coeffs_table = NULL;
		max_filter_type = 0;
		ASSERT(FALSE);
	}

	max_filter_coeffs = LCN20PHY_NUM_DIG_FILT_COEFFS - 1;

	/* Search for the right entry in the table */
	for (j = 0; j < max_filter_type; j++) {
		if (filt_type == dac_coeffs_table[j][0]) {
			filt_index = (int16)j;
			break;
		}
	}

	/* Grave problem if entry not found */
	if (filt_index == -1) {
		ASSERT(FALSE);
	} else {
		/* Apply the coefficients to the filter type */
		for (j = 0; j < max_filter_coeffs; j++)
			phy_reg_write(pi, addr_coeff[j], dac_coeffs_table[filt_index][j+1]);

		/* Scale cck coeffs */
		if (mode == TX_IIR_FILTER_CCK)
			wlc_lcn20phy_cckscale_tx_iir_filter(pi);
	}

	/* Reset the iir filter after setting the coefficients */
	wlc_lcn20phy_reset_iir_filter(pi);

	return (filt_index != -1) ? 0 : -1;
}

static void
wlc_lcn20phy_tx_init(phy_info_t *pi, uint8 channel)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	int16 cck_ftype, ofdm_ftype;

	PHY_REG_MOD(pi, LCN20PHY, Core1TxControl, loft_comp_shift, 1);
	wlc_lcn20phy_tx_farrow_init(pi, channel);

	/* Set DAC mode to use 12 MSBs when converting from 13-bit TX samples
	* to 12-bit DAC samples
	*/
	phy_reg_write(pi, LCN20PHY_BypassPredacRound, 0x0);

	/* Init/load the real iir filter coefficients */
	/* 20 ofdm settings */
	if (channel == 14)
		cck_ftype = 31;
	else if (pi_lcn20->cckdigiftype >= 0)
		cck_ftype = pi_lcn20->cckdigiftype;
	else
		cck_ftype = 21;

	if (pi_lcn20->ofdmdigiftype >= 0)
		ofdm_ftype = pi_lcn20->ofdmdigiftype;
	else
		ofdm_ftype = 6;

	wlc_lcn20phy_load_tx_iir_filter(pi, TX_IIR_FILTER_CCK, cck_ftype);
	wlc_lcn20phy_load_tx_iir_filter(pi, TX_IIR_FILTER_OFDM, ofdm_ftype);

	PHY_REG_MOD(pi, LCN20PHY, lpphyCtrl, txfiltSelect, 2);
}

static void
wlc_lcn20phy_agc_reset(phy_info_t *pi)
{
	PHY_REG_LIST_START
		PHY_REG_WRITE_ENTRY(LCN20PHY, crsgainCtrl_new, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, agcControl4, c_agc_fsm_en, 0)
		PHY_REG_OR_ENTRY(LCN20PHY, resetCtrl, 0x44)
		PHY_REG_WRITE_ENTRY(LCN20PHY, resetCtrl, 0x80)
		PHY_REG_WRITE_ENTRY(LCN20PHY, crsgainCtrl_new, 0xff)
		PHY_REG_MOD_ENTRY(LCN20PHY, agcControl4, c_agc_fsm_en, 1)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
wlc_lcn20phy_agc_tweaks(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Reset agc  */
	wlc_lcn20phy_agc_reset(pi);
}

static void
wlc_lcn20phy_rx_farrow_init(phy_info_t *pi, uint8 channel)
{
	uint32 freq = 0, farrow_mu, highest_period, GCD, rxperiod;
	const chan_info_20692_lcn20phy_t *chi;
	uint32 far_muLUT[14] = {
	31614566, 31680102, 31745638, 31811174, 31876710, 31942246, 32007782,
	32073318, 32138854,	32204390, 32269926,	32335462, 32400998,	32558285
	};

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Write the tuning tables */
	if (pi->xtalfreq == XTAL_FREQ_37P4MHZ)
		chi = &chan_info_20692_rev1_lcn20phy_37p4MHz[channel];
	else
		chi = &chan_info_20692_rev1_lcn20phy_26MHz[channel];
	ASSERT(chi != NULL);
	freq = chi->freq;

	farrow_mu = far_muLUT[channel - 1];

	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig2, fcw_value_lo, (farrow_mu & 0xffff));
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig3, fcw_value_hi, ((farrow_mu >> 16) & 0xffff));
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig3, fast_ADC_en, 0);

	highest_period = 1280;
	GCD = wlc_phy_gcd(highest_period, freq);
	rxperiod = highest_period / GCD;

	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig6, rx_farrow_drift_period, rxperiod);
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig1, rx_farrow_drift_en, 1);
}

static void
wlc_lcn20phy_set_sfo_chan_centers(phy_info_t *pi, uint8 channel)
{
	uint16 freq = 0, tmp;
	const chan_info_20692_lcn20phy_t *chi;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Write the tuning tables */
	if (pi->xtalfreq == XTAL_FREQ_37P4MHZ)
		chi = &chan_info_20692_rev1_lcn20phy_37p4MHz[channel];
	else
		chi = &chan_info_20692_rev1_lcn20phy_26MHz[channel];
	ASSERT(chi != NULL);
	freq = chi->freq;

	/* sfo_chan_center_Ts20 = round(fc / 20e6*(ten_mhz+1) * 8), fc in Hz
	*                      = round($channel * 0.4 *($ten_mhz+1)), $channel in MHz
	*/
	tmp = (freq * 4 / 5 + 1) >> 1;
	PHY_REG_MOD(pi, LCN20PHY, ptcentreTs20, centreTs20, tmp);
	/* sfo_chan_center_factor = round(2^17 ./ (fc/20e6)/(ten_mhz+1)), fc in Hz
	*                        = round(2621440 ./ $channel/($ten_mhz+1)), $channel in MHz
	*/
	tmp = (2621440 * 2 / freq + 1) >> 1;
	PHY_REG_MOD(pi, LCN20PHY, ptcentreFactor, centreFactor, tmp);
}

static void
wlc_lcn20phy_rx_init(phy_info_t *pi, uint8 channel)
{
	wlc_lcn20phy_rx_farrow_init(pi, channel);
	wlc_lcn20phy_set_sfo_chan_centers(pi, channel);

	if (NORADIO_ENAB(pi->pubpi)) {
		phy_reg_write(pi, LCN20PHY_DSSF_C_CTRL, 0x0);
		PHY_REG_MOD(pi, LCN20PHY, DSSSConfirmCnt, DSSSConfirmCntLoGain, 0x3);
	}

	/* Reset agc  */
	wlc_lcn20phy_agc_reset(pi);
}

static void
wlc_lcn20phy_radio20692_rc_cal(phy_info_t *pi)
{
	uint8 cal, done;
	uint16 rccal_itr, n0, n1;

	/* lpf, adc, dacbuf */
	uint8 sr[] = {0x1, 0x1, 0x0};
	uint8 sc[] = {0x0, 0x1, 0x2};
	uint8 x1[] = {0x1c, 0x70, 0x40};
	uint16 trc[] = {0x22d, 0xf0, 0x10a};
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	uint32 dn;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	ASSERT(RADIOID(pi->pubpi.radioid) == BCM20692_ID);

	/* Powerup rccal driver & set divider radio (rccal needs to run at 20mhz) */
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu,
		(READ_RADIO_REGFLD_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu) | 0x40));

	/* Calibrate lpf, adc, dacbuf */
	for (cal = 0; cal < LCN20PHY_NUM_2069_RCCAL_CAPS; cal++) {
		/* Setup */
		MOD_RADIO_REG_20692(pi, WL_RCCAL_LPO_CFG1, 0, wl_rccal_sr, sr[cal]);
		MOD_RADIO_REG_20692(pi, WL_RCCAL_LPO_CFG1, 0, wl_rccal_sc, sc[cal]);
		MOD_RADIO_REG_20692(pi, WL_RCCAL_CFG1, 0, wl_rccal_X1, x1[cal]);
		MOD_RADIO_REG_20692(pi, WL_RCCAL_CFG2, 0, wl_rccal_Trc, trc[cal]);

		/* For dacbuf force fixed dacbuf cap to be 0 while calibration, restore it later */
		if (cal == 2) {
			MOD_RADIO_REG_20692(pi, WL_TX_DAC_CFG5, 0, wl_DACbuf_fixed_cap, 0);
			MOD_RADIO_REG_20692(pi, WL_TX_BB_OVR1, 0, wl_ovr_DACbuf_fixed_cap, 1);
		}

		/* Toggle RCCAL power */
		MOD_RADIO_REG_20692(pi, WL_RCCAL_LPO_CFG1, 0, wl_rccal_pu, 0);
		OSL_DELAY(1);
		MOD_RADIO_REG_20692(pi, WL_RCCAL_LPO_CFG1, 0, wl_rccal_pu, 1);

		OSL_DELAY(35);

		/* Start RCCAL */
		MOD_RADIO_REG_20692(pi, WL_RCCAL_CFG1, 0, wl_rccal_START, 1);

		/* Wait for rcal to be done, max = 100us * 100 = 10ms  */
		done = 0;
		for (rccal_itr = 0;
			(rccal_itr < LCN20PHY_MAX_2069_RCCAL_WAITLOOPS) && (done == 0);
			rccal_itr++) {
			OSL_DELAY(100);
			done = READ_RADIO_REGFLD_20692(pi, WL_RCCAL_CFG3, 0, wl_rccal_DONE);
		}

		/* Stop RCCAL */
		MOD_RADIO_REG_20692(pi, WL_RCCAL_CFG1, 0, wl_rccal_START, 0);

		/* Make sure that RC Cal ran to completion */
		ASSERT(done);

		n0 = READ_RADIO_REGFLD_20692(pi, WL_RCCAL_CFG4, 0, wl_rccal_N0);
		n1 = READ_RADIO_REGFLD_20692(pi, WL_RCCAL_CFG5, 0, wl_rccal_N1);
		dn = n1 - n0; /* set dn [expr {$N1 - $N0}] */

		if (cal == 0) {
			/* lpf */
			/* set k [expr {$is_adc ? 102051 : 101541}] */
			/* set gmult_p12 [expr {$prod1 / $fxtal_pm12}] */
			pi_lcn20->rccal_gmult = (101541 * dn) / (pi->xtalfreq >> 12);
			pi_lcn20->rccal_gmult_rc = pi_lcn20->rccal_gmult;
			PHY_INFORM(("wl%d: %s rccal_lpf_gmult = %d\n", pi->sh->unit,
			            __FUNCTION__, pi_lcn20->rccal_gmult));
		} else if (cal == 1) {
			/* adc */
			/* set k [expr {$is_adc ? 102051 : 101541}] */
			/* set gmult_p12 [expr {$prod1 / $fxtal_pm12}] */
			pi_lcn20->rccal_adc_gmult = (102051 * dn) / (pi->xtalfreq >> 12);
			PHY_INFORM(("wl%d: %s rccal_adc = %d\n", pi->sh->unit,
			            __FUNCTION__, pi_lcn20->rccal_adc_gmult));
		} else {
			/* dacbuf */
			pi_lcn20->rccal_dacbuf =
			READ_RADIO_REGFLD_20692(pi, WL_RCCAL_CFG6, 0, wl_rccal_raw_dacbuf);
			MOD_RADIO_REG_20692(pi, WL_TX_BB_OVR1, 0, wl_ovr_DACbuf_fixed_cap, 0);
			PHY_INFORM(("wl%d: %s rccal_dacbuf = %d\n", pi->sh->unit,
				__FUNCTION__, pi_lcn20->rccal_dacbuf));
		}
		/* Turn off rccal */
		MOD_RADIO_REG_20692(pi, WL_RCCAL_LPO_CFG1, 0, wl_rccal_pu, 0);
	}
	/* Powerdown rccal driver */
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu,
		(READ_RADIO_REGFLD_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu) | 0x3F));
}

/* 20692_lpf_tx_set is the top Tx LPF function
*/
static void
wlc_lcn20phy_radio20692_lpf_tx_set(phy_info_t *pi, int8 bq_bw, int8 bq_gain,
	int8 rc_bw_ofdm, int8 rc_bw_cck)
{
	uint8 i;
	uint16 gmult;
	uint16 gmult_rc;
	uint16 g10_tuned, g11_tuned, g12_tuned, g21_tuned, bias;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	gmult = pi_lcn20->rccal_gmult;
	gmult_rc = pi_lcn20->rccal_gmult_rc;

	/* search for given bq_gain */
	for (i = 0; i < ARRAYSIZE(g_index1); i++) {
		if (bq_gain == g_index1[i])
			break;
	}

	if (i < ARRAYSIZE(g_index1)) {
		uint16 g_passive_rc_tx_tuned_ofdm, g_passive_rc_tx_tuned_cck;
		g10_tuned = (lpf_g10[bq_bw][i] * gmult) >> 15;
		g11_tuned = (lpf_g11[bq_bw] * gmult) >> 15;
		g12_tuned = (lpf_g12[bq_bw][i] * gmult) >> 15;
		g21_tuned = (lpf_g21[bq_bw][i] * gmult) >> 15;
		g_passive_rc_tx_tuned_ofdm = (g_passive_rc_tx[rc_bw_ofdm] * gmult_rc) >> 15;
		g_passive_rc_tx_tuned_cck = (g_passive_rc_tx[rc_bw_cck] * gmult_rc) >> 15;
		bias = biases[bq_bw];

		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG3, 0, wl_lpf_g10, g10_tuned);
		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG7, 0, wl_lpf_g11, g11_tuned);
		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG4, 0, wl_lpf_g12, g12_tuned);
		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG5, 0, wl_lpf_g21, g21_tuned);
		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG6, 0, wl_lpf_g_passive_rc_tx,
			g_passive_rc_tx_tuned_ofdm);
		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG8, 0, wl_lpf_bias_bq, bias);

		/* Note down the values of the passive_rc for OFDM and CCK in Shmem */
		wlapi_bmac_write_shm(pi->sh->physhim, M_LPF_PASSIVE_RC_OFDM,
			g_passive_rc_tx_tuned_ofdm);
		wlapi_bmac_write_shm(pi->sh->physhim, M_LPF_PASSIVE_RC_CCK,
			g_passive_rc_tx_tuned_cck);

		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG2, 0, wl_lpf_sel_5g_out_gm, 0);
		MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG3, 0, wl_lpf_sel_2g_5g_cmref_gm, 0);
	} else {
		PHY_ERROR(("wl%d: %s: Invalid bq_gain %d\n", pi->sh->unit, __FUNCTION__, bq_gain));
	}
}

static void
wlc_lcn20phy_detection_level(phy_info_t *pi, int dsss_thres, int ofdm_thres)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	PHY_REG_MOD(pi, LCN20PHY, SignalBlock_edet1, signalblk_det_thresh_dsss, dsss_thres);
	PHY_REG_MOD(pi, LCN20PHY, SignalBlock_edet1, signalblk_det_thresh_ofdm, ofdm_thres);
}

static void
wlc_lcn20phy_agc_setup(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* selection of low/mid/high levels for nbrssi/wrssi3 detectors
	* Nbrssi
	*/
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG12, 0, wl_tia_nbrssi_ref_low_sel, 3);
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG12, 0, wl_tia_nbrssi_ref_mid_sel, 3);
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG12, 0, wl_tia_nbrssi_ref_high_sel, 3);
	/* Wrssi3 */
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG12, 0, wl_tia_wrssi3_ref_low_sel, 3);
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG11, 0, wl_tia_wrssi3_ref_mid_sel, 3);
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG11, 0, wl_tia_wrssi3_ref_high_sel, 3);

	/* Nbrssi/Wrssi3 */
	/* output drive strength adjust for wrrsi3 and nbrssi detectors */
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG11, 0, wl_tia_rssi_drive_strength, 0);
	/* binary bias code for NBRSSI (and WRSSI3, detectors */
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG11, 0, wl_tia_rssi_biasadj, 0x10);
	/* Wrssi1 */
	/* lna1 power detector threshold */
	MOD_RADIO_REG_20692(pi, WL_LNA2G_RSSI1, 0, wl_lna2g_dig_wrssi1_threshold, 0x7);
	/* enables x8 drive */
	MOD_RADIO_REG_20692(pi, WL_LNA2G_RSSI1, 0, wl_lna2g_dig_wrssi1_drive_strength, 0);
	/* end of RSSI SETTINGS from LCN20 radio sheet */

	/* TR Attenuation Switch */
	PHY_REG_MOD(pi, LCN20PHY, radioTRCtrl, gainrequestTRAttnOnEn, 0);
	/* gain settle delay(2, 5MHz clk, after the radio gain is changed */
	PHY_REG_MOD(pi, LCN20PHY, agcControl11, gain_settle_dly_cnt, 4);

	/* clip detector (farrow o/p, */
	/* threshold to determine clip count of Farrow o/p signal (8bit, */
	PHY_REG_MOD(pi, LCN20PHY, ClipThresh, ClipThresh, 52);
	/* clip counter threshold in HIGH GAIN state);
	* after clip count(number of I/Q clips in last 16 samples)
	* exceeds this threshold AGC goes to WAIT RSSI
	*/
	PHY_REG_MOD(pi, LCN20PHY, ClipCtrThresh, ClipCtrThreshHiGain, 12);

	/* RSSI CLIP GAINS/THREHSOLDS in WAIT RSSI STATE */
	/*  Clip gains */
	/* Nbrssi */
	PHY_REG_MOD(pi, LCN20PHY, agcControl14, rssi_clip_gain_norm_0, 37);
	PHY_REG_MOD(pi, LCN20PHY, agcControl14, rssi_clip_gain_norm_1, 31);
	PHY_REG_MOD(pi, LCN20PHY, agcControl15, rssi_clip_gain_norm_2, 16);
	/* Wrssi3 */
	PHY_REG_MOD(pi, LCN20PHY, agcControl15, rssi_clip_gain_norm_3, 16);
	PHY_REG_MOD(pi, LCN20PHY, agcControl16, rssi_clip_gain_norm_4, 16);
	PHY_REG_MOD(pi, LCN20PHY, agcControl16, rssi_clip_gain_norm_5, 16);
	/* Wrssi2 */
	PHY_REG_MOD(pi, LCN20PHY, agcControl17, rssi_clip_gain_norm_6, 16);
	PHY_REG_MOD(pi, LCN20PHY, agcControl17, rssi_clip_gain_norm_7, 16);
	PHY_REG_MOD(pi, LCN20PHY, agcControl18, rssi_clip_gain_norm_8, 16);
	/* Wrssi1 */
	PHY_REG_MOD(pi, LCN20PHY, agcContro245, rssi_clip_gain_norm_9, 10);
	PHY_REG_MOD(pi, LCN20PHY, agcContro245, rssi_clip_gain_norm_10, 4);
	PHY_REG_MOD(pi, LCN20PHY, agcContro246, rssi_clip_gain_norm_11, -8);

	/*  Clip counter thresholds */
	/* Nbrssi */
	PHY_REG_MOD(pi, LCN20PHY, agcControl19, rssi_clip_thresh_norm_0, 22);
	PHY_REG_MOD(pi, LCN20PHY, agcControl19, rssi_clip_thresh_norm_1, 22);
	PHY_REG_MOD(pi, LCN20PHY, agcControl20, rssi_clip_thresh_norm_2, 22);
	/* W3 rssi */
	PHY_REG_MOD(pi, LCN20PHY, agcControl20, rssi_clip_thresh_norm_3, 0);
	PHY_REG_MOD(pi, LCN20PHY, agcControl21, rssi_clip_thresh_norm_4, 0);
	PHY_REG_MOD(pi, LCN20PHY, agcControl21, rssi_clip_thresh_norm_5, 0);
	/* w2 rssi (not used, */
	PHY_REG_MOD(pi, LCN20PHY, agcControl22, rssi_clip_thres_norm_6, 0);
	PHY_REG_MOD(pi, LCN20PHY, agcControl22, rssi_clip_thresh_norm_7, 0);
	PHY_REG_MOD(pi, LCN20PHY, agcControl23, rssi_clip_thresh_norm_8, 0);
	/* W1 rssi */
	PHY_REG_MOD(pi, LCN20PHY, agcControl23, rssi_clip_thresh_norm_9,  11);
	PHY_REG_MOD(pi, LCN20PHY, agcContro240, rssi_clip_thresh_norm_10, 11);
	PHY_REG_MOD(pi, LCN20PHY, agcContro240, rssi_clip_thresh_norm_11, 11);

	/* rssi no clip gain */
	PHY_REG_MOD(pi, LCN20PHY, agcControl38, rssi_no_clip_gain_normal, 46);

	/* end of RSSI CLIP GAINS/THREHSOLDS in WAIT RSSI STATE */

	/* lower DSSS threshold */
	wlc_lcn20phy_detection_level(pi, 97, 97);

	wlc_lcn20phy_agc_reset(pi);
}

static void
wlc_phy_chanspec_set_lcn20phy(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
	bool suspend;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Set the phy bandwidth as dictated by the chanspec
	* FIXME: For now, not calling this but seems like it might  be necessary
	* during the init, Need to check
	if (CHSPEC_BW(chanspec) != pi->bw)
		wlapi_bmac_bw_set(pi->sh->physhim, CHSPEC_BW(chanspec));
	*/

	wlc_lcn20phy_bandset(pi);

	wlc_lcn20phy_deaf_mode(pi, TRUE);

	suspend = !(R_REG(pi->sh->osh, &((phy_info_t *)pi)->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	/* Tune radio for the channel */
	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_lcn20phy_radio20692_channel_tune(pi, channel);
	}

	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);

	wlc_lcn20phy_tx_init(pi, channel);

	wlc_lcn20phy_rx_init(pi, channel);

	/* tune alpf settings for LCN20 radio */
	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_lcn20phy_radio20692_lpf_tx_set(pi, 2, 2, 2, 1);
	}

	/* Perform Tx IQ, Rx IQ and PAPD cal only if no scan in progress */

	if (!NORADIO_ENAB(pi->pubpi)) {
		/* DC calibration */
		wlc_lcn20phy_dccal_force_cal(pi,
			(LCN20PHY_DCCALMODE_DCOE | LCN20PHY_DCCALMODE_IDACC), 2);

		/* RX IQ calibration */
		wlc_lcn20phy_rx_iq_cal(pi);

		/* Tx IQLO calibrations */
		wlc_lcn20phy_tx_iqlo_cal_txpwr(pi);
	}

	if (!NORADIO_ENAB(pi->pubpi))
		wlc_lcn20phy_agc_setup(pi);
	else
		wlc_lcn20phy_agc_tweaks(pi);

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	wlc_lcn20phy_deaf_mode(pi, FALSE);

	/* Initialize power control */
	wlc_lcn20phy_txpwrctrl_init(pi);
}

/* END: SET CHANNEL above functions are part of set channel funtionality */

static void
wlc_phy_txpower_recalc_target_lcn20phy(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

#if defined(BCMDBG) || defined(WLTEST)
static int
wlc_phy_long_train_lcn20phy(phy_info_t *pi, int channel)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	return 0;
}
#endif // endif

static void
wlc_phy_get_tx_iqcc_lcn20phy(phy_info_t *pi, uint16 *a, uint16 *b)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}
static void
wlc_phy_set_tx_iqcc_lcn20phy(phy_info_t *pi, uint16 a, uint16 b)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

static uint16
wlc_phy_get_tx_locc_lcn20phy(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	return 0;
}

static void
wlc_phy_set_tx_locc_lcn20phy(phy_info_t *pi, uint16 didq)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

static void
wlc_phy_get_radio_loft_lcn20phy(phy_info_t *pi,
	uint8 *ei0,
	uint8 *eq0,
	uint8 *fi0,
	uint8 *fq0)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

static void
wlc_phy_watchdog_lcn20phy(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

static void
wlc_lcn20phy_btc_adjust(phy_info_t *pi, bool btactive)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

#if defined(WLTEST)
static void
wlc_phy_carrier_suppress_lcn20phy(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}
#endif // endif

static void
wlc_lcn20phy_anacore(phy_info_t *pi, bool on)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

static void
wlc_lcn20phy_switch_radio(phy_info_t *pi, bool on)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

void
wlc_lcn20phy_write_table(phy_info_t *pi, const phytbl_info_t *pti)
{
	uint16 saved_reg = 0;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT((pti->tbl_width == 8) || (pti->tbl_width == 16) ||
		(pti->tbl_width == 32));

	if (pti->tbl_id == LCN20PHY_TBL_ID_TXPWRCTL) {
		saved_reg = phy_reg_read(pi, LCN20PHY_RxFeCtrl1);
		PHY_REG_MOD(pi, LCN20PHY, RxFeCtrl1, disable_stalls, 1);
	}

	wlc_phy_write_table_ext(pi, pti, LCN20PHY_TableID,
		LCN20PHY_TableOffset, LCN20PHY_TableOffset,
		LCN20PHY_TableDataHi, LCN20PHY_TableDataLo);

	if (pti->tbl_id == LCN20PHY_TBL_ID_TXPWRCTL)
		phy_reg_write(pi, LCN20PHY_RxFeCtrl1, saved_reg);
}

void
wlc_lcn20phy_read_table(phy_info_t *pi, phytbl_info_t *pti)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT((pti->tbl_width == 8) || (pti->tbl_width == 16) ||
		(pti->tbl_width == 32));

	wlc_phy_read_table_ext(pi, pti, LCN20PHY_TableID, LCN20PHY_TableOffset,
		LCN20PHY_TableOffset, LCN20PHY_TableDataHi, LCN20PHY_TableDataLo);
}

void
wlc_lcn20phy_calib_modes(phy_info_t *pi, uint mode)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

/* ----START------- LPC feature macros and functions ------------ */
#if defined(LP_P2P_SOFTAP) || defined(WL_LPC)
#define LPC_MIN_IDX 38

#define LPC_TOT_IDX (LPC_MIN_IDX + 1)
#define LCN20PHY_TX_PWR_CTRL_MACLUT_MAX_ENTRIES	64
#define LCN20PHY_TX_PWR_CTRL_MACLUT_WIDTH	8

#ifdef WL_LPC
static uint8 lpc_pwr_level[LPC_TOT_IDX] =
	{0, 2, 4, 6, 8, 10,
	12, 14, 16, 18, 20,
	22, 24, 26, 28, 30,
	32, 34, 36, 38, 40,
	42, 44, 46, 48, 50,
	52, 54, 56, 58, 60,
	61, 62, 63, 64, 65,
	66, 67, 68};
#endif /* WL_LPC */

static void
wlc_lcn20phy_lpc_write_maclut(phy_info_t *pi)
{
	phytbl_info_t tab;

#if defined(LP_P2P_SOFTAP)
	uint8 i;
	/* Assign values from 0 to 63 qdB for now */
	for (i = 0; i < LCN20PHY_TX_PWR_CTRL_MACLUT_LEN; i++)
		pwr_lvl_qdB[i] = i;
	tab.tbl_ptr = pwr_lvl_qdB;
	tab.tbl_len = LCN20PHY_TX_PWR_CTRL_MACLUT_LEN;

#elif defined(WL_LPC)

	/* If not enabled, no need to clear out the table, just quit */
	if (!pi->lpc_algo)
		return;
	tab.tbl_ptr = lpc_pwr_level;
	tab.tbl_len = LPC_TOT_IDX;
#endif /* WL_LPC */

	tab.tbl_id = LCN20PHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = LCN20PHY_TX_PWR_CTRL_MACLUT_WIDTH;
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_MAC_OFFSET;

	/* Write to it */
	wlc_lcn20phy_write_table(pi, &tab);
}

#ifdef WL_LPC
uint8
wlc_lcn20phy_lpc_getminidx(void)
{
	return LPC_MIN_IDX;
}

void
wlc_lcn20phy_lpc_setmode(phy_info_t *pi, bool enable)
{
	if (enable)
		wlc_lcn20phy_lpc_write_maclut(pi);
}

uint8
wlc_lcn20phy_lpc_getoffset(uint8 index)
{
	return lpc_pwr_level[index];
	/* return lpc_pwr_level[index]; for PHYs which expect the actual offset
	 * for example, HT 4331.
	 */
}

uint8
wlc_lcn20phy_lpc_get_txcpwrval(uint16 phytxctrlword)
{
	return (phytxctrlword & PHY_TXC_PWR_MASK) >> PHY_TXC_PWR_SHIFT;
}

void
wlc_lcn20phy_lpc_set_txcpwrval(uint16 *phytxctrlword, uint8 txcpwrval)
{
	*phytxctrlword = (*phytxctrlword & ~PHY_TXC_PWR_MASK) |
		(txcpwrval << PHY_TXC_PWR_SHIFT);
	return;
}

#ifdef WL_LPC_DEBUG
uint8 *
wlc_lcn20phy_lpc_get_pwrlevelptr(void)
{
	return lpc_pwr_level;
}
#endif // endif
#endif /* WL_LPC */
#endif /* WL_LPC || LP_P2P_SOFTAP */

/* ----END------- LPC feature macros and functions ------------ */

/* BEGIN : TxPwrCtrl Below functions are part of Tx Power Control funtionality */

static void
wlc_lcn20phy_set_pa_gain(phy_info_t *pi, uint16 gain)
{
	PHY_REG_MOD(pi, LCN20PHY, txgainctrlovrval1, pagain_ovr_val1, gain);
}

static void
wlc_lcn20phy_set_dac_gain(phy_info_t *pi, uint16 dac_gain)
{
	uint16 dac_ctrl;

	dac_ctrl = (phy_reg_read(pi, LCN20PHY_AfeDACCtrl) >> LCN20PHY_AfeDACCtrl_dac_ctrl_SHIFT);
	dac_ctrl = dac_ctrl & ~(LCN20PHY_DACGAIN_MASK);
	dac_ctrl = dac_ctrl | (dac_gain << 7);
	PHY_REG_MOD(pi, LCN20PHY, AfeDACCtrl, dac_ctrl, dac_ctrl);
}

static uint16
wlc_lcn20phy_get_pa_gain(phy_info_t *pi)
{
	uint16 pa_gain;

	pa_gain = (phy_reg_read(pi, LCN20PHY_txgainctrlovrval1) &
		LCN20PHY_txgainctrlovrval1_pagain_ovr_val1_MASK) >>
		LCN20PHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT;

	return pa_gain;
}

static void
wlc_lcn20phy_set_tx_gain(phy_info_t *pi,  phy_txgains_t *target_gains)
{
	uint16 pa_gain = wlc_lcn20phy_get_pa_gain(pi);

	PHY_REG_MOD(pi, LCN20PHY, txgainctrlovrval0, txgainctrl_ovr_val0,
		(target_gains->gm_gain) | (target_gains->pga_gain << 8));

	PHY_REG_MOD(pi, LCN20PHY, txgainctrlovrval1, txgainctrl_ovr_val1,
		(target_gains->pad_gain) | (pa_gain << 8));

	wlc_lcn20phy_set_dac_gain(pi, target_gains->dac_gain);
	/* Enable gain overrides */
	wlc_lcn20phy_enable_tx_gain_override(pi);
}

static void
wlc_lcn20phy_set_tx_locc(phy_info_t *pi, uint16 didq)
{
	phytbl_info_t tab;

	/* Update iqloCaltbl */
	tab.tbl_id = LCN20PHY_TBL_ID_IQLOCAL;
	tab.tbl_width = 16;
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = LCN20PHY_TXIQLOCAL_DLOCOMP_OFFSET;
	wlc_lcn20phy_write_table(pi, &tab);
}

static void
wlc_lcn20phy_set_tx_iqcc(phy_info_t *pi, uint16 a, uint16 b)
{
	phytbl_info_t tab;
	uint16 iqcc[2];

	/* Fill buffer with coeffs */
	iqcc[0] = a;
	iqcc[1] = b;
	/* Update iqloCaltbl */
	tab.tbl_id = LCN20PHY_TBL_ID_IQLOCAL;
	tab.tbl_width = 16;
	tab.tbl_ptr = iqcc;
	tab.tbl_len = 2;
	tab.tbl_offset = LCN20PHY_TXIQLOCAL_IQCOMP_OFFSET;
	wlc_lcn20phy_write_table(pi, &tab);
}

static void
wlc_lcn20phy_force_pwr_index(phy_info_t *pi, int indx)
{
	phytbl_info_t tab;
	uint16 a, b;
	uint8 bb_mult;
	uint32 bbmultiqcomp, txgain, locoeffs, rfpower;
	phy_txgains_t gains;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lcnphy_calcache_t *cache = NULL;

	if (ctx)
		cache = &ctx->u.lcnphy_cache;
#endif // endif

	ASSERT(indx <= LCN20PHY_MAX_TX_POWER_INDEX);

	/* Save forced index */
	pi_lcn20->tx_power_idx_override = (int8)indx;
	pi_lcn20->current_index = (uint8)indx;

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LCN20PHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_len = 1;        /* # values   */

	/* Read index based bb_mult, a, b from the table */
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_PWR_OFFSET + indx; /* iqCoefLuts */
	tab.tbl_ptr = &bbmultiqcomp; /* ptr to buf */
	wlc_lcn20phy_read_table(pi,  &tab);

	/* Read index based tx gain from the table */
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_GAIN_OFFSET + indx; /* gainCtrlLuts */
	tab.tbl_width = 32;
	tab.tbl_ptr = &txgain; /* ptr to buf */
	wlc_lcn20phy_read_table(pi,  &tab);
	/* Apply tx gain */
	gains.gm_gain = (uint16)(txgain & 0x1f);
	gains.pga_gain = (uint16)(txgain >> 8) & 0xff;
	gains.pad_gain = (uint16)(txgain >> 16) & 0xff;
	gains.dac_gain = (uint16)(bbmultiqcomp >> 17) & 0xf;
	wlc_lcn20phy_set_tx_gain(pi, &gains);
	wlc_lcn20phy_set_pa_gain(pi,  (uint16)(txgain >> 24) & 0xff);
	/* Apply bb_mult */
	bb_mult = (uint8)((bbmultiqcomp >> 9) & 0xff);
	wlc_lcn20phy_set_bbmult(pi, bb_mult);
	/* Enable gain overrides */
	wlc_lcn20phy_enable_tx_gain_override(pi);
	/* the reading and applying lo, iqcc coefficients is not getting done for 4313A0 */
	/* to be fixed */

	/* Apply iqcc */
	a = (uint16)((bbmultiqcomp >> 10) & 0x3ff);
	b = (uint16)(bbmultiqcomp & 0x3ff);

#if defined(PHYCAL_CACHING)
	if (ctx && cache->txiqlocal_a[0]) {
#else
	/* if (pi_lcn20->lcnphy_cal_results.txiqlocal_a[0]) { */
	{
#endif /* defined(PHYCAL_CACHING) */
		wlc_lcn20phy_set_tx_iqcc(pi, a, b);
	/* Read index based di & dq from the table */
	}
#if defined(PHYCAL_CACHING)
	if (ctx && cache->txiqlocal_didq[0]) {
#else
	/* if (pi_lcn->lcnphy_cal_results.txiqlocal_didq[0]) { */
	{
#endif /* defined(PHYCAL_CACHING) */
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_LO_OFFSET + indx; /* loftCoefLuts */
	tab.tbl_ptr = &locoeffs; /* ptr to buf */
	wlc_lcn20phy_read_table(pi,  &tab);
	/* Apply locc */
	wlc_lcn20phy_set_tx_locc(pi, (uint16)locoeffs);
	}
	/* Apply PAPD rf power correction */
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_PWR_OFFSET + indx;
	tab.tbl_ptr = &rfpower; /* ptr to buf */
	wlc_lcn20phy_read_table(pi,  &tab);
	PHY_REG_MOD(pi, LCN20PHY, papd_analog_gain_ovr_val,
		papd_analog_gain_ovr_val, (rfpower & 0x1ff)* 8);
}

static void
wlc_lcn20phy_set_tx_pwr_by_index(phy_info_t *pi, int indx)
{
	/* Turn off automatic power control */
	wlc_lcn20phy_set_tx_pwr_ctrl(pi, LCN20PHY_TX_PWR_CTRL_OFF);

	/* Force tx power from the index */
	wlc_lcn20phy_force_pwr_index(pi, indx);
}

static void
wlc_lcn20phy_set_tx_gain_override(phy_info_t *pi, bool bEnable)
{
	uint16 bit = bEnable ? 1 : 0;

	PHY_REG_MOD(pi, LCN20PHY, rfoverride2, txgainctrl_ovr, bit);
	PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr, dacattctrl_ovr, bit);
}

static
void wlc_lcn20phy_get_tssi_floor(phy_info_t *pi, uint16 *floor)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	switch (wlc_phy_chanspec_bandrange_get(pi, pi->radio_chanspec)) {
		case WL_CHAN_FREQ_RANGE_2G:
			*floor = pi_lcn20->tssi_floor;
			break;
		default:
			ASSERT(FALSE);
			break;
	}
}

int32
wlc_lcn20phy_tssi2dbm(int32 tssi, int32 a1, int32 b0, int32 b1)
{
	int32 a, b, p;
	/* On lcnphy, estPwrLuts0/1 table entries are in S6.3 format */
	a = 32768 + (a1 * tssi);
	b = (1024 * b0) + (64 * b1 * tssi);
	p = ((2 * b) + a) / (2 * a);

	return p;
}

static
void wlc_lcn20phy_set_txpwr_clamp(phy_info_t *pi)
{
	uint16 tssi_floor = 0, idle_tssi_shift, adj_tssi_min;
	uint16 idleTssi_2C, idleTssi_OB, target_pwr_reg, intended_target;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	int32 a1 = 0, b0 = 0, b1 = 0;
	int32 target_pwr_cck_max, target_pwr_ofdm_max, pwr, max_ovr_pwr;
	int32 fudge = 0*8; /* 1dB */
	phytbl_info_t tab;
	uint32 rate_table[WL_RATESET_SZ_DSSS + WL_RATESET_SZ_OFDM + WL_RATESET_SZ_HT_MCS];
	uint8 ii;
	uint16 perPktIdleTssi;

	if (pi_lcn20->txpwr_clamp_dis || pi_lcn20->txpwr_tssifloor_clamp_dis) {
		pi_lcn20->target_pwr_ofdm_max = 0x7fffffff;
		pi_lcn20->target_pwr_cck_max = 0x7fffffff;
		if (pi_lcn20->btc_clamp) {
			target_pwr_cck_max = BTC_POWER_CLAMP;
			target_pwr_ofdm_max = BTC_POWER_CLAMP;
		} else {
			return;
		}
	} else {

		wlc_lcn20phy_get_tssi_floor(pi, &tssi_floor);
		wlc_phy_get_paparams_for_band(pi, &a1, &b0, &b1);

		perPktIdleTssi = PHY_REG_READ(pi, LCN20PHY, perPktIdleTssiCtrl,
			perPktIdleTssiUpdate_en);
		if (perPktIdleTssi)
			idleTssi_2C = PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlStatusNew6,
				avgidletssi);
		else
			idleTssi_2C = PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlIdleTssi,
				idleTssi0);

		if (idleTssi_2C >= 256)
			idleTssi_OB = idleTssi_2C - 256;
		else
			idleTssi_OB = idleTssi_2C + 256;

		idleTssi_OB = idleTssi_OB >> 2; /* Converting to 7 bits */
		idle_tssi_shift = (127 - idleTssi_OB) + 4;
		adj_tssi_min = MAX(tssi_floor, idle_tssi_shift);
		pwr = wlc_lcn20phy_tssi2dbm(adj_tssi_min, a1, b0, b1);
		target_pwr_ofdm_max = (pwr - fudge) >> 1;
		target_pwr_cck_max = (MIN(pwr, (pwr + pi_lcn20->cckPwrOffset)) - fudge) >> 1;
		PHY_TMP(("idleTssi_OB= %d, idle_tssi_shift= %d, adj_tssi_min= %d, "
				"pwr = %d, target_pwr_cck_max = %d, target_pwr_ofdm_max = %d\n",
				idleTssi_OB, idle_tssi_shift, adj_tssi_min, pwr,
				target_pwr_cck_max, target_pwr_ofdm_max));
		pi_lcn20->target_pwr_ofdm_max = target_pwr_ofdm_max;
		pi_lcn20->target_pwr_cck_max = target_pwr_cck_max;

		if (pi_lcn20->btc_clamp) {
			target_pwr_cck_max = MIN(target_pwr_cck_max, BTC_POWER_CLAMP);
			target_pwr_ofdm_max = MIN(target_pwr_ofdm_max, BTC_POWER_CLAMP);
		}
	}

	if (pi->txpwroverride) {
		max_ovr_pwr = MIN(target_pwr_ofdm_max, target_pwr_cck_max);
		{
			uint8 core;
			FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
				pi->tx_power_min_per_core[core] =
					MIN(pi->tx_power_min_per_core[core], max_ovr_pwr);
			}
		}
		return;
	}

	for (ii = 0; ii < ARRAYSIZE(rate_table); ii ++)
		rate_table[ii] = pi_lcn20->rate_table[ii];

	/* Adjust Rate Offset Table to ensure intended tx power for every OFDM/CCK */
	/* rate is less than target_power_ofdm_max/target_power_cck_max */
	target_pwr_reg = wlc_lcn20phy_get_target_tx_pwr(pi);
	for (ii = 0; ii < WL_RATESET_SZ_DSSS; ii ++) {
		intended_target = target_pwr_reg - rate_table[ii];
		if (intended_target > target_pwr_cck_max)
			rate_table[ii] = rate_table[ii] + (intended_target - target_pwr_cck_max);
		PHY_TMP(("Rate: %d, maxtar = %d, target = %d, origoff: %d, clampoff: %d\n",
			ii, target_pwr_cck_max, intended_target,
			pi_lcn20->rate_table[ii], rate_table[ii]));
	}
	for (ii = WL_RATESET_SZ_DSSS;
		ii < WL_RATESET_SZ_DSSS + WL_RATESET_SZ_OFDM + WL_RATESET_SZ_HT_MCS; ii ++) {
		intended_target = target_pwr_reg - rate_table[ii];
		if (intended_target > target_pwr_ofdm_max)
			rate_table[ii] = rate_table[ii] + (intended_target - target_pwr_ofdm_max);
		PHY_TMP(("Rate: %d, maxtar = %d, target = %d, origoff: %d, clampoff: %d\n",
			ii, target_pwr_ofdm_max, intended_target,
			pi_lcn20->rate_table[ii], rate_table[ii]));
	}

	tab.tbl_id = LCN20PHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_len = ARRAYSIZE(rate_table); /* # values   */
	tab.tbl_ptr = rate_table; /* ptr to buf */
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_lcn20phy_write_table(pi, &tab);
}

static void
wlc_lcn20phy_txpower_reset_npt(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	pi_lcn20->tssi_npt = LCN20PHY_TX_PWR_CTRL_START_NPT;
}

static void
wlc_lcn20phy_txpower_recalc_target(phy_info_t *pi)
{
	phytbl_info_t tab;
	ppr_dsss_rateset_t dsss_limits;
	ppr_ofdm_rateset_t ofdm_limits;
	ppr_ht_mcs_rateset_t mcs_limits;
	uint32 rate_table[WL_RATESET_SZ_DSSS + WL_RATESET_SZ_OFDM + WL_RATESET_SZ_HT_MCS];
	wl_tx_bw_t bw_mcs = WL_TX_BW_20;
	uint i, j;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	if (pi_lcn20->offset_targetpwr) {
		PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlTargetPwr,
			targetPwr0, (wlc_phy_txpower_get_target_min((wlc_phy_t*)pi) -
			(pi_lcn20->offset_targetpwr * 4)));
		return;
	}

	if (pi->tx_power_offset == NULL)
		return;

	/* Adjust rate based power offset */
	ppr_get_dsss(pi->tx_power_offset, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_limits);
	ppr_get_ofdm(pi->tx_power_offset, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_limits);
	ppr_get_ht_mcs(pi->tx_power_offset, bw_mcs, WL_TX_NSS_1,
		WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_limits);

	j = 0;
	for (i = 0; i < WL_RATESET_SZ_DSSS; i++, j++) {
		rate_table[j] = (uint32)((int32)(-dsss_limits.pwr[i]));
		pi_lcn20->rate_table[j] = rate_table[j];
		PHY_TMP((" Rate %d, offset %d\n", j, rate_table[j]));
	}

	for (i = 0; i < WL_RATESET_SZ_OFDM; i++, j++) {
		rate_table[j] = (uint32)((int32)(-ofdm_limits.pwr[i]));
		pi_lcn20->rate_table[j] = rate_table[j];
		PHY_TMP((" Rate %d, offset %d\n", j, rate_table[j]));
	}

	for (i = 0; i < WL_RATESET_SZ_HT_MCS; i++, j++) {
		rate_table[j] = (uint32)((int32)(-mcs_limits.pwr[i]));
		pi_lcn20->rate_table[j] = rate_table[j];
		PHY_TMP((" Rate %d, offset %d\n", j, rate_table[j]));
	}

	if (!pi_lcn20->uses_rate_offset_table) {
		/* Preset txPwrCtrltbl */
		tab.tbl_id = LCN20PHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;	/* 32 bit wide	*/
		tab.tbl_len = ARRAYSIZE(rate_table); /* # values   */
		tab.tbl_ptr = rate_table; /* ptr to buf */
		tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_RATE_OFFSET;
		wlc_lcn20phy_write_table(pi, &tab);
	}

	wlc_lcn20phy_set_txpwr_clamp(pi);

#if defined(LP_P2P_SOFTAP) || defined(WL_LPC)
	/* Update the MACAddr LUT which is cleared when doing recal */
#ifdef LP_P2P_SOFTAP
	if (pi_lcn20->pwr_offset_val)
#endif // endif
#ifdef WL_LPC
			if (pi->lpc_algo)
#endif /* WL_LPC */
			wlc_lcn20phy_lpc_write_maclut(pi);
#endif /* LP_P2P_SOFTAP || WL_LPC */

	/* Set new target power */
	wlc_lcn20phy_set_target_tx_pwr(pi, wlc_phy_txpower_get_target_min((wlc_phy_t*)pi));

	/* Should reset power index cache */
	wlc_lcn20phy_txpower_reset_npt(pi);
}

static void
wlc_lcn20phy_clear_tx_power_offsets(phy_info_t *pi)
{
	uint32 data_buf[64];
	phytbl_info_t tab;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	/* Clear out buffer */
	bzero(data_buf, sizeof(data_buf));

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LCN20PHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_ptr = data_buf; /* ptr to buf */

	/* we shouldn't be clearing the rate offset table */
	if (!pi_lcn20->uses_rate_offset_table) {
		/* Per rate power offset */
		tab.tbl_len = 20; /* # values   */
		tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_RATE_OFFSET;
		wlc_lcn20phy_write_table(pi, &tab);
	}
	/* Per index power offset */
	tab.tbl_len = 64; /* # values   */
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_MAC_OFFSET;
	wlc_lcn20phy_write_table(pi, &tab);
}

static void
wlc_lcn20phy_set_tx_pwr_ctrl(phy_info_t *pi, uint16 mode)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	uint16 old_mode = wlc_lcn20phy_get_tx_pwr_ctrl(pi);

	ASSERT(
		(LCN20PHY_TX_PWR_CTRL_OFF == mode) ||
		(LCN20PHY_TX_PWR_CTRL_SW == mode) ||
		(LCN20PHY_TX_PWR_CTRL_HW
		== (mode | LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)));

	if (old_mode != mode) {
		/* Setting txfront end clock also along with hwpwr control */
		PHY_REG_MOD(pi, LCN20PHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn,
			(LCN20PHY_TX_PWR_CTRL_HW ==
			(mode | LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)) ? 1 : 0);
		/* Feed back RF power level to PAPD block */
		PHY_REG_MOD(pi, LCN20PHY, papd_control2, papd_analog_gain_ovr,
			(LCN20PHY_TX_PWR_CTRL_HW ==
			(mode | LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)) ? 0 : 1);

		if (LCN20PHY_TX_PWR_CTRL_HW ==
			(mode | LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)) {
			PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRangeCmd, interpol_en, 1);
			/* interpolate using bbshift */
			PHY_REG_MOD(pi, LCN20PHY, bbShiftCtrl, bbshift_mode, 1);
		} else {
			PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRangeCmd, interpol_en, 0);
			/* interpolate using bbshift */
			PHY_REG_MOD(pi, LCN20PHY, bbShiftCtrl, bbshift_mode, 0);
		}

		if (LCN20PHY_TX_PWR_CTRL_HW ==
			(old_mode | LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)) {
			/* Clear out all power offsets */
			wlc_lcn20phy_clear_tx_power_offsets(pi);
			phy_reg_write(pi, LCN20PHY_BBmultCoeffSel, 0);
		}
		if (LCN20PHY_TX_PWR_CTRL_HW ==
			(mode | LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)) {
			/* Recalculate target power to restore power offsets */
			wlc_lcn20phy_txpower_recalc_target(pi);
			/* Set starting index & NPT to best known values for that target */
			wlc_lcn20phy_set_start_tx_pwr_idx(pi, pi_lcn20->tssi_idx);
			wlc_lcn20phy_set_start_CCK_tx_pwr_idx(pi, pi_lcn20->cck_tssi_idx);
			wlc_lcn20phy_set_tx_pwr_npt(pi, pi_lcn20->tssi_npt);
			phy_reg_write(pi, LCN20PHY_BBmultCoeffSel, 1);
			/* Reset frame counter for NPT calculations */
			pi_lcn20->tssi_tx_cnt = PHY_TOTAL_TX_FRAMES(pi);
			/* Disable any gain overrides */
			wlc_lcn20phy_disable_tx_gain_override(pi);
			pi_lcn20->tx_power_idx_override = -1;
		}
		else
			wlc_lcn20phy_enable_tx_gain_override(pi);

		/* Set requested tx power control mode */
		phy_reg_mod(pi, LCN20PHY_TxPwrCtrlCmd,
			(LCN20PHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			LCN20PHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK),
			mode);

		PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlCmd, use_txPwrCtrlCoefs, 0);

		PHY_INFORM(("wl%d: %s: %s \n", pi->sh->unit, __FUNCTION__,
			mode ? ((LCN20PHY_TX_PWR_CTRL_HW ==
			(mode | LCN20PHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)) ?
			"Auto" : "Manual") : "Off"));
	}
}

int8
wlc_lcn20phy_get_current_tx_pwr_idx(phy_info_t *pi)
{
	int8 indx;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	/* for txpwrctrl_off, return current_index */
	if (txpwrctrl_off(pi))
		indx = pi_lcn20->current_index;
	else
		indx = (int8)(wlc_lcn20phy_get_current_tx_pwr_idx_if_pwrctrl_on(pi)/2);

	return indx;
}

static void
wlc_lcn20phy_set_tssi_mux(phy_info_t *pi, lcn20phy_tssi_mode_t pos)
{
	/* Set TSSI/RSSI mux */
	if (LCN20PHY_TSSI_POST_PA == pos) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrl0, tssiSelVal0, 0)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrl0, tssiSelVal1, 1)
		PHY_REG_LIST_EXECUTE(pi);
	} else if (LCN20PHY_TSSI_EXT_POST_PAD == pos) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrl0, tssiSelVal0, 1)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrl0, tssiSelVal1, 0)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrl0, tssiRangeVal1, 1)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverride, 1)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverrideVal,
				0)
		PHY_REG_LIST_EXECUTE(pi);
	} else if (LCN20PHY_TSSI_PRE_PA == pos) {
		PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrl0, tssiSelVal0, 0x1);
		PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrl0, tssiSelVal1, 0);
	}

	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlCmdNew, txPwrCtrlScheme, 0);
}

static uint16
wlc_lcn20phy_rfseq_tbl_adc_pwrup(phy_info_t *pi)
{
	uint16 N1, N2, N3, N4, N5, N6, N;

	N1 = PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlNnum, Ntssi_delay);
	N2 = 1 << PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlNnum, Ntssi_intg_log2);
	N3 = PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlNum_Vbat, Nvbat_delay);
	N4 = 1 << PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlNum_Vbat, Nvbat_intg_log2);
	N5 = PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlNum_temp, Ntemp_delay);
	N6 = 1 << PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlNum_temp, Ntemp_intg_log2);
	N = 2 * (N1 + N2 + N3 + N4 + 2 *(N5 + N6)) + 80;
	if (N < 1600)
		N = 1600; /* min 20 us to avoid tx evm degradation */
	return N;
}

static void
wlc_lcn20phy_pwrctrl_rssiparams(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	uint16 auxpga_vmid, auxpga_gain = 0;

	auxpga_gain = pi_lcn20->rssi_gs;
	auxpga_vmid = (pi_lcn20->rssi_vc << 4) |
		pi_lcn20->rssi_vf;

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelVmidOverride, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelGainOverride, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverride, 0)
	PHY_REG_LIST_EXECUTE(pi);

	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlRfCtrl2,
		LCN20PHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelVmidVal0_MASK |
		LCN20PHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelGainVal0_MASK,
		(auxpga_vmid << LCN20PHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelVmidVal0_SHIFT) |
		(auxpga_gain << LCN20PHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelGainVal0_SHIFT));

	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlRfCtrl3,
		LCN20PHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelVmidVal1_MASK |
		LCN20PHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelGainVal1_MASK,
		(auxpga_vmid << LCN20PHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelVmidVal1_SHIFT) |
		(auxpga_gain << LCN20PHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelGainVal1_SHIFT));

	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlRfCtrl4,
		LCN20PHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelVmidVal2_MASK |
		LCN20PHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelGainVal2_MASK,
		(auxpga_vmid << LCN20PHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelVmidVal2_SHIFT) |
		(auxpga_gain << LCN20PHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelGainVal2_SHIFT));

	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlRfCtrl5,
		LCN20PHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelVmidVal3_MASK |
		LCN20PHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelGainVal3_MASK,
		(AUXPGA_VBAT_VMID_VAL << LCN20PHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelVmidVal3_SHIFT) |
		(AUXPGA_VBAT_GAIN_VAL << LCN20PHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelGainVal3_SHIFT));

	auxpga_vmid = AUXPGA_TEMPER_VMID_VAL;

	phy_reg_mod(pi, LCN20PHY_TxPwrCtrlRfCtrl6,
		LCN20PHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelVmidVal4_MASK |
		LCN20PHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelGainVal4_MASK,
		(auxpga_vmid << LCN20PHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelVmidVal4_SHIFT) |
		(AUXPGA_TEMPER_GAIN_VAL << LCN20PHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelGainVal4_SHIFT));
}

static void
wlc_lcn20phy_tssi_setup(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint32 *indxTbl, i;
	uint16 rfseq;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	int16 power_correction;

	if ((indxTbl = (uint32*) LCN20PHY_MALLOC(pi, 128 * sizeof(uint32))) == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	/* Setup estPwrLuts for measuring idle TSSI */
	tab.tbl_id = LCN20PHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_ptr = indxTbl; /* ptr to buf */
	tab.tbl_len = 128;        /* # values   */
	tab.tbl_offset = 0;
	for (i = 0; i < 128; i++) {
		*(indxTbl + i) = i;
	}
	wlc_lcn20phy_write_table(pi,  &tab);
	tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_EST_PWR_OFFSET;
	wlc_lcn20phy_write_table(pi,  &tab);

	if (indxTbl)
		LCN20PHY_MFREE(pi, indxTbl, 128 * sizeof(uint32));

	if (!PHY_EPA_SUPPORT(pi_lcn20->ePA))
		wlc_lcn20phy_set_tssi_mux(pi, LCN20PHY_TSSI_POST_PA);
	else
		wlc_lcn20phy_set_tssi_mux(pi, LCN20PHY_TSSI_EXT);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlCmd, hwtxPwrCtrl_en, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlCmd, txPwrCtrl_en, 1)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRangeCmd, force_vbatTemp, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlCmd, pwrIndex_init, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNnum, Ntssi_delay, 300)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNnum, Ntssi_intg_log2, 5)

		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNnum, Npt_intg_log2, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNum_Vbat, Nvbat_delay, 64)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNum_Vbat, Nvbat_intg_log2, 4)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNum_temp, Ntemp_delay, 64)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNum_temp, Ntemp_intg_log2, 4)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlDeltaPwrLimit, DeltaPwrLimit, 0x1)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRangeCmd, cckPwrOffset, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TempSenseCorrection, tempsenseCorr, 0)

		/*  Set idleTssi to (2^9-1) in OB format = (2^9-1-2^8) = 0xff in 2C format */
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlIdleTssi, rawTssiOffsetBinFormat, 1)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlIdleTssi, idleTssi0, 0xff)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlIdleTssi1, idleTssi1, 0xff)
	PHY_REG_LIST_EXECUTE(pi);

	if (pi_lcn20->tssical_time) {
		PHY_REG_MOD(pi, LCN20PHY, perPktIdleTssiCtrl, perPktIdleTssiUpdate_en, 1);
		power_correction = pi_lcn20->tempsenseCorr + pi_lcn20->idletssi_corr;
		PHY_REG_MOD(pi, LCN20PHY, TempSenseCorrection,
		tempsenseCorr, power_correction);
	} else
		PHY_REG_MOD(pi, LCN20PHY, perPktIdleTssiCtrl, perPktIdleTssiUpdate_en, 0);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LCN20PHY, perPktIdleTssiCtrl, perPktIdleTssi_en, 1)
		PHY_REG_MOD_ENTRY(LCN20PHY, perPktIdleTssiCtrl, Nidletssi_intg_log2, 4)
		PHY_REG_MOD_ENTRY(LCN20PHY, perPktIdleTssiCtrl, Nidletssi_delay, 45)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlIdleTssi2, Nidletssi_delay_cck, 45)

		/*  for CCK average over 40<<0 samples */
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNnumCCK, Ntssi_intg_log2_cck, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlNnumCCK, Ntssi_delay_cck, 300)

		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverride, 1)
		PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverrideVal, 2)
		/* set DVGA1 gain code (farrow gain during tx) */
		PHY_REG_MOD_ENTRY(LCN20PHY, RxSdFeConfig1, farrow_rshift_tx, 5)
	PHY_REG_LIST_EXECUTE(pi);

	wlc_lcn20phy_clear_tx_power_offsets(pi);

	rfseq = wlc_lcn20phy_rfseq_tbl_adc_pwrup(pi);

	tab.tbl_id = LCN20PHY_TBL_ID_RFSEQ;
	tab.tbl_width = 16;	/* 12 bit wide	*/
	tab.tbl_ptr = &rfseq;
	tab.tbl_len = 1;
	tab.tbl_offset = 6;
	wlc_lcn20phy_write_table(pi,  &tab);

	PHY_REG_MOD(pi, LCN20PHY, TSSIMode, tssiADCSel, 0);

	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_lcn20phy_radio20692_tssisetup(pi);

		/* Disable all overrides for tssi feedback path as they are in direct control
		* JIRA-468: Do not enable overrides for tssi feedback path,
		* direct control takes care of pu/pd
		*/

		/* Disable PU override for AMUX (a.k.a. testbuf) */
		MOD_RADIO_REG_20692(pi, WL_GPABUF_OVR1, 0, wl_ovr_testbuf_PU, 0x0);

		/* Disable PU override for AUX PGA */
		MOD_RADIO_REG_20692(pi, WL_AUX_RXPGA_OVR1, 0, wl_ovr_auxpga_i_pu, 0x0);

		/* Disable pu for post envelope detector tssi blocks */
		MOD_RADIO_REG_20692(pi, WL_TSSI_IQCAL_OVR1, 0, wl_ovr_iqcal_PU_tssi, 0x0);

		/* Disable PU override for TSSI envelope detector */
		MOD_RADIO_REG_20692(pi, WL_TX_TOP_2G_OVR2, 0, wl_ovr_pa2g_tssi_ctrl_pu, 0x0);

		/* Disable PU for vbat monitor and tempsense */
		MOD_RADIO_REG_20692(pi, WL_VBAT_MONITOR_OVR1, 0, wl_ovr_vbat_monitor_pu, 0x0);
		MOD_RADIO_REG_20692(pi, WL_TEMP_SENS_OVR1, 0, wl_ovr_tempsense_pu, 0x0);

		/* set auxpga vcm ctrl to default value */
		MOD_RADIO_REG_20692(pi, WL_AUXPGA_CFG1, 0, wl_auxpga_i_vcm_ctrl, 0x0);

		/* ensure that the dac mux is OFF because it
		* shares a line with the auxpga output
		*/
		MOD_RADIO_REG_20692(pi, WL_TX_DAC_CFG5, 0, wl_txbb_dac2adc, 0x0);
		MOD_RADIO_REG_20692(pi, WL_TX_DAC_CFG5, 0, wl_txbb_daciso_sw, 0x0);
	}

	/* set envelope detector gain */
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrlOverride0, paCtrlTssiOverride, 1);
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrlOverride0, paCtrlTssiOverrideVal, 1);

	wlc_lcn20phy_pwrctrl_rssiparams(pi);

	/* disable override  */
	PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1, afe_iqadc_aux_en_ovr, 0);
}

static void
wlc_lcn20phy_perpkt_idle_tssi_est(phy_info_t *pi)
{
	bool suspend;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	uint16 SAVE_txpwrctrl;
	uint8 SAVE_indx;

	suspend = !(R_REG(pi->sh->osh, &((phy_info_t *)pi)->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	SAVE_txpwrctrl = wlc_lcn20phy_get_tx_pwr_ctrl(pi);
	SAVE_indx = wlc_lcn20phy_get_current_tx_pwr_idx(pi);

	wlc_lcn20phy_set_tx_pwr_ctrl(pi, LCN20PHY_TX_PWR_CTRL_OFF);

	wlc_lcn20phy_tssi_setup(pi);

	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlCmdNew, txPwrCtrlScheme, 0);

	wlc_lcn20phy_set_txpwr_clamp(pi);

	PHY_REG_MOD(pi, LCN20PHY, RFOverride0, internalrftxpu_ovr, 0);

	wlc_lcn20phy_set_tx_pwr_by_index(pi, SAVE_indx);
	wlc_lcn20phy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRangeCmd, cckPwrOffset,
		pi_lcn20->cckPwrOffset + (pi_lcn20->cckPwrIdxCorr<<1));

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

static void
wlc_lcn20phy_save_idletssi(phy_info_t *pi, uint16 idleTssi0_regvalue_2C)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	switch (wlc_phy_chanspec_bandrange_get(pi, pi->radio_chanspec)) {
		case WL_CHAN_FREQ_RANGE_2G:
			pi_lcn20->idletssi0_cache.idletssi_2g =
			idleTssi0_regvalue_2C;
			break;
		default:
			PHY_ERROR(("wl%d: %s: Bad channel/band\n",
				pi->sh->unit, __FUNCTION__));
			break;
	}
}

static uint8
wlc_lcn20phy_get_bbmult(phy_info_t *pi)
{
	uint16 m0m1;
	phytbl_info_t tab;

	tab.tbl_ptr = &m0m1;
	tab.tbl_len = 1;
	tab.tbl_id = LCN20PHY_TBL_ID_IQLOCAL;
	tab.tbl_offset = 87;
	tab.tbl_width = 16;
	wlc_lcn20phy_read_table(pi, &tab);

	return (uint8)((m0m1 & 0xff00) >> 8);
}

static void
wlc_lcn20phy_get_tx_gain(phy_info_t *pi, phy_txgains_t *gains)
{
	uint16 dac_gain;

	dac_gain = phy_reg_read(pi, LCN20PHY_AfeDACCtrl) >>
		LCN20PHY_AfeDACCtrl_dac_ctrl_SHIFT;
	gains->dac_gain = (dac_gain & LCN20PHY_DACGAIN_MASK) >> 7;

	{
		uint16 rfgain0, rfgain1;

		rfgain0 = (phy_reg_read(pi, LCN20PHY_txgainctrlovrval0) &
			LCN20PHY_txgainctrlovrval0_txgainctrl_ovr_val0_MASK) >>
			LCN20PHY_txgainctrlovrval0_txgainctrl_ovr_val0_SHIFT;
		rfgain1 = (phy_reg_read(pi, LCN20PHY_txgainctrlovrval1) &
			LCN20PHY_txgainctrlovrval1_txgainctrl_ovr_val1_MASK) >>
			LCN20PHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT;

		gains->gm_gain = rfgain0 & 0xff;
		gains->pga_gain = (rfgain0 >> 8) & 0xff;
		gains->pad_gain = rfgain1 & 0xff;
	}
}

static void
wlc_lcn20phy_restore_idletssi(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	switch (wlc_phy_chanspec_bandrange_get(pi, pi->radio_chanspec)) {
		case WL_CHAN_FREQ_RANGE_2G:
			PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlIdleTssi,
			idleTssi0, pi_lcn20->idletssi0_cache.idletssi_2g);
			break;
		default:
			PHY_ERROR(("wl%d: %s: Bad channel/band\n",
				pi->sh->unit, __FUNCTION__));
			break;
	}
}

static void
wlc_lcn20phy_idle_tssi_est(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	bool suspend, tx_gain_override_old;
	phy_txgains_t old_gains;
	uint8 SAVE_bbmult;
	uint16 idleTssi, idleTssi0_2C, idleTssi0_OB, idleTssi0_regvalue_OB, idleTssi0_regvalue_2C;
	uint16 SAVE_txpwrctrl;
	uint8 SAVE_indx;

	suspend = !(R_REG(pi->sh->osh, &((phy_info_t *)pi)->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	idleTssi = phy_reg_read(pi, LCN20PHY_TxPwrCtrlStatus);

	SAVE_txpwrctrl = wlc_lcn20phy_get_tx_pwr_ctrl(pi);
	SAVE_indx = wlc_lcn20phy_get_current_tx_pwr_idx(pi);
	SAVE_bbmult = wlc_lcn20phy_get_bbmult(pi);

	/* Save old tx gains if needed */
	tx_gain_override_old = wlc_lcn20phy_tx_gain_override_enabled(pi);
	wlc_lcn20phy_get_tx_gain(pi, &old_gains);

	wlc_lcn20phy_set_tx_pwr_ctrl(pi, LCN20PHY_TX_PWR_CTRL_OFF);

	/* set txgain override */
	wlc_lcn20phy_enable_tx_gain_override(pi);
	wlc_lcn20phy_set_tx_pwr_by_index(pi, 127);

	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverride, 1);
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverrideVal, 1);

	wlc_lcn20phy_tssi_setup(pi);
	/* Restore TSSI if
	* 1. cal is not possible
	* 2. idle TSSI for the current band/subband is valid
	*/
	if (wlc_phy_no_cal_possible(pi)) {
		phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
		int range = wlc_phy_chanspec_bandrange_get(pi, pi->radio_chanspec);

		if ((range == WL_CHAN_FREQ_RANGE_2G) &&
			(pi_lcn20->idletssi0_cache.idletssi_2g)) {
			wlc_lcn20phy_restore_idletssi(pi);
			goto cleanIdleTSSI;
		}
	}

	wlc_lcn20phy_set_bbmult(pi, 0x0);

	wlc_btcx_override_enable(pi);
	wlc_phy_do_dummy_tx(pi, TRUE, OFF);
	/* Disable WLAN priority */
	wlc_phy_btcx_override_disable(pi);

	idleTssi = PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlStatus, estPwr);
	/* avgTssi value is in 2C (S9.0) format */
	idleTssi0_2C = PHY_REG_READ(pi, LCN20PHY, TxPwrCtrlStatusNew4, avgTssi);

	/* Convert idletssi1_2C from 2C to OB format by toggling MSB OB value */
	/* ranges from 0 to (2^9-1) = 511, 2C value ranges from -256 to (2^9-1-2^8) = 255 */
	/* Convert 9-bit idletssi1_2C to 9-bit idletssi1_OB. */
	if (idleTssi0_2C >= 256)
		idleTssi0_OB = idleTssi0_2C - 256;
	else
		idleTssi0_OB = idleTssi0_2C + 256;
	/* Convert 9-bit idletssi1_OB to 7-bit value for comparison with idletssi */
	if (idleTssi != (idleTssi0_OB >> 2))
		PHY_ERROR(("wl%d: %s, ERROR: idleTssi estPwr(OB): "
			"0x%04x Register avgTssi(OB, 7MSB): 0x%04x\n",
			pi->sh->unit, __FUNCTION__, idleTssi, idleTssi0_OB >> 2));

	idleTssi0_regvalue_OB = idleTssi0_OB;

	if (idleTssi0_regvalue_OB >= 256)
		idleTssi0_regvalue_2C = idleTssi0_regvalue_OB - 256;
	else
		idleTssi0_regvalue_2C = idleTssi0_regvalue_OB + 256;

	/* Write after idletssi1 is calculated since it depends on idleTssi0 set to 0xFF */
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlIdleTssi, idleTssi0, idleTssi0_regvalue_2C);
	/* Cache idle TSSI based on band/subband */
	wlc_lcn20phy_save_idletssi(pi, idleTssi0_regvalue_2C);

cleanIdleTSSI:

	wlc_lcn20phy_set_txpwr_clamp(pi);

	/* Clear tx PU override */
	PHY_REG_MOD(pi, LCN20PHY, RFOverride0, internalrftxpu_ovr, 0);
	wlc_lcn20phy_set_bbmult(pi, SAVE_bbmult);
	/* restore txgain override */
	wlc_lcn20phy_set_tx_gain_override(pi, tx_gain_override_old);
	wlc_lcn20phy_set_tx_gain(pi, &old_gains);
	wlc_lcn20phy_set_tx_pwr_by_index(pi, SAVE_indx);
	wlc_lcn20phy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRangeCmd, cckPwrOffset,
		pi_lcn20->cckPwrOffset + (pi_lcn20->cckPwrIdxCorr<<1));

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

/* Convert tssi to power LUT */
static void
wlc_lcn20phy_set_estPwrLUT(phy_info_t *pi, int32 lut_num)
{
	phytbl_info_t tab;
	int32 tssi;
	uint32 *pwr_table = NULL;
	int32 a1 = 0, b0 = 0, b1 = 0;

	if ((pwr_table = (uint32*) LCN20PHY_MALLOC(pi, 128 * sizeof(uint32))) == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	if (lut_num == 0) {
		/* Get the PA params for the particular channel we are in */
		wlc_phy_get_paparams_for_band(pi, &a1, &b0, &b1);
		 /* estPwrLuts */
		tab.tbl_offset = 0;
	} else {
		b0 = pi->txpa_2g_lo[0];
		b1 = pi->txpa_2g_lo[1];
		a1 = pi->txpa_2g_lo[2];
		 /* estPwrLuts1 */
		tab.tbl_offset = LCN20PHY_TX_PWR_CTRL_EST_PWR_OFFSET;
	}

	for (tssi = 0; tssi < 128; tssi++) {
		*(pwr_table + tssi) = wlc_lcn20phy_tssi2dbm(tssi, a1, b0, b1);
	}

	tab.tbl_id = LCN20PHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_ptr = pwr_table;
	tab.tbl_len = 128;
	wlc_lcn20phy_write_table(pi,  &tab);

	if (pwr_table)
		LCN20PHY_MFREE(pi, pwr_table, 128 * sizeof(uint32));
}

static void
WLBANDINITFN(wlc_lcn20phy_txpwrctrl_init)(phy_info_t *pi)
{
	bool suspend;
	uint8 stall_val;
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	int16 power_correction;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	suspend = !(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	if (NORADIO_ENAB(pi->pubpi)) {
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
		return;
	}

	stall_val = READ_LCN20PHYREGFLD(pi, RxFeCtrl1, disable_stalls);
	LCN20PHY_DISABLE_STALL(pi);

	if (!pi->hwpwrctrl_capable) {
		wlc_lcn20phy_set_tx_pwr_ctrl(pi, LCN20PHY_TX_PWR_CTRL_OFF);
	} else {
		/* Clear out all power offsets */
		wlc_lcn20phy_clear_tx_power_offsets(pi);

		if (pi_lcn20->tssical_time)
			wlc_lcn20phy_perpkt_idle_tssi_est(pi);
		else
			wlc_lcn20phy_idle_tssi_est(pi);

		/* Convert tssi to power LUT */
		wlc_lcn20phy_set_estPwrLUT(pi, 0);

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRangeCmd, pwrMinMaxEnable, 0)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlPwrMinMaxVal, pwrMinVal, 0)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlPwrMinMaxVal, pwrMaxVal, 0)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRangeCmd, txGainTable_mode, 0)
			PHY_REG_MOD_ENTRY(LCN20PHY, TxPwrCtrlRangeCmd, interpol_en, 0)
		PHY_REG_LIST_EXECUTE(pi);

		phy_reg_write(pi, LCN20PHY_TxPwrCtrlDeltaPwrLimit, 10);
		PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRangeCmd, cckPwrOffset,
			pi_lcn20->cckPwrOffset + (pi_lcn20->cckPwrIdxCorr<<1));

		PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlCmdCCK, baseIndex_cck_en, 1);
		PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlCmdCCK, pwrIndex_init_cck, 180);

		PHY_REG_MOD(pi, LCN20PHY, TempSenseCorrection, tempsenseCorr, 0);

		if (pi_lcn20->tssical_time) {
			power_correction = pi_lcn20->tempsenseCorr +
				pi_lcn20->idletssi_corr;
			PHY_REG_MOD(pi, LCN20PHY, TempSenseCorrection,
			tempsenseCorr, power_correction);
		}

		wlc_lcn20phy_set_target_tx_pwr(pi, LCN20_TARGET_PWR);

		/* Caching the inital indicies */
		pi_lcn20->tssi_idx = pi_lcn20->init_txpwrindex;
		pi_lcn20->cck_tssi_idx = pi_lcn20->init_ccktxpwrindex;

		ASSERT(pi_lcn20->tssi_idx > 0);
		ASSERT(pi_lcn20->cck_tssi_idx > 0);

		/* Enable hardware power control */
		wlc_lcn20phy_set_tx_pwr_ctrl(pi, LCN20PHY_TX_PWR_CTRL_HW);
	}

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	LCN20PHY_ENABLE_STALL(pi, stall_val);
}

/* END: TxPwrCtrl Below functions are part of Tx Power Control funtionality */

/* BEGIN: INIT Below functions are part of PHY/RADIO initialization functionality  */

static void
wlc_lcn20phy_phy_and_radio_reset(phy_info_t *pi)
{
	if (NORADIO_ENAB(pi->pubpi))
		return;

	phy_reg_or(pi, LCN20PHY_resetCtrl, 0x100);
	phy_reg_and(pi, LCN20PHY_resetCtrl, ~0x100);

}

static void
WLBANDINITFN(wlc_lcn20phy_tbl_init)(phy_info_t *pi)
{
	uint idx, tbl_info_sz;
	phytbl_info_t *tbl_info = NULL;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	tbl_info_sz = dot11lcn20phytbl_info_sz_rev0;
	tbl_info = (phytbl_info_t *)dot11lcn20phytbl_info_rev0;

	for (idx = 0; idx < tbl_info_sz; idx++)
		wlc_lcn20phy_write_table(pi, &tbl_info[idx]);

	for (idx = 0;
	     idx < dot11lcn20phytbl_2G_rx_gain_info_sz_rev0;
	     idx++) {
		wlc_lcn20phy_write_table(pi,
		    &dot11lcn20phytbl_2G_rx_gain_info_rev0[idx]);
	}
}

static void
WLBANDINITFN(wlc_lcn20phy_rev0_reg_init)(phy_info_t *pi)
{

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LCN20PHY, agcControl4, c_agc_fsm_en, 0)
		PHY_REG_WRITE_ENTRY(LCN20PHY, resetCtrl, 0x004f)
		PHY_REG_WRITE_ENTRY(LCN20PHY, AfeCtrlOvr, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, AfeCtrlOvr1, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, RFOverride0, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, rfoverride2, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, rfoverride3, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, rfoverride4, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, rfoverride7, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, rfoverride8, 0x0000)
		PHY_REG_WRITE_ENTRY(LCN20PHY, swctrlOvr, 0x0000)

		PHY_REG_MOD_ENTRY(LCN20PHY, wl_gain_tbl_offset, wl_gain_tbl_offset, 18)
		PHY_REG_MOD_ENTRY(LCN20PHY, nftrAdj, bt_gain_tbl_offset, 6)

		PHY_REG_MOD_ENTRY(LCN20PHY, RFOverrideVal0, rfpll_pu_ovr_val, 1)
		PHY_REG_MOD_ENTRY(LCN20PHY, RFOverride0, rfpll_pu_ovr, 1)
	PHY_REG_LIST_EXECUTE(pi);

	if (NORADIO_ENAB(pi->pubpi)) {
		PHY_REG_LIST_START
		/* In a 2-chip database although the connection is directly from
			* txbbmux o/p to rxfilt(ACI) i/p, rx decimation chain is somehow still
			* running which causes periodic clk-stalls if not disabled.
			* This will create glitches in the rx signal so we need to disable stalls
			* at all time in QT.
			*/
		PHY_REG_MOD_ENTRY(LCN20PHY, RxFeCtrl1, disable_stalls, 1)

		/* These two may be redundant. */
		PHY_REG_MOD_ENTRY(LCN20PHY, RFOverride0, rfpll_pu_ovr, 0)
		PHY_REG_MOD_ENTRY(LCN20PHY, radio_pu_cfg, rfpll_pu_byp, 1)

		/* Need to set ldo. */
		PHY_REG_MOD_ENTRY(LCN20PHY, RFOverrideVal0, ldos_on_ovr_val, 1)
		PHY_REG_MOD_ENTRY(LCN20PHY, RFOverride0, ldos_on_ovr, 1)

		/* On QT ACI i/p is the txbbmux o/p right-shifted by 2.
		* For 11b frames we need to further scale down the txbbmux o/p to have
		* proper rx magnitude.
		*/
		PHY_REG_MOD_ENTRY(LCN20PHY, BphyControl3, bphyScale, 5)
		PHY_REG_LIST_EXECUTE(pi);
	}

	PHY_REG_LIST_START
		/* Have finished radio, RFPLL stable, so reenable PHY */
		PHY_REG_MOD_ENTRY(LCN20PHY, agcControl4, c_agc_fsm_en, 1)
		PHY_REG_WRITE_ENTRY(LCN20PHY, resetCtrl, 0x0000)
	PHY_REG_LIST_EXECUTE(pi);

	if (NORADIO_ENAB(pi->pubpi)) {
		wlc_lcn20phy_set_bbmult(pi, 128);
		return;
	}

	wlc_lcn20phy_set_tx_pwr_by_index(pi, 40);
}

static void
WLBANDINITFN(wlc_lcn20phy_reg_init)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (LCN20REV_IS(pi->pubpi.phy_rev, 0))
			wlc_lcn20phy_rev0_reg_init(pi);
}

static void
wlc_lcn20phy_aci_init(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	PHY_INFORM(("aci init interference_mode %d\n", pi->sh->interference_mode));
}

static void
WLBANDINITFN(wlc_lcn20phy_baseband_init)(phy_info_t *pi)
{
	PHY_TRACE(("%s:\n", __FUNCTION__));
	/* Initialize LCN20PHY tables */
	wlc_lcn20phy_tbl_init(pi);
	wlc_lcn20phy_reg_init(pi);
	wlc_lcn20phy_aci_init(pi);
}

static void
WLBANDINITFN(wlc_lcn20phy_radio_init)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;
}

static void
WLBANDINITFN(wlc_phy_init_lcn20phy)(phy_info_t *pi)
{
	/* phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy; */

	PHY_TRACE(("%s:\n", __FUNCTION__));

	/* reset the PHY and the radio
	* calls the phy_reset and resetCtrl.radioReset
	* that resets all LDOs
	*/
	wlc_lcn20phy_phy_and_radio_reset(pi);

	/*  set band to g band */
	wlc_lcn20phy_bandset(pi);

	/* Initialize baseband */
	wlc_lcn20phy_baseband_init(pi);

	if (!NORADIO_ENAB(pi->pubpi)) {
		/* initialize the radio registers from 20692_rev0_regs.tcl to chipdefaults */
		wlc_lcn20phy_radio_init(pi);
		wlc_lcn20phy_radio20692_init(pi);
	}

	/* Tune to the current channel */
	wlc_phy_chanspec_set_lcn20phy(pi, pi->radio_chanspec);

	if (NORADIO_ENAB(pi->pubpi)) {
		PHY_REG_MOD(pi, LCN20PHY, RxFeCtrl1, disable_stalls, 1);

		/* Settings ported from DV */
		PHY_REG_WRITE(pi, LCN20PHY, DSSF_C_CTRL, 0);
		PHY_REG_MOD(pi, LCN20PHY, DSSSConfirmCnt, DSSSConfirmCntLoGain, 0x3);
	}
}
/* END: INIT above functions are part of PHY/RADIO initialization functionality  */

static void
wlc_lcn20phy_set_bbmult(phy_info_t *pi, uint8 m0)
{
	uint16 m0m1 = (uint16)m0 << 8;
	phytbl_info_t tab;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = LCN20PHY_TBL_ID_IQLOCAL;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_lcn20phy_write_table(pi, &tab);
}

void
wlc_lcn20phy_deaf_mode(phy_info_t *pi, bool mode)
{
	PHY_REG_MOD(pi, LCN20PHY, agcControl4, c_agc_fsm_en, !mode);
}

/* BEGIN: PLAYTONE Below functions are part of play tone feature */
static void
wlc_lcn20phy_tx_tone_samples(phy_info_t *pi, int32 f_Hz, uint16 max_val, uint32 *data_buf,
	uint32 phy_bw, uint16 num_samps)
{
	fixed theta = 0, rot = 0;
	uint16 i_samp, q_samp, t;
	cint32 tone_samp;
	/* set up params to generate tone */
	rot = FIXED((f_Hz * 36)/(phy_bw * 1000)) / 100; /* 2*pi*f/bw/1000  Note: f in KHz */
	theta = 0;			/* start angle 0 */

	/* tone freq = f_c MHz ; phy_bw = phy_bw MHz ; # samples = phy_bw (1us) ; max_val = 151 */
	/* TCL: set tone_buff [mimophy_gen_tone $f_c $phy_bw $phy_bw $max_val] */
	for (t = 0; t < num_samps; t++) {
		/* compute phasor */
		wlc_phy_cordic(theta, &tone_samp);
		/* update rotation angle */
		theta += rot;
		/* produce sample values for play buffer */
		i_samp = (uint16)(FLOAT(tone_samp.i * max_val) & 0x3ff);
		q_samp = (uint16)(FLOAT(tone_samp.q * max_val) & 0x3ff);
		data_buf[t] = (i_samp << 10) | q_samp;
	}
}

void
wlc_lcn20phy_stop_tx_tone(phy_info_t *pi)
{
	int16 playback_status, mask;
	int cnt = 0;
	pi->phy_tx_tone_freq = 0;

	/* Stop sample buffer playback */
	playback_status = READ_LCN20PHYREG(pi, sampleStatus);
	mask = LCN20PHY_sampleStatus_NormalPlay_MASK | LCN20PHY_sampleStatus_iqlocalPlay_MASK;
	do {
		playback_status = READ_LCN20PHYREG(pi, sampleStatus);
		if (playback_status & LCN20PHY_sampleStatus_NormalPlay_MASK) {
			wlc_lcn20phy_tx_pu(pi, 0);
			PHY_REG_MOD(pi, LCN20PHY, sampleCmd, stop, 1);
		} else if (playback_status & LCN20PHY_sampleStatus_iqlocalPlay_MASK)
			PHY_REG_MOD(pi, LCN20PHY, iqloCalCmdGctl, iqlo_cal_en, 0);
		OSL_DELAY(1);
		playback_status = READ_LCN20PHYREG(pi, sampleStatus);
		cnt++;
	} while ((cnt < 10) && (playback_status & mask));

	ASSERT(!(playback_status & mask));

	PHY_REG_LIST_START
		/* put back SPB into standby */
		PHY_REG_WRITE_ENTRY(LCN20PHY, sslpnCtrl3, 1)
		/* disable clokc to spb */
		PHY_REG_MOD_ENTRY(LCN20PHY, sslpnCalibClkEnCtrl, samplePlayClkEn, 0)
		/* disable clock to txFrontEnd */
		PHY_REG_MOD_ENTRY(LCN20PHY, sslpnCalibClkEnCtrl, forceaphytxFeclkOn, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Restore all the crs signals to the MAC */
	wlc_lcn20phy_deaf_mode(pi, FALSE);

	if (NORADIO_ENAB(pi->pubpi)) {
		PHY_REG_MOD(pi, LCN20PHY, RxFeCtrl1, disable_stalls, 0);
	}

	PHY_REG_MOD(pi, LCN20PHY, AphyControlAddr, phyloopbackEn, 0);
}

static uint16
wlc_lcn20phy_num_samples(phy_info_t *pi, int32 f_Hz, uint32 phy_bw)
{
	uint16 num_samps, k;
	uint32 bw;

	/* allocate buffer */
	if (f_Hz) {
		k = 1;
		do {
			bw = phy_bw * 1000 * k * 1000;
			num_samps = bw / ABS(f_Hz);
			ASSERT(num_samps <= 256);
			k++;
		} while ((num_samps * (uint32)(ABS(f_Hz))) !=  bw);
	} else
		num_samps = 2;

	return num_samps;
}

/*
 * Play samples from sample play buffer
 */
static void
wlc_lcn20phy_run_samples(phy_info_t *pi,
	uint16 num_samps,
	uint16 num_loops,
	uint16 wait,
	bool iqcalmode)
{
	uint16 playback_status, mask;
	int cnt = 0;

	/* enable clk to txFrontEnd */
	phy_reg_or(pi, LCN20PHY_sslpnCalibClkEnCtrl, 0x8080);

	phy_reg_mod(pi, LCN20PHY_sampleDepthCount,
		LCN20PHY_sampleDepthCount_DepthCount_MASK,
		(num_samps - 1) << LCN20PHY_sampleDepthCount_DepthCount_SHIFT);

	if (num_loops != 0xffff)
		num_loops--;
	phy_reg_mod(pi, LCN20PHY_sampleLoopCount,
		LCN20PHY_sampleLoopCount_LoopCount_MASK,
		num_loops << LCN20PHY_sampleLoopCount_LoopCount_SHIFT);

	phy_reg_mod(pi, LCN20PHY_sampleInitWaitCount,
		LCN20PHY_sampleInitWaitCount_InitWaitCount_MASK,
		wait << LCN20PHY_sampleInitWaitCount_InitWaitCount_SHIFT);

	mask = iqcalmode ? LCN20PHY_sampleStatus_iqlocalPlay_MASK :
		LCN20PHY_sampleStatus_NormalPlay_MASK;
	do {
		if (iqcalmode)
			/* Enable calibration */
			PHY_REG_MOD(pi, LCN20PHY, iqloCalCmdGctl, iqlo_cal_en, 1);
		else
			PHY_REG_MOD(pi, LCN20PHY, sampleCmd, start, 1);
		OSL_DELAY(1);
		cnt++;
		playback_status = READ_LCN20PHYREG(pi, sampleStatus);
	} while ((cnt < 10) && !(playback_status & mask));

	ASSERT((playback_status & mask));

	if (!iqcalmode)
		wlc_lcn20phy_tx_pu(pi, 1);
}

static void
wlc_lcn20phy_set_trsw_override(phy_info_t *pi, uint8 tr_mode, bool agc_reset)
{
	bool rx_switch = 0, tx_switch = 0;

	switch (tr_mode) {
		case LCN20PHY_TRS_TXMODE:
			rx_switch = 0;
			tx_switch = 1;
			break;
		case LCN20PHY_TRS_RXMODE:
			rx_switch = 1;
			tx_switch = 0;
			break;
		case LCN20PHY_TRS_MUTE:
			rx_switch = 0;
			tx_switch = 0;
			break;
		case LCN20PHY_TRS_TXRXMODE:
			rx_switch = 1;
			tx_switch = 1;
			break;
		default:
			PHY_ERROR(("wl%d: %s: Bad tr switch mode\n",
				pi->sh->unit, __FUNCTION__));
			break;
	}

	/* Apply TR switch override settings: */
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4, trsw_rx_pwrup_ovr, 1);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4val, trsw_rx_pwrup_ovr_val, rx_switch);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4, trsw_tx_pwrup_ovr, 1);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4val, trsw_tx_pwrup_ovr_val, tx_switch);

	if (agc_reset)
		wlc_lcn20phy_agc_reset(pi);
}

void
wlc_lcn20phy_tx_pu(phy_info_t *pi, bool bEnable)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (!bEnable) {
		/* Disable TX overrides: */
		/* iPA: Make sure iPA is powered-down first */
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4, papu_ovr, 0);
		/* Mixer, cascode, , .. */
		PHY_REG_MOD(pi, LCN20PHY, RFOverride0, internalrftxpu_ovr, 0);
		/* Power up txlogen (JIRA HW43430-225: txlogen needs to be powered up
		* with "trsw_tx_pwrup_pu", it should have been powered-up by "internalrftxpu_ovr")
		*/
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4, trsw_tx_pwrup_ovr, 0);
		/* DAC and LPF: */
		PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1, dac_pu_ovr, 0);

		/* Disable RX overrides: */
		/* gm/LNA2, mixer: */
		PHY_REG_MOD(pi, LCN20PHY, RFOverride0, internalrfrxpu_ovr, 0);
		/* LNA1: */
		PHY_REG_MOD(pi, LCN20PHY, rfoverride2, slna_pu_ovr, 0);

		/* Disable TR switch overrides: */
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4, trsw_rx_pwrup_ovr, 0);
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4, trsw_tx_pwrup_ovr, 0);
	} else {
		/* Power down receiver: */
		/* Power down gm/LNA2, mixer: */
		PHY_REG_MOD(pi, LCN20PHY, RFOverride0, internalrfrxpu_ovr, 1);
		PHY_REG_MOD(pi, LCN20PHY, RFOverrideVal0, internalrfrxpu_ovr_val, 0);
		/* Power down LNA1: */
		PHY_REG_MOD(pi, LCN20PHY, rfoverride2, slna_pu_ovr, 1);
		PHY_REG_MOD(pi, LCN20PHY, rfoverride2val, slna_pu_ovr_val, 0);

		/* Power up transmitter */
		/* Force the TR switch to transmit */
		wlc_lcn20phy_set_trsw_override(pi, LCN20PHY_TRS_TXMODE, FALSE);

		/* Power up mixer, cascode, , .. */
		phy_reg_mod(pi, LCN20PHY_RFOverrideVal0,
			LCN20PHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK,
			(1 << LCN20PHY_RFOverrideVal0_internalrftxpu_ovr_val_SHIFT));
		phy_reg_mod(pi, LCN20PHY_RFOverride0,
			LCN20PHY_RFOverride0_internalrftxpu_ovr_MASK,
			(1 << LCN20PHY_RFOverride0_internalrftxpu_ovr_SHIFT));

		/* Power up txlogen (JIRA HW43430-225: txlogen needs to be powered up
		* with "trsw_tx_pwrup_pu", it should have been powered-up by "internalrftxpu_ovr")
		*/
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4, trsw_tx_pwrup_ovr, 1);
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4val, trsw_tx_pwrup_ovr_val, 1);

		/* Power up DAC and LPF: */
		PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1, dac_pu_ovr, 1);
		PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1Val, dac_pu_ovr_val, 1);

		/* Power up iPA: Make sure iPA is powered-up only
		* AFTER TR-switch is configured for TX
		*/
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4val, papu_ovr_val, 1);
		PHY_REG_MOD(pi, LCN20PHY, rfoverride4, papu_ovr, 1);
	}
}

/*
* Given a test tone frequency, continuously play the samples. Ensure that num_periods
* specifies the number of periods of the underlying analog signal over which the
* digital samples are periodic
*/
/* equivalent to lcn20phy_play_tone */
void
wlc_lcn20phy_start_tx_tone(phy_info_t *pi, int32 f_Hz, uint16 max_val, bool iqcalmode)
{
	uint8 phy_bw;
	uint16 num_samps;
	uint32 *data_buf;

	phytbl_info_t tab;

	if ((data_buf = LCN20PHY_MALLOC(pi, sizeof(uint32) * 256)) == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	if (NORADIO_ENAB(pi->pubpi)) {
		PHY_REG_MOD(pi, LCN20PHY, RxFeCtrl1, disable_stalls, 1);
	}

	/* Save active tone frequency */
	pi->phy_tx_tone_freq = f_Hz;

	PHY_REG_MOD(pi, LCN20PHY, AphyControlAddr, phyloopbackEn, 1);

	/* Turn off all the crs signals to the MAC */
	wlc_lcn20phy_deaf_mode(pi, TRUE);

	phy_bw = 40;

	num_samps = wlc_lcn20phy_num_samples(pi, f_Hz, phy_bw);

	PHY_INFORM(("wl%d: %s: %d Hz, %d samples\n",
		pi->sh->unit, __FUNCTION__,
		f_Hz, num_samps));

	if (num_samps > 256) {
		PHY_ERROR(("wl%d: %s: Too many samples to fit in SPB\n",
			pi->sh->unit, __FUNCTION__));
		LCN20PHY_MFREE(pi, data_buf, 256 * sizeof(uint32));
		return;
	}

	/* we need to bring SPB out of standby before using it */
	PHY_REG_MOD(pi, LCN20PHY, sslpnCtrl3, sram_stby, 0);

	PHY_REG_MOD(pi, LCN20PHY, sslpnCalibClkEnCtrl, samplePlayClkEn, 1);

	wlc_lcn20phy_tx_tone_samples(pi, f_Hz, max_val, data_buf, phy_bw, num_samps);

	/* lcn20phy_load_sample_table */
	tab.tbl_ptr = data_buf;
	tab.tbl_len = num_samps;
	tab.tbl_id = LCN20PHY_TBL_ID_SAMPLEPLAY;
	tab.tbl_offset = 0;
	tab.tbl_width = 32;
	wlc_lcn20phy_write_table(pi, &tab);
	/* play samples from the sample play buffer */
	wlc_lcn20phy_run_samples(pi, num_samps, 0xffff, 0, iqcalmode);
	LCN20PHY_MFREE(pi, data_buf, 256 * sizeof(uint32));
}

void
wlc_lcn20phy_set_tx_tone_and_gain_idx(phy_info_t *pi)
{
	/* Force WLAN antenna */
	wlc_btcx_override_enable(pi);

	if (LCN20PHY_TX_PWR_CTRL_OFF != wlc_lcn20phy_get_tx_pwr_ctrl(pi)) {
		int8 curr_pwr_idx_val;
		curr_pwr_idx_val = wlc_lcn20phy_get_current_tx_pwr_idx(pi);
		wlc_lcn20phy_set_tx_pwr_by_index(pi, (int)curr_pwr_idx_val);
	}

	phy_reg_write(pi, LCN20PHY_sslpnCalibClkEnCtrl, 0xffff);
	wlc_lcn20phy_start_tx_tone(pi, pi->phy_tx_tone_freq, 120, 0); /* play tone */
}
/* END: PLAYTONE Above functions are part of play tone feature */

/* BEGIN: DCCAL Below functions are part of DC calibration feature */

/* initialize dcoe, idacc registers and tables
* arguments
* init_dcoe: 1 to initialize dcoe related parameters
* init_idacc: 1 to initialize idacc related parameters
* reset_table: 1 to clear tables
*/
static void
wlc_lcn20phy_dccal_init(phy_info_t *pi, uint8 dccal_mode, bool reset_tbl)
{

	/* Below is a place holder to assit table write in one shot */
	uint32 zero_tbl[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	phytbl_info_t tab;

	if (dccal_mode & LCN20PHY_DCCALMODE_DCOE) {
		/* FIXME: do we need to do the read and compare operation or we can do unconditional
		* this perhaps depends on the HW block, what it means to write into these bits
		*/
		phy_reg_mod(pi, LCN20PHY_phyreg2dccal_config_0,
			LCN20PHY_INITDCOE_MASK, LCN20PHY_INITDCOE_VAL);

		/* clear dcoe table */
		if (reset_tbl) {
			phytbl_info_t tab;
			tab.tbl_ptr = zero_tbl;
			tab.tbl_len = 12;
			tab.tbl_id = LCN20PHY_TBL_ID_DCOE;
			tab.tbl_width = 32;
			tab.tbl_offset = 0;
			wlc_lcn20phy_write_table(pi, &tab);
		}

		/* clear dcoe_done */
		PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_config_8, ld_dcoe_done, 0);
		PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_control_1, override_dcoe_done, 1);
	}

	if (dccal_mode & LCN20PHY_DCCALMODE_IDACC) {
		/* idac_gmap: entry format
		  * MSB->LSB {LNA1 bypass, LNA2 gm[3:0], LNA1 low-ct, LNA1 gm[2:0], LNA1 Rout[3:0]}
		  */
		uint16 idac_gmap[8] = {41, 944, 960, 825, 841, 857, 854, 848};
		uint16 mask;

		/* FIXME: do we need to do the read and compare operation or we can do unconditional
		* this perhaps depends on the HW block, what it means to write into these bits
		*/
		phy_reg_mod(pi, LCN20PHY_phyreg2dccal_config_2,
			LCN20PHY_INITIDACC_MASK, LCN20PHY_INITIDACC_VAL);

		/* re-init value for idac_cal_done */
		PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_config_3, idacc_done_init, 1);

		/* loading IDAC GMAP table */
		tab.tbl_ptr = idac_gmap;
		tab.tbl_len = 8;
		tab.tbl_id = LCN20PHY_TBL_ID_IDACGMAP;
		tab.tbl_width = 16;
		tab.tbl_offset = 0;
		wlc_lcn20phy_write_table(pi, &tab);

		/* clear idac table */
		if (reset_tbl) {
			tab.tbl_ptr = zero_tbl;
			tab.tbl_len = 8;
			tab.tbl_id = LCN20PHY_TBL_ID_IDAC;
			tab.tbl_width = 32;
			tab.tbl_offset = 0;
			wlc_lcn20phy_write_table(pi, &tab);
		}

		/* set idac_cal_done to 0x01 */
		PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_config_11, ld_idac_cal_done, 1);
		PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_control_1, override_idac_cal_done, 1);

		/* init override values */
		mask = (LCN20PHY_rfoverride2_tia_offset_dac_pu_ovr_MASK |
			LCN20PHY_rfoverride2_tia_offset_dac_ovr_MASK |
			LCN20PHY_rfoverride2_tia_offset_dac_sign_ovr_MASK);
		_PHY_REG_MOD(pi, LCN20PHY_rfoverride2, mask, 0);
		PHY_REG_MOD(pi, LCN20PHY, rfoverride2val, tia_offset_dac_pu_ovr_val, 1);

		/* init biasadj */
		MOD_RADIO_REG_20692(pi, WL_TIA_CFG8, 0, wl_tia_offset_dac_biasadj, 2);

		/* set RF override mix and div2 pu values appropriately */
		MOD_RADIO_REG_20692(pi, WL_RXMIX2G_CFG1, 0, wl_rxmix2g_pu, 0x1);
		MOD_RADIO_REG_20692(pi, WL_RXRF2G_CFG1, 0, wl_rxdiv2g_rs, 0x0);
		MOD_RADIO_REG_20692(pi, WL_RXRF2G_CFG1, 0, wl_rxdiv2g_pu_bias, 0x1);

		/* set override control to 0 for now; may want to change this later */
		mask = RF_20692_WL_RX_TOP_2G_OVR1_wl_ovr_rxmix2g_pu_MASK(0) |
			RF_20692_WL_RX_TOP_2G_OVR1_wl_ovr_rxdiv2g_rs_MASK(0) |
			RF_20692_WL_RX_TOP_2G_OVR1_wl_ovr_rxdiv2g_pu_bias_MASK(0);
		_MOD_RADIO_REG(pi, RF0_20692_WL_RX_TOP_2G_OVR1(0), mask, 0);
	}
}

/* enable dccal functions by setting bypass bits and idacc_*_reinit bits
* arguments
* mode[2:0] = {idact, idacc, dcoe}, set bit to 1 to enable particular function
* idacc_reinit_mode: disable reinits 0, per-packet reinit 1
*					tx2rx_only 4, tx2rx_reinit+tx2rx_only 6
*/
static void
wlc_lcn20phy_dccal_set_mode(phy_info_t *pi, uint8 dccal_mode, uint8 idacc_reinit_mode)
{
	uint16 dccalctrl0_val, dccalctrl0_mask;

	/* Setting the dccal ctrl0
	 * dccal_en is used in RTL as an extra gating signal for
	 * dccal_reqd/rdy signals in dcctrigger
	 * set dccal_en to 1 and use *_bypass to switch on/off dcoe/idacc/idact
	 * dccal_en default reset value is 0, so it needs to be set to 1
	 * bt2wl_dccal_mode is used to delay enabling dccal after bt2wl transition;
	 * 0-less delay, 1-more delay
	 */
	dccalctrl0_val = (1 << LCN20PHY_phyreg2dccal_control_0_dccal_en_SHIFT |
		(((dccal_mode & LCN20PHY_BYPASS_MASK) ^ LCN20PHY_BYPASS_MASK)
		<< LCN20PHY_phyreg2dccal_control_0_dcoe_bypass_SHIFT) |
		(1 << LCN20PHY_phyreg2dccal_control_0_bt2wl_dccal_mode_SHIFT));
	dccalctrl0_mask = (LCN20PHY_phyreg2dccal_control_0_dccal_en_MASK |
		LCN20PHY_BYPASS_MASK |
		LCN20PHY_phyreg2dccal_control_0_bt2wl_dccal_mode_MASK);
	PHY_INFORM(("dccal_ctrl0: %d\n", dccalctrl0_val));
	_PHY_REG_MOD(pi, LCN20PHY_phyreg2dccal_control_0, dccalctrl0_mask, dccalctrl0_val);

	_PHY_REG_MOD(pi, LCN20PHY_phyreg2dccal_config_3, LCN20PHY_IDACCMODE_MASK,
		(idacc_reinit_mode << LCN20PHY_phyreg2dccal_config_3_idacc_ppkt_reinit_SHIFT));
}

/* mode 0-7: 3bits {idact,idacc,dcoe} set bit to 1 to run particular cal */
static void
wlc_lcn20phy_dccal_force_cal(phy_info_t *pi, uint8 dccal_mode, int8 num_retry)
{
	bool reset_tbl = 0;

	PHY_TRACE(("%s:\n", __FUNCTION__));

	/* disable dccal in case it is on */
	wlc_lcn20phy_dccal_set_mode(pi, LCN20PHY_DCCALMODE_BYPASS, LCN20PHY_IDACCMODE_DISREINIT);

	/* initialize dcoe and idacc registers only */
	wlc_lcn20phy_dccal_init(pi, dccal_mode, reset_tbl);

	wlc_lcn20phy_dccal_set_mode(pi, dccal_mode, LCN20PHY_IDACCMODE_DISREINIT);

	/* put AGC fsm in HG lock mode, so that there is no RX activity between cals */
	PHY_REG_MOD(pi, LCN20PHY, agcControl4, c_agc_hg_lock, 1);

	do {
	    /* reset pkt_proc to perform dcoe, idacc; assumes PHY is in RX */
		PHY_REG_MOD(pi, LCN20PHY, resetCtrl, pktfsmSoftReset, 1);
		PHY_REG_MOD(pi, LCN20PHY, resetCtrl, pktfsmSoftReset, 0);

		/* wait for DCOE+IDACC to finish, max time depends on settling
		* and accumulation times
		*/
	    SPINWAIT((((dccal_mode & LCN20PHY_DCCALMODE_IDACC) &&
	    (READ_LCN20PHYREGFLD(pi, phyreg2dccal_status, idacc_on))) ||
	    ((dccal_mode & LCN20PHY_DCCALMODE_DCOE) &&
	    (READ_LCN20PHYREGFLD(pi, phyreg2dccal_status, dcoe_on)))),
	    LCN20PHY_SPINWAIT_DCCAL);

		if ((num_retry == 0) ||
			((READ_LCN20PHYREGFLD(pi, phyreg2dccal_config_11a, idac_cal_done)
			== 0xff) &&
			(READ_LCN20PHYREGFLD(pi, phyreg2dccal_config_9, dcoe_done) == 0xfff)))
			num_retry = 0;
		else
			num_retry--;
	} while (num_retry);

	/* remove HG lock on AGC fsm */
	PHY_REG_MOD(pi, LCN20PHY, agcControl4, c_agc_hg_lock, 0);

	/* disable dccal  */
	wlc_lcn20phy_dccal_set_mode(pi, LCN20PHY_DCCALMODE_BYPASS, LCN20PHY_IDACCMODE_DISREINIT);
}
/* Run idac cal for auxlna path by applying a softreset to pkt_proc fsm
* assumes RF is configured to use aux LNA and that the input to aux LNA is 0
* idac cal is run only for index 0; result from idac_table[0] is written to the
* override registers tiaOffsetDacQVal and tiaOffsetDacIVal and idac_table[0] is cleared
*/
static void
wlc_lcn20phy_dccal_auxlna(phy_info_t *pi)
{
	uint16 control_0, config_3, idac_cal_done_save;
	uint32 idac_aux_lna;
	phytbl_info_t tab;
	int8 num_retry;

	/* Disable dccal */
	wlc_lcn20phy_dccal_set_mode(pi, LCN20PHY_DCCALMODE_BYPASS,
		LCN20PHY_IDACCMODE_DISREINIT);

	/* Save registers which will be modified below */
	control_0 = READ_LCN20PHYREG(pi, phyreg2dccal_control_0);
	config_3 = READ_LCN20PHYREG(pi, phyreg2dccal_config_3);
	idac_cal_done_save = READ_LCN20PHYREGFLD(pi, phyreg2dccal_config_11a, idac_cal_done);

	/* Load 0xfe to idac_cal_done to run cal for index 0 only */
	PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_config_11, ld_idac_cal_done, 0xfe);
	PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_control_1, override_idac_cal_done, 1);

	/* To run idac cal only */
	wlc_lcn20phy_dccal_set_mode(pi, LCN20PHY_DCCALMODE_IDACC, LCN20PHY_IDACCMODE_DISREINIT);

	/* put AGC fsm in HG lock mode, so that there is no RX activity between cals */
	PHY_REG_MOD(pi, LCN20PHY, agcControl4, c_agc_hg_lock, 1);

	/* reset pkt_proc to perform dcoe, idacc; assumes PHY is in RX,
	 * adc clock should be on
	 */
	PHY_REG_MOD(pi, LCN20PHY, resetCtrl, pktfsmSoftReset, 1);
	PHY_REG_MOD(pi, LCN20PHY, resetCtrl, pktfsmSoftReset, 0);

	/* wait for IDACC to finish, max time depends on settling and accumulation times */
	num_retry = 5;
	do {
	    /* reset pkt_proc to perform dcoe, idacc; assumes PHY is in RX */
		PHY_REG_MOD(pi, LCN20PHY, resetCtrl, pktfsmSoftReset, 1);
		PHY_REG_MOD(pi, LCN20PHY, resetCtrl, pktfsmSoftReset, 0);

		/* wait for DCOE+IDACC to finish,
		* max time depends on settling and accumulation times
		*/
	    SPINWAIT((READ_LCN20PHYREGFLD(pi, phyreg2dccal_status, idacc_on)),
	    LCN20PHY_SPINWAIT_DCCAL);
		if ((num_retry == 0) ||
			((READ_LCN20PHYREGFLD(pi, phyreg2dccal_config_11a, idac_cal_done) == 0xff)))
			num_retry = 0;
		else
			num_retry--;
	} while (num_retry);

	/* remove HG lock on AGC fsm */
	PHY_REG_MOD(pi, LCN20PHY, agcControl4, c_agc_hg_lock, 0);

	/* read calibrated idac value */
	tab.tbl_ptr = &idac_aux_lna;
	tab.tbl_len = 1;
	tab.tbl_id = LCN20PHY_TBL_ID_IDAC;
	tab.tbl_width = 32;
	tab.tbl_offset = 0;
	wlc_lcn20phy_read_table(pi,  &tab);
	/* write idac values to the override offset dac registers */
	PHY_REG_WRITE(pi, LCN20PHY, tiaOffsetDacQVal, (idac_aux_lna & 0x1ff));
	PHY_REG_WRITE(pi, LCN20PHY, tiaOffsetDacIVal, ((idac_aux_lna >> 9) & 0x1ff));
	PHY_INFORM(("override tiaOffsetDacVal, 0x%3x\n", idac_aux_lna));
	/* clear entry 0 in idac_table */
	idac_aux_lna = 0;
	wlc_lcn20phy_write_table(pi,  &tab);

	/* write back saved parameters */
	PHY_REG_WRITE(pi, LCN20PHY, phyreg2dccal_control_0, control_0);
	PHY_REG_WRITE(pi, LCN20PHY, phyreg2dccal_config_3, config_3);
	PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_config_11, ld_idac_cal_done, idac_cal_done_save);
	PHY_REG_MOD(pi, LCN20PHY, phyreg2dccal_control_1, override_idac_cal_done, 1);
}

/* set/clear the offset_dac override bits
* arguments
* ovr_mode: 0 - clear override bits, 1 - set override bits and set pu_ovr_val to 1
* ovr_idac: 1 - write ovr_idac_value into tiaOffsetDac override registers
* ovr_idac_value: 18bits input to be used to set tiaOffsetDacQVal,
*					tiaOffsetDacIVal when ovr_mode=2
*/
static void
wlc_lcn20phy_dccal_ovr_idac(phy_info_t *pi, bool ovr_mode, bool ovr_idac, uint32 ovr_idac_value)
{
	uint16 mask = (LCN20PHY_rfoverride2_tia_offset_dac_pu_ovr_MASK |
		LCN20PHY_rfoverride2_tia_offset_dac_ovr_MASK |
		LCN20PHY_rfoverride2_tia_offset_dac_sign_ovr_MASK);

	if (!ovr_mode) {
		/* clear ovr bits */
		_PHY_REG_MOD(pi, LCN20PHY_rfoverride2, mask, 0);
	} else {
		/* set pu_ovr_val to 1 */
		PHY_REG_MOD(pi, LCN20PHY, rfoverride2val, tia_offset_dac_pu_ovr_val, 1);
		/* set ovr bits */
		_PHY_REG_MOD(pi, LCN20PHY_rfoverride2, mask, 7);
	}

	if (ovr_idac) {
		/* write idac values to the override offset dac registers */
		PHY_REG_WRITE(pi, LCN20PHY, tiaOffsetDacQVal, (ovr_idac_value & 0x1ff));
		PHY_REG_WRITE(pi, LCN20PHY, tiaOffsetDacIVal, ((ovr_idac_value >> 9) & 0x1ff));
	}
}
/* END:    DCCAL Above functions are part of play tone feature */

/* START: RXIQCAL Below functions are part of Rx IQ Calibrations  feature */

static void
wlc_lcn20phy_rx_iq_cal_phy_setup(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	lcn20phy_rxiqcal_phyregs_t *psave = &(pi_lcn20->rxiqcal_phyregs);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Make sure we didn't call the setup twice */
	ASSERT(!psave->is_orig);
	psave->is_orig = TRUE;

	/* Save RFCTRL state, save all RFCTRL override register */
	psave->RFOverride0 = READ_LCN20PHYREG(pi, RFOverride0);
	psave->RFOverrideVal0 = READ_LCN20PHYREG(pi, RFOverrideVal0);
	psave->rfoverride2 = READ_LCN20PHYREG(pi, rfoverride2);
	psave->rfoverride2val = READ_LCN20PHYREG(pi, rfoverride2val);
	psave->rfoverride4 = READ_LCN20PHYREG(pi, rfoverride4);
	psave->rfoverride4val = READ_LCN20PHYREG(pi, rfoverride4val);
	psave->rfoverride7 = READ_LCN20PHYREG(pi, rfoverride7);
	psave->rfoverride7val = READ_LCN20PHYREG(pi, rfoverride7val);
	psave->rfoverride8 = READ_LCN20PHYREG(pi, rfoverride8);
	psave->rfoverride8val = READ_LCN20PHYREG(pi, rfoverride8val);

	/* Save tx gain state
	* save bb_mult, txgain, papr/IIR related parameter
	*/
	psave->bbmult = wlc_lcn20phy_get_bbmult(pi);
	/* Turn off tx pwr ctrl */
	psave->SAVE_txpwrctrl_on = wlc_lcn20phy_get_tx_pwr_ctrl(pi);
	wlc_lcn20phy_set_tx_pwr_ctrl(pi, LCN20PHY_TX_PWR_CTRL_OFF);

	/* temporarily turn off PAPD in case it is enabled, disable PAPR */
	psave->PapdEnable0 = READ_LCN20PHYREG(pi, PapdEnable0);
	psave->papr_ctrl = READ_LCN20PHYREG(pi, papr_ctrl);
	PHY_REG_MOD(pi, LCN20PHY, PapdEnable0, papd_compEnb, 0);
	PHY_REG_MOD(pi, LCN20PHY, papr_ctrl, papr_blk_en, 0);

	/* save Rx/Tx farrow related register setting and DVGA2 related register setting */
	psave->RxSdFeConfig1 = READ_LCN20PHYREG(pi, RxSdFeConfig1);
	psave->RxSdFeConfig6 = READ_LCN20PHYREG(pi, RxSdFeConfig6);
	psave->phyreg2dvga2 = READ_LCN20PHYREG(pi, phyreg2dvga2);
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig1, farrow_rshift_force, 1);
	PHY_REG_MOD(pi, LCN20PHY, phyreg2dvga2, dvga2_gain_ovr, 1);

	/* enable LOFT comp, enable TX IQ comp */
	psave->SAVE_Core1TxControl = READ_LCN20PHYREG(pi, Core1TxControl);
	/* enable LOFT comp and TX IQ Ccmp */
	PHY_REG_MOD(pi, LCN20PHY, Core1TxControl, iqImbCompEnable, 1);
	PHY_REG_MOD(pi, LCN20PHY, Core1TxControl, loft_comp_en, 1);

	/* ensure the cal clock has been enabled */
	psave->sslpnCalibClkEnCtrl = READ_LCN20PHYREG(pi, sslpnCalibClkEnCtrl);
	PHY_REG_WRITE(pi, LCN20PHY, sslpnCalibClkEnCtrl, 0x0068);

	/* save DSSF control register */
	psave->DSSF_control_0 = READ_LCN20PHYREG(pi, DSSF_control_0);

	/* Disable DSSF as it can suppress reference loopback tone or its image,  restore it */
	PHY_REG_MOD(pi, LCN20PHY, DSSF_control_0, enabled_s1, 0);
	PHY_REG_MOD(pi, LCN20PHY, DSSF_control_0, enabled_s2, 0);
}

static void
wlc_lcn20phy_rx_gain_override(phy_info_t *pi, uint16 slna_byp, uint16 slna_rout,
	uint16 slna_gain, uint16 lna2_gain,
    uint16 tia, uint16 dvga1_gain, uint16 dvga2_gain)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	PHY_REG_MOD(pi, LCN20PHY, rfoverride2val, slna_byp_ovr_val, slna_byp);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride3_val, slna_rout_ctrl_ovr_val, slna_rout);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride2val, slna_gain_ctrl_ovr_val, slna_gain);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride5val, rxrf_lna2_gain_ovr_val, lna2_gain);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride5val, rxrf_tia_gain_ovr_val, tia);
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig1, rx_farrow_rshift_0, dvga1_gain);
	PHY_REG_MOD(pi, LCN20PHY, phyreg2dvga2, dvga2_gain_ovr_val, dvga2_gain);
}

static void
wlc_lcn20phy_rx_gain_override_enable(phy_info_t *pi, bool enable)
{
	uint16 ebit = enable ? 1 : 0;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	PHY_REG_MOD(pi, LCN20PHY, rfoverride2, slna_byp_ovr, ebit);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride3, slna_rout_ctrl_ovr, ebit);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride2, slna_gain_ctrl_ovr, ebit);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride5, rxrf_lna2_gain_ovr, ebit);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride5, rxrf_tia_gain_ovr, ebit);
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig1, farrow_rshift_force, ebit);
	PHY_REG_MOD(pi, LCN20PHY, phyreg2dvga2, dvga2_gain_ovr, ebit);
}

static void
wlc_lcn20phy_rx_iq_est(phy_info_t *pi, phy_iq_est_t *est, uint16 num_samps,
	uint8 wait_time, uint8 wait_for_crs, bool noise_cal)
{
	uint32 timeout_us;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	timeout_us = LCN20PHY_SPINWAIT_IQEST_USEC;
	if (NORADIO_ENAB(pi->pubpi)) {
		timeout_us = LCN20PHY_SPINWAIT_IQEST_QT_USEC;
	}

	/* Get Rx IQ Imbalance Estimate from modem */
	PHY_REG_MOD(pi, LCN20PHY, IqestSampleCount, NumSampToCol, num_samps);
	PHY_REG_MOD(pi, LCN20PHY, IqestWaitTime, waitTime, wait_time);
	PHY_REG_MOD(pi, LCN20PHY, IqestCmd, iqMode, wait_for_crs);
	PHY_REG_MOD(pi, LCN20PHY, IqestCmd, iqstart, 1);

	/* wait for estimate */
	SPINWAIT((READ_LCN20PHYREGFLD(pi, IqestCmd, iqstart) != 0), timeout_us);

	if (READ_LCN20PHYREGFLD(pi, IqestCmd, iqstart) == 0) {
		est->i_pwr = (READ_LCN20PHYREGFLD(pi, IqestipwrAccHi, ipwrAccHi) << 16) |
			READ_LCN20PHYREGFLD(pi, IqestipwrAccLo, ipwrAccLo);
		est->q_pwr = (READ_LCN20PHYREGFLD(pi, IqestqpwrAccHi, qpwrAccHi) << 16) |
			READ_LCN20PHYREGFLD(pi, IqestqpwrAccLo, qpwrAccLo);
		est->iq_prod = (READ_LCN20PHYREGFLD(pi, IqestIqAccHi, iqAccHi) << 16) |
			READ_LCN20PHYREGFLD(pi, IqestIqAccLo, iqAccLo);
		PHY_INFORM(("wlc_lcn20phy_rx_iq_est: "
				  "i_pwr = %u, q_pwr = %u, iq_prod = %d\n",
				  est->i_pwr, est->q_pwr,
				  est->iq_prod));
	} else {
		PHY_ERROR(("wl%d: %s: IQ measurement timed out\n", pi->sh->unit, __FUNCTION__));
	}
}

static void
wlc_lcn20phy_rx_iq_cal_txrxgain_control(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	const lcn20phy_rxcal_rxgain_t *gaintbl;
	uint8 g_index, done, found_ideal, wn, txindex;
	bool txdone;
	uint8 do_max = 8;
	uint8 tia, far;
	uint16 num_samps = 1024;
	uint32 meansq_max = 7000;
	uint32 meansq_min = 1111;
	uint32 i_meansq, q_meansq;
	uint8 dvga2 = 0;
	/* the gain range is from +8dBm to 20dBm, each step is 3 dB */
	const uint8 txindx_start = 104;
	const uint8 txindx_stop  = 56;
	const uint8 txindx_step  = 12;
	int p;
	phy_iq_est_t est = {0, 0, 0};

	ASSERT(pi_lcn20->rxiqcal_lpbkpath <= LCN20PHY_RXIQCAL_PAPDPATH);

	if (pi_lcn20->rxiqcal_lpbkpath == LCN20PHY_RXIQCAL_PAPDPATH)
		gaintbl = gaintbl_papdpath;
	else
		gaintbl = gaintbl_lkpath;

	g_index = 8;
	done = 0;
	found_ideal = 0;
	txdone = 0;
	wn = 0;
	txindex = txindx_start;

	wlc_lcn20phy_start_tx_tone(pi, (2*1000*1000), LCN20PHY_RXIQCAL_TONEAMP, 0);

	while ((done != do_max) && (g_index != 0) && (g_index != LCN20PHY_RXCAL_NUMRXGAINS)) {
		if (pi_lcn20->rxiqcal_lpbkpath == LCN20PHY_RXIQCAL_PAPDPATH) {
			/* papd loopback path */
			tia = gaintbl[g_index].tia;
			far = gaintbl[g_index].far;
			dvga2 = gaintbl[g_index].dvga;
		} else {
			/* leakage path for 5G */
			/* lna = gaintbl[g_index].lna; */
			tia = gaintbl[g_index].tia;
			far = gaintbl[g_index].far;
		}
		wlc_lcn20phy_rx_gain_override(pi, 0, 0, 0, 0, tia, far, dvga2);
		wlc_lcn20phy_rx_gain_override_enable(pi, TRUE);

		if (!txdone)
			wlc_lcn20phy_set_tx_pwr_by_index(pi, txindex);

		/* RSSI reading */
		for (p = 0; p < 8; p++) {
			wn +=  READ_RADIO_REGFLD_20692(pi, WL_TIA_CFG13, 0, nbrssi_Ich_low);
			wn +=  READ_RADIO_REGFLD_20692(pi, WL_TIA_CFG13, 0, nbrssi_Qch_low);
		}

		/* estimate digital power using rx_iq_est */
		wlc_lcn20phy_rx_iq_est(pi, &est, num_samps, 32, 0, FALSE);

		i_meansq = (est.i_pwr + num_samps / 2) / num_samps;
		q_meansq = (est.q_pwr + num_samps / 2) / num_samps;

		PHY_INFORM(("Rx IQCAL: txindx=%d g_index=%d tia=%d far=%d dvga=%d\n",
			txindex, g_index, tia, far, dvga2));
		PHY_INFORM(("Rx IQCAL: i_meansq=%d q_meansq=%d meansq_max=%d meansq_min=%d\n",
			i_meansq, q_meansq, meansq_max, meansq_min));

		txdone = txdone ||
			(txindex < txindx_stop) || (wn > 0) || (i_meansq > meansq_min) ||
			(q_meansq > meansq_min);

		if (!txdone) {
			txindex -= txindx_step;
			continue;
		}

		if ((i_meansq > meansq_max) || (q_meansq > meansq_max)) {
			g_index--;
			done++;
		} else if ((i_meansq < meansq_max) && (q_meansq < meansq_min)) {
			g_index++;
			done++;
		} else {
			done = do_max;
			found_ideal = 1;
		}
	}
	if (!found_ideal) {
		PHY_ERROR(("%s: Too much or too little power? (gain_index=%d)\n",
		           __FUNCTION__, g_index));
	}
	wlc_lcn20phy_stop_tx_tone(pi);
}

static void
wlc_lcn20phy_rx_iq_cal_loopback_setup(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	wlc_lcn20phy_deaf_mode(pi, TRUE);
	wlc_lcn20phy_rx_iq_cal_phy_setup(pi);

	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_lcn20phy_radio20692_rx_iq_cal_setup(pi);
	}

	if (pi_lcn20->dccalen_lpbkpath) {
		wlc_lcn20phy_start_tx_tone(pi, (2*1000*1000), LCN20PHY_RXIQCAL_TONEAMP, 0);
		wlc_lcn20phy_set_bbmult(pi, 0);
		wlc_lcn20phy_dccal_auxlna(pi);
		wlc_lcn20phy_dccal_ovr_idac(pi, 1, 0, 0);
		wlc_lcn20phy_stop_tx_tone(pi);
	}
	wlc_lcn20phy_rx_iq_cal_txrxgain_control(pi);
}

static void
wlc_lcn20phy_rx_iq_comp(phy_info_t *pi, uint8 setmode, phy_iq_comp_t *pcomp)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* write values */
	switch (setmode) {
		case LCN20PHY_RXIQCOMP_GET:
			pcomp->a = READ_LCN20PHYREG(pi, Core1RxIQCompA0);
			pcomp->b = READ_LCN20PHYREG(pi, Core1RxIQCompB0);
			break;
		case LCN20PHY_RXIQCOMP_SET:
			WRITE_LCN20PHYREG(pi, Core1RxIQCompA0, pcomp->a);
			WRITE_LCN20PHYREG(pi, Core1RxIQCompB0, pcomp->b);
			break;
		case LCN20PHY_RXIQCOMP_RESET:
			WRITE_LCN20PHYREG(pi, Core1RxIQCompA0, 0);
			WRITE_LCN20PHYREG(pi, Core1RxIQCompB0, 0);
			break;
		default:
			ASSERT(setmode <= 2);
			break;
	}
}

static void
wlc_lcn20phy_calc_iq_mismatch(phy_info_t *pi, phy_iq_est_t *est, lcn20phy_iq_mismatch_t *mismatch)
{
	int32  iq = est->iq_prod;
	uint32 ii = est->i_pwr;
	uint32 qq = est->q_pwr;

	int16  iq_nbits, qq_nbits, ii_nbits;
	int32  tmp;
	int32  den, num;
	int32  angle;
	cint32 val;

	/* angle = asin (-iq / sqrt( ii*qq ))
	* mag   = sqrt ( qq/ii )
	*/
	iq_nbits = wlc_phy_nbits(iq);
	qq_nbits = wlc_phy_nbits(qq);
	ii_nbits = wlc_phy_nbits(ii);
	if (ii_nbits > qq_nbits)
		qq_nbits = ii_nbits;

	if (30 >=  qq_nbits) {
		tmp = ii;
		tmp = tmp << (30 - qq_nbits);
		den = (int32) wlc_phy_sqrt_int((uint32) tmp);
		tmp = qq;
		tmp = tmp << (30 - qq_nbits);
		den *= (int32) wlc_phy_sqrt_int((uint32) tmp);
	} else {
		tmp = ii;
		tmp = tmp >> (qq_nbits - 30);
		den = (int32) wlc_phy_sqrt_int((uint32) tmp);
		tmp = qq;
		tmp = tmp >> (qq_nbits - 30);
		den *= (int32) wlc_phy_sqrt_int((uint32) tmp);
	}
	if (qq_nbits <= iq_nbits + 16) {
		den = den >> (16 + iq_nbits - qq_nbits);
	} else {
		den = den << (qq_nbits - (16 + iq_nbits));
	}

	tmp = -iq;
	num = (tmp << (30 - iq_nbits));
	if (num > 0)
		num += (den >> 1);
	else
		num -= (den >> 1);

	if (den == 0) {
		tmp = 0;
	} else {
		tmp = num / den; /* in X,16 */
	}

	mismatch->sin_angle = tmp;

	tmp = (tmp >> 1);
	tmp *= tmp;
	tmp = (1 << 30) - tmp;
	val.i = (int32) wlc_phy_sqrt_int((uint32) tmp);
	val.i = ( val.i << 1) ;

	val.q = mismatch->sin_angle;
	wlc_phy_inv_cordic(val, &angle);
	mismatch->angle = angle; /* in X,16 */

	iq_nbits = wlc_phy_nbits(qq - ii);
	if (iq_nbits % 2 == 1)
		iq_nbits++;

	den = ii;

	num = qq - ii;
	num = num << (30 - iq_nbits);
	if (iq_nbits > 10)
		den = den >> (iq_nbits - 10);
	else
		den = den << (10 - iq_nbits);
	if (num > 0)
		num += (den >> 1);
	else
		num -= (den >> 1);

	if (den == 0) {
		mismatch->mag = (1 << 10); /* in X,10 */
	} else {
		tmp = num / den + (1 << 20);
		mismatch->mag = (int32) wlc_phy_sqrt_int((uint32) tmp); /* in X,10 */
	}
	PHY_INFORM(("      Mag=%d, Angle=%d, cos(angle)=%d, sin(angle)=%d\n",
	(int)mismatch->mag, (int)mismatch->angle, (int)val.i, (int)val.q));
}

/*
* convert ii qq, iq to [mag phase]
*
* FIXME: We should be able to get rid of this wrapper function and use
*  wlc_lcn20phy_calc_iq_mismatch() directly
*/
static void
wlc_lcn20phy_rxiqcal_pnp(phy_info_t *pi, phy_iq_est_t *iqest, int32 *angle, int32 *mag)
{
	lcn20phy_iq_mismatch_t mismatch;

	wlc_lcn20phy_calc_iq_mismatch(pi, iqest, &mismatch);
	*angle = mismatch.angle;
	*mag = mismatch.mag;
}

static void
wlc_lcn20phy_lin_reg(phy_info_t *pi, lcn20phy_rx_fam_t *freq_ang_mag, uint16 num_data)
{
	int32 Sf2 = 0;
	int32 Sfa, Sa, Sm;
	int32 intcp, mag;
	int8 idx;
	phy_iq_comp_t coeffs;
	int32 sin_angle, cos_angle;
	cint32 cordic_out;
	int32  a, b, sign_sa;

	Sfa = 0; Sa = 0; Sm = 0;

	for (idx = 0; idx < num_data; idx++) {
		Sf2 += freq_ang_mag[idx].freq * freq_ang_mag[idx].freq;

		Sfa += freq_ang_mag[idx].freq * freq_ang_mag[idx].angle;
		Sa += freq_ang_mag[idx].angle;
		Sm += freq_ang_mag[idx].mag;
	}

	sign_sa = Sa >= 0 ? 1 : -1;
	intcp = (Sa + sign_sa * (num_data >> 1)) / num_data;
	mag   = (Sm + (num_data >> 1)) / num_data;

	wlc_phy_cordic(intcp, &cordic_out);
	sin_angle = cordic_out.q;
	cos_angle = cordic_out.i;

	b = mag * cos_angle;
	a = mag * sin_angle;

	b = ((b >> 15) + 1) >> 1;
	b -= (1 << 10);  /* 10 bit */
	a = ((a >> 15) + 1) >> 1;

	a = (a < -512) ? -512 : ((a > 511) ? 511 : a);
	b = (b < -512) ? -512 : ((b > 511) ? 511 : b);

	coeffs.a = a & 0x3ff;
	coeffs.b = b & 0x3ff;

	PHY_INFORM(("   a=%d b=%d :: ", a, b));

	wlc_lcn20phy_rx_iq_comp(pi, LCN20PHY_RXIQCOMP_SET, &coeffs);
}

static void
wlc_lcn20phy_rx_iq_cal_phy_cleanup(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	lcn20phy_rxiqcal_phyregs_t *psave = &(pi_lcn20->rxiqcal_phyregs);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Make sure we didn't call the setup twice */
	ASSERT(!psave->is_orig);
	psave->is_orig = FALSE;

	/* PAPD, PAPR, TX gain, TX power control restore */
	wlc_lcn20phy_set_tx_pwr_ctrl(pi, psave->SAVE_txpwrctrl_on);
	WRITE_LCN20PHYREG(pi, papr_ctrl, psave->papr_ctrl);
	WRITE_LCN20PHYREG(pi, PapdEnable0, psave->PapdEnable0);
	WRITE_LCN20PHYREG(pi, Core1TxControl, psave->SAVE_Core1TxControl);
	wlc_lcn20phy_set_bbmult(pi, psave->bbmult);

	/* Save RFCTRL state, save all RFCTRL override register */
	WRITE_LCN20PHYREG(pi, RFOverride0, psave->RFOverride0);
	WRITE_LCN20PHYREG(pi, RFOverrideVal0, psave->RFOverrideVal0);
	WRITE_LCN20PHYREG(pi, rfoverride2, psave->rfoverride2);
	WRITE_LCN20PHYREG(pi, rfoverride2val, psave->rfoverride2val);
	WRITE_LCN20PHYREG(pi, rfoverride4, psave->rfoverride4);
	WRITE_LCN20PHYREG(pi, rfoverride4val, psave->rfoverride4val);
	WRITE_LCN20PHYREG(pi, rfoverride7, psave->rfoverride7);
	WRITE_LCN20PHYREG(pi, rfoverride7val, psave->rfoverride7val);
	WRITE_LCN20PHYREG(pi, rfoverride8, psave->rfoverride8);
	WRITE_LCN20PHYREG(pi, rfoverride8val, psave->rfoverride8val);

	/* save DSSF control register */
	WRITE_LCN20PHYREG(pi, DSSF_control_0, psave->DSSF_control_0);

	/* ensure the cal clock has been enabled */
	WRITE_LCN20PHYREG(pi, sslpnCalibClkEnCtrl, psave->sslpnCalibClkEnCtrl);

	/* save Rx/Tx farrow related register setting and DVGA2 related register setting */
	WRITE_LCN20PHYREG(pi, RxSdFeConfig1, psave->RxSdFeConfig1);
	WRITE_LCN20PHYREG(pi, RxSdFeConfig6, psave->RxSdFeConfig6);
	WRITE_LCN20PHYREG(pi, phyreg2dvga2, psave->phyreg2dvga2);
}

static void
wlc_lcn20phy_rx_iq_cal_loopback_cleanup(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_lcn20phy_radio20692_rx_iq_cal_cleanup(pi);
	}

	wlc_lcn20phy_rx_iq_cal_phy_cleanup(pi);

	if (pi_lcn20->dccalen_lpbkpath) {
		wlc_lcn20phy_dccal_ovr_idac(pi, 0, 0, 0);
	}
}

static void
wlc_lcn20phy_rx_iq_cal(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	phy_iq_comp_t pcomp;
	int8 rxiqcal_freqs[LCN20PHY_RXIQCAL_MAXNUM_FREQS];
	uint8 fidx;
	lcn20phy_rx_fam_t freq_ang_mag[LCN20PHY_RXIQCAL_MAXNUM_FREQS];
	phy_iq_est_t loopback_rx_iq;
	uint16 num_samps;

	PHY_TRACE(("%s:\n", __FUNCTION__));

	pi_lcn20->dccalen_lpbkpath = 0;
	pi_lcn20->rxiqcal_lpbkpath = LCN20PHY_RXIQCAL_PAPDPATH;

	wlc_lcn20phy_rx_iq_cal_loopback_setup(pi);

	wlc_lcn20phy_rx_iq_comp(pi, LCN20PHY_RXIQCOMP_RESET, &pcomp);

	rxiqcal_freqs[0] = (LCN20PHY_RXIQCAL_TONEFREQKHZ_0/1000);
	rxiqcal_freqs[1] = - rxiqcal_freqs[0];
	rxiqcal_freqs[2] = (LCN20PHY_RXIQCAL_TONEFREQKHZ_1/1000);
	rxiqcal_freqs[3] = - rxiqcal_freqs[2];

	num_samps = LCN20PHY_RXIQCAL_NUMSAMPS;

	for (fidx = 0; fidx < LCN20PHY_RXIQCAL_MAXNUM_FREQS; fidx++) {
		int16 tone_freqhz;

		tone_freqhz = (int16)(rxiqcal_freqs[fidx] * 1000);
		tone_freqhz = (tone_freqhz >> 1) * 1000;

		/* Freq in MHz */
		freq_ang_mag[fidx].freq = rxiqcal_freqs[fidx];
		wlc_lcn20phy_start_tx_tone(pi, tone_freqhz, LCN20PHY_RXIQCAL_TONEAMP, 0);

		wlc_lcn20phy_rx_iq_est(pi, &loopback_rx_iq, num_samps, 32, 0, FALSE);

		wlc_lcn20phy_stop_tx_tone(pi);

		wlc_lcn20phy_rxiqcal_pnp(pi, &loopback_rx_iq,
			&(freq_ang_mag[fidx].angle), &(freq_ang_mag[fidx].mag));
	}

	/* convert [mag phase] to [a b] coeff for IQ compensation block */
	wlc_lcn20phy_lin_reg(pi, freq_ang_mag, LCN20PHY_RXIQCAL_MAXNUM_FREQS);

	wlc_lcn20phy_rx_iq_cal_loopback_cleanup(pi);
}

/* END: RXIQCAL Above functions are part of Rx IQ Calibrations  feature */

/* START: TXIQLOCAL Below functions are part of Tx IQ LO calibration feature */

static void
wlc_lcn20phy_tx_iqlo_cal_phy_save_state(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	lcn20phy_txiqlocal_phyregs_t *psave = &(pi_lcn20->txiqlocal_phyregs);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Make sure we didn't call the setup twice */
	ASSERT(!psave->is_orig);
	psave->is_orig = TRUE;

	/* Save tx gain state
	* save bb_mult, txgain, papr/IIR related parameter
	*/
	psave->bbmult = wlc_lcn20phy_get_bbmult(pi);

	/* Save and Adjust State of various PhyRegs
	* Internal RFCtrl: save and adjust state of internal PA override
	*/
	psave->RxFeCtrl1 = READ_LCN20PHYREG(pi, RxFeCtrl1);
	psave->TxPwrCtrlCmd = READ_LCN20PHYREG(pi, TxPwrCtrlCmd);
	psave->RxSdFeConfig1 = READ_LCN20PHYREG(pi, RxSdFeConfig1);
	psave->sslpnCalibClkEnCtrl = READ_LCN20PHYREG(pi, sslpnCalibClkEnCtrl);
	psave->AfeCtrlOvr1Val = READ_LCN20PHYREG(pi, AfeCtrlOvr1Val);
	psave->AfeCtrlOvr1 = READ_LCN20PHYREG(pi, AfeCtrlOvr1);
	psave->ClkEnCtrl = READ_LCN20PHYREG(pi, ClkEnCtrl);
	psave->lpfbwlutreg3 = READ_LCN20PHYREG(pi, lpfbwlutreg3);
	psave->RFOverride0 = READ_LCN20PHYREG(pi, RFOverride0);
	psave->RFOverrideVal0 = READ_LCN20PHYREG(pi, RFOverrideVal0);
	psave->rfoverride4 = READ_LCN20PHYREG(pi, rfoverride4);
	psave->rfoverride4val = READ_LCN20PHYREG(pi, rfoverride4val);
	psave->TxPwrCtrlRfCtrlOvr0 = READ_LCN20PHYREG(pi, TxPwrCtrlRfCtrlOverride0);
	psave->PapdEnable = READ_LCN20PHYREG(pi, PapdEnable0);
}

static void
wlc_phy_radio20692_tx_iqlo_cal_radio_setup(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

static void
wlc_lcn20phy_tx_iqlo_cal_phy_setup(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/*
	* Note that this Routine also takes care of Phyregs that control the
	* Radio through RFCtrl, while the corresponding radio_setup Routine
	* only takes care of Radio Settings that are controlled via direct
	* Radio-reg Access
	*/

	/* set sd adc full scale */
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig1, farrow_rshift_force, 1);
	PHY_REG_MOD(pi, LCN20PHY, RxSdFeConfig1, rx_farrow_rshift_0, 3);
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlCmd, txPwrCtrl_swap_iq, 0);

	/* Enable clk */
	PHY_REG_MOD(pi, LCN20PHY, ClkEnCtrl, forcerxfrontendclk, 1);

	PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1Val, afe_iqadc_aux_en_ovr_val, 1);
	PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1, afe_iqadc_aux_en_ovr, 1);

	/* Ideally, tssi_range should not impact IQcal at all.
	* However, we found IQ cal performance improved with tssi_range = 0
	*/
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverride, 1);
	PHY_REG_MOD(pi, LCN20PHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverrideVal, 0);

	/* set LPF bw for ofdm and cck */
	PHY_REG_MOD(pi, LCN20PHY, lpfbwlutreg3, lpf_ofdm_tx_bw_ctl, 4);

	/* disable papd comp */
	PHY_REG_MOD(pi, LCN20PHY, PapdEnable0, papd_compEnb, 0);

	/*
	* The phy reg overrides here are from lcn20_play_tone routine
	* as they are not set there during iqlocal
	* The sequence of power up is important
	*/
	/* Power up mixer, cascode, ... */
	PHY_REG_MOD(pi, LCN20PHY, RFOverride0, internalrftxpu_ovr, 1);
	PHY_REG_MOD(pi, LCN20PHY, RFOverrideVal0, internalrftxpu_ovr_val, 1);
	/* Power up txlogen (JIRA HW43430-225: txlogen needs to be powered up
	* with "trsw_tx_pwrup_pu", it should have been powered-up by "internalrftxpu_ovr",
	*/
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4, trsw_tx_pwrup_ovr, 1);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4val, trsw_tx_pwrup_ovr_val, 1);
	/* Power up DAC and LPF: */
	PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1, dac_pu_ovr, 1);
	PHY_REG_MOD(pi, LCN20PHY, AfeCtrlOvr1Val, dac_pu_ovr_val, 1);
	/* Power up iPA: Make sure iPA is powered-up only AFTER TR-switch is configured for TX */
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4, papu_ovr, 1);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride4val, papu_ovr_val, 1);
}

static void
wlc_lcn20phy_tx_iqlo_cal_phy_cleanup(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	lcn20phy_txiqlocal_phyregs_t *psave = &(pi_lcn20->txiqlocal_phyregs);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Make sure we didn't call the setup twice */
	ASSERT(!psave->is_orig);
	psave->is_orig = TRUE;

	/* Save tx gain state
	* save bb_mult, txgain, papr/IIR related parameter
	*/
	wlc_lcn20phy_set_bbmult(pi, psave->bbmult);

	/* Save and Adjust State of various PhyRegs
	* Internal RFCtrl: save and adjust state of internal PA override
	*/
	WRITE_LCN20PHYREG(pi, RxFeCtrl1, psave->RxFeCtrl1);
	WRITE_LCN20PHYREG(pi, TxPwrCtrlCmd, psave->TxPwrCtrlCmd);
	WRITE_LCN20PHYREG(pi, RxSdFeConfig1, psave->RxSdFeConfig1);
	WRITE_LCN20PHYREG(pi, sslpnCalibClkEnCtrl, psave->sslpnCalibClkEnCtrl);
	WRITE_LCN20PHYREG(pi, AfeCtrlOvr1Val, psave->AfeCtrlOvr1Val);
	WRITE_LCN20PHYREG(pi, AfeCtrlOvr1, psave->AfeCtrlOvr1);
	WRITE_LCN20PHYREG(pi, ClkEnCtrl, psave->ClkEnCtrl);
	WRITE_LCN20PHYREG(pi, lpfbwlutreg3, psave->lpfbwlutreg3);
	WRITE_LCN20PHYREG(pi, RFOverride0, psave->RFOverride0);
	WRITE_LCN20PHYREG(pi, RFOverrideVal0, psave->RFOverrideVal0);
	WRITE_LCN20PHYREG(pi, rfoverride4, psave->rfoverride4);
	WRITE_LCN20PHYREG(pi, rfoverride4val, psave->rfoverride4val);
	WRITE_LCN20PHYREG(pi, TxPwrCtrlRfCtrlOverride0, psave->TxPwrCtrlRfCtrlOvr0);
	WRITE_LCN20PHYREG(pi, PapdEnable0, psave->PapdEnable);
}

static void
wlc_phy_radio20692_tx_iqlo_cal_radio_cleanup(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
}

static void
wlc_lcn20phy_tx_iqlo_cal(phy_info_t *pi, phy_txiqlocal_mode_t cal_mode)
{
	/* Cal Commands
	*
	* This uses the following format (three hex nibbles left to right)
	* 1. cal_type: 0 = IQ (a/b),   1 = deprecated 2 = LOFT digital (di/dq)
	* 2. initial stepsize (in log2)
	* 3. number of cal precision "levels"
	*/
	uint16 commands_restart[] =  {0x265, 0x234, 0x084, 0x074, 0x056};
	uint16 commands_refine[] =  {0x265, 0x234, 0x084, 0x074, 0x056};
	uint16 commands_dlo_recal[] = {0x265, 0x234};

	uint16 syst_coeffs[] =
		{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};
	uint16 coeffs[2], bbmult_for_lad, tblentry;
	uint8  indx, n_cal_cmds, lad_len;
	uint32 bbmult_scaled;
	uint16 *cal_cmds = NULL;

#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lcnphy_calcache_t *cache = NULL;

	if (ctx)
		cache = &ctx->u.lcnphy_cache;
#endif // endif

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* first save the original values of all the registers we are going to touch */
	wlc_lcn20phy_tx_iqlo_cal_phy_save_state(pi);

	/* Radio and Phy Setup */
	wlc_phy_radio20692_tx_iqlo_cal_radio_setup(pi);
	wlc_lcn20phy_tx_iqlo_cal_phy_setup(pi);
	wlc_lcn20phy_deaf_mode(pi, TRUE);

	/* Enable all clk */
	WRITE_LCN20PHYREG(pi, sslpnCalibClkEnCtrl, 0xffff);

	/* no radio LOFT or programmable radio gain */
	WRITE_LCN20PHYREG(pi, TX_iqcal_gain_bwAddress, 0);
	WRITE_LCN20PHYREG(pi, TX_loft_fine_iAddress, 0);
	WRITE_LCN20PHYREG(pi, TX_loft_fine_qAddress, 0);
	WRITE_LCN20PHYREG(pi, TX_loft_coarse_iAddress, 0);
	WRITE_LCN20PHYREG(pi, TX_loft_coarse_qAddress, 0);

	/* save for gain ladder setup */
	bbmult_for_lad = wlc_lcn20phy_get_bbmult(pi);

	lad_len = sizeof(lcn20phy_txiqlocal_ladder_lo);
	/* ladders assumed to be of equal and even lengths */
	ASSERT(sizeof(lcn20phy_txiqlocal_ladder_lo) ==
		sizeof(lcn20phy_txiqlocal_ladder_lo));
	ASSERT((sizeof(lcn20phy_txiqlocal_ladder_lo) & 0x1) == 0);

	/* Set Gain Control Parameters including IQ/LO cal enable bit
	*  iqlocal_en<15> = 1
	* gain start_index = 0xa
	* ladder_length
	*/
	WRITE_LCN20PHYREG(pi, iqloCalCmdGctl,
		LCN20PHY_iqloCalCmdGctl_iqlo_cal_en_MASK |
		(0xa << LCN20PHY_iqloCalCmdGctl_index_gctl_start_SHIFT) |
		(lad_len << LCN20PHY_iqloCalCmdGctl_gctl_LADlen_d2_SHIFT));

	/* Interval Lengths (number of samples) */
	WRITE_LCN20PHYREG(pi, iqloCalCmdNnum, LCN20PHY_TXIQLOCAL_CMDNUM);

	/* Turn on Testtone in iqcal mode */
	/* if tone is already played out with iq cal mode zero then
	 * stop the tone and re-play with iq cal mode 1.
	 */
	wlc_lcn20phy_stop_tx_tone(pi);
	OSL_DELAY(5);

	wlc_lcn20phy_start_tx_tone(pi, LCN20PHY_TXIQLOCAL_TONEFHZ,
		LCN20PHY_TXIQLOCAL_TONEAMP, LCN20PHY_TXIQLOCAL_SPBMODE);

	/* Retrieve Start Coefficients */
	switch (cal_mode) {
		case TXIQLOCAL_RESTART:
			cal_cmds = commands_restart;
			n_cal_cmds = ARRAYSIZE(commands_restart);
			wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL, syst_coeffs,
				ARRAYSIZE(syst_coeffs), 16, LCN20PHY_TXIQLOCAL_IQCOEFF_OFFSET);
			break;

		case TXIQLOCAL_REFINE:
			cal_cmds = commands_refine;
			n_cal_cmds = ARRAYSIZE(commands_refine);
			/* for refine, get from data path regs
			* assumes dig BPHY = dig OFDM coeffs)
			*/
			/* iq comp */
			wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 2, 16, LCN20PHY_TXIQLOCAL_IQCOMP_OFFSET);
			wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 2, 16, LCN20PHY_TXIQLOCAL_IQCOEFF_OFFSET);
			/* dig loft comp */
			wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 1, 16, LCN20PHY_TXIQLOCAL_DLOCOMP_OFFSET);
			wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 1, 16, LCN20PHY_TXIQLOCAL_DLOCOEFF_OFFSET);
			break;

		case TXIQLOCAL_DLORECAL:
			cal_cmds = commands_dlo_recal;
			n_cal_cmds = ARRAYSIZE(commands_dlo_recal);
			wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL, syst_coeffs,
				ARRAYSIZE(syst_coeffs), 16, LCN20PHY_TXIQLOCAL_IQCOEFF_OFFSET);
			break;

		default:
			ASSERT(FALSE);
			goto cleanup;
	}

	/* -----------
	* Main Loop
	*-----------
	* dynamically set up gain ladder based on this core's bbmult
	* (the bbmult chosen above will become max entry in the ladder)
	*/
	for (indx = 0; indx < 18; indx++) {
		/* calculate and write LO cal gain ladder */
		bbmult_scaled = lcn20phy_txiqlocal_ladder_lo[indx].percent * bbmult_for_lad;
		bbmult_scaled /= 100;
		tblentry = ((bbmult_scaled & 0xff) << 8) | lcn20phy_txiqlocal_ladder_lo[indx].g_env;
		wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
			&tblentry, 1, 16, indx);

		/* calculate and write IQ cal gain ladder */
		bbmult_scaled = lcn20phy_txiqlocal_ladder_iq[indx].percent * bbmult_for_lad;
		bbmult_scaled /= 100;
		tblentry = ((bbmult_scaled & 0xff) << 8) | lcn20phy_txiqlocal_ladder_iq[indx].g_env;
		wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
			&tblentry, 1, 16, indx+32);
	}

	for (indx = 0; indx < n_cal_cmds; indx++) {
		uint8 cal_type;

		WRITE_LCN20PHYREG(pi, iqloCalgtlthres, LCN20PHY_TXIQLOCAL_THRSLAD);

		/* trigger calibration step */
		WRITE_LCN20PHYREG(pi, iqloCalCmd, (cal_cmds[indx] | 0x8000));

		/* wait for 100msec */
		SPINWAIT(((READ_LCN20PHYREG(pi, iqloCalCmd) & 0xc000) != 0),
			LCN20PHY_SPINWAIT_TXIQLOCAL_USEC);
		ASSERT((READ_LCN20PHYREG(pi, iqloCalCmd) & 0xc000) == 0);

		cal_type = READ_LCN20PHYREGFLD(pi, iqloCalCmd, cal_type);
		if (cal_type == LCN20PHY_CAL_TYPE_IQ) {
			wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 2, 16, LCN20PHY_TXIQLOCAL_IQBESTCOEFF_OFFSET);
			wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 2, 16, LCN20PHY_TXIQLOCAL_IQCOEFF_OFFSET);
		} else {
			wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 1, 16, LCN20PHY_TXIQLOCAL_DLOBESTCOEFF_OFFSET);
			wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
				coeffs, 1, 16, LCN20PHY_TXIQLOCAL_DLOCOEFF_OFFSET);
		}
	} /* For loop for cals */

	/* ----------------
	* Process Results
	*  ----------------
	*/
	/* copy "best" to "normal" */
	if (cal_mode != TXIQLOCAL_DLORECAL) {
		wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
			coeffs, 2, 16, LCN20PHY_TXIQLOCAL_IQBESTCOEFF_OFFSET);
		wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
			coeffs, 2, 16, LCN20PHY_TXIQLOCAL_IQCOMP_OFFSET);
		wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
			coeffs, 2, 16, LCN20PHY_TXIQLOCAL_BPHY_IQCOMP_OFFSET);
	}

	wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
		coeffs, 1, 16, LCN20PHY_TXIQLOCAL_DLOBESTCOEFF_OFFSET);
	wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
		coeffs, 1, 16, LCN20PHY_TXIQLOCAL_DLOCOMP_OFFSET);
	wlc_lcn20phy_common_write_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
		coeffs, 1, 16, LCN20PHY_TXIQLOCAL_BPHY_DLOCOMP_OFFSET);

	/* disable calibclk */
	WRITE_LCN20PHYREG(pi, sslpnCalibClkEnCtrl, 0xffbf);

cleanup:
	/* Switch off test tone */
	wlc_lcn20phy_stop_tx_tone(pi);
	wlc_lcn20phy_tx_iqlo_cal_phy_cleanup(pi);
	wlc_phy_radio20692_tx_iqlo_cal_radio_cleanup(pi);
	wlc_lcn20phy_deaf_mode(pi, FALSE);
}

static void
wlc_lcn20phy_populate_tx_iqlo_comp_tables(phy_info_t *pi, uint8 startidx, uint8 stopidx)
{
	uint16 iqcomp[2], didq, idx;
	uint32 *val_array = NULL;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if ((val_array = (uint32*)
		LCN20PHY_MALLOC(pi, (stopidx - startidx + 1) * sizeof(uint32))) == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	/* load IQ comp tables (19:10 -- "a" coeff   9:0 -- "b" coeff)
	*   load LO comp tables (15:8 -- I offset   7:0 -- Q offset)
	*   get from IQ cal result (iqloCaltbl) and use same IQ/LO comp for all
	*/
	wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
		iqcomp, 2, 16, LCN20PHY_TXIQLOCAL_IQCOMP_OFFSET);
	wlc_lcn20phy_common_read_table(pi, LCN20PHY_TBL_ID_IQLOCAL,
		&didq, 1, 16, LCN20PHY_TXIQLOCAL_DLOCOMP_OFFSET);

	for (idx = 0; idx <= (stopidx - startidx); idx++) {
		*(val_array + idx) = (*(val_array + idx) & 0x0ff00000) |
			((uint32)(iqcomp[0] & 0x3FF) << 10) | (iqcomp[1] & 0x3ff);
	}
	wlc_lcn20phy_common_write_table(pi, LCN20PHY_TX_PWR_CTRL_IQ_OFFSET,
		val_array, (stopidx - startidx + 1), 32, LCN20PHY_TX_PWR_CTRL_IQ_OFFSET);

	for (idx = 0; idx <= (stopidx - startidx); idx++)
			*(val_array + idx) = didq;
	wlc_lcn20phy_common_write_table(pi, LCN20PHY_TX_PWR_CTRL_IQ_OFFSET,
		val_array, (stopidx - startidx + 1), 16, LCN20PHY_TX_PWR_CTRL_LO_OFFSET);
}

/* Top level chip dependent iqlo cal routine
* Pick gain or gains at which to run cal and populate per gain index cal coeff tables
*/
static void
wlc_lcn20phy_tx_iqlo_cal_txpwr(phy_info_t *pi)
{
	uint8 SAVE_indx;
	uint16 SAVE_txpwrctrl;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	SAVE_txpwrctrl = wlc_lcn20phy_get_tx_pwr_ctrl(pi);
	SAVE_indx = wlc_lcn20phy_get_current_tx_pwr_idx(pi);

	wlc_lcn20phy_set_tx_pwr_by_index(pi, 127);

	wlc_lcn20phy_tx_iqlo_cal(pi, TXIQLOCAL_RESTART);

	wlc_lcn20phy_populate_tx_iqlo_comp_tables(pi, 0, 127);

	wlc_lcn20phy_set_tx_pwr_by_index(pi, SAVE_indx);
	wlc_lcn20phy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);
}

/* END: TXIQLOCAL Above functions are part of Tx IQ LO calibration feature */

/* BEGIN : SROM Below functions are part of srom read feature */
static const char BCMATTACHDATA(rstr_rssismf2g)[] = "rssismf2g";
static const char BCMATTACHDATA(rstr_rssismc2g)[] = "rssismc2g";
static const char BCMATTACHDATA(rstr_rssisav2g)[] = "rssisav2g";
static const char BCMATTACHDATA(rstr_tssitime)[] = "tssitime";
static const char BCMATTACHDATA(rstr_itssicorr)[] = "itssicorr";
static const char BCMATTACHDATA(rstr_initxidx)[] = "initxidx";
static const char BCMATTACHDATA(rstr_tssifloor2g)[] = "tssifloor2g";

static const char BCMATTACHDATA(rstr_cckdigfilttype)[] = "cckdigfilttype";
static const char BCMATTACHDATA(rstr_ofdmdigfilttype)[] = "ofdmdigfilttype";
static const char BCMATTACHDATA(rstr_ofdmdigfilttype2g)[] = "ofdmdigfilttype2g";
static const char BCMATTACHDATA(rstr_cckscale)[] = "cckscale";

static bool
BCMATTACHFN(wlc_lcn20phy_txpwr_srom_read)(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;

	/* RSSI */
	pi_lcn20->rssi_vf = (uint8)PHY_GETINTVAR_DEFAULT(pi, rstr_rssismf2g, 0);
	pi_lcn20->rssi_vc = (uint8)PHY_GETINTVAR_DEFAULT(pi, rstr_rssismc2g, 0);
	pi_lcn20->rssi_gs = (uint8)PHY_GETINTVAR_DEFAULT(pi, rstr_rssisav2g, 0);

	pi_lcn20->tssical_time = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_tssitime, 0);
	pi_lcn20->idletssi_corr = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_itssicorr, 0);
	pi_lcn20->init_txpwrindex = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_initxidx,
		LCN20PHY_TX_PWR_CTRL_START_INDEX_2G);
	pi_lcn20->tssi_floor =
		(uint8)PHY_GETINTVAR_DEFAULT(pi, rstr_tssifloor2g, 0);

	pi_lcn20->cckdigiftype =
		(int16)PHY_GETINTVAR_DEFAULT(pi, rstr_cckdigfilttype, -1);
	pi_lcn20->ofdmdigiftype =
		(int16)PHY_GETINTVAR_DEFAULT(pi, rstr_ofdmdigfilttype, -1);

	pi_lcn20->cckscale_fctr_db =
		(int8) PHY_GETINTVAR_DEFAULT(pi, rstr_cckscale, -1);

	return TRUE;
}
/* END: SROM Above functions are part of srom read feature */

static void
WLBANDINITFN(wlc_lcn20phy_radio20692_init)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* xtal powerup */
	MOD_RADIO_REG_20692(pi, WL_XTAL_OVR1, 0, wl_ovr_xtal_pu, 0x1);
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_pu, 0x1);
	OSL_DELAY(1000);
	MOD_RADIO_REG_20692(pi, WL_XTAL_OVR1, 0, wl_ovr_xtal_buf_pu, 0x1);
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_buf_pu, 0x1);
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu, 0x7f);

	/* PU the TX/RX/VCO/..LDO's	*/

	/* mini PMU init */
	MOD_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, wl_ovr_pmu_wlpmu_en, 1);
	MOD_RADIO_REG_20692(pi, WL_PMU_OP, 0, wl_wlpmu_en, 1);
	MOD_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, wl_ovr_pmu_TXldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PMU_OP, 0, wl_TXldo_pu, 1);
	OSL_DELAY(100);
	MOD_RADIO_REG_20692(pi, WL_VREG_HIGHV_VBAT_TOP_OVR1, 0, wl_ovr_vreg25_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_VREG_CFG, 0, wl_vreg_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_BG_TOP_V3_OVR1, 0, wl_ovr_bg_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_BG_CFG1, 0, wl_bg_pu, 1);
	OSL_DELAY(20);
	/* Turn on all wlpmu outputs */
	MOD_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, wl_ovr_pmu_VCOldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, wl_ovr_pmu_AFEldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, wl_ovr_pmu_RXldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, wl_ovr_pmu_ADCldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PMU_OP, 0, wl_VCOldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PMU_OP, 0, wl_AFEldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PMU_OP, 0, wl_RXldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PMU_OP, 0, wl_ADCldo_pu, 1);
	OSL_DELAY(200);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG2, 0, wl_synth_pwrsw_en, 1);

	/* pmu, regulator, pll related power ups from rg_init */
	MOD_RADIO_REG_20692(pi, WL_VREG_HIGHV_VBAT_TOP_OVR1, 0, wl_ovr_vreg25_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_VREG_CFG, 0, wl_vreg_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_BG_TOP_V3_OVR1, 0, wl_ovr_bg_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_BG_CFG1, 0, wl_bg_pu, 1);

	/* powerup pll */
	MOD_RADIO_REG_20692(pi, WL_SYNTH_OVR1, 0, wl_ovr_rfpll_vco_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_vco_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_SYNTH_OVR1, 0, wl_ovr_rfpll_vco_buf_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_vco_buf_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_SYNTH_OVR1, 0, wl_ovr_rfpll_synth_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_synth_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_SYNTH_OVR1, 0, wl_ovr_rfpll_monitor_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_monitor_pu, 1);

	/* Rcal */
	MOD_RADIO_REG_20692(pi, WL_BG_TOP_V3_OVR1, 0, wl_ovr_bg_rcal_trim, 1);
	MOD_RADIO_REG_20692(pi, WL_BG_CFG1, 0, wl_bg_rcal_trim, 0x8);
	MOD_RADIO_REG_20692(pi, WL_BG_TOP_V3_OVR1, 0, wl_ovr_otp_rcal_sel, 0);

	/* RC-CAL */
	wlc_lcn20phy_radio20692_rc_cal(pi);

	/* Dac settings */
	MOD_RADIO_REG_20692(pi, WL_CLK_DIV_CFG1, 0, wl_afeclkdiv_dac_driver_size, 4);
	MOD_RADIO_REG_20692(pi, WL_CLK_DIV_CFG1, 0, wl_afeclkdiv_sel_dac_div, 4);
	MOD_RADIO_REG_20692(pi, WL_TX_DAC_CFG5, 0, wl_DAC_invclk, 1);
	MOD_RADIO_REG_20692(pi, WL_TX_DAC_CFG5, 0, wl_DAC_pd_mode, 0);
	MOD_RADIO_REG_20692(pi, WL_BG_CFG1, 0, wl_bg_pulse, 1);
	MOD_RADIO_REG_20692(pi, WL_TX_DAC_CFG1, 0, wl_DAC_scram_off, 1);

	/* populate TIA LUT */
	wlc_lcn20phy_radio20692_tia_config(pi);

}

static void
wlc_lcn20phy_radio20692_tia_config(phy_info_t *pi)
{
	const uint8  *p8;
	const uint16 *p16;
	uint16 lut;

	STATIC_ASSERT(ARRAYSIZE(tiaRC_20692_8b_20) +
		ARRAYSIZE(tiaRC_20692_16b_20) == TIA_LUT_20692_LEN);

	p8 = tiaRC_20692_8b_20;
	p16 = tiaRC_20692_16b_20;

	lut = RADIO_REG_20692(pi, WL_TIA_LUT_0, 0);

	/* the assumption is that all the TIA LUT registers are in sequence */
	ASSERT(RADIO_REG_20692(pi, WL_TIA_LUT_108, core) - lut == (TIA_LUT_20692_LEN - 1));

	do {
		write_radio_reg(pi, lut++, *p8++);
	} while (lut <= RADIO_REG_20692(pi, WL_TIA_LUT_51, 0));

	do {
		write_radio_reg(pi, lut++, *p16++);
	} while (lut <= RADIO_REG_20692(pi, WL_TIA_LUT_108, 0));

}

static void
wlc_lcn20phy_radio20692_channel_tune(phy_info_t *pi, uint8 channel)
{
	uint8 xtal_out_pu;
	const chan_info_20692_lcn20phy_t *chi;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Xtal Tune : Pre Channel */
	/* powerup RCCAL driver */
	xtal_out_pu = READ_RADIO_REGFLD_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu) & 0x40;
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu, xtal_out_pu);

	/* Write the tuning tables */
	if (pi->xtalfreq == XTAL_FREQ_37P4MHZ)
		chi = &chan_info_20692_rev1_lcn20phy_37p4MHz[channel];
	else
		chi = &chan_info_20692_rev1_lcn20phy_26MHz[channel];

	ASSERT(chi != NULL);

	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL18, 0, chi->pll_vcocal18);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL3, 0, chi->pll_vcocal3);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL4, 0, chi->pll_vcocal4);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL7, 0, chi->pll_vcocal7);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL8, 0, chi->pll_vcocal8);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL20, 0, chi->pll_vcocal20);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL1, 0, chi->pll_vcocal1);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL12, 0, chi->pll_vcocal12);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL13, 0, chi->pll_vcocal13);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL10, 0, chi->pll_vcocal10);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL11, 0, chi->pll_vcocal11);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL19, 0, chi->pll_vcocal19);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL6, 0, chi->pll_vcocal6);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL9, 0, chi->pll_vcocal9);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL17, 0, chi->pll_vcocal17);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL5, 0, chi->pll_vcocal5);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL15, 0, chi->pll_vcocal15);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCOCAL2, 0, chi->pll_vcocal2);
	WRITE_RADIO_REG_20692(pi, WL_PLL_MMD1, 0, chi->pll_mmd1);
	WRITE_RADIO_REG_20692(pi, WL_PLL_MMD2, 0, chi->pll_mmd2);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCO2, 0, chi->pll_vco2);
	WRITE_RADIO_REG_20692(pi, WL_PLL_VCO1, 0, chi->pll_vco1);
	WRITE_RADIO_REG_20692(pi, WL_PLL_LF4, 0, chi->pll_lf4);
	WRITE_RADIO_REG_20692(pi, WL_PLL_LF5, 0, chi->pll_lf5);
	WRITE_RADIO_REG_20692(pi, WL_PLL_LF2, 0, chi->pll_lf2);
	WRITE_RADIO_REG_20692(pi, WL_PLL_LF3, 0, chi->pll_lf3);
	WRITE_RADIO_REG_20692(pi, WL_PLL_CP1, 0, chi->pll_cp1);
	WRITE_RADIO_REG_20692(pi, WL_PLL_CP2, 0, chi->pll_cp2);
	WRITE_RADIO_REG_20692(pi, WL_LOGEN_CFG2, 0, chi->logen_cfg2);
	WRITE_RADIO_REG_20692(pi, WL_LNA2G_TUNE, 0, chi->lna2g_tune);
	WRITE_RADIO_REG_20692(pi, WL_TXMIX2G_CFG5, 0, chi->txmix2g_cfg5);

	/* Band Set */
	MOD_RADIO_REG_20692(pi, WL_RXMIX2G_CFG1, 0, wl_rxgm2g_auxgm_pwrup, 0);
	MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG2, 0, wl_lpf_sel_5g_out_gm, 0);
	MOD_RADIO_REG_20692(pi, WL_TX_LPF_CFG3, 0, wl_lpf_sel_2g_5g_cmref_gm, 0);

	/* VCO Cal */
	wlc_lcn20phy_radio20692_vcocal(pi);
	wlc_lcn20phy_radio20692_vcocal_isdone(pi, TRUE);

	/* Xtal Tune : Post Channel */
	/* Power-down RCCAL driver  */
	xtal_out_pu = READ_RADIO_REGFLD_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu) & 0x3F;
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu, xtal_out_pu);

	/* Power-down BT driver */
	xtal_out_pu = READ_RADIO_REGFLD_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu) & 0x37;
	MOD_RADIO_REG_20692(pi, WL_XTAL_CFG1, 0, wl_xtal_out_pu, xtal_out_pu);
}

static void
wlc_lcn20phy_radio20692_vcocal(phy_info_t *pi)
{

	MOD_RADIO_REG_20692(pi, WL_PMU_CFG2, 0, wl_VCOldo_adj, 0);
	MOD_RADIO_REG_20692(pi, WL_PLL_VCOCAL1, 0, wl_rfpll_vcocal_enableCal, 0);
	OSL_DELAY(1);
	MOD_RADIO_REG_20692(pi, WL_PLL_VCOCAL1, 0, wl_rfpll_vcocal_enableCal, 1);
	/* VCO-Cal startup seq */
	/* TODO: The below registers have direct PHY control in 20692
		 so this reset should ideally be done by writing phy registers
	*/
	/* Disable PHY direct control for delta-sigma modulator reset signal */
	MOD_RADIO_REG_20692(pi, WL_SYNTH_OVR1, 0, wl_ovr_rfpll_rst_n, 1);
	/* Reset delta-sigma modulator */
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_rst_n, 0);
	/* Disable PHY direct control for vcocal reset */
	MOD_RADIO_REG_20692(pi, WL_SYNTH_OVR1, 0, wl_ovr_rfpll_vcocal_rst_n, 1);
	/* Reset VCO cal */
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_vcocal_rst_n, 0);
	OSL_DELAY(1);
	/* Release Reset */
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_rst_n, 1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_vcocal_rst_n, 1);
	OSL_DELAY(1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_cp_bias_reset, 1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_vco_bias_reset, 1);
	OSL_DELAY(1);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_cp_bias_reset, 0);
	MOD_RADIO_REG_20692(pi, WL_PLL_CFG1, 0, wl_rfpll_vco_bias_reset, 0);
	OSL_DELAY(50);
}

#define MAX_2069x_VCOCAL_WAITLOOPS 100
/* vcocal should take < 120 us */
static void
wlc_lcn20phy_radio20692_vcocal_isdone(phy_info_t *pi, bool set_delay)
{
	uint8 done, itr;

	/* Wait for vco_cal to be done, max = 100us * 10 = 1ms  */
	done = 0;
	for (itr = 0; itr < MAX_2069x_VCOCAL_WAITLOOPS; itr++) {
		OSL_DELAY(10);
		done = READ_RADIO_REGFLD_20692(pi, WL_PLL_STATUS1, 0, rfpll_vcocal_done_cal);
		if (done == 1)
			break;
	}

	/* Need to wait extra time after vcocal done bit is high for it to settle */
	if (set_delay == TRUE)
		OSL_DELAY(100);

	ASSERT(done & 0x1);

	PHY_INFORM(("wl%d: %s vcocal done\n", pi->sh->unit, __FUNCTION__));
}

static void
wlc_lcn20phy_radio20692_tssisetup(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Powerup gpaio block, powerdown rcal, clear all test point selection */
	MOD_RADIO_REG_20692(pi, WL_GPAIO_SEL2, 0, wl_cgpaio_pu, 1);

	/* Powerdown rcal otherwise it wont let any other test point go through */
	MOD_RADIO_REG_20692(pi, WL_RCAL_CFG1, 0, wl_rcal_pu, 0);

	MOD_RADIO_REG_20692(pi, WL_GPAIO_SEL0, 0, wl_cgpaio_sel_0to15_port, 0x0);
	MOD_RADIO_REG_20692(pi, WL_GPAIO_SEL1, 0, wl_cgpaio_sel_16to31_port, 0x0);

	MOD_RADIO_REG_20692(pi, WL_GPAIO_SEL0, 0, wl_cgpaio_sel_0to15_port, 0x0);
	MOD_RADIO_REG_20692(pi, WL_GPAIO_SEL1, 0, wl_cgpaio_sel_16to31_port, 0x0);

	/* INT TSSI setup */
	/* # B3: (1 = iqcal, 0 = tssi);
	 * # B2: (1 = ext-tssi, 0 = int-tssi)
	 * # B1: (1 = 5g, 0 = 2g)
	 * # B0: (1 = wo filter, 0 = w filter for ext-tssi)
	 */
	MOD_RADIO_REG_20692(pi, WL_IQCAL_CFG1, 0, wl_iqcal_sel_sw, 0x0);

	/* Select PA output (and not PA input) */
	MOD_RADIO_REG_20692(pi, WL_PA2G_CFG1, 0, wl_pa2g_tssi_ctrl_sel, 0);

	/* int-tssi select */
	MOD_RADIO_REG_20692(pi, WL_IQCAL_CFG1, 0, wl_iqcal_sel_ext_tssi, 0);

	/* dont bypass tia amp2 */
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_amp2_bypass, 1);
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG1, 0, wl_tia_amp2_bypass, 0);
}

static void
wlc_lcn20phy_tia_gain(phy_info_t *pi, uint16 gain)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_R1, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_R2, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_R3, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_R4, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_R5, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_R6, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_C1, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_C2, 0);

	PHY_REG_MOD(pi, LCN20PHY, rfoverride5, rxrf_tia_gain_ovr, 1);
	PHY_REG_MOD(pi, LCN20PHY, rfoverride5val, rxrf_tia_gain_ovr_val, gain);
}

static void
wlc_lcn20phy_radio20692_rx_iq_cal_setup(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	lcn20phy_rxiqcal_radioregs_t *psave = &(pi_lcn20->rxiqcal_radioregs);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Make sure we didn't call the setup twice */
	ASSERT(!psave->is_orig);
	psave->is_orig = TRUE;

	psave->wl_rx_adc_ovr1 = READ_RADIO_REG_20692(pi, WL_RX_ADC_OVR1, 0);
	psave->wl_tx_top_2g_ovr1 = READ_RADIO_REG_20692(pi, WL_TX_TOP_2G_OVR1, 0);
	psave->wl_rx_top_2g_ovr1 = READ_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0);
	psave->wl_rx_bb_ovr1 = READ_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0);
	psave->wl_minipmu_ovr1 = READ_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0);

	/* force off the dac 2 adc switches */
	MOD_RADIO_REG_20692(pi, WL_RX_ADC_OVR1, 0, wl_ovr_adc_in_test, 1);
	MOD_RADIO_REG_20692(pi, WL_ADC_CFG10, 0, wl_adc_in_test, 0);

	/* Don't enable bbpd input to TIA */
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG8, 0, wl_tia_tx_lpbck_i, 0);
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG8, 0, wl_tia_tx_lpbck_q, 0);

	/* PAPD LOOPBACK PATH */

	/* TR sw in TX mode */
	MOD_RADIO_REG_20692(pi, WL_TX_TOP_2G_OVR1, 0, wl_ovr_trsw2g_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_TRSW2G_CFG1, 0, wl_trsw2g_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_TX_TOP_2G_OVR1, 0, wl_ovr_trsw2g_bias_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_TRSW2G_CFG1, 0, wl_trsw2g_bias_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0, wl_ovr_lna2g_tr_rx_en, 1);
	MOD_RADIO_REG_20692(pi, WL_LNA2G_CFG1, 0, wl_lna2g_tr_rx_en, 0);

	/* Enable ipapd */
	MOD_RADIO_REG_20692(pi, WL_TX2G_CFG1, 0, wl_cal2g_pa_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_RXRF2G_CFG1, 0, wl_loopback2g_papdcal_pu, 1);

	/* Set txattn and rxattn */
	MOD_RADIO_REG_20692(pi, WL_TX2G_CFG1, 0, wl_cal2g_pa_atten, 3);
	MOD_RADIO_REG_20692(pi, WL_RXRF2G_CFG1, 0, wl_rf2g_papdcal_rx_attn, 3);

	/* Powerdown lna1 */
	MOD_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0, wl_ovr_lna2g_lna1_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_LNA2G_CFG1, 0, wl_lna2g_lna1_pu, 0);
	MOD_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0, wl_ovr_lna2g_lna1_out_short_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_LNA2G_CFG1, 0, wl_lna2g_lna1_out_short_pu, 1);

	/* powerdown rx main gm2g  */
	MOD_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0, wl_ovr_gm2g_pwrup, 1);
	MOD_RADIO_REG_20692(pi, WL_RXMIX2G_CFG1, 0, wl_rxgm2g_pwrup, 0x0);

	/* powerup auxgm2g */
	MOD_RADIO_REG_20692(pi, WL_RXMIX2G_CFG1, 0, wl_rxgm2g_auxgm_pwrup, 0x1);

	/* powerup rx mixer */
	MOD_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0, wl_ovr_rxmix2g_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_RXMIX2G_CFG1, 0, wl_rxmix2g_pu, 1);

	/* powerup rxdiv2g */
	MOD_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0, wl_ovr_rxdiv2g_pu_bias, 1);
	MOD_RADIO_REG_20692(pi, WL_RXRF2G_CFG1, 0, wl_rxdiv2g_pu_bias, 1);

	/* powerup tia */
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_pwrup_resstring, 1);
	/* Power up common mode reference */
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG3, 0, wl_tia_pwrup_resstring, 1);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_amp1_pwrup, 1);
	/* PU 1st amp */
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG3, 0, wl_tia_amp1_pwrup, 1);
	MOD_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, wl_ovr_tia_pwrup_amp2, 1);
	/* PU 2nd amp */
	MOD_RADIO_REG_20692(pi, WL_TIA_CFG3, 0, wl_tia_pwrup_amp2, 1);

	wlc_lcn20phy_tia_gain(pi, 3);

	/* powerup adc */
	MOD_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, wl_ovr_pmu_ADCldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_PMU_OP, 0, wl_ADCldo_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_RX_ADC_OVR1, 0, wl_ovr_adc_clk_slow_pu, 0x1);
	MOD_RADIO_REG_20692(pi, WL_ADC_CFG8, 0, wl_adc_clk_slow_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_RX_ADC_OVR1, 0, wl_ovr_adc_slow_pu, 0x1);
	MOD_RADIO_REG_20692(pi, WL_ADC_CFG1, 0, wl_adc_slow_pu, 1);
	MOD_RADIO_REG_20692(pi, WL_RX_ADC_OVR1, 0, wl_ovr_adc_sipo_pu, 0x1);
	MOD_RADIO_REG_20692(pi, WL_ADC_CFG1, 0, wl_adc_sipo_pu, 1);
}

static void
wlc_lcn20phy_radio20692_rx_iq_cal_cleanup(phy_info_t *pi)
{
	phy_info_lcn20phy_t *pi_lcn20 = pi->u.pi_lcn20phy;
	lcn20phy_rxiqcal_radioregs_t *psave = &(pi_lcn20->rxiqcal_radioregs);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Make sure we didn't call the setup twice */
	ASSERT(!psave->is_orig);
	psave->is_orig = FALSE;

	WRITE_RADIO_REG_20692(pi, WL_RX_ADC_OVR1, 0, psave->wl_rx_adc_ovr1);
	WRITE_RADIO_REG_20692(pi, WL_TX_TOP_2G_OVR1, 0, psave->wl_tx_top_2g_ovr1);
	WRITE_RADIO_REG_20692(pi, WL_RX_TOP_2G_OVR1, 0, psave->wl_rx_top_2g_ovr1);
	WRITE_RADIO_REG_20692(pi, WL_RX_BB_OVR1, 0, psave->wl_rx_bb_ovr1);
	WRITE_RADIO_REG_20692(pi, WL_MINIPMU_OVR1, 0, psave->wl_minipmu_ovr1);

	MOD_RADIO_REG_20692(pi, WL_TX2G_CFG1, 0, wl_cal2g_pa_pu, 0);
	MOD_RADIO_REG_20692(pi, WL_RXRF2G_CFG1, 0, wl_loopback2g_papdcal_pu, 0);
	MOD_RADIO_REG_20692(pi, WL_RXMIX2G_CFG1, 0, wl_rxgm2g_auxgm_pwrup, 0);
}

#endif /* #if ((defined(LCN20CONF) && (LCN20CONF != 0))) */
