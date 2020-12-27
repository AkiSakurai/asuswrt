/*
 * wl keymgmt command module
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
 * $Id: wluc_keymgmt.c 774769 2019-05-07 08:46:22Z $
 */

#include <wlioctl.h>

#if	defined(DONGLEBUILD)
#include <typedefs.h>
#include <osl.h>
#endif // endif

#include <bcmutils.h>
#include <bcmendian.h>
#include "wlu_common.h"
#include "wlu.h"

static cmd_func_t wl_wepstatus;
static cmd_func_t wl_addwep, wl_rmwep;
static cmd_func_t wl_primary_key;
static cmd_func_t wl_keys, wl_wsec_test;
static cmd_func_t wl_tsc;
static cmd_func_t wl_wsec_info;

static cmd_t wl_keymgmt_cmds[] = {
	{ "wepstatus", wl_wepstatus, -1, -1, /* Deprecated. Use "wsec" */
	"Set or Get WEP status\n"
	"\twepstatus [on|off]" },
	{ "primary_key", wl_primary_key, WLC_GET_KEY_PRIMARY, WLC_SET_KEY_PRIMARY,
	"Set or get index of primary key" },
	{ "addwep", wl_addwep, -1, WLC_SET_KEY,
	"Set an encryption key.  The key must be 5, 13 or 16 bytes long, or\n"
	"\t10, 26, 32, or 64 hex digits long.  The encryption algorithm is\n"
	"\tautomatically selected based on the key size. keytype is accepted\n"
	"\tonly when key length is 16 bytes/32 hex digits and specifies\n"
	"\twhether AES-OCB or AES-CCM encryption is used. Default is ccm.\n"
	"\tWAPI is selected if key len is 32 and arguments contain wapi.\n"
	"\taddwep <keyindex> <keydata> [ocb | ccm | wapi] [notx] [xx:xx:xx:xx:xx:xx]" },
	{ "rmwep", wl_rmwep, -1, WLC_SET_KEY,
	"Remove the encryption key at the specified key index." },
	{ "keys", wl_keys, WLC_GET_KEY, -1,
	"Prints a list of the current WEP keys" },
	{ "tsc", wl_tsc, WLC_GET_KEY_SEQ, -1,
	"Print Tx Sequence Couter for key at specified key index." },
	{ "wsec_test", wl_wsec_test, -1, WLC_SET_WSEC_TEST,
	"Generate wsec errors\n"
	"\twsec_test <test_type> <keyindex|xx:xx:xx:xx:xx:xx>\n"
	"\ttype \'wl wsec_test ?\' for test_types" },
	{ "wsec_info", wl_wsec_info, WLC_GET_VAR, WLC_SET_VAR,
	"Get wsec info (max_keys, key len, bss algo and tx_key_id) and set(or configure) algos\n"
	"\twsec_info <type> [algos] [algos_mask]\n"
	"\ttype \'wl wsec_info ?\' for wsec_info types"
	"\talgos set/reset algos bitmap"
	"\tmask selects algos to be set/reset"},
	{ NULL, NULL, 0, 0, NULL }
};

static char *buf;

/* module initialization */
void
wluc_keymgmt_module_init(void)
{
	/* get the global buf */
	buf = wl_get_buf();

	/* register keymgmt commands */
	wl_module_cmds_register(wl_keymgmt_cmds);
}

static int
wl_wepstatus(void *wl, cmd_t *cmd, char **argv)
{
	int val, error;
	const char *name = "wsec";
	int wsec;

	UNUSED_PARAMETER(cmd);

	if (!*++argv) {
		if ((error = wlu_iovar_getint(wl, name, &val) < 0))
			return error;

		printf("%d\n", val);
		return 0;
	} else {
		val = atoi(*argv);
		if ((error = wlu_iovar_getint(wl, name, &wsec) < 0))
			return error;

		if (val)
			wsec |= WEP_ENABLED;
		else
			wsec &= ~WEP_ENABLED;

		return wlu_iovar_setint(wl, name, wsec);
	}
}

static int
wl_primary_key(void *wl, cmd_t *cmd, char **argv)
{
	int i, val, ret = 0;

	if (!*++argv) {
		i = 0;
		do {
			val = htod32(i);
			if ((ret = wlu_get(wl, cmd->get, &val, sizeof(val))) < 0) {
				return ret;
			}
			if (dtoh32(val)) {
				printf("Key %d is primary\n", i);
				return 0;
			}
		} while (++i < DOT11_MAX_DEFAULT_KEYS);
		printf("No primary key set\n");

	} else {
		val = htod32(atoi(*argv));
		ret = wlu_set(wl, cmd->set, &val, sizeof(val));
	}
	return ret;
}

static int
wl_addwep(void *wl, cmd_t *cmd, char **argv)
{
	wl_wsec_key_t key;
	int bsscfg_idx = 0;
	int consumed;
	int error;

	memset(&key, 0, sizeof(key));

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "addwep", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	/* GET operation not allowed */
	if (!*argv)
		return BCME_USAGE_ERROR;

	key.index = atoi(*argv++);

	if (!*argv) {
		fprintf(stderr, "No key specified\n");
		return BCME_USAGE_ERROR;
	}
	if (parse_wep(argv, &key, TRUE))
		return BCME_BADARG;

	key.index = htod32(key.index);
	key.len = htod32(key.len);
	key.algo = htod32(key.algo);
	key.flags = htod32(key.flags);

	if (consumed == 0) {
		error = wlu_set(wl, cmd->set, &key, sizeof(key));
	} else {
		error = wlu_bssiovar_setbuf(wl, "wsec_key", bsscfg_idx,
			&key, sizeof(key), buf, WLC_IOCTL_MAXLEN);
	}

	return error;
}

static int
wl_rmwep(void *wl, cmd_t *cmd, char **argv)
{
	wl_wsec_key_t key;
	int bsscfg_idx = 0;
	int consumed;
	int error;

	memset(&key, 0, sizeof(key));

	argv++;

	/* parse a bsscfg_idx option if present */
	if ((error = wl_cfg_option(argv, "rmwep", &bsscfg_idx, &consumed)) != 0)
		return error;

	argv += consumed;

	/* GET operation not allowed */
	if (!*argv)
		return BCME_USAGE_ERROR;

	key.index = htod32(atoi(*argv++));

	if (*argv) {
		if (!(wl_ether_atoe(*argv, &key.ea)))
			return BCME_USAGE_ERROR;
	}

	if (consumed == 0) {
		error = wlu_set(wl, cmd->set, &key, sizeof(key));
	} else {
		error = wlu_var_setbuf(wl, "wsec_key", &key, sizeof(key));
	}

	return error;
}

static struct {
	uint value;
	const char *string;
} wsec_test[] = {
	{WSEC_GEN_MIC_ERROR, 	"mic_error"},
	{WSEC_GEN_REPLAY, 	"replay"},
	{WSEC_GEN_ICV_ERROR, "icv_error"},
	{WSEC_GEN_MFP_ACT_ERROR, "act_error"},
	{WSEC_GEN_MFP_DISASSOC_ERROR, "disassoc_error"},
	{WSEC_GEN_MFP_DEAUTH_ERROR, "deauth_error"},
	{0,		NULL}
};

static int
wl_wsec_test(void *wl, cmd_t *cmd, char **argv)
{
	wl_wsec_key_t *key;
	int i, len;
	char *endptr = NULL, *wsec_buf = NULL;
	uint32 val, last_val;
	int err = 0;

	if (!*++argv) {
		err = BCME_USAGE_ERROR;
		goto usage;
	}

	val = strtol(*argv, &endptr, 0);
	if (endptr == *argv) {
		/* the value string was not parsed by strtol */
		for (i = 0; wsec_test[i].value; i++)
			if (stricmp(wsec_test[i].string, *argv) == 0) {
				val = wsec_test[i].value;
				break;
			}
		if (wsec_test[i].value == 0) {
			err = BCME_BADARG;
			goto usage;
		}
	}
	++argv;

	switch (val) {
	case WSEC_GEN_REPLAY:
	case WSEC_GEN_MIC_ERROR:
	case WSEC_GEN_ICV_ERROR:
	case WSEC_GEN_MFP_ACT_ERROR:
	case WSEC_GEN_MFP_DISASSOC_ERROR:
	case WSEC_GEN_MFP_DEAUTH_ERROR:
		if (!*argv) {
			fprintf(stderr, "insufficient arguments\n");
			return BCME_USAGE_ERROR;
		}
		len = sizeof(wl_wsec_key_t) + 4;
		wsec_buf = malloc(len);
		if (wsec_buf == NULL) {
			fprintf(stderr, "Error allocating memory failed for wsec_buf");
			return BCME_NOMEM;
		}
		*(uint32 *)wsec_buf = htod32(val);
		key = (wl_wsec_key_t *)&wsec_buf[4];
		memset(key, 0, sizeof(wl_wsec_key_t));
		/* If it doesn't look like an ether addr, suppose it's a key index */
		if (!(wl_ether_atoe(*argv, &key->ea))) {
			memset(&key->ea, 0, ETHER_ADDR_LEN);
			key->index = htod32(atoi(*argv));
		}
		break;
	default:
		goto usage;
	}

	err = wlu_set(wl, cmd->set, wsec_buf, len);
	free(wsec_buf);
	goto exit;

usage:
	fprintf(stderr, "wsec test_type may be a number or name from the following set:");
	last_val = 0xffffffff;
	for (i = 0; (val = wsec_test[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n0x%04x %s", val, wsec_test[i].string);
		else
			fprintf(stderr, ", %s", wsec_test[i].string);
		last_val = val;
	}
	fprintf(stderr, "\n");

exit:
	return err;
}

static struct {
	uint value;
	const char *string;
} wsec_info_tbl[] = {
	{WL_WSEC_INFO_MAX_KEYS, "get max keys"},
	{WL_WSEC_INFO_BSS_TX_KEY_ID, "get bss tx key id"},
	{WL_WSEC_INFO_BSS_ALGO, "get bss algo"},
	{WL_WSEC_INFO_BSS_KEY_LEN, "get bss key length"},
	{WL_WSEC_INFO_BSS_ALGOS, "set bss algo"},
	{0,		NULL}
};

static int
wl_wsec_info_tlv_cb(void *ctx, const uint8 *data, uint16 type, uint16 len)
{
	int i;
	uint16 tlv_data = (*data) + (*(data + 1) << 8);
	uint32 val = 0xffffffff;

	BCM_REFERENCE(ctx);
	BCM_REFERENCE(len);

	for (i = 0; val; i++) {
		val = wsec_info_tbl[i].value;
		if (val == type) {
			printf("Type 0x%x %s: %d\n", type, wsec_info_tbl[i].string, tlv_data);
			return BCME_OK;
		}
	}
	printf("Type 0x%x is not supported\n", type);
	return BCME_ERROR;
}

static int
wl_wsec_info_usage(void)
{
	uint32 val, last_val;
	int i;

	fprintf(stderr, "Comamnd: wsec_info <type> [algos_bitmap] [algo_mask]\n"
		"\n\toption [algos_bitmap] [algo_mask] is only for set bss algo\n"
		"\nwsec_info type set:");
	last_val = 0xffffffff;
	for (i = 0; (val = wsec_info_tbl[i].value); i++) {
		if (val != last_val)
			fprintf(stderr, "\n\t0x%04x: %s", val, wsec_info_tbl[i].string);
		else
			fprintf(stderr, ", %s", wsec_info_tbl[i].string);
		last_val = val;
	}
	fprintf(stderr, "\n");
	return 0;
}

static int
wl_wsec_info(void *wl, cmd_t *cmd, char **argv)
{
	wl_wsec_info_t *wsec_info;
	bcm_xtlv_t *wsec_info_tlv;
	char *endptr = NULL;
	uint16 type;
	uint8 wsec_info_hdr_size = OFFSETOF(wl_wsec_info_t, tlvs);
	uint32 param_len = OFFSETOF(wl_wsec_info_t, tlvs);
	int err = 0;

	if (!*++argv) {
		err = BCME_USAGE_ERROR;
		return wl_wsec_info_usage();
	}

	type = strtol(*argv, &endptr, 0);

	if (endptr == *argv) {
		return wl_wsec_info_usage();
	}
	++argv;

	/* check for valid 'type' */
	wsec_info = (wl_wsec_info_t *)buf;
	wsec_info->version = WL_WSEC_INFO_VERSION;
	wsec_info_tlv = (bcm_xtlv_t *)(buf + wsec_info_hdr_size);

	/* handle set algos */
	if (type == WL_WSEC_INFO_BSS_ALGOS) {
		if (*argv && *(argv + 1)) {
			uint32 algos = strtol(*argv, &endptr, 0);
			uint32 mask = strtol(*(argv + 1), &endptr, 0);
			uint16 tlv_data_len = 0;
			uint8 tlv_data[8];

			/* only wsec_info algos has set operation */
			wsec_info->num_tlvs ++;

			memcpy(tlv_data, &algos, 4);
			memcpy(tlv_data + 4, &mask, 4);
			tlv_data_len = sizeof(tlv_data);
			bcm_xtlv_pack_xtlv(wsec_info_tlv, type, tlv_data_len, tlv_data, 0);

			param_len += (WL_WSEC_INFO_TLV_HDR_LEN + tlv_data_len);

			/* issue set wsec_info algos <algos> <mask> */
			if ((err = wlu_iovar_setbuf(wl, cmd->name, buf, param_len, buf,
					WLC_IOCTL_SMLEN))) {
					printf("Failed: set wsec_info algos failed\n");
			}

			goto done;
		} else if (*argv) {
			err = BCME_USAGE_ERROR;
			return wl_wsec_info_usage();
		}
		/* get will be handled below */
	}

	do {
		/* handle get of other wsec_info types */
		uint8 tlv_data[4];
		uint16 tlv_data_len = sizeof(uint32);
		uint16 tot_tlv_len;
		uint16 iov_len;

		tlv_data_len = 4;
		bcm_xtlv_pack_xtlv(wsec_info_tlv, type, tlv_data_len, tlv_data, 0);
		tlv_data_len = 4;
		param_len += (WL_WSEC_INFO_TLV_HDR_LEN + tlv_data_len);

		wsec_info->num_tlvs ++;
		/* issue get wsec_info <type> */
		iov_len = param_len + (strlen(cmd->name) + 1);
		if ((err = wlu_iovar_getbuf(wl, cmd->name, buf, param_len, buf, iov_len))) {
			printf("Failed: get wsec_info algos failed\n");
			return wl_wsec_info_usage();
		}

		tot_tlv_len = param_len - OFFSETOF(wl_wsec_info_t, tlvs);
		err = bcm_unpack_xtlv_buf(NULL, (uint8*)wsec_info_tlv, tot_tlv_len,
			BCM_XTLV_OPTION_ALIGN32, wl_wsec_info_tlv_cb);
	} while(0); /* No support for get of multiple types */

done:
	return err;
}

static int
wl_keys(void *wl, cmd_t *cmd, char **argv)
{
	uint i, j;
	union {
		int index;
		wl_wsec_key_t key;
	} u;
	int wep_is_on = 0;
	const char *addr;
	int ret = BCME_OK;

	UNUSED_PARAMETER(argv);

	if (wlu_iovar_getint(wl, "wsec", &wep_is_on) < 0)
		fprintf(stderr, "Could not query wsec status.\n");

	for (i = 0; i < 1024; i++) {
		memset(&u, 0, sizeof(u));
		u.index = htod32(i);
		ret = wlu_get(wl, cmd->get, &u, sizeof(u));
		if (ret == BCME_IOCTL_ERROR) {
			int bcmerr;
			if (wlu_iovar_getint(wl, "bcmerror", &bcmerr) >= 0 &&
				(bcmerr == BCME_BADKEYIDX)) {
				ret = BCME_OK;
			}
		}

		if (ret != BCME_OK)
			break;

		/* ignore empty keys */
		if (dtoh32(u.key.algo) == CRYPTO_ALGO_OFF || (u.key.len == 0))
			continue;

		if (ETHER_ISNULLADDR(&u.key.ea))
			addr = "(default)";
		else
			addr = wl_ether_etoa(&u.key.ea);

		printf("%3d: %-17s Key %d: %s ", i, addr, dtoh32(u.key.index),
		       bcm_crypto_algo_name(dtoh32(u.key.algo)));

		if (wep_is_on && dtoh32(u.key.flags) & WL_PRIMARY_KEY)
			printf("*");
		printf("\t");

		if (dtoh32(u.key.len) == 0)
			printf("No key present");
		else {
			if (dtoh32(u.key.flags) & WL_SOFT_KEY)
				printf("soft ");
			printf("len %d, data 0x", dtoh32(u.key.len));
			for (j = 0; j < dtoh32(u.key.len); j++)
				printf("%02X", u.key.data[j]);

			for (j = 0; j < dtoh32(u.key.len); j++)
				if (!isprint(u.key.data[j]))
					break;
			if (j == dtoh32(u.key.len))
				printf(" (%.*s)", (int)dtoh32(u.key.len), u.key.data);

		}

		printf("\n");
	}

	return ret;
}

static int
wl_tsc(void *wl, cmd_t *cmd, char **argv)
{
	union {
		int32 index;
		uint8 tsc[DOT11_WPA_KEY_RSC_LEN];
	} u;
	uint32 hi, lo;
	int idx, ret;

	if (!*++argv)
		return BCME_USAGE_ERROR;
	idx = atoi(*argv);
	if (idx < 0) {
		fprintf(stderr, "Key index %d out of range. Should be positive.\n", idx);
		return BCME_BADARG;
	}
	u.index = htod32(idx);
	if ((ret = wlu_get(wl, cmd->get, &u, sizeof(u))) < 0)
		return ret;
	lo = u.tsc[0] | (u.tsc[1] << 8) | (u.tsc[2] << 16) | (u.tsc[3] << 24);
	hi = u.tsc[4] | (u.tsc[5] << 8) | (u.tsc[6] << 16) | (u.tsc[7] << 24);

	printf("Key %d TSC: 0x%04x:%08x\n", idx, hi, lo);
	return 0;
}
