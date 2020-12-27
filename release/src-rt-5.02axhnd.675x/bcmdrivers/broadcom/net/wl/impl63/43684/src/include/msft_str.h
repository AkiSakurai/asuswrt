/*
 * Windows implementations of Broadcom secure string functions
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
 * $Id: msft_str.h 624262 2016-03-11 02:42:29Z $
 */

#ifndef __MSFT_STR_H__
#define __MSFT_STR_H__

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif // endif

int strcpy_s(char *dst, size_t noOfElements, const char *src);
int strncpy_s(char *dst, size_t noOfElements, const char *src, size_t count);
int strcat_s(char *dst, size_t noOfElements, const char *src);
int sprintf_s(char *buffer, size_t noOfElements, const char *format, ...);
int vsprintf_s(char *buffer, size_t noOfElements, const char *format, va_list argptr);
int _vsnprintf_s(char *buffer, size_t noOfElements, size_t count, const char *format,
	va_list argptr);
int wcscpy_s(wchar_t *dst, size_t noOfElements, const wchar_t *src);
int wcscat_s(wchar_t *dst, size_t noOfElements, const wchar_t *src);

static INLINE int bcm_strcpy_s(
	char *dst,
	size_t noOfElements,
	const char *src)
{
	int ret;

	ret = (int)strcpy_s(dst, noOfElements, src);
	return ret;
}

static INLINE int bcm_strncpy_s(
	char *dst,
	size_t noOfElements,
	const char *src,
	size_t count)
{
	return (int)strncpy_s(dst, noOfElements, src, count);
}

static INLINE int bcm_strcat_s(
	char *dst,
	size_t noOfElements,
	const char *src)
{
	int ret;

	ret = (int)strcat_s(dst, noOfElements, src);
	return ret;
}

static INLINE int bcm_sprintf_s(
	char *buffer,
	size_t noOfElements,
	const char *format,
	...)
{
	va_list argptr;
	int ret;

	va_start(argptr, format);
	ret = vsprintf_s(buffer, noOfElements, format, argptr);
	va_end(argptr);
	return ret;
}

static INLINE int bcm_vsprintf_s(
	char *buffer,
	size_t noOfElements,
	const char *format,
	va_list argptr)
{
	return vsprintf_s(buffer, noOfElements, format, argptr);
}

static INLINE int bcm_vsnprintf_s(
	char *buffer,
	size_t noOfElements,
	size_t count,
	const char *format,
	va_list argptr)
{
	return _vsnprintf_s(buffer, noOfElements, count, format, argptr);
}

static INLINE int bcm_wcscpy_s(
	wchar_t *dst,
	size_t noOfElements,
	const wchar_t *src)
{
	int ret;

	ret = (int)wcscpy_s(dst, noOfElements, src);
	return ret;
}

static INLINE int bcm_wcscat_s(
	wchar_t *dst,
	size_t noOfElements,
	const wchar_t *src)
{
	int ret;

	ret = (int)wcscat_s(dst, noOfElements, src);
	return ret;
}

#ifdef __cplusplus
}
#endif // endif

#endif /* __MSFT_STR_H__ */
