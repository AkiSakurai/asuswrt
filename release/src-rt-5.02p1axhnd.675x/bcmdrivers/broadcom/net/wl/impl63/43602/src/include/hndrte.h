/*
 * HND Run Time Environment for standalone MIPS programs.
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
 * $Id: hndrte.h 708652 2017-07-04 04:52:34Z $
 */

#ifndef	_HNDRTE_H
#define	_HNDRTE_H

#include <typedefs.h>
#include <siutils.h>
#include <sbchipc.h>
#include <osl_decl.h>
/* for sbconfig_t */
#include <sbconfig.h>

#if defined(_HNDRTE_SIM_)
#include <hndrte_sim.h>
#elif defined(mips)
#include <hndrte_mips.h>
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
#include <hndrte_arm.h>
#endif // endif

#include <bcmstdlib.h>
#include <hndrte_trap.h>

#define HNDRTE_DEV_NAME_MAX	16

/* RTE IOCTL definitions for generic ether devices */
#define RTEGHWADDR		0x8901
#define RTESHWADDR		0x8902
#define RTEGMTU			0x8903
#define RTEGSTATS		0x8904
#define RTEGALLMULTI		0x8905
#define RTESALLMULTI		0x8906
#define RTEGPROMISC		0x8907
#define RTESPROMISC		0x8908
#define RTESMULTILIST  		0x8909
#define RTEGUP			0x890A
#define RTEGPERMADDR		0x890B

#define RTE_IOCTL_QUERY			0x00
#define RTE_IOCTL_SET			0x01
#define RTE_IOCTL_OVL_IDX_MASK	0x1e
#define RTE_IOCTL_OVL_RSV		0x20
#define RTE_IOCTL_OVL			0x40
#define RTE_IOCTL_OVL_IDX_SHIFT	1

/* Forward declaration */
struct lbuf;
struct pktpool;

extern si_t *hndrte_sih;		/* Chip backplane handle */
extern osl_t *hndrte_osh;		/* Chip backplane osl */
extern chipcregs_t *hndrte_ccr;		/* Chipcommon core regs */
extern pmuregs_t *hndrte_pmur;		/* PMU core regs */
extern sbconfig_t *hndrte_ccsbr;	/* Chipcommon core SB config regs */
extern struct pktpool *pktpool_shared;

#define PKT_INVALID_ID                  ((uint16)(~0))
#define PKT_NULL_ID                     ((uint16)(0))

#if defined(BCMPKTIDMAP)

/*
 * Compress a 32bit pkt pointer to 16bit unique ID to save memory in storing
 * pointers to packets. [In NIC mode (as opposed to dongle modes), there is no
 * memory constraint ... In NIC mode a pkt pointer may be 64bits. If a packet
 * pointer to packet ID suppression is desired in NIC mode, then the ability to
 * specify the maximum number of packets and a global pktptr_map is required.
 *
 * When a packet is first allocated from the heap, an ID is associated with it.
 * Packets allocated from the heap and filled into a pktpool will also get a
 * unique packet ID. When packets are allocated and freed into the pktpool, the
 * packet ID is preserved. In hndrte, lb_alloc() and lb_free_one() are the
 * only means to first instantiate (allocate from heap and initialize) a packet.
 * Packet ID management is performed in these functions.
 *
 * The lifetime of a packet ID is from the time it is allocated from the heap
 * until it is freed back to the heap.
 *
 * Packet ID 0 is reserved and never used and corresponds to NULL pktptr.
 */
#if !defined(PKT_MAXIMUM_ID)
#error "PKT_MAXIMUM_ID is not defined"
#endif  /* ! PKT_MAXIMUM_ID */

extern struct lbuf **hndrte_pktptr_map;	/* exported pktid to pktptr map */

extern uint32 hndrte_pktid_free_cnt(void);
extern uint32 hndrte_pktid_fail_cnt(void);
extern void hndrte_pktid_inc_fail_cnt(void);
extern uint16 hndrte_pktid_allocate(const struct lbuf * pktptr);
extern void   hndrte_pktid_release(const struct lbuf * pktptr,
                                   const uint16 pktid);
extern bool   hndrte_pktid_sane(const struct lbuf * pktptr);

#else  /* ! BCMPKTIDMAP */

#if !defined(PKT_MAXIMUM_ID)
#define PKT_MAXIMUM_ID                  (0)
#endif  /* ! PKT_MAXIMUM_ID */

#define hndrte_pktid_free_cnt()         (1)
#define hndrte_pktid_allocate(p)        (0)
#define hndrte_pktid_release(p, i)      do {} while (0)
#define hndrte_pktid_sane(p)            (TRUE)
#define hndrte_pktid_fail_cnt()         (0)
#define hndrte_pktid_inc_fail_cnt()     do {} while (0)

#endif /* ! BCMPKTIDMAP */

typedef struct hndrte_devfuncs hndrte_devfuncs_t;

/* happens to mirror a section of linux's net_device_stats struct */
typedef struct {
	unsigned long	rx_packets;		/* total packets received	*/
	unsigned long	tx_packets;		/* total packets transmitted	*/
	unsigned long	rx_bytes;		/* total bytes received 	*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux	*/
	unsigned long   multicast;		/* multicast packets received */
} hndrte_stats_t;

struct fetch_rqst;
typedef void (*fetch_cmplt_cb_t)(struct fetch_rqst *rqst, bool cancelled);

/* Private fetch_rqst flags
 * Make sure these flags are cleared before dispatching
 */
#define FETCH_RQST_IN_BUS_LAYER 0x01
#define FETCH_RQST_CANCELLED 0x02

/* External utility API */
#define FETCH_RQST_FLAG_SET(fr, bit) ((fr)->flags |= bit)
#define FETCH_RQST_FLAG_GET(fr, bit) ((fr)->flags & bit)
#define FETCH_RQST_FLAG_CLEAR(fr, bit) ((fr)->flags &= (~bit))

typedef struct fetch_rqst {
	dma64addr_t haddr;
	uint16 size;
	uint8 flags;
	uint8 rsvd;
	uint8 *dest;
	fetch_cmplt_cb_t cb;
	void *ctx;
	struct fetch_rqst *next;
} fetch_rqst_t;

typedef int (*bus_dispatch_cb_t)(struct fetch_rqst *fr, void *arg);

struct fetch_rqst_q_t {
	fetch_rqst_t *head;
	fetch_rqst_t *tail;
};

struct fetch_module_info {
	struct pktpool *pool;
	bus_dispatch_cb_t cb;
	void *arg;
#ifdef DBG_BUS
	struct hndrte_dbg_bus_s {
		uint32 hndrte_fetch_rqst;
		uint32 hndrte_dispatch_fetch_rqst;
		uint32 hndrte_flush_fetch_rqstq;
		uint32 hndrte_remove_fetch_rqst_from_queue;
		uint32 hndrte_cancel_fetch_rqst;
		uint32 hndrte_pktfetch;
		uint32 hndrte_pktfetch_dispatch;
		uint32 hndrte_pktfetch_completion_handler;
	} dbg_bus;
#endif /* DBG_BUS */
};

extern struct fetch_module_info *fetch_info;

/* PktFetch module related stuff */
/* PktFetch info small enough to fit into PKTTAG of lbuf/lfrag */

typedef void (*pktfetch_cmplt_cb_t)(void *lbuf, void *orig_lfrag, void *ctx, bool cancelled);

struct pktfetch_info {
	void *lfrag;
	uint16 headroom;
	int16 host_offset;
	pktfetch_cmplt_cb_t cb;
	void *ctx;
	struct pktfetch_info *next;
	osl_t *osh;
};

/* No. of fetch_rqsts prealloced for the PKTFETCH module. This needs to be a tunable */
#ifndef PKTFETCH_MAX_FETCH_RQST
#define PKTFETCH_MAX_FETCH_RQST	8
#endif // endif
/* PKTFETCH LBUF Q size can never exceed the no. of fetch_rqsts allocated for use by the module */
#define PKTFETCH_LBUF_QSIZE PKTFETCH_MAX_FETCH_RQST

struct pktfetch_rqstpool {
	struct fetch_rqst *head;
	struct fetch_rqst *tail;
	uint16 len;
	uint16 max;
};

enum pktfetchq_state {
	INITED = 0x00,
	STOPPED_UNABLE_TO_GET_LBUF,
	STOPPED_UNABLE_TO_GET_FETCH_RQST
};

struct pktfetch_q {
	struct pktfetch_info *head;
	struct pktfetch_info *tail;
	uint8 state;
	uint16 count;
};

struct pktfetch_module_ctx {
	pktfetch_cmplt_cb_t cb;
	void *orig_ctx;
};

struct pktfetch_generic_ctx {
	uint32 ctx_count;
	uint32 ctx[0];
};

#ifdef BCMDBG
/* For average value counters, no. of calls to skip before polling
 * This MUST be power of 2
 */
#define PKTFETCH_STATS_SKIP_CNT 32
typedef struct pktfetch_debug_stats {
	uint16 pktfetch_rqsts_rcvd;
	uint16 pktfetch_rqsts_rejected;
	uint16 pktfetch_succesfully_dispatched;
	uint16 pktfetch_dispatch_failed_nomem;
	uint16 pktfetch_dispatch_failed_no_fetch_rqst;
	uint16 pktfetch_rqsts_enqued;
	uint16 pktfetch_wake_pktfetchq_dispatches;
	uint32 pktfetch_qsize_accumulator;
	uint16 pktfetch_qsize_poll_count;
} pktfetch_debug_stats_t;
#endif /* BCMDBG */

#ifdef BCMDBG_CPU
typedef struct {
	uint32 totalcpu_cycles;
	uint32 usedcpu_cycles;
	uint32 cpusleep_cycles;
	uint32 min_cpusleep_cycles;
	uint32 max_cpusleep_cycles;
	uint32 num_wfi_hit;
	uint32 last;
} hndrte_cpu_stats_t;
#endif // endif

/* Device instance */
typedef struct hndrte_dev {
	char			name[HNDRTE_DEV_NAME_MAX];
	hndrte_devfuncs_t	*funcs;
	uint32			devid;
	void			*softc;		/* Software context */
	uint32			flags;		/* RTEDEVFLAG_XXXX */
	struct hndrte_dev	*next;
	struct hndrte_dev	*chained;
	void			*pdev;
} hndrte_dev_t;

#define RTEDEVFLAG_HOSTASLEEP	0x000000001	/* host is asleep */
#define RTEDEVFLAG_USB30	0x000000002	/* has usb 3.0 device core  */

#ifdef SBPCI
typedef struct _pdev {
	struct _pdev	*next;
	si_t		*sih;
	uint16		vendor;
	uint16		device;
	uint		bus;
	uint		slot;
	uint		func;
	void		*address;
	bool		inuse;
} pdev_t;
#endif /* SBPCI */
typedef void (*txrxstatus_glom_cb)(void *ctx);
typedef struct {
	  txrxstatus_glom_cb cb;
	void *ctx;
} hndrte_txrxstatus_glom_info;
/* Device entry points */
struct hndrte_devfuncs {
	void *(*probe)(hndrte_dev_t *dev, void *regs, uint bus,
	               uint16 device, uint coreid, uint unit);
	int (*open)(hndrte_dev_t *dev);
	int (*close)(hndrte_dev_t *dev);
	int (*xmit)(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb);
	int (*recv)(hndrte_dev_t *src, hndrte_dev_t *dev, void *pkt);
	int (*ioctl)(hndrte_dev_t *dev, uint32 cmd, void *buffer, int len,
	             int *used, int *needed, int set);
	void (*txflowcontrol) (hndrte_dev_t *dev, bool state, int prio);
	void (*poll)(hndrte_dev_t *dev);
	int (*xmit_ctl)(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb);
	int (*xmit2)(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb, uint32 ep_idx);
#if defined(WL_WOWL_MEDIA) || defined(WOWLPF)
	void (*wowldown) (hndrte_dev_t *src, int up_flag);
#endif /* WL_WOWL_MEDIA || WOWLPF */
	uint32  (*flowring_link_update)
		(hndrte_dev_t *dev, uint16 flowid, uint8 op, uint8 * sa, uint8 *da, uint8 tid);

};

/* Use standard symbols for Armulator build which does not use the hndrte.lds linker script */
#if defined(_HNDRTE_SIM_) || defined(EXT_CBALL)
#define text_start	_start
#define text_end	etext
#define data_start	__data_start
#define data_end	edata
#define rodata_start	etext
#define rodata_end	__data_start
#define bss_start	__bss_start
#define bss_end		_end
#endif // endif

extern char text_start[], text_end[];
extern char rodata_start[], rodata_end[];
extern char data_start[], data_end[];
extern char bss_start[], bss_end[], _end[];

typedef struct {
	char *_text_start, *_text_end;
	char *_rodata_start, *_rodata_end;
	char *_data_start, *_data_end;
	char *_bss_start, *_bss_end;
#ifdef BCMRECLAIM
	char *_reclaim1_start, *_reclaim1_end;
#endif // endif
	char *_reclaim2_start, *_reclaim2_end;
	char *_reclaim3_start, *_reclaim3_end;
	char *_reclaim4_start, *_reclaim4_end;
	char *_boot_patch_start, *_boot_patch_end;
} hndrte_image_info_t;

extern void hndrte_image_info(hndrte_image_info_t *info);

/* Device support */
extern int hndrte_add_device(hndrte_dev_t *dev, uint16 coreid, uint16 device);
extern hndrte_dev_t *hndrte_get_dev(char *name);

/* ISR registration */
typedef void (*isr_fun_t)(void *cbdata);

extern int hndrte_add_isr(uint irq, uint coreid, uint unit,
                          isr_fun_t isr, void *cbdata, uint bus);

/* Registration without coreid */
extern int hndrte_add_isr_n(uint irq, uint isr_num,
                          isr_fun_t isr, void *cbdata);

/* Basic initialization and background */
extern void *hndrte_init(void);
#if defined(BCMPCIEDEV_ENABLED)
extern void hndrte_shared_init(void);
#endif // endif
extern void hndrte_poll(si_t *sih);
extern void hndrte_idle(si_t *sih);

/* Other initialization and background funcs */
extern void hndrte_isr(void);
extern void hndrte_timer_isr(void);
extern void hndrte_cpu_init(si_t *sih);
extern void hndrte_idle_init(si_t *sih);
extern void hndrte_arena_init(uintptr base, uintptr lim, uintptr stackbottom);
extern void hndrte_cons_init(si_t *sih);
extern void hndrte_cons_check(void);
extern void hndrte_cons_flush(void);
extern int hndrte_log_init(void);
extern void hndrte_stack_prot(void *stackTop);

#ifdef BCMDBG_CPU
extern void hndrte_update_stats(hndrte_cpu_stats_t *cpustats);
#endif // endif

/* Console command support */
typedef void (*cons_fun_t)(uint32 arg, uint argc, char *argv[]);

#if defined(HNDRTE_CONSOLE) || defined(BCM_OL_DEV)
extern void hndrte_cons_addcmd(const char *name, cons_fun_t fun, uint32 arg);
extern void process_ccmd(char *line, uint len);
#ifndef BCM_BOOTLOADER
extern void hndrte_cons_showcmds(uint32 arg, uint argc, char *argv[]);
#endif /* BCM_BOOTLOADER */
#else
#define hndrte_cons_addcmd(name, fun, arg) { (void)(name); (void)(fun); (void)(arg); }
#endif /* HNDRTE_CONSOLE || BCM_OL_DEV */

/* bcopy, bcmp, and bzero */
#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))

#ifdef BCMDBG
#define	TRACE_LOC		OSL_UNCACHED(0x18000044)	/* flash address reg in chipc */
#define	HNDRTE_TRACE(val)	do {*((uint32 *)TRACE_LOC) = val;} while (0)
#define	TRACE_LOC2		OSL_UNCACHED(0x180000d0)	/* bpaddrlow */
#define	HNDRTE_TRACE2(val)	do {*((uint32 *)TRACE_LOC2) = val;} while (0)
#define	TRACE_LOC3		OSL_UNCACHED(0x180000d8)	/* bpdata */
#define	HNDRTE_TRACE3(val)	do {*((uint32 *)TRACE_LOC3) = val;} while (0)
#else
#define	HNDRTE_TRACE(val)	do {} while (0)
#define	HNDRTE_TRACE2(val)	do {} while (0)
#define	HNDRTE_TRACE3(val)	do {} while (0)
#endif // endif

/* debugging prints */
#ifdef BCMDBG_ERR
#define	HNDRTE_ERROR(args)	do {printf args;} while (0)
#else /* BCMDBG_ERR */
#define	HNDRTE_ERROR(args)	do {} while (0)
#endif /* BCMDBG_ERR */

/* assert */
#if defined(BCMDBG_ASSERT)
extern void hndrte_assert(const char *file, int line);
	#if defined(BCMDBG_ASSERT_TRAP)
		/* DBG_ASSERT_TRAP causes a trap/exception when an ASSERT fails, instead of calling
		 * an assert handler to log the file and line number. This is a memory optimization
		 * that eliminates the strings associated with the file/line and the function call
		 * overhead associated with invoking the assert handler. The assert location can be
		 * determined based upon the program counter displayed by the trap handler.
		 */
		#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7R__)
			/* Use the system service call (SVC) instruction to generate a software
			 * interrupt for a failed assert. This will use a 2-byte instruction to
			 * generate the trap. It is more memory efficient than the alternate C
			 * implementation which may use more than 2-bytes to generate the trap.
			 * It also allows the trap handler to uniquely identify the trap as an
			 * assert (since this is the only use of software interrupts in the system).
			 */
			#define ASSERT(exp) do { if (!(exp)) asm volatile("SVC #0"); } while (0)
		#else /* !__ARM_ARCH_7M__ */
			#define ASSERT(exp) do { \
			                       if (!(exp)) { int *null = NULL; *null = 0; } \
			                    } while (0)
		#endif /* __ARM_ARCH_7M__ */
	#else /* !BCMDBG_ASSERT_TRAP */
#ifndef _FILENAME_
#define _FILENAME_ "_FILENAME_ is not defined"
#endif // endif
#define ASSERT(exp) \
	do { if (!(exp)) hndrte_assert(_FILENAME_, __LINE__); } while (0)
	#endif /* BCMDBG_ASSERT_TRAP */
#else
#define	ASSERT(exp)		do {} while (0)
#endif // endif

/* Timing */
typedef void (*to_fun_t)(void *arg);

typedef struct _ctimeout {
	struct _ctimeout *next;
	uint32 ms;
	to_fun_t fun;
	void *arg;
	bool expired;
} ctimeout_t;

extern uint32 _memsize;
extern uint32 _rambottom;
extern uint32 _atcmrambase;

extern void hndrte_delay(uint32 usec);
extern uint32 hndrte_time(void);

extern void hndrte_set_reftime(uint32 ms);
extern uint32 hndrte_reftime_ms(void);

extern uint32 hndrte_update_now(void);
#ifdef BCMDBG_SD_LATENCY
extern uint32 hndrte_time_us(void);
extern uint32 hndrte_update_now_us(void);
#endif /* BCMDBG_SD_LATENCY */
extern void hndrte_wait_irq(si_t *sih);
extern void hndrte_enable_interrupts(void);
extern void hndrte_disable_interrupts(void);
extern void hndrte_set_irq_timer(uint ms);
extern void hndrte_ack_irq_timer(void);
extern void hndrte_suspend_timer(void);
extern void hndrte_resume_timer(void);
extern void hndrte_trap_init(void);
extern void hndrte_memtrace_enab(bool on);
extern void hndrte_trap_handler(trap_t *tr);
typedef void (*hndrte_halt_handler)(void);
extern void hndrte_set_fwhalt(hndrte_halt_handler fwhalt_handler);
extern uint32 hndrte_get_memsize(void);
extern uint32 hndrte_get_rambase(void);

#ifdef BCMPCIEDEV_ENABLED
/* HostFetch module functions */
extern void hndrte_fetch_bus_dispatch_cb_register(bus_dispatch_cb_t cb, void *arg);
extern void hndrte_fetch_rqst(struct fetch_rqst *fr);
extern void hndrte_wake_fetch_rqstq(void);
extern bool hndrte_fetch_rqstq_empty(void);
extern void hndrte_flush_fetch_rqstq(void);
extern void hndrte_dmadesc_avail_cb(void);
extern int hndrte_cancel_fetch_rqst(struct fetch_rqst *fr);

/* PktFetch Module functions */
extern int hndrte_pktfetch(struct pktfetch_info *pinfo);
extern void hndrte_pktfetch_module_init(void);
extern void hndrte_pktfetchpool_init(void);
extern void hndrte_wake_pktfetchq(void);
extern void hndrte_lb_free_cb(void);
#endif /* BCMPCIEDEV_ENABLED */

typedef struct hndrte_timer
{
	uint32	*context;		/* first field so address of context is timer struct ptr */
	void	*data;
	void	(*mainfn)(struct hndrte_timer *);
	void	(*auxfn)(void *context);
	ctimeout_t t;
	int	interval;
	int	set;
	int	periodic;
	bool	_freedone;
} hndrte_timer_t, hndrte_task_t;

extern bool hndrte_timeout(ctimeout_t *t, uint32 ms, to_fun_t fun, void *arg);
extern void hndrte_del_timeout(ctimeout_t *t);
extern void hndrte_init_timeout(ctimeout_t *t);

extern hndrte_timer_t *hndrte_init_timer(void *context, void *data,
                                         void (*mainfn)(hndrte_timer_t *),
                                         void (*auxfn)(void*));
extern void hndrte_free_timer(hndrte_timer_t *t);
extern bool hndrte_add_timer(hndrte_timer_t *t, uint ms, int periodic);
extern bool hndrte_del_timer(hndrte_timer_t *t);

extern int hndrte_schedule_work(void *context, void *data,
	void (*taskfn)(hndrte_timer_t *), int delay);

/* malloc, free */
#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
#define hndrte_malloc(_size)	hndrte_malloc_align((_size), 0, __FILE__, __LINE__)
extern void *hndrte_malloc_align(uint size, uint alignbits, const char *file, int line);
#define hndrte_malloc_pt(_size)	hndrte_malloc_ptblk((_size), __FILE__, __LINE__)
extern void *hndrte_malloc_ptblk(uint size, const char *file, int line);
#else
#define hndrte_malloc(_size)	hndrte_malloc_align((_size), 0)
extern void *hndrte_malloc_align(uint size, uint alignbits);

#define hndrte_malloc_pt(_size)	hndrte_malloc_ptblk((_size))
extern void *hndrte_malloc_ptblk(uint size);
#endif /* BCMDBG_MEM */
extern void hndrte_append_ptblk(void);
extern void *hndrte_realloc(void *ptr, uint size);
extern void *hndrte_calloc(uint num, uint size);
extern int hndrte_free(void *ptr);
extern int hndrte_free_pt(void *ptr);
extern uint hndrte_memavail(void);
extern uint hndrte_hwm(void);
#ifndef BCM_BOOTLOADER
extern void hndrte_print_memuse(void);
extern void hndrte_dump_test(uint32 arg, uint argc, char *argv[]);
extern void hndrte_print_memwaste(uint32 arg, uint argc, char *argv[]);
#endif /* BCM_BOOTLOADER */
#ifdef BCM_VUSBD
extern void hndrte_print_vusbstats(void);
#endif // endif
/* Low Memory rescue functions
 * Implement a list of Low Memory free functions that hndrte can
 * call on allocation failure. List is populated through calls to
 * hndrte_pt_lowmem_register() API
 */
typedef void (*hndrte_lowmem_free_fn_t)(void *free_arg);
typedef struct hndrte_lowmem_free hndrte_lowmem_free_t;
struct hndrte_lowmem_free {
	hndrte_lowmem_free_t *next;
	hndrte_lowmem_free_fn_t free_fn;
	void *free_arg;
};

extern void hndrte_pt_lowmem_register(hndrte_lowmem_free_t *lowmem_free_elt);
extern void hndrte_pt_lowmem_unregister(hndrte_lowmem_free_t *lowmem_free_elt);

#if defined(BCMDBG_MEM) || defined(BCMDBG_MEMFAIL)
#ifdef BCMDBG_MEM
extern void hndrte_print_malloc(void);
extern int hndrte_memcheck(char *file, int line);
#endif // endif
extern void *hndrte_dma_alloc_consistent(uint size, uint16 align, uint *alloced,
	void *pap, char *file, int line);
#else
extern void *hndrte_dma_alloc_consistent(uint size, uint16 align, uint *alloced,
	void *pap);
#endif /* BCMDBG_MEM */
extern void hndrte_dma_free_consistent(void *va);

#ifdef DONGLEBUILD
extern void hndrte_reclaim(void);
#else
#define hndrte_reclaim() do {} while (0)
#endif /* DONGLEBUILD */

extern uint hndrte_arena_add(uint32 base, uint size);
#define BUS_GET_VAR     2
#define BUS_SET_VAR     3
#define BUS_FLOW_FLUSH_PEND 4
#define	BUS_SET_RX_OFFSET    5
#define	BUS_SET_MONITOR_MODE 6

#define SDPCMDEV_SET_MAXTXPKTGLOM	1

#ifndef	 HNDRTE_STACK_SIZE
#define	 HNDRTE_STACK_SIZE	(8192)
#endif // endif

extern volatile uint __watermark;

#ifdef HSIC_SDR_DEBUG
extern volatile uint __chipsdrenable;
extern volatile uint __nopubkeyinotp;
extern volatile uint __imagenotdigsigned;
extern volatile uint __pubkeyunavail;
extern volatile uint __rsaimageverify;
#endif // endif

#ifdef	BCMDBG
extern void hndrte_print_timers(uint32 arg, uint argc, char *argv[]);

#if defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
#define	BCMDBG_TRACE(x)		__watermark = (x)
#else
#define	BCMDBG_TRACE(x)
#endif	/* !__arm__ && !__thumb__ && !__thumb2__ */
#else
#define	BCMDBG_TRACE(x)
#endif	/* BCMDBG */

#ifdef HSIC_SDR_DEBUG
#define HSIC_SDR_DEBUG_1(x)		__chipsdrenable = (x)
#define HSIC_SDR_DEBUG_2(x)		__nopubkeyinotp = (x)
#define HSIC_SDR_DEBUG_3(x)		__imagenotdigsigned = (x)
#define HSIC_SDR_DEBUG_4(x)		__pubkeyunavail = (x)
#define HSIC_SDR_DEBUG_5(x)		__rsaimageverify = (x)
#else
#define HSIC_SDR_DEBUG_1(x)
#define HSIC_SDR_DEBUG_2(x)
#define HSIC_SDR_DEBUG_3(x)
#define HSIC_SDR_DEBUG_4(x)
#define HSIC_SDR_DEBUG_5(x)
#endif // endif

extern uint32 g_assert_type;
extern void hndrte_update_shared_struct(void * sh);

enum hndrte_ioctl_cmd {
	HNDRTE_DNGL_IS_SS = 1 /* true if device connected at super speed */
};

typedef void (*hndrte_ltrsend_cb_t)(void* arg, uint8 state);
typedef struct {
	hndrte_ltrsend_cb_t cb;
	void *arg;
} hndrte_ltrsend_cb_info_t;
extern int hndrte_register_ltrsend_callback(hndrte_ltrsend_cb_t cb, void *arg);
extern int hndrte_set_ltrstate(uint8 coreidx, uint8 state);

typedef void (*hndrte_devpwrstchg_cb_t)(void* arg, uint8 state);
typedef struct {
	hndrte_devpwrstchg_cb_t cb;
	void *arg;
} hndrte_devpwrstchg_cb_info_t;
extern int hndrte_register_devpwrstchg_callback(hndrte_devpwrstchg_cb_t cb, void *arg);
extern int hndrte_notify_devpwrstchg(bool state);

/* Register call back to toggle PME# */
typedef void (*hndrte_generate_pme_cb_t)(void* arg, uint8 state);
typedef struct {
	hndrte_generate_pme_cb_t cb;
	void *arg;
} hndrte_generate_pme_cb_info_t;
extern int hndrte_register_generate_pme_callback(hndrte_generate_pme_cb_t cb, void *arg);
extern int hndrte_generate_pme(bool state);

extern void hndrte_disable_deepsleep(bool disiable);
extern void* hndrte_enable_gci_gpioint(uint8 gci_gpio, uint8 sts,
	gci_gpio_handler_t handler, void *arg);
extern int32 hndrte_cpu_clockratio(si_t *sih, uint8 div);
extern void hndrte_register_cb_flush_chainedpkts(txrxstatus_glom_cb cb, void *ctx);
extern void hndrte_flush_glommed_txrxstatus(void);
typedef void (*hndrte_rx_reorder_qeueue_flush_cb_t)(void* arg, uint16 start, uint32 cnt);
typedef struct {
	hndrte_rx_reorder_qeueue_flush_cb_t cb;
	void *arg;
} hndrte_rx_reorder_qeueue_flush_cb_info_t;
extern int hndrte_register_rxreorder_queue_flush_cb(hndrte_rx_reorder_qeueue_flush_cb_t cb,
	void *arg);
extern int hndrte_flush_rxreorderqeue(uint16 start, uint32 cnt);

#endif	/* _HNDRTE_H */
