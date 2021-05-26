/*
 * gSPI Protocol
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
 * $Id: gspi.h 382882 2013-02-04 23:24:31Z $
 */

#define SPI_RW_FLAG_M			BITFIELD_MASK(1)	/* Bit [31] - R/W Command Bit */
#define SPI_RW_FLAG_S			31
#define SPI_ACCESS_M			BITFIELD_MASK(1)	/* Bit [30] - Fixed/Incr Access */
#define SPI_ACCESS_S			30
#define SPI_FUNCTION_M			BITFIELD_MASK(2)	/* Bits [29:28]	- Function Number */
#define SPI_FUNCTION_S			28
#define SPI_REG_ADDR_M			BITFIELD_MASK(17)	/* Bit [27:11] - Address */
#define SPI_REG_ADDR_S			11
#define SPI_LEN_M			BITFIELD_MASK(11)	/* Bit [10:0] - Packet length */
#define SPI_LEN_S			0

#define SPI_RSP_START_M		BITFIELD_MASK(1)	/* Bit [7] 	- Start Bit (always 0) */
#define SPI_RSP_START_S		7
#define SPI_RSP_PARAM_ERR_M	BITFIELD_MASK(1)	/* Bit [6] 	- Parameter Error */
#define SPI_RSP_PARAM_ERR_S	6
#define SPI_RSP_RFU5_M		BITFIELD_MASK(1)	/* Bit [5] 	- RFU (Always 0) */
#define SPI_RSP_RFU5_S		5
#define SPI_RSP_FUNC_ERR_M	BITFIELD_MASK(1)	/* Bit [4] 	- Function number error */
#define SPI_RSP_FUNC_ERR_S	4
#define SPI_RSP_CRC_ERR_M	BITFIELD_MASK(1)	/* Bit [3] 	- COM CRC Error */
#define SPI_RSP_CRC_ERR_S	3
#define SPI_RSP_ILL_CMD_M	BITFIELD_MASK(1)	/* Bit [2] 	- Illegal Command error */
#define SPI_RSP_ILL_CMD_S	2
#define SPI_RSP_RFU1_M		BITFIELD_MASK(1)	/* Bit [1] 	- RFU (Always 0) */
#define SPI_RSP_RFU1_S		1
#define SPI_RSP_IDLE_M		BITFIELD_MASK(1)	/* Bit [0] 	- In idle state */
#define SPI_RSP_IDLE_S		0
