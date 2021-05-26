/*
 * 802.11ax MU scheduler and scheduler statistics module.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 */

#ifdef WL_MUSCHEDULER

/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <802.11.h>
#include <wl_dbg.h>
#include <wlc_types.h>
#include <wlc_pub.h>
#include <wlc.h>
#include <wlc_dbg.h>
#include <wlc_dump.h>
#include <wlc_iocv_cmd.h>
#include <wlc_bsscfg.h>
#include <wlc_scb.h>
#include <wlc_musched.h>
#include <wlc_ht.h>
#include <wlc_he.h>
#include <wlc_scb_ratesel.h>
#include <wlc_bmac.h>
#include <wlc_hw_priv.h>
#include <wlc_ratelinkmem.h>
#include <wlc_lq.h>
#include <wlc_ampdu_cmn.h>
#include <wlc_stf.h>
#include <wlc_txbf.h>
#if defined(TESTBED_AP_11AX) || defined(BCMDBG)
#include <wlc_tx.h>
#endif /* TESTBED_AP_11AX || BCMDBG */
#include <wlc_macreq.h>
#include <wlc_txcfg.h>
#include <wlc_mutx.h>
#include <wlc_fifo.h>
#include <wlc_ulmu.h>
#ifdef WLTAF
#include <wlc_taf.h>
#endif // endif

/* forward declaration */
#define MUSCHED_RUCFG_ROW		16
#define MUSCHED_RUCFG_COL		16
#define MUSCHED_RU_SCB_STATS_NUM	16
#define MUSCHED_RU_IDX_NUM		HE_MAX_2x996_TONE_RU_INDX /* 68 */
#define MUSCHED_RU_IDX_MASK		0x7f
#define MUSCHED_RU_IDX(ru)		((ru) & MUSCHED_RU_IDX_MASK)
#define MUSCHED_RU_UPPER_MASK		0x80
#define MUSCHED_RU_UPPER_SHIFT		7
#define MUSCHED_RU_UPPER(ru)	\
	(((ru) & MUSCHED_RU_UPPER_MASK) >> MUSCHED_RU_UPPER_SHIFT)

#define MUSCHED_RUCFG_LE8_NUM_ROWS	8
#define MUSCHED_RUCFG_GT8_NUM_ROWS	8
#define MUSCHED_RUCFG_LE8_NUM_COLS	8
#define MUSCHED_RUCFG_GT8_NUM_COLS	16
#define MUSCHED_RUCFG_GT8_STRT_IDX	8
#define MUSCHED_RUCFG_ACK_SHIFT		8

/* max allowed minRU setting for BW20, BW40, BW80, BW160 */
static const uint8 per_bw_max_minru[D11_REV128_BW_SZ] =
	{
		D11AX_RU242_TYPE, /* BW20 */
		D11AX_RU484_TYPE, /* BW40 */
		D11AX_RU996_TYPE, /* BW80 */
		D11AX_RU1992_TYPE /* BW160 */
	};

/* Internal use by ucode for vht mumimo: */
#define MUSCHED_RUCFG_RSVD0_RTMEM		AMT_IDX_VHTMU_RSVD_START
#define MUSCHED_RUCFG_RSVD1_RTMEM		(AMT_IDX_VHTMU_RSVD_START + 1)
/* Used for DL OFDMA rates: */
#define MUSCHED_RUCFG_LE8_BW20_40_RTMEM		AMT_IDX_DLOFDMA_RSVD_START
#define MUSCHED_RUCFG_LE8_BW80_160_RTMEM	(AMT_IDX_DLOFDMA_RSVD_START + 1)
#define MUSCHED_RUCFG_GT8_BW20_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 2)
#define MUSCHED_RUCFG_GT8_BW40_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 3)
#define MUSCHED_RUCFG_GT8_BW80_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 4)
#define MUSCHED_RUCFG_GT8_BW160_RTMEM		(AMT_IDX_DLOFDMA_RSVD_START + 5)

#define MUSCHED_RUCFG_TBL_WSZ		(MUSCHED_RUCFG_GT8_NUM_ROWS * MUSCHED_RUCFG_GT8_NUM_COLS)
#define MUSCHED_RUCFG_TBL_BSZ		(MUSCHED_RUCFG_TBL_WSZ * 2) // 256B

#define MUSCHED_26TONE_RU4	4 // 26-tone-ru 4
#define MUSCHED_26TONE_RU13	13 // 26-tone-ru 13
#define MUSCHED_26TONE_RU18	18 // center-26-tone-ru 18
#define MUSCHED_26TONE_RU23	23 // 26-tone-ru 23
#define MUSCHED_26TONE_RU32	32 // 26-tone-ru 32

/* for dl ofdma */
#define MUSCHED_DLOFDMA_MINUSER_SZ		2	/* 0: <=80 MHz 1: 160MHz */
#define MUSCHED_DLOFDMA_BW20_MAX_NUM_USERS	8	/* Default DL OFDMA BW20 max clients */
#define MUSCHED_DLOFDMA_MAX_NUM_USERS		16	/* Default DL OFDMA max clients */

#define OPERAND_SHIFT			4
#define MUSCHED_TRIGFMT			16 // trigger frame types
#define MAX_USRHIST_PERLINE		8
#define AVG_ALHA_SHIFT			3
#define AVG_ALPHA_MUL			((1 << AVG_ALHA_SHIFT) - 1)

typedef struct musched_ru_stats {
	uint32	tx_cnt[MUSCHED_RU_TYPE_NUM]; /* total tx cnt per ru size */
	uint32	txsucc_cnt[MUSCHED_RU_TYPE_NUM]; /* succ tx cnt per ru size */
	uint8	ru_idx_use_bmap[MUSCHED_RU_BMP_ROW_SZ][MUSCHED_RU_BMP_COL_SZ];
} musched_ru_stats_t;

/* module info */
struct wlc_muscheduler_info {
	wlc_info_t *wlc;
	uint16	flags;
	int	scbh;
	int16	dl_policy;
	int8	dl_schidx; /* decided by dl_policy; internal used by ucode */
	int8	rualloc; /* rualloc mode */
	int8	ack_policy;
	bool	mix_ackp; /* 1: allow mixed ackp0 and ackp1. 0: disallow */
	uint8	lowat[D11_REV128_BW_SZ];
	uint8	maxn[D11_REV128_BW_SZ];
	uint8	rucfg[D11_REV128_BW_SZ][MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
	uint8	rucfg_ack[D11_REV128_BW_SZ][MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
	bool	rucfg_fixed; /* = TRUE: fixed/set by iovar */
	uint8	use_murts;
	uint16	tmout;
	int16	num_scb_stats;
	musched_ru_stats_t ru_stats;
	int16	num_dlofdma_users;
	uint16	min_dlofdma_users[MUSCHED_DLOFDMA_MINUSER_SZ]; /* min users to enable dlofdma */
	bool	mixbw;		/* TRUE: enabled; FALSE: disabled */
	bool	wfa20in80;	/* fixed RU alloc WAR for WFA test (HE-4.69.1) */
};

#define MUSCHED_DLOFDMA_CLIENTS_MAX	0xFFFF

#define MUSCHED_DLOFDMA_MASK			0x0001
#define MUSCHED_DLOFDMA_SHIFT			0
#define MUSCHED_AUTO_DLSCH_MASK			0x0002

#define MUSCHED_DLOFDMA(a)		((a)->flags & MUSCHED_DLOFDMA_MASK)
#define MUSCHED_DLOFDMA_ENABLE(a)	((a)->flags |= MUSCHED_DLOFDMA_MASK)
#define MUSCHED_DLOFDMA_DISABLE(a)	((a)->flags &= ~MUSCHED_DLOFDMA_MASK)
#define MUSCHED_AUTO_DLSCH(a)		((a)->flags & MUSCHED_AUTO_DLSCH_MASK)
#define MUSCHED_AUTO_DLSCH_ENABLE(a)	((a)->flags |= MUSCHED_AUTO_DLSCH_MASK)
#define MUSCHED_AUTO_DLSCH_DISABLE(a)	((a)->flags &= ~MUSCHED_AUTO_DLSCH_MASK)

#define MUSCHED_HEMSCH_SCHIDX_MASK	0x0003
#define MUSCHED_HEMSCH_STP_MASK		0x001C
#define MUSCHED_HEMSCH_MURTS_MASK	0x0060
#define MUSCHED_HEMSCH_SCHIDX_SHIFT	0
#define MUSCHED_HEMSCH_STP_SHIFT	2
#define MUSCHED_HEMSCH_MURTS_SHIFT	5
#define MUSCHED_HEMSCH_STP(a)		((a & MUSCHED_HEMSCH_STP_MASK) >> MUSCHED_HEMSCH_STP_SHIFT)
#define MUSCHED_HEMSCH_MURTS(a)		((a & MUSCHED_HEMSCH_MURTS_MASK) >> \
		MUSCHED_HEMSCH_MURTS_SHIFT)

#define MUSCHED_ACKPOLICY_SERIAL	0
#define MUSCHED_ACKPOLICY_TRIGINAMP	1
#define MUSCHED_ACKPOLICY_MUBAR		2
#define MUSCHED_ACKPOLICY_MAX		(MUSCHED_ACKPOLICY_MUBAR)

#define MUSCHED_MURTS_DISABLE		0 // disable mu-rts
#define MUSCHED_MURTS_STATIC		1 // turn on mu-rts statically
#define MUSCHED_MURTS_DYNAMIC		2 // turn on mu-rts dynamically if a client is in dyn-SMPS
#define MUSCHED_MURTS_RSVD		3 // reserved
#define MUSCHED_MURTS_DFLT		(MUSCHED_MURTS_DYNAMIC)
#define MUSCHED_TRVLSCH_TMOUT_DFLT		128

#define MUSCHED_TRVLSCH_LOWAT_DFLT		4
#define MUSCHED_TRVLSCH_MINN_DFLT		2

#define MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS	0

/* scb cubby */
typedef struct {
	musched_ru_stats_t *scb_ru_stats; /* pointer to ru usage stats */
	int8 dl_schpos;		/* HEMU DL scheduler pos */
	bool dlul_assoc;	/* eligible on / off, determined on (de}assoc/auth */
	uint8 min_ru;
} scb_musched_t;

/* cubby access macros */
#define SCB_MUSCHED_CUBBY(musched, scb)	(scb_musched_t **)SCB_CUBBY(scb, (musched)->scbh)
#define SCB_MUSCHED(musched, scb)	*SCB_MUSCHED_CUBBY(musched, scb)
#define SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, on) \
	do { \
		if (on == TRUE && musched_scb->dlul_assoc == FALSE) { \
			ASSERT(musched->num_dlofdma_users < wlc->pub->tunables->maxscb); \
			musched->num_dlofdma_users++; \
			musched_scb->dlul_assoc = TRUE; \
		} else if (on == FALSE && musched_scb->dlul_assoc == TRUE) { \
			ASSERT(musched->num_dlofdma_users > 0); \
			musched->num_dlofdma_users--; \
			musched_scb->dlul_assoc = FALSE; \
		} \
	} while (0)

/* local declarations */

/* wlc module */
static int wlc_muscheduler_wlc_init(void *ctx);
static int wlc_muscheduler_doiovar(void *context, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, wlc_if_t *wlcif);
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static int wlc_musched_dump(void *ctx, bcmstrbuf_t *b);
static int wlc_musched_dump_clr(void *ctx);
static void wlc_musched_dump_ru_stats(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b);
static void wlc_musched_scb_rustats_init(wlc_muscheduler_info_t *musched, scb_t *scb);
static void wlc_musched_print_ru_stats(musched_ru_stats_t* ru_stats, bcmstrbuf_t *b,
	bool is_160);
#endif // endif

#if defined(BCMDBG) || defined(UL_RU_STATS_DUMP)
#endif // endif
static void wlc_musched_config_ru_alloc_type(wlc_muscheduler_info_t *musched);
static void wlc_musched_config_mix_ackp(wlc_muscheduler_info_t *musched);
static void wlc_musched_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data);
static void wlc_musched_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *notif_data);
static void wlc_musched_admit_users_reset(wlc_muscheduler_info_t *musched,
		wlc_bsscfg_t *cfg);
static int wlc_musched_get_dlpolicy(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_ack_policy(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_use_murts(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_lowat(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_maxn(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_mindluser(wlc_muscheduler_info_t *musched);
static int wlc_musched_write_tmout(wlc_muscheduler_info_t *musched);
static int wlc_musched_upd_rucfg_rmem(wlc_muscheduler_info_t *musched, uint8 bw_idx);
static uint8 wlc_musched_ruidx_mapping(uint8 ru_idx, int8 scale_level, bool toBw160);
static uint8 wlc_musched_ackruidx_mapping(uint8 data_ruidx, uint8 ackru_type);
#ifdef MAC_AUTOTXV_OFF
static void wlc_musched_admit_users_reinit(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg);
#endif // endif
static bool wlc_scbmusched_is_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb);
static void wlc_scbmusched_enable_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb, bool enable);
/* scheduler admit control */

/* scb cubby */
static int wlc_musched_scb_init(void *ctx, scb_t *scb);
static void wlc_musched_scb_deinit(void *ctx, scb_t *scb);
static uint wlc_musched_scb_secsz(void *, scb_t *);
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static void wlc_musched_scb_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b);
#endif // endif
static void wlc_musched_scb_schedule_init(wlc_muscheduler_info_t *musched, scb_t *scb);

/* dump */
static void wlc_musched_dump_policy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b);
static void wl_musched_dump_ackpolicy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b);
static int wlc_musched_minru_update(wlc_muscheduler_info_t *musched, scb_t *scb, uint8 minru);
/* iovar table */
enum {
	IOV_MUSCHEDULER		= 0,
	IOV_LAST
};

static const bcm_iovar_t muscheduler_iovars[] = {
	{"msched", IOV_MUSCHEDULER, 0, 0, IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0, 0}
};

static const uint8 rucfg_numrus[MUSCHED_RU_TYPE_NUM] = {37, 16, 8, 4, 2, 1, 1};

/* Function to scale up and down on given ru index
 * level = 1: BW80->BW160
 * level = -1: BW80->BW40
 * level = -2: BW80->BW20
 * For example, given that input ru=61:
 *	BW80->BW40, level = -1, output is 53
 *	BW80->BW20, level = -2, output is 37
 *	BW80->BW160, level = 1, output is 65
 * XXX: Note: the best pratice of using this API is to do conversion from BW80 to other BWs.
 */
static uint8
wlc_musched_ruidx_mapping(uint8 ru_idx, int8 scale_level, bool toBw160)
{
	ru_type_t ru_type;
	int16 new_ruidx = ru_idx;
	int8 new_level;
	bool upper_sector = FALSE;

	if (ru_idx <= HE_MAX_26_TONE_RU_INDX) {
		ru_type = D11AX_RU26_TYPE;
		upper_sector = ru_idx >= 19 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_52_TONE_RU_INDX) {
		ru_type = D11AX_RU52_TYPE;
		upper_sector = ru_idx >= 45 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_106_TONE_RU_INDX) {
		ru_type = D11AX_RU106_TYPE;
		upper_sector = ru_idx >= 57 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_242_TONE_RU_INDX) {
		ru_type = D11AX_RU242_TYPE;
		upper_sector = ru_idx >= 63 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_484_TONE_RU_INDX) {
		ru_type = D11AX_RU484_TYPE;
		upper_sector = ru_idx >= 66 ? TRUE : FALSE;
	} else if (ru_idx <= HE_MAX_996_TONE_RU_INDX) {
		ru_type = D11AX_RU996_TYPE;
	} else if (ru_idx <= HE_MAX_2x996_TONE_RU_INDX) {
		ru_type = D11AX_RU1992_TYPE;
	} else {
		return new_ruidx;
	}
	new_level = ru_type + scale_level;
	if (new_level < 0 || new_level >= MUSCHED_RU_TYPE_NUM) {
		new_level = 0;
	}

	if (scale_level > 0) {
		/* scale up case */
		if (upper_sector && toBw160) {
			new_ruidx = (ru_idx | MUSCHED_RU_UPPER_MASK);
		}
		while (ru_type < new_level) {
			if (upper_sector && toBw160) {
				new_ruidx += (rucfg_numrus[ru_type++] / 2);
			} else {
				new_ruidx += rucfg_numrus[ru_type++];
			}
		}
	} else {
		/* scale down case */
		while (ru_type > new_level) {
			new_ruidx -= rucfg_numrus[--ru_type];
		}
		/* Skip some 26 tone RUs to avoid overlapping RUs */
		if (ru_type == D11AX_RU26_TYPE) {
			if (new_ruidx < MUSCHED_26TONE_RU4) {
				/* pass */
			} else if (new_ruidx < MUSCHED_26TONE_RU13 - 1) {
				new_ruidx += 1;
			} else if (new_ruidx < MUSCHED_26TONE_RU18 - 2) {
				new_ruidx += 2;
			} else if (new_ruidx < MUSCHED_26TONE_RU23 - 3) {
				new_ruidx += 3;
			} else if (new_ruidx < MUSCHED_26TONE_RU32 - 4) {
				new_ruidx += 4;
			}
		}
	}

	return (new_ruidx < 0 ? 0 : new_ruidx);
}

/*
 * Function to return ru idx for hetb ack given data ru idx and ack ru type
 */
static uint8
wlc_musched_ackruidx_mapping(uint8 data_ruidx, uint8 ackru_type)
{
	uint8 ruidx_start, ruidx_offset, ru_gap;
	int16 ack_ruidx = data_ruidx;
	int8 ru_upper = data_ruidx & MUSCHED_RU_UPPER_MASK;
	ru_type_t ru_type;

	/* mask out upper bit */
	data_ruidx &= MUSCHED_RU_IDX_MASK;

	if (data_ruidx <= HE_MAX_26_TONE_RU_INDX) {
		return ack_ruidx;
	} else if (data_ruidx <= HE_MAX_52_TONE_RU_INDX) {
		ru_type = D11AX_RU52_TYPE;
		ruidx_start = HE_MAX_26_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_106_TONE_RU_INDX) {
		ru_type = D11AX_RU106_TYPE;
		ruidx_start = HE_MAX_52_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_242_TONE_RU_INDX) {
		ru_type = D11AX_RU242_TYPE;
		ruidx_start = HE_MAX_106_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_484_TONE_RU_INDX) {
		ru_type = D11AX_RU484_TYPE;
		ruidx_start = HE_MAX_242_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_996_TONE_RU_INDX) {
		ru_type = D11AX_RU996_TYPE;
		ruidx_start = HE_MAX_484_TONE_RU_INDX + 1;
	} else if (data_ruidx <= HE_MAX_2x996_TONE_RU_INDX) {
		ru_type = D11AX_RU1992_TYPE;
		ruidx_start = HE_MAX_996_TONE_RU_INDX + 1;
	} else {
		return ack_ruidx;
	}

	if (ackru_type >= ru_type) {
		return ack_ruidx;
	}

	ru_gap = ackru_type - ru_type;
	ruidx_offset = data_ruidx - ruidx_start;
	ack_ruidx = wlc_musched_ruidx_mapping(ruidx_start, ru_gap, 0);
	ack_ruidx += (rucfg_numrus[ackru_type] / rucfg_numrus[ru_type]) * ruidx_offset;
	ack_ruidx += ru_upper;

	return ack_ruidx;
}

static const uint8 rucfg_init_tbl[MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL] = {
	{67},
	{65, 66},
	{65, 63, 64},
	{61, 62, 63, 64},
	{61, 62, 63, 59, 60},
	{61, 62, 57, 58, 59, 60},
	{61, 55, 56, 57, 58, 59, 60},
	{53, 54, 55, 56, 57, 58, 59, 60},
	{53, 54, 55, 56, 57, 58, 59, 51, 52},
	{53, 54, 55, 56, 57, 58, 49, 50, 51, 52},
	{53, 54, 55, 56, 57, 47, 48, 49, 50, 51, 52},
	{53, 54, 55, 56, 45, 46, 47, 48, 49, 50, 51, 52},
	{53, 54, 55, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
	{53, 54, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
	{53, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
	{37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52},
};

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
*/
#include <wlc_patch.h>

/* ======== attach/detach ======== */

wlc_muscheduler_info_t *
BCMATTACHFN(wlc_muscheduler_attach)(wlc_info_t *wlc)
{
	wlc_muscheduler_info_t *musched;
	scb_cubby_params_t cubby_params;
	int i, j, tbl_idx;
	int8 scale_level;
	bool toBw160 = FALSE;

	/* allocate private module info */
	if ((musched = MALLOCZ(wlc->osh, sizeof(*musched))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	musched->wlc = wlc;

	/* HE is attached before this module, so can rely on HE_ENAB here */
	if (!HE_ENAB(wlc->pub)) {
		WL_INFORM(("wl%d: %s: MU scheduler disabled\n", wlc->pub->unit,	__FUNCTION__));
		return musched;
	}

	musched->dl_policy = MUSCHED_DL_POLICY_AUTO;
	musched->rualloc = MUSCHED_RUALLOC_AUTO;
	musched->tmout = MUSCHED_TRVLSCH_TMOUT_DFLT;
	musched->ack_policy = MUSCHED_ACKPOLICY_TRIGINAMP;
	musched->mix_ackp = TRUE;
	musched->use_murts = MUSCHED_MURTS_DFLT;
	wlc->dsmps_war = FALSE;
	musched->wfa20in80 = FALSE;

	if (D11REV_IS(wlc->pub->corerev, 130) ||
		D11REV_IS(wlc->pub->corerev, 131)) {
		/* Do not admit 160 MHz clients for DLOFDMA
		 * while operating in BW:160 for corerev 130 and 131
		 */
		musched->mixbw = FALSE;
	} else {
		musched->mixbw = TRUE;
	}
	for (i = 0; i < D11_REV128_BW_SZ; i++) {
		musched->lowat[i] = MUSCHED_TRVLSCH_LOWAT_DFLT;
	}
	wlc_txcfg_dlofdma_maxn_init(wlc, musched->maxn);
	musched->rucfg_fixed = FALSE;
	/* init the different BWs' rucfg tbls based on BW80 template */
	for (tbl_idx = 0; tbl_idx < D11_REV128_BW_SZ; tbl_idx++) {
		scale_level = tbl_idx - 2;
		toBw160 = (tbl_idx == D11_REV128_BW_160MHZ) ? TRUE : FALSE;
		for (i = 0; i < MUSCHED_RUCFG_ROW; i++) {
			for (j = 0; j < MUSCHED_RUCFG_COL; j++) {
				musched->rucfg[tbl_idx][i][j] = wlc_musched_ruidx_mapping(
					rucfg_init_tbl[i][j], scale_level, toBw160);
				/* init 106T RU for HETB ACK */
				musched->rucfg_ack[tbl_idx][i][j] =
					wlc_musched_ackruidx_mapping(musched->rucfg
					[tbl_idx][i][j], D11AX_RU106_TYPE);
			}
		}
	}
	/* 2 dlofdma capable users are needed to enable dlofdma by default */
	for (i = 0; i < MUSCHED_DLOFDMA_MINUSER_SZ; i++) {
		musched->min_dlofdma_users[i] = MUSCHED_TRVLSCH_MINN_DFLT;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, muscheduler_iovars, "muscheduler", musched,
		wlc_muscheduler_doiovar, NULL, wlc_muscheduler_wlc_init, NULL)) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve some space in scb for private data */
	bzero(&cubby_params, sizeof(cubby_params));

	cubby_params.context = musched;
	cubby_params.fn_init = wlc_musched_scb_init;
	cubby_params.fn_deinit = wlc_musched_scb_deinit;
	cubby_params.fn_secsz = wlc_musched_scb_secsz;
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
	cubby_params.fn_dump = wlc_musched_scb_dump;
#endif // endif

	if ((musched->scbh =
		wlc_scb_cubby_reserve_ext(wlc, sizeof(scb_musched_t *), &cubby_params)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve_ext() failed\n", wlc->pub->unit,
			__FUNCTION__));
		goto fail;
	}

	if (wlc_scb_state_upd_register(wlc, wlc_musched_scb_state_upd, musched) != BCME_OK) {
		WL_ERROR(("wl%d: %s: unable to register callback wlc_musched_scb_state_upd\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if ((wlc_bsscfg_state_upd_register(wlc,
			wlc_musched_bsscfg_state_upd, musched)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_state_upd_register failed\n",
		          wlc->pub->unit, __FUNCTION__));
			goto fail;
	}

	/* debug dump */
#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
	wlc_dump_add_fns(wlc->pub, "msched", wlc_musched_dump, wlc_musched_dump_clr, musched);
#endif // endif

	return musched;

fail:
	wlc_muscheduler_detach(musched);
	return NULL;
}

void
BCMATTACHFN(wlc_muscheduler_detach)(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc;

	if (musched == NULL) {
		return;
	}

	wlc = musched->wlc;

	wlc_scb_state_upd_unregister(wlc, wlc_musched_scb_state_upd, musched);

	wlc_bsscfg_state_upd_unregister(wlc, wlc_musched_bsscfg_state_upd, musched);

	wlc_module_unregister(wlc->pub, "muscheduler", musched);

	MFREE(wlc->osh, musched, sizeof(*musched));
}

/* ======== iovar dispatch ======== */
static int
wlc_musched_cmd_get_dispatch(wlc_muscheduler_info_t *musched, wl_musched_cmd_params_t *params,
	char *outbuf, int outlen)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	bcmstrbuf_t bstr;
	int i, j, bw, start, end;
	uint16 uval16 = 0;
	uint offset;
	int8 direction; /* indicates 1:dl 2:ul 3:bi */

	BCM_REFERENCE(i);
	BCM_REFERENCE(offset);
	BCM_REFERENCE(direction);

	bcm_binit(&bstr, outbuf, outlen);
	wlc = musched->wlc;

	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!strncmp(params->keystr, "lowat", strlen("lowat"))) {
		bcm_bprintf(&bstr, "low watermark ");
		for (i = 0; i < D11_REV128_BW_SZ; i++) {
			bw = 20 << i;
			bcm_bprintf(&bstr, "bw%d: %d ", bw, musched->lowat[i]);
		}
		bcm_bprintf(&bstr, "\n");
#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_OQEXPECTN_BLK(wlc);
			for (i = 0; i < D11_REV128_BW_SZ; i++) {
				bw = 20 << i;
				uval16 = wlc_read_shmx(wlc, offset+(i*2));
				if (uval16 != musched->lowat[i]) {
					bcm_bprintf(&bstr, "shmx bw%d: %d ", bw, uval16);
				}
			}
			bcm_bprintf(&bstr, "\n");
		}
#endif // endif
	} else if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		bcm_bprintf(&bstr, "maxN ");
		for (i = 0; i < D11_REV128_BW_SZ; i++) {
			bw = 20 << i;
			bcm_bprintf(&bstr, "bw%d: %d ", bw, musched->maxn[i]);
		}
		bcm_bprintf(&bstr, "\n");
#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_OQMAXN_BLK(wlc);
			for (i = 0; i < D11_REV128_BW_SZ; i++) {
				bw = 20 << i;
				uval16 = wlc_read_shmx(wlc, offset+(i*2));
				if (uval16 != musched->maxn[i]) {
					bcm_bprintf(&bstr, "shmx bw%d: %d ", bw, uval16);
				}
			}
			bcm_bprintf(&bstr, "\n");
		}
#endif // endif
	} else if (!strncmp(params->keystr, "tmout", strlen("tmout"))) {
		bcm_bprintf(&bstr, "tmout %d us\n", musched->tmout);

#if defined(WL_PSMX)
		if (wlc->clk) {
			offset = MX_OMSCH_TMOUT(wlc);
			uval16 = wlc_read_shmx(wlc, offset);
			if (uval16 != musched->tmout) {
				bcm_bprintf(&bstr, "shmx tmout %d us\n", uval16);
			}
		}
#endif // endif
	} else if (!strncmp(params->keystr, "rucfg_ack", strlen("rucfg_ack")) ||
			!strncmp(params->keystr, "rucfg", strlen("rucfg"))) {
		int8 row, tbl_idx;
		uint8 (*ptr)[MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
		if (!strcmp(params->keystr, "rucfg")) {
			ptr = musched->rucfg;
		} else {
			ptr = musched->rucfg_ack;
		}
		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (tbl_idx = start; tbl_idx <= end; tbl_idx++) {
			row = params->row;
			if (row <= 0 || row > MUSCHED_RUCFG_ROW) {
				row = MUSCHED_RUCFG_ROW;
				i = 0;
			} else {
				i = row - 1;
			}
			bw = 20 << tbl_idx;
			bcm_bprintf(&bstr, "Nusr: ru allocation (%s) BW %dMHz\n",
				musched->rucfg_fixed ? "manual" : "default", bw);
			for (; i < row; i++) {
				bcm_bprintf(&bstr, "%2d:  ", i+1);
				for (j = MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS;
					j <= i+MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS; j++) {
					uval16 = ptr[tbl_idx][i][j];
					bcm_bprintf(&bstr, " %2d", uval16);
				}
				bcm_bprintf(&bstr, "\n");
			}
		}
	} else if (!strncmp(params->keystr, "ackpolicy", strlen("ackpolicy"))) {
		wl_musched_dump_ackpolicy(musched, &bstr);
	} else if (!strncmp(params->keystr, "murts", strlen("murts"))) {
		bcm_bprintf(&bstr, "Use MU-RTS: %d\n", musched->use_murts);
	} else if (!strncmp(params->keystr, "dsmps_war", strlen("dsmps_war"))) {
		bcm_bprintf(&bstr, "Dyn SMPS WAR: %d\n", wlc->dsmps_war);
	} else if (!strncmp(params->keystr, "rualloc", strlen("rualloc"))) {
		bcm_bprintf(&bstr, "RU alloc mode: %d\n", musched->rualloc);
	} else if (!strncmp(params->keystr, "mindlusers", strlen("mindlusers"))) {
		bcm_bprintf(&bstr, "dlofdma_users: min %d (<=80MHz) %d (160MHz) max %d num %d\n",
			musched->min_dlofdma_users[0],
			musched->min_dlofdma_users[MUSCHED_DLOFDMA_MINUSER_SZ-1],
			wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA),
			musched->num_dlofdma_users);
	} else if (!strncmp(params->keystr, "maxofdmausers", strlen("maxofdmausers"))) {
		bcm_bprintf(&bstr, "Max DL ofdma users: %d\n",
			wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA));
	} else if (!strncmp(params->keystr, "minru", strlen("minru"))) {
		scb_t *scb;
		scb_iter_t scbiter;
		scb_musched_t* musched_scb;

		if (!HE_DLMU_ENAB(musched->wlc->pub))
			return BCME_UNSUPPORTED;

		bcm_bprintf(&bstr, "minRU config:\n");

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
				continue;
			}
			musched_scb = SCB_MUSCHED(musched, scb);
			if (musched_scb) {
				bcm_bprintf(&bstr, ""MACF" (aid 0x%x) minRU: %d\n",
					ETHER_TO_MACF(scb->ea), scb->aid, musched_scb->min_ru);
			}
		}
	} else if (!strncmp(params->keystr, "mixbw", strlen("mixbw"))) {
		bcm_bprintf(&bstr, "mixbw mode (80+160): %d\n", musched->mixbw);
	} else {
		wlc_musched_dump_policy(musched, &bstr);
	}

	return err;
}

static int
wlc_musched_write_ack_policy(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	uint16 uval16;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}
	if (musched->dl_schidx == MCTL2_FIXED_SCHIDX ||
		musched->dl_schidx == MCTL2_TRIVIAL_SCHIDX) {
		offset = MX_HEMSCH0_BLK(wlc);
	} else {
		return BCME_ERROR;
	}
	uval16 = wlc_read_shmx(wlc, offset);
	uval16 &= ~MUSCHED_HEMSCH_STP_MASK;
	uval16 |= ((musched->ack_policy << MUSCHED_HEMSCH_STP_SHIFT) &
		MUSCHED_HEMSCH_STP_MASK);
	wlc_write_shmx(wlc, offset, uval16);
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static int
wlc_musched_write_use_murts(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	uint16 uval16;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}
	if (musched->dl_schidx == MCTL2_FIXED_SCHIDX ||
		musched->dl_schidx == MCTL2_TRIVIAL_SCHIDX) {
		offset = MX_HEMSCH0_BLK(wlc);
	} else {
		return BCME_ERROR;
	}

	uval16 = wlc_read_shmx(wlc, offset);
	uval16 &= ~MUSCHED_HEMSCH_MURTS_MASK;
	uval16 |= ((musched->use_murts << MUSCHED_HEMSCH_MURTS_SHIFT)
		& MUSCHED_HEMSCH_MURTS_MASK);
	wlc_write_shmx(wlc, offset, uval16);
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static int
wlc_musched_write_tmout(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc;
	wlc = musched->wlc;
	if (!wlc->clk)
		return BCME_NOCLK;
#if defined(WL_PSMX)
	wlc_write_shmx(wlc, MX_OMSCH_TMOUT(wlc), musched->tmout);
#endif // endif
	return BCME_OK;
}

static int
wlc_musched_write_mindluser(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int i;
	wlc = musched->wlc;
	if (!wlc->clk)
		return BCME_NOCLK;
	offset = MX_OQMINN_BLK(wlc);
	for (i = 0; i < MUSCHED_DLOFDMA_MINUSER_SZ; i++) {
		wlc_write_shmx(wlc, offset+(i*2), musched->min_dlofdma_users[i]);
	}
	wlc_write_shmx(wlc, MX_UCX2R_FLAGS(wlc),
		wlc_read_shmx(wlc, MX_UCX2R_FLAGS(wlc)) & ~D11_UCX2R_CONS_MASK);
#endif // endif
	return BCME_OK;
}

static int
wlc_musched_write_lowat(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int bw;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = MX_OQEXPECTN_BLK(wlc);
	for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
		wlc_write_shmx(wlc, offset+(bw*2), musched->lowat[bw]);
	}
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

static int
wlc_musched_write_maxn(wlc_muscheduler_info_t *musched)
{
#if defined(WL_PSMX)
	wlc_info_t *wlc;
	uint offset;
	int bw;

	wlc = musched->wlc;
	if (!wlc->clk) {
		return BCME_NOCLK;
	}

	offset = MX_OQMAXN_BLK(wlc);
	for (bw = 0; bw < D11_REV128_BW_SZ; bw++) {
		wlc_write_shmx(wlc, offset+(bw*2), musched->maxn[bw]);
	}
#ifdef WLTAF
	wlc_taf_set_dlofdma_maxn(wlc, &musched->maxn);
#endif // endif
#endif /* defined(WL_PSMX) */
	return BCME_OK;
}

void
wlc_musched_get_min_dlofdma_users(wlc_muscheduler_info_t *musched,
		uint16 *min_users, bool *allow_bw160)
{
	*min_users = musched->min_dlofdma_users[0];
	*allow_bw160 = musched->mixbw;
}
static int
wlc_musched_cmd_set_dispatch(wlc_muscheduler_info_t *musched, wl_musched_cmd_params_t *params)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	uint16 uval16;
	uint offset;
	int i, start, end;

	BCM_REFERENCE(i);
	BCM_REFERENCE(start);
	BCM_REFERENCE(end);
	BCM_REFERENCE(offset);

	wlc = musched->wlc;

	if (D11REV_LE(wlc->pub->corerev, 128))
		return BCME_UNSUPPORTED;

	if (!strncmp(params->keystr, "lowat", strlen("lowat"))) {
		uval16 = params->vals[0] >= 0 ? params->vals[0] : 0;

		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (i = start; i <= end; i++) {
			musched->lowat[i] = uval16;
		}

		wlc_musched_write_lowat(musched);

	} else if (!strncmp(params->keystr, "maxn", strlen("maxn"))) {
		uval16 = params->vals[0] >= 0 ? params->vals[0] : 0;

		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		for (i = start; i <= end; i++) {
			musched->maxn[i] = MIN(uval16, wlc_txcfg_ofdma_maxn_upperbound(wlc, i));
		}

		wlc_musched_write_maxn(musched);
	} else if (!strncmp(params->keystr, "tmout", strlen("tmout"))) {
		musched->tmout = (params->vals[0] >= 0) ? params->vals[0] : 0;
		wlc_musched_write_tmout(musched);
	} else if (!strncmp(params->keystr, "rucfg_ack", strlen("rucfg_ack")) ||
			!strncmp(params->keystr, "rucfg", strlen("rucfg"))) {
		int8 row, tbl_idx;
		uint8 (*ptr)[MUSCHED_RUCFG_ROW][MUSCHED_RUCFG_COL];
		if (!strcmp(params->keystr, "rucfg")) {
			ptr = musched->rucfg;
		} else {
			ptr = musched->rucfg_ack;
		}
#if !defined(TESTBED_AP_11AX) && !defined(BCMDBG)
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
#endif /* !TESTBED_AP_11AX && !BCMDBG */
		row = params->row;
		if (row == -1 || row > MUSCHED_RUCFG_ROW) {
			return BCME_USAGE_ERROR;
		}
		if (params->bw == -1) {
			start = 0;
			end = D11_REV128_BW_SZ-1;
		} else {
			start = end = params->bw;
		}
		musched->rucfg_fixed = TRUE;
		for (tbl_idx = start; tbl_idx <= end; tbl_idx++) {
			for (i = 0; i < params->num_vals; i++) {
				ptr[tbl_idx][row-1][i+MUSCHED_TRVLSCH_RUCFG_RUSTRT_POS]=
					params->vals[i];
			}
		}
#if defined(TESTBED_AP_11AX) || defined(BCMDBG)
		if (wlc->pub->up && RATELINKMEM_ENAB(wlc->pub)) {
			uint txpktpendtot = 0; /* # of packets handed over to d11 DMA */
			/* If there are pending packets on the fifo, then stop the fifo
			 * processing and re-enqueue packets
			 */
			for (i = 0; i < WLC_HW_NFIFO_INUSE(wlc); i++) {
				txpktpendtot += TXPKTPENDGET(wlc, i);
			}
			if (txpktpendtot > 0) {
				wlc_sync_txfifo_all(wlc, wlc->active_queue, SYNCFIFO);
			}
			/* write rucfg to ratemem */
			for (tbl_idx = start; tbl_idx <= end; tbl_idx++) {
				if ((err = wlc_musched_upd_rucfg_rmem(musched, tbl_idx))
					!= BCME_OK) {
					WL_ERROR(("wl%d: %s: fail to write rucfg "
						"bw_idx %d err %d\n",
						wlc->pub->unit, __FUNCTION__, tbl_idx, err));
					ASSERT(0);
				}
			}
		}
#endif /* TESTBED_AP_11AX || BCMDBG */
	} else if (!strncmp(params->keystr, "policy", strlen("policy"))) {
		if ((wlc_musched_get_dlpolicy(musched) != MUSCHED_DL_POLICY_DISABLE &&
			params->vals[0] == MUSCHED_DL_POLICY_DISABLE) ||
			(wlc_musched_get_dlpolicy(musched) == MUSCHED_DL_POLICY_DISABLE &&
			params->vals[0] != MUSCHED_DL_POLICY_DISABLE)) {
				wlc_musched_set_dlpolicy(musched, params->vals[0]);
			}
		else if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
	} else if (!strncmp(params->keystr, "ackpolicy", strlen("ackpolicy"))) {
		if (params->vals[0] > MUSCHED_ACKPOLICY_MAX || params->vals[0] < 0) {
			return BCME_RANGE;
		}

		musched->ack_policy = params->vals[0];
		wlc_musched_write_ack_policy(musched);
	} else if (!strncmp(params->keystr, "mixackp", strlen("mixackp"))) {
		if (musched->ack_policy == MUSCHED_ACKPOLICY_SERIAL) {
			/* No mix ackp needed if ack policy is set to serial ack */
			return BCME_BADARG;
		}
		musched->mix_ackp = params->vals[0] == 0 ? FALSE : TRUE;
		wlc_musched_config_mix_ackp(musched);
	} else if (!strncmp(params->keystr, "murts", strlen("murts"))) {
		if (params->vals[0] >= MUSCHED_MURTS_RSVD) {
			return BCME_RANGE;
		}
		musched->use_murts = params->vals[0];
		wlc_musched_write_use_murts(musched);
	} else if (!strncmp(params->keystr, "dsmps_war", strlen("dsmps_war"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		wlc->dsmps_war = params->vals[0] != 0 ? TRUE : FALSE;
	} else if (!strncmp(params->keystr, "rualloc", strlen("rualloc"))) {
		if (wlc->pub->up) {
			return BCME_NOTDOWN;
		}
		if (params->vals[0] > MUSCHED_RUALLOC_MAX ||
			params->vals[0] < MUSCHED_RUALLOC_AUTO) {
			return BCME_RANGE;
		}
		musched->rualloc = params->vals[0];
	} else if (!strncmp(params->keystr, "mindlusers", strlen("mindlusers"))) {
		uval16 = params->vals[0] >= 0 ? params->vals[0] : 0;

		if (params->bw == -1) {
			start = D11_REV128_BW_20MHZ;
			end = MIN(D11_REV128_BW_160MHZ, MUSCHED_DLOFDMA_MINUSER_SZ-1);
		} else if (params->bw >= D11_REV128_BW_20MHZ || params->bw <= D11_REV128_BW_80MHZ) {
			start = end = MIN(params->bw, MUSCHED_DLOFDMA_MINUSER_SZ-2);
		} else if (params->bw == D11_REV128_BW_160MHZ) {
			if (uval16 < MUSCHED_TRVLSCH_MINN_DFLT) {
				/* MU 1 160MHz limitation */
				return BCME_UNSUPPORTED;
			}
			start = end = MIN(params->bw, MUSCHED_DLOFDMA_MINUSER_SZ-1);
		} else {
			/* invalid BW input */
			return BCME_UNSUPPORTED;
		}
		ASSERT(start >= 0 && end >= 0);

		for (i = start; i < end; i++) {
			musched->min_dlofdma_users[i] = uval16;
		}

		if (end == MIN(D11_REV128_BW_160MHZ, MUSCHED_DLOFDMA_MINUSER_SZ-1)) {
			/* MU 1 160MHz limitation */
			musched->min_dlofdma_users[end] = MAX(uval16, MUSCHED_TRVLSCH_MINN_DFLT);
		}

		if (wlc->pub->up) {
			if (wlc_mutx_is_hedlmmu_enab(wlc->mutx) || HE_DLMU_ENAB(wlc->pub)) {
				wlc_mutx_upd_min_dlofdma_users(wlc->mutx);
			} else {
				wlc_musched_admit_dlclients(musched);
			}
		}
		wlc_musched_write_mindluser(musched);
	} else if (!strncmp(params->keystr, "minru", strlen("minru"))) {
		wlc_bsscfg_t *bsscfg;
		scb_t *scb;
		int8 idx;

		if (params->vals[0] > D11AX_RU1992_TYPE ||
			params->vals[0] < D11AX_RU26_TYPE) {
			return BCME_RANGE;
		}

		err = BCME_BADADDR;
		FOREACH_BSS(wlc, idx, bsscfg) {
			if (WL_MUSCHED_FLAGS_MACADDR(params) &&
				(scb = wlc_scbfind(wlc, bsscfg, &params->ea))) {
				/* update linkmem  */
				err = wlc_musched_minru_update(musched, scb, params->vals[0]);
			}
		}
	} else if (!strncmp(params->keystr, "mixbw", strlen("mixbw"))) {
		if (params->vals[0]) {
			uval16 = 0;
			musched->mixbw = TRUE;
		} else {
			uval16 = MXHF1_NOMIXBW;
			musched->mixbw = FALSE;
		}
		wlc_bmac_mhf(wlc->hw, MXHF1, MXHF1_NOMIXBW, uval16, WLC_BAND_ALL);
	} else if (!strncmp(params->keystr, "wfa20in80", strlen("wfa20in80"))) {
		uint16 minn_tmp;
		if (wlc->pub->up) {
			/* allow only when wl is down to limit rateset of STAs */
			return BCME_NOTDOWN;
		}
		if (params->vals[0]) {
			musched->wfa20in80 = TRUE;
			// set maxn and minn to 4
			for (i = 0; i <= (D11_REV128_BW_SZ-1); i++) {
				musched->maxn[i] = 4;
			}
			minn_tmp = 4;
			uval16 = MXHF1_WFA20IN80;
		} else {
			musched->wfa20in80 = FALSE;
			// set maxn and minn to default
			wlc_txcfg_dlofdma_maxn_init(wlc, musched->maxn);
			minn_tmp = MUSCHED_TRVLSCH_MINN_DFLT;
			uval16 = 0;
		}
		for (i = 0; i < MUSCHED_DLOFDMA_MINUSER_SZ; i++) {
			musched->min_dlofdma_users[i] = minn_tmp;
		}
		wlc_bmac_mhf(wlc->hw, MXHF1, MXHF1_WFA20IN80, uval16, WLC_BAND_ALL);
		/* maxn and mindluser are written at wl up time */
	} else {
		err = BCME_BADARG;
	}

	return err;
}

static int
wlc_muscheduler_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, wlc_if_t *wlcif)
{
	int err = BCME_OK;
	int32 *ret_int_ptr;
	wlc_muscheduler_info_t *musched = ctx;
	wlc_info_t *wlc = musched->wlc;
	wl_musched_cmd_params_t musched_cmd_params;
	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	BCM_REFERENCE(vsize);
	BCM_REFERENCE(wlc);
	BCM_REFERENCE(ret_int_ptr);

	if (D11REV_LT(wlc->pub->corerev, 129)) return BCME_ERROR;

	bcopy(params, &musched_cmd_params, sizeof(wl_musched_cmd_params_t));
	switch (actionid) {
	case IOV_GVAL(IOV_MUSCHEDULER):
		err = wlc_musched_cmd_get_dispatch(musched, &musched_cmd_params, arg, alen);
		break;
	case IOV_SVAL(IOV_MUSCHEDULER):
		err = wlc_musched_cmd_set_dispatch(musched, params);
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* ======== wlc module hooks ========= */

/* wlc init callback */
static int
wlc_muscheduler_wlc_init(void *ctx)
{
	wlc_muscheduler_info_t *musched = ctx;
	int err = BCME_OK;
	wlc_info_t *wlc = musched->wlc;

	if (!HE_ENAB(wlc->pub)) {
		return BCME_OK;
	}

	wlc_musched_set_dlpolicy(musched, musched->dl_policy);
	wlc_musched_config_ru_alloc_type(musched);
	wlc_musched_config_mix_ackp(musched);

	wlc_musched_write_lowat(musched);
	wlc_musched_write_maxn(musched);
	wlc_musched_write_mindluser(musched);
	wlc_musched_write_ack_policy(musched);
	wlc_musched_write_use_murts(musched);
	if (RATELINKMEM_ENAB(wlc->pub)) {
		int bw_idx;
		/* write rucfg to ratemem */
		for (bw_idx = 0; bw_idx < D11_REV128_BW_SZ; bw_idx++) {
			if ((err = wlc_musched_upd_rucfg_rmem(musched, bw_idx)) != BCME_OK) {
				WL_ERROR(("wl%d: %s: fail to write rucfg bw_idx %d err %d\n",
				wlc->pub->unit, __FUNCTION__, bw_idx, err));
				ASSERT(0);
			}
		}
	}

#ifdef MAC_AUTOTXV_OFF
	/* initialize bfi shm config */
	wlc_musched_admit_users_reinit(musched, NULL);
#endif // endif

	if (!wlc->pub->up) {
		wlc_musched_admit_users_reset(musched, NULL);
	}

	return err;
}

#ifdef MAC_AUTOTXV_OFF
static void wlc_musched_admit_users_reinit(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = musched->wlc;
	scb_iter_t scbiter;
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	scb_musched_t *musched_scb;
	int i;

	/* re-init all the already admitted dl/ul ofdma users */
	FOREACH_BSS(wlc, i, bsscfg) {
		if (cfg && cfg != bsscfg) {
			continue;
		}

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			musched_scb = SCB_MUSCHED(musched, scb);
			if (!musched_scb) {
				continue;
			}
			if (!wlc_musched_scb_isdlofdma_eligible(musched, scb)) {
				continue;
			}

			if (!musched_scb->dlul_assoc) {
				continue;
			}
			wlc_txbf_tbcap_update(wlc->txbf, scb);
			wlc_macreq_upd_bfi(wlc, scb, wlc_fifo_index_peek(wlc->fifo, scb, AC_BE),
				TRUE);

			WL_INFORM(("wl%d: %s: sta "MACF"\n",
				wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		}

	}
}
#endif /* MAC_AUTOTXV_OFF */

static void
wl_musched_dump_ackpolicy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b)
{
	uint16 uval16 = 0;
	uint offset;
	wlc_hw_info_t *wlc_hw = musched->wlc->hw;
	wlc_info_t *wlc = musched->wlc;
	char ackpolicy_str [][13] = {
		"serial",
		"trig_in_amp",
		"mu_bar",
		"mubar_in_amp",
		"invalid",
	};

	BCM_REFERENCE(offset);

	uval16 = MIN(musched->ack_policy, ARRAYSIZE(ackpolicy_str)-1);
	bcm_bprintf(b, "ack policy: %s (%d)\n",
		ackpolicy_str[uval16], musched->ack_policy);
	if (!wlc->clk) {
		return;
	}

	if (musched->dl_schidx == MCTL2_FIXED_SCHIDX ||
		musched->dl_schidx == MCTL2_TRIVIAL_SCHIDX) {
		offset = MX_HEMSCH0_BLK(wlc);
#if defined(WL_PSMX)
		uval16 = wlc_bmac_read_shmx(wlc_hw, offset);
#endif // endif
	} else {
		uval16 = ARRAYSIZE(ackpolicy_str) - 1;
		goto print;
	}

	uval16 = MUSCHED_HEMSCH_STP(uval16);
print:
	if (uval16 != musched->ack_policy) {
		bcm_bprintf(b, "shmx ack policy: %d\n", uval16);
	}
}

static void
wlc_musched_dump_policy(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b)
{
	wlc_info_t *wlc = musched->wlc;
	int i;
#ifdef NOT_YET
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* NOT_YET */
	uint16 max_dlofdma_users = wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA);
	char musched_dl_policy_str[][20] = {
		"DISABLE",
		"FIXED",
		"TRIVIAL",
		"AUTO",
	};
	char musched_ru_alloc_str[][6] = {
		"RUCFG",
		"UCRU",
	};
	uint dl_policy_idx = MIN((uint) wlc_musched_get_dlpolicy(musched),
		ARRAYSIZE(musched_dl_policy_str)-1);

	bcm_bprintf(b, "MU scheduler:\n");
	bcm_bprintf(b, "DL policy: %s (%d)",
		musched_dl_policy_str[dl_policy_idx], wlc_musched_get_dlpolicy(musched));
	bcm_bprintf(b, "  ofdma_en: %d schidx: %d flag: 0x%04x ",
		MUSCHED_DLOFDMA(musched), musched->dl_schidx, musched->flags);
	wl_musched_dump_ackpolicy(musched, b);
	bcm_bprintf(b, "           RU alloc mode: %s (%d), mu-rts: %d, dsmps_war: %d "
		"mixackp: %d, mixbw (80+160): %d\n"
		"           dlofdma_users min %d max %d num %d\n",
		musched_ru_alloc_str[wlc_musched_get_rualloctype(musched)],
		musched->rualloc, musched->use_murts, wlc->dsmps_war, musched->mix_ackp,
		musched->mixbw, musched->min_dlofdma_users[0],
		max_dlofdma_users, musched->num_dlofdma_users);
	bcm_bprintf(b, "           maxn ");
	for (i = 0; i < D11_REV128_BW_SZ; i++) {
		bcm_bprintf(b, "bw%d: %d ", 20 << i, musched->maxn[i]);
	}
	bcm_bprintf(b, "\n");
#ifdef NOT_YET
	bcm_bprintf(b, "max_dlofdma_users: %d, assigned MU FIFO slots:\n",
		max_dlofdma_users);
	for (i = 0; i < max_dlofdma_users; i++) {
		scb_t *scb = musched->dlofdma_scb[i];
		bcm_bprintf(b, "\t[%d]: %p (%s) ofdma: %d\n", i, scb, scb ?
			bcm_ether_ntoa(&scb->ea, eabuf) : "/",
			scb ? SCB_DLOFDMA(scb) : 0);
	}
#endif /* NOT_YET */
	bcm_bprintf(b, "\n");
}

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
/* debug dump for dl ofdma scheduler */
static int
wlc_musched_dump(void *ctx, bcmstrbuf_t *b)
{
	wlc_muscheduler_info_t *musched = ctx;

	wlc_musched_dump_policy(musched, b);

	if (HE_DLMU_ENAB(musched->wlc->pub)) {
		wlc_musched_dump_ru_stats(musched, b);
	}
	return BCME_OK;
}

static int
wlc_musched_dump_clr(void *ctx)
{
	wlc_muscheduler_info_t *musched = ctx;
	scb_iter_t scbiter;
	scb_t *scb;
	scb_musched_t* musched_scb;
	wlc_info_t *wlc = musched->wlc;

	/* clear global musched_ru_stats */
	memset(&musched->ru_stats, 0, sizeof(musched_ru_stats_t));

	/* clear musched_ru_stats for scb */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
			continue;
		}
		musched_scb = SCB_MUSCHED(musched, scb);
		if (musched_scb && musched_scb->scb_ru_stats) {
			memset(musched_scb->scb_ru_stats, 0, sizeof(musched_ru_stats_t));
		}
	}

	return BCME_OK;
}

static void
wlc_musched_dump_ru_stats(wlc_muscheduler_info_t *musched, bcmstrbuf_t *b)
{
	scb_iter_t scbiter;
	scb_t *scb;
	scb_musched_t* musched_scb;
	wlc_info_t *wlc = musched->wlc;
	uint scb_bw;

	bcm_bprintf(b, "DL RU stats:\n");

	wlc_musched_print_ru_stats(&musched->ru_stats, b, CHSPEC_IS160(musched->wlc->chanspec));

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb) || SCB_HEMMU(scb)) {
			continue;
		}
		musched_scb = SCB_MUSCHED(musched, scb);
		if (musched_scb && musched_scb->scb_ru_stats) {
			scb_bw = wlc_scb_ratesel_get_link_bw(wlc, scb);
			bcm_bprintf(b, ""MACF" (aid 0x%x) is_dlofdma: %d is_dsmps: %d\n",
				ETHER_TO_MACF(scb->ea), scb->aid & HE_STAID_MASK,
				SCB_DLOFDMA(scb), wlc_stf_is_scb_dynamic_smps(wlc, scb));

			wlc_musched_print_ru_stats(musched_scb->scb_ru_stats, b,
				(scb_bw == BW_160MHZ));
		}
	}

}

static void
wlc_musched_print_ru_stats(musched_ru_stats_t* ru_stats, bcmstrbuf_t *b, bool is_160)
{
	int i, k, cnt;
	uint32 total = 0;

	wlc_print_dump_table(b, "    TX", ru_stats->tx_cnt, "   PER", ru_stats->txsucc_cnt,
		MUSCHED_RU_TYPE_NUM, MUSCHED_RU_TYPE_NUM, TABLE_RU);

	for (i = 0; i < MUSCHED_RU_TYPE_NUM; i++) {
		total += ru_stats->tx_cnt[i];
	}

	if (total) {
		bcm_bprintf(b, "    RU : ");
		cnt = 0;
		for (i = 0; i < MUSCHED_RU_BMP_ROW_SZ; i++) {
			if (!is_160 && i > 0) {
				continue;
			}
			for (k = 0; k <= MUSCHED_RU_IDX_NUM; k++) {
				if ((cnt > 0) && (cnt % 16 == 0)) {
					bcm_bprintf(b, "\n");
				}
				if (getbits(ru_stats->ru_idx_use_bmap[i], MUSCHED_RU_BMP_COL_SZ,
					k, 1)) {
					bcm_bprintf(b, "%d%s ", k, i == 0 ? "" : "s");
					cnt++;
				}
			}
		}
		bcm_bprintf(b, "\n");
	}
}

int
wlc_musched_upd_ru_stats(wlc_muscheduler_info_t *musched, scb_t *scb, tx_status_t *txs)
{
	scb_musched_t* musched_scb = SCB_MUSCHED(musched, scb);
	uint8 ru, ruidx, upper, tx_cnt, txsucc_cnt;
	ru_type_t ru_type;

	ru = TX_STATUS128_HEOM_RUIDX(txs->status.s4);
	tx_cnt = TX_STATUS40_TXCNT_RT0(txs->status.s3);
	txsucc_cnt = TX_STATUS40_ACKCNT_RT0(txs->status.s3);
	ruidx = MUSCHED_RU_IDX(ru);

	if (ruidx > HE_MAX_2x996_TONE_RU_INDX) {
			WL_ERROR(("wl%d: %s: Invalid ru type. ru idx %d txs\n"
			"  %04X %04X | %04X %04X | %08X %08X || %08X %08X | %08X\n",
			musched->wlc->pub->unit, __FUNCTION__, ruidx,
			txs->status.raw_bits, txs->frameid, txs->sequence, txs->phyerr,
			txs->status.s3, txs->status.s4, txs->status.s5,
			txs->status.ack_map1, txs->status.ack_map2));
		ASSERT(!"Invalid ru type");
		return BCME_ERROR;
	} else {
		ru_type = wf_he_ruidx_to_ru_type(ruidx);
	}

	upper = MUSCHED_RU_UPPER(ru);

	musched->ru_stats.ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
	WLCNTADD(musched->ru_stats.tx_cnt[ru_type], tx_cnt);
	WLCNTADD(musched->ru_stats.txsucc_cnt[ru_type], txsucc_cnt);

	/* should not get RU stats for non-OFDMA SCB */
	ASSERT(musched_scb != NULL);

	if (musched_scb->scb_ru_stats) {
		musched_scb->scb_ru_stats->ru_idx_use_bmap[upper][ruidx / 8] |= 1 << (ruidx % 8);
		WLCNTADD(musched_scb->scb_ru_stats->tx_cnt[ru_type], tx_cnt);
		WLCNTADD(musched_scb->scb_ru_stats->txsucc_cnt[ru_type], txsucc_cnt);
	}

	return BCME_OK;
}
#endif // endif

#ifdef PKTQ_LOG
int
wlc_musched_dp_stats(wlc_muscheduler_info_t *musched, tx_status_t *txs, uint8 *count)
{
	uint8 ru, ruidx;
	ru_type_t ru_type;

	if (count == NULL) {
		return BCME_ERROR;
	}
	*count = TX_STATUS40_ACKCNT_RT0(txs->status.s3);

	ru = TX_STATUS128_HEOM_RUIDX(txs->status.s4);
	ruidx = MUSCHED_RU_IDX(ru);

	if (ruidx > HE_MAX_2x996_TONE_RU_INDX) {
		return BCME_ERROR;
	}
	else {
		ru_type = wf_he_ruidx_to_ru_type(ruidx);
		return (ru_type);
	}
}
#endif /* PKTQ_LOG */

/*
 * The main purpose of this  function is to update  DL_OFDMA scheduled stations list because of
 * any non-association related events. Currently used in response to HT action frame
 * that indicates Dynamic SMPS mode enabled/disabled. When the station indiactes it's
 * going to Dynamic SMPS mode, we delete that staion from DL-OFDMA list. It's a short term
 * WAR till we have MU-RTS feature.
 * To add other scenarios eligibility condition in wlc_musched_scb_isdlofdma_eligible to be modified
 */
void wlc_musched_update_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb)
{
	bool dlmu_on = wlc_musched_scb_isdlofdma_eligible(musched, scb);
	bool dlmu_redo = FALSE;
	wlc_info_t *wlc = musched->wlc;
	scb_musched_t* musched_scb = SCB_MUSCHED(musched, scb);

	if (!musched_scb) {
		return;
	}

	/* Since the last ofdma client is automatically removed from admit list,
	 * update dlul_assoc as well.
	 */
	if (!dlmu_on &&
		(wlc_scbmusched_is_dlofdma(musched, scb) || musched->num_dlofdma_users == 1)) {
		/*
		 * if client is currently in DL OFDMA list, and no longer eligible
		 * after ulmu status change or ht action, remove the client from DL OFDMA list
		 * decrease user counter for uneligible client and update onoff
		 */
		if (musched_scb->dlul_assoc == TRUE) {
			SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, FALSE);
			/* Release all allocated MU FIFOs to this client. */
			wlc_fifo_free_all(wlc->fifo, scb);
			if (musched->num_dlofdma_users < musched->min_dlofdma_users[0]) {
				/* if num_dlofdma_users becomes less than min, need to redo
				 * dlofdma admit
				 */
				dlmu_redo = TRUE;
			}
		}
		wlc_scbmusched_enable_dlofdma(wlc->musched, scb, FALSE);
	} else if (dlmu_on && !wlc_scbmusched_is_dlofdma(musched, scb)) {
		/* if client is currently not in DL OFDMA list and becomes eligible after
		 * ht action,check and add the STA into DL OFDMA list
		 */
		if ((musched_scb->dlul_assoc == FALSE) &&
			(musched->num_dlofdma_users <
				wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA))) {
			/* Release all previously allocated MU FIFOs to this client.
			 * New FIFOs will be allocated when traffic comes in
			 */
			wlc_fifo_free_all(wlc->fifo, scb);
			SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, TRUE);
		}
		dlmu_redo = TRUE;
	}

	if (dlmu_redo) {
		wlc_musched_admit_dlclients(wlc->musched);
	}
}

void
wlc_musched_admit_dlclients(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc = musched->wlc;
	scb_t *scb;
	scb_iter_t scbiter;
	bool onoff;

	onoff = (musched->num_dlofdma_users >= musched->min_dlofdma_users[0]) ? TRUE : FALSE;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		scb_musched_t *musched_scb = SCB_MUSCHED(musched, scb);
		bool dlmu_on;

		/* check scb_musched validity in case of pending scb deinit */
		if (!musched_scb) {
			continue;
		}

		dlmu_on = wlc_musched_scb_isdlofdma_eligible(musched, scb);

		/* also check dlul_assoc which include max ofdma count info */
		dlmu_on &= musched_scb->dlul_assoc;

		if (!dlmu_on || (onoff == wlc_scbmusched_is_dlofdma(musched, scb))) {
			/* skip ineligible or already set SCB */
			continue;
		}

		/* disable/enable dlmu explicitly */
		wlc_scbmusched_enable_dlofdma(musched, scb, onoff);
		wlc_scbmusched_set_dlschpos(musched, scb, 0);
		/* re-initialize rate info */
		wlc_scb_ratesel_init(wlc, scb);
	}
}

static void
wlc_musched_admit_users_reset(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = musched->wlc;
	scb_iter_t scbiter;
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	scb_musched_t *musched_scb;
	int i;

	/* skip, handled through wlc_mutx.c  */
	if (wlc_mutx_is_hedlmmu_enab(wlc->mutx) || HE_DLMU_ENAB(wlc->pub)) {
		return;
	}

	/* re-init all the already admitted dl/ul ofdma users */
	FOREACH_BSS(wlc, i, bsscfg) {
		if (cfg && cfg != bsscfg) {
			continue;
		}
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			musched_scb = SCB_MUSCHED(musched, scb);
			if (!wlc_musched_scb_isdlofdma_eligible(musched, scb)) {
				continue;
			}
			/* Reset should release all FIFOs allocated to this client */
			wlc_fifo_free_all(wlc->fifo, scb);

			if (!musched_scb || !musched_scb->dlul_assoc) {
				continue;
			}

			SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, FALSE);
			wlc_ulmu_del_usr(wlc->ulmu, scb, FALSE);

		}
	}
	WL_INFORM(("wl%d: %s: max_dlofdma_users: %d admitted %d\n",
		wlc->pub->unit, __FUNCTION__, wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA),
		musched->num_dlofdma_users));

	if (!wlc->pub->up) {
		ASSERT(!musched->num_dlofdma_users);
	}
}

static void
wlc_musched_bsscfg_state_upd(void *ctx, bsscfg_state_upd_data_t *notif_data)
{
	wlc_muscheduler_info_t *musched = (wlc_muscheduler_info_t *) ctx;
	wlc_bsscfg_t *cfg = notif_data->cfg;

	if (notif_data->old_up && !cfg->up) {
		/* up -> down */
		wlc_musched_admit_users_reset(musched, cfg);
	}
}

static void
wlc_musched_scb_state_upd(void *ctx, scb_state_upd_data_t *notif_data)
{
	wlc_muscheduler_info_t *musched = (wlc_muscheduler_info_t *) ctx;
	wlc_info_t *wlc = musched->wlc;
	scb_t *scb;
	wlc_bsscfg_t *bsscfg;
	uint8 oldstate;
	scb_musched_t *musched_scb;
	ASSERT(notif_data);

	scb = notif_data->scb;
	ASSERT(scb);
	oldstate = notif_data->oldstate;
	bsscfg = scb->bsscfg;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(oldstate);

	musched_scb = SCB_MUSCHED(musched, scb);

	if (D11REV_LT(wlc->pub->corerev, 129) || !HE_DLMU_ENAB(wlc->pub) ||
		!SCB_HE_CAP(scb) || !BSSCFG_AP(bsscfg) ||
#ifndef WL11AX
		/* force to return if WL11AX compilation flag is off */
		TRUE ||
#endif /* WL11AX */
		SCB_INTERNAL(scb) || !musched_scb || SCB_LEGACY_WDS(scb)) {
		return;
	}

	if (wlc_txbf_is_mu_bfe(wlc->txbf, scb) &&
	   ((wlc_mutx_is_hedlmmu_enab(wlc->mutx) || HE_DLMU_ENAB(wlc->pub)))) {
		return;
	}

	/* only DL OFDMA capable SCBs come here. */
	if ((!WSEC_ENABLED(bsscfg->wsec) && !(oldstate & ASSOCIATED) &&
		SCB_AUTHENTICATED(scb) && SCB_ASSOCIATED(scb)) ||
		(WSEC_ENABLED(bsscfg->wsec) && SCB_AUTHENTICATED(scb) &&
		SCB_ASSOCIATED(scb) && SCB_AUTHORIZED(scb) && !(oldstate & AUTHORIZED))) {
		/* open security: not assoc -> assoc
		 * security: assoc -> authorized
		 * OFDMA client if all of these are true
		 * 1. PIO is disabled.
		 * 2. SCB has less than or equal number of max allowed streams OFDMA.
		 */
		if (!PIO_ENAB_HW(wlc->wlc_hw) && (musched->num_dlofdma_users <
			wlc_txcfg_max_clients_get(wlc->txcfg, DLOFDMA))) {
			SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, TRUE);

			/* Release all previously allocated MU FIFOs to this client.
			* New FIFOs will be allocated when traffic comes in
			*/
			wlc_fifo_free_all(wlc->fifo, scb);
		}
	} else if ((oldstate & ASSOCIATED) && !SCB_ASSOCIATED(scb)) {
		if (musched_scb->dlul_assoc == TRUE) {
			SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, FALSE);

			/* Release all allocated MU FIFOs to this client. */
			wlc_fifo_free_all(wlc->fifo, scb);
		}
	} else {
		/* pass */
	}

	/* XXX: now let's just use simple policy to enable dl ofdma
	 * Current policy to enable dl ofdma: for AP: AP HE features = 7;
	 * for STA: 1) HE cap; 2) Associated; 3) total # of HE STA > 1
	 */
	wlc_musched_admit_dlclients(musched);
}

int
wlc_musched_set_dlpolicy(wlc_muscheduler_info_t *musched, int16 dl_policy)
{
	int err = BCME_OK;
	wlc_info_t *wlc;
	uint offset;
	uint16 uval16;

	BCM_REFERENCE(offset);
	BCM_REFERENCE(uval16);

	wlc = musched->wlc;

	if (dl_policy > MUSCHED_DL_POLICY_MAX) {
		musched->dl_policy = MUSCHED_DL_POLICY_AUTO;
	} else {
		musched->dl_policy = dl_policy;
	}

	if (!HE_DLMU_ENAB(wlc->pub)) {
		MUSCHED_DLOFDMA_DISABLE(musched);
	} else {
		switch (musched->dl_policy) {
			case MUSCHED_DL_POLICY_DISABLE:
				musched->dl_schidx = MCTL2_INVALID_SCHIDX;
				MUSCHED_DLOFDMA_DISABLE(musched);
				break;
			case MUSCHED_DL_POLICY_FIXED:
				musched->dl_schidx = MCTL2_FIXED_SCHIDX;
				MUSCHED_DLOFDMA_ENABLE(musched);
				break;
			case MUSCHED_DL_POLICY_AUTO:
				/* auto is default to trivial scheduler */
			case MUSCHED_DL_POLICY_TRIVIAL:
				musched->dl_schidx = MCTL2_TRIVIAL_SCHIDX;
				MUSCHED_DLOFDMA_ENABLE(musched);
				break;
			default:
				musched->dl_schidx = MCTL2_INVALID_SCHIDX;
				MUSCHED_DLOFDMA_DISABLE(musched);
				WL_ERROR(("wl%d: %s: incorrect musched dl ofdma sch policy %d\n",
					wlc->pub->unit, __FUNCTION__, musched->dl_policy));
				ASSERT(0);
				break;
		}
	}

#if defined(WL_PSMX)
	if (MUSCHED_DLOFDMA_ENABLE(musched)) {
		if (!wlc->clk) {
			return BCME_NOCLK;
		}
		if (musched->dl_schidx == MCTL2_FIXED_SCHIDX ||
			musched->dl_schidx == MCTL2_TRIVIAL_SCHIDX) {
			offset = MX_HEMSCH0_BLK(wlc);
		} else {
			return BCME_ERROR;
		}
		uval16 = wlc_read_shmx(wlc, offset);
		uval16 &= ~MUSCHED_HEMSCH_SCHIDX_MASK;
		uval16 |= ((musched->dl_schidx << MUSCHED_HEMSCH_SCHIDX_SHIFT) &
			MUSCHED_HEMSCH_SCHIDX_MASK);
		wlc_write_shmx(wlc, offset, uval16);
	}
#endif /* defined(WL_PSMX) */

	WL_MUTX(("wl%d: %s: set musched dl ofdma sch policy %d schidx %d\n",
		musched->wlc->pub->unit, __FUNCTION__,
		musched->dl_policy, musched->dl_schidx));

	return err;
}

bool
wlc_musched_is_dlofdma_allowed(wlc_muscheduler_info_t *musched)
{
	return MUSCHED_DLOFDMA(musched);
}

static int
wlc_musched_get_dlpolicy(wlc_muscheduler_info_t *musched)
{
	return musched->dl_policy;
}

/* ======== scb cubby ======== */

static int
wlc_musched_scb_init(void *ctx, scb_t *scb)
{
	wlc_muscheduler_info_t *musched = ctx;
	wlc_info_t *wlc = musched->wlc;
	scb_musched_t **psh = SCB_MUSCHED_CUBBY(musched, scb);

	ASSERT(*psh == NULL);

	*psh = wlc_scb_sec_cubby_alloc(wlc, scb, wlc_musched_scb_secsz(wlc, scb));

	if (*psh != NULL) {
		wlc_musched_scb_schedule_init(musched, scb);

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
		if (HE_DLMU_ENAB(wlc->pub)) {
			wlc_musched_scb_rustats_init(musched, scb);
		}
#endif // endif
	}

	return BCME_OK;
}

static void
wlc_musched_scb_deinit(void *ctx, scb_t *scb)
{
	wlc_muscheduler_info_t *musched = ctx;
	wlc_info_t *wlc = musched->wlc;
	scb_musched_t **psh = SCB_MUSCHED_CUBBY(musched, scb);
	scb_musched_t *sh = SCB_MUSCHED(musched, scb);

	/* Memory not allocated for scb, return */
	if (!sh) {
		return;
	}

	if (sh->dlul_assoc == TRUE) {
		WL_MUTX(("wl%d: %s STA "MACF" removed\n",
			wlc->pub->unit, __FUNCTION__, ETHER_TO_MACF(scb->ea)));
		SCB_MUSCHED_SET_DLUL_ASSOC(wlc, sh, FALSE);

		/* Release all allocated MU FIFOs to this client. */
		wlc_fifo_free_all(wlc->fifo, scb);
	}

	if (sh->scb_ru_stats != NULL) {
		MFREE(wlc->osh, sh->scb_ru_stats, sizeof(musched_ru_stats_t));
		--musched->num_scb_stats;
		ASSERT(musched->num_scb_stats >= 0);
	}

	wlc_scb_sec_cubby_free(wlc, scb, sh);
	*psh = NULL;
}

static uint
wlc_musched_scb_secsz(void *ctx, scb_t *scb)
{
	if (scb && !SCB_INTERNAL(scb)) {
		return sizeof(scb_musched_t);
	} else {
		return 0;
	}
}

#if defined(BCMDBG) || defined(DL_RU_STATS_DUMP)
static void
wlc_musched_scb_dump(void *ctx, scb_t *scb, bcmstrbuf_t *b)
{
	char musched_str[64];
	wlc_muscheduler_info_t *musched = ctx;
	scb_musched_t *sh = SCB_MUSCHED(musched, scb);

	if (sh == NULL) {
		return;
	}

	bcm_bprintf(b, "     musched_info: %s ", musched_str);
	bcm_bprintf(b, "     DL schpos %d\n",
		sh->dl_schpos);
}

static void
wlc_musched_scb_rustats_init(wlc_muscheduler_info_t *musched, scb_t *scb)
{
	scb_musched_t *musched_scb;
	int max_scb_stats;

	if (PIO_ENAB_HW(musched->wlc->wlc_hw)) {
		return;
	}

	if (SCB_INTERNAL(scb)) {
		return;
	}

	musched_scb = SCB_MUSCHED(musched, scb);
	if (!musched_scb) {
		return;
	}

#if defined(DONGLEBUILD)
	max_scb_stats = MUSCHED_RU_SCB_STATS_NUM;
#else
	max_scb_stats = wlc_txcfg_max_clients_get(musched->wlc->txcfg, DLOFDMA);
#endif // endif
	if (musched->num_scb_stats < max_scb_stats) {
		if ((musched_scb->scb_ru_stats =
			MALLOCZ(musched->wlc->osh, sizeof(musched_ru_stats_t))) != NULL) {
			++musched->num_scb_stats;
		}
	} else {
		musched_scb->scb_ru_stats = NULL;
	}
}
#endif // endif

static void
wlc_musched_scb_schedule_init(wlc_muscheduler_info_t *musched, scb_t *scb)
{
	scb_musched_t *musched_scb;

	if (PIO_ENAB_HW(musched->wlc->wlc_hw)) {
		return;
	}

	if (SCB_INTERNAL(scb)) {
		return;
	}

	musched_scb = SCB_MUSCHED(musched, scb);
	if (!musched_scb) {
		return;
	}

	musched_scb->dl_schpos = MCTL2_INVALID_SCHPOS;
}

static bool
wlc_scbmusched_is_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb)
{
	if (musched->dl_policy == MUSCHED_DL_POLICY_DISABLE ||
		SCB_INTERNAL(scb)) {
		return FALSE;
	}

	return  (SCB_DLOFDMA(scb));
}

static void
wlc_scbmusched_enable_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb, bool enable)
{
	wlc_info_t *wlc;
#ifdef MAC_AUTOTXV_OFF
	uint8 fifo_idx;
#endif // endif
	wlc = musched->wlc;
	BCM_REFERENCE(wlc);

	if (SCB_INTERNAL(scb)) {
		return;
	}

	if (enable) {
		SCB_DLOFDMA_ENABLE(scb);
	} else {
		SCB_DLOFDMA_DISABLE(scb);
	}
#ifdef WLTAF
	wlc_taf_scb_state_update(wlc->taf_handle, scb, TAF_NO_SOURCE, TAF_PARAM(SCB_DLOFDMA(scb)),
		TAF_SCBSTATE_MU_DL_OFDMA);
#endif // endif
	WL_MUTX(("wl%d: %s: %s dl ofdma STA "MACF" %x\n", wlc->pub->unit,
		__FUNCTION__, enable ? "Enable" : "Disable", ETHER_TO_MACF(scb->ea),
		SCB_DLOFDMA(scb)));

#ifdef MAC_AUTOTXV_OFF
	if (wlc_scbmusched_fifogrpidx_get(musched, scb, &fifo_idx) == BCME_OK) {
		/* set tbcap in bfi config */
		wlc_txbf_tbcap_update(wlc->txbf, scb);
		if (wlc_macreq_upd_bfi(wlc, scb, fifo_idx, enable) != BCME_OK) {
			WL_ERROR(("wl%d: %s MAC command failed to update bfi"
			" for STA "MACF" index %d\n", wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea), fifo_idx));
		}
	}
#else
	/* set tbcap in bfi config */
	wlc_txbf_tbcap_update(wlc->txbf, scb);
#endif // endif
}

/* Function to check eligibility of a SCB/client to be admitted as DL-OFDMA */
bool
wlc_musched_scb_isdlofdma_eligible(wlc_muscheduler_info_t *musched, scb_t* scb)
{
	bool ret = FALSE;
	scb_musched_t *musched_scb = SCB_MUSCHED(musched, scb);
	if (musched_scb && SCB_HE_CAP(scb) &&
		HE_DLMU_ENAB(musched->wlc->pub) && !(musched->wlc->dsmps_war &&
		wlc_stf_is_scb_dynamic_smps(musched->wlc, scb)) &&
		BSSCFG_AP(SCB_BSSCFG(scb)) && !SCB_HEMMU(scb) && !SCB_LEGACY_WDS(scb)) {
		ret = TRUE;
	}
	return ret;
}

/* API: turn on/off DLOFDMA for scb by other module */
void
wlc_scbmusched_set_dlofdma(wlc_muscheduler_info_t *musched, scb_t* scb, bool enable)
{
	scb_musched_t *musched_scb;
	wlc_info_t *wlc;

	wlc = musched->wlc;

	if (SCB_INTERNAL(scb)) {
		return;
	}

	if ((musched_scb = SCB_MUSCHED(musched, scb)) == NULL) {
		WL_ERROR(("wl%d: %s: Fail to get musched scb cubby STA "MACF"\n",
			wlc->pub->unit, __FUNCTION__,
			ETHER_TO_MACF(scb->ea)));
		return;
	}

	/* apply setting to scb_cubby dlul_assoc */
	if (wlc_mutx_is_hedlmmu_enab(wlc->mutx) || HE_DLMU_ENAB(wlc->pub)) {
		if (enable) {
			SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, TRUE);
		} else {
			SCB_MUSCHED_SET_DLUL_ASSOC(wlc, musched_scb, FALSE);
		}
	}

	/* enable/disable scb dlofdma */
	wlc_scbmusched_enable_dlofdma(musched, scb, enable);
}

/* function to configure musched scb dl mu scheduler position for scb */
int
wlc_scbmusched_set_dlschpos(wlc_muscheduler_info_t *musched, scb_t* scb, int8 schpos)
{
	scb_musched_t *musched_scb;
	int err;

	err = BCME_OK;

	if (SCB_INTERNAL(scb)) {
		return BCME_UNSUPPORTED;
	}

	musched_scb = SCB_MUSCHED(musched, scb);

	if (musched_scb == NULL) {
		return BCME_ERROR;
	}

	musched_scb->dl_schpos = schpos;

	return err;
}

/* function to get musched scb dl mu scheduler info */
int
wlc_scbmusched_get_dlsch(wlc_muscheduler_info_t *musched, scb_t* scb, int8* schidx, int8* schpos)
{
	scb_musched_t *musched_scb;
	int err;

	err = BCME_OK;

	if (SCB_INTERNAL(scb)) {
		return BCME_UNSUPPORTED;
	}

	musched_scb = SCB_MUSCHED(musched, scb);

	if (musched_scb == NULL) {
		return BCME_ERROR;
	}

	*schidx = musched->dl_schidx;
	*schpos = musched_scb->dl_schpos;

	return err;
}

static void
wlc_musched_config_mix_ackp(wlc_muscheduler_info_t *musched)
{
	uint16 uval16;
	wlc_info_t *wlc = musched->wlc;
	uval16 = musched->mix_ackp ? MXHF1_MIXACKP : 0;
	BCM_REFERENCE(uval16);
#if defined(WL_PSMX)
	wlc_bmac_mhf(wlc->hw, MXHF1, MXHF1_MIXACKP, uval16, WLC_BAND_ALL);
#endif /* WL_PSMX */
}

static void
wlc_musched_config_ru_alloc_type(wlc_muscheduler_info_t *musched)
{
	uint16 uval16 = 0;
	wlc_info_t *wlc = musched->wlc;

	/* set to use rucfg table */
	if (wlc_musched_get_rualloctype(musched) == MUSCHED_RUALLOC_UCODERU) {
		uval16 |= MXHF1_RUALLOC;
	}
	wlc_bmac_mhf(wlc->hw, MXHF1, MXHF1_RUALLOC, uval16, WLC_BAND_ALL);
}

uint8
wlc_musched_get_rualloctype(wlc_muscheduler_info_t *musched)
{
	wlc_info_t *wlc = musched->wlc;
	if (D11REV_GE(wlc->pub->corerev, 129) && D11REV_LT(wlc->pub->corerev, 132)) {
		return (musched->rualloc <= MUSCHED_RUALLOC_AUTO) ?
			MUSCHED_RUALLOC_UCODERU : musched->rualloc;
	} else {
		return (musched->rualloc <= MUSCHED_RUALLOC_AUTO) ?
			MUSCHED_RUALLOC_RUCFG : musched->rualloc;
	}
}

static int
wlc_musched_upd_rucfg_rmem(wlc_muscheduler_info_t *musched, uint8 bw_idx)
{
	wlc_info_t *wlc = musched->wlc;
	int i, j, rate_blk_le8, rate_blk_gt8;
	uint16 *rutbl_buf = NULL;
	uint16 *ptr = NULL;
	int ret = BCME_OK;

	if ((rutbl_buf = MALLOCZ(wlc->osh, MUSCHED_RUCFG_TBL_BSZ)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory for rucfg, %d / %d bytes\n", wlc->pub->unit,
			__FUNCTION__, MALLOCED(wlc->osh), MUSCHED_RUCFG_TBL_BSZ));
		ret = BCME_NOMEM;
		goto done;
	}

	STATIC_ASSERT(MUSCHED_RUCFG_LE8_BW20_40_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_LE8_BW20_40_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_LE8_BW80_160_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_LE8_BW80_160_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW20_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW20_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW40_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW40_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW80_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW80_RTMEM < AMT_IDX_SIZE_11AX);
	STATIC_ASSERT(MUSCHED_RUCFG_GT8_BW160_RTMEM >= AMT_IDX_RLM_RSVD_START &&
		MUSCHED_RUCFG_GT8_BW160_RTMEM < AMT_IDX_SIZE_11AX);

	switch (bw_idx) {
	case D11_REV128_BW_20MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW20_40_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW20_RTMEM;
		break;
	case D11_REV128_BW_40MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW20_40_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW40_RTMEM;
		break;
	case D11_REV128_BW_80MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW80_160_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW80_RTMEM;
		break;
	case D11_REV128_BW_160MHZ:
		rate_blk_le8 = MUSCHED_RUCFG_LE8_BW80_160_RTMEM;
		rate_blk_gt8 = MUSCHED_RUCFG_GT8_BW160_RTMEM;
		break;
	default:
		ret = BCME_ERROR;
		goto done;
	}

	/* Preparing table for row 1 - 8 */
	ptr = rutbl_buf;
	for (i = 0; i < MUSCHED_RUCFG_ROW; i++) {
		for (j = 0; j < MUSCHED_RUCFG_LE8_NUM_COLS; j++) {
			if (i < MUSCHED_RUCFG_LE8_NUM_ROWS) {
				/* write upper half of the table. bw20/bw80 is on upper half */
				*ptr++ = (musched->rucfg_ack[bw_idx & (~1)][i][j] <<
					MUSCHED_RUCFG_ACK_SHIFT) |
					musched->rucfg[bw_idx & (~1)][i][j];
			} else {
				/* write bottmo half of the table. bw40/bw160 is on lower half */
				*ptr++ = (musched->rucfg_ack[bw_idx | 1][i-
					MUSCHED_RUCFG_LE8_NUM_ROWS][j] << MUSCHED_RUCFG_ACK_SHIFT) |
					musched->rucfg[bw_idx | 1][i-MUSCHED_RUCFG_LE8_NUM_ROWS][j];
			}
		}
	}

	wlc_bmac_suspend_macx_and_wait(wlc->hw);
	/* Writing table for rows 1 - 8 */
	wlc_ratelinkmem_write_rucfg(wlc, (uint8*)rutbl_buf, MUSCHED_RUCFG_TBL_BSZ, rate_blk_le8);

	/* Preparing table for rows 9 - 16 */
	ptr = rutbl_buf;
	for (i = 0; i < MUSCHED_RUCFG_GT8_NUM_ROWS; i++) {
		for (j = 0; j < MUSCHED_RUCFG_GT8_NUM_COLS; j++) {
			*ptr++ = (musched->rucfg_ack[bw_idx][i+MUSCHED_RUCFG_LE8_NUM_ROWS][j] <<
				MUSCHED_RUCFG_ACK_SHIFT) |
				musched->rucfg[bw_idx][i+MUSCHED_RUCFG_LE8_NUM_ROWS][j];
		}
	}

	/* Writing table for rows 9 - 16 */
	wlc_ratelinkmem_write_rucfg(wlc, (uint8*)rutbl_buf, MUSCHED_RUCFG_TBL_BSZ, rate_blk_gt8);
	wlc_bmac_enable_macx(wlc->hw);

done:
	if (rutbl_buf) {
		MFREE(wlc->osh, rutbl_buf, MUSCHED_RUCFG_TBL_BSZ);
	}
	return ret;
}

static int
wlc_musched_minru_update(wlc_muscheduler_info_t *musched, scb_t *scb, uint8 minru)
{
	uint8 bw, link_bw;
	scb_musched_t* musched_scb;
	wlc_info_t *wlc = musched->wlc;

	bw = wlc_scb_ratesel_get_link_bw(wlc, scb);

	if (bw == BW_160MHZ) {
		link_bw = D11_REV128_BW_160MHZ;
	} else if (bw == BW_80MHZ) {
		link_bw = D11_REV128_BW_80MHZ;
	} else if (bw == BW_40MHZ) {
		link_bw = D11_REV128_BW_40MHZ;
	} else {
		link_bw = D11_REV128_BW_20MHZ;
	}

	if (minru > per_bw_max_minru[link_bw])
		return BCME_RANGE;

	if (!HE_DLMU_ENAB(musched->wlc->pub) || SCB_INTERNAL(scb) || !SCB_HE_CAP(scb))
		return BCME_UNSUPPORTED;

	musched_scb = SCB_MUSCHED(musched, scb);
	if (musched_scb) {
		musched_scb->min_ru = minru;
	}

	return wlc_ratelinkmem_update_link_entry(wlc, scb);
}

void
wlc_musched_fill_link_entry(wlc_muscheduler_info_t *musched, wlc_bsscfg_t *cfg, scb_t *scb,
	d11linkmem_entry_t *link_entry)
{
	scb_musched_t* musched_scb;

	ASSERT(link_entry != NULL);

	musched_scb = SCB_MUSCHED(musched, scb);

	if (musched_scb) {
		link_entry->fragTx_minru &= ~D11_REV128_MINRU_MASK;
		link_entry->fragTx_minru |= musched_scb->min_ru << D11_REV128_MINRU_SHIFT;
	}
}

void
wlc_musched_link_entry_dump(d11linkmem_entry_t *link_entry, bcmstrbuf_t *b)
{
	bcm_bprintf(b, "\t\tminRU: 0x%02x \n", link_entry->fragTx_minru >> D11_REV128_MINRU_SHIFT);
}

bool
wlc_musched_wfa20in80_enab(wlc_muscheduler_info_t *musched)
{
	return (musched->wfa20in80);
}

#endif /* WL_MUSCHEDULER */
