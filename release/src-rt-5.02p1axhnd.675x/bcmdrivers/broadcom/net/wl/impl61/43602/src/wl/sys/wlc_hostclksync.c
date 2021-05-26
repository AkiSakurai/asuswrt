/*
 * Host Clock Sync.
 *
 * This feature implements Host Clock Sync feature;periodic broadcast of TSF counter using ASTP.
 *
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
 *   $Id: wlc_hostclksync.h, pavank Exp $
 *
 */

/* ---- Include Files ---------------------------------------------------- */
#include <typedefs.h>
#ifdef WL_HOST_CLK_SYNC
#include <wlc_cfg.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <hndpmu.h>
#include <hndcpu.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc_pio.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_assoc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_scan.h>
#include <wlc_hostclksync.h>
#include <wlc_p2p.h>
#include <wlc_bmac.h>

#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif /* WLMCNX */

#include <bcmendian.h>
#include <wlc_ampdu.h>
#include <event_log.h>
#include <wlc_rate_sel.h>

/* ---- Public Variables ------------------------------------------------- */
/* ---- Private Constants and Types -------------------------------------- */

/* wlc_pub_t struct access macros */
#define WLCUNIT(info)   ((info)->wlc->pub->unit)
#define WLCOSH(info)    ((info)->wlc->osh)

/* debugging... */
#ifdef EVENT_LOG_COMPILE
#define WL_HOST_CLK_SYNC_ERR(str, p1, p2, p3, p4) \
			EVENT_LOG(EVENT_LOG_TAG_HOST_CLK_SYNC_ERR, str, p1, p2, p3, p4)
#define WL_HOST_CLK_SYNC_WARN(str, p1, p2, p3, p4) \
			EVENT_LOG(EVENT_LOG_TAG_HOST_CLK_SYNC_WARN, str, p1, p2, p3, p4)
#define WL_HOST_CLK_SYNC_INFO(str, p1, p2, p3, p4) \
			EVENT_LOG(EVENT_LOG_TAG_HOST_CLK_SYNC_INFO, str, p1, p2, p3, p4)
#else /* EVENT_LOG_COMPILE */
#define WL_HOST_CLK_SYNC_ERR(str, p1, p2, p3, p4)       WL_HOST_CLK_SYNC(str, p1, p2, p3, p4)
#define WL_HOST_CLK_SYNC_WARN(str, p1, p2, p3, p4)      WL_HOST_CLK_SYNC(str, p1, p2, p3, p4)
#define WL_HOST_CLK_SYNC_INFO(str, p1, p2, p3, p4)      WL_HOST_CLK_SYNC(str, p1, p2, p3, p4)
#endif /* EVENT_LOG_COMPILE */

#define HOST_CLK_SYNC_PRINT(x)                  /* printf x */

#define K_HOST_CLK_SYNC_FSM_INIT                0
#define K_HOST_CLK_SYNC_FSM_TRACK_INIT          1
#define K_HOST_CLK_SYNC_FSM_TRACK_LR_INIT       2
#define K_HOST_CLK_SYNC_FSM_TRACK               3

#define K_HOST_CLK_SYNC_LR_MAX_N		20
#define K_HOST_CLK_SYNC_LR_N                    20
#define K_HOST_CLK_SYNC_A_SHIFT                 24
#define K_HOST_CLK_SYNC_A_UPDATE_SHIFT          10
#define K_HOST_CLK_SYNC_B_UPDATE_SHIFT          10
#define K_HOST_CLK_SYNC_UPDATE_INTERVAL         9500 /* msecs */
#define K_HOST_CLK_SYNC_AWDL_OVER_ASTP		1
#define K_HOST_CLK_SYNC_TSF_OVER_ASTP		2
#define K_HOST_CLK_SYNC_DEF_OVER_ASTP		(K_HOST_CLK_SYNC_TSF_OVER_ASTP)

#define K_HOST_CLK_SYNC_CTRL_SHIFT              24
#define K_HOST_CLK_SYNC_CTRL_CAPTURE_TS         (0x1)
#define K_HOST_CLK_SYNC_CTRL_GET_TS             (0x2)
#define K_HOST_CLK_SYNC_CTRL_NULL               (0xFF)
#define K_HOST_CLK_SYNC_ID_MASK                 (0xFF)

#ifndef WL_HOST_CLK_SYNC_NIC
/* GPIO S/W control through chipControl_3 */
#define PMU_CHIPCONTROL_MASK                    ((1 << 3) | (1 << 1) | (1 << 0))
#define PMU_CHIPCONTROL                         ((1 << 3) | (0 << 1) | (1 << 0))
#endif /* !WL_HOST_CLK_SYNC_NIC */

#define TSTAMP_LEN                              (8)

#ifndef WL_HOST_CLK_SYNC_NIC
/* ---- NVRAM constants -------------------------------------------------------- */
static const char BCMATTACHDATA(host_clk_sync_gpio_est)[]  = "host_clk_sync_gpio_est";
static const char BCMATTACHDATA(host_clk_sync_gpio_raw)[]  = "host_clk_sync_gpio_raw";
#endif /* !WL_HOST_CLK_SYNC_NIC */

/* ---- Private Variables ------------------------------------------------ */
/* IOVar table */
enum {
	/* Get/set Host clock sync. */
	IOV_HOST_CLK_SYNC = 0,
	IOV_HOST_CLK_SYNC_CAP,
	} host_clk_sync_iov;

static const bcm_iovar_t host_clk_sync_iovars[] = {
	{
		"hostclksync", IOV_HOST_CLK_SYNC, (0), IOVT_INT8, 0
	},
	{
		"hostclksync_cap", IOV_HOST_CLK_SYNC_CAP, (0), IOVT_INT8, 0
	},
	{NULL, 0, 0, 0, 0 }
};

static void wlc_host_clk_sync_init(wlc_host_clk_sync_info_t *info);
static int host_clk_sync_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
                                 const char *name, void *p, uint plen, void *a,
                                 int alen, int vsize, struct wlc_if *wlcif);
static int wlc_host_clk_sync_up(void *ctx_info);
static int wlc_host_clk_sync_down(void *ctx_info);
static void wlc_host_clk_sync_fsm_init(wlc_host_clk_sync_info_t *info, uint32 hw_clk, uint32 tsf);
static void wlc_host_clk_sync_fsm_track(wlc_host_clk_sync_info_t *info, uint32 *hw_clk,
                                        uint32 *tsf, uint32 *tsf_est, int32 *e_a, int32 *e_b,
                                        int32 *lr_a, int32 *lr_b);

#ifndef WL_HOST_CLK_SYNC_NIC
static void wlc_host_clk_sync_configure(wlc_host_clk_sync_info_t *info);
static void wlc_host_clk_sync_gpio_init(wlc_host_clk_sync_info_t *info);
#endif /* !WL_HOST_CLK_SYNC_NIC */
static void wlc_host_clk_sync_cap_tstamp(wlc_host_clk_sync_info_t *info);

/* host_clk_sync module private info structure. */
struct wlc_host_clk_sync_info {
	wlc_info_t	*wlc;			/* Pointer back to wlc structure */
	wlc_bsscfg_t	*bsscfg;
	int		scbh;			/* scb cubby handle */
	struct wl_timer	*host_clk_sync_timer;
	bool		host_clk_sync_timer_enable;
	bool		host_clk_sync_timer_init;
	bool		host_clk_sync_use_est;
	uint32		host_clk_sync;
	uint32		host_clk_sync_timer_cnt;
	uint32		host_clk_sync_timer_est_lo;
	uint32		host_clk_sync_timer_est_hi;
	uint32		host_clk_sync_ctrl_id;
	/* relationship between hw clock ticks and tsf ticks */
	uint32		host_clk_sync_clk_freq;
	/* 0 = no update, N = update and print debug info up to N-1 times */
	uint32		host_clk_sync_update;
	/* how many updates so far */
	uint32		host_clk_sync_update_cnt;
	uint32		host_clk_sync_state;
	int32		host_clk_sync_a;
	int32		host_clk_sync_a_update_step;
	int32		host_clk_sync_b;
	int32		host_clk_sync_b_update_step;
	uint32		host_clk_sync_lr_n;
	int32		host_clk_sync_lr_x[K_HOST_CLK_SYNC_LR_MAX_N];
	int32		host_clk_sync_lr_y[K_HOST_CLK_SYNC_LR_MAX_N];
	uint32		host_clk_sync_hw_clk;
	uint32		host_clk_sync_hw_clk_start;
	uint32		host_clk_sync_tsf_start;
	uint32		host_clk_sync_tsf;
	uint32		host_clk_sync_tsf_phase;
	uint32		host_clk_sync_tsf_offset;
	int32		host_clk_sync_err_reset_val;
	uint32		host_clk_sync_err_reset_cnt;
	/* Protocol fields */
	uint8		astp_frame_hdr;
	uint8		astp_frame_sts;
	uint32		host_clk_sync_gpio_est_mask;
	uint32		host_clk_sync_gpio_est_bit_pos;
	uint32		host_clk_sync_gpio_raw_mask;
	uint32		host_clk_sync_gpio_clkref_mask;
	uint32		host_clk_sync_gpio_clk;
	/* stats */
	int32		host_clk_sync_err_cnt;
	int32		host_clk_sync_err_acc;
	int32		host_clk_sync_min_err;
	int32		host_clk_sync_max_err;
	uint32		host_clk_sync_missed_updates;
	uint32		host_clk_sync_last_upd_time;
	uint32		host_clk_sync_curr_upd_time;
	uint32		host_clk_freq;
	uint32		host_clk_sync_tsf_lo;
	uint32		host_clk_sync_tsf_hi;
};

/*
*****************************************************************************
* Function:   host_clk_sync_doiovar
*
* Purpose:    Handles Host Clock Sync related IOVars.
*
* Parameters: IOVARs specific data
*
* Returns:    0 on success.
*****************************************************************************
*/
static int
host_clk_sync_doiovar(
	void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *p, uint plen, void *a,
	 int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_host_clk_sync_info_t *info        = (wlc_host_clk_sync_info_t *)hdl;
	int32                    int_val      = 0;
	uint32                   bcmerror     = 0;
#ifdef WL_HOST_CLK_SYNC_NIC
	uint32			 tsf_lo;
	uint32			 tsf_hi;
#endif /* WL_HOST_CLK_SYNC_NIC */

	if (plen >= (int)sizeof(int_val)) {
		bcopy(p, &int_val, sizeof(int_val));
	}
	switch (actionid) {
	case IOV_GVAL(IOV_HOST_CLK_SYNC):
#ifndef WL_HOST_CLK_SYNC_NIC
		((uint32*)a)[0] = info->host_clk_sync_tsf_lo;
		((uint32*)a)[1] = info->host_clk_sync_tsf_hi;
#else
		wlc_read_tsf(info->wlc, &tsf_lo, &tsf_hi);
		((uint32*)a)[0] = hton32(tsf_lo);
		((uint32*)a)[1] = hton32(tsf_hi);
#endif /* !WL_HOST_CLK_SYNC_NIC */
		break;

	case IOV_SVAL(IOV_HOST_CLK_SYNC_CAP):
		wlc_host_clk_sync_cap_tstamp(info);
		break;

	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}
	return bcmerror;
}

void
BCMATTACHFN(wlc_host_clk_sync_detach)(wlc_host_clk_sync_info_t *info)
{
	wlc_info_t	*wlc;

	if (info == NULL)
		return;

	wlc = (wlc_info_t *)info->wlc;

	wlc_module_unregister(wlc->pub, "hostclksync", info);

	if (info->host_clk_sync_timer) {
		if (!wl_del_timer(wlc->wl, info->host_clk_sync_timer)) {
			info->host_clk_sync_timer_enable = FALSE;
			wl_free_timer(wlc->wl, info->host_clk_sync_timer);
		}
	}

	MFREE(WLCOSH(info), info, sizeof(wlc_host_clk_sync_info_t));

}

wlc_host_clk_sync_info_t *
BCMATTACHFN(wlc_host_clk_sync_attach)(wlc_info_t *wlc)
{
	wlc_host_clk_sync_info_t	*info;
	uint			unit;

	unit = wlc->pub->unit;
	BCM_REFERENCE(unit);

	/* Allocate host clock private info struct. */
	info = MALLOC(wlc->osh, sizeof(wlc_host_clk_sync_info_t));
	if (info == NULL) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	/* Init host clock private info struct. */
	bzero(info, sizeof(wlc_host_clk_sync_info_t));
	info->wlc = wlc;

	/* NVRAM data is reclaimed after attach */
#ifndef WL_HOST_CLK_SYNC_NIC
	wlc_host_clk_sync_configure(info);
	wlc_host_clk_sync_gpio_init(info);
#endif /* !WL_HOST_CLK_SYNC_NIC */

	/* Register TSF sync module. */
	if (wlc_module_register(
			wlc->pub, host_clk_sync_iovars, "hostclksync",
			info, host_clk_sync_doiovar,
			NULL, wlc_host_clk_sync_up, wlc_host_clk_sync_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* Read the host CPU clock frequency */
	info->host_clk_freq = 90;

	return (info);

fail:
	if (info != NULL)
		MFREE(WLCOSH(info), info, sizeof(wlc_host_clk_sync_info_t));

	WL_ERROR(("Host Clock Sync module init fail\n"));
	return (NULL);
}

static int
wlc_host_clk_sync_up(void *ctx_info)
{
	wlc_host_clk_sync_info_t	*info = (wlc_host_clk_sync_info_t *)ctx_info;
	int				err = 0;

	wlc_host_clk_sync_init(info);

	return err;
}

static int
wlc_host_clk_sync_down(void *ctx_info)
{
	wlc_host_clk_sync_info_t	*info = (wlc_host_clk_sync_info_t *)ctx_info;
	int				err = 0;

	wlc_host_clk_sync_reset(info);
	return err;
}

typedef struct
{
	ushort	x0;
	ushort	x1;
	ushort	x2;
	short	x3;
} VeryLong;

static inline long
wlc_abs(long x)
{
	if (x < 0)
		x = -x;
	return x;
}

static int64
wlc_int64_shift(int64 x, int s)
{
	uint32 u;
	int32 v;

	if (s > 32)
		s = 32;
	else if (s < -32)
		s = -32;
	u = (uint32)(x & 0xFFFFFFFFu);
	v = (int32) (x >> 32);
	if (s > 0) {
		v = (v << s) | ((u >> (32 - s)) & ((1 << s)-1));
		u = (u << s);
	} else if (s < 0) {
		s = -s;
		u = (u >> s) | (v << (32 - s));
		v = (v >> s);
	}
	x  = (int64)u & 0xFFFFFFFF;
	x |= (int64)v << 32;
	return x;
}

static void
wlc_int64_div(int64 a, int64 b, int scale, int32* dst)
{
	int	shiftX;
	int	shiftY;
	long	temp;
	long	numerator;
	long	denominator;
	VeryLong x;
	VeryLong y;

	/*
	 * We have to normalize x & y
	 */
	shiftX = 0;
	shiftY = 0;

	y.x0 = (ushort)(b & 0xFFFFu);
	y.x1 = (ushort)(b >> 16) & 0xFFFFu;
	y.x2 = (ushort)(b >> 32) & 0xFFFFu;
	y.x3 =  (short)(b >> 48);
	x.x0 = (ushort)(a & 0xFFFFu);
	x.x1 = (ushort)(a >> 16) & 0xFFFFu;
	x.x2 = (ushort)(a >> 32) & 0xFFFFu;
	x.x3 =  (short)(a >> 48);

	numerator = ((long)x.x3 << 16) + (ulong)x.x2;
	temp = (((ulong)x.x1 << 16) + (ulong)x.x0);
	if ((temp | numerator) == 0)
	{
		*dst = 0;
		return;
	}

	while (wlc_abs(numerator) < 0x3FFFFFFF)
	{
		numerator = (numerator << 1) + ((temp >> 31) & 1);
		temp <<= 1;
		shiftX++;
	}

	denominator = ((long)y.x3 << 16) + (ulong)y.x2;
	temp = (((ulong)y.x1 << 16) + (ulong)y.x0);
	if ((temp | denominator) == 0)
	{
		*dst = 0x7FFFFFFF;
		return;
	}

	if (wlc_abs(denominator) < 0x3FFF)
	{
		while (wlc_abs(denominator) < 0x3FFF)
		{
			denominator = (denominator << 1) + ((temp >> 31) & 1);
			temp <<= 1;
			shiftY++;
		}
	} else {
		while (wlc_abs(denominator) >= 0x7FFF)
		{
			denominator >>= 1;
			shiftY--;
		}
	}

	temp = numerator / denominator;

	shiftX -= shiftY;
	shiftX -= scale;
	if (shiftX >= 0)
		temp >>= shiftX;
	else
		temp <<= -shiftX;

	*dst = (int32)temp;
}

static void
wlc_host_clk_sync_lr(int n, int32* p_x, int32* p_y, int32* p_m, int32* p_b)
{
	int	i;
	int64	acc_x;
	int64	acc_y;
	int64	m_x;
	int64	m_y;
	int64	v_x;
	int64	v_y;
	int64	m;
	int64	b;
	int64	mask;
	int64	sign;
	int32	z;

	acc_x = acc_y = 0;
	for (i = 0; i < n; i++) {
		acc_x += (int64)p_x[i];
		acc_y += (int64)p_y[i];
	}
	wlc_int64_div((acc_x + (n/2)), n, 0, &z);
	m_x = (int64)z;
	wlc_int64_div((acc_y + (n/2)), n, 0, &z);
	m_y = (int64)z;

	HOST_CLK_SYNC_PRINT(("m_x %lu m_y %lu\n", m_x, m_y));
	acc_x = acc_y = 0;
	for (i = 0; i < n; i++) {
		v_x = (int64)p_x[i] - m_x;
		v_y = (int64)p_y[i] - m_y;
		acc_x += v_x * v_x;
		acc_y += v_x * v_y;
	}

	HOST_CLK_SYNC_PRINT(("v_x**2 %lu v_x*v_y %lu\n", acc_x, acc_y));

	/* msb in not the most efficient way */
	mask = ((uint64)0xFFFFFFFF << 32);
	sign = mask;
	if (acc_y >= 0)
		sign = 0;
	i = 0;

	while (i < 32) {
		if ((acc_y & mask) == sign)
			break;
		i++;
		mask = mask << 1;
		sign = sign << 1;
	}

	i += (K_HOST_CLK_SYNC_A_SHIFT + 1 - 32);
	if (i < 0)
		i = 0;
	acc_y = wlc_int64_shift(acc_y, (K_HOST_CLK_SYNC_A_SHIFT - i));
	acc_x = wlc_int64_shift(acc_x, -i);
	wlc_int64_div(acc_y, acc_x, 0, &z);

	m = (int64)z;
	b = m_y - ((m_x * m + (1<<(K_HOST_CLK_SYNC_A_SHIFT-1)))>>K_HOST_CLK_SYNC_A_SHIFT);

	HOST_CLK_SYNC_PRINT(("m %lu b %lu\n", m, b));

	*p_m = (int32)m;
	*p_b = (int32)b;
}

void
wlc_host_clk_sync_reset(wlc_host_clk_sync_info_t *info)
{
	if (info == NULL) {
		return;
	}
	info->host_clk_sync_a = 0;
	info->host_clk_sync_b = 0;
	info->host_clk_sync_tsf = 0;
	info->host_clk_sync_tsf_phase = 0;
	info->host_clk_sync_update_cnt = 0;
	info->host_clk_sync_state = K_HOST_CLK_SYNC_FSM_INIT;
	info->host_clk_sync_use_est = FALSE;
}

#ifndef WL_HOST_CLK_SYNC_NIC
void
wlc_host_clk_sync_configure(wlc_host_clk_sync_info_t *info)
{
	int8			raw = 0;
	int8			est = 0;
	char			*vars = info->wlc->pub->vars;

	if (getvar(vars, host_clk_sync_gpio_est)) {
		est = getintvar(vars, host_clk_sync_gpio_est);
		info->host_clk_sync_gpio_est_mask = (1 << est);
		info->host_clk_sync_gpio_est_bit_pos = est;
	} else {
		info->host_clk_sync_gpio_est_mask = 0;
	}

	if (getvar(vars, host_clk_sync_gpio_raw)) {
		raw = getintvar(vars, host_clk_sync_gpio_raw);
		info->host_clk_sync_gpio_raw_mask = (1 << raw);
	} else {
		info->host_clk_sync_gpio_raw_mask = 0;
	}

	HOST_CLK_SYNC_PRINT(("Init: Updt: %d Est: 0x%x Raw: 0x%x\n",
	info->host_clk_sync_update, info->host_clk_sync_gpio_est_mask,
	info->host_clk_sync_gpio_raw_mask, 0));
}
#endif /* !WL_HOST_CLK_SYNC_NIC */

static void
wlc_host_clk_sync_init(wlc_host_clk_sync_info_t *info)
{

	info->host_clk_sync_timer_cnt = 0;
	info->host_clk_sync_update = 1;
	info->host_clk_sync_clk_freq = 137;
	info->host_clk_sync_ctrl_id = K_HOST_CLK_SYNC_CTRL_NULL;
	wlc_host_clk_sync_reset(info);

	info->host_clk_sync_a_update_step = 128;
	info->host_clk_sync_b_update_step =  32;
	info->host_clk_sync_err_reset_val = 20;
	info->host_clk_sync_err_reset_cnt = 0;
	info->host_clk_sync_timer_est_hi = 0;
	info->host_clk_sync_missed_updates = 0;
	info->host_clk_sync_last_upd_time = 0;

	info->astp_frame_hdr   = 0;
	info->host_clk_sync_use_est = 0;
	info->astp_frame_sts   = 0;
}

static uint32
wlc_host_clk_sync_get_tsf_est(
	wlc_host_clk_sync_info_t *info, uint32 hw_clk, uint32* p_tsf,
	uint32* p_tsf_phase)
{
	uint32				t_hw_clk;
	uint32				delta_clk;
	uint32				delta_tsf;
	uint32				delta_tsf_phase;
	uint32				tsf;
	uint32				tsf_phase;
	uint32				tsf_est;
	uint32				a_shift;
	int32				a;
	int32				b;

	a = (info->host_clk_sync_a >> K_HOST_CLK_SYNC_A_UPDATE_SHIFT);
	b = (info->host_clk_sync_b >> K_HOST_CLK_SYNC_B_UPDATE_SHIFT);
	tsf = info->host_clk_sync_tsf;
	tsf_phase = info->host_clk_sync_tsf_phase;
	t_hw_clk = info->host_clk_sync_hw_clk;
	a_shift = (1 << K_HOST_CLK_SYNC_A_SHIFT);

	delta_clk = (uint32)(((uint64)(hw_clk - t_hw_clk) *
		(uint64)((a_shift) + a)) >> K_HOST_CLK_SYNC_A_SHIFT);

	delta_tsf = delta_clk / info->host_clk_sync_clk_freq;
	delta_tsf_phase = delta_clk % info->host_clk_sync_clk_freq;
	tsf += delta_tsf;
	tsf_phase += delta_tsf_phase;
	if (tsf_phase >= info->host_clk_sync_clk_freq) {
		tsf_phase -= info->host_clk_sync_clk_freq;
		tsf++;
	}

	tsf_est = tsf + b;
	*p_tsf = tsf;
	*p_tsf_phase = tsf_phase;

	return tsf_est;
}

static uint32
wlc_host_clk_sync_get_tsf_offset(wlc_host_clk_sync_info_t *info)
{
	uint32	tsf_offset_lo = 0;

	return tsf_offset_lo;
}

static void
wlc_host_clk_sync_fsm_init(wlc_host_clk_sync_info_t *info, uint32 hw_clk, uint32 tsf)
{
	info->host_clk_sync_lr_n = 1;
	info->host_clk_sync_lr_x[0] = 0;
	info->host_clk_sync_lr_y[0] = 0;
	info->host_clk_sync_hw_clk_start = hw_clk;
	info->host_clk_sync_tsf_start = tsf;
	info->host_clk_sync_state = K_HOST_CLK_SYNC_FSM_TRACK_INIT;

	info->host_clk_sync_err_cnt = 0;
	info->host_clk_sync_err_acc = 0;
	info->host_clk_sync_max_err = 0;
	info->host_clk_sync_min_err = 0;

	return;
}

static void
wlc_host_clk_sync_fsm_track(
			wlc_host_clk_sync_info_t *info,
			uint32 *hw_clk, uint32 *tsf, uint32 *tsf_est,
			int32 *e_a, int32 *e_b, int32 *lr_a, int32 *lr_b)
{
	wlc_info_t	*wlc = (wlc_info_t *)info->wlc;
	d11regs_t	*regs = wlc->regs;
	uint32		hardware_clock = *hw_clk;
	if (info->host_clk_sync_state != K_HOST_CLK_SYNC_FSM_TRACK_INIT) {
		/* update tsf estimate and compute error for updating b */
		*tsf_est =
			wlc_host_clk_sync_get_tsf_est(
				info, hardware_clock, &(info->host_clk_sync_tsf),
				&(info->host_clk_sync_tsf_phase));
		*e_b = *tsf - *tsf_est;
		HOST_CLK_SYNC_PRINT(("e_b %d tsf %lu tsf_est %lu\n", e_b, tsf, tsf_est));
		info->host_clk_sync_b += (info->host_clk_sync_b_update_step * (*e_b));

		/* In est mode we only keep the low 32 bits re-read the MSB 32 bits */
		info->host_clk_sync_timer_est_hi = R_REG(wlc->osh, &regs->tsf_timerhigh);

		if (wlc_abs((long)*e_b) > info->host_clk_sync_err_reset_val) {
			/* something is wrong  -- restart */
			info->host_clk_sync_err_reset_cnt++;
			wlc_host_clk_sync_reset(info);
			return;
		}
	}
	info->host_clk_sync_lr_x[info->host_clk_sync_lr_n] =
	*hw_clk - info->host_clk_sync_hw_clk_start;
	info->host_clk_sync_lr_y[info->host_clk_sync_lr_n] = *e_a;
	info->host_clk_sync_lr_n++;
	if (info->host_clk_sync_lr_n == K_HOST_CLK_SYNC_LR_N) {
		wlc_host_clk_sync_lr(
				K_HOST_CLK_SYNC_LR_N, info->host_clk_sync_lr_x,
				info->host_clk_sync_lr_y, lr_a, lr_b);
		if (info->host_clk_sync_state == K_HOST_CLK_SYNC_FSM_TRACK_INIT) {
			info->host_clk_sync_a = *lr_a << K_HOST_CLK_SYNC_A_UPDATE_SHIFT;
			info->host_clk_sync_tsf = *tsf;
			info->host_clk_sync_tsf_phase = 0;
		} else {
			info->host_clk_sync_a += (info->host_clk_sync_a_update_step * (*lr_a));
		}
		info->host_clk_sync_state = K_HOST_CLK_SYNC_FSM_TRACK_LR_INIT;
	} else if (info->host_clk_sync_state != K_HOST_CLK_SYNC_FSM_TRACK_INIT) {
		info->host_clk_sync_state = K_HOST_CLK_SYNC_FSM_TRACK;
	}

	return;
}

void
wlc_host_clk_sync_update(wlc_host_clk_sync_info_t *info)
{
	wlc_info_t			*wlc  = (wlc_info_t *)info->wlc;
	uint32				hw_clk;
	uint32				delta_clk;
	uint32				tsf;
	uint32				tsf_z;
	uint32				tsf_est;
	d11regs_t			*regs = wlc->regs;
	uint32				delta_tsf;
	uint32				host_clk_sync_state;
	int32				e_a;
	int32				e_b;
	int32				lr_a;
	int32				lr_b;
	int32				a;
	int32				b;

	if (!(si_iscoreup(wlc->pub->sih)))  {
		HOST_CLK_SYNC_PRINT(("Core is not UP\n"));
		return;
	}

	info->astp_frame_sts |= (1 << 5);
	if (!info->host_clk_sync_update) {
		wlc_host_clk_sync_reset(info);
		return;
	}

	/* wait until lsb of tsf changes and then record timestamp */
	info->host_clk_sync_tsf_offset = wlc_host_clk_sync_get_tsf_offset(info);
		tsf_z = R_REG(wlc->osh, &regs->tsf_timerlow) + info->host_clk_sync_tsf_offset;

	do {
		tsf = R_REG(wlc->osh, &regs->tsf_timerlow) + info->host_clk_sync_tsf_offset;
	} while (!(tsf ^ tsf_z));

	/* We need to measure the H/W clock as close to the TSF measurement as possible */
	OSL_GETCYCLES(hw_clk);
	tsf_z = tsf;

	a = (info->host_clk_sync_a >> K_HOST_CLK_SYNC_A_UPDATE_SHIFT);
	b = (info->host_clk_sync_b >> K_HOST_CLK_SYNC_B_UPDATE_SHIFT);
	tsf_est = tsf;
	e_a = e_b = 0;

	if ((info->host_clk_sync_state != K_HOST_CLK_SYNC_FSM_INIT) &&
	   (info->host_clk_sync_state != K_HOST_CLK_SYNC_FSM_TRACK_LR_INIT)) {
		/* error for updating a */
		delta_clk =
			(uint32)(((uint64)(hw_clk - info->host_clk_sync_hw_clk_start) *
			(uint64)((1 << K_HOST_CLK_SYNC_A_SHIFT) + a)) >> K_HOST_CLK_SYNC_A_SHIFT);

		delta_tsf = info->host_clk_sync_clk_freq * (tsf - info->host_clk_sync_tsf_start);
		e_a = delta_tsf - delta_clk;
		HOST_CLK_SYNC_PRINT(("delta_clk %lu delta_tsf %lu\n", delta_clk, delta_tsf));
	}

	/* fsm */
	host_clk_sync_state = info->host_clk_sync_state; /* for dbg printf */
	lr_a = lr_b = 0; /* for dbg printf */
	switch (info->host_clk_sync_state) {
	default:

	case K_HOST_CLK_SYNC_FSM_INIT:
		wlc_host_clk_sync_fsm_init(info, hw_clk, tsf);
		break;

	case K_HOST_CLK_SYNC_FSM_TRACK_LR_INIT:
		info->host_clk_sync_lr_n = 0;
		info->host_clk_sync_hw_clk_start = hw_clk;
		info->host_clk_sync_tsf_start = tsf;
		/* fall through */
	case K_HOST_CLK_SYNC_FSM_TRACK_INIT:
		/* fall through */
	case K_HOST_CLK_SYNC_FSM_TRACK:
		wlc_host_clk_sync_fsm_track(info, &hw_clk,
			&tsf, &tsf_est, &e_a, &e_b, &lr_a, &lr_b);
		break;
	}
	info->host_clk_sync_hw_clk = hw_clk;
	info->host_clk_sync_update_cnt++;

}

#ifndef WL_HOST_CLK_SYNC_NIC
void
wlc_host_clk_sync_gpio_init(wlc_host_clk_sync_info_t *info)
{
	wlc_hw_info_t	*wlc_hw = (wlc_hw_info_t *)info->wlc->hw;
	uint32		gpio_clk_mask = 0xFFFFFFFF;

	info->host_clk_sync_gpio_clk =
		info->host_clk_sync_gpio_est_mask |
		info->host_clk_sync_gpio_raw_mask;
	gpio_clk_mask = info->host_clk_sync_gpio_clk;

	/* Make gpios come from chipcommon */
	/* JTAG_SEL = 0 */
	si_gci_chipcontrol(wlc_hw->sih, CC_GCI_CHIPCTRL_06,
		CC_GCI_06_JTAG_SEL_MASK,
		(1 << CC_GCI_06_JTAG_SEL_SHIFT));
	/* Func Sel = 1 */
	si_gci_set_functionsel(wlc_hw->sih, CC4335_PIN_GPIO_03, CC4335_FNSEL_SAMEASPIN);
	/* GCI Chip Add Reg offset 0xc40 is 0 */
	si_gci_indirect(wlc_hw->sih, 0, OFFSETOF(chipcregs_t, gci_indirect_addr),
		0xFFFFFFFF, 0);
	/* Chip control bits 12 and 16 set to 1 */
	si_gci_chipcontrol(wlc_hw->sih, CC_GCI_CHIPCTRL_00, 0xFFFFFFFF,	0x00001000);
	si_gpiopull(wlc_hw->sih, GPIO_PULLDN, 0xFFFF, 0);
	/* Enable gpios */
	si_gpiocontrol(wlc_hw->sih, gpio_clk_mask, 0,
		       GPIO_DRV_PRIORITY); /* assign to chipcommon */
	si_gpioouten(wlc_hw->sih, gpio_clk_mask, info->host_clk_sync_gpio_clk,
		     GPIO_DRV_PRIORITY); /* configure as output */
	si_gpioout(wlc_hw->sih, gpio_clk_mask, 0,
		   GPIO_DRV_PRIORITY); /* set low by dflt */
}
#endif /* !WL_HOST_CLK_SYNC_NIC */

static void
wlc_host_clk_sync_cap_tstamp(wlc_host_clk_sync_info_t *info)
{
#ifndef WL_HOST_CLK_SYNC_NIC
	wlc_info_t		*wlc = (wlc_info_t *)info->wlc;
	uint32			tsf_lo;
	uint32			tsf_hi;
	wlc_hw_info_t		*wlc_hw = wlc->hw;
	static uint32		gpio_clk = 0xFFFFFFFF;
	static uint32		gpio_clk_mask = 0xFFFFFFFF;
	static uchar		host_clk_sync_gpio_clk_data = FALSE;

	if (!host_clk_sync_gpio_clk_data) {
		host_clk_sync_gpio_clk_data = TRUE;
		gpio_clk_mask =
			info->host_clk_sync_gpio_est_mask |
			info->host_clk_sync_gpio_raw_mask;
	}
	if (si_iscoreup(wlc->pub->sih)) {

#ifdef WLMCNX
		if (wlc->mcnx) {
			wlc_mcnx_read_tsf(wlc->mcnx, wlc->cfg, &tsf_lo, &tsf_hi);
#else
			wlc_read_tsf(info->wlc, &tsf_lo, &tsf_hi);
#endif // endif
			/* Toggle the GPIO to indicate capture time */
			/* Clock rising edge */
			si_gpioout(wlc_hw->sih, gpio_clk_mask, gpio_clk, GPIO_DRV_PRIORITY);
			/* Capture timestamp */
			info->host_clk_sync_tsf_lo = hton32(tsf_lo);
			info->host_clk_sync_tsf_hi = hton32(tsf_hi);

			/* Toggle clock low */
			si_gpioout(wlc_hw->sih, gpio_clk_mask, 0, GPIO_DRV_PRIORITY);
#ifdef WLMCNX
		}
#endif // endif
	}
#else
	return;
#endif /* !WL_HOST_CLK_SYNC_NIC */
}
#endif /* WL_HOST_CLK_SYNC */
