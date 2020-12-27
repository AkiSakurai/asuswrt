/*
 * Broadcom Proprietary and Confidential. Copyright (C) 2016,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 * $Id:$
 */

#include <stdio.h>

#define HSPOT_DEBUG_ERROR	0x0001
#define HSPOT_DEBUG_WARNING	0x0002
#define HSPOT_DEBUG_INFO	0x0004

extern int hspot_debug_level;

#define HS20_ERROR(fmt, arg...) \
	do { if (hspot_debug_level & HSPOT_DEBUG_ERROR) \
			printf("HS20 >> %s (%d): "fmt, __FUNCTION__, __LINE__, ##arg);} while (0)

#define HS20_WARNING(fmt, arg...) \
	do { if (hspot_debug_level & HSPOT_DEBUG_WARNING) \
			printf("HS20 >> %s (%d): "fmt, __FUNCTION__, __LINE__, ##arg);} while (0)

#define HS20_INFO(fmt, arg...) \
	do { if (hspot_debug_level & HSPOT_DEBUG_INFO) \
			printf("HS20 >> %s (%d): "fmt, __FUNCTION__, __LINE__, ##arg);} while (0)
