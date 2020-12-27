/*
 * ejtag access interface
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
 * $Id: ejtag.h 543582 2015-03-24 19:48:01Z $
 */

#ifndef INC_EJTAG_H
#define INC_EJTAG_H

/* type of jtag jig */
#define ALTERA	0
#define ALTERNATIVE	1
#define XILINX	2
#define HND_JTAGM	3
extern int dongle;

/* global indicating endianness of target */
extern bool ejtag_bigend;

/* global indicating mode of target access */
#define	EJTAG_MIPS	1
#define	EJTAG_CHIPC	2
extern int ejtag_mode;

/* Instruction & data register sizes */
#define DEF_DATA_SIZE	32		/* Default DR size */
#define DEF_INST_SIZE	8		/* Default IR size */
extern uint dr_size;
extern uint ir_size;

/* Tracing of jtag signals and/or register accesses */
#define	JTAG_TRACE_REGS		1
#define	JTAG_TRACE_SIGNALS	2
extern int jtag_trace;

/* Ejtag function prototypes */
#ifdef BCMDRIVER
extern int ejtag_init(uint16 devid, uint32 sbidh, void *regsva, bool bendian);
extern void ejtag_cleanup(void);
#else
extern void initialize_jtag_hardware(bool remote);
extern void close_jtag_hardware(void);

extern int ejtag_writereg(ulong instr, ulong write_data);
extern int ejtag_readreg(ulong instr, ulong *read_data);

extern void ejtag_reset(void);
#endif // endif

extern int write_ejtag(ulong addr, ulong write_data, uint size);
extern int read_ejtag(ulong addr, ulong *read_data, uint size);

#endif /* INC_EJTAG_H */
