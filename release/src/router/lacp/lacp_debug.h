/*
 * Broadcom LACP debug header
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */

#ifndef _LACP_DEBUG_H_
#define _LACP_DEBUG_H_

extern uint32 lacp_msg_level;

#define LACP_TIMER_VAL		0x00000001
#define LACP_FSM_VAL		0x00000002
#define LACP_SELECT_VAL		0x00000004
#define LACP_LINUX_VAL		0x00000008
#define LACP_LACPC_VAL		0x00000010
#define LACP_DUMP_LACPPKT_VAL   0x00000020
#define LACP_MAX_VAL            0x0000003F      /* the max. value when all flags are set */

#define LACP_PRINT(args) \
	do { printf("LACP %s(%d): ", __FUNCTION__, __LINE__); printf args; } while (0)

#define LACP_MSG(fmt, arg...) \
	do { printf("LACP %s(%d): MSG:"fmt, __FUNCTION__, __LINE__, ##arg); } while (0)
#define LACP_ERROR(fmt, arg...) \
	do { printf("LACP %s(%d): ERROR:"fmt, __FUNCTION__, __LINE__, ##arg); } while (0)

#define LACP_TIMER(args) \
	do {if (lacp_msg_level & LACP_TIMER_VAL) LACP_PRINT(args);} while (0)
#define LACP_FSM(args) \
	do {if (lacp_msg_level & LACP_FSM_VAL) LACP_PRINT(args);} while (0)
#define LACP_SELECT(args) \
	do {if (lacp_msg_level & LACP_SELECT_VAL) LACP_PRINT(args);} while (0)
#define LACP_LINUX(args) \
	do {if (lacp_msg_level & LACP_LINUX_VAL) LACP_PRINT(args);} while (0)
#define LACP_LACPC(args) \
	do {if (lacp_msg_level & LACP_LACPC_VAL) LACP_PRINT(args);} while (0)

#endif /* _LACP_DEBUG_H_ */
