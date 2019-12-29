/*
 * VASIP related declarations
 * Broadcom 802.11abg Networking Device Driver
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_vasip.h 775549 2019-06-04 04:40:02Z $
 */

#ifndef _WLC_VASIP_H_
#define _WLC_VASIP_H_

#include "wlc_cfg.h"

/* Module states */
struct wlc_vasip_info {
	wlc_info_t *wlc;
	uint mu_supported_Ntx;
	CONST uint32 *sym_map;
	CONST uint16 *sym_map_size;
};

#if defined(WLVASIP) || defined(SAVERESTORE)

/* VASIP FW verison */
#define VASIP_FW_VER_NUM(Vmaj, Vmin) ((Vmaj)*10000 + (((Vmin) < 100)? ((Vmin)*100): (Vmin)))
#define VASIP_FW_VER                 VASIP_FW_VER_NUM(VASIP_FW_VER_MAJOR, VASIP_FW_VER_MINOR)

/* m2v mailbox verison */
#define V2M_FMT_NOT_SUPPORT 0
#define V2M_FMT_V1          1
#define V2M_FMT_V1_5        2
#define V2M_FMT_V3          3

#define V2M_FORMAT_4366  (V2M_FMT_V1)
#define V2M_FORMAT_43684 (((VASIP_FW_VER) >= VASIP_FW_VER_NUM(6, 0)) ? V2M_FMT_V3 :\
		((VASIP_FW_VER) >= VASIP_FW_VER_NUM(5, 25)) ? V2M_FMT_V1_5 : V2M_FMT_V1)
#define V2M_FORMAT_63178 (((VASIP_FW_VER) >= VASIP_FW_VER_NUM(6, 2)) ? V2M_FMT_V3 : V2M_FMT_V1)

#define V2M_FORMAT_VER(corever) (D11REV_IS((corever), 64) ? V2M_FORMAT_4366 :\
		(D11REV_IS((corever), 65) ? V2M_FORMAT_4366 :\
		(D11REV_IS((corever), 128) ? V2M_FORMAT_43684 :\
		(D11REV_IS((corever), 129) ? V2M_FORMAT_43684 :\
		(D11REV_IS((corever), 130) ? V2M_FORMAT_63178 :\
		V2M_FMT_NOT_SUPPORT)))))

#define IS_V2M_FORMAT_V1(corever)   (V2M_FORMAT_VER((corever)) == V2M_FMT_V1)
#define IS_V2M_FORMAT_V1_5(corever) (V2M_FORMAT_VER((corever)) == V2M_FMT_V1_5)
#define IS_V2M_FORMAT_V3(corever)   (V2M_FORMAT_VER((corever)) == V2M_FMT_V3)
#define IS_V2M_FORMAT_V1X(corever)  (IS_V2M_FORMAT_V1((corever))||IS_V2M_FORMAT_V1_5((corever)))

void wlc_vasip_write(wlc_hw_info_t *wlc_hw, const uint32 vasip_code[],
	const uint nbytes, uint32 offset, uint32 offset_tbl);
void wlc_vasip_read(wlc_hw_info_t *wlc_hw, uint32 vasip_code[],
	const uint nbytes, uint32 offset);
bool wlc_vasip_present(wlc_hw_info_t *wlc_hw);

#define VASIP_RTCAP_SGI_NBIT    0x2
#define VASIP_RTCAP_LDPC_NBIT   0x4
#define VASIP_RTCAP_BCMSTA_NBIT 0x5

/* initialize vasip */
void wlc_vasip_init(wlc_hw_info_t *wlc_hw, uint32 vasipver, bool nopi);
/* vasip version provision */
bool wlc_vasip_support(wlc_hw_info_t *wlc_hw, uint32 *vasipver, bool nopi);

#if defined(BCMDBG)
/* dump vasip code info */
void wlc_vasip_code_info(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif

/* attach/detach */
extern wlc_vasip_info_t *wlc_vasip_attach(wlc_info_t *wlc);
extern void wlc_vasip_detach(wlc_vasip_info_t *vasip);
int wlc_svmp_mem_blk_set(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 len, uint16 *val);

/* VASIP offsets are 16-bit */
#define VASIP_OFFSET_32(OFFSET) ((OFFSET)>>1)
#define VASIP_OFFSET_BYTE(OFFSET) ((OFFSET)<<1)

#define VASIP_CODE_OFFSET	0

uint32 vasip_shared_offset(wlc_hw_info_t *wlc_hw, unsigned int sym);
uint32 vasip_shared_size(wlc_hw_info_t *wlc_hw, unsigned int sym);

#define VASIP_SYM(SYM) (__vasip_map_##SYM)
#define VASIP_SHARED_OFFSET(HW, SYM) vasip_shared_offset(HW, VASIP_SYM(SYM))
#define VASIP_SHARED_SIZE(HW, SYM) vasip_shared_size(HW, VASIP_SYM(SYM))

#ifdef WL_AIR_IQ
int wlc_svmp_mem_read64(wlc_hw_info_t *wlc_hw, uint64 *ret_svmp_addr, uint32 offset, uint16 len);
int wlc_svmp_mem_set_axi(wlc_hw_info_t *wlc_hw, uint32 offset, uint16 len, uint16 val);
int wlc_svmp_mem_read_axi(wlc_hw_info_t *wlc_hw, uint16 *ret_svmp_addr, uint32 offset, uint16 len);
#endif // endif
extern void wlc_svmp_update_ratecap(wlc_info_t *wlc, scb_t *scb, uint8 bfm_index);
#endif /* WLVASIP */
#if !defined(WLVASIP)
#define wlc_svmp_update_ratecap(a, b, c) do {} while (0)
#endif // endif
#endif /* _WLC_VASIP_H_ */
