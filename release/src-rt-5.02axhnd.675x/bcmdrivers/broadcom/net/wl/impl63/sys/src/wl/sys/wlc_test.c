/*
 * WLTEST related source code.
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wlc_test.c 781412 2019-11-20 11:35:46Z $
 */
/* XXX: Define wlc_cfg.h to be the first header file included as some builds
 * get their feature flags thru this file.
 */
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_rate_sel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <bcmsrom.h>
#include <bcmnvram.h>
#include <wlc_bmac.h>
#include <wlc_txbf.h>
#include <wlc_tx.h>
#include <wlc_test.h>
#include <wlc_dump.h>
#include <wlc_iocv.h>
#include <wlc_hw_priv.h>
#include <wlc_txtime.h>
#include <wlc_ampdu.h>
#include <wlc_objregistry.h>
#include <wl_export.h>
#ifdef WLSMC
#include <wlc_smc.h>
#include <d11smc_code.h>
#endif // endif
#include <wlc_ratelinkmem.h>

#include <wlc_he.h>
#include <wlc_stf.h>
#ifdef WL_MUPKTENG
#include <wlc_mutx.h>
#endif // endif
#include <wlc_musched.h>
#include <wlc_hrt.h>
#include <wlc_ht.h>
#include <wlc_ap.h>
#include <wlc_tso.h>

#define PKTENG_PKTBUFSZ			(PKTBUFSZ)
#define MPDU_OVERHEAD			(DOT11_MGMT_HDR_LEN + TXOFF)
#define PKTENG_MPDU_LEN_2K		(PKTBUFSZ - MPDU_OVERHEAD)
/*
 * Need bigger MPDU len for higher MCS
 * Use multiple of PKTBUFSZ to avoid small fragment
 */
#define PKTENG_MPDU_LEN_4K		((PKTBUFSZ * 2) - MPDU_OVERHEAD)
#define PKTENG_MPDU_LEN_8K		((PKTBUFSZ * 4) - MPDU_OVERHEAD)
#define PKTENG_MPDU_LEN_11K		((VHT_MPDU_LIMIT_11K) - MPDU_OVERHEAD)
#define PKTENG_MAXLEN_16K		(16 * 1024)
#define PKTENG_MAXLEN_64K		(64 * 1024)
#define PKTENG_MAXLEN_160K		(160 * 1024)
#define PKTENG_MAXLEN_320K		(320 * 1024)
#define HEMSCH_USRSZ		4		/* user block is 4B */
#define PKTENG_PARAM_CHKCNT(x, y) do {	\
	if ((x) < (y)) {		\
		err = BCME_BADARG;	\
		break;			\
	}} while (0)

#define TU2uS(tu) ((uint32)(tu) * 1024)
#define uS2TU(us) ((uint32)(us) / 1024)
#define SIM_PM_DEF_CYCLE 100
#define SIM_PM_DEF_UP 5
/* RU packet engine related */
#define RUIDX_160MHZ_80U		128
#define SCALE_FACT			8
#define SYM_SZ_SCALE			10
#define WL_LDPC_SHIFT			22
#define WL_STBC_SHIFT			20
#define SCALE_CODE			12
#define MCS_MAX_IDX			12
#define MAX_RU_TONES			6
#define MAX_HE_LTFSYMBSZ_IDX		3
#define MAX_DATA_SYMBSZ_IDX		3
#define MAX_PESZ_IDX			3
#define MAX_AFACT_ENCODE_IDX		5
#define M_RXTRIG_CMNINFO_LOC(x, i)	(M_RXTRIG_CMNINFO(x) + ((i) << 1))
#define M_RXTRIG_USRINFO_LOC(x, i)	(M_RXTRIG_USRINFO(x) + ((i) << 1))
#define LEFT_SHIFT(x, i)		((x) << i)
#define RIGHT_SHIFT(x, i)		((x) >> i)
#define PKTENG_TRG_TMR_DELAY_CORRECTION	200		/* usec */
#define PKTENG_PKT_TMR_DELAY_CORRECTION	200		/* usec */

typedef enum {
	SIM_PM_STATE_DISABLE = 0,
	SIM_PM_STATE_AWAKE = 1,
	SIM_PM_STATE_ASLEEP = 2
} sim_pm_state_t;

typedef enum {
	TXTRIG = 0,
	RXTRIG = 1
} triginfo_txrx_t;

/* IOVar table - please number the enumerators explicity */
enum {
	IOV_PKTENG = 0,
	IOV_PKTENG_MAXLEN = 1,	/* max packet size limit for packet engine */
	IOV_NVRAM_GET = 2,
	IOV_NVRAM_DUMP = 3,
	IOV_CISWRITE = 4,
	IOV_CISDUMP = 5,
	IOV_MANF_INFO = 6,
	IOV_LONGPKT = 7,	/* Enable long pkts for pktengine */
	IOV_PKTENG_STATUS = 8,
	IOV_SIM_PM = 9,
	IOV_LOAD_TRIG_INFO = 10,
	IOV_CCGPIOCTRL = 11,	/* CC GPIOCTRL REG */
	IOV_CCGPIOIN = 12,	/* CC GPIOIN REG */
	IOV_CCGPIOOUT = 13,	/* CC GPIOOUT REG */
	IOV_CCGPIOOUTEN = 14,	/* CC GPIOOUTEN REG */
	IOV_CCGPIOFNSEL_GPIO = 15,
	IOV_CCGPIOFNSEL_FUNC = 16,
	IOV_PKTENG_CMD = 17,	/* pkteng cmds setting */
	IOV_PKTENG_TRIG_TMR = 18,
	IOV_PKTENG_PKT_TMR = 19,
	IOV_GET_TRIG_INFO = 20,
	IOV_LAST
};

static const bcm_iovar_t test_iovars[] = {
#if defined(WLTEST) || defined(WLPKTENG)
	{"pkteng", IOV_PKTENG, IOVF_SET_UP | IOVF_MFG, 0, IOVT_BUFFER, sizeof(wl_pkteng_t)},
	{"pkteng_ru_fill", IOV_LOAD_TRIG_INFO, IOVF_SET_UP | IOVF_MFG, 0, IOVT_BUFFER,
	sizeof(wl_pkteng_ru_fill_t)},
	{"get_trig_info", IOV_GET_TRIG_INFO, IOVF_GET_UP | IOVF_MFG, 0, IOVT_BUFFER,
	sizeof(wl_trig_frame_info_t)},
	{"pkteng_maxlen", IOV_PKTENG_MAXLEN, IOVF_SET_UP | IOVF_MFG, 0, IOVT_UINT32, 0},
	{"pkteng_status", IOV_PKTENG_STATUS, IOVF_GET_UP | IOVF_MFG, 0, IOVT_BOOL, 0},
	{"longpkt", IOV_LONGPKT, (IOVF_SET_UP | IOVF_GET_UP), 0, IOVT_INT16, 0},
	{"pkteng_cmd", IOV_PKTENG_CMD, IOVF_SET_UP | IOVF_MFG, 0, IOVT_BUFFER,
	sizeof(wl_pkteng_cmd_params_t)},
#endif // endif
#if defined(WLTEST)
	{"nvram_get", IOV_NVRAM_GET, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"nvram_dump", IOV_NVRAM_DUMP, IOVF_MFG, 0, IOVT_BUFFER, 0},
	{"ciswrite", IOV_CISWRITE, IOVF_MFG, 0, IOVT_BUFFER, sizeof(cis_rw_t)},
	{"manfinfo", IOV_MANF_INFO, 0, 0, IOVT_BUFFER, 0},
	{"sim_pm", IOV_SIM_PM, (IOVF_MFG | IOVF_SET_UP), 0, IOVT_BUFFER, sizeof(uint32)},
#endif // endif
	{"cisdump", IOV_CISDUMP, 0, 0, IOVT_BUFFER, sizeof(cis_rw_t)},
	{"ccgpioctrl", IOV_CCGPIOCTRL, 0, 0, IOVT_UINT32, 0},
	{"ccgpioin", IOV_CCGPIOIN, 0, 0, IOVT_UINT32, 0},
	{"ccgpioout", IOV_CCGPIOOUT, 0, 0, IOVT_UINT32, 0},
	{"ccgpioouten", IOV_CCGPIOOUTEN, 0, 0, IOVT_UINT32, 0},
	{"ccgpiofnsel_gpio", IOV_CCGPIOFNSEL_GPIO, 0, 0, IOVT_UINT32, 0},
	{"ccgpiofnsel_func", IOV_CCGPIOFNSEL_FUNC, 0, 0, IOVT_UINT32, 0},
#ifdef TESTBED_AP_11AX
	{"pkteng_trgtmr", IOV_PKTENG_TRIG_TMR, 0, 0, IOVT_BOOL, 0},
	{"pkteng_pkttmr", IOV_PKTENG_PKT_TMR, 0, 0, IOVT_BOOL, 0},
#endif /* TESTBED_AP_11AX */
	{NULL, 0, 0, 0, 0, 0},
};

#if defined(WLTEST)
#define MANF_INFO_LEN			8
#define MANF_INFO_ROW_WIDTH		64
struct wlc_otp_manf_info {
	const char *name;	/* name for the segment */
	uint16	bit_pos_start;	/* start position for the segment */
	uint16	bit_pos_end;	/* end position for the segment */
	uint16	len;		/* length of the segment */
};

/** MFG OTP info twiki: Mwgroup/OtpProgramming#ATE_test_flow */
static const struct wlc_otp_manf_info wlc_manf_info[] = {
	/* row0: wafer sort data */
	{"OTP_LOT_NUM", 0, 16, 17},
	{"WAFER_NUM", 17, 21, 5},
	{"WAFER_X", 22, 30, 9},
	{"WAFER_Y", 31, 39, 9},
	{"PROG_REL_DATE", 40, 55, 16},
	{"PROG_REV_CRTL_0", 56, 60, 5},
	{"MEM_REP_0", 61, 61, 1},
	{"PROBED_BIN1", 62, 62, 1},
	{"LOCK_BIT_0", 63, 63, 1},

	/* row1: final(packaging) test data */
	{"FT1_PROG_REL", 0, 15, 16},
	{"FT2_PROG_REL", 16, 31, 16},
	{"FT_PROG_RESCRN", 32, 47, 16},
	{"PROG_REV_CTRL_1", 48, 52, 5},
	{"MEM_REP_1", 53, 53, 1},
	{"ANALOG_TRIM", 54, 54, 1},
	{"SCREEN_BIT", 55, 59, 5},
	{"QA_SAMP_TEST", 60, 61, 2},
	{"FT_BIN1", 62, 62, 1},
	{"LOCK_BIT_1", 63, 63, 1},

	{NULL, 0, 0, 0},
};
#endif // endif

#if defined(WLTEST) || defined(WLPKTENG)
#define WLC_TEST_PKTENG_MAXUSR		16
#define WLC_TEST_PKTENG_MAXFIFO		4
#define WLC_TEST_PKTENG_MAXQID		69

#define WLC_TEST_PKTENG_DFLT_NUSR	4
#define WLC_TEST_PKTENG_DFLT_NFIFO	1

const struct ether_addr pkteng_base_addr = {
	{0x0, 0x11, 0x22, 0x33, 0x44, 0}
};

typedef enum {
	PKTENGCMD_OPCODE_PRIV_START = PKTENGCMD_OPCODE_PUB_END,
	/* start of reg op code */
	PKTENGCMD_OPCODE_PRIV_ARGINT_SHM = PKTENGCMD_OPCODE_PRIV_START, /* write shm */
	PKTENGCMD_OPCODE_PRIV_ARGINT_SHMX,
	PKTENGCMD_OPCODE_PRIV_ARGINT_SHM1,
	PKTENGCMD_OPCODE_PRIV_ARGINT_IHR,
	PKTENGCMD_OPCODE_PRIV_ARGINT_IHR32,
	PKTENGCMD_OPCODE_PRIV_ARGINT_IHRX,
	PKTENGCMD_OPCODE_PRIV_ARGINT_IHR1,
	/* end of reg op code */
	PKTENGCMD_OPCODE_PRIV_GET_TEST = 255
} wlc_test_pecmd_private_opcode_t;

#define WLC_TEST_SIGA_SZ	3
#define WLC_TEST_PHYCTL_SZ	5

#define WLC_TEST_PKTENGCMD_DFLT_RSPEC		0xc3c00419

#define WLC_TEST_PKTENGCMD_VALID				(1 << 0)
#define WLC_TEST_PKTENGCMD_UL_DISABLE				(1 << 1)

#define WLC_TEST_PKTENGCMD_FLAG_RESET				0x0000
#define WLC_TEST_PKTENGCMD_FLAG_EN				0x0001
#define WLC_TEST_PKTENGCMD_FLAG_RUN				0x0002
#define WLC_TEST_PKTENGCMD_FLAG_COMMITED			0x0004
#define WLC_TEST_PKTENGCMD_FLAG_UPDATED				0x0008
#define WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA			0x0010
#define WLC_TEST_PKTENGCMD_FLAG_VERBOSE				0x0020
#define WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO			0x0040
#define WLC_TEST_PKTENGCMD_FLAG_DLSCH				0x0080
#define WLC_TEST_PKTENGCMD_FLAG_ULSCH				0x0100

#define WLC_TEST_SCHTYPE_DLSCH		0x1
#define WLC_TEST_SCHTYPE_ULSCH		0x2

/* HE trig frame typde-depedent per user info */
#define HETRIG_UITPDEP_TIDAGGLMT_SHIFT		2	/* TID Agg Limit */
#define HETRIG_UITPDEP_TIDAGGLMT_MASK		0x1C

#define WLC_TEST_PKTENGCMD_IS_VERBOSE(x)	(((x) & WLC_TEST_PKTENGCMD_FLAG_VERBOSE) != 0)

#define WLC_TEST_PKTENGCMD_DO_COMMIT(flag) do {		\
	(flag) &= ~WLC_TEST_PKTENGCMD_FLAG_UPDATED;	\
	(flag) |= WLC_TEST_PKTENGCMD_FLAG_COMMITED;	\
	(flag) |= WLC_TEST_PKTENGCMD_FLAG_EN;		\
	} while (0)

#define WLC_TEST_PKTENGCMD_DO_UPDATE(flag) do {		\
	(flag) &= ~WLC_TEST_PKTENGCMD_FLAG_COMMITED;	\
	(flag) |= WLC_TEST_PKTENGCMD_FLAG_UPDATED;	\
	} while (0)

#define WLC_TEST_PKTENGCMD_DO_VALID(flag) do {		\
	(flag) |= WLC_TEST_PKTENGCMD_VALID;		\
	} while (0)

#define WLC_TEST_PKTENGCMD_DO_INVALID(flag) do {	\
	(flag) &= ~WLC_TEST_PKTENGCMD_VALID;		\
	} while (0)

#define WLC_TEST_PKTENGCMD_DO_UL_DISABLE(flag) do {	\
	(flag) |= WLC_TEST_PKTENGCMD_UL_DISABLE;	\
	} while (0)

#define WLC_TEST_PKTENGCMD_DO_UL_ENABLE(flag) do {	\
	(flag) &= ~WLC_TEST_PKTENGCMD_UL_DISABLE;	\
	} while (0)

#define WLC_TEST_PKTENGCMD_CLR(flag) do {		\
	(flag) = 0;					\
	} while (0)

#define WLC_TEST_PKTENGCMD_SET_DLSCH(flag) do {	\
	(flag) |= WLC_TEST_PKTENGCMD_FLAG_DLSCH;	\
	} while (0)

#define WLC_TEST_PKTENGCMD_SET_ULSCH(flag) do {	\
	(flag) |= WLC_TEST_PKTENGCMD_FLAG_ULSCH;	\
	} while (0)

#define WLC_TEST_PKTENGCMD_IS_DLSCH(flag) (((flag) & WLC_TEST_PKTENGCMD_FLAG_DLSCH) != 0)
#define WLC_TEST_PKTENGCMD_IS_ULSCH(flag) (((flag) & WLC_TEST_PKTENGCMD_FLAG_ULSCH) != 0)
#define WLC_TEST_PKTENGCMD_IS_VALID(flag) (((flag) & WLC_TEST_PKTENGCMD_VALID) != 0)
#define WLC_TEST_PKTENGCMD_IS_ULDISABLED(flag) (((flag) & WLC_TEST_PKTENGCMD_UL_DISABLE) != 0)

#define WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_SHFT		0
#define WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_MASK		0x000f
#define WLC_PKTENGCMD_TXTRIGFLAG_FRMTYPE_SHIFT		4
#define WLC_PKTENGCMD_TXTRIGFLAG_FRMTYPE_MASK		0x00f0
#define WLC_PKTENGCMD_ULOFDMA_TXTRIG_CNT_MODE		1 /* bit-0: enable */
#define WLC_PKTENGCMD_ULOFDMA_TXTRIG_SCH_MODE		9 /* bit-0: enable; bit-3: refill mode */

#define WLC_TEST_PKTENGCMD_ULTX_STS_BLOCK_NBIT		1 /* ulofdma blk tx until rx trigger */
/* pkteng per fifo info */
typedef struct wlc_test_pkteng_fifoinfo {
	uint16			flags;
	uint8			idx;
	uint8			tid;
	uint8			pattern;
	uint16			ampdu_mpdu;
	uint32			frame_len;
} wlc_test_pkteng_fifoinfo_t;

/* pkteng per user info */
typedef struct wlc_test_pkteng_usrinfo {
	uint16				flags;
	uint8				idx;
	uint16				aid;
	uint16				ruidx;
	ratespec_t			rspec;
	uint8				bfmidx;
	uint8				bfmen;
	uint8				dcm;			/* for UL */
	uint8				ss_alloc;		/* for UL */
	uint8				type_dep_usr_info;	/* for UL */
	uint8				target_rssi;		/* for UL */
	uint8				txpwr;
	uint8				nfifo;	/* number of fifos */
	wlc_test_pkteng_fifoinfo_t	fifo[WLC_TEST_PKTENG_MAXFIFO];
	struct ether_addr		ea;
	struct scb			*scb;
	/* For Scheduler */
	int8				dl_schid;	/* downlink sch id */
	int8				dl_schpos;	/* downlink sch pos */
	int8				ul_schid;	/* uplink sch id */
	int8				ul_schpos;	/* uplink sch pos */
} wlc_test_pkteng_usrinfo_t;

/* pkteng shared info */
struct wlc_test_pkteng_info {
	/* Start debug print settings */
	uint8				verbose;
	uint8				cur_usr;	/* current targeted usr */
	uint8				cur_q;		/* current targeted q */
	/* End debug print settings */
	uint16				flags;
	uint16				mode;		/* pkteng run mode */
	uint16				ifs;		/* inter-frame space */
	uint16				state;		/* scheduler state */
	uint32				nframes;	/* num of test frames */
	uint16				siga[WLC_TEST_SIGA_SZ];
	uint16				phyctl[WLC_TEST_PHYCTL_SZ];
	ratespec_t			rspec;
	uint16				min_dur;	/* min ampdu dur */
	uint16				max_dur;	/* ppdu max dur or TxOP for ul */
	bool				ul_auto_ru;	/* auto ru idx calculation for trigger */
	/* Common Info field */
	uint8				txtrig_type;	/* trigger frame type */
	uint16				ul_lsig_len;	/* lsig len for uplink */
	bool				ul_more_tf;	/* more TF */
	int8				ul_cs_req;	/* cs_required for uplink */
	int8				ul_bw;		/* bw for uplink */
	int8				ul_cp_ltftype;	/* cp_ltf_type for uplink; use spec def */
	int8				ul_mu_mimo_ltf;	/* MU-MIMO LTF Mode */
	int8				ul_numheltf;	/* num he-ltf */
	bool				ul_stbc;	/* Uplink STBC */
	bool				ul_ldpc_extsym;	/* LDPC extra symb */
	int8				ul_ap_txpwr;	/* ap_txpwr for uplink */
	int8				ul_afact;	/* a_factor (2bit) */
	int8				ul_pe_disambig;	/* PE disambig (1bit) */
	int8				ul_spat_reuse;	/* spatial re-use */
	int8				ul_doppler;	/* doppler indx */
	int8				ul_hesiga2_rsvd; /* HE-SIGA2 rsvd */
	int8				ul_rsvd;	/* rsvd */
	int8				nbyte_pad;	/* bypte of padding for uplink */
	uint8				nusrs;
	bool				ulsched_enab;
	uint8				usrlist[WLC_TEST_PKTENG_MAXUSR];
	wlc_test_pkteng_usrinfo_t	usr[WLC_TEST_PKTENG_MAXUSR];
};
#endif // endif

typedef struct wlc_test_pkteng_info wlc_test_pkteng_info_t;

typedef struct wlc_test_cmn_info {
	int sim_pm_saved_PM;		/* PM state to return to */
	sim_pm_state_t sim_pm_state;	/* sim_pm state */
	uint32 sim_pm_cycle;		/* sim_pm cycle time in us */
	uint32 sim_pm_up;		/* sim_pm up time in us */
	struct wl_timer *sim_pm_timer;	/* timer to create simulated PM wake / sleep pattern */
	int16 ccgpiofnsel_gpio;
} wlc_test_cmn_info_t;

/* private info */
struct wlc_test_info {
	wlc_info_t *wlc;
	wlc_test_cmn_info_t *cmn;

	/* For D11 DMA loopback test */
	uint16 rxbmmap_save;
	uint16 fifosel_save;
	uint32 d11_dma_lpbk_init;

	/* for ofdma pkteng eng */
	wlc_test_pkteng_info_t *pkteng_info;
#ifdef TESTBED_AP_11AX
	uint32 pkteng_trig_tmr;
	wlc_hrt_to_t *trigger_timer;		/* trigger schedule timer */
	uint32 pkteng_pkt_tmr;
	wlc_hrt_to_t *packet_timer;		/* packet schedule timer */
#endif /* TESTBED_AP_11AX */
};

/* local functions */
static int wlc_test_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif);
static int wlc_test_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif);

#if (defined(WLTEST) && !defined(WLTEST_DISABLED)) || defined(WLPKTENG)
static void *wlc_tx_testframe_get(wlc_info_t *wlc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len, void *src_pkt,
	void *userdata, int userdata_len);
static bool wlc_tx_testframe_len_dur_isvalid(wlc_info_t *wlc, ratespec_t rate_override, uint len);
static int wlc_test_pkteng_trig_fillinfo(wlc_hw_info_t *wlc_hw,
		wl_pkteng_ru_fill_t *pkteng_ru_fill);
static int wlc_test_compute_trig_info(wl_pkteng_ru_fill_t *pkteng_trig_fill,
	he_trig_cmninfo_set_t *cmn_f, he_trig_usrinfo_set_t *usr_f);
static int wlc_test_get_trig_info(wlc_hw_info_t *wlc_hw, wl_trig_frame_info_t *trig_frame_info);

static int wlc_test_pkteng_cmd_set(wlc_test_info_t *test, wl_pkteng_cmd_params_t* params);
static int wlc_test_pkteng_cmd_getusr(wlc_test_info_t *test,
	wl_pkteng_cmd_params_t *params, char *outbuf, int outlen);
static int wlc_test_pkteng_cmd_getkey(wlc_test_info_t *test,
	wl_pkteng_cmd_params_t *params, char *outbuf, int outlen);
static int wlc_test_pkteng_cmd_init(wlc_test_info_t *test);
static int wlc_test_pkteng_cmd_use_rspec(wlc_test_info_t *test);
static int wlc_test_pkteng_addsta(wlc_test_info_t *test, int usridx);
static int wlc_test_pkteng_delsta(wlc_test_info_t *test, int usridx);
static int wlc_test_pkteng_cmd_setkey(wlc_test_info_t *test, wl_pkteng_cmd_params_t* params);

static uint wlc_test_incremental_write_shm(wlc_hw_info_t *wlc_hw,
uint start_offset, uint16 *val16, int num);

#if defined(WL_PSMX)
static uint wlc_test_incremental_write_shmx(wlc_hw_info_t *wlc_hw,
uint start_offset, uint16 *val16, int num);
static int wlc_test_pkteng_set_shmx_block(wlc_test_info_t *test);
#endif /* defined(WL_PSMX) */

static int wlc_test_pkteng_ofdma_dl_tx(wlc_test_info_t *test, uint32 nframes, uint16 ifs);
static int wlc_test_pkteng_prepare_for_trigger_frame(wlc_test_info_t *test,
	triginfo_txrx_t triginfo_txrx);
static void * wlc_test_gen_testframe(wlc_info_t *wlc, struct scb *scb, struct ether_addr *sa,
	ratespec_t rate_override, int fifo, int length, uint16 seq, uint8 ac);
#endif // endif

#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
static int wlc_manf_info_get(wlc_info_t *wlc, char *buf, int len, struct wlc_if *wlcif);
static void wlc_sim_pm_timer(void *arg);
#endif // endif

#if defined(WLTEST)
static int wlc_nvram_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif // endif

#ifdef TESTBED_AP_11AX
static void update_pkt_eng_trg_tmr(wlc_info_t *wlc, uint32 trg_tmr);
static void update_pkt_eng_pkt_tmr(wlc_info_t *wlc, uint32 pkt_tmr);
#endif /* TESTBED_AP_11AX */

/* This includes the auto generated ROM IOCTL/IOVAR patch handler C source file (if auto patching is
 * enabled). It must be included after the prototypes and declarations above (since the generated
 * source file may reference private constants, types, variables, and functions).
 */
#include <wlc_patch.h>

#if defined(WLTEST)
static int
wlc_nvram_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	char *nvram_vars;
	const char *q = NULL;
	int err;

	/* per-device vars first, if any */
	if (wlc->pub->vars) {
		q = wlc->pub->vars;
		/* loop to copy vars which contain null separated strings */
		while (*q != '\0') {
			bcm_bprintf(b, "%s\n", q);
			q += strlen(q) + 1;
		}
	}

	/* followed by global nvram vars second, if any */
	if ((nvram_vars = MALLOC(wlc->osh, MAXSZ_NVRAM_VARS)) == NULL) {
		err = BCME_NOMEM;
		goto exit;
	}
	if ((err = nvram_getall(nvram_vars, MAXSZ_NVRAM_VARS)) != BCME_OK)
		goto exit;
	if (nvram_vars[0]) {
		q = nvram_vars;
		/* loop to copy vars which contain null separated strings */
		while (((q - nvram_vars) < MAXSZ_NVRAM_VARS) && *q != '\0') {
			bcm_bprintf(b, "%s\n", q);
			q += strlen(q) + 1;
		}
	}

	/* check empty nvram */
	if (q == NULL)
		err = BCME_NOTFOUND;
exit:
	if (nvram_vars)
		MFREE(wlc->osh, nvram_vars, MAXSZ_NVRAM_VARS);

	return err;
}
#endif // endif

#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
static void
wlc_sim_pm_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t *)arg;
	wlc_test_cmn_info_t *cmn = wlc->testi->cmn;
	uint32 timeout = 0;

	wl_del_timer(wlc->wl, cmn->sim_pm_timer);
	switch (cmn->sim_pm_state) {
	case SIM_PM_STATE_AWAKE:
		cmn->sim_pm_state = SIM_PM_STATE_ASLEEP;
		if (mboolisset(wlc->hw->wake_override, WLC_WAKE_OVERRIDE_TEST_APPS))
			wlc_ucode_wake_override_clear(wlc->hw, WLC_WAKE_OVERRIDE_TEST_APPS);
		timeout = (cmn->sim_pm_cycle - cmn->sim_pm_up) / 1000;
		break;
	case SIM_PM_STATE_ASLEEP:
		cmn->sim_pm_state = SIM_PM_STATE_AWAKE;
		timeout = cmn->sim_pm_up / 1000;
		/* Fallthrough */
	case SIM_PM_STATE_DISABLE:
		if (!mboolisset(wlc->hw->wake_override, WLC_WAKE_OVERRIDE_TEST_APPS))
			wlc_ucode_wake_override_set(wlc->hw, WLC_WAKE_OVERRIDE_TEST_APPS);
		break;
	}
	if (cmn->sim_pm_state != SIM_PM_STATE_DISABLE)
		wl_add_timer(wlc->wl, cmn->sim_pm_timer, timeout, FALSE);
	return;
}
#endif // endif

static int
wlc_test_down(void *ctx)
{
#if (defined(WLTEST) && !defined(WLTEST_DISABLED)) || defined(WLPKTENG)
	wlc_test_info_t *test = ctx;
	int i;

	if (test->pkteng_info != NULL) {
		/* del sta (SCB) */
		for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
			(void) wlc_test_pkteng_delsta(test, i);
		}
		/* Reset num of stas that effectively resets sta list */
		test->pkteng_info->nusrs = 0;
		/* Reset flags */
		test->pkteng_info->flags = WLC_TEST_PKTENGCMD_FLAG_RESET;
		/* Reset mode */
		test->pkteng_info->mode = 0;
	}
#endif // endif
	return BCME_OK;
}

/* attach/detach */
wlc_test_info_t *
BCMATTACHFN(wlc_test_attach)(wlc_info_t *wlc)
{
	wlc_test_info_t *test;

	/* allocate module info */
	if ((test = MALLOCZ(wlc->osh, sizeof(*test))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	test->wlc = wlc;

		/* Check from obj registry if common info is allocated */
	test->cmn = (wlc_test_cmn_info_t *) obj_registry_get(wlc->objr, OBJR_TEST_CMN_INFO);

	if (test->cmn == NULL) {
		/* Object not found ! so alloc new object here and set the object */
		if (!(test->cmn = (wlc_test_cmn_info_t *)MALLOCZ(wlc->osh,
			sizeof(wlc_test_cmn_info_t)))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}

#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
		if (!(test->cmn->sim_pm_timer = wl_init_timer(wlc->wl, wlc_sim_pm_timer, wlc,
			"simpmtimer"))) {
			WL_ERROR(("wl%d: wl_init_timer for sim_pm_timer failed\n", wlc->pub->unit));
			goto fail;
		}
#endif // endif

		/* Update registry after all allocations */
		obj_registry_set(wlc->objr, OBJR_TEST_CMN_INFO, test->cmn);
	}

	(void) obj_registry_ref(wlc->objr, OBJR_TEST_CMN_INFO);

	/* register ioctl/iovar dispatchers and other module entries */
	if (wlc_module_add_ioctl_fn(wlc->pub, test, wlc_test_doioctl, 0, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, test_iovars, "wltest", test, wlc_test_doiovar,
			NULL, NULL, wlc_test_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#if defined(WLTEST)
	wlc_dump_register(wlc->pub, "nvram", (dump_fn_t)wlc_nvram_dump, (void *)wlc);
#endif // endif

	/* set maximum packet length */
	wlc->pkteng_maxlen = PKTBUFSZ - wlc->txhroff;

#if defined(WLTEST) || defined(WLPKTENG)
	if (D11REV_GE(wlc->hw->corerev, 128)) {
		if ((test->pkteng_info =
			MALLOCZ(wlc->osh, sizeof(wlc_test_pkteng_info_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			goto fail;
		}
		memset(test->pkteng_info, 0, sizeof(wlc_test_pkteng_info_t));
		wlc_test_pkteng_cmd_init(test);
	}
#endif // endif

	return test;

fail:
	MODULE_DETACH(test, wlc_test_detach);
	return NULL;
}

void
BCMATTACHFN(wlc_test_detach)(wlc_test_info_t *test)
{
	wlc_info_t *wlc;

	if (test == NULL)
		return;

	wlc = test->wlc;

	(void)wlc_module_remove_ioctl_fn(wlc->pub, test);
	(void)wlc_module_unregister(wlc->pub, "wltest", test);

#if defined(WLTEST) || defined(WLPKTENG)
	if (test->pkteng_info != NULL) {
		MFREE(test->wlc->osh, test->pkteng_info, sizeof(wlc_test_pkteng_info_t));
		test->pkteng_info = NULL;
	}
#endif // endif

	if (obj_registry_unref(test->wlc->objr, OBJR_TEST_CMN_INFO) == 0) {
		obj_registry_set(test->wlc->objr, OBJR_TEST_CMN_INFO, NULL);
		if (test->cmn != NULL) {
#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
			if (test->cmn->sim_pm_timer) {
				wl_free_timer(wlc->wl, test->cmn->sim_pm_timer);
				test->cmn->sim_pm_timer = NULL;
			}
#endif // endif
			MFREE(test->wlc->osh, test->cmn,
				sizeof(wlc_test_cmn_info_t));
		}
	}
	MFREE(wlc->osh, test, sizeof(*test));
}

wlc_test_info_t *
wlc_get_testi(wlc_info_t *wlc)
{
	return wlc->testi;
}

/* ioctl dispatcher */
static int
wlc_test_doioctl(void *ctx, uint cmd, void *arg, uint len, struct wlc_if *wlcif)
{
	wlc_test_info_t *test = ctx;
	wlc_info_t *wlc = test->wlc;
	int bcmerror = BCME_OK;

	BCM_REFERENCE(wlc);

	switch (cmd) {

#if !defined(DONGLEBUILD) || defined(BCMDBG) || defined(WLTEST)
	case WLC_GET_SROM: {
		srom_rw_t *s = (srom_rw_t *)arg;
		if (s->byteoff == 0x55ab) {	/* Hack for srcrc */
			s->byteoff = 0;
			bcmerror = wlc_iovar_op(wlc, "srcrc", arg, len, arg, len, IOV_GET, wlcif);
		} else
			bcmerror = wlc_iovar_op(wlc, "srom", arg, len, arg, len, IOV_GET, wlcif);
		break;
	}
#endif // endif

#if defined(BCMDBG) || defined(WLTEST)
	case WLC_SET_SROM: {
		bcmerror = wlc_iovar_op(wlc, "srom", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	}
#endif // endif

#if	defined(WLTEST)
	case WLC_NVRAM_GET:
		bcmerror = wlc_iovar_op(wlc, "nvram_get", arg, len, arg, len, IOV_GET, wlcif);
		break;

#endif // endif

#ifdef BCMNVRAMW
	case WLC_OTPW:
	case WLC_NVOTPW: {
		if (len & 1) {
			bcmerror = BCME_BADARG;
			break;
		}
		bcmerror = wlc_iovar_op(wlc, (cmd == WLC_OTPW) ? "otpw" : "nvotpw",
			NULL, 0, arg, len, IOV_SET, wlcif);

		break;
	}
#endif /* BCMNVRAMW */

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}

	return bcmerror;
} /* wlc_test_doioctl */

#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
/** Get chip manufacture information */
static int
wlc_manf_info_get(wlc_info_t *wlc, char *buf, int len, struct wlc_if *wlcif)
{
	uint16 otp_data[MANF_INFO_LEN]; /* 2 rows in OTP, 64 bits each */
	struct bcmstrbuf b;
	int bcmerror = BCME_OK;
	const struct wlc_otp_manf_info *pinfo;
	uint32 data, i;
	uint16 bit_pos, row_index = 0, bit_val;

	/* read 1st 2 row from OTP */
	for (i = 0; i < 8; i++) {
		bcmerror = wlc_iovar_op(wlc, "otprawr", &i, sizeof(uint32), &data,
			sizeof(uint32), IOV_GET, wlcif);
		if (bcmerror) {
			WL_ERROR(("fail to read OTP\n"));
			return BCME_ERROR;
		}
		otp_data[i] = (uint16)data;
		WL_TRACE(("\nOTP data%x: %x", i, otp_data[i]));
	}

	bcm_binit(&b, buf, len);
	pinfo = wlc_manf_info;
	while (pinfo->name != NULL) {
		data = 0;
		ASSERT(pinfo->bit_pos_end == pinfo->bit_pos_start + pinfo->len - 1);
		for (bit_pos = row_index * MANF_INFO_ROW_WIDTH + pinfo->bit_pos_start, i = 0;
		     i < pinfo->len; i++, bit_pos++) {
			if ((bit_pos >> 4) > MANF_INFO_LEN) {
				return BCME_ERROR;
			}
			/* extract the bit from the half word array */
			bit_val = (otp_data[bit_pos >> 4] >> (bit_pos & 0xf)) & 0x1;
			data |= (bit_val << i);
		}

		WL_TRACE(("%s : 0x%x\n", pinfo->name, data));
		if (pinfo->bit_pos_end == (MANF_INFO_ROW_WIDTH - 1)) {
			row_index++;
		}
		bcm_bprintf(&b, "%s: 0x%x\n", pinfo->name, data);
		pinfo++;
	}
	bcm_bprintf(&b, "Package ID:%x\n", wlc->pub->sih->chippkg);
	return bcmerror;
} /* wlc_manf_info_get */
#endif // endif

/* iovar dispatcher */
static int
wlc_test_doiovar(void *ctx, uint32 actionid,
	void *params, uint plen, void *arg, uint alen, uint vsize, struct wlc_if *wlcif)
{
	wlc_test_info_t *test = ctx;
	wlc_info_t *wlc = test->wlc;
	int32 *ret_int_ptr;
	int32 int_val = 0;
	int err = BCME_OK;
	uint corerev = wlc->hw->corerev;

	BCM_REFERENCE(wlc);
	BCM_REFERENCE(ret_int_ptr);
	BCM_REFERENCE(corerev);

	if (plen >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* all iovars require mcnx being enabled */
	switch (actionid) {

#if (defined(WLTEST) && !defined(WLTEST_DISABLED)) || defined(WLPKTENG)
	case IOV_SVAL(IOV_PKTENG): {
		wl_pkteng_t pkteng;
		void *pktlist[AMPDU_DEF_MAX_RELEASE_AQM] = {NULL};
		uint32 pkteng_flags;
		ratespec_t rate_override;
		int mpdulen;
		uint16 npkt = 0;
		int totlen, datalen;
		wlc_bsscfg_t *bsscfg_perif;
		void *userdata = NULL;
		int userdata_len = 0;
		wlc_test_pkteng_info_t *pkteng_info;

		bcopy(params, &pkteng, sizeof(wl_pkteng_t));
		pkteng_flags = pkteng.flags & WL_PKTENG_PER_MASK;

		pkteng_info = test->pkteng_info;

		if (plen > sizeof(wl_pkteng_t)) {
			/* check if caller-provide userdata to be filled within the pkt */
			userdata = (void *) ((uint8 *) params + sizeof(wl_pkteng_t));
			userdata_len = plen - sizeof(wl_pkteng_t);
		}

		/* update bsscfg w/provided interface context */
		bsscfg_perif = wlc_bsscfg_find_by_wlcif(wlc, wlcif);

		if ((pkteng_info == NULL) ||
			((pkteng_info->flags & WLC_TEST_PKTENGCMD_FLAG_EN) == 0)) {
			/* Need ~9.6KB pkt for transmitting enough symbols needed for composite
			 * EVM test at higher vht mcs rates. Picked 16K as a limit for now.
			 * XXX: change the limit based on rate and time.
			 */
			if (pkteng.length + wlc->txhroff > WL_PKTENG_MAXPKTSZ) {
				return BCME_BADLEN;
			}
		} else { /* Using the settings from pkteng_info */
			npkt = pkteng_info->usr[0].fifo[0].ampdu_mpdu;
			WL_ERROR(("%s: Pkteng: npkt=%d pktlist_size=%d ampdu_mpdu=%d\n",
				__FUNCTION__, npkt, (int) ARRAYSIZE(pktlist),
				pkteng_info->usr[0].fifo[0].ampdu_mpdu));
		}

		/* Prepare the packet for Tx */
		if ((pkteng_flags == WL_PKTENG_PER_TX_START) ||
			(pkteng_flags == WL_PKTENG_PER_TX_WITH_ACK_START) ||
			(pkteng_flags == WL_PKTENG_PER_TX_HETB_START) ||
			(pkteng_flags == WL_PKTENG_PER_TX_HETB_WITH_TRG_START)) {
			struct ether_addr *sa;
			int i = 0;
			bool doampdu = AMPDU_ENAB(wlc->pub);
			if (wlc->hw->pkteng_status != BCME_OK) {
				/* pkteng TX running already */
				WL_ERROR(("Pkteng busy...need to stop first\n"));
				return BCME_BUSY;
			}

			if (ETHER_ISMULTI(&pkteng.dest)) {
				rate_override = wlc->band->mrspec_override;
			} else {
				rate_override = wlc->band->rspec_override;
			}

			if (!RSPEC_ACTIVE(rate_override)) {
				rate_override = wlc->band->hwrs_scb->rateset.rates[0] & RATE_MASK;
			}

			/* Don't allow pkteng to start if associated */
			if (wlc->pub->associated)
				return BCME_ASSOCIATED;
			if (!wlc->band->rspec_override)
				return BCME_ERROR;
#ifdef WL_BEAMFORMING
			// Disable TxBF during PktEng TX as it causes imbalanced power in CDD signal
			if (!(D11REV_IS(corerev, 61)) && D11REV_LT(corerev, 128)) {
				if (TXBF_ENAB(wlc->pub)) {
					wlc_txbf_pkteng_tx_start(wlc->txbf, wlc->band->hwrs_scb);
				}
			}
#endif /*  WL_BEAMFORMING */

			sa = (ETHER_ISNULLADDR(&pkteng.src)) ?
				&wlc->pub->cur_etheraddr : &pkteng.src;
			/* pkt will be freed in wlc_bmac_pkteng() */

			totlen = pkteng.length;

			if (RSPEC_ISLEGACY(wlc->band->rspec_override) || !doampdu) {
				wlc->pkteng_maxlen = WL_PKTENG_MAXPKTSZ;
				wlc_write_shm(wlc, (uint16)0x20, (uint16)20000);
				wlc->RTSThresh = (uint16) 20000;
				mpdulen = totlen;
			} else if (totlen < PKTENG_MAXLEN_64K) {
				mpdulen = PKTENG_MPDU_LEN_2K;
			} else if (totlen < PKTENG_MAXLEN_160K) {
				/* Use bigger mpdu to send more data */
				mpdulen = PKTENG_MPDU_LEN_4K;
			} else if (totlen < PKTENG_MAXLEN_320K) {
				mpdulen = PKTENG_MPDU_LEN_8K;
			} else {
				if (totlen > PKTENG_MAXLEN_320K) {
					WL_ERROR(("Warning: packek length trimmed to 320K "
						"as BM may not have space for %d bytes\n",
						pkteng.length));
					totlen = PKTENG_MAXLEN_320K;
				}
				mpdulen = PKTENG_MPDU_LEN_11K;
			}

			if ((pkteng_info == NULL) ||
				((pkteng_info->flags & WLC_TEST_PKTENGCMD_FLAG_EN) == 0)) {
				/* Do not use the settings from pkteng_info */

				/* Chk if requested len and txtime based on rate>5.484ms allowed */
				if (doampdu && !wlc_tx_testframe_len_dur_isvalid(
					wlc, rate_override, pkteng.length)) {
					WL_ERROR(("pktlength not valid %d, "
					"since txtime based on rate > 5.484ms \n", pkteng.length));
					return BCME_BADLEN;
				}
				i = 0; npkt = 0;
				do {
					datalen = (totlen > mpdulen) ? mpdulen : totlen;
					/* create but don't (yet) enqueue test frame */
					pktlist[i] = wlc_tx_testframe(wlc, &pkteng.dest, sa,
						0, datalen, i ?  pktlist[0] : NULL, doampdu,
						bsscfg_perif, userdata, userdata_len);

					if (pktlist[i] == NULL) {
						if (npkt == 0) {
							return BCME_NOMEM;
						} else {
							/* No mem send what we allocate so far */
							WL_ERROR(("Warn: no mem for remaining:%d"
								"..just send npkt:%d\n",
								totlen, npkt));
							pkteng.length = mpdulen * npkt;
							break;
						}
					}
					npkt++;
					i++;
					totlen -= datalen;
				} while ((totlen > 0) && i < ARRAYSIZE(pktlist));

				if ((totlen > 0) && (i == ARRAYSIZE(pktlist))) {
					WL_ERROR(("Warn: Req len:%d > max 64 mpdu/ampdu, rem:%d\n",
						pkteng.length, totlen));
				}
			} else { /* Using ampdu_mpdu from pkteng_info */
				mpdulen = pkteng_info->usr[0].fifo[0].frame_len;
				for (i = 0; i < npkt; i++) {
					pktlist[i] = wlc_tx_testframe(wlc, &pkteng.dest, sa,
						0, mpdulen, i ?  pktlist[0] : NULL, doampdu,
						bsscfg_perif, userdata, userdata_len);

					if (pktlist[i] == NULL) {
						if (i == 0) {
							return BCME_NOMEM;
						} else {
							/* No mem left just warning */
							WL_ERROR(("Warn: no mem for remaining:%d"
								"..just send i:%d\n",
								npkt-i, i));
							npkt = i;
							break;
						}
					}
				}
				pkteng.length = npkt * mpdulen;

				WL_ERROR(("%s: Pkteng npkt=%d mpdulen=%d tot_len=%d doampdu=%d\n",
					__FUNCTION__, npkt, mpdulen, pkteng.length, doampdu));
			}

			/* Unmute the channel for pkteng if quiet */
			if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
				wlc_mute(wlc, OFF, 0); /* allows transmit */
			err = wlc_bmac_pkteng(wlc->hw, &pkteng, pktlist, npkt); // sends test frames
		} else {
			err = wlc_bmac_pkteng(wlc->hw, &pkteng, NULL, 0);
		}

		if ((err != BCME_OK) || (pkteng_flags == WL_PKTENG_PER_TX_STOP)) {
			/* restore Mute State after pkteng is done */
			if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
				wlc_mute(wlc, ON, 0);
#ifdef WL_BEAMFORMING
			if (TXBF_ENAB(wlc->pub) && D11REV_LT(corerev, 128)) {
				wlc_txbf_pkteng_tx_stop(wlc->txbf, wlc->band->hwrs_scb);
			}
#endif /* WL_BEAMFORMING */
		}
		break;
	}
	case IOV_SVAL(IOV_LOAD_TRIG_INFO): {
		if (D11REV_GE(corerev, 128)) {
			wl_pkteng_ru_fill_t pkteng_ru_fill;
			bcopy(params, &pkteng_ru_fill, sizeof(wl_pkteng_ru_fill_t));
			err = wlc_test_pkteng_trig_fillinfo(wlc->hw, &pkteng_ru_fill);
			if (err != BCME_OK)
				WL_ERROR(("problem with some params while filling trig info\n"));
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_GVAL(IOV_GET_TRIG_INFO): {
		if (D11REV_GE(corerev, 128)) {
			wl_trig_frame_info_t trig_frame_info;
			err = wlc_test_get_trig_info(wlc->hw, &trig_frame_info);
			bcopy(&trig_frame_info, (char *)arg, sizeof(wl_trig_frame_info_t));
			if (err != BCME_OK)
				WL_ERROR(("error while getting the trigger frame contents\n"));
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
	}
	case IOV_GVAL(IOV_PKTENG_MAXLEN): {
		*ret_int_ptr = wlc->pkteng_maxlen;
		break;
	}
	case IOV_GVAL(IOV_PKTENG_STATUS): {
		int32 pkteng_status_val = wlc->hw->pkteng_status;
		if (pkteng_status_val != PKTENG_IDLE) {
			*ret_int_ptr = BCME_BUSY;
		} else {
			*ret_int_ptr = BCME_OK;
		}
		break;
	}

	case IOV_SVAL(IOV_PKTENG_CMD):
		if (D11REV_GE(corerev, 128)) {
			wl_pkteng_cmd_params_t pkteng_cmd_params;

			if (sizeof(wl_pkteng_cmd_params_t) > plen) {
				err = BCME_BUFTOOSHORT;
			}

			bcopy(params, &pkteng_cmd_params, sizeof(wl_pkteng_cmd_params_t));

			if ((err = wlc_test_pkteng_cmd_set(test, &pkteng_cmd_params)) != BCME_OK) {
				WL_ERROR(("problem with some parameters while "
					"configuring pkteng\n"));
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_PKTENG_CMD):
		if (D11REV_GE(corerev, 128)) {
			wl_pkteng_cmd_params_t pkteng_cmd_params;

			if (sizeof(wl_pkteng_cmd_params_t) > plen) {
				err = BCME_BUFTOOSHORT;
			}

			bcopy(params, &pkteng_cmd_params, sizeof(wl_pkteng_cmd_params_t));

			switch (pkteng_cmd_params.opcode) {
			case PKTENGCMD_OPCODE_PUB_ARGKEY_GET:
				err = wlc_test_pkteng_cmd_getusr(test,
					&pkteng_cmd_params, arg, alen);
				break;
			case PKTENGCMD_OPCODE_PUB_ARGKEY_CFG:
				err = wlc_test_pkteng_cmd_getkey(test,
					&pkteng_cmd_params, arg, alen);
				break;
			case PKTENGCMD_OPCODE_PRIV_GET_TEST:
				break;
			}
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;
#ifdef TESTBED_AP_11AX
	case IOV_GVAL(IOV_PKTENG_TRIG_TMR):
		*ret_int_ptr = test->pkteng_trig_tmr;
		break;
	case IOV_SVAL(IOV_PKTENG_TRIG_TMR):
		update_pkt_eng_trg_tmr(wlc, int_val);
		break;
	case IOV_GVAL(IOV_PKTENG_PKT_TMR):
		*ret_int_ptr = test->pkteng_pkt_tmr;
		break;
	case IOV_SVAL(IOV_PKTENG_PKT_TMR):
		update_pkt_eng_pkt_tmr(wlc, int_val);
		break;
#endif /* TESTBED_AP_11AX */

#endif // endif

#if defined(WLTEST)
	case IOV_GVAL(IOV_NVRAM_GET): {
		const char *nv_str;

		nv_str = getvar(wlc->pub->vars, params);

		if (nv_str != NULL) {
			size_t nv_len = strlen(nv_str);
			if (nv_len >= MAXSZ_NVRAM_VARS) {
				err = BCME_ERROR;
				break;
			}
			if ((size_t)alen < nv_len + 1) {
				err = BCME_BUFTOOSHORT;
			} else {
				char *dst = (char *)arg;
				strncpy(dst, nv_str, nv_len);
				dst[nv_len] = '\0';
			}
		} else {
			err = BCME_NOTFOUND;
		}
		break;
	}

	case IOV_GVAL(IOV_NVRAM_DUMP): {
		struct bcmstrbuf b;

		bcm_binit(&b, (char*)arg, alen);
		err = wlc_nvram_dump(wlc, &b);
		break;
	}
#endif // endif

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && defined(WLTEST)
#ifdef BCMNVRAMW
	case IOV_SVAL(IOV_CISWRITE):
	{
		cis_rw_t cis;
		uint16 *tbuf;

		ASSERT(alen >= (int)sizeof(cis));
		bcopy(arg, &cis, sizeof(cis));
		alen -= sizeof(cis);
		arg = (uint8 *)arg + sizeof(cis);

		/* Basic validity checks.  For now retain 2-byte mode, insist on byteoff 0 */
		if (cis.source || (cis.byteoff & 1) || (cis.nbytes & 1) ||
			cis.byteoff) {
			err = BCME_BADARG;
			break;
		}

		/* Use a temp buffer to ensure alignment */
		if ((tbuf = (uint16*)MALLOC(wlc->osh, cis.nbytes)) == NULL) {
			err = BCME_NOMEM;
			break;
		}
		bcopy(arg, tbuf, cis.nbytes);
		ltoh16_buf(tbuf, cis.nbytes);

		err = wlc_bmac_ciswrite(wlc->hw, &cis, tbuf, alen);

		MFREE(wlc->osh, tbuf, cis.nbytes);
		break;
	}
#endif /* BCMNVRAMW */
#endif // endif

#if defined(BCM_CISDUMP_NO_RECLAIM) || (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && \
	(defined(WLTEST) && !defined(WLTEST_DISABLED))
	case IOV_GVAL(IOV_CISDUMP):
	{
		cis_rw_t cis;
		uint16 *tbuf = NULL;

		/* Extract parameters (to allow length spec) */
		if ((plen < sizeof(cis)) || (alen < (int)sizeof(cis))) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(params, &cis, sizeof(cis));

		/* Some basic consistency checks */
		if (cis.source || cis.byteoff || (cis.nbytes & 1) || (cis.nbytes > SROM_MAX)) {
			err = BCME_BADARG;
			break;
		}

		/* Use a temp buffer to ensure alignment */
		if ((tbuf = (uint16*)MALLOC(wlc->osh, SROM_MAX)) == NULL) {
			err = BCME_NOMEM;
			break;
		}
		memset(tbuf, 0, SROM_MAX);

		/* Adjust length to available bytes */
		alen -= sizeof(cis);

		/* Read the CIS */
		err = wlc_bmac_cisdump(wlc->hw, &cis, tbuf, alen);

		/* Prepare return buffer */
		if (!err) {
			bcopy(&cis, arg, sizeof(cis));
			htol16_buf(tbuf, cis.nbytes);
			bcopy(tbuf, (char*)arg + sizeof(cis), cis.nbytes);
		}

		MFREE(wlc->osh, tbuf, SROM_MAX);
		break;
	}
#endif // endif

#if (defined(WLTEST) && !defined(WLTEST_DISABLED))
	case IOV_GVAL(IOV_MANF_INFO):
		err = wlc_manf_info_get(wlc, (char *)arg, alen, wlcif);
		break;

	case IOV_GVAL(IOV_SIM_PM):
		{
			sim_pm_params_t *sim_pm = (sim_pm_params_t *)arg;
			sim_pm->enabled = (test->cmn->sim_pm_state != SIM_PM_STATE_DISABLE) ? 1 : 0;
			sim_pm->cycle = (uint16)uS2TU(test->cmn->sim_pm_cycle);
			sim_pm->up = (uint16)uS2TU(test->cmn->sim_pm_up);
			break;
		}

	case IOV_SVAL(IOV_SIM_PM):
		{
			uint32 enable = int_val;
			sim_pm_params_t *sim_pm = (sim_pm_params_t *)params;
			int def = (plen != sizeof(sim_pm_params_t));

			if (plen != (int)sizeof(int_val) && def) {
				err = BCME_BADARG;
				break;
			}

			if (enable) {
				if (test->cmn->sim_pm_state == SIM_PM_STATE_DISABLE) {
					wlc_get(wlc, WLC_GET_PM, &test->cmn->sim_pm_saved_PM);
					wlc_set(wlc, WLC_SET_PM, PM_FAST);
					wlc_mhf(wlc, MHF1, MHF1_ULP, MHF1_ULP, WLC_BAND_AUTO);
				}
				wlc_iovar_setint(wlc, "wd_disable", 1);
				if (mboolisset(wlc->hw->wake_override, WLC_WAKE_OVERRIDE_TEST_APPS))
					test->cmn->sim_pm_state = SIM_PM_STATE_AWAKE;
				else
					test->cmn->sim_pm_state = SIM_PM_STATE_ASLEEP;
				test->cmn->sim_pm_cycle = TU2uS(def ? SIM_PM_DEF_CYCLE :
					sim_pm->cycle);
				test->cmn->sim_pm_up = TU2uS(def ? SIM_PM_DEF_UP : sim_pm->up);
			}
			else {
				if (test->cmn->sim_pm_state == SIM_PM_STATE_DISABLE)
					break;
				wlc_iovar_setint(wlc, "wd_disable", 0);
				test->cmn->sim_pm_state = SIM_PM_STATE_DISABLE;
				wlc_mhf(wlc, MHF1, MHF1_ULP, 0, WLC_BAND_AUTO);
				wlc_set(wlc, WLC_SET_PM, test->cmn->sim_pm_saved_PM);
			}
			wl_del_timer(wlc->wl, test->cmn->sim_pm_timer);
			wl_add_timer(wlc->wl, test->cmn->sim_pm_timer, test->cmn->sim_pm_up / 1000,
				FALSE);
		}
		break;
#endif // endif

#if defined(WLTEST) || defined(WLPKTENG)
	case IOV_GVAL(IOV_LONGPKT):
		*ret_int_ptr = ((wlc->pkteng_maxlen == WL_PKTENG_MAXPKTSZ) ? 1 : 0);
		break;

	case IOV_SVAL(IOV_LONGPKT):
		{
			uint16 rtsthresh = wlc->RTSThresh;
			if ((int_val == 0) && (wlc->pkteng_maxlen == WL_PKTENG_MAXPKTSZ)) {
				/* restore 'wl rtsthresh' */
				rtsthresh = wlc->longpkt_rtsthresh;

				/* restore 'wl shmem 0x20' */
				wlc_write_shm(wlc, (uint16)0x20, wlc->longpkt_shm);

				/* restore pkteng_maxlen */
				wlc->pkteng_maxlen = PKTBUFSZ - wlc->txhroff;
			} else if ((int_val == 1) &&
			           (wlc->pkteng_maxlen == (PKTBUFSZ - wlc->txhroff))) {
				/* save current values */
				wlc->longpkt_rtsthresh = rtsthresh;
				wlc->longpkt_shm = wlc_read_shm(wlc, (uint16)0x20);

				/* 'wl rtsthresh 20000' */
				rtsthresh = (uint16) 20000;

				/* 'wl shmem 0x20 20000' */
				wlc_write_shm(wlc, (uint16)0x20, (uint16)20000);

				/* increase pkteng_maxlen */
				wlc->pkteng_maxlen = WL_PKTENG_MAXPKTSZ;
			}
		}
		break;
#endif // endif

	case IOV_SVAL(IOV_CCGPIOCTRL):
		si_gpiocontrol(wlc->pub->sih, ~0, int_val, GPIO_HI_PRIORITY);
		break;
	case IOV_GVAL(IOV_CCGPIOCTRL):
		*ret_int_ptr = si_gpiocontrol(wlc->pub->sih, 0, 0, GPIO_HI_PRIORITY);
		break;
	case IOV_GVAL(IOV_CCGPIOIN):
		*ret_int_ptr = si_gpioin(wlc->pub->sih);
		break;
	case IOV_SVAL(IOV_CCGPIOOUT):
		si_gpioout(wlc->pub->sih, ~0, int_val, GPIO_HI_PRIORITY);
		break;
	case IOV_GVAL(IOV_CCGPIOOUT):
		*ret_int_ptr = si_gpioout(wlc->pub->sih, 0, 0, GPIO_HI_PRIORITY);
		break;
	case IOV_SVAL(IOV_CCGPIOOUTEN):
		si_gpioouten(wlc->pub->sih, ~0, int_val, GPIO_HI_PRIORITY);
		break;
	case IOV_GVAL(IOV_CCGPIOOUTEN):
		*ret_int_ptr = si_gpioouten(wlc->pub->sih, 0, 0, GPIO_HI_PRIORITY);
		break;
	case IOV_SVAL(IOV_CCGPIOFNSEL_GPIO):
		test->cmn->ccgpiofnsel_gpio = (int16)int_val;
		break;
	case IOV_GVAL(IOV_CCGPIOFNSEL_GPIO):
		*ret_int_ptr = test->cmn->ccgpiofnsel_gpio;
		break;
	case IOV_SVAL(IOV_CCGPIOFNSEL_FUNC):
		si_gci_set_functionsel(wlc->pub->sih,
			test->cmn->ccgpiofnsel_gpio, int_val & 0xffff);
		break;
	case IOV_GVAL(IOV_CCGPIOFNSEL_FUNC):
		*ret_int_ptr = (test->cmn->ccgpiofnsel_gpio << 16) |
			si_gci_get_functionsel(wlc->pub->sih,
				test->cmn->ccgpiofnsel_gpio);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#if (defined(WLTEST) && !defined(WLTEST_DISABLED)) || defined(WLPKTENG)
static bool
wlc_tx_testframe_len_dur_isvalid(wlc_info_t *wlc, ratespec_t rate_override, uint len)
{
	wlcband_t *band = wlc_scbband(wlc, wlc->band->hwrs_scb);
	int bandtype = band->bandtype;
	bool is2g = BAND_2G(bandtype);
	uint txtime;
	bool shortpreamble;

	if (wlc->cfg->PLCPHdr_override == WLC_PLCP_LONG)
		shortpreamble = FALSE;
	else
		shortpreamble = TRUE;

	txtime = wlc_txtime(rate_override, is2g, shortpreamble, len);

	if (txtime > AMPDU_MAX_DUR) {
		WL_ERROR(("Error: Length too big for requested rate shortpreamble:%d"
			" rate_override:0x%x is2g:%d len:%u txtime=%dus > %dus\n",
			shortpreamble, rate_override, is2g, len, txtime, AMPDU_MAX_DUR));
		return FALSE;
	}

	WL_ERROR(("frame txtime:%dus(not valid for mcs10&11) len:%u rate:%dMbps\n",
		txtime, len, RSPEC2KBPS(rate_override)/1000));
	return TRUE;
}

static void*
wlc_tx_testframe_get(wlc_info_t *wlc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len, void *src_pkt,
	void *userdata, int userdata_len)
{
	int i = 0, start = 0, len, buflen = 0, filllen = 0;
	void *p = NULL, *prevp = NULL, *headp = NULL;
	osl_t *osh;
	struct dot11_management_header *hdr;
	uint8 *pbody = NULL;
	bool first = TRUE;
	uint16 fc = FC_DATA;
	bool clone = FALSE;
	int tmp_len;

	osh = wlc->osh;
	/* Note, if specified, 'userdata' is used for filling the frame-body,
	 * and the 'userdata_len' should have been included in the 'body_len',
	 * do not add the 'userdata_len' to 'len' again.
	 */
	len = DOT11_MGMT_HDR_LEN + wlc->txhroff + body_len;
#ifdef WL_MUPKTENG
	/* mupkteng: AMPDU will use QOS (Best Effort) */
	fc = wlc_mutx_pkteng_on(wlc->mutx)? FC_QOS_DATA : FC_DATA;
#endif // endif

#ifdef DONGLEBUILD
	if (!src_pkt) {
		clone = FALSE;
	} else {
		clone = TRUE;
	}
#endif /* DONGLEBUILD */

	while (len > 0) {
		buflen = (len > PKTENG_PKTBUFSZ) ? PKTENG_PKTBUFSZ : len;

		if (first) {
			buflen = DOT11_MGMT_HDR_LEN + wlc->txhroff;
		}

#ifdef DONGLEBUILD
		if (!first && clone) {
			int offset = 0;
			src_pkt = PKTNEXT(osh, src_pkt);
			p = hnd_pkt_clone(osh, src_pkt, offset, PKTLEN(osh, src_pkt));
		} else
#endif /* DONGLEBUILD */
		{
			BCM_REFERENCE(clone);
			BCM_REFERENCE(src_pkt);
			p = PKTGET(osh, buflen, TRUE);
		}

		if (!p) {
			WL_ERROR(("wl%d: wlc_frame_get_mgmt_test: pktget error for len %d \n",
				wlc->pub->unit, buflen));
			WLCNTINCR(wlc->pub->_cnt->txnobuf);
			PKTFREE(osh, headp, TRUE);
			return (NULL);
		}
		ASSERT(ISALIGNED((uintptr)PKTDATA(osh, p), sizeof(uint32)));

		if (first) {
			/* reserve tx headroom offset */
			PKTPULL(osh, p, wlc->txhroff);
			PKTSETLEN(osh, p, buflen - wlc->txhroff);

			/* Set MAX Prio for MGMT packets */
			PKTSETPRIO(p, MAXPRIO);

			/* Remember the head pointer */
			headp = p;

			/* construct a management frame */
			hdr = (struct dot11_management_header *)PKTDATA(osh, p);

			wlc_fill_mgmt_hdr(hdr, fc, da, sa, bssid);
			/* Find start of data and length */
			pbody = (uint8 *)&hdr[1];
			filllen = buflen - wlc->txhroff - sizeof(struct dot11_management_header);

			first = FALSE;
		} else {
			pbody = (uint8 *)PKTDATA(osh, p);
			filllen = buflen;
		}

		if (!clone && pbody) {
			if (userdata != NULL && userdata_len > 0) {
				/* fill in user-data if available */
				tmp_len = userdata_len <= filllen ? userdata_len : filllen;
				if (tmp_len > 0) {
					memcpy(pbody, userdata, tmp_len);
					start += tmp_len;
					filllen -= tmp_len;
					pbody += tmp_len;
				}
			}

			for (i = start; i < start + filllen; i++) {
				pbody[i - start] = (uint8) i;
			}
			start = i;
		}

		/* chain the pkt buffer */
		if (prevp)
			PKTSETNEXT(osh, prevp, p);

		prevp = p;
		len -= buflen;
	};

	return (headp);
} /* wlc_tx_testframe_get */

/** Create a test frame, don't enqueue it into tx fifo just yet */
void *
wlc_tx_testframe(wlc_info_t *wlc, struct ether_addr *da, struct ether_addr *sa,
	ratespec_t rate_override, int length, void *src_pkt, bool doampdu,
	wlc_bsscfg_t *bsscfg_perif,
	void *userdata, int userdata_len)
{
	void *p;
	bool shortpreamble;
	uint16 txh_off = 0;

	if ((p = wlc_tx_testframe_get(wlc, da, sa, sa, length, src_pkt,
		userdata, userdata_len)) == NULL)
		return NULL;

	/*
	 * XXX: hwrs scb doesn't include rate selection cubby with it,
	 *  so there should be a rate override associated with it
	 */
	/* check if the rate overrides are set */
	if (!RSPEC_ACTIVE(rate_override)) {
		if (ETHER_ISMULTI(da))
			rate_override = wlc->band->mrspec_override;
		else
			rate_override = wlc->band->rspec_override;

		if (!RSPEC_ACTIVE(rate_override))
			rate_override = wlc->band->hwrs_scb->rateset.rates[0] & RATE_MASK;
	}

	/* For TEST purpose, we could toggle short or long preamble in per interface
	* bsscfg via IOVAR SET command
	*/
	if (bsscfg_perif != NULL && (bsscfg_perif->PLCPHdr_override == WLC_PLCP_LONG)) {
		shortpreamble = FALSE;
	} else {
		shortpreamble = TRUE;
	}

	/* add headers */
	wlc_d11hdrs(wlc, p, wlc->band->hwrs_scb, shortpreamble, 0, 1,
	            TX_DATA_FIFO, 0, NULL, NULL, rate_override);

	if (doampdu &&
		AMPDU_AQM_ENAB(wlc->pub)) {

#ifdef WLTOEHW
		if (D11REV_GE(wlc->pub->corerev, 40) && (!wlc->toe_bypass)) {
			txh_off = wlc_tso_hdr_length((d11ac_tso_t*)PKTDATA(wlc->osh, p));
		}
#endif /* WLTOEHW */
		/* pull d11ac header */
		PKTPULL(wlc->osh, p, txh_off);

		wlc_ampdu_reqmpdu_to_aqm_aggregatable(wlc, p);

		/* set back the data pointer */
		PKTPUSH(wlc->osh, p, txh_off);
	}
	/* Originally, we would call wlc_txfifo() here */
	/* wlc_txfifo(wlc, TX_DATA_FIFO, p, TRUE, 1); */
	/* However, that job is now the job of wlc_pkteng(). We return the packet so we can pass */
	/* it in as a parameter to wlc_pkteng() */
	return p;
} /* wlc_tx_testframe */

#ifdef WL_MUPKTENG
/* Create a test frame */
void *
wlc_mutx_testframe(wlc_info_t *wlc, struct scb *scb, struct ether_addr *sa,
                 ratespec_t rate_override, int fifo, int length, uint16 seq)
{
	void *p;
	bool shortpreamble = FALSE;

	if ((p = wlc_tx_testframe_get(wlc, &scb->ea, sa, sa, length, NULL,
		NULL, 0)) == NULL) {
		WL_ERROR(("%s pkt is NULL\n", __FUNCTION__));
		return NULL;
	}

	/* Set BE Prio for packets */
	PKTSETPRIO(p, PRIO_8021D_BE);
	WLPKTTAG(p)->flags |= WLF_AMPDU_MPDU;
	WLPKTTAG(p)->flags |= WLF_BYPASS_TXC;
	WLPKTTAG(p)->flags &= ~WLF_EXPTIME;
	WLPKTTAG(p)->seq = seq;
	/* add headers */
	wlc_d11hdrs(wlc, p, scb, shortpreamble, 0, 1, fifo, 0, NULL, NULL, rate_override);

	return p;
}
#endif /*  WL_MUPKTENG */

static int
wlc_test_pkteng_trig_fillinfo(wlc_hw_info_t *wlc_hw, wl_pkteng_ru_fill_t *pkteng_trig_fill)
{
	he_trig_cmninfo_set_t cmninfo;
	he_trig_usrinfo_set_t usrinfo;

	int err;
	 /* Computations involves in forming the elements of trigger frame */
	err = wlc_test_compute_trig_info(pkteng_trig_fill, &cmninfo, &usrinfo);
	if (err == BCME_OK) {
		uint16 idx, *shmptr, shmval;

		/* common info */
		shmptr = (uint16*)&cmninfo;
		for (idx = 0; idx < (HE_TRIG_CMNINFO_SZ >> 1); idx++) {
			wlc_bmac_write_shm(wlc_hw, M_RXTRIG_CMNINFO_LOC(wlc_hw, idx), *shmptr++);
		}

		/* user info */
		shmval = (((uint16)usrinfo[1] << 8) | usrinfo[0]);
		wlc_bmac_write_shm(wlc_hw, M_RXTRIG_USRINFO_LOC(wlc_hw, 0), shmval);
		shmval = (((uint16)usrinfo[3] << 8) | usrinfo[2]);
		wlc_bmac_write_shm(wlc_hw, M_RXTRIG_USRINFO_LOC(wlc_hw, 1), shmval);
		shmval =  usrinfo[4];
		wlc_bmac_write_shm(wlc_hw, M_RXTRIG_USRINFO_LOC(wlc_hw, 2), shmval);

	}
	return err;
}

static int
wlc_test_compute_trig_info(wl_pkteng_ru_fill_t *pkteng_trig_fill, he_trig_cmninfo_set_t *cmn_f,
	he_trig_usrinfo_set_t *usr_f)
{
	uint8 ru_indx, err = BCME_OK;
	uint8 m_stbc, ldpc_extra_symb, pe_disambg;
	uint8 t_pe, a_calc, pe_time_req, a_init, pe_int1, pe_int2;
	uint16 n_excess, n_sym_calc = 0, a_factor, a_fac_int;
	uint16 lsig_len, l_ldpc, n_cw, n_shrt, n_punc;
	uint16 n_cbps_short, he_ltf_symb_sz, symb_sz;
	uint16 tx_time, n_sym_init = 0, pe_disamb_chk;
	uint32 n_dbps, num_bytes, n_pld, n_av_bits, tmp4 = 0, n_cbps_last_init, n_dbps_short;
	uint32 n_dbps_last_init, n_cbps, tmp1 = 0, tmp2 = 0, tmp3 = 0;
	/* below parameters are not defined at this point , needs to be changed during bring up */
	uint8 cs_req = 0, cas_ind = 0, ap_tx_pow = 0, spat_reuse = 0, aid = 1;
	uint8 doppler = 0, he_siga_rsvd = 0, rsvd = 0, strm_offset = 0;
	/* data bits per symbol , row :: RU_index and coloumn :: mcs_val */
	uint16 n_dbps_tabl[MAX_RU_TONES][MCS_MAX_IDX] = {{12, 24, 36, 48, 72, 96, 108, 120,
		144, 160, 180, 200},
		{24, 48, 72, 96, 144, 192, 216, 240, 288, 320, 360, 400},
		{51, 102, 153, 204, 306, 408, 459, 510, 612, 680, 765, 850},
		{117, 234, 351, 468, 702, 936, 1053, 1170, 1404, 1560, 1755, 1950},
		{234, 468, 702, 936, 1404, 1872, 2106, 2340, 2808, 3120, 3510, 3900},
		{490, 980, 1470, 1960, 2940, 3920, 4410, 4900, 5880, 6533, 7350, 8166}};
	/* short sd carriers per RU */
	uint8 n_sd_short[MAX_RU_TONES] = {6, 12, 24, 60, 120, 240};
	/* coding rates for different mcs index values */
	/* express in Q8 format ( scale_factor =  2^ 8) */
	uint16 code_rate[MCS_MAX_IDX] = {6, 6, 9, 6, 9, 8, 9, 10, 9, 10, 9, 10};
	/* bits per sub-carrier per spatial stream */
	uint8 n_bpscs[MCS_MAX_IDX] = {1, 2, 2, 4, 4, 6, 6, 6, 8, 8, 10, 10};
	/* he_ltf symbol sizes */
	/* x10 format */
	uint16 he_ltf_symbsize[MAX_HE_LTFSYMBSZ_IDX] = {48, 80, 160};
	/* data symbol sizes */
	/* x10 format */
	uint16 data_symbsize[MAX_DATA_SYMBSZ_IDX] = {144, 144, 160};
	/* pe time array */
	uint8 pe_array[MAX_PESZ_IDX] = {0, 8, 16};
	/* afactor_encode values */
	uint8 afactor_array[MAX_AFACT_ENCODE_IDX] = {16, 12, 8, 4, 0};
	/* get the values into local variables */
	uint8 nss_temp = pkteng_trig_fill->nss_val;
	uint8 mcs_temp = pkteng_trig_fill->mcs_val;
	uint8 cp_ltf_temp = pkteng_trig_fill->cp_ltf_val;
	uint8 he_ltf_symb_temp = pkteng_trig_fill->he_ltf_symb;
	uint8 ru_alloc_tmp = (pkteng_trig_fill->ru_alloc_val);
	uint8 pe_tmp = pkteng_trig_fill->pe_category;
	uint8 stbc_tmp = pkteng_trig_fill->stbc;
	uint8 coding_tmp = pkteng_trig_fill->coding_val;
	uint8 bw_info = pkteng_trig_fill->bw;
	uint8 mu_mimo_ltf = pkteng_trig_fill->mumimo_ltfmode;
	uint8 dcm = pkteng_trig_fill->dcm;
	uint8 target_rssi = pkteng_trig_fill->tgt_rssi;
	uint8 ru160 = 0;
	/* required code rate */
	uint16 code_rate_R = code_rate[mcs_temp];
	/* add the mac header of 32 bytes also here to account for total packet length */
	num_bytes = (pkteng_trig_fill->num_bytes) + 32;

	if (ru_alloc_tmp > RUIDX_160MHZ_80U) {
		ru160 = 1;
		ru_alloc_tmp -= RUIDX_160MHZ_80U;
	}
	if (ru_alloc_tmp <= HE_MAX_26_TONE_RU_INDX) {
		ru_indx = 0;
	} else if (ru_alloc_tmp <= HE_MAX_52_TONE_RU_INDX) {
		ru_indx = 1;
	} else if (ru_alloc_tmp <= HE_MAX_106_TONE_RU_INDX) {
		ru_indx = 2;
	} else if (ru_alloc_tmp <= HE_MAX_242_TONE_RU_INDX) {
		ru_indx = 3;
	} else if (ru_alloc_tmp <= HE_MAX_484_TONE_RU_INDX) {
		ru_indx = 4;
	} else {
		ru_indx = 5;
	}
	n_cbps_short = (n_bpscs[mcs_temp] * nss_temp * n_sd_short[ru_indx]);
	n_dbps_short = (code_rate_R * n_cbps_short);
	he_ltf_symb_sz = he_ltf_symbsize[cp_ltf_temp];
	symb_sz = data_symbsize[cp_ltf_temp];
	pe_time_req = pe_array[pe_tmp];
	n_dbps = ((n_dbps_tabl[ru_indx][mcs_temp])* nss_temp);
	n_cbps = SCALE_CODE * n_dbps/code_rate_R;
	/* stbc value for calculations */
	if (stbc_tmp == 1)
		m_stbc = 2;
	else
		m_stbc = 1;
	/* calculations based on LDPC/BCC coding */
	if (coding_tmp == 0) {
		n_excess = ((LEFT_SHIFT(num_bytes, 3) +
		HE_N_TAIL + HE_N_SERVICE)% (m_stbc * n_dbps));
	} else {
		n_excess = ((LEFT_SHIFT(num_bytes, 3) + HE_N_SERVICE)% (m_stbc * n_dbps));
	}
	if (n_excess == 0) {
		a_init = 4;
	} else {
		tmp4 = m_stbc * n_dbps_short / SCALE_CODE;
		a_init = MIN(CEIL(n_excess, ROUNDUP(tmp4, 1)), 4);
	}
	/* data and coded bits in last OFDM symbol */
	if (a_init < 4) {
		n_dbps_last_init = (a_init * n_dbps_short);
		n_dbps_last_init = ROUNDUP(n_dbps_last_init / SCALE_CODE, 1);
		n_cbps_last_init = (a_init * n_cbps_short);
	} else {
		n_dbps_last_init = n_dbps;
		n_cbps_last_init = n_cbps;
	}

	if (coding_tmp == 0) {
		//BCC case
		n_sym_init = m_stbc * CEIL((LEFT_SHIFT(num_bytes, 3) +
		HE_N_TAIL + HE_N_SERVICE), (m_stbc * n_dbps));
		n_sym_calc = n_sym_init;
		a_calc = a_init;
		ldpc_extra_symb = 0;
	} else {
		//ldpc case
		n_sym_init = m_stbc * CEIL((LEFT_SHIFT(num_bytes, 3) +
			HE_N_SERVICE), (m_stbc * n_dbps));
		n_pld = ((n_sym_init - m_stbc) * n_dbps) + (m_stbc * n_dbps_last_init);
		n_av_bits = ((n_sym_init - m_stbc) * n_cbps) + (m_stbc * n_cbps_last_init);
		tmp4 = SCALE_CODE - code_rate_R;
		if (n_av_bits > 2592) {
			l_ldpc = 1944;
			n_cw = CEIL(n_pld, 1944 * code_rate_R / SCALE_CODE);
		} else if (n_av_bits <= 2592 && n_av_bits > 1944) {
			l_ldpc = 1296;
			n_cw = 2;
			if (n_av_bits >= (n_pld + 2916 * tmp4 / SCALE_CODE)) {
				l_ldpc = 1944;
			}
		} else if (n_av_bits <= 1944 && n_av_bits > 1296) {
			l_ldpc = 1944;
			n_cw = 1;
		} else if (n_av_bits <= 1296 && n_av_bits > 648) {
			l_ldpc = 1296;
			n_cw = 1;
			if (n_av_bits >= (n_pld + 1464 * tmp4 / SCALE_CODE)) {
				l_ldpc = 1944;
			}
		} else {
			l_ldpc = 648;
			n_cw = 1;
			if (n_av_bits >= (n_pld + 912 * tmp4 / SCALE_CODE)) {
				l_ldpc = 1296;
			}
		}
		tmp4 = n_cw * l_ldpc * code_rate_R / SCALE_CODE;
		n_shrt = MAX(0, ((int32)(tmp4 - n_pld)));
		n_punc = MAX(0, ((int32)((n_cw * l_ldpc) - n_av_bits - n_shrt)));
		tmp4 = (n_cw * l_ldpc) * (SCALE_CODE - code_rate_R);
		tmp1 = tmp4 / SCALE_CODE;
		tmp2 = 12 * n_punc * code_rate_R/(
			SCALE_CODE - code_rate_R);
		tmp3 = 3 * tmp4 / SCALE_CODE;
		/* calculations for LDPC extra symbol */
		if (((10 * n_punc > tmp1) && (10 * n_shrt < tmp2)) || (10 * n_punc > tmp3)) {
			ldpc_extra_symb = 1;
			if (a_init == 4) {
				n_sym_calc = (n_sym_init + m_stbc);
				a_calc = 1;
			} else {
				n_sym_calc = n_sym_init;
				a_calc = a_init + 1;
			}
		} else {
				ldpc_extra_symb = 0;
				n_sym_calc = n_sym_init;
				a_calc = a_init;
		}
	}
	a_factor = (a_calc % 4);
	a_fac_int = a_calc;

	if (pe_time_req >= (HE_T_MAX_PE - afactor_array[a_fac_int])) {
		t_pe = (HE_T_MAX_PE - afactor_array[a_fac_int]);
	} else {
		t_pe = 0;
	}

	/* L-sig (STF) + L-sig (LTF) + L-sig + RL_SIG + HE-SIGA + HE_STF+ HE_PREAMBLE + DATA + PE */
	tx_time = HE_T_LEG_STF + HE_T_LEG_LTF + HE_T_LEG_LSIG +
		HE_T_RL_SIG + HE_T_SIGA + HE_T_STF +
		CEIL((he_ltf_symb_temp * he_ltf_symb_sz) +
		(n_sym_calc * symb_sz), SYM_SZ_SCALE) + t_pe;
	lsig_len = (CEIL((tx_time - HE_T_LEG_PREAMBLE),
		HE_T_LEG_SYMB)* HE_N_LEG_SYM) -3 - 2;
	if (lsig_len > 4095) {
		WL_ERROR(("lsig_len: %d > 4095\n", lsig_len));
		err =  BCME_RANGE;
	}
	/* PE-disambiguity bit calculation */
	/* http://confluence.broadcom.com/pages/viewpage.action?
	 * spaceKey=WLAN&title=Fixed+point+calculation+of+PE+disambiguity+bit
	 */
	pe_int1 = ((tx_time - HE_T_LEG_PREAMBLE)% HE_T_LEG_SYMB);
	pe_int2 = ((pe_int1 != 0)? 1 : 0);
	pe_disamb_chk = SYM_SZ_SCALE*(t_pe + ((4 * pe_int2) - pe_int1));
	if (pe_disamb_chk >= symb_sz) {
		pe_disambg = 1;
	} else {
		pe_disambg = 0;
	}

	/* put the required prints here for debugging */
	/* basic trigger */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_FRMTYPE_INDX,
		HE_TRIG_CMNINFO_FRMTYPE_FSZ, HE_TRIG_TYPE_BASIC_FRM);
	/* L-sig length */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_LSIGLEN_INDX,
		HE_TRIG_CMNINFO_LSIGLEN_FSZ, lsig_len);
	/* cascade index bit */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_CASCADEIND_INDX,
		HE_TRIG_CMNINFO_CASCADEIND_FSZ, cas_ind);
	/* CS required */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_CSREQ_INDX,
		HE_TRIG_CMNINFO_CSREQ_FSZ, cs_req);
	/* BW info */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_BWINFO_INDX,
		HE_TRIG_CMNINFO_BWINFO_FSZ, bw_info);
	/* GI-LTF */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_GI_LTF_INDX,
		HE_TRIG_CMNINFO_GI_LTF_FSZ, cp_ltf_temp);
	/* MUMIMO-LTF indx */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_MUMIMO_LTF_INDX,
		HE_TRIG_CMNINFO_MUMIMO_LTF_FSZ, mu_mimo_ltf);
	/* HE-LTF symbols :: writing encoded value /2 into TDC and PHYCTL  */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_HELTF_SYM_INDX,
		HE_TRIG_CMNINFO_HELTF_SYM_FSZ, (he_ltf_symb_temp >> 1));
	/* STBC */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_STBC_INDX,
		HE_TRIG_CMNINFO_STBC_FSZ, stbc_tmp);
	/* LDPC extra symb */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_LDPC_EXTSYM_INDX,
		HE_TRIG_CMNINFO_LDPC_EXTSYM_FSZ, ldpc_extra_symb);
	/* AP TX power */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_AP_TXPWR_INDX,
		HE_TRIG_CMNINFO_AP_TXPWR_FSZ, ap_tx_pow);
	/* a-factor */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_AFACT_INDX,
		HE_TRIG_CMNINFO_AFACT_FSZ, a_factor);
	/* PE disambig */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_PEDISAMBIG_INDX,
		HE_TRIG_CMNINFO_PEDISAMBIG_FSZ, pe_disambg);
	/* spatial re-use */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_SPTIAL_REUSE_INDX,
		HE_TRIG_CMNINFO_SPTIAL_REUSE_FSZ, spat_reuse);
	/* doppler indx */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_DOPPLER_INDX,
		HE_TRIG_CMNINFO_DOPPLER_FSZ, doppler);
	/* HE-SIGA rsvd */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_HESIGA_RSVD_INDX,
		HE_TRIG_CMNINFO_HESIGA_RSVD_FSZ, he_siga_rsvd);
	/* rsvd */
	setbits((uint8 *)cmn_f, sizeof(*cmn_f), HE_TRIG_CMNINFO_RSVD_INDX,
		HE_TRIG_CMNINFO_RSVD_FSZ, rsvd);

	/* user info fields */

	/* AID */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_AID_INDX,
		HE_TRIG_USRINFO_AID_FSZ, aid);
	/* RU Alloc index :: actual ru_alloc field is 8 bits so multiply by 2 and give to ucode */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_RU_ALLOC_INDX,
		HE_TRIG_USRINFO_RU_ALLOC_FSZ, (ru_alloc_tmp << 1) + ru160);
	/* coding type */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_CODING_INDX,
		HE_TRIG_USRINFO_CODING_FSZ, coding_tmp);
	/* mcs index */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_MCS_INDX,
		HE_TRIG_USRINFO_MCS_FSZ, mcs_temp);
	/* dcm */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_DCM_INDX,
		HE_TRIG_USRINFO_DCM_FSZ, dcm);
	/* stream offset */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_SSALLOC_STRMOFFSET_INDX,
		HE_TRIG_USRINFO_SSALLOC_STRMOFFSET_FSZ, strm_offset);
	/* number of spatial streams */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_SSALLOC_NSS_INDX,
		HE_TRIG_USRINFO_SSALLOC_NSS_FSZ, (nss_temp - 1));
	/* target rssi */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_TARGET_RSSI_INDX,
		HE_TRIG_USRINFO_TARGET_RSSI_FSZ, target_rssi);
	/* rsvd */
	setbits((uint8 *)usr_f, sizeof(*usr_f), HE_TRIG_USRINFO_RSVD_INDX,
		HE_TRIG_USRINFO_RSVD_FSZ, rsvd);

	return err;
}

static int
wlc_test_get_trig_info(wlc_hw_info_t *wlc_hw, wl_trig_frame_info_t *trig_frame_info)
{
	uint16 val;
	uint8 vals[8];

	trig_frame_info->version = TRIG_FRAME_FORMAT_11AX_DRAFT_1P1;
	trig_frame_info->length = sizeof(wl_trig_frame_info_t);

	wlc_bmac_copyfrom_shm(wlc_hw, M_RXTRIG_CMNINFO(wlc_hw), vals, sizeof(vals));

	trig_frame_info->trigger_type = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_FRMTYPE_INDX,
		HE_TRIG_CMNINFO_FRMTYPE_FSZ);
	trig_frame_info->lsig_len = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_LSIGLEN_INDX,
		HE_TRIG_CMNINFO_LSIGLEN_FSZ);

	trig_frame_info->cascade_indication = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_CASCADEIND_INDX,
		HE_TRIG_CMNINFO_CASCADEIND_FSZ);

	trig_frame_info->cs_req = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_CSREQ_INDX,
		HE_TRIG_CMNINFO_CSREQ_FSZ);

	trig_frame_info->bw = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_BWINFO_INDX,
		HE_TRIG_CMNINFO_BWINFO_FSZ);

	trig_frame_info->cp_ltf_type = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_GI_LTF_INDX,
		HE_TRIG_CMNINFO_GI_LTF_FSZ);

	trig_frame_info->mu_mimo_ltf_mode = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_MUMIMO_LTF_INDX,
		HE_TRIG_CMNINFO_MUMIMO_LTF_FSZ);

	trig_frame_info->num_he_ltf_syms = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_HELTF_SYM_INDX,
		HE_TRIG_CMNINFO_HELTF_SYM_FSZ);

	trig_frame_info->stbc = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_STBC_INDX,
		HE_TRIG_CMNINFO_STBC_FSZ);

	trig_frame_info->ldpc_extra_symb = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_LDPC_EXTSYM_INDX,
		HE_TRIG_CMNINFO_LDPC_EXTSYM_FSZ);

	trig_frame_info->ap_tx_pwr = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_AP_TXPWR_INDX,
		HE_TRIG_CMNINFO_AP_TXPWR_FSZ);

	trig_frame_info->afactor = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_AFACT_INDX,
		HE_TRIG_CMNINFO_AFACT_FSZ);

	trig_frame_info->pe_disambiguity = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_PEDISAMBIG_INDX,
		HE_TRIG_CMNINFO_PEDISAMBIG_FSZ);

	trig_frame_info->spatial_resuse = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_SPTIAL_REUSE_INDX,
		HE_TRIG_CMNINFO_SPTIAL_REUSE_FSZ);

	trig_frame_info->doppler = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_DOPPLER_INDX,
		HE_TRIG_CMNINFO_DOPPLER_FSZ);

	trig_frame_info->he_siga_rsvd = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_HESIGA_RSVD_INDX,
		HE_TRIG_CMNINFO_HESIGA_RSVD_FSZ);

	trig_frame_info->cmn_info_rsvd = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_CMNINFO_RSVD_INDX,
		HE_TRIG_CMNINFO_RSVD_FSZ);

	/* user info - SHM0 */
	wlc_bmac_copyfrom_shm(wlc_hw, M_RXTRIG_USRINFO(wlc_hw), vals, sizeof(vals));

	trig_frame_info->aid12 = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_AID_INDX,
		HE_TRIG_USRINFO_AID_FSZ);

	val = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_RU_ALLOC_INDX,
		HE_TRIG_USRINFO_RU_ALLOC_FSZ);
	trig_frame_info->ru_alloc = ((val | ((val & 1) << 8))) >> 1;

	trig_frame_info->coding_type = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_CODING_INDX,
		HE_TRIG_USRINFO_CODING_FSZ);

	trig_frame_info->mcs = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_MCS_INDX,
		HE_TRIG_USRINFO_MCS_FSZ);

	trig_frame_info->dcm = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_DCM_INDX,
		HE_TRIG_USRINFO_DCM_FSZ);

	trig_frame_info->ss_alloc = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_SSALLOC_STRMOFFSET_INDX,
		HE_TRIG_USRINFO_SSALLOC_STRMOFFSET_FSZ);

	trig_frame_info->ss_alloc = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_SSALLOC_NSS_INDX,
		HE_TRIG_USRINFO_SSALLOC_NSS_FSZ);

	trig_frame_info->tgt_rssi = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_TARGET_RSSI_INDX,
		HE_TRIG_USRINFO_TARGET_RSSI_FSZ);

	trig_frame_info->usr_info_rsvd = (uint16)getbits(&vals[0],
		sizeof(vals),
		HE_TRIG_USRINFO_RSVD_INDX,
		HE_TRIG_USRINFO_RSVD_FSZ);

	return BCME_OK;
}

/* Write a list of values to a starting shm address continuously */
static uint
wlc_test_incremental_write_shm(wlc_hw_info_t *wlc_hw, uint start_offset,
	uint16 *val16, int num)
{
	int i;
	uint offset;

	offset = start_offset;
	for (i = 0; i < num; i++) {
		wlc_bmac_write_shm(wlc_hw, offset, *val16);
		offset += 2;
		val16 ++;
	}
	return offset;
}

/* d11 dma loopback test */
int
wlc_d11_dma_lpbk_init(wlc_test_info_t *testi)
{
	wlc_info_t *wlc = testi->wlc;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	osl_t *osh = wlc_hw->osh;
	uint16 rcv_ctl;

	if (testi->d11_dma_lpbk_init) {
		WL_ERROR(("D11 DMA Loopback already initialized !!!\n"));
		return BCME_ERROR;
	}

	if (!wlc->clk) {
		WL_ERROR(("D11 DMA Loopback, No clk !!!\n"));
		return BCME_NOCLK;
	}

	if (wlc_isup(wlc)) {
		WL_ERROR(("D11 DMA Loopback, Not down !!!\n"));
		return BCME_NOTDOWN;
	}

	/* init tx and rx dma channels */
	dma_txinit(wlc_hw->di[TX_DATA_FIFO]);
	dma_rxinit(wlc_hw->di[RX_FIFO]);

	/* disable rxbmmap */
	if (D11REV_GE(wlc_hw->corerev, 48)) {
		uint16 bmccmd1;

		/* If 1, selects RXBMMAP pass-through path, meaning the
		 * block will behave like it does not exist.
		 * Normal RXBMMAP operation if 0.clear device interrupts
		 */
		testi->rxbmmap_save = R_REG(osh, D11_BMCCmd1(wlc_hw));
		bmccmd1 = testi->rxbmmap_save | (1 << BMCCmd1_RXMapPassThru_SHIFT);
		W_REG(osh, D11_BMCCmd1(wlc_hw), bmccmd1);
	}

	/* fifo selection */
	/* fifoSel field of RXE RCV_CTL */
	testi->fifosel_save = R_REG(osh, D11_RCV_CTL(wlc_hw));
	rcv_ctl = ((testi->fifosel_save & ~RX_CTL_FIFOSEL_MASK) |
		((RX_FIFO << RX_CTL_FIFOSEL_SHIFT) & RX_CTL_FIFOSEL_MASK));
	W_REG(osh, D11_RCV_CTL(wlc_hw), rcv_ctl);

	/* enable fifo-level loopback */
	dma_fifoloopbackenable(wlc_hw->di[TX_DATA_FIFO]);

	testi->d11_dma_lpbk_init = 1;
	return BCME_OK;
}

int
wlc_d11_dma_lpbk_uninit(wlc_test_info_t *testi)
{
	wlc_info_t *wlc = testi->wlc;
	wlc_hw_info_t *wlc_hw = wlc->hw;
	osl_t *osh = wlc_hw->osh;

	if (!testi->d11_dma_lpbk_init) {
		WL_ERROR(("D11 DMA Loopback not initialized !!!\n"));
		return BCME_ERROR;
	}

	/* restore rxbbmap */
	if (D11REV_GE(wlc_hw->corerev, 48)) {
		W_REG(osh, D11_BMCCmd1(wlc_hw), testi->rxbmmap_save);
	}

	/* restore rcv_ctl */
	W_REG(osh, D11_RCV_CTL(wlc_hw), testi->fifosel_save);

	/* reset tx and rx dma channels; also disable fifo loopback */
	dma_txreset(wlc_hw->di[TX_DATA_FIFO]);
	wlc_upd_suspended_fifos_clear(wlc_hw, TX_DATA_FIFO);

	dma_rxreset(wlc_hw->di[RX_FIFO]);

	/* free posted tx and rx packets */
	dma_txreclaim(wlc_hw->di[TX_DATA_FIFO], HNDDMA_RANGE_ALL);
	dma_rxreclaim(wlc_hw->di[RX_FIFO]);

	testi->d11_dma_lpbk_init = 0;
	return BCME_OK;
}

static uint32
wlc_do_d11_dma_lpbk(wlc_hw_info_t *wlc_hw, uint8 *buf, int len)
{
#define WAIT_COUNT_XI	100	/* wait count for tx interrupt */

	wlc_info_t *wlc = wlc_hw->wlc;
	osl_t *osh = wlc_hw->osh;
	void *p;
	void *rx_p = NULL;
	uint8 *data;
	uint8 *rx_data;
	uint32 intstatus;
	uint32 macintstatus;
	int i;
	uint32 status = BCME_OK;
#ifdef BULKRX_PKTLIST
	rx_list_t rx_list = {NULL};
#ifdef STS_FIFO_RXEN
	 rx_list_t rx_sts_list = {NULL};
#endif // endif
	int delay_cnt = 5000;
#endif /* BULKRX_PKTLIST */

	/* post receive buffers */
	dma_rxfill(wlc_hw->di[RX_FIFO]);
#ifdef STS_FIFO_RXEN
	if (STS_RX_ENAB(wlc->pub)) {
		wlc_bmac_dma_rxfill(wlc->hw, STS_FIFO);
	}
#endif /* STS_FIFO_RXEN */

	/* alloc tx packet */
	if ((p = PKTGET(osh, len, TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: tx PKTGET error\n", wlc_hw->unit, __FUNCTION__));
		status = BCME_NOMEM;
		goto cleanup;
	}
	data = (uint8 *)PKTDATA(osh, p);
	ASSERT(ISALIGNED(data, sizeof(uint32)));

	memcpy(data, buf, len);
	/* set buf to 0 so failure can be detected by comparing the data */
	memset(buf, 0, len);

	/* clear device interrupts */
	macintstatus = R_REG(osh, D11_MACINTSTATUS(wlc));
	W_REG(osh, D11_MACINTSTATUS(wlc), macintstatus);
	intstatus = R_REG(osh, &(D11Reggrp_intctrlregs(wlc_hw, TX_DATA_FIFO)->intstatus));
	W_REG(osh, &(D11Reggrp_intctrlregs(wlc_hw, TX_DATA_FIFO)->intstatus), intstatus);

	/* tx */
	wlc_bmac_txfifo(wlc_hw, TX_DATA_FIFO, p, TRUE, INVALIDFID, 1);

	/* expect tx interrupt */
	for (i = 0; i < WAIT_COUNT_XI; i++) {
		OSL_DELAY(10);	/* wait for fifo loopback to finish */
		intstatus = R_REG(osh, &(D11Reggrp_intctrlregs(wlc_hw, TX_DATA_FIFO)->intstatus));

		if (intstatus & (I_PC | I_PD | I_DE | I_RU | I_RO | I_XU)) {
			status = BCME_ERROR;

			macintstatus = R_REG(osh, D11_MACINTSTATUS(wlc));
			WL_ERROR(("wl%d: %s: error int: macintstatus 0x%x intstatus[%d] 0x%x\n",
				wlc_hw->unit, __FUNCTION__, macintstatus, TX_DATA_FIFO,
				intstatus));
			goto cleanup;
		}
		if (intstatus & I_XI)
			break;
	}
	if (!(intstatus & I_XI)) {
		status = BCME_ERROR;
		macintstatus = R_REG(osh, D11_MACINTSTATUS(wlc));
		WL_ERROR(("wl%d: %s: timeout waiting tx int; macintstatus 0x%x intstatus[%d]"
			" 0x%x\n",
			wlc_hw->unit, __FUNCTION__, macintstatus, TX_DATA_FIFO, intstatus));
		goto cleanup;
	}

	p = dma_getnexttxp(wlc_hw->di[TX_DATA_FIFO], HNDDMA_RANGE_TRANSMITTED);
	ASSERT(p != NULL);
	PKTFREE(wlc_hw->osh, p, TRUE);

	/* rx */
#ifdef BULKRX_PKTLIST
	do {
		OSL_DELAY(1);
		delay_cnt--;
		dma_rx(wlc_hw->di[RX_FIFO], &rx_list, NULL, 1);
	} while ((rx_list.rx_head == NULL) && (delay_cnt >= 0));

#ifdef STS_FIFO_RXEN
	if (STS_RX_ENAB(wlc->pub)) {
		delay_cnt = 5000;
		do {
			OSL_DELAY(1);
			delay_cnt--;
			dma_sts_rx(wlc_hw->di[STS_FIFO], &rx_sts_list);

		} while ((rx_sts_list.rx_head == NULL) && (delay_cnt >= 0));
		wlc_bmac_recv_process_sts(wlc_hw, RX_FIFO,
			&rx_list, &rx_sts_list, 0);
	}
#endif /* STS_FIFO_RXEN */
	rx_p = rx_list.rx_head;
#else
	SPINWAIT((rx_p = dma_rx(wlc_hw->di[RX_FIFO])) == NULL, 5000);
#endif /* BULKRX_PKTLIST */

	/* no packet received */
	if (rx_p == NULL) {
		status = BCME_ERROR;
		WL_ERROR(("wl%d: %s: no packet looped back\n", wlc_hw->unit, __FUNCTION__));
		goto cleanup;
	}

	/* Since rx interrupt is not asserted for fifo loopback, the surest way
	 * to ensure flushing is to have dma rx done.
	 */
	rx_data = (uint8 *)PKTDATA(osh, rx_p) + wlc->d11rxoff;
	memcpy(buf, rx_data, len);
	PKTFREE(osh, rx_p, FALSE);

cleanup:
	return status;
}

int
wlc_d11_dma_lpbk_run(wlc_test_info_t *testi, uint8 *buf, int len)
{
	#define MAX_T_LEN	64
	wlc_info_t *wlc = testi->wlc;
	int tlen, rlen = len;
	uint8 *p = buf;
	int err = 0;

	if ((buf == NULL) || (len <= 0)) {
		return BCME_BADARG;
	}

	if (!testi->d11_dma_lpbk_init) {
		WL_ERROR(("D11 DMA Loopback not initialized !!!\n"));
		memset(buf, 0, len);
		return BCME_ERROR;
	}

	/* In rx, only the frame header remains in dongle memory, other portion
	 * goes to host memory, so the length of one transaction cannot exceeds
	 * the size of frame header
	 */
	while (rlen) {
		tlen = MIN(rlen, MAX_T_LEN);
		/* p contains the input and output data, the output data are
		 * set to all 0s in failed cases, so the failure can be detected
		 * by comparing the data in host driver
		 */
		if ((err = wlc_do_d11_dma_lpbk(wlc->hw, p, tlen)) != 0) {
			WL_ERROR(("D11 DMA Loopback, failed %d\n", err));
			break;
		}
		p += tlen;
		rlen -= tlen;
	}

	return err;
}

static int
wlc_test_pkteng_cmd_getkey(wlc_test_info_t *test, wl_pkteng_cmd_params_t *params,
	char *outbuf, int outlen)
{
	struct bcmstrbuf bstr;
	int err;
	int8 staid, fifoid;
	int keylen, vallen;

	err = BCME_OK;

	staid = params->u.argkeyval.val[0];
	fifoid = params->u.argkeyval.val[1];
	keylen = params->u.argkeyval.keystr_len;
	vallen = params->u.argkeyval.valstr_len;

	bcm_binit(&bstr, outbuf, outlen);

	BCM_REFERENCE(vallen);
	BCM_REFERENCE(keylen);

	bcm_bprintf(&bstr, "Pkteng staidx %d qidx %d key %s\n",
	staid, fifoid, params->u.argkeyval.keystr);

	/* Get key handlers starts here */
	if (!strncmp(params->u.argkeyval.keystr, "all", keylen)) {
		bcm_bprintf(&bstr, "Pkteng staidx all\n");
	}

	return err;
}

static int
wlc_test_pkteng_cmd_getusr(wlc_test_info_t *test, wl_pkteng_cmd_params_t *params,
	char *outbuf, int outlen)
{
	struct bcmstrbuf bstr;
	wlc_info_t *wlc;
	int err, i, j, pkteng_shm_sts;
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	wlc_test_pkteng_fifoinfo_t *fifo;
	char eabuf[ETHER_ADDR_STR_LEN];
	bool is_verbose;
	char *keystr;
	int keylen;
	bool print_invalid_usr, is_dl, print_shm;
	uint offset;
	uint16 val16;
	uint16 nusrs;

	err = BCME_OK;
	print_invalid_usr = FALSE;
	wlc = test->wlc;
	info = test->pkteng_info;
	keystr = params->u.argkeyval.keystr;
	keylen = params->u.argkeyval.keystr_len;
	is_verbose = WLC_TEST_PKTENGCMD_IS_VERBOSE(info->flags) |
	(params->flags & PKTENGCMD_FLAGS_VERBOSE);
	pkteng_shm_sts = 0;
	print_invalid_usr = print_shm = FALSE;
	is_dl = (info->mode == WL_PKTENG_CMD_STRT_DL_CMD);

	bcm_binit(&bstr, outbuf, outlen);

	BCM_REFERENCE(nusrs);

	if (keylen != 0) {
		if (!strncmp(keystr, "allusr", strlen("allusr"))) {
			print_invalid_usr = TRUE;
		} else if (!strncmp(keystr, "shm", strlen("shm")))
			print_shm = TRUE;
	}

	if (print_shm) {
		if ((info->mode == WL_PKTENG_CMD_STRT_DL_CMD) ||
			WLC_TEST_PKTENGCMD_IS_DLSCH(info->flags)) {
#if defined(WL_PSMX)
			offset = MX_HEMSCH0_BLK(wlc);
			val16 = wlc_bmac_read_shmx(wlc->hw, offset);
			bcm_bprintf(&bstr, "++++ DL OFDMA scheduler in shmx ++++\n");
			bcm_bprintf(&bstr, "MX_HEMSCH_BLK (%04x): State %4x\n", offset, val16);

			offset = MX_HEMSCH0_SIGA(wlc);
			bcm_bprintf(&bstr, "MX_HEMSCH0_SIGA (%4x): ", offset);
			for (i = 0; i < 3; i++) {
				val16 = wlc_bmac_read_shmx(wlc->hw, offset+2*i);
				bcm_bprintf(&bstr, "%04x ", val16);
			}
			bcm_bprintf(&bstr, "\n");

			offset = MX_HEMSCH0_PCTL0(wlc);
			bcm_bprintf(&bstr, "MX_HEMSCH0_PCTL0 (%4x): ", offset);
			for (i = 0; i < 5; i++) {
				val16 = wlc_bmac_read_shmx(wlc->hw, offset+2*i);
				bcm_bprintf(&bstr, "%4x ", val16);
			}
			bcm_bprintf(&bstr, "\n");

			offset = MX_HEMSCH0_N(wlc);
			val16 = wlc_bmac_read_shmx(wlc->hw, offset);
			bcm_bprintf(&bstr, "MX_HEMSCH0_N (%04x): %4x", offset, val16);
			nusrs = val16;
			offset += 2;
			for (i = 0; i < nusrs; i++) {
				if ((i % 4 == 0)) {
					bcm_bprintf(&bstr, "\n  ");
				}
				val16 = wlc_bmac_read_shmx(wlc->hw, offset+4*i);
				bcm_bprintf(&bstr, "%x ", val16);
				val16 = wlc_bmac_read_shmx(wlc->hw, offset+4*i+2);
				bcm_bprintf(&bstr, "%x | ", val16);
			}
			bcm_bprintf(&bstr, "\n");
#endif /* defined(WL_PSMX) */
		}
		if ((info->mode == WL_PKTENG_CMD_STRT_UL_CMD) ||
			WLC_TEST_PKTENGCMD_IS_ULSCH(info->flags)) {
			/* print related shm for ul ofdma */
			bcm_bprintf(&bstr, "++++ UL OFDMA scheduler in shm ++++\n");
			/* print len, rate, mintime */
			offset = M_TXTRIG_FLAG(wlc);
			val16 = wlc_bmac_read_shm(wlc->hw, offset);
			bcm_bprintf(&bstr, "txtrig_flag num len (%04x): %04x", offset, val16);
			offset = M_TXTRIG_NUM(wlc);
			val16 = wlc_bmac_read_shm(wlc->hw, offset);
			bcm_bprintf(&bstr, " %04x", val16);
			offset = M_TXTRIG_LEN(wlc);
			val16 = wlc_bmac_read_shm(wlc->hw, offset);
			bcm_bprintf(&bstr, " %04x\n", val16);
			offset = M_TXTRIG_RATE(wlc);
			val16 = wlc_bmac_read_shm(wlc->hw, offset);
			bcm_bprintf(&bstr, "txtrig_rate mintime (%04x): %04x", offset, val16);
			offset = M_TXTRIG_MINTIME(wlc);
			val16 = wlc_bmac_read_shm(wlc->hw, offset);
			bcm_bprintf(&bstr, " %04x\n", val16);
			offset = M_TXTRIG_FRAME(wlc);
			bcm_bprintf(&bstr, "txtrig_frame (%04x): ", offset);
			for (i = 0; i < 8; i++) {
				val16 = wlc_bmac_read_shm(wlc->hw, offset);
				offset += 2;
				bcm_bprintf(&bstr, "%04x ", val16);
			}
			bcm_bprintf(&bstr, "\n");

			/* pirnt CMNINFO in TXTRIG */
			offset = M_TXTRIG_CMNINFO(wlc->hw);
			bcm_bprintf(&bstr, "cmninfo (%04x): ", offset);
			for (i = 0; i < 4; i++) {
				val16 = wlc_bmac_read_shm(wlc->hw, offset);
				offset += 2;
				bcm_bprintf(&bstr, "%04x ", val16);
			}
			bcm_bprintf(&bstr, "\n");

			/* print USRINFO in TXTRIG */
			bcm_bprintf(&bstr, "usrinfo (%04x):\n", offset);
			for (i = 0; i < info->nusrs; i++) {
				bcm_bprintf(&bstr, "  %d sta %d aid %x: ",
					i, info->usrlist[i], info->usr[info->usrlist[i]].aid);
				val16 = wlc_bmac_read_shm(wlc->hw, offset);
				offset += 2;
				bcm_bprintf(&bstr, "%04x ", val16);
				val16 = wlc_bmac_read_shm(wlc->hw, offset);
				offset += 2;
				bcm_bprintf(&bstr, "%04x ", val16);
				val16 = wlc_bmac_read_shm(wlc->hw, offset);
				offset += 2;
				bcm_bprintf(&bstr, "%04x\n", val16);
			}

			/* print USRINFO in TXTRIG */
			offset = M_TXTRIG_SRXCTL(wlc->hw);
			bcm_bprintf(&bstr, "srctl (%04x): ", offset);
			for (i = 0; i < 4; i++) {
				val16 = wlc_bmac_read_shm(wlc->hw, offset);
				offset += 2;
				bcm_bprintf(&bstr, "%04x ", val16);
			}
			bcm_bprintf(&bstr, "\n");

			offset = M_TXTRIG_SRXCTLUSR(wlc->hw);
			bcm_bprintf(&bstr, "rxctl usrlist (%04x): ", offset);
			for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
				if (i % 8 == 0) {
					bcm_bprintf(&bstr, "\n");
				}
				val16 = wlc_bmac_read_shm(wlc->hw, offset);
				offset += 2;
				bcm_bprintf(&bstr, "%04x ", val16);
			}
			bcm_bprintf(&bstr, "\n");
		}
		if (info->mode == WL_PKTENG_CMD_STRT_ULTX_CMD) {
			wl_trig_frame_info_t tinfo;
			wlc_test_get_trig_info(wlc->hw, &tinfo);
			/* pirnt CMNINFO in RXTRIG */
			bcm_bprintf(&bstr, "11ax Trigger Frame Parsed info\n\n");
			bcm_bprintf(&bstr, "Interface Version: %d\n", tinfo.version);
			bcm_bprintf(&bstr, "Common Info:\n\n");
			bcm_bprintf(&bstr, "trigger_type: 0x%x\n", tinfo.trigger_type);
			bcm_bprintf(&bstr, "Lsig length: 0x%x\n", tinfo.lsig_len);
			bcm_bprintf(&bstr, "cascade_indication: 0x%x\n", tinfo.cascade_indication);
			bcm_bprintf(&bstr, "cs_req: 0x%x\n", tinfo.cs_req);
			bcm_bprintf(&bstr, "bw: 0x%x\n", tinfo.bw);
			bcm_bprintf(&bstr, "cp_ltf_type: 0x%x\n", tinfo.cp_ltf_type);
			bcm_bprintf(&bstr, "mu_mimo_ltf_mode: 0x%x\n", tinfo.mu_mimo_ltf_mode);
			bcm_bprintf(&bstr, "num_he_ltf_syms: 0x%x\n", tinfo.num_he_ltf_syms);
			bcm_bprintf(&bstr, "stbc: 0x%x\n", tinfo.stbc);
			bcm_bprintf(&bstr, "ldpc_extra_symb: 0x%x\n", tinfo.ldpc_extra_symb);
			bcm_bprintf(&bstr, "ap_tx_pwr: 0x%x\n", tinfo.ap_tx_pwr);
			bcm_bprintf(&bstr, "afactor: 0x%x\n", tinfo.afactor);
			bcm_bprintf(&bstr, "pe_disambiguity: 0x%x\n", tinfo.pe_disambiguity);
			bcm_bprintf(&bstr, "spatial_resuse: 0x%x\n", tinfo.spatial_resuse);
			bcm_bprintf(&bstr, "doppler: 0x%x\n", tinfo.doppler);
			bcm_bprintf(&bstr, "he_siga_rsvd: 0x%x\n", tinfo.he_siga_rsvd);
			/* print USRINFO in RXTRIG */
			bcm_bprintf(&bstr, "\nUser Info:\n\n");
			bcm_bprintf(&bstr, "aid12: 0x%x\n", tinfo.aid12);
			bcm_bprintf(&bstr, "ru_alloc: 0x%x\n", tinfo.ru_alloc);
			bcm_bprintf(&bstr, "coding_type: 0x%x\n", tinfo.coding_type);
			bcm_bprintf(&bstr, "mcs: 0x%x\n", tinfo.mcs);
			bcm_bprintf(&bstr, "dcm: 0x%x\n", tinfo.dcm);
			bcm_bprintf(&bstr, "ss_alloc: 0x%x\n", tinfo.ss_alloc);
			bcm_bprintf(&bstr, "tgt_rssi: 0x%x\n", tinfo.tgt_rssi);
			bcm_bprintf(&bstr, "usr_info_rsvd: 0x%x\n", tinfo.usr_info_rsvd);
		}
		return err;
	}

	bcm_bprintf(&bstr, "Pkteng mode ");
	if (info->mode == WL_PKTENG_CMD_STRT_DL_CMD) {
		bcm_bprintf(&bstr, "DL ");

		pkteng_shm_sts = wlc_bmac_read_shm(wlc->hw, M_MFGTEST_NUM(wlc->hw));
		if ((pkteng_shm_sts & MFGTEST_TXMODE) == 0) {
			/* pkteng has been stopped, could be nframes = 0 */
			info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_RUN;
		}
	} else if (info->mode == WL_PKTENG_CMD_STRT_UL_CMD) {
		bcm_bprintf(&bstr, "UL ");
		pkteng_shm_sts = wlc_bmac_read_shm(wlc->hw, M_TXTRIG_FLAG(wlc->hw));
		if ((pkteng_shm_sts & 1) == 0) {
			/* pkteng has been stopped, could be nframes = 0 */
			info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_RUN;
		}
	} else if (info->mode == WL_PKTENG_CMD_STRT_ULTX_CMD) {
		bcm_bprintf(&bstr, "ULTX ");
		pkteng_shm_sts = wlc_bmac_read_shm(wlc->hw, M_MFGTEST_NUM(wlc->hw));
		if ((pkteng_shm_sts & MFGTEST_TXMODE) == 0) {
			/* pkteng has been stopped, could be nframes = 0 */
			info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_RUN;
		}
	} else {
		bcm_bprintf(&bstr, "XX ");
		info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_RUN;
	}

	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_EN) != 0) {
		bcm_bprintf(&bstr, "enabled ");
	}
	if (WLC_TEST_PKTENGCMD_IS_VERBOSE(info->flags)) {
		bcm_bprintf(&bstr, "verbose ");
	}
	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_COMMITED) != 0) {
		bcm_bprintf(&bstr, "commited ");
	}
	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_UPDATED) != 0) {
		bcm_bprintf(&bstr, "updated ");
	}
	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO) != 0) {
		bcm_bprintf(&bstr, "manual_ul_configure ");
	}
	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_RUN) != 0) {
		bcm_bprintf(&bstr, "running ");
	}
	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA) != 0) {
		bcm_bprintf(&bstr, "manual_siga ");
	}
	if (WLC_TEST_PKTENGCMD_IS_DLSCH(info->flags)) {
		bcm_bprintf(&bstr, "dl_sch ");
	}
	if (WLC_TEST_PKTENGCMD_IS_ULSCH(info->flags)) {
		bcm_bprintf(&bstr, "ul_sch ");
	}
	if  ((keylen != 0) && is_verbose) {
		bcm_bprintf(&bstr, " | input_key: %s", keystr);
	}

	if (info->mode == WL_PKTENG_CMD_STRT_ULTX_CMD) {
		bcm_bprintf(&bstr, "|| maxdur %d mindur %d\n", info->max_dur, info->min_dur);
	} else {
		bcm_bprintf(&bstr, "|| numtx %d ifs %d maxdur %d mindur %d\n",
		info->nframes, info->ifs, info->max_dur, info->min_dur);
	}

	if (WLC_TEST_PKTENGCMD_IS_ULSCH(info->flags) ||
		(info->mode == WL_PKTENG_CMD_STRT_ULTX_CMD)) {
		bcm_bprintf(&bstr, "\nUL info\n trig_type %d, lsig_len %d, more_tf %d\n "
			"cs_req %d bw %d cp_ltftype %d mu_mimo_ltf %d numheltf %d\n "
			"stbc %d ldpc_extsym %d ap_txpwr %d afact %d pe %d\n "
			"spat_reuse %d doppler %d hesig_rsvd %d rsvd %d nbyte_pad %d\n\n",
			info->txtrig_type, info->ul_lsig_len, info->ul_more_tf, info->ul_cs_req,
			info->ul_bw, info->ul_cp_ltftype, info->ul_mu_mimo_ltf,
			info->ul_numheltf, info->ul_stbc, info->ul_ldpc_extsym,
			info->ul_ap_txpwr, info->ul_afact, info->ul_pe_disambig,
			info->ul_spat_reuse, info->ul_doppler, info->ul_hesiga2_rsvd,
			info->ul_rsvd, info->nbyte_pad);
	}

	if (is_verbose) {

		bcm_bprintf(&bstr, "\tmode 0x%x flag 0x%x ul_numheltf %d siga [%x %x %x] "
			"phyctl [%x %x %x %x %x] rspec %x shm_sts %x\n",
			info->mode, info->flags, info->ul_numheltf,
			info->siga[0], info->siga[1], info->siga[2],
			info->phyctl[0], info->phyctl[1], info->phyctl[2],
			info->phyctl[3], info->phyctl[4],
			info->rspec, pkteng_shm_sts);

		bcm_bprintf(&bstr, "\nUL info\n trig_type %d, lsig_len %d, more_tf %d\n "
			"cs_req %d bw %d cp_ltftype %d mu_mimo_ltf %d numheltf %d\n "
			"stbc %d ldpc_extsym %d ap_txpwr %d afact %d pe %d\n "
			"spat_reuse %d doppler %d hesig_rsvd %d rsvd %d nbyte_pad %d\n\n",
			info->txtrig_type, info->ul_lsig_len, info->ul_more_tf, info->ul_cs_req,
			info->ul_bw, info->ul_cp_ltftype, info->ul_mu_mimo_ltf,
			info->ul_numheltf, info->ul_stbc, info->ul_ldpc_extsym,
			info->ul_ap_txpwr, info->ul_afact, info->ul_pe_disambig,
			info->ul_spat_reuse, info->ul_doppler, info->ul_hesiga2_rsvd,
			info->ul_rsvd, info->nbyte_pad);
	}

	bcm_bprintf(&bstr, "Pkteng STA_config num %d list: ", info->nusrs);

	bcm_bprintf(&bstr, "[");
	for (i = 0; i < info->nusrs; i++) {
		bcm_bprintf(&bstr, "%d", info->usrlist[i]);
		if (i != info->nusrs - 1) {
			bcm_bprintf(&bstr, " ");
		}
	}
	bcm_bprintf(&bstr, "]\n");
	for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i ++) {
		usr = &info->usr[i];

		if (!(WLC_TEST_PKTENGCMD_IS_VALID(usr->flags) || print_invalid_usr)) {
			continue;
		}

		if (is_verbose) {
			bcm_bprintf(&bstr, "\t usr %2d flag %x aid %x ru %d nss %d mcs %-2d "
				"ldpc %d rspec 0x%x sch id %d pos %d ea %s scb %p\n",
				usr->idx, usr->flags, usr->aid, usr->ruidx,
				(usr->rspec & WL_RSPEC_HE_NSS_MASK) >> 4,
				usr->rspec & WL_RSPEC_HE_MCS_MASK, RSPEC_ISLDPC(usr->rspec),
				usr->rspec,
				usr->dl_schid,
				usr->dl_schpos,
				bcm_ether_ntoa(&usr->ea, eabuf),
				usr->scb);

			bcm_bprintf(&bstr, "\t dcm %d ss_alloc 0x%x dep_usrinfo 0x%x "
				"target_rssi %d || txpwr %d bfmen %d bfmidx %d nqueue %d \n",
				usr->dcm, usr->ss_alloc, usr->type_dep_usr_info, usr->target_rssi,
				usr->txpwr, usr->bfmen, usr->bfmidx, usr->nfifo);
			for (j = 0; j < usr->nfifo; j++) {
				fifo = &usr->fifo[j];
				bcm_bprintf(&bstr, "\t\t queue %2d tid %d mpdu %d len %d "
					"flags 0x%x\n",
					fifo->idx, fifo->tid, fifo->ampdu_mpdu,
					fifo->frame_len, fifo->flags);
			}
		} else {
			bcm_bprintf(&bstr, "  usr %2d aid %x ru %d nss %d mcs %-2d "
				"ldpc %d ea %s ",
				usr->idx, usr->aid, usr->ruidx,
				(usr->rspec & WL_RSPEC_HE_NSS_MASK) >> 4,
				usr->rspec & WL_RSPEC_HE_MCS_MASK,
				RSPEC_ISLDPC(usr->rspec),
				bcm_ether_ntoa(&usr->ea, eabuf));

			if (WLC_TEST_PKTENGCMD_IS_DLSCH(info->flags) ||
				WLC_TEST_PKTENGCMD_IS_ULSCH(info->flags)) {
				bcm_bprintf(&bstr, "\n");
				if (WLC_TEST_PKTENGCMD_IS_DLSCH(info->flags)) {
					bcm_bprintf(&bstr, "    DL info: sch id %d pos %d"
						" txpwr %d bfmen %d bfmidx %d\n",
						usr->dl_schid, usr->dl_schpos, usr->txpwr,
						usr->bfmen, usr->bfmidx);
				}
				if (WLC_TEST_PKTENGCMD_IS_ULSCH(info->flags)) {
					bcm_bprintf(&bstr, "    UL info: sch id %d pos %d dcm %d "
						"ss_alloc 0x%x dep_usrinfo 0x%x target_rssi %d\n",
						usr->ul_schid, usr->ul_schpos,
						usr->dcm, usr->ss_alloc, usr->type_dep_usr_info,
						usr->target_rssi);
				}
			} else {
				if (is_dl) {
					bcm_bprintf(&bstr, "txpwr %d bfmen %d bfmidx %d "
						"nqueue %d\n",
						usr->txpwr, usr->bfmen, usr->bfmidx, usr->nfifo);
					for (j = 0; j < usr->nfifo; j++) {
						fifo = &usr->fifo[j];
						bcm_bprintf(&bstr, "\t\t queue %2d tid %d mpdu %d "
							"len %d flags 0x%x\n",
							fifo->idx, fifo->tid, fifo->ampdu_mpdu,
							fifo->frame_len, fifo->flags);
					}
				} else {
					bcm_bprintf(&bstr, "dcm %d ss_alloc 0x%x dep_usrinfo 0x%x "
						"target_rssi %d\n",
						usr->dcm, usr->ss_alloc, usr->type_dep_usr_info,
						usr->target_rssi);
				}
			}

		}
	}

	return err;
}

/*
 * Function to configure pkteng params, input could be either of 3 formats:
 * format-1: opcode argc argv[0] argv[1] ... argv[argc-1]
 * format-2: opcode key val
 * format-3: opcode opaque buffer
 */
static int
wlc_test_pkteng_cmd_set(wlc_test_info_t *test, wl_pkteng_cmd_params_t *params)
{
	int err, i, j;
	uint16 val16, coremask1;
	uint8 txcore;
	uint offset;
	int aid, nss_mcs, ruidx, ampdu_mpdu;
	int usridx, qid, tid, pattern, fifoidx, endval;
	int ldpc, dcm, ss_alloc, target_rssi, type_dep_usr_info;
	int nfifo, txpwr, bfmidx, bfmen;
	uint32 frame_len;
	wlc_info_t *wlc;
	wlc_hw_info_t *wlc_hw;
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	wlc_test_pkteng_fifoinfo_t *fifo;
	wl_pkteng_t *pkteng;
	struct ether_addr ea;

	/* init value */
	wlc = test->wlc;
	wlc_hw = wlc->hw;
	info = test->pkteng_info;
	pkteng = NULL;
	usr = NULL;
	err = BCME_OK;

	/* common part for writing reg */
	offset = params->u.argint.argv[0];
	val16 = (uint16) (params->u.argint.argv[1] & 0xffff);

	switch (params->opcode) {
	case PKTENGCMD_OPCODE_PUB_ARGBUF_SET:
		break;
	case PKTENGCMD_OPCODE_PRIV_ARGINT_SHM:
		PKTENG_PARAM_CHKCNT(params->u.argint.argc, 2);
		wlc_bmac_write_shm(wlc_hw, offset, val16);
		break;

#if defined(WL_PSMX)
	case PKTENGCMD_OPCODE_PRIV_ARGINT_SHMX:
		PKTENG_PARAM_CHKCNT(params->u.argint.argc, 2);
		wlc_bmac_write_shmx(wlc_hw, offset, val16);
		break;
#endif /* WL_PSMX */

	case PKTENGCMD_OPCODE_PUB_ARGINT_SIGA:
		PKTENG_PARAM_CHKCNT(params->u.argint.argc, WLC_TEST_SIGA_SZ);
		WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
		for (i = 0; i < WLC_TEST_SIGA_SZ; i++) {
			info->siga[i] = params->u.argint.argv[i];
		}
		info->flags |= WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA;
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_PCTL:
		PKTENG_PARAM_CHKCNT(params->u.argint.argc, WLC_TEST_PHYCTL_SZ);
		WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
		for (i = 0; i < WLC_TEST_PHYCTL_SZ; i++) {
			info->phyctl[i] = params->u.argint.argv[i];
		}
		info->flags |= WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA;
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_STALIST:
		for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
			WLC_TEST_PKTENGCMD_DO_INVALID(info->usr[i].flags);
			WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			/* Invalidate the schpos and schid */
			info->usr[i].dl_schpos = -1;
			info->usr[i].dl_schid = -1;
			info->usr[i].ul_schpos = -1;
			info->usr[i].ul_schid = -1;
		}
		usridx = 0;
		for (i = 0; i < params->u.argint.argc; i++) {
			val16 = (uint16) params->u.argint.argv[i];
			if (val16 > WLC_TEST_PKTENG_MAXUSR) {
				WL_INFORM(("%s Cannot set nusrs %d > MAX %d\n",
					__FUNCTION__, val16, WLC_TEST_PKTENG_MAXUSR));
				continue;
			}
			if (!WLC_TEST_PKTENGCMD_IS_VALID(info->usr[val16].flags)) {
				info->usrlist[usridx] = val16;
				info->usr[val16].dl_schpos = usridx;
				info->usr[val16].dl_schid = 0;
				info->usr[val16].ul_schpos = usridx;
				info->usr[val16].ul_schid = 0;
				usridx++;
				info->usr[val16].flags = 0;
				WLC_TEST_PKTENGCMD_DO_VALID(info->usr[val16].flags);
			}
		}
		info->nusrs = usridx;
		WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_STACFG:
		/* opcode usr_idx macaddr0:31 macaadr32:47 aid RU_idx nss/mcs
		 * ldpc dcm ss_alloc target_rssi type-def_info
		 */
		ldpc = dcm = ss_alloc = target_rssi = type_dep_usr_info = usridx =
		aid = nss_mcs = ruidx = -1;

		switch (params->u.argint.argc) {
		case 11:
			type_dep_usr_info = params->u.argint.argv[10];
			/* fall through */
		case 10:
			target_rssi = params->u.argint.argv[9];
			/* fall through */
		case 9:
			ss_alloc = params->u.argint.argv[8];
			/* fall through */
		case 8:
			dcm = params->u.argint.argv[7];
			/* fall through */
		case 7:
			ldpc = params->u.argint.argv[6];
			/* fall through */
		case 6:
			nss_mcs = params->u.argint.argv[5];
			/* fall through */
		case 5:
			ruidx = params->u.argint.argv[4];
			/* fall through */
		case 4:
			aid = params->u.argint.argv[3];
			/* fall through */
		case 3:
			ea.octet[0] = params->u.argint.argv[1] & 0xFF;
			ea.octet[1] = (params->u.argint.argv[1] >> 8) & 0xFF;
			ea.octet[2] = (params->u.argint.argv[1] >> 16) & 0xFF;
			ea.octet[3] = (params->u.argint.argv[1] >> 24) & 0xFF;
			ea.octet[4] = params->u.argint.argv[2] & 0xFF;
			ea.octet[5] = (params->u.argint.argv[2] >> 8) & 0xFF;
			/* fall through */
		case 2:
			/* this is macaddr part 1 */
		case 1:
			/* usr id */
			usridx  = params->u.argint.argv[0];
			break;
		case 0:
			/* no arg */
		default:
			err =  BCME_BADARG;
			break;
		}

		if (err == BCME_BADARG) {
			break;
		}

		endval = 0;
		if (usridx == -1) {
			i = 0;
			endval = WLC_TEST_PKTENG_MAXUSR;
		} else if (usridx > WLC_TEST_PKTENG_MAXUSR) {
			err = BCME_BADARG;
			break;
		} else {
			i = usridx;
			endval = usridx + 1;
		}

		while (i < endval) {
			usr = &info->usr[i];

			usr->type_dep_usr_info = (type_dep_usr_info != -1) ?
				(uint8) type_dep_usr_info : usr->type_dep_usr_info;
			usr->target_rssi = (target_rssi != -1) ?
				(uint8) target_rssi : usr->target_rssi;
			usr->ss_alloc = (ss_alloc != -1) ? (uint8) ss_alloc : usr->ss_alloc;
			usr->dcm = (dcm != -1) ? (uint8) dcm : usr->dcm;
			if (ldpc != -1) {
				usr->rspec &= ~WL_RSPEC_LDPC;
				usr->rspec |= (ldpc == 1 ? WL_RSPEC_LDPC : 0);
			}
			if (nss_mcs != -1) {
				usr->rspec &= ~(WL_RSPEC_HE_NSS_MASK |
				WL_RSPEC_HE_MCS_MASK);
				usr->rspec |= (nss_mcs & (WL_RSPEC_HE_NSS_MASK |
				WL_RSPEC_HE_MCS_MASK));
				usr->rspec |= WL_RSPEC_ENCODE_HE;
			}
			if (aid != -1) {
				usr->aid = aid;
				if (usridx == -1) {
					usr->aid = aid + i;
				}
			}
			if (ruidx != -1) {
				usr->ruidx = ruidx & 0xFFF;
				if (usridx == -1) {
					usr->ruidx = (ruidx + 1) & 0xFFF;
				}
			}
			if (usridx == -1) {
				ea.octet[0]++;
			}
			memcpy(&usr->ea, &ea, sizeof(ea));
			WLC_TEST_PKTENGCMD_DO_UPDATE(usr->flags);
			i++;
		}
		/* set the top-level updated flags */
		WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_STACFG1:
		/* opcode usr_idx macaddr0:31 macaadr32:47 aid RU_idx nss/mcs
		 * ldpc dcm ss_alloc target_rssi type-def_info
		 */

		nfifo = txpwr = bfmidx = bfmen = -1;

		switch (params->u.argint.argc) {
		case 5:
			nfifo = params->u.argint.argv[4];
			/* fall through */
		case 4:
			bfmidx = params->u.argint.argv[3];
			/* fall through */
		case 3:
			bfmen = params->u.argint.argv[2];
			/* fall through */
		case 2:
			txpwr = params->u.argint.argv[1];
			/* fall through */
		case 1:
			/* usr id */
			usridx  = params->u.argint.argv[0];
			break;
		case 0:
			/* no arg */
		default:
			err =  BCME_BADARG;
			break;
		}

		if (err == BCME_BADARG) {
			break;
		}

		endval = 0;
		if (usridx == -1) {
			i = 0;
			endval = WLC_TEST_PKTENG_MAXUSR;
		} else if (usridx > WLC_TEST_PKTENG_MAXUSR) {
			err = BCME_BADARG;
			break;
		} else {
			i = usridx;
			endval = usridx + 1;
		}

		while (i < endval) {
			usr = &info->usr[i];

			if (nfifo > 0 && nfifo <= WLC_TEST_PKTENG_MAXFIFO) {
				usr->nfifo = nfifo;
			}
			usr->bfmen = (bfmen != -1) ? (uint8) bfmen : usr->bfmen;
			usr->bfmidx = (bfmidx != -1) ? (uint8) bfmidx : usr->bfmidx;
			usr->txpwr = (txpwr != -1) ? (uint8) txpwr : usr->txpwr;
			WLC_TEST_PKTENGCMD_DO_UPDATE(usr->flags);
			i++;
		}
		/* set the top-level updated flags */
		WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_TXQCFG:
		usridx = tid = qid = pattern = frame_len = ampdu_mpdu = -1;
		/* make default fifo idx to 0 */
		fifoidx = 0;

		/* usridx fifoidx ampdu_mpdu frame_len pattern qid tid */
		switch (params->u.argint.argc) {
		case 7:
			tid		= params->u.argint.argv[6];
		case 6:
			qid		= params->u.argint.argv[5];
		case 5:
			pattern		= params->u.argint.argv[4];
		case 4:
			frame_len	= params->u.argint.argv[3];
		case 3:
			ampdu_mpdu	= params->u.argint.argv[2];
		case 2:
			fifoidx		= params->u.argint.argv[1];
		case 1:
			usridx		= params->u.argint.argv[0];
			break;
		case 0:
		default:
			err = BCME_BADARG;
			break;
		}

		if (qid > WLC_TEST_PKTENG_MAXQID) {
			err = BCME_BADARG;
		}

		if (err == BCME_BADARG) {
			break;
		}

		for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
			if (usridx != -1 && usridx != i) {
				continue;
			}
			usr = &info->usr[i];
			for (j = 0; j < WLC_TEST_PKTENG_MAXFIFO; j++) {
				if (fifoidx != -1 && fifoidx != j) {
					continue;
				}
				fifo = &usr->fifo[j];
				if (fifoidx != -1) {
					if (qid != -1) {
						fifo->idx = qid;
					}
					if (tid != -1) {
						fifo->tid = tid;
					}
				}
				if (pattern != -1) {
					fifo->pattern = pattern;
				}
				if (ampdu_mpdu != -1) {
					fifo->ampdu_mpdu = ampdu_mpdu;
				}
				if (frame_len != -1) {
					fifo->frame_len = frame_len;
				}
				fifo->flags |= 1; /* set is_set flag */
				WLC_TEST_PKTENGCMD_DO_UPDATE(fifo->flags);
			}
		}
		/* set the top-level updated flags */
		WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
		break;

	case PKTENGCMD_OPCODE_PUB_ARGINT_RESET:
		err = wlc_test_pkteng_cmd_init(test);
		info->flags |= WLC_TEST_PKTENGCMD_FLAG_EN;
		break;
	case PKTENGCMD_OPCODE_PUB_ARGINT_COMMIT:
		if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA) == 0 &&
			(info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO) == 0) {
			err = wlc_test_pkteng_cmd_use_rspec(test);
		}

		for (i = 0; i < info->nusrs; i++) {
			err = wlc_test_pkteng_addsta(test, info->usrlist[i]);
		}
		if (info->mode == WL_PKTENG_CMD_STRT_DL_CMD) {
#if defined(WL_PSMX)
			/* Disable OMU flow control */
			wlc_bmac_mhf(wlc->hw, MXHF0, MXHF0_FCTLDIS, MXHF0_FCTLDIS, WLC_BAND_AUTO);
			/* set the shmx block */
			if ((err = wlc_test_pkteng_set_shmx_block(test)) != BCME_OK) {
				break;
			}
			wlc_musched_set_dlpolicy(wlc->musched, MUSCHED_DL_POLICY_FIXED);
#else /* defined(WL_PSMX) */
			err = BCME_UNSUPPORTED;
			break;
#endif /* defined(WL_PSMX) */
		} else if (info->mode == WL_PKTENG_CMD_STRT_UL_CMD) {
			if ((err = wlc_test_pkteng_prepare_for_trigger_frame(test, TXTRIG))
				!= BCME_OK) {
				break;
			}
		}  else if (info->mode == WL_PKTENG_CMD_STRT_ULTX_CMD) {
			if ((err = wlc_test_pkteng_prepare_for_trigger_frame(test, RXTRIG))
				!= BCME_OK) {
				break;
			}
		} else {
			err = BCME_ERROR;
			break;
		}

		txcore = (info->phyctl[2] & 0xf);
		coremask1 = wlc_read_shm(wlc, M_COREMASK_BFM1(wlc));
		coremask1 = (txcore << 8) | (coremask1 & 0xff);
		wlc_write_shm(wlc, M_COREMASK_BFM1(wlc), coremask1);

		WLC_TEST_PKTENGCMD_DO_COMMIT(info->flags);
		break;
	case PKTENGCMD_OPCODE_PUB_ARGINT_DELSTA:
		if (params->u.argint.argc == 0) {
		/* Delete all usrs */
			for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
				err = wlc_test_pkteng_delsta(test, i);
			}
			info->nusrs = 0;
		} else {
		/* Delte given the usr list */
			for (i = 0; i < params->u.argint.argc; i++) {
				err = wlc_test_pkteng_delsta(test, params->u.argint.argv[i]);
			}
			/* re-generate the usr list */
			j = 0;
			for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
				usr = &info->usr[i];
				if (WLC_TEST_PKTENGCMD_IS_VALID(usr->flags)) {
					info->usrlist[j++] = i;
				}
			}
			info->nusrs = j;
		}
		break;
	case PKTENGCMD_OPCODE_PUB_ARGINT_START:

		if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_COMMITED) == 0 ||
			(info->flags & WLC_TEST_PKTENGCMD_FLAG_EN) == 0) {
			WL_ERROR(("%s Cannot start pkteng flags 0x%x\n",
				__FUNCTION__, info->flags));
			err = BCME_ERROR;
			break;
		}

		if (params->u.argint.argc == 0) {
			/* make the default start pkteng as DL ofdma */
			info->mode = WL_PKTENG_CMD_STRT_DL_CMD;
		} else {
			info->mode = params->u.argint.argv[0];
		}

		if (info->mode == WL_PKTENG_CMD_STRT_UL_CMD) {
			switch (params->u.argint.argc) {
			case 2:
				info->nframes = params->u.argint.argv[1];
				/* fall through */
			case 1:
				/* this is to get mode, we have got it before */
				/* fall through */
				break;
			default:
				break;
			}
		} else if (info->mode == WL_PKTENG_CMD_STRT_ULTX_CMD) {
			switch (params->u.argint.argc) {
			case 3:
				info->ifs = params->u.argint.argv[2];
				/* fall through */
			case 2:
				info->nframes = params->u.argint.argv[1];
				/* fall through */
			case 1:
				/* this is to get mode, we have got it before */
				/* fall through */
				break;
			default:
				break;
			}
		} else {
			/* all else goes to DL */
			switch (params->u.argint.argc) {
			case 3:
				info->ifs = params->u.argint.argv[2];
				/* fall through */
			case 2:
				info->nframes = params->u.argint.argv[1];
				/* fall through */
			case 1:
				/* this is to get mode, we have got it before */
				/* fall through */
				break;
			default:
				break;
			}
		}

		if (info->mode == WL_PKTENG_CMD_STRT_UL_CMD) {
			wlc_bmac_write_shm(wlc_hw, M_TXTRIG_FLAG(wlc), 1);
			wlc_bmac_write_shm(wlc_hw, M_TXTRIG_NUM(wlc), info->nframes);
		} else {
			/* all else goes to DL ofdma */
			err = wlc_test_pkteng_ofdma_dl_tx(test, info->nframes, info->ifs);
		}
		break;
	case PKTENGCMD_OPCODE_PUB_ARGINT_PAUSE:
	case PKTENGCMD_OPCODE_PUB_ARGINT_STOP:

		if ((pkteng = MALLOCZ(wlc->osh, sizeof(wl_pkteng_t))) == NULL) {
			err = BCME_ERROR;
			break;
		}
		pkteng->flags = WL_PKTENG_PER_TX_STOP;
		err = wlc_bmac_pkteng(wlc->hw, pkteng, NULL, 0);

		if (pkteng != NULL) {
			MFREE(wlc->osh, pkteng, sizeof(wl_pkteng_t));
			pkteng = NULL;
		}
		/* restore Mute State after pkteng is done */
		if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec)) {
			wlc_mute(wlc, ON, 0);
		}

		if (err != BCME_OK) {
			break;
		}

		if (params->opcode == PKTENGCMD_OPCODE_PUB_ARGINT_STOP) {
			/* del sta (SCB) */
			for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
				err = wlc_test_pkteng_delsta(test, i);
			}
			/* Reset num of stas that effectively resets sta list */
			info->nusrs = 0;
			/* Reset flags */
			info->flags = WLC_TEST_PKTENGCMD_FLAG_RESET;
			/* Reset mode */
			info->mode = 0;
		}
		info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_RUN;

		break;
	case PKTENGCMD_OPCODE_PUB_ARGKEY_CFG:
		wlc_test_pkteng_cmd_setkey(test, params);
		break;

#if defined(WL_PSMR1)
	case PKTENGCMD_OPCODE_PRIV_ARGINT_SHM1:
		if (params->u.argint.argc < 2) {
			err = BCME_BADARG;
			break;
		}
		wlc_bmac_write_shm1(wlc_hw, offset, val16);
		break;
#endif /* WL_PSMR1 */
	default:
		err = BCME_BADARG;
		break;
	}

	return err;
}

/* function to initilize pkteng_cmd */
static int
wlc_test_pkteng_cmd_init(wlc_test_info_t *test)
{
	int i, j;
	int ret;
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	wlc_test_pkteng_fifoinfo_t *fifo;
	wlc_info_t *wlc;
	ratespec_t rspec;
	uint8 sgi_tx;
	uint8 hegi;

	ret = BCME_OK;
	wlc = test->wlc;
	info = test->pkteng_info;
	memset(info, 0, sizeof(wlc_test_pkteng_fifoinfo_t));
	info->ifs = 60;
	info->mode = WL_PKTENG_CMD_STRT_DL_CMD;
	info->nusrs = WLC_TEST_PKTENG_DFLT_NUSR;
	info->max_dur = 5000;
	info->min_dur = AMPDU_DENSITY_8_US;

	/* default values for ul */
	info->txtrig_type = HE_TRIG_TYPE_BASIC_FRM;
	info->ul_lsig_len = 0;
	info->ul_more_tf = 0;
	info->ul_cs_req	= 0;
	info->ul_bw = 2;
	info->ul_cp_ltftype = 1;
	info->ul_mu_mimo_ltf = 0;
	info->ul_numheltf = 2;
	info->ul_stbc = 0;
	info->ul_ldpc_extsym = 0;
	info->ul_ap_txpwr = 0;
	info->ul_afact = 0;
	info->ul_pe_disambig = 0;
	info->ul_spat_reuse = 0;
	info->ul_doppler = 0;
	info->ul_hesiga2_rsvd = -1;
	info->ul_rsvd = 0;
	info->nbyte_pad = 12; /* 16 usec == 12 bytes at 6Mbps */

	rspec = wlc->band->rspec_override;
	if ((rspec & WL_RSPEC_OVERRIDE_RATE) == 0) {
		rspec = WLC_TEST_PKTENGCMD_DFLT_RSPEC;
		/* Update chanBW from chanspec */
		switch (CHSPEC_BW(wlc->chanspec)) {
			case WL_CHANSPEC_BW_20:
				rspec |= WL_RSPEC_BW_20MHZ;
				break;
			case WL_CHANSPEC_BW_40:
				rspec |= WL_RSPEC_BW_40MHZ;
				break;
			case WL_CHANSPEC_BW_80:
				rspec |= WL_RSPEC_BW_80MHZ;
				break;
			case WL_CHANSPEC_BW_160:
				rspec |= WL_RSPEC_BW_160MHZ;
				break;
			default:
				rspec |= WL_RSPEC_BW_20MHZ;
				break;
		}
		sgi_tx = WLC_HT_GET_SGI_TX(wlc->hti);
		rspec &= ~WL_RSPEC_GI_MASK;
		switch (sgi_tx) {
		case ON : /* 1 */
		case WL_HEGI_VAL(WL_RSPEC_HE_2x_LTF_GI_0_8us): /* 3 */
		case WL_HEGI_VAL(WL_RSPEC_HE_2x_LTF_GI_1_6us): /* 4 */
		case WL_HEGI_VAL(WL_RSPEC_HE_4x_LTF_GI_3_2us): /* 5 */
			if (sgi_tx == ON) {
				hegi = WL_RSPEC_HE_2x_LTF_GI_0_8us;
			} else {
				/* get hegi value bye subtract the delta */
				hegi = sgi_tx - WL_SGI_HEGI_DELTA;
			}
			rspec |= HE_GI_TO_RSPEC(hegi);
			break;
		case OFF: /* 0 */
		default: /* AUTO */
			/* 43684 doesn't support 1x_LTF_GI_0_8us */
			rspec |= HE_GI_TO_RSPEC(WL_RSPEC_HE_2x_LTF_GI_0_8us);
			break;
		}
	}
	info->rspec = rspec;

	for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
		usr = &info->usr[i];
		usr->idx = i;
		usr->scb = NULL;
		usr->dl_schpos = i;
		usr->dl_schid = 0;
		usr->ul_schpos = i;
		usr->ul_schid = 0;
		usr->flags = 0;
		usr->aid = 0xe20 + i;
		usr->ruidx = 37 + i;
		usr->rspec = info->rspec;
		memcpy(&usr->ea, &pkteng_base_addr, ETHER_ADDR_LEN);
		usr->ea.octet[ETHER_ADDR_LEN-1] = i;

		if (i < WLC_TEST_PKTENG_DFLT_NUSR) {
			info->usrlist[i] = i;
		}

		if (usr->scb != NULL) {
			wlc_scbfree(wlc, usr->scb);
			usr->scb = NULL;
		}

		usr->nfifo = WLC_TEST_PKTENG_DFLT_NFIFO;
		for (j = 0; j < WLC_TEST_PKTENG_MAXFIFO; j++) {
			fifo = &usr->fifo[j];
			fifo->idx = TX_FIFO_HE_MU_START + i * WLC_TEST_PKTENG_MAXFIFO + j;
			fifo->tid = j * 2;
			fifo->pattern = 0;
			fifo->ampdu_mpdu = 64;
			fifo->frame_len = 200;
		}
	}

	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA) == 0 &&
	(info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO) == 0) {
		ret = wlc_test_pkteng_cmd_use_rspec(test);
	}

	return ret;
}

/* function to convert rspec to siga/phyctl */
static int
wlc_test_pkteng_cmd_use_rspec(wlc_test_info_t *test)
{
	int ret;
	uint8 txcore;
	wlc_info_t *wlc;
	wlc_test_pkteng_info_t *info;
	ratespec_t rspec;

	ret = BCME_OK;
	info = test->pkteng_info;
	rspec = info->rspec;
	wlc = test->wlc;

	/* init siga based on rspec and chanspec */
	info->phyctl[0] = 0x3b54;
	info->phyctl[1] = (D11REV_GE(wlc->hw->corerev, 129) ? 0 : 0xff)
		<< D11_REV128_PHYCTL1_POSB_SHIFT;
	info->phyctl[2] = 0xf;
	info->phyctl[3] = 0;
	info->phyctl[4] = 0;

	memset(info->siga, 0, sizeof(info->siga));

	if ((rspec & WL_RSPEC_OVERRIDE_RATE) != 0)  {
		/* XXX: siga can be computed using API wlc_compute_plcp when HEMU
		 * format is supported
		 wlc_compute_plcp(wlc, wlc->cfg, rspec, 6, 0, plcp);
		 info->siga[0] = (plcp[1] << 8) | plcp[0];
		 info->siga[1] = (plcp[3] << 8) | plcp[2];
		 info->siga[2] = (plcp[5] << 8) | plcp[4];
		 */
		switch (RSPEC_BW(rspec)) {
			case WL_RSPEC_BW_20MHZ:
				info->ul_bw = D11_REV128_BW_20MHZ;
				break;
			case WL_RSPEC_BW_40MHZ:
				info->ul_bw = D11_REV128_BW_40MHZ;
				info->siga[0] |= (1 << 15);
				info->phyctl[1] |= D11_REV128_BW_40MHZ;
				break;
			case WL_RSPEC_BW_80MHZ:
				info->ul_bw = D11_REV128_BW_80MHZ;
				info->siga[1] |= 1;
				info->phyctl[1] |= D11_REV128_BW_80MHZ;
				break;
			case WL_RSPEC_BW_160MHZ:
				/* 160 / 80+80 Mhz */
				info->ul_bw = D11_REV128_BW_160MHZ;
				info->siga[0] |= (1 << 15);
				info->siga[1] |= 1;
				info->phyctl[1] |= D11_REV128_BW_160MHZ;
				break;
			default:
				ASSERT(0);
				break;
		}
		/* Reset he_ltf_gi field */
		info->siga[1] &= ~(3 << (23-16));
		info->siga[1] |= (RSPEC_HE_LTF_GI(rspec) << (23-16));
		/* set reserved bit */
		info->siga[2] |= 0x2;
		/* set up num HELTF bits 2-4 */
		info->siga[2] &= ~(7 << 2);
		info->siga[2] |= ((info->ul_numheltf) << 2);

		info->phyctl[0] &= ~0xFF00;
		info->phyctl[0] |= (rspec & (WL_RSPEC_HE_MCS_MASK | WL_RSPEC_HE_NSS_MASK)) << 8;

		/* get tx chain */
		txcore = wlc_stf_txcore_get(wlc, rspec);
		info->phyctl[2] = txcore;

		/* for uplink */
		info->ul_cp_ltftype = RSPEC_HE_LTF_GI(info->rspec);
	}
	return ret;
}

#if defined(WL_PSMX)
/* Write a list of values to a starting shmx address continuously */
static uint
wlc_test_incremental_write_shmx(wlc_hw_info_t *wlc_hw,	uint start_offset,
	uint16 *val16, int num)
{
	int i;
	uint offset;

	offset = start_offset;
	for (i = 0; i < num; i++) {
		wlc_bmac_write_shmx(wlc_hw, offset, *val16);
		offset += 2;
		val16 ++;
	}
	return offset;
}

static int
wlc_test_pkteng_set_shmx_block(wlc_test_info_t *test)
{
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;

	wlc_info_t *wlc;
	wlc_hw_info_t *wlc_hw;
	int ret, i;
	uint offset;
	int8 val8 = 0;
	uint16 val16;
	uint8 ruidx;
	uint8 ldpc, nss, mcs;

	wlc = test->wlc;
	wlc_hw = wlc->hw;
	ret = BCME_OK;

	info = test->pkteng_info;

	/* set state */
	offset = MX_HEMSCH0_BLK(wlc);
	wlc_bmac_write_shmx(wlc_hw, offset, info->state);
	/* set siga */
	offset = MX_HEMSCH0_SIGA(wlc);
	offset = wlc_test_incremental_write_shmx(wlc_hw, offset, &info->siga[0], WLC_TEST_SIGA_SZ);
	/* set phyctrl */
	offset = MX_HEMSCH0_PCTL0(wlc);
	offset = wlc_test_incremental_write_shmx(wlc_hw, offset, &info->phyctl[0],
		WLC_TEST_PHYCTL_SZ);
	/* set num of users */
	offset = MX_HEMSCH0_N(wlc);
	wlc_bmac_write_shmx(wlc_hw, offset, info->nusrs);
	/* set usrs */
	for (i = 0; i < info->nusrs; i++) {
		usr = &info->usr[info->usrlist[i]];
		if (D11REV_GE(wlc->pub->corerev, 128) && usr->scb != NULL) {
			val8 = wlc_txbf_get_mu_txvidx(wlc, usr->scb);
			if (val8 != BCME_NOTFOUND)
				usr->bfmidx = (uint16) val8;
		}

		offset = MX_HEMSCH0_USR(wlc) + HEMSCH_USRSZ * usr->dl_schpos;
		val16 = usr->txpwr << 8 | (usr->bfmen << 7) | usr->bfmidx;
		wlc_bmac_write_shmx(wlc_hw, offset, val16);
		offset += 2;

		ldpc = RSPEC_ISLDPC(usr->rspec);
		nss = (usr->rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;
		nss--;
		mcs = (usr->rspec & WL_RSPEC_HE_MCS_MASK);

		ruidx = usr->ruidx;

#ifdef WL11AX
		/* if station is not capable of 242 tone in 20 mhz then it cant use
		 * ru-idx 61, 62, 63, or 63. They will need to be remapped to 104 tone
		 * ru index. This is necessary for some WFA testbed STAs
		 */
		if ((ruidx >= 61) && (ruidx <= 64) && usr->scb) {
			if (!(wlc_he_get_scb_flags(wlc->hei, usr->scb) & SCB_HE_DL_242TONE)) {
				ruidx -= 61;
				ruidx *= 2;
				ruidx += 54; /* 106 tone base */
			}
		}
#endif /* WL11AX */

		val16 = (ldpc << 15) | (nss << 12) | (mcs << 8) | ruidx;

		wlc_bmac_write_shmx(wlc_hw, offset, val16);
	}
	return ret;
}
#endif /* defined(WL_PSMX) */

static int
wlc_test_pkteng_addsta(wlc_test_info_t *test, int usridx)
{
	struct scb *scb;
	char eabuf[ETHER_ADDR_STR_LEN];
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	he_cap_ie_t he_cap;
	wlc_info_t *wlc;
	ratesel_txparams_t ratesel_rates;
	int ret;

	ret = BCME_OK;
	wlc = test->wlc;
	info = test->pkteng_info;

	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(he_cap);

	if (usridx < 0 || usridx >= WLC_TEST_PKTENG_MAXUSR) {
		WL_ERROR(("%s Cannot add usr idx %d\n", __FUNCTION__, usridx));
		return BCME_ERROR;
	}

	usr = &info->usr[usridx];

	if (!WLC_TEST_PKTENGCMD_IS_VALID(usr->flags)) {
		WL_ERROR(("%s Cannot add scb for sta %d ea %s due to invalid flags 0x%x\n",
			__FUNCTION__, usridx, bcm_ether_ntoa(&usr->ea, eabuf), usr->flags));
		return BCME_ERROR;
	}

	if (usr->scb != NULL) {
		if ((ret = wlc_test_pkteng_delsta(test, usridx)) != BCME_OK) {
			WL_ERROR(("%s Cannot del scb for sta %d ea %s\n",
				__FUNCTION__, usridx, bcm_ether_ntoa(&usr->ea, eabuf)));
			return BCME_ERROR;
		}
	}
	scb = wlc_scblookup(wlc, wlc->cfg, &usr->ea);
	if ((scb == NULL) || (SCB_BSSCFG(scb) == NULL)) {
		WL_ERROR(("Cannot create scb for sta %d ea %s\n", usridx,
		bcm_ether_ntoa(&usr->ea, eabuf)));
		return BCME_ERROR;
	}
	SCB_BSSCFG(scb)->AID = usr->aid;
#ifdef WL11AX
	wlc_he_update_scb_state(wlc->hei, wlc->band->bandtype,
		scb, &he_cap, 0);
	SCB_SET_HE_CAP(scb);
#endif /* WL11AX */
	wlc_scb_setstatebit(wlc, scb, AUTHENTICATED);
	wlc_scb_setstatebit(wlc, scb, ASSOCIATED);
	SCB_VHTMU_ENABLE(scb);
	scb->aid = usr->aid;

	usr->scb = scb;

	usr->flags = 0;
	WLC_TEST_PKTENGCMD_DO_VALID(usr->flags);
	WLC_TEST_PKTENGCMD_DO_COMMIT(usr->flags);

	/* configure amdpu */
	scb_ampdu_update_config(wlc->ampdu_tx, scb);

	ASSERT(RATELINKMEM_ENAB(wlc->pub) == TRUE);
	wlc_ratelinkmem_update_link_entry(wlc, scb);

	ratesel_rates.num = 1;
	ratesel_rates.ac = 0;
	ratesel_rates.rspec[0] = usr->rspec;
	wlc_ratelinkmem_update_rate_entry(wlc, scb, &ratesel_rates, 0);

	wlc_scbmusched_set_dlofdma(wlc->musched, scb, TRUE);
	wlc_scbmusched_set_dlschpos(wlc->musched, scb, usr->dl_schpos);
	return ret;
}

static int
wlc_test_pkteng_delsta(wlc_test_info_t *test, int usridx)
{
	wlc_info_t *wlc;
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	struct scb *scb;
	int ret;

	wlc = test->wlc;
	info = test->pkteng_info;
	ret = BCME_OK;

	if (usridx < 0 || usridx >= WLC_TEST_PKTENG_MAXUSR) {
		WL_ERROR(("%s Cannot del usr idx %d\n", __FUNCTION__, usridx));
		return BCME_ERROR;
	}

	usr = &info->usr[usridx];

	WLC_TEST_PKTENGCMD_DO_INVALID(usr->flags);

	scb = usr->scb;
	if (scb != NULL) {
		wlc_scbfree(wlc, scb);
		usr->scb = NULL;
	}
	return ret;
}

static uint16
wlc_test_pkteng_get_default_ruidx(int8 bw, uint8 nusrs)
{
	uint16 ruidx;

	switch (bw) {
	case D11_REV128_BW_20MHZ:
		if (nusrs == 1) {
			ruidx = 61;
		} else if (nusrs == 2) {
			ruidx = 53;
		} else {
			ruidx = 37;
		}
		break;
	case D11_REV128_BW_40MHZ:
		if (nusrs == 1) {
			ruidx = 65;
		} else if (nusrs == 2) {
			ruidx = 61;
		} else {
			ruidx = 53;
		}
		break;
	default:
		if (nusrs == 1) {
			ruidx = 67;
		} else if (nusrs == 2) {
			ruidx = 65;
		} else {
			ruidx = 61;
		}
		break;
	}
	return ruidx;
}

#define MAX_LSIGLEN		4095
#define PREAMBLE_SIZE		40
#define LSIG_LTF_DIVIDER	10

static uint32
wlc_get_valid_lsiglen(uint32 lsiglen_in, uint16 ul_cp_ltftype, uint16 ul_numheltf)
{
	uint32 value;	/* intermediate value used between steps */
	uint32 t_HELTFxN_LTF;
	uint32 t_T_SYM;

	value = lsiglen_in > MAX_LSIGLEN ? MAX_LSIGLEN : lsiglen_in;

	value = (value + 5) / 3;
	/* value = cal_LSIG_LENGTH_s1 + (PREAMBLE_SIZE / 4); */
	value -= (PREAMBLE_SIZE / 4);
	/* value = cal_LSIG_LENGTH_s1; */
	value *= (LSIG_LTF_DIVIDER * 4);
	/* value = (t_HELTFxN_LTF + (t_N_SYM * t_T_SYM) - 10); */
	/* NOTE: this is incorrect, the -10 originates from division with
	 * rounding. The contra version with multiplication does not need the
	 * rounding factor. So instead of -10 it should be - 40, so add 40.
	 */
	value += 40;
	/* value = t_HELTFxN_LTF + (t_N_SYM * t_T_SYM); */

	t_HELTFxN_LTF = ul_numheltf * 2;
	if (t_HELTFxN_LTF == 0) {
		t_HELTFxN_LTF = 1;
	}
	if (ul_cp_ltftype == 0) {
		t_HELTFxN_LTF *= 48;
		t_T_SYM = 144;
	} else if (ul_cp_ltftype == 1) {
		t_HELTFxN_LTF *= 80;
		t_T_SYM = 144;
	} else {
		t_HELTFxN_LTF *= 160;
		t_T_SYM = 160;
	}
	value -= t_HELTFxN_LTF;
	/* value = t_N_SYM * t_T_SYM; */
	value /= t_T_SYM; /* Rounded down. Now it is done. */
	/* Number of sumbols desired known. Recalculate back to get to a
	 * valid lsig_len (which it may have been in the first place !! )
	 */

	value *= t_T_SYM;
	value += t_HELTFxN_LTF;
	value -= 10;
	value += (LSIG_LTF_DIVIDER - 1);
	value /= (LSIG_LTF_DIVIDER * 4);
	value += (PREAMBLE_SIZE / 4);
	value = (value * 3) - 5;

	return value;
}

/* Function to prepare TX/RX trigger frame and write it to shm */
static int
wlc_test_pkteng_prepare_for_trigger_frame(wlc_test_info_t *test, triginfo_txrx_t triginfo_txrx)
{
	wlc_info_t *wlc;
	wlc_hw_info_t *wlc_hw;
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;

	/* common info structure */
	he_trig_cmninfo_set_t *cmn_info;
	/* user info structure */
	uint8 *usr_info, *per_usr_info;
	int usr_info_sz, perusr_info_sz;

	int ret, i, nshm;
	uint offset;
	uint offset_srxctl = 0;
	int8 rspec_bw, bw, cp_ltf_type, num_heltf;
	uint16 lsig_len, trigfrm_len, val16;
	struct ether_addr *mac_addr;
	uint16 txtrig_flag;
	uint8 nusrs;
	uint16 ruidx;

	wlc = test->wlc;
	info = test->pkteng_info;
	wlc_hw = wlc->hw;
	per_usr_info = usr_info = NULL;
	perusr_info_sz = usr_info_sz = 0;
	cmn_info = NULL;
	txtrig_flag = 0;

	ret = BCME_OK;

	if ((cmn_info = MALLOCZ(wlc->osh,
	sizeof(he_trig_cmninfo_set_t))) == NULL) {
		WL_ERROR(("wl%d: %s: cmn_info, out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		ret = BCME_ERROR;
		goto exit;
	}

	if (info->txtrig_type ==  HE_TRIG_TYPE_BASIC_FRM) {
		perusr_info_sz = HE_TRIG_USRINFO_TYPEDEP_SZ;
	} else if (info->txtrig_type == HE_TRIG_TYPE_BSR_FRM) {
		perusr_info_sz = HE_TRIG_USRINFO_SZ;
	} else {
		/* not supported trigger type */
		WL_ERROR(("wl%d: %s: trigger type %d not supported\n",
			wlc->pub->unit, __FUNCTION__, info->txtrig_type));
		goto exit;
	}
	nusrs = 0;
	for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i ++) {
		usr = &info->usr[i];

		if (!(WLC_TEST_PKTENGCMD_IS_VALID(usr->flags))) {
			continue;
		}
		if (WLC_TEST_PKTENGCMD_IS_ULDISABLED(usr->flags)) {
			continue;
		}
		nusrs++;
	}
	if (nusrs == 0) {
		goto exit;
	}

	usr_info_sz = perusr_info_sz * nusrs;

	if ((usr_info = MALLOCZ(wlc->osh, usr_info_sz)) == NULL) {
		WL_ERROR(("wl%d: %s: type %d usr_info_t, out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, info->txtrig_type, MALLOCED(wlc->osh)));
		ret = BCME_ERROR;
		goto exit;
	}
	per_usr_info = usr_info;

	if (triginfo_txrx == TXTRIG) {
		/* Write the trigger frame mac header */
		offset = M_TXTRIG_FRAME(wlc);
		wlc_bmac_write_shm(wlc_hw, offset + 0, 0x24);
		/* DUR */
		wlc_bmac_write_shm(wlc_hw, offset + 1*2, 0);
		/* A1 */
		for (i = 0; i < 3; i++) {
			wlc_bmac_write_shm(wlc_hw, offset + (2+i)*2, -1);
		}
		/* A2 */
		mac_addr = &wlc_hw->etheraddr;
		for (i = 0; i < 3; i++) {
			val16 = ((mac_addr->octet[2*i+1]) << 8) | mac_addr->octet[2*i];
			wlc_bmac_write_shm(wlc_hw, offset + (5+i)*2, val16);
		}
	}

	rspec_bw = (info->rspec & WL_RSPEC_BW_MASK) >> WL_RSPEC_BW_SHIFT;
	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO) != 0) {
		bw = info->ul_bw;
		cp_ltf_type = info->ul_cp_ltftype;
		num_heltf = info->ul_numheltf;
	} else {
		/* get the info from rspec */
		info->ul_bw = bw = rspec_bw - 1;
		info->ul_cp_ltftype = cp_ltf_type = RSPEC_HE_LTF_GI(info->rspec);
		num_heltf = info->ul_numheltf;
	}

	lsig_len = info->ul_lsig_len ? info->ul_lsig_len : ((CEIL((
		info->max_dur - HE_T_LEG_PREAMBLE), HE_T_LEG_SYMB)* HE_N_LEG_SYM) -3 - 2);
	lsig_len = wlc_get_valid_lsiglen(lsig_len, cp_ltf_type,	num_heltf);

	if (triginfo_txrx == TXTRIG) {
		offset_srxctl = M_TXTRIG_SRXCTL(wlc);

		/* Write start byte offset X | nusrs */
		val16 = wlc_read_shm(wlc, offset_srxctl);
		val16 &= 0xff00; /* zero out nusrs */
		val16 |= nusrs;  /* update nusrs */
		wlc_write_shm(wlc, offset_srxctl, val16);

		/* tell SMC number of bytes in per_user_info at SMCCTL hdr byte offset 5 */
		offset_srxctl = M_TXTRIG_SRXCTL(wlc) + 4;
		val16 = wlc_read_shm(wlc, offset_srxctl);
		val16 &= 0x00ff;
		val16 |= (perusr_info_sz << 8);  /* update per user info size */
		wlc_write_shm(wlc, offset_srxctl, val16);
	}

	/* basic trigger */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_FRMTYPE_INDX,
		HE_TRIG_CMNINFO_FRMTYPE_FSZ, info->txtrig_type);
	/* L-sig length */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_LSIGLEN_INDX,
		HE_TRIG_CMNINFO_LSIGLEN_FSZ, lsig_len);
	/* cascade index bit */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_CASCADEIND_INDX,
		HE_TRIG_CMNINFO_CASCADEIND_FSZ, info->ul_more_tf);
	/* CS required */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_CSREQ_INDX,
		HE_TRIG_CMNINFO_CSREQ_FSZ, info->ul_cs_req);
	/* BW info */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_BWINFO_INDX,
		HE_TRIG_CMNINFO_BWINFO_FSZ, bw);
	/* GI-LTF */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_GI_LTF_INDX,
		HE_TRIG_CMNINFO_GI_LTF_FSZ, cp_ltf_type);
	/* MUMIMO-LTF indx */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_MUMIMO_LTF_INDX,
		HE_TRIG_CMNINFO_MUMIMO_LTF_FSZ, info->ul_mu_mimo_ltf);
	/* HE-LTF symbols */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_HELTF_SYM_INDX,
		HE_TRIG_CMNINFO_HELTF_SYM_FSZ, num_heltf);
	/* STBC */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_STBC_INDX,
		HE_TRIG_CMNINFO_STBC_FSZ, info->ul_stbc);
	/* LDPC extra symb */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_LDPC_EXTSYM_INDX,
		HE_TRIG_CMNINFO_LDPC_EXTSYM_FSZ, info->ul_ldpc_extsym);
	/* AP TX power */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_AP_TXPWR_INDX,
		HE_TRIG_CMNINFO_AP_TXPWR_FSZ, info->ul_ap_txpwr);
	/* a-factor */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_AFACT_INDX,
		HE_TRIG_CMNINFO_AFACT_FSZ, info->ul_afact);
	/* PE disambig */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_PEDISAMBIG_INDX,
		HE_TRIG_CMNINFO_PEDISAMBIG_FSZ, info->ul_pe_disambig);
	/* spatial re-use */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_SPTIAL_REUSE_INDX,
		HE_TRIG_CMNINFO_SPTIAL_REUSE_FSZ, info->ul_spat_reuse);
	/* doppler indx */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_DOPPLER_INDX,
		HE_TRIG_CMNINFO_DOPPLER_FSZ, info->ul_doppler);
	/* HE-SIGA rsvd */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_HESIGA_RSVD_INDX,
		HE_TRIG_CMNINFO_HESIGA_RSVD_FSZ, info->ul_hesiga2_rsvd);
	/* rsvd */
	setbits((uint8 *)cmn_info, sizeof(*cmn_info), HE_TRIG_CMNINFO_RSVD_INDX,
		HE_TRIG_CMNINFO_RSVD_FSZ, info->ul_rsvd);

	/* write trigger frame common info to shm */
	if (triginfo_txrx == TXTRIG) {
		offset = M_TXTRIG_CMNINFO(wlc);
	} else {
		offset = M_RXTRIG_CMNINFO(wlc);
	}
	nshm = CEIL(sizeof(he_trig_cmninfo_set_t), 2);
	offset = wlc_test_incremental_write_shm(wlc_hw, offset, (uint16 *)cmn_info, nshm);

	if (triginfo_txrx == TXTRIG) {
		offset_srxctl = M_TXTRIG_SRXCTLUSR(wlc);
	}

	ruidx = 0;
	if (info->ul_auto_ru) {
		ruidx = wlc_test_pkteng_get_default_ruidx(info->ul_bw, nusrs);
	}

	nusrs = 0;
	for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i ++) {
		uint8 dcm, nss, ldpc;
		usr = &info->usr[i];

		if (!(WLC_TEST_PKTENGCMD_IS_VALID(usr->flags))) {
			continue;
		}
		if (WLC_TEST_PKTENGCMD_IS_ULDISABLED(usr->flags)) {
			continue;
		}
		memset(per_usr_info, 0, perusr_info_sz);

		dcm = (usr->rspec & WL_RSPEC_DCM) != 0;

		nss = (usr->rspec & WL_RSPEC_HE_NSS_MASK) >> WL_RSPEC_HE_NSS_SHIFT;
		nss--;
		ldpc = RSPEC_ISLDPC(usr->rspec);

		/* user info fields */

		/* AID */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_AID_INDX,
			HE_TRIG_USRINFO_AID_FSZ, usr->aid);
		/* RU Alloc index :: actual ru_alloc field is internal ruidx << 1 */
		if (info->ul_auto_ru) {
			val16 = ruidx;
			ruidx++;
		} else {
			val16 = usr->ruidx;
		}

		val16 = val16 >= RUIDX_160MHZ_80U? ((val16 - RUIDX_160MHZ_80U) << 1) + 1:
			val16 << 1;
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_RU_ALLOC_INDX,
			HE_TRIG_USRINFO_RU_ALLOC_FSZ, val16);
		/* coding type */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_CODING_INDX,
			HE_TRIG_USRINFO_CODING_FSZ, ldpc);
		/* mcs index */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_MCS_INDX,
			HE_TRIG_USRINFO_MCS_FSZ, (usr->rspec & WL_RSPEC_HE_MCS_MASK));
		/* dcm */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_DCM_INDX,
			HE_TRIG_USRINFO_DCM_FSZ, dcm);
		/* SS_alloc */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_SSALLOC_STRMOFFSET_INDX,
			HE_TRIG_USRINFO_SSALLOC_STRMOFFSET_FSZ + HE_TRIG_USRINFO_SSALLOC_NSS_FSZ,
			usr->ss_alloc);
		/* SS_alloc NSS */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_SSALLOC_NSS_INDX,
			HE_TRIG_USRINFO_SSALLOC_NSS_FSZ, nss);
		/* target rssi */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_TARGET_RSSI_INDX,
			HE_TRIG_USRINFO_TARGET_RSSI_FSZ, usr->target_rssi);
		/* rsvd */
		setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_RSVD_INDX,
			HE_TRIG_USRINFO_RSVD_FSZ, 0);

		if (info->txtrig_type == HE_TRIG_TYPE_BASIC_FRM) {
			/* type dep per usr info */
			setbits(per_usr_info, perusr_info_sz, HE_TRIG_USRINFO_TYPEDEP_INDX,
				HE_TRIG_USRINFO_TYPEDEP_FSZ, usr->type_dep_usr_info);
		}

		if (triginfo_txrx == TXTRIG) {
			/* Write rxctl usr list */
			wlc_write_shm(wlc, offset_srxctl, (nusrs << 8) | nusrs);
			nusrs++;
			offset_srxctl += 2;
		}
		per_usr_info += perusr_info_sz;
	}

	/* Write usr_info into shm M_TX(RX)TRIG_CMNINFO */
	nshm = CEIL(usr_info_sz, 2);
	offset = wlc_test_incremental_write_shm(wlc_hw, offset, (uint16 *)usr_info, nshm);

	/* trigger frame MAC header = FC (2B) + Dur (2B) + RA (6B) + TA (6B)  and FCS (4B) */
	trigfrm_len = DOT11_CTL_HDR_LEN + DOT11_FCS_LEN;
	trigfrm_len += sizeof(he_trig_cmninfo_set_t);
	trigfrm_len += usr_info_sz;

	/* Write padding */
	if ((trigfrm_len & 1) == 1) {
		/* if there are odd number of bytes in trig frame */
		val16 = wlc_read_shm(wlc, offset-2);
		val16 |= 0xff00;
		wlc_write_shm(wlc, offset-2, val16);
	}
	nshm = CEIL(info->nbyte_pad, 2);
	for (i = 0; i < nshm; i++) {
		wlc_write_shm(wlc, offset, (uint16) -1);
		offset += 2;
	}

	trigfrm_len += info->nbyte_pad;

	if (triginfo_txrx == TXTRIG) {
		txtrig_flag = wlc_read_shm(wlc, M_TXTRIG_FLAG(wlc));
		txtrig_flag &= (~WLC_PKTENGCMD_TXTRIGFLAG_FRMTYPE_MASK);
		txtrig_flag |= ((info->txtrig_type << WLC_PKTENGCMD_TXTRIGFLAG_FRMTYPE_SHIFT) &
			WLC_PKTENGCMD_TXTRIGFLAG_FRMTYPE_MASK);
		wlc_bmac_update_shm(wlc_hw, M_MFGTEST_NUM(wlc), MFGTEST_ULOFDMA_PKTENG_EN,
			MFGTEST_ULOFDMA_PKTENG_EN);
		/* Write the trigger frame control info */
		wlc_bmac_write_shm(wlc_hw, M_TXTRIG_FLAG(wlc), txtrig_flag);
		wlc_bmac_write_shm(wlc_hw, M_TXTRIG_NUM(wlc), 0);
		wlc_bmac_write_shm(wlc_hw, M_TXTRIG_LEN(wlc), trigfrm_len);
		wlc_bmac_write_shm(wlc_hw, M_TXTRIG_MINTIME(wlc), info->min_dur);
	}

exit:
	if (cmn_info) {
		MFREE(wlc->osh, cmn_info, sizeof(he_trig_cmninfo_set_t));
	}

	if (usr_info && usr_info_sz) {
		MFREE(wlc->osh, usr_info, usr_info_sz);
	}
	return ret;
}

static int
wlc_test_pkteng_ofdma_dl_tx(wlc_test_info_t *test, uint32 nframes, uint16 ifs)
{
	wlc_info_t *wlc;
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	wlc_test_pkteng_fifoinfo_t *fifo;
	struct scb *scb;
	int ret;

	struct ether_addr *sa;
	char eabuf[ETHER_ADDR_STR_LEN];
	void *pkt;
	ratespec_t rate_override;
	int i, j, k;
	wl_pkteng_t pkteng;
	uint16 seq;

	wlc = test->wlc;
	info = test->pkteng_info;
	ret = BCME_OK;

	BCM_REFERENCE(eabuf);

	sa = &wlc->pub->cur_etheraddr;

	pkteng.flags = WL_MUPKTENG_PER_TX_START;
	pkteng.delay = ifs;
	pkteng.nframes = info->nframes = nframes;

	WL_INFORM(("%s sa %s total usrs %d ntx %d\n", __FUNCTION__, bcm_ether_ntoa(sa, eabuf),
		info->nusrs, info->nframes));

	if ((info->nusrs == 0) || (info->nusrs > WLC_TEST_PKTENG_MAXUSR)) {
		return BCME_ERROR;
	}

	for (i = 0; i < info->nusrs; i++) {
		usr = &info->usr[info->usrlist[i]];
		if (usr->scb == NULL) {
			WL_ERROR(("%s scb is NULL usr_idx %d\n", __FUNCTION__, i));
			return BCME_ERROR;
		}
	}

	info->flags |= WLC_TEST_PKTENGCMD_FLAG_RUN;
	rate_override = info->rspec;

	ret = wlc_bmac_pkteng(wlc->hw, &pkteng, NULL, 0);
	if ((ret != BCME_OK)) {
		/* restore Mute State after pkteng is done */
		if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
			wlc_mute(wlc, ON, 0);
		return BCME_ERROR;
	}

	/* Unmute the channel for pkteng if quiet */
	if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
		wlc_mute(wlc, OFF, 0);
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub))
		wlc_bmac_suspend_macx_and_wait(wlc->hw);
#endif // endif
	for (i = 0; i < info->nusrs; i++) {
		usr = &info->usr[info->usrlist[i]];
		scb = usr->scb;
		if (scb == NULL) {
			WL_ERROR(("%s scb is NULL usr_idx %d\n", __FUNCTION__, i));

			pkteng.flags = WL_MUPKTENG_PER_TX_STOP;
			wlc_bmac_pkteng(wlc->hw, &pkteng, NULL, 0);
			/* restore Mute State after pkteng is done */
			if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec)) {
				wlc_mute(wlc, ON, 0);
			}

			return BCME_ERROR;
		}
		for (j = 0; j < usr->nfifo; j++) {
			fifo = &usr->fifo[j];
			for (k = 0; k < fifo->ampdu_mpdu; k++) {
				seq = k + 1;
				pkt = wlc_test_gen_testframe(wlc, scb, sa, rate_override, fifo->idx,
					fifo->frame_len, seq, fifo->tid);
				if (pkt == NULL) {
					WL_ERROR(("%s: failed to allocate testframe\n",
						__FUNCTION__));
					pkteng.flags = WL_MUPKTENG_PER_TX_STOP;
					wlc_bmac_pkteng(wlc->hw, &pkteng, NULL, 0);
					/* restore Mute State after pkteng is done */
					if (wlc_quiet_chanspec(wlc->cmi, wlc->chanspec))
						wlc_mute(wlc, ON, 0);
					return BCME_ERROR;
				}
				wlc_bmac_txfifo(wlc->hw, fifo->idx, pkt, TRUE, INVALIDFID, 1);
			}
			WL_INFORM(("%s: usr %s idx %d queued nframes %d flen %d fifo_idx %d\n",
				__FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf), i,
				fifo->ampdu_mpdu, fifo->frame_len, fifo->idx));
		}
	}

	OSL_DELAY(10000);
#ifdef WL_PSMX
	if (PSMX_ENAB(wlc->pub))
		wlc_bmac_enable_macx(wlc->hw);
#endif // endif

	return ret;

}

static void *
wlc_test_gen_testframe(wlc_info_t *wlc, struct scb *scb, struct ether_addr *sa,
                 ratespec_t rate_override, int fifo, int length, uint16 seq, uint8 ac)
{
	void *p;
	bool shortpreamble = FALSE;

	if ((p = wlc_tx_testframe_get(wlc, &scb->ea, sa, sa, length, NULL,
		NULL, 0)) == NULL) {
		WL_ERROR(("%s pkt is NULL\n", __FUNCTION__));
		return NULL;
	}

	/* Set BE Prio for packets */
	PKTSETPRIO(p, (ac & 0xF));
	WLPKTTAG(p)->flags |= WLF_AMPDU_MPDU;
	WLPKTTAG(p)->flags |= WLF_BYPASS_TXC;
	WLPKTTAG(p)->flags &= ~WLF_EXPTIME;
	WLPKTTAG(p)->seq = seq;
	/* add headers */
	wlc_d11hdrs(wlc, p, scb, shortpreamble, 0, 1, fifo, 0, NULL, NULL, rate_override);

	return p;
}

/** Called when e.g. 'wl msched sch 2' is typed by the user */
static int
wlc_test_set_scheduler(wlc_test_info_t *test, uint16 sch_type)
{
	int err;
	uint16 coremask1;
	uint8 txcore;
	wlc_test_pkteng_info_t *info;
#ifdef WL11AX
	int i;
	wlc_test_pkteng_usrinfo_t *usr;
#endif /* WL11AX */
	wlc_info_t *wlc;

	wlc = test->wlc;
	err = BCME_OK;
	info = test->pkteng_info;

	if ((info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA) == 0 &&
		(info->flags & WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO) == 0) {
		if (wlc->band->rspec_override & WL_RSPEC_OVERRIDE_RATE) {
			info->rspec = wlc->band->rspec_override;
		}
		err = wlc_test_pkteng_cmd_use_rspec(test);
	}

	/* Configure dl scheduler */
	if (sch_type & WLC_TEST_SCHTYPE_DLSCH) {
#ifdef WL11AX
		/* Invalidate previous scheduler info at scb */
		for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
			usr = &info->usr[i];
			if (usr->scb) {
				wlc_scbmusched_set_dlofdma(wlc->musched, usr->scb, FALSE);
				wlc_scbmusched_set_dlschpos(wlc->musched, usr->scb,
					MCTL2_INVALID_SCHPOS);
			}
		}
#endif /* ifdef WL11AX */

#if defined(WL_PSMX)
		/* Disable OMU flow control */
		wlc_bmac_mhf(wlc->hw, MXHF0, MXHF0_FCTLDIS, MXHF0_FCTLDIS, WLC_BAND_AUTO);
		err = wlc_test_pkteng_set_shmx_block(test);
#endif /* defined(WL_PSMX) */

#ifdef WL11AX
		for (i = 0; i < info->nusrs; i++) {
			usr = &info->usr[info->usrlist[i]];
			if (usr->scb == NULL) {
				WL_ERROR(("%s scb is NULL usr_idx %d\n", __FUNCTION__, i));
				return BCME_ERROR;
			}
			wlc_scbmusched_set_dlofdma(wlc->musched, usr->scb, TRUE);
			if (wlc_scbmusched_set_dlschpos(wlc->musched, usr->scb, usr->dl_schpos)
				!= BCME_OK) {
				WL_ERROR(("%s: fail to set sch for usr %d ", __FUNCTION__, i));
			}
		}
#endif /* WL11AX */

		wlc_musched_set_dlpolicy(wlc->musched, MUSCHED_DL_POLICY_FIXED);

		txcore = (info->phyctl[2] & 0xf);
		coremask1 = wlc_read_shm(wlc, M_COREMASK_BFM1(wlc));
		coremask1 = (txcore << 8) | (coremask1 & 0xff);
		wlc_write_shm(wlc, M_COREMASK_BFM1(wlc), coremask1);
	}

	/* Configure ul scheduler */
	if (sch_type & WLC_TEST_SCHTYPE_ULSCH) {
		info->ulsched_enab = TRUE;
		err = wlc_test_pkteng_prepare_for_trigger_frame(test, TXTRIG);
	}

	return err;
}

#if WL11AX
static void
wlc_test_pkteng_cmd_set_schid_per_aid_order(wlc_test_info_t *test, bool is_incr_order)
{
	int i;
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;

	info = test->pkteng_info;

	for (i = 0; i < info->nusrs; i++) {
		usr = &info->usr[info->usrlist[i]];
		if (is_incr_order) {
			usr->dl_schpos = i;
		} else {
			usr->dl_schpos = info->nusrs - i - 1;
		}
	}
}

static int
wlc_test_update_stalist(wlc_test_info_t *test)
{
	int err;
	int idx, i, j;
	wlc_test_pkteng_info_t *info;
	wlc_info_t *wlc;
	struct scb *scb;
	struct scb_iter scbiter;
	wlc_bsscfg_t *bsscfg;
	wlc_test_pkteng_usrinfo_t *usr, *usr1;
	wlc_test_pkteng_usrinfo_t tmp_usr;

	wlc = test->wlc;
	err = BCME_OK;
	info = test->pkteng_info;

	wlc_test_pkteng_cmd_init(test);

	WLC_TEST_PKTENGCMD_CLR(info->flags);
	WLC_TEST_PKTENGCMD_SET_DLSCH(info->flags);
	WLC_TEST_PKTENGCMD_SET_ULSCH(info->flags);

	for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i++) {
		usr = &info->usr[i];
		WLC_TEST_PKTENGCMD_DO_INVALID(usr->flags);
		WLC_TEST_PKTENGCMD_DO_UPDATE(usr->flags);
	}

	idx = i = 0;
	FOREACH_BSS(wlc, idx, bsscfg) {
		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
			if (!scb || !SCB_ASSOCIATED(scb) || !SCB_HE_CAP(scb)) {
				continue;
			}
			if (i >= WLC_TEST_PKTENG_MAXUSR) {
				WL_ERROR(("%s: sta idx %d exceeds limit %d",
					__FUNCTION__, i, WLC_TEST_PKTENG_MAXUSR));
				break;
			}
			usr = &info->usr[i];
			usr->aid = scb->aid;
			usr->scb = scb;
			usr->dl_schpos = i;
			usr->dl_schid = 0;
			usr->ul_schpos = i;
			usr->ul_schid = 0;

			usr->type_dep_usr_info = (1 << HETRIG_UITPDEP_TIDAGGLMT_SHIFT);

			memcpy(&usr->ea, &scb->ea, sizeof(usr->ea));
			usr->flags = 0;
			WLC_TEST_PKTENGCMD_DO_VALID(usr->flags);
			WL_INFORM(("Pkteng BSS %d idx %d scb flags %x %x %x aid %d\n",
				idx, i, scb->flags, scb->flags2, scb->flags3, scb->aid));
			info->usrlist[i] = i;
			i++;
			info->nusrs = i;
		}
	}

	// bubble sorting based on aid in increasing order
	for (i = 0; i < info->nusrs; i++) {
		for (j = i + 1; j < info->nusrs; j++) {
			usr = &info->usr[i];
			usr1 = &info->usr[j];
			if (usr->aid > usr1->aid) {
				memcpy(&tmp_usr, usr, sizeof(tmp_usr));
				memcpy(usr, usr1, sizeof(tmp_usr));
				memcpy(usr1, &tmp_usr, sizeof(tmp_usr));
				/* keep the original usr->idx */
				usr->idx = i;
				usr1->idx = j;
			}
		}
	}

	wlc_test_pkteng_cmd_set_schid_per_aid_order(test, TRUE);

	return err;
}
#endif /* WL11AX */

static int
wlc_test_pkteng_cmd_setkey(wlc_test_info_t *test,
	wl_pkteng_cmd_params_t* params)
{
	wlc_test_pkteng_info_t *info;
	wlc_info_t *wlc;
	int i, j, ret, endidx;
	int usridx, qidx;
	uint16 val16;
	char *keystr, *valstr;
	struct ether_addr ea;

	info = test->pkteng_info;
	wlc = test->wlc;
	ret = BCME_OK;

	usridx = params->u.argkeyval.val[0];
	qidx = params->u.argkeyval.val[1];
	keystr = params->u.argkeyval.keystr;
	valstr = params->u.argkeyval.valstr;

	if (!strncmp(keystr, "maxdur", strlen("maxdur"))) {
		info->max_dur = bcm_strtoul(valstr, NULL, 0);
	} else if (!strncmp(keystr, "mindur", strlen("mindur"))) {
		info->min_dur = bcm_strtoul(valstr, NULL, 0);
	} else if (!strncmp(keystr, "ifs", strlen("ifs"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		if (val16 > 10 && val16 < 10000) {
			info->ifs = val16;
		} else {
			ret = BCME_BADARG;
		}
	} else if (!strncmp(keystr, "ntx", strlen("ntx"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		info->nframes = val16;
#if WL11AX
	} else if (!strncmp(keystr, "updsta", strlen("updsta"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		wlc_test_update_stalist(test);
	} else if (!strncmp(keystr, "sch_order", strlen("sch_order"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		wlc_test_pkteng_cmd_set_schid_per_aid_order(test, val16 ? TRUE : FALSE);
#endif /* WL11AX */
	} else if (!strncmp(keystr, "sch", strlen("sch"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		ret = wlc_test_set_scheduler(test, val16);
	} else if (!strncmp(keystr, "manual_siga", strlen("manual_siga"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		if (val16 == 0) {
			info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA;
		} else {
			info->flags |= WLC_TEST_PKTENGCMD_FLAG_MANUALSIGA;
		}

	} else if (!strncmp(keystr, "manual_ul", strlen("manual_ul"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		if (val16 == 0) {
			info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO;
		} else {
			info->flags |= WLC_TEST_PKTENGCMD_FLAG_MANUALULCMNINFO;
		}
	} else if (!strncmp(keystr, "verbose", strlen("verbose"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		if (val16 != 0) {
			info->flags |= WLC_TEST_PKTENGCMD_FLAG_VERBOSE;
		} else {
			info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_VERBOSE;
		}
	} else if (!strncmp(keystr, "enable", strlen("enable")) ||
	!strncmp(keystr, "en", strlen("en"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		if (val16 != 0) {
			info->flags |= WLC_TEST_PKTENGCMD_FLAG_EN;
		} else {
			info->flags &= ~WLC_TEST_PKTENGCMD_FLAG_EN;
		}
	} else if (!strncmp(keystr, "mode", strlen("mode"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		info->mode = val16;
	} else if (!strncmp(keystr, "txtrig_", strlen("txtrig_"))) {
		if (!strncmp(keystr, "txtrig_rate", strlen("txtrig_rate"))) {
			uint16 phy_rate;
			val16 = bcm_strtoul(valstr, NULL, 0);
			if ((phy_rate = rate_info[(val16 << 1) & RATE_INFO_RATE_MASK] &
				RATE_INFO_M_RATE_MASK) != 0) {
				/* write both trigger and response rate */
				wlc_write_shm(wlc, M_TXTRIG_RATE(wlc), (phy_rate << 8) | phy_rate);
			} else {
				ret = BCME_BADARG;
			}
			return ret; /* skip setting the update flag */
		} else if (!strncmp(keystr, "txtrig_sch", strlen("txtrig_sch"))) {
			uint16 txtrig_flag = 0;
			val16 = bcm_strtoul(valstr, NULL, 0);
			txtrig_flag = wlc_read_shm(wlc, M_TXTRIG_FLAG(wlc));
			if (val16 == 0) {
				txtrig_flag &= (~WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_MASK);
				wlc_write_shm(wlc, M_TXTRIG_FLAG(wlc), txtrig_flag);
				wlc_write_shm(wlc, M_TXTRIG_NUM(wlc), 0);
			} else {
				txtrig_flag &= (~WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_MASK);
				txtrig_flag |= WLC_PKTENGCMD_ULOFDMA_TXTRIG_SCH_MODE;
				wlc_write_shm(wlc, M_TXTRIG_FLAG(wlc), txtrig_flag);
			}
		} else if (!strncmp(keystr, "txtrig_cnt", strlen("txtrig_cnt"))) {
			uint16 txtrig_flag = 0;
			val16 = bcm_strtoul(valstr, NULL, 0);
			txtrig_flag = wlc_read_shm(wlc, M_TXTRIG_FLAG(wlc));
			if (val16 == 0) {
				txtrig_flag &= (~WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_MASK);
				wlc_write_shm(wlc, M_TXTRIG_FLAG(wlc), txtrig_flag);
				wlc_write_shm(wlc, M_TXTRIG_NUM(wlc), 0);
			} else {
				txtrig_flag &= (~WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_MASK);
				txtrig_flag |= WLC_PKTENGCMD_ULOFDMA_TXTRIG_CNT_MODE;
				wlc_write_shm(wlc, M_TXTRIG_FLAG(wlc), txtrig_flag);
				wlc_write_shm(wlc, M_TXTRIG_NUM(wlc), val16);
			}
		} else if (!strncmp(keystr, "txtrig_type", strlen("txtrig_type"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			if (val16 <= HE_TRIG_TYPE_BSR_FRM) {
				info->txtrig_type = val16;
			} else {
				ret = BCME_RANGE;
			}
		} else {
			ret = BCME_BADARG;
		}
	} else if (!strncmp(keystr, "ul_", strlen("ul_"))) {
		/* for ul params */
		if (!strncmp(keystr, "ul_lsig_len", strlen("ul_lsig_len"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_lsig_len = val16;
		} else if (!strncmp(keystr, "ul_more_tf", strlen("ul_more_tf"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_more_tf = val16;
		} else if (!strncmp(keystr, "ul_cs_req", strlen("ul_cs_req"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_cs_req = val16;
		} else if (!strncmp(keystr, "ul_bw", strlen("ul_bw"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			switch (val16) {
			case 20:
				info->ul_bw = D11_REV128_BW_20MHZ;
				break;
			case 40:
				info->ul_bw = D11_REV128_BW_40MHZ;
				break;
			case 80:
				info->ul_bw = D11_REV128_BW_80MHZ;
				break;
			case 160:
				info->ul_bw = D11_REV128_BW_160MHZ;
				break;
			default:
				ret = BCME_BADARG;
				break;
			}
		} else if (!strncmp(keystr, "ul_cp_ltftype", strlen("ul_cp_ltftype"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_cp_ltftype = val16;
		} else if (!strncmp(keystr, "ul_mu_mimo_ltf", strlen("ul_mu_mimo_ltf"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_mu_mimo_ltf = val16;
		} else if (!strncmp(keystr, "ul_stbc", strlen("ul_stbc"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_stbc = val16;
		} else if (!strncmp(keystr, "ul_ldpc_extsym", strlen("ul_ldpc_extsym"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_ldpc_extsym = val16;
		} else if (!strncmp(keystr, "ul_ap_txpwr", strlen("ul_ap_txpwr"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_ap_txpwr = val16;
		} else if (!strncmp(keystr, "ul_afact", strlen("ul_afact"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_afact = val16;
		} else if (!strncmp(keystr, "ul_pe_disambig", strlen("ul_pe_disambig"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_pe_disambig = val16;
		} else if (!strncmp(keystr, "ul_spat_reuse", strlen("ul_spat_reuse"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_spat_reuse = val16;
		} else if (!strncmp(keystr, "ul_doppler", strlen("ul_doppler"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_doppler = val16;
		} else if (!strncmp(keystr, "ul_hesiga2_rsvd", strlen("ul_hesiga2_rsvd"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_hesiga2_rsvd = val16;
		} else if (!strncmp(keystr, "ul_rsvd", strlen("ul_rsvd"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_rsvd = val16;
		} else if (!strncmp(keystr, "ul_pad", strlen("ul_pad"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->nbyte_pad = val16;
		} else if (!strncmp(keystr, "ul_numheltf", strlen("ul_numheltf"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_numheltf = val16;
		} else if (!strncmp(keystr, "ul_ap_txpwr", strlen("ul_ap_txpwr"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			info->ul_ap_txpwr = val16;
		} else {
			ret = BCME_BADARG;
		}
	}
#ifdef WLSMC
	else if (!strncmp(keystr, "smc", strlen("smc"))) {
		val16 = bcm_strtoul(valstr, NULL, 0);
		if (val16 == 1) {
			/* suspend mac */
			wlc_bmac_suspend_mac_and_wait(wlc->hw);
			wlc_smc_download(wlc->hw);
			/* resume mac */
			wlc_bmac_enable_mac(wlc->hw);
		}
	}
#endif /* WLSMC */
	else if (!strncmp(keystr, "ultx", strlen("ultx"))) {
		uint16 shm_val;
		val16 = bcm_strtoul(valstr, NULL, 0);
		shm_val =  wlc_bmac_read_shm(wlc->hw, M_ULTX_STS(wlc));
		if (val16 == 1) {
			shm_val |= (1 << WLC_TEST_PKTENGCMD_ULTX_STS_BLOCK_NBIT);
		} else {
			shm_val &= ~(1 << WLC_TEST_PKTENGCMD_ULTX_STS_BLOCK_NBIT);
		}
		wlc_bmac_write_shm(wlc->hw, M_ULTX_STS(wlc), shm_val);
	} else {
		if (usridx == -1) {
			i = 0;
			endidx = WLC_TEST_PKTENG_MAXUSR;
		} else if (usridx >= WLC_TEST_PKTENG_MAXUSR) {
			return BCME_BADARG;
		} else {
			i = usridx;
			endidx = usridx + 1;
		}

		/* for the per usr settings */

		if (!strncmp(keystr, "macaddr", strlen("macaddr"))) {
			if (!bcm_ether_atoe(valstr, &ea)) {
				return BCME_USAGE_ERROR;
			}
			for (; i < endidx; i++) {
				if (usridx == -1) {
					ea.octet[0]++;
				}
				memcpy(&info->usr[i].ea, &ea, sizeof(ea));
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "ru", strlen("ru"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				if (usridx != -1) {
					info->usr[i].ruidx = val16;
				} else {
					info->usr[i].ruidx = val16 + i;
				}
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "aid", strlen("aid"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				if (usridx != -1) {
					info->usr[i].aid = val16;
				} else {
					info->usr[i].aid = val16 + i;
				}
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "mcs", strlen("mcs"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].rspec &= ~WL_RSPEC_HE_MCS_MASK;
				info->usr[i].rspec |= (val16 & WL_RSPEC_HE_MCS_MASK);
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "nss", strlen("nss"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].rspec &= ~WL_RSPEC_HE_NSS_MASK;
				info->usr[i].rspec |= ((val16 << WL_RSPEC_HE_NSS_SHIFT) &
				WL_RSPEC_HE_NSS_MASK);
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "ldpc", strlen("ldpc"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].rspec &= ~WL_RSPEC_LDPC;
				info->usr[i].rspec |= (val16 == 1 ? WL_RSPEC_LDPC : 0);
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "dcm", strlen("dcm"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].rspec &= ~WL_RSPEC_DCM;
				info->usr[i].rspec |= ((val16 << 19) & WL_RSPEC_DCM);
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "txpwr", strlen("txpwr"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].txpwr = (uint8) val16;
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "ss_alloc", strlen("ss_alloc"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].ss_alloc = (uint8) val16;
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "dep_usrinfo", strlen("dep_usrinfo"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].type_dep_usr_info = (uint8) val16;
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "target_rssi", strlen("target_rssi"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].target_rssi = (uint8) val16;
				WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
			}
		} else if (!strncmp(keystr, "nfifo", strlen("nfifo"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			if (val16 > WLC_TEST_PKTENG_MAXFIFO) {
				ret = BCME_BADARG;
			} else {
				for (; i < endidx; i++) {
					info->usr[i].nfifo = val16;
					WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
				}
			}
		} else if (!strncmp(keystr, "framelen", strlen("framelen"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				if (qidx == -1) {
					WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
					for (j = 0; j < WLC_TEST_PKTENG_MAXFIFO; j++)
					{
						info->usr[i].fifo[j].frame_len = val16;
					}
				} else if (qidx < WLC_TEST_PKTENG_MAXFIFO) {
					info->usr[i].fifo[qidx].frame_len = val16;
					WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
				} else {
					ret = BCME_BADARG;
				}
			}
		} else if (!strncmp(keystr, "ampdu_mpdu", strlen("ampdu_mpdu"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				if (qidx == -1) {
					WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
					for (j = 0; j < WLC_TEST_PKTENG_MAXFIFO; j++)
					{
						info->usr[i].fifo[j].ampdu_mpdu = val16;
					}
				} else if (qidx < WLC_TEST_PKTENG_MAXFIFO) {
					info->usr[i].fifo[qidx].ampdu_mpdu = val16;
					WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[i].flags);
				} else {
					ret = BCME_BADARG;
				}
			}
		} else if (!strncmp(keystr, "dl_schpos", strlen("dl_schpos"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].dl_schpos = val16 < 255 ? val16 : -1;
			}
		} else if (!strncmp(keystr, "dl_schid", strlen("dl_schid"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].dl_schid = val16 < 255 ? val16 : -1;
			}
		} else if (!strncmp(keystr, "ul_schpos", strlen("ul_schpos"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].ul_schpos = val16 < 255 ? val16 : -1;
			}
		} else if (!strncmp(keystr, "ul_schid", strlen("ul_schid"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].ul_schid = val16 < 255 ? val16 : -1;
			}
		} else if (!strncmp(keystr, "schpos", strlen("schpos"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].dl_schpos = val16 < 255 ? val16 : -1;
				info->usr[i].ul_schpos = val16 < 255 ? val16 : -1;
			}
		} else if (!strncmp(keystr, "schid", strlen("schid"))) {
			val16 = bcm_strtoul(valstr, NULL, 0);
			for (; i < endidx; i++) {
				info->usr[i].dl_schid = val16 < 255 ? val16 : -1;
				info->usr[i].ul_schid = val16 < 255 ? val16 : -1;
			}
		}
	}

	if (ret == BCME_OK) {
		/* set the top-level updated flags */
		WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
	}

	return ret;
}

/* external functions */
bool
wlc_test_pkteng_run(wlc_test_info_t *testi)
{
	return ((testi->pkteng_info->flags & WLC_TEST_PKTENGCMD_FLAG_RUN) ? 1 : 0);
}

bool
wlc_test_pkteng_run_dlofdma(wlc_test_info_t *testi)
{
	return (wlc_test_pkteng_get_mode(testi) == WL_PKTENG_CMD_STRT_DL_CMD) &&
		wlc_test_pkteng_run(testi)? 1: 0;
}

bool
wlc_test_pkteng_en(wlc_test_info_t *testi)
{
	return ((testi->pkteng_info->flags & WLC_TEST_PKTENGCMD_FLAG_EN) ? 1 : 0);
}

int
wlc_test_pkteng_get_max_dur(wlc_test_info_t *testi)
{
	return testi->pkteng_info->max_dur;
}

int
wlc_test_pkteng_get_min_dur(wlc_test_info_t *testi)
{
	return testi->pkteng_info->min_dur;
}

int
wlc_test_pkteng_get_mode(wlc_test_info_t *testi)
{
	return testi->pkteng_info->mode;
}

#ifdef TESTBED_AP_11AX

void
update_pkt_eng_ulstate(wlc_info_t *wlc, bool on)
{
	uint16 txtrig_flag;

	if (!wlc->testi) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}
	txtrig_flag = wlc_read_shm(wlc, M_TXTRIG_FLAG(wlc));
	txtrig_flag &= (~WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_MASK);
	if (on) {
		wlc_write_shm(wlc, M_TXTRIG_NUM(wlc), 0xffff);
		txtrig_flag |= WLC_PKTENGCMD_ULOFDMA_TXTRIG_CNT_MODE;
		wlc_write_shm(wlc, M_TXTRIG_FLAG(wlc), txtrig_flag);
	} else {
		wlc_write_shm(wlc, M_TXTRIG_FLAG(wlc), txtrig_flag);
		wlc_write_shm(wlc, M_TXTRIG_NUM(wlc), 0);
	}
}

void
update_pkt_eng_ulnss(wlc_info_t *wlc, uint16 aid, uint16 nss)
{
	wlc_test_pkteng_info_t *info;
	uint16 userid;

	if (!wlc->testi) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}
	info = wlc->testi->pkteng_info;
	userid = aid - 1;
	info->usr[userid].rspec &= ~WL_RSPEC_HE_NSS_MASK;
	info->usr[userid].rspec |= (((nss + 1) << WL_RSPEC_HE_NSS_SHIFT) & WL_RSPEC_HE_NSS_MASK);
	WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[userid].flags);
	WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
	wlc_test_set_scheduler(wlc->testi, 2);
	update_pkt_eng_ulstate(wlc, TRUE);
}

void
update_pkt_eng_ulbw(wlc_info_t *wlc, uint16 aid, uint16 bw)
{
	wlc_test_pkteng_info_t *info;
	uint16 userid;

	if (!wlc->testi) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}
	info = wlc->testi->pkteng_info;
	info->ul_bw = bw;

	userid = aid - 1;
	switch (bw) {
	case D11_REV128_BW_20MHZ:
		info->usr[userid].ruidx = 61;
		break;
	case D11_REV128_BW_40MHZ:
		info->usr[userid].ruidx = 65;
		break;
	case D11_REV128_BW_80MHZ:
		info->usr[userid].ruidx = 67;
		break;
	}
	WLC_TEST_PKTENGCMD_DO_UPDATE(info->usr[userid].flags);
	WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
	wlc_test_set_scheduler(wlc->testi, 2);
	update_pkt_eng_ulstate(wlc, TRUE);
}

void
update_pkt_eng_trigcnt(wlc_info_t *wlc, uint16 trigger_count)
{
	uint16 txtrig_flag;

	if (!wlc->testi) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}
	txtrig_flag = wlc_read_shm(wlc, M_TXTRIG_FLAG(wlc));
	txtrig_flag &= (~WLC_PKTENGCMD_TXTRIGFLAG_CTLFLAG_MASK);
	txtrig_flag |= WLC_PKTENGCMD_ULOFDMA_TXTRIG_CNT_MODE;
	wlc_write_shm(wlc, M_TXTRIG_FLAG(wlc), txtrig_flag);
	wlc_write_shm(wlc, M_TXTRIG_NUM(wlc), trigger_count);
}

void
update_pkt_eng_trigger_enab(wlc_info_t *wlc, uint16 aid, bool enab)
{
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	int i;

	if (!wlc->testi) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}
	info = wlc->testi->pkteng_info;

	for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i ++) {
		usr = &info->usr[i];

		if (!(WLC_TEST_PKTENGCMD_IS_VALID(usr->flags))) {
			continue;
		}
		if (usr->aid == aid) {
			break;
		}
		usr = NULL;
	}
	if (usr == NULL) {
		return;
	}

	if (enab) {
		WLC_TEST_PKTENGCMD_DO_UL_ENABLE(usr->flags);
	} else {
		WLC_TEST_PKTENGCMD_DO_UL_DISABLE(usr->flags);
	}

	WLC_TEST_PKTENGCMD_DO_UPDATE(usr->flags);
	WLC_TEST_PKTENGCMD_DO_UPDATE(info->flags);
	wlc_test_set_scheduler(wlc->testi, 2);
}

void
update_pkt_eng_auto_ru_idx_enab(wlc_info_t *wlc)
{
	wlc_test_pkteng_info_t *info;
	wlc_test_pkteng_usrinfo_t *usr;
	int i;

	if (!wlc->testi) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}
	info = wlc->testi->pkteng_info;

	if (info->ul_auto_ru == FALSE) {
		info->ul_auto_ru = TRUE;
		for (i = 0; i < WLC_TEST_PKTENG_MAXUSR; i ++) {
			usr = &info->usr[i];

			if (!(WLC_TEST_PKTENGCMD_IS_VALID(usr->flags))) {
				continue;
			}
			WLC_TEST_PKTENGCMD_DO_UL_DISABLE(usr->flags);
		}
	}
}

void
update_pkt_eng_ul_lsig_len(wlc_info_t *wlc, uint16 ul_lsig_len)
{
	wlc_test_pkteng_info_t *info;

	if (!wlc->testi) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}
	info = wlc->testi->pkteng_info;
	info->ul_lsig_len = ul_lsig_len;
}

/** Callback of trigger timer */
static void
wlc_pkt_eng_trigger_timer(void *arg)
{
	wlc_test_info_t *test = (wlc_test_info_t *)arg;
	wlc_info_t *wlc = test->wlc;
	uint timeout;

	update_pkt_eng_trigcnt(wlc, 1);

	timeout = test->pkteng_trig_tmr * 1000;
	timeout -= PKTENG_TRG_TMR_DELAY_CORRECTION;
	timeout -= (timeout / 180);
	wlc_hrt_add_timeout(test->trigger_timer, timeout, wlc_pkt_eng_trigger_timer, test);
}

static void
update_pkt_eng_trg_tmr(wlc_info_t *wlc, uint32 trg_tmr)
{
	wlc_test_info_t *test = wlc->testi;

	if (!test) {
		return;
	}
	if (!wlc->testi->pkteng_info) {
		return;
	}
	if (!wlc->testi->pkteng_info->ulsched_enab) {
		return;
	}

	if (test->trigger_timer) {
		/* Timer is running, stop it */
		wlc_hrt_free_timeout(test->trigger_timer);
		test->trigger_timer = NULL;
	}

	test->pkteng_trig_tmr = trg_tmr;
	if (trg_tmr) {
		test->trigger_timer = wlc_hrt_alloc_timeout(wlc->hrti);
		if (!test->trigger_timer) {
			WL_ERROR(("%s Failed to init trigger timer\n", __FUNCTION__));
			return;
		}
		wlc_pkt_eng_trigger_timer(test);
	}
}

/** Callback of packet timer */
static void
wlc_pkt_eng_packet_timer(void *arg)
{
	wlc_test_info_t *test = (wlc_test_info_t *)arg;
	wlc_info_t *wlc = test->wlc;
	uint timeout;
	struct scb *scb;
	struct scb_iter scbiter;
	ratespec_t rate_override;

	wlc_bsscfg_t *bsscfg;

	if (!wlc->pub->up || !AP_ENAB(wlc->pub)) {
		return;
	}
	bsscfg = wlc_bsscfg_primary(wlc);
	if (!bsscfg || !bsscfg->up) {
		return;
	}
	scb = NULL;
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!ETHER_ISBCAST(&scb->ea))
			break;
	}
	if (!scb) {
		return;
	}
	rate_override = wlc_lowest_basicrate_get(bsscfg);

	wlc_sendnulldata(wlc, bsscfg, &scb->ea, rate_override, WLF_PSDONTQ, PRIO_8021D_VO, NULL,
		NULL);

	timeout = test->pkteng_pkt_tmr * 1000;
	timeout -= PKTENG_PKT_TMR_DELAY_CORRECTION;
	timeout -= (timeout / 180);
	wlc_hrt_add_timeout(test->packet_timer, timeout, wlc_pkt_eng_packet_timer, test);
}

static void
update_pkt_eng_pkt_tmr(wlc_info_t *wlc, uint32 pkt_tmr)
{
	wlc_test_info_t *test = wlc->testi;

	if (!test) {
		return;
	}

	if (test->packet_timer) {
		/* Timer is running, stop it */
		wlc_hrt_free_timeout(test->packet_timer);
		test->packet_timer = NULL;
	}

	test->pkteng_pkt_tmr = pkt_tmr;
	if (pkt_tmr) {
		test->packet_timer = wlc_hrt_alloc_timeout(wlc->hrti);
		if (!test->packet_timer) {
			WL_ERROR(("%s Failed to init packet timer\n", __FUNCTION__));
			return;
		}
		wlc_pkt_eng_packet_timer(test);
	}
}

#endif /* TESTBED_AP_11AX */

#endif // endif
