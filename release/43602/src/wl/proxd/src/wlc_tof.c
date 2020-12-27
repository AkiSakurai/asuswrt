/*
 * TOF based proximity detection implementation for Broadcom 802.11 Networking Driver
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_tof.c 467163 2014-04-02 17:41:32Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <wlc_hrt.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scan.h>
#include <wl_export.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_pcb.h>

#include <wlc_pdsvc.h>
#include <wlc_pddefs.h>
#include <wlc_pdmthd.h>
#include <wlc_fft.h>

#undef TOF_DBG

#define MAX_CORDIC32_ITER		17 /* maximum number of cordic32 iterations */
#define MAX_CORDIC32_NFRAC		17 /* Number of fractional bits in atan table */
#define CORDIC32_LOG2_PI_OVER_TWO	18 /* LOG2 PI over 2 */
#define CORDIC32_PI_OVER_TWO \
	(1<<CORDIC32_LOG2_PI_OVER_TWO) /* PI/2 as a fixed point number */
#define CORDIC32_PI			 \
	(CORDIC32_PI_OVER_TWO << 1)    /* PI as a fixed point number */
#define CORDIC32_NUM_FRAC_BITS_INTERNAL	29

#define k_log2_tof_rtd_adj_window_len	5
#define k_tof_rtd_adj_window_len	(1<<k_log2_tof_rtd_adj_window_len)

static int32 atan_tbl[MAX_CORDIC32_ITER] = {
	131072, 77376, 40884, 20753, 10417,
	5213, 2607, 1304, 652, 326, 163, 81, 41, 20, 10, 5, 3
};
/* Cordic calculation */
static int32
cordic(cint32 phasor)
{
	int32 x, y, z, prev_x, prev_y, prev_z, mu;
	int iter, signx, signy;

	x = phasor.i;
	y = phasor.q;
	z = 0;

	/* Figure out in which quadrant we are */
	signx = (x < 0)? -1 : 1;
	signy = (y <= 0)? -1 : 1;

	/* If x < 0, negate x and y (rotate 180 degrees) */
	/* This rotates into the 1st or 4th quadrant */
	/* CORDIC only works in 1st and 4th quadrant */
	x = signx * x;
	y = signx * y;

	/* CORDIC iteration */
	iter = 0;
	while (iter < MAX_CORDIC32_ITER)
	{
		prev_x = x;
		prev_y = y;
		prev_z = z;
		mu = (y < 0)? 1 : -1;
		z = prev_z - mu * atan_tbl[iter];
		x = prev_x - mu * ROUND(prev_y, iter);
		y = prev_y + mu * ROUND(prev_x, iter);
		x = LIMIT(x, -(1<<CORDIC32_NUM_FRAC_BITS_INTERNAL),
			(1<<CORDIC32_NUM_FRAC_BITS_INTERNAL)-1);
		y = LIMIT(y, -(1<<CORDIC32_NUM_FRAC_BITS_INTERNAL),
			(1<<CORDIC32_NUM_FRAC_BITS_INTERNAL)-1);
		iter++;
	}

	/* If in 2nd quadrant, output angle is (z + pi). */
	/* If in 3rd quadrant, output angle is (z - pi). */
	/* Otherwise, output angle is z. */
	if (signx < 0)
		z = z + CORDIC32_PI * signy;

	/* Limit angle to [-pi, pi) */
	z = LIMIT(z, -CORDIC32_PI, CORDIC32_PI);

	return z;
}
/* channel smoothing threshold adj calculation */
int
tof_rtd_adj(wlc_info_t *wlc, struct tof_rtd_adj_params *params)
{
	int32  tmp, theta, gd, gd_adj_ns, h_ts;
	cint32 acc, *H;
	int i, k, n1, i_l, n, nfft, nfft_over_2, rshift, max_log2_w, ret = 0;
	int32 w_int[k_tof_rtd_adj_window_len];
	int32 *w;
	int32 tmp1, tmp2, thresh = 0, mask;
	int wzero;
	int wlen;
	int thresh_idx = 0;

	if (params->bw == TOF_BW_20MHZ) {
		rshift = TOF_BW_20MHZ_INDEX;
		h_ts = (Q1_NS << 2);
		max_log2_w = 7;
		nfft = TOF_NFFT_20MHZ;
		i_l = 28;       /* for legacy this is 26, for vht-20 this is 28 */
	} else if (params->bw == TOF_BW_40MHZ) {
		rshift = TOF_BW_40MHZ_INDEX;
		h_ts = (Q1_NS << 1);
		max_log2_w = 6;
		nfft = TOF_NFFT_40MHZ;
		i_l = 58;
	} else if (params->bw == TOF_BW_80MHZ) {
		rshift = TOF_BW_80MHZ_INDEX;
		h_ts = Q1_NS;
		max_log2_w = 5;
		nfft = TOF_NFFT_80MHZ;
		i_l = 122;
	} else {
		return BCME_ERROR;
	}

	if (params->w_ext) {
		/* HW */

		gd_adj_ns = params->gd_ns; /* Q1 */
		params->gd_ns = (gd_adj_ns + 1) >> 1;
		w = params->w_ext;
		wlen = params->w_len;
		wzero = params->w_offset;
		thresh = 0;
		for (k = 0; k < wlen; k++) {
			if (w[k] > thresh) {
				thresh = w[k];
				thresh_idx = k;
			}
		}

		thresh = ((params->thresh_scale*thresh) >> params->thresh_log2);

	} else {
		  /* SW */

		nfft_over_2 = (nfft >> 1);
		H = (cint32*)params->H;

#ifdef TOF_DBG
		printf("H1=[\n");
		for (i = 0; i < nfft; i++) {
			printf("%d + i*%d\n", H[i].i, H[i].q);
		}
		printf("];\n");
#endif


		/* undo rotation */
		if (params->bw == TOF_BW_40MHZ) {
			for (i = 0; i < TOF_NFFT_40MHZ; i++) {
				tmp = H[i].i;
				H[i].i = H[i].q;
				H[i].q = -tmp;
			}
		} else if (params->bw == TOF_BW_80MHZ) {
			for (i = 0; i < NFFT_BASE; i++) {
				H[i].i = -H[i].i;
				H[i].q = -H[i].q;
			}
		}

#ifdef TOF_DBG
		printf("H2=[\n");
		for (i = 0; i < nfft; i++) {
			printf("%d + i*%d\n", H[i].i, H[i].q);
		}
		printf("];\n");
#endif

		/* group delay */
		acc.i = 0; acc.q = 0;
		n = (i_l << 1);
		i = nfft_over_2 - i_l;
		while (n-- > 0) {
			acc.i += ((H[i].i * H[i+1].i) + (H[i].q * H[i+1].q)) >> rshift;
			acc.q += ((H[i].q * H[i+1].i) - (H[i].i * H[i+1].q)) >> rshift;
			i++;
		}
		tmp = ((acc.i >> 28) ^ (acc.q >> 28)) & 0xf;
		if ((tmp != 0) && (tmp != 0xf)) {
			acc.i = acc.i >> 4;
			acc.q = acc.q >> 4;
		}
		theta = cordic(acc);
		gd = theta * nfft;
		gd_adj_ns = gd;
		gd = (gd >> (CORDIC32_LOG2_PI_OVER_TWO+2));
#ifdef TOF_DBG
		printf("gd=%d;\n", gd);
#endif
		tmp = (gd_adj_ns >> 28) & 0xf;
		if ((tmp != 0) && (tmp != 0xf)) {
			tmp = (gd_adj_ns >> 4) * h_ts;
			gd_adj_ns = ROUND(tmp, CORDIC32_LOG2_PI_OVER_TWO+2+4+1);
		} else {
			tmp = gd_adj_ns * h_ts;
			gd_adj_ns = ROUND(tmp, CORDIC32_LOG2_PI_OVER_TWO+2+1);
		}
		params->gd_ns = gd_adj_ns;

		gd_adj_ns = gd * h_ts;  /* Q1 */
		w = w_int;
		wlen = params->w_len;
		if (wlen > k_tof_rtd_adj_window_len)
			wlen = k_tof_rtd_adj_window_len;
		wzero = params->w_offset;

		if (params->bw == TOF_BW_80MHZ) {
			/* output is time reversed impulse response */
			ret = FFT256(wlc->osh, H, H);
		} else if (params->bw == TOF_BW_40MHZ) {
			max_log2_w -= 1;
			/* output is time reversed impulse response */
			ret = FFT128(wlc->osh, H, H);
		} else {
			/* output is time reversed impulse response */
			max_log2_w -= 2;
			ret = FFT64(wlc->osh, H, H);
		}

		if (ret)
		{
			return ret;
		}

#ifdef TOF_DBG
		printf("H3=[\n");
		for (i = 0; i < nfft; i++) {
			printf("%d + i*%d\n", H[i].i, H[i].q);
		}
		printf("];\n");
#endif

		do {
			thresh = 0;
			for (i = -gd + wzero, k = 0; k < wlen; i--, k++) {
				n1 = (i & (nfft - 1));
				tmp1 = H[n1].i;
				tmp2 = H[n1].q;
				tmp = tmp1*tmp1 + tmp2*tmp2;
				if (tmp > thresh) {
					thresh = tmp;
					thresh_idx = k;
				}
				w[k] = tmp;
			}
			thresh = ((params->thresh_scale*thresh) >> params->thresh_log2);
			if (w[0] < (thresh - (thresh >> 1)))
				break;
			wzero++;
		} while (wzero < wlen);

#ifdef RSSI_REFINE
		for (i = 0; i < nfft && params->p_A; i++) {
			tmp1 = H[i].i;
			tmp2 = H[i].q;
			tmp = tmp1 * tmp1 + tmp2 * tmp2;
			params->p_A[i] = tmp;
		}
#endif

#ifdef TOF_DBG
		printf("w=[\n");
		for (i = 0; i < wlen; i++) {
			printf("%d\n", w[i]);
		}
		printf("];\n");
		printf("wzero = %d\n", wzero);
#endif
	}

	if (w[0] >= thresh)
		return BCME_ERROR; /* something wrong -- don't use this frame */

	/* simple threshold crossing for now -- maybe good enough for LOS.
	   find first crossing in window.
	*/
	k = thresh_idx;
	max_log2_w += k_log2_tof_rtd_adj_window_len;
	mask = (0xffffffff << (32 - max_log2_w));
	tmp = 0;
	while (k > 0) {
		if ((w[k] >= thresh) && (w[k-1] < thresh)) {
			int32 w1, w0, th;
			int shft = 0;

			/* avoid overflow */
			w1 = w[k];
			w0 = w[k-1];
			th = thresh;
			while ((shft < 31) && (w1 & mask)) {
				shft++;
				/* fprintf(stderr,"SHFT %d %d\n",max_log2_w,shft); */
				w1 = w1 >> 1;
				w0 = w0 >> 1;
				th = th >> 1;
			};
			tmp = h_ts*((k - wzero)*(w1 - w0) + (th - w0))/(w1 - w0);
		}
		k--;
	}

	gd_adj_ns += tmp;
	gd_adj_ns = (gd_adj_ns + 1) >> 1;

	params->adj_ns = gd_adj_ns;
	return BCME_OK;
}
#ifdef RSSI_REFINE
#define RSSI_VHTSCALE	100
#define RSSI_NFFT_RATIO	(RSSI_VHTSCALE*256/8)
/* Find crossing point */
int32 find_crossing(int32* T, int max, int nfft, uint32 threshold)
{
	int i;
	uint32 vhigh, vth, z, rt;
	int32 delta;

	threshold = (1 << (TOF_MAXSCALE - threshold));
	for (i = 0; i < nfft; i++) {
		if (T[i] > threshold)
			break;
	}
	if (i == nfft || i == 0) i = 1;

	vhigh = T[i] - T[i-1];
	vth = threshold - T[i-1];
	z = vhigh / 10000000;
	if (z > 1) {
		vhigh /= z;
		vth /= z;
	}

	rt = (vth *RSSI_VHTSCALE) / vhigh;
	rt += RSSI_VHTSCALE * (i-1);
	delta = (nfft * RSSI_VHTSCALE / 2) - rt;
	/* rounding up to the nearest integer */
	delta = (delta * RSSI_NFFT_RATIO)/nfft;

	return delta;
}
#endif /* RSSI_REFINE */
