/*
 * wl slot_bss command module
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
 *
 * <<Broadcom-WL-IPTag/Proprietary:>>
 *
 * $Id: wluc_slotted_bss.c 774769 2019-05-07 08:46:22Z $
 */
#ifdef WLSLOTTED_BSS
#include <wlioctl.h>
#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif
#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"
#include "wlu_avail_utils.h"
#include <bcmiov.h>

typedef struct wl_slot_bss_sub_cmd wl_slot_bss_sub_cmd_t;
typedef int (slot_bss_cmd_handler_t)(void *wl,
	const wl_slot_bss_sub_cmd_t *cmd, int argc, char **argv,
		bool *is_set, uint8 *iov_data, uint16 *avail_len);
/*
 * slot_bss sub-commands list entry
 */
struct wl_slot_bss_sub_cmd {
	char *name;
	slot_bss_cmd_handler_t *handler;
	uint16 id;
	uint16 type;
	uint8 version;
};

static cmd_func_t wl_slot_bss_control;

/* Below API can be used in common for all get only iovars */
static slot_bss_cmd_handler_t wl_slot_bss_subcmd_get;
static slot_bss_cmd_handler_t wl_slot_bss_subcmd_chanseq;

static cmd_t wl_slot_bss_cmds[] = {
	{"slot_bss", wl_slot_bss_control, WLC_GET_VAR, WLC_SET_VAR,
	"wl slot_bss <subcmd> <args>"},
	{ NULL, NULL, 0, 0, NULL },
};

static const wl_slot_bss_sub_cmd_t slot_bss_subcmd_lists[] = {
	{"ver", wl_slot_bss_subcmd_get, WL_SLOTTED_BSS_CMD_VER, IOVT_UINT16, 0x01},
	{"chanseq", wl_slot_bss_subcmd_chanseq, WL_SLOTTED_BSS_CMD_CHANSEQ, IOVT_BUFFER, 0x01},
	{NULL, NULL, 0, 0, 0}
};

static char *buf;

/* module initialization */
void
wluc_slot_bss_module_init(void)
{
	(void)g_swap;

	/* get the global buf */
	buf = wl_get_buf();

	/* register scan commands */
	wl_module_cmds_register(wl_slot_bss_cmds);
}
static char *
get_slot_bss_cmd_name(uint16 cmd_id)
{
	const wl_slot_bss_sub_cmd_t *slot_bss_cmd = &slot_bss_subcmd_lists[0];

	while (slot_bss_cmd->name != NULL) {
		if (slot_bss_cmd->id == cmd_id) {
			return slot_bss_cmd->name;
		}
		slot_bss_cmd++;
	}
	return NULL;
}
const wl_slot_bss_sub_cmd_t *
slot_bss_get_subcmd_info(char **argv)
{
	char *cmdname = *argv;
	const wl_slot_bss_sub_cmd_t *p_subcmd_info = &slot_bss_subcmd_lists[0];

	while (p_subcmd_info->name != NULL) {
		if (stricmp(p_subcmd_info->name, cmdname) == 0) {
			return p_subcmd_info;
		}
		p_subcmd_info++;
	}

	return NULL;
}
static int
slot_bss_get_arg_count(char **argv)
{
	int count = 0;
	while (*argv != NULL) {
		if (strcmp(*argv, WL_IOV_BATCH_DELIMITER) == 0) {
			break;
		}
		argv++;
		count++;
	}

	return count;
}
static int
wlu_slot_bss_chanseq_tlv_cbfn(void *ctx, const uint8 *chseqdata, uint16 type, uint16 len)
{
	int res = BCME_OK;
	chan_seq_tlv_data_t *tlv_data = (chan_seq_tlv_data_t *)chseqdata;
	UNUSED_PARAMETER(ctx);
	UNUSED_PARAMETER(len);

	if (chseqdata == NULL) {
		printf("%s: Bad argument !!\n", __FUNCTION__);
		return BCME_BADARG;
	}
	switch (type) {
		case CHAN_SEQ_TYPE_SLICE:
		{
			slice_chan_seq_t *slice_chanseqresp = (slice_chan_seq_t *)tlv_data->data;
			printf("\n****Slice Channel sequence****\n");
			printf("Slice index:%d\n"
				"Num_chanspecs:%d\n",
				slice_chanseqresp->slice_index,
				slice_chanseqresp->num_chanspecs);
			prhex("Chan Sequence", (uint8 *)slice_chanseqresp->chanspecs,
					slice_chanseqresp->num_chanspecs * sizeof(chanspec_t));
			printf("\n");
			break;
		}
		default:
		{
			break;
		}
	}
	return res;
}
/*
 *  a cbfn function, displays bcm_xtlv variables rcvd in get ioctl's xtlv buffer.
 *  it can process GET result for all slot_bss commands, provided that
 *  XTLV types (AKA the explicit xtlv types) packed into the ioctl buff
 *  are unique across all slot_bss ioctl commands
 */
static int
wlu_slot_bss_resp_iovars_cbfn(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	int res = BCME_OK;
	bcm_iov_batch_buf_t *b_resp = (bcm_iov_batch_buf_t *)ctx;
	int32 status;
	uint16 cmd_rsp_len;
	char *cmd_name;
	/* if all tlvs are parsed, we should not be here */
	if (b_resp->count == 0) {
		return BCME_BADLEN;
	}

	/*  cbfn params may be used in f/w */
	if (len < sizeof(status)) {
		return BCME_BUFTOOSHORT;
	}

	/* first 4 bytes consists status */
	status = dtoh32(*(uint32 *)data);

	data = data + sizeof(status);
	cmd_rsp_len = len - sizeof(status);

	/* If status is non zero */
	if (status != BCME_OK) {
		if ((cmd_name = get_slot_bss_cmd_name(type)) == NULL) {
			printf("Undefined command type %04x len %04x\n", type, len);
		} else {
			printf("%s failed, status: %04x\n", cmd_name, status);
		}
		return status;
	}

	if (!cmd_rsp_len) {
		if (b_resp->is_set) {
			/* Set cmd resp may have only status, so len might be zero.
			 * just decrement batch resp count
			 */
			goto counter;
		}
		/* Every response for get command expects some data,
		 * return error if there is no data
		 */
		return BCME_ERROR;
	}

	/* TODO: could use more length checks in processing data */
	switch (type) {
		case WL_SLOTTED_BSS_CMD_VER: {
			uint16 version = *data;
			printf("IOV version:%d\n", version);
			break;
		}
		case WL_SLOTTED_BSS_CMD_CHANSEQ:
		{
			uint16 tlv_data_len = 0;
			//chan_seq_tlv_t *tlv_buf;
			sb_channel_sequence_t *sbchanseq = (sb_channel_sequence_t *)data;
			printf("Sched_flag:0x%x\n", sbchanseq->sched_flags);
			printf("Num_seq:%d\n", sbchanseq->num_seq);
			tlv_data_len = len - OFFSETOF(sb_channel_sequence_t, seq);
				res = bcm_unpack_xtlv_buf(sbchanseq, (const uint8 *)sbchanseq->seq,
					tlv_data_len, BCM_IOV_CMD_OPT_ALIGN32,
					wlu_slot_bss_chanseq_tlv_cbfn);
			break;
		}
		default:
			res = BCME_ERROR;
		break;
	}
counter:
	if (b_resp->count > 0) {
		b_resp->count--;
	}

	if (!b_resp->count) {
		res = BCME_IOV_LAST_CMD;
	}

	return res;
}
static int
wl_slot_bss_process_resp_buf(void *iov_resp, uint16 max_len, uint8 is_set)
{
	int res = BCME_UNSUPPORTED;
	uint16 version;
	uint16 tlvs_len;

	/* Check for version */
	version = dtoh16(*(uint16 *)iov_resp);
	if (version & (BCM_IOV_XTLV_VERSION | BCM_IOV_BATCH_MASK)) {
		bcm_iov_batch_buf_t *p_resp = (bcm_iov_batch_buf_t *)iov_resp;
		if (!p_resp->count) {
			res = BCME_RANGE;
			goto done;
		}
		p_resp->is_set = is_set;
		/* number of tlvs count */
		tlvs_len = max_len - OFFSETOF(bcm_iov_batch_buf_t, cmds[0]);
		/* Extract the tlvs and print their resp in cb fn */
		res = bcm_unpack_xtlv_buf((void *)p_resp, (const uint8 *)&p_resp->cmds[0],
			tlvs_len, BCM_IOV_CMD_OPT_ALIGN32, wlu_slot_bss_resp_iovars_cbfn);

		if (res == BCME_IOV_LAST_CMD) {
			res = BCME_OK;
		}
	}
	/* else non-batch not supported */
done:
	return res;
}

/*
 *   --- common for all slot_bss get commands ----
 */
int
wl_slot_bss_do_ioctl(void *wl, void *sbioc, uint16 iocsz, uint8 is_set)
{
	/* for gets we only need to pass ioc header */
	uint8 *iocresp = NULL;
	uint8 *resp = NULL;
	char *iov = "slot_bss";
	int res;

	if ((iocresp = malloc(WLC_IOCTL_MAXLEN)) == NULL) {
		printf("Failed to malloc %d bytes \n",
				WLC_IOCTL_MAXLEN);
		return BCME_NOMEM;
	}

	if (is_set) {
		int iov_len = strlen(iov) + 1;

		/*  send setbuf slot_bss iovar */
		res = wlu_iovar_setbuf(wl, iov, sbioc, iocsz, iocresp, WLC_IOCTL_MAXLEN);
		/* iov string is not received in set command resp buf */
		resp = &iocresp[iov_len];
	} else {
		/*  send getbuf slot_bss iovar */
		res = wlu_iovar_getbuf(wl, iov, sbioc, iocsz, iocresp, WLC_IOCTL_MAXLEN);
		resp = iocresp;
	}
	/*  check the response buff  */
	if ((res == BCME_OK) && (iocresp != NULL)) {
		res = wl_slot_bss_process_resp_buf(resp, WLC_IOCTL_MAXLEN, is_set);
	}

	free(iocresp);
	return res;
}

/* main control function */

static int
wl_slot_bss_control(void *wl, cmd_t *cmd, char **argv)
{
	int ret = BCME_USAGE_ERROR;
	const wl_slot_bss_sub_cmd_t *slot_bss_cmd = NULL;
	bcm_iov_batch_buf_t *b_buf = NULL;
	uint8 *p_iov_buf;
	uint16 iov_len, iov_len_start, subcmd_len, slotbss_cmd_data_len;
	bool is_set = TRUE;
	bool first_cmd_req = is_set;
	int argc = 0;
	/* Skip the command name */
	UNUSED_PARAMETER(cmd);

	argv++;
	/* skip to cmd name after "slot_bss" */
	if (*argv) {
		if (!strcmp(*argv, "-h") || !strcmp(*argv, "help"))  {
			/* help , or -h* */
			argv++;
			goto fail;
		}
	}

	/*
	 * malloc iov buf memory
	 */
	b_buf = (bcm_iov_batch_buf_t *)calloc(1, WLC_IOCTL_MEDLEN);
	if (b_buf == NULL) {
		return BCME_NOMEM;
	}
	/*
	 * Fill the header
	 */
	iov_len = iov_len_start = WLC_IOCTL_MEDLEN;
	b_buf->version = htol16(BCM_IOV_XTLV_VERSION | BCM_IOV_BATCH_MASK);
	b_buf->count = 0;
	p_iov_buf = (uint8 *)(&b_buf->cmds[0]);
	iov_len -= OFFSETOF(bcm_iov_batch_buf_t, cmds[0]);

	while (*argv != NULL) {
		bcm_iov_batch_subcmd_t *sub_cmd =
			(bcm_iov_batch_subcmd_t *)p_iov_buf;
		/*
		 * Lookup sub-command info
		 */
		slot_bss_cmd = slot_bss_get_subcmd_info(argv);
		if (!slot_bss_cmd) {
			goto fail;
		}
		/* skip over sub-cmd name */
		argv++;

		/*
		 * Get arg count for sub-command
		 */
		argc = slot_bss_get_arg_count(argv);

		sub_cmd->u.options =
			htol32(BCM_XTLV_OPTION_ALIGN32);
		/*
		 * Skip over sub-command header
		 */
		iov_len -= OFFSETOF(bcm_iov_batch_subcmd_t, data);

		/*
		 * take a snapshot of curr avail len,
		 * to calculate iovar data len to be packed.
		 */
		subcmd_len = iov_len;

		/* invoke slot_bss sub-command handler */
		ret = slot_bss_cmd->handler(wl, slot_bss_cmd, argc, argv, &is_set,
				(uint8 *)&sub_cmd->data[0], &subcmd_len);

		if (ret != BCME_OK) {
			goto fail;
		}
		slotbss_cmd_data_len = (iov_len - subcmd_len);
		/*
		 * In Batched commands, sub-commands TLV length
		 * includes size of options as well. Because,
		 * options are considered as part bcm xtlv data
		 */
		slotbss_cmd_data_len += sizeof(sub_cmd->u.options);

		/*
		 * Data buffer is set NULL, because sub-cmd
		 * tlv data is already filled by command hanlder
		 * and no need of memcpy.
		 */
		ret = bcm_pack_xtlv_entry(&p_iov_buf, &iov_len,
				slot_bss_cmd->id, slotbss_cmd_data_len,
				NULL, BCM_XTLV_OPTION_ALIGN32);

		/*
		 * iov_len is already compensated before sending
		 * the buffer to cmd handler.
		 * xtlv hdr and options size are compensated again
		 * in bcm_pack_xtlv_entry().
		 */
		iov_len += OFFSETOF(bcm_iov_batch_subcmd_t, data);
		if (ret == BCME_OK) {
			/* Note whether first command is set/get */
			if (!b_buf->count) {
				first_cmd_req = is_set;
			} else if (first_cmd_req != is_set) {
				/* Returning error, if sub-sequent commands req is
				 * not same as first_cmd_req type.
				 */
				 ret = BCME_UNSUPPORTED;
				 break;
			}

			/* bump sub-command count */
			b_buf->count++;
			/* No more commands to parse */
			if (*argv == NULL) {
				break;
			}
			/* Still un parsed arguments exist and
			 * immediate argument to parse is not
			 * a BATCH_DELIMITER
			 */
			while (*argv != NULL) {
				if (strcmp(*argv, WL_IOV_BATCH_DELIMITER) == 0) {
					/* skip BATCH_DELIMITER i.e "+" */
					argv++;
					break;
				}
				argv++;
			}
		} else {
			printf("Error handling sub-command %d\n", ret);
			break;
		}
	}

	/* Command usage error handling case */
	if (ret != BCME_OK) {
		goto fail;
	}
	iov_len = iov_len_start - iov_len;

		/*
		* Dispatch iovar
		*/
		ret = wl_slot_bss_do_ioctl(wl, (void *)b_buf, iov_len, is_set);

	fail:
		if (ret != BCME_OK) {
			printf("Error: %d\n", ret);
		}
		free(b_buf);
		return ret;
}

/*  ********get only handler************** */
static int
wl_slot_bss_subcmd_get(void *wl, const wl_slot_bss_sub_cmd_t  *cmd, int argc, char **argv,
	bool *is_set, uint8 *iov_data, uint16 *avail_len)
{
	int res = BCME_OK;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);
	UNUSED_PARAMETER(iov_data);
	UNUSED_PARAMETER(avail_len);

	/* If no more parameters are passed, it is GET command */
	if ((*argv == NULL) || argc == 0) {
		*is_set = FALSE;
	} else {
		return BCME_UNSUPPORTED;
	}
	return res;
}
static int
wl_slot_bss_subcmd_chanseq(void *wl, const wl_slot_bss_sub_cmd_t  *cmd, int argc, char **argv,
	bool *is_set, uint8 *iov_data, uint16 *avail_len)
{
	int err = BCME_OK;
	sb_channel_sequence_t *sbchanseq = NULL;
	chan_seq_tlv_t *pxtlv;
	uint16 buflen;
	uint16 total_len = 0;
	uint16 tlv_type;
	chan_seq_tlv_data_t *chanseq_data;

	UNUSED_PARAMETER(wl);
	UNUSED_PARAMETER(cmd);

	sbchanseq = (sb_channel_sequence_t *)iov_data;
	memset(sbchanseq, 0, *avail_len);

	pxtlv = sbchanseq->seq;

	/* If no more parameters are passed, it is GET command */
	if ((*argv == NULL) || argc == 0) {
		*is_set = FALSE;
	} else {
		*is_set = TRUE;
		buflen = OFFSETOF(sb_channel_sequence_t, seq);

		if (buflen > *avail_len) {
			printf("Buf short, requested:%d, available:%d\n",
					buflen, *avail_len);
			return BCME_BUFTOOSHORT;
		}
		*avail_len -= buflen;

		sbchanseq->sched_flags = strtoul(*argv, NULL, 0);
		argc--;
		if (argc ==  0)
		  return BCME_OK;
		argv++;
		sbchanseq->num_seq = strtoul(*argv++, NULL, 0);
		argc--;

		total_len += buflen;

		while ((argc > 0)&& (strcmp(*argv++, "-t") == 0)) {
			uint8 chanspecbuf[256] = {0};
			buflen = 0;
			argc--;

			tlv_type = strtoul(*argv++, NULL, 0);
			argc--;

			chanseq_data = (chan_seq_tlv_data_t *)chanspecbuf;

			buflen += OFFSETOF(chan_seq_tlv_data_t, data);

			switch (tlv_type) {
				case CHAN_SEQ_TYPE_SLICE:
				{
					uint16 chanspec_size;
					slice_chan_seq_t *slice_chanseq =
						(slice_chan_seq_t *)chanseq_data->data;
					if (*avail_len < sizeof(slice_chan_seq_t)) {
						return BCME_NOMEM;
					}
					slice_chanseq->slice_index = strtoul(*argv++, NULL, 0);
					argc--;
					chanspec_size = wl_pattern_atoh(*argv++,
						(char *)slice_chanseq->chanspecs);
					argc--;
					slice_chanseq->num_chanspecs =
						chanspec_size / sizeof(*slice_chanseq->chanspecs);
					buflen += (chanspec_size + WL_SLICE_CHAN_SEQ_FIXED_LEN);
					break;
				}
				default:
				{
					fprintf(stderr, "Invalid option\n");
					err = BCME_USAGE_ERROR;
					goto exit;
				}
			}

			err = bcm_pack_xtlv_entry((uint8 **)&pxtlv,
				avail_len, tlv_type,
					buflen, (uint8 *)chanseq_data,
					BCM_XTLV_OPTION_ALIGN32);
			if (err != BCME_OK) {
				goto exit;
			}
		}
	}
exit:
	return err;
}
#endif /* SLOTTED_BSS */
