/*
 * Microcode declarations for Broadcom 802.11abg
 * Networking Adapter Device Driver.
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
 * $Id: d11ucode.h 786797 2020-05-08 07:26:13Z $
 */

#ifndef _D11UCODE_H_
#define _D11UCODE_H_
#define CHARS_TO_INT(a, b, c, d)		(d << 24) | (c << 16) | (b << 8) | a

/* ucode and inits structure */
typedef struct d11init {
	uint16	addr;
	uint16	size;
	uint32	value;
} d11init_t;

typedef struct d11axiinit {
	uint32	addr;
	uint32	size;
	uint32	value;
} d11axiinit_t;

extern CONST uint32 d11ucode30_mimo[];
extern CONST uint d11ucode30_mimosz;
extern CONST uint32 d11ucode42[];
extern CONST uint d11ucode42sz;
extern CONST uint32 d11ucode48[];
extern CONST uint d11ucode48sz;
extern CONST uint32 d11ucode49[];
extern CONST uint d11ucode49sz;
extern CONST uint32 d11ucode56[];
extern CONST uint d11ucode56sz;
extern CONST uint32 d11ucode60[];
extern CONST uint d11ucode60sz;
extern CONST uint32 d11ucode61_D11a[];
extern CONST uint d11ucode61_D11asz;
extern CONST uint32 d11ucode61_D11b[];
extern CONST uint d11ucode61_D11bsz;
extern CONST uint32 d11ucode61_5_D11b[];
extern CONST uint d11ucode61_5_D11bsz;
extern CONST uint32 d11ucode62[];
extern CONST uint d11ucode62sz;
extern CONST uint32 d11ucode64[];
extern CONST uint d11ucode64sz;
extern CONST uint32 d11ucode65[];
extern CONST uint d11ucode65sz;
extern CONST uint32 d11ucodex65[];
extern CONST uint d11ucodex65sz;

extern CONST uint32 d11ucode_mu65[];
extern CONST uint d11ucode_mu65sz;
extern CONST uint32 d11ucodex_mu65[];
extern CONST uint d11ucodex_mu65sz;
#ifdef WLCX_ATLAS
extern CONST uint32 d11ucode_wlcx64[];
extern CONST uint d11ucode_wlcx64sz;
#endif /* WLCX_ATLAS */

extern CONST uint32 d11ucode129[]; /* rev129 = 43684b0 */
extern CONST uint d11ucode129sz;
extern CONST uint32 d11ucodex129[];
extern CONST uint d11ucodex129sz;
extern CONST uint32 d11ucode_mu129[];
extern CONST uint d11ucode_mu129sz;
extern CONST uint32 d11ucodex_mu129[];
extern CONST uint d11ucodex_mu129sz;

extern CONST uint32 d11ucode_mu_ftm129[];
extern CONST uint d11ucode_mu_ftm129sz;
extern CONST uint32 d11ucodex_mu_ftm129[];
extern CONST uint d11ucodex_mu_ftm129sz;

extern CONST uint32 d11ucode_btcxmu129_1[]; /* rev129.1 btcx = 43684b1 */
extern CONST uint d11ucode_btcxmu129_1sz;
extern CONST uint32 d11ucodex_btcxmu129_1[];
extern CONST uint d11ucodex_btcxmu129_1sz;

extern CONST uint32 d11ucode129_1[]; /* rev129.1 = 43684b1 */
extern CONST uint d11ucode129_1sz;
extern CONST uint32 d11ucodex129_1[];
extern CONST uint d11ucodex129_1sz;

#ifndef TESTBED_AP_11AX
extern CONST uint32 d11ucode_mu129_1[];
extern CONST uint d11ucode_mu129_1sz;
extern CONST uint32 d11ucodex_mu129_1[];
extern CONST uint d11ucodex_mu129_1sz;
#else
extern CONST uint32 d11ucode_wfa129_1[];
extern CONST uint d11ucode_wfa129_1sz;
extern CONST uint32 d11ucodex_wfa129_1[];
extern CONST uint d11ucodex_wfa129_1sz;
#endif /* TESTBED_AP_11AX */

extern CONST uint32 d11ucode_mu_ftm129_1[];
extern CONST uint d11ucode_mu_ftm129_1sz;
extern CONST uint32 d11ucodex_mu_ftm129_1[];
extern CONST uint d11ucodex_mu_ftm129_1sz;

extern CONST uint32 d11ucode_btcxmu129_2[]; /* rev129.2 btcx = 43684c0 */
extern CONST uint d11ucode_btcxmu129_2sz;
extern CONST uint32 d11ucodex_btcxmu129_2[];
extern CONST uint d11ucodex_btcxmu129_2sz;

extern CONST uint32 d11ucode129_2[]; /* rev129.2 = 43684c0 */
extern CONST uint d11ucode129_2sz;
extern CONST uint32 d11ucodex129_2[];
extern CONST uint d11ucodex129_2sz;

#ifndef TESTBED_AP_11AX
extern CONST uint32 d11ucode_mu129_2[];
extern CONST uint d11ucode_mu129_2sz;
extern CONST uint32 d11ucodex_mu129_2[];
extern CONST uint d11ucodex_mu129_2sz;
#else
extern CONST uint32 d11ucode_wfa129_2[];
extern CONST uint d11ucode_wfa129_2sz;
extern CONST uint32 d11ucodex_wfa129_2[];
extern CONST uint d11ucodex_wfa129_2sz;
#endif /* TESTBED_AP_11AX */

extern CONST uint32 d11ucode_mu_ftm129_2[];
extern CONST uint d11ucode_mu_ftm129_2sz;
extern CONST uint32 d11ucodex_mu_ftm129_2[];
extern CONST uint d11ucodex_mu_ftm129_2sz;

extern CONST uint32 d11ucode_btcxmu130[]; /* rev130 btcx = 63178a0 */
extern CONST uint d11ucode_btcxmu130sz;
extern CONST uint32 d11ucodex_btcxmu130[];
extern CONST uint d11ucodex_btcxmu130sz;

extern CONST uint32 d11ucode_btcx130[]; /* rev130 btcx = 63178a0 */
extern CONST uint d11ucode_btcx130sz;
extern CONST uint32 d11ucodex_btcx130[];
extern CONST uint d11ucodex_btcx130sz;

extern CONST uint32 d11ucode130[]; /* rev130 = 63178a0 */
extern CONST uint d11ucode130sz;
extern CONST uint32 d11ucodex130[];
extern CONST uint d11ucodex130sz;
extern CONST uint32 d11ucode_mu130[];
extern CONST uint d11ucode_mu130sz;
extern CONST uint32 d11ucodex_mu130[];
extern CONST uint d11ucodex_mu130sz;

extern CONST uint32 d11ucode_mu_ftm130[];
extern CONST uint d11ucode_mu_ftm130sz;
extern CONST uint32 d11ucodex_mu_ftm130[];
extern CONST uint d11ucodex_mu_ftm130sz;

extern CONST uint32 d11ucode_btcxmu130_1[]; /* rev130.1 btcx = 63178a2 */
extern CONST uint d11ucode_btcxmu130_1sz;
extern CONST uint32 d11ucodex_btcxmu130_1[];
extern CONST uint d11ucodex_btcxmu130_1sz;

extern CONST uint32 d11ucode_btcx130_1[]; /* rev130.1 btcx = 63178a2 */
extern CONST uint d11ucode_btcx130_1sz;
extern CONST uint32 d11ucodex_btcx130_1[];
extern CONST uint d11ucodex_btcx130_1sz;

extern CONST uint32 d11ucode130_1[]; /* rev130.1 = 63178a2 */
extern CONST uint d11ucode130_1sz;
extern CONST uint32 d11ucodex130_1[];
extern CONST uint d11ucodex130_1sz;
extern CONST uint32 d11ucode_mu130_1[];
extern CONST uint d11ucode_mu130_1sz;
extern CONST uint32 d11ucodex_mu130_1[];
extern CONST uint d11ucodex_mu130_1sz;

extern CONST uint32 d11ucode_mu_ftm130_1[];
extern CONST uint d11ucode_mu_ftm130_1sz;
extern CONST uint32 d11ucodex_mu_ftm130_1[];
extern CONST uint d11ucodex_mu_ftm130_1sz;

extern CONST uint32 d11ucode_mu130_2[];  /* rev130.2 = 6756 / 2x2 .ax 160mhz embedded core */
extern CONST uint d11ucode_mu130_2sz;
extern CONST uint32 d11ucodex_mu130_2[];
extern CONST uint d11ucodex_mu130_2sz;

extern CONST uint32 d11ucode_btcxmu130_2[];
extern CONST uint d11ucode_btcxmu130_2sz;
extern CONST uint32 d11ucodex_btcxmu130_2[];
extern CONST uint d11ucodex_btcxmu130_2sz;

extern CONST uint32 d11ucode_btcx130_2[];
extern CONST uint d11ucode_btcx130_2sz;
extern CONST uint32 d11ucodex_btcx130_2[];
extern CONST uint d11ucodex_btcx130_2sz;

extern CONST uint32 d11ucode_btcxmu131[]; /* rev131 btcx = 6710a0 */
extern CONST uint d11ucode_btcxmu131sz;
extern CONST uint32 d11ucodex_btcxmu131[];
extern CONST uint d11ucodex_btcxmu131sz;

extern CONST uint32 d11ucode_btcxeci131[]; /* rev131 btcx eci = 6710a0 */
extern CONST uint d11ucode_btcxeci131sz;
extern CONST uint32 d11ucodex_btcxeci131[];
extern CONST uint d11ucodex_btcxeci131sz;

extern CONST uint32 d11ucode131[]; /* rev131 = 6710a0 */
extern CONST uint d11ucode131sz;
extern CONST uint32 d11ucodex131[];
extern CONST uint d11ucodex131sz;
extern CONST uint32 d11ucode_mu131[];
extern CONST uint d11ucode_mu131sz;
extern CONST uint32 d11ucodex_mu131[];
extern CONST uint d11ucodex_mu131sz;

extern CONST uint32 d11ucode132[]; /* rev132 = 6715a0 */
extern CONST uint d11ucode132sz;
extern CONST uint32 d11ucodex132[];
extern CONST uint d11ucodex132sz;
extern CONST uint32 d11ucode_mu132[];
extern CONST uint d11ucode_mu132sz;
extern CONST uint32 d11ucodex_mu132[];
extern CONST uint d11ucodex_mu132sz;

extern CONST d11init_t d11n16initvals30[];
extern CONST d11init_t d11n16bsinitvals30[];
extern CONST d11init_t d11n0initvals31[];
extern CONST d11init_t d11n0bsinitvals31[];

extern CONST d11init_t d11ac1initvals42[];
extern CONST d11init_t d11ac1bsinitvals42[];
extern CONST d11init_t d11wakeac1initvals42[];
extern CONST d11init_t d11wakeac1bsinitvals42[];
extern CONST d11init_t d11ac8initvals48[];
extern CONST d11init_t d11ac8bsinitvals48[];
extern CONST d11init_t d11ac9initvals49[];
extern CONST d11init_t d11ac9bsinitvals49[];
extern CONST d11init_t d11ac24initvals56[];
extern CONST d11init_t d11ac24bsinitvals56[];
extern CONST d11init_t d11ac24initvals56core1[];
extern CONST d11init_t d11ac24bsinitvals56core1[];
extern CONST d11init_t d11ulpac36initvals60[];
extern CONST d11init_t d11ulpac36bsinitvals60[];
extern CONST d11init_t d11ac40initvals61_1[];
extern CONST d11init_t d11ac40bsinitvals61_1[];
extern CONST d11init_t d11ac40initvals61_1_D11a[];
extern CONST d11init_t d11ac40bsinitvals61_1_D11a[];
extern CONST d11init_t d11ac128initvals61_5[];
extern CONST d11init_t d11ac128bsinitvals61_5[];
extern CONST d11init_t d11ac32initvals64[];
extern CONST d11init_t d11ac32bsinitvals64[];
extern CONST d11init_t d11ac32initvalsx64[];
extern CONST d11init_t d11ac32bsinitvalsx64[];
extern CONST d11init_t d11ac33initvals65[];
extern CONST d11init_t d11ac33bsinitvals65[];
extern CONST d11init_t d11ac33initvalsx65[];
extern CONST d11init_t d11ac33bsinitvalsx65[];
extern CONST d11init_t d11waken0initvals16[];
extern CONST d11init_t d11waken0bsinitvals16[];
extern CONST d11init_t d11waken0initvals30[];
extern CONST d11init_t d11waken0bsinitvals30[];

extern CONST uint32 d11aeswakeucode16_lp[];
extern CONST uint32 d11aeswakeucode16_sslpn[];
extern CONST uint32 d11aeswakeucode16_mimo[];
extern CONST uint32 d11aeswakeucode30_mimo[];
extern CONST uint32 d11aeswakeucode42[];

extern CONST uint32 d11ucode_wowl16_lp[];
extern CONST uint32 d11ucode_wowl16_sslpn[];
extern CONST uint32 d11ucode_wowl16_mimo[];
extern CONST uint32 d11ucode_wowl30_mimo[];
extern CONST uint32 d11ucode_wowl42[];
extern CONST uint32 d11ucode_ulp46[];
extern CONST uint32 d11ucode_ulp60[];

extern CONST uint d11ucode_wowl16_lpsz;
extern CONST uint d11ucode_wowl16_sslpnsz;
extern CONST uint d11ucode_wowl16_mimosz;
extern CONST uint d11ucode_wowl30_mimosz;
extern CONST uint d11ucode_wowl42sz;
extern CONST uint d11ucode_ulp46sz;
extern CONST uint d11ucode_ulp60sz;

extern CONST uint d11aeswakeucode16_lpsz;
extern CONST uint d11aeswakeucode16_sslpnsz;
extern CONST uint d11aeswakeucode16_mimosz;
extern CONST uint d11aeswakeucode30_mimosz;
extern CONST uint d11aeswakeucode42sz;

#ifdef SAMPLE_COLLECT
extern CONST uint32 d11sampleucode16[];
extern CONST uint d11sampleucode16sz;
#endif // endif

/* BOM info for each ucode file */
extern CONST uint32 d11ucode_ge24_bommajor;
extern CONST uint32 d11ucode_ge24_bomminor;
extern CONST uint32 d11ucode_gt15_bommajor;
extern CONST uint32 d11ucode_gt15_bomminor;
extern CONST uint32 d11ucode_wowl_bommajor;
extern CONST uint32 d11ucode_wowl_bomminor;

#ifdef WLP2P_UCODE
extern CONST uint32 d11ucode_p2p_bommajor;
extern CONST uint32 d11ucode_p2p_bomminor;

extern CONST uint32 d11ucode_p2p30_mimo[];
extern CONST uint d11ucode_p2p30_mimosz;
extern CONST uint32 d11ucode_p2p42[];
extern CONST uint d11ucode_p2p42sz;
extern CONST uint32 d11ucode_p2p48[];
extern CONST uint d11ucode_p2p48sz;
extern CONST uint32 d11ucode_p2p49[];
extern CONST uint d11ucode_p2p49sz;
extern CONST uint32 d11ucode_p2p56[];
extern CONST uint d11ucode_p2p56sz;
extern CONST uint32 d11ucode_p2p60[];
extern CONST uint d11ucode_p2p60sz;
extern CONST uint32 d11ucode_p2p61_D11a[];
extern CONST uint d11ucode_p2p61_D11asz;
extern CONST uint32 d11ucode_p2p61_1_D11a[];
extern CONST uint d11ucode_p2p61_1_D11asz;
extern CONST uint32 d11ucode_p2p61_1_D11b[];
extern CONST uint d11ucode_p2p61_1_D11bsz;
extern CONST uint32 d11ucode_p2p62[];
extern CONST uint d11ucode_p2p62sz;
extern CONST uint32 d11ucode_p2p64[];
extern CONST uint d11ucode_p2p64sz;
extern CONST uint32 d11ucode_p2p65[];
extern CONST uint d11ucode_p2p65sz;
#ifdef UNRELEASEDCHIP
extern CONST uint32 d11ucode_p2p128[];
extern CONST uint d11ucode_p2p128sz;
#endif /* UNRELEASEDCHIP */
#endif /* WLP2P_UCODE */

extern CONST d11init_t d11ac36initvals60[];
extern CONST d11init_t d11ac36bsinitvals60[];
extern CONST uint32 d11ucode_ulp60[];
extern CONST uint d11ucode_ulp60sz;
extern CONST d11init_t d11ulpac36initvals60[];
extern CONST d11init_t d11ulpac36bsinitvals60[];
extern CONST d11axiinit_t d11ac36initvals60_axislave_order[];
extern CONST d11axiinit_t d11ac36initvals60_axislave[];
extern CONST d11axiinit_t d11ac36bsinitvals60_axislave_order[];
extern CONST d11axiinit_t d11ac36bsinitvals60_axislave[];
extern CONST d11axiinit_t d11ulpac36initvals60_axislave_order[];
extern CONST d11axiinit_t d11ulpac36initvals60_axislave[];
extern CONST d11axiinit_t d11ulpac36bsinitvals60_axislave_order[];
extern CONST d11axiinit_t d11ulpac36bsinitvals60_axislave[];

extern CONST d11init_t d11ac36initvals62[];
extern CONST d11init_t d11ac36bsinitvals62[];
extern CONST uint32 d11ucode_ulp62[];
extern CONST uint d11ucode_ulp62sz;
extern CONST d11init_t d11ulpac36initvals62[];
extern CONST d11init_t d11ulpac36bsinitvals62[];
extern CONST d11axiinit_t d11ac36initvals62_axislave_order[];
extern CONST d11axiinit_t d11ac36initvals62_axislave[];
extern CONST d11axiinit_t d11ac36bsinitvals62_axislave_order[];
extern CONST d11axiinit_t d11ac36bsinitvals62_axislave[];
extern CONST d11axiinit_t d11ulpac36initvals62_axislave_order[];
extern CONST d11axiinit_t d11ulpac36initvals62_axislave[];
extern CONST d11axiinit_t d11ulpac36bsinitvals62_axislave_order[];
extern CONST d11axiinit_t d11ulpac36bsinitvals62_axislave[];

extern CONST uint32 d11ucode_p2p80_D11b[];
extern CONST uint32 d11ucode_p2p80_D11a[];
extern CONST uint d11ucode_p2p80_D11bsz;
extern CONST uint d11ucode_p2p80_D11asz;

extern CONST uint32 d11ucode_p2p82_D11b[];
extern CONST uint32 d11ucode_p2p82_D11a[];
extern CONST uint d11ucode_p2p82_D11bsz;
extern CONST uint d11ucode_p2p82_D11asz;

extern CONST d11init_t d11ax44initvals80[];
extern CONST d11init_t d11ax44initvals80_D11a[];

extern CONST d11init_t d11ax44bsinitvals80[];
extern CONST d11init_t d11ax44bsinitvals80_D11a[];

extern CONST d11init_t d11ax44initvals82[];
extern CONST d11init_t d11ax44initvals82_D11a[];

extern CONST d11init_t d11ax44bsinitvals82[];
extern CONST d11init_t d11ax44bsinitvals82_D11a[];

extern CONST d11init_t d11ax47initvals129[];  /* rev129 = 43684b0 */
extern CONST d11init_t d11ax47bsinitvals129[];
extern CONST d11init_t d11ax47initvalsx129[];
extern CONST d11init_t d11ax47bsinitvalsx129[];
#if defined(WL_PSMR1)
extern CONST d11init_t d11ax47initvalsr1_129[];
extern CONST d11init_t d11ax47bsinitvalsr1_129[];
#endif // endif
extern CONST d11init_t d11ax47initvals129_2[];  /* rev129.2 = 43684c0 */
extern CONST d11init_t d11ax47bsinitvals129_2[];
extern CONST d11init_t d11ax47initvalsx129_2[];
extern CONST d11init_t d11ax47bsinitvalsx129_2[];
#if defined(WL_PSMR1)
extern CONST d11init_t d11ax47initvalsr1_129_2[];
extern CONST d11init_t d11ax47bsinitvalsr1_129_2[];
#endif // endif

extern CONST d11init_t d11ax51initvals130[];  /* rev130 = 63178a0, 63178a2 */
extern CONST d11init_t d11ax51bsinitvals130[];
extern CONST d11init_t d11ax51initvalsx130[];
extern CONST d11init_t d11ax51bsinitvalsx130[];

extern CONST d11init_t d11ax131initvals130_2[]; /* 130.2 = 6756a0 */
extern CONST d11init_t d11ax131initvalsx130_2[];

extern CONST d11init_t d11ax129initvals131[];  /* rev131 = 6710a0 */
extern CONST d11init_t d11ax129bsinitvals131[];
extern CONST d11init_t d11ax129initvalsx131[];
extern CONST d11init_t d11ax129bsinitvalsx131[];

extern CONST d11init_t d11ax130initvals132[];  /* rev132 = 6715a0 */
extern CONST d11init_t d11ax130bsinitvals132[];
extern CONST d11init_t d11ax130initvalsx132[];
extern CONST d11init_t d11ax130bsinitvalsx132[];
#if defined(WL_PSMR1)
extern CONST d11init_t d11ax130initvalsr1_132[];
extern CONST d11init_t d11ax130bsinitvalsr1_132[];
#endif // endif

#if defined(WLDIAG) /* diags ucode, for e.g. ucode loopback test */
extern CONST uint32 d11ucode_maindiag65[];
extern CONST uint d11ucode_maindiag65sz;
extern CONST uint32 d11ucodex_maindiag65[];
extern CONST uint d11ucodex_maindiag65sz;
extern CONST uint32 d11ucode_maindiag128[];
extern CONST uint d11ucode_maindiag128sz;
extern CONST uint32 d11ucodex_maindiag128[];
extern CONST uint d11ucodex_maindiag128sz;
extern CONST uint32 d11ucode_maindiag129[];
extern CONST uint d11ucode_maindiag129sz;
extern CONST uint32 d11ucodex_maindiag129[];
extern CONST uint d11ucodex_maindiag129sz;
extern CONST uint32 d11ucode_maindiag129_1[];
extern CONST uint d11ucode_maindiag129_1sz;
extern CONST uint32 d11ucodex_maindiag129_1[];
extern CONST uint d11ucodex_maindiag129_1sz;
extern CONST uint32 d11ucode_maindiag129_2[];
extern CONST uint d11ucode_maindiag129_2sz;
extern CONST uint32 d11ucodex_maindiag129_2[];
extern CONST uint d11ucodex_maindiag129_2sz;
extern CONST uint32 d11ucode_maindiag130[];
extern CONST uint d11ucode_maindiag130sz;
extern CONST uint32 d11ucodex_maindiag130[];
extern CONST uint d11ucodex_maindiag130sz;
extern CONST uint32 d11ucode_maindiag130_1[];
extern CONST uint d11ucode_maindiag130_1sz;
extern CONST uint32 d11ucodex_maindiag130_1[];
extern CONST uint d11ucodex_maindiag130_1sz;
extern CONST uint32 d11ucode_maindiag130_2[];
extern CONST uint d11ucode_maindiag130_2sz;
extern CONST uint32 d11ucodex_maindiag130_2[];
extern CONST uint d11ucodex_maindiag130_2sz;
extern CONST uint32 d11ucode_mudiag65[];
extern CONST uint d11ucode_mudiag65sz;
extern CONST uint32 d11ucodex_mudiag65[];
extern CONST uint d11ucodex_mudiag65sz;
extern CONST uint32 d11ucode_mudiag128[];
extern CONST uint d11ucode_mudiag128sz;
extern CONST uint32 d11ucodex_mudiag128[];
extern CONST uint d11ucodex_mudiag128sz;
extern CONST uint32 d11ucode_mudiag129[];
extern CONST uint d11ucode_mudiag129sz;
extern CONST uint32 d11ucodex_mudiag129[];
extern CONST uint d11ucodex_mudiag129sz;
extern CONST uint32 d11ucode_mudiag129_1[];
extern CONST uint d11ucode_mudiag129_1sz;
extern CONST uint32 d11ucodex_mudiag129_1[];
extern CONST uint d11ucodex_mudiag129_1sz;
extern CONST uint32 d11ucode_mudiag129_2[];
extern CONST uint d11ucode_mudiag129_2sz;
extern CONST uint32 d11ucodex_mudiag129_2[];
extern CONST uint d11ucodex_mudiag129_2sz;
extern CONST uint32 d11ucode_mudiag65[];
#endif /* WLDIAG */

#endif /* _D11UCODE_H_ */
