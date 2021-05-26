/*
 * Proxd FTM method implementation - iovar support. See twiki FineTimingMeasurement.
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: pdftmiov.c 787304 2020-05-26 09:21:52Z $
 */

#include "pdftmpvt.h"

typedef pdftm_iov_tlv_digest_t ftm_iov_tlv_digest_t;
typedef pdftm_iov_tlv_packer_t ftm_iov_tlv_packer_t;
typedef pdftm_iov_tlv_dig_info_t ftm_iov_tlv_dig_info_t;
typedef pdftm_iov_tlv_get_len_t ftm_iov_tlv_get_len_t;

#define DIG_DEV_ADDR tlv_data.config.dev_addr
#define DIG_AVAIL_SLOTS slots
#define DIG_LCI_REP			tlv_data.out.lci_rep
#define DIG_LCI_REP_LEN		tlv_data.out.lci_rep_len
#define DIG_CIVIC_REP		tlv_data.out.civic_rep
#define DIG_CIVIC_REP_LEN	tlv_data.out.civic_rep_len

/* packer - packs data of a given type into buffer */
#define PACKER_DECL(X) static void ftm_iov_pack_##X(const ftm_iov_tlv_digest_t *dig, \
	uint8 **buf, const uint8 *data, int len);

PACKER_DECL(uint8); /* raw byte copy - uint8, ether addr etc */
PACKER_DECL(uint16);
PACKER_DECL(uint32);
PACKER_DECL(intvl);
PACKER_DECL(info);
PACKER_DECL(session_info);
PACKER_DECL(session_status);
PACKER_DECL(counters);
PACKER_DECL(rtt_result);
#ifdef WL_RANGE_SEQ
PACKER_DECL(rtt_result_v2);
#endif /* WL_RANGE_SEQ */
PACKER_DECL(sessions);
PACKER_DECL(ftm_req);
#ifdef WL_FTM_RANGE
PACKER_DECL(ranging_info);
#endif // endif
#ifdef WL_FTM_11K
PACKER_DECL(lci_rep);
PACKER_DECL(civic_rep);
#endif // endif
PACKER_DECL(tune);

#define GETLEN_DECL(X) static uint16 ftm_iov_get_##X##_len(const ftm_iov_tlv_digest_t *dig, \
	const ftm_iov_tlv_dig_info_t *tlv_info);

GETLEN_DECL(rtt_result);
#ifdef WL_RANGE_SEQ
GETLEN_DECL(rtt_result_v2);
#endif /* WL_RANGE_SEQ */
GETLEN_DECL(sessions);
GETLEN_DECL(ftm_req);
#ifdef WL_FTM_11K
GETLEN_DECL(lci_rep);
GETLEN_DECL(civic_rep);
#endif // endif

#define DIGOFF(x) OFFSETOF(ftm_iov_tlv_digest_t, x)
#define PACKFN(_name) NULL, ftm_iov_pack_ ## _name
#define PACKFN2(_name) ftm_iov_get_ ## _name ## _len, ftm_iov_pack_##_name

static const ftm_iov_tlv_dig_info_t ftm_iov_tlv_dig_info[] = {
	{WL_PROXD_TLV_ID_FLAGS, sizeof(uint32), DIGOFF(tlv_data.config.flags), PACKFN(uint32)},
	{WL_PROXD_TLV_ID_CHANSPEC, sizeof(uint32),
	DIGOFF(tlv_data.config.chanspec), PACKFN(uint32)},
	{WL_PROXD_TLV_ID_TX_POWER, sizeof(uint32),
	DIGOFF(tlv_data.config.tx_power), PACKFN(uint32)},
	{WL_PROXD_TLV_ID_RATESPEC, sizeof(uint32),
	DIGOFF(tlv_data.config.ratespec), PACKFN(uint32)},
	{WL_PROXD_TLV_ID_BURST_DURATION, sizeof(wl_proxd_intvl_t),
	DIGOFF(tlv_data.config.burst_duration), PACKFN(intvl)},
	{WL_PROXD_TLV_ID_BURST_PERIOD, sizeof(wl_proxd_intvl_t),
	DIGOFF(tlv_data.config.burst_period), PACKFN(intvl)},
	{WL_PROXD_TLV_ID_BURST_FTM_SEP, sizeof(wl_proxd_intvl_t),
	DIGOFF(tlv_data.config.burst_ftm_sep), PACKFN(intvl)},
	{WL_PROXD_TLV_ID_BURST_TIMEOUT, sizeof(wl_proxd_intvl_t),
	DIGOFF(tlv_data.config.burst_timeout), PACKFN(intvl)},
	{WL_PROXD_TLV_ID_BURST_NUM_FTM, sizeof(uint16), DIGOFF(tlv_data.config.burst_num_ftm),
	PACKFN(uint16)},
	{WL_PROXD_TLV_ID_NUM_BURST, sizeof(uint16), DIGOFF(tlv_data.config.num_burst),
	PACKFN(uint16)},
	{WL_PROXD_TLV_ID_FTM_RETRIES, sizeof(uint8), DIGOFF(tlv_data.config.ftm_retries),
	PACKFN(uint8)},
	{WL_PROXD_TLV_ID_BSS_INDEX, sizeof(uint8), DIGOFF(bss_index), PACKFN(uint8)},
	{WL_PROXD_TLV_ID_BSSID, sizeof(struct ether_addr), DIGOFF(bssid), PACKFN(uint8)},
	{WL_PROXD_TLV_ID_INIT_DELAY, sizeof(wl_proxd_intvl_t), DIGOFF(tlv_data.config.init_delay),
	PACKFN(intvl)},
	{WL_PROXD_TLV_ID_EVENT_MASK, sizeof(uint32),
	DIGOFF(tlv_data.config.event_mask), PACKFN(uint32)},
	{WL_PROXD_TLV_ID_FLAGS_MASK, sizeof(uint32),
	DIGOFF(tlv_data.config.flags_mask), PACKFN(uint32)},
	{WL_PROXD_TLV_ID_PEER_MAC, sizeof(struct ether_addr), DIGOFF(tlv_data.config.peer_mac),
	PACKFN(uint8)},
	{WL_PROXD_TLV_ID_SESSION_FLAGS, sizeof(uint32), DIGOFF(tlv_data.config.session_flags),
	PACKFN(uint32)},
	{WL_PROXD_TLV_ID_SESSION_FLAGS_MASK, sizeof(uint32),
	DIGOFF(tlv_data.config.session_flags_mask), PACKFN(uint32)},
	{WL_PROXD_TLV_ID_DEBUG_MASK, sizeof(uint32),
	DIGOFF(tlv_data.config.debug_mask), PACKFN(uint32)},
	{ WL_PROXD_TLV_ID_RX_MAX_BURST, sizeof(uint16),
	DIGOFF(tlv_data.config.rx_max_burst), PACKFN(uint16) },
	{ WL_PROXD_TLV_ID_RANGING_FLAGS, sizeof(uint16), DIGOFF(tlv_data.config.ranging_flags),
	PACKFN(uint16) },
	{ WL_PROXD_TLV_ID_RANGING_FLAGS_MASK, sizeof(uint16),
	DIGOFF(tlv_data.config.ranging_flags_mask), PACKFN(uint16) },
	{WL_PROXD_TLV_ID_INFO, sizeof(wl_proxd_ftm_info_t),
	DIGOFF(tlv_data.out.info), PACKFN(info)},
	{WL_PROXD_TLV_ID_SESSION_INFO, sizeof(wl_proxd_ftm_session_info_t),
	DIGOFF(tlv_data.out.session_info), PACKFN(session_info)},
	{WL_PROXD_TLV_ID_SESSION_STATUS, sizeof(wl_proxd_ftm_session_status_t),
	DIGOFF(tlv_data.out.session_status), PACKFN(session_status)},
	{WL_PROXD_TLV_ID_COUNTERS, sizeof(wl_proxd_counters_t),
	DIGOFF(tlv_data.out.counters), PACKFN(counters)},
#ifdef WL_RANGE_SEQ
	{WL_PROXD_TLV_ID_RTT_RESULT, OFFSETOF(wl_proxd_rtt_result_t, rtt),
#else
	{WL_PROXD_TLV_ID_RTT_RESULT_V2, OFFSETOF(wl_proxd_rtt_result_t, rtt),
#endif /* WL_RANGE_SEQ */
	DIGOFF(tlv_data.out.rtt_result), PACKFN2(rtt_result)},
#ifdef WL_RANGE_SEQ
	{WL_PROXD_TLV_ID_RTT_RESULT_V2, OFFSETOF(wl_proxd_rtt_result_v2_t, rtt),
	DIGOFF(tlv_data.out.rtt_result), PACKFN2(rtt_result_v2)},
#endif /* WL_RANGE_SEQ */
	{WL_PROXD_TLV_ID_SESSION_ID_LIST, OFFSETOF(wl_proxd_session_id_list_t, ids),
	DIGOFF(tlv_data.config.sid_list), PACKFN2(sessions)},
	{WL_PROXD_TLV_ID_FTM_REQ, sizeof(dot11_ftm_req_t), /* protocol request - not aligned */
	DIGOFF(tlv_data.out.ftm_req), PACKFN2(ftm_req)},
#ifdef WL_FTM_RANGE
	{WL_PROXD_TLV_ID_RANGING_INFO, sizeof(wl_proxd_ranging_info_t),
	DIGOFF(tlv_data.out.ranging_info), PACKFN(ranging_info)},
#endif /* WL_FTM_RANGE */
	{WL_PROXD_TLV_ID_DEV_ADDR, sizeof(struct ether_addr),
	DIGOFF(DIG_DEV_ADDR), PACKFN(uint8)},
	{ WL_PROXD_TLV_ID_TLV_ID, sizeof(uint16),
	DIGOFF(tlv_ids), PACKFN(uint16) },
#ifdef WL_FTM_11K
	{ WL_PROXD_TLV_ID_LCI, sizeof(dot11_rmrep_ftm_lci_t),
	DIGOFF(DIG_LCI_REP), PACKFN2(lci_rep) },
	{ WL_PROXD_TLV_ID_CIVIC, sizeof(dot11_rmrep_ftm_civic_t),
	DIGOFF(DIG_CIVIC_REP), PACKFN2(civic_rep) },
#endif /* WL_FTM_11K */
	{WL_PROXD_TLV_ID_FTM_REQ_RETRIES, sizeof(uint8), DIGOFF(tlv_data.config.ftm_req_retries),
	PACKFN(uint8)},
	{WL_PROXD_TLV_ID_TPK, sizeof(wl_proxd_tpk_t), DIGOFF(tlv_data.config.tpk_ftm),
	PACKFN(uint8)},

	{WL_PROXD_TLV_ID_TUNE, sizeof(wl_proxd_params_tof_tune_t),
	DIGOFF(tune), PACKFN(tune)},
	{WL_PROXD_TLV_ID_CUR_ETHER_ADDR, sizeof(struct ether_addr),
	DIGOFF(cur_ether_addr), PACKFN(uint8)},

	/* add additional tlv info above */
	{WL_PROXD_TLV_ID_NONE, -1, -1, NULL, NULL}
};
#undef DIGOFF
#undef PACKFN

/* iov info lookup support */
enum {
	FTM_IOV_FLAG_NONE			= 0x00,
	FTM_IOV_FLAG_METHOD			= 0x01,
	FTM_IOV_FLAG_SESSION		= 0x02,
	FTM_IOV_FLAG_GET			= 0x04,
	FTM_IOV_FLAG_SET			= 0x08,
	FTM_IOV_FLAG_ALL			= 0xff
};
typedef int8 ftm_iov_flags_t;

typedef int (*ftm_iov_get_handler_t)(
	pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len);

typedef int (*ftm_iov_set_handler_t)(
	pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len);

/* iov handlers  - forward decls */

/* get handlers */
#define DEF_GETHANDLER(_name) static int ftm_iov_get_##_name (\
	pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid, \
	const wl_proxd_tlv_t *req_tlvs, int req_len, \
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)

DEF_GETHANDLER(tune);
DEF_GETHANDLER(version);
DEF_GETHANDLER(info);
DEF_GETHANDLER(session_info);
DEF_GETHANDLER(session_status);
DEF_GETHANDLER(counters);
DEF_GETHANDLER(session_counters);
DEF_GETHANDLER(result);
DEF_GETHANDLER(sessions);
#ifdef WL_FTM_RANGE
DEF_GETHANDLER(ranging_info);
#endif // endif

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
#define FTM_IOV_GET_DUMP ftm_iov_get_dump
DEF_GETHANDLER(dump);
#else
#define FTM_IOV_GET_DUMP NULL
#endif // endif

/* set handlers */
#define DEF_SETHANDLER(_name) static int ftm_iov_##_name (\
	pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid, \
	const wl_proxd_tlv_t *req_tlvs, int req_len)

DEF_SETHANDLER(tune);
DEF_SETHANDLER(enable);
DEF_SETHANDLER(disable);
DEF_SETHANDLER(config);
DEF_SETHANDLER(session_config);
DEF_SETHANDLER(session_start);
DEF_SETHANDLER(session_stop);
DEF_SETHANDLER(clear_counters);
DEF_SETHANDLER(clear_session_counters);
DEF_SETHANDLER(delete_session);
DEF_SETHANDLER(burst_request);
#ifdef WL_FTM_RANGE
DEF_SETHANDLER(start_ranging);
DEF_SETHANDLER(stop_ranging);
#endif // endif
/* end iov handlers - forward decls */

struct ftm_iov_info {
	wl_proxd_cmd_t cmd;
	ftm_iov_flags_t flags;
	ftm_iov_get_handler_t handle_get;
	ftm_iov_set_handler_t handle_set;
};
typedef struct ftm_iov_info ftm_iov_info_t;

#define GET_HANDLER(_name) ftm_iov_get_##_name
#define HANDLER(_name) ftm_iov_##_name

static const ftm_iov_info_t ftm_iov_info[] = {
	/* get commands */
	{WL_PROXD_CMD_GET_VERSION, (FTM_IOV_FLAG_METHOD | FTM_IOV_FLAG_SESSION),
	GET_HANDLER(version), NULL},
	{WL_PROXD_CMD_GET_INFO, FTM_IOV_FLAG_METHOD, GET_HANDLER(info), NULL},
	{WL_PROXD_CMD_GET_INFO, FTM_IOV_FLAG_SESSION, GET_HANDLER(session_info), NULL},
	{WL_PROXD_CMD_GET_STATUS, FTM_IOV_FLAG_SESSION, GET_HANDLER(session_status), NULL},
	{WL_PROXD_CMD_GET_COUNTERS, FTM_IOV_FLAG_METHOD, GET_HANDLER(counters), NULL},
	{WL_PROXD_CMD_GET_COUNTERS, FTM_IOV_FLAG_SESSION, GET_HANDLER(session_counters), NULL},
	{WL_PROXD_CMD_GET_RESULT, FTM_IOV_FLAG_SESSION, GET_HANDLER(result), NULL},
	{WL_PROXD_CMD_GET_SESSIONS, FTM_IOV_FLAG_METHOD, GET_HANDLER(sessions), NULL},
#ifdef WL_FTM_RANGE
	{WL_PROXD_CMD_GET_RANGING_INFO, FTM_IOV_FLAG_METHOD, GET_HANDLER(ranging_info), NULL },
#endif // endif
	{WL_PROXD_CMD_DUMP, (FTM_IOV_FLAG_METHOD | FTM_IOV_FLAG_SESSION), FTM_IOV_GET_DUMP, NULL},

	/* set commands */
	{WL_PROXD_CMD_DISABLE, FTM_IOV_FLAG_METHOD, NULL, HANDLER(disable)},
	{WL_PROXD_CMD_ENABLE, FTM_IOV_FLAG_METHOD, NULL, HANDLER(enable)},
	{WL_PROXD_CMD_CONFIG, FTM_IOV_FLAG_METHOD, NULL, HANDLER(config)},
	{WL_PROXD_CMD_CONFIG, FTM_IOV_FLAG_SESSION, NULL, HANDLER(session_config)},
	{WL_PROXD_CMD_START_SESSION, FTM_IOV_FLAG_SESSION, NULL, HANDLER(session_start)},
	{WL_PROXD_CMD_STOP_SESSION, FTM_IOV_FLAG_SESSION, NULL, HANDLER(session_stop)},
	{WL_PROXD_CMD_CLEAR_COUNTERS, FTM_IOV_FLAG_METHOD, NULL, HANDLER(clear_counters)},
	{WL_PROXD_CMD_CLEAR_COUNTERS, FTM_IOV_FLAG_SESSION, NULL, HANDLER(clear_session_counters)},
	{WL_PROXD_CMD_DELETE_SESSION, FTM_IOV_FLAG_SESSION, NULL, HANDLER(delete_session)},
	{WL_PROXD_CMD_BURST_REQUEST, FTM_IOV_FLAG_SESSION, NULL, HANDLER(burst_request)},
#ifdef WL_FTM_RANGE
	{ WL_PROXD_CMD_START_RANGING, FTM_IOV_FLAG_METHOD, NULL, HANDLER(start_ranging)},
	{ WL_PROXD_CMD_STOP_RANGING, FTM_IOV_FLAG_METHOD, NULL, HANDLER(stop_ranging)},
#endif // endif
	{ WL_PROXD_CMD_TUNE, FTM_IOV_FLAG_METHOD, GET_HANDLER(tune), HANDLER(tune)},

	{WL_PROXD_CMD_NONE, 0, NULL, NULL} /* must be last */
};

static const ftm_iov_info_t*
ftm_iov_get_iov_info(pdftm_t *ftm, wl_proxd_cmd_t cmd, ftm_iov_flags_t flags)
{
	const ftm_iov_info_t *ret = NULL;
	int i;

	/* limit commands when proxd is disabled */
	if (!PROXD_ENAB(FTM_PUB(ftm))) {
		if ((cmd != WL_PROXD_CMD_ENABLE) &&
		    (cmd != WL_PROXD_CMD_DISABLE) &&
		    (cmd != WL_PROXD_CMD_GET_INFO))
			goto done;
	}

	/* find iov for method or session - at least one must selected specified */
	ASSERT(flags & (FTM_IOV_FLAG_METHOD | FTM_IOV_FLAG_SESSION));

	for (i = 0; ftm_iov_info[i].cmd != WL_PROXD_CMD_NONE; ++i) {
		const ftm_iov_info_t *iov_info = &ftm_iov_info[i];

		ASSERT(iov_info->flags & (FTM_IOV_FLAG_METHOD | FTM_IOV_FLAG_SESSION));
		if (iov_info->cmd != cmd)
			continue;

		if (flags & FTM_IOV_FLAG_METHOD) {
			if (!(iov_info->flags & FTM_IOV_FLAG_METHOD))
				continue;
		} else if (flags & FTM_IOV_FLAG_SESSION) {
			if (!(iov_info->flags & FTM_IOV_FLAG_SESSION))
				continue;
		}

		ret = iov_info;
		break;
	}

done:
	return ret;
}

static const ftm_iov_tlv_dig_info_t*
ftm_iov_get_dig_info(uint16 id)
{
	const ftm_iov_tlv_dig_info_t *ret = NULL;
	int i;

	for (i = 0; ftm_iov_tlv_dig_info[i].id != WL_PROXD_TLV_ID_NONE; ++i) {
		const ftm_iov_tlv_dig_info_t *info = &ftm_iov_tlv_dig_info[i];
		if (info->id == id) {
			ret = info;
			break;
		}
	}
	return ret;
}

/* unpacks tlvs into the digest structure. assumes  caller has
 * validated len is valid for the buffer. upon success, buffer
 * is updated past the tlv
 */
static int
ftm_iov_unpack_tlv(void *ctx, const uint8 *buf, uint16 id, uint16 len)
{
	ftm_iov_tlv_digest_t *dig;
	int err = BCME_OK;
	const ftm_iov_tlv_dig_info_t *tlv_info;
	uint8 *data_dst;
	uint32 dst_ptr_size = sizeof(uint8*);

	ASSERT(ctx != NULL && buf != NULL);

	dig = (ftm_iov_tlv_digest_t *)ctx;
	tlv_info = ftm_iov_get_dig_info(id);
	if (!tlv_info) /* don't know this one, ignore */
		goto done;

	if (len < tlv_info->len) {
		err = BCME_BADLEN;
		goto done;
	}

	/* copy src data ptr to dst data ptr in its place in dig. it is assumed that
	 * buf points to data in xtlv that will be valid as long as xtlv buffer is
	 */
	data_dst = (uint8 *)dig + tlv_info->dig_off;
	memcpy(data_dst, &buf, dst_ptr_size);

done:
	FTM_LOG_STATUS(dig->ftm, err, (("wl%d: %s: status %d processing tlv %d len %d\n",
		FTM_UNIT(dig->ftm), __FUNCTION__, err, id, len)));
	return err;
}

/* resolve bsscfg - override with information from host/app */
static wlc_bsscfg_t*
ftm_iov_resolve_bsscfg(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, const ftm_iov_tlv_digest_t *dig)
{
	ASSERT(dig != NULL);

	if (dig->bss_index) {
		wlc_if_t *wlcif;

		/* some convolution to avoid bsscfg wlc member directly */
		wlcif = wlc_wlcif_get_by_index(ftm->wlc, dig->bss_index[0]);
		if (!wlcif)
			bsscfg = NULL;
		else
			bsscfg = wlc_bsscfg_find_by_wlcif(ftm->wlc, wlcif);
	} else if (dig->bssid) {
		if (ETHER_ISNULLADDR(dig->bssid))
			bsscfg = pdsvc_get_bsscfg(ftm->pd);
		else
			bsscfg = wlc_bsscfg_find_by_bssid(ftm->wlc, dig->bssid);
	}

	return bsscfg;
}

/* unpack request and return digest and bsscfg (resolved) */
static int
ftm_iov_unpack(pdftm_t *ftm, const wl_proxd_tlv_t *req_tlvs, int req_len,
	ftm_iov_tlv_digest_t *dig, wlc_bsscfg_t **bsscfg)
{
	int err;

	ASSERT(bsscfg != NULL && (*bsscfg) != NULL);

	pdftm_iov_dig_reset(ftm, NULL, dig);

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	dig->req_tlvs_end = (uint8 *)req_tlvs + req_len;
	GCC_DIAGNOSTIC_POP();

	err = bcm_unpack_xtlv_buf((void *)dig, (const void *)req_tlvs, req_len,
		FTM_XTLV_OPTIONS, ftm_iov_unpack_tlv);
	if (err != BCME_OK)
		goto done;

	*bsscfg = ftm_iov_resolve_bsscfg(ftm, *bsscfg, dig);
	if (!(*bsscfg)) {
		err = BCME_NOTFOUND;
		goto done;
	}

done:
	FTM_LOGIOV(ftm, (("wl%d: %s: status %d unpacking tlvs to dig %p.\n",
		FTM_UNIT(ftm), __FUNCTION__, err, OSL_OBFUSCATE_BUF(dig))));
	return err;
}

/* enable/disable */
static int
ftm_iov_set_enable(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len, bool enable)
{
	int err;
	bool enabled;
	uint bit_pos;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	enabled = FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg);
	if (enabled && enable) {
		FTM_LOGIOV(ftm, (("wl%d: %s: FTM already enabled for bss index %d\n",
			FTM_UNIT(ftm), __FUNCTION__, WLC_BSSCFG_IDX(bsscfg))));
		goto done;
	} else if (!enabled && !enable) {
		FTM_LOGIOV(ftm, (("wl%d: %s: FTM already disabled for bss index %d\n",
			FTM_UNIT(ftm), __FUNCTION__, WLC_BSSCFG_IDX(bsscfg))));
		goto done;
	}

	/* update ftm capability for bss and update bcn/prb - both tx and rx */
	if (enable) {
		bit_pos = FTM_BSSCFG_OPTION_BIT(bsscfg, FTM_BSSCFG_OPTION_TX);
		setbit(ftm->enabled_bss, bit_pos);
		ftm->config->flags |= WL_PROXD_FLAG_RX_ENABLED;
		bit_pos = FTM_BSSCFG_OPTION_BIT(bsscfg, FTM_BSSCFG_OPTION_RX);
		setbit(ftm->enabled_bss, bit_pos);
		proxd_enable(ftm->wlc, TRUE);
		/* security not enabled by default */
	} else {
		pdftm_bsscfg_clear_options(ftm, bsscfg);
		pdftm_free_sessions(ftm, bsscfg, FALSE);
		if (!pdftm_need_proxd(ftm, FTM_BSSCFG_OPTION_TX)) {
			ftm->config->flags &=
				~(WL_PROXD_FLAG_RX_ENABLED | WL_PROXD_FLAG_SECURE);
			proxd_enable(ftm->wlc, FALSE);
		}
	}

	wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_FTM_INITIATOR, enable);
	wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_FTM_RESPONDER, enable);

	if (!BSSCFG_HAS_NOIF(bsscfg) && bsscfg->up &&
		(BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg))) {
		wlc_bss_update_beacon(ftm->wlc, bsscfg);
		wlc_bss_update_probe_resp(ftm->wlc, bsscfg, TRUE);
	}

done:
	return err;
}

static int
ftm_iov_enable(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	return ftm_iov_set_enable(ftm, bsscfg, sid, req_tlvs, req_len, TRUE);
}

/* disable */
static int
ftm_iov_disable(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	return ftm_iov_set_enable(ftm, bsscfg, sid, req_tlvs, req_len, FALSE);
}

static int
ftm_iov_parse_intvl(const void *arg, wl_proxd_intvl_t *intvl)
{
	wl_proxd_tmu_t tmu;
	uint32 val;
	int err = BCME_OK;

	ASSERT(arg && intvl);
	val = ltoh32_ua(arg);
	tmu = ltoh16_ua((const uint8 *)arg + OFFSETOF(wl_proxd_intvl_t, tmu));

	if (tmu > WL_PROXD_TMU_PICO_SEC) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	/* for now limit to 0.1ns - resolution per spec */
	if ((tmu == WL_PROXD_TMU_PICO_SEC) && val < 100) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	intvl->intvl = val;
	intvl->tmu = tmu;
	intvl->pad[0] = intvl->pad[1] = 0;

done:
	return err;
}

/*
* convert 'number of bursts to power of 2
*/
static uint16
ftm_iov_adjust_num_burst(pdftm_t *ftm, uint16 num_burst)
{
	uint16 exponent;

	BCM_REFERENCE(ftm);

	/* get burst exponent and enforce limit */
	FTM_PROTO_BEXP(num_burst, exponent);
	if (exponent > FTM_MAX_NUM_BURST_EXP)
		exponent = FTM_MAX_NUM_BURST_EXP;
	return 1 << exponent;
}

static int
ftm_iov_session_config_from_dig(pdftm_t *ftm, const ftm_iov_tlv_digest_t *tlv_dig,
	pdftm_session_config_t *sncfg, wlc_bsscfg_t *bsscfg)
{
	int err = BCME_OK;
	int cnt = 0;
	const pdftm_iov_tlv_config_digest_t *dig;
	ftm_scb_t *ftm_scb;

	dig = &tlv_dig->tlv_data.config;

	if (dig->session_flags) {
		uint32 flags;
		uint32 mask;

		flags = ltoh32_ua(dig->session_flags);
		if (dig->session_flags_mask)
			mask = ltoh32_ua(dig->session_flags_mask);
		else
			mask = WL_PROXD_SESSION_FLAG_ALL;

		FTM_UPDATE_FLAGS(sncfg->flags, flags, mask);
		++cnt;
	}

	if (dig->event_mask) {
		sncfg->event_mask = ltoh32_ua(dig->event_mask);
		++cnt;
	}

#define BCF(_sncfg, _fld) (_sncfg)->burst_config->_fld

	if (dig->chanspec) {
		BCF(sncfg, chanspec) = (chanspec_t)ltoh32_ua(dig->chanspec);
		++cnt;
	}
	if (dig->tx_power) {
		BCF(sncfg, tx_power) = ltoh32_ua(dig->tx_power);
		++cnt;
	}
	if (dig->ratespec) {
		BCF(sncfg, ratespec) = ltoh32_ua(dig->ratespec);
		++cnt;
	}
	if (dig->burst_duration) {
		err = ftm_iov_parse_intvl(dig->burst_duration, &BCF(sncfg, duration));
		if (err == BCME_OK)
			++cnt;
		else
			goto done;
	}
	if (dig->burst_period) {
		err = ftm_iov_parse_intvl(dig->burst_period, &sncfg->burst_period);
		if (err == BCME_OK)
			++cnt;
		else
			goto done;
	}
	if (dig->burst_ftm_sep) {
		err = ftm_iov_parse_intvl(dig->burst_ftm_sep, &BCF(sncfg, ftm_sep));
		if (err == BCME_OK)
			++cnt;
		else
			goto done;
	}
	if (dig->burst_timeout) {
		err = ftm_iov_parse_intvl(dig->burst_timeout, &BCF(sncfg, timeout));
		if (err == BCME_OK)
			++cnt;
		else
			goto done;
	}
	if (dig->init_delay) {
		err = ftm_iov_parse_intvl(dig->init_delay, &sncfg->init_delay);
		if (err == BCME_OK)
			++cnt;
		else
			goto done;
	}
	if (dig->burst_num_ftm) {
		BCF(sncfg, num_ftm) = ltoh16_ua(dig->burst_num_ftm);
		++cnt;
	}
	if (dig->num_burst) {
		sncfg->num_burst = ftm_iov_adjust_num_burst(ftm, ltoh16_ua(dig->num_burst));
		++cnt;
	}
	if (dig->ftm_retries) {
		BCF(sncfg, ftm_retries) = ltoh16_ua(dig->ftm_retries);
		++cnt;
	}
	if (dig->peer_mac) {
		memcpy(&BCF(sncfg, peer_mac), dig->peer_mac, sizeof(struct ether_addr));
		++cnt;
	}
	if (dig->ftm_req_retries) {
		BCF(sncfg, ftm_req_retries) = ltoh16_ua(dig->ftm_req_retries);
		++cnt;
	}
	if (dig->tpk_ftm) {
		scb_t *scb;
		scb = wlc_scbfind_dualband(ftm->wlc, bsscfg, &dig->tpk_ftm->peer);
		if (scb) {
			ftm_scb = FTM_SCB(ftm, scb);
			if (ftm_scb->len < FTM_TPK_MAX_LEN) {
				err = BCME_BUFTOOSHORT;
				goto done;
			}
			memcpy(ftm_scb->tpk, dig->tpk_ftm->tpk, FTM_TPK_MAX_LEN);
		}
		++cnt;
	}
	if (tlv_dig->cur_ether_addr) {
		memcpy(&BCF(sncfg, cur_ether_addr), tlv_dig->cur_ether_addr,
		sizeof(struct ether_addr));
		++cnt;
	}
	if (tlv_dig->bssid) {
		memcpy(&BCF(sncfg, bssid), tlv_dig->bssid, sizeof(struct ether_addr));
		++cnt;
	}

#undef BCF

done:
	FTM_LOGIOV(ftm, (("wl%d: %s: exiting with status %d, updated %d fields from dig %p\n",
		FTM_UNIT(ftm), __FUNCTION__, err, cnt, OSL_OBFUSCATE_BUF(tlv_dig))));
	return err;
}

static int
ftm_iov_config(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	pdftm_session_config_t *sncfg;

	dig = ftm->dig;
	err = ftm_iov_unpack(ftm, req_tlvs, req_len, dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	/* handle flags, flags_mask */
	if (dig->tlv_data.config.flags) {
		uint32 mask;
		uint32 flags;
		uint bit_pos;
		ftm->config->scratch_flags = ftm->config->flags;
		flags = ltoh32_ua(dig->tlv_data.config.flags);
		if (dig->tlv_data.config.flags_mask)
			mask = ltoh32_ua(dig->tlv_data.config.flags_mask);
		else
			mask = WL_PROXD_FLAG_ALL;

		FTM_UPDATE_FLAGS(ftm->config->scratch_flags, flags, mask);
		mask &= ~(WL_PROXD_FLAG_RX_ENABLED | WL_PROXD_FLAG_SECURE);
		FTM_UPDATE_FLAGS(ftm->config->flags, flags, mask);
		/* note: per bss options are configured via method, but stored in
		 * per/bss space (currently ftm->enabled_bss)
		 */

		/* clear responder bit if rx is disabled; flags don't change often, so
		 * we don't optimize for no change of rx enable state.
		 */
		bit_pos = FTM_BSSCFG_OPTION_BIT(bsscfg, FTM_BSSCFG_OPTION_RX);
		if (FTM_RX_ENABLED(ftm->config->scratch_flags)) {
			ftm->config->flags |= WL_PROXD_FLAG_RX_ENABLED;
			setbit(ftm->enabled_bss, bit_pos);
		} else {
			clrbit(ftm->enabled_bss, bit_pos);
		}
		if (!pdftm_need_proxd(ftm, FTM_BSSCFG_OPTION_RX)) {
			ftm->config->flags &= ~WL_PROXD_FLAG_RX_ENABLED;
		}
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_FTM_RESPONDER,
			isset(ftm->enabled_bss, bit_pos));

		bit_pos = FTM_BSSCFG_OPTION_BIT(bsscfg, FTM_BSSCFG_OPTION_SECURE);
		if (FTM_SECURE(ftm->config->scratch_flags)) {
			ftm->config->flags |= WL_PROXD_FLAG_SECURE;
			setbit(ftm->enabled_bss, bit_pos);
		} else {
			clrbit(ftm->enabled_bss, bit_pos);
		}
		if (!pdftm_need_proxd(ftm, FTM_BSSCFG_OPTION_SECURE)) {
			ftm->config->flags &= ~WL_PROXD_FLAG_SECURE;
		}

		if (!BSSCFG_HAS_NOIF(bsscfg) && bsscfg->up &&
			(BSSCFG_AP(bsscfg) || BSSCFG_IBSS(bsscfg))) {
			wlc_bss_update_beacon(ftm->wlc, bsscfg);
			wlc_bss_update_probe_resp(ftm->wlc, bsscfg, TRUE);
		}
	}

	if (dig->tlv_data.config.event_mask)
		ftm->config->event_mask = ltoh32_ua(dig->tlv_data.config.event_mask);

	if (dig->tlv_data.config.rx_max_burst) {
		ftm->config->rx_max_burst = ftm_iov_adjust_num_burst(ftm,
			ltoh16_ua(dig->tlv_data.config.rx_max_burst));
	}

	sncfg = ftm->config->session_defaults;
	err = ftm_iov_session_config_from_dig(ftm, dig, sncfg, bsscfg);
	if (err != BCME_OK)
		goto done;

#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
	if (dig->tlv_data.config.debug_mask)
		ftm->config->debug = ltoh32_ua(dig->tlv_data.config.debug_mask);
#endif // endif

	if (dig->DIG_DEV_ADDR)
		memcpy(&ftm->config->dev_addr, dig->DIG_DEV_ADDR,
			sizeof(ftm->config->dev_addr));

done:
	return err;
}

static int
ftm_iov_session_config(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	pdftm_session_t *sn = NULL;
	pdftm_session_config_t *sncfg;
	bool newsn = FALSE;

	/* allocate session first. note: dig may be reset by this call */
	err = pdftm_alloc_session(ftm, bsscfg, sid, &sn, &newsn);
	if (err != BCME_OK)
		goto done;

	dig = ftm->dig;
	err = ftm_iov_unpack(ftm, req_tlvs, req_len, dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	/* update bsscfg for session in case it was overridden */
	if (newsn)
		sn->bsscfg = bsscfg;

	/* disallow config once session starts */
	if (sn->state >= WL_PROXD_SESSION_STATE_STARTED) {
		err = WL_PROXD_E_BAD_STATE;
		goto done;
	}

	sncfg = sn->config;
	err = ftm_iov_session_config_from_dig(ftm, dig, sncfg, bsscfg);
	if (err != BCME_OK)
		goto done;

	err = pdftm_change_session_state(sn, BCME_OK, WL_PROXD_SESSION_STATE_CONFIGURED);
	if (err != BCME_OK)
		goto done;

done:
	if (err != BCME_OK && sn != NULL) {
		if (newsn)
			pdftm_free_session(&sn);
		else
			pdftm_stop_session(sn, err);
	}

	return err;
}

static int
ftm_iov_session_start(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;
	pdftm_session_t *sn  = NULL;
	pdftm_session_config_t *sncfg;
	bool newsn = FALSE;

	/* allocate session first. note: dig may be reset by this call */
	err = pdftm_alloc_session(ftm, bsscfg, sid, &sn, &newsn);
	if (err != BCME_OK)
	    goto done;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	/* update bsscfg for session in case it was overridden */
	if (newsn)
		sn->bsscfg = bsscfg;

	/* if a new session was created, work with ftm defaults */
	sncfg = sn->config;
	BCM_REFERENCE(sncfg);

	if (newsn) {
		err = pdftm_change_session_state(sn, BCME_OK, WL_PROXD_SESSION_STATE_CONFIGURED);
		if (err != BCME_OK)
			goto done;
	}

	err = pdftm_start_session(sn);
	if (err != BCME_OK)
		goto done;

	if (FTM_SESSION_DELAYED(sn)) {
		FTM_CNT(ftm, sn, sched_fail);
		err = WL_PROXD_E_SCHED_FAIL;
		goto done;
	}

done:
	if (err != BCME_OK && sn != NULL) {
		if (newsn)
			pdftm_free_session(&sn);
		else
			pdftm_stop_session(sn, err);
	}

	return err;
}

static int
ftm_iov_session_stop(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
	    goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
	    err = BCME_DISABLED;
	    goto done;
	}

	err = wlc_ftm_stop_session(ftm, sid, WL_PROXD_E_CANCELED);

done:
	return err;
}

static int
ftm_iov_clear_counters(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	bzero(ftm->cnt, sizeof(*ftm->cnt));
done:
	return err;
}

static int
ftm_iov_clear_session_counters(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;
	pdftm_session_t *sn = NULL;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
	    goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
	    err = BCME_DISABLED;
	    goto done;
	}

	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	bzero(sn->cnt, sizeof(*sn->cnt));
done:
	return err;
}

static int
ftm_iov_delete_session(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

#ifdef WL_RANGE_SEQ
	/* The sequence of commands to trigger ranging requires us to issue ftm delete
	 * request first. This deletes the previous session states. For the first session,
	 * however, there is no previous session state created. So, instead of flagging
	 * it as an error, we continue with valid status
	 */
	if (!FTM_SESSION_FOR_SID(ftm, sid)) {
		err = BCME_OK;
		goto done;
	}
#endif /* WL_RANGE_SEQ */
	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	err = wlc_ftm_delete_session(ftm, sid);

done:
	return err;
}

static int
ftm_iov_burst_request(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err;
	pdftm_session_t *sn = NULL;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	err = pdftm_change_session_state(sn, BCME_OK, WL_PROXD_SESSION_STATE_DELAY);
	if (err != BCME_OK)
		goto done;

	if (FTM_SESSION_DELAYED(sn)) {
		FTM_CNT(ftm, sn, sched_fail);
		err = WL_PROXD_E_SCHED_FAIL;
		goto done;
	}

done:
	if (err != BCME_OK && sn != NULL)
		err = pdftm_stop_session(sn, err);

	return err;
}

#ifdef WL_FTM_RANGE
/* ranging */

/*
* This function is called from ftm_iov_start_ranging() after
* unpacking tlvs-request into the digest structure.
* If succeeds, create and return a ranging context
*/
static int
ftm_iov_ranging_ctx_from_dig(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	ftm_iov_tlv_digest_t *dig, wlc_ftm_ranging_ctx_t **out_rctx)
{
	int err = BCME_OK;
	wlc_ftm_ranging_ctx_t *rctx = NULL;
	wl_proxd_session_id_list_t	*arg_ranging_sids;
	uint16	num_sids = 0;
	int	i;
	wl_proxd_session_id_t	sid;

	*out_rctx = (wlc_ftm_ranging_ctx_t *) NULL;	/* init */

	/* verify the input-request arguments */
	arg_ranging_sids = dig->tlv_data.config.sid_list;
	if (arg_ranging_sids == NULL) {
		err = BCME_BADARG;
		goto done;
	}

	/* validate all sessions for ranging */
	num_sids = ltoh16_ua((uint8 *) &arg_ranging_sids->num_ids);
	if (num_sids == 0) {
		err = BCME_BADARG;
		goto done;
	}
	/* make sure input req-tlvs are valid */
	if (&arg_ranging_sids->ids[num_sids] > (const wl_proxd_session_id_t *)dig->req_tlvs_end) {
		err = BCME_BADARG;
		goto done;
	}

	/* create a ranging context */
	err = wlc_ftm_ranging_create(ftm, (wlc_ftm_ranging_cb_t) NULL,
		(void *) NULL, &rctx);
	if (err != BCME_OK)
		goto done;

	/* add requested session ids to the ranging context */
	for (i = 0; i < num_sids; i++) {
		sid = (wl_proxd_session_id_t) ltoh16_ua((uint8 *) &arg_ranging_sids->ids[i]);
		if (sid == WL_PROXD_SESSION_ID_GLOBAL) {
			err = BCME_BADARG;
			goto done;
		}

		err = wlc_ftm_ranging_add_sid(rctx, bsscfg, &sid);
		if (err != BCME_OK) {
			FTM_LOGIOV(ftm, (("wl%d: %s: failed to add sid %d for ranging, err=%d\n",
				FTM_UNIT(ftm), __FUNCTION__, sid, err)));
			goto done;
		}
	}

	/* handle flags, flags_mask */
	if (dig->tlv_data.config.ranging_flags) {
		uint16 mask;
		uint16 flags;

		flags = ltoh16_ua(dig->tlv_data.config.ranging_flags);
		if (dig->tlv_data.config.ranging_flags_mask)
			mask = ltoh16_ua(dig->tlv_data.config.ranging_flags_mask);
		else
			mask = WL_PROXD_RANGING_FLAG_ALL;
		err = wlc_ftm_ranging_set_flags(rctx, flags, mask);
	}

	/* success */
	*out_rctx = rctx;

done:
	if (err != BCME_OK && rctx != (wlc_ftm_ranging_ctx_t *) NULL) {
		/* cleanup */
		/* destroy the ranging ctx */
		wlc_ftm_ranging_destroy(&rctx);
	}
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d, add %d sids for ranging\n",
		FTM_UNIT(ftm), __FUNCTION__, err, num_sids)));

	return err;
}

/* start ranging */
static int
ftm_iov_start_ranging(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err, err2;

	ASSERT(sid == WL_PROXD_SESSION_ID_GLOBAL);

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	/* only allow starting one ranging at one time */
	if (ftm->rctx != (wlc_ftm_ranging_ctx_t *) NULL) {
		FTM_LOGIOV(ftm, (("wl%d: %s: FTM ranging is already active for bss index %d\n",
			FTM_UNIT(ftm), __FUNCTION__, WLC_BSSCFG_IDX(bsscfg))));
		err = BCME_BUSY;
		goto done;
	}

	/* create a ranging context */
	err = ftm_iov_ranging_ctx_from_dig(ftm, bsscfg, ftm->dig, &ftm->rctx);
	if (err != BCME_OK)
		goto done;

	/* start ranging */
	err = wlc_ftm_ranging_start(ftm->rctx);
	if (err != BCME_OK) {
		FTM_LOGIOV(ftm, (("wl%d: %s: failed to start FTM ranging, err=%d\n",
			FTM_UNIT(ftm), __FUNCTION__, err)));

		/* clean up */
		err2 = wlc_ftm_ranging_set_flags(ftm->rctx,
			WL_PROXD_RANGING_FLAG_NONE, WL_PROXD_RANGING_FLAG_DEL_SESSIONS_ON_STOP);
		if (err2 != BCME_OK)
			FTM_LOGIOV(ftm, (("wl%d: %s: failed to reset FTM ranging flags, err=%d\n",
			FTM_UNIT(ftm), __FUNCTION__, err2)));

		/* destroy the ranging ctx */
		wlc_ftm_ranging_destroy(&ftm->rctx);
	}

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));

	return err;
}

/* stop ranging */
static int
ftm_iov_stop_ranging(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err = BCME_OK, err2 = BCME_OK;

	ASSERT(sid == WL_PROXD_SESSION_ID_GLOBAL);

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	/* cancel ranging for the context */
	if (ftm->rctx == (wlc_ftm_ranging_ctx_t *) NULL) {
		FTM_LOGIOV(ftm, (("wl%d: %s: FTM ranging is not active for bss index %d\n",
			FTM_UNIT(ftm), __FUNCTION__, WLC_BSSCFG_IDX(bsscfg))));
		goto done;
	}

	err = wlc_ftm_ranging_cancel(ftm->rctx);
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: ranging cancelled, status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));

	/* destroy the ranging ctx, delete sessions if configured */
	err2 = wlc_ftm_ranging_destroy(&ftm->rctx);

	if (err == BCME_OK)
		err = err2;

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));

	return err;
}
#endif /* WL_FTM_RANGE */

/* get handlers support */

static void
ftm_iov_pack_uint8(const ftm_iov_tlv_digest_t *dig, uint8 **buf, const uint8 *data, int len)
{
	BCM_REFERENCE(dig);
	ASSERT(buf && *buf);
	memcpy(*buf, data, len);
	*buf += len;
}

static void
ftm_iov_pack_uint16(const ftm_iov_tlv_digest_t *dig, uint8 **buf, const uint8 *data, int len)
{
	uint16 v = *((const uint16 *)data);

	BCM_REFERENCE(dig);
	ASSERT(buf && *buf);
	ASSERT(len == sizeof(v));
	htol16_ua_store(v, *buf);
	*buf += sizeof(v);
}

static void
ftm_iov_pack_uint32(const ftm_iov_tlv_digest_t *dig, uint8 **buf, const uint8 *data, int len)
{
	uint32 v = *((const uint32 *)data);

	BCM_REFERENCE(dig);
	ASSERT(len == sizeof(v));
	ASSERT(buf && *buf);
	htol32_ua_store(v, *buf);
	*buf += sizeof(v);
}

/* note: uint64 packed as two uint32s separately - lo and then hi */

static void
ftm_iov_pack_intvl(const ftm_iov_tlv_digest_t *dig, uint8 **buf, const uint8 *data, int len)
{
	const wl_proxd_intvl_t *intvl;

	STATIC_ASSERT(sizeof(intvl->intvl) == sizeof(uint32));
	STATIC_ASSERT(sizeof(intvl->tmu) == sizeof(uint16));

	intvl = (const wl_proxd_intvl_t *)data;
	ASSERT(len == sizeof(*intvl));

	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&intvl->intvl, sizeof(uint32));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&intvl->tmu, sizeof(uint16));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&intvl->pad, sizeof(intvl->pad));
}

static void
ftm_iov_pack_info(const ftm_iov_tlv_digest_t *dig, uint8 **buf, const uint8 *data, int len)
{
	const wl_proxd_ftm_info_t *info;

	STATIC_ASSERT(sizeof(info->caps) == sizeof(uint16));
	STATIC_ASSERT(sizeof(info->max_sessions) == sizeof(uint16));
	STATIC_ASSERT(sizeof(info->num_sessions) == sizeof(uint16));

	ASSERT(len == sizeof(*info));

	info = (const wl_proxd_ftm_info_t *)data;
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&info->caps, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&info->max_sessions, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&info->num_sessions, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *) &info->rx_max_burst, sizeof(uint16));
}

static void
ftm_iov_pack_session_info(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	const wl_proxd_ftm_session_info_t *info;
	STATIC_ASSERT(sizeof(info->sid) == sizeof(uint16));
	STATIC_ASSERT(sizeof(info->state) == sizeof(uint16));
	STATIC_ASSERT(sizeof(info->status) == sizeof(uint32));

	info = (const wl_proxd_ftm_session_info_t *)data;
	ASSERT(len == sizeof(*info));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&info->sid, sizeof(uint16));
	ftm_iov_pack_uint8(dig, buf, &info->bss_index, sizeof(uint8));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&info->pad, sizeof(info->pad));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&info->bssid, sizeof(info->bssid));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&info->state, sizeof(uint16));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&info->status, sizeof(uint32));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&info->burst_num, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&info->wait_reason, sizeof(info->wait_reason));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&info->meas_start_lo,
		sizeof(info->meas_start_lo));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&info->meas_start_hi,
		sizeof(info->meas_start_hi));
}

static void
ftm_iov_pack_session_status(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	const wl_proxd_ftm_session_status_t *status;
	STATIC_ASSERT(sizeof(status->sid) == sizeof(uint16));
	STATIC_ASSERT(sizeof(status->state) == sizeof(uint16));
	STATIC_ASSERT(sizeof(status->status) == sizeof(uint32));

	status = (const wl_proxd_ftm_session_status_t *)data;
	ASSERT(len == sizeof(*status));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&status->sid, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&status->state, sizeof(uint16));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&status->status, sizeof(uint32));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&status->burst_num, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&status->pad, sizeof(status->pad));
}

#define CNTPACK32(_field) do { ftm_iov_pack_uint32(dig, buf, (const uint8 *)&cnt->_field, \
	sizeof(uint32));} while (0)

static void
ftm_iov_pack_counters(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	const wl_proxd_counters_t *cnt;

	cnt = (const wl_proxd_counters_t *)data;
	ASSERT(len == sizeof(wl_proxd_counters_t));
	CNTPACK32(tx);
	CNTPACK32(rx);
	CNTPACK32(burst);
	CNTPACK32(sessions);
	CNTPACK32(max_sessions);
	CNTPACK32(sched_fail);
	CNTPACK32(timeouts);
	CNTPACK32(protoerr);
	CNTPACK32(noack);
	CNTPACK32(txfail);
	CNTPACK32(lci_req_tx);
	CNTPACK32(lci_req_rx);
	CNTPACK32(lci_rep_tx);
	CNTPACK32(lci_rep_rx);
	CNTPACK32(civic_req_tx);
	CNTPACK32(civic_req_rx);
	CNTPACK32(civic_rep_tx);
	CNTPACK32(civic_rep_rx);
	CNTPACK32(rctx);
	CNTPACK32(rctx_done);
	CNTPACK32(publish_err);
	CNTPACK32(on_chan);
	CNTPACK32(off_chan);
	CNTPACK32(tsf_lo);
	CNTPACK32(tsf_hi);
	CNTPACK32(num_meas);
}
#undef CNTPACK32

#ifdef WL_FTM_RANGE
static void
ftm_iov_pack_ranging_info(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
    const uint8 *data, int len)
{
	const wl_proxd_ranging_info_t *ri;

	ri = dig->tlv_data.out.ranging_info;

	ftm_iov_pack_uint32(dig, buf, (const uint8*)&ri->status, sizeof(uint32));
	ftm_iov_pack_uint16(dig, buf, (const uint8*)&ri->state, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8*) &ri->flags, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8*)&ri->num_sids, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8*)&ri->num_done, sizeof(uint16));
}
#endif /* WL_FTM_RANGE */

static void
ftm_iov_pack_rtt_sample(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const wl_proxd_rtt_sample_t *s)
{
	STATIC_ASSERT(sizeof(s->version) == sizeof(uint16));
	STATIC_ASSERT(sizeof(s->length) == sizeof(uint16));
	STATIC_ASSERT(sizeof(s->rtt) == sizeof(wl_proxd_intvl_t));
	STATIC_ASSERT(sizeof(s->rssi) == sizeof(wl_proxd_rssi_t));
	STATIC_ASSERT(sizeof(s->snr) == sizeof(wl_proxd_snr_t));
	STATIC_ASSERT(sizeof(s->bitflips) == sizeof(wl_proxd_bitflips_t));
	STATIC_ASSERT(sizeof(s->ratespec) == sizeof(uint32));
	STATIC_ASSERT(sizeof(s->status) == sizeof(uint32));
	STATIC_ASSERT(sizeof(s->distance) == sizeof(int32));
	STATIC_ASSERT(sizeof(s->tof_phy_error) == sizeof(wl_proxd_phy_error_t));
	STATIC_ASSERT(sizeof(s->tof_tgt_phy_error) == sizeof(wl_proxd_phy_error_t));
	STATIC_ASSERT(sizeof(s->tof_tgt_snr) == sizeof(wl_proxd_snr_t));
	STATIC_ASSERT(sizeof(s->tof_tgt_bitflips) == sizeof(wl_proxd_bitflips_t));
	STATIC_ASSERT(sizeof(s->coreid) == sizeof(uint8));
	STATIC_ASSERT(sizeof(s->chanspec) == sizeof(uint32));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&s->version, sizeof(s->version));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&s->length, sizeof(s->length));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&s->id, sizeof(s->id));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&s->flags, sizeof(s->flags));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&s->rssi, sizeof(s->rssi));
	ftm_iov_pack_intvl(dig, buf, (const uint8 *)&s->rtt, sizeof(s->rtt));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&s->ratespec, sizeof(s->ratespec));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&s->snr, sizeof(s->snr));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&s->bitflips, sizeof(s->bitflips));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&s->status, sizeof(s->status));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&s->distance, sizeof(s->distance));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&s->tof_phy_error, sizeof(s->tof_phy_error));
	ftm_iov_pack_uint32(dig, buf,
		(const uint8 *)&s->tof_tgt_phy_error, sizeof(s->tof_tgt_phy_error));
	ftm_iov_pack_uint16(dig, buf,
		(const uint8 *)&s->tof_tgt_snr, sizeof(s->tof_tgt_snr));
	ftm_iov_pack_uint16(dig, buf,
		(const uint8 *)&s->tof_tgt_bitflips, sizeof(s->tof_tgt_bitflips));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&s->coreid, sizeof(s->coreid));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&s->pad, sizeof(s->pad));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&s->chanspec, sizeof(s->chanspec));
}

#ifdef WL_RANGE_SEQ
static void
ftm_iov_pack_rtt_sample_v2(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const wl_proxd_rtt_sample_t *s, uint32 chanspec)
{
	ftm_iov_pack_rtt_sample(dig, buf, s);
}
#endif /* WL_RANGE_SEQ */

static uint16
ftm_iov_get_rtt_result_len(const ftm_iov_tlv_digest_t *dig,
	const ftm_iov_tlv_dig_info_t *tlv_info)
{
	uint16 len;
	const wl_proxd_rtt_result_t *r;

	r = dig->tlv_data.out.rtt_result;
	len = OFFSETOF(wl_proxd_rtt_result_t, rtt);
	/* rtt[0] is the avg_rtt */
	len += (r->num_rtt + 1) * sizeof(wl_proxd_rtt_sample_t);
	return len;
}

#ifdef WL_RANGE_SEQ
static uint16
ftm_iov_get_rtt_result_v2_len(const ftm_iov_tlv_digest_t *dig,
	const ftm_iov_tlv_dig_info_t *tlv_info)
{
	BCM_REFERENCE(tlv_info);
	uint16 len;
	const wl_proxd_rtt_result_t *r;

	r = dig->tlv_data.out.rtt_result;
	len = OFFSETOF(wl_proxd_rtt_result_v2_t, rtt);
	/* avg_rtt is the first element of rtt[] */
	len += (r->num_rtt + 1u) * sizeof(wl_proxd_rtt_sample_v2_t);
	return len;
}

static void
ftm_iov_pack_rtt_result_v2(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	const wl_proxd_rtt_result_t *r;
	const wl_proxd_rtt_sample_t *samples;
	int i;
	uint32 chanspec = 0;
	uint16 version = WL_PROXD_RTT_RESULT_VERSION_2;
	uint16 length = sizeof(wl_proxd_rtt_result_v2_t) -
		OFFSETOF(wl_proxd_rtt_result_v2_t, sid);

	ASSERT(len >= (int)OFFSETOF(wl_proxd_rtt_result_v2_t, rtt));
	r = (const wl_proxd_rtt_result_t *)data;

	ftm_iov_pack_uint16(dig, buf, (const uint8*)&version, sizeof(version));
	ftm_iov_pack_uint16(dig, buf, (const uint8*)&length, sizeof(length));
	ftm_iov_pack_uint16(dig, buf, (const uint8*)&r->sid, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->flags, sizeof(uint16));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&r->status, sizeof(uint32));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->peer, sizeof(r->peer));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->state, sizeof(wl_proxd_session_state_t));
	ftm_iov_pack_intvl(dig, buf, (const uint8 *)&r->u.retry_after, sizeof(r->u.retry_after));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&r->avg_dist, sizeof(uint32));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->sd_rtt, sizeof(uint16));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->num_valid_rtt, sizeof(uint8));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->num_ftm, sizeof(uint8));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->burst_num, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->num_rtt, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->num_meas, sizeof(uint16));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->pad, sizeof(r->pad));

	ASSERT(dig->session->ftm_state != NULL);

	chanspec = dig->session->config->burst_config->chanspec;

	/* avg_rtt is the first element */
	ftm_iov_pack_rtt_sample_v2(dig, buf, &r->rtt[0], chanspec);

	samples = dig->session->ftm_state->rtt;
	/* note: num_rtt taken from input, as it is not needed in all cases */
	for (i = 0; i < MIN(dig->session->ftm_state->num_rtt, r->num_rtt); ++i) {
		ftm_iov_pack_rtt_sample_v2(dig, buf, &samples[i], chanspec);
	}
}
#endif /* WL_RANGE_SEQ */

static void
ftm_iov_pack_rtt_result(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	const wl_proxd_rtt_result_t *r;
	const wl_proxd_rtt_sample_t *samples;
	int i;

	STATIC_ASSERT(sizeof(r->version) == sizeof(uint16));
	STATIC_ASSERT(sizeof(r->length) == sizeof(uint16));
	STATIC_ASSERT(sizeof(r->sid) == sizeof(uint16));
	STATIC_ASSERT(sizeof(r->flags) == sizeof(uint16));
	STATIC_ASSERT(sizeof(r->status) == sizeof(uint32));
	STATIC_ASSERT(sizeof(r->avg_dist) == sizeof(uint32));
	STATIC_ASSERT(sizeof(r->burst_num) == sizeof(uint16));
	STATIC_ASSERT(sizeof(r->num_rtt) == sizeof(uint16));

	ASSERT(len >= (int)OFFSETOF(wl_proxd_rtt_result_t, rtt));
	r = (const wl_proxd_rtt_result_t *)data;

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->version, sizeof(r->version));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->length, sizeof(r->length));
	ftm_iov_pack_uint16(dig, buf, (const uint8*)&r->sid, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->flags, sizeof(uint16));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&r->status, sizeof(uint32));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->peer, sizeof(r->peer));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->state, sizeof(wl_proxd_session_state_t));
	ftm_iov_pack_intvl(dig, buf, (const uint8 *)&r->u.retry_after, sizeof(r->u.retry_after));

	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&r->avg_dist, sizeof(uint32));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->sd_rtt, sizeof(uint16));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->num_valid_rtt, sizeof(uint8));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->num_ftm, sizeof(uint8));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->burst_num, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->num_rtt, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&r->num_meas, sizeof(uint16));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&r->pad, sizeof(r->pad));

	ASSERT(dig->session->ftm_state != NULL);

	/* rtt[0] is avg_rtt */
	ftm_iov_pack_rtt_sample(dig, buf, &dig->session->ftm_state->avg_rtt);

	samples = dig->session->ftm_state->rtt;
	/* note: num_rtt taken from input, as it is not needed in all cases */
	for (i = 0; i < MIN(dig->session->ftm_state->num_rtt, r->num_rtt); ++i) {
		ftm_iov_pack_rtt_sample(dig, buf, &samples[i]);
	}
}

static uint16
ftm_iov_get_sessions_len(const ftm_iov_tlv_digest_t *dig,
	const ftm_iov_tlv_dig_info_t *tlv_info)
{
	uint16 len;
	uint16 num_sessions;

	len = OFFSETOF(wl_proxd_session_id_list_t, ids);
	num_sessions = dig->tlv_data.out.ranging_info ? dig->tlv_data.out.ranging_info->num_sids :
		dig->ftm->cnt->sessions;
	len += num_sessions * sizeof(wl_proxd_session_id_t);
	return len;
}

static void
ftm_iov_pack_default_sessions(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	int i;
	uint16 nsn;
	const pdftm_t *ftm;
	uint8 *buf_nsn;

	ASSERT(len >= (int)OFFSETOF(wl_proxd_session_id_list_t, ids));

	BCM_REFERENCE(data);
	ftm = dig->ftm;
	buf_nsn = *buf; /* reserve space for nsn */
	*buf += sizeof(uint16);
	for (i = 0, nsn = 0; i < ftm->max_sessions; ++i) {
		pdftm_session_t *sn = ftm->sessions[i];
		if (!FTM_VALID_SESSION(sn))
			continue;
		ftm_iov_pack_uint16(dig, buf, (const uint8 *)&sn->sid, sizeof(uint16));
		++nsn;
	}

	ASSERT(nsn == ftm->cnt->sessions);

	/* update the num sessions */
	ftm_iov_pack_uint16(dig, &buf_nsn, (const uint8*)&nsn, sizeof(uint16));
}

static void
ftm_iov_pack_sessions(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
#ifdef WL_FTM_RANGE
	if (dig->tlv_data.out.ranging_info) {
		pdftm_ranging_pack_sessions(dig->ftm->rctx, buf, len);
	}
	else
#endif /* WL_FTM_RANGE */
	{
		ftm_iov_pack_default_sessions(dig, buf, data, len);
	}
}

static uint16
ftm_iov_get_ftm_req_len(const ftm_iov_tlv_digest_t *dig,
	const ftm_iov_tlv_dig_info_t *tlv_info)
{
	return dig->tlv_data.out.ftm_req_len;
}

static void
ftm_iov_pack_ftm_req(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	ASSERT(data == dig->tlv_data.out.ftm_req);
	ASSERT(len == dig->tlv_data.out.ftm_req_len);
	ftm_iov_pack_uint8(dig, buf, data, len);
}

#ifdef WL_FTM_11K
static uint16
ftm_iov_get_lci_rep_len(const ftm_iov_tlv_digest_t *dig,
	const ftm_iov_tlv_dig_info_t *tlv_info)
{
	return dig->DIG_LCI_REP_LEN;
}
static uint16
ftm_iov_get_civic_rep_len(const ftm_iov_tlv_digest_t *dig,
	const ftm_iov_tlv_dig_info_t *tlv_info)
{
	return dig->DIG_CIVIC_REP_LEN;
}

static void
ftm_iov_pack_lci_rep(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	ASSERT(data == dig->DIG_LCI_REP);

	ASSERT(len == dig->DIG_LCI_REP_LEN);
	ftm_iov_pack_uint8(dig, buf, data, len);
}

static void
ftm_iov_pack_civic_rep(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
const uint8 *data, int len)
{
	ASSERT(data == dig->DIG_CIVIC_REP);

	ASSERT(len == dig->DIG_CIVIC_REP_LEN);
	ftm_iov_pack_uint8(dig, buf, data, len);
}
#endif /* WL_FTM_11K */
static bool
ftm_iov_pack_get_next(void *ctx, uint16 *tlv_id, uint16 *tlv_len)
{
	ftm_iov_tlv_digest_t *dig;
	const ftm_iov_tlv_dig_info_t *tlv_info;

	dig = (ftm_iov_tlv_digest_t *)ctx;
	ASSERT(dig != NULL);

	ASSERT(dig->next_tlv_idx < dig->num_tlvs);
	ASSERT(tlv_id && tlv_len);

	*tlv_id = dig->tlv_ids[dig->next_tlv_idx++];
	tlv_info = ftm_iov_get_dig_info(*tlv_id);
	if (tlv_info) {
		*tlv_len = (tlv_info->get_len) ? (*tlv_info->get_len)(dig, tlv_info) :
			tlv_info->len;
	} else {
		*tlv_len = 0;
	}

	dig->cur_tlv_info = tlv_info;
	return (dig->next_tlv_idx < dig->num_tlvs);
}

static void
ftm_iov_pack_pack_next(void *ctx, uint16 id, uint16 len, uint8* buf)
{
	ftm_iov_tlv_digest_t *dig;

	dig = (ftm_iov_tlv_digest_t *)ctx;
	ASSERT(dig && dig->cur_tlv_info && dig->cur_tlv_info->pack);

	if (dig->cur_tlv_info) {
		(*dig->cur_tlv_info->pack)(dig, &buf,
			(*(uint8 **)((uint8 *)dig + dig->cur_tlv_info->dig_off)), len);
	}
}

static int
ftm_iov_session_config_to_dig(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	pdftm_session_config_t *sncfg, ftm_iov_tlv_digest_t *tlv_dig)
{
	pdftm_iov_tlv_config_digest_t *dig;
	uint bit_pos;
	ftm->config->scratch_flags = ftm->config->flags;

	dig = &tlv_dig->tlv_data.config;

#define BCF(_sncfg, _fld) (_sncfg)->burst_config->_fld

	dig->session_flags = &sncfg->flags;
	dig->event_mask = &sncfg->event_mask;
	dig->chanspec = &BCF(sncfg, chanspec);
	dig->tx_power = &BCF(sncfg, tx_power);
	dig->ratespec = &BCF(sncfg, ratespec);
	dig->burst_duration = &BCF(sncfg, duration);
	dig->burst_num_ftm = &BCF(sncfg, num_ftm);
	dig->burst_period = &sncfg->burst_period;
	dig->num_burst = &sncfg->num_burst;
	dig->burst_ftm_sep = &BCF(sncfg, ftm_sep);
	dig->burst_timeout = &BCF(sncfg, timeout);
	dig->ftm_retries = &BCF(sncfg, ftm_retries);
	dig->init_delay = &sncfg->init_delay;
	dig->peer_mac = &BCF(sncfg, peer_mac);
	dig->ftm_req_retries = &BCF(sncfg, ftm_req_retries);

	/* following inherited from method */
	bit_pos = FTM_BSSCFG_OPTION_BIT(bsscfg, FTM_BSSCFG_OPTION_RX);
	if (isset(ftm->enabled_bss, bit_pos))
		ftm->config->scratch_flags |= WL_PROXD_FLAG_RX_ENABLED;
	else
		ftm->config->scratch_flags &= ~WL_PROXD_FLAG_RX_ENABLED;

	bit_pos = FTM_BSSCFG_OPTION_BIT(bsscfg, FTM_BSSCFG_OPTION_SECURE);
	if (isset(ftm->enabled_bss, bit_pos))
		ftm->config->scratch_flags |= WL_PROXD_FLAG_SECURE;
	else
		ftm->config->scratch_flags &= ~WL_PROXD_FLAG_SECURE;

	dig->flags = &ftm->config->scratch_flags;
	dig->debug_mask = &ftm->config->debug;

#undef BCF

	return BCME_OK;
}

/* helper to list tlv ids */
#define ID(A) WL_PROXD_TLV_ID_ ## A
#define ID2(A, B) ID(A), ID(B)
#define ID3(A, B, C) ID2(A, B), WL_PROXD_TLV_ID_ ## C
#define ID4(A, B, C, D) ID3(A, B, C), WL_PROXD_TLV_ID_ ## D

static int
ftm_iov_get_version(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	*rsp_len = 0;
	return BCME_OK;
}

static int
ftm_iov_get_info(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	pdftm_session_config_t *sncfg;
	wl_proxd_ftm_info_t ftm_info;
	const uint16 tlv_ids[] = {
		ID4(FLAGS, EVENT_MASK, DEBUG_MASK, SESSION_FLAGS),
		ID4(CHANSPEC, TX_POWER, RATESPEC, BURST_DURATION),
		ID4(BURST_NUM_FTM, BURST_PERIOD, NUM_BURST, BURST_FTM_SEP),
		ID4(FTM_RETRIES, INIT_DELAY, PEER_MAC, INFO),
		ID2(FTM_REQ_RETRIES, BURST_TIMEOUT),
		ID(DEV_ADDR)
	};
	int	tlvs_len;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
	    goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
	    err = BCME_DISABLED;
	    goto done;
	}

	/* initialize digest and populate fields */
	dig = pdftm_iov_dig_init(ftm, NULL);
	sncfg = ftm->config->session_defaults;
	err = ftm_iov_session_config_to_dig(ftm, bsscfg, sncfg, dig);
	if (err != BCME_OK)
		goto done;

	dig->tlv_data.out.info = &ftm_info;
	ftm_info.caps = ftm->caps;
	ftm_info.max_sessions = ftm->max_sessions;
	ftm_info.num_sessions = ftm->num_sessions;
	ftm_info.rx_max_burst = ftm->config->rx_max_burst;

	dig->DIG_DEV_ADDR = (uint8 *) &ftm->config->dev_addr;

	tlvs_len = 0;
	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids), dig, &tlvs_len);
	if (err != BCME_OK)
		goto done;
	*rsp_len = tlvs_len;

done:
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));

	return err;
}

static int
ftm_iov_get_session_info(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	wl_proxd_ftm_session_info_t session_info, *sip = &session_info;
	pdftm_session_t *sn;
	const uint16 tlv_ids[] = {
		ID3(SESSION_FLAGS, EVENT_MASK, DEBUG_MASK),
		ID4(CHANSPEC, TX_POWER, RATESPEC, BURST_DURATION),
		ID4(BURST_NUM_FTM, BURST_PERIOD, NUM_BURST, BURST_FTM_SEP),
		ID4(FTM_RETRIES, INIT_DELAY, PEER_MAC, SESSION_INFO),
		ID2(FTM_REQ_RETRIES, BURST_TIMEOUT),
		ID(CUR_ETHER_ADDR)
	};
	int	tlvs_len;

	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	/* no need to unpack - nothing expected */
	ASSERT(FTM_BSSCFG_FTM_ENABLED(ftm, sn->bsscfg));

	/* initialize digest and populate fields */
	dig = pdftm_iov_dig_init(ftm, NULL);
	err = ftm_iov_session_config_to_dig(ftm, bsscfg, sn->config, dig);
	if (err != BCME_OK)
		goto done;

	dig->tlv_data.out.session_info = sip;
	err = wlc_ftm_get_session_info(ftm, sid, sip);
	if (err != BCME_OK)
		goto done;
	dig->cur_ether_addr = &sn->config->burst_config->cur_ether_addr;

	tlvs_len = 0;
	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids), dig, &tlvs_len);
	if (err != BCME_OK)
		goto done;

	*rsp_len = tlvs_len;

done:
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));

	return err;
}

static int
ftm_iov_get_session_status(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	wl_proxd_ftm_session_status_t session_status, *ssp = &session_status;
	pdftm_session_t *sn;
	const uint16 tlv_ids[] = { ID(SESSION_STATUS) };

	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	/* no need to unpack - nothing expected */
	ASSERT(FTM_BSSCFG_FTM_ENABLED(ftm, sn->bsscfg));

	/* initialize digest and populate fields */
	dig = pdftm_iov_dig_init(ftm, NULL);
	err = ftm_iov_session_config_to_dig(ftm, bsscfg, sn->config, dig);
	if (err != BCME_OK)
		goto done;

	dig->tlv_data.out.session_status = ssp;
	ssp->sid = sid;
	ssp->state = sn->state;
	ssp->status = sn->status;
	ssp->burst_num = sn->ftm_state ? sn->ftm_state->burst_num : 0;
#ifdef NOTYET
	ssp->pad = 0;
#endif /* NOTYET */

	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids), dig, rsp_len);
	if (err != BCME_OK)
		goto done;

done:
	return err;
}

static int
ftm_iov_get_cnt(pdftm_t *ftm, const wl_proxd_counters_t *cnt,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	const uint16 tlv_ids[] = { ID(COUNTERS) };

	/* initialize digest and populate fields */
	dig = pdftm_iov_dig_init(ftm, NULL);
	dig->tlv_data.out.counters = cnt;

	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids), dig, rsp_len);
	if (err != BCME_OK)
		goto done;

done:
	return err;
}

static int
ftm_iov_get_counters(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	pdftm_time_t cur_tsf;

	/* update tsf in ftm counters */
	FTM_GET_TSF(ftm, cur_tsf);
	ftm->cnt->tsf_lo = (uint32)(cur_tsf & 0xffffffffULL);
	ftm->cnt->tsf_hi = (uint32)(cur_tsf >> 32);

	err =  ftm_iov_get_cnt(ftm, ftm->cnt, rsp_max, rsp_tlvs, rsp_len);

	return err;
}

static int
ftm_iov_get_session_counters(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	pdftm_session_t *sn;
	pdftm_time_t sn_tsf = 0;

	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	/* no need to unpack - nothing expected */
	ASSERT(FTM_BSSCFG_FTM_ENABLED(ftm, sn->bsscfg));

	/* update tsf in session counters */
	err = pdftm_get_session_tsf(sn, &sn_tsf);
	if (err == BCME_OK) {
		sn->cnt->tsf_lo = (uint32)(sn_tsf & 0xffffffffULL);
		sn->cnt->tsf_hi = (uint32)(sn_tsf >> 32);
	}

	err =  ftm_iov_get_cnt(ftm, sn->cnt, rsp_max, rsp_tlvs, rsp_len);
	if (err != BCME_OK)
		goto done;

done:
	return err;
}
#ifdef WL_FTM_11K
static int
ftm_iov_get_lci_civic_rep(pdftm_t *ftm, int rsp_max, wl_proxd_tlv_t *rsp_tlvs,
	uint16 tlv_id, uint8 *lci_civic_rep, uint16 lci_civic_rep_len,
	int *tlvs_len)
{
	ftm_iov_tlv_digest_t *dig;
	int err = BCME_OK;

	dig = pdftm_iov_dig_init(ftm, NULL);
	*tlvs_len = 0;

	ASSERT(tlv_id == WL_PROXD_TLV_ID_LCI || tlv_id == WL_PROXD_TLV_ID_CIVIC);

	if (!lci_civic_rep || lci_civic_rep_len == 0)
		goto done;	/* ignore if not available */

	if (tlv_id == WL_PROXD_TLV_ID_LCI) {
		dig->DIG_LCI_REP = lci_civic_rep;
		dig->DIG_LCI_REP_LEN = lci_civic_rep_len;
	}
	else { /* tlv_id == WL_PROXD_TLV_ID_CIVIC */
		dig->DIG_CIVIC_REP = lci_civic_rep;
		dig->DIG_CIVIC_REP_LEN = lci_civic_rep_len;
	}

	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, &tlv_id, 1, dig, tlvs_len);
	if (err != BCME_OK)
		goto done;
done:
	return err;

}
#endif /* WL_FTM_11K */

static int
ftm_iov_get_result(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	wl_proxd_rtt_result_t rtt_result, *rp = &rtt_result;
	pdftm_session_t *sn;
	const uint16 tlv_ids[] = { ID(RTT_RESULT_V2) };
	int tlvs_len;
#ifdef WL_FTM_11K
	int len;
#endif // endif

	sn = FTM_SESSION_FOR_SID(ftm, sid);
	if (!sn) {
		err = BCME_NOTFOUND;
		goto done;
	}

	/* no need to unpack - nothing expected */
	ASSERT(FTM_BSSCFG_FTM_ENABLED(ftm, sn->bsscfg));

	/* initialize digest and populate fields */
	dig = pdftm_iov_dig_init(ftm, sn);
	dig->tlv_data.out.rtt_result = rp;

	err = pdftm_get_session_result(ftm, sn, rp, 0);
	if (err != BCME_OK)
		goto done;

	/* provide samples if detail is requested; the samples are taken from sn ftm state */
	if (sn->config->flags & WL_PROXD_SESSION_FLAG_RTT_DETAIL)
		rp->num_rtt = sn->ftm_state->num_rtt;

	/* pack rtt-result */
	tlvs_len = 0;
	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids), dig, &tlvs_len);
	if (err != BCME_OK)
		goto done;
#ifdef WL_FTM_11K
	/* pack lci-report if available */
	err = ftm_iov_get_lci_civic_rep(ftm,
		rsp_max - tlvs_len, (wl_proxd_tlv_t *) ((char *) rsp_tlvs + tlvs_len),
		WL_PROXD_TLV_ID_LCI, sn->lci_rep, sn->lci_rep_len, &len);
	if (err != BCME_OK)
		goto done;
	tlvs_len += len;

	/* pack civic-report if available */
	err = ftm_iov_get_lci_civic_rep(ftm,
		rsp_max - tlvs_len, (wl_proxd_tlv_t *) ((char *) rsp_tlvs + tlvs_len),
		WL_PROXD_TLV_ID_CIVIC, sn->civic_rep, sn->civic_rep_len, &len);
	if (err != BCME_OK)
		goto done;
	tlvs_len += len;
#endif /* WL_FTM_11K */
	*rsp_len = tlvs_len;

done:
	return err;
}

static int
ftm_iov_get_sessions(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	const uint16 tlv_ids[] = { ID(SESSION_ID_LIST) };

	BCM_REFERENCE(sid);

	/* initialize digest and populate fields, sessions taken from ftm */
	dig = pdftm_iov_dig_init(ftm, NULL);
	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids), dig, rsp_len);
	if (err != BCME_OK)
		goto done;

done:
	return err;
}

#ifdef WL_FTM_RANGE
static int
ftm_iov_get_ranging_info(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;
	wl_proxd_ranging_info_t	ranging_info;
	const uint16 tlv_ids [] = {
		ID2(RANGING_INFO, SESSION_ID_LIST)
	};
	int	tlvs_len = 0;

	err = ftm_iov_unpack(ftm, req_tlvs, req_len, ftm->dig, &bsscfg);
	if (err != BCME_OK)
		goto done;

	if (!FTM_BSSCFG_FTM_ENABLED(ftm, bsscfg)) {
		err = BCME_DISABLED;
		goto done;
	}

	if (!ftm->rctx) {
		err = BCME_OK; /* ranging context has not been created */
		goto done;
	}

	/* initialize digest and populate fields */
	dig = pdftm_iov_dig_init(ftm, NULL);
	err = wlc_ftm_ranging_get_info(ftm->rctx, &ranging_info);
	if (err != BCME_OK)
		goto done;
	dig->tlv_data.out.ranging_info = &ranging_info;
	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids), dig, &tlvs_len);
	if (err != BCME_OK)
		goto done;

done:
	FTM_LOG_STATUS(ftm, err,
		(("wl%d: %s: status %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err)));

	*rsp_len = tlvs_len;

	return err;
}
#endif /* WL_FTM_RANGE */
#if defined(BCMDBG) || defined(FTM_DONGLE_DEBUG)
static int
ftm_iov_get_dump(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err = BCME_OK;
	pdftm_session_t *sn = NULL;
	struct bcmstrbuf buf;
	bcm_xtlv_t *tlv;
	uint16 tlv_len;
	const uint8 *tlv_datap;

	tlv = (bcm_xtlv_t *)rsp_tlvs;
	if (rsp_max < BCM_XTLV_HDR_SIZE_EX(FTM_XTLV_OPTIONS)) {
		err = BCME_BUFTOOSHORT;
		goto done;
	}

	/* obtain pointer to data for the tlv */
	bcm_xtlv_unpack_xtlv(tlv, NULL, NULL, &tlv_datap, FTM_XTLV_OPTIONS);

	GCC_DIAGNOSTIC_PUSH_SUPPRESS_CAST();
	bcm_binit(&buf, (char *)tlv_datap, (uint)rsp_max - (tlv_datap - (uint8 *)tlv));
	GCC_DIAGNOSTIC_POP();

	if (sid == WL_PROXD_SESSION_ID_GLOBAL) {
		pdftm_dump(ftm, &buf);
	} else {
		sn = FTM_SESSION_FOR_SID(ftm, sid);
		if (!sn) {
			err = BCME_NOTFOUND;
			goto done;
		}
		pdftm_dump_session(ftm, sn, &buf);
	}

	/* determine tlv len and null terminate data */
	tlv_len = (uint16)(buf.buf - buf.origbuf);
	if (tlv_len)
		tlv->data[tlv_len-1] = 0;

	/* pack id and length, data is already in place */
	bcm_xtlv_pack_xtlv(tlv, WL_PROXD_TLV_ID_STRBUF, tlv_len, NULL, FTM_XTLV_OPTIONS);

	*rsp_len = bcm_xtlv_size_for_data(tlv_len, FTM_XTLV_OPTIONS);

done:
	return err;
}
#endif // endif
static int
ftm_iov_get_tune(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	int err;
	ftm_iov_tlv_digest_t *dig;

	const uint16 tlv_ids[] = { ID(TUNE) };
	int tlvs_len;

	BCM_REFERENCE(bsscfg);
	BCM_REFERENCE(sid);
	BCM_REFERENCE(req_tlvs);
	BCM_REFERENCE(req_len);

	FTM_DIG_INIT(ftm, NULL, dig);
	dig->tune = proxd_get_tunep(ftm->wlc, NULL);
	tlvs_len = 0;
	err = pdftm_iov_pack(ftm, rsp_max, rsp_tlvs, tlv_ids, ARRAYSIZE(tlv_ids),
		dig, &tlvs_len);
	if (err != BCME_OK)
		goto done;
	*rsp_len = tlvs_len;

done:
	return err;
}

static int
ftm_iov_tune(pdftm_t *ftm, wlc_bsscfg_t *bsscfg, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	int err = BCME_OK;
	ftm_iov_tlv_digest_t *dig = NULL;
	wl_proxd_params_tof_tune_t *tunep = NULL;

	BCM_REFERENCE(sid);
	BCM_REFERENCE(req_tlvs);
	BCM_REFERENCE(req_len);

	if (!ftm)
		return BCME_NOTREADY;

	dig = ftm->dig;
	err = ftm_iov_unpack(ftm, req_tlvs, req_len, dig, &bsscfg);
	if (err != BCME_OK)
		goto done;
	if (dig->tune) {
		tunep = proxd_get_tunep(ftm->wlc, NULL);
		if (tunep) {
			memcpy(tunep, dig->tune,
				sizeof(wl_proxd_params_tof_tune_t));
			if (!(tunep->setflags & WL_PROXD_SETFLAG_K))
				tunep->Ki = tunep->Kt = 0;
			tunep->setflags &= ~WL_PROXD_SETFLAG_K;
#ifdef BCMDBG
		prhex("ftm_iov_tune: ORIG", (uint8 *)proxd_get_tunep(ftm->wlc, NULL),
			sizeof(wl_proxd_params_tof_tune_t));
		prhex("ftm_iov_tune: dig", (uint8 *)dig->tune,
			sizeof(wl_proxd_params_tof_tune_t));
		prhex("ftm_iov_tune: SET", (const uint8 *)req_tlvs->data, req_tlvs->len);
#endif /* BCMDBG */
		}
	}
done:
	return err;
}

static void
ftm_iov_pack_tune(const ftm_iov_tlv_digest_t *dig, uint8 **buf,
	const uint8 *data, int len)
{
	int i;
	const wl_proxd_params_tof_tune_t *tunep;
	uint32 ki = 0, kt = 0;

	BCM_REFERENCE(len);

	tunep = (const wl_proxd_params_tof_tune_t *)data;

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->version, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->len, sizeof(uint16));

	ki = tunep->Ki;
	kt = tunep->Kt;
	if (!ki && !kt) {
		wl_proxd_session_flags_t flags;
		const pdburst_config_t *configp;
		/* get the default burst config */
		configp = pdftm_get_burst_config(dig->ftm, NULL, &flags);
		if (configp) {
			ratespec_t ackrspec;
			if (!tunep->vhtack) {
				ackrspec = LEGACY_RSPEC(PROXD_DEFAULT_TX_RATE);
			} else {
				ackrspec = LEGACY_RSPEC(PROXD_DEFAULT_TX_RATE) |
				        WL_RSPEC_ENCODE_VHT;
			}
			wlc_phy_kvalue(WLC_PI(dig->ftm->wlc), configp->chanspec,
				proxd_get_ratespec_idx(configp->ratespec, ackrspec),
				&ki, &kt,
				((flags & WL_PROXD_SESSION_FLAG_SEQ_EN) ? WL_PROXD_SEQEN : 0));
		}
	}
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&ki, sizeof(uint32));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&kt, sizeof(uint32));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->vhtack, sizeof(uint16));
	*buf += sizeof(uint16);	/* pad */

	for (i = 0; i < TOF_BW_SEQ_NUM; i++) {
		ftm_iov_pack_uint16(dig, buf, (const uint8 *)(&tunep->N_log2[i]), sizeof(uint16));
	}
	*buf += sizeof(uint16);	/* pad */

	for (i = 0; i < TOF_BW_NUM; i++) {
		ftm_iov_pack_uint16(dig, buf, (const uint8 *)(&tunep->w_offset[i]), sizeof(uint16));
	}
	*buf += sizeof(uint16);	/* pad */

	for (i = 0; i < TOF_BW_NUM; i++) {
		ftm_iov_pack_uint16(dig, buf, (const uint8 *)(tunep->w_len+i), sizeof(uint16));
	}
	*buf += sizeof(uint16);	/* pad */

	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&tunep->maxDT, sizeof(uint32));
	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&tunep->minDT, sizeof(uint32));

	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->totalfrmcnt, sizeof(uint8));
	*buf += (sizeof(uint8) * 3); /* pad */

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->rsv_media, sizeof(uint16));
	*buf += sizeof(uint16);	/* pad */

	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&tunep->flags, sizeof(uint32));

	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->core, sizeof(uint8));

	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->setflags, sizeof(uint8));
	*buf += sizeof(uint16);	/* pad */

	for (i = 0; i < TOF_BW_SEQ_NUM; i++) {
		ftm_iov_pack_uint16(dig, buf, (const uint8 *)(&tunep->N_scale[i]), sizeof(uint16));
	}

	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->sw_adj, sizeof(uint8));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->hw_adj, sizeof(uint8));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->seq_en, sizeof(uint8));
	*buf += (sizeof(uint8) * 3); /* pad */

	for (i = 0; i < TOF_BW_SEQ_NUM; i++) {
		ftm_iov_pack_uint8(dig, buf, (const uint8 *)(&tunep->ftm_cnt[i]), sizeof(uint8));
	}
	*buf += (sizeof(uint8) * 3); /* pad */

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->N_log2_2g, sizeof(uint16));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->N_scale_2g, sizeof(uint16));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_5g20.N_tx_log2, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_5g20.N_rx_log2, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_5g20.N_tx_scale, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_5g20.N_rx_scale, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_5g20.w_len, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_5g20.w_offset, sizeof(uint16));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_2g20.N_tx_log2, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_2g20.N_rx_log2, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_2g20.N_tx_scale, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_2g20.N_rx_scale, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_2g20.w_len, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->seq_2g20.w_offset, sizeof(uint16));

	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->bitflip_thresh, sizeof(uint16));
	ftm_iov_pack_uint16(dig, buf, (const uint8 *)&tunep->snr_thresh, sizeof(uint16));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->recv_2g_thresh, sizeof(uint8));
	*buf += (sizeof(uint8) * 3); /* pad */

	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&tunep->acs_gdv_thresh, sizeof(uint32));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->acs_rssi_thresh, sizeof(uint8));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->smooth_win_en, sizeof(uint8));
	*buf += sizeof(uint16); /* pad */

	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&tunep->acs_gdmm_thresh, sizeof(uint32));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->acs_delta_rssi_thresh, sizeof(uint8));
	*buf += (sizeof(uint8) * 3); /* pad */

	ftm_iov_pack_uint32(dig, buf, (const uint8 *)&tunep->emu_delay, sizeof(uint32));
	ftm_iov_pack_uint8(dig, buf, (const uint8 *)&tunep->core_mask, sizeof(uint8));
	*buf += (sizeof(uint8) * 3); /* pad */

	return;
}

#undef ID
#undef ID2
#undef ID3
#undef ID4

/* external interface */

int
pdftm_iov_pack(pdftm_t *ftm, int rsp_max, wl_proxd_tlv_t *rsp_tlvs,
	const uint16 tlv_ids[], int num_tlvs, ftm_iov_tlv_digest_t *dig, int *rsp_len)
{
	int err = BCME_OK;

	*rsp_len = 0;
	if (!num_tlvs)
		goto done;

	dig->num_tlvs = num_tlvs;
	dig->tlv_ids = tlv_ids;
	dig->next_tlv_idx = 0;
	err = bcm_pack_xtlv_buf(dig, (uint8 *)rsp_tlvs, rsp_max,
		FTM_XTLV_OPTIONS, ftm_iov_pack_get_next, ftm_iov_pack_pack_next, rsp_len);

done:
	return err;
}

int
pdftm_get_iov(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	const ftm_iov_info_t *iov_info;
	ftm_iov_flags_t iov_flags;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));
	ASSERT(rsp_len != NULL);

	*rsp_len = 0;

	iov_flags = FTM_IOV_FLAG_GET;
	iov_flags |= (sid != WL_PROXD_SESSION_ID_GLOBAL) ?
		FTM_IOV_FLAG_SESSION : FTM_IOV_FLAG_METHOD;

	iov_info = ftm_iov_get_iov_info(ftm, cmd, iov_flags);
	if (!iov_info || !iov_info->handle_get) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	err = (*iov_info->handle_get)(ftm, bsscfg, sid, req_tlvs, req_len,
		rsp_max, rsp_tlvs, rsp_len);

	ASSERT(*rsp_len <= rsp_max);

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for cmd %d sid %d"
		" req len %d, rsp tlvs %p, rsp max %d rsp len %d\n", FTM_UNIT(ftm), __FUNCTION__,
		err, cmd, sid, req_len, OSL_OBFUSCATE_BUF(rsp_tlvs), rsp_max, *rsp_len)));
	return err;
}

int wlc_ftm_get_iov(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len,
	int rsp_max, wl_proxd_tlv_t *rsp_tlvs, int *rsp_len)
{
	return pdftm_get_iov(ftm, bsscfg, cmd, sid, req_tlvs, req_len,
		rsp_max, rsp_tlvs, rsp_len);
}

int
pdftm_set_iov(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	const ftm_iov_info_t *iov_info;
	ftm_iov_flags_t iov_flags;
	int err = BCME_OK;

	ASSERT(FTM_VALID(ftm));

	iov_flags = FTM_IOV_FLAG_SET;
	iov_flags |= (sid != WL_PROXD_SESSION_ID_GLOBAL) ?
		FTM_IOV_FLAG_SESSION : FTM_IOV_FLAG_METHOD;

	iov_info = ftm_iov_get_iov_info(ftm, cmd, iov_flags);
	if (!iov_info || !iov_info->handle_set) {
		err = BCME_UNSUPPORTED;
		goto done;
	}

	err = (*iov_info->handle_set)(ftm, bsscfg, sid, req_tlvs, req_len);

done:
	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for cmd %d sid %d req len %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, cmd, sid, req_len)));
	return err;
}

int
wlc_ftm_set_iov(pdftm_t *ftm, wlc_bsscfg_t *bsscfg,
	wl_proxd_cmd_t cmd, wl_proxd_session_id_t sid,
	const wl_proxd_tlv_t *req_tlvs, int req_len)
{
	return pdftm_set_iov(ftm, bsscfg, cmd, sid, req_tlvs, req_len);
}

/* support to get a config tlv */

struct ftm_get_tlv_data_ctx {
	uint16 type;
	uint8 *buf;
	int buf_len;
	int *out_len;
	int status;
};
typedef struct ftm_get_tlv_data_ctx ftm_get_tlv_data_ctx_t;

static int
ftm_get_config_tlv_cb(void *in_ctx, const uint8 *buf, uint16 type, uint16 len)
{
	int err = BCME_OK;
	ftm_get_tlv_data_ctx_t *ctx;

	ctx = (ftm_get_tlv_data_ctx_t *)in_ctx;
	if (type  !=  ctx->type)
		goto done; /* continue search */

	/* terminate search - assuming single isntance of matched tlv */
	err = WL_PROXD_E_CANCELED; /* terminate search */

	/* indicate length to caller regardless */
	if (ctx->out_len)
		*ctx->out_len = len;

	if (len > ctx->buf_len) {
		ctx->status = BCME_BUFTOOSHORT;
		goto done;
	}

	ASSERT(ctx->buf != NULL);

	/* copy data and indicate status to caller */
	memcpy(ctx->buf, buf, len);
	ctx->status = BCME_OK;

done:
	return err;
}

#define FTM_GET_TLV_MAX_BUF_LEN 256
int wlc_ftm_get_tlv_data(wlc_ftm_t *ftm, wl_proxd_session_id_t sid,
    uint16 tlv_id, uint8 *buf, int buf_len, int *out_len)
{
	wl_proxd_tlv_t *tlvs;
	int max_len = FTM_GET_TLV_MAX_BUF_LEN;
	int len = 0;
	int err = BCME_OK;
	ftm_get_tlv_data_ctx_t ctx;

	ASSERT(FTM_VALID(ftm));
	tlvs = (wl_proxd_tlv_t *)MALLOCZ(FTM_OSH(ftm), max_len);
	if (!tlvs) {
		err = BCME_NOMEM;
		goto done;
	}

	if (sid == WL_PROXD_SESSION_ID_GLOBAL) {
		err = ftm_iov_get_info(ftm, FTM_BSSCFG(ftm), sid, NULL, 0, max_len, tlvs, &len);
	} else {
		pdftm_session_t *sn;
		sn = FTM_SESSION_FOR_SID(ftm, sid);
		if (!sn) {
			err = BCME_NOTFOUND;
			goto done;
		}
		err = ftm_iov_get_session_info(ftm, sn->bsscfg, sid, NULL, 0, max_len, tlvs, &len);
	}

	if (err != BCME_OK) {
		if (err == BCME_BUFTOOSHORT)
			err = BCME_NOMEM;
		goto done;
	}

	ctx.type = tlv_id;
	ctx.buf = buf;
	ctx.buf_len = buf_len;
	ctx.out_len = out_len;
	ctx.status = BCME_NOTFOUND;
	(void)bcm_unpack_xtlv_buf((void *)&ctx, (uint8 *)tlvs, len,
		FTM_XTLV_OPTIONS, ftm_get_config_tlv_cb);
	err = ctx.status;

done:
	if (tlvs)
		MFREE(FTM_OSH(ftm), tlvs, max_len);

	FTM_LOG_STATUS(ftm, err, (("wl%d: %s: status %d for tlv id %d sid %d out len %d\n",
		FTM_UNIT(ftm), __FUNCTION__, err, tlv_id, sid, len)));
	return err;
}

pdftm_iov_tlv_digest_t*
pdftm_iov_dig_init(pdftm_t *ftm, pdftm_session_t *sn)
{
	ASSERT(FTM_VALID(ftm));
	pdftm_iov_dig_reset(ftm, sn, ftm->dig);
	return ftm->dig;
}

void
pdftm_iov_dig_reset(pdftm_t *ftm, pdftm_session_t *sn, pdftm_iov_tlv_digest_t *dig)
{
	ASSERT(FTM_VALID(ftm));
	if (dig) {
		bzero(dig, sizeof(*dig));
		dig->ftm = ftm;
		dig->session = sn;
	}
}
