/*
 * Copyright 2014 Trend Micro Incorporated
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifndef __IOC_COMMON_H__
#define __IOC_COMMON_H__

//#include "../../tdts/include/udb/ioctl/udb_ioctl_common.h"

#include "conf_app.h"

/*
 * ioctl stuff
 */
/*! magic */
#define UDB_SHELL_IOCTL_MAGIC 191

#define UDB_SHELL_IOCTL_CHRDEV_NAME "idpfw"
#define UDB_SHELL_IOCTL_CHRDEV_PATH "/dev/" UDB_SHELL_IOCTL_CHRDEV_NAME //!< The device node path in system.

/*!
 * \brief tdts_shell ioctl.
 *
 * \warning Cannot exceed 128 bytes.
 */

typedef struct udb_shell_ioctl
{
	uint32_t magic; //!< A fixed magic number to identify if this structure is for this module.

	uint8_t nr; //!< nr, ioctl nr to know which sub-system you want to call, e.g. bandwidth (bw), or other tables.
	uint8_t op; //!< op, the operation to run on the sub-system, e.g. set, reset, add, etc.

	uint8_t rsv[1];

	/* Input (user to kernel) */
	uint8_t in_type; //!< \sa tdts_shell_ioctl_type_t
	union
	{
		uint64_t in_raw; //!< use to store address to avoid the issue of 32bit user program in 64bit kernel.
		uint32_t in_u32;
	};

	uint32_t in_len; //!< Input length (bytes). Plz specify this value correctly.

	/* (Optional) Output (kernel to user) */
	uint64_t out; //!< Output buffer pointer (sent to kernel to save data)
	//!< use to store address to avoid the issue of 32bit user program in 64bit kernel.
	uint64_t out_used_len; //!< Output buffer used size
	 //!< use to store address to avoid the issue of 32bit user program in 64bit kernel.
	uint32_t out_len; //!< Available output length

	uint8_t rsv2[16]; //!< Reserve for future use
}udb_shell_ioctl_t;

//typedef struct udb_shell_ioctl udb_shell_ioctl_t;
#define _UDB_IOCTL_CMD_R(_nr) _IOR(UDB_SHELL_IOCTL_MAGIC, _nr, udb_shell_ioctl_t)
#define _UDB_IOCTL_CMD_W(_nr) _IOW(UDB_SHELL_IOCTL_MAGIC, _nr, udb_shell_ioctl_t)
#define _UDB_IOCTL_CMD_WR(_nr) _IOWR(UDB_SHELL_IOCTL_MAGIC, _nr, udb_shell_ioctl_t)

#define UDB_SHELL_IOCTL_CMD_NA		0x00 //!< N/A. Do not use
#define UDB_SHELL_IOCTL_CMD_INTERNAL	_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_INTERNAL)
#define UDB_SHELL_IOCTL_CMD_COMMON		_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_COMMON)
#define UDB_SHELL_IOCTL_CMD_PATROL_TQ	_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_PATROL_TQ)
#define UDB_SHELL_IOCTL_CMD_WRS		_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_WRS)
#define UDB_SHELL_IOCTL_CMD_WBL		_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_WBL)
#define UDB_SHELL_IOCTL_CMD_IQOS	_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_IQOS)
#define UDB_SHELL_IOCTL_CMD_VP		_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_VP)
#define UDB_SHELL_IOCTL_CMD_ANOMALY	_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_ANOMALY)
#define UDB_SHELL_IOCTL_CMD_DLOG	_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_DLOG)
#define UDB_SHELL_IOCTL_CMD_HWNAT	_UDB_IOCTL_CMD_WR(UDB_IOCTL_NR_HWNAT)
/* Input/output type of value. */
typedef enum
{
	UDB_SHELL_IOCTL_TYPE_NA = 0, //!< if no input/output, set type as N/A.
	UDB_SHELL_IOCTL_TYPE_U32, //!< type is u32 (4 bytes unsigned)
	UDB_SHELL_IOCTL_TYPE_RAW, //!< type is raw data, length must be specified correctly.
	UDB_SHELL_IOCTL_TYPE_MAX
} udb_shell_ioctl_type_t;

/*!
 * \brief Init a declared ioctl structure.
 */
#define udb_shell_init_ioctl_entry(_ioc) \
	do { memset(_ioc, 0x00, sizeof(*(_ioc))); (_ioc)->magic = UDB_SHELL_IOCTL_MAGIC; } while (0)

/*!
 * \brief Set ioctl input as TDTS_SHELL_IOCTL_TYPE_RAW type.
 */
#define udb_shell_ioctl_set_in_raw(_ioc, _buf, _buf_len) \
	do { \
		(_ioc)->in_type = UDB_SHELL_IOCTL_TYPE_RAW; \
		(_ioc)->in_raw = (uintptr_t) _buf; \
		(_ioc)->in_len = (_buf_len); \
	} while (0)

/*!
 * \brief Set ioctl input as TDTS_SHELL_IOCTL_TYPE_U32 type.
 */
#define udb_shell_ioctl_set_in_u32(_ioc, _u32) \
	do { \
		(_ioc)->in_type = UDB_SHELL_IOCTL_TYPE_U32; \
		(_ioc)->in_len = sizeof(((udb_shell_ioctl_t *) 0)->in_u32); \
		(_ioc)->in_u32 = _u32; \
	} while (0)

/*!
 * \brief Set ioctl output buffer.
 */
#define udb_shell_ioctl_set_out_buf(_ioc, _buf, _buf_len, _buf_used_len_p) \
	do { \
		(_ioc)->out = (uintptr_t) _buf; \
		(_ioc)->out_len = _buf_len; \
		(_ioc)->out_used_len = (uintptr_t) _buf_used_len_p; \
	} while (0)


/* From tdts/include/udb/ioctl/udb_ioctl.h */
enum
{
	UDB_IOCTL_NR_NA = 0x00,
	UDB_IOCTL_NR_INTERNAL,	/* internal usage */
	UDB_IOCTL_NR_COMMON,
	UDB_IOCTL_NR_WRS,
	UDB_IOCTL_NR_WBL,
	UDB_IOCTL_NR_IQOS,
	UDB_IOCTL_NR_PATROL_TQ,
	UDB_IOCTL_NR_VP,
	UDB_IOCTL_NR_ANOMALY,
	UDB_IOCTL_NR_DLOG,
	UDB_IOCTL_NR_HWNAT,
	UDB_IOCTL_NR_MAX
};

#define MAX_NAMELEN 31
typedef struct shnagent_nl_upnp_ioctl_t
{
	uint8_t		friendly_name[MAX_NAMELEN + 1];
	uint8_t		manufacturer[MAX_NAMELEN + 1];
} shnagent_nl_upnp_ioctl;

enum
{
	UDB_IOCTL_COMMON_OP_NA = 0,
	UDB_IOCTL_COMMON_OP_GET_DPI_CONFIG,
	UDB_IOCTL_COMMON_OP_SET_DPI_CONFIG,
	UDB_IOCTL_COMMON_OP_GET_USER,
	UDB_IOCTL_COMMON_OP_GET_APP,
	UDB_IOCTL_COMMON_OP_GET_APP_BW_RESET,
	UDB_IOCTL_COMMON_OP_SET_APP_PATROL,
	UDB_IOCTL_COMMON_OP_GET_APP_PATROL,
	UDB_IOCTL_COMMON_OP_SET_WPR_CONF,
	UDB_IOCTL_COMMON_OP_SET_WPR_ENABLE,
	UDB_IOCTL_COMMON_OP_SET_WPR_DISABLE,
	UDB_IOCTL_COMMON_OP_SET_REDIRECT_URL,
	UDB_IOCTL_COMMON_OP_MAX
};

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#ifndef IFACE_NAME_SIZE
#define IFACE_NAME_SIZE 16
#endif

#if TMCFG_E_UDB_CORE_RULE_FORMAT_V2
#define META_NAME_SSTR_LEN 64
#define META_NAME_LSTR_LEN 128
#endif

#define AVAILABLE	(uint8_t)1
#define NOT_AVAILABLE	(uint8_t)0

#define MAX_REDIRECT_URL_LEN 512

typedef struct wpr_config
{
	uint8_t period_min;
	uint8_t max_num;
	uint8_t url[MAX_REDIRECT_URL_LEN];
} wpr_config_t;

#define DEVID_MAX_USER 253
#define MAX_APP_PER_USER 128
#define DEVID_APP_RATE_TABLE_POOL_SIZE	(DEVID_MAX_USER * MAX_APP_PER_USER)

typedef struct devdb_ioc_entry
{
	uint16_t vendor_id;
	uint16_t name_id;   // v1.0: os_id
	uint16_t class_id;
	uint16_t cat_id;    // v1.0: type_id
	uint16_t dev_id;
	uint16_t family_id;
} devdb_ioctl_entry_t;

typedef struct udb_ioc_os
{
	devdb_ioctl_entry_t de; //!< Device entry identifier. Should be unique.
	/*!
	 * \brief The priority of currently matched os. This is used so that
	 * callers know when they must update the os. For short, update whenever the
	 * device is matched a higher priority rule.
	 */
	uint16_t de_prio;
} udb_ioctl_os_t;

#define UDB_ENTRY_HOST_NAME_SIZE 32

#define MAX_WRS_CAT_NUM 128
typedef struct udb_ioc_entry
{
	uint8_t uid;
	uint8_t mac[6];

	uint8_t ipv4[4];
	uint8_t ipv6[16];

	char host_name[UDB_ENTRY_HOST_NAME_SIZE];

	udb_ioctl_os_t os;

	uint64_t ts;
	uint64_t ts_create;

#if 0//TMCFG_E_UDB_CORE_RULE_FORMAT_V2
	uint32_t used_time_sec;
#endif
#if 0//TMCFG_E_UDB_CORE_SHN_QUERY
	shnagent_nl_upnp_ioctl upnp_data;
#endif
#if 0//TMCFG_E_UDB_CORE_URL_QUERY
	uint32_t wrs_stat[MAX_WRS_CAT_NUM];
#endif
	uint8_t available;
} udb_ioctl_entry_t;

typedef struct app_ioc_entry
{
	uint8_t uid;
	uint8_t cat_id;
	uint16_t app_id;

	uint64_t last_elapsed_ts;

	uint64_t down_recent_accl;
	uint32_t down_recent_accl_pkt;
	uint64_t up_recent_accl;
	uint32_t up_recent_accl_pkt;

	uint8_t available;
} app_ioctl_entry_t;

#define UDB_PATROL_LOG_SIZE 500

typedef struct app_patrol_list_ioc_entry
{
	uint8_t uid;
	uint8_t mac[6];
	uint8_t cat_id;
	uint16_t app_id;

	int app_meta_idx;

	uint64_t time;
	uint8_t flag;
	uint8_t available;
} app_patrol_list_ioc_entry_t;

typedef struct app_bw_ioc_entry
{
	uint8_t uid;
	uint8_t cat_id;
	uint16_t app_id;

	int app_meta_idx;

	uint64_t down_recent;
	uint64_t up_recent;

	uint8_t available;
} app_bw_ioctl_entry_t;

typedef struct patrol_ioc_app
{
	uint8_t cat_id;
	uint16_t app_id;

} patrol_ioc_app_t;

typedef struct patrol_ioc_pfile
{
	uint16_t pfile_id;
	uint32_t app_cnt;
	patrol_ioc_app_t app_entry[0];

} patrol_ioc_pfile_t;

typedef struct patrol_ioc_pfile_ptr
{
	uint16_t pfile_cnt;
	patrol_ioc_pfile_t pfile_entry[0];

} patrol_ioc_pfile_ptr_t;

typedef struct patrol_ioc_mac
{
	uint8_t mac[6];
	uint16_t pfile_id;

} patrol_ioc_mac_t;

typedef struct patrol_ioc_mac_ptr
{
	uint16_t mac_cnt;
	patrol_ioc_mac_t mac_entry[0];

} patrol_ioc_mac_ptr_t;

enum
{
	ACT_COMMON_GET_ALL_USR = 0,	/* get_all_user */
	ACT_COMMON_GET_USR_DETAIL,	/* get_user_detail */
	ACT_COMMON_GET_ALL_APP,		/* get_all_app */
	ACT_COMMON_GET_ALL_APP_CLEAR,	/* get_all_app_clear */
	ACT_COMMON_SET_APP_PATROL,	/* set_app_patrol */
	ACT_COMMON_GET_APP_PATROL,	/* get_app_patrol */
	ACT_COMMON_SET_WPR_CONF,	/* set_wpr_conf */
	ACT_COMMON_SET_WPR_ON,		/* set_wpr_on */
	ACT_COMMON_SET_WPR_OFF,		/* set_wpr_off */
	ACT_COMMON_SET_REDIRECT_URL,	/* set_redirect_url */
	ACT_COMMON_SET_META_DATA,	/* internal: set_meta_data */
	ACT_COMMON_DISABLE_FEEDBACK,	/* internal: disable_feedback */
	ACT_COMMON_ENABLE_FEEDBACK,	/* internal: enable_feedback */
	ACT_COMMON_SET_EULA_AGREED,	/* internal: set_eula_agreed */
	ACT_COMMON_SET_EULA_DISAGREED,	/* internal: set_eula_disagreed */
	ACT_COMMON_MAX
};

#define IOC_SHIFT_LEN_SAFE(len, siz, max) \
({ \
	uint8_t ok = 1; \
	if ((len) + (siz) > (max)) { \
		ok = 0; \
		DBG("len (%d) exceed max (%d)!\n", (len) + (siz), (max)); \
	} \
	else { \
		(len) += (siz); \
	} \
	ok; \
})

int run_ioctl(const char *path, int req, void *arg);
int get_fw_user_list(udb_ioctl_entry_t **output, uint32_t *used_len);

unsigned long get_build_date(void);
unsigned long get_build_number(void);

int parse_single_str_arg(int argc, char **argv, char opt, char *buf, int buf_len);

int common_options_init(struct cmd_option *cmd);

#endif /* __IOC_COMMON_H__ */

