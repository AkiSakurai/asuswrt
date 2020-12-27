/*
 * Ecounters interface
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
 * $Id:$
 */

#ifndef __ECOUNTERS_H_
#define __ECOUNTERS_H_
#include <wlioctl.h>

#if defined(ROM_ENAB_RUNTIME_CHECK)
	#define ECOUNTERS_ENAB()   (ecounters_enabled())
#elif defined(ECOUNTERS_DISABLED)
	#define ECOUNTERS_ENAB()   (0)
#else
	#define ECOUNTERS_ENAB()   (ecounters_enabled())
#endif // endif

#ifdef EVENT_LOG_COMPILE
#define ECOUNTER_ERROR(args)	\
	EVENT_LOG_COMPACT_CAST_PAREN_ARGS(EVENT_LOG_TAG_ECOUNTERS_ERROR, args)
#else
#define ECOUNTER_ERROR(args)	printf args
#endif // endif

#define ECOUNTERS_VERSION ECOUNTERS_VERSION_2

/* A top level module like WL_RTE can tell ecounters to
 * call its entry point instead of calling its clients directly.
 * If an entry point is not defined, or top level module is
 * not present in client, the registered function is called
 * directly.
 */
#define ECOUNTERS_TOP_LEVEL_SW_ENTITY_WL	0
#define ECOUNTERS_TOP_LEVEL_SW_ENTITY_BUS	1
#define ECOUNTERS_TOP_LEVEL_SW_ENTITY_MAX	2

#define ECOUNTERS_WILDCARD_SUB_REASON	0xFFFFFFFF

/* Ecounters framework triggers */
typedef uint16 ecounters_trigger_type_t;

enum ecounters_trigger_type {
	ECOUNTERS_TRIGGER_TYPE_INVALID = 0,
	ECOUNTERS_TRIGGER_TYPE_TIMER = 1,
	ECOUNTERS_TRIGGER_TYPE_WL_EVENTS = 2,
	ECOUNTERS_TRIGGER_TYPE_MAX
};

/* Ecounters work items mask */
#define ECOUNTERS_USER_JOB_OPTION_INVALID		0x0
/* Run to completion or best effort work item */
#define ECOUNTERS_USER_JOB_OPTION_BE			0x1
/* Fill in as much information as possible in one event log buffer and stop
 * processing work item.
 */
#define ECOUNTERS_USER_JOB_OPTION_ONE_SHOT		0x2

typedef uint8 ecounters_user_job_status_t;
enum ecounters_job_status {
	/* work item status */
	ECOUNTERS_USER_JOB_STATUS_INVALID = 0,
	ECOUNTERS_USER_JOB_STATUS_NEW = 1,
	ECOUNTERS_USER_JOB_STATUS_PENDING = 2,
	ECOUNTERS_USER_JOB_STATUS_COMPLETED = 3,
	ECOUNTERS_USER_JOB_STATUS_ABORTED = 4
};

/* Initial value of cookie passed to client as a parameter */
#define ECOUNTERS_CLIENT_PARAM_COOKIE_INVALID	(~0)

/* enumeration of registered ecounters clients. This list is not actually used
 * internally. ECOUNTERS_CLIENT_MAX is used to allocate a static table of registered
 * clients and their callbacks. If a new client is added, please add an entry here
 * and update the ECOUNTERS_CLIENT_MAX
 */
enum {
	ECOUNTERS_CLIENT_WL_COUNTERS_V30 = 0,
	ECOUNTERS_CLIENT_WL_BTCOEX = 1,
	ECOUNTERS_CLIENT_WL_LTECOEX = 2,
	ECOUNTERS_CLIENT_WL_AMPDU_TX_DUMP_SLICE = 3,
	ECOUNTERS_CLIENT_WL_AMPDU_RX_DUMP_SLICE = 4,
	ECOUNTERS_CLIENT_BUS_PCIE_IPCSTATS = 5,
	ECOUNTERS_CLIENT_WL_LQM = 6,
	ECOUNTERS_CLIENT_WL_PWRSTATS_PHY = 7,
	ECOUNTERS_CLIENT_WL_PWRSTATS_WAKE_V2 = 8,
	ECOUNTERS_CLIENT_WL_PWRSTATS_SCAN = 9,
	ECOUNTERS_CLIENT_WL_STA_INFO = 11,
	ECOUNTERS_CLIENT_WL_IF_STATS_GENERIC = 12,
	ECOUNTERS_CLIENT_WL_IF_STATS_INFRA_SPECIFIC = 13,
	ECOUNTERS_CLIENT_WL_IF_STATS_MGT_CNT = 14,
	ECOUNTERS_CLIENT_WL_TVPM_CNT = 15,
	/* Set max to the last client value + 1 */
	ECOUNTERS_CLIENT_MAX = 16
};

/* The user of ecounters framework could set bit 12 in
 * Ecounters_stats_type_req_t->flags field.
 * Setting bit 12 in the flag field means skip processing
 * of the container. Note that bits 12 - 15 of ecounters_stats_type_req_t->flag
 * are reserved for internal use.
 */
#define ECOUNTERS_JOB_CONTAINER_FILTER	(0x1 << 12)

typedef struct ecounters_user_job ecounters_user_job_t;
/* ecounters_stats_completion_fn: Called when a scheduled user job
 * in ecounters framework completes.
 */
typedef int (*ecounters_user_job_completion_fn)(void *context, ecounters_user_job_t *job);

/* Ecounters job filled by timer based or events based ecounters. */
struct ecounters_user_job {
	/* Information to be filled in by users of ecounters framework */
	/* Destination event log set */
	uint16 logset;
	/* Trigger reason for the job. This is also job type */
	ecounters_trigger_type_t reason;
	/* Any other subreason for the reason above */
	uint32 sub_reason_code;

	/* Status of job. When scheduling a job, this is
	 * always set to ECOUNTERS_JOB_NEW
	 */
	ecounters_user_job_status_t job_status;
	uint8 pad;		/* reserved */
	/* run to completion, one shot. stop on error, etc. */
	uint16 options_mask;

	/* A callback to call once job is completed. Optional */
	ecounters_user_job_completion_fn completion_fn;
	/* Any arguments to above callback */
	void *completion_arg;	/* Any arguments to above callback */

	/* active configuration. The configuration on which this job is active */
	uint8 *active_config;
	uint16 active_config_len;
};

typedef ecounters_config_request_v2_t ecounters_timer_based_config_t;
typedef struct ecounters_data ecounters_data_t;

#define ECOUNTERS_SUSPEND_VERSION ECOUNTERS_SUSPEND_VERSION_V1
typedef int (*ecounters_suspend_fn)(uint8 *flag, uint8 set, void *context);

/* Ecounters suspend resume */
typedef struct ecounters_suspend_info {
	ecounters_suspend_fn suspend_fn;
	void *context;
	uint16 trigger_reason;
} ecounters_suspend_info_t;

/* ecounters_stats_get_fn: Called by ecounters to collect stats from a
 * registered source.
 * stats_type: Types present in wl_ifstats_xtlv_id in wlioctl.h
 * context: Any context information required by source to collect
 *	stats
 * req: Ecounters report request that came down from the host
 * xtlvbuf: XTLV buffer context that can be used to populate data
 *	in buffer provided by the ecounters framework.
 * cookie: Some state information that the callback expects across
 *	multiple invocations. This cookie is actually populated
 *	by callback function but stored in ecounters framework.
 *	For example, if the callback could not fit all its data in
 *	buffer provided, the cookie may contain some opaque
 *	information  about where the callback left off.
 *	When the ecounters framework calls it again with a bigger buffer,
 *	this cookie could help the callback continue to continue from
 *	where it left off.
 * attempted_write_len: If this function could not write data in the
 *	buffer provided in XTLV buffer context, tell the framework
 *	how many bytes the function attempted to write. Ecounters framework
 *	will call this callback again with a bigger buffer.
 *	If this field is populated, the callback function needs to decide
 *	if the cookie needs to be populated.
 * tlv: Some client callbacks may need to look into the payload of requested
 *	XTLV type for reporting purposes. This parameter give access to the
 *	payload of the requested XTLV type
 *
 * Some rules in coding a client callback function:
 * 1.  Note on when to set attempted_write_len. attempted_write_len essentially
 *	means how much a callback attempted to write in a single call to
 *	bcm_xtlv_put_data(). If that functions returns BCME_NOMEM or
 *	BCME_BUFTOOSHORT, the callback is supposed to tell the framework
 *	how much more buffer space it needs to report stats.
 *	atempted_write_len cannot be > 1024.
 * 2. cookie: Ecounters framework will call the callback multiple times if
 *	the client callback function returned BCME_NOMEM or BCME_BUFTOOSHORT.
 *	It is the responsibility of the client callback to store the state of
 *	where it left off during stats population. If the client function
 *	successfully wrote all its data in the buffer then it must return
 *	BCME_OK and clear cookie.
 * 3. When there is no space in the buffer provided, the client function must
 *	return BCME_BUFTOOSHORT or BCME_NOMEM.
 * 4. All XTLV records must be aligned to word boundaries.
 * 5. Client function must not access/manipulate XTLV buffer context directly
 *	All clients must use BCM XTLV APIs to write XTLVs records to buffer
 * 6. A callback function must be registered only once for a given reportable
 *	stats type. The context information registered with ecounters
 *	framework along with other parameters passed at the time of
 *	stats collection must be sufficient to deliver necessary stats.
 */
typedef int (*ecounters_stats_get_fn)(uint16 stats_type, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie, const bcm_xtlv_t* tlv,
	uint16 *attempted_write_len);

/* ecounters_entity_entry_point: Gateway to the stats callback functions. In
 * some cases this gateway may need to do some additional processing
 * before calling registered stats functions.
 * stats_type: Types present in wl_ifstats_xtlv_id in wlioctl.h
 * ecounters_stats_get_fn: Ccallback that this gateway needs to call to
 *	report stats using other parameters to this function
 * context: Any context information required by source to collect
 *	stats
 * req: Ecounters report request that came down from the host
 * xtlvbuf: XTLV buffer context that can be used to populate data
 *	in buffer provided by the ecounters framework.
 * cookie: Some state information that the callback expects across
 *	multiple invocations. This cookie is actually populated
 *	by callback function but stored in ecounters framework.
 *	For example, if the callback could not fit all its data in
 *	buffer provided, the cookie may contain some opaque
 *	information  about where the callback left off.
 *	When the ecounters framework calls it again with a bigger buffer,
 *	this cookie could help the callback continue to continue from
 *	where it left off.
 * attempted_write_len: If this function could not write data in the
 *	buffer provided in XTLV buffer context, tell the framework
 *	how many bytes the function attempted to write. Ecounters framework
 *	will call this callback again with a bigger buffer.
 */
typedef int (*ecounters_entity_entry_point)(uint16 stats_type,
	ecounters_stats_get_fn fn, void *context,
	const ecounters_stats_types_report_req_t *req,
	struct bcm_xtlvbuf *xtlvbuf, uint32 *cookie,
	const bcm_xtlv_t *tlv, uint16 *attempted_write_len);

extern int ecounters_init(osl_t *osh);

/* A top level software entity needs to register its entry point with ecounters.
 * ecounters calls this entry point so callbacks of interested sources is called with right
 * set of parameters. ecounters, by itself, does not call the registered callbacks.
 */
extern int ecounters_register_entity_entry_point(uint16 id, ecounters_entity_entry_point fn);

/* ecounters_register_source: Each source advertises its type supported by
 * registering itself with ecounters.
 * stats_type: Which statistics are advertised.
 * top_level_module: The top level entity to which this module belongs. The top level
 *	entity may have to expose an entry point if this function cannot be called
 *	directly.
 * context: Any context information that is required for this client to
 *	execute.
 * some_fn: Some client's function that needs to be passed to the entry point for
 *	execution.
 */
extern int ecounters_register_source(uint16 stats_type, uint16 top_level_module,
	ecounters_stats_get_fn some_fn, void* context);

/* Useful in ECOUNTERS_ENAB() */
extern ecounters_data_t *ecounters_enabled(void);

/* constructor and destructor for a user job */
extern ecounters_user_job_t* ecounters_user_job_create(void);
extern void ecounters_user_job_free(ecounters_user_job_t* euj);

/* Schedule an ecounters job. Exported by the ecounters framework */
extern int ecounters_user_job_schedule(ecounters_user_job_t* euj);

/* Terminate all jobs of certain type */
extern int ecounters_user_job_type_terminate(uint16 type, uint32 sub_reason);

/* Ecounters suspend resume register */
extern int ecounters_suspend_register(uint16 trigger_reason,
	ecounters_suspend_fn fn, void *context);

extern int ecounters_suspend_unregister(uint16 trigger_reason);

extern int ecounters_suspend_handle(void *params, uint32 p_len,
	void *arg, uint32 alen, bool set);

/* Called when ecounters timer based configuration comes down to
 * FW.
 */
extern int ecounters_config_process(uint8 *tlv_buf, uint16 len);

/* Timer based ecounters */
extern int ecounters_timer_triggered_config(void *params, uint32 p_len);

#endif /* __ECOUNTERS_H */
