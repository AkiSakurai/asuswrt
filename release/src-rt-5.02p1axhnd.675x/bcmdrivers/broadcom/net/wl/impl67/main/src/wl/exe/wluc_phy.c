/*
 * wl phy command module
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
 * $Id: wluc_phy.c 790746 2020-09-02 03:51:07Z $
 */

#include <wlioctl.h>

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmsrom_fmt.h>
#include <bcmsrom_tbl.h>
#include <bcmdevs.h>
#include "wlu_common.h"
#include "wlu.h"
#include <miniopt.h>

/* For backwards compatibility, the absense of the define 'NO_FILESYSTEM_SUPPORT'
 * implies that a filesystem is supported.
 */
#if !defined(BWL_NO_FILESYSTEM_SUPPORT)
#define BWL_FILESYSTEM_SUPPORT
#endif // endif

static cmd_func_t wl_pkteng, wl_pkteng_stats, wl_phy_txpwrindex, wl_pkteng_status;
static cmd_func_t wl_pkteng_trig_fill;
static cmd_func_t wl_get_trig_info;
static cmd_func_t wl_pkteng_cmd;
static cmd_func_t wl_sample_collect;
static cmd_func_t wl_phy_force_crsmin;
static cmd_func_t wl_phy_rssi_ant;
static cmd_func_t wl_phy_snr_ant;
static cmd_func_t wl_tssi, wl_atten, wl_evm;
static cmd_func_t wl_interfere, wl_interfere_override;
static cmd_func_t wl_get_instant_power;
static cmd_func_t wl_phymsglevel;
#if defined(BCMDBG)
static cmd_func_t wl_phy_debug_cmd;
#endif // endif
static cmd_func_t wl_rifs;
static cmd_func_t wl_rifs_advert;
static cmd_func_t wl_test_tssi, wl_test_tssi_offs, wl_phy_rssiant, wl_rxiq;
static cmd_func_t wl_rxiq_sweep;
static cmd_func_t wl_test_idletssi;
static cmd_func_t wlu_afeoverride;
static cmd_func_t wl_phy_papdepstbl;
static cmd_func_t wl_phy_txiqcc, wl_phy_txlocc;
static cmd_func_t wl_rssi_cal_freq_grp_2g;
static cmd_func_t wl_phy_rssi_gain_delta_2g, wl_phy_rssi_gain_delta_5g;
static cmd_func_t wl_phy_rssi_gain_delta_2g_sub;
static cmd_func_t wl_phy_rxgainerr_2g, wl_phy_rxgainerr_5g, wl_phy_rxgainerr_6g;
static cmd_func_t wl_phytable, wl_phy_pavars, wl_phy_povars;
static cmd_func_t wl_phy_fem, wl_phy_maxpower;
static cmd_func_t wl_phy_rpcalvars;
static cmd_func_t wl_phy_rpcalphasevars;
static cmd_func_t wl_phy_force_vsdb_chans;
static cmd_func_t wl_radar_args, wl_radar_thrs, wl_radar_thrs2;
static cmd_func_t wl_phy_dyn_switch_th;
static cmd_func_t wl_btcoex_desense_rxgain;
static cmd_func_t wl_phy_align_sniffer;

#if defined(WLTEST)
static cmd_func_t wl_patrim;
#endif // endif
static cmd_func_t wl_phy_tpc_av, wl_phy_tpc_vmid;

/* txcal iovars */
static cmd_func_t wl_txcal_gainsweep;
static cmd_func_t wl_txcal_gainsweep_meas;
static cmd_func_t wl_txcal_pwr_tssi_tbl;
static cmd_func_t wl_olpc_anchoridx;
static cmd_func_t wl_read_estpwrlut;
static cmd_func_t wl_olpc_offset;
static cmd_func_t wl_dbg_regval;
static cmd_func_t wl_phy_ulofdma_stats;
static cmd_func_t wl_phy_tracked_freq;
static cmd_func_t wl_phy_dssf_setup;
static cmd_func_t wl_phy_temp_counters;

/* rxiqcal */
static cmd_func_t wl_phy_rxiqcal_gtbl;

#ifdef WL_EAP_NOISE_MEASUREMENTS
/* 8 phynoisebias command string iovars... */
static cmd_func_t wl_phy_noise_bias_2glo;
static cmd_func_t wl_phy_noise_bias_2ghi;
static cmd_func_t wl_phy_noise_bias_5glo;
static cmd_func_t wl_phy_noise_bias_5ghi;
static cmd_func_t wl_phy_noise_bias_5g_radar_lo;
static cmd_func_t wl_phy_noise_bias_5g_radar_hi;
static cmd_func_t wl_phy_noise_bias_6glo;
static cmd_func_t wl_phy_noise_bias_6ghi;
/* ... converge to a single call with parameters */
static int wl_phy_noise_bias_iov(void *wl, cmd_t *cmd, char **argv,
	wl_phynoisebias_band_t band, wl_phynoisebias_gain_t gain);
#endif /* WL_EAP_NOISE_MEASUREMENTS */

static int wl_pkteng_cmd_get_key_val(wl_pkteng_cmd_params_t *pecfg_params,
	char **argv, bool must_have_key);

typedef struct {
	uint value;
	const char *string;
} phy_msg_t;

static cmd_t wl_phy_cmds[] = {
	{ "restart", wl_void, -1, WLC_RESTART,
	"Restart driver.  Driver must already be down."},
	{ "phymsglevel", wl_phymsglevel, WLC_GET_VAR, WLC_SET_VAR,
	"set phy debugging message bitvector\n"
	"\ttype \'wl phymsglevel ?\' for values" },
	{ "tssi", wl_tssi, WLC_GET_TSSI, -1,
	"Get the tssi value from radio" },
	{ "txpathpwr", wl_int, WLC_GET_TX_PATH_PWR, WLC_SET_TX_PATH_PWR,
	"Turn the tx path power on or off on 2050 radios" },
	{ "powerindex", wl_int, WLC_GET_PWRIDX, WLC_SET_PWRIDX,
	"Set the transmit power for A band(0-63).\n"
	"\t-1 - default value" },
	{ "atten", wl_atten, WLC_GET_ATTEN, WLC_SET_ATTEN,
	"Set the transmit attenuation for B band. Args: bb radio txctl1.\n"
	"\tauto to revert to automatic control\n"
	"\tmanual to supspend automatic control" },
	{ "phyreg", wl_reg, WLC_GET_PHYREG, WLC_SET_PHYREG,
	"Get/Set a phy register:\n"
	"\toffset [ value ] [ band ]" },
	{ "radioreg", wl_reg, WLC_GET_RADIOREG, WLC_SET_RADIOREG,
	"Get/Set a radio register:\n"
	"\toffset [ value ] [ band/core ]\n"
	"HTPHY:\n"
	"\tGet a radio register: wl radioreg [ offset ] [ cr0/cr1/cr2 ]\n"
	"\tSet a radio register: wl radioreg [ offset ] [ value ] [ cr0/cr1/cr2/all ]\n"
	"ACPHY:\n"
	"\tGet a radio register: wl radioreg [ offset ] [ cr0/cr1/cr2/pll/pll0/pll1 ]\n"
	"\tSet a radio register: wl radioreg [ offset ] [ value ]"
	" [ cr0/cr1/cr2/pll/pll0/pll1/all ]"},
	{ "phy_afeoverride", wlu_afeoverride, WLC_GET_VAR, WLC_SET_VAR, "g/set AFE override"},
	{ "pcieserdesreg", wlu_reg3args, WLC_GET_VAR, WLC_SET_VAR,
	"g/set SERDES registers: dev offset [val]"},
	{ "txinstpwr", wl_get_instant_power, WLC_GET_VAR, -1,
	"Return tx power based on instant TSSI "},
	{ "evm", wl_evm, -1, WLC_EVM,
	"Start an EVM test on the given channel, or stop EVM test.\n"
	"\tArg 1 is channel number 1-14, or \"off\" or 0 to stop the test.\n"
	"\tArg 2 is optional rate (1, 2, 5.5 or 11)"},
	{ "noise", wl_int, WLC_GET_PHY_NOISE, -1,
	"Get noise (moving average) right after tx in dBm" },
	{ "fqacurcy", wl_int, -1, WLC_FREQ_ACCURACY,
	"Manufacturing test: set frequency accuracy mode.\n"
	"\tfreqacuracy syntax is: fqacurcy <channel>\n"
	"\tArg is channel number 1-14, or 0 to stop the test." },
	{ "crsuprs", wl_int, -1, WLC_CARRIER_SUPPRESS,
	"Manufacturing test: set carrier suppression mode.\n"
	"\tcarriersuprs syntax is: crsuprs <channel>\n"
	"\tArg is channel number 1-14, or 0 to stop the test." },
	{ "longtrain", wl_int, -1, WLC_LONGTRAIN,
	"Manufacturing test: set longtraining mode.\n"
	"\tlongtrain syntax is: longtrain <channel>\n"
	"\tArg is A band channel number or 0 to stop the test." },
	{ "interference", wl_interfere, WLC_GET_INTERFERENCE_MODE, WLC_SET_INTERFERENCE_MODE,
	"NON-ACPHY. Get/Set interference mitigation mode. Choices are:\n"
	"\t0 = none\n"
	"\t1 = non wlan\n"
	"\t2 = wlan manual\n"
	"\t3 = wlan automatic\n"
	"\t4 = wlan automatic with noise reduction"
	"\n\n\tACPHY. Get/Set interference mitigation mode. Bit-Mask:\n"
	"\t0 = desense based on glitches\n"
	"\t1 = limit pktgain based on hwaci (high pwr aci)\n"
	"\t2 = limit pktgain based on w2/nb (high pwr aci)\n"
	"\t3 = enable preemption\n"
	"\t4 = enable HWACI based mitigation\n"
	"\t5 = enable low power detect preemption (requires bit 3 - preemption - to be set too)\n"
	"\t6 = enable hw-obss mitigation\n"
	"\t7 = enable eLNA bypass desense\n"
	"\t8 = enable mclip aci mitigation\n"
	"\tSo a value of 9 would enable glitch based desens + preemption\n"},
	{ "interference_override", wl_interfere_override,
	WLC_GET_INTERFERENCE_OVERRIDE_MODE,
	WLC_SET_INTERFERENCE_OVERRIDE_MODE,
	"NON-ACPHY. Get/Set interference mitigation override. Choices are:\n"
	"\t0 = no interference mitigation\n"
	"\t1 = non wlan\n"
	"\t2 = wlan manual\n"
	"\t3 = wlan automatic\n"
	"\t4 = wlan automatic with noise reduction\n"
	"\t-1 = remove override, override disabled"
	"\n\n\tACPHY. Get/Set interference mitigation mode. Bit-Mask:\n"
	"\t-1 = remove override, override disabled\n"
	"\t0 = desense based on glitches\n"
	"\t1 = limit pktgain based on hwaci (high pwr aci)\n"
	"\t2 = limit pktgain based on w2/nb (high pwr aci)\n"
	"\t3 = enable preemption\n"
	"\t4 = enable HWACI based mitigation\n"
	"\t5 = enable low power detect preemption (requires bit 3 - preemption - to be set too)\n"
	"\t6 = enable hw-obss mitigation\n"
	"\t7 = enable eLNA bypass desense\n"
	"\t8 = enable mclip aci mitigation\n"
	"\tSo a value of 9 would enable glitch based desens + preemption\n"},
	{ "phy_txpwrindex", wl_phy_txpwrindex, WLC_GET_VAR, WLC_SET_VAR,
	"usage: (set) phy_txpwrindex core0_idx core1_idx core2_idx core3_idx"
	"       (get) phy_txpwrindex, return format: core0_idx core1_idx core2_idx core3_idx"
	"Set/Get txpwrindex"
	},
	{ "rssi_cal_freq_grp_2g", wl_rssi_cal_freq_grp_2g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: wl_rssi_cal_freq_grp_2g [chan_1_2,chan_3_4,...,chan_13_14]\n"
	"Each of the variables like - chan_1_2 is a byte"
	"Upper nibble of this byte is for chan1 and lower for chan2"
	"MSB of the nibble tells if the channel is used for calibration"
	"3 LSB's tell which group the channel falls in"
	"Set/get rssi calibration frequency grouping"
	},
	{ "phy_rssi_gain_delta_2gb0", wl_phy_rssi_gain_delta_2g_sub, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_2gb0 [val0 val1 ....]\n"
	"Number of arguments can be - "
	"\t 8 for single core (4345 and 4350)"
	"\t 9 by specifying core_num followed by 8 arguments (4345 and 4350)"
	"\t 16 for both cores (4350)"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_2gb1", wl_phy_rssi_gain_delta_2g_sub, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_2gb1 [val0 val1 ....]\n"
	"Number of arguments can be - "
	"\t 8 for single core (4345 and 4350)"
	"\t 9 by specifying core_num followed by 8 arguments (4345 and 4350)"
	"\t 16 for both cores (4350)"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_2gb2", wl_phy_rssi_gain_delta_2g_sub, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_2gb2 [val0 val1 ....]\n"
	"Number of arguments can be - "
	"\t 8 for single core (4345 and 4350)"
	"\t 9 by specifying core_num followed by 8 arguments (4345 and 4350)"
	"\t 16 for both cores (4350)"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_2gb3", wl_phy_rssi_gain_delta_2g_sub, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_2gb3 [val0 val1 ....]\n"
	"Number of arguments can be - "
	"\t 8 for single core (4345 and 4350)"
	"\t 9 by specifying core_num followed by 8 arguments (4345 and 4350)"
	"\t 16 for both cores (4350)"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_2gb4", wl_phy_rssi_gain_delta_2g_sub, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_2gb4 [val0 val1 ....]\n"
	"Number of arguments can be - "
	"\t 8 for single core (4345 and 4350)"
	"\t 9 by specifying core_num followed by 8 arguments (4345 and 4350)"
	"\t 16 for both cores (4350)"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_2g", wl_phy_rssi_gain_delta_2g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_2g [val0 val1 ....]\n"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_5gl", wl_phy_rssi_gain_delta_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_5gl [val0 val1 ....]\n"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_5gml", wl_phy_rssi_gain_delta_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_5gml [val0 val1 ....]\n"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_5gmu", wl_phy_rssi_gain_delta_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_5gmu [val0 val1 ....]\n"
	"Set/get rssi gain delta values"
	},
	{ "phy_rssi_gain_delta_5gh", wl_phy_rssi_gain_delta_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rssi_gain_delta_5gh [val0 val1 ....]\n"
	"Set/get rssi gain delta values"
	},
	{ "phy_rxgainerr_2g", wl_phy_rxgainerr_2g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rxgainerr_2g [val0 val1 ....]\n"
	"Set/get rx gain delta values"
	},
	{ "phy_rxgainerr_5gl", wl_phy_rxgainerr_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rxgainerr_5gl [val0 val1 ....]\n"
	"Set/get rx gain delta values"
	},
	{ "phy_rxgainerr_5gm", wl_phy_rxgainerr_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rxgainerr_5gml [val0 val1 ....]\n"
	"Set/get rx gain delta values"
	},
	{ "phy_rxgainerr_5gh", wl_phy_rxgainerr_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rxgainerr_5gmu [val0 val1 ....]\n"
	"Set/get rx gain delta values"
	},
	{ "phy_rxgainerr_5gu", wl_phy_rxgainerr_5g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rxgainerr_5gh [val0 val1 ....]\n"
	"Set/get rx gain delta values"
	},
	{ "phy_rxgainerr_6g", wl_phy_rxgainerr_6g, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_rxgainerr_6g [subband(0,1...6) ant0 ant1 ant2 ant3]\n"
	"Set/get rx gain delta values for 6G channel"
	},
	{ "phy_test_tssi", wl_test_tssi, WLC_GET_VAR, -1,
	"wl phy_test_tssi val"},
	{ "phy_test_tssi_offs", wl_test_tssi_offs, WLC_GET_VAR, -1,
	"wl phy_test_tssi_offs val"},
	{ "phy_rssiant", wl_phy_rssiant, WLC_GET_VAR, -1,
	"wl phy_rssiant antindex(0-3)"},
	{ "phy_rssi_ant", wl_phy_rssi_ant, WLC_GET_VAR, WLC_SET_VAR,
	"Get RSSI per antenna (only gives RSSI of current antenna for SISO PHY)"},
	{ "phy_test_idletssi", wl_test_idletssi, WLC_GET_VAR, -1,
	"get idletssi for the given core; wl phy_test_idletssi corenum"},
	{ "phy_setrptbl", wl_var_void, -1, WLC_SET_VAR,
	"populate the reciprocity compensation table based on SROM cal content\n\n"
	"\tusage: wl phy_setrptbl"},
	{ "phy_forceimpbf", wl_var_void, -1, WLC_SET_VAR,
	"force the beamformer into implicit TXBF mode and ready to construct steering matrix\n\n"
	"\tusage: wl phy_forceimpbf"},
	{ "phy_forcesteer", wl_var_setint, -1, WLC_SET_VAR,
	"force the beamformer to apply steering matrix when TXBF is turned on\n\n"
	"\tusage: wl phy_forcesteer 1/0"},
#if defined(BCMDBG)
	{ "phy_force_gainlevel", wl_phy_debug_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"Force rxgain level \n"
	"\t 0 : force to init gain\n"
	"\t 1 : force to clip hi gain\n"
	"\t 2 : force to clip md gain\n"
	"\t 3 : force to clip lo gain\n"
	"\t 4 : force to adc clip gain\n"
	"\t 5 : force to nb clip gain\n"
	"\t 6 : force to wb clip gain\n"
	"\t -1 : disable\n"
	"\t usage: wl phy_force_gainlevel <int32 var>"
	},
#endif // endif
#if defined(BCMDBG)
	{ "phy_force_fdiqi", wl_phy_debug_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable FDIQI Cal/Comp \n"
	"\t 0 : disable\n"
	"\t 1 : enable\n"
	"\t usage: wl phy_force_fdiqi <int32 var>"
	},
#endif // endif
#if defined(BCMDBG)
	{ "phy_btcoex_desense", wl_phy_debug_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"Enable/disable btcoex desense\n"
	"\t 0 : disable\n"
	"\t 1 : mode 1\n"
	"\t usage: wl phy_btcoex_desense <int32 var>"
	},
#endif // endif
	{ "phy_btcoex_desense_rxgain", wl_btcoex_desense_rxgain, WLC_GET_VAR, WLC_SET_VAR,
	"Set the phy btcoex desence rxgain values \n"
	"\t usage: wl phy_btcoex_desense_rxgain band num_cores value1 value2 ..\n"
	"Get the phy btcoex desence rxgain values \n"
	"\t usage: wl phy_btcoex_desense_rxgain ..\n"
	},
	{ "lcnphy_papdepstbl", wl_phy_papdepstbl, -1, WLC_GET_VAR,
	"print papd eps table; Usage: wl lcnphy_papdepstbl"
	},
	{ "rifs", wl_rifs, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the rifs status; usage: wl rifs <1/0> (On/Off)"
	},
	{ "rifs_advert", wl_rifs_advert, WLC_GET_VAR, WLC_SET_VAR,
	"set/get the rifs mode advertisement status; usage: wl rifs_advert <-1/0> (Auto/Off)"
	},
	{ "phy_rxiqest", wl_rxiq, WLC_GET_VAR, -1,
	"Get phy RX IQ noise in dBm:\n"
	"\t-s # of samples (2^n)\n"
	"\t-a antenna select, 0,1,2 or 3\n"
	  "\t-r resolution select, 0 (coarse) or 1 (fine)\n"
	  "\t-f lpf hpc override select, 0 (hpc unchanged) or 1 (overridden to ltrn mode)\n"
	  "\t-w dig lpf override select, 0 (lpf unchanged) or 1 (overridden to ltrn_lpf mode)"
	  "\t or 2 (bypass)\n"
	  "\t-g gain-correction select, 0 (disable), 1(enable full correction) \n"
	  "\t	2 (enable temperature correction) or 3(verify rssi_gain_delta)\n"
	  "\t-e extra INITgain in dB on top of default. Valid values = {0, 3, 6, .., 21, 24}\n"
	  "\t-i gain mode select, 0 (default gain), 1 (fixed high gain) or 4 (fixed low gain)."
	  "\t-n number of averaging iterations.\n"
	  "\t-d delay in usecs between iterations - default 10usecs.\n"
	},
	{ "phy_rxiqest_sweep", wl_rxiq_sweep, WLC_GET_VAR, -1,
	"Get phy RX IQ noise in dBm for requested channels:\n"
	"\t-c\n"
	"\t\tall - All channels"
	"\t\tcomma separated list of channels (e.g. 1,2,4,136)"
	"\t-s # of samples (2^n)\n"
	"\t-a antenna select, 0,1 or 3\n"
	"\t-r resolution select, 0 (coarse) or 1 (fine)\n"
	"\t-f lpf hpc override select, 0 (hpc unchanged) or 1 (overridden to ltrn mode)\n"
	"\t-w dig lpf override select, 0 (lpf unchanged) or 1 (overridden to ltrn_lpf mode)"
	"\t or 2 (bypass)\n"
	"\t-g gain-correction select, 0 (disable), 1(enable full correction) \n"
	"\t     2 (enable temperature correction) or 3(verify rssi_gain_delta)\n"
	"\t-e extra INITgain in dB on top of default. Valid values = {0, 3, 6, .., 21, 24}\n"
	"\t-i gain mode select, 0 (default gain), 1 (fixed high gain) or 4 (fixed low gain). \n"
	"\t-n number of averaging iterations. Max 5 iterations for a sweep of 10 channels or more\n"
	"\t-d delay in usecs between iterations - default 10usecs.\n"
	},
	{ "phy_txiqcc", wl_phy_txiqcc, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_txiqcc [a b]\n"
	"Set/get the iqcc a, b values"
	},
	{ "phy_txlocc", wl_phy_txlocc, WLC_GET_VAR, WLC_SET_VAR,
	"usage: phy_txlocc [di dq ei eq fi fq]\n"
	"Set/get locc di dq ei eq fi fq values"
	},
	{ "phytable", wl_phytable, WLC_GET_VAR, WLC_SET_VAR,
	"usage: wl phytable table_id offset width_of_table_element [table_element]\n"
	"Set/get table element of a table with the given ID at the given offset\n"
	"Note that table width supplied should be 8, 16, 32, 48 or 64\n"
	"table ID, table offset can not be negative"
	},
	{ "force_vsdb_chans", wl_phy_force_vsdb_chans, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get  channels for forced vsdb mode\n"
	"usage: wl force_vsdb_chans chan1 chan2\n"
	"Note: Give chan in the same format as chanspec: eg force_vsdb_chans 1l 48u"
	},
	{ "pavars", wl_phy_pavars, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get temp PA parameters\n"
	"usage: wl down\n"
	"       wl pavars pa2gw0a0=0x1 pa2gw1a0=0x2 pa2gw2a0=0x3 ... \n"
	"       wl pavars\n"
	"       wl up\n"
	"  override the PA parameters after driver attach(srom read), before diver up\n"
	"  These override values will be propogated to HW when driver goes up\n"
	"  PA parameters in one band range (2g, 5gl, 5g, 5gh) must all present if\n"
	"  one of them is specified in the command, otherwise it will be filled with 0"
	},
	{ "povars", wl_phy_povars, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get temp power offset\n"
	"usage: wl down\n"
	"       wl povars cck2gpo=0x1 ofdm2gpo=0x2 mcs2gpo=0x3 ... \n"
	"       wl povars\n"
	"       wl up\n"
	"  override the power offset after driver attach(srom read), before diver up\n"
	"  These override values will be propogated to HW when driver goes up\n"
	"  power offsets in one band range (2g, 5gl, 5g, 5gh) must all present if\n"
	"  one of them is specified in the command, otherwise it will be filled with 0"
	"  cck(2g only), ofdm, and mcs(0-7) for NPHY are supported "
	},
	{ "rpcalvars", wl_phy_rpcalvars, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get temp RPCAL parameters\n"
	"usage: wl down\n"
	"       wl rpcalvars rpcal2g=0x1 \n"
	"       wl rpcalvars\n"
	"       wl up\n"
	"  override the RPCAL parameters after driver attach(srom read), before diver up\n"
	"  These override values will be propogated to HW when driver goes up\n"
	"  Only the RPCAL parameter specified in the command is updated, the rest is untouched"
	},
	{ "rpcalphasevars", wl_phy_rpcalphasevars, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get temp RPCAL PHASE parameters\n"
	"usage: wl down\n"
	"       wl rpcalphasevars rpcal_phase2g=0x1 \n"
	"       wl rpcalphasevars\n"
	"       wl up\n"
	"  override the RPCAL PHASE parameters after driver attach(srom read), before diver up\n"
	"  These override values will be propogated to HW when driver goes up\n"
	"  Only the RPCALPHASE parameter specified in command is updated, the rest is untouched"
	},
	{ "align_sniffer", wl_phy_align_sniffer, WLC_GET_VAR, WLC_SET_VAR,
	"Set/get Ethernet Address of the desired AP to align the sniffer to.\n"
	"usage: wl up\n"
	"       wl align_sniffer 00:00:00:00:00:00 \n"
	"       wl align_sniffer\n"
	"       \n"
	"  Set/get Ethernet Address of the desired AP to align the sniffer to.\n"
	"  Set/get Ethernet Address to all zeros to disable."
	},
	{ "fem", wl_phy_fem, WLC_GET_VAR, WLC_SET_VAR,
	"Set temp fem2g/5g value\n"
	"usage: wl fem (tssipos2g=0x1 extpagain2g=0x2 pdetrange2g=0x1 triso2g=0x1 antswctl2g=0)\n"
	"	(tssipos5g=0x1 extpagain5g=0x2 pdetrange5g=0x1 triso5g=0x1 antswctl5g=0)"
	},
	{ "maxpower", wl_phy_maxpower, WLC_GET_VAR, WLC_SET_VAR,
	"Set temp maxp2g(5g)a0(a1) value\n"
	"usage: wl maxpower maxp2ga0=0x1 maxp2ga1=0x2 maxp5ga0=0xff maxp5ga1=0xff\n"
	"       maxp5gla0=0x3 maxp5gla1=0x4 maxp5gha0=0x5 maxp5gha1=0x6"
	},
	{ "sample_collect", wl_sample_collect, WLC_PHY_SAMPLE_COLLECT, -1,
	"Optional parameters ACPHY/HTPHY/(NPHY with NREV >= 7) are:\n"
	"\t-f File name to dump the sample buffer (default \"sample_collect.dat\")\n"
	"\t-t Trigger condition (default now)\n"
	"\t\t now, good_fcs, bad_fcs, bad_plcp, crs, crs_glitch, crs_deassert\n"
	"\t-b PreTrigger duration in us (default 10)\n"
	"\t-a PostTrigger duration in us (default 10) \n"
	"\t-m Sample collect mode (default 1) \n"
	  "\t\tSC_MODE_0_sd_adc\t\t\t0\n"
	  "\t\tSC_MODE_1_sd_adc_5bits\t\t\t1\n"
	  "\t\tSC_MODE_2_cic0\t\t\t\t2\n"
	  "\t\tSC_MODE_3_cic1\t\t\t\t3\n"
	  "\t\tSC_MODE_4s_rx_farrow_1core\t\t4\n"
	  "\t\tSC_MODE_4m_rx_farrow\t\t\t5\n"
	  "\t\tSC_MODE_5_iq_comp\t\t\t6\n"
	  "\t\tSC_MODE_6_dc_filt\t\t\t7\n"
	  "\t\tSC_MODE_7_rx_filt\t\t\t8\n"
	  "\t\tSC_MODE_8_rssi\t\t\t\t9\n"
	  "\t\tSC_MODE_9_rssi_all\t\t\t10\n"
	  "\t\tSC_MODE_10_tx_farrow\t\t\t11\n"
	  "\t\tSC_MODE_11_gpio\t\t\t\t12\n"
	  "\t\tSC_MODE_12_gpio_trans\t\t\t13\n"
	  "\t\tSC_MODE_14_spect_ana\t\t\t14\n"
	  "\t\tSC_MODE_5s_iq_comp\t\t\t15\n"
	  "\t\tSC_MODE_6s_dc_filt\t\t\t16\n"
	  "\t\tSC_MODE_7s_rx_filt\t\t\t17\n"
	  "\t\t HTPHY: 0=adc, 1..3=adc+rssi, 4=gpio\n"
	"\t\t NPHY: 1=Dual-Core adc[9:2], 2=Core0 adc[9:0], 3=Core1 adc[9:0], gpio=gpio\n"
	"\t-g GPIO mux select (default 0)\n"
	"\t\t use only for gpio mode\n"
	"\t-k GPIO capture mask. For ACPHY written to gpioCapMaskHigh/gpioCapMaskLow\n"
	"\t\t use only for gpio mode (default 0xFFFFFFFF)\n"
	"\t-d Downsample enable (default 0)\n"
	"\t\t use only for HTPHY\n"
	"\t-e BeDeaf enable (default 0)\n"
	"\t-i Timeout in units of 10us. (ACPHY is in 10ms unit) (default 1000)\n"
	"Optional parameters (NPHY with NREV < 7) are:\n"
	"\t-u Sample collect duration in us (default 60)\n"
	"\t-c Cores to do sample collect, only if BW=40MHz (default both)\n"
	"Optional parameters LCN40PHY are:\n"
	"\t-s Trigger State (default 0)\n"
	"\t-x Module_Sel1 (default 2)\n"
	"\t-y Module_Sel2 (default 6)\n"
	"\t-n Number of samples (Max 2048, default 2048)\n"
	"For (NREV < 7), the NPHY buffer returned has the format:\n"
	"\tIn 20MHz [(uint16)num_bytes, <I(core0), Q(core0), I(core1), Q(core1)>]\n"
	"\tIn 40MHz [(uint16)num_bytes(core0), <I(core0), Q(core0)>,\n"
	"\t\t(uint16)num_bytes(core1), <I(core1), Q(core1)>]"},
	{ "pkteng_start", wl_pkteng, -1, WLC_SET_VAR,
	"start packet engine tx usage: wl pkteng_start <xx:xx:xx:xx:xx:xx>"
	" <tx|txwithack> [(async)|sync |sync_unblk] [ipg] [len] [nframes] [src]\n"
	"\tstart packet engine rx usage: wl pkteng_start <xx:xx:xx:xx:xx:xx>"
	" <rx|rxwithack> [(async)|sync] [rxframes] [rxtimeout]\n"
	"\tsync: synchronous mode\n"
	"\tsync_unblk: synchronous unblock mode\n"
	"\tipg: inter packet gap in us\n"
	"\tlen: packet length\n"
	"\tnframes: number of frames; 0 indicates continuous tx test\n"
	"\tsrc: source mac address\n"
	"\trxframes: number of receive frames (sync mode only)\n"
	"\trxtimeout: maximum timout in msec (sync mode only)"},
	{ "get_trig_info", wl_get_trig_info, WLC_GET_VAR, -1,
	"get 11ax trigger frame info: wl get_trig_info"},
	{ "load_trig_info", wl_pkteng_trig_fill, -1, WLC_SET_VAR,
	"fill trigger frame info usuage: wl pkteng_trig_fill [bw] [ru_alloc_index] [mcs_val] "
	"[nss_val] [num_bytes] [cp_ltf_sz] [he_ltf_sym] [stbc_val] [coding_val] [pe_category] "
	"[dcm] [tgtrssi] [MUMIMO LTF mode]\n"
	"\t-b --bandwidth : bandwidth info\n"
	"\t-u --rualloc : ru index number\n"
	"\t-e --he : allocated mcs val\n"
	"\t-s --ss: num of spatial streams\n"
	"\t-len --length : approx packet length to calculate other req parameters\n"
	"\t-g --cp_ltf : guard interval and LTF symbol size\n"
	"\t-ls --ltf_sym : num of HE-LTF symbols\n"
	"\t-st --stbc : stbc support\n"
	"\t-l --ldpc : BCC or LDPC support\n"
	"\t-p --pe_category : PE dur supported \n"
	"\t-dcm : Dual carrier modulation support\n"
	"\t-tgtrssi : target rssi value in encoded format\n"
	"\t-lfm : MUMIMO ltf mode"},
	{ "pkteng_stop", wl_pkteng, -1, WLC_SET_VAR,
	"stop packet engine; usage: wl pkteng_stop <tx|rx>"},
	{ "pkteng_stats", wl_pkteng_stats, -1, WLC_GET_VAR,
	"packet engine stats; usage: wl pkteng_stats:\n"
	"\t-g temperature correction mode, 0 (enabled by default), 1 (disable)"},
	{ "pkteng_status", wl_pkteng_status, -1, WLC_GET_VAR,
	"packet engine status; usage: wl pkteng_status"},
	{"pkteng_cmd", wl_pkteng_cmd, WLC_GET_VAR, WLC_SET_VAR,
	"wl pkteng_cmd [--help TODO]\n"},
	{"phy_force_crsmin", wl_phy_force_crsmin, -1, WLC_SET_VAR,
	"Auto crsmin: \n"
	"       phy_force_crsmin -1\n"
	"Default crsmin value\n\n"
	"       phy_force_crsmin 0\n"
	"Set the crsmin value\n"
	"       phy_force_crsmin core0_th core1_offset core2_offset\n"
	"\n"
	"Threshold values = 2.5 x NoisePwr_dBm + intercept\n"
	"       where\n"
	"              NoisePwr_dBm ~= -36/-33/-30dBm for 20/40/80MHz, respectively\n"
	"              Intercept = 132/125/119 for 20/40/80MHz, respectively"
	},
	{ "radarargs", wl_radar_args, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set Radar parameters in \n"
	"\torder as version, npulses, ncontig, min_pw, max_pw, thresh0, thresh1,\n"
	"\tblank, fmdemodcfg, npulses_lp, min_pw_lp, max_pw_lp,\n"
	"\tmin_fm_lp, max_span_lp, min_deltat, max_deltat,\n"
	"\tautocorr, st_level_time, t2_min, fra_pulse_err, npulses_fra,\n"
	"\tnpulses_stg2, npulses_stg3, percal_mask, quant, \n"
	"\tmin_burst_intv_lp, max_burst_intv_lp, nskip_rst_lp, max_pw_tol, feature_mask, \n"
	"\tthresh0_sc, thresh1_sc"},
	{ "radarargs40", wl_radar_args, WLC_GET_VAR, WLC_SET_VAR,
	"Get/Set Radar parameters for 40Mhz channel in \n"
	"\torder as version, npulses, ncontig, min_pw, max_pw, thresh0, thresh1,\n"
	"\tthresh0_sc, thresh1_sc, blank, fmdemodcfg, npulses_lp, min_pw_lp, max_pw_lp,\n"
	"\tmin_fm_lp, max_span_lp, min_deltat, max_deltat,\n"
	"\tautocorr, st_level_time, t2_min, fra_pulse_err, npulses_fra,\n"
	"\tnpulses_stg2, npulses_stg3, percal_mask, quant, \n"
	"\tmin_burst_intv_lp, max_burst_intv_lp, nskip_rst_lp, max_pw_tol, feature_mask,\n"
	"\tthresh0_sc, thresh1_sc"},
	{ "radarthrs", wl_radar_thrs, -1, WLC_SET_VAR,
	"Set Radar threshold for both 20 & 40MHz & 80MHz BW:\n"
	"\torder as thresh0_20_lo, thresh1_20_lo, thresh0_40_lo, thresh1_40_lo\n"
	"\tthresh0_80_lo, thresh1_80_lo, thresh0_20_hi, thresh1_20_hi\n"
	"\tthresh0_40_hi, thresh1_40_hi, thresh0_80_hi, thresh1_80_hi\n"
	"\tthresh0_160_lo, thresh1_160_lo, thresh0_160_hi, thresh1_160_hi"
	},
	{ "radarthrs2", wl_radar_thrs2, WLC_GET_VAR, WLC_SET_VAR,
	"Set Radar threshold for both 20 & 40MHz & 80MHz BW:\n"
	"\tthresh0_sc_20_lo, thresh1_sc_20_lo, thresh0_sc_40_lo, thresh1_sc_40_lo\n"
	"\tthresh0_sc_80_lo, thresh1_sc_80_lo, thresh0_sc_20_hi, thresh1_sc_20_hi\n"
	"\tthresh0_sc_40_hi, thresh1_sc_40_hi, thresh0_sc_80_hi, thresh1_sc_80_hi\n"
	"\tthresh0_sc_160_lo, thresh1_sc_160_lo, thresh0_sc_160_hi, thresh1_sc_160_hi\n"
	"\tfc_varth_sb, fc_varth_bin5_sb, notradar_enb, max_notradar_lp, max_notradar,\n"
	"\tmax_notradar_lp_sc, max_notradar_sc, highpow_war_enb, highpow_sp_ratio,\n"
	"\tfm_chk_opt, fm_chk_pw, fm_var_chk_pw, fm_thresh_sp1, fm_thresh_sp2,\n"
	"\tfm_thresh_sp3, fm_thresh_etsi4, fm_thresh_p1c, fm_tol_div"},
	{ "phy_dyn_switch_th", wl_phy_dyn_switch_th, WLC_GET_VAR, WLC_SET_VAR,
	"Set wighting number for dynamic switch:\n"
	"\trssi_gain_80_3, rssi_gain_80_2, rssi_gain_80_1, rssi_gain_80_0\n"
	"\trssi_gain_160_3, rssi_gain_160_2, rssi_gain_160_1, rssi_gain_160_0\n"
	"\trssi_th_2, rssi_th_1, rssi_th_0"},
#if defined(WLTEST)
	{ "patrim", wl_patrim, WLC_GET_VAR, -1,
	"Get PA trim option" },
#endif // endif
	{ "phy_tpc_av", wl_phy_tpc_av, WLC_GET_VAR, WLC_SET_VAR,
	"usage: \n\t(set) phy_tpc_av <core> <sub-band> <av-value>"
	"       \n\t(get) phy_tpc_av <core> <sub-band>"
	"\n\tSet/Get Av for the given core and sub-band"
	"\n\tsub-band, 0 for 2G"
	"\n\tsub-band, 1 for 5G-ll"
	"\n\tsub-band, 2 for 5G-lu"
	"\n\tsub-band, 3 for 5G-ul"
	"\n\tsub-band, 4 for 5G-uu"
	"\n\tav-value, 0 to 7"
	},
	{ "phy_tpc_vmid", wl_phy_tpc_vmid, WLC_GET_VAR, WLC_SET_VAR,
	"usage: \n\t(set) phy_tpc_vmid <core> <sub-band> <vmid-value>"
	"       \n\t(get) phy_tpc_vmid <core> <sub-band>"
	"\n\tSet/Get Vmid for the given core and sub-band"
	"\n\tsub-band, 0 for 2G"
	"\n\tsub-band, 1 for 5G-ll"
	"\n\tsub-band, 2 for 5G-lu"
	"\n\tsub-band, 3 for 5G-ul"
	"\n\tsub-band, 4 for 5G-uu"
	"\n\tvmid-value, 0 to 255"
	},
	/* TXCAL IOVARS */
	{"phy_read_estpwrlut", wl_read_estpwrlut, WLC_GET_VAR, -1,
	"Read EstPwr LUT: wl phy_read_estpwrlut core"},
	{ "txcal_gainsweep", wl_txcal_gainsweep, -1, WLC_SET_VAR,
	"start Gain Sweep for TX Cal: wl txcal_gainsweep <xx:xx:xx:xx:xx:xx>"
	" [ipg] [len] [nframes] [gidx_start:step:gidx_stop]\n"
	"\tipg: inter packet gap in us\n"
	"\tlen: packet length\n"
	"\tnframes: number of frames; 0 indicates continuous tx test\n"
	"\tgidx_start: Starting TX gain Index\n"
	"\tgidx_stop: Stopping TX gain Index\n"
	"\tstep:step size for tx gain index increment"},
	{ "txcal_gainsweep_meas", wl_txcal_gainsweep_meas, WLC_GET_VAR, WLC_SET_VAR,
	"Get TSSI/PWR measurments from last TX Cal Gain Sweep: wl txcal_gainsweep_meas\n"
	"Set PWR measurements for TX Cal Gain Sweep: wl txcal_gainsweep_meas core p0 p1 ... p127"},
	{"txcal_pwr_tssi_tbl", wl_txcal_pwr_tssi_tbl, WLC_GET_VAR, WLC_SET_VAR,
	"Get the saved consolidated TSSI/PWR table: wl txcal_pwr_tssi_tbl <core> <chan>\n"
	"\tGenerate consolidated TSSI/PWR table from last TX Cal Gain Sweep:"
	" wl txcal_pwr_tssi_tbl <core> <Ps> <N> <Ch>\n"
	"\t\tPs: Starting Power in 6.3 format\n"
	"\t\tN: Number of entries in the table covering the power range (Ps : (Ps+N-1))\n"
	"\tSet the cosolidated TSSI/PWR table: "
	"wl txcal_pwr_tssi_tbl <core> <Ps> <N> <Ch> <Tssi_Ps Tssi_Ps+1 .. Tssi_Ps+N-1>\n"
	"\t\tPs: Starting Power in 6.3 format\n"
	"\t\tN: Number of entries in the table covering the power range (Ps : (Ps+N-1))\n"
	"\t\tCh: Channel Number\n"
	"\t\tTssi_X: Adjusted TSSI corresponding to Power X\n"
	"\tMax number of channel data allowed: 32\n"},
	{"olpc_anchoridx", wl_olpc_anchoridx, WLC_GET_VAR, WLC_SET_VAR,
	"Get the saved tx power idx and temperature at the olpc anchor power level:\n"
	"wl olpc_anchoridx <core> <chan>\n"
	"Set the temperature and tx power idx at the olpc anchor power level:\n"
	"wl olpc_anchoridx <core> <chan> <idx> <temp>\n"
	"olpc anchor power level is specified via nvram paramter or iovar.\n"},
	{"olpc_offset", wl_olpc_offset, WLC_GET_VAR, WLC_SET_VAR,
	"Get the offset to tx idx to be applied for baseindex calculation in LUT based OLPC\n"
	"wl olpc_offset \n"
	"Set the offset to tx idx to be applied for baseindex calculation in LUT based OLPC\n"
	"wl olpc_offset 2G 5GLow 5GMid 5Ghigh 5GX1\n"},
	{ "phy_snr_ant", wl_phy_snr_ant, WLC_GET_VAR, WLC_SET_VAR,
	"Get SNR per antenna (only gives SNR of current antenna for SISO PHY)"},
	{"phy_dbg_regval", wl_dbg_regval, WLC_GET_VAR, -1,
	"Read phy and radio regs.\n"
	"wl phy_dbg_reval [<<prefix>>]\n"
	"Outputs:\n"
	"<<prefix>>_phy_dump.txt\n"
	"<<prefix>>_radio_dump.txt\n"
	"Outputs: (when optional arg not provided)\n"
	"phy_dump.txt\n"
	"radio_dump.txt\n"},
	{"phy_ulofdma_stats", wl_phy_ulofdma_stats, WLC_GET_VAR, WLC_SET_VAR,
	"Dump the received ULOFDMA per user stats from rxstatus bytes\n"},
	{"phy_tracked_freq", wl_phy_tracked_freq, WLC_GET_VAR, WLC_SET_VAR,
	"Dump and calcuate the tracked frequency offset from rxstatus bytes:\n"},
	{"phy_dssf_setup", wl_phy_dssf_setup, -1, WLC_SET_VAR,
	"Program the DSSF. Usage: \n"
	"\tEnable DSSF stage on a core at inputted freq and notch depth:\n"
	"\t\twl phy_dssf_setup <0/1 - disable/enable> <freq kHz> <depth> <stage> <core>\n"
	"\t\t\t<freq kHz>:  notch frequency in kHz\n"
	"\t\t\t<depth>:     -1 -> automode, 0 -> bypass, 6(dB), 12(dB), 18(dB)\n"
	"\t\t\t<stage>:     1, 2\n"
	"\tDisable all DSSF stages on all cores:\n"
	"\t\twl phy_dssf_setup 0\n"},
	{"phy_temp_counters", wl_phy_temp_counters, WLC_GET_VAR, WLC_SET_VAR,
	"Dump the phy counters:\n"},
	{ "phy_rxiqcal_gtbl", wl_phy_rxiqcal_gtbl, WLC_GET_VAR, WLC_SET_VAR,
	"Usage:\n"
	"\t  (get) en start_idx end_idx\n"
	"\t  (set) en(0/1) start_idx end_idx (idx Optional)"
	},
#ifdef WL_EAP_NOISE_MEASUREMENTS
	{"phynoisebias2ghi", wl_phy_noise_bias_2ghi, WLC_GET_VAR, WLC_SET_VAR,
	"Noise bias for 2G high-gain measurements. Usage:\n"
	"\tphynoisebias2ghi [dB]\n"},
	{"phynoisebias2glo", wl_phy_noise_bias_2glo, WLC_GET_VAR, WLC_SET_VAR,
	"Noise bias for 2G low-gain measurements. Usage:\n"
	"\tphynoisebias2glo [dB]\n"},
	{"phynoisebias5ghi", wl_phy_noise_bias_5ghi, WLC_GET_VAR, WLC_SET_VAR,
	"Noise bias for 5G high-gain measurements, in dB.  Usage:\n"
	"\tphynoisebias5ghi [UNII-1] [UNII-2A] [UNII-2C] [UNII-3]\n"},
	{"phynoisebias5glo", wl_phy_noise_bias_5glo, WLC_GET_VAR, WLC_SET_VAR,
	"Noise bias for 5G low-gain measurements, in dB.  Usage:\n"
	"\tphynoisebias5glo [UNII-1] [UNII-2A] [UNII-2C] [UNII-3]\n"},
	{"phynoisebiasradarhi", wl_phy_noise_bias_5g_radar_hi, WLC_GET_VAR, WLC_SET_VAR,
	"Additional noise bias for high-gain measurements no 5G radar channels, in dB.  Usage:\n"
	"\tphynoisebiasradarhi [UNII-1] [UNII-2A] [UNII-2C] [UNII-3]\n"},
	{"phynoisebiasradarlo", wl_phy_noise_bias_5g_radar_lo, WLC_GET_VAR, WLC_SET_VAR,
	"Additional noise bias for low-gain measurements no 5G radar channels, in dB.  Usage:\n"
	"\tphynoisebiasradarlo [UNII-1] [UNII-2A] [UNII-2C] [UNII-3]\n"},
	{"phynoisebias6ghi", wl_phy_noise_bias_6ghi, WLC_GET_VAR, WLC_SET_VAR,
	"Noise bias for 6G high-gain measurements, in dB.  Usage:\n"
	"\tphynoisebias6ghi [UNII-5] [UNII-6] [UNII-7] [UNII-7]\n"},
	{"phynoisebias6glo", wl_phy_noise_bias_6glo, WLC_GET_VAR, WLC_SET_VAR,
	"Noise bias for 6G low-gain measurements, in dB.  Usage:\n"
	"\tphynoisebias6glo [UNII-5] [UNII-6] [UNII-7] [UNII-8]\n"},
#endif /* WL_EAP_NOISE_MEASUREMENTS */
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_phy_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register phy commands */
	wl_module_cmds_register(wl_phy_cmds);
}

static int
wl_phy_rssi_ant(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	uint i;
	wl_rssi_ant_t *rssi_ant_p;
	struct {
		struct ether_addr ea;
		chanspec_t chanspec;
	} phy_params;
	void *phy_params_addr;
	int phy_params_len;
	void *ptr = NULL;

	if (!*++argv) {
		phy_params_addr = NULL;
		phy_params_len = 0;
	}
	else if (wl_ether_atoe(*argv, &phy_params.ea)) {
		phy_params_addr = &phy_params;
		phy_params_len = ETHER_ADDR_LEN;
		if (*++argv) {
			phy_params.chanspec = dtoh16(strtoul(*argv, NULL, 0));
			phy_params_len += sizeof(chanspec_t);
		}
	}
	else {
		fprintf(stderr, " ERROR: no valid ether addr provided\n");
		return BCME_USAGE_ERROR;
	}

	if ((ret = wlu_var_getbuf(wl, cmd->name, phy_params_addr, phy_params_len, &ptr)) < 0)
		return ret;

	rssi_ant_p = (wl_rssi_ant_t *)ptr;
	rssi_ant_p->version = dtoh32(rssi_ant_p->version);
	rssi_ant_p->count = dtoh32(rssi_ant_p->count);

	if (rssi_ant_p->count == 0) {
		printf("not supported on this chip\n");
	} else {
		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
			if (rssi_ant_p->rssi_ant[i]) {
				printf("rssi[%d] %d  ", i, rssi_ant_p->rssi_ant[i]);
			}
		}
		printf("\n");
	}
	return ret;
}

static int
wl_phy_snr_ant(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	uint i;
	wl_snr_ant_t *snr_ant_p;
	struct ether_addr ea;
	struct ether_addr *ea_p;
	int ea_l;
	void *ptr = NULL;

	if (!*++argv) {
		ea_p = NULL;
		ea_l = 0;
	}
	else if (wl_ether_atoe(*argv, &ea)) {
		ea_p = &ea;
		ea_l = ETHER_ADDR_LEN;
	}
	else {
		fprintf(stderr, " ERROR: no valid ether addr provided\n");
		return BCME_USAGE_ERROR;
	}

	if ((ret = wlu_var_getbuf_sm(wl, cmd->name, ea_p, ea_l, &ptr)) < 0)
		return ret;

	snr_ant_p = (wl_snr_ant_t *)ptr;
	snr_ant_p->version = dtoh32(snr_ant_p->version);
	snr_ant_p->count = dtoh32(snr_ant_p->count);

	if (snr_ant_p->count == 0) {
		printf("not supported on this chip\n");
	} else {
		for (i = WL_ANT_IDX_1; i < WL_RSSI_ANT_MAX; i++) {
			if (snr_ant_p->snr_ant[i]) {
				printf("snr[%d] %d  ", i, snr_ant_p->snr_ant[i]);
			}
		}
		printf("\n");
	}
	return ret;
}

#include <phyioctl_defs.h>

static phy_msg_t wl_phy_msgs[] = {
	{PHYHAL_ERROR,   "error"},
	{PHYHAL_ERROR,   "err"},
	{PHYHAL_TRACE,   "trace"},
	{PHYHAL_INFORM,  "inform"},
	{PHYHAL_TMP,     "tmp"},
	{PHYHAL_TXPWR,   "txpwr"},
	{PHYHAL_CAL,     "cal"},
	{PHYHAL_ACI,     "aci"},
	{PHYHAL_RADAR,   "radar"},
	{PHYHAL_THERMAL, "thermal"},
	{PHYHAL_PAPD,    "papd"},
	{PHYHAL_FCBS,    "fcbs"},
	{PHYHAL_RXIQ,    "rxiq"},
	{PHYHAL_WD,      "wd"},
	{PHYHAL_CHANLOG, "chanlog"},
	{0,              NULL}
};

static int
wl_phymsglevel(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i;
	uint val = 0, last_val = 0;
	uint phymsglevel = 0, phymsglevel_add = 0, phymsglevel_del = 0;
	char *endptr;
	phy_msg_t *phy_msg = wl_phy_msgs;
	const char *cmdname = "phymsglevel";

	UNUSED_PARAMETER(cmd);
	if ((ret = wlu_iovar_getint(wl, cmdname, (int *)&phymsglevel) < 0)) {
		return ret;
	}
	phymsglevel = dtoh32(phymsglevel);
	if (!*++argv) {
		printf("0x%x ", phymsglevel);
		for (i = 0; (val = phy_msg[i].value); i++) {
			if ((phymsglevel & val) && (val != last_val))
				printf(" %s", phy_msg[i].string);
			last_val = val;
		}
		printf("\n");
		return (0);
	}
	while (*argv) {
		char *s = *argv;
		if (*s == '+' || *s == '-')
			s++;
		else
			phymsglevel_del = ~0; /* make the whole list absolute */
		val = strtoul(s, &endptr, 0);
		if (val == 0xFFFFFFFF) {
			fprintf(stderr,
				"Bits >32 are not supported on this driver version\n");
			val = 1;
		}
		/* not an integer if not all the string was parsed by strtoul */
		if (*endptr != '\0') {
			for (i = 0; (val = phy_msg[i].value); i++) {
				if (stricmp(phy_msg[i].string, s) == 0) {
					break;
				}
			}
			if (!val) {
					goto usage;
			}
		}
		if (**argv == '-')
			phymsglevel_del |= val;
		else
			phymsglevel_add |= val;
		++argv;
	}
	phymsglevel &= ~phymsglevel_del;
	phymsglevel |= phymsglevel_add;
	phymsglevel = htod32(phymsglevel);
	return (wlu_iovar_setint(wl, cmdname, (int)phymsglevel));

usage:
	fprintf(stderr, "msg values may be a list of numbers or names from the following set.\n");
	fprintf(stderr, "Use a + or - prefix to make an incremental change.");
	for (i = 0; (val = phy_msg[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, phy_msg[i].string);
		else
			fprintf(stderr, ", %s", phy_msg[i].string);
		last_val = val;
	}
	return 0;
}
static int
wl_get_instant_power(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int i;
	int mode;
	tx_inst_power_t *power;
	tx_inst_power_percore_t *power_percore;
	uint band_list[3];
	uint32 txchain_bitmap = 0;

	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		strcpy(buf, "txinstpwr");
		mode = 0;
		if ((ret = wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN)) < 0) {
			return ret;
		}
	} else {
		if (argv[0] && (!strcmp(argv[0], "--percore"))) {
			strcpy(buf, "txinstpwr_percore");
			mode = 1;
			if ((ret = wlu_get(wl, WLC_GET_VAR, &buf[0], WLC_IOCTL_MAXLEN)) < 0) {
				return ret;
			}
		} else {
			return BCME_BADARG;
		}
	}

	// report pwr est for 1st core
	if (mode == 0) {
		power = (tx_inst_power_t *)buf;

		/* Make the most of the info returned in band_list!
		 * b/g and a
		 * b/g-uni
		 * a-uni
		 * NOTE: NO a and b/g case ...
		 */
		if ((ret = wlu_get(wl, WLC_GET_BANDLIST, band_list, sizeof(band_list))) < 0)
			return (ret);
		band_list[0] = dtoh32(band_list[0]);
		band_list[1] = dtoh32(band_list[1]);
		band_list[2] = dtoh32(band_list[2]);

		/* If B/G is present it's always the lower index */
		if (band_list[1] == WLC_BAND_2G) {
			printf("Last phy CCK est. power:\t%2d.%d dBm\n",
				DIV_QUO((int8)power->txpwr_est_Pout[0], 4),
				DIV_REM(power->txpwr_est_Pout[0], 4));
			printf("Last phy OFDM est. power:\t%2d.%d dBm\n",
				DIV_QUO((int8)power->txpwr_est_Pout_gofdm, 4),
				DIV_REM(power->txpwr_est_Pout_gofdm, 4));
		/* A band */
		} else if (band_list[1] == WLC_BAND_5G ||
				(band_list[0] > 1 && band_list[2] == WLC_BAND_5G)) {
			printf("Last phy OFDM est. power:\t%2d.%d dBm\n",
				DIV_QUO((int8)power->txpwr_est_Pout[1], 4),
				DIV_REM(power->txpwr_est_Pout[1], 4));
		}
	}
	// report pwr est for all cores
	if (mode == 1) {
		power_percore = (tx_inst_power_percore_t *)buf;

		if ((ret = wlu_iovar_get(wl, "txchain", &txchain_bitmap,
				sizeof(txchain_bitmap))) < 0) {
			return (ret);
		}

		printf("Last phy est. power:     ");
		for (i = 0; i < WLC_TXCORE_MAX; i++) {
			if ((txchain_bitmap >> i) & 1) {
				printf("ant[%d]\t%2d.%d dBm,  ", i,
				DIV_QUO((int8)power_percore->txpwr_est_Pout[i], 4),
				DIV_REM(power_percore->txpwr_est_Pout[i], 4));
			} else {
				printf("ant[%d]\t  -    dBm,  ", i);
			}
		}
		printf("\n");
	}
	return ret;
}

static int
wl_evm(void *wl, cmd_t *cmd, char **argv)
{
	int val[3];

	/* Get channel */
	if (!*++argv) {
		fprintf(stderr, "Need to specify at least one parameter\n");
		return BCME_USAGE_ERROR;
	}

	if (!stricmp(*argv, "off"))
		val[0] = 0;
	else
		val[0] = atoi(*argv);

	/* set optional parameters to default */
	val[1] = 4;	/* rate in 500Kb units */
	val[2] = 0;	/* This is ignored */

	/* Get optional rate and convert to 500Kb units */
	if (*++argv)
		val[1] = rate_string2int(*argv);

	val[0] = htod32(val[0]);
	val[1] = htod32(val[1]);
	val[2] = htod32(val[2]);
	return wlu_set(wl, cmd->set, val, sizeof(val));
}

static int
wl_tssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;

	UNUSED_PARAMETER(argv);

	if (cmd->get < 0)
		return -1;
	if ((ret = wlu_get(wl, cmd->get, &val, sizeof(int))) < 0)
		return ret;

	val = dtoh32(val);
	printf("CCK %d OFDM %d\n", (val & 0xff), (val >> 8) & 0xff);
	return 0;
}

static int
wl_atten(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	atten_t atten;
	char *endptr;

	memset(&atten, 0, sizeof(atten_t));

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;

		if ((ret = wlu_get(wl, cmd->get, &atten, sizeof(atten_t))) < 0)
			return ret;

		printf("tx %s bb/radio/ctl1 %d/%d/%d\n",
		       (dtoh16(atten.auto_ctrl) ? "auto" : ""),
			dtoh16(atten.bb), dtoh16(atten.radio), dtoh16(atten.txctl1));

		return 0;
	} else {
		if (cmd->set < 0)
			return -1;

		if (!stricmp(*argv, "auto")) {
			atten.auto_ctrl = WL_ATTEN_PCL_ON;
			atten.auto_ctrl = htod16(atten.auto_ctrl);
		}
		else if (!stricmp(*argv, "manual")) {
			atten.auto_ctrl = WL_ATTEN_PCL_OFF;
			atten.auto_ctrl = htod16(atten.auto_ctrl);
		}
		else {
			atten.auto_ctrl = WL_ATTEN_APP_INPUT_PCL_OFF;
			atten.auto_ctrl = htod16(atten.auto_ctrl);

			atten.bb = (uint16)strtoul(*argv, &endptr, 0);
			atten.bb = htod16(atten.bb);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}

			if (!*++argv)
				return BCME_USAGE_ERROR;

			atten.radio = (uint16)strtoul(*argv, &endptr, 0);
			atten.radio = htod16(atten.radio);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}

			if (!*++argv)
				return BCME_USAGE_ERROR;

			atten.txctl1 = (uint16)strtoul(*argv, &endptr, 0);
			atten.txctl1 = htod16(atten.txctl1);
			if (*endptr != '\0') {
				/* not all the value string was parsed by strtol */
				return BCME_USAGE_ERROR;
			}

		}

		return wlu_set(wl, cmd->set, &atten, sizeof(atten_t));
	}
}

static int
wl_interfere(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr = NULL;
	int mode;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &mode, sizeof(mode))) < 0)
			return ret;
		mode = dtoh32(mode);
		if (phytype == WLC_PHY_TYPE_AC) {
			mode &= dtoh32(ACPHY_ACI_MAX_MODE);
			if (mode == INTERFERE_NONE) {
				printf("All interference mitigation is disabled. (mode 0)\n");
			} else {
				printf("\nMode = %d. Following ACI modes are enabled:\n", mode);
				if (mode & ACPHY_ACI_GLITCHBASED_DESENSE)
					printf("\tbit-mask %d:  Receiver Desense based on glitch "
					       "count\n",
					       ACPHY_ACI_GLITCHBASED_DESENSE);
				if (mode & ACPHY_ACI_HWACI_PKTGAINLMT)
					printf("\tbit-mask %d:  Limit pktgain based on hwaci "
					       "(high pwr aci)\n",
					       ACPHY_ACI_HWACI_PKTGAINLMT);
				if (mode & ACPHY_ACI_W2NB_PKTGAINLMT)
					printf("\tbit-mask %d:  Limit pktgain based on w2/nb "
					       "(high pwr aci)\n",
					       ACPHY_ACI_W2NB_PKTGAINLMT);
				if (mode & ACPHY_ACI_PREEMPTION)
					printf("\tbit-mask %d:  Preemption is enabled\n",
					       ACPHY_ACI_PREEMPTION);
				if (mode & ACPHY_HWACI_MITIGATION)
					printf("\tbit-mask %d: HW ACI Detection + Mitigation\n",
					       ACPHY_HWACI_MITIGATION);
				if (mode & ACPHY_LPD_PREEMPTION)
					printf("\tbit-mask %d: Low Power Detect Preemption\n",
					       ACPHY_LPD_PREEMPTION);
				if (mode & ACPHY_HWOBSS_MITIGATION)
					printf("\tbit-mask %d: HW OBSS Detection + Mitigation\n",
					       ACPHY_HWOBSS_MITIGATION);
				if (mode & ACPHY_ACI_ELNABYPASS)
					printf("\tbit-mask %d: ELNA-BYPASS on very high-pwr "
					       "signal\n", ACPHY_ACI_ELNABYPASS);
				if (mode & ACPHY_MCLIP_ACI_MIT)
					printf("\tbit-mask %d: MCLIP ACI mitigation enabled \n",
					       ACPHY_MCLIP_ACI_MIT);
			}
			printf("\n");
		} else {
			switch (mode & 0x0f) {
			case INTERFERE_NONE:
				printf("All interference mitigation is disabled. (mode 0)\n");
				break;
			case NON_WLAN:
				printf("Non-wireless LAN Interference mitigation is enabled."
					" (mode 1)\n");
				break;
			case WLAN_MANUAL:
				printf("Wireless LAN Interference mitigation is enabled."
					" (mode 2)\n");
				break;
			case WLAN_AUTO:
				printf("Auto Wireless LAN Interference mitigation is enabled and ");
				if (mode & AUTO_ACTIVE)
					printf("active. (mode 3)\n");
				else
					printf("not active. (mode 3)\n");

				break;
			case WLAN_AUTO_W_NOISE:
				printf("Auto Wireless LAN Interference mitigation is enabled and ");
				if (mode & AUTO_ACTIVE)
					printf("active, ");
				else
					printf("not active, ");

				printf("and noise reduction is enabled. (mode 4)\n");
				break;
			}
		}
		return 0;
	} else {
		mode = INTERFERE_NONE;
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return BCME_USAGE_ERROR;
		if (phytype == WLC_PHY_TYPE_AC) {
			if (val > ACPHY_ACI_MAX_MODE) {
				printf("ACPHY Interference mode > ACPHY_ACI_MAX_MODE!\n");
				return -1;
			} else if ((val & ACPHY_LPD_PREEMPTION) && !(val & ACPHY_ACI_PREEMPTION)) {
				fprintf(stderr, "Low Power Detect Preemption (bit 5)"
						" requires Preemption (bit3) to be enabled\n");
				return BCME_USAGE_ERROR;
			} else {
				mode = val;
			}
		} else {
			switch (val) {
			case 0:
				mode = INTERFERE_NONE;
				break;
			case 1:
				mode = NON_WLAN;
				break;
			case 2:
				mode = WLAN_MANUAL;
				break;
			case 3:
				mode = WLAN_AUTO;
				break;
			case 4:
				mode = WLAN_AUTO_W_NOISE;
				break;
			default:
				return BCME_BADARG;
			}
		}

		mode = htod32(mode);
		return wlu_set(wl, cmd->set, &mode, sizeof(mode));
	}
}

static int
wl_interfere_override(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char *endptr;
	int mode;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (!*++argv) {
		if (cmd->get < 0)
			return -1;
		if ((ret = wlu_get(wl, cmd->get, &mode, sizeof(mode))) < 0) {
			return ret;
		}
		printf("\nMode = %d. Following ACI modes :\n", mode);
		mode = dtoh32(mode);
		if (phytype == WLC_PHY_TYPE_AC) {
			if (mode == INTERFERE_OVRRIDE_OFF)
				printf("Interference override disabled.\n");
			else if (mode == INTERFERE_NONE)
				printf("Interference override NONE, "
				       "all mitigation disabled. (mode 0)\n");
			else {
				printf("\nInterference override mode = %d. Following ACI modes "
				       "are enabled:\n", mode);
				if (mode & ACPHY_ACI_GLITCHBASED_DESENSE)
					printf("\tbit-mask %d:  Receiver Desense based on glitch "
					       "count\n",
					       ACPHY_ACI_GLITCHBASED_DESENSE);
				if (mode & ACPHY_ACI_HWACI_PKTGAINLMT)
					printf("\tbit-mask %d:  Limit pktgain based on hwaci "
					       "(high pwr aci)\n",
					       ACPHY_ACI_HWACI_PKTGAINLMT);
				if (mode & ACPHY_ACI_W2NB_PKTGAINLMT)
					printf("\tbit-mask %d:  Limit pktgain based on w2/nb "
					       "(high pwr aci)\n",
					       ACPHY_ACI_W2NB_PKTGAINLMT);
				if (mode & ACPHY_ACI_PREEMPTION)
					printf("\tbit-mask %d:  Preemption is enabled\n",
					       ACPHY_ACI_PREEMPTION);
				if (mode & ACPHY_HWACI_MITIGATION)
					printf("\tbit-mask %d: HW ACI Detection + Mitigation\n",
					       ACPHY_HWACI_MITIGATION);
				if (mode & ACPHY_LPD_PREEMPTION)
					printf("\tbit-mask %d: Low Power Detect Preemption\n",
					       ACPHY_LPD_PREEMPTION);
				if (mode & ACPHY_HWOBSS_MITIGATION)
					printf("\tbit-mask %d: HW OBSS Detection + Mitigation\n",
					       ACPHY_HWOBSS_MITIGATION);
				if (mode & ACPHY_ACI_ELNABYPASS)
					printf("\tbit-mask %d: ELNA-BYPASS on very high-pwr "
					       "signal\n", ACPHY_ACI_ELNABYPASS);
				if (mode & ACPHY_MCLIP_ACI_MIT)
					printf("\tbit-mask %d: MCLIP ACI mitigation enabled \n",
					       ACPHY_MCLIP_ACI_MIT);
			}
			printf("\n");
		} else {
			switch (mode) {
			case INTERFERE_NONE:
				printf("Interference override NONE, "
				"all mitigation disabled. (mode 0)\n");
				break;
			case NON_WLAN:
				printf("Interference override enabled. "
					" Non-wireless LAN Interference mitigation is enabled."
					" (mode 1)\n");
				break;
			case WLAN_MANUAL:
				printf("Interference override enabled.  "
					" Wireless LAN Interference mitigation is enabled."
					" (mode 2)\n");
				break;
			case WLAN_AUTO:
				printf("Interference override enabled. "
					" Interference mitigation is enabled and ");
				if (mode & AUTO_ACTIVE)
					printf("active. (mode 3)\n");
				else
					printf("not active. (mode 3)\n");
				break;
			case WLAN_AUTO_W_NOISE:
				printf("Interference override enabled. "
					" Interference mitigation is enabled and ");
				if (mode & AUTO_ACTIVE)
					printf("active, ");
				else
					printf("not active, ");
				printf("and noise reduction is enabled. (mode 4)\n");
				break;
			case INTERFERE_OVRRIDE_OFF:
				printf("Interference override disabled. \n");
				break;
			}
		}
		return 0;
	} else {
		mode = INTERFERE_NONE;
		val = strtol(*argv, &endptr, 0);
		if (*endptr != '\0')
			return BCME_USAGE_ERROR;
		if (phytype == WLC_PHY_TYPE_AC) {
			if (val > ACPHY_ACI_MAX_MODE) {
				return -1;
			} else if ((val & ACPHY_LPD_PREEMPTION) && !(val & ACPHY_ACI_PREEMPTION)) {
				fprintf(stderr, "Low Power Detect Preemption (bit 5)"
						" requires Preemption (bit3) to be enabled\n");
				return BCME_USAGE_ERROR;
			} else {
				mode = val;
			}
		} else {
			switch (val) {
			case 0:
				mode = INTERFERE_NONE;
				break;
			case 1:
				mode = NON_WLAN;
				break;
			case 2:
				mode = WLAN_MANUAL;
				break;
			case 3:
				mode = WLAN_AUTO;
				break;
			case 4:
				mode = WLAN_AUTO_W_NOISE;
				break;
			case INTERFERE_OVRRIDE_OFF:
				mode = INTERFERE_OVRRIDE_OFF;
				break;
			default:
				return BCME_BADARG;
			}
		}

		mode = htod32(mode);
		return wlu_set(wl, cmd->set, &mode, sizeof(mode));
	}
}

#define ACI_SPIN	"spin"
#define ACI_ENTER	"enter"
#define ACI_EXIT	"exit"
#define ACI_GLITCH	"glitch"

#define NPHY_ACI_ADCPWR_ENTER "adcpwr_enter"
#define NPHY_ACI_ADCPWR_EXIT "adcpwr_exit"
#define NPHY_ACI_REPEAT_CTR "repeat"
#define NPHY_ACI_NUM_SAMPLES "samples"
#define NPHY_ACI_UNDETECT "undetect_sz"
#define NPHY_ACI_LOPWR "loaci"
#define NPHY_ACI_MDPWR "mdaci"
#define NPHY_ACI_HIPWR "hiaci"
#define NPHY_ACI_NOISE_NOASSOC_GLITCH_TH_UP "nphy_noise_noassoc_glitch_th_up"
#define NPHY_ACI_NOISE_NOASSOC_GLITCH_TH_DN "nphy_noise_noassoc_glitch_th_dn"
#define NPHY_ACI_NOISE_ASSOC_GLITCH_TH_UP "nphy_noise_assoc_glitch_th_up"
#define NPHY_ACI_NOISE_ASSOC_GLITCH_TH_DN "nphy_noise_assoc_glitch_th_dn"
#define NPHY_ACI_NOISE_ASSOC_ACI_GLITCH_TH_UP "nphy_noise_assoc_aci_glitch_th_up"
#define NPHY_ACI_NOISE_ASSOC_ACI_GLITCH_TH_DN "nphy_noise_assoc_aci_glitch_th_dn"
#define NPHY_ACI_NOISE_NOASSOC_ENTER_TH "nphy_noise_noassoc_enter_th"
#define NPHY_ACI_NOISE_ASSOC_ENTER_TH "nphy_noise_assoc_enter_th"
#define NPHY_ACI_NOISE_ASSOC_RX_GLITCH_BADPLCP_ENTER_TH \
"nphy_noise_assoc_rx_glitch_badplcp_enter_th"
#define NPHY_ACI_NOISE_ASSOC_CRSIDX_INCR "nphy_noise_assoc_crsidx_incr"
#define NPHY_ACI_NOISE_NOASSOC_CRSIDX_INCR "nphy_noise_noassoc_crsidx_incr"
#define NPHY_ACI_NOISE_CRSIDX_DECR "nphy_noise_crsidx_decr"

#if defined(BWL_FILESYSTEM_SUPPORT)
#if !defined(_CFE_) && !defined(DONGLEBUILD)
static int
wl_do_samplecollect_lcn40(void *wl, wl_samplecollect_args_t *collect, uint8 *buff, FILE *fp)
{
	uint32 cnt;
	int ret = 0;
	uint32 *data;
	int16 IData, QData;
	uint16 wordlength = 14;
	uint16 mask = ((0x1 << wordlength) - 1);
	uint16 wrap = (0x1 << (wordlength - 1));
	uint16 maxd = (0x1 << wordlength);

	ret = wlu_iovar_getbuf(wl, "sample_collect", collect, sizeof(wl_samplecollect_args_t),
		buff, WLC_SAMPLECOLLECT_MAXLEN);

	if (ret)
		return ret;

	data = (uint32*)buff;
	for (cnt = 0; cnt < collect->nsamps; cnt++) {

		IData = data[cnt] & mask;
		QData = ((data[cnt] >> 16) & mask);

		if (IData >= wrap) {
			IData = IData - maxd;
		}
		if (QData >= wrap) {
			QData = QData - maxd;
		}
		fprintf(fp, "%d %d\n", IData, QData);
	}
	return cnt;
}
static int
wl_do_samplecollect_n(void *wl, wl_samplecollect_args_t *collect, uint8 *buff, FILE *fp)
{
	uint16 nbytes;
	int ret = 0;

	ret = wlu_iovar_getbuf(wl, "sample_collect", collect, sizeof(wl_samplecollect_args_t),
		buff, WLC_SAMPLECOLLECT_MAXLEN);

	if (ret)
		return ret;

	/* bytes 1:0 indicate capture length */
	while ((nbytes = ltoh16_ua(buff))) {
		nbytes += 2;
		ret = fwrite(buff, 1, nbytes, fp);
		if (ret != nbytes) {
			fprintf(stderr, "Error writing %d bytes to file, rc %d!\n",
				nbytes, ret);
			ret = -1;
			break;
		} else {
			fprintf(stderr, "Wrote %d bytes\n", nbytes);
		}
		buff += nbytes;
	}
	return (ret);
}
#endif /* !defined(_CFE_) && !defined(DONGLEBUILD) */
#endif /* defined(BWL_FILESYSTEM_SUPPORT) */

#if defined(BWL_FILESYSTEM_SUPPORT)
#if (!defined(_CFE_) && !defined(DONGLEBUILD))
static int
wl_do_samplecollect(void *wl, wl_samplecollect_args_t *collect, int sampledata_version,
	uint32 *buff, FILE *fp)
{
	uint16 nbytes, tag;
	uint32 flag, *header, sync;
	uint8 *ptr;
	int err;
	wl_sampledata_t *sample_collect;
	wl_sampledata_t sample_data, *psample;

	err = wlu_iovar_getbuf(wl, "sample_collect", collect, sizeof(wl_samplecollect_args_t),
		buff, WLC_SAMPLECOLLECT_MAXLEN);

	if (err)
		return err;

	sample_collect = (wl_sampledata_t *)buff;
	header = (uint32 *)&sample_collect[1];
	tag = ltoh16_ua(&sample_collect->tag);
	if (tag != WL_SAMPLEDATA_HEADER_TYPE) {
		fprintf(stderr, "Expect SampleData Header type %d, receive type %d\n",
			WL_SAMPLEDATA_HEADER_TYPE, tag);
		return -1;
	}

	nbytes = ltoh16_ua(&sample_collect->length);
	flag = ltoh32_ua(&sample_collect->flag);
	sync = ltoh32_ua(&header[0]);
	if (sync != 0xACDC2009) {
		fprintf(stderr, "Header sync word mismatch (0x%08x)\n", sync);
		return -1;
	}

	err = fwrite((uint8 *)header, 1, nbytes, fp);
	if (err != (int)nbytes)
		  fprintf(stderr, "Failed write file-header to file %d\n", err);

	memset(&sample_data, 0, sizeof(wl_sampledata_t));
	sample_data.version = sampledata_version;
	sample_data.size = htol16(sizeof(wl_sampledata_t));
	flag = 0;
	/* new format, used in htphy */
	do {
		sample_data.tag = htol16(WL_SAMPLEDATA_TYPE);
		sample_data.length = htol16(WLC_SAMPLECOLLECT_MAXLEN);
		/* mask seq# */
	        sample_data.flag = htol32((flag & 0xff));

		err = wlu_iovar_getbuf(wl, "sample_data", &sample_data, sizeof(wl_sampledata_t),
			buff, WLC_SAMPLECOLLECT_MAXLEN);
		if (err) {
			fprintf(stderr, "Error reading back sample collected data\n");
			err = -1;
			break;
		}

		ptr = (uint8 *)buff + sizeof(wl_sampledata_t);
		psample = (wl_sampledata_t *)buff;
		tag = ltoh16_ua(&psample->tag);
		nbytes = ltoh16_ua(&psample->length);
		flag = ltoh32_ua(&psample->flag);
		if (tag != WL_SAMPLEDATA_TYPE) {
			fprintf(stderr, "Expect SampleData type %d, receive type %d\n",
				WL_SAMPLEDATA_TYPE, tag);
			err = -1;
			break;
		}
		if (nbytes == 0) {
			fprintf(stderr, "Done retrieving sample data\n");
			err = -1;
			break;
		}

		err = fwrite(ptr, 1, nbytes, fp);
		if (err != (int)nbytes) {
			fprintf(stderr, "Error writing %d bytes to file, rc %d!\n",
				(int)nbytes, err);
			err = -1;
			break;
		} else {
			printf("Wrote %d bytes\n", err);
			err = 0;
		}
	} while (flag & WL_SAMPLEDATA_MORE_DATA);
	return err;
}
#endif /* (!defined(_CFE_) && !defined(DONGLEBUILD)) */
#endif /* defined(BWL_FILESYSTEM_SUPPORT) */

static int
wl_sample_collect(void *wl, cmd_t *cmd, char **argv)
{
#if !defined(BWL_FILESYSTEM_SUPPORT)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return (-1);
#elif defined(_CFE_)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return CFE_ERR_UNSUPPORTED;
#elif defined(DONGLEBUILD)
	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd); UNUSED_PARAMETER(argv);
	return 0;
#else
	int ret = -1;
	uint8 *buff = NULL;
	wl_samplecollect_args_t collect;
	wlc_rev_info_t revinfo;
	uint32 phytype;
	uint32 phyrev;
	const char *fname = "sample_collect.dat";
	FILE *fp = NULL;

	/* Default setting for sampledata_version */
	int sampledata_version = htol16(WL_SAMPLEDATA_T_VERSION);

	UNUSED_PARAMETER(cmd);

	memset(&revinfo, 0, sizeof(revinfo));
	if ((ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return ret;

	phytype = dtoh32(revinfo.phytype);
	phyrev = dtoh32(revinfo.phyrev);

	/* Assign some default params first */
	/* 60us is roughly the max we can store (for NPHY with NREV < 7). */
	collect.coll_us = 60;
	collect.cores = -1;
	collect.bitStart = -1;
	/* extended settings */
	collect.trigger = TRIGGER_NOW;
	collect.mode = 1;
	collect.post_dur = 10;
	collect.pre_dur = 10;
	collect.gpio_sel = 0;
	collect.gpioCapMask = (uint32)-1;
	collect.downsamp = FALSE;
	collect.be_deaf = FALSE;
	collect.timeout = 1000;
	collect.agc = FALSE;
	collect.filter = FALSE;
	collect.trigger_state = 0;
	collect.module_sel1 = 2;
	collect.module_sel2 = 6;
	collect.nsamps = 2048;
	collect.version = WL_SAMPLECOLLECT_T_VERSION;
	collect.length = sizeof(wl_samplecollect_args_t);

	/* Skip the command name */
	argv++;
	ret = -1;
	while (*argv) {
		char *s = *argv;

		if (argv[1] == NULL) {
			ret = BCME_USAGE_ERROR;
			goto exit;
		}
		if (!strcmp(s, "-f")) {
			fname = argv[1];
		} else if (!strcmp(s, "-u"))
			collect.coll_us = atoi(argv[1]);
		else if (!strcmp(s, "-c"))
			collect.cores = atoi(argv[1]);
		/* extended settings */
		else if (!strcmp(s, "-t")) {
			/* event trigger */
			if (!strcmp(argv[1], "crs"))
				collect.trigger = TRIGGER_CRS;
			else if (!strcmp(argv[1], "crs_deassert"))
				collect.trigger = TRIGGER_CRSDEASSERT;
			else if (!strcmp(argv[1], "good_fcs"))
				collect.trigger = TRIGGER_GOODFCS;
			else if (!strcmp(argv[1], "bad_fcs"))
				collect.trigger = TRIGGER_BADFCS;
			else if (!strcmp(argv[1], "bad_plcp"))
				collect.trigger = TRIGGER_BADPLCP;
			else if (!strcmp(argv[1], "crs_glitch"))
				collect.trigger = TRIGGER_CRSGLITCH;
		}
		else if (!strcmp(s, "-m")) {
			if (!strcmp(argv[1], "gpio")) {
				if (phytype == WLC_PHY_TYPE_HT) {
					collect.mode = 4;
				} else {
					/* MIMOPHY */
					collect.mode = 0xff;
				}
			} else {
			collect.mode = atoi(argv[1]);
			}
		}
		else if (!strcmp(s, "-k"))
			collect.gpioCapMask = atoi(argv[1]);
		else if (!strcmp(s, "-s"))
			collect.bitStart = atoi(argv[1]);
		else if (!strcmp(s, "-b"))
			collect.pre_dur = atoi(argv[1]);
		else if (!strcmp(s, "-a"))
			collect.post_dur = atoi(argv[1]);
		else if (!strcmp(s, "-g"))
			collect.gpio_sel = atoi(argv[1]);
		else if (!strcmp(s, "-d"))
			collect.downsamp = atoi(argv[1]);
		else if (!strcmp(s, "-e"))
			collect.be_deaf = atoi(argv[1]);
		else if (!strcmp(s, "-i"))
			collect.timeout = atoi(argv[1]);
		else if (!strcmp(s, "--agc")) {
			/* perform software agc for sample collect */
			collect.agc = atoi(argv[1]);
		}
		else if (!strcmp(s, "--filter")) {
			/* Set HPC for LPF to lowest possible value (0x1) */
			collect.filter = atoi(argv[1]);
		}
		else if (!strcmp(s, "-v"))
			 sampledata_version = atoi(argv[1]);
		else if (!strcmp(s, "-s"))
			collect.trigger_state = atoi(argv[1]);
		else if (!strcmp(s, "-x"))
			collect.module_sel1 = atoi(argv[1]);
		else if (!strcmp(s, "-y"))
			collect.module_sel2 = atoi(argv[1]);
		else if (!strcmp(s, "-n"))
			collect.nsamps = atoi(argv[1]);
		else {
			ret = BCME_USAGE_ERROR;
			goto exit;
		}

		argv += 2;
	}

	buff = CALLOC(WLC_SAMPLECOLLECT_MAXLEN);
	if (buff == NULL) {
		fprintf(stderr, "Failed to allocate dump buffer of %d bytes\n",
			WLC_SAMPLECOLLECT_MAXLEN);
		return BCME_NOMEM;
	}

	if ((fp = fopen(fname, "wb")) == NULL) {
		fprintf(stderr, "Problem opening file %s\n", fname);
		ret = BCME_BADARG;
		goto exit;
	}

	if ((phytype == WLC_PHY_TYPE_HT) || (phytype == WLC_PHY_TYPE_AC)) {
		ret = wl_do_samplecollect(wl, &collect, sampledata_version, (uint32 *)buff, fp);
	}
	else if (phytype == WLC_PHY_TYPE_N) {
		if (phyrev < 7) {
		ret = wl_do_samplecollect_n(wl, &collect, buff, fp);
		} else {
			ret = wl_do_samplecollect(wl, &collect, sampledata_version,
				(uint32 *)buff, fp);
		}
	} else if (phytype == WLC_PHY_TYPE_LCN40) {
		if (collect.nsamps > (WLC_SAMPLECOLLECT_MAXLEN >> 2)) {
			fprintf(stderr, "Max number of samples supported = %d\n",
				WLC_SAMPLECOLLECT_MAXLEN >> 2);
			ret = -1;
			goto exit;
		}
		ret = wl_do_samplecollect_lcn40(wl, &collect, buff, fp);
	}
exit:
	if (buff) free(buff);
#ifndef ATE_BUILD
	if (fp) fclose(fp);
#endif // endif
	return ret;
#endif /* !BWL_FILESYSTEM_SUPPORT */
}

static int
wl_test_tssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char* endptr = NULL;

	/* toss the command name */
	argv++;

	if (!*argv)
		return BCME_USAGE_ERROR;

	val = htod32(strtol(*argv, &endptr, 0));
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer\n", *argv);
		return BCME_USAGE_ERROR;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, &val, sizeof(val),
	                      buf, WLC_IOCTL_MAXLEN);

	if (ret)
		return ret;

	val = dtoh32(*(int*)buf);

	wl_printint(val);

	return ret;
}

static int
wl_test_tssi_offs(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char* endptr = NULL;

	/* toss the command name */
	argv++;

	if (!*argv)
		return BCME_USAGE_ERROR;

	val = htod32(strtol(*argv, &endptr, 0));
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer\n", *argv);
		return BCME_USAGE_ERROR;
	}

	ret = wlu_iovar_getbuf(wl, cmd->name, &val, sizeof(val),
	                      buf, WLC_IOCTL_MAXLEN);

	if (ret)
		return ret;

	val = dtoh32(*(int*)buf);

	wl_printint(val);

	return ret;
}

static int
wl_test_idletssi(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	int val;
	char* endptr = NULL;

	/* toss the command name */
	argv++;

	if (!*argv)
		return BCME_USAGE_ERROR;

	val = htod32(strtol(*argv, &endptr, 0));
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer\n", *argv);
		return -1;
	}

	if ((ret = wlu_iovar_getbuf(wl, cmd->name, &val, sizeof(val),
		buf, WLC_IOCTL_MAXLEN)) >= 0) {
		val = dtoh32(*(int*)buf);
		wl_printint(val);
	}

	return ret;
}

static int
wl_phy_rssiant(void *wl, cmd_t *cmd, char **argv)
{
	uint32 antindex;
	int buflen, err;
	char *param;
	int16 antrssi;

	if (!*++argv) {
		printf(" Usage: %s antenna_index[0-3]\n", cmd->name);
		return BCME_USAGE_ERROR;
	}

	antindex = htod32(atoi(*argv));

	strcpy(buf, "nphy_rssiant");
	buflen = strlen(buf) + 1;
	param = (char *)(buf + buflen);
	memcpy(param, (char*)&antindex, sizeof(antindex));

	if ((err = wlu_get(wl, cmd->get, buf, WLC_IOCTL_MAXLEN)) < 0)
		return err;

	antindex = dtoh32(antindex);
	antrssi = dtoh16(*(int16 *)buf);
	printf("\nnphy_rssiant ant%d = %d\n", antindex, antrssi);

	return (0);
}

static int
wl_pkteng_stats(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkteng_stats_t *stats;
	void *ptr = NULL;
	int err, argc, opt_err;
	uint16 *pktstats;
	int i, j;
	miniopt_t to;
	uint8 gain_correct = 0;
	const char* fn_name = "wl_pkteng_stats";
	int32 tmp;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	if (argc != 0) {
		miniopt_init(&to, fn_name, NULL, FALSE);
		while ((opt_err = miniopt(&to, argv)) != -1) {
			if (opt_err == 1) {
				err = BCME_USAGE_ERROR;
				goto exit;
			}
			argv += to.consumed;

			if (to.opt == 'g') {
				if (!to.good_int) {
					fprintf(stderr,
						"%s: could not parse \"%s\" as an int"
						" for gain-correction (0, 1)\n",
						fn_name, to.valstr);

					err = BCME_BADARG;
					goto exit;
				}
				if ((to.val < 0) || (to.val > 1)) {
					fprintf(stderr, "%s: invalid gain-correction select %d"
						" (0,1)\n", fn_name, to.val);
					err = BCME_BADARG;
					goto exit;
				}
				gain_correct = to.val & 0xf;
			}
		}
	}

	if ((err = wlu_var_getbuf_sm(wl, cmd->name, &gain_correct, 1, &ptr)) < 0)
		return err;

	stats = ptr;
	stats->rssi = dtoh32(stats->rssi);
	stats->rssi_qdb = dtoh32(stats->rssi_qdb);
	tmp = (stats->rssi << 2) + (stats->rssi_qdb);
	printf("Lost frame count %d\n", dtoh32(stats->lostfrmcnt));

	if (stats->version == 1) {
		/* For rssi format of the following:
		   rssi: rxpwr_qdBm >>2
		   rssi_qdb: rxpwr_qdBm &3
		*/
		if (tmp < 0) {
			tmp = -tmp;
			printf("RSSI -%d.%02d\n", (tmp >> 2), (tmp & 0x3)*25);
		} else {
			printf("RSSI %d.%02d\n", (tmp >> 2), (tmp & 0x3)*25);
		}
	} else {
		printf("RSSI %d.%02d\n", stats->rssi, 25*stats->rssi_qdb);
	}
	printf("Signal to noise ratio %d\n", dtoh32(stats->snr));
	printf("rx1mbps %d rx2mbps %d rx5mbps5 %d\n"
		"rx6mbps %d rx9mbps %d, rx11mbps %d\n"
		"rx12mbps %d rx18mbps %d rx24mbps %d\n"
		"rx36mbps %d rx48mbps %d rx54mbps %d\n",
		stats->rxpktcnt[3], stats->rxpktcnt[1], stats->rxpktcnt[2],
		stats->rxpktcnt[7], stats->rxpktcnt[11], stats->rxpktcnt[0],
		stats->rxpktcnt[6], stats->rxpktcnt[10], stats->rxpktcnt[5],
		stats->rxpktcnt[9], stats->rxpktcnt[4], stats->rxpktcnt[8]);
	pktstats = &stats->rxpktcnt[NUM_80211b_RATES+NUM_80211ag_RATES];
	for (i = 0; i < NUM_80211n_RATES/4; i++) {
		for (j = 0; j < 4; j++) {
			printf("rxmcs%d %d ", j+4*i, pktstats[j+4*i]);
		}
		printf("\n");
	}
	printf("rxmcsother %d\n", stats->rxpktcnt[NUM_80211_RATES]);
	return 0;

exit:
	return err;
}

static int
wl_pkteng_status(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int status = 0;

	BCM_REFERENCE(argv);
	if ((err = wlu_iovar_get(wl, cmd->name, &status, sizeof(status))) < 0) {
		return err;
	}

	printf("pkteng_status : %d \n", status);
	return 0;
}

#define LPPHY_PAPD_EPS_TBL_SIZE 64
static int
wl_phy_papdepstbl(void *wl, cmd_t *cmd, char **argv)
{
	int32 eps_real, eps_imag;
	int i;
	uint32 eps_tbl[LPPHY_PAPD_EPS_TBL_SIZE];
	int err;

	UNUSED_PARAMETER(argv);

	if ((err = wlu_iovar_get(wl, cmd->name, &eps_tbl, sizeof(eps_tbl))) < 0)
		return err;

	for (i = 0; i < LPPHY_PAPD_EPS_TBL_SIZE; i++) {
		if ((eps_real = (int32)(eps_tbl[i] >> 12)) > 0x7ff)
				eps_real -= 0x1000; /* Sign extend */
		if ((eps_imag = (int32)(eps_tbl[i] & 0xfff)) > 0x7ff)
				eps_imag -= 0x1000; /* Sign extend */
		printf("%d %d\n", eps_real, eps_imag);
	}

	return 0;
}

static int
wl_phy_txiqcc(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err;
	int32 iqccValues[4];
	int32 value;
	char *endptr;
	int32 a, b, a1, b1;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	if ((err = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return err;

	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_N) {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, iqccValues, 2*sizeof(int32))) < 0)
				return err;
			a = (int16)iqccValues[0];
			b = (int16)iqccValues[1];
			/* sign extend a, b from 10 bit signed value to 32 bit signed value */
			a = ((a << 22) >> 22);
			b = ((b << 22) >> 22);
			printf("%d  %d\n", a, b);
		}
		else
		{
			for (i = 0; i < 2; i++) {
				value = strtol(*argv++, &endptr, 0);
				if (value > 511 || value < -512) {
					return BCME_BADARG;
				}
				iqccValues[i] = value;
			}

		if ((err = wlu_var_setbuf(wl, cmd->name, iqccValues, 2*sizeof(int32))) < 0)
			return err;
		}
	} else {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, iqccValues, 4*sizeof(int32))) < 0)
				return err;
			a = (int16)iqccValues[0];
			b = (int16)iqccValues[1];
			a1 = (int16)iqccValues[2];
			b1 = (int16)iqccValues[3];
			/* sign extend a, b from 10 bit signed value to 32 bit signed value */
			a = ((a << 22) >> 22);
			b = ((b << 22) >> 22);
			a1 = ((a1 << 22) >> 22);
			b1 = ((b1 << 22) >> 22);
			printf("%d  %d  %d  %d\n", a, b, a1, b1);
		}
		else
		{
			for (i = 0; i < 4; i++) {
				value = strtol(*argv++, &endptr, 0);
				if (value > 511 || value < -512) {
					return BCME_BADARG;
				}
				iqccValues[i] = value;
			}

			if ((err = wlu_var_setbuf(wl, cmd->name, iqccValues, 4*sizeof(int32))) < 0)
				return err;
		}
	}

	return 0;
}

static int
wl_phy_txlocc(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err;
	int8 loccValues[12];
	int32 value;
	char *endptr;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	if ((err = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
		return err;

	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_N) {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, loccValues,
				sizeof(loccValues))) < 0)
				return err;

			/* sign extend the loccValues */
			loccValues[2] = (loccValues[2] << 3) >> 3;
			loccValues[3] = (loccValues[3] << 3) >> 3;
			loccValues[4] = (loccValues[4] << 3) >> 3;
			loccValues[5] = (loccValues[5] << 3) >> 3;

			printf("%d  %d  %d  %d  %d  %d\n", loccValues[0],
				loccValues[1], loccValues[2], loccValues[3],
				loccValues[4], loccValues[5]);
		}
		else
		{
			for (i = 0; i < 6; i++) {
				value = strtol(*argv++, &endptr, 0);
				if ((i >= 2) && (value > 15 || value < -15)) {
					return BCME_BADARG;
				}
				loccValues[i] = (int8)value;
			}

			if ((err = wlu_var_setbuf(wl, cmd->name, loccValues, 6*sizeof(int8))) < 0)
				return err;
		}
	} else {
		if (!*++argv) {
			if ((err = wlu_iovar_get(wl, cmd->name, loccValues,
				sizeof(loccValues))) < 0)
				return err;

			/* sign extend the loccValues */
			loccValues[2] = (loccValues[2] << 3) >> 3;
			loccValues[3] = (loccValues[3] << 3) >> 3;
			loccValues[4] = (loccValues[4] << 3) >> 3;
			loccValues[5] = (loccValues[5] << 3) >> 3;
			loccValues[8] = (loccValues[8] << 3) >> 3;
			loccValues[9] = (loccValues[9] << 3) >> 3;
			loccValues[10] = (loccValues[10] << 3) >> 3;
			loccValues[11] = (loccValues[11] << 3) >> 3;

			printf("%d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d  %d\n", loccValues[0],
				loccValues[1], loccValues[2], loccValues[3], loccValues[4],
				loccValues[5], loccValues[6], loccValues[7], loccValues[8],
				loccValues[9], loccValues[10], loccValues[11]);
		}
		else
		{
			for (i = 0; i < 12; i++) {
				value = strtol(*argv++, &endptr, 0);
				if (((i < 2) && (value > 63 || value < -64)) ||
					((i >= 2) && (value > 15 || value < -15))) {
					return BCME_BADARG;
				}
				loccValues[i] = (int8)value;
			}

			if ((err = wlu_var_setbuf(wl, cmd->name, loccValues, 12*sizeof(int8))) < 0)
				return err;
		}
	}

	return 0;
}

static int
wl_rssi_cal_freq_grp_2g(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err = -1;
	uint8 nvramValues[14];
	char *endptr;
	uint8 N = 0;

	if (!*++argv) {
		/* Reading the NVRAM variable */
		if ((err = wlu_iovar_get(wl, cmd->name, nvramValues, sizeof(nvramValues))) < 0)
			return err;

		N = 14; /* 14 corresponds to number of channels in 2g */

		for (i = 0; i < N-2; i++) {
			printf("0x%x%x,", nvramValues[i], nvramValues[i+1]);
			i++;
		}
		printf("0x%x%x\n", nvramValues[i], nvramValues[i+1]);
	} else {
		/* Writing to NVRAM variable */

		char *splt;
		int8 tmp;
		splt = strtok(*argv, ",");

		/* N = 14 corresponds to number of channels in 2g */
		/* N = N /2 to package 2 channel's nibbles into 1 byte */
		N = 7;

		i = 0;
		while (splt != NULL) {
			/* Splitting the input based on charecter ','
			 * Further each byte is divided into 2 nibbles
			 * and saved into 2 elements of array.
			 */
			tmp = strtol(splt, &endptr, 0);
			nvramValues[i] = (tmp >> 4) & 0xf;
			i++;
			nvramValues[i] = tmp & 0xf;
			splt = strtok(NULL, ",");
			i++;
		}
		if (i != 14) {
			printf("Insufficient arguments \n");
			return BCME_BADARG;
		}
		if ((err = wlu_var_setbuf(wl, cmd->name, nvramValues, N*2*sizeof(int8))) < 0)
			return err;
	}

	return 0;
}

static int
wl_phy_rssi_gain_delta_2g_sub(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err = -1;
	int8 deltaValues[28];
	int32 value;
	char *endptr;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;
	uint8 N = 0;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0) {
			return err;
		}
		N = 27; /* 9 entries per core, 43602WLCSP - 27 MAX entried;
					 * 4350 - 18 MAX entries; 4345 9 MAX entries
					 */
		for (i = 0; i < N; i++) {
			if (i%9 == 0 && i > 0) {
				printf("\n");
				if (deltaValues[i] == -1) {
					break;
				}
			}

			printf("%d ", deltaValues[i]);
		}
		if (i == N)
			printf("\n");
	} else {
		int argc = 0;
		int index = 0;
		while (argv[argc])
			argc++;

		/* ACPHY : 8/9 entries for a core; core 0 delta's can be
		 * given with or with out core_num as first element
		 */
		N = argc;

		if (!(N == 9 || N == 8 || N == 16 || N == 24)) {
		  printf("Incorrect number of arguments.\n");
		  return 0;
		}

		for (i = 0; i < N; i++) {
			value = strtol(*argv++, &endptr, 0);
			if ((value > 63 || value < -64)) {
				return BCME_BADARG;
			}
			if (argc == 9) {
				/* If number of arguments is 9, then core
				 * number has been provided.
				 * And 8 elements related to 2
				 * (BWs - 20 n 40) and 4 gain settings
				 * (elna_on, elna_off, rout_1, rout_2) are
				 * provided. So, 2 * 4 = 8 + 1 core_num = 9
				 */
				deltaValues[i] = (int8)value;
			} else {
				/* If the number of elements is not eq to 9,
				 * then, core number was not provided.
				 * So, if only 8 elements are provided, only
				 * core 0's info is given. So, for i = 0,
				 * deltaValues element is 0 (core_num). If 16
				 * elements are provided, then core 0 and 1's info is
				 * provided. So, i =0 element has core_num = 0,
				 * then, next 8 elements are core 0's
				 * deltas. For i = 8, core 1's core_num = 1
				 * is inserted into deltaValues array.
				 * Similarly for third core data.
				 */
				if (i == 0) {
					deltaValues[index] = 0;
					index++;
				} else if (i == 8) {
					deltaValues[index] = 1;
					index++;
				} else if (i == 16) {
					deltaValues[index] = 2;
					index++;
				}
				deltaValues[index] = (int8)value;
				index++;
			}
		}
		/* If argc == 8, then only 1 core's info was given,
		 * so, setbuf() is called once.
		 * If argc == 16 then core 0 and 1's info was given.
		 * So, setbuf() is called twice.
		 * If argc == 24 then core 0, 1 and 2's info was given.
		 * So, setbuf() is called thrice.
		 */
		if ((err = wlu_var_setbuf(wl, cmd->name,
		     deltaValues, 9*sizeof(int8))) < 0)
			return err;
		if (argc >= 16) {
			if ((err = wlu_var_setbuf(wl, cmd->name,
			      deltaValues + 9, 9*sizeof(int8))) < 0)
				return err;
		}
		if (argc == 24) {
			if ((err = wlu_var_setbuf(wl, cmd->name,
			     deltaValues + 18, 9*sizeof(int8))) < 0)
				return err;
		}
	}

	return 0;
}

static int
wl_phy_rssi_gain_delta_2g(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err = -1;
	int8 deltaValues[18];
	int32 value;
	char *endptr;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;
	uint8 N = 0;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;
		if (phytype == WLC_PHY_TYPE_AC)
			N = 15; /* ACPHY: 3 cores max x 5 entries */
		for (i = 0; i < N; i++) {
			if ((phytype == WLC_PHY_TYPE_AC) && (i%5 == 0)) {
				if (i > 0) printf("\n");
				if (deltaValues[i] == -1) break;
			}
			printf("%d ", deltaValues[i]);
		}
		if (i == N)
			printf("\n");
	} else {
		int argc = 0;
		int index = 0;
		while (argv[argc])
			argc++;
		if (phytype == WLC_PHY_TYPE_AC) {
			/* N = 5;  ACPHY : 5 entries for a core */
			N = argc;
		}

		for (i = 0; i < N; i++) {
			value = strtol(*argv++, &endptr, 0);
			if ((value > 63 || value < -64)) {
				return BCME_BADARG;
			}
			if (argc == 5) {
				/* If number of arguments is 5, then core number has been provided.
				 * And 8 elements related to 2 (BWs - 20 n 40) and 2 gain settings
				 * (elna_on, elna_off) are provided. So, 2 * 2 = 4 + 1 core_num = 5
				 */
				deltaValues[i] = (int8)value;
			} else {
				/* If the number of elements is not eq to 5,
				 * then, core number was not provided.
				 * So, if only 4 elements are provided, only
				 * core 0's info is given. So, for i = 0,
				 * deltaValues element is 0 (core_num). If 8
				 * elements are provided, then core 0 and 1's info is
				 * provided. So, i =0 element has core_num = 0,
				 * then, next 4 elements are core 0's
				 * deltas. For i = 4, core 1's core_num = 1
				 * is inserted into deltaValues array.
				 * Similarly for third core data.
				 */
				if (i == 0) {
					deltaValues[index] = 0;
					index++;
				} else if (i == 4) {
					deltaValues[index] = 1;
					index++;
				} else if (i == 8) {
					deltaValues[index] = 2;
					index++;
				}
				deltaValues[index] = (int8)value;
				index++;
			}

		}
		/* If argc == 4, then only 1 core's info was given,
		 * so, setbuf() is called once.
		 * If argc == 8 then core 0 and 1's info was given.
		 * So, setbuf() is called twice.
		 * If argc == 12 then core 0, 1 and 2's info was given.
		 * So, setbuf() is called thrice.
		 */
		if ((err = wlu_var_setbuf(wl, cmd->name,
		     deltaValues, 5*sizeof(int8))) < 0)
			return err;
		if (argc >= 8) {
			if ((err = wlu_var_setbuf(wl, cmd->name,
			     deltaValues + 5, 5*sizeof(int8))) < 0)
				return err;
		}
		if (argc == 12) {
			if ((err = wlu_var_setbuf(wl, cmd->name,
			     deltaValues + 10, 5*sizeof(int8))) < 0)
				return err;
		}

	}

	return 0;
}

static int
wl_phy_rssi_gain_delta_5g(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err = -1;
	int8 deltaValues[40];
	int32 value = 0;
	char *endptr;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;
	uint8 N = 0, n_per_core, n_per_core_p1;

	char *varname = "rssi_cal_rev";

	err = wlu_iovar_getint(wl, varname, &value);
	if ((err < 0) || ((err == 0) && (value == 0))) {
		/* This means, 'rssi_cal_rev' is not supported or Variable is 0 */
		/* Calls old function */
		n_per_core = 6;
	} else {
		n_per_core = 12;
	}

	n_per_core_p1 = n_per_core  + 1;
	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;
		if (phytype == WLC_PHY_TYPE_AC)
			N = n_per_core_p1 * 3; /* ACPHY: 3 cores max x 7 entries */
		for (i = 0; i < N; i++) {
			if ((phytype == WLC_PHY_TYPE_AC) && (i%n_per_core_p1 == 0)) {
				if (i > 0) printf("\n");
				if (deltaValues[i] == -1) break;
			}
			printf("%d ", deltaValues[i]);
		}
		if (i == N)
			printf("\n");
	} else {
		int argc = 0;
		int index = 0;
		while (argv[argc])
			argc++;

		if (phytype == WLC_PHY_TYPE_AC) {
		        /* N = 7; ACPHY : 7 entries for a core if n_per_core == 6 */
		        /* N = 13; ACPHY : 12 entries for a core if n_per_core == 12 */
			N = argc;
		}

		if ((n_per_core == 12) && !(N == 13 || N == 12 || N == 24 || N == 36)) {
		  printf("Incorrect number of arguments\n");
		  return 0;
		}

		for (i = 0; i < N; i++) {
			value = strtol(*argv++, &endptr, 0);
			if ((value > 63 || value < -64)) {
				return BCME_BADARG;
			}
			if (argc == n_per_core_p1) {
				/* For Old implementation, ie, no Routs, n_per_core_p1 == 5
				 * for New implementation, ie, no Routs, n_per_core_p1 == 9
				 * If number of arguments is "n_per_core_p1",
				 * then core number has been provided.
				 */
				deltaValues[i] = (int8)value;
			} else  {
				/* If the number of elements is not eq to
				 *"n_per_core", then, core number was not provided.
				 * So, if only "n_per_core" elements are provided,
				 * only core 0's info is given. So, for i = 0,
				 * deltaValues element is 0 (core_num). If "n_per_core * 2"
				 * elements are provided, then core 0 and 1's info is
				 * provided. So, i =0 element has core_num = 0, then,
				 * next "n_per_core" elements are core 0's
				 * deltas. For i = "n_per_core", core 1's
				 * core_num = 1 is inserted into deltaValues array.
				 * Similarly for third core data.
				 */

				if (i == (n_per_core * 0)) {
					deltaValues[index] = 0;
					index++;
				}
				if (i == (n_per_core * 1)) {
					deltaValues[index] = 1;
					index++;
				}
				if (i == (n_per_core * 2)) {
					deltaValues[index] = 2;
					index++;
				}

				deltaValues[index] = (int8)value;
				index++;
			}

		}
		/* If argc == "n_per_core", then only 1 core's infoxs
		 * was given, so, setbuf() is called once.
		 * If argc == "n_per_core * 2" then core 0 and 1's info
		 * was given. So, setbuf() is called twice.
		 * If argc == "n_per_core * 3" then core 0, 1 and 2's
		 * info was given. So, setbuf() is called thrice.
		 */
		if ((err = wlu_var_setbuf(wl, cmd->name, deltaValues,
		      n_per_core_p1*sizeof(int8))) < 0)
			return err;
		if (argc >= (n_per_core * 2)) {
			if ((err = wlu_var_setbuf(wl, cmd->name, deltaValues +
			     (n_per_core_p1 * 1), n_per_core_p1*sizeof(int8))) < 0)
				return err;
		}
		if (argc == (n_per_core * 3)) {
			if ((err = wlu_var_setbuf(wl, cmd->name, deltaValues +
			     (n_per_core_p1 * 2), n_per_core_p1*sizeof(int8))) < 0)
				return err;
		}

	}
	return 0;
}

static int
wl_phy_rxgainerr_2g(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err = -1;
	int8 deltaValues[18] = { 0 };
	int32 value;
	char *endptr;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;
	int coremask = 0;
	uint8 N = 0;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	if ((err = wlu_iovar_get(wl, "hw_rxchain", &coremask, sizeof(coremask))) < 0)
		return err;

	while (coremask > 0) {
		N += coremask & 1;
		coremask = coremask >> 1;
	}

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;

		for (i = 0; i < N; i++) {
			printf("%d ", deltaValues[i]);
		}
		if (i == N)
			printf("\n");
	} else {
		int argc = 0;
		while (argv[argc])
			argc++;

		if (argc != N) {
			printf("IOVAR works only for %d cores scenario.\n", N);
			return err;
		}

		for (i = 0; i < N; i++) {
			value = strtol(*argv++, &endptr, 0);
			if ((value > 63 || value < -64)) {
				return BCME_BADARG;
			}
			deltaValues[i] = (int8)value;
		}
		if ((err = wlu_var_setbuf(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;
	}

	return 0;
}

static int
wl_phy_rxgainerr_5g(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err = -1;
	int8 deltaValues[28] = { 0 };
	int32 value;
	char *endptr;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;
	int coremask = 0;
	uint8 N = 0;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	if ((err = wlu_iovar_get(wl, "hw_rxchain", &coremask, sizeof(coremask))) < 0)
		return err;

	while (coremask > 0) {
		N += coremask & 1;
		coremask = coremask >> 1;
	}

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;

		for (i = 0; i < N; i++) {
			printf("%d ", deltaValues[i]);
		}
		if (i == N)
			printf("\n");
	} else {
		int argc = 0;
		while (argv[argc])
			argc++;

		if (argc != N) {
			printf("IOVAR works only for %d cores scenario.\n", N);
			return err;
		}

		for (i = 0; i < N; i++) {
			value = strtol(*argv++, &endptr, 0);
			if ((value > 63 || value < -64)) {
				return BCME_BADARG;
			}
			deltaValues[i] = (int8)value;
		}
		if ((err = wlu_var_setbuf(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;
	}
	return 0;
}

static int
wl_phy_rxgainerr_6g(void *wl, cmd_t *cmd, char **argv)
{
	int i;
	int err = -1;
	int8 deltaValues[28] = { 0 };
	int32 value;
	char *endptr;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;
	int coremask = 0;
	uint8 N = 0;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	if ((err = wlu_iovar_get(wl, "hw_rxchain", &coremask, sizeof(coremask))) < 0)
		return err;

	while (coremask > 0) {
		N += coremask & 1;
		coremask = coremask >> 1;
	}

	if (!*++argv) {
		if ((err = wlu_iovar_get(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;

		printf("6G subband: %d\nGain errors [ant0 ant1 ...]:\n", deltaValues[0]);

		for (i = 0; i < N; i++) {
			printf("%d ", deltaValues[i+1]);
		}
		printf("\n");
	} else {
		int argc = 0;
		while (argv[argc])
			argc++;

		if (argc != (N+1)) {
			printf("Invalid args, enter: subband(0,1...6), ant0, ... ant%d\n", (N-1));
			return err;
		}

		for (i = 0; i < (N+1); i++) {
			value = strtol(*argv++, &endptr, 0);
			if ((value > 63 || value < -64)) {
				return BCME_BADARG;
			}
			deltaValues[i] = (int8)value;
		}
		if ((err = wlu_var_setbuf(wl, cmd->name, deltaValues, sizeof(deltaValues))) < 0)
			return err;
	}
	return 0;
}

static int
wl_phytable(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	uint32 tableInfo[5];
	char *endptr;
	void *ptr = NULL;
	int32 tableId, tableOffset, tableWidth;
	uint64 tableElement;

	if (*++argv != NULL)
		tableId = strtol(*argv, &endptr, 0);
	else
		return BCME_USAGE_ERROR;

	if (*++argv != NULL)
		tableOffset = strtol(*argv, &endptr, 0);
	else
		return BCME_USAGE_ERROR;

	if (*++argv != NULL)
		tableWidth = strtol(*argv, &endptr, 0);
	else
		return BCME_USAGE_ERROR;

	if ((tableId < 0) || (tableOffset < 0))
		return BCME_BADARG;

	if ((tableWidth != 8) && (tableWidth != 16) && (tableWidth != 32) &&
		(tableWidth != 48) && (tableWidth != 64))
		return BCME_BADARG;

	if (!*++argv) { /* wl utility reads a PHY table element */
		tableInfo[0] = tableId;
		tableInfo[1] = tableOffset;
		tableInfo[2] = tableWidth;

		if ((err = wlu_var_getbuf(wl, cmd->name, tableInfo, 4*sizeof(int32), &ptr)) < 0)
			return err;

		tableElement = ((uint64*)ptr)[0]; /* ptr is guaranteed to be 64 bits aligned */

		/* Mask out the correct data */
		if (tableWidth == 8)
			tableElement &= 0xFF;
		else if (tableWidth == 16)
			tableElement &= 0xFFFF;
		else if (tableWidth == 32)
			tableElement &= 0xFFFFFFFF;
		else if (tableWidth == 48)
			tableElement &= 0xFFFFFFFFFFFFULL;

		printf("0x%llx(%lld)\n", tableElement, tableElement);
	} else { /* wl utility writes a PHY table element */
		tableElement = bcm_strtoull(*argv++, &endptr, 0);

		tableInfo[0] = tableId;
		tableInfo[1] = tableOffset;
		tableInfo[2] = tableWidth;
		htol64_ua_store(tableElement, &tableInfo[3]);

		if ((err = wlu_var_setbuf(wl, cmd->name, tableInfo, 5 * sizeof(int32))) < 0)
			return err;
	}

	return 0;
}

static int
wl_phy_force_crsmin(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	int8 th[4] = { 0 };
	int32 value;
	uint argc = 0;
	char *endptr;
	uint8 i = 0;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc == 0) {
		/* No get for now */
		return err;
	} else {
		if (argc > 3) {
			printf("IOVAR works only for up to 3 cores. \n");
			return err;
		}
		for (i = 0; i < argc; i++) {
			value = strtol(argv[i + 1], &endptr, 0);
			if ((i == 0) && (value < -1)) {
				/* Offset values (2nd/3rd arguments) can be negative */
				return BCME_BADARG;
			}
			th[i] = (int8) value;
		}
		if ((err = wlu_var_setbuf(wl, cmd->name, th, 4*sizeof(int8))) < 0)
			return err;
	}

	return 0;
}

static int
wl_phy_txpwrindex(void *wl, cmd_t *cmd, char **argv)
{
	uint i;
	int ret;
	uint32 txpwridx[4] = { 0 };
	int8 idx[4] = { 0 };
	uint argc;
	char *endptr;

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	for (i = 0; i < 4; i++) {
		if (argc > i) {
			txpwridx[i] = strtol(argv[1 + i], &endptr, 0);
			if (*endptr != '\0') {
				printf("error\n");
				return BCME_USAGE_ERROR;
			}
		}
	}

	if (argc == 0) {
		if ((ret = wlu_iovar_getint(wl, cmd->name, (int*)&txpwridx[0])) < 0) {
			return (ret);
		}
		txpwridx[0] = dtoh32(txpwridx[0]);
		idx[0] = (int8)(txpwridx[0] & 0xff);
		idx[1] = (int8)((txpwridx[0] >> 8) & 0xff);
		idx[2] = (int8)((txpwridx[0] >> 16) & 0xff);
		idx[3] = (int8)((txpwridx[0] >> 24) & 0xff);
		printf("txpwrindex for core{0...3}: %d %d %d %d\n", idx[0], idx[1],
		       idx[2], idx[3]);
	} else {
		wlc_rev_info_t revinfo;
		uint32 phytype;

		memset(&revinfo, 0, sizeof(revinfo));
		if ((ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo))) < 0)
			return ret;

		phytype = dtoh32(revinfo.phytype);

		if (phytype == WLC_PHY_TYPE_HT) {
			if (argc != 3) {
				printf("HTPHY must specify 3 core txpwrindex\n");
				return BCME_USAGE_ERROR;
			}
		} else if (phytype == WLC_PHY_TYPE_N) {
			if (argc != 2) {
				printf("NPHY must specify 2 core txpwrindex\n");
				return BCME_USAGE_ERROR;
			}
		}

		ret = wlu_iovar_setbuf(wl, cmd->name, txpwridx, 4*sizeof(uint32),
			buf, WLC_IOCTL_MAXLEN);
	}

	return ret;
}

static int
wl_phy_force_vsdb_chans(void *wl, cmd_t *cmd, char **argv)
{
	uint16 *chans = NULL;
	int ret = 0;
	void	*ptr;

	UNUSED_PARAMETER(wl); UNUSED_PARAMETER(cmd);

	if (argv[1] == NULL) {
		if ((ret = wlu_var_getbuf(wl, "force_vsdb_chans", NULL, 0, &ptr) < 0)) {
				printf("wl_phy_maxpower: fail to get maxpower\n");
				return ret;
		}
		chans = (uint16*)ptr;
		printf("Chans : %x %x \n", chans[0], chans[1]);
	} else if (argv[1] && argv[2]) {
		/* Allocate memory */
		chans = (uint16*)CALLOC(2 * sizeof(uint16));
		if (chans == NULL) {
			printf("unable to allocate Memory \n");
			return BCME_NOMEM;
		}
		chans[0]  = wf_chspec_aton(argv[1]);
		chans[1]  = wf_chspec_aton(argv[2]);
		if (((chans[0] & 0xff) == 0) || ((chans[1] & 0xff) == 0)) {
			chans[0] = 0;
			chans[1] = 0;
		}
		ret = wlu_iovar_setbuf(wl, cmd->name, chans, 2 * sizeof(uint16),
			buf, WLC_IOCTL_MAXLEN);
		if (chans)
			free(chans);
	} else {
		ret = BCME_USAGE_ERROR;
	}

	return ret;
}

static int
wl_phy_pavars(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
		return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
		return 0;
#else
		const pavars_t *pav = pavars;
		uint16	inpa[WL_PHY_PAVARS_LEN];
		char	*cpar = NULL, *p = NULL;
		char	*par;
		char	delimit[2] = " \0";
		int err = 0;
		unsigned int val, val2[SROM_PAVAR];
		void	*ptr = NULL;
		int paparambwver = 0;
		int sromrev = 0;

		const char *iovar = "nvram_dump";
		void *p1 = NULL;

		if ((err = wlu_var_getbuf(wl, iovar, NULL, 0, &p1)) < 0) {
			if ((err = wlu_get(wl, WLC_NVRAM_DUMP, &buf[0], WLC_IOCTL_MAXLEN)) < 0)
				return err;
			p1 = (void *)buf;
		}
		if ((p1 = strstr(p1, "paparambwver"))) {
			char *q = NULL;
			p1 = (void*)((char*)p1 + 13);
			paparambwver = strtoul(p1, &q, 10);
		}

		err = wlu_iovar_getint(wl, "sromrev", &sromrev);
		if (paparambwver == 1)
			pav = pavars_bwver_1;
		else if (paparambwver == 2)
			pav = pavars_bwver_2;
		else if (paparambwver == 3)
			pav = pavars_bwver_3;
		else if (paparambwver == 4)
			pav = pavars_bwver_4;
		else {
		  if (sromrev > 12 && sromrev != 16 && sromrev != 18) {
			pav = pavars_SROM13;
		  }
		  if (sromrev == 18)
			pav = pavars_SROM18;
		  if (sromrev == 12)
			pav = pavars_SROM12;
		  if (sromrev < 12 || sromrev == 16 || sromrev == 17) {
			pav = pavars;
		  }
		}
		if (*++argv) {	/* set */
			while (pav->phy_type != PHY_TYPE_NULL) {
				bool found = FALSE;
				int i = 0;
				inpa[i++] = pav->phy_type;
				inpa[i++] = pav->bandrange;
				inpa[i++] = pav->chain;

				par = CALLOC(strlen(pav->vars)+1);
				if (!par)
					return BCME_NOMEM;

				strcpy(par, pav->vars);

				cpar = strtok (par, delimit);	/* current param */

				if ((pav->phy_type == PHY_TYPE_AC) ||
						(pav->phy_type == PHY_TYPE_LCN20)) {
				  int pnum = 0, n;
				  if (sromrev >= 12 && sromrev != 16 && sromrev != 17) {
					if ((pav->bandrange == WL_CHAN_FREQ_RANGE_2G) ||
					   (pav->bandrange == WL_CHAN_FREQ_RANGE_2G_40) ||
					   (pav->bandrange == WL_CHAN_FREQ_RANGE_2G_20_CCK)) {
							pnum = 4;
					} else if ((pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5G_5BAND) ||
						(pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5G_5BAND_40) ||
						(pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5G_5BAND_80))
							pnum = 20;
					else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_5BAND_160)
						pnum = 16;
					else if ((pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND) ||
						(pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND_40) ||
						(pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND_80))
							pnum = 8;
					else if ((pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND_160))
							pnum = 12;
				  }
				  if (sromrev < 12 || sromrev == 16) {
					if (pav->bandrange == WL_CHAN_FREQ_RANGE_2G)
					  pnum = 3;
					else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_4BAND)
					  pnum = 12;
				  }
				  /* defining 4 term PA-params with 4 sub-bands on 5G */
				  if (sromrev == 17) {
					if (pav->bandrange == WL_CHAN_FREQ_RANGE_2G)
					  pnum = 4;
					else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_4BAND)
					  pnum = 16;
				  }
				  if (cpar) {
					/* Find the parameter in the input argument list */
					if ((p = find_pattern2(argv, cpar, val2, pnum))) {
						found = TRUE;
						for (n = 0; n < pnum; n ++)
							inpa[i + n] = (uint16)val2[n];
					}
				  }
				} else {
					do {
						val = 0;
						if (cpar == NULL) {
							inpa[i] = val;
							break;
						}
						/* Find the parameter in the input argument list */
						if ((p = find_pattern(argv, cpar, &val))) {
							found = TRUE;
							inpa[i] = (uint16)val;
						} else
							inpa[i] = 0;
						i++;
					} while ((cpar = strtok (NULL, delimit)) != NULL);
				}
				free(par);

				if (found) {
					if ((err = wlu_var_setbuf(wl, cmd->name, inpa,
						WL_PHY_PAVARS_LEN * sizeof(uint16))) < 0) {
						printf("wl_phy_pavars: fail to set\n");
						return err;
					}
				}
				pav++;
			}
		} else {	/* get */
			while (pav->phy_type != PHY_TYPE_NULL) {
				int i = 0;
				uint16	*outpa;

				inpa[i++] = pav->phy_type;
				inpa[i++] = pav->bandrange;
				inpa[i++] = pav->chain;

				par = CALLOC(strlen(pav->vars)+1);
				if (!par)
					return BCME_NOMEM;
				strcpy(par, pav->vars);
				if ((err = wlu_var_getbuf_sm(wl, cmd->name, inpa,
					WL_PHY_PAVARS_LEN * sizeof(uint16), &ptr)) < 0) {
					printf("phy %x band %x chain %d err %d\n", pav->phy_type,
						pav->chain, pav->bandrange, err);
					free(par);
					break;
				}

				outpa = (uint16*)ptr;
				if (outpa[0] == PHY_TYPE_NULL) {
					pav++;
					free(par);
					continue;
				}

				cpar = strtok(par, delimit);	/* current param */

				if ((pav->phy_type == PHY_TYPE_AC) ||
						(pav->phy_type == PHY_TYPE_LCN20)) {
				  int pnum = 0, n;
				  if (sromrev >= 12 && sromrev != 16 && sromrev != 17) {
					if ((pav->bandrange == WL_CHAN_FREQ_RANGE_2G) ||
						(pav->bandrange == WL_CHAN_FREQ_RANGE_2G_40) ||
						(pav->bandrange == WL_CHAN_FREQ_RANGE_2G_20_CCK)) {
							pnum = 4;
					} else if ((pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5G_5BAND) ||
						(pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5G_5BAND_40) ||
						(pav->bandrange == WL_CHAN_FREQ_RANGE_5G_5BAND_80))
							pnum = 20;
					else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_5BAND_160)
						pnum = 16;
					else if ((pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND) ||
						(pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND_40) ||
						(pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND_80))
							pnum = 8;
					else if ((pav->bandrange ==
						WL_CHAN_FREQ_RANGE_5GEXT_5BAND_160))
							pnum = 12;
				  }
				  if (sromrev < 12 || sromrev == 16) {
					if (pav->bandrange == WL_CHAN_FREQ_RANGE_2G)
					  pnum = 3;
					else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_4BAND)
					  pnum = 12;
				  }
				  /* defining 4 term PA-params with 4 sub-bands on 5G */
				  if (sromrev == 17) {
					if (pav->bandrange == WL_CHAN_FREQ_RANGE_2G)
					  pnum = 4;
					else if (pav->bandrange == WL_CHAN_FREQ_RANGE_5G_4BAND)
					  pnum = 16;
				  }
				  printf("%s=", cpar);
				  for (n = 0; n < pnum; n ++) {
					if (n != 0)
					  printf(",");
					printf("0x%x", outpa[i + n]);
				  }
				  printf("\n");
				} else {
					do {
						printf("%s=0x%x\n", cpar, outpa[i++]);
					} while ((cpar = strtok (NULL, delimit)) != NULL);
				}
				pav++;
				free(par);
			}
		}
		return err;
#endif /* _CFE_ */

}

static int
wl_phy_povars(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	const povars_t *pov = povars;
	wl_po_t	inpo;
	char	*cpar = NULL, *p = NULL;
	char	*par;	/* holds longest povars->vars */
	char	delimit[2] = " \0";
	int	err = 0;
	uint val;
	void	*ptr = NULL;

	if (*++argv) {	/* set */
		while (pov->phy_type != PHY_TYPE_NULL) {
			bool found = FALSE;
			int i = 0;

			inpo.phy_type = pov->phy_type;
			inpo.band = pov->bandrange;

			par = CALLOC(strlen(pov->vars)+1);
			if (!par)
				return BCME_NOMEM;

			strcpy(par, pov->vars);

			/* Take care of cck and ofdm before walking through povars->vars */
			if (pov->bandrange == WL_CHAN_FREQ_RANGE_2G) {
				p = find_pattern(argv, "cck2gpo", &val);
				if (p)	found = TRUE;
				inpo.cckpo = p ? (uint16)val : 0;

				p = find_pattern(argv, "ofdm2gpo", &val);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GL) {
				p = find_pattern(argv, "ofdm5glpo", &val);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GM) {
				p = find_pattern(argv, "ofdm5gpo", &val);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GH) {
				p = find_pattern(argv, "ofdm5ghpo", &val);
			}
			inpo.ofdmpo = p ? (uint32)val : 0;
			if (p)	found = TRUE;

			cpar = strtok (par, delimit);	/* current param */
			do {
				val = 0;
				if (cpar == NULL) {
					inpo.mcspo[i] = val;
					break;
				}

				/* Find the parameter in the input argument list */
				p = find_pattern(argv, cpar, &val);
				if (p)	found = TRUE;
				inpo.mcspo[i] = p ? (uint16)val : 0;
				i++;
			} while ((cpar = strtok (NULL, delimit)) != NULL);

			if (found) {
				if ((err = wlu_var_setbuf(wl, cmd->name, &inpo,
					sizeof(wl_po_t))) < 0) {
					printf("wl_phy_povars: fail to set\n");
					free(par);
					return err;
				}
			}
			pov++;
			free(par);
		}
	} else {	/* get */
		while (pov->phy_type != PHY_TYPE_NULL) {
			int i = 0;
			wl_po_t	*outpo;

			inpo.phy_type = pov->phy_type;
			inpo.band = pov->bandrange;

			par = CALLOC(strlen(pov->vars)+1);
			if (!par)
				return BCME_NOMEM;

			strcpy(par, pov->vars);

			if ((err = wlu_var_getbuf(wl, cmd->name, &inpo, sizeof(povars_t),
				&ptr)) < 0) {
				printf("phy %x band %x err %d\n", pov->phy_type,
					pov->bandrange, err);
				free(par);
				break;
			}

			outpo = (wl_po_t*)ptr;
			if (outpo->phy_type == PHY_TYPE_NULL) {
				pov++;
				free(par);
				continue;
			}

			/* Take care of cck and ofdm before walking through povars->vars */
			if (outpo->band == WL_CHAN_FREQ_RANGE_2G) {
				printf("cck2gpo=0x%x\n", outpo->cckpo);
				printf("ofdm2gpo=0x%x\n", outpo->ofdmpo);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GL) {
				printf("ofdm5glpo=0x%x\n", outpo->ofdmpo);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GM) {
				printf("ofdm5gpo=0x%x\n", outpo->ofdmpo);
			} else if (pov->bandrange == WL_CHAN_FREQ_RANGE_5GH) {
				printf("ofdm5ghpo=0x%x\n", outpo->ofdmpo);
			}

			cpar = strtok(par, delimit);	/* current param */
			do {
				printf("%s=0x%x\n", cpar, outpo->mcspo[i++]);
			} while ((cpar = strtok (NULL, delimit)));

			pov++;
			free(par);
		}
	}

	return err;
#endif /* _CFE_ */
}

static int
wl_phy_rpcalvars(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	int    err = 0, k;
	unsigned int val;
	wl_rpcal_t rpcal[2*WL_NUM_RPCALVARS], *rpcal_out;
	void *ptr = NULL;

	if (*++argv) {	/* set */
		bool found = FALSE;

		/* initialization */
		memset(&(rpcal[0]), 0, sizeof(wl_rpcal_t)*2*WL_NUM_RPCALVARS);

		if (find_pattern(argv, "rpcal2g", &val)) {
			found = TRUE;
			rpcal[WL_CHAN_FREQ_RANGE_2G].value  = (uint16) val;
			rpcal[WL_CHAN_FREQ_RANGE_2G].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb0", &val)) {
			found = TRUE;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND0].value  = (uint16) val;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND0].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb1", &val)) {
			found = TRUE;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND1].value  = (uint16) val;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND1].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb2", &val)) {
			found = TRUE;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND2].value  = (uint16) val;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND2].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb3", &val)) {
			found = TRUE;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND3].value  = (uint16) val;
			rpcal[WL_CHAN_FREQ_RANGE_5G_BAND3].update = 1;
		}

		if (find_pattern(argv, "rpcal2gcore3", &val)) {
			found = TRUE;
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_2G].value =
			(uint16) (val & 0xff);
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_2G].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb0core3", &val)) {
			found = TRUE;
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND0].value =
			(uint16) (val & 0xff);
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND0].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb1core3", &val)) {
			found = TRUE;
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND1].value =
			(uint16) (val & 0xff);
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND1].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb2core3", &val)) {
			found = TRUE;
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND2].value =
			(uint16) (val & 0xff);
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND2].update = 1;
		}

		if (find_pattern(argv, "rpcal5gb3core3", &val)) {
			found = TRUE;
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND3].value =
			(uint16) (val & 0xff);
			rpcal[WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND3].update = 1;
		}

		if (found) {
			err = wlu_var_setbuf(wl, cmd->name, &(rpcal[0]),
			                     sizeof(wl_rpcal_t)*2*WL_NUM_RPCALVARS);
			if (err < 0) {
				printf("wl_phy_rpcalvars: fail to set\n");
				return err;
			}
		} else {
			printf("wl_phy_rpcalvars: fail to found matching rpcalvar name\n");
			return err;
		}

	} else {	/* get */

		err = wlu_var_getbuf(wl, cmd->name, &(rpcal[0]),
		                     sizeof(wl_rpcal_t)*2*WL_NUM_RPCALVARS, &ptr);

		if (err < 0) {
			printf("wl_phy_rpcalvars: fail to get\n");
			return err;
		} else {
			rpcal_out = (wl_rpcal_t*) ptr;
		}

		for (k = 0; k < 2*WL_NUM_RPCALVARS; k++) {

			switch (k) {
			case WL_CHAN_FREQ_RANGE_2G:
				printf("rpcal2g=0x%x ", rpcal_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND0:
				printf("rpcal5gb0=0x%x ", rpcal_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND1:
				printf("rpcal5gb1=0x%x ", rpcal_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND2:
				printf("rpcal5gb2=0x%x ", rpcal_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND3:
				printf("rpcal5gb3=0x%x\n", rpcal_out[k].value);
				break;
			case WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_2G:
				printf("rpcal2gcore3=0x%x ", rpcal_out[k].value);
				break;
			case WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND0:
				printf("rpcal5gb0core3=0x%x ", rpcal_out[k].value);
				break;
			case WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND1:
				printf("rpcal5gb1core3=0x%x ", rpcal_out[k].value);
				break;
			case WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND2:
				printf("rpcal5gb2core3=0x%x ", rpcal_out[k].value);
				break;
			case WL_NUM_RPCALVARS+WL_CHAN_FREQ_RANGE_5G_BAND3:
				printf("rpcal5gb3core3=0x%x\n", rpcal_out[k].value);
				break;
			}
		}
	}

	return 0;
#endif /* _CFE_ */
}

static int
wl_phy_rpcalphasevars(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	int    err = 0, k;
	unsigned int val;
	wl_rpcal_phase_t rpcal_phase[WL_NUM_RPCALPHASEVARS], *rpcal_phase_out;
	void *ptr = NULL;

	if (*++argv) {	/* set */
		bool found = FALSE;

		/* initialization */
		memset(&(rpcal_phase[0]), 0, sizeof(wl_rpcal_phase_t)*WL_NUM_RPCALPHASEVARS);

		if (find_pattern(argv, "rpcal_phase2g", &val)) {
			found = TRUE;
			rpcal_phase[WL_CHAN_FREQ_RANGE_2G].value  = (uint16) val;
			rpcal_phase[WL_CHAN_FREQ_RANGE_2G].update = 1;
		}

		if (find_pattern(argv, "rpcal_phase5gb0", &val)) {
			found = TRUE;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND0].value  = (uint16) val;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND0].update = 1;
		}

		if (find_pattern(argv, "rpcal_phase5gb1", &val)) {
			found = TRUE;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND1].value  = (uint16) val;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND1].update = 1;
		}

		if (find_pattern(argv, "rpcal_phase5gb2", &val)) {
			found = TRUE;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND2].value  = (uint16) val;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND2].update = 1;
		}

		if (find_pattern(argv, "rpcal_phase5gb3", &val)) {
			found = TRUE;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND3].value  = (uint16) val;
			rpcal_phase[WL_CHAN_FREQ_RANGE_5G_BAND3].update = 1;
		}

		if (found) {
			err = wlu_var_setbuf(wl, cmd->name, &(rpcal_phase[0]),
			                     sizeof(wl_rpcal_phase_t)*WL_NUM_RPCALPHASEVARS);
			if (err < 0) {
				printf("wl_phy_rpcalphasevars: fail to set\n");
				return err;
			}
		} else {
			printf("wl_phy_rpcalphasevars: fail to found matching rpcalphase name\n");
			return err;
		}

	} else {	/* get */

		err = wlu_var_getbuf(wl, cmd->name, &(rpcal_phase[0]),
		                     sizeof(wl_rpcal_phase_t)*WL_NUM_RPCALPHASEVARS, &ptr);

		if (err < 0) {
			printf("wl_phy_rpcalphasevars: fail to get\n");
			return err;
		} else {
			rpcal_phase_out = (wl_rpcal_phase_t*) ptr;
		}

		for (k = 0; k < WL_NUM_RPCALPHASEVARS; k++) {

			switch (k) {
			case WL_CHAN_FREQ_RANGE_2G:
				printf("rpcal_phase2g=0x%x ", rpcal_phase_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND0:
				printf("rpcal_phase5gb0=0x%x ", rpcal_phase_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND1:
				printf("rpcal_phase5gb1=0x%x ", rpcal_phase_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND2:
				printf("rpcal_phase5gb2=0x%x ", rpcal_phase_out[k].value);
				break;
			case WL_CHAN_FREQ_RANGE_5G_BAND3:
				printf("rpcal_phase5gb3=0x%x\n", rpcal_phase_out[k].value);
				break;
			}
		}
	}

	return 0;
#endif /* _CFE_ */
}

static int
wl_phy_align_sniffer(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	int err = 0;
	void *ptr = NULL;
	struct ether_addr ea;
	struct ether_addr *eapp;

	if (!*++argv) { /* get */
		err = wlu_var_getbuf(wl, cmd->name, &ea,
		                     sizeof(struct ether_addr), &ptr);
		if (err < 0) {
			printf("wl_phy_align_sniffer: fail to get\n");
			return err;
		} else {
			eapp = (struct ether_addr *)ptr;
			ea  = (struct ether_addr)(*eapp);
		}
		printf("%s\n", wl_ether_etoa(&ea));
	}
	else if (wl_ether_atoe(*argv, &ea)) { /* set */
		err = wlu_var_setbuf(wl, cmd->name, &ea,
		                     sizeof(struct ether_addr));
		if (err < 0) {
			printf("wl_phy_align_sniffer: fail to set\n");
			return err;
		}
	}
	else {
		fprintf(stderr, " ERROR: align: no valid ether addr provided\n");
		return BCME_USAGE_ERROR;
	}

	return ret;
}

static int
wl_phy_fem(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	srom_fem_t	fem;
	srom_fem_t	*rfem;
	void		*ptr;
	bool	found = FALSE;
	int	err = 0;
	uint	val;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {	/* write fem */

		/* fem2g */
		memset(&fem, 0, sizeof(srom_fem_t));

		if (find_pattern(argv, "tssipos2g", &val)) {
			found = TRUE;
			fem.tssipos = val;
		}

		if (find_pattern(argv, "extpagain2g", &val)) {
			found = TRUE;
			fem.extpagain = val;
		}

		if (find_pattern(argv, "pdetrange2g", &val)) {
			found = TRUE;
			fem.pdetrange = val;
		}

		if (find_pattern(argv, "triso2g", &val)) {
			found = TRUE;
			fem.triso = val;
		}

		if (find_pattern(argv, "antswctl2g", &val)) {
			found = TRUE;
			fem.antswctrllut = val;
		}

		if (found) {
			if ((err = wlu_var_setbuf(wl, "fem2g", &fem, sizeof(srom_fem_t)) < 0))
				printf("wl_phy_fem: fail to set fem2g\n");
			else
				printf("fem2g set\n");
		}

		found = FALSE;
		/* fem5g */
		memset(&fem, 0, sizeof(srom_fem_t));

		if (find_pattern(argv, "tssipos5g", &val)) {
			found = TRUE;
			fem.tssipos = val;
		}

		if (find_pattern(argv, "extpagain5g", &val)) {
			found = TRUE;
			fem.extpagain = val;
		}

		if (find_pattern(argv, "pdetrange5g", &val)) {
			found = TRUE;
			fem.pdetrange = val;
		}

		if (find_pattern(argv, "triso5g", &val)) {
			found = TRUE;
			fem.triso = val;
		}

		if (find_pattern(argv, "antswctl5g", &val)) {
			found = TRUE;
			fem.antswctrllut = val;
		}

		if (found) {
			if ((err = wlu_var_setbuf(wl, "fem5g", &fem, sizeof(srom_fem_t)) < 0))
				printf("wl_phy_fem: fail to set fem5g\n");
			else
				printf("fem5g set\n");
		}
	} else {
		if ((err = wlu_var_getbuf(wl, "fem2g", NULL, 0, (void**)&ptr) < 0)) {
			printf("wl_phy_fem: fail to get fem2g\n");
		} else {
			rfem = (srom_fem_t*)ptr; /* skip the "fem2g" */
			printf("tssipos2g=0x%x extpagain2g=0x%x pdetrange2g=0x%x"
			       " triso2g=0x%x antswctl2g=0x%x\n",
			       rfem->tssipos, rfem->extpagain, rfem->pdetrange,
			       rfem->triso, rfem->antswctrllut);
	       }

		if ((err = wlu_var_getbuf(wl, "fem5g", NULL, 0, (void**)&ptr) < 0)) {
			printf("wl_phy_fem: fail to get fem5g\n");
		} else {
			rfem = (srom_fem_t*)ptr; /* skip the "fem2g" */
			printf("tssipos5g=0x%x extpagain5g=0x%x pdetrange5g=0x%x"
			       " triso5g=0x%x antswctl5g=0x%x\n",
			       rfem->tssipos, rfem->extpagain, rfem->pdetrange,
			       rfem->triso, rfem->antswctrllut);
		}
	}

	return err;
#endif /* _CFE_ */
}

static int
wl_phy_maxpower(void *wl, cmd_t *cmd, char **argv)
{
#if	defined(_CFE_)
	return CFE_ERR_UNSUPPORTED;
#elif	defined(DONGLEBUILD)
	return 0;
#else
	int	err = 0;
	uint	val;
	uint8	maxp[8];
	void	*ptr;
	uint8	*rmaxp;

	UNUSED_PARAMETER(cmd);

	if (*++argv) {	/* write maxpower */

		if (find_pattern(argv, "maxp2ga0", &val))
			maxp[0] = val;
		else
			printf("Missing maxp2ga0\n");

		if (find_pattern(argv, "maxp2ga1", &val))
			maxp[1] = val;
		else
			printf("Missing maxp2ga1\n");

		if (find_pattern(argv, "maxp5ga0", &val))
			maxp[2] = val;
		else
			printf("Missing maxp5ga0\n");

		if (find_pattern(argv, "maxp5ga1", &val))
			maxp[3] = val;
		else
			printf("Missing maxp5ga1\n");

		if (find_pattern(argv, "maxp5gla0", &val))
			maxp[4] = val;
		else
			printf("Missing maxp5gla0\n");

		if (find_pattern(argv, "maxp5gla1", &val))
			maxp[5] = val;
		else
			printf("Missing maxp5gla1\n");

		if (find_pattern(argv, "maxp5gha0", &val))
			maxp[6] = val;
		else
			printf("Missing maxp5gha0\n");

		if (find_pattern(argv, "maxp5gha1", &val))
			maxp[7] = val;
		else
			printf("Missing maxp5gha1\n");

		if ((err = wlu_var_setbuf(wl, "maxpower", &maxp, 8 * sizeof(uint8)) < 0)) {
			printf("wl_phy_maxpower: fail to set\n");
		}
	} else {
		if ((err = wlu_var_getbuf(wl, "maxpower", NULL, 0, &ptr) < 0)) {
			printf("wl_phy_maxpower: fail to get maxpower\n");
			return err;
		}
		rmaxp = (uint8*)ptr;
		printf("maxp2ga0=%x\n", rmaxp[0]);
		printf("maxp2ga1=%x\n", rmaxp[1]);
		printf("maxp5ga0=%x\n", rmaxp[2]);
		printf("maxp5ga1=%x\n", rmaxp[3]);
		printf("maxp5gla0=%x\n", rmaxp[4]);
		printf("maxp5gla1=%x\n", rmaxp[5]);
		printf("maxp5gha0=%x\n", rmaxp[6]);
		printf("maxp5gha1=%x\n", rmaxp[7]);
	}

	return err;
#endif /* _CFE_ */
}

static int
wl_pkteng(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkteng_t pkteng;

	memset(&pkteng, 0, sizeof(pkteng));
	if (strcmp(cmd->name, "pkteng_stop") == 0) {
		if (!*++argv)
			return BCME_USAGE_ERROR;
		if (strcmp(*argv, "tx") == 0)
			pkteng.flags = WL_PKTENG_PER_TX_STOP;
		else if (strcmp(*argv, "rx") == 0)
			pkteng.flags = WL_PKTENG_PER_RX_STOP;
		else
			return BCME_USAGE_ERROR;
	}
	else if (strcmp(cmd->name, "pkteng_start") == 0) {
		if (!*++argv)
			return BCME_USAGE_ERROR;
		if (!wl_ether_atoe(*argv, (struct ether_addr *)&pkteng.dest))
			return BCME_USAGE_ERROR;
		if (!*++argv)
			return BCME_USAGE_ERROR;
		if ((strcmp(*argv, "tx") == 0) || (strcmp(*argv, "txwithack") == 0) ||
			(strcmp(*argv, "tgrtx") == 0) || (strcmp(*argv, "frmtrgtx") == 0))  {
			if (strcmp(*argv, "tx") == 0)
				pkteng.flags = WL_PKTENG_PER_TX_START;
			else if (strcmp(*argv, "txwithack") == 0)
				pkteng.flags = WL_PKTENG_PER_TX_WITH_ACK_START;
			else if (strcmp(*argv, "tgrtx") == 0)
				pkteng.flags = WL_PKTENG_PER_TX_HETB_START;
			else
				pkteng.flags = WL_PKTENG_PER_TX_HETB_WITH_TRG_START;
			if (!*++argv)
				return BCME_USAGE_ERROR;
			if (strcmp(*argv, "async") == 0)
				pkteng.flags &= ~(WL_PKTENG_SYNCHRONOUS |
						WL_PKTENG_SYNCHRONOUS_UNBLK);
			else if (strcmp(*argv, "sync") == 0)
				pkteng.flags |= WL_PKTENG_SYNCHRONOUS;
			else if (!strncmp(*argv, "sync_unblk", strlen("sync_unblk"))) {
				pkteng.flags |= WL_PKTENG_SYNCHRONOUS_UNBLK;
			}
			else {
				/* neither optional parameter [async|sync|sync_unblk] */
				--argv;
			}
			if (!*++argv)
				return BCME_USAGE_ERROR;
			pkteng.delay = strtoul(*argv, NULL, 0);
			if (!*++argv)
				return BCME_USAGE_ERROR;
			pkteng.length = strtoul(*argv, NULL, 0);
			if (!*++argv)
				return BCME_USAGE_ERROR;
			pkteng.nframes = strtoul(*argv, NULL, 0);
			if (*++argv)
				if (!wl_ether_atoe(*argv, (struct ether_addr *)&pkteng.src))
					return BCME_USAGE_ERROR;
		}
		else if ((strcmp(*argv, "rx") == 0) || (strcmp(*argv, "rxwithack") == 0)) {
			if ((strcmp(*argv, "rx") == 0))
				pkteng.flags = WL_PKTENG_PER_RX_START;
			else
				pkteng.flags = WL_PKTENG_PER_RX_WITH_ACK_START;

			if (*++argv) {
				if (strcmp(*argv, "async") == 0)
					pkteng.flags &= ~WL_PKTENG_SYNCHRONOUS;
				else if (strcmp(*argv, "sync") == 0) {
					pkteng.flags |= WL_PKTENG_SYNCHRONOUS;
					/* sync mode requires number of frames and timeout */
					if (!*++argv)
						return BCME_USAGE_ERROR;
					pkteng.nframes = strtoul(*argv, NULL, 0);
					if (!*++argv)
						return BCME_USAGE_ERROR;
					pkteng.delay = strtoul(*argv, NULL, 0);
				}
			}
		}
		else
			return BCME_USAGE_ERROR;
	}
	else {
		printf("Invalid command name %s\n", cmd->name);
		return 0;
	}

	pkteng.flags = htod32(pkteng.flags);
	pkteng.delay = htod32(pkteng.delay);
	pkteng.nframes = htod32(pkteng.nframes);
	pkteng.length = htod32(pkteng.length);

	return (wlu_var_setbuf(wl, "pkteng", &pkteng, sizeof(pkteng)));
}

static int
wl_get_trig_info(void *wl, cmd_t *cmd, char **argv)
{
	uint8 argc;
	char var[256];
	wl_trig_frame_info_t *ptr;
	void *temp = NULL;

	for (argc = 0; argv[argc]; argc++);
	/* Get Functionality */
	if (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &temp) < 0)
		return -1;

	ptr = (wl_trig_frame_info_t *)temp;
	printf("11ax Trigger Frame Parsed info\n\n");
	printf("Interface Version: %d\n", ptr->version);
	printf("Common Info:\n\n");
	printf("trigger_type: 0x%x\n", ptr->trigger_type);
	printf("Lsig length: 0x%x\n", ptr->lsig_len);
	printf("cascade_indication: 0x%x\n", ptr->cascade_indication);
	printf("cs_req: 0x%x\n", ptr->cs_req);
	printf("bw: 0x%x\n", ptr->bw);
	printf("cp_ltf_type: 0x%x\n", ptr->cp_ltf_type);
	printf("mu_mimo_ltf_mode: 0x%x\n", ptr->mu_mimo_ltf_mode);
	printf("num_he_ltf_syms: 0x%x\n", ptr->num_he_ltf_syms);
	printf("stbc: 0x%x\n", ptr->stbc);
	printf("ldpc_extra_symb: 0x%x\n", ptr->ldpc_extra_symb);
	printf("ap_tx_pwr: 0x%x\n", ptr->ap_tx_pwr);
	printf("afactor: 0x%x\n", ptr->afactor);
	printf("pe_disambiguity: 0x%x\n", ptr->pe_disambiguity);
	printf("spatial_resuse: 0x%x\n", ptr->spatial_resuse);
	printf("doppler: 0x%x\n", ptr->doppler);
	printf("he_siga_rsvd: 0x%x\n", ptr->he_siga_rsvd);
	printf("\nUser Info:\n\n");
	printf("aid12: 0x%x\n", ptr->aid12);
	printf("ru_alloc: 0x%x\n", ptr->ru_alloc);
	printf("coding_type: 0x%x\n", ptr->coding_type);
	printf("mcs: 0x%x\n", ptr->mcs);
	printf("dcm: 0x%x\n", ptr->dcm);
	printf("ss_alloc: 0x%x\n", ptr->ss_alloc);
	printf("tgt_rssi: 0x%x\n", ptr->tgt_rssi);
	printf("usr_info_rsvd: 0x%x\n", ptr->usr_info_rsvd);

	return BCME_OK;
}

static int
wl_pkteng_trig_fill(void *wl, cmd_t *cmd, char **argv)
{
	wl_pkteng_ru_fill_t pkteng_ru_fill;
	uint8 argc;
	uint8 max_cmdargs = 27;

	memset(&pkteng_ru_fill, 0, sizeof(pkteng_ru_fill));
	/* check for number of parameters ( 24 arguments + cmd name) */
	for (argc = 0; argv[argc]; argc++);
	if (argc != max_cmdargs)
		return BCME_USAGE_ERROR;
	/* fill the version and length fields */
	pkteng_ru_fill.version = WL_PKTENG_RU_FILL_VER_1;
	pkteng_ru_fill.length = sizeof(pkteng_ru_fill);
	pkteng_ru_fill.length = htod32(pkteng_ru_fill.length);
	/* default values */
	pkteng_ru_fill.bw = 0;
	pkteng_ru_fill.coding_val = 0;
	pkteng_ru_fill.cp_ltf_val = 0;
	pkteng_ru_fill.dcm = 0;
	pkteng_ru_fill.he_ltf_symb = 1;
	pkteng_ru_fill.stbc = 0;
	pkteng_ru_fill.mcs_val = 0;
	pkteng_ru_fill.mumimo_ltfmode = 0;
	pkteng_ru_fill.nss_val = 1;
	pkteng_ru_fill.num_bytes = 100;
	pkteng_ru_fill.pe_category = 2;
	pkteng_ru_fill.ru_alloc_val = 0;
	pkteng_ru_fill.tgt_rssi = 127;
	/* skip command name */
	argv++;
	if (strcmp(cmd->name, "load_trig_info") == 0) {
		while (*argv) {
			char *s = *argv;
			if (argv[1] == NULL)
				return BCME_USAGE_ERROR;
			if (!strcmp(s, "-b") || !strcmp(s, "--bandwidth")) {
				if (!strcmp(argv[1], "20"))
					pkteng_ru_fill.bw = 0;
				else if (!strcmp(argv[1], "40"))
					pkteng_ru_fill.bw = 1;
				else if (!strcmp(argv[1], "80"))
					pkteng_ru_fill.bw = 2;
				else if (!strcmp(argv[1], "160"))
					pkteng_ru_fill.bw = 3;
				else
					return BCME_USAGE_ERROR;
			}
			else if (!strcmp(s, "-u") || !strcmp(s, "--rualloc"))
				pkteng_ru_fill.ru_alloc_val = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-e") || !strcmp(s, "--he"))
				pkteng_ru_fill.mcs_val = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-s") || !strcmp(s, "--ss"))
				pkteng_ru_fill.nss_val = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-len") || !strcmp(s, "--length")) {
				pkteng_ru_fill.num_bytes = strtoul(argv[1], NULL, 0);
				pkteng_ru_fill.num_bytes = htod32(pkteng_ru_fill.num_bytes);
			}
			else if (!strcmp(s, "-g") || !strcmp(s, "--cp_ltf"))
				pkteng_ru_fill.cp_ltf_val = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-ls") || !strcmp(s, "--ltf_sym"))
				pkteng_ru_fill.he_ltf_symb = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-st") || !strcmp(s, "--stbc"))
				pkteng_ru_fill.stbc = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-l") || !strcmp(s, "--ldpc"))
				pkteng_ru_fill.coding_val = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-p") || !strcmp(s, "--pe_category"))
				pkteng_ru_fill.pe_category = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-dcm"))
				pkteng_ru_fill.dcm = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-tgtrssi"))
				pkteng_ru_fill.tgt_rssi = (uint8)strtoul(argv[1], NULL, 0);
			else if (!strcmp(s, "-lfm"))
				pkteng_ru_fill.mumimo_ltfmode = (uint8)strtoul(argv[1], NULL, 0);
			else
				return BCME_USAGE_ERROR;
			argv += 2;
			}
	} else {
		printf("Invalid command name %s\n", cmd->name);
		return 0;
	}
	return (wlu_var_setbuf(wl, "pkteng_ru_fill", &pkteng_ru_fill, sizeof(pkteng_ru_fill)));
}

static int
wl_pkteng_cmd(void *wl, cmd_t *cmd, char **argv)
{
	int ret, i, argc;
	int tmp1, tmp2;
	char *retbuf, *splt, *endptr;
	char **tmp_argv;
	uint pkteng_cmd_param_size;
	wl_pkteng_cmd_params_t *pecfg_params;
	bool has_key;
	bool is_get;
	struct ether_addr ea;

	BCM_REFERENCE(cmd);
	BCM_REFERENCE(wl);

	retbuf = NULL;
	pecfg_params = NULL;
	ret = BCME_OK;
	pkteng_cmd_param_size = sizeof(wl_pkteng_cmd_params_t);

	argc = 0;
	tmp_argv = argv;
	while (*tmp_argv++) argc++;
	if (argc > 12) {
		fprintf(stderr, "Error: too many arguments argc: %d \n", argc);
		ret = BCME_BADARG;
		goto exit;
	}

	/* reset argc */
	argc = 0;

	if ((pecfg_params = CALLOC(pkteng_cmd_param_size)) == NULL) {
		fprintf(stderr, "Error allocating %d bytes for pkteng_cmd params\n",
		pkteng_cmd_param_size);
		ret = BCME_NOMEM;
		goto exit;
	}

	/* skip the command name itself */
	argv++;

	/* check if there is verbose setting */
	tmp_argv = argv;
	while (*tmp_argv) {
		if (!strncmp(*tmp_argv, "-v", strlen("-v"))) {
			pecfg_params->flags |= PKTENGCMD_FLAGS_VERBOSE;
			if (tmp_argv == argv) {
				/* eat -v */
				argv++;
			}
			break;
		}
		tmp_argv++;
	}

	/* no argument is given, treat it as get */
	if (*argv == NULL) {
		pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGKEY_GET;

	/* parse if giving -- settings */
	} else if (!strncmp(*argv, "-", strlen("-"))) {
		/* command is using - style */

		pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGKEY_GET;
		tmp_argv = argv - 1;
		has_key = FALSE;

		while (*++tmp_argv) {
			if (!strncmp(*tmp_argv, "--key", strlen("--key")) ||
				!strncmp(*tmp_argv, "-k", strlen("-k"))) {
				has_key = TRUE;
				if ((ret = wl_pkteng_cmd_get_key_val(pecfg_params,
					++tmp_argv, TRUE)) != BCME_OK) {
					goto exit;
				}
				break;
			}
		}

		if (has_key) {
			pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGKEY_CFG;
			pecfg_params->u.argkeyval.val[0] = -1;
			pecfg_params->u.argkeyval.val[1] = -1;
			while (*argv) {
				if (!strncmp(*argv, "--sta", strlen("--sta")) ||
					!strncmp(*argv, "-u", strlen("-u")) ||
					!strncmp(*argv, "-usr", strlen("-usr")) ||
					!strncmp(*argv, "-p1", strlen("-p1"))) {
					argv++;
					if (*argv) {
						pecfg_params->u.argkeyval.val[0] =
							strtol(*argv++, NULL, 0);
					}
				} else if (!strncmp(*argv, "--queue", strlen("--queue")) ||
					!strncmp(*argv, "-q", strlen("-q")) ||
					!strncmp(*argv, "-p2", strlen("-p2"))) {
					argv++;
					if (*argv) {
						pecfg_params->u.argkeyval.val[1] =
							strtol(*argv++, NULL, 0);
					}
				} else {
					argv++;
				}
			}
		} else {
			/* handle pkteng dump print command */
			if (!strncmp(*argv, "-p", strlen("-p"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGKEY_GET;
			/* handle stop pkteng command */
			} else if (!strncmp(*argv, "--stop", strlen("--stop")) ||
				!strncmp(*argv, "-e", strlen("-e"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_STOP;
			} else if (!strncmp(*argv, "--pause", strlen("--pause"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_PAUSE;
			/* handle start pkteng command */
			} else if (!strncmp(*argv, "--startdl", strlen("--startdl")) ||
				!strncmp(*argv, "--dl", strlen("--dl"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_START;
				pecfg_params->u.argint.argv[argc++] = WL_PKTENG_CMD_STRT_DL_CMD;
			} else if (!strncmp(*argv, "--startultx", strlen("--startultx")) ||
				!strncmp(*argv, "--ssr", strlen("--ssr"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_START;
				pecfg_params->u.argint.argv[argc++] = WL_PKTENG_CMD_STRT_ULTX_CMD;
			} else if (!strncmp(*argv, "--startul", strlen("--startul")) ||
				!strncmp(*argv, "--ul", strlen("--ul"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_START;
				pecfg_params->u.argint.argv[argc++] = WL_PKTENG_CMD_STRT_UL_CMD;
			} else if (!strncmp(*argv, "--start", strlen("--start")) ||
				!strncmp(*argv, "-s", strlen("-s"))) {
				/* the foramt is wl pkteng_cmd --start <dl/ul> [nframes] [ifs] */
				/* try to get the 2 optional params */
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_START;
			} else if (!strncmp(*argv, "--commit", strlen("--commit")) ||
				!strncmp(*argv, "-c", strlen("-c"))) {
			/* set list of stas (in order) */
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_COMMIT;
			} else if (!strncmp(*argv, "--list", strlen("--list")) ||
				!strncmp(*argv, "-l", strlen("-l"))) {
			/* set list of stas (in order) */
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_STALIST;
			} else if (!strncmp(*argv, "--delsta", strlen("--delsta"))) {
			/* set list of stas (in order) */
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_DELSTA;
			} else if (!strncmp(*argv, "--reset", strlen("--reset")) ||
				!strncmp(*argv, "--init", strlen("--init"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_RESET;
			} else if (!strncmp(*argv, "--sta1", strlen("--sta1")) ||
				!strncmp(*argv, "-u1", strlen("-u1")) ||
				!strncmp(*argv, "-usr1", strlen("-usr1"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_STACFG1;
			} else if (!strncmp(*argv, "--sta", strlen("--sta")) ||
				!strncmp(*argv, "-u", strlen("-u")) ||
				!strncmp(*argv, "-usr", strlen("-usr"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_STACFG;
			} else if (!strncmp(*argv, "--queue", strlen("--queue")) ||
				!strncmp(*argv, "-q", strlen("-q"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_TXQCFG;
			} else if (!strncmp(*argv, "--siga", strlen("--siga"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_SIGA;
			} else if (!strncmp(*argv, "--pctl", strlen("--ptctl"))) {
				pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_PCTL;
			} else {
				ret = BCME_BADARG;
				goto exit;
			}

			argv++;
		}

	/* parse if giving digit-style settings */
	} else {
		/* get opcode */
		pecfg_params->opcode = strtol(*argv, NULL, 0);
		if (pecfg_params->opcode == 0 && !bcm_isdigit(*argv[0])) {
			/* the case of <cmd> key, still need to parse this arg later */
		} else {
			argv++;
		}
	}

	is_get = FALSE;

	if (*argv && !strncmp(*argv, "-v", strlen("-v"))) {
		/* skip -v if it is the first */
		argv++;
	}

	switch (pecfg_params->opcode) {
	case PKTENGCMD_OPCODE_PUB_ARGKEY_GET:
		pecfg_params->u.argkeyval.val[0] = -1;
		pecfg_params->u.argkeyval.val[1] = -1;
		is_get = TRUE;
		ret = wl_pkteng_cmd_get_key_val(pecfg_params, argv, FALSE);
		break;
	case PKTENGCMD_OPCODE_PUB_ARGKEY_GNRAL:
		pecfg_params->u.argkeyval.val[0] = -1;
		pecfg_params->u.argkeyval.val[1] = -1;
		if (*argv) {
			pecfg_params->u.argkeyval.val[0] = strtol(*argv++, NULL, 0);
			if (*argv) {
				pecfg_params->u.argkeyval.val[0] = strtol(*argv++, NULL, 0);
				ret = wl_pkteng_cmd_get_key_val(pecfg_params, argv, TRUE);
			}
		}
		/* fall through */
	case PKTENGCMD_OPCODE_PUB_ARGKEY_CFG:
		if (pecfg_params->u.argkeyval.valstr_len == 0) {
			is_get = TRUE;
		}
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_DELSTA:
		/* fall through */
	case PKTENGCMD_OPCODE_PUB_ARGINT_STALIST:
		/* format is 1,3,5 or 1,3,6-7 */
		if (*argv == NULL) {
			is_get = TRUE;
			break;
		}
		splt = strtok(*argv, ",");
		while (splt != NULL) {
			tmp1 = strtoul(splt, &endptr, 0);
			if ((endptr != NULL) && (*endptr == '-')) {
				endptr++;
				tmp2 = strtoul(endptr, NULL, 0);
			} else {
				tmp2 = tmp1;
			}
			for (i = tmp1; i <= tmp2 && argc < WL_PKTENG_CMD_MAX_ARGS; i++) {
				pecfg_params->u.argint.argv[argc++] = i;
			}
			splt = strtok(NULL, ",");
		}
		pecfg_params->u.argint.argc = argc;

		break;

	case PKTENGCMD_OPCODE_PUB_ARGBUF_GET:
		is_get = TRUE;
		/* fall through */
	case PKTENGCMD_OPCODE_PUB_ARGBUF_SET:
		if (strlen(*argv) > WL_PKTENG_CMD_MAX_BUF) {
			ret = BCME_BADARG;
		} else {
			pecfg_params->u.argbuffer.sz = strlen(*argv);
			memcpy(pecfg_params->u.argbuffer.buf, *argv, pecfg_params->u.argbuffer.sz);
			ret = wlu_iovar_set(wl, cmd->name, pecfg_params, pkteng_cmd_param_size);
		}
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_STACFG:
		if (*argv == NULL) {
			ret = BCME_BADARG;
			break;
		}
		pecfg_params->u.argint.argv[argc++] = strtol(*argv++, NULL, 0);
		if (*argv == NULL) {
			break;
		}

		/* the second param must be ethernet addr */
		if (!strncmp(*argv, "-1", strlen("-1"))) {
			pecfg_params->u.argint.argv[argc++] = -1;
			pecfg_params->u.argint.argv[argc++] = -1;
		} else if (!strncmp(*argv, "-q", strlen("-q"))) {
			/* special treatment for -u <uid> -q <id> case */
			argv++;
			if (*argv == NULL) {
				ret = BCME_BADARG;
				break;
			}
			pecfg_params->opcode = PKTENGCMD_OPCODE_PUB_ARGINT_TXQCFG;
			pecfg_params->u.argint.argv[argc++] = strtol(*argv, NULL, 0);
		} else {
			if (!wl_ether_atoe(*argv, &ea)) {
				ret = BCME_USAGE_ERROR;
				break;
			}
			pecfg_params->u.argint.argv[argc++] = ea.octet[0] |
			(ea.octet[1] << 8) | (ea.octet[2] << 16) | (ea.octet[3] << 24);
			pecfg_params->u.argint.argv[argc++] = ea.octet[4] | (ea.octet[5] << 8);
		}
		argv++;
		/* fall through */
	case PKTENGCMD_OPCODE_PUB_ARGINT_TXQCFG:
	case PKTENGCMD_OPCODE_PUB_ARGINT_SIGA:
	case PKTENGCMD_OPCODE_PUB_ARGINT_PCTL:
	case PKTENGCMD_OPCODE_PUB_ARGINT_RESET:
	case PKTENGCMD_OPCODE_PUB_ARGINT_START:
	case PKTENGCMD_OPCODE_PUB_ARGINT_STOP:
	case PKTENGCMD_OPCODE_PUB_ARGINT_PAUSE:
	case PKTENGCMD_OPCODE_PUB_ARGINT_COMMIT:
	default:
		while (*argv != NULL && argc < WL_PKTENG_CMD_MAX_ARGS) {
			if (!strncmp(*argv, "-v", strlen("-v"))) {
				/* skip -v option if any */
				argv++;
				continue;
			}
			pecfg_params->u.argint.argv[argc++] = strtol(*argv++, NULL, 0);
		}
		pecfg_params->u.argint.argc = argc;

	}

	if (pecfg_params->opcode > PKTENGCMD_OPCODE_PUB_END && pecfg_params->opcode >= 100) {
		is_get = (pecfg_params->opcode % 2 == 0); /* even is get cmd */
	}

	if (ret != BCME_OK) {
		goto exit;
	}

	if ((pecfg_params->flags & PKTENGCMD_FLAGS_VERBOSE) != 0) {
		if (pecfg_params->opcode == PKTENGCMD_OPCODE_PUB_ARGKEY_CFG ||
			pecfg_params->opcode == PKTENGCMD_OPCODE_PUB_ARGKEY_GNRAL ||
			pecfg_params->opcode == PKTENGCMD_OPCODE_PUB_ARGKEY_GET) {
			fprintf(stdout, "opcode %d flags %x is_get %d val1 %d val2 %d keylen %d "
				"vallen %d keystr \"%s\" valstr \"%s\"\n",
				pecfg_params->opcode,
				pecfg_params->flags,
				is_get,
				pecfg_params->u.argkeyval.val[0],
				pecfg_params->u.argkeyval.val[1],
				pecfg_params->u.argkeyval.keystr_len,
				pecfg_params->u.argkeyval.valstr_len,
				pecfg_params->u.argkeyval.keystr,
				pecfg_params->u.argkeyval.valstr);
		} else if (pecfg_params->opcode == PKTENGCMD_OPCODE_PUB_ARGBUF_GET ||
			pecfg_params->opcode == PKTENGCMD_OPCODE_PUB_ARGBUF_SET) {
			fprintf(stdout, "opcode %d flags %x is_get %d string %s ",
				pecfg_params->opcode,
				pecfg_params->flags,
				is_get,
				pecfg_params->u.argbuffer.buf);
		} else {
			fprintf(stdout, "opcode %d flags %x argc %d isget %d ",
				pecfg_params->opcode,
				pecfg_params->flags,
				pecfg_params->u.argint.argc, is_get);
			for (i = 0; i < pecfg_params->u.argint.argc; i++) {
				fprintf(stdout, "argv[%d] %d(0x%x) ",
					i, pecfg_params->u.argint.argv[i],
					pecfg_params->u.argint.argv[i]);
			}
		}
	}
	if (is_get) {
	/* Handle the get case */
		if ((retbuf = CALLOC(WL_DUMP_BUF_LEN)) == NULL) {
			fprintf(stderr, "Error allocating return buffer for MAC sample capture\n");
			ret = BCME_NOMEM;
			goto exit;
		}
		ret = wlu_iovar_getbuf(wl, cmd->name, pecfg_params,
			pkteng_cmd_param_size, retbuf, WL_DUMP_BUF_LEN);
		if (ret < 0) {
			goto exit;
		}
		fputs(retbuf, stdout);
	} else {
	/* Handle the set case */
		ret = wlu_iovar_set(wl, cmd->name, pecfg_params, pkteng_cmd_param_size);
	}

exit:
	if (pecfg_params)
		free(pecfg_params);

	if (retbuf)
		free(retbuf);
	return ret;
}

static int
wl_pkteng_cmd_get_key_val(wl_pkteng_cmd_params_t *pecfg_params, char **argv, bool must_have_key)
{
	int str_len;
	if (*argv == NULL) {
		pecfg_params->u.argkeyval.keystr_len = 0;
		if (must_have_key) {
			return BCME_USAGE_ERROR;
		} else {
			pecfg_params->u.argkeyval.keystr[0] = '\0';
			return BCME_OK;
		}
	}
	if ((str_len = strlen(*argv)) >= WL_PKTENG_CMD_KEYSTR_MAXLEN)
	{
		return BCME_BADARG;
	}
	pecfg_params->u.argkeyval.keystr_len = str_len;
	strncpy(pecfg_params->u.argkeyval.keystr, *argv, str_len);
	argv++;
	if (*argv == NULL) {
		pecfg_params->u.argkeyval.valstr_len = 0;
		pecfg_params->u.argkeyval.valstr[0] = '\0';
	} else if (**argv == '-') {
		pecfg_params->u.argkeyval.valstr_len = 0;
		pecfg_params->u.argkeyval.valstr[0] = '\0';
	} else {
		if ((str_len = strlen(*argv)) >= WL_PKTENG_CMD_VALSTR_MAXLEN) {
			return BCME_BADARG;
		}
		pecfg_params->u.argkeyval.valstr_len = str_len;
		strncpy(pecfg_params->u.argkeyval.valstr, *argv, str_len);
	}

	return BCME_OK;
}

static uint32
wl_rxiq_prepare(char **argv, wl_iqest_params_t *params, uint8 *resolution)
{
	miniopt_t to;
	const char* fn_name = "wl_rxiqest_prepare";
	int err = BCME_OK, argc, opt_err;
	uint8 lpf_hpc = 1;
	uint8 dig_lpf = 1;
	uint8 gain_correct = 0;
	uint8 extra_gain_3dBsteps = 0;
	uint8 force_gain_type = 0;
	uint8 antenna = 3;

	*resolution = 0;
	/* arg count */
	for (argc = 0; argv[argc]; argc++);

	/* DEFAULT:
	 * gain_correct = 0 (disable gain correction),
	 * lpf_hpc = 1 (sets lpf hpc to lowest value),
	 * dig_lpf = 1; (sets to ltrn_lpf mode)
	 * resolution = 0 (coarse),
	 * samples = 1024 (2^10) and antenna = 3
	 * force_gain_type = 0 (init gain mode)
	 */
	params->rxiq = (extra_gain_3dBsteps << 28) | (gain_correct << 24) | (dig_lpf << 22)
	        | (lpf_hpc << 20) | (*resolution << 16) | (10 << 8) | (force_gain_type << 4)
	        | antenna;

	params->niter = 1;
	params->delay = PHY_RXIQEST_AVERAGING_DELAY;
	if (argc == 0)
		return 0;

	miniopt_init(&to, fn_name, NULL, FALSE);
	while ((opt_err = miniopt(&to, argv)) != -1) {
		if (opt_err == 1) {
			err = BCME_USAGE_ERROR;
			goto exit;
		}
		argv += to.consumed;

		if (to.opt == 'g') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int"
					" for gain-correction (0, 1, 2, 3, 4, 7, 8)\n",
					fn_name, to.valstr);

				err = BCME_BADARG;
				goto exit;
			}
			if ((to.val < 0) || (to.val > 8)) {
				fprintf(stderr, "%s: invalid gain-correction select %d"
					" (0,1,2,3,4,7,8)\n", fn_name, to.val);
				err = BCME_BADARG;
				goto exit;
			}
			gain_correct = to.val & 0xf;
			params->rxiq = ((gain_correct << 24) | (params->rxiq & 0xf0ffffff));
		}
		if (to.opt == 'f') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int"
					" for lpf-hpc override select (0, 1)\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if ((to.val < 0) || (to.val > 1)) {
				fprintf(stderr, "%s: invalid lpf-hpc override select %d"
					" (0,1)\n", fn_name, to.val);
				err = BCME_BADARG;
				goto exit;
			}
			lpf_hpc = to.val & 0x3;
			params->rxiq = ((lpf_hpc << 20) | (params->rxiq & 0xffcfffff));
		}
		if (to.opt == 'w') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int"
					" for dig-lpf override select (0, 1 or 2)\n",
					fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if ((to.val < 0) || (to.val > 2)) {
				fprintf(stderr, "%s: invalid dig-lpf override select %d"
					" (0,1,2)\n", fn_name, to.val);
				err = BCME_BADARG;
				goto exit;
			}
			dig_lpf = to.val & 0x3;
			params->rxiq = ((dig_lpf << 22) | (params->rxiq & 0xff3fffff));
		}
		if (to.opt == 'r') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int"
					" for resolution (0, 1)\n", fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if ((to.val < 0) || (to.val > 1)) {
				fprintf(stderr, "%s: invalid resolution select %d"
					" (0,1)\n", fn_name, to.val);
				err = BCME_BADARG;
				goto exit;
			}
			*resolution = to.val & 0xf;
			params->rxiq = ((*resolution << 16) | (params->rxiq & 0xfff0ffff));
		}
		if (to.opt == 's') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int for"
					" the sample count\n", fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if (to.val < 0 || to.val > 16) {
				fprintf(stderr, "%s: sample count too large %d"
					"(10 <= x <= 16)\n", fn_name, to.val);
				err = BCME_BADARG;
				goto exit;
			}
			params->rxiq = (((to.val & 0xff) << 8) | (params->rxiq & 0xffff00ff));
		}
		if (to.opt == 'a') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int"
					" for antenna (0, 1, 3)\n", fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if ((to.val < 0) || (to.val > 3)) {
				fprintf(stderr, "%s: invalid antenna select %d\n",
					fn_name, to.val);
				err = BCME_BADARG;
				goto exit;
			}
			params->rxiq = ((params->rxiq & 0xfffffff0) | (to.val & 0xf));
		}
		if (to.opt == 'e') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int"
					" for extra INITgain\n", fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if ((to.val < 0) || (to.val > 24) || (to.val % 3 != 0)) {
				fprintf(stderr,
					"%s: Valid extra INITgain = {0, 3, .., 21, 24}\n",
					fn_name);
				err = BCME_BADARG;
				goto exit;
			}
			params->rxiq = ((((to.val/3) & 0xf) << 28) | (params->rxiq & 0x0fffffff));
		}
		if (to.opt == 'i') {
			if (!to.good_int) {
				fprintf(stderr,
					"%s: could not parse \"%s\" as an int"
					" for init or clipLO mode\n", fn_name, to.valstr);
				err = BCME_BADARG;
				goto exit;
			}
			if ((to.val != 0) && (to.val != 1) &&
					(to.val != 2) && (to.val != 3) && (to.val != 4)) {
				fprintf(stderr,
					"%s: Valid options - 0(default gain), 1(fixed high gain)"
					"or 4(fixed low gain). \n",
						fn_name);
				err = BCME_BADARG;
				goto exit;
			}
			params->rxiq = ((params->rxiq & 0xffffff0f) | ((to.val << 4) & 0xf0));
		}
	}
	params->rxiq = htod32(params->rxiq);
exit:
	return err;
}

static void
wl_rxiq_print_4365(uint16 *iqest_core, uint8 resolution)
{
	if (resolution == 1) {
		/* fine resolution power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;
		/* Four chains: */
		for (core = 0; core < WL_STA_ANT_MAX; core ++) {
			tmp = iqest_core[core];
			if (tmp < 0) {
				tmp = -1*tmp;
				printf("-%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
			} else if (tmp > 0) {
				printf("%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
			}
		}
		printf("\n");
	} else {
		/* fine resolution power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;
		/* Four chains: */
		for (core = 0; core < WL_STA_ANT_MAX; core ++) {
			tmp = (int8)(iqest_core[core]);
			if (tmp != 0)
				printf("%ddBm ", tmp);
		}
		printf("\n");
	}
}

static void
wl_rxiq_print(uint32 rxiq, uint8 resolution)
{
	if (resolution == 1) {
		/* fine resolution power reporting (0.25dB resolution) */
		uint8 core;
		int16 tmp;
		if (rxiq >> 20) {
			/* Three chains: */
			for (core = 0; core < 3; core ++) {
				tmp = (rxiq >> (10*core)) & 0x3ff;
				tmp = ((int16)(tmp << 6)) >> 6; /* sign extension */
				if (tmp < 0) {
					tmp = -1*tmp;
					printf("-%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				} else {
					printf("%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				}
			}
			printf("\n");
		} else if (rxiq >> 10) {
			/* 2 chains */
			for (core = 0; core < 2; core ++) {
				tmp = (rxiq >> (10*core)) & 0x3ff;
				tmp = ((int16)(tmp << 6)) >> 6; /* sign extension */
				if (tmp < 0) {
					tmp = -1*tmp;
					printf("-%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				} else {
					printf("%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
				}
			}
			printf("\n");
		} else {
			/* 1 chain */
			tmp = rxiq & 0x3ff;
			tmp = ((int16)(tmp << 6)) >> 6; /* sign extension */
			if (tmp < 0) {
				tmp = -1*tmp;
				printf("-%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
			} else {
				printf("%d.%ddBm ", (tmp >> 2), (tmp & 0x3)*25);
			}
			printf("\n");
		}

	} else {
		if (rxiq >> 24)
			printf("%ddBm %ddBm %ddBm %ddBm \n", (int8)(rxiq & 0xff),
				(int8)((rxiq >> 8) & 0xff), (int8)((rxiq >> 16) & 0xff),
				(int8)((rxiq >> 24) & 0xff));
		else if (rxiq >> 16)
			printf("%ddBm %ddBm %ddBm\n", (int8)(rxiq & 0xff),
				(int8)((rxiq >> 8) & 0xff), (int8)((rxiq >> 16) & 0xff));
		else if (rxiq >> 8)
			printf("%ddBm %ddBm\n", (int8)(rxiq & 0xff), (int8)((rxiq >> 8) & 0xff));
		else
			printf("%ddBm\n", (int8)(rxiq & 0xff));
	}
}

#define IFERR(x) do { err = (x); if (err) return err; } while (0)
static int
wl_rxiq(void *wl, cmd_t *cmd, char **argv)
{
	wl_iqest_params_t params;
	uint8 resolution;
	int err;
	uint16 iqest_core[WL_STA_ANT_MAX];
	wlc_rev_info_t revinfo;

	memset(&params, 0, sizeof(params));
	memset(&revinfo, 0, sizeof(revinfo));
	memset(&iqest_core, 0, sizeof(iqest_core));
	IFERR(wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo)));

	IFERR(wl_rxiq_prepare(argv, &params, &resolution));

	if (BCM4365_CHIP(dtoh32(revinfo.chipnum)) ||
		BCM7271_CHIP(dtoh32(revinfo.chipnum)) ||
		EMBEDDED_2x2AX_CORE(dtoh32(revinfo.chipnum)) ||
		BCM43684_CHIP(dtoh32(revinfo.chipnum)) ||
		BCM6710_CHIP(dtoh32(revinfo.chipnum))) {
		IFERR(wlu_var_setbuf(wl, cmd->name, &params, sizeof(params)));
		IFERR(wlu_iovar_get(wl, cmd->name, iqest_core, WL_STA_ANT_MAX*sizeof(int16)));
		wl_rxiq_print_4365(iqest_core, resolution);
		return BCME_OK;
	}
	IFERR(wlu_iovar_set(wl, cmd->name, &params, sizeof(params)));
	IFERR(wlu_iovar_getint(wl, cmd->name, (int*)&params.rxiq));
	wl_rxiq_print(params.rxiq, resolution);

	return BCME_OK;
}

#define SWEEP_ERROR "phy_rxiqest_sweep Error: "
static int
wl_rxiq_sweep(void *wl, cmd_t *cmd, char **argv)
{
	miniopt_t to;
	int err, params_size, buf_size, i, offset;
	uint8 nchannels, resolution, all;
	char *ptr, *channels = NULL;
	wl_iqest_sweep_params_t *sweep_params;
	wl_iqest_result_t *result;
	wlc_rev_info_t revinfo;

	memset(&revinfo, 0, sizeof(revinfo));
	IFERR(wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo)));

	if (BCM4365_CHIP(dtoh32(revinfo.chipnum)) ||
		BCM43684_CHIP(dtoh32(revinfo.chipnum)) ||
		EMBEDDED_2x2AX_CORE(dtoh32(revinfo.chipnum)) ||
		BCM7271_CHIP(dtoh32(revinfo.chipnum)))
		return BCME_UNSUPPORTED;

	offset = strlen(cmd->name) + 1;
	sweep_params = (wl_iqest_sweep_params_t *)(buf + offset);
	if ((err = wl_rxiq_prepare(argv, &sweep_params->params, &resolution)))
		return err;
	miniopt_init(&to, "wl_rxiq_sweep", NULL, FALSE);
	while ((err = miniopt(&to, argv)) != -1) {
		if (err == 1)
			return BCME_USAGE_ERROR;

		argv += to.consumed;

		if (to.opt == 'c') {
			if (channels) {
				err = BCME_BADARG;
				fprintf(stderr, SWEEP_ERROR "Duplicate channel parameters\n");
				goto Exit;
			}
			channels = strdup(to.valstr);
			if (!channels) {
				fprintf(stderr, SWEEP_ERROR "Unable to allocate %d bytes for "
					"wl_rxiq_sweep\n", (int)strlen(to.valstr));
				return BCME_NOMEM;
			}
		}
	}

	if (!channels) {
		err = BCME_BADCHAN;
		fprintf(stderr, SWEEP_ERROR "Missing channel parameters\n");
		goto Exit;
	}

	for (nchannels = 0, all = 0, ptr = channels; ptr; ptr = strchr(ptr + 1, ','), ++nchannels) {
		char *channel = *ptr == ',' ? ptr + 1 : ptr;

		if (!strnicmp(channel, "all", 3)) {
			if (channel == ptr && !channel[3]) {
				nchannels = 1;
				all = 1;
				break;
			}
			fprintf(stderr, SWEEP_ERROR "Channel argument should be either all or a "
				"comma separated list of channels\n");
			err = BCME_BADCHAN;
			goto Exit;
		}
	}

	if (nchannels > WL_NUMCHANNELS) {
		fprintf(stderr, SWEEP_ERROR "Maximum of %d channels allowed\n", WL_NUMCHANNELS);
		err = BCME_BADCHAN;
		goto Exit;
	}

	params_size = sizeof(*sweep_params) + (nchannels - 1) * sizeof(uint8);
	buf_size = sizeof(*result) + ((all ? WL_NUMCHANNELS : nchannels) - 1)
		* sizeof(wl_iqest_value_t);
	if (buf_size < params_size + offset)
		buf_size = params_size + offset;
	if (buf_size > WLC_IOCTL_MEDLEN) {
		fprintf(stderr, SWEEP_ERROR "Internal error - buffer is not big enough\n");
		err = BCME_BUFTOOSHORT;
		goto Exit;
	}
	offset += params_size;
	if (all) {
		sweep_params->nchannels = 1;
		sweep_params->channel[0] = 0;
		if (sweep_params->params.niter > WL_ITER_LIMIT_MANY_CHAN) {
			fprintf(stderr, SWEEP_ERROR "Maximum %d averaging iterations allowed if "
				"number of channel is greater than %d\n", WL_ITER_LIMIT_MANY_CHAN,
				WL_NUMCHANNELS_MANY_CHAN);
			err = BCME_BADARG;
			goto Exit;
		}
	}
	else {
		sweep_params->nchannels = 0;
		for (i = 0, ptr = strtok(channels, ","); ptr; ++i, ptr = strtok(NULL, ",")) {
			int j;
			char *endptr = NULL;
			int channel = (int)strtoul(ptr, &endptr, 0);

			if (*endptr || channel < 1 || channel > 173) {
				fprintf(stderr, SWEEP_ERROR "wrong channel value %s\n", ptr);
				err = BCME_BADARG;
				goto Exit;
			}
			for (j = 0; j < sweep_params->nchannels; ++j) {
				if (channel == sweep_params->channel[j]) {
					err = BCME_BADCHAN;
					fprintf(stderr, SWEEP_ERROR "Duplicate channel "
						"parameters\n");
					goto Exit;
				}
			}
			sweep_params->channel[sweep_params->nchannels++] = (uint8)channel;
		}
	}

	if (sweep_params->nchannels > WL_NUMCHANNELS_MANY_CHAN && sweep_params->params.niter
		> WL_ITER_LIMIT_MANY_CHAN) {
		fprintf(stderr, SWEEP_ERROR "Maximum %d averaging iterations allowed if number"
			" of channel is greater than %d\n", WL_ITER_LIMIT_MANY_CHAN,
			WL_NUMCHANNELS_MANY_CHAN);
		err = BCME_BADARG;
		goto Exit;
	}

	if ((err = wlu_iovar_getbuf(wl, cmd->name, sweep_params, params_size, buf, buf_size)) < 0)
		goto Exit;

	result = (wl_iqest_result_t *)buf;
	for (i = 0; i < result->nvalues; ++i) {
		printf("Channel: %u\t", result->value[i].channel);
		wl_rxiq_print(dtoh32(result->value[i].rxiq), resolution);
	}
Exit:
	free(channels);
	return err;
}

#if defined(BCMDBG)
static int
wl_phy_debug_cmd(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int val;
	char *val_name;

	UNUSED_PARAMETER(cmd);

	/* command name */
	val_name = *argv++;
	val = (*argv == NULL) ? 0 : atoi(*argv);

	/* XXX
	printf("\n--------------------------------------------------------\n");
	printf("PHY COMMAND (%s) with input argument : %d", val_name, val);
	printf("\n--------------------------------------------------------\n");
	printf("\n");
	*/

	if ((err = wlu_iovar_setint(wl, val_name, (int)val)) < 0)
		printf("PHY DEBUG COMMAND error %d\n", err);

	return err;

}
#endif // endif

static int
wl_rifs(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int val, rifs;
	char *val_name;

	UNUSED_PARAMETER(cmd);

	/* command name */
	val_name = *argv++;

	if (!*argv) {
		if ((err = wlu_iovar_getint(wl, val_name, (int*)&rifs)) < 0)
			return err;

		printf("%s\n", ((rifs & 0xff) ? "On" : "Off"));
		return 0;
	}

	val = rifs = (atoi(*argv) ? 1 : 0);

	if ((err = wlu_set(wl, WLC_SET_FAKEFRAG, &val, sizeof(int))) < 0) {
		printf("Set frameburst error %d\n", err);
		return err;
	}
	if ((err = wlu_iovar_setint(wl, val_name, (int)rifs)) < 0)
		printf("Set rifs error %d\n", err);

	return err;
}

static int
wl_rifs_advert(void *wl, cmd_t *cmd, char **argv)
{
	int err;
	int rifs_advert;
	char *val_name;

	BCM_REFERENCE(cmd);

	/* command name */
	val_name = *argv++;

	if (!*argv) {
		if ((err = wlu_iovar_getint(wl, val_name, (int*)&rifs_advert)) < 0)
			return err;

		printf("%s\n", ((rifs_advert & 0xff) ? "On" : "Off"));
		return 0;
	}

	if (strcmp(*argv, "-1") && strcmp(*argv, "0"))
		return BCME_USAGE_ERROR;

	rifs_advert = atoi(*argv);

	if ((err = wlu_iovar_setint(wl, val_name, (int)rifs_advert)) < 0)
		printf("Set rifs mode advertisement error %d\n", err);

	return err;
}

static int
wlu_afeoverride(void *wl, cmd_t *cmd, char **argv)
{
	char var[256];
	uint32 int_val;
	bool get = TRUE;
	void *ptr = NULL;
	char *endptr;

	if (argv[1]) {
		uint32 get_val;
		get = FALSE;
		int_val = htod32(strtoul(argv[1], &endptr, 0));
		if (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr) < 0)
			return -1;
		get_val = *(int *)ptr;
		get_val &= ~1;
		if (int_val)
			int_val = get_val | 1;
		else
			int_val = get_val;
		memcpy(var, (char *)&int_val, sizeof(int_val));
	}
	if (get) {
		if (wlu_var_getbuf(wl, cmd->name, var, sizeof(var), &ptr) < 0)
			return -1;
		printf("0x%x\n", dtoh32(*(int *)ptr));
	}
	else
		wlu_var_setbuf(wl, cmd->name, &var, sizeof(var));
	return 0;
}

/*
 *  RADAR detection parameter control
 */
static int
wl_radar_args(void *wl, cmd_t *cmd, char **argv)
{
	int ret;
	wl_radar_args_t ra;

	/* Skip the command name */
	argv++;

	if (*argv == NULL) {
		/* Get */

		if ((ret = wlu_iovar_get(wl, cmd->name, &ra, sizeof(ra))) < 0)
			return ret;

		if (ra.version != WL_RADAR_ARGS_VERSION) {
			printf("\tIncorrect version of RADAR_ARGS struct: expected %d; got %d\n",
			       WL_RADAR_ARGS_VERSION, ra.version);
			return -1;
		}
		printf("version %d npulses %d ncontig %d min_pw %d max_pw %d thresh0 0x%x "
		       "thresh1 0x%x\n",
		       ra.version, ra.npulses, ra.ncontig, ra.min_pw,
		       ra.max_pw, ra.thresh0, ra.thresh1);
		printf("blank 0x%x fmdemodcfg 0x%x npulses_lp %d min_pw_lp %d "
		       "max_pw_lp %d\n",
		       ra.blank, ra.fmdemodcfg, ra.npulses_lp, ra.min_pw_lp,
		       ra.max_pw_lp);
		printf("min_fm_lp %d max_span_lp %d min_deltat %d max_deltat %d\n",
		       ra.min_fm_lp, ra.max_span_lp, ra.min_deltat, ra.max_deltat);

		printf("autocorr 0x%x st_level_time 0x%x  t2_min %d fra_pulse_err %d\n",
		       ra.autocorr, ra.st_level_time, ra.t2_min, ra.fra_pulse_err);
		printf("npulses_fra %d npulses_stg2 %d npulses_stg3 %d percal_mask 0x%x quant %d\n",
			ra.npulses_fra, ra.npulses_stg2, ra.npulses_stg3, ra.percal_mask,
			ra.quant);
		printf("min_burst_intv_lp %d max_burst_intv_lp %d nskip_rst_lp %d max_pw_tol %d "
				"feature_mask 0x%x\n",
				ra.min_burst_intv_lp, ra.max_burst_intv_lp, ra.nskip_rst_lp,
				ra.max_pw_tol, ra.feature_mask);
		printf("thresh0_sc 0x%x thresh1_sc 0x%x\n",
				ra.thresh0_sc, ra.thresh1_sc);

		/* this part prints only param values */
		printf("%d %d %d %d %d 0x%x "
		       "0x%x",
		       ra.version, ra.npulses, ra.ncontig, ra.min_pw,
		       ra.max_pw, ra.thresh0, ra.thresh1);
		printf(" 0x%x 0x%x %d %d "
		       "%d",
		       ra.blank, ra.fmdemodcfg, ra.npulses_lp, ra.min_pw_lp,
		       ra.max_pw_lp);
		printf(" %d %d %d %d",
		       ra.min_fm_lp, ra.max_span_lp, ra.min_deltat, ra.max_deltat);

		printf(" 0x%x 0x%x %d %d",
		       ra.autocorr, ra.st_level_time, ra.t2_min, ra.fra_pulse_err);
		printf(" %d %d %d 0x%x %d",
			ra.npulses_fra, ra.npulses_stg2, ra.npulses_stg3, ra.percal_mask,
			ra.quant);
		printf(" %d %d %d %d "
				"0x%x  0x%x 0x%x\n",
				ra.min_burst_intv_lp, ra.max_burst_intv_lp, ra.nskip_rst_lp,
				ra.max_pw_tol, ra.feature_mask, ra.thresh0_sc, ra.thresh1_sc);

	} else {
		/* Set */
		char *endptr = NULL;
		int val_count = 32;
		long vals[32];
		long *pval;
		int i;

		for (i = 0; i < val_count; i++, argv++) {
			/* verify that there is another arg */
			if (*argv == NULL)
				return BCME_USAGE_ERROR;

			vals[i] = strtol(*argv, &endptr, 0);

			/* make sure all the value string was parsed by strtol */
			if (*endptr != '\0')
				return BCME_USAGE_ERROR;
		}

		pval = vals;

		ra.version       = *pval++;
		ra.npulses       = *pval++;
		ra.ncontig       = *pval++;
		ra.min_pw        = *pval++;
		ra.max_pw        = *pval++;
		ra.thresh0       = (uint16)*pval++;
		ra.thresh1       = (uint16)*pval++;
		ra.blank         = (uint16)*pval++;
		ra.fmdemodcfg    = (uint16)*pval++;
		ra.npulses_lp    = *pval++;
		ra.min_pw_lp     = *pval++;
		ra.max_pw_lp     = *pval++;
		ra.min_fm_lp     = *pval++;
		ra.max_span_lp   = *pval++;
		ra.min_deltat    = *pval++;
		ra.max_deltat    = *pval++;
		ra.autocorr      = (uint16)*pval++;
		ra.st_level_time = (uint16)*pval++;
		ra.t2_min        = (uint16)*pval++;
		ra.fra_pulse_err = (uint32)*pval++;
		ra.npulses_fra   = (int)*pval++;
		ra.npulses_stg2  = (int)*pval++;
		ra.npulses_stg3  = (int)*pval++;
		ra.percal_mask   = (int)*pval++;
		ra.quant         = (int)*pval++;
		ra.min_burst_intv_lp = (uint32)*pval++;
		ra.max_burst_intv_lp = (uint32)*pval++;
		ra.nskip_rst_lp  = (int)*pval++;
		ra.max_pw_tol    = (int)*pval++;
		ra.feature_mask  = (uint16)*pval++;
		ra.thresh0_sc    = (uint16)*pval++;
		ra.thresh1_sc    = (uint16)*pval++;

		return wlu_var_setbuf(wl, cmd->name, &ra, sizeof(wl_radar_args_t));
	}
	return ret;
}

static int
wl_radar_thrs(void *wl, cmd_t *cmd, char **argv)
{
	int ret = -1;
	wl_radar_thr_t radar_thrs;

	if (*++argv) {
		/* Set */
		char *endptr;
		int val_count = 16;
		uint16 vals[16];
		uint16 *pval;
		int i;

		for (i = 0; i < val_count; i++, argv++) {
			/* verify that there is another arg */
			if (i < 12) {
				/* for BW20,40,80 */
				if (*argv == NULL)
					return BCME_USAGE_ERROR;
				vals[i] = (uint16)strtol(*argv, &endptr, 0);
				/* make sure all the value string was parsed by strtol */
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
			} else {
				/* for BW160 */
				if (*argv == NULL) {
					if (i == 12) {
						/* To be compatable with older commands */
						vals[i] = 0;
						vals[i+1] = 0;
						vals[i+2] = 0;
						vals[i+3] = 0;
						break;
					} else {
						return BCME_USAGE_ERROR;
					}
				} else {
					vals[i] = (uint16)strtol(*argv, &endptr, 0);
					/* make sure all the value string was parsed by strtol */
					if (*endptr != '\0')
						return BCME_USAGE_ERROR;
				}

			}
		}

		radar_thrs.version = WL_RADAR_THR_VERSION;

		/* Order thresh0_20_lo, thresh1_20_lo, thresh0_40_lo, thresh1_40_lo
		 * thresh0_20_hi, thresh1_20_hi, thresh0_40_hi, thresh1_40_hi
		 */
		pval = vals;
		radar_thrs.thresh0_20_lo = (uint16)*pval++;
		radar_thrs.thresh1_20_lo = (uint16)*pval++;
		radar_thrs.thresh0_40_lo = (uint16)*pval++;
		radar_thrs.thresh1_40_lo = (uint16)*pval++;
		radar_thrs.thresh0_80_lo = (uint16)*pval++;
		radar_thrs.thresh1_80_lo = (uint16)*pval++;
		radar_thrs.thresh0_20_hi = (uint16)*pval++;
		radar_thrs.thresh1_20_hi = (uint16)*pval++;
		radar_thrs.thresh0_40_hi = (uint16)*pval++;
		radar_thrs.thresh1_40_hi = (uint16)*pval++;
		radar_thrs.thresh0_80_hi = (uint16)*pval++;
		radar_thrs.thresh1_80_hi = (uint16)*pval++;
		radar_thrs.thresh0_160_lo = (uint16)*pval++;
		radar_thrs.thresh1_160_lo = (uint16)*pval++;
		radar_thrs.thresh0_160_hi = (uint16)*pval++;
		radar_thrs.thresh1_160_hi = (uint16)*pval++;

		return wlu_var_setbuf(wl, cmd->name, &radar_thrs, sizeof(wl_radar_thr_t));
	}
	return ret;
}

static int
wl_radar_thrs2(void *wl, cmd_t *cmd, char **argv)
{
	int ret = -1;
	wl_radar_thr2_t radar_thrs2;
	argv++;

	if (*argv == NULL) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &radar_thrs2, sizeof(radar_thrs2))) < 0)
					return ret;

				if (radar_thrs2.version != WL_RADAR_ARGS_VERSION) {
					printf("\tIncorrect version"
					"\tof RADAR_ARGS struct:expected %d; got %d\n",
					WL_RADAR_ARGS_VERSION, radar_thrs2.version);
					return -1;
				}
				printf("version %d\n"
				"thresh0_sc_20_lo 0x%x thresh1_sc_20_lo 0x%x "
				"thresh0_sc_40_lo 0x%x thresh1_sc_40_lo 0x%x "
				"thresh0_sc_80_lo 0x%x thresh1_sc_80_lo 0x%x\n"
				"thresh0_sc_20_hi 0x%x thresh1_sc_20_hi 0x%x "
				"thresh0_sc_40_hi 0x%x thresh1_sc_40_hi 0x%x "
				"thresh0_sc_80_hi 0x%x thresh1_sc_80_hi 0x%x\n"
				"thresh0_sc_160_lo 0x%x thresh1_sc_160_lo 0x%x "
				"thresh0_sc_160_hi 0x%x thresh1_sc_160_hi 0x%x\n"
				"fc_varth_sb 0x%x fc_varth_bin5_sb 0x%x notradar_enb 0x%x\n"
				"max_notradar_lp 0x%x max_notradar 0x%x max_notradar_lp_sc 0x%x "
				"max_notradar_sc 0x%x highpow_war_enb 0x%x highpow_sp_ratio 0x%x\n"
				"fm_chk_opt 0x%x fm_chk_pw 0x%x fm_var_chk_pw 0x%x\n"
				"fm_thresh_sp1 0x%x fm_thresh_sp2 0x%x fm_thresh_sp3 0x%x "
				"fm_thresh_etsi4 0x%x fm_thresh_p1c 0x%x fm_tol_div 0x%x\n",
				radar_thrs2.version, radar_thrs2.thresh0_sc_20_lo,
				radar_thrs2.thresh1_sc_20_lo,
				radar_thrs2.thresh0_sc_40_lo, radar_thrs2.thresh1_sc_40_lo,
				radar_thrs2.thresh0_sc_80_lo, radar_thrs2.thresh1_sc_80_lo,
				radar_thrs2.thresh0_sc_20_hi, radar_thrs2.thresh1_sc_20_hi,
				radar_thrs2.thresh0_sc_40_hi, radar_thrs2.thresh1_sc_40_hi,
				radar_thrs2.thresh0_sc_80_hi, radar_thrs2.thresh1_sc_80_hi,
				radar_thrs2.thresh0_sc_160_lo, radar_thrs2.thresh1_sc_160_lo,
				radar_thrs2.thresh0_sc_160_hi, radar_thrs2.thresh1_sc_160_hi,
				radar_thrs2.fc_varth_sb, radar_thrs2.fc_varth_bin5_sb,
				radar_thrs2.notradar_enb, radar_thrs2.max_notradar_lp,
				radar_thrs2.max_notradar,
				radar_thrs2.max_notradar_lp_sc, radar_thrs2.max_notradar_sc,
				radar_thrs2.highpow_war_enb, radar_thrs2.highpow_sp_ratio,
				radar_thrs2.fm_chk_opt, radar_thrs2.fm_chk_pw,
				radar_thrs2.fm_var_chk_pw, radar_thrs2.fm_thresh_sp1,
				radar_thrs2.fm_thresh_sp2, radar_thrs2.fm_thresh_sp3,
				radar_thrs2.fm_thresh_etsi4, radar_thrs2.fm_thresh_p1c,
				radar_thrs2.fm_tol_div);

				/* this part prints only param values */
				printf("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x"
				" 0x%x 0x%x 0x%x 0x%x"
				" 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x"
				" 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				radar_thrs2.thresh0_sc_20_lo, radar_thrs2.thresh1_sc_20_lo,
				radar_thrs2.thresh0_sc_40_lo, radar_thrs2.thresh1_sc_40_lo,
				radar_thrs2.thresh0_sc_80_lo, radar_thrs2.thresh1_sc_80_lo,
				radar_thrs2.thresh0_sc_20_hi, radar_thrs2.thresh1_sc_20_hi,
				radar_thrs2.thresh0_sc_40_hi, radar_thrs2.thresh1_sc_40_hi,
				radar_thrs2.thresh0_sc_80_hi, radar_thrs2.thresh1_sc_80_hi,
				radar_thrs2.thresh0_sc_160_lo, radar_thrs2.thresh1_sc_160_lo,
				radar_thrs2.thresh0_sc_160_hi, radar_thrs2.thresh1_sc_160_hi,
				radar_thrs2.fc_varth_sb, radar_thrs2.fc_varth_bin5_sb,
				radar_thrs2.notradar_enb,
				radar_thrs2.max_notradar_lp, radar_thrs2.max_notradar,
				radar_thrs2.max_notradar_lp_sc, radar_thrs2.max_notradar_sc,
				radar_thrs2.highpow_war_enb, radar_thrs2.highpow_sp_ratio,
				radar_thrs2.fm_chk_opt, radar_thrs2.fm_chk_pw,
				radar_thrs2.fm_var_chk_pw, radar_thrs2.fm_thresh_sp1,
				radar_thrs2.fm_thresh_sp2, radar_thrs2.fm_thresh_sp3,
				radar_thrs2.fm_thresh_etsi4, radar_thrs2.fm_thresh_p1c,
				radar_thrs2.fm_tol_div);
	} else {
		/* Set */
		char *endptr = NULL;
		int val_count = 34;
		long vals[34];
		long *pval;
		int i, k;

		for (i = 0; i < val_count; i++, argv++) {
			/* verify that there is another arg */
			if (i < 21) {
				if (*argv == NULL)
					return BCME_USAGE_ERROR;

				vals[i] = strtol(*argv, &endptr, 0);

				/* make sure all the value string was parsed by strtol */
				if (*endptr != '\0')
					return BCME_USAGE_ERROR;
			} else {
				/* for BW160 */
				if (*argv == NULL) {
					if (i == 21) {
						/* To be compatable with older commands */
						for (k = val_count; k > 11; k--) {
							if (k < 16) {
								vals[k] = 0;
							} else {
								if (k == 26)
									vals[k] = 0xe;
								else if (k > 26)
									vals[k] = 0;
								else
									vals[k] = vals[k-4];
							}
						}
						break;
					} else {
						return BCME_USAGE_ERROR;
					}
				} else {
					vals[i] = (uint16)strtol(*argv, &endptr, 0);
					/* make sure all the value string was parsed by strtol */
					if (*endptr != '\0')
						return BCME_USAGE_ERROR;
				}
			}
		}

		radar_thrs2.version = WL_RADAR_THR_VERSION;

		pval = vals;
		radar_thrs2.thresh0_sc_20_lo = (uint16)*pval++;
		radar_thrs2.thresh1_sc_20_lo = (uint16)*pval++;
		radar_thrs2.thresh0_sc_40_lo = (uint16)*pval++;
		radar_thrs2.thresh1_sc_40_lo = (uint16)*pval++;
		radar_thrs2.thresh0_sc_80_lo = (uint16)*pval++;
		radar_thrs2.thresh1_sc_80_lo = (uint16)*pval++;
		radar_thrs2.thresh0_sc_20_hi = (uint16)*pval++;
		radar_thrs2.thresh1_sc_20_hi = (uint16)*pval++;
		radar_thrs2.thresh0_sc_40_hi = (uint16)*pval++;
		radar_thrs2.thresh1_sc_40_hi = (uint16)*pval++;
		radar_thrs2.thresh0_sc_80_hi = (uint16)*pval++;
		radar_thrs2.thresh1_sc_80_hi = (uint16)*pval++;
		radar_thrs2.thresh0_sc_160_lo = (uint16)*pval++;
		radar_thrs2.thresh1_sc_160_lo = (uint16)*pval++;
		radar_thrs2.thresh0_sc_160_hi = (uint16)*pval++;
		radar_thrs2.thresh1_sc_160_hi = (uint16)*pval++;
		radar_thrs2.fc_varth_sb = (uint16)*pval++;
		radar_thrs2.fc_varth_bin5_sb = (uint16)*pval++;
		radar_thrs2.notradar_enb = (uint16)*pval++;
		radar_thrs2.max_notradar_lp = (uint16)*pval++;
		radar_thrs2.max_notradar = (uint16)*pval++;
		radar_thrs2.max_notradar_lp_sc = (uint16)*pval++;
		radar_thrs2.max_notradar_sc = (uint16)*pval++;
		radar_thrs2.highpow_war_enb = (uint16)*pval++;
		radar_thrs2.highpow_sp_ratio = (uint16)*pval++;
		radar_thrs2.fm_chk_opt = (uint16)*pval++;
		radar_thrs2.fm_chk_pw = (uint16)*pval++;
		radar_thrs2.fm_var_chk_pw = (uint16)*pval++;
		radar_thrs2.fm_thresh_sp1 = (uint16)*pval++;
		radar_thrs2.fm_thresh_sp2 = (uint16)*pval++;
		radar_thrs2.fm_thresh_sp3 = (uint16)*pval++;
		radar_thrs2.fm_thresh_etsi4 = (uint16)*pval++;
		radar_thrs2.fm_thresh_p1c = (uint16)*pval++;
		radar_thrs2.fm_tol_div = (uint16)*pval++;
		return wlu_var_setbuf(wl, cmd->name, &radar_thrs2, sizeof(wl_radar_thr2_t));
	}
	return ret;

}

static int
wl_phy_temp_counters(void *wl, cmd_t *cmd, char **argv)
{
	phy_txcore_temp_cnt_t txcore_temp_cnt;
	argv++;
	int ret = -1;
	uint16 total_number_of_events;

	if (*argv == NULL) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &txcore_temp_cnt,
			sizeof(txcore_temp_cnt))) < 0)
			return ret;
		printf("phy temp throttling counters \n");
		printf("phy tx chain reducing cnts: %d \n", txcore_temp_cnt.phy_temp_reduce_cnt);
		printf("phy tx chain increasing cnts: %d \n", txcore_temp_cnt.phy_temp_incr_cnt);
		total_number_of_events = txcore_temp_cnt.phy_temp_1_tx_reduce
			 + txcore_temp_cnt.phy_temp_2_tx_reduce;
		total_number_of_events += txcore_temp_cnt.phy_temp_3_tx_reduce;
		printf("phy total thermal throttling events: %d \n",
			total_number_of_events);
		printf("1 TX chain reduction: %d\n", txcore_temp_cnt.phy_temp_1_tx_reduce);
		printf("2 TX chain reduction: %d\n", txcore_temp_cnt.phy_temp_2_tx_reduce);
		printf("3 TX chain reduction: %d\n", txcore_temp_cnt.phy_temp_3_tx_reduce);
	} else {
		return BCME_USAGE_ERROR;
	}

	return ret;
}

static int
wl_phy_ulofdma_stats(void *wl, cmd_t *cmd, char **argv)
{
	int ret = -1;
	wl_ulofdma_per_user_rxstats_t per_user_stats;
	uint8 user;
	int16 urssi = 0;
	argv++;

	if (*argv == NULL) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &per_user_stats,
			sizeof(per_user_stats))) < 0)
			return ret;

		printf("ULOFDMA phy rx status (printing all fields for max number of users):\n");
		printf("Number of users: [%d]\n", per_user_stats.num_users);
		printf("MU fstr error:   [%d]\n", per_user_stats.mu_fstr_error);
		printf("MU error code:   [%d]\n", per_user_stats.mu_error_code);

		printf("User mac index:  [");
		for (user = 0; user < MAX_NUM_ULOFDMA_USERS; user++) {
			printf("%8d", per_user_stats.user_mac_idx[user]);
		}
		printf("]\n");

		printf("User rssi:       [");
		for (user = 0; user < MAX_NUM_ULOFDMA_USERS; user++) {
			urssi = per_user_stats.user_rssi[user];
			printf("%5d.%02d", (urssi >> 2), (urssi & 0x3)*25);
		}
		printf("] dBm\n");

		printf("User snr:        [");
		for (user = 0; user < MAX_NUM_ULOFDMA_USERS; user++) {
			printf("%8d", per_user_stats.user_snr[user]);
		}
		printf("] dB\n");

		printf("User freq error: [");
		for (user = 0; user < MAX_NUM_ULOFDMA_USERS; user++) {
			printf("%8d", per_user_stats.user_freq_error[user]);
		}
		printf("] Hz\n");
	} else {
		return BCME_USAGE_ERROR;
	}
	return ret;
}

static int
wl_phy_tracked_freq(void *wl, cmd_t *cmd, char **argv)
{
	int ret = -1;
	wl_tracked_freq_offset_t tracked_freq_stats;
	uint32 offsetHz = 0;
	bool pos_freq;

	argv++;
	if (*argv == NULL) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &tracked_freq_stats,
			sizeof(tracked_freq_stats))) < 0)
			return ret;

		printf("Tracked Freq Offset:\n");
		printf("low:   %d\n", tracked_freq_stats.freq_offset_l);
		printf("high:  %d\n", tracked_freq_stats.freq_offset_h);
		printf("total: %d\n", tracked_freq_stats.freq_offset_total);
		//printf("sign: %d\n",  tracked_freq_stats.pos_freq);
		offsetHz = tracked_freq_stats.tracked_freq_offsetHz;
		pos_freq = tracked_freq_stats.pos_freq;
		printf("freqOffsetHz: %s%d\n", (pos_freq ? " " : "-"), (offsetHz>>13));
		printf("PLL wild base adjust: %s%d\n",
			(pos_freq ? " " : "-"), tracked_freq_stats.pll_wild_base_offset);
	} else {
		return BCME_USAGE_ERROR;
	}
	return ret;
}

static int
wl_phy_dyn_switch_th(void *wl, cmd_t *cmd, char **argv)
{
	int ret = -1;
	wl_dyn_switch_th_t dyn_switch_th;
	argv++;

	if (*argv == NULL) {
		if ((ret = wlu_iovar_get(wl, cmd->name, &dyn_switch_th, sizeof(dyn_switch_th))) < 0)
					return ret;

				if (dyn_switch_th.ver != WL_PHY_DYN_SWITCH_TH_VERSION) {
					printf("\tIncorrect version"
					"\tof phy_dyn_switch_th:expected %d; got %d\n",
					WL_PHY_DYN_SWITCH_TH_VERSION, dyn_switch_th.ver);
					return -1;
				}
				printf("version %d\n"
				"rssi_gain_80_3 %d rssi_gain_80_2 %d "
				"rssi_gain_80_1 %d rssi_gain_80_0 %d \n"
				"rssi_gain_160_3 %d rssi_gain_160_2 %d "
				"rssi_gain_160_1 %d rssi_gain_160_0 %d \n"
				"rssi_th_2 %d rssi_th_1 %d "
				"rssi_th_0 %d\n",
				dyn_switch_th.ver, dyn_switch_th.rssi_gain_80[3],
				dyn_switch_th.rssi_gain_80[2],
				dyn_switch_th.rssi_gain_80[1], dyn_switch_th.rssi_gain_80[0],
				dyn_switch_th.rssi_gain_160[3], dyn_switch_th.rssi_gain_160[2],
				dyn_switch_th.rssi_gain_160[1], dyn_switch_th.rssi_gain_160[0],
				dyn_switch_th.rssi_th[2], dyn_switch_th.rssi_th[1],
				dyn_switch_th.rssi_th[0]);

				/* this part prints only param values */
				printf("%d %d %d %d %d %d %d %d %d "
				"%d %d\n",
				dyn_switch_th.rssi_gain_80[3], dyn_switch_th.rssi_gain_80[2],
				dyn_switch_th.rssi_gain_80[1], dyn_switch_th.rssi_gain_80[0],
				dyn_switch_th.rssi_gain_160[3], dyn_switch_th.rssi_gain_160[2],
				dyn_switch_th.rssi_gain_160[1], dyn_switch_th.rssi_gain_160[0],
				dyn_switch_th.rssi_th[2], dyn_switch_th.rssi_th[1],
				dyn_switch_th.rssi_th[0]);

	} else {
		/* Set */
		char *endptr = NULL;
		uint val_count = 11;
		long vals[11];
		long *pval;
		uint i;

		for (i = 0; i < val_count; i++, argv++) {
			/* verify that there is another arg */
			if (*argv == NULL)
				return BCME_USAGE_ERROR;
			vals[i] = strtol(*argv, &endptr, 0);

			/* make sure all the value string was parsed by strtol */
			if (*endptr != '\0')
				return BCME_USAGE_ERROR;
		}

		dyn_switch_th.ver = WL_PHY_DYN_SWITCH_TH_VERSION;

		pval = vals;
		dyn_switch_th.rssi_gain_80[3] = (uint16)*pval++;
		dyn_switch_th.rssi_gain_80[2] = (uint16)*pval++;
		dyn_switch_th.rssi_gain_80[1] = (uint16)*pval++;
		dyn_switch_th.rssi_gain_80[0] = (uint16)*pval++;
		dyn_switch_th.rssi_gain_160[3] = (uint16)*pval++;
		dyn_switch_th.rssi_gain_160[2] = (uint16)*pval++;
		dyn_switch_th.rssi_gain_160[1] = (uint16)*pval++;
		dyn_switch_th.rssi_gain_160[0] = (uint16)*pval++;
		dyn_switch_th.rssi_th[2] = (int16)*pval++;
		dyn_switch_th.rssi_th[1] = (int16)*pval++;
		dyn_switch_th.rssi_th[0] = (int16)*pval++;
		return wlu_var_setbuf(wl, cmd->name, &dyn_switch_th, sizeof(wl_dyn_switch_th_t));
	}
	return ret;

}

static int
wl_phy_tpc_av(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	uint argc;
	char *endptr;
	uint8 Av_buff[3];
	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc < 2 || argc > 3)
		return BCME_USAGE_ERROR;
	Av_buff[0] = (uint8)strtol(argv[1], &endptr, 0);
	Av_buff[1] = (uint8)strtol(argv[2], &endptr, 0);

	if (argc == 2) {
		if ((ret = wlu_iovar_getbuf(wl, cmd->name, Av_buff, 2*sizeof(uint8),
				buf, WLC_IOCTL_MAXLEN)) < 0) {
			return (ret);
		}
		printf("Av = %d for core%d and sub-band %d\n", *(uint*)buf, Av_buff[0], Av_buff[1]);
	} else if (argc == 3) {
		Av_buff[2] = (uint8)strtol(argv[3], &endptr, 0);
		ret = wlu_iovar_setbuf(wl, cmd->name, Av_buff, 3*sizeof(uint8),
			buf, WLC_IOCTL_MAXLEN);
	}
	return ret;
}

static int
wl_phy_tpc_vmid(void *wl, cmd_t *cmd, char **argv)
{
	int ret = 0;
	uint argc;
	char *endptr;
	uint8 Vmid_buff[3];
	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc < 2 || argc > 3)
		return BCME_USAGE_ERROR;
	Vmid_buff[0] = (uint8)strtol(argv[1], &endptr, 0);
	Vmid_buff[1] = (uint8)strtol(argv[2], &endptr, 0);

	if (argc == 2) {
		if ((ret = wlu_iovar_getbuf(wl, cmd->name, Vmid_buff, 2*sizeof(uint8),
				buf, WLC_IOCTL_MAXLEN)) < 0) {
			return (ret);
		}
		printf("Vmid = %d for core%d and sub-band %d\n", *(uint*)buf, Vmid_buff[0],
				Vmid_buff[1]);
	} else if (argc == 3) {
		Vmid_buff[2] = (uint8)strtol(argv[3], &endptr, 0);
		ret = wlu_iovar_setbuf(wl, cmd->name, Vmid_buff, 3*sizeof(uint8),
			buf, WLC_IOCTL_MAXLEN);
	}
	return ret;
}

static int
wl_phy_dssf_setup(void *wl, cmd_t *cmd, char **argv)
{
	int err = -1;
	int32 dssf_params[5] = { 0 };
	uint argc = 0;
	char *endptr;
	uint8 i = 0;
	int ret = -1;
	wlc_rev_info_t revinfo;
	uint32 phytype;

	memset(&revinfo, 0, sizeof(revinfo));
	ret = wlu_get(wl, WLC_GET_REVINFO, &revinfo, sizeof(revinfo));
	if (ret) {
		return ret;
	}
	phytype = dtoh32(revinfo.phytype);

	if (phytype != WLC_PHY_TYPE_AC) {
		return err;
	}

	/* arg count */
	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc == 0) {
		/* No get for now */
		return BCME_USAGE_ERROR;
	} else {
		if (!((argc == 5) || (argc == 1))) {
			printf("Enter all arguments: <en:1/dis:0> <freq KHz> <depth> "
			       "<stage> <core>\n \t \t \t  OR wl phy_dssf_setup 0 to "
			       "disable DSSF on all cores\n");
			return err;
		}

		for (i = 0; i < argc; i++) {
			dssf_params[i] = strtol(argv[i + 1], &endptr, 0);
		}

		if (argc == 5) {
			if ((dssf_params[1] < -80000) || (dssf_params[1] > 80000)) {
				printf("DSSF freq should be within phy bw\n");
				return BCME_BADARG;
			}

			if (!((dssf_params[2] == 0) || (dssf_params[2] == -1) ||
				(dssf_params[2] == 6) || (dssf_params[2] == 12) ||
				(dssf_params[2] == 18))) {
				printf("DSSF depth options: -1 -> automode, 0 -> bypass, "
				       "6(dB), 12(dB), 18(dB)\n");
				return BCME_BADARG;
			}

			if ((dssf_params[3] < 1) || (dssf_params[3] > 2)) {
				printf("DSSF stage options: 1, 2\n");
				return BCME_BADARG;
			}
		}

		if ((err = wlu_var_setbuf(wl, cmd->name, dssf_params, 5*sizeof(int32))) < 0) {
			printf("Unable to program DSSF\n");
			return err;
		}
	}
	return 0;
}

#if defined(WLTEST)
typedef struct {
	uint16	val;
	const char *str;
} patrim_t;

static const patrim_t patrims[] = {
	{0x0, "NULL"},
	{0x1, "bw40"},
	{0x2, "bw80"},
	{0x3, "bw4080"},
	{0x4, "cck"},
	{0x11, "NULL, bw40"},
	{0x12, "NULL, bw80"},
	{0x13, "NULL, bw4080"},
	{0x14, "NULL, cck"},
	{0x15, "bw40, bw80"},
	{0x21, "NULL, bw40, bw80"},
	{0x22, "NULL, bw40, cck"},
};

static int
wl_patrim(void *wl, cmd_t *cmd, char **argv)
{
	int patrim, ret;
	uint32 i;

	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(argv);

	if ((ret = wlu_iovar_getint(wl, "patrim", &patrim)) < 0)
		return ret;

	for (i = 0; i < sizeof(patrims) / sizeof(patrims[0]); i ++) {
		if (patrim == patrims[i].val) {
			printf("%s\n", patrims[i].str);
			return 0;
		}
	}

	return BCME_ERROR;
}
#endif // endif

/* TXCAL IOVARS */
static int
wl_txcal_gainsweep_meas(void *wl, cmd_t *cmd, char **argv)
{
	wl_txcal_meas_ncore_t * txcal_meas, * txcal_meas_old;
	wl_txcal_meas_percore_t * per_core;
	wl_txcal_meas_old_t * txcal_meas_legacy;
	int16 pwr[WLC_TXCORE_MAX][MAX_NUM_TXCAL_MEAS];

	void *ptr = NULL;
	int err = BCME_OK;
	int version_err = BCME_OK;
	void* buf = NULL;
	uint8 core;
	uint16 i;
	uint16 buf_size = OFFSETOF(wl_txcal_meas_ncore_t, txcal_percore) +
			WLC_TXCORE_MAX* sizeof(wl_txcal_meas_percore_t);

	/* Allocate buffer for set iovar */
	buf = CALLOC(buf_size);
	if (buf == NULL) {
		err = BCME_NOMEM;
		goto fail;
	}

	txcal_meas = (wl_txcal_meas_ncore_t *)buf;

	if (*++argv) {
		i = 0;
		core = strtoul(*argv, NULL, 0);

		if (core > (WLC_TXCORE_MAX- 1)) {
			err = BCME_USAGE_ERROR;
			goto fail;
		}

		if (!*++argv) {
			err = BCME_USAGE_ERROR;
			goto fail;
		}

		/* Check for version */
		version_err = wlu_var_getbuf_sm(wl, "txcal_ver", NULL, 0, &ptr);
		if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* using wl with backward compatibility for old firmware */
			memset(pwr, 0, WLC_TXCORE_MAX_OLD*MAX_NUM_TXCAL_MEAS*sizeof(pwr[0][0]));
			i = 0;
			if ((err = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, &ptr)) < 0)
			   goto fail;
			txcal_meas_legacy = (wl_txcal_meas_old_t *)ptr;
			memcpy(&pwr[0][0], &txcal_meas_legacy->pwr[0][0],
			        WLC_TXCORE_MAX_OLD*MAX_NUM_TXCAL_MEAS*sizeof(pwr[0][0]));
			do {
				if (i >= MAX_NUM_TXCAL_MEAS) {
					printf("Entries exceeded max allowed\n");
					err = BCME_USAGE_ERROR;
					goto fail;
				}
				pwr[core][i] = strtoul(*argv, NULL, 0);
				i++;
			} while (*++argv);
			if (i != txcal_meas_legacy->valid_cnt)	{
				printf("Incorrect Number of Entries. Expected %d, Entered %d\n",
					txcal_meas_legacy->valid_cnt, i);
				err = BCME_USAGE_ERROR;
				goto fail;
			}
			err = wlu_var_setbuf(wl, cmd->name, pwr, WLC_TXCORE_MAX_OLD*
					MAX_NUM_TXCAL_MEAS*sizeof(pwr[0][0]));
		} else {
			txcal_meas->version = TXCAL_IOVAR_VERSION;
			if ((err = wlu_var_getbuf(wl, cmd->name, txcal_meas, buf_size, &ptr)) < 0)
				goto fail;
			txcal_meas_old = (wl_txcal_meas_ncore_t *)ptr;

			if (txcal_meas_old->version != TXCAL_IOVAR_VERSION) {
				printf("Version mismatch %d \n", txcal_meas_old->version);
				err = BCME_UNSUPPORTED;
				goto fail;
			}
			/* 	wl support for new txcal structures
			 *  check for max core support from dongle
			 */
			if (core > txcal_meas_old->num_core - 1) {
				printf("Dongle supports only %d  cores \n"
					"Allowed range 0 - %d \n", txcal_meas_old->num_core,
					txcal_meas_old->num_core - 1);
				err = BCME_USAGE_ERROR;
				goto fail;
			}
			/* Initialize set structure with fw copy to start with */
			memcpy(txcal_meas, txcal_meas_old, buf_size);

			/* Get to per core info */
			per_core = txcal_meas->txcal_percore;
			per_core += core;
			do {
				if (i >= MAX_NUM_TXCAL_MEAS) {
					printf("Entries exceeded max allowed\n");
					err = BCME_USAGE_ERROR;
					goto fail;
				}
				per_core->pwr[i] = strtoul(*argv, NULL, 0);
				i++;
			} while (*++argv);
			if (i != txcal_meas_old->valid_cnt)	{
				printf("Incorrect Number of Entries. Expected %d, Entered %d\n",
					txcal_meas_old->valid_cnt, i);
				err = BCME_USAGE_ERROR;
				goto fail;
			}
			/* Add magic seq number */
			txcal_meas->version = TXCAL_IOVAR_VERSION;
			err = wlu_var_setbuf(wl, cmd->name, txcal_meas, buf_size);
		}
	} else {

		/* Check for version */
		version_err = wlu_var_getbuf_sm(wl, "txcal_ver", NULL, 0, &ptr);
		if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
			/* support for old firmware with old txcal structures */
			if ((err = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, &ptr)) < 0)
			goto fail;
			txcal_meas_legacy = (wl_txcal_meas_old_t *)ptr;
			for (core = 0; core < WLC_TXCORE_MAX_OLD; core++) {
				printf("CORE%d\tTSSI\t\tPWR\n", core);
				for (i = 0; i < txcal_meas_legacy->valid_cnt; i++)
					printf("\t%d\t\t%d\n", txcal_meas_legacy->tssi[core][i],
					        txcal_meas_legacy->pwr[core][i]);
			}
		} else {
			txcal_meas->version = TXCAL_IOVAR_VERSION;
			if ((err = wlu_var_getbuf(wl, cmd->name, txcal_meas, buf_size, &ptr)) < 0)
				goto fail;
			if (txcal_meas->version != TXCAL_IOVAR_VERSION) {
				printf("version %d unsupported \n", txcal_meas->version);
				err = BCME_UNSUPPORTED;
				goto fail;
			}
		   /* support for new firmware with new txcal structures */
			txcal_meas = (wl_txcal_meas_ncore_t *)ptr;
			per_core = txcal_meas->txcal_percore;
			/* Dump the info */
			for (core = 0; core < txcal_meas->num_core; core++) {
				printf("CORE%d\tTSSI\t\tPWR\n", core);
				for (i = 0; i < txcal_meas->valid_cnt; i++) {
					printf("\t%d\t\t%d\n", per_core->tssi[i],
					        per_core->pwr[i]);
				}
				per_core++;
			}
		}
	}

fail:
	if (buf) {
		free(buf);
	}

	return err;
}

static int
wl_txcal_gainsweep(void *wl, cmd_t *cmd, char **argv)
{
	wl_txcal_params_t txcal_params;
	uint8 gidx_start, gidx_stop;
	int8 gidx_step;
	char *endptr = NULL;
	char *gidx_str;
	int ret;

	memset(&txcal_params, 0, sizeof(txcal_params));

	if (!*++argv)
		return BCME_USAGE_ERROR;
	if (!wl_ether_atoe(*argv, (struct ether_addr *)&txcal_params.pkteng.dest))
		return BCME_USAGE_ERROR;
	if (!*++argv)
		return BCME_USAGE_ERROR;
	txcal_params.pkteng.delay = strtoul(*argv, NULL, 0);
	if (!*++argv)
		return BCME_USAGE_ERROR;
	txcal_params.pkteng.length = strtoul(*argv, NULL, 0);
	if (!*++argv)
		return BCME_USAGE_ERROR;
	txcal_params.pkteng.nframes = strtoul(*argv, NULL, 0);
	if (txcal_params.pkteng.nframes == 0)
		txcal_params.pkteng.nframes = 4;

	txcal_params.pkteng.flags = WL_PKTENG_PER_TX_START;
	txcal_params.pkteng.flags |= WL_PKTENG_SYNCHRONOUS;

	txcal_params.pkteng.flags = htod32(txcal_params.pkteng.flags);
	txcal_params.pkteng.delay = htod32(txcal_params.pkteng.delay);
	txcal_params.pkteng.nframes = htod32(txcal_params.pkteng.nframes);
	txcal_params.pkteng.length = htod32(txcal_params.pkteng.length);

	/* read gidx start */
	if (!*++argv)
		return BCME_USAGE_ERROR;
	gidx_str = *argv;
	gidx_start = strtoul(gidx_str, &endptr, 10);
	if (*endptr == ':') {
		endptr++;
		gidx_str = endptr;
	} else {
		return BCME_USAGE_ERROR;
	}

	/* read gidx step */
	gidx_step = strtoul(gidx_str, &endptr, 10);
	if (*endptr == ':') {
		endptr++;
		gidx_str = endptr;
	} else {
		return BCME_USAGE_ERROR;
	}
	if (gidx_step == 0)
		return BCME_USAGE_ERROR;

	/* read gidx stop */
	gidx_stop = strtoul(gidx_str, &endptr, 10);
	if ((*endptr != '\0') && (*endptr != '\n') && (*endptr != ' '))
		return BCME_USAGE_ERROR;

	txcal_params.gidx_start = gidx_start;
	txcal_params.gidx_step = gidx_step;
	txcal_params.gidx_stop = gidx_stop;

	ret = (wlu_var_setbuf(wl, cmd->name, &txcal_params, sizeof(txcal_params)));

	return ret;
}

int8
wl_txcal_mode(void *wl)
{
	int err = -1;
	char *cmd_name = "nrate";
	uint32 val = 0;
	int8 mode = 1;
	if ((err = wlu_iovar_getint(wl, cmd_name, (int*)&val)) < 0)
		return err;

	if (val == 0) {
		mode = -1;		/* invalid mode */
	} else {
		uint8 is_legacy = ((val & WL_RSPEC_ENCODING_MASK) == 0);
		uint rate = (val & WL_RSPEC_RATE_MASK);
		if (is_legacy && ((rate == 2) || (rate == 4) ||
				(rate == 11) || (rate == 22))) {
			mode = 0;	/* BPHY mode */
		} else {
			mode = 1;	/* OFDM mode */
		}
	}
	return mode;
}

/* wl txcal_pwr_tssi_tbl */
static int
wl_txcal_pwr_tssi_tbl(void *wl, cmd_t *cmd, char **argv)
{
	wl_txcal_power_tssi_ncore_t * txcal_tssi;
	wl_txcal_power_tssi_ncore_t * txcal_tssi_old;
	wl_txcal_power_tssi_percore_t * per_core;
	wl_txcal_power_tssi_old_t txcal_pwr_tssi;
	wl_txcal_power_tssi_old_t *txcal_pwr_tssi_ptr;

	/* Total buffer size to be allocated */
	uint16 buf_size = OFFSETOF(wl_txcal_power_tssi_ncore_t, tssi_percore) +
		WLC_TXCORE_MAX* sizeof(wl_txcal_power_tssi_percore_t);

	void *ptr = NULL;
	int err  = BCME_OK;
	int version_err = BCME_OK;
	uint8 i, core = 0;
	uint8 channel = 0;
	void * buf = NULL;
	int16 pwr_start = 0;
	uint8 num_entries = 0;
	int8 mode = 1;

	/* detect the current mode for txcal */
	mode = wl_txcal_mode(wl);
	if ((mode < 0) || (mode > 1)) {
		printf("BPHY or OFDM rate needs to be specified properly!");
		goto fail;
	}

	/* Allocate buffer for set iovar */
	buf = CALLOC(buf_size);
	if (buf == NULL) {
		err = BCME_NOMEM;
		goto fail;
	}

	/* copy older values */
	txcal_tssi = (wl_txcal_power_tssi_ncore_t *)buf;

	/* core info */
	if (!(*++argv)) {
		err = BCME_USAGE_ERROR;
		goto fail;
	}
	core = strtoul(*argv, NULL, 0);

	if (core > (WLC_TXCORE_MAX- 1)) {
		err = BCME_USAGE_ERROR;
		goto fail;
	}

	/* channle info */
	if (!(*++argv)) {
		err = BCME_USAGE_ERROR;
		goto fail;
	}
	channel = strtoul(*argv, NULL, 0);

	if (!(*++argv)) {		/* Get */

		version_err = wlu_var_getbuf_sm(wl, "txcal_ver", NULL, 0, &ptr);
		if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
			/* support for firmware with old txcal structures */
			if ((err = wlu_var_getbuf_med(wl, cmd->name, &channel,
				sizeof(uint8), &ptr)) < 0)
		   goto fail;
			txcal_pwr_tssi_ptr = (wl_txcal_power_tssi_old_t *)ptr;
			printf("CORE %d\n", core);
			printf("\tChannel = %d\n", txcal_pwr_tssi_ptr->channel);
			printf("\tStarting Power = %d\n", txcal_pwr_tssi_ptr->pwr_start[core]);
			printf("\tNum of Entries = %d\n", txcal_pwr_tssi_ptr->num_entries[core]);
			printf("\tTSSI values:\n");
			for (i = 0; i < txcal_pwr_tssi_ptr->num_entries[core]; i++)
				printf("\t%d\n", txcal_pwr_tssi_ptr->tssi[core][i]);

			goto fail;
		} else {
			txcal_tssi->version = TXCAL_IOVAR_VERSION;
			txcal_tssi->channel = channel;
			txcal_tssi->ofdm = mode;
			txcal_tssi->set_core = core;

			if ((err = wlu_var_getbuf(wl, cmd->name, txcal_tssi, buf_size, &ptr)) < 0)
				goto fail;
			txcal_tssi = (wl_txcal_power_tssi_ncore_t *)ptr;

			/* checking txcal version */
			if (txcal_tssi->version != TXCAL_IOVAR_VERSION) {
				printf("version %d unsupported \n", txcal_tssi->version);
				err = BCME_UNSUPPORTED;
				goto fail;
			}

			/* support for firmware with new txcal structures
			 * check for max core support from dongle
			 */
			if (core > txcal_tssi->num_core - 1) {
				printf("Dongle supports only %d cores \n"
					"Allowed range 0 - %d \n", txcal_tssi->num_core,
					txcal_tssi->num_core - 1);
				err = BCME_USAGE_ERROR;
				goto fail;
			}
			/* per core pointer */
			per_core = txcal_tssi->tssi_percore;

			/* Move to requested core */
			per_core += core;
			printf("CORE %d\n", core);
			printf("\tChannel = %d\n", txcal_tssi->channel);
			if (txcal_tssi->ofdm)
				printf("\tMode = OFDM\n");
			else
				printf("\tMode = BPHY\n");
			printf("\tStarting Power = %d\n", per_core->pwr_start);
			printf("\tNum of Entries = %d\n", per_core->num_entries);
			printf("\tTSSI values:\n");
			for (i = 0; i < per_core->num_entries; i++)
				printf("\t%d\n", per_core->tssi[i]);

			goto fail;
		}
	} else {
		argv = argv - 2;
	}
	if (*++argv) {
		pwr_start = strtol(*argv, NULL, 0);
	} else {
		err = BCME_USAGE_ERROR;
		goto fail;
	}

	if (*++argv) {
		num_entries = strtoul(*argv, NULL, 0);
	} else {
		err = BCME_USAGE_ERROR;
		goto fail;
	}
	if (num_entries >= MAX_NUM_PWR_STEP) {
		printf("Entries exceeded max allowed\n");
		err = -1;
		goto fail;
	}

	if (*++argv) {
		channel = strtoul(*argv, NULL, 0);
	} else {
		err = BCME_USAGE_ERROR;
		goto fail;
	}

	version_err = wlu_var_getbuf_sm(wl, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if ((err = wlu_var_getbuf_med(wl, cmd->name, &channel, sizeof(uint8), &ptr)) < 0)
		goto fail;
	    txcal_pwr_tssi_ptr = ptr;
		txcal_pwr_tssi = *txcal_pwr_tssi_ptr;
		txcal_pwr_tssi.channel = channel;
		txcal_pwr_tssi.set_core = core;
		txcal_pwr_tssi.pwr_start[core] = pwr_start;
		txcal_pwr_tssi.num_entries[core] = num_entries;

		if (*++argv) { /* Set */
			memset(txcal_pwr_tssi.tssi[core], 0,
			        MAX_NUM_PWR_STEP*sizeof(txcal_pwr_tssi.tssi[0][0]));
			i = 0;
			do {
				if (i >= MAX_NUM_PWR_STEP) {
					printf("Entries exceeded max allowed\n");
					err = -1;
					goto fail;
				}
				txcal_pwr_tssi.tssi[core][i] = strtoul(*argv, NULL, 0);
				i++;
			} while (*++argv);
			if (i != txcal_pwr_tssi.num_entries[core]) {
				printf("Incorrect Number of Entries. Expected %d, Entered %d\n",
					txcal_pwr_tssi.num_entries[core], i);
				err = -1;
				goto fail;
			}
			txcal_pwr_tssi.gen_tbl = 0;

			if ((err = wlu_var_setbuf(wl, cmd->name, &txcal_pwr_tssi,
			        sizeof(txcal_pwr_tssi))) < 0)
				goto fail;
		} else { /* Generate */
			txcal_pwr_tssi.gen_tbl = 1;
			err = wlu_var_setbuf(wl, cmd->name, &txcal_pwr_tssi,
					sizeof(txcal_pwr_tssi));
			if ((err = wlu_var_getbuf_med(wl, cmd->name, &channel,
					sizeof(uint8), &ptr)) < 0)
				goto fail;
			txcal_pwr_tssi_ptr = ptr;
			printf("CORE %d\n", core);
			printf("\tChannel = %d\n", txcal_pwr_tssi_ptr->channel);
			printf("Starting Power = %d\n", txcal_pwr_tssi_ptr->pwr_start[core]);
			printf("Num of Entries = %d\n", txcal_pwr_tssi_ptr->num_entries[core]);
			printf("TSSI values:\n");
			for (i = 0; i < txcal_pwr_tssi_ptr->num_entries[core]; i++)
				printf("%d\n", txcal_pwr_tssi_ptr->tssi[core][i]);
		}

	} else {

		txcal_tssi->version = TXCAL_IOVAR_VERSION;
		txcal_tssi->channel = channel;
		txcal_tssi->ofdm = mode;
		if ((err = wlu_var_getbuf(wl, cmd->name, txcal_tssi, buf_size, &ptr)) < 0)
			goto fail;

		/* Current copy form fw */
		txcal_tssi_old = (wl_txcal_power_tssi_ncore_t *)ptr;

		if (txcal_tssi_old->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", txcal_tssi_old->version);
			err = BCME_UNSUPPORTED;
			goto fail;
		}
		/* support for firmware with new txcal structures */
		/* check for max core support from dongle */
		if (core > txcal_tssi_old->num_core - 1) {
			printf("Dongle supports only %d  cores \n"
				"Allowed range 0 - %d\n", txcal_tssi_old->num_core,
				txcal_tssi_old->num_core - 1);
			err = BCME_USAGE_ERROR;
			goto fail;
		}

		memcpy(txcal_tssi, txcal_tssi_old, buf_size);

		/* Update user input values */
		txcal_tssi->channel = channel;
		txcal_tssi->set_core = core;
		txcal_tssi->ofdm = mode;

		/* Move to requested core */
		per_core = txcal_tssi->tssi_percore;
		per_core += core;

		/* Update per core info */
		per_core->pwr_start = pwr_start;
		per_core->num_entries = num_entries;

		if (*++argv) { /* Set */
			memset(per_core->tssi, 0, MAX_NUM_PWR_STEP * sizeof(per_core->tssi[0]));
			i = 0;
			do {
				if (i >= MAX_NUM_PWR_STEP) {
					printf("Entries exceeded max allowed\n");
					err = -1;
					goto fail;
				}
				per_core->tssi[i] = strtoul(*argv, NULL, 0);
				i++;
			} while (*++argv);
			if (i != num_entries) {
				printf("Incorrect Number of Entries. Expected %d, Entered %d\n",
					num_entries, i);
				err = -1;
				goto fail;
			}
			txcal_tssi->gen_tbl = 0;
			txcal_tssi->version = TXCAL_IOVAR_VERSION;
			if ((err = wlu_var_setbuf(wl, cmd->name, txcal_tssi,
			        buf_size)) < 0)
				goto fail;
		} else { /* Generate */
			txcal_tssi->gen_tbl = 1;
			txcal_tssi->version = TXCAL_IOVAR_VERSION;
			txcal_tssi->channel = channel;
			err = wlu_var_setbuf(wl, cmd->name, txcal_tssi, buf_size);

			txcal_tssi->version = TXCAL_IOVAR_VERSION;
			txcal_tssi->channel = channel;

			if ((err = wlu_var_getbuf(wl, cmd->name, txcal_tssi, buf_size, &ptr)) < 0)
				goto fail;

			txcal_tssi = (wl_txcal_power_tssi_ncore_t *)ptr;

			/* Move to requested core */
			per_core = txcal_tssi->tssi_percore;
			per_core += core;

			printf("CORE %d\n", core);
			printf("\tChannel = %d\n", txcal_tssi->channel);
			if (txcal_tssi->ofdm)
				printf("\tMode = OFDM\n");
			else
				printf("\tMode = BPHY\n");
			printf("Starting Power = %d\n", per_core->pwr_start);
			printf("Num of Entries = %d\n", per_core->num_entries);
			printf("TSSI values:\n");
			for (i = 0; i < per_core->num_entries; i++)
				printf("%d\n", per_core->tssi[i]);
		}
	}

fail:
	if (buf)
		free(buf);
	return err;
}
static int
wl_olpc_anchoridx(void *wl, cmd_t *cmd, char **argv)
{
	wl_txcal_power_tssi_old_t txcal_pwr_tssi;
	wl_txcal_power_tssi_old_t *txcal_pwr_tssi_ptr;
	wl_olpc_pwr_t *olpc_pwr_ptr, olpc_pwr;
	void *ptr = NULL;
	int err = 0;
	int version_err = BCME_OK;
	uint8 core = 0;
	uint8 channel = 0;
	int16 tempsense = 0;
	uint8 olpc_idx = 0;
	uint16 buf_size = sizeof(wl_olpc_pwr_t);
	if (!(*++argv)) {
		return BCME_USAGE_ERROR;
	}
	core = strtoul(*argv, NULL, 0);
	if (!(*++argv)) {
		return BCME_USAGE_ERROR;
	}
	channel = strtoul(*argv, NULL, 0);
	olpc_pwr_ptr = &olpc_pwr;
	olpc_pwr_ptr->core = core;
	olpc_pwr_ptr->channel = channel;
	olpc_pwr_ptr->version = TXCAL_IOVAR_VERSION;

	if (!(*++argv)) {		/* Get */
		version_err = wlu_var_getbuf_sm(wl, "txcal_ver", NULL, 0, &ptr);
		if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
			/* support for firmware with old txcal structures */
			if ((err = wlu_var_getbuf_med(wl, cmd->name, &channel,
				sizeof(uint8), &ptr)) < 0)
				 return err;
			txcal_pwr_tssi_ptr = ptr;
			printf("CORE %d\n", core);
			printf("\tChannel = %d\n", txcal_pwr_tssi_ptr->channel);
			printf("\tTemperature = %d\n", txcal_pwr_tssi_ptr->tempsense[core]);
			printf("\tTx pwr idx at anchor power is %d\n",
				txcal_pwr_tssi_ptr->pwr_start_idx[core]);
			return err;
		} else {
			if ((err = wlu_var_getbuf_sm(wl, cmd->name, olpc_pwr_ptr,
				buf_size, &ptr)) < 0)
				return err;
			olpc_pwr_ptr = (wl_olpc_pwr_t *)ptr;
			/* Checking for txcal version */
			if (olpc_pwr_ptr->version != TXCAL_IOVAR_VERSION) {
				printf("version %d unsupported \n", olpc_pwr_ptr->version);
				err = BCME_UNSUPPORTED;
				return err;
			}
			/* support for firmware with new txcal structures */
			printf("CORE %d\n", core);
			printf("\tChannel = %d\n", olpc_pwr_ptr->channel);
			printf("\tTemperature = %d\n", olpc_pwr_ptr->tempsense);
			printf("\tTx pwr idx at anchor power is %d\n",
				olpc_pwr_ptr->olpc_idx);
			return err;
		}
	} else {
		olpc_idx = strtoul(*argv, NULL, 0);
	}
	if (!(*++argv))
		return BCME_USAGE_ERROR;

	tempsense = strtoul(*argv, NULL, 0);

	if (*++argv)
		return BCME_USAGE_ERROR;

	version_err = wlu_var_getbuf_sm(wl, "txcal_ver", NULL, 0, &ptr);
	if ((version_err != BCME_OK) || (*(int*)ptr == 0)) {
		/* support for firmware with old txcal structures */
		if ((err = wlu_var_getbuf_med(wl, cmd->name, &channel, sizeof(uint8), &ptr)) < 0)
			return err;
		txcal_pwr_tssi_ptr = ptr;
		txcal_pwr_tssi = *txcal_pwr_tssi_ptr;
		txcal_pwr_tssi.channel = channel;
		txcal_pwr_tssi.tempsense[core] = tempsense;
		txcal_pwr_tssi.set_core = core;
		txcal_pwr_tssi.pwr_start_idx[core] = olpc_idx;
		if ((err = wlu_var_setbuf(wl, cmd->name, &txcal_pwr_tssi,
			sizeof(txcal_pwr_tssi))) < 0)
			return err;
	} else {
		/* Checking for txcal version */
		if ((err = wlu_var_getbuf_sm(wl, cmd->name, olpc_pwr_ptr, buf_size, &ptr)) < 0)
			return err;
		txcal_pwr_tssi_ptr = ptr;
		if (olpc_pwr_ptr->version != TXCAL_IOVAR_VERSION) {
			printf("version %d unsupported \n", olpc_pwr_ptr->version);
			err = BCME_UNSUPPORTED;
			return err;
		}
	    /* support for firmware with new txcal structures */
		olpc_pwr_ptr = &olpc_pwr;
		olpc_pwr_ptr->channel = channel;
		olpc_pwr_ptr->tempsense = tempsense;
		olpc_pwr_ptr->core = core;
		olpc_pwr_ptr->olpc_idx = olpc_idx;
		olpc_pwr_ptr->version = TXCAL_IOVAR_VERSION;

		if ((err = wlu_var_setbuf_sm(wl, cmd->name, olpc_pwr_ptr, buf_size)) < 0)
			return err;
	}
	return err;
}

static int
wl_read_estpwrlut(void *wl, cmd_t *cmd, char **argv)
{
	uint16 *estpwrlut;
	void *ptr = NULL;
	int err;
	uint8 i;
	int val;
	char* endptr = NULL;

	argv++;

	if (!*argv)
		return BCME_USAGE_ERROR;

	val = htod32(strtol(*argv, &endptr, 0));
	if (*endptr != '\0') {
		/* not all the value string was parsed by strtol */
		printf("set: error parsing value \"%s\" as an integer\n", *argv);
		return BCME_USAGE_ERROR;
	}

	if ((err = wlu_var_getbuf_med(wl, cmd->name, &val, sizeof(val), &ptr)) < 0)
		return err;
	estpwrlut = ptr;
	printf("ESTPWR LUT FOR CORE %d\n", val);
	for (i = 0; i < 128; i++)
		/* this change is to print out the estpwrlut correctly
		 * even the table width is expanded to 24bits
		 */
		printf("0x%x\n", estpwrlut[i]);
	return err;
}

static int
wl_olpc_offset(void *wl, cmd_t *cmd, char **argv)
{
	void *ptr = NULL;
	int err = 0;
	int8 olpc_offset[5];
	int8 *olpc_offset_ptr;
	uint8 i = 0;

	if (!(*++argv)) {
		if ((err = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, &ptr)) < 0) {
			return err;
		}
		olpc_offset_ptr = ptr;
		printf("OLPC offset for 2G: %d\n", olpc_offset_ptr[0]);
		printf("OLPC offset for 5G low/mid/high/x1: %d %d %d %d\n", olpc_offset_ptr[1],
		       olpc_offset_ptr[2], olpc_offset_ptr[3], olpc_offset_ptr[4]);
	} else {
		do {
			if (i > 4) {
				return BCME_USAGE_ERROR;
			}

			olpc_offset[i] = strtol(*argv, NULL, 0);
			i++;
		} while (*++argv);

		/* Expect 5 entries */
		if (i != 5) {
			return BCME_USAGE_ERROR;
		}

		if ((err = wlu_var_setbuf(wl, cmd->name, olpc_offset, 5*sizeof(int8))) < 0) {
			printf("Unable to set olpc_offsets.\n");
			return BCME_ERROR;
		}
	}
	return err;
}

static int
wl_btcoex_desense_rxgain(void *wl, cmd_t *cmd, char **argv)
{
	wl_desense_restage_gain_t desense_restage_gain;
	wl_desense_restage_gain_t *desense_restage_gain_ptr;
	uint8 num_cores = 0, i;
	uint32 num_params = 0;
	int err = 0;
	void *ptr = NULL;

	memset(&desense_restage_gain, 0, sizeof(desense_restage_gain));
	desense_restage_gain.version =  0;
	desense_restage_gain.length = sizeof(desense_restage_gain);

	if (!(*++argv)) {		/* Get */
		if ((err = wlu_var_getbuf_med(wl, cmd->name, NULL, 0, &ptr)) < 0)
			return err;

		desense_restage_gain_ptr = ptr;

		if (desense_restage_gain_ptr->band == WLC_BAND_2G)
				printf("Band: 2G\n");

		if (desense_restage_gain_ptr->band == WLC_BAND_5G)
				printf("Band: 5G\n");

		printf("# of cores: %d\n", desense_restage_gain_ptr->num_cores);

		for (i = 0; i < desense_restage_gain_ptr->num_cores; i++)
				printf("Desense for core[%d] = %d \n",
					i, desense_restage_gain_ptr->desense_array[i]);

		return BCME_OK;
	}

	/* Set */
	if (strcmp(*argv, "b") == 0) {
		desense_restage_gain.band = WLC_BAND_2G;
	} else if (strcmp(*argv, "a") == 0) {
		desense_restage_gain.band = WLC_BAND_5G;
	} else {
		return BCME_USAGE_ERROR;
	}

	++argv;
	if (*argv == NULL) {
		return BCME_USAGE_ERROR;
	}
	num_cores = strtoul(*argv, NULL, 0);

	if (num_cores > WL_TX_CHAINS_MAX) {
		printf("Number of cores %d greater than max value %d\n",
				num_cores, WL_TX_CHAINS_MAX);
		return BCME_USAGE_ERROR;
	}

	desense_restage_gain.num_cores = num_cores;

	++argv;
	while ((argv[num_params] != NULL) &&
		(num_params < num_cores)) {
		desense_restage_gain.desense_array[num_params] =
			strtoul(argv[num_params], NULL, 0);
		num_params++;
	}

	if (num_params != num_cores || (argv[num_params] != NULL)) {
		printf("Number of parameters(%d) not matching number of cores(%d)\n",
			num_params, num_cores);
		return BCME_USAGE_ERROR;
	}

	return (wlu_var_setbuf(wl, cmd->name, &desense_restage_gain, sizeof(desense_restage_gain)));
}

#define FILE_NAME_LEN_MAX 50
/*
* Base file name length.
* strlen("radio_dump.txt") = 14
*/
#define BASE_FILE_NAME_LEN 14
#define HEADER_LEN_MAX (FILE_NAME_LEN_MAX - BASE_FILE_NAME_LEN)

static int
wl_dbg_regval(void *wl, cmd_t *cmd, char **argv)
{
	char *buff = NULL;
	char *ptr = NULL;
	char *tmp_buffer = NULL;
	char *tmp_buffer_iter = NULL;
	uint tmp_buffer_space_avail = 0;
	char *prefix = NULL;

	/* Add 1 byte for null termiantion. */
	char file_name[FILE_NAME_LEN_MAX + 1];
	FILE *fptr = NULL;
	int err = BCME_OK;
	wl_regval_capture_args_t *capture_args;
	uint32 capture_args_size = 0;
	uint8 dump_type;
	int types [] = {WLC_REGVAL_DUMP_PHYREG, WLC_REGVAL_DUMP_RADREG};
	int len = ARRAYSIZE(types);
	int i;
	int char_count = 0;
	uint num_chars_to_write = 0;
	uint num_chars_written = 0;

	memset(file_name, 0, FILE_NAME_LEN_MAX);
	capture_args_size = sizeof(*capture_args);

	for (i = 0; i < len; i++) {
		dump_type = types[i];
		wlu_iovar_setint(wl, cmd->name, dump_type);
		if (argv[1]) {
			prefix = argv[1];
			char_count = snprintf(file_name,
				FILE_NAME_LEN_MAX, "%s_", prefix);
			if (char_count < 0) {
				printf("Error while generating the output file.\n");
				return BCME_ERROR;
			} else if (char_count >= HEADER_LEN_MAX) {
				printf("ERROR: file name too long.\n");
				printf("Max allowed len: %d\n", (HEADER_LEN_MAX - 2));
				return BCME_USAGE_ERROR;
			}
		}

		if (dump_type == WLC_REGVAL_DUMP_PHYREG) {
			strncat(file_name, "phy_dump.txt",
				(FILE_NAME_LEN_MAX - char_count));
		} else {
			strncat(file_name, "radio_dump.txt",
				(FILE_NAME_LEN_MAX - char_count));
		}

		fptr = fopen(file_name, "w");
		if (fptr == NULL) {
			printf("Error: unable to open file: %s\n", file_name);
			return BCME_NORESOURCE;
		}

		buff = calloc(PHYREGVAL_CAPTURE_BUFFER_LEN + capture_args_size, sizeof(*buff));
		tmp_buffer = calloc(WL_DUMP_BUF_LEN, sizeof(*tmp_buffer));
		if ((buff == NULL) || (tmp_buffer == NULL)) {
			if (buff != NULL) {
				free(buff);
			}
			if (tmp_buffer != NULL) {
				free(tmp_buffer);
			}
			fclose(fptr);
			return BCME_NOMEM;
		} else {
			tmp_buffer_iter = tmp_buffer;
			tmp_buffer_space_avail = WL_DUMP_BUF_LEN;
		}

		do {
			if ((err = wlu_iovar_getbuf(wl, cmd->name,
				buff, 0, buff,
				PHYREGVAL_CAPTURE_BUFFER_LEN + capture_args_size)) < 0)
			{
				fclose(fptr);
				free(buff);
				free(tmp_buffer);
				goto exit;
			}

			/* Extract the header. */
			capture_args = (wl_regval_capture_args_t *) buff;

			/* Extract the data. */
			ptr = (buff + capture_args_size);

			/* Check if enough space is available for print. */
			if (tmp_buffer_space_avail < strlen(ptr)) {
				/* Breakout if the available buffer space won't fit the data. */
				printf("ERROR: No more buffer space available for print. \n");
				break;
			}
			char_count = sprintf(tmp_buffer_iter, "%s", ptr);
			tmp_buffer_iter += char_count;

			/* Reduce the available buffer space by num char printed. */
			tmp_buffer_space_avail -= char_count;
		} while (capture_args->control_flag == WLC_REGVAL_MORE_DATA);

		num_chars_to_write = tmp_buffer_iter - tmp_buffer;
		num_chars_written = fwrite(tmp_buffer,
			sizeof(*tmp_buffer), num_chars_to_write, fptr);

		if (num_chars_written != num_chars_to_write) {
			/* IO error. Cleanup and exit. */
			fclose(fptr);
			free(buff);
			free(tmp_buffer);
			printf("Error writing %d bytes to file %s\n",
				num_chars_to_write, file_name);
			err = BCME_ERROR;
		}

		if (err) {
			break;
		}

		fclose(fptr);
		free(buff);
		free(tmp_buffer);
		memset(file_name, 0, FILE_NAME_LEN_MAX);
	} /* End for */

exit:
	return err;
}

static int
wl_phy_rxiqcal_gtbl(void *wl, cmd_t *cmd, char **argv)
{
	uint argc;
	int8 en, start_idx = -1, end_idx = -1;
	uint32 value, cmd_val;
	char *endptr;
	int err = BCME_OK;

	for (argc = 0; argv[argc]; argc++);
	argc--;

	if (argc == 0) {
		wlu_iovar_getint(wl, cmd->name, (int *)&value);
		en = value & 1;
		start_idx = (value >> 8) & 0xff;
		end_idx = (value >> 16) & 0xff;
		printf("en = %d, start_idx = %d, end_idx = %d\n", en, start_idx, end_idx);
	} else if ((argc == 1) || (argc == 3)) {
		en = strtol(argv[1], &endptr, 0);
		if (*endptr != '\0') {
			printf("error\n");
			return BCME_USAGE_ERROR;
		}

		if (argc > 1) {
			start_idx = strtol(argv[2], &endptr, 0);
			end_idx = strtol(argv[3], &endptr, 0);
			if (end_idx < start_idx) {
				printf("End index cannot be greater than start index\n");
				return BCME_USAGE_ERROR;
			}
		}

		cmd_val = (end_idx << 16) | (start_idx << 8) | en;
		err = wlu_iovar_setint(wl, cmd->name, cmd_val);
	} else {
		err = BCME_USAGE_ERROR;
	}

	return err;
}

#ifdef WL_EAP_NOISE_MEASUREMENTS
static int
wl_phy_noise_bias_iov(void *wl, cmd_t *cmd, char **argv,
	wl_phynoisebias_band_t band, wl_phynoisebias_gain_t gain)
{
	uint argc;
	char *endptr;
	int err = BCME_OK;
	wl_phynoisebias_iov_t nb, *pnb;
	int i;
	static const char * const override_name = "_phynoisebias";

	for (argc = 0; argv[argc]; argc++);
	argc--;

	/* To reduce redundency in phy code, override the specific
	 * phy noise bias command names (e.g., phynoisebias2ghi) with a generic
	 * IOVAR command name (_phynoisebias), distiguished by 'band' parameter
	 */
	 cmd->name = override_name;

	if (argc == 0) {  /* Get */
		/* request offsets used by this band */
		nb.band = band;
		nb.gain	= gain;
		pnb = &nb;
		err = wlu_iovar_getbuf(wl, cmd->name, &nb, sizeof(wl_phynoisebias_iov_t),
			buf, WLC_IOCTL_MAXLEN);
		if (err == BCME_OK) {
			/* Support both old and new versions (single and four UNII values) */
			pnb = (wl_phynoisebias_iov_t *)buf;
			if (pnb->dB_offsets.count != 1 && pnb->dB_offsets.count != 4) {
				pnb->dB_offsets.count = 1;
			}
			for (i = 0; (i < pnb->dB_offsets.count) && (i < WL_NUM_NOISE_BIASES); i++) {
				printf("%d ", pnb->dB_offsets.dBs[i]);
			}
			printf("\n");
		}
	} else { /* Set */
		nb.band = band;
		nb.gain	= gain;
		nb.dB_offsets.count = 0;
		if (argc <= WL_NUM_NOISE_BIASES) {
			uint i;
			for (i = 1; i <= argc; i++) {
				nb.dB_offsets.count = i;
				nb.dB_offsets.dBs[i-1] = (int8)strtol(argv[i], &endptr, 0);
				// mixed args?
				if (*endptr != '\0') {
					err = BCME_USAGE_ERROR;
					break;
				}
			} /* for */
		} else {
			err = BCME_USAGE_ERROR;
		}

		if (err == BCME_USAGE_ERROR) {
			printf("Error: Command requires 0, 1, or 4 dB values\n");
		} else {
			err = wlu_iovar_setbuf(wl, cmd->name, &nb, sizeof(wl_phynoisebias_iov_t),
					buf, WLC_IOCTL_MAXLEN);
		}
	}  /* Set */
	return err;
}

static int
wl_phy_noise_bias_2glo(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_2G, WL_PHY_NB_GAIN_LO);
}
static int
wl_phy_noise_bias_2ghi(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_2G, WL_PHY_NB_GAIN_HI);
}
static int
wl_phy_noise_bias_5glo(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_5G, WL_PHY_NB_GAIN_LO);
}
static int
wl_phy_noise_bias_5ghi(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_5G, WL_PHY_NB_GAIN_HI);
}
static int
wl_phy_noise_bias_5g_radar_lo(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_5G_RADAR, WL_PHY_NB_GAIN_LO);
}
static int
wl_phy_noise_bias_5g_radar_hi(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_5G_RADAR, WL_PHY_NB_GAIN_HI);
}
static int
wl_phy_noise_bias_6glo(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_6G, WL_PHY_NB_GAIN_LO);
}
static int
wl_phy_noise_bias_6ghi(void *wl, cmd_t *cmd, char **argv)
{
	return wl_phy_noise_bias_iov(wl, cmd, argv,
		WL_PHY_NB_6G, WL_PHY_NB_GAIN_HI);
}
#endif /* WL_EAP_NOISE_MEASUREMENTS */
