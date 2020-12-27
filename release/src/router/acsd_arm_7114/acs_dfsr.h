/*
 *      acs_dfsr.h
 *
 *	Header file for the ACSD DFS Re-entry module.
 *
 *	Broadcom Proprietary and Confidential. Copyright (C) 2016,
 *	All Rights Reserved.
 *	
 *	This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom;
 *	the contents of this file may not be disclosed to third parties, copied
 *	or duplicated in any form, in whole or in part, without the prior
 *	written permission of Broadcom.
 *
 *	$Id$
 */
#ifndef __acs_dfsr_h__
#define __acs_dfsr_h__

typedef struct dfsr_context dfsr_context_t;


typedef enum {
	DFS_REENTRY_NONE = 0,
	DFS_REENTRY_DEFERRED,
	DFS_REENTRY_IMMEDIATE
} dfsr_reentry_type_t;

extern dfsr_context_t *acs_dfsr_init(char *prefix, bool enable, acs_bgdfs_info_t *acs_bgdfs);
extern void acs_dfsr_exit(dfsr_context_t *);
extern dfsr_reentry_type_t acs_dfsr_chanspec_update(dfsr_context_t *, chanspec_t,
	const char *caller, char *if_name);
extern int acs_dfsr_set_reentry_type(dfsr_context_t *ctx, int type);
extern dfsr_reentry_type_t acs_dfsr_set(dfsr_context_t *ctx, chanspec_t channel,
        const char *caller);

extern dfsr_reentry_type_t acs_dfsr_activity_update(dfsr_context_t *, char *if_name);
extern dfsr_reentry_type_t acs_dfsr_reentry_type(dfsr_context_t *);
extern void acs_dfsr_reentry_done(dfsr_context_t *);
extern bool acs_dfsr_enabled(dfsr_context_t *ctx);
extern bool acs_dfsr_enable(dfsr_context_t *ctx, bool enable);
extern int acs_dfsr_dump(dfsr_context_t *ctx, char *buf, unsigned buflen);
extern void acs_bgdfs_sw_add(dfsr_context_t *ctx, time_t now, uint32_t frame_count);
extern unsigned acs_bgdfs_sw_sum(dfsr_context_t *ctx);

#endif /* __acs_dfsr_h__ */
