/**
 * @file
 * @brief
 *
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 * <<Broadcom-WL-IPTag/Proprietary:>>
 */

#ifndef _acsconfig_h_
#define _acsconfig_h_

#define TRUE 1
#define FALSE 0
#define CONFIG_FILE_LINE_LENGTH 80
#define HASH_MAX_ENTRY_IN_POWER_OF_2 6
#define HASH_MAX_ENTRY (1 << HASH_MAX_ENTRY_IN_POWER_OF_2)
#define HASH_MAX_ENTRY_MASK (HASH_MAX_ENTRY - 1)

char* acsd_config_safe_get(const char *name);
char* acsd_config_get(const char *name);
int acsd_config_match(const char *name, const char *match);
void acsd_free_mem();

#endif //_acsconfig_h_
