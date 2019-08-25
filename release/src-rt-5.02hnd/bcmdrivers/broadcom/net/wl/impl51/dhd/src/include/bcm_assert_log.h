/*
 * Global ASSERT Logging Public Interface
 *
 * Broadcom Proprietary and Confidential. Copyright (C) 2017,
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom.
 *
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: bcm_assert_log.h 523133 2014-12-27 05:50:30Z $
 */
#ifndef _WLC_ASSERT_LOG_H_
#define _WLC_ASSERT_LOG_H_

#include "wlioctl.h"

typedef struct bcm_assert_info bcm_assert_info_t;

extern void bcm_assertlog_init(void);
extern void bcm_assertlog_deinit(void);
extern int bcm_assertlog_get(void *outbuf, int iobuf_len);

extern void bcm_assert_log(char *str);

#endif /* _WLC_ASSERT_LOG_H_ */
