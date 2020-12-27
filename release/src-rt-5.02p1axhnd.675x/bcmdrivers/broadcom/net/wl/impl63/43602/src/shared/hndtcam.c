/*
 * BCM43XX Sonics SiliconBackplane TCAM Access routines
 *
 * TCAM init/load can be done before si_attach()
 *
 * Copyright (C) 2020, Broadcom. All Rights Reserved.
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
 * $Id: hndtcam.c 455422 2014-02-13 23:56:55Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndsoc.h>
#include <sbsocram.h>
#include <hndtcam.h>

#define TCAM_OUT(args)		printf args
#ifdef BCMDBG_TCAM
#define TCAM_TRACE(args)	printf args
#else
#define TCAM_TRACE(args)
#endif // endif

#define PATCHHDR(_p)		__attribute__ ((__section__ (".patchhdr."#_p))) _p
#define PATCHENTRY(_p)		__attribute__ ((__section__ (".patchentry."#_p))) _p
#define PATBL_IDX(_p)		((_p) - _patch_table_start)
#define PATBL_ENTRY(i)		(&_patch_table_start[(i)])
#define PATBL_CNT		(_patch_table_last - _patch_table_start)
#define PATBL_MAX		(_patch_table_end - _patch_table_start)
#define PATBL_TABLE		_patch_table_start
#define PATBL_ESIZE		(sizeof(patch_entry_t))
#define PATBL_ESTART(p)		(((uintptr)(p)) & ~(PATBL_ESIZE - 1))
#define PATBL_EEND(p)		(((patch_entry_t *)PATBL_ESTART(p)) + 1)
#define PATHDR_SIZE		(sizeof(patch_hdr_t))
#define PATHDR_CNT		(_patch_hdr_end - _patch_hdr_start)
#define PATHDR_TABLE		_patch_hdr_start

#ifdef BCMDBG_TCAM
#define TCAM_MAX		256

typedef struct {
	patch_hdr_t hdr;
	patch_entry_t ram_entry;
	patch_entry_t rom_entry;
	patch_hdr_t *hdr_addr;
	patch_entry_t *entry_addr;
	uint32 hidx;
	void   *hentry;
	uint32 eidx;
	uint32 tcam_val;
	uint32 nentries;
	uint32 fit;
	uint32 *staddr;
	uint32 *endaddr;
	uint32 tcam_entries[TCAM_MAX + 1];
} tcam_dump_t;

static tcam_dump_t tcam_dbg;
#endif /* BCMDBG_TCAM */

/* patch table region */
extern patch_entry_t _patch_table_start[], _patch_table_last[], _patch_table_end[];
/* control block region */
extern patch_hdr_t _patch_hdr_start[], _patch_hdr_end[];
extern char _patch_align_start[];

static uint32 *g_patchtable = NULL;
/*
 * FIX: create patch table for only entries needed
 * g_npatch <= total TCAM entries
 */
static uint16 g_npatch = 0;

extern uint8 patch_pair;

/* Patching mechanism is different between CR4 and old cores
 * In old arm core (cm3) the patching is handled by socram core
 * In cr4 as there is no socram the patching is handled by
 * a wrapper logic in arm itself. So all the registers for
 * patching are different between cm3 and cr4
 */

#if defined(__ARM_ARCH_7R__)

#define TCAM_WRDAT(m, p, v)		((p & m) | v)
#define ARMCR4_TCAM_MAX_ENTRY	256

void
BCMATTACHFN(hnd_tcam_write)(void *armp, uint16 idx, uint32 data)
{
	W_REG(SI_OSH, ARMREG(armp, tcamdatareg), data);
	W_REG(SI_OSH, ARMREG(armp, tcamcmdreg), (ARMCR4_TCAM_WRITE | idx));
	SPINWAIT(((R_REG(SI_OSH, ARMREG(armp, tcamcmdreg)) & ARMCR4_TCAM_CMD_DONE) == 0),
		ARMCR4_TCAM_CMD_DONE_DLY);
}

void
BCMATTACHFN(hnd_tcam_read)(void *armp, uint16 idx, uint32 *content)
{
	W_REG(SI_OSH, ARMREG(armp, tcamcmdreg), (ARMCR4_TCAM_READ | idx));
	SPINWAIT(((R_REG(SI_OSH, ARMREG(armp, tcamcmdreg)) & ARMCR4_TCAM_CMD_DONE) == 0),
		ARMCR4_TCAM_CMD_DONE_DLY);
	*content = R_REG(SI_OSH, ARMREG(armp, tcamdatareg));
}

void *
BCMATTACHFN(hnd_tcam_init)(void *armp, int no_addrs)
{
	uint32 *pa = NULL;
	int i, entries = 0;
	uint32 patchctrl;

	ASSERT(g_patchtable == NULL);
	if (g_patchtable != NULL)
		return NULL;

	entries = ARMCR4_TCAM_MAX_ENTRY;

	if (no_addrs == -1)
		no_addrs = entries;
	ASSERT(no_addrs <= entries);

	g_npatch = MIN(no_addrs, entries);
	if (entries) {
		uint size = (sizeof(uint) * (no_addrs * ARMCR4_PATCHNLOC));
#ifndef CONFIG_XIP
		/* table has to aligned based on tcam size * conecutive patch
		 * count * 4. Log of this is: 8 + ARMCR4_TCAMPATCHCOUNT + 2.
		 */
		uint align_bits = (8 + 2 + (ARMCR4_TCAMPATCHCOUNT));

		pa = (uint32 *)MALLOC_ALIGN(SI_OSH, size, align_bits);
		ASSERT(pa != NULL);
		if (pa == NULL)
			return NULL;
#else
		/* In case of bootloader fixing the table to begining ofATCM
		 * In case of 4335 the heap for bootloader will be present in
		 * BTCM. But patch table shoud be placed in ATCM. For this
		 * purpose initially the patchtable will put put at the
		 * begining of ATCM and the code will be downloaded after
		 * this table. Before start executing the firmware the
		 * bootloader will disable patch and copy the image back
		 * to this place.
		 */
		pa = (uint32 *)(_atcmrambase);
#endif /* CONFIG_XIP */
		bzero(pa, size);
		g_patchtable = (uint32 *) pa;

		/* Disable patching */
		hnd_tcam_disablepatch(armp);

		/* Program the patch count and enable clock */
		patchctrl = R_REG(SI_OSH, ARMREG(armp, tcampatchctrl));
		patchctrl &= ~(ARMCR4_TCAM_PATCHCNT_MASK);
		patchctrl |= (ARMCR4_TCAM_CLKENAB | ARMCR4_TCAMPATCHCOUNT);

		W_REG(SI_OSH, ARMREG(armp, tcampatchctrl), patchctrl);
		W_REG(SI_OSH, ARMREG(armp, tcampatchtblbaseaddr), (uint32)pa);

		/* initialize cam entries to zero */
		for (i = 0; i < entries; i++)
			hnd_tcam_write(armp, i, 0);
	}

	return (void *)pa;
}

/** Enable patching */
void
BCMATTACHFN(hnd_tcam_enablepatch)(void *armp)
{
	OR_REG(SI_OSH, ARMREG(armp, tcampatchctrl), ARMCR4_TCAM_ENABLE);
	/* Wait for write completion */
	SPINWAIT(((R_REG(SI_OSH, ARMREG(armp, tcampatchctrl)) & ARMCR4_TCAM_ENABLE) == 0),
		ARMCR4_TCAM_CMD_DONE_DLY);
}

/** Disable patching */
void
BCMATTACHFN(hnd_tcam_disablepatch)(void *armp)
{
	W_REG(SI_OSH, ARMREG(armp, tcampatchctrl), (~(ARMCR4_TCAM_ENABLE)
		& (R_REG(SI_OSH, ARMREG(armp, tcampatchctrl)))));
	/* Wait for write completion */
	SPINWAIT(((R_REG(SI_OSH, ARMREG(armp, tcampatchctrl)) & ARMCR4_TCAM_ENABLE)
		== ARMCR4_TCAM_ENABLE), ARMCR4_TCAM_CMD_DONE_DLY);
}

void
hnd_tcam_patchdisable(void)
{
	hnd_tcam_disablepatch((void *)arm_regs);
}

#if defined(BCMDBG_ASSERT) || defined(BCMROMOFFLOAD)
/** Get ROM size */
static uint32
BCMATTACHFN(hnd_tcam_romsize)(void *armp)
{
	uint32 cinfo, binfo;
	uint32 nbnk;
	uint32 romsz = 0, i;

	cinfo = R_REG(SI_OSH, ARMREG(armp, corecapabilities));
	nbnk = (cinfo & ARMCR4_ROMNB_MASK) >> ARMCR4_ROMNB_SHIFT;

	for (i = 0; i < nbnk; i++) {
		W_REG(SI_OSH, ARMREG(armp, bankidx), (ARMCR4_MT_ROM | i));
		binfo = R_REG(SI_OSH, ARMREG(armp, bankinfo));
		romsz += ((binfo & ARMCR4_BSZ_MASK) + 1) * ARMCR4_BSZ_MULT;
	}
	return romsz;
}
#endif /* BCMDBG_ASSERT || BCMROMOFFLOAD */

#if !defined(BCM_BOOTLOADER) && defined(BCMROMOFFLOAD)
static uint32
BCMATTACHFN(hnd_tcam_atcmsize)(void *armp)
{
	uint32 cinfo, binfo;
	uint32 nbnk;
	uint32 atcmsz = 0, i;

	cinfo = R_REG(SI_OSH, ARMREG(armp, corecapabilities));
	nbnk = (cinfo & ARMCR4_TCBANB_MASK) >> ARMCR4_TCBANB_SHIFT;

	for (i = 0; i < nbnk; i++) {
		W_REG(SI_OSH, ARMREG(armp, bankidx), (ARMCR4_MT_RAM | i));
		binfo = R_REG(SI_OSH, ARMREG(armp, bankinfo));
		atcmsz += ((binfo & ARMCR4_BSZ_MASK) + 1) * ARMCR4_BSZ_MULT;
	}
	return atcmsz;
}
#endif /* BCM_BOOTLOADER */

#ifdef CONFIG_XIP
void
BCMATTACHFN(hnd_tcam_bootloader_load)(void *armp, char *pvars)
{
	uint32 i;
	uint32 paddr;
	uint32 *pdataptr = NULL;

	pdataptr = (uint32 *)hnd_tcam_init(armp, patch_pair);
	if (pdataptr == NULL)
		return;

	ASSERT(hnd_tcam_romsize(armp) != 0);

	for (i = 0; i < patch_pair; i++) {
		char pa[8], pdh[8], pdl[8];

		sprintf(pa, "pa%d", i);
		sprintf(pdh, "pdh%d", i);
		sprintf(pdl, "pdl%d", i);

		pdataptr[i*2] = getintvar(pvars, pdl);
		pdataptr[i*2+1] = getintvar(pvars, pdh);

		paddr = (uint32) getintvar(pvars, pa);

		hnd_tcam_write(armp, i, TCAM_WRDAT(ARMCR4_TCAMADDR_MASK, paddr, 0x1));
	}

	hnd_tcam_enablepatch(armp);
}

#else
static bool
BCMATTACHFN(hnd_tcam_isenab)(void *armp)
{
	uint32 val;

	val = R_REG(SI_OSH, ARMREG(armp, tcampatchctrl));
	return ((val & ARMCR4_TCAM_ENABLE) != 0);
}

/** Find free entry in the cam */
static int
BCMATTACHFN(hnd_tcam_entry_get)(void *armp)
{
	int i;
	uint32 content;

	ASSERT(g_npatch);
	for (i = 0; i < g_npatch; i++) {
		hnd_tcam_read(armp, i, &content);
		if (content == 0)
			return i;
	}

	return -1;
}

/** Loads a patch pair */
void
BCMATTACHFN(hnd_tcam_load)(void *armp, const patchaddrvalue_t *patchtbl)
{
	uint32 pa;
	int entry, i;
	int offset;
	bool enab;

	ASSERT(g_patchtable);
	ASSERT(g_npatch);
	if ((g_patchtable == NULL))
		return;

	enab = hnd_tcam_isenab(armp);
	if (enab)
		hnd_tcam_disablepatch(armp);

	entry = hnd_tcam_entry_get(armp);
	if (entry < 0) {
		if (enab)
			hnd_tcam_enablepatch(armp);
		return;
	}

	ASSERT(hnd_tcam_romsize(armp) != 0);

	pa = patchtbl[0].addr;
	hnd_tcam_write(armp, entry, TCAM_WRDAT(ARMCR4_TCAMADDR_MASK, pa, 0x1));
	offset = entry * ARMCR4_PATCHNLOC;

	for (i = 0; i < ARMCR4_PATCHNLOC; i++)
		g_patchtable[offset++] = patchtbl[i].value;

	hnd_tcam_enablepatch(armp);
}

#ifdef BCMROMOFFLOAD
/**
 * Returns FALSE if patch entry crosses boundary and needs
 * an additional patch entry
 */
static bool
BCMATTACHFN(hnd_tcam_patentry_isfit)(patch_hdr_t *hdr)
{
	uint8 *paddr, *endaddr;

	endaddr = (uint8 *) PATBL_EEND(hdr->addr);
	paddr = (uint8 *) hdr->addr;

	if ((paddr + hdr->len) > endaddr)
		return FALSE;

	return TRUE;
}

static const char BCMATTACHDATA(rstr_TCAM_D_used_D_exceedD)[] = "TCAM: %d used: %d exceed:%d\n";
static void
BCMATTACHFN(hnd_tcam_stat)(void)
{
#ifdef BCMDBG_TCAM
	int eidx = PATHDR_CNT;
#endif /* BCMDBG_TCAM */
	patch_hdr_t *hdr;
	int i, tot, used;

	tot = ARMCR4_TCAM_MAX_ENTRY;

	hdr = PATHDR_TABLE;
	used = PATHDR_CNT;
	for (i = 0; i < PATHDR_CNT; i++, hdr++) {
		if (hnd_tcam_patentry_isfit(hdr) == FALSE) {
#ifdef BCMDBG_TCAM
			patch_entry_t *extra = PATBL_ENTRY(eidx++);
			TCAM_TRACE(("0x%x: %d(0x%p),%d(0x%p)\n", hdr->addr,
				PATBL_IDX(hdr->entry), hdr->entry,
				PATBL_IDX(extra), extra));
#endif /* BCMDBG_TCAM */
			used++;
		} else {
			TCAM_TRACE(("0x%x: %d(0x%p)\n", hdr->addr,
				PATBL_IDX(hdr->entry), hdr->entry));
		}
	}

	/* exceed: number of entries needing two patch entries */
	TCAM_OUT((rstr_TCAM_D_used_D_exceedD, tot, used, used - PATHDR_CNT));
}

#ifdef BCMDBG_TCAM
static void *
BCMATTACHFN(hnd_tcam_find_entry)(void *addr)
{
	patch_hdr_t *hdr;
	int i, n;

	hdr = PATHDR_TABLE;
	n = PATHDR_CNT;
	for (i = 0; i < n; i++, hdr++) {
		if (hdr->addr == addr)
			return hdr->entry;
	}

	return NULL;
}

static int
BCMATTACHFN(hnd_tcam_find_idx)(void *addr)
{
	patch_hdr_t *hdr;
	int i, n;

	hdr = PATHDR_TABLE;
	n = PATHDR_CNT;
	for (i = 0; i < n; i++, hdr++) {
		if (hdr->addr == addr)
			return PATBL_IDX(hdr->entry);
	}

	return -1;
}

static void
BCMATTACHFN(hnd_tcam_dump)(tcam_dump_t *info, patch_hdr_t *hdr, patch_entry_t *extra)
{
	cr4regs_t *armp = (cr4regs_t *) arm_regs;
	int len, i, n;
	patch_entry_t *entry;
	uint32 *src, *dst;

	entry = extra ? extra : hdr->entry;

	/* Need to disable first before reading */
	hnd_tcam_disablepatch((void *)armp);

	info->eidx = PATBL_IDX(entry);
	hnd_tcam_read((void *)armp, info->eidx, &info->tcam_val);

	n = MIN(PATBL_CNT, TCAM_MAX);
	for (i = 0; i < n; i++)
		hnd_tcam_read((void *)armp, i, &info->tcam_entries[i]);
	info->tcam_entries[n] = -1;

	/* Re-enable */
	hnd_tcam_enablepatch((void *)armp);

	info->hdr_addr = hdr;
	info->entry_addr = entry;
	info->fit = hnd_tcam_patentry_isfit(hdr);
	info->staddr = (uint32 *) PATBL_ESTART(hdr->addr);
	info->endaddr = (uint32 *) PATBL_EEND(hdr->addr);
	info->nentries = PATBL_CNT;
	info->hidx = hnd_tcam_find_idx(hdr->addr);
	info->hentry = hnd_tcam_find_entry(hdr->addr);

	/* Dump Patch Header */
	len = PATHDR_SIZE/4;
	src = (uint32 *) hdr;
	dst = (uint32 *) &info->hdr;
	while (len--)
		*dst++ = *src++;

	/* Dump RAM content */
	len = PATBL_ESIZE/4;
	src = (uint32 *) entry;
	dst = (uint32 *) &info->ram_entry;
	while (len--)
		*dst++ = *src++;

	/* Dump ROM content */
	len = PATBL_ESIZE/4;
	src = (uint32 *) info->staddr;
	dst = (uint32 *) &info->rom_entry;
	while (len--)
		*dst++ = *src++;
}
#endif /* BCMDBG_TCAM */

/**
 * Read back and verify
 * Early in startup w/o printf so check __watermark
 */
static void
BCMATTACHFN(hnd_tcam_verify)(void)
{
	patch_hdr_t *hdr;
	patch_entry_t *extra;
	uint8 *src, *dst;
	int i, n, len, eidx;

	eidx = PATHDR_CNT;
	hdr = PATHDR_TABLE;
	n = PATHDR_CNT;
	for (i = 0; i < n; i++, hdr++) {
		src = (uint8 *) hdr->entry->data;
		dst = (uint8 *) PATBL_ESTART(hdr->addr);
		len = PATBL_ESIZE;

		/* verify w/ byte access */
		while (len--) {
			if (*src++ != *dst++) {
#ifdef BCMDBG_TCAM
				hnd_tcam_dump(&tcam_dbg, hdr, NULL);
#endif // endif
				hndrte_die();
			}
		}

		/* If requires two entries, assume first available
		 * patch entry MUST pair with this patch header
		 */
		if (hnd_tcam_patentry_isfit(hdr) == FALSE) {
			extra = PATBL_ENTRY(eidx++);
			src = (uint8 *) extra->data;
			len = PATBL_ESIZE;
			while (len--) {
				if (*src++ != *dst++) {
#ifdef BCMDBG_TCAM
					hnd_tcam_dump(&tcam_dbg, hdr, extra);
#endif // endif
					hndrte_die();
				}
			}
		}
	}
}

static void
BCMATTACHFN(hnd_tcam_reset)(void *armp, void *ptbl)
{
	int i, tot;
	uint32 patchctrl;

	tot = ARMCR4_TCAM_MAX_ENTRY;

	hnd_tcam_disablepatch(armp);

	/* Set the clock and patch count */
	ASSERT(ARMCR4_TCAMPATCHCOUNT > 0);
	ASSERT(ARMCR4_TCAMPATCHCOUNT < 5);

	/* Write patch count and enable clock */
	patchctrl = R_REG(SI_OSH, ARMREG(armp, tcampatchctrl));
	patchctrl &= ~(ARMCR4_TCAM_PATCHCNT_MASK);
	patchctrl |= (ARMCR4_TCAM_CLKENAB | ARMCR4_TCAMPATCHCOUNT);

	W_REG(SI_OSH, ARMREG(armp, tcampatchctrl), patchctrl);

	W_REG(SI_OSH, ARMREG(armp, tcampatchtblbaseaddr), (uint32) ptbl);

	/* Initialize cam entries to zero */
	for (i = 0; i < tot; i++)
		hnd_tcam_write(armp, i, 0);
}

/**
 * Allocate a patch entry from the pre-allocated patch table
 */
static patch_entry_t *
BCMATTACHFN(hnd_tcam_patentry_alloc)(void)
{
	static patch_entry_t *next = _patch_table_last;

	if (next == _patch_table_end)
		return NULL;
	else
		return next++;
}

void
BCMATTACHFN(hnd_tcam_reclaim)()
{
	uint8 *as = (uint8 *) _patch_align_start;
	uint8 *hs = (uint8 *) _patch_hdr_start;
	uint8 *he = (uint8 *) _patch_hdr_end;
	uint8 *pf = (uint8 *) _patch_table_start;
	uint8 *ps = (uint8 *) hnd_tcam_patentry_alloc();
	uint8 *pe = (uint8 *) _patch_table_end;
	int asize = pf - as;
	int hsize = he - hs;
	int psize = pe - (ps ? ps : (uint8 *)_patch_table_end);

	/* Dump stats before reclaim */
	hnd_tcam_stat();

	/* Reclaim padding from alignment */
	if (asize) {
		bzero(as, asize);
		hndrte_arena_add((uint32)as, asize);
	}

	/* Reclaim patch header list */
	if (hsize) {
		bzero(hs, hsize);
		hndrte_arena_add((uint32)hs, hsize);
	}

	/* Reclaim unused patch entries from pre-allocated patch table */
	if (psize) {
		bzero(ps, psize);
		hndrte_arena_add((uint32)ps, psize);
	}
}

/**
 * Fill unused patch entry with ROM data
 * Don't use bcopy(), memcpy(), etc. which may need patching as well.
 */
static int
BCMATTACHFN(hnd_tcam_patentry_fixup)(patch_hdr_t *hdr, patch_entry_t *extra)
{
	uint8 *dest, *src;
	uint8 *staddr;
	uint8 *paddr;
	uint8 *endaddr;
	patch_entry_t etmp;
	int n, len, rem = 0;

	len = hdr->len;
	staddr = (uint8 *) PATBL_ESTART(hdr->addr);
	endaddr = (uint8 *) PATBL_EEND(hdr->addr);
	paddr = (uint8 *) hdr->addr;

	if (ISALIGNED((uintptr) hdr->addr, PATBL_ESIZE) && ((paddr + len) == endaddr)) {
		/* No fixup needed since entire entry is used. */
		return 0;
	}

	/* Save patch data before fixing up */
	dest = (uint8 *) etmp.data;
	src = (uint8 *) hdr->entry->data;
	n = len;
	while (n--)
		*dest++ = *src++;

	if ((paddr + len) > endaddr) {
		/* Exceeds patch entry boundary but no extra entry specified */
		if (extra == NULL)
			return BCME_ERROR;

		rem = (paddr + len) - endaddr;
		len -= rem;
	}

	/* Start rebuilding patch entry */
	dest = (uint8 *) hdr->entry->data;

	/* Front fill */
	src = staddr;
	n = paddr - staddr;
	while (n--)
		*dest++ = *src++;

	/* Restore saved data */
	src = (uint8 *) etmp.data;
	n = len;
	while (n--)
		*dest++ = *src++;

	/* Backfill */
	src = paddr + len;
	n = endaddr - src;
	while (n--)
		*dest++ = *src++;

	/* Handle boundary case */
	if (rem > 0) {
		/* Next patch entry */
		dest = (uint8 *) extra->data;

		/* Restore remaining saved data */
		src = (uint8 *) etmp.data + len;
		n = rem;
		while (n--)
			*dest++ = *src++;

		/* Backfill the rest */
		src = (uint8 *) endaddr + rem;
		n = PATBL_ESIZE - rem;
		while (n--)
			*dest++ = *src++;
	}

	return 0;
}

/**
 * Loads auto generated patch table immediately after startup
 * Each patch entry needs PATCHENTRY() and PATCHHDR().
 */
void
BCMATTACHFN(hnd_tcam_load_default)()
{
	cr4regs_t *armp = (cr4regs_t *) arm_regs;
	patch_hdr_t *hdr;
	patch_entry_t *extra;
	int i, n, tot;
	uint32 pa, mask, romsz;
	int tidx;

	if (((uint32)PATBL_TABLE < hndrte_get_rambase()) ||
		((uint32)PATBL_TABLE + (ARMCR4_TCAM_MAX_ENTRY * 4 * ARMCR4_PATCHNLOC)) >
		(hndrte_get_rambase() + hnd_tcam_atcmsize((void *)armp)))
		hndrte_die();

	hnd_tcam_reset((void *)armp, PATBL_TABLE);

	tot = ARMCR4_TCAM_MAX_ENTRY;

	n = PATHDR_CNT;
	if (n == 0)
		return;

	if (n > tot)
		hndrte_die();

	romsz = hnd_tcam_romsize((void *)armp);
	ASSERT(romsz);
	mask = ARMCR4_TCAMADDR_MASK;

	hdr = PATHDR_TABLE;
	for (i = 0; i < n; i++, hdr++) {
		if ((hdr->len == 0) || (hdr->len > PATBL_ESIZE) || (hdr->entry == NULL))
			hndrte_die();
		if ((uint32)hdr->addr >= romsz)
		    hndrte_die();

		extra = NULL;
		if (hnd_tcam_patentry_isfit(hdr) == FALSE) {
			/* Need additional patch entry */
			extra = hnd_tcam_patentry_alloc();
			if (extra == NULL)
				hndrte_die();
		}

		if (hnd_tcam_patentry_fixup(hdr, extra) != 0)
			hndrte_die();

		tidx = PATBL_IDX(hdr->entry);
		pa = (uint32) PATBL_ESTART(hdr->addr);

		hnd_tcam_write((void *)armp, tidx, TCAM_WRDAT(mask, pa, 0x1));

		if (extra) {
			pa = (uint32) PATBL_EEND(hdr->addr);
			hnd_tcam_write((void *)armp, PATBL_IDX(extra), TCAM_WRDAT(mask, pa, 0x1));
		}
	}

	hnd_tcam_enablepatch((void *)armp);

	hnd_tcam_verify();
}
#endif /* BCMROMOFFLOAD */
#endif /* CONFIG_XIP */
#else /* ! __ARM_ARCH_7R__ below */

#define TCAM_ADDR_MASK(rs)		((rs) - 4)
#define TCAM_ADDR_VALID(rs)		((socram_rev >= 15) ? 0x1 : (rs) >> 2)
#define TCAM_WRDAT(m, p, v)		((socram_rev >= 15) ? \
					((p) | (v)) : ((((m) & (p)) >> 2) | (v)))

void
BCMATTACHFN(hnd_tcam_write)(void *srp, uint16 idx, uint32 data)
{
	sbsocramregs_t *sr = (sbsocramregs_t *)srp;
	W_REG(SI_OSH, &sr->cambankdatareg, data);
	W_REG(SI_OSH, &sr->cambankcmdreg, (SRCMD_WRITE | idx));
	SPINWAIT(((R_REG(SI_OSH, &sr->cambankcmdreg) & SRCMD_DONE) == 0),
		SRCMD_DONE_DLY);
}

void
BCMATTACHFN(hnd_tcam_read)(void *srp, uint16 idx, uint32 *content)
{
	sbsocramregs_t *sr = (sbsocramregs_t *)srp;
	W_REG(SI_OSH, &sr->cambankcmdreg, (SRCMD_READ | idx));
	SPINWAIT(((R_REG(SI_OSH, &sr->cambankcmdreg) & SRCMD_DONE) == 0),
		SRCMD_DONE_DLY);
	*content = R_REG(SI_OSH, &sr->cambankdatareg);
}

extern uint8 patch_pair;
void *
BCMATTACHFN(hnd_tcam_init)(void *srp, int no_addrs)
{
	sbsocramregs_t *sr = (sbsocramregs_t *)srp;
	uint32 *pa = NULL;
	int i, entries = 0;

	ASSERT(g_patchtable == NULL);
	if (g_patchtable != NULL)
		return NULL;

	entries = SRECC_BANKSIZE((R_REG(SI_OSH, &sr->extracoreinfo)
		& SRECC_BANKSIZE_MASK));

	if (no_addrs == -1)
		no_addrs = entries;
	ASSERT(no_addrs <= entries);

	g_npatch = MIN(no_addrs, entries);
	if (entries) {
		uint size = (sizeof(uint) * (no_addrs * SRPC_PATCHNLOC));
		uint align_bits = (6 + 2 + (SRPC_PATCHCOUNT));

		/* SRPC_PATCHNLOC will be 1 for bootloader */
		/* table has to aligned based on tcam size * conecutive patch
		 * count * 4. Log of this is: 6 + SRPC_PATCHCOUNT + 2.
		 */
		pa = (uint32 *)MALLOC_ALIGN(SI_OSH, size, align_bits);
		ASSERT(pa != NULL);
		if (pa == NULL)
			return NULL;

		bzero(pa, size);
		g_patchtable = (uint32 *) pa;

		/* Disable patching */
		hnd_tcam_disablepatch(srp);

		/* Just program the patch count here */
		W_REG(SI_OSH, &sr->cambankpatchctrl, (SRPC_PATCHCOUNT));
		W_REG(SI_OSH, &sr->cambankpatchtblbaseaddr, (uint32)pa);

		/* initialize cam entries to zero */
		for (i = 0; i < entries; i++)
			hnd_tcam_write(srp, i, 0);
	}

	return (void *)pa;
}

/** Enable patching */
void
BCMATTACHFN(hnd_tcam_enablepatch)(void *srp)
{
	sbsocramregs_t *sr = (sbsocramregs_t *)srp;
	OR_REG(SI_OSH, &sr->cambankpatchctrl, SRCBPC_PATCHENABLE);
}

/** Disable patching */
void
BCMATTACHFN(hnd_tcam_disablepatch)(void *srp)
{
	sbsocramregs_t *sr = (sbsocramregs_t *)srp;
	W_REG(SI_OSH, &sr->cambankpatchctrl, (~(SRCBPC_PATCHENABLE)
		& (R_REG(SI_OSH, &sr->cambankpatchctrl))));
}

void
hnd_tcam_patchdisable(void)
{
	hnd_tcam_disablepatch((void *)socram_regs);
}

/** Get ROM size */
static uint32
BCMATTACHFN(hnd_tcam_romsize)(sbsocramregs_t *sr)
{
	uint32 cinfo, binfo;
	uint32 nbnk, bnksz;
	uint32 romsz = 0, i;

	cinfo = R_REG(SI_OSH, &sr->coreinfo);
	nbnk = (cinfo & SRCI_ROMNB_MASK) >> SRCI_ROMNB_SHIFT;

	if ((socram_rev >= 3) && (socram_rev < 8)) {
		bnksz = (cinfo & SRCI_ROMBSZ_MASK) >> SRCI_ROMBSZ_SHIFT;
		romsz = (1 << (bnksz + 14)); /* 2 ** (bnksz + 14) */
		romsz = nbnk * romsz;
	} else if (socram_rev >= 8) {
		for (i = 0; i < nbnk; i++) {
			W_REG(SI_OSH, &sr->bankidx,
				(SOCRAM_MEMTYPE_R0M << SOCRAM_BANKIDX_MEMTYPE_SHIFT) | i);
			binfo = R_REG(SI_OSH, &sr->bankinfo);
			ASSERT(binfo & SOCRAM_BANKIDX_ROM_MASK);

			bnksz = binfo & SOCRAM_BANKINFO_SZMASK;
			romsz += (bnksz + 1) * SOCRAM_BANKINFO_SZBASE;
		}
	}

	return romsz;
}

#ifdef CONFIG_XIP
void
BCMATTACHFN(hnd_tcam_bootloader_load)(void *srp, char *pvars)
{
	uint32 i;
	uint32 paddr;
	uint32 *pdataptr = NULL;
	uint32 romsz, valid = 0, mask = 0;

	pdataptr = (uint32 *)hnd_tcam_init(srp, patch_pair);
	if (pdataptr == NULL)
		return;

	romsz = hnd_tcam_romsize(srp);
	ASSERT(romsz);
	mask = TCAM_ADDR_MASK(romsz);
	valid = TCAM_ADDR_VALID(romsz);

	for (i = 0; i < patch_pair; i++) {
		char pa[8], pd[8];

		sprintf(pa, "pa%d", i);
		sprintf(pd, "pd%d", i);

		pdataptr[i] = getintvar(pvars, pd);
		paddr = (uint32) getintvar(pvars, pa);

		hnd_tcam_write(srp, i, TCAM_WRDAT(mask, paddr, valid));
	}

	hnd_tcam_enablepatch(srp);
}

#else
static bool
BCMATTACHFN(hnd_tcam_isenab)(sbsocramregs_t *sr)
{
	uint32 val;

	val = R_REG(SI_OSH, &sr->cambankpatchctrl);
	return ((val & SRCBPC_PATCHENABLE) != 0);
}

/** Find free entry in the cam */
static int
BCMATTACHFN(hnd_tcam_entry_get)(sbsocramregs_t *sr)
{
	int i;
	uint32 content;

	ASSERT(g_npatch);
	for (i = 0; i < g_npatch; i++) {
		hnd_tcam_read((void *)sr, i, &content);
		if (content == 0)
			return i;
	}

	return -1;
}

/** Loads a patch pair */
void
BCMATTACHFN(hnd_tcam_load)(void *srp, const patchaddrvalue_t *patchtbl)
{
	sbsocramregs_t *sr = (sbsocramregs_t *)srp;
	uint32 pa, valid, mask;
	int entry, i;
	int offset, romsz;
	bool enab;

	ASSERT(g_patchtable);
	ASSERT(g_npatch);
	if ((socram_rev < 3) || (g_patchtable == NULL))
		return;

	enab = hnd_tcam_isenab(sr);
	if (enab)
		hnd_tcam_disablepatch(srp);

	entry = hnd_tcam_entry_get(sr);
	if (entry < 0) {
		if (enab)
			hnd_tcam_enablepatch(srp);
		return;
	}

	romsz = hnd_tcam_romsize(sr);
	ASSERT(romsz);
	mask = TCAM_ADDR_MASK(romsz);
	valid = TCAM_ADDR_VALID(romsz);

	pa = patchtbl[0].addr;
	hnd_tcam_write(srp, entry, TCAM_WRDAT(mask, pa, valid));
	offset = entry * SRPC_PATCHNLOC;

	for (i = 0; i < SRPC_PATCHNLOC; i++)
		g_patchtable[offset++] = patchtbl[i].value;

	hnd_tcam_enablepatch(srp);
}

#ifdef BCMROMOFFLOAD
/**
 * Returns FALSE if patch entry crosses boundary and needs
 * an additional patch entry
 */
static bool
BCMATTACHFN(hnd_tcam_patentry_isfit)(patch_hdr_t *hdr)
{
	uint8 *paddr, *endaddr;

	endaddr = (uint8 *) PATBL_EEND(hdr->addr);
	paddr = (uint8 *) hdr->addr;

	if ((paddr + hdr->len) > endaddr)
		return FALSE;

	return TRUE;
}

static void
BCMATTACHFN(hnd_tcam_stat)(void)
{
#ifdef BCMDBG_TCAM
	int eidx = PATHDR_CNT;
#endif /* BCMDBG_TCAM */
	sbsocramregs_t *srp = (sbsocramregs_t *) socram_regs;
	patch_hdr_t *hdr;
	int i, tot, used;

	tot = SRECC_BANKSIZE((R_REG(SI_OSH, &srp->extracoreinfo)
		& SRECC_BANKSIZE_MASK));

	hdr = PATHDR_TABLE;
	used = PATHDR_CNT;
	for (i = 0; i < PATHDR_CNT; i++, hdr++) {
		if (hnd_tcam_patentry_isfit(hdr) == FALSE) {
#ifdef BCMDBG_TCAM
			patch_entry_t *extra = PATBL_ENTRY(eidx++);
			TCAM_TRACE(("0x%x: %d(0x%p),%d(0x%p)\n", hdr->addr,
				PATBL_IDX(hdr->entry), hdr->entry,
				PATBL_IDX(extra), extra));
#endif /* BCMDBG_TCAM */
			used++;
		} else {
			TCAM_TRACE(("0x%x: %d(0x%p)\n", hdr->addr,
				PATBL_IDX(hdr->entry), hdr->entry));
		}
	}

	/* exceed: number of entries needing two patch entries */
	TCAM_OUT(("TCAM: %d used: %d exceed:%d\n", tot, used, used - PATHDR_CNT));
}

#ifdef BCMDBG_TCAM
static void *
BCMATTACHFN(hnd_tcam_find_entry)(void *addr)
{
	patch_hdr_t *hdr;
	int i, n;

	hdr = PATHDR_TABLE;
	n = PATHDR_CNT;
	for (i = 0; i < n; i++, hdr++) {
		if (hdr->addr == addr)
			return hdr->entry;
	}

	return NULL;
}

static int
BCMATTACHFN(hnd_tcam_find_idx)(void *addr)
{
	patch_hdr_t *hdr;
	int i, n;

	hdr = PATHDR_TABLE;
	n = PATHDR_CNT;
	for (i = 0; i < n; i++, hdr++) {
		if (hdr->addr == addr)
			return PATBL_IDX(hdr->entry);
	}

	return -1;
}

static void
BCMATTACHFN(hnd_tcam_dump)(tcam_dump_t *info, patch_hdr_t *hdr, patch_entry_t *extra)
{
	sbsocramregs_t *srp = (sbsocramregs_t *) socram_regs;
	int len, i, n;
	patch_entry_t *entry;
	uint32 *src, *dst;

	entry = extra ? extra : hdr->entry;

	/* Need to disable first before reading */
	hnd_tcam_disablepatch((void *)srp);

	info->eidx = PATBL_IDX(entry);
	hnd_tcam_read((void *)srp, info->eidx, &info->tcam_val);

	n = MIN(PATBL_CNT, TCAM_MAX);
	for (i = 0; i < n; i++)
		hnd_tcam_read((void *)srp, i, &info->tcam_entries[i]);
	info->tcam_entries[n] = -1;

	/* Re-enable */
	hnd_tcam_enablepatch((void *)srp);

	info->hdr_addr = hdr;
	info->entry_addr = entry;
	info->fit = hnd_tcam_patentry_isfit(hdr);
	info->staddr = (uint32 *) PATBL_ESTART(hdr->addr);
	info->endaddr = (uint32 *) PATBL_EEND(hdr->addr);
	info->nentries = PATBL_CNT;
	info->hidx = hnd_tcam_find_idx(hdr->addr);
	info->hentry = hnd_tcam_find_entry(hdr->addr);

	/* Dump Patch Header */
	len = PATHDR_SIZE/4;
	src = (uint32 *) hdr;
	dst = (uint32 *) &info->hdr;
	while (len--)
		*dst++ = *src++;

	/* Dump RAM content */
	len = PATBL_ESIZE/4;
	src = (uint32 *) entry;
	dst = (uint32 *) &info->ram_entry;
	while (len--)
		*dst++ = *src++;

	/* Dump ROM content */
	len = PATBL_ESIZE/4;
	src = (uint32 *) info->staddr;
	dst = (uint32 *) &info->rom_entry;
	while (len--)
		*dst++ = *src++;
}
#endif /* BCMDBG_TCAM */

/**
 * Read back and verify
 * Early in startup w/o printf so check __watermark
 */
static void
BCMATTACHFN(hnd_tcam_verify)(void)
{
	patch_hdr_t *hdr;
	patch_entry_t *extra;
	uint8 *src, *dst;
	int i, n, len, eidx;

	eidx = PATHDR_CNT;
	hdr = PATHDR_TABLE;
	n = PATHDR_CNT;
	for (i = 0; i < n; i++, hdr++) {
		src = (uint8 *) hdr->entry->data;
		dst = (uint8 *) PATBL_ESTART(hdr->addr);
		len = PATBL_ESIZE;

		/* verify w/ byte access */
		while (len--) {
			if (*src++ != *dst++) {
#ifdef BCMDBG_TCAM
				hnd_tcam_dump(&tcam_dbg, hdr, NULL);
#endif // endif
				hndrte_die();
			}
		}

		/* If requires two entries, assume first available
		 * patch entry MUST pair with this patch header
		 */
		if (hnd_tcam_patentry_isfit(hdr) == FALSE) {
			extra = PATBL_ENTRY(eidx++);
			src = (uint8 *) extra->data;
			len = PATBL_ESIZE;
			while (len--) {
				if (*src++ != *dst++) {
#ifdef BCMDBG_TCAM
					hnd_tcam_dump(&tcam_dbg, hdr, extra);
#endif // endif
					hndrte_die();
				}
			}
		}
	}
}

static void
BCMATTACHFN(hnd_tcam_reset)(sbsocramregs_t *sr, void *ptbl)
{
	int i, tot;

	tot = SRECC_BANKSIZE((R_REG(SI_OSH, &sr->extracoreinfo)
		& SRECC_BANKSIZE_MASK));

	hnd_tcam_disablepatch((void *) sr);

	W_REG(SI_OSH, &sr->cambankpatchctrl, SRPC_PATCHCOUNT);
	W_REG(SI_OSH, &sr->cambankpatchtblbaseaddr, (uint32) ptbl);

	/* Initialize cam entries to zero */
	for (i = 0; i < tot; i++)
		hnd_tcam_write((void *) sr, i, 0);
}

/** Allocate a patch entry from the pre-allocated patch table */
static patch_entry_t *
BCMATTACHFN(hnd_tcam_patentry_alloc)(void)
{
	static patch_entry_t *next = _patch_table_last;

	if (next == _patch_table_end)
		return NULL;
	else
		return next++;
}

void
BCMATTACHFN(hnd_tcam_reclaim)()
{
	uint8 *as = (uint8 *) _patch_align_start;
	uint8 *hs = (uint8 *) _patch_hdr_start;
	uint8 *he = (uint8 *) _patch_hdr_end;
	uint8 *pf = (uint8 *) _patch_table_start;
	uint8 *ps = (uint8 *) hnd_tcam_patentry_alloc();
	uint8 *pe = (uint8 *) _patch_table_end;
	int asize = pf - as;
	int hsize = he - hs;
	int psize = pe - (ps ? ps : (uint8 *)_patch_table_end);

	/* Dump stats before reclaim */
	hnd_tcam_stat();

	/* Reclaim padding from alignment */
	if (asize) {
		bzero(as, asize);
		hndrte_arena_add((uint32)as, asize);
	}

	/* Reclaim patch header list */
	if (hsize) {
		bzero(hs, hsize);
		hndrte_arena_add((uint32)hs, hsize);
	}

	/* Reclaim unused patch entries from pre-allocated patch table */
	if (psize) {
		bzero(ps, psize);
		hndrte_arena_add((uint32)ps, psize);
	}
}

/**
 * Fill unused patch entry with ROM data
 * Don't use bcopy(), memcpy(), etc. which may need patching as well.
 */
static int
BCMATTACHFN(hnd_tcam_patentry_fixup)(patch_hdr_t *hdr, patch_entry_t *extra)
{
	uint8 *dest, *src;
	uint8 *staddr;
	uint8 *paddr;
	uint8 *endaddr;
	patch_entry_t etmp;
	int n, len, rem = 0;

	len = hdr->len;
	staddr = (uint8 *) PATBL_ESTART(hdr->addr);
	endaddr = (uint8 *) PATBL_EEND(hdr->addr);
	paddr = (uint8 *) hdr->addr;

	if (ISALIGNED((uintptr) hdr->addr, PATBL_ESIZE) && ((paddr + len) == endaddr)) {
		/* No fixup needed since entire entry is used. */
		return 0;
	}

	/* Save patch data before fixing up */
	dest = (uint8 *) etmp.data;
	src = (uint8 *) hdr->entry->data;
	n = len;
	while (n--)
		*dest++ = *src++;

	if ((paddr + len) > endaddr) {
		/* Exceeds patch entry boundary but no extra entry specified */
		if (extra == NULL)
			return BCME_ERROR;

		rem = (paddr + len) - endaddr;
		len -= rem;
	}

	/* Start rebuilding patch entry */
	dest = (uint8 *) hdr->entry->data;

	/* Front fill */
	src = staddr;
	n = paddr - staddr;
	while (n--)
		*dest++ = *src++;

	/* Restore saved data */
	src = (uint8 *) etmp.data;
	n = len;
	while (n--)
		*dest++ = *src++;

	/* Backfill */
	src = paddr + len;
	n = endaddr - src;
	while (n--)
		*dest++ = *src++;

	/* Handle boundary case */
	if (rem > 0) {
		/* Next patch entry */
		dest = (uint8 *) extra->data;

		/* Restore remaining saved data */
		src = (uint8 *) etmp.data + len;
		n = rem;
		while (n--)
			*dest++ = *src++;

		/* Backfill the rest */
		src = (uint8 *) endaddr + rem;
		n = PATBL_ESIZE - rem;
		while (n--)
			*dest++ = *src++;
	}

	return 0;
}

/**
 * Loads auto generated patch table immediately after startup
 * Each patch entry needs PATCHENTRY() and PATCHHDR().
 */
void
BCMATTACHFN(hnd_tcam_load_default)()
{
	sbsocramregs_t *srp = (sbsocramregs_t *) socram_regs;
	patch_hdr_t *hdr;
	patch_entry_t *extra;
	int i, n, tot;
	uint32 pa, valid, mask;
	int tidx, romsz;

	hnd_tcam_reset(srp, PATBL_TABLE);

	tot = SRECC_BANKSIZE((R_REG(SI_OSH, &srp->extracoreinfo)
		& SRECC_BANKSIZE_MASK));

	n = PATHDR_CNT;
	if (n == 0)
		return;

	if (n > tot)
		hndrte_die();

	romsz = hnd_tcam_romsize(srp);
	ASSERT(romsz);
	mask = TCAM_ADDR_MASK(romsz);
	valid = TCAM_ADDR_VALID(romsz);

	hdr = PATHDR_TABLE;
	for (i = 0; i < n; i++, hdr++) {
		if ((hdr->len == 0) || (hdr->len > PATBL_ESIZE) || (hdr->entry == NULL))
			hndrte_die();

		extra = NULL;
		if (hnd_tcam_patentry_isfit(hdr) == FALSE) {
			/* Need additional patch entry */
			extra = hnd_tcam_patentry_alloc();
			if (extra == NULL)
				hndrte_die();
		}

		if (hnd_tcam_patentry_fixup(hdr, extra) != 0)
			hndrte_die();

		tidx = PATBL_IDX(hdr->entry);
		pa = (uint32) PATBL_ESTART(hdr->addr);

		hnd_tcam_write((void *)srp, tidx, TCAM_WRDAT(mask, pa, valid));

		if (extra) {
			pa = (uint32) PATBL_EEND(hdr->addr);
			hnd_tcam_write((void *)srp, PATBL_IDX(extra), TCAM_WRDAT(mask, pa, valid));
		}
	}

	hnd_tcam_enablepatch((void *)srp);

	hnd_tcam_verify();
}
#endif /* BCMROMOFFLOAD */
#endif /* CONFIG_XIP */
#endif /* defined(__ARM_ARCH_7R__) */
