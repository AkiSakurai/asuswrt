/*
 * +----------------------------------------------------------------------------
 *
 * BCM BUZZZ Performance tracing tool for ARM Cortex-R4
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * +----------------------------------------------------------------------------
 */
#include <typedefs.h>
#include <bcm_buzzz.h>
#include <osl.h>
#include <bcmpcie.h>

#if defined(BCM_BUZZZ)

#define BUZZZ_PR(x)             printf x

#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
/*
 * +----------------------------------------------------------------------------
 * ARM CR4 performance counter manipulations
 * +----------------------------------------------------------------------------
 */

#if !defined(__ARM_ARCH_7R__)
#error "BUZZZ_CONFIG_CPU_ARM_CR4 defined for invalid ARM Arch"
#endif /* ! __ARM_ARCH_7R__ */

#include <arminc.h>

/* Use buzzz_config_ctr() to select a different 3rd counter */

/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Performance MoNitor Control (PMNC) Register
 *  MRC p15, 0, <Rd>, c9, c12, 0 ; Read PMNC Register
 *  MCR p15, 0, <Rd>, c9, c12, 0 ; Write PMNC Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_PMNC {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 enable      : 1;    /* E: enable all counters incld cctr  */
		uint32 evt_reset   : 1;    /* P: event counter reset             */
		uint32 cctr_reset  : 1;    /* C: cycle counter reset             */
		uint32 clk_divider : 1;    /* D: cycle count divider: 0= 1:1     */
		uint32 export_en   : 1;    /* X: export enable                   */
		uint32 prohibit    : 1;    /* DP: disable in prohibited regions  */
		uint32 reserved    : 5;    /* ReadAZ, Wr: ShouldBeZero/Preserved */
		uint32 counter     : 5;    /* N: number of event counters        */
		uint32 id_code     : 8;    /* IDCODE: identification code        */
		uint32 impl_code   : 8;    /* IMP: implementer code              */
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_PMNC_RD(void)
{
	union cp15_c9_c12_PMNC pmnc;
	__asm__ volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(pmnc.u32));
	return pmnc.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_PMNC_WR(const uint32 v32)
{
	union cp15_c9_c12_PMNC pmnc;
	pmnc.u32 = v32;
	pmnc.reserved = 0;  /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 0" : : "r"(pmnc.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Count Enable Set (CNTENS) Register
 *  MRC p15, 0, <Rd>, c9, c12, 1 ; Read CNTENS Register
 *  MCR p15, 0, <Rd>, c9, c12, 1 ; Write CNTENS Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_CNTENS {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 ctr0        : 1;    /* P0: Enable counter#0 set  */
		uint32 ctr1        : 1;    /* P1: Enable counter#1 set  */
		uint32 ctr2        : 1;    /* P2: Enable counter#2 set  */
		uint32 reserved    : 28;
		uint32 cctr        : 1;    /* C: Enable cycle count set */
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_CNTENS_RD(void)
{
	union cp15_c9_c12_CNTENS ctens;
	__asm__ volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(ctens.u32));
	return ctens.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_CNTENS_WR(const uint32 v32)
{
	union cp15_c9_c12_CNTENS ctens;
	ctens.u32 = v32;
	ctens.reserved = 0; /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(ctens.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Count Enable Clear (CNTENC) Register
 *  MRC p15, 0, <Rd>, c9, c12, 2 ; Read CNTENC Register
 *  MCR p15, 0, <Rd>, c9, c12, 2 ; Write CNTENC Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_CNTENC {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 ctr0        : 1;    /* P0: Enable counter#0 clear  */
		uint32 ctr1        : 1;    /* P1: Enable counter#1 clear  */
		uint32 ctr2        : 1;    /* P2: Enable counter#2 clear  */
		uint32 reserved    : 28;
		uint32 cctr        : 1;    /* C: Enable cycle count clear */
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_CNTENC_RD(void)
{
	union cp15_c9_c12_CNTENC ctenc;
	__asm__ volatile("mrc p15, 0, %0, c9, c12, 2" : "=r"(ctenc.u32));
	return ctenc.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_CNTENC_WR(const uint32 v32)
{
	union cp15_c9_c12_CNTENC ctenc;
	ctenc.u32 = v32;
	ctenc.reserved = 0; /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(ctenc.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Overflow Flag Status (FLAG) Register
 *  MRC p15, 0, <Rd>, c9, c12, 3 ; Read FLAG Register
 *  MCR p15, 0, <Rd>, c9, c12, 3 ; Write FLAG Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_FLAG {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 ctr0        : 1;    /* P0: Enable counter#0 overflow  */
		uint32 ctr1        : 1;    /* P1: Enable counter#1 overflow  */
		uint32 ctr2        : 1;    /* P2: Enable counter#2 overflow  */
		uint32 reserved    : 28;
		uint32 cctr        : 1;    /* C: Enable cycle count overflow */
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_FLAG_RD(void)
{
	union cp15_c9_c12_FLAG flag;
	__asm__ volatile("mrc p15, 0, %0, c9, c12, 3" : "=r"(flag.u32));
	return flag.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_FLAG_WR(const uint32 v32)
{
	union cp15_c9_c12_FLAG flag;
	flag.u32 = v32;
	flag.reserved = 0; /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 3" : : "r"(flag.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Software Increment (SWINCR) Register
 *  MRC p15, 0, <Rd>, c9, c12, 4 ; Read SWINCR Register
 *  MCR p15, 0, <Rd>, c9, c12, 4 ; Write SWINCR Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_SWINCR {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 ctr0        : 1;    /* P0: Increment counter#0  */
		uint32 ctr1        : 1;    /* P1: Increment counter#1  */
		uint32 ctr2        : 1;    /* P2: Increment counter#2  */
		uint32 reserved    : 29;
	};
};
/* may only be used with EVTSEL register = 0x0 */
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_SWINC_RD(void)
{
	union cp15_c9_c12_SWINCR swincr;
	__asm__ volatile("mrc p15, 0, %0, c9, c12, 4" : "=r"(swincr.u32));
	return swincr.u32;
}
/* may only be used with EVTSEL register = 0x0 */
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_SWINCR_WR(const uint32 v32)
{
	union cp15_c9_c12_SWINCR swincr;
	swincr.u32 = v32;
	swincr.reserved = 0;    /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 4" : : "r"(swincr.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Performance Counter Selection (PMNXSEL) Register
 *  MCR p15, 0, <Rd>, c9, c12, 5 ; Write PMNXSELRegister
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c12_PMNXSEL {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 ctr_sel   : 5;      /* event counter selecter */
		uint32 reserved  :25;      /* reserved               */
	};
};
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_PMNXSEL_WR(const uint32 v32)
{
	union cp15_c9_c12_PMNXSEL pmnxsel;
	pmnxsel.u32 = v32;
	pmnxsel.reserved = 0;
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(pmnxsel.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Cycle Count (CCNT) Register
 *  MRC p15, 0, <Rd>, c9, c13, 0; Read CCNT Register
 *  MCR p15, 0, <Rd>, c9, c13, 0; Write CCNT Register
 * +----------------------------------------------------------------------------
 */
struct cp15_c9_c13_CCNT {
	uint32 u32;
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_CCNT_RD(void)
{
	struct cp15_c9_c13_CCNT ccnt;
	__asm__ volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(ccnt.u32));
	return ccnt.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_CCNT_WR(const uint32 v32)
{
	struct cp15_c9_c13_CCNT ccnt;
	ccnt.u32 = v32;
	__asm__ volatile("mcr p15, 0, %0, c9, c13, 0" : : "r"(ccnt.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Event Selection (EVTSEL0 to EVTSEL2) Registers
 *  MRC p15, 0, <Rd>, c9, c13, 1; Read EVTSELx Register
 *  MCR p15, 0, <Rd>, c9, c13, 1; Write EVTSELx Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c13_EVTSEL {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 evt_type :  8;      /* event type to count */
		uint32 reserved : 24;
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_EVTSEL_RD(void)
{
	union cp15_c9_c13_EVTSEL evtsel;
	__asm__ volatile("mrc p15, 0, %0, c9, c13, 1" : "=r"(evtsel.u32));
	return evtsel.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_EVTSEL_WR(const uint32 v32)
{
	union cp15_c9_c13_EVTSEL evtsel;
	evtsel.u32 = v32;
	evtsel.reserved = 0;    /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(evtsel.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Performance Count (PMC0-PMC2) Register
 *  MRC p15, 0, <Rd>, c9, c13, 2; Read PMCx Register
 *  MCR p15, 0, <Rd>, c9, c13, 2; Write PMCx Register
 * +----------------------------------------------------------------------------
 */
struct cp15_c9_c13_PMC {
	uint32 u32;
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_PMC_RD(void)
{
	struct cp15_c9_c13_PMC pmc;
	__asm__ volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(pmc.u32));
	return pmc.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_PMC_WR(const uint32 v32)
{
	struct cp15_c9_c13_PMC pmc;
	pmc.u32 = v32;
	__asm__ volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(pmc.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 User Enable (USEREN) Register
 *  MRC p15, 0, <Rd>, c9, c14, 0; Read USEREN Register
 *  MCR p15, 0, <Rd>, c9, c14, 0; Write USEREN Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c14_USEREN {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 enable   : 1;        /* user mode enable  */
		uint32 reserved : 31;
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_USEREN_RD(void)
{
	union cp15_c9_c14_USEREN useren;
	__asm__ volatile("mrc p15, 0, %0, c9, c14, 0" : "=r"(useren.u32));
	return useren.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_USEREN_WR(const uint32 v32)
{
	union cp15_c9_c14_USEREN useren;
	useren.u32 = v32;
	useren.reserved = 0;    /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c14, 0" : : "r"(useren.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Counter Overflow Interrupt Enable Set (INTENS) Register
 *  MRC p15, 0, <Rd>, c9, c14, 1 ; Read INTENS Register
 *  MCR p15, 0, <Rd>, c9, c14, 1 ; Write INTENS Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c14_INTENS {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 ctr0        : 1;    /* P0: Enable counter#0 interrupt set */
		uint32 ctr1        : 1;    /* P1: Enable counter#1 interrupt set */
		uint32 ctr2        : 1;    /* P2: Enable counter#2 interrupt set */
		uint32 reserved    : 28;
		uint32 cctr        : 1;    /* C: Enable cyclecount interrupt set */
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_INTENS_RD(void)
{
	union cp15_c9_c14_INTENS intens;
	__asm__ volatile("mrc p15, 0, %0, c9, c14, 1" : "=r"(intens.u32));
	return intens.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_INTENS_WR(const uint32 v32)
{
	union cp15_c9_c14_INTENS intens;
	intens.u32 = v32;
	intens.reserved = 0;    /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c14, 1" : : "r"(intens.u32));
}


/*
 * +----------------------------------------------------------------------------
 * ARM CR4 Counter Overflow Interrupt Enable Clear (INTENC) Register
 *  MRC p15, 0, <Rd>, c9, c14, 2 ; Read INTENC Register
 *  MCR p15, 0, <Rd>, c9, c14, 2 ; Write INTENC Register
 * +----------------------------------------------------------------------------
 */
union cp15_c9_c14_INTENC {
	uint32 u32;
	struct {    /* Little Endian */
		uint32 ctr0        : 1;    /* P0: Enable counter#0 interrupt clr */
		uint32 ctr1        : 1;    /* P1: Enable counter#1 interrupt clr */
		uint32 ctr2        : 1;    /* P2: Enable counter#2 interrupt clr */
		uint32 reserved    : 28;
		uint32 cctr        : 1;    /* C: Enable cyclecount interrupt clr */
	};
};
static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
__armcr4_INTENC_RD(void)
{
	union cp15_c9_c14_INTENC intenc;
	__asm__ volatile("mrc p15, 0, %0, c9, c14, 2" : "=r"(intenc.u32));
	return intenc.u32;
}
static BUZZZ_INLINE void BUZZZ_NOINSTR_FUNC
__armcr4_INTENC_WR(const uint32 v32)
{
	union cp15_c9_c14_INTENC intenc;
	intenc.u32 = v32;
	intenc.reserved = 0;    /* Should Be Zero Preserved */
	__asm__ volatile("mcr p15, 0, %0, c9, c14, 2" : : "r"(intenc.u32));
}


static void BUZZZ_NOINSTR_FUNC
_armcr4_pmc_enable(void)
{
	union cp15_c9_c12_PMNC pmnc;
	union cp15_c9_c12_CNTENS cntens;
	union cp15_c9_c12_FLAG flag;

	/* Disable overflow interrupts on 3 buzzz counters: ctr0 .. ctr2 */
	flag.u32 = __armcr4_FLAG_RD();
	flag.ctr0 = flag.ctr1 = flag.ctr2 = 0;
	__armcr4_FLAG_WR(flag.u32);

	/* Set Enable bit in PMNC */
	pmnc.u32 = __armcr4_PMNC_RD();          /* mrc p15, 0, r0, c9, c12, 0 */
	pmnc.enable = 1;                        /* orr r0, r0, #0x01          */
	__armcr4_PMNC_WR(pmnc.u32);             /* mcr p15, 0, r0, c9, c12, 0 */

	/* Enable the 3 buzzz counters: ctr0 to ctr2 */
	cntens.u32 = __armcr4_CNTENS_RD();      /* mrc p15, 0, r0, c9, c12, 1 */
	cntens.ctr0 = cntens.ctr1 = cntens.ctr2 = 1;
	__armcr4_CNTENS_WR(cntens.u32);         /* mcr p15, 0, r0, c9, c12, 1 */
}

static void BUZZZ_NOINSTR_FUNC
_armcr4_pmc_disable(void)
{
	union cp15_c9_c12_CNTENC cntenc;

	cntenc.u32 = 0U;
	cntenc.ctr0 = cntenc.ctr1 = cntenc.ctr2 = 1;
	__armcr4_CNTENC_WR(cntenc.u32);
}

/* Macro equivalent to _armcr4_pmc_read_buzzz_ctr() */
#define __armcr4_pmc_read_buzzz_ctr(ctr_sel)                                   \
({                                                                             \
	uint32 v32 = ctr_sel;                                                      \
	__asm__ volatile("mcr p15, 0, %0, c9, c12, 5" : : "r"(v32));               \
	__asm__ volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(v32));                \
	v32;                                                                       \
})

static BUZZZ_INLINE uint32 BUZZZ_NOINSTR_FUNC
_armcr4_pmc_read_buzzz_ctr(const uint32 ctr_sel)
{
	uint32 v32;
	union cp15_c9_c12_PMNXSEL pmnxsel;

	pmnxsel.u32 = 0U;
	pmnxsel.ctr_sel = ctr_sel;
	__armcr4_PMNXSEL_WR(pmnxsel.u32);

	v32 = __armcr4_PMC_RD();

	return v32;
}

#define BUZZZ_READ_COUNTER(ctrsel)	__armcr4_pmc_read_buzzz_ctr(ctrsel)

static void BUZZZ_NOINSTR_FUNC
_armcr4_pmnc_config_buzzz_ctr(const uint32 ctr_sel, const uint32 evt_type)
{
	union cp15_c9_c13_EVTSEL evtsel;
	union cp15_c9_c12_PMNXSEL pmnxsel;

	/* Select counter to be configured */
	pmnxsel.u32 = 0U;
	pmnxsel.ctr_sel = ctr_sel;
	__armcr4_PMNXSEL_WR(pmnxsel.u32);

	/* Configure event to be counted for selected counter */
	evtsel.u32 = 0U;
	evtsel.evt_type = evt_type;
	__armcr4_EVTSEL_WR(evtsel.u32);
}

#endif /*  BUZZZ_CONFIG_CPU_ARM_CR4 */


#if defined(BUZZZ_CONFIG_CPU_ARM_CM3)
/*
 * +----------------------------------------------------------------------------
 * ARM CM3 performance counter manipulations
 * +----------------------------------------------------------------------------
 */

#if !defined(__ARM_ARCH_7M__)
#error "BUZZZ_CONFIG_CPU_ARM_CM3 defined for invalid ARM Arch"
#endif /* ! __ARM_ARCH_7M__ */

#include <arminc.h>

/* Debug Exception and Monitor Control Register */
#define BUZZZ_DEMCR                (0xE000EDFC)
#define BUZZZ_DEMCR_TRCENA         (1 << 24)

/* Available Debug Watchpoint and Trace (DWT) counters */
#define BUZZZ_DWT_CTRL             (0xE0001000)

#define BUZZZ_DWT_CYCCNT           (0xE0001004)
#define BUZZZ_DWT_CYCEVTENA        (1 << 22)
#define BUZZZ_DWT_CYCCNTENA        (1 << 0)

#define BUZZZ_DWT_CPICNT           (0xE0001008)
#define BUZZZ_DWT_CPIEVTENA        (1 << 17)

#define BUZZZ_DWT_EXCCNT           (0xE000100C)
#define BUZZZ_DWT_EXCEVTENA        (1 << 18)

#define BUZZZ_DWT_SLEEPCNT         (0xE0001010)
#define BUZZZ_DWT_SLEEPEVTENA      (1 << 19)

#define BUZZZ_DWT_LSUCNT           (0xE0001014)
#define BUZZZ_DWT_LSUEVTENA        (1 << 20)

#define BUZZZ_DWT_FOLDCNT          (0xE0001018)
#define BUZZZ_DWT_FOLDEVTENA       (1 << 21)

/* Enable all DWT counters */
#define BUZZZ_DWT_CTRL_ENAB                                                    \
	(BUZZZ_DWT_CYCCNTENA | BUZZZ_DWT_CPIEVTENA | BUZZZ_DWT_EXCEVTENA |         \
	 BUZZZ_DWT_SLEEPEVTENA | BUZZZ_DWT_LSUEVTENA | BUZZZ_DWT_FOLDEVTENA)

/* Pack 8 bit CM3 counters */
typedef union cm3_cnts {
	uint32 u32;
	uint8 u8[4];
	struct {
		uint8 cpicnt;
		uint8 exccnt;
		uint8 sleepcnt;
		uint8 lsucnt;
	};
} cm3_cnts_t;

static void BUZZZ_NOINSTR_FUNC
_armcm3_enable_dwt(void)
{
	uint32 v32;
	volatile uint32 * reg32;

	/* Set Trace Enable bit24 in Debug Exception and Monitor Control Register */
	reg32 = (volatile uint32 *)BUZZZ_DEMCR;
	v32 = *reg32; *reg32 = v32 | BUZZZ_DEMCR_TRCENA;           /* OR_REG */

	/* Reset values in selected counters */
	reg32 = (volatile uint32 *)BUZZZ_DWT_CYCCNT; *reg32 = 0;   /* W_REG */

	reg32 = (volatile uint32 *)BUZZZ_DWT_CTRL; /* Enable DWT */
	v32 = *reg32; v32 &= 0xF0000000; /* Save the NUMCOMP ReadOnly bits 28::31 */
	/* Ensure bit 12 is not set! */
	*reg32 = v32 | BUZZZ_DWT_CTRL_ENAB;                       /* OR_REG */
}


static void BUZZZ_NOINSTR_FUNC
_armcm3_disable_dwt(void)
{
	uint32 v32;
	volatile uint32 * reg32;

	reg32 = (volatile uint32 *)BUZZZ_DWT_CTRL;
	v32 = *reg32; *reg32 = v32 & 0xF0000000;                   /* AND_REG */

	/* Clr Trace Enable bit24 in Debug Exception and Monitor Control Register */
	reg32 = (volatile uint32 *)BUZZZ_DEMCR;
	v32 = *reg32; *reg32 = v32 & ~BUZZZ_DEMCR_TRCENA;          /* AND_REG */
}

#endif /* BUZZZ_CONFIG_CPU_ARM_CM3 */


/* -------------------------------------------------------------------------- */

uint32 buzzz_config_ctrs[BUZZZ_COUNTERS] = {
#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
    BUZZZ_ARMCR4_CYCLECNT_EVT,
    BUZZZ_ARMCR4_INSTRCNT_EVT,
    BUZZZ_ARMCR4_BRMISS_EVT
#endif /* BUZZZ_CONFIG_CPU_ARM_CR4 */
};


typedef struct buzzz_log
{
#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
	uint32 ctr[BUZZZ_COUNTERS];
#endif /* BUZZZ_CONFIG_CPU_ARM_CR4 */

#if defined(BUZZZ_CONFIG_CPU_ARM_CM3)
	uint32 cyccnt;
	cm3_cnts_t cnts;
#endif /* BUZZZ_CONFIG_CPU_ARM_CM3 */

	buzzz_arg0_t arg0;
	uint32 arg1;                  /* upto 1 argument */

#if defined(BUZZZ_4ARGS)
	uint32 arg2, arg3, arg4;      /* upto 4 arguments */
#endif /*   BUZZZ_4ARGS */

} buzzz_log_t;


static buzzz_t buzzz_g =
{
	.log    = (uint32)NULL,
	.cur    = (uint32)NULL,
	.end    = (uint32)NULL,

	.count  = 0U,
	.status = BUZZZ_DISABLED,
	.wrap   = BUZZZ_FALSE,

	.buffer_sz = BUZZZ_LOG_BUFSIZE,
	.log_sz    = sizeof(buzzz_log_t),
	.counters  = BUZZZ_COUNTERS,
	.ovhd      = {
#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
	               BUZZZ_CR4_CYCLECNT_OVHD,
	               BUZZZ_CR4_INSTRCNT_OVHD,
	               BUZZZ_CR4_BRMISPRD_OVHD,
	               0, 0, 0, 0, 0
#endif /* BUZZZ_CONFIG_CPU_ARM_CR4 */
	             }
};

#define BUZZZ_G (buzzz_g)


#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
#define _BUZZZ_PREAMBLE(log_p, evt_id, num_args)                               \
	if (BUZZZ_G.status != BUZZZ_ENABLED) return;                               \
	log_p = (buzzz_log_t *)BUZZZ_G.cur;                                        \
	(log_p)->arg0.klog.id = (evt_id);                                          \
	(log_p)->arg0.klog.args = (num_args);                                      \
	(log_p)->ctr[0] = BUZZZ_READ_COUNTER(0);                                   \
	(log_p)->ctr[1] = BUZZZ_READ_COUNTER(1);                                   \
	(log_p)->ctr[2] = BUZZZ_READ_COUNTER(2);
#endif /* BUZZZ_CONFIG_CPU_ARM_CR4 */

#if defined(BUZZZ_CONFIG_CPU_ARM_CM3)
#define _BUZZZ_PREAMBLE(log_p, evt_id, num_args)                               \
({                                                                             \
	cm3_cnts_t cm3_cnts;                                                       \
	volatile uint8  * reg8;                                                    \
	volatile uint32 * reg32;                                                   \
	if (BUZZZ_G.status != BUZZZ_ENABLED) return;                               \
	log_p = (buzzz_log_t *)BUZZZ_G.cur;                                        \
	reg32 = (volatile uint32 *)BUZZZ_DWT_CYCCNT; (log_p)->cyccnt = *reg32;     \
	reg8 = (volatile uint8 *)BUZZZ_DWT_CPICNT; cm3_cnts.cpicnt = *reg8;        \
	reg8 = (volatile uint8 *)BUZZZ_DWT_EXCCNT; cm3_cnts.exccnt = *reg8;        \
	reg8 = (volatile uint8 *)BUZZZ_DWT_SLEEPCNT; cm3_cnts.sleepcnt = *reg8;    \
	reg8 = (volatile uint8 *)BUZZZ_DWT_LSUCNT; cm3_cnts.lsucnt = *reg8;        \
	(log_p)->cnts.u32 = cm3_cnts.u32;                                          \
	reg8 = (volatile uint8 *)BUZZZ_DWT_FOLDCNT;                                \
	(log_p)->arg0.u32 = (*reg8) | ((num_args) << 8) | ((evt_id) << 16);        \
})
#endif /* BUZZZ_CONFIG_CPU_ARM_CM3 */

#define _BUZZZ_POSTAMBLE()                                                     \
	BUZZZ_G.cur = (uint32)(((buzzz_log_t *)BUZZZ_G.cur) + 1);                   \
	BUZZZ_G.count++;                                                           \
	if (BUZZZ_G.cur >= BUZZZ_G.end) {                                          \
		BUZZZ_G.wrap = BUZZZ_TRUE;                                             \
		BUZZZ_G.cur = BUZZZ_G.log;                                             \
	}                                                                          \


void BUZZZ_NOINSTR_FUNC
buzzz_log0(uint32 evt_id)
{
	buzzz_log_t * log_p;
	_BUZZZ_PREAMBLE(log_p, evt_id, 0);
	_BUZZZ_POSTAMBLE();
}

void BUZZZ_NOINSTR_FUNC
buzzz_log1(uint32 evt_id, uint32 arg1)
{
	buzzz_log_t * log_p;
	_BUZZZ_PREAMBLE(log_p, evt_id, 1);
	log_p->arg1 = arg1;
	_BUZZZ_POSTAMBLE();
}

/* Not supported */
#if defined(BUZZZ_4ARGS)
void BUZZZ_NOINSTR_FUNC
buzzz_log2(uint32 evt_id, uint32 arg1, uint32 arg2)
{
	buzzz_log_t * log_p;
	_BUZZZ_PREAMBLE(log_p, evt_id, 2);
	log_p->arg1 = arg1;
	log_p->arg2 = arg2;
	_BUZZZ_POSTAMBLE();
}

void BUZZZ_NOINSTR_FUNC
buzzz_log3(uint32 evt_id, uint32 arg1, uint32 arg2, uint32 arg3)
{
	buzzz_log_t * log_p;
	_BUZZZ_PREAMBLE(log_p, evt_id, 3);
	log_p->arg1 = arg1;
	log_p->arg2 = arg2;
	log_p->arg3 = arg3;
	_BUZZZ_POSTAMBLE();
}

void BUZZZ_NOINSTR_FUNC
buzzz_log4(uint32 evt_id,
           uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4)
{
	buzzz_log_t * log_p;
	_BUZZZ_PREAMBLE(log_p, evt_id, 4);
	log_p->arg1 = arg1;
	log_p->arg2 = arg2;
	log_p->arg3 = arg3;
	log_p->arg4 = arg4;
	_BUZZZ_POSTAMBLE();
}
#endif /* BUZZZ_4ARGS */


void BUZZZ_NOINSTR_FUNC
buzzz_clear(void)
{
	if (BUZZZ_G.log == NULL) {
		BUZZZ_PR(("buzzz not registered\n"));
		return;
	}

	BUZZZ_G.count = 0;

	if (BUZZZ_G.status == BUZZZ_ENABLED) {
		BUZZZ_PR(("buzzz is enabled\n"));
		return;
	}
}

/*
 * -----------------------------------------------------------------------------
 * Start kernel event tracing. Configures the performance counters and enables
 * performance counting. Tool status is set enabled.
 * -----------------------------------------------------------------------------
 */
void BUZZZ_NOINSTR_FUNC
buzzz_start(void)
{
	if (BUZZZ_G.log == NULL) {
		BUZZZ_PR(("buzzz not registered\n"));
		return;
	}

	BUZZZ_G.wrap  = BUZZZ_FALSE;
	BUZZZ_G.cur   = BUZZZ_G.log;
	BUZZZ_G.end   = (uint32)((char*)(BUZZZ_G.log)
	              + (BUZZZ_LOG_BUFSIZE - BUZZZ_LOGENTRY_MAXSZ));

#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
	/* Select counter and configure event */
	_armcr4_pmnc_config_buzzz_ctr(0, buzzz_config_ctrs[0]);
	_armcr4_pmnc_config_buzzz_ctr(1, buzzz_config_ctrs[1]);
	_armcr4_pmnc_config_buzzz_ctr(2, buzzz_config_ctrs[2]);
	_armcr4_pmc_enable();
#endif  /*  BUZZZ_CONFIG_CPU_ARM_CR4 */

#if defined(BUZZZ_CONFIG_CPU_ARM_CM3)
	_armcm3_enable_dwt();
#endif  /*  BUZZZ_CONFIG_CPU_ARM_CM3 */

	BUZZZ_G.status = BUZZZ_ENABLED;
}


/*
 * -----------------------------------------------------------------------------
 * Stops kernel event tracing. Disable performance counting and tool status set
 * to disabled.
 * -----------------------------------------------------------------------------
 */
void BUZZZ_NOINSTR_FUNC
buzzz_stop(void)
{
	/* Estimate the overhead per buzzz_log call */

	if (BUZZZ_G.status != BUZZZ_DISABLED) {
		BUZZZ_G.status = BUZZZ_DISABLED;

#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
		_armcr4_pmnc_config_buzzz_ctr(0, 0x0);
		_armcr4_pmnc_config_buzzz_ctr(1, 0x0);
		_armcr4_pmnc_config_buzzz_ctr(2, 0x0);
		_armcr4_pmc_disable();
#endif  /*  BUZZZ_CONFIG_CPU_ARM_CR4 */

#if defined(BUZZZ_CONFIG_CPU_ARM_CM3)
		_armcm3_disable_dwt();
#endif  /*  BUZZZ_CONFIG_CPU_ARM_CM3 */

	}
}

#if defined(BCM_OLA)
void BUZZZ_NOINSTR_FUNC
buzzz_dump()
{
	printf("Use: dhd -i eth1 buzzz_dump\n");
}
#endif /* BCM_OLA */

void BUZZZ_NOINSTR_FUNC
buzzz_config_ctr(uint32 ctr_sel)
{
#if defined(BUZZZ_CONFIG_CPU_ARM_CR4)
	buzzz_config_ctrs[2] = ctr_sel;
	buzzz_g.ovhd[2] = 0; /* clear the overhead */
	BUZZZ_PR(("ARM CR4: Counters <%02X, %02X, %02X>\n",
	       buzzz_config_ctrs[0], buzzz_config_ctrs[1], buzzz_config_ctrs[2]));
#endif /* BUZZZ_CONFIG_CPU_ARM_CR4 */
}


/* Invoke this once in a datapath module's init */
int /* Register the format strings for each event point   */
buzzz_register(void * shared)
{
	pciedev_shared_t *sh = (pciedev_shared_t *)shared;
	void * buffer_p = NULL;

	if ((buffer_p = MALLOC(NULL, BUZZZ_LOG_BUFSIZE)) == NULL) {
		BUZZZ_PR(("buzzz_register failed memory alloc\n"));
		return -1;
	}

	buzzz_g.log = (uint32)buffer_p;

	/* Register with Host side */
	sh->buzzz = (uint32)&buzzz_g;

	BUZZZ_PR(("BUZZZ registered CR4<%p>\n", &buzzz_g));

	return 0;
}

#endif /* BCM_BUZZZ */
