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
 * $Id: wlc_fft.h 451999 2014-01-28 21:57:46Z $
 */
#ifndef _wlc_fft_h
#define _wlc_fft_h

#include <typedefs.h>
#include <wlc_types.h>
#include <bcmutils.h>
#include <osl.h>
#include <wlc_phy_int.h>

#define LIMIT(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
#define ROUND(x, s) (((x) >> (s)) + (((x) >> ((s) - 1)) & (s != 0)))

extern int FFT64(osl_t *osh, cint32 *inBuf, cint32 *outBuf);
extern int FFT128(osl_t *osh, cint32 *inBuf, cint32 *outBuf);
extern int FFT256(osl_t *osh, cint32 *inBuf, cint32 *outBuf);

#endif /* _wlc_fft_h */
