/*
 * MBO declarations/definitions for
 * Broadcom 802.11abgn Networking Device Driver
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
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id$
 *
 */

/**
 * WFA Multiband Operation (MBO) certification program certify features that facilitate
 * efficient use of multiple frequency bands/channels available to Access Points(APs)
 * and wireless devices(STAs) that may associates with them. The prerequisites of the
 * program is that AP and STAs have information which can help in making most effective
 * selection of the spectrum band in which the STA and AP should be communicating.
 * AP and STAs enable each other to make intelligent decisions collectively for more
 * efficient use of the available spectrum by exchanging this information.
 */

#ifndef _wlc_mbo_h_
#define _wlc_mbo_h_

#include <wlc_types.h>

/* Forward declarations */
typedef uint16 bcm_iov_cmd_id_t;
typedef uint16 bcm_iov_cmd_flags_t;
typedef uint16 bcm_iov_cmd_mflags_t;
typedef struct bcm_iov_cmd_info bcm_iov_cmd_info_t;
typedef struct bcm_iov_buf bcm_iov_buf_t;
typedef struct bcm_iov_batch_buf bcm_iov_batch_buf_t;

typedef struct wlc_mbo_chan_pref {
	uint8 opclass;
	uint8 chan;
	uint8 pref;
	uint8 reason;
} wlc_mbo_chan_pref_t;

#define MBO_ASSOC_DISALLOWED_REASON_UNSPECIFIED			0x01
#define MBO_ASSOC_DISALLOWED_REASON_MAX_STA_LIMIT_REACHED	0x02
#define MBO_ASSOC_DISALLOWED_REASON_AIR_INTERFACE_OVERLOAD	0x03
#define MBO_ASSOC_DISALLOWED_REASON_AUTH_SERVER_OVERLOAD	0x04
#define MBO_ASSOC_DISALLOWED_REASON_INSUFFICIENT_RSSI		0x05
/* change REASON_MAX once reason updates in MBO standard */
#define MBO_ASSOC_DISALLOWED_REASON_MAX				0x05
/*
 * iov validation handler - All the common checks that are required
 * for processing of iovars for any given command.
 */
typedef int (*bcm_iov_cmd_validate_t)(void *ptr,
	uint32 actionid, const uint8* ibuf, size_t ilen, uint8 *obuf, size_t *olen);

/* iov get handler - process subcommand specific input and return output.
 * input and output may overlap, so the callee needs to check if
 * that is supported. For xtlv data a tlv digest is provided to make
 * parsing simpler. Output tlvs may be packed into output buffer using
 * bcm xtlv support. olen is input/output parameter. On input contains
 * max available obuf length and callee must fill the correct length
 * to represent the length of output returned.
 */
typedef int (*bcm_iov_cmd_get_t)(void *ptr,
	const uint8* ibuf, size_t ilen, uint8 *obuf, size_t *olen, wlc_bsscfg_t *bsscfg);

/* iov set handler - process subcommand specific input and return output
 * input and output may overlap, so the callee needs to check if
 * that is supported. olen is input/output parameter. On input contains
 * max available obuf length and callee must fill the correct length
 * to represent the length of output returned.
 */
typedef int (*bcm_iov_cmd_set_t)(void *ptr,
	const uint8* ibuf, size_t ilen, uint8 *obuf, size_t *olen, wlc_bsscfg_t *bsscfg);

/*
 * Batched commands will have the following memory layout
 * +--------+---------+--------+-------+
 * |version |count    | is_set |sub-cmd|
 * +--------+---------+--------+-------+
 * version >= 0x8000
 * count = number of sub-commands encoded in the iov buf
 * sub-cmd one or more sub-commands for processing
 * Where sub-cmd is padded byte buffer with memory layout as follows
 * +--------+---------+-----------------------+-------------+------
 * |cmd-id  |length   |IN(options) OUT(status)|command data |......
 * +--------+---------+-----------------------+-------------+------
 * cmd-id =sub-command ID
 * length = length of this sub-command
 * IN(options) = On input processing options/flags for this command
 * OUT(status) on output processing status for this command
 * command data = encapsulated IOVAR data as a single structure or packed TLVs for each
 * individual sub-command.
 */
struct bcm_iov_batch_subcmd {
	uint16 id;
	uint16 len;
	union {
		uint32 options;
		uint32 status;
	} u;
	uint8 data[1];
};

struct bcm_iov_batch_buf {
	uint16 version;
	uint8 count;
	uint8 is_set; /* to differentiate set or get */
	struct bcm_iov_batch_subcmd cmds[0];
};
/* information about the command, xtlv options and xtlvs_off are meaningful
 * only if XTLV_DATA cmd flag is selected
 */
struct bcm_iov_cmd_info {
	bcm_iov_cmd_id_t	cmd;		/* the (sub)command - module specific */
	bcm_iov_cmd_flags_t	flags;		/* checked by bcmiov but set by module */
	bcm_iov_cmd_mflags_t	mflags;		/* owned and checked by module */
	bcm_xtlv_opts_t		xtlv_opts;
	bcm_iov_cmd_validate_t	validate_h;	/* command validation handler */
	bcm_iov_cmd_get_t	get_h;
	bcm_iov_cmd_set_t	set_h;
	uint16			xtlvs_off;	/* offset to beginning of xtlvs in cmd data */
	uint16			min_len_set;
	uint16			max_len_set;
	uint16			min_len_get;
	uint16			max_len_get;
};

/* non-batched command version = major|minor w/ major <= 127 */
struct bcm_iov_buf {
	uint16 version;
	uint16 len;
	bcm_iov_cmd_id_t id;
	uint16 data[1]; /* 32 bit alignment may be repurposed by the command */
	/* command specific data follows */
};

/* iov options flags */
enum {
	BCM_IOV_CMD_OPT_ALIGN_NONE = 0x0000,
	BCM_IOV_CMD_OPT_ALIGN32 = 0x0001,
	BCM_IOV_CMD_OPT_TERMINATE_SUB_CMDS = 0x0002
};

/* iov command flags */
enum {
	BCM_IOV_CMD_FLAG_NONE = 0,
	BCM_IOV_CMD_FLAG_STATUS_PRESENT = (1 << 0), /* status present at data start - output only */
	BCM_IOV_CMD_FLAG_XTLV_DATA = (1 << 1),  /* data is a set of xtlvs */
	BCM_IOV_CMD_FLAG_HDR_IN_LEN = (1 << 2), /* length starts at version - non-bacthed only */
	BCM_IOV_CMD_FLAG_NOPAD = (1 << 3) /* No padding needed after iov_buf */
};
wlc_mbo_info_t * wlc_mbo_attach(wlc_info_t *wlc);
void wlc_mbo_detach(wlc_mbo_info_t *mbo);
#ifdef WL_MBO_OCE
wlc_mbo_oce_info_t *wlc_init_mbo_oce(wlc_info_t* wlc);
#endif /* WL_MBO_OCE */
int wlc_mbo_process_wnm_notif(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len);
void wlc_mbo_update_scb_band_cap(wlc_info_t* wlc, struct scb* scb, uint8* data);
int wlc_mbo_process_bsstrans_resp(wlc_info_t* wlc, struct scb* scb, uint8* body, int body_len);
void wlc_mbo_add_mbo_ie_bsstrans_req(wlc_info_t* wlc, uint8* data, bool assoc_retry_attr,
	uint8 retry_delay, uint8 transition_reason);
int wlc_mbo_calc_len_mbo_ie_bsstrans_req(uint8 reqmode, bool* assoc_retry_attr);
bool wlc_mbo_reject_assoc_req(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
bool wlc_mbo_is_channel_non_preferred(wlc_info_t* wlc, struct scb* scb, uint8 channel,
	uint8 opclass);
int32 wlc_mbo_get_gas_support(wlc_info_t* wlc);
void wlc_mbo_update_attr_assoc_disallowed(wlc_bsscfg_t *cfg, uint8 assoc_disallowed);
bool wlc_is_mbo_enabled(wlc_mbo_info_t *mbo, wlc_bsscfg_t *bsscfg);
#endif	/* _wlc_mbo_h_ */
